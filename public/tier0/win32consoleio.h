//======= Copyright © 1996-2006, Valve Corporation, All rights reserved. ======
//
// Purpose: Win32 Console API helpers
//
//=============================================================================
#ifndef WIN32_CONSOLE_IO_H
#define WIN32_CONSOLE_IO_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

// Function to attach a console for I/O to a Win32 GUI application in a reasonably smart fashion.
PLATFORM_INTERFACE bool SetupWin32ConsoleIO();

// Win32 Console Color API Helpers, originally from cmdlib.

struct Win32ConsoleColorContext_t
{
	int  m_InitialColor;
	uint16 m_LastColor;
	uint16 m_BadColor;
	uint16 m_BackgroundFlags;
};

PLATFORM_INTERFACE void InitWin32ConsoleColorContext( Win32ConsoleColorContext_t *pContext );

PLATFORM_INTERFACE uint16 SetWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, int nRed, int nGreen, int nBlue, int nIntensity );

PLATFORM_INTERFACE void RestoreWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, uint16 prevColor );

#endif 
