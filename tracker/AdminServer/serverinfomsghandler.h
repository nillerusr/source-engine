//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERINFOMSGHANDLERDETAILS_H
#define SERVERINFOMSGHANDLERDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"

class CServerInfo;

//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CServerInfoMsgHandlerDetails : public CMsgHandler
{
public:
	CServerInfoMsgHandlerDetails(CServerInfo *baseobject, HANDLERTYPE type, void *typeinfo = NULL);

	virtual bool Process(netadr_t *from, CMsgBuffer *msg);

private:
	// the parent class that we push info back to
	CServerInfo *m_pServerInfo;
};




#endif // SERVERINFOMSGHANDLERDETAILS_H
