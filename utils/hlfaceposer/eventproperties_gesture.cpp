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
#include "StudioModel.h"
#include "faceposer_models.h"
#include "KeyValues.h"

static CEventParams g_Params;

class CEventPropertiesGestureDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:
	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );

private:
	void		PopulateGestureList( HWND wnd );

	bool		CheckSequenceType( StudioModel *model, int iSequence, char *szType );
};

void CEventPropertiesGestureDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "Gesture", "Gesture" );
}

void CEventPropertiesGestureDialog::PopulateGestureList( HWND wnd )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if (hdr)
	{
		int i;
		for (i = 0; i < hdr->GetNumSeq(); i++)
		{
			if (CheckSequenceType( models->GetActiveStudioModel(), i, "gesture" ))
			{
				SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)hdr->pSeqdesc(i).pszLabel() ); 
			}
		}
		for (i = 0; i < hdr->GetNumSeq(); i++)
		{
			if (CheckSequenceType( models->GetActiveStudioModel(), i, "posture" ))
			{
				SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)hdr->pSeqdesc(i).pszLabel() ); 
			}
		}
	}
}

void CEventPropertiesGestureDialog::InitControlData( CEventParams *params )
{
	BaseClass::InitControlData( params );

	HWND choices1 = GetControl( IDC_EVENTCHOICES );
	SendMessage( choices1, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices1, WM_SETTEXT , 0, (LPARAM)params->m_szParameters );

	SendMessage( GetControl( IDC_CHECK_SYNCTOFOLLOWINGGESTURE ), BM_SETCHECK, 
		( WPARAM ) g_Params.m_bSyncToFollowingGesture ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );

	PopulateGestureList( choices1 );
}

void CEventPropertiesGestureDialog::InitDialog( HWND hwndDlg )
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

static CEventPropertiesGestureDialog g_EventPropertiesGestureDialog;

bool CEventPropertiesGestureDialog::CheckSequenceType( StudioModel *model, int iSequence, char *szType )
{
	KeyValues *seqKeyValues = new KeyValues("");
	bool isType = false;
	if ( seqKeyValues->LoadFromBuffer( model->GetFileName( ), model->GetKeyValueText( iSequence ) ) )
	{
		// Do we have a build point section?
		KeyValues *pkvAllFaceposer = seqKeyValues->FindKey("faceposer");
		if ( pkvAllFaceposer )
		{
			KeyValues *pkvType = pkvAllFaceposer->FindKey("type");

			if (pkvType)
			{
				isType = (stricmp( pkvType->GetString(), szType ) == 0) ? true : false;
			}
		}
	}

	seqKeyValues->deleteThis();

	return isType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesGestureDialog::ShowControlsForEventType( CEventParams *params )
{
	BaseClass::ShowControlsForEventType( params );

	// NULL Gesture doesn't have these controls either
	if ( g_Params.m_nType == CChoreoEvent::GESTURE && 
		!Q_stricmp( g_Params.m_szName, "NULL" ) )
	{
		ShowWindow( GetControl( IDC_EVENTNAME ), SW_HIDE );
		ShowWindow( GetControl( IDC_TAGS ), SW_HIDE );
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
static BOOL CALLBACK EventPropertiesGestureDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesGestureDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

BOOL CEventPropertiesGestureDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
					Q_strncpy( g_Params.m_szName, g_Params.m_szParameters, sizeof( g_Params.m_szName ) );
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
		case IDC_CHECK_SYNCTOFOLLOWINGGESTURE:
			{
				g_Params.m_bSyncToFollowingGesture = SendMessage( GetControl( IDC_CHECK_SYNCTOFOLLOWINGGESTURE ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
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
int EventProperties_Gesture( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_GESTURE ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesGestureDialogProc );

	*params = g_Params;

	return retval;
}