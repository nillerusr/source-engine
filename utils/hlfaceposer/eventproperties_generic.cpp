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

static CEventParams g_Params;

class CEventPropertiesGenericDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:
	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );

private:

	void		PopulateAIGeneric( HWND control, CEventParams *params );
};

void CEventPropertiesGenericDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "Generic", "Generic AI Event (text)" );
}

void CEventPropertiesGenericDialog::InitControlData( CEventParams *params )
{
	BaseClass::InitControlData( params );

	HWND choices1 = GetControl( IDC_EVENTCHOICES );
	SendMessage( choices1, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices1, WM_SETTEXT , 0, (LPARAM)params->m_szParameters );

	HWND choices2 = GetControl( IDC_EVENTCHOICES2 );
	SendMessage( choices2, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices2, WM_SETTEXT , 0, (LPARAM)params->m_szParameters2 );

	SendMessage( GetControl( IDC_FILENAME ), WM_SETTEXT, sizeof( params->m_szParameters ), (LPARAM)params->m_szParameters );

	PopulateAIGeneric( choices1, params );
	PopulateNamedActorList( choices2, params );
}

void CEventPropertiesGenericDialog::InitDialog( HWND hwndDlg )
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

static CEventPropertiesGenericDialog g_EventPropertiesGenericDialog;

void CEventPropertiesGenericDialog::PopulateAIGeneric( HWND control, CEventParams *params )
{
	// FIXME: this should load from a config file
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_BLINK" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_HOLSTER" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_UNHOLSTER" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_AIM" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_RANDOMLOOK" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_RANDOMFACEFLEX" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_RANDOMHEADFLEX" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_IGNORECOLLISION" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"AI_DISABLEAI" );

	SendMessage( control, WM_SETTEXT , 0, (LPARAM)params->m_szParameters );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesGenericDialog::ShowControlsForEventType( CEventParams *params )
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
static BOOL CALLBACK EventPropertiesGenericDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesGenericDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

BOOL CEventPropertiesGenericDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
					Q_snprintf( g_Params.m_szName, sizeof( g_Params.m_szName ), "%s to %s", g_Params.m_szParameters, g_Params.m_szParameters2 );
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
int EventProperties_Generic( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_GENERIC ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesGenericDialogProc );

	*params = g_Params;

	return retval;
}