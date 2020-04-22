//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rendering and mouse handling in the 2D view.
//
//============================================================================//

#include "stdafx.h"
#include "MapView2DBase.h"
#include "hammer.h"
#include "MapEntity.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapViewLogical.h"
#include "MapView3D.h"
#include "tooldefs.h"
#include "StockSolids.h"
#include "statusbarids.h"
#include "ObjectProperties.h"
#include "Options.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "Render2D.h"
#include "TitleWnd.h"
#include "ToolManager.h"
#include "ToolMorph.h"		// FIXME: remove
#include "ToolInterface.h"
#include "MapPlayerHullHandle.h"
#include "vgui_controls/EditablePanel.h"
#include "camera.h"
#include "material.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define SnapToGrid(line,grid) (line - (line % grid))

#define ZOOM_MIN_DEFAULT	0.02125
#define ZOOM_MAX			256.0

static float s_fDragRestX, s_fDragRestY;


IMPLEMENT_DYNCREATE(CMapView2DBase, CView)

class CMapView2DBasePanel : public vgui::EditablePanel
{
public:
	CMapView2DBasePanel( CMapView2DBase *pMapView, const char *panelName ) : 
		vgui::EditablePanel( NULL, panelName )
	{
		m_pMapView = pMapView;
	}

	virtual void OnSizeChanged(int newWide, int newTall)
	{
		// call Panel and not EditablePanel OnSizeChanged.
		Panel::OnSizeChanged(newWide, newTall);
	}

	virtual void Paint()
	{
		m_pMapView->Render();
	}

	CMapView2DBase *m_pMapView;
};

BEGIN_MESSAGE_MAP(CMapView2DBase, CView)
	//{{AFX_MSG_MAP(CMapView2DBase)
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_RBUTTONDOWN()
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_COMMAND(ID_EDIT_PROPERTIES, OnEditProperties)
	ON_WM_KEYUP()
	ON_WM_CHAR()
	ON_WM_RBUTTONUP()
	ON_UPDATE_COMMAND_UI(ID_CREATEOBJECT, OnUpdateEditFunction)
	ON_WM_ERASEBKGND()
	ON_UPDATE_COMMAND_UI(ID_EDIT_PROPERTIES, OnUpdateEditFunction)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//	---------------------------------------------------------------------------
CMapView2DBase::CMapView2DBase(void)
{
	//
	// Must initialize the title window pointer before calling SetDrawType!
	//
	m_pwndTitle = NULL;

	m_flMinZoom = ZOOM_MIN_DEFAULT;

	m_fZoom = -1;			// make sure setzoom performs
	m_vViewOrigin.Init();
	
	m_ViewMin.Init();
	m_ViewMax.Init();

	m_xScroll = m_yScroll = 0;
	m_bActive = false;
	m_bMouseDrag = false;
	
	m_pCamera = new CCamera();

	m_pCamera->SetOrthographic( 0.25f, -99999, 99999 );

	m_pRender = new CRender2D();

	m_pRender->SetView( this );

	m_pRender->SetDefaultRenderMode( RENDER_MODE_FLAT_NOZ );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees dynamically allocated resources.
//-----------------------------------------------------------------------------
CMapView2DBase::~CMapView2DBase(void)
{
	if (m_pwndTitle != NULL)
	{
		delete m_pwndTitle;
	}

	if ( m_pCamera )
	{
		delete m_pCamera;
	}

	if ( m_pRender )
	{
		delete m_pRender;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2DBase::UpdateTitleWindowPos(void)
{
	if (m_pwndTitle != NULL)
	{
		if (!::IsWindow(m_pwndTitle->m_hWnd))
		{
			return;
		}

		m_pwndTitle->SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		m_pwndTitle->ShowWindow(SW_SHOW);
	}
}


//-----------------------------------------------------------------------------
// Create a title window.
//-----------------------------------------------------------------------------
void CMapView2DBase::CreateTitleWindow(void)
{
	m_pwndTitle = CTitleWnd::CreateTitleWnd(this, ID_2DTITLEWND);
	Assert(m_pwndTitle != NULL);
	UpdateTitleWindowPos();
}


//-----------------------------------------------------------------------------
// Purpose: First-time initialization of this view.
//-----------------------------------------------------------------------------
void CMapView2DBase::OnInitialUpdate(void)
{
	// CMainFrame::LoadWindowStates calls InitialUpdateFrame which causes us to get two
	// OnInitialUpdate messages! Check for a NULL renderer to avoid processing twice.
	if ( GetMainPanel() != NULL )
		return;

	CMapDoc *pDoc = GetMapDoc();
	m_pToolManager = pDoc->GetTools();

	CenterView();
	SetColorMode(Options.view2d.bWhiteOnBlack);

	ShowScrollBar(SB_HORZ, Options.view2d.bScrollbars);
	ShowScrollBar(SB_VERT, Options.view2d.bScrollbars);

	CView::OnInitialUpdate();

	vgui::EditablePanel *pMainPanel = new CMapView2DBasePanel( this, "MapView2DPanel" );

	SetParentWindow( this );
	SetMainPanel( pMainPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Called by the tools to scroll the 2D view so that a point is visible.
//			Sets a timer to do the scroll so that we don't much with the view state
//			while the tool is handling a mouse message.
// Input  : point - Point in client coordinates to make visible.
//-----------------------------------------------------------------------------
void CMapView2DBase::ToolScrollToPoint(const Vector2D &ptClient)
{
	int nScrollSpeed = 10 / m_fZoom;

	if ((GetCapture() == this) && 
 		( ptClient.x < 0 || ptClient.y  < 0 || ptClient.x >= m_ClientWidth || ptClient.y >= m_ClientHeight ) )
	{
		// reset these
		m_xScroll = m_yScroll = 0;
		if (ptClient.x < 0)
		{
			// scroll left
			m_xScroll = -nScrollSpeed;
		}
		else if (ptClient.x >= m_ClientWidth)
		{
			// scroll right
			m_xScroll = nScrollSpeed;
		}
		if (ptClient.y < 0)
		{
			// scroll up
			m_yScroll = nScrollSpeed;
		}
		else if (ptClient.y >= m_ClientHeight)
		{
			// scroll down
			m_yScroll = -nScrollSpeed;
		}
	
		SetTimer( TIMER_SCROLLVIEW, 10, NULL);
	}
	else
	{
		m_xScroll = m_yScroll = 0;
		KillTimer( TIMER_SCROLLVIEW );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adjusts a color's intensity - will not overbrighten.
// Input  : ulColor - Color to adjust.
//			nIntensity - Percentage of original color intensity to keep (0 - 100).
//			bReverse - True ramps toward black, false ramps toward the given color.
// Output : Returns the adjusted color.
//-----------------------------------------------------------------------------
void CMapView2DBase::AdjustColorIntensity(Color &color, int nIntensity)
{
	if (!Options.view2d.bWhiteOnBlack)
	{
		nIntensity = 100 - nIntensity;
	}

	nIntensity = clamp(nIntensity, 0, 100);
	
	//
	// Adjust each component's intensity.
	//
	color.SetColor( min( (color.r() * nIntensity) / 100, 255 ),
					min( (color.g() * nIntensity) / 100, 255 ),
					min( (color.b() * nIntensity) / 100, 255 ),
					color.a() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bWhiteOnBlack - 
//-----------------------------------------------------------------------------
void CMapView2DBase::SetColorMode(bool bWhiteOnBlack)
{
	// Grid color.
	COLORREF clr = Options.colors.clrGrid;
	m_clrGrid.SetColor( GetRValue(clr), GetGValue(clr), GetBValue(clr), 255 );
	if (Options.colors.bScaleGridColor)
	{
		AdjustColorIntensity(m_clrGrid, Options.view2d.iGridIntensity);
	}

	// Grid highlight color.
	clr = Options.colors.clrGrid10;
	m_clrGridCustom.SetColor( GetRValue(clr), GetGValue(clr), GetBValue(clr), 255 );
	if (Options.colors.bScaleGrid10Color)
	{
		AdjustColorIntensity(m_clrGridCustom, 1.5 * Options.view2d.iGridIntensity);
	}
	

	// Grid 1024 highlight color.
	clr = Options.colors.clrGrid1024;
	m_clrGrid1024.SetColor( GetRValue(clr), GetGValue(clr), GetBValue(clr), 255 );
	if (Options.colors.bScaleGrid1024Color)
	{
		AdjustColorIntensity(m_clrGrid1024, Options.view2d.iGridIntensity);
	}
	
	// Dotted grid color. No need to create a pen since all we do is SetPixel with it.
	clr = Options.colors.clrGridDot;
	m_clrGridDot.SetColor( GetRValue(clr), GetGValue(clr), GetBValue(clr), 255 );
	if (Options.colors.bScaleGridDotColor)
	{
		AdjustColorIntensity(m_clrGridDot, Options.view2d.iGridIntensity + 20);
	}

	// Axis color.
	clr = Options.colors.clrAxis;
	m_clrAxis.SetColor( GetRValue(clr), GetGValue(clr), GetBValue(clr), 255 );
	if (Options.colors.bScaleAxisColor)
	{
		AdjustColorIntensity(m_clrAxis, Options.view2d.iGridIntensity);
	}

	clr = Options.colors.clrBackground;
	m_ClearColor.SetColor( GetRValue(clr), GetGValue(clr), GetBValue(clr), 255 );
	m_bClearZBuffer = false;
}

// quick & dirty:
static bool s_bGridDots;
static int s_iCustomGridSpacing;


bool CMapView2DBase::HighlightGridLine( CRender2D *pRender, int nGridLine )
{
	if (nGridLine == 0)
	{
		pRender->SetDrawColor( m_clrAxis );
		return true;
	}
	//
	// Optionally highlight every 1024.
	//
	if (Options.view2d.bGridHigh1024 && (!(nGridLine % 1024)))
	{
		pRender->SetDrawColor( m_clrGrid1024 );
		return true;
	}
	//
	// Optionally highlight every 64.
	//
	else if (Options.view2d.bGridHigh64 && (!(nGridLine % 64)))
	{
		if (!s_bGridDots)
		{
			pRender->SetDrawColor( m_clrGridCustom );
			return true;
		}
	}
	//
	// Optionally highlight every nth grid line.
	//

	if (Options.view2d.bGridHigh10 && (!(nGridLine % s_iCustomGridSpacing)))
	{
		if (!s_bGridDots)
		{
			pRender->SetDrawColor( m_clrGridCustom );
			return true;
		}
	}

	return false;

}


//-----------------------------------------------------------------------------
// Purpose: Draws the grid, using dots or lines depending on the user setting.
// Input  : pDC - Device context to draw in.
//-----------------------------------------------------------------------------
void CMapView2DBase::DrawGrid(CRender2D *pRender, int xAxis, int yAxis, float depth, bool bNoSmallGrid )
{
	CMapDoc *pDoc = GetMapDoc();

	if (pDoc == NULL)
		return;
	
	// Check for too small grid.
	int nGridSpacing = pDoc->GetGridSpacing();

	// never allow a grid spacing samller then 2 pixel
	while ( ((float)nGridSpacing * m_fZoom) < 2.0f )
	{
		nGridSpacing*=2;
	}

	if ((((float)nGridSpacing * m_fZoom) < 4.0f) && Options.view2d.bHideSmallGrid)
	{
		bNoSmallGrid = true;
	}

	// No dots if too close together.
	s_bGridDots = Options.view2d.bGridDots;
	s_iCustomGridSpacing = nGridSpacing * Options.view2d.iGridHighSpec;

	int xMin = SnapToGrid( (int)max( g_MIN_MAP_COORD, m_ViewMin[xAxis]-nGridSpacing ), nGridSpacing );
	int xMax = SnapToGrid( (int)min( g_MAX_MAP_COORD, m_ViewMax[xAxis]+nGridSpacing ), nGridSpacing );
	int yMin = SnapToGrid( (int)max( g_MIN_MAP_COORD, m_ViewMin[yAxis]-nGridSpacing ), nGridSpacing );
	int yMax = SnapToGrid( (int)min( g_MAX_MAP_COORD, m_ViewMax[yAxis]+nGridSpacing ), nGridSpacing );
	
	
	Assert( xMin < xMax );
	Assert( yMin < yMax );

	// Draw the vertical grid lines.

	Vector vPointMin(depth,depth,depth);
	Vector vPointMax(depth,depth,depth);

	vPointMin[xAxis] = xMin;
	vPointMax[xAxis] = xMax;

	// draw dots first, for the shake of speed do really ugly things
	if (s_bGridDots && !bNoSmallGrid)
	{
		pRender->BeginClientSpace();

		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		IMesh* pMesh = pRenderContext->GetDynamicMesh();

		for (int y = yMin; y <= yMax; y += nGridSpacing )
		{
			Vector vPoint(depth,depth,depth);
			vPoint[yAxis] = y;
			vPoint[xAxis] = xMin;
			Vector2D  v2D; WorldToClient( v2D, vPoint );
			v2D.y = (int)(v2D.y+0.5);
			
			// dot drawing isn't precise enough in world space
			// so we still do it in client space

			int nNumPoints = 1+abs(xMax-xMin)/nGridSpacing;

			meshBuilder.Begin( pMesh, MATERIAL_LINES, nNumPoints );

			float fOffset = nGridSpacing * m_fZoom;

			while( nNumPoints > 0)
			{
                float roundfx = (int)(v2D.x+0.5);
				v2D.x += fOffset;

				meshBuilder.Position3f( roundfx, v2D.y, 0 );
				meshBuilder.Color4ubv( (byte*)&m_clrGridDot );
				meshBuilder.AdvanceVertex();
                
				meshBuilder.Position3f( roundfx+1, v2D.y+1, 0 );
				meshBuilder.Color4ubv( (byte*)&m_clrGridDot );
				meshBuilder.AdvanceVertex();

				nNumPoints--;
			}

			meshBuilder.End();
			pMesh->Draw();
		}

		pRender->EndClientSpace();
	}

	for (int y = yMin; y <= yMax; y += nGridSpacing )
	{
		pRender->SetDrawColor( m_clrGrid );

		int bHighligh = HighlightGridLine( pRender, y );
		
		// Don't draw the base grid if it is too small.
		
		if (!bHighligh && bNoSmallGrid)
			continue;

		// Always draw lines for the axes and map boundaries.

		if ((!s_bGridDots) || (bHighligh) || (y == g_MAX_MAP_COORD) || (y == g_MIN_MAP_COORD))
		{
			vPointMin[yAxis] = vPointMax[yAxis] = y;
			pRender->DrawLine( vPointMin, vPointMax );
		}
	}

	vPointMin[yAxis] = yMin;
	vPointMax[yAxis] = yMax;

	for (int x = xMin; x <= xMax; x += nGridSpacing )
	{
		pRender->SetDrawColor( m_clrGrid );

		int bHighligh = HighlightGridLine( pRender, x );

		// Don't draw the base grid if it is too small.
		if ( !bHighligh && bNoSmallGrid )
			continue;

		// Always draw lines for the axes and map boundaries.
		
 		if ((!s_bGridDots) || (bHighligh) || (x == g_MAX_MAP_COORD) || (x == g_MIN_MAP_COORD))
		{
			vPointMin[xAxis] = vPointMax[xAxis] = x;
			pRender->DrawLine( vPointMin, vPointMax );
		}
	}
}


void CMapView2DBase::DrawGridLogical( CRender2D *pRender )
{
	CMapDoc *pDoc = GetMapDoc();

	if (pDoc == NULL)
		return;
	
	// Grid in logical view is always 1024
	int nGridSpacing = 1024;

	s_iCustomGridSpacing = nGridSpacing;
	s_bGridDots = false;

	int xAxis = 0;
	int yAxis = 1;
	int xMin = SnapToGrid( (int)max( g_MIN_MAP_COORD, m_ViewMin[xAxis]-nGridSpacing ), nGridSpacing );
	int xMax = SnapToGrid( (int)min( g_MAX_MAP_COORD, m_ViewMax[xAxis]+nGridSpacing ), nGridSpacing );
	int yMin = SnapToGrid( (int)max( g_MIN_MAP_COORD, m_ViewMin[yAxis]-nGridSpacing ), nGridSpacing );
	int yMax = SnapToGrid( (int)min( g_MAX_MAP_COORD, m_ViewMax[yAxis]+nGridSpacing ), nGridSpacing );
	
	Assert( xMin < xMax );
	Assert( yMin < yMax );

	// Draw the vertical grid lines.
	float depth = 0.0f;
	Vector vPointMin(depth,depth,depth);
	Vector vPointMax(depth,depth,depth);

	vPointMin[xAxis] = xMin;
	vPointMax[xAxis] = xMax;

	for (int y = yMin; y <= yMax; y += nGridSpacing )
	{
		pRender->SetDrawColor( m_clrGrid );

		HighlightGridLine( pRender, y );

		vPointMin[yAxis] = vPointMax[yAxis] = y;
		pRender->DrawLine( vPointMin, vPointMax );
	}

	vPointMin[yAxis] = yMin;
	vPointMax[yAxis] = yMax;

	for (int x = xMin; x <= xMax; x += nGridSpacing )
	{
		pRender->SetDrawColor( m_clrGrid );

		HighlightGridLine( pRender, x );

		vPointMin[xAxis] = vPointMax[xAxis] = x;
		pRender->DrawLine( vPointMin, vPointMax );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pointCheck - 
//			pointRef - 
//			nDist - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapView2DBase::CheckDistance(const Vector2D &vecCheck, const Vector2D &vecRef, int nDist)
{
	if ((fabs(vecRef.x - vecCheck.x) <= nDist) &&
		(fabs(vecRef.y - vecCheck.y) <= nDist))
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the center point of the view in world coordinates.
// Input  : pt - Receives the center point. Only dimensions initialized with
//				COORD_NOTINIT will be filled out.
//-----------------------------------------------------------------------------
void CMapView2DBase::GetCenterPoint(Vector &pt)
{
	Vector2D ptCenter( m_ClientWidth/2, m_ClientHeight/2); 
	Vector vCenter;

	ClientToWorld(vCenter, ptCenter );

	if (pt[axHorz] == COORD_NOTINIT)
	{
		pt[axHorz] = vCenter[axHorz];
	}

	if (pt[axVert] == COORD_NOTINIT)
	{
		pt[axVert] = vCenter[axVert];
	}
}


void CMapView2DBase::SetViewOrigin( float fHorz, float fVert, bool bRelative )
{
	Vector vCurPos;

	m_pCamera->GetViewPoint( vCurPos );
	

	if ( bRelative )
	{
		if ( fHorz == 0 && fVert == 0 ) 
			return;

		vCurPos[axHorz] += fHorz;
		vCurPos[axVert] += fVert;
	}
	else
	{
		if ( fHorz == vCurPos[axHorz] && fVert == vCurPos[axVert] ) 
			return;

		vCurPos[axHorz] = fHorz;
		vCurPos[axVert] = fVert;
	}

	if ( axThird == 1 )
	{
		vCurPos[axThird] = g_MIN_MAP_COORD;
	}
	else
	{
		vCurPos[axThird] = g_MAX_MAP_COORD;
	}

	m_pCamera->SetViewPoint( vCurPos );

	// Msg("SetViewOrigin: (%i,%i) %s (%i,%i) \n", x, y, bRelative?"rel":"abs", m_ptViewOrigin.x, m_ptViewOrigin.y );

	UpdateClientView();
}


//-----------------------------------------------------------------------------
// Purpose: Calculates all viewport related variables
//-----------------------------------------------------------------------------
void CMapView2DBase::UpdateClientView(void)
{
	if (!::IsWindow(m_hWnd))
		return;

	m_fZoom = m_pCamera->GetZoom();
	m_pCamera->GetViewPoint( m_vViewOrigin );

	CRect rectClient;
	GetClientRect( &rectClient );

	m_ClientWidth = rectClient.Width();
	m_ClientHeight = rectClient.Height();

	float viewWidth = (float)m_ClientWidth / m_fZoom;
 	float viewHeight = (float)m_ClientHeight / m_fZoom;

	m_fClientWidthHalf = (float)m_ClientWidth / 2;
	m_fClientHeightHalf = (float)m_ClientHeight / 2;

	float flMaxExtents = fabs(g_MIN_MAP_COORD) + fabs(g_MAX_MAP_COORD);
	m_flMinZoom = min(m_ClientWidth / flMaxExtents, m_ClientHeight / flMaxExtents);

	if ( Options.view2d.bScrollbars )
	{
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;

		si.nMin = g_MIN_MAP_COORD - m_fClientWidthHalf;
		si.nMax = g_MAX_MAP_COORD + m_fClientWidthHalf;
		si.nPage = viewWidth;
		si.nPos = m_vViewOrigin[axHorz];

		if ( bInvertHorz )
			si.nPos = -si.nPos;

		SetScrollInfo(SB_HORZ, &si);

		si.nMin = g_MIN_MAP_COORD-m_fClientHeightHalf;
		si.nMax = g_MAX_MAP_COORD+m_fClientHeightHalf;
		si.nPage = viewHeight;
		si.nPos = m_vViewOrigin[axVert];

		if ( bInvertVert )
			si.nPos = -si.nPos;

		SetScrollInfo(SB_VERT, &si);
	}
	else
	{
		ShowScrollBar(SB_HORZ, FALSE);
		ShowScrollBar(SB_VERT, FALSE);
	}

	// calc view axis
	m_vViewAxis.Init();
	m_vViewAxis[axThird] = 1;

	if ( bInvertHorz && bInvertVert )
		m_vViewAxis = -m_vViewAxis;

	m_pCamera->SetViewPort( m_ClientWidth, m_ClientHeight );
	m_pCamera->SetYaw( 0 );
	m_pCamera->SetPitch( 0 );
	m_pCamera->SetRoll( 0 );

   	switch ( axThird )
	{
   	case 0 :	m_pCamera->SetYaw( -90 );
				break;

	case 1 :	m_pCamera->SetRoll( 0 ); 
				break;

   	case 2 :	m_pCamera->SetPitch( 90 );
				break;
	}

	// update 3D world bounding box for 2D client view

	int xmin = 0;
	int xmax = m_ClientWidth;
	int ymin = 0;
	int ymax = m_ClientHeight;

	Vector2D ptViewMin(xmin, ymin);
	Vector2D ptViewMax(xmax, ymax);

	ClientToWorld( m_ViewMin, ptViewMin );
	ClientToWorld( m_ViewMax, ptViewMax );

	m_ViewMin[axThird] = g_MIN_MAP_COORD;
	m_ViewMax[axThird] = g_MAX_MAP_COORD;

	NormalizeBox( m_ViewMin, m_ViewMax );

	Assert( m_ViewMin.x <= m_ViewMax.x );
	Assert( m_ViewMin.y <= m_ViewMax.y );
	Assert( m_ViewMin.z <= m_ViewMax.z );

	OnRenderListDirty();
	m_bUpdateView = true;

	UpdateStatusBar();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2DBase::UpdateStatusBar()
{
	if(!IsWindow(m_hWnd))
		return;

	char szBuf[128];
	sprintf(szBuf, " Zoom: %.2f ", m_fZoom);
	SetStatusText(SBI_GRIDZOOM, szBuf);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fNewZoom - 
//-----------------------------------------------------------------------------
void CMapView2DBase::SetZoom(float fNewZoom)
{
	float fOldZoom = m_pCamera->GetZoom();

	fNewZoom = clamp( fNewZoom, m_flMinZoom, ZOOM_MAX );

	if (fOldZoom == fNewZoom)
	{
		return;
	}

	if (IsWindow(m_hWnd))
	{
		// zoom in on cursor position
		POINT ptClient;
		GetCursorPos(&ptClient);
		ScreenToClient(&ptClient);

		Vector2D newOrigin,vecClient(ptClient.x,ptClient.y);

		if (!PointInClientRect(vecClient))
		{
			// cursor is not in window; zoom on center instead
			vecClient.x = m_fClientWidthHalf;
			vecClient.y = m_fClientHeightHalf;
		}

		Vector vecWorld;
		ClientToWorld( vecWorld, vecClient );
		
		vecClient.x -= m_fClientWidthHalf;
		vecClient.y -= m_fClientHeightHalf;
		
		vecClient.x /= fNewZoom;
		vecClient.y /= fNewZoom;

		if (bInvertVert)
		{
			vecClient.y = -vecClient.y;
		}

		if (bInvertHorz)
		{
			vecClient.x = -vecClient.x;
		}

		newOrigin.x = vecWorld[axHorz] - vecClient.x;
		newOrigin.y = vecWorld[axVert] - vecClient.y;
	
		m_pCamera->SetZoom( fNewZoom );

		SetViewOrigin( newOrigin.x, newOrigin.y );

		UpdateClientView();
	}

	
	
}


#ifdef _DEBUG
void CMapView2DBase::AssertValid() const
{
	CView::AssertValid();
}

void CMapView2DBase::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cs - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapView2DBase::PreCreateWindow(CREATESTRUCT& cs) 
{
	static CString className;
	
	if(className.IsEmpty())
	{
		className = AfxRegisterWndClass(CS_BYTEALIGNCLIENT | CS_DBLCLKS, 
			AfxGetApp()->LoadStandardCursor(IDC_ARROW), HBRUSH(NULL));
	}

	cs.lpszClass = className;
	
	return CView::PreCreateWindow(cs);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nChar - 
//			nRepCnt - 
//			nFlags - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CMapDoc *pDoc = GetMapDoc();

	if ( !pDoc || !m_pToolManager )
		return;

	if (nChar == VK_SPACE)
	{
		// Switch the cursor to the hand. We'll start panning the view
		// on the left button down event.
		
		if ( m_bMouseDrag )
			SetCursor("Resource/ifm_grab.cur");
		else
			SetCursor("Resource/ifm_move.cur");

		return;
	}

	// Pass the message to the active tool.
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			if ( pTool->OnKeyDownLogical( static_cast<CMapViewLogical*>( this ), nChar, nRepCnt, nFlags ) )
				return;
		}
		else
		{
			if ( pTool->OnKeyDown2D( static_cast<CMapView2D*>( this ), nChar, nRepCnt, nFlags ) )
				return;
		}
	}

	// The tool didn't handle the key. Perform default handling for this view.
//	bool bShift = nFlags & MK_SHIFT;
	bool bCtrl = nFlags & MK_CONTROL;

	switch (nChar)
	{
		//
		// Zoom in.
		//
		case '+':
		case VK_ADD:
		{
			ZoomIn(bCtrl);
			break;
		}

		//
		// Zoom out.
		//
		case '-':
		case VK_SUBTRACT:
		{
			ZoomOut(bCtrl);
			break;
		}

		case VK_UP:
		{
			// scroll up
			OnVScroll(SB_LINEUP, 0, NULL);
			break;
		}

		case VK_DOWN:
		{
			// scroll down
			OnVScroll(SB_LINEDOWN, 0, NULL);
			break;
		}

		case VK_LEFT:
		{
			// scroll up
			OnHScroll(SB_LINELEFT, 0, NULL);
			break;
		}

		case VK_RIGHT:
		{
			// scroll up
			OnHScroll(SB_LINERIGHT, 0, NULL);
			break;
		}

		//
		// 1-9 +0 shortcuts to various zoom levels.
		//
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '0':
		{
			int iZoom = nChar - '1';
			if (nChar == '0')
			{
				iZoom = 9;
			}
			SetZoom(m_flMinZoom * (1 << iZoom));
			break;
		}
	}

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Per CWnd::OnKeyUp.
//-----------------------------------------------------------------------------
void CMapView2DBase::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if ( !m_pToolManager )
		return;

	if (nChar == VK_SPACE)
	{
		//
		// Releasing the space bar stops panning the view.
		//
		SetCursor( vgui::dc_arrow );
	}
	else
	{
		//
		// Pass the message to the active tool.
		//
		CBaseTool *pTool = m_pToolManager->GetActiveTool();
		if (pTool)
		{
			if ( IsLogical() )
			{
				if ( pTool->OnKeyUpLogical( static_cast<CMapViewLogical*>( this ), nChar, nRepCnt, nFlags ) )
					return;
			}
			else
			{
				if ( pTool->OnKeyUp2D( static_cast<CMapView2D*>( this ), nChar, nRepCnt, nFlags ) )
					return;
			}
		}
	}

	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if ( !m_pToolManager )
		return;

	// Pass the message to the active tool.
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if ( pTool )
	{
		if ( IsLogical() )
		{
			if ( pTool->OnCharLogical( static_cast<CMapViewLogical*>( this ), nChar, nRepCnt, nFlags ) )
				return;
		}
		else
		{
			if ( pTool->OnChar2D( static_cast<CMapView2D*>( this ), nChar, nRepCnt, nFlags ) )
				return;
		}
	}

	CView::OnChar( nChar, nRepCnt, nFlags );
}


//-----------------------------------------------------------------------------
// Hit test
//-----------------------------------------------------------------------------
bool CMapView2DBase::HitTest( const Vector2D &vPoint, const Vector& mins, const Vector& maxs)
{
	Vector2D vecMinClient,vecMaxClient;

	WorldToClient(vecMinClient, mins);
	WorldToClient(vecMaxClient, maxs);

	CRect rect(vecMinClient.x, vecMinClient.y, vecMaxClient.x, vecMaxClient.y);
	rect.NormalizeRect();

	return rect.PtInRect( CPoint( vPoint.x, vPoint.y) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : point - Point in client coordinates.
//			bMakeFirst - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapView2DBase::ObjectsAt( const Vector2D &vPoint, HitInfo_t *pHitData, int nMaxObjects, unsigned int nFlags )
{
	CMapDoc		*pDoc = GetMapDoc();
	CMapWorld	*pWorld = pDoc->GetMapWorld();

	return ObjectsAt( pWorld, vPoint, pHitData, nMaxObjects, nFlags );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : point - Point in client coordinates.
//			bMakeFirst - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CMapView2DBase::ObjectsAt( CMapWorld *pWorld, const Vector2D &vPoint, HitInfo_t *pHitData, int nMaxObjects, unsigned int nFlags )
{
	int nIndex = 0;

	const CMapObjectList *pChildren = pWorld->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass *pChild = pChildren->Element(pos);
		CMapWorld *pWorldChild = dynamic_cast< CMapWorld * >( pChild );

		if ( pWorldChild )
		{
			nIndex += ObjectsAt( pWorldChild, vPoint, &pHitData[ nIndex ], nMaxObjects - nIndex );
		}
		else if ( IsLogical() )
		{
			if ( pChild->HitTestLogical( static_cast<CMapViewLogical*>(this), vPoint, pHitData[nIndex] ) )
			{
				nIndex++;
			}
		}
		else
		{
			if ( pChild->HitTest2D( static_cast<CMapView2D*>(this), vPoint, pHitData[nIndex] ) )
			{
				nIndex++;
			}
		}
	}

	return nIndex;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if ( !m_pToolManager )
		return;

	// Check for view-specific keyboard overrides.
	
	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
	{
		//
		// Space bar + mouse move scrolls the view.
		//

		m_bMouseDrag = true;
		m_ptLDownClient = point;
		s_fDragRestX = s_fDragRestY = 0;

   		SetCapture();
		
		SetCursor( "Resource/ifm_grab.cur" );
		
		return;
	}

    //
	// Pass the message to the active tool.
    //
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			if ( pTool->OnLMouseDownLogical( static_cast<CMapViewLogical*>( this ), nFlags, Vector2D( point.x, point.y ) ) )
				return;
		}
		else
		{
			if ( pTool->OnLMouseDown2D( static_cast<CMapView2D*>( this ), nFlags, Vector2D( point.x, point.y ) ) )
				return;
		}
	}

	m_ptLDownClient = point;

	CView::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnMouseMove(UINT nFlags, CPoint point) 
{
	//
	// Make sure we are the active view.
	//		   

	CMapDoc *pDoc = GetMapDoc();

	if ( !pDoc || !m_pToolManager )
		return;

	if ( !IsActive() )
	{	 
		pDoc->SetActiveView(this);
	}

	//
	// If we are the active application, make sure this view has the input focus.
	//	
	if (APP()->IsActiveApp() && !IsRunningInEngine() )
	{
		if (GetFocus() != this)
		{
			SetFocus();
		}
	}

	//
	// Panning the view with the mouse, just exit.
	//
	if ( m_bMouseDrag )
	{
		if ( point == m_ptLDownClient )
			return;

		float fdx = point.x - m_ptLDownClient.x;
		float fdy = point.y - m_ptLDownClient.y;

		fdx /= m_fZoom;
		fdy /= m_fZoom;

		if ( bInvertHorz )
			fdy = -fdy;

		if ( bInvertVert )
			fdx = -fdx;

		fdx += s_fDragRestX;
		fdy += s_fDragRestY;

		int idx = fdx; 
		int idy = fdy; 

		if ( idy == 0  && idx == 0 )
			return;

		s_fDragRestX = fdx - idx;
		s_fDragRestY = fdy - idy;

		SetViewOrigin( idx, idy, true );

		SetCursor( "Resource/ifm_grab.cur" );

		// reset mouse pos
		m_ptLDownClient = point;
		return;
	}

	// Pass the message to the active tool.
    CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		Vector2D vPoint( point.x, point.y );
		if ( IsLogical() )
		{
			if ( pTool->OnMouseMoveLogical( static_cast<CMapViewLogical*>( this ), nFlags, vPoint ) )
				return;
		}
		else
		{
			if ( pTool->OnMouseMove2D( static_cast<CMapView2D*>( this ), nFlags, vPoint ) )
				return;
		}
	}

	//
	// The tool didn't handle the message. Make sure the cursor is set.
	//
	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
	{
		SetCursor( "Resource/ifm_move.cur" );
	}
	else
	{
		SetCursor( vgui::dc_arrow );
	}

	CView::OnMouseMove(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse wheel events. The mouse wheel is used to zoom the 2D
//			view in and out.
// Input  : Per CWnd::OnMouseWheel.
//-----------------------------------------------------------------------------
BOOL CMapView2DBase::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
    if ( !m_pToolManager )
		return TRUE;


	// Pass the message to the active tool.
    //
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			if ( pTool->OnMouseWheelLogical( static_cast<CMapViewLogical*>( this ), nFlags, zDelta, Vector2D(point.x,point.y) ) )
				return TRUE;
		}
		else
		{
			if ( pTool->OnMouseWheel2D( static_cast<CMapView2D*>( this ), nFlags, zDelta, Vector2D(point.x,point.y) ) )
				return TRUE;
		}
	}

	if (zDelta < 0)
	{
		ZoomOut(nFlags & MK_CONTROL);
	}
	else
	{
		ZoomIn(nFlags & MK_CONTROL);
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Scrolls the view to make sure that the position in world space is visible.
// Input  : vecPos - 
//-----------------------------------------------------------------------------
void CMapView2DBase::EnsureVisible(Vector &vecPos, float flMargin)
{
	Vector2D pt;
	WorldToClient(pt, vecPos);

	// check to see if it's in the client
	if (pt.x < 0)
	{
		pt.x = -pt.x + flMargin;
	}
	else if (pt.x > m_ClientWidth )
	{
		pt.x = m_ClientWidth - pt.x - flMargin;
	}
	else
	{
		pt.x = 0;
	}

	if (pt.y < 0)
	{
		pt.y = -pt.y + flMargin;
	}
	else if (pt.y > m_ClientHeight)
	{
		pt.y = m_ClientHeight - pt.y - flMargin;
	}
	else
	{
		pt.y = 0;
	}

	// if it's not in the client, scroll
	if (pt.x || pt.y)
	{
		SetViewOrigin( pt.x, pt.y, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CMapDoc *pDoc = GetMapDoc();

	if ( !pDoc || !m_pToolManager )
		return;

	ReleaseCapture();

	if ( m_bMouseDrag )
	{
		m_bMouseDrag = false;
   		// KillTimer(TIMER_MOUSEDRAG);
		OnMouseMove(nFlags, point);
		return;
	}

    //
	// Pass the message to the active tool.
    //
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			if ( pTool->OnLMouseUpLogical( static_cast<CMapViewLogical*>( this ), nFlags, Vector2D(point.x,point.y) ) )
				return;
		}
		else
		{
			if ( pTool->OnLMouseUp2D( static_cast<CMapView2D*>( this ), nFlags, Vector2D(point.x,point.y) ) )
				return;
		}
	}
	
	// we might have removed some stuff that was relevant:
	pDoc->UpdateStatusbar();
	
	CView::OnLButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button double click event.
//-----------------------------------------------------------------------------
void CMapView2DBase::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	//
	// Don't forward message if we are controlling the camera.
	//
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0)
		return;

	if ( !m_pToolManager )
		return;

	// Pass the message to the active tool.
	
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x, point.y );
		if ( IsLogical() )
		{
			pTool->OnLMouseDblClkLogical( static_cast<CMapViewLogical*>( this ), nFlags, vPoint );
			pTool->OnLMouseDownLogical( static_cast<CMapViewLogical*>( this ), nFlags, vPoint );
		}
		else
		{
			pTool->OnLMouseDblClk2D( static_cast<CMapView2D*>( this ), nFlags, vPoint );
			pTool->OnLMouseDown2D( static_cast<CMapView2D*>( this ), nFlags, vPoint );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bActivate - 
//			pActivateView - 
//			pDeactiveView - 
//-----------------------------------------------------------------------------
void CMapView2DBase::ActivateView(bool bActivate) 
{
	CMapView::ActivateView( bActivate );

	if ( bActivate )
	{
		CMapDoc *pDoc = GetMapDoc();
		CMapDoc::SetActiveMapDoc( pDoc );

		pDoc->UpdateTitle( this );
		UpdateStatusBar();
	}
	else
	{
		m_xScroll = m_yScroll = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2DBase::UpdateView( int nFlags ) 
{
	if ( nFlags & MAPVIEW_UPDATE_ONLY_3D )
		return;

	if ( IsLogical() )
	{
		if ( nFlags & MAPVIEW_UPDATE_ONLY_2D )
			return;
	}
	else
	{
		if ( nFlags & MAPVIEW_UPDATE_ONLY_LOGICAL )
			return;
	}

	if(nFlags & MAPVIEW_OPTIONS_CHANGED)
	{
		ShowScrollBar(SB_HORZ, Options.view2d.bScrollbars);
		ShowScrollBar(SB_VERT, Options.view2d.bScrollbars);
		SetColorMode(Options.view2d.bWhiteOnBlack);
		
		UpdateClientView();
	}

	// Render the world if the flag is specified.
	if ( nFlags & (MAPVIEW_UPDATE_OBJECTS|MAPVIEW_UPDATE_VISGROUP_STATE|MAPVIEW_UPDATE_VISGROUP_ALL) )
	{
		// rebuild render list since objects or visiblity was changed
		OnRenderListDirty();
	}

	if ( m_pwndTitle != NULL )
	{
		m_pwndTitle->Invalidate(false);
	}
	
	CMapView::UpdateView( nFlags );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt3 - 
//-----------------------------------------------------------------------------
void CMapView2DBase::CenterView(Vector *pCenter)
{
	CMapWorld *pWorld = GetMapDoc()->GetMapWorld();

	float fPointX, fPointY;

	if( pCenter )
	{
		// use provided point
		fPointX = (*pCenter)[axHorz];
		fPointY = (*pCenter)[axVert];
	}
	else
	{
		//
		// Use center of map.
		//
		Vector vecMins;
		Vector vecMaxs;
		pWorld->GetRender2DBox(vecMins, vecMaxs);

		fPointX = (vecMaxs[axHorz] + vecMins[axHorz]) / 2;
		fPointY = (vecMaxs[axVert] + vecMins[axVert]) / 2;
	}

	SetViewOrigin( fPointX, fPointY );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nSBCode - 
//			nPos - 
//			pScrollBar - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar) 
{
	int iPos = int(nPos);

	float viewWidth = (float)m_ClientWidth / m_fZoom;
		
	switch (nSBCode)
	{
		case SB_LINELEFT:
		{
			iPos = -int(viewWidth / 4);
			break;
		}
		case SB_LINERIGHT:
		{
			iPos = int(viewWidth / 4);
			break;
		}
		case SB_PAGELEFT:
		{
			iPos = -int(viewWidth / 2);
			break;
		}
		case SB_PAGERIGHT:
		{
			iPos = int(viewWidth / 2);
			break;
		}
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION:
		{
			if ( bInvertHorz )
				iPos = -iPos;

			SetViewOrigin( iPos, m_vViewOrigin[axVert] );
			return;
		}
	}

	if ( bInvertHorz )
		iPos = -iPos;

	SetViewOrigin( iPos, 0, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nSBCode - 
//			nPos - 
//			pScrollBar - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar) 
{
	int iPos = int(nPos);

	float viewHeight = (float)m_ClientHeight / m_fZoom;

	switch (nSBCode)
	{
		case SB_LINEUP:
		{
			iPos = -int(viewHeight / 4);
			break;
		}
		case SB_LINEDOWN:
		{
			iPos = int(viewHeight / 4);
			break;
		}
		case SB_PAGEUP:
		{
			iPos = -int(viewHeight / 2);
			break;
		}
		case SB_PAGEDOWN:
		{
			iPos = int(viewHeight / 2);
			break;
		}
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION:
		{
			if ( bInvertVert )
				iPos = -iPos;

			SetViewOrigin( m_vViewOrigin[axHorz], iPos );
			return;
		}
	}

	if ( bInvertVert )
		iPos = -iPos;

	SetViewOrigin( 0, iPos, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnRButtonDown(UINT nFlags, CPoint point) 
{
    // Pass the message to the active tool.

	if ( !m_pToolManager )
		return;
    
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			if ( pTool->OnRMouseDownLogical( static_cast<CMapViewLogical*>( this ), nFlags, Vector2D(point.x,point.y) ) )
				return;
		}
		else
		{
			if ( pTool->OnRMouseDown2D( static_cast<CMapView2D*>( this ), nFlags, Vector2D(point.x,point.y) ) )
				return;
		}
	}

	CView::OnRButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIDEvent - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnTimer(UINT nIDEvent) 
{
	if ( nIDEvent == TIMER_SCROLLVIEW )
	{
		KillTimer( TIMER_SCROLLVIEW );

		if (m_xScroll || m_yScroll)
		{
			SetViewOrigin(m_xScroll, m_yScroll, true);
			// force mousemove event
			CPoint pt;
			GetCursorPos(&pt);
			ScreenToClient(&pt);
			OnMouseMove(0, pt);
		}
	}

	CView::OnTimer(nIDEvent);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pWnd - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnContextMenu(UINT nFlags, const Vector2D &vPoint) 
{
	if ( m_bMouseDrag || !m_pToolManager )
	{
		return;
	}

    //
	// Pass the message to the active tool.
    //
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			if ( pTool->OnContextMenuLogical( static_cast<CMapViewLogical*>( this ), nFlags, vPoint ) )
				return;
		}
		else
		{
			if ( pTool->OnContextMenu2D( static_cast<CMapView2D*>( this ), nFlags, vPoint ) )
				return;
		}
	}

	static CMenu menu, menuDefault;
	static bool bInit = false;

	if(!bInit)
	{
		bInit = true;
		menu.LoadMenu(IDR_POPUPS);
		menuDefault.Attach(::GetSubMenu(menu.m_hMenu, 2));
	}

	if(!PointInClientRect( vPoint ) )
		return;

	CPoint ptScreen( vPoint.x, vPoint.y );
	ClientToScreen( &ptScreen );
	menuDefault.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, this);
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever the view is resized.
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnSize(UINT nType, int cx, int cy) 
{	   
	CView::OnSize(nType, cx, cy);

	UpdateClientView();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnEditProperties() 
{
	// kludge for trackpopupmenu()
	GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnRButtonUp(UINT nFlags, CPoint point) 
{
	if ( !m_pToolManager )
		return;

    // Pass the message to the active tool.
    
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( IsLogical() )
		{
			pTool->OnRMouseUpLogical( static_cast<CMapViewLogical*>( this ), nFlags, Vector2D(point.x,point.y) );
		}
		else
		{
			pTool->OnRMouseUp2D( static_cast<CMapView2D*>( this ), nFlags, Vector2D(point.x,point.y) );
		}
	}

	OnContextMenu( nFlags, Vector2D(point.x,point.y) );

	CView::OnRButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCmdUI - 
//-----------------------------------------------------------------------------
void CMapView2DBase::OnUpdateEditFunction(CCmdUI *pCmdUI)
{
	pCmdUI->Enable((m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL) &&
		            !GetMainWnd()->IsShellSessionActive());
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDC - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapView2DBase::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CMapView2DBase::WorldToClient(Vector2D &ptClient, const Vector &vecWorld)
{
	Assert(!bInvertHorz);
	Assert(bInvertVert);

	ptClient.x = (m_fZoom * ( vecWorld[axHorz] - m_vViewOrigin[axHorz] )) + m_fClientWidthHalf;
	ptClient.y = (m_fZoom * ( m_vViewOrigin[axVert] - vecWorld[axVert] )) + m_fClientHeightHalf;

/*	if (bInvertHorz)
	{
	ptClient.x = -ptClient.x;
	}

	if ( bInvertVert )
	{
	ptClient.y = -ptClient.y;
	}

	// Also valid:

	Vector2D vClient;

	m_pCamera->WorldToView( vecWorld, vClient );

	ptClient.x = vClient.x;
	ptClient.y = vClient.y; */
}

//-----------------------------------------------------------------------------
// Purpose: Converts a 2D client coordinate into 3D world coordinates.
// Input  : vecWorld - 
//			ptClient - 
//-----------------------------------------------------------------------------
void CMapView2DBase::ClientToWorld(Vector &vecWorld, const Vector2D &ptClient)
{
	vecWorld[axHorz] = ptClient.x - m_fClientWidthHalf;
	vecWorld[axVert] = ptClient.y - m_fClientHeightHalf;
	vecWorld[axThird] = 0;

	vecWorld[axHorz] /= m_fZoom;
	vecWorld[axVert] /= m_fZoom;

	if (bInvertHorz)
	{
		vecWorld[axHorz] = -vecWorld[axHorz];
	}

	if (bInvertVert)
	{
		vecWorld[axVert] = -vecWorld[axVert];
	}

	vecWorld += m_vViewOrigin;
}

void CMapView2DBase::BuildRay( const Vector2D &ptClient, Vector& vStart, Vector& vEnd )
{
	ClientToWorld( vStart, ptClient );
	vEnd = vStart;
	vStart[axThird] = -99999;
	vEnd[axThird] = 99999;
}

void CMapView2DBase::GetBestTransformPlane( Vector &horzAxis, Vector &vertAxis, Vector &thirdAxis)
{
	horzAxis.Init(); horzAxis[axHorz] = 1;
	vertAxis.Init(); vertAxis[axVert] = 1;
	thirdAxis.Init(); thirdAxis[axThird] = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Zooms the 2D view in.
// Input  : bAllViews - Whether to set all 2D views to this zoom level.
//-----------------------------------------------------------------------------
void CMapView2DBase::ZoomIn(BOOL bAllViews)
{
	float newZoom = m_fZoom * 1.2;
	SetZoom( newZoom );

	//
	// Set all doc 2d view zooms to this zoom level.
	//
	if (bAllViews)
	{
		VIEW2DINFO vi;
		vi.wFlags = VI_ZOOM;
		vi.fZoom = newZoom;

		CMapDoc *pDoc = GetMapDoc();
		if (pDoc != NULL)
		{
			pDoc->SetView2dInfo(vi);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Zooms the 2D view out.
// Input  : bAllViews - Whether to set all 2D views to this zoom level.
//-----------------------------------------------------------------------------
void CMapView2DBase::ZoomOut(BOOL bAllViews)
{
	SetZoom(m_fZoom / 1.2);

	//
	// Set all doc 2d view zooms to this zoom level.
	//
	if (bAllViews)
	{
		VIEW2DINFO vi;
		vi.wFlags = VI_ZOOM;
		vi.fZoom = m_fZoom;

		CMapDoc *pDoc = GetMapDoc();
		if (pDoc != NULL)
		{
			pDoc->SetView2dInfo(vi);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the entire 3D box is visible in this 2D view.
//-----------------------------------------------------------------------------
bool CMapView2DBase::IsBoxFullyVisible(const Vector &minsWorld, const Vector &maxsWorld)
{
	Vector2D minsClient;
	Vector2D maxsClient;
	WorldToClient(minsClient, minsWorld);
	WorldToClient(maxsClient, maxsWorld);

	return (PointInClientRect( minsClient ) &&
			PointInClientRect( maxsClient ) );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the entire 3D box is visible in this 2D view.
//-----------------------------------------------------------------------------
bool CMapView2DBase::CanBoxFitInView(const Vector &minsWorld, const Vector &maxsWorld)
{
	Vector2D minsClient;
	Vector2D maxsClient;
	WorldToClient(minsClient, minsWorld);
	WorldToClient(maxsClient, maxsWorld);

	return ((m_ClientWidth > maxsClient.x - minsClient.x) &&
			(m_ClientHeight > maxsClient.y - minsClient.y));
}

void CMapView2DBase::RenderView()
{
	DrawVGuiPanel();
	m_bUpdateView = false;
}

LRESULT CMapView2DBase::WindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_KEYDOWN: 
		case WM_SYSKEYDOWN:
		case WM_SYSCHAR:
		case WM_CHAR: 
		case WM_KEYUP: 
		case WM_SYSKEYUP: 
			{
				// don't invalidate window on these events, too much
				return CView::WindowProc( message, wParam, lParam ) ;
			}
		case WM_PAINT:
			{
				CWnd *focusWnd = GetForegroundWindow();

				if ( focusWnd && focusWnd->ContinueModal() )
				{
					// render the view now since were not running the main loop
                    RenderView();
				}
				else
				{
					// just flag view to be update with next main loop
					m_bUpdateView = true;
				}
							
				return CView::WindowProc( message, wParam, lParam ) ;
			}
	}

	if ( !WindowProcVGui( message, wParam, lParam ) )
	{
		return CView::WindowProc( message, wParam, lParam ) ;
	}

	return 1;
}

bool CMapView2DBase::IsInClientView( const Vector &vecMin, const Vector &vecMax )
{
	// check render view bounds in world space, dont translate every object
	if ( (vecMin.x > m_ViewMax.x) || (vecMax.x < m_ViewMin.x) )
		return false;

	if ( (vecMin.y > m_ViewMax.y) || (vecMax.y < m_ViewMin.y) )
		return false;

	if ( (vecMin.z > m_ViewMax.z) || (vecMax.z < m_ViewMin.z) )
		return false;

	return true;
}

bool CMapView2DBase::IsInClientView( const Vector2D &vecMin, const Vector2D &vecMax )
{
	// check render view bounds in world space, dont translate every object
	if ( (vecMin.x > m_ViewMax.x) || (vecMax.x < m_ViewMin.x) )
		return false;

	if ( (vecMin.y > m_ViewMax.y) || (vecMax.y < m_ViewMin.y) )
		return false;

	return true;
}

const Vector& CMapView2DBase::GetViewAxis()
{
	return m_vViewAxis;
}
