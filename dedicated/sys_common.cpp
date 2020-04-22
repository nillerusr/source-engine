//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#ifdef _WIN32
#include <windows.h> 
#elif POSIX
#include <unistd.h>
#else
#error
#endif
#include <stdio.h>
#include <stdlib.h>
#include "isys.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "filesystem.h"
#include "tier0/vcrmode.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "idedicatedexports.h"
#include "vgui/vguihelpers.h"

static long		hDLLThirdParty	= 0L;

//-----------------------------------------------------------------------------
// Modules...
//-----------------------------------------------------------------------------
CSysModule *s_hMatSystemModule = NULL;	
CSysModule *s_hEngineModule = NULL;
CSysModule *s_hSoundEmitterModule = NULL;

CreateInterfaceFn s_MaterialSystemFactory;
CreateInterfaceFn s_EngineFactory;
CreateInterfaceFn s_SoundEmitterFactory;

/*
==============
Load3rdParty

Load support for third party .dlls ( gamehost )
==============
*/
void Load3rdParty( void )
{
	// Only do this if the server operator wants the support.
	// ( In case of malicious code, too )
	if ( CommandLine()->CheckParm( "-usegh" ) )   
	{
		hDLLThirdParty = sys->LoadLibrary( "ghostinj.dll" );
	}
}

/*
==============
EF_VID_ForceUnlockedAndReturnState

Dummy funcion called by engine
==============
*/
int  EF_VID_ForceUnlockedAndReturnState(void)
{
	return 0;
}

/*
==============
EF_VID_ForceLockState

Dummy funcion called by engine
==============
*/
void EF_VID_ForceLockState(int)
{
}

/*
==============
InitInstance

==============
*/
bool InitInstance( )
{
	Load3rdParty();

	return true;
}

/*
==============
ProcessConsoleInput

==============
*/
int ProcessConsoleInput(void)
{
	char *s;
	int count = 0;

	if ( engine )
	{
		do
		{
			char szBuf[ 256 ];
			s = sys->ConsoleInput( count++, szBuf, sizeof( szBuf ) );
			if (s && s[0] )
			{
				V_strcat_safe( szBuf, "\n" );
				engine->AddConsoleText ( szBuf );
			}
		} while (s);
	}

	return count;
}

void RunServer( void );

class CDedicatedExports : public CBaseAppSystem<IDedicatedExports>
{
public:
	virtual void Sys_Printf( char *text )
	{
		if ( sys )
		{
			sys->Printf( "%s", text );
		}
	}

	virtual void RunServer()
	{
		void RunServer( void );
		::RunServer();
	}
};

EXPOSE_SINGLE_INTERFACE( CDedicatedExports, IDedicatedExports, VENGINE_DEDICATEDEXPORTS_API_VERSION );

static const char *get_consolelog_filename()
{
	static bool s_bInited = false;
	static char s_consolelog[ MAX_PATH ];

	if ( !s_bInited )
	{
		s_bInited = true;

		// Don't do the -consolelog thing if -consoledebug is present.
		//  CTextConsoleUnix::Print() looks for -consoledebug.
		const char *filename = NULL;
		if ( !CommandLine()->FindParm( "-consoledebug" ) &&
			  CommandLine()->CheckParm( "-consolelog", &filename ) &&
			  filename )
		{
			V_strcpy_safe( s_consolelog, filename );
		}
	}

	return s_consolelog;
}

SpewRetval_t DedicatedSpewOutputFunc( SpewType_t spewType, char const *pMsg )
{
	if ( sys )
	{
		sys->Printf( "%s", pMsg );

		// If they have specified -consolelog, log this message there. Otherwise these
		//	wind up being lost because Sys_InitGame hasn't been called yet, and 
		//  Sys_SpewFunc is the thing that logs stuff to -consolelog, etc.
		const char *filename = get_consolelog_filename();
		if ( filename[ 0 ] && pMsg[ 0 ] )
		{
			FileHandle_t fh = g_pFullFileSystem->Open( filename, "a" );
			if ( fh != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->Write( pMsg, V_strlen( pMsg ), fh );
				g_pFullFileSystem->Close( fh );
			}
		}
	}
#ifdef _WIN32
	Plat_DebugString( pMsg );
#endif

	if (spewType == SPEW_ERROR)
	{
		// In Windows vgui mode, make a message box or they won't ever see the error.
#ifdef _WIN32
		extern bool g_bVGui;
		if ( g_bVGui )
		{
			MessageBox( NULL, pMsg, "Error", MB_OK | MB_TASKMODAL );
		}
		TerminateProcess( GetCurrentProcess(), 1 );
#elif POSIX
		fflush(stdout);
		_exit(1);
#else
#error "Implement me"
#endif
		
		return SPEW_ABORT;
	}
	if (spewType == SPEW_ASSERT)
	{
		if ( CommandLine()->FindParm( "-noassert" ) == 0 )
			return SPEW_DEBUGGER;
		else
			return SPEW_CONTINUE;
	}
	return SPEW_CONTINUE;
}

int Sys_GetExecutableName( char *out )
{
#ifdef _WIN32
	if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), out, 256 ) )
	{
		return 0;
	}
#else
	strcpy( out, g_szEXEName );
#endif
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
const char *UTIL_GetExecutableDir( )
{
	static char	exedir[ MAX_PATH ];

	exedir[ 0 ] = 0;
	if ( !Sys_GetExecutableName(exedir) )
		return NULL;

	char *pSlash;
	char *pSlash2;
	pSlash = strrchr( exedir,'\\' );
	pSlash2 = strrchr( exedir,'/' );
	if ( pSlash2 > pSlash )
	{
		pSlash = pSlash2;
	}
	if (pSlash)
	{
		*pSlash = 0;
	}

	// Return the bin directory as the executable dir if it's not in there
	// because that's really where we're running from...
	int exeLen = strlen(exedir);
	if ( 	exedir[exeLen-4] != CORRECT_PATH_SEPARATOR || 
		exedir[exeLen-3] != 'b' || 
		exedir[exeLen-2] != 'i' || 
		exedir[exeLen-1] != 'n' )
	{
		Q_strncat( exedir, "\\bin", sizeof( exedir ), COPY_ALL_CHARACTERS );
		Q_FixSlashes( exedir );
	}

	return exedir;
}


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
const char *UTIL_GetBaseDir( void )
{
	static char	basedir[ MAX_PATH ];

	char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
	if ( pOverrideDir )
		return pOverrideDir;

	basedir[ 0 ] = 0;
	const char *pExeDir = UTIL_GetExecutableDir( );
	if ( pExeDir )
	{
		strcpy( basedir, pExeDir );
                int dirlen = strlen( basedir );
                if ( basedir[ dirlen - 3 ] == 'b' &&
                     basedir[ dirlen - 2 ] == 'i' &&
                     basedir[ dirlen - 1 ] == 'n' )
                {
                        basedir[ dirlen - 4 ] = 0;
                }
	}

	return basedir;
}
