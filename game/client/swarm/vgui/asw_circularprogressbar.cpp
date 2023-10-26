//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "asw_input.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <vgui_controls/Controls.h>

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>

#include "asw_circularprogressbar.h"

#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "engine/IVDebugOverlay.h"

#include "mathlib/mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( ASWCircularProgressBar );

ConVar asw_crosshair_progress_size( "asw_crosshair_progress_size", "20", FCVAR_ARCHIVE );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ASWCircularProgressBar::ASWCircularProgressBar(Panel *parent, const char *panelName) : CircularProgressBar(parent, panelName)
{
	m_iProgressDirection = CircularProgressBar::PROGRESS_CCW;

	m_bIsOnCursor = false;
	m_flScale = 1.0f;
	m_fStartProgress = 0.0;

	for ( int i = 0; i < NUM_PROGRESS_TEXTURES; i++ )
	{
		m_nTextureId[i] = -1;
		m_pszImageName[i] = NULL;
		m_lenImageName[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ASWCircularProgressBar::~ASWCircularProgressBar()
{
	for ( int i = 0; i < NUM_PROGRESS_TEXTURES; i++ )
	{
		delete [] m_pszImageName[i];
		m_lenImageName[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ASWCircularProgressBar::ApplySettings(KeyValues *inResourceData)
{
	for ( int i = 0; i < NUM_PROGRESS_TEXTURES; i++ )
	{
		delete [] m_pszImageName[i];
		m_pszImageName[i] = NULL;
		m_lenImageName[i] = 0;
	}

	const char *imageName = inResourceData->GetString("fg_image", "");
	if (*imageName)
	{
		SetFgImage( imageName );
	}
	imageName = inResourceData->GetString("bg_image", "");
	if (*imageName)
	{
		SetBgImage( imageName );
	}

	BaseClass::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ASWCircularProgressBar::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFgColor(GetSchemeColor("CircularProgressBar.FgColor", pScheme));
	SetBgColor(GetSchemeColor("CircularProgressBar.BgColor", pScheme));
	SetBorder(NULL);

	for ( int i = 0; i < NUM_PROGRESS_TEXTURES; i++ )
	{
		if ( m_pszImageName[i] && strlen( m_pszImageName[i] ) > 0 )
		{
			if ( m_nTextureId[i] == -1 )
			{
				m_nTextureId[i] = surface()->CreateNewTextureID();
			}

			surface()->DrawSetTextureFile( m_nTextureId[i], m_pszImageName[i], true, false);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets an image by file name
//-----------------------------------------------------------------------------
void ASWCircularProgressBar::SetImage(const char *imageName, progress_textures_t iPos)
{
	const char *pszDir = "vgui/";
	int len = Q_strlen(imageName) + 1;
	len += strlen(pszDir);

	if ( m_pszImageName[iPos] && ( m_lenImageName[iPos] < len ) )
	{
		// If we already have a buffer, but it is too short, then free the buffer
		delete [] m_pszImageName[iPos];
		m_pszImageName[iPos] = NULL;
		m_lenImageName[iPos] = 0;
	}

	if ( !m_pszImageName[iPos] )
	{
		m_pszImageName[iPos] = new char[ len ];
		m_lenImageName[iPos] = len;
	}

	Q_snprintf( m_pszImageName[iPos], len, "%s%s", pszDir, imageName );
	InvalidateLayout(false, true); // force applyschemesettings to run
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ASWCircularProgressBar::PaintBackground()
{
	int x = 0;
	int y = 0;
	int w, h;

	if ( m_bIsOnCursor )
	{
		int ux, uy;
		ASWInput()->GetSimulatedFullscreenMousePos(&x, &y, &ux, &uy);
		w = (YRES( asw_crosshair_progress_size.GetInt() ) * 2) * m_flScale;
		h = w;
	
		// do the rotation to match the crosshair
		float fFacingYaw = GetMarineFacingYaw( x, y );
		QAngle angFacing(0, -fFacingYaw + 90.0, 0);

		int iArrowHalfSize = w / 2;
		Vector2D vArrowCenter( x , y );

		Vector vecCornerTL(-iArrowHalfSize, -iArrowHalfSize, 0);
		Vector vecCornerTR(iArrowHalfSize, -iArrowHalfSize, 0);
		Vector vecCornerBR(iArrowHalfSize, iArrowHalfSize, 0);
		Vector vecCornerBL(-iArrowHalfSize, iArrowHalfSize, 0);
		Vector vecCornerTL_rotated, vecCornerTR_rotated, vecCornerBL_rotated, vecCornerBR_rotated;

		// rotate it by our facing yaw
		VectorRotate(vecCornerTL, angFacing, vecCornerTL_rotated);
		VectorRotate(vecCornerTR, angFacing, vecCornerTR_rotated);
		VectorRotate(vecCornerBR, angFacing, vecCornerBR_rotated);
		VectorRotate(vecCornerBL, angFacing, vecCornerBL_rotated);

		// If we don't have a Bg image, use the foreground
		int iTextureID = m_nTextureId[PROGRESS_TEXTURE_BG] != -1 ? m_nTextureId[PROGRESS_TEXTURE_BG] : m_nTextureId[PROGRESS_TEXTURE_FG];
		vgui::surface()->DrawSetTexture( iTextureID );
		vgui::surface()->DrawSetColor( GetBgColor() );

		Vertex_t points[4] = 
		{ 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerTL_rotated.x, vArrowCenter.y + vecCornerTL_rotated.y), 
			Vector2D(0,0) ), 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerTR_rotated.x, vArrowCenter.y + vecCornerTR_rotated.y), 
			Vector2D(1,0) ), 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerBR_rotated.x, vArrowCenter.y + vecCornerBR_rotated.y), 
			Vector2D(1,1) ), 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerBL_rotated.x, vArrowCenter.y + vecCornerBL_rotated.y), 
			Vector2D(0,1) ) 
		}; 
		surface()->DrawTexturedPolygon( 4, points );
	}
	else
	{
		GetSize(w, h);

		// If we don't have a Bg image, use the foreground
		int iTextureID = m_nTextureId[PROGRESS_TEXTURE_BG] != -1 ? m_nTextureId[PROGRESS_TEXTURE_BG] : m_nTextureId[PROGRESS_TEXTURE_FG];
		vgui::surface()->DrawSetTexture( iTextureID );
		vgui::surface()->DrawSetColor( GetBgColor() );

		vgui::surface()->DrawTexturedRect( x - w/2, y - h/2, (x - w/2) + w, (y - h/2) + h );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ASWCircularProgressBar::Paint()
{
	int x = 0;
	int y = 0;
	int w, h;

	if ( m_bIsOnCursor )
	{
		int ux, uy;
		ASWInput()->GetSimulatedFullscreenMousePos(&x, &y, &ux, &uy);
		w = YRES( asw_crosshair_progress_size.GetInt() ) * m_flScale;
		h = w;
	}
	else
	{
		GetSize(w, h);
	}

	float flProgress = GetProgress();
	float flStartProgress = GetStartProgress();
	float flEndAngle;
	float flStartAngle;

	if ( GetProgressDirection() == PROGRESS_CW )
	{
		flEndAngle = flProgress;
		flStartAngle = flStartProgress;
	}
	else
	{
		flEndAngle = ( 1.0 - flProgress );
		flStartAngle = ( 1.0 - flStartProgress );
	}

	if ( m_nTextureId[PROGRESS_TEXTURE_FG] == -1 )
		return;

	vgui::surface()->DrawSetTexture( m_nTextureId[PROGRESS_TEXTURE_FG] );
	vgui::surface()->DrawSetColor( GetFgColor() );

	float flRotation = 0.0f;
	if ( m_bIsOnCursor )
	{
		// do the rotation to match the crosshair
		flRotation = 90 - GetMarineFacingYaw( x, y );
	}
	DrawCircleSegmentAtPosition( x, y, w, h, flStartAngle, flEndAngle, ( GetProgressDirection() == PROGRESS_CW ), flRotation );
}

typedef struct
{
	float minProgressRadians;

	float vert1x;
	float vert1y;
	float vert2x;
	float vert2y;

	int swipe_dir_x;
	int swipe_dir_y;
} asw_circular_progress_segment_t;

// This defines the properties of the 8 circle segments
// in the circular progress bar.
asw_circular_progress_segment_t Segments[8] = 
{
	{ 0.0,			0.5, 0.0, 1.0, 0.0, 1, 0 },
	{ M_PI * 0.25,	1.0, 0.0, 1.0, 0.5, 0, 1 },
	{ M_PI * 0.5,	1.0, 0.5, 1.0, 1.0, 0, 1 },
	{ M_PI * 0.75,	1.0, 1.0, 0.5, 1.0, -1, 0 },
	{ M_PI,			0.5, 1.0, 0.0, 1.0, -1, 0 },
	{ M_PI * 1.25,	0.0, 1.0, 0.0, 0.5, 0, -1 },
	{ M_PI * 1.5,	0.0, 0.5, 0.0, 0.0, 0, -1 },
	{ M_PI * 1.75,	0.0, 0.0, 0.5, 0.0, 1, 0 },
};

#define SEGMENT_ANGLE	( M_PI / 4 )


// function to draw from A to B degrees, with a direction
// we draw starting from the top ( 0 progress )
void ASWCircularProgressBar::DrawCircleSegmentAtPosition( int x, int y, int w, int h, float flStartProgress, float flEndProgress, bool bClockwise, float flRotation, float flUVRotation, float subrect_u, float subrect_v, float subrect_s, float subrect_t )
{
	float flWide = (float)w*2;
	float flTall = (float)h*2;

	float flHalfWide = (float)flWide / 2;
	float flHalfTall = (float)flTall / 2;

	// if we want to progress CCW, reverse a few things
	if ( !bClockwise )
	{
		if ( flStartProgress < flEndProgress )
			return;

		float flEndProgressRadians = flEndProgress * M_PI * 2;
		float flStartProgressRadians = flStartProgress * M_PI * 2;

		int i;
		for ( i=0;i<8;i++ )
		{
			float segmentRadiansMin = Segments[i].minProgressRadians;
			float segmentRadiansMax = segmentRadiansMin + SEGMENT_ANGLE;

			if ( flStartProgressRadians < segmentRadiansMax )
				continue;

			if ( flEndProgressRadians > segmentRadiansMax )
				continue;


			// get the point for the End Progress first
			vgui::Vertex_t v[3];

			// vert 0 is ( 0.5, 0.5 )
			v[0].m_Position.Init( flHalfWide, flHalfTall );
			v[0].m_TexCoord.Init( 0.5f, 0.5f );

			float flInternalProgress = segmentRadiansMax - flEndProgressRadians;

			if ( flInternalProgress < SEGMENT_ANGLE )
			{
				// Calc how much of this slice we should be drawing
				flInternalProgress = SEGMENT_ANGLE - flInternalProgress;

				if ( i % 2 == 1 )
				{
					flInternalProgress = SEGMENT_ANGLE - flInternalProgress;
				}

				float flTan = tan(flInternalProgress);

				float flDeltaX, flDeltaY;

				if ( i % 2 == 1 )
				{
					flDeltaX = ( flHalfWide - flHalfTall * flTan ) * Segments[i].swipe_dir_x;
					flDeltaY = ( flHalfTall - flHalfWide * flTan ) * Segments[i].swipe_dir_y;
				}
				else
				{
					flDeltaX = flHalfTall * flTan * Segments[i].swipe_dir_x;
					flDeltaY = flHalfWide * flTan * Segments[i].swipe_dir_y;
				}

				v[1].m_Position.Init( Segments[i].vert1x * flWide + flDeltaX, Segments[i].vert1y * flTall + flDeltaY );
				v[1].m_TexCoord.Init( Segments[i].vert1x + ( flDeltaX / flHalfWide ) * 0.5, Segments[i].vert1y + ( flDeltaY / flHalfTall ) * 0.5 );
			}
			else
			{
				// full segment, easy calculation
				v[1].m_Position.Init( flHalfWide + flWide * ( Segments[i].vert1x - 0.5 ), flHalfTall + flTall * ( Segments[i].vert1y - 0.5 ) );
				v[1].m_TexCoord.Init( Segments[i].vert1x, Segments[i].vert1y );
			}

			// vert 2 is the start progress
			float flStartInternalProgress = segmentRadiansMin + flStartProgressRadians;

			if ( flStartInternalProgress < SEGMENT_ANGLE )
			{
				// Calc how much of this slice we should be drawing
				flStartInternalProgress = SEGMENT_ANGLE - flStartInternalProgress;

				if ( i % 2 == 1 )
				{
					flStartInternalProgress = SEGMENT_ANGLE - flStartInternalProgress;
				}

				float flTan = tan(flStartInternalProgress);

				float flDeltaX, flDeltaY;

				if ( i % 2 == 1 )
				{
					flDeltaX = ( flHalfWide - flHalfTall * flTan ) * Segments[i].swipe_dir_x;
					flDeltaY = ( flHalfTall - flHalfWide * flTan ) * Segments[i].swipe_dir_y;
				}
				else
				{
					flDeltaX = flHalfTall * flTan * Segments[i].swipe_dir_x;
					flDeltaY = flHalfWide * flTan * Segments[i].swipe_dir_y;
				}

				v[2].m_Position.Init( Segments[i].vert1x * flWide + flDeltaX, Segments[i].vert1y * flTall + flDeltaY );
				v[2].m_TexCoord.Init( Segments[i].vert1x + ( flDeltaX / flHalfWide ) * 0.5, Segments[i].vert1y + ( flDeltaY / flHalfTall ) * 0.5 );
			}
			else
			{
				// vert 2 is ( Segments[i].vert1x, Segments[i].vert1y )
				v[2].m_Position.Init( flHalfWide + flWide * ( Segments[i].vert2x - 0.5 ), flHalfTall + flTall * ( Segments[i].vert2y - 0.5 ) );
				v[2].m_TexCoord.Init( Segments[i].vert2x, Segments[i].vert2y );
			}

			// rotate verts
			if ( flRotation != 0.0f )
			{
				QAngle angFacing(0, flRotation, 0);

				// now go and offset our verts by our position and rotate it;
				for ( int i=0;i<=2;i++ )
				{
					Vector vecCorner( v[i].m_Position.x - flHalfWide, v[i].m_Position.y - flHalfTall, 0 );
					Vector vecCorner_rotated;

					// rotate it by our facing yaw
					VectorRotate(vecCorner, angFacing, vecCorner_rotated);

					v[i] = Vertex_t( Vector2D( vecCorner_rotated.x + flHalfWide, vecCorner_rotated.y + flHalfTall ),
						Vector2D( v[i].m_TexCoord.x, v[i].m_TexCoord.y ) );
				}
			}
			// rotate UVs
			if ( flUVRotation != 0.0f )
			{
				QAngle angFacing(0, flUVRotation, 0);

				// now go and offset our verts by our position and rotate it;
				for ( int i=0;i<=2;i++ )
				{
					Vector vecCorner( v[i].m_TexCoord.x - 0.5f, v[i].m_TexCoord.y - 0.5f, 0 );
					Vector vecCorner_rotated;

					// rotate it by our facing yaw
					VectorRotate(vecCorner, angFacing, vecCorner_rotated);

					v[i] = Vertex_t( Vector2D( v[i].m_Position.x, v[i].m_Position.y ),
						Vector2D( vecCorner_rotated.x + 0.5f, vecCorner_rotated.y  + 0.5f ) );
				}
			}
			
			// shift verts to the new position
			for ( int i = 0; i < 3; i++ )
			{
				v[i].m_Position.x += x - flHalfWide;
				v[i].m_Position.y += y - flHalfTall;
			}

			// remap tex coords to sub rect
			for ( int i = 0; i < 3; i++ )
			{
				v[i].m_TexCoord.x = subrect_u + ( subrect_s - subrect_u ) * v[i].m_TexCoord.x;
				v[i].m_TexCoord.y = subrect_v + ( subrect_t - subrect_v ) * v[i].m_TexCoord.y;
			}

			vgui::surface()->DrawTexturedPolygon( 3, v );
		}
		return;
	}

	float flEndProgressRadians = flEndProgress * M_PI * 2;

	int i;
	for ( i=0;i<8;i++ )
	{
		if ( flEndProgressRadians > Segments[i].minProgressRadians )
		{
			vgui::Vertex_t v[3];

			// vert 0 is ( 0.5, 0.5 )
			v[0].m_Position.Init( flHalfWide, flHalfTall );
			v[0].m_TexCoord.Init( 0.5f, 0.5f );

			float flInternalProgress = flEndProgressRadians - Segments[i].minProgressRadians;

			if ( flInternalProgress < SEGMENT_ANGLE )
			{
				// Calc how much of this slice we should be drawing

				if ( i % 2 == 1 )
				{
					flInternalProgress = SEGMENT_ANGLE - flInternalProgress;
				}

				float flTan = tan(flInternalProgress);
	
				float flDeltaX, flDeltaY;

				if ( i % 2 == 1 )
				{
					flDeltaX = ( flHalfWide - flHalfTall * flTan ) * Segments[i].swipe_dir_x;
					flDeltaY = ( flHalfTall - flHalfWide * flTan ) * Segments[i].swipe_dir_y;
				}
				else
				{
					flDeltaX = flHalfTall * flTan * Segments[i].swipe_dir_x;
					flDeltaY = flHalfWide * flTan * Segments[i].swipe_dir_y;
				}

				v[2].m_Position.Init( Segments[i].vert1x * flWide + flDeltaX, Segments[i].vert1y * flTall + flDeltaY );
				v[2].m_TexCoord.Init( Segments[i].vert1x + ( flDeltaX / flHalfWide ) * 0.5, Segments[i].vert1y + ( flDeltaY / flHalfTall ) * 0.5 );
			}
			else
			{
				// full segment, easy calculation
				v[2].m_Position.Init( flHalfWide + flWide * ( Segments[i].vert2x - 0.5 ), flHalfTall + flTall * ( Segments[i].vert2y - 0.5 ) );
				v[2].m_TexCoord.Init( Segments[i].vert2x, Segments[i].vert2y );
			}

			// vert 2 is ( Segments[i].vert1x, Segments[i].vert1y )
			v[1].m_Position.Init( flHalfWide + flWide * ( Segments[i].vert1x - 0.5 ), flHalfTall + flTall * ( Segments[i].vert1y - 0.5 ) );
			v[1].m_TexCoord.Init( Segments[i].vert1x, Segments[i].vert1y );

			// rotate verts
			if ( flRotation != 0.0f )
			{
				QAngle angFacing(0, flRotation, 0);

				// now go and offset our verts by our position and rotate it;
				for ( int i=0;i<=2;i++ )
				{
					Vector vecCorner( v[i].m_Position.x - flHalfWide, v[i].m_Position.y - flHalfTall, 0 );
					Vector vecCorner_rotated;

					// rotate it by our facing yaw
					VectorRotate(vecCorner, angFacing, vecCorner_rotated);

					v[i] = Vertex_t( Vector2D( vecCorner_rotated.x + flHalfWide, vecCorner_rotated.y + flHalfTall ),
						Vector2D( v[i].m_TexCoord.x, v[i].m_TexCoord.y ) );
				}
			}
			// rotate UVs
			if ( flUVRotation != 0.0f )
			{
				QAngle angFacing(0, flUVRotation, 0);

				// now go and offset our verts by our position and rotate it;
				for ( int i=0;i<=2;i++ )
				{
					Vector vecCorner( v[i].m_TexCoord.x - 0.5f, v[i].m_TexCoord.y - 0.5f, 0 );
					Vector vecCorner_rotated;

					// rotate it by our facing yaw
					VectorRotate(vecCorner, angFacing, vecCorner_rotated);

					v[i] = Vertex_t( Vector2D( v[i].m_Position.x, v[i].m_Position.y ),
						Vector2D( vecCorner_rotated.x + 0.5f, vecCorner_rotated.y  + 0.5f ) );
				}
			}

			// shift verts to the new position
			for ( int i = 0; i < 3; i++ )
			{
				v[i].m_Position.x += x - flHalfWide;
				v[i].m_Position.y += y - flHalfTall;
			}

			// remap tex coords to sub rect
			for ( int i = 0; i < 3; i++ )
			{
				v[i].m_TexCoord.x = subrect_u + ( subrect_s - subrect_u ) * v[i].m_TexCoord.x;
				v[i].m_TexCoord.y = subrect_v + ( subrect_t - subrect_v ) * v[i].m_TexCoord.y;
			}

			vgui::surface()->DrawTexturedPolygon( 3, v );
		}
	}
}

float ASWCircularProgressBar::GetMarineFacingYaw( int x, int y )
{
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if (!local)
		return 0;	

	C_ASW_Marine *pMarine = local->GetMarine();
	if (!pMarine)
		return 0;

	Vector vecFacing;
	Vector screenPos;
	Vector MarinePos = pMarine->GetRenderOrigin();
	debugoverlay->ScreenPosition( MarinePos + Vector( 0, 0, 45 ), screenPos );

	int current_posx = 0;
	int current_posy = 0;
	ASWInput()->GetSimulatedFullscreenMousePos( &current_posx, &current_posy );
	vecFacing.x = screenPos.x - current_posx;
	vecFacing.y = screenPos.y - current_posy;

	return -UTIL_VecToYaw( vecFacing );
}