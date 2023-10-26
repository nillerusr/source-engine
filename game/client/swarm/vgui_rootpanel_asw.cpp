//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "vgui_controls/Panel.h"
#include "vgui/IVgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

class CPanelEffect;


// Serial under of effect, for safe lookup
typedef unsigned int EFFECT_HANDLE;

//-----------------------------------------------------------------------------
// Purpose: Sits between engine and client .dll panels
//  Responsible for drawing screen overlays
//-----------------------------------------------------------------------------
class C_ASWRootPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	C_ASWRootPanel( vgui::VPANEL parent, int slot );
	virtual				~C_ASWRootPanel( void );

	// Draw Panel effects here
	virtual void		PostChildPaint();

	// Clear list of Panel Effects
	virtual void		LevelInit( void );
	virtual void		LevelShutdown( void );

	// Run effects and let them decide whether to remove themselves
	void				OnTick( void );

	virtual void		PaintTraverse( bool Repaint, bool allowForce = true );

	virtual void		OnThink();
private:

	// Render all panel effects
	void		RenderPanelEffects( void );

	// List of current panel effects
	CUtlVector< CPanelEffect *> m_Effects;
	int			m_nSplitSlot;
};

//-----------------------------------------------------------------------------
// C_SDKRootPanel implementation.
//-----------------------------------------------------------------------------
C_ASWRootPanel::C_ASWRootPanel( vgui::VPANEL parent, int slot )
	: BaseClass( NULL, "SDK Root Panel" ), m_nSplitSlot( slot )
{
	SetParent( parent );
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	// This panel does post child painting
	SetPostChildPaintEnabled( true );

	// Make it screen sized
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );

	// Ask for OnTick messages
	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ASWRootPanel::~C_ASWRootPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASWRootPanel::PostChildPaint()
{
	BaseClass::PostChildPaint();

	// Draw all panel effects
	RenderPanelEffects();
}

//-----------------------------------------------------------------------------
// Purpose: For each panel effect, check if it wants to draw and draw it on
//  this panel/surface if so
//-----------------------------------------------------------------------------
void C_ASWRootPanel::RenderPanelEffects( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASWRootPanel::OnTick( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Reset effects on level load/shutdown
//-----------------------------------------------------------------------------
void C_ASWRootPanel::LevelInit( void )
{
}

void C_ASWRootPanel::LevelShutdown( void )
{
}

void C_ASWRootPanel::PaintTraverse( bool Repaint, bool allowForce /*= true*/ )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitSlot);
	BaseClass::PaintTraverse( Repaint, allowForce );
}

void C_ASWRootPanel::OnThink()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitSlot );
	BaseClass::OnThink();
}

static C_ASWRootPanel *g_pRootPanel[ MAX_SPLITSCREEN_PLAYERS ];
static C_ASWRootPanel *g_pFullscreenRootPanel;

void VGui_GetPanelList( CUtlVector< Panel * > &list )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		list.AddToTail( g_pRootPanel[ i ] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateClientDLLRootPanel( void )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		g_pRootPanel[ i ] = new C_ASWRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ), i );
	}

	g_pFullscreenRootPanel = new C_ASWRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ), 0 );
	g_pFullscreenRootPanel->SetZPos( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyClientDLLRootPanel( void )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		delete g_pRootPanel[ i ];
		g_pRootPanel[ i ] = NULL;
	}

	delete g_pFullscreenRootPanel;
	g_pFullscreenRootPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Game specific root panel
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetClientDLLRootPanel( void )
{
	Assert( engine->IsLocalPlayerResolvable() );
	return g_pRootPanel[ engine->GetActiveSplitScreenPlayerSlot() ]->GetVPanel();
}



//-----------------------------------------------------------------------------
// Purpose: Fullscreen root panel for shared hud elements during splitscreen
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *VGui_GetFullscreenRootPanel( void )
{
	return g_pFullscreenRootPanel;
}

vgui::VPANEL VGui_GetFullscreenRootVPANEL( void )
{
	return g_pFullscreenRootPanel->GetVPanel();
}
