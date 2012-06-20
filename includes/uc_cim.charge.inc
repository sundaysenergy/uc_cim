<?php

/**
 * @file
 * UC CIM Charge
 */


/**
 * Helper to uc_cim_charge
 *
 * @TODO: This function is absurdly huge. lets break it up into more managable pieces
 */
function _uc_cim_charge($order_id, $amount, $data = NULL, $update_cim_order_table = TRUE) {
  global $user; //For comment logging only

  $gateway_message = '';
  $bank_message = '';

  //CURL is required
  if (!function_exists('curl_init')) {
    drupal_set_message(t('There has been an error.  Please contact customer support for assistance.'));
    $gateway_message = 'communications error';
    return array('success' => FALSE, 'gateway_message' => $gateway_message);
  }

  //Load the order data for this transaction
  if (is_object($order_id)) $order = $order_id; //Support for passing an order object instead of just an ID - used for API calls
  else $order = uc_order_load($order_id);
  $user_id = $order->uid;

  //Get the payment profile id, but let $data override whatever was stored in the order object (so admins can charge whichever stored card they want.)
  $payment_profile_id = $order->cim_ppid;

  if ($data && is_string($data)) { //@todo - Sometimes $data is not a string and throws an error.  Maybe ubercart is using different data structure.  investigate?
    $data = unserialize($data);
    if ($data['ppid']) {
      $payment_profile_id = $data['ppid'];
    }
  }

  //Check if the user is logged in.
  if ($order->uid == 0) { //Customer
    if ($user->uid == 0) { //Anonymous - setup their account
      // Send anonymous users to login page when anonymous checkout is disabled.
      if (!variable_get('uc_checkout_anonymous', TRUE)) {
        drupal_set_message(t('You must login before you can proceed to checkout.'));
        drupal_set_message(t('If you do not have an account, you can <a href="!url">click here</a> to create one.', array('!url' => url('user/register', array('absolute' =>  TRUE)))));
        $_SESSION['checkout-redirect'] = TRUE;
        drupal_goto('user');
      }
      // Check for an existing user account with the e-mail address from checkout.
      $result = db_query("SELECT uid FROM {users} WHERE mail = '%s'", $order->primary_email);

      // If it was found, update the order.
      if ($account = db_fetch_object($result)) {
        $order->uid = $account->uid;
        db_query("UPDATE {uc_orders} SET uid = %d WHERE order_id = %d", $order->uid, $order->order_id);
        $message_type = 'existing_user';
      }
      else {
        // Get a valid new username.
        if (empty($order->data['new_user']['name'])) {
          $name = uc_store_email_to_username($order->primary_email);
        }
        else {
          $name = $order->data['new_user']['name'];
        }

        // Setup the account fields array and save it as a new user.
        $fields = array(
          'name' => $name,
          'mail' => $order->primary_email,
          'init' => $order->primary_email,
          'pass' => empty($order->data['new_user']['pass']) ? user_password(variable_get('uc_pwd_length', 6)) : $order->data['new_user']['pass'],
          'roles' => array(),
          'status' => variable_get('uc_new_customer_status_active', TRUE) ? 1 : 0,
        );
        $account = user_save('', $fields);

        //Set a message for the user to let them know their username and password
        $userid = $account->uid;
        $message = t("Your user account has been created. Your username is '%name' and your password is '%pass'. These can be edited", array(
            '%name' => $name,
            '%pass' => $fields['pass'],
          )
        );
        $message .= ' '.  l(t('here'), "user/$userid/edit") .'.';
        drupal_set_message($message);

        // Send the customer their account details if enabled.
        if (variable_get('uc_new_customer_email', TRUE)) {
          // Manually set the password so it appears in the e-mail.
          $account->password = $fields['pass'];

          // Send the e-mail through the user module.
          drupal_mail('user', 'register_no_approval_required', $order->primary_email, NULL, array('account' => $account), uc_store_email_from());
        }

        // Update the order's uid in this request and in the database.
        $order->uid = $account->uid;
        db_query("UPDATE {uc_orders} SET uid = %d WHERE order_id = %d", $order->uid, $order->order_id);

        // Doesn't work if we try to login the user - do not uncomment
        //$form_state = array('values' => $fields);
        //drupal_execute('user_login', $form_state);
        user_authenticate($fields);

        //Set the user_id variable for the CIM
        $user_id = $order->uid;
      }
    }
    else {                //Admin order
      $result = array(
        'success' => FALSE,
        'comment' => t('You must select select or create a customer account to process an order.'),
        'message' => t('You must select select or create a customer account to process an order.'),
        'uid' => $user->uid,
      );
      return $result;
    }
  }

  //Look up CIM customer id for the user.
  $profile_id = uc_cim_get_profile_id($user_id);

  //If there is a CIM id and a payment profile id, load the saved profile
  if ($profile_id && ($payment_profile_id != 0)) {
    $profile = uc_cim_get_payment_profiles($profile_id, $payment_profile_id);
  }

  //If there is a CIM id, but no payment profile id was passed, try to match an existing payment profile to the credit card entered. This means a customer can only have one entry with the same last_four.  That's better than getting "duplicate entry" errors back from auth.net.
  if ($profile_id && ($payment_profile_id == 0)) {
    $payment_profiles = uc_cim_get_payment_profiles($profile_id);
    if ($payment_profiles) {
      foreach ($payment_profiles as $profile) {
        if ($profile['last_four'] == substr($order->payment_details['cc_number'] , -4)) {

          //Save the found payment profile id
          $payment_profile_id = $profile['customer_payment_profile_id'];

          //Update our CIM stored data with the information entered to make sure it's accurate
          $response = update_customer_payment_profile_request($profile_id, $order, $payment_profile_id);

          break;
        }
      }
    }
  }

  //If there's no user profile, create it now
  if (!$profile_id) {
    $response = create_customer_profile_request($user_id);
    if ($response->messages->message->code == 'I00001') {
      $profile_id = $response->customerProfileId;
    }
    elseif ($response->messages->message->code == 'E00039') {
        // Authorize.net already has a customer profile for this user and
        // won't let us create a new one, so let's load our database from that.
        $matches = array();
        preg_match('|A duplicate record with id (\d+) already exists.|', $response->messages->message->text, $matches);
        $profile_id = (int)$matches[1];
        // store it locally
        $sql = "INSERT INTO {". uc_cim_table('uc_cim') ."} SET uid=%d, customer_profile_id=%d";
        db_query($sql, $user_id, $profile_id);
    }
    else {
      // Unable to create profile.
      $gateway_message = (string)$response->messages->message->text;
      $result = array(
        'success' => FALSE,
        'message' => t('Unable to store payment info on secure server.  For your safety, this transaction cannot be completed at this time.  Please try again soon.  This error has been logged and will be investigated.'),
        'gateway_message' => $gateway_message,
      );
      watchdog('uc_cim', t('Unable to create customer profile during checkout.  Order: !order_id  User: !user_id  Code: !code  Text: !text'), array('!order_id' => $order->order_id, '!user_id' => $order->uid, '!code' => (string)$response->messages->message->code, '!text' => (string)$response->messages->message->text), WATCHDOG_ERROR);
      return $result;
    }
  }

  //If there's no payment profile, create it now
  if ($payment_profile_id == 0) {
    $response = create_customer_payment_profile_request($profile_id, $order);
    $payment_profile_id = $response->customerPaymentProfileId;
    $new_cim_ppid = TRUE;
  }

  //Process transaction using the payment profile. If it's a $0 order (and it's not a new cim_ppid, which was just validated), just validate the profile.
  if ($profile_id && $payment_profile_id != 0 && $amount > 0) {

    //If we're in test mode (not developer test mode or production) just validate the card.  The best we can do...
    if (variable_get('cim_transaction_mode', 'test') == 'test_mode') $response = validate_customer_payment_profile_request($profile_id, $payment_profile_id);
    else $response = create_customer_profile_transaction_request($profile_id, $payment_profile_id, $order, $amount);

  }
  elseif ($profile_id && $payment_profile_id != 0 && $amount <= 0 && !$new_cim_ppid) {
    $response = validate_customer_payment_profile_request($profile_id, $payment_profile_id);
  }

  if ($response) {
    $gateway_message = (string)$response->messages->message->text;
  }
  else {
    $gateway_message = t('Invalid response from gateway.');
  }

  //Process the response
  if (!isset($response->directResponse) && isset($response->messages->message->code) && $response->messages->message->code != 'I00001') {
    //If we failed to create a profile, we just have the CIM gateway error message
    $directResponse[0] = 0;
    $directResponse[3] = $response->messages->message->text;
  }
  elseif ($response->directResponse) {
    //If we processed a transaction, we'll have a directResponse from the bank with the details.
    $directResponse = explode(variable_get('cim_delimiter', ','), $response->directResponse);
    if (!is_numeric($directResponse[0])) {
      // wrong delimiter.  try the other one and log the problem
      // If this still fails, the response should be handled correctly since
      //   it is cast to an integer, but authorization code will be lost.
      $delimiter = (variable_get('cim_delimiter', ',') == ',') ? '|' : ',';
      $directResponse = explode($delimiter, $response->directResponse);
      watchdog('uc_cim', 'Incorrect delimiter "'. variable_get('cim_delimiter', ',') .'" set for response: '. $response->directResponse .'. Raw CIM response: '. $response->messages->message->text, array(), WATCHDOG_WARNING);
    }
    $bank_message = $directResponse[3];
  }
  elseif ($response->validationDirectResponse) {
    //If we failed to process the transaction but did perform a validation during payment profile creation, we'll have a validationDirectResponse with the details. Note that the respose for validate_customer_payment_profile_request is in directResonse. Go figure. That's ok, though, it's handled above.
    $directResponse = explode(variable_get('cim_delimiter', ','), $response->validationDirectResponse);
    if (!is_numeric($directResponse[0])) {
      // wrong delimiter.  try the other one and log the problem
      // If this still fails, the response should be handled correctly since
      //   it is cast to an integer, but authorization code will be lost.
      $delimiter = (variable_get('cim_delimiter', ',') == ',') ? '|' : ',';
      $directResponse = explode($delimiter, $response->validationDirectResponse);
      watchdog('uc_cim', 'Incorrect delimiter "'. variable_get('cim_delimiter', ',') .'" set for response: '. $response->directResponse .'. Raw CIM response: '. $response->messages->message->text, array(), WATCHDOG_WARNING);
    }
    $bank_message = $directResponse[3];
  }
  else {
    //This should never happen... but just in case...
    $directResponse[0] = 0;
    $directResponse[3] = 'Error: '. $response->messages->message->text;
  }

  $response_code = $directResponse[0];
  $response_text = $directResponse[3];
  $approval_code = $directResponse[4];
  $transaction_id = $directResponse[6];
  $amount_charged = $directResponse[9];

  if ($response_code != 1) {
    $message = t('Credit card declined: !amount', array('!amount' => uc_currency_format($amount_charged)));
    $result = array(
      'success' => FALSE,
      'comment' => t('Credit card payment failed: !text', array('!text' => $response_text)),
      'message' => t('Credit card payment failed: !text', array('!text' => $response_text)),
      'uid' => $user->uid,
      'gateway_message' => $gateway_message,
      'bank_message' => $bank_message,
    );
    watchdog('uc_cim', 'CIM Charge failure. Raw gateway response: '. $response->directResponse .'. Raw CIM response: '. $response->messages->message->text, array(), WATCHDOG_NOTICE);
  }
  else {
    if ($amount > 0) {
      $message = t('Credit card charged: !amount', array('!amount' => uc_currency_format($amount_charged)));
      $last_four = $profile['last_four'] ? $profile['last_four'] : substr($order->payment_details['cc_number'] , -4);
      $result = array(
        'success' => TRUE,
        'comment' => t('Credit card payment processed successfully. Approval code: !code', array('!code' => $approval_code)),
        'message' => t('Credit card payment processed successfully. Approval code: !code', array('!code' => $approval_code)),
        'uid' => $user->uid,
        'data' => array(
          'transaction_id' => $transaction_id,  //Save the transaction id, so we can perform refunds
          'last_four' => $last_four,            //Save the last four CC #'s of this payment (since we can now use more than 1 CC per order), so we can issue refunds.
        ),
        'gateway_message' => $gateway_message,
        'bank_message' => $bank_message,
      );
    }
    else {
      $message = t('Credit card verified: !text', array('!text' => $response_text));
      $result = array(
        'success' => TRUE,
        'comment' => $message,
        'message' => $message,
        'uid' => $user->uid,
        'gateway_message' => $gateway_message,
        'bank_message' => $bank_message,
      );
    }
  }

  //Save the comment if there is an order_id.  If this was called from an API call, there may be no order_id
  if ($order->order_id) uc_order_comment_save($order->order_id, $user->uid, $message, 'admin'); //currently doesn't save the uid (0)

  //Save the cim_ppid to the order object, so that we can know later which credit card was used on this order. We don't have access to the pass-by-reference object here, so we need to save it to the db.
  //BUT only if the $update_cim_order_table flag is set. This allows uc_cim_charge to be used as an API call (ie, not used in a normal checkout).
  if ($payment_profile_id != 0 && $update_cim_order_table == TRUE) {
    db_query("UPDATE {uc_payment_cim} SET cim_ppid = %d WHERE order_id = %d", $payment_profile_id, $order->order_id);
    if (db_affected_rows() == 0) {
      db_query("INSERT INTO {uc_payment_cim} (order_id, cim_ppid) VALUES (%d, %d)", $order->order_id, $payment_profile_id);
    }
  }

  //Add cim profile id used to the $result array, so that API calls know what was used
  $result['cim_ppid'] = $payment_profile_id;

  return $result;
}
