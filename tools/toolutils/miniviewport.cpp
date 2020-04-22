//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "toolutils/miniviewport.h"
#include "tier1/utlstring.h"
#include "vgui/ISurface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "tier1/KeyValues.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "view_shared.h"
#include "texture_group_names.h"
#include "vgui_controls/PropertySheet.h"
#include "tier2/tier2.h"
#include <windows.h>	// for MultiByteToWideChar
#include "cdll_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CMiniViewportEngineRenderArea;

using namespace vgui;

extern IMatSystemSurface *g_pMatSystemSurface;

#define DEFAULT_PREVIEW_WIDTH 1280

//-----------------------------------------------------------------------------
// Purpose: This is a "frame" which is used to position the engine
//-----------------------------------------------------------------------------
class CMiniViewportPropertyPage : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMiniViewportPropertyPage, vgui::EditablePanel );

public:
	CMiniViewportPropertyPage( Panel *parent, const char *panelName );

	virtual Color GetBgColor();

	void	GetEngineBounds( int& x, int& y, int& w, int& h );

	void	RenderFrameBegin();

	CMiniViewportEngineRenderArea *GetViewportArea() { return m_pViewportArea; }

private:
	virtual void PerformLayout();

	Color	m_bgColor;

	CMiniViewportEngineRenderArea				*m_pViewportArea;
};

//-----------------------------------------------------------------------------
//
// the actual renderable area
//
//-----------------------------------------------------------------------------
class CMiniViewportEngineRenderArea : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMiniViewportEngineRenderArea, vgui::EditablePanel );

public:
	CMiniViewportEngineRenderArea( Panel *parent, const char *panelName );
	~CMiniViewportEngineRenderArea();

	virtual void PaintBackground();
	virtual void GetEngineBounds( int& x, int& y, int& w, int& h );
	virtual void ApplySchemeSettings( IScheme *pScheme );

	void	RenderFrameBegin();
	void	SetOverlayText( const char *pText );

	// Called when the layoff texture needs to be released
	void ReleaseLayoffTexture();

protected:
	void			InitSceneMaterials();
	void			ShutdownSceneMaterials();

	// Paints the black borders around the engine window
	void PaintEngineBorders( int x, int y, int w, int h );

	// Paints the engine window	itself
	void PaintEngineWindow( int x, int y, int w, int h );

	// Paints the overlay text
	void PaintOverlayText( );

	int m_nEngineOutputTexture;
	vgui::HFont	m_OverlayTextFont;
	CUtlString m_OverlayText;

	CTextureReference			m_ScreenBuffer;	
	CMaterialReference			m_ScreenMaterial;
};


CMiniViewportEngineRenderArea::CMiniViewportEngineRenderArea( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( true );

	m_nEngineOutputTexture = vgui::surface()->CreateNewTextureID();
}

CMiniViewportEngineRenderArea::~CMiniViewportEngineRenderArea()
{
	ShutdownSceneMaterials();
}

void CMiniViewportEngineRenderArea::RenderFrameBegin()
{
	if ( !enginetools->IsInGame() )
		return;

	InitSceneMaterials();

	CViewSetup playerViewSetup;
	int x, y, w, h;
	GetEngineBounds( x, y, w, h );
	enginetools->GetPlayerView( playerViewSetup, 0, 0, w, h );

	// NOTE: This is a workaround to a nasty problem. Vgui uses stencil
	// to determing if the panels should occlude each other. The engine
	// has now started to use stencil for various random effects.
	// To prevent these different stencil uses from clashing, we will
	// render the engine prior to vgui painting + cache the result off in
	// 
	// Make the engine draw the scene
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->PushRenderTargetAndViewport( m_ScreenBuffer, 0, 0, w, h );

	// Tell the engine to tell the client to render the view (sans viewmodel)
	enginetools->SetMainView( playerViewSetup.origin, playerViewSetup.angles );
	enginetools->RenderView( playerViewSetup, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, RENDERVIEW_DRAWHUD | RENDERVIEW_DRAWVIEWMODEL );

	// Pop the target
	pRenderContext->PopRenderTargetAndViewport();
}

void CMiniViewportEngineRenderArea::InitSceneMaterials()
{
	if ( m_ScreenBuffer )
		return;

	if ( g_pMaterialSystem->IsTextureLoaded( "_rt_LayoffResult"	) ) 
	{
		ITexture *pTexture = g_pMaterialSystem->FindTexture( "_rt_LayoffResult", TEXTURE_GROUP_RENDER_TARGET );
		m_ScreenBuffer.Init( pTexture );
	}
	else
	{
		// For now, layoff dimensions match aspect of back buffer
		int nBackBufferWidth, nBackBufferHeight;
		g_pMaterialSystem->GetBackBufferDimensions( nBackBufferWidth, nBackBufferHeight );
		float flAspect = nBackBufferWidth / (float)nBackBufferHeight;
		int nPreviewWidth = min( DEFAULT_PREVIEW_WIDTH, nBackBufferWidth );
		int nPreviewHeight = ( int )( nPreviewWidth / flAspect + 0.5f );

		g_pMaterialSystem->BeginRenderTargetAllocation();								// Begin allocating RTs which IFM can scribble into

		// LDR final result of either HDR or LDR rendering
		m_ScreenBuffer.Init( g_pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			"_rt_LayoffResult", nPreviewWidth, nPreviewHeight, RT_SIZE_OFFSCREEN,
			g_pMaterialSystem->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_BORDER ) );

		g_pMaterialSystem->EndRenderTargetAllocation();									// End allocating RTs which IFM can scribble into
	}

	KeyValues *pVMTKeyValues = NULL;
	pVMTKeyValues= new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", m_ScreenBuffer->GetName() );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	m_ScreenMaterial.Init( "MiniViewportEngineRenderAreaSceneMaterial", pVMTKeyValues );
	m_ScreenMaterial->Refresh();
}


//-----------------------------------------------------------------------------
// Called when the layoff texture needs to be released
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::ReleaseLayoffTexture()
{
	m_ScreenBuffer.Shutdown();
	m_ScreenMaterial.Shutdown();
}


//-----------------------------------------------------------------------------
// Apply scheme settings
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::ApplySchemeSettings( IScheme *pScheme )
{   
	BaseClass::ApplySchemeSettings( pScheme );
	m_OverlayTextFont = pScheme->GetFont( "DefaultLargeOutline" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::ShutdownSceneMaterials()
{
	m_ScreenBuffer.Shutdown();
	m_ScreenMaterial.Shutdown();
}


//-----------------------------------------------------------------------------
// Sets text to draw over the window
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::SetOverlayText( const char *pText )
{
	m_OverlayText = pText;
}

	
//-----------------------------------------------------------------------------
// Paints the black borders around the engine window
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::PaintEngineBorders( int x, int y, int w, int h )
{
	// Draws black borders around the engine window
	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	if ( x != 0 )
	{
		surface()->DrawFilledRect( 0, 0, x, h );
		surface()->DrawFilledRect( x + w, 0, w + 2 * x, h );
	}
	else if ( y != 0 )
	{
		surface()->DrawFilledRect( 0, 0, w, y );
		surface()->DrawFilledRect( 0, y + h, w, h + 2 * y );
	}
}


//-----------------------------------------------------------------------------
// Paints the overlay text
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::PaintOverlayText( )
{
	if ( !m_OverlayText.Length() )
		return;

	int cw, ch;
	GetSize( cw, ch );

	int nTextWidth, nTextHeight;
	int nBufLen = m_OverlayText.Length()+1;
	wchar_t *pTemp = (wchar_t*)_alloca( nBufLen * sizeof(wchar_t) );
	::MultiByteToWideChar( CP_UTF8, 0, m_OverlayText.Get(), -1, pTemp, nBufLen );

	g_pMatSystemSurface->GetTextSize( m_OverlayTextFont, pTemp, nTextWidth, nTextHeight );
	int lx = (cw - nTextWidth) / 2;
	if ( lx < 10 )
	{
		lx = 10;
	}
	int ly = ch - 10 - nTextHeight;
	g_pMatSystemSurface->DrawColoredTextRect( m_OverlayTextFont, 
		lx, ly, cw - lx, ch - ly,
		255, 255, 255, 255, "%s", m_OverlayText.Get() );
}


//-----------------------------------------------------------------------------
// Paints the engine window	itself
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::PaintEngineWindow( int x, int y, int w, int h )
{
	if ( !enginetools->IsInGame() )
	{
		surface()->DrawSetColor( Color( 127, 127, 200, 63 ) );
		surface()->DrawFilledRect( x, y, x + w, y + h );
	}
	else
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

		g_pMatSystemSurface->DrawSetTextureMaterial( m_nEngineOutputTexture, m_ScreenMaterial );
		surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );

		int nTexWidth = m_ScreenBuffer->GetActualWidth();
		int nTexHeight = m_ScreenBuffer->GetActualHeight();
		float flOOWidth = 1.0f / nTexWidth;
		float flOOHeight = 1.0f / nTexHeight;

		float s0, s1, t0, t1;

		s0 = ( 0.5f ) * flOOWidth;
		t0 = ( 0.5f ) * flOOHeight;
		s1 = ( (float)w - 0.5f ) * flOOWidth;
		t1 = ( (float)h - 0.5f ) * flOOHeight;

		vgui::surface()->DrawTexturedSubRect( x, y, x+w, y+h, s0, t0, s1, t1 );

		PaintOverlayText();
	}
}

//-----------------------------------------------------------------------------
// Paints the background
//-----------------------------------------------------------------------------
void CMiniViewportEngineRenderArea::PaintBackground()
{
	int x, y, w, h;
	GetEngineBounds( x, y, w, h );
	PaintEngineBorders( x, y, w, h );
	PaintEngineWindow( x, y, w, h );
}

void CMiniViewportEngineRenderArea::GetEngineBounds( int& x, int& y, int& w, int& h )
{
	x = 0;
	y = 0;
	GetSize( w, h );

	// Check aspect ratio
	int sx, sy;
	surface()->GetScreenSize( sx, sy );

	if ( sy > 0 && 
		h > 0 )
	{
		float screenaspect = (float)sx / (float)sy;
		float aspect = (float)w / (float)h;

		float ratio = screenaspect / aspect;

		// Screen is wider, need bars at top and bottom
		if ( ratio > 1.0f )
		{
			int usetall = (float)w / screenaspect;
			y = ( h - usetall ) / 2;
			h = usetall;
		}
		// Screen is narrower, need bars at left/right
		else
		{
			int usewide = (float)h * screenaspect;
			x = ( w - usewide ) / 2;
			w = usewide;
		}
	}
}

CMiniViewportPropertyPage::CMiniViewportPropertyPage(Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName )
{
	m_bgColor = Color( 0, 0, 0, 0 );

	m_pViewportArea = new CMiniViewportEngineRenderArea( this, "Engine" );
}

void CMiniViewportPropertyPage::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );
	m_pViewportArea->SetBounds( 0, 0, w, h );
}

Color CMiniViewportPropertyPage::GetBgColor()
{
	return m_bgColor;
}


void CMiniViewportPropertyPage::GetEngineBounds( int& x, int& y, int& w, int& h )
{
	m_pViewportArea->GetEngineBounds( x, y, w, h );
	m_pViewportArea->LocalToScreen( x, y );
}

void CMiniViewportPropertyPage::RenderFrameBegin()
{
	m_pViewportArea->RenderFrameBegin();
}

CMiniViewport::CMiniViewport( vgui::Panel *parent, bool contextLabel, vgui::IToolWindowFactory *factory /*= 0*/, 
	vgui::Panel *page /*= NULL*/, char const *title /*= NULL*/, bool contextMenu /*= false*/ ) :
	BaseClass( parent, contextLabel, factory, page, title, contextMenu, false )
{
	SetCloseButtonVisible( false );

	GetPropertySheet()->SetDraggableTabs( false );

	// Add the viewport panel
	m_hPage = new CMiniViewportPropertyPage( this, "ViewportPage" );

	AddPage( m_hPage.Get(), "#ToolMiniViewport", false );
}

void CMiniViewport::GetViewport( bool& enabled, int& x, int& y, int& w, int& h )
{
	enabled = false;
	x = y = w = h = 0;

	int screenw, screenh;
	surface()->GetScreenSize( screenw, screenh );

	m_hPage->GetEngineBounds( x, y, w, h );

	y = screenh - ( y + h );
}

void CMiniViewport::GetEngineBounds( int& x, int& y, int& w, int& h )
{
	m_hPage->GetEngineBounds( x, y, w, h );
}


//-----------------------------------------------------------------------------
// Called when the layoff texture needs to be released
//-----------------------------------------------------------------------------
void CMiniViewport::ReleaseLayoffTexture()
{
	if ( m_hPage.Get() )
	{
		m_hPage->GetViewportArea()->ReleaseLayoffTexture();
	}
}


//-----------------------------------------------------------------------------
// Sets text to draw over the window
//-----------------------------------------------------------------------------
void CMiniViewport::SetOverlayText( const char *pText )
{
	if ( m_hPage.Get() )
	{
		m_hPage->GetViewportArea()->SetOverlayText( pText );
	}
}

void CMiniViewport::RenderFrameBegin()
{
	if ( m_hPage.Get() )
	{
		m_hPage->RenderFrameBegin();
	}
}
