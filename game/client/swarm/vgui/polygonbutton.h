// PolygonButton.h
// Copyright (c) 2006 Turtle Rock Studios, Inc.

#ifndef POLYGON_BUTTON_H
#define POLYGON_BUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/button.h>
#include <vgui/isurface.h>

//--------------------------------------------------------------------------------------------------------
/**
 * Button class that clips its visible bounds to an concave polygon (defined counter-clockwise).
 * An optional second hotspot can be defined for capturing mouse input outside of the visible hotspot.
 */
class CPolygonButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CPolygonButton, vgui::Button );

public:

	CPolygonButton( vgui::Panel *parent, const char *panelName );

	virtual void ApplySettings( KeyValues *data );

	/**
	 * Clip out cursor positions inside our overall rectangle that are outside our hotspot.
	 */
	virtual vgui::VPANEL IsWithinTraverse( int x, int y, bool traversePopups );

	/**
	 * Perform the standard layout, and scale our hotspot points - they are specified as a 0..1 percentage
	 * of the button's full size.
	 */
	virtual void PerformLayout( void );

	/**
	 * Center the text in the extent that encompasses our hotspot.
	 * TODO: allow the res file and/or the individual menu to specify a different center for text.
	 */
	virtual void ComputeAlignment( int &tx0, int &ty0, int &tx1, int &ty1 );
	virtual void PaintBackground( void );
	virtual void PaintBorder( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	virtual void UpdateHotspots( KeyValues *data );

protected:
	int m_nWhiteMaterial;

	CUtlVector< Vector2D > m_unscaledHotspotPoints;
	CUtlVector< Vector2D > m_unscaledVisibleHotspotPoints;
	vgui::Vertex_t *m_hotspotPoints;
	int m_numHotspotPoints;
	vgui::Vertex_t *m_visibleHotspotPoints;
	int m_numVisibleHotspotPoints;

	Vector2D m_hotspotMins;
	Vector2D m_hotspotMaxs;
};


#endif // POLYGON_BUTTON_H
