//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "rulesinfo.h"
#include "Iresponse.h"

#include "RulesInfoMsgHandler.h"
#include "Socket.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

CRulesInfo::CRulesInfo(IResponse *target,serveritem_t &server) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;

	m_bIsRefreshing=false;

	m_vRules=NULL;

	int bytecode = S2A_RULES;
	m_pQuery = new CSocket("internet rules query", -1);
	m_pQuery->AddMessageHandler(new CRulesInfoMsgHandlerDetails(this, CMsgHandler::MSGHANDLER_ALL, &bytecode));
}

CRulesInfo::~CRulesInfo() {
	delete m_pQuery;

}


//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CRulesInfo::Query()
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
	buffer->WriteString("rules");

	// Sendmessage
	m_pQuery->SendMessage( &adr, buffer );

}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRulesInfo::RunFrame()
{
	if (m_pQuery)
	{
		m_pQuery->Frame();
	}

}

void CRulesInfo::UpdateServer(netadr_t *adr, CUtlVector<vgui::KeyValues *> *Rules)
{

	m_Server.hadSuccessfulResponse = true;

	m_vRules=Rules;

	m_bIsRefreshing=false;
	m_bRefreshed=true;

	// notify the UI of the new server info
	m_pResponseTarget->ServerResponded();

}

void CRulesInfo::Refresh() 
{
	Query();
}

bool CRulesInfo::IsRefreshing() 
{
	return m_bIsRefreshing;
}

serveritem_t &CRulesInfo::GetServer() 
{
	return m_Server;
}

bool CRulesInfo::Refreshed() 
{
	bool val = m_bRefreshed;
	m_bRefreshed=false;

	return val;
}

CUtlVector<vgui::KeyValues *> *CRulesInfo::Rules() 
{
	return m_vRules;
}