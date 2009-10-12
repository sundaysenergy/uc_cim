Authorize.net Advanced (CIM) Gateway 

Developed by ChadCrew 
 * For help integrating this module or setting up an Authorize.net CIM gateway and 
   Merchant Account, please contact chadcrew at gmail.com

This module enables advanced functionality for the Authorize.net payment gateway,
based on their Customer Information Manager (CIM) system. With it you can store
your customer's credit card information on Authorize.net's servers. This makes
several important features possible, without exposing a store owner to the
security risk of storing credit #'s in their database.

You'll like this module if you want to:

	-Help your customers checkout more quickly by entering their credit card info
	only once.
	
	-Accept pre-orders for a product and charge the customer when you ship it.
	
	-Process recurring payments. (Currently only available through an API call. This
	will be integreated with Ubercart's recurring payment system when that is
	available).
	
	-Process refunds from within Ubercart.


Requirements:

-CURL
-SimpleXML
-Authorize.net test account or live account with CIM enabled 
	-See http://developer.authorize.net/testaccount/ to request a test account
	-Enable CIM on your account by logging in to your Authorize.net web interface
	and browsing to Tools, then Customer Information Manager


Installation:

1) Install and enable the uc_cim module. You may disable the uc_authorizenet module,
		it is not required to use uc_cim.

2) Gateway settings (admin/store/settings/payment/edit/gateways):
		-Enable the Authorize.net CIM payment gateway
		-Enter your Login ID and Transaction Key
		-Decide whether or not to process out-of-stock items at checkout. If this is enabled
		the customers credit card info will be saved, but not charged, at checkout. An admin
		can then charge the card later when the product is shipped, for example.
		-Select your transaction mode.  The test mode is for developers. If you do not
		have a develeopers test account, you can still perform a test by leaving this setting
		in production mode and setting your Auth.net account to test mode in the Auth.net
		web interface.

3) Payment Method settings (admin/store/settings/payment/edit/methods):
		-Make Authorize.net CIM your default payment gateway
		-ENABLE CVV on checkout form. (optional, but recommended)
		-DISABLE card owner field on checkout form (it's not used).
		-DISABLE issuing bank field on checkout form (it's not used).
		-ENABLE card type field on checkout form.
		-ENABLE Attempt to process credit card payments at checkout.

4) Checkout settings (admin/store/settings/checkout/edit):
		-Disbale anonymous checkout (it is currently not supported). 

5) Checkout Panes (admin/store/settings/checkout/edit/panes): 
		You have a choice!  Either:
	
		A) Disable the Payment Method and Billing Information panes, and enable Payment Information pane.
				
				The Payment Information pane is a new pane defined in this module that allows
				customers to select a	credit card they used previously or enter a new
				credit card and billing	address. This is one of the key benefits of using the CIM system. 
				
				The main drawback to using this new pane is that it currently does not support
				an order total preview.  This may be added soon (your help is welcome!) or you
				can display the order total preview in the Cart Contents pane like I do currently.
		
		or
		
		B) Leave the Payment Method and Billing Information panes enabled, and disable Payment Information pane.
				
				With this choice you get to keep the standard Ubercart checkout interface,
				with CIM just working in the background.  While your users won't be able to
				select previously used credit cards, store admins will be able to charge
				customers after checkout (useful for taking pre-orders that you won't
				charge for until you ship, supporting recurring payments, taking follow-up
				orders by phone...).  You also gain the ability to process refunds from
				within Ubercart.

5) Order panes (admin/store/settings/orders/edit/panes):
		-Disable the Payment pane and enable the Credit Card Payment pane for all screens.
		-Disable the Bill To pane for the Edit Screen. (You may leave it enabled for
		view, invoice, and customer, but it will only contain address data when a
		new credit card has been entered)

6) If you are using the uc_recurring module to set up recurring payments, be sure to 
		select uc_cim as your recurring fee handler at (admin/store/settings/products/edit/features)


Some additional use notes:

-View, charge, and edit stored credit cards from admin/store/orders/[order_id].
	-When editing any card info, you must re-enter the credit card #.

-Process refunds from the payment tab (admin/store/orders/[oder_id]/payments)

-Visit admin/store/orders/cim to see a list of stored profiles. Click a profile
to see stored credit cards. Useful for testing, not much else currently. 


API Example:

See uc_cim_terminal, uc_cim_terminal_form, and uc_cim_terminal_form_submit in
the uc_cim.module for a detailed example. The basic process is:

1) Create a new order or select an order to process a payment for. Save the $order_id.

2) Get payment profiles for a user:

	$profile_id = uc_cim_get_profile_id($user_id);
	$payment_profiles = uc_cim_get_payment_profiles($profile_id);
	
3) Process a payment for one of the saved payment profiles. (You might let your user
select this on a form, for instance). This code just uses the first returned payment
profile:

	$data = array('ppid' => $payment_profiles[0]['payment_profile_id']);
  $data = serialize($data);
  uc_payment_process('credit', $order_id, $amount, $data, FALSE, 'cim', TRUE);


CHANGES:
V0.3
	-Added ability for customers to edit their credit card info from the checkout screen
	-Added a seperate database table for developer account test mode so that profile id's do
	not get mixed between the two accounts.
	-Added an CIM-specific order pane ("Credit Card Payment")
	-Integrated support for pre-orders, based on inventory level set by the uc_stock module.
	-Customers can now edit their credit card info and delete stored profiles.
	-Integrated with uc_recurring module.
	
V0.4
	-Added validation to new credit cards on the order screen.
	
v0.5
	-Fixed refunds (the product url was set wrong).
	-Fixed .info file
	-Fixed javascrip error that hid "add new card" form when it shouldn't have.

v0.6
	-Put files in a uc_cim directory to make untarring easier.
	-Documentation note about selecting the correct uc_recurring handler.

v0.7
	-Fixed response code bug
	-Fixed table {bracketing} bug for multi-site setups
	-Added option to choose the delimitting character for Authorize.net response
	-Made compatible with Ubercart RC5.  This probably broke compatibility with prior Ubercart versions.

v0.8
  -Fixed recurring fee handler bug (would fail for purchases not made with the special CIM pane)
  -Fixed bug that could let some transactions look like they succeeded, when they actually failed.
  -Added a "test mode" for non-developers.  It is still only moderately useful due to CIM limitations.
	
TO DO:

-Add support for anonymous checkout
	-Main problem is that the user account for anonymous users is not created until
	after the order is processed successfully, but this module needs a user ID to
	associate credit cards with that user
	

UPDATES:
-----------------------
New Features

- Uc_cim is now allowed to be used as a regular payment method.  The CIM Credit Card payment method can be enabled (admin/store/settings/payment/edit/methods),
Credit Card should be disabled, the Payment Method pane can be enabled, the CIM Functionality pane must be enabled, and Payment Information pane can be disabled.  
If you previously wanted to use uc_cim with additional payment methods, like checks or cod, now you can!
Alternately, you can still use the method outlined in the instructions above (README.txt).

- Added a new user credit card manager page: user/x/creditcards.  Exists as a tab on your "My account" page that allows you to view, edit, or delete all of the credit cards that uc_cim has stored on your behalf.

- Authorize.net profiles are now able to be deleted from the local database even if there is no remote record of it (this can be helpful if you are using multiple sandboxes and data is getting shuffled around)

- Added support for anonymous checkout (thanks to todda00!)

- Added support for recurring fee handling via the new uc_recuring module (thanks again todda00)

-----------------------
Bug fixes

- Check whether anonymous checkout is allowed before allowing to proceed and allow for checkout redirect if user logs in instead

- fixed a query error when resubmitting cc form

- fixed a bug where uc_cim could attempt to validate a cc number with masked values 

- fixed a cc form bug

- Display proper info message about selecting previously used cards or entering new ones when entering payment details

-----------------------
Misc

- Added some code comments with links to ubercart api pages and other info for ease of reading/editing/understanding code.

