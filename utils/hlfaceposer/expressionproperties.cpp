//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <mxtk/mx.h>
#include "resource.h"
#include "ExpressionProperties.h"
#include "mdlviewer.h"

static CExpressionParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK ExpressionPropertiesDialogProc
//-----------------------------------------------------------------------------
static BOOL CALLBACK ExpressionPropertiesDialogProc ( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) 
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		{
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
			g_Params.PositionSelf( hwndDlg );
			
			SetDlgItemText( hwndDlg, IDC_EXPRESSIONNAME, g_Params.m_szName );
			SetDlgItemText( hwndDlg, IDC_EXPRESSIONDESC, g_Params.m_szDescription );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_EXPRESSIONNAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_Params.m_szName[ 0 ] = 0;
			GetDlgItemText( hwndDlg, IDC_EXPRESSIONNAME, g_Params.m_szName, 256 );
			GetDlgItemText( hwndDlg, IDC_EXPRESSIONDESC, g_Params.m_szDescription, 256 );
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
int ExpressionProperties( CExpressionParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EXPRESSIONPROPERTIES ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)ExpressionPropertiesDialogProc );

	*params = g_Params;

	return retval;
}