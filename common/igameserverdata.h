//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IGAMESERVERDATA_H
#define IGAMESERVERDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "netadr.h"

typedef unsigned int ra_listener_id;
const ra_listener_id INVALID_LISTENER_ID = 0xffffffff;


//-----------------------------------------------------------------------------
// Purpose: interface for the dedicated server UI to access the game server data
//			designed to be a simple data parsing interface so that the implementation
//			can be as similar to remote administration as possible
//-----------------------------------------------------------------------------
abstract_class IGameServerData : public IBaseInterface
{
public:
	// writes out a request
	virtual void WriteDataRequest( ra_listener_id listener, const void *buffer, int bufferSize) = 0;

	// returns the number of bytes read
	virtual int ReadDataResponse( ra_listener_id listener, void *buffer, int bufferSize) = 0;

	// get a handle to refer to this connection to the gameserver data interface
	// is authConnection is true the SERVERDATA_AUTH command needs to succeed before other commands
	virtual ra_listener_id GetNextListenerID( bool authConnection = true, const netadr_t *adr = NULL ) = 0;
	// tell the remote access class that this ID is the special dedicated server UI callback (and not an rcon one)
	virtual void RegisterAdminUIID( ra_listener_id listener ) = 0;
};

// enumerations for writing out the requests
enum ServerDataRequestType_t
{
	SERVERDATA_REQUESTVALUE,
	SERVERDATA_SETVALUE,
	SERVERDATA_EXECCOMMAND,
	SERVERDATA_AUTH, // special RCON command to authenticate a connection
	SERVERDATA_VPROF, // subscribe to a vprof stream
	SERVERDATA_REMOVE_VPROF, // unsubscribe from a vprof stream
	SERVERDATA_TAKE_SCREENSHOT,
	SERVERDATA_SEND_CONSOLE_LOG,
};

enum ServerDataResponseType_t
{
	SERVERDATA_RESPONSE_VALUE = 0,
	SERVERDATA_UPDATE,
	SERVERDATA_AUTH_RESPONSE,
	SERVERDATA_VPROF_DATA,
	SERVERDATA_VPROF_GROUPS,
	SERVERDATA_SCREENSHOT_RESPONSE,
	SERVERDATA_CONSOLE_LOG_RESPONSE,
	SERVERDATA_RESPONSE_STRING,
};

/* PACKET FORMAT

REQUEST:
  int requestID;
  int ServerDataRequestType_t;
  NullTerminatedString (variable or command)
  NullTerminatedString (value)

RESPONSE:
  int requestID;
  int ServerDataResponseType_t;
  NullTerminatedString (variable)
  NullTerminatedString (value)

*/

#define GAMESERVERDATA_INTERFACE_VERSION "GameServerData001"


#endif // IGAMESERVERDATA_H
