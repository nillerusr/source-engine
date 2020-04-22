//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	BINDINGS.CPP
//
//	Keyboard Shortcuts
//=====================================================================================//
#include "vxconsole.h"

#define ID_BINDINGS_LISTVIEW 100

// column id
#define ID_BIND_KEYCODE		0
#define ID_BIND_MENUNAME	1
#define ID_BIND_COMMAND		2

typedef struct
{	const CHAR*			name;
	int					width;
	int					subItemIndex;
} label_t;

typedef struct
{
	int			keyCode;
	const char	*pKeyString;
	char		*pMenuName;
	char		*pCommandString;
} bind_t;

//	{VK_F1,		"F1",	"WireFrame",		"incrementvar mat_wireframe 0 2 1"},
//	{VK_F2, 	"F2",	"FullBright",		"incrementvar mat_fullbright 0 2 1"},
//	{VK_F3,		"F3",	"Impulse 101",		"impulse 101"},
//	{VK_F4,		"F4",	"Screenshot",		"*screenshot"},


bind_t g_bindings[MAX_BINDINGS] = 
{
	{VK_F1,		"F1",	NULL,	NULL},
	{VK_F2, 	"F2",	NULL,	NULL},
	{VK_F3,		"F3",	NULL,	NULL},
	{VK_F4,		"F4",	NULL,	NULL},
	{VK_F5,		"F5",	NULL,	NULL},
	{VK_F6,		"F6",	NULL,	NULL},
	{VK_F7,		"F7",	NULL,	NULL},
	{VK_F8,		"F8",	NULL,	NULL},
	{VK_F9,		"F9",	NULL,	NULL},
	{VK_F10,	"F10",	NULL,	NULL},
	{VK_F11,	"F11",	NULL,	NULL},
	{VK_F12,	"F12",	NULL,	NULL},
};

label_t g_bindings_labels[] =
{
	{"Key",			80,		ID_BIND_KEYCODE},
	{"Menu Name",	150,	ID_BIND_MENUNAME},
	{"Command",		400,	ID_BIND_COMMAND},
};

HWND	g_bindings_hWnd;
HWND	g_bindings_hWndListView;
RECT	g_bindings_windowRect;
char	g_bindingsModify_menuName[256];
char	g_bindingsModify_command[256];
char	g_bindingsModify_keyCode[256];

//-----------------------------------------------------------------------------
//	Bindings_ModifyEntry
//
//-----------------------------------------------------------------------------
void Bindings_ModifyEntry( int i, const char *pMenuName, const char *pCommandString )
{
	if ( g_bindings[i].pMenuName && g_bindings[i].pMenuName[0] )
		Sys_Free( g_bindings[i].pMenuName );
	g_bindings[i].pMenuName = Sys_CopyString( pMenuName );

	if ( g_bindings[i].pCommandString && g_bindings[i].pCommandString[0] )
		Sys_Free( g_bindings[i].pCommandString );
	g_bindings[i].pCommandString = Sys_CopyString( pCommandString );
}

//-----------------------------------------------------------------------------
//	Bindings_GetSelectedItem
//
//-----------------------------------------------------------------------------
int Bindings_GetSelectedItem()
{
	int	i;
	int selection = -1;
	int	state;

	for ( i=0; i<MAX_BINDINGS; i++ )
	{
		state = ListView_GetItemState( g_bindings_hWndListView, i, LVIS_SELECTED );
		if ( state == LVIS_SELECTED )
		{
			selection = i;
			break;
		}
	}

	return selection;
}

//-----------------------------------------------------------------------------
//	Bindings_TranslateKey
//
//-----------------------------------------------------------------------------
bool Bindings_TranslateKey( int vkKeyCode )
{
	int		i;

	for ( i=0; i<MAX_BINDINGS; i++ )
	{
		if ( !g_bindings[i].pCommandString || !g_bindings[i].pCommandString[0] )
			continue;

		if ( vkKeyCode == g_bindings[i].keyCode )
		{
			ProcessCommand( g_bindings[i].pCommandString );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//	Bindings_MenuSelection
//
//-----------------------------------------------------------------------------
bool Bindings_MenuSelection( int wID )
{
	int index;

	index = wID - IDM_BINDINGS_BIND1;
	if ( index < 0 || index > MAX_BINDINGS-1 )
		return false;

	// as if the key were pressed...
	return ( Bindings_TranslateKey( g_bindings[index].keyCode ) );
}

//-----------------------------------------------------------------------------
//	Bindings_UpdateMenu
//
//	Builds the dynamic menu
//-----------------------------------------------------------------------------
void Bindings_UpdateMenu()
{
	HMENU			hMenu;
	HMENU			hNewMenu;
	MENUITEMINFO	menuItemInfo;
	int				i;
	int				numAdded;
	char			menuBuff[64];

	hMenu = GetMenu( g_hDlgMain );
	if ( !hMenu )
		return;

	memset( &menuItemInfo, 0, sizeof( menuItemInfo ) );
	menuItemInfo.cbSize = sizeof( MENUITEMINFO );

	numAdded = 0;
	hNewMenu = CreatePopupMenu();
	menuItemInfo.fMask = MIIM_ID|MIIM_FTYPE|MIIM_STRING;
	menuItemInfo.fType = MFT_STRING;
	for ( i=MAX_BINDINGS-1; i>=0; i-- )
	{
		if ( !g_bindings[i].pCommandString || !g_bindings[i].pCommandString[0] )
			continue;

		menuItemInfo.wID = IDM_BINDINGS_BIND1+i;
		sprintf( menuBuff, "%s\tF%d", g_bindings[i].pMenuName, g_bindings[i].keyCode-VK_F1+1 );
		menuItemInfo.dwTypeData = ( LPSTR )menuBuff;
		InsertMenuItem( hNewMenu, 0, true, &menuItemInfo );

		numAdded++;
	}

	if ( numAdded )
	{
		// add seperator
		menuItemInfo.fMask  = MIIM_FTYPE;
		menuItemInfo.fType  = MFT_SEPARATOR;
		InsertMenuItem( hNewMenu, 0, true, &menuItemInfo );
	}

	menuItemInfo.fMask      = MIIM_ID|MIIM_FTYPE|MIIM_STRING;
	menuItemInfo.fType      = MFT_STRING;
	menuItemInfo.wID        = IDM_BINDINGS_EDIT;
	menuItemInfo.dwTypeData = "Edit...";
	InsertMenuItem( hNewMenu, 0, true, &menuItemInfo );

	// delete the previous menu
	menuItemInfo.fMask = MIIM_SUBMENU;
	GetMenuItemInfo( hMenu, IDM_BINDINGS, false, &menuItemInfo );
	if ( menuItemInfo.hSubMenu )
		DestroyMenu( menuItemInfo.hSubMenu );
	else
	{
		// add a new menu at the tail of the app menu
		AppendMenu( hMenu, MF_STRING, ( UINT_PTR )IDM_BINDINGS, "Bindings" );
	}

	// add the new menu to bar
	menuItemInfo.fMask    = MIIM_SUBMENU;
	menuItemInfo.hSubMenu = hNewMenu;
	SetMenuItemInfo( hMenu, IDM_BINDINGS, false, &menuItemInfo );

	DrawMenuBar( g_hDlgMain );
}

//-----------------------------------------------------------------------------
//	BindingsModifyDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK BindingsModifyDlg_Proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{ 
	switch ( message ) 
	{ 
		case WM_INITDIALOG:
			SetDlgItemText( hWnd, IDC_MODIFYBIND_KEYCODE, g_bindingsModify_keyCode );
			SetDlgItemText( hWnd, IDC_MODIFYBIND_MENUNAME, g_bindingsModify_menuName );
			SetDlgItemText( hWnd, IDC_MODIFYBIND_COMMAND, g_bindingsModify_command );
			return ( TRUE );

		case WM_COMMAND: 
            switch ( LOWORD( wParam ) ) 
            {
				case IDC_OK: 
					GetDlgItemText( hWnd, IDC_MODIFYBIND_MENUNAME, g_bindingsModify_menuName, sizeof( g_bindingsModify_menuName ) );
					GetDlgItemText( hWnd, IDC_MODIFYBIND_COMMAND, g_bindingsModify_command, sizeof( g_bindingsModify_command ) );
				case IDCANCEL:
				case IDC_CANCEL: 
					EndDialog( hWnd, wParam );
					return ( TRUE ); 
			}
			break; 
	}
	return ( FALSE ); 
} 

//-----------------------------------------------------------------------------
//	BindingsModifyDlg_Open
//
//-----------------------------------------------------------------------------
void BindingsModifyDlg_Open( int selection )
{
	int result;
	
	sprintf( g_bindingsModify_keyCode, "F%d", selection+1 );
	strcpy( g_bindingsModify_menuName, g_bindings[selection].pMenuName );
	strcpy( g_bindingsModify_command, g_bindings[selection].pCommandString );

	result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_MODIFYBIND ), g_hDlgMain, ( DLGPROC )BindingsModifyDlg_Proc );
	if ( LOWORD( result ) != IDC_OK )
		return;

	// accept changes and update
	Bindings_ModifyEntry( selection, g_bindingsModify_menuName, g_bindingsModify_command );
	ListView_RedrawItems( g_bindings_hWndListView, 0, MAX_BINDINGS-1 );
	UpdateWindow( g_bindings_hWndListView );
	Bindings_UpdateMenu();
}

//-----------------------------------------------------------------------------
//	Bindings_LoadConfig
//
//-----------------------------------------------------------------------------
void Bindings_LoadConfig()
{
	char	valueBuff[256];
	char	keyBuff[32];
	char	menuName[256];
	char	commandString[256];
	char	*ptr;
	char	*token;
	int		keyCode;
	int		i;
	int		numArgs;
	char	buff[256];

	for ( i=0; i<MAX_BINDINGS; i++ )
	{
		menuName[0] = '\0';
		commandString[0] = '\0';

		sprintf( keyBuff, "bind%d", i );
		Sys_GetRegistryString( keyBuff, valueBuff, "", sizeof( valueBuff ) );

		// parse and populate valid values
		ptr = valueBuff;
		token = Sys_GetToken( &ptr, false, NULL );
		if ( token[0] )
			keyCode = atoi( token );

		token = Sys_GetToken( &ptr, false, NULL );
		if ( token[0] )
			strcpy( menuName, token );

		token = Sys_GetToken( &ptr, false, NULL );
		if ( token[0] )
			strcpy( commandString, token );

		Bindings_ModifyEntry( i, menuName, commandString );
	}

	Sys_GetRegistryString( "bindingsWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_bindings_windowRect.left, &g_bindings_windowRect.top, &g_bindings_windowRect.right, &g_bindings_windowRect.bottom );
	if ( numArgs != 4 )
		memset( &g_bindings_windowRect, 0, sizeof( g_bindings_windowRect ) );
}

//-----------------------------------------------------------------------------
//	Bindings_SaveConfig
//
//-----------------------------------------------------------------------------
void Bindings_SaveConfig()
{
	char			valueBuff[256];
	char			buff[256];
	char			keyBuff[32];
	char			*pMenuName;
	char			*pCommandString;
	int				len;	
	int				i;
	WINDOWPLACEMENT wp;

	if ( g_bindings_hWnd )
	{
		memset( &wp, 0, sizeof( wp ) );
		wp.length = sizeof( WINDOWPLACEMENT );
		GetWindowPlacement( g_bindings_hWnd, &wp );
		g_bindings_windowRect = wp.rcNormalPosition;
		sprintf( buff, "%d %d %d %d", wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom );
		Sys_SetRegistryString( "bindingsWindowRect", buff );
	}

	for ( i=0; i<MAX_BINDINGS; i++ )
	{
		sprintf( keyBuff, "bind%d", i );

		pMenuName = g_bindings[i].pMenuName;
		if ( !pMenuName )
			pMenuName = "";

		pCommandString = g_bindings[i].pCommandString;
		if ( !pCommandString )
			pCommandString = "";

		len = _snprintf( valueBuff, sizeof( valueBuff ), "%d \"%s\" \"%s\"", g_bindings[i].keyCode, pMenuName, pCommandString );
		if ( len == -1 )
		{
			// kill it
			valueBuff[0] = '\0';		
		}

		Sys_SetRegistryString( keyBuff, valueBuff );
	}
}

//-----------------------------------------------------------------------------
//	Bindings_SizeWindow
// 
//-----------------------------------------------------------------------------
void Bindings_SizeWindow( HWND hwnd, int cx, int cy )
{
    if ( cx==0 || cy==0 )
    {
        RECT rcClient;
        GetClientRect( hwnd, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

	// position the ListView
	SetWindowPos( g_bindings_hWndListView, NULL, 0, 0, cx, cy, SWP_NOZORDER );
}

//-----------------------------------------------------------------------------
//	Bindings_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK Bindings_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD	wID = LOWORD( wParam );
	bind_t*	pBind;
	int		selection;

	switch ( message )
	{
		case WM_CREATE:
			return 0L;

		case WM_DESTROY:
			Bindings_SaveConfig();
			Bindings_UpdateMenu();

			g_bindings_hWnd = NULL;
			return 0L;
	
		case WM_SIZE:
			Bindings_SizeWindow( hwnd, LOWORD( lParam ), HIWORD( lParam ) );
			return 0L;

		case WM_NOTIFY:
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_GETDISPINFO:
					NMLVDISPINFO* plvdi;
					plvdi = ( NMLVDISPINFO* )lParam;
					pBind = ( bind_t* )plvdi->item.lParam;
					switch ( plvdi->item.iSubItem )
					{
						case ID_BIND_KEYCODE:
							plvdi->item.pszText = ( LPSTR )pBind->pKeyString;
							return 0L;
	  					
						case ID_BIND_MENUNAME:
							plvdi->item.pszText = pBind->pMenuName;
							return 0L;

						case ID_BIND_COMMAND:
							plvdi->item.pszText = pBind->pCommandString;
							return 0L;

						default:
							break;
					}
					break;

				case NM_DBLCLK:
					NMITEMACTIVATE *pnmitem;
					pnmitem = ( LPNMITEMACTIVATE )lParam;
					BindingsModifyDlg_Open( pnmitem->iItem );
					break;
			}
			break;
		
		case WM_COMMAND:
            switch ( wID )
            {
				case IDM_BINDOPTIONS_MODIFY:
					selection = Bindings_GetSelectedItem();
					if ( selection >= 0 )
						BindingsModifyDlg_Open( selection );
					return 0L;

				case IDM_BINDOPTIONS_DELETE:
					selection = Bindings_GetSelectedItem();
					if ( selection >= 0 )
					{
						Bindings_ModifyEntry( selection, "", "" );
						ListView_RedrawItems( g_bindings_hWndListView, 0, MAX_BINDINGS-1 );
						UpdateWindow( g_bindings_hWndListView );
						Bindings_UpdateMenu();
					}
					return 0L;
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	Bindings_Open
//
//-----------------------------------------------------------------------------
void Bindings_Open()
{
	RECT			clientRect;
	HWND			hWnd;
	int				i;
	LVITEM			lvi;

	if ( g_bindings_hWnd )
	{
		// only one instance
		if ( IsIconic( g_bindings_hWnd ) )
			ShowWindow( g_bindings_hWnd, SW_RESTORE );
		SetForegroundWindow( g_bindings_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"BINDINGSCLASS",
				"Edit Bindings",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				300,
				g_hDlgMain,
				NULL,
				g_hInstance,
				NULL );
	g_bindings_hWnd = hWnd;

	GetClientRect( g_bindings_hWnd, &clientRect ); 
	hWnd = CreateWindow( 
				WC_LISTVIEW, 
				"", 
				WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL|LVS_NOSORTHEADER, 
				0, 
				0, 
				clientRect.right-clientRect.left,
				clientRect.bottom-clientRect.top, 
				g_bindings_hWnd,
				( HMENU )ID_BINDINGS_LISTVIEW,
				g_hInstance,
				NULL ); 
	g_bindings_hWndListView = hWnd;

	// init list view columns
	for ( i=0; i<sizeof( g_bindings_labels )/sizeof( g_bindings_labels[0] ); i++ )
	{
		LVCOLUMN lvc; 
		memset( &lvc, 0, sizeof( lvc ) );

		lvc.mask     = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
		lvc.iSubItem = 0;
		lvc.cx       = g_bindings_labels[i].width;
		lvc.fmt      = LVCFMT_LEFT;
		lvc.pszText  = ( LPSTR )g_bindings_labels[i].name;

		ListView_InsertColumn( g_bindings_hWndListView, i, &lvc );
	}
	
	ListView_SetBkColor( g_bindings_hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( g_bindings_hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx( g_bindings_hWndListView, style, style );

	// populate list view
	for ( i=0; i<MAX_BINDINGS; i++ )
	{
		int itemCount = ListView_GetItemCount( g_bindings_hWndListView );

		// setup and insert at end of list
		memset( &lvi, 0, sizeof( lvi ) );
		lvi.mask      = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
		lvi.iItem     = itemCount;
		lvi.iSubItem  = 0;
		lvi.state     = 0; 
		lvi.stateMask = 0;
		lvi.pszText   = LPSTR_TEXTCALLBACK;  
		lvi.lParam    = ( LPARAM )&g_bindings[i];
								
		ListView_InsertItem( g_bindings_hWndListView, &lvi );
	}
	
	// set the first item selected
	ListView_SetItemState( g_bindings_hWndListView, 0, LVIS_SELECTED, LVIS_SELECTED );
	SetFocus( g_bindings_hWndListView );

	if ( g_bindings_windowRect.right && g_bindings_windowRect.bottom )
		MoveWindow( g_bindings_hWnd, g_bindings_windowRect.left, g_bindings_windowRect.top, g_bindings_windowRect.right-g_bindings_windowRect.left, g_bindings_windowRect.bottom-g_bindings_windowRect.top, FALSE );
	ShowWindow( g_bindings_hWnd, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	Bindings_Init
//
//-----------------------------------------------------------------------------
bool Bindings_Init()
{
	// set up our window class
	WNDCLASS wndclass;
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = Bindings_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_BINDOPTIONS );
	wndclass.lpszClassName = "BINDINGSCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	Bindings_LoadConfig();
	Bindings_UpdateMenu();

	return true;
}