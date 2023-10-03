//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VQuickJoinGroups.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

extern ConVar cl_quick_join_scroll_max;
extern ConVar cl_quick_join_scroll_start;

DECLARE_BUILD_FACTORY( QuickJoinGroupsPanel );

//=============================================================================
// Quick Join Panel
//=============================================================================
QuickJoinGroupsPanel::QuickJoinGroupsPanel( vgui::Panel *parent , const char* panelName ):
	BaseClass( parent, panelName )
{
}

void QuickJoinGroupsPanel::OnCommand(const char *command)
{
	if ( StringHasPrefix( command, "GroupServer_" ) )
	{
		// relay the command up to our parent
		if ( Panel *pParent = GetParent() )
		{
			pParent->OnCommand( command );
		}
	}
}

void QuickJoinGroupsPanel::OnMousePressed( vgui::MouseCode code )
{
	switch ( code )
	{
	case MOUSE_LEFT:
		if ( GetParent() )
		{
			GetParent()->OnCommand( "PlayOnGroupServer" );
		}
		break;
	}
}

void QuickJoinGroupsPanel::AddServersToList( void )
{
	IServerManager *mgr = g_pMatchFramework->GetMatchSystem()->GetUserGroupsServerManager();
	int iNumServers = mgr->GetNumServers();

	// steam groups games
	for( int i = 0; i < iNumServers; ++i )
	{
		IMatchServer *pServer = mgr->GetServerByIndex( i );
		if ( !pServer )
			continue;

		KeyValues *pServerDetails = pServer->GetGameDetails();
		if ( !pServerDetails )
			continue;

		char const *szName = pServerDetails->GetString( "server/name", NULL );
		if ( !szName || !*szName )
			continue;

		QuickInfo qi;
		qi.m_eType = qi.TYPE_SERVER;
		qi.m_xuid = pServer->GetOnlineId();
		Q_strncpy( qi.m_szName, szName, sizeof( qi.m_szName ) - 1 );
		
		m_FriendInfo.AddToTail( qi );
	}
}