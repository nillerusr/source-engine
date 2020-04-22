//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"			// For ObjectProperties
#include "MapDoc.h"
#include "MapAxisHandle.h"
#include "MapPointHandle.h"
#include "MapView2D.h"
#include "Render2D.h"
#include "StatusBarIDs.h"		// For SetStatusText
#include "ToolManager.h"
#include "ToolAxisHandle.h"
#include "ToolPointHandle.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CToolAxisHandle::CToolAxisHandle(void)
{
	m_pAxis = NULL;
	m_nPointIndex = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the point to the tool for manipulation.
//-----------------------------------------------------------------------------
void CToolAxisHandle::Attach(CMapAxisHandle *pAxis, int nPointIndex)
{
	if ((pAxis != NULL) && (nPointIndex < 2))
	{
		m_pAxis = pAxis;
		m_nPointIndex = nPointIndex;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolAxisHandle::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	//
	// Activate this tool and start dragging the axis endpoint.
	//
	ToolManager()->PushTool(TOOL_AXIS_HANDLE);
	pView->SetCapture();

	CMapDoc *pDoc = pView->GetMapDoc();
	GetHistory()->MarkUndoPosition(pDoc->GetSelection()->GetList(), "Modify Axis");
	GetHistory()->Keep(m_pAxis);

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolAxisHandle::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	// dvsFIXME: do we need to update the point here?

	ToolManager()->PopTool();
	ReleaseCapture();

	CMapDoc *pDoc = pView->GetMapDoc();
	pDoc->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolAxisHandle::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	//
	// Make sure the point is visible.
	//
	pView->ToolScrollToPoint( vPoint);

	//
	// Snap the point to half the grid size. Do this so that we can always center
	// the axis even on odd-width objects.
	//
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	CMapDoc *pDoc = pView->GetMapDoc();
	pDoc->Snap(vecWorld, constrainHalfSnap);

	//
	// Move to the snapped position.
	//
	Vector vecPos[2];
	m_pAxis->GetEndPoint(vecPos[m_nPointIndex], m_nPointIndex);

	vecPos[m_nPointIndex][pView->axHorz] = vecWorld[pView->axHorz];
	vecPos[m_nPointIndex][pView->axVert] = vecWorld[pView->axVert];
	
	m_pAxis->UpdateEndPoint(vecPos[m_nPointIndex], m_nPointIndex);

	int nOtherIndex = (m_nPointIndex == 0);
	m_pAxis->GetEndPoint(vecPos[nOtherIndex], nOtherIndex);

	//
	// Update the status bar and the views.
	//
	char szBuf[128];
	sprintf(szBuf, " (%.0f %.0f %0.f) ", vecPos[m_nPointIndex][0], vecPos[m_nPointIndex][1], vecPos[m_nPointIndex][2]);
	SetStatusText(SBI_COORDS, szBuf);

	pDoc->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Renders the tool in the 2D view.
// Input  : pRender - The interface to use for rendering.
//-----------------------------------------------------------------------------
void CToolAxisHandle::RenderTool2D(CRender2D *pRender)
{
	SelectionState_t eState = m_pAxis->SetSelectionState(SELECT_MODIFY, m_nPointIndex);
	m_pAxis->Render2D(pRender);
	m_pAxis->SetSelectionState(eState);
}


