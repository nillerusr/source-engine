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
#include "choreoevent.h"
#include "filesystem.h"
#include <commctrl.h>
#include "scriplib.h"


static CEventParams g_Params;

class CEventPropertiesMoveToDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:
	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );

private:

	void		PopulateMovementStyle( HWND control, CEventParams *params );
	void		SetDistanceToTargetText( CEventParams *params );
};

void CEventPropertiesMoveToDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "MoveTo", "Move To Actor" );
}

void CEventPropertiesMoveToDialog::InitControlData( CEventParams *params )
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

	HWND choices2 = GetControl( IDC_EVENTCHOICES2 );
	SendMessage( choices2, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices2, WM_SETTEXT , 0, (LPARAM)params->m_szParameters2 );

	HWND choices3 = GetControl( IDC_EVENTCHOICES3 );
	SendMessage( choices3, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices3, WM_SETTEXT , 0, (LPARAM)params->m_szParameters3 );

	HWND control = GetControl( IDC_SLIDER_DISTANCE );
	SendMessage( control, TBM_SETRANGE, 0, (LPARAM)MAKELONG( 0, 200 ) );
	SendMessage( control, TBM_SETPOS, 1, (LPARAM)(LONG)params->m_flDistanceToTarget );

	SendMessage( GetControl( IDC_CHECK_FORCESHORTMOVEMENT ), BM_SETCHECK, 
		( WPARAM ) g_Params.m_bForceShortMovement ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );

	PopulateNamedActorList( choices1, params );

	SendMessage( GetControl( IDC_CHOICES2PROMPT ), WM_SETTEXT, 0, (LPARAM)"Movement Style:" );

	PopulateMovementStyle( choices2, params );

	if (strlen( params->m_szParameters3 ) != 0)
	{
		// make sure blank is a valid choice
		SendMessage( choices3, CB_ADDSTRING, 0, (LPARAM)"" ); 
	}
	PopulateNamedActorList( choices3, params );

	SetDistanceToTargetText( params );
}

void CEventPropertiesMoveToDialog::InitDialog( HWND hwndDlg )
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

static CEventPropertiesMoveToDialog g_EventPropertiesMoveToDialog;

void CEventPropertiesMoveToDialog::PopulateMovementStyle( HWND control, CEventParams *params )
{
	char movement_style[ 256 ];
	char distance_to_target[ 256 ];

	movement_style[0] = 0;
	distance_to_target[0]= 0;

	ParseFromMemory( params->m_szParameters2, strlen( params->m_szParameters2 ) );
	if ( TokenAvailable() )
	{
		GetToken( false );
		strcpy( movement_style, token );
		if ( TokenAvailable() )
		{
			GetToken( false );
			strcpy( distance_to_target, token );
		}
	}

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"Walk" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"Run" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CrouchWalk" );

	SendMessage( control, WM_SETTEXT , 0, (LPARAM)movement_style );
}


void CEventPropertiesMoveToDialog::SetDistanceToTargetText( CEventParams *params )
{
	HWND control;

	control = GetControl( IDC_STATIC_DISTANCEVAL );
	SendMessage( control, WM_SETTEXT , 0, (LPARAM)va( "%i", (int)params->m_flDistanceToTarget ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesMoveToDialog::ShowControlsForEventType( CEventParams *params )
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
static BOOL CALLBACK EventPropertiesMoveToDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesMoveToDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

BOOL CEventPropertiesMoveToDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
		
	case WM_HSCROLL:
		{
			HWND control = (HWND)lParam;
			if ( control == GetControl( IDC_SLIDER_DISTANCE ))
			{
				g_Params.m_flDistanceToTarget = (float)SendMessage( GetControl( IDC_SLIDER_DISTANCE ), TBM_GETPOS, 0, 0 );

				SetDistanceToTargetText( &g_Params );
				return TRUE;
			}
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
					Q_snprintf( g_Params.m_szName, sizeof( g_Params.m_szName ), "Moveto %s", g_Params.m_szParameters );
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
		case IDC_EVENTCHOICES2:
			{
				HWND control = (HWND)lParam;
				if ( control )
				{
					if ( g_Params.m_nType != CChoreoEvent::MOVETO )
					{
						SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szParameters2 ), (LPARAM)g_Params.m_szParameters2 );
					}
					else
					{
						char buf1[ 256 ];
						SendMessage( GetControl( IDC_EVENTCHOICES2 ), WM_GETTEXT, (WPARAM)sizeof( buf1 ), (LPARAM)buf1 );

						Q_snprintf( g_Params.m_szParameters2, sizeof( g_Params.m_szParameters2 ), "%s", buf1 );
					}
				}
			}
			break;
		case IDC_EVENTCHOICES3:
			{
				HWND control = (HWND)lParam;
				if ( control )
				{
					if ( g_Params.m_nType != CChoreoEvent::MOVETO )
					{
						SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szParameters3 ), (LPARAM)g_Params.m_szParameters3 );
					}
					else
					{
						char buf1[ 256 ];
						SendMessage( GetControl( IDC_EVENTCHOICES3 ), WM_GETTEXT, (WPARAM)sizeof( buf1 ), (LPARAM)buf1 );

						Q_snprintf( g_Params.m_szParameters3, sizeof( g_Params.m_szParameters3 ), "%s", buf1 );
					}
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
		case IDC_CHECK_FORCESHORTMOVEMENT:
			{
				g_Params.m_bForceShortMovement = SendMessage( GetControl( IDC_CHECK_FORCESHORTMOVEMENT ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
			}
			break;
		default:
			return FALSE;
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
int EventProperties_MoveTo( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_MOVETO ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesMoveToDialogProc );

	*params = g_Params;

	return retval;
}