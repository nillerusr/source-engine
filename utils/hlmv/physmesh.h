//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSMESH_H
#define PHYSMESH_H

#ifdef _WIN32
#pragma once
#endif

#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vcollide_parse.h"


struct studiohdr_t;

struct hlmvsolid_t : public solid_t
{
	float	massBias;
	int		surfacePropIndex;
};

struct merge_t
{
	int parent;
	int child;
};

struct editparams_t
{
	float	totalMass;
	char	rootName[128];
	int		concave;
	int		mergeCount;
	merge_t	mergeList[32];	// there are never very many of these, so don't bother doing anything dynamic
};

class CPhysmesh
{
public:
	void	Clear( void );

	char	m_boneName[64];
	int		m_vertCount;
	Vector	*m_pVerts;
	matrix3x4_t	m_matrix;

	hlmvsolid_t		m_solid;
	constraint_ragdollparams_t	m_constraint;
	ICollisionQuery *m_pCollisionModel;
};

class IStudioPhysics
{
public:
	virtual ~IStudioPhysics( void ) {}

	virtual char		*DumpQC( void ) = 0;
	virtual int			Count( void ) = 0;
	virtual CPhysmesh	*GetMesh( int index ) = 0;
	virtual float		GetMass( void ) = 0;
};

IStudioPhysics *LoadPhysics( MDLHandle_t mdlHandle );
void DestroyPhysics( IStudioPhysics *pStudioPhysics );

#endif // PHYSMESH_H
