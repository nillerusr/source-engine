//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	VXCONSOLE.CPP
//
//	Valve XBox Console.
//=====================================================================================//
#include "vxconsole.h"

HWND				g_hDlgMain;
HWND				g_hwndCommandCombo; 
HWND				g_hwndOutputWindow;
HWND				g_hwndCommandHint;
WNDPROC				g_hwndCommandSubclassed;
BOOL				g_connectedToXBox; 
BOOL				g_connectedToApp; 
PDMN_SESSION		g_pdmnSession;
PDM_CONNECTION		g_pdmConnection;
printQueue_t		g_PrintQueue;
UINT_PTR			g_autoConnectTimer;
BOOL				g_autoConnect;
BOOL				g_debugCommands;
BOOL				g_captureDebugSpew;
BOOL				g_captureGameSpew = TRUE;
CHAR				g_xboxName[MAX_XBOXNAMELEN];
DWORD				g_xboxAddress;
HINSTANCE			g_hInstance;
HICON				g_hIcons[MAX_ICONS];
HBRUSH				g_hBackgroundBrush;
HFONT				g_hFixedFont;
BOOL				g_reboot;
char*				g_rebootArgv[MAX_ARGVELEMS];
int					g_rebootArgc;
COLORREF			g_backgroundColor;
TEXTMETRIC			g_fixedFontMetrics;
int					g_connectCount;
int					g_currentIcon = -1;
HACCEL				g_hAccel;
HMODULE				g_hRichEdit;
DWORD				g_connectedTime;
RECT				g_mainWindowRect;
HFONT				g_hProportionalFont;
HANDLE				g_hCommandReadyEvent;
int					g_currentCommandSelection;
int					g_connectFailure;
int					g_configID;
bool                g_bSuppressBlink = false;
BOOL				g_bPlayTestMode = TRUE;

LRESULT CALLBACK Main_DlgProc( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK CommandWindow_SubclassedProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );

bool ParseCommandLineArg( const char *pCmdLine, const char *pKey, char *pValueBuff, int valueBuffSize )
{
	const char* pArg = V_stristr( (char*)pCmdLine, pKey );
	if ( !pArg )
	{
		return false;
	}

	if ( pValueBuff )
	{
		// caller wants next token
		pArg += strlen( pKey );

		int i;
		for ( i=0; i<valueBuffSize; i++ )
		{
			pValueBuff[i] = *pArg;
			if ( *pArg == '\0' || *pArg == ' ' )
			{
				break;
			}
			pArg++;
		}
		pValueBuff[i] = '\0';
	}
	
	return true;
}

void MakeConfigString( const char *pString, int configID, char *pOutBuff, int outBuffSize )
{
	if ( configID <= 0 )
	{
		// as-is, undecorated
		V_snprintf( pOutBuff, outBuffSize, "%s", pString );	
		return;
	}

	int len = strlen( pString );
	bool bAddTerminalSlash =  ( len > 1 && pString[len-1] == '\\' );

	V_snprintf( pOutBuff, outBuffSize, "%s_%d",	pString, configID );

	if ( bAddTerminalSlash )
	{
		V_strncat( pOutBuff, "\\", outBuffSize );
	}
}

//-----------------------------------------------------------------------------
//	LoadConfig
// 
//-----------------------------------------------------------------------------
void LoadConfig()
{
	char	buff[256];
	int		numArgs;

	ConfigDlg_LoadConfig();

	// initial menu state is from persisted config
	g_captureDebugSpew = g_captureDebugSpew_StartupState;

	Sys_GetRegistryString( "mainWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_mainWindowRect.left, &g_mainWindowRect.top, &g_mainWindowRect.right, &g_mainWindowRect.bottom );
	if ( numArgs != 4 || g_mainWindowRect.left < 0 || g_mainWindowRect.top < 0 || g_mainWindowRect.right < 0 || g_mainWindowRect.bottom < 0 )
		memset( &g_mainWindowRect, 0, sizeof( g_mainWindowRect ) );
}

//-----------------------------------------------------------------------------
//	SaveConfig
// 
//-----------------------------------------------------------------------------
void SaveConfig()
{
	char	buff[256];

	// get window placement
	WINDOWPLACEMENT wp;
	memset( &wp, 0, sizeof( wp ) );
	wp.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( g_hDlgMain, &wp );
	g_mainWindowRect = wp.rcNormalPosition;

	sprintf( buff, "%d %d %d %d", g_mainWindowRect.left, g_mainWindowRect.top, g_mainWindowRect.right, g_mainWindowRect.bottom );
	Sys_SetRegistryString( "mainWindowRect", buff );
}

//-----------------------------------------------------------------------------
//	SetConnectionIcon
//
//-----------------------------------------------------------------------------
void SetConnectionIcon( int icon )
{
	if ( g_currentIcon == icon )
		return;
	
	g_currentIcon = icon;
	SetClassLongPtr( g_hDlgMain, GCLP_HICON, ( LONG_PTR )g_hIcons[icon] );
}

//-----------------------------------------------------------------------------
//	SetMainWindowTitle
//
//-----------------------------------------------------------------------------
void SetMainWindowTitle()
{
	if ( !g_hDlgMain )
	{
		return;
	}

	char titleBuff[128];
	if ( !g_xboxTargetName[0] )
	{
		strcpy( titleBuff, VXCONSOLE_TITLE );
	}
	else
	{
		sprintf( titleBuff, "%s: %s", VXCONSOLE_TITLE, g_xboxTargetName );
		if ( g_configID )
		{
			char configBuff[32];
			sprintf( configBuff, " (%d)", g_configID );
			V_strncat( titleBuff, configBuff, sizeof( titleBuff ) );
		}
	}
	SetWindowText( g_hDlgMain, titleBuff );
}

//-----------------------------------------------------------------------------
// DmAPI_DisplayError
// 
//-----------------------------------------------------------------------------
VOID DmAPI_DisplayError( const CHAR* message, HRESULT hr )
{
    CHAR strError[128];

	ConsoleWindowPrintf( RGB( 255,0,0 ), "%s\n", message );

	HRESULT hrError = DmTranslateError( hr, strError, sizeof( strError ) );
	if ( !FAILED( hrError ) && strError[0] )
		ConsoleWindowPrintf( RGB( 255,0,0 ), "Reason: '%s'\n", strError );
	else
		ConsoleWindowPrintf( RGB( 255,0,0 ), "Reason: 0x%08lx\n", hr );
}

//-----------------------------------------------------------------------------
//	DmAPI_SendCommand
//
//	Send the specified string across the debugger channel to the application
//-----------------------------------------------------------------------------
HRESULT DmAPI_SendCommand( const char* strCommand, bool wait )
{
	DWORD	dwResponseLen;
	CHAR	strResponse[MAX_PATH];
	int		retval;
	bool	bIgnorePingResponse;
	char*	ptr;

	retval = WaitForSingleObject( g_hCommandReadyEvent, wait ? INFINITE : 0 );
	if ( retval != WAIT_OBJECT_0 )
	{
		// cannot send command 
		// some other previous command has not responded and signaled the release
		// testing has shown DmSendCommand() is not re-entrant and callers get
		// their responses out of sync
		return XBDM_UNDEFINED;
	}

	// clear the event mutex until ready
	ResetEvent( g_hCommandReadyEvent );

	bIgnorePingResponse = false;
	dwResponseLen  = sizeof( strResponse );
	strResponse[0] = '\0';

	if ( strCommand[0] == '*' )
	{
		// skip past internal command identifier
		strCommand++;
	}
	else if ( !stricmp( strCommand, VXCONSOLE_COMMAND_PREFIX "!" ) )
	{
		// empty ping command
		// must be done as a synchronous command with a response because there
		// is no way to bind an asynch response to the owner
		bIgnorePingResponse = true;
	}
	
	HRESULT hr = DmSendCommand( g_pdmConnection, strCommand, strResponse, &dwResponseLen );
	if ( !FAILED( hr ) )
	{
		// success
        switch ( hr )
        {
            case XBDM_NOERR:
                if ( !bIgnorePingResponse )
				{
					// skip past possible ack prefix
					ptr = strstr( strResponse, VXCONSOLE_COMMAND_ACK );
					if ( ptr )
					{
						ptr += strlen( VXCONSOLE_COMMAND_ACK );

						// ignore remote acknowledge response
						if ( !stricmp( ptr, "OK" ) )
							break;
					}
					else
					{
						ptr = strResponse;
					}
                    ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s\n", ptr );
				}
                break;

			case XBDM_MULTIRESPONSE:
				// multi-line response - loop, looking for end of response
                while ( 1 )
                {
                    dwResponseLen = sizeof( strResponse );

                    hr = DmReceiveSocketLine( g_pdmConnection, strResponse, &dwResponseLen );
                    if ( FAILED( hr ) || strResponse[0] == '.' )
                        break;
                    ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s\n", strResponse );
                }
                break;

            case XBDM_BINRESPONSE:
                ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Binary response - not implemented\n" );
                break;

            case XBDM_READYFORBIN:
                ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Ready for binary - not implemented\n" );
                break;

            default:
                ConsoleWindowPrintf( XBX_CLR_RED, "Unknown Response: ( %s ).\n", strResponse );
                break;
		}
	}

	SetEvent( g_hCommandReadyEvent );
	return hr;
}

//-----------------------------------------------------------------------------
// PrintToQueue
// 
// Formats the string and adds it to the print queue
//-----------------------------------------------------------------------------
void PrintToQueue( COLORREF rgb, LPCTSTR strFormat, ... )
{
	char buffer[MAX_QUEUEDSTRINGLEN];

    // enter critical section so we don't try to process the list
    EnterCriticalSection( &g_PrintQueue.CriticalSection );

    assert( g_PrintQueue.numMessages <= MAX_QUEUEDSTRINGS );

    // when the queue is full, the main thread is probably blocked and not dequeing
    if ( !g_captureGameSpew || g_PrintQueue.numMessages == MAX_QUEUEDSTRINGS )
    {
        LeaveCriticalSection( &g_PrintQueue.CriticalSection );
        return;
    }

    va_list arglist;
    va_start( arglist, strFormat );
    int len = _vsnprintf( buffer, MAX_QUEUEDSTRINGLEN, strFormat, arglist );
	if ( len == -1 )
	{
		buffer[sizeof(buffer)-1] = '\0';
	}
	va_end( arglist );
	
    // queue the message into the next slot
	g_PrintQueue.pMessages[g_PrintQueue.numMessages] = Sys_CopyString( buffer );
    g_PrintQueue.aColors[g_PrintQueue.numMessages++] = rgb;

    // ensure we post a message to process the print queue
    if ( g_PrintQueue.numMessages == 1 )
        PostMessage( g_hDlgMain, WM_USER, 0, 0 );

    // the main thread can now safely process the list
    LeaveCriticalSection( &g_PrintQueue.CriticalSection );
}

//-----------------------------------------------------------------------------
//	ProcessPrintQueue
//
//-----------------------------------------------------------------------------
void ProcessPrintQueue()
{
    // enter critical section so we don't try to add anything while we're processing
    EnterCriticalSection( &g_PrintQueue.CriticalSection );

	// dequeue all entries
    for ( int i = 0; i < g_PrintQueue.numMessages; i++ )
    {
        ConsoleWindowPrintf( g_PrintQueue.aColors[i], "%s", g_PrintQueue.pMessages[i] );
		Sys_Free( g_PrintQueue.pMessages[i] );
    }

    g_PrintQueue.numMessages = 0;

    // now we can safely add to the list
    LeaveCriticalSection( &g_PrintQueue.CriticalSection );
}

//-----------------------------------------------------------------------------
// ConsoleWindowPrintf
//
// Writes out a string directly to the console window
//-----------------------------------------------------------------------------
int ConsoleWindowPrintf( COLORREF rgb, LPCTSTR strFormat, ... )
{
    int       dwStrLen;
    char      strTemp[512];
    va_list   arglist;
    CHARRANGE cr = { -1, -2 };

    if ( rgb != XBX_CLR_DEFAULT )
    {
        // set whatever colors, etc. they want
        CHARFORMAT cf  = {0};
        cf.cbSize      = sizeof( cf );
        cf.dwMask      = CFM_COLOR;
        cf.dwEffects   = 0;
        cf.crTextColor = rgb;
        SendDlgItemMessage( g_hDlgMain, IDC_OUTPUT, EM_SETCHARFORMAT, SCF_SELECTION, ( LPARAM )&cf );
    }

    // Get our string to print
    va_start( arglist, strFormat );
    dwStrLen = _vsnprintf( strTemp, sizeof( strTemp ), strFormat, arglist );
    va_end( arglist );

    // Move the selection to the end
    SendDlgItemMessage( g_hDlgMain, IDC_OUTPUT, EM_EXSETSEL, 0, ( LPARAM )&cr );

    // Add the text and scroll it into view
    SendDlgItemMessage( g_hDlgMain, IDC_OUTPUT, EM_REPLACESEL, 0, ( LONG )( LPSTR )strTemp );
    SendDlgItemMessage( g_hDlgMain, IDC_OUTPUT, EM_SCROLLCARET, 0, 0L );

	if ( g_bPlayTestMode )
	{
		char szLogPath[MAX_PATH];
		char szLogName[MAX_PATH];
		V_snprintf( szLogName, sizeof( szLogName ), "vxconsole_%s.log", g_xboxTargetName ); 
		V_ComposeFileName( g_localPath, szLogName, szLogPath, sizeof( szLogPath ) );
		FILE *fp = fopen( szLogPath, "at+" );
		if ( fp )
		{
			fprintf( fp, strTemp );
			fclose( fp );
		}
	}

    return dwStrLen;
}

//-----------------------------------------------------------------------------
// ProcessCommand
// 
//-----------------------------------------------------------------------------
bool ProcessCommand( const char* strCmdIn )
{
	char    strRemoteCmd[MAX_PATH + 10];
    TCHAR	strCmdBak[MAX_PATH];
	char	strCmd[MAX_PATH];
    char*	argv[MAX_ARGVELEMS];
    BOOL	isXCommand = FALSE;
	BOOL	isLocal    = FALSE;
	BOOL	isRemote   = FALSE;
	int		iIndex;

	// local copy for destructive purposes
    strcpy( strCmd, strCmdIn );

    // copy of the original command string
    lstrcpyA( strCmdBak, strCmdIn );

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "] %s\n", strCmd );

	// parse argstring into components
    int argc = CmdToArgv( strCmd, argv, MAX_ARGVELEMS );
    if ( !argc )
	{
		// empty command
        return true;
	}

    if ( ( iIndex = ComboBox_GetCount( g_hwndCommandCombo ) ) >= MAX_COMMANDHISTORY )
	{
	    // Limit the history of items, purge oldest
		ComboBox_DeleteString( g_hwndCommandCombo, 0 );
	}

	// add to end of list
    iIndex = ComboBox_InsertItemData( g_hwndCommandCombo, -1, strCmdBak );
	ComboBox_SetCurSel( g_hwndCommandCombo, -1 );

    // find command in local list
    for ( int i=0; i<g_numLocalCommands; i++ )
    {
        if ( lstrcmpiA( g_localCommands[i].strCommand, argv[0] ) )
		{
			// no match
			continue;
		}

		isLocal = TRUE;
		if ( !g_localCommands[i].pfnHandler )
		{
			// no handler, remote xcommand 
			isXCommand = TRUE;
		}

		if ( ( g_localCommands[i].flags & FN_XBOX ) && !g_connectedToXBox )
		{
			// not allowed yet
			ConsoleWindowPrintf( RGB( 255,0,0 ), "'%s' is not available until connected to XBox.\n", argv[0] );
			return true;
		}
		else if ( ( g_localCommands[i].flags & FN_APP ) && !g_connectedToApp )
		{
			// not allowed yet
			ConsoleWindowPrintf( RGB( 255,0,0 ), "'%s' is not available until connected to Application.\n", argv[0] );
			return true;
		}
	
		if ( isXCommand )
			break;

		// do local command
		g_localCommands[i].pfnHandler( argc, argv );
		return true;
    }

	// find command in remote list
	if ( !isLocal && !isXCommand && g_connectedToApp )
	{
		for ( int i=0; i<g_numRemoteCommands; i++ )
		{
			if ( lstrcmpiA( g_remoteCommands[i]->strCommand, argv[0] ) )
			{
				// no match
				continue;
			}

			isRemote = TRUE;

			if ( !g_connectedToApp )
			{
				// not allowed yet
				ConsoleWindowPrintf( RGB( 255,0,0 ), "'%s' is not available until connected to Application.\n", argv[0] );
				return true;
			}
			break;
		}
	}

	if ( !isLocal && !isXCommand && !isRemote )
	{
		if ( !g_connectedToApp || g_numRemoteCommands != 0 )
		{
			// unrecognized
			ConsoleWindowPrintf( RGB( 255,0,0 ), "'%s' is not a recognized command.\n", argv[0] );
			return true;
		}
	}

    if ( isXCommand )
    {
		// send the xcommand directly
        lstrcpyA( strRemoteCmd, strCmdBak );
    }
    else
    {
		// add remote command prefix
        lstrcpyA( strRemoteCmd, VXCONSOLE_COMMAND_PREFIX "!" );
        lstrcatA( strRemoteCmd, strCmdBak );
    }

    // send the command to the Xbox
	HRESULT hr = DmAPI_SendCommand( strRemoteCmd, true );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "DmSendCommand", hr );
		return false;
	}

    return TRUE;
}

//-----------------------------------------------------------------------------
// CommandWindow_HandleKey
//
// Handle a WM_KEYDOWN in our RTF cmd window
//-----------------------------------------------------------------------------
BOOL CommandWindow_HandleKey( WPARAM wParam )
{
    BOOL	bHandled = FALSE;
	int		curSel;
	int		numItems;

	if ( wParam >= VK_F1 && wParam <= VK_F12 )
	{
		if ( Bindings_TranslateKey( wParam ) )
		{
			// handled
			return true;				
		}
	}

    switch ( wParam )
    {
		case VK_TAB:
		case VK_UP:
		case VK_DOWN:
			if ( IsWindowVisible( g_hwndCommandHint ) )
			{
				// hint window open
				char hintCmd[MAX_PATH];
				char userCmd[MAX_PATH];

				// scroll through the hint selections
				curSel = SendMessage( g_hwndCommandHint, (UINT)LB_GETCURSEL, NULL, NULL );
				SendMessage( g_hwndCommandHint, (UINT)LB_GETTEXT, (WPARAM)curSel, (LPARAM)hintCmd );

				numItems = SendMessage( g_hwndCommandHint, (UINT)LB_GETCOUNT, NULL, NULL );
				if ( numItems < 0 )
					numItems = 0;

				if ( wParam == VK_TAB )
				{
					// get command typed so far	
					ComboBox_GetText( g_hwndCommandCombo, userCmd, MAX_PATH );

					// strip the auto-space off the end
					int len = Q_strlen(userCmd);
					if ( userCmd[len-1] == ' ' )
					{
						userCmd[len-1] = '\0';
					}

					if ( !stricmp( userCmd, hintCmd ) )
					{
						// cycle to next or prev command with tab or shift-tab
						if ( GetKeyState(VK_SHIFT) < 0 )
						{
							wParam = VK_UP;
						}
						else 
						{
							wParam = VK_DOWN;
						}
					}
				}

				// move the selection
				if ( wParam == VK_UP )
					curSel--;
				else if ( wParam == VK_DOWN )
					curSel++;
				if ( curSel < 0 )
					curSel = numItems - 1;
				else if ( curSel > numItems-1 )
					curSel = 0;
				if ( curSel < 0 )
					curSel = 0;

				// set the selection and get highlighted command
				SendMessage( g_hwndCommandHint, (UINT)LB_SETCURSEL, (WPARAM)curSel, NULL );
				SendMessage( g_hwndCommandHint, (UINT)LB_GETTEXT, (WPARAM)curSel, (LPARAM)hintCmd );

				// add a space to the end for easier parameter setting
				Q_strncat( hintCmd, " ", sizeof(hintCmd), 1 );

				// replace command string
				ComboBox_SetText( g_hwndCommandCombo, hintCmd );

				// set cursor to end of command
				SendMessage( g_hwndCommandCombo, (UINT)CB_SETEDITSEL, (WPARAM)0, MAKELONG( MAX_PATH,MAX_PATH ) );

				bHandled = TRUE;
			}
			else
			{
				curSel = SendMessage( g_hwndCommandCombo, (UINT)CB_GETCURSEL, NULL, NULL );
				if ( curSel < 0 )
				{
					// combo box has no selection
					// override combo box behavior and set selection
					numItems = SendMessage( g_hwndCommandCombo, (UINT)CB_GETCOUNT, NULL, NULL );
					if ( numItems > 0 )
					{
						if ( wParam == VK_UP )
						{
							// set to bottom of list
							curSel = numItems-1;
						}
						else if ( wParam == VK_DOWN )
						{
							// set to top of list
							curSel = 0;
						}

						SendMessage( g_hwndCommandCombo, (UINT)CB_SETCURSEL, (WPARAM)curSel, NULL );
						bHandled = TRUE;
					}
				}
			}
			break;

        case VK_RETURN:
            // user hit return in the combo box
            if ( ComboBox_GetDroppedState( g_hwndCommandCombo ) )
            {
                ComboBox_ShowDropdown( g_hwndCommandCombo, FALSE );
            }
            else
            {
                PostMessage( g_hDlgMain, WM_APP, 0, 0 );
                bHandled = TRUE;
            }
            break;
    }

    return bHandled;
}

//-----------------------------------------------------------------------------
// CommandWindow_SubclassedProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK CommandWindow_SubclassedProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
		case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            if ( CommandWindow_HandleKey( wParam ) )
                return 0;
            break;

        case WM_CHAR:
            if ( wParam == VK_RETURN )
                return 0;
            break;	
	}

    return CallWindowProc( g_hwndCommandSubclassed, hDlg, msg, wParam, lParam );
}

//-----------------------------------------------------------------------------
// Main_SizeWindow
//
// Handles a WM_SIZE message by resizing all our child windows to match the main window
//-----------------------------------------------------------------------------
void Main_SizeWindow( HWND hDlg, UINT wParam, int cx, int cy )
{
    if ( cx==0 || cy==0 )
    {
        RECT rcClient;
        GetClientRect( hDlg, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

    // if we're big enough, position our child windows
    if ( g_hwndCommandCombo && cx > 64 && cy > 64 )
    {
        RECT rcCmd;
		RECT rcHint;
	    RECT rcOut;

        // fit the "command" combo box into our window
        GetWindowRect( g_hwndCommandCombo, &rcCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcCmd + 1 );
        int x  = rcCmd.left;
        int dx = cx - 8 - x;
        int dy = rcCmd.bottom - rcCmd.top;
        int y  = cy - 8 - dy;
        SetWindowPos( 
			g_hwndCommandCombo, 
			NULL,
			x,
			y,
			dx,
			dy,
			SWP_NOZORDER );

		// position the "hint popup" window above the "command" window
		GetWindowRect( g_hwndCommandHint, &rcHint );
		ScreenToClient( g_hDlgMain, ( LPPOINT )&rcHint );
		ScreenToClient( g_hDlgMain, ( LPPOINT )&rcHint + 1 );
		SetWindowPos( 
			g_hwndCommandHint, 
			NULL, 
			rcCmd.left,
			( rcCmd.top - 4 ) - ( rcHint.bottom - rcHint.top + 1 ),
			0,
			0,
			SWP_NOSIZE | SWP_NOZORDER );

        // position the "Cmd" label
        RECT rcStaticCmd;
        HWND hStaticCmd = GetDlgItem( g_hDlgMain, IDC_LABEL );
        GetWindowRect( hStaticCmd, &rcStaticCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcStaticCmd );
        ScreenToClient( hDlg, ( LPPOINT )&rcStaticCmd + 1 );
        SetWindowPos( 
			hStaticCmd, 
			NULL, 
			8,
			y + ( dy - ( rcStaticCmd.bottom - rcStaticCmd.top ) ) / 2 - 1,
            0,
			0,
			SWP_NOSIZE | SWP_NOZORDER );

	    // position the "output" window
		GetWindowRect( g_hwndOutputWindow, &rcOut );
        ScreenToClient( hDlg, ( LPPOINT )&rcOut );
        int dwWidth  = cx - rcOut.left - 8;
        int dwHeight = y - rcOut.top - 8;
        SetWindowPos( g_hwndOutputWindow, NULL, 0, 0, dwWidth, dwHeight, SWP_NOMOVE | SWP_NOZORDER );
    }
}

//-----------------------------------------------------------------------------
// _SortCommands
// 
//-----------------------------------------------------------------------------
int _SortCommands( const void* a, const void* b )
{
	const char* strA = *( const char** )a;
	const char* strB = *( const char** )b;

	return ( stricmp( strA, strB ) );
}

//-----------------------------------------------------------------------------
// EnableCommandHint
// 
// Open/Close the command hint popup
//-----------------------------------------------------------------------------
void EnableCommandHint( bool enable )
{
	int			w = 0;
	int			h = 0;
	int			itemHeight;
	int			i;
	int			maxLen;
	int			len;
	const char*	cmds[256];
	int			numLocalCommands;
	int			numRemoteCommands;
	int			curSel;

	if ( !enable )
		goto cleanUp;

	// get the current command
	char strCmd[MAX_PATH];
	ComboBox_GetText( g_hwndCommandCombo, strCmd, MAX_PATH );
	if ( !strCmd[0] )
	{
		// user has typed nothing
		enable = false;
		goto cleanUp;
	}

	SendMessage( g_hwndCommandHint, ( UINT )LB_RESETCONTENT, NULL, NULL );  

	// get a list of possible matches
	maxLen = 0;
	numLocalCommands  = MatchLocalCommands( strCmd, cmds, 256 );
	numRemoteCommands = MatchRemoteCommands( strCmd, cmds+numLocalCommands, 256-numLocalCommands );
	for ( i=0; i<numLocalCommands+numRemoteCommands; i++ )
	{
		len = strlen( cmds[i] );
		if ( maxLen < len )
			maxLen = len;		
	}
	if ( !maxLen )
	{
		// no matches
		enable = false;
		goto cleanUp;
	}

	// sort the list ( eschew listbox's autosorting )
	qsort( cmds, numLocalCommands+numRemoteCommands, sizeof( const char* ), _SortCommands );

	curSel = -1;
	len    = strlen( strCmd );
	for ( i=0; i<numLocalCommands+numRemoteCommands; i++ )
	{
		// populate the listbox
		SendMessage( g_hwndCommandHint, ( UINT )LB_ADDSTRING, 0, ( LPARAM )cmds[i] );
	
		// determine first best match
		if ( curSel == -1 && !strnicmp( strCmd, cmds[i], len ) )
			curSel = i;
	}

	if ( curSel != -1 )
	{
		// set the selection to the first best string
		// ensure the top string is shown ( avoids odd auto-vscroll choices )
		SendMessage( g_hwndCommandHint, ( UINT )LB_SETCURSEL, ( WPARAM )curSel, NULL );
		if ( !curSel )
			SendMessage( g_hwndCommandHint, ( UINT )LB_SETTOPINDEX, 0, NULL );
	}

	RECT rcCmd;
	GetWindowRect( g_hwndCommandCombo, &rcCmd );
	ScreenToClient( g_hDlgMain, ( LPPOINT )&rcCmd );
	ScreenToClient( g_hDlgMain, ( LPPOINT )&rcCmd + 1 );

	// clamp listbox height to client space
	itemHeight = SendMessage( g_hwndCommandHint, ( UINT )LB_GETITEMHEIGHT, 0, NULL );
	if ( itemHeight <= 0 )
	{
		// oops, shouldn't happen
		enable = false;
		goto cleanUp;
	}	

	h = ( numLocalCommands + numRemoteCommands )*itemHeight + 2;
	if ( h > rcCmd.top - 8)
	{
		h = rcCmd.top - 8;
	}
	
	// clamp listbox width
	w = ( maxLen + 5 ) * g_fixedFontMetrics.tmMaxCharWidth;

	// position the "hint popup" window above the "command" window
	SetWindowPos( 
		g_hwndCommandHint, 
		NULL, 
		rcCmd.left,
		( rcCmd.top - 4 ) - h,
		w,
		h,
		SWP_NOZORDER );

cleanUp:
	BOOL isVisible = IsWindowVisible( g_hwndCommandHint );
	if ( !enable && isVisible )
		ShowWindow( g_hwndCommandHint, SW_HIDE );
	else if ( enable && !isVisible )
		ShowWindow( g_hwndCommandHint, SW_SHOWNA );
}

//-----------------------------------------------------------------------------
// Main_DlgProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK Main_DlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD	wID = LOWORD( wParam );
	BOOL	isConnect;

    switch ( message )
    {
        case WM_APP:
			// user has pressed enter
			// take the string from the command window and process it
			// extra room for \r\n
            char strCmd[MAX_PATH + 3];
            ComboBox_GetText( g_hwndCommandCombo, strCmd, MAX_PATH );
            ProcessCommand( strCmd );
            ComboBox_SetText( g_hwndCommandCombo, "" );
			EnableCommandHint( false );
            break;

        case WM_USER:
            ProcessPrintQueue();
            break;

		case WM_TIMER:
			// Don't do auto-connect stuff while the Assert dialog is up
			// (it uses a synchronous command, so calling 'Dm' funcs here can cause a lockup)
			if ( !g_AssertDialogActive )
			{
				if ( wID == TIMERID_AUTOCONNECT )
				{
					AutoConnectTimerProc( hDlg, TIMERID_AUTOCONNECT );
					return 0L;
				}
			}
			break;

		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLOREDIT:
			SetBkColor( ( HDC )wParam,g_backgroundColor );
			return ( BOOL )g_hBackgroundBrush;
				
        case WM_SIZE:
			Main_SizeWindow( hDlg, wParam, LOWORD( lParam ), HIWORD( lParam ) );
            break;

        case WM_SYSCOMMAND:
            if ( wID == SC_CLOSE )
            {
                PostMessage( hDlg, WM_CLOSE, 0, 0 );
                return 0L;
            }
            break;

        case WM_CLOSE:
            // disconnect before closing
            lc_disconnect( 0, NULL );

			SaveConfig();
            DestroyWindow( hDlg );
            break;

        case WM_DESTROY:
            SubclassWindow( g_hwndCommandCombo, g_hwndCommandSubclassed );
            PostQuitMessage( 0 );
            return 0L;

        case WM_INITMENU:
			isConnect = g_connectedToXBox || g_connectedToApp;
            CheckMenuItem( ( HMENU )wParam, IDM_AUTOCONNECT, MF_BYCOMMAND | ( g_autoConnect ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( ( HMENU )wParam, IDM_CAPTUREGAMESPEW, MF_BYCOMMAND | ( g_captureGameSpew ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( ( HMENU )wParam, IDM_CAPTUREDEBUGSPEW, MF_BYCOMMAND | ( g_captureDebugSpew ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( ( HMENU )wParam, IDM_DEBUGCOMMANDS, MF_BYCOMMAND | ( g_debugCommands ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( ( HMENU )wParam, IDM_PLAYTESTMODE, MF_BYCOMMAND | ( g_bPlayTestMode ? MF_CHECKED : MF_UNCHECKED ) );
			EnableMenuItem( ( HMENU )wParam, IDM_SYNCFILES, MF_BYCOMMAND | ( g_connectedToXBox ? MF_ENABLED : MF_GRAYED ) );
			EnableMenuItem( ( HMENU )wParam, IDM_SYNCINSTALL, MF_BYCOMMAND | ( g_connectedToXBox ? MF_ENABLED : MF_GRAYED ) );
			return 0L;

        case WM_COMMAND:
            switch ( wID )
            {
				case IDC_COMMAND:
					switch ( HIWORD( wParam ) )
					{
						case CBN_EDITCHANGE:
						case CBN_SETFOCUS:
							EnableCommandHint( true );
							break;

						case CBN_KILLFOCUS:
							EnableCommandHint( false );
							break;
					}
					break;

				case IDM_CONFIG:
					ConfigDlg_Open();
					return 0L;
			
                case IDM_CAPTUREGAMESPEW:
                    g_captureGameSpew ^= 1;
                    return 0L;

                case IDM_CAPTUREDEBUGSPEW:
                    g_captureDebugSpew ^= 1;
                    return 0L;

				case IDM_DEBUGCOMMANDS:
					g_debugCommands ^= 1;
					return 0L;

				case IDM_BUG:
					BugDlg_Open();
					return 0L;

				case IDM_MEMORYDUMP:
					ShowMemDump_Open();
					return 0L;

				case IDM_TIMESTAMPLOG:
					TimeStampLog_Open();
					return 0L;

				case IDM_SYNCFILES:
					SyncFilesDlg_Open();
					return 0L;

				case IDM_SYNCINSTALL:
					InstallDlg_Open();
					return 0L;

				case IDM_EXCLUDEPATHS:
					ExcludePathsDlg_Open();
					return 0L;

				case IDM_CPU_SAMPLES:
					CpuProfileSamples_Open();
					return 0L;

				case IDM_CPU_HISTORY:
					CpuProfileHistory_Open();
					return 0L;

				case IDM_D3D_SAMPLES:
					TexProfileSamples_Open();
					return 0L;

				case IDM_D3D_HISTORY:
					TexProfileHistory_Open();
					return 0L;

				case IDM_SHOWMEMORYUSAGE:
					MemProfile_Open();
					return 0L;

				case IDM_PLAYTESTMODE:
					g_bPlayTestMode ^= 1;
					return 0L;

				case IDM_SHOWRESOURCES_TEXTURES:
					ShowTextures_Open();
					return 0L;

				case IDM_SHOWRESOURCES_MATERIALS:
					ShowMaterials_Open();
					return 0L;

				case IDM_SHOWRESOURCES_SOUNDS:
					ShowSounds_Open();
					return 0L;

				case IDM_SHOWRESOURCES_MODELS:
					NotImplementedYet();
					return 0L;

                case IDM_AUTOCONNECT:
                    if ( g_connectedToXBox || g_connectedToApp || g_autoConnect )
                        lc_disconnect( 0, NULL );
                    else
						lc_autoConnect( 0, NULL );
                    return 0L;

				case IDM_BINDINGS_EDIT:
					Bindings_Open();
					return 0L;

				case IDM_BINDINGS_BIND1:
				case IDM_BINDINGS_BIND2:
				case IDM_BINDINGS_BIND3:
				case IDM_BINDINGS_BIND4:
				case IDM_BINDINGS_BIND5:
				case IDM_BINDINGS_BIND6:
				case IDM_BINDINGS_BIND7:
				case IDM_BINDINGS_BIND8:
				case IDM_BINDINGS_BIND9:
				case IDM_BINDINGS_BIND10:
				case IDM_BINDINGS_BIND11:
				case IDM_BINDINGS_BIND12:
					Bindings_MenuSelection( wID );
					return 0L;

                case IDM_EXIT:
                    PostMessage( hDlg, WM_CLOSE, 0, 0 );
                    return 0L;
            }
            break;
    }

    return ( DefDlgProc( hDlg, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	CmdToArgv
//
//	Parses a string into argv and return # of args.
//-----------------------------------------------------------------------------
int CmdToArgv( char* str, char* argv[], int maxargs )
{
    int   argc = 0;
    int   argcT = 0;
    char* strNil = str + lstrlenA( str );

    while ( argcT < maxargs )
    {
        // Eat whitespace
        while ( *str && ( *str==' ' ) )
            str++;

        if ( !*str )
        {
            argv[argcT++] = strNil;
        }
        else
        {
            // Find the end of this arg
            char  chEnd = ( *str == '"' || *str == '\'' ) ? *str++ : ' ';
            char* strArgEnd = str;
            while ( *strArgEnd && ( *strArgEnd != chEnd ) )
                strArgEnd++;

            // Record this argument
            argv[argcT++] = str;
            argc = argcT;

            // Move strArg to the next argument ( or not, if we hit the end )
            str = *strArgEnd ? strArgEnd + 1 : strArgEnd;
            *strArgEnd = 0;
        }
    }

    return argc;
}

//-----------------------------------------------------------------------------
//	CreateCommandHint
// 
//-----------------------------------------------------------------------------
void CreateCommandHint()
{
	// create the "hint" popup
	g_hwndCommandHint = CreateWindowEx( 
			WS_EX_NOPARENTNOTIFY, 
			"LISTBOX", 
			"", 
			WS_BORDER|WS_CHILD|LBS_HASSTRINGS|WS_VSCROLL, 
			0, 0, 100, 0, 
			g_hDlgMain, 
			( HMENU )IDC_COMMANDHINT, 
			g_hInstance, 
			NULL ); 

	// force the popup to head of z-order
	// to draw over all other children
	BringWindowToTop( g_hwndCommandHint );

	// set font
	SendDlgItemMessage( g_hDlgMain, IDC_COMMANDHINT, WM_SETFONT, ( WPARAM )g_hFixedFont, TRUE );
}

//-----------------------------------------------------------------------------
//	CreateResources
// 
//-----------------------------------------------------------------------------
bool CreateResources()
{
	LOGFONT lf; 
	HFONT	hFontOld;
	HDC		hDC;
	int		i;

    // pull in common controls
	INITCOMMONCONTROLSEX initCommon;
	initCommon.dwSize = sizeof( INITCOMMONCONTROLSEX );
	initCommon.dwICC  = ICC_LISTVIEW_CLASSES|ICC_USEREX_CLASSES;
    if ( !InitCommonControlsEx( &initCommon ) )
		return false;

	// pull in rich edit controls
    g_hRichEdit = LoadLibrary( "Riched32.dll" );
	if ( !g_hRichEdit )
		return false;
	
	g_backgroundColor   = XBX_CLR_LTGREY;
	g_hBackgroundBrush  = CreateSolidBrush( g_backgroundColor );

	// get icons
	g_hIcons[ICON_APPLICATION]    = LoadIcon( g_hInstance, MAKEINTRESOURCE( IDI_VXCONSOLE ) );
	g_hIcons[ICON_DISCONNECTED]   = LoadIcon( g_hInstance, MAKEINTRESOURCE( IDI_DISCONNECTED ) );
	g_hIcons[ICON_CONNECTED_XBOX] = LoadIcon( g_hInstance, MAKEINTRESOURCE( IDI_CONNECT1_ON ) );
	g_hIcons[ICON_CONNECTED_APP0] = LoadIcon( g_hInstance, MAKEINTRESOURCE( IDI_CONNECT2_OFF ) );
	g_hIcons[ICON_CONNECTED_APP1] = LoadIcon( g_hInstance, MAKEINTRESOURCE( IDI_CONNECT2_ON ) );
	for ( i=0; i<MAX_ICONS; i++ )
	{
		if ( !g_hIcons[i] )
			return false;
	}

	// get the font feight
	hDC = GetWindowDC( NULL );
	int nHeight = -MulDiv( VXCONSOLE_FONTSIZE, GetDeviceCaps( hDC, LOGPIXELSY ), 72 );
	ReleaseDC( NULL, hDC );

	// create fixed font
	memset( &lf, 0, sizeof( LOGFONT ) ); 
	lf.lfHeight = nHeight; 
	lf.lfWeight = 400;
	strcpy( lf.lfFaceName, VXCONSOLE_FONT ); 
	g_hFixedFont = CreateFontIndirect( &lf );
	if ( !g_hFixedFont )
		return false;

	// create proportional font
	memset( &lf, 0, sizeof( LOGFONT ) ); 
	lf.lfHeight = -11; 
	lf.lfWeight = 400;
	strcpy( lf.lfFaceName, "Tahoma" ); 
	g_hProportionalFont = CreateFontIndirect( &lf );
	if ( !g_hProportionalFont )
		return false;

	// get the font metrics
	hDC = GetWindowDC( NULL );
	hFontOld = ( HFONT )SelectObject( hDC, g_hFixedFont );
	GetTextMetrics( hDC, &g_fixedFontMetrics );
	SelectObject( hDC, hFontOld );
	ReleaseDC( NULL, hDC );

	return true;
}

//-----------------------------------------------------------------------------
//	Shutdown
// 
// Free all resources
//-----------------------------------------------------------------------------
void Shutdown()
{
	BugReporter_FreeInterfaces();

	if ( g_PrintQueue.bInit )
	{
		DeleteCriticalSection( &g_PrintQueue.CriticalSection );
		g_PrintQueue.bInit = false;
	}

	if ( g_hCommandReadyEvent )
	{
		CloseHandle( g_hCommandReadyEvent );
		g_hCommandReadyEvent = 0;
	}

	if ( g_hRichEdit )
	{
		FreeLibrary( g_hRichEdit );
		g_hRichEdit = 0;
	}

	if ( g_hBackgroundBrush )
	{
		DeleteObject( g_hBackgroundBrush );
		g_hBackgroundBrush = 0;
	}

	if ( g_hFixedFont )
	{
		DeleteObject( g_hFixedFont );
		g_hFixedFont = 0;
	}

	if ( g_hProportionalFont )
	{
		DeleteObject( g_hProportionalFont );
		g_hProportionalFont = 0;
	}
}

//-----------------------------------------------------------------------------
//	Startup
// 
//-----------------------------------------------------------------------------
bool Startup()
{	
	// Load the xenon debugging dll (xbdm.dll) manually due to its absence from system path
    const char *pPath = getenv( "path" );
    const char *pXEDKDir = getenv( "xedk" );
    if ( !pXEDKDir )
        pXEDKDir = "";

	int len = strlen( pPath ) + strlen( pXEDKDir ) + 256;
	char *pNewPath = (char*)_alloca( len );
	sprintf( pNewPath, "path=%s;%s\\bin\\win32", pPath, pXEDKDir );
    _putenv( pNewPath );

	HMODULE hXBDM = LoadLibrary( TEXT( "xbdm.dll" ) );
    if ( !hXBDM )
    {
        if ( pXEDKDir[0] )
            Sys_Error( "Couldn't load xbdm.dll" );
        else
            Sys_Error( "Couldn't load xbdm.dll\nXEDK environment variable not set." );
    }

	LoadConfig();

	if ( !CreateResources() )
		return false;

    // set up our print queue
    InitializeCriticalSection( &g_PrintQueue.CriticalSection );
	g_PrintQueue.bInit = true;

	// manual reset, initially signaled
	g_hCommandReadyEvent = CreateEvent( NULL, TRUE, TRUE, NULL );

    // set up our window class
    WNDCLASS wndclass;
	memset( &wndclass, 0, sizeof( wndclass ) );
    wndclass.style         = 0;
    wndclass.lpfnWndProc   = Main_DlgProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = VXCONSOLE_WINDOWBYTES;
    wndclass.hInstance     = g_hInstance;
    wndclass.hIcon         = g_hIcons[ICON_DISCONNECTED];
    wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
    wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_VXCONSOLE );
    wndclass.lpszClassName = VXCONSOLE_CLASSNAME;
    if ( !RegisterClass( &wndclass ) )
		return false;

    g_hAccel = LoadAccelerators( g_hInstance, MAKEINTRESOURCE( IDR_MAIN_ACCEL ) );
	if ( !g_hAccel )
		return false;

	// Create our main dialog
	g_hDlgMain = CreateDialog( g_hInstance, MAKEINTRESOURCE( IDD_VXCONSOLE ), 0, NULL );
    if ( !g_hDlgMain )
		return false;
	SetWindowLong( g_hDlgMain, VXCONSOLE_CONFIGID, g_configID );

	if ( !BugDlg_Init() )
		return false;

	if ( !CpuProfile_Init() )
		return false;

	if ( !TexProfile_Init() )
		return false;

	if ( !MemProfile_Init() )
		return false;

	if ( !Bindings_Init() )
		return false;

	if ( !SyncFilesDlg_Init() )
		return false;

	if ( !ShowMaterials_Init() )
		return false;

	if ( !ShowTextures_Init() )
		return false;

	if ( !ShowSounds_Init() )
		return false;

	if ( !ShowMemDump_Init() )
		return false;

	if ( !TimeStampLog_Init() )
		return false;

	if ( !ExcludePathsDlg_Init() )
		return false;

    g_hwndOutputWindow = GetDlgItem( g_hDlgMain, IDC_OUTPUT );
    g_hwndCommandCombo = GetDlgItem( g_hDlgMain, IDC_COMMAND );

	CreateCommandHint();

    // subclass our dropdown command listbox to handle return key
	g_hwndCommandSubclassed = SubclassWindow( GetWindow( g_hwndCommandCombo, GW_CHILD ), CommandWindow_SubclassedProc );

    // Change the font type of the output window to courier
    CHARFORMAT cf;
    cf.cbSize = sizeof( CHARFORMAT );
    SendMessage( g_hwndOutputWindow, EM_GETCHARFORMAT, 0, ( LPARAM )&cf );
    cf.dwMask &= ~CFM_COLOR;
	cf.yHeight = VXCONSOLE_FONTSIZE*20;
    lstrcpyA( cf.szFaceName, VXCONSOLE_FONT );
    SendMessage( g_hwndOutputWindow, EM_SETCHARFORMAT, SCF_ALL, ( LPARAM )&cf );
	SendMessage( g_hwndOutputWindow, EM_SETBKGNDCOLOR, 0, g_backgroundColor );

	// ensure the output window adheres to its z ordering
	LONG style = GetWindowLong( g_hwndOutputWindow, GWL_STYLE );
	style |= WS_CLIPSIBLINGS;
	SetWindowLong( g_hwndOutputWindow, GWL_STYLE, style );

	// change the font of the command and its hint window to courier
	SendDlgItemMessage( g_hDlgMain, IDC_COMMAND, WM_SETFONT, ( WPARAM )g_hFixedFont, TRUE );

	// set the window title
	SetMainWindowTitle();

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "VXConsole %s [%s Build: %s %s] [Protocol: %d]\n", VXCONSOLE_VERSION, VXCONSOLE_BUILDTYPE, __DATE__, __TIME__, VXCONSOLE_PROTOCOL_VERSION );
	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "type '*help' for list of commands...\n\n" );

	g_currentIcon = -1;
	SetConnectionIcon( ICON_DISCONNECTED );

	if ( g_alwaysAutoConnect)
	{
		// user wants to auto-connect at startup
		lc_autoConnect( 0, NULL );
	}

	if ( g_mainWindowRect.right && g_mainWindowRect.bottom )
		MoveWindow( g_hDlgMain, g_mainWindowRect.left, g_mainWindowRect.top, g_mainWindowRect.right-g_mainWindowRect.left, g_mainWindowRect.bottom-g_mainWindowRect.top, FALSE );

	// ready for display
	int cmdShow = SW_SHOWNORMAL;
	if ( g_startMinimized )
		cmdShow = SW_SHOWMINIMIZED;
    ShowWindow( g_hDlgMain, cmdShow );

	// success
	return true;
}

//-----------------------------------------------------------------------------
//	WinMain
//
//	Entry point for program
//-----------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow )
{
	bool	error = true;
	MSG		msg = {0};

	g_hInstance = hInstance;

	g_bSuppressBlink = ParseCommandLineArg( pCmdLine, "-noblink", NULL, 0 );

	// optional -config <ID> can specify a specific configuration
	char buff[128];
	buff[0] = '\0';
	ParseCommandLineArg( pCmdLine, "-config ", buff, sizeof( buff ) );
	g_configID = atoi( buff );

	MakeConfigString( VXCONSOLE_REGISTRY, g_configID, buff, sizeof( buff ) );
	Sys_SetRegistryPrefix( buff );

	HWND hwnd = FindWindow( VXCONSOLE_CLASSNAME, NULL );
	if ( hwnd && GetWindowLong( hwnd, VXCONSOLE_CONFIGID ) == g_configID ) 
	{
		// single instance only
		// bring other version to foreground
		if ( IsIconic( hwnd ) )
			ShowWindow( hwnd, SW_RESTORE );
		SetForegroundWindow( hwnd );
		return ( FALSE );
	}

    if ( !Startup() )
		goto cleanUp;

	// message pump
	while ( GetMessage( &msg, NULL, 0, 0 ) )
    {
        if ( !TranslateAccelerator( g_hDlgMain, g_hAccel, &msg ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

	// no-error, end of app
	error = false;

cleanUp:
	if ( error )
	{
        char str[255];
        FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, str, 255, NULL );
		MessageBox( NULL, str, NULL, MB_OK );
	}

	Shutdown();

    return ( msg.wParam );
}


