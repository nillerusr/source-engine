//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DECAL_CLIP_H
#define DECAL_CLIP_H
#ifdef _WIN32
#pragma once
#endif


#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "mathlib/compressed_vector.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "filesystem.h"
#include "decal_private.h"
#include "gl_model_private.h"


#define MAX_DECALCLIPVERT		48


class CDecalVert
{
public:
	Vector		m_vPos;
	int			m_decalIndex;		// used by the cache code

	// These are the texcoords for the decal itself
	Vector2D	m_ctCoords;
	// Lightmap texcoords for the decal.
	Vector2D	m_cLMCoords;
};


// Clip pOutVerts/nStartVerts into the decal's texture space.
CDecalVert* R_DoDecalSHClip( 
	CDecalVert *pInVerts,
	CDecalVert *pOutVerts, 
	decal_t *pDecal, 
	int nStartVerts,
	const Vector &vecNormal );

// Generate clipped vertex list for decal pdecal projected onto polygon psurf
CDecalVert* R_DecalVertsClip( 
	CDecalVert *pOutVerts, 
	decal_t *pDecal, 
	SurfaceHandle_t surfID, 
	IMaterial *pMaterial );

// Compute the unscaled basis for the decal.
void R_DecalComputeBasis( 
	Vector const& surfaceNormal, 
	Vector const* pSAxis, 
	Vector* textureSpaceBasis );

// Compute the basis for the decal and scale the axes so the whole decal fits
// into the (0,0) - (1,1) range.
void R_SetupDecalTextureSpaceBasis(
	decal_t *pDecal,
	Vector &vSurfNormal,
	IMaterial *pMaterial,
	Vector textureSpaceBasis[3],
	float decalWorldScale[2] );

extern ALIGN16 CDecalVert g_DecalClipVerts[MAX_DECALCLIPVERT] ALIGN16_POST;

#endif // DECAL_CLIP_H
