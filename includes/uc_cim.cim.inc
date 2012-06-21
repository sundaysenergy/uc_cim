<?php

/**
 * @file
 * CIM API
 *
 * An API to create customer profiles, customer payment profiles, and transactions
 * for the Authorize.net CIM gateway.
 */

/**
 * create_customer_profile_request
 */
function create_customer_profile_request($user_id) {
  //Load user
  $user = user_load(array('uid' => $user_id));

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<createCustomerProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<profile>'.
    '<merchantCustomerId>'. uc_cim_xml_clean($user->uid) .'</merchantCustomerId>'.
    '<description>'. uc_cim_xml_clean($user->name) .'</description>'.
    '<email>'. uc_cim_xml_clean($user->mail) .'</email>'.
  '</profile>'.
  '</createCustomerProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  //Process the response
  if ($profile_id = $response->customerProfileId) {
    uc_cim_save_profile_id($user_id, $profile_id);
  }
  else {
    $message = 'Customer profile could not be created. User: '. $user_id .', Error: '. $response->messages->message->text;
    watchdog('uc_cim', $message, array(), WATCHDOG_WARNING);
  }

  return $response;
}

/**
 * create_customer_payment_profile_request
 */
function create_customer_payment_profile_request($profile_id, $order) {

  //Validate this payment profile in live mode for production sites.  This performs stronger card validation, but fails for test transactions.
  if (variable_get('uc_cim_transaction_mode', 'test') == 'production') {
    $validation_mode = '<validationMode>liveMode</validationMode>';
  }
  else {
    $validation_mode = '<validationMode>testMode</validationMode>';
  }

  //Make expiration date month 2 digits
  if (strlen($order->payment_details['cc_exp_month']) == 1) {
    $order->payment_details['cc_exp_month'] = '0'. $order->payment_details['cc_exp_month'];
  }

  //Get Country
  $country = uc_get_country_data(array('country_id' => $order->billing_country));

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<createCustomerPaymentProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '<paymentProfile>'.
    '<billTo>'.
      //'<address>'. CIM DOCUMENTATION SAYS THIS IS NEEDED, but appears to by an error
      '<firstName>'. uc_cim_xml_clean($order->billing_first_name) .'</firstName>'.
      '<lastName>'. uc_cim_xml_clean($order->billing_last_name) .'</lastName>'.
      '<company>'. uc_cim_xml_clean($order->billing_company) .'</company>'.
      '<address>'. uc_cim_xml_clean($order->billing_street1) .'</address>'.
      '<city>'. uc_cim_xml_clean($order->billing_city) .'</city>'.
      '<state>'. uc_cim_xml_clean(uc_get_zone_code($order->billing_zone)) .'</state>'.
      '<zip>'. uc_cim_xml_clean($order->billing_postal_code) .'</zip>'.
      '<country>'. uc_cim_xml_clean($country[0]['country_name']) .'</country>'.
      //'</address>'.
      '<phoneNumber>'. uc_cim_xml_clean($order->billing_phone) .'</phoneNumber>'.
    '</billTo>'.
    '<payment>'.
      '<creditCard>'.
        '<cardNumber>'. uc_cim_xml_clean($order->payment_details['cc_number']) .'</cardNumber>'.
        '<expirationDate>'. uc_cim_xml_clean($order->payment_details['cc_exp_year'] .'-'. $order->payment_details['cc_exp_month']) .'</expirationDate>'. //example: 2008-10
      '</creditCard>'.
    '</payment>'.
  '</paymentProfile>'.
  $validation_mode .
  '</createCustomerPaymentProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  //Process the response and save the profile
  $last_four = substr($order->payment_details['cc_number'], -4);

  if ($payment_profile_id = $response->customerPaymentProfileId) {
    //Get the payment profile id from the response and save it
    uc_cim_save_payment_profile($profile_id, $payment_profile_id, $last_four, $order->payment_details['cc_type'], $order->payment_details['cc_exp_year'], $order->payment_details['cc_exp_month']);
  }
  else {
    //If that didn't work, we've got a problem...
    $message = 'Customer payment profile could not be created. User: '. $order->uid .', Error: '. $response->messages->message->text;
    watchdog('uc_cim', $message, array(), WATCHDOG_WARNING);
  }

  return $response;
}

/**
 * create_customer_profile_transaction_request
 */
function update_customer_payment_profile_request($profile_id, $order, $payment_profile_id) {

  //Make expiration date month 2 digits
  if (strlen($order->payment_details['cc_exp_month']) == 1) {
    $order->payment_details['cc_exp_month'] = '0'. $order->payment_details['cc_exp_month'];
  }

  //Get Country
  $country = uc_get_country_data(array('country_id' => $order->billing_country));

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<updateCustomerPaymentProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '<paymentProfile>'.
    '<billTo>'.
      //'<address>'. CIM DOCUMENTATION SAYS THIS IS NEEDED, but appears to by an error
      '<firstName>'. uc_cim_xml_clean($order->billing_first_name) .'</firstName>'.
      '<lastName>'. uc_cim_xml_clean($order->billing_last_name) .'</lastName>'.
      '<company>'. uc_cim_xml_clean($order->billing_company) .'</company>'.
      '<address>'. uc_cim_xml_clean($order->billing_street1) .'</address>'.
      '<city>'. uc_cim_xml_clean($order->billing_city) .'</city>'.
      '<state>'. uc_cim_xml_clean(uc_get_zone_code($order->billing_zone)) .'</state>'.
      '<zip>'. uc_cim_xml_clean($order->billing_postal_code) .'</zip>'.
      '<country>'. uc_cim_xml_clean($country[0]['country_name']) .'</country>'.
      //'</address>'.
      '<phoneNumber>'. uc_cim_xml_clean($order->billing_phone) .'</phoneNumber>'.
    '</billTo>'.
     '<payment>'.
      '<creditCard>'.
        '<cardNumber>'. uc_cim_xml_clean($order->payment_details['cc_number']) .'</cardNumber>'.
        '<expirationDate>'. uc_cim_xml_clean($order->payment_details['cc_exp_year'] .'-'. $order->payment_details['cc_exp_month']) .'</expirationDate>'. //example: 2008-10
      '</creditCard>'.
    '</payment>'.
    '<customerPaymentProfileId>'. uc_cim_xml_clean($payment_profile_id) .'</customerPaymentProfileId>'.
  '</paymentProfile>'.
  '</updateCustomerPaymentProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  //Process the response and save the profile
  $last_four = substr($order->payment_details['cc_number'], -4);

  if ($payment_profile_id) {
    //If we already have the id, just save it
    uc_cim_save_payment_profile($profile_id, $payment_profile_id, $last_four, $order->payment_details['cc_type'], $order->payment_details['cc_exp_year'], $order->payment_details['cc_exp_month']);
  }
  elseif ($payment_profile_id = $response->customerPaymentProfileId) {
    //Otherwise, get the payment profile id from the response and save it
    uc_cim_save_payment_profile($profile_id, $payment_profile_id, $last_four, $order->payment_details['cc_type'], $order->payment_details['cc_exp_year'], $order->payment_details['cc_exp_month']);
  }
  else {
    //If that didn't work, we've got a problem...
    $message = 'Customer payment profile could not be created. User: '. $order->uid .', Error: '. $response->messages->message->text;
    watchdog('uc_cim', $message, array(), WATCHDOG_WARNING);
  }

  return $response;
}

/**
 * create_customer_profile_transaction_request
 */
function create_customer_profile_transaction_request($profile_id, $payment_profile_id, $order, $amount) {

  //Build the order description
  $description = '';
  if (is_array($order->products)) {
    foreach ($order->products as $product) {
      if (!empty($description)) {
        $description .= ' / ';
      }
      $description .= uc_cim_xml_clean($product->title . ' x' . $product->qty);
      if (is_array($product->data['attributes'])) {
        foreach ($product->data['attributes'] as $key => $value) {
          $description .= uc_cim_xml_clean(', ' . $key . ': ' . $value);
        }
      }
    }
  }
  $description = str_replace(variable_get('uc_cim_delimiter', ','), '', $description);  //Since this is returned in the response, delimitters will cause problems
  $description = substr($description, 0, 255);

  //Determine whether to add CVV field
  if (strlen($order->payment_details['cc_cvv']) >= 3 && $order->payment_details['cc_cvv'] != '000') {
    $cvv = '<cardCode>'. uc_cim_xml_clean($order->payment_details['cc_cvv']) .'</cardCode>';
  }
  else $cvv = '';

  //Determine whether to add a recurring flag
  //<recurringBilling>". $this->params['transactionRecurringBilling'] ."</recurringBilling>
  /* if (strlen($order->payment_details['cc_cvv']) >= 3) {
    $cvv = '<cardCode>'. $order->payment_details['cc_cvv']. '</cardCode>';
  }
  else $cvv = ''; */

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<createCustomerProfileTransactionRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<transaction>'.
    '<profileTransAuthCapture>'.
      '<amount>'. uc_cim_xml_clean($amount) .'</amount>'.
      '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
      '<customerPaymentProfileId>'. uc_cim_xml_clean($payment_profile_id) .'</customerPaymentProfileId>'.
      '<order>'.
        '<invoiceNumber>'. uc_cim_xml_clean($order->order_id) .'</invoiceNumber>'.
        '<description>'. uc_cim_xml_clean($description) .'</description>'.
      '</order>'.
      $recurring .
      $cvv .
    '</profileTransAuthCapture>'.
  '</transaction>'.

  '</createCustomerProfileTransactionRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  //Process the response later

  return $response;
}

/**
 * delete_customer_profile_request
 */
function delete_customer_profile_request($profile_id) {
  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<deleteCustomerProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '</deleteCustomerProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  return $response;
}

/**
 * delete_customer_payment_profile_request
 */
function delete_customer_payment_profile_request($profile_id, $payment_profile_id) {
  // TODO: should check here if any recurring fees are using this payment profile?
  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<deleteCustomerPaymentProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '<customerPaymentProfileId>'. uc_cim_xml_clean($payment_profile_id) .'</customerPaymentProfileId>'.
  '</deleteCustomerPaymentProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  //Process the response
  if ($response->messages->message->code != 'I00001') {
    drupal_set_message(t('There was an error processing your request. Pleast contact customer support for assistance'), 'error');
    watchdog('uc_cim',
      'CIM payment profile delete error for profile id !profile_id: !message',
      array(
        '!profile_id' => $profile_id,
        '!message' => $response->messages->message->text,
      ),
      WATCHDOG_ERROR
    );
  }
  else {
    uc_cim_delete_payment_profile($profile_id, $payment_profile_id);
    drupal_set_message(t('Your credit card information has been removed.'));
  }

  return $response;
}

/**
 * get_customer_profile_request
 */
function get_customer_profile_request($profile_id) {

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<getCustomerProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '</getCustomerProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  return $response;
}

/**
 * get_customer_payment_profile_request
 */
function get_customer_payment_profile_request($profile_id, $payment_profile_id) {

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<getCustomerPaymentProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '<customerPaymentProfileId>'. uc_cim_xml_clean($payment_profile_id) .'</customerPaymentProfileId>'.
  '</getCustomerPaymentProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  return $response;
}

/**
 * validate_customer_payment_profile_request
 */
function validate_customer_payment_profile_request($profile_id, $payment_profile_id) {

  //Validate this payment profile in live mode for production sites.  This performs stronger card validation, but fails for test transactions.
  if (variable_get('uc_cim_transaction_mode', 'test') == 'production') {
    $validation_mode = '<validationMode>liveMode</validationMode>';
  }
  else {
    $validation_mode = '<validationMode>testMode</validationMode>';
  }

  //Build XML to post
  $content =
  '<?xml version="1.0" encoding="utf-8"?>'.
  '<validateCustomerPaymentProfileRequest xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd">'.
  merchant_auth_xml() .
  '<customerProfileId>'. uc_cim_xml_clean($profile_id) .'</customerProfileId>'.
  '<customerPaymentProfileId>'. uc_cim_xml_clean($payment_profile_id) .'</customerPaymentProfileId>'.
  $validation_mode .
  '</validateCustomerPaymentProfileRequest>';

  //Send the XML via curl
  $response = uc_cim_curl_send($content);

  return $response;

}

/**
 * uc_cim_curl_send
 */
function uc_cim_curl_send($content) {
  //Build the path to post to, depending on whether we're in test mode or not
  if (variable_get('uc_cim_transaction_mode', 'test') == 'test') {
    $posturl = 'https://apitest.authorize.net/xml/v1/request.api';
  }
  else {
    $posturl = 'https://api.authorize.net/xml/v1/request.api';
  }

  $ch = curl_init();
  curl_setopt($ch, CURLOPT_URL, $posturl);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
  curl_setopt($ch, CURLOPT_HTTPHEADER, Array("Content-Type: text/xml"));
  curl_setopt($ch, CURLOPT_HEADER, 0);  //Set to TRUE to return the header in the output - good for debugging, but messes with xml parsing
  curl_setopt($ch, CURLOPT_POSTFIELDS, $content);
  curl_setopt($ch, CURLOPT_POST, 1);
  curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
  curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 0);

  $authorize = curl_exec($ch);
  if ($error = curl_error($ch)) {
    watchdog('uc_cim', $error, array(), WATCHDOG_ERROR);
    uc_cim_notify_error('Connection error (cURL)', $error);
  }
  else {
    $response = uc_cim_parse_return($authorize);
  }

  if (variable_get('uc_cim_logging_level', 'standard') == 'extended') {
    // log request and response as raw xml
    $sent_xml = preg_replace('|\<cardNumber\>(.*)\<\/cardNumber\>|', '<cardNumber>[Not logged]</cardNumber>', $content);
    $sent_xml = preg_replace('|\<cardCode\>(.*)\<\/cardCode\>|', '<cardCode>[Not logged]</cardCode>', $sent_xml);
    $message = "Request Sent: <br />\n". uc_cim_xml_clean( $sent_xml ) ."\n<br />";
    $message .= "<br />\nResponse Received: <br />\n". uc_cim_xml_clean( $authorize ) ."\n";
    watchdog('CIM-ext', $message, array(), WATCHDOG_NOTICE);
  }

  curl_close($ch);
  return $response;
}

/**
 * uc_cim_parse_return
 */
function uc_cim_parse_return($response) {
  $xml_raw = str_replace('xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns="AnetApi/xml/v1/schema/AnetApiSchema.xsd"', '', $response);
  try {
    $xml_data = new SimpleXMLElement($xml_raw);
  }
  catch (Exception $e) {
    watchdog('uc_cim', $e->getMessage(), array(), WATCHDOG_ERROR);
    uc_cim_notify_error('Connection error (SimpleXML)', $e->getMessage());
    $xml_data = NULL;
  }
  return $xml_data;
}

/**
 * Optionally send notification on critical error, such as Authorize.net
 * not responding.
 * Sends to configured address, or store admin address if none is configured,
 * or site admin address if no store admin is configured.
 */
function uc_cim_notify_error($error_type, $error_message) {
  if (variable_get('uc_cim_send_error_notifications', 1) && flood_is_allowed('cim_error_notice', variable_get('uc_cim_hourly_threshold', 3))) {
    $params['error_type'] = $error_type;
    $params['error_message'] = $error_message;
    $to = variable_get('uc_cim_error_notifications_to', '');
    if (!$to) {
      if (!$to) {
        $account = user_load(array('uid' => 1));
        $to = $account->mail;
      }
    }
    if ($to) {
      drupal_mail('uc_cim', 'error_notice', $to, language_default(), $params, NULL, TRUE);
      flood_register_event('cim_error_notice');
    }
  }
}

/**
 * merchant_auth_xml
 */
function merchant_auth_xml() {
  //Get the login info for the mode we're in
  if (variable_get('uc_cim_transaction_mode', 'test') == 'test') {
    $loginname = variable_get('uc_cim_login_id_TEST', '');
    $transactionkey = variable_get('uc_cim_transaction_key_TEST', '');
  }
  else {
    $loginname = variable_get('uc_cim_login_id', '');
    $transactionkey = variable_get('uc_cim_transaction_key', '');
  }

  return '<merchantAuthentication><name>'. uc_cim_xml_clean($loginname) .'</name><transactionKey>'. uc_cim_xml_clean($transactionkey) .'</transactionKey></merchantAuthentication>';
}

/**
 * Sanitize data being passed as XML values.
 */
function uc_cim_xml_clean($string) {
  return uc_cim_numeric_entities(htmlentities( $string, ENT_QUOTES, 'UTF-8' ));
}

/**
 * Many named entities from HTML are not valid in XML, and many word-sized
 * characters do not have named entities, so convert to numeric entities.
 */
function uc_cim_numeric_entities($string) {
  static $mapping;
  if (!$mapping) {
    $mapping = array();
    foreach (get_html_translation_table(HTML_ENTITIES, ENT_QUOTES) as $char => $entity) {
      $mapping[$entity] = '&#'. ord($char) .';';
    }
  }
  $string = str_replace(array_keys($mapping), $mapping, $string);
  // multibyte characters > 0xFF may not work
  //$string = mb_encode_numericentity ($string, array (0x0100, 0xffff, 0x0100, 0xffff), 'UTF-8');
  return $string;
}