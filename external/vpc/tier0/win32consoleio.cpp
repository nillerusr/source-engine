//======= Copyright © 1996-2006, Valve Corporation, All rights reserved. ======
//
// Purpose: Win32 Console API helpers
//
//=============================================================================

#include "pch_tier0.h"
#include "win32consoleio.h"

#if defined( _WIN32 )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>

#endif // defined( _WIN32 )

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


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
// 3. If things get to here call AllocConsole which will pop open
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

		*stdout = *_fdopen( _open_osfhandle( reinterpret_cast< intp >( GetStdHandle( STD_OUTPUT_HANDLE ) ), _O_TEXT ), "w" );
		setvbuf( stdout, NULL, _IONBF, 0 );

		*stdin = *_fdopen( _open_osfhandle( reinterpret_cast< intp >( GetStdHandle( STD_INPUT_HANDLE ) ), _O_TEXT ), "r" );
		setvbuf( stdin, NULL, _IONBF, 0 );

		*stderr = *_fdopen( _open_osfhandle( reinterpret_cast< intp >( GetStdHandle( STD_ERROR_HANDLE ) ), _O_TEXT ), "w" );
		setvbuf( stdout, NULL, _IONBF, 0 );

		std::ios_base::sync_with_stdio();
	}

	return newConsole;

#else // defined( _WIN32 )

	return false;

#endif // defined( _WIN32 )
}

//-----------------------------------------------------------------------------
// Win32 Console Color API Helpers, originally from cmdlib.
// Retrieves the current console color attributes.
//-----------------------------------------------------------------------------
void InitWin32ConsoleColorContext( Win32ConsoleColorContext_t *pContext )
{
#if PLATFORM_WINDOWS_PC
	// Get the old background attributes.
	CONSOLE_SCREEN_BUFFER_INFO oldInfo;
	GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &oldInfo );
	pContext->m_InitialColor = pContext->m_LastColor = oldInfo.wAttributes & (FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
	pContext->m_BackgroundFlags = oldInfo.wAttributes & (BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY);

	pContext->m_BadColor = 0;
	if (pContext->m_BackgroundFlags & BACKGROUND_RED)
		pContext->m_BadColor |= FOREGROUND_RED;
	if (pContext->m_BackgroundFlags & BACKGROUND_GREEN)
		pContext->m_BadColor |= FOREGROUND_GREEN;
	if (pContext->m_BackgroundFlags & BACKGROUND_BLUE)
		pContext->m_BadColor |= FOREGROUND_BLUE;
	if (pContext->m_BackgroundFlags & BACKGROUND_INTENSITY)
		pContext->m_BadColor |= FOREGROUND_INTENSITY;
#else
	pContext->m_InitialColor = 0;
#endif
}

//-----------------------------------------------------------------------------
// Sets the active console foreground color. This function is smart enough to 
// avoid setting the color to something that would be unreadable given
// the user's potentially customized background color. It leaves the 
// background color unchanged.
// Returns: The console's previous foreground color.
//-----------------------------------------------------------------------------
uint16 SetWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, int nRed, int nGreen, int nBlue, int nIntensity )
{
#if PLATFORM_WINDOWS_PC
	uint16 ret = pContext->m_LastColor;
	pContext->m_LastColor = 0;
	if ( nRed )	pContext->m_LastColor |= FOREGROUND_RED;
	if ( nGreen ) pContext->m_LastColor |= FOREGROUND_GREEN;
	if ( nBlue )  pContext->m_LastColor |= FOREGROUND_BLUE;
	if ( nIntensity ) pContext->m_LastColor |= FOREGROUND_INTENSITY;

	// Just use the initial color if there's a match...
	if ( pContext->m_LastColor == pContext->m_BadColor )
		pContext->m_LastColor = pContext->m_InitialColor;

	SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), pContext->m_LastColor | pContext->m_BackgroundFlags );
	return ret;
#else	
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// Restore's the active foreground console color, without distributing the current
// background color.
//-----------------------------------------------------------------------------
void RestoreWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, uint16 prevColor )
{
#if PLATFORM_WINDOWS_PC
	SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), prevColor | pContext->m_BackgroundFlags );
	pContext->m_LastColor = prevColor;
#endif
}
