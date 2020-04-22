//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"
#include "OPTGeneral.h"
#include "Options.h"
#include "hammer_mathlib.h"
#include "MapFace.h"
#include "MapGroup.h"
#include "MapSolid.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Create a segment using two polygons and a start and end position in
//			those polygons.
// Input  : fZMin - 
//			fZMax - 
//			fOuterPoints - 
//			fInnerPoints - 
//			iStart - 
//			iEnd - 
//			bCreateSouthFace - 
// Output : 
//-----------------------------------------------------------------------------
static CMapSolid *CreateSegment(float fZMin, float fZMax, float fOuterPoints[][2], float fInnerPoints[][2], int iStart, int iEnd, BOOL bCreateSouthFace)
{
	CMapFace Face;
	Vector points[4];	// all sides have four vertices

	CMapSolid *pSolid = new CMapSolid;

	int iNorthSouthPoints = 3 + (bCreateSouthFace ? 1 : 0);

	// create top face
	points[0][0] = fOuterPoints[iStart][0];
	points[0][1] = fOuterPoints[iStart][1];
	points[0][2] = fZMin;

	points[1][0] = fOuterPoints[iEnd][0];
	points[1][1] = fOuterPoints[iEnd][1];
	points[1][2] = fZMin;

	points[2][0] = fInnerPoints[iEnd][0];
	points[2][1] = fInnerPoints[iEnd][1];
	points[2][2] = fZMin;

	points[3][0] = fInnerPoints[iStart][0];
	points[3][1] = fInnerPoints[iStart][1];
	points[3][2] = fZMin;

	Face.CreateFace(points, iNorthSouthPoints);
	pSolid->AddFace(&Face);

	// bottom face - set other z value and reverse order
	for (int i = 0; i < 4; i++)
	{
		points[i][2] = fZMax;
	}

	Face.CreateFace(points, -iNorthSouthPoints);
	pSolid->AddFace(&Face);

	// left side
	points[0][0] = fOuterPoints[iStart][0];
	points[0][1] = fOuterPoints[iStart][1];
	points[0][2] = fZMax;

	points[1][0] = fOuterPoints[iStart][0];
	points[1][1] = fOuterPoints[iStart][1];
	points[1][2] = fZMin;

	points[2][0] = fInnerPoints[iStart][0];
	points[2][1] = fInnerPoints[iStart][1];
	points[2][2] = fZMin;

	points[3][0] = fInnerPoints[iStart][0];
	points[3][1] = fInnerPoints[iStart][1];
	points[3][2] = fZMax;

	Face.CreateFace(points, 4);
	pSolid->AddFace(&Face);

	// right side
	points[0][0] = fOuterPoints[iEnd][0];
	points[0][1] = fOuterPoints[iEnd][1];
	points[0][2] = fZMin;

	points[1][0] = fOuterPoints[iEnd][0];
	points[1][1] = fOuterPoints[iEnd][1];
	points[1][2] = fZMax;

	points[2][0] = fInnerPoints[iEnd][0];
	points[2][1] = fInnerPoints[iEnd][1];
	points[2][2] = fZMax;

	points[3][0] = fInnerPoints[iEnd][0];
	points[3][1] = fInnerPoints[iEnd][1];
	points[3][2] = fZMin;

	Face.CreateFace(points, 4);
	pSolid->AddFace(&Face);

	// north face
	points[0][0] = fOuterPoints[iEnd][0];
	points[0][1] = fOuterPoints[iEnd][1];
	points[0][2] = fZMin;

	points[1][0] = fOuterPoints[iStart][0];
	points[1][1] = fOuterPoints[iStart][1];
	points[1][2] = fZMin;

	points[2][0] = fOuterPoints[iStart][0];
	points[2][1] = fOuterPoints[iStart][1];
	points[2][2] = fZMax;

	points[3][0] = fOuterPoints[iEnd][0];
	points[3][1] = fOuterPoints[iEnd][1];
	points[3][2] = fZMax;

	Face.CreateFace(points, 4);
	pSolid->AddFace(&Face);

	// south face
	if (bCreateSouthFace)
	{
		points[0][0] = fInnerPoints[iStart][0];
		points[0][1] = fInnerPoints[iStart][1];
		points[0][2] = fZMin;

		points[1][0] = fInnerPoints[iEnd][0];
		points[1][1] = fInnerPoints[iEnd][1];
		points[1][2] = fZMin;

		points[2][0] = fInnerPoints[iEnd][0];
		points[2][1] = fInnerPoints[iEnd][1];
		points[2][2] = fZMax;

		points[3][0] = fInnerPoints[iStart][0];
		points[3][1] = fInnerPoints[iStart][1];
		points[3][2] = fZMax;

		Face.CreateFace(points, 4);
		pSolid->AddFace(&Face);
	}

	pSolid->InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);

	return(pSolid);
}


//-----------------------------------------------------------------------------
// Purpose: Create a segment using two polygons and a start and end position in
//			those polygons.
// Input  : fZMin - 
//			fZMax - 
//			fOuterPoints - 
//			fInnerPoints - 
//			iStart - 
//			iEnd - 
//			bCreateSouthFace - 
// Output : 
//-----------------------------------------------------------------------------
static CMapSolid *CreateSegment(float fStartOuterPoints[][3], float fStartInnerPoints[][3],
								float fEndOuterPoints[][3], float fEndInnerPoints[][3],
								int iStart, int iEnd, BOOL bCreateSouthFace)
{
	CMapFace Face;
	Vector points[4];	// all sides have four vertices

	CMapSolid *pSolid = new CMapSolid;

	// create top face
	points[0][0] = fStartOuterPoints[iStart][0];
	points[0][1] = fStartOuterPoints[iStart][1];
	points[0][2] = fStartOuterPoints[iStart][2];

	points[1][0] = fStartOuterPoints[iEnd][0];
	points[1][1] = fStartOuterPoints[iEnd][1];
	points[1][2] = fStartOuterPoints[iEnd][2];

	points[2][0] = fStartInnerPoints[iEnd][0];
	points[2][1] = fStartInnerPoints[iEnd][1];
	points[2][2] = fStartInnerPoints[iEnd][2];

	points[3][0] = fStartInnerPoints[iStart][0];
	points[3][1] = fStartInnerPoints[iStart][1];
	points[3][2] = fStartInnerPoints[iStart][2];

	Face.CreateFace(points, -4);
	pSolid->AddFace(&Face);

	// bottom face - set other z value and reverse order
	points[0][0] = fEndOuterPoints[iStart][0];
	points[0][1] = fEndOuterPoints[iStart][1];
	points[0][2] = fEndOuterPoints[iStart][2];

	points[1][0] = fEndOuterPoints[iEnd][0];
	points[1][1] = fEndOuterPoints[iEnd][1];
	points[1][2] = fEndOuterPoints[iEnd][2];

	points[2][0] = fEndInnerPoints[iEnd][0];
	points[2][1] = fEndInnerPoints[iEnd][1];
	points[2][2] = fEndInnerPoints[iEnd][2];

	points[3][0] = fEndInnerPoints[iStart][0];
	points[3][1] = fEndInnerPoints[iStart][1];
	points[3][2] = fEndInnerPoints[iStart][2];

	Face.CreateFace(points, 4);
	pSolid->AddFace(&Face);

	// left side
	points[0][0] = fEndOuterPoints[iStart][0];
	points[0][1] = fEndOuterPoints[iStart][1];
	points[0][2] = fEndOuterPoints[iStart][2];

	points[1][0] = fStartOuterPoints[iStart][0];
	points[1][1] = fStartOuterPoints[iStart][1];
	points[1][2] = fStartOuterPoints[iStart][2];

	points[2][0] = fStartInnerPoints[iStart][0];
	points[2][1] = fStartInnerPoints[iStart][1];
	points[2][2] = fStartInnerPoints[iStart][2];

	points[3][0] = fEndInnerPoints[iStart][0];
	points[3][1] = fEndInnerPoints[iStart][1];
	points[3][2] = fEndInnerPoints[iStart][2];

	Face.CreateFace(points, -4);
	pSolid->AddFace(&Face);

	// right side
	points[0][0] = fStartOuterPoints[iEnd][0];
	points[0][1] = fStartOuterPoints[iEnd][1];
	points[0][2] = fStartOuterPoints[iEnd][2];

	points[1][0] = fEndOuterPoints[iEnd][0];
	points[1][1] = fEndOuterPoints[iEnd][1];
	points[1][2] = fEndOuterPoints[iEnd][2];

	points[2][0] = fEndInnerPoints[iEnd][0];
	points[2][1] = fEndInnerPoints[iEnd][1];
	points[2][2] = fEndInnerPoints[iEnd][2];

	points[3][0] = fStartInnerPoints[iEnd][0];
	points[3][1] = fStartInnerPoints[iEnd][1];
	points[3][2] = fStartInnerPoints[iEnd][2];

	Face.CreateFace(points, -4);
	pSolid->AddFace(&Face);

	// north face
	points[0][0] = fStartOuterPoints[iEnd][0];
	points[0][1] = fStartOuterPoints[iEnd][1];
	points[0][2] = fStartOuterPoints[iEnd][2];

	points[1][0] = fStartOuterPoints[iStart][0];
	points[1][1] = fStartOuterPoints[iStart][1];
	points[1][2] = fStartOuterPoints[iStart][2];

	points[2][0] = fEndOuterPoints[iStart][0];
	points[2][1] = fEndOuterPoints[iStart][1];
	points[2][2] = fEndOuterPoints[iStart][2];

	points[3][0] = fEndOuterPoints[iEnd][0];
	points[3][1] = fEndOuterPoints[iEnd][1];
	points[3][2] = fEndOuterPoints[iEnd][2];

	Face.CreateFace(points, -4);
	pSolid->AddFace(&Face);

	// south face
	if (bCreateSouthFace)
	{
		points[0][0] = fStartInnerPoints[iStart][0];
		points[0][1] = fStartInnerPoints[iStart][1];
		points[0][2] = fStartInnerPoints[iStart][2];

		points[1][0] = fStartInnerPoints[iEnd][0];
		points[1][1] = fStartInnerPoints[iEnd][1];
		points[1][2] = fStartInnerPoints[iEnd][2];

		points[2][0] = fEndInnerPoints[iEnd][0];
		points[2][1] = fEndInnerPoints[iEnd][1];
		points[2][2] = fEndInnerPoints[iEnd][2];

		points[3][0] = fEndInnerPoints[iStart][0];
		points[3][1] = fEndInnerPoints[iStart][1];
		points[3][2] = fEndInnerPoints[iStart][2];

		Face.CreateFace(points, -4);
		pSolid->AddFace(&Face);
	}

	pSolid->InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE);

	return(pSolid);
}


//-----------------------------------------------------------------------------
// Make a 2d arc
//-----------------------------------------------------------------------------
void MakeArcCenterRadius(float xCenter, float yCenter, float xrad, float yrad, int npoints, float start_ang, float fArc, float points[][2])
{
    int point;
    float angle = start_ang;
	float angle_delta;

	angle_delta = fArc / (float)npoints;

	// Add an additional points if we are not doing a full circle
	if (fArc != 360.0)
	{
		++npoints;
	}
	
    for( point = 0; point < npoints; point++ )
    {
        if ( angle > 360 )
		{
           angle -= 360;
		}

        points[point][0] = V_rint(xCenter + (float)cos(DEG2RAD(angle)) * xrad);
        points[point][1] = V_rint(yCenter + (float)sin(DEG2RAD(angle)) * yrad);

		angle += angle_delta;
    }

	// Full circle, recopy the first point as the closing point.
	if (fArc == 360.0)
	{
	    points[point][0] = points[0][0];
		points[point][1] = points[0][1];
	}
}

void MakeArc(float x1, float y1, float x2, float y2, int npoints, float start_ang, float fArc, float points[][2])
{
    float xrad = (x2 - x1) / 2.0f;
	float yrad = (y2 - y1) / 2.0f;

	// make centerpoint for polygon:
    float xCenter = x1 + xrad;
    float yCenter = y1 + yrad;

	MakeArcCenterRadius( xCenter, yCenter, xrad, yrad, npoints, start_ang, fArc, points );
}

#define ARC_MAX_POINTS 4096


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pBox - 
//			fStartAngle - 
//			iSides - 
//			fArc - 
//			iWallWidth - 
//			iAddHeight - 
//			bPreview - 
// Output : Returns a group containing the arch solids.
//-----------------------------------------------------------------------------
CMapClass *CreateArch(BoundBox *pBox, float fStartAngle, int iSides, float fArc, int iWallWidth, int iAddHeight, BOOL bPreview)
{
	float fOuterPoints[ARC_MAX_POINTS][2];
	float fInnerPoints[ARC_MAX_POINTS][2];

	//
	// create outer points
	//
	MakeArc(pBox->bmins[AXIS_X], pBox->bmins[AXIS_Y],
		pBox->bmaxs[AXIS_X], pBox->bmaxs[AXIS_Y], iSides,
		fStartAngle, fArc, fOuterPoints);

	//
	// create inner points
	//
	MakeArc(pBox->bmins[AXIS_X] + iWallWidth, 
		pBox->bmins[AXIS_Y] + iWallWidth, 
		pBox->bmaxs[AXIS_X] - iWallWidth, 
		pBox->bmaxs[AXIS_Y] - iWallWidth, iSides, 
		fStartAngle, fArc, fInnerPoints);


	//
	// check wall width - if it's half or more of the total,
	//  set the inner poinst to the center point of the box
	//  and turn off the CreateSouthFace flag
	//	
	BOOL bCreateSouthFace = TRUE;
	Vector Center;
	pBox->GetBoundsCenter(Center);
	if((iWallWidth*2+8) >= (pBox->bmaxs[AXIS_X] - pBox->bmins[AXIS_X]) ||
		(iWallWidth*2+8) >= (pBox->bmaxs[AXIS_Y] - pBox->bmins[AXIS_Y]))
	{
		for(int i = 0; i < ARC_MAX_POINTS; i++)
		{
			fInnerPoints[i][AXIS_X] = Center[AXIS_X];
			fInnerPoints[i][AXIS_Y] = Center[AXIS_Y];
		}
		bCreateSouthFace = FALSE;
	}

	// create group for segments
	CMapGroup *pGroup = new CMapGroup;

	Vector MoveAccum( 0.f, 0.f, 0.f );

	float fMinZ, fMaxZ;

	fMinZ = pBox->bmins[2];
	fMaxZ = pBox->bmaxs[2];

	if ((fMaxZ - fMinZ) < 1.0f)
		fMaxZ = fMinZ + 1.0f;

	for (int i = 0; i < iSides; i++)
	{
		int iNextPoint = i+1;
		if (iNextPoint >= iSides + 1)
			iNextPoint = 0;

		CMapSolid *pSolid = CreateSegment(
			fMinZ, fMaxZ,
			fOuterPoints, fInnerPoints,
			i, iNextPoint, bCreateSouthFace);

		pGroup->AddChild(pSolid);

		if (iAddHeight && i)	// don't move first segment
		{
			MoveAccum[2] += iAddHeight;
			pSolid->TransMove(MoveAccum);
		}
	}

	pGroup->CalcBounds(TRUE);
	if (Options.general.bStretchArches)
	{
		// make sure size of group's bounds are size of original bounds -
		//  if not, scale up. this can happen when we use rotation.
		Vector objsize, boundsize;
		pBox->GetBoundsSize(boundsize);
		pGroup->GetBoundsSize(objsize);

		if (boundsize[AXIS_X] > objsize[AXIS_X] || 
			boundsize[AXIS_Y] > objsize[AXIS_Y])
		{
			Vector scale;
			scale[AXIS_X] = boundsize[AXIS_X] / objsize[AXIS_X];
			scale[AXIS_Y] = boundsize[AXIS_Y] / objsize[AXIS_Y];
			scale[AXIS_Z] = 1.0f;  // xxxYWB scaling by 0 causes veneers, so I changed to 1.0
			Vector center;
			pBox->GetBoundsCenter(center);
			pGroup->TransScale(center, scale);
		}
	}

	return pGroup;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pBox - 
//			fStartAngle - 
//			iSides - 
//			fArc - 
//			iWallWidth - 
//			iAddHeight - 
//			bPreview - 
// Output : Returns a group containing the arch solids.
//-----------------------------------------------------------------------------
typedef float TorusPointList_t[ARC_MAX_POINTS][3];

CMapClass *CreateTorus(BoundBox *pBox, float fStartAngle, int iSides, float fArc, int iWallWidth, float flCrossSectionalRadius,
	float fRotationStartAngle, int iRotationSides, float fRotationArc, int iAddHeight, BOOL bPreview)
{
	float xCenter = (pBox->bmaxs[AXIS_X] + pBox->bmins[AXIS_X]) * 0.5f;
	float yCenter = (pBox->bmaxs[AXIS_Y] + pBox->bmins[AXIS_Y]) * 0.5f;
	float xRad = (pBox->bmaxs[AXIS_X] - xCenter);
	float yRad = (pBox->bmaxs[AXIS_Y] - yCenter);
	if (xRad < 0.0f )
	{
		xRad = 0.0f;
	}

	if (yRad < 0.0f )
	{
		yRad = 0.0f;
	}

	if ( flCrossSectionalRadius > (xRad * 0.5f) )
	{
		flCrossSectionalRadius = (xRad * 0.5f);
	}
	if ( flCrossSectionalRadius > (yRad * 0.5f) )
	{
		flCrossSectionalRadius = (yRad * 0.5f);
	}

	if ( iWallWidth < flCrossSectionalRadius )
	{
		flCrossSectionalRadius -= iWallWidth;
	}
	else
	{
		iWallWidth = flCrossSectionalRadius;
		flCrossSectionalRadius = 0.0f;
	}

	float flCrossSectionHalfWidth = flCrossSectionalRadius + iWallWidth;
	xRad -= flCrossSectionHalfWidth;
	yRad -= flCrossSectionHalfWidth;

	float fOuterPoints[ARC_MAX_POINTS][2];
	float fInnerPoints[ARC_MAX_POINTS][2];

	// create outer points (unrotated)
	MakeArcCenterRadius(0.0f, 0.0f,
		flCrossSectionalRadius + iWallWidth, flCrossSectionalRadius + iWallWidth, 
		iSides, fStartAngle, fArc, fOuterPoints);

	BOOL bCreateSouthFace = TRUE;
	if ( flCrossSectionalRadius != 0.0f )
	{
		// create inner points (unrotated)
		MakeArcCenterRadius(0.0f, 0.0f, flCrossSectionalRadius, flCrossSectionalRadius, 
			iSides, fStartAngle, fArc, fInnerPoints);
	}
	else
	{
		for( int i = 0; i < iSides; i++)
		{
			fInnerPoints[i][0] = fInnerPoints[i][1] = 0.0f;
		}
		bCreateSouthFace = FALSE;
	}

	// create group for segments
	CMapGroup *pGroup = new CMapGroup;

	TorusPointList_t innerPoints[2];
	TorusPointList_t outerPoints[2];
	TorusPointList_t *pStartInnerPoints;
	TorusPointList_t *pStartOuterPoints;
	TorusPointList_t *pEndInnerPoints = &innerPoints[1];
	TorusPointList_t *pEndOuterPoints = &outerPoints[1];
	int nCurrIndex = 0;

	float flCurrentZ = pBox->bmins[AXIS_Z] + iWallWidth + flCrossSectionalRadius;
	float flDeltaZ = (float)iAddHeight / (float)(iRotationSides);

	float flRotationAngle = fRotationStartAngle;
	float flRotationDeltaAngle = fRotationArc / iRotationSides;

	bool bIsCircle = ( iAddHeight == 0.0f ) && ( fRotationArc == 360.0f );
	++iRotationSides;
	for ( int i = 0; i != iRotationSides; ++i )
	{
		// This eliminates a seam in circular toruses
		if ( bIsCircle && (i == iRotationSides - 1) )
		{
			flRotationAngle = fRotationStartAngle;
		}

		float xCurrCenter, yCurrCenter;

		float flCosAngle = cos( DEG2RAD(flRotationAngle) );
		float flSinAngle = sin( DEG2RAD(flRotationAngle) );
		xCurrCenter = xCenter + xRad * flCosAngle;
		yCurrCenter = yCenter + yRad * flSinAngle;

		// Update buffers
		pStartInnerPoints = pEndInnerPoints;
		pStartOuterPoints = pEndOuterPoints;
		pEndInnerPoints = &innerPoints[nCurrIndex];
		pEndOuterPoints = &outerPoints[nCurrIndex];
		nCurrIndex = 1 - nCurrIndex;

		// Transform points into actual space.
		int jPrevPoint = -1;
		int j = 0;
		do
		{
			// x original is transformed into x/y based on rotation
			// y original is transformed into z
			(*pEndInnerPoints)[j][0] = xCurrCenter + fInnerPoints[j][0] * flCosAngle;
			(*pEndInnerPoints)[j][1] = yCurrCenter + fInnerPoints[j][0] * flSinAngle;
			(*pEndInnerPoints)[j][2] = flCurrentZ + fInnerPoints[j][1];

			(*pEndOuterPoints)[j][0] = xCurrCenter + fOuterPoints[j][0] * flCosAngle;
			(*pEndOuterPoints)[j][1] = yCurrCenter + fOuterPoints[j][0] * flSinAngle;
			(*pEndOuterPoints)[j][2] = flCurrentZ + fOuterPoints[j][1];

			// We'll use the j == 0 data when iNextPoint = iSides - 1
			if (( i != 0 ) && ( jPrevPoint != -1 ))
			{
				CMapSolid *pSolid = CreateSegment(
					*pStartOuterPoints, *pStartInnerPoints,
					*pEndOuterPoints, *pEndInnerPoints,
					jPrevPoint, j, bCreateSouthFace);

				pGroup->AddChild(pSolid);
			}

			jPrevPoint = j;
			++j;
		} while( jPrevPoint != iSides );

		flRotationAngle += flRotationDeltaAngle;
		flCurrentZ += flDeltaZ;

		if ( flRotationAngle >= 360.0f )
		{
			flRotationAngle -= 360.0f;
		}
	}

	pGroup->CalcBounds(TRUE);

	if (Options.general.bStretchArches)
	{
		// make sure size of group's bounds are size of original bounds -
		//  if not, scale up. this can happen when we use rotation.
		Vector objsize, boundsize;
		pBox->GetBoundsSize(boundsize);
		pGroup->GetBoundsSize(objsize);

		if (boundsize[AXIS_X] > objsize[AXIS_X] || 
			boundsize[AXIS_Y] > objsize[AXIS_Y])
		{
			Vector scale;
			scale[AXIS_X] = boundsize[AXIS_X] / objsize[AXIS_X];
			scale[AXIS_Y] = boundsize[AXIS_Y] / objsize[AXIS_Y];
			scale[AXIS_Z] = 1.0f;  // xxxYWB scaling by 0 causes veneers, so I changed to 1.0
			Vector center;
			pBox->GetBoundsCenter(center);
			pGroup->TransScale(center, scale);
		}
	}

	return pGroup;
}


