//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "hammer.h"
#include "MainFrm.h"
#include "FaceEditSheet.h"
#include "GlobalFunctions.h"
#include "DispDlg.h"
#include "MapFace.h"
#include "MapDisp.h"
#include "ToolDisplace.h"
#include "ToolManager.h"
#include "SculptOptions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define DISPPAINT_DISTANCE_MIN			0
#define DISPPAINT_DISTANCE_MAX			60
#define DISPPAINT_SPATIALRADIUS_MIN		1
#define DISPPAINT_SPATIALRADIUS_MAX		1024
#define DISPPAINT_SPATIALRADIUS_STEP	16

//=============================================================================
//
// Displacement Create Dialog Functions
//
BEGIN_MESSAGE_MAP(CDispCreateDlg, CDialog)
	//{{AFX_MSG_MAP(CDispCreateDlg)
	ON_WM_VSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

extern CToolDisplace* GetDisplacementTool();

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CDispCreateDlg::CDispCreateDlg( CWnd *pParent ) :
	CDialog( CDispCreateDlg::IDD, pParent )
{
	m_Power = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CDispCreateDlg::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	// set the initial power "3"
	SetDlgItemInt( ID_DISP_CREATE_POWER, 3 );

	// setup the spinner - set range (range [2..4])
	m_spinPower.SetBuddy( &m_editPower );
	m_spinPower.SetRange( 2, 4 );
	m_spinPower.SetPos( 3 );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispCreateDlg::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CDispCreateDlg)
	DDX_Control( pDX, ID_DISP_CREATE_POWER_SPIN, m_spinPower );
	DDX_Control( pDX, ID_DISP_CREATE_POWER, m_editPower );
	DDX_Text( pDX, ID_DISP_CREATE_POWER, m_Power );
	//}}AFX_DATA_MAP

	// clamp the power
	if( m_Power < 2 ) { m_Power = 2; }
	if( m_Power > 4 ) { m_Power = 4; }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispCreateDlg::OnVScroll( UINT nSBCode, UINT nPos, CScrollBar *pScrollBar ) 
{
	m_spinPower.SetPos( nPos );
	SetDlgItemInt( ID_DISP_CREATE_POWER, nPos );
}


//=============================================================================
//
// Displacement Noise Dialog Functions
//
BEGIN_MESSAGE_MAP(CDispNoiseDlg, CDialog)
	//{{AFX_MSG_MAP(CDispNoiseDlg)
	ON_NOTIFY( UDN_DELTAPOS, ID_DISP_NOISE_MIN_SPIN, OnSpinUpDown )
	ON_NOTIFY( UDN_DELTAPOS, ID_DISP_NOISE_MAX_SPIN, OnSpinUpDown )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CDispNoiseDlg::CDispNoiseDlg( CWnd *pParent ) :
	CDialog( CDispNoiseDlg::IDD, pParent )
{
	m_Min = m_Max = 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CDispNoiseDlg::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	//
	// set min, max initially to zero!!
	//
	CString strZero = "0.0";
	SetDlgItemText( ID_DISP_NOISE_MIN, strZero );
	SetDlgItemText( ID_DISP_NOISE_MAX, strZero );

	//
	// setup spinners
	//
	m_spinMin.SetBuddy( &m_editMin );
	m_spinMax.SetBuddy( &m_editMax );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispNoiseDlg::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CDispNoiseDlg)
	DDX_Control( pDX, ID_DISP_NOISE_MIN_SPIN, m_spinMin );
	DDX_Control( pDX, ID_DISP_NOISE_MAX_SPIN, m_spinMax );
	DDX_Control( pDX, ID_DISP_NOISE_MIN, m_editMin );
	DDX_Control( pDX, ID_DISP_NOISE_MAX, m_editMax );
	DDX_Text( pDX, ID_DISP_NOISE_MIN, m_Min );
	DDX_Text( pDX, ID_DISP_NOISE_MAX, m_Max );
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispNoiseDlg::OnSpinUpDown( NMHDR *pNMHDR, LRESULT *pResult ) 
{
	//
	// get scroll up down edit box
	//
	NM_UPDOWN *pNMUpDown = ( NM_UPDOWN* )pNMHDR;
	switch( pNMUpDown->hdr.idFrom )
	{
		case ID_DISP_NOISE_MIN_SPIN:
		{
			CEdit *pEdit = ( CEdit* )GetDlgItem( ID_DISP_NOISE_MIN );
			CString strMin;
			pEdit->GetWindowText( strMin );
			m_Min = atof( strMin );
			m_Min += 0.5f * ( -pNMUpDown->iDelta );
			strMin.Format( "%4.2f", m_Min );
			pEdit->SetWindowText( strMin );
			*pResult = 0;
			break;
		}

		case ID_DISP_NOISE_MAX_SPIN:
		{
			CEdit *pEdit = ( CEdit* )GetDlgItem( ID_DISP_NOISE_MAX );
			CString strMax;
			pEdit->GetWindowText( strMax );
			m_Max = atof( strMax );
			m_Max += 0.5f * ( -pNMUpDown->iDelta );
			strMax.Format( "%4.2f", m_Max );
			pEdit->SetWindowText( strMax );
			*pResult = 0;
			break;
		}
	}
}


//=============================================================================
//
// Displacement Paint Distance Dialog Functions
//
BEGIN_MESSAGE_MAP(CDispPaintDistDlg, CDialog)
	//{{AFX_MSG_MAP(CDispPaintDistDlg)
	ON_BN_CLICKED( ID_DISP_PAINT_DIST_RAISELOWER, OnEffectRaiseLowerGeo )
	ON_BN_CLICKED( ID_DISP_PAINT_DIST_RAISETO, OnEffectRaiseToGeo )
	ON_BN_CLICKED( ID_DISP_PAINT_DIST_SMOOTH, OnEffectSmoothGeo )

	ON_BN_CLICKED( ID_DISPPAINT_SOFTEDGE, OnBrushTypeSoftEdge )
	ON_BN_CLICKED( ID_DISPPAINT_HARDEDGE, OnBrushTypeHardEdge )

	ON_BN_CLICKED( ID_DISP_PAINT_DIST_SPATIAL, OnCheckSpatial )
	ON_BN_CLICKED( ID_DISP_PAINT_DIST_AUTOSEW, OnCheckAutoSew )

	ON_CBN_SELCHANGE( ID_DISP_PAINT_DIST_BRUSH, OnComboBoxBrushGeo )
	ON_CBN_SELCHANGE( ID_DISP_PAINT_DIST_AXIS, OnComboBoxAxis )

	ON_WM_HSCROLL()
	ON_EN_CHANGE( ID_DISP_PAINT_DIST_EDIT_DISTANCE, OnEditDistance )
	ON_EN_CHANGE( ID_DISP_PAINT_DIST_EDIT_RADIUS, OnEditRadius )

	ON_WM_CLOSE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CDispPaintDistDlg::CDispPaintDistDlg( CWnd *pParent ) :
	CDialog( CDispPaintDistDlg::IDD, pParent )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDispPaintDistDlg::~CDispPaintDistDlg()
{
	if ( m_comboboxBrush.m_hWnd )
	{
		m_comboboxBrush.Detach();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CDispPaintDistDlg::OnInitDialog( void )
{
	static bool bInit = false;

	CDialog::OnInitDialog();

	CToolDisplace *pTool = GetDisplacementTool();
	if ( !pTool )
		return FALSE;

	// Set spatial tool flag.
	if ( !pTool->IsSpatialPainting() )
	{
		pTool->ToggleSpatialPainting();
	}

	if ( !bInit )
	{
		m_flPrevDistance = 1.0f;
		m_flPrevRadius = 1.0f;
		m_nPrevBrush = 0;
		m_nPrevEffect = pTool->GetEffect();
		pTool->GetPaintAxis( m_nPrevPaintAxis, m_vecPrevPaintAxis );
		bInit = true;
	}
	else
	{
		SetWindowPos( &wndTop, m_DialogPosRect.left, m_DialogPosRect.top,
			          m_DialogPosRect.Width(), m_DialogPosRect.Height(), SWP_NOZORDER );
	}

	// Initialize the combo boxes.
	InitComboBoxBrushGeo();
	InitComboBoxAxis();
	// Initialize the sliders.
	InitDistance();
	InitRadius();

	// Initialize the brush types.
	InitBrushType();

	return TRUE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::InitDistance( void )
{
	// Set the slider range and initialize the "buddy."
	m_sliderDistance.SetBuddy( &m_editDistance, FALSE );
	m_sliderDistance.SetRange( DISPPAINT_DISTANCE_MIN, DISPPAINT_DISTANCE_MAX );

	// Get the displacement tool.
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->GetChannel( DISPPAINT_CHANNEL_POSITION, m_flPrevDistance );

		// Initialize the distance slider and edit box.
		UpdateSliderDistance( m_flPrevDistance, true );
		UpdateEditBoxDistance( m_flPrevDistance, true );
	}
	else
	{
		// Init distance slider and edit box.
		UpdateSliderDistance( 1.0f, true );
		UpdateEditBoxDistance( 1.0f, true );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::InitRadius( void )
{
	// Set the slider range and initialize the "buddy."
	m_sliderRadius.SetBuddy( &m_editRadius, FALSE );
	m_sliderRadius.SetRange( DISPPAINT_SPATIALRADIUS_MIN, DISPPAINT_SPATIALRADIUS_MAX );
	m_sliderRadius.SetTicFreq( DISPPAINT_SPATIALRADIUS_STEP );
	// Get the displacement tool.
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		CButton *pcheckSpatial = ( CButton* )GetDlgItem( ID_DISP_PAINT_DIST_SPATIAL );
		if ( pTool->IsSpatialPainting() )
		{
			pcheckSpatial->SetCheck( true );
			EnableSliderRadius();
			DisablePaintingComboBoxes();
		}
		else
		{
			pcheckSpatial->SetCheck( false );
			DisableSliderRadius();
			EnablePaintingComboBoxes();
		}
	}
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::EnableSliderRadius( void )
{
	// Enable the radius slider and edit box.
	m_sliderRadius.EnableWindow( TRUE );
	m_editRadius.EnableWindow( TRUE );

	// Get the displacement tool and restore the radius.
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		m_flPrevRadius = pTool->GetSpatialRadius();

		// Update the radius slider and edit box.
		UpdateSliderRadius( m_flPrevRadius, true );
		UpdateEditBoxRadius( m_flPrevRadius, true );
	}
	else
	{
		// Set the radius slider and edit box with default values.
		UpdateSliderRadius( 1.0f, true );
		UpdateEditBoxRadius( 1.0f, true );
	}
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::DisableSliderRadius( void )
{
	// Disable the radius slider and edit box.
	m_sliderRadius.EnableWindow( FALSE );
	m_editRadius.EnableWindow( FALSE );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::UpdateSpatialData( void )
{
	// Get the displacement tool and restore the radius.
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		m_flPrevRadius = pTool->GetSpatialRadius();

		// Update the radius slider and edit box.
		UpdateSliderRadius( m_flPrevRadius, true );
		UpdateEditBoxRadius( m_flPrevRadius, true );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispPaintDistDlg::InitComboBoxBrushGeo( void )
{
	//
	// get the displacement paint brush icon combo box
	//
	m_comboboxBrush.Attach( GetDlgItem( ID_DISP_PAINT_DIST_BRUSH )->m_hWnd );
	m_comboboxBrush.Init();

	// reset the size of the combo box list item
	m_comboboxBrush.SetItemHeight( -1, m_comboboxBrush.m_IconSize.cy + 2 );

	// initialize the radio button/brush combo box geometry data
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->SetEffect( m_nPrevEffect );

		switch ( m_nPrevEffect )
		{
		case DISPPAINT_EFFECT_RAISELOWER:
			{
				pTool->SetEffect( DISPPAINT_EFFECT_RAISELOWER );
				SetEffectButtonGeo( DISPPAINT_EFFECT_RAISELOWER );
				FilterComboBoxBrushGeo( DISPPAINT_EFFECT_RAISELOWER, true );
				break;
			}
		case DISPPAINT_EFFECT_RAISETO: 
			{ 
				pTool->SetEffect( DISPPAINT_EFFECT_RAISETO );
				SetEffectButtonGeo( DISPPAINT_EFFECT_RAISETO );
				FilterComboBoxBrushGeo( DISPPAINT_EFFECT_RAISETO, true );
				break; 
			}
		case DISPPAINT_EFFECT_SMOOTH: 
			{  
				pTool->SetEffect( DISPPAINT_EFFECT_SMOOTH );
				SetEffectButtonGeo( DISPPAINT_EFFECT_SMOOTH );
				FilterComboBoxBrushGeo( DISPPAINT_EFFECT_SMOOTH, true );
				break; 
			}
		default: 
			{ 
				return false; 
			}
		}

		OnComboBoxBrushGeo();
	}
	else
	{
		OnEffectRaiseLowerGeo();
		OnComboBoxBrushGeo();
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::InitBrushType( void )
{
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		unsigned int uiBrushType = pTool->GetBrushType();
		switch ( uiBrushType )
		{
		case DISPPAINT_BRUSHTYPE_SOFT:
			{
				SetBrushTypeButtonGeo( DISPPAINT_BRUSHTYPE_SOFT );
				break;
			}
		case DISPPAINT_BRUSHTYPE_HARD:
			{
				SetBrushTypeButtonGeo( DISPPAINT_BRUSHTYPE_HARD );
				break;
			}
		}

		if ( pTool->IsSpatialPainting() )
		{
			EnableBrushTypeButtons();
		}
		else
		{
			DisableBrushTypeButtons();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::FilterComboBoxBrushGeo( unsigned int nEffect, bool bInit  )
{
	//
	// remove all the old combo box data
	//
	int count = m_comboboxBrush.GetCount();
	for ( int ndx = count - 1; ndx >= 0; ndx-- )
	{
		m_comboboxBrush.DeleteIcon( ndx );
	}

	//
	// add the new combo box data based on the current paint "effect"
	//
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		CDispMapImageFilterManager *pFilterMgr;
		switch ( nEffect )
		{
		case DISPPAINT_EFFECT_RAISELOWER: { pFilterMgr = pTool->GetFilterRaiseLowerMgr(); break; }
		case DISPPAINT_EFFECT_RAISETO: { pFilterMgr = pTool->GetFilterRaiseToMgr(); break; }
		case DISPPAINT_EFFECT_SMOOTH: { pFilterMgr = pTool->GetFilterSmoothMgr(); break; }
		default: return;
		}

		if( pFilterMgr )
		{
			//
			// for each filter - add its icon to the icon combo box
			//
			for ( int iFilter = 0; iFilter < pFilterMgr->GetFilterCount(); iFilter++ )
			{
				// get the current filter
				CDispMapImageFilter *pFilter = pFilterMgr->GetFilter( iFilter );

				// get the application directory
				char appDir[MAX_PATH];
				APP()->GetDirectory( DIR_PROGRAM, appDir );
				
				// append the filters directory name
				strcat( appDir, "filters\\" );
				
				// append the directory prefix to the icon name
				CString iconFilename = appDir + pFilter->m_Name;
				
				// add the icon to the icon combo box
				m_comboboxBrush.AddIcon( iconFilename );
			}

			// set initial paint brush
			if( bInit )
			{
				m_comboboxBrush.SetCurSel( m_nPrevBrush );
			}
			else
			{
				m_comboboxBrush.SetCurSel( 0 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispPaintDistDlg::InitComboBoxAxis( void )
{
	//
	// add the paint types to the combo box -- keep them in their "defined" order
	//
	CString strPaintDir;

	// axial x direction
	strPaintDir = "X-Axis";
	m_comboboxAxis.AddString( strPaintDir );

	// axial y direction
	strPaintDir = "Y-Axis";
	m_comboboxAxis.AddString( strPaintDir );

	// axial z direction
	strPaintDir = "Z-Axis";
	m_comboboxAxis.AddString( strPaintDir );

	// subdivision direction
	strPaintDir = "Subdiv Normal";
	m_comboboxAxis.AddString( strPaintDir );

	// face normal direction
	strPaintDir = "Face Normal";
	m_comboboxAxis.AddString( strPaintDir );

	// set initial value
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		m_comboboxAxis.SetCurSel( m_nPrevPaintAxis );
		pTool->SetPaintAxis( m_nPrevPaintAxis, m_vecPrevPaintAxis );		
	}
	else
	{
		m_comboboxAxis.SetCurSel( 4 );
		OnComboBoxAxis();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::EnablePaintingComboBoxes( void )
{
	m_comboboxBrush.EnableWindow( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::DisablePaintingComboBoxes( void )
{
	m_comboboxBrush.EnableWindow( FALSE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::EnableBrushTypeButtons( void )
{
	CButton *pRadioButton;
	pRadioButton = ( CButton* )GetDlgItem( ID_DISPPAINT_SOFTEDGE );
	pRadioButton->EnableWindow( TRUE );
	pRadioButton = ( CButton* )GetDlgItem( ID_DISPPAINT_HARDEDGE );
	pRadioButton->EnableWindow( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::DisableBrushTypeButtons( void )
{
	CButton *pRadioButton;
	pRadioButton = ( CButton* )GetDlgItem( ID_DISPPAINT_SOFTEDGE );
	pRadioButton->EnableWindow( FALSE );
	pRadioButton = ( CButton* )GetDlgItem( ID_DISPPAINT_HARDEDGE );
	pRadioButton->EnableWindow( FALSE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CDispPaintDistDlg)
	DDX_Control( pDX, ID_DISP_PAINT_DIST_SLIDER_DISTANCE, m_sliderDistance );
	DDX_Control( pDX, ID_DISP_PAINT_DIST_SLIDER_RADIUS, m_sliderRadius );
	DDX_Control( pDX, ID_DISP_PAINT_DIST_EDIT_DISTANCE, m_editDistance );
	DDX_Control( pDX, ID_DISP_PAINT_DIST_EDIT_RADIUS, m_editRadius );
	DDX_Control( pDX, ID_DISP_PAINT_DIST_AXIS, m_comboboxAxis );
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnComboBoxBrushGeo( void )
{
	// get the displacement's filter manager
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		// get current selection
		int iSel = m_comboboxBrush.GetCurSel();
		if ( iSel == LB_ERR )
			return;

		unsigned int nEffect = pTool->GetEffect();
		CDispMapImageFilterManager *pFilterMgr;
		switch ( nEffect )
		{
		case DISPPAINT_EFFECT_RAISELOWER: { pFilterMgr = pTool->GetFilterRaiseLowerMgr(); break; }
		case DISPPAINT_EFFECT_RAISETO: { pFilterMgr = pTool->GetFilterRaiseToMgr(); break; }
		case DISPPAINT_EFFECT_SMOOTH: { pFilterMgr = pTool->GetFilterSmoothMgr(); break; }
		default: return;
		}

		if ( pFilterMgr )
		{
			pFilterMgr->SetActiveFilter( iSel );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnComboBoxAxis( void )
{
	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		//
		// get the current paint type selection
		//
		int ndxSel = m_comboboxAxis.GetCurSel();
		if ( ndxSel == LB_ERR )
			return;

		// update the paint type
		UpdateAxis( ndxSel );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::UpdateAxis( int nAxis )
{
	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if ( !pTool )
		return;

	//
	// update the paint type - direction
	//
	switch ( nAxis )
	{
	case DISPPAINT_AXIS_X: { pTool->SetPaintAxis( nAxis, Vector( 1.0f, 0.0f, 0.0f ) ); return; }
	case DISPPAINT_AXIS_Y: { pTool->SetPaintAxis( nAxis, Vector( 0.0f, 1.0f, 0.0f ) ); return; }
	case DISPPAINT_AXIS_Z: { pTool->SetPaintAxis( nAxis, Vector( 0.0f, 0.0f, 1.0f ) ); return; }
	case DISPPAINT_AXIS_SUBDIV: { pTool->SetPaintAxis( nAxis, Vector( 0.0f, 0.0f, 0.0f ) ); return; }
	case DISPPAINT_AXIS_FACE: { pTool->SetPaintAxis( nAxis, Vector( 0.0f, 0.0f, 1.0f ) ); return; }
	default: { return; }
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnCheckAutoSew( void )
{
	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->ToggleAutoSew();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnCheckSpatial( void )
{
	// Get the displacement tool and toggle the spatial painting bit.
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->ToggleSpatialPainting();
		if ( pTool->IsSpatialPainting() )
		{
			EnableSliderRadius();
			DisablePaintingComboBoxes();
			EnableBrushTypeButtons();
		}
		else
		{
			DisableSliderRadius();
			EnablePaintingComboBoxes();
			DisableBrushTypeButtons();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::SetEffectButtonGeo( unsigned int nEffect )
{
	CButton *radiobutton;
	radiobutton = ( CButton* )GetDlgItem( ID_DISP_PAINT_DIST_RAISELOWER );
	radiobutton->SetCheck( nEffect == DISPPAINT_EFFECT_RAISELOWER );
	radiobutton = ( CButton* )GetDlgItem( ID_DISP_PAINT_DIST_RAISETO );
	radiobutton->SetCheck( nEffect == DISPPAINT_EFFECT_RAISETO );
	radiobutton = ( CButton* )GetDlgItem( ID_DISP_PAINT_DIST_SMOOTH ); 
	radiobutton->SetCheck( nEffect == DISPPAINT_EFFECT_SMOOTH );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::SetBrushTypeButtonGeo( unsigned int uiBrushType )
{
	CButton *pRadioButton;
	pRadioButton = ( CButton* )GetDlgItem( ID_DISPPAINT_SOFTEDGE );
	pRadioButton->SetCheck( uiBrushType == DISPPAINT_BRUSHTYPE_SOFT );
	pRadioButton = ( CButton* )GetDlgItem( ID_DISPPAINT_HARDEDGE );
	pRadioButton->SetCheck( uiBrushType == DISPPAINT_BRUSHTYPE_HARD );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnEffectRaiseLowerGeo( void )
{
	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->SetEffect( DISPPAINT_EFFECT_RAISELOWER );
		SetEffectButtonGeo( DISPPAINT_EFFECT_RAISELOWER );
		FilterComboBoxBrushGeo( DISPPAINT_EFFECT_RAISELOWER, false );
		OnComboBoxBrushGeo();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnEffectRaiseToGeo( void )
{
	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if( pTool )
	{
		pTool->SetEffect( DISPPAINT_EFFECT_RAISETO );
		SetEffectButtonGeo( DISPPAINT_EFFECT_RAISETO );
		FilterComboBoxBrushGeo( DISPPAINT_EFFECT_RAISETO, false );
		OnComboBoxBrushGeo();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnEffectSmoothGeo( void )
{
	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if( pTool )
	{
		pTool->SetEffect( DISPPAINT_EFFECT_SMOOTH );
		SetEffectButtonGeo( DISPPAINT_EFFECT_SMOOTH );
		FilterComboBoxBrushGeo( DISPPAINT_EFFECT_SMOOTH, false );
		OnComboBoxBrushGeo();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnBrushTypeSoftEdge( void )
{
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->SetBrushType( DISPPAINT_BRUSHTYPE_SOFT );
		SetBrushTypeButtonGeo( DISPPAINT_BRUSHTYPE_SOFT );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnBrushTypeHardEdge( void )
{
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		pTool->SetBrushType( DISPPAINT_BRUSHTYPE_HARD );
		SetBrushTypeButtonGeo( DISPPAINT_BRUSHTYPE_HARD );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::UpdateSliderDistance( float flDistance, bool bForceInit )
{
	if ( ( flDistance != m_flPrevDistance ) || bForceInit )
	{
		int nDistance = ( int )flDistance;

		// clamp
		if( nDistance < DISPPAINT_DISTANCE_MIN ) { nDistance = DISPPAINT_DISTANCE_MIN; }
		if( nDistance > DISPPAINT_DISTANCE_MAX ) { nDistance = DISPPAINT_DISTANCE_MAX; }

		m_sliderDistance.SetPos( nDistance );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::UpdateEditBoxDistance( float flDistance, bool bForceInit )
{
	if ( ( flDistance != m_flPrevDistance ) || bForceInit )
	{
		CString strDistance;
		strDistance.Format( "%4.2f", flDistance );
		m_editDistance.SetWindowText( strDistance );
	}
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::UpdateSliderRadius( float flRadius, bool bForceInit )
{
	if ( ( flRadius != m_flPrevRadius ) || bForceInit )
	{
		int nRadius = ( int )flRadius;

		// clamp
		if( nRadius < DISPPAINT_SPATIALRADIUS_MIN ) { nRadius = DISPPAINT_SPATIALRADIUS_MIN; }
		if( nRadius > DISPPAINT_SPATIALRADIUS_MAX ) { nRadius = DISPPAINT_SPATIALRADIUS_MAX; }

		m_sliderRadius.SetPos( nRadius );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::UpdateEditBoxRadius( float flRadius, bool bForceInit )
{
	if ( ( flRadius != m_flPrevRadius ) || bForceInit )
	{
		CString strRadius;
		strRadius.Format( "%4.2f", flRadius );
		m_editRadius.SetWindowText( strRadius );
	}
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnHScroll( UINT nSBCode, UINT nPos, CScrollBar *pScrollBar ) 
{
	// Get the displacement tool.
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		// Get the distance slider control.
		CSliderCtrl *pDistSlider = ( CSliderCtrl* )GetDlgItem( ID_DISP_PAINT_DIST_SLIDER_DISTANCE );
		if ( pDistSlider )
		{
			// Get the slider position.
			int nDistPos = pDistSlider->GetPos();
			if ( nDistPos != m_flPrevDistance )
			{
				// Update the displacement tool info.
				pTool->SetChannel( DISPPAINT_CHANNEL_POSITION, ( float )nDistPos );

				// Update the "buddy" edit box.
				CString strDistance;
				strDistance.Format( "%4.2f", ( float )nDistPos );
				m_editDistance.SetWindowText( strDistance );
			}
		}

		// Get the radius slider control.
		CSliderCtrl *pRadiusSlider = ( CSliderCtrl* )GetDlgItem( ID_DISP_PAINT_DIST_SLIDER_RADIUS );
		if ( pRadiusSlider )
		{
			// Get the slider position.
			int nRadiusPos = pRadiusSlider->GetPos();
			if ( nRadiusPos != m_flPrevRadius )
			{
				// Update the displacement tool info.
				pTool->SetSpatialRadius( ( float )nRadiusPos );

				// Update the "buddy" edit box.
				CString strRadius;
				strRadius.Format( "%4.2f", ( float )nRadiusPos );
				m_editRadius.SetWindowText( strRadius );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnEditDistance( void )
{
	//
	// get the edit box distance data
	//
	CString strDistance;
	m_editDistance.GetWindowText( strDistance );
	float flDistance = atof( strDistance );

	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		UpdateSliderDistance( flDistance, false );
		pTool->SetChannel( DISPPAINT_CHANNEL_POSITION, flDistance );
		// Save the change in the distance.
		m_flPrevDistance = flDistance;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnEditRadius( void )
{
	//
	// Get the edit box radius data.
	//
	CString strRadius;
	m_editRadius.GetWindowText( strRadius );
	float flRadius = atof( strRadius );

	// get the displacement tool
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		UpdateSliderRadius( flRadius, false );
		pTool->SetSpatialRadius( flRadius );

		// Save the change in the spatial radius.
		m_flPrevRadius = flRadius;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnClose( void )
{
	// get the displacement tool and set selection tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_SELECT );
	}

	// set "select" as the current tool - this should destroy this dialog!!
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if ( pSheet )
	{
		pSheet->m_DispPage.SetTool( CFaceEditDispPage::FACEEDITTOOL_SELECT );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDistDlg::OnDestroy( void )
{
	//
	// save the current dialog data - window position, effect, etc...
	//
	GetWindowRect( &m_DialogPosRect );

	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		m_nPrevEffect = pTool->GetEffect();
		pTool->GetPaintAxis( m_nPrevPaintAxis, m_vecPrevPaintAxis );

		// Reset spatial tool flag.
		if ( pTool->IsSpatialPainting() )
		{
			pTool->ToggleSpatialPainting();
		}
	}

	m_nPrevBrush = m_comboboxBrush.GetCurSel();

	// detach the brush combo box!!
	m_comboboxBrush.Detach();
}


//=============================================================================
//
// Paint Scult Dialog Functions
//
BEGIN_MESSAGE_MAP(CPaintSculptDlg, CDialog)
	//{{AFX_MSG_MAP(CPaintSculptDlg)
	ON_BN_CLICKED( ID_DISP_PAINT_DIST_AUTOSEW, OnCheckAutoSew )
	ON_WM_CLOSE()
	ON_WM_DESTROY()	
	ON_BN_CLICKED(IDC_SCULPT_PUSH, &CPaintSculptDlg::OnBnClickedSculptPush)
	ON_BN_CLICKED(IDC_SCULPT_CARVE, &CPaintSculptDlg::OnBnClickedSculptCarve)
	ON_BN_CLICKED(IDC_SCULPT_PROJECT, &CPaintSculptDlg::OnBnClickedSculptProject)
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CPaintSculptDlg::CPaintSculptDlg( CWnd *pParent ) :
CDialog( CPaintSculptDlg::IDD, pParent )
{
	m_bAutoSew = true;
	m_SculptMode = SCULPT_MODE_PUSH;

	m_PushOptions = new CSculptPushOptions();
	m_CarveOptions = new CSculptCarveOptions();
//	m_ProjectOptions = new CSculptProjectOptions();
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CPaintSculptDlg::~CPaintSculptDlg( )
{
	delete m_PushOptions;
	delete m_CarveOptions;
//	delete m_ProjectOptions;
}


//-----------------------------------------------------------------------------
// Purpose: intialized the dialog
// Output : returns true if successful
//-----------------------------------------------------------------------------
BOOL CPaintSculptDlg::OnInitDialog( )
{
	static bool bInit = false;

	CDialog::OnInitDialog();

	CToolDisplace *pTool = GetDisplacementTool();
	if ( !pTool )
	{
		return FALSE;
	}

#if 0
	// Set spatial tool flag.
	if ( !pTool->IsSpatialPainting() )
	{
		pTool->ToggleSpatialPainting();
	}
#endif

	if ( !bInit )
	{
		bInit = true;
	}
	else
	{
		SetWindowPos( &wndTop, m_DialogPosRect.left, m_DialogPosRect.top, m_DialogPosRect.Width(), m_DialogPosRect.Height(), SWP_NOZORDER );
	}

	m_AutoSew.SetCheck( m_bAutoSew );

	m_PushOptions->SetPaintOwner( this );
	m_CarveOptions->SetPaintOwner( this );
//	m_ProjectOptions->SetPaintOwner( this );

	if( !m_PushOptions->Create( IDD_DISP_SCULPT_PUSH_OPTIONS, this ) )
	{
		return FALSE;	
	}

	if( !m_CarveOptions->Create( IDD_DISP_SCULPT_CARVE_OPTIONS, this ) )
	{
		return FALSE;	
	}

#if 0
	if( !m_ProjectOptions->Create( IDD_DISP_SCULPT_PROJECT_OPTIONS, this ) )
	{
		return FALSE;	
	}
#endif

	RECT	OptionsLoc, ThisLoc;

 	m_SculptOptionsLoc.GetWindowRect( &OptionsLoc );
	GetWindowRect( &ThisLoc );

	m_PushOptions->SetWindowPos( NULL, 10, OptionsLoc.top - ThisLoc.top - 20, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW );
	m_CarveOptions->SetWindowPos( NULL, 10, OptionsLoc.top - ThisLoc.top - 20, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW );
//	m_ProjectOptions->SetWindowPos( NULL, 10, OptionsLoc.top - ThisLoc.top - 20, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW );

	m_PushOptions->ShowWindow( SW_HIDE );
	m_CarveOptions->ShowWindow( SW_HIDE );
//	m_ProjectOptions->ShowWindow( SW_HIDE );

	m_ProjectButton.EnableWindow( FALSE );

	SetActiveMode( m_SculptMode );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: set up the data exchange between the dialog and variables
// Input  : pDX - data exchange object
//-----------------------------------------------------------------------------
void CPaintSculptDlg::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CPaintSculptDlg)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_SCULPT_OPTIONS_LOC, m_SculptOptionsLoc);
	DDX_Control(pDX, ID_DISP_PAINT_DIST_AUTOSEW, m_AutoSew);
	DDX_Control(pDX, IDC_SCULPT_PUSH, m_PushButton);
	DDX_Control(pDX, IDC_SCULPT_CARVE, m_CarveButton);
	DDX_Control(pDX, IDC_SCULPT_PROJECT, m_ProjectButton);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the autosew option
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnCheckAutoSew( )
{
	m_bAutoSew = ( m_AutoSew.GetCheck() != 0 );
}


//-----------------------------------------------------------------------------
// Purpose: handles shutting down the dialog
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnClose( )
{
	// get the displacement tool and set selection tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_SELECT );
	}

	// set "select" as the current tool - this should destroy this dialog!!
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if ( pSheet )
	{
		pSheet->m_DispPage.SetTool( CFaceEditDispPage::FACEEDITTOOL_SELECT );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left button up
// Input  : nFlags - button flags
//			point - the location of the click
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnLButtonUp( UINT nFlags, CPoint point )
{
	CToolDisplace *pDispTool = GetDisplacementTool();
	CSculptPainter *painter = dynamic_cast< CSculptPainter * >( pDispTool->GetSculptPainter() );

	if ( painter )
	{
		painter->OnLButtonUpDialog( nFlags, point );
	}
	
	__super::OnLButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left button down
// Input  : nFlags - button flags
//			point - the location of the click
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnLButtonDown( UINT nFlags, CPoint point )
{
	CToolDisplace *pDispTool = GetDisplacementTool();
	CSculptPainter *painter = dynamic_cast< CSculptPainter * >( pDispTool->GetSculptPainter() );

	if ( painter )
	{
		painter->OnLButtonDownDialog( nFlags, point );
	}

	__super::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move
// Input  : nFlags - button flags
//			point - the location of the click
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnMouseMove( UINT nFlags, CPoint point )
{
	CToolDisplace *pDispTool = GetDisplacementTool();
	CSculptPainter *painter = dynamic_cast< CSculptPainter * >( pDispTool->GetSculptPainter() );

	if ( painter )
	{
		painter->OnMouseMoveDialog( nFlags, point );
	}

	__super::OnMouseMove(nFlags, point);
}

//-----------------------------------------------------------------------------
// Purpose: handles the destruction of the window
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnDestroy( )
{
	//
	// save the current dialog data - window position, effect, etc...
	//
	GetWindowRect( &m_DialogPosRect );

#if 0
	CToolDisplace *pTool = GetDisplacementTool();
	if ( pTool )
	{
		// Reset spatial tool flag.
		if ( pTool->IsSpatialPainting() )
		{
			pTool->ToggleSpatialPainting();
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: sets the active mode to push
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnBnClickedSculptPush( )
{
	SetActiveMode( SCULPT_MODE_PUSH );
}


//-----------------------------------------------------------------------------
// Purpose: sets the active mode to carve
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnBnClickedSculptCarve( )
{
	SetActiveMode( SCULPT_MODE_CARVE );
}


//-----------------------------------------------------------------------------
// Purpose: sets the active mode to sculpt
//-----------------------------------------------------------------------------
void CPaintSculptDlg::OnBnClickedSculptProject( )
{
//	SetActiveMode( SCULPT_MODE_PROJECT );
}

#if 0
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CPaintSculptDlg::PreTranslateMessage( MSG* pMsg )
{
	return __super::PreTranslateMessage( pMsg );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: sets the active mode
// Input  : NewMode - the mode we are going to
//-----------------------------------------------------------------------------
void CPaintSculptDlg::SetActiveMode( SculptMode NewMode )
{
	m_SculptMode = NewMode;

	m_PushButton.SetCheck( m_SculptMode == SCULPT_MODE_PUSH );
	m_CarveButton.SetCheck( m_SculptMode == SCULPT_MODE_CARVE );
	m_ProjectButton.SetCheck( m_SculptMode == SCULPT_MODE_PROJECT );

	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		CDialog *painter = dynamic_cast< CDialog * >( pDispTool->GetSculptPainter() );

		if ( painter )
		{
			painter->ShowWindow( SW_HIDE );
		}

		switch( m_SculptMode )
		{
			case SCULPT_MODE_PUSH:
				m_PushOptions->ShowWindow( SW_SHOW );
				pDispTool->SetSculptPainter( m_PushOptions );
				break;

			case SCULPT_MODE_CARVE:
				m_CarveOptions->ShowWindow( SW_SHOW );
				pDispTool->SetSculptPainter( m_CarveOptions );
				break;

#if 0
			case SCULPT_MODE_PROJECT:
				m_ProjectOptions->ShowWindow( SW_SHOW );
				pDispTool->SetSculptPainter( m_ProjectOptions );
				break;
#endif
		}
	}
}
















//=============================================================================
//
// Set Paint Distance Dialog Functions
//
BEGIN_MESSAGE_MAP(CDispPaintDataDlg, CDialog)
	//{{AFX_MSG_MAP(CDispPaintDataDlg)
	ON_BN_CLICKED( ID_DISP_PAINT_DATA_RAISELOWER, OnEffectRaiseLowerData )
	ON_BN_CLICKED( ID_DISP_PAINT_DATA_RAISETO, OnEffectRaiseToData )
	ON_BN_CLICKED( ID_DISP_PAINT_DATA_SMOOTH, OnEffectSmoothData )

	ON_CBN_SELCHANGE( ID_DISP_PAINT_DATA_BRUSH, OnComboBoxBrushData )
	ON_CBN_SELCHANGE( ID_DISP_PAINT_DATA_TYPE, OnComboBoxType )

	ON_WM_HSCROLL()
	ON_EN_CHANGE( ID_DISP_PAINT_DATA_EDIT_VALUE, OnEditValue )

	ON_WM_CLOSE()
	ON_WM_DESTROY()	
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CDispPaintDataDlg::CDispPaintDataDlg( CWnd *pParent ) :
	CDialog( CDispPaintDataDlg::IDD, pParent )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDispPaintDataDlg::~CDispPaintDataDlg()
{
	if( m_comboboxBrush.m_hWnd )
	{
		m_comboboxBrush.Detach();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CDispPaintDataDlg::OnInitDialog(void)
{
	static bool bInit = false;

	CDialog::OnInitDialog();

	if( !bInit )
	{
		CToolDisplace *pDispTool = GetDisplacementTool();
		if( pDispTool )
		{
			m_uiPrevEffect = pDispTool->GetEffect();
			pDispTool->GetChannel( DISPPAINT_CHANNEL_ALPHA, m_fPrevPaintValue );
			m_iPrevBrush = 0;

			bInit = true;
		}
	}
	else
	{
		SetWindowPos( &wndTop, m_DialogPosRect.left, m_DialogPosRect.top,
			          m_DialogPosRect.Width(), m_DialogPosRect.Height(), SWP_NOZORDER );
	}

	// initialize the sliders
	InitValue();

	// initialize the combo boxes
	InitComboBoxBrushData();
	InitComboBoxType();

	return TRUE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::InitValue( void )
{
	// init slider value
	m_sliderValue.SetBuddy( &m_editValue, FALSE );
	m_sliderValue.SetRange( 1, 255 );
	m_sliderValue.SetTicFreq( 25 );

	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetChannel( DISPPAINT_CHANNEL_ALPHA, m_fPrevPaintValue );

		// init slider value
		UpdateSliderValue( m_fPrevPaintValue );
		
		// initialize the value edit box
		CString strValue;
		strValue.Format( "%4.2f", m_fPrevPaintValue );
		m_editValue.SetWindowText( strValue );
	}
	else
	{
		UpdateSliderValue( 15.0f );
		
		// initialize the value edit box
		m_editValue.SetWindowText( "15.00" );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispPaintDataDlg::InitComboBoxBrushData( void )
{
	//
	// get the displacement paint brush icon combo box
	//
	m_comboboxBrush.Attach( GetDlgItem( ID_DISP_PAINT_DATA_BRUSH )->m_hWnd );
	m_comboboxBrush.Init();

	// reset the size of the combo box list item
	m_comboboxBrush.SetItemHeight( -1, m_comboboxBrush.m_IconSize.cy + 2 );

	// set initial radio button/brush combo box data
	// initialize the radio button/brush combo box geometry data
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetEffect( m_uiPrevEffect );

		switch( m_uiPrevEffect )
		{
		case DISPPAINT_EFFECT_RAISELOWER:
			{
				pDispTool->SetEffect( DISPPAINT_EFFECT_RAISELOWER );
				SetEffectButtonData( DISPPAINT_EFFECT_RAISELOWER );
				FilterComboBoxBrushData( DISPPAINT_EFFECT_RAISELOWER, true );
				break;
			}
		case DISPPAINT_EFFECT_RAISETO: 
			{ 
				pDispTool->SetEffect( DISPPAINT_EFFECT_RAISETO );
				SetEffectButtonData( DISPPAINT_EFFECT_RAISETO );
				FilterComboBoxBrushData( DISPPAINT_EFFECT_RAISETO, true );
				break; 
			}
		case DISPPAINT_EFFECT_SMOOTH: 
			{  
				pDispTool->SetEffect( DISPPAINT_EFFECT_SMOOTH );
				SetEffectButtonData( DISPPAINT_EFFECT_SMOOTH );
				FilterComboBoxBrushData( DISPPAINT_EFFECT_SMOOTH, true );
				break; 
			}
		default: 
			{ 
				return false; 
			}
		}

		OnComboBoxBrushData();
	}
	else
	{
		OnEffectRaiseLowerData();
		OnComboBoxBrushData();
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::FilterComboBoxBrushData( unsigned int uiEffect, bool bInit )
{
	//
	// remove all the old combo box data
	//
	int count = m_comboboxBrush.GetCount();
	for( int ndx = count - 1; ndx >= 0; ndx-- )
	{
		m_comboboxBrush.DeleteIcon( ndx );
	}

	//
	// add the new combo box data based on the current paint "effect"
	//
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		CDispMapImageFilterManager *pFilterMgr;
		switch( uiEffect )
		{
		case DISPPAINT_EFFECT_RAISELOWER: { pFilterMgr = pDispTool->GetFilterRaiseLowerMgr(); break; }
		case DISPPAINT_EFFECT_RAISETO: { pFilterMgr = pDispTool->GetFilterRaiseToMgr(); break; }
		case DISPPAINT_EFFECT_SMOOTH: { pFilterMgr = pDispTool->GetFilterSmoothMgr(); break; }
		default: return;
		}

		if( pFilterMgr )
		{
			//
			// for each filter - add its icon to the icon combo box
			//
			for( int ndxFilter = 0; ndxFilter < pFilterMgr->GetFilterCount(); ndxFilter++ )
			{
				// get the current filter
				CDispMapImageFilter *pFilter = pFilterMgr->GetFilter( ndxFilter );

				// get the application directory
				char appDir[MAX_PATH];
				APP()->GetDirectory( DIR_PROGRAM, appDir );
				
				// append the filters directory name
				strcat( appDir, "filters\\" );
				
				// append the directory prefix to the icon name
				CString iconFilename = appDir + pFilter->m_Name;
				
				// add the icon to the icon combo box
				m_comboboxBrush.AddIcon( iconFilename );
			}

			// set initial paint brush
			if( bInit )
			{
				m_comboboxBrush.SetCurSel( m_iPrevBrush );
			}
			else
			{
				m_comboboxBrush.SetCurSel( 0 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispPaintDataDlg::InitComboBoxType( void )
{
	// alpha type
	CString strType = "Alpha";
	m_comboboxType.AddString( strType );
	m_comboboxType.SetCurSel( 0 );

	// turn off for now
	m_comboboxType.EnableWindow( FALSE );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::DoDataExchange( CDataExchange *pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CDispPaintDistDlg)
	DDX_Control( pDX, ID_DISP_PAINT_DATA_SLIDER_VALUE, m_sliderValue );
	DDX_Control( pDX, ID_DISP_PAINT_DATA_EDIT_VALUE, m_editValue );
	DDX_Control( pDX, ID_DISP_PAINT_DATA_TYPE, m_comboboxType );
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnComboBoxBrushData( void )
{
	// get the displacement's filter manager
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		// get current selection
		int iSel = m_comboboxBrush.GetCurSel();
		if( iSel == LB_ERR )
			return;

		unsigned int uiEffect = pDispTool->GetEffect();
		CDispMapImageFilterManager *pFilterMgr;
		switch( uiEffect )
		{
		case DISPPAINT_EFFECT_RAISELOWER: { pFilterMgr = pDispTool->GetFilterRaiseLowerMgr(); break; }
		case DISPPAINT_EFFECT_RAISETO: { pFilterMgr = pDispTool->GetFilterRaiseToMgr(); break; }
		case DISPPAINT_EFFECT_SMOOTH: { pFilterMgr = pDispTool->GetFilterSmoothMgr(); break; }
		default: return;
		}

		if( pFilterMgr )
		{
			pFilterMgr->SetActiveFilter( iSel );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnComboBoxType( void )
{
	return;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::SetEffectButtonData( unsigned int effect )
{
	CButton *radiobutton;
	radiobutton = ( CButton* )GetDlgItem( ID_DISP_PAINT_DATA_RAISELOWER );
	radiobutton->SetCheck( effect == DISPPAINT_EFFECT_RAISELOWER );
	radiobutton = ( CButton* )GetDlgItem( ID_DISP_PAINT_DATA_RAISETO );
	radiobutton->SetCheck( effect == DISPPAINT_EFFECT_RAISETO );
	radiobutton = ( CButton* )GetDlgItem( ID_DISP_PAINT_DATA_SMOOTH ); 
	radiobutton->SetCheck( effect == DISPPAINT_EFFECT_SMOOTH );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnEffectRaiseLowerData( void )
{
	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetEffect( DISPPAINT_EFFECT_RAISELOWER );
		SetEffectButtonData( DISPPAINT_EFFECT_RAISELOWER );
		FilterComboBoxBrushData( DISPPAINT_EFFECT_RAISELOWER, false );
		OnComboBoxBrushData();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnEffectRaiseToData( void )
{
	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetEffect( DISPPAINT_EFFECT_RAISETO );
		SetEffectButtonData( DISPPAINT_EFFECT_RAISETO );
		FilterComboBoxBrushData( DISPPAINT_EFFECT_RAISETO, false );
		OnComboBoxBrushData();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnEffectSmoothData( void )
{
	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetEffect( DISPPAINT_EFFECT_SMOOTH );
		SetEffectButtonData( DISPPAINT_EFFECT_SMOOTH );
		FilterComboBoxBrushData( DISPPAINT_EFFECT_SMOOTH, false );
		OnComboBoxBrushData();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::UpdateSliderValue( float fValue )
{
	int iValue = ( int )fValue;

	// clamp
	if( iValue < 1 ) { iValue = 1; }
	if( iValue > 255 ) { iValue = 255; }

	m_sliderValue.SetPos( iValue );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnHScroll( UINT nSBCode, UINT nPos, CScrollBar *pScrollBar ) 
{
	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		// get the slider control
		CSliderCtrl *pSlider = ( CSliderCtrl* )GetDlgItem( ID_DISP_PAINT_DATA_SLIDER_VALUE );
		if( pSlider )
		{
			// get the slider position
			int pos = pSlider->GetPos();

			pDispTool->SetChannel( DISPPAINT_CHANNEL_ALPHA, ( float )pos );

			//
			// update "the buddy" the disp value cedit box
			//	
			CString strValue;
			strValue.Format( "%4.2f", ( float )pos );
			m_editValue.SetWindowText( strValue );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnEditValue( void )
{
	//
	// get the edit box distance data
	//
	CString strValue;
	m_editValue.GetWindowText( strValue );
	float fValue = atof( strValue );

	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		UpdateSliderValue( fValue );
		pDispTool->SetChannel( DISPPAINT_CHANNEL_ALPHA, fValue );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnClose( void )
{
	// get the displacement tool and set selection tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_SELECT );
	}

	// set "select" as the current tool - this should destroy this dialog!!
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		pSheet->m_DispPage.SetTool( CFaceEditDispPage::FACEEDITTOOL_SELECT );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispPaintDataDlg::OnDestroy( void )
{
	// save the current window position
	GetWindowRect( &m_DialogPosRect );

	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		m_uiPrevEffect = pDispTool->GetEffect();
		pDispTool->GetChannel( DISPPAINT_CHANNEL_ALPHA, m_fPrevPaintValue );
	}

	m_iPrevBrush = m_comboboxBrush.GetCurSel();

	// detach the brush combo box!!
	m_comboboxBrush.Detach();
}
