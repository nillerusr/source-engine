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
#include "EventProperties.h"
#include "mdlviewer.h"
#include <commctrl.h>

static CEventParams g_Params;

class CEventPropertiesLookAtDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:
	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );

private:

	void		SetupLookAtControls( CEventParams *params );
	void		SetPitchYawText(  CEventParams *params );
};

void CEventPropertiesLookAtDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "LookAt", "Look At Actor" );
}

void CEventPropertiesLookAtDialog::SetupLookAtControls( CEventParams *params )
{
	SetPitchYawText( params );

	if ( params->pitch != 0 ||
		 params->yaw != 0 )
	{
		params->usepitchyaw = true;
	}
	else
	{
		params->usepitchyaw = false;
	}
	
	HWND control = GetControl( IDC_CHECK_LOOKAT );
	SendMessage( control, BM_SETCHECK, (WPARAM) params->usepitchyaw ? BST_CHECKED : BST_UNCHECKED, 0 );

	// Set up sliders
	control = GetControl( IDC_SLIDER_PITCH );
	SendMessage( control, TBM_SETRANGE, 0, (LPARAM)MAKELONG( -100, 100 ) );
	SendMessage( control, TBM_SETPOS, 1, (LPARAM)(LONG)params->pitch );

	control = GetControl( IDC_SLIDER_YAW );
	SendMessage( control, TBM_SETRANGE, 0, (LPARAM)MAKELONG( -100, 100 ) );
	SendMessage( control, TBM_SETPOS, 1, (LPARAM)(LONG)params->yaw );
}

void CEventPropertiesLookAtDialog::InitControlData( CEventParams *params )
{
	SetDlgItemText( m_hDialog, IDC_STARTTIME, va( "%f", g_Params.m_flStartTime ) );
	SetDlgItemText( m_hDialog, IDC_ENDTIME, va( "%f", g_Params.m_flEndTime ) );
	SendMessage( GetControl( IDC_CHECK_ENDTIME ), BM_SETCHECK, 
		( WPARAM ) g_Params.m_bHasEndTime ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );

	SendMessage( GetControl( IDC_CHECK_RESUMECONDITION ), BM_SETCHECK, 
		( WPARAM ) g_Params.m_bResumeCondition ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );

	PopulateTagList( params );

	HWND choices1 = GetControl( IDC_EVENTCHOICES );
	SendMessage( choices1, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices1, WM_SETTEXT , 0, (LPARAM)params->m_szParameters );

	SetupLookAtControls( params );
	PopulateNamedActorList( choices1, params );
}

void CEventPropertiesLookAtDialog::InitDialog( HWND hwndDlg )
{
	m_hDialog = hwndDlg;

	g_Params.PositionSelf( m_hDialog );
	
	// Set working title for dialog, etc.
	SetTitle();

	// Show/Hide dialog controls
	ShowControlsForEventType( &g_Params );
	InitControlData( &g_Params );

	UpdateTagRadioButtons( &g_Params );

	SetFocus( GetControl( IDC_EVENTNAME ) );
}

static CEventPropertiesLookAtDialog g_EventPropertiesLookAtDialog;

void CEventPropertiesLookAtDialog::SetPitchYawText( CEventParams *params )
{
	HWND control;

	control = GetControl( IDC_STATIC_PITCHVAL );
	SendMessage( control, WM_SETTEXT , 0, (LPARAM)va( "%i", params->pitch ) );
	control = GetControl( IDC_STATIC_YAWVAL );
	SendMessage( control, WM_SETTEXT , 0, (LPARAM)va( "%i", params->yaw ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesLookAtDialog::ShowControlsForEventType( CEventParams *params )
{
	BaseClass::ShowControlsForEventType( params );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK EventPropertiesLookAtDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesLookAtDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

BOOL CEventPropertiesLookAtDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	m_hDialog = hwndDlg;

	bool handled = false;
	BOOL bret = InternalHandleMessage( &g_Params, hwndDlg, uMsg, wParam, lParam, handled );
	if ( handled )
		return bret;

	switch(uMsg)
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps; 
			HDC hdc;
			
            hdc = BeginPaint(hwndDlg, &ps); 
			DrawSpline( hdc, GetControl( IDC_STATIC_SPLINE ), g_Params.m_pEvent );
            EndPaint(hwndDlg, &ps); 

            return FALSE; 
		}
		break;
	case WM_VSCROLL:
		{
			RECT rcOut;
			GetSplineRect( GetControl( IDC_STATIC_SPLINE ), rcOut );

			InvalidateRect( hwndDlg, &rcOut, TRUE );
			UpdateWindow( hwndDlg );
			return FALSE;
		}
		break;
    case WM_INITDIALOG:
		{
			InitDialog( hwndDlg );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				HWND control = GetControl( IDC_EVENTCHOICES );
				if ( control )
				{
					SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szParameters ), (LPARAM)g_Params.m_szParameters );
				}

				GetDlgItemText( m_hDialog, IDC_EVENTNAME, g_Params.m_szName, sizeof( g_Params.m_szName ) );

				if ( !g_Params.m_szName[ 0 ] )
				{
					Q_snprintf( g_Params.m_szName, sizeof( g_Params.m_szName ), "Look at %s", g_Params.m_szParameters );
				}

				char szTime[ 32 ];
				GetDlgItemText( m_hDialog, IDC_STARTTIME, szTime, sizeof( szTime ) );
				g_Params.m_flStartTime = atof( szTime );
				GetDlgItemText( m_hDialog, IDC_ENDTIME, szTime, sizeof( szTime ) );
				g_Params.m_flEndTime = atof( szTime );

				// Parse tokens from tags
				ParseTags( &g_Params );

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		case IDC_CHECK_ENDTIME:
			{
				g_Params.m_bHasEndTime = SendMessage( GetControl( IDC_CHECK_ENDTIME ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
				if ( !g_Params.m_bHasEndTime )
				{
					ShowWindow( GetControl( IDC_ENDTIME ), SW_HIDE );
				}
				else
				{
					ShowWindow( GetControl( IDC_ENDTIME ), SW_RESTORE );
				}
			}
			break;
		case IDC_CHECK_RESUMECONDITION:
			{
				g_Params.m_bResumeCondition = SendMessage( GetControl( IDC_CHECK_RESUMECONDITION ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
			}
			break;
		case IDC_EVENTCHOICES:
			{
				HWND control = (HWND)lParam;
				if ( control )
				{
					SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szParameters ), (LPARAM)g_Params.m_szParameters );
				}
			}
			break;
		case IDC_ABSOLUTESTART:
			{
				g_Params.m_bUsesTag = false;
				UpdateTagRadioButtons( &g_Params );
			}
			break;
		case IDC_RELATIVESTART:
			{
				g_Params.m_bUsesTag = true;
				UpdateTagRadioButtons( &g_Params );
			}
			break;
		case IDC_CHECK_LOOKAT:
			{
				HWND control = GetControl( IDC_CHECK_LOOKAT );
				bool checked = SendMessage( control, BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
				if ( !checked )
				{
					g_Params.yaw = 0;
					g_Params.pitch = 0;

					SetPitchYawText( &g_Params );

					control = GetControl( IDC_SLIDER_PITCH );
					SendMessage( control, TBM_SETPOS, 1, (LPARAM)(LONG)g_Params.pitch );
	
					control = GetControl( IDC_SLIDER_YAW );
					SendMessage( control, TBM_SETPOS, 1, (LPARAM)(LONG)g_Params.yaw );
				}

			}
			break;
		}
		return TRUE;
	case WM_HSCROLL:
		{
			HWND control = (HWND)lParam;
			if ( control == GetControl( IDC_SLIDER_YAW ) ||
				 control == GetControl( IDC_SLIDER_PITCH ) )
			{
				g_Params.yaw = (float)SendMessage( GetControl( IDC_SLIDER_YAW ), TBM_GETPOS, 0, 0 );
				g_Params.pitch = (float)SendMessage( GetControl( IDC_SLIDER_PITCH ), TBM_GETPOS, 0, 0 );

				SetPitchYawText( &g_Params );

				control = GetControl( IDC_CHECK_LOOKAT );
				if ( g_Params.pitch != 0 ||
					 g_Params.yaw != 0 )
				{
					g_Params.usepitchyaw = true;
				}
				else
				{
					g_Params.usepitchyaw = false;
				}
				
				SendMessage( control, BM_SETCHECK, (WPARAM) g_Params.usepitchyaw ? BST_CHECKED : BST_UNCHECKED, 0 );

				return TRUE;
			}
		}
		return FALSE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int EventProperties_LookAt( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_LOOKAT ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesLookAtDialogProc );

	*params = g_Params;

	return retval;
}