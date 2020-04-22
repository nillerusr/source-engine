//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "RconMsgHandler.h"

#include "rcon.h"
#include "info.h"

#include "DialogGameInfo.h"
#include "inetapi.h"
#include "proto_oob.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRconMsgHandlerDetails::CRconMsgHandlerDetails( CRcon *baseobject, HANDLERTYPE type, void *typeinfo /*= NULL*/ ) 
	: CMsgHandler( type, typeinfo )
{ 
	m_pRcon = baseobject;
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CRconMsgHandlerDetails::Process( netadr_t *from, CMsgBuffer *msg )
{
	// Skip the control character
	//if (!m_pRcon->Challenge() && msg->ReadByte()!=A2C_PRINT) 
	//	return false;

	// get response name
	const char *str = msg->ReadString();
	if ( !str || !str[0] )
		return false;

	char info[ 2048 ];
	v_strncpy( info, str, 2047 );
	info[2047] = 0;

	if(!_strnicmp(info,"lBad rcon_password",18) ) 
	{
		m_pRcon->BadPassword(info+1);
		return true;
	}


	if(m_pRcon->Challenge() ) 
	{ // if we know the challenge value pass it to UpdateServer

		int challenge = atoi(info+strlen("challenge rcon"));
		m_pRcon->UpdateServer(from,	challenge, 	NULL);
	} 
	else // else pass a default value
	{
		m_pRcon->UpdateServer(from,	-1,	info+1 );
	}

	return true;
}


