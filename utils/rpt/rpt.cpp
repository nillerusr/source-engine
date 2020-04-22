//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================


// NOTE: We specifically don't want to include *any* valve libraries
// because this is expected to be run on a customer's machine and we
// don't want to have to deal with any strange path stuff
#include <windows.h>
#include <stdio.h>


//-----------------------------------------------------------------------------
// Print help
//-----------------------------------------------------------------------------
void PrintHelp( )
{
	printf( "Usage: rpt -i <in .csv file> -m <.mdl relative path> -o <output dir>\n" );
	printf( "				[-f <fac output dir>] [-p] [-w] [-r <sample rate in hz>] [-s <sample filter size>]\n" );
	printf( "				[-nop4] [-vproject <path to gameinfo.txt>]\n" );
	printf( "\t-h,-help,-?\t:Displays this help message.\n" );
	printf( "\t-m\t: Indicates the path of the .mdl file under game/mod/models/ to use in the sfm files.\n" );
	printf( "\t-o\t: Indicates output directory to place generated files in.\n" );
	printf( "\t-f\t: [Optional] Indicates that facial files should be created in the specified fac output dir.\n" );
	printf( "\t-p\t: [Optional] Indicates the extracted phonemes should be written into the wav file.\n" );
	printf( "\t-w\t: [Optional] Indicates the phonemes should be read from the wav file. Cannot also have -p specified.\n" );
	printf( "\t-r\t: [Optional] Specifies the phoneme extraction sample rate (default = 20)\n" );
	printf( "\t-s\t: [Optional] Specifies the phoneme extraction sample filter size (default = 0.08)\n" );
	printf( "\t-nop4\t: Disables auto perforce checkout/add.\n" );
	printf( "\t-vproject\t: Specifies path to a gameinfo.txt file (which mod to build for).\n" );
}


//-----------------------------------------------------------------------------
// Used to parse the commandline
//-----------------------------------------------------------------------------
static int FindParm( const char *pParam, int argc, char **argv )
{
	// Start at 1 so as to not search the exe name
	for ( int i = 1; i < argc; ++i )
	{
		if ( !strcmp( pParam, argv[i] ) )
			return i;
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Strips the last file from a filename, returns int offset of string end
//-----------------------------------------------------------------------------
static size_t StripFileName( char *pFileName )
{
	// In this case, we queried the key properly. Strip off the steamclient.dll name
	// to make a good full path to steam.exe
	char *pSlash = strrchr( pFileName, '\\' );
	char *pSlash2 = strrchr( pFileName, '/' );
	if ( pSlash2 > pSlash )
	{
		pSlash = pSlash2;
	}
	if ( !pSlash )
	{
		pSlash = pFileName;
	}
	else
	{
		++pSlash;
	}
	*pSlash = 0;
	size_t nLen = (size_t)pSlash - (size_t)pFileName;
	return nLen;
}


//-----------------------------------------------------------------------------
// Used to detect where steam is running from
//-----------------------------------------------------------------------------
static DWORD GetActiveSteamPID( )
{
	HKEY hKey;
	LONG hResult = RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", &hKey );
	if ( hResult != ERROR_SUCCESS )
		return 0;

	DWORD nType;
	char pTempBuf[256];
	DWORD nLen = sizeof(pTempBuf);
	hResult = RegQueryValueEx( hKey, "pid", NULL, &nType, (unsigned char *)pTempBuf, &nLen );
	RegCloseKey( hKey );
	if ( hResult != ERROR_SUCCESS || nLen == 0 )
		return 0;

	switch( nType )
	{
	case REG_DWORD:
		return *(DWORD*)pTempBuf;

	case REG_QWORD:
		return (DWORD)( *(__int64*)pTempBuf );

	case REG_SZ:
	case REG_EXPAND_SZ:
		return atoi( pTempBuf );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Used to detect where steam is running from
//-----------------------------------------------------------------------------
static bool DetectSteamPath( char *pFileName, int nBufLen )
{
	*pFileName = 0;

	DWORD nLen;

	// First check to see if there's an active process
	HKEY hKey;
	LONG hResult = RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", &hKey );
	if ( hResult == ERROR_SUCCESS )
	{
		nLen = (DWORD)nBufLen;
		hResult = RegQueryValueEx( hKey, "SteamClientDll", NULL, NULL, (LPBYTE)pFileName, &nLen );
		RegCloseKey( hKey );
		if ( hResult == ERROR_SUCCESS )
		{
			// In this case, we queried the key properly. Strip off the steamclient.dll name
			// to make a good full path to steam.exe
			size_t nUsed = StripFileName( pFileName );
			strncpy( &pFileName[nUsed], "steam.exe", nBufLen - nUsed );
			return true;
		}
	}

	// If not, then get the default install location
	hResult = RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Steam", &hKey );
	if ( hResult != ERROR_SUCCESS )
		return false;

	nLen = (DWORD)nBufLen;
	hResult = RegQueryValueEx( hKey, "SteamExe", NULL, NULL, (LPBYTE)pFileName, &nLen );
	RegCloseKey( hKey );
	return ( hResult == ERROR_SUCCESS && nLen != 0 );
}


//-----------------------------------------------------------------------------
// Displays a windows error
//-----------------------------------------------------------------------------
static void DisplayWindowsError( const char *pMessage ) 
{ 
	DWORD dw = GetLastError(); 
	char pErrorMessage[2048];
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pErrorMessage, sizeof(pErrorMessage), NULL );
	printf( "%s\n%s\n", pMessage, pErrorMessage );
}


//-----------------------------------------------------------------------------
// Deletes steam.cfg
//-----------------------------------------------------------------------------
static void DeleteCfgFile( const char *pFullPath ) 
{ 
	remove( pFullPath );
}


//-----------------------------------------------------------------------------
// Launches steam.exe, returns process ID (0  if failed)
//-----------------------------------------------------------------------------
static DWORD LaunchSteam( const char *pFullPath, const char *pArgs )
{
	// Try to launch steam, shutting down any existing instance.
	STARTUPINFO si;
	memset( &si, 0, sizeof si );
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	if ( !CreateProcess( pFullPath, (char*)pArgs, NULL, NULL, TRUE, DETACHED_PROCESS, NULL, NULL, &si, &pi ) )
		return 0;

	return pi.dwProcessId;
}


//-----------------------------------------------------------------------------
// Is process active?
//-----------------------------------------------------------------------------
static bool IsSteamProcessActive( DWORD nProcessID )
{
	if ( nProcessID == 0 )
		return false;

	HANDLE hProcess = OpenProcess( PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, nProcessID );
	bool bProcessActive = ( hProcess != 0 );
	if ( bProcessActive )
	{
		DWORD nExitCode;
		BOOL bOk = GetExitCodeProcess( hProcess, &nExitCode );
		if ( !bOk || ( nExitCode != STILL_ACTIVE ) )
		{
			bProcessActive = false;
		}
	}

	CloseHandle( hProcess );
	return bProcessActive;
}


static void TerminateSteamProcess( DWORD nProcessID )
{
	if ( nProcessID == 0 )
		return;

	HANDLE hProcess = OpenProcess( PROCESS_TERMINATE, FALSE, nProcessID );
	if ( hProcess != 0 )
	{
		TerminateProcess( hProcess, 0 );
		CloseHandle( hProcess );
	}
}


//-----------------------------------------------------------------------------
// Shuts down existing steam process
//-----------------------------------------------------------------------------
static bool ShutdownExistingSteamProcess( const char *pSteamPath )
{
	DWORD nSteamPID = GetActiveSteamPID( );

	// Ensure the process is actually running. 
	// The registry simply stores the last run steam process id
	if ( !IsSteamProcessActive( nSteamPID ) )
	{
		nSteamPID = 0;
	}

	// Try to shut down existing instances of steam
	if ( nSteamPID == 0 )
		return true;

	if ( !LaunchSteam( pSteamPath, "-shutdown" ) )
	{
		DisplayWindowsError( "rpt: Unable to shutdown existing steam process!" );
		return false;
	}

	// Wait for the old process to terminate
	// Wait for at least 30 seconds. If it doesn't start by that time,
	// then kill the existing process.
	DWORD nStartTime = GetTickCount();
	DWORD nTestTime = 30;
	while ( IsSteamProcessActive( nSteamPID ) )
	{
		Sleep( 1000 );
		DWORD dt = ( GetTickCount() - nStartTime ) / 1000;
		if ( dt <= nTestTime )
			continue;

		TerminateSteamProcess( nSteamPID );
		break;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Copies test files into position
//-----------------------------------------------------------------------------
bool CopyFilesRecursively( const char *pSrcPath, const char *pDestPath )
{
	// Create a file that makes steam not delete different DLLs
	char pSearchStr[MAX_PATH];
	_snprintf( pSearchStr, sizeof(pSearchStr), "%s*", pSrcPath );

	WIN32_FIND_DATA find;
	HANDLE hFind = FindFirstFile( pSearchStr, &find );
	if( INVALID_HANDLE_VALUE == hFind )
		return true;

	// Make sure the dest path exists
	if ( ( GetFileAttributes( pDestPath ) & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
	{
		if ( !CreateDirectory( pDestPath, NULL ) )
		{
			printf( "Unable to create directory %s\n", pDestPath );
			return false;
		}
	}

	// Display each file and ask for the next.
	do
	{
		// Skip . and ..
		if ( !strcmp( find.cFileName, "." ) || !strcmp( find.cFileName, ".." ) )
			continue;

		char pSrcChildPath[MAX_PATH];
		char pDestChildPath[MAX_PATH];

		// Deal with directories
		if ( ( find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && ( ( find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) == 0 ) )
		{
			_snprintf( pSrcChildPath, sizeof(pSrcChildPath), "%s%s\\", pSrcPath, find.cFileName );
			_snprintf( pDestChildPath, sizeof(pDestChildPath), "%s%s\\", pDestPath, find.cFileName );
			if ( !CopyFilesRecursively( pSrcChildPath, pDestChildPath ) )
				return false;
			continue;
		}

		_snprintf( pSrcChildPath, sizeof(pSrcChildPath), "%s%s", pSrcPath, find.cFileName );
		_snprintf( pDestChildPath, sizeof(pDestChildPath), "%s%s", pDestPath, find.cFileName );

		// Copy everything else
		// NOTE: This may fail if the file isn't already there
		SetFileAttributes( pDestChildPath, FILE_ATTRIBUTE_NORMAL );

		if ( CopyFile( pSrcChildPath, pDestChildPath, FALSE ) == 0 )
		{
			printf( "Unable to copy file %s to %s\n", pSrcChildPath, pDestChildPath );
			return false;
		}

		if ( !SetFileAttributes( pDestChildPath, FILE_ATTRIBUTE_READONLY ) )
		{
			printf( "Unable to make file %s read only.\n", pDestChildPath );
			return false;
		}

	} while( FindNextFile( hFind, &find ) );

	// Close the find handle.
	FindClose( hFind );
	return true;
}


//-----------------------------------------------------------------------------
// Copies test files into position
//-----------------------------------------------------------------------------
static bool CopyFilesIntoPosition( const char *pSteamPath )
{
	// Create a file that makes steam not delete different DLLs
	char pSteamRoot[MAX_PATH];
	strncpy( pSteamRoot, pSteamPath, sizeof(pSteamRoot) );
	StripFileName( pSteamRoot );

	char pSrcRoot[MAX_PATH];
	if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), pSrcRoot, sizeof(pSrcRoot) ) )
	{
		printf( "Unable to find rpt launch path!\n" );
		return false;
	}
	StripFileName( pSrcRoot );

	return CopyFilesRecursively( pSrcRoot, pSteamRoot );
}


//-----------------------------------------------------------------------------
// Launches new steam process
//-----------------------------------------------------------------------------
static bool LaunchNewSteamProcess( const char *pSteamPath )
{
	// Create a file that makes steam not delete different DLLs
	char pCfgPath[MAX_PATH];
	strncpy( pCfgPath, pSteamPath, sizeof(pCfgPath) );
	size_t nUsed = StripFileName( pCfgPath );
	strncpy( &pCfgPath[nUsed], "steam.cfg", sizeof(pCfgPath) - nUsed );

	FILE* fp = fopen( pCfgPath, "wt" );
	if ( !fp )
	{
		printf( "rpt: Unable to relaunch steam! Aborting...\n" );
		return false;
	}

	fprintf( fp, "MinFootprintAutoRefresh = disable\n" );
	fclose( fp );

	// Launch a new instance of steam
	DWORD nNewPID = LaunchSteam( pSteamPath, "" );

	// Wait for the new process to start.
	// Wait for at least 30 seconds. If it doesn't start by that time,
	// stop what we're doing.
	DWORD nStartTime = GetTickCount();
	DWORD nTestTime = 30;
	while ( GetActiveSteamPID( ) != nNewPID )
	{
		Sleep( 1000 );
		DWORD dt = ( GetTickCount() - nStartTime ) / 1000;
		if ( dt <= nTestTime )
			continue;

		TerminateSteamProcess( nNewPID );
		DeleteCfgFile( pCfgPath );
		printf( "rpt: Unable to launch new steam process!" );
		return false;
	}

	DeleteCfgFile( pCfgPath );

	return true;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int main( int argc, char **argv )
{
	if ( FindParm( "-h", argc, argv ) || FindParm( "-help", argc, argv ) || FindParm( "-?", argc, argv ) )
	{
		PrintHelp();
		return 0;
	}

	// Detect the currently running version of steam
	char pSteamPath[MAX_PATH];
	if ( !DetectSteamPath( pSteamPath, sizeof(pSteamPath) ) )
	{
		printf( "rpt: Unable to detect steam installation path! Aborting...\n" );
		return 0;
	}

	if ( !ShutdownExistingSteamProcess( pSteamPath ) )
		return 0;

	if ( !CopyFilesIntoPosition( pSteamPath ) )
		return 0;

	if ( !LaunchNewSteamProcess( pSteamPath ) )
		return 0;

	return -1;
}