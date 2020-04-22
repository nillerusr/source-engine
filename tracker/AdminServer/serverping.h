//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERPING_H
#define SERVERPING_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"

class CSocket;
class IResponse;


class CServerPing 
{

public:

	CServerPing(IResponse *target,serveritem_t &server);
	~CServerPing();

	// send a ping to the server
	void Query();

	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();
	bool Refreshed();

	void UpdateServer(float recvTime);

private:

	serveritem_t m_Server;
	CSocket	*m_pQuery;	// Game server query socket
	
	IResponse *m_pResponseTarget;

	bool m_bIsRefreshing;
	float m_fSendTime;
	bool m_bRefreshed;

};


#endif // SERVERPING_H