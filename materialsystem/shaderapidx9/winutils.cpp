//========= Copyright Valve Corporation, All rights reserved. ============//
//
// winutils.cpp
//
//===========================================================================//

#include "winutils.h"

#ifndef _WIN32

#include "appframework/ilaunchermgr.h"

// LINUX path taken from //Steam/main/src/tier0/platform_posix.cpp - Returns installed RAM in MB. 
static unsigned long GetInstalledRAM()
{
	unsigned long ulTotalRamMB = 2047;

#ifdef LINUX
	char rgchLine[256];
	FILE *fpMemInfo = fopen( "/proc/meminfo", "r" );
	if ( !fpMemInfo )
		return ulTotalRamMB;

	const char *pszSearchString = "MemTotal:";
	const uint cchSearchString = strlen( pszSearchString );
	while ( fgets( rgchLine, sizeof(rgchLine), fpMemInfo ) )
	{
		if ( !strncasecmp( pszSearchString, rgchLine, cchSearchString ) )
		{
			char *pszVal = rgchLine+cchSearchString;
			while( isspace(*pszVal) )
				++pszVal;
			ulTotalRamMB = atol( pszVal ) / 1024; // go from kB to MB
			break;
		}
	}
	fclose( fpMemInfo );
#endif

	// 128 Gb limit for now (should future proof us for a while)
	ulTotalRamMB = MIN( ulTotalRamMB, 1024 * 128 );
	return ulTotalRamMB;
}

void GlobalMemoryStatus( MEMORYSTATUS *pOut )
{
	unsigned long nInstalledRamInMB = GetInstalledRAM();

	// For safety assume at least 128MB
	nInstalledRamInMB = MAX( nInstalledRamInMB, 128 );

	uint64 ulTotalRam = static_cast<uint64>( nInstalledRamInMB ) * ( 1024 * 1024 );
	ulTotalRam = MIN( ulTotalRam, 0xFFFFFFFF );
	
	pOut->dwTotalPhys = static_cast<SIZE_T>( ulTotalRam );
}

void Sleep( unsigned int ms )
{
	DebuggerBreak();
	ThreadSleep( ms );
}

bool IsIconic( VD3DHWND hWnd )
{
	// FIXME for now just act non-minimized all the time
	//DebuggerBreak();
	return false;
}

BOOL ClientToScreen( VD3DHWND hWnd, LPPOINT pPoint )
{
	DebuggerBreak();
	return true;
}

void* GetCurrentThread()
{
	DebuggerBreak();
	return 0;
}

void SetThreadAffinityMask( void *hThread, int nMask )
{
	DebuggerBreak();
}

bool GUID::operator==( const struct _GUID &other ) const
{
	DebuggerBreak();
	return memcmp( this, &other, sizeof( GUID ) ) == 0;
}
#endif
