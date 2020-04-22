//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "hammer.h"
#include "ToolDisplace.h"
#include "MainFrm.h"
#include "FaceEditSheet.h"
#include "GlobalFunctions.h"
#include "MapAtom.h"
#include "MapSolid.h"
#include "MapView3D.h"
#include "History.h"
#include "Camera.h"
#include "MapDoc.h"
#include "ChunkFile.h"
#include "ToolManager.h"
#include "SculptOptions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CToolDisplace::CToolDisplace()
{
    m_uiTool = DISPTOOL_SELECT;
	m_uiEffect = DISPPAINT_EFFECT_RAISELOWER;
	m_uiBrushType = DISPPAINT_BRUSHTYPE_SOFT;

	m_iPaintChannel = DISPPAINT_CHANNEL_POSITION;
	m_flPaintValueGeo = 5.0f;
	m_flPaintValueData = 25.0f;
	m_iPaintAxis = DISPPAINT_AXIS_FACE;
	m_vecPaintAxis.Init( 0.0f, 0.0f, 1.0f );

	m_bAutoSew = false;
	m_bSpatial = false;
	m_flSpatialRadius = 15.0f;
	m_bSpatialRadius = false;

	m_bSelectMaskTool = true;
	m_bGridMaskTool = false;

	m_bLMBDown = false;
	m_bRMBDown = false;

	m_bNudge = false;
	m_bNudgeInit = false;
	m_EditDispHandle = EDITDISPHANDLE_INVALID;

	// load filters from file
	static char szProgramDir[MAX_PATH];
	APP()->GetDirectory( DIR_PROGRAM, szProgramDir );
	strcat( szProgramDir, "filters\\dispfilters.txt" );
	LoadFilters( szProgramDir );
	AddFiltersToManagers();

	m_SculptTool = NULL;
	m_MousePoint.Init( 0.0f, 0.0f );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CToolDisplace::~CToolDisplace()
{
	// destroy filters
	m_FilterRaiseLowerMgr.Destroy();
	m_FilterRaiseToMgr.Destroy();
	m_FilterSmoothMgr.Destroy();
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is activated.
// Input  : eOldTool - The ID of the previously active tool.
//-----------------------------------------------------------------------------
void CToolDisplace::OnActivate()
{
	//
	// initialize masks
	//
	CMapDisp::SetSelectMask( m_bSelectMaskTool );
	CMapDisp::SetGridMask( m_bGridMaskTool );
}
 
   
//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void CToolDisplace::OnDeactivate()
{
	//
	// reset masks
	//
	CMapDisp::SetSelectMask( false );
	CMapDisp::SetGridMask( false );
	
	if ( m_pDocument->GetTools()->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL )
	{
		// Clear the selected faces when we are deactivated.
		m_pDocument->SelectFace(NULL, 0, scClear );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::UpdateMapViews( CMapView3D *pView )
{
	CMapDoc *pDoc = pView->GetMapDoc();
	if( pDoc )
	{
		pDoc->SetModifiedFlag();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::CalcViewCenter( CMapView3D *pView )
{
	CRect windowRect;
	pView->GetWindowRect( windowRect );
	m_viewCenter = windowRect.CenterPoint();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// Set down flags
	m_bLMBDown = true;

	if( m_uiTool == DISPTOOL_PAINT_SCULPT )
	{
		m_SculptTool->OnLMouseDown3D( pView, nFlags, vPoint );
		m_SculptTool->BeginPaint( pView, vPoint );
		ApplySculptSpatialPaintTool( pView, nFlags, vPoint );

		// update
		UpdateMapViews( pView );

		return true;
	}

	// Selection.
	if( m_uiTool == DISPTOOL_SELECT || ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) )
	{
		// handle selection at point
		HandleSelection( pView, vPoint );

		// update
		UpdateMapViews( pView );
		return true;
	}

	// Tagging.
	if ( m_uiTool == DISPTOOL_TAG_WALKABLE || m_uiTool == DISPTOOL_TAG_BUILDABLE || m_uiTool == DISPTOOL_TAG_REMOVE )
	{
		// Do tagging.
		HandleTagging( pView, vPoint );
		return true;
	}

	// Resize the spatial painting sphere.
	if( ( m_uiTool == DISPTOOL_PAINT ) && ( IsSpatialPainting() ) &&
		( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
	{
		ResizeSpatialRadius_Activate( pView );
		return true;
	}

	// Nudging.
	if( ( m_uiTool == DISPTOOL_PAINT ) && ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) )
	{
		// is the current effect raise/lower (nudge only works in raise/lower mode)
		if( m_uiEffect == DISPPAINT_EFFECT_RAISELOWER )
		{
			EditDispHandle_t handle = GetHitPos( pView, vPoint );
			if( handle != EDITDISPHANDLE_INVALID )
			{
				m_EditDispHandle = handle;

				Nudge_Activate( pView, handle );
				UpdateMapViews( pView );
				return true;
			}
		}
	}

	// Painting.
	if( m_uiTool == DISPTOOL_PAINT )
	{
		// get hit info
		EditDispHandle_t handle = GetHitPos( pView, vPoint );
		if( handle == EDITDISPHANDLE_INVALID )
			return false;
		m_EditDispHandle = handle;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if( !pDispMgr )
			return false;

		pDispMgr->PreUndo( "Displacement Modifier" );

		// Paint using the correct mode.
		if ( m_bSpatial )
		{
			int nDispCount = pDispMgr->SelectCount();
			for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
			{
				CMapDisp *pDispSelect = pDispMgr->GetFromSelect( iDisp );
				if ( pDispSelect )
				{
					pDispSelect->Paint_Init( DISPPAINT_CHANNEL_POSITION );
				}
			}

			// setup for undo - modifying the displacement (painting)
			ApplySpatialPaintTool( nFlags, vPoint, pDisp );
		}
		else
		{
			// setup for undo - modifying the displacement (painting)
			pDispMgr->Undo( handle, true );
			pDisp = EditDispMgr()->GetDisp( handle );
			ApplyPaintTool( nFlags, vPoint, pDisp );    
		}

		// update
		UpdateMapViews( pView );
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// left button up
	m_bLMBDown = false;

	if( m_uiTool == DISPTOOL_PAINT_SCULPT )
	{
		m_SculptTool->OnLMouseUp3D( pView, nFlags, vPoint );
		return true;
	}

	if ( m_bNudge )
	{
		Nudge_Deactivate();
	}

	if ( m_bSpatialRadius )
	{
		ResizeSpatialRadius_Deactivate();
	}

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PostUndo();
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// left button down
    m_bRMBDown = true;

	if( m_uiTool == DISPTOOL_PAINT_SCULPT )
	{
		m_SculptTool->OnRMouseDown3D( pView, nFlags, vPoint );
		m_SculptTool->BeginPaint( pView, vPoint );
		ApplySculptSpatialPaintTool( pView, nFlags, vPoint );

		// update
		UpdateMapViews( pView );

		return true;
	}

	//
	// lifting the face normal - painting with the axis set to "Face Normal"
	//
	if( ( m_uiTool == DISPTOOL_PAINT ) && ( m_iPaintAxis == DISPPAINT_AXIS_FACE ) &&
		( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
	{
		LiftFaceNormal( pView, vPoint );
		return true;
	}

	// Tagging.
	if ( m_uiTool == DISPTOOL_TAG_WALKABLE || m_uiTool == DISPTOOL_TAG_BUILDABLE || m_uiTool == DISPTOOL_TAG_REMOVE )
	{
		// Do tagging.
		HandleTaggingReset( pView, vPoint );
		return true;
	}

	//
	// handle the normal paint procedure
	//
	if( m_uiTool == DISPTOOL_PAINT )
	{
		// get hit info
		EditDispHandle_t handle = GetHitPos( pView, vPoint );
		if( handle == EDITDISPHANDLE_INVALID )
			return false;
		m_EditDispHandle = handle;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
		if( !pDispMgr )
			return false;

		pDispMgr->PreUndo( "Displacement Modifier" );

		// apply the current displacement tool
		if ( m_bSpatial )
		{
			int nDispCount = pDispMgr->SelectCount();
			for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
			{
				CMapDisp *pDispSelect = pDispMgr->GetFromSelect( iDisp );
				if ( pDispSelect )
				{
					pDispSelect->Paint_Init( DISPPAINT_CHANNEL_POSITION );
				}
			}

			ApplySpatialPaintTool( nFlags, vPoint, pDisp );
		}
		else
		{
			// setup for undo
			pDispMgr->Undo( handle, true );
			pDisp = EditDispMgr()->GetDisp( handle );
			ApplyPaintTool( nFlags, vPoint, pDisp );    
		}

		// update
		UpdateMapViews( pView );
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// left button up
	m_bRMBDown = false;

	if( m_uiTool == DISPTOOL_PAINT_SCULPT )
	{
		m_SculptTool->OnRMouseUp3D( pView, nFlags, vPoint );
		return true;
	}

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( pDispMgr )
	{
		pDispMgr->PostUndo();
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	m_MousePoint = vPoint;

	if( m_uiTool == DISPTOOL_PAINT_SCULPT )
	{
		m_SculptTool->OnMouseMove3D( pView, nFlags, vPoint );

		if( ( m_bLMBDown || m_bRMBDown ) )
		{	
			ApplySculptSpatialPaintTool( pView, nFlags, vPoint );
		}

		// update
		UpdateMapViews( pView );

		return true;
	}

	// nudging
	if ( ( m_uiTool == DISPTOOL_PAINT ) && ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) && 
		m_bLMBDown && m_bNudge )
	{
		Nudge_Do();
	}
	// Resizing the spatial sphere.
	else if ( ( m_uiTool == DISPTOOL_PAINT ) && ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) &&
		      m_bLMBDown && m_bSpatialRadius )
	{
		ResizeSpatialRadius_Do();
	}
	// painting
	else
	{
		// get hit info
		EditDispHandle_t handle = GetHitPos( pView, vPoint );
		if( handle == EDITDISPHANDLE_INVALID )
			return false;
		m_EditDispHandle = handle;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		//
		// continue with tool operation?!
		//
		if( ( m_bLMBDown || m_bRMBDown ) && !( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) &&
			( m_uiTool == DISPTOOL_PAINT ) )
		{	
			if ( m_bSpatial )
			{
				ApplySpatialPaintTool( nFlags, vPoint, pDisp );
			}
			else
			{
				ApplyPaintTool( nFlags, vPoint, pDisp );
			}
		}

		// not nudging anymore -- if we were
		if( m_bNudge )
		{
			Nudge_Deactivate();
		}

		if ( m_bSpatialRadius )
		{
			ResizeSpatialRadius_Deactivate();
		}
	}

	// update
	UpdateMapViews( pView );

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::LiftFaceNormal( CMapView3D *pView, const Vector2D &vPoint )
{
	//
	// check for closest solid object
	//
	ULONG		ulFace;
	CMapClass	*pObject;

	if( ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL ) )
	{
		if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
		{
			// get the solid
			CMapSolid *pSolid = ( CMapSolid* )pObject;
			if( !pSolid )
				return;

			// trace a line and get the normal -- will get a displacement normal
			// if one exists
			CMapFace *pFace = pSolid->GetFace( ulFace );
			if( !pFace )
				return;

			Vector vRayStart, vRayEnd;
			pView->GetCamera()->BuildRay( vPoint, vRayStart, vRayEnd );

			Vector vHitPos, vHitNormal;
			if( pFace->TraceLine( vHitPos, vHitNormal, vRayStart, vRayEnd ) )
			{
				// set the paint direction
				m_vecPaintAxis = vHitNormal;
			}
			else
			{
				// will default to z if no normal found
				m_vecPaintAxis.Init( 0.0f, 0.0f, 1.0f );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::Nudge_Activate( CMapView3D *pView, EditDispHandle_t dispHandle )
{
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return;

	pDispMgr->PreUndo( "Displacement Nudge" );

	// Setup paint (nudge) using the correct mode.
	if ( m_bSpatial )
	{
		int nDispCount = pDispMgr->SelectCount();
		for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
		{
			CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
			if ( pDisp )
			{
				pDisp->Paint_Init( DISPPAINT_CHANNEL_POSITION );
			}
		}
	}
	else
	{
		// setup for undo
		pDispMgr->Undo( dispHandle, true );
	}

	// setup the cursor for "nudging"
	CalcViewCenter( pView );
	SetCursorPos( m_viewCenter.x, m_viewCenter.y );
	pView->SetCapture();

	// set nudging
	m_bNudge = true;
	m_bNudgeInit = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::Nudge_Deactivate( void )
{
	ReleaseCapture();
	m_bNudge = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::Nudge_Do( void )
{
	CMapDisp *pNudgeDisp = GetEditDisp();
	if (pNudgeDisp == NULL)
	{
		return;
	}

	//
	// find the greatest delta and "nudge"
	//
	// NOTE: using get cursor position, because it is different than the 
	//       "point" incoming into mouse move????
	//
	CPoint nudgePos;
	GetCursorPos( &nudgePos );

	CPoint nudgeDelta;
	nudgeDelta.x = nudgePos.x - m_viewCenter.x;
	nudgeDelta.y = nudgePos.y - m_viewCenter.y;

	float delta;
	if( abs( nudgeDelta.x ) < abs( nudgeDelta.y ) )
	{
		delta = nudgeDelta.y;
	}
	else
	{
		delta = nudgeDelta.x;
	}
	delta = -delta;

	if ( !IsSpatialPainting() )
	{
		CDispMapImageFilter *pFilter = m_FilterRaiseLowerMgr.GetActiveFilter();
		if( !pFilter )
			return;
		
		// set the dynamic filter data
		pFilter->m_DataType = DISPPAINT_CHANNEL_POSITION;
		pFilter->m_Scale = ( delta * 0.25 ) * ( float )( ( int )( m_flPaintValueGeo / 10.0f ) + 1 ) ;
		
		// apply the filter to the displacement surface(s)
		m_FilterRaiseLowerMgr.Apply( pFilter, pNudgeDisp, m_iPaintAxis, m_vecPaintAxis, m_bAutoSew );
	}
	else
	{
		// Get the hit index and check for validity.
		int iHit = pNudgeDisp->GetTexelHitIndex();
		if ( iHit != -1 )
		{
			// Initialize the spatial paint data.
			SpatialPaintData_t spatialData;
			spatialData.m_nEffect = DISPPAINT_EFFECT_RAISELOWER;
			spatialData.m_uiBrushType = m_uiBrushType;
			spatialData.m_flRadius = m_flSpatialRadius;
			spatialData.m_flScalar = delta;
			spatialData.m_bNudge = true;
			spatialData.m_bNudgeInit = m_bNudgeInit;
			pNudgeDisp->GetVert( iHit, spatialData.m_vCenter );
			VectorCopy( m_vecPaintAxis, spatialData.m_vPaintAxis );
			
			m_DispPaintMgr.Paint( spatialData, m_bAutoSew );

			// Done with the init.
			m_bNudgeInit = false;
		}
	}

	// reset the cursor pos
	SetCursorPos( m_viewCenter.x, m_viewCenter.y );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::ApplyPaintTool( UINT nFlags, const Vector2D &vPoint, CMapDisp *pDisp )
{
	switch( m_uiEffect )
	{ 
	case DISPPAINT_EFFECT_RAISELOWER:
		{
			CDispMapImageFilter *pFilter = m_FilterRaiseLowerMgr.GetActiveFilter();
			if( pFilter )
			{
				pFilter->m_DataType = m_iPaintChannel;
				if ( m_iPaintChannel == DISPPAINT_CHANNEL_POSITION )
				{
					pFilter->m_Scale = m_flPaintValueGeo;
				}
				else if ( m_iPaintChannel == DISPPAINT_CHANNEL_ALPHA )
				{
					pFilter->m_Scale = m_flPaintValueData;
				}

				if( m_bRMBDown )
				{
					pFilter->m_Scale = -pFilter->m_Scale;
				}
				
				// apply the filter to the displacement surface(s)
				m_FilterRaiseLowerMgr.Apply( pFilter, pDisp, m_iPaintAxis, m_vecPaintAxis, m_bAutoSew );
			}
			return;
		}
	case DISPPAINT_EFFECT_MODULATE:
		{
			// no modulate filters or filter manager currently!
			return;
		}
	case DISPPAINT_EFFECT_SMOOTH:
		{
			CDispMapImageFilter *pFilter = m_FilterSmoothMgr.GetActiveFilter();
			if( pFilter )
			{
				pFilter->m_DataType = m_iPaintChannel;
				pFilter->m_Scale = 1.0f;
				
				int areaValue = 3;
				if ( m_iPaintChannel == DISPPAINT_CHANNEL_POSITION )
				{
					areaValue = ( m_flPaintValueGeo * 2 ) + 1;
				}
				else if ( m_iPaintChannel == DISPPAINT_CHANNEL_ALPHA )
				{
					areaValue = ( m_flPaintValueData * 2 ) + 1;
				}				
				if( areaValue < 3 ) { areaValue = 3; }
				if( areaValue > 7 ) { areaValue = 7; }
				
				pFilter->m_AreaHeight = areaValue;
				pFilter->m_AreaWidth = areaValue;

				// apply the filter to the displacement surface(s)
				m_FilterSmoothMgr.Apply( pFilter, pDisp, m_iPaintAxis, m_vecPaintAxis, m_bAutoSew );
			}
			return;
		}
	case DISPPAINT_EFFECT_RAISETO:
		{
			CDispMapImageFilter *pFilter = m_FilterRaiseToMgr.GetActiveFilter();
			if( pFilter )
			{
				pFilter->m_DataType = m_iPaintChannel;
				if ( m_iPaintChannel == DISPPAINT_CHANNEL_POSITION )
				{
					pFilter->m_Scale = m_flPaintValueGeo;
				}
				else if ( m_iPaintChannel == DISPPAINT_CHANNEL_ALPHA )
				{
					pFilter->m_Scale = m_flPaintValueData;
				}

				// apply the filter to the displacement surface(s)
				m_FilterRaiseToMgr.Apply( pFilter, pDisp, m_iPaintAxis, m_vecPaintAxis, m_bAutoSew );
			}
			return;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::ApplySpatialPaintTool( UINT nFlags, const Vector2D &vPoint, CMapDisp *pDisp )
{
	// Right mouse button only used to paint in a Raise/Lower situation.
	if ( ( m_uiEffect != DISPPAINT_EFFECT_RAISELOWER ) && m_bRMBDown )
		return;

	// Get the hit index and check for validity.
	int iHit = pDisp->GetTexelHitIndex();
	if ( iHit == -1 )
		return;

	// Initialize the spatial paint data.
	SpatialPaintData_t spatialData;
	spatialData.m_nEffect = m_uiEffect;
	spatialData.m_uiBrushType = m_uiBrushType;
	spatialData.m_flRadius = m_flSpatialRadius;
	spatialData.m_flScalar = m_flPaintValueGeo;
	spatialData.m_bNudge = false;
	if ( m_bRMBDown )
	{
		spatialData.m_flScalar = -spatialData.m_flScalar;
	}
	pDisp->GetVert( iHit, spatialData.m_vCenter );
	VectorCopy( m_vecPaintAxis, spatialData.m_vPaintAxis );

	m_DispPaintMgr.Paint( spatialData, m_bAutoSew );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::ApplySculptSpatialPaintTool( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// Initialize the spatial paint data.
	SpatialPaintData_t spatialData;

	spatialData.m_vCenter.Init();

	// get hit info
	EditDispHandle_t handle = GetHitPos( pView, vPoint );
	if( handle != EDITDISPHANDLE_INVALID )
	{
		m_EditDispHandle = handle;
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		// Get the hit index and check for validity.
		int iHit = pDisp->GetTexelHitIndex();
		if ( iHit != -1 )
		{
			pDisp->GetVert( iHit, spatialData.m_vCenter );
		}
	}

	spatialData.m_nEffect = m_uiEffect;
	spatialData.m_uiBrushType = m_uiBrushType;
	spatialData.m_flRadius = m_flSpatialRadius;
	spatialData.m_flScalar = m_flPaintValueGeo;
	spatialData.m_bNudge = false;
	if ( m_bRMBDown )
	{
		spatialData.m_flScalar = -spatialData.m_flScalar;
	}
	VectorCopy( m_vecPaintAxis, spatialData.m_vPaintAxis );

	m_SculptTool->Paint( pView, vPoint, spatialData );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::ResizeSpatialRadius_Activate( CMapView3D *pView )
{
	// Calculate the center of the view and capture the mouse cursor.
	CalcViewCenter( pView );
	SetCursorPos( m_viewCenter.x, m_viewCenter.y );
	pView->SetCapture();

	m_bSpatialRadius = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::ResizeSpatialRadius_Deactivate( void )
{
	ReleaseCapture();
	m_bSpatialRadius = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::ResizeSpatialRadius_Do( void )
{
	CPoint cursorPos;
	GetCursorPos( &cursorPos );

	// Calculate the delta between the cursor from last frame and this one.
	CPoint cursorDelta;
	cursorDelta.x = cursorPos.x - m_viewCenter.x;
	cursorDelta.y = cursorPos.y - m_viewCenter.y;

	float flDelta;
	if( abs( cursorDelta.x ) < abs( cursorDelta.y ) )
	{
		flDelta = cursorDelta.y;
	}
	else
	{
		flDelta = cursorDelta.x;
	}
	flDelta = -flDelta;

	// Adjust the sphere radius.
	m_flSpatialRadius += flDelta;

	// reset the cursor pos
	SetCursorPos( m_viewCenter.x, m_viewCenter.y );

	//
	// Update the paint dialog.
	//
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( pSheet )
	{
		pSheet->m_DispPage.UpdatePaintDialogs();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::HandleSelection( CMapView3D *pView, const Vector2D &vPoint )
{
	//
	// check for closest solid object
	//
	ULONG		ulFace;
	CMapClass	*pObject;

	bool bShift = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );

	if( ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL ) )
	{
		if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
		{
			// get the solid
			CMapSolid *pSolid = ( CMapSolid* )pObject;
			
			// setup initial command state
			int cmd = scToggle | scClear;
			
			//
			// don't "clear" if CTRL is pressed
			//
			if( GetAsyncKeyState( VK_CONTROL ) & 0x8000 )
			{
				cmd &= ~scClear;
			}
			
			CMapDoc *pDoc = pView->GetMapDoc();
			if( !pDoc )
				return;
			
			// If they are holding down SHIFT, select the entire solid.
			if ( bShift )
			{
				pDoc->SelectFace( pSolid, -1, cmd );
			}
			// Otherwise, select a single face.
			else
			{
				pDoc->SelectFace( pSolid, ulFace, cmd );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle the overriding of displacement triangle tag.
//-----------------------------------------------------------------------------
void CToolDisplace::HandleTagging( CMapView3D *pView, const Vector2D &vPoint )
{
	// Get the displacement face (if any) at the 2d point.
	ULONG ulFace;
	CMapClass *pObject = NULL;

	if( ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL ) )
	{
		if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
		{
			// Get the face and check for a displacement.
			CMapSolid *pSolid = ( CMapSolid* )pObject;
			CMapFace *pFace = pSolid->GetFace( ( int )ulFace );
			if ( pFace && pFace->HasDisp() )
			{
				EditDispHandle_t hDisp = pFace->GetDisp();
				CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );

				Vector vecStart, vecEnd;
				pView->GetCamera()->BuildRay( vPoint, vecStart, vecEnd );

				float flFraction;
				int iTri = pDisp->CollideWithDispTri( vecStart, vecEnd, flFraction );
				if ( iTri != -1 )
				{
					if ( m_uiTool == DISPTOOL_TAG_WALKABLE )
					{
						if ( pDisp->IsTriTag( iTri, COREDISPTRI_TAG_FORCE_WALKABLE_BIT ) )
						{
							pDisp->ToggleTriTag( iTri, COREDISPTRI_TAG_FORCE_WALKABLE_VAL );
						}
						else
						{
							pDisp->SetTriTag( iTri, COREDISPTRI_TAG_FORCE_WALKABLE_BIT );
							if ( !pDisp->IsTriTag( iTri, COREDISPTRI_TAG_WALKABLE ) )
							{
								pDisp->SetTriTag( iTri, COREDISPTRI_TAG_FORCE_WALKABLE_VAL );
							}
							else
							{
								pDisp->ResetTriTag( iTri, COREDISPTRI_TAG_FORCE_WALKABLE_VAL );
							}
						}

						pDisp->UpdateWalkable();
					}
					else if ( m_uiTool == DISPTOOL_TAG_BUILDABLE )
					{
						if ( pDisp->IsTriTag( iTri, COREDISPTRI_TAG_FORCE_BUILDABLE_BIT ) )
						{
							pDisp->ToggleTriTag( iTri, COREDISPTRI_TAG_FORCE_BUILDABLE_VAL );
						}
						else
						{
							pDisp->SetTriTag( iTri, COREDISPTRI_TAG_FORCE_BUILDABLE_BIT );
							if ( !pDisp->IsTriTag( iTri, COREDISPTRI_TAG_BUILDABLE ) )
							{
								pDisp->SetTriTag( iTri, COREDISPTRI_TAG_FORCE_BUILDABLE_VAL );
							}
							else
							{
								pDisp->ResetTriTag( iTri, COREDISPTRI_TAG_FORCE_BUILDABLE_VAL );
							}
						}

						pDisp->UpdateBuildable();
					}
					else if ( m_uiTool == DISPTOOL_TAG_REMOVE )
					{
						HandleTaggingRemove( pDisp, iTri );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CToolDisplace::HandleTaggingRemove( CMapDisp *pDisp, int nTriIndex )
{
	pDisp->ToggleTriTag( nTriIndex, COREDISPTRI_TAG_FORCE_REMOVE_BIT );
	pDisp->UpdateTriRemove();
}


//-----------------------------------------------------------------------------
// Purpose: Handle the overriding of displacement triangle tag.
//-----------------------------------------------------------------------------
void CToolDisplace::HandleTaggingReset( CMapView3D *pView, const Vector2D &vPoint )
{
	// Get the displacement face (if any) at the 2d point.
	ULONG ulFace;
	CMapClass *pObject = NULL;

	if( ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL ) )
	{
		if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
		{
			// Get the face and check for a displacement.
			CMapSolid *pSolid = ( CMapSolid* )pObject;
			CMapFace *pFace = pSolid->GetFace( ( int )ulFace );
			if ( pFace && pFace->HasDisp() )
			{
				EditDispHandle_t hDisp = pFace->GetDisp();
				CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );

				Vector vecStart, vecEnd;
				pView->GetCamera()->BuildRay( vPoint, vecStart, vecEnd );

				float flFraction;
				int iTri = pDisp->CollideWithDispTri( vecStart, vecEnd, flFraction );
				if ( iTri != -1 )
				{
					if ( m_uiTool == DISPTOOL_TAG_WALKABLE )
					{
						pDisp->ResetTriTag( iTri, COREDISPTRI_TAG_FORCE_WALKABLE_BIT );
						pDisp->UpdateWalkable();
					}
					else if ( m_uiTool == DISPTOOL_TAG_BUILDABLE )
					{
						pDisp->ResetTriTag( iTri, COREDISPTRI_TAG_FORCE_BUILDABLE_BIT );
						pDisp->UpdateBuildable();
					}
					else if ( m_uiTool == DISPTOOL_TAG_REMOVE )
					{
						pDisp->ResetTriTag( iTri, COREDISPTRI_TAG_FORCE_REMOVE_BIT );
						pDisp->UpdateBuildable();
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
EditDispHandle_t CToolDisplace::GetHitPos( CMapView3D *pView, const Vector2D &vPoint )
{
	//
	// get ray info
	//
	Vector rayStart, rayEnd;
	pView->GetCamera()->BuildRay( vPoint, rayStart, rayEnd );

	// generate selected displacement list
	int dispCount = GetSelectedDisps();
	if( dispCount == 0 )
		return NULL;

	// collide against all "active" displacements and set texel hit data
	return CollideWithSelectedDisps( rayStart, rayEnd );	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CToolDisplace::GetSelectedDisps( void )
{
	//
	// get a valid displacement manager
	//
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return 0;

	// clear the selection list
	pDispMgr->SelectClear();

	//
	// add all selected displacements to "displacement manager"'s selection list
	//
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( !pSheet )
		return 0;

	int faceCount = pSheet->GetFaceListCount();
	for( int i = 0; i < faceCount; i++ )
	{
		CMapFace *pFace = pSheet->GetFaceListDataFace( i );
		if( !pFace )
			continue;

		if( pFace->HasDisp() )
		{
			EditDispHandle_t handle = pFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
			pDisp->ResetTexelHitIndex();
			pDispMgr->AddToSelect( handle );
		}
	}

	// return the number of displacements in list
	return pDispMgr->SelectCount();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
EditDispHandle_t CToolDisplace::CollideWithSelectedDisps( const Vector &rayStart, const Vector &rayEnd )
{
	EditDispHandle_t handle = EDITDISPHANDLE_INVALID;
	float			 minDist = 99999.9f;
	int				 minIndex = -1;

	//
	// get a valid displacement manager
	//
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return NULL;

	int dispCount = pDispMgr->SelectCount();
	for( int i = 0; i < dispCount; i++ )
	{
		// get the current displacement
		CMapDisp *pDisp = pDispMgr->GetFromSelect( i );
		if( !pDisp )
			continue;

		bool bCollide = RayAABBTest( pDisp, rayStart, rayEnd );
		if( bCollide )
		{
			Vector point;
			int size = pDisp->GetSize();
			for( int j = 0; j < size; j++ )
			{
				// get current point
				pDisp->GetVert( j, point );

				// find point closest to ray
				float dist = DistFromPointToRay( rayStart, rayEnd, point );
				if( dist < minDist )
				{
					CMapFace *pFace = ( CMapFace* )pDisp->GetParent();
					handle = pFace->GetDisp();
					minDist = dist;
					minIndex = j;
				}
			}
		}
	}

	//
	// set the texel hit index
	//
	if( handle != EDITDISPHANDLE_INVALID )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
		pDisp->SetTexelHitIndex( minIndex );
	}

	return handle;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::RayAABBTest( CMapDisp *pDisp, const Vector &rayStart, const Vector &rayEnd )
{
	//
	// make planes
	//
	PLANE planes[6];
	Vector boxMin, boxMax;
	pDisp->GetBoundingBox( boxMin, boxMax );
	BuildParallelepiped( boxMin, boxMax, planes );

	bool bCollide = false;
	for( int planeIndex = 0; planeIndex < 6; planeIndex++ )
	{
		bCollide = RayPlaneTest( &planes[planeIndex], rayStart, rayEnd );
		if( !bCollide )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::BuildParallelepiped( const Vector &boxMin, const Vector &boxMax,
							             PLANE planes[6] )
{
	int planeIndex = 0;
	for( int axis = 0; axis < 3; axis++ )
	{
		for( int direction = -1; direction < 2; direction += 2 )
		{
			// clear the current plane info
			VectorClear( planes[planeIndex].normal );
			planes[planeIndex].normal[axis] = direction;
			if( direction == 1 )
			{
				planes[planeIndex].dist = boxMax[axis];
			}
			else
			{
				planes[planeIndex].dist = -boxMin[axis];
			}

			planeIndex++;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::RayPlaneTest( PLANE *pPlane, const Vector& rayStart, const Vector& rayEnd /*, float *fraction*/ )
{
	//
	// get the distances both trace start and end from the bloated plane
	//
	float distStart = DotProduct( rayStart, pPlane->normal ) - pPlane->dist;
	float distEnd = DotProduct( rayEnd, pPlane->normal ) - pPlane->dist;

	//
	// no collision - both points are in front or behind of the given plane
	//
	if( ( distStart > 0.0f ) && ( distEnd > 0.0f ) )
		return false;

	if( ( distStart > 0.0f ) && ( distEnd > 0.0f ) )
		return false;

	// calculate the parameterized "t" component along the ray
	//*fraction = distStart / ( distStart - distEnd );

	// collision
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CToolDisplace::DistFromPointToRay( const Vector& rayStart, const Vector& rayEnd,
										 const Vector& point )
{
	//
	// calculate the ray
	//
	Vector ray;
	VectorSubtract( rayEnd, rayStart, ray );
	VectorNormalize( ray );

	//
	// get a ray to point
	//
	Vector seg;
	VectorSubtract( point, rayStart, seg );

	//
	// project point segment onto ray - get perpendicular point
	//
	float value = DotProduct( ray, seg );
	VectorScale( ray, value, ray );
	VectorAdd( rayStart, ray, ray );

	//
	// find the distance between the perpendicular point and point
	//
	VectorSubtract( ray, point, seg );
	float dist = VectorLength( seg );

	return dist;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolDisplace::AddFiltersToManagers( void )
{
	int count = m_FilterLoaderMgr.GetFilterCount();
	for( int ndxFilter = 0; ndxFilter < count; ndxFilter++ )
	{
		CDispMapImageFilter *pFilter = m_FilterLoaderMgr.GetFilter( ndxFilter );
		if( pFilter )
		{
			switch( pFilter->m_Type )
			{
			case DISPPAINT_EFFECT_RAISELOWER:
				{
					m_FilterRaiseLowerMgr.Add( pFilter );
					break;
				}
			case DISPPAINT_EFFECT_RAISETO:
				{
					m_FilterRaiseToMgr.Add( pFilter );
					break;
				}
			case DISPPAINT_EFFECT_SMOOTH:
				{
					m_FilterSmoothMgr.Add( pFilter );
					break;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolDisplace::LoadFilters( const char *filename )
{
	//
	// Open the file.
	//
	CChunkFile File;
	ChunkFileResult_t eResult = File.Open( filename, ChunkFile_Read );
	
	if( eResult != ChunkFile_Ok )
	{
		Msg( mwError, "Couldn't load filter file %s!\n", filename );
	}

	//
	// Read the file.
	//
	if (eResult == ChunkFile_Ok)
	{
		//
		// Set up handlers for the subchunks that we are interested in.
		//
		CChunkHandlerMap Handlers;
		Handlers.AddHandler( "Filter", ( ChunkHandler_t )CToolDisplace::LoadFiltersCallback, this );
		File.PushHandlers( &Handlers );

		//
		// Read the sub-chunks. We ignore keys in the root of the file, so we don't pass a
		// key value callback to ReadChunk.
		//
		while (eResult == ChunkFile_Ok)
		{
			eResult = File.ReadChunk();
		}

		if (eResult == ChunkFile_EOF)
		{
			eResult = ChunkFile_Ok;
		}

		File.PopHandlers();
	}

	return( eResult == ChunkFile_Ok );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CToolDisplace::LoadFiltersCallback( CChunkFile *pFile, CToolDisplace *pDisplaceTool )
{
	//
	// allocate a new filter
	//
	CDispMapImageFilter *pFilter = pDisplaceTool->m_FilterLoaderMgr.Create();
	if( !pFilter )
		return ChunkFile_Fail;

	// load the filter data
	ChunkFileResult_t eResult = pFilter->LoadFilter( pFile );
	return( eResult );	
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender - 
//			*pTool - 
//-----------------------------------------------------------------------------
void CToolDisplace::RenderPaintSphere( CRender3D *pRender )
{
	CMapDisp *pDisp = GetEditDisp();
	if (pDisp == NULL)
		return;
	
	// Get the sphere center.
	int iHit = pDisp->GetTexelHitIndex();
	if ( iHit == -1 )
		return;

	// Get the sphere center and radius.
	Vector vCenter;
	pDisp->GetVert( iHit, vCenter );
	float flRadius = GetSpatialRadius();

	int size = ( int )( flRadius * 0.05f );
	if ( size < 6 ) { size = 6; }
	if ( size > 12 ) { size = 12; }

	// Render the sphere.
	if ( !IsNudging() )
	{
		pRender->RenderWireframeSphere( vCenter, flRadius, size, size, 0, 255, 0 );
	}
	else
	{
		pRender->RenderWireframeSphere( vCenter, flRadius, size, size, 255, 255, 0 );
	}

	// Render the displacement axis (as an arrow).
	int nPaintAxis;
	Vector vPaintAxis;
	GetPaintAxis( nPaintAxis, vPaintAxis );
	if( nPaintAxis == DISPPAINT_AXIS_SUBDIV )
	{
		pDisp->GetSubdivNormal( iHit, vPaintAxis );
	}
	float flBloat = flRadius * 0.15f;
	pRender->RenderArrow( vCenter, vCenter + ( vPaintAxis * ( flRadius + flBloat ) ), 255, 255, 0 );

	// Render cube at center point.
 	Vector	vBoxMin, vBoxMax;
	for ( int iAxis = 0; iAxis < 3; iAxis++ )
	{
		vBoxMin[iAxis] = vCenter[iAxis] - ( flBloat * 0.25f );
		vBoxMax[iAxis] = vCenter[iAxis] + ( flBloat * 0.25f );
	}
	pRender->RenderBox( vBoxMin, vBoxMax, 255, 255, 0, SELECT_NONE );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//			bNudge - 
//-----------------------------------------------------------------------------
void CToolDisplace::RenderHitBox( CRender3D *pRender )
{
	CMapDisp *pDisp = GetEditDisp();
	if (pDisp == NULL)
		return;

    //
    // get selection
    //
    int index = pDisp->GetTexelHitIndex();
    if( index == -1 )
        return;

    //
    // get the displacement map width and height
    //
    int width = pDisp->GetWidth();
    int height = pDisp->GetHeight();

	Vector seg[2];
	Vector points[2];

	pDisp->GetVert( 0, points[0] );
	pDisp->GetVert( ( width - 1 ), points[1] );
	VectorSubtract( points[1], points[0], seg[0] );
	pDisp->GetVert( ( ( width - 1 ) * height ), points[1] );
	VectorSubtract( points[1], points[0], seg[1] );

	VectorAdd( seg[0], seg[1], seg[0] );
	VectorScale( seg[0], 0.5f, seg[0] );
    
    //
    // determine a good size to make the "box" surrounding the selected point
    //
	float length = VectorLength( seg[0] );
    length *= 0.025f;

    //
    // render the box
    //
	pDisp->GetVert( index, points[0] );

 	Vector	minb, maxb;
    minb[0] = points[0][0] - length;
    minb[1] = points[0][1] - length;
    minb[2] = points[0][2] - length;

    maxb[0] = points[0][0] + length;
    maxb[1] = points[0][1] + length;
    maxb[2] = points[0][2] + length;

	if( !IsNudging() )
	{
	    pRender->RenderWireframeBox( minb, maxb, 0, 255, 0 );
	}
	else
	{
	    pRender->RenderWireframeBox( minb, maxb, 255, 255, 0 );
	}

	//
	// render the normal
	//
	// get hit box origin
	Vector hbOrigin;
	pDisp->GetVert( index, hbOrigin );

	// get 4x length
	float length4 = length * 4.0f;

	int paintAxis;
	Vector vecPaint;
	GetPaintAxis( paintAxis, vecPaint );
	if( paintAxis == DISPPAINT_AXIS_SUBDIV )
	{
		pDisp->GetSubdivNormal( index, vecPaint );
	}

	//
	// render the normal -- just a yellow line at this point
	//
	pRender->RenderArrow( hbOrigin, hbOrigin + ( vecPaint * length4 ), 255, 255, 0 );
#if 0
	CMeshBuilder meshBuilder;
	IMesh *pMesh = MaterialSystemInterface()->GetDynamicMesh();
	
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );
	meshBuilder.Position3f( hbOrigin.x, hbOrigin.y, hbOrigin.z );
	meshBuilder.Color3ub( 255, 255, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f( hbOrigin.x + ( normal.x * length4 ), 
		                    hbOrigin.y + ( normal.y * length4 ), 
							hbOrigin.z + ( normal.z * length4 ) );
	meshBuilder.Color3ub( 255, 255, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.End();
	
	pMesh->Draw();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender - 
//-----------------------------------------------------------------------------
void CToolDisplace::RenderTool3D(CRender3D *pRender)
{
	unsigned int uiTool = GetTool();
	if ( uiTool == DISPTOOL_PAINT )
	{
		if ( IsSpatialPainting() )
		{
			RenderPaintSphere( pRender );
		}
		else
		{
			RenderHitBox( pRender );
		}
	}

	if ( uiTool == DISPTOOL_PAINT_SCULPT )
	{
		m_SculptTool->RenderTool3D( pRender );
	}
}
