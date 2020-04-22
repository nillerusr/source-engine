//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "stdafx.h"
#include "vguiwnd.h"
#include <vgui_controls/EditablePanel.h>
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "HammerVGui.h"
#include "material.h"
#include "istudiorender.h"
#include "hammer.h"


IMPLEMENT_DYNCREATE(CVGuiPanelWnd, CWnd)

#define REPAINT_TIMER_ID	1042 //random value, hopfully no collisions	

class CBaseMainPanel : public vgui::EditablePanel
{
public:

	CBaseMainPanel(Panel *parent, const char *panelName) : vgui::EditablePanel( parent, panelName ) {};

	virtual	void OnSizeChanged(int newWide, int newTall)
	{
		// call Panel and not EditablePanel OnSizeChanged.
		Panel::OnSizeChanged(newWide, newTall);
	}
};

LRESULT CVGuiPanelWnd::WindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	if ( !WindowProcVGui( message, wParam, lParam ) )
	{
		return CWnd::WindowProc( message, wParam, lParam ) ;
	}

	return 1;
}

BEGIN_MESSAGE_MAP(CVGuiPanelWnd, CWnd)
END_MESSAGE_MAP()

CVGuiWnd::CVGuiWnd(void)
{
	m_pMainPanel = NULL;
	m_pParentWnd = NULL;
	m_hVGuiContext = vgui::DEFAULT_VGUI_CONTEXT;
	m_bIsDrawing = false;
	m_ClearColor.SetColor( 0,0,0,255 );
	m_bClearZBuffer = true;
}

CVGuiWnd::~CVGuiWnd(void)
{
	if ( HammerVGui()->HasFocus( this )	)
	{
		HammerVGui()->SetFocus( NULL );
	}

	if ( m_hVGuiContext != vgui::DEFAULT_VGUI_CONTEXT )
	{
		vgui::ivgui()->DestroyContext( m_hVGuiContext );
		m_hVGuiContext = vgui::DEFAULT_VGUI_CONTEXT;
	}

	// kill the timer if any
	::KillTimer( m_pParentWnd->GetSafeHwnd(), REPAINT_TIMER_ID );


	if ( m_pMainPanel )
		m_pMainPanel->MarkForDeletion();
}

void CVGuiWnd::SetParentWindow(CWnd *pParent)
{
	m_pParentWnd = pParent;

	m_pParentWnd->EnableWindow( true );
	m_pParentWnd->SetFocus();
}

int	CVGuiWnd::GetVGuiContext()
{
	return m_hVGuiContext;
}

void CVGuiWnd::SetCursor(vgui::HCursor cursor)
{
	if ( m_pMainPanel )
	{
		m_pMainPanel->SetCursor( cursor );
	}
}

void CVGuiWnd::SetCursor(const char *filename)
{
	vgui::HCursor hCursor = vgui::surface()->CreateCursorFromFile( filename );
	m_pMainPanel->SetCursor( hCursor );
}

void CVGuiWnd::SetMainPanel( vgui::EditablePanel * pPanel )
{
	Assert( m_pMainPanel == NULL );
	Assert( m_hVGuiContext == vgui::DEFAULT_VGUI_CONTEXT );

	m_pMainPanel = pPanel;
	
	m_pMainPanel->SetParent( vgui::surface()->GetEmbeddedPanel() );
	m_pMainPanel->SetVisible( true );
	m_pMainPanel->SetPaintBackgroundEnabled( false );
	m_pMainPanel->SetCursor( vgui::dc_arrow );
	
	m_hVGuiContext = vgui::ivgui()->CreateContext();
	vgui::ivgui()->AssociatePanelWithContext( m_hVGuiContext, m_pMainPanel->GetVPanel() );
}

vgui::EditablePanel *CVGuiWnd::CreateDefaultPanel()
{
	return new CBaseMainPanel( NULL, "mainpanel" );
}

vgui::EditablePanel	*CVGuiWnd::GetMainPanel()
{
	return m_pMainPanel;
}

CWnd *CVGuiWnd::GetParentWnd()
{
	return m_pParentWnd;
}

void CVGuiWnd::SetRepaintInterval( int msecs )
{
	::SetTimer( m_pParentWnd->GetSafeHwnd(), REPAINT_TIMER_ID, msecs, NULL );
}

void CVGuiWnd::DrawVGuiPanel()
{
	if ( !m_pMainPanel || !m_pParentWnd || m_bIsDrawing )
		return;

	m_bIsDrawing = true; // avoid recursion
	
	HWND hWnd = m_pParentWnd->GetSafeHwnd();

	int w,h;
	RECT rect; ::GetClientRect(hWnd, &rect);
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );

	MaterialSystemInterface()->SetView( hWnd );

	pRenderContext->Viewport( 0, 0, rect.right, rect.bottom );

	pRenderContext->ClearColor4ub( m_ClearColor.r(), m_ClearColor.g(), m_ClearColor.b(), m_ClearColor.a() );

	pRenderContext->ClearBuffers( true, m_bClearZBuffer );
	
	MaterialSystemInterface()->BeginFrame( 0 );
	g_pStudioRender->BeginFrame();

	// draw from the main panel down

	m_pMainPanel->GetSize( w , h );

	if ( w != rect.right || h != rect.bottom )
	{
		m_pMainPanel->SetBounds(0, 0, rect.right, rect.bottom );
		m_pMainPanel->Repaint();
	}

	HammerVGui()->Simulate(); 

	vgui::surface()->PaintTraverseEx( m_pMainPanel->GetVPanel(), true );
	
	g_pStudioRender->EndFrame();
	MaterialSystemInterface()->EndFrame();

	MaterialSystemInterface()->SwapBuffers();

	m_bIsDrawing = false;
}

LRESULT CVGuiWnd::WindowProcVGui( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{

	case WM_GETDLGCODE :
		{
			// forward all keyboard into to vgui panel
			return DLGC_WANTALLKEYS|DLGC_WANTCHARS;	
		}

	case WM_PAINT :
		{
			// draw the VGUI panel now
			DrawVGuiPanel();
			break;
		}

	case WM_TIMER :
		{
			if ( wParam == REPAINT_TIMER_ID )
			{
				m_pParentWnd->Invalidate();
			}
			break;
		}

	case WM_SETCURSOR:
		return 1; // don't pass WM_SETCURSOR

	case WM_LBUTTONDOWN: 
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_MOUSEMOVE: 
        {
			// switch vgui focus to this panel
			HammerVGui()->SetFocus( this );
						
			// request keyboard focus too on mouse down
			if ( uMsg != WM_MOUSEMOVE)
			{
				m_pParentWnd->Invalidate();
				m_pParentWnd->SetFocus();
			}
			break;
		}
	case WM_KILLFOCUS:
		{
			// restore normal arrow cursor when mouse leaves VGUI panel
			SetCursor( vgui::dc_arrow );
			break;
		}

	case WM_LBUTTONUP: 
	case WM_RBUTTONUP: 
	case WM_MBUTTONUP: 
	case WM_LBUTTONDBLCLK: 
	case WM_RBUTTONDBLCLK: 
	case WM_MBUTTONDBLCLK: 
	case WM_MOUSEWHEEL:
	case WM_KEYDOWN: 
	case WM_SYSKEYDOWN:
	case WM_SYSCHAR:
	case WM_CHAR: 
	case WM_KEYUP: 
	case WM_SYSKEYUP: 
		{
			// redraw window
			m_pParentWnd->Invalidate();
			break;
		}
	}
	return 0;
}