//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Default implementation for interface common to 2D and 3D views.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapDoc.h"
#include "MapView.h"
#include "History.h"
#include "mapsolid.h"
#include "camera.h"

#define MMNOMIXER
#include <mmsystem.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244 4305)


//-----------------------------------------------------------------------------
// Purpose: Constructor, sets to inactive.
//-----------------------------------------------------------------------------
CMapView::CMapView(void)
{
	m_bActive = false;
	m_bUpdateView = false;
	m_eDrawType = VIEW_INVALID;
	m_pCamera = NULL;
	m_dwTimeLastRender = 0;
	m_nRenderedFrames = 0;
	m_pToolManager = NULL;
}

bool CMapView::IsOrthographic()
{
	return m_pCamera->IsOrthographic();
}

void CMapView::UpdateView( int nFlags )
{
	m_bUpdateView = true;

	if ( nFlags & MAPVIEW_RENDER_NOW )
	{
		RenderView();
	}
}



void CMapView::ActivateView(bool bActivate)
{
	if ( bActivate != m_bActive )
	{
		m_bActive = bActivate;
		m_bUpdateView = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Recalculates framerate since the last call to this function and
//			determines whether or not we are exceeding the framerate limit.
// Output : Returns true if we are beyond the framerate limit, false if not.
//-----------------------------------------------------------------------------
bool CMapView::ShouldRender()
{
	DWORD dwTimeNow = timeGetTime();

	if (m_dwTimeLastRender != 0)
	{
		DWORD dwTimeElapsed = dwTimeNow - m_dwTimeLastRender;
	
		if ( dwTimeElapsed <= 0 )
			return false;
	
		float flFrameRate = (1000.0f / dwTimeElapsed);

		if (flFrameRate > 100.0f)
		{
			// never update view faster then 100Hz
			return false;
		}
	}

	// update view if needed
	if ( m_bUpdateView )
	{
		m_dwTimeLastRender = dwTimeNow;
		m_nRenderedFrames++;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : point - Point in client coordinates.
//			bMakeFirst - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapView::SelectAt(const Vector2D &ptClient, bool bMakeFirst, bool bFace)
{
	CMapDoc *pDoc = GetMapDoc();
	CSelection *pSelection = pDoc->GetSelection();
	const CMapObjectList *pSelList = pSelection->GetList();

	pSelection->ClearHitList();

	GetHistory()->MarkUndoPosition(pSelList, "Selection");

	//
	// Check all the objects in the world for a hit at this point.
	//

	HitInfo_t HitData[MAX_PICK_HITS];
	int nHits = ObjectsAt(ptClient, HitData, MAX_PICK_HITS);
	
	//
	// If there were no hits at the given point, clear selection.
	//
	if ( nHits == 0 )
	{
		if (bMakeFirst)
		{
			pDoc->SelectFace(NULL, 0, scClear|scSaveChanges );
			pDoc->SelectObject(NULL, scClear|scSaveChanges );
		}

		return false;
	}

	SelectMode_t eSelectMode = pSelection->GetMode();

	for ( int i=0; i<nHits; i++ )
	{
		if ( HitData[i].pObject )
		{
			CMapClass *pSelObject = HitData[i].pObject->PrepareSelection( eSelectMode );
			if (pSelObject)
			{
				pSelection->AddHit(pSelObject);
			}
		}
	}

	//
	// If we're selecting faces, mark all faces on the first solid we hit.
	//
	if ( bFace )
	{
		const CMapObjectList *pList = pSelection->GetHitList();
		FOR_EACH_OBJ( *pList, pos )
		{
			CMapClass *pObject = pList->Element(pos);
			if (pObject->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
			{
				pDoc->SelectFace((CMapSolid*)pObject, -1, scSelect | (bMakeFirst ? scClear : 0));
				break;
			}
		}

		return true;
	}

	//
	// Select a single object.
	//
	if ( bMakeFirst )
	{
		pDoc->SelectFace(NULL, 0, scClear|scSaveChanges);
		pDoc->SelectObject(NULL, scClear|scSaveChanges);
	}

	pSelection->SetCurrentHit(hitFirst);

	return true;
}

void CMapView::BuildRay( const Vector2D &vView, Vector& vStart, Vector& vEnd )
{
	m_pCamera->BuildRay( vView, vStart, vEnd );
}

const Vector &CMapView::GetViewAxis()
{
	static Vector vForward;
	m_pCamera->GetViewForward(vForward);
	return vForward;
}


