//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "basemodpanel.h"
#include "basemodframe.h"
#include "UIGameData.h"


#include "VGenericConfirmation.h"
#include "VAttractScreen.h"

// vgui controls
#include "vgui/ILocalize.h"

// matchsystem
#include "matchmaking/imatchframework.h"

#ifndef _X360
#include "steam/steam_api.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace BaseModUI;
using namespace vgui;


//
// Invite approval
//

static int s_nInviteApprovalConf = 0;
static ISelectStorageDeviceClient *s_pPendingInviteStorageSelector = NULL;

enum InviteUserMapping_t
{
	INVITE_USER_BLOCK_INPUT,
	INVITE_USER_ALLOW_INPUT
};
static void Invite_MapUserForUiInput( InviteUserMapping_t eUi )
{
	// Check invited user if it was the active user
#ifdef _X360
	if ( XBX_GetInvitedUserId() == XBX_INVALID_USER_ID )
		return;

	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetInvitedUserId() == (DWORD) XBX_GetUserId( k ) )
			return;
	}

	// Invited user artificial mapping
	switch ( eUi )
	{
	case INVITE_USER_BLOCK_INPUT:
		XBX_ClearUserId( XBX_GetInvitedUserId() );
		break;
	case INVITE_USER_ALLOW_INPUT:
		XBX_SetUserId( XBX_GetNumGameUsers(), XBX_GetInvitedUserId() );
		break;
	}
#endif
}

static void Invite_NotifyAction( char const *szNotifyAction )
{
	s_nInviteApprovalConf = 0;

	Invite_MapUserForUiInput( INVITE_USER_BLOCK_INPUT );

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"OnInvite", "action", szNotifyAction ) );
}

static void Invite_Approved()
{
	CUIGameData::Get()->Invite_Connecting();
	Invite_NotifyAction( "join" );
}
static void Invite_Declined()
{
	Invite_NotifyAction( "deny" );
}

void CUIGameData::RunFrame_Invite()
{
	if ( s_nInviteApprovalConf )
	{
		// Check that the confirmation wasn't dismissed without notifying the invite system
		GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
		if ( !confirmation ||
			confirmation->GetUsageId() != s_nInviteApprovalConf )
		{
			// Well, pretend like user declined the prompt
			Invite_Declined();
		}
	}

	if ( s_pPendingInviteStorageSelector && !IsXUIOpen() )
	{
		SelectStorageDevice( s_pPendingInviteStorageSelector );
		s_pPendingInviteStorageSelector = NULL;
	}
}


//=============================================================================
#ifdef _X360
class CInviteSelectStorageDevice : public CChangeStorageDevice
{
public:
	explicit CInviteSelectStorageDevice();

public:
	virtual void DeviceChangeCompleted( bool bChanged );
};

CInviteSelectStorageDevice::CInviteSelectStorageDevice() :
	CChangeStorageDevice( XBX_GetInvitedUserId() )
{
	// Get UI panel
	CBaseModPanel &ui = CBaseModPanel::GetSingleton();
	ui.OnGameUIActivated();

	// Allow non-involved controller
	m_bAnyController = true;

	// Don't force to re-select, just reload configs
	m_bForce = false;

	Invite_MapUserForUiInput( INVITE_USER_ALLOW_INPUT );
}

void CInviteSelectStorageDevice::DeviceChangeCompleted( bool bChanged )
{
	CChangeStorageDevice::DeviceChangeCompleted( bChanged );

	// Proceed with joining the invite session
	Invite_NotifyAction( "join" );
}
#endif

//=============================================================================
bool CUIGameData::Invite_IsStorageDeviceValid()
{
#ifdef _X360
	//
	// Note: the only code path that should lead to this routine is
	// from invite accepting code.
	// XBX_GetInvitedUserId() is set to the user id of who accepted the invite
	// For that user the storage device has to be validated.
	//
	// If this function returns "true" it means that the user has a valid device
	// selected and it is safe to proceed with the invite.
	//
	// If this function returns "false" it will send the "join" action OnInvite event
	// after storage device selection process is over.
	//

	int iCtrlr = XBX_GetInvitedUserId();
	if ( iCtrlr < 0 || iCtrlr >= XUSER_MAX_COUNT )
		return true;

	// Check what device the guy currently has mapped
	DWORD dwDevice = XBX_GetStorageDeviceId( iCtrlr );

	if ( XBX_DescribeStorageDevice( dwDevice ) ||
		 XBX_STORAGE_DECLINED == dwDevice )
		// The guy has a valid device selected
		// or allow the guy to play because we already
		// told him earlier that his settings will not
		// be saved
		return true;

	//
	// Need to show device selector
	//

	s_pPendingInviteStorageSelector = new CInviteSelectStorageDevice;

	return false;

#endif
	return true;
}

void CUIGameData::Invite_Confirm()
{
	// Activate game ui
	CBaseModPanel &ui = CBaseModPanel::GetSingleton();
	if ( !ui.IsVisible() )
	{
		// Activate game ui to see the dialog
		engine->ExecuteClientCmd( "gameui_activate" );
	}

	// Get current window
	CBaseModFrame *pFrame = NULL;
	WINDOW_TYPE wt = ui.GetActiveWindowType();
	if ( wt != WT_NONE &&
		wt != WT_GENERICCONFIRMATION )
		pFrame = ui.GetWindow( wt );

	// Show a prompt
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( ui.
		OpenWindow( WT_GENERICCONFIRMATION, pFrame, false ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#L4D360UI_LeaveInviteConf";
	data.pMessageText = "#L4D360UI_LeaveInviteConfTxt";
	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	data.pfnOkCallback = Invite_Approved;
	data.pfnCancelCallback = Invite_Declined;

	s_nInviteApprovalConf = confirmation->SetUsageData(data);

	Invite_MapUserForUiInput( INVITE_USER_ALLOW_INPUT );
}

void CUIGameData::Invite_Connecting()
{
	// Close any session that we might have outstanding
	g_pMatchFramework->CloseSession();

	// Navigate to attract screen which might take a frame
	CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );
	CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_ACCEPTINVITE );
	CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
}


