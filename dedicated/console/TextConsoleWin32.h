//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TextConsoleWin32.h: Win32 interface for the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined TEXTCONSOLE_WIN32_H
#define TEXTCONSOLE_WIN32_H
#pragma once

#ifdef _WIN32

#include <windows.h>
#include "TextConsole.h"

#define MAX_CONSOLE_TEXTLEN 256
#define MAX_BUFFER_LINES	30

class CTextConsoleWin32 : public CTextConsole
{
public:
	CTextConsoleWin32();
	virtual ~CTextConsoleWin32() { };

	// CTextConsole
	bool		Init();
	void		ShutDown( void );
	void		Print( char *pszMsg );

	void		SetTitle( char * pszTitle );
	void		SetStatusLine( char * pszStatus );
	void		UpdateStatus( void );

	char *		GetLine( int index, char *buf, int buflen );
	int			GetWidth( void );

	void		SetVisible( bool visible );

protected:
	// CTextConsoleWin32
	void		SetColor( WORD );
	void		PrintRaw( const char * pszMsg, int nChars = -1 );

private:
	char	m_szConsoleText[ MAX_CONSOLE_TEXTLEN ];						// console text buffer
	int		m_nConsoleTextLen;											// console textbuffer length
	int		m_nCursorPosition;											// position in the current input line

	// Saved input data when scrolling back through command history
	char	m_szSavedConsoleText[ MAX_CONSOLE_TEXTLEN ];				// console text buffer
	int		m_nSavedConsoleTextLen;										// console textbuffer length

	char	m_aszLineBuffer[ MAX_BUFFER_LINES ][ MAX_CONSOLE_TEXTLEN ];	// command buffer last MAX_BUFFER_LINES commands
	int		m_nInputLine;												// Current line being entered
	int		m_nBrowseLine;												// current buffer line for up/down arrow
	int		m_nTotalLines;												// # of nonempty lines in the buffer

	int		ReceiveNewline( void );
	void	ReceiveBackspace( void );
	void	ReceiveTab( void );
	void	ReceiveStandardChar( const char ch );
	void	ReceiveUpArrow( void );
	void	ReceiveDownArrow( void );
	void	ReceiveLeftArrow( void );
	void	ReceiveRightArrow( void );

private:
	HANDLE	hinput;		// standard input handle
	HANDLE	houtput;	// standard output handle
	WORD	Attrib;		// attrib colours for status bar
	
	char	statusline[81];			// first line in console is status line
};

#endif // _WIN32

#endif // !defined TEXTCONSOLE_WIN32_H
