//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the cordon tool. The cordon tool defines a rectangular
//			volume that acts as a visibility filter. Only objects that intersect
//			the cordon are rendered in the views. When saving the MAP file while
//			the cordon tool is active, only brushes that intersect the cordon
//			bounds are saved. The cordon box is replaced by brushes in order to
//			seal the map.
//
//=============================================================================//

#include "stdafx.h"
#include "ChunkFile.h"
#include "ToolCordon.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "MapDefs.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "MapWorld.h"
#include "StatusBarIDs.h"
#include "ToolManager.h"
#include "Options.h"
#include "WorldSize.h"
#include "vgui/Cursor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
Cordon3D::Cordon3D(void)
{
	SetDrawColors(RGB(0, 255, 255), RGB(255, 0, 0));
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is activated.
// Input  : eOldTool - The ID of the previously active tool.
//-----------------------------------------------------------------------------
void Cordon3D::OnActivate()
{
	if (!IsActiveTool())
	{
		Vector mins,maxs;
		m_pDocument->GetCordon( mins, maxs );
		SetBounds( mins,maxs );

		m_bEmpty = !IsValidBox();
		EnableHandles( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Cordon3D::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	unsigned int uConstraints = GetConstraints( nFlags );

	if ( HitTest(pView, vPoint, true) )
	{
		StartTranslation( pView, vPoint, m_LastHitTestHandle );
	}
	else
	{
		// getvisiblepoint fills in any coord that's still set to COORD_NOTINIT:
		vecWorld[pView->axThird] = COORD_NOTINIT;
		m_pDocument->GetBestVisiblePoint(vecWorld);

		// snap starting position to grid
		if ( uConstraints & constrainSnap )
			m_pDocument->Snap(vecWorld,uConstraints);
		
		StartNew( pView, vPoint, vecWorld, Vector(0,0,0) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Cordon3D::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint) ;

	if ( IsTranslating() )
	{
		FinishTranslation( true );
		m_pDocument->SetCordon( bmins, bmaxs );
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Cordon3D::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	vgui::HCursor hCursor = vgui::dc_arrow;

	Tool3D::OnMouseMove2D(pView, nFlags, vPoint) ;

	unsigned int uConstraints = GetConstraints( nFlags );
					    
	// Convert to world coords.
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);
	
	// Update status bar position display.
	//
	char szBuf[128];

	m_pDocument->Snap(vecWorld,uConstraints);

	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert]);
	SetStatusText(SBI_COORDS, szBuf);
	
	if ( IsTranslating() )
	{
		// cursor is cross here
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );
		
		hCursor = vgui::dc_none;
	}
	else if ( HitTest(pView, vPoint, true) )
	{
		hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
	}
	
	if ( hCursor != vgui::dc_none  )
		pView->SetCursor( hCursor );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Cordon3D::OnEscape(void)
{
	if ( IsTranslating() )
	{
		FinishTranslation( false );
	}
	else
	{
		m_pDocument->GetTools()->SetTool(TOOL_POINTER);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output :
//-----------------------------------------------------------------------------
bool Cordon3D::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_ESCAPE)
	{
		OnEscape();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output :
//-----------------------------------------------------------------------------
bool Cordon3D::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_ESCAPE)
	{
		OnEscape();
		return true;
	}

	return false;
}

