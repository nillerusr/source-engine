//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Attaches a console for I/O to a Win32 GUI application in a
//          reasonably smart fashion
//
//=============================================================================

#include "pch_tier0.h"

#if defined( _WIN32 )

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>

#endif // defined( _WIN32 )

//-----------------------------------------------------------------------------
//
// Attach a console to a Win32 GUI process and setup stdin, stdout & stderr
// along with the std::iostream (cout, cin, cerr) equivalents to read and
// write to and from that console
// 
// 1. Ensure the handle associated with stdio is FILE_TYPE_UNKNOWN
//    if it's anything else just return false.  This supports cygwin
//    style command shells like rxvt which setup pipes to processes
//    they spawn
//
// 2. See if the Win32 function call AttachConsole exists in kernel32
//    It's a Windows 2000 and above call.  If it does, call it and see
//    if it succeeds in attaching to the console of the parent process.
//    If that succeeds, return false (for no new console allocated).
//    This supports someone typing the command from a normal windows
//    command window and having the output go to the parent window.
//    It's a little funny because a GUI app detaches so the command
//    prompt gets intermingled with output from this process
//    
// 3. If thigns get to here call AllocConsole which will pop open
//    a new window and allow output to go to that window.  The
//    window will disappear when the process exists so if it's used
//    for something like a help message then do something like getchar()
//    from stdin to wait for a keypress.  if AllocConsole is called
//    true is returned.
//
// Return: true if AllocConsole() was used to pop open a new windows console
// 
//-----------------------------------------------------------------------------


bool SetupWin32ConsoleIO()
{
#if defined( _WIN32 )
	// Only useful on Windows platforms

	bool newConsole( false );

	if ( GetFileType( GetStdHandle( STD_OUTPUT_HANDLE ) ) == FILE_TYPE_UNKNOWN )
	{

		HINSTANCE hInst = ::LoadLibrary( "kernel32.dll" );
		typedef BOOL ( WINAPI * pAttachConsole_t )( DWORD );
		pAttachConsole_t pAttachConsole( ( BOOL ( _stdcall * )( DWORD ) )GetProcAddress( hInst, "AttachConsole" ) );

		if ( !( pAttachConsole && (*pAttachConsole)( ( DWORD ) - 1 ) ) )
		{
			newConsole = true;
			AllocConsole();
		}

		*stdout = *_fdopen( _open_osfhandle( reinterpret_cast< intptr_t >( GetStdHandle( STD_OUTPUT_HANDLE ) ), _O_TEXT ), "w" );
		setvbuf( stdout, NULL, _IONBF, 0 );

		*stdin = *_fdopen( _open_osfhandle( reinterpret_cast< intptr_t >( GetStdHandle( STD_INPUT_HANDLE ) ), _O_TEXT ), "r" );
		setvbuf( stdin, NULL, _IONBF, 0 );

		*stderr = *_fdopen( _open_osfhandle( reinterpret_cast< intptr_t >( GetStdHandle( STD_ERROR_HANDLE ) ), _O_TEXT ), "w" );
		setvbuf( stdout, NULL, _IONBF, 0 );

		std::ios_base::sync_with_stdio();
	}

	return newConsole;

#else // defined( _WIN32 )

	return false;

#endif // defined( _WIN32 )
}