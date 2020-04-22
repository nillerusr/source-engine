//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// WaitAndRestart.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "tier1/strtools.h"
#include "vmpi_defs.h"


void PrintLog( const char *pMsg, ... )
{
#ifdef VMPI_SERVICE_LOGS
	char str[4096];
	va_list marker;
	
	va_start( marker, pMsg );
	_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );
	
	printf( "%s", str );
	
	static FILE *fp = fopen( "c:\\vmpi_WaitAndRestart.log", "wt" );
	if ( fp )
	{
		fprintf( fp, "%s", str );
		fflush( fp );
	}
#endif
}


char* GetLastErrorString()
{
	static char err[2048];
	
	LPVOID lpMsgBuf;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
	strncpy( err, (char*)lpMsgBuf, sizeof( err ) );
	LocalFree( lpMsgBuf );

	err[ sizeof( err ) - 1 ] = 0;
	return err;
}


int main( int argc, char* argv[] )
{
Sleep(5000);
	if ( argc < 4 )
	{
		PrintLog( "WaitAndRestart <seconds to wait> <working directory> command line...\n" );
		return 1;
	}

	PrintLog( "WaitAndRestart <seconds to wait> <working directory> command line...\n" );


	const char *pTimeToWait = argv[1];
	const char *pWorkingDir = argv[2];
	
	// If a * precedes the time-to-wait arg, then it's a process ID and we wait for that process to exit.
	if ( pTimeToWait[0] == '*' )
	{
		++pTimeToWait;
		DWORD dwProcessId;
		sscanf( pTimeToWait, "%lu", &dwProcessId );
		
		PrintLog( "Waiting for process %lu to exit. Press a key to cancel...\n", dwProcessId );
		
		HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | SYNCHRONIZE, false, dwProcessId );
		if ( hProcess )
		{
			while ( 1 )
			{
				DWORD val = WaitForSingleObject( hProcess, 100 );
				if ( val == WAIT_OBJECT_0 )
				{
					break;
				}
				else if ( val == WAIT_ABANDONED )
				{
					PrintLog( "Got WAIT_ABANDONED (error). Waiting 5 seconds, then continuing.\n" );
					Sleep( 5000 );
					break;
				}

				if ( kbhit() )
					return 2;
			}
			PrintLog( "Process %lu terminated. Continuing.\n", dwProcessId );
		}
		else
		{
			PrintLog( "Process %lu not running. Continuing.\n", dwProcessId );
		}
		
		CloseHandle( hProcess );
	}
	else
	{
		DWORD timeToWait = (DWORD)atoi( argv[1] );
		
		PrintLog( "\n\nWaiting for %d seconds to launch ' ", timeToWait );
		PrintLog( "%s> ", pWorkingDir );
		for ( int i=3; i < argc; i++ )
		{
			PrintLog( "%s ", argv[i] );
		}
		PrintLog( "'\n\nPress a key to cancel... " );

		DWORD startTime = GetTickCount();
		while ( GetTickCount() - startTime < (timeToWait*1000) )
		{
			if ( kbhit() )
				return 2;
			
			Sleep( 100 );
		}
	}

	// Ok, launch it!
	char commandLine[1024] = {0};
	for ( int i=3; i < argc; i++ )
	{
		Q_strncat( commandLine, "\"", sizeof( commandLine ), COPY_ALL_CHARACTERS );
		Q_strncat( commandLine, argv[i], sizeof( commandLine ), COPY_ALL_CHARACTERS );
		Q_strncat( commandLine, "\" ", sizeof( commandLine ), COPY_ALL_CHARACTERS );
	}
	
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	if ( CreateProcess( 
		NULL, 
		commandLine, 
		NULL,						// security
		NULL,
		FALSE,
		0,							// flags
		NULL,						// environment
		pWorkingDir,						// current directory
		&si,
		&pi ) )
	{
		PrintLog( "Process started.\n" );
		CloseHandle( pi.hThread );	// We don't care what the process does.
		CloseHandle( pi.hProcess );
	}
	else
	{
		PrintLog( "CreateProcess error!\n%s", GetLastErrorString() );
	}

	return 0;
}

