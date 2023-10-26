//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "VInGameMainMenu.h"
#include "VGenericConfirmation.h"
#include "VFooterPanel.h"
#include "VFlyoutMenu.h"
#include "VHybridButton.h"
#include "EngineInterface.h"

#include "fmtstr.h"

#include "game/client/IGameClientExports.h"
#include "GameUI_Interface.h"

#include "vgui/ILocalize.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"

#include "materialsystem/materialsystem_config.h"

#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern class IMatchSystem *matchsystem;
extern IVEngineClient *engine;

void Demo_DisableButton( Button *pButton );
void OpenGammaDialog( VPANEL parent );

//=============================================================================
InGameMainMenu::InGameMainMenu( Panel *parent, const char *panelName ):
BaseClass( parent, panelName, false, true )
{
	SetDeleteSelfOnClose(true);

	SetProportional( true );
	SetTitle( "", false );

	SetLowerGarnishEnabled( true );

	SetFooterState();
}

//=============================================================================
static void LeaveGameOkCallback()
{
	COM_TimestampedLog( "Exit Game" );

	InGameMainMenu* self = 
		static_cast< InGameMainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) );

	if ( self )
	{
		self->Close();
	}

	engine->ExecuteClientCmd( "gameui_hide" );

	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		// Closing an active session results in disconnecting from the game.
		g_pMatchFramework->CloseSession();
	}
	else
	{
		// On PC people can be playing via console bypassing matchmaking
		// and required session settings, so to leave game duplicate
		// session closure with an extra "disconnect" command.
		engine->ExecuteClientCmd( "disconnect" );
	}

	engine->ExecuteClientCmd( "gameui_activate" );

	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}

void ShowPlayerList();

//=============================================================================
void InGameMainMenu::OnCommand( const char *command )
{
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling ingame menu command %s from user%d ctrlr%d\n",
			command, iUserSlot, XBX_GetUserId( iUserSlot ) );
	}

	int iOldSlot = GetGameUIActiveSplitScreenPlayerSlot();

	SetGameUIActiveSplitScreenPlayerSlot( iUserSlot );

	GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iUserSlot );

	if ( !Q_strcmp( command, "ReturnToGame" ) )
	{
		engine->ClientCmd("gameui_hide");
	}
	else if ( !Q_strcmp( command, "GoIdle" ) )
	{
		engine->ClientCmd("gameui_hide");
		engine->ClientCmd("go_away_from_keyboard");
	}
	else if (!Q_strcmp(command, "BootPlayer"))
	{
#if defined ( _X360 )
		OnCommand( "ReturnToGame" );
		engine->ClientCmd("togglescores");
#else
		CBaseModPanel::GetSingleton().OpenWindow(WT_INGAMEKICKPLAYERLIST, this, true );
#endif
	}
	else if ( !Q_strcmp(command, "ChangeScenario") && !demo_ui_enable.GetString()[0] )
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_INGAMECHAPTERSELECT, this, true );
	}
	else if ( !Q_strcmp( command, "ChangeChapter" ) && !demo_ui_enable.GetString()[0] )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_INGAMECHAPTERSELECT, this, true );
	}
	else if (!Q_strcmp(command, "ChangeDifficulty"))
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_INGAMEDIFFICULTYSELECT, this, true );
	}
	else if (!Q_strcmp(command, "RestartScenario"))
	{
		engine->ClientCmd("gameui_hide");
		engine->ClientCmd("callvote RestartGame;");
	}
	else if (!Q_strcmp(command, "ReturnToLobby"))
	{
		engine->ClientCmd("gameui_hide");
		engine->ClientCmd("callvote ReturnToLobby;");
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
	else if ( !Q_strcmp( command, "StatsAndAchievements" ) )
	{
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

		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow( WT_ACHIEVEMENTS, this, true );
	}
	else if ( char const *szLeaderboards = StringAfterPrefix( command, "Leaderboards_" ) )
	{
		if ( CheckAndDisplayErrorIfNotLoggedIn() ||
			CUIGameData::Get()->CheckAndDisplayErrorIfOffline( this,
			"#L4D360UI_MainMenu_SurvivalLeaderboards_Tip_Disabled" ) )
			return;

		KeyValues *pSettings = NULL;
		if ( *szLeaderboards )
		{
			pSettings = KeyValues::FromString(
				"settings",
				" game { "
					" mode = "
				" } "
				);
			pSettings->SetString( "game/mode", szLeaderboards );
		}
		else
		{
			pSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
		}
		
		if ( !pSettings )
			return;
		
		KeyValues::AutoDelete autodelete( pSettings );
		
		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow( WT_LEADERBOARD, this, true, pSettings );
	}
	else if (!Q_strcmp(command, "AudioVideo"))
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_AUDIOVIDEO, this, true );
	}
	else if (!Q_strcmp(command, "Controller"))
	{
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
		// Trigger storage device selector
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( XBX_GetUserId( iUserSlot ) ) );
	}
	else if (!Q_strcmp(command, "Audio"))
	{
		// audio options dialog, PC only
		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow(WT_AUDIO, this, true );
	}
	else if (!Q_strcmp(command, "Video"))
	{
		// video options dialog, PC only
		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow(WT_VIDEO, this, true );
	}
	else if (!Q_strcmp(command, "Brightness"))
	{
		// brightness options dialog, PC only
		OpenGammaDialog( GetVParent() );
	}
	else if (!Q_strcmp(command, "KeyboardMouse"))
	{
		// standalone keyboard/mouse dialog, PC only
		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow(WT_KEYBOARDMOUSE, this, true );
	}
	else if( Q_stricmp( "#L4D360UI_Controller_Edit_Keys_Buttons", command ) == 0 )
	{
		FlyoutMenu::CloseActiveMenu();
		CBaseModPanel::GetSingleton().OpenKeyBindingsDialog( this );
	}
	else if (!Q_strcmp(command, "MultiplayerSettings"))
	{
		// standalone multiplayer settings dialog, PC only
		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow(WT_MULTIPLAYER, this, true );
	}
	else if (!Q_strcmp(command, "CloudSettings"))
	{
		// standalone cloud settings dialog, PC only
		m_ActiveControl->NavigateFrom( );
		CBaseModPanel::GetSingleton().OpenWindow(WT_CLOUD, this, true );
	}
	else if ( !Q_strcmp( command, "EnableSplitscreen" ) || !Q_strcmp( command, "DisableSplitscreen" ) )
	{
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_LeaveMultiplayerConf";
		data.pMessageText = "#L4D360UI_MainMenu_SplitscreenChangeConfMsg";

		data.bOkButtonEnabled = true;
		data.pfnOkCallback = &LeaveGameOkCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if( !Q_strcmp( command, "ExitToMainMenu" ) )
	{
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_LeaveMultiplayerConf";
		data.pMessageText = "#L4D360UI_LeaveMultiplayerConfMsg";
		data.bOkButtonEnabled = true;
		data.pfnOkCallback = &LeaveGameOkCallback;
		data.bCancelButtonEnabled = true;

		confirmation->SetUsageData(data);
	}
	else if( !Q_strcmp( command, "Addons" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_ADDONS, this, true );
	}
	else
	{
		const char *pchCommand = command;
#ifdef _X360
		{
			if ( !Q_strcmp(command, "FlmOptionsFlyout") )
			{
				if ( XBX_GetPrimaryUserIsGuest() )
				{
					pchCommand = "FlmOptionsGuestFlyout";
				}
			}
		}
#endif

		if ( !Q_strcmp( command, "FlmVoteFlyout" ) )
		{
			if ( gpGlobals->maxClients <= 1 )
			{
				engine->ClientCmd("asw_restart_mission");
			}
			else
			{
				ShowPlayerList();
			}
			engine->ClientCmd("gameui_hide");
			return;
			/*
			static ConVarRef mp_gamemode( "mp_gamemode" );
			if ( mp_gamemode.IsValid() )
			{
				char const *szGameMode = mp_gamemode.GetString();
				if ( char const *szNoTeamMode = StringAfterPrefix( szGameMode, "team" ) )
					szGameMode = szNoTeamMode;

				if ( !Q_strcmp( szGameMode, "versus" ) || !Q_strcmp( szGameMode, "scavenge" ) )
				{
					pchCommand = "FlmVoteFlyoutVersus";
				}
				else if ( !Q_strcmp( szGameMode, "survival" ) )
				{
					pchCommand = "FlmVoteFlyoutSurvival";
				}
			}
			*/
		}

		// does this command match a flyout menu?
		BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( pchCommand ) );
		if ( flyout )
		{
			// If so, enumerate the buttons on the menu and find the button that issues this command.
			// (No other way to determine which button got pressed; no notion of "current" button on PC.)
			for ( int iChild = 0; iChild < GetChildCount(); iChild++ )
			{
				BaseModHybridButton *hybrid = dynamic_cast<BaseModHybridButton *>( GetChild( iChild ) );
				if ( hybrid && hybrid->GetCommand() && !Q_strcmp( hybrid->GetCommand()->GetString( "command"), command ) )
				{
#ifdef _X360
					hybrid->NavigateFrom( );
#endif //_X360
					// open the menu next to the button that got clicked
					flyout->OpenMenu( hybrid );
					break;
				}
			}
		}
	}

	SetGameUIActiveSplitScreenPlayerSlot( iOldSlot );
}

//=============================================================================
void InGameMainMenu::OnKeyCodePressed( KeyCode code )
{
	int userId = GetJoystickForCode( code );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_START:
	case KEY_XBUTTON_B:
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
		OnCommand( "ReturnToGame" );
		break;
	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
void InGameMainMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( demo_ui_enable.GetString()[0] )
	{
		LoadControlSettings( CFmtStr( "Resource/UI/BaseModUI/InGameMainMenu_%s.res", demo_ui_enable.GetString() ) );
	}
	else
	{
		LoadControlSettings( "Resource/UI/BaseModUI/InGameMainMenu.res" );
	}

	SetPaintBackgroundEnabled( true );

	SetFooterState();
}

//=============================================================================
void InGameMainMenu::OnOpen()
{
	BaseClass::OnOpen();

	SetFooterState();
}

void InGameMainMenu::OnClose()
{
	Unpause();

	// During shutdown this calls delete this, so Unpause should occur before this call
	BaseClass::OnClose();
}


void InGameMainMenu::OnThink()
{
	int iSlot = GetGameUIActiveSplitScreenPlayerSlot();

	GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iSlot );

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	KeyValues *pGameSettings = pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;
	
	char const *szNetwork = pGameSettings->GetString( "system/network", "offline" );
	char const *szGameMode = pGameSettings->GetString( "game/mode", "campaign" );
	char const *szGameState = pGameSettings->GetString( "game/state", "lobby" );

	bool bCanInvite = !Q_stricmp( "LIVE", szNetwork );
	bool bInFinale = !Q_stricmp( "finale", szGameState );

	if ( bCanInvite && pIMatchSession )
	{
		bCanInvite = !bInFinale;
	}

	SetControlEnabled( "BtnInviteFriends", bCanInvite );
	SetControlEnabled( "BtnLeaderboard", CUIGameData::Get()->SignedInToLive() && !Q_stricmp( szGameMode, "survival" ) );

	{
		BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnOptions" ) );
		if ( button )
		{
			BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );

			if ( flyout )
			{
#ifdef _X360
				bool bIsSplitscreen = ( XBX_GetNumGameUsers() > 1 );
#else
				bool bIsSplitscreen = false;
#endif

				Button *pButton = flyout->FindChildButtonByCommand( "EnableSplitscreen" );
				if ( pButton )
				{
					pButton->SetVisible( !bIsSplitscreen );
#ifdef _X360
					pButton->SetEnabled( !XBX_GetPrimaryUserIsGuest() && Q_strcmp( engine->GetLevelName(), "maps/credits.360.bsp" ) != 0 );
#endif
				}

				pButton = flyout->FindChildButtonByCommand( "DisableSplitscreen" );
				if ( pButton )
				{
					pButton->SetVisible( bIsSplitscreen );
				}
			}
		}
	}

	bool bCanGoIdle = !Q_stricmp( "campaign", szGameMode ) || !Q_stricmp( "single_mission", szGameMode );

	// TODO: determine if player can go idle
#if 0
	if ( bCanGoIdle )
	{
		int iLocalPlayerTeam;
		if ( !GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) || iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() )
		{
			bCanGoIdle = false;
		}
		else
		{
			int iNumAliveHumanPlayersOnTeam = GameClientExports()->GetNumPlayersAliveHumanPlayersOnTeam( iLocalPlayerTeam );

			if ( iNumAliveHumanPlayersOnTeam <= 1 )
			{
				bCanGoIdle = false;
			}
		}
	}
#endif

	SetControlEnabled( "BtnGoIdle", bCanGoIdle );

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

	if ( IsVisible() )
	{
		// Yield to generic wait screen or message box if one of those is present
		WINDOW_TYPE arrYield[] = { WT_GENERICWAITSCREEN, WT_GENERICCONFIRMATION };
		for ( int j = 0; j < ARRAYSIZE( arrYield ); ++ j )
		{
			CBaseModFrame *pYield = CBaseModPanel::GetSingleton().GetWindow( arrYield[j] );
			if ( pYield && pYield->IsVisible() && !pYield->HasFocus() )
			{
				pYield->Activate();
				pYield->RequestFocus();
			}
		}
	}
}

//=============================================================================
void InGameMainMenu::PerformLayout( void )
{
	BaseClass::PerformLayout();

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	KeyValues *pGameSettings = pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;

	char const *szNetwork = pGameSettings->GetString( "system/network", "offline" );

	bool bPlayOffline = !Q_stricmp( "offline", szNetwork );

	bool bInCommentary = engine->IsInCommentaryMode();

	bool bCanInvite = !Q_stricmp( "LIVE", szNetwork );
	SetControlEnabled( "BtnInviteFriends", bCanInvite );

	bool bCanVote = true;

	if ( bInCommentary )
	{
		bCanVote = false;
	}

	if ( gpGlobals->maxClients <= 1 )
	{
		bCanVote = false;
	}

	// Do not allow voting in credits map
	if ( !Q_stricmp( pGameSettings->GetString( "game/campaign" ), "credits" ) )
	{
		bCanVote = false;
	}

	/*	// this block used to restrict IDLE players from starting a vote
	else
	{
		int iSlot = GetGameUIActiveSplitScreenPlayerSlot();

		int iOldSplitSlot = engine->GetActiveSplitScreenPlayerSlot();

		engine->SetActiveSplitScreenPlayerSlot( iSlot );

		int iLocalPlayerTeam;
		if ( GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) )
		{
			if ( iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() &&
				 iLocalPlayerTeam != GameClientExports()->GetTeamId_Infected() )
			{
				bCanVote = false;
			}
		}

		engine->SetActiveSplitScreenPlayerSlot( iOldSplitSlot );
	}*/

	vgui::Button *pVoteButton = dynamic_cast< vgui::Button* >( FindChildByName( "BtnCallAVote" ) );
	if ( pVoteButton )
	{
		if ( bCanVote )
		{
			pVoteButton->SetText( "#L4D360UI_InGameMainMenu_CallAVote" );
		}
		else
		{
			pVoteButton->SetText( "#asw_button_restart_mis" );
		}
		SetControlEnabled( "BtnCallAvote", true );
	}
	//SetControlEnabled( "BtnCallAVote", bCanVote );

	BaseModUI::FlyoutMenu *flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsFlyout" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
	}

	flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmOptionsGuestFlyout" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
	}

	flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmVoteFlyout" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
		
		bool bSinglePlayer = true;

#ifdef _X360
		bSinglePlayer = ( XBX_GetNumGameUsers() == 1 );
#endif

		Button *pButton = flyout->FindChildButtonByCommand( "ReturnToLobby" );
		if ( pButton )
		{
			static CGameUIConVarRef r_sv_hosting_lobby( "sv_hosting_lobby", true );
			bool bEnabled = r_sv_hosting_lobby.IsValid() && r_sv_hosting_lobby.GetBool() &&
				// Don't allow return to lobby if playing local singleplayer (it has no lobby)
				!( bPlayOffline && bSinglePlayer );
			pButton->SetEnabled( bEnabled );
		}

		pButton = flyout->FindChildButtonByCommand( "BootPlayer" );
		if ( pButton )
		{
			// Don't allow kick player in local games (nobody to kick)
			pButton->SetEnabled( !bPlayOffline );
		}
	}

	flyout = dynamic_cast< FlyoutMenu* >( FindChildByName( "FlmVoteFlyoutVersus" ) );
	if ( flyout )
	{
		flyout->SetListener( this );
	}

	BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnReturnToGame" ) );
	if ( button )
	{
		if( m_ActiveControl )
			m_ActiveControl->NavigateFrom();

		button->NavigateTo();
	}
}

void InGameMainMenu::Unpause( void )
{
}

//=============================================================================
void InGameMainMenu::OnNotifyChildFocus( vgui::Panel* child )
{
}

void InGameMainMenu::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	SetFooterState();
}

void InGameMainMenu::OnFlyoutMenuCancelled()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when the GameUI is hidden
//-----------------------------------------------------------------------------
void InGameMainMenu::OnGameUIHidden()
{
	Unpause();
	Close();
}


//=============================================================================
void InGameMainMenu::PaintBackground()
{
	vgui::Panel *pPanel = FindChildByName( "PnlBackground" );
	if ( !pPanel )
		return;

	int x, y, wide, tall;
	pPanel->GetBounds( x, y, wide, tall );
	DrawSmearBackground( x, y, wide, tall );
}

//=============================================================================
void InGameMainMenu::SetFooterState()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
	}
}
