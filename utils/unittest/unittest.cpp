//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "appframework/IAppSystemGroup.h"
#include "appframework/AppFramework.h"
#include "tier0/dbg.h"
#include <stdio.h>
#include "vstdlib/iprocessutils.h"
#include "tier1/interface.h"
#include "vstdlib/cvar.h"

#if defined(POSIX)
#include <dirent.h>
#elif defined(WIN32)
#pragma warning (disable:4100)
#include <windows.h>
#endif

static int g_TestResult = 0;

SpewRetval_t UnitTestSpew( SpewType_t type, char const *pMsg )
{
	switch( type )
	{
    case SPEW_WARNING:
		printf( "UnitTest Warning:\n" );
		break;
    case SPEW_ASSERT:
		printf( "UnitTest Assert:\n" );
        g_TestResult = 1;
		break;
    case SPEW_ERROR:
		printf( "UnitTest Error:\n" );
        g_TestResult = 1;
		break;
	}
	printf( "%s", pMsg );
	fflush(stdout);

	if ( Sys_IsDebuggerPresent() )
		return ( type == SPEW_ASSERT || type == SPEW_ERROR ) ? SPEW_DEBUGGER : SPEW_CONTINUE;
	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CUnitTestApp : public CDefaultAppSystemGroup<CSteamAppSystemGroup>
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual int Main();
	virtual void Destroy();

private:
};

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CUnitTestApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CUnitTestApp::Create()
{
	// Install a special Spew handler that ignores all assertions and lets us
	// run for as long as possible
	SpewOutputFunc( UnitTestSpew );

	// FIXME: This list of dlls should come from the unittests themselves
	AppSystemInfo_t appSystems[] = 
	{
//        { "vstdlib.so",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	// Very simple... just iterate over all .DLLs in the current directory 
	// see if they export UNITTEST_INTERFACE_VERSION. If not, then unload them
	// just as quick.

	// We may want to make this more sophisticated, giving it a search path,
	// or giving test DLLs special extensions, or statically linking the test DLLs
	// to this program.

#ifdef WIN32
	WIN32_FIND_DATA findFileData;
	HANDLE hFind= FindFirstFile("tests/*.dll", &findFileData);

	while (hFind != INVALID_HANDLE_VALUE)
	{
		static char path[2048];
		snprintf(path, sizeof(path), "tests/%s", findFileData.cFileName);

		CSysModule* hLib = Sys_LoadModule(path);
		if ( hLib )
		{
			CreateInterfaceFn factory = Sys_GetFactory( hLib );
			if ( factory && factory( UNITTEST_INTERFACE_VERSION, NULL ) )
			{
				AppModule_t module = LoadModule( factory );
				AddSystem( module, UNITTEST_INTERFACE_VERSION );
			}
			else
			{
				Sys_UnloadModule( hLib );
			}
		}

		if (!FindNextFile( hFind, &findFileData ))
			break;
	}
#elif POSIX
	DIR *d;
	struct dirent *dir;
	d = opendir("tests");
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			int len = strlen(dir->d_name);
			if( len > 2 && strcmp(dir->d_name+len-strlen(DLL_EXT_STRING), DLL_EXT_STRING) == 0)
			{
				static char path[2048];
				snprintf(path, sizeof(path), "tests/%s", dir->d_name);
				CSysModule* hLib = Sys_LoadModule(path);
				if ( hLib )
				{
					CreateInterfaceFn factory = Sys_GetFactory( hLib );
					if ( factory && factory( UNITTEST_INTERFACE_VERSION, NULL ) )
					{
						AppModule_t module = LoadModule( factory );
						AddSystem( module, UNITTEST_INTERFACE_VERSION );
					}
					else
					{
						Sys_UnloadModule( hLib );
					}
				}
			}
		}
		closedir(d);
	}
#else
#error "Implement me!"
#endif

	return true;
}

void CUnitTestApp::Destroy()
{
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CUnitTestApp::Main()
{
    printf( "Valve Software - unittest (%s)\n", __DATE__ );

	int nTestCount = UnitTestCount();
	for ( int i = 0; i < nTestCount; ++i )
	{
		ITestCase* pTestCase = GetUnitTest(i);
		printf("Starting test %s....\n", pTestCase->GetName() );
		pTestCase->RunTest();
	}

    return g_TestResult;
}
