//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the networking code in the game 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "networkmanager.h"
#include "legion.h"
#include "networksystem/inetworkmessage.h"
#include "tier1/bitbuf.h"
#include "inetworkmessagelistener.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CNetworkManager s_NetworkManager;
extern CNetworkManager *g_pNetworkManager = &s_NetworkManager;


//-----------------------------------------------------------------------------
// Static members.
// NOTE: Do *not* set this to 0; it could cause us to lose some registered
// menus since that list is set up during construction
//-----------------------------------------------------------------------------
INetworkMessageFactory *CNetworkManager::m_pFirstFactory;


//-----------------------------------------------------------------------------
// Call to register methods to register network messages
//-----------------------------------------------------------------------------
INetworkMessageFactory *CNetworkManager::RegisterNetworkMessage( INetworkMessageFactory *pNetworkMessageFactory )
{
	// NOTE: This method is expected to be called during global constructor
	// time, so it must not require any global constructors to be called to work
	INetworkMessageFactory *pPrevFactory = m_pFirstFactory;
	m_pFirstFactory = pNetworkMessageFactory;
	return pPrevFactory;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CNetworkManager::Init()
{
	m_bIsClient = m_bIsServer = false;
	m_pClientToServerConnection = NULL;

	// Make a listener array for each message type

	// Register all network messages
	INetworkMessageFactory *pFactory;
	for ( pFactory = m_pFirstFactory; pFactory; pFactory = pFactory->GetNextFactory() )
	{
		INetworkMessage *pNetworkMessage = pFactory->CreateNetworkMessage();
		g_pNetworkSystem->RegisterMessage( pNetworkMessage );
	}

	return true;
}

void CNetworkManager::Shutdown()
{
	ShutdownClient();
	ShutdownServer();
}


//-----------------------------------------------------------------------------
// Start a server up
//-----------------------------------------------------------------------------
bool CNetworkManager::StartServer( unsigned short nServerListenPort )
{
	ShutdownServer( );
	if ( nServerListenPort == LEGION_DEFAULT_SERVER_PORT )
	{
		nServerListenPort = CommandLine()->ParmValue( "-serverport", NETWORKSYSTEM_DEFAULT_SERVER_PORT );
	}
	m_bIsServer = g_pNetworkSystem->StartServer( nServerListenPort );
	return m_bIsServer;
}

void CNetworkManager::ShutdownServer( )
{
	if ( m_bIsServer )
	{
		g_pNetworkSystem->ShutdownServer( );
		m_bIsServer = false;
	}
}

	
//-----------------------------------------------------------------------------
// Start a client up
//-----------------------------------------------------------------------------
bool CNetworkManager::StartClient( unsigned short nClientListenPort )
{
	ShutdownClient( );

	if ( nClientListenPort == LEGION_DEFAULT_CLIENT_PORT )
	{
		nClientListenPort = CommandLine()->ParmValue( "-clientport", NETWORKSYSTEM_DEFAULT_CLIENT_PORT );
	}

	m_bIsClient = g_pNetworkSystem->StartClient( nClientListenPort );
	return m_bIsClient;
}

void CNetworkManager::ShutdownClient( )
{
	if ( m_bIsClient )
	{
		DisconnectClientFromServer();
		g_pNetworkSystem->ShutdownClient( );
		m_bIsClient = false;
	}
}


//-----------------------------------------------------------------------------
// Connects/disconnects the client to a server
//-----------------------------------------------------------------------------
bool CNetworkManager::ConnectClientToServer( const char *pServerName, unsigned short nServerListenPort )
{
	DisconnectClientFromServer();
	if ( !IsClient() )
		return false;

	if ( nServerListenPort == LEGION_DEFAULT_SERVER_PORT )
	{
		nServerListenPort = CommandLine()->ParmValue( "-serverport", NETWORKSYSTEM_DEFAULT_SERVER_PORT );
	}
	m_pClientToServerConnection = g_pNetworkSystem->ConnectClientToServer( pServerName, nServerListenPort );
	return ( m_pClientToServerConnection != NULL );
}

void CNetworkManager::DisconnectClientFromServer( )
{
	if ( m_pClientToServerConnection )
	{
		g_pNetworkSystem->DisconnectClientFromServer( m_pClientToServerConnection );	
		m_pClientToServerConnection = NULL;
	}
}


//-----------------------------------------------------------------------------
// Start up a game where the local player is playing
//-----------------------------------------------------------------------------
bool CNetworkManager::HostGame()
{
	if ( !StartServer() )
		return false;

	if ( !StartClient() )
	{
		ShutdownServer();
		return false;
	}

	if ( !ConnectClientToServer( "localhost" ) )
	{
		ShutdownClient();
		ShutdownServer();
		return false;
	}

	return true;
}

void CNetworkManager::StopHostingGame()
{
	ShutdownClient();
	ShutdownServer();
}


//-----------------------------------------------------------------------------
// Are we a client?/Are we a server? (we can be both)
//-----------------------------------------------------------------------------
bool CNetworkManager::IsClient()
{
	return m_bIsClient;
}

bool CNetworkManager::IsServer()
{
	return m_bIsServer;
}

	
//-----------------------------------------------------------------------------
// If we are a client, are we connected to the server?
//-----------------------------------------------------------------------------
bool CNetworkManager::IsClientConnected()
{
	return m_bIsClient && ( m_pClientToServerConnection != NULL ) && ( m_pClientToServerConnection->GetConnectionState() == CONNECTION_STATE_CONNECTED );
}


//-----------------------------------------------------------------------------
// Post a network message to the server
//-----------------------------------------------------------------------------
void CNetworkManager::PostClientToServerMessage( INetworkMessage *pMessage )
{
	if ( IsClientConnected() )
	{
		m_pClientToServerConnection->AddNetMsg( pMessage );
	}
}

	
//-----------------------------------------------------------------------------
// Broadcast a network message to all clients
//-----------------------------------------------------------------------------
void CNetworkManager::BroadcastServerToClientMessage( INetworkMessage *pMessage )
{
	if ( IsServer() )
	{
		int nCount = m_ServerToClientConnection.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			m_ServerToClientConnection[i]->AddNetMsg( pMessage );
		}
	}
}

	
//-----------------------------------------------------------------------------
// Add, remove network message listeners
//-----------------------------------------------------------------------------
void CNetworkManager::AddListener( NetworkMessageRoute_t route, int nGroup, int nType, INetworkMessageListener *pListener )
{
	CUtlVector< CUtlVector< CUtlVector < INetworkMessageListener* > > > &listeners = m_Listeners[ route ];
	if ( listeners.Count() <= nGroup )
	{
		listeners.AddMultipleToTail( nGroup - listeners.Count() + 1 );
	}

	if ( listeners[nGroup].Count() <= nType )
	{
		listeners[nGroup].AddMultipleToTail( nType - listeners[nGroup].Count() + 1 );
	}

	Assert( listeners[nGroup][nType].Find( pListener ) < 0 );
	listeners[nGroup][nType].AddToTail( pListener );
}

void CNetworkManager::RemoveListener( NetworkMessageRoute_t route, int nGroup, int nType, INetworkMessageListener *pListener )
{
	CUtlVector< CUtlVector< CUtlVector < INetworkMessageListener* > > > &listeners = m_Listeners[ route ];
	if ( listeners.Count() > nGroup )
	{
		if( listeners[nGroup].Count() > nType )
		{
			// Maintain order of listeners (not sure if it matters)
			listeners[nGroup][nType].FindAndRemove( pListener );
		}
	}
}


//-----------------------------------------------------------------------------
// Notifies listeners about a network message
//-----------------------------------------------------------------------------
void CNetworkManager::NotifyListeners( NetworkMessageRoute_t route, INetworkMessage *pMessage )
{
	CUtlVector< CUtlVector< CUtlVector < INetworkMessageListener* > > > &listeners = m_Listeners[ route ];

	// based on id, search for installed listeners
	int nGroup = pMessage->GetGroup();
	if ( listeners.Count() > nGroup )
	{
		int nType = pMessage->GetType();
		if ( listeners[nGroup].Count() > nType )
		{
			CUtlVector< INetworkMessageListener* > &list = listeners[nGroup][nType];
			int nCount = list.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				list[i]->OnNetworkMessage( route, pMessage );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Process messages received by the client
//-----------------------------------------------------------------------------
void CNetworkManager::ProcessClientMessages()
{
	NetworkEvent_t *pEvent = g_pNetworkSystem->FirstNetworkEvent();
	for ( ; pEvent; pEvent = g_pNetworkSystem->NextNetworkEvent( ) )
	{
		switch ( pEvent->m_nType )
		{
			/*
		case NETWORK_EVENT_CONNECTED:
			{
				NetworkConnectionEvent_t* pConnectEvent = static_cast<NetworkConnectionEvent_t*>( pEvent );
				m_pClientToServerConnection = pConnectEvent->m_pChannel;
			}
			break;

		case NETWORK_EVENT_DISCONNECTED:
			{
				NetworkDisconnectionEvent_t* pDisconnectEvent = static_cast<NetworkDisconnectionEvent_t*>( pEvent );
				Assert( m_pClientToServerConnection == pDisconnectEvent->m_pChannel );
				if ( m_pClientToServerConnection == pDisconnectEvent->m_pChannel )
				{
					m_pClientToServerConnection = NULL;
				}
			}
			break;
		*/

		case NETWORK_EVENT_MESSAGE_RECEIVED:
			{
				NetworkMessageReceivedEvent_t* pPacketEvent = static_cast<NetworkMessageReceivedEvent_t*>( pEvent );
				NotifyListeners( NETWORK_MESSAGE_SERVER_TO_CLIENT, pPacketEvent->m_pNetworkMessage );
			}
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Process messages received by the server
//-----------------------------------------------------------------------------
void CNetworkManager::ProcessServerMessages()
{
	NetworkEvent_t *pEvent = g_pNetworkSystem->FirstNetworkEvent();
	for ( ; pEvent; pEvent = g_pNetworkSystem->NextNetworkEvent( ) )
	{
		switch ( pEvent->m_nType )
		{
		case NETWORK_EVENT_CONNECTED:
			{
				NetworkConnectionEvent_t* pConnectEvent = static_cast<NetworkConnectionEvent_t*>( pEvent );
				m_ServerToClientConnection.AddToTail( pConnectEvent->m_pChannel );
			}
			break;

		case NETWORK_EVENT_DISCONNECTED:
			{
				NetworkDisconnectionEvent_t* pDisconnectEvent = static_cast<NetworkDisconnectionEvent_t*>( pEvent );
				m_ServerToClientConnection.FindAndRemove( pDisconnectEvent->m_pChannel );
			}
			break;
		
		case NETWORK_EVENT_MESSAGE_RECEIVED:
			{
				NetworkMessageReceivedEvent_t* pPacketEvent = static_cast<NetworkMessageReceivedEvent_t*>( pEvent );
				NotifyListeners( NETWORK_MESSAGE_CLIENT_TO_SERVER, pPacketEvent->m_pNetworkMessage );
			}
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------
void CNetworkManager::Update( )
{
	if ( IsClient() )
	{
		g_pNetworkSystem->ClientSendMessages();
	}

	if ( IsServer() )
	{
		g_pNetworkSystem->ServerReceiveMessages();
		ProcessServerMessages();
		g_pNetworkSystem->ServerSendMessages();
	}

	if ( IsClient() )
	{
		g_pNetworkSystem->ClientReceiveMessages();
		ProcessClientMessages();
	}
}


