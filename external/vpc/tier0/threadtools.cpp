//========== Copyright 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "tier0/platform.h"

#if defined( PLATFORM_WINDOWS_PC )
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0403
#include <windows.h>
#endif

#ifdef PLATFORM_WINDOWS
	#include <process.h>
	#ifdef PLATFORM_WINDOWS_PC
		#include <Mmsystem.h>
		#pragma comment(lib, "winmm.lib")
	#endif
#elif PLATFORM_PS3
	#include <sched.h>
	#include <unistd.h>
	#include <exception>
	#include <errno.h>
	#include <pthread.h>
	#include <sys/time.h>
	#include <sys/timer.h>
	#define GetLastError() errno
	typedef void *LPVOID;
#elif PLATFORM_POSIX
	#include <sched.h>
	#include <exception>
	#include <errno.h>
	#include <signal.h>
	#include <pthread.h>
	#include <sys/time.h>
	#define GetLastError() errno
	typedef void *LPVOID;
#if !defined(OSX)
	#include <sys/fcntl.h>
	#include <sys/unistd.h>
	#define sem_unlink( arg )
#else
	#define pthread_yield pthread_yield_np
	#include <mach/thread_act.h>
#endif // !OSX

#endif

#ifndef _PS3
#include <memory.h>
#endif
#include "tier0/threadtools.h"

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

#include <map>

// Must be last header...
#include "tier0/memdbgon.h"

#ifdef _PS3
#include "ps3/ps3_win32stubs.h"
#define NEW_WAIT_FOR_MULTIPLE_OBJECTS
bool gbCheckNotMultithreaded = true;
#endif

#define THREADS_DEBUG 1

#define DEBUG_ERROR(XX) Assert(0)

// Need to ensure initialized before other clients call in for main thread ID
#ifdef _WIN32
#pragma warning(disable:4073)
#pragma init_seg(lib)
#endif

#ifdef _WIN32
ASSERT_INVARIANT(TT_SIZEOF_CRITICALSECTION == sizeof(CRITICAL_SECTION));
ASSERT_INVARIANT(TT_INFINITE == INFINITE);
#endif

// thread creation counter.
// this is used to provide a unique threadid for each running thread in g_nThreadID ( a thread local variable ).

const int MAX_THREAD_IDS = 128;

static volatile bool s_bThreadIDAllocated[MAX_THREAD_IDS];

#ifdef LINUX

DLL_CLASS_EXPORT __thread int g_nThreadID;

#elif defined(_PS3)
	#include "tls_ps3.h"
#else 
	CTHREADLOCALINT g_nThreadID;
#endif 


static CThreadFastMutex s_ThreadIDMutex;

PLATFORM_INTERFACE void AllocateThreadID( void )
{
	AUTO_LOCK( s_ThreadIDMutex );
	for( int i = 1; i < MAX_THREAD_IDS; i++ )
	{
		if ( ! s_bThreadIDAllocated[i] )
		{
			g_nThreadID = i;
			s_bThreadIDAllocated[i] = true;
			return;
		}
	}
	Error( "Out of thread ids. Decrease the number of threads or increase MAX_THREAD_IDS\n" );
}

PLATFORM_INTERFACE void FreeThreadID( void )
{
	AUTO_LOCK( s_ThreadIDMutex );
	int nThread = g_nThreadID;
	if ( nThread )
		s_bThreadIDAllocated[nThread] = false;
}
		

//-----------------------------------------------------------------------------
// Simple thread functions. 
// Because _beginthreadex uses stdcall, we need to convert to cdecl
//-----------------------------------------------------------------------------
struct ThreadProcInfo_t
{
	ThreadProcInfo_t( ThreadFunc_t pfnThread, void *pParam )
	  : pfnThread( pfnThread),
		pParam( pParam )
	{
	}
	
	ThreadFunc_t pfnThread;
	void *		 pParam;
};

//---------------------------------------------------------

#ifdef PLATFORM_WINDOWS
static DWORD WINAPI ThreadProcConvert( void *pParam )
{
	ThreadProcInfo_t info = *((ThreadProcInfo_t *)pParam);
	AllocateThreadID();
	delete ((ThreadProcInfo_t *)pParam);
	unsigned nRet = (*info.pfnThread)(info.pParam);
	FreeThreadID();
	return nRet;
}
#elif defined( PLATFORM_PS3 )
union ThreadProcInfoUnion_t
{
	struct Val_t
	{
		ThreadFunc_t pfnThread;
		void *		 pParam;
	}
	val;
	uint64_t val64;
};
static void ThreadProcConvertUnion( uint64_t param )
{
	COMPILE_TIME_ASSERT( sizeof( ThreadProcInfoUnion_t ) == 8 );
	ThreadProcInfoUnion_t info;
	info.val64 = param;
	AllocateThreadID();
	unsigned nRet = (*info.val.pfnThread)(info.val.pParam);
	FreeThreadID();
	sys_ppu_thread_exit( nRet );
}
static void* ThreadProcConvert( void *pParam )
{
	ThreadProcInfo_t info = *((ThreadProcInfo_t *)pParam);
	AllocateThreadID();
	delete ((ThreadProcInfo_t *)pParam);
	unsigned nRet = (*info.pfnThread)(info.pParam);
	FreeThreadID();
	return ( void * ) nRet;
}

#else
static void* ThreadProcConvert( void *pParam )
{
	ThreadProcInfo_t info = *((ThreadProcInfo_t *)pParam);
	AllocateThreadID();
	delete ((ThreadProcInfo_t *)pParam);
	unsigned nRet = (*info.pfnThread)(info.pParam);
	FreeThreadID();
	return ( void * ) nRet;
}
#endif



#if defined( _PS3 )

/*******************************************************************************
* Thread Local Storage globals and functions
*******************************************************************************/
#ifndef _PS3
__thread void *gTLSValues[ MAX_TLS_VALUES ] = { NULL };
__thread bool  gTLSFlags[ MAX_TLS_VALUES ] = { false };
__thread bool gbWaitObjectsCreated = false;
__thread sys_semaphore_t gWaitObjectsSemaphore;
#endif // !_PS3

static char gThreadName[28] = "";

// Simple TLS allocator.  Linearly searches for a free slot.
uint32 TlsAlloc()
{
	for ( int i = 0; i < MAX_TLS_VALUES; ++i )
	{
		if ( !gTLSFlags[i] )
		{
			gTLSFlags[i] = true;
			return i;
		}
	}

#ifdef _PS3
	DEBUG_ERROR("TlsAlloc(): Out of TLS\n");
#endif

	return 0xFFFFFFFF;
}

void TlsFree( uint32 index )
{
	gTLSValues[ index ] = NULL;
	gTLSFlags[ index ] = false;
}

void *TlsGetValue( uint32 index )
{
	return gTLSValues[ index ];
}

void TlsSetValue( uint32 index, void *pValue )
{
	gTLSValues[ index ] = pValue;
}
#endif //_PS3



#ifdef PLATFORM_WINDOWS
class CThreadHandleToIDMap
{
public:
	HANDLE	m_hThread;
	uint	m_ThreadID;
	CThreadHandleToIDMap *m_pNext;
};
static CThreadHandleToIDMap *g_pThreadHandleToIDMaps = NULL;
static CThreadMutex g_ThreadHandleToIDMapMutex;
static volatile int g_nThreadHandleToIDMaps = 0;

static void AddThreadHandleToIDMap( HANDLE hThread, uint threadID )
{
	if ( !hThread )
		return;

	// Remember this handle/id combo.
	CThreadHandleToIDMap *pMap = new CThreadHandleToIDMap;
	pMap->m_hThread = hThread;
	pMap->m_ThreadID = threadID;

	// Add it to the global list.
	g_ThreadHandleToIDMapMutex.Lock();
	pMap->m_pNext = g_pThreadHandleToIDMaps;
	g_pThreadHandleToIDMaps = pMap;
	++g_nThreadHandleToIDMaps;

	g_ThreadHandleToIDMapMutex.Unlock();

	if ( g_nThreadHandleToIDMaps > 500 )
		Error( "ThreadHandleToIDMap overflow." );
}

// This assumes you've got g_ThreadHandleToIDMapMutex locked!!
static bool InternalLookupHandleToThreadIDMap( HANDLE hThread, CThreadHandleToIDMap* &pMap, CThreadHandleToIDMap** &ppPrev )
{
	ppPrev = &g_pThreadHandleToIDMaps;
	for ( pMap=g_pThreadHandleToIDMaps; pMap; pMap=pMap->m_pNext )
	{
		if ( pMap->m_hThread == hThread )
			return true;

		ppPrev = &pMap->m_pNext;
	}

	return false;
}

static void RemoveThreadHandleToIDMap( HANDLE hThread )
{
	if ( !hThread )
		return;

	CThreadHandleToIDMap *pMap, **ppPrev;
	
	g_ThreadHandleToIDMapMutex.Lock();

	if ( g_nThreadHandleToIDMaps <= 0 )
		Error( "ThreadHandleToIDMap underflow." );

	if ( InternalLookupHandleToThreadIDMap( hThread, pMap, ppPrev ) )
	{
		*ppPrev = pMap->m_pNext;
		delete pMap;
		--g_nThreadHandleToIDMaps;
	}

	g_ThreadHandleToIDMapMutex.Unlock();
}

static uint LookupThreadIDFromHandle( HANDLE hThread )
{
	if ( hThread == NULL || hThread == GetCurrentThread() )
		return GetCurrentThreadId();

	float flStartTime = Plat_FloatTime();
	while ( Plat_FloatTime() - flStartTime < 2 )
	{
		CThreadHandleToIDMap *pMap, **ppPrev;

		g_ThreadHandleToIDMapMutex.Lock();
		bool bRet = InternalLookupHandleToThreadIDMap( hThread, pMap, ppPrev );
		g_ThreadHandleToIDMapMutex.Unlock();
		
		if ( bRet )
			return pMap->m_ThreadID;

		// We should only get here if a thread that is just starting up is currently in AddThreadHandleToIDMap.
		// Give up the timeslice and try again.
		ThreadSleep( 1 );
	}

	Assert( !"LookupThreadIDFromHandle failed!" );
	Warning( "LookupThreadIDFromHandle couldn't find thread ID for handle." );
	return 0;
}
#endif


//---------------------------------------------------------

ThreadHandle_t * CreateTestThreads( ThreadFunc_t fnThread, int numThreads, int nProcessorsToDistribute )
{
	ThreadHandle_t *pHandles = (new ThreadHandle_t[numThreads+1]) + 1;
	pHandles[-1] = (ThreadHandle_t)INT_TO_POINTER( numThreads );
	for( int i = 0; i < numThreads; ++i )
	{
		//TestThreads();
		ThreadHandle_t hThread;
		const unsigned int nDefaultStackSize = 64 * 1024; // this stack size is used in case stackSize == 0
		hThread = CreateSimpleThread( fnThread, INT_TO_POINTER( i ), nDefaultStackSize );

		if ( nProcessorsToDistribute )
		{
			int32 mask = 1 << (i % nProcessorsToDistribute);
			ThreadSetAffinity( hThread, mask );
		}
		
/*		
		ThreadProcInfoUnion_t info;
		info.val.pfnThread = fnThread;
		info.val.pParam = (void*)(i);
		if ( int nError = sys_ppu_thread_create( &hThread, ThreadProcConvertUnion, info.val64, 1001, nDefaultStackSize, SYS_PPU_THREAD_CREATE_JOINABLE, "SimpleThread" ) != CELL_OK )
		{
			printf( "PROBLEM!\n" );
			Error( "Cannot create thread, error %d\n", nError );
			return 0;
		}
*/
		//ThreadHandle_t hThread = CreateSimpleThread( fnThread, (void*)i );
		pHandles[i] = hThread;
	}
//	printf("Finishinged CreateTestThreads(%p,%d)\n", (void*)fnThread,  numThreads );
	return pHandles;
}

void JoinTestThreads( ThreadHandle_t *pHandles )
{
	int nCount = POINTER_TO_INT( pHandles[-1] );
// 	printf("Joining test threads @%p[%d]:\n", pHandles, nCount );
// 	for( int i = 0; i < nCount; ++i )
// 	{
// 		printf("    %p,\n", (void*)pHandles[i] );
// 	}
	for( int i = 0; i < nCount; ++i )
	{
//		printf( "Joining %p", (void*) pHandles[i] );
//		if( !i ) sys_timer_usleep(100000);
		ThreadJoin( pHandles[i] );
		ReleaseThreadHandle( pHandles[i] );
	}
	delete[]( pHandles - 1 );
}



ThreadHandle_t CreateSimpleThread( ThreadFunc_t pfnThread, void *pParam, unsigned stackSize )
{
#ifdef PLATFORM_WINDOWS
	DWORD threadID;
	HANDLE hThread = (HANDLE)CreateThread( NULL, stackSize, ThreadProcConvert, new ThreadProcInfo_t( pfnThread, pParam ), 0, &threadID );
	AddThreadHandleToIDMap( hThread, threadID );
	return (ThreadHandle_t)hThread;
#elif PLATFORM_PS3
	//TestThreads();
	ThreadHandle_t th;
	ThreadProcInfoUnion_t info;
	info.val.pfnThread = pfnThread;
	info.val.pParam = pParam;
	const unsigned int nDefaultStackSize = 64 * 1024; // this stack size is used in case stackSize == 0
	if ( sys_ppu_thread_create( &th, ThreadProcConvertUnion, info.val64, 1001, stackSize ? stackSize : nDefaultStackSize, SYS_PPU_THREAD_CREATE_JOINABLE, "SimpleThread" ) != CELL_OK )
	{
		AssertMsg1( 0, "Failed to create thread (error 0x%x)", errno );
		return 0;
	}
	return th;
#elif PLATFORM_POSIX
	pthread_t tid;
	pthread_create( &tid, NULL, ThreadProcConvert, new ThreadProcInfo_t( pfnThread, pParam ) );
	return ( ThreadHandle_t ) tid;
#else
	Assert( 0 );
	DebuggerBreak();
	return 0;
#endif
}


bool ReleaseThreadHandle( ThreadHandle_t hThread )
{
#ifdef _WIN32
	bool bRetVal = ( CloseHandle( hThread ) != 0 );
	RemoveThreadHandleToIDMap( (HANDLE)hThread );
	return bRetVal;
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
//
// Wrappers for other simple threading operations
//
//-----------------------------------------------------------------------------

void ThreadSleep(unsigned nMilliseconds)
{
#ifdef _WIN32

#ifdef PLATFORM_WINDOWS_PC
	static bool bInitialized = false;
	if ( !bInitialized )
	{
		bInitialized = true;
		// Set the timer resolution to 1 ms (default is 10.0, 15.6, 2.5, 1.0 or
		// some other value depending on hardware and software) so that we can
		// use Sleep( 1 ) to avoid wasting CPU time without missing our frame
		// rate.
		timeBeginPeriod( 1 );
	}
#endif

	Sleep( nMilliseconds );
#elif PLATFORM_PS3
	if( nMilliseconds == 0 )
	{
		// sys_ppu_thread_yield doesn't seem to function properly, so sleep instead.
		sys_timer_usleep( 60 );
	}
	else
	{
		sys_timer_usleep( nMilliseconds * 1000 );
	}
#elif defined(POSIX)
   usleep( nMilliseconds * 1000 ); 
#endif
}

//-----------------------------------------------------------------------------

#ifndef ThreadGetCurrentId
ThreadId_t ThreadGetCurrentId()
{
#ifdef _WIN32
	return GetCurrentThreadId();
#elif defined( _PS3 )
	sys_ppu_thread_t th = 0;
	sys_ppu_thread_get_id( &th );
	return th;
#elif defined(POSIX)
	return (ThreadId_t)pthread_self();
#else
	Assert(0);
	DebuggerBreak();
	return 0;
#endif
}
#endif

//-----------------------------------------------------------------------------
ThreadHandle_t ThreadGetCurrentHandle()
{
#ifdef _WIN32
	return (ThreadHandle_t)GetCurrentThread();
#elif defined( _PS3 )
	sys_ppu_thread_t th = 0;
	sys_ppu_thread_get_id( &th );
	return th;
#elif defined(POSIX)
	return (ThreadHandle_t)pthread_self();
#else
	Assert(0);
	DebuggerBreak();
	return 0;
#endif
}

//-----------------------------------------------------------------------------

int ThreadGetPriority( ThreadHandle_t hThread )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

#ifdef _WIN32
	return ::GetThreadPriority( (HANDLE)hThread );
#elif defined( _PS3 )
	int iPri = 0;
	sys_ppu_thread_get_priority( hThread, &iPri );
	return iPri;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------

bool ThreadSetPriority( ThreadHandle_t hThread, int priority )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

#ifdef _WIN32
	return ( SetThreadPriority(hThread, priority) != 0 );
#elif defined( _PS3 )
	int retval = sys_ppu_thread_set_priority( hThread, priority );
	return retval >= CELL_OK;
#elif defined(POSIX)
	struct sched_param thread_param; 
	thread_param.sched_priority = priority; 
	//pthread_setschedparam( (pthread_t ) hThread, SCHED_RR, &thread_param );
	return true;
#endif
}

//-----------------------------------------------------------------------------

void ThreadSetAffinity( ThreadHandle_t hThread, int nAffinityMask )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

#ifdef _WIN32
	SetThreadAffinityMask( hThread, nAffinityMask );
#elif defined(POSIX)
// 	cpu_set_t cpuSet;
// 	CPU_ZERO( cpuSet );
// 	for( int i = 0 ; i < 32; i++ )
// 	  if ( nAffinityMask & ( 1 << i ) )
// 	    CPU_SET( cpuSet, i );
// 	sched_setaffinity( hThread, sizeof( cpuSet ), &cpuSet );
#endif

}

//-----------------------------------------------------------------------------

#ifndef _X360
uint InitMainThread()
{
	ThreadSetDebugName( "MainThrd" );

	return ThreadGetCurrentId();
}

unsigned long g_ThreadMainThreadID = InitMainThread();

bool ThreadInMainThread()
{
	return ( ThreadGetCurrentId() == g_ThreadMainThreadID );
}

void DeclareCurrentThreadIsMainThread()
{
	g_ThreadMainThreadID = ThreadGetCurrentId();
}

#else
byte *InitMainThread()
{
	byte b;

	return  AlignValue( &b, 64*1024 );
}
#define STACK_SIZE_360 327680
byte *g_pBaseMainStack = InitMainThread();
byte *g_pLimitMainStack = InitMainThread() - STACK_SIZE_360;
#endif

//-----------------------------------------------------------------------------
bool ThreadJoin( ThreadHandle_t hThread, unsigned timeout )
{
	if ( !hThread )
	{
		return false;
	}

#ifdef _WIN32
	DWORD dwWait = WaitForSingleObject( (HANDLE)hThread, timeout );
	if ( dwWait == WAIT_TIMEOUT)
		return false;
	if ( dwWait != WAIT_OBJECT_0 && ( dwWait != WAIT_FAILED && GetLastError() != 0 ) )
	{
		Assert( 0 );
		return false;
	}
#elif defined( _PS3 )
	uint64 uiExitCode = 0;
	int retval = sys_ppu_thread_join( hThread, &uiExitCode );
	return ( retval >= CELL_OK );
#elif defined(POSIX)
	if ( pthread_join( (pthread_t)hThread, NULL ) != 0 )
		return false;
#else
	Assert(0);
	DebuggerBreak();
#endif
	return true;
}

//-----------------------------------------------------------------------------
void ThreadSetDebugName( ThreadHandle_t hThread, const char *pszName )
{
#ifdef WIN32
	if ( Plat_IsInDebugSession() )
	{
#define MS_VC_EXCEPTION 0x406d1388

		typedef struct tagTHREADNAME_INFO
		{
			DWORD dwType;        // must be 0x1000
			LPCSTR szName;       // pointer to name (in same addr space)
			DWORD dwThreadID;    // thread ID (-1 caller thread)
			DWORD dwFlags;       // reserved for future use, most be zero
		} THREADNAME_INFO;

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = pszName;
		info.dwThreadID = LookupThreadIDFromHandle( hThread );

		if ( info.dwThreadID != 0 )
		{
			info.dwFlags = 0;

			__try
			{
				RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD), (ULONG_PTR *)&info);
			}
			__except (EXCEPTION_CONTINUE_EXECUTION)
			{
			}
		}
	}
#endif
}



//-----------------------------------------------------------------------------
// Used to thread LoadLibrary on the 360
//-----------------------------------------------------------------------------
static ThreadedLoadLibraryFunc_t s_ThreadedLoadLibraryFunc = 0;
PLATFORM_INTERFACE void SetThreadedLoadLibraryFunc( ThreadedLoadLibraryFunc_t func )
{
	s_ThreadedLoadLibraryFunc = func;
}

PLATFORM_INTERFACE ThreadedLoadLibraryFunc_t GetThreadedLoadLibraryFunc()
{
	return s_ThreadedLoadLibraryFunc;
}


//-----------------------------------------------------------------------------
//
//   CThreadSyncObject (note nothing uses this directly (I think) )
//
//-----------------------------------------------------------------------------


#ifdef _PS3
uint32_t CThreadSyncObject::m_bstaticMutexInitialized = false;
uint32_t CThreadSyncObject::m_bstaticMutexInitializing = false;
sys_lwmutex_t CThreadSyncObject::m_staticMutex;
#endif


CThreadSyncObject::CThreadSyncObject()
#ifdef _WIN32
  : m_hSyncObject( NULL ), m_bCreatedHandle(false)
#elif defined(POSIX) && !defined(PLATFORM_PS3)
  : m_bInitalized( false )
#endif
{
#ifdef _PS3
	//Do we nee to initialise the staticMutex?
	if (m_bstaticMutexInitialized) return;

	//If we are the first thread then create the mutex
	if ( cellAtomicCompareAndSwap32(&m_bstaticMutexInitializing, false, true) == false )
	{
		sys_lwmutex_attribute_t mutexAttr;
		sys_lwmutex_attribute_initialize( mutexAttr );
		mutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
		int err = sys_lwmutex_create( &m_staticMutex, &mutexAttr );
		Assert(err == CELL_OK);
		m_bstaticMutexInitialized = true;
	}
	else
	{
		//Another thread is already in the process of initialising the mutex, wait for it
		while ( !m_bstaticMutexInitialized )
		{
			// sys_ppu_thread_yield doesn't seem to function properly, so sleep instead.
			sys_timer_usleep( 60 );
		}
	}
#endif
}

//---------------------------------------------------------

CThreadSyncObject::~CThreadSyncObject()
{
#ifdef _WIN32
   if ( m_hSyncObject && m_bCreatedHandle )
   {
      if ( !CloseHandle(m_hSyncObject) )
	  {
		  Assert( 0 );
	  }
   }
#elif defined(POSIX) && !defined( PLATFORM_PS3 )
   if ( m_bInitalized )
   {
		pthread_cond_destroy( &m_Condition );
        pthread_mutex_destroy( &m_Mutex );
		m_bInitalized = false;
   }
#endif
}

//---------------------------------------------------------

bool CThreadSyncObject::operator!() const
{
#if PLATFORM_PS3
	return m_bstaticMutexInitialized;
#elif defined( _WIN32 ) 
   return !m_hSyncObject;
#elif defined(POSIX)
   return !m_bInitalized;
#endif
}

//---------------------------------------------------------

void CThreadSyncObject::AssertUseable()
{
#ifdef THREADS_DEBUG
#if PLATFORM_PS3
	AssertMsg( m_bstaticMutexInitialized, "Thread synchronization object is unuseable" );
#elif defined( _WIN32 )
   AssertMsg( m_hSyncObject, "Thread synchronization object is unuseable" );
#elif defined(POSIX)
   AssertMsg( m_bInitalized, "Thread synchronization object is unuseable" );
#endif
#endif
}

//---------------------------------------------------------

#if defined(_WIN32) || ( defined(POSIX) && !defined( _PS3 ) )
bool CThreadSyncObject::Wait( uint32 dwTimeout )
{
#ifdef THREADS_DEBUG
   AssertUseable();
#endif
#ifdef _WIN32
   return ( WaitForSingleObject( m_hSyncObject, dwTimeout ) == WAIT_OBJECT_0 );
#elif defined( POSIX ) && !defined( PLATFORM_PS3 )
    pthread_mutex_lock( &m_Mutex );
    bool bRet = false;
    if ( m_cSet > 0 )
    {
		bRet = true;
		m_bWakeForEvent = false;
    }
    else
    {
		volatile int ret = 0;

		while ( !m_bWakeForEvent && ret != ETIMEDOUT )
		{
			struct timeval tv;
			gettimeofday( &tv, NULL );
			volatile struct timespec tm;
			
			uint64 actualTimeout = dwTimeout;
			
			if ( dwTimeout == TT_INFINITE && m_bManualReset )
				actualTimeout = 10; // just wait 10 msec at most for manual reset events and loop instead
				
			volatile uint64 nNanoSec = (uint64)tv.tv_usec*1000 + (uint64)actualTimeout*1000000;
			tm.tv_sec = tv.tv_sec + nNanoSec /1000000000;
			tm.tv_nsec = nNanoSec % 1000000000;

			do
			{   
				ret = pthread_cond_timedwait( &m_Condition, &m_Mutex, (const timespec *)&tm );
			} 
			while( ret == EINTR );

			bRet = ( ret == 0 );
			
			if ( m_bManualReset )
			{
				if ( m_cSet )
					break;
				if ( dwTimeout == TT_INFINITE && ret == ETIMEDOUT )
					ret = 0; // force the loop to spin back around
			}
		}
		
		if ( bRet )
			m_bWakeForEvent = false;
    }
    if ( !m_bManualReset && bRet )
		m_cSet = 0;
    pthread_mutex_unlock( &m_Mutex );
    return bRet;
#endif
}
#endif

#ifdef _WIN32
#pragma optimize( "", off )	// The compiler optimizer screws up the function below.
#endif

uint32 CThreadSyncObject::WaitForMultiple( int nObjects, CThreadSyncObject **ppObjects, bool bWaitAll, uint32 dwTimeout )
{
#if defined( _WIN32 )
	
	CThreadSyncObject *pHandles = (CThreadSyncObject*)stackalloc( sizeof(CThreadSyncObject) * nObjects );
	for ( int i=0; i < nObjects; i++ )
	{
		pHandles[i].m_hSyncObject = ppObjects[i]->m_hSyncObject;
	}

	return WaitForMultiple( nObjects, pHandles, bWaitAll, dwTimeout );

#else
	
	// TODO: Need a more efficient implementation of this.
	uint32 dwStartTime = 0;
	
	if ( dwTimeout != TT_INFINITE )
		dwStartTime = Plat_MSTime();
	
	// If bWaitAll = true, then we need to track which ones were triggered.
	char *pWasTriggered = NULL;
	int nTriggered = 0;
	if ( bWaitAll )
	{
		pWasTriggered = (char*)stackalloc( nObjects );
		memset( pWasTriggered, 0, nObjects );
	}

	while ( 1 )
	{
		for ( int i=0; i < nObjects; i++ )
		{
			if ( bWaitAll && pWasTriggered[i] )
				continue;

#ifdef _PS3
			Assert( !"Not implemented!" );
			if ( false )
#else
			if ( ppObjects[i]->Wait( 0 ) )
#endif
			{
				++nTriggered;
				if ( bWaitAll )
				{
					if ( nTriggered == nObjects )
						return 0;
					else
						pWasTriggered[i] = 1;
				}
				else
				{
					return i;
				}
			}
		}

		// Timeout?
		if ( dwTimeout != TT_INFINITE )
		{
			if ( Plat_MSTime() - dwStartTime >= dwTimeout )
				return TW_TIMEOUT;
		}

		ThreadSleep( 0 );
	}

#endif
}

#ifdef _WIN32
#pragma optimize( "", on )
#endif 

uint32 CThreadSyncObject::WaitForMultiple( int nObjects, CThreadSyncObject *pObjects, bool bWaitAll, uint32 dwTimeout )
{
#if defined(_WIN32 )

	HANDLE *pHandles = (HANDLE*)stackalloc( sizeof(HANDLE) * nObjects );
	for ( int i=0; i < nObjects; i++ )
	{
		pHandles[i] = pObjects[i].m_hSyncObject;
	}
	
	DWORD ret = WaitForMultipleObjects( nObjects, pHandles, bWaitAll, dwTimeout );
	if ( ret == WAIT_TIMEOUT )
		return TW_TIMEOUT;
	else if ( ret >= WAIT_OBJECT_0 && (ret-WAIT_OBJECT_0) < (uint32)nObjects )
		return (int)(ret - WAIT_OBJECT_0);
	else if ( ret >= WAIT_ABANDONED_0 && (ret - WAIT_ABANDONED_0) < (uint32)nObjects )
		Error( "Unhandled WAIT_ABANDONED in WaitForMultipleObjects" );
	else if ( ret == WAIT_FAILED )
		return TW_FAILED;
	else
		Error( "Unknown return value (%lu) from WaitForMultipleObjects", ret );

	// We'll never get here..
	return 0;

#else

	CThreadSyncObject **ppObjects = (CThreadSyncObject**)stackalloc( sizeof( CThreadSyncObject* ) * nObjects );
	for ( int i=0; i < nObjects; i++ )
	{
		ppObjects[i] = &pObjects[i];
	}

	return WaitForMultiple( nObjects, ppObjects, bWaitAll, dwTimeout );

#endif
}

// To implement these, I need to check that casts are safe
uint32 CThreadEvent::WaitForMultiple( int nObjects, CThreadEvent *pObjects, bool bWaitAll, uint32 dwTimeout )
{
	// If data ever gets added to CThreadEvent, then we need a different implementation.
#ifdef _PS3
	CThreadEvent **ppObjects = (CThreadEvent**)stackalloc( sizeof( CThreadEvent* ) * nObjects );
	for ( int i=0; i < nObjects; i++ )
	{
		ppObjects[i] = &pObjects[i];
	}
	return WaitForMultipleObjects( nObjects, ppObjects, bWaitAll, dwTimeout );
#else
	COMPILE_TIME_ASSERT( sizeof( CThreadSyncObject ) == 0 || sizeof( CThreadEvent ) == sizeof( CThreadSyncObject ) );
	return CThreadSyncObject::WaitForMultiple( nObjects, (CThreadSyncObject*)pObjects, bWaitAll, dwTimeout );
#endif
}


uint32 CThreadEvent::WaitForMultiple( int nObjects, CThreadEvent **ppObjects, bool bWaitAll, uint32 dwTimeout )
{
#ifdef _PS3
	return WaitForMultipleObjects( nObjects, ppObjects, bWaitAll, dwTimeout );
#else
	// If data ever gets added to CThreadEvent, then we need a different implementation.
	COMPILE_TIME_ASSERT( sizeof( CThreadSyncObject )== 0 || sizeof( CThreadEvent ) == sizeof( CThreadSyncObject ) );
	return CThreadSyncObject::WaitForMultiple( nObjects, (CThreadSyncObject**)ppObjects, bWaitAll, dwTimeout );
#endif
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadEvent::CThreadEvent( bool bManualReset )
{
#ifdef _WIN32
    m_hSyncObject = CreateEvent( NULL, bManualReset, FALSE, NULL );
	m_bCreatedHandle = true;
    AssertMsg1(m_hSyncObject, "Failed to create event (error 0x%x)", GetLastError() );
#elif defined( _PS3 )

	m_bManualReset = bManualReset;
	m_bSet = 0;
	m_bInitalized = false;
	m_numWaitingThread = 0;

	// set up linked list of wait objects

	memset(&m_waitObjects[0], 0, sizeof(m_waitObjects));
	m_pWaitObjectsList = &m_waitObjects[0];
	m_pWaitObjectsPool = &m_waitObjects[1];

	for (int i = 2; i < CTHREADEVENT_MAX_WAITING_THREADS + 2; i++)
	{
		LLLinkNode(m_pWaitObjectsPool, &m_waitObjects[i]);
	}
#elif defined( POSIX )
    pthread_mutexattr_t Attr;
    pthread_mutexattr_init( &Attr );
    pthread_mutex_init( &m_Mutex, &Attr );
    pthread_mutexattr_destroy( &Attr );
    pthread_cond_init( &m_Condition, NULL );
    m_bInitalized = true;
    m_cSet = 0;
	m_bWakeForEvent = false;
    m_bManualReset = bManualReset;
#else
#error "Implement me"
#endif
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

#ifdef _PS3

//
// linked list functionality
//

//-----------------------------------------------------------------------------
// Purpose: Linked list implementation
//-----------------------------------------------------------------------------

CThreadEventWaitObject* CThreadEvent::LLUnlinkNode(CThreadEventWaitObject *node)
{
	// <sergiy> Note: if you have a null-access crash here, it may mean that CTHREADEVENT_MAX_WAITING_THREADS is not high enough
	//                and the linked list pool is simply exhausted
	node->m_pPrev->m_pNext = node->m_pNext;
	if (node->m_pNext) node->m_pNext->m_pPrev = node->m_pPrev;
	node->m_pNext = node->m_pPrev = NULL;

	return node;
}

CThreadEventWaitObject* CThreadEvent::LLLinkNode(CThreadEventWaitObject* list, CThreadEventWaitObject *node)
{
	node->m_pNext = list->m_pNext;
	if (node->m_pNext)
	{
		node->m_pNext->m_pPrev = node;
	}

	list->m_pNext = node;
	node->m_pPrev = list;

	return node;
}

//-----------------------------------------------------------------------------
// Helper function to atomically write index into destination and set semaphore
// This is used by WaitForMultipleObjects(WAIT_ANY) because once the semaphore
// is set, the waiting thread also needs to know which event triggered it
// We do NOT need this to be atomic because if a number of events fire it doesn't
// matter which one of these we pick
//-----------------------------------------------------------------------------
void CThreadEventWaitObject::Set()
{
	*m_pFlag = m_index;
	sys_semaphore_post(*m_pSemaphore, 1);	
}

//
// CThreadEvent::RegisterWaitingThread
//
void CThreadEvent::RegisterWaitingThread(sys_semaphore_t *pSemaphore, int index, int *flag)
{
	sys_lwmutex_lock(&m_staticMutex, 0);

	// if we are already set, then signal this semaphore
	if (m_bSet)
	{
		CThreadEventWaitObject waitObject;
		waitObject.Init(pSemaphore, index, flag);
		waitObject.Set();

		if (!m_bManualReset)
		{
			m_bSet = false;
		}
	}
	else
	{
		if (!m_pWaitObjectsPool->m_pNext)
		{
			DEBUG_ERROR("CThreadEvent: Ran out of events; cannot register waiting thread\n");
		}

		// add this semaphore to linked list - can be added more than once it doesn't matter

		CThreadEventWaitObject *pWaitObject = LLUnlinkNode(m_pWaitObjectsPool->m_pNext);

		pWaitObject->Init(pSemaphore, index, flag);

		LLLinkNode(m_pWaitObjectsList, pWaitObject);
	}

	sys_lwmutex_unlock(&m_staticMutex);
}

//
// CThreadEvent::UnregisterWaitingThread
//
void CThreadEvent::UnregisterWaitingThread(sys_semaphore_t *pSemaphore)
{
	// remove all instances of this semaphore from linked list

	sys_lwmutex_lock(&m_staticMutex, 0);

	CThreadEventWaitObject *pWaitObject = m_pWaitObjectsList->m_pNext;

	while (pWaitObject)
	{
		CThreadEventWaitObject *pNext = pWaitObject->m_pNext;

		if (pWaitObject->m_pSemaphore == pSemaphore)
		{
			LLUnlinkNode(pWaitObject);
			LLLinkNode(m_pWaitObjectsPool, pWaitObject);
		}

		pWaitObject = pNext;
	}

	sys_lwmutex_unlock(&m_staticMutex);
}

#endif // _PS3


#ifdef PLATFORM_WINDOWS
	CThreadEvent::CThreadEvent( const char *name, bool initialState, bool bManualReset )
	{
		m_hSyncObject = CreateEvent( NULL, bManualReset, (BOOL) initialState, name );
		AssertMsg1( m_hSyncObject, "Failed to create event (error 0x%x)", GetLastError() );
	}


	NamedEventResult_t CThreadEvent::CheckNamedEvent( const char *name, uint32 dwTimeout )
	{
		HANDLE eHandle = OpenEvent( SYNCHRONIZE, FALSE, name );
		
		if ( eHandle == NULL ) return TT_EventDoesntExist;
		
		DWORD result = WaitForSingleObject( eHandle, dwTimeout );
		
		return ( result == WAIT_OBJECT_0 ) ?  TT_EventSignaled : TT_EventNotSignaled;
	}
#endif 

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------


//---------------------------------------------------------

bool CThreadEvent::Set()
{
//////////////////////////////////////////////////////////////
#ifndef NEW_WAIT_FOR_MULTIPLE_OBJECTS
//////////////////////////////////////////////////////////////
   AssertUseable();
#ifdef _WIN32
   return ( SetEvent( m_hSyncObject ) != 0 );
#elif defined( _PS3 )

   sys_lwmutex_lock(&m_staticMutex, 0);
   
   if (m_bManualReset)
	{
		//Mark event as set
		m_bSet = true;
		
		//If any threads are already waiting then signal them to run
		if (m_bInitalized)
		{
			int err = sys_semaphore_post( m_Semaphore, m_numWaitingThread);
			Assert(err == CELL_OK);
		}
	}
	else
	{
		//If any threads are already waiting then signal ONE to run, else signal next to run

		if (m_numWaitingThread>0)
		{
			int err = sys_semaphore_post( m_Semaphore, 1);
			Assert(err == CELL_OK);
		}
		else
		{
			m_bSet=true;
		}
	}

   sys_lwmutex_unlock(&m_staticMutex);

	return true;


#elif defined(POSIX)
   pthread_mutex_lock( &m_Mutex );
	m_cSet = 1;
	m_bWakeForEvent = true;
    int ret = pthread_cond_signal( &m_Condition );
   pthread_mutex_unlock( &m_Mutex );
   return ret == 0;
#endif


//////////////////////////////////////////////////////////////
#else	// NEW_WAIT_FOR_MULTIPLE_OBJECTS
//////////////////////////////////////////////////////////////

	sys_lwmutex_lock(&m_staticMutex, 0);

	//Mark event as set
	m_bSet = true;

	// signal registered semaphores
	while (m_pWaitObjectsList->m_pNext)
	{
	   CThreadEventWaitObject *pWaitObject = LLUnlinkNode(m_pWaitObjectsList->m_pNext);
	   
	   pWaitObject->Set();

	   LLLinkNode(m_pWaitObjectsPool, pWaitObject);

	   if (!m_bManualReset)
	   {
		   m_bSet = false;
		   break;
	   }
	}

	sys_lwmutex_unlock(&m_staticMutex);

	return true;

//////////////////////////////////////////////////////////////
#endif	// NEW_WAIT_FOR_MULTIPLE_OBJECTS
//////////////////////////////////////////////////////////////
}

//---------------------------------------------------------

bool CThreadEvent::Reset()
{
#ifdef THREADS_DEBUG
   AssertUseable();
#endif
#ifdef _WIN32
   return ( ResetEvent( m_hSyncObject ) != 0 );
#elif defined( _PS3 )

   //Just mark us as no longer signaled
   m_bSet = 0;

   return true;
#elif defined(POSIX)
	pthread_mutex_lock( &m_Mutex );
	m_cSet = 0;
	m_bWakeForEvent = false;
	pthread_mutex_unlock( &m_Mutex );
	return true; 
#endif
}

//---------------------------------------------------------

bool CThreadEvent::Check()
{
	#ifdef _PS3
	return m_bSet; // Please, use for debugging only!
	#endif
#ifdef THREADS_DEBUG
   AssertUseable();
#endif
	return Wait( 0 );
}



bool CThreadEvent::Wait( uint32 dwTimeout )
{
//////////////////////////////////////////////////////////////
#ifndef NEW_WAIT_FOR_MULTIPLE_OBJECTS
//////////////////////////////////////////////////////////////


#if defined( _WIN32 ) || ( defined( POSIX ) && !defined( _PS3 ) )
	return CThreadSyncObject::Wait( dwTimeout );
#elif defined( _PS3 )

	{

		if (dwTimeout == 0)
		{
			//If timeout is 0 then just test it now (and reset it if manual )
			if (m_bSet)
			{
				if ( !m_bManualReset ) m_bSet=false;
				return true;
			}
			return false;
		}

		if (!AddWaitingThread())
		{
			//Waiting thread NOT added because m_bSet was already set
			if ( !m_bManualReset ) m_bSet=false;
			return true;
		}		
				
		uint32 timeout;
		int countTimeout = 0;
		int ret = ETIMEDOUT;
		while ( timeout=MIN(1, dwTimeout) )
		{
			// on the PS3, "infinite timeout" is specified by zero, not 
			// 0xFFFFFFFF, so we need to perform that ternary here.
#error Untested code:
			ret = sys_semaphore_wait( m_Semaphore, timeout == TT_INFINITE ? 0 : timeout * 1000 );
			Assert( (ret == CELL_OK) || (ret == ETIMEDOUT) );
			
			if ( ret == CELL_OK )
				break;

			dwTimeout -= timeout;
			countTimeout++;
			if (countTimeout > 30)
			{
				printf("WARNING: possible deadlock in CThreadEvent::Wait() !!!\n");
			}
		}

		RemoveWaitingThread();

		if ( !m_bManualReset ) m_bSet=false;

		return ret == CELL_OK;
	}

#endif

//////////////////////////////////////////////////////////////
#else	// NEW_WAIT_FOR_MULTIPLE_OBJECTS
//////////////////////////////////////////////////////////////


	CThreadEvent *pThis = this;
	DWORD res = WaitForMultipleObjects(1, &pThis, true, dwTimeout);
	return res == WAIT_OBJECT_0;


//////////////////////////////////////////////////////////////
#endif	// NEW_WAIT_FOR_MULTIPLE_OBJECTS
//////////////////////////////////////////////////////////////
}

#ifdef _WIN32
//-----------------------------------------------------------------------------
//
// CThreadSemaphore
//
// To get Posix implementation, try http://www-128.ibm.com/developerworks/eserver/library/es-win32linux-sem.html
//
//-----------------------------------------------------------------------------

CThreadSemaphore::CThreadSemaphore( int32 initialValue, int32 maxValue )
{
#ifdef _WIN32
	if ( maxValue )
	{
		AssertMsg( maxValue > 0, "Invalid max value for semaphore" );
		AssertMsg( initialValue >= 0 && initialValue <= maxValue, "Invalid initial value for semaphore" );

		m_hSyncObject = CreateSemaphore( NULL, initialValue, maxValue, NULL );

		AssertMsg1(m_hSyncObject, "Failed to create semaphore (error 0x%x)", GetLastError());
	}
	else
	{
		m_hSyncObject = NULL;
	}
#elif defined( _PS3 )
	if ( maxValue )
	{
		m_sema_max_val = maxValue;
		m_semaCount = initialValue;
	}
#endif
}


#ifdef _PS3
//---------------------------------------------------------

bool CThreadSemaphore::AddWaitingThread()
{
	bool result;

	sys_lwmutex_lock(&m_staticMutex, 0);

	if (cellAtomicTestAndDecr32(&m_semaCount) > 0)
	{
		result=false;
	}
	else
	{
		result=true;
		m_numWaitingThread++;

		if ( m_numWaitingThread == 1 )
		{
			sys_semaphore_attribute_t semAttr;
			sys_semaphore_attribute_initialize( semAttr );
			Assert(m_semaCount == 0);
			int err = sys_semaphore_create( &m_Semaphore, &semAttr, 0, m_sema_max_val );
			Assert( err == CELL_OK );
			m_bInitalized = true;
		}
	}

	sys_lwmutex_unlock(&m_staticMutex);
	return result;
}

void CThreadSemaphore::RemoveWaitingThread()
{
	sys_lwmutex_lock(&m_staticMutex, 0);

	m_numWaitingThread--;

	if ( m_numWaitingThread == 0)
	{
		int err = sys_semaphore_destroy( m_Semaphore );
		Assert( err == CELL_OK );
		m_bInitalized = false;
	}

	sys_lwmutex_unlock(&m_staticMutex);
}

#endif

#ifdef _PS3

bool CThreadSemaphore::Wait( uint32 dwTimeout )
{
#ifdef THREADS_DEBUG
	AssertUseable();
#endif


#ifndef NO_THREAD_SYNC
	if (!AddWaitingThread())
	{
		//Waiting thread NOT added because semaphore was already in a signaled state
		return true;
	}

	int ret = sys_semaphore_wait( m_Semaphore, dwTimeout == TT_INFINITE ? 0 : dwTimeout * 1000 );
	Assert( (ret == CELL_OK) || (ret == ETIMEDOUT) );

	RemoveWaitingThread();

	int old = cellAtomicDecr32(&m_semaCount);
	Assert(old>0);
#else
	int ret = CELL_OK;
#endif

	// sys_ppu_thread_yield doesn't seem to function properly, so sleep instead.
	sys_timer_usleep( 60 );



	return ret == CELL_OK;
}

#endif

//---------------------------------------------------------

bool CThreadSemaphore::Release( int32 releaseCount, int32 *pPreviousCount )
{
#ifdef THRDTOOL_DEBUG
   AssertUseable();
#endif
#ifdef _WIN32
   return ( ReleaseSemaphore( m_hSyncObject, releaseCount, (LPLONG)pPreviousCount ) != 0 );
#elif defined( _PS3 )

#ifndef NO_THREAD_SYNC

   if (m_bInitalized)
   {
	   sys_semaphore_value_t previousVal;
	   sys_semaphore_get_value( m_Semaphore, &previousVal );

	   cellAtomicAdd32(&m_semaCount, releaseCount);

	   *pPreviousCount = previousVal;

	   int err = sys_semaphore_post( m_Semaphore, releaseCount );
	   Assert(err == CELL_OK);
   }

#endif

   return true;
#endif
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadFullMutex::CThreadFullMutex( bool bEstablishInitialOwnership, const char *pszName )
{
   m_hSyncObject = CreateMutex( NULL, bEstablishInitialOwnership, pszName );

   AssertMsg1( m_hSyncObject, "Failed to create mutex (error 0x%x)", GetLastError() );
}

//---------------------------------------------------------

bool CThreadFullMutex::Release()
{
#ifdef THRDTOOL_DEBUG
   AssertUseable();
#endif
   return ( ReleaseMutex( m_hSyncObject ) != 0 );
}

#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

#if ( defined(WIN32) || defined(OSX) ) && !defined(PLATFORM_PS3) 
namespace GenericThreadLocals
{
CThreadLocalBase::CThreadLocalBase()
{
#ifdef _WIN32
	m_index = TlsAlloc();
	AssertMsg( m_index != 0xFFFFFFFF, "Bad thread local" );
	if ( m_index == 0xFFFFFFFF )
		Error( "Out of thread local storage!\n" );
#elif defined(POSIX)
	if ( pthread_key_create(  (pthread_key_t *)&m_index, NULL ) != 0 )
		Error( "Out of thread local storage!\n" );
#endif
}

//---------------------------------------------------------

CThreadLocalBase::~CThreadLocalBase()
{
#ifdef _WIN32
	if ( m_index != 0xFFFFFFFF )
		TlsFree( m_index );
	m_index = 0xFFFFFFFF;
#elif defined(POSIX)
	pthread_key_delete( m_index );
#endif
}

//---------------------------------------------------------

void * CThreadLocalBase::Get() const
{
#ifdef _WIN32
	if ( m_index != 0xFFFFFFFF )
		return TlsGetValue( m_index );
	AssertMsg( 0, "Bad thread local" );
	return NULL;
#elif defined(POSIX)
	void *value = pthread_getspecific( m_index );
	return value;
#endif
}

//---------------------------------------------------------

void CThreadLocalBase::Set( void *value )
{
#ifdef _WIN32
	if (m_index != 0xFFFFFFFF)
		TlsSetValue(m_index, value);
	else
		AssertMsg( 0, "Bad thread local" );
#elif defined(POSIX)
	if ( pthread_setspecific( m_index, value ) != 0 )
		AssertMsg( 0, "Bad thread local" );
#endif
}
} // namespace GenericThreadLocals
#endif // ( defined(WIN32) || defined(POSIX) ) && !defined(PLATFORM_PS3) 
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

#ifdef MSVC
//#ifdef _X360
#define TO_INTERLOCK_PARAM(p)		((volatile long *)p)
#define TO_INTERLOCK_PTR_PARAM(p)	((void **)p)
//#else
//#define TO_INTERLOCK_PARAM(p)		(p)
//#define TO_INTERLOCK_PTR_PARAM(p)	(p)
//#endif

#if !defined(USE_INTRINSIC_INTERLOCKED) && !defined(_X360)
int32 ThreadInterlockedIncrement( int32 volatile *pDest )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedIncrement( TO_INTERLOCK_PARAM(pDest) );
}

int32 ThreadInterlockedDecrement( int32 volatile *pDest )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedDecrement( TO_INTERLOCK_PARAM(pDest) );
}

int32 ThreadInterlockedExchange( int32 volatile *pDest, int32 value )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedExchange( TO_INTERLOCK_PARAM(pDest), value );
}

int32 ThreadInterlockedExchangeAdd( int32 volatile *pDest, int32 value )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedExchangeAdd( TO_INTERLOCK_PARAM(pDest), value );
}

int32 ThreadInterlockedCompareExchange( int32 volatile *pDest, int32 value, int32 comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedCompareExchange( TO_INTERLOCK_PARAM(pDest), value, comperand );
}

bool ThreadInterlockedAssignIf( int32 volatile *pDest, int32 value, int32 comperand )
{
	Assert( (size_t)pDest % 4 == 0 );

#if !(defined(_WIN64) || defined (_X360))
	__asm 
	{
		mov	eax,comperand
		mov	ecx,pDest
		mov edx,value
		lock cmpxchg [ecx],edx 
		mov eax,0
		setz al
	}
#else
	return ( InterlockedCompareExchange( TO_INTERLOCK_PARAM(pDest), value, comperand ) == comperand );
#endif
}

#endif

#if !defined( USE_INTRINSIC_INTERLOCKED ) || defined( _WIN64 )
void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedExchangePointer( TO_INTERLOCK_PTR_PARAM(pDest), value );
}

void *ThreadInterlockedCompareExchangePointer( void * volatile *pDest, void *value, void *comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedCompareExchangePointer( TO_INTERLOCK_PTR_PARAM(pDest), value, comperand );
}

bool ThreadInterlockedAssignPointerIf( void * volatile *pDest, void *value, void *comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
#if !(defined(_WIN64) || defined (_X360))
	__asm 
	{
		mov	eax,comperand
		mov	ecx,pDest
		mov edx,value
		lock cmpxchg [ecx],edx 
		mov eax,0
		setz al
	}
#else
	return ( InterlockedCompareExchangePointer( TO_INTERLOCK_PTR_PARAM(pDest), value, comperand ) == comperand );
#endif
}
#endif

int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
	Assert( (size_t)pDest % 8 == 0 );

#if defined(_WIN64) || defined (_X360)
	return InterlockedCompareExchange64( pDest, value, comperand );
#else
	__asm 
	{
		lea esi,comperand;
		lea edi,value;

		mov eax,[esi];
		mov edx,4[esi];
		mov ebx,[edi];
		mov ecx,4[edi];
		mov esi,pDest;
		lock CMPXCHG8B [esi];			
	}
#endif
}

bool ThreadInterlockedAssignIf64(volatile int64 *pDest, int64 value, int64 comperand ) 
{
	Assert( (size_t)pDest % 8 == 0 );

#if defined(_X360) || defined(_WIN64)
	return ( ThreadInterlockedCompareExchange64( pDest, value, comperand ) == comperand ); 
#else
	__asm
	{
		lea esi,comperand;
		lea edi,value;

		mov eax,[esi];
		mov edx,4[esi];
		mov ebx,[edi];
		mov ecx,4[edi];
		mov esi,pDest;
		lock CMPXCHG8B [esi];			
		mov eax,0;
		setz al;
	}
#endif
}

#elif defined(GNUC)

#ifdef OSX
#include <libkern/OSAtomic.h>
#endif


long ThreadInterlockedIncrement( long volatile *pDest )
{
	return __sync_fetch_and_add( pDest, 1 ) + 1;
}

long ThreadInterlockedDecrement( long volatile *pDest )
{
	return __sync_fetch_and_sub( pDest, 1 ) - 1;
}

long ThreadInterlockedExchange( long volatile *pDest, long value )
{
	return __sync_lock_test_and_set( pDest, value );
}

long ThreadInterlockedExchangeAdd( long volatile *pDest, long value )
{
	return  __sync_fetch_and_add( pDest, value );
}

long ThreadInterlockedCompareExchange( long volatile *pDest, long value, long comperand )
{
	return  __sync_val_compare_and_swap( pDest, comperand, value );
}

bool ThreadInterlockedAssignIf( long volatile *pDest, long value, long comperand )
{
	return __sync_bool_compare_and_swap( pDest, comperand, value );
}

#if !defined( USE_INTRINSIC_INTERLOCKED ) 
void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	return __sync_lock_test_and_set( pDest, value );
}

void *ThreadInterlockedCompareExchangePointer( void *volatile *pDest, void *value, void *comperand )
{	
	return  __sync_val_compare_and_swap( pDest, comperand, value );
}

bool ThreadInterlockedAssignPointerIf( void * volatile *pDest, void *value, void *comperand )
{
	return  __sync_bool_compare_and_swap( pDest, comperand, value );
}
#endif

int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
#if defined(OSX)
	int64 retVal = *pDest;
	if ( OSAtomicCompareAndSwap64( comperand, value, pDest ) )
		retVal = *pDest;
	
	return retVal;
#else
	return __sync_val_compare_and_swap( pDest, comperand, value  );
#endif
}

bool ThreadInterlockedAssignIf64( int64 volatile * pDest, int64 value, int64 comperand ) 
{
	return __sync_bool_compare_and_swap( pDest, comperand, value );
}


#elif defined( _PS3 )

// This is defined in the header!

#else
// This will perform horribly,
#error "Falling back to mutexed interlocked operations, you really don't have intrinsics you can use?"ß
CThreadMutex g_InterlockedMutex;

long ThreadInterlockedIncrement( long volatile *pDest )
{
	AUTO_LOCK( g_InterlockedMutex );
	return ++(*pDest);
}

long ThreadInterlockedDecrement( long volatile *pDest )
{
	AUTO_LOCK( g_InterlockedMutex );
	return --(*pDest);
}

long ThreadInterlockedExchange( long volatile *pDest, long value )
{
	AUTO_LOCK( g_InterlockedMutex );
	long retVal = *pDest;
	*pDest = value;
	return retVal;
}

void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	AUTO_LOCK( g_InterlockedMutex );
	void *retVal = *pDest;
	*pDest = value;
	return retVal;
}

long ThreadInterlockedExchangeAdd( long volatile *pDest, long value )
{
	AUTO_LOCK( g_InterlockedMutex );
	long retVal = *pDest;
	*pDest += value;
	return retVal;
}

long ThreadInterlockedCompareExchange( long volatile *pDest, long value, long comperand )
{
	AUTO_LOCK( g_InterlockedMutex );
	long retVal = *pDest;
	if ( *pDest == comperand )
		*pDest = value;
	return retVal;
}

void *ThreadInterlockedCompareExchangePointer( void * volatile *pDest, void *value, void *comperand )
{
	AUTO_LOCK( g_InterlockedMutex );
	void *retVal = *pDest;
	if ( *pDest == comperand )
		*pDest = value;
	return retVal;
}


int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
	Assert( (size_t)pDest % 8 == 0 );
	AUTO_LOCK( g_InterlockedMutex );
	int64 retVal = *pDest;
	if ( *pDest == comperand )
		*pDest = value;
	return retVal;
}

#endif

int64 ThreadInterlockedExchange64( int64 volatile *pDest, int64 value )
{
	Assert( (size_t)pDest % 8 == 0 );
	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);

	return Old;
}


//-----------------------------------------------------------------------------

#if defined(_WIN32) && defined(THREAD_PROFILER)
void ThreadNotifySyncNoop(void *p) {}

#define MAP_THREAD_PROFILER_CALL( from, to ) \
	void from(void *p) \
	{ \
		static CDynamicFunction<void (*)(void *)> dynFunc( "libittnotify.dll", #to, ThreadNotifySyncNoop ); \
		(*dynFunc)(p); \
	}

MAP_THREAD_PROFILER_CALL( ThreadNotifySyncPrepare, __itt_notify_sync_prepare );
MAP_THREAD_PROFILER_CALL( ThreadNotifySyncCancel, __itt_notify_sync_cancel );
MAP_THREAD_PROFILER_CALL( ThreadNotifySyncAcquired, __itt_notify_sync_acquired );
MAP_THREAD_PROFILER_CALL( ThreadNotifySyncReleasing, __itt_notify_sync_releasing );

#endif

//-----------------------------------------------------------------------------
//
// CThreadMutex
//
//-----------------------------------------------------------------------------

#ifdef _PS3
CThreadMutex::CThreadMutex()
{
	// sys_mutex with recursion enabled is like a win32 critical section
	sys_mutex_attribute_t mutexAttr;
	sys_mutex_attribute_initialize( mutexAttr );
	mutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
	sys_mutex_create( &m_Mutex, &mutexAttr );
}
CThreadMutex::~CThreadMutex()
{
	sys_mutex_destroy( m_Mutex );
}
#elif !defined( POSIX )
CThreadMutex::CThreadMutex()
{
#ifdef THREAD_MUTEX_TRACING_ENABLED
	memset( &m_CriticalSection, 0, sizeof(m_CriticalSection) );
#endif
	InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION *)&m_CriticalSection, 4000);
#ifdef THREAD_MUTEX_TRACING_SUPPORTED
	// These need to be initialized unconditionally in case mixing release & debug object modules
	// Lock and unlock may be emitted as COMDATs, in which case may get spurious output
	m_currentOwnerID = m_lockCount = 0;
	m_bTrace = false;
#endif
}

CThreadMutex::~CThreadMutex()
{
	DeleteCriticalSection((CRITICAL_SECTION *)&m_CriticalSection);
}
#endif // !POSIX

#ifdef IS_WINDOWS_PC
typedef BOOL (WINAPI*TryEnterCriticalSectionFunc_t)(LPCRITICAL_SECTION);
static CDynamicFunction<TryEnterCriticalSectionFunc_t> DynTryEnterCriticalSection( "Kernel32.dll", "TryEnterCriticalSection" );
#elif defined( _X360 )
#define DynTryEnterCriticalSection TryEnterCriticalSection
#endif

bool CThreadMutex::TryLock()
{
#if defined( MSVC )
#ifdef THREAD_MUTEX_TRACING_ENABLED
	uint thisThreadID = ThreadGetCurrentId();
	if ( m_bTrace && m_currentOwnerID && ( m_currentOwnerID != thisThreadID ) )
		Msg( "Thread %u about to try-wait for lock %x owned by %u\n", ThreadGetCurrentId(), (CRITICAL_SECTION *)&m_CriticalSection, m_currentOwnerID );
#endif
	if ( DynTryEnterCriticalSection != NULL )
	{
		if ( (*DynTryEnterCriticalSection )( (CRITICAL_SECTION *)&m_CriticalSection ) != FALSE )
		{
#ifdef THREAD_MUTEX_TRACING_ENABLED
			if (m_lockCount == 0)
			{
				// we now own it for the first time.  Set owner information
				m_currentOwnerID = thisThreadID;
				if ( m_bTrace )
					Msg( "Thread %u now owns lock 0x%x\n", m_currentOwnerID, (CRITICAL_SECTION *)&m_CriticalSection );
			}
			m_lockCount++;
#endif
			return true;
		}
		return false;
	}
	Lock();
	return true;
#elif defined( _PS3 )

#ifndef NO_THREAD_SYNC
	if ( sys_mutex_trylock( m_Mutex ) == CELL_OK )
#endif

		return true;

	return false; // ?? moved from EA code

#elif defined( POSIX )
	return pthread_mutex_trylock( &m_Mutex ) == 0;
#else
#error "Implement me!"
	return true;
#endif
}

//-----------------------------------------------------------------------------
//
// CThreadFastMutex
//
//-----------------------------------------------------------------------------

#ifdef THREAD_FAST_MUTEX_TIMINGS
// This is meant to be used in combination with breakpoints and in-debugee, so we turn the optimizer off
#pragma optimize( "", off )
CThreadFastMutex *g_pIgnoredMutexes[256]; // Ignore noisy non-problem mutex. Probably could be an array. Right now needed only for sound thread
float g_MutexTimingTolerance = 5;
bool g_bMutexTimingOutput;

void TrapMutexTimings( uint32 probableBlocker, uint32 thisThread, volatile CThreadFastMutex *pMutex, CFastTimer &spikeTimer, CAverageCycleCounter &sleepTimer )
{
	spikeTimer.End();
	if ( spikeTimer.GetDuration().GetMillisecondsF() > g_MutexTimingTolerance )
	{	
		bool bIgnore = false;
		for ( int j = 0; j < ARRAYSIZE( g_pIgnoredMutexes ) && g_pIgnoredMutexes[j]; j++ )
		{
			if ( g_pIgnoredMutexes[j] == pMutex )
			{
				bIgnore = true;
				break;
			}
		}

		if ( !bIgnore && spikeTimer.GetDuration().GetMillisecondsF() < 100 )
		{
			volatile float FastMutexDuration = spikeTimer.GetDuration().GetMillisecondsF();
			volatile float average = sleepTimer.GetAverageMilliseconds();
			volatile float peak = sleepTimer.GetPeakMilliseconds();			volatile int xx = 6;
			if ( g_bMutexTimingOutput )
			{
				char szBuf[256];
				Msg( "M (%.8x): [%.8x <-- %.8x] (%f,%f,%f)\n", pMutex, probableBlocker, thisThread, FastMutexDuration, average, peak );
			}
		}
	}
}

#else
#define TrapMutexTimings( a, b, c, d, e ) ((void)0)
#endif

//-------------------------------------

#define THREAD_SPIN (8*1024)

void CThreadFastMutex::Lock( const uint32 threadId, unsigned nSpinSleepTime ) volatile 
{
#ifdef THREAD_FAST_MUTEX_TIMINGS
	CAverageCycleCounter sleepTimer;
	CFastTimer spikeTimer;
	uint32 currentOwner = m_ownerID;
	spikeTimer.Start();
	sleepTimer.Init();
#endif
	int i;
	if ( nSpinSleepTime != TT_INFINITE )
	{
		for ( i = THREAD_SPIN; i != 0; --i )
		{
			if ( TryLock( threadId ) )
			{
				TrapMutexTimings( currentOwner, threadId, this, spikeTimer, sleepTimer );
				return;
			}
			ThreadPause();
		}

		for ( i = THREAD_SPIN; i != 0; --i )
		{
			if ( TryLock( threadId ) )
			{
				TrapMutexTimings( currentOwner, threadId, this, spikeTimer, sleepTimer );
				return;
			}
			ThreadPause();
			if ( i % 1024 == 0 )
			{
#ifdef THREAD_FAST_MUTEX_TIMINGS
				CAverageTimeMarker marker( &sleepTimer );
#endif
				ThreadSleep( 0 );
			}
		}

#ifdef _WIN32
		if ( !nSpinSleepTime && GetThreadPriority( GetCurrentThread() ) > THREAD_PRIORITY_NORMAL )
		{
			nSpinSleepTime = 1;
		} 
#endif

		if ( nSpinSleepTime )
		{
			for ( i = THREAD_SPIN; i != 0; --i )
			{
#ifdef THREAD_FAST_MUTEX_TIMINGS
				CAverageTimeMarker marker( &sleepTimer );
#endif
				if ( TryLock( threadId ) )
				{
					TrapMutexTimings( currentOwner, threadId, this, spikeTimer, sleepTimer );
					return;
				}

				ThreadPause();
				ThreadSleep( 0 );
			}

		}

		for ( ;; )
		{
#ifdef THREAD_FAST_MUTEX_TIMINGS
			CAverageTimeMarker marker( &sleepTimer );
#endif
			if ( TryLock( threadId ) )
			{
				TrapMutexTimings( currentOwner, threadId, this, spikeTimer, sleepTimer );
				return;
			}

			ThreadPause();
			ThreadSleep( nSpinSleepTime );
		}
	}
	else
	{
		for ( ;; )
		{
			if ( TryLock( threadId ) )
			{
				TrapMutexTimings( currentOwner, threadId, this, spikeTimer, sleepTimer );
				return;
			}

			ThreadPause();
		}
	}
}

#ifdef THREAD_FAST_MUTEX_TIMINGS
#pragma optimize( "", on )
#endif

//-----------------------------------------------------------------------------
//
// CThreadRWLock
//
//-----------------------------------------------------------------------------

void CThreadRWLock::WaitForRead()
{
	m_nPendingReaders++;

	do
	{
		m_mutex.Unlock();
		m_CanRead.Wait();
		m_mutex.Lock();
	}
	while (m_nWriters);

	m_nPendingReaders--;
}


void CThreadRWLock::LockForWrite()
{
	m_mutex.Lock();
	bool bWait = ( m_nWriters != 0 || m_nActiveReaders != 0 );
	m_nWriters++;
	m_CanRead.Reset();
	m_mutex.Unlock();

	if ( bWait )
	{
		m_CanWrite.Wait();
	}
}

void CThreadRWLock::UnlockWrite()
{
	m_mutex.Lock();
	m_nWriters--;
	if ( m_nWriters == 0)
	{
		if ( m_nPendingReaders )
		{
			m_CanRead.Set();
		}
	}
	else
	{
		m_CanWrite.Set();
	}
	m_mutex.Unlock();
}

//-----------------------------------------------------------------------------
//
// CThreadSpinRWLock
//
//-----------------------------------------------------------------------------
#ifndef OLD_SPINRWLOCK

void CThreadSpinRWLock::SpinLockForWrite()
{
	int i;

	if ( TryLockForWrite_UnforcedInline() )
	{
		return;
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
		if ( i % 1024 == 0 )
		{
			ThreadSleep( 0 );
		}
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForWrite_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

void CThreadSpinRWLock::SpinLockForRead()
{
	int i;
	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}
		ThreadPause();
		if ( i % 1024 == 0 )
		{
			ThreadSleep( 0 );
		}
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForRead_UnforcedInline() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

#else
/* (commented out to reduce distraction in colorized editor, remove entirely when new implementation settles)
void CThreadSpinRWLock::SpinLockForWrite( const uint32 threadId )
{
	int i;

	if ( TryLockForWrite( threadId ) )
	{
		return;
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}
		ThreadPause();
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}
		ThreadPause();
		if ( i % 1024 == 0 )
		{
			ThreadSleep( 0 );
		}
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

void CThreadSpinRWLock::LockForRead()
{
	int i;
	if ( TryLockForRead() )
	{
		return;
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForRead() )
		{
			return;
		}
		ThreadPause();
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if ( TryLockForRead() )
		{
			return;
		}
		ThreadPause();
		if ( i % 1024 == 0 )
		{
			ThreadSleep( 0 );
		}
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if ( TryLockForRead() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForRead() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

void CThreadSpinRWLock::UnlockRead()
{
	int i;

	Assert( m_lockInfo.m_nReaders > 0 && m_lockInfo.m_writerId == 0 );

	//uint32 nLockInfoReaders = m_lockInfo.m_nReaders;
	LockInfo_t oldValue;
	LockInfo_t newValue;
	
	if( IsX360() )
	{
		// this is the code equivalent to original code (see below) that doesn't cause LHS on Xbox360
		// WARNING: This code assumes BIG Endian CPU
		oldValue.m_i64 = uint32( m_lockInfo.m_nReaders );
		newValue.m_i64 = oldValue.m_i64 - 1; // NOTE: when we have -1 (or 0xFFFFFFFF) readers, this will result in non-equivalent code
	}
	else
	{
		// this is the original code that worked here for a while
		oldValue.m_nReaders = m_lockInfo.m_nReaders;
		oldValue.m_writerId = 0;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
		newValue.m_writerId = 0;
	}
	ThreadMemoryBarrier();
	if( AssignIf( newValue, oldValue ) )
		return;
	
	ThreadPause();
	oldValue.m_nReaders = m_lockInfo.m_nReaders;
	newValue.m_nReaders = oldValue.m_nReaders - 1;

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		oldValue.m_nReaders = m_lockInfo.m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}

	for ( i = THREAD_SPIN; i != 0; --i )
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		if ( i % 512 == 0 )
		{
			ThreadSleep( 0 );
		}
		oldValue.m_nReaders = m_lockInfo.m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}

	for ( i = THREAD_SPIN * 4; i != 0; --i )
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		ThreadSleep( 0 );
		oldValue.m_nReaders = m_lockInfo.m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		ThreadSleep( 1 );
		oldValue.m_nReaders = m_lockInfo.m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}
}

void CThreadSpinRWLock::UnlockWrite()
{
	Assert( m_lockInfo.m_writerId == ThreadGetCurrentId()  && m_lockInfo.m_nReaders == 0 );
	static const LockInfo_t newValue = { { 0, 0 } };
	ThreadMemoryBarrier();
	ThreadInterlockedExchange64(  (int64 *)&m_lockInfo, *((int64 *)&newValue) );
	m_nWriters--;
}
*/
#endif

#if defined( _PS3 )
// All CThread code is inline in the header for PS3

// This function is implemented here rather than the header because g_pCurThread resolves to GetCurThread() on PS3
// and we don't want to create a dependency on the ELF stub for everyone who includes the header.
PLATFORM_INTERFACE CThread *GetCurThreadPS3()
{
	return (CThread*)g_pCurThread;
}

PLATFORM_INTERFACE void SetCurThreadPS3( CThread *pThread )
{
	g_pCurThread = pThread;
}
#else
// The CThread implementation needs to be inlined for performance on the PS3 - It makes a difference of more than 1ms/frame
// for other platforms, we include the .inl in the .cpp file where it existed before
#include "../public/tier0/threadtools.inl"
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CWorkerThread::CWorkerThread()
:	m_EventSend(true),                 // must be manual-reset for PeekCall()
	m_EventComplete(true),             // must be manual-reset to handle multiple wait with thread properly
	m_Param(0),
	m_ReturnVal(0)
{
}

//---------------------------------------------------------

int CWorkerThread::CallWorker(unsigned dw, unsigned timeout, bool fBoostWorkerPriorityToMaster)
{
	return Call(dw, timeout, fBoostWorkerPriorityToMaster);
}

//---------------------------------------------------------

int CWorkerThread::CallMaster(unsigned dw, unsigned timeout)
{
	return Call(dw, timeout, false);
}

//---------------------------------------------------------

CThreadEvent &CWorkerThread::GetCallHandle()
{
	return m_EventSend;
}

//---------------------------------------------------------

unsigned CWorkerThread::GetCallParam() const
{
	return m_Param;
}

//---------------------------------------------------------

int CWorkerThread::BoostPriority()
{
	int iInitialPriority = GetPriority();

#ifdef WIN32
	const int iNewPriority = ThreadGetPriority( GetThreadHandle() );
	if (iNewPriority > iInitialPriority)
		ThreadSetPriority( GetThreadHandle(), iNewPriority);
#elif !defined( _PS3 )
	const int iNewPriority = ThreadGetPriority( (ThreadHandle_t)GetThreadID() );
	if (iNewPriority > iInitialPriority)
		ThreadSetPriority( (ThreadHandle_t)GetThreadID(), iNewPriority);
#endif

	return iInitialPriority;
}

//---------------------------------------------------------
static uint32 DefaultWaitFunc( uint32 nHandles, CThreadEvent** ppHandles, int bWaitAll, uint32 timeout )
{
	return CThreadEvent::WaitForMultiple( nHandles, ppHandles, bWaitAll!=0, timeout ) ;
}


int CWorkerThread::Call(unsigned dwParam, unsigned timeout, bool fBoostPriority, WaitFunc_t waitFunc)
{
	AssertMsg(!m_EventSend.Check(), "Cannot perform call if there's an existing call pending" );

	AUTO_LOCK( m_Lock );

	if (!IsAlive())
		return WTCR_FAIL;

	int iInitialPriority = 0;
	if (fBoostPriority)
	{
		iInitialPriority = BoostPriority();
	}

	// set the parameter, signal the worker thread, wait for the completion to be signaled
	m_Param = dwParam;

	m_EventComplete.Reset();
	m_EventSend.Set();

	WaitForReply( timeout, waitFunc );

	if (fBoostPriority)
		SetPriority(iInitialPriority);

	return m_ReturnVal;
}

//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------
int CWorkerThread::WaitForReply( unsigned timeout )
{
	return WaitForReply( timeout, NULL );
}

int CWorkerThread::WaitForReply( unsigned timeout, WaitFunc_t pfnWait )
{
	if (!pfnWait)
	{
		pfnWait = &DefaultWaitFunc;
	}

	CThreadEvent *waits[] =
	{
		&m_EventComplete,
		&m_ExitEvent
	};

	unsigned result;
	bool bInDebugger = Plat_IsInDebugSession();

	uint32 dwActualTimeout = ( (timeout==TT_INFINITE) ? 30000 : timeout );

	do
	{
#ifdef WIN32
		// Make sure the thread handle hasn't been closed
		if ( !GetThreadHandle() )
		{
			result = 1;
			break;
		}
#endif

		result = (*pfnWait)( ARRAYSIZE( waits ), waits, false, dwActualTimeout );

		AssertMsg(timeout != TT_INFINITE || result != TW_TIMEOUT, "Possible hung thread, call to thread timed out");

	} while ( bInDebugger && ( timeout == TT_INFINITE && result == TW_TIMEOUT ) );

	if ( result != 0 )
	{
		if (result == TW_TIMEOUT)
		{
			m_ReturnVal = WTCR_TIMEOUT;
		}
		else if (result == 1)
		{
			DevMsg( 2, "Thread failed to respond, probably exited\n");
			m_EventSend.Reset();
			m_ReturnVal = WTCR_TIMEOUT;
		}
		else
		{
			m_EventSend.Reset();
			m_ReturnVal = WTCR_THREAD_GONE;
		}
	}

	return m_ReturnVal;
}


//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------

bool CWorkerThread::WaitForCall(unsigned * pResult)
{
	return WaitForCall(TT_INFINITE, pResult);
}

//---------------------------------------------------------

bool CWorkerThread::WaitForCall(unsigned dwTimeout, unsigned * pResult)
{
	bool returnVal = m_EventSend.Wait(dwTimeout);
	if (pResult)
		*pResult = m_Param;
	return returnVal;
}

//---------------------------------------------------------
//
// is there a request?
//

bool CWorkerThread::PeekCall(unsigned * pParam)
{
	if (!m_EventSend.Check())
	{
		return false;
	}
	else
	{
		if (pParam)
		{
			*pParam = m_Param;
		}
		return true;
	}
}

//---------------------------------------------------------
//
// Reply to the request
//

void CWorkerThread::Reply(unsigned dw)
{
	m_Param = 0;
	m_ReturnVal = dw;

	// The request is now complete so PeekCall() should fail from
	// now on
	//
	// This event should be reset BEFORE we signal the client
	m_EventSend.Reset();

	// Tell the client we're finished
	m_EventComplete.Set();
}


//-----------------------------------------------------------------------------


#if defined( _PS3 )

/*******************************************************************************
* PS3 equivalent to Win32 function for setting events
*******************************************************************************/
BOOL SetEvent( CThreadEvent *pEvent )
{
	bool bRetVal = pEvent->Set();
	if ( !bRetVal )
		Assert(0);

	return bRetVal;
}

/*******************************************************************************
* PS3 equivalent to Win32 function for resetting events
*******************************************************************************/
BOOL ResetEvent( CThreadEvent *pEvent )
{
	return pEvent->Reset();
}

#define MAXIMUM_WAIT_OBJECTS 64

/*******************************************************************************
* Wait for a selection of events to terminate
*******************************************************************************/
DWORD WaitForMultipleObjects( DWORD nCount, CThreadEvent **lppHandles, BOOL bWaitAll, DWORD dwMilliseconds )
{
	//////////////////////////////////////////////////////////////
#ifndef NEW_WAIT_FOR_MULTIPLE_OBJECTS
	//////////////////////////////////////////////////////////////


	// Support for a limited amount of events
	if ( nCount >= MAXIMUM_WAIT_OBJECTS )
	{
		Assert(0);
		return false;
	}

	bool bRunning = true;
	unsigned int result = TW_FAILED;

	// For bWaitAll
	int numEvent = 0;
	int eventComplete[ MAXIMUM_WAIT_OBJECTS ] = {0};

	uint64_t timeDiffMS = 0;
	uint64_t startTimeMS = Plat_MSTime();
	uint64_t endTimeMS = 0;

	while ( bRunning )
	{
		// Check for a timeout
		if ( bRunning && ( dwMilliseconds != INFINITE ) && ( timeDiffMS > dwMilliseconds ) )
		{
			result = TW_TIMEOUT;
			bRunning = false;
		}

		// Wait for all the events to be set
		if ( bWaitAll )
		{
			for ( int event = 0; event < nCount; ++event )
			{
				if ( lppHandles[event]->Wait(1) )
				{
					// If an event is complete, mark it as complete in our list
					if ( eventComplete[ event ] == 0 )
					{
						numEvent++;
						eventComplete[ event ] = 1;
					}
				}
			}    

			// If all the events have been set, terminate the function
			if ( numEvent >= nCount )
			{
				result = WAIT_OBJECT_0;
				bRunning = false;
			}
		}

		// Wait for one event to be set
		else
		{
			for ( int event = 0; event < nCount; ++event )
			{
				if ( lppHandles[event]->Wait(1) )
				{
					result = WAIT_OBJECT_0 + event;
					bRunning = false;
					break;
				}
			}    
		}

		endTimeMS = Plat_MSTime();
		timeDiffMS = endTimeMS - startTimeMS;
	}

	return result;



	//////////////////////////////////////////////////////////////
#else	// NEW_WAIT_FOR_MULTIPLE_OBJECTS  // (expected PS3 only)
	//////////////////////////////////////////////////////////////
#ifndef _PS3
#error This code was written expecting to be run on PS3.
#endif

	// check if we have a wait objects semaphore
	if (!gbWaitObjectsCreated)
	{
		sys_semaphore_attribute_t semAttr;
		sys_semaphore_attribute_initialize(semAttr);
		sys_semaphore_create(&gWaitObjectsSemaphore, &semAttr, 0, 0xFFFF);

		gbWaitObjectsCreated = true;
	}

	// Support for a limited amount of events
	if ( nCount >= MAXIMUM_WAIT_OBJECTS )
	{
		Assert(0);
		return false;
	}

	unsigned int result = WAIT_FAILED;
	int res = CELL_OK;
	int event = -1;
	int numEvent = 0;

	// run through events registering this thread with each one
	for (int i = 0; i < nCount; i++)
	{
		lppHandles[i]->RegisterWaitingThread(&gWaitObjectsSemaphore, i, &event);		
	}


	// in the Source API, a timeOut of 0 means very short timeOut, not (as in the PS3 spec) an infinite timeout.
	// TT_INFINITE is #defined to 2^31-1, which means "infinite timeout" on PC and "72 minutes, 35 seconds" on PS3.
	// conversely, the code below (around deltaTime) expects to be able to compare against the timeout
	// value given here, so we cannot just replace 0 with 1 and TT_INFINITE with 0. 
	// So, we replace 0 with 1, meaning "a very short time", and test for the special value TT_INFINITE 
	// at the moment of calling sys_semaphore_wait, where we replace it with the real "infinite timeout"
	// value. It isn't safe to simply increase the declaration size of TT_INFINITE, because as you can
	// see it is often assigned to uint32s. 
	// Also, Source timeouts are specified in milliseconds, and PS3 timeouts are in microseconds, 
	// so we need to multiply by one thousand. 
	uint32 timeOut = dwMilliseconds;
	if ( timeOut == 0 )
	{
		timeOut = 1;
	}
	else if ( timeOut != TT_INFINITE )
	{
		timeOut *= 1000;
		// note that it's impossible for dwMilliseconds * 1000
		// to coincidentally equal TT_INFINITE since TT_INFINITE
		// is not divisible by 1000.
		COMPILE_TIME_ASSERT( TT_INFINITE % 1000 != 0 );
	}

	COMPILE_TIME_ASSERT( TT_INFINITE != 0 ); // The code here was written expecting (working around) that TT_INFINITE is 
											 // MAXINT, so if you changed this number, please read the comment above and
											 // carefully examine the code here to make sure that timeouts still work 
											 // correctly on PS3. Be aware that in many places in Source, a timeout of
											 // 0 has some special meaning other than "infinite timeout", so track those
											 // down too.
										  

	// Wait for all the events to be set
	if ( bWaitAll )
	{
		while (numEvent < nCount)
		{
			uint64_t deltaTime = Timer_GetTimeUS();

			res = sys_semaphore_wait(gWaitObjectsSemaphore, timeOut == TT_INFINITE ? 0 : timeOut );

			deltaTime = Timer_GetTimeUS() - deltaTime;

			if (res == ETIMEDOUT)
			{
				result = TW_TIMEOUT;
				break;
			}
			else if (res == CELL_OK)
			{				
				numEvent++;

				if (deltaTime >= timeOut)
				{
					// note - if this is not truly a time out
					// then it will be set to WAIT_OBJECT_0
					// after this loop
					result = TW_TIMEOUT;
					break;
				}
				else
				{
					timeOut -= deltaTime;
				}
			}
			else
			{
				result = TW_FAILED;
				break;
			}
		}

		if (numEvent >= nCount)
		{
			result = WAIT_OBJECT_0;
		}
	}
	else	// Wait for one event to be set
	{
		// no event fired yet, wait on semaphore
		res = sys_semaphore_wait( gWaitObjectsSemaphore, timeOut == TT_INFINITE ? 0 : timeOut  );

		if (res == ETIMEDOUT)
		{
			result = TW_TIMEOUT;
		}
		else if (res == CELL_OK)
		{
			if ((event < 0) || (event >= nCount))
			{
				DEBUG_ERROR("Bad event\n");
			}

			result = WAIT_OBJECT_0 + event;
		}	
	}

	// run through events unregistering this thread, for benefit
	// of those events that did not fire, or fired before semaphore
	// was registered
	for (int i = 0; i < nCount; i++)
	{
		lppHandles[i]->UnregisterWaitingThread(&gWaitObjectsSemaphore);
	}

	// reset semaphore
	while (sys_semaphore_trywait(gWaitObjectsSemaphore) != EBUSY);

	return result;


	//////////////////////////////////////////////////////////////
#endif	// NEW_WAIT_FOR_MULTIPLE_OBJECTS
	//////////////////////////////////////////////////////////////
}

#endif
