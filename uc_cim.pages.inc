<?php

/**
 * @file
 * UC CIM Pages
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
