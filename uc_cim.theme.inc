<?php

/**
 * @file
 * UC CIM Theme
 */

function theme_uc_cim_credit_form($form) {
  $path = base_path() . drupal_get_path('module', 'uc_credit');
  $output .= '<table class="inline-pane-table" cellpadding="2">';
  /* if (strlen($form['cc_policy']) > 0) {
    $output .= '<tr><td colspan="2">'. variable_get('uc_credit_policy', '')
              .'</td></tr>';
  }*/
  if (variable_get('uc_credit_type_enabled', FALSE)) {
    $form['cc_type']['#title'] = '';
    $output .= '<tr><td class="field-label">'. t('Card Type:') .'</td><td>'
             . drupal_render($form['cc_type']) .'</td></tr>';
  }
  if (variable_get('uc_credit_owner_enabled', TRUE)) {
    $form['cc_owner']['#title'] = '';
    $output .= '<tr><td class="field-label">'. t('Card Owner:') .'</td><td>'
             . drupal_render($form['cc_owner']) .'</td></tr>';
  }
  $output .= '<tr><td class="field-label">'. t('Card Number:') .'</td><td>'
           . drupal_render($form['cc_number']) .'</td></tr>';
  $output .= '<tr><td class="field-label">'. t('Expiration Date:') .'</td><td>'
           . drupal_render($form['cc_exp_month']) .' '
           . drupal_render($form['cc_exp_year']) .'</td></tr>';
  if (variable_get('uc_credit_cvv_enabled', TRUE)) {
    drupal_add_js(drupal_get_path('module', 'uc_payment') .'/uc_payment.js');
    $form['cc_cvv']['#title'] = '';
    if (  $form['cc_cvv']['#required']) {
      $sRequired = '<span class="form-required">*</span>';
    }
    $output .= '<tr><td class="field-label">'. $sRequired . t('CVV:') .'</td><td>'. drupal_render($form['cc_cvv'])
              .' <img src="'. $path .'/images/info.png" onclick="cvv_info_popup();" style="cursor: pointer; position: relative; top: 3px;"> <a style="cursor: pointer; font-weight: normal;" onclick="cvv_info_popup();">'
              . t("What's the CVV?") .'</a></td></tr>';
  }
  if (variable_get('uc_credit_bank_enabled', FALSE)) {
    $form['cc_bank']['#title'] = '';
    $output .= '<tr><td class="field-label">'. t('Issuing Bank:') .'</td><td>'
             . drupal_render($form['cc_bank']) .'</td></tr>';
  }
  $output .= '</table>';

  return $output;
}

function theme_uc_cim_edit_card_form($form) {
  //Prep the credit card portion of the form for themeing and then call the special theme function, then render the form
  $form['credit_card']['cc_policy']['#value'] = '';
  $form['credit_card']['cc_number']['#title'] = '';
  $form['credit_card']['cc_exp_month']['#title'] = '';
  $form['credit_card']['cc_exp_year']['#title'] = '';
  $form['credit_card']['#theme'] = 'uc_cim_credit_form';
  $output .= drupal_render($form['credit_card']);

  //Display rest of form
  $output .= drupal_render($form);
  return $output;
}

function theme_uc_cim_refund_form($form) {
  $output .= '<p><strong>'. t('Order total:') .'</strong> '. drupal_render($form['order_total'])
           .'<br /><strong>'. t('Payment Received:') .'</strong> '
          . drupal_render($form['payment']) .'</p>';
  $output .= '<p>Enter the amount you would like to refund from this payment received. The refund amount cannot be greater than the payment itself less any previous refunds. To attempt to void the entire transaction, enter the full payment amount. Note: refunds cannot be made on payments older than 120 days.</p>';
  $output .= '<div>'. drupal_render($form) .'</div>';
  return $output;
}