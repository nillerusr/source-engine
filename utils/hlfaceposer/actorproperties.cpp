//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "resource.h"
#include "ActorProperties.h"
#include "ChoreoView.h"
#include "choreoactor.h"
#include "mdlviewer.h"

static CActorParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK ActorPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_ACTORNAME, g_Params.m_szName );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_ACTORNAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_Params.m_szName[ 0 ] = 0;
			GetDlgItemText( hwndDlg, IDC_ACTORNAME, g_Params.m_szName, 256 );
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
int ActorProperties( CActorParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_ACTORPROPERTIES ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)ActorPropertiesDialogProc );

	*params = g_Params;

	return retval;
}