//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Database Backed Object caching and manipulation
//
//=============================================================================
#include "stdafx.h"
#include "sdocache.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

GCConVar s_ConVarSDOCacheLRULimitMB( "@SDOCacheLRULimitMB", "400", FCVAR_REPLICATED );
GCConVar s_ConVarSDOCacheMaxMemcachedReadJobs( "@SDOCacheMaxMemcachedReadJobs", "4", FCVAR_REPLICATED );
GCConVar s_ConVarSDOCacheMaxMemcachedReadBatchSize( "@SDOCacheMaxMemcachedReadBatchSize", "100", FCVAR_REPLICATED );
GCConVar s_ConVarSDOCacheMaxSQLReadJobsPerSDOType( "@SDOCacheMaxSQLReadJobsPerSDOType", "4", FCVAR_REPLICATED );
GCConVar s_ConVarSDOCacheMaxSQLReadBatchSize( "@SDOCacheMaxSQLReadBatchSize", "100", FCVAR_REPLICATED );
GCConVar s_ConVarSDOCacheMaxPendingSQLReads( "@SDOCacheMaxPendingSQLReads", "2000", FCVAR_REPLICATED );
GCConVar s_ConVarSDOCacheMaxPendingMemcachedReads( "@SDOCacheMaxPendingMemcachedReads", "25000", FCVAR_REPLICATED );

namespace GCSDK
{
// A string used to tell the difference between nil objects and actual objects in memcached
const char k_rgchNilObjSerializedValue[] = "nilobj";


// Global instance
CSDOCache &GSDOCache()
{
	static CSDOCache s_SDOCache;
	return s_SDOCache;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a key name that looks like "Prefix_%u" but faster
//-----------------------------------------------------------------------------
void CSDOCache::CreateSimpleMemcachedName( CFmtStr &strDest, const char *pchPrefix, uint32 unPrefixLen, uint32 unSuffix )
{
	Assert( FMTSTR_STD_LEN - unPrefixLen > 10 + 1 );
	V_memcpy( strDest.Access(), pchPrefix, unPrefixLen );
	_i64toa( unSuffix, strDest.Access() + unPrefixLen, 10 );
	strDest.Access()[ FMTSTR_STD_LEN - 1 ] = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CSDOCache::CSDOCache() :
	m_cubLRUItems( 0 ),
	m_mapTypeStats( DefLessFunc( int ) )
{
	memset( &m_StatsSDOCache, 0, sizeof( m_StatsSDOCache ) );
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CSDOCache::~CSDOCache()
{
	// delete all the entries from our cache
	FOR_EACH_MAP_FAST( m_mapISDOLoaded, iMap )
	{
		delete m_mapISDOLoaded.Key( iMap );
	}

	FOR_EACH_MAP_FAST( m_mapQueuedRequests, iMap )
	{
		delete m_mapQueuedRequests.Key( iMap );
	}
}


//-----------------------------------------------------------------------------
// Purpose: registers an SDO type
//-----------------------------------------------------------------------------
void CSDOCache::RegisterSDO( int nType, const char *pchName )
{
	if ( !m_mapQueueSQLRequests.HasElement( nType ) )
	{
		m_mapQueueSQLRequests.Insert( nType, new SQLRequestManager_t );
		int iMap = m_mapTypeStats.Insert( nType );
		m_mapTypeStats[iMap].m_strName = pchName;
	}
}


//-----------------------------------------------------------------------------
// Purpose: a SDO object has been referenced
//-----------------------------------------------------------------------------
void CSDOCache::OnSDOReferenced( ISDO *pSDO )
{
	// lookup where the SDO is, it has where we are in the LRU
	int iMap = m_mapISDOLoaded.Find( pSDO );
	Assert( m_mapISDOLoaded.IsValidIndex( iMap ) );
	if ( !m_mapISDOLoaded.IsValidIndex( iMap ) )
		return;

	// move us out of the LRU
	if ( m_listLRU.IsValidIndex( m_mapISDOLoaded[iMap] ) )
	{
		RemoveSDOFromLRU( iMap );
	}
	else
	{
		// we may not have been in the LRU if it's a custom insert
	}

	// Update stats
	int iMapStats = m_mapTypeStats.Find( pSDO->GetType() );
	if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
	{
		m_mapTypeStats[iMapStats].m_nRefed++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: a SDO object has gone released
//-----------------------------------------------------------------------------
void CSDOCache::OnSDOReleased( ISDO *pSDO )
{
	// Update stats
	int iMapStats = m_mapTypeStats.Find( pSDO->GetType() );
	if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
	{
		m_mapTypeStats[iMapStats].m_nRefed--;
	}

	// lookup where the SDO is, it has where we are in the LRU
	int iMap = m_mapISDOLoaded.Find( pSDO );
	if ( m_mapISDOLoaded.IsValidIndex( iMap ) )
	{
		// we shouldn't be in the LRU
		bool bInLRU = m_listLRU.IsValidIndex( m_mapISDOLoaded[iMap] );
		Assert( !bInLRU );
		if ( bInLRU )
			return;

		// count the bytes and move us to the head of the LRU
		ISDO *pSDO = m_mapISDOLoaded.Key( iMap );
		LRUItem_t item = { pSDO, pSDO->CubBytesUsed() };
		m_mapISDOLoaded[iMap] = m_listLRU.AddToTail( item );
		m_cubLRUItems += item.m_cub;

		if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
		{
			m_mapTypeStats[iMapStats].m_cubUnrefed += item.m_cub;
		}
	}
	else
	{
		// it's actually valid it's not in the SDO cache anymore - could be an orphaned pointer
		if ( pSDO->GetRefCount() == 0 )
		{
			delete pSDO;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: pulls a cached SDO from the right maps
//-----------------------------------------------------------------------------
void CSDOCache::RemoveSDOFromLRU( int iMap )
{
	int iLRU = m_mapISDOLoaded[iMap];

	Assert( m_listLRU.IsValidIndex( iLRU ) );
	if ( !m_listLRU.IsValidIndex( iLRU ) )
		return;

	int iMapStats = m_mapTypeStats.Find( m_listLRU[iLRU].m_pSDO->GetType() );
	if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
	{
		m_mapTypeStats[iMapStats].m_cubUnrefed -= m_listLRU[iLRU].m_cub;
	}

	m_cubLRUItems -= m_listLRU[iLRU].m_cub;
	m_listLRU.Remove( iLRU );
	m_mapISDOLoaded[iMap] = m_listLRU.InvalidIndex();

	// Update stats
}


//-----------------------------------------------------------------------------
// Purpose: writes a SDO to memcached
//-----------------------------------------------------------------------------
bool CSDOCache::WriteSDOToMemcached( ISDO *pSDO )
{
	CFmtStr strKey;
	CUtlBuffer buf;

	pSDO->GetMemcachedKeyName( strKey );
	pSDO->WriteToBuffer( buf );
	return GGCBase()->BMemcachedSet( strKey.Access(), buf );
}


//-----------------------------------------------------------------------------
// Purpose: writes a SDO to memcached
//-----------------------------------------------------------------------------
bool CSDOCache::DeleteSDOFromMemcached( ISDO *pSDO )
{
	CFmtStr strKey;
	pSDO->GetMemcachedKeyName( strKey );
	return GGCBase()->BMemcachedDelete( strKey.Access() );
}


//-----------------------------------------------------------------------------
// Purpose: runs the queued batch loader
//-----------------------------------------------------------------------------
class CGCJobLoadSDOSetFromMemcached : public CGCJob
{
public:
	CGCJobLoadSDOSetFromMemcached( CGCBase *pGC ) 
		: CGCJob( pGC )
	{
	}

	void AddSDOToLoad( ISDO *pSDO, int iRequestID )
	{
		int i = m_vecRequests.AddToTail();
		m_vecRequests[i].m_pSDO = pSDO;
		m_vecRequests[i].m_iRequestID = iRequestID;
	}

	bool BYieldingRunJob( void * )
	{
		Assert( m_vecRequests.Count() > 0 );
		if ( 0 == m_vecRequests.Count() )
		{
			GSDOCache().OnMemcachedLoadJobComplete( GetJobID() );
			return false;
		}

		// get the names of all the items
		CUtlVector<CUtlString> vecKeys;
		vecKeys.AddMultipleToTail( m_vecRequests.Count() );
		FOR_EACH_VEC( m_vecRequests, i )
		{
			CFmtStr strKey;
			m_vecRequests[i].m_pSDO->GetMemcachedKeyName( strKey );
			vecKeys[i] = strKey;
		}

		// ask in a batch from memcached
		CUtlVector<CGCBase::GCMemcachedGetResult_t> vecGetResults;
		bool bGetSuccess = m_pGC->BYieldingMemcachedGet( vecKeys, vecGetResults );

		// go through each request looking up the results
		FOR_EACH_VEC( m_vecRequests, i )
		{
			if ( bGetSuccess && vecGetResults.IsValidIndex( i ) && vecGetResults[i].m_bKeyFound )
			{
				const CGCBase::GCMemcachedGetResult_t &result = vecGetResults[i];
				bool bNilObj = ( result.m_bufValue.Count() == sizeof( k_rgchNilObjSerializedValue ) 
								 && 0 == V_memcmp( result.m_bufValue.Base(), k_rgchNilObjSerializedValue, sizeof( k_rgchNilObjSerializedValue ) ) ); 
	
				// we've loaded OK
				if ( bNilObj || ( result.m_bufValue.Count() && m_vecRequests[i].m_pSDO->BReadFromBuffer( result.m_bufValue.Base(), result.m_bufValue.Count() ) ) )
				{
					GSDOCache().OnSDOLoadSuccess( m_vecRequests[i].m_pSDO->GetType(), m_vecRequests[i].m_iRequestID, bNilObj, &m_vecRequests[i].m_pSDO );
					GSDOCache().GetStats().m_cItemsLoadedFromMemcached += 1;
					GSDOCache().GetStats().m_cNilItemsLoadedFromMemcached += bNilObj ? 1 : 0;
				}
				else
				{
					// couldn't load; delete the entry
					m_pGC->BMemcachedDelete( vecKeys[i] );
					GSDOCache().OnMemcachedSDOLoadFailure( m_vecRequests[i].m_pSDO->GetType(), m_vecRequests[i].m_iRequestID );
				}
			}
			else
			{
				// post back failure
				GSDOCache().OnMemcachedSDOLoadFailure( m_vecRequests[i].m_pSDO->GetType(), m_vecRequests[i].m_iRequestID );
			}
		}

		GSDOCache().OnMemcachedLoadJobComplete( GetJobID() );
		return true;
	}

private:
	struct Request_t
	{
		int m_iRequestID;
		ISDO *m_pSDO;
	};

	CUtlVector<Request_t> m_vecRequests;
};


//-----------------------------------------------------------------------------
// Purpose: runs the queued batch loader
//-----------------------------------------------------------------------------
class CGCJobLoadSDOSetFromSQL : public CGCJob
{
public:
	CGCJobLoadSDOSetFromSQL( CGCBase *pGC, int eSDOType ) 
		: CGCJob( pGC ), m_eSDOType( eSDOType )
	{
	}

	void AddSDOToLoad( ISDO *pSDO, int iRequestID )
	{
		m_vecPSDO.AddToTail( pSDO );
		m_vecRequestID.AddToTail( iRequestID );
		m_vecResults.AddToTail( false );
	}

	bool BYieldingRunJob( void * )
	{
		Assert( m_vecPSDO.Count() == m_vecRequestID.Count() && m_vecPSDO.Count() == m_vecResults.Count() );
		Assert( m_vecPSDO.Count() > 0 );
		if ( 0 == m_vecPSDO.Count()
			 || m_vecPSDO.Count() != m_vecRequestID.Count()
			 || m_vecPSDO.Count() != m_vecResults.Count() )
		{
			GSDOCache().OnSQLLoadJobComplete( m_eSDOType, GetJobID() );
			return false;
		}

		// use the first item to load the rest
		bool bSQLLayerSucceeded = m_vecPSDO[0]->BYldLoadFromSQL( m_vecPSDO, m_vecResults );

		Assert( m_vecPSDO.Count() == m_vecRequestID.Count() && m_vecPSDO.Count() == m_vecResults.Count() );
		Assert( m_vecPSDO.Count() > 0 );
		if ( 0 == m_vecPSDO.Count()
			|| m_vecPSDO.Count() != m_vecRequestID.Count()
			|| m_vecPSDO.Count() != m_vecResults.Count() )
		{
			GSDOCache().OnSQLLoadJobComplete( m_eSDOType, GetJobID() );
			return false;
		}

		// walk each result
		FOR_EACH_VEC( m_vecRequestID, i )
		{
			if ( bSQLLayerSucceeded )
			{
				// loaded, great
				GSDOCache().OnSDOLoadSuccess( m_eSDOType, m_vecRequestID[i], !m_vecResults[i], &m_vecPSDO[i] );
				GSDOCache().WriteSDOToMemcached( m_vecPSDO[i] );

				GSDOCache().GetStats().m_cItemsLoadedFromSQL += 1;
				GSDOCache().GetStats().m_cNilItemsLoadedFromSQL += m_vecResults[i] ? 0 : 1;
			}
			else
			{
				// no good, item couldn't load
				GSDOCache().OnSQLSDOLoadFailure( m_eSDOType, m_vecRequestID[i], bSQLLayerSucceeded );
				GSDOCache().GetStats().m_cItemsFailedLoadFromSQL += 1;
			}
		}

		GSDOCache().OnSQLLoadJobComplete( m_eSDOType, GetJobID() );
		return true;
	}

private:
	int m_eSDOType;

	// these objects all stay in sync
	// they need to be separate vectors so that they can be easily passed into other API's
	CUtlVector<ISDO *> m_vecPSDO;
	CUtlVector<int> m_vecRequestID;
	CUtlVector<bool> m_vecResults;
};


//-----------------------------------------------------------------------------
// Purpose: continues any jobs that were waiting on items
//-----------------------------------------------------------------------------
bool CSDOCache::BFrameFuncRunJobsUntilCompleted( CLimitTimer &limitTimer )
{
	bool bDoneWork = false;

	// continue any jobs
	while ( m_queueJobsToContinue.Count() && !limitTimer.BLimitReached() )
	{
		// pop the item off the head
		JobToWake_t jobToWake = m_queueJobsToContinue[ m_queueJobsToContinue.Head() ];
		m_queueJobsToContinue.Remove( m_queueJobsToContinue.Head() );
		GGCBase()->GetJobMgr().BRouteWorkItemCompletedIfExists( jobToWake.m_jobID, !jobToWake.m_bLoadLayerSuccess );
	}

	// if we're over the limit, LRU an item
	while ( !limitTimer.BLimitReached() && m_cubLRUItems > (uint32)( s_ConVarSDOCacheLRULimitMB.GetInt() * k_nMegabyte ) && m_listLRU.Count() )
	{
		// pull off the last item in the LRU
		LRUItem_t item = m_listLRU[ m_listLRU.Head() ];

		// kill the item
		int iMap = m_mapISDOLoaded.Find( item.m_pSDO );
		Assert( m_mapISDOLoaded.IsValidIndex( iMap ) );
		if ( m_mapISDOLoaded.IsValidIndex( iMap ) )
		{
			Assert( 0 == item.m_pSDO->GetRefCount() );
			if ( 0 == item.m_pSDO->GetRefCount() )
			{
				RemoveSDOFromLRU( iMap );

				int iMapStats = m_mapTypeStats.Find( item.m_pSDO->GetType() );
				if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
				{
					if ( item.m_pSDO->BNilObject() )
					{
						m_mapTypeStats[iMapStats].m_nNilObjects--;
					}
					else
					{
						m_mapTypeStats[iMapStats].m_nLoaded--;
					}
				}

				m_mapISDOLoaded.RemoveAt( iMap );
				delete item.m_pSDO;
			}

			m_StatsSDOCache.m_cItemsLRUd += 1;
			m_StatsSDOCache.m_cBytesLRUd += item.m_cub;
		}

		bDoneWork = true;
	}

	// return true if there is still work remaining
	return bDoneWork || m_queueJobsToContinue.Count() > 0;
}


//-----------------------------------------------------------------------------
// Purpose: runs the queued batch loader
//-----------------------------------------------------------------------------
bool CSDOCache::BFrameFuncRunMemcachedQueriesUntilCompleted( CLimitTimer &limitTimer )
{
	bool bDoneWork = false;

	// batch load the set
	if ( m_queueMemcachedRequests.Count() )
	{
		// pass these off to a job
		if ( m_vecMemcachedJobs.Count() < s_ConVarSDOCacheMaxMemcachedReadJobs.GetInt() )
		{
			CGCJobLoadSDOSetFromMemcached *pJob = new CGCJobLoadSDOSetFromMemcached( GGCBase() );

			// add a full batch to the job
			int cItemsInBatch = 0;
			while ( m_queueMemcachedRequests.Count() && cItemsInBatch < s_ConVarSDOCacheMaxMemcachedReadBatchSize.GetInt() )
			{
				m_StatsSDOCache.m_cQueuedMemcachedRequests--;
				int iMapQueuedRequest = m_queueMemcachedRequests[ m_queueMemcachedRequests.Head() ];
				m_queueMemcachedRequests.Remove( m_queueMemcachedRequests.Head() );
				Assert( m_mapQueuedRequests.IsValidIndex( iMapQueuedRequest ) );
				if ( m_mapQueuedRequests.IsValidIndex( iMapQueuedRequest ) )
				{
					 cItemsInBatch++;
					 pJob->AddSDOToLoad( m_mapQueuedRequests.Key( iMapQueuedRequest ), iMapQueuedRequest );
				}
			}

			if ( cItemsInBatch > 0 )
			{
				m_StatMemcachedBatchSize.AddSample( cItemsInBatch * 100 );
				m_StatsSDOCache.m_nMemcachedBatchSizeAvgx100 = (int)m_StatMemcachedBatchSize.GetAveragedSample();

				// add to the list
				m_vecMemcachedJobs.AddToTail( pJob->GetJobID() );

				// start the job
				pJob->StartJob( NULL );

				// mark that we should be ran again
				bDoneWork = true;
			}
			else
			{
				delete pJob;
			}
		}
	}

	// return if we still have work to do
	return bDoneWork;
}


//-----------------------------------------------------------------------------
// Purpose: runs the queued batch loader
//-----------------------------------------------------------------------------
bool CSDOCache::BFrameFuncRunSQLQueriesUntilCompleted( CLimitTimer &limitTimer )
{
	bool bDoneWork = false;

	// loop through all items looking for batches to load
	FOR_EACH_MAP_FAST( m_mapQueueSQLRequests, iMapType )
	{
		// batch load the set
		SQLRequestManager_t *sqlRequestManager = m_mapQueueSQLRequests[iMapType];
		if ( sqlRequestManager->m_queueRequestIDsToLoadFromSQL.Count() )
		{
			// pass these off to a job
			if ( sqlRequestManager->m_vecSQLJobs.Count() < s_ConVarSDOCacheMaxSQLReadJobsPerSDOType.GetInt() )
			{
				int nSDOType = m_mapQueueSQLRequests.Key( iMapType );
				CGCJobLoadSDOSetFromSQL *pJob = new CGCJobLoadSDOSetFromSQL( GGCBase(), nSDOType );

				// add a full batch to the job
				int cItemsInBatch = 0;
				while ( sqlRequestManager->m_queueRequestIDsToLoadFromSQL.Count() && cItemsInBatch < s_ConVarSDOCacheMaxSQLReadBatchSize.GetInt() )
				{
					m_StatsSDOCache.m_cQueuedSQLRequests--;
					int iMapQueuedRequest = sqlRequestManager->m_queueRequestIDsToLoadFromSQL[ sqlRequestManager->m_queueRequestIDsToLoadFromSQL.Head() ];
					sqlRequestManager->m_queueRequestIDsToLoadFromSQL.Remove( sqlRequestManager->m_queueRequestIDsToLoadFromSQL.Head() );
					Assert( m_mapQueuedRequests.IsValidIndex( iMapQueuedRequest ) );
					if ( m_mapQueuedRequests.IsValidIndex( iMapQueuedRequest ) )
					{
						pJob->AddSDOToLoad( m_mapQueuedRequests.Key( iMapQueuedRequest ), iMapQueuedRequest );
						cItemsInBatch++;
					}
				}

				if ( cItemsInBatch > 0 )
				{
					m_StatSQLBatchSize.AddSample( cItemsInBatch * 100 );
					m_StatsSDOCache.m_nSQLBatchSizeAvgx100 = (int)m_StatSQLBatchSize.GetAveragedSample();

					// add to the list
					sqlRequestManager->m_vecSQLJobs.AddToTail( pJob->GetJobID() );

					// start the job
					pJob->StartJob( NULL );
					bDoneWork = true;
				}
				else
				{
					delete pJob;
				}
			}
		}
	}

	// update stats
	m_StatsSDOCache.m_cItemsInCache = m_mapISDOLoaded.Count();
	m_StatsSDOCache.m_cItemsQueuedToLoad = m_mapQueuedRequests.Count();
	m_StatsSDOCache.m_cBytesUnreferenced = m_cubLRUItems;
	m_StatsSDOCache.m_cItemsUnreferenced = m_listLRU.Count();

	if ( m_StatsSDOCache.m_cItemsUnreferenced )
	{
		// estimate the total bytes from the size of the average size of an unreferenced item
		m_StatsSDOCache.m_cBytesInCacheEst = (m_StatsSDOCache.m_cItemsInCache * m_StatsSDOCache.m_cBytesUnreferenced) / m_StatsSDOCache.m_cItemsUnreferenced;
	}

	// return true if we still have work to do
	return bDoneWork;
}


//-----------------------------------------------------------------------------
// Purpose: queues a new item to try load from memcached
//-----------------------------------------------------------------------------
int CSDOCache::QueueMemcachedLoad( ISDO *pSDO )
{
	int iMap = -1;
	if ( m_queueMemcachedRequests.Count() < s_ConVarSDOCacheMaxPendingMemcachedReads.GetInt() )
	{
		// insert a fresh item into the list
		iMap = m_mapQueuedRequests.Insert( pSDO );

		// add the key to the queue
		m_queueMemcachedRequests.AddToTail( iMap );
		m_StatsSDOCache.m_cQueuedMemcachedRequests++;
	}
	else
	{
		m_StatsSDOCache.m_cMemcachedRequestsRejectedTooBusy++;
		delete pSDO;
	}

	return iMap;
}


//-----------------------------------------------------------------------------
// Purpose: job results
//-----------------------------------------------------------------------------
void CSDOCache::OnSDOLoadSuccess( int eSDOType, int iRequestID, bool bNilObj, ISDO **ppSDO )
{
	Assert( m_mapQueuedRequests.IsValidIndex( iRequestID ) );
	if ( !m_mapQueuedRequests.IsValidIndex( iRequestID ) )
		return;

	// set jobs waiting for the data to wake up
	CCopyableUtlVector<JobID_t> &vecJobs = m_mapQueuedRequests[iRequestID];
	FOR_EACH_VEC( vecJobs, i )
	{
		JobToWake_t jobToWake = { vecJobs[i], true /* success */ };
		m_queueJobsToContinue.AddToTail( jobToWake );
	}

	// move from requested to loaded
	ISDO *pSDO = m_mapQueuedRequests.Key( iRequestID );
	m_mapQueuedRequests.RemoveAt( iRequestID );

	// If the query succeeded but the object doesn't exist then we need to cache that
	if ( bNilObj )
	{
		ISDO *pInvalidSDO = pSDO->AllocNilObject();
		delete pSDO;
		pSDO = pInvalidSDO;
		*ppSDO = pSDO;
	}
	else
	{
		// do any extra initialization on the SDO
		pSDO->PostLoadInit();
	}

	Assert( !m_mapISDOLoaded.HasElement( pSDO ) );
	int iMap = m_mapISDOLoaded.Insert( pSDO, m_listLRU.InvalidIndex() );

	// update stats
	int iMapStats = m_mapTypeStats.Find( pSDO->GetType() );
	if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
	{
		if ( pSDO->BNilObject() )
		{
			m_mapTypeStats[iMapStats].m_nNilObjects++;
		}
		else
		{
			m_mapTypeStats[iMapStats].m_nLoaded++;
		}
	}

	// put us in the LRU - if it's just a hint, we may not be referenced immediately
	Assert( m_mapISDOLoaded.IsValidIndex( iMap ) );
	if ( m_mapISDOLoaded.IsValidIndex( iMap ) )
	{
		LRUItem_t item = { pSDO, pSDO->CubBytesUsed() };
		m_mapISDOLoaded[iMap] = m_listLRU.AddToTail( item );
		m_cubLRUItems += item.m_cub;

		if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
		{
			m_mapTypeStats[iMapStats].m_cubUnrefed += item.m_cub;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: job results
//-----------------------------------------------------------------------------
void CSDOCache::OnMemcachedSDOLoadFailure( int eSDOType, int iRequestID )
{
	// we've failed to load an item from memcached - mark as needing load from SQL
	Assert( m_mapQueuedRequests.IsValidIndex( iRequestID ) );
	if ( !m_mapQueuedRequests.IsValidIndex( iRequestID ) )
		return;

	Assert( eSDOType == m_mapQueuedRequests.Key( iRequestID )->GetType() );
	if ( eSDOType != m_mapQueuedRequests.Key( iRequestID )->GetType() )
		return;

	int iSQLIndex = m_mapQueueSQLRequests.Find( eSDOType );
	AssertMsg1( m_mapQueueSQLRequests.IsValidIndex( iSQLIndex ), "Error, could not load SDO from SQL for unregistered type %d", eSDOType );
	if ( !m_mapQueueSQLRequests.IsValidIndex( iSQLIndex ) )
		return;

	SQLRequestManager_t *sqlRequestManager = m_mapQueueSQLRequests[iSQLIndex];

	if ( sqlRequestManager->m_queueRequestIDsToLoadFromSQL.Count() < s_ConVarSDOCacheMaxPendingSQLReads.GetInt() )
	{
		sqlRequestManager->m_queueRequestIDsToLoadFromSQL.AddToTail( iRequestID );
		m_StatsSDOCache.m_cQueuedSQLRequests++;
	}
	else
	{
		// too many outstanding items, reject and fail immediately
		m_StatsSDOCache.m_cSQLRequestsRejectedTooBusy++;
		OnSQLSDOLoadFailure( eSDOType, iRequestID, false /* loader failure */ );
	}
}


//-----------------------------------------------------------------------------
// Purpose: job results
//-----------------------------------------------------------------------------
void CSDOCache::OnSQLSDOLoadFailure( int eSDOType, int iRequestID, bool bSQLLayerSucceeded )
{
	Assert( m_mapQueuedRequests.IsValidIndex( iRequestID ) );
	if ( !m_mapQueuedRequests.IsValidIndex( iRequestID ) )
		return;

	// failed to load from SQL
	// set jobs waiting for the data to wake up
	CCopyableUtlVector<JobID_t> &vecJobs = m_mapQueuedRequests[iRequestID];
	FOR_EACH_VEC( vecJobs, i )
	{
		JobToWake_t jobToWake = { vecJobs[i], bSQLLayerSucceeded };
		m_queueJobsToContinue.AddToTail( jobToWake );
	}

	// kill the object - no one should have references to it, since it's only in the request list
	ISDO *pSDO = m_mapQueuedRequests.Key( iRequestID );
	m_mapQueuedRequests.RemoveAt( iRequestID );

	Assert( 0 == pSDO->GetRefCount() );
	if ( 0 == pSDO->GetRefCount() )
	{
		delete pSDO;
	}
}


//-----------------------------------------------------------------------------
// Purpose: job results
//-----------------------------------------------------------------------------
void CSDOCache::OnMemcachedLoadJobComplete( JobID_t jobID )
{
	m_vecMemcachedJobs.FindAndRemove( jobID );
}


//-----------------------------------------------------------------------------
// Purpose: job results
//-----------------------------------------------------------------------------
void CSDOCache::OnSQLLoadJobComplete( int eSDOType, JobID_t jobID )
{
	int iSQLIndex = m_mapQueueSQLRequests.Find( eSDOType );
	AssertMsg1( m_mapQueueSQLRequests.IsValidIndex( iSQLIndex ), "Error, could not load SDO from SQL for unregistered type %d", eSDOType );
	if ( !m_mapQueueSQLRequests.IsValidIndex( iSQLIndex ) )
		return;

	SQLRequestManager_t *sqlRequestManager = m_mapQueueSQLRequests[iSQLIndex];
	sqlRequestManager->m_vecSQLJobs.FindAndRemove( jobID );
}


//-----------------------------------------------------------------------------
// Purpose: deletes all unreferenced objects
//-----------------------------------------------------------------------------
void CSDOCache::Flush()
{
	int cReferencedObjects = 0;
	FOR_EACH_MAP_FAST( m_mapISDOLoaded, iMap )
	{
		if ( m_mapISDOLoaded.Key( iMap )->GetRefCount() == 0 )
		{
			int iMapStats = m_mapTypeStats.Find( m_mapISDOLoaded.Key( iMap )->GetType() );
			if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
			{
				if ( m_mapISDOLoaded.Key( iMap )->BNilObject() )
				{
					m_mapTypeStats[iMapStats].m_nNilObjects--;
				}
				else
				{
					m_mapTypeStats[iMapStats].m_nLoaded--;
				}
			}

			RemoveSDOFromLRU( iMap );
			DeleteSDOFromMemcached( m_mapISDOLoaded.Key( iMap ) );
			delete m_mapISDOLoaded.Key( iMap );
			m_mapISDOLoaded.RemoveAt( iMap );
		}
		else
		{
			cReferencedObjects++;
		}
	}

	Assert( cReferencedObjects == 0 );
	Assert( m_cubLRUItems == 0 );
}


//-----------------------------------------------------------------------------
// Purpose: returns the number of bytes we estimate we have referenced
//-----------------------------------------------------------------------------
int CSDOCache::CubReferencedEst()
{
	return MAX( 0, (int)m_StatsSDOCache.m_cBytesInCacheEst - (int)m_StatsSDOCache.m_cBytesUnreferenced );
}


//-----------------------------------------------------------------------------
// Purpose: Prints information about the cache
//-----------------------------------------------------------------------------
void CSDOCache::Dump()
{
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "SDO cache:\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Items Cached:                %llu\n", m_StatsSDOCache.m_cItemsInCache );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Estimated Bytes Cached:      %s\n", V_pretifymem( m_StatsSDOCache.m_cBytesInCacheEst, 2, true ) );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Loads Queued:                %llu\n", m_StatsSDOCache.m_cItemsQueuedToLoad );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Memcached Loads Queued:      %llu\n", m_StatsSDOCache.m_cQueuedMemcachedRequests );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    SQL Loads Queued:            %llu\n", m_StatsSDOCache.m_cQueuedSQLRequests );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Items Unreferenced:          %llu\n", m_StatsSDOCache.m_cItemsUnreferenced );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Bytes Unreferenced:          %s\n", V_pretifymem( m_StatsSDOCache.m_cBytesUnreferenced, 2, true ) );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Items LRU'd:                 %llu\n", m_StatsSDOCache.m_cItemsLRUd );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Bytes LRU'd:                 %s\n", V_pretifymem( m_StatsSDOCache.m_cBytesLRUd, 2, true ) );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Items Loaded From Memcached: %llu\n", m_StatsSDOCache.m_cItemsLoadedFromMemcached );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Items Loaded From SQL:       %llu\n", m_StatsSDOCache.m_cItemsLoadedFromSQL );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Nils Loaded From Memcached:  %llu\n", m_StatsSDOCache.m_cNilItemsLoadedFromMemcached );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Nils Loaded From SQL:        %llu\n", m_StatsSDOCache.m_cNilItemsLoadedFromSQL );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Average Memcached Batch:     %f\n", (float)m_StatsSDOCache.m_nMemcachedBatchSizeAvgx100 / 100.0f );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Average SQL Batch:           %f\n", (float)m_StatsSDOCache.m_nSQLBatchSizeAvgx100 / 100.0f );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Memcached Requests Rejected: %llu\n", m_StatsSDOCache.m_cMemcachedRequestsRejectedTooBusy );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    SQL Requests Rejected:       %llu\n", m_StatsSDOCache.m_cSQLRequestsRejectedTooBusy );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "    Failed SQL Loads:            %llu\n", m_StatsSDOCache.m_cItemsFailedLoadFromSQL );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Per-type stats\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, 
		"%-35s  --- loaded --- referenced --- nil objects --- size (est)\n", "name" );

	FOR_EACH_MAP_FAST( m_mapTypeStats, i )
	{
		int nLoaded = m_mapTypeStats[i].m_nLoaded + m_mapTypeStats[i].m_nNilObjects;
		char *pchRefed = "unknown";
		if ( m_mapTypeStats[i].m_nRefed < nLoaded && m_mapTypeStats[i].m_cubUnrefed > 0 )
		{
			pchRefed = V_pretifymem( ( ((int64)nLoaded * (int64)m_mapTypeStats[i].m_cubUnrefed) / ( nLoaded - m_mapTypeStats[i].m_nRefed ) ), 2, true );
		}
		
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%-35s %11d %14d %15d %14s\n", m_mapTypeStats[i].m_strName.String(), m_mapTypeStats[i].m_nLoaded, m_mapTypeStats[i].m_nRefed, m_mapTypeStats[i].m_nNilObjects, pchRefed );
	}
}


//**tempcomment** - This is good code. We'll hook it back up later
//
//static ConVar s_ConVarVerifyMemcacheDeletesBadEntries( "@VerifyMemcacheDeletesBadEntries", "1", FCVAR_REPLICATED );
//
////-----------------------------------------------------------------------------
//// Purpose: verifies a set of memcached data against what's in SQL
////-----------------------------------------------------------------------------
//void CSDOCache::YldVerifyMemcachedData( CreateSDOFunc_t pCreateSDOFunc, CUtlVector<uint32> &vecIDs, int *pcMatches, int *pcMismatches )
//{
//	// create the objects
//	CUtlVector<ISDO *> vecPSDOSQL;
//	CUtlVector<bool> vecSQLResults;
//	FOR_EACH_VEC( vecIDs, i )
//	{
//		vecPSDOSQL.AddToTail( pCreateSDOFunc( *this, vecIDs[i] ) );
//		vecSQLResults.AddToTail( false );
//	}
//
//	CUtlVector<CUtlString> vecSKeysToDelete;
//
//	// load them in a batch
//	bool bSQLLayerSucceeded = vecPSDOSQL[0]->BYldLoadFromSQL( vecIDs, vecPSDOSQL, vecSQLResults );
//	if ( bSQLLayerSucceeded )
//	{
//		// retrieve the memcache data
//		CUtlVector<CUtlString> vecSKeys;
//		FOR_EACH_VEC( vecPSDOSQL, i )
//		{
//			int iKey = vecSKeys.AddToTail();
//			vecPSDOSQL[i]->GetMemcachedKeyName( vecSKeys[iKey] );
//		}
//		CMemcachedAccess memcachedAccess( m_pServer->GetPMemcachedMgr() );
//		CUtlVector<MemcachedGetResult_t> vecMemcacheResults;
//		if ( memcachedAccess.BYldGetMulti( vecSKeys, vecMemcacheResults ) )
//		{
//			Assert( vecMemcacheResults.Count() == vecSQLResults.Count() );
//
//			FOR_EACH_VEC( vecSQLResults, i )
//			{
//				bool bClearKey = false;
//				if ( vecSQLResults[i] && vecMemcacheResults[i].m_bKeyFound )
//				{
//					// compare the results
//					ISDO *pSDOCached = pCreateSDOFunc( *this, vecIDs[i] );
//					const CUtlAllocation &allocMemcache = vecMemcacheResults[i].m_bufValue;
//					if ( pSDOCached->BReadFromBuffer( allocMemcache.Base(), allocMemcache.Count() ) )
//					{
//						if ( pSDOCached->IsEqual( vecPSDOSQL[i] ) )
//						{
//							// cool
//							*pcMatches += 1;
//						}
//						else
//						{
//							// boo
//							*pcMismatches += 1;
//
//							// print the differing bytes
//							/*
//							Msg( "key %s differs:\n", vecSKeys[i] );
//							for ( int n = 0; n < allocMemcache.Count(); n++ )
//							{
//								const byte *p1 = (byte*)bufSQL.Base() + n;
//								const byte *p2 = (byte*)bufCached.Base() + n;
//								if ( *p1 != *p2 )
//								{
//									Msg( "  %3d: %2x %2x\n", n, *p1, *p2 );
//								}
//							}
//							*/
//
//							if ( s_ConVarVerifyMemcacheDeletesBadEntries.GetBool() )
//								bClearKey = true;
//						}
//					}
//					else
//					{
//						// data failed to parse, clear
//						bClearKey = true;
//					}
//					delete pSDOCached;
//
//					if ( bClearKey )
//						vecSKeysToDelete.AddToTail( vecSKeys[i] );
//				}
//			}
//		}
//	}
//	else
//	{
//		// SQL failure, ignore
//	}
//
//	// clear any suspect keys
//	if ( vecSKeysToDelete.Count() )
//	{
//		CMemcachedAccess memcachedAccess( m_pServer->GetPMemcachedMgr() );
//		memcachedAccess.BAsyncDeleteMulti( vecSKeysToDelete );
//	}
//	
//	// delete all the SDO objects
//	FOR_EACH_VEC( vecPSDOSQL, i )
//	{
//		delete vecPSDOSQL[i];
//	}
//}
//
//</**tempcomment**> - This is good code. We'll hook it back up later


//-----------------------------------------------------------------------------
// Purpose: default comparison function - compares serialized versions
//-----------------------------------------------------------------------------
bool CompareSDOObjects( ISDO *pSDO1, ISDO *pSDO2 )
{
	CUtlBuffer b1, b2;
	pSDO1->WriteToBuffer( b1 );
	pSDO2->WriteToBuffer( b2 );

	return ( b1.TellPut() == b2.TellPut() && 0 == Q_memcmp( b1.Base(), b2.Base(), b1.TellPut() ) );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validates memory
//-----------------------------------------------------------------------------
void CSDOCache::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	for ( int i = k_ESDOTypeInvalid+1; i < k_ESDOTypeMax; i++ )
	{
		SDOSet_t &SDOSet = m_rgSDOSet[i];
		ValidateObj( SDOSet.m_mapISDOLoaded );
		ValidateObj( SDOSet.m_mapQueuedRequests );
		ValidateObj( SDOSet.m_queueRequestIDsToLoadFromSQL );
		ValidateObj( SDOSet.m_vecSQLJobs );

		FOR_EACH_MAP_FAST( SDOSet.m_mapISDOLoaded, iMap )
		{
			ValidatePtr( SDOSet.m_mapISDOLoaded[iMap].m_pSDO );
		}

		FOR_EACH_MAP_FAST( SDOSet.m_mapQueuedRequests, iMap )
		{
			ValidatePtr( SDOSet.m_mapQueuedRequests[iMap].m_pSDO );
			ValidateObj( SDOSet.m_mapQueuedRequests[iMap].m_vecJobsWaiting );
		}
	}

	ValidateObj( m_queueMemcachedRequests );
	ValidateObj( m_vecMemcachedJobs );
	ValidateObj( m_queueJobsToContinue );
	ValidateObj( m_listLRU );
	ValidateObj( m_StatMemcachedBatchSize );
	ValidateObj( m_StatSQLBatchSize );
}
#endif // DBGFLAG_VALIDATE

} // namespace GCSDK
