//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Math functions specific to the editor.
//
//=============================================================================//

#include "hammer_mathlib.h"
#include <string.h>
#include <Windows.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// provide implementation for mathlib Sys_Error()
extern void Error(char* fmt, ...);
extern "C" void Sys_Error( char *error, ... )
{
	Error( "%s", error );
}

float V_rint(float f)
{
	if (f > 0.0f) {
		return (float) floor(f + 0.5f);
	} else if (f < 0.0f) {
		return (float) ceil(f - 0.5f);
	} else
		return 0.0f;
}

static int s_BoxFaces[6][3] =
{
	{ 0, 4, 2 },
	{ 4, 5, 6 },
	{ 5, 1, 7 },
	{ 1, 0, 3 },
	{ 2, 6, 3 },
	{ 5, 4, 1 },
};

void polyMake( float x1, float  y1, float x2, float y2, int npoints, float start_ang, Vector *pmPoints )
{
    int		point;
    double  angle = start_ang, angle_delta = 360.0 / (double) npoints;
    double  xrad = (x2-x1) / 2, yrad = (y2-y1) / 2;

	// make centerpoint for polygon:
    float xCenter = x1 + xrad;
    float yCenter = y1 + yrad;

    for( point = 0; point < npoints; point++, angle += angle_delta )
    {
        if( angle > 360 )
            angle -= 360;

        pmPoints[point][0] = V_rint(xCenter + (sin(DEG2RAD(angle)) * (float)xrad));
        pmPoints[point][1] = V_rint(yCenter + (cos(DEG2RAD(angle)) * (float)yrad));
    }

    pmPoints[point][0] = pmPoints[0][0];
    pmPoints[point][1] = pmPoints[0][1];
}


float fixang(float a)
{
	if(a < 0.0)
		return a+360.0;
	if(a > 359.9)
		return a-360.0;

	return a;
}


float lineangle(float x1, float y1, float x2, float y2)
{
    float x, y;
	float rvl;

    x = x2 - x1;
    y = y2 - y1;

    if(!x && !y)
        return 0.0;

    rvl = RAD2DEG(atan2( y, x ));

    return (rvl);
}


//-----------------------------------------------------------------------------
// Purpose: Builds the matrix for a counterclockwise rotation about an arbitrary axis.
//
//		   | ax2 + (1 - ax2)cosQ		axay(1 - cosQ) - azsinQ		azax(1 - cosQ) + aysinQ |
// Ra(Q) = | axay(1 - cosQ) + azsinQ	ay2 + (1 - ay2)cosQ			ayaz(1 - cosQ) - axsinQ |
//		   | azax(1 - cosQ) - aysinQ	ayaz(1 - cosQ) + axsinQ		az2 + (1 - az2)cosQ     |
//          
// Input  : Matrix - 
//			Axis - 
//			fAngle - 
//-----------------------------------------------------------------------------
void AxisAngleMatrix(VMatrix& Matrix, const Vector& Axis, float fAngle)
{
	float fRadians;
	float fAxisXSquared;
	float fAxisYSquared;
	float fAxisZSquared;
	float fSin;
	float fCos;

	fRadians = fAngle * M_PI / 180.0;

	fSin = sin(fRadians);
	fCos = cos(fRadians);

	fAxisXSquared = Axis[0] * Axis[0];
	fAxisYSquared = Axis[1] * Axis[1];
	fAxisZSquared = Axis[2] * Axis[2];

	// Column 0:
	Matrix[0][0] = fAxisXSquared + (1 - fAxisXSquared) * fCos;
	Matrix[1][0] = Axis[0] * Axis[1] * (1 - fCos) + Axis[2] * fSin;
	Matrix[2][0] = Axis[2] * Axis[0] * (1 - fCos) - Axis[1] * fSin;
	Matrix[3][0] = 0;

	// Column 1:
	Matrix[0][1] = Axis[0] * Axis[1] * (1 - fCos) - Axis[2] * fSin;
	Matrix[1][1] = fAxisYSquared + (1 - fAxisYSquared) * fCos;
	Matrix[2][1] = Axis[1] * Axis[2] * (1 - fCos) + Axis[0] * fSin;
	Matrix[3][1] = 0;

	// Column 2:
	Matrix[0][2] = Axis[2] * Axis[0] * (1 - fCos) + Axis[1] * fSin;
	Matrix[1][2] = Axis[1] * Axis[2] * (1 - fCos) - Axis[0] * fSin;
	Matrix[2][2] = fAxisZSquared + (1 - fAxisZSquared) * fCos;
	Matrix[3][2] = 0;

	// Column 3:
	Matrix[0][3] = 0;
	Matrix[1][3] = 0;
	Matrix[2][3] = 0;
	Matrix[3][3] = 1;
}


void RotateAroundAxis(VMatrix& Matrix, float fDegrees, int nAxis)
{
	int a,b;

	if ( fDegrees == 0 )
		return;

	if ( nAxis == 0 )
	{
		a=1; b=2;
	}
	else if ( nAxis == 1)
	{
		a=0;b=2;
	}
	else
	{
		a=0; b=1;
	}

	float fRadians = DEG2RAD(fDegrees);

	float fSin = (float)sin(fRadians);
	float fCos = (float)cos(fRadians);

	if ( nAxis == 1 )
		fSin = -fSin;

	float Temp0a = Matrix[0][a] * fCos + Matrix[0][b] * fSin;
	float Temp1a = Matrix[1][a] * fCos + Matrix[1][b] * fSin;
	float Temp2a = Matrix[2][a] * fCos + Matrix[2][b] * fSin;
	float Temp3a = Matrix[3][a] * fCos + Matrix[3][b] * fSin;

	if ( nAxis == 1 )
		fSin = -fSin;

	float Temp0b = Matrix[0][a] * -fSin + Matrix[0][b] * fCos;
	float Temp1b = Matrix[1][a] * -fSin + Matrix[1][b] * fCos;
	float Temp2b = Matrix[2][a] * -fSin + Matrix[2][b] * fCos;
	float Temp3b = Matrix[3][a] * -fSin + Matrix[3][b] * fCos;

	Matrix[0][a] = Temp0a;
	Matrix[1][a] = Temp1a;
	Matrix[2][a] = Temp2a;
	Matrix[3][a] = Temp3a;

	Matrix[0][b] = Temp0b;
	Matrix[1][b] = Temp1b;
	Matrix[2][b] = Temp2b;
	Matrix[3][b] = Temp3b;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt1 - 
//			pt2 - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
bool IsLineInside(const Vector2D &pt1, const Vector2D &pt2, int x1, int y1, int x2, int y2)
{
    int lx1 = pt1.x;
    int ly1 = pt1.y;
    int lx2 = pt2.x;
    int ly2 = pt2.y;
    int i;

    // is the line totally on one side of the box?
    if( (lx2 > x2 && lx1 > x2) ||
        (lx2 < x1 && lx1 < x1) ||
        (ly2 > y2 && ly1 > y2) ||
        (ly2 < y1 && ly1 < y1) )
        return false;

    if( lx1 >= x1 && lx1 <= x2 && ly1 >= y1 && ly1 <= y2 )
        return true; // the first point is inside the box

    if( lx2 >= x1 && lx2 <= x2 && ly2 >= y1 && ly2 <= y2 )
        return true; // the second point is inside the box

    if( (ly1 > y1) != (ly2 > y1) )
    {
        i = lx1 + (int) ( (long) (y1 - ly1) * (long) (lx2 - lx1) / (long) (ly2 - ly1));
        if( i >= x1 && i <= x2 )
            return true; // the line crosses the y1 side (left)
    }

    if( (ly1 > y2) != (ly2 > y2))
    {
        i = lx1 + (int) ( (long) (y2 - ly1) * (long) (lx2 - lx1) / (long) (ly2 - ly1));
        if( i >= x1 && i <= x2 )
            return true; // the line crosses the y2 side (right)
    }

    if( (lx1 > x1) != (lx2 > x1))
    {
        i = ly1 + (int) ( (long) (x1 - lx1) * (long) (ly2 - ly1) / (long) (lx2 - lx1));
        if( i >= y1 && i <= y2 )
            return true; // the line crosses the x1 side (down)
    }

    if( (lx1 > x2) != (lx2 > x2))
    {
        i = ly1 + (int) ( (long) (x2 - lx1) * (long) (ly2 - ly1) / (long) (lx2 - lx1));
        if( i >= y1 && i <= y2 )
            return true; // the line crosses the x2 side (up)
    }

    // The line does not intersect the box.
    return false;
}

bool IsPointInside(const Vector2D &pt, const Vector2D &mins, const Vector2D &maxs )
{
	return ( pt.x >= mins.x ) && ( pt.y >= mins.y ) && ( pt.x <= maxs.x ) && ( pt.y <= maxs.y );
}

// Is box 1 inside box 2?
bool IsBoxInside( const Vector2D &min1, const Vector2D &max1, const Vector2D &min2, const Vector2D &max2 )
{
	if ( ( min1.x < min2.x ) || ( max1.x > max2.x ) )
		return false;

	if ( ( min1.y < min2.y ) || ( max1.y > max2.y ) )
		return false;

	return true;
}

bool IsBoxIntersecting( const Vector2D &min1, const Vector2D &max1, const Vector2D &min2, const Vector2D &max2 )
{
	if ( ( min1.x >= max2.x ) || ( max1.x <= min2.x ) )
		return false;

	if ( ( min1.y >= max2.y ) || ( max1.y <= min2.y ) )
		return false;

	return true;
}

void NormalizeBox( Vector &mins, Vector &maxs )
{
	for (int i=0; i<3; i++ )
	{
		if ( mins[i] > maxs[i])
		{
			V_swap( mins[i], maxs[i] );
		}
	}
}

void NormalizeBox( Vector2D &mins, Vector2D &maxs )
{
	if ( mins.x > maxs.x )
	{
		V_swap( mins.x, maxs.x );
	}
	if ( mins.y > maxs.y )
	{
		V_swap( mins.y, maxs.y );
	}
}


bool IsValidBox( Vector &mins, Vector &maxs )
{
	return ( mins.x <= maxs.x ) && ( mins.y <= maxs.y ) && ( mins.z <= maxs.z );
}

bool IsValidBox( const Vector2D &mins, const Vector2D &maxs )
{
	return ( mins.x <= maxs.x ) && ( mins.y <= maxs.y );
}

void LimitBox( Vector &mins, Vector &maxs, float limit )
{
	for ( int i=0; i<3;i++)
	{
		if ( mins[i] < -limit )
			mins[i] = -limit;

		if ( maxs[i] > limit )
			maxs[i] = limit;
	}
}

void GetAxisFromFace( int nFace, Vector& vHorz, Vector &vVert, Vector &vThrd )
{
	Assert( nFace >= 0 && nFace < 6);

	Vector points[8];
	PointsFromBox( Vector(0,0,0), Vector(1,1,1), points );

	Vector p1 = points[s_BoxFaces[nFace][0]];
	Vector p2 = points[s_BoxFaces[nFace][1]];
	Vector p3 = points[s_BoxFaces[nFace][2]];

	// compose equation
	vHorz = p2 - p1;
	vVert = p3 - p1;
	vThrd = CrossProduct( vHorz, vVert );
}

//
// Generate the corner points of a box:
//    3---7   
//   /|  /|
//  / | / |
// 2---6  |
// |  1|--5
// | / | /
// |/  |/
// 0---4 

void PointsFromBox( const Vector &mins, const Vector &maxs, Vector *points )
{
	points[0][0] = mins[0];
	points[0][1] = mins[1];
	points[0][2] = mins[2];

	points[1][0] = mins[0];
	points[1][1] = mins[1];
	points[1][2] = maxs[2];

	points[2][0] = mins[0];
	points[2][1] = maxs[1];
	points[2][2] = mins[2];

	points[3][0] = mins[0];
	points[3][1] = maxs[1];
	points[3][2] = maxs[2];

	points[4][0] = maxs[0];
	points[4][1] = mins[1];
	points[4][2] = mins[2];

	points[5][0] = maxs[0];
	points[5][1] = mins[1];
	points[5][2] = maxs[2];

	points[6][0] = maxs[0];
	points[6][1] = maxs[1];
	points[6][2] = mins[2];

	points[7][0] = maxs[0];
	points[7][1] = maxs[1];
	points[7][2] = maxs[2];
}

float IntersectionLineAABBox( const Vector& mins, const Vector& maxs, const Vector& vStart, const Vector& vEnd, int &nFace )
{
	Vector vz = vEnd - vStart;

	// quick distance check first
	Vector vCenter = (mins+maxs)/2;
	Vector vTmp = maxs-vCenter;
	float  radius = DotProduct(vTmp,vTmp);
	vTmp = CrossProduct(vz,(vStart-vCenter));
	float  dist =  DotProduct( vTmp,vTmp ) / DotProduct( vz,vz );

	nFace = -1; 

	if ( dist > radius )
	{
		return -1; 
	}

	// ok, now check against all 6 faces
	Vector points[8];
	PointsFromBox( mins, maxs, points );
	
	vz = -vz;
	
	float fDistance = 999999;
	
	for ( int i=0; i<6; i++ )
	{
		// get points of face
		Vector p1 = points[s_BoxFaces[i][0]];
		Vector p2 = points[s_BoxFaces[i][1]];
		Vector p3 = points[s_BoxFaces[i][2]];

		// compose equation
		Vector v0 = vStart - p1;
		Vector vx = p2 - p1;
		Vector vy = p3 - p1;

		Vector vOut;
		
		// solve equation v0 = x*v1 + y*v2 + z*v3
		if ( !SolveLinearEquation( v0, vx, vy, vz, vOut) )
			continue;

		if ( vOut.z < 0 || vOut.z > 1 )
			continue;

		if ( vOut.x < 0 || vOut.x > 1 )
			continue;

		if ( vOut.y < 0 || vOut.y > 1 )
			continue;

		if ( vOut.z < fDistance )
		{
			nFace = i;
			fDistance = vOut.z;
		}
	}

	if ( nFace >= 0 )
	{
		return fDistance*VectorLength(vz);
	}
	else
	{
		return -1;
	}
}




void RoundVector( Vector2D &v )
{
	v.x = (int)(v.x+0.5f);
	v.y = (int)(v.y+0.5f);
}

void PointsRevertOrder( Vector *pPoints, int nPoints)
{
	Vector *tmpPoints = (Vector*)_alloca( sizeof(Vector)*nPoints );
	memcpy( tmpPoints, pPoints, sizeof(Vector)*nPoints );
	for ( int i = 0; i<nPoints; i++)
	{
		pPoints[i] = tmpPoints[nPoints-i-1];
	}
}

const Vector &GetNormalFromFace( int nFace )
{
	// ok, now check against all 6 faces
	
	Vector points[8];

	Assert( nFace>=0 && nFace<6 );

	PointsFromBox( Vector(0,0,0), Vector(1,1,1), points );

 	return GetNormalFromPoints( points[s_BoxFaces[nFace][0]], points[s_BoxFaces[nFace][1]],points[s_BoxFaces[nFace][2]] );
}

const Vector &GetNormalFromPoints( const Vector &p0, const Vector &p1, const Vector &p2 )
{
	static Vector vNormal;
	Vector v1 = p0 - p1;
	Vector v2 = p2 - p1;
	CrossProduct(v1, v2, vNormal);
	VectorNormalize(vNormal);
	return vNormal;
}

// solve equation v0 = x*v1 + y*v2 + z*v3
bool SolveLinearEquation( const Vector& v0, const Vector& v1, const Vector& v2, const Vector& v3, Vector& vOut)
{
	VMatrix matrix, inverse;
	matrix.Init(
		v1.x, v1.y, v1.z, 0,
		v2.x, v2.y, v2.z, 0,
		v3.x, v3.y, v3.z, 0,
		0.0f, 0.0f, 0.0f, 1
		);

	if( !matrix.InverseGeneral(inverse) )
		return false;
	
	vOut = inverse.VMul3x3Transpose( v0 );
	return true;
}

bool BuildAxesFromNormal( const Vector &vNormal, Vector &vHorz, Vector &vVert )
{
	vHorz.Init();
	vVert.Init();

	// find the major axis
	float bestMin = 99999;
	int bestAxis = -1;
	for (int i=0 ; i<3; i++)
	{
		float a = fabs(vNormal[i]);
		if (a < bestMin)
		{
			bestAxis = i;
			bestMin = a;
		}
	}

	if (bestAxis==-1)
		return false;
	
	vHorz[bestAxis] = 1;
	
	CrossProduct( vNormal,vHorz,vVert);
	CrossProduct( vNormal,vVert,vHorz);

	VectorNormalize( vHorz );
	VectorNormalize( vVert );

	return true;
}
