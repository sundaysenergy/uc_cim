<?php

/**
 * @file
 * UC CIM Pages
 */

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