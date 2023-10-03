//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "basemodpanel.h"
#include "basemodframe.h"
#include "UIGameData.h"

#include <ctype.h>


#include "./GameUI/IGameUI.h"
#include "ienginevgui.h"
#include "icommandline.h"
#include "vgui/ISurface.h"
#include "EngineInterface.h"
#include "tier0/dbg.h"
#include "ixboxsystem.h"
#include "GameUI_Interface.h"
#include "game/client/IGameClientExports.h"
#include "fmtstr.h"
#include "vstdlib/random.h"
#include "utlbuffer.h"
#include "filesystem/IXboxInstaller.h"
#include "tier1/tokenset.h"
#include "FileSystem.h"
#include "filesystem/IXboxInstaller.h"

#include <time.h>

// BaseModUI High-level windows

#include "VFoundGames.h"
#include "VFoundGroupGames.h"
#include "VGameLobby.h"
#include "VGenericConfirmation.h"
#include "VGenericWaitScreen.h"
#include "VInGameMainMenu.h"
#include "VMainMenu.h"
#include "VFooterPanel.h"
#include "VAttractScreen.h"
#include "VPasswordEntry.h"
// vgui controls
#include "vgui/ILocalize.h"



#ifndef _X360
#include "steam/steam_api.h"
#endif

#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
extern const char *COM_GetModDirectory( void );

ConVar x360_audio_english("x360_audio_english", "0", 0, "Keeps track of whether we're forcing english in a localized language." );

ConVar demo_ui_enable( "demo_ui_enable", "", FCVAR_DEVELOPMENTONLY, "Suffix for the demo UI" );
ConVar demo_connect_string( "demo_connect_string", "", FCVAR_DEVELOPMENTONLY, "Connect string for demo UI" );

///Asyncronous Operations

ConVar mm_ping_max_green( "ping_max_green", "70" );
ConVar mm_ping_max_yellow( "ping_max_yellow", "140" );
ConVar mm_ping_max_red( "ping_max_red", "250" );

//=============================================================================

const tokenset_t< const char * > BaseModUI::s_characterPortraits[] =
{
	{ "",			"select_Random" },
	{ "random",		"select_Random" },

	//{ "BtnNamVet",	"select_Bill" },
	//{ "BtnTeenGirl",	"select_Zoey" },
	//{ "BtnBiker",		"select_Francis" },
	//{ "BtnManager",	"select_Louis" },

	{ "coach",		"s_panel_lobby_coach" },
	{ "producer",	"s_panel_lobby_producer" },
	{ "gambler",	"s_panel_lobby_gambler" },
	{ "mechanic",	"s_panel_lobby_mechanic" },

	{ "infected",	"s_panel_hand" },

	{ NULL, "" }
};

//=============================================================================
// Xbox 360 Marketplace entry point
//=============================================================================
struct X360MarketPlaceEntryPoint
{
	DWORD dwEntryPoint;
	uint64 uiOfferID;
};
static X360MarketPlaceEntryPoint g_MarketplaceEntryPoint;

#ifdef _X360
struct X360MarketPlaceQuery
{
	uint64 uiOfferID;
	HRESULT hResult;
	XOVERLAPPED xOverlapped;
};
static CUtlVector< X360MarketPlaceQuery * > g_arrMarketPlaceQueries;
#endif

static void GoToMarketplaceForOffer()
{
#ifdef _X360
	// Stop installing to the hard drive, otherwise STFC fragmentation hazard, as multiple non sequential HDD writes will occur.
	// This needs to be done before the DLC might be downloaded to the HDD, otherwise it could be fragmented.
	// We restart the installer on DLC download completion. We do not handle the cancel/abort case. The installer
	// will restart through the pre-dlc path, i.e. after attract or exiting a map back to the main menu.
	if ( g_pXboxInstaller )
		g_pXboxInstaller->Stop();

	// See if we need to free some of the queries
	for ( int k = 0; k < g_arrMarketPlaceQueries.Count(); ++ k )
	{
		X360MarketPlaceQuery *pQuery = g_arrMarketPlaceQueries[k];
		if ( XHasOverlappedIoCompleted( &pQuery->xOverlapped ) )
		{
			delete pQuery;
			g_arrMarketPlaceQueries.FastRemove( k -- );
		}
	}

	// Allocate a new query
	X360MarketPlaceQuery *pQuery = new X360MarketPlaceQuery;
	memset( pQuery, 0, sizeof( *pQuery ) );
	pQuery->uiOfferID = g_MarketplaceEntryPoint.uiOfferID;
	g_arrMarketPlaceQueries.AddToTail( pQuery );

	// Open the marketplace entry point
	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	xonline->XShowMarketplaceDownloadItemsUI( iCtrlr,
		g_MarketplaceEntryPoint.dwEntryPoint, &pQuery->uiOfferID, 1,
		&pQuery->hResult, &pQuery->xOverlapped );
#endif
}

static void ShowMarketplaceUiForOffer()
{
#ifdef _X360
	// Stop installing to the hard drive, otherwise STFC fragmentation hazard, as multiple non sequential HDD writes will occur.
	// This needs to be done before the DLC might be downloaded to the HDD, otherwise it could be fragmented.
	// We restart the installer on DLC download completion. We do not handle the cancel/abort case. The installer
	// will restart through the pre-dlc path, i.e. after attract or exiting a map back to the main menu.
	if ( g_pXboxInstaller )
		g_pXboxInstaller->Stop();

	// Open the marketplace entry point
	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	DWORD ret = xonline->XShowMarketplaceUI( iCtrlr, g_MarketplaceEntryPoint.dwEntryPoint, g_MarketplaceEntryPoint.uiOfferID, DWORD( -1 ) );
	DevMsg( "XShowMarketplaceUI for offer %llx entry point %d ctrlr%d returned %d\n",
		g_MarketplaceEntryPoint.uiOfferID, g_MarketplaceEntryPoint.dwEntryPoint, iCtrlr, ret );
#endif
}

#ifdef _X360
CON_COMMAND( x360_marketplace_offer, "Get a known offer from x360 marketplace" )
{
	if ( args.ArgC() != 4 )
	{
		Warning( "Usage: x360_marketplace_offer type 0xOFFERID ui|dl\n" );
		return;
	}

	int iEntryPoint = Q_atoi( args.Arg( 1 ) );
	char const *szArg2 = args.Arg( 2 );
	uint64 uiOfferId = 0ull;
	if ( 1 != sscanf( szArg2, "0x%llx", &uiOfferId ) )
		uiOfferId = 0ull;

	// Go to marketplace
	g_MarketplaceEntryPoint.dwEntryPoint = iEntryPoint;
	g_MarketplaceEntryPoint.uiOfferID = uiOfferId;
	if ( !Q_stricmp( args.Arg( 3 ), "ui" ) )
		ShowMarketplaceUiForOffer();
	else
		GoToMarketplaceForOffer();
}
#endif

//=============================================================================
//
//=============================================================================
CUIGameData* CUIGameData::m_Instance = 0;
bool CUIGameData::m_bModuleShutDown = false;

//=============================================================================
CUIGameData::CUIGameData() :
#if !defined( _X360 ) && !defined( NO_STEAM )
	m_CallbackPersonaStateChanged( this, &CUIGameData::Steam_OnPersonaStateChanged ),
#endif
	m_CGameUIPostInit( false )
{
	m_LookSensitivity = 1.0f;

	m_flShowConnectionProblemTimer = 0.0f;
	m_flTimeLastFrame = Plat_FloatTime();
	m_bShowConnectionProblemActive = false;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_bXUIOpen = false;

	m_bWaitingForStorageDeviceHandle = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = NULL;

	m_pSelectStorageClient = NULL;

	SetDefLessFunc( m_mapUserXuidToAvatar );
	SetDefLessFunc( m_mapUserXuidToName );
}

//=============================================================================
CUIGameData::~CUIGameData()
{
	// Unsubscribe from events system
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//=============================================================================
CUIGameData* CUIGameData::Get()
{
	if ( !m_Instance && !m_bModuleShutDown )
	{
		m_Instance = new CUIGameData();
	}

	return m_Instance;
}

void CUIGameData::Shutdown()
{
	if ( !m_bModuleShutDown )
	{
		m_bModuleShutDown = true;
		delete m_Instance;
		m_Instance = NULL;
	}
}

#ifdef _X360
CON_COMMAND( ui_fake_connection_problem, "" )
{
	int numMilliSeconds = 1000;
	if ( args.ArgC() > 1 )
	{
		numMilliSeconds = Q_atoi( args.Arg( 1 ) );
	}
	
	float flTime = Plat_FloatTime();
	DevMsg( "ui_fake_connection_problem %d @%.2f\n", numMilliSeconds, flTime );

	int numTries = 2;
	while ( ( 1000 * ( Plat_FloatTime() - flTime ) < numMilliSeconds ) &&
		numTries --> 0 )
	{
		ThreadSleep( numMilliSeconds + 50 );
	}

	flTime = Plat_FloatTime();
	DevMsg( "ui_fake_connection_problem finished @%.2f\n", flTime );
}
#endif

//=============================================================================
void CUIGameData::RunFrame()
{
	RunFrame_Storage();

	RunFrame_Invite();

	if ( m_flShowConnectionProblemTimer > 0.0f )
	{
		float flCurrentTime = Plat_FloatTime();
		float flTimeElapsed = ( flCurrentTime - m_flTimeLastFrame );

		m_flTimeLastFrame = flCurrentTime;

		if ( flTimeElapsed > 0.0f )
		{
			m_flShowConnectionProblemTimer -= flTimeElapsed;
		}

		if ( m_flShowConnectionProblemTimer > 0.0f )
		{
			if ( !m_bShowConnectionProblemActive &&
				 !CBaseModPanel::GetSingleton().IsVisible() )
			{
				GameUI().ActivateGameUI();
				OpenWaitScreen( "#GameUI_RetryingConnectionToServer", 0.0f );
				m_bShowConnectionProblemActive = true;
			}
		}
		else
		{
			if ( m_bShowConnectionProblemActive )
			{
				// Before closing this particular waitscreen we need to establish
				// a correct navback, otherwise it will not close - Vitaliy (bugbait #51272)
				if ( CBaseModFrame *pWaitScreen = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
				{
					if ( !pWaitScreen->GetNavBack() )
					{
						if ( CBaseModFrame *pIngameMenu = CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) )
							pWaitScreen->SetNavBack( pIngameMenu );
					}

					if ( !pWaitScreen->GetNavBack() )
					{
						// This waitscreen will fail to close, force the close!
						pWaitScreen->Close();
					}
				}

				CloseWaitScreen( NULL, "Connection Problems" );
				GameUI().HideGameUI();
				m_bShowConnectionProblemActive = false;
			}
		}
	}
}

void CUIGameData::OnSetStorageDeviceId( int iController, uint nDeviceId )
{
	// Check to see if there is enough room on this storage device
	if ( nDeviceId == XBX_STORAGE_DECLINED || nDeviceId == XBX_INVALID_STORAGE_ID )
	{
		CloseWaitScreen( NULL, "ReportNoDeviceSelected" );
		m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_NOT_SELECTED );
		m_pSelectStorageClient = NULL;
	}
	else if ( xboxsystem->DeviceCapacityAdequate( iController, nDeviceId, COM_GetModDirectory() ) == false )
	{
		CloseWaitScreen( NULL, "ReportDeviceFull" );
		m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_FULL );
		m_pSelectStorageClient = NULL;
	}
	else
	{
		// Set the storage device
		XBX_SetStorageDeviceId( iController, nDeviceId );
		OnDeviceAttached();
		m_pSelectStorageClient->OnDeviceSelected();
	}
}

//=============================================================================
void CUIGameData::OnGameUIPostInit()
{
	m_CGameUIPostInit = true;
}

//=============================================================================
bool CUIGameData::CanPlayer2Join()
{
	if ( demo_ui_enable.GetString()[0] )
		return false;

#ifdef _X360
	if ( XBX_GetNumGameUsers() != 1 )
		return false;

	if ( XBX_GetPrimaryUserIsGuest() )
		return false;

	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() != WT_MAINMENU )
		return false;

	return true;
#else
	return false;
#endif
}

//=============================================================================
void CUIGameData::OpenFriendRequestPanel(int index, uint64 playerXuid)
{
#ifdef _X360 
	XShowFriendRequestUI(index, playerXuid);
#endif
}

//=============================================================================
void CUIGameData::OpenInviteUI( char const *szInviteUiType )
{
#ifdef _X360 
	int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iCtrlr = XBX_GetUserIsGuest( iSlot ) ? XBX_GetPrimaryUserId() : XBX_GetUserId( iSlot );
	
	if ( !Q_stricmp( szInviteUiType, "friends" ) )
		::XShowFriendsUI( iCtrlr );
	else if ( !Q_stricmp( szInviteUiType, "players" ) )
		xonline->XShowGameInviteUI( iCtrlr, NULL, 0, 0 );
	else if ( !Q_stricmp( szInviteUiType, "party" ) )
		xonline->XShowPartyUI( iCtrlr );
	else if ( !Q_stricmp( szInviteUiType, "inviteparty" ) )
		xonline->XPartySendGameInvites( iCtrlr, NULL );
	else if ( !Q_stricmp( szInviteUiType, "community" ) )
		xonline->XShowCommunitySessionsUI( iCtrlr, XSHOWCOMMUNITYSESSION_SHOWPARTY );
	else if ( !Q_stricmp( szInviteUiType, "voiceui" ) )
		::XShowVoiceChannelUI( iCtrlr );
	else if ( !Q_stricmp( szInviteUiType, "gamevoiceui" ) )
		::XShowGameVoiceChannelUI();
	else
	{
		DevWarning( "OpenInviteUI with wrong parameter `%s`!\n", szInviteUiType );
		Assert( 0 );
	}
#endif
}

void CUIGameData::ExecuteOverlayCommand( char const *szCommand )
{
#if !defined( _X360 ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamFriends() &&
		 steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
	{
		steamapicontext->SteamFriends()->ActivateGameOverlay( szCommand );
	}
	else
	{
		DisplayOkOnlyMsgBox( NULL, "#L4D360UI_SteamOverlay_Title", "#L4D360UI_SteamOverlay_Text" );
	}
#else
	ExecuteNTimes( 5, DevWarning( "ExecuteOverlayCommand( %s ) is unsupported\n", szCommand ) );
	Assert( !"ExecuteOverlayCommand" );
#endif
}

//=============================================================================
bool CUIGameData::SignedInToLive()
{
#ifdef _X360

	if ( XBX_GetNumGameUsers() <= 0 ||
		 XBX_GetPrimaryUserIsGuest() )
		 return false;

	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iController = XBX_GetUserId( k );
		IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( !player )
			return false;
		if ( player->GetOnlineState() != IPlayer::STATE_ONLINE )
			return false;
	}
#endif
	
	return true;
}

bool CUIGameData::AnyUserSignedInToLiveWithMultiplayerDisabled()
{
#ifdef _X360
	if ( XBX_GetNumGameUsers() <= 0 ||
		XBX_GetPrimaryUserIsGuest() )
		return false;

	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iController = XBX_GetUserId( k );
		IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( player && player->GetOnlineState() == IPlayer::STATE_NO_MULTIPLAYER )
			return true;
	}
#endif

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg )
{
#ifdef _X360
	bool bOnlineFound = false;
	if ( XBX_GetNumGameUsers() > 0 &&
		!XBX_GetPrimaryUserIsGuest() )
	{
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			int iController = XBX_GetUserId( k );
			IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
			if ( player && player->GetOnlineState() > IPlayer::STATE_OFFLINE )
				return false;
		}
	}

	if ( bOnlineFound )
		return false;

	DisplayOkOnlyMsgBox( pCallerFrame, "#L4D360UI_XboxLive", szMsg );
	return true;
#endif

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame )
{
	if ( !IsX360() )
		return false;

	if ( SignedInToLive() )
		return false;

	char const *szMsg = "";

	if ( AnyUserSignedInToLiveWithMultiplayerDisabled() )
	{
		szMsg = "#L4D360UI_MsgBx_NeedLiveNonGoldMsg";

#ifdef _X360
		// Show the splitscreen version if there are 2 non-guest accounts
		if ( XBX_GetNumGameUsers() > 1 && XBX_GetUserIsGuest( 0 ) == false && XBX_GetUserIsGuest( 1 ) == false )
		{
			szMsg = "#L4D360UI_MsgBx_NeedLiveNonGoldSplitscreenMsg";
		}
#endif
	}
	else
	{
		szMsg = "#L4D360UI_MsgBx_NeedLiveSinglescreenMsg";

#ifdef _X360
		// Show the splitscreen version if there are 2 non-guest accounts
		if ( XBX_GetNumGameUsers() > 1 && XBX_GetUserIsGuest( 0 ) == false && XBX_GetUserIsGuest( 1 ) == false )
		{
			szMsg = "#L4D360UI_MsgBx_NeedLiveSplitscreenMsg";
		}
#endif
	}

	DisplayOkOnlyMsgBox( pCallerFrame, "#L4D360UI_XboxLive", szMsg );

	return true;
}

void CUIGameData::DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg )
{
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pCallerFrame, false ) );
	GenericConfirmation::Data_t data;
	data.pWindowTitle = szTitle;
	data.pMessageText = szMsg;
	data.bOkButtonEnabled = true;
	confirmation->SetUsageData(data);
}

const char *CUIGameData::GetLocalPlayerName( int iController )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

	IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !player )
	{
		return "";
	}

	return player->GetName();
}


//////////////////////////////////////////////////////////////////////////


//=============================================================================
void CUIGameData::SetLookSensitivity(float sensitivity)
{
	m_LookSensitivity = sensitivity;

	static CGameUIConVarRef joy_yawsensitivity("joy_yawsensitivity");
	if(joy_yawsensitivity.IsValid())
	{
		float defaultValue = atof(joy_yawsensitivity.GetDefault());
		joy_yawsensitivity.SetValue(defaultValue * sensitivity);
	}

	static CGameUIConVarRef joy_pitchsensitivity("joy_pitchsensitivity");
	if(joy_pitchsensitivity.IsValid())
	{
		float defaultValue = atof(joy_pitchsensitivity.GetDefault());
		joy_pitchsensitivity.SetValue(defaultValue * sensitivity);
	}
}

//=============================================================================
float CUIGameData::GetLookSensitivity()
{
	return m_LookSensitivity;
}

bool CUIGameData::IsXUIOpen()
{
	return m_bXUIOpen;
}

void CUIGameData::OpenWaitScreen( const char * messageText, float minDisplayTime, KeyValues *pSettings, float maxDisplayTime )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] OpenWaitScreen( %s )\n", messageText );
	}

	WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	CBaseModFrame * backFrame = CBaseModPanel::GetSingleton().GetWindow( wtActive );
	if ( wtActive == WT_GENERICWAITSCREEN && backFrame )
	{
		backFrame = backFrame->GetNavBack();
		DevMsg( "CUIGameData::OpenWaitScreen - setting navback to %s instead of waitscreen\n", backFrame ? backFrame->GetName() : "NULL" );
	}
	if ( wtActive == WT_GENERICCONFIRMATION && backFrame )
	{
		DevWarning( "Cannot display waitscreen! Active window of higher priority: %s\n", backFrame->GetName() );
		return;
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICWAITSCREEN, backFrame, false, pSettings ) );
	if( waitScreen )
	{
		waitScreen->SetNavBack( backFrame );
		waitScreen->ClearData();
		waitScreen->AddMessageText( messageText, minDisplayTime );
		waitScreen->SetMaxDisplayTime( maxDisplayTime );
	}
}

void CUIGameData::UpdateWaitPanel( const char * messageText, float minDisplayTime )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] UpdateWaitPanel( %s )\n", messageText );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->AddMessageText( messageText, minDisplayTime );
	}
}

void CUIGameData::UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] UpdateWaitPanel( %S )\n", messageText );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->AddMessageText( messageText, minDisplayTime );
	}
}

void CUIGameData::CloseWaitScreen( vgui::Panel * callbackPanel, const char * message )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CloseWaitScreen( %s )\n", message );
	}

	GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	if( waitScreen )
	{
		waitScreen->SetCloseCallback( callbackPanel, message );
	}
}

void CUIGameData::NeedConnectionProblemWaitScreen ( void )
{
	m_flShowConnectionProblemTimer = 1.0f;
}

static void PasswordEntered()
{
	CUIGameData::Get()->FinishPasswordUI( true );
}

static void PasswordNotEntered()
{
	CUIGameData::Get()->FinishPasswordUI( false );
}

void CUIGameData::ShowPasswordUI( char const *pchCurrentPW )
{
	PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().OpenWindow( WT_PASSWORDENTRY, NULL, false ) );
	if ( pwEntry )
	{
		PasswordEntry::Data_t data;
		data.pWindowTitle = "#L4D360UI_PasswordEntry_Title";
		data.pMessageText = "#L4D360UI_PasswordEntry_Prompt";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;
		data.m_szCurrentPW = pchCurrentPW;
		data.pfnOkCallback = PasswordEntered;
		data.pfnCancelCallback = PasswordNotEntered;
		pwEntry->SetUsageData(data);
	}
}

void CUIGameData::FinishPasswordUI( bool bOk )
{
	PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().GetWindow( WT_PASSWORDENTRY ) );
	if ( pwEntry )
	{
		if ( bOk )
		{
			char pw[ 256 ];
			pwEntry->GetPassword( pw, sizeof( pw ) );
			engine->SetConnectionPassword( pw );
		}
		else
		{
			engine->SetConnectionPassword( "" );
		}
	}
}

IImage *CUIGameData::GetAvatarImage( XUID playerID )
{
#ifdef _X360
	return NULL;
#else
	if ( !playerID )
		return NULL;

	// do we already have this image cached?
	CGameUiAvatarImage *pImage = NULL;
	int iIndex = m_mapUserXuidToAvatar.Find( playerID );
	
	if ( iIndex == m_mapUserXuidToAvatar.InvalidIndex() )
	{
		// cache a new image
		pImage = new CGameUiAvatarImage();

		// We may fail to set the steam ID - if the player is not our friend and we are not in a lobby or game, eg
		if ( !pImage->SetAvatarSteamID( playerID ) )
		{
			delete pImage;
			return NULL;
		}

		iIndex = m_mapUserXuidToAvatar.Insert( playerID, pImage );
	}
	else
	{
		pImage = m_mapUserXuidToAvatar.Element( iIndex );
	}

	return pImage;
#endif // !_X360
}

char const * CUIGameData::GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

#if !defined( _X360 ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamUtils() &&
		steamapicontext->SteamFriends() && steamapicontext->SteamUser() )
	{
		int iIndex = m_mapUserXuidToName.Find( playerID );
		if ( iIndex == m_mapUserXuidToName.InvalidIndex() )
		{
			char const *szName = steamapicontext->SteamFriends()->GetFriendPersonaName( playerID );
			if ( szName && *szName )
			{
				iIndex = m_mapUserXuidToName.Insert( playerID, szName );
			}
		}

		if ( iIndex != m_mapUserXuidToName.InvalidIndex() )
			return m_mapUserXuidToName.Element( iIndex ).Get();
	}
#endif

	return szPlayerNameSpeculative;
}

#if !defined( _X360 ) && !defined( NO_STEAM )
void CUIGameData::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( !pParam->m_ulSteamID )
		return;

	if ( pParam->m_nChangeFlags & k_EPersonaChangeName )
	{
		int iIndex = m_mapUserXuidToName.Find( pParam->m_ulSteamID );
		if ( iIndex != m_mapUserXuidToName.InvalidIndex() )
		{
			CUtlString utlName = m_mapUserXuidToName.Element( iIndex );
			m_mapUserXuidToName.RemoveAt( iIndex );
			GetPlayerName( pParam->m_ulSteamID, utlName.Get() );
		}
	}

	if ( pParam->m_nChangeFlags & k_EPersonaChangeAvatar )
	{
		CGameUiAvatarImage *pImage = NULL;
		int iIndex = m_mapUserXuidToAvatar.Find( pParam->m_ulSteamID );
		if ( iIndex != m_mapUserXuidToAvatar.InvalidIndex() )
		{
			pImage = m_mapUserXuidToAvatar.Element( iIndex );
		}

		// Re-fetch the image if we have it cached
		if ( pImage )
		{
			pImage->SetAvatarSteamID( pParam->m_ulSteamID );
		}
	}
}
#endif

CON_COMMAND_F( ui_reloadscheme, "Reloads the resource files for the active UI window", 0 )
{
	g_pFullFileSystem->SyncDvdDevCache();
	CUIGameData::Get()->ReloadScheme();
}

void CUIGameData::ReloadScheme()
{
	CBaseModPanel::GetSingleton().ReloadScheme();
	CBaseModFrame *window = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	if( window )
	{
		window->ReloadSettings();
	}
	CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
	if( footer )
	{
		footer->ReloadSettings();
	}
}

CBaseModFrame * CUIGameData::GetParentWindowForSystemMessageBox()
{
	WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	WINDOW_PRIORITY wPriority = CBaseModPanel::GetSingleton().GetActiveWindowPriority();

	CBaseModFrame *pCandidate = CBaseModPanel::GetSingleton().GetWindow( wtActive );

	if ( pCandidate )
	{
		if ( wPriority >= WPRI_WAITSCREEN && wPriority <= WPRI_MESSAGE )
		{
			if ( UI_IsDebug() )
			{
				DevMsg( "GetParentWindowForSystemMessageBox: using navback of %s\n", pCandidate->GetName() );
			}

			// Message would not be able to nav back to waitscreen or another message
			pCandidate = pCandidate->GetNavBack();
		}
		else if ( wPriority > WPRI_MESSAGE )
		{
			if ( UI_IsDebug() )
			{
				DevMsg( "GetParentWindowForSystemMessageBox: using NULL since a higher priority window is open %s\n", pCandidate->GetName() );
			}

			// Message would not be able to nav back to a higher level priority window
			pCandidate = NULL;
		}
	}

	return pCandidate;
}

bool CUIGameData::IsActiveSplitScreenPlayerSpectating( void )
{
// 	int iLocalPlayerTeam;
// 	if ( GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) )
// 	{
// 		if ( iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() &&
// 			 iLocalPlayerTeam != GameClientExports()->GetTeamId_Infected() )
// 			return true;
// 	}

	return false;
}

void CUIGameData::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnSysXUIEvent", szEvent ) )
	{
		m_bXUIOpen = !Q_stricmp( "opening", pEvent->GetString( "action", "" ) );
	}
	else if ( !Q_stricmp( "OnProfileUnavailable", szEvent ) )
	{
#if defined( _DEMO ) && defined( _X360 )
		return;
#endif
		// Activate game ui to see the dialog
		if ( !CBaseModPanel::GetSingleton().IsVisible() )
		{
			engine->ExecuteClientCmd( "gameui_activate" );
		}

		// Pop a message dialog if their storage device was changed
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			GetParentWindowForSystemMessageBox(), false ) );
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MsgBx_AchievementNotWrittenTitle";
		data.pMessageText = "#L4D360UI_MsgBx_AchievementNotWritten"; 
		data.bOkButtonEnabled = true;

		confirmation->SetUsageData( data );
	}
	else if ( !Q_stricmp( "OnInvite", szEvent ) )
	{
		// Check if the user just accepted invite
		if ( !Q_stricmp( "accepted", pEvent->GetString( "action" ) ) )
		{
			// Check if we have an outstanding session
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( !pIMatchSession )
			{
				Invite_Connecting();
				return;
			}

			// User is accepting an invite and has an outstanding
			// session, TCR requires confirmation of destructive actions
			if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
			{
				*pnConfirmed = 0;
			}

			// Show the dialog
			Invite_Confirm();
		}
		else if ( !Q_stricmp( "storage", pEvent->GetString( "action" ) ) )
		{
			if ( !Invite_IsStorageDeviceValid() )
			{
				if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
				{
					*pnConfirmed = 0;	// make the invite accepting code wait
				}
			}
		}
		else if ( !Q_stricmp( "error", pEvent->GetString( "action" ) ) )
		{
			char const *szReason = pEvent->GetString( "error", "" );

			if ( XBX_GetNumGameUsers() < 2 )
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline1", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer1", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole1", RemapText_t::MATCH_FULL },
					{ NULL, NULL, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}
			else
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline2", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer2", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole2", RemapText_t::MATCH_FULL },
					{ NULL, NULL, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}

			// Show the message box
			GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				GetParentWindowForSystemMessageBox(), false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_XboxLive";
			data.pMessageText = szReason;
			data.bOkButtonEnabled = true;

			confirmation->SetUsageData(data);
		}
	}
	else if ( !Q_stricmp( "OnSysStorageDevicesChanged", szEvent ) )
	{
#if defined( _DEMO ) && defined( _X360 )
		return;
#endif

		// If a storage device change is in progress, the simply ignore
		// the notification callback, but pop the dialog
		if ( m_pSelectStorageClient )
		{
			DevWarning( "Ignored OnSysStorageDevicesChanged while the storage selection was in progress...\n" );
		}

		// Activate game ui to see the dialog
		if ( !CBaseModPanel::GetSingleton().IsVisible() )
		{
			engine->ExecuteClientCmd( "gameui_activate" );
		}

		// Pop a message dialog if their storage device was changed
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			GetParentWindowForSystemMessageBox(), false ) );
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#GameUI_Console_StorageRemovedTitle";
		data.pMessageText = "#L4D360UI_MsgBx_StorageDeviceRemoved"; 
		data.bOkButtonEnabled = true;
		
		extern void OnStorageDevicesChangedSelectNewDevice();
		data.pfnOkCallback = m_pSelectStorageClient ? NULL : &OnStorageDevicesChangedSelectNewDevice;	// No callback if already in the middle of selecting a storage device

		confirmation->SetUsageData( data );
	}
	else if ( !Q_stricmp( "OnSysInputDevicesChanged", szEvent ) )
	{
		unsigned int nInactivePlayers = 0;  // Number of users on the spectating team (ie. idle), or disconnected in this call
		int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
		int nDisconnectedDevices = pEvent->GetInt( "mask" );
		for ( unsigned int nSlot = 0; nSlot < XBX_GetNumGameUsers(); ++nSlot, nDisconnectedDevices >>= 1 )
		{
			engine->SetActiveSplitScreenPlayerSlot( nSlot );
			
			// See if this player is spectating (ie. idle)
			bool bSpectator = IsActiveSplitScreenPlayerSpectating();
			if ( bSpectator )
			{
				nInactivePlayers++;
			}
		
			if ( nDisconnectedDevices & 0x1 )
			{
				// Only count disconnections if that player wasn't idle
				if ( !bSpectator )
				{
					nInactivePlayers++;
				}

				engine->ClientCmd( "go_away_from_keyboard" );
			}
		}
		engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

		// If all the spectators and all the disconnections account for all possible users, we need to pop a message
		// Also, if the GameUI is up, always show the disconnection message
		if ( CBaseModPanel::GetSingleton().IsVisible() || nInactivePlayers == XBX_GetNumGameUsers() )
		{
			if ( !CBaseModPanel::GetSingleton().IsVisible() )
			{
				engine->ExecuteClientCmd( "gameui_activate" );
			}

			// Pop a message if a valid controller was removed!
			GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				GetParentWindowForSystemMessageBox(), false ) );
			
			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_MsgBx_ControllerUnpluggedTitle";
			data.pMessageText = "#L4D360UI_MsgBx_ControllerUnplugged";
			data.bOkButtonEnabled = true;

			confirmation->SetUsageData(data);
		}
	}
	else if ( !Q_stricmp( "OnMatchPlayerMgrReset", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );
		bool bShowDisconnectedMsgBox = true;
		if ( !Q_stricmp( szReason, "GuestSignedIn" ) )
		{
			char const *szDestroyedSessionState = pEvent->GetString( "settings/game/state", "lobby" );
			if ( !Q_stricmp( "lobby", szDestroyedSessionState ) )
				bShowDisconnectedMsgBox = false;
		}

		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the attract screen
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		GenericConfirmation* confirmation = bShowDisconnectedMsgBox ? static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) ) : NULL;
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL );

		if ( confirmation )
		{
			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MsgBx_SignInChangeC";
			data.pMessageText = "#L4D360UI_MsgBx_SignInChange";
			data.bOkButtonEnabled = true;

			if ( !Q_stricmp( szReason, "GuestSignedIn" ) )
			{
				data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";	// "Disconnect"
				data.pMessageText = "#L4D360UI_MsgBx_SignInChange";				// "Sign-in change has occured."
			}

			confirmation->SetUsageData(data);

#ifdef _X360
			// When a confirmation shows up it prevents attract screen from opening, so reset user slots here:
			XBX_ResetUserIdSlots();
			XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
			XBX_SetPrimaryUserIsGuest( 0 );	
			XBX_SetNumGameUsers( 0 ); // users not selected yet
#endif
		}
	}
	else if ( !Q_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );

		if ( char const *szDisconnectHdlr = pEvent->GetString( "disconnecthdlr", NULL ) )
		{
			// If a disconnect handler was set during the event, then we don't interfere with
			// the dialog explaining disconnection, just let the disconnect handler do everything.
			return;
		}

		RemapText_t arrText[] = {
			{ "", "#DisconnectReason_Unknown", RemapText_t::MATCH_FULL },
			{ "Lost connection to LIVE", "#DisconnectReason_LostConnectionToLIVE", RemapText_t::MATCH_FULL },
			{ "Player removed from host session", "#DisconnectReason_PlayerRemovedFromSession", RemapText_t::MATCH_SUBSTR },
			{ "Connection to server timed out", "#L4D360UI_MsgBx_DisconnectedFromServer", RemapText_t::MATCH_SUBSTR },
			{ "Added to banned list", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "Kicked and banned", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "You have been voted off", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "All players idle", "#L4D_ServerShutdownIdle", RemapText_t::MATCH_SUBSTR },
#ifdef _X360
			{ "", "#DisconnectReason_Unknown", RemapText_t::MATCH_START },	// Catch all cases for X360
#endif
			{ NULL, NULL, RemapText_t::MATCH_FULL }
		};

		szReason = RemapText_t::RemapRawText( arrText, szReason );

		//
		// Go back to main menu and display the disconnection reason
		//
		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the main menu
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
		CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";	// "Disconnect"
		data.pMessageText = szReason;
		data.bOkButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !Q_stricmp( "OnEngineEndGame", szEvent ) )
	{
		// If we are connected and there was no session object to handle the event
		if ( !g_pMatchFramework->GetMatchSession() )
		{
			// Issue the disconnect command
			engine->ExecuteClientCmd( "disconnect" );
		}
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !Q_stricmp( "error", pEvent->GetString( "state", "" ) ) )
		{
			g_pMatchFramework->CloseSession();

			char chErrorMsgBuffer[128] = {0};
			char chErrorTitleBuffer[128] = {0};
			char const *szError = pEvent->GetString( "error", "" );
			char const *szErrorTitle = "#L4D360UI_MsgBx_DisconnectedFromSession";

			RemapText_t arrText[] = {
				{ "", "#SessionError_Unknown", RemapText_t::MATCH_FULL },
				{ "n/a", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "create", "#SessionError_Create", RemapText_t::MATCH_FULL },
				{ "createclient", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "connect", "#SessionError_Connect", RemapText_t::MATCH_FULL },
				{ "full", "#SessionError_Full", RemapText_t::MATCH_FULL },
				{ "lock", "#SessionError_Lock", RemapText_t::MATCH_FULL },
				{ "kicked", "#SessionError_Kicked", RemapText_t::MATCH_FULL },
				{ "migrate", "#SessionError_Migrate", RemapText_t::MATCH_FULL },
				{ "nomap", "#SessionError_NoMap", RemapText_t::MATCH_FULL },
				{ "SteamServersDisconnected", "#SessionError_SteamServersDisconnected", RemapText_t::MATCH_FULL },
				{ NULL, NULL, RemapText_t::MATCH_FULL }
			};

			szError = RemapText_t::RemapRawText( arrText, szError );

			if ( !Q_stricmp( "turequired", szError ) )
			{
				// Special case for TU required message
				// If we have a localization string for the TU message then this means that the other box
				// is running and older version of the TU
				char const *szTuRequiredCode = pEvent->GetString( "turequired" );
				CFmtStr strLocKey( "#SessionError_TU_Required_%s", szTuRequiredCode );
				if ( g_pVGuiLocalize->Find( strLocKey ) )
				{
					Q_strncpy( chErrorMsgBuffer, strLocKey, sizeof( chErrorMsgBuffer ) );
					szError = chErrorMsgBuffer;
				}
				else
				{
					szError = "#SessionError_TU_RequiredMessage";
				}
				szErrorTitle = "#SessionError_TU_RequiredTitle";
			}

			// Go to the main menu
			CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

			// Show the message box
			GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
			CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = szErrorTitle;
			data.pMessageText = szError;
			data.bOkButtonEnabled = true;

			if ( !Q_stricmp( "dlcrequired", szError ) )
			{
				// Special case for DLC required message
				uint64 uiDlcRequiredMask = pEvent->GetUint64( "dlcrequired" );
				int iDlcRequired = 0;

				// Find the first DLC in the reported missing mask that is required
				for ( int k = 1; k < sizeof( uiDlcRequiredMask ); ++ k )
				{
					if ( uiDlcRequiredMask & ( 1ull << k ) )
					{
						iDlcRequired = k;
						break;
					}
				}

				CFmtStr strLocKey( "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				if ( !g_pVGuiLocalize->Find( strLocKey ) )
					iDlcRequired = 0;

				// Try to figure out if this DLC is paid/free/unknown
				KeyValues *kvDlcDetails = new KeyValues( "" );
				KeyValues::AutoDelete autodelete_kvDlcDetails( kvDlcDetails );
				if ( !kvDlcDetails->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/dlcdetailsinfo.res", "MOD" ) )
					kvDlcDetails = NULL;

				// Determine the DLC offer ID
				uint64 uiDlcOfferID = 0ull;
				if ( 1 != sscanf( kvDlcDetails->GetString( CFmtStr( "dlc%d/offerid", iDlcRequired ) ), "0x%llx", &uiDlcOfferID ) )
					uiDlcOfferID = 0ull;
				
				// Format the strings
				bool bKicked = !Q_stricmp( pEvent->GetString( "action" ), "kicked" );
				wchar_t const *wszLine1 = g_pVGuiLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", bKicked ? "Kicked" : "Join", iDlcRequired ) );
				wchar_t const *wszLine2 = g_pVGuiLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", uiDlcOfferID ? "Offer" : "Message", iDlcRequired ) );
				
				int numBytesTwoLines = ( Q_wcslen( wszLine1 ) + Q_wcslen( wszLine2 ) + 4 ) * sizeof( wchar_t );
				wchar_t *pwszTwoLines = ( wchar_t * ) stackalloc( numBytesTwoLines );
				Q_snwprintf( pwszTwoLines, numBytesTwoLines, L"%s%s", wszLine1, wszLine2 );
				data.pMessageTextW = pwszTwoLines;
				data.pMessageText = NULL;

				Q_snprintf( chErrorTitleBuffer, sizeof( chErrorTitleBuffer ), "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				data.pWindowTitle = chErrorTitleBuffer;

				if ( uiDlcOfferID )
				{
					data.bCancelButtonEnabled = true;
					data.pfnOkCallback = GoToMarketplaceForOffer;
					
					g_MarketplaceEntryPoint.uiOfferID = uiDlcOfferID;
					g_MarketplaceEntryPoint.dwEntryPoint = kvDlcDetails->GetInt( CFmtStr( "dlc%d/type", iDlcRequired ) );
				}
			}
			
			confirmation->SetUsageData(data);
		}
	}
}



//////////////////////////////////////////////////////////////////////////
//
//
// A bunch of helper KeyValues hierarchy readers
//
//
//////////////////////////////////////////////////////////////////////////


bool GameModeHasDifficulty( char const *szGameMode )
{
	return true;	// all alien swarm game modes have difficulty
	//return !Q_stricmp( szGameMode, "campaign" ) || !Q_stricmp( szGameMode, "single_mission" );
}

char const * GameModeGetDefaultDifficulty( char const *szGameMode )
{
	if ( !GameModeHasDifficulty( szGameMode ) )
		return "normal";

	IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
	if ( !pProfile )
		return "normal";

	UserProfileData const &upd = pProfile->GetPlayerProfileData();
	switch ( upd.difficulty )
	{
	case 1: return "easy";
	case 2: return "hard";
	default: return "normal";
	}
}

bool GameModeHasRoundLimit( char const *szGameMode )
{
	return !Q_stricmp( szGameMode, "scavenge" ) || !Q_stricmp( szGameMode, "teamscavenge" );
}

bool GameModeIsSingleChapter( char const *szGameMode )
{
	return !Q_stricmp( szGameMode, "survival" ) || !Q_stricmp( szGameMode, "scavenge" ) || !Q_stricmp( szGameMode, "teamscavenge" );
}


uint64 GetDlcInstalledMask()
{
	static ConVarRef mm_dlcs_mask_fake( "mm_dlcs_mask_fake" );
	char const *szFakeDlcsString = mm_dlcs_mask_fake.GetString();
	if ( *szFakeDlcsString )
		return atoi( szFakeDlcsString );

	static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	uint64 uiDLCmask = ( unsigned ) mm_dlcs_mask_extras.GetInt();

	bool bSearchPath = false;
	int numDLCs = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPath );

	for ( int j = 0; j < numDLCs; ++ j )
	{
		unsigned int uiDlcHeader = 0;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( j, &uiDlcHeader, NULL, 0 ) )
			continue;

		int idDLC = DLC_LICENSE_ID( uiDlcHeader );
		if ( idDLC < 1 || idDLC >= 31 )
			continue;	// unsupported DLC id

		uiDLCmask |= ( 1ull << idDLC );
	}

	return uiDLCmask;
}


