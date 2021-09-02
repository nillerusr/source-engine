//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"
#include "tier0/vcrmode.h"
#include "tier0/memalloc.h"
#include "tier0/dbg.h"
#include <algorithm>
#include <vector>

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#ifdef OSX
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>
#endif
#ifdef LINUX
#include <time.h>
#include <fcntl.h>
#endif

#include "tier0/memdbgon.h"
// Benchmark mode uses this heavy-handed method 

// *** WARNING ***. On Linux gettimeofday returns the system's best guess at
// actual wall clock time and this can go backwards. You need to use
// clock_gettime( CLOCK_MONOTONIC ... ) if this isn't what you want.

// If you want to try using rdtsc for Plat_FloatTime(), enable USE_RDTSC_FOR_FLOATTIME:
// 
// Make sure you know what you're doing. This was disabled due to the long startup time, and
//  in our testing, even though constant_tsc was set, we couldn't rely on the
//  max frequency result returned from CalculateCPUFreq() (ie /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq).
// 
// #define USE_RDTSC_FOR_FLOATTIME

extern VCRMode_t g_VCRMode;

static bool g_bBenchmarkMode = false;
static double g_FakeBenchmarkTime = 0;
static double g_FakeBenchmarkTimeInc = 1.0 / 66.0;

#ifdef USE_RDTSC_FOR_FLOATTIME

static bool s_bTimeInitted;
static bool s_bUseRDTSC;
static uint64 s_nRDTSCBase;
static float s_flRDTSCToMicroSeconds;
static double s_flRDTSCScale;

#endif // USE_RDTSC_FOR_FLOATTIME

bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}


#define N_ITERATIONS_OF_RDTSC_TEST_TO_RUN 5					// should be odd
#define TEST_RDTSC_FLOATTIME 0

size_t ApproximateProcessMemoryUsage( void )
{
/*
From http://man7.org/linux/man-pages/man5/proc.5.html:
       /proc/[pid]/statm
              Provides information about memory usage, measured in pages.
              The columns are:

                  size       (1) total program size
                             (same as VmSize in /proc/[pid]/status)
                  resident   (2) resident set size
                             (same as VmRSS in /proc/[pid]/status)
                  share      (3) shared pages (i.e., backed by a file)
                  text       (4) text (code)
                  lib        (5) library (unused in Linux 2.6)
                  data       (6) data + stack
                  dt         (7) dirty pages (unused in Linux 2.6)
*/

// This returns the resident memory size (RES column in 'top') in bytes.
	size_t nRet = 0;
	FILE *pFile = fopen( "/proc/self/statm", "r" );
	if ( pFile )
	{
		size_t nSize, nResident, nShare, nText, nLib_Unused, nDataPlusStack, nDt_Unused;
		if ( fscanf( pFile, "%zu %zu %zu %zu %zu %zu %zu", &nSize, &nResident, &nShare, &nText, &nLib_Unused, &nDataPlusStack, &nDt_Unused ) >= 2 )
		{
			nRet = 4096 * nResident;
		}
		fclose( pFile );
	}
	return nRet;
}

#ifdef USE_RDTSC_FOR_FLOATTIME

static void InitTimeSystem( void )
{
	s_bTimeInitted = true;

	// now, see if we can use rdtsc instead. If this is one of the chips with a separate constant clock for rdtsc, we can
	FILE *pCpuInfo = fopen( "/proc/cpuinfo", "r" );
	if ( pCpuInfo )
	{
		bool bAnyBadCores = false;
		char lbuf[2048];
		while( fgets( lbuf, sizeof( lbuf ), pCpuInfo ) )
		{
			if ( memcmp( lbuf, "flags", 4 ) == 0 )
			{
				if ( ! strstr( lbuf, "constant_tsc" ) )
				{
					bAnyBadCores = true;
					break;
				}
			}
		}
		fclose( pCpuInfo );
		if ( ! bAnyBadCores )
		{
			// this system appears to have the proper cpu setup to use rdtsc from reliable timing. Let's either read the cpu frequency from an
			// environment variable, or time it ourselves
			char const *pEnv = getenv( "RDTSC_FREQUENCY" );
			if ( pEnv )
			{
				// the environment variable is allowed to hold either a benchmark result, or a string such as "disable"
				if ( pEnv && ( ( pEnv[0] > '9' ) || ( pEnv[0] < '0' ) ) )
					return;									// leave rdtsc disabled
				// the variable holds the number of ticks per microsecond
				s_flRDTSCToMicroSeconds = atof( pEnv );
				// sanity check
				if ( s_flRDTSCToMicroSeconds > 1.0 )
				{
					s_bUseRDTSC = true;
					s_flRDTSCScale = 1.0  / ( 1000.0 * 1000.0 * s_flRDTSCToMicroSeconds );
					s_nRDTSCBase = Plat_Rdtsc();
					return;
				}
			}
			else
			{
				printf( "Running a benchmark to measure system clock frequency...\n" );
				// run n iterations and use the median
				double flRDTSCToMicroSeconds[N_ITERATIONS_OF_RDTSC_TEST_TO_RUN];
				for( int i = 0; i < ARRAYSIZE( flRDTSCToMicroSeconds ) ; i++ )
				{
					uint64 stime = Plat_Rdtsc();
					struct timeval stimeval;
					gettimeofday( &stimeval, NULL );
					sleep( 1 );
					uint64 etime = Plat_Rdtsc() - stime;
					struct timeval etimeval;
					gettimeofday( &etimeval, NULL );
					// subtract timevals to get elapsed microseconds
					struct timeval elapsedtimeval;
					timersub( &etimeval, &stimeval, &elapsedtimeval );
					uint64 nUs = 1000000 * elapsedtimeval.tv_sec + elapsedtimeval.tv_usec;
					flRDTSCToMicroSeconds[ i ] = ( etime / nUs );
				}
				std::make_heap( flRDTSCToMicroSeconds, flRDTSCToMicroSeconds + ARRAYSIZE( flRDTSCToMicroSeconds ) - 1 );
				std::sort_heap( flRDTSCToMicroSeconds, flRDTSCToMicroSeconds + ARRAYSIZE( flRDTSCToMicroSeconds ) - 1 );
				s_flRDTSCToMicroSeconds = flRDTSCToMicroSeconds[ARRAYSIZE( flRDTSCToMicroSeconds ) / 2 ];
				s_flRDTSCScale = 1.0  / ( 1000.0 * 1000.0 * s_flRDTSCToMicroSeconds );
				printf( "Finished RDTSC test. To prevent the startup delay from this benchmark, set the environment variable RDTSC_FREQUENCY to %f on this system."
						" This value is dependent upon the CPU clock speed and architecture and should be determined separately for each server. The use of this mechanism"
						" for timing can be disabled by setting RDTSC_FREQUENCY to 'disabled'.\n",
						s_flRDTSCToMicroSeconds );
				s_nRDTSCBase = Plat_Rdtsc();
				s_bUseRDTSC = true;
#if TEST_RDTSC_FLOATTIME
				printf( "RDTSC test results:\n" );
				for( int i = 0; i < ARRAYSIZE( flRDTSCToMicroSeconds ); i++ )
					printf(" [%d] = %f\n", i, flRDTSCToMicroSeconds[i] );
				printf( "scale factor = %f\n", s_flRDTSCScale );
				uint64 srdtsc_time = Plat_Rdtsc();
				for( int i = 0; i < 1000 * 1000 * 10; i++ )
				{
					float p = Plat_FloatTime();
				}
				printf( "slow = %lld\n", Plat_Rdtsc() - srdtsc_time );
				// now, run a benchmark to see how much this optimization buys us
				srdtsc_time = Plat_Rdtsc();
				for( int i = 0; i < 1000 * 1000 * 10; i++ )
				{
					float p = Plat_FloatTime();
				}
				printf( "sfast = %lld\n", Plat_Rdtsc() - srdtsc_time );
#endif
			}
		}
	}
}

static FORCEINLINE void TestTimeSystem( void )
{
#if TEST_RDTSC_FLOATTIME
	// now, test that plat_float time actually works
	for( int t = 0 ; t < 5; t++ )
	{
		float flStartT = Plat_FloatTime();
		struct timeval stime;
		gettimeofday( &stime, NULL );
		sleep( 5 );
		float flElapsedT = Plat_FloatTime() - flStartT;
		struct timeval etime;
		gettimeofday( &etime, NULL );
		struct timeval dtime;
		timersub( &etime, &stime, &dtime );
		printf( " plat_float time says %f elapsed. gettimeofday says %f\n",
				flElapsedT, dtime.tv_sec + dtime.tv_usec / 1000000.0 );
	}
#endif
}

#endif // USE_RDTSC_FOR_FLOATTIME

double Plat_FloatTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
#ifdef OSX
	// OSX
	static uint64 start_time = 0;
	static mach_timebase_info_data_t    sTimebaseInfo;
	static double conversion = 0.0;
	
	if ( !start_time )
	{
		start_time = mach_absolute_time();
		mach_timebase_info(&sTimebaseInfo);
		conversion = 1e-9 * (double) sTimebaseInfo.numer / (double) sTimebaseInfo.denom;
	}
	
	uint64 now = mach_absolute_time();
	
	return ( now - start_time ) * conversion;
#else
	// Linux
	static struct timespec start_time = { 0, 0 };
	static bool bInitialized = false;	

	if ( !bInitialized )
	{
		bInitialized = true;
		clock_gettime( CLOCK_MONOTONIC, &start_time );
	}

	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	
	return ( now.tv_sec - start_time.tv_sec ) + ( now.tv_nsec * 1e-9 );

#ifdef USE_RDTSC_FOR_FLOATTIME
	if ( ! s_bTimeInitted )
	{
		InitTimeSystem();
		TestTimeSystem();
	}
	if ( s_bUseRDTSC )
	{
		uint64 nTicks = Plat_Rdtsc() - s_nRDTSCBase;
		return ( (double) nTicks) * s_flRDTSCScale;
	}
	else
	{
		struct timeval  tp;
	        gettimeofday( &tp, NULL );

		if (VCRGetMode() == VCR_Disabled)
			return (( tp.tv_sec - s_nSecBase ) + tp.tv_usec / 1000000.0 );
		
		return VCRHook_Sys_FloatTime( ( tp.tv_sec - s_nSecBase ) + tp.tv_usec / 1000000.0 );
	}
#endif // USE_RDTSC_FOR_FLOATTIME

#endif
}

unsigned int Plat_MSTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (unsigned int)(g_FakeBenchmarkTime * 1000.0);
	}

#ifdef USE_RDTSC_FOR_FLOATTIME
	if ( ! s_bTimeInitted )
	{
		InitTimeSystem();
		TestTimeSystem();
	}
	if ( s_bUseRDTSC )
	{
		uint64 nTicks = Plat_Rdtsc() - s_nRDTSCBase;
		return 1000.0 * nTicks * s_flRDTSCScale;
	}
	else
#endif // USE_RDTSC_FOR_FLOATTIME
	{
		return ( uint )( Plat_FloatTime() * 1000 );
	}
}

uint64 Plat_USTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (unsigned int)(g_FakeBenchmarkTime * 1000000.0);
	}

#ifdef USE_RDTSC_FOR_FLOATTIME
	if ( ! s_bTimeInitted )
	{
		InitTimeSystem();
		TestTimeSystem();
	}
	if ( s_bUseRDTSC )
	{
		uint64 nTicks = Plat_Rdtsc() - s_nRDTSCBase;
		return 1000000.0 * nTicks * s_flRDTSCScale;
	}
	else
#endif // USE_RDTSC_FOR_FLOATTIME
	{
		return ( uint64 )( Plat_FloatTime() * 1000000 );
	}
}

// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
	return ctime_r( timep, buf );
}

// Wraps the thread-safe versions of gmtime
struct tm *Plat_gmtime( const time_t *timep, struct tm *result )
{
	return gmtime_r( timep, result );
}

time_t Plat_timegm( struct tm *timeptr )
{
	return timegm( timeptr );
}

// Wraps the thread-safe versions of localtime
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
	return localtime_r( timep, result );
}

bool vtune( bool resume )
{
  return 0;
}


// -------------------------------------------------------------------------------------------------- //
// Memory stuff.
// -------------------------------------------------------------------------------------------------- //

#ifndef NO_HOOK_MALLOC

PLATFORM_INTERFACE void Plat_DefaultAllocErrorFn( unsigned long size )
{
}

typedef void (*Plat_AllocErrorFn)( unsigned long size );
Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;

PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size )
{
	void *pRet = MemAlloc_Alloc( size );
	if ( pRet )
	{
		return pRet;
	}
	else
	{
		g_AllocError( size );
		return 0;
	}
}


PLATFORM_INTERFACE void* Plat_Realloc( void *ptr, unsigned long size )
{
	void *pRet = g_pMemAlloc->Realloc( ptr, size );
	if ( pRet )
	{
		return pRet;
	}
	else
	{
		g_AllocError( size );
		return 0;
	}
}


PLATFORM_INTERFACE void Plat_Free( void *ptr )
{
	g_pMemAlloc->Free( ptr );
}


PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}

#endif // !NO_HOOK_MALLOC

#if defined( OSX )

// From the Apple tech note: http://developer.apple.com/library/mac/#qa/qa1361/_index.html
bool Plat_IsInDebugSession()
{
	int                 junk;
	int                 mib[4];
	struct kinfo_proc   info;
	size_t              size;
	static int s_IsInDebugSession = -1;

	if ( s_IsInDebugSession == -1 )
	{
		// Initialize the flags so that, if sysctl fails for some bizarre 
		// reason, we get a predictable result.

		info.kp_proc.p_flag = 0;

		// Initialize mib, which tells sysctl the info we want, in this case
		// we're looking for information about a specific process ID.

		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PID;
		mib[3] = getpid();

		// Call sysctl.

		size = sizeof(info);
		junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);

		// We're being debugged if the P_TRACED flag is set.

		s_IsInDebugSession = ( (info.kp_proc.p_flag & P_TRACED) != 0 );
	}

	return !!s_IsInDebugSession;
}

#elif defined( LINUX )

bool Plat_IsInDebugSession()
{
	// For linux: http://stackoverflow.com/questions/3596781/detect-if-gdb-is-running
	// Don't use "if (ptrace(PTRACE_TRACEME, 0, NULL, 0) == -1)" as it means debuggers can't attach.
	// 	Other solutions they mention involve forking. Ugh.
	//
	// Good solution from Pierre-Loup: Check TracerPid in /proc/self/status.
	// from "man proc"
	//     TracerPid: PID of process tracing this process (0 if not being traced).
	int tracerpid = -1;

	int fd = open( "/proc/self/status", O_RDONLY, S_IRUSR );
	if( fd >= 0 )
	{
		char buf[ 1024 ];
		static const char s_TracerPid[] = "TracerPid:";

		int len = read( fd, buf, sizeof( buf ) - 1 );
		if ( len > 0 )
		{
			buf[ len ] = 0;

			const char *str = strstr( buf, s_TracerPid );
			tracerpid = str ? atoi( str + sizeof( s_TracerPid ) ) : -1;
		}

		close( fd );
	}

	return ( tracerpid > 0 );
}


#endif // defined( LINUX )

void Plat_DebugString( const char * psz )
{
	printf( "%s", psz );
}

static char g_CmdLine[ 2048 ];
PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine )
{
	strncpy( g_CmdLine, cmdLine, sizeof(g_CmdLine) );
	g_CmdLine[ sizeof(g_CmdLine) -1 ] = 0;
}

PLATFORM_INTERFACE const tchar *Plat_GetCommandLine()
{
#ifdef LINUX
	if( !g_CmdLine[ 0 ] )
	{
		FILE *fp = fopen( "/proc/self/cmdline", "rb" );

		if( fp )
		{
			size_t nCharRead = 0;

			// -1 to leave room for the '\0'
			nCharRead = fread( g_CmdLine, sizeof( g_CmdLine[0] ), ARRAYSIZE( g_CmdLine ) - 1, fp );
			if ( feof( fp ) && !ferror( fp ) ) // Should have read the whole command line without error
			{
				Assert ( nCharRead < ARRAYSIZE( g_CmdLine ) );

				for( int i = 0; i < nCharRead; i++ )
				{
					if( g_CmdLine[ i ] == '\0' )
						g_CmdLine[ i ] = ' ';
				}
				
				g_CmdLine[ nCharRead ] = '\0';

			}
			fclose( fp );
		}

		Assert( g_CmdLine[ 0 ] );
	}
#endif // LINUX

	return g_CmdLine;
}

PLATFORM_INTERFACE const char *Plat_GetCommandLineA()
{
	return Plat_GetCommandLine();
}

PLATFORM_INTERFACE bool GetMemoryInformation( MemoryInformation *pOutMemoryInfo )
{
	#if defined( LINUX ) || defined( OSX )
		return false;
	#else
		#error "Need to fill out GetMemoryInformation or at least return false for this platform"
	#endif
}


PLATFORM_INTERFACE bool Is64BitOS()
{
#if defined OSX
	return true;
#elif defined LINUX
	FILE *pp = popen( "uname -m", "r" );
	if ( pp != NULL )
	{
		char rgchArchString[256];
		fgets( rgchArchString, sizeof( rgchArchString ), pp );
		pclose( pp );
		if ( !strncasecmp( rgchArchString, "x86_64", strlen( "x86_64" ) ) )
			return true;
	}
#else
	Assert( !"implement Is64BitOS" );
#endif
	return false;
}

PLATFORM_INTERFACE void Plat_ExitProcess( int nCode )
{
	_exit( nCode );
}


static int s_nWatchDogTimerTimeScale = 0;
static bool s_bInittedWD = false;
static int s_WatchdogTime = 0;
static Plat_WatchDogHandlerFunction_t s_pWatchDogHandlerFunction;

static void InitWatchDogTimer( void )
{
	if( !strstr( g_CmdLine, "-nowatchdog" ) )
	{
#ifdef _DEBUG
		s_nWatchDogTimerTimeScale = 10;						// debug is slow
#else
		s_nWatchDogTimerTimeScale = 1;
#endif

	}

}

// SIGALRM handler. Used by Watchdog timer code.
static void WatchDogHandler( int s )
{
	Plat_DebugString( "WatchDog! Server took too long to process (probably infinite loop).\n" );

	DebuggerBreakIfDebugging();

	if ( s_pWatchDogHandlerFunction )
	{
		s_pWatchDogHandlerFunction();
	}
	else
	{
		// force a crash
		abort();
	}
}

// watchdog timer support
PLATFORM_INTERFACE void Plat_BeginWatchdogTimer( int nSecs )
{
	if ( !s_bInittedWD )
	{
		s_bInittedWD = true;
		InitWatchDogTimer();
	}

	nSecs *= s_nWatchDogTimerTimeScale;
	nSecs = MIN( nSecs, 5 * 60 );							// no more than 5 minutes no matter what
	if ( nSecs )
	{
		s_WatchdogTime = nSecs;
		signal( SIGALRM, WatchDogHandler );
		alarm( nSecs );
	}
}

PLATFORM_INTERFACE void Plat_EndWatchdogTimer( void )
{
	alarm( 0 );
	signal( SIGALRM, SIG_DFL );
	s_WatchdogTime = 0;
}

PLATFORM_INTERFACE int Plat_GetWatchdogTime( void )
{
	return s_WatchdogTime;
}

PLATFORM_INTERFACE void Plat_SetWatchdogHandlerFunction( Plat_WatchDogHandlerFunction_t function )
{
	s_pWatchDogHandlerFunction = function;
}

#ifndef NO_HOOK_MALLOC

// memory logging this functionality is portable code, except for the way in which it hooks
// malloc/free. glibc contains the ability for the app to install hooks into malloc/free.

#include <malloc.h>
#include <tier1/utlintrusivelist.h>
#include <execinfo.h>
#include <tier1/utlvector.h>
     
#define MEMALLOC_HASHSIZE 8193
typedef uint32 ptrint_t;



struct CLinuxMemStats
{
	int nNumMallocs;										// total every
	int nNumFrees;											// total
	int nNumMallocsInUse;
	int nTotalMallocInUse;

};

#define MAX_STACK_TRACEBACK 20

struct CLinuxMallocContext
{
	CLinuxMallocContext *m_pNext;

	void *pStackTraceBack[MAX_STACK_TRACEBACK];

	int m_nCurrentAllocSize;
	int m_nNumAllocsInUse;
	int m_nMaximumSize;
	int m_TotalNumAllocs;
	int m_nLastAllocSize;


	CLinuxMallocContext( void )
	{
		memset( this, 0, sizeof( *this ) );
	}

};


static CUtlIntrusiveList<CLinuxMallocContext> s_ContextHash[MEMALLOC_HASHSIZE];

CLinuxMemStats g_LinuxMemStats;


struct RememberedAlloc_t
{
	RememberedAlloc_t *m_pNext, *m_pPrev;	// all addresses that hash to the same value are linked

	CLinuxMallocContext *m_pAllocContext;
	ptrint_t m_nAddress;										// the address of the memory that came from malloc/realloc
	size_t m_nSize;

	void AdjustSize( size_t nsize )
	{
		int nDelta = nsize - m_nSize;
		m_nSize = nsize;
		m_pAllocContext->m_nCurrentAllocSize += nDelta;
		m_pAllocContext->m_nMaximumSize = MAX( m_pAllocContext->m_nMaximumSize, m_pAllocContext->m_nCurrentAllocSize );
	}

};

static inline int AddressHash( ptrint_t nAdr )
{
	return ( nAdr % MEMALLOC_HASHSIZE );
}

static CUtlIntrusiveDList<RememberedAlloc_t> s_AddressData[MEMALLOC_HASHSIZE];

static struct RememberedAlloc_t *FindAddress( void *pAdr, int *pHash = NULL )
{
	ptrint_t nAdr = ( ptrint_t ) pAdr;
	int nHash = AddressHash( nAdr );
	if ( pHash )
		*pHash = nHash;
	for( RememberedAlloc_t *i = s_AddressData[nHash].m_pHead; i; i = i->m_pNext )
	{
		if ( i->m_nAddress == nAdr )
			return i;
	}
	return NULL;
}

#ifdef LINUX
static void *MallocHook( size_t, const void * );
static void FreeHook( void*, const void * );
static void *ReallocHook( void *ptr, size_t size, const void *caller );

static void RemoveHooks( void )
{
	__malloc_hook = NULL;
	__free_hook = NULL;
	__realloc_hook = NULL;
}


static void InstallHooks( void )
{
	__malloc_hook = MallocHook;
	__free_hook = FreeHook;
	__realloc_hook = ReallocHook;

}
#elif OSX


static void RemoveHooks( void )
{
}


static void InstallHooks( void )
{	
}


#else
#error
#endif


static void AddMemoryAllocation( void *pResult, size_t size )
{
	if ( pResult )
	{
		g_LinuxMemStats.nNumMallocs++;
		g_LinuxMemStats.nNumMallocsInUse++;
		g_LinuxMemStats.nTotalMallocInUse += size;

		RememberedAlloc_t *pNew = new RememberedAlloc_t;
		pNew->m_nAddress = ( ptrint_t ) pResult;
		pNew->m_nSize = size;
		s_AddressData[AddressHash( pNew->m_nAddress )].AddToHead( pNew );


		// now, find the stack traceback context for this call
		void *pTraceBack[MAX_STACK_TRACEBACK];
		int nNumGot = backtrace( pTraceBack, ARRAYSIZE( pTraceBack ) );
		for( int n = MAX( 0, nNumGot - 1 ); n < MAX_STACK_TRACEBACK; n++ )
			pTraceBack[n] = NULL;

		uint32 nHash = 0;
		for( int i = 0; i < MAX_STACK_TRACEBACK; i++ )
		{
			nHash = ( nHash * 3 ) + ( ( ptrint_t ) pTraceBack[i] );
		}
		nHash %= MEMALLOC_HASHSIZE;

		CLinuxMallocContext *pFoundCtx = NULL;
		// see if we have this context
		for( CLinuxMallocContext *i = s_ContextHash[nHash].m_pHead; i ; i = i->m_pNext )
		{
			if ( memcmp( pTraceBack, i->pStackTraceBack, sizeof( pTraceBack ) ) == 0 )
			{
				pFoundCtx = i;
				break;
			}
		}
		if ( ! pFoundCtx )
		{
			pFoundCtx = new CLinuxMallocContext;
			memcpy( pFoundCtx->pStackTraceBack, pTraceBack, sizeof( pTraceBack ) );
			s_ContextHash[nHash].AddToHead( pFoundCtx );
		}
		pNew->m_pAllocContext = pFoundCtx;
		pFoundCtx->m_nCurrentAllocSize += size;
		pFoundCtx->m_nNumAllocsInUse++;
		pFoundCtx->m_nMaximumSize = MAX( pFoundCtx->m_nCurrentAllocSize, pFoundCtx->m_nMaximumSize );
		pFoundCtx->m_TotalNumAllocs++;
	}
}


static CThreadFastMutex s_MemoryMutex;

static void *ReallocHook( void *ptr, size_t size, const void *caller )
{
	AUTO_LOCK( s_MemoryMutex );
	RemoveHooks();
	void *nResult = realloc( ptr, size );

	if ( ptr )												// did we have this memory before
	{
		int nHash;
		RememberedAlloc_t *pBlock = FindAddress( ptr, &nHash );
		if ( pBlock )
		{
			if ( ptr == nResult )
			{
				// it successfully alloced, just need to update size info, etc
				pBlock->AdjustSize( size );
				g_LinuxMemStats.nTotalMallocInUse += ( size - pBlock->m_nSize );
				
			}
			else
			{
				pBlock->m_pAllocContext->m_nCurrentAllocSize -= pBlock->m_nSize;
				pBlock->m_pAllocContext->m_nNumAllocsInUse--;
				s_AddressData[nHash].RemoveNode( pBlock );	// throw away this node
				AddMemoryAllocation( nResult, size );
			}
		}
		else
			AddMemoryAllocation( nResult, size );
	}
	else
		AddMemoryAllocation( nResult, size );
	InstallHooks();
	return nResult;



}


static void *MallocHook(size_t size, const void *caller)
{
	// turn off hooking so we won't recurse
	AUTO_LOCK( s_MemoryMutex );
	RemoveHooks();


	void *pResult = malloc (size);

	// now, add this memory chunk to our list
	AddMemoryAllocation( pResult, size );

	InstallHooks();

	return pResult;
}
     
static void FreeHook(void *ptr, const void *caller )
{
	AUTO_LOCK( s_MemoryMutex );
	RemoveHooks();

	// call real free
	free (ptr);

	// look in our list
	if ( ptr )
	{
		int nHash;
		RememberedAlloc_t *pFound = FindAddress( ptr, &nHash );
		if ( !pFound )
		{
			//printf(" free of unallocated adr %p (maybe)\n", ptr );
		}
		else
		{
			pFound->m_pAllocContext->m_nCurrentAllocSize -= pFound->m_nSize;
			pFound->m_pAllocContext->m_nNumAllocsInUse--;
			g_LinuxMemStats.nTotalMallocInUse -= pFound->m_nSize;
			g_LinuxMemStats.nNumFrees++;
			g_LinuxMemStats.nNumMallocsInUse--;
			s_AddressData[nHash].RemoveNode( pFound );
			delete pFound;
		}
	}
	InstallHooks();
}
     
void EnableMemoryLogging( bool bOnOff )
{
	if ( bOnOff )
	{
		InstallHooks();
#if 0
		// simple test
		char *p[10];
		for( int i =0; i < 10; i++ )
			p[i] = new char[10];
		printf( "log with memory\n" );
		DumpMemoryLog();
		for( int i = 0; i < 10; i++ )
			delete[] p[i];
		printf( "after free,\n" );
		DumpMemoryLog();

		// now, try som realloc action
		int *p1 = NULL;
		int *p2;
		for( int i =1 ; i < 10; i++ )
		{
			p1 = (int * ) realloc( p1, i * 100 );
			if ( i == 3 )
				p2 = new int[300];
		}
		printf(" after realloc loop\n" );
		DumpMemoryLog();
		delete[] p2;
		free( p1 );
		printf(" after realloc frees\n" );
		DumpMemoryLog();
#endif
	}

	else
		RemoveHooks();
}


static inline bool SortLessFunc( CLinuxMallocContext * const &left, CLinuxMallocContext  * const &right )
{
	return left->m_nCurrentAllocSize > right->m_nCurrentAllocSize;
}

void DumpMemoryLog( int nThresh )
{
	AUTO_LOCK( s_MemoryMutex );
	EndWatchdogTimer();
	RemoveHooks();
	std::vector<CLinuxMallocContext *> memList;
	
	for( int i =0 ; i < MEMALLOC_HASHSIZE; i++ )
	{
		for( CLinuxMallocContext *p = s_ContextHash[i].m_pHead; p; p=p->m_pNext )
		{
			if ( p->m_nCurrentAllocSize >= nThresh )
			{
				memList.push_back( p );
			}
		}
	}

	std::sort( memList.begin(), memList.end(), SortLessFunc );
	
	for( int i = 0; i < memList.size(); i++ )
	{
		CLinuxMallocContext *p = memList[i];
		char **strings = backtrace_symbols( p->pStackTraceBack, MAX_STACK_TRACEBACK );
		Msg( "Context cursize=%d nallocs=%d maxsize=%d total_allocs=%d\n", p->m_nCurrentAllocSize, p->m_nNumAllocsInUse, p->m_nMaximumSize, p->m_TotalNumAllocs );
		Msg("  stack\n" );
		for( int n = 0 ; n < MAX_STACK_TRACEBACK; n++ )
			if ( p->pStackTraceBack[n] )
				Msg("  %p %s\n", p->pStackTraceBack[n], strings[n] );
		free( strings );
	}
	Msg("End of memory list\n" );
	InstallHooks();
}

void DumpChangedMemory( int nThresh )
{
	AUTO_LOCK( s_MemoryMutex );
	EndWatchdogTimer();
	RemoveHooks();
	std::vector<CLinuxMallocContext *> memList;
	
	for( int i =0 ; i < MEMALLOC_HASHSIZE; i++ )
	{
		for( CLinuxMallocContext *p = s_ContextHash[i].m_pHead; p; p=p->m_pNext )
		{
			if ( p->m_nCurrentAllocSize - p->m_nLastAllocSize > nThresh )
			{
				memList.push_back( p );
			}
		}
	}

	std::sort( memList.begin(), memList.end(), SortLessFunc );
	for( int i = 0; i < memList.size(); i++ )
	{
		CLinuxMallocContext *p = memList[i];
		char **strings = backtrace_symbols( p->pStackTraceBack, MAX_STACK_TRACEBACK );
		Msg( "Context cursize=%d lastsize=%d nallocs=%d maxsize=%d total_allocs=%d\n", p->m_nCurrentAllocSize, p->m_nLastAllocSize, p->m_nNumAllocsInUse, p->m_nMaximumSize, p->m_TotalNumAllocs );
		Msg("  stack\n" );
		for( int n = 0 ; n < MAX_STACK_TRACEBACK; n++ )
			if ( p->pStackTraceBack[n] )
				Msg("  %p %s\n", p->pStackTraceBack[n], strings[n] );
		free( strings );
	}
	Msg("End of memory list\n" );
	InstallHooks();
}

void SetMemoryMark( void )
{
	AUTO_LOCK( s_MemoryMutex );
	for( int i =0 ; i < MEMALLOC_HASHSIZE; i++ )
	{
		for( CLinuxMallocContext *p = s_ContextHash[i].m_pHead; p; p=p->m_pNext )
		{
			p->m_nLastAllocSize = p->m_nCurrentAllocSize;
		}
	}
}


void DumpMemorySummary( void )
{
	Msg( "Total memory in use = %d, NumMallocs=%d, Num Frees=%d approx process usage=%ul\n", g_LinuxMemStats.nTotalMallocInUse, g_LinuxMemStats.nNumMallocs, g_LinuxMemStats.nNumFrees,
		 (unsigned int)ApproximateProcessMemoryUsage() );
}

#endif // !NO_HOOK_MALLOC

// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"


