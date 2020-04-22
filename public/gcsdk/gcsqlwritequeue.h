//====== Copyright ©, Valve Corporation, All rights reserved. =======
// GCSqlWriteQueue.h
//
// A utility class that allows for templating based upon a SQL schema type, and then
// queuing up a number of those records to be written to SQL. This will buffer them until
// either a certain number have been queued or a period of time has elapsed, at which point
// it will flush them to SQL in a single transaction.
//
//===================================================================
#ifndef GCSQLWRITEQUEUE_H
#define GCSQLWRITEQUEUE_H

#include "scheduledfunction.h"

namespace GCSDK
{

class CGCBase;

template < typename TSqlClass >
class CGCSQLWriteQueue
{
public:

	CGCSQLWriteQueue( uint32 nMaxToCache, uint32 nMaxMSToWrite ) :
		m_nMaxToCache( nMaxToCache ),
		m_nMaxMSToWrite( nMaxMSToWrite )
	{
		m_QueuedRecords.EnsureCapacity( nMaxToCache );
	}
	
	//called to queue the record for writing, which will occur either when the maximum time between writebacks has occurred, or
	//when the cache has filled up
	void QueueRecord( const TSqlClass& sch )
	{
		m_QueuedRecords.AddToTail( sch );

		//now handle either dispatching, or scheduling a timeout dispatch
		if( ( uint32 )m_QueuedRecords.Count() >= m_nMaxToCache )
		{
			//unschedule first. This way if while we are yielded, we add another entry, it can schedule a new timeout
			m_TimeCommit.Cancel();
			CreateJobToCommitSQL();
		}
		else if( !m_TimeCommit.BIsScheduled() )
		{
			m_TimeCommit.Schedule( this, &CGCSQLWriteQueue< TSqlClass >::CreateJobToCommitSQL, m_nMaxMSToWrite );			
		}
	}

	//a yielding version of the above function
	void YieldingQueueRecord( const TSqlClass& sch )
	{
		m_QueuedRecords.AddToTail( sch );

		//now handle either dispatching, or scheduling a timeout dispatch
		if( ( uint32 )m_QueuedRecords.Count() >= m_nMaxToCache )
		{
			//unschedule first. This way if while we are yielded, we add another entry, it can schedule a new timeout
			m_TimeCommit.Cancel();
			YieldingFlushQueuedViewsToSQL();
		}
		else if( !m_TimeCommit.BIsScheduled() )
		{
			m_TimeCommit.Schedule( this, &CGCSQLWriteQueue< TSqlClass >::CreateJobToCommitSQL, m_nMaxMSToWrite );			
		}
	}
	
private:

	//called internally when we kick off a job after N amount of time has expired
	template < typename TSqlClass >
	class CTimeExpiredCommitJob : public CGCJob
	{
	public:
		CTimeExpiredCommitJob( CGCBase* pGC, CGCSQLWriteQueue< TSqlClass >* pClass ) : CGCJob( pGC ), m_pClass( pClass )	{}
		virtual bool BYieldingRunJob( void* pvStartParm )
		{
			m_pClass->YieldingFlushQueuedViewsToSQL();
			return true;
		}
	private:
		CGCSQLWriteQueue< TSqlClass >*	m_pClass;
	};

	//the function called when time expires to start a job and commit the requests to SQL
	void CreateJobToCommitSQL()
	{
		//kick off our job, which just calls the flush
		CGCJob* pJob = new CTimeExpiredCommitJob< TSqlClass >( GGCBase(), this );
		pJob->StartJobDelayed( NULL );
	}

	//handles committing the list of queued views to SQL
	void YieldingFlushQueuedViewsToSQL()
	{
		if( m_QueuedRecords.Count() == 0 )
			return;

		//move the contents into a local vector so we don't have any conflicts of global state
		CUtlVector< TSqlClass > localQueue;
		localQueue.Swap( m_QueuedRecords );
		//prepare the queue for the next batch (so we don't have intermediate resizes)
		m_QueuedRecords.EnsureCapacity( m_nMaxToCache );

		// start a transaction for all this work
		CSQLAccess sqlAccess;
		sqlAccess.BBeginTransaction( "CGCWatchDownloadedReplayJob::FlushQueuedViewsToSQL" );

		FOR_EACH_VEC( localQueue, nCurrView )
		{
			sqlAccess.BYieldingInsertRecord( &localQueue[ nCurrView ] );
		}

		sqlAccess.BCommitTransaction();
	}

	//the records that we have queued
	CUtlVector< TSqlClass >									m_QueuedRecords;
	//schedules a write back to ensure we commit at least every N seconds
	CScheduledFunction< CGCSQLWriteQueue< TSqlClass > >		m_TimeCommit;
	//maximum number of seconds between commits
	uint32													m_nMaxMSToWrite;
	//maximum number of records to buffer before writing back
	uint32													m_nMaxToCache;
};

} //namespace GCSDK

#endif

