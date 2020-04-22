//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SHOW_MATERIALS.CPP
//
//	Show Materials Display.
//=====================================================================================//
#include "vxconsole.h"

#define ID_SHOWMATERIALS_LISTVIEW 100

// column id
#define ID_SM_NAME			0
#define ID_SM_SHADER		1
#define ID_SM_REFCOUNT		2

typedef struct
{
	int			listIndex;
	char		*pName;
	char		*pShaderName;
	int			refCount;
	char		refCountBuff[16];
} material_t;

typedef struct
{	const CHAR*			name;
	int					width;
	int					subItemIndex;
	CHAR				nameBuff[32];
} label_t;

HWND		g_showMaterials_hWnd;
HWND		g_showMaterials_hWndListView;
RECT		g_showMaterials_windowRect;
int			g_showMaterials_sortColumn;
int			g_showMaterials_sortDescending;
material_t	*g_showMaterials_pMaterials;
int			g_showMaterials_numMaterials;
int			g_showMaterials_currentFrame;

label_t g_showMaterials_Labels[] =
{
	{"Name",		300,	ID_SM_NAME},
	{"Shader",		150,	ID_SM_SHADER},
	{"RefCount",	80,		ID_SM_REFCOUNT},
};

//-----------------------------------------------------------------------------
//	ShowMaterials_SaveConfig
// 
//-----------------------------------------------------------------------------
void ShowMaterials_SaveConfig()
{
	char	buff[256];

	Sys_SetRegistryInteger( "showMaterialsCurrentFrame", g_showMaterials_currentFrame );
	Sys_SetRegistryInteger( "showMaterialsSortColumn", g_showMaterials_sortColumn );
	Sys_SetRegistryInteger( "showMaterialsSortDescending", g_showMaterials_sortDescending );

	WINDOWPLACEMENT wp;
	memset( &wp, 0, sizeof( wp ) );
	wp.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( g_showMaterials_hWnd, &wp );
	g_showMaterials_windowRect = wp.rcNormalPosition;

	sprintf( buff, "%d %d %d %d", g_showMaterials_windowRect.left, g_showMaterials_windowRect.top, g_showMaterials_windowRect.right, g_showMaterials_windowRect.bottom );
	Sys_SetRegistryString( "showMaterialsWindowRect", buff );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_LoadConfig	
// 
//-----------------------------------------------------------------------------
void ShowMaterials_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	Sys_GetRegistryInteger( "showMaterialsCurrentFrame", false, g_showMaterials_currentFrame );
	Sys_GetRegistryInteger( "showMaterialsSortColumn", ID_SM_NAME, g_showMaterials_sortColumn );
	Sys_GetRegistryInteger( "showMaterialsSortDescending", false, g_showMaterials_sortDescending );

	Sys_GetRegistryString( "showMaterialsWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_showMaterials_windowRect.left, &g_showMaterials_windowRect.top, &g_showMaterials_windowRect.right, &g_showMaterials_windowRect.bottom );
	if ( numArgs != 4 || g_showMaterials_windowRect.left < 0 || g_showMaterials_windowRect.top < 0 || g_showMaterials_windowRect.right < 0 || g_showMaterials_windowRect.bottom < 0 )
		memset( &g_showMaterials_windowRect, 0, sizeof( g_showMaterials_windowRect ) );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_Clear
// 
//-----------------------------------------------------------------------------
void ShowMaterials_Clear()
{
	// delete all the list view entries
	if ( g_showMaterials_hWnd )
		ListView_DeleteAllItems( g_showMaterials_hWndListView );

	if ( !g_showMaterials_pMaterials )
		return;

	for ( int i=0; i<g_showMaterials_numMaterials; i++ )
	{
		free( g_showMaterials_pMaterials[i].pName );
		free( g_showMaterials_pMaterials[i].pShaderName );
	}

	g_showMaterials_pMaterials   = NULL;
	g_showMaterials_numMaterials = 0;
}

//-----------------------------------------------------------------------------
//	ShowMaterials_Export
// 
//-----------------------------------------------------------------------------
void ShowMaterials_Export()
{
}

//-----------------------------------------------------------------------------
//	ShowMaterials_SetTitle
// 
//-----------------------------------------------------------------------------
void ShowMaterials_SetTitle()
{
	char	titleBuff[128];

	if ( g_showMaterials_hWnd )
	{
		strcpy( titleBuff, "Materials " );
		if ( g_showMaterials_currentFrame )
			strcat( titleBuff, " [FRAME]" );

		SetWindowText( g_showMaterials_hWnd, titleBuff );
	}
}

//-----------------------------------------------------------------------------
//	ShowMaterials_CompareFunc
// 
//-----------------------------------------------------------------------------
int CALLBACK ShowMaterials_CompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	material_t*	pMaterialA = ( material_t* )lParam1;
	material_t*	pMaterialB = ( material_t* )lParam2;

	int sort = 0;
	switch ( g_showMaterials_sortColumn )
	{
		case ID_SM_NAME:
			sort = stricmp( pMaterialA->pName, pMaterialB->pName );
			break;

		case ID_SM_SHADER:
			sort = stricmp( pMaterialA->pShaderName, pMaterialB->pShaderName );
			break;

		case ID_SM_REFCOUNT:
			sort = pMaterialA->refCount - pMaterialB->refCount;
			break;
	}

	// flip the sort order
	if ( g_showMaterials_sortDescending )
		sort *= -1;

	return ( sort );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_SortItems
// 
//-----------------------------------------------------------------------------
void ShowMaterials_SortItems()
{
	LVITEM		lvitem;
	material_t	*pMaterial;
	int			i;

	if ( !g_showMaterials_hWnd )
	{
		// only sort if window is visible
		return;
	}

	ListView_SortItems( g_showMaterials_hWndListView, ShowMaterials_CompareFunc, 0 );

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask = LVIF_PARAM;

	// get each item and reset its list index
	int itemCount = ListView_GetItemCount( g_showMaterials_hWndListView );
	for ( i=0; i<itemCount; i++ )
	{
		lvitem.iItem = i;
		ListView_GetItem( g_showMaterials_hWndListView, &lvitem );

		pMaterial = ( material_t* )lvitem.lParam;
		pMaterial->listIndex = i;
	}

	// update list view columns with sort key
	for ( i=0; i<sizeof( g_showMaterials_Labels )/sizeof( g_showMaterials_Labels[0] ); i++ )
	{
		char		symbol;
		LVCOLUMN	lvc; 

		if ( i == g_showMaterials_sortColumn )
			symbol = g_showMaterials_sortDescending ? '<' : '>';
		else
			symbol = ' ';
		sprintf( g_showMaterials_Labels[i].nameBuff, "%s %c", g_showMaterials_Labels[i].name, symbol );

		memset( &lvc, 0, sizeof( lvc ) );
		lvc.mask    = LVCF_TEXT; 
		lvc.pszText = ( LPSTR )g_showMaterials_Labels[i].nameBuff;

		ListView_SetColumn( g_showMaterials_hWndListView, i, &lvc );
	}
}

//-----------------------------------------------------------------------------
//	ShowMaterials_AddViewItem
// 
//-----------------------------------------------------------------------------
void ShowMaterials_AddViewItem( material_t* pMaterial )
{
	LVITEM	lvi;

	if ( !g_showMaterials_hWnd )
	{
		// only valid if log window is visible
		return;
	}

	// update the text callback buffers
	sprintf( pMaterial->refCountBuff, "%d", pMaterial->refCount );

	int itemCount = ListView_GetItemCount( g_showMaterials_hWndListView );

	// setup and insert at end of list
	memset( &lvi, 0, sizeof( lvi ) );
	lvi.mask      = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	lvi.iItem     = itemCount;
	lvi.iSubItem  = 0;
	lvi.state     = 0; 
	lvi.stateMask = 0;
	lvi.pszText   = LPSTR_TEXTCALLBACK;  
	lvi.lParam    = ( LPARAM )pMaterial;
								
	// insert and set the real index 
	pMaterial->listIndex = ListView_InsertItem( g_showMaterials_hWndListView, &lvi );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_Refresh
// 
//-----------------------------------------------------------------------------
void ShowMaterials_Refresh()
{
	char	command[256];

	strcpy( command, "mat_material_list" );

//	if ( !g_showMaterials_currentFrame )
//		strcat( command, " all" );

	// send the command to application which replies with list data
	if ( g_connectedToApp )
		ProcessCommand( command );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_SizeWindow
// 
//-----------------------------------------------------------------------------
void ShowMaterials_SizeWindow( HWND hwnd, int cx, int cy )
{
    if ( cx==0 || cy==0 )
    {
        RECT rcClient;
        GetClientRect( hwnd, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

	// position the ListView
	SetWindowPos( g_showMaterials_hWndListView, NULL, 0, 0, cx, cy, SWP_NOZORDER );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK ShowMaterials_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD		wID = LOWORD( wParam );
	material_t*	pMaterial;

	switch ( message )
	{
		case WM_CREATE:
			return 0L;

		case WM_DESTROY:
			ShowMaterials_SaveConfig();
			g_showMaterials_hWnd = NULL;
			return 0L;
	
		case WM_INITMENU:
            CheckMenuItem( ( HMENU )wParam, IDM_OPTIONS_CURRENTFRAME, MF_BYCOMMAND | ( g_showMaterials_currentFrame ? MF_CHECKED : MF_UNCHECKED ) );
			return 0L;

		case WM_SIZE:
			ShowMaterials_SizeWindow( hwnd, LOWORD( lParam ), HIWORD( lParam ) );
			return 0L;

		case WM_NOTIFY:
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_COLUMNCLICK:
					NMLISTVIEW*	pnmlv;
					pnmlv = ( NMLISTVIEW* )lParam; 
					if ( g_showMaterials_sortColumn == pnmlv->iSubItem )
					{
						// user has clicked on same column - flip the sort
						g_showMaterials_sortDescending ^= 1;
					}
					else
					{
						// sort by new column
						g_showMaterials_sortColumn = pnmlv->iSubItem;
					}
					ShowMaterials_SortItems();
					return 0L;

				case LVN_GETDISPINFO:
					NMLVDISPINFO* plvdi;
					plvdi    = ( NMLVDISPINFO* )lParam;
					pMaterial = ( material_t* )plvdi->item.lParam;
					switch ( plvdi->item.iSubItem )
					{
						case ID_SM_NAME:
							plvdi->item.pszText = pMaterial->pName;
							return 0L;
	  					
						case ID_SM_SHADER:
							plvdi->item.pszText = pMaterial->pShaderName;
							return 0L;

						case ID_SM_REFCOUNT:
							plvdi->item.pszText = pMaterial->refCountBuff;
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
				case IDM_OPTIONS_REFRESH:
					ShowMaterials_Refresh();
					return 0L;

				case IDM_OPTIONS_EXPORT:
					ShowMaterials_Export();
					return 0L;

				case IDM_OPTIONS_CURRENTFRAME:
					g_showMaterials_currentFrame ^= 1;
					ShowMaterials_SetTitle();
					ShowMaterials_Refresh();
					return 0L;
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	ShowMaterials_Init
// 
//-----------------------------------------------------------------------------
bool ShowMaterials_Init()
{
	// set up our window class
	WNDCLASS wndclass;

	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = ShowMaterials_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_SHOWMATERIALS );
	wndclass.lpszClassName = "SHOWMATERIALSCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	ShowMaterials_LoadConfig();

	return true;
}

//-----------------------------------------------------------------------------
//	ShowMaterials_Open	
// 
//-----------------------------------------------------------------------------
void ShowMaterials_Open()
{
	RECT			clientRect;
	HWND			hWnd;
	int				i;

	if ( g_showMaterials_hWnd )
	{
		// only one instance
		if ( IsIconic( g_showMaterials_hWnd ) )
			ShowWindow( g_showMaterials_hWnd, SW_RESTORE );
		SetForegroundWindow( g_showMaterials_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"SHOWMATERIALSCLASS",
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
	g_showMaterials_hWnd = hWnd;

	GetClientRect( g_showMaterials_hWnd, &clientRect ); 
	hWnd = CreateWindow( 
				WC_LISTVIEW, 
				"", 
				WS_VISIBLE|WS_CHILD|LVS_REPORT, 
				0, 
				0, 
				clientRect.right-clientRect.left,
				clientRect.bottom-clientRect.top, 
				g_showMaterials_hWnd,
				( HMENU )ID_SHOWMATERIALS_LISTVIEW,
				g_hInstance,
				NULL ); 
	g_showMaterials_hWndListView = hWnd;

	// init list view columns
	for ( i=0; i<sizeof( g_showMaterials_Labels )/sizeof( g_showMaterials_Labels[0] ); i++ )
	{
		LVCOLUMN lvc; 
		memset( &lvc, 0, sizeof( lvc ) );

		lvc.mask     = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
		lvc.iSubItem = 0;
		lvc.cx       = g_showMaterials_Labels[i].width;
		lvc.fmt      = LVCFMT_LEFT;
		lvc.pszText  = ( LPSTR )g_showMaterials_Labels[i].name;

		ListView_InsertColumn( g_showMaterials_hWndListView, i, &lvc );
	}

	ListView_SetBkColor( g_showMaterials_hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( g_showMaterials_hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx( g_showMaterials_hWndListView, style, style );

	// populate list view
	for ( i=0; i<g_showMaterials_numMaterials; i++ )
		ShowMaterials_AddViewItem( &g_showMaterials_pMaterials[i] );
	ShowMaterials_SortItems();

	ShowMaterials_SetTitle();

	if ( g_showMaterials_windowRect.right && g_showMaterials_windowRect.bottom )
		MoveWindow( g_showMaterials_hWnd, g_showMaterials_windowRect.left, g_showMaterials_windowRect.top, g_showMaterials_windowRect.right-g_showMaterials_windowRect.left, g_showMaterials_windowRect.bottom-g_showMaterials_windowRect.top, FALSE );
	ShowWindow( g_showMaterials_hWnd, SHOW_OPENWINDOW );

	// get data from application
	ShowMaterials_Refresh();
}

//-----------------------------------------------------------------------------
// rc_MaterialList
//
// Sent from application with material list
//-----------------------------------------------------------------------------
int rc_MaterialList( char* commandPtr )
{
	char*			cmdToken;
	int				numMaterials;
	int				materialList;
	int				retAddr;
	int				retVal;
	int				errCode = -1;
	xrMaterial_t*	pLocalList;

	// remove old entries
	ShowMaterials_Clear();

	// get number of materials
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &numMaterials );

	// get material list
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &materialList );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &retAddr );

	pLocalList = new xrMaterial_t[numMaterials];
	memset( pLocalList, 0, numMaterials*sizeof( xrMaterial_t ) );

	g_showMaterials_numMaterials = numMaterials;
	g_showMaterials_pMaterials   = new material_t[numMaterials];
	memset( g_showMaterials_pMaterials, 0, numMaterials*sizeof( material_t ) );

	// get the caller's command list 
	DmGetMemory( ( void* )materialList, numMaterials*sizeof( xrMaterial_t ), pLocalList, NULL );

	// build out the resident list
	for ( int i=0; i<numMaterials; i++ )
	{
		// swap the structure
		pLocalList[i].refCount = BigDWord( pLocalList[i].refCount );

		g_showMaterials_pMaterials[i].pName = strdup( pLocalList[i].nameString );
		g_showMaterials_pMaterials[i].pShaderName = strdup( pLocalList[i].shaderString );
		g_showMaterials_pMaterials[i].refCount = pLocalList[i].refCount;

		// add to list view
		ShowMaterials_AddViewItem( &g_showMaterials_pMaterials[i] );
	}

	// return the result
	retVal = numMaterials;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = MaterialList( 0x%8.8x, 0x%8.8x )\n", retVal, numMaterials, materialList );

	delete [] pLocalList;

	// update
	ShowMaterials_SortItems();

	// success
	errCode = 0;

cleanUp:	
	return ( errCode );
}
