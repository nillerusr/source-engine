//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"

#ifdef WIN32
#include "typeinfo.h"
#else
#include <typeinfo>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
//-----------------------------------------------------------------------------
// Purpose: EJobPauseReason descriptions
//-----------------------------------------------------------------------------
static const char * const k_prgchJobPauseReason[] =
{
	"active",
	"not started",
	"netmsg",
	"sleep for time",
	"waiting for lock",
	"yielding",
	"SQL",
	"work item",
};

COMPILE_TIME_ASSERT( ARRAYSIZE( k_prgchJobPauseReason ) == k_EJobPauseReasonCount );

CJob *g_pJobCur = NULL;


//-----------------------------------------------------------------------------
// Purpose: Delete a Job
// Input:	pJob -			The Job to delete
//-----------------------------------------------------------------------------
void CJob::DeleteJob( CJob *pJob )
{
	// we can't delete the if we still have a pending work item
	pJob->WaitForThreadFuncWorkItemBlocking();
	delete pJob;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input:	pServerParent -		The server we belong to
//-----------------------------------------------------------------------------
CJob::CJob( CJobMgr &jobMgr, const char *pchJobName ) : m_JobMgr( jobMgr ), m_pchJobName( pchJobName )
{
	m_ePauseReason = k_EJobPauseReasonNotStarted;

	m_JobID = jobMgr.GetNewJobID();
	m_pJobType = NULL;
	m_bWorkItemCanceled = false;
	m_hCoroutine = Coroutine_Create( &BRunProxy, this );
	m_pvStartParam = NULL;
	m_bRunFromMsg = false;
	m_pJobPrev = NULL;
	m_pWaitingOnLock = NULL;
	m_pJobToNotifyOnLockRelease = NULL;
	m_pWaitingOnWorkItem = NULL;
	m_STimeStarted.SetToJobTime();
	m_STimeSwitched.SetToJobTime();
	m_STimeNextHeartbeat.SetFromJobTime( k_cMicroSecJobHeartbeat );
	m_bIsLongRunning = false;
	m_cLocksAttempted = 0;
	m_cLocksWaitedFor = 0;
	m_flags.m_uFlags = 0;
	m_cyclecountTotal = 0;
	m_unWaitMsgType = 0;

	GetJobMgr().InsertJob( *this );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CJob::~CJob()
{
	// don't want SendMsgToConnection to call back into us, we *know*
	// we are replying to these other jobs now
	g_pJobCur = NULL;

	// reset the job pointer
	g_pJobCur = m_pJobPrev;

	// remove from the job tracking list
	GetJobMgr().RemoveJob( *this );

	// Forcefully release any locks
	ReleaseLocks();

	// free any network messages we've allocated
	FOR_EACH_VEC( m_vecNetPackets, i )
	{
		m_vecNetPackets[i]->Release();
	}
	m_vecNetPackets.RemoveAll();

	AssertMsg2( 0 == GetDoNotYieldDepth(), "Job ending with %d open Do Not Yields. Are we missing a END_DO_NOT_YIELD()? Innermost delared at %s",
		GetDoNotYieldDepth(), m_stackDoNotYieldGuards[m_stackDoNotYieldGuards.Head()] );
}


//-----------------------------------------------------------------------------
// Purpose: if necessary wait for the pending work item to finish
//-----------------------------------------------------------------------------
void CJob::WaitForThreadFuncWorkItemBlocking()
{
	if ( m_pWaitingOnWorkItem )
	{
		switch ( GetPauseReason() )
		{
		case k_EJobPauseReasonWorkItem:
			// force the workitem to be canceled in case it's still in the in-queue
			m_pWaitingOnWorkItem->ForceTimeOut();

			// we can't shutdown the job while it's work item is currently running
			// alot of work items refernce back into the job object
			while ( m_pWaitingOnWorkItem->BIsRunning() )
				ThreadSleep( 25 );

			m_pWaitingOnWorkItem = NULL;
			break;

#if 0 // not used in gcsdk
		case k_EJobPauseReasonGeneric:
			AssertMsg1( ( !m_pWaitingForGeneric || ( m_pWaitingForGeneric == ( void * ) 1 ) ), "CJob::WaitForThreadFuncWorkItemBlocking job %s will leak generic heap object", GetName() );
			// Let another assert fire later, don't null-out: m_pWaitingForGeneric = NULL;
			break;
#endif

		default:
			AssertMsg1( false, "CJob::WaitForThreadFuncWorkItemBlocking job %s has unexpected work item state", GetName() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns the name of the job
//-----------------------------------------------------------------------------
const char *CJob::GetName() const
{
	if ( m_pchJobName )
		return m_pchJobName;
	else if ( m_pJobType )
		return m_pJobType->m_pchName;
	else
		return "";
}


//-----------------------------------------------------------------------------
// Purpose: string description of why we're paused
//-----------------------------------------------------------------------------
const char *CJob::GetPauseReasonDescription()  const
{
	static char srgchPauseReason[k_cSmallBuff];
	if ( GetPauseReason() < Q_ARRAYSIZE( k_prgchJobPauseReason ) )
	{
		switch( GetPauseReason() )
		{
			case k_EJobPauseReasonWaitingForLock:
			{
				Q_snprintf( srgchPauseReason, k_cSmallBuff, "WOL: 0x%x (%s)", (unsigned int)m_pWaitingOnLock, m_pWaitingOnLock ? m_pWaitingOnLock->GetName() : "null" );
				return srgchPauseReason;
			}

			case k_EJobPauseReasonNetworkMsg:
			{
				const char *pchMsgType;
				if( g_theMessageList.GetMessage( m_unWaitMsgType, &pchMsgType, 0xFF ) )
				{
					Q_snprintf( srgchPauseReason, k_cSmallBuff, "NetMsg: %s", pchMsgType );
					return srgchPauseReason;
				}
				else
				{
					Q_snprintf( srgchPauseReason, k_cSmallBuff, "NetMsg: Unknown %d", m_unWaitMsgType );
					return srgchPauseReason;
				}
			}

			default:
				return k_prgchJobPauseReason[ GetPauseReason() ];
		}
	}

	return "undefined";
}


//-----------------------------------------------------------------------------
// Purpose: accessor to get access to the JobMgr from the server we belong to
//-----------------------------------------------------------------------------
CJobMgr &CJob::GetJobMgr() 
{ 
	return m_JobMgr;
}


//-----------------------------------------------------------------------------
// Purpose: Starts the job, based on the current network msg
// Input  : hConnection -	Connection that the message was received form
//			pubPkt -			The raw message packet
//			cubPkt -			The size of the message packet
//-----------------------------------------------------------------------------
void CJob::StartJobFromNetworkMsg( IMsgNetPacket *pNetPacket, const JobID_t &gidJobIDSrc )
{
	// hang on to the packet with the message that started this job
	AddPacketToList( pNetPacket, gidJobIDSrc );
	SetFromFromMsg( true );
	// start running this job
	InitCoroutine();
	//and start executing the job
	Continue();
}

//-----------------------------------------------------------------------------
// Purpose: Starts the job
//-----------------------------------------------------------------------------
void CJob::StartJob( void * pvStartParam )
{
	// job must not be started
	AssertMsg1( m_ePauseReason == k_EJobPauseReasonNotStarted, "CJob::StartJob() called twice on job %s\n", GetName() );
	// save the start params for this job
	SetStartParam( pvStartParam );
	m_JobMgr.CheckThreadID();

	// start running this job
	InitCoroutine();
	Continue();
}

//-----------------------------------------------------------------------------
// Purpose: Starts the job in a suspended state
//-----------------------------------------------------------------------------

void CJob::StartJobDelayed( void * pvStartParam )
{
	// job must not be started
	AssertMsg1( m_ePauseReason == k_EJobPauseReasonNotStarted, "CJob::StartJob() called twice on job %s\n", GetName() );
	// save the start params for this job
	SetStartParam( pvStartParam );
	m_JobMgr.CheckThreadID();

	//init the job, but don't start it
	InitCoroutine();

	//set our job as suspended (a yield)
	m_ePauseReason = k_EJobPauseReasonYield;
	m_JobMgr.AddDelayedJobToYieldList( *this );
}

//-----------------------------------------------------------------------------
// Purpose: setup the debug memory and job name before running a job the first time
//-----------------------------------------------------------------------------
void CJob::InitCoroutine()
{
	// make sure we have an appropriate chunk of memory to store 
	// our debug alloc info
	if( MemAlloc_GetDebugInfoSize() )
	{
		m_memAllocStack.EnsureCapacity( MemAlloc_GetDebugInfoSize() );

		// Set the job name as the root
		MemAlloc_InitDebugInfo( m_memAllocStack.Base(), GetName(), 0 ); 
	}

	// Set the job name
	if ( !m_pJobType && !m_pchJobName )
	{
#ifdef _WIN32
		m_pchJobName = typeid( *this ).raw_name();
		if ( m_pchJobName[0] == '.' && m_pchJobName[1] == '?' && m_pchJobName[2] == 'A')
			m_pchJobName += 4;
		if ( m_pchJobName[0] == '?' && m_pchJobName[1] == '$' )
			m_pchJobName += 2;
#else
		m_pchJobName = typeid( *this ).name();
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: proxy function for starting the job in the coroutine
//-----------------------------------------------------------------------------
void CJob::BRunProxy( void *pvThis )
{
	CJob *pJob = (CJob *)pvThis;

	// run the job
	bool bJobReturn = false;
	if ( pJob->m_bRunFromMsg )
	{
		Assert( pJob->m_vecNetPackets.Count() > 0 );
		bJobReturn = pJob->BYieldingRunJobFromMsg( pJob->m_vecNetPackets.Head() );
	}
	else
	{
		bJobReturn = pJob->BYieldingRunJob( pJob->m_pvStartParam );
	}

	pJob->m_flags.m_bits.m_bJobFailed = ( true != bJobReturn );

	// kill it
	DeleteJob( pJob );
}


//-----------------------------------------------------------------------------
// Purpose: Adds this packet to the linked list of packets for this job
//-----------------------------------------------------------------------------
void CJob::AddPacketToList( IMsgNetPacket *pNetPacket, const GID_t gidJobIDSrc )
{
	Assert( pNetPacket );
	pNetPacket->AddRef();

	m_vecNetPackets.AddToTail( pNetPacket );
}


//-----------------------------------------------------------------------------
// Purpose: marks a net packet as being finished with, releases the packet and frees the memory
//-----------------------------------------------------------------------------
void CJob::ReleaseNetPacket( IMsgNetPacket *pNetPacket )
{
	int iVec = m_vecNetPackets.Find( pNetPacket );
	if ( iVec != m_vecNetPackets.InvalidIndex() )
	{
		pNetPacket->Release();
		m_vecNetPackets.Remove( iVec );
	}
	else
	{
		AssertMsg( false, "Job failed trying to release a IMsgNetPacket it doesn't own" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: continues the current job
//-----------------------------------------------------------------------------
void CJob::Continue()
{
	AssertNotRunningThisJob();

	m_pJobPrev = g_pJobCur;
	g_pJobCur = this;

	// Record frame we're starting in and start a timer to track how long we work for within the frame
	// Also, add in how much time has passed since we were last paused to track heartbeat requirements
	m_FastTimerDelta.Start();

	m_STimeSwitched.SetToJobTime();

	// Check if we need to heartbeat
	if ( BJobNeedsToHeartbeat() )
	{
		Heartbeat();
	}

	m_JobMgr.GetJobStats().m_cTimeslices++;

	m_ePauseReason = k_EJobPauseReasonNone;
#if defined(_WIN32) && defined(COROUTINE_TRACE)
	const char *pchRawName = typeid( *this ).raw_name();
	if ( pchRawName[0] == '.' && pchRawName[1] == '?' && pchRawName[2] == 'A')
		pchRawName += 4;
	if ( pchRawName[0] == '?' && pchRawName[1] == '$' )
		pchRawName += 2;
#else
	const char *pchRawName = "";
#endif

	// Save debug credit "call stack"
	void *pvSaveDebugInfo = GetJobMgr().GetMainMemoryDebugInfo();
	MemAlloc_SaveDebugInfo( pvSaveDebugInfo );
	MemAlloc_RestoreDebugInfo( m_memAllocStack.Base() );

	// continue the coroutine, with the profiling if necessary
	bool bJobStillActive;
#if defined( VPROF_ENABLED )
	if ( g_VProfCurrentProfile.IsEnabled() )
	{
		VPROF_BUDGET( GetName(), VPROF_BUDGETGROUP_JOBS_COROUTINES ); 
		bJobStillActive = Coroutine_Continue( m_hCoroutine, pchRawName );
	}
	else
#endif
	{
		bJobStillActive = Coroutine_Continue( m_hCoroutine, pchRawName );
	}

	// WARNING: MEMBER VARIABLES ARE NOW UNSAFE TO ACCESS - this CJob may be deleted

	// Restore debug credit call stack
	if( bJobStillActive )
	{
		// only save off debug info for jobs that are still running
		MemAlloc_SaveDebugInfo( m_memAllocStack.Base() );
	}
	MemAlloc_RestoreDebugInfo( pvSaveDebugInfo );

}

void CJob::Debug()
{
	AssertNotRunningThisJob();

	// This function will 'load' this coroutine then immediately
	// break into the debugger. When execution is continued, it
	// will pop back out to this context

	// So, we don't set m_pJobPrev or g_pJobCur because nobody
	// would ever have the chance to see them anyway. 
	Coroutine_DebugBreak( m_hCoroutine );
}

//-----------------------------------------------------------------------------
// Purpose: pauses the current job
//-----------------------------------------------------------------------------
void CJob::Pause( EJobPauseReason eReason )
{
	AssertRunningThisJob();
	AssertMsg1( 0 == m_stackDoNotYieldGuards.Count(), "Yielding while in a BEGIN_DO_NOT_YIELD() block declared at %s", m_stackDoNotYieldGuards[m_stackDoNotYieldGuards.Head()] );

	g_pJobCur = m_pJobPrev;

	// End our timer so we know how much time we've spent
	m_FastTimerDelta.End();
	m_cyclecountTotal += m_FastTimerDelta.GetDuration();

	if ( m_FastTimerDelta.GetDuration().GetMicroseconds() > k_cMicroSecTaskGranularity * 10 )
	{
		m_flags.m_bits.m_bLongInterYield = true;
	}
	// pause this job, remembering which frame and why
	m_ePauseReason = eReason;
	// We shouldn't have to set the frame -- it should be the same one
	Assert( m_STimeSwitched.LTime() == CJobTime::LJobTimeCur() );
	Coroutine_YieldToMain();
}

void CJob::GenerateAssert( const char *pchMsg )
{
	// Default message if they didn't provide a custom one
	if ( !pchMsg )
	{
		pchMsg = "Forced assert failure";
	}

	// Just for grins, allow this function to be called whether
	// we are the current job or not
	if ( this == g_pJobCur )
	{
		AssertMsg1( !"Job assertion requested", "%s", pchMsg );
	}
	else
	{
		Coroutine_DebugAssert( m_hCoroutine, pchMsg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: pauses the job until a network msg for the job arrives
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitForMsg( IMsgNetPacket **ppNetPacket )
{
	AssertRunningThisJob();
	*ppNetPacket = NULL;

	// await and retrieve the network message
	if ( GetJobMgr().BYieldingWaitForMsg( *this ) )
	{
		Assert( m_vecNetPackets.Count() > 0 );
		*ppNetPacket = m_vecNetPackets.Tail();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: pauses the job until a network msg for the job arrives
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitForMsg( CGCMsgBase *pMsg, MsgType_t eMsg )
{
	IMsgNetPacket *pNetPacket = NULL;

	// Check if we already waited for a message of this type
	// but timed out.  If so, then we currently don't have a way
	// to tell if the message we might receive is the reply
	// to the old mesage, of the one we're about to send.
	// So let's just disallow this entirely.
	if ( BHasFailedToReceivedMsgType( eMsg ) )
	{
		AssertMsg2( false, "Job %s cannot wait for msg %u, it has already failed to wait for that msg type.", GetName(), eMsg );
		return false;
	}

	m_unWaitMsgType = eMsg;
	if ( !BYieldingWaitForMsg( &pNetPacket) )
	{
		// Remember this event, so we can at least detect if a reply comes late, we don't get confused.
		MarkFailedToReceivedMsgType( eMsg );
		return false;
	}

	pMsg->SetPacket( pNetPacket );

	if ( pMsg->Hdr().m_eMsg != eMsg )
	{
		// Remember this event, so we can at least detect if a reply comes late, we don't get confused.
		MarkFailedToReceivedMsgType( eMsg );

		AssertMsg2( false, "CJob::BYieldingWaitForMsg expected msg %u but received %u", eMsg, pMsg->Hdr().m_eMsg );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
bool CJob::BHasFailedToReceivedMsgType( MsgType_t m ) const
{
	FOR_EACH_VEC( m_vecMsgTypesFailedToReceive, i )
	{
		if ( m_vecMsgTypesFailedToReceive[i] == m )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
void CJob::MarkFailedToReceivedMsgType( MsgType_t m )
{
	if ( !BHasFailedToReceivedMsgType( m ) )
	{
		m_vecMsgTypesFailedToReceive.AddToTail( m );
	}
}

//-----------------------------------------------------------------------------
void CJob::ClearFailedToReceivedMsgType( MsgType_t m )
{
	m_vecMsgTypesFailedToReceive.FindAndFastRemove( m );
}

//-----------------------------------------------------------------------------
// Purpose: pauses the job until a network msg for the job arrives
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitForMsg( CProtoBufMsgBase *pMsg, MsgType_t eMsg )
{
	IMsgNetPacket *pNetPacket = NULL;

	// Check if we already waited for a message of this type
	// but timed out.  If so, then we currently don't have a way
	// to tell if the message we might receive is the reply
	// to the old mesage, of the one we're about to send.
	// So let's just disallow this entirely.
	if ( BHasFailedToReceivedMsgType( eMsg ) )
	{
		AssertMsg2( false, "Job %s cannot wait for msg %u, it has already failed to wait for that msg type.", GetName(), eMsg );
		return false;
	}

	m_unWaitMsgType = eMsg;
	if ( !BYieldingWaitForMsg( &pNetPacket) )
	{
		// Remember this event, so we can at least detect if a reply comes late, we don't get confused.
		MarkFailedToReceivedMsgType( eMsg );
		return false;
	}

	pMsg->InitFromPacket( pNetPacket );

	if ( pMsg->GetEMsg() != eMsg )
	{
		// Remember this event, so we can at least detect if a reply comes late, we don't get confused.
		MarkFailedToReceivedMsgType( eMsg );

		EmitError( SPEW_GC, "CJob::BYieldingWaitForMsg expected msg %u but received %u", eMsg, pMsg->GetEMsg() );
		return false;
	}

	return true;
}

#ifdef GC

bool CJob::BYieldingWaitForMsg( CGCMsgBase *pMsg, MsgType_t eMsg, const CSteamID &expectedID )
{
	if( !BYieldingWaitForMsg( pMsg, eMsg ) )
		return false;

	if( pMsg->Hdr().m_ulSteamID != expectedID.ConvertToUint64() )
	{
		EmitError( SPEW_GC, "CJob::BYieldingWaitForMsg expected reply from steam ID %s, but instead got a response from %s for message %d\n", expectedID.Render(), CSteamID( pMsg->Hdr().m_ulSteamID ).Render(), eMsg );
		return false;
	}

	return true;
}


bool CJob::BYieldingWaitForMsg( CProtoBufMsgBase *pMsg, MsgType_t eMsg, const CSteamID &expectedID )
{
	if( !BYieldingWaitForMsg( pMsg, eMsg ) )
		return false;

	if( pMsg->GetClientSteamID() != expectedID )
	{
		EmitError( SPEW_GC, "CJob::BYieldingWaitForMsg expected reply from steam ID %s, but instead got a response from %s for message %d\n", expectedID.Render(), pMsg->GetClientSteamID().Render(), eMsg );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: pauses the job until a network msg for the job arrives
//-----------------------------------------------------------------------------
bool CJob::BYieldingRunQuery( CGCSQLQueryGroup *pQueryGroup, ESchemaCatalog eSchemaCatalog )
{
	AssertRunningThisJob();

	// await and retrieve the network message
	return GetJobMgr().BYieldingRunQuery( *this, pQueryGroup, eSchemaCatalog );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: pauses the job until a work item callback occurs
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitForWorkItem( const char *pszWorkItemName )
{
	AssertRunningThisJob();

	// await the work item completion
	return GetJobMgr().BYieldingWaitForWorkItem( *this, pszWorkItemName );
}


//-----------------------------------------------------------------------------
// Purpose: CWorkItem for processing functions in CJob-derived classes on another thread
//-----------------------------------------------------------------------------
class CJobThreadFuncWorkItem : public CWorkItem
{
public:
	DECLARE_WORK_ITEM( CJobThreadFuncWorkItem );
	CJobThreadFuncWorkItem( CJob *pJob, JobThreadFunc_t jobThreadFunc, CFunctor *pFunctor ) : CWorkItem( pJob->GetJobID() ),
		m_pJob( pJob ),
		m_pJobThreadFunc( jobThreadFunc ),
		m_pFunctor( pFunctor )
	{
	}

	virtual bool ThreadProcess( CWorkThread *pThread )
	{
		if ( m_pJobThreadFunc )
			(m_pJob->*m_pJobThreadFunc)();
		if ( m_pFunctor )
			(*m_pFunctor)();
		return true;
	}

private:
	CJob *m_pJob;
	JobThreadFunc_t m_pJobThreadFunc;
	CFunctor *m_pFunctor;
};


//-----------------------------------------------------------------------------
// Purpose: pauses the job until a work item callback occurs
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitForThreadFuncWorkItem( CWorkItem *pItem )
{
	AssertRunningThisJob();
	Assert( m_pWaitingOnWorkItem == NULL );
	Assert( pItem->GetJobID() == GetJobID() );

	m_pWaitingOnWorkItem = pItem;

	// add it to a central thread pool
	GetJobMgr().AddThreadedJobWorkItem( pItem );

	// await the work item completion
	bool bSuccess = GetJobMgr().BYieldingWaitForWorkItem( *this );

	m_pWaitingOnWorkItem = NULL;

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: pauses the job until a work item callback occurs
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitForThreadFunc( CFunctor *jobFunctor )
{
	// store off which function to launch when we're done
	CJobThreadFuncWorkItem *pJobThreadFuncWorkItem = new CJobThreadFuncWorkItem( this, NULL, jobFunctor );

	bool bSuccess = BYieldingWaitForThreadFuncWorkItem( pJobThreadFuncWorkItem );

	// free the thread func
	SafeRelease( jobFunctor );
	SAFE_RELEASE( pJobThreadFuncWorkItem );

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Allows a job that was paused for a specific reason to resume
//-----------------------------------------------------------------------------
void CJob::EndPause( EJobPauseReason eExpectedState ) 
{ 
	Assert( m_ePauseReason == eExpectedState );
	if( m_ePauseReason == eExpectedState )
	{
		m_ePauseReason = k_EJobPauseReasonYield; 
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of heartbeats to wait before timing out this job
//-----------------------------------------------------------------------------
uint32 CJob::CHeartbeatsBeforeTimeout() 
{ 
	return k_cJobHeartbeatsBeforeTimeoutDefault; 
}


//-----------------------------------------------------------------------------
// Purpose: Send heartbeat messages to our listeners during long operations
//			to let them know we're still alive and avoid timeouts
//			This should be called by the CJobMgr
//-----------------------------------------------------------------------------
void CJob::Heartbeat()
{
	// Reset our counter
	m_STimeNextHeartbeat.SetFromJobTime( k_cMicroSecJobHeartbeat );
}




//-----------------------------------------------------------------------------
// Purpose: waits for specified time and checks for timeout.  Useful when you
//			need to repeatedly sleep while waiting for something to happen.
//			This function uses STime (server "pseudo" time) to determine
//			timeout conditions.
//
// Input:	cMicrosecondsToSleep - duration to sleep this call
//			stimeStarted - the time to calculate timeout from.  (Typically,
//				the time you start calling this in a loop, passing the same
//				start time each time you call this method.)
//			nMicroSecLimit - duration from stimeStarted to consider timed out
// Output : Returns true if not timed out yet, false if timed out
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitTimeWithLimit( uint32 cMicrosecondsToSleep, CJobTime &stimeStarted, int64 nMicroSecLimit )
{
	if ( stimeStarted.CServerMicroSecsPassed() > nMicroSecLimit )
		return false;

	return BYieldingWaitTime( cMicrosecondsToSleep );
}


//-----------------------------------------------------------------------------
// Purpose: waits for specified time and checks for timeout.  Useful when you
//			need to repeatedly sleep while waiting for something to happen.
//			This function uses RTime (wall-clock "real" time) to determine
//			timeout conditions.
//
// Input:	cMicrosecondsToSleep - duration to sleep this call
//			nSecLimit - duration from stimeStarted to consider timed out
// Output : Returns true if not timed out yet, false if timed out
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitTimeWithLimitRealTime( uint32 cMicrosecondsToSleep, int nSecondsLimit )
{
	return BYieldingWaitTime( cMicrosecondsToSleep );
}


//-----------------------------------------------------------------------------
// Purpose: pauses the job for the specified amount of time
// Input  : m_cMicrosecondsToSleep - microseconds to wait for
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitTime( uint32 cMicrosecondsToSleep )
{
	AssertRunningThisJob();
	return GetJobMgr().BYieldingWaitTime( *this, cMicrosecondsToSleep );
}

//-----------------------------------------------------------------------------
// Purpose: pauses the job until the next time the JobMgr Run() is called
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYield()
{
	AssertRunningThisJob();
	return GetJobMgr().BYield( *this );
}

//-----------------------------------------------------------------------------
// Purpose: Pauses the job ONLY IF JobMgr decides it needs to based on time run and priority
//			If pausing, pauses until the next time the JobMgr Run() is called
// Input:	pbYielded -	Set to true if we did yield
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYieldIfNeeded( bool *pbYielded )
{
	AssertRunningThisJob();

	if ( pbYielded )
		*pbYielded = false;

	// Assume only low priority jobs need to yield
	// Automatically bail out here if the job is not low priority

	return GetJobMgr().BYieldIfNeeded( *this, pbYielded );
}


//-----------------------------------------------------------------------------
// Purpose: Pauses the job for a single frame
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::BYieldingWaitOneFrame()
{
	return BYieldingWaitTime( 1 );
}


//-----------------------------------------------------------------------------
// Purpose: Blocks until we acquire the lock on the specified object
// Input  : *pLock - object to lock
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::_BYieldingAcquireLock( CLock *pLock, const char *filename, int line )
{
	AssertRunningThisJob();

	// Skip the path info from the filename.  It just maks the debug messages excessively long.
	filename = V_GetFileName( filename );

	// Is the lock locked by this job? If so, inc the ref count.
	if ( pLock->GetJobLocking() == this )
	{
		pLock->IncrementReference();
		return true;
	}

	// jobs can have multiple locks as long as they are in priority order
	FOR_EACH_VEC( m_vecLocks, i )
	{
		if( m_vecLocks[i]->GetLockType() == pLock->GetLockType() )
		{
			if( m_vecLocks[i]->GetLockSubType() <= pLock->GetLockSubType() )
			{
				AssertMsg7( false, "Job %s Locking %s at %s:(%d) with yielding; holds lock %s from %s(%d)\n",
					GetName(), 
					pLock->GetName(),
					filename, line,
					m_vecLocks[i]->GetName(),
					m_vecLocks[i]->m_pFilename, m_vecLocks[i]->m_line );
				return false;
			}
		}
		else if ( m_vecLocks[i]->GetLockType() < pLock->GetLockType() )
		{
			AssertMsg7( false, "Job %s Locking %s at %s:(%d) with yielding; holds lock %s from %s(%d)\n",
				GetName(), 
				pLock->GetName(),
				filename, line,
				m_vecLocks[i]->GetName(),
				m_vecLocks[i]->m_pFilename, m_vecLocks[i]->m_line );
			return false;
		}
	}

	if( m_pWaitingOnLock != NULL )
	{
		AssertMsg7( false, "Job (%s) locking %s at %s(%d); already waiting on %s at %s(%d).\n", 
				  GetName(), 
				  pLock->GetName(), 
				  filename, line,
				  m_pWaitingOnLock->GetName(),
				  m_pWaitingOnLock->m_pFilename, m_pWaitingOnLock->m_line );
		return false;
	}

	m_cLocksAttempted++;
	if ( pLock->BIsLocked() )
	{
		// tell the job we want the lock next
		// But walking the entire linked list is slow so
		// skip to the tail pointer
		pLock->AddToWaitingQueue( this );

		// We should be the tail of the list
		Assert( NULL == m_pJobToNotifyOnLockRelease );

		// yield until we get the lock
		m_pWaitingOnLock = pLock;
		m_pWaitingOnLockFilename = filename;
		m_waitingOnLockLine = line;
		m_cLocksWaitedFor++;
		Pause( k_EJobPauseReasonWaitingForLock );
		m_pWaitingOnLock = NULL;

		// make sure we actually got it, instead of timing out
		int index = m_vecLocks.Find( pLock );
		if ( index != m_vecLocks.InvalidIndex() && this == pLock->GetJobLocking() )
		{
			pLock->IncrementReference();
			return true;
		}
		else
		{
			m_flags.m_bits.m_bLocksFailed = true;
			EmitWarning( SPEW_JOB, LOG_ALWAYS, "Failed to get lock %s at %s(%d) after waiting in %s\n", pLock->GetName(), filename, line, GetName() );
			if ( m_vecLocks.Count() == 0 )
			{
				EmitWarning( SPEW_JOB, LOG_ALWAYS, "m_vecLocks.Count(): %d, this: 0x%p, pLock->GetJobLocking(): %s (0x%p)\n", 
					m_vecLocks.Count(), this, pLock->GetJobLocking() ? pLock->GetJobLocking()->GetName() : "(null)", pLock->GetJobLocking() );
			}
			else
			{
				EmitWarning( SPEW_JOB, LOG_ALWAYS, "m_vecLocks.Count(): %d this: 0x%p, pLock: 0x%p pLock->GetJobLocking(): %s (0x%p)\n", 
					m_vecLocks.Count(), this, pLock, pLock->GetJobLocking() ? pLock->GetJobLocking()->GetName() : "(null)", pLock->GetJobLocking() );
				FOR_EACH_VEC( m_vecLocks, i )
				{
					EmitWarning( SPEW_JOB, LOG_ALWAYS, "m_vecLocks[%d]: %s (0x%p) %s(%d)\n", 
						i, m_vecLocks[i] ? m_vecLocks[i]->GetName() : "(null)", m_vecLocks[i], m_vecLocks[i]->m_pFilename, m_vecLocks[i]->m_line  );
				}
			}
			return false;
		}
	}
	else
	{
		// unused, take it for ourself
		pLock->IncrementReference();
		_SetLock( pLock, filename, line );
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Either locks on the specified object immediately or returns failure
// Input  : *pLock - object to lock
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CJob::_BAcquireLockImmediate( CLock *pLock, const char *filename, int line )
{
	AssertRunningThisJob();

	AssertMsg5( m_pWaitingOnLock == NULL, "Job (%s) at %s(%d) trying to take a lock while it was already waiting for the first one at %s(%d)",  GetName(), filename, line, m_pWaitingOnLockFilename, m_waitingOnLockLine );

	m_cLocksAttempted++;

	// Is the lock locked by this job? If so, inc the ref count.
	if ( pLock->GetJobLocking() == this )
	{
		pLock->IncrementReference();
		return true;
	}

	if ( !pLock->BIsLocked() )
	{
		// unused, take it for ourself
		pLock->IncrementReference();
		_SetLock( pLock, filename, line );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Releases the specified lock, passing it on to the next job if necessary
//-----------------------------------------------------------------------------
void CJob::_ReleaseLock( CLock *pLock, bool bForce, const char *filename, int line )
{
	Assert( pLock );
	if ( !pLock )
		return;

	Assert( m_vecLocks.HasElement( pLock ) );
	if ( !m_vecLocks.HasElement( pLock ) )
	{
		EmitError( SPEW_JOB, "Job %s trying to release lock %s at %s(%d) it's not holding\n", GetName(), pLock->GetName(), filename, line );
		return;
	}

	if ( pLock->GetJobLocking() != this )
	{
		EmitError( SPEW_JOB, "Job %s trying to release lock %s at %s(%d) though the lock is held by %s\n", GetName(), pLock->GetName(), filename, line, pLock->GetJobLocking()->GetName() );
		return;
	}

	if ( bForce )
	{
		// Force clear reference count
		pLock->ClearReference();
	}
	else
	{
		// Dec the reference count. If it is not yet zero, don't fully unlock
		if ( pLock->DecrementReference() > 0 )
		{
			return;
		}
	}

	if ( pLock->m_pJobToNotifyOnLockRelease )
	{
		// post a message to the main system to wakeup the next lock
		PassLockToJob( pLock->m_pJobToNotifyOnLockRelease, pLock );
		m_pJobToNotifyOnLockRelease = NULL;

		Assert( this != pLock->m_pJobWaitingQueueTail );
	}
	else
	{
		// just release
		UnsetLock( pLock );
		Assert( NULL == pLock->m_pJobWaitingQueueTail || this == pLock->m_pJobWaitingQueueTail );
		pLock->m_pJobWaitingQueueTail = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Release all locks this job holds. This is only to be used by long lived
// jobs that don't destruct.
//-----------------------------------------------------------------------------
void CJob::ReleaseLocks()
{
	// release any locks - do this in reverse order because they're being removed from the vector in the loop
	FOR_EACH_VEC_BACK( m_vecLocks, nLock )
	{
		_ReleaseLock( m_vecLocks[nLock], true, __FILE__, __LINE__ );
	}
	m_vecLocks.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Assert that we don't hold any locks, and if we hold them, release them
//-----------------------------------------------------------------------------
void CJob::ShouldNotHoldAnyLocks()
{
	if ( m_vecLocks.Count() == 0 )
		return;

	CUtlString sErrMsg;
	sErrMsg.Format( "Job %s detected and cleaned up leak of %d lock(s):\n", GetName(), m_vecLocks.Count() );
	FOR_EACH_VEC_BACK( m_vecLocks, nLock )
	{
		CLock *pLock = m_vecLocks[nLock];
		sErrMsg.Append( CFmtStr( "    Lock %s, acquired %s(%d)\n", pLock->GetName(), pLock->m_pFilename, pLock->m_line ).Access() );
	}

	AssertMsg1( false, "%s", sErrMsg.String() );

	// Now release them
	ReleaseLocks();
}


//-----------------------------------------------------------------------------
// Purpose: sets up the job to notify when we've release our locks
//-----------------------------------------------------------------------------
void CJob::AddJobToNotifyOnLockRelease( CJob *pJob )
{
	// if we already are going to be notifying someone, then have them notify the new requester
	if ( m_pJobToNotifyOnLockRelease )
	{
		AssertMsg( false, "AddJobToNotifyOnLockRelease attempting to walk the linked list. We've optimized this out." );
		m_pJobToNotifyOnLockRelease->AddJobToNotifyOnLockRelease( pJob );
	}
	else
	{
		m_pJobToNotifyOnLockRelease = pJob;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the lock
//-----------------------------------------------------------------------------
void CJob::_SetLock( CLock *pLock, const char *filename, int line )
{
	Assert( !m_vecLocks.HasElement( pLock ) );
	Assert( !pLock->BIsLocked() );

	pLock->m_pJob = this;
	pLock->m_sTimeAcquired.SetToJobTime();
	pLock->m_pFilename = filename;
	pLock->m_line = line;
	m_vecLocks.AddToTail( pLock );
}


//-----------------------------------------------------------------------------
// Purpose: Removes the lock
//-----------------------------------------------------------------------------
void CJob::UnsetLock( CLock *pLock )
{
	Assert( pLock->GetJobLocking() == this );

	pLock->m_pJob = NULL;
	// if we've held the lock for more than a few seconds, make noise.
	if ( /*!BIsTest() &&*/ pLock->m_sTimeAcquired.CServerMicroSecsPassed() >= 10 * k_nMillion ) 
	{
		m_flags.m_bits.m_bLocksLongHeld = true;
		if ( pLock->m_pJobToNotifyOnLockRelease )
		{
			pLock->m_pJobToNotifyOnLockRelease->m_flags.m_bits.m_bLocksLongWait = true;
			EmitWarning( SPEW_JOB, 4, "Job of type %s held lock for %.2f seconds while job of type %s was waiting\n", GetName(), (double) pLock->m_sTimeAcquired.CServerMicroSecsPassed() / k_nMillion, pLock->m_pJobToNotifyOnLockRelease->GetName() );
		}
		else
			EmitWarning( SPEW_JOB, 4, "Job of type %s held lock for %.2f seconds\n", GetName(), (double) pLock->m_sTimeAcquired.CServerMicroSecsPassed() / k_nMillion );
	}
	m_vecLocks.FindAndRemove( pLock );
}


//-----------------------------------------------------------------------------
// Purpose: Releases the lock from the old job, and immediately passes it on to the waiting job
//-----------------------------------------------------------------------------
void CJob::PassLockToJob( CJob *pNewJob, CLock *pLock )
{
	Assert( pNewJob->GetPauseReason() == k_EJobPauseReasonWaitingForLock );
	Assert( pNewJob->m_pWaitingOnLock == pLock );

	pLock->m_pJobToNotifyOnLockRelease = pNewJob->m_pJobToNotifyOnLockRelease;
	if ( NULL == pLock->m_pJobToNotifyOnLockRelease )
	{
		pLock->m_pJobWaitingQueueTail = NULL;
	}

	pNewJob->m_pJobToNotifyOnLockRelease = NULL;
	Assert( pLock->m_nWaitingCount > 0 );
	pLock->m_nWaitingCount--;

	// release the lock
	UnsetLock( pLock );
	
	// If the other job isn't waiting on a lock, then we certainly don't
	// want to call SetLock() on it
	if ( pNewJob->GetPauseReason() == k_EJobPauseReasonWaitingForLock && pNewJob->m_pWaitingOnLock == pLock )
	{
		// give the job the lock
		pNewJob->_SetLock( pLock, pNewJob->m_pWaitingOnLockFilename, m_waitingOnLockLine );

		// set the job with the newly acquired lock to wakeup
		pNewJob->GetJobMgr().WakeupLockedJob( *pNewJob );
	}
	else
	{
		EmitError( SPEW_JOB, "Job passed lock it wasn't waiting for. Job: %s, Lock: %s %s(%d), Paused for %s, Waiting on %s\n", 
			pNewJob->GetName(), pLock->GetName(), pLock->m_pFilename, pLock->m_line, pNewJob->GetPauseReasonDescription(), m_pWaitingOnLock ? m_pWaitingOnLock->GetName() : "none" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: a lock is letting us know it's been deleted
//			fail all jobs trying to get the lock
//-----------------------------------------------------------------------------
void CJob::OnLockDeleted( CLock *pLock )
{
	//EmitWarning( SPEW_JOB, SPEW_ALWAYS, "Deleting lock %s\n", GetName() );

	Assert( pLock->BIsLocked() );
	Assert( pLock->m_pJob == this );

	// fail all the jobs waiting on the lock
	CJob *pJob = pLock->m_pJobToNotifyOnLockRelease;
	while ( pJob )
	{
		// insert the job into the sleep list with 0 time, so it wakes up immediately
		// it will see it doesn't have the desired lock and suicide
		pJob->GetJobMgr().WakeupLockedJob( *pJob );

		// move to the next job
		CJob *pJobT = pJob;
		pJob = pJob->m_pJobToNotifyOnLockRelease;
		pJobT->m_pJobToNotifyOnLockRelease = NULL;
	}

	m_pJobToNotifyOnLockRelease = NULL;
	pLock->m_pJobToNotifyOnLockRelease = NULL;
	pLock->m_pJobWaitingQueueTail = NULL;

	// remove the lock
	UnsetLock( pLock );
}

//-----------------------------------------------------------------------------
// Purpose: Reports how many Do Not Yield guards the job currently has
//-----------------------------------------------------------------------------
int32 CJob::GetDoNotYieldDepth() const
{
	return m_stackDoNotYieldGuards.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a Do Not Yield guard to the job
//-----------------------------------------------------------------------------
void CJob::PushDoNotYield( const char *pchFileAndLine )
{
	m_stackDoNotYieldGuards.AddToHead( pchFileAndLine );
}


//-----------------------------------------------------------------------------
// Purpose: Removes the last-added Do Not Yield guard from the job
//-----------------------------------------------------------------------------
void CJob::PopDoNotYield()
{
	AssertMsg( m_stackDoNotYieldGuards.Count() > 0, "Could not pop a Do Not Yield guard when the job's stack is empty" );
	if ( m_stackDoNotYieldGuards.Count() > 0 )
	{
		m_stackDoNotYieldGuards.Remove( m_stackDoNotYieldGuards.Head() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Implementation of the stack-scope Do Not Yield guard
//-----------------------------------------------------------------------------
CDoNotYieldScope::CDoNotYieldScope( const char *pchFileAndLine )
{
	AssertRunningJob();

	GJobCur().PushDoNotYield( pchFileAndLine );
}

CDoNotYieldScope::~CDoNotYieldScope()
{
	AssertRunningJob();

	GJobCur().PopDoNotYield();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CJob::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_stackDoNotYieldGuards );
}


//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our static data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CJob::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "CJob class statics" );
}
#endif // DBGFLAG_VALIDATE


CLock::CLock( ) 
: m_pJob( NULL ), 
m_pJobToNotifyOnLockRelease( NULL ), 
m_pJobWaitingQueueTail( NULL ), 
m_nWaitingCount(0),
m_nsLockType(0),
m_nsNameType( k_ENameTypeNone ),
m_ulID( 0 ),
m_pchConstStr( NULL ),
m_unLockSubType ( 0 ),
m_nRefCount( 0 ),
m_pFilename( "unknown" ),
m_line( 0 )
{ 
}


CLock::~CLock()
{
	if ( m_pJob )
	{
		m_pJob->OnLockDeleted( this );
	}
}


void CLock::AddToWaitingQueue( CJob *pJob )
{
	if ( m_pJobWaitingQueueTail )
	{
		Assert( NULL == m_pJobWaitingQueueTail->m_pJobToNotifyOnLockRelease );
		m_pJobWaitingQueueTail->AddJobToNotifyOnLockRelease( pJob );
	}
	else
	{
		Assert( NULL == m_pJobToNotifyOnLockRelease );
		m_pJobToNotifyOnLockRelease = pJob;
	}
	
	m_pJobWaitingQueueTail = pJob;
	m_nWaitingCount++;
}


void CLock::SetName( const char *pchName )
{
	m_nsNameType = k_ENameTypeConstStr;
	m_pchConstStr = pchName;
}


void CLock::SetName( const char *pchPrefix, uint64 ulID )
{
	m_nsNameType = k_ENameTypeConcat;
	m_pchConstStr = pchPrefix;
	m_ulID = ulID;
}


void CLock::SetName( const CSteamID &steamID )
{
	m_nsNameType = k_ENameTypeSteamID;
	m_ulID = steamID.ConvertToUint64();
}


const char *CLock::GetName() const
{
	switch ( m_nsNameType )
	{
	case k_ENameTypeNone:
		return "None";
	case k_ENameTypeSteamID:
		return CSteamID::Render( m_ulID );
	case k_ENameTypeConstStr:
		return m_pchConstStr;
	case k_ENameTypeConcat:
		if ( !m_strName.Length() )
			m_strName.Format( "%s %llu", m_pchConstStr, m_ulID );
		return m_strName.Get();
	default:
		AssertMsg1( false, "Invalid lock name type %d", m_nsNameType );
		return "(Unknown)";
	}
}

#define REF_COUNT_ASSERT 1000

void CLock::IncrementReference()
{
	m_nRefCount++;
	Assert( m_nRefCount != REF_COUNT_ASSERT );
}

int CLock::DecrementReference()
{
	Assert( m_nRefCount > 0 );
	if ( m_nRefCount > 0 )
	{
		m_nRefCount--;
	}
	return m_nRefCount;
}

void CLock::Dump( const char *pszPrefix, int nPrintMax, bool bPrintWaiting ) const
{
	if ( m_pJob != NULL )
	{
		EmitInfo( SPEW_JOB, SPEW_ALWAYS, LOG_ALWAYS, "%s%s: Lock owner: %s, Type: %d, %d Waiting\n", pszPrefix, GetName(), CFmtStr( "%s (%llu), Reason: %s", m_pJob->GetName(), m_pJob->GetJobID(), m_pJob->GetPauseReasonDescription() ).Access(), (int32)m_nsLockType, m_nWaitingCount );
		EmitInfo( SPEW_JOB, SPEW_ALWAYS, LOG_ALWAYS, "%sLock acquired: %s:%d\n", pszPrefix, m_pFilename, m_line );
	}
	else
	{
		EmitInfo( SPEW_JOB, SPEW_ALWAYS, LOG_ALWAYS, "%s%s: Lock owner: None, Type: %d, %d Waiting\n", pszPrefix, GetName(), (int32)m_nsLockType, m_nWaitingCount );
	}

	CJob *pCurrWaiting = m_pJobToNotifyOnLockRelease;
	int nPrinted = 0;
	int nTotal = 0;
	while( pCurrWaiting != NULL && nPrinted < nPrintMax )
	{
		bool bPrint = false;
		if ( nPrinted < nPrintMax )
		{
			bPrint = true;
		}
		if ( pCurrWaiting->GetPauseReason() == k_EJobPauseReasonWaitingForLock && pCurrWaiting->m_pWaitingOnLock == this )
		{
			bPrint = true;
		}

		if ( bPrint && bPrintWaiting )
		{
			EmitInfo( SPEW_JOB, SPEW_ALWAYS, LOG_ALWAYS, "%s\tOther jobs waiting for this lock: %s (%llu)\n", pszPrefix, pCurrWaiting->GetName(), pCurrWaiting->GetJobID() );
			if ( pCurrWaiting->GetPauseReason() == k_EJobPauseReasonWaitingForLock && pCurrWaiting->m_pWaitingOnLock == this )
			{
				EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, 3, "%s\tAt: %s:%d\n", pszPrefix, pCurrWaiting->m_pWaitingOnLockFilename, pCurrWaiting->m_waitingOnLockLine );
			}
			nPrinted++;
		}
		pCurrWaiting = pCurrWaiting->m_pJobToNotifyOnLockRelease;
		nTotal++;
	}
	if ( bPrintWaiting || nTotal != 0 )
	{
		EmitInfo( SPEW_JOB, SPEW_ALWAYS, LOG_ALWAYS, "%s%d out of %d waiting jobs printed.\n", pszPrefix, nPrinted, nTotal );
	}
}

} // namespace GCSDK
