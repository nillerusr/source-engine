//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.
//
//=====================================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#endif
#if defined( _X360 )
#define _XBOX
#include <xtl.h>
#include <xbdm.h>
#include <stdio.h>
#include <assert.h>
#include "xbox\xbox_core.h"
#include "xbox\xbox_launch.h"
#endif
#ifdef POSIX
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <limits.h>
#include <string.h>
#define MAX_PATH PATH_MAX
#endif

#include "tier0/basetypes.h"

#ifdef WIN32
typedef int (*LauncherMain_t)( HINSTANCE hInstance, HINSTANCE hPrevInstance, 
							  LPSTR lpCmdLine, int nCmdShow );
#elif POSIX
typedef int (*LauncherMain_t)( int argc, char **argv );
#else
#error
#endif

#ifdef WIN32
// hinting the nvidia driver to use the dedicated graphics card in an optimus configuration
// for more info, see: http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" { _declspec( dllexport ) DWORD NvOptimusEnablement = 0x00000001; }

// same thing for AMD GPUs using v13.35 or newer drivers
extern "C" { __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1; }

#endif


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
#if !defined( _X360 )

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	size_t j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

#ifdef WIN32

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");

	// Use the .EXE name to determine the root directory
	char moduleName[ MAX_PATH ];
	char szBuffer[4096];
	if ( !GetModuleFileName( hInstance, moduleName, MAX_PATH ) )
	{
		MessageBox( 0, "Failed calling GetModuleFileName", "Launcher Error", MB_OK );
		return 0;
	}

	// Get the root directory the .exe is in
	char* pRootDir = GetBaseDir( moduleName );

#ifdef _DEBUG
	int len = 
#endif
	_snprintf( szBuffer, sizeof( szBuffer ), "PATH=%s\\bin\\;%s", pRootDir, pPath );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';
	assert( len < sizeof( szBuffer ) );
	_putenv( szBuffer );

	// Assemble the full path to our "launcher.dll"
	_snprintf( szBuffer, sizeof( szBuffer ), "%s\\bin\\launcher.dll", pRootDir );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';

	// STEAM OK ... filesystem not mounted yet
#if defined(_X360)
	HINSTANCE launcher = LoadLibrary( szBuffer );
#else
	HINSTANCE launcher = LoadLibraryEx( szBuffer, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
#endif
	if ( !launcher )
	{
		char *pszError;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszError, 0, NULL);

		char szBuf[1024];
		_snprintf(szBuf, sizeof( szBuf ), "Failed to load the launcher DLL:\n\n%s", pszError);
		szBuf[sizeof( szBuf ) - 1] = '\0';
		MessageBox( 0, szBuf, "Launcher Error", MB_OK );

		LocalFree(pszError);
		return 0;
	}

	LauncherMain_t main = (LauncherMain_t)GetProcAddress( launcher, "LauncherMain" );
	return main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
}

#elif defined (POSIX)

#if defined( LINUX )

#include <fcntl.h>

static bool IsDebuggerPresent( int time )
{
	// Need to get around the __wrap_open() stuff. Just find the open symbol
	// directly and use it...
	typedef int (open_func_t)( const char *pathname, int flags, mode_t mode );
	open_func_t *open_func = (open_func_t *)dlsym( RTLD_NEXT, "open" );

	if ( open_func )
	{
		for ( int i = 0; i < time; i++ )
		{
			int tracerpid = -1;

			int fd = (*open_func)( "/proc/self/status", O_RDONLY, S_IRUSR );
			if (fd >= 0)
			{
				char buf[ 4096 ];
				static const char tracerpid_str[] = "TracerPid:";

				const int len = read( fd, buf, sizeof(buf) - 1 );
				if ( len > 0 )
				{
					buf[ len ] = 0;

					const char *str = strstr( buf, tracerpid_str );
					tracerpid = str ? atoi( str + sizeof( tracerpid_str ) ) : -1;
				}

				close( fd );
			}

			if ( tracerpid > 0 )
				return true;

			sleep( 1 );
		}
	}

	return false;
}

static void WaitForDebuggerConnect( int argc, char *argv[], int time )
{
	for ( int i = 1; i < argc; i++ )
	{
		if ( strstr( argv[i], "-wait_for_debugger" ) )
		{
			printf( "\nArg -wait_for_debugger found.\nWaiting %dsec for debugger...\n", time );
			printf( "  pid = %d\n", getpid() );

			if ( IsDebuggerPresent( time ) )
				printf("Debugger connected...\n\n");

			break;
		}
	}
}

#else

static void WaitForDebuggerConnect( int argc, char *argv[], int time )
{
}

#endif // !LINUX

int main( int argc, char *argv[] )
{
	void *launcher = dlopen( "bin/launcher" DLL_EXT_STRING, RTLD_NOW );
	if ( !launcher )
	{
		fprintf( stderr, "Failed to load the launcher\n" );
		return 0;
	}
	
	LauncherMain_t main = (LauncherMain_t)dlsym( launcher, "LauncherMain" );
	if ( !main )
	{
		fprintf( stderr, "Failed to load the launcher entry proc\n" );
		return 0;
	}

#if defined(__clang__) && !defined(OSX)
	// When building with clang we absolutely need the allocator to always
	// give us 16-byte aligned memory because if any objects are tagged as
	// being 16-byte aligned then clang will generate SSE instructions to move
	// and initialize them, and an allocator that does not respect the
	// contract will lead to crashes. On Linux we normally use the default
	// allocator which does not give us this alignment guarantee.
	// The google tcmalloc allocator gives us this guarantee.
	// Test the current allocator to make sure it gives us the required alignment.
	void* pointers[20];
	for (int i = 0; i < ARRAYSIZE(pointers); ++i)
	{
		void* p = malloc( 16 );
		pointers[ i ] = p;
		if ( ( (size_t)p ) & 0xF )
		{
			printf( "%p is not 16-byte aligned. Aborting.\n", p );
			printf( "Pass /define:CLANG to VPC to correct this.\n" );
			return -10;
		}
	}
	for (int i = 0; i < ARRAYSIZE(pointers); ++i)
	{
		if ( pointers[ i ] )
			free( pointers[ i ] );
	}

	if ( __has_feature(address_sanitizer) )
	{
		printf( "Address sanitizer is enabled.\n" );
	}
	else
	{
		printf( "No address sanitizer!\n" );
	}
#endif

	WaitForDebuggerConnect( argc, argv, 30 );

	return main( argc, argv );
}

#else
#error
#endif // WIN32 || POSIX


#else // X360
//-----------------------------------------------------------------------------
// 360 Quick and dirty command line parsing. Returns true if key found,
// false otherwise. Caller can optionally get next argument.
//-----------------------------------------------------------------------------
bool ParseCommandLineArg( const char *pCmdLine, const char* pKey, char* pValueBuff = NULL, int valueBuffSize = 0 )
{
	int keyLen = (int)strlen( pKey );
	const char* pArg = pCmdLine;
	for ( ;; )
	{
		// scan for match
		pArg = strstr( (char*)pArg, pKey );
		if ( !pArg )
		{
			return false;
		}
		
		// found, but could be a substring
		if ( pArg[keyLen] == '\0' || pArg[keyLen] == ' ' )
		{
			// exact match
			break;
		}

		pArg += keyLen;
	}

	if ( pValueBuff )
	{
		// caller wants next token
		// skip past key and whitespace
		pArg += keyLen;
		while ( *pArg == ' ' )
		{
			pArg++;
		}

		int i;
		for ( i=0; i<valueBuffSize; i++ )
		{
			pValueBuff[i] = *pArg;
			if ( *pArg == '\0' || *pArg == ' ' )
				break;
			pArg++;
		}
		pValueBuff[i] = '\0';
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// 360 Quick and dirty command line arg stripping.
//-----------------------------------------------------------------------------
void StripCommandLineArg( const char *pCmdLine, char *pNewCmdLine, const char *pStripArg )
{
	// cannot operate in place
	assert( pCmdLine != pNewCmdLine );

	int numTotal = strlen( pCmdLine ) + 1;
	const char* pArg = strstr( pCmdLine, pStripArg );
	if ( !pArg )
	{
		strcpy( pNewCmdLine, pCmdLine );
		return;
	}

	int numDiscard = strlen( pStripArg );
	while ( pArg[numDiscard] && ( pArg[numDiscard] != '-' && pArg[numDiscard] != '+' ) )
	{
		// eat whitespace up to the next argument
		numDiscard++;
	}

	memcpy( pNewCmdLine, pCmdLine, pArg - pCmdLine );
	memcpy( pNewCmdLine + ( pArg - pCmdLine ), (void*)&pArg[numDiscard], numTotal - ( pArg + numDiscard - pCmdLine  ) );

	// ensure we don't leave any trailing whitespace, occurs if last arg is stripped
	int len = strlen( pNewCmdLine );
	while ( len > 0 &&  pNewCmdLine[len-1] == ' ' )
	{
		len--;
	}
	pNewCmdLine[len] = '\0';
}

//-----------------------------------------------------------------------------
// 360 Conditional spew
//-----------------------------------------------------------------------------
void Spew( const char *pFormat, ... )
{
#if defined( _DEBUG )
	char	msg[2048];
	va_list	argptr;

	va_start( argptr, pFormat );
	vsprintf( msg, pFormat, argptr );
	va_end( argptr );

	OutputDebugString( msg );
#endif
}

//-----------------------------------------------------------------------------
// Adheres to possible xbox exclude paths in for -dvddev mode.
//-----------------------------------------------------------------------------
bool IsBinExcluded( const char *pRemotePath, bool *pExcludeAll )
{
	*pExcludeAll = false;
	bool bIsBinExcluded = false;

	// find optional exclusion file
	char szExcludeFile[MAX_PATH];
	sprintf( szExcludeFile, "%s\\xbox_exclude_paths.txt", pRemotePath );
	HANDLE hFile = CreateFile( szExcludeFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		int len = GetFileSize( hFile, NULL );
		if ( len > 0 )
		{
			char *pBuffer = ( char *)malloc( len + 1 );
			memset( pBuffer, 0, len+1 );
			DWORD numBytesRead;
			if ( ReadFile( hFile, pBuffer, len, &numBytesRead, NULL ) )
			{
				strlwr( pBuffer );
				if ( strstr( pBuffer, "\"*\"" ) )
				{
					*pExcludeAll = true;
					bIsBinExcluded = true;
				}
				else if ( strstr( pBuffer, "\"\\bin\"" ) )
				{
					// override file either specifies an exclusion of the root or the bin directory
					bIsBinExcluded = true;
				}
			}
			free( pBuffer );
		}
		CloseHandle( hFile );
	}

	return bIsBinExcluded;
}

//-----------------------------------------------------------------------------
// Get the new entry point and command line
//-----------------------------------------------------------------------------
LauncherMain_t GetLaunchEntryPoint( char *pNewCommandLine )
{
	HMODULE		hModule;
	char		*pCmdLine;

	// determine source of our invocation, internal or external
	// a valid launch payload will have an embedded command line
	// command line could be from internal restart in dev or retail mode
	CXboxLaunch xboxLaunch;
	int payloadSize;
	unsigned int launchID;
	char *pPayload;
	bool bInternalRestart = xboxLaunch.GetLaunchData( &launchID, (void**)&pPayload, &payloadSize );
	if ( !bInternalRestart || !payloadSize || launchID != VALVE_LAUNCH_ID )
	{
		// launch is not ours
		if ( launchID == LAUNCH_DATA_DEMO_ID )
		{
			// data is a demo blob, not ready to handle yet, so ignore
		}

		// could be first time, get command line from system
		pCmdLine = GetCommandLine();
		if ( !stricmp( pCmdLine, "\"default.xex\"" ) )
		{
			// matches retail xex and no arguments, mut be first time retail launch
			pCmdLine = "default.xex -dvd";
#if defined( _MEMTEST )
			pCmdLine = "default.xex -dvd +mat_picmip 2";
#endif
		}
	}
	else
	{
		// get embedded command line from payload
		pCmdLine = pPayload;
	}

	int launchFlags = xboxLaunch.GetLaunchFlags();
#if !defined( _CERT )
	if ( launchFlags & LF_ISDEBUGGING )
	{
		while ( !DmIsDebuggerPresent() )
		{
		}

		Sleep( 1000 );
		Spew( "Resuming debug session.\n" );
	}
#endif

	if ( launchID == VALVE_LAUNCH_ID )
	{
		// unforunately, the xbox erases its internal store upon first fetch
		// must re-establish it so the payload that contains other data (past command line) can be accessed by the game
		// the launch data will be owned by tier0 and supplied to game
		xboxLaunch.SetLaunchData( pPayload, payloadSize, launchFlags );
	}

	// The 360 has no paths and therefore the xex must reside in the same location as the dlls.
	// Only the xex must reside locally, on the box, but the dlls can be mounted from the remote share.
	// Resolve all known implicitly loaded dlls to be explicitly loaded now to allow their remote location.
	const char *pImplicitDLLs[] =
	{
		"tier0_360.dll",
		"vstdlib_360.dll",
		"vxbdm_360.dll",

		// last slot reserved, as dynamic, used to determine which application gets launched
		"???"
	};

	// Corresponds to pImplicitDLLs. A dll load failure is only an error if that dll is tagged as required.
	const bool bDllRequired[] = 
	{
		true,	// tier0
		true,	// vstdlib
		false,	// vxbdm
		true,	// ???
	};

	char gameName[32];
	bool bDoChooser = false;
	if ( !ParseCommandLineArg( pCmdLine, "-game", gameName, sizeof( gameName ) ) )
	{
		// usage of remote share requires a game (default to hl2) 
		// remote share mandates an absolute game path which is detected by the host
		strcpy( gameName, "hl2" );
		bDoChooser = true;
	}
	else
	{
		// sanitize a possible absolute game path back to expected game name
		char *pSlash = strrchr( gameName, '\\' );
		if ( pSlash )
		{
			memcpy( gameName, pSlash+1, strlen( pSlash+1 )+1 );
		}
	}

	char shareName[32];
	if ( !ParseCommandLineArg( pCmdLine, "-share", shareName, sizeof( shareName ) ) )
	{
		// usage of remote share requires a share name for the game folder (default to game) 
		strcpy( shareName, "game" );
	}

	if ( ( xboxLaunch.GetLaunchFlags() & LF_EXITFROMGAME ) && !( xboxLaunch.GetLaunchFlags() & LF_GAMERESTART ) )
	{
		// exiting from a game back to chooser
		bDoChooser = true;
	}

	// If we're restarting from an invite, we're funneling into TF2
	if ( launchFlags & LF_INVITERESTART )
	{
		strcpy( gameName, "tf" );
		bDoChooser = false;
	}

	// resolve which application gets launched
	if ( bDoChooser )
	{
		// goto high level 1 of N game selector
		pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1] = "AppChooser_360.dll"; 
	}
	else
	{
		pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1] = "launcher_360.dll";
	}

	// the base path is the where the game is predominantly anchored
	char basePath[128];
	// a remote path is for development mode only, on the host pc
	char remotePath[128];

#if !defined( _CERT )
	if ( !ParseCommandLineArg( pCmdLine, "-dvd" ) )
	{
		// development mode only, using host pc
		// auto host name detection can be overriden via command line
		char hostName[32];
		if ( !ParseCommandLineArg( pCmdLine, "-host", hostName, sizeof( hostName ) ) )
		{
			// auto detect, the 360 machine name must be <HostPC>_360
			DWORD length = sizeof( hostName );
			HRESULT hr = DmGetXboxName( hostName, &length );
			if ( hr != XBDM_NOERR )
			{
				Spew( "FATAL: Could not get xbox name: %s\n", hostName );
				return NULL;
			}
			char *p = strstr( hostName, "_360" );
			if ( !p )
			{
				Spew( "FATAL: Xbox name must be <HostPC>_360\n" );
				return NULL;
			}
			*p = '\0';
		}

		sprintf( remotePath, "net:\\smb\\%s\\%s", hostName, shareName );

		// network remote shares seem to be buggy, but always manifest as the gamedir being unaccessible
		// validate now, otherwise longer wait until process eventually fails
		char szFullPath[MAX_PATH];
		WIN32_FIND_DATA findData;
		sprintf( szFullPath, "%s\\%s\\*.*", remotePath, gameName );
		HANDLE hFindFile = FindFirstFile( szFullPath, &findData );
		if ( hFindFile == INVALID_HANDLE_VALUE )
		{
			Spew( "*******************************************************************\n" );
			Spew( "FATAL: Access to remote share '%s' on host PC lost. Forcing cold reboot.\n", szFullPath );
			Spew( "FATAL: After reboot completes to dashboard, restart application.\n" );
			Spew( "*******************************************************************\n" );
			DmRebootEx( DMBOOT_COLD, NULL, NULL, NULL );
			return NULL;
		}
		FindClose( hFindFile );
	}
#endif

	char *searchPaths[2];
	searchPaths[0] = basePath;
	int numSearchPaths = 1;

	bool bExcludeAll = false;
	bool bAddRemotePath = false;

	if ( ParseCommandLineArg( pCmdLine, "-dvd" ) )
	{
		// game runs from dvd only 
		strcpy( basePath, "d:" );
	}
	else if ( ParseCommandLineArg( pCmdLine, "-dvddev" ) )
	{
		// dvd development, game runs from dvd and can fall through to access remote path
		// check user configuration for possible \bin exclusion from Xbox HDD
		strcpy( basePath, "d:" );
		
		if ( IsBinExcluded( remotePath, &bExcludeAll ) )
		{
			searchPaths[0] = remotePath;
			numSearchPaths = 1;
		}
		else
		{
			searchPaths[0] = basePath;
			searchPaths[1] = remotePath;
			numSearchPaths = 2;
		}

		if ( bExcludeAll )
		{
			// override, user has excluded everything, game runs from remote path only
			strcpy( basePath, remotePath );
		}
		else
		{
			// -dvddev appends a -remote <remotepath> for the filesystem to detect
			bAddRemotePath = true;
		}
	}
	else
	{
		// game runs from remote path only
		strcpy( basePath, remotePath );
	}

	// load all the dlls specified
	char dllPath[MAX_PATH];
	for ( int i=0; i<ARRAYSIZE( pImplicitDLLs ); i++ )
	{
		hModule = NULL;
		for ( int j = 0; j < numSearchPaths; j++ )
		{
			sprintf( dllPath, "%s\\bin\\%s", searchPaths[j], pImplicitDLLs[i] );
			hModule = LoadLibrary( dllPath );
			if ( hModule )
			{
				break;
			}
		}
		if ( !hModule && bDllRequired[i] )
		{
			Spew( "FATAL: Failed to load dll: '%s'\n", dllPath );
			return NULL;
		}
	}

	char cleanCommandLine[512];
	char tempCommandLine[512];
	StripCommandLineArg( pCmdLine, tempCommandLine, "-basedir" );
	StripCommandLineArg( tempCommandLine, cleanCommandLine, "-game" );

	if ( bAddRemotePath )
	{
		// the remote path is only added for -dvddev mode
		StripCommandLineArg( cleanCommandLine, tempCommandLine, "-remote" );
		sprintf( cleanCommandLine, "%s -remote %s", tempCommandLine, remotePath );
	}

	// set the alternate command line
	sprintf( pNewCommandLine, "%s -basedir %s -game %s\\%s", cleanCommandLine, basePath, basePath, gameName );

	// the 'main' export is guaranteed to be at ordinal 1
	// the library is already loaded, this just causes a lookup that will resolve against the shortname
	const char *pLaunchDllName = pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1];
	hModule = LoadLibrary( pLaunchDllName );
	LauncherMain_t main = (LauncherMain_t)GetProcAddress( hModule, (LPSTR)1 );
	if ( !main )
	{
		Spew( "FATAL: 'LauncherMain' entry point not found in %s\n", pLaunchDllName );
		return NULL;
	}

	return main;
}

//-----------------------------------------------------------------------------
// 360 Application Entry Point.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
	char newCmdLine[512];
	LauncherMain_t newMain = GetLaunchEntryPoint( newCmdLine );
	if ( newMain )
	{
		// 360 has no concept of instances, spoof one 
		newMain( (HINSTANCE)1, (HINSTANCE)0, (LPSTR)newCmdLine, 0 );
	}
}
#endif
