//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERINFO_H
#define SERVERINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"

class CSocket;
class IResponse;


class CServerInfo 
{

public:

	CServerInfo(IResponse *target,serveritem_t &server);
	~CServerInfo();

	// send an rcon command to a server
	void Query();

	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();
	bool Refreshed();

	void UpdateServer(netadr_t *adr, bool proxy, const char *serverName, const char *map, 
						 const char *gamedir, const char *gameDescription, int players, 
						 int maxPlayers, float recvTime, bool password);

	int serverID;
	int received;

private:

	serveritem_t m_Server;
	CSocket	*m_pQuery;	// Game server query socket
	
	IResponse *m_pResponseTarget;

	bool m_bIsRefreshing;
	float m_fSendTime;
	bool m_bRefreshed;

};


#endif // SERVERINFO_H