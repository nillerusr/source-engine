//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"
#include "tslist.h"
#include <workthreadpool.h>
#include <gclogger.h>

#include "tier0/memdbgon.h"

namespace GCSDK {

IWorkThreadPoolSignal *CWorkThreadPool::sm_pWorkItemsCompletedSignal = NULL;

//-----------------------------------------------------------------------------
// Purpose:	CWorkThread constructors
//-----------------------------------------------------------------------------
CWorkThread::CWorkThread( CWorkThreadPool *pThreadPool )
:	m_pThreadPool( pThreadPool ),
	m_bExitThread( false ),
	m_bFinished( false )
{
}

CWorkThread::CWorkThread( CWorkThreadPool *pThreadPool, const char *pszName )
:	m_pThreadPool( pThreadPool ),
	m_bExitThread( false ),
	m_bFinished( false )
{
	SetName( pszName );
}

//-----------------------------------------------------------------------------
// Purpose:	Tell work thread pool not to set event on every item added (SetEvent is very expensive)
//-----------------------------------------------------------------------------
void CWorkThreadPool::SetNeverSetEventOnAdd( bool bNeverSet ) 
{ 
	bool bWasSet = m_bNeverSetOnAdd;
	m_bNeverSetOnAdd = bNeverSet; 

	// In case of disabling set right away to make sure if we have pending work we execute it now with no latency
	if ( bWasSet && !m_bNeverSetOnAdd )
		m_EventNewWorkItem.Set();
}



//-----------------------------------------------------------------------------
// Purpose:	performs the work loop for the thread, waits for work,
//			notifies the owner (the pool) as it completes work and before it exits
//-----------------------------------------------------------------------------
int CWorkThread::Run()
{
	// manage our thread pool's statistics
	++m_pThreadPool->m_cThreadsRunning;

#ifdef _SERVER
	g_CompletionPortManager.AssociateCallingThreadWithIOCP();
#endif

	OnStart();

#if 0 // need to port over new vprof code
#if defined( VPROF_ENABLED )
	CVProfile *pProfile = GetVProfProfileForCurrentThread();
#endif
#endif

	CWorkThreadPool *pPool = m_pThreadPool;

	int nIterations = 0;
	const int nMaxFastIterations = 4;
	while ( !m_bExitThread )
	{
#if 0 // game vprof doesn't yet support TLS'd vprof instances, until new vprof code is ported
#if defined( VPROF_ENABLED )
		if ( pProfile )
			pProfile->MarkFrame( GetName() );
#endif
#endif
		pPool->m_cActiveThreads++;

		nIterations = 0;
		while ( (pPool->BNeverSetEventOnAdd() && nIterations < nMaxFastIterations) || nIterations == 0 )
		{
			// process any items which have arrived
			CWorkItem *pWorkItem = pPool->GetNextWorkItemToProcess( );
			while ( pWorkItem )
			{
#if 0
				pPool->m_StatWaitTime.Update( pWorkItem->WaitingTime() );
#endif
				if ( pWorkItem->HasTimedOut() )
				{
					pWorkItem->m_bCanceled = true;
				}
				else
				{
					// call the work item to do its work
					pWorkItem->m_bCanceled = false;

					CFastTimer fastTimer;
					fastTimer.Start();
					pWorkItem->m_bRunning = true;
					bool bSuccess = pWorkItem->ThreadProcess( this );
					pWorkItem->m_bRunning = false;
					fastTimer.End();
					CCycleCount cycleCount = fastTimer.GetDuration();
					pWorkItem->SetCycleCount(cycleCount);
#if 0
					pPool->m_StatExecutionTime.Update( cycleCount.GetUlMicroseconds() );
#endif
					if ( bSuccess )
						pPool->m_cSuccesses ++;
					else
						pWorkItem->m_bResubmit ? pPool->m_cRetries++ : pPool->m_cFailures++;
				}

				// do we need to resubmit this item?
				if ( pWorkItem->m_bResubmit )
				{
					pWorkItem->m_bResubmit = false;
					pWorkItem->m_bCanceled = false;
					// put it at the tail of the incoming queue 
					pPool->AddWorkItem( pWorkItem );
					pWorkItem->Release(); // dec since AddWorkItem added 1 more again
				}
				else
				{
					// put it in the outgoing queue 
					pPool->OnWorkItemCompleted( pWorkItem );
				}

				// If we are flagged as exiting don't try to get more work, we need to exit right away and orphan the work
				// to avoid blocking shutdown.
				if ( !m_bExitThread )
				{
					// get the next work item (if any)
					pWorkItem = pPool->GetNextWorkItemToProcess( );
				}
				else
				{
					pWorkItem = NULL;
				}

#if 0 // game vprof doesn't yet support TLS'd vprof instances, until new vprof code is ported
#if defined( VPROF_ENABLED )
				if ( pProfile && pWorkItem )
					pProfile->MarkFrame( GetName() );
#endif
#endif
			}

			if ( m_bExitThread )
				break;

			++nIterations;
			if ( pPool->BNeverSetEventOnAdd() && nIterations < nMaxFastIterations )
			{
				VPROF_BUDGET( "CWorkThread -- Sleep", VPROF_BUDGETGROUP_SLEEPING );
				ThreadSleep( 2 );
			}
		}

		pPool->m_cActiveThreads--;

		// wait for a new work item to arrive in the queue, check the counts first just to be sure
		{
			VPROF_BUDGET( "CWorkThread -- Sleep", VPROF_BUDGETGROUP_SLEEPING );
#ifdef _SERVER
			if ( pPool->BNeverSetEventOnAdd() )
				pPool->m_EventNewWorkItem.Wait( 15 );
			else
				pPool->m_EventNewWorkItem.Wait( 50 );
#else
			pPool->m_EventNewWorkItem.Wait( 50 );
#endif
		}
	}

	// Since we are exiting, we must have been signaled to shutdown, and we should signal any remaining threads
	// since each signal wakes only one thread.
	pPool->m_EventNewWorkItem.Set();
	
	m_bFinished = true;

	// updates stats
	--m_pThreadPool->m_cThreadsRunning;

	return EXIT_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose:  Construct a new CWorkThreadPool object
//-----------------------------------------------------------------------------
CWorkThreadPool::CWorkThreadPool( const char *pszThreadName )
 :	
#if 0
    m_StatWaitTime( 100 ),
	m_StatExecutionTime( 100 ),
#endif
	m_bThreadsInitialized( false ),
	m_cThreadsRunning( 0 ),
	m_cActiveThreads( 0 ),
	m_bMayHaveJobTimeouts( false ),
	m_bExiting( false ),
	m_bAutoCreateThreads( false ),
	m_cMaxThreads( 0 ),
	m_cFailures( 0 ),
	m_cSuccesses( 0 ),
	m_pWorkThreadConstructor( NULL ),
	m_ulLastCompletedSequenceNumber( 0 ),
	m_ulLastUsedSequenceNumber( 0 ),
	m_ulLastDispatchedSequenceNumber( 0 ),
	m_bEnsureOutputOrdering( false ),
	m_bNeverSetOnAdd( false )
{
	Assert( pszThreadName != NULL );
	Q_strncpy( m_szThreadNamePfx, pszThreadName, sizeof( m_szThreadNamePfx ) );
	m_LimitTimerCreateNewThreads.SetLimit( 1 );

	m_pTSQueueToProcess = new CTSQueue< CWorkItem* >;
	m_pTSQueueCompleted = new CTSQueue< CWorkItem* >;
}


//-----------------------------------------------------------------------------
// Purpose:  destructor; does assertion checks to make sure we weere shut down cleanly
//			 cleans up even if we weren't cleanly stopped
//-----------------------------------------------------------------------------
CWorkThreadPool::~CWorkThreadPool() 
{
	// If you hit this you probably didn't call StopWorkThreads() first
	AssertMsg1( ( !m_bThreadsInitialized || m_bExiting ) && 0 == m_cThreadsRunning,
		"CWorkThreadPool::~CWorkThreadPool(): Thread pool %s shutdown incorrectly.\n",
		m_szThreadNamePfx );

	if ( m_WorkThreads.Count() )
	{
		StopWorkThreads();
		Assert( 0 == m_WorkThreads.Count() );
	}

	Assert( 0 == m_cThreadsRunning );

	// WARNING: We need to release any items left in the queues
	CWorkItem *pWorkItem = NULL;
	if ( m_pTSQueueCompleted->Count() > 0 )
	{
		EmitWarning( SPEW_THREADS, 2, "CWorkThreadPool::~CWorkThreadPool: work complete queue not empty, %d items discarded.\n", m_pTSQueueCompleted->Count() );
		pWorkItem = NULL;
		while ( m_pTSQueueCompleted->PopItem( &pWorkItem ) )
		{
			while( pWorkItem->Release() )
			{
				/* nothing */
			}
		}
	}

	if ( m_pTSQueueToProcess->Count() > 0 )
	{
		EmitWarning( SPEW_THREADS, 2, "CWorkThreadPool::~CWorkThreadPool: work processing queue not empty: %d items discarded.\n", m_pTSQueueToProcess->Count() );
		while ( m_pTSQueueToProcess->PopItem( &pWorkItem ) )
		{
			while( pWorkItem->Release() )
			{
				/* nothing */
			}
		}
	}

	delete m_pTSQueueToProcess;
	delete m_pTSQueueCompleted;
}


#if 0
//-----------------------------------------------------------------------------
// Purpose:	estimate the current backlog time using previous execution time,
//			the number of outstanding items, and the number of running threads
//-----------------------------------------------------------------------------
uint64 CWorkThreadPool::GetCurrentBacklogTime() const
{
	if ( m_WorkThreads.Count() == 0 )
		return 0;
	return ( m_pTSQueueToProcess->Count() * m_StatExecutionTime.GetUlAvg() ) / m_WorkThreads.Count();
}
#endif


int CWorkThreadPool::AddWorkThread( CWorkThread *pThread ) 
{
	AUTO_LOCK( m_WorkThreadMutex );
	Assert( pThread );
	return m_WorkThreads.AddToTail( pThread );
}


void CWorkThreadPool::StartWorkThread( CWorkThread *pWorkThread, int iName )
{
	char rgchThreadName[32];
	Q_snprintf( rgchThreadName, sizeof( rgchThreadName ), "%s:%d", m_szThreadNamePfx, iName );
	pWorkThread->SetName( rgchThreadName );
	if ( !pWorkThread->Start() )
		EmitError( SPEW_THREADS, "CWorkThreadPool::StartWorkThread: Thread creation failed.\n" );
}


void CWorkThreadPool::StartWorkThreads()
{
	m_bThreadsInitialized = true;
	if ( 0 == m_WorkThreads.Count() )
	{
		EmitWarning( SPEW_THREADS, 2, "CWorkThreadPool::StartWorkThreads: called with no threads in the pool, this is probably a bug.\n" );
		return;
	}
	m_bExiting = false;
	m_cThreadsRunning = 0;
	AUTO_LOCK( m_WorkThreadMutex );
	FOR_EACH_VEC( m_WorkThreads, i )
	{
		StartWorkThread( m_WorkThreads[i], i );
	}

	// XXX why?
	while ( m_cThreadsRunning == (uint) 0 )
	{
		ThreadSleep( 1 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: stops whatever work threads we're running
//			this must be called before the thread pool object is destroyed
//-----------------------------------------------------------------------------
void CWorkThreadPool::StopWorkThreads()
{
	// indicate that we're shutting down;
	// don't accept more work in this thread
	m_bExiting = true;

	AUTO_LOCK( m_WorkThreadMutex );

	FOR_EACH_VEC( m_WorkThreads, i )
	{
		m_WorkThreads[i]->m_bExitThread = true;
		m_WorkThreads[i]->Cancel();
	}

	// loop until all threads are dead
	while ( true )
	{
		// This thread already holds the mutex; recursive try-lock should always succeed
		DbgVerify( BTryDeleteExitedWorkerThreads() );

		if ( m_WorkThreads.Count() == 0 )
			break;

		// Keep waking up threads until they're all dead.
		m_EventNewWorkItem.Set();
		
#ifdef _PS3
		// call to abort any running call to gethostbyname().
		// this is called over all the remaining work threads, while
		// waiting for the rest of the work threads to finish so that they won't
		// spuriously block on new calls to gethostbyname() as the
		// sys_net_abort_resolver call only stops the next call to the 
		// network API, not any future calls.

		FOR_EACH_VEC( m_WorkThreads, iPS3 )
		{
			// PS3 hack to abort gethostbyname() calls that may be blocking...
			sys_net_abort_resolver( m_WorkThreads[ iPS3 ]->GetThreadID(), SYS_NET_ABORT_STRICT_CHECK );
		}
#endif

		const uint k_uJoinTimeoutMillisec = 10000; // 10 seconds seems pretty arbitrary.

		CWorkThread *pWorkThread = m_WorkThreads[0];
		bool bJoined = pWorkThread->Join( k_uJoinTimeoutMillisec );
		if ( !bJoined )
		{
			// Print thread id as a pointer for cross-platform compatibility
			EmitWarning( SPEW_THREADS, 2, "Thread \"%s\" (ID %p) failed to shut down", pWorkThread->GetName(), (void*)pWorkThread->GetThreadID() );
		}
		else
		{
			// Succesful join means that the thread has terminated.
			if ( !pWorkThread->m_bFinished )
			{
				// This would be a logic error in the thread proc if it ever tripped.
				AssertMsg( false, "pWorkThread->m_bFinished is false but thread is not running" );
				// Recover by flagging the thread as potentially eligable for deletion, since it's dead.
				pWorkThread->m_bFinished = true;
			}
		}
	}

	Assert( m_WorkThreads.Count() == 0 && m_cThreadsRunning == (uint32) 0 );
}


//-----------------------------------------------------------------------------
// Purpose: sees if we have a non-zero number of work threads,
//			or a non-zero number of active threads
//-----------------------------------------------------------------------------
bool CWorkThreadPool::HasWorkItemsToProcess() const
{
	return ( m_pTSQueueToProcess->Count() > 0 ) 
 		|| ( m_cActiveThreads > 0 );
}


//-----------------------------------------------------------------------------
// Purpose: sets dynamic thread construction
//-----------------------------------------------------------------------------
void CWorkThreadPool::SetWorkThreadAutoConstruct( int cMaxThreads, IWorkThreadFactory *pWorkThreadConstructor )
{
	AUTO_LOCK( m_WorkThreadMutex );

	m_bThreadsInitialized = true;
	m_bAutoCreateThreads = true;
	m_cMaxThreads = MAX( 1, cMaxThreads );
	m_pWorkThreadConstructor = pWorkThreadConstructor;

	// If we have too many threads now, mark some to exit next time they loop.
	for ( int i = m_cMaxThreads; i < m_WorkThreads.Count(); i++ )
	{
		m_WorkThreads[i]->m_bExitThread = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a work item
// Output:	true if successful,
//			false if a low priority work item is not added due to a busy system
//			false if this work pool is shutting down and work isn't being accepted
// NOTE:	Adding normal priority items should always succeed
//-----------------------------------------------------------------------------
bool CWorkThreadPool::AddWorkItem( CWorkItem *pWorkItem )
{
	Assert( !m_bExiting );
	if ( m_bExiting )
		return false;

	if ( m_bEnsureOutputOrdering )
	{
		AssertMsg( pWorkItem->m_bResubmit == false, "CWorkThreadPool can't support item auto resubmission when ensuring output ordering" );
	}

	// if we're in auto-create mode, make sure we have enough threads running
	if ( m_bAutoCreateThreads && m_WorkThreads.Count() < m_cMaxThreads )
	{
		int cPendingItems = m_pTSQueueToProcess->Count();

		// we shouldn't get more than 12 items queued per already existing thread, otherwise we
		// want to create a new thread to help us keep up.
		if ( m_WorkThreads.Count() < 1 || m_WorkThreads.Count() * 12 < ( cPendingItems + 1 ) )
		{
			if ( m_WorkThreads.Count() >= 2 && !m_LimitTimerCreateNewThreads.BLimitReached() )
			{
				// Don't create more yet, we don't want to create them too fast
			}
			else
			{
				// create another thread
				CWorkThread *pWorkThread = NULL;
				if ( m_pWorkThreadConstructor )
				{
					pWorkThread = m_pWorkThreadConstructor->CreateWorkerThread( this );
				}
				else
				{
					pWorkThread = new CWorkThread( this );
				}
				if( pWorkThread != NULL ) 
				{
					int iName = AddWorkThread( pWorkThread );
					StartWorkThread( pWorkThread, iName );
				}
				m_LimitTimerCreateNewThreads.SetLimit( 250*k_nThousand );
			}
		}
	}

	//
	//	Do we actually have any threads ?   If creating threads can fail, then maybe we don't !
	//	In that case, this WorkItem is not going to run !
	//
	if ( m_WorkThreads.Count() == 0 ) 
	{
		Assert(false);
		return	false ; 
	}
	

	// WARNING: We need to call pWorkItem AddRef() and Release() at all entry/exit points for the thread pool system.
	pWorkItem->AddRef();

	pWorkItem->m_ulSequenceNumber = (++m_ulLastUsedSequenceNumber);
	m_pTSQueueToProcess->PushItem( pWorkItem );

	if ( !BNeverSetEventOnAdd() && m_cActiveThreads == 0 )
	{
		VPROF_BUDGET( "SetEvent()", VPROF_BUDGETGROUP_THREADINGMAIN );
		m_EventNewWorkItem.Set();
	}

	return true;
}


CWorkItem *CWorkThreadPool::GetNextCompletedWorkItem( )
{
	CWorkItem *pWorkItem = NULL;

	// Use a while loop just in case ref counts get screwed up and an item gets deleted when we release our reference to it
	while ( m_pTSQueueCompleted->PopItem( &pWorkItem ) )
	{
		// WARNING: We need to call workitem AddRef() and Release() at all entry/exit points for the thread pool system.
		// Release() returns the current refcount of the object (after decrementing it by one) and should be non-zero unless the
		// the caller has released it already.
		if ( pWorkItem != NULL && pWorkItem->Release() > 0 )
		{
			return pWorkItem;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: gets the next work item to process. This non-blocking function
//			returns NULL immediately if there's nothing left in the queue.
//			otherwise, a pointer to the next CWorkItem.
//-----------------------------------------------------------------------------
CWorkItem *CWorkThreadPool::GetNextWorkItemToProcess( )
{
	CWorkItem *pWorkItem = NULL;

	if ( m_pTSQueueToProcess->Count() && m_pTSQueueToProcess->PopItem( &pWorkItem ) )
	{
		return pWorkItem;
	}
	
	return NULL;
}


bool CWorkThreadPool::BDispatchCompletedWorkItems( const CLimitTimer &limitTimer, CJobMgr *pJobMgr )
{
	BTryDeleteExitedWorkerThreads();

	CWorkItem *pWorkItem = GetNextCompletedWorkItem( );
	while ( pWorkItem != NULL )
	{
		uint64 ulSequenceNumber = pWorkItem->m_ulSequenceNumber;
		// NOTE: despite its name, this YIELDS - the target job
		// is resumed, and we resume here.
		if ( !pWorkItem->DispatchCompletedWorkItem( pJobMgr ) )
		{
			EmitWarning( SPEW_THREADS, 2, "Work Item for Work Pool %s completed but job no longer existed to notify\n", m_szThreadNamePfx == NULL ? "UNKNOWN" :m_szThreadNamePfx );
			AssertMsg1( m_bMayHaveJobTimeouts, "Work Item for Work Pool %s completed but job no longer existed to notify", m_szThreadNamePfx == NULL ? "UNKNOWN" :m_szThreadNamePfx );
		}

		// pWorkItem was released by DispatchCompletedWorkItem
		m_ulLastDispatchedSequenceNumber = ulSequenceNumber;
		if ( limitTimer.BLimitReached() )
			break;

		pWorkItem = GetNextCompletedWorkItem( );
	}

	return ( GetCompletedWorkItemCount() > 0 ); 	
}


//-----------------------------------------------------------------------------
// Purpose:	delete any thread objects that have exited
//			we'll make sure the thread has actually ended;
//			if they haven't, they'll remain in the threads to delete list
//-----------------------------------------------------------------------------
bool CWorkThreadPool::BTryDeleteExitedWorkerThreads()
{
	if ( m_WorkThreadMutex.TryLock() )
	{
		if ( m_cThreadsRunning < (uint) m_WorkThreads.Count() )
		{
			FOR_EACH_VEC_BACK( m_WorkThreads, i )
			{
				CWorkThread *pWorkThread = m_WorkThreads[i];
				if ( pWorkThread->m_bFinished && !pWorkThread->IsThreadRunning() )
				{
					m_WorkThreads.FastRemove( i );
					delete pWorkThread;
				}
			}
		}
		m_WorkThreadMutex.Unlock();
		return true;
	}
	return false;
}


bool CWorkItem::DispatchCompletedWorkItem( CJobMgr *pJobMgr )
{
	// Check if this work item needs to signal a job
	if ( pJobMgr && k_GIDNil != m_JobID )
	{
		if ( !pJobMgr->BRouteWorkItemCompletedIfExists( m_JobID, m_bCanceled ) )
			return false;
	}
	else if ( k_GIDNil != m_JobID )
	{
		// This should never happen since we have already released our reference to the work item
		// and the calling job should have released its ref when it exited
		AssertMsg( false, "CWorkItem::DispatchCompletedWorkItem: got a work item with no job ID" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:	Called by the worker thread when it finishes an individual work item
//			This function will see if our work is meant to be well-ordred; if so,
//			it will do the necessary work to ensure ordering.
//
//			It adds the item to the completed work item list so 
//			the pool owner can retrieve it and checks to see if any threads
//			deserve to be shut down.
//-----------------------------------------------------------------------------
void CWorkThreadPool::OnWorkItemCompleted( CWorkItem *pWorkItem ) 
{ 
	if ( sm_pWorkItemsCompletedSignal != NULL )
		sm_pWorkItemsCompletedSignal->Signal();

	if ( !m_bEnsureOutputOrdering )
	{
		// Since we aren't locking this sequence number could get screwed up a bit, but it's 
		// pretty meaningless if ensure output ordering if off anyway...
		m_ulLastCompletedSequenceNumber = pWorkItem->m_ulSequenceNumber;
		m_pTSQueueCompleted->PushItem( pWorkItem ); 
	}
	else
	{
		// In the ordered case we need to lock completely here since we'll be moving around between
		// various data structures and also need to ensure the ordering of items in the TS queue
		m_MutexOnItemCompletedOrdered.Lock();
		if ( m_ulLastCompletedSequenceNumber + 1 == pWorkItem->m_ulSequenceNumber )
		{
			m_ulLastCompletedSequenceNumber = pWorkItem->m_ulSequenceNumber;
			m_pTSQueueCompleted->PushItem( pWorkItem ); 

			// We walk the vector multiple times, but it should be very short as items are likely to come in
			// close to in order, just mixed up a little if we have lots of threads or one item is much more
			// costly than others.  
			bool bFoundNext = false;
			do 
			{
				bFoundNext = false;
				FOR_EACH_VEC( m_vecCompletedAndWaiting, i )
				{
					CWorkItem *pWaiting = m_vecCompletedAndWaiting[i];
					if ( m_ulLastCompletedSequenceNumber + 1 == pWaiting->m_ulSequenceNumber )
					{
						m_ulLastCompletedSequenceNumber = pWaiting->m_ulSequenceNumber;
						m_pTSQueueCompleted->PushItem( pWaiting ); 
						m_vecCompletedAndWaiting.FastRemove( i );
						bFoundNext = true;
						break;
					}
				}
			} while ( bFoundNext == true );
		}
		else
		{
			m_vecCompletedAndWaiting.AddToTail( pWorkItem );
		}
		m_MutexOnItemCompletedOrdered.Unlock();
	}
}


//-----------------------------------------------------------------------------
// Purpose: return the count of items we've queued to process
//-----------------------------------------------------------------------------
int CWorkThreadPool::GetWorkItemToProcessCount() const
{
	return m_pTSQueueToProcess->Count();
}


//-----------------------------------------------------------------------------
// Purpose: return the count of items we've completed but not notified the consumer about
//-----------------------------------------------------------------------------
int CWorkThreadPool::GetCompletedWorkItemCount() const 
{ 
	int nCount = m_pTSQueueCompleted->Count();
	return nCount; 
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Validates memory
//-----------------------------------------------------------------------------
void CWorkThreadPool::Validate( CValidator &validator, const char *pchName )	
{
	VALIDATE_SCOPE();
	AUTO_LOCK( m_WorkThreadMutex );

	ValidateObj( m_WorkThreads );
	FOR_EACH_VEC( m_WorkThreads, iWorkThread )
	{
		m_WorkThreads[ iWorkThread ]->Suspend();
		ValidatePtr( m_WorkThreads[ iWorkThread ] );
	}

	ValidateAlignedPtr( m_pTSQueueCompleted );
	ValidateAlignedPtr( m_pTSQueueToProcess );
	ValidateObj( m_vecCompletedAndWaiting );
	FOR_EACH_VEC( m_vecCompletedAndWaiting, j )
	{
		ValidatePtr( m_vecCompletedAndWaiting.Element( j ) );
	}

	FOR_EACH_VEC( m_WorkThreads, iWorkThread )
	{
		m_WorkThreads[ iWorkThread ]->Resume();
	}

#if 0
	ValidateObj( m_StatExecutionTime );
	ValidateObj( m_StatWaitTime );
#endif
}
#endif // DBGFLAG_VALIDATE

} // namespace GCSDK
