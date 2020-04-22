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
CFavoriteGames::CFavoriteGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "FavoriteGames", eFavoritesServer )
{
	m_bRefreshOnListReload = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFavoriteGames::~CFavoriteGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: loads favorites list from disk
//-----------------------------------------------------------------------------
void CFavoriteGames::LoadFavoritesList()
{
	if ( steamapicontext->SteamMatchmaking() && steamapicontext->SteamMatchmaking()->GetFavoriteGameCount() == 0 )
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

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
bool CFavoriteGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
	case ADDSERVER:
		return true;

	case ADDCURRENTSERVER:
		return !IsSteam() && BFiltersVisible();
	
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CFavoriteGames::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	SetRefreshing(false);
	if ( steamapicontext->SteamMatchmaking() && steamapicontext->SteamMatchmaking()->GetFavoriteGameCount() == 0 )
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

	}
	m_pGameList->SortList();

	BaseClass::RefreshComplete( hReq, response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CFavoriteGames::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());

	// get the server
	int serverID = GetSelectedServerID();

	if ( serverID != -1 )
	{
		// Activate context menu
		menu->ShowMenu(this, serverID, true, true, true, false);
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromFavorites", new KeyValues("RemoveFromFavorites"), this);
	}
	else
	{
		// no selected rows, so don't display default stuff in menu
		menu->ShowMenu( this,(uint32)-1, false, false, false, false );
	}
	
	menu->AddMenuItem("AddServerByName", "#ServerBrowser_AddServerByIP", new KeyValues("AddServerByName"), this);
}


//-----------------------------------------------------------------------------
// Purpose: removes a server from the favorites
//-----------------------------------------------------------------------------
void CFavoriteGames::OnRemoveFromFavorites()
{
	if ( !steamapicontext->SteamMatchmakingServers() || !steamapicontext->SteamMatchmaking() )
		return;

	// iterate the selection
	for ( int iGame = 0; iGame < m_pGameList->GetSelectedItemsCount(); iGame++ )
	{
		int itemID = m_pGameList->GetSelectedItem( iGame );
		int serverID = m_pGameList->GetItemData(itemID)->userData;
		
		gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, serverID );
		
		if ( pServer )
		{
			steamapicontext->SteamMatchmaking()->RemoveFavoriteGame( pServer->m_nAppID, pServer->m_NetAdr.GetIP(), pServer->m_NetAdr.GetConnectionPort(), pServer->m_NetAdr.GetQueryPort(), k_unFavoriteFlagFavorite );
		}
	}

	UpdateStatus();	
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a server by IP address
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddServerByName()
{
	// open the add server dialog
	CDialogAddServer *dlg = new CDialogAddServer( &ServerBrowserDialog(), this );
	dlg->MoveToCenterOfScreen();
	dlg->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Adds the currently connected server to the list
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddCurrentServer()
{
	gameserveritem_t *pConnected = ServerBrowserDialog().GetCurrentConnectedServer();

	if ( pConnected && steamapicontext->SteamMatchmaking() )
	{
		steamapicontext->SteamMatchmaking()->AddFavoriteGame( pConnected->m_nAppID, pConnected->m_NetAdr.GetIP(), pConnected->m_NetAdr.GetConnectionPort(), pConnected->m_NetAdr.GetQueryPort(), k_unFavoriteFlagFavorite, time( NULL ) );
		m_bRefreshOnListReload = true;

		if ( GameSupportsReplay() )
		{
			// send command to propagate to the client so the client can send it on to the GC
			char command[ 256 ];
			Q_snprintf( command, Q_ARRAYSIZE( command ), "rfgc %s\n", pConnected->m_NetAdr.GetConnectionAddressString() );
			g_pRunGameEngine->AddTextCommand( command );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse posted messages
//			 
//-----------------------------------------------------------------------------
void CFavoriteGames::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "AddServerByName"))
	{
		OnAddServerByName();
	}
	else if (!Q_stricmp(command, "AddCurrentServer" ))
	{
		OnAddCurrentServer();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: enables adding server
//-----------------------------------------------------------------------------
void CFavoriteGames::OnConnectToGame()
{
	m_pAddCurrentServer->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: disables adding current server
//-----------------------------------------------------------------------------
void CFavoriteGames::OnDisconnectFromGame( void )
{
	m_pAddCurrentServer->SetEnabled( false );
}
