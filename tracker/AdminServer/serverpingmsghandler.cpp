//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerPingMsgHandler.h"

#include "ServerPing.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerPingMsgHandlerDetails::CServerPingMsgHandlerDetails( CServerPing *baseobject, HANDLERTYPE type, void *typeinfo /*= NULL*/ ) 
	: CMsgHandler( type, typeinfo )
{ 
	m_pServerPing = baseobject;
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CServerPingMsgHandlerDetails::Process( netadr_t *from, CMsgBuffer *msg )
{

	m_pServerPing->UpdateServer(msg->GetTime());
	return true;
}


