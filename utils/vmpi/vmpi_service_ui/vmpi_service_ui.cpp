//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vmpi_service_ui.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "consolewnd.h"
#include "resource.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "shell_icon_mgr.h"
#include "vmpi.h"
#include "service_conn_mgr.h"
#include <io.h>
#include <time.h>



void UpdatePopupMenuState();



const char *g_pIconTooltip = VMPI_SERVICE_NAME;

IConsoleWnd *g_pConsoleWnd = NULL;
HINSTANCE g_hInstance = NULL;


#define MYWM_NOTIFYICON		(WM_APP+100)
CShellIconMgr g_ShellIconMgr;

bool g_bHighlightIconWhenBusy = false;


// STATE THE SERVICE OWNS.
int g_iCurState = 0; // One of the VMPI_SERVICE_STATE_ defines.
char *g_pPassword = NULL;
bool g_bScreensaverMode = false;


void LogString( const char *pStr, ... )
{
#ifdef VMPI_SERVICE_LOGS
	char str[4096];
	va_list marker;
	va_start( marker, pStr );
	_vsnprintf( str, sizeof( str ), pStr, marker );
	va_end( marker );
	
	static FILE *fp = fopen( "c:\\vmpi_service_ui.log", "wt" );
	if ( fp )
	{
		fprintf( fp, "%s", str );
		fflush( fp );
	}
#endif
}

char* GetLastErrorString()
{
	static char err[2048];
	
	LPVOID lpMsgBuf;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
	strncpy( err, (char*)lpMsgBuf, sizeof( err ) );
	LocalFree( lpMsgBuf );

	err[ sizeof( err ) - 1 ] = 0;
	return err;
}


// ------------------------------------------------------------------------------------------ //
// Persistent state in the registry.
// ------------------------------------------------------------------------------------------ //
void LoadStateFromRegistry()
{
	HKEY hKey = NULL;
	RegCreateKey( HKEY_LOCAL_MACHINE, VMPI_SERVICE_KEY, &hKey );
	if ( hKey )
	{
		DWORD val = 0;
		DWORD type = REG_DWORD;
		DWORD size = sizeof( val );
		if ( RegQueryValueEx( 
			hKey,
			"HighlightIconWhenBusy",
			0,
			&type,
			(unsigned char*)&val,
			&size ) == ERROR_SUCCESS && 
			type == REG_DWORD && 
			size == sizeof( val ) )
		{
			g_bHighlightIconWhenBusy = (val != 0);
		}
	}
}

void SaveStateToRegistry()
{
	HKEY hKey = NULL;
	RegCreateKey( HKEY_LOCAL_MACHINE, VMPI_SERVICE_KEY, &hKey );
	if ( hKey )
	{
		DWORD val = g_bHighlightIconWhenBusy;
		RegSetValueEx( 
			hKey,
			"HighlightIconWhenBusy",
			0,
			REG_DWORD,
			(unsigned char*)&val,
			sizeof( val ) );
	}
}


// ------------------------------------------------------------------------------------------ //
// Our CServiceConnMgr packet handler.
// ------------------------------------------------------------------------------------------ //

void UpdateAppIcon()
{
	if ( g_iCurState == VMPI_SERVICE_STATE_IDLE )
	{
		g_ShellIconMgr.ChangeIcon( IDI_WAITING_ICON );
	}
	else if ( g_iCurState == VMPI_SERVICE_STATE_BUSY )
	{
		if ( g_bHighlightIconWhenBusy )
			g_ShellIconMgr.ChangeIcon( IDI_BUSY_ICON );
		else
			g_ShellIconMgr.ChangeIcon( IDI_WAITING_ICON );
	}
	else
	{
		g_ShellIconMgr.ChangeIcon( IDI_DISABLED_ICON );
	}
}


class CUIConnMgr : public CServiceConnMgr
{
public:
	virtual void	HandlePacket( const char *pData, int len );
};


void CUIConnMgr::HandlePacket( const char *pData, int len )
{
	if ( pData[0] != VMPI_SERVICE_UI_PROTOCOL_VERSION )
		return;
	
	int packetID = pData[1];
	
	int offset = 2;
	if ( packetID == VMPI_SERVICE_TO_UI_CONSOLE_TEXT )
	{
		Msg( &pData[offset] );
	}
	else if ( packetID == VMPI_SERVICE_TO_UI_STATE )
	{
		// Get the new state out..
		g_iCurState = *((int*)&pData[offset]);
		offset += 4;

		LogString( "New UI state: %d.\n", g_iCurState );
		
		// Update our icon.
		UpdateAppIcon();

		g_bScreensaverMode = (*((char*)&pData[offset]) != 0);
		++offset;

		// Store the current password.
		if ( g_pPassword )
			delete [] g_pPassword;

		const char *pStr = &pData[offset];
		g_pPassword = new char[ strlen( pStr ) + 1 ];
		strcpy( g_pPassword, pStr );
		offset += strlen( pStr ) + 1;

		UpdatePopupMenuState();
	}
	else if ( packetID == VMPI_SERVICE_TO_UI_PATCHING )
	{
		LogString( "Got a VMPI_SERVICE_TO_UI_PATCHING packet.\n" );
		
		int bExitAfter = pData[offset];
		++offset;
		
		char workingDir[MAX_PATH], commandLine[4096];
		V_strncpy( workingDir, &pData[offset], sizeof( workingDir ) );
		offset += V_strlen( workingDir ) + 1;
		
		V_strncpy( commandLine, &pData[offset], sizeof( commandLine ) );
		offset += V_strlen( commandLine ) + 1;
		
		// Run whatever they said to run.
		STARTUPINFO si;
		memset( &si, 0, sizeof( si ) );
		si.cb = sizeof( si );
		
		PROCESS_INFORMATION pi;
		memset( &pi, 0, sizeof( pi ) );
		if ( CreateProcess( NULL, commandLine, NULL, NULL, false, CREATE_NO_WINDOW, NULL, workingDir, &si, &pi ) )
		{
			LogString( "CreateProcess succeeded:\n%s\n", commandLine );

			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );
			
			if ( bExitAfter )
				PostQuitMessage( 0 );
		}
		else
		{
			LogString( "CreateProcess failed: %s", GetLastErrorString() );
		}
	}
	else if ( packetID == VMPI_SERVICE_TO_UI_EXIT )
	{
		// Exit.
		PostQuitMessage( 0 );
	}
}


// ------------------------------------------------------------------------------------------ //
// Helpers.
// ------------------------------------------------------------------------------------------ //

void InternalSpew( const char *pMsg )
{
	if ( g_pConsoleWnd )
	{
		g_pConsoleWnd->PrintToConsole( pMsg );
	}

	OutputDebugString( pMsg );
}


SpewRetval_t MySpewOutputFunc( SpewType_t spewType, const char *pMsg )
{
	// Prepend the time.
	time_t aclock;
	time( &aclock );
	struct tm *newtime = localtime( &aclock );
	
	// Get rid of the \n.
	char timeString[512];
	Q_strncpy( timeString, asctime( newtime ), sizeof( timeString ) );
	char *pEnd = strstr( timeString, "\n" );
	if ( pEnd )
		*pEnd = 0;
	InternalSpew( timeString );
	
	InternalSpew( " - " );
	InternalSpew( pMsg );
	
	if ( spewType == SPEW_ASSERT )
		return SPEW_DEBUGGER;
	else if( spewType == SPEW_ERROR )
		return SPEW_ABORT;
	else
		return SPEW_CONTINUE;
}



CUIConnMgr g_ConnMgr;

void InitConsoleWindow()
{
	// Only initialize it once.
	if ( g_pConsoleWnd )
		return;

	g_pConsoleWnd = CreateConsoleWnd( 
		g_hInstance, 
		IDD_VMPI_SERVICE, 
		IDC_DEBUG_OUTPUT, 
		false );
}


// ------------------------------------------------------------------------------------------ //
// Implementation of IShellIconMgrHelper.
// ------------------------------------------------------------------------------------------ //

HMENU g_hMenu = NULL;
HMENU g_hPopupMenu = NULL;	// This is just a submenu of g_hMenu.


bool LoadPopupMenu()
{
	g_hMenu = LoadMenu( g_hInstance, MAKEINTRESOURCE( IDR_POPUP_MENU ) );
	if ( !g_hMenu )
	{
		Assert( false );
		Warning( "LoadMenu failed.\n" );
		return false;
	}

	g_hPopupMenu = GetSubMenu( g_hMenu, 0 );
	return true;
}


void TermPopupMenu()
{
	if ( g_hMenu )
	{
		DestroyMenu( g_hMenu );
		g_hMenu = NULL;
	}
	g_hPopupMenu = NULL;
}


void UpdatePopupMenuState()
{
	bool bEnabled = (g_iCurState == VMPI_SERVICE_STATE_IDLE || g_iCurState == VMPI_SERVICE_STATE_BUSY);
	EnableMenuItem( g_hPopupMenu, ID_ENABLE_WORKER,  bEnabled  ? MF_GRAYED : MF_ENABLED );
	EnableMenuItem( g_hPopupMenu, ID_DISABLE_WORKER, !bEnabled ? MF_GRAYED : MF_ENABLED );

	// Enable or disable console items.
	EnableMenuItem( g_hPopupMenu, ID_SHOW_CONSOLE_WINDOW,  g_pConsoleWnd->IsVisible() ? MF_GRAYED : MF_ENABLED );
	EnableMenuItem( g_hPopupMenu, ID_HIDE_CONSOLE_WINDOW, !g_pConsoleWnd->IsVisible() ? MF_GRAYED : MF_ENABLED );

	CheckMenuItem( g_hPopupMenu, ID_SCREENSAVER_MODE, g_bScreensaverMode ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( g_hPopupMenu, ID_HIGHLIGHT_ICON_WHEN_BUSY, g_bHighlightIconWhenBusy ? MF_CHECKED : MF_UNCHECKED );
}


int CALLBACK SetPasswordDlgProc(
  HWND hwndDlg,  // handle to dialog box
  UINT uMsg,     // message
  WPARAM wParam, // first message parameter
  LPARAM lParam  // second message parameter
)
{
	switch( uMsg )
	{
		case WM_INITDIALOG:
		{
			if ( g_pPassword )
			{
				HWND hWnd = GetDlgItem( hwndDlg, IDC_PASSWORD );
				SetWindowText( hWnd, g_pPassword );
			}
		}
		break;

		case WM_COMMAND:
		{
			switch( wParam )
			{
				case IDOK:
				{
					// Set our new password.
					HWND hWnd = GetDlgItem( hwndDlg, IDC_PASSWORD );
					if ( hWnd )
					{
						char tempBuf[512];
						GetWindowText( hWnd, tempBuf, sizeof( tempBuf ) );

						// Send it to the service.
						CUtlVector<char> data;
						data.AddToTail( VMPI_SERVICE_UPDATE_PASSWORD );
						data.AddMultipleToTail( strlen( tempBuf ) + 1, tempBuf );
						g_ConnMgr.SendPacket( -1, data.Base(), data.Count() );
					}
					EndDialog( hwndDlg, 0 );
				}
				break;
			
				case IDCANCEL:
				{
					EndDialog( hwndDlg, 0 );
				}
				break;
			}
		}
		break;
	}

	return FALSE;
}


class CShellIconMgrHelper : public IShellIconMgrHelper
{
public:
	virtual HINSTANCE	GetHInstance()
	{
		return g_hInstance;
	}

	virtual int		WindowProc( void *pWnd, int uMsg, long wParam, long lParam )
	{
		HWND hWnd = (HWND)pWnd;

		switch( uMsg )
		{
			// Right button brings up the popup menu.
			case MYWM_NOTIFYICON:
			{
				if ( lParam == WM_RBUTTONDOWN )
				{
					POINT cursorPos;
					GetCursorPos( &cursorPos );

					UpdatePopupMenuState();

					// Make a popup menu.
					SetForegroundWindow( hWnd );
					TrackPopupMenu( g_hPopupMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, cursorPos.x, cursorPos.y, 0, hWnd, NULL );
					return 0;
				}
				else if ( lParam == WM_LBUTTONDOWN )
				{
					// Left button brings up the console.
					g_pConsoleWnd->SetVisible( true );
					UpdatePopupMenuState();
					return 0;
				}
			}
			break;
			
			case WM_COMMAND:
			{
				switch( wParam )
				{
					case ID_ENABLE_WORKER:
					{
						char cPacket = VMPI_SERVICE_ENABLE;
						g_ConnMgr.SendPacket( -1, &cPacket, 1 );
					}
					break;

					case ID_DISABLE_WORKER:
					{
						char cPacket = VMPI_SERVICE_DISABLE;
						g_ConnMgr.SendPacket( -1, &cPacket, 1 );
					}
					break;
					
					case ID_KILLCURRENTJOB:
					{
						char cPacket = VMPI_KILL_PROCESS;
						g_ConnMgr.SendPacket( -1, &cPacket, 1 );
					}
					break;

					case ID_HIGHLIGHT_ICON_WHEN_BUSY:
					{
						g_bHighlightIconWhenBusy = !g_bHighlightIconWhenBusy;
						SaveStateToRegistry();
						UpdateAppIcon();
						UpdatePopupMenuState();
					}
					break;

					case ID_SCREENSAVER_MODE:
					{
						g_bScreensaverMode = !g_bScreensaverMode;
						char cPacket[2] = { VMPI_SERVICE_SCREENSAVER_MODE, g_bScreensaverMode };
						g_ConnMgr.SendPacket( -1, cPacket, sizeof( cPacket ) );
					}
					break;


					case ID_SHOW_CONSOLE_WINDOW:
					{
						g_pConsoleWnd->SetVisible( true );
						UpdatePopupMenuState();
					}
					break;

					case ID_HIDE_CONSOLE_WINDOW:
					{
						g_pConsoleWnd->SetVisible( false );
						UpdatePopupMenuState();
					}
					break;
				
					case ID_SET_PASSWORD:
					{
						DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_SET_PASSWORD ), NULL, SetPasswordDlgProc );
					}
					break;

					case ID_EXIT_SERVICE:
					{
						// Quit the service app..
						char cPacket = VMPI_SERVICE_EXIT;
						g_ConnMgr.SendPacket( -1, &cPacket, 1 );

						// Stop showing the icon.
						g_ShellIconMgr.Term();

						// Wait for a bit for the connection to go away.
						DWORD startTime = GetTickCount();
						while ( GetTickCount()-startTime < 2000 )
						{
							g_ConnMgr.Update();
							if ( !g_ConnMgr.IsConnected() )
								break;
							else
								Sleep( 10 );
						}							
						
						// Quit the UI app.
						PostQuitMessage( 0 );
						return 1;
					}
					break;
				}
			}
			break;
		}	

		return DefWindowProc( (HWND)hWnd, uMsg, wParam, lParam );
	}
};

CShellIconMgrHelper g_ShellIconMgrHelper;



int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	g_hInstance = hInstance;


	LogString( "vmpi_service_ui startup.\n" );

	// Don't run multiple instances.	
	HANDLE hMutex = CreateMutex( NULL, FALSE, "vmpi_service_ui_mutex" );
	if ( hMutex && GetLastError() == ERROR_ALREADY_EXISTS )
		return 1;


	// Hook spew output.
	SpewOutputFunc( MySpewOutputFunc );
	InitConsoleWindow();
	LogString( "Setup console window.\n" );

	LoadStateFromRegistry();

	// Setup the popup menu.
	if( !LoadPopupMenu() )
	{
		return false;
	}
	UpdatePopupMenuState();


	// Setup the tray icon.
	Msg( "Waiting for jobs...\n" );
	if ( !g_ShellIconMgr.Init( &g_ShellIconMgrHelper, g_pIconTooltip, MYWM_NOTIFYICON, IDI_WAITING_ICON ) )
	{
		return false;
	}


	// Connect to the VMPI service.
	g_ConnMgr.InitClient();

	LogString( "Entering main loop.\n" );
	
	while ( 1 )
	{
		MSG msg;
		msg.message = !WM_QUIT;	// So it doesn't accidentally exit.
		while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if ( msg.message == WM_QUIT )
				break;
			
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		if ( msg.message == WM_QUIT )
			break;
	
		g_ConnMgr.Update();
		if ( !g_ConnMgr.IsConnected() )
		{
			g_ShellIconMgr.ChangeIcon( IDI_UNCONNECTED );
		}

		Sleep( 30 );
	}

	// Important that we call this instead of letting the destructor do it because it deletes its 
	// socket and it needs to cleanup some threads.
	g_ConnMgr.Term();
	
	g_ShellIconMgr.Term();
	
	return 0;
}



