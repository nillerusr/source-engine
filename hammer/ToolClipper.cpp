//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapWorld.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"
#include "RenderUtils.h"
#include "StatusBarIDs.h"		// dvs: remove
#include "ToolClipper.h"
#include "ToolManager.h"
#include "vgui/Cursor.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning( disable:4244 )


//=============================================================================
//
// Friend Function (for MapClass->EnumChildren Callback)
//

//-----------------------------------------------------------------------------
// Purpose: This function creates a new clip group with the given solid as 
//          the original solid.
//   Input: pSolid - the original solid to put in the clip list
//          pClipper - the clipper tool
//  Output: successful?? (true/false)
//-----------------------------------------------------------------------------
BOOL AddToClipList( CMapSolid *pSolid, Clipper3D *pClipper )
{
    CClipGroup *pClipGroup = new CClipGroup;
    if( !pClipGroup )
        return false;

    pClipGroup->SetOrigSolid( pSolid );
    pClipper->m_ClipResults.AddToTail( pClipGroup );

    return true;
}


//=============================================================================
//
// CClipGroup
//

//-----------------------------------------------------------------------------
// Purpose: Destructor. Gets rid of the unnecessary clip solids.
//-----------------------------------------------------------------------------
CClipGroup::~CClipGroup()
{
	delete m_pClipSolids[0];
	delete m_pClipSolids[1];
}


//-----------------------------------------------------------------------------
// Purpose: constructor - initialize the clipper variables
//-----------------------------------------------------------------------------
Clipper3D::Clipper3D(void)
{
	m_Mode = FRONT;

    m_ClipPlane.normal.Init();
    m_ClipPlane.dist = 0.0f;
    m_ClipPoints[0].Init();
    m_ClipPoints[1].Init();
    m_ClipPointHit = -1;

    m_pOrigObjects = NULL;

	m_bDrawMeasurements = false;
	SetEmpty();
}


//-----------------------------------------------------------------------------
// Purpose: deconstructor
//-----------------------------------------------------------------------------
Clipper3D::~Clipper3D(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is activated.
// Input  : eOldTool - The ID of the previously active tool.
//-----------------------------------------------------------------------------
void Clipper3D::OnActivate()
{
	if (IsActiveTool())
	{
		//
		// Already the active tool - toggle the mode.
		//
        IterateClipMode();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void Clipper3D::OnDeactivate()
{
	SetEmpty();
}


//-----------------------------------------------------------------------------
// Purpose: (virtual imp) This function handles the "dragging" of the mouse
//          while the left mouse button is depressed.  It updates the position
//          of the clippoing plane point selected in the StartTranslation
//          function.  This function rebuilds the clipping plane and updates
//          the clipping solids when necessary.
//   Input: pt - current location of the mouse in the 2DView
//          uFlags - constrained clipping plane point movement
//          *dragSize - not used in the virtual implementation
//  Output: success of translation (TRUE/FALSE)
//-----------------------------------------------------------------------------
bool Clipper3D::UpdateTranslation( const Vector &vUpdate, UINT uFlags )
{
    // sanity check
    if( IsEmpty() )
        return false;

	Vector vNewPos = m_vOrgPos + vUpdate;

    // snap point if need be
	if ( uFlags & constrainSnap )
		m_pDocument->Snap( vNewPos, uFlags );

    //
    // update clipping point positions
    //
	if ( m_ClipPoints[m_ClipPointHit] == vNewPos )
		return false;

    
    if( uFlags & constrainMoveAll )
    {
		//
		// calculate the point and delta - to move both clip points simultaneously
		//
		
		Vector delta = vNewPos - m_ClipPoints[m_ClipPointHit];
		m_ClipPoints[(m_ClipPointHit+1)%2] += delta;
    }

	m_ClipPoints[m_ClipPointHit] = vNewPos;

    // build the new clip plane and update clip results
    BuildClipPlane();

    GetClipResults();

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

    return true;
}


//-----------------------------------------------------------------------------
// Purpose: (virtual imp) This function defines all finishing functionality 
//          necessary at the end of a clipping action.  Nothing really!!!
// Input  : bSave - passed along the the Tool finish translation call
//-----------------------------------------------------------------------------
void Clipper3D::FinishTranslation( bool bSave )
{
    // get the clip results -- in case the update is a click and not a drag
    GetClipResults();

    Tool3D::FinishTranslation( bSave );
}


//-----------------------------------------------------------------------------
// Purpose: iterate through the types of clipping modes, update after an
//          iteration takes place to visualize the new clip results
//-----------------------------------------------------------------------------
void Clipper3D::IterateClipMode( void )
{
    //
    // increment the clipping mode (wrap when necessary)
    //
    m_Mode++;

    if( m_Mode > BOTH )
    {
        m_Mode = FRONT;
    }

    // update the clipped objects based on the mode
    GetClipResults();
}


//-----------------------------------------------------------------------------
// Purpose: This resets the solids to clip (the original list) and calls the
//          CalcClipResults function to generate new "clip" solids
//-----------------------------------------------------------------------------
void Clipper3D::GetClipResults( void )
{
    // reset the clip list to the original solid lsit
    SetClipObjects( m_pOrigObjects );

    // calculate the clipped objects based on the current "clip plane"
    CalcClipResults();
}


//-----------------------------------------------------------------------------
// Purpose: This function allows one to specifically set the clipping plane
//          information, as opposed to building a clip plane during "translation"
//   Input: pPlane - the plane information used to create the clip plane
//-----------------------------------------------------------------------------
void Clipper3D::SetClipPlane( PLANE *pPlane )
{
    //
    // copy the clipping plane info
    //
    m_ClipPlane.normal = pPlane->normal;
    m_ClipPlane.dist = pPlane->dist;
}


//-----------------------------------------------------------------------------
// Purpose: This function builds a clipping plane based on the clip point
//          locations manipulated in the "translation" functions and the 2DView
//-----------------------------------------------------------------------------
void Clipper3D::BuildClipPlane( void )
{
	// calculate the up vector
    Vector upVect = m_vPlaneNormal;
    
	// calculate the right vector
    Vector rightVect;
    VectorSubtract( m_ClipPoints[1], m_ClipPoints[0], rightVect );
    
    // calculate the forward (normal) vector
    Vector forwardVect;
    CrossProduct( upVect, rightVect, forwardVect );
    VectorNormalize( forwardVect );
    
    //
    // save the clip plane info
    //
    m_ClipPlane.normal = forwardVect;
    m_ClipPlane.dist = DotProduct( m_ClipPoints[0], forwardVect );
}


//-----------------------------------------------------------------------------
// Purpose: This functions sets up the list of objects to be clipped.  
//          Initially the list is passed in (typically a Selection set).  On 
//          subsequent "translation" updates the list is refreshed from the 
//          m_pOrigObjects list.
//   Input: pList - the list of objects (solids) to be clipped
//-----------------------------------------------------------------------------
void Clipper3D::SetClipObjects( const CMapObjectList *pList )
{
    // check for an empty list
    if( !pList )
        return;

    // save the original list
    m_pOrigObjects = pList;

    // clear the clip results list
    ResetClipResults();

    //
    // copy solids into the clip list
    //
    FOR_EACH_OBJ( *m_pOrigObjects, pos )
    {
        CMapClass *pObject = m_pOrigObjects->Element( pos );
        if( !pObject )
            continue;

        if( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
        {
            AddToClipList( ( CMapSolid* )pObject, this );
        }

        pObject->EnumChildren( ENUMMAPCHILDRENPROC( AddToClipList ), DWORD( this ), MAPCLASS_TYPE( CMapSolid ) );
    }

    // the clipping list is not empty anymore
    m_bEmpty = false;
}


//-----------------------------------------------------------------------------
// Purpose: This function calculates based on the defined or given clipping
//          plane and clipping mode the new clip solids.
//-----------------------------------------------------------------------------
void Clipper3D::CalcClipResults( void )
{
    // sanity check
    if( IsEmpty() )
        return;

    //
    // iterate through and clip all of the solids in the clip list
    //
    FOR_EACH_OBJ( m_ClipResults, pos )
    {
        CClipGroup *pClipGroup = m_ClipResults.Element( pos );
        CMapSolid *pOrigSolid = pClipGroup->GetOrigSolid();
        if( !pOrigSolid )
            continue;

        //
        // check the modes for which solids to generate
        //
        CMapSolid *pFront = NULL;
        CMapSolid *pBack = NULL;
        if( m_Mode == FRONT )
        {
            pOrigSolid->Split( &m_ClipPlane, &pFront, NULL );
        }
        else if( m_Mode == BACK )
        {
            pOrigSolid->Split( &m_ClipPlane, NULL, &pBack );
        }
        else if( m_Mode == BOTH )
        {
            pOrigSolid->Split( &m_ClipPlane, &pFront, &pBack );
        }

        if( pFront )
        {
			pFront->SetTemporary(true);
            pClipGroup->SetClipSolid( pFront, FRONT );
        }

        if( pBack )
        {
			pBack->SetTemporary(true);
            pClipGroup->SetClipSolid( pBack, BACK );
        }
    }
}


//-----------------------------------------------------------------------------
// Purpose: This function handles the removal of the "original" solid when it
//          has been clipped into new solid(s) or removed from the world (group
//          or entity) entirely.  It handles this in an undo safe fashion.
//   Input: pOrigSolid - the solid to remove
//-----------------------------------------------------------------------------
void Clipper3D::RemoveOrigSolid( CMapSolid *pOrigSolid )
{
	m_pDocument->DeleteObject(pOrigSolid);

    //
    // remove the solid from the selection set if in the seleciton set and
    // its parent is the world, or set the selection state to none parent is group
    // or entity in the selection set
    //    

	CSelection *pSelection = m_pDocument->GetSelection();

    if ( pSelection->IsSelected( pOrigSolid ) )
    {
        pSelection->SelectObject( pOrigSolid, scUnselect );
    }
    else
    {
        pOrigSolid->SetSelectionState( SELECT_NONE );
    }
}


//-----------------------------------------------------------------------------
// Purpose: This function handles the saving of newly clipped solids (derived
//          from an "original" solid).  It handles them in an undo safe fashion.
//   Input: pSolid - the newly clipped solid
//          pOrigSolid - the "original" solid or solid the clipped solid was
//                       derived from
//-----------------------------------------------------------------------------
void Clipper3D::SaveClipSolid( CMapSolid *pSolid, CMapSolid *pOrigSolid )
{
    //
    // no longer a temporary solid
    //
    pSolid->SetTemporary( FALSE );

    //
    // Add the new solid to the original solid's parent (group, entity, world, etc.).
    //
	m_pDocument->AddObjectToWorld(pSolid, pOrigSolid->GetParent());
    
    //
    // handle linking solid into selection -- via selection set when parent is the world
    // and selected, or set the selection state if parent is group or entity in selection set
    //
    if( m_pDocument->GetSelection()->IsSelected( pOrigSolid ) )
    {
        m_pDocument->SelectObject( pSolid, scSelect );
    }
    else
    {
        pSolid->SetSelectionState( SELECT_NORMAL );
    }

    GetHistory()->KeepNew( pSolid );
}


//-----------------------------------------------------------------------------
// Purpose: This function saves all the clipped solid information.  If new solids
//          were generated from the original, they are saved and the original is
//          set for desctruciton.  Otherwise, the original solid is kept.
//-----------------------------------------------------------------------------
void Clipper3D::SaveClipResults( void )
{
    // sanity check!
    if( IsEmpty() )
        return;

	// mark this place in the history
    GetHistory()->MarkUndoPosition( NULL, "Clip Objects" );

    //
    // save all new objects into the selection list
    //
    FOR_EACH_OBJ( m_ClipResults, pos )
    {
        CClipGroup *pClipGroup = m_ClipResults.Element( pos );
        if( !pClipGroup )
            continue;

        CMapSolid *pOrigSolid = pClipGroup->GetOrigSolid();
        CMapSolid *pBackSolid = pClipGroup->GetClipSolid( CClipGroup::BACK );
        CMapSolid *pFrontSolid = pClipGroup->GetClipSolid( CClipGroup::FRONT );

        //
        // save the front clip solid and clear the clip results list of itself
        //
        if( pFrontSolid )
        {
            SaveClipSolid( pFrontSolid, pOrigSolid );
            pClipGroup->SetClipSolid( NULL, CClipGroup::FRONT );
        }

        //
        // save the front clip solid and clear the clip results list of itself
        //
        if( pBackSolid )
        {
            SaveClipSolid( pBackSolid, pOrigSolid );
            pClipGroup->SetClipSolid( NULL, CClipGroup::BACK );
        }
     
		// Send the notification that this solid as been clipped.
		pOrigSolid->PostUpdate( Notify_Clipped );

        // remove the original solid
        RemoveOrigSolid( pOrigSolid );
    }

    // set the the clipping results list as empty
    ResetClipResults();

	// update world and views
	
    m_pDocument->SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Draws the measurements of a brush in the 2D view.
// Input  : pRender - 
//			pSolid - 
//			nFlags - 
//-----------------------------------------------------------------------------
void Clipper3D::DrawBrushExtents( CRender2D *pRender, CMapSolid *pSolid, int nFlags )
{
    //
    // get the bounds of the solid
    //
	Vector Mins, Maxs;
	pSolid->GetRender2DBox( Mins, Maxs );

	//
	// Determine which side of the clipping plane this solid is on in screen
	// space. This tells us where to draw the extents.
	//
    if( ( m_ClipPlane.normal[0] == 0 ) && ( m_ClipPlane.normal[1] == 0 ) && ( m_ClipPlane.normal[2] == 0 ) )
        return;

    Vector normal = m_ClipPlane.normal;

    if( nFlags & DBT_BACK )
    {
       VectorNegate( normal );
    }

	Vector2D planeNormal;

    pRender->TransformNormal( planeNormal, normal );

	if( planeNormal.x <= 0 )
    {
        nFlags &= ~DBT_RIGHT;
        nFlags |= DBT_LEFT;
    }
    else if( planeNormal.x > 0 )
    {
        nFlags &= ~DBT_LEFT;
        nFlags |= DBT_RIGHT;
    }

    if( planeNormal.y <= 0 )
    {
        nFlags &= ~DBT_BOTTOM;
        nFlags |= DBT_TOP;
    }
    else if( planeNormal.y > 0 )
    {
        nFlags &= ~DBT_TOP;
        nFlags |= DBT_BOTTOM;
    }

	DrawBoundsText(pRender, Mins, Maxs, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender - 
//-----------------------------------------------------------------------------
void Clipper3D::RenderTool2D(CRender2D *pRender)
{
	if ( IsEmpty() )
		return;

    // check flag for rendering vertices
    bool bDrawVerts = ( bool )( Options.view2d.bDrawVertices == TRUE );

    // setup the line to use

	pRender->SetDrawColor( 255, 255, 255 );

    //
    // render the clipped solids
    //
    FOR_EACH_OBJ( m_ClipResults, pos )
    {
        CClipGroup *pClipGroup = m_ClipResults.Element( pos );
        CMapSolid *pClipBack = pClipGroup->GetClipSolid( CClipGroup::BACK );
        CMapSolid *pClipFront = pClipGroup->GetClipSolid( CClipGroup::FRONT );
        if( !pClipBack && !pClipFront )
            continue;

        //
        // draw clip solids with the extents
        //
        if( pClipBack )
        {
            int faceCount = pClipBack->GetFaceCount();
            for( int i = 0; i < faceCount; i++ )
            {
                CMapFace *pFace = pClipBack->GetFace( i );

				// size 4
                pRender->DrawPolyLine( pFace->nPoints, pFace->Points );

				if ( bDrawVerts )
				{
					pRender->DrawHandles( pFace->nPoints, pFace->Points );
				}

                if( m_bDrawMeasurements )
                {
                    DrawBrushExtents( pRender, pClipBack, DBT_TOP | DBT_LEFT | DBT_BACK );
                }
            }
        }

        if( pClipFront )
        {
            int faceCount = pClipFront->GetFaceCount();
            for( int i = 0; i < faceCount; i++ )
            {
                CMapFace *pFace = pClipFront->GetFace( i );

                pRender->DrawPolyLine( pFace->nPoints, pFace->Points );

				if ( bDrawVerts )
				{
					pRender->DrawHandles( pFace->nPoints, pFace->Points );
				}

                if( m_bDrawMeasurements )
                {
                    DrawBrushExtents( pRender, pClipFront, DBT_BOTTOM | DBT_RIGHT );
                }
            }
        }
	}

    //
	// draw the clip-plane
	//
	pRender->SetDrawColor( 0, 255, 255 );
	pRender->DrawLine( m_ClipPoints[0], m_ClipPoints[1] );

	//
	// draw the clip-plane endpoints
	//

	pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
	pRender->SetHandleColor( 255, 255, 255 );

    pRender->DrawHandle( m_ClipPoints[0] );
    pRender->DrawHandle( m_ClipPoints[1] );
}


//-----------------------------------------------------------------------------
// Purpose: Renders the brushes that will be left by the clipper in white
//			wireframe.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void Clipper3D::RenderTool3D( CRender3D *pRender )
{
    // is there anything to render?
    if( m_bEmpty )
        return;

    //
    // setup the renderer
    //
    pRender->PushRenderMode( RENDER_MODE_WIREFRAME );
    
    FOR_EACH_OBJ( m_ClipResults, pos )
    {
        CClipGroup *pClipGroup = m_ClipResults.Element( pos );

        CMapSolid *pFrontSolid = pClipGroup->GetClipSolid( CClipGroup::FRONT );
        if( pFrontSolid )
        {
			color32 rgbColor = pFrontSolid->GetRenderColor();
            pFrontSolid->SetRenderColor(255, 255, 255);
            pFrontSolid->Render3D(pRender);
            pFrontSolid->SetRenderColor(rgbColor);
        }

        CMapSolid *pBackSolid = pClipGroup->GetClipSolid( CClipGroup::BACK );
        if( pBackSolid )
        {
			color32 rgbColor = pBackSolid->GetRenderColor();
            pBackSolid->SetRenderColor(255, 255, 255);
            pBackSolid->Render3D(pRender);
            pBackSolid->SetRenderColor(rgbColor);
        }
    }

    pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: (virtual imp)
// Input  : pt - 
//			BOOL - 
// Output : int
//-----------------------------------------------------------------------------
int Clipper3D::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles)
{
    // check points
    
	for ( int i=0; i<2;i++ )
	{
		if ( HitRect(pView, ptClient, m_ClipPoints[i], HANDLE_RADIUS) )
		{
			return i+1; // return clip point index + 1
		}
	}
    
    // neither point hit
    return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Reset (clear) the clip results.
//-----------------------------------------------------------------------------
void Clipper3D::ResetClipResults( void )
{
    //
    // delete the clip solids held in the list -- originals are just pointers
    // to pre-existing objects
    //
    FOR_EACH_OBJ( m_ClipResults, pos )
    {
        CClipGroup *pClipGroup = m_ClipResults.Element(pos);

        if( pClipGroup )
        {
            delete pClipGroup;
        }
    }

	m_ClipResults.RemoveAll();

    // the clipping list is empty
    SetEmpty();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nChar - 
//			nRepCnt - 
//			nFlags - 
//-----------------------------------------------------------------------------
bool Clipper3D::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar)
	{
		case 'O':
		{
			//
			// Toggle the rendering of measurements.
			//
			ToggleMeasurements();
			return true;
		}

		case VK_RETURN:
		{
			//
			// Do the clip.
			//
			if (!IsEmpty() )
			{
				SaveClipResults();
			}
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;	
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	unsigned int uConstraints = GetConstraints( nFlags );

	//
	// Convert point to world coords.
	//
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);
	vecWorld[pView->axThird] = COORD_NOTINIT;

	// getvisiblepoint fills in any coord that's still set to COORD_NOTINIT:
	m_pDocument->GetBestVisiblePoint(vecWorld);

	// snap starting position to grid
	if ( uConstraints & constrainSnap )
		m_pDocument->Snap(vecWorld, uConstraints);
	
	
	bool bStarting = false;

	// if the tool is not empty, and shift is not held down (to
	//  start a new camera), don't do anything.
	if(!IsEmpty())
	{
		// test for clip point hit (result = {0, 1, 2}
		int hitPoint = HitTest( pView, vPoint );

		if ( hitPoint > 0 )
		{
			// test for clip point hit (result = {0, 1, -1})
			m_ClipPointHit = hitPoint-1; // convert back to index
			m_vOrgPos = m_ClipPoints[m_ClipPointHit];
			StartTranslation( pView, vPoint );
		}
		else if ( m_vPlaneNormal != pView->GetViewAxis() )
		{
			SetEmpty();
			bStarting = true;
		}
		else
		{
			if (nFlags & MK_SHIFT)
			{
				SetEmpty();
				bStarting = true;
			}
			else
			{
				return true; // do nothing;
			}
		}
	}
	else
	{
		bStarting = true;
	}

	SetClipObjects(m_pDocument->GetSelection()->GetList());

	if (bStarting)
	{
		// start the tools translation functionality
		StartTranslation( pView, vPoint );

		// set the initial clip points
		m_ClipPointHit = 0;
		m_ClipPoints[0] = vecWorld; 
		m_ClipPoints[1] = vecWorld;
		m_vOrgPos = vecWorld;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	if ( IsTranslating() )
	{
		FinishTranslation(true);
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}

unsigned int Clipper3D::GetConstraints(unsigned int nKeyFlags)
{
	unsigned int uConstraints = Tool3D::GetConstraints( nKeyFlags );

	if(nKeyFlags & MK_CONTROL)
	{
		uConstraints |= constrainMoveAll;
	}

	return uConstraints;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	vgui::HCursor hCursor = vgui::dc_arrow;
	unsigned int uConstraints = GetConstraints( nFlags );

	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);
					    
	//
	// Convert to world coords.
	//
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);
	
	//
	// Update status bar position display.
	//
	char szBuf[128];

	if ( uConstraints & constrainSnap )
		m_pDocument->Snap(vecWorld,uConstraints);

	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert]);
	SetStatusText(SBI_COORDS, szBuf);
	
	if (IsTranslating())
	{
		// cursor is cross here
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints);

		hCursor = vgui::dc_none;
	}
	else if (!IsEmpty())
	{
		//
		// If the cursor is on a handle, set it to a cross.
		//
		if (HitTest( pView, vPoint, true))
		{
			hCursor = vgui::dc_crosshair;
		}
	}

	if ( hCursor != vgui::dc_none  )
		pView->SetCursor( hCursor );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles character events.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch (nChar)
	{
		case VK_RETURN:
		{
			if (!IsEmpty()) // dvs: what does isempty mean for the clipper?
			{
				SaveClipResults();
			}
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Clipper3D::OnEscape(void)
{
	// If we're clipping, clear it
	if (!IsEmpty())
	{
		SetEmpty();
	}
	else
	{
		m_pDocument->GetTools()->SetTool(TOOL_POINTER);
	}
}

