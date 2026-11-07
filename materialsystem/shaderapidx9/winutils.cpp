//========= Copyright Valve Corporation, All rights reserved. ============//
//
// winutils.cpp
//
//===========================================================================//

#include "winutils.h"

#ifndef _WIN32

#include "appframework/ilaunchermgr.h"
#include <ctype.h>
#ifdef DXVK
#include <SDL.h>
#endif

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

// With ToGL these functions are never expected to be called (DebuggerBreak); the real D3D9
// path used with DXVK-native does call them.

void Sleep( unsigned int ms )
{
#ifndef DXVK
	DebuggerBreak();
#endif
	ThreadSleep( ms );
}

#ifdef DXVK
// DXVK-native: HWND is the SDL_Window*

bool IsIconic( VD3DHWND hWnd )
{
	return hWnd && ( SDL_GetWindowFlags( (SDL_Window *)hWnd ) & SDL_WINDOW_MINIMIZED );
}

BOOL GetClientRect( VD3DHWND hWnd, RECT *pRect )
{
	int w = 0, h = 0;
	if ( hWnd )
		SDL_GetWindowSize( (SDL_Window *)hWnd, &w, &h );
	pRect->left = pRect->top = 0;
	pRect->right = w;
	pRect->bottom = h;
	return hWnd != NULL;
}

BOOL ClientToScreen( VD3DHWND hWnd, LPPOINT pPoint )
{
	int x = 0, y = 0;
	if ( hWnd )
		SDL_GetWindowPosition( (SDL_Window *)hWnd, &x, &y );
	pPoint->x += x;
	pPoint->y += y;
	return TRUE;
}

#else

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

#endif // DXVK

void* GetCurrentThread()
{
#ifndef DXVK
	DebuggerBreak();
#endif
	return 0;
}

void SetThreadAffinityMask( void *hThread, int nMask )
{
#ifndef DXVK
	DebuggerBreak();
#endif
}

#ifndef DXVK // DXVK's GUID has its own operator==
bool GUID::operator==( const struct _GUID &other ) const
{
	DebuggerBreak();
	return memcmp( this, &other, sizeof( GUID ) ) == 0;
}
#endif
#endif
