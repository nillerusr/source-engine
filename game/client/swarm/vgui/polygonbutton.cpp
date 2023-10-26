// CPolygonButton.cpp
// Copyright (c) 2006 Turtle Rock Studios, Inc.

#include "cbase.h"

#include "vgui/polygonbutton.h"
#include "keyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------
CPolygonButton::CPolygonButton( vgui::Panel *parent, const char *panelName )
	: vgui::Button( parent, panelName, L"" )
{
	m_unscaledHotspotPoints.RemoveAll();
	m_unscaledVisibleHotspotPoints.RemoveAll();
	m_hotspotPoints = NULL;
	m_visibleHotspotPoints = NULL;
	m_numHotspotPoints = 0;
	m_numVisibleHotspotPoints = 0;

	m_nWhiteMaterial = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_nWhiteMaterial, "vgui/white" , true, false );
}

//--------------------------------------------------------------------------------------------------------
void CPolygonButton::ApplySettings( KeyValues *data )
{
	BaseClass::ApplySettings( data );

	// Re-read hotspot data from disk
	UpdateHotspots( data );
}

//--------------------------------------------------------------------------------------------------------
void CPolygonButton::UpdateHotspots( KeyValues *data )
{
	// clear out our old hotspot
	if ( m_hotspotPoints )
	{
		delete[] m_hotspotPoints;
		m_hotspotPoints = NULL;
		m_numHotspotPoints = 0;
	}
	if ( m_visibleHotspotPoints )
	{
		delete[] m_visibleHotspotPoints;
		m_visibleHotspotPoints = NULL;
		m_numVisibleHotspotPoints = 0;
	}
	m_unscaledHotspotPoints.RemoveAll();
	m_unscaledVisibleHotspotPoints.RemoveAll();

	// read in a new one
	KeyValues *points = data->FindKey( "Hotspot", false );
	if ( points )
	{
		for ( KeyValues *value = points->GetFirstValue(); value; value = value->GetNextValue() )
		{
			const char *str = value->GetString();

			float x, y;
			if ( 2 == sscanf( str, "%f %f", &x, &y ) )
			{
				m_unscaledHotspotPoints.AddToTail( Vector2D( x, y ) );
			}
		}
	}
	points = data->FindKey( "VisibleHotspot", false );
	if ( !points )
	{
		points = data->FindKey( "Hotspot", false );
	}
	if ( points )
	{
		for ( KeyValues *value = points->GetFirstValue(); value; value = value->GetNextValue() )
		{
			const char *str = value->GetString();

			float x, y;
			if ( 2 == sscanf( str, "%f %f", &x, &y ) )
			{
				m_unscaledVisibleHotspotPoints.AddToTail( Vector2D( x, y ) );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------
/**
 * Clip out cursor positions inside our overall rectangle that are outside our hotspot.
 */
vgui::VPANEL CPolygonButton::IsWithinTraverse( int x, int y, bool traversePopups )
{
	if ( m_numHotspotPoints < 3 )
	{
		return NULL;
	}

	vgui::VPANEL within = BaseClass::IsWithinTraverse( x, y, traversePopups );
	if ( within == GetVPanel() )
	{
		int wide, tall;
		GetSize( wide, tall );
		ScreenToLocal( x, y );

		bool inside = true;
		for ( int i=0; i<m_numHotspotPoints; ++i )
		{
			const Vector2D& pos1 = (i==0)?m_hotspotPoints[m_numHotspotPoints-1].m_Position:m_hotspotPoints[i-1].m_Position;
			const Vector2D& pos2 = m_hotspotPoints[i].m_Position;
			Vector p1( pos1.x - x, pos1.y - y, 0 );
			Vector p2( pos2.x - x, pos2.y - y, 0 );
			Vector out = p1.Cross( p2 );
			if ( out.z < 0 )
			{
				inside = false;
			}
		}

		if ( !inside )
		{
			within = NULL;
		}
	}

	return within;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Perform the standard layout, and scale our hotspot points - they are specified as a 0..1 percentage
 * of the button's full size.
 */
void CPolygonButton::PerformLayout( void )
{
	int wide, tall;
	GetSize( wide, tall );

	if ( m_hotspotPoints )
	{
		delete[] m_hotspotPoints;
		m_hotspotPoints = NULL;
		m_numHotspotPoints = 0;
	}
	if ( m_visibleHotspotPoints )
	{
		delete[] m_visibleHotspotPoints;
		m_visibleHotspotPoints = NULL;
		m_numVisibleHotspotPoints = 0;
	}

	// generate scaled points
	m_numHotspotPoints = m_unscaledHotspotPoints.Count();
	if ( m_numHotspotPoints )
	{
		m_hotspotPoints = new vgui::Vertex_t[ m_numHotspotPoints ];
		for ( int i=0; i<m_numHotspotPoints; ++i )
		{
			float x = m_unscaledHotspotPoints[i].x * wide;
			float y = m_unscaledHotspotPoints[i].y * tall;
			m_hotspotPoints[i].Init( Vector2D( x, y ), m_unscaledHotspotPoints[i] );
		}
	}

	// track our visible extent
	m_hotspotMins.Init( wide, tall );
	m_hotspotMaxs.Init( 0, 0 );

	m_numVisibleHotspotPoints = m_unscaledVisibleHotspotPoints.Count();
	if ( m_numVisibleHotspotPoints )
	{
		m_visibleHotspotPoints = new vgui::Vertex_t[ m_numVisibleHotspotPoints ];
		for ( int i=0; i<m_numVisibleHotspotPoints; ++i )
		{
			float x = m_unscaledVisibleHotspotPoints[i].x * wide;
			float y = m_unscaledVisibleHotspotPoints[i].y * tall;
			m_visibleHotspotPoints[i].Init( Vector2D( x, y ), m_unscaledVisibleHotspotPoints[i] );

			m_hotspotMins.x = MIN( x, m_hotspotMins.x );
			m_hotspotMins.y = MIN( y, m_hotspotMins.y );
			m_hotspotMaxs.x = MAX( x, m_hotspotMaxs.x );
			m_hotspotMaxs.y = MAX( y, m_hotspotMaxs.y );
		}
	}

	BaseClass::PerformLayout();
}

//--------------------------------------------------------------------------------------------------------
/**
 * Center the text in the extent that encompasses our hotspot.
 * TODO: allow the res file and/or the individual menu to specify a different center for text.
 */
void CPolygonButton::ComputeAlignment( int &tx0, int &ty0, int &tx1, int &ty1 )
{
	Vector2D center( (m_hotspotMins + m_hotspotMaxs) * 0.5f );

	BaseClass::ComputeAlignment( tx0, ty0, tx1, ty1 );
	int textWide, textTall;
	textWide = tx1 - tx0;
	textTall = ty1 - ty0;

	tx0 = center.x - textWide/2;
	ty0 = center.y - textTall/2;
	tx1 = tx0 + textWide;
	ty1 = ty0 + textTall;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Paints the polygonal background
 */
void CPolygonButton::PaintBackground( void )
{
	Color c = GetButtonBgColor();
	vgui::surface()->DrawSetColor( c );
	vgui::surface()->DrawSetTexture( m_nWhiteMaterial );
	vgui::surface()->DrawTexturedPolygon( m_numVisibleHotspotPoints, m_visibleHotspotPoints );
}

//--------------------------------------------------------------------------------------------------------
/**
 * Paints the polygonal border
 */
void CPolygonButton::PaintBorder( void )
{
	Color c = GetButtonFgColor();
	vgui::surface()->DrawSetColor( c );
	vgui::surface()->DrawSetTexture( m_nWhiteMaterial );
	vgui::surface()->DrawTexturedPolyLine( m_visibleHotspotPoints, m_numVisibleHotspotPoints );
}

//--------------------------------------------------------------------------------------------------------
void CPolygonButton::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	InvalidateLayout(); // so we can reposition the text
}


//--------------------------------------------------------------------------------------------------------




