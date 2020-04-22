//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdio.h>
#include "convert.h"
#include "ivp_cache_object.hxx"
#include "coordsize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if 1
// game is in inches
vphysics_units_t g_PhysicsUnits = 
{ 
	METERS_PER_INCH, //float		unitScaleMeters;			// factor that converts game units to meters
	1.0f / METERS_PER_INCH, //float		unitScaleMetersInv;		// factor that converts meters to game units
	0.25f,			// float		globalCollisionTolerance;	// global collision tolerance in game units
	DIST_EPSILON,	// float		collisionSweepEpsilon;		// collision sweep tests clip at this, must be the same as engine's DIST_EPSILON
	1.0f/256.0f,	// float		collisionSweepIncrementalEpsilon;	// near-zero test for incremental steps in collision sweep tests
};
#else
// game is in meters
vphysics_units_t g_PhysicsUnits = 
{ 
	1.0f,			//float		unitScaleMeters;			// factor that converts game units to meters
	1.0f,			//float		unitScaleMetersInv;			// factor that converts meters to game units
	0.01f,			// float		globalCollisionTolerance;	// global collision tolerance in game units
	0.01f,			// float		collisionSweepEpsilon;		// collision sweep tests clip at this, must be the same as engine's DIST_EPSILON
	1e-4f,			// float		collisionSweepIncrementalEpsilon;	// near-zero test for incremental steps in collision sweep tests
};
#endif

//-----------------------------------------------------------------------------
// HL to IVP conversions
//-----------------------------------------------------------------------------

void ConvertBoxToIVP( const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs )
{
	float tmpZ;

	tmpZ = mins.y;
	outmins.y = -HL2IVP(mins.z);
	outmins.z = HL2IVP(tmpZ);
	outmins.x = HL2IVP(mins.x);
	tmpZ = maxs.y;
	outmaxs.y = -HL2IVP(maxs.z);
	outmaxs.z = HL2IVP(tmpZ);
	outmaxs.x = HL2IVP(maxs.x);

	tmpZ = outmaxs.y;
	outmaxs.y = outmins.y;
	outmins.y = tmpZ;
}


void ConvertMatrixToIVP( const matrix3x4_t& matrix, IVP_U_Matrix &out )
{
	Vector forward, left, up;

	forward.x = matrix[0][0];
	forward.y = matrix[1][0];
	forward.z = matrix[2][0];

	left.x = matrix[0][1];
	left.y = matrix[1][1];
	left.z = matrix[2][1];

	up.x = matrix[0][2];
	up.y = matrix[1][2];
	up.z = matrix[2][2];

	up = -up;

	IVP_U_Float_Point ivpForward, ivpLeft, ivpUp;

	ConvertDirectionToIVP( forward, ivpForward );
	ConvertDirectionToIVP( left, ivpLeft );
	ConvertDirectionToIVP( up, ivpUp );

	out.set_col( IVP_INDEX_X, &ivpForward );
	out.set_col( IVP_INDEX_Z, &ivpLeft );
	out.set_col( IVP_INDEX_Y, &ivpUp );

	out.vv.k[0] = HL2IVP(matrix[0][3]);
	out.vv.k[1] = -HL2IVP(matrix[2][3]);
	out.vv.k[2] = HL2IVP(matrix[1][3]);
}


void ConvertRotationToIVP( const QAngle& angles, IVP_U_Matrix3 &out )
{
	Vector forward, right, up;
	IVP_U_Float_Point ivpForward, ivpLeft, ivpUp;
	
	AngleVectors( angles, &forward, &right, &up );
	// now this is left
	right = -right;

	up = -up;
	
	ConvertDirectionToIVP( forward, ivpForward );
	ConvertDirectionToIVP( right, ivpLeft );
	ConvertDirectionToIVP( up, ivpUp );
	
	out.set_col( IVP_INDEX_X, &ivpForward );
	out.set_col( IVP_INDEX_Z, &ivpLeft );
	out.set_col( IVP_INDEX_Y, &ivpUp );
}

void ConvertRotationToIVP( const QAngle& angles, IVP_U_Quat &out )
{
	IVP_U_Matrix3 tmp;
	ConvertRotationToIVP( angles, tmp );
	out.set_quaternion( &tmp );
}

//-----------------------------------------------------------------------------
// IVP to HL conversions
//-----------------------------------------------------------------------------

void ConvertMatrixToHL( const IVP_U_Matrix &in, matrix3x4_t& output )
{
#if 1
	// copy the row vectors over, swapping z & -y.  Also, negate output z
	output[0][0] = in.get_elem(0, 0);
	output[0][2] = -in.get_elem(0, 1);
	output[0][1] = in.get_elem(0, 2);

	output[1][0] = in.get_elem(2, 0);
	output[1][2] = -in.get_elem(2, 1);
	output[1][1] = in.get_elem(2, 2);

	output[2][0] = -in.get_elem(1, 0);
	output[2][2] = in.get_elem(1, 1);
	output[2][1] = -in.get_elem(1, 2);

#else

	// this code is conceptually simpler, but the above is smaller/faster
	Vector forward, left, up;
	IVP_U_Float_Point out;

	in.get_col( IVP_INDEX_X, &out );
	ConvertDirectionToHL( out, forward );
	in.get_col( IVP_INDEX_Z, &out );
	ConvertDirectionToHL( out, left);
	in.get_col( IVP_INDEX_Y, &out );
	ConvertDirectionToHL( out, up );
	up = -up;

	output[0][0] = forward.x;
	output[1][0] = forward.y;
	output[2][0] = forward.z;

	output[0][1] = left.x;
	output[1][1] = left.y;
	output[2][1] = left.z;

	output[0][2] = up.x;
	output[1][2] = up.y;
	output[2][2] = up.z;
#endif
	output[0][3] = IVP2HL(in.vv.k[0]);
	output[1][3] = IVP2HL(in.vv.k[2]);
	output[2][3] = -IVP2HL(in.vv.k[1]);
}


void ConvertRotationToHL( const IVP_U_Matrix3 &in, QAngle& angles )
{
	IVP_U_Float_Point out;
	Vector forward, right, up;

	in.get_col( IVP_INDEX_X, &out );
	ConvertDirectionToHL( out, forward );
	in.get_col( IVP_INDEX_Z, &out );
	ConvertDirectionToHL( out, right );
	in.get_col( IVP_INDEX_Y, &out );
	ConvertDirectionToHL( out, up );

	float xyDist = sqrt( forward[0] * forward[0] + forward[1] * forward[1] );
	
	// enough here to get angles?
	if ( xyDist > 0.001 )
	{
		// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
		angles[1] = RAD2DEG( atan2( forward[1], forward[0] ) );

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );

		// (roll)	z = ATAN( -right.z, up.z );
		angles[2] = RAD2DEG( atan2( -right[2], up[2] ) ) + 180;
	}
	else	// forward is mostly Z, gimbal lock
	{
		// (yaw)	y = ATAN( -right.x, right.y );		-- forward is mostly z, so use right for yaw
		angles[1] = RAD2DEG( atan2( right[0], -right[1] ) );

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );

		// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
		angles[2] = 180;
	}
}


void ConvertRotationToHL( const IVP_U_Quat &in, QAngle& angles )
{
	IVP_U_Matrix3 tmp;
	in.set_matrix( &tmp );
	ConvertRotationToHL( tmp, angles );
}

// utiltiy code
void TransformIVPToLocal( IVP_U_Point &point, IVP_Real_Object *pObject, bool translate )
{
	IVP_U_Point tmp = point;
	TransformIVPToLocal( tmp, point, pObject, translate );
}

void TransformLocalToIVP( IVP_U_Point &point, IVP_Real_Object *pObject, bool translate )
{
	IVP_U_Point tmp = point;
	TransformLocalToIVP( tmp, point, pObject, translate );
}


// UNDONE: use IVP_Cache_Object instead?  Measure perf differences.
#define USE_CACHE_OBJECT	0


//-----------------------------------------------------------------------------
// Purpose: This is ONLY for use by the routines below.  It's not reentrant!!!
//			No threads or recursive calls!
//-----------------------------------------------------------------------------
#if USE_CACHE_OBJECT
#else
static const IVP_U_Matrix *GetTmpObjectMatrix( IVP_Real_Object *pObject )
{
	static IVP_U_Matrix coreShiftMatrix;
	const IVP_U_Matrix *pOut = pObject->get_core()->get_m_world_f_core_PSI();

	if ( !pObject->flags.shift_core_f_object_is_zero )
	{
		coreShiftMatrix.set_matrix( pOut );
		coreShiftMatrix.vmult4( pObject->get_shift_core_f_object(), &coreShiftMatrix.vv );
		return &coreShiftMatrix;
	}
	return pOut;
}
#endif

void TransformIVPToLocal( const IVP_U_Point &pointIn, IVP_U_Point &pointOut, IVP_Real_Object *pObject, bool translate )
{
#if USE_CACHE_OBJECT
	IVP_Cache_Object *cache = pObject->get_cache_object_no_lock();

	if ( translate )
	{
		cache->transform_position_to_object_coords( &pointIn, &pointOut );
	}
	else
	{
		cache->transform_vector_to_object_coords( &pointIn, &pointOut );
	}
#else
	const IVP_U_Matrix *pMatrix = GetTmpObjectMatrix( pObject );
	if ( translate )
	{
		pMatrix->inline_vimult4( &pointIn, &pointOut );
	}
	else
	{
		pMatrix->inline_vimult3( &pointIn, &pointOut );
	}
#endif
}


void TransformLocalToIVP( const IVP_U_Point &pointIn, IVP_U_Point &pointOut, IVP_Real_Object *pObject, bool translate )
{
#if USE_CACHE_OBJECT
	IVP_Cache_Object *cache = pObject->get_cache_object_no_lock();

	if ( translate )
	{
		IVP_U_Float_Point floatPointIn;
		floatPointIn.set( &pointIn );
		cache->transform_position_to_world_coords( &floatPointIn, &pointOut );
	}
	else
	{
		cache->transform_vector_to_world_coords( &pointIn, &pointOut );
	}
#else
	const IVP_U_Matrix *pMatrix = GetTmpObjectMatrix( pObject );

	if ( translate )
	{
		pMatrix->inline_vmult4( &pointIn, &pointOut );
	}
	else
	{
		pMatrix->inline_vmult3( &pointIn, &pointOut );
	}
#endif
}

void TransformLocalToIVP( const IVP_U_Float_Point &pointIn, IVP_U_Point &pointOut, IVP_Real_Object *pObject, bool translate )
{
#if USE_CACHE_OBJECT
	IVP_Cache_Object *cache = pObject->get_cache_object_no_lock();

	if ( translate )
	{
		cache->transform_position_to_world_coords( &pointIn, &pointOut );
	}
	else
	{
		IVP_U_Point doublePointIn;
		doublePointIn.set( &pointIn );
		cache->transform_vector_to_world_coords( &doublePointIn, &pointOut );
	}
#else
	const IVP_U_Matrix *pMatrix = GetTmpObjectMatrix( pObject );
	IVP_U_Float_Point out;

	if ( translate )
	{
		pMatrix->inline_vmult4( &pointIn, &out );
	}
	else
	{
		pMatrix->inline_vmult3( &pointIn, &out );
	}
	pointOut.set( &out );
#endif
}

void TransformLocalToIVP( const IVP_U_Float_Point &pointIn, IVP_U_Float_Point &pointOut, IVP_Real_Object *pObject, bool translate )
{
	IVP_U_Point tmpOut;
	TransformLocalToIVP( pointIn, tmpOut, pObject, translate );
	pointOut.set( &tmpOut );
}

static char axisMap[] = {0,2,1,3};

int ConvertCoordinateAxisToIVP( int axisIndex )
{
	return axisIndex < 4 ? axisMap[axisIndex] : 0;
}

int ConvertCoordinateAxisToHL( int axisIndex )
{
	return axisIndex < 4 ? axisMap[axisIndex] : 0;
}

