//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Serialized Digital Object caching and manipulation
//
//=============================================================================

#ifndef SBOCACHE_H
#define SBOCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlhashmaplarge.h"
#include "tier1/utlqueue.h"
#include "tier1/utlvector.h"
#include <algorithm>

namespace GCSDK
{
// Call to register SDOs. All SDO types must be registered before loaded
#define REG_SDO( classname )	GSDOCache().RegisterSDO( classname::k_eType, #classname )

// A string used to tell the difference between nil objects and actual objects in memcached
extern const char k_rgchNilObjSerializedValue[];


//-----------------------------------------------------------------------------
// Purpose: Keeps a moving average of a data set
//-----------------------------------------------------------------------------
template< int SAMPLES >
class CMovingAverage
{
public:
	CMovingAverage()
	{
		Reset();
	}

	void Reset()
	{
		memset( m_rglSamples, 0, sizeof( m_rglSamples ) );
		m_cSamples = 0;
		m_lTotal = 0;
	}

	void AddSample( int64 lSample )
	{
		int iIndex = m_cSamples % SAMPLES;
		m_lTotal += ( lSample - m_rglSamples[iIndex] );
		m_rglSamples[iIndex] = lSample;
		m_cSamples++;
	}

	uint64 GetAveragedSample() const
	{
		if ( !m_cSamples )
			return 0;

		int64 iMax = (int64)MIN( m_cSamples, SAMPLES );
		return m_lTotal / iMax;
	}

private:
	int64 m_rglSamples[SAMPLES];
	int64 m_lTotal;
	uint64 m_cSamples;
};


//-----------------------------------------------------------------------------
// Purpose: Global accessor to the manager
//-----------------------------------------------------------------------------
class CSDOCache;
CSDOCache &GSDOCache();


//-----------------------------------------------------------------------------
// Purpose: interface to a Database Backed Object
//-----------------------------------------------------------------------------
class ISDO
{
public:
	virtual ~ISDO() {}

	// Identification
	virtual int GetType() const = 0;
	virtual uint32 GetHashCode() const = 0;
	virtual bool IsEqual( const ISDO *pSDO ) const = 0;

	// Ref counting
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	virtual int GetRefCount() = 0;

	// memory usage
	virtual size_t CubBytesUsed() = 0;

	// Serialization tools
	virtual bool BReadFromBuffer( const byte *pubData, int cubData ) = 0;
	virtual void WriteToBuffer( CUtlBuffer &memBuffer ) = 0;

	// memcached batching tools
	virtual void GetMemcachedKeyName( CFmtStr &strDest ) = 0;

	// SQL loading
	virtual bool BYldLoadFromSQL( CUtlVector<ISDO *> &vecSDOToLoad, CUtlVector<bool> &vecResults ) const = 0;

	// post-load initialization (whether loaded from SQL or memcached)
	virtual void PostLoadInit() = 0;

	// comparison function for validating memcached copies vs SQL copies
	virtual bool IsIdentical( ISDO *pSDO ) = 0;

	// Returns true if this is not the actual object, but a placeholder to remember that this object
	// doesn't actually exist
	virtual bool BNilObject() const = 0;

	// Allocs an SDO that must return true for IsEqual( this ) and BNilObject(). This is so we can
	// cache load attempts for objects that don't exist
	virtual ISDO *AllocNilObject() = 0;

	// Creates a key name that looks like "Prefix_%u" but faster
};


//-----------------------------------------------------------------------------
// Purpose: base class for a Serialized Digital Object
//-----------------------------------------------------------------------------
template<typename KeyType, int eSDOType, class ProtoMsg>
class CBaseSDO : public ISDO
{
public:
	typedef KeyType KeyType_t;
	enum { k_eType = eSDOType };

	CBaseSDO( const KeyType &key ) : m_Key( key ), m_nRefCount( 0 ) {}

	const KeyType &GetKey() const		{ return m_Key; }

	// ISDO implementation
	virtual int AddRef();
	virtual int Release();
	virtual int GetRefCount();
	virtual int GetType() const			{ return eSDOType; }
	virtual bool BNilObject() const			{ return false; }
	virtual uint32 GetHashCode() const;
	virtual bool BReadFromBuffer( const byte *pubData, int cubData );
	virtual void WriteToBuffer( CUtlBuffer &memBuffer );

	// Implement this in a subclass if there's some value like 0 or -1 that's invalid so
	// the system can know to not even attempt to load it
	static bool BKeyValid( const KeyType_t &key ) { return true; }

	// We use protobufs for all serialization
	virtual void SerializeToProtobuf( ProtoMsg &msg ) = 0;
	virtual bool DeserializeFromProtobuf( const ProtoMsg &msg ) = 0;

	// default comparison function - override to do your own compare
	virtual bool IsEqual( const ISDO *pSDO ) const;

	// default load from SQL is no-op as not all types have permanent storage - override to create a
	// batch load
	virtual bool BYldLoadFromSQL( CUtlVector<ISDO *> &vecSDOToLoad, CUtlVector<bool> &vecResults ) const;

	// override to do initialization after load
	virtual void PostLoadInit() {}

	// compares the serialized versions by default. Override to have more specific behavior
	virtual bool IsIdentical( ISDO *pSDO );

	// Creates a copy of the object with the same key
	virtual ISDO *AllocNilObject();

	// tools
	bool WriteToMemcached();
	bool DeleteFromMemcached();

	// Creates a key name that looks like "Prefix_%u" but faster. Also makes sure the correct buffer size is passed
	template <size_t prefixBufSize>
	void CreateSimpleMemcachedName( CFmtStr &strDest, char (&rgchPrefix)[prefixBufSize], uint32 unSuffix )
	{
		CSDOCache::CreateSimpleMemcachedName( strDest, rgchPrefix, prefixBufSize - 1, unSuffix );
	}

private:
	int m_nRefCount;
	KeyType m_Key;
};


//-----------------------------------------------------------------------------
// Purpose: Represents an object we tried to load but didn't exist. We use
//	this to cache the failure so we don't keep trying to look it up
//-----------------------------------------------------------------------------
template<typename KeyType, int eSDOType, class ProtoMsg>
class CNilSDO : public CBaseSDO<KeyType, eSDOType, ProtoMsg>
{
public:
	CNilSDO( const KeyType &key, const char *pchMemcachedKeyName ) : CBaseSDO<KeyType, eSDOType, ProtoMsg>( key ), m_sMemcachedKeyName( pchMemcachedKeyName ) {}

	virtual bool BNilObject() const										{ return true; }
	virtual bool BReadFromBuffer( const byte *pubData, int cubData )	{ return false; }
	virtual void WriteToBuffer( CUtlBuffer &memBuffer )					{ memBuffer.PutString( k_rgchNilObjSerializedValue ); }
	virtual void SerializeToProtobuf( ProtoMsg &msg )					{}
	virtual bool DeserializeFromProtobuf( const ProtoMsg &msg )			{ return false; }
	virtual size_t CubBytesUsed()										{ return sizeof( *this ) + m_sMemcachedKeyName.Length(); }
	virtual void GetMemcachedKeyName( CFmtStr &strKey )					{ V_strncpy( strKey.Access(), m_sMemcachedKeyName, FMTSTR_STD_LEN ); }
private:
	CUtlString m_sMemcachedKeyName;
};


//-----------------------------------------------------------------------------
// Purpose: references to a database-backed object
//			maintains refcount of the object
//-----------------------------------------------------------------------------
template<class T>
class CSDORef
{
	T *m_pSDO;
public:
	CSDORef() { m_pSDO = NULL; }
	explicit CSDORef( CSDORef<T> &SDORef ) { m_pSDO = SDORef.Get(); if ( m_pSDO ) m_pSDO->AddRef(); }
	explicit CSDORef( T *pSDO ) { m_pSDO = pSDO; if ( m_pSDO ) m_pSDO->AddRef(); }
	~CSDORef() { if ( m_pSDO ) m_pSDO->Release(); }

	T *Get() { return m_pSDO; }
	const T *Get() const { return m_pSDO; }

	T *operator->() { return Get(); }
	const T *operator->() const { return Get(); }

	operator const T *() const	{ return m_pSDO; }
	operator const T *()        { return m_pSDO; }
	operator T *()				{ return m_pSDO; }

	CSDORef<T> &operator=( T *pSDO ) { if ( m_pSDO ) m_pSDO->Release();  m_pSDO = pSDO; if ( m_pSDO ) m_pSDO->AddRef(); return *this; }

	bool operator !() const { return Get() == NULL; }

	bool IsValid( void ) const { return Get() != NULL; }
};


//-----------------------------------------------------------------------------
// Purpose: manages a cache of SDO objects
//-----------------------------------------------------------------------------
class CSDOCache
{
public:
	CSDOCache();
	~CSDOCache();

	// Call to register SDOs. All SDO types must be registered before loaded
	void RegisterSDO( int nType, const char *pchName );

	// A struct to hold stats for the system. This is generated code in Steam. It would be great to make
	// it generated code here if we could bring Steam's operational stats system in the GC
	struct StatsSDOCache_t
	{
		uint64 m_cItemsLRUd;
		uint64 m_cBytesLRUd;
		uint64 m_cItemsUnreferenced;
		uint64 m_cBytesUnreferenced;
		uint64 m_cItemsInCache;
		uint64 m_cBytesInCacheEst;
		uint64 m_cItemsQueuedToLoad;
		uint64 m_cItemsLoadedFromMemcached;
		uint64 m_cItemsLoadedFromSQL;
		uint64 m_cItemsFailedLoadFromSQL;
		uint64 m_cQueuedMemcachedRequests;
		uint64 m_cQueuedSQLRequests;
		uint64 m_nSQLBatchSizeAvgx100;
		uint64 m_nMemcachedBatchSizeAvgx100;
		uint64 m_cSQLRequestsRejectedTooBusy;
		uint64 m_cMemcachedRequestsRejectedTooBusy;
		uint64 m_cNilItemsLoadedFromMemcached;
		uint64 m_cNilItemsLoadedFromSQL;
	};

	// loads a SDO, and assigns a reference to it
	// returns false if the item couldn't be loaded, or timed out loading
	template<class T>
	bool BYldLoadSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key, bool *pbTimeoutLoading = NULL );

	// gets access to a SDO, but only if it's currently loaded
	template<class T>
	bool BGetLoadedSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key, bool *pbFoundNil = NULL );

	// starts loading a SDO you're going to reference soon with the above BYldLoadSDO()
	// use this to batch up requests, hinting a set then getting reference to a set is significantly faster
	template<class T>
	void HintLoadSDO( const typename T::KeyType_t &key );
	// as above, but starts load a set
	template<class T>
	void HintLoadSDO( const CUtlVector<typename T::KeyType_t> &vecKeys );

	// Clears a nil object if one exists for this key.
	template<class T>
	void RemoveNil( const typename T::KeyType_t &key );

	// force a deletes a SDO from the cache - waits until the object is not referenced
	template<class T>
	bool BYldDeleteSDO( const typename T::KeyType_t &key, uint64 unMicrosecondsToWaitForUnreferenced );

	// SDO refcount management
	void OnSDOReferenced( ISDO *pSDO );
	void OnSDOReleased( ISDO *pSDO );

	// writes a SDO to memcached immediately
	bool WriteSDOToMemcached( ISDO *pSDO );
	// delete the SDO record from memcached
	bool DeleteSDOFromMemcached( ISDO *pSDO );

	// job results
	void OnSDOLoadSuccess( int eSDO, int iRequestID, bool bNilObj, ISDO **ppSDO );
	void OnMemcachedSDOLoadFailure( int eSDO, int iRequestID );
	void OnSQLSDOLoadFailure( int eSDO, int iRequestID, bool bSQLLayerSucceeded );
	void OnMemcachedLoadJobComplete( JobID_t jobID );
	void OnSQLLoadJobComplete( int eSDO, JobID_t jobID );

	// test access - deletes all unreferenced objects
	void Flush();

	// stats access
	StatsSDOCache_t &GetStats()	{ return m_StatsSDOCache; }
	int CubReferencedEst();	// number of bytes referenced in the cache

	// prints info about the class
	void Dump();

	static void CreateSimpleMemcachedName( CFmtStr &strDest, const char *pchPrefix, uint32 unPrefixLen, uint32 unSuffix );

	// memcached verification - returns the number of mismatches
//**tempcomment**		void YldVerifyMemcachedData( CreateSDOFunc_t pCreateSDOFunc, CUtlVector<uint32> &vecIDs, int *pcMatches, int *pcMismatches );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

	// Functions that need to be in the frame loop
	virtual bool BFrameFuncRunJobsUntilCompleted( CLimitTimer &limitTimer );
	virtual bool BFrameFuncRunMemcachedQueriesUntilCompleted( CLimitTimer &limitTimer );
	virtual bool BFrameFuncRunSQLQueriesUntilCompleted( CLimitTimer &limitTimer );

private:
	// Custom comparator for our hash map
	class CDefPISDOEquals
	{
	public:
		CDefPISDOEquals() {}
		CDefPISDOEquals( int i ) {}
		inline bool operator()( const ISDO *lhs, const ISDO *rhs ) const { return ( lhs->IsEqual( rhs ) );	}
		inline bool operator!() const { return false; }
	};

	class CPISDOHashFunctor
	{
	public:
		uint32 operator()(const ISDO *pSDO ) const { return pSDO->GetHashCode(); }
	};

	template<class T>
	int FindLoadedSDO( const typename T::KeyType_t &key );

	template<class T>
	int QueueLoad( const typename T::KeyType_t &key );
	int QueueMemcachedLoad( ISDO *pSDO );

	// items already loaded - Maps the SDO to the LRU position
	CUtlHashMapLarge<ISDO *, int, CDefPISDOEquals, CPISDOHashFunctor> m_mapISDOLoaded;

	// items we have queued to load, in the state of either being loaded from memcached or SQL
	// maps SDO to a list of jobs waiting on the load
	CUtlHashMapLarge<ISDO *, CCopyableUtlVector<JobID_t>, CDefPISDOEquals, CPISDOHashFunctor> m_mapQueuedRequests;

	// requests to load from memcached
	CUtlLinkedList<int, int> m_queueMemcachedRequests;

	// Jobs currently processing memcached load requests
	CUtlVector<JobID_t> m_vecMemcachedJobs;

	// Loading from SQL is divided by SDO type
	struct SQLRequestManager_t
	{
		// requests to load from SQL. Maps to an ID in the map of queued requests
		CUtlLinkedList<int, int> m_queueRequestIDsToLoadFromSQL;

		// SQL jobs we have active doing reads for cache items
		CUtlVector<JobID_t> m_vecSQLJobs;
	};

	// a queue of requests to load from SQL for each type
	CUtlHashMapLarge<int, SQLRequestManager_t *> m_mapQueueSQLRequests;

	// jobs to wake up, since we've satisfied their SDO load request
	struct JobToWake_t
	{
		JobID_t m_jobID;
		bool m_bLoadLayerSuccess;
	};
	CUtlLinkedList<JobToWake_t, int> m_queueJobsToContinue;

	struct LRUItem_t
	{
		ISDO *	m_pSDO;
		size_t	m_cub;
	};
	CUtlLinkedList<LRUItem_t, int> m_listLRU;
	uint32 m_cubLRUItems;
	void RemoveSDOFromLRU( int iMapSDOLoaded );

	struct TypeStats_t
	{
		TypeStats_t() 
			: m_nLoaded( 0 )
			, m_nRefed( 0 )
			, m_cubUnrefed( 0 )
			, m_nNilObjects( 0 )
		{}

		CUtlString	m_strName;
		int			m_nLoaded;
		int			m_nRefed;
		int			m_cubUnrefed;
		int			m_nNilObjects;
	};

	StatsSDOCache_t m_StatsSDOCache;
	CMovingAverage<100> m_StatMemcachedBatchSize, m_StatSQLBatchSize;
	CUtlMap<int, TypeStats_t> m_mapTypeStats;
};


//-----------------------------------------------------------------------------
// Definition of CBaseSDO template functions now that CSDOCache is defined and
// GSDOCache() can safely be used.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: adds a reference
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
int CBaseSDO<KeyType,ESDOType,ProtoMsg>::AddRef()
{
	if ( ++m_nRefCount == 1 )
		GSDOCache().OnSDOReferenced( this );

	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: releases a reference
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
int CBaseSDO<KeyType,ESDOType,ProtoMsg>::Release()
{
	DbgVerify( m_nRefCount > 0 );

	int nRefCount = --m_nRefCount;

	if ( nRefCount == 0 )
		GSDOCache().OnSDOReleased( this );

	return nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: ref count
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
int CBaseSDO<KeyType,ESDOType,ProtoMsg>::GetRefCount()
{
	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: Hashes the object for insertion into a hashtable
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
uint32 CBaseSDO<KeyType,ESDOType,ProtoMsg>::GetHashCode() const
{
#pragma pack( push, 1 )
	struct hashcode_t
	{
		int m_Type;
		KeyType_t m_Key;
	} hashStruct = { ESDOType, m_Key };
#pragma pack( pop )

	return PearsonsHashFunctor<hashcode_t>()( hashStruct );
}


//-----------------------------------------------------------------------------
// Purpose: Deserializes the object
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::BReadFromBuffer( const byte *pubData, int cubData )
{
	ProtoMsg msg;
	if ( !msg.ParseFromArray( pubData, cubData ) )
		return false;

	if ( !DeserializeFromProtobuf( msg ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Serializes the object
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
void CBaseSDO<KeyType,ESDOType,ProtoMsg>::WriteToBuffer( CUtlBuffer &memBuffer )
{
	ProtoMsg *pMsg = CProtoBufMsg<ProtoMsg>::AllocProto();

	SerializeToProtobuf( *pMsg );
	uint32 unSize = pMsg->ByteSize();
	memBuffer.EnsureCapacity( memBuffer.Size() + unSize );
	pMsg->SerializeWithCachedSizesToArray( (uint8*)memBuffer.Base() + memBuffer.TellPut() );
	memBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, memBuffer.TellPut() + unSize );
	
	CProtoBufMsg<ProtoMsg>::FreeProto( pMsg );
}


//-----------------------------------------------------------------------------
// Purpose: does an immediate write of the object to memcached
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::WriteToMemcached()
{
	return GSDOCache().WriteSDOToMemcached( this );
}


//-----------------------------------------------------------------------------
// Purpose: does an immediate write of the object to memcached
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::DeleteFromMemcached()
{
	return GSDOCache().DeleteSDOFromMemcached( this );
}


//-----------------------------------------------------------------------------
// Purpose: default equality function - compares type and key
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::IsEqual( const ISDO *pSDO ) const
{
	if ( GetType() != pSDO->GetType() )
		return false;

	return ( GetKey() == static_cast<const CBaseSDO<KeyType,ESDOType,ProtoMsg> *>( pSDO )->GetKey() );
}


//-----------------------------------------------------------------------------
// Purpose: Batch load a group of SDO's of the same type from SQL. 
//  Default is no-op as not all types have permanent storage.
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::BYldLoadFromSQL( CUtlVector<ISDO *> &vecSDOToLoad, CUtlVector<bool> &vecResults ) const
{
	FOR_EACH_VEC( vecResults, i )
	{
		vecResults[i] = true;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: default validation function - compares serialized versions
//-----------------------------------------------------------------------------
bool CompareSDOObjects( ISDO *pSDO1, ISDO *pSDO2 );

template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::IsIdentical( ISDO *pSDO )
{
	return CompareSDOObjects( this, pSDO );
}


//-----------------------------------------------------------------------------
// Purpose: default validation function - compares serialized versions
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
ISDO *CBaseSDO<KeyType,ESDOType,ProtoMsg>::AllocNilObject()
{
	CFmtStr strKey;
	GetMemcachedKeyName( strKey );
	return new CNilSDO<KeyType,ESDOType,ProtoMsg>( GetKey(), strKey );
}


//-----------------------------------------------------------------------------
// Purpose: Finds a loaded SDO in memory. Returns the index of the object
//	into the loaded SDOs map
//-----------------------------------------------------------------------------
template<class T>
int CSDOCache::FindLoadedSDO( const typename T::KeyType_t &key )
{
	// see if we have it in cache first
	T probe( key );
	return m_mapISDOLoaded.Find( &probe );
}


//-----------------------------------------------------------------------------
// Purpose: Queues loading an SDO. Returns the index of the entry in the
//	load queue
//-----------------------------------------------------------------------------
template<class T>
int CSDOCache::QueueLoad( const typename T::KeyType_t &key )
{
	T probe( key );
	int iMap = m_mapQueuedRequests.Find( &probe );
	if ( m_mapQueuedRequests.IsValidIndex( iMap ) )
		return iMap;

	return QueueMemcachedLoad( new T( key ) );
}


//-----------------------------------------------------------------------------
// Purpose: Preloads the object into the local cache
//-----------------------------------------------------------------------------
template<class T>
void CSDOCache::HintLoadSDO( const typename T::KeyType_t &key )
{
	// see if this is something we should even try to load
	if ( !T::BKeyValid( key ) )
		return;

	// see if we have it in cache first
	if ( !m_mapISDOLoaded.IsValidIndex( FindLoadedSDO<T>( key ) ) )
	{
		QueueLoad<T>( key );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Preloads a set set of objects into the local cache
//-----------------------------------------------------------------------------
template<class T>
void CSDOCache::HintLoadSDO( const CUtlVector<typename T::KeyType_t> &vecKeys )
{
	FOR_EACH_VEC( vecKeys, i )
	{
		HintLoadSDO<T>( vecKeys[i] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns an already-loaded SDO
//-----------------------------------------------------------------------------
template<class T>
bool CSDOCache::BGetLoadedSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key, bool *pbFoundNil )
{
	if ( NULL != pbFoundNil )
	{
		*pbFoundNil = false;
	}

	int iMap = FindLoadedSDO<T>( key );
	if ( !m_mapISDOLoaded.IsValidIndex( iMap ) )
		return false;

	ISDO *pObj = m_mapISDOLoaded.Key( iMap );
	if ( pObj->BNilObject() )
	{
		int iLRU = m_mapISDOLoaded[ iMap ];
		Assert( m_listLRU.IsValidIndex( iLRU ) );
		if ( m_listLRU.IsValidIndex( iLRU ) )
		{
			// Even though we don't return nil objects, this is a hit on it
			// so it needs to go to the back of the LRU
			m_listLRU.LinkToTail( m_mapISDOLoaded[ iMap ] );
		}

		if ( NULL != pbFoundNil )
		{
			*pbFoundNil = true;
		}

		return false;
	}
	else
	{
		*pPSDORef = assert_cast<T*>( pObj );
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads the object into memory
//-----------------------------------------------------------------------------
template<class T>
bool CSDOCache::BYldLoadSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key, bool *pbTimeoutLoading /* = NULL */ )
{
	VPROF_BUDGET( "CSDOCache::BYldLoadSDO", VPROF_BUDGETGROUP_STEAM );
	if ( pbTimeoutLoading )
		*pbTimeoutLoading = false;

	// Clear the current object the ref is holding
	*pPSDORef = NULL;

	// see if this is something we should even try to load
	if ( !T::BKeyValid( key ) )
		return false;

	// see if we have it in cache first
	bool bFoundNil = false;
	if ( BGetLoadedSDO( pPSDORef, key, &bFoundNil ) )
		return true;

	// If we've already tried to look this up in the past don't bother looking it up again
	if ( bFoundNil )
		return false;

	// otherwise batch it for load
	int iMap = QueueLoad<T>( key );

	// make sure we could queue it
	if ( !m_mapQueuedRequests.IsValidIndex( iMap ) )
		return false;

	// add the current job to this list waiting for the object to load 
	m_mapQueuedRequests[iMap].AddToTail( GJobCur().GetJobID() );

	// wait for it to load (loader will signal our job when done)
	if ( !GJobCur().BYieldingWaitForWorkItem() )
	{
		if ( pbTimeoutLoading )
			*pbTimeoutLoading = true;
		return false;
	}

	// should be loaded - look up in the load map and try again
	bool bRet = BGetLoadedSDO( pPSDORef, key );
	//Assert( bRet );

	return bRet;
}


//-----------------------------------------------------------------------------
// Clears a nil object if one exists for this key.
//-----------------------------------------------------------------------------
template<class T>
void CSDOCache::RemoveNil( const typename T::KeyType_t &key )
{
	// See if this key is allowed to exist
	if ( !T::BKeyValid( key ) )
	{
		AssertMsg( false, "RemoveNil called with an invalid key" );
		return;
	}

	// Remove the nil if there is one
	int iMap = FindLoadedSDO< T >( key );
	if ( m_mapISDOLoaded.IsValidIndex( iMap ) )
	{
		ISDO *pSDO = m_mapISDOLoaded.Key( iMap );
		if ( !pSDO->BNilObject() )
			return;

		int iMapStats = m_mapTypeStats.Find( pSDO->GetType() );
		if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
		{
			m_mapTypeStats[iMapStats].m_nNilObjects--;
		}

		RemoveSDOFromLRU( iMap );
		m_mapISDOLoaded.RemoveAt( iMap );
		delete pSDO;
	}

	// Remove the object from memcached. Do this here because while we have for sure removed any nil that was in memory
	// this may have been a nil that was not in memory but instead cached in memcached.
	T temp( key );
	DeleteSDOFromMemcached( &temp );
}


//-----------------------------------------------------------------------------
// Purpose: reloads an existing element from the SQL DB
//-----------------------------------------------------------------------------
template<class T>
bool CSDOCache::BYldDeleteSDO( const typename T::KeyType_t &key, uint64 unMicrosecondsToWaitForUnreferenced )
{
	// see if we have it in cache first
	int iMap = FindLoadedSDO<T>( key );
	if ( !m_mapISDOLoaded.IsValidIndex( iMap ) )
	{
		T temp( key );
		temp.DeleteFromMemcached();
		return true; /* we're good, it's not loaded */
	}

	assert_cast<T *>(m_mapISDOLoaded.Key( iMap ))->DeleteFromMemcached();

	// check the ref count
	int64 cAttempts = MAX( 1LL, (int64)(unMicrosecondsToWaitForUnreferenced / k_cMicroSecPerShellFrame) );
	while ( cAttempts-- > 0 )
	{
		if ( 0 == m_mapISDOLoaded.Key( iMap )->GetRefCount() )
		{
			// delete the object
			Assert( m_listLRU.IsValidIndex( m_mapISDOLoaded[iMap] ) );

			RemoveSDOFromLRU( iMap );
			ISDO *pSDO = m_mapISDOLoaded.Key( iMap );
			m_mapISDOLoaded.RemoveAt( iMap );

			int iMapStats = m_mapTypeStats.Find( m_mapISDOLoaded.Key( iMap )->GetType() );
			if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
			{
				if ( pSDO->BNilObject() )
				{
					m_mapTypeStats[iMapStats].m_nNilObjects--;
				}
				else
				{
					m_mapTypeStats[iMapStats].m_nLoaded--;
				}
			}

			delete pSDO;
			return true;
		}
		else
		{
			GJobCur().BYieldingWaitOneFrame();
		}
	}
	
	// couldn't reload
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: A class to factor out the common code in most SDO SQL loading funcitons
//-----------------------------------------------------------------------------
template<class T>
class CSDOSQLLoadHelper
{
public:

	//this is the results of the load helper, abstracted to provide a more efficient means to lookup the queries without
	//having to do very expensive memory management
	template< class SCH >
	class CResults
	{
	public:

		//given a key, this will return the range of indices that include this key. -1 will be set to the start if no match is found. This will return
		//the number of matches found
		int			GetKeyIndexRange( typename T::KeyType_t Key, int& nStart, int& nEnd ) const;

		//given a key, this will return the first schema that matches this key, or NULL if none is found. Only use this if you know there
		//is a maximum of a single value
		const SCH*	GetSingleResultForKey( typename T::KeyType_t Key ) const;

		//used to start iteration over a group of results. Given a key, this will return the index that can be used to get the result or -1 if no match is found
		int			GetFirstResultIndex( typename T::KeyType_t Key ) const;
		//given an index that was previously obtained from GetFirst/NextResultIndex, this will get the next result that has the same key, or return -1 if no further keys of that type are found
		int			GetNextResultIndex( int nOldResultIndex ) const;
		//given an index from GetFirst/NextResultIndex that is valid (i.e. not -1), this will return the result that was obtained
		const SCH*	GetResultFromIndex( int nIndex ) const;
		//given an index returned by the GetFirst/NextResultIndex, this will determine if it is valid
		static int	InvalidIndex()		{ return -1; }

	private:
		//given a key, this maps to the specific result
		template< class TKeyType >
		struct SKeyToResult
		{
			bool operator< (const SKeyToResult< TKeyType >& rhs ) const			{ return m_Key < rhs.m_Key; }
			typename TKeyType	m_Key;				//key value
			uint32				m_nResultIndex;		//index into our result list
		};

		friend class CSDOSQLLoadHelper;

		CUtlVector< SKeyToResult< typename T::KeyType_t > >	m_KeyToResult;
		CUtlVector< SCH >							m_Results;
	};


	// Initializes with the vector of objects being loaded
	CSDOSQLLoadHelper( const CUtlVector<ISDO *> *vecSDOToLoad, const char *pchProfileName );

	// Loads all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	bool BYieldingExecuteSingleTable( int nFieldID, CResults< SCH >& Results );
	// Loads the specified columns for all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	bool BYieldingExecuteSingleTable( int nFieldID, const CColumnSet &csetRead, CResults< SCH >& Results );

	// Functions to load rows from more than one table at a time

	// Loads all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	void AddTableToQuery( int nFieldID );

	// Loads the specified columns for all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	void AddTableToQuery( int nFieldID, const CColumnSet &csetRead );

	// Executes the mutli-table query
	bool BYieldingExecute();
		
	// Gets the results for a table from a multi-table query
	template<class SCH>
	bool BGetResults( int nQuery, CResults< SCH >& Results );

private:
	CUtlVector<typename T::KeyType_t> m_vecKeys;
	CSQLAccess m_sqlAccess;

	struct Query_t
	{
		Query_t( const CColumnSet &columnSet, int nKeyCol ) : m_ColumnSet( columnSet ), m_nKeyCol( nKeyCol ) {}
		CColumnSet	m_ColumnSet;
		int			m_nKeyCol;
	};

	CUtlVector<Query_t> m_vecQueries;
};

//utility to help with iteration through a SQL load result list
#define FOR_EACH_SQL_LOAD_RESULT( Results, Key, Index )		for( int Index = Results.GetFirstResultIndex( Key ); Index != Results.InvalidIndex(); Index = Results.GetNextResultIndex( Index ) )


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes with the vector of objects being loaded
//-----------------------------------------------------------------------------
template<class T>
CSDOSQLLoadHelper<T>::CSDOSQLLoadHelper( const CUtlVector<ISDO *> *vecSDOToLoad, const char *pchProfileName )
	: m_vecKeys( 0, vecSDOToLoad->Count() )
{
	FOR_EACH_VEC( *vecSDOToLoad, i )
	{
		m_vecKeys.AddToTail( ( (T*)vecSDOToLoad->Element( i ) )->GetKey() );
	}

	Assert( m_vecKeys.Count() > 0 );
	DbgVerify( m_sqlAccess.BBeginTransaction( pchProfileName ) );
}


//-----------------------------------------------------------------------------
// Purpose: Loads all rows in the SCH table whose field nFieldID match the 
//	key of an SDO being loaded.
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
bool CSDOSQLLoadHelper<T>::BYieldingExecuteSingleTable( int nFieldID, CResults< SCH >& Results )
{
	static const CColumnSet cSetRead = CColumnSet::Full<SCH>();
	return BYieldingExecuteSingleTable( nFieldID, cSetRead, Results );
}


//-----------------------------------------------------------------------------
// Purpose: Loads the specified columns for all rows in the SCH table whose 
//	field nFieldID match the key of an SDO being loaded
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
bool CSDOSQLLoadHelper<T>::BYieldingExecuteSingleTable( int nFieldID, const CColumnSet &csetRead, CResults< SCH >& Results )
{
	AddTableToQuery<SCH>( nFieldID, csetRead );
	if ( !BYieldingExecute() )
		return false;

	return BGetResults<SCH>( 0, Results );
}


//-----------------------------------------------------------------------------
// Purpose: Loads all rows in the SCH table whose field nFieldID match the key 
//	of an SDO being loaded
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
void CSDOSQLLoadHelper<T>::AddTableToQuery( int nFieldID )
{
	static const CColumnSet cSetRead = CColumnSet::Full<SCH>();
	AddTableToQuery<SCH>( nFieldID, cSetRead );
}


//-----------------------------------------------------------------------------
// Purpose: Loads the specified columns for all rows in the SCH table whose 
//	field nFieldID match the key of an SDO being loaded
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
void CSDOSQLLoadHelper<T>::AddTableToQuery( int nFieldID, const CColumnSet &csetRead )
{
	Assert( csetRead.GetRecordInfo() == GSchemaFull().GetSchema( SCH::k_iTable ).GetRecordInfo() );

	// Bind the params
	FOR_EACH_VEC( m_vecKeys, i )
	{
		m_sqlAccess.AddBindParam( m_vecKeys[i] );
	}

	// Build the query
	CUtlString sCommand;
	{
		TSQLCmdStr sSelect;
		const char *pchColumnName = GSchemaFull().GetSchema( SCH::k_iTable ).GetRecordInfo()->GetColumnInfo( nFieldID ).GetName();

		BuildSelectStatementText( &sSelect, csetRead );
		sCommand.Format( "%s WHERE %s IN (%.*s)", sSelect.Access(), pchColumnName, GetInsertArgStringChars( m_vecKeys.Count() ), GetInsertArgString() );
	}

	// Execute. Because we're in a transaction this will delay to the commit
	DbgVerify( m_sqlAccess.BYieldingExecute( NULL, sCommand ) );

	m_vecQueries.AddToTail( Query_t( csetRead, nFieldID ) );
}


//-----------------------------------------------------------------------------
// Purpose: Executes the mutli-table query
//-----------------------------------------------------------------------------
template<class T>
bool CSDOSQLLoadHelper<T>::BYieldingExecute()
{
	if ( 0 == m_vecKeys.Count() )
	{
		m_sqlAccess.RollbackTransaction();
		return false;
	}

	if ( !m_sqlAccess.BCommitTransaction() )
		return false;

	Assert( (uint32)m_vecQueries.Count() == m_sqlAccess.GetResultSetCount() );
	return (uint32)m_vecQueries.Count() == m_sqlAccess.GetResultSetCount();
}


//-----------------------------------------------------------------------------
// Purpose: Gets the results for a table from a multi-table query
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
bool CSDOSQLLoadHelper<T>::BGetResults( int nQuery, CResults< SCH >& Results )
{
	//clear any previous results
	Results.m_KeyToResult.Purge();
	Results.m_Results.Purge();

	IGCSQLResultSetList *pSQLResults = m_sqlAccess.GetResults();
	Assert( pSQLResults && nQuery >= 0 && (uint32)nQuery < pSQLResults->GetResultSetCount() && pSQLResults->GetResultSetCount() == (uint32)m_vecQueries.Count() );
	if ( NULL == pSQLResults || nQuery < 0 || (uint32)nQuery >= pSQLResults->GetResultSetCount() || pSQLResults->GetResultSetCount() != (uint32)m_vecQueries.Count() )
		return false;

	Assert( m_vecQueries[nQuery].m_ColumnSet.GetRecordInfo()->GetTableID() == SCH::k_iTable );
	if ( m_vecQueries[nQuery].m_ColumnSet.GetRecordInfo()->GetTableID() != SCH::k_iTable )
		return false;

	//copy the results from the SQL queries over to our result list
	Results.m_Results.EnsureCapacity( pSQLResults->GetResultSet( nQuery )->GetRowCount() );
	if ( !CopyResultToSchVector( pSQLResults->GetResultSet( nQuery ), m_vecQueries[nQuery].m_ColumnSet, &Results.m_Results ) )
		return false;

	// Make a map that counts maps from our results into a sorted list for fast lookup
	Results.m_KeyToResult.SetSize( Results.m_Results.Count() );
	FOR_EACH_VEC( Results.m_Results, nCurrResult )
	{
		//get our key value
		uint8 *pubData;
		uint32 cubData;
		if ( !Results.m_Results[ nCurrResult ].BGetField( m_vecQueries[nQuery].m_nKeyCol, &pubData, &cubData ) )
			return false;

		Assert( cubData == sizeof( T::KeyType_t ) );
		if ( cubData != sizeof( T::KeyType_t ) )
		{
			Results.m_KeyToResult.Purge();
			Results.m_Results.Purge();
			return false;
		}

		//setup our record
		Results.m_KeyToResult[ nCurrResult ].m_Key = *((T::KeyType_t *)pubData);
		Results.m_KeyToResult[ nCurrResult ].m_nResultIndex = nCurrResult;
	}

	//sort for binary search capabilities
	std::sort( Results.m_KeyToResult.begin(), Results.m_KeyToResult.end() );

	return true;
}

template< typename T >
template< typename SCH >
int CSDOSQLLoadHelper< T >::CResults< SCH >::GetKeyIndexRange( typename T::KeyType_t Key, int& nStart, int& nEnd ) const
{
	//find the start of the range
	nStart = GetFirstResultIndex( Key );
	if( nStart == InvalidIndex() )
	{
		nEnd = InvalidIndex();
		return 0;
	}
	//expand the end as long as it lies on a key in range and with the same value
	for( nEnd = nStart + 1; ( nEnd < m_KeyToResult.Count() ) && ( m_KeyToResult[ nEnd ].m_Key == Key ); nEnd++ )
	{
	}
	//and return the resulting number of elements we found
	return nEnd - nStart;
}


template< typename T >
template< typename SCH >
const SCH* CSDOSQLLoadHelper< T >::CResults< SCH >::GetSingleResultForKey( typename T::KeyType_t Key ) const
{
	int nIndex = GetFirstResultIndex( Key );
	AssertMsg( ( nIndex == InvalidIndex() ) || ( GetNextResultIndex( nIndex ) == InvalidIndex() ), "Requested a result from a SQL load helper assuming it was a singular entry, but found multiple instances of it" );
	return GetResultFromIndex( nIndex );
}

template< typename T >
template< typename SCH >
int CSDOSQLLoadHelper< T >::CResults< SCH >::GetFirstResultIndex( typename T::KeyType_t Key ) const
{
	//dummy entry to compare against
	SKeyToResult< typename T::KeyType_t > SearchKey;
	SearchKey.m_Key = Key;	

	//binary search to find our index
	const SKeyToResult< typename T::KeyType_t >* pMatch = std::lower_bound( m_KeyToResult.begin(), m_KeyToResult.end(), SearchKey );
	//see if we found a match
	if( ( pMatch == m_KeyToResult.end() ) || ( pMatch->m_Key != Key ) )
		return InvalidIndex();

	return ( pMatch - m_KeyToResult.begin() );
}

template< typename T >
template< typename SCH >
int CSDOSQLLoadHelper< T >::CResults< SCH >::GetNextResultIndex( int nOldResultIndex ) const
{
	//handle out of range elements. Either invalid, or the last element or beyond in our array
	if( ( nOldResultIndex < 0 ) || ( nOldResultIndex + 1 >= m_KeyToResult.Count() ) )
		return InvalidIndex();

	//see if we are less than the next value in the list. If so, we aren't equal and need to stop iteration	
	if( m_KeyToResult[ nOldResultIndex ] < m_KeyToResult[ nOldResultIndex + 1 ] )
		return InvalidIndex();
	
	//same key for the next element, so return a match
	return nOldResultIndex + 1;
}

template< typename T >
template< typename SCH >
const SCH* CSDOSQLLoadHelper< T >::CResults< SCH >::GetResultFromIndex( int nIndex ) const
{
	//ensure that the index is in range
	if( ( nIndex < 0 ) || ( nIndex >= m_KeyToResult.Count() ) )
		return NULL;
	return &( m_Results[ m_KeyToResult[ nIndex ].m_nResultIndex ] );
}

} // namespace GCSDK

#endif // SDOCACHE_H
