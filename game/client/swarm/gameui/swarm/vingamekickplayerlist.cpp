//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VInGameKickPlayerList.h"
#include "VFooterPanel.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "VHybridButton.h"
#include "vgui/ILocalize.h"
#include "game/client/IGameClientExports.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define KICK_PLAYER_LIST_MAX_PLAYERS 3

//=============================================================================
InGameKickPlayerList::InGameKickPlayerList(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);


	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( 0, FF_NONE, true );
	}
}

//=============================================================================
void InGameKickPlayerList::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

//=============================================================================
void InGameKickPlayerList::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );
}


//=============================================================================
void InGameKickPlayerList::LoadLayout()
{
	BaseClass::LoadLayout();

	//int iLocalPlayerTeam;

	if ( false ) // TODO: !GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) )
	{
		// no local player?
		Assert( 0 );

		// hide all the buttons, weird state
		for ( int i=0;i<KICK_PLAYER_LIST_MAX_PLAYERS; i++ )
		{
			BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( VarArgs( "BtnPlayer%d", i ) ) );
			if ( pButton )
			{
				pButton->SetVisible( false );
			}
		}

		return;
	}

	// get a list of players that we can kick

	m_KickablePlayersUserIDs.Purge();

	for( int i = 1; i <= engine->GetMaxClients(); ++i)
	{
		player_info_t playerInfo;
		if( engine->GetPlayerInfo(i, &playerInfo) && !playerInfo.fakeplayer )
		{
			if ( true ) // TODO: GameClientExports()->IsPlayerKickableByLocalPlayer( i ) )
			{
				m_KickablePlayersUserIDs.AddToTail( playerInfo.userID );
			}
		}
	}

	int iKickablePlayers = m_KickablePlayersUserIDs.Count();

	// If there are no players to be kicked, hide the description and show the label explaining why 
	// there are no players to choose

	SetControlVisible( "LblDescription", iKickablePlayers > 0 );
	SetControlVisible( "LblNoPlayers", iKickablePlayers == 0 );

	for ( int i=0;i<KICK_PLAYER_LIST_MAX_PLAYERS; i++ )
	{
		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( VarArgs( "BtnPlayer%d", i+1 ) ) );
		if ( pButton )
		{
			if ( i < iKickablePlayers )
			{
				int userID = m_KickablePlayersUserIDs.Element(i);

				player_info_t playerInfo;
				if( engine->GetPlayerInfo( engine->GetPlayerForUserID(userID), &playerInfo ) )
				{
					pButton->SetVisible( true );
					pButton->SetText( playerInfo.name );
					pButton->SetCommand( VarArgs( "KickPlayer%d", userID ) );
				}
				else
				{
					pButton->SetVisible( false );
				}
			}
			else
			{
				pButton->SetVisible( false );
			}
		}
	}
}

//=============================================================================
void InGameKickPlayerList::OnCommand(const char *command)
{
	if ( !Q_strncmp( command, "KickPlayer", 10 ) && Q_strlen(command) > 10 )
	{
		int iUserID = atoi( command+10 );

		// make sure it's a real userid
		if( engine->GetPlayerForUserID( iUserID ) > 0 )
		{
			engine->ClientCmd( VarArgs( "callvote kick %d", iUserID ) );
		}
		
		engine->ClientCmd("gameui_hide");		
		Close();
	}
	else if ( !Q_strcmp( command, "Cancel" ) )
	{
		engine->ClientCmd("gameui_hide");		
		Close();
	}
}