//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the networking code in the game 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "networksystem/inetworksystem.h"
#include "inetworkmessagelistener.h"


//-----------------------------------------------------------------------------
// Default ports
//-----------------------------------------------------------------------------
enum
{
	LEGION_DEFAULT_SERVER_PORT = 0xFFFF,
	LEGION_DEFAULT_CLIENT_PORT = 0xFFFF,
};


//-----------------------------------------------------------------------------
// Interface used to create network messages
//-----------------------------------------------------------------------------
abstract_class INetworkMessageFactory
{
public:
	// Creates the menu
	virtual INetworkMessage *CreateNetworkMessage( ) = 0;
	
	// Used to build a list during construction
	virtual INetworkMessageFactory *GetNextFactory( ) = 0;

protected:
	virtual ~INetworkMessageFactory() {}
};


//-----------------------------------------------------------------------------
// Network management system
//-----------------------------------------------------------------------------
class CNetworkManager : public CGameManager<>
{
public:
	// Inherited from IGameManager
	virtual bool Init();
	virtual void Update( );
	virtual void Shutdown();

	// Are we a client?/Are we a server? (we can be both)
	bool IsClient();
	bool IsServer();

	// If we are a client, are we connected to the server?
	bool IsClientConnected();

	// Start a server up
	bool StartServer( unsigned short nServerListenPort = LEGION_DEFAULT_SERVER_PORT );
	void ShutdownServer( );

	// Start a client up
	bool StartClient( unsigned short nClientListenPort = LEGION_DEFAULT_CLIENT_PORT );
	void ShutdownClient( );

	// Connects the client to a server
	bool ConnectClientToServer( const char *pServerName, unsigned short nServerListenPort = LEGION_DEFAULT_SERVER_PORT );
	void DisconnectClientFromServer();

	// Add, remove network message listeners
	void AddListener( NetworkMessageRoute_t route, int nGroup, int nType, INetworkMessageListener *pListener );
	void RemoveListener( NetworkMessageRoute_t route, int nGroup, int nType, INetworkMessageListener *hListener );

	// Post a network message to the server
	void PostClientToServerMessage( INetworkMessage *pMessage );

	// Broadcast a network message to all clients
	void BroadcastServerToClientMessage( INetworkMessage *pMessage );

	// Start up a game where the local player is playing
	bool HostGame();
	void StopHostingGame();

	// Call to register methods which can construct menus w/ particular ids
	// NOTE: This method is not expected to be called directly. Use the REGISTER_NETWORK_MESSAGE macro instead
	// It returns the previous head of the list of factories
	static INetworkMessageFactory* RegisterNetworkMessage( INetworkMessageFactory *pMenuFactory );

private:
	// Process messages received by the client
	void ProcessClientMessages();

	// Process messages received by the server
	void ProcessServerMessages();

	// Notifies listeners about a network message
	void NotifyListeners( NetworkMessageRoute_t route, INetworkMessage *pMessage );

	bool m_bIsClient;
	bool m_bIsServer;

	INetChannel *m_pClientToServerConnection;
	CUtlVector< INetChannel* > m_ServerToClientConnection;
	CUtlVector< CUtlVector< CUtlVector < INetworkMessageListener* > > > m_Listeners[ NETWORK_MESSAGE_ROUTE_COUNT ];
	static INetworkMessageFactory* m_pFirstFactory;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CNetworkManager *g_pNetworkManager;


//-----------------------------------------------------------------------------
// Macro used to register menus with the menu system
// For example, add the line REGISTER_NETWORK_MESSAGE( CNetworkMessageChat );
// into the file defining the chat message
//-----------------------------------------------------------------------------
template < class T >
class CNetworkMessageFactory : public INetworkMessageFactory
{
public:
	CNetworkMessageFactory( )
	{
		m_pNextFactory = CNetworkManager::RegisterNetworkMessage( this );
	}

	// Creates the network message
	virtual INetworkMessage *CreateNetworkMessage( )
	{
		return new T;
	}

	// Used to build a list during construction
	virtual INetworkMessageFactory *GetNextFactory( )
	{
		return m_pNextFactory;
	}

private:
	INetworkMessage *m_pMessage;
	INetworkMessageFactory *m_pNextFactory;
};

#define REGISTER_NETWORK_MESSAGE( _className )	\
	static CNetworkMessageFactory< _className > __s_Factory ## _className


#endif // NETWORKMANAGER_H

