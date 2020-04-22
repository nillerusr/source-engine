//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// CTextConsoleWin32.cpp: Win32 implementation of the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#include "TextConsoleWin32.h"
#include "tier0/dbg.h"
#include "utlvector.h"

// Could possibly switch all this code over to using readline. This:
//   http://mingweditline.sourceforge.net/?Description
// readline() / add_history(char *)

#ifdef _WIN32

BOOL WINAPI ConsoleHandlerRoutine( DWORD CtrlType )
{
	NOTE_UNUSED( CtrlType );
	/* TODO ?
	if ( CtrlType != CTRL_C_EVENT && CtrlType != CTRL_BREAK_EVENT )
		m_System->Stop();	 // don't quit on break or ctrl+c
	*/

	return TRUE;
}

// GetConsoleHwnd() helper function from MSDN Knowledge Base Article Q124103
// needed, because HWND GetConsoleWindow(VOID) is not avaliable under Win95/98/ME

HWND GetConsoleHwnd(void)
{
	typedef HWND (WINAPI *PFNGETCONSOLEWINDOW)( VOID );
	static PFNGETCONSOLEWINDOW s_pfnGetConsoleWindow = (PFNGETCONSOLEWINDOW) GetProcAddress( GetModuleHandle("kernel32"), "GetConsoleWindow" );
	if ( s_pfnGetConsoleWindow )
		return s_pfnGetConsoleWindow();

	HWND hwndFound;         // This is what is returned to the caller.
	char pszNewWindowTitle[1024]; // Contains fabricated WindowTitle
	char pszOldWindowTitle[1024]; // Contains original WindowTitle

	// Fetch current window title.
	GetConsoleTitle( pszOldWindowTitle, 1024 );

	// Format a "unique" NewWindowTitle.
	wsprintf( pszNewWindowTitle,"%d/%d", GetTickCount(), GetCurrentProcessId() );

	// Change current window title.
	SetConsoleTitle(pszNewWindowTitle);

	// Ensure window title has been updated.
	Sleep(40);

	// Look for NewWindowTitle.
	hwndFound = FindWindow( NULL, pszNewWindowTitle );

	// Restore original window title.

	SetConsoleTitle( pszOldWindowTitle );

	return hwndFound;
} 

CTextConsoleWin32::CTextConsoleWin32()
{
	hinput	= NULL;
	houtput = NULL;
	Attrib = 0;
	statusline[0] = '\0';
}

bool CTextConsoleWin32::Init()
{
	(void) AllocConsole();
	SetTitle( "SOURCE DEDICATED SERVER" );

	hinput = GetStdHandle ( STD_INPUT_HANDLE );
	houtput = GetStdHandle ( STD_OUTPUT_HANDLE );
	
	if ( !SetConsoleCtrlHandler( &ConsoleHandlerRoutine, TRUE) )
	{
		Print( "WARNING! TextConsole::Init: Could not attach console hook.\n" );
	}

	Attrib = FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY ;

	SetWindowPos( GetConsoleHwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW );

	memset( m_szConsoleText, 0, sizeof( m_szConsoleText ) );
	m_nConsoleTextLen	= 0;
	m_nCursorPosition	= 0;

	memset( m_szSavedConsoleText, 0, sizeof( m_szSavedConsoleText ) );
	m_nSavedConsoleTextLen = 0;

	memset( m_aszLineBuffer, 0, sizeof( m_aszLineBuffer ) );
	m_nTotalLines	= 0;
	m_nInputLine	= 0;
	m_nBrowseLine	= 0;

	// these are log messages, not related to console
	Msg( "\n" );
	Msg( "Console initialized.\n" );

	return CTextConsole::Init();
}

void CTextConsoleWin32::ShutDown( void )
{
	FreeConsole();
}

void CTextConsoleWin32::SetVisible( bool visible )
{
	ShowWindow ( GetConsoleHwnd(), visible ? SW_SHOW : SW_HIDE );
	m_ConsoleVisible = visible;
}

char * CTextConsoleWin32::GetLine( int index, char *buf, int buflen )
{
	while ( 1 )
	{
		INPUT_RECORD	recs[ 1024 ];
		unsigned long	numread;
		unsigned long	numevents;

		if ( !GetNumberOfConsoleInputEvents( hinput, &numevents ) )
		{
			Error("CTextConsoleWin32::GetLine: !GetNumberOfConsoleInputEvents");
			return NULL;
		}

		if ( numevents <= 0 )
			break;

		if ( !ReadConsoleInput( hinput, recs, ARRAYSIZE( recs ), &numread ) )
		{
			Error("CTextConsoleWin32::GetLine: !ReadConsoleInput");
			return NULL;
		}

		if ( numread == 0 )
			return NULL;

		for ( int i=0; i < (int)numread; i++ )
		{
			INPUT_RECORD *pRec = &recs[i];
			if ( pRec->EventType != KEY_EVENT )
				continue;

			if ( pRec->Event.KeyEvent.bKeyDown )
			{
				// check for cursor keys
				if ( pRec->Event.KeyEvent.wVirtualKeyCode == VK_UP )
				{
					ReceiveUpArrow();
				}
				else if ( pRec->Event.KeyEvent.wVirtualKeyCode == VK_DOWN )
				{
					ReceiveDownArrow();
				}
				else if ( pRec->Event.KeyEvent.wVirtualKeyCode == VK_LEFT )
				{
					ReceiveLeftArrow();
				}
				else if ( pRec->Event.KeyEvent.wVirtualKeyCode == VK_RIGHT )
				{
					ReceiveRightArrow();
				}
				else
				{
					char	ch;
					int		nLen;

					ch = pRec->Event.KeyEvent.uChar.AsciiChar;
					switch ( ch )
					{
					case '\r':	// Enter
						nLen = ReceiveNewline();
						if ( nLen )
						{
							strncpy( buf, m_szConsoleText, buflen );
							buf[ buflen - 1 ] = 0;
							return buf;
						}
						break;

					case '\b':	// Backspace
						ReceiveBackspace();
						break;

					case '\t':	// TAB
						ReceiveTab();
						break;

					default:
						if ( ( ch >= ' ') && ( ch <= '~' ) )	// dont' accept nonprintable chars
						{
							ReceiveStandardChar( ch );
						}
						break;
					}
				}
			}
		}
	}

	return NULL;
}

void CTextConsoleWin32::Print( char * pszMsg )
{
	if ( m_nConsoleTextLen )
	{
		int nLen;

		nLen = m_nConsoleTextLen;

		while ( nLen-- )
		{
			PrintRaw( "\b \b" );
		}
	}

	PrintRaw( pszMsg );

	if ( m_nConsoleTextLen )
	{
		PrintRaw( m_szConsoleText, m_nConsoleTextLen );
	}

	UpdateStatus();
}

void CTextConsoleWin32::PrintRaw( const char * pszMsg, int nChars )
{
	unsigned long dummy;

	if ( houtput == NULL )
	{
		houtput = GetStdHandle ( STD_OUTPUT_HANDLE );
		if ( houtput == NULL )
			return;
	}
	
	if ( nChars <= 0 )
	{
		nChars = strlen( pszMsg );
		if ( nChars <= 0 )
			return;
	}

	// filter out ASCII BEL characters because windows actually plays a
	// bell sound, which can be used to lag the server in a DOS attack.
	char * pTempBuf = NULL;
	for ( int i = 0; i < nChars; ++i )
	{
		if ( pszMsg[i] == 0x07 /*BEL*/ )
		{
			if ( !pTempBuf )
			{
				pTempBuf = ( char * ) malloc( nChars );
				memcpy( pTempBuf, pszMsg, nChars );
			}
			pTempBuf[i] = '.';
		}
	}
	
	WriteFile( houtput, pTempBuf ? pTempBuf : pszMsg, nChars, &dummy, NULL );

	free( pTempBuf ); // usually NULL
}

int CTextConsoleWin32::GetWidth( void )
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	int							nWidth;

	nWidth = 0;

	if ( GetConsoleScreenBufferInfo( houtput, &csbi ) )
	{
		nWidth = csbi.dwSize.X;
	}

	if ( nWidth <= 1 )
		nWidth = 80;

	return nWidth;
} 

void CTextConsoleWin32::SetStatusLine( char * pszStatus )
{
	strncpy( statusline, pszStatus, 80 );
	statusline[ 79 ] = '\0';
	UpdateStatus();
}

void CTextConsoleWin32::UpdateStatus( void )
{
	COORD	coord;
	DWORD	dwWritten = 0;
	WORD	wAttrib[ 80 ];
	
	for ( int i = 0; i < 80; i++ )
	{
		wAttrib[i] = Attrib; //FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY ;
	}

	coord.X = coord.Y = 0;

	WriteConsoleOutputAttribute( houtput, wAttrib, 80, coord, &dwWritten );
	WriteConsoleOutputCharacter( houtput, statusline, 80, coord, &dwWritten );	
}


void CTextConsoleWin32::SetTitle( char * pszTitle )
{
	SetConsoleTitle( pszTitle );
}

void CTextConsoleWin32::SetColor(WORD attrib) 
{
	Attrib = attrib;
}	

int CTextConsoleWin32::ReceiveNewline( void )
{
	int nLen = 0;

	PrintRaw( "\n" );

	if ( m_nConsoleTextLen )
	{
		nLen = m_nConsoleTextLen;

		m_szConsoleText[ m_nConsoleTextLen ] = 0;
		m_nConsoleTextLen = 0;
		m_nCursorPosition = 0;

		// cache line in buffer, but only if it's not a duplicate of the previous line
		if ( ( m_nInputLine == 0 ) || ( strcmp( m_aszLineBuffer[ m_nInputLine - 1 ], m_szConsoleText ) ) )
		{
			strncpy( m_aszLineBuffer[ m_nInputLine ], m_szConsoleText, MAX_CONSOLE_TEXTLEN );
			
			m_nInputLine++;

			if ( m_nInputLine > m_nTotalLines )
				m_nTotalLines = m_nInputLine;

			if ( m_nInputLine >= MAX_BUFFER_LINES )
				m_nInputLine = 0;

		}

		m_nBrowseLine = m_nInputLine;
	}

	return nLen;
}


void CTextConsoleWin32::ReceiveBackspace( void )
{
	int nCount;

	if ( m_nCursorPosition == 0 )
	{
		return;
	}

	m_nConsoleTextLen--;
	m_nCursorPosition--;

	PrintRaw( "\b" );

	for ( nCount = m_nCursorPosition; nCount < m_nConsoleTextLen; nCount++ )
	{
		m_szConsoleText[ nCount ] = m_szConsoleText[ nCount + 1 ];
		PrintRaw( m_szConsoleText + nCount, 1 );
	}

	PrintRaw( " " );

	nCount = m_nConsoleTextLen;
	while ( nCount >= m_nCursorPosition )
	{
		PrintRaw( "\b" );
		nCount--;
	}

	m_nBrowseLine = m_nInputLine;
}


void CTextConsoleWin32::ReceiveTab( void )
{
	CUtlVector<char *> matches;

	m_szConsoleText[ m_nConsoleTextLen ] = 0;

	if ( matches.Count() == 0 )
	{
		return;
	}

	if ( matches.Count() == 1 )
	{
		char * pszCmdName;
		char * pszRest;

		pszCmdName	= matches[0];
		pszRest		= pszCmdName + strlen( m_szConsoleText );

		if ( pszRest )
		{
			PrintRaw( pszRest );
			strcat( m_szConsoleText, pszRest );
			m_nConsoleTextLen += strlen( pszRest );

			PrintRaw( " " );
			strcat( m_szConsoleText, " " );
			m_nConsoleTextLen++;
		}
	}
	else
	{
		int		nLongestCmd;
		int		nTotalColumns;
		int		nCurrentColumn;
		char *	pszCurrentCmd;
		int i = 0;

		nLongestCmd = 0;

		pszCurrentCmd = matches[0];
		while ( pszCurrentCmd )
		{
			if ( (int)strlen( pszCurrentCmd) > nLongestCmd )
			{
				nLongestCmd = strlen( pszCurrentCmd);
			}

			i++;
			pszCurrentCmd = (char *)matches[i];
		}

		nTotalColumns	= ( GetWidth() - 1 ) / ( nLongestCmd + 1 );
		nCurrentColumn	= 0;

		PrintRaw( "\n" );

		// Would be nice if these were sorted, but not that big a deal
		pszCurrentCmd = matches[0];
		i = 0;
		while ( pszCurrentCmd )
		{
			char szFormatCmd[ 256 ];

			nCurrentColumn++;

			if ( nCurrentColumn > nTotalColumns )
			{
				PrintRaw( "\n" );
				nCurrentColumn = 1;
			}

			Q_snprintf( szFormatCmd, sizeof(szFormatCmd), "%-*s ", nLongestCmd, pszCurrentCmd );
			PrintRaw( szFormatCmd );

			i++;
			pszCurrentCmd = matches[i];
		}

		PrintRaw( "\n" );
		PrintRaw( m_szConsoleText );
		// TODO: Tack on 'common' chars in all the matches, i.e. if I typed 'con' and all the
		//       matches begin with 'connect_' then print the matches but also complete the
		//       command up to that point at least.
	}

	m_nCursorPosition	= m_nConsoleTextLen;
	m_nBrowseLine		= m_nInputLine;
}



void CTextConsoleWin32::ReceiveStandardChar( const char ch )
{
	int nCount;

	// If the line buffer is maxed out, ignore this char
	if ( m_nConsoleTextLen >= ( sizeof( m_szConsoleText ) - 2 ) )
	{
		return;
	}

	nCount = m_nConsoleTextLen;
	while ( nCount > m_nCursorPosition )
	{
		m_szConsoleText[ nCount ] = m_szConsoleText[ nCount - 1 ];
		nCount--;
	}

	m_szConsoleText[ m_nCursorPosition ] = ch;

	PrintRaw( m_szConsoleText + m_nCursorPosition, m_nConsoleTextLen - m_nCursorPosition + 1 );

	m_nConsoleTextLen++;
	m_nCursorPosition++;

	nCount = m_nConsoleTextLen;
	while ( nCount > m_nCursorPosition )
	{
		PrintRaw( "\b" );
		nCount--;
	}

	m_nBrowseLine = m_nInputLine;
}


void CTextConsoleWin32::ReceiveUpArrow( void )
{
	int nLastCommandInHistory;

	nLastCommandInHistory = m_nInputLine + 1;
	if ( nLastCommandInHistory > m_nTotalLines )
	{
		nLastCommandInHistory = 0;
	}

	if ( m_nBrowseLine == nLastCommandInHistory )
	{
		return;
	}

	if ( m_nBrowseLine == m_nInputLine )
	{
		if ( m_nConsoleTextLen > 0 )
		{
			// Save off current text
			strncpy( m_szSavedConsoleText, m_szConsoleText, m_nConsoleTextLen );
			// No terminator, it's a raw buffer we always know the length of
		}

		m_nSavedConsoleTextLen = m_nConsoleTextLen;
	}

	m_nBrowseLine--;
	if ( m_nBrowseLine < 0 )
	{
		m_nBrowseLine = m_nTotalLines - 1;
	}

	while ( m_nConsoleTextLen-- )	// delete old line
	{
		PrintRaw( "\b \b" );
	}

	// copy buffered line
	PrintRaw( m_aszLineBuffer[ m_nBrowseLine ] );

	strncpy( m_szConsoleText, m_aszLineBuffer[ m_nBrowseLine ], MAX_CONSOLE_TEXTLEN );
	m_nConsoleTextLen = strlen( m_aszLineBuffer[ m_nBrowseLine ] );

	m_nCursorPosition = m_nConsoleTextLen;
}


void CTextConsoleWin32::ReceiveDownArrow( void )
{
	if ( m_nBrowseLine == m_nInputLine )
	{
		return;
	}

	m_nBrowseLine++;
	if ( m_nBrowseLine > m_nTotalLines )
	{
		m_nBrowseLine = 0;
	}

	while ( m_nConsoleTextLen-- )	// delete old line
	{
		PrintRaw( "\b \b" );
	}

	if ( m_nBrowseLine == m_nInputLine )
	{
		if ( m_nSavedConsoleTextLen > 0 )
		{
			// Restore current text
			strncpy( m_szConsoleText, m_szSavedConsoleText, m_nSavedConsoleTextLen );
			// No terminator, it's a raw buffer we always know the length of

			PrintRaw( m_szConsoleText, m_nSavedConsoleTextLen );
		}

		m_nConsoleTextLen = m_nSavedConsoleTextLen;
	}
	else
	{
		// copy buffered line
		PrintRaw( m_aszLineBuffer[ m_nBrowseLine ] );

		strncpy( m_szConsoleText, m_aszLineBuffer[ m_nBrowseLine ], MAX_CONSOLE_TEXTLEN );

		m_nConsoleTextLen = strlen( m_aszLineBuffer[ m_nBrowseLine ] );
	}

	m_nCursorPosition = m_nConsoleTextLen;
}


void CTextConsoleWin32::ReceiveLeftArrow( void )
{
	if ( m_nCursorPosition == 0 )
	{
		return;
	}

	PrintRaw( "\b" );
	m_nCursorPosition--;
}


void CTextConsoleWin32::ReceiveRightArrow( void )
{
	if ( m_nCursorPosition == m_nConsoleTextLen )
	{
		return;
	}

	PrintRaw( m_szConsoleText + m_nCursorPosition, 1 );
	m_nCursorPosition++;
}

#endif // _WIN32
