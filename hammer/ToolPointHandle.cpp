//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"			// FIXME: For ObjectProperties
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapPointHandle.h"
#include "PopupMenus.h"
#include "Render2D.h"
#include "StatusBarIDs.h"		// For SetStatusText
#include "ToolManager.h"
#include "ToolPointHandle.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


class CToolPointHandleMsgWnd : public CWnd
{
	public:

		bool Create(void);
		void PreMenu2D(CToolPointHandle *pTool, CMapView2D *pView);

	protected:

		//{{AFX_MSG_MAP(CToolPointHandleMsgWnd)
		afx_msg void OnCenter();
		//}}AFX_MSG
	
		DECLARE_MESSAGE_MAP()

	private:

		CToolPointHandle *m_pToolPointHandle;
		CMapView2D *m_pView2D;
};


static CToolPointHandleMsgWnd s_wndToolMessage;
static const char *g_pszClassName = "ValveEditor_PointHandleToolWnd";


BEGIN_MESSAGE_MAP(CToolPointHandleMsgWnd, CWnd)
	//{{AFX_MSG_MAP(CToolPointHandleMsgWnd)
	ON_COMMAND(ID_CENTER_ON_ENTITY, OnCenter)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Creates the hidden window that receives context menu commands for the
//			block tool.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolPointHandleMsgWnd::Create(void)
{
	WNDCLASS wndcls;
	memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.lpfnWndProc   = AfxWndProc;
    wndcls.hInstance     = AfxGetInstanceHandle();
    wndcls.lpszClassName = g_pszClassName;

	if (!AfxRegisterClass(&wndcls))
	{
		return(false);
	}

	return(CWnd::CreateEx(0, g_pszClassName, g_pszClassName, 0, CRect(0, 0, 10, 10), NULL, 0) == TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the tool to this window before activating the context menu.
//-----------------------------------------------------------------------------
void CToolPointHandleMsgWnd::PreMenu2D(CToolPointHandle *pToolPointHandle, CMapView2D *pView)
{
	Assert(pToolPointHandle != NULL);
	m_pToolPointHandle = pToolPointHandle;
	m_pView2D = pView;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolPointHandleMsgWnd::OnCenter()
{
	if (m_pToolPointHandle)
	{
		m_pToolPointHandle->CenterOnParent(m_pView2D);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CToolPointHandle::CToolPointHandle(void)
{
	m_pPoint = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the point to the tool for manipulation.
//-----------------------------------------------------------------------------
void CToolPointHandle::Attach(CMapPointHandle *pPoint)
{
	m_pPoint = pPoint;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolPointHandle::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &VPoint)
{
	//
	// Activate this tool and start dragging the axis endpoint.
	//
	ToolManager()->PushTool(TOOL_POINT_HANDLE);
	pView->SetCapture();

	GetHistory()->MarkUndoPosition( m_pDocument->GetSelection()->GetList(), "Modify Origin");
	GetHistory()->Keep(m_pPoint);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolPointHandle::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &VPoint)
{
	m_pPoint->UpdateOrigin(m_pPoint->m_Origin);

	ToolManager()->PopTool();
	ReleaseCapture();

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolPointHandle::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	//
	// Make sure the point is visible.
	//
	pView->ToolScrollToPoint( vPoint );

	//
	// Snap the point to half the grid size. Do this so that we can always center
	// the origin even on odd-width objects.
	//
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	m_pDocument->Snap(vecWorld, constrainHalfSnap);

	//
	// Move to the snapped position.
	//
	m_pPoint->m_Origin[pView->axHorz] = vecWorld[pView->axHorz];
	m_pPoint->m_Origin[pView->axVert] = vecWorld[pView->axVert];
	
	//
	// Update the status bar and the views.
	//
	char szBuf[128];
	sprintf(szBuf, " @%.0f, %.0f ", m_pPoint->m_Origin[pView->axHorz], m_pPoint->m_Origin[pView->axVert]);
	SetStatusText(SBI_COORDS, szBuf);

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Renders the tool in the 2D view.
// Input  : pRender - The interface to use for rendering.
//-----------------------------------------------------------------------------
void CToolPointHandle::RenderTool2D(CRender2D *pRender)
{
	SelectionState_t eState = m_pPoint->SetSelectionState(SELECT_MODIFY);
	m_pPoint->Render2D(pRender);
	m_pPoint->SetSelectionState(eState);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolPointHandle::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	static CMenu menu, menuCreate;
	static bool bInit = false;

	if (!bInit)
	{
		bInit = true;

		// Create the menu.
		menu.LoadMenu(IDR_POPUPS);
		menuCreate.Attach(::GetSubMenu(menu.m_hMenu, IDM_POPUP_POINT_HANDLE));

		// Create the window that handles menu messages.
		s_wndToolMessage.Create();
	}

	if (!pView->PointInClientRect(vPoint) )
	{
		return false;
	}

	CPoint ptScreen( vPoint.x,vPoint.y);
	pView->ClientToScreen(&ptScreen);

	s_wndToolMessage.PreMenu2D(this, pView);
	menuCreate.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, &s_wndToolMessage);

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolPointHandle::CenterOnParent(CMapView *pView)
{
	if (m_pPoint)
	{
		GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "Center Origin");
		GetHistory()->Keep(m_pPoint);

		CMapClass *pParent = m_pPoint->GetParent();

		Vector vecCenter;
		pParent->GetBoundsCenter(vecCenter);
		m_pPoint->UpdateOrigin(vecCenter);
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
	}
}

