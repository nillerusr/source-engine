//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/platform.h"
#include "tier0/memalloc.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include <sys/time.h>
#include <unistd.h>

#ifdef OSX
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif


static bool g_bBenchmarkMode = false;
static double g_FakeBenchmarkTime = 0;
static double g_FakeBenchmarkTimeInc = 1.0 / 66.0;


bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}



#ifdef OSX

static uint64 start_time = 0;
static mach_timebase_info_data_t    sTimebaseInfo;
static double conversion = 0.0;

void InitTime()
{
	start_time = mach_absolute_time();
	mach_timebase_info(&sTimebaseInfo);
	conversion = 1e-9 * (double) sTimebaseInfo.numer / (double) sTimebaseInfo.denom;	
}

uint64 Plat_GetClockStart()
{
	if ( !start_time )
	{
		InitTime();
	}
	
	return start_time * conversion;
}

double Plat_FloatTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
	if ( !start_time )
	{
		InitTime();
	}
	
	uint64 now = mach_absolute_time();
	
	return ( now - start_time ) * conversion;
}
#else

static int      secbase = 0;

void InitTime( struct timeval &tp )
{
	secbase = tp.tv_sec;
}

uint64 Plat_GetClockStart()
{
	if ( !secbase )
	{
		struct timeval  tp;
		gettimeofday( &tp, NULL );
		InitTime( tp );
	}
	
	return secbase;
}


double Plat_FloatTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
	struct timeval  tp;
	
	gettimeofday( &tp, NULL );
	
	if ( !secbase )
	{
		InitTime( tp );
		return ( tp.tv_usec / 1000000.0 );
	}
	
	return (( tp.tv_sec - secbase ) + tp.tv_usec / 1000000.0 );
}
#endif


uint32 Plat_MSTime()
{
 	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (unsigned long)(g_FakeBenchmarkTime * 1000.0);
	}

	struct timeval  tp;
	static int      secbase = 0;
	
	gettimeofday( &tp, NULL );
	
	if ( !secbase )
	{
		secbase = tp.tv_sec;
		return ( tp.tv_usec / 1000.0 );
	}
	
	return (unsigned long)(( tp.tv_sec - secbase )*1000.0 + tp.tv_usec / 1000.0 );

}

// Wraps the thread-safe versions of asctime. buf must be at least 26 bytes 
char *Plat_asctime( const struct tm *tm, char *buf, size_t bufsize )
{
	return asctime_r( tm, buf );
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
}


// -------------------------------------------------------------------------------------------------- //
// Memory stuff.
// -------------------------------------------------------------------------------------------------- //

PLATFORM_INTERFACE void Plat_DefaultAllocErrorFn( unsigned long size )
{
}

typedef void (*Plat_AllocErrorFn)( unsigned long size );
Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;

PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size )
{
	void *pRet = g_pMemAlloc->Alloc( size );
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
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_pMemAlloc->Free( ptr );
#else
	free( ptr );
#endif   
}


PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}

static char g_CmdLine[ 2048 ];
PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine )
{
	strncpy( g_CmdLine, cmdLine, sizeof(g_CmdLine) );
	g_CmdLine[ sizeof(g_CmdLine) -1 ] = 0;
}

PLATFORM_INTERFACE void Plat_SetCommandLineArgs( char **argv, int argc )
{
	g_CmdLine[0] = 0;
	for ( int i = 0; i < argc; i++ )
	{
		strncat( g_CmdLine, argv[i], sizeof(g_CmdLine) - strlen(g_CmdLine) );
	}
	
	g_CmdLine[ sizeof(g_CmdLine) -1 ] = 0;
}


PLATFORM_INTERFACE const tchar *Plat_GetCommandLine()
{
	return g_CmdLine;
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


bool Plat_IsInDebugSession()
{
#if defined(OSX)
	int mib[4];
	struct kinfo_proc info;
	size_t size;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = getpid();
	size = sizeof(info);
	info.kp_proc.p_flag = 0;
	sysctl(mib,4,&info,&size,NULL,0);
	bool result = ((info.kp_proc.p_flag & P_TRACED) == P_TRACED);
	return result;
#elif defined(LINUX)
	char s[256];
	snprintf(s, 256, "/proc/%d/cmdline", getppid());
	FILE * fp = fopen(s, "r");
	if (fp != NULL) 
	{
		fread(s, 256, 1, fp);
		fclose(fp);
		return (0 == strncmp(s, "gdb", 3));
	}
	return false;
#endif
}



void Plat_ExitProcess( int nCode )
{
	_exit( nCode );
}

static int s_nWatchDogTimerTimeScale = 0;
static bool s_bInittedWD = false;


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

// watchdog timer support
void BeginWatchdogTimer( int nSecs )
{
	if (! s_bInittedWD )
	{
		s_bInittedWD = true;
		InitWatchDogTimer();
	}
	nSecs *= s_nWatchDogTimerTimeScale;
	nSecs = MIN( nSecs, 5 * 60 );							// no more than 5 minutes no matter what
	if ( nSecs )
		alarm( nSecs );
}

void EndWatchdogTimer( void )
{
	alarm( 0 );
}

static CThreadMutex g_LocalTimeMutex;


void Plat_GetLocalTime( struct tm *pNow )
{
	// We just provide a wrapper on this function so we can protect access to time() everywhere.
	time_t ltime;
	time( &ltime );
	
	Plat_ConvertToLocalTime( ltime, pNow );
}

void Plat_ConvertToLocalTime( uint64 nTime, struct tm *pNow )
{
	// Since localtime() returns a global, we need to protect against multiple threads stomping it.
	g_LocalTimeMutex.Lock();
	
	time_t ltime = (time_t)nTime;
	tm *pTime = localtime( &ltime );
	if ( pTime )
		*pNow = *pTime;
	else
		memset( pNow, 0, sizeof( *pNow ) );
	
	g_LocalTimeMutex.Unlock();
}

void Plat_GetTimeString( struct tm *pTime, char *pOut, int nMaxBytes )
{
	g_LocalTimeMutex.Lock();
	
	char *pStr = asctime( pTime );
	strncpy( pOut, pStr, nMaxBytes );
	pOut[nMaxBytes-1] = 0;
	
	g_LocalTimeMutex.Unlock();
}


void Plat_gmtime( uint64 nTime, struct tm *pTime )
{
	time_t tmtTime = nTime;
	struct tm * tmp = gmtime( &tmtTime );
	* pTime = * tmp;
}

#ifdef LINUX
size_t ApproximateProcessMemoryUsage( void )
{
	int nRet = 0;
	FILE *pFile = fopen( "/proc/self/statm", "r" );
	if ( pFile )
	{
		int nSize, nTotalProgramSize, nResident, nResidentSetSize, nShare, nSharedPagesTotal, nDummy0;
		if ( fscanf( pFile, "%d %d %d %d %d %d %d", &nSize, &nTotalProgramSize, &nResident, &nResidentSetSize, &nShare, &nSharedPagesTotal, &nDummy0 ) )
		{
			nRet = 4096 * nSize;
		}
		fclose( pFile );
	}
	return nRet;
}
#else

size_t ApproximateProcessMemoryUsage( void )
{
	return 0;
}

#endif
