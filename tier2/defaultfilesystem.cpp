//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include <tier0/platform.h>
#include <tier2/tier2.h>
#include <filesystem_init.h>


static CSysModule *g_pFullFileSystemModule = NULL;

void* DefaultCreateInterfaceFn(const char *pName, int *pReturnCode)
{
	if ( pReturnCode )
	{
		*pReturnCode = 0;
	}
	return NULL;
}

void InitDefaultFileSystem( void )
{
	AssertMsg( !g_pFullFileSystem, "Already set up the file system" );

	if ( !Sys_LoadInterface( "filesystem_stdio", FILESYSTEM_INTERFACE_VERSION,
		&g_pFullFileSystemModule, (void**)&g_pFullFileSystem ) )
	{
		if ( !Sys_LoadInterface( "filesystem_steam", FILESYSTEM_INTERFACE_VERSION,
			&g_pFullFileSystemModule, (void**)&g_pFullFileSystem ) )
		{
			exit(0);
		}
	}

	if ( !g_pFullFileSystem->Connect( DefaultCreateInterfaceFn ) )
	{
		exit(0);
	}

	if ( g_pFullFileSystem->Init() != INIT_OK )
	{
		exit(0);
	}

	g_pFullFileSystem->RemoveAllSearchPaths();
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD );
}

void ShutdownDefaultFileSystem(void)
{
	AssertMsg( g_pFullFileSystem, "File system not set up" );
	g_pFullFileSystem->Shutdown();
	g_pFullFileSystem->Disconnect();
	Sys_UnloadModule( g_pFullFileSystemModule );
}
