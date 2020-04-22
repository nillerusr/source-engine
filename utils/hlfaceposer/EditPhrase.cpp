//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#define UNICODE

#include "resource.h"
#include "EditPhrase.h"
#include "mxtk/mx.h"
#include "mdlviewer.h"

static CEditPhraseParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK EditPhraseDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );


			SetDlgItemTextW( hwndDlg, IDC_INPUTSTRING, g_Params.m_szInputText );
			SetDlgItemTextA( hwndDlg, IDC_STATIC_PROMPT, g_Params.m_szPrompt );

			SetWindowTextA( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_INPUTSTRING ) );
			SendMessage( GetDlgItem( hwndDlg, IDC_INPUTSTRING ), EM_SETSEL, 0, MAKELONG(0, 0xffff) );

		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_Params.m_szInputText[ 0 ] = 0;
			GetDlgItemTextW( hwndDlg, IDC_INPUTSTRING, g_Params.m_szInputText, ARRAYSIZE( g_Params.m_szInputText ) );
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
int EditPhrase( CEditPhraseParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EDITPHRASE ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EditPhraseDialogProc );

	*params = g_Params;

	return retval;
}