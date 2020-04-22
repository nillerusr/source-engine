//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerInfoMsgHandler.h"

#include "serverinfo.h"
#include "info.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerInfoMsgHandlerDetails::CServerInfoMsgHandlerDetails( CServerInfo *baseobject, HANDLERTYPE type, void *typeinfo /*= NULL*/ ) 
	: CMsgHandler( type, typeinfo )
{ 
	m_pServerInfo = baseobject;
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CServerInfoMsgHandlerDetails::Process( netadr_t *from, CMsgBuffer *msg )
{
	// Skip the control character
	msg->ReadByte();

	// get response name
	const char *str = msg->ReadString();
	if ( !str || !str[0] )
		return false;

	// get infostring
	str = msg->ReadString();
	if ( !str || !str[0] )
		return false;

	char info[ 2048 ];
	strncpy( info, str, 2047 );
	info[2047] = 0;

	char name[256], map[256], gamedir[256], desc[256];

	v_strncpy(name, Info_ValueForKey(info, "hostname"), 255);
	v_strncpy(map, Info_ValueForKey(info, "map"), 255);
	v_strncpy(gamedir, Info_ValueForKey(info, "gamedir"), 255);
	strlwr(gamedir);
	v_strncpy(desc, Info_ValueForKey(info, "description"), 255);
	int players = atoi(Info_ValueForKey(info, "players"));
	int maxplayers = atoi(Info_ValueForKey(info, "max"));
	char serverType = *Info_ValueForKey(info, "type");
	bool password = atoi(Info_ValueForKey(info, "password"));

	m_pServerInfo->UpdateServer(from,	// index of server
					(serverType == 'p'),
					name,			
					map,	
					gamedir,
					desc,
					players,
					maxplayers,
					msg->GetTime(),		// receive time
					password
					);
	

	return true;
}


