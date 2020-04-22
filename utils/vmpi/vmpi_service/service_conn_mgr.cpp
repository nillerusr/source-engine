//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "service_conn_mgr.h"
#include "vmpi.h"
#include "tier0/dbg.h"
#include "tcpsocket_helpers.h"


#define SERVICECONNMGR_CONNECT_ATTEMPT_INTERVAL	1000



// ------------------------------------------------------------------------------------------- //
// CServiceConn.
// ------------------------------------------------------------------------------------------- //

CServiceConn::CServiceConn()
{
	m_pSocket = NULL;
}


CServiceConn::~CServiceConn()
{
	if ( m_pSocket )
		m_pSocket->Release();
}


// ------------------------------------------------------------------------------------------- //
// CServiceConnMgr.
// ------------------------------------------------------------------------------------------- //


CServiceConnMgr::CServiceConnMgr()
{
	m_bServer = false;
	m_bShuttingDown = false;
	m_pListenSocket = NULL;
}


CServiceConnMgr::~CServiceConnMgr()
{
	Term();
}


bool CServiceConnMgr::InitServer()
{
	Term();

	m_bServer = true;
	
	// Create a socket to listen on.
	for ( int iPort=VMPI_SERVICE_FIRST_UI_PORT; iPort <= VMPI_SERVICE_LAST_UI_PORT; iPort++ )
	{
		m_pListenSocket = CreateTCPListenSocketEmu( iPort, 5 );
		if ( m_pListenSocket )
			break;
	}
	if ( !m_pListenSocket )
		return false;

	return true;
}


bool CServiceConnMgr::InitClient()
{
	Term();

	m_bServer = false;

	AttemptConnect();
	return true;
}


void CServiceConnMgr::Term()
{
	m_bShuttingDown = true;	// This prevents some reentrancy.

	// Get rid of our registry key.
	if ( m_pListenSocket )
	{
		m_pListenSocket->Release();
		m_pListenSocket = NULL;
	}

	m_Connections.PurgeAndDeleteElements();

	m_bShuttingDown = false;
}


bool CServiceConnMgr::IsConnected()
{
	return m_Connections.Count() != 0;
}


void CServiceConnMgr::Update()
{
	DWORD curTime = GetTickCount();

	// Connect if we're an unconnected client.
	if ( m_bServer )
	{
		if ( m_pListenSocket )
		{
			// Listen for more connections.
			while ( 1 )
			{
				CIPAddr addr;
				ITCPSocket *pSocket = m_pListenSocket->UpdateListen( &addr );
				if ( !pSocket )
					break;

				CServiceConn *pConn = new CServiceConn;
				pConn->m_ID = m_Connections.AddToTail( pConn );
				pConn->m_LastRecvTime = curTime;
				pConn->m_pSocket = pSocket;

				OnNewConnection( pConn->m_ID );
			}
		}
	}
	else
	{
		if ( !IsConnected() && curTime - m_LastConnectAttemptTime >= SERVICECONNMGR_CONNECT_ATTEMPT_INTERVAL )
		{
			AttemptConnect();
		}
	}

	// Check for timeouts and send acks.
	int iNext;
	for ( int iCur=m_Connections.Head(); iCur != m_Connections.InvalidIndex(); iCur=iNext )
	{
		iNext = m_Connections.Next( iCur );
		CServiceConn *pConn = m_Connections[iCur];

		if ( pConn->m_pSocket->IsConnected() )
		{
			DWORD startTime = GetTickCount();
			CUtlVector<unsigned char> data;
			while ( pConn->m_pSocket->Recv( data ) )
			{
				HandlePacket( (char*)data.Base(), data.Count() );
				
				// Don't sit in this loop too long.
				if ( (GetTickCount() - startTime) > 50 )
					break;
			}
		}
		else
		{
			OnTerminateConnection( iCur );
			m_Connections.Remove( iCur );
			delete pConn;
		}
	}
}


void CServiceConnMgr::SendPacket( int id, const void *pData, int len )
{
	if ( id == -1 )
	{
		FOR_EACH_LL( m_Connections, i )
		{
			m_Connections[i]->m_pSocket->Send( pData, len );
		}
	}
	else
	{
		m_Connections[id]->m_pSocket->Send( pData, len );
	}
}


void CServiceConnMgr::AttemptConnect()
{
	m_LastConnectAttemptTime = GetTickCount();


	ITCPSocket *pSocket = NULL;
	for ( int iPort=VMPI_SERVICE_FIRST_UI_PORT; iPort <= VMPI_SERVICE_LAST_UI_PORT; iPort++ )
	{
		pSocket = CreateTCPSocketEmu();
		if ( !pSocket || !pSocket->BindToAny( 0 ) )
			return;

		CIPAddr addr( 127, 0, 0, 1, iPort );
		if ( TCPSocket_Connect( pSocket, &addr, 0.1 ) )
			break;

		pSocket->Release();
		pSocket = NULL;
	}

	if ( pSocket )
	{
		CServiceConn *pConn = new CServiceConn;
		pConn->m_ID = m_Connections.AddToTail( pConn );
		pConn->m_LastRecvTime = GetTickCount();
		pConn->m_pSocket = pSocket;

		OnNewConnection( pConn->m_ID );
	}
}


void CServiceConnMgr::OnNewConnection( int id )
{
}


void CServiceConnMgr::OnTerminateConnection( int id )
{
}


void CServiceConnMgr::HandlePacket( const char *pData, int len )
{
}

