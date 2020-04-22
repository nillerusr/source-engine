//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef REMOTESERVER_H
#define REMOTESERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlLinkedList.h"
#include "igameserverdata.h"

class IServerDataResponse;

//-----------------------------------------------------------------------------
// Purpose: Configures and installs the connection to the game server (remote or local)
//-----------------------------------------------------------------------------
class CRemoteServer
{
public:
	CRemoteServer();
	~CRemoteServer();

	// setup this object 
	void Initialize();

	// remote connection
	void ConnectRemoteGameServer(unsigned int ip, unsigned short port, const char *password);

	// request a cvar/data from the server
	void RequestValue(IServerDataResponse *requester, const char *variable);

	// sets a value
	void SetValue(const char *variable, const char *value);

	// sends a custom command
	void SendCommand(const char *commandString);

	// changes the current password on the server
	// responds with "PasswordChange" "true" or "PasswordChange" "false"
	void ChangeAccessPassword(IServerDataResponse *requester, const char *newPassword);

	// process any return values, firing any IServerDataResponse items
	// returns true if any items were fired
	bool ProcessServerResponse();

	// Adds a constant watches for a particular message.  Used to handle
	// information channels from the server->client (map changed, player joined, etc.)
	void AddServerMessageHandler(IServerDataResponse *handler, const char *watch);

	// removes a requester from the list to guarantee the pointer won't be used
	void RemoveServerDataResponseTarget(IServerDataResponse *invalidRequester);

private:
	int m_iCurrentRequestID;
	ra_listener_id m_ListenerID;
	bool			m_bInitialized;

	// list of all the currently waiting response handlers
	struct ResponseHandler_t
	{
		int requestID;
		IServerDataResponse *handler;
	};
	CUtlLinkedList<ResponseHandler_t, int> m_ResponseHandlers;

	// list of constant response handlers
	struct MessageHandler_t
	{
		char messageName[32];
		IServerDataResponse *handler;
	};
	CUtlLinkedList<MessageHandler_t, int> m_MessageHandlers;

};

//-----------------------------------------------------------------------------
// Purpose: callback interface
//-----------------------------------------------------------------------------
class IServerDataResponse
{
public:
	// called when the server has returned a requested value
	virtual void OnServerDataResponse(const char *value, const char *response) = 0;
};

// singleton accessor
extern CRemoteServer &RemoteServer();


#endif // REMOTESERVER_H
