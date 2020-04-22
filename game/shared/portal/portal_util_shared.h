//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_UTIL_SHARED_H
#define PORTAL_UTIL_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "engine/IEngineTrace.h"

extern bool g_bBulletPortalTrace;

#ifdef CLIENT_DLL
	#include "client_class.h"
	#include "interpolatedvar.h"
	class C_Prop_Portal;
	typedef C_Prop_Portal CProp_Portal;
	class C_Beam;
	typedef C_Beam CBeam;
#else
	class CProp_Portal;
	class CBeam;
#endif

Color UTIL_Portal_Color( int iPortal );

void UTIL_Portal_Trace_Filter( class CTraceFilterSimpleClassnameList *traceFilterPortalShot );

CProp_Portal* UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fMustBeCloserThan );

bool UTIL_Portal_TraceRay_Bullets( const CProp_Portal *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true );
CProp_Portal* UTIL_Portal_TraceRay_Beam( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, float *pfFraction );
bool UTIL_Portal_Trace_Beam( const CBeam *pBeam, Vector &vecStart, Vector &vecEnd, Vector &vecIntersectionStart, Vector &vecIntersectionEnd, ITraceFilter *pTraceFilter );

void UTIL_Portal_TraceRay_With( const CProp_Portal *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true );
CProp_Portal* UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true ); //traces a ray normally, then sees if portals have anything to say about it
CProp_Portal* UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall = true );

void UTIL_Portal_TraceRay( const CProp_Portal *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true ); //traces against a specific portal's environment, does no *real* tracing
void UTIL_Portal_TraceRay( const CProp_Portal *pPortal, const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall = true );

void UTIL_PortalLinked_TraceRay( const CProp_Portal *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true ); //traces against a specific portal's environment, does no *real* tracing
void UTIL_PortalLinked_TraceRay( const CProp_Portal *pPortal, const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall = true );

// tests if a ray's trace hits any portals
bool UTIL_DidTraceTouchPortals ( const Ray_t& ray, const trace_t& trace, CProp_Portal** pOutLocal = NULL, CProp_Portal** pOutRemote = NULL );

// Version of the TraceEntity functions which trace through portals
void UTIL_Portal_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
							 unsigned int mask, ITraceFilter *pFilter, trace_t *ptr );

void UTIL_Portal_PointTransform( const VMatrix matThisToLinked, const Vector &ptSource, Vector &ptTransformed );
void UTIL_Portal_VectorTransform( const VMatrix matThisToLinked, const Vector &vSource, Vector &vTransformed );
void UTIL_Portal_AngleTransform( const VMatrix matThisToLinked, const QAngle &qSource, QAngle &qTransformed );
void UTIL_Portal_RayTransform( const VMatrix matThisToLinked, const Ray_t &raySource, Ray_t &rayTransformed );
void UTIL_Portal_PlaneTransform( const VMatrix matThisToLinked, const cplane_t &planeSource, cplane_t &planeTransformed );
void UTIL_Portal_PlaneTransform( const VMatrix matThisToLinked, const VPlane &planeSource, VPlane &planeTransformed );

void UTIL_Portal_Triangles( const Vector &ptPortalCenter, const QAngle &qPortalAngles, Vector pvTri1[ 3 ], Vector pvTri2[ 3 ] );
void UTIL_Portal_Triangles( const CProp_Portal *pPortal, Vector pvTri1[ 3 ], Vector pvTri2[ 3 ] );
void UTIL_Portal_AABB( const CProp_Portal *pPortal, Vector &vMin, Vector &vMax );

float UTIL_Portal_DistanceThroughPortal( const CProp_Portal *pPortal, const Vector &vPoint1, const Vector &vPoint2 );
float UTIL_Portal_DistanceThroughPortalSqr( const CProp_Portal *pPortal, const Vector &vPoint1, const Vector &vPoint2 );
float UTIL_Portal_ShortestDistance( const Vector &vPoint1, const Vector &vPoint2, CProp_Portal **pShortestDistPortal_Out = NULL, bool bRequireStraightLine = false );
float UTIL_Portal_ShortestDistanceSqr( const Vector &vPoint1, const Vector &vPoint2, CProp_Portal **pShortestDistPortal_Out = NULL, bool bRequireStraightLine = false );

//-----------------------------------------------------------------------------
//
// UTIL_IntersectRayWithPortal
//
// Intersects a ray with a portal, returns distance t along ray.
// t will be less than zero if no intersection occurred
//
//-----------------------------------------------------------------------------
float UTIL_IntersectRayWithPortal( const Ray_t &ray, const CProp_Portal *pPortal );

bool UTIL_IntersectRayWithPortalOBB( const CProp_Portal *pPortal, const Ray_t &ray, trace_t *pTrace );
bool UTIL_IntersectRayWithPortalOBBAsAABB( const CProp_Portal *pPortal, const Ray_t &ray, trace_t *pTrace );

bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const Vector &ptPortalCenter, const QAngle &qPortalAngles, float flTolerance = 0.0f );
bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const CProp_Portal *pPortal, float flTolerance = 0.0f );

CProp_Portal *UTIL_IntersectEntityExtentsWithPortal( const CBaseEntity *pEntity );

void UTIL_Portal_NDebugOverlay( const Vector &ptPortalCenter, const QAngle &qPortalAngles, int r, int g, int b, int a, bool noDepthTest, float duration );
void UTIL_Portal_NDebugOverlay( const CProp_Portal *pPortal, int r, int g, int b, int a, bool noDepthTest, float duration );

bool FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask = MASK_SOLID ); //assumes the object is already in a mostly passable space

#ifdef CLIENT_DLL
void UTIL_TransformInterpolatedAngle( CInterpolatedVar< QAngle > &qInterped, matrix3x4_t matTransform, bool bSkipNewest );
void UTIL_TransformInterpolatedPosition( CInterpolatedVar< Vector > &vInterped, VMatrix matTransform, bool bSkipNewest );
#endif

bool UTIL_Portal_EntityIsInPortalHole( const CProp_Portal *pPortal, CBaseEntity *pEntity );

#endif //#ifndef PORTAL_UTIL_SHARED_H

