//========= Copyright Valve Corporation, All rights reserved. ============//
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

#include "dodteammenu.h"
#include "dodclassmenu.h"
#include "dodclientscoreboard.h"
#include "dodspectatorgui.h"
#include "dodtextwindow.h"
#include "dodmenubackground.h"
#include "dodoverview.h"

#include "IGameUIFuncs.h"

// viewport definitions
#include <baseviewport.h>
#include "dodviewport.h"
#include "vguicenterprint.h"
#include "text_message.h"
#include "c_dod_player.h"


CON_COMMAND_F( changeteam, "Choose a new team", FCVAR_SERVER_CAN_EXECUTE|FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer && pPlayer->CanShowTeamMenu() )
	{
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
	}
}

CON_COMMAND_F( changeclass, "Choose a new class", FCVAR_SERVER_CAN_EXECUTE|FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer && pPlayer->CanShowClassMenu())
	{
		switch( pPlayer->GetTeamNumber() )
		{
		case TEAM_ALLIES:
			gViewPortInterface->ShowPanel( PANEL_CLASS_ALLIES, true );
			break;
		case TEAM_AXIS:
			gViewPortInterface->ShowPanel( PANEL_CLASS_AXIS, true );
			break;
		default:
			break;
		}
	}
}

CON_COMMAND_F( spec_menu, "Activates spectator menu", FCVAR_SERVER_CAN_EXECUTE|FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	bool bShowIt = true;

	if ( args.ArgC() == 2 )
	{
		 bShowIt = atoi( args[ 1 ] ) == 1;
	}

	if ( gViewPortInterface )
	{
		gViewPortInterface->ShowPanel( PANEL_SPECMENU, bShowIt );
	}
}

CON_COMMAND_F( togglescores, "Toggles score panel", FCVAR_SERVER_CAN_EXECUTE|FCVAR_CLIENTCMD_CAN_EXECUTE)
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


void DODViewport::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	gHUD.InitColors( pScheme );

	SetPaintBackgroundEnabled( false );
}


IViewPortPanel* DODViewport::CreatePanelByName(const char *szPanelName)
{
	IViewPortPanel* newpanel = NULL;

	// overwrite MOD specific panel creation
	if ( Q_strcmp(PANEL_TEAM, szPanelName) == 0 )
	{
		newpanel = new CDODTeamMenu( this );
	}
	else if ( Q_strcmp(PANEL_CLASS_ALLIES, szPanelName) == 0 )
	{
		newpanel = new CDODClassMenu_Allies( this );	
	}
	else if ( Q_strcmp(PANEL_CLASS_AXIS, szPanelName) == 0 )
	{
		newpanel = new CDODClassMenu_Axis( this );	
	}
	else if ( Q_strcmp(PANEL_SCOREBOARD, szPanelName) == 0)
	{
		newpanel = new CDODClientScoreBoardDialog( this );
	}
	else if ( Q_strcmp(PANEL_SPECGUI, szPanelName) == 0 )
	{
		newpanel = new CDODSpectatorGUI( this );	
	}
	else if ( Q_strcmp(PANEL_INFO, szPanelName) == 0 )
	{
		newpanel = new CDODTextWindow( this );
	}
	else
	{
		// create a generic base panel, don't add twice
		newpanel = BaseClass::CreatePanelByName( szPanelName );
	}

	return newpanel; 
}

void DODViewport::CreateDefaultPanels( void )
{
	AddNewPanel( CreatePanelByName( PANEL_TEAM ), "PANEL_TEAM" );
	AddNewPanel( CreatePanelByName( PANEL_CLASS_ALLIES ), "PANEL_CLASS_ALLIES" );
	AddNewPanel( CreatePanelByName( PANEL_CLASS_AXIS ), "PANEL_CLASS_AXIS" );

	BaseClass::CreateDefaultPanels();
}

int DODViewport::GetDeathMessageStartHeight( void )
{
	int y = YRES(5);

	if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
	{
		y = g_pSpectatorGUI->GetTopBarHeight() + YRES(5);
	}

	if ( g_pMapOverview && g_pMapOverview->IsVisible() )
	{
		if ( g_pMapOverview->GetMode() == CMapOverview::MAP_MODE_INSET )
		{
			int map_x, map_y, map_w, map_h;
			g_pMapOverview->GetBounds( map_x, map_y, map_w, map_h );

			y = map_y + map_h + YRES(5);
		}
	}

	return y;
}
