//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	CONFIG.CPP
//
//	Configuration Dialog
//=====================================================================================//
#include "vxconsole.h"

CHAR	g_xboxTargetName[MAX_XBOXNAMELEN];
char	g_localPath[MAX_PATH];
char	g_targetPath[MAX_PATH];
BOOL	g_clsOnConnect;
BOOL	g_loadSymbolsOnConnect;
char	g_xexTargetPath[MAX_PATH];
BOOL	g_alwaysAutoConnect;
BOOL	g_startMinimized;
char	g_installPath[MAX_PATH];
BOOL	g_captureDebugSpew_StartupState;

//-----------------------------------------------------------------------------
//	ConfigDlg_LoadConfig
//
//-----------------------------------------------------------------------------
void ConfigDlg_LoadConfig()
{
	// get our config
	Sys_GetRegistryString( "xboxName", g_xboxTargetName, "", sizeof( g_xboxTargetName ) );
	Sys_GetRegistryString( "localPath", g_localPath, "u:\\dev\\game", sizeof( g_localPath ) );
	Sys_GetRegistryString( "targetPath", g_targetPath, "e:\\valve", sizeof( g_targetPath ) );
	Sys_GetRegistryString( "installPath", g_installPath, "\\\\fileserver\\user\\xbox\\xbox_orange", sizeof( g_installPath ) );
	Sys_GetRegistryInteger( "clearOnConnect", true, g_clsOnConnect );
	Sys_GetRegistryInteger( "loadSymbolsOnConnect", false, g_loadSymbolsOnConnect );
	Sys_GetRegistryInteger( "alwaysAutoConnect", false, g_alwaysAutoConnect );
	Sys_GetRegistryInteger( "startMinimized", false, g_startMinimized );
	Sys_GetRegistryInteger( "captureDebugSpew", true, g_captureDebugSpew_StartupState );
}

//-----------------------------------------------------------------------------
//	ConfigDlg_SaveConfig
//
//-----------------------------------------------------------------------------
void ConfigDlg_SaveConfig()
{
	// save config
	Sys_SetRegistryString( "xboxName", g_xboxTargetName );
	Sys_SetRegistryString( "localPath", g_localPath );
	Sys_SetRegistryString( "targetPath", g_targetPath );
	Sys_SetRegistryString( "installPath", g_installPath );
	Sys_SetRegistryInteger( "clearOnConnect", g_clsOnConnect );
	Sys_SetRegistryInteger( "loadSymbolsOnConnect", g_loadSymbolsOnConnect );
	Sys_SetRegistryInteger( "alwaysAutoConnect", g_alwaysAutoConnect );
	Sys_SetRegistryInteger( "startMinimized", g_startMinimized );
	Sys_SetRegistryInteger( "captureDebugSpew", g_captureDebugSpew_StartupState );

	// update
	SetMainWindowTitle();
}

//-----------------------------------------------------------------------------
//	ConfigDlg_Setup
//
//-----------------------------------------------------------------------------
void ConfigDlg_Setup( HWND hWnd )
{
	SetDlgItemText( hWnd,IDC_CONFIG_XBOXNAME, g_xboxTargetName ); 
	SetDlgItemText( hWnd,IDC_CONFIG_LOCALPATH, g_localPath ); 
	SetDlgItemText( hWnd,IDC_CONFIG_TARGETPATH, g_targetPath ); 
	SetDlgItemText( hWnd,IDC_CONFIG_INSTALLPATH, g_installPath ); 
	
	EnableWindow( GetDlgItem( hWnd, IDC_CONFIG_PING ), strlen( g_xboxTargetName ) > 0 );

	CheckDlgButton( hWnd, IDC_CONFIG_CLEARONCONNECT, g_clsOnConnect ? BST_CHECKED : BST_UNCHECKED );
	CheckDlgButton( hWnd, IDC_CONFIG_ALWAYSAUTOCONNECT, g_alwaysAutoConnect ? BST_CHECKED : BST_UNCHECKED );
	CheckDlgButton( hWnd, IDC_CONFIG_STARTMINIMIZED, g_startMinimized ? BST_CHECKED : BST_UNCHECKED );
	CheckDlgButton( hWnd, IDC_CONFIG_CAPTUREDEBUGSPEW, g_captureDebugSpew_StartupState ? BST_CHECKED : BST_UNCHECKED );
}

//-----------------------------------------------------------------------------
//	ConfigDlg_Ping
//
//-----------------------------------------------------------------------------
BOOL ConfigDlg_Ping( HWND hwnd )
{
	char	xboxName[MAX_XBOXNAMELEN];
	BOOL	canConnect;
	char*	args[1];

	xboxName[0] = '\0';
	GetDlgItemText( hwnd, IDC_CONFIG_XBOXNAME, xboxName, MAX_XBOXNAMELEN ); 

	// ignore ping to current connection
	if ( !stricmp( g_xboxName, xboxName ) )
	{
		if ( g_connectedToXBox )
		{
			Sys_MessageBox( "Ping", "Already Connected To: '%s'", xboxName );
			return true;
		}
	}

	// terminate any current connection
	lc_disconnect( 0, NULL );

	// trial connect
	args[0]    = xboxName;
	canConnect = lc_connect( 1, args );

	if ( !canConnect )
		Sys_MessageBox( "Ping FAILURE", "Could Not Connect To: %s", xboxName );
	else
		Sys_MessageBox( "Ping SUCCESS", "Connection Valid To: %s", g_xboxName );

	if ( canConnect )
		lc_disconnect( 0, NULL );

	return canConnect;
}

//-----------------------------------------------------------------------------
//	ConfigDlg_GetChanges
//
//-----------------------------------------------------------------------------
bool ConfigDlg_GetChanges( HWND hwnd )
{
	char	remotePath[MAX_PATH];
	char	localPath[MAX_PATH];
	char	targetPath[MAX_PATH];
	char	installPath[MAX_PATH];
	char	xboxName[MAX_XBOXNAMELEN];
	char	xexLocalPath[MAX_PATH];
	char	xexTargetPath[MAX_PATH];

	xboxName[0]      = '\0';
	remotePath[0]    = '\0';
	localPath[0]     = '\0';
	targetPath[0]    = '\0';
	xexLocalPath[0]  = '\0';
	xexTargetPath[0] = '\0';

	GetDlgItemText( hwnd, IDC_CONFIG_XBOXNAME, xboxName, MAX_XBOXNAMELEN ); 
	GetDlgItemText( hwnd, IDC_CONFIG_LOCALPATH, localPath, MAX_PATH ); 
	GetDlgItemText( hwnd, IDC_CONFIG_TARGETPATH, targetPath, MAX_PATH ); 
	GetDlgItemText( hwnd, IDC_CONFIG_INSTALLPATH, installPath, MAX_PATH ); 

	strcpy( g_localPath, localPath );
	Sys_NormalizePath( g_localPath, true );

	strcpy( g_targetPath, targetPath );
	Sys_NormalizePath( g_targetPath, true );

	strcpy( g_installPath, installPath );
	Sys_NormalizePath( g_installPath, true );

	strcpy( g_xboxTargetName, xboxName );

	g_clsOnConnect = IsDlgButtonChecked( hwnd, IDC_CONFIG_CLEARONCONNECT );
	g_loadSymbolsOnConnect = IsDlgButtonChecked( hwnd, IDC_CONFIG_LOADSYMBOLS );
	g_alwaysAutoConnect = IsDlgButtonChecked( hwnd, IDC_CONFIG_ALWAYSAUTOCONNECT );
	g_startMinimized = IsDlgButtonChecked( hwnd, IDC_CONFIG_STARTMINIMIZED );
	g_captureDebugSpew_StartupState = IsDlgButtonChecked( hwnd, IDC_CONFIG_CAPTUREDEBUGSPEW );

	// success
	return ( true );
}

//-----------------------------------------------------------------------------
//	ConfigDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK ConfigDlg_Proc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{ 
	switch ( message ) 
	{ 
		case WM_INITDIALOG:
			ConfigDlg_Setup( hwnd );
			return ( TRUE );

		case WM_COMMAND: 
            switch ( LOWORD( wParam ) ) 
            {
				case IDC_CONFIG_PING:
					ConfigDlg_Ping( hwnd );
					break;

				case IDC_CONFIG_XBOXNAME:
					CHAR buff[MAX_XBOXNAMELEN];
					GetDlgItemText( hwnd, IDC_CONFIG_XBOXNAME, buff, sizeof( buff ) );
					EnableWindow( GetDlgItem( hwnd, IDC_CONFIG_PING ), strlen( buff ) > 0 );
					break;

				case IDC_OK: 
					if ( !ConfigDlg_GetChanges( hwnd ) )
						break;
				case IDCANCEL:
				case IDC_CANCEL: 
					EndDialog( hwnd, wParam );
					return ( TRUE ); 
			}
			break; 
	}
	return ( FALSE ); 
} 

//-----------------------------------------------------------------------------
//	ConfigDlg_Open
//
//-----------------------------------------------------------------------------
void ConfigDlg_Open( void )
{
	int result;

	result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_CONFIG ), g_hDlgMain, ( DLGPROC )ConfigDlg_Proc );
	if ( LOWORD( result ) != IDC_OK )
		return;

	ConfigDlg_SaveConfig();
}
