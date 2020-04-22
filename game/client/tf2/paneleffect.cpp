//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "PanelEffect.h"
#include "vgui_int.h"
#include <vgui_controls/Panel.h>
#include <vgui/IPanel.h>

void PanelToPanelRectangle( int& x, int& y, int& w, int& h, vgui::Panel *output, vgui::Panel *input );

// Global panel counter for effects
EFFECT_HANDLE CPanelEffect::m_nHandleCount = 0;

//-----------------------------------------------------------------------------
// Purpose: Base panel effect
// Input  : *owner - 
//-----------------------------------------------------------------------------
CPanelEffect::CPanelEffect( ITFHintItem *owner )
	: m_pOwner( owner )
{
	// Assign it a unique handle index
	m_Handle		= ++m_nHandleCount;

	SetType( UNKNOWN );
	SetPanel( NULL );
	SetPanelOther( NULL );
	SetColor( 0, 0, 0, 255 );
	SetEndTime( 0.0f );
	SetShouldRemove( false );
	SetUsingOffset( false, 0, 0 );
	SetTargetType( ENDPOINT_UNKNOWN );
	SetVisible( true );

	m_ptX = 0;
	m_ptY = 0;
	m_rectX = 0;
	m_rectY = 0;
	m_rectW = 0;
	m_rectH = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPanelEffect::~CPanelEffect()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *owner - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetOwner( ITFHintItem *owner )
{
	m_pOwner = owner;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : ITFHintItem
//-----------------------------------------------------------------------------
ITFHintItem *CPanelEffect::GetOwner( void )
{
	return m_pOwner;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : EFFECT_HANDLE
//-----------------------------------------------------------------------------
EFFECT_HANDLE CPanelEffect::GetHandle( void )
{
	return m_Handle;
}

//-----------------------------------------------------------------------------
// Purpose: Override for specific painting effects
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CPanelEffect::doPaint( vgui::Panel *panel )
{
}

//-----------------------------------------------------------------------------
// Purpose: Default think checks for panels that have a specific lifetime
//-----------------------------------------------------------------------------
void CPanelEffect::Think( void )
{
	if ( !m_flEndTime )
		return;

	if ( gpGlobals->curtime > m_flEndTime )
	{
		SetShouldRemove( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPanelEffect::ShouldRemove( void )
{
	return m_bShouldRemove;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : remove - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetShouldRemove( bool remove )
{
	m_bShouldRemove = remove;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetType( int type )
{
	m_nType = type;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CPanelEffect::GetType( void )
{
	return m_nType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetPanel( vgui::Panel *panel )
{
	m_hPanel = panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *CPanelEffect::GetPanel( void )
{
	return m_hPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetPanelOther( vgui::Panel *panel )
{
	SetTargetType( ENDPOINT_PANEL );
	m_hOtherPanel = panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *CPanelEffect::GetPanelOther( void )
{
	return m_hOtherPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetColor( int r, int g, int b, int a )
{
	m_r = r;
	m_g = g;
	m_b = b;
	m_a = a;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void CPanelEffect::GetColor( int& r, int& g, int& b, int& a )
{
	r = m_r;
	g = m_g;
	b = m_b;
	a = m_a;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetEndTime( float time )
{
	m_flEndTime = time;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CPanelEffect::GetEndTime( void )
{
	return m_flEndTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : active - 
//			x - 
//			y - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetUsingOffset( bool active, int x, int y )
{
	m_bEndpointIsCoordinate = active;
	m_nOffset[ 0 ] = x;
	m_nOffset[ 1 ] = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPanelEffect::GetUsingOffset( void )
{
	return m_bEndpointIsCoordinate;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CPanelEffect::GetOffset( int& x, int& y )
{
	x = m_nOffset[ 0 ];
	y = m_nOffset[ 1 ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns false if the panel is not visible or is the child of a not visible
//  parent
// assumes panel hierarchy stops at the client .dll root panel
// If parent is some other panel, then this will return false
// Input  : *panel - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPanelEffect::IsVisibleIncludingParent( vgui::Panel *panel )
{
	vgui::VPANEL p = panel->GetVPanel();
	while ( p )
	{
		if ( !vgui::ipanel()->IsVisible(p) )
			return false;

		if ( p == VGui_GetClientDLLRootPanel() )
		{
			return true;
		}

		p = vgui::ipanel()->GetParent(p);
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CPanelEffect::GetTargetType( void )
{
	return m_TargetType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetTargetType( int type )
{
	m_TargetType = type;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *outpanel - 
//			int&x - 
//			int&y - 
//			int&w - 
//			int&h - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPanelEffect::GetTargetRectangle( vgui::Panel *outpanel, int&x, int&y, int&w, int&h )
{
	x = y = 0;
	w = h = 1;

	switch ( m_TargetType )
	{
	default:
		return false;
		break;
	case ENDPOINT_PANEL:
		{
			vgui::Panel *to = m_hOtherPanel;
			if ( !to )
				return false;

			if ( !IsVisibleIncludingParent( to ) )
			{
				return false;
			}

			PanelToPanelRectangle( x, y, w, h, outpanel, to );

			// Using an offset into a panel
			if ( GetUsingOffset() )
			{
				int ofsx, ofsy;

				GetOffset( ofsx, ofsy );

				x = x + ofsx-3;
				w = 6;
				y = y + ofsy-3;
				h = 6;
			}
		}
		break;
	case ENDPOINT_RECTANGLE:
		{
			x = m_rectX;
			y = m_rectY;
			w = m_rectW;
			h = m_rectH;
		}
		break;
	case ENDPOINT_POINT:
		{
			x = m_ptX;
			y = m_ptY;
			w = 1;
			h = 1;
		}
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetTargetPoint( int x, int y )
{
	SetTargetType( ENDPOINT_POINT );
	m_ptX = x;
	m_ptY = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetTargetRect( int x, int y, int w, int h )
{
	SetTargetType( ENDPOINT_RECTANGLE );
	m_rectX = x;
	m_rectY = y;
	m_rectW = w;
	m_rectH = h;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : visible - 
//-----------------------------------------------------------------------------
void CPanelEffect::SetVisible( bool visible )
{
	m_bVisible = visible;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPanelEffect::GetVisible( void )
{
	return m_bVisible;
}

