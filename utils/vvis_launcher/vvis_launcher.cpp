//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
// vvis_launcher.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <direct.h>
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "ilaunchabledll.h"


char* GetLastErrorString()
{
	static char err[2048];

	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);

	strncpy( err, (char*)lpMsgBuf, sizeof( err ) );
	LocalFree( lpMsgBuf );

	err[ sizeof( err ) - 1 ] = 0;

	return err;
}


int main(int argc, char* argv[])
{
	CommandLine()->CreateCmdLine( argc, argv );
#ifndef MAPBASE
	const char *pDLLName = "vvis_dll.dll";
	CSysModule *pModule = Sys_LoadModule( pDLLName );

	if ( !pModule )
	{
		printf( "vvis launcher error: can't load %s\n%s", pDLLName, GetLastErrorString() );
		return 1;
	}
#else
	// Coming through!
	const char *pDLLName = "vvis_dll.dll";

	// With this, we just load the DLL with our filename.
	// This allows for custom DLLs without having to bother with the launcher.
	char filename[128];
	Q_FileBase(argv[0], filename, sizeof(filename));
	Q_snprintf(filename, sizeof(filename), "%s_dll.dll", filename);
	pDLLName = filename;

	CSysModule *pModule = Sys_LoadModule( pDLLName );
	if ( !pModule )
	{
		// Try loading the default then
		pDLLName = "vvis_dll.dll";
		pModule = Sys_LoadModule( pDLLName );
	}

	if ( !pModule )
	{
		printf( "vvis launcher error: can't load %s\n%s", pDLLName, GetLastErrorString() );
		return 1;
	}
#endif // MAPBASE

	CreateInterfaceFn fn = Sys_GetFactory( pModule );
	if( !fn )
	{
		printf( "vvis launcher error: can't get factory from %s\n", pDLLName );
		Sys_UnloadModule( pModule );
		return 2;
	}

	int retCode = 0;
	ILaunchableDLL *pDLL = (ILaunchableDLL*)fn( LAUNCHABLE_DLL_INTERFACE_VERSION, &retCode );
	if( !pDLL )
	{
		printf( "vvis launcher error: can't get IVVisDLL interface from %s\n", pDLLName );
		Sys_UnloadModule( pModule );
		return 3;
	}

	pDLL->main( argc, argv );
	Sys_UnloadModule( pModule );

	return 0;
}

