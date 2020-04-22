//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "RulesInfoMsgHandler.h"

#include "rulesinfo.h"
#include "info.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"
#include "inetapi.h"


extern void v_strncpy(char *dest, const char *src, int bufsize);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRulesInfoMsgHandlerDetails::CRulesInfoMsgHandlerDetails( CRulesInfo *baseobject, HANDLERTYPE type, void *typeinfo /*= NULL*/ ) 
	: CMsgHandler( type, typeinfo )
{ 
	m_pRulesInfo = baseobject;
	m_vRules = new CUtlVector<vgui::KeyValues *>();
}

//-----------------------------------------------------------------------------
// Purpose: Process cracked message
//-----------------------------------------------------------------------------
bool CRulesInfoMsgHandlerDetails::Process( netadr_t *from, CMsgBuffer *msg )
{
	m_vRules->RemoveAll();
	m_vRules->Purge();

	
	// Check type of data.
	if (msg->ReadByte() != S2A_RULES) 
		return false;


	int pNumber;
	pNumber = msg->ReadByte();

	// there is a null string at the start of the rules...
	msg->ReadString();


	// Read the data
	for (int i = 0; i < pNumber; i++)
	{
		vgui::KeyValues *kv=new vgui::KeyValues("rules");
		char *cvar,*value;
		cvar = new char[50];
		value = new char[50];
		v_strncpy(cvar,msg->ReadString(),50);
		v_strncpy(value,msg->ReadString(),50);


		kv->SetString("cvar",cvar);
		kv->SetString("value",value);

		m_vRules->AddToTail(kv);
	}

/*	serveritem_t server;
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


	m_pRulesInfo->UpdateServer(from, m_vRules);
	

	return true;
}


