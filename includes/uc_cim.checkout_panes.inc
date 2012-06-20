<?php

/**
 * @file
 * UC CIM - Checkout Panes
 */
 
 
/**
 * Checkout Pane Builder Function
 * http://www.ubercart.org/docs/developer/245/checkout
 *
 * @param  $op
 * ops:
 * view - Called to display the pane on the checkout screen. Expected to return a pane object with the attribute fields set to be an array of form elements. $arg1 is the current order object if it exists.
 * review - Called to display a review of the pane's information the order review screen when checking out. Expected to return a string containing HTML output that will be dropped into a div. $arg1 is the current order object.
 * process - Called when the temporary order is created during checkout after the customer continues from the checkout screen and before the review screen is displayed. No return is expected. $arg1 is a reference to the current order object. (So, by modifying $arg1 in the process block, you are modifying the actual order.) $arg2 is the contents of the array of the submitted form information for that pane.
 *
 * @param $arg1 - The order object, taken by reference when the op is 'process'
 *
 * @param $arg2 - Submitted form information from this pane
 */
function _uc_checkout_pane_cim($op, &$arg1, $arg2) {
  switch ($op) {
    case 'view':
      global $user;
      //Get the CIM credit card form
      $arg1->payment_details = uc_credit_cache('save', $_SESSION['sescrd']);
      $contents = uc_cim_credit_card_form($user->uid, $arg1);

      //Add the appropriate description
      if (sizeof($contents['cards']['#options'] > 1)) $description = t('Select a credit card you used previously or enter a new one:');
      else $description = t('Enter your billing information below.');

      return array('description' => $description, 'contents' => $contents);

    case 'process':
      //Set the payment method to cim so that we can add some functionality before calling uc_credit submit function.
      $arg1->payment_method = 'cim';

      //Store the payment profile id for processing later
      $arg1->cim_ppid = $arg2['cards'];
      db_query("UPDATE {uc_payment_cim} SET cim_ppid = %d WHERE order_id = %d", $arg1->cim_ppid, $arg1->order_id);
      if (db_affected_rows() == 0) {
        db_query("INSERT INTO {uc_payment_cim} (order_id, cim_ppid) VALUES (%d, %d)", $arg1->order_id, $arg1->cim_ppid);
      }

      if ($arg2['cards'] == 0) {
        //If the user selected to enter a new card....

        //Store the credit card details in the order object
        $formvalues_temp = array_merge($arg2['credit_card'], $arg2['billing_address']);
        uc_cim_create_order_object($arg1, $formvalues_temp, TRUE);

        //Put CC data in POST and call the uc_credit process function to let it handle encrptyed credit card data
        $_POST['cc_type'] = $arg2['credit_card']['cc_type'];
        $_POST['cc_owner'] = $arg2['credit_card']['cc_owner'];
        $_POST['cc_number'] = $arg2['credit_card']['cc_number'];
        $_POST['cc_exp_month'] = $arg2['credit_card']['cc_exp_month'];
        $_POST['cc_exp_year'] = $arg2['credit_card']['cc_exp_year'];
        $_POST['cc_cvv'] = $arg2['credit_card']['cc_cvv'];
        $_POST['cc_bank'] = $arg2['credit_card']['cc_bank'];

        $return = uc_payment_method_credit('cart-process', $arg1);

        //If another button was pressed, don't validate (support for uc_gift_card module, for instance)
        if ($_POST['op'] != variable_get('uc_checkout_review_button', t('Review order'))) {
          return TRUE;
        }

        return $return;
      }
      else {
        // Using existing payment profile.  Add billing address info to order
        uc_cim_update_order_billing_address($arg1);
      }

      return TRUE;

    case 'review':
      //Dont display our review pane if cim wasn't used as the payment method
      if (  $arg1->payment_method != 'cim') {
        return;
      }

      //Display line items - Copied from uc_payment_checkout_pane
      $line_items = $arg1->line_items;
      $items = _line_item_list();
      foreach ($items as $item) {
        if (isset($item['display_only']) && $item['display_only'] == TRUE) {
          $result = $item['callback']('display', $arg1);
          if (is_array($result)) {
            foreach ($result as $line) {
              $line_items[] = array(
                'title' => $line['title'],
                'amount' => $line['amount'],
                'weight' => $item['weight']
              );
            }
          }
        }
      }
      usort($line_items, 'uc_weight_sort');
      foreach ($line_items as $line_item) {
        $review[] = array('title' => $line_item['title'], 'data' => uc_currency_format($line_item['amount']));
      }

      //Add our pre-order total line item -- Not working great because of complications (taxes, discounts, etc.)
      /* $charge_now = uc_cim_in_stock_amount($arg1);
      $review[] = array('border' => 'top', 'title' => 'Pre-ordered (charged when item ships)', 'data' => uc_currency_format($arg1->order_total - $charge_now));
      $review[] = array('title' => 'Total charged now', 'data' => uc_currency_format($charge_now)); */

      $review[] = array('border' => 'top', 'title' => t('Paying by'), 'data' => 'Credit Card');

      if ($arg1->cim_ppid == 0) {
        //If the user entered a new card, display the usual card and address data:

        //Display credit card data - Copied from uc_credit cart-review
        if (variable_get('uc_credit_type_enabled', FALSE)) {
          $review[] = array('title' => t('Card Type'), 'data' => $arg1->payment_details['cc_type']);
        }
        if (variable_get('uc_credit_owner_enabled', TRUE)) {
          $review[] = array('title' => t('Card Owner'), 'data' => $arg1->payment_details['cc_owner']);
        }
        $review[] = array('title' => t('Card Number'), 'data' => uc_credit_display_number($arg1->payment_details['cc_number']));
        $review[] = array('title' => t('Expiration'), 'data' => $arg1->payment_details['cc_exp_month'] .'/'. $arg1->payment_details['cc_exp_year']);
        if (variable_get('uc_credit_cvv_enabled', TRUE)) {
          $review[] = array('title' => t('CVV'), 'data' => user_access('view cc numbers') ? $arg1->payment_details['cc_cvv'] : str_repeat('-', strlen($arg1->payment_details['cc_cvv'])));
        }
        if (variable_get('uc_credit_bank_enabled', FALSE)) {
          $review[] = array('title' => t('Issuing Bank'), 'data' => $arg1->payment_details['cc_bank']);
        }

        //Display billing address - Copied from uc_cart_checkout_pane_billing review
        $review[] = array('title' => t('Address'), 'data' => uc_order_address($arg1, 'billing', FALSE));
        if (uc_address_field_enabled('phone') && !empty($arg1->billing_phone)) {
          $review[] = array('title' => t('Phone'), 'data' => check_plain($arg1->billing_phone));
        }
      }
      else {
        //Get the selected card profile and display it's payment summary

        $profile_id = uc_cim_get_profile_id($arg1->uid);
        if ($profile_id) $profile = uc_cim_get_payment_profiles($profile_id, $arg1->cim_ppid);

        if (variable_get('uc_credit_type_enabled', FALSE)) {
          $review[] = array('title' => t('Card Type'), 'data' => $profile['cc_type']);
        }
        $review[] = array('title' => t('Card Number'), 'data' => uc_credit_display_number($profile['last_four']));
        $review[] = array('title' => t('Expiration'), 'data' => $profile['cc_exp_month'] .'/'. $profile['cc_exp_year']);

      }
      return $review;

  }
}