//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_tf2rootpanel.h"
#include <vgui_controls/Controls.h>
#include <vgui/IVGui.h>
#include "paneleffect.h"
#include "itfhintitem.h"
#include "clientmode_commander.h"
#include "commanderoverlaypanel.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
C_TF2RootPanel::C_TF2RootPanel( vgui::VPANEL parent )
	: BaseClass( NULL, "TF2 Root Panel" )
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
C_TF2RootPanel::~C_TF2RootPanel( void )
{
	ClearAllEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TF2RootPanel::PostChildPaint()
{
	BaseClass::PostChildPaint();

	// Draw all panel effects
	RenderPanelEffects();
}

//-----------------------------------------------------------------------------
// Purpose: For each panel effect, check if it wants to draw and draw it on
//  this panel/surface if so
//-----------------------------------------------------------------------------
void C_TF2RootPanel::RenderPanelEffects( void )
{
	for ( int i = 0; i < m_Effects.Size(); i++ )
	{
		CPanelEffect *e = m_Effects[ i ];
		Assert( e );
		ITFHintItem *owner = e->GetOwner();
		if ( owner && !owner->ShouldRenderPanelEffects() )
			continue;
		if ( e->GetVisible() )
		{
			e->doPaint( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add effect to list
// Input  : *effect - 
//-----------------------------------------------------------------------------
EFFECT_HANDLE C_TF2RootPanel::AddEffect( CPanelEffect *effect )
{
	Assert( effect );

	m_Effects.AddToTail( effect );

	return effect->GetHandle();
}

//-----------------------------------------------------------------------------
// Purpose: Remove effect from list
// Input  : *effect - 
//-----------------------------------------------------------------------------
void C_TF2RootPanel::RemoveEffect( EFFECT_HANDLE handle )
{
	for ( int i = m_Effects.Size() - 1; i >= 0; i-- )
	{
		CPanelEffect *e = m_Effects[ i ];
		if ( e->GetHandle() == handle )
		{
			m_Effects.Remove( i );
			delete e;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find effect by handle
// Input  : handle - 
// Output : CPanelEffect
//-----------------------------------------------------------------------------
CPanelEffect *C_TF2RootPanel::FindEffect( EFFECT_HANDLE handle )
{
	for ( int i = 0; i < m_Effects.Size(); i++ )
	{
		CPanelEffect *e = m_Effects[ i ];
		if ( e->GetHandle() == handle )
		{
			return e;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Delete all effects
//-----------------------------------------------------------------------------
void C_TF2RootPanel::ClearAllEffects( void )
{
	while ( m_Effects.Size() > 0 )
	{
		CPanelEffect *e = m_Effects[ 0 ];
		m_Effects.Remove( 0 );
		delete e;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TF2RootPanel::OnTick( void )
{
	// Go backards
	for ( int i = m_Effects.Size() - 1; i >= 0; i-- )
	{
		CPanelEffect *e = m_Effects[ i ];
		Assert( e );

		// Allow panel to think
		e->Think();

		// See if panel should disappear
		if ( e->ShouldRemove() )
		{
			m_Effects.Remove( i );
			delete e;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset effects on level load/shutdown
//-----------------------------------------------------------------------------
void C_TF2RootPanel::LevelInit( void )
{
	ClearAllEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TF2RootPanel::LevelShutdown( void )
{
	ClearAllEffects();
}

//-----------------------------------------------------------------------------
// Purpose: Delete any panel effects owned by owner
// Input  : *owner - 
//-----------------------------------------------------------------------------
void C_TF2RootPanel::DestroyPanelEffects( ITFHintItem *owner )
{
	for ( int i = m_Effects.Size() - 1; i >= 0; i-- )
	{
		CPanelEffect *e = m_Effects[ i ];
		if ( e->GetOwner() == owner )
		{
			m_Effects.Remove( i );
			delete e;
		}
	}
}

