//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef LANGAMES_H
#define LANGAMES_H
#ifdef _WIN32
#pragma once
#endif

class CLanBroadcastMsgHandler;

//-----------------------------------------------------------------------------
// Purpose: Favorite games list
//-----------------------------------------------------------------------------
class CLanGames : public CBaseGamesPage
{
	DECLARE_CLASS_SIMPLE( CLanGames, CBaseGamesPage );

public:
	CLanGames(vgui::Panel *parent, bool bAutoRefresh=true, const char *pCustomResFilename=NULL);
	~CLanGames();

	// property page handlers
	virtual void OnPageShow();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(InterfaceItem_e item);

	// Control which button are visible.
	void ManualShowButtons( bool bShowConnect, bool bShowRefreshAll, bool bShowFilter );

	// If you pass NULL for pSpecificAddresses, it will broadcast on certain points.
	// If you pass a non-null value, then it will send info queries directly to those ports.
	void InternalGetNewServerList( CUtlVector<netadr_t> *pSpecificAddresses );
 
	virtual void StartRefresh();

	// stops current refresh/GetNewServerList()
	virtual void StopRefresh();


	// IServerRefreshResponse handlers
	// called when a server response has timed out
	virtual void ServerFailedToRespond( HServerListRequest hReq, int iServer );

	// called when the current refresh list is complete
	virtual void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response );

	// Tell the game list what to put in there when there are no games found.
	virtual void SetEmptyListText();

private:
	// vgui message handlers
	virtual void OnTick();

	// lan timeout checking
	virtual void CheckRetryRequest();

	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	// number of servers refreshed
	int m_iServerRefreshCount;	

	// true if we're broadcasting for servers
	bool m_bRequesting;

	// time at which we last broadcasted
	double m_fRequestTime;

	bool m_bAutoRefresh;
};



#endif // LANGAMES_H
