//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Utility functions for cell coordinate calculations
// to reduce bandwidth usage
//
// $NoKeywords: $
//=============================================================================//

#ifndef CELLCOORD_H
#define CELLCOORD_H
#ifdef _WIN32
#pragma once
#endif

#include "worldsize.h"

// Given a world coord, return the cell it should be in
inline int CellFromCoord( int cellwidth, float f )
{
	// We handle each side of zero difference to reduce precision errors
	if ( f < 0.0f )
	{
		return Float2Int( f + MAX_COORD_INTEGER ) / cellwidth;
	}
	else
	{
		return Float2Int(f) / cellwidth + ( MAX_COORD_INTEGER / cellwidth );
	}
}

// Given a cell and a world coord, return the offset into the cell
// cell should have been returned from CellFromCoord with the same f, we don't
// recompute here
inline float CellInCoord( int cellwidth, int cell, float f )
{
	float r;

	int c = abs( cell * cellwidth - MAX_COORD_INTEGER ) ;

	if ( f < 0.0f )
	{
		r = c + f;
	}
	else
	{
		r = f - c;
	}

	// Pecision errors can futz the edges
	return clamp( r, 0.0f, (float)cellwidth );
}

// Given a cell and an offset in that cell, reconstructor the world coord
inline float CoordFromCell( int cellwidth, int cell, float f )
{
	int cellPos = ( cell * cellwidth );

	float r = ( cellPos - MAX_COORD_INTEGER ) + f;
	return r;
}

#endif //CELLCOORDCONVERTER_H

