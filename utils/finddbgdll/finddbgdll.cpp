//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Finds debug DLLs
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include <stdio.h>
#include <windows.h>

#pragma warning (disable:4100)

SpewRetval_t FindDbgDLLSpew( SpewType_t type, char const *pMsg )
{
	printf( "%s", pMsg );
	OutputDebugString( pMsg );
	return SPEW_CONTINUE;
}


/*
============
main
============
*/
int main (int argc, char **argv)
{
	printf( "Valve Software - finddbgdll.exe (%s)\n", __DATE__ );

	// Install a special Spew handler that ignores all assertions and lets us
	// run for as long as possible
	SpewOutputFunc( FindDbgDLLSpew );

	// Very simple... just iterate over all .DLLs in the current directory and call
	// LoadLibrary on them; check to see if they define the BuiltDebug symbol. I

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile("*.dll", &findFileData);

	while (hFind != INVALID_HANDLE_VALUE)
	{
		// Ignore 360 DLLs, can't load them on the PC
		if ( !Q_strstr( findFileData.cFileName, "360" ) )
		{
			HMODULE hLib = LoadLibrary(findFileData.cFileName);

			if ( GetProcAddress( hLib, "BuiltDebug" ) )
			{
				Msg( "Module %s is a debug build\n", findFileData.cFileName );
			}

			FreeLibrary( hLib );
		}

		if (!FindNextFile( hFind, &findFileData ))
			break;
	}

	return 0;
}

