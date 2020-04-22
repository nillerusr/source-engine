//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a base set of services for operations in an orthorgraphic
//			projection. This is used as a base class for the 2D view and for
//			the tools that work in the 2D views.
//
// $NoKeywords: $
//=============================================================================//

#ifndef AXES2_H
#define AXES2_H
#ifdef _WIN32
#pragma once
#endif

#include "hammer_mathlib.h"

class Axes2
{
public:
	Axes2() 
	{
		bInvertHorz = bInvertVert = false;
		axHorz = AXIS_X;
		axVert = AXIS_Y;
		axThird = AXIS_Z;
	}

	void SetAxes(int h, bool bInvertH, int v, bool bInvertV);
	void SetAxes(Axes2 &axes);

	bool bInvertHorz;	// Whether the horizontal axis is inverted.
	bool bInvertVert;	// Whether the vertical axis is inverted.

	int axHorz;			// Index of the horizontal axis (x=0, y=1, z=2)
	int axVert;			// Index of the vertical axis (x=0, y=1, z=2)
	int axThird;		// Index of the "out of the screen" axis (x=0, y=1, z=2)
};

#endif // AXES2_H
