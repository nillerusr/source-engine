//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMELIST_H
#define IGAMELIST_H
#ifdef _WIN32
#pragma once
#endif

struct serveritem_t;
#include "FindSteamServers.h"

#define EMPTY_SERVER_NAME "0.0.0.0:0"

//-----------------------------------------------------------------------------
// Purpose: Interface to accessing a game list
//-----------------------------------------------------------------------------
class IGameList
{
public:

	enum InterfaceItem_e
	{
		FILTERS,
		GETNEWLIST,
		ADDSERVER,
		ADDCURRENTSERVER,
	};

	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(InterfaceItem_e item) = 0;

	// starts the servers refreshing
	virtual void StartRefresh() = 0;

	// gets a new server list
	virtual void GetNewServerList() = 0;

	// stops current refresh/GetNewServerList()
	virtual void StopRefresh() = 0;

	// returns true if the list is currently refreshing servers
	virtual bool IsRefreshing() = 0;

	// gets information about specified server
	virtual serveritem_t &GetServer(unsigned int serverID) = 0;

	// adds a new server to list
	virtual void AddNewServer(serveritem_t &server) = 0;

	// marks that server list has been fully received
	virtual void ListReceived(bool moreAvailable, const char *lastUniqueIP, ESteamServerType serverType) = 0;

	// called when Connect button is pressed
	virtual void OnBeginConnect() = 0;

	// reapplies filters
	virtual void ApplyFilters() = 0;

	// invalid server index
	virtual int GetInvalidServerListID() = 0;
};


#endif // IGAMELIST_H
