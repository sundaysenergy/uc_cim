<?php

/**
 * @file
 * UC CIM - Order Panes
 */

/**
 * Order Pane - CIM Stored Credit Card processing
 */
function _uc_order_pane_cim($op, $arg1) {

  drupal_add_css(drupal_get_path('module', 'uc_cim') .'/uc_cim.css');
  drupal_add_css(drupal_get_path('module', 'uc_cart') .'/uc_cart.css');
  drupal_add_js(drupal_get_path('module', 'uc_cim') .'/uc_cim.js');

  switch ($op) {
    case 'view':
      //Display order payment balance
      if (variable_get('uc_payment_tracking', TRUE)) {
        $output = t('Balance:') .' '. uc_currency_format(uc_payment_balance($arg1));
        $output .= ' ('. l(t('View'), 'admin/store/orders/'. $arg1->order_id .'/payments') .')<br />';
      }

      //Add list of credit cards
      $profile_id = uc_cim_get_profile_id($arg1->uid);
      if ($profile_id) {
        $profiles = uc_cim_get_payment_profiles($profile_id);
      }
      if (is_array($profiles)) {
        foreach ($profiles as $profile) {
          if ($arg1->cim_ppid == $profile['customer_payment_profile_id']) $output .= '<span class="cim-card-used">';
            $output .= $profile['cc_type'] .': x'. $profile['last_four'] .' ';
            $output .= l('(charge)', 'admin/store/orders/'. $arg1->order_id .'/cim-charge/'. $profile['customer_payment_profile_id'], array(), 'dest=admin/store/orders/'. $arg1->order_id) .' ';
            $output .= l('(edit)', 'admin/store/orders/'. $arg1->uid .'/cim-edit/'. $profile['customer_payment_profile_id'], array('attributes' => array('title' => 'Edit credit card information'), 'query' => 'dest=admin/store/orders/'. $arg1->order_id)) .'<br/>';
          if ($arg1->cim_ppid == $profile['customer_payment_profile_id']) {
            $output .= '</span>';
          }
        }
      }
      $output .= l('Add a credit card', 'admin/store/orders/'. $arg1->uid .'/cim-edit', array('query' => 'dest=admin/store/orders/'. $arg1->order_id)) .'<br/>';
      return $output;

    case 'customer':
      $profile_id = uc_cim_get_profile_id($arg1->uid);
      if ($profile_id && $arg1->cim_ppid != 0) {
        $profile = uc_cim_get_payment_profiles($profile_id, $arg1->cim_ppid);
      }
      if ($profile) {
        $output = $profile['cc_type'] .': XXXX'. $profile['last_four'];
      }
      else {
        $output = $arg1->payment_details['cc_type'] .': XXXX'. $arg1->payment_details['cc_number'];
      }
      $output .= drupal_get_form('uc_cim_switch_payment_profile_form', $arg1, $profile);
      return $output;

    case 'edit-form':
      $form = uc_cim_credit_card_form($arg1->uid, $arg1, 'billing', 'delivery');

      //Add the required tag, if there are NO existing credit cards, to the credit card form
      if (count($form['cards']['#options']) < 1) {
        $elements = element_children($form['credit_card']);
        foreach ($elements as $element) {
          $form['credit_card'][$element]['#required'] = TRUE;
        }
      }
      return $form;

    case 'edit-theme':
      $output = '<div class="order-edit-cim">';
      $output .= drupal_render($arg1['cards']);
      $output .= drupal_render($arg1['credit_card']);
      $output .= drupal_render($arg1['billing_address']);
      $output .= '</div>';
      return $output;

    case 'edit-process':

      //If an existing card was selected, just save that selection
      if ($arg1['cards'] != 0) $changes['cim_ppid'] = $arg1['cards'];

      //Otherwise, save the whole form
      else {
        //Save the form
        $changes['cim_ppid'] = $arg1['cards'];
        $changes['payment_details']['cc_number'] = check_plain($arg1['cc_number']);
        $changes['payment_details']['cc_type'] = check_plain($arg1['cc_type']);
        $changes['payment_details']['cc_owner'] = check_plain($arg1['cc_owner']);
        $changes['payment_details']['cc_exp_month'] = check_plain($arg1['cc_exp_month']);
        $changes['payment_details']['cc_exp_year'] = check_plain($arg1['cc_exp_year']);
        $changes['payment_details']['cc_cvv'] = check_plain($arg1['cc_cvv']);
        $changes['payment_details']['cc_bank'] = check_plain($arg1['cc_bank']);
        $changes['billing_first_name'] = $arg1['billing_first_name'];
        $changes['billing_last_name'] = $arg1['billing_last_name'];
        $changes['billing_company'] = $arg1['billing_company'];
        $changes['billing_street1'] = $arg1['billing_street1'];
        $changes['billing_street2'] = $arg1['billing_street2'];
        $changes['billing_city'] = $arg1['billing_city'];
        $changes['billing_zone'] = $arg1['billing_zone'];
        $changes['billing_postal_code'] = $arg1['billing_postal_code'];
        $changes['billing_country'] = $arg1['billing_country'];
        $changes['billing_phone'] = $arg1['billing_phone'];

        //Set the payment method to cim, so that we can take care of partial payments
        $changes['payment_method'] = 'cim';

        //Validate the new credit card
        $process = TRUE;
        if (variable_get('uc_credit_owner_enabled', TRUE) && empty($arg1['cc_owner'])) {
          drupal_set_message(t('Enter the owner name as it appears on the card.'), 'error');
          $process = FALSE;
        }
        if (variable_get('uc_credit_cvv_enabled', TRUE) && !_valid_cvv($arg1['cc_cvv'])) {
          drupal_set_message(t('You have entered an invalid CVV number.'), 'error');
          $process = FALSE;
        }
        if (variable_get('uc_credit_bank_enabled', FALSE) && empty($arg1['cc_bank'])) {
          drupal_set_message(t('You must enter the issuing bank for that card.'), 'error');
          $process = FALSE;
        }
        if (variable_get('uc_credit_validate_numbers', TRUE) && !_valid_card_number($arg1['cc_number'])) {
          drupal_set_message(t('You have entered an invalid credit card number.'), 'error');
          $process = FALSE;
        }
        if (!$process) return $changes;

        //Then create and validate the new profile by sending $0 to the charge function with an order object.
        uc_cim_create_order_object($order, $arg1, TRUE);  //Create the order object.
        $order->uid = $arg1['uid'];
        $order->order_id = $arg1['order_id'];
        $result = uc_cim_charge($order, 0);
        if ($result['success'] == TRUE) {
          drupal_set_message($result['message']);

          //Mask the stored credit card
          if (variable_get('uc_credit_checkout_no_store', FALSE)) {
            $changes['payment_details']['cc_number'] = substr($changes['payment_details']['cc_number'], -4);
            $changes['payment_details']['cc_cvv'] = '000';
            //Handled by uc_credt_order save: db_query("UPDATE {uc_payment_credit} SET cc_number = RIGHT(cc_number, 4), cc_cvv = '000' WHERE order_id = %d", $arg1['order_id']);
          }
        }
        else drupal_set_message($result['message'], 'error');
      }

      return $changes;

  }
}