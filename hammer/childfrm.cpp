//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the MDI child window. Each MDI child window contains one
//			or more views. If it contains more than one view, there is a splitter
//			seperating the views. All views in a given MDI child window reflect
//			a single document.
//
//===========================================================================//

#include "stdafx.h"
#include <oleauto.h>
#include <oaidl.h>
#if _MSC_VER < 1300
#include <afxpriv.h>
#else
#define WM_INITIALUPDATE    0x0364  // (params unused) - sent to children
#endif
#include "hammer.h"
#include "Options.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapViewLogical.h"
#include "MapView3D.h"
#include "GlobalFunctions.h"
#include "materialdlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWnd)


BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame)
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_CLOSE()
	ON_COMMAND(ID_VIEW_2DXY, OnView2dxy)
	ON_COMMAND(ID_VIEW_2DYZ, OnView2dyz)
	ON_COMMAND(ID_VIEW_2DXZ, OnView2dxz)
	ON_COMMAND(ID_VIEW_2DLOGICAL, OnViewLogical)
	ON_COMMAND(ID_VIEW_3DPOLYGON, OnView3dPolygon)
	ON_COMMAND(ID_VIEW_3DTEXTURED, OnView3dTextured)
	ON_COMMAND(ID_VIEW_3DTEXTURED_SHADED, OnView3dTexturedShaded)
	ON_COMMAND(ID_VIEW_LIGHTINGPREVIEW, OnView3dLightingPreview)
	ON_COMMAND(ID_VIEW_LIGHTINGPREVIEW_RAYTRACED, OnView3dLightingPreviewRayTraced)
	ON_COMMAND(ID_VIEW_3DLIGHTMAP_GRID, OnView3dLightmapGrid)
	ON_COMMAND(ID_VIEW_3DWIREFRAME, OnView3dWireframe)
	ON_COMMAND(ID_VIEW_3DSMOOTH, OnView3dSmooth)
	//ON_COMMAND(ID_VIEW_3DENGINE, OnView3dEngine)
	ON_COMMAND(ID_VIEW_AUTOSIZE4, OnViewAutosize4)
	ON_UPDATE_COMMAND_UI(ID_VIEW_AUTOSIZE4, OnUpdateViewAutosize4)
	ON_COMMAND(ID_VIEW_MAXIMIZEPANE, OnViewMaximizepane)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MAXIMIZERESTOREACTIVEVIEW, OnUpdateViewMaximizepane)
	ON_COMMAND(ID_WINDOW_TOGGLE, OnWindowToggle)
	ON_COMMAND(ID_VIEW_MAXIMIZERESTOREACTIVEVIEW, OnViewMaximizepane)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


static BOOL g_b4Views = TRUE;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : b4Views - 
//-----------------------------------------------------------------------------
void SetDefaultChildType(BOOL b4Views)
{
	g_b4Views = b4Views;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CChildFrame::CChildFrame(void)
{
	m_bReady = FALSE;
	bAutosize4 = TRUE;
	bFirstPaint = TRUE;
	bUsingSplitter = !g_b4Views ? FALSE : !Options.general.bIndependentwin;
	m_wndSplitter = NULL;
	m_bNeedsCentered = FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChildFrame::~CChildFrame(void)
{
	if (m_wndSplitter != NULL)
	{
		m_wndSplitter->DestroyWindow();
		delete m_wndSplitter;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cs - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	//cs.style |= WS_MAXIMIZE;

	return(CMDIChildWnd::PreCreateWindow(cs));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : CView *
//-----------------------------------------------------------------------------
CView * CChildFrame::GetActiveView()
{
	// find active pane in splitter wnd and replace it there
	int iRow, iCol;
	CView *pCurrentView;

	if(bUsingSplitter)
	{
		m_wndSplitter->GetActivePane(&iRow, &iCol);

		// If no active view for the frame, return FALSE because this
		// function retrieves the current document from the active view.
		if ((pCurrentView= (CView*) m_wndSplitter->GetPane(iRow, iCol))==NULL)
			return NULL;
	}
	else
	{
		pCurrentView = CMDIChildWnd::GetActiveView();
	}

	return pCurrentView;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSplitter - 
//-----------------------------------------------------------------------------
void CChildFrame::SetSplitterMode(BOOL bSplitter)
{
	if(bSplitter == bUsingSplitter)
		return;	// already at this mode

	if(!bSplitter)
	{
		// delete the splitterwnd.. first, get the view that it is currently
		//  using.
		CView * pActiveView = GetActiveView();
		CDocument* pDoc = pActiveView->GetDocument();

	    BOOL bAutoDelete=pDoc->m_bAutoDelete;
		pDoc->m_bAutoDelete=FALSE;

		pActiveView->SetRedraw(FALSE);
		m_wndSplitter->SetRedraw(FALSE);
		pActiveView->SetParent(this);

		m_wndSplitter->DestroyWindow();
		delete m_wndSplitter;
		m_wndSplitter = NULL;

		pDoc->m_bAutoDelete = bAutoDelete;

		// redraw active view
		CRect r;
		GetClientRect(r);
		pActiveView->SetRedraw(TRUE);
		pActiveView->MoveWindow(0, 0, r.right, r.bottom, TRUE);
	}
	else
	{
		CView * pActiveView = GetActiveView();
		Assert(pActiveView);
		CMapDoc* pDoc = (CMapDoc*) pActiveView->GetDocument();

	    BOOL bAutoDelete=pDoc->m_bAutoDelete;
		pDoc->m_bAutoDelete=FALSE;
		pActiveView->DestroyWindow();
		pDoc->m_bAutoDelete = bAutoDelete;

		// creating new four views.
		m_wndSplitter = new CMySplitterWnd;
		if(!m_wndSplitter->CreateStatic(this, 2, 2))
		{
			TRACE0("Failed to create split bar ");
			return;    // failed to create
		}

		CRect r;
		GetClientRect(r);
		CSize sizeView(r.Width()/2 - 3, r.Height()/2 - 3);

		CCreateContext context;
		context.m_pNewViewClass = NULL;
		context.m_pCurrentDoc = pDoc;
		context.m_pNewDocTemplate = NULL;
		context.m_pLastView = NULL;
		context.m_pCurrentFrame = this;

		context.m_pNewViewClass = RUNTIME_CLASS(CMapView2D);
		m_wndSplitter->CreateView(0, 1, RUNTIME_CLASS(CMapView2D),
			sizeView, &context);
		m_wndSplitter->CreateView(1, 0, RUNTIME_CLASS(CMapView2D),
			sizeView, &context);
		m_wndSplitter->CreateView(1, 1, RUNTIME_CLASS(CMapView2D),
			sizeView, &context);

		context.m_pNewViewClass = RUNTIME_CLASS(CMapView3D);

		m_wndSplitter->CreateView(0, 0, context.m_pNewViewClass,
			sizeView, &context);
	}

	bUsingSplitter = bSplitter;
}


//-----------------------------------------------------------------------------
// Purpose: Replaces the current active view with a given view type.
// Input  : *pViewClass - 
// Output : CView
//-----------------------------------------------------------------------------
CView *CChildFrame::ReplaceView(CRuntimeClass *pViewClass)
{
	//
	// Verify that there is an active view to replace.
	//
	CView *pCurrentView = GetActiveView();
	if (!pCurrentView)
	{
		return(NULL);
	}
	
	//
	// If we're already displaying this kind of view, no need to go
	// further.
	//
	if ((pCurrentView->IsKindOf(pViewClass)) == TRUE)
	{
		return(pCurrentView);
	}
 
	//
	// Get pointer to CDocument object so that it can be used in the
	// creation process of the new view. Set flag so that the document
	// will not be deleted when view is destroyed.
	//
	CMapDoc *pDoc = (CMapDoc *)pCurrentView->GetDocument();
    BOOL bAutoDelete = pDoc->m_bAutoDelete;
    pDoc->m_bAutoDelete=FALSE;

	int iRow = 0, iCol = 0;
	CRect rect;

	// Delete existing view
	if (bUsingSplitter)
	{
		pCurrentView->GetClientRect(rect);
		m_wndSplitter->GetActivePane(&iRow, &iCol);
		m_wndSplitter->DeleteView(iRow, iCol);
	}
	else
	{
		pCurrentView->DestroyWindow();
	}

    // Restore the autodelete flag.
    pDoc->m_bAutoDelete = bAutoDelete;
 
	// Create new view and redraw.
	CCreateContext context;

	context.m_pNewViewClass = pViewClass;
	context.m_pCurrentDoc = pDoc;
	context.m_pNewDocTemplate = NULL;
	context.m_pLastView = NULL;
	context.m_pCurrentFrame=this;
 
	CView *pNewView = NULL;

	if (bUsingSplitter)
	{
		if (m_wndSplitter->CreateView(iRow, iCol, pViewClass, rect.Size(), &context))
		{
			pNewView = (CView *)m_wndSplitter->GetPane(iRow, iCol);
		}
	}
	else
	{
		pNewView = (CView *)pViewClass->CreateObject();
		if (pNewView)
		{
			CRect r;
			GetClientRect(r);

			if (!pNewView->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, r, this, AFX_IDW_PANE_FIRST, &context))
			{
				pNewView = NULL;
			}
		}
 	}

	if (!pNewView) 
	{
		TRACE0("Warning: couldn't create view for frame\n");
		return(NULL);
	}
 
	pNewView->SendMessage(WM_INITIALUPDATE, 0, 0);

	if (bUsingSplitter)
	{
		m_wndSplitter->RecalcLayout();
	}
 
	return(pNewView);
}


#ifdef _DEBUG
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CChildFrame::AssertValid() const
{
	CMDIChildWnd::AssertValid();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dc - 
//-----------------------------------------------------------------------------
void CChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWnd::Dump(dc);
}

#endif //_DEBUG


//-----------------------------------------------------------------------------
// Purpose: Stores layout information in the registry for use next session.
//-----------------------------------------------------------------------------
void CChildFrame::SaveOptions(void)
{
	if (bUsingSplitter)
	{
		//
		// Save the draw type for each pane of the splitter.
		//
		for (int nRow = 0; nRow < 2; nRow++)
		{
			for (int nCol = 0; nCol < 2; nCol++)
			{
				CMapView *pView =  dynamic_cast<CMapView*>(m_wndSplitter->GetPane(nRow, nCol));
				if (pView != NULL)
				{
					char szKey[30];
					sprintf(szKey, "DrawType%d,%d", nRow, nCol);
					APP()->WriteProfileInt("Splitter", szKey, pView->GetDrawType());
				}
			}
		}

		int nWidth, nHeight, nMin;
		m_wndSplitter->GetColumnInfo( 0, nWidth, nMin );
		m_wndSplitter->GetRowInfo( 0, nHeight, nMin );

		APP()->WriteProfileInt( "Splitter", "SplitterWidth", nWidth );
		APP()->WriteProfileInt( "Splitter", "SplitterHeight", nHeight );

		//
		// Save the window position, size, and minimized/maximized state.
		//
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		GetWindowPlacement(&wp);

		char szPlacement[100];
		sprintf(szPlacement, "(%d %d) (%d %d) (%d %d %d %d) %d", wp.ptMaxPosition.x, wp.ptMaxPosition.y, wp.ptMinPosition.x, wp.ptMinPosition.y, wp.rcNormalPosition.bottom, wp.rcNormalPosition.left, wp.rcNormalPosition.right, wp.rcNormalPosition.top, wp.showCmd);
		APP()->WriteProfileString("Splitter", "WindowPlacement", szPlacement);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calls ReplaceView with the appropriate runtime class information to
//			switch the active view to given view type.
// Input  : eViewType - 2d xy, xz, 3d textured, flat, etc.
//-----------------------------------------------------------------------------
void CChildFrame::SetViewType(DrawType_t eViewType)
{
	CMapView *pNewView = NULL;
	
	switch (eViewType)
	{
		case VIEW2D_XY:
		case VIEW2D_XZ:
		case VIEW2D_YZ:
			pNewView = (CMapView2D *)ReplaceView(RUNTIME_CLASS(CMapView2D));
			break;

		case VIEW_LOGICAL:
			pNewView = (CMapViewLogical *)ReplaceView(RUNTIME_CLASS(CMapViewLogical));
			break;

		default:
		case VIEW3D_WIREFRAME:
		case VIEW3D_POLYGON:
		case VIEW3D_TEXTURED:
		case VIEW3D_TEXTURED_SHADED:
		case VIEW3D_LIGHTMAP_GRID:
		case VIEW3D_LIGHTING_PREVIEW2:
		case VIEW3D_LIGHTING_PREVIEW_RAYTRACED:
		case VIEW3D_SMOOTHING_GROUP:
		//case VIEW3D_ENGINE:
			pNewView = (CMapView3D *)ReplaceView(RUNTIME_CLASS(CMapView3D));
			break;
	}

	if (pNewView != NULL)
	{
		SetActiveView( dynamic_cast<CView*>(pNewView->GetViewWnd()) );
		pNewView->SetDrawType(eViewType);
		pNewView->UpdateView( MAPVIEW_UPDATE_OBJECTS );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_2DXY command when the active view is a 3D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView2dxy(void)
{
	SetViewType(VIEW2D_XY);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_2DYZ command when the active view is a 3D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView2dyz(void)
{
	SetViewType(VIEW2D_YZ);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_2DXZ command when the active view is a 3D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView2dxz(void)
{
	SetViewType(VIEW2D_XZ);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_2DLOGICAL command when the active view is a 3D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnViewLogical(void)
{
	SetViewType(VIEW_LOGICAL);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_3DWIREFRAME command when the active view is a 2D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView3dWireframe(void)
{
	SetViewType(VIEW3D_WIREFRAME);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_3DPOLYGON command when the active view is a 2D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView3dPolygon(void)
{
	SetViewType(VIEW3D_POLYGON);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_3DTEXTURED command when the active view is a 2D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView3dTextured(void)
{
	SetViewType(VIEW3D_TEXTURED);
}

void CChildFrame::OnView3dTexturedShaded(void)
{
	SetViewType(VIEW3D_TEXTURED_SHADED);
}

void CChildFrame::OnView3dLightingPreview(void)
{
	SetViewType(VIEW3D_LIGHTING_PREVIEW2);
}

void CChildFrame::OnView3dLightingPreviewRayTraced(void)
{
	SetViewType(VIEW3D_LIGHTING_PREVIEW_RAYTRACED);
}
//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_3DTEXTURED command when the active view is a 2D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView3dLightmapGrid(void)
{
	SetViewType(VIEW3D_LIGHTMAP_GRID);
}

//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_3DSMOOTH command when the active view is a 3D view.
//-----------------------------------------------------------------------------
void CChildFrame::OnView3dSmooth(void)
{
	SetViewType(VIEW3D_SMOOTHING_GROUP);
}

//-----------------------------------------------------------------------------
// Purpose: Handles the ID_VIEW_3DENGINE command when the active view is a 2D view.
//-----------------------------------------------------------------------------
//void CChildFrame::OnView3dEngine(void)
//{
//	SetViewType(VIEW3D_ENGINE);
//}


//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the 2x2 splitter. If the splitter is enabled,
//			the splitter window is createed and one 3D view and three 2D views
//			are added to it.
// Input  : lpcs - 
//			pContext - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CChildFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
	//
	// If we are using the splitter, create the splitter and 4 views.
	//
	if (bUsingSplitter)
	{
		m_wndSplitter = new CMySplitterWnd;
		Assert(m_wndSplitter != NULL);
		
		if (m_wndSplitter == NULL)
		{
			return(FALSE);
		}

		if (!m_wndSplitter->CreateStatic(this, 2, 2))
		{
			delete m_wndSplitter;
			m_wndSplitter = NULL;
			TRACE0("Failed to create split bar ");
			return(FALSE);
		}
		
		//
		// Calculate the size of each view within the splitter,
		//
		CRect r;
		GetClientRect(r);
		CSize sizeView((r.Width() / 2) - 3, (r.Height() / 2) - 3);

		//
		// Create the 4 views as they were when the user last closed the app.
		//
		DrawType_t eDrawType[2][2];

		eDrawType[0][0] = (DrawType_t)APP()->GetProfileInt("Splitter", "DrawType0,0", VIEW3D_WIREFRAME);
		eDrawType[0][1] = (DrawType_t)APP()->GetProfileInt("Splitter", "DrawType0,1", VIEW2D_XY);
		eDrawType[1][0] = (DrawType_t)APP()->GetProfileInt("Splitter", "DrawType1,0", VIEW2D_YZ);
		eDrawType[1][1] = (DrawType_t)APP()->GetProfileInt("Splitter", "DrawType1,1", VIEW2D_XZ);

		for (int nRow = 0; nRow < 2; nRow++)
		{
			for (int nCol = 0; nCol < 2; nCol++)
			{
				// These might be lying around in people's registry.
				if ((eDrawType[nRow][nCol] == VIEW3D_ENGINE) || (eDrawType[nRow][nCol] >= VIEW_TYPE_LAST))
				{
					eDrawType[nRow][nCol] = VIEW3D_TEXTURED;
				}
			
				switch (eDrawType[nRow][nCol])
				{
					case VIEW2D_XY:
					case VIEW2D_XZ:
					case VIEW2D_YZ:
					{
						m_wndSplitter->CreateView(nRow, nCol, RUNTIME_CLASS(CMapView2D), sizeView, pContext);
						break;
					}

					case VIEW_LOGICAL:
					{
						m_wndSplitter->CreateView(nRow, nCol, RUNTIME_CLASS(CMapViewLogical), sizeView, pContext);
						break;
					}

					case VIEW3D_WIREFRAME:
					case VIEW3D_POLYGON:
					case VIEW3D_TEXTURED:
					case VIEW3D_TEXTURED_SHADED:
					case VIEW3D_LIGHTMAP_GRID:
					case VIEW3D_LIGHTING_PREVIEW2:
					case VIEW3D_LIGHTING_PREVIEW_RAYTRACED:
					case VIEW3D_SMOOTHING_GROUP:
					{
						m_wndSplitter->CreateView(nRow, nCol, RUNTIME_CLASS(CMapView3D), sizeView, pContext);
						break;
					}
				}

				CMapView *pView = dynamic_cast<CMapView*>(m_wndSplitter->GetPane(nRow, nCol));
				if (pView != NULL)
				{
					pView->SetDrawType(eDrawType[nRow][nCol]);
				}
			}
		}
		
		int nWidth = APP()->GetProfileInt("Splitter", "SplitterWidth", -1);
		int nHeight = APP()->GetProfileInt("Splitter", "SplitterHeight", -1);

		if ( nWidth != -1 && nHeight != -1 )
		{
			m_wndSplitter->SetRowInfo( 0, nHeight, 0 );
			m_wndSplitter->SetColumnInfo( 0, nWidth, 0 );
			m_wndSplitter->RecalcLayout();
		}
		else
		{
			m_bNeedsCentered = TRUE;
		}

        m_bReady = TRUE;
		return TRUE;
	}

	//
	// No splitter, call default creation code.
	//
	return(CMDIChildWnd::OnCreateClient(lpcs, pContext));
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChildFrame::CenterViews(void)
{
	if (!bUsingSplitter || !m_bReady)
	{
		return;
	}

	WriteDebug("childfrm::centerviews");

	CRect r;
	GetClientRect(r);
	CSize sizeView(r.Width()/2 - 3, r.Height()/2 - 3);

	sizeView.cy = max(0, sizeView.cy);
	sizeView.cx = max(0, sizeView.cx);

	m_wndSplitter->SetRowInfo(0, sizeView.cy, 0);
	m_wndSplitter->SetRowInfo(1, sizeView.cy, 0);
	m_wndSplitter->SetColumnInfo(0, sizeView.cx, 0);
	m_wndSplitter->SetColumnInfo(1, sizeView.cx, 0);
	m_wndSplitter->RecalcLayout();
	
	WriteDebug("childfrm::centerviews done");
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChildFrame::OnViewAutosize4(void)
{
	if (!bUsingSplitter)
	{
		return;
	}

	if (0) // bAutosize4)
	{
		bAutosize4 = FALSE;
	}
	else
	{
		// resize 4 views
		CenterViews();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CChildFrame::OnUpdateViewAutosize4(CCmdUI *pCmdUI) 
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CChildFrame::OnSize(UINT nType, int cx, int cy) 
{
	CMDIChildWnd::OnSize(nType, cx, cy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpCreateStruct - 
// Output : int
//-----------------------------------------------------------------------------
int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
	{
		return(-1);
	}

	//
	// The splitter gets its layout from the registry.
	//
	if ( bUsingSplitter && CHammer::IsNewDocumentVisible() )
	{
		CString str = APP()->GetProfileString("Splitter", "WindowPlacement", "");
		if (!str.IsEmpty())
		{
			WINDOWPLACEMENT wp;
			wp.length = sizeof(wp);
			wp.flags = 0;
			sscanf(str, "(%d %d) (%d %d) (%d %d %d %d) %d", &wp.ptMaxPosition.x, &wp.ptMaxPosition.y, &wp.ptMinPosition.x, &wp.ptMinPosition.y, &wp.rcNormalPosition.bottom, &wp.rcNormalPosition.left, &wp.rcNormalPosition.right, &wp.rcNormalPosition.top, &wp.showCmd);

			if (wp.showCmd == SW_SHOWMAXIMIZED)
			{
				PostMessage(WM_SYSCOMMAND, SC_MAXIMIZE);
			}
			else
			{
				SetWindowPlacement(&wp);
			}
		}
	}

	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChildFrame::OnPaint(void)
{
	if (bFirstPaint)
	{
		ValidateRect(NULL);
		bFirstPaint = FALSE;
		if ( m_bNeedsCentered )
		{
			CenterViews();
		}
		Invalidate();
		return;
	}

	CPaintDC dc(this); // device context for painting
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWnd - 
//-----------------------------------------------------------------------------
void CMySplitterWnd::ToggleMax(CWnd *pWnd)
{
	int ir, ic;

	if(!pMaxPrev)
	{
		int dummy;
		// save current info
		GetRowInfo(0, sizePrev[1][0], dummy);
		GetRowInfo(1, sizePrev[1][1], dummy);
		GetColumnInfo(0, sizePrev[0][0], dummy);
		GetColumnInfo(1, sizePrev[0][1], dummy);
	}

	if(pWnd != pMaxPrev)
	{
		// maximize this one
		int iRow, iCol;
		CRect r;
		GetClientRect(r);
		VERIFY(IsChildPane(pWnd, &iRow, &iCol));

		for(ir = 0; ir < 2; ir++)
		{
			for(ic = 0; ic < 2; ic++)
			{
				SetRowInfo(ir, 0, 0);
				SetColumnInfo(ic, 0, 0);
			}
		}

		SetRowInfo(iRow, r.Height(), 5);
		SetColumnInfo(iCol, r.Width(), 5);

		pMaxPrev = pWnd;
	}
	else
	{
		// restore saved info
		SetRowInfo(0, sizePrev[1][0], 0);
		SetRowInfo(1, sizePrev[1][1], 0);
		SetColumnInfo(0, sizePrev[0][0], 0);
		SetColumnInfo(1, sizePrev[0][1], 0);

		pMaxPrev = NULL;
	}

	RecalcLayout();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChildFrame::OnViewMaximizepane(void)
{
	if (!bUsingSplitter)
	{
		return;
	}

	// find current wndsplitter pane, & call ToggleMax() in
	// cmysplitterwnd.
	if (m_wndSplitter->GetActivePane())
	{
		m_wndSplitter->ToggleMax(m_wndSplitter->GetActivePane());
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pCmdUI - 
//-----------------------------------------------------------------------------
void CChildFrame::OnUpdateViewMaximizepane(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable();	
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CChildFrame::OnWindowToggle(void)
{
//	SetSplitterMode(!bUsingSplitter);
}


//-----------------------------------------------------------------------------
// Purpose: Saves the splitter setup for next time.
//-----------------------------------------------------------------------------
void CChildFrame::OnClose(void)
{
	SaveOptions();
	CFrameWnd::OnClose();
}
void CChildFrame::OnSetFocus( CWnd* pOldWnd )
{
	CMDIChildWnd::OnSetFocus( pOldWnd );

	CMapDoc *pDoc = dynamic_cast<CMapDoc*>(GetActiveDocument());

	if ( pDoc )
		CMapDoc::SetActiveMapDoc( pDoc );
}

