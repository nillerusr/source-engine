//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef CLIENTENUMS_H
#define CLIENTENUMS_H
#ifdef _WIN32
#pragma once
#endif


enum ELogonState
{
	k_ELogonStateNotLoggedOn = 0,
	k_ELogonStateLoggingOn = 1,
	k_ELogonStateLoggingOff = 2,
	k_ELogonStateLoggedOn = 3
};


// Enums for all personal questions supported by the system.
enum EPersonalQuestion
{
	// Never ever change these after initial release.
	k_EPSMsgNameOfSchool = 0,			// Question: What is the name of your school?
	k_EPSMsgFavoriteTeam = 1,			// Question: What is your favorite team?
	k_EPSMsgMothersName = 2,			// Question: What is your mother's maiden name?
	k_EPSMsgNameOfPet = 3,				// Question: What is the name of your pet?
	k_EPSMsgChildhoodHero = 4,			// Question: Who was your childhood hero?
	k_EPSMsgCityBornIn = 5,				// Question: What city were you born in?

	k_EPSMaxPersonalQuestion
};


// account flags (stored in DB)
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASEd
enum EAccountFlags
{
	m_EAccountFlagNormalUser = 0,				// Standard user level (yes, this is meant to be zero)
	k_EAccountFlagPersonaNameSet = ( 1 << 0 ),	// true if the user has set the persona name they really want, instead of the auto-generated one
	k_EAccountFlagUnbannable	= ( 1 << 1 ),	// whatever happens, this account can't be banned
	k_EAccountFlagPasswordSet	= ( 1 << 2 ),	// we've set the password at least once on this account
	k_EAccountFlagSupport = ( 1 << 3 ),			// Enables use of web support tool
	k_EAccountFlagAdmin = ( 1 << 4 ),			// The name says it all, can do everything
	k_EAccountFlagSupervisor = ( 1 << 5 ),		// support supervisory role
	k_EAccountFlagAppEditor = ( 1 << 6 ),		// Can edit app info
	k_EAccountFlagHWIDSet = ( 1 << 7 ),			// Set HWID once
	k_EAccountFlagPersonalQASet = ( 1 << 8 ),	// user has personal Question & anser set
	k_EAccountFlagVacBeta = ( 1 << 9 ),			// user participates in VAC beta tests
	k_EAccountFlagDebug = ( 1 << 10 ),			// user is in debug mode, eg VAC doesn't kick etc
	k_EAccountFlagDisabled = ( 1 << 11 ),		// account is disabled.
	k_EAccountFlagLimitedUser = ( 1 << 12 ),	// account is limited user account because it doesnt own anything
	k_EAccountFlagLimitedUserForce = ( 1 << 13 ),	// account is limited user account because we forced it to be
	k_EAccountFlagEmailValidated = ( 1 << 14 ),		// user has verified email address via WG
	k_EAccountFlagMarketingTreatment = ( 1 << 15),		// account is flagged as being in a treatment for marketing/sales experiments
};

// profile state (stored in DB)
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ECommunityProfileState
{
	k_ECommunityProfileNotCreated = 0, // user hasn't setup community account yet
	k_ECommunityProfileActive = 1,	// user joined community, site is public
	k_ECommunityProfilePrivate = 2,	// user joined community, site is private
	k_ECommunityProfileLocked = 3,	// user got locked, content can't be changed but is still accessible
	k_ECommunityProfileDisabled = 4, // user got disabled, site not accessible anymore
};


// profile privacy option setting (stored in DB)
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ECommunityPrivacyState
{
	k_ECommunityPrivacyInvalid = 0,
	k_ECommunityPrivacyPrivate = 1,			// ain't nobody can see it
	k_ECommunityPrivacyFriendsOnly = 2,		// only your friends can see it
	k_ECommunityPrivacyPublic = 3,			// anybody could see it
};

enum ECommunityVisibilityState
{
	k_ECommunityVisibilityPrivate = 1,			// private, requester see only public fields
	k_ECommunityVisibilityFriendsOnly = 2,		// friends only, requester sees only public fields
	k_ECommunityVisibilityOpen = 3,				// it is visible to requester; they are owner or friend or public
	k_ECommunityVisibilitySupportPrivate = 4,	// was private, but it's a support account asking
	k_ECommunityVisibilitySupportFriendsOnly = 5,// was friends only, but it's a support account asking
};

enum ECommentPermission
{
	k_ECommentPermissionFriendsOnly = 0,		// only friends can leave a comment
	k_ECommentPermissionAnyone = 1,				// anybody can leave a comment
	k_ECommentPermissionSelfOnly = 2,			// only the account owner can leave a comment
};

// Payment methods for purchases - BIT FLAGS so can be used to indicate
// acceptable payment methods for packages
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum EPaymentMethod
{
	k_EPaymentMethodNone			= 0x000,		// user got the license for free
	k_EPaymentMethodActivationCode	= 0x001,		// user paid by entering unused CD-Key or other activation code
	k_EPaymentMethodCreditCard		= 0x002,		// user paid with credit card
	k_EPaymentMethodPayPal			= 0x004,		// user paid with via paypal
	k_EPaymentMethodGuestPass		= 0x008,		// user paid by redeeming a guest pass
	k_EPaymentMethodHardwarePromo	= 0x010,		// user presented machine credentials
	k_EPaymentMethodClickAndBuy		= 0x020,		// ClickandBuy
	k_EPaymentMethodAutoGrant		= 0x040,		// server side purchased package, things like German specific TF2 free weekend
	k_EPaymentMethodWallet			= 0x080,		// user paid with wallet
	k_EPaymentMethodOEMTicket		= 0x100,		// user paid by redeeming a OEM license ticket
	k_EPaymentMethodSplit			= 0x200,		// user paid with wallet AND a provider
};

// Sources for WalletTxn records
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum EWalletSource
{
	k_EWalletSourceInvalid = 0,
	k_EWalletSourcePurchase = 1,					// Created from a purchase, refund, chargeback, or reverse chargeback (PurchaseRefGID -> TransID)
	k_EWalletSourceGuestPass = 2,					// Created from a guest pass (PurchaseRefGID -> GuestPassID)
	k_EWalletSourceConversion = 3,					// Created from a wallet conversion (PurchaseRefGID -> GID shared between debit & credit records)
	k_EWalletSourceRebate = 4,						// Created from a rebate (PurchaseRefGID -> TransID)
};

// License types
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ELicenseType
{
	k_ENoLicense = 0,								// for shipped goods
	k_ESinglePurchase = 1,							// single purchase
	k_ESinglePurchaseLimitedUse = 2,				// single purchase w/ expiration
	k_ERecurringCharge = 3,							// recurring subscription
	k_ERecurringChargeLimitedUse = 4,				// recurring subscription w/ limited minutes per period
	k_ERecurringChargeLimitedUseWithOverages = 5,	// like above but w/ soft limit and overage charges
};

// Flags for licenses - BITS
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ELicenseFlags
{
	k_ELicenseFlagNone	= 0x00,				// just a place holder
	k_ELicenseFlagRenew = 0x01,				// Renew this license next period
	k_ELicenseFlagRenewalFailed = 0x02,		// Auto-renew failed
	k_ELicenseFlagPending = 0x04,			// Purchase or renewal is pending
	k_ELicenseFlagExpired = 0x08,			// Set if no longer active (whatever the reason)
	k_ELicenseFlagCancelledByUser = 0x10,	// Cancelled by the user
	k_ELicenseFlagCancelledByAdmin = 0x20,	// Cancelled by customer support
	k_ELicenseFlagLowViolenceContent = 0x40,// license is for low violence content
};

// Status of a package
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum EPackageStatus
{
	k_EPackageAvailable = 0,		// Available for purchase and use
	k_EPackagePreorder = 1,			// Available for purchase, as a pre-order
	k_EPackageUnavailable = 2,		// Not available for new purchases, may still be owned
	k_EPackageInvalid = 3,			// Either an unknown package or a deleted one that nobody should own
};

// Purchase status
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum EPurchaseStatus
{
	k_EPurchasePending = 0,			// purchase is pending, valid but pending subscription
	k_EPurchaseSucceeded = 1,		// purchase successful, valid subscription
	k_EPurchaseFailed = 2,			// purchase failed, no subscription
	k_EPurchaseRefunded = 3,		// we refunded the purchase and removed subscription
	k_EPurchaseInit = 4,			// user started purchase
	k_EPurchaseChargedback = 5,		// the user issued a chargeback, we removed subscription 
	k_EPurchaseRevoked = 6,			// we revoked the purchase and removed subscription. Usually stolen CD-Keys
	k_EPurchaseInDispute = 7,		// the purchase is being disputed by the user, preliminary to a chargeback 
};

// LineItemTypes
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASEd
enum ELineItemType
{
	k_ELineItemTypeInvalid = 0,					// Unknown - load all purchase line items
	k_ELineItemTypeMicroTxn = ( 1 << 0 ),		// Transaction has data in MicroTxnLineItem table
	k_ELineItemTypeWallet = ( 1 << 1 ),			// Transaction has data in WalletLineItem table
	k_ELineItemTypePkg = ( 1 << 2 ),			// Transaction has data in PurchaseLineItem table
};

// Enum for the types of news push items you can get
enum ENewsUpdateType
{
	k_EAppNews = 0,	 // news about a particular app
	k_ESteamAds = 1, // Marketing messages
	k_ESteamNews = 2, // EJ's corner and the like
	k_ECDDBUpdate = 3, // backend has a new CDDB for you to load
	k_EClientUpdate = 4,	// new version of the steam client is available
};

// Detailed purchase result codes for the client
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum EPurchaseResultDetail 
{
	k_EPurchaseResultNoDetail = 0,
	k_EPurchaseResultAVSFailure = 1,
	k_EPurchaseResultInsufficientFunds = 2,
	k_EPurchaseResultContactSupport = 3,
	k_EPurchaseResultTimeout = 4,

	k_EPurchaseResultInvalidPackage = 5,
	k_EPurchaseResultInvalidPaymentMethod = 6,
	k_EPurchaseResultInvalidData = 7,
	k_EPurchaseResultOthersInProgress = 8,
	k_EPurchaseResultAlreadyPurchased = 9,
	k_EPurchaseResultWrongPrice = 10,
	k_EPurchaseResultFraudCheckFailed = 11,
	k_EPurchaseResultCancelledByUser = 12,
	k_EPurchaseResultRestrictedCountry = 13,
	k_EPurchaseResultBadActivationCode = 14,		// this code gives no receipt
	k_EPurchaseResultDuplicateActivationCode = 15,

	k_EPurchaseResultUseOtherPaymentMethod = 16,	// User should try a different payment method
	k_EPurchaseResultUseOtherFundingSource = 17,	// Select a different funding source (paypal)
	k_EPurchaseResultInvalidShippingAddress = 18,	// Shipping address is invalid (paypal)
	k_EPurchaseResultRegionNotSupported = 19,		// This region is not supported with this payment type

	k_EPurchaseResultAcctIsBlocked = 20,			// Acct has been blocked by provider - user should contact provider to resolve
	k_EPurchaseResultAcctNotVerified = 21,			// Provider indicated account needs to be verified for transaction to complete

	k_EPurchaseResultInvalidAccount = 22,			// Provider indicated the account is invalid or no longer usable
	k_EPurchaseResultStoreBillingCountryMismatch = 23,	// store country code & billing country code do not match
	k_EPurchaseResultDoesNotOwnRequiredApp = 24,		// user does not own one of the apps required for purchase
	k_EPurchaseResultCanceledByNewTransaction = 25,		// user made a new transaction which canceled an old, pending transaction
	k_EPurchaseResultForceCanceledPending = 26,			// A pending transaction was force canceled (no response from provider)
	k_EPurchaseResultFailCurrencyTransProvider = 27,	// selected transaction provider does not support calculated currency
	k_EPurchaseResultFailedCyberCafe = 28,				// cybercafe account tried to purchase or use an activation code

	k_EPurchaseResultNeedsPreApproval = 29,				// Transaction needs approval from support
	k_EPurchaseResultPreApprovalDenied = 30,			// Transaction was denied by support
	k_EPurchaseResultWalletCurrencyMismatch = 31,		// Currency of purchase does not match currency of wallet
};

// Type of system IM.  The client can use this to do special UI handling in specific circumstances
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ESystemIMType
{
	k_ESystemIMRawText = 0,
	k_ESystemIMInvalidCard = 1,
	k_ESystemIMRecurringPurchaseFailed = 2,
	k_ESystemIMCardWillExpire = 3,
	k_ESystemIMSubscriptionExpired = 4,
	k_ESystemIMGuestPassReceived = 5, // User has received a guest pass from a friend
	k_ESystemIMGuestPassGranted = 6, // System has granted a user a guest pass to give out
	k_ESystemIMGiftRevoked = 7,			// We revoked a gift due to chargeback, etc

	//
	k_ESystemIMTypeMax
};

// Ways an external cd key can be munged onto a users PC
enum ELegacyKeyRegistrationMethod
{
	eLegacyKeyRegistrationMethodNone = 0, // doesn't support legacy cd keys
	eLegacyKeyRegistrationMethodRegistry, // just place it into the registry
	eLegacyKeyRegistrationMethodDisk, // put it in a file on disk
};

// Support events, generated by system or support input
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ESupportEvent
{
	// support activity
	eSupportNote = 0,			// a generic support note
	eSupportLogin = 1,			// support account logged in out, data: IP:Port
	eSupportLogoff = 2,			// support account logged out, text: IP:Port
	eSupportTicketCreated = 3,	// a support ticket was created
	eSupportTicketClosed = 4,	// problem was solved, ticket closed

	// account changes
	eSupportEnableAccount = 5,		// support enabled account, text: reason
	eSupportDisableAccount = 6,		// support disabled account, text: reason
	eSupportChangeAccountPassword = 7,	// support or user changed password, data: DONT include old password
	eSupportChangeAccountEmail = 8,		// support or user changed email, data: old email
	eSupportChangeAccountName = 9,		// support or user changed name, data: old name
	eSupportChangeAccountPlayer = 10,	// support or user changed player name, data: old name

	eSupportPurchaseChargedback = 11,	// a charge back was issued
	eSupportPurchaseRefunded = 12,		// a refund was issued
	eSupportPurchaseForcedCompletion = 13, // support forced a pending purchase to complete

	// license handling
	eSupportLicenseAdded = 14,		// support added a license, text: reason
	eSupportLicenseCanceled = 15,	// support or user cancel a license	
	eSupportLicenseChanged = 16,	// support removed a license, text: reason
	eSupportBannedGame = 17,		// support banned game for an account
	eSupportUnbannedGame = 18,		// support unbanned game for an account

	// purchase activity
	eSupportRunPurchase = 19,
	eSupportChangedCreditCard = 20,		// support updated a credit card 
	eSupportSetNoFraudCheckFlag = 21,	// support disabled fraud check, data: reason

	// banning
	eSupportBannedCreditCard = 22,	// support banned a credit card
	eSupportBannedIP = 23,			// support banned an IP
	eSupportBannedCDKey = 24,		// support banned an CDKey
	eSupportBannedCountry = 25,		// support banned a country
	eSupportBannedPayPalAccount = 26,

	eSupportPurchaseCanceled = 27,	// support forced a pending purchase to cancel

	eSupportChangeAvatar = 28,
	eSupportChangeProfileURL = 29,

	eSupportRegisterCDKey = 30,			// support added a CD key to this account
	eSupportGrantGuestPass = 31,		// support granted a guest pass to this account
	eSupportResubmitTransaction = 32,	// support resubmitted a transaction

	eSupportResetContent = 33,			// reset user content based on abuse reports
	eSupportLockProfile = 34,			// temp block a user from modifying community content
	eSupportSetCommunityState = 35,		// perm lock a user profile, can't be modified
	eSupportDeleteAbuseReports = 36,	// deleted abuse reports for the SteamID
	eSupportSetAccountFlags = 37,		// changed account flags

	eSupportChargebackStatusUpdate = 38,	// support updated the pending chargeback status

	eSupportRefundForcedCompletion = 39,	// support forced a pending refund to complete
	eSupportRefundCanceled = 40,			// support forced a pending refund to cancel

	eSupportRevokeActivationKey = 41,		// support revoked and unlocked activation key
	eSupportReverseChargeback = 42,			// a charge back was reversed
	eSupportRejectedActivationKey = 43,		// activation key was rejected (invalid or already used)

	eSupportPurchaseError = 44,				// Purchase error
	eSupportAudited			= 45,

	// items
	eSupportBanItems = 46,					// support banned a user's items
	eSupportRestoreBannedItems = 47,		// support restored banned items
	eSupportRestoreDeletedItems = 48,		// support restored deleted items

	// comments
	eSupportDeleteComments = 49,			// cleared comments on this account (not written by this account)

	eSupportDeleteItems = 50,				// support deleted a user's items
	eSupportDeleteCachedCard = 51,			// support deleted a user's cached credit card

	eSupportConvertedWallet = 52,			// user's wallet was converted

	eSupportTxnApproved = 53,				// PreApproval granted
	eSupportTxnDenied = 54,					// PreApproval denied

	eSupportGCAction = 55,					// used for all support actions from the GC
};


// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ESupportTicket
{
	// all kinds of problems tickets that have to be handled by support
	k_ETicketUnknown = 0,		// an unknown problem. yay.
	k_ETicketManual = 1,		// a problem manually entered by support. 
	k_ETicketFraudRedFlag = 2,	// fraud detection marked this account 
	k_ETicketPurchaseError = 3,	// a purchase error happened 
	k_ETicketChargeback = 4,	// a chargeback needs attention 
};

// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ESupportTicketState
{
	// all kinds of problems tickets that have to be handled by support
	k_ETicketStateUnknown = 0,		// support issue state is unknown
	k_ETicketStateUnassigned = 1,	// support issue is 'open' but not assigned yet
	k_ETicketStateInProcess = 2,	// support issue is assigned to an support actor
	k_ETicketStateResolved = 3,		// support issue is fixed and closed 
	k_ETicketStateUnresolved = 4,	// support issue couldn't be fixed. closed anyway
	k_ETicketStateAutoClosed = 5,	// System closed the ticket automatically
};


//-----------------------------------------------------------------------------
// types of content that can be reported as abused
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
//-----------------------------------------------------------------------------
enum ECommunityContentType
{
	k_EContentUnspecified = 0,
	k_EContentAll = 1,			// reset all community content
	k_EContentAvatarImage = 2,  // clear avatar image
	k_EContentProfileText = 3,  // reset profile text
	k_EContentWebLinks = 4,		// delete web links
	k_EContentAnnouncement = 5,
	k_EContentEventText = 6,
	k_EContentCustomCSS = 7,
	k_EContentProfileURL = 8,	// delete community URL ID
	k_EContentComments = 9,		// just comments this guy has written
};


//-----------------------------------------------------------------------------
// types of reasons why a violation report was issued
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
//-----------------------------------------------------------------------------
enum EAbuseReportType
{
	k_EAbuseUnspecified = 0,
	k_EAbuseInappropriate = 1,	// just not ok to post
	k_EAbuseProhibited = 2,		// prohibited by EULA or general law
	k_EAbuseSpamming = 3,		// excessive spamming
	k_EAbuseAdvertisement = 4,	// unwanted advertisement
	k_EAbuseExploit = 5,		// content data attempts to exploit code issue
	k_EAbuseSpoofing = 6,		// user/group is impersonating an official contact
	k_EAbuseLanguage = 7,		// bad language
	k_EAbuseAdultContent = 8,	// any kind of adult material, references etc
	k_EAbuseHarassment = 9,		// harassment, discrimination, racism etc
};

//-----------------------------------------------------------------------------
// actions for a user within a clan for logging in the ClanHistory table
//-----------------------------------------------------------------------------

enum EClanAction
{
	k_EJoined = 1,					// joined the clan
	k_ELeft = 2,					// left the clan
	k_EPromoted = 3,				// promoted to officer	
	k_EDemoted = 4,					// demoted from officer
	k_EKicked = 5,					// kicked off the clan
	k_ECreated = 6,					// clan was created
	k_EInvited = 7,					// invited someone
	k_EEventCreated = 8,			// clan event created
	k_EEventUpdated = 9,			// clan event updated
	k_EEventDeleted = 10,			// clan event deleted
	k_EPermissionsChanged = 11,		// clan permissions were changed
	k_EAnnouncementCreated = 12,	// clan announcement created
	k_EAnnouncementUpdated = 13,	// clan announcement updated
	k_EAnnouncementDeleted = 14,	// clan announcement deleted
	k_EPOTWChanged = 15,			// changed the POTW
	k_ELinksChanged = 16,			// links changed
	k_EDetailsChanged = 17,			// details changed
	k_ESupportResetContent = 18,	// support reset some or all of the clan content
	k_ESupportLockedGroup = 19,		// support locked this clan, it can't be modified anymore
	k_ESupportUnlockedGroup = 20,	// support unlocked this clan
	k_ESupportChangedOwner = 21,	// support transfered ownership
	k_EMadePublic = 22,				// made from private into public
	k_EMadePrivate = 23,			// made from public into private
	k_ESupportDisabledGroup = 24,	// support disabled this group, nobody can see it anymore
	k_EKickedChat = 25,				// kicked from chat
	k_EBannedChat = 26,				// banned from chat
	k_EUnBannedChat = 27,			// un-banned from chat

	k_EHighestValidAction			// keep me updated, please!
};


//-----------------------------------------------------------------------------
// types of events for use in the Clan Event Type table
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
//-----------------------------------------------------------------------------
enum EClanEventType
{
	k_EOtherEvent = 1,
	k_EGameEvent = 2,
	k_EPartyEvent = 3,
	k_EMeetingEvent = 4,
	k_ESpecialCauseEvent = 5,
	k_EMusicAndArtsEvent = 6,
	k_ESportsEvent = 7,
	k_ETripEvent = 8,
	k_EChatEvent = 9,
	k_EGameReleaseEvent = 10,
};

//-----------------------------------------------------------------------------
// types of marketing messages displayed to users
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
//-----------------------------------------------------------------------------
enum EMarketingMessageType
{
	k_EMarketingMessageNowAvailable = 1,
	k_EMarketingMessageWeekendDeal = 2,
	k_EMarketingMessagePrePurchase = 3,
	k_EMarketingMessagePlayNow = 4,
	k_EMarketingMessagePreloadNow = 5,
	k_EMarketingMessageGeneral = 6,
	k_EMarketingMessageDemoQuit = 7,
	k_EMarketingMessageGifting = 8,
	k_EMarketingMessageEJsKorner = 9,
};

//-----------------------------------------------------------------------------
// types of associations a marketing message may have
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
//-----------------------------------------------------------------------------
enum EMarketingMessageAssociationType
{
	k_EMarketingMessageNoAssociation = 0,
	k_EMarketingMessageAppAssociation = 1,
	k_EMarketingMessageSubscriptionAssociation = 2,
	k_EMarketingMessagePublisherAssociation = 3,
	k_EMarketingMessageGenreAssociation = 4,
};

//-----------------------------------------------------------------------------
// Marketing message visibility
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
//-----------------------------------------------------------------------------
enum EMarketingMessageVisibility
{
	k_EMarketingMessageVisibleBeta = 1,
	k_EMarketingMessageVisiblePublic = 2,
};

//-----------------------------------------------------------------------------
// Structures used in multiple messages
//-----------------------------------------------------------------------------
// Purchase message constants
// WARNING: Do not change these if an instance of this record may exist in a database!!!
// BUGBUG derrick - Since these also define schema, they should be moved into steamschema.h
const int k_cchCCNumMax			= 16 + 1;
const int k_cchHolderNameMax	= 100 + 1;
const int k_cchExpYearMax		= 4 + 1;
const int k_cchExpMonthMax		= 2 + 1;
const int k_cchCVV2Max			= 4 + 1;
const int k_cchAddressMax		= 128 + 1;
const int k_cchAddress2Max		= k_cchAddressMax;
const int k_cchCityMax			= 50 + 1;
const int k_cchPostcodeMax		= 16 + 1;
const int k_cchStateMax			= 32 + 1;
const int k_cchPhoneMax			= 20 + 1;
const int k_cchEmailMax			= 100 + 1;
const int k_cchCountryCodeMax	= 2 + 1;
const int k_cchPayPalCheckoutTokenMax = 20 + 1;
const int k_cchStateCodeMax		= 3 + 1;
const int k_cchCurrencyCodeMax	= 3 + 1;

const int k_cubMaxDfsURL = 128;				// Max size for URL descriptors on DFS


// License information
struct LicenseInfo_t
{
	PackageId_t m_unPackageID;
	RTime32 m_RTime32Created;
	RTime32 m_RTime32NextProcess;
	int32 m_nMinuteLimit;
	int32 m_nMinutesUsed;
	EPaymentMethod m_ePaymentMethod;
	uint32 m_nFlags;
	char m_rgchPurchaseCountryCode[k_cchCountryCodeMax];
	int32 m_nTerritoryCode;
};

// Supported Currency Codes
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ECurrencyCode
{
	k_ECurrencyCodeInvalid = 0,
	k_ECurrencyCodeUSD = 1,
	k_ECurrencyCodeGBP = 2,
	k_ECurrencyCodeEUR = 3,

	k_ECurrencyCodeMax = 4
};

enum ETaxType
{
	k_ETaxTypeInvalid = 0,
	k_ETaxTypeUSState = 1,
	k_ETaxTypeVAT = 2
};

// client stat list
// needs to be kept in the same order, since it's part of the protocol
enum EClientStat
{
	k_EClientStatP2PConnectionsUDP = 0,
	k_EClientStatP2PConnectionsRelay = 1,
	k_EClientStatP2PGameConnections = 2,
	k_EClientStatP2PVoiceConnections = 3,
	
	k_EClientStatBytesDownloaded = 4,

	k_EClientStatMax,	// must be last, used as array's of data
};

enum EP2PState
{
	k_EP2PStateNotConnected,
	k_EP2PStateUDP,
	k_EP2PStateRelay,
};

// User response for authentication request
enum EMicroTxnAuthResponse
{
	k_EMicroTxnAuthResponseInvalid = 0,		// Invalid value
	k_EMicroTxnAuthResponseAuthorize = 1,	// user accepted microtransaction
	k_EMicroTxnAuthResponseDeny = 2,		// user denied microtransaction
	k_EMicroTxnAuthResponseAutoDeny = 3,	// client automatically denied microtransaction (user wasn't in game, etc.)
};

// Result of authorization request, returned to client
enum EMicroTxnAuthResult
{
	k_EMicroTxnAuthResultInvalid = 0,				// Invalid value
	k_EMicroTxnAuthResultOK = 1,					// Successfully authorized
	k_EMicroTxnAuthResultFail = 2,					// An error occurred
	k_EMicroTxnAuthResultInsufficientFunds = 3,		// User has insufficient funds to complete microtransaction
};

#endif
