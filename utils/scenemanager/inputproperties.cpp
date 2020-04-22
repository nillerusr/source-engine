//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "resource.h"
#include "InputProperties.h"
#include "workspacemanager.h"

static CInputParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK InputPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_INPUTSTRING, g_Params.m_szInputText );
			SetDlgItemText( hwndDlg, IDC_STATIC_PROMPT, g_Params.m_szPrompt );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_INPUTSTRING ) );
			SendMessage( GetDlgItem( hwndDlg, IDC_INPUTSTRING ), EM_SETSEL, 0, MAKELONG(0, 0xffff) );

		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_Params.m_szInputText[ 0 ] = 0;
			GetDlgItemText( hwndDlg, IDC_INPUTSTRING, g_Params.m_szInputText, sizeof( g_Params.m_szInputText ) );
			EndDialog( hwndDlg, 1 );
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		}
		return TRUE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int InputProperties( CInputParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_INPUTDIALOG ),
		(HWND)GetWorkspaceManager()->getHandle(),
		(DLGPROC)InputPropertiesDialogProc );

	*params = g_Params;

	return retval;
}