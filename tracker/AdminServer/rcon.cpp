//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "rcon.h"
#include "Iresponse.h"

#include "RconMsgHandler.h"
#include "Socket.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"
#include "inetapi.h"
#include "dialogkickplayer.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

typedef enum
{
	NONE = 0,
	INFO_REQUESTED,
	INFO_RECEIVED
} RCONSTATUS;


CRcon::CRcon(IResponse *target,serveritem_t &server,const char *password) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;

	m_bIsRefreshing	=	false;
	m_bChallenge	=	false;
	m_bNewRcon		=	false;
	m_bPasswordFail	=	false;
	m_bDisable		=	false;
	m_bGotChallenge	=	false;
	m_iChallenge	=	0;
	m_fQuerySendTime=	0;

	v_strncpy(m_sPassword,password,100);

	int bytecode = S2A_INFO_DETAILED;
	m_pQuery = new CSocket("internet rcon query", -1);
	m_pQuery->AddMessageHandler(new CRconMsgHandlerDetails(this, CMsgHandler::MSGHANDLER_ALL, &bytecode));
}

CRcon::~CRcon() {
	delete m_pQuery;
}

//-----------------------------------------------------------------------------
// Purpose: resets the state of the rcon object back to startup conditions (i.e get challenge again)
//-----------------------------------------------------------------------------
void CRcon::Reset() 
{
	m_bIsRefreshing	=	false;
	m_bChallenge	=	false;
	m_bNewRcon		=	false;
	m_bPasswordFail	=	false;
	m_bDisable		=	false;
	m_bGotChallenge	=	false;
	m_iChallenge	=	0;
	m_fQuerySendTime=	0;
}


//-----------------------------------------------------------------------------
// Purpose: sends a challenge request to the server if we have yet to get one,
//			else it sends the rcon itself
//-----------------------------------------------------------------------------
void CRcon::SendRcon(const char *command)
{

	if(m_bDisable==true) // rcon is disabled because it has failed. 
	{
		m_pResponseTarget->ServerFailedToRespond();
		return;
	}

	if(m_bIsRefreshing)
	{ // we are already processing a request, lets queue this
		queue_requests_t queue;
		strncpy(queue.queued,command,1024);
		
		if(requests.Count()>10)  // to many request already...
			return;

		requests.AddToTail(queue);
		return;
	}

	m_bIsRefreshing=true;
	m_bPasswordFail=false;

	if(m_bGotChallenge==false) // haven't got the challenge id yet
	{
		GetChallenge();
		v_strncpy(m_sCmd,command,1024); // store away the command for later :)
		m_bChallenge=true; // note that we are requesting a challenge and need to still run this command
	} 
	else
	{
		RconRequest(command,m_iChallenge);
	}

}



//-----------------------------------------------------------------------------
// Purpose: runs a frame of the net handler
//-----------------------------------------------------------------------------
void CRcon::RunFrame()
{
	// only the "ping" command has a timeout
/*	float curtime = CSocket::GetClock();
	if(m_fQuerySendTime!=0 && (curtime-m_fQuerySendTime)> 10.0f) // 10 seconds
	{	
		m_fQuerySendTime=	0;
		m_pResponseTarget->ServerFailedToRespond();
	}
	*/

	if (m_pQuery)
	{
		m_pQuery->Frame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: emits a challenge request
//-----------------------------------------------------------------------------
void CRcon::GetChallenge() 
{
	CMsgBuffer *buffer = m_pQuery->GetSendBuffer();
	assert( buffer );
	
	if ( !buffer ) 
	{
		return;
	}
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
	buffer->WriteString("challenge rcon");
		// Sendmessage
	m_pQuery->SendMessage( &adr );

	// set the clock for this send
	m_fQuerySendTime	= CSocket::GetClock();
}


//-----------------------------------------------------------------------------
// Purpose: emits a valid rcon request, the challenge id has already been found
//-----------------------------------------------------------------------------
void CRcon::RconRequest(const char *command, int challenge) 
{
	CMsgBuffer *buffer = m_pQuery->GetSendBuffer();
	assert( buffer );
	
	if ( !buffer ) 
	{
		return;
	}
	
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
	char rcon_cmd[600];
	_snprintf(rcon_cmd,600,"rcon %u \"%s\" %s",challenge,m_sPassword,command);

	buffer->WriteString(rcon_cmd);
	// Sendmessage
	m_pQuery->SendMessage( &adr );
	// set the clock for this send
	m_fQuerySendTime	= CSocket::GetClock();
}

//-----------------------------------------------------------------------------
// Purpose: called when an rcon responds
//-----------------------------------------------------------------------------
void CRcon::UpdateServer(netadr_t *adr, int challenge, const char *resp)
{

	m_fQuerySendTime=	0;
	if(m_bChallenge==true)  // now send the RCON request itself
	{ 
		m_bChallenge=false; // m_bChallenge is set to say we just requested the challenge value
		m_iChallenge=challenge;
		m_bGotChallenge=true;
		RconRequest(m_sCmd,m_iChallenge);
	} 
	else  // this is the result of the RCON request
	{ 
		m_bNewRcon=true;
		v_strncpy(m_sRconResponse,resp,2048);
		m_bIsRefreshing=false;

		// this must be before the SeverResponded() :)
		if(requests.Count()>0)
		{ // we have queued requests
			SendRcon(requests[0].queued);
			requests.Remove(0); // now delete this element
		}
		
		
		// notify the UI of the new server info
		m_pResponseTarget->ServerResponded();

	}

}

//-----------------------------------------------------------------------------
// Purpose: run when a refresh is asked for
//-----------------------------------------------------------------------------
void CRcon::Refresh() 
{
	
}

//-----------------------------------------------------------------------------
// Purpose: returns if a rcon is currently being performed
//-----------------------------------------------------------------------------
bool CRcon::IsRefreshing() 
{

	return m_bIsRefreshing;
}

//-----------------------------------------------------------------------------
// Purpose: the server to which this rcon class is bound
//-----------------------------------------------------------------------------
serveritem_t &CRcon::GetServer() 
{
	return m_Server;
}

//-----------------------------------------------------------------------------
// Purpose: the challenge id used in rcons
//-----------------------------------------------------------------------------
bool CRcon::Challenge() 
{
	return m_bChallenge;
}

//-----------------------------------------------------------------------------
// Purpose: returns if a new rcon result is available
//-----------------------------------------------------------------------------
bool CRcon::NewRcon()
{
	bool val = m_bNewRcon;
	m_bNewRcon=false;

	return val;
}

//-----------------------------------------------------------------------------
// Purpose: returns the response of the last rcon
//-----------------------------------------------------------------------------
const char *CRcon::RconResponse()
{
	return (const char *)m_sRconResponse;
}

//-----------------------------------------------------------------------------
// Purpose: called when the wrong password is used, when a ServerFailed is called
//-----------------------------------------------------------------------------
bool CRcon::PasswordFail()
{
	bool val=m_bPasswordFail;
	m_bPasswordFail=false;
	return val;
	//m_pResponseTarget->ServerFailedToRespond();
}

//-----------------------------------------------------------------------------
// Purpose: called by the rcon message handler to denote a bad password
//-----------------------------------------------------------------------------
void CRcon::BadPassword(const char *info) 
{
	strncpy(m_sRconResponse,info,100);
	m_bPasswordFail=true;
	m_bDisable=true;
	m_fQuerySendTime=	0;

	m_bIsRefreshing=false;

/*	// this must be before the ServerFailedToRespond() :)
	if(requests.Count()>0)
	{ // we have queued requests
		SendRcon(requests[0].queued);
		requests.Remove(0); // now delete this element
	}
*/

	m_pResponseTarget->ServerFailedToRespond();
}

//-----------------------------------------------------------------------------
// Purpose: return whether rcon has been disabled (due to bad passwords)
//-----------------------------------------------------------------------------
bool CRcon::Disabled() 
{
	return m_bDisable;
}

//-----------------------------------------------------------------------------
// Purpose: sets the password to use for rcons
//-----------------------------------------------------------------------------
void CRcon::SetPassword(const char *newPass) 
{
	strncpy(m_sPassword,newPass,100);
	m_bDisable=false; // new password, so we can try again
}

