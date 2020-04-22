//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFriendsGames::CFriendsGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "FriendsGames",  eFriendsServer )
{
	m_iServerRefreshCount = 0;
	
	if ( !IsSteamGameServerBrowsingEnabled() )
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_OfflineMode");
		m_pConnect->SetEnabled( false );
		m_pRefreshAll->SetEnabled( false );
		m_pRefreshQuick->SetEnabled( false );
		m_pAddServer->SetEnabled( false );
		m_pFilter->SetEnabled( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFriendsGames::~CFriendsGames()
{
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFriendsGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
		return true;

	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CFriendsGames::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	SetRefreshing(false);
	m_pGameList->SortList();
	m_iServerRefreshCount = 0;

	if ( IsSteamGameServerBrowsingEnabled() )
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFriendsServers");
	}

	BaseClass::RefreshComplete( hReq, response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CFriendsGames::OnOpenContextMenu(int itemID)
{
	// get the server
	int serverID = GetSelectedServerID();

	if ( serverID == -1 )
		return;

	// Activate context menu
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());
	menu->ShowMenu(this, serverID, true, true, true, true);
}
