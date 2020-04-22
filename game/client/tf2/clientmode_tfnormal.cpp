//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Normal HUD mode
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_chat.h"
#include "clientmode_tfnormal.h"
#include "clientmode.h"
#include "hud.h"
#include "iinput.h"
#include "c_basetfplayer.h"
#include "hud_timer.h"
#include "usercmd.h"
#include "in_buttons.h"
#include "c_tf_playerclass.h"
#include "engine/IEngineSound.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Instance the singleton and expose the interface to it.
// Output : IClientMode
//-----------------------------------------------------------------------------
IClientMode *GetClientModeNormal( void )
{
	static ClientModeTFNormal g_ClientModeNormal;
	return &g_ClientModeNormal;
}

ClientModeTFNormal::ClientModeTFNormal()
{
	m_pViewport = new Viewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

ClientModeTFNormal::Viewport::Viewport()
{
	// use a custom scheme for the hud
	m_bHumanScheme = true;
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientSchemeHuman.res", "HudScheme");
	SetScheme(scheme);
}

void ClientModeTFNormal::Viewport::OnThink()
{
	BaseClass::OnThink();

	// See if scheme should change
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return;
	int team = pPlayer->GetTeamNumber();
	if ( !team )
		return;

	bool human = ( team == TEAM_HUMANS ) ? true : false;
	if ( human != m_bHumanScheme )
	{
		ReloadScheme();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeTFNormal::Viewport::ReloadScheme()
{
	// See if scheme should change
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if(!pPlayer)
		return;

	const char *schemeFile = NULL;

	int team = pPlayer->GetTeamNumber();
	if ( team )
	{
		m_bHumanScheme = ( team == TEAM_HUMANS ) ? true : false;

		if ( m_bHumanScheme )
		{
			schemeFile = "resource/ClientSchemeHuman.res";
		}
		else
		{
			schemeFile = "resource/ClientSchemeAlien.res";
		}
	}

//	BaseClass::ReloadScheme( schemeFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *ClientModeTFNormal::GetMinimapParent( void )
{
	return GetViewport();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeTFNormal::Update()
{
	BaseClass::Update();
	HudCommanderOverlayMgr()->Tick( );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void ClientModeTFNormal::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	// Let the player override the view.
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if(!pPlayer)
		return;

	// Let the player at it
	pPlayer->CreateMove( flInputSampleTime, cmd );

	// Handle knockdowns
	if ( pPlayer->CheckKnockdownAngleOverride() )
	{
		QAngle ang;
		pPlayer->GetKnockdownAngles( ang );

		cmd->viewangles = ang;
		engine->SetViewAngles( ang );

		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
		// Only keep score if it's down
		cmd->buttons &= ( IN_SCORE );
	}

	if ( pPlayer->GetPlayerClass() )
	{
		C_PlayerClass *pPlayerClass = pPlayer->GetPlayerClass();
		pPlayerClass->CreateMove( flInputSampleTime, cmd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ClientModeTFNormal::ShouldDrawViewModel( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if(!pPlayer)
		return false;

	return pPlayer->ShouldDrawViewModel();
}





