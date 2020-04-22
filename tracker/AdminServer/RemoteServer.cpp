//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "RemoteServer.h"
#include <assert.h>
#include <stdio.h>

#include "tier1/utlbuffer.h"
#include "IGameServerData.h"
extern IGameServerData *g_pGameServerData;

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CRemoteServer &RemoteServer()
{
	static CRemoteServer s_RemoteServer;	
	return s_RemoteServer;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRemoteServer::CRemoteServer()
{
	m_iCurrentRequestID = 0;
	m_ListenerID = INVALID_LISTENER_ID;
	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRemoteServer::~CRemoteServer()
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
void CRemoteServer::Initialize()
{
	m_bInitialized = true;
	Assert( g_pGameServerData );
	m_ListenerID = g_pGameServerData->GetNextListenerID( false ); // don't require auth on this connection
	g_pGameServerData->RegisterAdminUIID( m_ListenerID );
}

//-----------------------------------------------------------------------------
// Purpose: connects to a remote game server
//-----------------------------------------------------------------------------
void CRemoteServer::ConnectRemoteGameServer(unsigned int ip, unsigned short port, const char *password)
{
	assert(!("CRemoteServer::ConnectRemoteGameServer() not yet implemented"));
}

//-----------------------------------------------------------------------------
// Purpose: request a cvar/data from the server
//-----------------------------------------------------------------------------
void CRemoteServer::RequestValue(IServerDataResponse *requester, const char *variable)
{
	Assert( m_bInitialized );
	// add to the response handling table
	int i = m_ResponseHandlers.AddToTail();
	m_ResponseHandlers[i].requestID = m_iCurrentRequestID;
	m_ResponseHandlers[i].handler = requester;

	// build the command
	char buf[512];
	CUtlBuffer cmd(buf, sizeof(buf));

	cmd.PutInt(m_iCurrentRequestID++);
	cmd.PutInt(SERVERDATA_REQUESTVALUE);
	cmd.PutString(variable);
	cmd.PutString("");

	// send to server
	g_pGameServerData->WriteDataRequest(m_ListenerID, cmd.Base(), cmd.TellPut());
}

//-----------------------------------------------------------------------------
// Purpose: sets a value
//-----------------------------------------------------------------------------
void CRemoteServer::SetValue(const char *variable, const char *value)
{
	Assert( m_bInitialized );
	// build the command
	char buf[512];
	CUtlBuffer cmd(buf, sizeof(buf));

	cmd.PutInt(m_iCurrentRequestID++);
	cmd.PutInt(SERVERDATA_SETVALUE);
	cmd.PutString(variable);
	cmd.PutString(value);

	// send to server
	g_pGameServerData->WriteDataRequest(m_ListenerID, cmd.Base(), cmd.TellPut());
}

//-----------------------------------------------------------------------------
// Purpose: sends a custom command
//-----------------------------------------------------------------------------
void CRemoteServer::SendCommand(const char *commandString)
{
	Assert( m_bInitialized );
	// build the command
	char buf[512];
	CUtlBuffer cmd(buf, sizeof(buf));

	cmd.PutInt(m_iCurrentRequestID++);
	cmd.PutInt(SERVERDATA_EXECCOMMAND);
	cmd.PutString(commandString);
	cmd.PutString("");

	g_pGameServerData->WriteDataRequest(m_ListenerID, cmd.Base(), cmd.TellPut());
}

//-----------------------------------------------------------------------------
// Purpose: changes the current password on the server
//			responds with "PasswordChange" "true" or "PasswordChange" "false"
//-----------------------------------------------------------------------------
void CRemoteServer::ChangeAccessPassword(IServerDataResponse *requester, const char *newPassword)
{
}

//-----------------------------------------------------------------------------
// Purpose: process any return values, firing any IServerDataResponse items
// Output : returns true if any items were fired
//-----------------------------------------------------------------------------
bool CRemoteServer::ProcessServerResponse()
{
	Assert(g_pGameServerData != NULL);
	Assert( m_bInitialized );

	char charbuf[4096];
	bool bProcessedAnyPackets = false;
	while (1)
	{
		// get packet from networking
		int bytesRead = g_pGameServerData->ReadDataResponse(m_ListenerID, charbuf, sizeof(charbuf));
		if (bytesRead < 1)
			break;
		bProcessedAnyPackets = true;

		// parse response
		CUtlBuffer buf(charbuf, bytesRead, CUtlBuffer::READ_ONLY);
		int requestID = buf.GetInt();
		int responseType = buf.GetInt();
		char variable[64];
		buf.GetString(variable);

		switch (responseType)
		{
		case SERVERDATA_RESPONSE_VALUE:
			{
				int valueSize = buf.GetInt();
				Assert(valueSize > 0);
				CUtlBuffer value(0, valueSize);
				if (valueSize > 0)
				{
					value.Put(buf.PeekGet(), valueSize);
				}
				else
				{
					// null terminate
					value.PutChar(0);
				}

				// find callback (usually will be the first one in the list)
				for (int i = m_ResponseHandlers.Head(); m_ResponseHandlers.IsValidIndex(i); i = m_ResponseHandlers.Next(i))
				{
					if (m_ResponseHandlers[i].requestID == requestID)
					{
						// found, call
						m_ResponseHandlers[i].handler->OnServerDataResponse(variable, (const char *)value.Base());

						// remove from list
						m_ResponseHandlers.Remove(i);
						
						// there is only ever one handler for a message
						break;
					}
				}
			}
			break;

		case SERVERDATA_UPDATE:
			{
				// find all the people watching for this message
				for (int i = m_MessageHandlers.Head(); m_MessageHandlers.IsValidIndex(i); i = m_MessageHandlers.Next(i))
				{
					if (!stricmp(m_MessageHandlers[i].messageName, variable))
					{
						// found, call
						m_MessageHandlers[i].handler->OnServerDataResponse(variable, "");

						// keep looking, there can be more than one handler for a message
					}
				}
			}
			break;
		default:
			Assert(responseType == SERVERDATA_RESPONSE_VALUE || responseType == SERVERDATA_UPDATE);
			break;
		}
	}

	return bProcessedAnyPackets;
}

//-----------------------------------------------------------------------------
// Purpose: adds a constant watches for a particular message
//-----------------------------------------------------------------------------
void CRemoteServer::AddServerMessageHandler(IServerDataResponse *handler, const char *watch)
{
	// add to the server message handling table
	int i = m_MessageHandlers.AddToTail();
	strncpy(m_MessageHandlers[i].messageName, watch, sizeof(m_MessageHandlers[i].messageName) - 1);
	m_MessageHandlers[i].messageName[sizeof(m_MessageHandlers[i].messageName) - 1] = 0;
	m_MessageHandlers[i].handler = handler;
}

//-----------------------------------------------------------------------------
// Purpose: removes a requester from the list to guarantee the pointer won't be used
//-----------------------------------------------------------------------------
void CRemoteServer::RemoveServerDataResponseTarget(IServerDataResponse *invalidRequester)
{
	// iterate the responses
	for (int i = 0; i < m_ResponseHandlers.MaxElementIndex(); i++)
	{
		if (m_ResponseHandlers.IsValidIndex(i))
		{
			if (m_ResponseHandlers[i].handler == invalidRequester)
			{
				// found invalid handler, remove from list
				m_ResponseHandlers.Remove(i);
			}
		}
	}
	// iterate the message handlers
	for (int i = 0; i < m_MessageHandlers.MaxElementIndex(); i++)
	{
		if (m_MessageHandlers.IsValidIndex(i))
		{
			if (m_MessageHandlers[i].handler == invalidRequester)
			{
				// found invalid handler, remove from list
				m_MessageHandlers.Remove(i);
			}
		}
	}
}
