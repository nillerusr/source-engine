//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
#ifdef DEBUG_JOB_LIST
CUtlLinkedList<CJob *,int> CJobMgr::sm_listAllJobs;
#endif

typedef int (__cdecl *QSortCompareFuncCtx_t)(void *, const void *, const void *);


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CJobMgr::CJobMgr()
:	m_MapJob( 0, 0, DefLessFunc( GID_t ) ), 
	m_QueueJobSleeping( 0, 0, &JobSleepingLessFunc ),
	m_unNextJobID( 0 ),
	m_mapStatsBucket( 0, 0, DefLessFunc(uint32) ),
	m_WorkThreadPool( "CJobMgr::m_WorkThreadPool" ),
	m_bDebugDisallowPause( false )
{
	SetDefLessFunc( m_MapJobTimeoutsIndexByJobID );
	SetDefLessFunc( m_mapOrphanMessages );
    m_bJobTimedOut = false;
	m_nCurrentYieldIterationRegPri = 0;
	m_bProfiling = false;
	m_bIsShuttingDown = false;
	m_cErrorsToReport = 0;
	m_unFrameFuncThreadID = 0;
	m_WorkThreadPool.SetWorkThreadAutoConstruct( 1, NULL );
	
	if( MemAlloc_GetDebugInfoSize() > 0 )
	{
		g_memMainDebugInfo.Init( 0, MemAlloc_GetDebugInfoSize() );
	}

	if( MemAlloc_GetDebugInfoSize() > 0 )
	{
		g_memMainDebugInfo.EnsureCapacity( MemAlloc_GetDebugInfoSize() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CJobMgr::~CJobMgr()
{
	m_WorkThreadPool.StopWorkThreads();
}


//-----------------------------------------------------------------------------
// Purpose: limit the size of our thread pool
//-----------------------------------------------------------------------------
void CJobMgr::SetThreadPoolSize( uint cThreads )
{
	m_WorkThreadPool.SetWorkThreadAutoConstruct( cThreads, NULL );
}


//-----------------------------------------------------------------------------
// Purpose: gets the next available job ID
//-----------------------------------------------------------------------------
JobID_t CJobMgr::GetNewJobID()
{
#ifdef GC
	return GGCHost()->GenerateGID();
#else
	return ++m_unNextJobID;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Run jobs
//			Runs once per frame and resumes any sleeping jobs that are scheduled
//			to run again, also checks for jobs which have timed out.
//
// Input:   limitTimer - limit timer not to exceed
// Output:  true if there is still work remaining to do, false otherwise
//-----------------------------------------------------------------------------
bool CJobMgr::BFrameFuncRunSleepingJobs( CLimitTimer &limitTimer )
{
	CheckThreadID(); // make sure frame function is called from correct thread

	bool bWorkRemaining = false;

	{
		VPROF_BUDGET( "CJobMgr::BResumeSleepingJobs", VPROF_BUDGETGROUP_JOBS_COROUTINES );
		bWorkRemaining |= BResumeSleepingJobs( limitTimer );
	}

	{
		VPROF_BUDGET( "CJobMgr::CheckForJobTimeouts", VPROF_BUDGETGROUP_JOBS_COROUTINES );
		CheckForJobTimeouts( limitTimer );
	}

	m_JobStats.m_cJobsCurrent = CountJobs();

	return bWorkRemaining;
}



//-----------------------------------------------------------------------------
// Purpose: Run jobs
//			This function is called repeatedly in a single frame if time is left
//			and will first run any yielding jobs
// Input:   limitTimer - limit timer not to exceed
// Output:  true if there is still work remaining to do, false otherwise
//-----------------------------------------------------------------------------
bool CJobMgr::BFrameFuncRunYieldingJobs( CLimitTimer &limitTimer )
{
	CheckThreadID(); // make sure frame function is called from correct thread

	bool bWorkRemaining = false;

	{
		VPROF_BUDGET( "CJobMgr::BResumeYieldingJobs", VPROF_BUDGETGROUP_JOBS_COROUTINES );
		bWorkRemaining |= BResumeYieldingJobs( limitTimer );
	}

	{
		VPROF_BUDGET( "CJobMgr -- Dispatch completed work items", VPROF_BUDGETGROUP_JOBS_COROUTINES );
		bWorkRemaining |= m_WorkThreadPool.BDispatchCompletedWorkItems( limitTimer, this );
	}

	m_JobStats.m_cJobsCurrent = CountJobs();

	return bWorkRemaining;
}


//-----------------------------------------------------------------------------
// Purpose: Registers a new job for us to keep track of.
// Input:	job -			The job in question
//-----------------------------------------------------------------------------
void CJobMgr::InsertJob( CJob &job )
{
	Assert( m_MapJob.Find( job.GetJobID() ) == m_MapJob.InvalidIndex() );
	m_MapJob.Insert( job.GetJobID(), &job );
#ifdef DEBUG_JOB_LIST
	sm_listAllJobs.AddToTail( &job );
#endif
}


//-----------------------------------------------------------------------------
// purpose: This job is done, accumulate its stats
//-----------------------------------------------------------------------------
void CJobMgr::AccumulateStatsofJob( CJob &job )
{
	// if we are not profiling, but the job experienced some kind of failure
	// record it anyway - we will issue a consolidated spew about it
	if ( !m_bProfiling && job.m_flags.m_uFlags == 0 )
		return;
	if ( job.m_flags.m_uFlags )
		m_cErrorsToReport++;

	job.m_FastTimerDelta.End();
	job.m_cyclecountTotal += job.m_FastTimerDelta.GetDuration();

	uint32 eBucket = 0;
	// the pointer to the name is a pointer to a constant string
	// so use this dirty trick to make lookups fast
	eBucket = (uint32)job.GetName();
	int iBucket = m_mapStatsBucket.Find( eBucket );
	if ( iBucket == m_mapStatsBucket.InvalidIndex() )
	{
		iBucket = m_mapStatsBucket.Insert( eBucket );
		V_strcpy_safe( m_mapStatsBucket[iBucket].m_rgchName, job.GetName() );
	}

	JobStatsBucket_t *pJobStatsBucket = &m_mapStatsBucket[iBucket];
	pJobStatsBucket->m_cCompletes++;
	pJobStatsBucket->m_cLocksAttempted += job.m_cLocksAttempted;
	pJobStatsBucket->m_cLocksWaitedFor += job.m_cLocksWaitedFor;
	pJobStatsBucket->m_cLocksFailed += job.m_flags.m_bits.m_bLocksFailed ? 1 : 0;
	pJobStatsBucket->m_cLocksLongHeld += job.m_flags.m_bits.m_bLocksLongHeld ? 1 : 0;
	pJobStatsBucket->m_cLocksLongWait += job.m_flags.m_bits.m_bLocksLongWait ? 1 : 0;
	pJobStatsBucket->m_cWaitTimeout += job.m_flags.m_bits.m_bWaitTimeout ? 1 : 0;
	pJobStatsBucket->m_cJobsFailed += job.m_flags.m_bits.m_bJobFailed ? 1 : 0;
	pJobStatsBucket->m_cLongInterYieldTime += job.m_flags.m_bits.m_bLongInterYield ? 1 : 0;
	pJobStatsBucket->m_cTimeoutNetMsg += job.m_flags.m_bits.m_bTimeoutNetMsg ? 1 : 0;

	pJobStatsBucket->m_u64RunTime += job.m_cyclecountTotal.GetLongCycles();
	if ( (uint64)job.m_cyclecountTotal.GetLongCycles() > pJobStatsBucket->m_u64RunTimeMax )
		pJobStatsBucket->m_u64RunTimeMax = job.m_cyclecountTotal.GetLongCycles();
	if ( job.m_STimeSwitched != job.m_STimeStarted )
	{
		pJobStatsBucket->m_cJobsPaused++;
		pJobStatsBucket->m_u64JobDuration += job.m_STimeStarted.CServerMicroSecsPassed();
	}
	else
	{
		pJobStatsBucket->m_u64JobDuration += job.m_cyclecountTotal.GetMicroseconds();
	}
}


//-----------------------------------------------------------------------------
// purpose: This message was orphaned, accumulate for stats
//-----------------------------------------------------------------------------
void CJobMgr::RecordOrphanedMessage( MsgType_t eMsg, JobID_t jobIDTarget )
{
	EG_MSG( SPEW_JOB, "Message %s arrived responding to job %lld which no longer exists, dropping message\n", PchMsgNameFromEMsg( eMsg ), jobIDTarget );
	int iBucket = m_mapOrphanMessages.Find( eMsg );
	if ( iBucket == m_mapOrphanMessages.InvalidIndex() )
	{
		int ct = 0;
		iBucket = m_mapOrphanMessages.Insert( eMsg, ct );
	}
	m_mapOrphanMessages[iBucket]++;
}


//-----------------------------------------------------------------------------
// Purpose: Removes a job from the manager.  Note that we don't free it.
// Input:	job -			The job in question
//-----------------------------------------------------------------------------
void CJobMgr::RemoveJob( CJob &job )
{
	m_MapJob.Remove( job.GetJobID() );

	AccumulateStatsofJob( job );
	m_JobStats.m_cJobsTotal++;
	if ( job.m_flags.m_bits.m_bJobFailed )
		m_JobStats.m_cJobsFailed++;

	uint64 u64JobDuration = job.m_STimeStarted.CServerMicroSecsPassed();
	m_JobStats.m_flSumJobTimeMicrosec += u64JobDuration;
	m_JobStats.m_flSumSqJobTimeMicrosec += ((double)u64JobDuration * (double)u64JobDuration);
	if ( u64JobDuration > m_JobStats.m_unMaxJobTimeMicrosec )
	{
		m_JobStats.m_unMaxJobTimeMicrosec = u64JobDuration;
	}

#ifdef DEBUG_JOB_LIST
	sm_listAllJobs.FindAndRemove( &job );
#endif
}


#ifdef GC
//-----------------------------------------------------------------------------
// Purpose: resumes the specified job if it is, in fact, waiting for a SQL query 
//			to return
//-----------------------------------------------------------------------------
bool CJobMgr::BResumeSQLJob( JobID_t jobID )
{
	int iMap = m_mapSQLQueriesInFlight.Find( jobID );
	if ( m_mapSQLQueriesInFlight.IsValidIndex( iMap ) )
	{
		if ( m_bSQLProfiling && m_dictSQLBuckets.IsValidIndex( m_mapSQLQueriesInFlight[iMap].m_iBucket ) )
		{
			SQLProfileBucket_t &bucket = m_dictSQLBuckets[ m_mapSQLQueriesInFlight[iMap].m_iBucket ];
			bucket.m_unCount++;
			bucket.m_nTotalMicrosec += (int64)m_sqlTimer.GetDurationInProgress().GetUlMicroseconds() - m_mapSQLQueriesInFlight[iMap].m_nStartMicrosec;
		}

		m_mapSQLQueriesInFlight.RemoveAt( iMap );
	}

	int iJob;
	if ( !BGetIJob( jobID, k_EJobPauseReasonSQL, true, &iJob ) )
	{
		EG_MSG( SPEW_JOB, "BResumeSQLJob called for a job that could not be found!\n" );
		return false;
	}

	// Just change the job's pause reason and add it to the yield list
	// it will wake up on the next heartbeat
	m_MapJob[iJob]->EndPause( k_EJobPauseReasonSQL );
	AddToYieldList( *m_MapJob[iJob] );

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: returns true if we're running any jobs of the specified name
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJobMgr::BIsJobRunning( const char *pchJobName )
{
	FOR_EACH_MAP_FAST( m_MapJob, i )
	{
		if ( !Q_stricmp( m_MapJob[i]->GetName(), pchJobName ) )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if there is a job active with the specified ID
//-----------------------------------------------------------------------------
bool CJobMgr::BJobExists( JobID_t jobID ) const
{
	return ( m_MapJob.Find( jobID ) != m_MapJob.InvalidIndex() );
}


//-----------------------------------------------------------------------------
// Purpose: returns a job pointer by id
//-----------------------------------------------------------------------------
const CJob *CJobMgr::GetPJob( JobID_t jobID ) const
{
	int iMap = m_MapJob.Find( jobID );
	if ( iMap != m_MapJob.InvalidIndex() )
	{
		return m_MapJob[iMap];
	}
	return NULL;
}

CJob *CJobMgr::GetPJob( JobID_t jobID )
{
	int iMap = m_MapJob.Find( jobID );
	if ( iMap != m_MapJob.InvalidIndex() )
	{
		return m_MapJob[iMap];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Examines an incoming message to see if it belongs to an active job,
//			and if so, sends it to that job.  Creates a new job if necessary.
// Output:	true if the message was routed to a job
//-----------------------------------------------------------------------------
bool CJobMgr::BRouteMsgToJob( void *pParent, IMsgNetPacket *pNetPacket, const JobMsgInfo_t &jobMsgInfo )
{
	if ( pNetPacket == NULL )
	{
		AssertMsg(pNetPacket, "CJobMgr::BRouteMsgToJob received NULL packet.");
		return false;
	}

	if ( jobMsgInfo.m_JobIDTarget != k_GIDNil )
	{
		// This message is a reply to a running job
		VPROF_BUDGET( "CJobMgr::BRouteMsgToJob() - continue job", VPROF_BUDGETGROUP_JOBS_COROUTINES );

		// Find the job that this packet is destined for
		int iJob = m_MapJob.Find( jobMsgInfo.m_JobIDTarget );
		if ( m_MapJob.InvalidIndex() != iJob )
		{
			// found the right job, pass it off
			PassMsgToJob( *(m_MapJob[iJob]), pNetPacket, jobMsgInfo );
			return true;
		}

		// The job is no longer running, it most likely timed out before the response arrived.
		// Continue and see if a job is registered to launch from this message
	}

	// no job, so try creating a job that can handle the msg
	// We pass in a pointer to m_JobIDTarget so that it gets set to the new Job's ID. This ensures
	// that anyone replying to this message from within the new job has the right JobIDSource.
	VPROF_BUDGET( "CJobMgr::BRouteMsgToJob() - job", VPROF_BUDGETGROUP_JOBS_COROUTINES );
	bool bRet = BLaunchJobFromNetworkMsg( pParent, jobMsgInfo, pNetPacket );

	if ( !bRet && jobMsgInfo.m_JobIDTarget != k_GIDNil )
	{
		RecordOrphanedMessage( jobMsgInfo.m_eMsg, jobMsgInfo.m_JobIDTarget );
		// return that we've handled this message (as much as it possibly can be) -- was intended for a job that has
		// timed out, no one else can do anything with it
		return true;
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Routes a message directly to the specified job
//-----------------------------------------------------------------------------
void CJobMgr::PassMsgToJob( CJob &job, IMsgNetPacket *pNetPacket, const JobMsgInfo_t &jobMsgInfo  )
{
	// Check if this job previously failed to wait for this message type,
	// then this is probably a late reply.  Discard it
	if ( job.BHasFailedToReceivedMsgType( jobMsgInfo.m_eMsg ) )
	{
		EmitInfo( SPEW_JOB, 2, LOG_ALWAYS, "Reply msg type %s to job %s is too late; discarding\n", PchMsgNameFromEMsg( jobMsgInfo.m_eMsg ), job.GetName() );
		return;
	}

	// make sure it's what we're waiting for
	if ( job.GetPauseReason() != k_EJobPauseReasonNetworkMsg )
	{
		AssertMsg3( false, "CJobMgr::PassMsgToJob() job %s received unexpected message %s when paused for %s\n", job.GetName(), PchMsgNameFromEMsg( jobMsgInfo.m_eMsg ), job.GetPauseReasonDescription() );
	}

	// In case of error, we need to throw this message away
	if ( job.GetPauseReason() != k_EJobPauseReasonNetworkMsg )
		return;

	// Add the packet and resume the job
	job.AddPacketToList( pNetPacket, jobMsgInfo.m_JobIDSource );
	job.EndPause( k_EJobPauseReasonNetworkMsg );
	AddToYieldList( job );

	return;
}


//-----------------------------------------------------------------------------
// Purpose: pauses the job until a network msg for the specified job arrives
//-----------------------------------------------------------------------------
bool CJobMgr::BYieldingWaitForMsg( CJob &job )
{
	// wait until we're woken up by a networking callback, or a timeout
	PauseJob( job, k_EJobPauseReasonNetworkMsg );
	return !m_bJobTimedOut;
}


//-----------------------------------------------------------------------------
// Purpose: Returns IJob matching a JobID, if it is paused for the given reason
// Input:	jobID -						The job that should be paused for the given reason
//			eJobPauseReason -			Pause reason
//			bShouldExist -				If true, job should exist, so asserts on not finding it ok
//			pIJob -						IJob to fill in
// Output:	true if job paused for matching reason found
//-----------------------------------------------------------------------------
bool CJobMgr::BGetIJob( JobID_t jobID, EJobPauseReason eJobPauseReason, bool bShouldExist, int *pIJob )
{
	// If this isn't owned by a job, we don't handle it
	if ( k_GIDNil == jobID )
		return false;

	// Figure out which job the msg belongs to
	int iJob = m_MapJob.Find( jobID );
	Assert( m_MapJob.InvalidIndex() != iJob || !bShouldExist );

	// If it's not one of ours, ignore it
	if ( m_MapJob.InvalidIndex() == iJob )
		return false;

	// make sure it's what we're waiting for
	if ( m_MapJob[iJob]->GetPauseReason() != eJobPauseReason )
		return false;

	*pIJob = iJob;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: yields for a set amount of time
// Input  : &job - job that is yielding
//			m_cMicrosecondsToSleep - number of microseconds to wait for before resuming job
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJobMgr::BYieldingWaitTime( CJob &job, uint32 cMicrosecondsToSleep )
{
	Assert( cMicrosecondsToSleep < k_cMicroSecJobPausedTimeout );
	// sleep of zero causes an infinite loop
	Assert( 0 != cMicrosecondsToSleep );

#ifdef _DEBUG
	for ( int i = 0; i < m_QueueJobSleeping.Count(); i++ )
	{
		Assert( m_QueueJobSleeping.Element(i).m_JobID != job.GetJobID() );
	}
#endif

	// insert the job into the sleep list
	JobSleeping_t jobSleeping;
	jobSleeping.m_JobID = job.GetJobID();
	jobSleeping.m_SWakeupTime.SetFromJobTime( cMicrosecondsToSleep );
	jobSleeping.m_STimeTouched.SetToJobTime();
	m_QueueJobSleeping.Insert( jobSleeping );

	// yield
	PauseJob( job, k_EJobPauseReasonSleepForTime );
	if ( m_bJobTimedOut )
		return false;

	return true;
}


#ifdef GC
//-----------------------------------------------------------------------------
// Purpose: yields waiting for a query response
// Input  : &job - job that is yielding
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
// yields waiting for a query response
bool CJobMgr::BYieldingRunQuery( CJob &job, CGCSQLQueryGroup *pQueryGroup, ESchemaCatalog eSchemaCatalog )
{
	// clear the existing results pointer, if any, to make space for the results
	// for this query
	pQueryGroup->SetResults( NULL );

	if ( m_bSQLProfiling )
	{
		const char *pchName = pQueryGroup->PchName();
		if ( !pchName || !pchName[0] )
		{
			if ( pQueryGroup->GetStatementCount() == 1 )
			{
				pchName = pQueryGroup->PchCommand( 0 );
			}

			if ( !pchName || !pchName[0] )
			{
				pchName = job.GetName();
			}
		}

		PendingSQLJob_t sqlJob;
		sqlJob.m_nStartMicrosec = (int64)m_sqlTimer.GetDurationInProgress().GetUlMicroseconds();
		sqlJob.m_iBucket = m_dictSQLBuckets.Find( pchName );
		if ( !m_dictSQLBuckets.IsValidIndex( sqlJob.m_iBucket ) )
		{
			SQLProfileBucket_t bucket = { 0, 0 };
			sqlJob.m_iBucket = m_dictSQLBuckets.Insert( pchName, bucket );
		}
		m_mapSQLQueriesInFlight.Insert( job.GetJobID(), sqlJob );
	}

	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - SQLQuery", VPROF_BUDGETGROUP_STEAM );
		GGCHost()->SQLQuery( job.GetJobID(), pQueryGroup, eSchemaCatalog );
	}
	PauseJob( job, k_EJobPauseReasonSQL );
	return pQueryGroup->GetResults() && pQueryGroup->GetResults()->GetError() == k_EGCSQLErrorNone;
}


//-----------------------------------------------------------------------------
// Purpose: turns on sql profiling
//-----------------------------------------------------------------------------
void CJobMgr::StartSQLProfiling()
{
	if ( m_bSQLProfiling )
		return;

	m_mapSQLQueriesInFlight.RemoveAll();
	m_dictSQLBuckets.RemoveAll();
	m_sqlTimer.Start();
	m_bSQLProfiling = true;
}


//-----------------------------------------------------------------------------
// Purpose: turns off sql profiling
//-----------------------------------------------------------------------------
void CJobMgr::StopSQLProfiling()
{
	if ( !m_bSQLProfiling )
		return;

	m_mapSQLQueriesInFlight.RemoveAll();
	m_sqlTimer.End();
	m_bSQLProfiling = false;
}



//-----------------------------------------------------------------------------
// Purpose: sql profile sort func
//-----------------------------------------------------------------------------
int CJobMgr::SQLProfileSortFunc( void *pCtx, const int *lhs, const int *rhs )
{
	SQLProfileCtx_t *pSQLProfileCtx = (SQLProfileCtx_t *)pCtx;
	CUtlDict<SQLProfileBucket_t> *pDictBuckets = pSQLProfileCtx->pdictBuckets;
	SQLProfileBucket_t &lhsBucket = pDictBuckets->Element( *lhs );
	SQLProfileBucket_t &rhsBucket = pDictBuckets->Element( *rhs );

	switch ( pSQLProfileCtx->m_eSort )
	{
	default:
	case k_ESQLProfileSortTotalTime:	return rhsBucket.m_nTotalMicrosec - lhsBucket.m_nTotalMicrosec;
	case k_ESQLProfileSortTotalCount:	return rhsBucket.m_unCount - lhsBucket.m_unCount;
	case k_ESQLProfileSortAvgTime:		return ( rhsBucket.m_nTotalMicrosec / rhsBucket.m_unCount ) - ( lhsBucket.m_nTotalMicrosec / lhsBucket.m_unCount );
	case k_ESQLProfileSortName:			return Q_stricmp( pDictBuckets->GetElementName( *lhs ), pDictBuckets->GetElementName( *rhs ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: dumps the current sql profile
//-----------------------------------------------------------------------------
void CJobMgr::DumpSQLProfile( ESQLProfileSort eSort )
{
	CUtlVector<int> vecSort;
	for ( int iDict = 0; iDict < m_dictSQLBuckets.MaxElement(); iDict++ )
	{
		if ( !m_dictSQLBuckets.IsValidIndex( iDict ) )
			continue;

		if ( m_dictSQLBuckets[iDict].m_unCount > 0 )
		{
			vecSort.AddToTail( iDict );
		}
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "SQL statement stats:\n" );
	if ( 0 == vecSort.Count() )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tNo SQL stats collected; use sql_profile_on / sql_profile_off to collect stats first\n" );
		return;
	}

	// sort
	SQLProfileCtx_t ctx;
	ctx.m_eSort = eSort;
	ctx.pdictBuckets = &m_dictSQLBuckets;

	V_qsort_s( vecSort.Base(), vecSort.Count(), sizeof(int), (QSortCompareFuncCtx_t)SQLProfileSortFunc, &ctx );

	// display
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%8s %8s   %8s\n", "count", "time", "avg" );
	FOR_EACH_VEC( vecSort, i )
	{
		SQLProfileBucket_t &bucket = m_dictSQLBuckets[ vecSort[i] ];
		const char *pchStatement = m_dictSQLBuckets.GetElementName( vecSort[i] );

		// cleanup the statement text
		char rgchCleaned[140];
		V_strcpy_safe( rgchCleaned, pchStatement );
		for ( int i = 0; NULL != rgchCleaned[i]; i++ )
		{
			if ( '\n' == rgchCleaned[i] || '\t' == rgchCleaned[i] )
			{
				rgchCleaned[i] = ' ';
			}
		}

		bool bSeconds = bucket.m_nTotalMicrosec > k_nMillion;
		float fTime = bucket.m_nTotalMicrosec / 1000.0f / ( bSeconds ? 1000.0f : 1.0f );

		// render
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%8d %8.2f%s %8.2f  %s\n", 
			bucket.m_unCount, 
			fTime,
			bSeconds ? "s " : "ms",
			(float)bucket.m_nTotalMicrosec / (float)bucket.m_unCount / 1000.0f,
			rgchCleaned );
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: pauses job until a work item completes
//-----------------------------------------------------------------------------
bool CJobMgr::BYieldingWaitForWorkItem( CJob &job, const char *pszWorkItemName )
{
	// wait until we're woken up by a work item completed, or a timeout
	PauseJob( job, k_EJobPauseReasonWorkItem );
	if ( m_bJobTimedOut || job.m_bWorkItemCanceled )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: adds a job work item to the thread pool
//-----------------------------------------------------------------------------
void CJobMgr::AddThreadedJobWorkItem( CWorkItem *pWorkItem )
{
	m_WorkThreadPool.AddWorkItem( pWorkItem );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if we're still working
//-----------------------------------------------------------------------------
bool CJobMgr::HasOutstandingThreadPoolWorkItems()
{
	return m_WorkThreadPool.HasWorkItemsToProcess();
}

//-----------------------------------------------------------------------------
// Purpose: Mark that we're shutting down
//-----------------------------------------------------------------------------
void CJobMgr::SetIsShuttingDown()
{
	m_WorkThreadPool.AllowTimeouts( true ); // during shutdown, we might abort jobs before waiting for the work item to complete
	m_bIsShuttingDown = true;
}


//-----------------------------------------------------------------------------
// Purpose: Wakes up the specified waiting job.
// Input:	jobID -			The job that owns this work item
//			bWorkItemCanceled - true if this job 
//			bShouldExist -	Do we assert if the job doesn't exist?
// Output:	true if the message was routed to a job
//-----------------------------------------------------------------------------
bool CJobMgr::BRouteWorkItemCompletedInternal( JobID_t jobID, bool bWorkItemCanceled, bool bShouldExist, bool bResumeImmediately )
{
	int iJob;

	// this can resume jobs, make sure we didn't switch threads
	CheckThreadID();

	if ( !BGetIJob( jobID, k_EJobPauseReasonWorkItem, bShouldExist, &iJob ) )
	{
		EG_MSG( SPEW_JOB, "BRouteWorkItemCompleted called for a job that could not be found!\n" );
		return false;
	}

	// continue the job
	m_MapJob[iJob]->m_bWorkItemCanceled = bWorkItemCanceled;
	if ( bResumeImmediately )
	{
		m_MapJob[iJob]->Continue();
	}
	else
	{
		AddToYieldList( *m_MapJob[iJob] );

		// reset the sleep reason
		m_MapJob[iJob]->m_ePauseReason = k_EJobPauseReasonYield;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Adds job to yield list (without actually pausing it) - internal
// Input  : &job - job that is yielding
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CJobMgr::AddToYieldList( CJob &job )
{
#ifdef _DEBUG
	FOR_EACH_LL( m_ListJobsYieldingRegPri, i )
	{
		Assert( m_ListJobsYieldingRegPri[i].m_JobID != job.GetJobID() );
	}
#endif

	// insert the job into the sleep list
	JobYielding_t jobYielding;
	jobYielding.m_JobID = job.GetJobID();
	jobYielding.m_nIteration = m_nCurrentYieldIterationRegPri;
	m_ListJobsYieldingRegPri.AddToTail( jobYielding );
}

//-----------------------------------------------------------------------------
// called by a job that has just been started to place itself on the yield queue instead of running
//-----------------------------------------------------------------------------
void CJobMgr::AddDelayedJobToYieldList( CJob &job )
{
	//make sure that this job is setup to be yielded at this point, otherwise it will not resume properly
	AssertMsg1( job.GetPauseReason() == k_EJobPauseReasonYield, "Delayed job %s was added to yield list but was not in expected yield state\n", job.GetName() );
	AddToYieldList( job );
}


//-----------------------------------------------------------------------------
// Purpose: yields until the next Run()
// Input  : &job - job that is yielding
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJobMgr::BYield( CJob &job )
{
	AddToYieldList( job );

	// yield
	PauseJob( job, k_EJobPauseReasonYield );
	if ( m_bJobTimedOut )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: yields IF NEEDED until the next Run()
// Input  : &job - job that is possibly yielding
//			pbYielded - optional, set to true if we did yield
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJobMgr::BYieldIfNeeded( CJob &job, bool *pbYielded )
{
	if ( pbYielded )
		*pbYielded = false;

	if ( job.GetMicrosecondsRun() > ( k_cMicroSecTaskGranularity / 2 ) )
	{
		bool bRet = BYield( job );
		if ( pbYielded )
			*pbYielded = bRet;
		return bRet;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Resumes jobs in list passed in that are ready to be awakened
//-----------------------------------------------------------------------------
bool CJobMgr::BResumeYieldingJobsFromList( CUtlLinkedList<JobYielding_t, int> &listJobsYielding, uint nCurrentIteration,
	 CLimitTimer &limitTimer )
{
	while ( listJobsYielding.Count() )
	{
		int iJobYielding = listJobsYielding.Head();
		const JobYielding_t &jobYielding = listJobsYielding[ iJobYielding ];

		if ( jobYielding.m_nIteration > nCurrentIteration )
			break;

		// pop the sleep off the top of the queue
		int iJob = m_MapJob.Find( jobYielding.m_JobID );
		listJobsYielding.Remove( iJobYielding );

		if ( m_MapJob.InvalidIndex() == iJob )
			continue;

		Assert( m_MapJob[iJob]->GetPauseReason() == k_EJobPauseReasonYield );

		// Should never be false, but if it is we 
		// don't want to do anything to this job
		if ( m_MapJob[iJob]->GetPauseReason() == k_EJobPauseReasonYield )
		{
			// resume the job
			m_MapJob[iJob]->Continue();
		}

		if ( limitTimer.BLimitReached() )
			break;
	}

	return ( listJobsYielding.Count() > 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Resumes any jobs that have are ready to be awaken
// Input:   limitTimer - limit timer not to exceed
// Output:  true if there is still work remaining to do, false otherwise
//-----------------------------------------------------------------------------
bool CJobMgr::BResumeYieldingJobs( CLimitTimer &limitTimer )
{
	return BResumeYieldingJobsFromList( m_ListJobsYieldingRegPri, m_nCurrentYieldIterationRegPri++, limitTimer );
}


//-----------------------------------------------------------------------------
// Purpose: Resumes any jobs that have are ready to be awaken
// Input:   limitTimer - limit timer not to exceed
// Output:  true if there is still work remaining to do, false otherwise
//-----------------------------------------------------------------------------
bool CJobMgr::BResumeSleepingJobs( CLimitTimer &limitTimer )
{
	while ( m_QueueJobSleeping.Count() )
	{
		const JobSleeping_t &jobSleeping = m_QueueJobSleeping.ElementAtHead();
		if ( jobSleeping.m_SWakeupTime.LTime() > CJobTime::LJobTimeCur() )
		{
			// Check if we need to heartbeat
			if ( jobSleeping.m_STimeTouched.CServerMicroSecsPassed() >= k_cMicroSecJobHeartbeat )
			{
				int iJob = m_MapJob.Find( jobSleeping.m_JobID );
				if ( m_MapJob.InvalidIndex() != iJob )
				{
					m_MapJob[iJob]->Heartbeat();
				}
			}

			return false;
		}

		// pop the sleep off the top of the queue
		int iJob = m_MapJob.Find( jobSleeping.m_JobID );
		m_QueueJobSleeping.RemoveAtHead();

		if ( m_MapJob.InvalidIndex() == iJob )
			continue;

		Assert( m_MapJob[iJob]->GetPauseReason() == k_EJobPauseReasonSleepForTime );
		
		// should never be false, but if it is we don't want to do anything to this job
		if ( m_MapJob[iJob]->GetPauseReason() == k_EJobPauseReasonSleepForTime )
		{
			// resume the job
			m_MapJob[iJob]->Continue();
		}

		if ( limitTimer.BLimitReached() )
			break;
	}

	return ( m_QueueJobSleeping.Count() > 0 );
}


//-----------------------------------------------------------------------------
// Purpose: comparison function for sorting sleeping jobs list by time
// Output : Returns true on if lhs is greater than the rhs
//-----------------------------------------------------------------------------
bool CJobMgr::JobSleepingLessFunc( JobSleeping_t const &lhs, JobSleeping_t const &rhs )
{
	// a lower time is a higher priority
	return ( lhs.m_SWakeupTime.LTime() > rhs.m_SWakeupTime.LTime() );
}

JobID_t g_DebugJob = k_GIDNil;

//-----------------------------------------------------------------------------
// Purpose: quickly iterates the list of jobs to make sure none have been paused 
//			for too long
//-----------------------------------------------------------------------------
void CJobMgr::CheckForJobTimeouts( CLimitTimer &limitTimer )
{
	// look through each active jobs
	// remove from the list any job that has successfully received it's I/O
	// send a failure msg to any job that has timed out
	// since the timeout time is constant, we only have to check until we find a job

	int cIter = 0;
	while ( m_ListJobTimeouts.Head() != m_ListJobTimeouts.InvalidIndex() )
	{
		cIter ++;

		// Break if limit timer is reached and we've already processed at least one item.
		if ( cIter > 1 && limitTimer.BLimitReached() )
			break;

		JobTimeout_t &jobtimeout = m_ListJobTimeouts[ m_ListJobTimeouts.Head() ];
		
		// see if it's timed out
		if ( !m_bIsShuttingDown && jobtimeout.m_STimeTouched.CServerMicroSecsPassed() < k_cMicroSecJobHeartbeat )
		{
			// we haven't reached our recycle or timeout limit, which means none of the jobs passed us in the queue would have either
			break;
		}

		// get the first job in the list, which is the most likely to have timed out
		int iJob = m_MapJob.Find( jobtimeout.m_JobID );
		if ( m_MapJob.InvalidIndex() == iJob )
		{
			m_MapJobTimeoutsIndexByJobID.Remove( jobtimeout.m_JobID );
			m_ListJobTimeouts.Remove( m_ListJobTimeouts.Head() );
			continue;
		}

		// job still exists, make sure it is still paused at the same point
		CJob *pJob = m_MapJob[iJob];

		if ( pJob->GetTimeSwitched().LTime() == jobtimeout.m_STimePaused.LTime() )
		{
			jobtimeout.m_cHeartbeatsBeforeTimeout--;

			if ( pJob->GetJobID() == g_DebugJob )
			{
				EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Heartbeat!\n" );
			}

			// Always heartbeat so anyone waiting on the job (say on another server) will know it is still alive
			// Note that we even do this right before we timeout, since the job will actually be continued and may just loop itself right back into this waiting state
			// Note also that we do NOT check pJob->GetNextHeartbeatTime() since we've already been watching our own timer
			pJob->Heartbeat();

			if ( m_bIsShuttingDown || jobtimeout.m_cHeartbeatsBeforeTimeout <= 0 )
			{
				// Job finished all its available heartbeats before its timeout limit, timeout if appropriate and remove from the list

				m_MapJobTimeoutsIndexByJobID.Remove( jobtimeout.m_JobID );
				m_ListJobTimeouts.Remove( m_ListJobTimeouts.Head() );

				bool bShouldTimeout = true;
				switch ( pJob->m_ePauseReason )
				{
				case k_EJobPauseReasonWaitingForLock:
				case k_EJobPauseReasonYield:
				case k_EJobPauseReasonSQL:
					bShouldTimeout = false;
					break;
				case k_EJobPauseReasonSleepForTime:
					bShouldTimeout = m_bIsShuttingDown;
					break;
				} // switch

				// If the job WAS waiting on IO but now is waiting on a Lock, Sleeping,
				// or Yielding, don't time it out. 
				// BUGBUG taylor we should fix things so that we can timeout Jobs waiting on
				// Locks and have them properly unlink themselves from the Lock chain
				if ( bShouldTimeout )
				{
					TimeoutJob( *( pJob ) );
				}
			}
			else
			{
				// Job has not yet used up all its available heartbeats before its timeout limit
				// We've already decremented its m_cHeartbeatsBeforeTimeout, now Reset its touched time too
				jobtimeout.m_STimeTouched.SetToJobTime();
				// Move it back to the end of the queue so it can come back up to the top for either another heartbeat or a timeout
				m_ListJobTimeouts.LinkToTail( m_ListJobTimeouts.Head() );
				int iIndexMap = m_MapJobTimeoutsIndexByJobID.Find( jobtimeout.m_JobID );
				if ( iIndexMap != m_MapJobTimeoutsIndexByJobID.InvalidIndex() )
				{
					int &iListIndex = m_MapJobTimeoutsIndexByJobID.Element( iIndexMap );
					iListIndex = m_ListJobTimeouts.Tail();
				}
				else
				{
					AssertMsg( false, "Map of jobs to timeout is corrupted" );
				}
			}

			continue;
		}
		else
		{
			// This is really the common heartbeating case, where the job waited a short while without ever reaching the k_cMicroSecJobHeartbeat limit
			// Thus, we need to heartbeat before removing it from the list IF the job has gone too long without heartbeating
			if ( pJob->BJobNeedsToHeartbeat() )
			{
				pJob->Heartbeat();
			}

			// Since the job didn't actually time out, clear this timeout event
			m_MapJobTimeoutsIndexByJobID.Remove( jobtimeout.m_JobID );
			m_ListJobTimeouts.Remove( m_ListJobTimeouts.Head() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Continues a job in a timed out state
//-----------------------------------------------------------------------------
void CJobMgr::TimeoutJob( CJob &job )
{

	if ( job.GetPauseReason() == k_EJobPauseReasonNetworkMsg )
		job.m_flags.m_bits.m_bTimeoutNetMsg = true;
	else
	{
		// these are so rare I dont want to add a column for them in the rollup
		EG_WARNING( SPEW_JOB, "Resuming job '%s (id: %lld)' due to timeout while paused for %s\n", job.GetName(), 
			job.GetJobID(), job.GetPauseReasonDescription() );
		job.m_flags.m_bits.m_bTimeoutOther = true;
	}

	m_JobStats.m_cJobsTimedOut++;
	m_bJobTimedOut = true;
	job.Continue();
    m_bJobTimedOut = false;
}


//-----------------------------------------------------------------------------
// Purpose: wakes up a job that was waiting on a lock
//-----------------------------------------------------------------------------
void CJobMgr::WakeupLockedJob( CJob &job )
{
	Assert( job.m_ePauseReason == k_EJobPauseReasonWaitingForLock );

	// in case of error, bug out now so as not
	// to cause more trouble
	if ( job.m_ePauseReason != k_EJobPauseReasonWaitingForLock )
	{
		return;
	}

	// insert the job into the yielding list so it will wakeup next Run
	AddToYieldList( job );

	// reset the sleep reason
	job.m_ePauseReason = k_EJobPauseReasonYield;
}


//-----------------------------------------------------------------------------
// Purpose: Pauses a job, and puts it in a list to check for timeouts
//-----------------------------------------------------------------------------
void CJobMgr::PauseJob( CJob &job, EJobPauseReason eJobPauseReason )
{
	Assert( !m_bDebugDisallowPause );
	if ( m_bDebugDisallowPause )
	{
		EmitError( SPEW_GC, "Job %s attempted to pause even though pauses were disabled\n", job.GetName() );
	}

	// add to list to check for timeouts later (or update the existing entry if it is already there)
	JobTimeout_t *pJobTimeout;
	int iMapIndex = m_MapJobTimeoutsIndexByJobID.Find( job.GetJobID() );
	if ( iMapIndex == m_MapJobTimeoutsIndexByJobID.InvalidIndex() )
	{
		 pJobTimeout = &m_ListJobTimeouts[ m_ListJobTimeouts.AddToTail() ];
		 m_MapJobTimeoutsIndexByJobID.Insert( job.GetJobID(), m_ListJobTimeouts.Tail() );
	}
	else
	{
		// There was an existing entry, in addition to updating it, move it to the tail
		int &iListIndex = m_MapJobTimeoutsIndexByJobID.Element( iMapIndex );
		m_ListJobTimeouts.LinkToTail( iListIndex );
		iListIndex = m_ListJobTimeouts.Tail();

		pJobTimeout = &m_ListJobTimeouts.Element( iListIndex );
	}

	pJobTimeout->m_JobID = job.GetJobID();
	pJobTimeout->m_STimePaused.SetToJobTime();
	pJobTimeout->m_STimeTouched.SetToJobTime();
	pJobTimeout->m_cHeartbeatsBeforeTimeout = job.CHeartbeatsBeforeTimeout();
	if ( eJobPauseReason == k_EJobPauseReasonWorkItem )
	{
		// work items control their own schedule - wait up to 6 hours
		pJobTimeout->m_cHeartbeatsBeforeTimeout = (6 * 60 * 60 * k_nMillion) / k_cMicroSecJobHeartbeat;
	}

	if ( pJobTimeout->m_cHeartbeatsBeforeTimeout <= 0 )
	{
		pJobTimeout->m_cHeartbeatsBeforeTimeout = k_cJobHeartbeatsBeforeTimeoutDefault;
	}

	// tell the job to pause
	job.Pause( eJobPauseReason );
}

//-----------------------------------------------------------------------------
// Purpose: dumps a list of currently active jobs to the console
// Output : int - number of jobs listed
//-----------------------------------------------------------------------------
int CJobMgr::DumpJobSummary()
{
	CUtlMap< uint32, JobStatsBucket_t, int > mapStatsBucket( 0, 0, DefLessFunc( uint32 ) );

	FOR_EACH_MAP_FAST( m_MapJob, i )
	{
		CJob &job = *m_MapJob[i];

		// the pointer to the name is a pointer to a constant string
		// so use this dirty trick to make lookups fast
		uint32 eBucket = (uint32)job.GetName();
		int iBucket = mapStatsBucket.Find( eBucket );
		if ( iBucket == mapStatsBucket.InvalidIndex() )
		{
			iBucket = mapStatsBucket.Insert( eBucket );
			V_strcpy_safe( mapStatsBucket[iBucket].m_rgchName, job.GetName() );
		}

		JobStatsBucket_t *pJobStatsBucket = &mapStatsBucket[iBucket];
		pJobStatsBucket->m_cCompletes++;												// overloading this to really mean "jobs running" for this spew
		pJobStatsBucket->m_cLocksAttempted += job.m_vecLocks.Count();					// overloading this to really be used for "locks held" for this spew
		pJobStatsBucket->m_u64JobDuration += job.m_STimeStarted.CServerMicroSecsPassed();

		switch ( job.m_ePauseReason )
		{
		case k_EJobPauseReasonNetworkMsg:		pJobStatsBucket->m_cPauseReasonNetworkMsg++;		break;
		case k_EJobPauseReasonSleepForTime:		pJobStatsBucket->m_cPauseReasonSleepForTime++;		break;
		case k_EJobPauseReasonWaitingForLock:	pJobStatsBucket->m_cPauseReasonWaitingForLock++;	break;
		case k_EJobPauseReasonYield:			pJobStatsBucket->m_cPauseReasonYield++;				break;
		case k_EJobPauseReasonSQL:				pJobStatsBucket->m_cPauseReasonSQL++;				break;
		case k_EJobPauseReasonWorkItem:			pJobStatsBucket->m_cPauseReasonWorkItem++;			break;
		default:	break;
		}
	}


	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, 
		"%50s --- running jobs (usec)-- -- locks held -- ----- pause reasons ---------------------------------\n", " " );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, 
		"%50s    count      aveduration                    netmsg      sql    sleep waitlock    yield workitem\n", "name" );

	JobProfileStats_t jobprofilestats;
	jobprofilestats.m_iJobProfileSort = k_EJobProfileSortOrder_Count;
	jobprofilestats.pmapStatsBucket = &mapStatsBucket;

	CUtlVector<int> vecSort( 0, mapStatsBucket.Count() );
	FOR_EACH_MAP_FAST( mapStatsBucket, iBucket )
	{
		vecSort.AddToTail( iBucket );
	}
	V_qsort_s( vecSort.Base(), vecSort.Count(), sizeof(int), (QSortCompareFuncCtx_t)ProfileSortFunc, &jobprofilestats );

	FOR_EACH_VEC( vecSort, iVec )
	{
		JobStatsBucket_t &bucket = mapStatsBucket[ vecSort[iVec] ];

		int64 msecDurationAve = bucket.m_u64JobDuration / bucket.m_cCompletes;

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%50s %8lld %16lld %13lld %11lld %8lld %8lld %8lld %8lld %8lld \n",
			bucket.m_rgchName,
			bucket.m_cCompletes,
			msecDurationAve,
			bucket.m_cLocksAttempted,

			bucket.m_cPauseReasonNetworkMsg,
			bucket.m_cPauseReasonSQL,
			bucket.m_cPauseReasonSleepForTime,
			bucket.m_cPauseReasonWaitingForLock,
			bucket.m_cPauseReasonYield,
			bucket.m_cPauseReasonWorkItem
			);
	}

	return m_MapJob.Count();
}


//-----------------------------------------------------------------------------
// Purpose: spews details about a job by ID
//-----------------------------------------------------------------------------
void CJobMgr::DumpJob( JobID_t jobID, int nPrintLocksMax ) const
{
	const CJob *pJob = GetPJob( jobID );
	if( !pJob )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Invalid job ID %llu\n", jobID );
	}
	else
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%llu\t%12s %12s\n",
			pJob->GetJobID(),
			pJob->GetName(),
			pJob->GetPauseReasonDescription() );

		if ( pJob->GetPauseReason() == k_EJobPauseReasonWaitingForLock && pJob->m_pWaitingOnLock != NULL )
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tWaiting for lock %s from: %s line %d\n", pJob->m_pWaitingOnLock->GetName(), pJob->m_pWaitingOnLockFilename, pJob->m_waitingOnLockLine );
			pJob->m_pWaitingOnLock->Dump( "\t ", nPrintLocksMax, true );
		}

		FOR_EACH_VEC( pJob->m_vecLocks, nLock )
		{
			CLock *pLock = pJob->m_vecLocks[nLock];
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tHolding lock %s:\n", pLock->GetName() );
			pLock->Dump( "\t ", nPrintLocksMax, true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: count the number of active jobs
//-----------------------------------------------------------------------------
int CJobMgr::CountJobs() const
{
	return m_MapJob.Count();
}


//-----------------------------------------------------------------------------
// Purpose: verify that current thread is correct
//-----------------------------------------------------------------------------
void CJobMgr::CheckThreadID()
{
	uint unCurrentThread = ThreadGetCurrentId();

	if ( m_unFrameFuncThreadID == 0 )
	{
		m_unFrameFuncThreadID = unCurrentThread;
	}
	else
	{
		// if this Assert goes of, you most likely tried to start
		// a job from a different thread then the frame function thread
		Assert( m_unFrameFuncThreadID == unCurrentThread );
	}
}


//-----------------------------------------------------------------------------
// Purpose: JobType_t comparer, used to sort the list of registered
//			jobs into a tree by msg that creates them
//-----------------------------------------------------------------------------
bool JobTypeSortFuncByMsg( JobType_t const * const &lhs, JobType_t const * const &rhs )
{
	if ( lhs->m_eCreationMsg == rhs->m_eCreationMsg )
	{
		return ( lhs->m_eServerType < rhs->m_eServerType );
	}

	return ( lhs->m_eCreationMsg < rhs->m_eCreationMsg );
}


//-----------------------------------------------------------------------------
// Purpose: JobType_t comparer, used to sort the list of registered
//			jobs into a tree by job name
//-----------------------------------------------------------------------------
bool JobTypeSortFuncByName( JobType_t const * const &lhs, JobType_t const * const &rhs )
{
	int iCompare = Q_strcmp( lhs->m_pchName, rhs->m_pchName );
	if ( iCompare == 0 )
	{
		return ( lhs->m_eServerType < rhs->m_eServerType );
	}

	return ( iCompare < 0 );
}


// singeton accessor to list of registered jobs
CUtlRBTree<const JobType_t *> &GMapJobTypesByMsg()
{
	static CUtlRBTree<const JobType_t *> s_MapJobTypes( 0, 0, JobTypeSortFuncByMsg );
	return s_MapJobTypes;
}

// singeton accessor to list of registered jobs
CUtlRBTree<const JobType_t *> &GMapJobTypesByName()
{
	static CUtlRBTree<const JobType_t *> s_MapJobTypes( 0, 0, JobTypeSortFuncByName );
	return s_MapJobTypes;
}


//-----------------------------------------------------------------------------
// Purpose: adds a new type of job into the global list
//-----------------------------------------------------------------------------
void CJobMgr::RegisterJobType( const JobType_t *pJobType )
{
	Assert( pJobType->m_pchName != NULL );
	Assert( pJobType->m_pJobFactory != NULL );
	GMapJobTypesByMsg().Insert( pJobType );
	GMapJobTypesByName().Insert( pJobType );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new job from the network msg
// Input  : *pServerParent - server to attach job to
//			msg - network msg
// Output : true if a job was created
//-----------------------------------------------------------------------------
bool CJobMgr::BLaunchJobFromNetworkMsg( void *pParent, const JobMsgInfo_t &jobMsgInfo, IMsgNetPacket *pNetPacket )
{
	if ( pNetPacket == NULL )
	{
		AssertMsg(pNetPacket, "CJobMgr::BLaunchJobFromNetworkMsg received NULL packet.");
		return false;
	}

	if ( pNetPacket->BHasTargetJobName() && BIsValidSystemMsg( pNetPacket->GetEMsg(), NULL ) )
	{
		JobType_t jobSearch = { pNetPacket->GetTargetJobName(), k_EGCMsgInvalid, jobMsgInfo.m_eServerType };
		int iJobType = GMapJobTypesByName().Find( &jobSearch );

		if ( GMapJobTypesByName().IsValidIndex( iJobType ) )
		{

			// Get shortcut to job info
			const JobType_t *pJobType = (GMapJobTypesByName())[iJobType];
			Assert( pJobType );
			Assert( pJobType->m_pchName );

			// Create the job
			CJob *job = pJobType->m_pJobFactory( pParent, NULL );

			// Safety check
			if ( job == NULL )
			{
				AssertMsg1( job, "Job factory returned NULL for job named '%s'!\n", pJobType->m_pchName );
				return false;
			}

			// Start the job
			job->StartJobFromNetworkMsg( pNetPacket, jobMsgInfo.m_JobIDSource );
			return true;
		}
	}
	else
	{
		JobType_t jobSearch = { 0, jobMsgInfo.m_eMsg, jobMsgInfo.m_eServerType };
		int iJobType = GMapJobTypesByMsg().Find( &jobSearch );

		if ( GMapJobTypesByMsg().IsValidIndex( iJobType ) )
		{

			// Get shortcut to job info
			const JobType_t *pJobType = (GMapJobTypesByMsg())[iJobType];
			Assert( pJobType );
			Assert( pJobType->m_pchName );

			// Create the job
			CJob *job = pJobType->m_pJobFactory( pParent, NULL );

			// Safety check
			if ( job == NULL )
			{
				AssertMsg3( job, "Job factory returned NULL for job msg %d, server type %d (named '%s')!\n", (int)jobMsgInfo.m_eMsg, (int)jobMsgInfo.m_eServerType, pJobType->m_pchName );
				return false;
			}

			// Start the job
			job->StartJobFromNetworkMsg( pNetPacket, jobMsgInfo.m_JobIDSource );
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: profile sort func
//-----------------------------------------------------------------------------
int CJobMgr::ProfileSortFunc( void *pCtx, const int *lhs, const int *rhs )
{
	JobProfileStats_t *pJobprofilestats = (JobProfileStats_t *)pCtx;
	int64 d = 0;
	switch ( pJobprofilestats->m_iJobProfileSort )
	{
	default:
	case k_EJobProfileSortOrder_Alpha:
		return Q_stricmp( pJobprofilestats->pmapStatsBucket->Element(*lhs).m_rgchName, 
							pJobprofilestats->pmapStatsBucket->Element(*rhs).m_rgchName );
	case k_EJobProfileSortOrder_Count:
		d = ((int64)pJobprofilestats->pmapStatsBucket->Element(*rhs).m_cCompletes - 
			(int64)pJobprofilestats->pmapStatsBucket->Element(*lhs).m_cCompletes);
		break;
	case k_EJobProfileSortOrder_TotalRuntime:
		d = ((int64)pJobprofilestats->pmapStatsBucket->Element(*rhs).m_u64RunTime - 
			(int64)pJobprofilestats->pmapStatsBucket->Element(*lhs).m_u64RunTime);
		break;
	}
	if ( d < 0 )
		return -1;
	if ( d > 0 )
		return 1;
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: dump out accumulated job profile data
//-----------------------------------------------------------------------------
void CJobMgr::ProfileJobs( EJobProfileAction ejobProfileAction, EJobProfileSortOrder iSortOrder )
{
	bool bClearBuckets = false;
	
	if ( ejobProfileAction == k_EJobProfileAction_Start )
	{
		if ( !m_bProfiling )
		{
			bClearBuckets = true;
		}
		m_bProfiling = true;
	}
	else if ( ejobProfileAction == k_EJobProfileAction_Stop )
	{
		m_bProfiling = false;
	}
	else if ( ejobProfileAction == k_EJobProfileAction_Clear )
	{
		bClearBuckets = true;
	}

	if ( bClearBuckets )
	{
		m_mapStatsBucket.RemoveAll();
	}

	if ( k_EJobProfileAction_Dump != ejobProfileAction )
		return;

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS,
		"%44s  --- completed jobs (usec)----------------------------------  ------ lock counts----------------------------------  ------ failures -----------\n", " " );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS,
		"%44s        count   averuntime   maxruntime  aveduration #yielded  attempted waited   failed longheld longwait wait-t/o  t/o-msg jobfailed longslice\n", "name" );

	JobProfileStats_t jobprofilestats;
	jobprofilestats.m_iJobProfileSort = iSortOrder;
	jobprofilestats.pmapStatsBucket = &m_mapStatsBucket;

	CUtlVector<int> vecSort( 0, m_mapStatsBucket.Count() );
	FOR_EACH_MAP_FAST( m_mapStatsBucket, iBucket )
	{
		vecSort.AddToTail( iBucket );
	}
	V_qsort_s( vecSort.Base(), vecSort.Count(), sizeof(int), (QSortCompareFuncCtx_t)ProfileSortFunc, &jobprofilestats );

	FOR_EACH_VEC( vecSort, iVec )
	{
		JobStatsBucket_t &bucket = m_mapStatsBucket[ vecSort[iVec] ];
		if ( bucket.m_cCompletes )
		{
			CCycleCount ccRunTime( bucket.m_u64RunTime / bucket.m_cCompletes );
			int64 usecAve = ccRunTime.GetMicroseconds();

			CCycleCount ccRunTimeMax( bucket.m_u64RunTimeMax );
			int64 usecMax = ccRunTimeMax.GetMicroseconds();

			int64 msecDurationAve = bucket.m_u64JobDuration / bucket.m_cCompletes;

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%44s %12lld %12lld %12lld %12lld %8lld %8lld %8lld %8lld %8lld %8lld %8lld %8lld %8lld %8lld\n",
				bucket.m_rgchName,
				bucket.m_cCompletes,
				usecAve,
				usecMax,
				msecDurationAve,
				bucket.m_cJobsPaused,
				bucket.m_cLocksAttempted,
				bucket.m_cLocksWaitedFor,
				bucket.m_cLocksFailed,
				bucket.m_cLocksLongHeld,
				bucket.m_cLocksLongWait,
				bucket.m_cWaitTimeout,
				bucket.m_cTimeoutNetMsg,
				bucket.m_cJobsFailed,
				bucket.m_cLongInterYieldTime );
		}
	}
	if ( m_mapOrphanMessages.Count() )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Messages that arrived responding to jobs that no longer exists and were dropped\n" );
		FOR_EACH_MAP_FAST( m_mapOrphanMessages, iBucket )
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%44s %12d\n", PchMsgNameFromEMsg( m_mapOrphanMessages.Key(iBucket) ), m_mapOrphanMessages[iBucket] );
		}
		m_mapOrphanMessages.RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dump a list of all jobs to the console
//			Each job is indexed, and that index can be used with
//			DebugJob() to cause a debug break in that job.
//-----------------------------------------------------------------------------
void CJobMgr::DumpJobs( const char *pszJobName, int nMax, int nPrintLocksMax ) const
{
	FOR_EACH_MAP_FAST( m_MapJob, iJob )
	{
		if ( nMax <= 0 )
			break;
		nMax--;

		if ( pszJobName == NULL || V_strcmp( pszJobName, m_MapJob[iJob]->GetName() ) == 0 )
		{
			DumpJob( m_MapJob.Key(iJob), nPrintLocksMax );
		}
	}
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Total job count: %d\n", m_MapJob.Count() );
}


//-----------------------------------------------------------------------------
// Purpose: cause a debug break in the given job
//-----------------------------------------------------------------------------
void CJobMgr::DebugJob( int iJob )
{
#ifdef DEBUG_JOB_LIST
	if ( sm_listAllJobs.IsValidIndex( iJob ) )
	{
		sm_listAllJobs[iJob]->Debug();
	}
	else
	{
		EmitInfo( SPEW_CONSOLE, 1, 1, "Job not found\n" );
	}
#else
	EmitInfo( SPEW_CONSOLE, 1, 1, "Job debugging disabled\n" );
#endif
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CJobMgr::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_MapJob );
	FOR_EACH_MAP_FAST( m_MapJob, iJob )
	{
		ValidatePtr( m_MapJob[iJob] );
	}

	ValidateObj( m_mapStatsBucket );
	FOR_EACH_MAP_FAST( m_mapStatsBucket, iBucket )
	{
		ValidateObj( m_mapStatsBucket[iBucket] );
	}

	ValidateObj( m_ListJobsYieldingRegPri );
	ValidateObj( m_ListJobTimeouts );
	ValidateObj( m_MapJobTimeoutsIndexByJobID );
	ValidateObj( m_QueueJobSleeping );
	ValidateObj( m_WorkThreadPool );
}


//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our global data
// Input:	validator -		Our global validator object
//-----------------------------------------------------------------------------
void CJobMgr::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "CJobMgr class statics" );

	ValidateObj( GMapJobTypesByMsg() );
	ValidateObj( GMapJobTypesByName() );
#ifdef DEBUG_JOB_LIST
	ValidateObj( sm_listAllJobs );
#endif
}
#endif // DBGFLAG_VALIDATE

} // namespace GCSDK
