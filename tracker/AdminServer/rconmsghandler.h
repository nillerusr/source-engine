//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef RCONMSGHANDLERDETAILS_H
#define RCONMSGHANDLERDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"

class CRcon;

//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CRconMsgHandlerDetails : public CMsgHandler
{
public:
	CRconMsgHandlerDetails(CRcon *baseobject, HANDLERTYPE type, void *typeinfo = NULL);

	virtual bool Process(netadr_t *from, CMsgBuffer *msg);

private:
	// the parent class that we push info back to
	CRcon *m_pRcon;
};




#endif // RCONMSGHANDLERDETAILS_H
