//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	ASSERT_DIALOG.CPP
//
//	Handle Remote Assert().
//=====================================================================================//
#include "vxconsole.h"

AssertAction_t		g_AssertAction			= ASSERT_ACTION_BREAK;
static const char *	g_AssertInfo			= "Assert Info Not Available.";
bool				g_AssertDialogActive	= false;

//-----------------------------------------------------------------------------
//	AssertDialogProc
//
//-----------------------------------------------------------------------------
int CALLBACK AssertDialogProc( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_INITDIALOG:
		{
			SetWindowText( hDlg, "Xbox 360 Assert!" ); 
			SetDlgItemText( hDlg, IDC_FILENAME_CONTROL, g_AssertInfo );

			// Center the dialog.
			RECT rcDlg, rcDesktop;
			GetWindowRect( hDlg, &rcDlg );
			GetWindowRect( GetDesktopWindow(), &rcDesktop );
			SetWindowPos( 
				hDlg, 
				HWND_TOP, 
				((rcDesktop.right-rcDesktop.left) - (rcDlg.right-rcDlg.left)) / 2,
				((rcDesktop.bottom-rcDesktop.top) - (rcDlg.bottom-rcDlg.top)) / 2,
				0,
				0,
				SWP_NOSIZE );
		}
		return true;

	case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
				// Ignore Asserts in this file from now on.
			case IDC_IGNORE_FILE:
				{
					g_AssertAction = ASSERT_ACTION_IGNORE_FILE;
					EndDialog( hDlg, 0 );
					return true;
				}

				// Ignore this Assert once.
			case IDC_IGNORE_THIS:
				{
					g_AssertAction = ASSERT_ACTION_IGNORE_THIS;
					EndDialog( hDlg, 0 );
					return true;
				}

				// Always ignore this Assert.
			case IDC_IGNORE_ALWAYS:
				{
					g_AssertAction = ASSERT_ACTION_IGNORE_ALWAYS;
					EndDialog( hDlg, 0 );
					return true;
				}

				// Ignore all Asserts from now on.
			case IDC_IGNORE_ALL:
				{
					g_AssertAction = ASSERT_ACTION_IGNORE_ALL;
					EndDialog( hDlg, 0 );
					return true;
				}

			case IDC_BREAK:
				{
					g_AssertAction = ASSERT_ACTION_BREAK;
					EndDialog( hDlg, 0 );
					return true;
				}
			}

		case WM_KEYDOWN:
			{
				// Escape?
				if ( wParam == 2 )
				{
					// Ignore this Assert.
					g_AssertAction = ASSERT_ACTION_IGNORE_THIS;
					EndDialog( hDlg, 0 );
					return true;
				}
			}
		}
		return true;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// rc_Assert
//
// Sent from application on hitting an Assert
//-----------------------------------------------------------------------------
int rc_Assert( char* commandPtr )
{
	char*			cmdToken;
	int				retAddr;
	int				errCode = -1;

	// Flash the taskbar icon (otherwise users may not realise the app has stalled on an Assert, esp. during loading)
	FLASHWINFO flashWInfo = { sizeof(FLASHWINFO), g_hDlgMain, FLASHW_ALL|FLASHW_TIMERNOFG, 0, 1000 };
	FlashWindowEx( &flashWInfo );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	if (1 != sscanf( cmdToken, "%x", &retAddr ))
		goto cleanUp;

	// skip whitespace
	while ( commandPtr[0] == ' ' )
	{
		commandPtr++;
	}

	// Display file/line/expression info from the message in the Assert dialog
	// (convert '\t' to '\n'; way simpler than tokenizing a general assert expression)
	g_AssertInfo = commandPtr;
	char *tab = commandPtr;
	while( ( tab = strchr( tab, '\t' ) ) != NULL )
	{
		tab[0] = '\n';
	}

	// Open the Assert dialog, to determine the desired action
	g_AssertAction = ASSERT_ACTION_BREAK;
	g_AssertDialogActive = true;
	DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_ASSERT_DIALOG ), g_hDlgMain, ( DLGPROC )AssertDialogProc );	
	g_AssertDialogActive = false;

	// Write the (endian-converted) result directly back into the application's memory:
	int xboxRetVal = BigDWord( g_AssertAction );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	// success
	errCode = 0;

cleanUp:	
	return errCode;
}
