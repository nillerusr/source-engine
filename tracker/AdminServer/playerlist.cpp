//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "playerlist.h"
#include "Iresponse.h"

#include "PlayerMsgHandler.h"
#include "Socket.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"
#include "inetapi.h"
#include "TokenLine.h"
#include "dialogkickplayer.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

typedef enum
{
	NONE = 0,
	INFO_REQUESTED,
	INFO_RECEIVED
} RCONSTATUS;

CPlayerList::CPlayerList(IResponse *target,serveritem_t &server, const char *rconPassword) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;

	m_bIsRefreshing=false;
	m_bNewPlayerList=false;
	m_bRconFailed=false;

	v_strncpy(m_szRconPassword,rconPassword,100);
	
	m_pRcon = new CRcon(this  , server,rconPassword);
	m_PlayerList=NULL;
	m_pPlayerInfoMsg=NULL;

	m_pQuery = new CSocket("internet player query", -1);

}

CPlayerList::~CPlayerList() {
	delete m_pQuery;
	delete m_pRcon;
}


//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CPlayerList::SendQuery()
{
	CMsgBuffer *buffer = m_pQuery->GetSendBuffer();
	assert( buffer );
	
	if ( !buffer ) 
	{
		return;
	}

	int bytecode = S2A_PLAYER;
	if ( m_pPlayerInfoMsg != NULL )
	{
		delete m_pPlayerInfoMsg;
	}
	m_pPlayerInfoMsg=new CPlayerMsgHandlerDetails(this, &m_PlayerList,CMsgHandler::MSGHANDLER_ALL, &bytecode);
	m_pQuery->AddMessageHandler(m_pPlayerInfoMsg);

	m_bIsRefreshing=true;

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
	buffer->WriteString("players");

	// Sendmessage
	m_pQuery->SendMessage( &adr );
	
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerList::RunFrame()
{
	if (m_pQuery && m_bIsRefreshing)
	{
		m_pQuery->Frame();
	}
	if(m_pRcon) 
	{
		m_pRcon->RunFrame();
	}

}


void CPlayerList::ServerResponded()
{
	const char *rconResp = m_pRcon->RconResponse();
	const char *cur = strstr(rconResp,"name userid uniqueid frag time ping loss adr\n")
							+ strlen("name userid uniqueid frag time ping loss adr\n");

// status format:
//	#      name userid uniqueid frag time ping loss adr
//  # 1 "Player" 1 4294967295   0 30:56   22    0 192.168.3.64:27005


	for(int i=0;i<m_PlayerList.Count();i++) 
	{
		if(cur!=NULL) 
		{	
			TokenLine playerLine;
			playerLine.SetLine(cur);
		
			if(playerLine.CountToken() >= 9 ) 
			{
				//	playerLine.GetToken(1); // count
				//	playerLine.GetToken(2); // player name
				//	char *player=	playerLine.GetToken(2);

				m_PlayerList[i].userid=atoi(playerLine.GetToken(3));  // userid
				v_strncpy(m_PlayerList[i].authid,playerLine.GetToken(4),20); // authid
				//playerLine.GetToken(5); // frag
				//	playerLine.GetToken(6); // time
				m_PlayerList[i].ping=atoi(playerLine.GetToken(7)); //ping
				m_PlayerList[i].loss=atoi(playerLine.GetToken(8)); // loss
				//	playerLine.GetToken(9); // adr
			}
			cur=strchr(cur,'\n')+1;

		}
	}
	
	m_bNewPlayerList=true;
	m_bIsRefreshing=false;

	// notify the UI of the new server info
	m_pResponseTarget->ServerResponded();

}

void CPlayerList::ServerFailedToRespond()
{
	m_bNewPlayerList=true;
	m_bIsRefreshing=false;

	if(m_bRconFailed==false)
	{
		m_bRconFailed=true;
	//	CDialogKickPlayer *box = new CDialogKickPlayer();
		//box->addActionSignalTarget(this);
	//	box->Activate("","Bad Rcon Password","badrcon");
	}

	// rcon failed BUT we still have some valid data :)
	m_pResponseTarget->ServerResponded();
}


void CPlayerList::UpdateServer()
{

	m_pQuery->RemoveMessageHandler(m_pPlayerInfoMsg);
	// you CANNOT delete this handler because we are inside of it at the moment... (yes, this was an ugly bug)
	//	delete m_pPlayerInfoMsg;

	// now use "rcon status" to pull extra info about the players
	m_pRcon->SendRcon("status");

}

void CPlayerList::Refresh() 
{
	SendQuery();
}

bool CPlayerList::IsRefreshing() 
{

	return m_bIsRefreshing;
}

serveritem_t &CPlayerList::GetServer() 
{
	return m_Server;
}


bool CPlayerList::NewPlayerList() 
{
	return m_bNewPlayerList;
}

CUtlVector<Players_t> *CPlayerList::GetPlayerList() 
{
	m_bNewPlayerList=false;
	return &m_PlayerList;
}

void CPlayerList::SetPassword(const char *newPass) 
{
	m_pRcon->SetPassword(newPass);
	m_bRconFailed=false;
}