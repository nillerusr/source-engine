//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef LOGMSGHANDLER_H
#define LOGMSGHANDLER_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"
#include "utlvector.h"
#include "player.h"
#include "rcon.h"


//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CLogMsgHandlerDetails : public CMsgHandler
{
public:
	CLogMsgHandlerDetails(IResponse *baseobject, HANDLERTYPE type, void *typeinfo = NULL);
	~CLogMsgHandlerDetails();

	virtual bool Process(netadr_t *from, CMsgBuffer *msg);
	
	// indicates if a new message has arrived
	bool NewMessage();

	// returns the text of the last message recieved
	const char *GetBuf();


private:

	IResponse *m_pLogList;
	bool m_bNewMessage;
	char message[512];
};

#endif // LOGMSGHANDLER_H
