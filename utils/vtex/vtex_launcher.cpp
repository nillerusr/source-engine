//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include "tier1/interface.h"
#include "ilaunchabledll.h"


int main( int argc, char **argv )
{
	const char *pModuleName = "vtex_dll.dll";
	
	CSysModule *pModule = Sys_LoadModule( pModuleName );
	if ( !pModule )
	{
		printf( "Can't load %s.", pModuleName );
		return 1;
	}

	CreateInterfaceFn fn = Sys_GetFactory( pModule );
	if ( !fn )
	{
		printf( "Can't get factory from %s.", pModuleName );
		Sys_UnloadModule( pModule );
		return 1;
	}

	ILaunchableDLL *pInterface = (ILaunchableDLL*)fn( LAUNCHABLE_DLL_INTERFACE_VERSION, NULL );
	if ( !pInterface )
	{
		printf( "Can't get '%s' interface from %s.", LAUNCHABLE_DLL_INTERFACE_VERSION, pModuleName );
		Sys_UnloadModule( pModule );
		return 1;
	}

	int iRet = pInterface->main( argc, argv );
	Sys_UnloadModule( pModule );
	return iRet;
}



