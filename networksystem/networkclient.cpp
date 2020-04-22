//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "networkclient.h"
#include "UDP_Socket.h"
#include "NetChannel.h"
#include "UDP_Process.h"
#include <winsock.h>
#include "tier1/bitbuf.h"
#include "networksystem.h"
#include "sm_protocol.h"


//-----------------------------------------------------------------------------
// Construction/Destruction
//-----------------------------------------------------------------------------
CNetworkClient::CNetworkClient() : m_bConnected( false )
{
	m_pSocket = new CUDPSocket;
}

CNetworkClient::~CNetworkClient()
{
	delete m_pSocket;
}


//-----------------------------------------------------------------------------
// Init/Shutdown
//-----------------------------------------------------------------------------
bool CNetworkClient::Init( int nListenPort )
{
	if ( !m_pSocket->Init( nListenPort ) )
	{
		Warning( "CNetworkClient:  Unable to create socket!!!\n" );
		return false;
	}
	return true;
}

void CNetworkClient::Shutdown()
{
	m_pSocket->Shutdown();
}

#define SPEW_MESSAGES 

#if defined( SPEW_MESSAGES )
#define SM_SPEW_MESSAGE( code, remote ) \
	Warning( "Message:  %s from '%s'\n", #code, remote );
#else
#define SM_SPEW_MESSAGE( code, remote ) 
#endif

// process a connectionless packet
bool CNetworkClient::ProcessConnectionlessPacket( CNetPacket *packet )
{
	int code = packet->m_Message.ReadByte();
	switch ( code )
	{
	case s2c_connect_accept:
		{
			SM_SPEW_MESSAGE( s2c_connect_accept, packet->m_From.ToString() );

			m_NetChan.Setup( false, &packet->m_From, m_pSocket, "client", this );
			m_bConnected = true;
		}
		break;
	default:
		{
			Warning( "CSmServer::ProcessConnectionlessPacket:  Unknown code '%i' from '%s'\n",
				code, packet->m_From.ToString() );
		}
		break;
	}

	return true;
}

INetChannel *CNetworkClient::FindNetChannel( const netadr_t& from )
{
	if ( from.CompareAdr( m_NetChan.GetRemoteAddress() ) )
		return &m_NetChan;
	return NULL;
}

void CNetworkClient::ReadPackets( void )
{
	UDP_ProcessSocket( m_pSocket, this, this );	
}

void CNetworkClient::SendUpdate()
{
	if ( m_bConnected )
	{
		m_NetChan.SendDatagram( NULL );
	}
}


void CNetworkClient::Disconnect()
{
	if ( !m_bConnected )
		return;

	byte data[ 512 ];
	bf_write buf( "CNetworkClient::Connect", data, sizeof( data ) );

	WriteSystemNetworkMessage( buf, net_disconnect );
	buf.WriteString( "client disconnected" );

	m_NetChan.SendDatagram( &buf );
	m_NetChan.SendDatagram( &buf );
	m_NetChan.SendDatagram( &buf );
}

bool CNetworkClient::Connect( char const *server, int port /*=SM_SERVER_PORT*/ )
{
	byte data[ 512 ];
	bf_write buf( "CNetworkClient::Connect", data, sizeof( data ) );

	buf.WriteLong( -1 );
	buf.WriteByte( c2s_connect );

	netadr_t remote;
	remote.type = NA_IP;
	remote.port = htons( (unsigned short)port );
	
	// Resolve remote name
	sockaddr sa;
	if ( !Q_stricmp( server, "localhost" ) )
	{
		remote.ip[ 0 ] = 127;
		remote.ip[ 1 ] = 0;
		remote.ip[ 2 ] = 0;
		remote.ip[ 3 ] = 1;
	}
	else
	{
		if ( !g_pNetworkSystemImp->StringToSockaddr( server, &sa ) )
		{
			Msg( "Unable to resolve '%s'\n", server );
			return false;
		}

		remote.SetFromSockadr( &sa );
	}

	m_NetChan.SetConnectionState( CONNECTION_STATE_CONNECTING );

	return m_pSocket->SendTo( remote, buf.GetData(), buf.GetNumBytesWritten() );
}

void CNetworkClient::OnConnectionClosing( INetChannel *channel, char const *reason )
{
	Warning( "OnConnectionClosing '%s'\n", reason );
	if ( channel == &m_NetChan )
	{
		m_NetChan.Shutdown( "received disconnect\n" );
		m_bConnected = false;
	}
}

void CNetworkClient::OnConnectionStarted( INetChannel *channel )
{
	Warning( "OnConnectionStarted\n" );
}

void CNetworkClient::OnPacketStarted( int inseq, int outseq )
{
}

void CNetworkClient::OnPacketFinished()
{
}