//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef IDISPINFO_H
#define IDISPINFO_H

#ifdef _WIN32
#pragma once
#endif


//=============================================================================

#include <assert.h>
#include "bspfile.h"
#include "mathlib/vmatrix.h"
#include "dispnode.h"
#include "builddisp.h"
#include "utlvector.h"
#include "engine/ishadowmgr.h"
#include "getintersectingsurfaces_struct.h"
#include "surfacehandle.h"
#include "ivrenderview.h"

struct model_t;
struct Ray_t;
struct RayDispOutput_t;
struct decal_t;
class CMeshBuilder;


//-----------------------------------------------------------------------------
// Handle to decals + shadows on displacements
//-----------------------------------------------------------------------------
typedef unsigned short DispDecalHandle_t;

enum
{
	DISP_DECAL_HANDLE_INVALID = (DispDecalHandle_t)~0
};

typedef unsigned short DispShadowHandle_t;

enum
{
	DISP_SHADOW_HANDLE_INVALID = (DispShadowHandle_t)~0
};


//-----------------------------------------------------------------------------
// Displacement interface to the engine (and WorldCraft?)
//-----------------------------------------------------------------------------
abstract_class IDispInfo
{
public:

	virtual				~IDispInfo() {}

	// Builds a list of displacement triangles intersecting the sphere.
	virtual void		GetIntersectingSurfaces( GetIntersectingSurfaces_Struct *pStruct ) = 0;

	virtual void		RenderWireframeInLightmapPage( int pageId ) = 0;
	
	virtual void		GetBoundingBox( Vector &bbMin, Vector &bbMax ) = 0;

	// Get and set the parent surfaces.
	virtual void		SetParent( SurfaceHandle_t surfID ) = 0;
	virtual SurfaceHandle_t	GetParent() = 0;

	// Add dynamic lights to the lightmap for this surface.
	virtual void AddDynamicLights( struct dlight_t *pLights, unsigned int lightMask ) = 0;
	// Compute the mask for the lights hitting this surface.
	virtual unsigned int ComputeDynamicLightMask( struct dlight_t *pLights ) = 0;

	// Add and remove decals.
	// flSize is like the radius of the decal so the decal isn't put on any disp faces it's too far away from.
	virtual DispDecalHandle_t	NotifyAddDecal( decal_t *pDecal, float flSize ) = 0;
	virtual void				NotifyRemoveDecal( DispDecalHandle_t h ) = 0;
	
	virtual DispShadowHandle_t	AddShadowDecal( ShadowHandle_t shadowHandle ) = 0;
	virtual void				RemoveShadowDecal( DispShadowHandle_t handle ) = 0;

	// Compute shadow fragments for a particular shadow, return the vertex + index count of all fragments
	virtual bool		ComputeShadowFragments( DispShadowHandle_t h, int& vertexCount, int& indexCount ) = 0;

	// Tag the surface and check if it's tagged. You can untag all the surfaces
	// with DispInfo_ClearAllTags. Note: it just uses a frame counter to track the
	// tag state so it's really really fast to call ClearAllTags (just increments
	// a variable 99.999% of the time).
	virtual bool		GetTag() = 0;
	virtual void		SetTag() = 0;

	// Cast a ray against this surface
	virtual	bool	TestRay( Ray_t const& ray, float start, float end, float& dist, Vector2D* lightmapUV, Vector2D* textureUV ) = 0;

	// Computes the texture + lightmap coordinate given a displacement uv
	virtual void	ComputeLightmapAndTextureCoordinate( RayDispOutput_t const& uv, Vector2D* luv, Vector2D* tuv ) = 0;
};


// ----------------------------------------------------------------------------- //
// Adds shadow rendering data to a particular mesh builder
// The function will return the new base index
// ----------------------------------------------------------------------------- //
int				DispInfo_AddShadowsToMeshBuilder( CMeshBuilder& meshBuilder, 
										DispShadowHandle_t h, int baseIndex );


typedef void* HDISPINFOARRAY;


// Init and shutdown for the material system (references global, materials).
void			DispInfo_InitMaterialSystem();
void			DispInfo_ShutdownMaterialSystem();


// Use these to manage a list of IDispInfos.
HDISPINFOARRAY	DispInfo_CreateArray( int nElements );
void			DispInfo_DeleteArray( HDISPINFOARRAY hArray );
IDispInfo*		DispInfo_IndexArray( HDISPINFOARRAY hArray, int iElement );
int				DispInfo_ComputeIndex( HDISPINFOARRAY hArray, IDispInfo* pInfo );

// Clear the tags for all displacements in the array.
void			DispInfo_ClearAllTags( HDISPINFOARRAY hArray );

// Call this to render a list of displacements.
// If bOrtho is true, then no backface removal is done on dispinfos.
void			DispInfo_RenderList( int nSortGroup, SurfaceHandle_t *pList, int listCount, bool bOrtho, unsigned long flags, ERenderDepthMode DepthMode );


// This should be called from Map_LoadDisplacements (while the map file is open).
// It loads the displacement data from the file and prepares the displacements for rendering.
//
// bRestoring is set to true when just restoring the data from the mapfile
// (ie: displacements already are initialized but need new static buffers).
bool			DispInfo_LoadDisplacements( model_t *pWorld, bool bRestoring );

// Deletes all the static vertex buffers.
void			DispInfo_ReleaseMaterialSystemObjects( model_t *pWorld );


#endif // IDISPINFO_H
