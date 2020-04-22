//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  a very simple wrapper to spawn another process based upon the contents of runme.dat
//
//=============================================================================
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// runme.dat contains the command line to run (passed directly to createprocess() )
	FILE *f = fopen( "runme.dat", "rb" );
	if ( !f )
		return -1;

	char szCommand[MAX_PATH];
	memset( szCommand, 0x0, sizeof(szCommand) );
	fread( szCommand, sizeof(szCommand), 1, f );
	fclose( f );
	int iCh = 0;
	while ( iCh < sizeof(szCommand) && szCommand[ iCh ] && szCommand[ iCh ] != '\r' && szCommand[ iCh ] != '\n' )
	{
		iCh++;
	}
	szCommand[ iCh ] = ' ';
	szCommand[ iCh + 1 ] = 0;
	strncpy( &szCommand[ iCh + 1 ], lpCmdLine, sizeof(szCommand) - iCh - 1 );
	szCommand[ sizeof(szCommand) - 1 ] = 0;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	memset(&si, 0x0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0x0, sizeof(pi));

	// run the command
	if(!CreateProcess(NULL, szCommand, 0, 0, FALSE, NORMAL_PRIORITY_CLASS, 0, 0,&si, &pi))
	{
		return -1;
	}

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

	return 0;
}


