//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "MapWorld.h"
#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "ToolOverlay.h"
#include "MapDoc.h"
#include "History.h"
#include "CollisionUtils.h"
#include "cmodel.h"
#include "MapView3D.h"
#include "MapView2D.h"
#include "MapSolid.h"
#include "Camera.h"
#include "ObjectProperties.h"  // FIXME: For ObjectProperties::RefreshData
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define OVERLAY_TOOL_SNAP_DISTANCE	35.0f

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CToolOverlay::CToolOverlay()
{
	m_bDragging = false;
	m_pActiveOverlay = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CToolOverlay::~CToolOverlay()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::OnActivate()
{
	m_bDragging = false;
	m_pActiveOverlay = NULL;
}
    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::OnDeactivate()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// Post drag events.
	PostDrag();

	// Update the entity properties dialog.
	GetMainWnd()->pObjectProperties->MarkDataDirty();

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	// Handle the overlay "handle" selection.
	if ( HandleSelection( pView, vPoint ) )
	{
		PreDrag();
		return true;
	}

	// Handle adding and removing overlay entities from the selection list.
	OverlaySelection( pView, nFlags, vPoint );

	// Handle the overlay creation and placement (if we hit a solid).
	ULONG ulFace;
	CMapClass *pObject = NULL;
	if ( ( pObject = pView->NearestObjectAt( vPoint, ulFace ) ) != NULL )
	{
		CMapSolid *pSolid = dynamic_cast<CMapSolid*>( pObject );
		if ( pSolid )
		{
			return CreateOverlay( pSolid, ulFace, pView, vPoint );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	if ( m_bDragging )
	{
		bool bShift = ( ( GetKeyState( VK_SHIFT ) & 0x8000 ) != 0 );

		// Build the ray and drag the overlay handle to the impact point.
		const CCamera *pCamera = pView->GetCamera();
		if ( pCamera )
		{
			Vector vecStart, vecEnd;
			pView->BuildRay( vPoint, vecStart, vecEnd );
			OnDrag( vecStart, vecEnd, bShift );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::CreateOverlay( CMapSolid *pSolid, ULONG iFace, CMapView3D *pView, Vector2D point )
{
	// Build a ray to trace against the face that they clicked on to
	// find the point of intersection.
	Vector vecStart, vecEnd;
	pView->BuildRay( point, vecStart, vecEnd );
	
	Vector vecHitPos, vecHitNormal;
	CMapFace *pFace = pSolid->GetFace( iFace );
	if( pFace->TraceLine( vecHitPos, vecHitNormal, vecStart, vecEnd ) )
	{
		// Create and initialize the "entity" --> "overlay."
		CMapEntity *pEntity = new CMapEntity;
		pEntity->SetKeyValue( "material", GetDefaultTextureName() );
		pEntity->SetPlaceholder( TRUE );
		pEntity->SetOrigin( vecHitPos );
		pEntity->SetClass( "info_overlay" );				
		
		// Add the entity to the world.
		m_pDocument->AddObjectToWorld( pEntity );
		
		// Setup "history."
		GetHistory()->MarkUndoPosition( NULL, "Create Overlay" );
		GetHistory()->KeepNew( pEntity );
				
		// Initialize the overlay.
		InitOverlay( pEntity, pFace );

		pEntity->CalcBounds( TRUE );
		
		// Add to selection list.
		m_pDocument->SelectObject( pEntity, scSelect );
		m_bEmpty = false;
		
		// Set modified and update views.
		m_pDocument->SetModifiedFlag();
		
		m_pShoreline = NULL;

		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CToolOverlay::InitOverlay( CMapEntity *pEntity, CMapFace *pFace )
{
	// Valid face?
	if ( !pFace )
		return;

	const CMapObjectList *pChildren  = pEntity->GetChildren();

	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos) );
		if ( pOverlay )
		{
			pOverlay->Basis_Init( pFace );
			pOverlay->Handles_Init( pFace );
			pOverlay->SideList_Init( pFace );
			pOverlay->SetOverlayType( OVERLAY_TYPE_GENERIC );
			pOverlay->SetLoaded( true );
			pOverlay->CalcBounds( true );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::OverlaySelection( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	CMapObjectList aSelectionList;
	m_pDocument->GetSelection()->ClearHitList();

	// Find out how many (and what) map objects are under the point clicked on.
	HitInfo_t Objects[MAX_PICK_HITS];
	int nHits = pView->ObjectsAt( vPoint, Objects, sizeof( Objects ) / sizeof( Objects[0] ) );
	if ( nHits != 0 )
	{
		// We now have an array of pointers to CMapAtoms. Any that can be upcast to CMapClass objects?
		for ( int iHit = 0; iHit < nHits; ++iHit )
		{
			CMapClass *pMapClass = dynamic_cast<CMapClass*>( Objects[iHit].pObject );
			if ( pMapClass )
			{
				aSelectionList.AddToTail( pMapClass );
			}
		}
	}

	// Did we hit anything?
	if ( !aSelectionList.Count() )
	{
		m_pDocument->SelectFace( NULL, 0, scClear );
		m_pDocument->SelectObject( NULL, scClear|scSaveChanges );
		SetEmpty();
		return;
	}

	// Find overlays.
	bool bUpdateViews = false;
	
	SelectMode_t eSelectMode = m_pDocument->GetSelection()->GetMode();
	
	FOR_EACH_OBJ( aSelectionList, pos )
	{
		CMapClass *pObject = aSelectionList.Element( pos );
		CMapClass *pHitObject = pObject->PrepareSelection( eSelectMode );
		if ( pHitObject )
		{
			if ( pHitObject->IsMapClass( MAPCLASS_TYPE( CMapEntity ) ) ) 
			{
				const CMapObjectList *pChildren = pHitObject->GetChildren();
				FOR_EACH_OBJ( *pChildren, pos2 )
				{
					CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos2) );
					if ( pOverlay )
					{
						m_pDocument->GetSelection()->AddHit( pHitObject );
						m_bEmpty = false;

						UINT cmd = scClear | scSelect | scSaveChanges;
						if (nFlags & MK_CONTROL)
						{
							cmd = scToggle;
						}
						m_pDocument->SelectObject( pHitObject, cmd );
						bUpdateViews = true;
					}
				}
			}
		}
	}

	// Update the views.
	if ( bUpdateViews )
	{
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );		
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::OnContextMenu2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint )
{
	static CMenu menu, menuOverlay;
	static bool bInit = false;

	if ( !bInit )
	{
		// Create the menu.
		menu.LoadMenu( IDR_POPUPS );
		menuOverlay.Attach( ::GetSubMenu( menu.m_hMenu,  6 ) );
		bInit = true;
	}

	if ( !pView->PointInClientRect(vPoint) )
		return false;

	if (!IsEmpty())
	{
		if ( HitTest( pView, vPoint, false ) )
		{
			CPoint ptScreen( vPoint.x,vPoint.y);
			pView->ClientToScreen(&ptScreen);
			menuOverlay.TrackPopupMenu( TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, pView );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::UpdateTranslation( const Vector &vUpdate, UINT nFlags)
{
//	if( m_bBoxSelecting )
//		return Box3D::UpdateTranslation( pt, nFlags );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::HandlesReset( void )
{
	// Go through selection list and reset overlay handles.
	const CMapObjectList *pSelection = m_pDocument->GetSelection()->GetList();
	for( int iSelection = 0; iSelection < pSelection->Count(); ++iSelection )
	{
		CMapClass *pMapClass = pSelection->Element( iSelection );
		if ( pMapClass && pMapClass->IsMapClass( MAPCLASS_TYPE( CMapEntity ) ) )
		{	
			const CMapObjectList *pChildren = pMapClass->GetChildren();
			FOR_EACH_OBJ( *pChildren, pos )
			{
				CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos) );
				if ( pOverlay )
				{
					pOverlay->HandlesReset();
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::HandleSelection( CMapView *pView, const Vector2D &vPoint )
{
	// Reset the hit overlay.
	m_pActiveOverlay = NULL;

	// Go through selection list and test all overlay's handles and set the
	// "hit" overlay current.
	const CMapObjectList *pSelection = m_pDocument->GetSelection()->GetList();
	for ( int iSelection = 0; iSelection < pSelection->Count(); ++iSelection )
	{
		CMapClass *pMapClass = pSelection->Element( iSelection );
		if ( pMapClass && pMapClass->IsMapClass( MAPCLASS_TYPE( CMapEntity ) ) )
		{
			const CMapObjectList *pChildren = pMapClass->GetChildren();
			FOR_EACH_OBJ( *pChildren, pos )
			{
				CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos) );
				if ( pOverlay && pOverlay->IsSelected() )
				{
					if ( pOverlay->HandlesHitTest( pView, vPoint ) )
					{
						m_pActiveOverlay = pOverlay;
						break;
					}
				}
			}
		}
	}

	if ( !m_pActiveOverlay )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::HandleSnap( CMapOverlay *pOverlay, Vector &vecHandlePt )
{
	Vector vecTmp;
	for ( int i = 0; i < OVERLAY_HANDLES_COUNT; i++ )
	{
		pOverlay->GetHandlePos( i, vecTmp );
		vecTmp -= vecHandlePt;
		float flDist = vecTmp.Length();

		if ( flDist < OVERLAY_TOOL_SNAP_DISTANCE )
		{
			// Snap!
			pOverlay->GetHandlePos( i, vecHandlePt );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CToolOverlay::HandleInBBox( CMapOverlay *pOverlay, Vector const &vecHandlePt )
{
	Vector vecMin, vecMax;
	pOverlay->GetCullBox( vecMin, vecMax );

	for ( int iAxis = 0; iAxis < 3; iAxis++ )
	{
		vecMin[iAxis] -= OVERLAY_TOOL_SNAP_DISTANCE;
		vecMax[iAxis] += OVERLAY_TOOL_SNAP_DISTANCE;

		if( ( vecHandlePt[iAxis] < vecMin[iAxis] ) || ( vecHandlePt[iAxis] > vecMax[iAxis] ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::SnapHandle( Vector &vecHandlePt )
{
	CMapWorld *pWorld = GetActiveWorld();
	if ( !pWorld )
		return;

	EnumChildrenPos_t posWorld;
	CMapClass *pChild = pWorld->GetFirstDescendent( posWorld );
	while ( pChild )
	{
		CMapEntity *pEntity = dynamic_cast<CMapEntity*>( pChild );
		if ( pEntity )
		{
			const CMapObjectList *pChildren = pEntity->GetChildren();
			FOR_EACH_OBJ( *pChildren, pos )
			{
				CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pChildren->Element(pos) );
				if ( pOverlay && pOverlay != m_pActiveOverlay && pOverlay->IsSelected() )
				{
					// Intersection test and attempt to snap
					if ( HandleInBBox( pOverlay, vecHandlePt ) )
					{
						if ( HandleSnap( pOverlay, vecHandlePt ) )
							return;
					}
				}
			}
		}

		pChild = pWorld->GetNextDescendent( posWorld );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::OnDrag( Vector const &vecRayStart, Vector const &vecRayEnd, bool bShift )
{
	// Get the current overlay.
	CMapOverlay *pOverlay = m_pActiveOverlay;
	if ( !pOverlay )
		return;

	// Get a list of faces and test for "impact."
	Vector vecImpact( 0.0f, 0.0f, 0.0f );
	Vector vecImpactNormal( 0.0f, 0.0f, 0.0f );
	CMapFace *pFace = NULL;
		
	int nFaceCount = pOverlay->GetFaceCount();
	int iFace;
	for ( iFace = 0; iFace < nFaceCount; iFace++ )
	{
		pFace = pOverlay->GetFace( iFace );
		if ( pFace )
		{
			if ( pFace->TraceLineInside( vecImpact, vecImpactNormal, vecRayStart, vecRayEnd ) )
				break;
		}
	}

	// Test for impact (face index = count mean no impact).
	if ( iFace == nFaceCount )
		return;

	if ( bShift )
	{
		SnapHandle( vecImpact );
	}
	
	// Pass the new handle position to the overlay.
	pOverlay->HandlesDragTo( vecImpact, pFace );

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::PreDrag( void )
{
	m_bDragging = true;
	SetupHandleDragUndo();
}


//-----------------------------------------------------------------------------
// Purpose: Renders the cordon tool in the 3D view.
//-----------------------------------------------------------------------------
void CToolOverlay::RenderTool3D(CRender3D *pRender)
{
	// TODO render tool handles here and not in overlay rendering code
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::PostDrag( void )
{
	if ( !m_bDragging )
		return;

	m_bDragging = false;

	// Get the current overlay.
	CMapOverlay *pOverlay = m_pActiveOverlay;
	if ( pOverlay )
	{
		pOverlay->DoClip();
		pOverlay->CenterEntity();
		pOverlay->PostUpdate( Notify_Changed );
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );	
	}

	// Reset the overlay handles.
	HandlesReset();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CToolOverlay::SetupHandleDragUndo( void )
{
	// Get the current overlay.
	CMapOverlay *pOverlay = m_pActiveOverlay;
	if ( pOverlay )
	{
		CMapEntity *pEntity = ( CMapEntity* )pOverlay->GetParent();
		if ( pEntity )
		{
			// Setup for drag undo.
			GetHistory()->MarkUndoPosition( m_pDocument->GetSelection()->GetList(), "Drag Overlay Handle" );
			GetHistory()->Keep( ( CMapClass* )pEntity );				
		}
	}
}
