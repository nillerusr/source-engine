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
#endif

#endif // WINUTILS_H
