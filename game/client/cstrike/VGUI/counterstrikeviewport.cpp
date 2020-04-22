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

// cstrike specific dialogs
#include "cstriketextwindow.h"
#include "cstriketeammenu.h"
#include "cstrikeclassmenu.h"
#include "cstrikebuymenu.h"
#include "cstrikebuyequipmenu.h"
#include "cstrikespectatorgui.h"
#include "cstrikeclientscoreboard.h"
#include "clientmode_csnormal.h"
#include "IGameUIFuncs.h"

// viewport definitions
#include <baseviewport.h>
#include "counterstrikeviewport.h"
#include "cs_gamerules.h"
// #include "c_user_message_register.h"
#include "vguicenterprint.h"
#include "text_message.h"


static void OpenPanelWithCheck( const char *panelToOpen, const char *panelToCheck )
{
	IViewPortPanel *checkPanel = gViewPortInterface->FindPanelByName( panelToCheck );
	if ( !checkPanel || !checkPanel->IsVisible() )
	{
		gViewPortInterface->ShowPanel( panelToOpen, true );
	}
}


CON_COMMAND( buyequip, "Show equipment buy menu" )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if( pPlayer && pPlayer->m_lifeState == LIFE_ALIVE && pPlayer->State_Get() == STATE_ACTIVE )
	{
		if( !pPlayer->IsInBuyZone() )
		{
			internalCenterPrint->Print( "#Cstrike_NotInBuyZone" );
		}
		else if( CSGameRules()->IsBuyTimeElapsed() )
		{
			char strBuyTime[16];
			Q_snprintf( strBuyTime, sizeof( strBuyTime ), "%d", (int)CSGameRules()->GetBuyTimeLength() );
			
			wchar_t buffer[128];
			wchar_t buytime[16];
			g_pVGuiLocalize->ConvertANSIToUnicode( strBuyTime, buytime, sizeof(buytime) );
			g_pVGuiLocalize->ConstructString( buffer, sizeof(buffer), g_pVGuiLocalize->Find("#Cstrike_TitlesTXT_Cant_buy"), 1, buytime );
			internalCenterPrint->Print( buffer );
		}
		else
		{
			if( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				OpenPanelWithCheck( PANEL_BUY_EQUIP_CT, PANEL_BUY_CT );
			}
			else if( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				OpenPanelWithCheck( PANEL_BUY_EQUIP_TER, PANEL_BUY_TER );
			}
		}
	}
}

CON_COMMAND( buymenu, "Show main buy menu" )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if( pPlayer )
	{
		if ( pPlayer->m_lifeState != LIFE_ALIVE && pPlayer->State_Get() != STATE_ACTIVE )
			return;

		if( !pPlayer->IsInBuyZone() )
		{
			internalCenterPrint->Print( "#Cstrike_NotInBuyZone" );
		}
		else if( CSGameRules()->IsBuyTimeElapsed() )
		{
			char strBuyTime[16];
			Q_snprintf( strBuyTime, sizeof( strBuyTime ), "%d", (int)CSGameRules()->GetBuyTimeLength() );

			wchar_t buffer[128];
			wchar_t buytime[16];
			g_pVGuiLocalize->ConvertANSIToUnicode( strBuyTime, buytime, sizeof(buytime) );
			g_pVGuiLocalize->ConstructString( buffer, sizeof(buffer), g_pVGuiLocalize->Find("#Cstrike_TitlesTXT_Cant_buy"), 1, buytime );
			internalCenterPrint->Print( buffer );
		}
		else
		{
			if( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				OpenPanelWithCheck( PANEL_BUY_CT, PANEL_BUY_EQUIP_CT );
			}
			else if( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				OpenPanelWithCheck( PANEL_BUY_TER, PANEL_BUY_EQUIP_TER );
			}
		}
	}
}

CON_COMMAND( chooseteam, "Choose a new team" )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer && pPlayer->CanShowTeamMenu() )
	{
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
	}
}

CON_COMMAND_F( spec_help, "Show spectator help screen", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_INFO, true );
}

CON_COMMAND_F( spec_menu, "Activates spectator menu", FCVAR_CLIENTCMD_CAN_EXECUTE)
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

CON_COMMAND_F( togglescores, "Toggles score panel", FCVAR_CLIENTCMD_CAN_EXECUTE)
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
void CounterStrikeViewport::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	BaseClass::Start( pGameUIFuncs, pGameEventManager );
}

void CounterStrikeViewport::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	gHUD.InitColors( pScheme );

	SetPaintBackgroundEnabled( false );
}


IViewPortPanel* CounterStrikeViewport::CreatePanelByName(const char *szPanelName)
{
	IViewPortPanel* newpanel = NULL;

	// overwrite MOD specific panel creation

	if ( Q_strcmp(PANEL_SCOREBOARD, szPanelName) == 0)
	{
		newpanel = new CCSClientScoreBoardDialog( this );
	}

	else if ( Q_strcmp(PANEL_SPECGUI, szPanelName) == 0 )
	{
		newpanel = new CCSSpectatorGUI( this );	
	}

	else if ( Q_strcmp(PANEL_CLASS_CT, szPanelName) == 0 )
	{
		newpanel = new CClassMenu_CT( this );	
	}

	else if ( Q_strcmp(PANEL_CLASS_TER, szPanelName) == 0 )
	{
		newpanel = new CClassMenu_TER( this );	
	}

	else if ( Q_strcmp(PANEL_BUY_CT, szPanelName) == 0 )
	{
		newpanel = new CCSBuyMenu_CT( this );
	}

	else if ( Q_strcmp(PANEL_BUY_TER, szPanelName) == 0 )
	{
		newpanel = new CCSBuyMenu_TER( this );
	}

	else if ( Q_strcmp(PANEL_BUY_EQUIP_CT, szPanelName) == 0 )
	{
		newpanel = new CCSBuyEquipMenu_CT( this );
	}

	else if ( Q_strcmp(PANEL_BUY_EQUIP_TER, szPanelName) == 0 )
	{
		newpanel = new CCSBuyEquipMenu_TER( this );
	}

	else if ( Q_strcmp(PANEL_TEAM, szPanelName) == 0 )
	{
		newpanel = new CCSTeamMenu( this );
	}

	else if ( Q_strcmp(PANEL_INFO, szPanelName) == 0 )
	{
		newpanel = new CCSTextWindow( this );
	}

	else
	{
		// create a generic base panel, don't add twice
		newpanel = BaseClass::CreatePanelByName( szPanelName );
	}

	return newpanel; 
}

void CounterStrikeViewport::CreateDefaultPanels( void )
{
	AddNewPanel( CreatePanelByName( PANEL_TEAM ), "PANEL_TEAM" );
	AddNewPanel( CreatePanelByName( PANEL_CLASS_CT ), "PANEL_CLASS_CT" );
	AddNewPanel( CreatePanelByName( PANEL_CLASS_TER ), "PANEL_CLASS_TER" );

	AddNewPanel( CreatePanelByName( PANEL_BUY_CT ), "PANEL_BUY_CT" );
	AddNewPanel( CreatePanelByName( PANEL_BUY_TER ), "PANEL_BUY_TER" );
	AddNewPanel( CreatePanelByName( PANEL_BUY_EQUIP_CT ), "PANEL_BUY_EQUIP_CT" );
	AddNewPanel( CreatePanelByName( PANEL_BUY_EQUIP_TER ), "PANEL_BUY_EQUIP_TER" );

	BaseClass::CreateDefaultPanels();

}

int CounterStrikeViewport::GetDeathMessageStartHeight( void )
{
	int x = YRES(2);

	if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
	{
		x += g_pSpectatorGUI->GetTopBarHeight();
	}

	return x;
}

/*
==========================
HUD_ChatInputPosition

Sets the location of the input for chat text
==========================
*/
//MIKETODO: positioning of chat text (and other engine output)
/*
	#include "Exports.h"

	void CL_DLLEXPORT HUD_ChatInputPosition( int *x, int *y )
	{
		RecClChatInputPosition( x, y );
		if ( gViewPortInterface )
		{
			gViewPortInterface->ChatInputPosition( x, y );
		}
	}

	EXPOSE_SINGLE_INTERFACE(CounterStrikeViewport, IClientVGUI, CLIENTVGUI_INTERFACE_VERSION);
*/
