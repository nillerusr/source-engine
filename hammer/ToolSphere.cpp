//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"			// FIXME: For ObjectProperties
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapSphere.h"
#include "StatusBarIDs.h"		// For updating status bar text
#include "ToolManager.h"
#include "ToolSphere.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CToolSphere::CToolSphere()
{
	m_pSphere = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSphere - 
//-----------------------------------------------------------------------------
void CToolSphere::Attach(CMapSphere *pSphere)
{
	m_pSphere = pSphere;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolSphere::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	//
	// Activate this tool and start resizing the sphere.
	//
	ToolManager()->PushTool(TOOL_SPHERE);
	pView->SetCapture();

	GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "Modify Radius");
	GetHistory()->Keep(m_pSphere);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolSphere::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	ToolManager()->PopTool();

	ReleaseCapture();

	m_pDocument->SetModifiedFlag();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolSphere::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	// Make sure the point is visible.
	pView->ToolScrollToPoint( vPoint );

	Vector	vecWorld;
	pView->ClientToWorld( vecWorld,  vPoint );
	m_pDocument->Snap( vecWorld, constrainSnap );

	//
	// Use whichever axis they dragged the most along as the drag axis.
	// Calculate the drag distance in client coordinates.
	//
	float flHorzRadius = fabs((float)vecWorld[pView->axHorz] - m_pSphere->m_Origin[pView->axHorz]);
	float flVertRadius = fabs((float)vecWorld[pView->axVert] - m_pSphere->m_Origin[pView->axVert]);
	float flRadius = max(flHorzRadius, flVertRadius);
	
	m_pSphere->SetRadius(flRadius);

	//
	// Update the status bar text with the new value of the radius.
	//
	char szBuf[128];
	sprintf(szBuf, " %s = %g ", m_pSphere->m_szKeyName, (double)m_pSphere->m_flRadius);
	SetStatusText(SBI_COORDS, szBuf);

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return true;
}

