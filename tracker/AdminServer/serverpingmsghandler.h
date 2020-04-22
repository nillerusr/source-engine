//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERPINGMSGHANDLERDETAILS_H
#define SERVERPINGMSGHANDLERDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"

class CServerPing;

//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CServerPingMsgHandlerDetails : public CMsgHandler
{
public:
	CServerPingMsgHandlerDetails(CServerPing *baseobject, HANDLERTYPE type, void *typeinfo = NULL);

	virtual bool Process(netadr_t *from, CMsgBuffer *msg);

private:
	// the parent class that we push info back to
	CServerPing *m_pServerPing;
};




#endif // SERVERPINGMSGHANDLERDETAILS_H
