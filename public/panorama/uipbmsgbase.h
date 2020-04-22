//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PROTOBUFPOOL_H
#define PROTOBUFPOOL_H

#ifdef _WIN32
#pragma once
#endif


#include "tier0/tslist.h"
#include "tier1/fmtstr.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#if !defined( SOURCE2_PANORAMA )
#include "tier1/tsmempool.h"
#include "misc.h"
#endif
#include "tier0/vprof.h"

#include "protobuf-2.3.0/src/google/protobuf/message_lite.h"

namespace panorama
{

#define PBMEM_POOL_LOW_TARGET 256
#define PBMEM_POOL_HIGH_TARGET 512

class IProtoBufMsgMemoryPool
{
public:

	virtual ~IProtoBufMsgMemoryPool() {}

	// Methods that need to be exposed out to examine memory
	virtual uint32 GetEstimatedSize() = 0;
	virtual uint32 GetCount() = 0;
	virtual CUtlString GetName() = 0;
	virtual uint32 GetAllocHitCount() = 0;
	virtual uint32 GetAllocMissCount() = 0;
	virtual void Free( void *pObjectVoid ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName ) = 0;
#endif
};


#if defined( SOURCE2_PANORAMA )
#include <tier0/memdbgon.h>
#endif

//-----------------------------------------------------------------------------
// CProtoBufMsgMemoryPool - Implementation for allocation pools for protobufmsgs.
// We create one of these per protobuf msg type, created on first construction of
// an object of that type.
//-----------------------------------------------------------------------------
template< typename PB_OBJECT_TYPE > 
class CUIProtoBufMsgMemoryPool : public IProtoBufMsgMemoryPool
{
public:

	typedef typename CTSList< PB_OBJECT_TYPE * >::Node_t Node;

	CUIProtoBufMsgMemoryPool( uint32 unTargetLow, uint32 unTargetHigh )
	{
		m_unTargetCountLow = unTargetLow;
		m_unTargetCountHigh = unTargetHigh;
		m_unEstimatedSize = 0;
		m_unAllocHitCounter = 0;
		m_unAllocMissCounter = 0;
		m_pTListFreeObjects = new CTSList< PB_OBJECT_TYPE * >();
	}


	~CUIProtoBufMsgMemoryPool()
	{
		Node *pObject = m_pTListFreeObjects->Pop();
		while ( pObject )
		{
			Destruct( pObject->elem );
#if defined( SOURCE2_PANORAMA )
			free( pObject->elem );
#else
			FreePv( pObject->elem );
#endif
			delete pObject;

			pObject = m_pTListFreeObjects->Pop();
		}
		m_pTListFreeObjects->Purge();
		delete m_pTListFreeObjects;
	}

	CUtlString GetName() 
	{ 
		return PB_OBJECT_TYPE::default_instance().GetTypeName().c_str(); 
	}

	uint32 GetEstimatedSize()
	{
		return m_pTListFreeObjects->Count()*m_unEstimatedSize;
	}

	uint32 GetCount() 
	{
		return m_pTListFreeObjects->Count();
	}

	uint32 GetAllocHitCount()
	{
		return m_unAllocHitCounter;
	}

	uint32 GetAllocMissCount()
	{
		return m_unAllocMissCounter;
	}


	Node * AllocProtoBuf()
	{
		Node *pObject = m_pTListFreeObjects->Pop();
		if ( !pObject )
		{
			++m_unAllocMissCounter;
			pObject = new Node;
#if defined( SOURCE2_PANORAMA )
			pObject->elem = (PB_OBJECT_TYPE *)malloc( sizeof( PB_OBJECT_TYPE ) );
#else
			pObject->elem = (PB_OBJECT_TYPE *)PvAllocNoLeakTracking( sizeof( PB_OBJECT_TYPE ) );
#endif
			Construct( pObject->elem );
		}
		else
		{
			++m_unAllocHitCounter;
			bool bFreeAnother = false;
			uint32 unCount = m_pTListFreeObjects->Count();

			// We'll free an extra cached msg every alloc if we are over the higher limit, and every 6th if we
			// are over the lower limit.  This allows some elasticity to peaks in demand.
			if ( unCount > m_unTargetCountHigh )
				bFreeAnother = true;
			else if ( unCount > m_unTargetCountLow && m_unAllocHitCounter % 6 == 0 )
				bFreeAnother = true;

			if ( bFreeAnother )
			{
				// Pop an extra item, so we can get down to target count over time
				Node *pThrowAway = m_pTListFreeObjects->Pop();
				if ( pThrowAway )
				{
					Destruct( pThrowAway->elem );
#if defined( SOURCE2_PANORAMA )
					free( pThrowAway->elem );
#else
					FreePv( pThrowAway->elem );
#endif
					delete pThrowAway;
				}
			}
		}
		return pObject;
	}

	inline void Free( void *pObjectVoid )
	{
		Free( (Node *)pObjectVoid );
	}

	void Free( Node *pObject )
	{
		if ( m_unFreeCounter++ % 2000 == 0 || m_unEstimatedSize == 0 )
		{
			m_unEstimatedSize = pObject->elem->SpaceUsed();
		}

		if ( m_unTargetCountHigh > 0 )
		{
			// We always cache for re-use on free, though we may throw out later on alloc to shrink the pool
			pObject->elem->Clear();
			m_pTListFreeObjects->Push( pObject );
		}
		else
		{
			Destruct( pObject->elem );
#if defined( SOURCE2_PANORAMA )
			free( pObject->elem );
#else
			FreePv( pObject->elem );
#endif
			delete pObject;
		}
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		m_pTListFreeObjects->Validate( validator, "m_pTListFreeObjects" );
		validator.ClaimMemory_Aligned( m_pTListFreeObjects );
	}
#endif // DBGFLAG_VALIDATE

private:
	CTSList< PB_OBJECT_TYPE * > *m_pTListFreeObjects;

	// Not critical for these to be "right" so they don't need to be thread safe
	uint32 m_unEstimatedSize;
	uint32 m_unFreeCounter;

	// These counters are important to get correct, so interlocked in case of allocating on threads
	CInterlockedInt m_unAllocHitCounter;
	CInterlockedInt m_unAllocMissCounter;

	// Only set at construction, so not needed to be thread safe
	uint32 m_unTargetCountLow;
	uint32 m_unTargetCountHigh;
};

#if defined( SOURCE2_PANORAMA )
#include <tier0/memdbgoff.h>
#endif

//-----------------------------------------------------------------------------
// Purpose: Ref count wrapper around protobuf message objects
//-----------------------------------------------------------------------------
class CMsgLiteRefCount 
{
public:
	CMsgLiteRefCount()
	{
		m_pMemoryPool = NULL;
		m_pTSListNode = NULL;
		m_cRef = 1;
	}

	// Has to be public, so we can use in memory pool, should always already be at zero ref count though
	~CMsgLiteRefCount() { DbgAssert( 0 == m_cRef ); }

	inline int AddRef() 
	{ 
		return ThreadInterlockedIncrement( &m_cRef ); 
	}

	int Release();

	inline google::protobuf::MessageLite *AccessMsg()
	{
		return (google::protobuf::MessageLite *)(((CTSList<void *>::Node_t *)m_pTSListNode)->elem);
	}

	IProtoBufMsgMemoryPool *m_pMemoryPool;
	void *m_pTSListNode;
	void *m_pSelfNode;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		

		if ( m_cRef != 0 )
		{
			CTSList<void *>::Node_t *pNode = (CTSList<void *>::Node_t *)m_pSelfNode;

			if ( pNode )
			{

				void *pvMem =  MemAlloc_Unalign( pNode );
				if ( !validator.IsClaimed( pvMem ) )
					validator.ClaimMemory_Aligned( pNode );
			
				if ( !validator.IsClaimed( pNode->elem ) )
					validator.ClaimMemory( pNode->elem );
			}

			pNode = (CTSList<void *>::Node_t *)m_pTSListNode;
			if ( pNode )
			{
				void *pvMem =  MemAlloc_Unalign( pNode );
				if ( !validator.IsClaimed( pvMem ) )
					validator.ClaimMemory_Aligned( pNode );
			}
		}
		
	}
#endif

private:

	friend class CUIProtoBufMsgMemoryPoolMgr;

	volatile int32 m_cRef;
};


//-----------------------------------------------------------------------------
// CProtoBufMsgMemoryPoolMgr - Manages all the message pools for render messages proto buf objects.  
// Should have one global singleton instance of this which tracks all the pools
// for individual message types.
//-----------------------------------------------------------------------------
class CUIProtoBufMsgMemoryPoolMgr
{
public:

	CUIProtoBufMsgMemoryPoolMgr()
	{
		m_pTSListMsgLiteRefCount = new CTSList<CMsgLiteRefCount*>;
	}

	~CUIProtoBufMsgMemoryPoolMgr() 
	{
		CTSList<CMsgLiteRefCount*>::Node_t *pNode = m_pTSListMsgLiteRefCount->Pop();
		while ( pNode )
		{
			delete pNode->elem;
			delete pNode;
			pNode = m_pTSListMsgLiteRefCount->Pop();
		}

		FOR_EACH_VEC( m_vecMsgPools, i )
		{
			delete m_vecMsgPools[i];
		}
		m_vecMsgPools.RemoveAll();
	}

	CTSList<CMsgLiteRefCount*>::Node_t * AllocMsgLiteRef()
	{
		CTSList<CMsgLiteRefCount*>::Node_t *pNode = m_pTSListMsgLiteRefCount->Pop();
		if ( !pNode )
		{
			pNode = new CTSList<CMsgLiteRefCount*>::Node_t;
			pNode->elem = new CMsgLiteRefCount();
			return pNode;
		}
		else
		{
			DbgAssert( pNode->elem->m_cRef == 0 );
			pNode->elem->m_cRef = 1;
		}
		return pNode;
	}

	void FreeMsgLiteRef( CTSList<CMsgLiteRefCount*>::Node_t * pRef )
	{
#if defined(_DEBUG) && !defined(NO_MALLOC_OVERRIDE) && !defined( SOURCE2_PANORAMA )
		Assert( g_pMemAllocSteam->IsValid( pRef->elem ) );
#endif
		m_pTSListMsgLiteRefCount->Push( pRef );
	}

	void RegisterPool( IProtoBufMsgMemoryPool * pPool )
	{
		m_vecMsgPools.AddToTail( pPool );
	}

	void DumpPoolInfo()
	{
		uint32 unTotalSize = 0;
		Msg( "CRenderMsgMemoryPoolMgr:\n" );
		Msg( "  PoolName                                       Count   Est. Size    Hit Rate\n" );
		Msg( "  ----------------------------------------- ----------- ----------- -----------\n" );
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
			Msg( "%43s%12d%12s%12s\n", m_vecMsgPools[i]->GetName().String(), m_vecMsgPools[i]->GetCount(), V_pretifymem( (float)unEstimate, 2, true ), CFmtStr( "%f%%", flHitRate ).Access() );
			unTotalSize += unEstimate;
		}
		Msg( "  -----------------------------------------------------------------------------\n" );
		Msg( "  Total: %s\n", V_pretifymem( (float)unTotalSize, 2, true ) );

		Msg( "  -----------------------------------------------------------------------------\n" );
		Msg( "TSList for CMsgLiteRef size: %s\n", V_pretifymem( (float)m_pTSListMsgLiteRefCount->Count(), 2, true ) );
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_vecMsgPools );
		FOR_EACH_VEC( m_vecMsgPools, i )
		{
			ValidatePtr( m_vecMsgPools[i] );
		}
		validator.ClaimMemory_Aligned( m_pTSListMsgLiteRefCount );
		m_pTSListMsgLiteRefCount->Validate( validator, "m_pTSListMsgLiteRefCount" );

		CUtlVector< CTSList<CMsgLiteRefCount *>::Node_t * > vecTemp;

		CTSList<CMsgLiteRefCount *>::Node_t *pNode = m_pTSListMsgLiteRefCount->Pop();
		while ( pNode )
		{
			ValidatePtrIfNeeded( pNode->elem );
			vecTemp.AddToTail( pNode );
			pNode = m_pTSListMsgLiteRefCount->Pop();
		}

		FOR_EACH_VEC( vecTemp, i )
		{
			m_pTSListMsgLiteRefCount->Push( vecTemp[i] );
		}
	}
#endif // DBGFLAG_VALIDATE

private:
	CUtlVector< IProtoBufMsgMemoryPool * > m_vecMsgPools;
	CTSList<CMsgLiteRefCount *> *m_pTSListMsgLiteRefCount;
};


// Interface that rendermsgs of all types implement
class IUIProtoBufMsg
{
public:
	virtual ~IUIProtoBufMsg() {}
	virtual void SerializeInProc( CUtlBuffer *pBuffer ) const = 0;
};


//-----------------------------------------------------------------------------
// Purpose: Base class for protobuf objects
//-----------------------------------------------------------------------------
template< typename PB_OBJECT_TYPE > 
class CUIProtoBufMsg : public IUIProtoBufMsg
{
private:
	static bool s_bRegisteredWithMemoryPoolMgr;
	static CThreadMutex s_Mutex;
	static CUIProtoBufMsgMemoryPool< PB_OBJECT_TYPE > *s_pMemoryPool;

public:

	typedef typename CTSList<PB_OBJECT_TYPE *>::Node_t Node;

	// Called on construction of each object of this type, but only does work
	// once to setup memory pools for the class type.
	static void OneTimeInit()
	{
		// If we haven't done registration do so now
		if ( !s_bRegisteredWithMemoryPoolMgr )
		{
			// Get the lock and make sure we still haven't
			s_Mutex.Lock();
			if ( !s_bRegisteredWithMemoryPoolMgr )
			{
				s_pMemoryPool = new CUIProtoBufMsgMemoryPool< PB_OBJECT_TYPE >( PBMEM_POOL_LOW_TARGET, PBMEM_POOL_HIGH_TARGET );
				UIEngine()->MsgMemoryPoolMgr()->RegisterPool( s_pMemoryPool );
				s_bRegisteredWithMemoryPoolMgr = true;
			}
			s_Mutex.Unlock();
		}
	}

	CUIProtoBufMsg()
	{
		OneTimeInit();
		CTSList<CMsgLiteRefCount *>::Node_t *pRefCountNode = UIEngine()->MsgMemoryPoolMgr()->AllocMsgLiteRef();
		m_pMsgRefCount = pRefCountNode->elem;
		m_pMsgRefCount->m_pSelfNode = pRefCountNode;
		m_pMsgRefCount->m_pMemoryPool = s_pMemoryPool;
		Node *pNode = s_pMemoryPool->AllocProtoBuf();
		m_pMsgRefCount->m_pTSListNode = pNode;
		m_bIsValid = true;
	}

	// Expose memory pool for direct allocation of underlying PB msg objects
	static Node *AllocProtoBufMsgObject() 
	{
		OneTimeInit();
		return s_pMemoryPool->AllocProtoBuf();
	}

	// Expose memory pool for direct allocation of underlying PB msg objects
	static void FreeProtoBufMsgObject( Node *pMsg ) 
	{
		s_pMemoryPool->Free( pMsg );
	}

	// Construct and deserialize in one
	CUIProtoBufMsg( CUtlBuffer *pBuffer )
	{
		OneTimeInit();
		m_pMsgRefCount = NULL;
		m_bIsValid = BDeserializeInProc( pBuffer );
	}

	// Destructor
	virtual ~CUIProtoBufMsg()
	{
		CleanupAllocations();
	}

	bool BIsValid() { return m_bIsValid; }

	inline void SerializeInProc( CUtlBuffer *pBuffer ) const
	{

		// Ensure enough for type, size, and serialized data
		pBuffer->EnsureCapacity( pBuffer->TellPut() + sizeof(uint32) + sizeof( uint64 ) ); 

		m_pMsgRefCount->AddRef();
		pBuffer->PutUnsignedInt( (int)m_eCmd );
		pBuffer->PutUnsignedInt64( (uint64)m_pMsgRefCount );
	}

	inline bool BDeserializeInProc( CUtlBuffer *pBuffer )
	{
		if ( pBuffer->GetBytesRemaining() < (int)sizeof(uint64) )
			return false;

		uint64 ulPtr = pBuffer->GetUnsignedInt64();
		if ( ulPtr == 0 )
			return false;

		CleanupAllocations();
		m_pMsgRefCount = (CMsgLiteRefCount*)ulPtr;
		m_pMsgRefCount->AddRef();
		return true;
	}
	
	void SerializeCrossProc( CUtlBuffer *pBuffer ) const
	{
		VPROF_BUDGET( "CUIProtoBufMsg::SerializeCrossProc", VPROF_BUDGETGROUP_TENFOOT );
		uint32 unSize = m_pMsgRefCount->AccessMsg()->ByteSize();

		// Ensure enough for type, size, and serialized data
		pBuffer->EnsureCapacity( pBuffer->TellPut() + sizeof(uint32) * 3 + unSize ); // bugbug cboyd - drop to * 2 whenpassthrough is removed below

		pBuffer->PutUnsignedInt( (int)m_eCmd );
		pBuffer->PutUnsignedInt( unSize );
		
		if ( unSize == 0 )
			return;

		uint8 *pBody = (uint8*)pBuffer->Base()+pBuffer->TellPut();
		m_pMsgRefCount->AccessMsg()->SerializeWithCachedSizesToArray( pBody );
		pBuffer->SeekPut( CUtlBuffer::SEEK_CURRENT, unSize );
	}
	
	bool BDeserializeCrossProc( CUtlBuffer *pBuffer )
	{
		VPROF_BUDGET( "CUIProtoBufMsg::BDeserialize", VPROF_BUDGETGROUP_TENFOOT );
		if ( pBuffer->GetBytesRemaining() < (int)sizeof(uint32) )
			return false;
		uint32 unSize = pBuffer->GetUnsignedInt();

		if ( unSize == 0 )
			return true;

		if ( pBuffer->GetBytesRemaining() < (int)unSize )
			return false;

		bool bSucccess = m_pMsgRefCount->AccessMsg()->ParseFromArray( (uint8*)pBuffer->Base()+pBuffer->TellGet(), unSize );
		pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, unSize );

		return bSucccess;
	}

	// Accessors
	PB_OBJECT_TYPE &Body() { return *((PB_OBJECT_TYPE*)(m_pMsgRefCount->AccessMsg())); }
	const PB_OBJECT_TYPE &BodyConst() const { return *((const PB_OBJECT_TYPE*)(m_pMsgRefCount->AccessMsg())); }

protected:
	CMsgLiteRefCount *m_pMsgRefCount;
	int m_eCmd;
	bool m_bIsValid;
private:

	void CleanupAllocations()
	{
		SAFE_RELEASE( m_pMsgRefCount );
	}

};


// Statics
template< typename PB_OBJECT_TYPE > bool CUIProtoBufMsg< PB_OBJECT_TYPE>::s_bRegisteredWithMemoryPoolMgr = false;
template< typename PB_OBJECT_TYPE > CThreadMutex CUIProtoBufMsg< PB_OBJECT_TYPE>::s_Mutex;
template< typename PB_OBJECT_TYPE > CUIProtoBufMsgMemoryPool< PB_OBJECT_TYPE > *CUIProtoBufMsg< PB_OBJECT_TYPE>::s_pMemoryPool = NULL;

} // namespace panorama

#endif //PROTOBUFPOOL_H
