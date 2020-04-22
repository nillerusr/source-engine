//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	VPROJECT.CPP
//
//=====================================================================================//
#include "vproject.h"

#define MAX_PROJECTS 50

#define ID_PROJECTS_LISTVIEW 100

// column id
#define ID_PROJECT_NAME			0
#define ID_PROJECT_DIRECTORY	1

#define WM_TRAY					(WM_APP + 1)
#define ID_TRAY					5000
#define ID_TRAY_ADDVPROJECT		6000
#define ID_TRAY_MODIFYVPROJECT	6001
#define ID_TRAY_MOVEUP			6002
#define ID_TRAY_MOVEDOWN		6003
#define ID_TRAY_EXIT			6004

typedef struct
{
	char	*pName;
	char	*pGamedir;
} project_t;

HWND				g_hWnd;
char				g_project_name[256];
char				g_project_gamedir[256];
HINSTANCE			g_hInstance;
project_t			g_projects[MAX_PROJECTS];
int					g_numProjects;
NOTIFYICONDATA		g_iconData;
HMENU				g_hMenu;
int					g_nActiveVProject;
POINT				g_cursorPoint;			

void TrayMessageHandler( HWND hWnd, UINT uMessageID );

//-----------------------------------------------------------------------------
//	SetVProject
//
//-----------------------------------------------------------------------------
void SetVProject( const char *pProjectName )
{
	char	*pGamedir;
	char	project[256];
	int		i;

	if ( pProjectName )
	{
		strcpy( project, pProjectName );
		Sys_StripQuotesFromToken( project );

		for ( i=0; i<g_numProjects; i++ )
		{
			if ( !stricmp( g_projects[i].pName, project ) )
			{
				// found
				break;
			}
		}

		if ( i >= g_numProjects )
		{
			// not found
			return;
		}

		pGamedir = g_projects[i].pGamedir;
	}
	else
	{
		pGamedir = "";
	}

	// Changed to CURRENT_USER to solve security issues in vista!
	Sys_SetRegistryString( 
		//"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager\\Environment\\VProject",
		"HKEY_CURRENT_USER\\Environment\\VProject"
		pGamedir );

	DWORD	result;
	SendMessageTimeout(
		HWND_BROADCAST,
		WM_SETTINGCHANGE,
		0,
		(LPARAM)"Environment",
		SMTO_ABORTIFHUNG,
		0,
		&result );
}

//-----------------------------------------------------------------------------
//	ModifyVProject
//
//-----------------------------------------------------------------------------
void ModifyVProject( int index, const char *pName, const char *pGamedir )
{
	free( g_projects[index].pName );
	free( g_projects[index].pGamedir );

	if ( !pName || !pName[0] )
	{
		// delete
		if ( g_numProjects-index-1 > 0 )
		{
			// shift remaining elements
			memcpy( &g_projects[index], &g_projects[index+1], (g_numProjects-index-1)*sizeof( project_t ) );
		}

		g_projects[g_numProjects-1].pName = NULL;
		g_projects[g_numProjects-1].pGamedir = NULL;

		g_numProjects--;

		if ( g_nActiveVProject == index+1 )
		{
			// deleted current vproject
			if ( !g_numProjects )
			{
				// no more projects
				g_nActiveVProject = 0;
				SetVProject( NULL );
			}
			else
			{
				// set to top
				g_nActiveVProject = 1;
				SetVProject( g_projects[0].pName );
			}
		}
	}
	else
	{
		g_projects[index].pName = strdup( pName );
		g_projects[index].pGamedir = strdup( pGamedir );
	}
}

//-----------------------------------------------------------------------------
//	AddVProject
//
//-----------------------------------------------------------------------------
void AddVProject( const char *pName, const char *pGamedir )
{
	if ( !pName || !pName[0] )
	{
		// do not add empty projects
		return;
	}

	ModifyVProject( g_numProjects, pName, pGamedir );
	g_numProjects++;
}

//-----------------------------------------------------------------------------
//	LoadRegistryValues
//
//-----------------------------------------------------------------------------
void LoadRegistryValues()
{
	char	keyBuff[32];
	char	valueBuff[256];
	char	projectName[256];
	char	gamedirString[256];
	char	*ptr;
	char	*token;
	int		i;

	for ( i=0; i<MAX_PROJECTS; i++ )
	{
		projectName[0] = '\0';
		gamedirString[0] = '\0';

		sprintf( keyBuff, "project%d", i );
		Sys_GetRegistryString( keyBuff, valueBuff, "", sizeof( valueBuff ) );

		// parse and populate valid values
		ptr = valueBuff;
		token = Sys_GetToken( &ptr, false, NULL );
		if ( token[0] )
		{
			strcpy( projectName, token );
		}
		else
		{
			continue;
		}

		token = Sys_GetToken( &ptr, false, NULL );
		if ( token[0] )
		{
			strcpy( gamedirString, token );
		}

		AddVProject( projectName, gamedirString );
	}
}

//-----------------------------------------------------------------------------
//	SaveRegistryValues
//
//-----------------------------------------------------------------------------
void SaveRegistryValues()
{
	char			valueBuff[256];
	char			keyBuff[32];
	char			*pProjectName;
	char			*pGamedir;
	int				len;	
	int				i;

	for ( i=0; i<MAX_PROJECTS; i++ )
	{
		sprintf( keyBuff, "project%d", i );

		pProjectName = g_projects[i].pName;
		if ( !pProjectName )
		{
			pProjectName = "";
		}

		pGamedir = g_projects[i].pGamedir;
		if ( !pGamedir )
		{
			pGamedir = "";
		}

		len = _snprintf( valueBuff, sizeof( valueBuff ), "\"%s\" \"%s\"", pProjectName, pGamedir );
		if ( len == -1 )
		{
			// kill it
			valueBuff[0] = '\0';		
		}

		Sys_SetRegistryString( keyBuff, valueBuff );
	}
}

//-----------------------------------------------------------------------------
//	ShiftActiveProjectUp
//
//-----------------------------------------------------------------------------
void ShiftActiveProjectUp()
{
	if ( g_numProjects <= 1 || !g_nActiveVProject )
	{
		// nothing to do
		return;
	}

	int	active = g_nActiveVProject-1;
	int previous = (active + g_numProjects - 1) % g_numProjects;

	project_t	tempProject;
	tempProject = g_projects[previous];
	g_projects[previous] = g_projects[active];
	g_projects[active] = tempProject;

	g_nActiveVProject = previous + 1;
}

//-----------------------------------------------------------------------------
//	ShiftActiveProjectDown
//
//-----------------------------------------------------------------------------
void ShiftActiveProjectDown()
{
	if ( g_numProjects <= 1 || !g_nActiveVProject )
	{
		// nothing to do
		return;
	}

	int	active = g_nActiveVProject-1;
	int next = (active + g_numProjects + 1) % g_numProjects;

	project_t tempProject;
	tempProject = g_projects[next];
	g_projects[next] = g_projects[active];
	g_projects[active] = tempProject;

	g_nActiveVProject = next + 1;
}

//-----------------------------------------------------------------------------
//	ModifyDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK ModifyDlg_Proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{
	size_t	len;
	int		width;
	int		height;
	RECT	rect;

	switch ( message ) 
	{
		case WM_INITDIALOG:
			SetDlgItemText( hWnd, IDC_MODIFY_PROJECT, g_project_name );
			SetDlgItemText( hWnd, IDC_MODIFY_GAMEDIR, g_project_gamedir );

			// center dialog
			GetWindowRect( hWnd, &rect );
			width = GetSystemMetrics( SM_CXSCREEN ); 
			height = GetSystemMetrics( SM_CYSCREEN ); 
			SetWindowPos( hWnd, NULL, ( width - ( rect.right - rect.left ) )/2, ( height - ( rect.bottom - rect.top ) )/2, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
			return ( TRUE );

		case WM_COMMAND: 
            switch ( LOWORD( wParam ) ) 
            {
				case IDC_OK: 
					GetDlgItemText( hWnd, IDC_MODIFY_PROJECT, g_project_name, sizeof( g_project_name ) );
					GetDlgItemText( hWnd, IDC_MODIFY_GAMEDIR, g_project_gamedir, sizeof( g_project_gamedir ) );

					// remove trailing seperator
					Sys_NormalizePath( g_project_gamedir, false );
					len = strlen( g_project_gamedir );
					if ( len > 2 && g_project_gamedir[len-1] == '\\' )
					{
						g_project_gamedir[len-1] = '\0';
					}
					// fall through

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
//	ModifyDlg_Open
//
//-----------------------------------------------------------------------------
BOOL ModifyDlg_Open()
{
	int result;
	
	result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_VPROJECT ), g_hWnd, ( DLGPROC )ModifyDlg_Proc );
	if ( LOWORD( result ) != IDC_OK )
	{
		return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
//	ShowPopupMenu
//
//-----------------------------------------------------------------------------
void ShowPopupMenu( HWND hWnd, bool bUseCachedMenuPos )
{
	BOOL bDelete = true;

	// delete existing entries
	for ( int nIndex = 1; nIndex < ID_TRAY_ADDVPROJECT && bDelete; nIndex++ )
	{
		bDelete = DeleteMenu( g_hMenu, nIndex, MF_BYCOMMAND );
	}

	// Insert projects
	char szMenuItem[MAX_PATH];
	for ( int nIndex = 0; nIndex < g_numProjects; nIndex++ )
	{
		strcpy( szMenuItem, g_projects[nIndex].pName );
		strcat( szMenuItem, "\t" );
		strcat( szMenuItem, g_projects[nIndex].pGamedir );
		strcat( szMenuItem, "     " );

		InsertMenu( g_hMenu, nIndex, MF_BYPOSITION | MF_STRING, nIndex + 1, szMenuItem );
	}

	if ( g_nActiveVProject )
	{
		CheckMenuItem( g_hMenu, g_nActiveVProject, MF_BYCOMMAND|MF_CHECKED );
		SetMenuDefaultItem( g_hMenu, g_nActiveVProject, FALSE );
	}

	// Display the popup menu at the current cursor location
	// Use the cached cursor position if set
	if ( !bUseCachedMenuPos || ( !g_cursorPoint.x && !g_cursorPoint.y ) )
	{
		GetCursorPos( &g_cursorPoint );
	}
	SetForegroundWindow( hWnd );
	TrackPopupMenu( g_hMenu, 0, g_cursorPoint.x, g_cursorPoint.y, 0, hWnd, 0 );
	PostMessage( hWnd, WM_NULL, 0, 0 );
}

//-----------------------------------------------------------------------------
//	TrayMessageHandler
//
//-----------------------------------------------------------------------------
void TrayMessageHandler( HWND hWnd, UINT uMessageID )
{
	switch ( uMessageID )
	{
	case 0:
		break;

	case ID_TRAY_EXIT:
		DestroyWindow( hWnd );
		break;

	case ID_TRAY_ADDVPROJECT:
		SetForegroundWindow( hWnd );
		g_project_name[0] = '\0';
		g_project_gamedir[0] = '\0';
		if ( ModifyDlg_Open() )
		{
			AddVProject( g_project_name, g_project_gamedir );
			SaveRegistryValues();
		}
		ShowPopupMenu( hWnd, true );
		break;

	case ID_TRAY_MODIFYVPROJECT:
		SetForegroundWindow( hWnd );
		if ( g_nActiveVProject )
		{
			strcpy( g_project_name, g_projects[g_nActiveVProject-1].pName );
			strcpy( g_project_gamedir, g_projects[g_nActiveVProject-1].pGamedir );
			if ( ModifyDlg_Open() )
			{
				ModifyVProject( g_nActiveVProject-1, g_project_name, g_project_gamedir );
				SaveRegistryValues();
			}
			ShowPopupMenu( hWnd, true );
		}
		break;

	case ID_TRAY_MOVEUP:
		SetForegroundWindow( hWnd );
		if ( g_nActiveVProject )
		{
			ShiftActiveProjectUp();
			SaveRegistryValues();
			ShowPopupMenu( hWnd, true );
		}
		break;

	case ID_TRAY_MOVEDOWN:
		SetForegroundWindow( hWnd );
		if ( g_nActiveVProject )
		{
			ShiftActiveProjectDown();
			SaveRegistryValues();
			ShowPopupMenu( hWnd, true );
		}
		break;

	default:
		if ( uMessageID >= 1 && uMessageID <= MAX_PROJECTS )
		{
			// set current vproject
			g_nActiveVProject = uMessageID;
			SetVProject( g_projects[uMessageID-1].pName );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
//	Main_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK Main_WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_DESTROY:
			PostQuitMessage( 0 );
			return 0L;
	
		case WM_TRAY:
			if ( lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN )
			{
				ShowPopupMenu( hWnd, false );
				return 0L;
			}
			break;

		case WM_COMMAND:
			TrayMessageHandler( hWnd, LOWORD( wParam ) );
			break;
	}	

	return ( DefWindowProc( hWnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	Startup
//
//-----------------------------------------------------------------------------
bool Startup()
{
	int				i;

	// set up our window class
	WNDCLASS wndclass;
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = Main_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = LoadIcon( g_hInstance, (LPCTSTR)IDI_VPROJECT );
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = VPROJECT_CLASSNAME;
	if ( !RegisterClass( &wndclass ) )
	{
		return false;
	}

	// create the hidden window
	g_hWnd = CreateWindow( 
				VPROJECT_CLASSNAME,
				0,
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				0,
				CW_USEDEFAULT,
				0,
				NULL,
				NULL,
				g_hInstance,
				NULL );
	
	// Create tray icon
	g_iconData.cbSize           = sizeof( NOTIFYICONDATA );
	g_iconData.hIcon            = LoadIcon( g_hInstance, (LPCTSTR)IDI_VPROJECT );
	g_iconData.hWnd             = g_hWnd;
	g_iconData.uCallbackMessage = WM_TRAY;
	g_iconData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_iconData.uID              = ID_TRAY;
	strcpy(g_iconData.szTip, "VPROJECT");
	Shell_NotifyIcon( NIM_ADD, &g_iconData );

	// Create popup menu and add initial items
	g_hMenu = CreatePopupMenu();
	AppendMenu( g_hMenu, MF_SEPARATOR, 0, 0);
	AppendMenu( g_hMenu, MF_STRING, ID_TRAY_ADDVPROJECT, "Add VProject" );
	AppendMenu( g_hMenu, MF_STRING, ID_TRAY_MODIFYVPROJECT, "Modify VProject" );
	AppendMenu( g_hMenu, MF_STRING, ID_TRAY_MOVEUP, "Move Up" );
	AppendMenu( g_hMenu, MF_STRING, ID_TRAY_MOVEDOWN, "Move Down" );
	AppendMenu( g_hMenu, MF_SEPARATOR, 0, 0);
	AppendMenu( g_hMenu, MF_STRING, ID_TRAY_EXIT, "Remove From Tray" );

	// find the current vproject
	char* vproject = getenv( "vproject" );
	if ( vproject && vproject[0] )
	{
		char temp[MAX_PATH];
		strcpy( temp, vproject );
		Sys_NormalizePath( temp, false );
		for ( i=0; i<g_numProjects; i++ )
		{
			if ( !stricmp( g_projects[i].pGamedir, temp ) )
			{
				// found
				g_nActiveVProject = i+1;
				break;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//	Shutdown
// 
// Free all resources
//-----------------------------------------------------------------------------
void Shutdown()
{
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

	// get the project list
	LoadRegistryValues();

	if ( pCmdLine && pCmdLine[0] )
	{
		// set directly
		SetVProject( pCmdLine );
		return 0;
	}

	HWND hwnd = FindWindow( VPROJECT_CLASSNAME, NULL );
	if ( hwnd ) 
	{
		// single instance only
		return ( FALSE );
	}

    if ( !Startup() )
	{
		goto cleanUp;
	}

	// message pump
	while ( GetMessage( &msg, NULL, 0, 0 ) )
    {
		TranslateMessage( &msg );
		DispatchMessage( &msg );
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

    return ( (int)msg.wParam );
}


