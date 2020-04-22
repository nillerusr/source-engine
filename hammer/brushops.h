//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BRUSHOPS_H
#define BRUSHOPS_H
#ifdef _WIN32
#pragma once
#endif

#include "MapFace.h"


#define	ON_PLANE_EPSILON			0.5f		// Vertices must be within this many units of the plane to be considered on the plane.
#define	MIN_EDGE_LENGTH_EPSILON		0.1f		// Edges shorter than this are considered degenerate.
#define	ROUND_VERTEX_EPSILON		0.01f		// Vertices within this many units of an integer value will be rounded to an integer value.


void Add3dError(DWORD dwObjectID, LPCTSTR pszReason, PVOID pInfo);


winding_t *ClipWinding(winding_t *in, PLANE *split);
winding_t *CopyWinding(winding_t *w);
winding_t *NewWinding(int points);
void FreeWinding (winding_t *w);
winding_t *CreateWindingFromPlane(PLANE *pPlane);
size_t WindingSize(int points);
void RemoveDuplicateWindingPoints(winding_t *pWinding, float fMinDist = 0);


#endif // BRUSHOPS_H
