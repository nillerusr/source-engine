//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <vgui/ISurface.h>
#include "dodmenubackground.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODMenuBackground::CDODMenuBackground(Panel *parent) : EditablePanel(parent, "MenuBackground")
{
	SetProportional(true);
	SetVisible(true);

	SetZPos( -1 );

	LoadControlSettings("Resource/UI/MenuBackground.res");	
}

void CDODMenuBackground::Init( void )
{
	m_iBackgroundTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iBackgroundTexture, "vgui/white", true, false);
}

void CDODMenuBackground::ApplySchemeSettings( IScheme *pScheme )
{
	int top[8];
	int main[8];
	int box[8];
	int i;
	for( i=0;i<8;i++ )
	{
		top[i] = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(),iTopDims[i]);
		main[i] = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(),iMainDims[i]);
		box[i] = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(),iBoxDims[i]);

		if ( i < 6 )
			m_LineDims[i] = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(),iLineDims[i]);
	}

	m_BackgroundTopVerts[0].Init( Vector2D( top[0], top[1] ) );
	m_BackgroundTopVerts[1].Init( Vector2D( top[2], top[3] ) );
	m_BackgroundTopVerts[2].Init( Vector2D( top[4], top[5] ) );
	m_BackgroundTopVerts[3].Init( Vector2D( top[6], top[7] ) );

	m_BackgroundMainVerts[0].Init( Vector2D( main[0], main[1] ) );
	m_BackgroundMainVerts[1].Init( Vector2D( main[2], main[3] ) );
	m_BackgroundMainVerts[2].Init( Vector2D( main[4], main[5] ) );
	m_BackgroundMainVerts[3].Init( Vector2D( main[6], main[7] ) );

	m_BoxVerts[0].Init( Vector2D( box[0], box[1] ) );
	m_BoxVerts[1].Init( Vector2D( box[2], box[3] ) );
	m_BoxVerts[2].Init( Vector2D( box[4], box[5] ) );
	m_BoxVerts[3].Init( Vector2D( box[6], box[7] ) );

	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Purpose: paint the dod style background
//-----------------------------------------------------------------------------
void CDODMenuBackground::Paint(void)
{
	vgui::surface()->DrawSetColor(128,110,53,235);
	vgui::surface()->DrawSetTexture( m_iBackgroundTexture );

	//top background
	vgui::surface()->DrawTexturedPolygon( 4, m_BackgroundTopVerts );

	//main background
	vgui::surface()->DrawTexturedPolygon( 4, m_BackgroundMainVerts );

	// top white line
	vgui::surface()->DrawSetColor(255,255,255,196);
	vgui::surface()->DrawLine( m_LineDims[0], m_LineDims[1], m_LineDims[2], m_LineDims[3] );
	vgui::surface()->DrawLine( m_LineDims[2], m_LineDims[3], m_LineDims[4], m_LineDims[5] );

	// top white box	
	vgui::surface()->DrawTexturedPolygon( 4, m_BoxVerts );
}

