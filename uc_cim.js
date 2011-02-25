
/* Copied from uc_cart.js and revised to support uc_cim css selectors */

var copy_box_checked = false;

/**
 * Copy the delivery information to the payment information on the checkout
 * screen if corresponding fields exist. REVISED FOR UC_CIM.
 */
function uc_cim_copy_delivery_to_billing(checked, address_book_path, delivery_path) {
  if (!checked) {
    $('#billing-address div.address-pane-table').slideDown();
    copy_box_checked = false;
    return false;
  }

  // Hide the billing information fields.
  $('#billing-address div.address-pane-table').slideUp();
  copy_box_checked = true;

  // Copy over the zone options manually.
  if ($('#edit-' + address_book_path + '-zone').html() != $('#edit-' + delivery_path + '-zone').html()) {
    $('#edit-' + address_book_path + '-zone').empty().append($('#edit-' + delivery_path + '-zone').children().clone());
  }

  // Copy over the information and set it to update if delivery info changes.
  $('input, select, textarea').each(
    function() {
      if (this.id.substring(0, 5 + delivery_path.length) == 'edit-' + delivery_path) {
        $('#edit-' + address_book_path + this.id.substring(5 + delivery_path.length)).val($(this).val());
        $(this).change(function () { update_billing_field(this, address_book_path, 5 + delivery_path.length); });
      }
    }
  );

  return false;
}

function update_billing_field(field, address_book_path, delivery_path_length) {
  if (copy_box_checked) {
    $('#edit-' + address_book_path + field.id.substring(delivery_path_length)).val($(field).val());
  }
}

/**
 * Apply the selected address to the appropriate fields in the cart form.
 */
function uc_cim_apply_address(type, address_str) {
  if (address_str == '0') {
    return;
  }

  eval('var address = ' + address_str + ';');
  var temp = type;
  
  $('#edit-' + temp + '-first-name').val(address.first_name).trigger('change');
  $('#edit-' + temp + '-last-name').val(address.last_name).trigger('change');
  $('#edit-' + temp + '-phone').val(address.phone).trigger('change');
  $('#edit-' + temp + '-company').val(address.company).trigger('change');
  $('#edit-' + temp + '-street1').val(address.street1).trigger('change');
  $('#edit-' + temp + '-street2').val(address.street2).trigger('change');
  $('#edit-' + temp + '-city').val(address.city).trigger('change');
  $('#edit-' + temp + '-postal-code').val(address.postal_code).trigger('change');

  if ($('#edit-' + temp + '-country').val() != address.country) {
    $('#edit-' + temp + '-country').val(address.country);
    try {
      uc_update_zone_select('edit-' + temp + '-country', address.zone);
    }
    catch (err) { }
  }

  $('#edit-' + temp + '-zone').val(address.zone).trigger('change');
}

/**
 * Hide or display the enter new credit card form.
 */
function uc_cim_toggle_new_card(selection) {
  if (selection == '0') $('div.enter-new-credit-card').show();
  else $('div.enter-new-credit-card').hide();
}

/**
 * When the checkout page loads, if js is enabled, hide the enter new credit card form and setup thickbox pop-ups.
 */
if (Drupal.jsEnabled) {
  $(document).ready(function() {
    $('input[@name="panes[cim][cards]"]:checked').val() == 0 ? $('#uc-cart-checkout-form div.enter-new-credit-card').show() : $('#uc-cart-checkout-form div.enter-new-credit-card').hide();
    $('input[@name="cards"]:checked').val() == 0 ? $('#uc-order-edit-form div.enter-new-credit-card').show() : $('#uc-order-edit-form div.enter-new-credit-card').hide();    
  })
}