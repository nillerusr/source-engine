//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MASTERMSGHANDLER_H
#define MASTERMSGHANDLER_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"

//-----------------------------------------------------------------------------
// Purpose: Socket handler for lan broadcast pings
//-----------------------------------------------------------------------------
class CMasterMsgHandler : public CMsgHandler
{
public:
	CMasterMsgHandler( IGameList *baseobject, HANDLERTYPE type, void *typeinfo = NULL );

	virtual bool Process( netadr_t *from, CMsgBuffer *msg );

private:
	IGameList *m_pGameList;
};



#endif // MASTERMSGHANDLER_H
