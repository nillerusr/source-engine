//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier1/utlbuffer.h"
#include "UDP_Socket.h"
#include "NetChannel.h"
#include "sm_protocol.h"
#include "networksystem.h"


//-----------------------------------------------------------------------------
// Allocates memory for packets
//-----------------------------------------------------------------------------
static CUtlMemoryPool s_PacketBufferAlloc( NET_MAX_MESSAGE, 8, CUtlMemoryPool::GROW_SLOW );
DEFINE_FIXEDSIZE_ALLOCATOR( CNetPacket, 32, CUtlMemoryPool::GROW_SLOW );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CNetPacket::CNetPacket()
{
	m_pData = reinterpret_cast<byte*>( s_PacketBufferAlloc.Alloc() );
	m_nRefCount = 1;
}

CNetPacket::~CNetPacket()
{
	s_PacketBufferAlloc.Free( m_pData );
}

void CNetPacket::AddRef()
{
	++m_nRefCount;
}

void CNetPacket::Release()
{
	if ( --m_nRefCount <= 0 )
	{
		delete this;
	}
}
	
bool UDP_ReceiveDatagram( CUDPSocket *pSocket, CNetPacket* pPacket )
{
	Assert( pPacket );

	netadr_t packetFrom;
	CUtlBuffer buf( pPacket->m_pData, NET_MAX_MESSAGE );
	if ( pSocket->RecvFrom( packetFrom, buf ) )
	{
		pPacket->m_From = packetFrom;
		pPacket->m_nSizeInBytes = buf.TellPut();
		return true;
	}
	return false;
}

CNetPacket *UDP_GetPacket( CUDPSocket *pSocket )
{
	CNetPacket *pPacket = new CNetPacket;

	// setup new packet
	pPacket->m_From.SetType( NA_IP );
	pPacket->m_From.Clear();
	pPacket->m_flReceivedTime = g_pNetworkSystemImp->GetTime();
	pPacket->m_pSource = pSocket;	
	pPacket->m_nSizeInBytes = 0;
	pPacket->m_Message.SetDebugName("inpacket.message");

	// then check UDP data 
	if ( !UDP_ReceiveDatagram( pSocket, pPacket ) )
	{
		delete pPacket;
		return NULL;
	}
		
	Assert( pPacket->m_nSizeInBytes ); 
	
	// prepare bitbuffer for reading packet with new size
	pPacket->m_Message.StartReading( pPacket->m_pData, pPacket->m_nSizeInBytes );
	return pPacket;
}

void UDP_ProcessSocket( CUDPSocket *pSocket, IConnectionlessPacketHandler *pHandler, ILookupChannel *pLookup )
{
	CNetPacket *pPacket;
	
	Assert( pSocket );

	// Get datagrams from sockets
	while ( ( pPacket = UDP_GetPacket ( pSocket ) ) != NULL )
	{
		/*
		if ( Filter_ShouldDiscard ( pPacket->m_From ) )	// filterning is done by network layer
		{
			Filter_SendBan( pPacket->m_From );	// tell them we aren't listening...
			continue;
		} 
		*/

		// Find the netchan it came from
		if ( *(unsigned int *)pPacket->m_pData == CONNECTIONLESS_HEADER )
		{
			g_pNetworkSystemImp->EnqueueConnectionlessNetworkPacket( pPacket, pHandler );
		}
		else
		{
			INetChannel *pNetChan = pLookup->FindNetChannel( pPacket->m_From );
			g_pNetworkSystemImp->EnqueueNetworkPacket( pPacket, static_cast<CNetChannel*>( pNetChan ) );
		}
	}
}

void UDP_ProcessConnectionlessPacket( CNetPacket *pPacket, IConnectionlessPacketHandler *pHandler )
{
	// check for connectionless packet (0xffffffff) first
	Assert( *(unsigned int *)pPacket->m_pData == CONNECTIONLESS_HEADER );

	pPacket->m_Message.ReadLong();	// read the -1

	/*
	if ( net_showudp.GetInt() )
	{
		Msg("UDP <- %s: sz=%i OOB '%c'\n", info.m_pPacket->m_From.ToString(), pPacket->m_nSizeInBytes, pPacket->m_pData[4] );
	}
	*/

	pHandler->ProcessConnectionlessPacket( pPacket );
}
