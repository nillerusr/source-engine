//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#if defined( _WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "tier0/dbg.h"
#include "tier0/tslist.h"
#include "tier0/icommandline.h"
#include "vstdlib/jobthread.h"
#include "vstdlib/random.h"
#include "tier1/functors.h"
#include "tier1/fmtstr.h"
#include "tier1/utlvector.h"
#include "tier1/generichash.h"
#include "tier0/vprof.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include "tier0/memdbgon.h"


class CJobThread;

//-----------------------------------------------------------------------------

inline void ServiceJobAndRelease( CJob *pJob, int iThread = -1 )
{
	// TryLock() would only fail if another thread has entered
	// Execute() or Abort()
	if ( !pJob->IsFinished() && pJob->TryLock() )
	{
		// ...service the request
		pJob->SetServiceThread( iThread );
		pJob->Execute();
		pJob->Unlock();
	}
	pJob->Release();
}

//-----------------------------------------------------------------------------

class ALIGN16 CJobQueue
{
public:
	CJobQueue() :
		m_nItems( 0 ),
		m_nMaxItems( INT_MAX )
	{
		for ( int i = 0; i < ARRAYSIZE( m_pQueues ); i++ )
		{
			m_pQueues[i] = new CTSQueue<CJob *>;
		}
	}

	~CJobQueue()
	{
		for ( int i = 0; i < ARRAYSIZE( m_pQueues ); i++ )
		{
			delete m_pQueues[i];
		}
	}

	int Count()
	{
		return m_nItems;
	}

	int Count( JobPriority_t priority )
	{
		return m_pQueues[priority]->Count();
	}


	CJob *PrePush()
	{
		if ( m_nItems >= m_nMaxItems )
		{
			CJob *pOverflowJob;
			if ( Pop( &pOverflowJob ) )
			{
				return pOverflowJob;
			}
		}
		return NULL;
	}

	int Push( CJob *pJob, int iThread = -1 )
	{
		pJob->AddRef();

		CJob *pOverflowJob;
		int nOverflow = 0;
		while ( ( pOverflowJob = PrePush() ) != NULL )
		{
			ServiceJobAndRelease( pJob );
			nOverflow++;
		}

		m_pQueues[pJob->GetPriority()]->PushItem( pJob );

		m_mutex.Lock();
		if ( ++m_nItems == 1 )
		{
			m_JobAvailableEvent.Set();
		}
		m_mutex.Unlock();

		return nOverflow;
	}

	bool Pop( CJob **ppJob )
	{
		m_mutex.Lock();
		if ( !m_nItems )
		{
			m_mutex.Unlock();
			*ppJob = NULL;
			return false;
		}
		if ( --m_nItems == 0 )
		{
			m_JobAvailableEvent.Reset();
		}
		m_mutex.Unlock();

		for ( int i = JP_HIGH; i >= 0; --i )
		{
			if ( m_pQueues[i]->PopItem( ppJob ) )
			{
				return true;
			}
		}


		AssertMsg( 0, "Expected at least one queue item" );
		*ppJob = NULL;
		return false;
	}

	CThreadEvent &GetEventHandle()
	{
		return m_JobAvailableEvent;
	}

	void Flush()
	{
		// Only safe to call when system is suspended
		m_mutex.Lock();
		m_nItems = 0;
		m_JobAvailableEvent.Reset();
		CJob *pJob;
		for ( int i = JP_HIGH; i >= 0; --i )
		{
			while ( m_pQueues[i]->PopItem( &pJob ) )
			{
				pJob->Abort();
				pJob->Release();
			}
		}
		m_mutex.Unlock();
	}

private:
	CTSQueue<CJob *>	*m_pQueues[JP_HIGH + 1];
	int					m_nItems;
	int					m_nMaxItems;
	CThreadMutex		m_mutex;
	CThreadManualEvent	m_JobAvailableEvent;

} ALIGN16_POST;

//-----------------------------------------------------------------------------
//
// CThreadPool
//
//-----------------------------------------------------------------------------

class CThreadPool : public CRefCounted1<IThreadPool, CRefCountServiceMT>
{
public:
	CThreadPool();
	~CThreadPool();

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	bool Start( const ThreadPoolStartParams_t &startParams = ThreadPoolStartParams_t() ) { return Start( startParams, NULL ); }
	bool Start( const ThreadPoolStartParams_t &startParams, const char *pszNameOverride );
	bool Stop( int timeout = TT_INFINITE );
	void Distribute( bool bDistribute = true, int *pAffinityTable = NULL );

	//-----------------------------------------------------
	// Functions for any thread
	//-----------------------------------------------------
	unsigned GetJobCount()							{ return m_nJobs; }
	int NumThreads();
	int NumIdleThreads();

	//-----------------------------------------------------
	// Pause/resume processing jobs
	//-----------------------------------------------------
	int SuspendExecution();
	int ResumeExecution();

	//-----------------------------------------------------
	// Offer the current thread to the pool
	//-----------------------------------------------------
	virtual int YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
	virtual int YieldWait( CJob **, int nJobs, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
	void Yield( unsigned timeout );

	//-----------------------------------------------------
	// Add a native job to the queue (master thread)
	//-----------------------------------------------------
	void AddJob( CJob * );
	void InsertJobInQueue( CJob * );

	//-----------------------------------------------------
	// All threads execute pFunctor asap. Thread will either wake up
	//  and execute or execute pFunctor right after completing current job and
	//  before looking for another job.
	//-----------------------------------------------------
	void ExecuteHighPriorityFunctor( CFunctor *pFunctor );

	//-----------------------------------------------------
	// Add an function object to the queue (master thread)
	//-----------------------------------------------------
	void AddFunctorInternal( CFunctor *, CJob ** = NULL, const char *pszDescription = NULL, unsigned flags = 0 );

	//-----------------------------------------------------
	// Remove a job from the queue (master thread)
	//-----------------------------------------------------
	virtual void ChangePriority( CJob *p, JobPriority_t priority );

	//-----------------------------------------------------
	// Bulk job manipulation (blocking)
	//-----------------------------------------------------
	int ExecuteToPriority( JobPriority_t toPriority, JobFilter_t pfnFilter = NULL  );
	int AbortAll();

	virtual void Reserved1() {}

	void WaitForIdle( bool bAll = true );

private:
	enum
	{
		IO_STACKSIZE = ( 64 * 1024 ),
		COMPUTATION_STACKSIZE = 0,
	};

	//-----------------------------------------------------
	//
	//-----------------------------------------------------
	CJob *PeekJob();
	CJob *GetDummyJob();

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	int Run();

private:
	friend class CJobThread;

	CJobQueue				m_SharedQueue;
	CInterlockedInt			m_nIdleThreads;
	CUtlVector<CJobThread *> m_Threads;
	CUtlVector<CThreadEvent *>		m_IdleEvents;

	CThreadMutex			m_SuspendMutex;
	int						m_nSuspend;
	CInterlockedInt			m_nJobs;

	// Some jobs should only be executed on the threadpool thread(s). Ie: the rendering thread has the GL context
	//	and the main thread coming in and "helping" with jobs breaks that pretty nicely. This flag states that
	//	only the threadpool threads should execute these jobs.
	bool					m_bExecOnThreadPoolThreadsOnly;
};

//-----------------------------------------------------------------------------

JOB_INTERFACE IThreadPool *CreateThreadPool()
{
	return new CThreadPool;
}

JOB_INTERFACE void DestroyThreadPool( IThreadPool *pPool )
{
	delete pPool;
}

//-----------------------------------------------------------------------------

class CGlobalThreadPool : public CThreadPool
{
public:
	virtual bool Start( const ThreadPoolStartParams_t &startParamsIn )
	{
		int nThreads = ( CommandLine()->ParmValue( "-threads", -1 ) - 1 );
		ThreadPoolStartParams_t startParams = startParamsIn;

		if ( nThreads >= 0 )
		{
			startParams.nThreads = nThreads;
		}
		else
		{
			// Cap the GlobPool threads at 4.
			startParams.nThreadsMax = 4;
		}
		return CThreadPool::Start( startParams, "Glob" );
	}

	virtual bool OnFinalRelease()
	{
		AssertMsg( 0, "Releasing global thread pool object!" );
		return false;
	}
};

//-----------------------------------------------------------------------------

class CJobThread : public CWorkerThread
{
public:
	CJobThread( CThreadPool *pOwner, int iThread ) : 
		m_SharedQueue( pOwner->m_SharedQueue ),
		m_pOwner( pOwner ),
		m_iThread( iThread )
	{
	}

	CThreadEvent &GetIdleEvent()
	{
		return m_IdleEvent;
	}

	CJobQueue &AccessDirectQueue()
	{ 
		return m_DirectQueue;
	}

private:
	unsigned Wait()
	{
		unsigned waitResult;
		tmZone( TELEMETRY_LEVEL0, TMZF_IDLE, "%s", __FUNCTION__ );
#ifdef WIN32
		enum Event_t
		{
			CALL_FROM_MASTER,
			SHARED_QUEUE,
			DIRECT_QUEUE,

			NUM_EVENTS
		};

		HANDLE	 waitHandles[NUM_EVENTS];
		
		waitHandles[CALL_FROM_MASTER]	= GetCallHandle().GetHandle();
		waitHandles[SHARED_QUEUE]		= m_SharedQueue.GetEventHandle().GetHandle();
		waitHandles[DIRECT_QUEUE] 		= m_DirectQueue.GetEventHandle().GetHandle();
		
#ifdef _DEBUG
		while ( ( waitResult = WaitForMultipleObjects( ARRAYSIZE(waitHandles), waitHandles, FALSE, 10 ) ) == WAIT_TIMEOUT )
		{
			waitResult = waitResult; // break here
		}
#else
		waitResult = WaitForMultipleObjects( ARRAYSIZE(waitHandles), waitHandles, FALSE, INFINITE );
#endif
#else // !win32
		bool bSet = false;
		int nWaitTime = 100;

		while( !bSet )
		{
			// Jobs are typically enqueued to the shared job queue so wait on it first.
			bSet = m_SharedQueue.GetEventHandle().Wait( nWaitTime );
			if( !bSet )
				bSet = m_DirectQueue.GetEventHandle().Wait( 10 );
			if ( !bSet )
				bSet = GetCallHandle().Wait( 0 );
		}

		if ( !bSet )
			waitResult = WAIT_TIMEOUT;
		else
			waitResult = WAIT_OBJECT_0;
#endif
		return waitResult;
	}

	int Run()
	{


		// Wait for either a call from the master thread, or an item in the queue...
		unsigned waitResult;
		bool	 bExit = false;

		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		m_pOwner->m_nIdleThreads++;
		m_IdleEvent.Set();
		while (!bExit && ( ( waitResult = Wait() ) != WAIT_FAILED ) )
		{
			if ( PeekCall() )
			{
				CFunctor *pFunctor = NULL;
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s PeekCall():%d", __FUNCTION__, GetCallParam() );

				switch ( GetCallParam( &pFunctor ) )
				{
				case TPM_EXIT:
					Reply( true );
					bExit = TRUE;
					break;

				case TPM_SUSPEND:
					Reply( true );
					SuspendCooperative();
					break;

				case TPM_RUNFUNCTOR:
					if( pFunctor )
					{
						( *pFunctor )();
						Reply( true );
					}
					else
					{
						Assert( pFunctor );
						Reply( false );
					}
					break;

				default:
					AssertMsg( 0, "Unknown call to thread" );
					Reply( false );
					break;
				}
			}
			else
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s !PeekCall()", __FUNCTION__ );

				CJob *pJob;
				bool bTookJob = false;
				do
				{
					if ( !m_DirectQueue.Pop( &pJob) )
					{
						if ( !m_SharedQueue.Pop( &pJob ) )
						{
							// Nothing to process, return to wait state
							break;
						}
					}
					if ( !bTookJob )
					{
						m_IdleEvent.Reset();
						m_pOwner->m_nIdleThreads--;
						bTookJob = true;
					}
					ServiceJobAndRelease( pJob, m_iThread );
					m_pOwner->m_nJobs--;
				} while ( !PeekCall() );

				if ( bTookJob )
				{
					m_pOwner->m_nIdleThreads++;
					m_IdleEvent.Set();
				}
			}
		}
		m_pOwner->m_nIdleThreads--;
		m_IdleEvent.Reset();
		return 0;
	}

	CJobQueue			m_DirectQueue;
	CJobQueue &			m_SharedQueue;
	CThreadPool *		m_pOwner;
	CThreadManualEvent	m_IdleEvent;
	int					m_iThread;
};

//-----------------------------------------------------------------------------

CGlobalThreadPool g_ThreadPool;
IThreadPool *g_pThreadPool = &g_ThreadPool;

//-----------------------------------------------------------------------------
//
// CThreadPool
//
//-----------------------------------------------------------------------------

CThreadPool::CThreadPool() :
	m_nIdleThreads( 0 ),
	m_nJobs( 0 ),
	m_nSuspend( 0 )
{
}

//---------------------------------------------------------

CThreadPool::~CThreadPool()
{
	Stop();
}

//---------------------------------------------------------
// 
//---------------------------------------------------------
int CThreadPool::NumThreads()
{
	return m_Threads.Count();
}

//---------------------------------------------------------
// 
//---------------------------------------------------------
int CThreadPool::NumIdleThreads()
{
	return m_nIdleThreads;
}

void CThreadPool::ExecuteHighPriorityFunctor( CFunctor *pFunctor )
{
	int i;
	for ( i = 0; i < m_Threads.Count(); i++ )
	{
		m_Threads[i]->CallWorker( TPM_RUNFUNCTOR, 0, false, pFunctor );
	}

	for ( i = 0; i < m_Threads.Count(); i++ )
	{
		m_Threads[i]->WaitForReply();
	}
}

//---------------------------------------------------------
// Pause/resume processing jobs
//---------------------------------------------------------
int CThreadPool::SuspendExecution()
{
	AUTO_LOCK( m_SuspendMutex );

	// If not already suspended
	if ( m_nSuspend == 0 )
	{
		// Make sure state is correct
		int i;
		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->CallWorker( TPM_SUSPEND, 0 );
		}

		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->WaitForReply();
		}

		// Because worker must signal before suspending, we could reach
		// here with the thread not actually suspended
		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->BWaitForThreadSuspendCooperative();
		}
	}

	return m_nSuspend++;
}

//---------------------------------------------------------

int CThreadPool::ResumeExecution()
{
	AUTO_LOCK( m_SuspendMutex );
	AssertMsg( m_nSuspend >= 1, "Attempted resume when not suspended");
	int result = m_nSuspend--;
	if (m_nSuspend == 0 )
	{
		for ( int i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->ResumeCooperative();
		}
	}
	return result;
}

//---------------------------------------------------------

void CThreadPool::WaitForIdle( bool bAll )
{
	ThreadWaitForEvents( m_IdleEvents.Count(), m_IdleEvents.Base(), bAll, 60000 );
}

//---------------------------------------------------------

int CThreadPool::YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll, unsigned timeout )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_IDLE, "%s(%d) SPINNING %t", __FUNCTION__, timeout, tmSendCallStack( TELEMETRY_LEVEL0, 0 ) );

	Assert( timeout == TT_INFINITE ); // unimplemented

	int result;
	CJob *pJob;
	// Always wait for zero milliseconds initially, to let us process jobs on this thread.
	timeout = 0;
	while ( ( result = ThreadWaitForEvents( nEvents, pEvents, bWaitAll, timeout ) ) == WAIT_TIMEOUT )
	{
		if ( !m_bExecOnThreadPoolThreadsOnly && m_SharedQueue.Pop( &pJob ) )
		{
			ServiceJobAndRelease( pJob );
			m_nJobs--;
		}
		else
		{
			// Since there are no jobs for the main thread set the timeout to infinite.
			// The only disadvantage to this is that if a job thread creates a new job
			// then the main thread will not be available to pick it up, but if that
			// is a problem you can just create more worker threads. Debugging test runs
			// of TF2 suggests that jobs are only ever added from the main thread which
			// means that there is no disadvantage.
			// Waiting on the events instead of busy spinning has multiple advantages.
			// It avoids wasting CPU time/electricity, it makes it more obvious in profiles
			// when the main thread is idle versus busy, and it allows ready thread analysis
			// in xperf to find out what woke up a waiting thread.
			// It also avoids unnecessary CPU starvation -- seen on customer traces of TF2.
			timeout = TT_INFINITE;
		}
	}
	return result;
}

//---------------------------------------------------------

int CThreadPool::YieldWait( CJob **ppJobs, int nJobs, bool bWaitAll, unsigned timeout )
{
	CUtlVectorFixed<CThreadEvent *, 64> handles;
	if ( nJobs > handles.NumAllocated() - 2 )
	{
		return TW_FAILED;
	}

	for ( int i = 0; i < nJobs; i++ )
	{
		handles.AddToTail( ppJobs[i]->AccessEvent() );
	}

	return YieldWait( handles.Base(), handles.Count(), bWaitAll, timeout);
}

//---------------------------------------------------------

void CThreadPool::Yield( unsigned timeout )
{
	// @MULTICORE (toml 10/24/2006): not implemented
	Assert( ThreadInMainThread() );
	if ( !ThreadInMainThread() )
	{
		ThreadSleep( timeout );
		return;
	}
	ThreadSleep( timeout );
}

//---------------------------------------------------------
// Add a job to the queue
//---------------------------------------------------------

void CThreadPool::AddJob( CJob *pJob )
{
	if ( !pJob )
	{
		return;
	}

	if ( pJob->m_ThreadPoolData != JOB_NO_DATA )
	{
		Warning( "Cannot add a thread job already committed to another thread pool\n" );
		return;
	}

	if ( m_Threads.Count() == 0 )
	{
		// So only threadpool jobs are supposed to execute the jobs, but there are no threadpool threads?
		Assert( !m_bExecOnThreadPoolThreadsOnly );

		pJob->Execute();
		return;
	}

	int flags = pJob->GetFlags();

	if ( !m_bExecOnThreadPoolThreadsOnly && ( ( flags & ( JF_IO | JF_QUEUE ) ) == 0 ) /* @TBD && !m_queue.Count() */ )
	{
		if ( !NumIdleThreads() )
		{
			pJob->Execute();
			return;
		}
		pJob->SetPriority( JP_HIGH );
	}


	if ( !pJob->CanExecute() )
	{
		// Already handled
		ExecuteOnce( Warning( "Attempted to add job to job queue that has already been completed\n" ) );
		return;
	}

	pJob->m_pThreadPool = this;
	pJob->m_status = JOB_STATUS_PENDING;
	InsertJobInQueue( pJob );
	++m_nJobs;
}

//---------------------------------------------------------
//
//---------------------------------------------------------

void CThreadPool::InsertJobInQueue( CJob *pJob )
{
	CJobQueue *pQueue;

	if ( !( pJob->GetFlags() & JF_SERIAL ) )
	{
		int iThread = pJob->GetServiceThread();
		if ( iThread == -1 || !m_Threads.IsValidIndex( iThread ) )
		{
			pQueue = &m_SharedQueue;
		}
		else
		{
			pQueue = &(m_Threads[iThread]->AccessDirectQueue());
		}
	}
	else
	{
		pQueue = &(m_Threads[0]->AccessDirectQueue());
	}

	m_nJobs -= pQueue->Push( pJob );
}

//---------------------------------------------------------
// Add an function object to the queue (master thread)
//---------------------------------------------------------

void CThreadPool::AddFunctorInternal( CFunctor *pFunctor, CJob **ppJob, const char *pszDescription, unsigned flags )
{
	// Note: assumes caller has handled refcount
	CJob *pJob = new CFunctorJob( pFunctor, pszDescription );

	pJob->SetFlags( flags );

	AddJob( pJob );

	if ( ppJob )
	{
		*ppJob = pJob;
	}
	else
	{
		pJob->Release();
	}
}

//---------------------------------------------------------
// Remove a job from the queue
//---------------------------------------------------------

void CThreadPool::ChangePriority( CJob *pJob, JobPriority_t priority )
{
	// Right now, only support upping the priority
	if ( pJob->GetPriority() < priority )
	{
		pJob->SetPriority( priority );
		m_SharedQueue.Push( pJob );
	}
	else
	{
		ExecuteOnce( if ( pJob->GetPriority() != priority ) DevMsg( "CThreadPool::RemoveJob not implemented right now" ) );
	}

}

//---------------------------------------------------------
// Execute to a specified priority
//---------------------------------------------------------

int CThreadPool::ExecuteToPriority( JobPriority_t iToPriority, JobFilter_t pfnFilter )
{
	SuspendExecution();

	CJob *pJob;
	int nExecuted = 0;
	int i;
	int nJobsTotal = GetJobCount();
	CUtlVector<CJob *> jobsToPutBack;

	for ( int iCurPriority = JP_HIGH; iCurPriority >= iToPriority; --iCurPriority )
	{
		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			CJobQueue &queue = m_Threads[i]->AccessDirectQueue();
			while ( queue.Count( (JobPriority_t)iCurPriority ) )
			{
				queue.Pop( &pJob );
				if ( pfnFilter && !(*pfnFilter)( pJob ) )
				{
					if ( pJob->CanExecute() )
					{
						jobsToPutBack.EnsureCapacity( nJobsTotal );
						jobsToPutBack.AddToTail( pJob );
					}
					else
					{
						m_nJobs--;
						pJob->Release(); // an already serviced job in queue, may as well ditch it (as in, main thread probably force executed)
					}
					continue;
				}
				ServiceJobAndRelease( pJob );
				m_nJobs--;
				nExecuted++;
			}

		}

		while ( m_SharedQueue.Count( (JobPriority_t)iCurPriority ) )
		{
			m_SharedQueue.Pop( &pJob );
			if ( pfnFilter && !(*pfnFilter)( pJob ) )
			{
				if ( pJob->CanExecute() )
				{
					jobsToPutBack.EnsureCapacity( nJobsTotal );
					jobsToPutBack.AddToTail( pJob );
				}
				else
				{
					m_nJobs--;
					pJob->Release(); // see above
				}
				continue;
			}

			ServiceJobAndRelease( pJob );
			m_nJobs--;
			nExecuted++;
		}
	}

	for ( i = 0; i < jobsToPutBack.Count(); i++ )
	{
		InsertJobInQueue( jobsToPutBack[i] );
		jobsToPutBack[i]->Release();
	}

	ResumeExecution();

	return nExecuted;
}

//---------------------------------------------------------
//
//---------------------------------------------------------

int CThreadPool::AbortAll()
{
	SuspendExecution();
	CJob *pJob;

	int iAborted = 0;
	while ( m_SharedQueue.Pop( &pJob ) )
	{
		pJob->Abort();
		pJob->Release();
		iAborted++;
	}

	for ( int i = 0; i < m_Threads.Count(); i++ )
	{
		CJobQueue &queue = m_Threads[i]->AccessDirectQueue();
		while ( queue.Pop( &pJob ) )
		{
			pJob->Abort();
			pJob->Release();
			iAborted++;
		}

	}

	m_nJobs = 0;

	ResumeExecution();

	return iAborted;
}

//---------------------------------------------------------
// CThreadPool thread functions
//---------------------------------------------------------

bool CThreadPool::Start( const ThreadPoolStartParams_t &startParams, const char *pszName )
{
	int nThreads = startParams.nThreads;

	m_bExecOnThreadPoolThreadsOnly = startParams.bExecOnThreadPoolThreadsOnly;

	if ( nThreads < 0 )
	{
		const CPUInformation &ci = *GetCPUInformation();
		if ( startParams.bIOThreads )
		{
			nThreads = ci.m_nLogicalProcessors;
		}
		else
		{
			nThreads = ( ci.m_nLogicalProcessors / (( ci.m_bHT ) ? 2 : 1) ) - 1; // One per
			if ( IsPC() )
			{
				if ( nThreads > 3 )
				{
					DevMsg( "Defaulting to limit of 3 worker threads, use -threads on command line if want more\n" ); // Current >4 processor configs don't really work so well, probably due to cache issues? (toml 7/12/2007)
					nThreads = 3;
				}
			}
		}

		if ( ( startParams.nThreadsMax >= 0 ) && ( nThreads > startParams.nThreadsMax ) )
		{
			nThreads = startParams.nThreadsMax;
		}
	}

	if ( nThreads <= 0 )
	{
		return true;
	}

	int nStackSize = startParams.nStackSize;

	if ( nStackSize < 0 )
	{
		if ( startParams.bIOThreads )
		{
			nStackSize = IO_STACKSIZE;
		}
		else
		{
			nStackSize = COMPUTATION_STACKSIZE;
		}
	}

	int priority = startParams.iThreadPriority;

	if ( priority == SHRT_MIN )
	{
		if ( startParams.bIOThreads )
		{
			priority = THREAD_PRIORITY_HIGHEST;
		}
		else
		{
			priority = ThreadGetPriority();
		}
	}

	bool bDistribute;
	if ( startParams.fDistribute != TRS_NONE )
	{
		bDistribute = ( startParams.fDistribute == TRS_TRUE );
	}
	else
	{
		bDistribute = !startParams.bIOThreads;
	}

	//--------------------------------------------------------

	m_Threads.EnsureCapacity( nThreads );
	m_IdleEvents.EnsureCapacity( nThreads );

	if ( !pszName )
	{
		pszName = ( startParams.bIOThreads ) ? "IOJobX" : "CmpJobX";
	}
	while ( nThreads-- )
	{
		int iThread = m_Threads.AddToTail();
		m_IdleEvents.AddToTail();
		m_Threads[iThread] = new CJobThread( this, iThread );
		m_IdleEvents[iThread] = &m_Threads[iThread]->GetIdleEvent();
		m_Threads[iThread]->SetName( CFmtStr( "%s%d", pszName, iThread ) );
		m_Threads[iThread]->Start( nStackSize );
		m_Threads[iThread]->GetIdleEvent().Wait();
#ifdef WIN32
		ThreadSetPriority( (ThreadHandle_t)m_Threads[iThread]->GetThreadHandle(), priority );
#endif
	}

	Distribute( bDistribute, startParams.bUseAffinityTable ? (int *)startParams.iAffinityTable : NULL );

	return true;
}

//---------------------------------------------------------

void CThreadPool::Distribute( bool bDistribute, int *pAffinityTable )
{
	if ( bDistribute )
	{
		const CPUInformation &ci = *GetCPUInformation();
		int nHwThreadsPer = (( ci.m_bHT ) ? 2 : 1);
		if ( ci.m_nLogicalProcessors > 1 )
		{
			if ( !pAffinityTable )
			{
#if defined( IS_WINDOWS_PC )
				// no affinity table, distribution is cycled across all available
				HINSTANCE hInst = LoadLibrary( "kernel32.dll" );
				if ( hInst )
				{
					typedef DWORD (WINAPI *SetThreadIdealProcessorFn)(ThreadHandle_t hThread, DWORD dwIdealProcessor);
					SetThreadIdealProcessorFn Thread_SetIdealProcessor = (SetThreadIdealProcessorFn)GetProcAddress( hInst, "SetThreadIdealProcessor" );
					if ( Thread_SetIdealProcessor )
					{
						ThreadHandle_t hMainThread = ThreadGetCurrentHandle();
						Thread_SetIdealProcessor( hMainThread, 0 );
						int iProc = 0;
						for ( int i = 0; i < m_Threads.Count(); i++ )
						{
							iProc += nHwThreadsPer;
							if ( iProc >= ci.m_nLogicalProcessors )
							{
								iProc %= ci.m_nLogicalProcessors;
								if ( nHwThreadsPer > 1 )
								{
									iProc = ( iProc + 1 ) % nHwThreadsPer;
								}
							}
							Thread_SetIdealProcessor((ThreadHandle_t)m_Threads[i]->GetThreadHandle(), iProc);
						}
					}
					FreeLibrary( hInst );
				}
#else
				// no affinity table, distribution is cycled across all available
				int iProc = 0;
				for ( int i = 0; i < m_Threads.Count(); i++ )
				{
					iProc += nHwThreadsPer;
					if ( iProc >= ci.m_nLogicalProcessors )
					{
						iProc %= ci.m_nLogicalProcessors;
						if ( nHwThreadsPer > 1 )
						{
							iProc = ( iProc + 1 ) % nHwThreadsPer;
						}
					}
#ifdef WIN32
					ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), 1 << iProc );
#endif
				}
#endif
			}
			else
			{
				// distribution is from affinity table
				for ( int i = 0; i < m_Threads.Count(); i++ )
				{
#ifdef WIN32
					ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), pAffinityTable[i] );
#endif
				}
			}
		}
	}
	else
	{
#ifdef WIN32
		DWORD_PTR dwProcessAffinity, dwSystemAffinity;
		if ( GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity ) )
		{
			for ( int i = 0; i < m_Threads.Count(); i++ )
			{
				ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), dwProcessAffinity );
			}
		}
#endif
	}
}

//---------------------------------------------------------

bool CThreadPool::Stop( int timeout )
{
	for ( int i = 0; i < m_Threads.Count(); i++ )
	{
		m_Threads[i]->CallWorker( TPM_EXIT );
	}

	for ( int i = 0; i < m_Threads.Count(); ++i )
	{
		while( m_Threads[i]->IsAlive() )
		{
			ThreadSleep( 0 );
		}
		delete m_Threads[i];
	}

	m_nJobs = 0;
	m_SharedQueue.Flush();
	m_nIdleThreads = 0;
	m_Threads.RemoveAll();
	m_IdleEvents.RemoveAll();

	return true;
}

//---------------------------------------------------------

CJob *CThreadPool::GetDummyJob()
{
	class CDummyJob : public CJob
	{
	public:
		CDummyJob()
		{
			Execute();
		}

		virtual JobStatus_t DoExecute() { return JOB_OK; }
	};

	static CDummyJob dummyJob;

	dummyJob.AddRef();
	return &dummyJob;
}

//-----------------------------------------------------------------------------


namespace ThreadPoolTest 
{
int g_iSleep;

CThreadEvent g_done;
int g_nTotalToComplete;
CThreadPool *g_pTestThreadPool;

class CCountJob : public CJob
{
public:
	virtual JobStatus_t DoExecute()
	{
		m_nCount++;
		ThreadPause();
		if ( g_iSleep >= 0)
			ThreadSleep( g_iSleep );
		if ( bDoWork )
		{
			byte pMemory[1024];
			int i;
			for ( i = 0; i < 1024; i++ )
			{
				pMemory[i] = rand();
			}
			for ( i = 0; i < 50; i++ )
			{
				sqrt( (float)HashBlock( pMemory, 1024 ) + HashBlock( pMemory, 1024 ) + 10.0 );
			}
			bDoWork = false;
		}
		if ( m_nCount == g_nTotalToComplete )
			g_done.Set();
		return 0;
	}

	static CInterlockedInt m_nCount;
	bool bDoWork;
};
CInterlockedInt CCountJob::m_nCount;
int g_nTotalAtFinish;

void Test( bool bDistribute, bool bSleep = true, bool bFinishExecute = false, bool bDoWork = false )
{
	for ( int bInterleavePushPop = 0; bInterleavePushPop < 2; bInterleavePushPop++ )
	{
		for ( g_iSleep = -10; g_iSleep <= 10; g_iSleep += 10 )
		{
			Msg( "ThreadPoolTest:         Testing! Sleep %d, interleave %d \n", g_iSleep, bInterleavePushPop );
			int nMaxThreads = ( IsX360() ) ? 6 : 8;
			int nIncrement = ( IsX360() ) ? 1 : 2;
			for ( int i = 1; i <= nMaxThreads; i += nIncrement )
			{
				CCountJob::m_nCount = 0;
				g_nTotalAtFinish = 0;
				ThreadPoolStartParams_t params;
				params.nThreads = i;
				params.fDistribute = ( bDistribute) ? TRS_TRUE : TRS_FALSE;
				g_pTestThreadPool->Start( params, "Tst" );
				if ( !bInterleavePushPop )
				{
					g_pTestThreadPool->SuspendExecution();
				}

				CCountJob jobs[4000];
				g_nTotalToComplete = ARRAYSIZE(jobs);

				CFastTimer timer, suspendTimer;

				suspendTimer.Start();
				timer.Start();
				for ( int j = 0; j < ARRAYSIZE(jobs); j++ )
				{
					jobs[j].SetFlags( JF_QUEUE );
					jobs[j].bDoWork = bDoWork;
					g_pTestThreadPool->AddJob( &jobs[j] );
					if ( bSleep && j % 16 == 0 )
					{
						ThreadSleep( 0 );
					}
				}
				if ( !bInterleavePushPop )
				{
					g_pTestThreadPool->ResumeExecution();
				}
				if ( bFinishExecute && g_iSleep <= 1 )
				{
					g_done.Wait();
				}
				g_nTotalAtFinish = CCountJob::m_nCount;
				timer.End();
				g_pTestThreadPool->SuspendExecution();
				suspendTimer.End();
				g_pTestThreadPool->ResumeExecution();
				g_pTestThreadPool->Stop();
				g_done.Reset();

				int counts[8] = { 0 };
				for ( int j = 0; j < ARRAYSIZE(jobs); j++ )
				{
					if ( jobs[j].GetServiceThread() != -1 )
					{
						counts[jobs[j].GetServiceThread()]++;
						jobs[j].ClearServiceThread();
					}
				}

				Msg( "ThreadPoolTest:         %d threads -- %d (%d) jobs processed in %fms, %fms to suspend (%f/%f) [%d, %d, %d, %d, %d, %d, %d, %d]\n", 
					i, g_nTotalAtFinish, (int)CCountJob::m_nCount, timer.GetDuration().GetMillisecondsF(), suspendTimer.GetDuration().GetMillisecondsF() - timer.GetDuration().GetMillisecondsF(),
					timer.GetDuration().GetMillisecondsF() / (float)CCountJob::m_nCount, (suspendTimer.GetDuration().GetMillisecondsF())/(float)g_nTotalAtFinish,
					counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7] );
			}
		}
	}
}


bool g_bOutputError;
volatile int g_ReadyToExecute;
CInterlockedInt g_nReady;

class CExecuteTestJob : public CJob
{
public:
	virtual JobStatus_t DoExecute()
	{
		byte pMemory[1024];
		int i;
		for ( i = 0; i < 1024; i++ )
		{
			pMemory[i] = rand();
		}
		for ( i = 0; i < 50; i++ )
		{
			sqrt( (float)HashBlock( pMemory, 1024 ) + HashBlock( pMemory, 1024 ) + 10.0 );
		}
		if ( AccessEvent()->Check() || IsFinished() )
		{
			if ( !g_bOutputError )
			{
				Msg( "Forced execute test failed!\n" );
				DebuggerBreakIfDebugging();
			}
		}
		return 0;
	}
};

class CExecuteTestExecuteJob : public CJob
{
public:
	virtual JobStatus_t DoExecute()
	{
		bool bAbort = ( RandomInt(  1, 10 ) == 1 );
		g_nReady++;
		while ( !g_ReadyToExecute )
		{
			ThreadPause();
		}

		if ( !bAbort )
			m_pTestJob->Execute();
		else
			m_pTestJob->Abort();
		g_nReady--;
		return 0;
	}

	CExecuteTestJob *m_pTestJob;
};


void TestForcedExecute()
{
	Msg( "TestForcedExecute\n" );
	for ( int tests = 0; tests < 30; tests++ )
	{
		for ( int i = 1; i <= 5; i += 2 )
		{
			g_nReady = 0;
			ThreadPoolStartParams_t params;
			params.nThreads = i;
			params.fDistribute = TRS_TRUE;
			g_pTestThreadPool->Start( params, "Tst" );

			static CExecuteTestJob jobs[4000];
			for ( int j = 0; j < ARRAYSIZE(jobs); j++ )
			{
				g_ReadyToExecute = false;
				for ( int k = 0; k < i; k++ )
				{
					CExecuteTestExecuteJob *pJob = new CExecuteTestExecuteJob;
					pJob->SetFlags( JF_QUEUE );
					pJob->m_pTestJob = &jobs[j];
					g_pTestThreadPool->AddJob( pJob );
					pJob->Release();
				}
				while ( g_nReady < i )
				{
					ThreadPause();
				}
				g_ReadyToExecute = true;
				ThreadSleep();
				jobs[j].Execute();
				while ( g_nReady > 0 )
				{
					ThreadPause();
				}
			}
			g_pTestThreadPool->Stop();
		}
	}
	Msg( "TestForcedExecute DONE\n" );
}

} // namespace ThreadPoolTest

void RunThreadPoolTests()
{
	CThreadPool pool;
	ThreadPoolTest::g_pTestThreadPool = &pool;
	RunTSQueueTests(10000);
	RunTSListTests(10000);

#ifdef _WIN32
	DWORD_PTR mask1 = 0;
	--mask1;
	DWORD_PTR mask2 = 0;
	--mask2;
	GetProcessAffinityMask( GetCurrentProcess(), &mask1, &mask2 );
#else
	int32 mask1=-1;
#endif
	Msg( "ThreadPoolTest: Job distribution speed\n" );
	for ( int i = 0; i < 2; i++ )
	{
		bool bToCompletion = ( i % 2 != 0 );
		if ( !IsX360() )
		{
			Msg( "ThreadPoolTest:     Non-distribute\n" );
			ThreadPoolTest::Test( false, true, bToCompletion );
		}

		Msg( "ThreadPoolTest:     Distribute\n" );
		ThreadPoolTest::Test( true, true, bToCompletion  );

		Msg( "ThreadPoolTest:     One core\n" );
		ThreadSetAffinity( 0, 1 );
		ThreadPoolTest::Test( false, true, bToCompletion  );
		ThreadSetAffinity( 0, mask1 );

		Msg( "ThreadPoolTest:     NO Sleep\n" );
		ThreadPoolTest::Test( false, false, bToCompletion  );

		Msg( "ThreadPoolTest:     Distribute\n" );
		ThreadPoolTest::Test( true, false, bToCompletion  );

		Msg( "ThreadPoolTest:     One core\n" );
		ThreadSetAffinity( 0, 1 );
		ThreadPoolTest::Test( false, false, bToCompletion  );
		ThreadSetAffinity( 0, mask1 );
	}

	Msg( "ThreadPoolTest: Jobs doing work\n" );
	for ( int i = 0; i < 2; i++ )
	{
		bool bToCompletion = true;// = ( i % 2 != 0 );
		if ( !IsX360() )
		{
			Msg( "ThreadPoolTest:     Non-distribute\n" );
			ThreadPoolTest::Test( false, true, bToCompletion, true );
		}

		Msg( "ThreadPoolTest:     Distribute\n" );
		ThreadPoolTest::Test( true, true, bToCompletion, true );

		Msg( "ThreadPoolTest:     One core\n" );
		ThreadSetAffinity( 0, 1 );
		ThreadPoolTest::Test( false, true, bToCompletion, true  );
		ThreadSetAffinity( 0, mask1 );

		Msg( "ThreadPoolTest:     NO Sleep\n" );
		ThreadPoolTest::Test( false, false, bToCompletion, true  );

		Msg( "ThreadPoolTest:     Distribute\n" );
		ThreadPoolTest::Test( true, false, bToCompletion, true  );

		Msg( "ThreadPoolTest:     One core\n" );
		ThreadSetAffinity( 0, 1 );
		ThreadPoolTest::Test( false, false, bToCompletion, true  );
		ThreadSetAffinity( 0, mask1 );
	}
#ifdef _WIN32
	GetProcessAffinityMask( GetCurrentProcess(), &mask1, &mask2 );
#endif

	ThreadPoolTest::TestForcedExecute();
}
