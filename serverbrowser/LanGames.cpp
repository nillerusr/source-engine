//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

const float BROADCAST_LIST_TIMEOUT = 0.4f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLanGames::CLanGames(vgui::Panel *parent, bool bAutoRefresh, const char *pCustomResFilename ) : 
	CBaseGamesPage(parent, "LanGames", eLANServer, pCustomResFilename)
{
	m_iServerRefreshCount = 0;
	m_bRequesting = false;
	m_bAutoRefresh = bAutoRefresh;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLanGames::~CLanGames()
{
}


//-----------------------------------------------------------------------------
// Purpose: Activates the page, starts refresh
//-----------------------------------------------------------------------------
void CLanGames::OnPageShow()
{
	if ( m_bAutoRefresh )
		StartRefresh();
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CLanGames::OnTick()
{
	BaseClass::OnTick();
	CheckRetryRequest();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CLanGames::SupportsItem(InterfaceItem_e item)
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
// Purpose: starts the servers refreshing
//-----------------------------------------------------------------------------
void CLanGames::StartRefresh()
{
	BaseClass::StartRefresh();
	m_fRequestTime = Plat_FloatTime();
}


//-----------------------------------------------------------------------------
// Purpose: Control which button are visible.
//-----------------------------------------------------------------------------
void CLanGames::ManualShowButtons( bool bShowConnect, bool bShowRefreshAll, bool bShowFilter )
{
	m_pConnect->SetVisible( bShowConnect );
	m_pRefreshAll->SetVisible( bShowRefreshAll );
	m_pFilter->SetVisible( bShowFilter );
}


//-----------------------------------------------------------------------------
// Purpose: stops current refresh/GetNewServerList()
//-----------------------------------------------------------------------------
void CLanGames::StopRefresh()
{
	BaseClass::StopRefresh();
	// clear update states
	m_bRequesting = false;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we've finished looking for local servers
//-----------------------------------------------------------------------------
void CLanGames::CheckRetryRequest()
{
	if (!m_bRequesting)
		return;

	double curtime = Plat_FloatTime();
	if (curtime - m_fRequestTime <= BROADCAST_LIST_TIMEOUT)
	{
		return;
	}

	// time has elapsed, finish up
	m_bRequesting = false;
}

//-----------------------------------------------------------------------------
// Purpose: called when a server response has timed out, remove it
//-----------------------------------------------------------------------------
void CLanGames::ServerFailedToRespond( HServerListRequest hReq, int iServer )
{
	int iServerMap = m_mapServers.Find( iServer );
	if ( iServerMap != m_mapServers.InvalidIndex() )
		RemoveServer( m_mapServers[ iServerMap ] );
}

//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CLanGames::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	SetRefreshing( false );
	m_pGameList->SortList();
	m_iServerRefreshCount = 0;
	m_pGameList->SetEmptyListText("#ServerBrowser_NoLanServers");
	SetEmptyListText();

	BaseClass::RefreshComplete( hReq, response );
}

void CLanGames::SetEmptyListText()
{
	m_pGameList->SetEmptyListText("#ServerBrowser_NoLanServers");
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CLanGames::OnOpenContextMenu(int row)
{
	int serverID = GetSelectedServerID();

	if ( serverID == -1 )
		return;

	// Activate context menu
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());
	menu->ShowMenu(this, serverID, true, true, true, false);
}

