//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Options.h"
#include "StatusBarIDs.h"
#include "ToolBlock.h"
#include "ToolManager.h"
#include "vgui/Cursor.h"
#include "Selection.h"


class CToolBlockMessageWnd : public CWnd
{
	public:

		bool Create(void);
		void PreMenu2D(CToolBlock *pTool, CMapView2D *pView);

	protected:

		//{{AFX_MSG_MAP(CToolBlockMessageWnd)
		afx_msg void OnCreateObject();
		//}}AFX_MSG
	
		DECLARE_MESSAGE_MAP()

	private:

		CToolBlock *m_pToolBlock;
		CMapView2D *m_pView2D;
};


static CToolBlockMessageWnd s_wndToolMessage;
static const char *g_pszClassName = "ValveEditor_BlockToolWnd";


BEGIN_MESSAGE_MAP(CToolBlockMessageWnd, CWnd)
	//{{AFX_MSG_MAP(CToolMessageWnd)
	ON_COMMAND(ID_CREATEOBJECT, OnCreateObject)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Creates the hidden window that receives context menu commands for the
//			block tool.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolBlockMessageWnd::Create(void)
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
// Purpose: Attaches the block tool to this window before activating the context
//			menu.
//-----------------------------------------------------------------------------
void CToolBlockMessageWnd::PreMenu2D(CToolBlock *pToolBlock, CMapView2D *pView)
{
	Assert(pToolBlock != NULL);
	m_pToolBlock = pToolBlock;
	m_pView2D = pView;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolBlockMessageWnd::OnCreateObject()
{
	m_pToolBlock->CreateMapObject(m_pView2D);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CToolBlock::CToolBlock(void)
{
	// We always show our dimensions when we render.
	SetDrawFlags(GetDrawFlags() | Box3D::boundstext);
	SetDrawColors(Options.colors.clrToolHandle, Options.colors.clrToolBlock);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CToolBlock::~CToolBlock(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 2D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar)
	{
		case VK_RETURN:
		{
			if ( !IsEmpty() )
			{
				CreateMapObject(pView);
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
// Purpose: Handles context menu events in the 2D view.
// Input  : Per CWnd::OnContextMenu.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	static CMenu menu, menuCreate;
	static bool bInit = false;

	if (!bInit)
	{
		bInit = true;

		// Create the menu.
		menu.LoadMenu(IDR_POPUPS);
		menuCreate.Attach(::GetSubMenu(menu.m_hMenu, 1));

		// Create the window that handles menu messages.
		s_wndToolMessage.Create();
	}

	if ( !pView->PointInClientRect(vPoint) )
	{
		return false;
	}

	if ( !IsEmpty() )
	{
		if ( HitTest(pView, vPoint, false) )
		{
			CPoint ptScreen( vPoint.x,vPoint.y);
			pView->ClientToScreen(&ptScreen);

			s_wndToolMessage.PreMenu2D(this, pView);
			menuCreate.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, &s_wndToolMessage);
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	Tool3D::OnLMouseDown3D(pView, nFlags, vPoint);

	//
	// If we have a box, see if they clicked on any part of it.
	//
	if ( !IsEmpty() )
	{
		if ( HitTest( pView, vPoint, true ) )
		{
			// just update current block
			StartTranslation( pView, vPoint, m_LastHitTestHandle );
			return true;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	// If we have a box, see if they clicked on any part of it.
	if ( !IsEmpty() )
	{
		if ( HitTest( pView, vPoint, true ) )
		{
			// just update current block
            StartTranslation( pView, vPoint, m_LastHitTestHandle );
			return true;
		}
	}
		
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	if (IsTranslating())
	{
		FinishTranslation(true);
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp3D(pView, nFlags, vPoint);

	if (IsTranslating())
	{
		FinishTranslation(true);
	}

	m_pDocument->UpdateStatusbar();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	vgui::HCursor hCursor = vgui::dc_arrow;

	// Snap it to the grid.
	unsigned int uConstraints = GetConstraints( nFlags );

	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);
							    
	//
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
	
	if ( IsTranslating() )
	{
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints);

		hCursor =  vgui::dc_none;
	}
	else if ( m_bMouseDragged[MOUSE_LEFT] )
	{
		// Starting a new box. Build a starting point for the drag.
		pView->SetCursor( "Resource/block.cur" );

		Vector vecStart;
		pView->ClientToWorld(vecStart, m_vMouseStart[MOUSE_LEFT] );
		
		// Snap it to the grid.
		if ( uConstraints & constrainSnap )
			m_pDocument->Snap( vecStart, uConstraints);

		// Start the new box with the extents of the last selected thing.

		m_pDocument->GetSelection()->GetLastValidBounds(bmins, bmaxs);

		vecStart[pView->axThird] = bmins[pView->axThird];

		StartNew(pView, vPoint, vecStart, pView->GetViewAxis() * (bmaxs-bmins) );
	}
	else if ( !IsEmpty() )
	{
		// If the cursor is on a handle, set it to a cross.
		if ( HitTest(pView, vPoint, true) )
		{
			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
	}
	

	if ( hCursor !=  vgui::dc_none )
		pView->SetCursor( hCursor );

	return true;
}

bool CToolBlock::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnMouseMove3D(pView, nFlags, vPoint);

	if ( IsTranslating() )
	{
		unsigned int uConstraints = GetConstraints( nFlags );

		// If they are dragging with a valid handle, update the views.
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 3D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar)
	{
		case VK_RETURN:
		{
			if ( !IsEmpty() )
			{
				CreateMapObject(pView);
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
void CToolBlock::OnEscape(void)
{
	//
	// If we have nothing blocked, go back to the pointer tool.
	//
	if ( IsEmpty() )
	{
		ToolManager()->SetTool(TOOL_POINTER);
	}
	//
	// We're blocking out a region, so clear it.
	//
	else
	{
		SetEmpty();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates a solid in the given view.
//-----------------------------------------------------------------------------
void CToolBlock::CreateMapObject(CMapView *pView)
{
	CMapWorld *pWorld = m_pDocument->GetMapWorld();

	if (!(bmaxs[0] - bmins[0]) || !(bmaxs[1] - bmins[1]) || !(bmaxs[2] - bmins[2]))
	{
		AfxMessageBox("The box is empty.");
		SetEmpty();
		return;
	}

	BoundBox NewBox = *this;
	if (Options.view2d.bOrientPrimitives)
	{
		switch (pView->GetDrawType())
		{
			case VIEW2D_XY:
			{
				break;
			}

			case VIEW2D_YZ:
			{
				NewBox.Rotate90(AXIS_Y);
				break;
			}

			case VIEW2D_XZ:
			{
				NewBox.Rotate90(AXIS_X);
				break;
			}
		}
	}

	// Create the object within the given bounding box.
	CMapClass *pObject = GetMainWnd()->m_ObjectBar.CreateInBox(&NewBox, pView);
	if (pObject == NULL)
	{
		SetEmpty();
		return;
	}

	m_pDocument->ExpandObjectKeywords(pObject, pWorld);

	GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "New Object");

	m_pDocument->AddObjectToWorld(pObject);

	//
	// Do we need to rotate this object based on the view we created it in?
	//
	if (Options.view2d.bOrientPrimitives)
	{
		Vector center;
		pObject->GetBoundsCenter( center );

		QAngle angles(0, 0, 0);
		switch (pView->GetDrawType())
		{
			case VIEW2D_XY:
			{
				break;
			}
			
			case VIEW2D_YZ:
			{
 				angles[1] = 90.f;
				pObject->TransRotate(center, angles);
				break;
			}

			case VIEW2D_XZ:
			{
				angles[0] = 90.f;
				pObject->TransRotate(center, angles);
				break;
			}
		}
	}

	GetHistory()->KeepNew(pObject);

	// Select the new object.
	m_pDocument->SelectObject(pObject, scClear|scSelect|scSaveChanges);

	m_pDocument->SetModifiedFlag();

	SetEmpty();
	ResetBounds();
}





