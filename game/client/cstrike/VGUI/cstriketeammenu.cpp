//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstriketeammenu.h"
#include "backgroundpanel.h"
#include <convar.h>
#include "hud.h" // for gEngfuncs
#include "c_cs_player.h"
#include "cs_gamerules.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSTeamMenu::CCSTeamMenu(IViewPort *pViewPort) : CTeamMenu(pViewPort)
{
	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSTeamMenu::~CCSTeamMenu()
{
}

void CCSTeamMenu::ShowPanel(bool bShow)
{
	if ( bShow )
	{
		engine->CheckPoint( "TeamMenu" );
	}

	BaseClass::ShowPanel( bShow );
}

//-----------------------------------------------------------------------------
// Purpose: called to update the menu with new information
//-----------------------------------------------------------------------------
void CCSTeamMenu::Update( void )
{
	BaseClass::Update();

	const ConVar *allowspecs =  cvar->FindVar( "mp_allowspectators" );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	
	if ( !pPlayer || !CSGameRules() )
		return;

	if ( allowspecs && allowspecs->GetBool() )
	{
		// if we're not already a CT or T...or the freeze time isn't over yet...or we're dead
		if ( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED || 
			CSGameRules()->IsFreezePeriod() || 
			( pPlayer && pPlayer->IsPlayerDead() ) )
		{
			SetVisibleButton("specbutton", true);
		}
		else
		{
			SetVisibleButton("specbutton", false);
		}
	}
	else
	{
		SetVisibleButton("specbutton", false );
	}

	m_bVIPMap = false;

	char mapName[MAX_MAP_NAME];

	Q_FileBase( engine->GetLevelName(), mapName, sizeof(mapName) );

	if ( !Q_strncmp( mapName, "maps/as_", 8 ) )
	{
		m_bVIPMap = true;
	}
		
	// if this isn't a VIP map or we're a spectator/terrorist, then disable the VIP button
	if ( !CSGameRules()->IsVIPMap() || ( pPlayer->GetTeamNumber() != TEAM_CT ) )
	{
		SetVisibleButton("vipbutton", false);
	}
	else // this must be a VIP map and we must already be a CT
	{
		SetVisibleButton("vipbutton", true);
	}

	if( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED ) // we aren't on a team yet
	{
		SetVisibleButton("CancelButton", false); 
	}
	else
	{
		SetVisibleButton("CancelButton", true); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTeamMenu::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if ( state )
	{
		Button *pAutoButton = dynamic_cast< Button* >( FindChildByName( "autobutton" ) );
		if ( pAutoButton )
		{
			pAutoButton->RequestFocus();
			pAutoButton->SetArmed( true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to 
//			cause the player to join a team
//-----------------------------------------------------------------------------
void CCSTeamMenu::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "vguicancel" ) )
	{
		engine->ClientCmd( command );
	}
	
	
	BaseClass::OnCommand(command);

	gViewPortInterface->ShowBackGround( false );
	OnClose();
}


void CCSTeamMenu::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ENTER )
	{
		Button *pAutoButton = dynamic_cast< Button* >( FindChildByName( "autobutton" ) );
		if ( pAutoButton )
		{
			pAutoButton->DoClick();
		}
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the visibility of a button by name
//-----------------------------------------------------------------------------
void CCSTeamMenu::SetVisibleButton(const char *textEntryName, bool state)
{
	Button *entry = dynamic_cast<Button *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->SetVisible(state);
	}
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CCSTeamMenu::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CCSTeamMenu::PerformLayout()
{
	BaseClass::PerformLayout();

	// stretch the window to fullscreen
	if ( !m_backgroundLayoutFinished )
		LayoutBackgroundPanel( this );
	m_backgroundLayoutFinished = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTeamMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );
}

