//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <string.h>

#include <vgui_controls/Panel.h>
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

#include "vgui_internal.h"
#include "ImageBorder.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ImageBorder::ImageBorder()
{
	_name = NULL;
	m_eBackgroundType = IBorder::BACKGROUND_TEXTURED;

	m_pszImageName = NULL;
	m_iTextureID = g_pSurface->CreateNewTextureID();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ImageBorder::~ImageBorder()
{
	if ( vgui::surface() && m_iTextureID != -1 )
	{
		vgui::surface()->DestroyTextureID( m_iTextureID );
		m_iTextureID = -1;
	}

	delete [] _name;
	if ( m_pszImageName )
	{
		delete [] m_pszImageName;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImageBorder::SetImage(const char *imageName)
{
	if ( m_pszImageName )
	{
		delete [] m_pszImageName;
		m_pszImageName = NULL;
	}

	if (*imageName)
	{
		int len = Q_strlen(imageName) + 1 + 5;	// 5 for "vgui/"
		delete [] m_pszImageName;
		m_pszImageName = new char[ len ];
		Q_snprintf( m_pszImageName, len, "vgui/%s", imageName );

		g_pSurface->DrawSetTextureFile( m_iTextureID, m_pszImageName, true, false);
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImageBorder::SetInset(int left,int top,int right,int bottom)
{
	_inset[SIDE_LEFT] = left;
	_inset[SIDE_TOP] = top;
	_inset[SIDE_RIGHT] = right;
	_inset[SIDE_BOTTOM] = bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImageBorder::GetInset(int& left,int& top,int& right,int& bottom)
{
	left = _inset[SIDE_LEFT];
	top = _inset[SIDE_TOP];
	right = _inset[SIDE_RIGHT];
	bottom = _inset[SIDE_BOTTOM];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImageBorder::Paint(int x, int y, int wide, int tall)
{
	Paint(x, y, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Draws the border with the specified size
//-----------------------------------------------------------------------------
void ImageBorder::Paint(int x, int y, int wide, int tall, int breakSide, int breakStart, int breakEnd)
{
	if ( !m_pszImageName || !m_pszImageName[0] )
		return;

	g_pSurface->DrawSetColor( 255, 255, 255, 255 );
	g_pSurface->DrawSetTexture( m_iTextureID );

	float uvx = 0;
	float uvy = 0;
	float uvw = 1.0;
	float uvh = 1.0;
	Vector2D uv11( uvx, uvy );
	Vector2D uv21( uvx+uvw, uvy );
	Vector2D uv22( uvx+uvw, uvy+uvh );
	Vector2D uv12( uvx, uvy+uvh );

	if ( m_bTiled )
	{
		int imageWide, imageTall;
		g_pSurface->DrawGetTextureSize( m_iTextureID, imageWide, imageTall );

		int y = 0;
		while ( y < tall )
		{
			int x = 0;
			while (x < wide)
			{
				vgui::Vertex_t verts[4];
				verts[0].Init( Vector2D( x, y ), uv11 );
				verts[1].Init( Vector2D( x+imageWide, y ), uv21 );
				verts[2].Init( Vector2D( x+imageWide, y+imageTall ), uv22 );
				verts[3].Init( Vector2D( x, y+imageTall ), uv12  );

				g_pSurface->DrawTexturedPolygon( 4, verts );	

				x += imageWide;
			}

			y += imageTall;
		}
	}
	else
	{
		vgui::Vertex_t verts[4];
		verts[0].Init( Vector2D( x, y ), uv11 );
		verts[1].Init( Vector2D( x+wide, y ), uv21 );
		verts[2].Init( Vector2D( x+wide, y+tall ), uv22 );
		verts[3].Init( Vector2D( x, y+tall ), uv12  );

		g_pSurface->DrawTexturedPolygon( 4, verts );	
	}

	g_pSurface->DrawSetTexture(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImageBorder::Paint(VPANEL panel)
{
	// get panel size
	int wide, tall;
	ipanel()->GetSize( panel, wide, tall );
	Paint(0, 0, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImageBorder::ApplySchemeSettings(IScheme *pScheme, KeyValues *inResourceData)
{
	m_eBackgroundType = (backgroundtype_e)inResourceData->GetInt("backgroundtype");
	m_bTiled = inResourceData->GetInt( "tiled" );

	const char *imageName = inResourceData->GetString("image", "");
	SetImage( imageName );

	m_bPaintFirst = inResourceData->GetInt("paintfirst", true );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *ImageBorder::GetName()
{
	if (_name)
		return _name;
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void ImageBorder::SetName(const char *name)
{
	if (_name)
	{
		delete [] _name;
	}

	int len = Q_strlen(name) + 1;
	_name = new char[ len ];
	Q_strncpy( _name, name, len );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IBorder::backgroundtype_e ImageBorder::GetBackgroundType()
{
	return m_eBackgroundType;
}

