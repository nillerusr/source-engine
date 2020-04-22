//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "resource.h"
#include "ToolMagnify.h"
#include "HammerVGui.h"
#include <VGuiMatSurface/IMatSystemSurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Loads the cursor (only once).
//-----------------------------------------------------------------------------
CToolMagnify::CToolMagnify(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			nFlags - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolMagnify::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	// Return true to suppress the default view context menu behavior.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true to indicate that the message was handled.
//-----------------------------------------------------------------------------
bool CToolMagnify::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	pView->SetZoom(pView->GetZoom() * 2);
	pView->Invalidate();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true to indicate that the message was handled.
//-----------------------------------------------------------------------------
bool CToolMagnify::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	// cursors are cached by surface
	pView->SetCursor( "Resource/magnify.cur" );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true to indicate that the message was handled.
//-----------------------------------------------------------------------------
bool CToolMagnify::OnRMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	pView->SetZoom(pView->GetZoom() * 0.5f);
	return true;
}


