//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SERVICE_CONN_MGR_H
#define SERVICE_CONN_MGR_H
#ifdef _WIN32
#pragma once
#endif


#include "utllinkedlist.h"
#include "utlvector.h"
#include "tcpsocket.h"
#include "ThreadedTCPSocketEmu.h"


class CServiceConn
{
public:
				CServiceConn();
				~CServiceConn();

	int			m_ID;
	ITCPSocket	*m_pSocket;
	DWORD		m_LastRecvTime;
};


// ------------------------------------------------------------------------------------------ // 
// CServiceConnMgr. This class manages connections to all the UIs (there should only be one UI at 
// any given time, but it's conceivable that multiple people can be logged into NT servers
// simultaneously).
// ------------------------------------------------------------------------------------------ // 

class CServiceConnMgr
{
public:
	
			CServiceConnMgr();
			~CServiceConnMgr();
	
	bool	InitServer();	// Registers as a systemwide server.
	bool	InitClient();	// Connects to the server.
	void	Term();

	// Returns true if there are any connections. If you used InitClient() and there are 
	// no connections, it will continuously try to connect with a server.
	bool	IsConnected();
	
	// This should be called as often as possible. It checks for dead connections and it 
	// handles incoming packets from UIs.
	void	Update();

	// This sends out a message. If id is -1, then it sends to all connections.
	void	SendPacket( int id, const void *pData, int len );


// Overridables.
public:
	
	virtual void	OnNewConnection( int id );
	virtual void	OnTerminateConnection( int id );
	virtual void	HandlePacket( const char *pData, int len );


private:

	void			AttemptConnect();


private:
	
	CUtlLinkedList<CServiceConn*, int>	m_Connections;

	bool	m_bShuttingDown;

	// This tells if we're running as a client or server.
	bool	m_bServer;

	// If we're a client, this is the last time we tried to connect.
	DWORD	m_LastConnectAttemptTime;

	// If we're the server, this is the socket we listen on.
	ITCPListenSocket	*m_pListenSocket;
};


#endif // SERVICE_CONN_MGR_H
