//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dodbutton.h"

#include <vgui/ISurface.h>
#include <vgui_controls/EditablePanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//===============================================
// CDODButtonShape - drawing class for dod button shape
//===============================================
void CDODButtonShape::DrawShapedBorder( int x, int y, int wide, int tall, Color fgcolor )
{
	int halfheight = tall / 3;

	surface()->DrawSetColor(fgcolor);

	// top
	surface()->DrawLine( 0, 1, wide-1, 1 );	

	// left
	surface()->DrawLine( 1, 1, 1, tall-1 );

	// bottom
	surface()->DrawLine( 0, tall-1, wide-halfheight, tall-1 );
	
	// right
	surface()->DrawLine( wide-1, 0, wide-1, tall-halfheight );

	// diagonal
	surface()->DrawLine( wide-1, tall-halfheight-1, wide-halfheight-1, tall-1 );
}

void CDODButtonShape::DrawShapedBackground( int x, int y, int wide, int tall, Color bgcolor )
{
	int halfheight = tall / 3;

	if ( m_iWhiteTexture < 0 )
	{
		m_iWhiteTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iWhiteTexture, "vgui/white" , true, false);
	}

	surface()->DrawSetColor(bgcolor);
	surface()->DrawSetTexture( m_iWhiteTexture );
	
	Vertex_t verts[5];

	verts[0].Init( Vector2D( 0, 0 ) );
	verts[1].Init( Vector2D( wide-1, 0 ) );
	verts[2].Init( Vector2D( wide-1, tall-halfheight ) );
	verts[3].Init( Vector2D( wide-halfheight, tall-1 ) );
	verts[4].Init( Vector2D( 0, tall-1 ) );

	surface()->DrawTexturedPolygon(5, verts);

	surface()->DrawSetTexture(0);
}
	
//===============================================
// CDODButton - shaped button
//===============================================
void CDODButton::PaintBackground()
{
	int wide, tall;
	GetSize(wide,tall);
	DrawShapedBackground( 0, 0, wide, tall, GetBgColor() );
}

void CDODButton::PaintBorder()
{
	int wide, tall;
	GetSize(wide,tall);
	DrawShapedBorder( 0, 0, wide, tall, GetFgColor() );
}

//===============================================
// CDODProgressBar - used for weapon stat bars
//===============================================
void CDODProgressBar::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFgColor(	GetSchemeColor("ClassMenuLight", pScheme ) );
	SetBgColor(	GetSchemeColor("ClassMenuDark", pScheme ) );
	SetBarInset(0);
	SetSegmentInfo( 0, 1 );
	SetBorder(NULL);
}