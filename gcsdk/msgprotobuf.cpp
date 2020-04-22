//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================


#include "stdafx.h"
#include <limits.h>
#include "msgprotobuf.h"

#include "tier0/memdbgoff.h"

#ifdef GC
namespace GCSDK
{
IMPLEMENT_CLASS_MEMPOOL( CProtoBufNetPacket, 1000, UTLMEMORYPOOL_GROW_SLOW );
}
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
// Global pool for protobuf header allocations
CProtoBufMsgMemoryPoolMgr *GProtoBufMsgMemoryPoolMgr()
{
	static CProtoBufMsgMemoryPoolMgr s_ProtoBufMsgMemoryPoolMgr;
	return &s_ProtoBufMsgMemoryPoolMgr;
}

CThreadMutex CProtoBufMsgBase::s_PoolRegMutex;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CProtoBufNetPacket::CProtoBufNetPacket( CNetPacket *pNetPacket, GCProtoBufMsgSrc eMsgSrc, const CSteamID steamID, uint32 nGCDirIndex, MsgType_t msgType )
	: m_msgType( msgType ), m_steamID( steamID ), m_bIsValid( false )
{
#ifdef VALVE_BIG_ENDIAN
	ProtoBufMsgHeader_t * pHdr = (ProtoBufMsgHeader_t *)pNetPacket->PubData();
	pHdr->m_EMsgFlagged = LittleDWord( pHdr->m_EMsgFlagged );
	pHdr->m_cubProtoBufExtHdr = LittleDWord( pHdr->m_cubProtoBufExtHdr );
#endif

	m_pNetPacket = pNetPacket;
	m_pNetPacket->AddRef();

	m_pHeader = GProtoBufMsgMemoryPoolMgr()->AllocProtoBufHdr();

	// Pull the length of the header out of the packet, but then validate it's not longer than the packet itself (ie, corrupt packet)
	const uint32 unLenProtoBufHeader = GetFixedHeader().m_cubProtoBufExtHdr;
	if ( unLenProtoBufHeader + sizeof( ProtoBufMsgHeader_t ) <= pNetPacket->CubData() )
	{
		if ( !m_pHeader->ParseFromArray( pNetPacket->PubData()+sizeof(ProtoBufMsgHeader_t), unLenProtoBufHeader ) )
		{
			AssertMsg4( false, "Failed parsing ProtoBuf extended header out of message: %s(%d) from %s size=%u", PchMsgNameFromEMsg( GetEMsg() ), GetEMsg(), steamID.Render(), pNetPacket->CubData() );
		}
		else
		{
			//if this packet doesn't have a source provided, we need to set the source of this message. If one is already provided, we should never stomp that data as that is the ultimate source of the message
			if( m_pHeader->gc_msg_src() == GCProtoBufMsgSrc_Unspecified )
			{
				//make sure to set the steam ID to what steam provided so clients can't spoof it
				m_pHeader->set_client_steam_id( steamID.ConvertToUint64() );
				//and track our original source type and which GC it was first received by
				m_pHeader->set_gc_dir_index_source( nGCDirIndex );
				m_pHeader->set_gc_msg_src( eMsgSrc );
			}
			
			m_bIsValid = true;
		}
	}
	else
	{
		AssertMsg4( false, "Unparseable protobuf message arrived: message %s(%u) from %s size=%u", PchMsgNameFromEMsg( GetEMsg() ), GetEMsg(), steamID.Render(), pNetPacket->CubData() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CProtoBufNetPacket::~CProtoBufNetPacket()
{
	GProtoBufMsgMemoryPoolMgr()->FreeProtoBufHdr( m_pHeader );
	m_pNetPacket->Release();
}

//-----------------------------------------------------------------------------
// Purpose: Gets the body from the associated packet
//-----------------------------------------------------------------------------
bool CProtoBufNetPacket::GetMsgBody( const uint8*& pubData, uint32& cubData ) const
{
	if ( !IsValid() )
	{
		pubData = NULL;
		cubData = 0;
		return false;
	}

	//when it initializes, it verifies the size, so we don't need to do validation here
	const uint32 nHdrOffset = sizeof( ProtoBufMsgHeader_t ) + GetFixedHeader().m_cubProtoBufExtHdr;
	pubData = PubData() + nHdrOffset;
	cubData = CubData() - nHdrOffset;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CProtoBufMsgMemoryPoolBase::CProtoBufMsgMemoryPoolBase( uint32 unTargetLow, uint32 unTargetHigh )
{
	m_unTargetCountLow = unTargetLow;
	m_unTargetCountHigh = unTargetHigh;
	m_unAllocHitCounter = 0;
	m_unAllocMissCounter = 0;
	m_unAllocated = 0;
	m_pTSQueueFreeObjects = new CTSQueue< ::google::protobuf::Message * >();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CProtoBufMsgMemoryPoolBase::~CProtoBufMsgMemoryPoolBase()
{
	AssertMsg( 0 == m_pTSQueueFreeObjects->Count(), "CProtoBufMsgMemoryPoolBase destructor called with items still in the queue. They must be freed by the derived class or they will be leaked" );

	m_pTSQueueFreeObjects->Purge();
	delete m_pTSQueueFreeObjects;
}


//-----------------------------------------------------------------------------
// Purpose: Allocs a message from the pool
//-----------------------------------------------------------------------------
::google::protobuf::Message *CProtoBufMsgMemoryPoolBase::Alloc()
{
	::google::protobuf::Message *pObject;
	if ( !m_pTSQueueFreeObjects->PopItem( &pObject ) || !pObject )
	{
		++m_unAllocMissCounter;
		pObject = InternalAlloc();
	}
	else
	{
		++m_unAllocHitCounter;
		uint32 unCount = m_pTSQueueFreeObjects->Count();

		// We'll free an extra cached msg every alloc if we are over the higher limit, and every 6th if we
		// are over the lower limit.  This allows some elasticity to peaks in demand.
		bool bFreeAnother = ( unCount > m_unTargetCountHigh )
						 || ( unCount > m_unTargetCountLow && m_unAllocHitCounter % 6 == 0 );

		if ( bFreeAnother )
		{
			// Pop an extra item, so we can get down to target count over time
			::google::protobuf::Message *pThrowAway;
			if ( m_pTSQueueFreeObjects->PopItem( &pThrowAway ) && pThrowAway )
			{
				InternalFree( pThrowAway );
			}
		}
	}

	++m_unAllocated;
	return pObject;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a message to the pool
//-----------------------------------------------------------------------------
void CProtoBufMsgMemoryPoolBase::Free( ::google::protobuf::Message *pObject )
{
	// We always cache for re-use on free, though we may throw out later on alloc to shrink the pool
	pObject->Clear();

	--m_unAllocated;
	m_pTSQueueFreeObjects->PushItem( pObject );
}


//-----------------------------------------------------------------------------
// Purpose: Counts the size of all the objects in the pool
//-----------------------------------------------------------------------------
uint32 CProtoBufMsgMemoryPoolBase::GetEstimatedSize()
{
	// to fix a linux build break
	typedef ::google::protobuf::Message ProtoMsg_t;
	CUtlVector<ProtoMsg_t *> vecTemp;
	vecTemp.EnsureCapacity( m_pTSQueueFreeObjects->Count() * 2 );

	// Should be "while (pObject != NULL)" because if we were actually using threads depending on count
	// would be bad. Unfortunately that gives me an infinite loop. See if this can be changed when this
	// is updated
	while ( m_pTSQueueFreeObjects->Count() > 0 )
	{	
		::google::protobuf::Message *pObject = NULL;
		m_pTSQueueFreeObjects->PopItem( &pObject );
		vecTemp.AddToTail( pObject );
	}

	uint32 unEstimate = 0;
	FOR_EACH_VEC( vecTemp, i )
	{
		unEstimate += vecTemp[i]->SpaceUsed();
		m_pTSQueueFreeObjects->PushItem( vecTemp[i] );
	}

	// Scale the estimate by the number of objects outstanding
	float fScale = 1.0f;
	if ( vecTemp.Count() > 0 )
	{
		float fFree = (float)vecTemp.Count();
		fScale = ( fFree + (float)m_unAllocated ) / fFree;
		unEstimate *= fScale;
	}

	return unEstimate;
}


//-----------------------------------------------------------------------------
// Purpose: Pops an item from the queue. To be used by the derived class
//	destructor to free the oustanding allocations
//-----------------------------------------------------------------------------
bool CProtoBufMsgMemoryPoolBase::PopItem( google::protobuf::Message **ppMsg )
{
	return m_pTSQueueFreeObjects->PopItem( ppMsg );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CProtoBufMsgMemoryPoolMgr::CProtoBufMsgMemoryPoolMgr() : m_PoolHeaders()
{
	m_vecMsgPools.AddToTail( &m_PoolHeaders );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CProtoBufMsgMemoryPoolMgr::~CProtoBufMsgMemoryPoolMgr() 
{
	FOR_EACH_VEC( m_vecMsgPools, i )
	{
		if ( m_vecMsgPools[i] != &m_PoolHeaders )
			delete m_vecMsgPools[i];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Takes control of a message pool registered for a specific protobuf type
//-----------------------------------------------------------------------------
void CProtoBufMsgMemoryPoolMgr::RegisterPool( CProtoBufMsgMemoryPoolBase *pPool )
{
	m_vecMsgPools.AddToTail( pPool );
}


//-----------------------------------------------------------------------------
// Purpose: Prints info on protobuf memory usage
//-----------------------------------------------------------------------------
void CProtoBufMsgMemoryPoolMgr::DumpPoolInfo()
{
	uint32 unTotalSize = 0;
	Msg( "CProtoBufMsgMemoryPoolMgr:\n" );
	Msg( "  PoolName                                   Allocated        Free   Est. Size    Hit Rate\n" );
	Msg( "  ----------------------------------------- ----------- ----------- ----------- -----------\n" );
	FOR_EACH_VEC( m_vecMsgPools, i )
	{
		uint32 unHitCount = m_vecMsgPools[i]->GetAllocHitCount();
		uint32 unMissCount = m_vecMsgPools[i]->GetAllocMissCount();
		float flHitRate = 0.0f;
		if ( unHitCount > 0 || unMissCount > 0 )
		{
			flHitRate = (float)unHitCount / (float)(unHitCount+unMissCount);
			flHitRate *= 100.0f;
		}

		uint32 unEstimate = m_vecMsgPools[i]->GetEstimatedSize();
		Msg( "%43s%12d%12d%12s%12s\n", m_vecMsgPools[i]->GetName().String(), m_vecMsgPools[i]->GetAllocated(), m_vecMsgPools[i]->GetFree(), Q_pretifymem( (float)unEstimate, 2, true ), CFmtStr( "%f%%", flHitRate ).Access() );
		unTotalSize += unEstimate;
	}
	Msg( "  -----------------------------------------------------------------------------------------\n" );
	Msg( "  Total: %s\n", Q_pretifymem( (float)unTotalSize, 2, true ) );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor - no eMsg, so it's a receive constructor. Don't alloc
//	a header in this case
//-----------------------------------------------------------------------------
CProtoBufMsgBase::CProtoBufMsgBase()
	: m_pNetPacket( NULL )
	, m_eMsg( 0 )
	, m_pProtoBufHdr( NULL )
{ 
}


//-----------------------------------------------------------------------------
// Purpose: Constructor - Has an eMsg, so it's a send constructor. Alloc a header
//-----------------------------------------------------------------------------
CProtoBufMsgBase::CProtoBufMsgBase( MsgType_t eMsg )
	: m_pNetPacket( NULL )
	, m_eMsg( eMsg )
	, m_pProtoBufHdr( NULL )
{ 
	m_pProtoBufHdr = GProtoBufMsgMemoryPoolMgr()->AllocProtoBufHdr();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CProtoBufMsgBase::~CProtoBufMsgBase()
{
	// If we have a net packet then it allocated the header, so we don't free it ourselves
	// If we don't have a net packet then we still may have not allocated a header ( default constructor )
	if ( m_pNetPacket )
	{
		m_pNetPacket->Release();
		m_pNetPacket = NULL;
	}
	else if ( m_pProtoBufHdr )
	{
		GProtoBufMsgMemoryPoolMgr()->FreeProtoBufHdr( m_pProtoBufHdr );
	}

	m_pProtoBufHdr = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Inits a message from a received net packet
//-----------------------------------------------------------------------------
bool CProtoBufMsgBase::InitFromPacket( IMsgNetPacket * pNetPacket )
{
	VPROF_BUDGET( "CProtoBufMsg::InitFromPacket( IMsgNetPacket )", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	AssertMsg( NULL == m_pProtoBufHdr, "Encountered a packet being initialized that has outstanding memory. This will leak memory, and is often caused by receiving messages using the wrong constructor which preallocates or not clearing the message between uses" );
	Assert( k_EMsgFormatTypeProtocolBuffer == pNetPacket->GetEMsgFormatType() );

	m_pNetPacket = static_cast<CProtoBufNetPacket *>( pNetPacket );
	m_pNetPacket->AddRef();
	ProtoBufMsgHeader_t &refFixedHdr = m_pNetPacket->GetFixedHeader();

	m_eMsg = refFixedHdr.m_EMsgFlagged;
	m_pProtoBufHdr = m_pNetPacket->GetProtoHeader();

	const uint8* pubData;
	uint32 cubData;
	if( !m_pNetPacket->GetMsgBody( pubData, cubData ) )
		return false;
		
	::google::protobuf::Message *pBody = GetGenericBody();
	return pBody->ParseFromArray( pubData, (int)cubData );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message using the given handler 
//-----------------------------------------------------------------------------
bool CProtoBufMsgBase::BAsyncSend( IProtoBufSendHandler & sender ) const
{
	return BAsyncSendProto( sender, GetEMsg(), *m_pProtoBufHdr, *GetGenericBody() );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a protobuf object out as a message
//-----------------------------------------------------------------------------
bool CProtoBufMsgBase::BAsyncSendProto( IProtoBufSendHandler& sender, MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, const ::google::protobuf::Message& proto )
{
	VPROF_BUDGET( "CProtoBufMsg::BAsyncSendProto", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	// Compute message sizes and serialize out to buffer.  Need to work on a more efficient 
	// way to write these to the net without so much copying.
	uint32 cubBodySize = proto.ByteSize();

	uint32 cubTotalSize = 0;
	uint8* pMsgMem = AllocateMessageMemory( eMsgType, hdr, cubBodySize, &cubTotalSize );
	proto.SerializeWithCachedSizesToArray( pMsgMem + cubTotalSize - cubBodySize );

	bool bSuccess = sender.BAsyncSend( eMsgType, pMsgMem, cubTotalSize );
	FreeMessageMemory( pMsgMem );

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message using the given handler 
//-----------------------------------------------------------------------------
bool CProtoBufMsgBase::BAsyncSendWithPreSerializedBody( IProtoBufSendHandler & pSender, const byte *pubBody, uint32 cubBody ) const
{
	return BAsyncSendWithPreSerializedBody( pSender, GetEMsg(), *m_pProtoBufHdr, pubBody, cubBody );
}

//-----------------------------------------------------------------------------
//free standing version to send a protobuff given a header and pre-serialized body. Primarily used for efficient message routing
//-----------------------------------------------------------------------------
bool CProtoBufMsgBase::BAsyncSendWithPreSerializedBody( IProtoBufSendHandler& sender, MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, const byte* pubBody, uint32 cubBody )
{
	VPROF_BUDGET( "CProtoBufMsg::BAsyncSendWithPreSerializedBody", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	// Compute message sizes and serialize out to buffer.  Need to work on a more efficient 
	// way to write these to the net without so much copying.
	uint32 cubTotalSize = 0;
	uint8* pMsgMem = AllocateMessageMemory( eMsgType, hdr, cubBody, &cubTotalSize );

	V_memcpy( pMsgMem + cubTotalSize - cubBody, pubBody, cubBody );

	bool bSuccess = sender.BAsyncSend( eMsgType, pMsgMem, cubTotalSize );

	FreeMessageMemory( pMsgMem );

	return bSuccess;
}


//-----------------------------------------------------------------------------
//utility function that handles allocating a memory pool big enough for the provided header and specified body
//size and writing the header into the pool. This will return a pointer to the memory, as well as the header size.
//-----------------------------------------------------------------------------
uint8* CProtoBufMsgBase::AllocateMessageMemory( MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, uint32 cubBodySize, uint32* pCubTotalSizeOut )
{
	uint32 cubExtHdrSize = hdr.ByteSize();
	uint32 cubTotalSize = sizeof( ProtoBufMsgHeader_t ) + cubExtHdrSize + cubBodySize;
	ProtoBufMsgHeader_t fixedHdr( eMsgType, cubExtHdrSize );

	uint8 *pubMsgBytes = (uint8*)g_MemPoolMsg.Alloc( cubTotalSize );

	// Copy over basic fixed header
	// We place bytes on the network in little endian to optimize for those platforms, even though network byte order is really big endian.
	#if defined(VALVE_BIG_ENDIAN)
		ProtoBufMsgHeader_t hdrByteSwapped = fixedHdr;
		hdrByteSwapped.m_EMsgFlagged = DWordSwap( hdrByteSwapped.m_EMsgFlagged );
		hdrByteSwapped.m_cubProtoBufExtHdr = DWordSwap( hdrByteSwapped.m_cubProtoBufExtHdr );
		Q_memcpy( pubMsgBytes, &hdrByteSwapped, sizeof( ProtoBufMsgHeader_t ) );
	#else
		Q_memcpy( pubMsgBytes, &fixedHdr, sizeof( ProtoBufMsgHeader_t ) );
	#endif

	// Serialize extended pb header, we guarantee we didn't modify the message since the last call to ByteSize above, so use the cached sizes serialize
	uint8 *pEnd = hdr.SerializeWithCachedSizesToArray( pubMsgBytes + sizeof( ProtoBufMsgHeader_t ) );
	NOTE_UNUSED( pEnd );
	Assert( pEnd == pubMsgBytes + sizeof( ProtoBufMsgHeader_t ) + cubExtHdrSize );

	if( pCubTotalSizeOut )
		*pCubTotalSizeOut = cubTotalSize;
	return pubMsgBytes;
}

void CProtoBufMsgBase::FreeMessageMemory( uint8* pMemory )
{
	g_MemPoolMsg.Free( pMemory );
}

}