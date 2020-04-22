//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERLIST_H
#define SERVERLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"
#include "serverinfo.h"
#include "iresponse.h"


#include "utlrbtree.h"

class CSocket;
class IServerRefreshResponse;

// holds a single query - definition needs to be public
struct query_t
{
	netadr_t addr;
	int serverID;
	float sendTime;
};

//-----------------------------------------------------------------------------
// Purpose: Handles a list of servers, and can refresh them
//-----------------------------------------------------------------------------
class CServerList: public IResponse
{
public:
	CServerList(IServerRefreshResponse *gameList);
	~CServerList();

	// Handles a frame of networking
	void RunFrame();
	
	// gets a server from the list by id, range [0, ServerCount)
	serveritem_t &GetServer(unsigned int serverID);

	// returns the number of servers
	int ServerCount();

	// adds a new server to the list, returning a handle to the server
	unsigned int AddNewServer(serveritem_t &server);

	// starts a refresh
	void StartRefresh();

	// stops all refreshing
	void StopRefresh();

	// clears all servers from the list
	void Clear();

	// marks a server to be refreshed
	void AddServerToRefreshList(unsigned int serverID);

	// returns true if servers are currently being refreshed
	bool IsRefreshing();

	// returns the number of servers not yet pinged
	int RefreshListRemaining();

	// implementation of IRconResponse interface
	// called when the server has successfully responded to an info command
	virtual void ServerResponded();

	// called when a server response has timed out
	virtual void ServerFailedToRespond();


private:
	// Run query logic for this frame
	void QueryFrame();
	// recalculates a servers ping, from the last few ping times
	int CalculateAveragePing(serveritem_t &server);

	IServerRefreshResponse *m_pResponseTarget;

	enum
	{
		MAX_QUERY_SOCKETS = 255,
	};


	// holds the list of all the currently active queries
	CUtlRBTree<query_t, unsigned short> m_Queries;

	// list of all known servers
	CUtlVector<CServerInfo *> m_Servers;

	// list of servers to be refreshed, in order... this would be more optimal as a queue
	CUtlVector<int> m_RefreshList;

	int	m_iUpdateSerialNumber; // serial number of current update so we don't get results overlapping between different server list updates
	bool m_bQuerying;	// Is refreshing taking place?
 	int	m_nMaxActive;	// Max # of simultaneous sockets to use for querying
	int m_nMaxRampUp;	// increasing number of sockets to use
	int m_nRampUpSpeed;	// amount to ramp up each frame
	int	m_nInvalidServers;	// number of servers marked as 'no not refresh'
	int	m_nRefreshedServers; // count of servers refreshed
};

#endif // SERVERLIST_H
