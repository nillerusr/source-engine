//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Serves as the base panel for the entire matchmaking UI
//
//=============================================================================//

#include "matchmakingbasepanel.h"
#include "welcomedialog.h"
#include "pausedialog.h"
#include "leaderboarddialog.h"
#include "achievementsdialog.h"
#include "sessionoptionsdialog.h"
#include "sessionlobbydialog.h"
#include "sessionbrowserdialog.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/MessageDialog.h"
#include "vgui/ISurface.h"
#include "EngineInterface.h"
#include "game/client/IGameClientExports.h"
#include "GameUI_Interface.h"
#include "engine/imatchmaking.h"
#include "KeyValues.h"
#include "vstdlib/jobthread.h"
#include "BasePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------
// CMatchmakingBasePanel
//--------------------------------
CMatchmakingBasePanel::CMatchmakingBasePanel( vgui::Panel *pParent ) : BaseClass( pParent, "MatchmakingBasePanel" )
{
	SetDeleteSelfOnClose( true );
	SetPaintBackgroundEnabled( false );

	vgui::scheme()->LoadSchemeFromFile( "Resource/ClientScheme.res", "ClientScheme" );
	SetScheme( "ClientScheme" );

	m_pFooter = new CFooterPanel( this, "MatchmakingFooterPanel" );

	m_nGameType = GAMETYPE_STANDARD_MATCH;
}

CMatchmakingBasePanel::~CMatchmakingBasePanel()
{
	if ( m_pFooter )
	{
		delete m_pFooter;
		m_pFooter = NULL;
	}
}

void CMatchmakingBasePanel::SetFooterButtons( CBaseDialog *pOwner, KeyValues *pKeyValues, int nButtonGap /* = -1 */ )
{
	// Don't lay out the buttons if the dialog is not at the top of the stack
	if ( m_DialogStack.Count() )
	{
		CBaseDialog *pDlg = m_DialogStack.Top();
		if ( pDlg != pOwner )
			return;
	}

	if ( m_pFooter )
	{
		m_pFooter->ClearButtons();

		if ( pKeyValues )
		{
			for ( KeyValues *pButton = pKeyValues->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey() )
			{
				if ( !Q_stricmp( pButton->GetName(), "button" ) )
				{
					// Add a button to the footer
					const char *pText = pButton->GetString( "text", NULL );
					const char *pIcon = pButton->GetString( "icon", NULL );

					if ( pText && pIcon )
					{
						m_pFooter->AddNewButtonLabel( pText, pIcon );
					}
				}
			}
		}
		else
		{
			// no data was passed so just setup the standard footer buttons
			m_pFooter->SetStandardDialogButtons();
		}

		if ( nButtonGap > 0 )
		{
			m_pFooter->SetButtonGap( nButtonGap );
		}
		else
		{
			m_pFooter->UseDefaultButtonGap();
		}
	}
}

void CMatchmakingBasePanel::ShowFooter( bool bShown )
{
	m_pFooter->SetVisible( bShown );
}

void CMatchmakingBasePanel::SetFooterButtonVisible( const char *pszText, bool bVisible )
{
	if ( m_pFooter )
	{
		m_pFooter->ShowButtonLabel( pszText, bVisible );
	}
}

void CMatchmakingBasePanel::Activate( void )
{
	BaseClass::Activate();

	// Close animation may have set this to zero
	SetAlpha( 255 );

	if ( !GameUI().IsInLevel() )
	{
		OnOpenWelcomeDialog();
	}
	else
	{
		OnOpenPauseDialog();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle commands from all matchmaking dialogs
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( "OpenWelcomeDialog", pCommand ) )
	{
		OnOpenWelcomeDialog();
	}
	if ( !Q_stricmp( "OpenPauseDialog", pCommand ) )
	{
		OnOpenPauseDialog();
	}
	if ( !Q_stricmp( "OpenRankingsDialog", pCommand ) )
	{
		OnOpenRankingsDialog();
	}
	else if ( !Q_stricmp( "OpenSystemLinkDialog", pCommand ) )
	{
		OnOpenSystemLinkDialog();
	}
	else if ( !Q_stricmp( "OpenPlayerMatchDialog", pCommand ) )
	{
		OnOpenPlayerMatchDialog();
	}
	else if ( !Q_stricmp( "OpenRankedMatchDialog", pCommand ) )
	{
		OnOpenRankedMatchDialog();
	}
    else if ( !Q_stricmp( "OpenAchievementsDialog", pCommand ) )
    {
        OnOpenAchievementsDialog();
    }

    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] Specific code for CS Achievements Display
    //=============================================================================

    else if ( !Q_stricmp( "OpenCSAchievementsDialog", pCommand ) )
    {
        OnOpenCSAchievementsDialog();
    }

    //=============================================================================
    // HPE_END
    //=============================================================================

	else if ( !Q_stricmp( "LevelLoadingStarted", pCommand ) )
	{
		OnLevelLoadingStarted();
	}
	else if ( !Q_stricmp( "LevelLoadingFinished", pCommand ) )
	{
		OnLevelLoadingFinished();
	}
	else if ( !Q_stricmp( "SessionOptions_Modify", pCommand ) )
	{
		OnOpenSessionOptionsDialog( pCommand );
	}
	else if ( !Q_stricmp( "ModifySession", pCommand ) )
	{
		matchmaking->ModifySession();
	}
	else if ( !Q_stricmp( "ChangeClass", pCommand ) )
	{
		engine->ClientCmd_Unrestricted( "changeclass" );
		OnCommand( "ResumeGame" );
	}
	else if ( !Q_stricmp( "ChangeTeam", pCommand ) )
	{
		engine->ClientCmd_Unrestricted( "changeteam" );
		OnCommand( "ResumeGame" );
	}
	else if ( !Q_stricmp( "ShowMapInfo", pCommand ) )
	{
		engine->ClientCmd_Unrestricted( "showmapinfo" );
		OnCommand( "ResumeGame" );
	}
	else if ( !Q_stricmp( "StartHost", pCommand ) )
	{
		// Show progress dialog
		GameUI().ShowMessageDialog( MD_CREATING_GAME, this );

		// Send the host start command
		matchmaking->StartHost();
	}
	else if ( !Q_stricmp( "StartSystemLinkHost", pCommand ) )
	{
		// Show progress dialog
		GameUI().ShowMessageDialog( MD_CREATING_GAME, this );

		m_nGameType = GAMETYPE_SYSTEMLINK_MATCH;
		matchmaking->StartHost( true );
	}
	else if ( !Q_stricmp( "StartClient", pCommand ) )
	{
		// Show progress dialog
		GameUI().ShowMessageDialog( MD_SEARCHING_FOR_GAMES, this );

		// Tell matchmaking to start a client and search for games
		matchmaking->StartClient( false );
	}
	else if ( !Q_stricmp( "StartSystemLinkClient", pCommand ) )
	{
		// Show progress dialog
		GameUI().ShowMessageDialog( MD_SEARCHING_FOR_GAMES, this );

		// Set the system link flag
		matchmaking->AddSessionProperty( SESSION_FLAG, "SESSION_CREATE_SYSTEMLINK", NULL, NULL );

		// Tell matchmaking to start a client and search for games
		m_nGameType = GAMETYPE_SYSTEMLINK_MATCH;
		matchmaking->StartClient( true );
	}
	else if ( Q_stristr( pCommand, "StartQuickMatchClient_" ) )
	{
		// Show progress dialog
		GameUI().ShowMessageDialog( MD_SEARCHING_FOR_GAMES, this );

		if ( Q_stristr( pCommand, "_Ranked" ) )
		{
			// Set the basic flags
			matchmaking->AddSessionProperty( SESSION_CONTEXT, "CONTEXT_GAME_MODE", "CONTEXT_GAME_MODE_MULTIPLAYER", NULL );
			matchmaking->AddSessionProperty( SESSION_CONTEXT, "CONTEXT_GAME_TYPE", "CONTEXT_GAME_TYPE_RANKED", NULL );
			matchmaking->AddSessionProperty( SESSION_FLAG, "SESSION_CREATE_LIVE_MULTIPLAYER_RANKED", NULL, NULL );
			m_nGameType = GAMETYPE_RANKED_MATCH;
		}
		else
		{
			// Set the standard match flag
			matchmaking->AddSessionProperty( SESSION_CONTEXT, "CONTEXT_GAME_MODE", "CONTEXT_GAME_MODE_MULTIPLAYER", NULL );
			matchmaking->AddSessionProperty( SESSION_CONTEXT, "CONTEXT_GAME_TYPE", "CONTEXT_GAME_TYPE_STANDARD", NULL );
			matchmaking->AddSessionProperty( SESSION_FLAG, "SESSION_CREATE_LIVE_MULTIPLAYER_STANDARD", NULL, NULL );
			m_nGameType = GAMETYPE_STANDARD_MATCH;
		}

		// Tell matchmaking to start a client and search for games
		matchmaking->StartClient( false );
	}
	else if ( !Q_stricmp( "StartGame", pCommand ) )
	{
		// Tell matchmaking the host wants to start the game
		matchmaking->StartGame();
	}
	else if ( Q_stristr( pCommand, "LeaderboardDialog_" ) )
	{
		// This covers LeaderboardDialog_[Ranked|Stats]
		OnOpenLeaderboardDialog( pCommand );
	}
	else if ( Q_stristr( pCommand, "SessionOptions_" ) )
	{
		// This covers six command strings: *_Host[Standard|Ranked|Systemlink], *_Client[Standard|Ranked|Systemlink]
		// Each command has a unique options menu - the command string is used as the name of the .res file.
		OnOpenSessionOptionsDialog( pCommand );
	}
	else if ( !Q_stricmp( pCommand, "DialogClosing" ) )
	{
		PopDialog();
	}
	else if ( !Q_stricmp( pCommand, "AchievementsDialogClosing" ) )
	{
		PopDialog();
	}
    else if ( !Q_stricmp( pCommand, "show_achievements_dialog" ) )
    {
        OnOpenAchievementsDialog();
    }

    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] Specific code for CS Achievements Display
    //=============================================================================

    else if ( !Q_stricmp( pCommand, "show_csachievements_dialog" ) )
    {
        OnOpenCSAchievementsDialog();
    }

    //=============================================================================
    // HPE_END
    //=============================================================================

    else if ( !Q_stricmp( pCommand, "ShowSessionOptionsDialog" ) )
	{
		// Need to close the client options dialog and open the host options equivalent
		PopDialog();

		switch( m_nGameType )
		{
		case GAMETYPE_STANDARD_MATCH:
			OnOpenSessionOptionsDialog( "SessionOptions_HostStandard" );
			break;

		case GAMETYPE_RANKED_MATCH:
			OnOpenSessionOptionsDialog( "SessionOptions_HostRanked" );
			break;

		case GAMETYPE_SYSTEMLINK_MATCH:
			OnOpenSessionOptionsDialog( "SessionOptions_SystemLink" );
			break;
		}
	}
	else if ( !Q_stricmp( pCommand, "ReturnToMainMenu" ) )
	{
		CloseAllDialogs();
		Activate();
	}
	else if ( !Q_stricmp( pCommand, "CancelOperation" ) )
	{
		GameUI().CloseMessageDialog();
		PopDialog();
		matchmaking->CancelCurrentOperation();
	}
	else if ( !Q_stricmp( pCommand, "StorageDeviceDenied" ) )
	{
		// Set us as declined
		XBX_SetStorageDeviceId( XBX_STORAGE_DECLINED );
	}
	else
	{
		if ( !Q_stricmp( "ResumeGame", pCommand ) )
		{
			CloseAllDialogs();
		}
 
		CallParentFunction( new KeyValues( "Command", "command", pCommand ) );
	}

	// We should handle the case when user launched the game via invite,
	// was prompted for a storage device and cancelled the picker.
	// In this case whenever any command gets selected from the main menu
	// we should cancel the wait for storage device selection.
	BasePanel()->ValidateStorageDevice( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Handle notifications from matchmaking in the engine.
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::SessionNotification( const int notification, const int param )
{
	switch( notification )
	{
	case SESSION_NOTIFY_FAIL_SEARCH:
		GameUI().CloseMessageDialog();
		GameUI().ShowMessageDialog( MD_SESSION_SEARCH_FAILED, this );
		break;

	case SESSION_NOTIFY_CONNECT_NOTAVAILABLE:
		CloseAllDialogs();
		GameUI().ShowMessageDialog( MD_SESSION_CONNECT_NOTAVAILABLE, this );
		break;

	case SESSION_NOTIFY_CONNECT_SESSIONFULL:
		CloseAllDialogs();
		GameUI().ShowMessageDialog( MD_SESSION_CONNECT_SESSIONFULL, this );
		break;

	case SESSION_NOTIFY_CONNECT_FAILED:
		CloseAllDialogs();
		GameUI().ShowMessageDialog( MD_SESSION_CONNECT_FAILED, this );
		break;

	case SESSION_NOTIFY_FAIL_CREATE:
		CloseAllDialogs();
		GameUI().ShowMessageDialog( MD_SESSION_CREATE_FAILED, this );
		break;

	case SESSION_NOTIFY_CLIENT_KICKED:
		CloseAllDialogs();
		GameUI().ShowMessageDialog( MD_CLIENT_KICKED, this );
		break;

	case SESSION_NOTIFY_LOST_HOST:
		CloseBaseDialogs();
		GameUI().ShowMessageDialog( MD_LOST_HOST, this );
		break;

	case SESSION_NOTIFY_LOST_SERVER:
		CloseBaseDialogs();
		GameUI().ShowMessageDialog( MD_LOST_SERVER, this );
		break;

	case SESSION_NOFIFY_MODIFYING_SESSION:
		GameUI().ShowMessageDialog( MD_MODIFYING_SESSION, this );
		break;

	case SESSION_NOTIFY_SEARCH_COMPLETED:
		GameUI().CloseMessageDialog();

		LoadSessionProperties();

		// Switch to the session browser
		switch( m_nGameType )
		{
		case GAMETYPE_STANDARD_MATCH:
		case GAMETYPE_RANKED_MATCH:
			OnOpenSessionBrowserDialog( "SessionBrowser_Live" );
			break;

		case GAMETYPE_SYSTEMLINK_MATCH:
			OnOpenSessionBrowserDialog( "SessionBrowser_SystemLink" );
			break;
		}
		break;

	case SESSION_NOTIFY_CREATED_HOST:
	case SESSION_NOTIFY_MODIFYING_COMPLETED_HOST:
		GameUI().CloseMessageDialog();

		LoadSessionProperties();

		// Switch to the Lobby
		switch( m_nGameType )
		{
		case GAMETYPE_STANDARD_MATCH:
		case GAMETYPE_RANKED_MATCH:
		case GAMETYPE_SYSTEMLINK_MATCH:
			OnOpenSessionLobbyDialog( "SessionLobby_Host" );
			break;
		}
		break;

	case SESSION_NOTIFY_CREATED_CLIENT:
		GameUI().ShowMessageDialog( MD_SESSION_CONNECTING, this );
		break;

	case SESSION_NOTIFY_CONNECTED_TOSESSION:
	case SESSION_NOTIFY_MODIFYING_COMPLETED_CLIENT:
		GameUI().CloseMessageDialog();

		LoadSessionProperties();

		// Switch to the Lobby
		switch( m_nGameType )
		{
		case GAMETYPE_STANDARD_MATCH:
		case GAMETYPE_RANKED_MATCH:
		case GAMETYPE_SYSTEMLINK_MATCH:
			OnOpenSessionLobbyDialog( "SessionLobby_Client" );
			break;
		}
		break;

	case SESSION_NOTIFY_CONNECTED_TOSERVER:
		CloseAllDialogs( false );
		break;

	case SESSION_NOTIFY_ENDGAME_RANKED:
		// Return to the main menu
		CloseAllDialogs();
		break;

	case SESSION_NOTIFY_ENDGAME_HOST:
		CloseBaseDialogs();
		OnOpenSessionLobbyDialog( "SessionLobby_Host" );
		break;

	case SESSION_NOTIFY_ENDGAME_CLIENT:
		CloseBaseDialogs();
		OnOpenSessionLobbyDialog( "SessionLobby_Client" );
		break;

	case SESSION_NOTIFY_COUNTDOWN:
		{
			CSessionLobbyDialog *pDlg = (CSessionLobbyDialog*)m_hSessionLobbyDialog.Get();
			if ( pDlg )
			{
				pDlg->UpdateCountdown( param );
			}

			if ( param == 0 )
			{
				BasePanel()->RunAnimationWithCallback( this, "CloseMatchmakingUI", new KeyValues( "LoadMap" ) );
			}
		}
		break;

	case SESSION_NOTIFY_DUMPSTATS:
		Msg( "[MM] %d open dialogs\n", m_DialogStack.Count() );
		for ( int i = 0; i < m_DialogStack.Count(); ++i )
		{
			const char *pString = "NULL";
			bool bVisible = false;
			float fAlpha = 0.f;
			CBaseDialog *pDlg = m_DialogStack[i];
			if ( pDlg )
			{
				pString = pDlg->GetName();
				bVisible = pDlg->IsVisible();
				fAlpha = pDlg->GetAlpha();
			}
			const char *pVisible = bVisible ? "YES" : "NO";
			Msg( "[MM] Dialog %d: %s, Visible %s, Alpha %f\n", i, pString, pVisible, fAlpha );
		}
		break;

	case SESSION_NOTIFY_WELCOME:
		CloseGameDialogs( false );
		Activate();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: System Notification
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::SystemNotification( const int notification )
{
	switch( notification )
	{
	case SYSTEMNOTIFY_USER_SIGNEDOUT:
		// See if this was us
#if defined( _X360 )
		uint state = XUserGetSigninState( XBX_GetPrimaryUserId() );
		if ( state == eXUserSigninState_NotSignedIn )
		{
			matchmaking->KickPlayerFromSession( 0 );
			CloseAllDialogs();
		}
		else if ( state != eXUserSigninState_SignedInToLive )
		{
			// User was signed out of live
			if ( m_bPlayingOnline )
			{
				matchmaking->KickPlayerFromSession( 0 );
				CloseAllDialogs();
			}
		}
#endif
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check whether a player meets the signin requirements for a multiplayer game
//-----------------------------------------------------------------------------
bool CMatchmakingBasePanel::ValidateSigninAndStorage( bool bOnlineRequired, const char *pIssuingCommand )
{
	// Check the signin state of the primary user
	bool bSignedIn = false;
	bool bOnlineEnabled = false;
	bool bOnlineSignedIn = false;

#if defined( _X360 )
	int userIdx = XBX_GetPrimaryUserId();
	if ( userIdx != INVALID_USER_ID )
	{
		XUSER_SIGNIN_INFO info;
		uint ret = XUserGetSigninInfo( userIdx, 0, &info );
		if ( ret == ERROR_SUCCESS )
		{
			bSignedIn = true;
			if ( info.dwInfoFlags & XUSER_INFO_FLAG_LIVE_ENABLED )
			{
				bOnlineEnabled = true;
				uint state = XUserGetSigninState( XBX_GetPrimaryUserId() );
				if ( state == eXUserSigninState_SignedInToLive )
				{
					bOnlineSignedIn = true;

					// Check privileges
					BOOL bPrivCheck = false;
					DWORD dwPrivCheck = XUserCheckPrivilege( userIdx, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bPrivCheck );
					if ( ERROR_SUCCESS != dwPrivCheck ||
						 !bPrivCheck )
					{
						bOnlineEnabled = false;
					}
				}
			}
		}
	}
#endif

	if ( bOnlineRequired && !bOnlineEnabled )
	{
		// Player must sign in an online account 
		GameUI().ShowMessageDialog( MD_NOT_ONLINE_ENABLED );
		return false;
	}
	else if ( bOnlineRequired && !bOnlineSignedIn )
	{
		// Player's live account isn't signed in to live 
		GameUI().ShowMessageDialog( MD_NOT_ONLINE_SIGNEDIN );
		return false;
	}
	else if ( !bSignedIn )
	{
		// Eat the input and make the user sign in
		xboxsystem->ShowSigninUI( 1, 0 ); // One user, no special flags
		return false;
	}

	// Handle the storage device selection
	if ( !BasePanel()->HandleStorageDeviceRequest( pIssuingCommand ) )
		return false;

	// If we succeeded, clear the command out
	BasePanel()->ClearPostPromptCommand( pIssuingCommand );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update player information in the lobby
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost )
{
	CSessionLobbyDialog *pLobby = dynamic_cast< CSessionLobbyDialog* >( m_hSessionLobbyDialog.Get() );
	if ( pLobby )
	{
		pLobby->UpdatePlayerInfo( nPlayerId, pName, nTeam, cVoiceState, nPlayersNeeded, bHost );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a search result to the browser dialog
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping )
{
	CSessionBrowserDialog *pBrowser = dynamic_cast< CSessionBrowserDialog* >( m_hSessionBrowserDialog.Get() );
	if ( pBrowser )
	{
		pBrowser->SessionSearchResult( searchIdx, pHostData, pResult, ping );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pre level load ops
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::OnLevelLoadingStarted()
{
}

//-----------------------------------------------------------------------------
// Purpose: Post level load ops
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::OnLevelLoadingFinished()
{
}

//-----------------------------------------------------------------------------
// Purpose: Hide the current dialog, add a new one to the stack and activate it.
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::PushDialog( vgui::DHANDLE< CBaseDialog > &hDialog )
{
	if ( m_DialogStack.Count() )
	{
		if ( m_DialogStack.Top() )
		{
			m_DialogStack.Top()->Close();
		}
		else
		{
			m_DialogStack.Pop();
		}
	}
	hDialog->Activate();
	m_DialogStack.Push( hDialog );
}

//-----------------------------------------------------------------------------
// Purpose: Close the current dialog, pop it from the top of the stack, and activate the next one.
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::PopDialog( bool bActivateNext )
{
	if ( m_DialogStack.Count() > 1 )
	{
		if ( m_DialogStack.Top() )
		{
			m_DialogStack.Top()->SetDeleteSelfOnClose( true );
			m_DialogStack.Top()->Close();
			m_DialogStack.Pop();
		}

		// Drop down to the next available dialog
		while ( m_DialogStack.Count() && !m_DialogStack.Top() )
		{
			m_DialogStack.Pop();
		}

		if ( bActivateNext && m_DialogStack.Count() && m_DialogStack.Top() )
		{
			m_DialogStack.Top()->Activate();
		}
	}

	if ( m_DialogStack.Count() <= 1 )
	{
		// Back at the welcome menu
		m_bPlayingOnline = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close all open dialogs down to the main menu
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::CloseGameDialogs( bool bActivateNext )
{
	CloseBaseDialogs();
	while ( m_DialogStack.Count() > 1 )
	{
		PopDialog( bActivateNext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close all open dialogs down to the main menu
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::CloseAllDialogs( bool bActivateNext )
{
	GameUI().CloseMessageDialog();
	CloseGameDialogs( bActivateNext );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::CloseBaseDialogs( void )
{
	if ( BasePanel() )
	{
		BasePanel()->CloseBaseDialogs();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get session property keyvalues from base panel and matchmaking
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::LoadSessionProperties()
{
	// Grab the session property keys from XboxDialogs.res and from matchmaking
	m_pSessionKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "PropertyDisplayKeys" );
	if ( m_pSessionKeys )
	{
		m_pSessionKeys->ChainKeyValue( matchmaking->GetSessionProperties() );	
	}

	// Cache off the map name
	const char *pDiskName = NULL;
	KeyValues *pName = m_pSessionKeys->FindKey( "MapDiskNames" );
	if ( pName )
	{
		KeyValues *pScenario = m_pSessionKeys->FindKey( "CONTEXT_SCENARIO" );
		if ( pScenario )
		{
			pDiskName = pName->GetString( pScenario->GetString( "displaystring" ), NULL );
		}
	}
	
	if ( pDiskName )
	{
		Q_strncpy( m_szMapLoadName, pDiskName, sizeof( m_szMapLoadName ) ); 
		Msg( "Storing mapname %s\n", m_szMapLoadName );
		if ( Q_strlen( m_szMapLoadName ) < 5 )
		{
			Warning( "Bad map name!\n" );
		}
	}
	else
	{
		// X360TBD: Generate a create error
	}
}

//-----------------------------------------------------------------------------
// Purpose: Open dialog functions.
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::OnOpenWelcomeDialog()
{
	if ( !m_hWelcomeDialog.Get() )
	{
		m_hWelcomeDialog = new CWelcomeDialog( this );
		m_DialogStack.Push( m_hWelcomeDialog );
	}

	m_hWelcomeDialog->Activate();
	m_bPlayingOnline = false;
}

void CMatchmakingBasePanel::OnOpenPauseDialog()
{
	if ( !m_hPauseDialog.Get() )
	{
		m_hPauseDialog = new CPauseDialog( this );
	}
	PushDialog( m_hPauseDialog );
}

void CMatchmakingBasePanel::OnOpenRankingsDialog()
{
	if ( !ValidateSigninAndStorage( true, "OpenRankingDialog" ) )
		return;

	if ( !m_hRankingsDialog.Get() )
	{
		m_hRankingsDialog = new CBaseDialog( this, "RankingsDialog" );
	}
	PushDialog( m_hRankingsDialog );
}

void CMatchmakingBasePanel::OnOpenSystemLinkDialog()
{
	if ( !ValidateSigninAndStorage( false, "OpenSystemLinkDialog" ) )
		return;

	if ( !m_hSystemLinkDialog.Get() )
	{
		m_hSystemLinkDialog = new CBaseDialog( this, "SystemLinkDialog" );
	}
	PushDialog( m_hSystemLinkDialog );
}

void CMatchmakingBasePanel::OnOpenPlayerMatchDialog()
{
	if ( !ValidateSigninAndStorage( true, "OpenPlayerMatchDialog" ) )
		return;

	if ( !m_hPlayerMatchDialog.Get() )
	{
		m_hPlayerMatchDialog = new CBaseDialog( this, "PlayerMatchDialog" );
	}
	PushDialog( m_hPlayerMatchDialog );
	m_bPlayingOnline = true;
}

void CMatchmakingBasePanel::OnOpenRankedMatchDialog()
{
	if ( !ValidateSigninAndStorage( true, "OpenRankedMatchDialog" ) )
		return;

	if ( !m_hRankedMatchDialog.Get() )
	{
		m_hRankedMatchDialog = new CBaseDialog( this, "RankedMatchDialog" );
	}
	PushDialog( m_hRankedMatchDialog );
	m_bPlayingOnline = true;
}

void CMatchmakingBasePanel::OnOpenAchievementsDialog()
{
	if ( !ValidateSigninAndStorage( false, "OpenAchievementsDialog" ) )
		return;

	if ( !m_hAchievementsDialog.Get() )
	{
		m_hAchievementsDialog = new CAchievementsDialog_XBox( this );
	}
	PushDialog( m_hAchievementsDialog );
}

//=============================================================================
// HPE_BEGIN:
// [dwenger] Specific code for CS Achievements Display
//=============================================================================

void CMatchmakingBasePanel::OnOpenCSAchievementsDialog()
{
    if ( !ValidateSigninAndStorage( false, "OpenCSAchievementsDialog" ) )
        return;

    if ( !m_hAchievementsDialog.Get() )
    {
        // $TODO(HPE):  m_hAchievementsDialog = new CAchievementsDialog_XBox( this );
    }
    PushDialog( m_hAchievementsDialog );
}

//=============================================================================
// HPE_END
//=============================================================================

void CMatchmakingBasePanel::OnOpenSessionOptionsDialog( const char *pResourceName )
{
	if ( !m_hSessionOptionsDialog.Get() )
	{
		m_hSessionOptionsDialog = new CSessionOptionsDialog( this );
	}

	if ( Q_stristr( pResourceName, "Ranked" ) )
	{
		m_nGameType = GAMETYPE_RANKED_MATCH;
	}
	else if ( Q_stristr( pResourceName, "Standard" ) )
	{
		m_nGameType = GAMETYPE_STANDARD_MATCH;
	}
	else if ( Q_stristr( pResourceName, "SystemLink" ) )
	{
		m_nGameType = GAMETYPE_SYSTEMLINK_MATCH;
	}

	LoadSessionProperties();

	CSessionOptionsDialog* pDlg = ((CSessionOptionsDialog*)m_hSessionOptionsDialog.Get());
	pDlg->SetGameType( pResourceName );
	pDlg->SetDialogKeys( m_pSessionKeys );

	PushDialog( m_hSessionOptionsDialog );
}

void CMatchmakingBasePanel::OnOpenSessionLobbyDialog( const char *pResourceName )
{
	if ( !m_hSessionLobbyDialog.Get() )
	{	
		m_hSessionLobbyDialog = new CSessionLobbyDialog( this );
	}
	CSessionLobbyDialog *pDlg = (CSessionLobbyDialog*)m_hSessionLobbyDialog.Get();
	pDlg->SetDialogKeys( m_pSessionKeys );

	m_hSessionLobbyDialog->SetName( pResourceName );
	PushDialog( m_hSessionLobbyDialog );
}

void CMatchmakingBasePanel::OnOpenSessionBrowserDialog( const char *pResourceName )
{
	if ( !m_hSessionBrowserDialog.Get() )
	{
		m_hSessionBrowserDialog = new CSessionBrowserDialog( this, m_pSessionKeys );
		m_hSessionBrowserDialog->SetName( pResourceName );

		// Matchmaking will start adding results immediately, so prepare the dialog
		SETUP_PANEL( m_hSessionBrowserDialog.Get() );
	}
	PushDialog( m_hSessionBrowserDialog );
}

void CMatchmakingBasePanel::OnOpenLeaderboardDialog( const char *pResourceName )
{
	if ( !m_hLeaderboardDialog.Get() )
	{
		m_hLeaderboardDialog = new CLeaderboardDialog( this );
		m_hLeaderboardDialog->SetName( pResourceName );
		SETUP_PANEL( m_hLeaderboardDialog.Get() );
	}
	PushDialog( m_hLeaderboardDialog );
	m_hLeaderboardDialog->OnCommand( "CenterOnPlayer" );
}

//-----------------------------------------------------------------------------
// Purpose: Callback function to start map load after ui fades out.
//-----------------------------------------------------------------------------
void CMatchmakingBasePanel::LoadMap( const char *mapname )
{
	CloseAllDialogs( false );

	char cmd[MAX_PATH];
	Q_snprintf( cmd, sizeof( cmd ), "map %s", m_szMapLoadName );
	BasePanel()->FadeToBlackAndRunEngineCommand( cmd );
}

//-------------------------------------------------------
// Keyboard input
//-------------------------------------------------------
void CMatchmakingBasePanel::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_XBUTTON_B:
		// Can't close the matchmaking base panel
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}
