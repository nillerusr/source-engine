//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Math functions specific to the editor.
//
//===========================================================================//

#ifndef HAMMER_MATHLIB_H
#define HAMMER_MATHLIB_H

#ifdef _WIN32
#pragma once
#endif

typedef unsigned char byte;

#include "mathlib/mathlib.h"
#include "mathlib/vmatrix.h"
#include <math.h>


typedef vec_t vec5_t[5];

enum
{
	AXIS_X = 0,
	AXIS_Y,
	AXIS_Z
};

//
// Matrix functions:
//

void RotateAroundAxis(VMatrix& Matrix, float fDegrees, int nAxis);
void AxisAngleMatrix(VMatrix& Matrix, const Vector &Axis, float fAngle);

float fixang(float a);
float lineangle(float x1, float y1, float x2, float y2);
void polyMake( float x1, float  y1, float x2, float y2, int npoints, float start_ang, Vector *pmPoints );
float V_rint(float);

inline int fsign( float x) { if(x==0) return 0; else if (x>0) return 1; else return -1; }
inline bool fequal( float value, float target, float delta) { return ( (value<(target+delta))&&(value>(target-delta)) ); }

void RoundVector( Vector2D &v );
bool IsLineInside(const Vector2D &pt1, const Vector2D &pt2, int x1, int y1, int x2, int y2);
bool IsPointInside(const Vector2D &pt, const Vector2D &mins, const Vector2D &maxs );

bool IsValidBox( Vector &mins, Vector &maxs );
bool IsValidBox( const Vector2D &mins, const Vector2D &maxs );
void NormalizeBox( Vector &mins, Vector &maxs );
void NormalizeBox( Vector2D &mins, Vector2D &maxs );
void PointsFromBox( const Vector &mins, const Vector &maxs, Vector *points );
void LimitBox( Vector &mins, Vector &maxs, float limit );
void PointsRevertOrder( Vector *pPoints, int nPoints);
// Is box 1 inside box 2?
bool IsBoxInside( const Vector2D &min1, const Vector2D &max1, const Vector2D &min2, const Vector2D &max2 );
bool IsBoxIntersecting( const Vector2D &min1, const Vector2D &max1, const Vector2D &min2, const Vector2D &max2 );

const Vector &GetNormalFromPoints( const Vector &p1, const Vector &p2, const Vector &p3 );
const Vector &GetNormalFromFace( int nFace );

inline void TransformPoint( const VMatrix& matrix, Vector &point )
{
	Vector orgVector = point;
	matrix.V3Mul( orgVector, point );

}

// solve equation v0 = x*v1 + y*v2 + z*v3
void	GetAxisFromFace( int nFace, Vector& vHorz, Vector &vVert, Vector &vThrd );
bool SolveLinearEquation( const Vector& v0, const Vector& v1, const Vector& v2, const Vector& v3, Vector& vOut);
// test intersection line & AABB, returns -1 if no intersection occurs
float IntersectionLineAABBox( const Vector& mins, const Vector& maxs, const Vector& vStart, const Vector& vEnd, int &nFace );

bool BuildAxesFromNormal( const Vector &vNormal, Vector &vHorz, Vector &vVert );

#endif // HAMMER_MATHLIB_H
