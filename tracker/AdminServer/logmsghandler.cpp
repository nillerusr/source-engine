//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LogMsgHandler.h"

#include "info.h"
#include "proto_oob.h"

#include "inetapi.h"
#include "DialogGameInfo.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLogMsgHandlerDetails::CLogMsgHandlerDetails(IResponse *baseobject, HANDLERTYPE type, void *typeinfo ) 
	: CMsgHandler( type, typeinfo )
{ 
	m_pLogList = baseobject;
	m_bNewMessage=false;
}

CLogMsgHandlerDetails::~CLogMsgHandlerDetails()
{
}


//-------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CLogMsgHandlerDetails::Process( netadr_t *from, CMsgBuffer *msg )
{

	m_bNewMessage=true;
	v_strncpy(message,msg->ReadString(),512);
	message[strlen(message)-1]='\n';
	message[strlen(message)]='\0';

	// now tell the UI we have this new message	
	m_pLogList->ServerResponded();

	return true;
}

//-------------------------------------------------------------------------
// Purpose: returns if a new message is waiting
//-----------------------------------------------------------------------------
bool CLogMsgHandlerDetails::NewMessage()
{
	bool val=m_bNewMessage;
	m_bNewMessage=false;
	return val;
}

//-------------------------------------------------------------------------
// Purpose: returns the text contained in the last message recieved
//-----------------------------------------------------------------------------
const char *CLogMsgHandlerDetails::GetBuf()
{
	return message;
}