//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Stuff which interacts with system libraries
//
//=============================================================================


// Windows includes
#include <windows.h>
#include <direct.h>


// Valve includes
#include "itemtest/itemtest.h"
#include "tier1/fmtstr.h"


// Last include
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

HANDLE g_hInputFile = NULL;

#define BUFSIZE 4096

 
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::CreateDirectory( const char *pszDirectory )
{
	char szBuf[ BUFSIZE ];
	V_strncpy( szBuf, pszDirectory, 1024 );

	for ( int i = 0; i < V_strlen( szBuf ); ++i )
	{
		if ( szBuf[i] == '/' || szBuf[i] == '\\' )
		{
			szBuf[i] = '\0';
			mkdir( szBuf );
			szBuf[i] = CORRECT_PATH_SEPARATOR;
		}
	}

	mkdir( szBuf );

	return true;
}


//-----------------------------------------------------------------------------
// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT. 
// Stop when there is no more data. 
//-----------------------------------------------------------------------------
static void ReadFromPipe( CItemLog *pLog )
{
	DWORD dwRead;
	char chBuf[BUFSIZE + 1];
	BOOL bSuccess = FALSE;

	// Close the write end of the pipe before reading from the 
	// read end of the pipe, to control child process execution.
	// The pipe is assumed to have enough buffer space to hold the
	// data the child process has already written to it.

	if ( !CloseHandle( g_hChildStd_OUT_Wr ) )
		return;

	// TODO: Prefix each line, print in color?

	for (;;) 
	{
		// This can hang if the process is waiting for input, for example...
		bSuccess = ReadFile( g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL );

		if ( !bSuccess || dwRead == 0 )
			break;

		chBuf[dwRead] = '\0';

		pLog->Warning( chBuf );
	}
}
 

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::RunCommandLine( const char *pszCmdLine, const char *pszWorkingDir, CItemLog *pLog )
{ 
	bool bOk = false;

	Msg( "Launching: %s\n", pszCmdLine );
	Msg( "Directory: %s\n", pszWorkingDir );

	if ( pLog )
	{
		SECURITY_ATTRIBUTES saAttr; 

		// Set the bInheritHandle flag so pipe handles are inherited. 
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		saAttr.bInheritHandle = TRUE; 
		saAttr.lpSecurityDescriptor = NULL; 

		// Create a pipe for the child process's STDOUT. 
		if ( !CreatePipe( &g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0 ) ) 
			return false;

		// Ensure the read handle to the pipe for STDOUT is not inherited.
		if ( !SetHandleInformation( g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0 ) )
			return false;
	}

	// Create the child process. 
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;

	// Set up members of the PROCESS_INFORMATION structure. 
	V_memset( &piProcInfo, 0, sizeof( PROCESS_INFORMATION ) );

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	V_memset( &siStartInfo, 0, sizeof( STARTUPINFO ) );
	siStartInfo.cb = sizeof( STARTUPINFO ); 

	if ( pLog )
	{
		siStartInfo.hStdError = g_hChildStd_OUT_Wr;
		siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
		siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	}

	// Create the child process. 
	const BOOL bSuccess = CreateProcess(NULL, 
		const_cast< char * >( pszCmdLine ),     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		CREATE_NO_WINDOW,	// creation flags
		NULL,          // use parent's environment 
		pszWorkingDir, // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo ); // receives PROCESS_INFORMATION 

	// If an error occurs, return
	if ( !bSuccess ) 
		return false;

	if ( pLog )
	{
		ReadFromPipe( pLog ); 

		WaitForSingleObject( piProcInfo.hProcess, INFINITE );

		bOk = false;

		DWORD nExitCode = 0;
		if ( GetExitCodeProcess( piProcInfo.hProcess, &nExitCode ) )
		{
			bOk = ( nExitCode == 0 );
		}

		CloseHandle( piProcInfo.hProcess );
		CloseHandle( piProcInfo.hThread );
	}
	else
	{
		bOk = true;
	}

	return bOk;
}


//-----------------------------------------------------------------------------
// Determine if 2 paths point ot the same file...
// Note: This only works if the file exists
//-----------------------------------------------------------------------------
bool CItemUpload::IsSameFile( const char *szPath1, const char *szPath2 )
{
	if ( !szPath1 || !szPath2 )
		return false;

	HANDLE handle1 = ::CreateFile( szPath1, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ); 
	HANDLE handle2 = ::CreateFile( szPath2, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ); 

	bool bResult = false;

	if ( handle1 != NULL && handle2 != NULL )
	{
		BY_HANDLE_FILE_INFORMATION fileInfo1;
		BY_HANDLE_FILE_INFORMATION fileInfo2;
		if ( ::GetFileInformationByHandle( handle1, &fileInfo1 ) && ::GetFileInformationByHandle( handle2, &fileInfo2 ) )
		{
			bResult = fileInfo1.dwVolumeSerialNumber == fileInfo2.dwVolumeSerialNumber &&
				fileInfo1.nFileIndexHigh == fileInfo2.nFileIndexHigh &&
				fileInfo1.nFileIndexLow == fileInfo2.nFileIndexLow;
		}
	}

	if ( handle1 != NULL )
	{
		::CloseHandle(handle1);
	}

	if ( handle2 != NULL )
	{
		::CloseHandle( handle2 );
	}

	return bResult;
}


//-----------------------------------------------------------------------------
// Gets the filename to the current executable
//-----------------------------------------------------------------------------
bool CItemUpload::GetCurrentExecutableFileName( CUtlString &sCurrentExecutableFileName )
{
	char szModuleFileName[MAX_PATH];
	szModuleFileName[0] = '\0';

	if ( !::GetModuleFileName( (HMODULE)NULL, szModuleFileName, ARRAYSIZE( szModuleFileName ) ) )
		return false;

	sCurrentExecutableFileName = szModuleFileName;

	return true;
}


//-----------------------------------------------------------------------------
// Get the install location of the app from the registry via
//
// HKEY_LOCAL_MACHINE :
//		"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App %d"
//
//-----------------------------------------------------------------------------
bool CItemUpload::GetSteamAppInstallLocation( CUtlString &sSteamAppInstallLocation, int nAppId )
{
	HKEY hKey;
	char szSteamAppInstallLocation[ 65536 ] = "";

	const CFmtStr sRegKey( "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App %d", nAppId );

	if ( ERROR_SUCCESS == RegOpenKey( HKEY_LOCAL_MACHINE, sRegKey.String(), &hKey ) )
	{
		DWORD dwSize = sizeof( szSteamAppInstallLocation );
		RegQueryValueEx( hKey, "InstallLocation", NULL, NULL, (LPBYTE)szSteamAppInstallLocation, &dwSize );
		RegCloseKey( hKey );

		sSteamAppInstallLocation = szSteamAppInstallLocation;

		return true;
	}

	return false;
}