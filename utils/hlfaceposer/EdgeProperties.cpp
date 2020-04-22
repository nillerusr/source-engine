//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "EdgeProperties.h"
#include "mdlviewer.h"
#include "hlfaceposer.h"
#include "choreoevent.h"
#include "choreoscene.h"
#include "expressions.h"
#include "choreoactor.h"
#include "ifaceposersound.h"
#include "expclass.h"
#include "scriplib.h"

static CEdgePropertiesParams g_Params;

void CEdgePropertiesParams::SetFromFlexTrack( CFlexAnimationTrack *track )
{
	for ( int i = 0 ; i < 2; ++i )
	{
		m_bActive[ i ] = track->IsEdgeActive( i == 0 ? true : false );

		int curveType = 0;
		track->GetEdgeInfo( i == 0 ? true : false, curveType, m_flValue[ i ] );

		if ( i == 0 )
		{
			m_InterpolatorType[ i ] = GET_RIGHT_CURVE( curveType );
		}
		else
		{
			m_InterpolatorType[ i ] = GET_LEFT_CURVE( curveType );
		}
	}
}

void CEdgePropertiesParams::ApplyToTrack( CFlexAnimationTrack *track )
{
	for ( int i = 0 ; i < 2; ++i )
	{
		track->SetEdgeActive( i == 0 ? true : false, m_bActive[ i ] );

		int curveType = 0;
		if ( i == 0 )
		{
			curveType = MAKE_CURVE_TYPE( 0, m_InterpolatorType[ i ] );
		}
		else
		{
			curveType = MAKE_CURVE_TYPE( m_InterpolatorType[ i ], 0 );
		}

		track->SetEdgeInfo( i == 0 ? true : false, curveType, m_flValue[ i ] );
	}
}

void CEdgePropertiesParams::SetFromCurve( CCurveData *ramp )
{
	for ( int i = 0 ; i < 2; ++i )
	{
		m_bActive[ i ] = ramp->IsEdgeActive( i == 0 ? true : false );

		int curveType = 0;
		ramp->GetEdgeInfo( i == 0 ? true : false, curveType, m_flValue[ i ] );

		if ( i == 0 )
		{
			m_InterpolatorType[ i ] = GET_RIGHT_CURVE( curveType );
		}
		else
		{
			m_InterpolatorType[ i ] = GET_LEFT_CURVE( curveType );
		}
	}
}

void CEdgePropertiesParams::ApplyToCurve( CCurveData *ramp )
{
	for ( int i = 0 ; i < 2; ++i )
	{
		ramp->SetEdgeActive( i == 0 ? true : false, m_bActive[ i ] );

		int curveType = 0;
		if ( i == 0 )
		{
			curveType = MAKE_CURVE_TYPE( 0, m_InterpolatorType[ i ] );
		}
		else
		{
			curveType = MAKE_CURVE_TYPE( m_InterpolatorType[ i ], 0 );
		}

		ramp->SetEdgeInfo( i == 0 ? true : false, curveType, m_flValue[ i ] );
	}
}


static void PopulateCurveType( HWND control, CEdgePropertiesParams *params, bool isLeftEdge )
{
	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	for ( int i = 0; i < NUM_INTERPOLATE_TYPES; ++i )
	{
		SendMessage( control, CB_ADDSTRING, 0, (LPARAM)Interpolator_NameForInterpolator( i, true ) ); 
	}

	SendMessage( control, CB_SETCURSEL , params->m_InterpolatorType[ isLeftEdge ? 0 : 1 ], 0 );

}

static void Reset( HWND hwndDlg, bool left )
{
	SendMessage( GetDlgItem( hwndDlg, left ? IDC_LEFT_ACTIVE : IDC_RIGHT_ACTIVE ), BM_SETCHECK, 
				( WPARAM )BST_UNCHECKED,
				( LPARAM )0 );
	SendMessage( GetDlgItem( hwndDlg, left ? IDC_LEFT_CURVETYPE : IDC_RIGHT_CURVETYPE ), CB_SETCURSEL, 0, 0 ); 
	SetDlgItemText( hwndDlg, left ? IDC_LEFT_ZEROVALUE : IDC_RIGHT_ZEROVALUE, "0.0" );
	SendMessage( GetDlgItem( hwndDlg, IDC_HOLD_OUT ), BM_SETCHECK, 
				( WPARAM )BST_UNCHECKED,
				( LPARAM )0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK EdgePropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		{
			g_Params.PositionSelf( hwndDlg );
			
			PopulateCurveType( GetDlgItem( hwndDlg, IDC_LEFT_CURVETYPE ), &g_Params, true );
			PopulateCurveType( GetDlgItem( hwndDlg, IDC_RIGHT_CURVETYPE ), &g_Params, false );

			SetDlgItemText( hwndDlg, IDC_LEFT_ZEROVALUE, va( "%f", g_Params.m_flValue[ 0 ] ) );
			SetDlgItemText( hwndDlg, IDC_RIGHT_ZEROVALUE, va( "%f", g_Params.m_flValue[ 1 ] ) );

			SendMessage( GetDlgItem( hwndDlg, IDC_LEFT_ACTIVE ), BM_SETCHECK, 
				( WPARAM ) g_Params.m_bActive[ 0 ] ? BST_CHECKED : BST_UNCHECKED,
				( LPARAM )0 );

			SendMessage( GetDlgItem( hwndDlg, IDC_RIGHT_ACTIVE ), BM_SETCHECK, 
				( WPARAM ) g_Params.m_bActive[ 1 ] ? BST_CHECKED : BST_UNCHECKED,
				( LPARAM )0 );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_LEFT_ZEROVALUE ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_LEFT_RESET:
			{
				Reset( hwndDlg, true );
			}
			break;
		case IDC_RIGHT_RESET:
			{
				Reset( hwndDlg, false );
			}
			break;
		case IDC_LEFT_CURVETYPE:
			{
				if ( HIWORD( wParam ) == CBN_SELCHANGE )
				{
					SendMessage( GetDlgItem( hwndDlg, IDC_LEFT_ACTIVE ), BM_SETCHECK, 
						( WPARAM ) BST_CHECKED,
						( LPARAM )0 );
				}
			}
			break;
		case IDC_RIGHT_CURVETYPE:
			{
				if ( HIWORD( wParam ) == CBN_SELCHANGE )
				{
					SendMessage( GetDlgItem( hwndDlg, IDC_RIGHT_ACTIVE ), BM_SETCHECK, 
						( WPARAM ) BST_CHECKED,
						( LPARAM )0 );
				}
			}
			break;
		case IDC_LEFT_ZEROVALUE:
			{
				if ( HIWORD( wParam ) == EN_CHANGE )
				{
					SendMessage( GetDlgItem( hwndDlg, IDC_LEFT_ACTIVE ), BM_SETCHECK, 
						( WPARAM ) BST_CHECKED,
						( LPARAM )0 );
				}
			}
			break;
		case IDC_RIGHT_ZEROVALUE:
			{
				if ( HIWORD( wParam ) == EN_CHANGE )
				{
					SendMessage( GetDlgItem( hwndDlg, IDC_RIGHT_ACTIVE ), BM_SETCHECK, 
						( WPARAM ) BST_CHECKED,
						( LPARAM )0 );
				}
			}
			break;
		case IDOK:
			{
				char sz[ 64 ];
				GetDlgItemText( hwndDlg, IDC_LEFT_ZEROVALUE, sz, sizeof( sz ) );
				g_Params.m_flValue[ 0 ] = clamp( Q_atof( sz ), 0.0f, 1.0f );
				GetDlgItemText( hwndDlg, IDC_RIGHT_ZEROVALUE, sz, sizeof( sz ) );
				g_Params.m_flValue[ 1 ] =  clamp( Q_atof( sz ), 0.0f, 1.0f );

				g_Params.m_bActive[ 0 ] = SendMessage( GetDlgItem( hwndDlg, IDC_LEFT_ACTIVE ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
				g_Params.m_bActive[ 1 ] = SendMessage( GetDlgItem( hwndDlg, IDC_RIGHT_ACTIVE ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;

				int interpolatorType;
				interpolatorType = SendMessage(  GetDlgItem( hwndDlg, IDC_LEFT_CURVETYPE ), CB_GETCURSEL, 0, 0 );
				if ( interpolatorType != CB_ERR )
				{
					g_Params.m_InterpolatorType[ 0 ] = interpolatorType;
				}
				interpolatorType = SendMessage(  GetDlgItem( hwndDlg, IDC_RIGHT_CURVETYPE ), CB_GETCURSEL, 0, 0 );
				if ( interpolatorType != CB_ERR )
				{
					g_Params.m_InterpolatorType[ 1 ] = interpolatorType;
				}

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
int EdgeProperties( CEdgePropertiesParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EDGEPROPERTIES ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EdgePropertiesDialogProc );

	*params = g_Params;

	return retval;
}