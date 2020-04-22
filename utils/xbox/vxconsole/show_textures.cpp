//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SHOW_TEXTURES.CPP
//
//	Show Textures Display.
//=====================================================================================//
#include "vxconsole.h"

#define ID_SHOWTEXTURES_LISTVIEW 100

// column id
#define ID_ST_NAME			0
#define ID_ST_SIZE			1
#define ID_ST_GROUP			2
#define ID_ST_FORMAT		3
#define ID_ST_WIDTH			4
#define ID_ST_HEIGHT		5
#define ID_ST_DEPTH			6
#define ID_ST_NUMLEVELS		7
#define ID_ST_BINDS			8
#define ID_ST_REFCOUNT		9
#define ID_ST_LOAD			10

typedef enum
{
	LS_STATIC,			// surface
	LS_PROCEDURAL,		// lacks disk based bits
	LS_SYNCHRONOUS,		// loaded synchronously
	LS_FALLBACK,		// fallback version, queued hires
	LS_HIRES,			// finalized at hires
	LS_FAILED,			// failed to load
	LS_MAX
} loadState_e;

typedef struct
{
	int			listIndex;
	char		*pLongName;
	char		*pShortName;
	char		*pGroupName;
	char		*pFormatName;
	int			size;
	char		sizeBuff[16];
	int			width;
	char		widthBuff[16];
	int			height;
	char		heightBuff[16];
	int			depth;
	char		depthBuff[16];
	int			numLevels;
	char		numLevelsBuff[16];
	int			binds;
	char		bindsBuff[16];
	int			refCount;
	char		refCountBuff[16];
	int			loadState;
	int			edram;
} texture_t;

typedef struct
{	
	const CHAR*			name;
	int					width;
	int					subItemIndex;
	CHAR				nameBuff[32];
} label_t;

HWND		g_showTextures_hWnd;
HWND		g_showTextures_hWndListView;
RECT		g_showTextures_windowRect;
int			g_showTextures_sortColumn;
int			g_showTextures_sortDescending;
texture_t	*g_showTextures_pTextures;
int			g_showTextures_numTextures;
int			g_showTextures_currentFrame;
int			g_showTextures_fullPath;
char		g_showTextures_drawTextureName[MAX_PATH];

char *g_showTextures_loadStrings[LS_MAX] =
{
	"Static",
	"Procedural",
	"Synchronous",
	"Fallback",
	"",
	"Failed",
};

label_t g_showTextures_Labels[] =
{
	{"Name",		150,	ID_ST_NAME},
	{"D3D Size",	80,		ID_ST_SIZE},
	{"Group",		150,	ID_ST_GROUP},
	{"Format",		120,	ID_ST_FORMAT},
	{"Width",		80,		ID_ST_WIDTH},
	{"Height",		80,		ID_ST_HEIGHT},
	{"Depth",		80,		ID_ST_DEPTH},
	{"NumLevels",	80,		ID_ST_NUMLEVELS},
	{"Binds",		80,		ID_ST_BINDS},
	{"RefCount",	80,		ID_ST_REFCOUNT},
	{"Load State",	120,	ID_ST_LOAD},
};

//-----------------------------------------------------------------------------
//	ShowTextures_SaveConfig
// 
//-----------------------------------------------------------------------------
void ShowTextures_SaveConfig()
{
	char	buff[256];

	Sys_SetRegistryInteger( "showTexturesFullPath", g_showTextures_fullPath );
	Sys_SetRegistryInteger( "showTexturesCurrentFrame", g_showTextures_currentFrame );
	Sys_SetRegistryInteger( "showTexturesSortColumn", g_showTextures_sortColumn );
	Sys_SetRegistryInteger( "showTexturesSortDescending", g_showTextures_sortDescending );

	WINDOWPLACEMENT wp;
	memset( &wp, 0, sizeof( wp ) );
	wp.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( g_showTextures_hWnd, &wp );
	g_showTextures_windowRect = wp.rcNormalPosition;

	sprintf( buff, "%d %d %d %d", g_showTextures_windowRect.left, g_showTextures_windowRect.top, g_showTextures_windowRect.right, g_showTextures_windowRect.bottom );
	Sys_SetRegistryString( "showTexturesWindowRect", buff );
}

//-----------------------------------------------------------------------------
//	ShowTextures_LoadConfig	
// 
//-----------------------------------------------------------------------------
void ShowTextures_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	Sys_GetRegistryInteger( "showTexturesFullPath", true, g_showTextures_fullPath );
	Sys_GetRegistryInteger( "showTexturesCurrentFrame", false, g_showTextures_currentFrame );
	Sys_GetRegistryInteger( "showTexturesSortColumn", ID_ST_NAME, g_showTextures_sortColumn );
	Sys_GetRegistryInteger( "showTexturesSortDescending", false, g_showTextures_sortDescending );

	Sys_GetRegistryString( "showTexturesWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_showTextures_windowRect.left, &g_showTextures_windowRect.top, &g_showTextures_windowRect.right, &g_showTextures_windowRect.bottom );
	if ( numArgs != 4 || g_showTextures_windowRect.left < 0 || g_showTextures_windowRect.top < 0 || g_showTextures_windowRect.right < 0 || g_showTextures_windowRect.bottom < 0 )
	{
		memset( &g_showTextures_windowRect, 0, sizeof( g_showTextures_windowRect ) );
	}
}

//-----------------------------------------------------------------------------
//	ShowTextures_Clear
// 
//-----------------------------------------------------------------------------
void ShowTextures_Clear()
{
	// delete all the list view entries
	if ( g_showTextures_hWnd )
	{
		ListView_DeleteAllItems( g_showTextures_hWndListView );
	}

	if ( !g_showTextures_pTextures )
	{
		return;
	}

	for ( int i=0; i<g_showTextures_numTextures; i++ )
	{
		free( g_showTextures_pTextures[i].pLongName );
		free( g_showTextures_pTextures[i].pShortName );
		free( g_showTextures_pTextures[i].pGroupName );
		free( g_showTextures_pTextures[i].pFormatName );
	}

	g_showTextures_pTextures = NULL;
	g_showTextures_numTextures = 0;
}

//-----------------------------------------------------------------------------
//	ShowTextures_Export
// 
//-----------------------------------------------------------------------------
void ShowTextures_Export()
{
	OPENFILENAME		ofn;
	char				logFilename[MAX_PATH]; 
	int					retval;
	FILE*				fp;
	int					i;
	int					count;

	memset( &ofn, 0, sizeof( ofn ) );
	ofn.lStructSize     = sizeof( ofn );
	ofn.hwndOwner       = g_showTextures_hWnd;
	ofn.lpstrFile       = logFilename;
	ofn.lpstrFile[0]    = '\0';
	ofn.nMaxFile        = sizeof( logFilename );
	ofn.lpstrFilter     = "Excel CSV\0*.CSV\0All Files\0*.*\0";
	ofn.nFilterIndex    = 1;
	ofn.lpstrFileTitle  = NULL;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = "c:\\";
	ofn.Flags           = OFN_PATHMUSTEXIST;

	// display the Open dialog box. 
	retval = GetOpenFileName( &ofn ); 
	if ( !retval )
	{
		return;
	}

	Sys_AddExtension( ".csv", logFilename, sizeof( logFilename ) );

	fp = fopen( logFilename, "wt+" );
	if ( !fp )
	{
		return;
	}

	// labels
	count = sizeof( g_showTextures_Labels )/sizeof( g_showTextures_Labels[0] );
	for ( i=0; i<count; i++ )
	{
		fprintf( fp, "\"%s\"", g_showTextures_Labels[i].name );
		if ( i != count-1 )
		{
			fprintf( fp, "," );
		}
	}
	fprintf( fp, "\n" );

	// dump to the log
	for ( i=0; i<g_showTextures_numTextures; i++ )
	{
		if ( g_showTextures_fullPath )
		{
			fprintf( fp, "\"%s\"", g_showTextures_pTextures[i].pLongName );
		}
		else
		{
			fprintf( fp, "\"%s\"", g_showTextures_pTextures[i].pShortName );
		}
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].sizeBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].pGroupName );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].pFormatName );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].widthBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].heightBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].depthBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].numLevelsBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].bindsBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_pTextures[i].refCountBuff );
		fprintf( fp, ",\"%s\"", g_showTextures_loadStrings[g_showTextures_pTextures[i].loadState] );
		fprintf( fp, "\n" );
	}

	fclose( fp );
}

//-----------------------------------------------------------------------------
//	ShowTextures_Summary
// 
//-----------------------------------------------------------------------------
void ShowTextures_Summary()
{
	char	buff[1024];

	// tally the totals
	int total = 0;
	for ( int i=0; i<g_showTextures_numTextures; i++ )
	{
		if ( g_showTextures_pTextures[i].edram )
		{
			// edram is does not affect system memory total
			continue;
		}
		total += g_showTextures_pTextures[i].size;
	}

	sprintf( 
		buff, 
		"Entries:\t\t\t%d\n"
		"System D3D Memory:\t%.2f MB\n",
		g_showTextures_numTextures, 
		( float )total/( 1024.0F*1024.0F ) );

	MessageBox( g_showTextures_hWnd, buff, "Texture Summary", MB_OK|MB_APPLMODAL );
}

//-----------------------------------------------------------------------------
//	ShowTextures_DrawTexture
// 
//-----------------------------------------------------------------------------
void ShowTextures_DrawTexture()
{
	char		command[256];
	texture_t*	pTexture;
	int			selection;
	LVITEM		lvitem;

	if ( !g_connectedToApp )
		return;

	selection = ListView_GetSelectionMark( g_showTextures_hWndListView );
	if ( selection == -1 )
		return;

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask  = LVIF_PARAM;
	lvitem.iItem = selection;
	ListView_GetItem( g_showTextures_hWndListView, &lvitem );

	pTexture = ( texture_t* )lvitem.lParam;

	if ( !V_stricmp( g_showTextures_drawTextureName, pTexture->pLongName ) )
	{
		// hide
		sprintf( command, "mat_drawTexture \"\"" );
		g_showTextures_drawTextureName[0] = '\0';
	}
	else
	{
		// show
		sprintf( command, "mat_drawTexture %s", pTexture->pLongName );
		V_strncpy( g_showTextures_drawTextureName, pTexture->pLongName, sizeof( g_showTextures_drawTextureName ) );
	}

	// send the command to application
	ProcessCommand( command );
}

//-----------------------------------------------------------------------------
//	ShowTextures_SetTitle
// 
//-----------------------------------------------------------------------------
void ShowTextures_SetTitle()
{
	char	titleBuff[128];

	if ( g_showTextures_hWnd )
	{
		strcpy( titleBuff, "Textures " );
		if ( g_showTextures_currentFrame )
		{
			strcat( titleBuff, " [FRAME]" );
		}
		if ( g_showTextures_fullPath )
		{
			strcat( titleBuff, " [FULL PATH]" );
		}

		SetWindowText( g_showTextures_hWnd, titleBuff );
	}
}

//-----------------------------------------------------------------------------
//	ShowTextures_CompareFunc
// 
//-----------------------------------------------------------------------------
int CALLBACK ShowTextures_CompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	texture_t*	pTextureA = ( texture_t* )lParam1;
	texture_t*	pTextureB = ( texture_t* )lParam2;

	int sort = 0;
	switch ( g_showTextures_sortColumn )
	{
		case ID_ST_NAME:
			if ( g_showTextures_fullPath )
			{
				sort = stricmp( pTextureA->pLongName, pTextureB->pLongName );
			}
			else
			{
				sort = stricmp( pTextureA->pShortName, pTextureB->pShortName );
			}
			break;

		case ID_ST_GROUP:
			sort = stricmp( pTextureA->pGroupName, pTextureB->pGroupName );
			break;

		case ID_ST_FORMAT:
			sort = stricmp( pTextureA->pFormatName, pTextureB->pFormatName );
			break;

		case ID_ST_SIZE:
			sort = pTextureA->size - pTextureB->size;
			break;

		case ID_ST_WIDTH:
			sort = pTextureA->width - pTextureB->width;
			break;

		case ID_ST_HEIGHT:
			sort = pTextureA->height - pTextureB->height;
			break;

		case ID_ST_DEPTH:
			sort = pTextureA->depth - pTextureB->depth;
			break;

		case ID_ST_NUMLEVELS:
			sort = pTextureA->numLevels - pTextureB->numLevels;
			break;

		case ID_ST_BINDS:
			sort = pTextureA->binds - pTextureB->binds;
			break;

		case ID_ST_REFCOUNT:
			sort = pTextureA->refCount - pTextureB->refCount;
			break;

		case ID_ST_LOAD:
			sort = stricmp( g_showTextures_loadStrings[pTextureA->loadState], g_showTextures_loadStrings[pTextureB->loadState] );
			break;
	}

	// flip the sort order
	if ( g_showTextures_sortDescending )
	{
		sort *= -1;
	}

	return ( sort );
}

//-----------------------------------------------------------------------------
//	ShowTextures_SortItems
// 
//-----------------------------------------------------------------------------
void ShowTextures_SortItems()
{
	LVITEM		lvitem;
	texture_t	*pTexture;
	int			i;

	if ( !g_showTextures_hWnd )
	{
		// only sort if window is visible
		return;
	}

	ListView_SortItems( g_showTextures_hWndListView, ShowTextures_CompareFunc, 0 );

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask = LVIF_PARAM;

	// get each item and reset its list index
	int itemCount = ListView_GetItemCount( g_showTextures_hWndListView );
	for ( i=0; i<itemCount; i++ )
	{
		lvitem.iItem = i;
		ListView_GetItem( g_showTextures_hWndListView, &lvitem );

		pTexture = ( texture_t* )lvitem.lParam;
		pTexture->listIndex = i;
	}

	// update list view columns with sort key
	for ( i=0; i<sizeof( g_showTextures_Labels )/sizeof( g_showTextures_Labels[0] ); i++ )
	{
		char		symbol;
		LVCOLUMN	lvc; 

		if ( i == g_showTextures_sortColumn )
		{
			symbol = g_showTextures_sortDescending ? '<' : '>';
		}
		else
		{
			symbol = ' ';
		}
		sprintf( g_showTextures_Labels[i].nameBuff, "%s %c", g_showTextures_Labels[i].name, symbol );

		memset( &lvc, 0, sizeof( lvc ) );
		lvc.mask    = LVCF_TEXT; 
		lvc.pszText = ( LPSTR )g_showTextures_Labels[i].nameBuff;

		ListView_SetColumn( g_showTextures_hWndListView, i, &lvc );
	}
}

//-----------------------------------------------------------------------------
//	ShowTextures_AddViewItem
// 
//-----------------------------------------------------------------------------
void ShowTextures_AddViewItem( texture_t* pTexture )
{
	LVITEM	lvi;

	if ( !g_showTextures_hWnd )
	{
		// only valid if log window is visible
		return;
	}

	// update the text callback buffers
	sprintf( pTexture->sizeBuff, "%d", pTexture->size );
	sprintf( pTexture->widthBuff, "%d", pTexture->width );
	sprintf( pTexture->heightBuff, "%d", pTexture->height );
	sprintf( pTexture->depthBuff, "%d", pTexture->depth );
	sprintf( pTexture->numLevelsBuff, "%d", pTexture->numLevels );
	sprintf( pTexture->bindsBuff, "%d", pTexture->binds );
	sprintf( pTexture->refCountBuff, "%d", pTexture->refCount );

	int itemCount = ListView_GetItemCount( g_showTextures_hWndListView );

	// setup and insert at end of list
	memset( &lvi, 0, sizeof( lvi ) );
	lvi.mask      = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	lvi.iItem     = itemCount;
	lvi.iSubItem  = 0;
	lvi.state     = 0; 
	lvi.stateMask = 0;
	lvi.pszText   = LPSTR_TEXTCALLBACK;  
	lvi.lParam    = ( LPARAM )pTexture;
								
	// insert and set the real index 
	pTexture->listIndex = ListView_InsertItem( g_showTextures_hWndListView, &lvi );
}

//-----------------------------------------------------------------------------
//	ShowTextures_Refresh
// 
//-----------------------------------------------------------------------------
void ShowTextures_Refresh()
{
	char	command[256];

	strcpy( command, "mat_get_textures" );

	if ( !g_showTextures_currentFrame )
	{
		strcat( command, " all" );
	}

	// send the command to application which replies with list data
	if ( g_connectedToApp )
	{
		ProcessCommand( command );
	}
}

//-----------------------------------------------------------------------------
//	ShowTextures_SizeWindow
// 
//-----------------------------------------------------------------------------
void ShowTextures_SizeWindow( HWND hwnd, int cx, int cy )
{
    if ( cx==0 || cy==0 )
    {
        RECT rcClient;
        GetClientRect( hwnd, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

	// position the ListView
	SetWindowPos( g_showTextures_hWndListView, NULL, 0, 0, cx, cy, SWP_NOZORDER );
}

//-----------------------------------------------------------------------------
//	ShowTextures_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK ShowTextures_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD		wID = LOWORD( wParam );
	texture_t*	pTexture;

	switch ( message )
	{
		case WM_CREATE:
			return 0L;

		case WM_DESTROY:
			ShowTextures_SaveConfig();
			g_showTextures_hWnd = NULL;
			return 0L;
	
		case WM_INITMENU:
            CheckMenuItem( ( HMENU )wParam, IDM_OPTIONS_CURRENTFRAME, MF_BYCOMMAND | ( g_showTextures_currentFrame ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( ( HMENU )wParam, IDM_OPTIONS_FULLPATH, MF_BYCOMMAND | ( g_showTextures_fullPath ? MF_CHECKED : MF_UNCHECKED ) );
			return 0L;

		case WM_SIZE:
			ShowTextures_SizeWindow( hwnd, LOWORD( lParam ), HIWORD( lParam ) );
			return 0L;

		case WM_NOTIFY:
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_COLUMNCLICK:
					NMLISTVIEW*	pnmlv;
					pnmlv = ( NMLISTVIEW* )lParam; 
					if ( g_showTextures_sortColumn == pnmlv->iSubItem )
					{
						// user has clicked on same column - flip the sort
						g_showTextures_sortDescending ^= 1;
					}
					else
					{
						// sort by new column
						g_showTextures_sortColumn = pnmlv->iSubItem;
					}
					ShowTextures_SortItems();
					return 0L;

				case LVN_GETDISPINFO:
					NMLVDISPINFO* plvdi;
					plvdi = ( NMLVDISPINFO* )lParam;
					pTexture = ( texture_t* )plvdi->item.lParam;
					switch ( plvdi->item.iSubItem )
					{
						case ID_ST_NAME:
							if ( g_showTextures_fullPath )
								plvdi->item.pszText = pTexture->pLongName;
							else
								plvdi->item.pszText = pTexture->pShortName;
							return 0L;
	  					
						case ID_ST_GROUP:
							plvdi->item.pszText = pTexture->pGroupName;
							return 0L;

						case ID_ST_FORMAT:
							plvdi->item.pszText = pTexture->pFormatName;
							return 0L;

						case ID_ST_SIZE:
							plvdi->item.pszText = pTexture->sizeBuff;
							return 0L;

						case ID_ST_WIDTH:
							plvdi->item.pszText = pTexture->widthBuff;
							return 0L;

						case ID_ST_HEIGHT:
							plvdi->item.pszText = pTexture->heightBuff;
							return 0L;

						case ID_ST_DEPTH:
							plvdi->item.pszText = pTexture->depthBuff;
							return 0L;

						case ID_ST_NUMLEVELS:
							plvdi->item.pszText = pTexture->numLevelsBuff;
							return 0L;

						case ID_ST_BINDS:
							plvdi->item.pszText = pTexture->bindsBuff;
							return 0L;

						case ID_ST_LOAD:
							plvdi->item.pszText = g_showTextures_loadStrings[pTexture->loadState];
							return 0L;

						case ID_ST_REFCOUNT:
							plvdi->item.pszText = pTexture->refCountBuff;
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
					ShowTextures_Summary();
					return 0L;

				case IDM_OPTIONS_REFRESH:
					ShowTextures_Refresh();
					return 0L;

				case IDM_OPTIONS_EXPORT:
					ShowTextures_Export();
					return 0L;

				case IDM_OPTIONS_CURRENTFRAME:
					g_showTextures_currentFrame ^= 1;
					ShowTextures_SetTitle();
					ShowTextures_Refresh();
					return 0L;

				case IDM_OPTIONS_FULLPATH:
					g_showTextures_fullPath ^= 1;
					ShowTextures_SetTitle();
					ShowTextures_SortItems();
					return 0L;

				case IDM_OPTIONS_DRAWTEXTURE:
					ShowTextures_DrawTexture();
					return 0L;
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	ShowTextures_Init
// 
//-----------------------------------------------------------------------------
bool ShowTextures_Init()
{
	// set up our window class
	WNDCLASS wndclass;

	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style = 0;
	wndclass.lpfnWndProc = ShowTextures_WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = g_hInstance;
	wndclass.hIcon = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_SHOWTEXTURES );
	wndclass.lpszClassName = "SHOWTEXTURESCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	ShowTextures_LoadConfig();

	return true;
}

//-----------------------------------------------------------------------------
//	ShowTextures_Open	
// 
//-----------------------------------------------------------------------------
void ShowTextures_Open()
{
	RECT			clientRect;
	HWND			hWnd;
	int				i;

	if ( g_showTextures_hWnd )
	{
		// only one instance
		if ( IsIconic( g_showTextures_hWnd ) )
		{
			ShowWindow( g_showTextures_hWnd, SW_RESTORE );
		}
		SetForegroundWindow( g_showTextures_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"SHOWTEXTURESCLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				700,
				400,
				g_hDlgMain,
				NULL,
				g_hInstance,
				NULL );
	g_showTextures_hWnd = hWnd;

	GetClientRect( g_showTextures_hWnd, &clientRect ); 
	hWnd = CreateWindow( 
				WC_LISTVIEW, 
				"", 
				WS_VISIBLE|WS_CHILD|LVS_REPORT, 
				0, 
				0, 
				clientRect.right-clientRect.left,
				clientRect.bottom-clientRect.top, 
				g_showTextures_hWnd,
				( HMENU )ID_SHOWTEXTURES_LISTVIEW,
				g_hInstance,
				NULL ); 
	g_showTextures_hWndListView = hWnd;

	// init list view columns
	for ( i=0; i<sizeof( g_showTextures_Labels )/sizeof( g_showTextures_Labels[0] ); i++ )
	{
		LVCOLUMN lvc; 
		memset( &lvc, 0, sizeof( lvc ) );

		lvc.mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
		lvc.iSubItem = 0;
		lvc.cx = g_showTextures_Labels[i].width;
		lvc.fmt = LVCFMT_LEFT;
		lvc.pszText  = ( LPSTR )g_showTextures_Labels[i].name;
		ListView_InsertColumn( g_showTextures_hWndListView, i, &lvc );
	}

	ListView_SetBkColor( g_showTextures_hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( g_showTextures_hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx( g_showTextures_hWndListView, style, style );

	// populate list view
	for ( i=0; i<g_showTextures_numTextures; i++ )
	{
		ShowTextures_AddViewItem( &g_showTextures_pTextures[i] );
	}
	ShowTextures_SortItems();

	ShowTextures_SetTitle();

	if ( g_showTextures_windowRect.right && g_showTextures_windowRect.bottom )
	{
		MoveWindow( g_showTextures_hWnd, g_showTextures_windowRect.left, g_showTextures_windowRect.top, g_showTextures_windowRect.right-g_showTextures_windowRect.left, g_showTextures_windowRect.bottom-g_showTextures_windowRect.top, FALSE );
	}
	ShowWindow( g_showTextures_hWnd, SHOW_OPENWINDOW );

	// get data from application
	ShowTextures_Refresh();
}

//-----------------------------------------------------------------------------
// rc_TextureList
//
// Sent from application with texture list
//-----------------------------------------------------------------------------
int rc_TextureList( char* commandPtr )
{
	int				errCode = -1;
	char*			cmdToken;
	int				numTextures;
	int				textureList;
	int				retAddr;
	int				retVal;
	xrTexture_t*	pLocalList;

	// remove old entries
	ShowTextures_Clear();

	// get number of textures
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &numTextures );

	// get texture list
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &textureList );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &retAddr );

	pLocalList = new xrTexture_t[numTextures];
	memset( pLocalList, 0, numTextures*sizeof( xrTexture_t ) );

	g_showTextures_numTextures = numTextures;
	g_showTextures_pTextures   = new texture_t[numTextures];
	memset( g_showTextures_pTextures, 0, numTextures*sizeof( texture_t ) );

	// get the caller's command list 
	DmGetMemory( ( void* )textureList, numTextures*sizeof( xrTexture_t ), pLocalList, NULL );

	// build out the resident list
	for ( int i=0; i<numTextures; i++ )
	{
		// swap the structure
		pLocalList[i].size = BigDWord( pLocalList[i].size );
		pLocalList[i].width = BigDWord( pLocalList[i].width );
		pLocalList[i].height = BigDWord( pLocalList[i].height );
		pLocalList[i].depth = BigDWord( pLocalList[i].depth );
		pLocalList[i].numLevels = BigDWord( pLocalList[i].numLevels );
		pLocalList[i].binds = BigDWord( pLocalList[i].binds );
		pLocalList[i].refCount = BigDWord( pLocalList[i].refCount );
		pLocalList[i].sRGB = BigDWord( pLocalList[i].sRGB );
		pLocalList[i].edram = BigDWord( pLocalList[i].edram );
		pLocalList[i].procedural = BigDWord( pLocalList[i].procedural );
		pLocalList[i].fallback = BigDWord( pLocalList[i].fallback );
		pLocalList[i].final = BigDWord( pLocalList[i].final );
		pLocalList[i].failed = BigDWord( pLocalList[i].failed );

		// get the name
		g_showTextures_pTextures[i].pLongName = strdup( pLocalList[i].nameString );

		CHAR shortName[MAX_PATH];
		Sys_StripPath( g_showTextures_pTextures[i].pLongName, shortName, sizeof( shortName ) );
		g_showTextures_pTextures[i].pShortName = strdup( shortName );

		// get the group name
		g_showTextures_pTextures[i].pGroupName = strdup( pLocalList[i].groupString );

		// get the format name
		const char *pFormatName = pLocalList[i].formatString;
		if ( !strnicmp( pFormatName, "D3DFMT_", 7 ) )
		{
			// strip D3DFMT_
			pFormatName += 7;
		}
		char tempName[MAX_PATH];
		if ( pLocalList[i].sRGB )
		{
			strcpy( tempName, pFormatName );
			strcat( tempName, " (SRGB)" );
			pFormatName = tempName;
		}
		g_showTextures_pTextures[i].pFormatName = strdup( pFormatName );

		g_showTextures_pTextures[i].size = pLocalList[i].size;
		g_showTextures_pTextures[i].width = pLocalList[i].width;
		g_showTextures_pTextures[i].height = pLocalList[i].height;
		g_showTextures_pTextures[i].depth = pLocalList[i].depth;
		g_showTextures_pTextures[i].numLevels = pLocalList[i].numLevels;
		g_showTextures_pTextures[i].binds = pLocalList[i].binds;
		g_showTextures_pTextures[i].refCount = pLocalList[i].refCount;
		g_showTextures_pTextures[i].edram = pLocalList[i].edram;

		loadState_e loadState;
		if ( pLocalList[i].edram || V_stristr( g_showTextures_pTextures[i].pGroupName, "RenderTarget" ) )
		{
			loadState = LS_STATIC;
		}
		else if ( pLocalList[i].procedural )
		{
			loadState = LS_PROCEDURAL;
		}
		else if ( pLocalList[i].final )
		{
			loadState = LS_HIRES;
		}
		else if ( pLocalList[i].failed )
		{
			loadState = LS_FAILED;
		}
		else if ( pLocalList[i].fallback )
		{
			loadState = LS_FALLBACK;
		}
		else
		{
			loadState = LS_SYNCHRONOUS;
		}
		g_showTextures_pTextures[i].loadState = loadState;

		// add to list view
		ShowTextures_AddViewItem( &g_showTextures_pTextures[i] );
	}

	// return the result
	retVal = numTextures;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = TextureList( 0x%8.8x, 0x%8.8x )\n", retVal, numTextures, textureList );

	delete [] pLocalList;

	// update
	ShowTextures_SortItems();

	// success
	errCode = 0;

cleanUp:	
	return errCode;
}
