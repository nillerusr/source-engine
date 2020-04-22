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
#include "ScalableImageBorder.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ScalableImageBorder::ScalableImageBorder()
{
	_inset[0]=0;
	_inset[1]=0;
	_inset[2]=0;
	_inset[3]=0;
	_name = NULL;
	m_eBackgroundType = IBorder::BACKGROUND_TEXTURED;

	m_iSrcCornerHeight = 0;
	m_iSrcCornerWidth = 0;
	m_iCornerHeight = 0;
	m_iCornerWidth = 0;
	m_pszImageName = NULL;
	m_iTextureID = g_pSurface->CreateNewTextureID();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ScalableImageBorder::~ScalableImageBorder()
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
void ScalableImageBorder::SetImage(const char *imageName)
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

		// get image dimensions, compare to m_iSrcCornerHeight, m_iSrcCornerWidth
		int wide,tall;
		g_pSurface->DrawGetTextureSize( m_iTextureID, wide, tall );

		m_flCornerWidthPercent = ( wide > 0 ) ? ( (float)m_iSrcCornerWidth / (float)wide ) : 0;
		m_flCornerHeightPercent = ( tall > 0 ) ? ( (float)m_iSrcCornerHeight / (float)tall ) : 0;
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::SetInset(int left,int top,int right,int bottom)
{
	_inset[SIDE_LEFT] = left;
	_inset[SIDE_TOP] = top;
	_inset[SIDE_RIGHT] = right;
	_inset[SIDE_BOTTOM] = bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::GetInset(int& left,int& top,int& right,int& bottom)
{
	left = _inset[SIDE_LEFT];
	top = _inset[SIDE_TOP];
	right = _inset[SIDE_RIGHT];
	bottom = _inset[SIDE_BOTTOM];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::Paint(int x, int y, int wide, int tall)
{
	Paint(x, y, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Draws the border with the specified size
//-----------------------------------------------------------------------------
void ScalableImageBorder::Paint(int x, int y, int wide, int tall, int breakSide, int breakStart, int breakEnd)
{
	if ( !m_pszImageName || !m_pszImageName[0] )
		return;

	g_pSurface->DrawSetColor( m_Color );
	g_pSurface->DrawSetTexture( m_iTextureID );

	float uvx = 0;
	float uvy = 0;
	float uvw, uvh;

	float drawW, drawH;

	int row, col;
	for ( row=0;row<3;row++ )
	{
		x = 0;
		uvx = 0;

		if ( row == 0 || row == 2 )
		{
			//uvh - row 0 or 2, is src_corner_height
			uvh = m_flCornerHeightPercent;
			drawH = m_iCornerHeight;
		}
		else
		{
			//uvh - row 1, is tall - ( 2 * src_corner_height ) ( min 0 )
			uvh = max( 1.f - 2.f * m_flCornerHeightPercent, 0.0f );
			drawH = max( 0, ( tall - 2 * m_iCornerHeight ) );
		}

		for ( col=0;col<3;col++ )
		{
			if ( col == 0 || col == 2 )
			{
				//uvw - col 0 or 2, is src_corner_width
				uvw = m_flCornerWidthPercent;
				drawW = m_iCornerWidth;
			}
			else
			{
				//uvw - col 1, is wide - ( 2 * src_corner_width ) ( min 0 )
				uvw = max( 1.f - 2.f * m_flCornerWidthPercent, 0.0f );
				drawW = max( 0, ( wide - 2 * m_iCornerWidth ) );
			}

			Vector2D uv11( uvx, uvy );
			Vector2D uv21( uvx+uvw, uvy );
			Vector2D uv22( uvx+uvw, uvy+uvh );
			Vector2D uv12( uvx, uvy+uvh );

			vgui::Vertex_t verts[4];
			verts[0].Init( Vector2D( x, y ), uv11 );
			verts[1].Init( Vector2D( x+drawW, y ), uv21 );
			verts[2].Init( Vector2D( x+drawW, y+drawH ), uv22 );
			verts[3].Init( Vector2D( x, y+drawH ), uv12  );

			g_pSurface->DrawTexturedPolygon( 4, verts );	

			x += drawW;
			uvx += uvw;
		}

		y += drawH;
		uvy += uvh;
	}

	g_pSurface->DrawSetTexture(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::Paint(VPANEL panel)
{
	// get panel size
	int wide, tall;
	ipanel()->GetSize( panel, wide, tall );
	Paint(0, 0, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImageBorder::ApplySchemeSettings(IScheme *pScheme, KeyValues *inResourceData)
{
	m_eBackgroundType = (backgroundtype_e)inResourceData->GetInt("backgroundtype");

	m_iSrcCornerHeight = inResourceData->GetInt( "src_corner_height" );
	m_iSrcCornerWidth = inResourceData->GetInt( "src_corner_width" );
	m_iCornerHeight = inResourceData->GetInt( "draw_corner_height" );
	m_iCornerWidth = inResourceData->GetInt( "draw_corner_width" );

	// scale the x and y up to our screen co-ords
	m_iCornerHeight = scheme()->GetProportionalScaledValue( m_iCornerHeight);
	m_iCornerWidth = scheme()->GetProportionalScaledValue(m_iCornerWidth);

	const char *imageName = inResourceData->GetString("image", "");
	SetImage( imageName );

	m_bPaintFirst = inResourceData->GetInt("paintfirst", true );

	const char *col = inResourceData->GetString("color", NULL);
	if ( col && col[0] )
	{
		m_Color = pScheme->GetColor(col, Color(255, 255, 255, 255));
	}
	else
	{
		m_Color = Color(255, 255, 255, 255);
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *ScalableImageBorder::GetName()
{
	if (_name)
		return _name;
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void ScalableImageBorder::SetName(const char *name)
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
IBorder::backgroundtype_e ScalableImageBorder::GetBackgroundType()
{
	return m_eBackgroundType;
}

