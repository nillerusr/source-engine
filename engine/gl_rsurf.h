//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef GL_RSURF_H
#define GL_RSURF_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "bsptreedata.h"
#include "materialsystem/imesh.h"

class Vector;
struct WorldListInfo_t;
class IMaterial;
class IClientRenderable;
class IBrushRenderer;
class IClientEntity;
struct model_t;
struct cplane_t;
struct VisibleFogVolumeInfo_t;

struct LightmapUpdateInfo_t
{
	SurfaceHandle_t m_SurfHandle;
	int				transformIndex;
};

struct LightmapTransformInfo_t
{
	model_t *pModel;
	matrix3x4_t xform;
};
extern CUtlVector<LightmapUpdateInfo_t> g_LightmapUpdateList;
extern CUtlVector<LightmapTransformInfo_t> g_LightmapTransformList;

//-----------------------------------------------------------------------------
// Helper class to iterate over leaves
//-----------------------------------------------------------------------------
class IEngineSpatialQuery : public ISpatialQuery
{
public:
};

extern IEngineSpatialQuery* g_pToolBSPTree;

class IWorldRenderList;
IWorldRenderList *AllocWorldRenderList();

void R_Surface_LevelInit();
void R_Surface_LevelShutdown();
void R_SceneBegin( void );
void R_SceneEnd( void );
void R_BuildWorldLists( IWorldRenderList *pRenderList, WorldListInfo_t* pInfo, int iForceViewLeaf, const struct VisOverrideData_t* pVisData, bool bShadowDepth = false, float *pWaterReflectionHeight = NULL );
void R_DrawWorldLists( IWorldRenderList *pRenderList, unsigned long flags, float waterZAdjust );

void R_GetVisibleFogVolume( const Vector& vEyePoint, VisibleFogVolumeInfo_t *pInfo );
void R_SetFogVolumeState( int fogVolume, bool useHeightFog );
IMaterial *R_GetFogVolumeMaterial( int fogVolume, bool bEyeInFogVolume );
void R_SetupSkyTexture( model_t *pWorld );

void Shader_DrawLightmapPageChains( IWorldRenderList *pRenderList, int pageId );
void Shader_DrawLightmapPageSurface( SurfaceHandle_t surfID, float red, float green, float blue );
void Shader_DrawTranslucentSurfaces( IWorldRenderList *pRenderList, int sortIndex, unsigned long flags, bool bShadowDepth );
bool Shader_LeafContainsTranslucentSurfaces( IWorldRenderList *pRenderList, int sortIndex, unsigned long flags );
void R_DrawTopView( bool enable );
void R_TopViewBounds( const Vector2D & mins, const Vector2D & maxs );

// Resets a world render list
void ResetWorldRenderList( IWorldRenderList *pRenderList );

// Computes the centroid of a surface
void Surf_ComputeCentroid( SurfaceHandle_t surfID, Vector *pVecCentroid );

// Installs a client-side renderer for brush models
void R_InstallBrushRenderOverride( IBrushRenderer* pBrushRenderer );

// update dlight status on a brush model
extern int R_MarkDlightsOnBrushModel( model_t *model, IClientRenderable *pRenderable );

void R_DrawBrushModel( 
	IClientEntity *baseentity, 
	model_t *model, 
	const Vector& origin, 
	const QAngle& angles, 
	ERenderDepthMode DepthMode, bool bDrawOpaque, bool bDrawTranslucent );

void R_DrawBrushModelShadow( IClientRenderable* pRender );
void R_BrushBatchInit( void );

int R_GetBrushModelPlaneCount( const model_t *model );
const cplane_t &R_GetBrushModelPlane( const model_t *model, int nIndex, Vector *pOrigin );

bool TangentSpaceSurfaceSetup( SurfaceHandle_t surfID, Vector &tVect );
void TangentSpaceComputeBasis( Vector& tangentS, Vector& tangentT, const Vector& normal, const Vector& tVect, bool negateTangent );

#ifndef NEWMESH
inline void BuildIndicesForSurface( CMeshBuilder &meshBuilder, SurfaceHandle_t surfID )
{
	int nSurfTriangleCount = MSurf_VertCount( surfID ) - 2;
	unsigned short startVert = MSurf_VertBufferIndex( surfID );
	Assert(startVert!=0xFFFF);

	// NOTE: This switch appears to help performance
	// add surface to this batch
	switch (nSurfTriangleCount)
	{
	case 1:
		meshBuilder.FastIndex( startVert );
		meshBuilder.FastIndex( startVert + 1 );
		meshBuilder.FastIndex( startVert + 2 );
		break;

	case 2:
		meshBuilder.FastIndex( startVert );
		meshBuilder.FastIndex( startVert + 1 );
		meshBuilder.FastIndex( startVert + 2 );
		meshBuilder.FastIndex( startVert );
		meshBuilder.FastIndex( startVert + 2 );
		meshBuilder.FastIndex( startVert + 3 );
		break;

	default:
		{
			for ( unsigned short v = 0; v < nSurfTriangleCount; ++v )
			{
				meshBuilder.FastIndex( startVert );
				meshBuilder.FastIndex( startVert + v + 1 );
				meshBuilder.FastIndex( startVert + v + 2 );
			}
		}
		break;
	}
}

inline void BuildIndicesForWorldSurface( CMeshBuilder &meshBuilder, SurfaceHandle_t surfID, worldbrushdata_t *pData )
{
	if ( SurfaceHasPrims(surfID) )
	{
		mprimitive_t *pPrim = &pData->primitives[MSurf_FirstPrimID( surfID, pData )];
		Assert(pPrim->vertCount==0);
		unsigned short startVert = MSurf_VertBufferIndex( surfID );
		Assert( pPrim->indexCount == ((MSurf_VertCount( surfID ) - 2)*3));

		for ( int primIndex = 0; primIndex < pPrim->indexCount; primIndex++ )
		{
			meshBuilder.FastIndex( pData->primindices[pPrim->firstIndex + primIndex] + startVert );
		}
	}
	else
	{
		BuildIndicesForSurface( meshBuilder, surfID );
	}
}

#else

inline void BuildIndicesForSurface( CIndexBufferBuilder &indexBufferBuilder, SurfaceHandle_t surfID )
{
	int nSurfTriangleCount = MSurf_VertCount( surfID ) - 2;
	unsigned short startVert = MSurf_VertBufferIndex( surfID );
	Assert(startVert!=0xFFFF);

	// NOTE: This switch appears to help performance
	// add surface to this batch
	switch (nSurfTriangleCount)
	{
	case 1:
		indexBufferBuilder.FastIndex( startVert );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert ) );
		indexBufferBuilder.FastIndex( startVert + 1 );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + 1 ) );
		indexBufferBuilder.FastIndex( startVert + 2 );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + 2 ) );
		break;

	case 2:
		indexBufferBuilder.FastIndex( startVert );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert ) );
		indexBufferBuilder.FastIndex( startVert + 1 );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + 1 ) );
		indexBufferBuilder.FastIndex( startVert + 2 );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + 2 ) );
		indexBufferBuilder.FastIndex( startVert );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert ) );
		indexBufferBuilder.FastIndex( startVert + 2 );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + 2 ) );
		indexBufferBuilder.FastIndex( startVert + 3 );
		Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + 3 ) );
		break;

	default:
		{
			for ( unsigned short v = 0; v < nSurfTriangleCount; ++v )
			{
				indexBufferBuilder.FastIndex( startVert );
				Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert ) );
				indexBufferBuilder.FastIndex( startVert + v + 1 );
				Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + v + 1 ) );
				indexBufferBuilder.FastIndex( startVert + v + 2 );
				Warning( "BuildIndicesForSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( startVert + v + 2 ) );
			}
		}
		break;
	}
}

inline void BuildIndicesForWorldSurface( CIndexBufferBuilder &indexBufferBuilder, SurfaceHandle_t surfID, worldbrushdata_t *pData )
{
	if ( SurfaceHasPrims(surfID) )
	{
		mprimitive_t *pPrim = &pData->primitives[MSurf_FirstPrimID( surfID, pData )];
		Assert(pPrim->vertCount==0);
		unsigned short startVert = MSurf_VertBufferIndex( surfID );
		Assert( pPrim->indexCount == ((MSurf_VertCount( surfID ) - 2)*3));

		for ( int primIndex = 0; primIndex < pPrim->indexCount; primIndex++ )
		{
			indexBufferBuilder.FastIndex( pData->primindices[pPrim->firstIndex + primIndex] + startVert );
			Warning( "BuildIndicesForWorldSurface: indexBufferBuilder.FastIndex( %d )\n", ( int )( pData->primindices[pPrim->firstIndex + primIndex] + startVert ) );
		}
	}
	else
	{
		BuildIndicesForSurface( indexBufferBuilder, surfID );
	}
}
#endif
#endif // GL_RSURF_H
