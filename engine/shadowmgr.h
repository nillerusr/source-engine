//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADOWMGR_H
#define SHADOWMGR_H

#ifdef _WIN32
#pragma once
#endif

#include "engine/ishadowmgr.h"
#include "mathlib/vmatrix.h"
#include "surfacehandle.h"
#include "mathlib/ssemath.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDispInfo;


//-----------------------------------------------------------------------------
// Shadow decals are applied to a single surface
//-----------------------------------------------------------------------------
typedef unsigned short ShadowDecalHandle_t;

enum
{
	SHADOW_DECAL_HANDLE_INVALID = (ShadowDecalHandle_t)~0
};


//-----------------------------------------------------------------------------
// This structure contains the vertex information for shadows
//-----------------------------------------------------------------------------
struct ShadowVertex_t
{
	Vector	m_Position;
	Vector	m_ShadowSpaceTexCoord;
};


//-----------------------------------------------------------------------------
// This structure contains info to accelerate shadow darkness computation
//-----------------------------------------------------------------------------
struct ShadowDecalRenderInfo_t
{
	Vector2D m_vTexOrigin;
	Vector2D m_vTexSize;
	float m_flFalloffOffset;
	float m_flOOZFalloffDist;
	float m_flFalloffAmount;
	float m_flFalloffBias;
};


//-----------------------------------------------------------------------------
// Shadow-related functionality internal to the engine
//-----------------------------------------------------------------------------
abstract_class IShadowMgrInternal : public IShadowMgr
{
public:
	virtual void LevelInit( int nSurfCount ) = 0;
	virtual void LevelShutdown() = 0;

	// This surface has been rendered; and we'll want to render the shadows
	// on this surface
	virtual void AddShadowsOnSurfaceToRenderList( ShadowDecalHandle_t decalHandle ) = 0;

	// This will render all shadows that were previously added using
	// AddShadowsOnSurfaceToRenderList. If there's a model to world transform
	// for the shadow receiver, then send it in!
	// NOTE: This draws both shadows and projected textures.
	virtual void RenderProjectedTextures( VMatrix const* pModelToWorld = 0 ) = 0;

	// NOTE: This draws shadows
	virtual void RenderShadows( VMatrix const* pModelToWorld = 0 ) = 0;

	// NOTE: This draws flashlights
	virtual void RenderFlashlights( bool bDoMasking, VMatrix const* pModelToWorld = 0 ) = 0;

	// Clears the list of shadows to render 
	virtual void ClearShadowRenderList() = 0;

	// Projects + clips shadows
	// count + ppPosition describe an array of pointers to vertex positions
	// of the unclipped shadow
	// ppOutVertex is pointed to the head of an array of pointers to
	// clipped vertices the function returns the number of clipped vertices
	virtual int ProjectAndClipVertices( ShadowHandle_t handle, int count, 
		Vector** ppPosition, ShadowVertex_t*** ppOutVertex ) = 0;

	// Computes information for rendering
	virtual void ComputeRenderInfo( ShadowDecalRenderInfo_t* pInfo, ShadowHandle_t handle ) const = 0;

	// Shadow state...
	virtual unsigned short InvalidShadowIndex( ) = 0;
	virtual void SetModelShadowState( ModelInstanceHandle_t instance ) = 0;

	virtual void SetNumWorldMaterialBuckets( int numMaterialSortBins ) = 0;

	virtual void DrawFlashlightDecals( int sortGroup, bool bDoMasking ) = 0;

	virtual void DrawFlashlightDecalsOnSingleSurface( SurfaceHandle_t surfID, bool bDoMasking ) = 0;

	virtual void DrawFlashlightOverlays( int nSortGroup, bool bDoMasking ) = 0;

	virtual void DrawFlashlightDepthTexture( ) = 0;

	virtual void DrawFlashlightDecalsOnDisplacements( int sortGroup, CDispInfo **visibleDisps, int nVisibleDisps, bool bDoMasking ) = 0;

	virtual void SetFlashlightStencilMasks( bool bDoMasking ) = 0;
	virtual bool ModelHasShadows( ModelInstanceHandle_t instance ) = 0;

	// Converts z value to darkness
	inline unsigned char ComputeDarkness( float z, const ShadowDecalRenderInfo_t& info ) const;
};


//-----------------------------------------------------------------------------
// inline methods
//-----------------------------------------------------------------------------
inline unsigned char IShadowMgrInternal::ComputeDarkness( float z, const ShadowDecalRenderInfo_t& info ) const
{
	// NOTE: 0 means black, non-zero adds towards white...
	z = ( z - info.m_flFalloffOffset ) * info.m_flOOZFalloffDist;
	z = fsel( z, z, 0.0f );
	z = info.m_flFalloffBias + z * info.m_flFalloffAmount;
	z = fsel( z - 255.0f, 255.0f, z );
	return z;
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern IShadowMgrInternal* g_pShadowMgr;

#endif
