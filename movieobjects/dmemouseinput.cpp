//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmemouseinput.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"

#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "tier3/tier3.h"

#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMouseInput, CDmeMouseInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMouseInput::OnConstruction()
{
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );

	m_xOrigin = 0.0f;
	m_yOrigin = 0.0f;
}

void CDmeMouseInput::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeMouseInput::IsDirty()
{
	float flX, flY;
	GetNormalizedCursorPos( flX, flY );
	flX -= m_xOrigin;
	flY -= m_yOrigin;

	return ( flX != GetValue< float >( "x" ) ) || ( flY != GetValue< float >( "y" ) );
}

void CDmeMouseInput::Operate()
{
	float flX, flY;
	GetNormalizedCursorPos( flX, flY );

	SetValue( "x", flX - m_xOrigin );
	SetValue( "y", flY - m_yOrigin );

//	Msg( "CDmeMouseInput::Operate() at <%f, %f>\n", flX, flY );
}

void CDmeMouseInput::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
}

void CDmeMouseInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( GetAttribute( "x" ) );
	attrs.AddToTail( GetAttribute( "y" ) );
}

void CDmeMouseInput::ResetOrigin( float dx, float dy )
{
	GetNormalizedCursorPos( m_xOrigin, m_yOrigin );
	m_xOrigin += dx;
	m_yOrigin += dy;
}

void CDmeMouseInput::GetNormalizedCursorPos( float &flX, float &flY )
{
	int x, y;
	g_pVGuiInput->GetCursorPos( x, y );

	vgui::VPANEL vpanel = g_pVGuiInput->GetFocus();
	if ( !vpanel )
	{
		flX = flY = 0.0f;
		return;
	}

	int x0, y0;
	g_pVGuiPanel->GetPos( vpanel, x0, y0 );

	int w, h;
	g_pVGuiPanel->GetSize( vpanel, w, h );

	flX = ( x - x0 ) / float(w);
	flY = ( y - y0 ) / float(h);
}
