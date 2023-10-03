//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VAttractScreen.h"
#include "VSignInDialog.h"
#include "EngineInterface.h"
#include "inputsystem/iinputsystem.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/imagepanel.h"
#include "VGenericConfirmation.h"
#include "VFooterPanel.h"
#include "vgui/ISurface.h"
#include "gameui_util.h"
#include "tier0/icommandline.h"
#ifdef _X360
#include "xbox/xbox_launch.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern IVEngineClient *engine;

//
//	Primary user id is the one who attract screen uses for
//	signing in. All other systems will see no primary user
//	until attract screen transitions into the main menu.
//
static int s_idPrimaryUser = -1;
static int s_idSecondaryUser = -1;
static int s_bSecondaryUserIsGuest = 0;
static int s_eStorageUI = 0;

static int s_iAttractModeRequestCtrlr = -1;
static int s_iAttractModeRequestPriCtrlr = -1;
static CAttractScreen::AttractMode_t s_eAttractMode = CAttractScreen::ATTRACT_GAMESTART;
void CAttractScreen::SetAttractMode( AttractMode_t eMode, int iCtrlr )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::SetAttractMode(%d)\n", eMode );
	}

	s_eAttractMode = eMode;
	
#ifdef _X360
	if ( !XBX_GetNumGameUsers() )
		return;

	if ( iCtrlr >= 0 )
	{
		s_iAttractModeRequestCtrlr = iCtrlr;
		s_iAttractModeRequestPriCtrlr = XBX_GetPrimaryUserId();
		Msg( "[GAMEUI] CAttractScreen::SetAttractMode(%d) bookmarked ctrlr%d (primary %d)\n", eMode, s_iAttractModeRequestCtrlr, s_iAttractModeRequestPriCtrlr );
	}
#endif
}

bool IsUserSignedInProperly( int iCtrlr )
{
#ifdef _X360
	XUSER_SIGNIN_INFO xsi;
	if ( iCtrlr >= 0 && iCtrlr < XUSER_MAX_COUNT &&
		XUserGetSigninState( iCtrlr ) != eXUserSigninState_NotSignedIn &&
		ERROR_SUCCESS == XUserGetSigninInfo( iCtrlr, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
		!(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) )
		return true;
	else
		return false;
#else
	return true;
#endif
}

bool IsUserSignedInToLiveWithMultiplayer( int iCtrlr )
{
#ifdef _X360
	BOOL bPriv = FALSE;
	return IsUserSignedInProperly( iCtrlr ) &&
		( eXUserSigninState_SignedInToLive == XUserGetSigninState( iCtrlr ) ) &&
		( ERROR_SUCCESS == XUserCheckPrivilege( iCtrlr, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPriv ) ) &&
		bPriv;
#else
	return true;
#endif
}

static bool IsPrimaryUserSignedInProperly()
{
	return IsUserSignedInProperly( s_idPrimaryUser );
}

static void ChangeGamers();


// safe to reset timeout to TCR 003: 120 seconds
ConVar sys_attract_mode_timeout( "sys_attract_mode_timeout", "120", FCVAR_DEVELOPMENTONLY );

#if defined( _X360 )
CON_COMMAND_F( ui_force_attract, "", FCVAR_DEVELOPMENTONLY )
{
	// for development only testing, force the attract mode to beign its timeout
	CAttractScreen* pAttractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) );
	if ( pAttractScreen )
	{
		// enable it, regardles of initial setup
		pAttractScreen->ResetAttractDemoTimeout();
	}
}
#endif

bool CAttractScreen::IsUserIdleForAttractMode()
{
	if ( m_bHidePressStart )
		// attract screen is in ghost mode
		return false;

	if ( m_pPressStartlbl && !m_pPressStartlbl->IsVisible() )
		// something is going on and "Press START" is not flashing
		return false;

	if ( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) )
		// confirmation is up
		return false;

	if ( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
		// progress spinner is up
		return false;

	if ( CUIGameData::Get()->IsXUIOpen() )
		// X360 blade UI is open
		return false;

	// All good, can kick off idle countdown
	return true;
}



//////////////////////////////////////////////////////////////////////////
//
// Device selector implementation
//
//////////////////////////////////////////////////////////////////////////

class CAttractScreenDeviceSelector : public ISelectStorageDeviceClient
{
public:
	CAttractScreenDeviceSelector( int iCtrlr, bool bForce, bool bAllowDeclined ) :
	  m_iCtrlr( iCtrlr ), m_bForce( bForce ), m_bAllowDeclined( bAllowDeclined ) {}
public:
	virtual int  GetCtrlrIndex() { return m_iCtrlr; }
	virtual bool ForceSelector() { return m_bForce; }
	virtual bool AllowDeclined() { return m_bAllowDeclined; }
	virtual bool AllowAnyController() { return false; }

	virtual void OnDeviceFail( FailReason_t eReason );

	virtual void OnDeviceSelected();		// After device has been picked in XUI blade, but before mounting symbolic roots and opening containers
	virtual void AfterDeviceMounted();		// After device has been successfully mounted, configs processed, etc.

public:
	int m_iCtrlr;
	bool m_bForce;
	bool m_bAllowDeclined;
};

void CAttractScreenDeviceSelector::OnDeviceFail( FailReason_t eReason )
{
	XBX_SetStorageDeviceId( GetCtrlrIndex(), XBX_STORAGE_DECLINED );

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );
	
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->ReportDeviceFail( eReason );
	}
	else
	{
		ChangeGamers();
	}
	delete this;
}

void CAttractScreenDeviceSelector::OnDeviceSelected()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StartGame_Storage_Selected - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	CUIGameData::Get()->UpdateWaitPanel( "#L4D360UI_WaitScreen_SignOnSucceded" );
}

void CAttractScreenDeviceSelector::AfterDeviceMounted()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StartGame_Storage_Loaded - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );

	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		switch ( s_eStorageUI )
		{
		case 1:
			attractScreen->StartGame_Stage2_Storage2();
			return;
		case 2:
			attractScreen->StartGame_Stage3_Ready();
			return;
		default:
			ChangeGamers();
			return;
		}
	}
	else
	{
		ChangeGamers();
	}
	delete this;
}

//////////////////////////////////////////////////////////////////////////
//
// Attract screen implementation
//
//////////////////////////////////////////////////////////////////////////

CAttractScreen::CAttractScreen( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true )
{
	SetProportional( true );
	SetPaintBackgroundEnabled( false );

	SetDeleteSelfOnClose( true );

	m_pPressStartlbl = NULL;
	m_pPressStartShadowlbl = NULL;
	m_bHidePressStart = false;
	m_msgData = 0;
	m_pfnMsgChanged = NULL;
	m_bladeStatus = BLADE_NOTWAITING;
	m_flFadeStart = 0;

	m_eSignInUI = SIGNIN_NONE;
	m_eStorageUI = STORAGE_NONE;

	// -1 means reset timer when viable
	// 0 means NEVER timeout
	// >0 means enabled, is the high water mark time
	m_AttractDemoTimeout = -1;
#if defined( _X360 ) && defined( _DEMO )
	if ( engine->IsDemoHostedFromShell() )
	{
		// there is already a master demo timeout that overrides
		// prevent any attract mode timeout
		m_AttractDemoTimeout = 0;
	}
#endif

	if ( IsX360() && 
		( Sys_IsDebuggerPresent() || 
			CommandLine()->FindParm( "-nostartupmenu" ) || 
			CommandLine()->FindParm( "-noattract" ) || 
			CommandLine()->FindParm( "-dev" ) ) )
	{
		// in development, prevent attract mode
		m_AttractDemoTimeout = 0;
	}

	// Subscribe for the events
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

CAttractScreen::~CAttractScreen()
{
	delete m_pPressStartShadowlbl;

	// Unsubscribe for the events
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

void CAttractScreen::ResetAttractDemoTimeout()
{
	// explicity reset
	m_AttractDemoTimeout = -1;
}

void CAttractScreen::OnThink()
{
	// If the required message changed, call the requested callback
	// this is required because generic confirmations drive the signon
	// state machine and if another confirmation interferes the signon
	// machine can be aborted and leave UI in undetermined state
	if ( m_pfnMsgChanged )
	{
		GenericConfirmation* pMsg = ( GenericConfirmation* ) CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION );
		if ( !pMsg || pMsg->GetUsageId() != m_msgData )
		{
			void (*pfn)() = m_pfnMsgChanged;
			m_pfnMsgChanged = NULL;

			(*pfn)();
		}
	}

	// image and labels fade in 
	float flFade = 1.0f;
	if ( m_flFadeStart )
	{
		flFade = RemapValClamped( Plat_FloatTime(), m_flFadeStart, m_flFadeStart + TRANSITION_TO_MOVIE_FADE_TIME, 0.0f, 1.0f );
		if ( flFade >= 1.0f )
		{
			// done
			m_flFadeStart = 0;
		}
	}

	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetAlpha( flFade * 255.0f );
	}

	if ( m_pPressStartShadowlbl )
	{
		int alpha = flFade * ( 80.0f + 50.0f * sin( Plat_FloatTime() * 4.0f ) );
		m_pPressStartShadowlbl->SetAlpha( alpha );
	}

	BladeStatus_t bladeStatus = GetBladeStatus();
	if ( bladeStatus != BLADE_NOTWAITING && m_AttractDemoTimeout )
	{
		// there is blade activity
		// reset the attract timeout
		m_AttractDemoTimeout = -1;
	}		

	switch ( bladeStatus )
	{
	case BLADE_WAITINGFOROPEN:
		{
			HidePressStart();
			if( CUIGameData::Get()->IsXUIOpen() )
			{
				SetBladeStatus( BLADE_WAITINGFORCLOSE );				
			}
		}
		break;

	case BLADE_WAITINGFORCLOSE:
		{
			HidePressStart();
			if( !CUIGameData::Get()->IsXUIOpen() )
			{
				m_flWaitStart = Plat_FloatTime();
				SetBladeStatus( BLADE_WAITINGFORSIGNIN );
			}
		}
		break;

	case BLADE_WAITINGFORSIGNIN:
		{
			if ( Plat_FloatTime() - m_flWaitStart > 0.1f )
			{
				SetBladeStatus( BLADE_NOTWAITING );

				if ( m_eSignInUI == SIGNIN_PROMOTETOGUEST )
				{
					// The user took down the blade without promoting an account to guest
					// start the game with the previously selected gamers if both of
					// them are properly signed in
					if ( IsUserSignedInProperly( s_idPrimaryUser ) && IsUserSignedInProperly( s_idSecondaryUser ) )
						StartGame( s_idPrimaryUser, s_idSecondaryUser );
					else
						SetBladeStatus( BLADE_WAITINGTOSHOWSIGNIN2 );
				}
				else
				{
					ShowPressStart();
				}

				m_eSignInUI = SIGNIN_NONE;
			}
		}
		break;

	case BLADE_NOTWAITING: //not waiting
		if ( CUIGameData::Get()->IsXUIOpen() )
		{
			// Check if our confirmation is up
			if ( GenericConfirmation* pMsg = ( GenericConfirmation* ) CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) )
			{
				if ( pMsg->GetUsageId() == m_msgData )
				{
					m_flWaitStart = Plat_FloatTime();
					SetBladeStatus( BLADE_TAKEDOWNCONFIRMATION );
				}
			}
		}
		else if ( s_eAttractMode == ATTRACT_GAMESTART )
		{
			if ( IsX360() )
			{
				if ( !IsUserIdleForAttractMode() && m_AttractDemoTimeout )
				{
					m_AttractDemoTimeout = -1;
				}

				// we are at "press start" and there is no system xui open
				if ( m_AttractDemoTimeout  < 0 )
				{
					// safe to reset timeout to TCR 003: 120 seconds
					m_AttractDemoTimeout = Plat_FloatTime() + sys_attract_mode_timeout.GetFloat();
				}
				else if ( m_AttractDemoTimeout > 0 && Plat_FloatTime() > m_AttractDemoTimeout )
				{
					// timeout expired
					// start the exiting sequence, which cannot be stopped
					m_AttractDemoTimeout = 0;
					CBaseModPanel::GetSingleton().StartExitingProcess( false );
				}
			}
		}

		if ( s_eAttractMode == ATTRACT_GOSPLITSCREEN )
		{
			s_idPrimaryUser = s_iAttractModeRequestPriCtrlr;
			s_idSecondaryUser = s_iAttractModeRequestCtrlr;
			s_iAttractModeRequestCtrlr = -1;
			SetBladeStatus( BLADE_WAITINGTOSHOWSIGNIN2 );
		}
		if ( s_eAttractMode == ATTRACT_GOSINGLESCREEN )
		{
			SetAttractMode( ATTRACT_GAMESTART );
			ShowSignInDialog( s_iAttractModeRequestCtrlr, -1 );
			s_iAttractModeRequestCtrlr = -1;
		}
		if ( s_eAttractMode == ATTRACT_GUESTSIGNIN )
		{
			SetAttractMode( ATTRACT_GAMESTART );
			ShowSignInDialog( s_iAttractModeRequestCtrlr, -1, SIGNIN_SINGLE );
			s_iAttractModeRequestCtrlr = -1;
		}
		if ( s_eAttractMode == ATTRACT_ACCEPTINVITE )
		{
			SetAttractMode( ATTRACT_GAMESTART );
		}
		break;

	case BLADE_TAKEDOWNCONFIRMATION:
		if ( CUIGameData::Get()->IsXUIOpen() )
		{
			// Check if our confirmation is up
			bool bShouldWaitLonger = false;
			if ( GenericConfirmation* pMsg = ( GenericConfirmation* ) CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) )
			{
				if ( pMsg->GetUsageId() == m_msgData )
				{
					if ( Plat_FloatTime() - m_flWaitStart > 1.0f )
					{
						if ( UI_IsDebug() )
						{
							Msg( "[GAMEUI] TakeDownConfirmation( %s )\n", pMsg->GetName() );
						}
						
						ShowPressStart();
						pMsg->Close();
					}
					else
					{
						bShouldWaitLonger = true;
					}
				}
			}

			if ( !bShouldWaitLonger )
			{
				SetBladeStatus( BLADE_NOTWAITING );
			}
		}
		else
		{
			SetBladeStatus( BLADE_NOTWAITING );
		}
		break;

	case BLADE_WAITINGTOSHOWPROMOTEUI:
#ifdef _X360
		if ( !CUIGameData::Get()->IsXUIOpen() )
		{
			SetBladeStatus( BLADE_WAITINGFOROPEN );
			m_eSignInUI = SIGNIN_PROMOTETOGUEST;
			XShowSigninUI( 1, XSSUI_FLAGS_CONVERTOFFLINETOGUEST );
		}
#endif
		break;

	case BLADE_WAITINGTOSHOWSTORAGESELECTUI:
#ifdef _X360
		if ( !CUIGameData::Get()->IsXUIOpen() )
		{
			SetBladeStatus( BLADE_NOTWAITING );

			int iSlot = m_eStorageUI - 1;
			int iContorller = XBX_GetUserId( iSlot );
			DWORD dwDevice = XBX_GetStorageDeviceId( iContorller );

			CUIGameData::Get()->SelectStorageDevice( new CAttractScreenDeviceSelector(
				iContorller,
				( dwDevice == XBX_INVALID_STORAGE_ID ) ? true : false, // force the XUI to display
				true ) );
		}
#endif
		break;

	case BLADE_WAITINGTOSHOWSIGNIN2:
		if ( !CUIGameData::Get()->IsXUIOpen() )
		{
			SetAttractMode( ATTRACT_GAMESTART );
			SetBladeStatus( BLADE_NOTWAITING );
			ShowSignInDialog( s_idPrimaryUser, s_idSecondaryUser, SIGNIN_DOUBLE );
		}
		break;

	default:
		break;
	}

	BaseClass::OnThink();
}

void CAttractScreen::OnKeyCodePressed( KeyCode code )
{
	// any key activity anywhere resets the attract timeout
	if ( m_AttractDemoTimeout )
	{
		m_AttractDemoTimeout = -1;
	}

	if ( m_bHidePressStart )
		// Cannot process button presses if PRESS START is not available
		return;

	int userId = GetJoystickForCode( code );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_XBUTTON_START:
		if ( s_eAttractMode == ATTRACT_GAMESTART )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
			ShowSignInDialog( userId, -1 );
		}	
		break;
	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

bool CAttractScreen::BypassAttractScreen()
{
	// Attract screen can only be bypassed once
	static bool s_bBypassOnce = false;
	if ( s_bBypassOnce )
		return false;
	s_bBypassOnce = true;

	// Check if command line allows to bypass
	if ( !CommandLine()->FindParm( "-noattractscreen" ) )
		return false;

#ifdef _X360
	// Check if there's a user signed in properly
	for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
	{
		if ( !IsUserSignedInProperly( k ) )
			continue;

		m_bHidePressStart = true;
		HidePressStart();

		XBX_SetStorageDeviceId( k, XBX_STORAGE_DECLINED );

		StartGame( k );
		return true;
	}
#endif

	// Couldn't find a user to be signed in
	return false;
}

void CAttractScreen::AcceptInvite()
{
#ifdef _X360
	static bool s_bAcceptedOnce = false;
	if ( s_bAcceptedOnce )
		return;
	s_bAcceptedOnce = true;

	int iInvitedUser = XBX_GetInvitedUserId();
	XUID iInvitedXuid = XBX_GetInvitedUserXuid();
	XNKID nSessionID = XBX_GetInviteSessionId();

	Msg( "[GAMEUI] Invite for user %d (%llx) session id: %llx\n", iInvitedUser, iInvitedXuid, ( uint64 const & ) nSessionID );

	if ( !( ( const uint64 & )( nSessionID ) ) ||
		( iInvitedUser < 0 || iInvitedUser >= XUSER_MAX_COUNT ) ||
		!iInvitedXuid )
	{
		return;
	}

	if ( !IsUserSignedInProperly( iInvitedUser ) )
	{
		Warning( "[GAMEUI] User no longer signed in\n" );
		return;
	}

	XUSER_SIGNIN_INFO xsi;
	if ( ERROR_SUCCESS != XUserGetSigninInfo( iInvitedUser, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) ||
		xsi.xuid != iInvitedXuid )
	{
		Warning( "[GAMEUI] Failed to match user xuid with invite\n" );
		return;
	}

	//
	// Proceed accepting the invite
	//
	m_bHidePressStart = true;

	// Need to fire off a game state change event to
	// all listeners
	XBX_SetInvitedUserId( iInvitedUser );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", (int) XBX_GetNumGameUsers() ) );

	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		" system { "
			" network LIVE "
		" } "
		" options { "
			" action joininvitesession "
		" } "
		);
	pSettings->SetUint64( "options/sessionid", * ( uint64 * ) &nSessionID );
	KeyValues::AutoDelete autodelete( pSettings );

	Msg( "[GAMEUI] CAttractScreen - invite restart, invite accepted!\n" );
	g_pMatchFramework->MatchSession( pSettings );
#endif
}

void CAttractScreen::OnOpen()
{
	BaseClass::OnOpen();

#ifdef _X360
	if ( BypassAttractScreen() )
		return;

	AcceptInvite();
#endif

	if ( s_eAttractMode == ATTRACT_GAMESTART &&	!m_bHidePressStart )
	{
		ShowPressStart();
	}
	else if ( s_eAttractMode == ATTRACT_GOSINGLESCREEN ||
		s_eAttractMode == ATTRACT_GOSPLITSCREEN ||
		s_eAttractMode == ATTRACT_GUESTSIGNIN )
	{
		ShowPressStart();
		m_bHidePressStart = true;
		HidePressStart();
	}
	else
	{
		m_bHidePressStart = true;
		HidePressStart();
	}

	if ( s_eAttractMode == ATTRACT_ACCEPTINVITE )
	{
		CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_WaitScreen_JoiningParty" );
	}
}

void CAttractScreen::OnClose()
{
	// SetAttractMode( ATTRACT_GAMESTART );	// reset attract mode, not relying on OnThink
	// cannot reset attract mode due to some crazy flow when attract screen is created
	// and closed 4 times while joining an invite from PRESS START screen!
	// now when attract screen needs to pop up calling code will set ATTRACT_GAMESTART mode.
	BaseClass::OnClose();
}

void CAttractScreen::OnEvent( KeyValues *pEvent )
{
#ifdef _X360
	if ( m_AttractDemoTimeout &&
		!Q_stricmp( "OnSysXUIEvent", pEvent->GetName() ) )
	{
		// XUI blade activity resets the attract demo timeout
		m_AttractDemoTimeout = -1;
	}
	else if ( m_eSignInUI == SIGNIN_NONE &&
		!Q_stricmp( "OnSysSigninChange", pEvent->GetName() ) &&
		!Q_stricmp( "signout", pEvent->GetString( "action", "" ) ) )
	{
		// Make sure that one of the committed controllers is not signing out
		int nMask = pEvent->GetInt( "mask", 0 );
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			int iController = XBX_GetUserId( k );
			bool bSignedOut = !!( nMask & ( 1 << iController ) );
			
			if ( bSignedOut || XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
			{
				if ( UI_IsDebug() )
				{
					Msg( "[GAMEUI] CAttractScreen::OnEvent(OnSysSigninChange) - ctrlr %d signed out!\n", iController );
				}

				// Ooops, a committed user signed out while we were processing user settings and stuff...
				ShowPressStart();
				HideProgress();
				HideMsgs();
				return;
			}
		}
	}

	// We aren't expecting any more notifications if we aren't waiting for the blade
	if ( BLADE_NOTWAITING == GetBladeStatus() )
		return;

	if ( !Q_stricmp( "OnSysSigninChange", pEvent->GetName() ) &&
		 !Q_stricmp( "signin", pEvent->GetString( "action", "" ) ) )
	{
		int numUsers = pEvent->GetInt( "numUsers", 0 );
		int nMask = pEvent->GetInt( "mask", 0 );

		if ( UI_IsDebug() )
		{
			Msg( "[GAMEUI] AttractScreen::signin %d ctrlrs (0x%x), state=%d\n", numUsers, nMask, m_eSignInUI );
		}

		if ( numUsers > 0 )
		{
			for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
			{
				if ( ( nMask & ( 1 << k ) ) &&
					 XUserGetSigninState( k ) == eXUserSigninState_NotSignedIn )
				{
					Msg( "[GAMEUI] AttractScreen::SYSTEMNOTIFY_USER_SIGNEDIN is actually a sign out, discarded!\n" );
					return;
				}
			}

			switch ( m_eSignInUI )
			{
			case SIGNIN_SINGLE:
				{
					// We expected a profile to sign in
					m_eSignInUI = SIGNIN_NONE;
					SetBladeStatus( BLADE_NOTWAITING );
					StartGame( pEvent->GetInt( "user0", -1 ) );
				}
				break;

			case SIGNIN_DOUBLE:
				{
					m_eSignInUI = SIGNIN_NONE;
					SetBladeStatus( BLADE_NOTWAITING );

					// We expected two profiles to sign in
					if ( numUsers == 2 &&
						 ( IsUserSignedInProperly( pEvent->GetInt( "user0", -1 ) ) ||
						   IsUserSignedInProperly( pEvent->GetInt( "user1", -1 ) ) ) )
					{
						XBX_SetUserIsGuest( 0, 0 );
						XBX_SetUserIsGuest( 1, 0 );

						s_idPrimaryUser = pEvent->GetInt( "user0", -1 );
						s_idSecondaryUser = pEvent->GetInt( "user1", -1 );

						if ( !OfferPromoteToLiveGuest() )
						{
							StartGame( s_idPrimaryUser, s_idSecondaryUser );
						}
					}
					else
					{
						// We need to reset all controller state to be
						// able to dismiss the message box
						ShowPressStart();

						GenericConfirmation* confirmation = 
							static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

						GenericConfirmation::Data_t data;

						data.pWindowTitle = "#L4D360UI_MsgBx_NeedTwoProfilesC";
						data.pMessageText = "#L4D360UI_MsgBx_NeedTwoProfilesTxt";
						data.bOkButtonEnabled = true;

						m_msgData = confirmation->SetUsageData(data);

						HideFooter();
					}
				}
				break;

			case SIGNIN_PROMOTETOGUEST:
				{
					m_eSignInUI = SIGNIN_NONE;
					SetBladeStatus( BLADE_NOTWAITING );

					if ( UI_IsDebug() )
					{
						Msg( "[GAMEUI] Promoted to guest (%d:0x%x) (expected secondary %d), primary=%d\n",
							numUsers, nMask,
							s_idSecondaryUser, s_idPrimaryUser );
					}

					// Find the primary or secondary controller that was picked previously
					// and mark that guy as promoted to guest

					if ( !!( nMask & ( 1 << s_idSecondaryUser ) ) )
					{
						// Figure out which slot this controller will occupy
						// int nSlotPrimary = (s_idPrimaryUser < s_idSecondaryUser) ? 0 : 1;
						int nSlotSecondary = (s_idPrimaryUser < s_idSecondaryUser) ? 1 : 0;

						if ( UI_IsDebug() )
						{
							Msg( "[GAMEUI] Marking slot%d ctrlrs%d as guest\n",
								nSlotSecondary, s_idSecondaryUser );
						}

						// Mark the secondary slot as guest
						s_bSecondaryUserIsGuest = 1;
					}

					// Start the game
					StartGame( s_idPrimaryUser, s_idSecondaryUser );
				}
				break;
			}
			return;
		}
	}
#endif
}

void CAttractScreen::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pPressStartlbl = dynamic_cast< vgui::Label* >( FindChildByName( "LblPressStart" ) );
	if ( m_pPressStartlbl && !m_pPressStartShadowlbl )
	{
		// title shadow
		wchar_t buffer[128];
		m_pPressStartlbl->GetText( buffer, sizeof( buffer ) );

		m_pPressStartShadowlbl = new CShadowLabel( this, "LblPressStartShadow", buffer );
		SETUP_PANEL( m_pPressStartShadowlbl );
		m_pPressStartShadowlbl->SetFont( pScheme->GetFont( "FrameTitleBlur", IsProportional() ) );
		m_pPressStartShadowlbl->SizeToContents();

		int x, y;
		m_pPressStartlbl->GetPos( x, y );
		m_pPressStartShadowlbl->SetPos( x, y );
		int z;
		z = m_pPressStartlbl->GetZPos();
		m_pPressStartShadowlbl->SetZPos( z+1 );
		int wide, tall;
		m_pPressStartlbl->GetSize( wide, tall );
		m_pPressStartShadowlbl->SetSize( wide, tall );

		m_pPressStartShadowlbl->SetContentAlignment( vgui::Label::a_center );
		m_pPressStartShadowlbl->SetFgColor( Color( 200, 200, 200, 255 ) );
		m_pPressStartShadowlbl->SetVisible( true );
	}

	if ( m_pPressStartlbl && m_bHidePressStart )
	{
		HidePressStart();
	}

	// press start and its shadow fade in
	m_flFadeStart = Plat_FloatTime() + TRANSITION_TO_MOVIE_DELAY_TIME;
	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetAlpha( 0 );
	}
	if ( m_pPressStartShadowlbl )
	{
		m_pPressStartShadowlbl->SetAlpha( 0 );
	}
}

void CAttractScreen::OpenMainMenu()
{
	m_bHidePressStart = false;
	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL, true );
}

void CAttractScreen::OpenMainMenuJoinFailed( const char *msg )
{
	m_bHidePressStart = false;
	CBaseModPanel::GetSingleton().CloseAllWindows();
	
	CBaseModFrame *pMainMenu = CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL, true );
	if ( pMainMenu )
		pMainMenu->PostMessage( pMainMenu, new KeyValues( "OpenMainMenuJoinFailed", "msg", msg ) );
}

void CAttractScreen::OnChangeGamersFromMainMenu()
{
	CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
	CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
	ShowPressStart();
}

void CAttractScreen::StartWaitingForBlade1()
{
#ifdef _X360
	if ( IsPrimaryUserSignedInProperly() )
	{
		StartGame( s_idPrimaryUser );
		return;
	}

	m_eSignInUI = SIGNIN_SINGLE;

	XEnableGuestSignin( FALSE );
	XShowSigninUI( 1, XSSUI_FLAGS_LOCALSIGNINONLY );

	SetBladeStatus( BLADE_WAITINGFOROPEN );
#endif
}

void CAttractScreen::StartWaitingForBlade2()
{
#ifdef _X360
	m_eSignInUI = SIGNIN_DOUBLE;

	// Determine if the primary user is properly signed in to LIVE
	bool bShowLiveProfiles = IsUserSignedInToLiveWithMultiplayer( s_idPrimaryUser ) &&
		!IsUserSignedInProperly( s_idSecondaryUser );

	XEnableGuestSignin( TRUE );
	XShowSigninUI( 2, bShowLiveProfiles ? XSSUI_FLAGS_SHOWONLYONLINEENABLED : XSSUI_FLAGS_LOCALSIGNINONLY );

	SetBladeStatus( BLADE_WAITINGFOROPEN );
#endif
}

CAttractScreen::BladeStatus_t CAttractScreen::GetBladeStatus()
{
	return m_bladeStatus;
}

void CAttractScreen::SetBladeStatus( BladeStatus_t bladeStatus )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::SetBladeStatus( %d )\n", bladeStatus );
	}

	m_bladeStatus = bladeStatus;
}

void CAttractScreen::HidePressStart()
{
	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetVisible( false );
	}

	if ( m_pPressStartShadowlbl )
	{
		m_pPressStartShadowlbl->SetVisible( false );
	}
}

void CAttractScreen::HideFooter()
{
	// Make sure we hide the footer
	if ( CBaseModFrame *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel() )
	{
		pFooter->SetVisible( false );
	}
}

void CAttractScreen::HideProgress()
{
	CBaseModFrame *pMsg = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN );
	if ( pMsg )
	{
		pMsg->Close();
	}
}

void CAttractScreen::HideMsgs()
{
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
	if ( confirmation && confirmation->GetUsageId() == m_msgData )
	{
		confirmation->Close();
	}
}

void CAttractScreen::ShowPressStart()
{
#ifdef _X360
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::ShowPressStart\n" );
	}

	m_pfnMsgChanged = NULL;

	XBX_ResetUserIdSlots();
	XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
	XBX_SetPrimaryUserIsGuest( 0 );	
	XBX_SetNumGameUsers( 0 ); // users not selected yet
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", int(0) ) );

	if ( m_pPressStartlbl )
	{
		m_pPressStartlbl->SetVisible( true );
	}

	if ( m_pPressStartShadowlbl )
	{
		m_pPressStartShadowlbl->SetVisible( true );
	}

	m_bHidePressStart = false;
#endif
}

void CAttractScreen::ShowSignInDialog( int iPrimaryUser, int iSecondaryUser, BladeSignInUI_t eForceSignin )
{
	s_idPrimaryUser = iPrimaryUser;
	s_idSecondaryUser = -1;
	s_bSecondaryUserIsGuest = 0;

	// Whoever presses start becomes the primary user
	// and determines who's config we load, etc.
	g_pInputSystem->SetPrimaryUserId( iPrimaryUser );

	// Lock the UI convar options to a particular splitscreen player slot
	SetGameUIActiveSplitScreenPlayerSlot( 0 );

	HidePressStart();
	m_bHidePressStart = true;

	if ( iSecondaryUser >= 0 )
	{
		s_idSecondaryUser = iSecondaryUser;
		StartWaitingForBlade2();
	}
	else if ( eForceSignin == SIGNIN_DOUBLE )
	{
		StartWaitingForBlade2();
	}
	else if ( IsPrimaryUserSignedInProperly() )
	{
		StartGame( s_idPrimaryUser );
	}
	else if ( eForceSignin == SIGNIN_SINGLE )
	{
		StartWaitingForBlade1();
	}
	else
	{
		CBaseModPanel::GetSingleton().SetLastActiveUserId( iPrimaryUser );
		CBaseModPanel::GetSingleton().OpenWindow( WT_SIGNINDIALOG, this, false );
	}
}

static void PlayGameWithTemporaryProfile();
void CAttractScreen::StartGameWithTemporaryProfile_Stage1()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGameWithTemporaryProfile_Stage1 - pri=%d\n", s_idPrimaryUser );
	}

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;

	data.pWindowTitle = "#L4D360UI_MsgBx_ConfirmGuestC";
	data.pMessageText = "#L4D360UI_MsgBx_ConfirmGuestTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	data.pfnOkCallback = PlayGameWithTemporaryProfile;
	data.pfnCancelCallback = ChangeGamers;

	m_msgData = confirmation->SetUsageData(data);
	m_pfnMsgChanged = ChangeGamers;

	HideFooter();
}

void CAttractScreen::StartGameWithTemporaryProfile_Real()
{
#if defined( _X360 )
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGameWithTemporaryProfile_Real - pri=%d\n", s_idPrimaryUser );
	}

	// Turn off all other controllers
	XBX_ClearUserIdSlots();

	g_pInputSystem->SetPrimaryUserId( s_idPrimaryUser );

	XBX_SetPrimaryUserId( s_idPrimaryUser );
	XBX_SetPrimaryUserIsGuest( 1 );		// primary user id is retained

	XBX_SetUserId( 0, s_idPrimaryUser );
	XBX_SetUserIsGuest( 0, 1 );

	XBX_SetNumGameUsers( 1 );			// playing single-screen
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", int(1) ) );

	OpenMainMenu();
#endif //defined( _X360 )
}

static void ChangeGamers()
{
	CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >(
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true ) ) )
	{
		attractScreen->ShowPressStart();
	}
}

static void PlayGameWithTemporaryProfile()
{
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();
		attractScreen->StartGameWithTemporaryProfile_Real();
	}
	else
	{
		ChangeGamers();
	}
}

static void PlayGameWithSelectedProfiles()
{
	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();
		attractScreen->StartGame( s_idPrimaryUser, s_idSecondaryUser );
	}
	else
	{
		ChangeGamers();
	}
}

bool CAttractScreen::OfferPromoteToLiveGuest()
{
#ifdef _X360
	int state1 = XUserGetSigninState( s_idPrimaryUser );
	int state2 = XUserGetSigninState( s_idSecondaryUser );

	BOOL bPriv1, bPriv2;

	if ( ERROR_SUCCESS != XUserCheckPrivilege( s_idPrimaryUser, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPriv1 ) )
		bPriv1 = FALSE;
	if ( ERROR_SUCCESS != XUserCheckPrivilege( s_idSecondaryUser, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPriv2 ) )
		bPriv2 = FALSE;

	BOOL bProperSignin1 = IsUserSignedInProperly( s_idPrimaryUser ) ? 1 : 0;
	BOOL bProperSignin2 = IsUserSignedInProperly( s_idSecondaryUser ) ? 1 : 0;

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] OfferPromoteToLiveGuest ( 0: %d %d %d %d ; 1: %d %d %d %d )\n",
			s_idPrimaryUser, state1, bPriv1, bProperSignin1, s_idSecondaryUser, state2, bPriv2, bProperSignin2 );
	}

	if ( ( bProperSignin2 > bProperSignin1 ) ||
		 ( bProperSignin2 && ( bPriv2 > bPriv1 ) ) )
	{
		V_swap( s_idPrimaryUser, s_idSecondaryUser );
		V_swap( state1, state2 );
		V_swap( bPriv1, bPriv2 );
		V_swap( bProperSignin1, bProperSignin2 );
	}

	if ( !bProperSignin2 )
	{
		// Secondary guy has all access to LIVE, but is
		// not properly signed-in
		s_bSecondaryUserIsGuest = 1;
	}

	// Now if somebody is Live, then it's the first ctrlr
	if ( state1 == eXUserSigninState_SignedInToLive && bPriv1 )
	{
		if ( !bPriv2 )
		{
			// We should offer the secondary user to upgrade to guest
			SetBladeStatus( BLADE_WAITINGTOSHOWPROMOTEUI );

			if ( UI_IsDebug() )
			{
				Msg( "[GAMEUI] OfferPromoteToLiveGuest - offering to ctrlr %d\n",
					s_idSecondaryUser );
			}

			return true;
		}
	}
	// Nobody is signed in to Live
	else
	{
		// We can't show this dialog, because it doesn't take into account that both players me be using LIVE accounts
		// with multiplayer disabled (or one Silver account). We don't have time to add localization for these other 
		// cases. So no instead of popping this choice dialog we're going to go ahead with it and let them get warnings
		// later when they try to access mutiplayer items from the menu with the correct detection and text.
		PlayGameWithSelectedProfiles();

		/*GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MsgBx_NeedLiveSplitscreenC";
		data.pMessageText = "#L4D360UI_MsgBx_NeedLiveSplitscreenTxt";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;

		data.pfnOkCallback = PlayGameWithSelectedProfiles;
		data.pfnCancelCallback = ChangeGamers;

		m_msgData = confirmation->SetUsageData(data);
		m_pfnMsgChanged = ChangeGamers;

		HideFooter();*/

		return true;
	}

#endif

	return false;
}

void CAttractScreen::StartGame( int idxUser1 /* = -1 */, int idxUser2 /* = -1 */ )
{
#if defined( _X360 )
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGame - starting for %d and %d.\n", idxUser1, idxUser2 );
	}

	// Keep information about our users
	s_idPrimaryUser = idxUser1;
	s_idSecondaryUser = idxUser2;

	// Turn off all controllers
	XBX_ClearUserIdSlots();

	// Configure the game type and controller assignments
	g_pInputSystem->SetPrimaryUserId( idxUser1 );
	XBX_SetPrimaryUserId( idxUser1 );
	XBX_SetPrimaryUserIsGuest( 0 );

	if ( idxUser2 < 0 )
	{
		XBX_SetUserId( 0, idxUser1 );
		XBX_SetUserIsGuest( 0, 0 );

		XBX_SetNumGameUsers( 1 );
	}
	else
	{
		// Second user's guest status should be remembered earlier if this is the case
		XBX_SetUserId( 0, min( idxUser1, idxUser2 ) );
		XBX_SetUserId( 1, max( idxUser1, idxUser2 ) );

		int iSecondaryUserSlot = (idxUser2 < idxUser1) ? 0 : 1;
		XBX_SetUserIsGuest( iSecondaryUserSlot, s_bSecondaryUserIsGuest );

		XBX_SetNumGameUsers( 2 );	// Splitscreen
	}

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", (int) XBX_GetNumGameUsers() ) );

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] Starting game...\n" );
		Msg( "[GAMEUI]   num game users = %d\n", XBX_GetNumGameUsers() );
		Msg( "[GAMEUI]   pri ctrlr%d guest%d\n", XBX_GetPrimaryUserId(), XBX_GetPrimaryUserIsGuest() );
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			Msg( "[GAMEUI]   slot%d ctrlr%d guest%d\n", k, XBX_GetUserId( k ), XBX_GetUserIsGuest( k ) );
		}
	}

	// Select storage device for user1
	StartGame_Stage1_Storage1();
#endif //defined( _X360 )

}

static void StorageContinue()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StorageContinue - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();

		switch ( s_eStorageUI )
		{
		case 1:
			attractScreen->StartGame_Stage2_Storage2();
			return;
		case 2:
			attractScreen->StartGame_Stage3_Ready();
			return;
		default:
			ChangeGamers();
			return;
		}
	}
	else
	{
		ChangeGamers();
	}
}

static void StorageSelectAgain()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::CBCK=StorageSelectAgain - pri=%d, sec=%d, stage=%d\n", s_idPrimaryUser, s_idSecondaryUser, s_eStorageUI );
	}

	if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
	{
		attractScreen->OnMsgResetCancelFn();

		int iSlot = s_eStorageUI - 1;
		int iContorller = XBX_GetUserId( iSlot );
		XBX_SetStorageDeviceId( iContorller, XBX_INVALID_STORAGE_ID );

		switch ( s_eStorageUI )
		{
		case 1:
			attractScreen->StartGame_Stage1_Storage1();
			return;
		case 2:
			attractScreen->StartGame_Stage2_Storage2();
			return;
		default:
			ChangeGamers();
			return;
		}
	}
	else
	{
		ChangeGamers();
	}
}

void CAttractScreen::ReportDeviceFail( ISelectStorageDeviceClient::FailReason_t eReason )
{
	if ( eReason == ISelectStorageDeviceClient::FAIL_NOT_SELECTED &&
		 // Check if command line allows to bypass warning in this case
		 CommandLine()->FindParm( "-nostorage" ) )
	{
		StorageContinue();
		return;
	}

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	GenericConfirmation::Data_t data;

	switch ( eReason )
	{
	case ISelectStorageDeviceClient::FAIL_ERROR:
	case ISelectStorageDeviceClient::FAIL_NOT_SELECTED:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceNoneC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceNoneTxt";
		break;
	case ISelectStorageDeviceClient::FAIL_FULL:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceFullC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceFullTxt";
		break;
	case ISelectStorageDeviceClient::FAIL_CORRUPT:
	default:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceCorruptC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceCorruptTxt";
		break;
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	
	data.pfnOkCallback = StorageContinue;
	data.pfnCancelCallback = StorageSelectAgain;

	m_msgData = confirmation->SetUsageData(data);
	m_pfnMsgChanged = ChangeGamers;

	HideFooter();
}

void CAttractScreen::StartGame_Stage1_Storage1()
{
#ifdef _X360
	HideProgress();

	if ( XBX_GetUserIsGuest( 0 ) )
	{
		StartGame_Stage2_Storage2();
		return;
	}

	m_eStorageUI = STORAGE_0;
	s_eStorageUI = STORAGE_0;
	SetBladeStatus( BLADE_WAITINGTOSHOWSTORAGESELECTUI );

	// Now we should expect one of the following:
	//		msg = ReportNoDeviceSelected
	//		msg = ReportDeviceFull
	//		selected callback followed by loaded callback, then XBX_GetStorageDeviceId will be set for our controller
#endif
}

void CAttractScreen::StartGame_Stage2_Storage2()
{
#ifdef _X360
	HideProgress();

	if ( XBX_GetNumGameUsers() < 2 ||
		 XBX_GetUserIsGuest( 1 ) )
	{
		StartGame_Stage3_Ready();
		return;
	}

	m_eStorageUI = STORAGE_1;
	s_eStorageUI = STORAGE_1;
	SetBladeStatus( BLADE_WAITINGTOSHOWSTORAGESELECTUI );

	// Now we should expect one of the following:
	//		msg = ReportNoDeviceSelected
	//		msg = ReportDeviceFull
	//		selected callback followed by loaded callback, then XBX_GetStorageDeviceId will be set for our controller
#endif
}

void CAttractScreen::StartGame_Stage3_Ready()
{
	HideProgress();
	StartGame_Real( s_idPrimaryUser, s_idSecondaryUser );
}

void CAttractScreen::StartGame_Real( int idxUser1 /* = -1 */, int idxUser2 /* = -1 */ )
{
#if defined( _X360 )
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::StartGame_Real - starting for %d and %d.\n", idxUser1, idxUser2 );
	}

	extern CUtlString g_CurrentModeIdSave;
	char const *szDefaultOption = CUIGameData::Get()->SignedInToLive() ? "BtnCoop" : "BtnPlaySolo";
	g_CurrentModeIdSave = szDefaultOption;

	NavigateFrom();

	OpenMainMenu();
#endif //defined( _X360 )
}

void CAttractScreen::OnMsgResetCancelFn()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CAttractScreen::OnMsgResetCancelFn\n" );
	}

	m_pfnMsgChanged = NULL;
}
