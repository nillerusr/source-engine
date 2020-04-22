//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "PlayerMsgHandler.h"

#include "playerlist.h"
#include "info.h"
#include "proto_oob.h"

#include "inetapi.h"
#include "DialogGameInfo.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPlayerMsgHandlerDetails::CPlayerMsgHandlerDetails( CPlayerList *baseobject, CUtlVector<Players_t> *players,HANDLERTYPE type, void *typeinfo /*= NULL*/ ) 
	: CMsgHandler( type, typeinfo )
{ 
	m_pPlayerList = baseobject;
	m_pPlayerNames=players;
}

CPlayerMsgHandlerDetails::~CPlayerMsgHandlerDetails()
{
//	delete m_pPlayerNames;
}


//-------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CPlayerMsgHandlerDetails::Process( netadr_t *from, CMsgBuffer *msg )
{

		m_pPlayerNames->RemoveAll();
		m_pPlayerNames->Purge();


		// Check type of data.
		if (msg->ReadByte() != S2A_PLAYER) 
			return false;

		int pNumber;
		pNumber = msg->ReadByte();


		if (pNumber <= 0 || pNumber > 32) // you still need to update the player vector if this happens, the server could be empty
		{
			m_pPlayerList->UpdateServer();
			return false;
		}

		// Read the data
		for (int i = 0; i < pNumber; i++)
		{
			Players_t player;

			memset(&player,0x0,sizeof(Players_t));
			player.userid =     msg->ReadByte();
			v_strncpy(player.name ,msg->ReadString(),100);
			player.frags  =		msg->ReadLong();
			player.time   =		msg->ReadFloat();

			m_pPlayerNames->AddToTail(player);
		}


/*			serveritem_t server;
	memset(&server, 0, sizeof(server));
	netadr_t netaddr;
	if (net->StringToAdr("192.168.1.66", &netaddr))
	{
		memcpy(server.ip,netaddr.ip,4);
	}

	server.port = 27015;

	CDialogGameInfo *gameDialog = new CDialogGameInfo(NULL, 0,*((int *)server.ip),pNumber);
	gameDialog->Run("Stuff");
*/

	m_pPlayerList->UpdateServer();

	return true;
}


