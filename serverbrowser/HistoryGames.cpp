//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHistoryGames::CHistoryGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "HistoryGames", eHistoryServer )
{
	m_bRefreshOnListReload = false;
	m_pGameList->AddColumnHeader(10, "LastPlayed", "#ServerBrowser_LastPlayed", 100);
	m_pGameList->SetSortFunc(10, LastPlayedCompare);
	m_pGameList->SetSortColumn(10);

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
CHistoryGames::~CHistoryGames()
{
}		

//-----------------------------------------------------------------------------
// Purpose: loads favorites list from disk
//-----------------------------------------------------------------------------
void CHistoryGames::LoadHistoryList()
{
	if ( IsSteamGameServerBrowsingEnabled() )
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoServersPlayed");
	}

	if ( m_bRefreshOnListReload )
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CHistoryGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
		return true;
	
	case ADDSERVER:
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CHistoryGames::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	SetRefreshing(false);
	m_pGameList->SetEmptyListText("#ServerBrowser_NoServersPlayed");
	m_pGameList->SortList();

	BaseClass::RefreshComplete( hReq, response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CHistoryGames::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());

	// get the server
	int serverID = GetSelectedServerID();

	if(  serverID != -1 )
	{
		// Activate context menu
		menu->ShowMenu(this, serverID, true, true, true, true);
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromHistory", new KeyValues("RemoveFromHistory"), this);
	}
	else
	{
		// no selected rows, so don't display default stuff in menu
		menu->ShowMenu(this, (uint32)-1, false, false, false, false);
	}
}


//-----------------------------------------------------------------------------
// Purpose: removes a server from the favorites
//-----------------------------------------------------------------------------
void CHistoryGames::OnRemoveFromHistory()
{
	if ( !steamapicontext->SteamMatchmakingServers() || !steamapicontext->SteamMatchmaking() )
		return;

	// iterate the selection
	for ( int i = m_pGameList->GetSelectedItemsCount() - 1; i >= 0; i-- )
	{
		int itemID = m_pGameList->GetSelectedItem( i );
		int serverID = m_pGameList->GetItemData(itemID)->userData;
		
		gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, serverID );
		if ( pServer )
			steamapicontext->SteamMatchmaking()->RemoveFavoriteGame( pServer->m_nAppID, pServer->m_NetAdr.GetIP(), pServer->m_NetAdr.GetConnectionPort(), pServer->m_NetAdr.GetQueryPort(), k_unFavoriteFlagHistory );
	}

	UpdateStatus();	
	InvalidateLayout();
	Repaint();
}

