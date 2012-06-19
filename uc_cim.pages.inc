<?php

/**
 * @file
 * UC CIM Pages
 */

//Charge a credit card
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

function uc_cim_terminal_form_submit($form, $form_state) {
  $formvalues = $form_state['values'];
  $data = array('ppid' => $formvalues['payment_profile_id']);
  $data = serialize($data);
  uc_payment_process('credit', $formvalues['order_id'], $formvalues['amount'], $data, FALSE, 'cim', TRUE);
}

//Edit card info
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

function uc_user_edit_card($payment_profile_id) {
  global $user;

  //Display instructions to the user
  $output .= '<p>To update any of your credit card information you must re-enter your credit card number.</p>';

  //Get the form - with user's id
  $output .= drupal_get_form('uc_cim_edit_card_form', $user->uid, $payment_profile_id);

  return $output;

}

/**
 * Menu callback
 * user/x/creditcards
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

function uc_cim_manage_page_form_submit(&$form, &$form_state) {
  uc_order_pane_cim('edit-process', $form_state['values']);
}

//Refund payments
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

function uc_cim_refund_form_validate($form, &$form_state) {
  $formvalues = $form_state['values'];
  if (!is_numeric($formvalues['amount']) || $formvalues['amount'] < 0 ) {
    form_set_error('amount', t('You must enter a postive number for the amount.'));
  }
  return TRUE;
}

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
