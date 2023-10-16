//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CONVERT_H
#define CONVERT_H
#pragma once

#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "ivp_physics.hxx"
struct cplane_t;
#include "vphysics_interface.h"

// UNDONE: Remove all conversion/scaling
// Convert our units (inches) to IVP units (meters)
struct vphysics_units_t
{
	float		unitScaleMeters;			// factor that converts game units to meters
	float		unitScaleMetersInv;			// factor that converts meters to game units
	float		globalCollisionTolerance;	// global collision tolerance in game units
	float		collisionSweepEpsilon;		// collision sweep tests clip at this, must be the same as engine's DIST_EPSILON
	float		collisionSweepIncrementalEpsilon;	// near-zero test for incremental steps in collision sweep tests
};

extern vphysics_units_t g_PhysicsUnits;

#define HL2IVP_FACTOR	g_PhysicsUnits.unitScaleMeters
#define IVP2HL(x)		(float)(x * (g_PhysicsUnits.unitScaleMetersInv))
#define HL2IVP(x)		(double)(x * HL2IVP_FACTOR)

// Convert HL engine units to IVP units
inline void ConvertPositionToIVP( const Vector &in, IVP_U_Float_Point &out )
{
	float tmpZ;

	tmpZ = in[1];

	out.k[0] = HL2IVP(in[0]);
	out.k[1] = -HL2IVP(in[2]);
	out.k[2] = HL2IVP(tmpZ);
}

inline void ConvertPositionToIVP( const Vector &in, IVP_U_Point &out )
{
	float tmpZ;

	tmpZ = in[1];

	out.k[0] = HL2IVP(in[0]);
	out.k[1] = -HL2IVP(in[2]);
	out.k[2] = HL2IVP(tmpZ);
}

inline void ConvertPositionToIVP( const Vector &in, IVP_U_Float_Point3 &out )
{
	float tmpZ;

	tmpZ = in[1];

	out.k[0] = HL2IVP(in[0]);
	out.k[1] = -HL2IVP(in[2]);
	out.k[2] = HL2IVP(tmpZ);
}

inline void ConvertPositionToIVP( float &x, float &y, float &z )
{
	float tmpZ;

	tmpZ = y;
	y = -HL2IVP(z);
	z = HL2IVP(tmpZ);
	x = HL2IVP(x);
}

inline void ConvertDirectionToIVP( const Vector &in, IVP_U_Float_Point &out )
{
	float tmpZ;

	tmpZ = in[1];

	out.k[0] = in[0];
	out.k[1] = -in[2];
	out.k[2] = tmpZ;
}


inline void ConvertDirectionToIVP( const Vector &in, IVP_U_Point &out )
{
	float tmpZ;

	tmpZ = in[1];

	out.k[0] = in[0];
	out.k[1] = -in[2];
	out.k[2] = tmpZ;
}


// forces are handled the same as positions & velocities (scaled by distance conversion factor)
#define ConvertForceImpulseToIVP ConvertPositionToIVP
#define ConvertForceImpulseToHL ConvertPositionToHL

inline float ConvertAngleToIVP( float angleIn )
{
	return DEG2RAD(angleIn);
}

inline void ConvertAngularImpulseToIVP( const AngularImpulse &in, IVP_U_Float_Point &out )
{
	float tmpZ;

	tmpZ = in[1];

	out.k[0] = DEG2RAD(in[0]);
	out.k[1] = -DEG2RAD(in[2]);
	out.k[2] = DEG2RAD(tmpZ);
}


inline float ConvertDistanceToIVP( float distance )
{
	return HL2IVP( distance );
}

inline void ConvertPlaneToIVP( const Vector &pNormal, float dist, IVP_U_Hesse &plane )
{
	ConvertDirectionToIVP( pNormal, (IVP_U_Point &)plane );
	// HL stores planes as Ax + By + Cz = D
	// IVP stores them as  Ax + BY + Cz + D = 0
	plane.hesse_val = -ConvertDistanceToIVP( dist );
}


inline void ConvertPlaneToIVP( const Vector &pNormal, float dist, IVP_U_Float_Hesse &plane )
{
	ConvertDirectionToIVP( pNormal, (IVP_U_Float_Point &)plane );
	// HL stores planes as Ax + By + Cz = D
	// IVP stores them as  Ax + BY + Cz + D = 0
	plane.hesse_val = -ConvertDistanceToIVP( dist );
}

inline float ConvertDensityToIVP( float density )
{
	return density;
}

// in convert.cpp
extern void ConvertMatrixToIVP( const matrix3x4_t& matrix, IVP_U_Matrix &out );
extern void ConvertRotationToIVP( const QAngle &angles, IVP_U_Matrix3 &out );
extern void ConvertRotationToIVP( const QAngle& angles, IVP_U_Quat &out );
extern void ConvertBoxToIVP( const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs );
extern int ConvertCoordinateAxisToIVP( int axisIndex );
extern int ConvertCoordinateAxisToHL( int axisIndex );

// IVP to HL conversions
inline void ConvertPositionToHL( const IVP_U_Point &point, Vector& out )
{
	float tmpY = IVP2HL(point.k[2]);
	out[2] = -IVP2HL(point.k[1]);
	out[1] = tmpY;
	out[0] = IVP2HL(point.k[0]);
}

inline void ConvertPositionToHL( const IVP_U_Float_Point &point, Vector& out )
{
	float tmpY = IVP2HL(point.k[2]);
	out[2] = -IVP2HL(point.k[1]);
	out[1] = tmpY;
	out[0] = IVP2HL(point.k[0]);
}

inline void ConvertPositionToHL( const IVP_U_Float_Point3 &point, Vector& out )
{
	float tmpY = IVP2HL(point.k[2]);
	out[2] = -IVP2HL(point.k[1]);
	out[1] = tmpY;
	out[0] = IVP2HL(point.k[0]);
}

inline void ConvertDirectionToHL( const IVP_U_Point &point, Vector& out )
{
	float tmpY = point.k[2];
	out[2] = -point.k[1];
	out[1] = tmpY;
	out[0] = point.k[0];
}


inline void ConvertDirectionToHL( const IVP_U_Float_Point &point, Vector& out )
{
	float tmpY = point.k[2];
	out[2] = -point.k[1];
	out[1] = tmpY;
	out[0] = point.k[0];
}


inline float ConvertAngleToHL( float angleIn )
{
	return RAD2DEG(angleIn);
}

inline void ConvertAngularImpulseToHL( const IVP_U_Float_Point &point, AngularImpulse &out )
{
	float tmpY = point.k[2];
	out[2] = -RAD2DEG(point.k[1]);
	out[1] = RAD2DEG(tmpY);
	out[0] = RAD2DEG(point.k[0]);
}

inline float ConvertDistanceToHL( float distance )
{
	return IVP2HL( distance );
}


// NOTE: Converts in place
inline void ConvertPlaneToHL( cplane_t &plane )
{
	IVP_U_Float_Hesse tmp(plane.normal.x, plane.normal.y, plane.normal.z, -plane.dist);
	ConvertDirectionToHL( (IVP_U_Float_Point &)tmp, plane.normal );
	// HL stores planes as Ax + By + Cz = D
	// IVP stores them as  Ax + BY + Cz + D = 0
	plane.dist = -ConvertDistanceToHL( tmp.hesse_val );
}

inline void ConvertPlaneToHL( const IVP_U_Float_Hesse &plane, Vector *pNormalOut, float *pDistOut )
{
	if ( pNormalOut )
	{
		ConvertDirectionToHL( plane, *pNormalOut );
	}
	// HL stores planes as Ax + By + Cz = D
	// IVP stores them as  Ax + BY + Cz + D = 0
	if ( pDistOut )
	{
		*pDistOut = -ConvertDistanceToHL( plane.hesse_val );
	}
}

inline float ConvertVolumeToHL( float volume )
{
	float factor = IVP2HL(1.0);
	factor = (factor * factor * factor);
	return factor * volume;
}

#define INSQR_PER_METERSQR (1.f / (METERS_PER_INCH*METERS_PER_INCH))
inline float ConvertEnergyToHL( float energy )
{
	return energy * INSQR_PER_METERSQR;
}

inline void IVP_Float_PointAbs( IVP_U_Float_Point &out, const IVP_U_Float_Point &in )
{
	out.k[0] = fabsf( in.k[0] );
	out.k[1] = fabsf( in.k[1] );
	out.k[2] = fabsf( in.k[2] );
}

// convert.cpp
extern void ConvertRotationToHL( const IVP_U_Matrix3 &in, QAngle &angles );
extern void ConvertMatrixToHL( const IVP_U_Matrix &in, matrix3x4_t& output );
extern void ConvertRotationToHL( const IVP_U_Quat &in, QAngle& angles );

extern void TransformIVPToLocal( IVP_U_Point &pointInOut, IVP_Real_Object *pObject, bool translate );
extern void TransformLocalToIVP( IVP_U_Point &pointInOut, IVP_Real_Object *pObject, bool translate );

extern void TransformIVPToLocal( const IVP_U_Point &pointIn, IVP_U_Point &pointOut, IVP_Real_Object *pObject, bool translate );
extern void TransformLocalToIVP( const IVP_U_Point &pointIn, IVP_U_Point &pointOut, IVP_Real_Object *pObject, bool translate );

extern void TransformLocalToIVP( const IVP_U_Float_Point &pointIn, IVP_U_Point &pointOut, IVP_Real_Object *pObject, bool translate );
extern void TransformLocalToIVP( const IVP_U_Float_Point &pointIn, IVP_U_Float_Point &pointOut, IVP_Real_Object *pObject, bool translate );

#endif // CONVERT_H
