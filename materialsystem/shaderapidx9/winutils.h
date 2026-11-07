//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//==================================================================================================
#ifndef WINUTILS_H
#define WINUTILS_H

#include "togl/rendermechanism.h" // for win types

#if !defined(_WIN32)

	void Sleep( unsigned int ms );
	bool IsIconic( VD3DHWND hWnd );
	BOOL ClientToScreen( VD3DHWND hWnd, LPPOINT pPoint );
	void* GetCurrentThread();
	void SetThreadAffinityMask( void *hThread, int nMask );
	void GlobalMemoryStatus( MEMORYSTATUS *pOut );

#if defined( DXVK )
	// DXVK-native: HWND is the SDL_Window* (implemented with SDL in winutils.cpp);
	// with ToGL this one comes from togl.
	BOOL GetClientRect( VD3DHWND hWnd, RECT *pRect );
#endif
#endif

#endif // WINUTILS_H
