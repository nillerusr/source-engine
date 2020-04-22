//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "IServerRefreshResponse.h"
#include "ServerList.h"
//#include "ServerMsgHandlerDetails.h"
#include "Socket.h"
#include "proto_oob.h"

// for debugging
#include <VGUI_Controls.h>
#include <VGUI_ISystem.h>
#include <VGUI_IVGui.h>

typedef enum
{
	NONE = 0,
	INFO_REQUESTED,
	INFO_RECEIVED
} QUERYSTATUS;

extern void v_strncpy(char *dest, const char *src, int bufsize);
#define min(a,b)    (((a) < (b)) ? (a) : (b))

//-----------------------------------------------------------------------------
// Purpose: Comparison function used in query redblack tree
//-----------------------------------------------------------------------------
bool QueryLessFunc( const query_t &item1, const query_t &item2 )
{
	// compare port then ip
	if (item1.addr.port < item2.addr.port)
		return true;
	else if (item1.addr.port > item2.addr.port)
		return false;

	int ip1 = *(int *)&item1.addr.ip;
	int ip2 = *(int *)&item2.addr.ip;

	return ip1 < ip2;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerList::CServerList(IServerRefreshResponse *target) : m_Queries(0, MAX_QUERY_SOCKETS, QueryLessFunc)
{
	m_pResponseTarget = target;
	m_iUpdateSerialNumber = 1;

	// calculate max sockets based on users' rate
	char speedBuf[32];
	int internetSpeed;
	if (!vgui::system()->GetRegistryString("HKEY_CURRENT_USER\\Software\\Valve\\Tracker\\Rate", speedBuf, sizeof(speedBuf)-1))
	{
		// default to DSL speed if no reg key found (an unlikely occurance)
		strcpy(speedBuf, "7500");
	}
	internetSpeed = atoi(speedBuf);
	int maxSockets = (MAX_QUERY_SOCKETS * internetSpeed) / 10000;
	if (internetSpeed < 6000)
	{
		// reduce the number of active queries again for slow internet speeds
		maxSockets /= 2;
	}
	m_nMaxActive = maxSockets;

	m_nRampUpSpeed = 1;
	m_bQuerying = false;
	m_nMaxRampUp = 1;

	m_nInvalidServers = 0;
	m_nRefreshedServers = 0;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerList::~CServerList()
{
//	delete m_pQuery;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerList::RunFrame()
{

	for(int i=0;i<m_Servers.Count();i++) {
			m_Servers[i]->RunFrame();
	}

	QueryFrame();
}

//-----------------------------------------------------------------------------
// Purpose: gets a server from the list by id, range [0, ServerCount)
//-----------------------------------------------------------------------------
serveritem_t &CServerList::GetServer(unsigned int serverID)
{
	if (m_Servers.IsValidIndex(serverID))
	{
		return m_Servers[serverID]->GetServer();
	}

	// return a dummy
	static serveritem_t dummyServer;
	memset(&dummyServer, 0, sizeof(dummyServer));
	return dummyServer;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of servers
//-----------------------------------------------------------------------------
int CServerList::ServerCount()
{
	return m_Servers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of servers not yet pinged
//-----------------------------------------------------------------------------
int CServerList::RefreshListRemaining()
{
	return m_RefreshList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the server list is still in the process of talking to servers
//-----------------------------------------------------------------------------
bool CServerList::IsRefreshing()
{
	return m_bQuerying;
}

//-----------------------------------------------------------------------------
// Purpose: adds a new server to the list
//-----------------------------------------------------------------------------
unsigned int CServerList::AddNewServer(serveritem_t &server)
{

	unsigned int serverID = m_Servers.AddToTail(new CServerInfo(this ,server));
	m_Servers[serverID]->serverID = serverID;
	return serverID;
}

//-----------------------------------------------------------------------------
// Purpose: Clears all servers from the list
//-----------------------------------------------------------------------------
void CServerList::Clear()
{
	StopRefresh();

	m_Servers.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: stops all refreshing
//-----------------------------------------------------------------------------
void CServerList::StopRefresh()
{
	// Reset query context
	m_Queries.RemoveAll();

	// reset server received states
	int count = ServerCount();
	for (int i = 0; i < count; i++)
	{
		m_Servers[i]->received = 0;
	}

	m_RefreshList.RemoveAll();

	// up the serial number so previous results don't interfere
	m_iUpdateSerialNumber++;

	m_nInvalidServers = 0;
	m_nRefreshedServers = 0;
	m_bQuerying = false;
	m_nMaxRampUp = m_nRampUpSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: marks a server to be refreshed
//-----------------------------------------------------------------------------
void CServerList::AddServerToRefreshList(unsigned int serverID)
{
	if (!m_Servers.IsValidIndex(serverID))
		return;

	serveritem_t &server = m_Servers[serverID]->GetServer();
	server.received = NONE;

	m_RefreshList.AddToTail(serverID);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerList::StartRefresh()
{
	if (m_RefreshList.Count() > 0)
	{
		m_bQuerying = true;
	}
}


void CServerList::ServerResponded()
{
	for(int i=0;i<m_Servers.Count();i++) {
		if ( m_Servers[i]->Refreshed() ) {

			serveritem_t &server = m_Servers[i]->GetServer();
			// copy in data necessary for filters

			// add to ping times list 
			server.pings[0] = server.pings[1];
			server.pings[1] = server.pings[2];
			server.pings[2] = server.ping;

			// calculate ping
			int ping = CalculateAveragePing(server);

			// make sure the ping changes each time so the user can see the server has updated
			if (server.ping == ping && ping>0)
			{
				ping--;
			}
			server.ping = ping;
			server.received = INFO_RECEIVED;

			netadr_t adr;
			adr.ip[0] = server.ip[0];
			adr.ip[1] = server.ip[1];
			adr.ip[2] = server.ip[2];
			adr.ip[3] = server.ip[3];
			adr.port = (server.port & 0xff) << 8 | (server.port & 0xff00) >> 8;
			adr.type = NA_IP;

			query_t finder;
			finder.addr = adr;

			m_Queries.Remove(finder);
			// notify the UI of the new server info
			m_pResponseTarget->ServerResponded(server);
		}
	
	}
		

}

//-----------------------------------------------------------------------------
// Purpose: called when a server response has timed out
//-----------------------------------------------------------------------------
void CServerList::ServerFailedToRespond()
{

}



//-----------------------------------------------------------------------------
// Purpose: recalculates a servers ping, from the last few ping times
//-----------------------------------------------------------------------------
int CServerList::CalculateAveragePing(serveritem_t &server)
{
	if (server.pings[0])
	{
		// three pings, throw away any the most extreme and average the other two
		int middlePing = 0, lowPing = 1, highPing = 2;
		if (server.pings[0] < server.pings[1])
		{
			if (server.pings[0] > server.pings[2])
			{
				lowPing = 2;
				middlePing = 0;
				highPing = 1;
			}
			else if (server.pings[1] < server.pings[2])
			{
				lowPing = 0;
				middlePing = 1;
				highPing = 2;
			}
			else
			{
				lowPing = 0;
				middlePing = 2;
				highPing = 1;
			}
		}
		else
		{
			if (server.pings[1] > server.pings[2])
			{
				lowPing = 2;
				middlePing = 1;
				highPing = 0;
			}
			else if (server.pings[0] < server.pings[2])
			{
				lowPing = 1;
				middlePing = 0;
				highPing = 2;
			}
			else
			{
				lowPing = 1;
				middlePing = 2;
				highPing = 0;
			}
		}

		// we have the middle ping, see which it's closest to
		if ((server.pings[middlePing] - server.pings[lowPing]) < (server.pings[highPing] - server.pings[middlePing]))
		{
			return (server.pings[middlePing] + server.pings[lowPing]) / 2;
		}
		else
		{
			return (server.pings[middlePing] + server.pings[highPing]) / 2;
		}
	}
	else if (server.pings[1])
	{
		// two pings received, average them
		return (server.pings[1] + server.pings[2]) / 2;
	}
	else
	{
		// only one ping received so far, use that
		return server.pings[2];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame to check queries
//-----------------------------------------------------------------------------
void CServerList::QueryFrame()
{
	if (!m_bQuerying)
		return;

	float curtime = CSocket::GetClock();

	// walk the query list, looking for any server timeouts
	unsigned short idx = m_Queries.FirstInorder();
	while (m_Queries.IsValidIndex(idx))
	{
		query_t &query = m_Queries[idx];
		if ((curtime - query.sendTime) > 1.2f)
		{
			// server has timed out
			serveritem_t &item = m_Servers[query.serverID]->GetServer();

			// mark the server
			item.pings[0] = item.pings[1];
			item.pings[1] = item.pings[2];
			item.pings[2] = 1200;
			item.ping = CalculateAveragePing(item);
			if (!item.hadSuccessfulResponse)
			{
				// remove the server if it has never responded before
				item.doNotRefresh = true;
				m_nInvalidServers++;
			}
			// respond to the game list notifying of the lack of response
			m_pResponseTarget->ServerFailedToRespond(item);
			item.received = false;

			// get the next server now, since we're about to delete it from query list
			unsigned short nextidx = m_Queries.NextInorder(idx);
			
			// delete the query
			m_Queries.RemoveAt(idx);

			// move to next item
			idx = nextidx;
		}
		else
		{
			// still waiting for server result
			idx = m_Queries.NextInorder(idx);
		}
	}
	

	// increment the number of sockets to use
	m_nMaxRampUp = min(m_nMaxActive, m_nMaxRampUp + m_nRampUpSpeed);

	// see if we should send more queries
	while (m_RefreshList.Count() > 0 && (int)m_Queries.Count() < m_nMaxRampUp)
	{
		// get the first item from the list to refresh
		int currentServer = m_RefreshList[0];
		if (!m_Servers.IsValidIndex(currentServer))
			break;
		serveritem_t &item = m_Servers[currentServer]->GetServer();

		item.time = curtime;

		//QueryServer(m_pQuery, currentServer);
		m_Servers[currentServer]->Query();
		
		query_t query;
	
		netadr_t adr;
		adr.ip[0] = item.ip[0];
		adr.ip[1] = item.ip[1];
		adr.ip[2] = item.ip[2];
		adr.ip[3] = item.ip[3];
		adr.port = (item.port & 0xff) << 8 | (item.port & 0xff00) >> 8;
		adr.type = NA_IP;

		query.addr =adr;
		query.sendTime=curtime;
		query.serverID=item.serverID;

		m_Queries.Insert(query);

		// remove the server from the refresh list
		m_RefreshList.Remove((int)0);
	}

	// Done querying?
	if (m_Queries.Count() < 1)
	{
		m_bQuerying = false;
		m_pResponseTarget->RefreshComplete();

		// up the serial number, so that we ignore any late results
		m_iUpdateSerialNumber++;
	}
}




