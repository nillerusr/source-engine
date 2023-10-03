//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Blob.
//
//=============================================================================//

#ifndef BLOB_NETWORKBYPASS_H
#define BLOB_NETWORKBYPASS_H
#ifdef _WIN32
#pragma once
#endif

#include "bitvec.h"

#define BLOB_MAX_LEVEL_PARTICLES 4000 // maximum number of blob particles in a given level at any one time
#define BLOB_MAX_LEVEL_PARTICLES_BITS 12 // the number of bits needed to represent the number of particles above (should be ceil(lg(BLOB_MAX_LEVEL_PARTICLES)))
#define PARTICLEUSAGENUMINTS ((BLOB_MAX_LEVEL_PARTICLES + (BITS_PER_INT-1)) / BITS_PER_INT)
#define BLOBPARTICLEPOSITION(x) (g_pBlobNetworkBypass->vParticlePositions[x])
#define BLOBPARTICLERADIUS(x) (g_pBlobNetworkBypass->vParticleRadii[x])
#define BLOBPARTICLECLOSESTSURFDIR(x) (g_pBlobNetworkBypass->vParticleClosestSurfDir[x])

#ifdef CLIENT_DLL
#define BLOBPARTICLEPOS_INTERP(x) (g_BlobParticleInterpolation.vInterpolatedPositions[x])
#define BLOBPARTICLERADIUS_INTERP(x) (g_BlobParticleInterpolation.vInterpolatedRadii[x])
#define BLOBPARTICLECLOSESTSURFDIR_INTERP(x) (g_BlobParticleInterpolation.vInterpolatedClosestSurfDir[x])
#endif

struct BlobNetworkBypass_t
{
	//these 2 ints and bitvec help us communicate which particles contain valid data
	uint32 			iNumParticlesAllocated;
	uint32			iHighestIndexUsed;
	CBitVec<BLOB_MAX_LEVEL_PARTICLES> bCurrentlyInUse;

	//actual data we want to communicate using the bypass
	Vector			vParticlePositions[BLOB_MAX_LEVEL_PARTICLES];
	float			vParticleRadii[BLOB_MAX_LEVEL_PARTICLES];
	Vector			vParticleClosestSurfDir[BLOB_MAX_LEVEL_PARTICLES];
	float			fTimeDataUpdated;
	bool			bDataUpdated;
};

#ifdef CLIENT_DLL
struct BlobParticleInterpolation_t
{
	Vector			vInterpolatedPositions[BLOB_MAX_LEVEL_PARTICLES];
	float			vInterpolatedRadii[BLOB_MAX_LEVEL_PARTICLES];
	Vector			vInterpolatedClosestSurfDir[BLOB_MAX_LEVEL_PARTICLES];
};
extern BlobParticleInterpolation_t g_BlobParticleInterpolation;
#endif

extern BlobNetworkBypass_t *g_pBlobNetworkBypass;


#ifndef CLIENT_DLL
int AllocateBlobNetworkBypassIndex( void );
void ReleaseBlobNetworkBypassIndex( int iIndex );
#endif

#endif // BLOB_NETWORKBYPASS_H
