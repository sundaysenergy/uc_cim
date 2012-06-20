<?php

/**
 * @file
 * UC CIM Pages
 */

/**
 * @todo This function should utilize callback arguments from the D6 menu system instead of checking arg()
 */
function uc_cim_management() {

  if (!is_numeric(arg(4))) {
    //The table to get from depends on test mode or production mode - get the right one
    $table = uc_cim_table('uc_cim');

    //Get all profiles
    $sql = "SELECT * FROM {". $table ."}";
    $result = db_query($sql);
    while ($getdata = db_fetch_array($result)) {
      $row = array();
      $row['data'][0] = $getdata['uid'];
      $row['data'][1] = l($getdata['customer_profile_id'], 'admin/store/customers/cim/'. $getdata['customer_profile_id']);
      $row['data'][2] = l('delete', 'admin/store/customers/cim/'. $getdata['customer_profile_id'] .'/delete');
      $rows[] = $row;
    }

    $header = array('UID', 'Profile ID', 'Delete');
    $output .= theme_table($header, $rows);
  }
  elseif (is_numeric(arg(4))) {

    //Get profile details
    $profile_id = arg(4);

    //Build XML to post
    $content =
    '<?xml version="1.0" encoding="utf-8"?>'.
    '<getCustomerProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
    merchant_auth_xml() .
      '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
    '</getCustomerProfileRequest>';

    //Send the XML via curl
    $response = uc_cim_curl_send($content);

    $output .= '<p>CIM Data Request Status: '. $response->messages->message->code .' ('. $response->messages->message->text .')</p>';
    $output .= '<p>'. l('return to profile list', 'admin/store/customers/cim') .'</p>';

    $output .= '<h2>Customer Profile:</h2>';
    $output .= 'User ID: '. $response->profile->merchantCustomerId .'<br/>';
    $output .= 'Description: '. $response->profile->description .'<br/>';
    $output .= 'Email: '. $response->profile->email .'<br/>';
    $output .= 'Profile ID: '. $response->profile->customerProfileId .'<br/>';

    $output .= '<h2>Payment Profiles:</h2>';
    foreach ($response->profile->paymentProfiles as $profile) {
      $output .= '<div class="payment-profile">';
      $output .= '<b>Payment Profile ID: </b>'. $profile->customerPaymentProfileId .'<br/>';
      $output .= '<b>Billing Address:</b><br/>';
      foreach ((array) $profile->billTo as $key => $value) {
        $output .= $key .': '. $value .'<br/>';
      }
      $output .= '<b>Credit Card:</b>'. $profile->payment->creditCard->cardNumber .'<br/>';
      $output .= '</div>';
    }
  }
  //Add our CSS for some formatting
  drupal_add_css(drupal_get_path('module', 'uc_cim') .'/uc_cim.css');

  return $output;
}

/**
 * Delete a profile
 */
function uc_cim_delete_profile($profile_id) {
  $response = delete_customer_profile_request($profile_id);

  //The table to update depends on test mode or production mode - get the right one
  $table = uc_cim_table('uc_cim_payment_profiles');

  //Delete
  $sql = "DELETE FROM {". $table ."} WHERE customer_profile_id = %d";
  db_query($sql, $profile_id);

  $table = uc_cim_table('uc_cim');
  $sql = "DELETE FROM {". $table ."} WHERE customer_profile_id = %d";
  db_query($sql, $profile_id);
  if ($response->messages->message->code == 'I00001') {
    drupal_set_message('The profile was successfully deleted');
  }
  else {
    drupal_set_message('The profile was deleted from the local database, but errors were encountered through authorize.net: '. $response->messages->message->text);
  }
  drupal_goto('admin/store/customers/cim');

  //@todo should we delete from uc_payment_cim as well?
}

/**
 * Charge a credit card
 */
function uc_cim_terminal($order_id) {
  $order = uc_order_load($order_id);
  if ($order === FALSE) {
    drupal_set_message(t('Order !order_id does not exist.', array('!order_id' => $order_id)));
    drupal_goto('admin/store/orders');
  }

  //Add our CSS for some formatting
  drupal_add_css(drupal_get_path('module', 'uc_cim') .'/uc_cim.css');

  //Get the customer payment profile
  $profile_id = uc_cim_get_profile_id($order->uid);
  $payment_profile_id = arg(5);
  $response = get_customer_payment_profile_request($profile_id, $payment_profile_id);

  //Display the profile and a terminal form
  $output .= '<div class="stored-credit-card">';
  $output .= '<h3>Stored Credit Card:</h3>';

  if ($response->messages->message->code != 'I00001') {
    $output .= '<p>ERROR: '. $response->messages->message->code .' ('. $response->messages->message->text .')</p>';
    $output .= '</div>';
  }
  else {
    $output .= '<b>Credit Card: </b>'. $response->paymentProfile->payment->creditCard->cardNumber .'<br/>';
    $output .= '<b>Expiration: </b>'. $response->paymentProfile->payment->creditCard->expirationDate .'<br/>';
    $output .= '<b>Billing Address:</b><br/>';
    foreach ((array) $response->paymentProfile->billTo as $key => $value) {
      $row['data'][0] = $key;
      $row['data'][1] = $value;
      $rows[] = $row;
    }
    $output .= theme_table(array(), $rows);
    $output .= '</div>';

    //Display the terminal form
    $output .= '<h3>Charge a stored Credit Card</h3>';
    $output .= l(t('Return to order view screen.'), 'admin/store/orders/'. $order_id);

    $balance = uc_payment_balance($order);
    $output .= '<p>'. t('Use this terminal to process credit card payments. Confirm that the stored credit card information is correct.') .'</p>'
              .'<table style="width:20em"><tbody style="border-top: 0px;"><tr><td><strong>'
             . t('Order total:') .'</strong> </td><td>'
             . uc_currency_format($order->order_total) .'</td></tr><tr><td>'
              .'<strong>'. t('Balance:') .'</strong> </td><td>'
             . uc_currency_format($balance) .'</td></tr></tbody></table>';

    $output .= drupal_get_form('uc_cim_terminal_form', $order_id, $balance, $payment_profile_id);
  }

  return $output;
}

/**
 * Terminal Form
 */
function uc_cim_terminal_form($form_state, $order_id, $amount = 0, $payment_profile_id) {
  $form['amount'] = array(
    '#type' => 'textfield',
    '#title' => t('Charge Amount'),
    '#default_value' => uc_currency_format($amount, FALSE, FALSE),
    '#size' => 10,
    '#weight' => 0,
    '#field_prefix' => variable_get('uc_sign_after_amount', FALSE) ? '' : variable_get('uc_currency_sign', '$'),
    '#field_suffix' => variable_get('uc_sign_after_amount', FALSE) ? variable_get('uc_currency_sign', '$') : '',
  );
  $form['order_id'] = array(
    '#type' => 'hidden',
    '#value' => $order_id,
  );
  $form['payment_profile_id'] = array(
    '#type' => 'hidden',
    '#value' => $payment_profile_id,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Charge Card'),
    '#weight' => 10,
  );

  return $form;
}

/**
 * Terminal Form - validate
 */
function uc_cim_terminal_form_validate($form, $form_state) {
  $formvalues = $form_state['values'];
  if (!is_numeric($formvalues['amount']) || $formvalues['amount'] <= 0) {
    form_set_error('amount', t('You must enter a positive number for the amount. Process refunds from the payment tab.'));
  }

  $order = uc_order_load($formvalues['order_id']);
  if ($order === FALSE) {
    form_set_error('', t('Invalid order ID.  Unable to process payment.'));
  }
}

/**
 * Terminal Form - submit
 */
function uc_cim_terminal_form_submit($form, $form_state) {
  $formvalues = $form_state['values'];
  $data = array('ppid' => $formvalues['payment_profile_id']);
  $data = serialize($data);
  uc_payment_process('credit', $formvalues['order_id'], $formvalues['amount'], $data, FALSE, 'cim', TRUE);
}

/**
 * Edit card info - Admin menu callback
 */
function uc_admin_edit_card($user_id) {

  //Display link back to this customer's orders to
  $output .= '<p>To update any of the credit card information you must re-enter the credit card number.</p>';

  //Get the cim_ppid to edit
  if (is_numeric(arg(5))) $payment_profile_id = arg(5);

  //Get the form
  $output .= '<div id="error-output"></div>';
  $output .= drupal_get_form('uc_cim_edit_card_form', $user_id, $payment_profile_id);

  return $output;

}

/**
 * Card edit form - Menu Callback
 *
 * @TODO: Might make sense to use hook_help, and have the menu item call the form directly
 */
function uc_user_edit_card($payment_profile_id) {
  global $user;

  //Display instructions to the user
  $output .= '<p>To update any of your credit card information you must re-enter your credit card number.</p>';

  //Get the form - with user's id
  $output .= drupal_get_form('uc_cim_edit_card_form', $user->uid, $payment_profile_id);

  return $output;

}

/**
 * Card edit form
 */
function uc_cim_edit_card_form($form_state, $user_id, $payment_profile_id = NULL) {

  //Get the customers CIM profile id, or create a new one
  $profile_id = uc_cim_get_profile_id($user_id);

  //If this customer doesn't have an id yet, create it now
  if (!$profile_id) {
    $response = create_customer_profile_request($user_id);
    $profile_id = $response->customerProfileId;
  }
  else {
    if (is_numeric($payment_profile_id)) {
      //If we're editing a card, get that card info now
      $payment_profile_id = $payment_profile_id;
      $database_profile = uc_cim_get_payment_profiles($profile_id, $payment_profile_id);
      $response = get_customer_payment_profile_request($profile_id, $payment_profile_id);

      if ($response->messages->message->code != 'I00001') {
        $message = 'ERROR: '. $response->messages->message->code .' ('. $response->messages->message->text .')';
        drupal_set_message($message, 'error');
        drupal_goto($_GET['dest']);
      }
    }
  }

  //Create an object in the "order format"
  $order->uid = $user_id;
  $order->payment_details['cc_number'] = (string)$response->paymentProfile->payment->creditCard->cardNumber;
  $order->payment_details['cc_type'] = $database_profile['cc_type'];
  $order->payment_details['cc_exp_month'] = $database_profile['cc_exp_month'];
  $order->payment_details['cc_exp_year'] = $database_profile['cc_exp_year'];
  $order->order->payment_details['cc_type'] = $database_profile['cc_type'];
  $order->billing_first_name = (string)$response->paymentProfile->billTo->firstName;
  $order->billing_last_name = (string)$response->paymentProfile->billTo->lastName;
  $order->billing_company = (string)$response->paymentProfile->billTo->company;
  $order->billing_street1 = (string)$response->paymentProfile->billTo->address;
  $order->billing_city = (string)$response->paymentProfile->billTo->city;
  $order->billing_postal_code = (string)$response->paymentProfile->billTo->zip;
  $country = db_result(db_query("SELECT country_id FROM {uc_countries} WHERE country_name LIKE '%s'", (string) $response->paymentProfile->billTo->country));
  if ($country) {
    $state_id = db_result(db_query("SELECT zone_id FROM {uc_zones} WHERE zone_code = '%s' AND zone_country_id=%d", (string) $response->paymentProfile->billTo->state, $country));
    $order->billing_zone = $state_id;
  }
  $order->billing_country = $country;
  $order->billing_phone = (string)$response->paymentProfile->billTo->phoneNumber;

  //Get the credit card form
  $credit_form['credit_card'] = array(
    '#type' => 'fieldset',
    '#title' => t('Credit card'),
    '#attributes' => array('id' => 'credit-card'),
  );
  $credit_form['credit_card'] = array_merge($credit_form['credit_card'], uc_payment_method_credit_form(array(), $order));

  //Get the billing address form
  $pane = uc_checkout_pane_billing('view', $order, NULL);
  unset($pane['contents']['copy_address']);
  $pane['contents']['billing_address_select'] = uc_select_address($user_id, 'billing', "uc_cim_apply_address('billing', this.value);", $title = NULL, $icon_suffix = FALSE);
  $pane_holder['billing-pane'] = array(
    '#type' => 'fieldset',
    '#title' => t('Billing address'),
    '#attributes' => array('id' => 'billing-pane'),
    '#theme' => $pane['theme'],
  );
  $address_form['billing-pane'] = array_merge($pane_holder['billing-pane'], $pane['contents']);

  //Merge the forms and add some hidden feilds to carry our profile id's
  $form = array_merge($credit_form, $address_form);
  $form['profile_id'] = array(
    '#type' => 'hidden',
    '#value' => $profile_id,
  );
  $form['payment_profile_id'] = array(
    '#type' => 'hidden',
    '#value' => $payment_profile_id,
  );

  //Add a submit button
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit Changes'),
  );

  //Delete button
  $form['delete'] = array(
    '#type' => 'submit',
    '#value' => t('Delete Credit Card'),
    '#title' => t('Remove this credit card from you account.'),
    '#attributes' => array('class' => 'negative'),
  );

  //Cancel button - link that returns to where you came from
  $form['cancel'] = array(
    '#value' => l('Cancel', $_GET['dest'], array('attributes' => array('class' => 'button negative'))),
  );

  //Add the javascript and css
  drupal_add_js(drupal_get_path('module', 'uc_cim') .'/uc_cim.js');
  //drupal_add_js(drupal_get_path('module', 'uc_payment') .'/uc_payment.js');
  drupal_add_css(drupal_get_path('module', 'uc_cart') .'/uc_cart.css');

  return $form;
}

/**
 * Card edit form - validate
 */
function uc_cim_edit_card_form_validate($form, &$form_state) {
  $formvalues = $form_state['values'];
  //No validation for deleting...
  if ($_POST['op'] == t('Delete Credit Card')) {
    return;
  }

  //Dont attempt to validate credit cards with masked values (for when a customer is updating through user account interface)
  if (stristr($formvalues['cc_number'], 'XXXX')) {
    return;
  }
  $form_state['values']['cc_number'] = str_replace( ' ', '', trim( $form_state['values']['cc_number'] ) );
  if (!_valid_card_number($form_state['values']['cc_number'])) {
    form_set_error('cc_number', t('The credit card number is not valid.'));
  }
}

/**
 * Card edit form - submit
 */
function uc_cim_edit_card_form_submit(&$form, &$form_state) {
  $formvalues = $form_state['values'];
  //If the user pressed delete, remove the card from Auth.net and database
  if ($_POST['op'] == t('Delete Credit Card')) {
    $response = delete_customer_payment_profile_request($formvalues['profile_id'], $formvalues['payment_profile_id']);
    $form_state['redirect'] = $_GET['dest'];
    return;
  }

  //Create an object in the "order format" to pass to the create_payment_profile api.
  uc_cim_create_order_object($order, $formvalues);

  //Update payment profile
  if (is_numeric($formvalues['payment_profile_id'])) {
    $response = update_customer_payment_profile_request($formvalues['profile_id'], $order, $formvalues['payment_profile_id']);
  }

  //Or create a new payment profile
  else {
    $response = create_customer_payment_profile_request($formvalues['profile_id'], $order, $formvalues['payment_profile_id']);
  }

  //Process the response
  if ($response->messages->message->code != 'I00001') {
    drupal_set_message(t('There was an error processing your request. Pleast contact customer support for assistance'), 'error');
    watchdog('uc_cim',
      'Error edit credit card information for profile id !profile_id: !message',
      array(
        '!profile_id' => $formvalues['profile_id'],
        '!message' => $response->messages->message->text,
      ),
      WATCHDOG_ERROR
    );
  }
  else {
    drupal_set_message('Billing information successfully updated.');
  }

  $form_state['redirect'] = $_GET['dest'];
}

/**
 * Manage cards form
 * Menu callback: user/x/creditcards
 * Allows user to add new and edit existing credit cards
 */
function uc_cim_manage_page_form($form_state, $user) {
  drupal_add_css(drupal_get_path('module', 'uc_cim') .'/uc_cim_manage_page.css');
  drupal_add_js(drupal_get_path('module', 'uc_cart') .'/uc_cart.js');
  $order->payment_details = FALSE;
  $form = uc_cim_credit_card_form($user->uid, $order, 'billing');
  $form['cards']['#default_value'] = 0;
  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $user->uid,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Save new card',
  );
  //alterations go here...
  return $form;
}

/**
 * Manage cards form - submit
 */
function uc_cim_manage_page_form_submit(&$form, &$form_state) {
  uc_order_pane_cim('edit-process', $form_state['values']);
}

/**
 * Refund payments form
 */
function uc_cim_refund_form(&$form_state, $order_id, $receipt_id) {

  //How much to refund?
  $order = uc_order_load($order_id);
  if ($order === FALSE) {
    drupal_set_message(t('Order %id does not exist.', array('%id' => $order_id)));
    drupal_goto('admin/store/orders');
  }
  $payment = uc_payment_load($receipt_id);

  $form['order_total'] = array('#value' => uc_currency_format($order->order_total));
  $form['payment'] = array('#value' => uc_currency_format($payment->amount));

  $form['amount']  = array(
    '#type' => 'textfield',
    '#title' => t('Amount'),
    '#size' => 7,
    '#required' => TRUE,
    '#description' => t('The amount to be refunded (must be a positive number)'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Process Refund',
  );

  $form['cancel'] = array(
    '#value' => l('cancel', 'admin/store/orders/'. $order_id .'/payments/'),
  );

  $form['receipt_id'] = array(
    '#type' => 'hidden',
    '#value' => $receipt_id,
  );

  return $form;

}

/**
 * Refund payments form - validate
 */
function uc_cim_refund_form_validate($form, &$form_state) {
  $formvalues = $form_state['values'];
  if (!is_numeric($formvalues['amount']) || $formvalues['amount'] < 0 ) {
    form_set_error('amount', t('You must enter a postive number for the amount.'));
  }
  return TRUE;
}

/**
 * Refund payments form - submit
 */
function uc_cim_refund_form_submit($form, &$form_state) {
  $formvalues = $form_state['values'];
  global $user; //For payment comment, NOT refund submission

  //Get the payment receipt data - including the transaction id stored in the serialized data table
  $payment = uc_payment_load($formvalues['receipt_id']);
  $data = unserialize($payment->data);

  //If the amount is for the total, try to void the transaction first
  if ($formvalues['amount'] == $payment->amount) {
    $result = uc_cim_refund($payment->order_id, $data['transaction_id'], $data['last_four'], $formvalues['amount'], 'VOID');
  }

  //Otherwise (or if that failed), try to process a refund
  if ($formvalues['amount'] != $payment->amount || $result['success'] == FALSE) {
    $result = uc_cim_refund($payment->order_id, $data['transaction_id'], $data['last_four'], $formvalues['amount']);
  }

  //Record the refund in the payments table, display the result to the admin, and save the comment
  if ($result['success'] === TRUE) {
    uc_payment_enter($payment->order_id, 'Credit Card', -1 * $formvalues['amount'], $user->uid, '', 'REFUND processed');
  }
  drupal_set_message($result['message']);
  uc_order_comment_save($order_id, $user->uid, $message, 'admin');

  $form_state['redirect'] = 'admin/store/orders/'. $payment->order_id .'/payments/';
}
