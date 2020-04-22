//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef INETWORKSYSTEM_H
#define INETWORKSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "appframework/IAppSystem.h"


//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class INetworkMessageHandler;
class INetworkMessage;
class INetChannel;
class INetworkMessageFactory;
class bf_read;
class bf_write;
typedef struct netadr_s netadr_t;
class CNetPacket;


//-----------------------------------------------------------------------------
// Default ports
//-----------------------------------------------------------------------------
enum
{
	NETWORKSYSTEM_DEFAULT_SERVER_PORT = 27001,
	NETWORKSYSTEM_DEFAULT_CLIENT_PORT = 27002
};


//-----------------------------------------------------------------------------
// This interface encompasses a one-way communication path between two
//-----------------------------------------------------------------------------
typedef int ConnectionHandle_t;

enum ConnectionStatus_t
{
	CONNECTION_STATE_DISCONNECTED = 0,
	CONNECTION_STATE_CONNECTING,
	CONNECTION_STATE_CONNECTION_FAILED,
	CONNECTION_STATE_CONNECTED,
};


//-----------------------------------------------------------------------------
// This interface encompasses a one-way communication path between two machines
//-----------------------------------------------------------------------------
abstract_class INetChannel
{
public:
//	virtual INetworkMessageHandler *GetMsgHandler( void ) const = 0;
	virtual const netadr_t	&GetRemoteAddress( void ) const = 0;

	// send a net message
	// NOTE: There are special connect/disconnect messages?
	virtual bool AddNetMsg( INetworkMessage *msg, bool bForceReliable = false ) = 0; 
//	virtual bool RegisterMessage( INetworkMessage *msg ) = 0;

	virtual ConnectionStatus_t GetConnectionState( ) = 0;

/*
	virtual ConnectTo( const netadr_t& to ) = 0;
	virtual Disconnect() = 0;

	virtual const netadr_t& GetLocalAddress() = 0;

	virtual const netadr_t& GetRemoteAddress() = 0;
*/
};


//-----------------------------------------------------------------------------
// Network event types + structures
//-----------------------------------------------------------------------------
enum NetworkEventType_t
{
	NETWORK_EVENT_CONNECTED = 0,
	NETWORK_EVENT_DISCONNECTED,
	NETWORK_EVENT_MESSAGE_RECEIVED,
};

struct NetworkEvent_t
{
	NetworkEventType_t m_nType;
};

struct NetworkConnectionEvent_t : public NetworkEvent_t
{
	INetChannel *m_pChannel;
};

struct NetworkDisconnectionEvent_t : public NetworkEvent_t
{
	INetChannel *m_pChannel;
};

struct NetworkMessageReceivedEvent_t : public NetworkEvent_t
{
	INetChannel *m_pChannel;
	INetworkMessage *m_pNetworkMessage;
};


//-----------------------------------------------------------------------------
// Main interface for low-level networking (packet sending). This is a low-level interface
//-----------------------------------------------------------------------------
#define NETWORKSYSTEM_INTERFACE_VERSION	"NetworkSystemVersion001"
abstract_class INetworkSystem : public IAppSystem
{
public:
	// Installs network message factories to be used with all connections
	virtual bool RegisterMessage( INetworkMessage *msg ) = 0;

	// Start, shutdown a server
	virtual bool StartServer( unsigned short nServerListenPort = NETWORKSYSTEM_DEFAULT_SERVER_PORT ) = 0;
	virtual void ShutdownServer( ) = 0;

	// Process server-side network messages
	virtual void ServerReceiveMessages() = 0;
	virtual void ServerSendMessages() = 0;

	// Start, shutdown a client
	virtual bool StartClient( unsigned short nClientListenPort = NETWORKSYSTEM_DEFAULT_CLIENT_PORT ) = 0;
	virtual void ShutdownClient( ) = 0;

	// Process client-side network messages
	virtual void ClientSendMessages() = 0;
	virtual void ClientReceiveMessages() = 0;

	// Connect, disconnect a client to a server
	virtual INetChannel* ConnectClientToServer( const char *pServer, int nServerListenPort = NETWORKSYSTEM_DEFAULT_SERVER_PORT ) = 0;
	virtual void DisconnectClientFromServer( INetChannel* pChan ) = 0;

	// Event queue
	virtual NetworkEvent_t *FirstNetworkEvent( ) = 0;
	virtual NetworkEvent_t *NextNetworkEvent( ) = 0;

	// Returns the local host name
	virtual const char* GetLocalHostName( void ) const = 0;
	virtual const char* GetLocalAddress( void ) const = 0;

	/*
	// NOTE: Server methods
	// NOTE: There's only 1 client INetChannel ever
	// There can be 0-N server INetChannels.
	virtual INetChannel* CreateConnection( bool bIsClientConnection ) = 0;

	// Add methods for setting unreliable payloads
	*/
};


#endif // INETWORKSYSTEM_H
