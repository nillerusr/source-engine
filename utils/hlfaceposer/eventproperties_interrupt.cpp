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

static CEventParams g_Params;

class CEventPropertiesInterruptDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:
	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );
};

void CEventPropertiesInterruptDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "Interrupt", "Interrupt" );
}

void CEventPropertiesInterruptDialog::InitControlData( CEventParams *params )
{
	BaseClass::InitControlData( params );

	HWND choices1 = GetControl( IDC_EVENTCHOICES );
	SendMessage( choices1, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices1, WM_SETTEXT , 0, (LPARAM)params->m_szParameters );
}

void CEventPropertiesInterruptDialog::InitDialog( HWND hwndDlg )
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

static CEventPropertiesInterruptDialog g_EventPropertiesInterruptDialog;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesInterruptDialog::ShowControlsForEventType( CEventParams *params )
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
static BOOL CALLBACK EventPropertiesInterruptDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesInterruptDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

BOOL CEventPropertiesInterruptDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
					Q_strncpy( g_Params.m_szName, "Interrupt", sizeof( g_Params.m_szName ) );
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
int EventProperties_Interrupt( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_INTERRUPT ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesInterruptDialogProc );

	*params = g_Params;

	return retval;
}