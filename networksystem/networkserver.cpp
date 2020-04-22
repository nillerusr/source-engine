//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "networkserver.h"
#include "networksystem.h"
#include "icvar.h"
#include "filesystem.h"
#include "UDP_Socket.h"
#include "sm_protocol.h"
#include "NetChannel.h"
#include "UDP_Process.h"
#include <winsock.h>
#include "networkclient.h"



//-----------------------------------------------------------------------------
//
// Implementation of CPlayer
//
//-----------------------------------------------------------------------------
CNetworkServer::CNetworkServer( )
{
	m_pSocket = new CUDPSocket;
}

CNetworkServer::~CNetworkServer()
{
	delete m_pSocket;
}

bool CNetworkServer::Init( int nServerPort )
{
	if ( !m_pSocket->Init( nServerPort ) )
	{
		Warning( "CNetworkServer:  Unable to create socket!!!\n" );
		return false;
	}

	return true;
}

void CNetworkServer::Shutdown()
{
	m_pSocket->Shutdown();
}

CNetChannel *CNetworkServer::FindNetChannel( const netadr_t& from )
{
	CPlayer *pl = FindPlayerByAddress( from );
	if ( pl )
		return &pl->m_NetChan;
	return NULL;
}

CPlayer *CNetworkServer::FindPlayerByAddress( const netadr_t& adr )
{
	int c = m_Players.Count();
	for ( int i = 0; i < c; ++i )
	{
		CPlayer *player = m_Players[ i ];
		if ( player->GetRemoteAddress().CompareAdr( adr ) )
			return player;
	}
	return NULL;
}

CPlayer *CNetworkServer::FindPlayerByNetChannel( INetChannel *chan )
{
	int c = m_Players.Count();
	for ( int i = 0; i < c; ++i )
	{
		CPlayer *player = m_Players[ i ];
		if ( &player->m_NetChan == chan )
			return player;
	}
	return NULL;
}

#define SPEW_MESSAGES 

#if defined( SPEW_MESSAGES )
#define SM_SPEW_MESSAGE( code, remote ) \
	Warning( "Message:  %s from '%s'\n", #code, remote );
#else
#define SM_SPEW_MESSAGE( code, remote ) 
#endif
// process a connectionless packet
bool CNetworkServer::ProcessConnectionlessPacket( CNetPacket *packet )
{
	int code = packet->m_Message.ReadByte();
	switch ( code )
	{
	case c2s_connect:
		{
			SM_SPEW_MESSAGE( c2s_connect, packet->m_From.ToString() );

			CPlayer *pl = FindPlayerByAddress( packet->m_From );
			if ( pl )
			{
				Warning( "Player already exists for %s\n", packet->m_From.ToString() );
			}
			else
			{
				// Creates the connection
				pl = new CPlayer( this, packet->m_From );
				m_Players.AddToTail( pl );

				// Now send the conn accepted message
				AcceptConnection( packet->m_From );
			}
		}
		break;
	default:
		{
			Warning( "CNetworkServer::ProcessConnectionlessPacket:  Unknown code '%i' from '%s'\n",
				code, packet->m_From.ToString() );
		}
		break;
	}

	return true;
}


void CNetworkServer::AcceptConnection( const netadr_t& remote )
{
	byte data[ 512 ];
	bf_write buf( "CNetworkServer::AcceptConnection", data, sizeof( data ) );

	buf.WriteLong( -1 );
	buf.WriteByte( s2c_connect_accept );

	m_pSocket->SendTo( remote, buf.GetData(), buf.GetNumBytesWritten() );
}

void CNetworkServer::ReadPackets( void )
{
	UDP_ProcessSocket( m_pSocket, this, this );

	int c = m_Players.Count();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		if ( m_Players[ i ]->m_bMarkedForDeletion )
		{
			CPlayer *pl = m_Players[ i ];
			m_Players.Remove( i );
			delete pl;
		}
	}
}

void CNetworkServer::SendUpdates()
{
	int c = m_Players.Count();
	for ( int i = 0; i < c; ++i )
	{
		m_Players[ i ]->SendUpdate();
	}
}

void CNetworkServer::OnConnectionStarted( INetChannel *pChannel )
{
	// Create a network event
	NetworkConnectionEvent_t *pConnection = g_pNetworkSystemImp->CreateNetworkEvent< NetworkConnectionEvent_t >( );
	pConnection->m_nType = NETWORK_EVENT_CONNECTED;
	pConnection->m_pChannel = pChannel;
}

void CNetworkServer::OnConnectionClosing( INetChannel *pChannel, char const *reason )
{
	Warning( "OnConnectionClosing '%s'\n", reason );

	CPlayer *pPlayer = FindPlayerByNetChannel( pChannel );
	if ( pPlayer )
	{
		pPlayer->Shutdown();
	}

	// Create a network event
	NetworkDisconnectionEvent_t *pDisconnection = g_pNetworkSystemImp->CreateNetworkEvent< NetworkDisconnectionEvent_t >( );
	pDisconnection->m_nType = NETWORK_EVENT_DISCONNECTED;
	pDisconnection->m_pChannel = pChannel;
}

void CNetworkServer::OnPacketStarted( int inseq, int outseq )
{
}

void CNetworkServer::OnPacketFinished()
{
}


//-----------------------------------------------------------------------------
//
// Implementation of CPlayer
//
//-----------------------------------------------------------------------------
CPlayer::CPlayer( CNetworkServer *server, netadr_t& remote ) :
	m_bMarkedForDeletion( false )
{
	m_NetChan.Setup( true, &remote, server->m_pSocket, "player", server );
}

void CPlayer::Shutdown()
{
	m_bMarkedForDeletion = true;
	m_NetChan.Shutdown( "received disconnect\n" );
}

void CPlayer::SendUpdate()
{
	if ( m_NetChan.CanSendPacket() )
	{
		m_NetChan.SendDatagram( NULL );
	}
}



