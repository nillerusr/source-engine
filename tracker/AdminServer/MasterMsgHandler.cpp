//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "MasterMsgHandler.h"

#include "IGameList.h"
#include "serverpage.h"

#include <VGUI_Controls.h>
#include <VGUI_IVGui.h>

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMasterMsgHandler::CMasterMsgHandler( IGameList *baseobject, HANDLERTYPE type, void *typeinfo /*= NULL*/ ) 
	: CMsgHandler( type, typeinfo )
{
	m_pGameList = baseobject;
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CMasterMsgHandler::Process( netadr_t *from, CMsgBuffer *msg )
{
	// Skip the control character
	msg->ReadByte();
	// Skip the return character
	msg->ReadByte();

	// Get the batch ID
	int unique = msg->ReadLong();

	// Remainder of message length is 6 byte IP addresses
	int nNumAddresses =  msg->GetCurSize() - msg->GetReadCount();
	assert( !( nNumAddresses % 6 ) );
	// Each address is 6 bytes long
	nNumAddresses /= 6;

	while (nNumAddresses-- > 0)
	{
		serveritem_t server;
		memset(&server, 0, sizeof(server));
		for (int i = 0; i < 4; i++)
		{
			server.ip[i] = msg->ReadByte();
		}
		server.port = msg->ReadShort();
		server.port = (server.port & 0xff) << 8 | (server.port & 0xff00) >> 8; // roll your own ntohs
		server.received = 0;
		server.listEntry = NULL;
		server.doNotRefresh = false;
		server.hadSuccessfulResponse = false;
		server.map[0] = 0;
		server.gameDir[0] = 0;

		m_pGameList->AddNewServer(server);
	}

	if (!unique)
	{
		m_pGameList->ListReceived(false, 0);
	}
	else
	{
		// wait until we've refreshed the list before getting next batch of servers
		m_pGameList->ListReceived(true, unique);
	}

	return true;
}

