//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include <afxcmn.h>
#include "hammer.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "FaceEdit_DispPage.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "MapDisp.h"
#include "FaceEditSheet.h"
#include "MapView3D.h"
#include "History.h"
#include "ToolDisplace.h"
#include "ToolManager.h"
#include "DispSew.h"
#include "builddisp.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CToolDisplace* GetDisplacementTool() { return dynamic_cast<CToolDisplace*>(ToolManager()->GetToolForID(TOOL_FACEEDIT_DISP)); }

//=============================================================================

IMPLEMENT_DYNAMIC( CFaceEditDispPage, CPropertyPage )

BEGIN_MESSAGE_MAP( CFaceEditDispPage, CPropertyPage )
	//{{AFX_MSG_MAP( CFaceEditDispPage )
	ON_BN_CLICKED( ID_DISP_MASK_SELECT, OnCheckMaskSelect )
	ON_BN_CLICKED( ID_DISP_MASK_GRID, OnCheckMaskGrid )

	ON_BN_CLICKED( ID_DISP_NOPHYSICS_COLL, OnCheckNoPhysicsCollide )
	ON_BN_CLICKED( ID_DISP_NOHULL_COLL, OnCheckNoHullCollide )
	ON_BN_CLICKED( ID_DISP_NORAY_COLL, OnCheckNoRayCollide )

	ON_BN_CLICKED( ID_DISP_SELECT2, OnButtonSelect )
	ON_BN_CLICKED( ID_DISP_CREATE, OnButtonCreate )
	ON_BN_CLICKED( ID_DISP_DESTROY, OnButtonDestroy )
	ON_BN_CLICKED( ID_DISP_NOISE, OnButtonNoise )
	ON_BN_CLICKED( ID_DISP_SUBDIVIDE, OnButtonSubdivide )
	ON_BN_CLICKED( ID_DISP_SEW, OnButtonSew )
	ON_BN_CLICKED( ID_DISP_PAINT_GEO, OnButtonPaintGeo )
	ON_BN_CLICKED( ID_DISP_PAINT_DATA, OnButtonPaintData )
	ON_BN_CLICKED( ID_DISP_TAG_WALK, OnButtonTagWalkable )
	ON_BN_CLICKED( ID_DISP_TAG_BUILD, OnButtonTagBuildable )
	ON_BN_CLICKED( ID_DISP_TAG_REMOVE, OnButtonTagRemove )
	ON_BN_CLICKED( ID_DISP_INVERT_ALPHA, OnButtonInvertAlpha )
	ON_BN_CLICKED( IDC_SELECT_ADJACENT, OnSelectAdjacent )

	ON_NOTIFY( UDN_DELTAPOS, ID_SPIN_DISP_POWER, OnSpinUpDown )
	ON_NOTIFY( UDN_DELTAPOS, ID_SPIN_DISP_ELEVATION, OnSpinUpDown )
	ON_BN_CLICKED( ID_DISP_APPLY, OnButtonApply )
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(ID_DISP_SCULPT_PAINT, &CFaceEditDispPage::OnBnClickedDispSculptPaint)
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFaceEditDispPage::CFaceEditDispPage() : CPropertyPage( IDD )
{
	m_uiTool = FACEEDITTOOL_SELECT;

	m_bForceShowWalkable = false;
	m_bForceShowBuildable = false;
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CFaceEditDispPage::~CFaceEditDispPage()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CFaceEditDispPage::PostToolUpdate( void )
{
	// update the mapdoc views
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( pDoc )
	{
		pDoc->SetModifiedFlag();
	}

	// update the dialogs data
	UpdateDialogData();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::ClickFace( CMapSolid *pSolid, int faceIndex, int cmd, int clickMode )
{
	m_bIsEditable = ( pSolid ? pSolid->IsEditable() : true );

	UpdateDialogData();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::UpdateEditControls( bool bAllDisps, bool bHasFace )
{
	//
	// masks - always active!!
	//
	CButton *pcheckSelect = ( CButton* )GetDlgItem( ID_DISP_MASK_SELECT );
	CButton *pcheckGrid = ( CButton* )GetDlgItem( ID_DISP_MASK_GRID );
	pcheckSelect->EnableWindow( TRUE );
	pcheckGrid->EnableWindow( TRUE );

	//
	// tool buttons
	//
	CButton *pbuttonSelect = ( CButton* )GetDlgItem( ID_DISP_SELECT2 );
	CButton *pbuttonCreate = ( CButton* )GetDlgItem( ID_DISP_CREATE );
	CButton *pbuttonDestroy = ( CButton* )GetDlgItem( ID_DISP_DESTROY );
	CButton *pbuttonPaintGeo = ( CButton* )GetDlgItem( ID_DISP_PAINT_GEO );
	CButton *pbuttonPaintSculpt = ( CButton* )GetDlgItem( ID_DISP_SCULPT_PAINT );
	CButton *pbuttonPaintData = ( CButton* )GetDlgItem( ID_DISP_PAINT_DATA );
	CButton *pbuttonSubdiv = ( CButton* )GetDlgItem( ID_DISP_SUBDIVIDE );
	CButton *pbuttonSew = ( CButton* )GetDlgItem( ID_DISP_SEW );
	CButton *pbuttonNoise = ( CButton* )GetDlgItem( ID_DISP_NOISE );
	CButton *pButtonWalk = ( CButton* )GetDlgItem( ID_DISP_TAG_WALK );
	CButton *pButtonBuild = ( CButton* )GetDlgItem( ID_DISP_TAG_BUILD );

	pbuttonSelect->EnableWindow( TRUE );
	pbuttonCreate->EnableWindow( FALSE );	
	pbuttonDestroy->EnableWindow( FALSE );
	pbuttonPaintGeo->EnableWindow( FALSE );
	pbuttonPaintSculpt->EnableWindow( FALSE );
	pbuttonPaintData->EnableWindow( FALSE );
	pbuttonSubdiv->EnableWindow( FALSE );
	pbuttonSew->EnableWindow( FALSE );
	pbuttonNoise->EnableWindow( FALSE );
	pButtonWalk->EnableWindow( TRUE );
	pButtonBuild->EnableWindow( TRUE );

	//
	// attributes (displacement info)
	//
	CEdit *peditPower = ( CEdit* )GetDlgItem( ID_DISP_POWER );
	CSpinButtonCtrl *pspinPower = ( CSpinButtonCtrl* )GetDlgItem( ID_SPIN_DISP_POWER );
	CEdit *peditElevation = ( CEdit* )GetDlgItem( ID_DISP_ELEVATION );
	CSpinButtonCtrl *pspinElevation = ( CSpinButtonCtrl* )GetDlgItem( ID_SPIN_DISP_ELEVATION );
	CEdit *peditScale = ( CEdit* )GetDlgItem( ID_DISP_SCALE );
	CButton *pbuttonApply = ( CButton* )GetDlgItem( ID_DISP_APPLY );
	CButton *pbuttonInvertAlpha = ( CButton* )GetDlgItem( ID_DISP_INVERT_ALPHA );
	CButton *pCheckNoPhysicsColl = ( CButton* )GetDlgItem( ID_DISP_NOPHYSICS_COLL );
	CButton *pCheckNoHullColl = ( CButton* )GetDlgItem( ID_DISP_NOHULL_COLL );
	CButton *pCheckNoRayColl = ( CButton* )GetDlgItem( ID_DISP_NORAY_COLL );

	peditPower->EnableWindow( FALSE );
	pspinPower->EnableWindow( FALSE );
	peditElevation->EnableWindow( FALSE );
	pspinElevation->EnableWindow( FALSE );
	peditScale->EnableWindow( FALSE );
	pbuttonApply->EnableWindow( FALSE );
	pbuttonInvertAlpha->EnableWindow( FALSE );
	pCheckNoPhysicsColl->EnableWindow( FALSE );
	pCheckNoHullColl->EnableWindow( FALSE );
	pCheckNoRayColl->EnableWindow( FALSE );

	// if there aren't any faces selected then the only active item should be selection
	if( !bHasFace )
		return;

	//
	// if not all selected faces are displacements then highlight only
	// SELECTION, CREATE, DESTROY, and SEW
	//
	if ( m_bIsEditable )
	{
		if( !bAllDisps )
		{
			pbuttonCreate->EnableWindow( TRUE );	
			pbuttonDestroy->EnableWindow( TRUE );
			pbuttonSew->EnableWindow( TRUE );
		}
		// highlight all tool buttons, but CREATE
		else
		{
			pbuttonDestroy->EnableWindow( TRUE );
			pbuttonPaintGeo->EnableWindow( TRUE );
			pbuttonPaintSculpt->EnableWindow( TRUE );
			pbuttonPaintData->EnableWindow( TRUE );
			pbuttonSubdiv->EnableWindow( TRUE );
			pbuttonSew->EnableWindow( TRUE );
			pbuttonNoise->EnableWindow( TRUE );
		}

		// active attributes if in selection mode
		if( m_uiTool == FACEEDITTOOL_SELECT )
		{
			peditPower->EnableWindow( TRUE );
			pspinPower->EnableWindow( TRUE );
			peditElevation->EnableWindow( TRUE );
			pspinElevation->EnableWindow( TRUE );
			peditScale->EnableWindow( TRUE );
			pbuttonApply->EnableWindow( TRUE );
			pbuttonInvertAlpha->EnableWindow( TRUE );
			pCheckNoPhysicsColl->EnableWindow( TRUE );
			pCheckNoHullColl->EnableWindow( TRUE );
			pCheckNoRayColl->EnableWindow( TRUE );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::FillEditControls( bool bAllDisps )
{
	CString strText;

	//
	// if all selected face have displacements
	//
	if( bAllDisps )
	{
		int		power = 0;
		bool	bAllPower = true;

		float	elevation = 0.0f;
		bool	bAllElevation = true;

		float   scale = 1.0f;
		bool    bAllScale = true;

		bool	bAllNoPhysics = true;
		bool	bAllNoHull = true;
		bool	bAllNoRay = true;

		CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
		if( pSheet )
		{
			int faceCount = pSheet->GetFaceListCount();
			if( faceCount > 0 )
			{
				CMapFace *pFace = pSheet->GetFaceListDataFace( 0 );
				EditDispHandle_t handle = pFace->GetDisp();
				CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
				
				power = pDisp->GetPower();
				elevation = pDisp->GetElevation();
				scale = pDisp->GetScale();
			}

			//
			// test all faces for "equal" attributes
			//
			for( int faceIndex = 0; faceIndex < faceCount; faceIndex++ )
			{
				CMapFace *pFace = pSheet->GetFaceListDataFace( faceIndex );
				EditDispHandle_t handle = pFace->GetDisp();
				CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
				
				// test power, elevation, and scale
				if( power != pDisp->GetPower() ) { bAllPower = false; }				
				if( elevation != pDisp->GetElevation() ) { bAllElevation = false; }	
				if( scale != pDisp->GetScale() ) { bAllScale = false; }

				if ( !pDisp->CheckFlags( CCoreDispInfo::SURF_NOPHYSICS_COLL ) ) { bAllNoPhysics = false; }
				if ( !pDisp->CheckFlags( CCoreDispInfo::SURF_NOHULL_COLL ) ) { bAllNoHull = false; }
				if ( !pDisp->CheckFlags( CCoreDispInfo::SURF_NORAY_COLL ) ) { bAllNoRay = false; }
			}

			// set displacement power value to 3 - 9x9
			SetDlgItemText( ID_DISP_POWER, "" );
			if( bAllPower )
			{
				SetDlgItemInt( ID_DISP_POWER, power );
			}
			
			// set elevation value
			SetDlgItemText( ID_DISP_ELEVATION, "" );
			if( bAllElevation )
			{
				strText.Format( "%4.2f", elevation );
				SetDlgItemText( ID_DISP_ELEVATION, strText );
			}

			// set scale value
			SetDlgItemText( ID_DISP_SCALE, "" );
			if( bAllScale )
			{
				strText.Format( "%4.4f", scale );
				SetDlgItemText( ID_DISP_SCALE, strText );
			}

			CButton *pCheckBox;
			pCheckBox = ( CButton* )GetDlgItem( ID_DISP_NOPHYSICS_COLL );
			pCheckBox->SetCheck( FALSE );
			if ( bAllNoPhysics )
			{
				pCheckBox->SetCheck( TRUE );
			}

			pCheckBox = ( CButton* )GetDlgItem( ID_DISP_NOHULL_COLL );
			pCheckBox->SetCheck( FALSE );
			if ( bAllNoHull )
			{
				pCheckBox->SetCheck( TRUE );
			}

			pCheckBox = ( CButton* )GetDlgItem( ID_DISP_NORAY_COLL );
			pCheckBox->SetCheck( FALSE );
			if ( bAllNoRay )
			{
				pCheckBox->SetCheck( TRUE );
			}
		}
	}
	else
	{
		// set initial displacement power value to 3 - 9x9
		CSpinButtonCtrl *pSpin = ( CSpinButtonCtrl* )GetDlgItem( ID_SPIN_DISP_POWER );
		pSpin->SetPos( 0 );
		SetDlgItemInt( ID_DISP_POWER, 0 );
		
		// set initial elevation value
		pSpin = ( CSpinButtonCtrl* )GetDlgItem( ID_SPIN_DISP_ELEVATION );
		pSpin->SetPos( 0 );
		SetDlgItemText( ID_DISP_ELEVATION, "" );

		// set initial scale value
		SetDlgItemText( ID_DISP_SCALE, "" );
	}

	//
	// get tool specific data for page
	//
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		CButton *checkBox;
		checkBox = ( CButton* )GetDlgItem( ID_DISP_MASK_SELECT );
		checkBox->SetCheck( pDispTool->HasSelectMask() );
		checkBox = ( CButton* )GetDlgItem( ID_DISP_MASK_GRID );
		checkBox->SetCheck( pDispTool->HasGridMask() );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::UpdateDialogData( void )
{
	bool bAllDisps = false;
	bool bHasFace = false;

	//
	// get face and displacement info
	//
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		int faceCount = pSheet->GetFaceListCount();
		if( faceCount > 0 )
		{
			bHasFace = true;
			bAllDisps = true;
		}

		for( int i = 0; i < faceCount; i++ )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( i );
			if( !pFace->HasDisp() )
			{
				bAllDisps = false;
			}
		}
	}

	// fill in edit controls
	FillEditControls( bAllDisps );

	// update the edit controls
	UpdateEditControls( bAllDisps, bHasFace );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::UpdatePaintDialogs( void )
{
	m_PaintDistDlg.UpdateSpatialData();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnCheckMaskSelect( void )
{
	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->ToggleSelectMask();
	}
}

	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnCheckMaskGrid( void )
{
	// get the displacement tool
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->ToggleGridMask();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnCheckNoPhysicsCollide( void )
{
	// Toggle marked faces as collidable.
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// Check for a face list.
		int nFaceCount = pSheet->GetFaceListCount();
		if( nFaceCount == 0 )
			return;

		// Are we setting or reseting the collision flag?
		CButton *pCheckNoCollide = ( CButton* )GetDlgItem( ID_DISP_NOPHYSICS_COLL );
		bool bReset = false;
		if ( !pCheckNoCollide->GetCheck() )
		{
			bReset = true;
		}

		// Get faces with displacements and toggle the collide flag.
		int iFace = 0;
		for( iFace = 0; iFace < nFaceCount; iFace++ )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
			if( !pFace || !pFace->HasDisp() )
				continue;

			EditDispHandle_t hDisp = pFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );
			if( pDisp )
			{
				if ( bReset )
				{
					int nFlags = pDisp->GetFlags();
					nFlags &= ~CCoreDispInfo::SURF_NOPHYSICS_COLL;
					pDisp->SetFlags( nFlags );
				}
				else
				{
					int nFlags = pDisp->GetFlags();
					nFlags |= CCoreDispInfo::SURF_NOPHYSICS_COLL;
					pDisp->SetFlags( nFlags );
				}
			}
		}
	}	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnCheckNoHullCollide( void )
{
	// Toggle marked faces as collidable.
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// Check for a face list.
		int nFaceCount = pSheet->GetFaceListCount();
		if( nFaceCount == 0 )
			return;

		// Are we setting or reseting the collision flag?
		CButton *pCheckNoCollide = ( CButton* )GetDlgItem( ID_DISP_NOHULL_COLL );
		bool bReset = false;
		if ( !pCheckNoCollide->GetCheck() )
		{
			bReset = true;
		}

		// Get faces with displacements and toggle the collide flag.
		int iFace = 0;
		for( iFace = 0; iFace < nFaceCount; iFace++ )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
			if( !pFace || !pFace->HasDisp() )
				continue;

			EditDispHandle_t hDisp = pFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );
			if( pDisp )
			{
				if ( bReset )
				{
					int nFlags = pDisp->GetFlags();
					nFlags &= ~CCoreDispInfo::SURF_NOHULL_COLL;
					pDisp->SetFlags( nFlags );
				}
				else
				{
					int nFlags = pDisp->GetFlags();
					nFlags |= CCoreDispInfo::SURF_NOHULL_COLL;
					pDisp->SetFlags( nFlags );
				}
			}
		}
	}	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnCheckNoRayCollide( void )
{
	// Toggle marked faces as collidable.
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// Check for a face list.
		int nFaceCount = pSheet->GetFaceListCount();
		if( nFaceCount == 0 )
			return;

		// Are we setting or reseting the collision flag?
		CButton *pCheckNoCollide = ( CButton* )GetDlgItem( ID_DISP_NORAY_COLL );
		bool bReset = false;
		if ( !pCheckNoCollide->GetCheck() )
		{
			bReset = true;
		}

		// Get faces with displacements and toggle the collide flag.
		int iFace = 0;
		for( iFace = 0; iFace < nFaceCount; iFace++ )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
			if( !pFace || !pFace->HasDisp() )
				continue;

			EditDispHandle_t hDisp = pFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );
			if( pDisp )
			{
				if ( bReset )
				{
					int nFlags = pDisp->GetFlags();
					nFlags &= ~CCoreDispInfo::SURF_NORAY_COLL;
					pDisp->SetFlags( nFlags );
				}
				else
				{
					int nFlags = pDisp->GetFlags();
					nFlags |= CCoreDispInfo::SURF_NORAY_COLL;
					pDisp->SetFlags( nFlags );
				}
			}
		}
	}	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::SetTool( unsigned int tool )
{
	if( m_uiTool == FACEEDITTOOL_PAINTGEO )
	{
		m_PaintDistDlg.DestroyWindow();
	}

	if( m_uiTool == FACEEDITTOOL_PAINTSCULPT )
	{
		m_PaintSculptDlg.DestroyWindow();
	}

	if( m_uiTool == FACEEDITTOOL_PAINTDATA )
	{
		m_PaintDataDlg.DestroyWindow();
	}

	if ( ( m_uiTool == FACEEDITTOOL_TAG_WALK ) ||
		 ( m_uiTool == FACEEDITTOOL_TAG_BUILD ) || 
		 ( m_uiTool == FACEEDITTOOL_TAG_REMOVE ) )
	{
		ResetForceShows();
	}

	m_uiTool = tool;

	// set button checked
	CButton *pbuttonSelect = ( CButton* )GetDlgItem( ID_DISP_SELECT2 );
	CButton *pbuttonCreate = ( CButton* )GetDlgItem( ID_DISP_CREATE );
	CButton *pbuttonDestroy = ( CButton* )GetDlgItem( ID_DISP_DESTROY );
	CButton *pbuttonPaintGeo = ( CButton* )GetDlgItem( ID_DISP_PAINT_GEO );
	CButton *pbuttonPaintSculpt = ( CButton* )GetDlgItem( ID_DISP_SCULPT_PAINT );
	CButton *pbuttonPaintData = ( CButton* )GetDlgItem( ID_DISP_PAINT_DATA );
	CButton *pbuttonSubdiv = ( CButton* )GetDlgItem( ID_DISP_SUBDIVIDE );
	CButton *pbuttonSew = ( CButton* )GetDlgItem( ID_DISP_SEW );
	CButton *pbuttonNoise = ( CButton* )GetDlgItem( ID_DISP_NOISE );
	CButton *pButtonWalk = ( CButton* )GetDlgItem( ID_DISP_TAG_WALK );
	CButton *pButtonBuild = ( CButton* )GetDlgItem( ID_DISP_TAG_BUILD );
	CButton *pButtonRemove = ( CButton* )GetDlgItem( ID_DISP_TAG_REMOVE );

	pbuttonSelect->SetCheck( m_uiTool == FACEEDITTOOL_SELECT );
	pbuttonCreate->SetCheck( m_uiTool == FACEEDITTOOL_CREATE );
	pbuttonDestroy->SetCheck( m_uiTool == FACEEDITTOOL_DESTROY );
	pbuttonPaintGeo->SetCheck( m_uiTool == FACEEDITTOOL_PAINTGEO );
	pbuttonPaintSculpt->SetCheck( m_uiTool == FACEEDITTOOL_PAINTSCULPT );
	pbuttonPaintData->SetCheck( m_uiTool == FACEEDITTOOL_PAINTDATA );
	pbuttonSubdiv->SetCheck( m_uiTool == FACEEDITTOOL_SUBDIV );
	pbuttonSew->SetCheck( m_uiTool == FACEEDITTOOL_SEW );
	pbuttonNoise->SetCheck( m_uiTool == FACEEDITTOOL_NOISE );
	pButtonWalk->SetCheck( m_uiTool == FACEEDITTOOL_TAG_WALK );
	pButtonBuild->SetCheck( m_uiTool == FACEEDITTOOL_TAG_BUILD );
	pButtonRemove->SetCheck( m_uiTool == FACEEDITTOOL_TAG_REMOVE );

	// Update button state, etc.
	UpdateDialogData();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonSelect( void )
{
	// set selection tool
	SetTool( FACEEDITTOOL_SELECT );

	// get the displacement tool and set selection tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_SELECT );
		UpdateDialogData();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonCreate( void )
{
	// set creation tool
	SetTool( FACEEDITTOOL_CREATE );

	//
	// "create" all selected faces
	//
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// check for faces to create
		int faceCount = pSheet->GetFaceListCount();
		if( faceCount == 0 )
			return;
	
		//
		// open the dialog and get the desired creation power
		//
		if( m_CreateDlg.DoModal() == IDCANCEL )
		{
			// reset selection tool
			SetTool( FACEEDITTOOL_SELECT );

			return;
		}

		// clamped the "power" (range [2..4])
		int power = m_CreateDlg.m_Power;		

		//
		// get the active map doc and displacement manager
		//
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if( !pDispMgr )
			return;

		//
		// create faces
		//
		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( !pFace )
				continue;
	
			// make sure the face has the appropriate point count
			if( pFace->GetPointCount() != 4 )
				continue;

			// check to see if the face already has a displacement,
			// ignore it if so
			if( pFace->HasDisp() )
				continue;

			EditDispHandle_t dispHandle = EditDispMgr()->Create();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );
			if( pDisp )
			{
				// set dependencies
				pFace->SetDisp( dispHandle );
				pDisp->SetParent( pFace );

				// get surface data
				pDisp->InitDispSurfaceData( pFace, true );

				// add displacement to world list (this should be done automatically!)
				pDispMgr->AddToWorld( dispHandle );

				// create a new displacement surface based on data
				pDisp->InitData( power );
				pDisp->Create();

				// Post a change to the solid.
				CMapSolid *pSolid = ( CMapSolid* )pFace->GetParent();
				pSolid->PostUpdate( Notify_Changed );

				// Update autovisgroups for the solid
				CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
				pDoc->RemoveFromAutoVisGroups( pSolid );
				pDoc->AddToAutoVisGroup( pSolid );
			}
		}
	}

	// reset selection tool
	SetTool( FACEEDITTOOL_SELECT );

	// update
	PostToolUpdate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonDestroy( void )
{
	// set destruction tool
	SetTool( FACEEDITTOOL_DESTROY );

	//
	// "destroy" all selected faces
	//
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// check for faces to destroy
		int faceCount = pSheet->GetFaceListCount();
		if( faceCount == 0 )
			return;

		// get the displacement manager
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if( !pDispMgr )
			return;

		// mark history position
		pDispMgr->PreUndo( "Displacement Destroy" );

		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			// get the current displacement
			// get current displacement
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( !pFace || !pFace->HasDisp() )
				continue;

			CMapSolid *pSolid = ( CMapSolid* )pFace->GetParent();
			if ( pSolid && !pSolid->IsVisible() )
				continue;

			EditDispHandle_t handle = pFace->GetDisp();
			
			// setup for undo
			pDispMgr->Undo( handle, false );

			pDispMgr->RemoveFromWorld( handle );
			pFace->SetDisp( EDITDISPHANDLE_INVALID );

			// Post a change to the solid.
			pSolid->PostUpdate( Notify_Changed );

			CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
			pDoc->RemoveFromAutoVisGroups( pSolid );
			pDoc->AddToAutoVisGroup( pSolid );
		}

		pDispMgr->PostUndo();
	}

	// reset selection tool
	SetTool( FACEEDITTOOL_SELECT );

	// update
	PostToolUpdate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonNoise( void )
{
	// set noise tool
	SetTool( FACEEDITTOOL_NOISE );

	//
	// "noise" all selected faces
	//
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// check for faces to add noise to
		int faceCount = pSheet->GetFaceListCount();
		if( faceCount == 0 )
			return;

		//
		// open the dialog and get the desired noise parameters
		//
		if( m_NoiseDlg.DoModal() == IDCANCEL )
		{
			// reset selection tool
			SetTool( FACEEDITTOOL_SELECT );

			return;
		}

		// mark for undo
		// get the displacement manager
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if( !pDispMgr )
			return;

		pDispMgr->PreUndo( "Displacement Noise" );

		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			// get current displacement
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( pFace )
			{
				if( pFace->HasDisp() )
				{
					EditDispHandle_t handle = pFace->GetDisp();

					// setup for undo
					pDispMgr->Undo( handle, false );

					CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
					pDisp->ApplyNoise( m_NoiseDlg.m_Min, m_NoiseDlg.m_Max, 1.0f );
				}
			}
		}

		pDispMgr->PostUndo();
	}

	// reset selection tool
	SetTool( FACEEDITTOOL_SELECT );

	// update
	PostToolUpdate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonSubdivide( void )
{
	// set subdivision tool
	SetTool( FACEEDITTOOL_SUBDIV );

	//
	// create selection list
	//
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return;

	pDispMgr->SelectClear();

	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// check for faces to subdivide
		int faceCount = pSheet->GetFaceListCount();
		if( faceCount == 0 )
			return;

		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			// get current displacement
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( pFace )
			{
				if( pFace->HasDisp() )
				{
					EditDispHandle_t handle = pFace->GetDisp();
					pDispMgr->AddToSelect( handle );

					CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
					pDisp->ResetTexelHitIndex();
				}
			}
		}
	}

	// subdivide
	pDispMgr->CatmullClarkSubdivide();

	// reset selection tool
	SetTool( FACEEDITTOOL_SELECT );

	// update
	PostToolUpdate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonSew( void )
{
	// set sew tool
	SetTool( FACEEDITTOOL_SEW );

	//
	// "sew" all selected faces
	//
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		// check for faces to sew
		int faceCount = pSheet->GetFaceListCount();
		if( faceCount == 0 )
			return;

		// get the displacement manager
		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if( !pDispMgr )
			return;

		// mark history position
		pDispMgr->PreUndo( "Displacement Sewing" );

		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			// get the current displacement
			// get current displacement
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( pFace )
			{
				if( pFace->HasDisp() )
				{
					EditDispHandle_t handle = pFace->GetDisp();

					// setup for undo
					pDispMgr->Undo( handle, false );
				}
			}
		}

		pDispMgr->PostUndo();

		// sew faces
		FaceListSewEdges();

		// update the parents - force a rebuild
		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			// get current displacement
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( pFace )
			{
				if( pFace->HasDisp() )
				{
					CMapSolid *pSolid = ( CMapSolid* )pFace->GetParent();
					pSolid->PostUpdate( Notify_Rebuild );
				}
			}
		}
	}

	// reset selection tool
	SetTool( FACEEDITTOOL_SELECT );

	// update
	PostToolUpdate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonPaintGeo( void )
{
	// set selection tool
	SetTool( FACEEDITTOOL_PAINTGEO );

	// get the displacement tool and set the paint tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_PAINT );
		UpdateDialogData();
	}

	if( !m_PaintDistDlg.Create( IDD_DISP_PAINT_DIST, this ) )
		return;	

	m_PaintDistDlg.ShowWindow( SW_SHOW );
}


//-----------------------------------------------------------------------------
// Purpose: handles the user clicking on the sculpt button
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnBnClickedDispSculptPaint( )
{
	// set selection tool
	SetTool( FACEEDITTOOL_PAINTSCULPT );

	// get the displacement tool and set the paint tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_PAINT_SCULPT );
		UpdateDialogData();
	}

	if( !m_PaintSculptDlg.Create( IDD_DISP_PAINT_SCULPT, this ) )
		return;	

	m_PaintSculptDlg.ShowWindow( SW_SHOW );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonPaintData( void )
{
	// set selection tool
	SetTool( FACEEDITTOOL_PAINTDATA );

	// get the displacement tool and set the paint tool active
	CToolDisplace *pDispTool = GetDisplacementTool();
	if( pDispTool )
	{
		pDispTool->SetTool( DISPTOOL_PAINT );
		UpdateDialogData();
	}

	if( !m_PaintDataDlg.Create( IDD_DISP_PAINT_DATA, this ) )
		return;

	m_PaintDataDlg.ShowWindow( SW_SHOW );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonTagWalkable( void )
{
	// Set walkalbe faces viewable -- if they are not already.
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	CToolDisplace *pDispTool = GetDisplacementTool();
	if( !pDispTool )
		return;

	// Toggle the functionality.
	if ( GetTool() == FACEEDITTOOL_TAG_WALK )
	{
		// Set the select tool.
		SetTool( FACEEDITTOOL_SELECT );
		pDispTool->SetTool( DISPTOOL_SELECT_DISP_FACE );

		if ( m_bForceShowWalkable )
		{
			pDoc->SetDispDrawWalkable( false );
			m_bForceShowWalkable = false;
		}
	}
	else
	{
		// Set the tag walkable tool.
		SetTool( FACEEDITTOOL_TAG_WALK );
		pDispTool->SetTool( DISPTOOL_TAG_WALKABLE );

		if ( !pDoc->IsDispDrawWalkable() )
		{
			pDoc->SetDispDrawWalkable( true );
			m_bForceShowWalkable = true;
		}		
	}

	UpdateDialogData();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonTagBuildable( void )
{
	// Set buildalbe faces viewable -- if they are not already.
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	CToolDisplace *pDispTool = GetDisplacementTool();
	if( !pDispTool )
		return;

	// Toggle the functionality.
	if ( GetTool() == FACEEDITTOOL_TAG_BUILD )
	{
		// Set the select tool.
		SetTool( FACEEDITTOOL_SELECT );
		pDispTool->SetTool( DISPTOOL_SELECT_DISP_FACE );

		if ( m_bForceShowBuildable )
		{
			pDoc->SetDispDrawBuildable( false );
			m_bForceShowBuildable = false;
		}
	}
	else
	{
		// Set the tag buildable tool.
		SetTool( FACEEDITTOOL_TAG_BUILD );
		pDispTool->SetTool( DISPTOOL_TAG_BUILDABLE );

		if ( !pDoc->IsDispDrawBuildable() )
		{
			pDoc->SetDispDrawBuildable( true );
			m_bForceShowBuildable = true;
		}
	}

	UpdateDialogData();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonTagRemove( void )
{
	// Set removed faces viewable -- if they are not already.
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	CToolDisplace *pDispTool = GetDisplacementTool();
	if( !pDispTool )
		return;

	// Toggle the functionality.
	if ( GetTool() == FACEEDITTOOL_TAG_REMOVE )
	{
		// Set the select tool.
		SetTool( FACEEDITTOOL_SELECT );
		pDispTool->SetTool( DISPTOOL_SELECT_DISP_FACE );
		pDoc->SetDispDrawRemove( false );
	}
	else
	{
		// Set the tag remove tool.
		SetTool( FACEEDITTOOL_TAG_REMOVE );
		pDispTool->SetTool( DISPTOOL_TAG_REMOVE );
		if ( !pDoc->IsDispDrawRemove() )
		{
			pDoc->SetDispDrawRemove( true );
		}
	}

	UpdateDialogData();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonInvertAlpha( void )
{
	// Invert the alpha channel on all the selected displacements.
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		int nFaceCount = pSheet->GetFaceListCount();
		for( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
			if( pFace )
			{
				if( pFace->HasDisp() )
				{
					EditDispHandle_t handle = pFace->GetDisp();
					CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
					pDisp->InvertAlpha();
				}
			}
		}
	}

	// Update
	PostToolUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: Select any displacement faces adjacent to the already-selected ones.
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnSelectAdjacent()
{
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();

	bool bSelectedAny = false;
	
	// For all selected displacements...
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( !pSheet )
		return;

	// check for faces
	int faceCount = pSheet->GetFaceListCount();
	if( faceCount == 0 )
		return;

	for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
	{
		// get current displacement
		CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
		if( !pFace || !pFace->HasDisp() )
			continue;

		EditDispHandle_t handle = pFace->GetDisp();
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		// Now test all other disps in the world. We could do the code commented below, but then
		// we won't be selecting adjacent neighbors of a different power.
		//
		//for ( int iEdge=0; iEdge < 4; iEdge++ )
		//{
		//	EditDispHandle_t hNeighbor = pDisp->GetEdgeNeighbor( iEdge );
		
		int totalDispCount = pDispMgr->WorldCount();
		for ( int iTestDisp=0; iTestDisp < totalDispCount; iTestDisp++ )
		{
			int edge1[4], edge2[4];
			CMapDisp *pNeighbor = pDispMgr->GetFromWorld( iTestDisp );
			if ( !pNeighbor || pNeighbor == pDisp || pDispMgr->NumSharedPoints( pDisp, pNeighbor, edge1, edge2 ) != 2 )
				continue;

			// Get its map face and solid.
			pFace = dynamic_cast< CMapFace* >( pNeighbor->GetParent() );
			if ( !pFace || pFace->GetSelectionState() != SELECT_NONE )
				continue;

			CMapSolid *pSolid = dynamic_cast< CMapSolid* >( pFace->GetParent() );
			if ( !pSolid || !pSolid->IsVisible() )
				continue;

			// The function we use to select a face wants the face's index into its CMapSolid, so we need that.
			int iFaceIndex = pSolid->GetFaceIndex( pFace );
			if ( iFaceIndex == -1 )
				continue;

			// Finally, select the face.
			pSheet->ClickFace( pSolid, iFaceIndex, CFaceEditSheet::cfSelect, CFaceEditSheet::ModeSelect );
			bSelectedAny = true;
		}
	}
	
	if ( bSelectedAny )
	{
		CMapDoc::GetActiveMapDoc()->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	}
}

//-----------------------------------------------------------------------------
// Resample
//-----------------------------------------------------------------------------
void CFaceEditDispPage::UpdatePower( CMapDisp *pDisp )
{
	// input check
	if( !pDisp )
		return;

	//
	// get the power from the power edit box
	//
	CString strPower;
	GetDlgItemText( ID_DISP_POWER, strPower );
	int power = atoi( strPower );

	if( power < 2 ) { power = 2; }
	if( power > 4 ) { power = 4; }

	// check for a change and resample if need be
	if( power != pDisp->GetPower() )
	{
		pDisp->Resample( power );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::UpdateElevation( CMapDisp *pDisp )
{
	// input check
	if( !pDisp )
		return;

	//
	// get current elevation value
	//
	CString strElevation;
	GetDlgItemText( ID_DISP_ELEVATION, strElevation );
	int elevation = atof( strElevation );

	// check for change and set new elevation if need be
	if( elevation != pDisp->GetElevation() )
	{
		pDisp->Elevate( elevation );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pNMHDR - 
//			pResult - 
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnSpinUpDown( NMHDR *pNMHDR, LRESULT *pResult ) 
{
	//
	// get scroll up down edit box
	//
	NM_UPDOWN *pNMUpDown = ( NM_UPDOWN* )pNMHDR;
	switch( pNMUpDown->hdr.idFrom )
	{
		case ID_SPIN_DISP_POWER:
		{
			CEdit *pEdit = ( CEdit* )GetDlgItem( ID_DISP_POWER );
			CString strPower;
			pEdit->GetWindowText( strPower );
			int power = atoi( strPower );
			power += ( -pNMUpDown->iDelta );
			if( power < 2 ) { power = 2; }
			if( power > 4 ) { power = 4; }
			strPower.Format( "%d", power );
			pEdit->SetWindowText( strPower );
			*pResult = 0;
			break;
		}

		case ID_SPIN_DISP_ELEVATION:
		{
			CEdit *pEdit = ( CEdit* )GetDlgItem( ID_DISP_ELEVATION );
			CString strElevation;
			pEdit->GetWindowText( strElevation );
			float elevation = atof( strElevation );
			elevation += 0.5f * ( -pNMUpDown->iDelta );
			strElevation.Format( "%4.2f", elevation );
			pEdit->SetWindowText( strElevation );
			*pResult = 0;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::UpdateScale( CMapDisp *pDisp )
{
	if( !pDisp )
		return;

	//
	// get the current scale value
	//
	CString strScale;
	GetDlgItemText( ID_DISP_SCALE, strScale );
	float scale = atof( strScale );

	// check for change and set new scale if need be
	if( scale != pDisp->GetScale() )
	{
		pDisp->Scale( scale );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::OnButtonApply( void )
{
	//
	// "create" all selected faces
	//
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if( pSheet )
	{
		int faceCount = pSheet->GetFaceListCount();
		for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
		{
			// get current face
			CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
			if( pFace )
			{
				if( pFace->HasDisp() )
				{
					EditDispHandle_t handle = pFace->GetDisp();
					CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

					// update attribs
					UpdatePower( pDisp );
					UpdateElevation( pDisp );
					UpdateScale( pDisp );
				}
			}
		}
	}

	// update
	PostToolUpdate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::CloseAllDialogs( void )
{
	if( m_uiTool == FACEEDITTOOL_PAINTGEO )
	{
		m_PaintDistDlg.DestroyWindow();
	}

	if( m_uiTool == FACEEDITTOOL_PAINTSCULPT )
	{
		m_PaintSculptDlg.DestroyWindow();
	}

	if( m_uiTool == FACEEDITTOOL_PAINTDATA )
	{
		m_PaintDataDlg.DestroyWindow();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditDispPage::ResetForceShows( void )
{
	// Get the active map doc.
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	// Walkable
	if ( m_bForceShowWalkable )
	{
		if ( pDoc )
		{
			pDoc->SetDispDrawWalkable( false );
		}
		m_bForceShowWalkable = false;
	}

	// Buildable
	if ( m_bForceShowBuildable )
	{
		if ( pDoc )
		{
			pDoc->SetDispDrawBuildable( false );
		}
		m_bForceShowBuildable = false;
	}

	// Always force this off!
	if ( pDoc )
	{
		pDoc->SetDispDrawRemove( false );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditDispPage::OnSetActive( void )
{
	ToolManager()->SetTool( TOOL_FACEEDIT_DISP );

	// set the selection as the tool to use!!!
	OnButtonSelect();

	return CPropertyPage::OnSetActive();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditDispPage::OnKillActive( void )
{
	CloseAllDialogs();
	ResetForceShows();
	return TRUE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditDispPage::PreTranslateMessage( MSG *pMsg )
{
	HACCEL hAccel = GetMainWnd()->GetAccelTable();
	if( !(hAccel && ::TranslateAccelerator( GetMainWnd()->m_hWnd, hAccel, pMsg ) ) )
	{
		return CPropertyPage::PreTranslateMessage( pMsg );
	}
	else
	{
		return TRUE;
	}
}

