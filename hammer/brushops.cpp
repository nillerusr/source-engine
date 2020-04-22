//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <stdio.h>
#include <math.h>
#include "hammer.h"
#include "MapEntity.h"
#include "MapDefs.h"
#include "MapFace.h"
#include "hammer_mathlib.h"
#include "history.h"
#include "Error3d.h"
#include "BrushOps.h"
#include "GlobalFunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#pragma warning( disable : 4244 )  // Disable warning messages


#define	SIDE_FRONT		0
#define	SIDE_BACK		1
#define	SIDE_ON			2

#define	BOGUS_RANGE	( MAX_COORD_INTEGER * 4 )


float		lightaxis[3] = {1, 0.6f, 0.75f};

const int MAX_POINTS_ON_WINDING	= 128;


void Error(char* fmt, ...)
{
	char str[300];
	sprintf(str, fmt, (&fmt)+1);
	Msg(mwError, str);
}


/*
=============================================================================

			TURN PLANES INTO GROUPS OF FACES

=============================================================================
*/


/*
==================
NewWinding
==================
*/
winding_t *NewWinding (int points)
{
	winding_t	*w;

	if (points > MAX_POINTS_ON_WINDING)
		Error ("NewWinding: %i points", points);

	w = (winding_t *)malloc(sizeof(*w));
	w->numpoints = 0; // None are occupied yet even though allocated.
	w->p = (Vector *)calloc( points, sizeof(Vector) );

	return w;
}

void FreeWinding (winding_t *w)
{
	if (*(unsigned *)w == 0xdeaddead)
		Error ("FreeWinding: freed a freed winding");
	*(unsigned *)w = 0xdeaddead;

	if (w->p)
	{
	    free (w->p);
		w->p = NULL;
	}
	free (w);
}

size_t WindingSize(int points)
{
	return (size_t)(&((winding_t *)0)->p[points]);
}


//-----------------------------------------------------------------------------
// Purpose: Removes points that are withing a given distance from each other
//			from the winding.
// Input  : pWinding - The winding to remove duplicates from.
//			fMinDist - The minimum distance two points must be from one another
//				to be considered different. If this is zero, the points must be
//				identical to be considered duplicates.
//-----------------------------------------------------------------------------
void RemoveDuplicateWindingPoints(winding_t *pWinding, float fMinDist)
{
	for (int i = 0; i < pWinding->numpoints; i++)
	{
		for (int j = i + 1; j < pWinding->numpoints; j++)
		{
			Vector edge;
			VectorSubtract(pWinding->p[i], pWinding->p[j], edge);

			if (VectorLength(edge) < fMinDist)
			{
				if (j + 1 < pWinding->numpoints)
				{
					memmove(&pWinding->p[j], &pWinding->p[j + 1], (pWinding->numpoints - (j + 1)) * sizeof(pWinding->p[0]));
				}

				pWinding->numpoints--;
			}
		}		
	}
}


/*
==================
CopyWinding
==================
*/
winding_t	*CopyWinding (winding_t *w)
{
	int			size;
	winding_t	*c;

	c = NewWinding (w->numpoints);
	c->numpoints = w->numpoints;
	size = w->numpoints*sizeof(w->p[0]);
	memcpy (c->p, w->p, size);
	return c;
}


/*
==================
ClipWinding

Clips the winding to the plane, returning the new winding on the positive side
Frees the input winding.
==================
*/
// YWB ADDED SPLIT EPS to match qcsg splitting
#define	SPLIT_EPSILON	0.01
winding_t *ClipWinding (winding_t *in, PLANE *split)
{
	float	dists[MAX_POINTS_ON_WINDING];
	int		sides[MAX_POINTS_ON_WINDING];
	int		counts[3];
	float	dot;
	int		i, j;
	Vector	*p1, *p2, *mid;
	winding_t	*neww;
	int		maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->p[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > SPLIT_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -SPLIT_EPSILON)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0] && !counts[1])
		return in;

	if (!counts[0])
	{
		free (in);
		return NULL;
	}
	if (!counts[1])
		return in;

	maxpts = in->numpoints+4;	// can't use counts[0]+2 because
								// of fp grouping errors
	neww = NewWinding (maxpts);

	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = &in->p[i];

		mid = &neww->p[neww->numpoints];

		if (sides[i] == SIDE_FRONT || sides[i] == SIDE_ON)
		{
			*mid = *p1;
			neww->numpoints++;
			if (sides[i] == SIDE_ON)
				continue;
			mid = &neww->p[neww->numpoints];
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

	// generate a split point
		if (i == in->numpoints - 1)
			p2 = &in->p[0];
		else
			p2 = p1 + 1;

		neww->numpoints++;

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[0][j] = split->dist;
			else if (split->normal[j] == -1)
				mid[0][j] = -split->dist;
			mid[0][j] = p1[0][j] + dot*(p2[0][j]-p1[0][j]);
		}
//		mid[3] = p1[3] + dot*(p2[3]-p1[3]);
//		mid[4] = p1[4] + dot*(p2[4]-p1[4]);
	}

	if (neww->numpoints > maxpts)
		Error ("ClipWinding: points exceeded estimate");

// free the original winding
	FreeWinding (in);

	return neww;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a huge quadrilateral winding given a plane.
// Input  : pPlane - Plane normal and distance to use when creating the winding.
// Output : Returns a winding with 4 points.
//-----------------------------------------------------------------------------
// dvs: read through this and clean it up
winding_t *CreateWindingFromPlane(PLANE *pPlane)
{
	int		i, x;
	float	max, v;
	Vector	org, vright, vup;
	winding_t	*w;
	
	// find the major axis
	max = -BOGUS_RANGE;
	x = -1;
	for (i=0 ; i<3; i++)
	{
		v = fabs(pPlane->normal[i]);
		if (v > max)
		{
			x = i;
			max = v;
		}
	}
	if (x==-1)
		Error ("BasePolyForPlane: no axis found");

	vup = vec3_origin;
	switch (x)
	{
		case 0:
		case 1:
			vup[2] = 1;
			break;
		case 2:
			vup[0] = 1;
			break;
	}

	v = DotProduct (vup, pPlane->normal);
	VectorMA (vup, -v, pPlane->normal, vup);
	VectorNormalize (vup);

	org = pPlane->normal * pPlane->dist;

	CrossProduct (vup, pPlane->normal, vright);

	vup = vup * MAX_TRACE_LENGTH;
	vright = vright * MAX_TRACE_LENGTH;

	// project a really big	axis aligned box onto the plane
	w = NewWinding (4);
	w->numpoints = 4;

	VectorSubtract (org, vright, w->p[0]);
	VectorAdd (w->p[0], vup, w->p[0]);

	VectorAdd (org, vright, w->p[1]);
	VectorAdd (w->p[1], vup, w->p[1]);

	VectorAdd (org, vright, w->p[2]);
	VectorSubtract (w->p[2], vup, w->p[2]);

	VectorSubtract (org, vright, w->p[3]);
	VectorSubtract (w->p[3], vup, w->p[3]);

	return w;
}


static CArray<error3d, error3d&> Errors;
static int nErrors;

void Add3dError(DWORD dwObjectID, LPCTSTR pszReason, PVOID pInfo)
{
	error3d err;
	err.dwObjectID = dwObjectID;
	err.pszReason = pszReason;
	err.pInfo = pInfo;
	Errors.Add(err);
	++nErrors;
}

int Get3dErrorCount()
{
	return nErrors;
}

error3d * Enum3dErrors(BOOL bStart)
{
	static int iCurrent = 0;

	if(bStart)
		iCurrent = 0;

	if(iCurrent == nErrors)
		return NULL;

	return & Errors.GetData()[iCurrent++];
}


