//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include "resource.h"
#include "ChannelProperties.h"
#include "ChoreoView.h"
#include "choreoactor.h"
#include "choreoscene.h"
#include "mdlviewer.h"

static CChannelParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK ChannelPropertiesDialogProc
//-----------------------------------------------------------------------------
static BOOL CALLBACK ChannelPropertiesDialogProc ( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) 
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_CHANNELNAME, g_Params.m_szName );

			HWND control = GetDlgItem( hwndDlg, IDC_ACTORCHOICE );

			if ( !g_Params.m_bShowActors )
			{
				// Hide the combo box
				if ( control )
				{
					ShowWindow( control, SW_HIDE );
				}
				control = GetDlgItem( hwndDlg, IDC_STATIC_ACTOR );
				if ( control )
				{
					ShowWindow( control, SW_HIDE );
				}
			}
			else
			{
				SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

				if ( g_Params.m_pScene )
				{
					for ( int i = 0 ; i < g_Params.m_pScene->GetNumActors() ; i++ )
					{
						CChoreoActor *actor = g_Params.m_pScene->GetActor( i );
						if ( actor )
						{
							// add text to combo box
							SendMessage( control, CB_ADDSTRING, 0, (LPARAM)actor->GetName() ); 
						}
					}
				}
				
				SendMessage( control, CB_SETCURSEL, (WPARAM)0, (LPARAM)0 );
			}

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_CHANNELNAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_Params.m_szName[ 0 ] = 0;
			GetDlgItemText( hwndDlg, IDC_CHANNELNAME, g_Params.m_szName, 256 );

			if ( g_Params.m_bShowActors )
			{
				HWND control = GetDlgItem( hwndDlg, IDC_ACTORCHOICE );
				if ( control )
				{
					SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szSelectedActor ), (LPARAM)g_Params.m_szSelectedActor );
				}
			}

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
int ChannelProperties( CChannelParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_CHANNELPROPERTIES ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)ChannelPropertiesDialogProc );

	*params = g_Params;

	return retval;
}