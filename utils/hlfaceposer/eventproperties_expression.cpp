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
#include "expressions.h"
#include "expclass.h"

static CEventParams g_Params;

class CEventPropertiesExpressionDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:
	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );
private:

	void		PopulateExpressionList( HWND wnd );
	void		PopulateExpressionClass( HWND control, CEventParams *params );
};

void CEventPropertiesExpressionDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "Expression", "Expression" );
}

void CEventPropertiesExpressionDialog::PopulateExpressionList( HWND wnd )
{
	for ( int i = 0 ; i < expressions->GetNumClasses() ; i++ )
	{
		CExpClass *cl = expressions->GetClass( i );
		if( !cl )
			continue;

		// add text to combo box
		SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)cl->GetName() ); 
	}
}

void CEventPropertiesExpressionDialog::InitControlData( CEventParams *params )
{
	BaseClass::InitControlData( params );
	
	HWND choices1 = GetControl( IDC_EVENTCHOICES );
	SendMessage( choices1, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices1, WM_SETTEXT , 0, (LPARAM)params->m_szParameters );

	HWND choices2 = GetControl( IDC_EVENTCHOICES2 );
	SendMessage( choices2, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices2, WM_SETTEXT , 0, (LPARAM)params->m_szParameters2 );

	PopulateExpressionList( choices1 );
	SendMessage( GetControl( IDC_CHOICES2PROMPT ), WM_SETTEXT, 0, (LPARAM)"Name:" );
	PopulateExpressionClass( choices2, params );
}

void CEventPropertiesExpressionDialog::InitDialog( HWND hwndDlg )
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

static CEventPropertiesExpressionDialog g_EventPropertiesExpressionDialog;

void CEventPropertiesExpressionDialog::PopulateExpressionClass( HWND control, CEventParams *params )
{
	// Find parameter 1
	for ( int c = 0; c < expressions->GetNumClasses(); c++ )
	{
		CExpClass *cl = expressions->GetClass( c );
		if ( !cl )
			continue;

		if ( Q_stricmp( cl->GetName(), params->m_szParameters ) )
			continue;

		for ( int i = 0 ; i < cl->GetNumExpressions() ; i++ )
		{
			CExpression *exp = cl->GetExpression( i );
			if ( exp )
			{
				// add text to combo box
				SendMessage( control, CB_ADDSTRING, 0, (LPARAM)exp->name ); 
			}
		}
		
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesExpressionDialog::ShowControlsForEventType( CEventParams *params )
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
static BOOL CALLBACK EventPropertiesExpressionDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesExpressionDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

BOOL CEventPropertiesExpressionDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
					Q_snprintf( g_Params.m_szName, sizeof( g_Params.m_szName ), "%s/%s", g_Params.m_szParameters, g_Params.m_szParameters2 );
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

					PopulateExpressionClass( GetControl( IDC_EVENTCHOICES2 ), &g_Params );
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
int EventProperties_Expression( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_EXPRESSION ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesExpressionDialogProc );

	*params = g_Params;

	return retval;
}