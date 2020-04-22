//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Holds the CNetPacket class
//
//=============================================================================


#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CNetPacket::CNetPacket()
{
	m_cRef = 0;
	m_pubData = NULL;
	m_cubData = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor, validates refcount
//-----------------------------------------------------------------------------
CNetPacket::~CNetPacket()
{
	Assert( m_cRef == 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Inits all members
// Input  : hConnection - connection we're from
//			pubData - message data
//			cubData - message size
//			*pubNetworkBuffer - network buffer that contains the message
//								assumes control of this memory
//-----------------------------------------------------------------------------
void CNetPacket::Init( uint32 cubData, const void* pCopyData )
{
	Assert( cubData );

	m_pubData = (uint8 *)g_MemPoolMsg.Alloc( cubData );
	m_cubData = cubData;
	
	if( pCopyData )
	{
		Q_memcpy( m_pubData, pCopyData, cubData );
	}

	AddRef();
}


//-----------------------------------------------------------------------------
// Purpose: adds to refcount
//-----------------------------------------------------------------------------
void CNetPacket::AddRef()		
{ 
	++m_cRef;
}

//-----------------------------------------------------------------------------
void CNetPacket::InitAdoptBuffer( uint32 cubData, uint8* pubData )
{
	Assert( !m_pubData );
	
	m_pubData = pubData;
	m_cubData = cubData;

	AddRef();
}

//-----------------------------------------------------------------------------
void CNetPacket::OrphanBuffer()
{
	m_pubData = NULL;
	m_cubData = 0;
}

//-----------------------------------------------------------------------------
// Purpose: decrements refcount 
//-----------------------------------------------------------------------------
void CNetPacket::Release()	
{ 
	Assert( m_cRef > 0 );
	if ( --m_cRef == 0 ) 
	{
		// delete the network buffer we're associated with, if we have one
		if ( m_pubData )
		{
			g_MemPoolMsg.Free( m_pubData );
		}
		// delete ourselves
		g_cNetPacket--;
		CNetPacketPool::sm_MemPoolNetPacket.Free( this );
	}
}

} // namespace GCSDK
