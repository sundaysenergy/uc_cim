<?php

/**
 * @file
 * UC CIM - Recurring
 */

/**
 * Callback function for setting up a recurring fee
 */
function uc_recurring_cim_fee($order, &$fee) {
  // Get a default SKU if none was supplied.
  if (empty($fee->model)) {
    $fee->model = db_result(db_query("SELECT model FROM {uc_products} WHERE nid = %d", $fee->nid));
  }

  $data = array(
    'cim_ppid' => $order->cim_ppid,
    'model' => $fee->model,
  );

  // need to set gateway specific fee information for renewals
  $fee->fee_handler = 'uc_cim';
  $fee->data = $data;

  return TRUE;
}

/**
 * Callback to process a recurring renewal
 */
function uc_recurring_uc_cim_renew($order, $fee) {
  // check that payment profile still exists first
  $profile_id = uc_cim_get_profile_id($order->uid);
  if ($profile_id) {
    $ppid = uc_cim_get_payment_profiles($profile_id, $order->cim_ppid);
  }

  if ($ppid) {
    // Run the charge.
    $result = uc_cim_charge($order, $fee['fee_amount'], $data = NULL, $update_cim_order_table = TRUE);
    // expose a hook that allows other modules to record/act on the responses
    // hook_gateway_renew_response($gateway_module, $order, $gateway_response, $bank_response);
    module_invoke_all('gateway_renew_response', 'uc_cim', $order, $result['gateway_message'], $result['bank_message']);
  }
  else {
    watchdog('uc_cim',
      'Order !order does not have a valid payment profile (!ppid) associated with it.',
      array(
        '!ppid' => $order->cim_ppid,
        '!order' => $order->order_id,
      ),
      WATCHDOG_ERROR,
      l(t('Order #!order', array('!order' => $order->order_id)), 'admin/store/orders/'. $order->order_id)
    );

    uc_order_comment_save($order->order_id, 0, t('Order does not have a valid payment profile (!ppid) for customer !profile_id associated with it.', array('!ppid' => $order->cim_ppid, '!profile_id' => l((int)$profile_id, 'admin/store/customers/cim/'. (int)$profile_id))));

    $message = t('Order !order does not have a valid payment profile (!ppid) associated with it.', array('!order' => $order->order_id, '!ppid' => $order->cim_ppid));
    $result = array(
      'success' => FALSE,
      'comment' => $message,
      'message' => $message,
      'uid' => 0,
      'gateway_message' => t('Invalid payment profile.'),
    );
    // expose a hook that allows other modules to record/act on the responses
    module_invoke_all('gateway_renew_response', 'uc_cim', $order, $result['gateway_message'], $result['bank_message']);
  }

  // Handle the result.
  if ($result['success'] === TRUE) {
    // expose a hook that allows other modules to record/act on the responses
    module_invoke_all('gateway_renew_response', 'uc_cim', $order, $result['gateway_message'], $result['bank_message']);
    uc_payment_enter($fee['order_id'], 'credit', $fee['fee_amount'], 0, $result['data'], t('Recurring fee payment.') .'<br />'. $result['comment']);
  }
  else {
    // expose a hook that allows other modules to record/act on the responses
    module_invoke_all('gateway_renew_response', 'uc_cim', $order, $result['gateway_message'], $result['bank_message']);

    // notify customer and/or admin
    $params = array(
      'order_id' => $fee['order_id'],
      'account' => $account,
      'amount' => $fee['fee_amount'],
      'message' => $result['message'],
    );

    // customer notification
    if (variable_get('uc_cim_send_recurring_error_notifications', 1)) {
      $account = user_load(array('uid' => $order->uid));
      $to = $account->mail;
      drupal_mail('uc_cim', 'recurring_fail_customer', $to, language_default(), $params, NULL, TRUE);
    }
    // admin notification
    if (variable_get('uc_cim_send_admin_recurring_error_notifications', 1)) {
      $admin_account = user_load(array('uid' => 1));
      $to = variable_get('uc_cim_recurring_error_notifications_to', $admin_account->mail);
      drupal_mail('uc_cim', 'recurring_fail_admin', $to, language_default(), $params, NULL, TRUE);
    }

    // mark failed order as failed and store its info
    uc_order_update_status($order->order_id, 'failed');
    $order->order_status = 'failed';
    uc_order_save($order);
    uc_order_comment_save($order->order_id, 0, $result['message']);
    //uc_order_comment_save($order->order_id, 0, t('!amount recurring fee failed. (ID: <a href="!url">!fee</a>)', array('!url' => url('admin/store/orders/recurring/view/fee/'. $fee['rfid']), '!fee' => $fee['rfid'], '!amount' => uc_currency_format($fee['fee_amount']))));

  }

  return $result['success'];
}

/**
 * Callback for uc_recurring_fee_ops().
 */
function uc_recurring_uc_cim_fee_ops($context, $fee) {
  $ops = array();

  switch ($context) {
    case 'fee_admin':
      if ($fee['remaining_intervals'] > 0) {
        $ops[] = l(t('edit'), 'admin/store/orders/recurring/'. $fee['rfid'] .'/edit');
        $ops[] = l(t('cancel'), 'admin/store/orders/recurring/'. $fee['rfid'] .'/cancel');
      }
      else {
        $ops[] = l(t('delete'), 'admin/store/orders/recurring/'. $fee['rfid'] .'/delete');
      }
      break;
    case 'user':
      $ops[] = l(t('Cancel'), 'user/'. $fee['uid'] .'/recurring/'. $fee['rfid'] .'/cancel');
      break;
  }

  return $ops;
}
