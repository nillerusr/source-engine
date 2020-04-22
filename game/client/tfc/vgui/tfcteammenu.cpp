//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tfcteammenu.h"
#include <convar.h>
#include "hud.h" // for gEngfuncs
#include "c_tfc_player.h"
#include "tfc_gamerules.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFCTeamMenu::CTFCTeamMenu(IViewPort *pViewPort) : CTeamMenu(pViewPort)
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFCTeamMenu::~CTFCTeamMenu()
{
}

void CTFCTeamMenu::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings( inResourceData );
}

void CTFCTeamMenu::ShowPanel(bool bShow)
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
void CTFCTeamMenu::Update( void )
{
	BaseClass::Update();

	const ConVar *allowspecs =  cvar->FindVar( "mp_allowspectators" );

	if ( allowspecs && allowspecs->GetBool() )
	{
		C_TFCPlayer *pPlayer = C_TFCPlayer::GetLocalTFCPlayer();
		if ( !pPlayer || !TFCGameRules() )
			return;

		// if we're not already a CT or T...or the freeze time isn't over yet...or we're dead
		if ( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED || 
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

	char mapName[MAX_MAP_NAME];

	Q_FileBase( engine->GetLevelName(), mapName, sizeof(mapName) );
	if( C_TFCPlayer::GetLocalTFCPlayer()->GetTeamNumber() == TEAM_UNASSIGNED ) // we aren't on a team yet
	{
		SetVisibleButton("CancelButton", false); 
	}
	else
	{
		SetVisibleButton("CancelButton", true); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to 
//			cause the player to join a team
//-----------------------------------------------------------------------------
void CTFCTeamMenu::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "vguicancel" ) )
	{
		engine->ClientCmd( command );
	}
	
	
	BaseClass::OnCommand(command);

	gViewPortInterface->ShowBackGround( false );
	OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the visibility of a button by name
//-----------------------------------------------------------------------------
void CTFCTeamMenu::SetVisibleButton(const char *textEntryName, bool state)
{
	Button *entry = dynamic_cast<Button *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->SetVisible(state);
	}
}
