//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	TIMESTAMP_LOG.CPP
//
//	TimeStamp Log Display.
//=====================================================================================//
#include "vxconsole.h"

#define ID_TIMESTAMPLOG_LISTVIEW 100

// column id
#define ID_TSL_TIME			0
#define ID_TSL_DELTATIME	1
#define ID_TSL_MEMORY		2
#define ID_TSL_DELTAMEMORY	3
#define ID_TSL_MESSAGE		4

typedef struct
{	const CHAR*			name;
	int					width;
	int					subItemIndex;
	CHAR				nameBuff[32];
} label_t;

struct timeStampLogNode_t
{
	int					index;
	float				time;
	float				deltaTime;
	int					memory;
	int					deltaMemory;
	char				timeBuff[32];
	char				deltaTimeBuff[32];
	char				memoryBuff[32];
	char				deltaMemoryBuff[32];
	char				*pMessage;
	timeStampLogNode_t	*pNext;
};

HWND				g_timeStampLog_hWnd;
HWND				g_timeStampLog_hWndListView;
RECT				g_timeStampLog_windowRect;
timeStampLogNode_t	*g_timeStampLog_pNodes;
int					g_timeStampLog_sortColumn;
int					g_timeStampLog_sortDescending;

label_t g_timeStampLog_Labels[] =
{
	{"Time",			100,	ID_TSL_TIME},
	{"Delta Time",		100,	ID_TSL_DELTATIME},
	{"Memory",			100,	ID_TSL_MEMORY},
	{"Delta Memory",	100,	ID_TSL_DELTAMEMORY},
	{"Message",			400,	ID_TSL_MESSAGE},
};

//-----------------------------------------------------------------------------
//	TimeStampLog_CompareFunc
// 
//-----------------------------------------------------------------------------
int CALLBACK TimeStampLog_CompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	timeStampLogNode_t*	pNodeA = ( timeStampLogNode_t* )lParam1;
	timeStampLogNode_t*	pNodeB = ( timeStampLogNode_t* )lParam2;

	int sort = 0;
	switch ( g_timeStampLog_sortColumn )
	{
		case ID_TSL_TIME:
			sort = ( int )( 1000.0f*pNodeA->time - 1000.0f*pNodeB->time );
			break;

		case ID_TSL_DELTATIME:
			sort = ( int )( 1000.0f*pNodeA->deltaTime - 1000.0f*pNodeB->deltaTime );
			break;

		case ID_TSL_MEMORY:
			sort = pNodeA->memory - pNodeB->memory;
			break;

		case ID_TSL_DELTAMEMORY:
			sort = pNodeA->deltaMemory - pNodeB->deltaMemory;
			break;

		case ID_TSL_MESSAGE:
			sort = stricmp( pNodeA->pMessage, pNodeB->pMessage );
			break;
	}

	// flip the sort order
	if ( g_timeStampLog_sortDescending )
		sort *= -1;

	return ( sort );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_SortItems
// 
//-----------------------------------------------------------------------------
void TimeStampLog_SortItems()
{
	LVITEM				lvitem;
	timeStampLogNode_t	*pNode;
	int					i;

	if ( !g_timeStampLog_hWnd )
	{
		// only sort if window is visible
		return;
	}

	ListView_SortItems( g_timeStampLog_hWndListView, TimeStampLog_CompareFunc, 0 );

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask = LVIF_PARAM;

	// get each item and reset its list index
	int itemCount = ListView_GetItemCount( g_timeStampLog_hWndListView );
	for ( i=0; i<itemCount; i++ )
	{
		lvitem.iItem = i;
		ListView_GetItem( g_timeStampLog_hWndListView, &lvitem );

		pNode = ( timeStampLogNode_t* )lvitem.lParam;
		pNode->index = i;
	}

	// update list view columns with sort key
	for ( i=0; i<sizeof( g_timeStampLog_Labels )/sizeof( g_timeStampLog_Labels[0] ); i++ )
	{
		char		symbol;
		LVCOLUMN	lvc; 

		if ( i == g_timeStampLog_sortColumn )
			symbol = g_timeStampLog_sortDescending ? '<' : '>';
		else
			symbol = ' ';
		sprintf( g_timeStampLog_Labels[i].nameBuff, "%s %c", g_timeStampLog_Labels[i].name, symbol );

		memset( &lvc, 0, sizeof( lvc ) );
		lvc.mask    = LVCF_TEXT; 
		lvc.pszText = ( LPSTR )g_timeStampLog_Labels[i].nameBuff;

		ListView_SetColumn( g_timeStampLog_hWndListView, i, &lvc );
	}
}

//-----------------------------------------------------------------------------
//	TimeStampLog_AddViewItem
// 
//-----------------------------------------------------------------------------
void TimeStampLog_AddViewItem( timeStampLogNode_t* pNode )
{
	LVITEM	lvi;

	if ( !g_timeStampLog_hWnd )
	{
		// only valid if log window is visible
		return;
	}

	// update the text callback buffers
	sprintf( pNode->timeBuff, "%2.2d:%2.2d:%2.2d:%3.3d", ( int )pNode->time/3600, ( ( int )pNode->time/60 )%60, ( int )pNode->time%60, ( int )( 1000*( pNode->time-( int )pNode->time ) )%1000 );
	sprintf( pNode->deltaTimeBuff, "%.3f", pNode->deltaTime );
	sprintf( pNode->memoryBuff, "%.2f MB", pNode->memory/( 1024.0f*1024.0f ) );
	sprintf( pNode->deltaMemoryBuff, "%d", pNode->deltaMemory );

	int itemCount = ListView_GetItemCount( g_timeStampLog_hWndListView );

	// setup and insert at end of list
	memset( &lvi, 0, sizeof( lvi ) );
	lvi.mask      = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	lvi.iItem     = itemCount;
	lvi.iSubItem  = 0;
	lvi.state     = 0; 
	lvi.stateMask = 0;
	lvi.pszText   = LPSTR_TEXTCALLBACK;  
	lvi.lParam    = ( LPARAM )pNode;
								 
	// insert
	pNode->index = ListView_InsertItem( g_timeStampLog_hWndListView, &lvi );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_AddItem
// 
//-----------------------------------------------------------------------------
void TimeStampLog_AddItem( float time, float deltaTime, int memory, int deltaMemory, const char* pMessage )
{
	timeStampLogNode_t*	pNode;

	// create and init
	pNode = new timeStampLogNode_t;
	memset( pNode, 0, sizeof( timeStampLogNode_t ) );

	pNode->time        = time;
	pNode->deltaTime   = deltaTime;
	pNode->memory      = memory;
	pNode->deltaMemory = deltaMemory;
	pNode->pMessage    = Sys_CopyString( pMessage ? pMessage : "" );

	// link in
	pNode->pNext = g_timeStampLog_pNodes;
	g_timeStampLog_pNodes = pNode;

	TimeStampLog_AddViewItem( pNode );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_Clear
// 
//-----------------------------------------------------------------------------
void TimeStampLog_Clear()
{
	timeStampLogNode_t*	pNode;
	timeStampLogNode_t*	pNextNode;

	// delete all the list view entries
	if ( g_timeStampLog_hWnd )
		ListView_DeleteAllItems( g_timeStampLog_hWndListView );

	// delete nodes in chain
	pNode = g_timeStampLog_pNodes;
	while ( pNode )
	{
		pNextNode = pNode->pNext;

		Sys_Free( pNode->pMessage );
		delete pNode;	
		
		pNode = pNextNode;
	}
	g_timeStampLog_pNodes = NULL;
}

//-----------------------------------------------------------------------------
//	TimeStampLog_SaveConfig
// 
//-----------------------------------------------------------------------------
void TimeStampLog_SaveConfig()
{
	char	buff[256];

	Sys_SetRegistryInteger( "timeStampLogSortColumn", g_timeStampLog_sortColumn );
	Sys_SetRegistryInteger( "timeStampLogSortDescending", g_timeStampLog_sortDescending );

	WINDOWPLACEMENT wp;
	memset( &wp, 0, sizeof( wp ) );
	wp.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( g_timeStampLog_hWnd, &wp );
	g_timeStampLog_windowRect = wp.rcNormalPosition;

	sprintf( buff, "%d %d %d %d", g_timeStampLog_windowRect.left, g_timeStampLog_windowRect.top, g_timeStampLog_windowRect.right, g_timeStampLog_windowRect.bottom );
	Sys_SetRegistryString( "timeStampLogWindowRect", buff );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_LoadConfig	
// 
//-----------------------------------------------------------------------------
void TimeStampLog_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	Sys_GetRegistryInteger( "timeStampLogSortColumn", ID_TSL_TIME, g_timeStampLog_sortColumn );
	Sys_GetRegistryInteger( "timeStampLogSortDescending", false, g_timeStampLog_sortDescending );

	Sys_GetRegistryString( "timeStampLogWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_timeStampLog_windowRect.left, &g_timeStampLog_windowRect.top, &g_timeStampLog_windowRect.right, &g_timeStampLog_windowRect.bottom );
	if ( numArgs != 4 || g_timeStampLog_windowRect.left < 0 || g_timeStampLog_windowRect.top < 0 || g_timeStampLog_windowRect.right < 0 || g_timeStampLog_windowRect.bottom < 0 )
		memset( &g_timeStampLog_windowRect, 0, sizeof( g_timeStampLog_windowRect ) );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_SizeWindow
// 
//-----------------------------------------------------------------------------
void TimeStampLog_SizeWindow( HWND hwnd, int width, int height )
{
    if ( width==0 || height==0 )
    {
        RECT rcClient;
        GetClientRect( hwnd, &rcClient );
        width  = rcClient.right;
        height = rcClient.bottom;
    }

	// position the ListView
	SetWindowPos( g_timeStampLog_hWndListView, NULL, 0, 0, width, height, SWP_NOZORDER );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_Export
// 
//-----------------------------------------------------------------------------
void TimeStampLog_Export()
{
	OPENFILENAME		ofn;
	char				logFilename[MAX_PATH]; 
	int					retval;
	FILE*				fp;
	int					i;
	int					count;

	memset( &ofn, 0, sizeof( ofn ) );
	ofn.lStructSize     = sizeof( ofn );
	ofn.hwndOwner       = g_timeStampLog_hWnd;
	ofn.lpstrFile       = logFilename;
	ofn.lpstrFile[0]    = '\0';
	ofn.nMaxFile        = sizeof( logFilename );
	ofn.lpstrFilter     = "Excel CSV\0*.CSV\0All Files\0*.*\0";
	ofn.nFilterIndex    = 1;
	ofn.lpstrFileTitle  = NULL;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = "c:\\";
	ofn.Flags           = OFN_PATHMUSTEXIST;

	// display the open dialog box 
	retval = GetOpenFileName( &ofn ); 
	if ( !retval )
		return;

	Sys_AddExtension( ".csv", logFilename, sizeof( logFilename ) );

	fp = fopen( logFilename, "wt+" );
	if ( !fp )
		return;

	// labels
	count = ARRAYSIZE( g_timeStampLog_Labels );
	for ( i=0; i<count; i++ )
	{
		fprintf( fp, "\"%s\"", g_timeStampLog_Labels[i].name );
		if ( i != count-1 )
			fprintf( fp, "," );
	}
	fprintf( fp, "\n" );

	// build a list for easy reverse traversal
	CUtlVector< timeStampLogNode_t* > nodeList;
	timeStampLogNode_t	*pNode;
	pNode = g_timeStampLog_pNodes;
	while ( pNode )
	{
		nodeList.AddToTail( pNode );
		pNode = pNode->pNext;
	}

	// dump to the log
	for ( int i=nodeList.Count()-1; i>=0; i-- )
	{
		pNode = nodeList[i];

		fprintf( fp, "\"%s\"", pNode->timeBuff );
		fprintf( fp, ",\"%s\"", pNode->deltaTimeBuff );
		fprintf( fp, ",\"%s\"", pNode->memoryBuff );
		fprintf( fp, ",\"%s\"", pNode->deltaMemoryBuff );
		fprintf( fp, ",\"%s\"", pNode->pMessage );
		fprintf( fp, "\n" );
	}

	fclose( fp );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK TimeStampLog_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD				wID = LOWORD( wParam );
	timeStampLogNode_t	*pNode;

	switch ( message )
	{
		case WM_CREATE:
			return 0L;

		case WM_DESTROY:
			TimeStampLog_SaveConfig();
			g_timeStampLog_hWnd = NULL;
			return 0L;
	
		case WM_SIZE:
			TimeStampLog_SizeWindow( hwnd, LOWORD( lParam ), HIWORD( lParam ) );
			return 0L;

		case WM_NOTIFY:
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_COLUMNCLICK:
					NMLISTVIEW*	pnmlv;
					pnmlv = ( NMLISTVIEW* )lParam; 
					if ( g_timeStampLog_sortColumn == pnmlv->iSubItem )
					{
						// user has clicked on same column - flip the sort
						g_timeStampLog_sortDescending ^= 1;
					}
					else
					{
						// sort by new column
						g_timeStampLog_sortColumn = pnmlv->iSubItem;
					}
					TimeStampLog_SortItems();
					return 0L;

				case LVN_GETDISPINFO:
					NMLVDISPINFO* plvdi;
					plvdi = ( NMLVDISPINFO* )lParam;
					pNode = ( timeStampLogNode_t * )plvdi->item.lParam;
					switch ( plvdi->item.iSubItem )
					{
						case ID_TSL_TIME:
							plvdi->item.pszText = pNode->timeBuff;
							return 0L;
	  					
						case ID_TSL_DELTATIME:
							plvdi->item.pszText = pNode->deltaTimeBuff;
							return 0L;

						case ID_TSL_MEMORY:
							plvdi->item.pszText = pNode->memoryBuff;
							return 0L;

						case ID_TSL_DELTAMEMORY:
							plvdi->item.pszText = pNode->deltaMemoryBuff;
							return 0L;

						case ID_TSL_MESSAGE:
							plvdi->item.pszText = pNode->pMessage;
							return 0L;

						default:
							break;
					}
					break;
			}
			break;

        case WM_COMMAND:
            switch ( wID )
            {
				case IDM_OPTIONS_CLEAR:
					TimeStampLog_Clear();
					return 0L;

				case IDM_OPTIONS_EXPORT:
					TimeStampLog_Export();
					return 0L;
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	TimeStampLog_Init
// 
//-----------------------------------------------------------------------------
bool TimeStampLog_Init()
{
	// set up our window class
	WNDCLASS wndclass;

	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = TimeStampLog_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_TIMESTAMPLOG );
	wndclass.lpszClassName = "TIMESTAMPLOGCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	TimeStampLog_LoadConfig();

	return true;
}

//-----------------------------------------------------------------------------
//	TimeStampLog_Open	
// 
//-----------------------------------------------------------------------------
void TimeStampLog_Open()
{
	RECT				clientRect;
	HWND				hWnd;
	int					i;
	timeStampLogNode_t	*pNode;

	if ( g_timeStampLog_hWnd )
	{
		// only one file log instance
		if ( IsIconic( g_timeStampLog_hWnd ) )
			ShowWindow( g_timeStampLog_hWnd, SW_RESTORE );
		SetForegroundWindow( g_timeStampLog_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"TIMESTAMPLOGCLASS",
				"TimeStamp Log",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				300,
				g_hDlgMain,
				NULL,
				g_hInstance,
				NULL );
	g_timeStampLog_hWnd = hWnd;

	GetClientRect( g_timeStampLog_hWnd, &clientRect ); 
	hWnd = CreateWindow( 
				WC_LISTVIEW, 
				"", 
				WS_VISIBLE|WS_CHILD|LVS_REPORT, 
				0, 
				0, 
				clientRect.right-clientRect.left,
				clientRect.bottom-clientRect.top, 
				g_timeStampLog_hWnd,
				( HMENU )ID_TIMESTAMPLOG_LISTVIEW,
				g_hInstance,
				NULL ); 
	g_timeStampLog_hWndListView = hWnd;

	// init list view columns
	for ( i=0; i<sizeof( g_timeStampLog_Labels )/sizeof( g_timeStampLog_Labels[0] ); i++ )
	{
		LVCOLUMN lvc; 
		memset( &lvc, 0, sizeof( lvc ) );

		lvc.mask     = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
		lvc.iSubItem = 0;
		lvc.cx       = g_timeStampLog_Labels[i].width;
		lvc.fmt      = LVCFMT_LEFT;
		lvc.pszText  = ( LPSTR )g_timeStampLog_Labels[i].name;

		ListView_InsertColumn( g_timeStampLog_hWndListView, i, &lvc );
	}
	
	ListView_SetBkColor( g_timeStampLog_hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( g_timeStampLog_hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx( g_timeStampLog_hWndListView, style, style );

	// populate list view
	pNode = g_timeStampLog_pNodes;
	while ( pNode )
	{
		TimeStampLog_AddViewItem( pNode );
		pNode = pNode->pNext;
	}
	TimeStampLog_SortItems();

	if ( g_timeStampLog_windowRect.right && g_timeStampLog_windowRect.bottom )
		MoveWindow( g_timeStampLog_hWnd, g_timeStampLog_windowRect.left, g_timeStampLog_windowRect.top, g_timeStampLog_windowRect.right-g_timeStampLog_windowRect.left, g_timeStampLog_windowRect.bottom-g_timeStampLog_windowRect.top, FALSE );
	ShowWindow( g_timeStampLog_hWnd, SHOW_OPENWINDOW );
}


//-----------------------------------------------------------------------------
// rc_TimeStampLog
//
// Sent from application with time stamp log
//-----------------------------------------------------------------------------
int rc_TimeStampLog( char* commandPtr )
{
	char			*cmdToken;
	int				timeStampAddr;
	int				retAddr;
	int				retVal;
	int				errCode = -1;
	xrTimeStamp_t	timeStamp;

	// get data
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &timeStampAddr );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &retAddr );

	// get the caller's data
	DmGetMemory( ( void* )timeStampAddr, sizeof( xrTimeStamp_t ), &timeStamp, NULL );

	// swap the structure
	BigFloat( &timeStamp.time, &timeStamp.time );
	BigFloat( &timeStamp.deltaTime, &timeStamp.deltaTime );
	timeStamp.memory = BigDWord( timeStamp.memory );
	timeStamp.deltaMemory = BigDWord( timeStamp.deltaMemory );

	// add to log
	TimeStampLog_AddItem( timeStamp.time, timeStamp.deltaTime, timeStamp.memory, timeStamp.deltaMemory, timeStamp.messageString );
	TimeStampLog_SortItems();

	// return the result
	retVal = 0;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = TimeStampLog( 0x%8.8x )\n", retVal, timeStampAddr );

	// success
	errCode = 0;

cleanUp:	
	return ( errCode );
}
