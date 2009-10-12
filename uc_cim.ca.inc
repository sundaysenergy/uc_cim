<?php
// $Id$

/**
 * @file
 * Conditional actions hooks for UC CIM
 */

/**
 * Implementation of hook_ca_entity().
 */
function uc_cim_ca_entity() {
  $entities['uc_cim_payment_profile'] = array(
    '#title' => t('Payment profile'),
    '#type' => 'object',
  );

  return $entities;
}

/**
 * Implementation of hook_ca_trigger().
 */
function uc_cim_ca_trigger() {
  $account = array(
    '#entity' => 'user',
    '#title' => t('User'),
  );
  $payment_profile = array(
    '#entity' => 'uc_cim_payment_profile',
    '#title' => t('Payment profile'),
  );

  $triggers['uc_cim_notify_expiring'] = array(
    '#title' => t('E-mail when credit card on payment profile is going to expire.'),
    '#category' => t('Notification'),
    '#arguments' => array(
      'user' => $account,
      'uc_cim_payment_profile' => $payment_profile,
    ),
  );

  return $triggers;
}

/**
 * Implementation of hook_ca_action().
 */
function uc_cim_ca_action() {
  // Send an email about an attempt
  $actions['uc_cim_expiring_email'] = array(
    '#title' => t('Send an order email regarding payment profile.'),
    '#category' => t('Notification'),
    '#callback' => 'uc_cim_action_expiring_email',
    '#arguments' => array(
      'user' => array(
        '#entity' => 'user',
        '#title' => t('User'),
      ),
      'uc_cim_payment_profile' => array(
        '#entity' => 'uc_cim_payment_profile',
        '#title' => t('Payment profile'),
      ),
    ),
  );

  return $actions;
}

/**
 * Send an email with order replacement tokens.
 *
 * The recipients, subject, and message fields take order token replacements.
 */
function uc_cim_action_expiring_email($account, $payment_profile, $settings) {
  $language = language_default();

  // Token replacements for the subject and body
  $settings['replacements'] = array(
    'global' => NULL,
    'user' => $account,
    'uc_cim_payment_profile' => $payment_profile,
  );

  // Replace tokens and parse recipients.
  $recipients = array();
  $addresses = token_replace_multiple($settings['addresses'], $settings['replacements']);
  foreach (explode("\n", $addresses) as $address) {
    $recipients[] = trim($address);
  }

  // Send to each recipient.
  foreach ($recipients as $email) {
    $sent = drupal_mail('uc_order', 'action-mail', $email, $language, $settings, $settings['from']);

    if (!$sent['result']) {
      watchdog('ca', 'Attempt to e-mail @email concerning credit card @ppid failed.', array('@email' => $email, '@ppid' => $payment_profile->customer_payment_profile_id), WATCHDOG_ERROR);
    }
  }

  db_query("UPDATE {". uc_cim_table('uc_cim_payment_profiles') ."} SET exp_notification=%d WHERE customer_payment_profile_id=%d AND customer_profile_id=%d", time(), $payment_profile->customer_payment_profile_id, $payment_profile->customer_profile_id);
}

// Email settings form
function uc_cim_action_expiring_email_form($form_state, $settings = array()) {
  return ca_build_email_form($form_state, $settings, array('global', 'user', 'uc_cim_payment_profile'));
}

function uc_cim_ca_predicate() {
  $expiring_args = array(
    'user' => 'user',
    'uc_cim_payment_profile' => 'uc_cim_payment_profile',
  );

  $configurations['uc_cim_notify_expiring'] = array(
    '#title' => t('Notify customer when a credit card is expiring/about to expire.'),
    '#description' => t('Notify customer when a credit card is expiring/about to expire.'),
    '#class' => 'notification',
    '#trigger' => 'uc_cim_notify_expiring',
    '#status' => 1,
    '#actions' => array(
      array(
        '#name' => 'uc_cim_expiring_email',
        '#title' => t('Send an e-mail to the customer'),
        '#argument_map' => $expiring_args,
        '#settings' => array(
          'from' => uc_store_email_from(),
          'addresses' => '[mail]',
          'subject' => uc_get_message('uc_cim_expiring_subject'),
          'message' => uc_get_message('uc_cim_expiring_message'),
          'format' => 1,
        ),
      ),
    ),
  );

  return $configurations;
}
