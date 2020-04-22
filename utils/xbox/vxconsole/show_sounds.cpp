//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SHOW_SOUNDS.CPP
//
//	Show Sounds Display.
//=====================================================================================//
#include "vxconsole.h"

#define ID_SHOWSOUNDS_LISTVIEW 100

// column id
#define ID_SS_NAME		0
#define ID_SS_PREFIX	1
#define ID_SS_FORMAT	2
#define ID_SS_RATE		3
#define ID_SS_BITS		4
#define ID_SS_CHANNELS	5
#define ID_SS_SIZE		6
#define ID_SS_STREAMED	7
#define ID_SS_LOOPED	8
#define ID_SS_LENGTH	9

typedef struct
{
	int			listIndex;
	char		*pName;
	char		*pPrefix;
	char		*pFormat;
	int			rate;
	char		rateBuff[16];
	int			bits;
	char		bitsBuff[16];
	int			channels;
	char		channelsBuff[16];
	int			numSamples;
	int			dataSize;
	char		dataSizeBuff[16];
	int			streamed;
	char		streamedBuff[16];
	int			looped;
	char		loopedBuff[16];
	float		length;
	char		lengthBuff[16];
} sound_t;

typedef struct
{	const CHAR*			name;
	int					width;
	int					subItemIndex;
	CHAR				nameBuff[32];
} label_t;

HWND		g_showSounds_hWnd;
HWND		g_showSounds_hWndListView;
RECT		g_showSounds_windowRect;
int			g_showSounds_sortColumn;
int			g_showSounds_sortDescending;
sound_t		*g_showSounds_pSounds;
int			g_showSounds_numSounds;
int			g_showSounds_currentFrame;

label_t g_showSounds_Labels[] =
{
	{"Name",		300,	ID_SS_NAME},
	{"Prefix",		80,		ID_SS_PREFIX},
	{"Format",		80,		ID_SS_FORMAT},
	{"Rate",		80,		ID_SS_RATE},
	{"Bits",		80,		ID_SS_BITS},
	{"Channels",	80,		ID_SS_CHANNELS},
	{"Size",		80,		ID_SS_SIZE},
	{"Streamed",	80,		ID_SS_STREAMED},
	{"Looped",		80,		ID_SS_LOOPED},
	{"Length",		80,		ID_SS_LENGTH},
};

//-----------------------------------------------------------------------------
//	ShowSounds_SaveConfig
// 
//-----------------------------------------------------------------------------
void ShowSounds_SaveConfig()
{
	char	buff[256];

	Sys_SetRegistryInteger( "showSoundsSortColumn", g_showSounds_sortColumn );
	Sys_SetRegistryInteger( "showSoundsSortDescending", g_showSounds_sortDescending );

	WINDOWPLACEMENT wp;
	memset( &wp, 0, sizeof( wp ) );
	wp.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( g_showSounds_hWnd, &wp );
	g_showSounds_windowRect = wp.rcNormalPosition;

	sprintf( buff, "%d %d %d %d", g_showSounds_windowRect.left, g_showSounds_windowRect.top, g_showSounds_windowRect.right, g_showSounds_windowRect.bottom );
	Sys_SetRegistryString( "showSoundsWindowRect", buff );
}

//-----------------------------------------------------------------------------
//	ShowSounds_LoadConfig	
// 
//-----------------------------------------------------------------------------
void ShowSounds_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	Sys_GetRegistryInteger( "showSoundsSortColumn", ID_SS_NAME, g_showSounds_sortColumn );
	Sys_GetRegistryInteger( "showSoundsSortDescending", false, g_showSounds_sortDescending );

	Sys_GetRegistryString( "showSoundsWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_showSounds_windowRect.left, &g_showSounds_windowRect.top, &g_showSounds_windowRect.right, &g_showSounds_windowRect.bottom );
	if ( numArgs != 4 || g_showSounds_windowRect.left < 0 || g_showSounds_windowRect.top < 0 || g_showSounds_windowRect.right < 0 || g_showSounds_windowRect.bottom < 0 )
		memset( &g_showSounds_windowRect, 0, sizeof( g_showSounds_windowRect ) );
}

//-----------------------------------------------------------------------------
//	ShowSounds_Clear
// 
//-----------------------------------------------------------------------------
void ShowSounds_Clear()
{
	// delete all the list view entries
	if ( g_showSounds_hWnd )
		ListView_DeleteAllItems( g_showSounds_hWndListView );

	if ( !g_showSounds_pSounds )
		return;

	for ( int i=0; i<g_showSounds_numSounds; i++ )
	{
		free( g_showSounds_pSounds[i].pName );
		free( g_showSounds_pSounds[i].pPrefix );
		free( g_showSounds_pSounds[i].pFormat );
	}

	g_showSounds_pSounds   = NULL;
	g_showSounds_numSounds = 0;
}

//-----------------------------------------------------------------------------
//	ShowSounds_Export
// 
//-----------------------------------------------------------------------------
void ShowSounds_Export()
{
}

//-----------------------------------------------------------------------------
//	ShowSounds_Summary
// 
//-----------------------------------------------------------------------------
void ShowSounds_Summary()
{
	char	buff[1024];

	// tally the totals
	int totalStreamed = 0;
	int totalStatic = 0;
	for ( int i=0; i<g_showSounds_numSounds; i++ )
	{
		if ( g_showSounds_pSounds[i].streamed )
		{
			totalStreamed += g_showSounds_pSounds[i].dataSize;
		}
		else
		{
			totalStatic += g_showSounds_pSounds[i].dataSize;
		}
	}

	sprintf( 
		buff, 
		"Entries:\t\t\t%d\n"
		"Static Memory:\t\t%.2f MB\n"
		"Streamed Memory:\t\t%.2f MB\n",
		g_showSounds_numSounds, 
		( float )totalStatic/( 1024.0F*1024.0F ),
		( float )totalStreamed/( 1024.0F*1024.0F ) );

	MessageBox( g_showSounds_hWnd, buff, "Sound Summary", MB_OK|MB_APPLMODAL );
}

//-----------------------------------------------------------------------------
//	ShowSounds_Play
// 
//-----------------------------------------------------------------------------
void ShowSounds_Play()
{
	char		command[256];
	sound_t*	pSound;
	int			selection;
	LVITEM		lvitem;

	if ( !g_connectedToApp )
		return;

	selection = ListView_GetSelectionMark( g_showSounds_hWndListView );
	if ( selection == -1 )
		return;

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask  = LVIF_PARAM;
	lvitem.iItem = selection;
	ListView_GetItem( g_showSounds_hWndListView, &lvitem );

	pSound = ( sound_t* )lvitem.lParam;

	sprintf( command, "play %s%s", pSound->pPrefix, pSound->pName );

	// send the command to application
	ProcessCommand( command );
}

//-----------------------------------------------------------------------------
//	ShowSounds_CompareFunc
// 
//-----------------------------------------------------------------------------
int CALLBACK ShowSounds_CompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	sound_t*	pSoundA = ( sound_t* )lParam1;
	sound_t*	pSoundB = ( sound_t* )lParam2;

	int sort = 0;
	switch ( g_showSounds_sortColumn )
	{
		case ID_SS_NAME:
			sort = stricmp( pSoundA->pName, pSoundB->pName );
			break;

		case ID_SS_PREFIX:
			sort = stricmp( pSoundA->pPrefix, pSoundB->pPrefix );
			break;

		case ID_SS_FORMAT:
			sort = stricmp( pSoundA->pFormat, pSoundB->pFormat );
			break;

		case ID_SS_RATE:
			sort = pSoundA->rate - pSoundB->rate;
			break;
		
		case ID_SS_BITS:
			sort = pSoundA->bits - pSoundB->bits;
			break;

		case ID_SS_CHANNELS:
			sort = stricmp( pSoundA->channelsBuff, pSoundB->channelsBuff );
			break;

		case ID_SS_SIZE:
			sort = pSoundA->dataSize - pSoundB->dataSize;
			break;

		case ID_SS_STREAMED:
			sort = stricmp( pSoundA->streamedBuff, pSoundB->streamedBuff );
			break;

		case ID_SS_LOOPED:
			sort = stricmp( pSoundA->loopedBuff, pSoundB->loopedBuff );
			break;

		case ID_SS_LENGTH:
			if ( pSoundA->length < pSoundB->length )
				sort = -1;
			else if ( pSoundA->length == pSoundB->length )
				sort = 0;
			else
				sort = 1;
			break;
	}

	// flip the sort order
	if ( g_showSounds_sortDescending )
		sort *= -1;

	return ( sort );
}

//-----------------------------------------------------------------------------
//	ShowSounds_SortItems
// 
//-----------------------------------------------------------------------------
void ShowSounds_SortItems()
{
	LVITEM		lvitem;
	sound_t		*pSound;
	int			i;

	if ( !g_showSounds_hWnd )
	{
		// only sort if window is visible
		return;
	}

	ListView_SortItems( g_showSounds_hWndListView, ShowSounds_CompareFunc, 0 );

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask = LVIF_PARAM;

	// get each item and reset its list index
	int itemCount = ListView_GetItemCount( g_showSounds_hWndListView );
	for ( i=0; i<itemCount; i++ )
	{
		lvitem.iItem = i;
		ListView_GetItem( g_showSounds_hWndListView, &lvitem );

		pSound = ( sound_t* )lvitem.lParam;
		pSound->listIndex = i;
	}

	// update list view columns with sort key
	for ( i=0; i<sizeof( g_showSounds_Labels )/sizeof( g_showSounds_Labels[0] ); i++ )
	{
		char		symbol;
		LVCOLUMN	lvc; 

		if ( i == g_showSounds_sortColumn )
			symbol = g_showSounds_sortDescending ? '<' : '>';
		else
			symbol = ' ';
		sprintf( g_showSounds_Labels[i].nameBuff, "%s %c", g_showSounds_Labels[i].name, symbol );

		memset( &lvc, 0, sizeof( lvc ) );
		lvc.mask    = LVCF_TEXT; 
		lvc.pszText = ( LPSTR )g_showSounds_Labels[i].nameBuff;

		ListView_SetColumn( g_showSounds_hWndListView, i, &lvc );
	}
}

//-----------------------------------------------------------------------------
//	ShowSounds_AddViewItem
// 
//-----------------------------------------------------------------------------
void ShowSounds_AddViewItem( sound_t* pSound )
{
	LVITEM	lvi;

	if ( !g_showSounds_hWnd )
	{
		// only valid if log window is visible
		return;
	}

	// update the text callback buffers
	if ( pSound->rate >= 0 )
		sprintf( pSound->rateBuff, "%5.2f KHz", ( float )pSound->rate/1000.0f );
	else
		strcpy( pSound->rateBuff, "???" );

	if ( pSound->bits >= 0 )
		sprintf( pSound->bitsBuff, "%d", pSound->bits );
	else
		strcpy( pSound->bitsBuff, "???" );

	if ( pSound->channels >= 1 )
		strcpy( pSound->channelsBuff, pSound->channels == 2 ? "Stereo" : "Mono" );
	else
		strcpy( pSound->channelsBuff, "???" );

	if ( pSound->dataSize >= 0 )
		sprintf( pSound->dataSizeBuff, "%d", pSound->dataSize );
	else
		strcpy( pSound->dataSizeBuff, "???" );

	if ( pSound->streamed >= 0 )
		strcpy( pSound->streamedBuff, pSound->streamed ? "Stream" : "Static" );
	else
		strcpy( pSound->streamedBuff, "???" );

	if ( pSound->looped >= 0 )
		strcpy( pSound->loopedBuff, pSound->looped ? "Looped" : "One-Shot" );
	else
		strcpy( pSound->loopedBuff, "???" );

	sprintf( pSound->lengthBuff, "%2.2d:%2.2d:%3.3d", ( int )pSound->length/60, ( int )pSound->length%60, ( int )( 1000*( pSound->length-( int )pSound->length ) )%1000 );

	int itemCount = ListView_GetItemCount( g_showSounds_hWndListView );

	// setup and insert at end of list
	memset( &lvi, 0, sizeof( lvi ) );
	lvi.mask      = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	lvi.iItem     = itemCount;
	lvi.iSubItem  = 0;
	lvi.state     = 0; 
	lvi.stateMask = 0;
	lvi.pszText   = LPSTR_TEXTCALLBACK;  
	lvi.lParam    = ( LPARAM )pSound;
								
	// insert and set the real index 
	pSound->listIndex = ListView_InsertItem( g_showSounds_hWndListView, &lvi );
}

//-----------------------------------------------------------------------------
//	ShowSounds_Refresh
// 
//-----------------------------------------------------------------------------
void ShowSounds_Refresh()
{
	char	command[256];

	strcpy( command, "vx_soundlist" );

	// send the command to application which replies with list data
	if ( g_connectedToApp )
		ProcessCommand( command );
}

//-----------------------------------------------------------------------------
//	ShowSounds_SizeWindow
// 
//-----------------------------------------------------------------------------
void ShowSounds_SizeWindow( HWND hwnd, int cx, int cy )
{
    if ( cx==0 || cy==0 )
    {
        RECT rcClient;
        GetClientRect( hwnd, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

	// position the ListView
	SetWindowPos( g_showSounds_hWndListView, NULL, 0, 0, cx, cy, SWP_NOZORDER );
}

//-----------------------------------------------------------------------------
//	ShowSounds_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK ShowSounds_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD		wID = LOWORD( wParam );
	sound_t*	pSound;

	switch ( message )
	{
		case WM_CREATE:
			return 0L;

		case WM_DESTROY:
			ShowSounds_SaveConfig();
			g_showSounds_hWnd = NULL;
			return 0L;
	
		case WM_INITMENU:
			return 0L;

		case WM_SIZE:
			ShowSounds_SizeWindow( hwnd, LOWORD( lParam ), HIWORD( lParam ) );
			return 0L;

		case WM_NOTIFY:
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_COLUMNCLICK:
					NMLISTVIEW*	pnmlv;
					pnmlv = ( NMLISTVIEW* )lParam; 
					if ( g_showSounds_sortColumn == pnmlv->iSubItem )
					{
						// user has clicked on same column - flip the sort
						g_showSounds_sortDescending ^= 1;
					}
					else
					{
						// sort by new column
						g_showSounds_sortColumn = pnmlv->iSubItem;
					}
					ShowSounds_SortItems();
					return 0L;

				case LVN_GETDISPINFO:
					NMLVDISPINFO* plvdi;
					plvdi    = ( NMLVDISPINFO* )lParam;
					pSound = ( sound_t* )plvdi->item.lParam;
					switch ( plvdi->item.iSubItem )
					{
						case ID_SS_NAME:
							plvdi->item.pszText = pSound->pName;
							return 0L;

						case ID_SS_PREFIX:
							plvdi->item.pszText = pSound->pPrefix;
							return 0L;
	  					
						case ID_SS_FORMAT:
							plvdi->item.pszText = pSound->pFormat;
							return 0L;

						case ID_SS_RATE:
							plvdi->item.pszText = pSound->rateBuff;
							return 0L;

						case ID_SS_BITS:
							plvdi->item.pszText = pSound->bitsBuff;
							return 0L;

						case ID_SS_CHANNELS:
							plvdi->item.pszText = pSound->channelsBuff;
							return 0L;

						case ID_SS_SIZE:
							plvdi->item.pszText = pSound->dataSizeBuff;
							return 0L;

						case ID_SS_STREAMED:
							plvdi->item.pszText = pSound->streamedBuff;
							return 0L;

						case ID_SS_LOOPED:
							plvdi->item.pszText = pSound->loopedBuff;
							return 0L;

						case ID_SS_LENGTH:
							plvdi->item.pszText = pSound->lengthBuff;
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
				case IDM_OPTIONS_SUMMARY:
					ShowSounds_Summary();
					return 0L;

				case IDM_OPTIONS_REFRESH:
					ShowSounds_Refresh();
					return 0L;

				case IDM_OPTIONS_EXPORT:
					ShowSounds_Export();
					return 0L;

				case IDM_OPTIONS_PLAYSOUND:
					ShowSounds_Play();
					return 0L;
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	ShowSounds_Init
// 
//-----------------------------------------------------------------------------
bool ShowSounds_Init()
{
	// set up our window class
	WNDCLASS wndclass;

	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = ShowSounds_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_SHOWSOUNDS );
	wndclass.lpszClassName = "SHOWSOUNDSCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	ShowSounds_LoadConfig();

	return true;
}

//-----------------------------------------------------------------------------
//	ShowSounds_Open	
// 
//-----------------------------------------------------------------------------
void ShowSounds_Open()
{
	RECT			clientRect;
	HWND			hWnd;
	int				i;

	if ( g_showSounds_hWnd )
	{
		// only one instance
		if ( IsIconic( g_showSounds_hWnd ) )
			ShowWindow( g_showSounds_hWnd, SW_RESTORE );
		SetForegroundWindow( g_showSounds_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"SHOWSOUNDSCLASS",
				"Sounds",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				700,
				400,
				g_hDlgMain,
				NULL,
				g_hInstance,
				NULL );
	g_showSounds_hWnd = hWnd;

	GetClientRect( g_showSounds_hWnd, &clientRect ); 
	hWnd = CreateWindow( 
				WC_LISTVIEW, 
				"", 
				WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL, 
				0, 
				0, 
				clientRect.right-clientRect.left,
				clientRect.bottom-clientRect.top, 
				g_showSounds_hWnd,
				( HMENU )ID_SHOWSOUNDS_LISTVIEW,
				g_hInstance,
				NULL ); 
	g_showSounds_hWndListView = hWnd;

	// init list view columns
	for ( i=0; i<sizeof( g_showSounds_Labels )/sizeof( g_showSounds_Labels[0] ); i++ )
	{
		LVCOLUMN lvc; 
		memset( &lvc, 0, sizeof( lvc ) );

		lvc.mask     = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
		lvc.iSubItem = 0;
		lvc.cx       = g_showSounds_Labels[i].width;
		lvc.fmt      = LVCFMT_LEFT;
		lvc.pszText  = ( LPSTR )g_showSounds_Labels[i].name;

		ListView_InsertColumn( g_showSounds_hWndListView, i, &lvc );
	}

	ListView_SetBkColor( g_showSounds_hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( g_showSounds_hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx( g_showSounds_hWndListView, style, style );

	// populate list view
	for ( i=0; i<g_showSounds_numSounds; i++ )
		ShowSounds_AddViewItem( &g_showSounds_pSounds[i] );
	ShowSounds_SortItems();

	if ( g_showSounds_windowRect.right && g_showSounds_windowRect.bottom )
		MoveWindow( g_showSounds_hWnd, g_showSounds_windowRect.left, g_showSounds_windowRect.top, g_showSounds_windowRect.right-g_showSounds_windowRect.left, g_showSounds_windowRect.bottom-g_showSounds_windowRect.top, FALSE );
	ShowWindow( g_showSounds_hWnd, SHOW_OPENWINDOW );

	// get data from application
	ShowSounds_Refresh();
}

//-----------------------------------------------------------------------------
// rc_SoundList
//
// Sent from application with sound list
//-----------------------------------------------------------------------------
int rc_SoundList( char* commandPtr )
{
	char*			cmdToken;
	int				numSounds;
	int				soundList;
	int				retAddr;
	int				retVal;
	int				errCode = -1;
	xrSound_t*		pLocalList;
	int				prefixLen;
	char			*pStr;

	// remove old entries
	ShowSounds_Clear();

	// get number of sounds
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &numSounds );

	// get sound list
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &soundList );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &retAddr );

	pLocalList = new xrSound_t[numSounds];
	memset( pLocalList, 0, numSounds*sizeof( xrSound_t ) );

	g_showSounds_numSounds = numSounds;
	g_showSounds_pSounds   = new sound_t[numSounds];
	memset( g_showSounds_pSounds, 0, numSounds*sizeof( sound_t ) );

	// get the caller's command list 
	DmGetMemory( ( void* )soundList, numSounds*sizeof( xrSound_t ), pLocalList, NULL );

	// build out the resident list
	for ( int i=0; i<numSounds; i++ )
	{
		// swap the structure
		pLocalList[i].rate = BigDWord( pLocalList[i].rate );
		pLocalList[i].bits = BigDWord( pLocalList[i].bits );
		pLocalList[i].channels = BigDWord( pLocalList[i].channels );
		pLocalList[i].looped = BigDWord( pLocalList[i].looped );
		pLocalList[i].dataSize = BigDWord( pLocalList[i].dataSize );
		pLocalList[i].numSamples = BigDWord( pLocalList[i].numSamples );
		pLocalList[i].streamed = BigDWord( pLocalList[i].streamed );

		// strip the prefix
		pStr = pLocalList[i].nameString;
		while ( *pStr )
		{
			if ( __iscsym( *pStr ) )
			{
				// first non-preifx character
				break;
			}
			pStr++;
		}
		g_showSounds_pSounds[i].pName = strdup( pStr );

		char prefixString[256];
		prefixLen = pStr - pLocalList[i].nameString;
		memcpy( prefixString, pLocalList[i].nameString, prefixLen );
		prefixString[prefixLen] = '\0';
		g_showSounds_pSounds[i].pPrefix = strdup( prefixString );

		// get the format name
		g_showSounds_pSounds[i].pFormat = strdup( pLocalList[i].formatString );

		g_showSounds_pSounds[i].rate       = pLocalList[i].rate;
		g_showSounds_pSounds[i].bits       = pLocalList[i].bits;
		g_showSounds_pSounds[i].channels   = pLocalList[i].channels;
		g_showSounds_pSounds[i].dataSize   = pLocalList[i].dataSize;
		g_showSounds_pSounds[i].numSamples = pLocalList[i].numSamples;
		g_showSounds_pSounds[i].streamed   = pLocalList[i].streamed;
		g_showSounds_pSounds[i].looped     = pLocalList[i].looped;

		// determine duration
		// must use sample count due to compression
		if ( g_showSounds_pSounds[i].rate > 0 )
			g_showSounds_pSounds[i].length = ( float )g_showSounds_pSounds[i].numSamples/( float )g_showSounds_pSounds[i].rate;
		else
			g_showSounds_pSounds[i].length = 0;

		// add to list view
		ShowSounds_AddViewItem( &g_showSounds_pSounds[i] );
	}

	// return the result
	retVal = numSounds;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = SoundList( 0x%8.8x, 0x%8.8x )\n", retVal, numSounds, soundList );

	delete [] pLocalList;

	// update
	ShowSounds_SortItems();

	// success
	errCode = 0;

cleanUp:	
	return ( errCode );
}
