//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "ServerPing.h"
#include "Iresponse.h"

#include "ServerPingMsgHandler.h"
#include "Socket.h"
#include "proto_oob.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

CServerPing::CServerPing(IResponse *target,serveritem_t &server) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;

	m_bIsRefreshing=false;

	int bytecode = S2A_INFO_DETAILED;
	m_pQuery = new CSocket("internet server ping", -1);
	m_pQuery->AddMessageHandler(new CServerPingMsgHandlerDetails(this, CMsgHandler::MSGHANDLER_ALL, &bytecode));
	m_fSendTime=0;
}

CServerPing::~CServerPing() {
	delete m_pQuery;

}


//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CServerPing::Query()
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

	// Create query message
	buffer->Clear();
	// Write control sequence
	buffer->WriteLong(0xffffffff);

	// Write query string
	buffer->WriteString("ping");

	// Sendmessage
	m_pQuery->SendMessage( &adr );

	m_fSendTime = CSocket::GetClock();
	
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerPing::RunFrame()
{
/*	float curtime = CSocket::GetClock();
	if(m_fSendTime!=0 && (curtime-m_fSendTime)> 5.0f) // 10 seconds timeout
	{	
		m_fSendTime = 0;
		m_pResponseTarget->ServerFailedToRespond();
	}
*/

	if (m_pQuery)
	{
		m_pQuery->Frame();
	}

}

void CServerPing::UpdateServer(float recvTime)
{

	//A2A_ACK
	int ping = (int)((recvTime - m_fSendTime) * 1000);


	if (ping > 10000 || ping < 0)
	{
		// make sure ping is valid
		ping = 1200;
	}

	m_Server.ping = ping;


	m_bIsRefreshing=false;
	m_bRefreshed=true;
	m_fSendTime=0;

	// notify the UI of the new server info
	m_pResponseTarget->ServerResponded();

}

void CServerPing::Refresh() 
{
	Query();
}

bool CServerPing::IsRefreshing() 
{

	return m_bIsRefreshing;
}

serveritem_t &CServerPing::GetServer() 
{
	return m_Server;
}

bool CServerPing::Refreshed() 
{
	bool val = m_bRefreshed;
	m_bRefreshed=false;

	return val;
}