//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: sheet code for particles and other sprite functions
//
//===========================================================================//

#ifndef PARTICLES_INTERNAL_H
#define PARTICLES_INTERNAL_H

#ifdef _WIN32
#pragma once
#endif


#include "tier1/UtlStringMap.h"
#include "tier1/utlbuffer.h"
#include "tier2/fileutils.h"

#define MAX_WORLD_PLANAR_CONSTRAINTS ( 26 + 5 + 10 )

#define COLLISION_MODE_PER_PARTICLE_TRACE 0
#define COLLISION_MODE_PER_FRAME_PLANESET 1
#define COLLISION_MODE_INITIAL_TRACE_DOWN 2
#define COLLISION_MODE_USE_NEAREST_TRACE 3

struct CWorldCollideContextData
{
	FourVectors m_TraceStartPnt[MAX_WORLD_PLANAR_CONSTRAINTS];
	FourVectors m_TraceEndPnt[MAX_WORLD_PLANAR_CONSTRAINTS];
	FourVectors m_PointOnPlane[MAX_WORLD_PLANAR_CONSTRAINTS];
	FourVectors m_PlaneNormal[MAX_WORLD_PLANAR_CONSTRAINTS];

	int m_nActivePlanes;
	int m_nNumFixedPlanes;
	float m_flLastUpdateTime;
	Vector m_vecLastUpdateOrigin;
	bool m_bPlaneActive[MAX_WORLD_PLANAR_CONSTRAINTS];

	void *operator new( size_t nSize );
	void *operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine );
	void operator delete(void *pData);
	void operator delete( void* p, int nBlockUse, const char *pFileName, int nLine );

	void SetBaseTrace(  int nIndex, Vector const &rayStart, Vector const &traceDir, int nCollisionGroup, bool bKeepMisses );

	void CalculatePlanes( CParticleCollection *pParticles, int nCollisionMode, int nCollisionGroupNumber,
						  Vector const *pCpOffset = NULL, float flMovementTolerance = 0.  );
};

#endif // PARTICLES_INTERNAL_H	


