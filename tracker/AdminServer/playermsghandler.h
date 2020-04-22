//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERMSGHANDLER_H
#define PLAYERMSGHANDLER_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"
#include "utlvector.h"
#include "player.h"
#include "rcon.h"

class CPlayerList;


//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CPlayerMsgHandlerDetails : public CMsgHandler
{
public:
	CPlayerMsgHandlerDetails(CPlayerList *baseobject, CUtlVector<Players_t> *players,HANDLERTYPE type, void *typeinfo = NULL);
	~CPlayerMsgHandlerDetails();

	virtual bool Process(netadr_t *from, CMsgBuffer *msg);


private:
	// the parent class that we push info back to
	CPlayerList *m_pPlayerList;
	CUtlVector<Players_t> *m_pPlayerNames;

};

#endif // PLAYERMSGHANDLER_H
