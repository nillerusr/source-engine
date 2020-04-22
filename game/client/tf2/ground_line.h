//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GROUND_LINE_H
#define GROUND_LINE_H
#ifdef _WIN32
#pragma once
#endif


#include "mathlib/vector.h"
#include "hud_minimap.h"


class IMaterial;


#define MAX_GROUNDLINE_SEGMENTS	100


// This class will lay out a line of a specified width along the ground. It follows
// the contour of the ground as well as it can.
class CGroundLine : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
					CGroundLine();
					~CGroundLine();

	// One-time initialization.
	bool			Init(const char *pMaterialName);

	// Setup the line's rendering parameters.
	void			SetParameters(
		const Vector &vStart, 
		const Vector &vEnd, 
		const Vector &vStartColor,	// Color values 0-1
		const Vector &vEndColor,	// Color values 0-1
		float alpha,
		float lineWidth
		);

	// Called by the renderer when it's time to render the ground lines.
	static void		DrawAllGroundLines();

	// Set the visibility
	void			SetVisible( bool bVisible );
	bool			IsVisible( void );

private:
	
	// Draw a line along the ground.
	void			Draw();
	void			Paint();

private:
	// Rendering parameters.
	IMaterial		*m_pMaterial;
	Vector			m_vStartColor;
	Vector			m_vEndColor;
	float			m_Alpha;
	Vector			m_vStart;
	Vector			m_vEnd;
	float			m_LineWidth;
	bool			m_bVisible;

	// Points along the line.
	Vector			m_Points[MAX_GROUNDLINE_SEGMENTS];
	unsigned int	m_nPoints;

	unsigned short	m_ListHandle;
};


#endif // GROUND_LINE_H
