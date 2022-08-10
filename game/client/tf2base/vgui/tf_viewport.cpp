//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#pragma warning( disable : 4800  )  // disable forcing int to bool performance warning

// VGUI panel includes
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>
#include <vgui/Cursor.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/VGUI.h>

// client dll/engine defines
#include "hud.h"
#include <voice_status.h>

// viewport definitions
#include <baseviewport.h>
#include "tf_viewport.h"
#include "tf_teammenu.h"

#include "vguicenterprint.h"
#include "text_message.h"
#include "tf_classmenu.h"

#include "tf_textwindow.h"
#include "tf_clientscoreboard.h"
#include "tf_spectatorgui.h"
#include "intromenu.h"
#include "tf_intromenu.h"

#include "tf_controls.h"
#include "tf_mapinfomenu.h"
#include "tf_roundinfo.h"

//#include "tf_overview.h"

/*
CON_COMMAND( spec_help, "Show spectator help screen")
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_INFO, true );
}

CON_COMMAND( spec_menu, "Activates spectator menu")
{
	bool bShowIt = true;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer && !pPlayer->IsObserver() )
		return;

	if ( args.ArgC() == 2 )
	{
		bShowIt = atoi( args[ 1 ] ) == 1;
	}

	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_SPECMENU, bShowIt );
}
*/

CON_COMMAND( showmapinfo, "Show map info panel" )
{
	if ( !gViewPortInterface )
		return;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	// don't let the player open the team menu themselves until they're a spectator or they're on a regular team and have picked a class
	if ( pPlayer )
	{
		if ( ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR ) || 
		     ( ( pPlayer->GetTeamNumber() != TEAM_UNASSIGNED ) && ( pPlayer->GetPlayerClass()->GetClassIndex() != TF_CLASS_UNDEFINED ) ) )
		{
			// close all the other panels that could be open
			gViewPortInterface->ShowPanel( PANEL_TEAM, false );
			gViewPortInterface->ShowPanel( PANEL_CLASS_RED, false );
			gViewPortInterface->ShowPanel( PANEL_CLASS_BLUE, false );
			gViewPortInterface->ShowPanel( PANEL_INTRO, false );
			gViewPortInterface->ShowPanel( PANEL_ROUNDINFO, false );

			gViewPortInterface->ShowPanel( PANEL_MAPINFO, true );
		}
	}
}

CON_COMMAND( changeteam, "Choose a new team" )
{
	if ( !gViewPortInterface )
		return;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	// don't let the player open the team menu themselves until they're on a team
	if ( pPlayer && ( pPlayer->GetTeamNumber() != TEAM_UNASSIGNED ) )
	{
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
	}
}

CON_COMMAND( changeclass, "Choose a new class" )
{
	if ( !gViewPortInterface )
		return;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pPlayer && pPlayer->CanShowClassMenu() )
	{
		switch( pPlayer->GetTeamNumber() )
		{
		case TF_TEAM_RED:
			gViewPortInterface->ShowPanel( PANEL_CLASS_RED, true );
			break;
		case TF_TEAM_BLUE:
			gViewPortInterface->ShowPanel( PANEL_CLASS_BLUE, true );
			break;
		default:
			break;
		}
	}
}

CON_COMMAND( togglescores, "Toggles score panel")
{
	if ( !gViewPortInterface )
		return;

	IViewPortPanel *scoreboard = gViewPortInterface->FindPanelByName( PANEL_SCOREBOARD );

	if ( !scoreboard )
		return;

	if ( scoreboard->IsVisible() )
	{
		gViewPortInterface->ShowPanel( scoreboard, false );
		GetClientVoiceMgr()->StopSquelchMode();
	}
	else
	{
		gViewPortInterface->ShowPanel( scoreboard, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the VGUI subsystem starts up
//			Creates the sub panels and initialises them
//-----------------------------------------------------------------------------
void TFViewport::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	BaseClass::Start( pGameUIFuncs, pGameEventManager );
}

void TFViewport::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	gHUD.InitColors( pScheme );

	SetPaintBackgroundEnabled( false );

	// Precache some font characters for the 360
 	if ( IsX360() )
 	{
 		wchar_t *pAllChars = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.!:";
 		wchar_t *pNumbers = L"0123456789";
 
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "ScoreboardTeamName" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "ScoreboardMedium" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "ScoreboardSmall" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "ScoreboardVerySmall" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "TFFontMedium" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "TFFontSmall" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "HudFontMedium" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "HudFontMediumSmallSecondary" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "HudFontSmall" ), pAllChars );
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "DefaultSmall" ), pAllChars );
 
 		vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "ScoreboardTeamScore" ), pNumbers );
 	}
}

//
// This is the main function of the viewport. Right here is where we create our class menu, 
// team menu, and anything else that we want to turn on and off in the UI.
//
IViewPortPanel* TFViewport::CreatePanelByName(const char *szPanelName)
{
	IViewPortPanel* newpanel = NULL;

	// overwrite MOD specific panel creation

	if ( Q_strcmp( PANEL_SCOREBOARD, szPanelName ) == 0 )
	{
		newpanel = new CTFClientScoreBoardDialog( this );
	}
	else if ( Q_strcmp( PANEL_SPECGUI, szPanelName ) == 0 )
	{
		newpanel = new CTFSpectatorGUI( this );	
	}
	else if ( Q_strcmp( PANEL_SPECMENU, szPanelName ) == 0 )
	{
//		newpanel = new CTFSpectatorGUI( this );	
	}
	else if ( Q_strcmp( PANEL_OVERVIEW, szPanelName ) == 0 )
	{
//		newpanel = new CTFMapOverview( this );
	}
	else if ( Q_strcmp( PANEL_INFO, szPanelName ) == 0 )
	{
		newpanel = new CTFTextWindow( this );
	}
	else if ( Q_strcmp( PANEL_MAPINFO, szPanelName ) == 0 )
	{
		newpanel = new CTFMapInfoMenu( this );
	}
	else if ( Q_strcmp( PANEL_ROUNDINFO, szPanelName ) == 0 )
	{
		newpanel = new CTFRoundInfo( this );
	}
	else if ( Q_strcmp( PANEL_TEAM, szPanelName ) == 0 )
	{
		newpanel = new CTFTeamMenu( this );	
	}
	else if ( Q_strcmp( PANEL_CLASS_RED, szPanelName ) == 0 )
	{
		newpanel = new CTFClassMenu_Red( this );	
	}
	else if ( Q_strcmp( PANEL_CLASS_BLUE, szPanelName ) == 0 )
	{
		newpanel = new CTFClassMenu_Blue( this );	
	}
	else if ( Q_strcmp( PANEL_INTRO, szPanelName ) == 0 )
	{
		newpanel = new CTFIntroMenu( this );
	}
	else
	{
		// create a generic base panel, don't add twice
		newpanel = BaseClass::CreatePanelByName( szPanelName );
	}

	return newpanel; 
}

void TFViewport::CreateDefaultPanels( void )
{
	AddNewPanel( CreatePanelByName( PANEL_MAPINFO ), "PANEL_MAPINFO" );
	AddNewPanel( CreatePanelByName( PANEL_TEAM ), "PANEL_TEAM" );
	AddNewPanel( CreatePanelByName( PANEL_CLASS_RED ), "PANEL_CLASS_RED" );
	AddNewPanel( CreatePanelByName( PANEL_CLASS_BLUE ), "PANEL_CLASS_BLUE" );
	AddNewPanel( CreatePanelByName( PANEL_INTRO ), "PANEL_INTRO" );
	AddNewPanel( CreatePanelByName( PANEL_ROUNDINFO ), "PANEL_ROUNDINFO" );

	BaseClass::CreateDefaultPanels();
}

int TFViewport::GetDeathMessageStartHeight( void )
{
	int y = YRES(2);

	if ( IsX360() )
	{
		y = YRES(36);
	}

	if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
	{
		y = YRES(2) + g_pSpectatorGUI->GetTopBarHeight();
	}

	return y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFViewport::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	BaseClass::OnScreenSizeChanged( iOldWide, iOldTall );

	// we've changed resolution, let's try to figure out if we need to show any of our menus
	if ( !gViewPortInterface )
		return;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pPlayer )
	{
		// are we on a team yet?
		if ( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
		{
			engine->ClientCmd( "show_motd" );
		}
		else if ( ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR ) && ( pPlayer->m_Shared.GetDesiredPlayerClassIndex() == TF_CLASS_UNDEFINED ) )
		{
			switch( pPlayer->GetTeamNumber() )
			{
			case TF_TEAM_RED:
				gViewPortInterface->ShowPanel( PANEL_CLASS_RED, true );
				break;
			case TF_TEAM_BLUE:
				gViewPortInterface->ShowPanel( PANEL_CLASS_BLUE, true );
				break;
			}
		}
	}
}