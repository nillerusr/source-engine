//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "pch_tier0.h"
#include <time.h>

#if defined(_WIN32) && !defined(_X360)
#define WINDOWS_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0403
#include <windows.h>
#endif
#include <errno.h>
#include <assert.h>
#include "tier0/platform.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif
#include "tier0/threadtools.h"

#include "tier0/memalloc.h"

#if defined( _PS3 )
#include <cell/fios/fios_common.h>
#include <cell/fios/fios_memory.h>
#include <cell/fios/fios_configuration.h>
#include <sys/process.h>

#if !defined(_CERT)
#include "sn/LibSN.h"
#endif

/*
#include <sys/types.h>
#include <sys/process.h>
#include <sys/prx.h>

#include <sysutil/sysutil_syscache.h>

#include <cell/sysmodule.h>
*/
#include <cell/fios/fios_time.h>
#endif // _PS3

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _WIN32
static LARGE_INTEGER g_PerformanceFrequency;
static LARGE_INTEGER g_MSPerformanceFrequency;
static LARGE_INTEGER g_ClockStart;
static bool s_bTimeInitted;
#endif

// Benchmark mode uses this heavy-handed method 
static bool g_bBenchmarkMode = false;
#ifdef _WIN32
static double g_FakeBenchmarkTime = 0;
static double g_FakeBenchmarkTimeInc = 1.0 / 66.0;
#endif

static CThreadFastMutex g_LocalTimeMutex;


#ifdef _WIN32
static void InitTime()
{
	if( !s_bTimeInitted )
	{
		s_bTimeInitted = true;
		QueryPerformanceFrequency(&g_PerformanceFrequency);
		g_MSPerformanceFrequency.QuadPart = g_PerformanceFrequency.QuadPart / 1000;
		QueryPerformanceCounter(&g_ClockStart);
	}
}
#endif

bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}

#ifdef _PS3
cell::fios::abstime_t g_fiosLaunchTime = 0;
#endif


double Plat_FloatTime()
{
#ifdef _WIN32
	if (! s_bTimeInitted )
		InitTime();
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}

	LARGE_INTEGER CurrentTime;

	QueryPerformanceCounter( &CurrentTime );

	double fRawSeconds = (double)( CurrentTime.QuadPart - g_ClockStart.QuadPart ) / (double)(g_PerformanceFrequency.QuadPart);

	return fRawSeconds;
#else
	return cell::fios::FIOSAbstimeToMicroseconds( cell::fios::FIOSGetCurrentTime() - g_fiosLaunchTime ) * 1e-6;
#endif
}


uint32 Plat_MSTime()
{
#ifdef _WIN32
	if (! s_bTimeInitted )
		InitTime();
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (uint32)(g_FakeBenchmarkTime * 1000.0);
	}

	LARGE_INTEGER CurrentTime;

	QueryPerformanceCounter( &CurrentTime );

	return (uint32) ( ( CurrentTime.QuadPart - g_ClockStart.QuadPart ) / g_MSPerformanceFrequency.QuadPart);
#elif defined(_PS3)
	return (uint32) cell::fios::FIOSAbstimeToMilliseconds( cell::fios::FIOSGetCurrentTime() - g_fiosLaunchTime );
#else
	#error
#endif	
}

uint64 Timer_GetTimeUS()
{
#ifdef _PS3
	return cell::fios::FIOSAbstimeToMicroseconds( cell::fios::FIOSGetCurrentTime() - g_fiosLaunchTime );
#else
	return uint64( Plat_FloatTime() * 1000000 );
#endif
}

uint64 Plat_GetClockStart()
{
#if defined( _WIN32 )
	if ( !s_bTimeInitted )
		InitTime();

	return g_ClockStart.QuadPart;
#elif defined( _PS3 )
	return g_fiosLaunchTime;
#else
	return 0;
#endif
}

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
#ifdef _PS3
	struct tm * tmp = gmtime( &tmtTime );
	* pTime = * tmp;
#else
	gmtime_s( pTime, &tmtTime );
#endif
}

time_t Plat_timegm( struct tm *timeptr )
{
#ifndef _GAMECONSOLE
	return _mkgmtime( timeptr );
#else
	int *pnCrashHereBecauseConsolesDontSupportMkGmTime = 0;
	*pnCrashHereBecauseConsolesDontSupportMkGmTime = 0;
	return 0;
#endif
}

void Plat_GetModuleFilename( char *pOut, int nMaxBytes )
{
#ifdef PLATFORM_WINDOWS_PC
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
	TerminateProcess( GetCurrentProcess(), nCode );
#elif defined(_PS3)
	// We do not use this path to exit on PS3 (naturally), rather we want a clear crash:
	int *x = NULL; *x = 1;
#else	
	_exit( nCode );
#endif
}

void GetCurrentDate( int *pDay, int *pMonth, int *pYear )
{
	struct tm long_time;
	Plat_GetLocalTime( &long_time );

	*pDay = long_time.tm_mday;
	*pMonth = long_time.tm_mon + 1;
	*pYear = long_time.tm_year + 1900;
}

// Wraps the thread-safe versions of asctime. buf must be at least 26 bytes 
char *Plat_asctime( const struct tm *tm, char *buf, size_t bufsize )
{
#ifdef _PS3
	snprintf( buf, bufsize, "%s", asctime(tm) );
	return buf;
#else
	if ( EINVAL == asctime_s( buf, bufsize, tm ) )
		return NULL;
	else
		return buf;
#endif
}


// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
#ifdef _PS3
	snprintf( buf, bufsize, "%s", ctime( timep ) );
	return buf;
#else
	if ( EINVAL == ctime_s( buf, bufsize, timep ) )
		return NULL;
	else
		return buf;
#endif
}


// Wraps the thread-safe versions of gmtime
struct tm *Plat_gmtime( const time_t *timep, struct tm *result )
{
#ifdef _PS3
	*result = *gmtime( timep );
	return result;
#else
	if ( EINVAL == gmtime_s( result, timep ) )
		return NULL;
	else
		return result;
#endif
}


// Wraps the thread-safe versions of localtime
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
#ifdef _PS3
	*result = *localtime( timep );
	return result;
#else
	if ( EINVAL == localtime_s( result, timep ) )
		return NULL;
	else
		return result;
#endif
}

bool vtune( bool resume )
{
#if IS_WINDOWS_PC
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
#if defined( _X360 )
	return (XBX_IsDebuggerPresent() != 0);
#elif defined( _WIN32 )
	return (IsDebuggerPresent() != 0);
#elif defined( _PS3 ) && !defined(_CERT)
	return snIsDebuggerPresent();
#else
	return false;
#endif
}

void Plat_DebugString( const char * psz )
{
#ifdef _CERT
	return; // do nothing!
#endif

#if defined( _X360 )
	XBX_OutputDebugString( psz );
#elif defined( _WIN32 )
	::OutputDebugStringA( psz );
#elif defined(_PS3)
	printf("%s",psz);
#else 
	// do nothing?
#endif
}


#if defined( PLATFORM_WINDOWS_PC )
void Plat_MessageBox( const char *pTitle, const char *pMessage )
{
	MessageBox( NULL, pMessage, pTitle, MB_OK );
}
#endif

PlatOSVersion_t Plat_GetOSVersion()
{
#ifdef PLATFORM_WINDOWS_PC
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if ( GetVersionEx( &info ) )
		return (PlatOSVersion_t)info.dwMajorVersion;
	return PLAT_OS_VERSION_UNKNOWN;
#elif defined( PLATFORM_X360 )
	return PLAT_OS_VERSION_XBOX360;
#else
	return PLAT_OS_VERSION_UNKNOWN;
#endif
}

#if defined( PLATFORM_PS3 )
//copied from platform_posix.cpp
static char g_CmdLine[ 2048 ] = "";
PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine )
{
	strncpy( g_CmdLine, cmdLine, sizeof(g_CmdLine) );
	g_CmdLine[ sizeof(g_CmdLine) -1 ] = 0;
}
#endif

PLATFORM_INTERFACE const tchar *Plat_GetCommandLine()
{
#if defined( _PS3 )
#pragma message("Plat_GetCommandLine() not implemented on PS3")	// ****
	return g_CmdLine;
#elif defined( TCHAR_IS_WCHAR )
	return GetCommandLineW();
#else
	return GetCommandLine();
#endif
}

PLATFORM_INTERFACE const char *Plat_GetCommandLineA()
{
#if defined( _PS3 )
#pragma message("Plat_GetCommandLineA() not implemented on PS3")	// ****
	return g_CmdLine;
#else
	return GetCommandLineA();
#endif
}


//-----------------------------------------------------------------------------
// Dynamically load a function
//-----------------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS

void *Plat_GetProcAddress( const char *pszModule, const char *pszName )
{
	HMODULE hModule = ::LoadLibrary( pszModule );
	return ( hModule ) ? ::GetProcAddress( hModule, pszName ) : NULL;
}

#endif



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

#if !defined( _X360 ) && !defined( _PS3 )


CRITICAL_SECTION g_AllocCS;
class CAllocCSInit
{
public:
	CAllocCSInit()
	{
		InitializeCriticalSection( &g_AllocCS );
	}
} g_AllocCSInit;


PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size )
{
	EnterCriticalSection( &g_AllocCS );
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
		void *pRet = MemAlloc_Alloc( size );
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

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}
#endif

#endif


#endif