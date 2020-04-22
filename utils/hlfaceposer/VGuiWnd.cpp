//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "vguiwnd.h"
#include <vgui_controls/EditablePanel.h>
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "FacePoser_VGui.h"
// #include "material.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"

#define REPAINT_TIMER_ID	1042 //random value, hopfully no collisions	

inline MaterialSystem_Config_t& MaterialSystemConfig()
{
	extern MaterialSystem_Config_t g_materialSystemConfig;
	return g_materialSystemConfig;
}

inline IMaterialSystemHardwareConfig* MaterialSystemHardwareConfig()
{
	extern IMaterialSystemHardwareConfig* g_pMaterialSystemHardwareConfig;
	return g_pMaterialSystemHardwareConfig;
}

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

int CVGuiPanelWnd::handleEvent( mxEvent *event )
{
	if ( !HandeEventVGui( event ) )
	{
		return BaseClass::handleEvent( event );
	}

	return 1;
}

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
	if ( FaceposerVGui()->HasFocus( this )	)
	{
		FaceposerVGui()->SetFocus( NULL );
	}

	if ( m_hVGuiContext != vgui::DEFAULT_VGUI_CONTEXT )
	{
		vgui::ivgui()->DestroyContext( m_hVGuiContext );
		m_hVGuiContext = vgui::DEFAULT_VGUI_CONTEXT;
	}

	// kill the timer if any
	::KillTimer( (HWND)m_pParentWnd->getHandle(), REPAINT_TIMER_ID );


	if ( m_pMainPanel )
		m_pMainPanel->MarkForDeletion();
}

void CVGuiWnd::SetParentWindow(mxWindow *pParent)
{
	m_pParentWnd = pParent;

	/*
	m_pParentWnd->EnableWindow( true );
	m_pParentWnd->SetFocus();
	*/

	HWND h = (HWND)m_pParentWnd->getHandle();
	EnableWindow( h, TRUE );
	SetFocus( h );
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
	SetRepaintInterval( 75 );

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

mxWindow *CVGuiWnd::GetParentWnd()
{
	return m_pParentWnd;
}

void CVGuiWnd::SetRepaintInterval( int msecs )
{
	m_pParentWnd->setTimer( msecs );
}

void CVGuiWnd::DrawVGuiPanel()
{
	if ( !m_pMainPanel || !m_pParentWnd || m_bIsDrawing )
		return;


	m_bIsDrawing = true; // avoid recursion

	HWND hWnd = (HWND)m_pParentWnd->getHandle();

	int w,h;
	RECT rect; 
	::GetClientRect(hWnd, &rect);
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	g_pMaterialSystem->SetView( hWnd );

	pRenderContext->Viewport( rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top );

	pRenderContext->ClearColor4ub( m_ClearColor.r(), m_ClearColor.g(), m_ClearColor.b(), m_ClearColor.a() );

	pRenderContext->ClearBuffers( true, m_bClearZBuffer );

	g_pMaterialSystem->BeginFrame( 0 );

	// draw from the main panel down

	m_pMainPanel->GetSize( w , h );

	if ( w != rect.right || h != rect.bottom )
	{
		m_pMainPanel->SetBounds( 2 + rect.left, 2 + rect.top, rect.right - rect.left - 4, rect.bottom - rect.top - 4 );
		m_pMainPanel->Repaint();
	}

	FaceposerVGui()->Simulate(); 

	vgui::surface()->PaintTraverseEx( m_pMainPanel->GetVPanel(), true );

	g_pMaterialSystem->EndFrame();

	g_pMaterialSystem->SwapBuffers();

	m_bIsDrawing = false;
}

CVGuiPanelWnd::CVGuiPanelWnd( mxWindow *parent, int x, int y, int w, int h )
: BaseClass( parent, x, y, w, h )
{
}

void CVGuiPanelWnd::redraw()
{
	DrawVGuiPanel();
}

int CVGuiWnd::HandeEventVGui( mxEvent *event )
{
	if ( !m_pParentWnd )
		return 0;

	HWND hWnd = (HWND)m_pParentWnd->getHandle();

//	switch( uMsg )
//	{

//	case WM_GETDLGCODE :
//		{
//			// forward all keyboard into to vgui panel
//			return DLGC_WANTALLKEYS|DLGC_WANTCHARS;	
//		}

//	case WM_PAINT :
//		{
//			// draw the VGUI panel now
//			DrawVGuiPanel();
//			break;
//		}

//	case WM_TIMER :
//		{
//			if ( wParam == REPAINT_TIMER_ID )
//			{
//				m_pParentWnd->Invalidate();
//			}
//			break;
//		}

//	case WM_SETCURSOR:
//		return 1; // don't pass WM_SETCURSOR

/*
	case WM_LBUTTONDOWN: 
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_MOUSEMOVE: 
		{
			// switch vgui focus to this panel
			FaceposerVGui()->SetFocus( this );

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
*/

	switch ( event->event )
	{
	case mxEvent::KeyUp:
	case mxEvent::KeyDown:
	case mxEvent::MouseUp:
	case mxEvent::MouseWheeled:
	case mxEvent::Char:
	case mxEvent::MouseMove:
		{
			InvalidateRect( hWnd, NULL, FALSE );
		}
		break;

	case mxEvent::Timer:
		{
			InvalidateRect( hWnd, NULL, FALSE );
		}
		break;
	case mxEvent::Focus:
		break;
	case mxEvent::MouseDown:
		{
			// switch vgui focus to this panel
			FaceposerVGui()->SetFocus( this );

			// request keyboard focus too on mouse down
			if ( event->event != mxEvent::MouseMove )
			{
				InvalidateRect( hWnd, NULL, FALSE );
				SetFocus( hWnd );
			}
		}
		break;
	}

	return 0;
}