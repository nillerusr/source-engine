//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "networksystem.h"
#include "filesystem.h"
#include "UDP_Socket.h"
#include "sm_protocol.h"
#include "NetChannel.h"
#include "UDP_Process.h"
#include <winsock.h>
#include "networkclient.h"
#include "networkserver.h"
#include "networksystem/inetworkmessage.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CNetworkSystem g_NetworkSystem;
CNetworkSystem *g_pNetworkSystemImp = &g_NetworkSystem; 
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CNetworkSystem, INetworkSystem, 
						NETWORKSYSTEM_INTERFACE_VERSION, g_NetworkSystem );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CNetworkSystem::CNetworkSystem()
{
	m_bWinsockInitialized = false;
	m_bNetworkEventCreated = false;
	m_bInMidPacket = false;
	m_pServer = NULL;
	m_pClient = NULL;
	m_nGroupBits = 1;
	m_nTypeBits = LargestPowerOfTwoLessThanOrEqual( net_num_messages );
}

CNetworkSystem::~CNetworkSystem()
{
}


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CNetworkSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	// initialize winsock 2.0
	WSAData wsaData;
	
	if ( WSAStartup( MAKEWORD(2,0), &wsaData ) != 0 )
	{
		Warning( "Error! Failed to load network socket library.\n");
		return INIT_OK;
	}
	else
	{
		m_bWinsockInitialized = true;
	}

	LPHOSTENT lp = gethostbyname("localhost");
	if ( !lp )
	{
		Warning( "Error! Failed to query local host info\n");
		return INIT_OK;
	}
	m_LocalHostName = lp->h_name;

	lp = gethostbyname( m_LocalHostName );
 	if ( !lp )
	{
		Warning( "Error! Failed to query local host info\n");
		return INIT_OK;
	}

	sockaddr ip;
	m_LocalAddressString = inet_ntoa( *((in_addr*)(lp->h_addr_list[0])) );
	StringToSockaddr( m_LocalAddressString, &ip );
	m_LocalAddress.SetFromSockadr( &ip );
	return INIT_OK;
}

void CNetworkSystem::Shutdown()
{
	if ( m_bWinsockInitialized )
	{
		WSACleanup();
	}
	CleanupNetworkMessages();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CNetworkSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem )
	{
		Warning( "The network system requires the filesystem to run!\n" );
		return false;
	}

	return INIT_OK; 
}


//-----------------------------------------------------------------------------
// Returns the current time
//-----------------------------------------------------------------------------
float CNetworkSystem::GetTime( void )
{
	return Plat_FloatTime();
}


//-----------------------------------------------------------------------------
// Installs network message factories to be used with all connections
//-----------------------------------------------------------------------------
bool CNetworkSystem::RegisterMessage( INetworkMessage *pMessage )
{
	if ( m_pServer || m_pClient )
	{
		Warning( "Cannot register messages after connection has started.\n" );
		return false;
	}

	if ( pMessage->GetGroup() == 0 )
	{
		Warning( "Network message group 0 is reserved by the network system.\n" );
		return false;
	}

	// Look for already registered message
	if ( m_NetworkMessages.Find( pMessage ) >= 0 )
		return false;

	// Allocate more space in messages 
	int nGroupBits = LargestPowerOfTwoLessThanOrEqual( pMessage->GetGroup() );
	int nTypeBits = LargestPowerOfTwoLessThanOrEqual( pMessage->GetType() );
	m_nGroupBits = max( nGroupBits, m_nGroupBits );
	m_nTypeBits = max( nTypeBits, m_nTypeBits );

	m_NetworkMessages.AddToTail( pMessage );
	return true;
}

void CNetworkSystem::CleanupNetworkMessages( )
{
	int nCount = m_NetworkMessages.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_NetworkMessages[i]->Release();
	}

	m_NetworkMessages.RemoveAll();
}


//-----------------------------------------------------------------------------
// Finds a network message given a particular message type
//-----------------------------------------------------------------------------
INetworkMessage* CNetworkSystem::FindNetworkMessage( int group, int type )
{
	int nCount = m_NetworkMessages.Count();
	for (int i=0; i < nCount; i++ )
	{
		if ( ( m_NetworkMessages[i]->GetGroup() == group ) && ( m_NetworkMessages[i]->GetType() == type ) )
			return m_NetworkMessages[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNetworkSystem::StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];
	
	Q_memset (sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpy (copy, s, sizeof( copy ) );
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));	
		}
	
	if (copy[0] >= '0' && copy[0] <= '9' && Q_strstr( copy, "." ) )
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
//		if ( net_nodns )
//			return false;	// DNS names disabled

		if ( (h = gethostbyname(copy)) == NULL )
			return false;

		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Returns the local address
//-----------------------------------------------------------------------------
const char* CNetworkSystem::GetLocalHostName( void ) const
{
	return m_LocalHostName;
}

const char* CNetworkSystem::GetLocalAddress( void ) const
{
	return m_LocalAddressString;
}


//-----------------------------------------------------------------------------
// Start, shutdown a server
//-----------------------------------------------------------------------------
bool CNetworkSystem::StartServer( unsigned short nServerListenPort )
{
	if ( !m_bWinsockInitialized )
		return false;

	Assert( !m_pServer );
	m_pServer = new CNetworkServer;
	return m_pServer->Init( nServerListenPort );
}

void CNetworkSystem::ShutdownServer( )
{
	if ( m_pServer )
	{
		m_pServer->Shutdown();
		delete m_pServer;
		m_pServer = NULL;
	}
}


//-----------------------------------------------------------------------------
// Server update
//-----------------------------------------------------------------------------
void CNetworkSystem::ServerReceiveMessages()
{
	if ( m_pServer )
	{
		m_pServer->ReadPackets();
	}
}

void CNetworkSystem::ServerSendMessages()
{
	if ( m_pServer )
	{
		m_pServer->SendUpdates();
	}
}


//-----------------------------------------------------------------------------
// Start, shutdown a client
//-----------------------------------------------------------------------------
bool CNetworkSystem::StartClient( unsigned short nClientListenPort )
{
	if ( !m_bWinsockInitialized )
		return false;

	Assert( !m_pClient );
	m_pClient = new CNetworkClient;
	return m_pClient->Init( nClientListenPort );
}

void CNetworkSystem::ShutdownClient( )
{
	if ( m_pClient )
	{
		m_pClient->Shutdown();
		delete m_pClient;
		m_pClient = NULL;
	}
}


//-----------------------------------------------------------------------------
// Server update
//-----------------------------------------------------------------------------
void CNetworkSystem::ClientReceiveMessages()
{
	if ( m_pClient )
	{
		m_pClient->ReadPackets();
	}
}

void CNetworkSystem::ClientSendMessages()
{
	if ( m_pClient )
	{
		m_pClient->SendUpdate();
	}
}


//-----------------------------------------------------------------------------
// Server update
//-----------------------------------------------------------------------------
INetChannel* CNetworkSystem::ConnectClientToServer( const char *pServer, int nServerListenPort )
{
	if ( m_pClient )
	{
		if ( m_pClient->Connect( pServer, nServerListenPort ) )
			return m_pClient->GetNetChannel();
	}
	return NULL;
}

void CNetworkSystem::DisconnectClientFromServer( INetChannel* pChannel )
{
	if ( m_pClient && ( m_pClient->GetNetChannel() == pChannel ) )
	{
		m_pClient->Disconnect();
	}
}


//-----------------------------------------------------------------------------
// Queues up a network packet
//-----------------------------------------------------------------------------
void CNetworkSystem::EnqueueConnectionlessNetworkPacket( CNetPacket *pPacket, IConnectionlessPacketHandler *pHandler )
{
	int i = m_PacketQueue.AddToTail( );

	PacketInfo_t& info = m_PacketQueue[i];
	info.m_pPacket = pPacket;
	info.m_pHandler = pHandler;
	info.m_pNetChannel = NULL;
}

void CNetworkSystem::EnqueueNetworkPacket( CNetPacket *pPacket, CNetChannel *pNetChannel )
{
	int i = m_PacketQueue.AddToTail( );

	PacketInfo_t& info = m_PacketQueue[i];
	info.m_pPacket = pPacket;
	info.m_pHandler = NULL;
	info.m_pNetChannel = pNetChannel;
}


//-----------------------------------------------------------------------------
// Network event iteration helpers
//-----------------------------------------------------------------------------
bool CNetworkSystem::StartProcessingNewPacket()
{
	PacketInfo_t& info = m_PacketQueue[ m_nProcessingPacket ];
	if ( info.m_pHandler )
	{
		UDP_ProcessConnectionlessPacket( info.m_pPacket, info.m_pHandler );
		return false;
	}

	if ( !info.m_pNetChannel )
	{
		// Not an error that may happen during connect or disconnect
		Warning( "Sequenced packet without connection from %s\n" , info.m_pPacket->m_From.ToString() );
		return false;
	}

	return info.m_pNetChannel->StartProcessingPacket( info.m_pPacket );
}

bool CNetworkSystem::AdvanceProcessingNetworkPacket( )
{
	m_PacketQueue[ m_nProcessingPacket ].m_pPacket->Release();
	m_PacketQueue[ m_nProcessingPacket ].m_pPacket = NULL;

	++m_nProcessingPacket;
	bool bOverflowed = ( m_nProcessingPacket >= m_PacketQueue.Count() );
	if ( bOverflowed )
	{
		m_PacketQueue.RemoveAll();
		m_nProcessingPacket = 0;
	}
	return !bOverflowed;
}


//-----------------------------------------------------------------------------
// Network event iteration
//-----------------------------------------------------------------------------
NetworkEvent_t *CNetworkSystem::FirstNetworkEvent( )
{
	Assert( !m_bInMidPacket && !m_nProcessingPacket );
	m_nProcessingPacket = 0;
	m_bInMidPacket = false;
	return NextNetworkEvent();
}

NetworkEvent_t *CNetworkSystem::NextNetworkEvent( )
{
	int nPacketCount = m_PacketQueue.Count();
	if ( m_nProcessingPacket >= nPacketCount )
		return NULL;

	while( true )
	{
		// Continue processing the packet we're currently on
		if ( m_bInMidPacket )
		{
			PacketInfo_t& info = m_PacketQueue[ m_nProcessingPacket ];
			while( info.m_pNetChannel->ProcessPacket( info.m_pPacket ) )
			{
				Assert( m_bNetworkEventCreated );
				m_bNetworkEventCreated = false;
				return (NetworkEvent_t*)m_EventMessageBuffer;
			}
			info.m_pNetChannel->EndProcessingPacket( info.m_pPacket );
			m_bInMidPacket = false;

			if ( !AdvanceProcessingNetworkPacket() )
				return NULL;
		}

		// Keep reading packets until we find one that either generates an event,
		// one that encounters a packet we need to process, or we exhaust the event queue
		while( !StartProcessingNewPacket() )
		{
			bool bOverflowed = !AdvanceProcessingNetworkPacket();

			if ( m_bNetworkEventCreated )
			{
				m_bNetworkEventCreated = false;
				return (NetworkEvent_t*)m_EventMessageBuffer;
			}

			if ( bOverflowed )
				return NULL;
		}

		m_bInMidPacket = true;
	}
}


//-----------------------------------------------------------------------------
// Network event creation
//-----------------------------------------------------------------------------
NetworkEvent_t* CNetworkSystem::CreateNetworkEvent( int nSizeInBytes )
{
	Assert( nSizeInBytes <= sizeof( m_EventMessageBuffer ) );

	// If this assertion fails, it means two or more network events were created
	// before the main network event loop had a chance to inform external code
	Assert( !m_bNetworkEventCreated );
	m_bNetworkEventCreated = true;
	return ( NetworkEvent_t* )m_EventMessageBuffer;
}

bool CNetworkSystem::IsNetworkEventCreated()
{
	return m_bNetworkEventCreated;
}
	
