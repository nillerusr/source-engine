//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "GlobalEventProperties.h"
#include "mdlviewer.h"
#include "hlfaceposer.h"
#include "choreoevent.h"
#include "choreoscene.h"
#include "expressions.h"
#include "choreoactor.h"
#include "ifaceposersound.h"
#include "expclass.h"
#include "scriplib.h"

static CGlobalEventParams g_Params;

static void ExtractAutoStateFromParams( CGlobalEventParams *params )
{
	ParseFromMemory( params->m_szAction, strlen( params->m_szAction ) );

	params->m_bAutomate = false;
	if ( TokenAvailable() )
	{
		GetToken( false );
		params->m_bAutomate = !stricmp( token, "automate" ) ? true : false;
	}

	if ( params->m_bAutomate )
	{
		params->m_szType[ 0 ] = 0;
		if ( TokenAvailable() )
		{
			GetToken( false );
			strcpy( params->m_szType, token );
		}

		params->m_flWaitTime = 0.0f;
		if ( TokenAvailable() )
		{
			GetToken( false );
			params->m_flWaitTime = (float)atof( token );
		}
	}
}

static void CreateAutoStateFromControls( CGlobalEventParams *params )
{
	if ( params->m_bAutomate )
	{
		sprintf( params->m_szAction, "automate %s %f", params->m_szType, params->m_flWaitTime );
	}
	else
	{
		sprintf( params->m_szAction, "noaction" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK GlobalEventPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		{
			g_Params.PositionSelf( hwndDlg );
			
			SetDlgItemText( hwndDlg, IDC_EVENTNAME, g_Params.m_szName );

			SetDlgItemText( hwndDlg, IDC_STARTTIME, va( "%f", g_Params.m_flStartTime ) );

			switch ( g_Params.m_nType )
			{
			default:
				Assert(0);
				break;
			case CChoreoEvent::SECTION:
				{
					ShowWindow( GetDlgItem( hwndDlg, IDC_LOOPCOUNT ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_LOOPCOUNT ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_LOOPTIME ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_LOOPTIME ), SW_HIDE );

					ExtractAutoStateFromParams( &g_Params );
				}
				break;
			case CChoreoEvent::LOOP:
				{
					SendMessage( GetDlgItem( hwndDlg, IDC_LOOPCOUNT ), WM_SETTEXT , 0, (LPARAM)va( "%i", g_Params.m_nLoopCount ) );
					SendMessage( GetDlgItem( hwndDlg, IDC_LOOPTIME ), WM_SETTEXT , 0, (LPARAM)va( "%f", g_Params.m_flLoopTime ) );

					ShowWindow( GetDlgItem( hwndDlg, IDC_CB_AUTOACTION ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_DURATION ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_CHECK_AUTOCHECK ), SW_HIDE );

					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_AFTER ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_SECONDS ), SW_HIDE );
				}
				break;
			case CChoreoEvent::STOPPOINT:
				{
					ShowWindow( GetDlgItem( hwndDlg, IDC_LOOPCOUNT ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_LOOPCOUNT ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_LOOPTIME ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_LOOPTIME ), SW_HIDE );

					ShowWindow( GetDlgItem( hwndDlg, IDC_CB_AUTOACTION ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_DURATION ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_CHECK_AUTOCHECK ), SW_HIDE );

					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_AFTER ), SW_HIDE );
					ShowWindow( GetDlgItem( hwndDlg, IDC_STATIC_SECONDS ), SW_HIDE );
				}
				break;
			}
	

			SendMessage( GetDlgItem( hwndDlg, IDC_CHECK_AUTOCHECK ), BM_SETCHECK, 
				( WPARAM ) g_Params.m_bAutomate ? BST_CHECKED : BST_UNCHECKED,
				( LPARAM )0 );

			SetDlgItemText( hwndDlg, IDC_DURATION, va( "%f", g_Params.m_flWaitTime ) );

			SendMessage( GetDlgItem( hwndDlg, IDC_CB_AUTOACTION ), WM_SETTEXT , 0, (LPARAM)g_Params.m_szType );
			// add text to combo box
			SendMessage( GetDlgItem( hwndDlg, IDC_CB_AUTOACTION ), CB_ADDSTRING, 0, (LPARAM)"Cancel" ); 
			SendMessage( GetDlgItem( hwndDlg, IDC_CB_AUTOACTION ), CB_ADDSTRING, 0, (LPARAM)"Resume" ); 


			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_EVENTNAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				char szTime[ 32 ];

				SendMessage( GetDlgItem( hwndDlg, IDC_CB_AUTOACTION ), WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szType ), (LPARAM)g_Params.m_szType );

				GetDlgItemText( hwndDlg, IDC_DURATION, szTime, sizeof( szTime ) );
				g_Params.m_flWaitTime = atof( szTime );

				g_Params.m_bAutomate = SendMessage( GetDlgItem( hwndDlg, IDC_CHECK_AUTOCHECK ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;

				CreateAutoStateFromControls( &g_Params );

				GetDlgItemText( hwndDlg, IDC_EVENTNAME, g_Params.m_szName, sizeof( g_Params.m_szName ) );

				GetDlgItemText( hwndDlg, IDC_STARTTIME, szTime, sizeof( szTime ) );
				g_Params.m_flStartTime = atof( szTime );
				
				char szLoop[ 32 ];
				GetDlgItemText( hwndDlg, IDC_LOOPCOUNT, szLoop, sizeof( szLoop ) );
				g_Params.m_nLoopCount = atoi( szLoop );
				GetDlgItemText( hwndDlg, IDC_LOOPTIME, szLoop, sizeof( szLoop ) );
				g_Params.m_flLoopTime = (float)atof( szLoop );

				EndDialog( hwndDlg, 1 );
			}
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
int GlobalEventProperties( CGlobalEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_GLOBALEVENTPROPERTIES ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)GlobalEventPropertiesDialogProc );

	*params = g_Params;

	return retval;
}