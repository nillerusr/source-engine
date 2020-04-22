//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "pch_tier0.h"
#include <time.h>

#if defined(_WIN32) && !defined(_X360)
#include <errno.h>
#endif
#include <assert.h>
#include "tier0/platform.h"
#include "tier0/minidump.h"
#ifdef _X360
#include "xbox/xbox_console.h"
#include "xbox/xbox_win32stubs.h"
#else
#include "tier0/vcrmode.h"
#endif
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/memalloc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#endif

//our global error callback function. Note that this is not initialized, but static space guarantees this is NULL at app start.
//If you initialize, it will set to zero again when the CPP runs its static initializers, which could stomp the value if another
//CPP sets this value while initializing its static space
static ExitProcessWithErrorCBFn g_pfnExitProcessWithErrorCB; //= NULL

#ifndef _X360
extern VCRMode_t g_VCRMode;
#endif
static LARGE_INTEGER g_PerformanceFrequency;
static double g_PerformanceCounterToS;
static double g_PerformanceCounterToMS;
static double g_PerformanceCounterToUS;
static LARGE_INTEGER g_ClockStart;
static bool s_bTimeInitted;

// Benchmark mode uses this heavy-handed method 
static bool g_bBenchmarkMode = false;
static double g_FakeBenchmarkTime = 0;
static double g_FakeBenchmarkTimeInc = 1.0 / 66.0;

static void InitTime()
{
	if( !s_bTimeInitted )
	{
		s_bTimeInitted = true;
		QueryPerformanceFrequency(&g_PerformanceFrequency);
		g_PerformanceCounterToS = 1.0 / g_PerformanceFrequency.QuadPart;
		g_PerformanceCounterToMS = 1e3 / g_PerformanceFrequency.QuadPart;
		g_PerformanceCounterToUS = 1e6 / g_PerformanceFrequency.QuadPart;
		QueryPerformanceCounter(&g_ClockStart);
	}
}

bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}

double Plat_FloatTime()
{
	if (! s_bTimeInitted )
		InitTime();
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}

	LARGE_INTEGER CurrentTime;

	QueryPerformanceCounter( &CurrentTime );

	double fRawSeconds = (double)( CurrentTime.QuadPart - g_ClockStart.QuadPart ) * g_PerformanceCounterToS;

	return fRawSeconds;
}

uint32 Plat_MSTime()
{
	if (! s_bTimeInitted )
		InitTime();
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (uint32)(g_FakeBenchmarkTime * 1000.0);
	}

	LARGE_INTEGER CurrentTime;

	QueryPerformanceCounter( &CurrentTime );

	return (uint32) ( ( CurrentTime.QuadPart - g_ClockStart.QuadPart ) * g_PerformanceCounterToMS );
}

uint64 Plat_USTime()
{
	if (! s_bTimeInitted )
		InitTime();
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (uint64)(g_FakeBenchmarkTime * 1e6);
	}

	LARGE_INTEGER CurrentTime;

	QueryPerformanceCounter( &CurrentTime );

	return (uint64) ( ( CurrentTime.QuadPart - g_ClockStart.QuadPart ) * g_PerformanceCounterToUS );
}

void GetCurrentDate( int *pDay, int *pMonth, int *pYear )
{
	struct tm *pNewTime;
	time_t long_time;

	time( &long_time );                /* Get time as long integer. */
	pNewTime = localtime( &long_time ); /* Convert to local time. */

	*pDay = pNewTime->tm_mday;
	*pMonth = pNewTime->tm_mon + 1;
	*pYear = pNewTime->tm_year + 1900;
}


// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
	if ( EINVAL == ctime_s( buf, bufsize, timep ) )
		return NULL;
	else
		return buf;
}

void Plat_GetModuleFilename( char *pOut, int nMaxBytes )
{
#ifdef PLATFORM_WINDOWS_PC
	SetLastError( ERROR_SUCCESS ); // clear the error code
	GetModuleFileName( NULL, pOut, nMaxBytes );
	if ( GetLastError() != ERROR_SUCCESS )
		Error( "Plat_GetModuleFilename: The buffer given is too small (%d bytes).", nMaxBytes );
#elif PLATFORM_X360
	pOut[0] = 0x00;		// return null string on Xbox 360
#else
	// We shouldn't need this on POSIX.
	Assert( false );
	pOut[0] = 0x00;    // Null the returned string in release builds
#endif
}

void Plat_ExitProcess( int nCode )
{
#if defined( _WIN32 ) && !defined( _X360 )
	// We don't want global destructors in our process OR in any DLL to get executed.
	// _exit() avoids calling global destructors in our module, but not in other DLLs.
	const char *pchCmdLineA = Plat_GetCommandLineA();
	if ( nCode || ( strstr( pchCmdLineA, "gc.exe" ) && strstr( pchCmdLineA, "gc.dll" ) && strstr( pchCmdLineA, "-gc" ) ) )
	{
		int *x = NULL; *x = 1; // cause a hard crash, GC is not allowed to exit voluntarily from gc.dll
	}
	TerminateProcess( GetCurrentProcess(), nCode );
#elif defined(_PS3)
	// We do not use this path to exit on PS3 (naturally), rather we want a clear crash:
	int *x = NULL; *x = 1;
#else	
	_exit( nCode );
#endif
}

void Plat_ExitProcessWithError( int nCode, bool bGenerateMinidump )
{
	//try to delegate out if they have registered a callback
	if( g_pfnExitProcessWithErrorCB )
	{
		if( g_pfnExitProcessWithErrorCB( nCode ) )
			return;
	}

	//handle default behavior
	if( bGenerateMinidump )
	{
		//don't generate mini dumps in the debugger
		if( !Plat_IsInDebugSession() )
		{
			WriteMiniDump();
		}
	}

	//and exit our process
	Plat_ExitProcess( nCode );
}

void Plat_SetExitProcessWithErrorCB( ExitProcessWithErrorCBFn pfnCB )
{
	g_pfnExitProcessWithErrorCB = pfnCB;	
}

// Wraps the thread-safe versions of gmtime
struct tm *Plat_gmtime( const time_t *timep, struct tm *result )
{
	if ( EINVAL == gmtime_s( result, timep ) )
		return NULL;
	else
		return result;
}


time_t Plat_timegm( struct tm *timeptr )
{
	return _mkgmtime( timeptr );
}


// Wraps the thread-safe versions of localtime
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
	if ( EINVAL == localtime_s( result, timep ) )
		return NULL;
	else
		return result;
}


bool vtune( bool resume )
{
#ifndef _X360
	static bool bInitialized = false;
	static void (__cdecl *VTResume)(void) = NULL;
	static void (__cdecl *VTPause) (void) = NULL;

	// Grab the Pause and Resume function pointers from the VTune DLL the first time through:
	if( !bInitialized )
	{
		bInitialized = true;

		HINSTANCE pVTuneDLL = LoadLibrary( "vtuneapi.dll" );

		if( pVTuneDLL )
		{
			VTResume = (void(__cdecl *)())GetProcAddress( pVTuneDLL, "VTResume" );
			VTPause  = (void(__cdecl *)())GetProcAddress( pVTuneDLL, "VTPause" );
		}
	}

	// Call the appropriate function, as indicated by the argument:
	if( resume && VTResume )
	{
		VTResume();
		return true;

	} 
	else if( !resume && VTPause )
	{
		VTPause();
		return true;
	}
#endif
	return false;
}

bool Plat_IsInDebugSession()
{
#if defined( _WIN32 ) && !defined( _X360 )
	return (IsDebuggerPresent() != 0);
#elif defined( _WIN32 ) && defined( _X360 )
	return (XBX_IsDebuggerPresent() != 0);
#elif defined( LINUX )
	#error This code is implemented in platform_posix.cpp
#else
	return false;
#endif
}

void Plat_DebugString( const char * psz )
{
#if defined( _WIN32 ) && !defined( _X360 )
	::OutputDebugStringA( psz );
#elif defined( _WIN32 ) && defined( _X360 )
	XBX_OutputDebugString( psz );
#endif
}


const tchar *Plat_GetCommandLine()
{
#ifdef TCHAR_IS_WCHAR
	return GetCommandLineW();
#else
	return GetCommandLine();
#endif
}

bool GetMemoryInformation( MemoryInformation *pOutMemoryInfo )
{
	if ( !pOutMemoryInfo ) 
		return false;

	MEMORYSTATUSEX	memStat;
	ZeroMemory( &memStat, sizeof( MEMORYSTATUSEX ) );
	memStat.dwLength = sizeof( MEMORYSTATUSEX );

	if ( !GlobalMemoryStatusEx( &memStat ) ) 
		return false;

	const uint cOneMb = 1024 * 1024;

	switch ( pOutMemoryInfo->m_nStructVersion )
	{
	case 0:
		( *pOutMemoryInfo ).m_nPhysicalRamMbTotal     = memStat.ullTotalPhys / cOneMb;
		( *pOutMemoryInfo ).m_nPhysicalRamMbAvailable = memStat.ullAvailPhys / cOneMb;

		( *pOutMemoryInfo ).m_nVirtualRamMbTotal      = memStat.ullTotalVirtual / cOneMb;
		( *pOutMemoryInfo ).m_nVirtualRamMbAvailable  = memStat.ullAvailVirtual / cOneMb;
		break;

	default:
		return false;
	};

	return true;
}


const char *Plat_GetCommandLineA()
{
	return GetCommandLineA();
}

//--------------------------------------------------------------------------------------------------
// Watchdog timer
//--------------------------------------------------------------------------------------------------
void Plat_BeginWatchdogTimer( int nSecs )
{
}
void Plat_EndWatchdogTimer( void )
{
}
int Plat_GetWatchdogTime( void )
{
	return 0;
}
void Plat_SetWatchdogHandlerFunction( Plat_WatchDogHandlerFunction_t function )
{
}

bool Is64BitOS()
{
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	static LPFN_ISWOW64PROCESS pfnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress( GetModuleHandle("kernel32"), "IsWow64Process" );

	static BOOL bIs64bit = FALSE;
	static bool bInitialized = false;
	if ( bInitialized ) 
		return bIs64bit == (BOOL)TRUE;
	else
	{
		bInitialized = true;
		return pfnIsWow64Process && pfnIsWow64Process(GetCurrentProcess(), &bIs64bit) && bIs64bit;
	}
}


// -------------------------------------------------------------------------------------------------- //
// Memory stuff. 
//
// DEPRECATED. Still here to support binary back compatability of tier0.dll
//
// -------------------------------------------------------------------------------------------------- //
#ifndef _X360
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

typedef void (*Plat_AllocErrorFn)( unsigned long size );

void Plat_DefaultAllocErrorFn( unsigned long size )
{
}

Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;
#endif

#ifndef _X360
CRITICAL_SECTION g_AllocCS;
class CAllocCSInit
{
public:
	CAllocCSInit()
	{
		InitializeCriticalSection( &g_AllocCS );
	}
} g_AllocCSInit;
#endif

#ifndef _X360
PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size )
{
	EnterCriticalSection( &g_AllocCS );
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
		void *pRet = g_pMemAlloc->Alloc( size );
#else
		void *pRet = malloc( size );
#endif
	LeaveCriticalSection( &g_AllocCS );
	if ( pRet )
	{
		return pRet;
	}
	else
	{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
		g_AllocError( size );
#endif
		return 0;
	}
}
#endif

#ifndef _X360
PLATFORM_INTERFACE void* Plat_Realloc( void *ptr, unsigned long size )
{
	EnterCriticalSection( &g_AllocCS );
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
		void *pRet = g_pMemAlloc->Realloc( ptr, size );
#else
		void *pRet = realloc( ptr, size );
#endif
	LeaveCriticalSection( &g_AllocCS );
	if ( pRet )
	{
		return pRet;
	}
	else
	{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
		g_AllocError( size );
#endif
		return 0;
	}
}
#endif

#ifndef _X360
PLATFORM_INTERFACE void Plat_Free( void *ptr )
{
	EnterCriticalSection( &g_AllocCS );
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
		g_pMemAlloc->Free( ptr );
#else
		free( ptr );
#endif
	LeaveCriticalSection( &g_AllocCS );
}
#endif

#ifndef _X360
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}
#endif
#endif

#endif
