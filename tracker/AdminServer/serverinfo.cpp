//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "serverinfo.h"
#include "Iresponse.h"

#include "ServerInfoMsgHandler.h"
#include "Socket.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

namespace
{
	const float SERVER_TIMEOUT =5.0f; // timeout before failing
}

typedef enum
{
	NONE = 0,
	INFO_REQUESTED,
	INFO_RECEIVED
} RCONSTATUS;

CServerInfo::CServerInfo(IResponse *target,serveritem_t &server) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;

	m_bIsRefreshing=false;

	int bytecode = S2A_INFO_DETAILED;
	m_pQuery = new CSocket("internet server query", -1);
	m_pQuery->AddMessageHandler(new CServerInfoMsgHandlerDetails(this, CMsgHandler::MSGHANDLER_ALL, &bytecode));

	m_fSendTime= 0;
}

CServerInfo::~CServerInfo() {
	delete m_pQuery;

}


//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CServerInfo::Query()
{
	CMsgBuffer *buffer = m_pQuery->GetSendBuffer();
	assert( buffer );
	
	if ( !buffer ) 
	{
		return;
	}

	m_bIsRefreshing=true;
	m_bRefreshed=false;

	netadr_t adr;

	adr.ip[0] = m_Server.ip[0];
	adr.ip[1] = m_Server.ip[1];
	adr.ip[2] = m_Server.ip[2];
	adr.ip[3] = m_Server.ip[3];
	adr.port = (m_Server.port & 0xff) << 8 | (m_Server.port & 0xff00) >> 8;
	adr.type = NA_IP;

	// Set state
	m_Server.received = (int)INFO_REQUESTED;

	// Create query message
	buffer->Clear();
	// Write control sequence
	buffer->WriteLong(0xffffffff);

	// Write query string
	buffer->WriteString("infostring");

	// Sendmessage
	m_pQuery->SendMessage( &adr );

	m_fSendTime = CSocket::GetClock();
	
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerInfo::RunFrame()
{

	float curtime = CSocket::GetClock();
	if(m_fSendTime!=0 && (curtime-m_fSendTime)> 5.0f) // 10 seconds timeout
	{	
		m_fSendTime = 0;
		m_pResponseTarget->ServerFailedToRespond();
	}


	if (m_pQuery)
	{
		m_pQuery->Frame();
	}

}

void CServerInfo::UpdateServer(netadr_t *adr, bool proxy, const char *serverName, const char *map, 
						 const char *gamedir, const char *gameDescription, int players, 
						 int maxPlayers, float recvTime, bool password)
{

	m_Server.received = INFO_RECEIVED;

	m_Server.hadSuccessfulResponse = true;

	// copy in data necessary for filters
	v_strncpy(m_Server.gameDir, gamedir, sizeof(m_Server.gameDir) - 1);
	v_strncpy(m_Server.map, map, sizeof(m_Server.map) - 1);
	v_strncpy(m_Server.name, serverName, sizeof(m_Server.name) - 1);
	v_strncpy(m_Server.gameDescription, gameDescription, sizeof(m_Server.gameDescription) - 1);
	m_Server.players = players;
	m_Server.maxPlayers = maxPlayers;
	m_Server.proxy = proxy;
	m_Server.password = password;


	int ping = (int)((recvTime - m_fSendTime) * 1000);


	if (ping > 3000 || ping < 0)
	{
		// make sure ping is valid
		ping = 1200;
	}

	// add to ping times list 
//	server.pings[0] = server.pings[1];
//	server.pings[1] = server.pings[2];
//	server.pings[2] = ping;

	// calculate ping
//	ping = CalculateAveragePing(server);

	m_Server.ping = ping;


	m_bIsRefreshing=false;
	m_bRefreshed=true;
	m_fSendTime = 0;

	// notify the UI of the new server info
	m_pResponseTarget->ServerResponded();

}

void CServerInfo::Refresh() 
{
	Query();
}

bool CServerInfo::IsRefreshing() 
{

	return m_bIsRefreshing;
}

serveritem_t &CServerInfo::GetServer() 
{
	return m_Server;
}

bool CServerInfo::Refreshed() 
{
	bool val = m_bRefreshed;
	m_bRefreshed=false;

	return val;
}