//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "VSSProperties.h"
#include "workspacemanager.h"

static CVSSParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK VSSPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_VSS_USERNAME, g_Params.m_szUserName );
			SetDlgItemText( hwndDlg, IDC_VSS_PROJECT, g_Params.m_szProject );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_VSS_USERNAME ) );
			SendMessage( GetDlgItem( hwndDlg, IDC_VSS_USERNAME ), EM_SETSEL, 0, MAKELONG(0, 0xffff) );

		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_Params.m_szUserName[ 0 ] = 0;
			g_Params.m_szProject[ 0 ] = 0;
			GetDlgItemText( hwndDlg, IDC_VSS_USERNAME, g_Params.m_szUserName, sizeof( g_Params.m_szUserName ) );
			GetDlgItemText( hwndDlg, IDC_VSS_PROJECT, g_Params.m_szProject, sizeof( g_Params.m_szProject ) );
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
int VSSProperties( CVSSParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_VSSPROPERTIES ),
		(HWND)GetWorkspaceManager()->getHandle(),
		(DLGPROC)VSSPropertiesDialogProc );

	*params = g_Params;

	return retval;
}