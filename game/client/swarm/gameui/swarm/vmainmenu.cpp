//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VMainMenu.h"
#include "EngineInterface.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "VFlyoutMenu.h"
#include "vGenericConfirmation.h"
#include "VQuickJoin.h"
#include "basemodpanel.h"
#include "UIGameData.h"
#include "VGameSettings.h"
#include "VSteamCloudConfirmation.h"
#include "vaddonassociation.h"

#include "VSignInDialog.h"
#include "VGuiSystemModuleLoader.h"
#include "VAttractScreen.h"
#include "gamemodes.h"

#include "vgui/ILocalize.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Image.h"

#include "steam/isteamremotestorage.h"
#include "materialsystem/materialsystem_config.h"

#include "ienginevgui.h"
#include "basepanel.h"
#include "vgui/ISurface.h"
#include "tier0/icommandline.h"
#include "fmtstr.h"

#include "matchmaking/swarm/imatchext_swarm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
static ConVar connect_lobby( "connect_lobby", "", FCVAR_HIDDEN, "Sets the lobby ID to connect to on start." );
static ConVar ui_old_options_menu( "ui_old_options_menu", "0", FCVAR_HIDDEN, "Brings up the old tabbed options dialog from Keyboard/Mouse when set to 1." );
static ConVar ui_play_online_browser( "ui_play_online_browser",
#if defined( _DEMO ) && !defined( _X360 )
									 "0",
									 FCVAR_NONE,
#else
									 "1",
									 FCVAR_RELEASE,
#endif
									 "Whether play online displays a browser or plain search dialog." );

ConVar asw_show_all_singleplayer_maps( "asw_show_all_singleplayer_maps", "0", FCVAR_NONE, "If set, offline practice option on the main menu will show all maps." );

void Demo_DisableButton( Button *pButton );
void OpenGammaDialog( VPANEL parent );

//=============================================================================
MainMenu::MainMenu( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false, false )
{
	SetProportional( true );
	SetTitle( "", false );
	SetMoveable( false );
	SetSizeable( false );

	SetLowerGarnishEnabled( true );

	AddFrameListener( this );

	m_iQuickJoinHelpText = MMQJHT_NONE;

	SetDeleteSelfOnClose( true );
}

//=============================================================================
MainMenu::~MainMenu()
{
	RemoveFrameListener( this );

}

//=============================================================================
void MainMenu::OnCommand( const char *command )
{
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling main menu command %s from user%d ctrlr%d\n",
			command, iUserSlot, XBX_GetUserId( iUserSlot ) );
	}

	bool bOpeningFlyout = false;

	if ( char const *szQuickMatch = StringAfterPrefix( command, "QuickMatch_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network LIVE "
			" } "
			" game { "
				" mode = "
			" } "
			" options { "
				" action quickmatch "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szQuickMatch );

		// TCR: We need to respect the default difficulty
		if ( GameModeHasDifficulty( szQuickMatch ) )
			pSettings->SetString( "game/difficulty", GameModeGetDefaultDifficulty( szQuickMatch ) );

		g_pMatchFramework->MatchSession( pSettings );
	}
	else if ( char const *szCustomMatch = StringAfterPrefix( command, "CustomMatch_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			 CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network LIVE "
			" } "
			" game { "
				" mode = "
			" } "
			" options { "
				" action custommatch "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szCustomMatch );

		CBaseModPanel::GetSingleton().OpenWindow(
			ui_play_online_browser.GetBool() ? WT_FOUNDPUBLICGAMES : WT_GAMESETTINGS,
			this, true, pSettings );
	}
	else if ( char const *szFriendsMatch = StringAfterPrefix( command, "FriendsMatch_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		if ( StringHasPrefix( szFriendsMatch, "team" ) &&
			CUIGameData::Get()->CheckAndDisplayErrorIfNotSignedInToLive( this ) )
			// Team games require to be signed in to LIVE
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
				" mode = "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szFriendsMatch );

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow( WT_ALLGAMESEARCHRESULTS, this, true, pSettings );
	}	
	else if ( char const *szGroupServer = StringAfterPrefix( command, "GroupServer_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
				// " mode = "
			" } "
			" options { "
				" action groupserver "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		if ( *szGroupServer )
			pSettings->SetString( "game/mode", szGroupServer );

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow( WT_STEAMGROUPSERVERS, this, true, pSettings );
	}
	else if ( char const *szLeaderboards = StringAfterPrefix( command, "Leaderboards_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfOffline( this,
			"#L4D360UI_MainMenu_SurvivalLeaderboards_Tip_Disabled" ) )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
				" mode = "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "game/mode", szLeaderboards );

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow( WT_LEADERBOARD, this, true, pSettings );
	}
	else if( !Q_strcmp( command, "VersusSoftLock" ) )
	{
		OnCommand( "FlmVersusFlyout" );
		return;
	}
	else if ( !Q_strcmp( command, "SurvivalCheck" ) )
	{
		OnCommand( "FlmSurvivalFlyout" );
		return;
	}
	else if ( !Q_strcmp( command, "ScavengeCheck" ) )
	{
		OnCommand( "FlmScavengeFlyout" );
		return;
	}
	else if ( !Q_strcmp( command, "SoloPlay" ) )
	{
		if ( !asw_show_all_singleplayer_maps.GetBool() )
		{
			KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
			" network offline "
			" } "
			" game { "
			" mode single_mission "
			" campaign jacob "
			" mission asi-jac1-landingbay_pract "
			" } "
			);
			KeyValues::AutoDelete autodelete( pSettings );

			pSettings->SetString( "Game/difficulty", GameModeGetDefaultDifficulty( pSettings->GetString( "Game/mode" ) ) );

			g_pMatchFramework->CreateSession( pSettings );

			// Automatically start the credits session, no configuration required
			if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
			{
				pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
			}
		}
		else
		{
			KeyValues *pSettings = KeyValues::FromString(
				"Settings",
				" System { "
				" network offline "
				" } "
				" Game { "
				" mode campaign "
				" campaign jacob "
				" mission asi-jac1-landingbay_01 "
				" } "
				);
			KeyValues::AutoDelete autodelete( pSettings );

			// TCR: We need to respect the default difficulty
			pSettings->SetString( "Game/difficulty", GameModeGetDefaultDifficulty( pSettings->GetString( "Game/mode" ) ) );

			g_pMatchFramework->CreateSession( pSettings );
		}
	}
	else if ( !Q_strcmp( command, "DeveloperCommentary" ) )
	{
#ifdef _X360
		if ( XBX_GetNumGameUsers() > 1 )
		{
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MainMenu_SplitscreenDisableConf";
			data.pMessageText = "#L4D360UI_Extras_Commentary_ss_Msg";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptSplitscreenDisableCallback;
			data.bCancelButtonEnabled = true;

			confirmation->SetUsageData(data);
			return;
		}
#endif
		// Explain the rules of commentary
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#GAMEUI_CommentaryDialogTitle";
		data.pMessageText = "#L4D360UI_Commentary_Explanation";

		data.bOkButtonEnabled = true;
		data.pfnOkCallback = &AcceptCommentaryRulesCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
		NavigateFrom();
	}
	else if ( !Q_strcmp( command, "StatsAndAchievements" ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

#ifdef _X360
		// If 360 make sure that the user is not a guest
		if ( XBX_GetUserIsGuest( CBaseModPanel::GetSingleton().GetLastActiveUserId() ) )
		{
			GenericConfirmation* confirmation = 
				static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );
			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_MsgBx_AchievementsDisabled";
			data.pMessageText = "#L4D360UI_MsgBx_GuestsUnavailableToGuests";
			data.bOkButtonEnabled = true;
			confirmation->SetUsageData(data);

			return;
		}
#endif //_X360
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}

		CBaseModPanel::GetSingleton().OpenWindow( WT_ACHIEVEMENTS, this, true );
	}
	else if ( !Q_strcmp( command, "FlmExtrasFlyoutCheck" ) )
	{
		if ( IsX360() && CUIGameData::Get()->SignedInToLive() )
			OnCommand( "FlmExtrasFlyout_Live" );
		else
			OnCommand( "FlmExtrasFlyout_Simple" );
		return;
	}
	else if ( char const *szInviteType = StringAfterPrefix( command, "InviteUI_" ) )
	{
		if ( IsX360() )
		{
			CUIGameData::Get()->OpenInviteUI( szInviteType );
		}
		else
		{
			CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
	}
	else if (!Q_strcmp(command, "Game"))
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow(WT_GAMEOPTIONS, this, true );
	}
	else if (!Q_strcmp(command, "AudioVideo"))
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow(WT_AUDIOVIDEO, this, true );
	}
	else if (!Q_strcmp(command, "Controller"))
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow(WT_CONTROLLER, this, true );
	}
	else if (!Q_strcmp(command, "Storage"))
	{
#ifdef _X360
		if ( XBX_GetUserIsGuest( iUserSlot ) )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}
#endif
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}

		// Trigger storage device selector
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( XBX_GetUserId( iUserSlot ) ) );
	}
	else if (!Q_strcmp(command, "Credits"))
	{
#ifdef _X360
		if ( XBX_GetNumGameUsers() > 1 )
		{
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MainMenu_SplitscreenDisableConf";
			data.pMessageText = "#L4D360UI_Extras_Credits_ss_Msg";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptSplitscreenDisableCallback;
			data.bCancelButtonEnabled = true;

			confirmation->SetUsageData(data);
			return;
		}
#endif
		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network offline "
			" } "
			" game { "
				" mode single_mission "
			" } "
			" options { "
				" play credits "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		g_pMatchFramework->CreateSession( pSettings );

		// Automatically start the credits session, no configuration required
		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
		{
			pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
		}
	}
	else if (!Q_strcmp(command, "QuitGame"))
	{
		if ( IsPC() )
		{
			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_MainMenu_Quit_Confirm";
			data.pMessageText = "#L4D360UI_MainMenu_Quit_ConfirmMsg";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptQuitGameCallback;
			data.bCancelButtonEnabled = true;

			confirmation->SetUsageData(data);

			NavigateFrom();
		}

		if ( IsX360() )
		{
			engine->ExecuteClientCmd( "demo_exit" );
		}
	}
	else if ( !Q_stricmp( command, "QuitGame_NoConfirm" ) )
	{
		if ( IsPC() )
		{
			engine->ClientCmd( "quit" );
		}
	}
	else if ( !Q_strcmp( command, "EnableSplitscreen" ) )
	{
		Msg( "Enabling splitscreen from main menu...\n" );

		CBaseModPanel::GetSingleton().CloseAllWindows();
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GOSPLITSCREEN );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
	}
	else if ( !Q_strcmp( command, "DisableSplitscreen" ) )
	{
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_MainMenu_SplitscreenDisableConf";
		data.pMessageText = "#L4D360UI_MainMenu_SplitscreenDisableConfMsg";

		data.bOkButtonEnabled = true;
		data.pfnOkCallback = &AcceptSplitscreenDisableCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if ( !Q_strcmp( command, "DisableSplitscreen_NoConfirm" ) )
	{
		Msg( "Disabling splitscreen from main menu...\n" );

		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART  );
		OnCommand( "ActivateAttractScreen" );
	}
	else if (!Q_strcmp(command, "ChangeGamers"))	// guest SIGN-IN command
	{
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GUESTSIGNIN, XBX_GetUserId( iUserSlot ) );
		OnCommand( "ActivateAttractScreen" );
	}
	else if (!Q_strcmp(command, "ActivateAttractScreen"))
	{
		if ( IsX360() )
		{
			Close();
			CBaseModPanel::GetSingleton().CloseAllWindows();
			CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
			CBaseModFrame *pWnd = CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
			if ( pWnd )
			{
				pWnd->PostMessage( pWnd, new KeyValues( "ChangeGamers" ) );
			}
		}		
	}
	else if (!Q_strcmp(command, "Audio"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// audio options dialog, PC only
			if ( m_ActiveControl )
			{
				m_ActiveControl->NavigateFrom( );
			}
			CBaseModPanel::GetSingleton().OpenWindow(WT_AUDIO, this, true );
		}
	}
	else if (!Q_strcmp(command, "Video"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// video options dialog, PC only
			if ( m_ActiveControl )
			{
				m_ActiveControl->NavigateFrom( );
			}
			CBaseModPanel::GetSingleton().OpenWindow(WT_VIDEO, this, true );
		}
	}
	else if (!Q_strcmp(command, "Brightness"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// brightness options dialog, PC only
			OpenGammaDialog( GetVParent() );
		}
	}
	else if (!Q_strcmp(command, "KeyboardMouse"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// standalone keyboard/mouse dialog, PC only
			if ( m_ActiveControl )
			{
				m_ActiveControl->NavigateFrom( );
			}
			CBaseModPanel::GetSingleton().OpenWindow(WT_KEYBOARDMOUSE, this, true );
		}
	}
	else if( Q_stricmp( "#L4D360UI_Controller_Edit_Keys_Buttons", command ) == 0 )
	{
		FlyoutMenu::CloseActiveMenu();
		CBaseModPanel::GetSingleton().OpenKeyBindingsDialog( this );
	}
	else if (!Q_strcmp(command, "MultiplayerSettings"))
	{
		if ( ui_old_options_menu.GetBool() )
		{
			CBaseModPanel::GetSingleton().OpenOptionsDialog( this );
		}
		else
		{
			// standalone multiplayer settings dialog, PC only
			if ( m_ActiveControl )
			{
				m_ActiveControl->NavigateFrom( );
			}
			CBaseModPanel::GetSingleton().OpenWindow(WT_MULTIPLAYER, this, true );
		}
	}
	else if (!Q_strcmp(command, "CloudSettings"))
	{
		// standalone cloud settings dialog, PC only
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow(WT_CLOUD, this, true );
	}
	else if (!Q_strcmp(command, "SeeAll"))
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" game { "
			// passing empty game settings to indicate no preference
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		CBaseModPanel::GetSingleton().OpenWindow( WT_ALLGAMESEARCHRESULTS, this, true, pSettings );
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
	}
	else if ( !Q_strcmp( command, "OpenServerBrowser" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		// on PC, bring up the server browser and switch it to the LAN tab (tab #5)
		engine->ClientCmd( "openserverbrowser" );
	}
	else if ( !Q_strcmp( command, "DemoConnect" ) )
	{
		g_pMatchFramework->GetMatchTitle()->PrepareClientForConnect( NULL );
		engine->ClientCmd( CFmtStr( "connect %s", demo_connect_string.GetString() ) );
	}
	else if (command && command[0] == '#')
	{
		// Pass it straight to the engine as a command
		engine->ClientCmd( command+1 );
	}
	else if( !Q_strcmp( command, "Addons" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_ADDONS, this, true );
	}
	else if( !Q_strcmp( command, "CreateGame" ) )
	{
		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
			" network LIVE "
			" access public "
			" } "
			" game { "
			" mode = "
			" campaign = "
			" mission = "
			" } "
			" options { "
			" action create "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		char const *szGameMode = "campaign";
		pSettings->SetString( "game/mode", szGameMode );
		pSettings->SetString( "game/campaign", "jacob" );
		pSettings->SetString( "game/mission", "asi-jac1-landingbay_01" );

		if ( !CUIGameData::Get()->SignedInToLive() )
		{
			pSettings->SetString( "system/network", "lan" );
			pSettings->SetString( "system/access", "public" );
		}

		if ( StringHasPrefix( szGameMode, "team" ) )
		{
			pSettings->SetString( "system/netflag", "teamlobby" );
		}
// 		else if ( !Q_stricmp( "custommatch", m_pDataSettings->GetString( "options/action", "" ) ) )
// 		{
// 			pSettings->SetString( "system/access", "public" );
// 		}

		// TCR: We need to respect the default difficulty
		pSettings->SetString( "game/difficulty", GameModeGetDefaultDifficulty( szGameMode ) );

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		CBaseModPanel::GetSingleton().CloseAllWindows();
		CBaseModPanel::GetSingleton().OpenWindow( WT_GAMESETTINGS, NULL, true, pSettings );
	}
	else
	{
		const char *pchCommand = command;
		if ( !Q_strcmp(command, "FlmOptionsFlyout") )
		{
#ifdef _X360
			if ( XBX_GetPrimaryUserIsGuest() )
			{
				pchCommand = "FlmOptionsGuestFlyout";
			}
#endif
		}
		else if ( !Q_strcmp(command, "FlmVersusFlyout") )
		{
			command = "VersusSoftLock";
		}
		else if ( !Q_strcmp( command, "FlmSurvivalFlyout" ) )
		{
			command = "SurvivalCheck";
		}
		else if ( !Q_strcmp( command, "FlmScavengeFlyout" ) )
		{
			command = "ScavengeCheck";
		}
		else if ( StringHasPrefix( command, "FlmExtrasFlyout_" ) )
		{
			command = "FlmExtrasFlyoutCheck";
		}

		// does this command match a flyout menu?
		BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( pchCommand ) );
		if ( flyout )
		{
			bOpeningFlyout = true;

			// If so, enumerate the buttons on the menu and find the button that issues this command.
			// (No other way to determine which button got pressed; no notion of "current" button on PC.)
			for ( int iChild = 0; iChild < GetChildCount(); iChild++ )
			{
				bool bFound = false;
				GameModes *pGameModes = dynamic_cast< GameModes *>( GetChild( iChild ) );
				if ( pGameModes )
				{
					for ( int iGameMode = 0; iGameMode < pGameModes->GetNumGameInfos(); iGameMode++ )
					{
						BaseModHybridButton *pHybrid = pGameModes->GetHybridButton( iGameMode );
						if ( pHybrid && pHybrid->GetCommand() && !Q_strcmp( pHybrid->GetCommand()->GetString( "command"), command ) )
						{
							pHybrid->NavigateFrom();
							// open the menu next to the button that got clicked
							flyout->OpenMenu( pHybrid );
							flyout->SetListener( this );
							bFound = true;
							break;
						}
					}
				}

				if ( !bFound )
				{
					BaseModHybridButton *hybrid = dynamic_cast<BaseModHybridButton *>( GetChild( iChild ) );
					if ( hybrid && hybrid->GetCommand() && !Q_strcmp( hybrid->GetCommand()->GetString( "command"), command ) )
					{
						hybrid->NavigateFrom();
						// open the menu next to the button that got clicked
						flyout->OpenMenu( hybrid );
						flyout->SetListener( this );
						break;
					}
				}
			}
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

	if( !bOpeningFlyout )
	{
		FlyoutMenu::CloseActiveMenu(); //due to unpredictability of mouse navigation over keyboard, we should just close any flyouts that may still be open anywhere.
	}
}

//=============================================================================
void MainMenu::OpenMainMenuJoinFailed( const char *msg )
{
	// This is called when accepting an invite or joining friends game fails
	CUIGameData::Get()->OpenWaitScreen( msg );
	CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
}

//=============================================================================
void MainMenu::OnNotifyChildFocus( vgui::Panel* child )
{
}

void MainMenu::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	SetFooterState();
}

void MainMenu::OnFlyoutMenuCancelled()
{
}

//=============================================================================
void MainMenu::OnKeyCodePressed( KeyCode code )
{
	int userId = GetJoystickForCode( code );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// Capture the B key so it doesn't play the cancel sound effect
		break;
	case KEY_XBUTTON_X:
		{
			QuickJoinPanel *pQuickJoin = dynamic_cast<QuickJoinPanel*>( FindChildByName( "PnlQuickJoin" ) );
			if ( pQuickJoin && pQuickJoin->ShouldBeVisible() )
			{
				// X shortcut only works if the panel is showing
				OnCommand( "SeeAll" );
			}

			break;
		}

	case KEY_XBUTTON_BACK:
#ifdef _X360
		if ( XBX_GetNumGameUsers() > 1 )
		{
			OnCommand( "DisableSplitscreen" );
		}
#endif
		break;

	case KEY_XBUTTON_INACTIVE_START:
#ifdef _X360
		if ( !XBX_GetPrimaryUserIsGuest() &&
			 userId != (int) XBX_GetPrimaryUserId() &&
			 userId >= 0 &&
			 CUIGameData::Get()->CanPlayer2Join() )
		{
			// Pass the index of controller which wanted to join splitscreen
			CBaseModPanel::GetSingleton().CloseAllWindows();
			CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GOSPLITSCREEN, userId );
			CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
		}
#endif
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
void MainMenu::OnThink()
{
	// need to change state of flyout if user suddenly disconnects
	// while flyout is open
	BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmCampaignFlyout" ) );
	if ( flyout )
	{
		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( flyout->FindChildButtonByCommand( "QuickMatchCoOp" ) );
		if ( pButton )
		{
			if ( !CUIGameData::Get()->SignedInToLive() )
			{
				pButton->SetText( "#L4D360UI_QuickStart" );
				if ( m_iQuickJoinHelpText != MMQJHT_QUICKSTART )
				{
					pButton->SetHelpText( "#L4D360UI_QuickMatch_Offline_Tip" );
					m_iQuickJoinHelpText = MMQJHT_QUICKSTART;
				}
			}
			else
			{
				pButton->SetText( "#L4D360UI_QuickMatch" );
				if ( m_iQuickJoinHelpText != MMQJHT_QUICKMATCH )
				{
					pButton->SetHelpText( "#L4D360UI_QuickMatch_Tip" );
					m_iQuickJoinHelpText = MMQJHT_QUICKMATCH;
				}
			}
		}
	}

	if ( IsPC() )
	{
		FlyoutMenu *pFlyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
		if ( pFlyout )
		{
			const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
			pFlyout->SetControlEnabled( "BtnBrightness", !config.Windowed() );
		}
	}

	BaseClass::OnThink();
}

//=============================================================================
void MainMenu::OnOpen()
{
	if ( IsPC() && connect_lobby.GetString()[0] )
	{
		// if we were launched with "+connect_lobby <lobbyid>" on the command line, join that lobby immediately
		uint64 nLobbyID = _atoi64( connect_lobby.GetString() );
		if ( nLobbyID != 0 )
		{
			KeyValues *pSettings = KeyValues::FromString(
				"settings",
				" system { "
					" network LIVE "
				" } "
				" options { "
					" action joinsession "
				" } "
				);
			pSettings->SetUint64( "options/sessionid", nLobbyID );
			KeyValues::AutoDelete autodelete( pSettings );

			g_pMatchFramework->MatchSession( pSettings );
		}
		// clear the convar so we don't try to join that lobby every time we return to the main menu
		connect_lobby.SetValue( "" );
	}

	BaseClass::OnOpen();

	SetFooterState();

#ifndef _X360
	bool bSteamCloudVisible = false;

	{
		static CGameUIConVarRef cl_cloud_settings( "cl_cloud_settings" );
		if ( cl_cloud_settings.GetInt() == -1 )
		{
			CBaseModPanel::GetSingleton().OpenWindow( WT_STEAMCLOUDCONFIRM, this, false );
			bSteamCloudVisible = true;
		}
	}

	if ( !bSteamCloudVisible )
	{
		if ( AddonAssociation::CheckAndSeeIfShouldShow() )
		{
			CBaseModPanel::GetSingleton().OpenWindow( WT_ADDONASSOCIATION, this, false );
		}
	}
#endif
}

//=============================================================================
void MainMenu::RunFrame()
{
	BaseClass::RunFrame();
}

//=============================================================================
#ifdef _X360
void MainMenu::Activate()
{
	BaseClass::Activate();
	OnFlyoutMenuClose( NULL );
}
#endif

//=============================================================================
void MainMenu::PaintBackground() 
{
}

void MainMenu::SetFooterState()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		CBaseModFooterPanel::FooterButtons_t buttons = FB_ABUTTON;
#if defined( _X360 )
		if ( XBX_GetPrimaryUserIsGuest() == 0 )
		{
			buttons |= FB_XBUTTON;
		}
#endif

		footer->SetButtons( buttons, FF_MAINMENU, false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_XBUTTON, "#L4D360UI_MainMenu_SeeAll" );
	}
}

//=============================================================================
void MainMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	const char *pSettings = "Resource/UI/BaseModUI/MainMenu.res";

#if !defined( _X360 )
	if ( !g_pMatchFramework->GetMatchSystem() )
	{
		Msg( "BAD!\n" );
	}
	if ( !g_pMatchFramework->GetMatchSystem()->GetPlayerManager() )
	{
		Msg( "BAD PLAYER MANAGER!\n" );
	}
	if ( !g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( 0 ) )
	{
		pSettings = "Resource/UI/BaseModUI/MainMenuStub.res";
	}
#endif

	LoadControlSettings( pSettings );

	BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnPlaySolo" ) );
	if ( button )
	{
#ifdef _X360
		button->SetText( ( XBX_GetNumGameUsers() > 1 ) ? ( "#L4D360UI_MainMenu_PlaySplitscreen" ) : ( "#L4D360UI_MainMenu_PlaySolo" ) );
		button->SetHelpText( ( XBX_GetNumGameUsers() > 1 ) ? ( "#L4D360UI_MainMenu_OfflineCoOp_Tip" ) : ( "#L4D360UI_MainMenu_PlaySolo_Tip" ) );
#endif
	}

#ifdef _X360
	if ( !XBX_GetPrimaryUserIsGuest() )
	{
		wchar_t wszListText[ 128 ];
		wchar_t wszPlayerName[ 128 ];

		IPlayer *player1 = NULL;
		if ( XBX_GetNumGameUsers() > 0 )
		{
			player1 = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( 0 ) );
		}

		IPlayer *player2 = NULL;
		if ( XBX_GetNumGameUsers() > 1 )
		{
			player2 = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( 1 ) );
		}

		if ( player1 )
		{
			Label *pLblPlayer1GamerTag = dynamic_cast< Label* >( FindChildByName( "LblPlayer1GamerTag" ) );
			if ( pLblPlayer1GamerTag )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( player1->GetName(), wszPlayerName, sizeof( wszPlayerName ) );
				g_pVGuiLocalize->ConstructString( wszListText, sizeof( wszListText ), g_pVGuiLocalize->Find( "#L4D360UI_MainMenu_LocalProfilePlayer1" ), 1, wszPlayerName );

				pLblPlayer1GamerTag->SetVisible( true );
				pLblPlayer1GamerTag->SetText( wszListText );
			}
		}

		if ( player2 )
		{
			Label *pLblPlayer2GamerTag = dynamic_cast< Label* >( FindChildByName( "LblPlayer2GamerTag" ) );
			if ( pLblPlayer2GamerTag )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( player2->GetName(), wszPlayerName, sizeof( wszPlayerName ) );
				g_pVGuiLocalize->ConstructString( wszListText, sizeof( wszListText ), g_pVGuiLocalize->Find( "#L4D360UI_MainMenu_LocalProfilePlayer2" ), 1, wszPlayerName );

				pLblPlayer2GamerTag->SetVisible( true );
				pLblPlayer2GamerTag->SetText( wszListText );

				// in split screen, have player2 gamer tag instead of enable, and disable
				SetControlVisible( "LblPlayer2DisableIcon", true );
				SetControlVisible( "LblPlayer2Disable", true );
				SetControlVisible( "LblPlayer2Enable", false );
			}
		}
		else
		{
			SetControlVisible( "LblPlayer2DisableIcon", false );
			SetControlVisible( "LblPlayer2Disable", false );

			// not in split screen, no player2 gamertag, instead have enable
			SetControlVisible( "LblPlayer2GamerTag", false );
			SetControlVisible( "LblPlayer2Enable", true );
		}
	}
#endif

	if ( IsPC() )
	{
		FlyoutMenu *pFlyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
		if ( pFlyout )
		{
			bool bUsesCloud = false;

#ifdef IS_WINDOWS_PC
			ISteamRemoteStorage *pRemoteStorage = SteamClient()?(ISteamRemoteStorage *)SteamClient()->GetISteamGenericInterface(
				SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;
#else
			ISteamRemoteStorage *pRemoteStorage =  NULL; 
			AssertMsg( false, "This branch run on a PC build without IS_WINDOWS_PC defined." );
#endif

			int32 availableBytes, totalBytes = 0;
			if ( pRemoteStorage && pRemoteStorage->GetQuota( &totalBytes, &availableBytes ) )
			{
				if ( totalBytes > 0 )
				{
					bUsesCloud = true;
				}
			}

			pFlyout->SetControlEnabled( "BtnCloud", bUsesCloud );
		}
	}

	SetFooterState();

	if ( IsX360() )
	{		
		GameModes *pGameModes =  dynamic_cast< GameModes* >( FindChildByName( "BtnGameModes" ) );	
		if ( pGameModes )
		{
			char lastActive[MAX_PATH];
			if ( pGameModes->GetLastActiveNameId( lastActive, sizeof( lastActive ) ) )
			{
				pGameModes->SetActive( lastActive, true );
			}
			else
			{
				pGameModes->SetActive( "BtnPlaySolo", true );
			}
			m_ActiveControl = pGameModes;
		}
	}

	if ( IsPC() )
	{
		vgui::Panel *firstPanel = FindChildByName( "BtnCoOp" );
		if ( firstPanel )
		{
			if ( m_ActiveControl )
			{
				m_ActiveControl->NavigateFrom( );
			}
			firstPanel->NavigateTo();
		}
	}

#if defined( _X360 ) && defined( _DEMO )
	SetControlVisible( "BtnExtras", !engine->IsDemoHostedFromShell() );
	SetControlVisible( "BtnQuit", engine->IsDemoHostedFromShell() );
#endif

	// CERT CATCH ALL JUST IN CASE!
#ifdef _X360
	bool bAllUsersCorrectlySignedIn = ( XBX_GetNumGameUsers() > 0 );
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( !g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( k ) ) )
			bAllUsersCorrectlySignedIn = false;
	}
	if ( !bAllUsersCorrectlySignedIn )
	{
		Warning( "======= SIGNIN FAIL SIGNIN FAIL SIGNIN FAIL SIGNIN FAIL ==========\n" );
		Assert( 0 );
		CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );
		CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL, true );
		Warning( "======= SIGNIN RESET SIGNIN RESET SIGNIN RESET SIGNIN RESET ==========\n" );
	}
#endif
}

const char *pDemoDisabledButtons[] = { "BtnVersus", "BtnSurvival", "BtnStatsAndAchievements", "BtnExtras" };

void MainMenu::Demo_DisableButtons( void )
{
	for ( int i = 0; i < ARRAYSIZE( pDemoDisabledButtons ); i++ )
	{
		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( pDemoDisabledButtons[i] ) );

		if ( pButton )
		{
			Demo_DisableButton( pButton );
		}
	}
}

void MainMenu::AcceptCommentaryRulesCallback() 
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network offline "
			" } "
			" game { "
				" mode single_mission "
			" } "
			" options { "
				" play commentary "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		g_pMatchFramework->CreateSession( pSettings );
	}

}

void MainMenu::AcceptSplitscreenDisableCallback()
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		pMainMenu->OnCommand( "DisableSplitscreen_NoConfirm" );
	}
}

void MainMenu::AcceptQuitGameCallback()
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		pMainMenu->OnCommand( "QuitGame_NoConfirm" );
	}
}

void MainMenu::AcceptVersusSoftLockCallback()
{
	if ( MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) ) )
	{
		pMainMenu->OnCommand( "FlmVersusFlyout" );
	}
}


#ifndef _X360
CON_COMMAND_F( openserverbrowser, "Opens server browser", 0 )
{
	bool isSteam = IsPC() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils();
	if ( isSteam )
	{
		// show the server browser
		g_VModuleLoader.ActivateModule("Servers");

		// if an argument was passed, that's the tab index to show, send a message to server browser to switch to that tab
		if ( args.ArgC() > 1 )
		{
			KeyValues *pKV = new KeyValues( "ShowServerBrowserPage" );
			pKV->SetInt( "page", atoi( args[1] ) );
			g_VModuleLoader.PostMessageToAllModules( pKV );
		}

#ifdef INFESTED_DLL
		KeyValues *pSchemeKV = new KeyValues( "SetCustomScheme" );
		pSchemeKV->SetString( "SchemeName", "SwarmServerBrowserScheme" );
		g_VModuleLoader.PostMessageToAllModules( pSchemeKV );
#endif
	}
}
#endif
