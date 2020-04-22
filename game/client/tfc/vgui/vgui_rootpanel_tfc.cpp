//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "vgui_rootpanel_tfc.h"
#include "vgui/IVGui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_TFCRootPanel *g_pRootPanel = NULL;


//-----------------------------------------------------------------------------
// Global functions.
//-----------------------------------------------------------------------------
void VGUI_CreateClientDLLRootPanel( void )
{
	g_pRootPanel = new C_TFCRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
}

void VGUI_DestroyClientDLLRootPanel( void )
{
	delete g_pRootPanel;
	g_pRootPanel = NULL;
}

vgui::VPANEL VGui_GetClientDLLRootPanel( void )
{
	return g_pRootPanel->GetVPanel();
}


//-----------------------------------------------------------------------------
// C_TFCRootPanel implementation.
//-----------------------------------------------------------------------------
C_TFCRootPanel::C_TFCRootPanel( vgui::VPANEL parent )
	: BaseClass( NULL, "TFC Root Panel" )
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
C_TFCRootPanel::~C_TFCRootPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFCRootPanel::PostChildPaint()
{
	BaseClass::PostChildPaint();

	// Draw all panel effects
	RenderPanelEffects();
}

//-----------------------------------------------------------------------------
// Purpose: For each panel effect, check if it wants to draw and draw it on
//  this panel/surface if so
//-----------------------------------------------------------------------------
void C_TFCRootPanel::RenderPanelEffects( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFCRootPanel::OnTick( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Reset effects on level load/shutdown
//-----------------------------------------------------------------------------
void C_TFCRootPanel::LevelInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFCRootPanel::LevelShutdown( void )
{
}

