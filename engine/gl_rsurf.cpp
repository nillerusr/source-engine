//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


#include "render_pch.h"
#include "client.h"
#include "gl_model_private.h"
#include "gl_water.h"
#include "gl_cvars.h"
#include "zone.h"
#include "decal.h"
#include "decal_private.h"
#include "gl_lightmap.h"
#include "r_local.h"
#include "gl_matsysiface.h"
#include "gl_rsurf.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ivballoctracker.h"
#include "tier2/tier2.h"
#include "collisionutils.h"
#include "cdll_int.h"
#include "utllinkedlist.h"
#include "r_areaportal.h"
#include "bsptreedata.h"
#include "cmodel_private.h"
#include "tier0/dbg.h"
#include "crtmemdebug.h"
#include "iclientrenderable.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "gl_rmain.h"
#include "tier0/vprof.h"
#include "bitvec.h"
#include "debugoverlay.h"
#include "host.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "cl_main.h"
#include "cmodel_engine.h"
#include "r_decal.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "coordsize.h"
#include "mempool.h"
#ifndef SWDS
#include "Overlay.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BACKFACE_EPSILON	-0.01f

#define BRUSHMODEL_DECAL_SORT_GROUP		MAX_MAT_SORT_GROUPS
const int MAX_VERTEX_FORMAT_CHANGES = 128;
int g_MaxLeavesVisible = 512;

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class IClientEntity;

// interface to shader drawing
void Shader_BrushBegin( model_t *model, IClientEntity *baseentity = NULL );
void Shader_BrushSurface( SurfaceHandle_t surfID, model_t *model, IClientEntity *baseentity = NULL );
void Shader_BrushEnd( IMatRenderContext *pRenderContext, VMatrix const* brushToWorld, model_t *model, bool bShadowDepth, IClientEntity *baseentity = NULL );
#ifdef NEWMESH
void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, float overbright, CVertexBufferBuilder &builder );
#else
void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, float overbright, CMeshBuilder &builder );
#endif

//-----------------------------------------------------------------------------
// Information about the fog volumes for this pass of rendering
//-----------------------------------------------------------------------------

struct FogState_t
{
	MaterialFogMode_t m_FogMode;
	float m_FogStart;
	float m_FogEnd;
	float m_FogColor[3];
	bool m_FogEnabled;
};

struct FogVolumeInfo_t : public FogState_t
{
	bool	m_InFogVolume;
	float	m_FogSurfaceZ;
	float	m_FogMinZ;
	int		m_FogVolumeID;
};

//-----------------------------------------------------------------------------
// Cached convars...
//-----------------------------------------------------------------------------
struct CachedConvars_t
{
	bool	m_bDrawWorld;
	int		m_nDrawLeaf;
	bool	m_bDrawFuncDetail;
};


static CachedConvars_t s_ShaderConvars;

// AR - moved so SWDS can access these vars
Frustum_t g_Frustum;


//-----------------------------------------------------------------------------
// Convars
//-----------------------------------------------------------------------------
static ConVar r_drawtranslucentworld( "r_drawtranslucentworld", "1", FCVAR_CHEAT );
static ConVar mat_forcedynamic( "mat_forcedynamic", "0", FCVAR_CHEAT );
static ConVar r_drawleaf( "r_drawleaf", "-1", FCVAR_CHEAT, "Draw the specified leaf." );
static ConVar r_drawworld( "r_drawworld", "1", FCVAR_CHEAT, "Render the world." );
static ConVar r_drawfuncdetail( "r_drawfuncdetail", "1", FCVAR_CHEAT, "Render func_detail" );
static ConVar fog_enable_water_fog( "fog_enable_water_fog", "1", FCVAR_CHEAT );
static ConVar r_fastzreject( "r_fastzreject", "0", FCVAR_ALLOWED_IN_COMPETITIVE, "Activate/deactivates a fast z-setting algorithm to take advantage of hardware with fast z reject. Use -1 to default to hardware settings" );
static ConVar r_fastzrejectdisp( "r_fastzrejectdisp", "0", 0, "Activates/deactivates fast z rejection on displacements (360 only). Only active when r_fastzreject is on." );


//-----------------------------------------------------------------------------
// Installs a client-side renderer for brush models
//-----------------------------------------------------------------------------
static IBrushRenderer* s_pBrushRenderOverride = 0;

//-----------------------------------------------------------------------------
// Make sure we don't render the same surfaces twice
//-----------------------------------------------------------------------------
int	r_surfacevisframe = 0;
#define r_surfacevisframe dont_use_r_surfacevisframe_here


//-----------------------------------------------------------------------------
// Fast z reject displacements?
//-----------------------------------------------------------------------------
static bool s_bFastZRejectDisplacements = false;

//-----------------------------------------------------------------------------
// Top view bounds
//-----------------------------------------------------------------------------
static bool r_drawtopview = false;
static Vector2D s_OrthographicCenter;
static Vector2D s_OrthographicHalfDiagonal;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
typedef CVarBitVec CVisitedSurfs;


//-----------------------------------------------------------------------------
// Returns planes in brush models
//-----------------------------------------------------------------------------
int R_GetBrushModelPlaneCount( const model_t *model )
{
	return model->brush.nummodelsurfaces;
}

const cplane_t &R_GetBrushModelPlane( const model_t *model, int nIndex, Vector *pOrigin )
{
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	surfID += nIndex;
	Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

	if ( pOrigin )
	{
		int vertCount = MSurf_VertCount( surfID );
		if ( vertCount > 0 )
		{
			int nFirstVertex = model->brush.pShared->vertindices[MSurf_FirstVertIndex( surfID )];
			*pOrigin = model->brush.pShared->vertexes[nFirstVertex].position;
		}
		else
		{
			const cplane_t &plane = MSurf_Plane( surfID );
			VectorMultiply( plane.normal, plane.dist, *pOrigin );
		}
	}

	return MSurf_Plane( surfID );
}

//-----------------------------------------------------------------------------
// Computes the centroid of a surface
//-----------------------------------------------------------------------------
void Surf_ComputeCentroid( SurfaceHandle_t surfID, Vector *pVecCentroid )
{
	int nCount = MSurf_VertCount( surfID );
	int nFirstVertIndex = MSurf_FirstVertIndex( surfID );

	float flTotalArea = 0.0f;
	Vector vecNormal;
	pVecCentroid->Init(0,0,0);
	int vertIndex = host_state.worldbrush->vertindices[nFirstVertIndex];
	Vector vecApex = host_state.worldbrush->vertexes[vertIndex].position;
	for (int v = 1; v < nCount - 1; ++v )
	{
		vertIndex = host_state.worldbrush->vertindices[nFirstVertIndex+v];
		Vector v1 = host_state.worldbrush->vertexes[vertIndex].position;
		vertIndex = host_state.worldbrush->vertindices[nFirstVertIndex+v+1];
		Vector v2 = host_state.worldbrush->vertexes[vertIndex].position;
		CrossProduct( v2 - v1, v1 - vecApex, vecNormal );
		float flArea = vecNormal.Length();
		flTotalArea += flArea;
		*pVecCentroid += (vecApex + v1 + v2) * flArea / 3.0f;
	}

	if (flTotalArea)
	{
		*pVecCentroid /= flTotalArea;
	}
}

//-----------------------------------------------------------------------------
// Converts sort infos to lightmap pages
//-----------------------------------------------------------------------------
int SortInfoToLightmapPage( int sortID )
{
        return materialSortInfoArray[sortID].lightmapPageID;
}



#ifndef SWDS

class CWorldRenderList : public CRefCounted1<IWorldRenderList>
{
public:
	CWorldRenderList()
	{
	}

	~CWorldRenderList()
	{
		Purge();
	}

	static CWorldRenderList *FindOrCreateList( int nSurfaces )
	{
		CWorldRenderList *p = g_Pool.GetObject();
		if ( p->m_VisitedSurfs.GetNumBits() == 0 )
		{
			p->Init( nSurfaces );
		}
		else
		{
			p->AddRef();
		}

		AssertMsg( p->m_VisitedSurfs.GetNumBits() == nSurfaces, "World render list pool not cleared between maps" );

		return p;
	}

	static void PurgeAll()
	{
		CWorldRenderList *p;
		while ( ( p = g_Pool.GetObject( false ) ) != NULL )
		{
			p->Purge();
			delete p;
		}
	}

	virtual bool OnFinalRelease()
	{
		Reset();
		g_Pool.PutObject( this );
		return false;
	}

	void Init( int nSurfaces )
	{
		m_SortList.Init(materials->GetNumSortIDs(), 512);
		m_AlphaSortList.Init( g_MaxLeavesVisible, 64 );
		m_DispSortList.Init(materials->GetNumSortIDs(), 32);
		m_DispAlphaSortList.Init( g_MaxLeavesVisible, 32 );
		m_VisitedSurfs.Resize( nSurfaces );
		m_bSkyVisible = false;
	}

	void Purge()
	{
		g_MaxLeavesVisible = max(g_MaxLeavesVisible,m_VisibleLeaves.Count());

		m_VisibleLeaves.Purge();
		m_VisibleLeafFogVolumes.Purge();
		for ( int i = 0; i < MAX_MAT_SORT_GROUPS; i++ )
		{
			m_ShadowHandles[i].Purge();
			m_DlightSurfaces[i].Purge();
		}
		m_SortList.Shutdown();
		m_AlphaSortList.Shutdown();
		m_DispSortList.Shutdown();
		m_DispAlphaSortList.Shutdown();
	}

	void Reset()
	{
		g_MaxLeavesVisible = max(g_MaxLeavesVisible,m_VisibleLeaves.Count());
		m_SortList.Reset();
		m_AlphaSortList.Reset();
		m_DispSortList.Reset();
		m_DispAlphaSortList.Reset();

		m_bSkyVisible = false;
		for (int j = 0; j < MAX_MAT_SORT_GROUPS; ++j)
		{
			//Assert(pRenderList->m_ShadowHandles[j].Count() == 0 );
			m_ShadowHandles[j].RemoveAll();
			m_DlightSurfaces[j].RemoveAll();
		}

		// We haven't found any visible leafs this frame
		m_VisibleLeaves.RemoveAll();
		m_VisibleLeafFogVolumes.RemoveAll();

		m_VisitedSurfs.ClearAll();
	}

	CMSurfaceSortList m_SortList;
	CMSurfaceSortList m_DispSortList;
	CMSurfaceSortList m_AlphaSortList;
	CMSurfaceSortList m_DispAlphaSortList;

	//-------------------------------------------------------------------------
	// List of decals to render this frame (need an extra one for brush models)
	//-------------------------------------------------------------------------
	CUtlVector<ShadowDecalHandle_t> m_ShadowHandles[MAX_MAT_SORT_GROUPS];
	
	// list of surfaces with dynamic lightmaps
	CUtlVector<SurfaceHandle_t>	m_DlightSurfaces[MAX_MAT_SORT_GROUPS];

	//-------------------------------------------------------------------------
	// Used to generate a list of the leaves visited, and in back-to-front order
	// for this frame of rendering
	//-------------------------------------------------------------------------
	CUtlVector<LeafIndex_t>		m_VisibleLeaves;
	CUtlVector<LeafFogVolume_t>	m_VisibleLeafFogVolumes;

	CVisitedSurfs m_VisitedSurfs;
	bool						m_bSkyVisible;

	static CObjectPool<CWorldRenderList> g_Pool;
};

CObjectPool<CWorldRenderList> CWorldRenderList::g_Pool;

IWorldRenderList *AllocWorldRenderList()
{
	return CWorldRenderList::FindOrCreateList( host_state.worldbrush->numsurfaces );
}


FORCEINLINE bool VisitSurface( CVisitedSurfs &visitedSurfs, SurfaceHandle_t surfID )
{
	return !visitedSurfs.TestAndSet( MSurf_Index( surfID ) );
}

FORCEINLINE void MarkSurfaceVisited( CVisitedSurfs &visitedSurfs, SurfaceHandle_t surfID )
{
	visitedSurfs.Set( MSurf_Index( surfID ) );
}

FORCEINLINE bool VisitedSurface( CVisitedSurfs &visitedSurfs, SurfaceHandle_t surfID )
{
	return visitedSurfs.IsBitSet( MSurf_Index( surfID ) );
}

FORCEINLINE bool VisitedSurface( CVisitedSurfs &visitedSurfs, int index )
{
	return visitedSurfs.IsBitSet( index );
}

//-----------------------------------------------------------------------------
// Activates top view
//-----------------------------------------------------------------------------

void R_DrawTopView( bool enable )
{
	r_drawtopview = enable;
}

void R_TopViewBounds( Vector2D const& mins, Vector2D const& maxs )
{
	Vector2DAdd( maxs, mins, s_OrthographicCenter );
	s_OrthographicCenter *= 0.5f;
	Vector2DSubtract( maxs, s_OrthographicCenter, s_OrthographicHalfDiagonal );
}

#define MOVE_DLIGHTS_TO_NEW_TEXTURE 0

#if MOVE_DLIGHTS_TO_NEW_TEXTURE
bool DlightSurfaceSetQueuingFlag(SurfaceHandle_t surfID)
{
	if ( MSurf_Flags( surfID ) & SURFDRAW_HASLIGHTSYTLES )
	{
		msurfacelighting_t *pLighting = SurfaceLighting(surfID);
		for( int maps = 1; maps < MAXLIGHTMAPS && pLighting->m_nStyles[maps] != 255; maps++ )
		{
			if( d_lightstylenumframes[pLighting->m_nStyles[maps]] != 1 )
			{
				MSurf_Flags( surfID ) |= SURFDRAW_DLIGHTPASS;
				return true;
			}
		}

		return false;
	}

	MSurf_Flags( surfID ) |= SURFDRAW_DLIGHTPASS;
	return true;
}
#else
bool DlightSurfaceSetQueuingFlag(SurfaceHandle_t surfID) { return false; }
#endif




//-----------------------------------------------------------------------------
// Adds surfaces to list of things to render
//-----------------------------------------------------------------------------
void Shader_TranslucentWorldSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	Assert( !SurfaceHasDispInfo( surfID ) && (pRenderList->m_VisibleLeaves.Count() > 0) );

	// Hook into the chain of translucent objects for this leaf
	int sortGroup = MSurf_SortGroup( surfID );
	pRenderList->m_AlphaSortList.AddSurfaceToTail( surfID, sortGroup, pRenderList->m_VisibleLeaves.Count()-1 );
	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[sortGroup].AddToTail( surfID );
		
		DlightSurfaceSetQueuingFlag(surfID);
	}
}

inline void Shader_WorldSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	// Hook it into the list of surfaces to render with this material
	// Do it in a way that generates a front-to-back ordering for fast z reject
	Assert( !SurfaceHasDispInfo( surfID ) );

	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );

	// Add decals on non-displacement surfaces
	if( SurfaceHasDecals( surfID ) )
	{
		DecalSurfaceAdd( surfID, nSortGroup );
	}

	int nMaterialSortID = MSurf_MaterialSortID( surfID );

	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[nSortGroup].AddToTail( surfID );
		if ( !DlightSurfaceSetQueuingFlag(surfID) )
		{
			pRenderList->m_SortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
		}
	}
	else
	{
		pRenderList->m_SortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
	}
}

// The NoCull flavor of this function optimizes for shadow depth map rendering
// No decal work, dlights or material sorting, for example
inline void Shader_WorldSurfaceNoCull( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	// Hook it into the list of surfaces to render with this material
	// Do it in a way that generates a front-to-back ordering for fast z reject
	Assert( !SurfaceHasDispInfo( surfID ) );

	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );

	int nMaterialSortID = MSurf_MaterialSortID( surfID );
	pRenderList->m_SortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
}


//-----------------------------------------------------------------------------
// Adds displacement surfaces to list of things to render
//-----------------------------------------------------------------------------
void Shader_TranslucentDisplacementSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	Assert( SurfaceHasDispInfo( surfID ) && (pRenderList->m_VisibleLeaves.Count() > 0));

	// For translucent displacement surfaces, they can exist in many
	// leaves. We want to choose the leaf that's closest to the camera
	// to render it in. Thankfully, we're iterating the tree in front-to-back
	// order, so this is very simple.

	// NOTE: You might expect some problems here when displacements cross fog volume 
	// planes. However, these problems go away (I hope!) because the first planes
	// that split a scene are the fog volume planes. That means that if we're
	// in a fog volume, the closest leaf that the displacement will be in will
	// also be in the fog volume. If we're not in a fog volume, the closest
	// leaf that the displacement will be in will not be a fog volume. That should
	// hopefully hide any discontinuities between fog state that occur when 
	// rendering displacements that straddle fog volume boundaries.

	// Each surface is in exactly one group
	int sortGroup = MSurf_SortGroup( surfID );
	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[sortGroup].AddToTail( surfID );
		if ( !DlightSurfaceSetQueuingFlag(surfID) )
		{
			pRenderList->m_DispAlphaSortList.AddSurfaceToTail(surfID, sortGroup, pRenderList->m_VisibleLeaves.Count()-1);
		}
	}
	else
	{
		pRenderList->m_DispAlphaSortList.AddSurfaceToTail(surfID, sortGroup, pRenderList->m_VisibleLeaves.Count()-1);
	}
}

void Shader_DisplacementSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	Assert( SurfaceHasDispInfo( surfID ) );

	// For opaque displacement surfaces, we're going to build a temporary list of 
	// displacement surfaces in each material bucket, and then add those to
	// the actual displacement lists in a separate pass.
	// We do this to sort the displacement surfaces by material

	// Each surface is in exactly one group
	int nSortGroup = MSurf_SortGroup( surfID );
	int nMaterialSortID = MSurf_MaterialSortID( surfID );
	if ( MSurf_Flags( surfID ) & (SURFDRAW_HASLIGHTSYTLES|SURFDRAW_HASDLIGHT) )
	{
		pRenderList->m_DlightSurfaces[nSortGroup].AddToTail( surfID );
		if ( !DlightSurfaceSetQueuingFlag(surfID) )
		{
			pRenderList->m_DispSortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
		}
	}
	else
	{
		pRenderList->m_DispSortList.AddSurfaceToTail( surfID, nSortGroup, nMaterialSortID );
	}
}
 
//-----------------------------------------------------------------------------
// Purpose: This draws a single surface using the dynamic mesh
//-----------------------------------------------------------------------------
void Shader_DrawSurfaceDynamic( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, bool bShadowDepth )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s %d", __FUNCTION__, surfID );

	if( !SurfaceHasPrims( surfID ) )
	{
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_POLYGON, MSurf_VertCount( surfID ) );
		BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, OVERBRIGHT, meshBuilder );
		meshBuilder.End();
		pMesh->Draw();
		return;
	}

	mprimitive_t *pPrim = &host_state.worldbrush->primitives[MSurf_FirstPrimID( surfID )];

	if ( pPrim->vertCount )
	{
#ifdef DBGFLAG_ASSERT
		int primType = pPrim->type;
#endif
		IMesh *pMesh = pRenderContext->GetDynamicMesh( false );
		CMeshBuilder meshBuilder;
		for( int i = 0; i < MSurf_NumPrims( surfID ); i++, pPrim++ )
		{
			// Can't have heterogeneous primitive lists
			Assert( primType == pPrim->type );
			switch( pPrim->type )
			{
			case PRIM_TRILIST:
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, pPrim->vertCount, pPrim->indexCount );
				break;
			case PRIM_TRISTRIP:
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, pPrim->vertCount, pPrim->indexCount );
				break;
			default:
				Assert( 0 );
				return;
			}
			Assert( pPrim->indexCount );
			BuildMSurfacePrimVerts( host_state.worldbrush, pPrim, meshBuilder, surfID );
			BuildMSurfacePrimIndices( host_state.worldbrush, pPrim, meshBuilder );
			meshBuilder.End();
			pMesh->Draw();
		}
	}
	else
	{
		// prims are just a tessellation
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, MSurf_VertCount( surfID ), pPrim->indexCount );
		BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, OVERBRIGHT, meshBuilder );
		for ( int primIndex = 0; primIndex < pPrim->indexCount; primIndex++ )
		{
			meshBuilder.FastIndex( host_state.worldbrush->primindices[pPrim->firstIndex + primIndex] );
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Purpose: This draws a single surface using its static mesh
//-----------------------------------------------------------------------------

/*
// NOTE: Since a static vb/dynamic ib IMesh doesn't buffer, we shouldn't use this
// since it causes a lock and drawindexedprimitive per surface! (gary)
void Shader_DrawSurfaceStatic( SurfaceHandle_t surfID )
{
	VPROF( "Shader_DrawSurfaceStatic" );
	if ( 
#ifdef USE_CONVARS
		mat_forcedynamic.GetInt() || 
#endif
		(MSurf_Flags( surfID ) & SURFDRAW_WATERSURFACE) )
	{
		Shader_DrawSurfaceDynamic( pRenderContext, surfID );
		return;
	}

	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, 
		g_pWorldStatic[MSurf_MaterialSortID( surfID )].m_pMesh );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 0, (MSurf_VertCount( surfID )-2)*3 );

	unsigned short startVert = MSurf_VertBufferIndex( surfID );
	Assert(startVert!=0xFFFF);
	for ( int v = 0; v < MSurf_VertCount( surfID )-2; v++ )
	{
		meshBuilder.Index( startVert );
		meshBuilder.AdvanceIndex();
		meshBuilder.Index( startVert + v + 1 );
		meshBuilder.AdvanceIndex();
		meshBuilder.Index( startVert + v + 2 );
		meshBuilder.AdvanceIndex();
	}
	meshBuilder.End();
	pMesh->Draw();
}
*/

//-----------------------------------------------------------------------------
// Sets the lightmapping state
//-----------------------------------------------------------------------------
static inline void Shader_SetChainLightmapState( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID )
{
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		if( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		}
		else
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE );
		}
	}
	else
	{
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
		pRenderContext->BindLightmapPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );
	}
}


//-----------------------------------------------------------------------------
// Sets the lightmap + texture to render with
//-----------------------------------------------------------------------------
void Shader_SetChainTextureState( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, IClientEntity* pBaseEntity, bool bShadowDepth )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	if ( bShadowDepth )
	{
		IMaterial *pDrawMaterial = MSurf_TexInfo( surfID )->material;
		// Select proper override material
		int nAlphaTest = (int) pDrawMaterial->IsAlphaTested();
		int nNoCull = (int) pDrawMaterial->IsTwoSided();
		IMaterial *pDepthWriteMaterial = g_pMaterialDepthWrite[nAlphaTest][nNoCull];

		if ( nAlphaTest == 1 )
		{
			static unsigned int originalTextureVarCache = 0;
			IMaterialVar *pOriginalTextureVar = pDrawMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
			static unsigned int originalTextureFrameVarCache = 0;
			IMaterialVar *pOriginalTextureFrameVar = pDrawMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
			static unsigned int originalAlphaRefCache = 0;
			IMaterialVar *pOriginalAlphaRefVar = pDrawMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

			static unsigned int textureVarCache = 0;
			IMaterialVar *pTextureVar = pDepthWriteMaterial->FindVarFast( "$basetexture", &textureVarCache );
			static unsigned int textureFrameVarCache = 0;
			IMaterialVar *pTextureFrameVar = pDepthWriteMaterial->FindVarFast( "$frame", &textureFrameVarCache );
			static unsigned int alphaRefCache = 0;
			IMaterialVar *pAlphaRefVar = pDepthWriteMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

			if( pTextureVar && pOriginalTextureVar )
			{
				pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
			}

			if( pTextureFrameVar && pOriginalTextureFrameVar )
			{
				pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
			}

			if( pAlphaRefVar && pOriginalAlphaRefVar )
			{
				pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
			}
		}

		pRenderContext->Bind( pDepthWriteMaterial );
	}
	else
	{
		pRenderContext->Bind( MSurf_TexInfo( surfID )->material, pBaseEntity ? pBaseEntity->GetClientRenderable() : NULL );
		Shader_SetChainLightmapState( pRenderContext, surfID );
	}
}

void Shader_DrawDynamicChain( const CMSurfaceSortList &sortList, const surfacesortgroup_t &group, bool bShadowDepth )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	CMatRenderContextPtr pRenderContext( materials );

	SurfaceHandle_t hSurfID = sortList.GetSurfaceAtHead(group);
	if ( !IS_SURF_VALID( hSurfID ))
		return;
	Shader_SetChainTextureState( pRenderContext, hSurfID, 0, bShadowDepth );

	MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
	{
		Shader_DrawSurfaceDynamic( pRenderContext, surfID, bShadowDepth );
	}
	MSL_FOREACH_SURFACE_IN_GROUP_END()
}

void Shader_DrawChainsDynamic( const CMSurfaceSortList &sortList, int nSortGroup, bool bShadowDepth )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
	{
		Shader_DrawDynamicChain( sortList, group, bShadowDepth );
	}
	MSL_FOREACH_GROUP_END()
}

struct vertexformatlist_t
{
	unsigned short numbatches;
	unsigned short firstbatch;
#ifdef NEWMESH
	IVertexBuffer *pVertexBuffer;
#else
	IMesh	*pMesh;
#endif
};

struct batchlist_t
{
	SurfaceHandle_t	surfID;		// material and lightmap info
	unsigned short firstIndex;
	unsigned short numIndex;
};

void Shader_DrawChainsStatic( const CMSurfaceSortList &sortList, int nSortGroup, bool bShadowDepth )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	//VPROF("DrawChainsStatic");
	CUtlVectorFixed<vertexformatlist_t, MAX_VERTEX_FORMAT_CHANGES> meshList;
	int meshMap[MAX_VERTEX_FORMAT_CHANGES];
	CUtlVectorFixedGrowable<batchlist_t, 512> batchList;
	CUtlVectorFixedGrowable<const surfacesortgroup_t *, 8> dynamicGroups;
	bool bWarn = true;
#ifdef NEWMESH
	CIndexBufferBuilder indexBufferBuilder;
#else
	CMeshBuilder meshBuilder;
#endif

	bool skipBind = false;
	if ( g_pMaterialSystemConfig->nFullbright == 1  )
	{
		skipBind = true;
	}

	const CUtlVector<surfacesortgroup_t *> &groupList = sortList.GetSortList(nSortGroup);
	int count = groupList.Count();
	
	int i, listIndex = 0;

	CMatRenderContextPtr pRenderContext( materials );

	//PIXEVENT( pRenderContext, "Shader_DrawChainsStatic" );

	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	while ( listIndex < count )
	{
		const surfacesortgroup_t &groupBase = *groupList[listIndex];
		SurfaceHandle_t surfIDBase = sortList.GetSurfaceAtHead( groupBase );
		int sortIDBase = MSurf_MaterialSortID( surfIDBase );
#ifdef NEWMESH
		IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer( MATERIAL_INDEX_FORMAT_16BIT, false );
		indexBufferBuilder.Begin( pBuildIndexBuffer, nMaxIndices );
		IVertexBuffer *pLastVertexBuffer = NULL;
#else
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[sortIDBase] );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, nMaxIndices );
		IMesh *pLastMesh = NULL;
#endif
		int indexCount = 0;
		int meshIndex = -1;

		for ( ; listIndex < count; listIndex++ )
		{
			const surfacesortgroup_t &group = *groupList[listIndex];
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			Assert( IS_SURF_VALID( surfID ) );
			if ( MSurf_Flags(surfID) & SURFDRAW_DYNAMIC )
			{
				dynamicGroups.AddToTail( &group );
				continue;
			}

			Assert( group.triangleCount > 0 );
			int numIndex = group.triangleCount * 3;
			if ( indexCount + numIndex > nMaxIndices )
			{
				if ( numIndex > nMaxIndices )
				{
					DevMsg("Too many faces with the same material in scene!\n");
					break;
				}

#ifdef NEWMESH
				pLastVertexBuffer = NULL;
#else
				pLastMesh = NULL;
#endif
				break;
			}
			
			int sortID = MSurf_MaterialSortID( surfID );

#ifdef NEWMESH
			if ( g_WorldStaticMeshes[sortID] != pLastVertexBuffer )
#else
			if ( g_WorldStaticMeshes[sortID] != pLastMesh )
#endif
			{
				if( meshList.Count() < MAX_VERTEX_FORMAT_CHANGES - 1 )
				{
					meshIndex = meshList.AddToTail();
					meshList[meshIndex].numbatches = 0;
					meshList[meshIndex].firstbatch = batchList.Count();
#ifdef NEWMESH
					pLastVertexBuffer = g_WorldStaticMeshes[sortID];
					Assert( pLastVertexBuffer );
					meshList[meshIndex].pVertexBuffer = pLastVertexBuffer;
#else
					pLastMesh = g_WorldStaticMeshes[sortID];
					Assert( pLastMesh );
					meshList[meshIndex].pMesh = pLastMesh;
#endif
				}
				else
				{
					if ( bWarn )
					{
						Warning( "Too many vertex format changes in frame, whole world not rendered\n" );
						bWarn = false;
					}
					continue;
				}
			}

			int batchIndex = batchList.AddToTail();
			batchlist_t &batch = batchList[batchIndex];
			batch.firstIndex = indexCount;
			batch.surfID = surfID;
			batch.numIndex = numIndex;
			Assert( indexCount + batch.numIndex < nMaxIndices );
			indexCount += batch.numIndex;

			meshList[meshIndex].numbatches++;

			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfIDList)
			{
				tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "BuildIndicesForWorldSurface" );
#ifdef NEWMESH
				BuildIndicesForWorldSurface( indexBufferBuilder, surfIDList, host_state.worldbrush );
#else
				Assert( meshBuilder.m_nFirstVertex == 0 );
				BuildIndicesForWorldSurface( meshBuilder, surfIDList, host_state.worldbrush );
#endif
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}

		// close out the index buffer
#ifdef NEWMESH
		indexBufferBuilder.End( false ); // this one matches (world rendering)
#else
		meshBuilder.End( false, false );
#endif

		int meshTotal = meshList.Count();
		VPROF_INCREMENT_COUNTER( "vertex format changes", meshTotal );

		// HACKHACK: Crappy little bubble sort
		// UNDONE: Make the traversal happen so that they are already sorted when you get here.
		// NOTE: Profiled in a fairly complex map.  This is not even costing 0.01ms / frame!
		for ( i = 0; i < meshTotal; i++ )
		{
			meshMap[i] = i;
		}

		bool swapped = true;
		while ( swapped )
		{
			swapped = false;
			for ( i = 1; i < meshTotal; i++ )
			{
#ifdef NEWMESH
				if ( meshList[meshMap[i]].pVertexBuffer < meshList[meshMap[i-1]].pVertexBuffer )
#else
				if ( meshList[meshMap[i]].pMesh < meshList[meshMap[i-1]].pMesh )
#endif
				{
					int tmp = meshMap[i-1];
					meshMap[i-1] = meshMap[i];
					meshMap[i] = tmp;
					swapped = true;
				}
			}
		}

#ifndef NEWMESH
		pRenderContext->BeginBatch( pBuildMesh );
#endif
		for ( int m = 0; m < meshTotal; m++ )
		{
			vertexformatlist_t &mesh = meshList[meshMap[m]];
			IMaterial *pBindMaterial = materialSortInfoArray[MSurf_MaterialSortID( batchList[mesh.firstbatch].surfID )].material;
#ifdef NEWMESH
			Assert( mesh.pVertexBuffer && pBuildIndexBuffer );
#else
			Assert( mesh.pMesh && pBuildMesh );
#endif
#ifdef NEWMESH
			IIndexBuffer *pIndexBuffer = pRenderContext->GetDynamicIndexBuffer( MATERIAL_INDEX_FORMAT_16BIT, false );
#else
//			IMesh *pMesh = pRenderContext->GetDynamicMesh( false, mesh.pMesh, pBuildMesh, pBindMaterial );
			pRenderContext->BindBatch( mesh.pMesh, pBindMaterial );
#endif

			for ( int b = 0; b < mesh.numbatches; b++ )
			{
				batchlist_t &batch = batchList[b+mesh.firstbatch];
				IMaterial *pDrawMaterial = materialSortInfoArray[MSurf_MaterialSortID( batch.surfID )].material;

				if ( bShadowDepth )
				{
					// Select proper override material
					int nAlphaTest = (int) pDrawMaterial->IsAlphaTested();
					int nNoCull = (int) pDrawMaterial->IsTwoSided();
					IMaterial *pDepthWriteMaterial = g_pMaterialDepthWrite[nAlphaTest][nNoCull];

					if ( nAlphaTest == 1 )
					{
						static unsigned int originalTextureVarCache = 0;
						IMaterialVar *pOriginalTextureVar = pDrawMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
						static unsigned int originalTextureFrameVarCache = 0;
						IMaterialVar *pOriginalTextureFrameVar = pDrawMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
						static unsigned int originalAlphaRefCache = 0;
						IMaterialVar *pOriginalAlphaRefVar = pDrawMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

						static unsigned int textureVarCache = 0;
						IMaterialVar *pTextureVar = pDepthWriteMaterial->FindVarFast( "$basetexture", &textureVarCache );
						static unsigned int textureFrameVarCache = 0;
						IMaterialVar *pTextureFrameVar = pDepthWriteMaterial->FindVarFast( "$frame", &textureFrameVarCache );
						static unsigned int alphaRefCache = 0;
						IMaterialVar *pAlphaRefVar = pDepthWriteMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

						if( pTextureVar && pOriginalTextureVar )
						{
							pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
						}

						if( pTextureFrameVar && pOriginalTextureFrameVar )
						{
							pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
						}

						if( pAlphaRefVar && pOriginalAlphaRefVar )
						{
							pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
						}
					}

					pRenderContext->Bind( pDepthWriteMaterial );
				}
				else
				{
					pRenderContext->Bind( pDrawMaterial, NULL );
						
					if ( skipBind )
					{
						if( MSurf_Flags( batch.surfID ) & SURFDRAW_BUMPLIGHT )
						{
							pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
						}
						else
						{
							pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE );
						}
					}
					else
					{
						pRenderContext->BindLightmapPage( materialSortInfoArray[MSurf_MaterialSortID( batch.surfID )].lightmapPageID );
					}
				}
#ifdef NEWMESH
				// FIXME: IMaterial::GetVertexFormat() should do this stripping (add a separate 'SupportsCompression' accessor)
				VertexFormat_t vertexFormat = pBindMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
				pRenderContext->BindVertexBuffer( 0, mesh.pVertexBuffer, 0, vertexFormat );
				pRenderContext->BindIndexBuffer( pIndexBuffer, 0 );
				Warning( "pRenderContext->Draw( MATERIAL_TRIANGLES, batch.firstIndex = %d, batch.numIndex = %d )\n", 
					( int )batch.firstIndex, ( int )batch.numIndex );
				pRenderContext->Draw( MATERIAL_TRIANGLES, batch.firstIndex, batch.numIndex );
#else
//				pMesh->Draw( batch.firstIndex, batch.numIndex );
				pRenderContext->DrawBatch( batch.firstIndex, batch.numIndex );
#endif
			}
		}
#ifndef NEWMESH
		pRenderContext->EndBatch();
#endif


		// if we get here and pLast mesh is NULL and we rendered somthing, we need to loop
#ifdef NEWMESH
		if ( pLastVertexBuffer || !meshTotal )
#else
		if ( pLastMesh || !meshTotal )
#endif
			break;

		meshList.RemoveAll();
		batchList.RemoveAll();
	}
	for ( i = 0; i < dynamicGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( sortList, *dynamicGroups[i], bShadowDepth );
	}
}

//-----------------------------------------------------------------------------
// The following methods will display debugging info in the middle of each surface
//-----------------------------------------------------------------------------
typedef void (*SurfaceDebugFunc_t)( SurfaceHandle_t surfID, const Vector &vecCentroid );

void DrawSurfaceID( SurfaceHandle_t surfID, const Vector &vecCentroid )
{
	char buf[32];
	Q_snprintf(buf, sizeof( buf ), "0x%p", surfID );
	CDebugOverlay::AddTextOverlay( vecCentroid, 0, buf );
}

void DrawSurfaceIDAsInt( SurfaceHandle_t surfID, const Vector &vecCentroid )
{
	int nInt = (msurface2_t*)surfID - host_state.worldbrush->surfaces2;
	char buf[32];
	Q_snprintf( buf, sizeof( buf ), "%d", nInt );
	CDebugOverlay::AddTextOverlay( vecCentroid, 0, buf );
}

void DrawSurfaceMaterial( SurfaceHandle_t surfID, const Vector &vecCentroid )
{
	mtexinfo_t * pTexInfo = MSurf_TexInfo(surfID);

	const char *pFullMaterialName = pTexInfo->material ? pTexInfo->material->GetName() : "no material";
	const char *pSlash = strrchr( pFullMaterialName, '/' );
	const char *pMaterialName = strrchr( pFullMaterialName, '\\' );
	if (pSlash > pMaterialName)
		pMaterialName = pSlash;
	if (pMaterialName)
		++pMaterialName;
	else
		pMaterialName = pFullMaterialName;

	CDebugOverlay::AddTextOverlay( vecCentroid, 0, pMaterialName );
}


//-----------------------------------------------------------------------------
// Displays the surface id # in the center of the surface.
//-----------------------------------------------------------------------------
void Shader_DrawSurfaceDebuggingInfo( const CUtlVector<msurface2_t *> &surfaceList, SurfaceDebugFunc_t func )
{
	for ( int i = 0; i < surfaceList.Count(); i++ )
	{
		SurfaceHandle_t surfID = surfaceList[i];
		Assert( !SurfaceHasDispInfo( surfID ) );

		// Compute the centroid of the surface
		int nCount = MSurf_VertCount( surfID );
		if (nCount >= 3)
		{
			Vector vecCentroid;
			Surf_ComputeCentroid( surfID, &vecCentroid );
			func( surfID, vecCentroid );
		}
	}
}



//-----------------------------------------------------------------------------
// Doesn't draw internal triangles
//-----------------------------------------------------------------------------
void Shader_DrawWireframePolygons( const CUtlVector<msurface2_t *> &surfaceList )
{
	int nLineCount = 0;
	for ( int i = 0; i < surfaceList.Count(); i++ )
	{
		int nCount = MSurf_VertCount( surfaceList[i] );
		if (nCount >= 3)
		{
			nLineCount += nCount;
		}
	}

	if (nLineCount == 0)
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_materialWorldWireframe );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nLineCount );

	for ( int i = 0; i < surfaceList.Count(); i++ )
	{
		SurfaceHandle_t surfID = surfaceList[i];
		Assert( !SurfaceHasDispInfo( surfID ) );

		// Compute the centroid of the surface
		int nCount = MSurf_VertCount( surfID );
		if (nCount >= 3)
		{
			int nFirstVertIndex = MSurf_FirstVertIndex( surfID );
			int nVertIndex = host_state.worldbrush->vertindices[nFirstVertIndex + nCount - 1];
			Vector vecPrevPos = host_state.worldbrush->vertexes[nVertIndex].position;
			for (int v = 0; v < nCount; ++v )
			{
				// world-space vertex
				nVertIndex = host_state.worldbrush->vertindices[nFirstVertIndex + v];
				Vector& vec = host_state.worldbrush->vertexes[nVertIndex].position;

				// output to mesh
				meshBuilder.Position3fv( vecPrevPos.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( vec.Base() );
				meshBuilder.AdvanceVertex();

				vecPrevPos = vec;
			}
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Debugging mode, renders the wireframe.
//-----------------------------------------------------------------------------
static void Shader_DrawChainsWireframe(	const CUtlVector<msurface2_t *> &surfaceList )
{
	int nWireFrameMode = WireFrameMode();

	switch( nWireFrameMode )
	{
	case 3:
		// Doesn't draw internal triangles
		Shader_DrawWireframePolygons(surfaceList);
		break;

	default:
		{
			CMatRenderContextPtr pRenderContext( materials );
			if( nWireFrameMode == 2 )
			{
				pRenderContext->Bind( g_materialWorldWireframeZBuffer );
			}
			else
			{
				pRenderContext->Bind( g_materialWorldWireframe );
			}
			for ( int i = 0; i < surfaceList.Count(); i++ )
			{
				SurfaceHandle_t surfID = surfaceList[i];
				Assert( !SurfaceHasDispInfo( surfID ) );
				Shader_DrawSurfaceDynamic( pRenderContext, surfID, false );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Debugging mode, renders the normals
//-----------------------------------------------------------------------------
static void Shader_DrawChainNormals( const CUtlVector<msurface2_t *> &surfaceList )
{
	Vector p, tVect, tangentS, tangentT;

	CMatRenderContextPtr pRenderContext( materials );

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	pRenderContext->Bind( g_pMaterialWireframeVertexColor );

	for ( int i = 0; i < surfaceList.Count(); i++ )
	{
		SurfaceHandle_t surfID = surfaceList[i];
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, MSurf_VertCount( surfID ) * 3 );
	
		bool negate = TangentSpaceSurfaceSetup( surfID, tVect );

		int vertID;
		for( vertID = 0; vertID < MSurf_VertCount( surfID ); ++vertID )
		{
			int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+vertID];
			Vector& pos = pBrushData->vertexes[vertIndex].position;
			Vector& norm = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID )+vertID] ];

			TangentSpaceComputeBasis( tangentS, tangentT, norm, tVect, negate );

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, norm, p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, tangentT, p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, tangentS, p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();
		}
		
		meshBuilder.End();
		pMesh->Draw();
	}
}

static void Shader_DrawChainBumpBasis( const CUtlVector<msurface2_t *> &surfaceList )
{
	Vector p, tVect, tangentS, tangentT;

	CMatRenderContextPtr pRenderContext( materials );

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	pRenderContext->Bind( g_pMaterialWireframeVertexColor );

	for ( int i = 0; i < surfaceList.Count(); i++ )
	{
		SurfaceHandle_t surfID = surfaceList[i];
		IMesh *pMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, MSurf_VertCount( surfID ) * 3 );
	
		bool negate = TangentSpaceSurfaceSetup( surfID, tVect );

		int vertID;
		for( vertID = 0; vertID < MSurf_VertCount( surfID ); ++vertID )
		{
			int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+vertID];
			Vector& pos = pBrushData->vertexes[vertIndex].position;
			Vector& norm = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID )+vertID] ];

			TangentSpaceComputeBasis( tangentS, tangentT, norm, tVect, negate );

			Vector worldSpaceBumpBasis[3];

			for( int j = 0; j < 3; j++ )
			{
				worldSpaceBumpBasis[j][0] = 
					g_localBumpBasis[j][0] * tangentS[0] +
					g_localBumpBasis[j][1] * tangentS[1] + 
					g_localBumpBasis[j][2] * tangentS[2];
				worldSpaceBumpBasis[j][1] = 
					g_localBumpBasis[j][0] * tangentT[0] +
					g_localBumpBasis[j][1] * tangentT[1] + 
					g_localBumpBasis[j][2] * tangentT[2];
				worldSpaceBumpBasis[j][2] = 
					g_localBumpBasis[j][0] * norm[0] +
					g_localBumpBasis[j][1] * norm[1] + 
					g_localBumpBasis[j][2] * norm[2];
			}

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, worldSpaceBumpBasis[0], p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 255, 0, 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, worldSpaceBumpBasis[1], p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 255, 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( pos, 5.0f, worldSpaceBumpBasis[2], p );
			meshBuilder.Position3fv( p.Base() );
			meshBuilder.Color3ub( 0, 0, 255 );
			meshBuilder.AdvanceVertex();
		}
		
		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Debugging mode, renders the luxel grid.
//-----------------------------------------------------------------------------
static void Shader_DrawLuxels( const CUtlVector<msurface2_t *> &surfaceList )
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_materialDebugLuxels );

	for ( int i = 0; i < surfaceList.Count(); i++ )
	{
		SurfaceHandle_t surfID = surfaceList[i];
		Assert( !SurfaceHasDispInfo( surfID ) );

		// Gotta bind the lightmap page so the rendering knows the lightmap scale
		pRenderContext->BindLightmapPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );
		Shader_DrawSurfaceDynamic( pRenderContext, surfID, false );
	}
}

static struct CShaderDebug
{
	bool		wireframe;
	bool		normals;
	bool		luxels;
	bool		bumpBasis;
	bool		surfacematerials;
	bool		anydebug;
	int			surfaceid;

	void TestAnyDebug()
	{
		anydebug = wireframe || normals || luxels || bumpBasis || ( surfaceid != 0 ) || surfacematerials;
	}

} g_ShaderDebug;


ConVar mat_surfaceid("mat_surfaceid", "0", FCVAR_CHEAT);
ConVar mat_surfacemat("mat_surfacemat", "0", FCVAR_CHEAT);


//-----------------------------------------------------------------------------
// Purpose: 
// Output : static void
//-----------------------------------------------------------------------------
static void ComputeDebugSettings( void )
{
	g_ShaderDebug.wireframe = ShouldDrawInWireFrameMode() || (r_drawworld.GetInt() == 2);
	g_ShaderDebug.normals = mat_normals.GetBool();
	g_ShaderDebug.luxels = mat_luxels.GetBool();
	g_ShaderDebug.bumpBasis = mat_bumpbasis.GetBool();
	g_ShaderDebug.surfaceid = mat_surfaceid.GetInt();
	g_ShaderDebug.surfacematerials = mat_surfacemat.GetBool();
	g_ShaderDebug.TestAnyDebug();
}

//-----------------------------------------------------------------------------
// Draw debugging information
//-----------------------------------------------------------------------------
static void DrawDebugInformation( const CUtlVector<msurface2_t *> &surfaceList )
{
	// Overlay with wireframe if we're in that mode
	if( g_ShaderDebug.wireframe )
	{
		Shader_DrawChainsWireframe(surfaceList);
	}

	// Overlay with normals if we're in that mode
	if( g_ShaderDebug.normals )
	{
		Shader_DrawChainNormals(surfaceList);
	}

	if( g_ShaderDebug.bumpBasis )
	{
		Shader_DrawChainBumpBasis(surfaceList);
	}
	
	// Overlay with luxel grid if we're in that mode
	if( g_ShaderDebug.luxels )
	{
		Shader_DrawLuxels(surfaceList);
	}

	if ( g_ShaderDebug.surfaceid )
	{
		// Draw the surface id in the middle of the surfaces
		Shader_DrawSurfaceDebuggingInfo( surfaceList, (g_ShaderDebug.surfaceid != 2 ) ? DrawSurfaceID : DrawSurfaceIDAsInt );
	}
	else if ( g_ShaderDebug.surfacematerials )
	{
		// Draw the material name in the middle of the surfaces
		Shader_DrawSurfaceDebuggingInfo( surfaceList, DrawSurfaceMaterial );
	}
}


void AddProjectedTextureDecalsToList( CWorldRenderList *pRenderList, int nSortGroup )
{
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	MSL_FOREACH_GROUP_BEGIN( sortList, nSortGroup, group )
	{
		MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, surfID)
		{
			Assert( !SurfaceHasDispInfo( surfID ) );
			if ( SHADOW_DECAL_HANDLE_INVALID != MSurf_ShadowDecals( surfID ) )
			{
				// No shadows on water surfaces
				if ((MSurf_Flags( surfID ) & SURFDRAW_NOSHADOWS) == 0)
				{
					MEM_ALLOC_CREDIT();
					pRenderList->m_ShadowHandles[nSortGroup].AddToTail( MSurf_ShadowDecals( surfID ) );
				}
			}
			// Add overlay fragments to list.
			if ( OVERLAY_FRAGMENT_INVALID != MSurf_OverlayFragmentList( surfID ) )
			{
				OverlayMgr()->AddFragmentListToRenderList( nSortGroup, MSurf_OverlayFragmentList( surfID ), false );
			}
		}
		MSL_FOREACH_SURFACE_IN_GROUP_END();
	}
	MSL_FOREACH_GROUP_END()
}

//-----------------------------------------------------------------------------
// Draws all of the opaque non-displacement surfaces queued up previously
//-----------------------------------------------------------------------------
void Shader_DrawChains( const CWorldRenderList *pRenderList, int nSortGroup, bool bShadowDepth )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	CMatRenderContextPtr pRenderContext(materials);
	Assert( !g_EngineRenderer->InLightmapUpdate() );
	VPROF("Shader_DrawChains");
	// Draw chains...
#ifdef USE_CONVARS
	if ( !mat_forcedynamic.GetInt() && !g_pMaterialSystemConfig->bDrawFlat )
#else
	if( 1 )
#endif
	{
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( "Shader_DrawChainsStatic" );
		Shader_DrawChainsStatic( pRenderList->m_SortList, nSortGroup, bShadowDepth );
	}
	else
	{
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( "Shader_DrawChainsDynamic" );
		Shader_DrawChainsDynamic( pRenderList->m_SortList, nSortGroup, bShadowDepth );
	}
	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( NULL );

#if MOVE_DLIGHTS_TO_NEW_TEXTURE
	for ( int i = 0; i < pRenderList->m_DlightSurfaces[nSortGroup].Count(); i++ )
	{
		SurfaceHandle_t surfID = pRenderList->m_DlightSurfaces[nSortGroup][i];
		if ( !SurfaceHasDispInfo( surfID ) && (MSurf_Flags(surfID) & SURFDRAW_DLIGHTPASS) )
		{
			pRenderContext->Bind( MSurf_TexInfo( surfID )->material, NULL );
			Shader_SetChainLightmapState( pRenderContext, surfID );
			Shader_DrawSurfaceDynamic( pRenderContext, surfID, bShadowDepth );
		}
	}
#endif

	if ( bShadowDepth )	// Skip debug stuff in shadow depth map
		return;

#ifdef USE_CONVARS
	if ( g_ShaderDebug.anydebug )
	{
		const CMSurfaceSortList &sortList = pRenderList->m_SortList;
		// Debugging information
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			CUtlVector<msurface2_t *> surfList;
			sortList.GetSurfaceListForGroup( surfList, group );
			DrawDebugInformation( surfList );
		}
		MSL_FOREACH_GROUP_END()
	}
#endif
}


//-----------------------------------------------------------------------------
// Draws all of the opaque displacement surfaces queued up previously
//-----------------------------------------------------------------------------
void Shader_DrawDispChain( int nSortGroup, const CMSurfaceSortList &list, unsigned long flags, ERenderDepthMode DepthMode )
{
	VPROF_BUDGET( "Shader_DrawDispChain", VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING );
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	int count = 0;
	msurface2_t **pList;
	MSL_FOREACH_GROUP_BEGIN( list, nSortGroup, group )
	{
		count += group.surfaceCount;
	}
	MSL_FOREACH_GROUP_END()

	if (count)
	{
		pList = (msurface2_t **)stackalloc( count * sizeof(msurface2_t *));
		int i = 0;
		MSL_FOREACH_GROUP_BEGIN( list, nSortGroup, group )
		{
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(list,group,surfID)
			{
				pList[i] = surfID;
				++i;
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		MSL_FOREACH_GROUP_END()
		Assert(i==count);

		// draw displacments, batch decals
		DispInfo_RenderList( nSortGroup, pList, count, g_EngineRenderer->ViewGetCurrent().m_bOrtho, flags, DepthMode );
		stackfree(pList);
	}
}

static void Shader_BuildDynamicLightmaps( CWorldRenderList *pRenderList )
{
	VPROF( "Shader_BuildDynamicLightmaps" );

	R_DLightStartView();

	// Build all lightmaps for opaque surfaces
	for ( int nSortGroup = 0; nSortGroup < MAX_MAT_SORT_GROUPS; ++nSortGroup)
	{
#if 0
		int updateStart = g_LightmapUpdateList.Count();
#endif
		for ( int i = pRenderList->m_DlightSurfaces[nSortGroup].Count()-1; i >= 0; --i )
		{
			LightmapUpdateInfo_t tmp;
			tmp.m_SurfHandle = pRenderList->m_DlightSurfaces[nSortGroup].Element(i);
			tmp.transformIndex = 0;
			g_LightmapUpdateList.AddToTail( tmp );
		}

		// UNDONE: Redo this list?  Make a new list with the texture coord info for the new lightmaps?
#if 0
		pRenderList->m_DlightSurfaces[nSortGroup].RemoveAll();
		for ( int i = updateStart; i < g_LightmapUpdateList.Count(); i++ )
		{
			if ( MSurf_Flags(g_LightmapUpdateList[i].m_SurfHandle) & SURFDRAW_DLIGHTPASS )
			{
				pRenderList->m_DlightSurfaces[nSortGroup].AddToTail(g_LightmapUpdateList[i].m_SurfHandle);
			}
		}
#endif
	}

	R_DLightEndView();
}


//-----------------------------------------------------------------------------
// Compute if we're in or out of a fog volume
//-----------------------------------------------------------------------------
static void ComputeFogVolumeInfo( FogVolumeInfo_t *pFogVolume )
{
	pFogVolume->m_InFogVolume = false;
	int leafID = CM_PointLeafnum( CurrentViewOrigin() );
	if( leafID < 0 || leafID >= host_state.worldbrush->numleafs )
		return;

	mleaf_t* pLeaf = &host_state.worldbrush->leafs[leafID];
	pFogVolume->m_FogVolumeID = pLeaf->leafWaterDataID;
	if( pFogVolume->m_FogVolumeID == -1 )
		return;

	pFogVolume->m_InFogVolume = true;

	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[pLeaf->leafWaterDataID];
	if( pLeafWaterData->surfaceTexInfoID == -1 )
	{
		// Should this ever happen?????
		pFogVolume->m_FogEnabled = false;
		return;
	}
	mtexinfo_t* pTexInfo = &host_state.worldbrush->texinfo[pLeafWaterData->surfaceTexInfoID];

	IMaterial* pMaterial = pTexInfo->material;
	if( pMaterial )
	{
		IMaterialVar* pFogColorVar	= pMaterial->FindVar( "$fogcolor", NULL );
		IMaterialVar* pFogEnableVar = pMaterial->FindVar( "$fogenable", NULL );
		IMaterialVar* pFogStartVar	= pMaterial->FindVar( "$fogstart", NULL );
		IMaterialVar* pFogEndVar	= pMaterial->FindVar( "$fogend", NULL );

		pFogVolume->m_FogEnabled = pFogEnableVar->GetIntValue() ? true : false;
		pFogColorVar->GetVecValue( pFogVolume->m_FogColor, 3 );
		pFogVolume->m_FogStart		= -pFogStartVar->GetFloatValue();
		pFogVolume->m_FogEnd		= -pFogEndVar->GetFloatValue();
		pFogVolume->m_FogSurfaceZ	= pLeafWaterData->surfaceZ;
		pFogVolume->m_FogMinZ		= pLeafWaterData->minZ;
		pFogVolume->m_FogMode		= MATERIAL_FOG_LINEAR;
	}
	else
	{
		static bool bComplained = false;
		if( !bComplained )
		{
			Warning( "***Water vmt missing . . check console for missing materials!***\n" );
			bComplained = true;
		}
		pFogVolume->m_FogEnabled = false;
	}
}


//-----------------------------------------------------------------------------
// Resets a world render list
//-----------------------------------------------------------------------------
void ResetWorldRenderList( CWorldRenderList *pRenderList )
{
	if ( pRenderList )
	{
		pRenderList->Reset();
	}
}


//-----------------------------------------------------------------------------
// Call this before rendering; it clears out the lists of stuff to render
//-----------------------------------------------------------------------------
void Shader_WorldBegin( CWorldRenderList *pRenderList )
{
	// Cache the convars so we don't keep accessing them...
	s_ShaderConvars.m_bDrawWorld = r_drawworld.GetBool();
	s_ShaderConvars.m_nDrawLeaf = r_drawleaf.GetInt();
	s_ShaderConvars.m_bDrawFuncDetail = r_drawfuncdetail.GetBool();

	ResetWorldRenderList( pRenderList );

	// Clear out the decal list
	DecalSurfacesInit( false );

	// Clear out the render lists of overlays
	OverlayMgr()->ClearRenderLists();

	// Clear out the render lists of shadows
	g_pShadowMgr->ClearShadowRenderList( );
}



//-----------------------------------------------------------------------------
// Performs the z-fill
//-----------------------------------------------------------------------------
static void Shader_WorldZFillSurfChain( const CMSurfaceSortList &sortList, const surfacesortgroup_t &group, CMeshBuilder &meshBuilder, int &nStartVertIn, unsigned int includeFlags )
{
	int nStartVert = nStartVertIn;
	mvertex_t *pWorldVerts = host_state.worldbrush->vertexes;

	MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(sortList, group, nSurfID)
	{
		if ( (MSurf_Flags( nSurfID ) & includeFlags) == 0 )
			continue;

		// Skip water surfaces since it may move up or down to fixup water transitions.
		if ( MSurf_Flags( nSurfID ) & SURFDRAW_WATERSURFACE )
			continue;

		int nSurfTriangleCount = MSurf_VertCount( nSurfID ) - 2;

		unsigned short *pVertIndex = &(host_state.worldbrush->vertindices[MSurf_FirstVertIndex( nSurfID )]);

		// add surface to this batch
		if ( SurfaceHasPrims(nSurfID) )
		{
			mprimitive_t *pPrim = &host_state.worldbrush->primitives[MSurf_FirstPrimID( nSurfID )];
			if ( pPrim->vertCount == 0 )
			{
				int firstVert = MSurf_FirstVertIndex( nSurfID );
				for ( int i = 0; i < MSurf_VertCount(nSurfID); i++ )
				{
					int vertIndex = host_state.worldbrush->vertindices[firstVert + i];
					meshBuilder.Position3fv( pWorldVerts[vertIndex].position.Base() );
					meshBuilder.AdvanceVertex();
				}
				for ( int primIndex = 0; primIndex < pPrim->indexCount; primIndex++ )
				{
					meshBuilder.FastIndex( host_state.worldbrush->primindices[pPrim->firstIndex + primIndex] + nStartVert );
				}
			}
		}
		else
		{
			switch (nSurfTriangleCount)
			{
			case 1:
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();

				meshBuilder.FastIndex( nStartVert );
				meshBuilder.FastIndex( nStartVert + 1 );
				meshBuilder.FastIndex( nStartVert + 2 );

				break;

			case 2:
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
				meshBuilder.AdvanceVertex();
				meshBuilder.FastIndex( nStartVert );
				meshBuilder.FastIndex( nStartVert + 1 );
				meshBuilder.FastIndex( nStartVert + 2 );
				meshBuilder.FastIndex( nStartVert );
				meshBuilder.FastIndex( nStartVert + 2 );
				meshBuilder.FastIndex( nStartVert + 3 );
				break;

			default:
				{
					for ( unsigned short v = 0; v < nSurfTriangleCount; ++v )
					{
						meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
						meshBuilder.AdvanceVertex();

						meshBuilder.FastIndex( nStartVert );
						meshBuilder.FastIndex( nStartVert + v + 1 );
						meshBuilder.FastIndex( nStartVert + v + 2 );
					}

					meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
					meshBuilder.AdvanceVertex();
					meshBuilder.Position3fv( pWorldVerts[*pVertIndex++].position.Base() );
					meshBuilder.AdvanceVertex();
				}
				break;
			}
		}
		nStartVert += nSurfTriangleCount + 2;
	}
	MSL_FOREACH_SURFACE_IN_GROUP_END()

	nStartVertIn = nStartVert;
}

static const int s_DrawWorldListsToSortGroup[MAX_MAT_SORT_GROUPS] = 
{
	MAT_SORT_GROUP_STRICTLY_ABOVEWATER,
	MAT_SORT_GROUP_STRICTLY_UNDERWATER,
	MAT_SORT_GROUP_INTERSECTS_WATER_SURFACE,
	MAT_SORT_GROUP_WATERSURFACE,
};

static ConVar r_flashlightrendermodels(  "r_flashlightrendermodels", "1" );

//-----------------------------------------------------------------------------
// Performs the shadow depth texture fill
//-----------------------------------------------------------------------------
static void Shader_WorldShadowDepthFill( CWorldRenderList *pRenderList, unsigned long flags )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	// First, count the number of vertices + indices
	int nVertexCount = 0;
	int nIndexCount = 0;
	ERenderDepthMode DepthMode = DEPTH_MODE_SHADOW;
	if ( flags & DRAWWORLDLISTS_DRAW_SSAO )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}

	int g;
	CUtlVector<const surfacesortgroup_t *> alphatestedGroups;

	const CMSurfaceSortList &sortList = pRenderList->m_SortList;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			if ( MSurf_Flags( surfID ) & SURFDRAW_WATERSURFACE )
				continue;
			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

			if( pMaterial->IsTranslucent() )
				continue;

			if ( pMaterial->IsAlphaTested() )
			{
				alphatestedGroups.AddToTail( &group );
				continue;
			}

			nVertexCount += group.vertexCount;
			nIndexCount += group.triangleCount*3;
		}
		MSL_FOREACH_GROUP_END()

		// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
		Shader_DrawDispChain( nSortGroup, pRenderList->m_DispSortList, flags, DepthMode );
	}
	if ( nVertexCount == 0 )
		return;
 
	CMatRenderContextPtr pRenderContext( materials );

	if ( DepthMode == DEPTH_MODE_SHADOW )
	{
		pRenderContext->Bind( g_pMaterialDepthWrite[0][1] );
	}
	else
	{
		pRenderContext->Bind( g_pMaterialSSAODepthWrite[0][1] );
	}

	IMesh *pMesh = pRenderContext->GetDynamicMesh( false );

	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( g_pMaterialDepthWrite[0][1] );	// opaque, nocull

	// nBatchIndexCount and nBatchVertexCount are the number of indices and vertices we can fit in this batch
	// Each batch must have fewer than nMaxIndices and nMaxVertices or the material system will fail
	int nBatchIndexCount  = min( nIndexCount,  nMaxIndices  );
	int nBatchVertexCount = min( nVertexCount, nMaxVertices );


	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );

	int nStartVert = 0;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			// Check to see if we can add this list to the current batch...
			int nCurrIndexCount = group.triangleCount*3;
			int nCurrVertexCount = group.vertexCount;
			if ( ( nCurrIndexCount == 0 ) || ( nCurrVertexCount == 0 ) )
				continue;

			// this group is too big to draw so push it into the alphatested groups
			// this will run much slower but at least it won't crash
			// alphatested groups will draw each surface one at a time.
			if ( nCurrIndexCount > nMaxIndices || nCurrVertexCount > nMaxVertices )
			{
				alphatestedGroups.AddToTail( &group );
				continue;
			}
			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

			// Opaque only on this loop
			if( pMaterial->IsTranslucent() ||  pMaterial->IsAlphaTested() )
				continue;

			Assert( nCurrIndexCount  <= nMaxIndices  );
			Assert( nCurrVertexCount <= nMaxVertices );

			if ( ( nBatchIndexCount < nCurrIndexCount ) || ( nBatchVertexCount < nCurrVertexCount ) )
			{
				// Nope, fire off the current batch...
				meshBuilder.End();
				pMesh->Draw();
				nBatchIndexCount  = min( nIndexCount,  nMaxIndices  );
				nBatchVertexCount = min( nVertexCount, nMaxVertices );
				pMesh = pRenderContext->GetDynamicMesh( false );
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );
				nStartVert = 0;
			}

			nBatchIndexCount  -= nCurrIndexCount;
			nIndexCount       -= nCurrIndexCount;
			nBatchVertexCount -= nCurrVertexCount;
			nVertexCount      -= nCurrVertexCount;
			// 0xFFFFFFFF means include all surfaces
			Shader_WorldZFillSurfChain( sortList, group, meshBuilder, nStartVert, 0xFFFFFFFF );
		}
		MSL_FOREACH_GROUP_END()
	}

	meshBuilder.End();
	pMesh->Draw();

	// Now draw the alpha-tested groups we stored away earlier
	for ( int i = 0; i < alphatestedGroups.Count(); i++ )
	{
		Shader_DrawDynamicChain( sortList, *alphatestedGroups[i], true );
	}
}



//-----------------------------------------------------------------------------
// Performs the z-fill
//-----------------------------------------------------------------------------
static void Shader_WorldZFill( CWorldRenderList *pRenderList, unsigned long flags )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	// First, count the number of vertices + indices
	int nVertexCount = 0;
	int nIndexCount = 0;

	int g;
	const CMSurfaceSortList &sortList = pRenderList->m_SortList;

#ifdef _X360
	bool bFastZRejectDisplacements = s_bFastZRejectDisplacements || ( r_fastzrejectdisp.GetInt() != 0 );
#endif

	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);
			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;
			if( pMaterial->IsAlphaTested() || pMaterial->IsTranslucent() )
			{
				continue;
			}
			nVertexCount += group.vertexCountNoDetail;
			nIndexCount += group.indexCountNoDetail;
		}
		MSL_FOREACH_GROUP_END()

#ifdef _X360
		// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
		// NOTE: This only makes sense on the 360, since the extra batches aren't
		// worth it on the PC (I think!)
		if ( bFastZRejectDisplacements )
		{
			Shader_DrawDispChain( nSortGroup, pRenderList->m_DispSortList, flags, true );
		}
#endif
	}

	if ( nVertexCount == 0 )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_pMaterialWriteZ );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false );

	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( g_pMaterialWriteZ );

	// nBatchIndexCount and nBatchVertexCount are the number of indices and vertices we can fit in this batch
	// Each batch must have fewe than nMaxIndices and nMaxVertices or the material system will fail
	int nBatchIndexCount  = min( nIndexCount,  nMaxIndices  );
	int nBatchVertexCount = min( nVertexCount, nMaxVertices );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );

	int nStartVert = 0;
	for ( g = 0; g < MAX_MAT_SORT_GROUPS; ++g )
	{
		if ( ( flags & ( 1 << g ) ) == 0 )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[g];
		MSL_FOREACH_GROUP_BEGIN(sortList, nSortGroup, group )
		{
			SurfaceHandle_t surfID = sortList.GetSurfaceAtHead(group);

			// Check to see if we can add this list to the current batch...
			int nCurrIndexCount = group.indexCountNoDetail;
			int nCurrVertexCount = group.vertexCountNoDetail;
			if ( ( nCurrIndexCount == 0 ) || ( nCurrVertexCount == 0 ) )
				continue;

			IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

			if( pMaterial->IsAlphaTested() || pMaterial->IsTranslucent() )
				continue;

			Assert( nCurrIndexCount  <= nMaxIndices  );
			Assert( nCurrVertexCount <= nMaxVertices );

			if ( ( nBatchIndexCount < nCurrIndexCount ) || ( nBatchVertexCount < nCurrVertexCount ) )
			{
				// Nope, fire off the current batch...
				meshBuilder.End();
				pMesh->Draw();
				nBatchIndexCount  = min( nIndexCount,  nMaxIndices  );
				nBatchVertexCount = min( nVertexCount, nMaxVertices );
				pMesh = pRenderContext->GetDynamicMesh( false );
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nBatchVertexCount, nBatchIndexCount );
				nStartVert = 0;
			}

			nBatchIndexCount  -= nCurrIndexCount;
			nIndexCount       -= nCurrIndexCount;
			nBatchVertexCount -= nCurrVertexCount;
			nVertexCount      -= nCurrVertexCount;

			// only draw surfaces on nodes (i.e. no detail surfaces)
			Shader_WorldZFillSurfChain( sortList, group, meshBuilder, nStartVert, SURFDRAW_NODE );
		}
		MSL_FOREACH_GROUP_END()
	}

	meshBuilder.End();
	pMesh->Draw();

	// FIXME: Do fast z reject on displacements!
}

//-----------------------------------------------------------------------------
// Call this after lists of stuff to render are made; it renders opaque surfaces
//-----------------------------------------------------------------------------
static void Shader_WorldEnd( CWorldRenderList *pRenderList, unsigned long flags, float waterZAdjust )
{
	VPROF("Shader_WorldEnd");

	CMatRenderContextPtr pRenderContext( materials );

	if ( flags & ( DRAWWORLDLISTS_DRAW_SHADOWDEPTH | DRAWWORLDLISTS_DRAW_SSAO ) )
	{
		Shader_WorldShadowDepthFill( pRenderList, flags );
		return;
	}

	// Draw the skybox
	if ( flags & DRAWWORLDLISTS_DRAW_SKYBOX )
	{
		if ( pRenderList->m_bSkyVisible || Map_VisForceFullSky() )
		{
			if( flags & DRAWWORLDLISTS_DRAW_CLIPSKYBOX )
			{
				R_DrawSkyBox( g_EngineRenderer->GetZFar() );
			}
			else
			{
				// Don't clip the skybox with height clip in this path.
				MaterialHeightClipMode_t nClipMode = pRenderContext->GetHeightClipMode();
				pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
				R_DrawSkyBox( g_EngineRenderer->GetZFar() );
				pRenderContext->SetHeightClipMode( nClipMode );
			}
		}
	}

	// Perform the fast z-fill pass
	bool bFastZReject = (r_fastzreject.GetInt() != 0);
	if ( bFastZReject )
	{
		Shader_WorldZFill( pRenderList, flags );
	}
	
	// Gotta draw each sort group
	// Draw the fog volume first, if there is one, because it turns out
	// that we only draw fog volumes if we're in the fog volume, which
	// means it's closer. We want to render closer things first to get
	// fast z-reject.
	int i;
	for ( i = MAX_MAT_SORT_GROUPS; --i >= 0; )
	{
		if ( !( flags & ( 1 << i ) ) )
			continue;

		int nSortGroup = s_DrawWorldListsToSortGroup[i];
		if ( nSortGroup == MAT_SORT_GROUP_WATERSURFACE  )
		{
			if ( waterZAdjust != 0.0f )
			{
				pRenderContext->MatrixMode( MATERIAL_MODEL );
				pRenderContext->PushMatrix();
				pRenderContext->LoadIdentity();
				pRenderContext->Translate( 0.0f, 0.0f, waterZAdjust );
			}
		}

		// Draws opaque displacement surfaces along with shadows, overlays, flashlights, etc.
		Shader_DrawDispChain( nSortGroup, pRenderList->m_DispSortList, flags, DEPTH_MODE_NORMAL );

		// Draws opaque non-displacement surfaces
		// This also add shadows to pRenderList->m_ShadowHandles.
		Shader_DrawChains( pRenderList, nSortGroup, false );
		AddProjectedTextureDecalsToList( pRenderList, nSortGroup );

		// Adds shadows to render lists
		for ( int j = pRenderList->m_ShadowHandles[nSortGroup].Count()-1; j >= 0; --j )
		{
			g_pShadowMgr->AddShadowsOnSurfaceToRenderList( pRenderList->m_ShadowHandles[nSortGroup].Element(j) );
		}
		pRenderList->m_ShadowHandles[nSortGroup].RemoveAll();

		// Don't stencil or scissor the flashlight if we're rendering to an offscreen view
		bool bFlashlightMask = !( (flags & DRAWWORLDLISTS_DRAW_REFRACTION ) || (flags & DRAWWORLDLISTS_DRAW_REFLECTION ));

		// Set masking stencil bits for flashlights
		g_pShadowMgr->SetFlashlightStencilMasks( bFlashlightMask );

		// Draw shadows and flashlights on world surfaces
		g_pShadowMgr->RenderFlashlights( bFlashlightMask );

		// Render the fragments from the surfaces + displacements.
		// FIXME: Actually, this call is irrelevant (for displacements) because it's done from
		// within DrawDispChain currently, but that should change.
		// We need to split out the disp decal rendering from DrawDispChain
		// and do it after overlays are rendered....
		OverlayMgr()->RenderOverlays( nSortGroup );
		g_pShadowMgr->DrawFlashlightOverlays( nSortGroup, bFlashlightMask );
		OverlayMgr()->ClearRenderLists( nSortGroup );

		// Draws decals lying on opaque non-displacement surfaces
		DecalSurfaceDraw( pRenderContext, nSortGroup );

		// Draw the flashlight lighting for the decals.
		g_pShadowMgr->DrawFlashlightDecals( nSortGroup, bFlashlightMask );

		// Draw RTT shadows
		g_pShadowMgr->RenderShadows( );
		g_pShadowMgr->ClearShadowRenderList();

		if ( nSortGroup == MAT_SORT_GROUP_WATERSURFACE && waterZAdjust != 0.0f )
		{
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();
		}
	}
}


//-----------------------------------------------------------------------------
// Renders translucent surfaces
//-----------------------------------------------------------------------------
bool Shader_LeafContainsTranslucentSurfaces( IWorldRenderList *pRenderListIn, int sortIndex, unsigned long flags )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	int i;
	for ( i = 0; i < MAX_MAT_SORT_GROUPS; ++i )
	{
		if( !( flags & ( 1 << i ) ) )
			continue;

		int sortGroup = s_DrawWorldListsToSortGroup[i];
	
		// Set the fog state here since it will be the same for all things
		// in this list of translucent objects (except for displacements)
		const surfacesortgroup_t &group = pRenderList->m_AlphaSortList.GetGroupForSortID( sortGroup, sortIndex );
		if ( group.surfaceCount )
			return true;
		const surfacesortgroup_t &dispGroup = pRenderList->m_DispAlphaSortList.GetGroupForSortID( sortGroup, sortIndex );
		if ( dispGroup.surfaceCount )
			return true;
	}

	return false;
}

void Shader_DrawTranslucentSurfaces( IWorldRenderList *pRenderListIn, int sortIndex, unsigned long flags, bool bShadowDepth )
{
	if ( !r_drawtranslucentworld.GetBool() )
		return;

	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);

	CMatRenderContextPtr pRenderContext( materials );

	bool skipLight = false;
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		skipLight = true;
	}



	// Gotta draw each sort group
	// Draw the fog volume first, if there is one, because it turns out
	// that we only draw fog volumes if we're in the fog volume, which
	// means it's closer. We want to render closer things first to get
	// fast z-reject.
	int i;
	CUtlVector<msurface2_t *> surfaceList;
	for ( i = 0; i < MAX_MAT_SORT_GROUPS; ++i )
	{
		if( !( flags & ( 1 << i ) ) )
		{
			continue;
		}
		int sortGroup = s_DrawWorldListsToSortGroup[i];
	
		// Set the fog state here since it will be the same for all things
		// in this list of translucent objects (except for displacements)

		surfaceList.RemoveAll();
		const surfacesortgroup_t &group = pRenderList->m_AlphaSortList.GetGroupForSortID( sortGroup, sortIndex );
		const surfacesortgroup_t &dispGroup = pRenderList->m_DispAlphaSortList.GetGroupForSortID( sortGroup, sortIndex );
		// Empty? skip...
		if (!group.surfaceCount && !dispGroup.surfaceCount )
			continue;

		pRenderList->m_AlphaSortList.GetSurfaceListForGroup( surfaceList, group );

		// Interate in back-to-front order
		for ( int listIndex = surfaceList.Count(); --listIndex >= 0; )
		{
			SurfaceHandle_t surfID = surfaceList[listIndex];
			pRenderContext->Bind( MSurf_TexInfo( surfID )->material );

			Assert( MSurf_MaterialSortID( surfID ) >= 0 && 
				    MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );

			if ( !skipLight )
			{
				pRenderContext->BindLightmapPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );
			}
			
//			NOTE: Since a static vb/dynamic ib IMesh doesn't buffer, we shouldn't use this
//			since it causes a lock and drawindexedprimitive per surface! (gary)
//			Shader_DrawSurfaceStatic( surfID );
			Shader_DrawSurfaceDynamic( pRenderContext, surfID, false );

//			g_pShadowMgr->ClearShadowRenderList();

			// Add shadows/flashlights to list.
			ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
			if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
			{
				g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
			}

			bool bFlashlightMask = !( (flags & DRAWWORLDLISTS_DRAW_REFRACTION ) || (flags & DRAWWORLDLISTS_DRAW_REFLECTION ));

			// Draw flashlights
			g_pShadowMgr->RenderFlashlights( bFlashlightMask );

			// Draw overlays on the surface.
			OverlayMgr()->AddFragmentListToRenderList( i, MSurf_OverlayFragmentList( surfID ), false );
			OverlayMgr()->RenderOverlays( i );

			// Draw flashlight overlays
			g_pShadowMgr->DrawFlashlightOverlays( i, bFlashlightMask );
			OverlayMgr()->ClearRenderLists( i );

		    // Draw decals on the surface
			DrawDecalsOnSingleSurface( pRenderContext, surfID );

			// Draw flashlight decals
			g_pShadowMgr->DrawFlashlightDecalsOnSingleSurface( surfID, bFlashlightMask );

			// draw shadows
			g_pShadowMgr->RenderShadows();
			g_pShadowMgr->ClearShadowRenderList();
		}
		// Draw wireframe, etc information
		DrawDebugInformation( surfaceList );

		// Now draw the translucent displacements; we need to do these *after* the
		// non-displacement surfaces because most likely the displacement will always
		// be in front (or at least not behind) the non-displacement translucent surfaces
		// that exist in the same leaf.
		
		// Draws translucent displacement surfaces

		surfaceList.RemoveAll();
		surfaceList.EnsureCapacity(dispGroup.surfaceCount);
		MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(pRenderList->m_DispAlphaSortList, dispGroup, surfID)
		{
			surfaceList.AddToTail(surfID);
		}
		MSL_FOREACH_SURFACE_IN_GROUP_END()

		DispInfo_RenderList( i, surfaceList.Base(), surfaceList.Count(), g_EngineRenderer->ViewGetCurrent().m_bOrtho, flags, DEPTH_MODE_NORMAL );
	}
}



//=============================================================
//
//	WORLD MODEL
//
//=============================================================

void FASTCALL R_DrawSurface( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	ASSERT_SURF_VALID( surfID );
	Assert( !SurfaceHasDispInfo( surfID ) );
	if ( MSurf_Flags( surfID ) & SURFDRAW_SKY )
	{
		pRenderList->m_bSkyVisible = true;
	}
//	else if ( surf->texinfo->material->IsTranslucent() )
	else if( MSurf_Flags( surfID ) & SURFDRAW_TRANS )
	{
		Shader_TranslucentWorldSurface( pRenderList, surfID );
	}
	else
	{
		Shader_WorldSurface( pRenderList, surfID );
	}
}

// The NoCull flavor of this function calls functions which optimize for shadow depth map rendering
void FASTCALL R_DrawSurfaceNoCull( CWorldRenderList *pRenderList, SurfaceHandle_t surfID )
{
	ASSERT_SURF_VALID( surfID );
	if( !(MSurf_Flags( surfID ) & SURFDRAW_TRANS) && !(MSurf_Flags( surfID ) & SURFDRAW_SKY) )
	{
		Shader_WorldSurfaceNoCull( pRenderList, surfID );
	}
}

//-----------------------------------------------------------------------------
// Draws displacements in a leaf
//-----------------------------------------------------------------------------
static inline void DrawDisplacementsInLeaf( CWorldRenderList *pRenderList, mleaf_t* pLeaf )
{
	// add displacement surfaces
	if (!pLeaf->dispCount)
		return;

	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );

		// NOTE: We're not using the displacement's touched method here 
		// because we're just using the parent surface's visframe in the
		// surface add methods below...
		SurfaceHandle_t parentSurfID = pDispInfo->GetParent();

		// already processed this frame? Then don't do it again!
		if ( VisitSurface( visitedSurfs, parentSurfID ) )
		{
			if ( MSurf_Flags( parentSurfID ) & SURFDRAW_TRANS)
			{
				Shader_TranslucentDisplacementSurface( pRenderList, parentSurfID );
			}
			else
			{
				Shader_DisplacementSurface( pRenderList, parentSurfID );
			}
		}
	}
}

int LeafToIndex( mleaf_t* pLeaf );

//-----------------------------------------------------------------------------
// Updates visibility + alpha lists
//-----------------------------------------------------------------------------
static inline void UpdateVisibleLeafLists( CWorldRenderList *pRenderList, mleaf_t* pLeaf )
{
	// Consistency check...
	MEM_ALLOC_CREDIT();
	
	// Add this leaf to the list of visible leafs
	int nLeafIndex = LeafToIndex( pLeaf );
	pRenderList->m_VisibleLeaves.AddToTail( nLeafIndex );
	int leafCount = pRenderList->m_VisibleLeaves.Count();
	pRenderList->m_VisibleLeafFogVolumes.AddToTail( pLeaf->leafWaterDataID );
	pRenderList->m_AlphaSortList.EnsureMaxSortIDs( leafCount );
	pRenderList->m_DispAlphaSortList.EnsureMaxSortIDs( leafCount );
}

 
//-----------------------------------------------------------------------------
// Draws all displacements + surfaces in a leaf
//-----------------------------------------------------------------------------
static void FASTCALL R_DrawLeaf( CWorldRenderList *pRenderList, mleaf_t *pleaf )
{
	// Add this leaf to the list of visible leaves
	UpdateVisibleLeafLists( pRenderList, pleaf );

	// Debugging to only draw at a particular leaf
#ifdef USE_CONVARS
	if ( (s_ShaderConvars.m_nDrawLeaf >= 0) && (s_ShaderConvars.m_nDrawLeaf != LeafToIndex(pleaf)) )
		return;
#endif

	// add displacement surfaces
	DrawDisplacementsInLeaf( pRenderList, pleaf );

#ifdef USE_CONVARS
	if( !s_ShaderConvars.m_bDrawWorld )
		return;
#endif

	// Add non-displacement surfaces
	int i;
	int nSurfaceCount = pleaf->nummarknodesurfaces;
	SurfaceHandle_t *pSurfID = &host_state.worldbrush->marksurfaces[pleaf->firstmarksurface];
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( i = 0; i < nSurfaceCount; ++i )
	{
		// garymctoptimize - can we prefetch the next surfaces?
		// We seem to be taking a huge hit here for referencing the surface for the first time.
		SurfaceHandle_t surfID = pSurfID[i];
		ASSERT_SURF_VALID( surfID );
		// there are never any displacements or nodraws in the leaf list
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
		Assert( (MSurf_Flags( surfID ) & SURFDRAW_NODE) );
		Assert( !SurfaceHasDispInfo(surfID) );
		// mark this one to be drawn at the node
		MarkSurfaceVisited( visitedSurfs, surfID );
	}

#ifdef USE_CONVARS
	if( !s_ShaderConvars.m_bDrawFuncDetail )
		return;
#endif

	for ( ; i < pleaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pSurfID[i];

		// Don't process the same surface twice
		if ( !VisitSurface( visitedSurfs, surfID ) )
			continue;

		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODE) );

		// Back face cull; only func_detail are drawn here
		if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
		{
			if ( (DotProduct(MSurf_Plane( surfID ).normal, modelorg) -
				  MSurf_Plane( surfID ).dist ) < BACKFACE_EPSILON )
				continue;
		}

		R_DrawSurface( pRenderList, surfID );
	}
}

static ConVar r_frustumcullworld( "r_frustumcullworld", "1" );

static void FASTCALL R_DrawLeafNoCull( CWorldRenderList *pRenderList, mleaf_t *pleaf )
{
	// Add this leaf to the list of visible leaves
	UpdateVisibleLeafLists( pRenderList, pleaf );

	// add displacement surfaces
	DrawDisplacementsInLeaf( pRenderList, pleaf );
	int i;
	SurfaceHandle_t *pSurfID = &host_state.worldbrush->marksurfaces[pleaf->firstmarksurface];
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( i = 0; i < pleaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pSurfID[i];

		// Don't process the same surface twice
		if ( !VisitSurface( visitedSurfs, surfID ) )
			continue;

		R_DrawSurfaceNoCull( pRenderList, surfID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: recurse on the BSP tree, calling the surface visitor
// Input  : *node - BSP node
//-----------------------------------------------------------------------------
static void R_RecursiveWorldNodeNoCull( CWorldRenderList *pRenderList, mnode_t *node, int nCullMask )
{
	int			side;
	cplane_t	*plane;
	float		dot;

	while (true)
	{
		// no polygons in solid nodes
		if (node->contents == CONTENTS_SOLID)
			return;		// solid

		// Check PVS signature
		if (node->visframe != r_visframecount)
			return;

		// Cull against the screen frustum or the appropriate area's frustum.
		if ( nCullMask != FRUSTUM_SUPPRESS_CLIPPING )
		{
			if (node->contents >= -1)
			{
				if ((nCullMask != 0) || ( node->area > 0 ))
				{
					if ( R_CullNode( &g_Frustum, node, nCullMask ) )
						return;
				}
			}
			else
			{
				// This prevents us from culling nodes that are too small to worry about
				if (node->contents == -2)
				{
					nCullMask = FRUSTUM_SUPPRESS_CLIPPING;
				}
			}
		}

		// if a leaf node, draw stuff
		if (node->contents >= 0)
		{
			R_DrawLeafNoCull( pRenderList, (mleaf_t *)node );
			return;
		}

		// node is just a decision point, so go down the appropriate sides

		// find which side of the node we are on
		plane = node->plane;
		if ( plane->type <= PLANE_Z )
		{
			dot = modelorg[plane->type] - plane->dist;
		}
		else
		{
			dot = DotProduct (modelorg, plane->normal) - plane->dist;
		}

		// recurse down the children, closer side first.
		// We have to do this because we need to find if the surfaces at this node
		// exist in any visible leaves closer to the camera than the node is. If so,
		// their r_surfacevisframe is set to indicate that we need to render them
		// at this node.
		side = dot >= 0 ? 0 : 1;

		// Recurse down the side closer to the camera
		R_RecursiveWorldNodeNoCull (pRenderList, node->children[side], nCullMask );

		// recurse down the side farther from the camera
		// NOTE: With this while loop, this is identical to just calling
		// R_RecursiveWorldNodeNoCull (node->children[!side], nCullMask );
		node = node->children[!side];
	}
}


//-----------------------------------------------------------------------------
// Purpose: recurse on the BSP tree, calling the surface visitor
// Input  : *node - BSP node
//-----------------------------------------------------------------------------

static void R_RecursiveWorldNode( CWorldRenderList *pRenderList, mnode_t *node, int nCullMask )
{
	int			side;
	cplane_t	*plane;
	float		dot;

	while (true)
	{
		// no polygons in solid nodes
		if (node->contents == CONTENTS_SOLID)
			return;		// solid

		// Check PVS signature
		if (node->visframe != r_visframecount)
			return;

		// Cull against the screen frustum or the appropriate area's frustum.
		if ( nCullMask != FRUSTUM_SUPPRESS_CLIPPING )
		{
			if (node->contents >= -1)
			{
				if ((nCullMask != 0) || ( node->area > 0 ))
				{
					if ( R_CullNode( &g_Frustum, node, nCullMask ) )
						return;
				}
			}
			else
			{
				// This prevents us from culling nodes that are too small to worry about
				if (node->contents == -2)
				{
					nCullMask = FRUSTUM_SUPPRESS_CLIPPING;
				}
			}
		}

		// if a leaf node, draw stuff
		if (node->contents >= 0)
		{
			R_DrawLeaf( pRenderList, (mleaf_t *)node );
			return;
		}

		// node is just a decision point, so go down the appropriate sides

		// find which side of the node we are on
		plane = node->plane;
		if ( plane->type <= PLANE_Z )
		{
			dot = modelorg[plane->type] - plane->dist;
		}
		else
		{
			dot = DotProduct (modelorg, plane->normal) - plane->dist;
		}

		// recurse down the children, closer side first.
		// We have to do this because we need to find if the surfaces at this node
		// exist in any visible leaves closer to the camera than the node is. If so,
		// their r_surfacevisframe is set to indicate that we need to render them
		// at this node.
		side = dot >= 0 ? 0 : 1;

		// Recurse down the side closer to the camera
		R_RecursiveWorldNode (pRenderList, node->children[side], nCullMask );

		// draw stuff on the node

		SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface );
		int i = MSurf_Index( surfID );
		int nLastSurface = i + node->numsurfaces;
		CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
		for ( ; i < nLastSurface; ++i, ++surfID )
		{
			// Only render things at this node that have previously been marked as visible
			if ( !VisitedSurface( visitedSurfs, i ) )
				continue;

			// Don't add surfaces that have displacement
			// UNDONE: Don't emit these at nodes in vbsp!
			// UNDONE: Emit them at the end of the surface list
			Assert( !SurfaceHasDispInfo( surfID ) );

			// If a surface is marked to draw at a node, then it's not a func_detail.
			// Only func_detail render at leaves. In the case of normal world surfaces,
			// we only want to render them if they intersect a visible leaf.
			int nFlags = MSurf_Flags( surfID );

			Assert( nFlags & SURFDRAW_NODE );

			Assert( !(nFlags & SURFDRAW_NODRAW) );

			if ( !(nFlags & SURFDRAW_UNDERWATER) && ( side ^ !!(nFlags & SURFDRAW_PLANEBACK)) )
				continue;		// wrong side

			R_DrawSurface( pRenderList, surfID );
		}

		// recurse down the side farther from the camera
		// NOTE: With this while loop, this is identical to just calling
		// R_RecursiveWorldNode (node->children[!side], nCullMask );
		node = node->children[!side];
	}
}


//-----------------------------------------------------------------------------
// Set up fog for a particular leaf
//-----------------------------------------------------------------------------
#define INVALID_WATER_HEIGHT 1000000.0f
inline float R_GetWaterHeight( int nFogVolume )
{
	if( nFogVolume < 0 || nFogVolume > host_state.worldbrush->numleafwaterdata )
		return INVALID_WATER_HEIGHT;

	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[nFogVolume];
	return pLeafWaterData->surfaceZ;
}

IMaterial *R_GetFogVolumeMaterial( int nFogVolume, bool bEyeInFogVolume )
{
	if( nFogVolume < 0 || nFogVolume > host_state.worldbrush->numleafwaterdata )
		return NULL;

	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[nFogVolume];
	mtexinfo_t* pTexInfo = &host_state.worldbrush->texinfo[pLeafWaterData->surfaceTexInfoID];

	IMaterial* pMaterial = pTexInfo->material;
	if( bEyeInFogVolume )
	{
		IMaterialVar *pVar = pMaterial->FindVar( "$bottommaterial", NULL );
		if( pVar )
		{
			const char *pMaterialName = pVar->GetStringValue();
			if( pMaterialName )
			{
				pMaterial = materials->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER );
			}
		}
	}
	return pMaterial;
}

void R_SetFogVolumeState( int fogVolume, bool useHeightFog )
{
	// useHeightFog == eye out of water
	// !useHeightFog == eye in water
	IMaterial *pMaterial = R_GetFogVolumeMaterial( fogVolume, !useHeightFog );
	mleafwaterdata_t* pLeafWaterData = &host_state.worldbrush->leafwaterdata[fogVolume];
	IMaterialVar* pFogColorVar	= pMaterial->FindVar( "$fogcolor", NULL );
	IMaterialVar* pFogEnableVar = pMaterial->FindVar( "$fogenable", NULL );
	IMaterialVar* pFogStartVar	= pMaterial->FindVar( "$fogstart", NULL );
	IMaterialVar* pFogEndVar	= pMaterial->FindVar( "$fogend", NULL );

	CMatRenderContextPtr pRenderContext( materials );

	if( pMaterial && pFogEnableVar->GetIntValueFast() && fog_enable_water_fog.GetBool() )
	{
		pRenderContext->SetFogZ( pLeafWaterData->surfaceZ );
		if( useHeightFog )
		{
			pRenderContext->FogMode( MATERIAL_FOG_LINEAR_BELOW_FOG_Z );
		}
		else
		{
			pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		}
		float fogColor[3];
		pFogColorVar->GetVecValueFast( fogColor, 3 );

		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( pFogStartVar->GetFloatValueFast() );
		pRenderContext->FogEnd( pFogEndVar->GetFloatValueFast() );
		pRenderContext->FogMaxDensity( 1.0 );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}

static inline bool R_CullNodeTopView( mnode_t *pNode )
{
	Vector2D delta, size;
	Vector2DSubtract( pNode->m_vecCenter.AsVector2D(), s_OrthographicCenter, delta );
	Vector2DAdd( pNode->m_vecHalfDiagonal.AsVector2D(), s_OrthographicHalfDiagonal, size );
	return ( FloatMakePositive( delta.x ) > size.x ) ||
		( FloatMakePositive( delta.y ) > size.y );
}


//-----------------------------------------------------------------------------
// Draws all displacements + surfaces in a leaf
//-----------------------------------------------------------------------------
static void R_DrawTopViewLeaf( CWorldRenderList *pRenderList, mleaf_t *pleaf )
{
	// Add this leaf to the list of visible leaves
	UpdateVisibleLeafLists( pRenderList, pleaf );

	// add displacement surfaces
	DrawDisplacementsInLeaf( pRenderList, pleaf );

#ifdef USE_CONVARS
	if( !s_ShaderConvars.m_bDrawWorld )
		return;
#endif

	// Add non-displacement surfaces
	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pleaf->firstmarksurface];
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	for ( int i = 0; i < pleaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];

		// Mark this surface as being in a visible leaf this frame. If this
		// surface is meant to be drawn at a node (SURFDRAW_NODE), 
		// then it will be drawn in the recursive code down below.
		if ( !VisitSurface( visitedSurfs, surfID ) )
			continue;

		// Don't add surfaces that have displacement; they are handled above
		// In fact, don't even set the vis frame; we need it unset for translucent
		// displacement code
		if ( SurfaceHasDispInfo(surfID) )
			continue;

		if ( MSurf_Flags( surfID ) & SURFDRAW_NODE )
			continue;

		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		// Back face cull; only func_detail are drawn here
		if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
		{
			if (MSurf_Plane( surfID ).normal.z <= 0.0f)
				continue;
		}

		// FIXME: For now, blow off translucent world polygons.
		// Gotta solve the problem of how to render them all, unsorted,
		// in a pass after the opaque world polygons, and before the
		// translucent entities.

		if ( !( MSurf_Flags( surfID ) & SURFDRAW_TRANS ))
//		if ( !surf->texinfo->material->IsTranslucent() )
			Shader_WorldSurface( pRenderList, surfID );
	}
}


//-----------------------------------------------------------------------------
// Fast path for rendering top-views
//-----------------------------------------------------------------------------
void R_RenderWorldTopView( CWorldRenderList *pRenderList, mnode_t *node )
{
	CVisitedSurfs &visitedSurfs = pRenderList->m_VisitedSurfs;
	do
	{
		// no polygons in solid nodes
		if (node->contents == CONTENTS_SOLID)
			return;		// solid

		// Check PVS signature
		if (node->visframe != r_visframecount)
			return;

		// Cull against the screen frustum or the appropriate area's frustum.
		if( R_CullNodeTopView( node ) )
			return;

		// if a leaf node, draw stuff
		if (node->contents >= 0)
		{
			R_DrawTopViewLeaf( pRenderList, (mleaf_t *)node );
			return;
		}

#ifdef USE_CONVARS
		if (s_ShaderConvars.m_bDrawWorld)
#endif
		{
			// draw stuff on the node
			SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface );
			for ( int i = 0; i < node->numsurfaces; i++, surfID++ )
			{
				if ( !VisitSurface( visitedSurfs, surfID ) )
					continue;

				// Don't add surfaces that have displacement
				if ( SurfaceHasDispInfo( surfID ) )
					continue;

				// If a surface is marked to draw at a node, then it's not a func_detail.
				// Only func_detail render at leaves. In the case of normal world surfaces,
				// we only want to render them if they intersect a visible leaf.
				Assert( (MSurf_Flags( surfID ) & SURFDRAW_NODE) );

				if ( MSurf_Flags( surfID ) & (SURFDRAW_UNDERWATER|SURFDRAW_SKY) )
					continue;

				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
				// Back face cull
				if ( (MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
				{
					if (MSurf_Plane( surfID ).normal.z <= 0.0f)
						continue;
				}

				// FIXME: For now, blow off translucent world polygons.
				// Gotta solve the problem of how to render them all, unsorted,
				// in a pass after the opaque world polygons, and before the
				// translucent entities.

				if ( !( MSurf_Flags( surfID ) & SURFDRAW_TRANS ) )
//				if ( !surf->texinfo->material->IsTranslucent() )
					Shader_WorldSurface( pRenderList, surfID );
			}
		}

		// Recurse down both children, we don't care the order...
		R_RenderWorldTopView ( pRenderList, node->children[0]);
		node = node->children[1];

	} while (node);
}

//-----------------------------------------------------------------------------
// Spews the leaf we're in
//-----------------------------------------------------------------------------
static void SpewLeaf()
{
	int leaf = CM_PointLeafnum( g_EngineRenderer->ViewOrigin() );
	ConMsg(	"view leaf %d\n", leaf );
}


//-----------------------------------------------------------------------------
// Main entry points for starting + ending rendering the world
//-----------------------------------------------------------------------------
void R_BuildWorldLists( IWorldRenderList *pRenderListIn, WorldListInfo_t* pInfo, 
	int iForceViewLeaf, const VisOverrideData_t* pVisData, bool bShadowDepth /* = false */, float *pWaterReflectionHeight )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	// Safety measure just in case. I haven't seen that we need this, but...
	if ( g_LostVideoMemory )
	{
		if (pInfo)
		{
			pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
			pInfo->m_LeafCount = 0;
			pInfo->m_pLeafList = pRenderList->m_VisibleLeaves.Base();
			pInfo->m_pLeafFogVolume = pRenderList->m_VisibleLeafFogVolumes.Base();
		}
		return;
	}

	VPROF( "R_BuildWorldLists" );
	VectorCopy( g_EngineRenderer->ViewOrigin(), modelorg );

#ifdef USE_CONVARS
	static ConVar r_spewleaf("r_spewleaf", "0");
	if ( r_spewleaf.GetInt() )
	{
		SpewLeaf();
	}
#endif

	Shader_WorldBegin( pRenderList );

	if ( !r_drawtopview )
	{
		R_SetupAreaBits( iForceViewLeaf, pVisData, pWaterReflectionHeight );

		if ( bShadowDepth )
		{
			R_RecursiveWorldNodeNoCull( pRenderList, host_state.worldbrush->nodes, r_frustumcullworld.GetBool() ? FRUSTUM_CLIP_ALL : FRUSTUM_SUPPRESS_CLIPPING );
		}
		else
		{
			R_RecursiveWorldNode( pRenderList, host_state.worldbrush->nodes, r_frustumcullworld.GetBool() ? FRUSTUM_CLIP_ALL : FRUSTUM_SUPPRESS_CLIPPING );
		}
	}
	else
	{
		R_RenderWorldTopView( pRenderList, host_state.worldbrush->nodes );
	}

		// This builds all lightmaps, including those for translucent surfaces
	// Don't bother in topview?
	if ( !r_drawtopview && !bShadowDepth )
	{
		Shader_BuildDynamicLightmaps( pRenderList );
	}

	// Return the back-to-front leaf ordering
	if ( pInfo )
	{
		// Compute fog volume info for rendering
		if ( !bShadowDepth )
		{
			FogVolumeInfo_t fogInfo;
			ComputeFogVolumeInfo( &fogInfo );
			if( fogInfo.m_InFogVolume )
			{
				pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_UNDERWATER;
			}
			else
			{
				pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
			}
		}
		else
		{
			pInfo->m_ViewFogVolume = MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
		}
		pInfo->m_LeafCount = pRenderList->m_VisibleLeaves.Count();
		pInfo->m_pLeafList = pRenderList->m_VisibleLeaves.Base();
		pInfo->m_pLeafFogVolume = pRenderList->m_VisibleLeafFogVolumes.Base();
	}
}


//-----------------------------------------------------------------------------
// Used to determine visible fog volumes
//-----------------------------------------------------------------------------
class CVisibleFogVolumeQuery
{
public:
	void FindVisibleFogVolume( const Vector &vecViewPoint, int *pVisibleFogVolume, int *pVisibleFogVolumeLeaf );

private:
	bool RecursiveGetVisibleFogVolume( mnode_t *node );

	// Input
	Vector m_vecSearchPoint;

	// Output
	int m_nVisibleFogVolume;
	int m_nVisibleFogVolumeLeaf;
};


//-----------------------------------------------------------------------------
// Main entry point for the query
//-----------------------------------------------------------------------------
void CVisibleFogVolumeQuery::FindVisibleFogVolume( const Vector &vecViewPoint, int *pVisibleFogVolume, int *pVisibleFogVolumeLeaf )
{
	R_SetupAreaBits();

	m_vecSearchPoint = vecViewPoint;
	m_nVisibleFogVolume = -1;
	m_nVisibleFogVolumeLeaf = -1;

	RecursiveGetVisibleFogVolume( host_state.worldbrush->nodes );

	*pVisibleFogVolume = m_nVisibleFogVolume;
	*pVisibleFogVolumeLeaf = m_nVisibleFogVolumeLeaf;
}


//-----------------------------------------------------------------------------
// return true to continue searching
//-----------------------------------------------------------------------------
bool CVisibleFogVolumeQuery::RecursiveGetVisibleFogVolume( mnode_t *node )
{
	int			side;
	cplane_t	*plane;
	float		dot;

	// no polygons in solid nodes
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid

	// Check PVS signature
	if (node->visframe != r_visframecount)
		return true;

	// Cull against the screen frustum or the appropriate area's frustum.
	int fixmeTempRemove = FRUSTUM_CLIP_ALL;
	if( R_CullNode( &g_Frustum, node, fixmeTempRemove ) )
		return true;

	// if a leaf node, check if we are in a fog volume and get outta here.
	if (node->contents >= 0)
	{
		mleaf_t *pLeaf = (mleaf_t *)node;

		// Don't return a leaf that's not filled with liquid
		if ( pLeaf->leafWaterDataID == -1 )
			return true;

		// Never return SLIME as being visible, as it's opaque
		if ( pLeaf->contents & CONTENTS_SLIME )
			return true;

		m_nVisibleFogVolume = pLeaf->leafWaterDataID;
		m_nVisibleFogVolumeLeaf = pLeaf - host_state.worldbrush->leafs;
		return false;  // found it, so stop searching
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;
	if ( plane->type <= PLANE_Z )
	{
		dot = m_vecSearchPoint[plane->type] - plane->dist;
	}
	else
	{
		dot = DotProduct( m_vecSearchPoint, plane->normal ) - plane->dist;
	}

	// recurse down the children, closer side first.
	// We have to do this because we need to find if the surfaces at this node
	// exist in any visible leaves closer to the camera than the node is. If so,
	// their r_surfacevisframe is set to indicate that we need to render them
	// at this node.
	side = (dot >= 0) ? 0 : 1;

	// Recurse down the side closer to the camera
	if( !RecursiveGetVisibleFogVolume (node->children[side]) )
		return false;

	// recurse down the side farther from the camera
	return RecursiveGetVisibleFogVolume (node->children[!side]);
}


static void ClearFogInfo( VisibleFogVolumeInfo_t *pInfo )
{
	pInfo->m_bEyeInFogVolume = false;
	pInfo->m_nVisibleFogVolume = -1;
	pInfo->m_nVisibleFogVolumeLeaf = -1;
	pInfo->m_pFogVolumeMaterial = NULL;
	pInfo->m_flWaterHeight = INVALID_WATER_HEIGHT;
}

ConVar fast_fogvolume("fast_fogvolume", "0");
//-----------------------------------------------------------------------------
// Main entry point from renderer to get the fog volume
//-----------------------------------------------------------------------------
void R_GetVisibleFogVolume( const Vector& vEyePoint, VisibleFogVolumeInfo_t *pInfo )
{
	VPROF_BUDGET( "R_GetVisibleFogVolume", VPROF_BUDGETGROUP_WORLD_RENDERING );

	if ( host_state.worldmodel->brush.pShared->numleafwaterdata == 0 )
	{
		ClearFogInfo( pInfo );
		return;
	}

	int nLeafID = CM_PointLeafnum( vEyePoint );
	mleaf_t* pLeaf = &host_state.worldbrush->leafs[nLeafID];

	int nLeafContents = pLeaf->contents;
	if ( pLeaf->leafWaterDataID != -1 )
	{
		Assert( nLeafContents & (CONTENTS_SLIME | CONTENTS_WATER) );
		pInfo->m_bEyeInFogVolume = true;
		pInfo->m_nVisibleFogVolume = pLeaf->leafWaterDataID;
		pInfo->m_nVisibleFogVolumeLeaf = nLeafID;
		pInfo->m_pFogVolumeMaterial = R_GetFogVolumeMaterial( pInfo->m_nVisibleFogVolume, true );
		pInfo->m_flWaterHeight = R_GetWaterHeight( pInfo->m_nVisibleFogVolume );
	}
	else if ( nLeafContents & CONTENTS_TESTFOGVOLUME )
	{
		Assert( (nLeafContents & (CONTENTS_SLIME | CONTENTS_WATER)) == 0 );
		if ( fast_fogvolume.GetBool() && host_state.worldbrush->numleafwaterdata == 1 )
		{
			pInfo->m_nVisibleFogVolume = 0;
			pInfo->m_nVisibleFogVolumeLeaf = host_state.worldbrush->leafwaterdata[0].firstLeafIndex;
		}
		else
		{
			CVisibleFogVolumeQuery query;
			query.FindVisibleFogVolume( vEyePoint, &pInfo->m_nVisibleFogVolume, &pInfo->m_nVisibleFogVolumeLeaf ); 
		}

		pInfo->m_bEyeInFogVolume = false;
		pInfo->m_pFogVolumeMaterial = R_GetFogVolumeMaterial( pInfo->m_nVisibleFogVolume, false );
		pInfo->m_flWaterHeight = R_GetWaterHeight( pInfo->m_nVisibleFogVolume );
	}
	else
	{
		ClearFogInfo( pInfo );
	}

	if( host_state.worldbrush->m_LeafMinDistToWater )
	{
		pInfo->m_flDistanceToWater = ( float )host_state.worldbrush->m_LeafMinDistToWater[nLeafID];
	}
	else
	{
		pInfo->m_flDistanceToWater = 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Draws the list of surfaces build in the BuildWorldLists phase
//-----------------------------------------------------------------------------

// Uncomment this to allow code to draw wireframe over a particular surface for debugging
//#define DEBUG_SURF 1

#ifdef DEBUG_SURF
int g_DebugSurfIndex = -1;
#endif

void R_DrawWorldLists( IWorldRenderList *pRenderListIn, unsigned long flags, float waterZAdjust )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	if ( g_bTextMode || g_LostVideoMemory )
		return;

	VPROF("R_DrawWorldLists");
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__  );
	Shader_WorldEnd( pRenderList, flags, waterZAdjust );

#ifdef DEBUG_SURF
	{
		VPROF("R_DrawWorldLists (DEBUG_SURF)");
		if (g_pDebugSurf)
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->Bind( g_materialWorldWireframe );
			Shader_DrawSurfaceDynamic( pRenderContext, g_pDebugSurf, false );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void R_SceneBegin( void )
{
	ComputeDebugSettings();
}

void R_SceneEnd( void )
{
}

//-----------------------------------------------------------------------------
// Debugging code to draw the lightmap pages
//-----------------------------------------------------------------------------

void Shader_DrawLightmapPageSurface( SurfaceHandle_t surfID, float red, float green, float blue )
{
	Vector2D lightCoords[32][4];

	int bumpID, count;
	if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
	{
		count = NUM_BUMP_VECTS + 1;
	}
	else
	{
		count = 1;
	}

	BuildMSurfaceVerts( host_state.worldbrush, surfID, NULL, NULL, lightCoords );

	int lightmapPageWidth, lightmapPageHeight;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_materialWireframe );
	materials->GetLightmapPageSize( 
		SortInfoToLightmapPage(MSurf_MaterialSortID( surfID )), 
		&lightmapPageWidth, &lightmapPageHeight );

	for( bumpID = 0; bumpID < count; bumpID++ )
	{
		// assumes that we are already in ortho mode.
		IMesh* pMesh = pRenderContext->GetDynamicMesh( );
					
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, MSurf_VertCount( surfID ) );
		
		int i;

		for( i = 0; i < MSurf_VertCount( surfID ); i++ )
		{
			float x, y;
			float *texCoord;

			texCoord = &lightCoords[i][bumpID][0];

			x = lightmapPageWidth * texCoord[0];
			y = lightmapPageHeight * texCoord[1];
#ifdef _XBOX
			// xboxissue - border safe
			x += 32;
			y += 32;
#endif			
			meshBuilder.Position3f( x, y, 0.0f );
			meshBuilder.AdvanceVertex();

			texCoord = &lightCoords[(i+1)%MSurf_VertCount( surfID )][bumpID][0];
			x = lightmapPageWidth * texCoord[0];
			y = lightmapPageHeight * texCoord[1];
#ifdef _XBOX
			// xboxissue - border safe
			x += 32;
			y += 32;
#endif			
			meshBuilder.Position3f( x, y, 0.0f );
			meshBuilder.AdvanceVertex();
		}
		
		meshBuilder.End();
		pMesh->Draw();
	}
}

void Shader_DrawLightmapPageChains( IWorldRenderList *pRenderListIn, int pageId )
{
	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);
	for (int j = 0; j < MAX_MAT_SORT_GROUPS; ++j)
	{
		MSL_FOREACH_GROUP_BEGIN( pRenderList->m_SortList, j, group )
		{
			SurfaceHandle_t surfID = pRenderList->m_SortList.GetSurfaceAtHead( group );
			Assert(IS_SURF_VALID(surfID));
			Assert( MSurf_MaterialSortID( surfID ) >= 0 && MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
			if( materialSortInfoArray[MSurf_MaterialSortID( surfID ) ].lightmapPageID != pageId )
			{
				continue;
			}
			MSL_FOREACH_SURFACE_IN_GROUP_BEGIN( pRenderList->m_SortList, group, surfIDList )
			{
				Assert( !SurfaceHasDispInfo( surfIDList ) );
				Shader_DrawLightmapPageSurface( surfIDList, 0.0f, 1.0f, 0.0f );
			}
			MSL_FOREACH_SURFACE_IN_GROUP_END()
		}
		MSL_FOREACH_GROUP_END()

		// render displacement lightmap page info
		MSL_FOREACH_SURFACE_BEGIN(pRenderList->m_DispSortList, j, surfID)
		{
			surfID->pDispInfo->RenderWireframeInLightmapPage( pageId );
		}
		MSL_FOREACH_SURFACE_END()
	}
}

//-----------------------------------------------------------------------------
//
// All code related to brush model rendering
//
//-----------------------------------------------------------------------------

class CBrushSurface : public IBrushSurface
{
public:
	CBrushSurface( SurfaceHandle_t surfID );

	// Computes texture coordinates + lightmap coordinates given a world position
	virtual void ComputeTextureCoordinate( const Vector& worldPos, Vector2D& texCoord );
	virtual void ComputeLightmapCoordinate( const Vector& worldPos, Vector2D& lightmapCoord );

	// Gets the vertex data for this surface
	virtual int  GetVertexCount() const;
	virtual void GetVertexData( BrushVertex_t* pVerts );

	// Gets at the material properties for this surface
	virtual IMaterial* GetMaterial();

private:
	SurfaceHandle_t m_SurfaceID;
	SurfaceCtx_t m_Ctx;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBrushSurface::CBrushSurface( SurfaceHandle_t surfID ) : m_SurfaceID(surfID)
{
	Assert(IS_SURF_VALID(surfID));
	SurfSetupSurfaceContext( m_Ctx, surfID );
}


//-----------------------------------------------------------------------------
// Computes texture coordinates + lightmap coordinates given a world position
//-----------------------------------------------------------------------------
void CBrushSurface::ComputeTextureCoordinate( const Vector& worldPos, Vector2D& texCoord )
{
	SurfComputeTextureCoordinate( m_Ctx, m_SurfaceID, worldPos, texCoord );
}

void CBrushSurface::ComputeLightmapCoordinate( const Vector& worldPos, Vector2D& lightmapCoord )
{
	SurfComputeLightmapCoordinate( m_Ctx, m_SurfaceID, worldPos, lightmapCoord );
}


//-----------------------------------------------------------------------------
// Gets the vertex data for this surface
//-----------------------------------------------------------------------------
int  CBrushSurface::GetVertexCount() const
{
	if( !SurfaceHasPrims( m_SurfaceID ) )
	{
		// Create a temporary vertex array for the data...
		return MSurf_VertCount( m_SurfaceID );
	}
	else
	{
		// not implemented yet
		Assert(0);
		return 0;
	}
}

void CBrushSurface::GetVertexData( BrushVertex_t* pVerts )
{
	Assert( pVerts );

	if( !SurfaceHasPrims( m_SurfaceID ) )
	{
		// Fill in the vertex data
		BuildBrushModelVertexArray( host_state.worldbrush, m_SurfaceID, pVerts );
	}
	else
	{
		// not implemented yet
		Assert(0);
	}
}


//-----------------------------------------------------------------------------
// Activates fast z reject for displacements
//-----------------------------------------------------------------------------
void R_FastZRejectDisplacements( bool bEnable )
{
	s_bFastZRejectDisplacements = bEnable;
}


//-----------------------------------------------------------------------------
// Gets at the material properties for this surface
//-----------------------------------------------------------------------------
IMaterial* CBrushSurface::GetMaterial()
{
	return MSurf_TexInfo( m_SurfaceID )->material;
}

//-----------------------------------------------------------------------------
// Installs a client-side renderer for brush models
//-----------------------------------------------------------------------------
void R_InstallBrushRenderOverride( IBrushRenderer* pBrushRenderer )
{
	s_pBrushRenderOverride = pBrushRenderer;
}

//-----------------------------------------------------------------------------
// Here, we allow the client DLL to render brush surfaces however they'd like
// NOTE: This involves a vertex copy, so don't use this everywhere
//-----------------------------------------------------------------------------

bool Shader_DrawBrushSurfaceOverride( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, IClientEntity *baseentity )
{
	// Set the lightmap state
	Shader_SetChainLightmapState( pRenderContext, surfID );

	CBrushSurface brushSurface( surfID );
	return s_pBrushRenderOverride->RenderBrushModelSurface( baseentity, &brushSurface );
}


FORCEINLINE void ModulateMaterial( IMaterial *pMaterial, float *pOldColor )
{
	if ( g_bIsBlendingOrModulating )
	{
		pOldColor[3] = pMaterial->GetAlphaModulation( );
		pMaterial->GetColorModulation( &pOldColor[0], &pOldColor[1], &pOldColor[2] );
		pMaterial->AlphaModulate( r_blend );
		pMaterial->ColorModulate( r_colormod[0], r_colormod[1], r_colormod[2] );
	}

}

FORCEINLINE void UnModulateMaterial( IMaterial *pMaterial, float *pOldColor )
{
	if ( g_bIsBlendingOrModulating )
	{
		pMaterial->AlphaModulate( pOldColor[3] );
		pMaterial->ColorModulate( pOldColor[0], pOldColor[1], pOldColor[2] );
	}
}

//-----------------------------------------------------------------------------
// Main method to draw brush surfaces
//-----------------------------------------------------------------------------
void Shader_BrushSurface( SurfaceHandle_t surfID, model_t *model, IClientEntity *baseentity )
{
	CMatRenderContextPtr pRenderContext(materials);
	float pOldColor[4];

	bool drawDecals;
	if (!s_pBrushRenderOverride)
	{
		drawDecals = true;

		IMaterial *pMaterial = MSurf_TexInfo( surfID )->material;

		ModulateMaterial( pMaterial, pOldColor );
		Shader_SetChainTextureState( pRenderContext, surfID, baseentity, false );

//		NOTE: Since a static vb/dynamic ib IMesh doesn't buffer, we shouldn't use this
//		since it causes a lock and drawindexedprimitive per surface! (gary)
//		Shader_DrawSurfaceStatic( surfID );
		Shader_DrawSurfaceDynamic( pRenderContext, surfID, false );

		// FIXME: This may cause an unnecessary flush to occur!
		// Thankfully, this is a rare codepath. I don't think anything uses it now.
		UnModulateMaterial( pMaterial, pOldColor );
	}
	else
	{
		drawDecals = Shader_DrawBrushSurfaceOverride( pRenderContext, surfID, baseentity );
	}

	// fixme: need to get "allowDecals" from the material
	//	if ( g_BrushProperties.allowDecals && pSurf->pdecals ) 
	if( SurfaceHasDecals( surfID ) && drawDecals )
	{
		DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
	}

	// Add overlay fragments to list.
	// FIXME: A little code support is necessary to get overlays working on brush models
//	OverlayMgr()->AddFragmentListToRenderList( MSurf_OverlayFragmentList( surfID ), false );

	// Add shadows too....
	ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
	if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
	{
		g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
	}
}


// UNDONE: These are really guesses.  Do we ever exceed these limits?
const int MAX_TRANS_NODES = 256;
const int MAX_TRANS_DECALS = 256;
const int MAX_TRANS_BATCHES = 1024;
const int MAX_TRANS_SURFACES = 1024;

class CBrushBatchRender
{
public:
	// These are the compact structs produced by the brush render cache.  The goal is to have a compact
	// list of drawing instructions for drawing an opaque brush model in the most optimal order.
	// These structs contain ONLY the opaque surfaces of a brush model.
	struct brushrendersurface_t
	{
		short	surfaceIndex;
		short	planeIndex;
	};

	// a batch is a list of surfaces with the same material - they can be drawn with one call to the materialsystem
	struct brushrenderbatch_t
	{
		short	firstSurface;
		short	surfaceCount;
		IMaterial *pMaterial;
		int		sortID;
		int		indexCount;
	};

	// a mesh is a list of batches with the same vertex format.
	struct brushrendermesh_t
	{
		short		firstBatch;
		short		batchCount;
	};

	// This is the top-level struct containing all data necessary to render an opaque brush model in optimal order
	struct brushrender_t
	{
		// UNDONE: Compact these arrays into a single allocation
		// UNDONE: Compact entire struct to a single allocation?  Store brushrender_t * in the linked list?
		void Free()
		{
			delete[] pPlanes;
			delete[] pMeshes;
			delete[] pBatches;
			delete[] pSurfaces;
			pPlanes = NULL;
			pMeshes = NULL;
			pBatches = NULL;
			pSurfaces = NULL;
		}

		cplane_t				**pPlanes;
		brushrendermesh_t		*pMeshes;			
		brushrenderbatch_t		*pBatches;
		brushrendersurface_t	*pSurfaces;
		short planeCount;
		short meshCount;
		short batchCount;
		short surfaceCount;
		short totalIndexCount;
		short totalVertexCount;
	};

	// Surfaces are stored in a list like this temporarily for sorting purposes only.  The compact structs do not store these.
	struct surfacelist_t
	{
		SurfaceHandle_t surfID;
		short	surfaceIndex;
		short	planeIndex;
	};
	
	// These are the compact structs produced for translucent brush models.  These structs contain
	// only the translucent surfaces of a brush model.

	// a batch is a list of surfaces with the same material - they can be drawn with one call to the materialsystem
	struct transbatch_t
	{
		short	firstSurface;
		short	surfaceCount;
		IMaterial *pMaterial;
		int		sortID;
		int		indexCount;
	};

	// This is a list of surfaces that have decals.
	struct transdecal_t
	{
		short firstSurface;
		short surfaceCount;
	};

	// A node is the list of batches that can be drawn without sorting errors.  When no decals are present, surfaces
	// from the next node may be appended to this one to improve performance without causing sorting errors.
	struct transnode_t
	{
		short			firstBatch;
		short			batchCount;
		short			firstDecalSurface;
		short			decalSurfaceCount;
	};

	// This is the top-level struct containing all data necessary to render a translucent brush model in optimal order.
	// NOTE: Unlike the opaque struct, the order of the batches is view-dependent, so caching this is pointless since 
	// the view usually changes.
	struct transrender_t
	{
		transnode_t		nodes[MAX_TRANS_NODES];
		SurfaceHandle_t	surfaces[MAX_TRANS_SURFACES];
		SurfaceHandle_t	decalSurfaces[MAX_TRANS_DECALS];
		transbatch_t	batches[MAX_TRANS_BATCHES];
		transbatch_t	*pLastBatch;	// These are used to append surfaces to existing batches across nodes.
		transnode_t		*pLastNode;		// This improves performance.
		short			nodeCount;
		short			batchCount;
		short			surfaceCount;
		short			decalSurfaceCount;
	};

	// Builds a transrender_t, then executes it's drawing commands
	void DrawTranslucentBrushModel( model_t *model, IClientEntity *baseentity )
	{
		transrender_t renderT;
		renderT.pLastBatch = NULL;
		renderT.pLastNode = NULL;
		renderT.nodeCount = 0;
		renderT.surfaceCount = 0;
		renderT.batchCount = 0;
		renderT.decalSurfaceCount = 0;
		BuildTransLists_r( renderT, model, model->brush.pShared->nodes + model->brush.firstnode );
		void *pProxyData = baseentity ? baseentity->GetClientRenderable() : NULL;
		DrawTransLists( renderT, pProxyData );
	}

	void AddSurfaceToBatch( transrender_t &renderT, transnode_t *pNode, transbatch_t *pBatch, SurfaceHandle_t surfID )
	{
		pBatch->surfaceCount++;
		Assert( renderT.surfaceCount < MAX_TRANS_SURFACES);

		pBatch->indexCount += (MSurf_VertCount( surfID )-2)*3;
		renderT.surfaces[renderT.surfaceCount] = surfID;
		renderT.surfaceCount++;
		if ( SurfaceHasDecals( surfID ) )
		{
			Assert( renderT.decalSurfaceCount < MAX_TRANS_DECALS);
			pNode->decalSurfaceCount++;
			renderT.decalSurfaces[renderT.decalSurfaceCount] = surfID;
			renderT.decalSurfaceCount++;
		}
	}

	void AddTransNode( transrender_t &renderT )
	{
		renderT.pLastNode = &renderT.nodes[renderT.nodeCount];
		renderT.nodeCount++;
		Assert( renderT.nodeCount < MAX_TRANS_NODES);
		renderT.pLastBatch = NULL;
		renderT.pLastNode->firstBatch = renderT.batchCount;
		renderT.pLastNode->firstDecalSurface = renderT.decalSurfaceCount;
		renderT.pLastNode->batchCount = 0;
		renderT.pLastNode->decalSurfaceCount = 0;
	}

	void AddTransBatch( transrender_t &renderT, SurfaceHandle_t surfID )
	{
		transbatch_t &batch = renderT.batches[renderT.pLastNode->firstBatch + renderT.pLastNode->batchCount];
		Assert( renderT.batchCount < MAX_TRANS_BATCHES);
		renderT.pLastNode->batchCount++;
		renderT.batchCount++;
		batch.firstSurface = renderT.surfaceCount;
		batch.surfaceCount = 0;
		batch.pMaterial = MSurf_TexInfo( surfID )->material;
		batch.sortID = MSurf_MaterialSortID( surfID );
		batch.indexCount = 0;
		renderT.pLastBatch = &batch;
		AddSurfaceToBatch( renderT, renderT.pLastNode, &batch, surfID );
	}

	// build node lists
	void BuildTransLists_r( transrender_t &renderT, model_t *model, mnode_t *node )
	{
		float		dot;

		if (node->contents >= 0)
			return;

		// node is just a decision point, so go down the apropriate sides
		// find which side of the node we are on
		cplane_t *plane = node->plane;
		if ( plane->type <= PLANE_Z )
		{
			dot = modelorg[plane->type] - plane->dist;
		}
		else
		{
			dot = DotProduct (modelorg, plane->normal) - plane->dist;
		}

		int side = (dot >= 0) ? 0 : 1;

		// back side first - translucent surfaces need to render in back to front order
		// to appear correctly.
		BuildTransLists_r( renderT, model, node->children[!side]);

		// emit all surfaces on node
		CUtlVectorFixed<surfacelist_t, 256> sortList;
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface, model->brush.pShared );
		for ( int i = 0; i < node->numsurfaces; i++, surfID++ )
		{
			// skip opaque surfaces
			if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
			{
				if ( ((MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0) )
				{
					// Backface cull here, so they won't do any more work
					if ( ( side ^ !!(MSurf_Flags( surfID ) & SURFDRAW_PLANEBACK)) )
						continue;
				}

				// If this can be appended to the previous batch, do so
				int sortID = MSurf_MaterialSortID( surfID );
				if ( renderT.pLastBatch && renderT.pLastBatch->sortID == sortID )
				{
					AddSurfaceToBatch( renderT, renderT.pLastNode, renderT.pLastBatch, surfID );
				}
				else
				{
					// save it off for sorting, then a later append
					int sortIndex = sortList.AddToTail();
					sortList[sortIndex].surfID = surfID;
				}
			}
		}

		// We've got surfaces on this node that can't be added to the previous node
		if ( sortList.Count() )
		{
			// sort by material
			sortList.Sort( SurfaceCmp );
			
			// add a new sort group
			AddTransNode( renderT );
			int lastSortID = -1;
			// now add the optimal number of batches to that group
			for ( int i = 0; i < sortList.Count(); i++ )
			{
				surfID = sortList[i].surfID;
				int sortID = MSurf_MaterialSortID( surfID );
				if ( lastSortID == sortID )
				{
					// can be drawn in a single call with the current list of surfaces, append
					AddSurfaceToBatch( renderT, renderT.pLastNode, renderT.pLastBatch, surfID );
				}
				else
				{
					// requires a break (material/lightmap change).
					AddTransBatch( renderT, surfID );
					lastSortID = sortID;
				}
			}

			// don't batch across decals or decals will sort incorrectly
			if ( renderT.pLastNode->decalSurfaceCount )
			{
				renderT.pLastNode = NULL;
				renderT.pLastBatch = NULL;
			}
		}

		// front side
		BuildTransLists_r( renderT, model, node->children[side]);
	}

	void DrawTransLists( transrender_t &renderT, void *pProxyData )
	{
		CMatRenderContextPtr pRenderContext( materials );

		PIXEVENT( pRenderContext, "DrawTransLists" );

		bool skipLight = false;
		if ( g_pMaterialSystemConfig->nFullbright == 1 )
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
			skipLight = true;
		}

		float pOldColor[4];


		for ( int i = 0; i < renderT.nodeCount; i++ )
		{
			int j;
			const transnode_t &node = renderT.nodes[i];
			for ( j = 0; j < node.batchCount; j++ )
			{
				const transbatch_t &batch = renderT.batches[node.firstBatch+j];
				
#ifdef NEWMESH
				CIndexBufferBuilder indexBufferBuilder;
#else
				CMeshBuilder meshBuilder;
#endif
				IMaterial *pMaterial = batch.pMaterial;

				ModulateMaterial( pMaterial, pOldColor );

				if ( !skipLight )
				{
					pRenderContext->BindLightmapPage( materialSortInfoArray[batch.sortID].lightmapPageID );
				}
				pRenderContext->Bind( pMaterial, pProxyData );

#ifdef NEWMESH
				IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer( MATERIAL_INDEX_FORMAT_16BIT, false );
				indexBufferBuilder.Begin( pBuildIndexBuffer, batch.indexCount );
#else
				IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[batch.sortID], NULL, NULL );
				meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, batch.indexCount );
#endif

				for ( int k = 0; k < batch.surfaceCount; k++ )
				{
					SurfaceHandle_t surfID = renderT.surfaces[batch.firstSurface + k];
					Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
#ifdef NEWMESH
					BuildIndicesForSurface( indexBufferBuilder, surfID );
#else
					BuildIndicesForSurface( meshBuilder, surfID );
#endif

				}
#ifdef NEWMESH
				indexBufferBuilder.End( false ); // haven't tested this one yet (alpha blended world geom I think)
				// FIXME: IMaterial::GetVertexFormat() should do this stripping (add a separate 'SupportsCompression' accessor)
				VertexFormat_t vertexFormat = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
				pRenderContext->BindVertexBuffer( 0, g_WorldStaticMeshes[batch.sortID], 0, vertexFormat );
				pRenderContext->BindIndexBuffer( pBuildIndexBuffer, 0 );
				pRenderContext->Draw( MATERIAL_TRIANGLES, 0, batch.indexCount );
#else
				meshBuilder.End( false, true );
#endif

				// Don't leave the material in a bogus state
				UnModulateMaterial( pMaterial, pOldColor );
			}

			if ( node.decalSurfaceCount )
			{
				for ( j = 0; j < node.decalSurfaceCount; j++ )
				{
					SurfaceHandle_t surfID = renderT.decalSurfaces[node.firstDecalSurface + j];
					Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
					if( SurfaceHasDecals( surfID ) )
					{
						DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
					}

					// Add shadows too....
					ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
					if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
					{
						g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
					}
				}

				// Now draw the decals + shadows for this node
				// This order relative to the surfaces is important for translucency to work correctly.
				DecalSurfaceDraw(pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP);

				// FIXME: Decals are not being rendered while illuminated by the flashlight
				DecalSurfacesInit( true );

				// Draw all shadows on the brush
				g_pShadowMgr->RenderProjectedTextures( );
			}
			if ( g_ShaderDebug.anydebug )
			{
				CUtlVector<msurface2_t *> brushList;
				for ( j = 0; j < node.batchCount; j++ )
				{
					const transbatch_t &batch = renderT.batches[node.firstBatch+j];
					for ( int k = 0; k < batch.surfaceCount; k++ )
					{
						brushList.AddToTail( renderT.surfaces[batch.firstSurface + k] );
					}
				}
				DrawDebugInformation( brushList );
			}
		}
	}

	static int __cdecl SurfaceCmp(const surfacelist_t *s0, const surfacelist_t *s1 );

	void LevelInit();
	brushrender_t *FindOrCreateRenderBatch( model_t *pModel );
	void DrawOpaqueBrushModel( IClientEntity *baseentity, model_t *model, const Vector& origin, ERenderDepthMode DepthMode );
	void DrawTranslucentBrushModel( IClientEntity *baseentity, model_t *model, const Vector& origin, bool bShadowDepth, bool bDrawOpaque, bool bDrawTranslucent );
	void DrawBrushModelShadow( model_t *model, IClientRenderable *pRenderable );

private:
	void ClearRenderHandles();

	CUtlLinkedList<brushrender_t> m_renderList;
};

int __cdecl CBrushBatchRender::SurfaceCmp(const surfacelist_t *s0, const surfacelist_t *s1 )
{
	int sortID0 = MSurf_MaterialSortID( s0->surfID );
	int sortID1 = MSurf_MaterialSortID( s1->surfID );

	return sortID0 - sortID1;
}


CBrushBatchRender g_BrushBatchRenderer;

//-----------------------------------------------------------------------------
// Purpose: This is used when the mat_dxlevel is changed to reset the brush
//          models.
//-----------------------------------------------------------------------------
void R_BrushBatchInit( void )
{
	g_BrushBatchRenderer.LevelInit();
}

void CBrushBatchRender::LevelInit()
{
	unsigned short iNext;
	for( unsigned short i=m_renderList.Head(); i != m_renderList.InvalidIndex(); i=iNext )
	{
		iNext = m_renderList.Next(i);
		m_renderList.Element(i).Free();
	}

	m_renderList.Purge();

	ClearRenderHandles();
}

void CBrushBatchRender::ClearRenderHandles( void )
{
	for ( int iBrush = 1 ; iBrush < host_state.worldbrush->numsubmodels ; ++iBrush )
	{
		char szBrushModel[5]; // inline model names "*1", "*2" etc
		Q_snprintf( szBrushModel, sizeof( szBrushModel ), "*%i", iBrush );
		model_t *pModel = modelloader->GetModelForName( szBrushModel, IModelLoader::FMODELLOADER_SERVER );	
		if ( pModel )
		{
			pModel->brush.renderHandle = 0;
		}
	}
}

// Create a compact, optimal list of rendering commands for the opaque parts of a brush model
// NOTE: This just skips translucent surfaces assuming a separate transrender_t pass!
CBrushBatchRender::brushrender_t *CBrushBatchRender::FindOrCreateRenderBatch( model_t *pModel )
{
	if ( !pModel->brush.nummodelsurfaces )
		return NULL;

	unsigned short index = pModel->brush.renderHandle - 1;

	if ( m_renderList.IsValidIndex( index ) )
		return &m_renderList.Element(index);

	index = m_renderList.AddToTail();
	pModel->brush.renderHandle = index + 1;
	brushrender_t &renderT = m_renderList.Element(index);
	renderT.pPlanes = NULL;
	renderT.pMeshes = NULL;
	renderT.planeCount = 0;
	renderT.meshCount = 0;
	renderT.totalIndexCount = 0;
	renderT.totalVertexCount = 0;

	CUtlVector<cplane_t *>	planeList;
	CUtlVector<surfacelist_t> surfaceList;

	int i;

	SurfaceHandle_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
	for (i=0 ; i<pModel->brush.nummodelsurfaces; i++, surfID++)
	{
		// UNDONE: For now, just draw these in a separate pass
		if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
			continue;

		cplane_t *plane = surfID->plane;
		int planeIndex = planeList.Find(plane);
		if ( planeIndex == -1 )
		{
			planeIndex = planeList.AddToTail( plane );
		}
		surfacelist_t tmp;
		tmp.surfID = surfID;
		tmp.surfaceIndex = i;
		tmp.planeIndex = planeIndex;
		surfaceList.AddToTail( tmp );
	}
	surfaceList.Sort( SurfaceCmp );
	renderT.pPlanes = new cplane_t *[planeList.Count()];
	renderT.planeCount = planeList.Count();
	memcpy( renderT.pPlanes, planeList.Base(), sizeof(cplane_t *)*planeList.Count() );
	renderT.pSurfaces = new brushrendersurface_t[surfaceList.Count()];
	renderT.surfaceCount = surfaceList.Count();

	int meshCount = 0;
	int batchCount = 0;
	int lastSortID = -1;
#ifdef NEWMESH
	IVertexBuffer *pLastVertexBuffer = NULL;
#else
	IMesh *pLastMesh = NULL;
#endif
	brushrendermesh_t *pMesh = NULL;
	brushrendermesh_t tmpMesh[MAX_VERTEX_FORMAT_CHANGES];
	brushrenderbatch_t *pBatch = NULL;
	brushrenderbatch_t tmpBatch[128];

	for ( i = 0; i < surfaceList.Count(); i++ )
	{
		renderT.pSurfaces[i].surfaceIndex = surfaceList[i].surfaceIndex;
		renderT.pSurfaces[i].planeIndex = surfaceList[i].planeIndex;

		surfID = surfaceList[i].surfID;
		int sortID = MSurf_MaterialSortID( surfID );
#ifdef NEWMESH
		if ( g_WorldStaticMeshes[sortID] != pLastVertexBuffer )
#else
		if ( g_WorldStaticMeshes[sortID] != pLastMesh )
#endif
		{
			pMesh = tmpMesh + meshCount;
			pMesh->firstBatch = batchCount;
			pMesh->batchCount = 0;
			lastSortID = -1; // force a new batch
			meshCount++;
		}
		if ( sortID != lastSortID )
		{
			pBatch = tmpBatch + batchCount;
			pBatch->firstSurface = i;
			pBatch->surfaceCount = 0;
			pBatch->sortID = sortID;
			pBatch->pMaterial = MSurf_TexInfo( surfID )->material;
			pBatch->indexCount = 0;
			pMesh->batchCount++;
			batchCount++;
		}
#ifdef NEWMESH
		pLastVertexBuffer = g_WorldStaticMeshes[sortID];
#else
		pLastMesh = g_WorldStaticMeshes[sortID];
#endif
		lastSortID = sortID;
		pBatch->surfaceCount++;
		int vertCount = MSurf_VertCount( surfID );
		int indexCount = (vertCount - 2) * 3;
		pBatch->indexCount += indexCount;
		renderT.totalIndexCount += indexCount;
		renderT.totalVertexCount += vertCount;
	}

	renderT.pMeshes = new brushrendermesh_t[meshCount];
	memcpy( renderT.pMeshes, tmpMesh, sizeof(brushrendermesh_t) * meshCount );
	renderT.meshCount = meshCount;
	renderT.pBatches = new brushrenderbatch_t[batchCount];
	memcpy( renderT.pBatches, tmpBatch, sizeof(brushrenderbatch_t) * batchCount );
	renderT.batchCount = batchCount;
	return &renderT;
}


//-----------------------------------------------------------------------------
// Draws an opaque (parts of a) brush model
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawOpaqueBrushModel( IClientEntity *baseentity, model_t *model, const Vector& origin, ERenderDepthMode DepthMode )
{
	VPROF( "R_DrawOpaqueBrushModel" );
	SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );

	brushrender_t *pRender = FindOrCreateRenderBatch( model );
	int i;
	if ( !pRender )
		return;

	bool skipLight = false;
	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "DrawOpaqueBrushModel" );

	if ( (g_pMaterialSystemConfig->nFullbright == 1) || DepthMode == DEPTH_MODE_SHADOW )
	{
		pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		skipLight = true;
	}

	void *pProxyData = baseentity ? baseentity->GetClientRenderable() : NULL;
	bool backface[1024];
	Assert( pRender->planeCount < 1024 );

	// NOTE: Backface culling is almost no perf gain.  Can be removed from brush model rendering.
	// Check the shared planes once
	for ( i = 0; i < pRender->planeCount; i++ )
	{
		float dot = DotProduct( modelorg, pRender->pPlanes[i]->normal) - pRender->pPlanes[i]->dist;
		backface[i] = ( DepthMode == DEPTH_MODE_NORMAL && ( dot < BACKFACE_EPSILON ) ) ? true : false; // don't backface cull when rendering to shadow map
	}

	float pOldColor[4];

	for ( i = 0; i < pRender->meshCount; i++ )
	{
		brushrendermesh_t &mesh = pRender->pMeshes[i];
		for ( int j = 0; j < mesh.batchCount; j++ )
		{
			brushrenderbatch_t &batch = pRender->pBatches[mesh.firstBatch + j];

			int k;
			for ( k = 0; k < batch.surfaceCount; k++ )
			{
				brushrendersurface_t &surface = pRender->pSurfaces[batch.firstSurface + k];
				if ( !backface[surface.planeIndex] )
					break;
			}

			if ( k == batch.surfaceCount )
				continue;

			CMeshBuilder meshBuilder;
			IMaterial *pMaterial = NULL;

			if ( DepthMode != DEPTH_MODE_NORMAL )
			{
				// Select proper override material
				int nAlphaTest = (int) batch.pMaterial->IsAlphaTested();
				int nNoCull = (int) batch.pMaterial->IsTwoSided();
				IMaterial *pDepthWriteMaterial;
				
				if ( DepthMode == DEPTH_MODE_SSA0 )
				{
					pDepthWriteMaterial = g_pMaterialSSAODepthWrite[nAlphaTest][nNoCull];
				}
				else
				{
					pDepthWriteMaterial = g_pMaterialDepthWrite[nAlphaTest][nNoCull];
				}

				if ( nAlphaTest == 1 )
				{
					static unsigned int originalTextureVarCache = 0;
					IMaterialVar *pOriginalTextureVar = batch.pMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
					static unsigned int originalTextureFrameVarCache = 0;
					IMaterialVar *pOriginalTextureFrameVar = batch.pMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
					static unsigned int originalAlphaRefCache = 0;
					IMaterialVar *pOriginalAlphaRefVar = batch.pMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

					static unsigned int textureVarCache = 0;
					IMaterialVar *pTextureVar = pDepthWriteMaterial->FindVarFast( "$basetexture", &textureVarCache );
					static unsigned int textureFrameVarCache = 0;
					IMaterialVar *pTextureFrameVar = pDepthWriteMaterial->FindVarFast( "$frame", &textureFrameVarCache );
					static unsigned int alphaRefCache = 0;
					IMaterialVar *pAlphaRefVar = pDepthWriteMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

					if( pTextureVar && pOriginalTextureVar )
					{
						pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
					}

					if( pTextureFrameVar && pOriginalTextureFrameVar )
					{
						pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
					}

					if( pAlphaRefVar && pOriginalAlphaRefVar )
					{
						pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
					}
				}

				pMaterial = pDepthWriteMaterial;
			}
			else
			{
				pMaterial = batch.pMaterial;

				// Store off the old color + alpha
				ModulateMaterial( pMaterial, pOldColor );
				if ( !skipLight )
				{
					pRenderContext->BindLightmapPage( materialSortInfoArray[batch.sortID].lightmapPageID );
				}
			}

			pRenderContext->Bind( pMaterial, pProxyData );
#ifdef NEWMESH
			IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer( MATERIAL_INDEX_FORMAT_16BIT, false );
			CIndexBufferBuilder indexBufferBuilder;
			indexBufferBuilder.Begin( pBuildIndexBuffer, batch.indexCount );
#else
			IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[batch.sortID], NULL, NULL );
			meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, batch.indexCount );
#endif

			for ( ; k < batch.surfaceCount; k++ )
			{
				brushrendersurface_t &surface = pRender->pSurfaces[batch.firstSurface + k];
				if ( backface[surface.planeIndex] )
					continue;
				SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;

				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
#ifdef NEWMESH
				BuildIndicesForSurface( indexBufferBuilder, surfID );
#else
				BuildIndicesForSurface( meshBuilder, surfID );
#endif

				if( SurfaceHasDecals( surfID ) && DepthMode == DEPTH_MODE_NORMAL )
				{
					DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
				}

				// Add overlay fragments to list.
				// FIXME: A little code support is necessary to get overlays working on brush models
				//	OverlayMgr()->AddFragmentListToRenderList( MSurf_OverlayFragmentList( surfID ), false );

				if ( DepthMode == DEPTH_MODE_NORMAL )
				{
					// Add render-to-texture shadows too....
					ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
					if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
					{
						g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
					}
				}
			}
			
#ifdef NEWMESH
			indexBufferBuilder.End( false ); // this one is broken (opaque brush model. .tv)
			pRenderContext->BindVertexBuffer( 0, g_WorldStaticMeshes[batch.sortID], 0, g_WorldStaticMeshes[batch.sortID]->GetVertexFormat() );
			pRenderContext->BindIndexBuffer( pBuildIndexBuffer, 0 );
			pRenderContext->Draw( MATERIAL_TRIANGLES, 0, pBuildIndexBuffer->NumIndices() );//batch.indexCount );
#else
			meshBuilder.End( false, true );
#endif

			if ( DepthMode == DEPTH_MODE_NORMAL )
			{
				// Don't leave the material in a bogus state
				UnModulateMaterial( pMaterial, pOldColor );
			}
		}
	}

	if ( DepthMode != DEPTH_MODE_NORMAL )
	{
		return;
	}

	if ( g_ShaderDebug.anydebug )
	{
		for ( i = 0; i < pRender->meshCount; i++ )
		{
			brushrendermesh_t &mesh = pRender->pMeshes[i];
			CUtlVector<msurface2_t *> brushList;
			for ( int j = 0; j < mesh.batchCount; j++ )
			{
				brushrenderbatch_t &batch = pRender->pBatches[mesh.firstBatch + j];
				for ( int k = 0; k < batch.surfaceCount; k++ )
				{
					brushrendersurface_t &surface = pRender->pSurfaces[batch.firstSurface + k];
					if ( backface[surface.planeIndex] )
						continue;
					SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;
					brushList.AddToTail(surfID);
				}
			}
			// now draw debug for each drawn surface
			DrawDebugInformation( brushList );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws an translucent (sorted) brush model
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawTranslucentBrushModel( IClientEntity *baseentity, model_t *model, const Vector& origin, bool bShadowDepth, bool bDrawOpaque, bool bDrawTranslucent )
{
	if ( bDrawOpaque )
	{
		DrawOpaqueBrushModel( baseentity, model, origin, bShadowDepth ? DEPTH_MODE_SHADOW : DEPTH_MODE_NORMAL );
	}

	if ( !bShadowDepth && bDrawTranslucent )
	{
		DrawTranslucentBrushModel( model, baseentity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a brush model shadow for render-to-texture shadows
//-----------------------------------------------------------------------------
// UNDONE: This is reasonable, but it could be much faster as follows:
// Build a vertex buffer cache.  A block-allocated static mesh with 1024 verts
// per block or something.
// When a new brush is encountered, fill it in to the current block or the 
// next one (first fit allocator).  Then this routine could simply draw
// a static mesh with a single index buffer build, draw call (no dynamic vb).
void CBrushBatchRender::DrawBrushModelShadow( model_t *model, IClientRenderable *pRenderable )
{
	brushrender_t *pRender = FindOrCreateRenderBatch( (model_t *)model );
	if ( !pRender )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_pMaterialShadowBuild, pRenderable );

	// Draws all surfaces in the brush model in arbitrary order
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, pRender->totalVertexCount, pRender->totalIndexCount );

	for ( int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
	{
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
			continue;

		int startVert = MSurf_FirstVertIndex( surfID );
		int vertCount = MSurf_VertCount( surfID );
		int startIndex = meshBuilder.GetCurrentVertex();
		int j;
		for ( j = 0; j < vertCount; j++ )
		{
			int vertIndex = model->brush.pShared->vertindices[startVert + j];

			// world-space vertex
			meshBuilder.Position3fv( model->brush.pShared->vertexes[vertIndex].position.Base() );
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
			meshBuilder.AdvanceVertex();
		}

		for ( j = 0; j < vertCount-2; j++ )
		{
			meshBuilder.FastIndex( startIndex );
			meshBuilder.FastIndex( startIndex + j + 1 );
			meshBuilder.FastIndex( startIndex + j + 2 );
		}
	}
	meshBuilder.End();
	pMesh->Draw();
}

void R_Surface_LevelInit()
{
	g_BrushBatchRenderer.LevelInit();
	// reset this to the default at the start of each level
	g_MaxLeavesVisible = 512;
}


void R_Surface_LevelShutdown()
{
	CWorldRenderList::PurgeAll();
}

//-----------------------------------------------------------------------------
static void R_DrawBrushModel_Override( IClientEntity *baseentity, model_t *model, const Vector& origin )
{
	VPROF( "R_DrawOpaqueBrushModel_Override" );
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	for (int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
	{
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		Shader_BrushSurface( surfID, model, baseentity );
	}
	// now draw debug for each drawn surface
	if ( g_ShaderDebug.anydebug )
	{
		CUtlVector<msurface2_t *> surfaceList;
		surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
		for (int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
		{
			surfaceList.AddToTail(surfID);
		}
		DrawDebugInformation( surfaceList );
	}
}

int R_MarkDlightsOnBrushModel( model_t *model, IClientRenderable *pRenderable )
{
	int count = 0;
	if ( g_bActiveDlights )
	{
		extern int R_MarkLights (dlight_t *light, int bit, mnode_t *node);

		g_BrushToWorldMatrix.SetupMatrixOrgAngles( pRenderable->GetRenderOrigin(), pRenderable->GetRenderAngles() );
		Vector saveOrigin;

		for (int k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.GetTime()) ||
				(!cl_dlights[k].IsRadiusGreaterThanZero()))
				continue;

			VectorCopy( cl_dlights[k].origin, saveOrigin );
			cl_dlights[k].origin = g_BrushToWorldMatrix.VMul4x3Transpose( saveOrigin );

			mnode_t *node = model->brush.pShared->nodes + model->brush.firstnode;
			if ( IsBoxIntersectingSphereExtents( node->m_vecCenter, node->m_vecHalfDiagonal, cl_dlights[k].origin, cl_dlights[k].GetRadius() ) )
			{
				count += R_MarkLights( &cl_dlights[k], 1<<k, node );
			}
			VectorCopy( saveOrigin, cl_dlights[k].origin );
		}
		if ( count )
		{
			model->flags |= MODELFLAG_HAS_DLIGHT;
		}
		g_BrushToWorldMatrix.Identity();
	}
	return count;
}


//-----------------------------------------------------------------------------
// Stuff to do right before and after brush model rendering
//-----------------------------------------------------------------------------
void Shader_BrushBegin( model_t *model, IClientEntity *baseentity /*=NULL*/ )
{
	// Clear out the render list of decals
	DecalSurfacesInit( true );

	// Clear out the render lists of shadows
	g_pShadowMgr->ClearShadowRenderList( );
}

void Shader_BrushEnd( IMatRenderContext *pRenderContext, VMatrix const* pBrushToWorld, model_t *model, bool bShadowDepth, IClientEntity *baseentity /* = NULL */ )
{
	if ( bShadowDepth )
		return;

	DecalSurfaceDraw(pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP);

	// draw the flashlight lighting for the decals on the brush.
	g_pShadowMgr->DrawFlashlightDecals( BRUSHMODEL_DECAL_SORT_GROUP, false );

	// Draw all shadows on the brush
	g_pShadowMgr->RenderProjectedTextures( pBrushToWorld );
}

class CBrushModelTransform
{
public:
	CBrushModelTransform( const Vector &origin, const QAngle &angles, IMatRenderContext *pRenderContext )
	{
		bool rotated = ( angles[0] || angles[1] || angles[2] );
		m_bIdentity = (origin == vec3_origin) && (!rotated);

		// Don't change state if we don't need to
		if (!m_bIdentity)
		{
			m_savedModelorg = modelorg;
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PushMatrix();
			g_BrushToWorldMatrix.SetupMatrixOrgAngles( origin, angles );
			pRenderContext->LoadMatrix( g_BrushToWorldMatrix );
			modelorg = g_BrushToWorldMatrix.VMul4x3Transpose(g_EngineRenderer->ViewOrigin());
		}
	}
	~CBrushModelTransform()
	{
		if ( !m_bIdentity )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();
			g_BrushToWorldMatrix.Identity();
			modelorg = m_savedModelorg;
		}
	}

	VMatrix *GetNonIdentityMatrix()
	{
		return m_bIdentity ? NULL : &g_BrushToWorldMatrix;
	}

	inline bool IsIdentity() { return m_bIdentity; }

	Vector	m_savedModelorg;
	bool	m_bIdentity;
};

//-----------------------------------------------------------------------------
// Purpose: Draws a brush model using the global shader/surfaceVisitor
// Input  : *e - entity to draw
// Output : void R_DrawBrushModel
//-----------------------------------------------------------------------------
void R_DrawBrushModel( IClientEntity *baseentity, model_t *model, 
	const Vector& origin, const QAngle& angles, ERenderDepthMode DepthMode, bool bDrawOpaque, bool bDrawTranslucent )
{
	VPROF( "R_DrawBrushModel" );

#ifdef USE_CONVARS
	if ( !r_drawbrushmodels.GetInt() )
	{
		return;
	}
	bool bWireframe = false;
	if ( r_drawbrushmodels.GetInt() == 2 )
	{
		// save and override
		bWireframe = g_ShaderDebug.wireframe;
		g_ShaderDebug.wireframe = true;
		g_ShaderDebug.anydebug = true;
	}
#endif

	CMatRenderContextPtr pRenderContext( materials );
	CBrushModelTransform brushTransform( origin, angles, pRenderContext );

	Assert(model->brush.firstmodelsurface != 0);

	// Draw the puppy...
	Shader_BrushBegin( model, baseentity );
	
	if ( model->flags & MODELFLAG_FRAMEBUFFER_TEXTURE )
	{
		CMatRenderContextPtr pRenderContextMat( materials );
		pRenderContext->CopyRenderTargetToTexture( pRenderContextMat->GetFrameBufferCopyTexture( 0 ) );
	}

	if ( s_pBrushRenderOverride )
	{
		R_DrawBrushModel_Override( baseentity, model, origin );
	}
	else
	{
		if ( model->flags & MODELFLAG_TRANSLUCENT )
		{
			if ( DepthMode == DEPTH_MODE_NORMAL )
			{
				g_BrushBatchRenderer.DrawTranslucentBrushModel( baseentity, model, origin, false, bDrawOpaque, bDrawTranslucent );
			}
		}
		else if ( bDrawOpaque )
		{
			g_BrushBatchRenderer.DrawOpaqueBrushModel( baseentity, model, origin, DepthMode );
		}
	}

	Shader_BrushEnd( pRenderContext, brushTransform.GetNonIdentityMatrix(), model, DepthMode != DEPTH_MODE_NORMAL, baseentity );

#ifdef USE_CONVARS
	if ( r_drawbrushmodels.GetInt() == 2 )
	{
		// restore
		g_ShaderDebug.wireframe = bWireframe;
		g_ShaderDebug.TestAnyDebug();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Draws a brush model shadow for render-to-texture shadows
//-----------------------------------------------------------------------------

void R_DrawBrushModelShadow( IClientRenderable *pRenderable )
{
	if( !r_drawbrushmodels.GetInt() )
		return;

	model_t *model = (model_t *)pRenderable->GetModel();
	const Vector& origin = pRenderable->GetRenderOrigin();
	QAngle const& angles = pRenderable->GetRenderAngles();

	CMatRenderContextPtr pRenderContext( materials );
	CBrushModelTransform brushTransform( origin, angles, pRenderContext );
	g_BrushBatchRenderer.DrawBrushModelShadow( model, pRenderable );
}


void R_DrawIdentityBrushModel( IWorldRenderList *pRenderListIn, model_t *model )
{
	if ( !model )
		return;

	CWorldRenderList *pRenderList = assert_cast<CWorldRenderList *>(pRenderListIn);

	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	
	for (int j=0 ; j<model->brush.nummodelsurfaces ; ++j, surfID++)
	{
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		// FIXME: Can't insert translucent stuff into the list
		// of translucent surfaces because we don't know what leaf
		// we're in. At the moment, the client doesn't add translucent
		// brushes to the identity brush model list
//		Assert ( (psurf->flags & SURFDRAW_TRANS ) == 0 );

		// OPTIMIZE: Backface cull these guys?!?!?
		if ( MSurf_Flags( surfID ) & SURFDRAW_TRANS)
//		if ( psurf->texinfo->material->IsTranslucent() )
		{
			Shader_TranslucentWorldSurface( pRenderList, surfID );
		}
		else
		{
			Shader_WorldSurface( pRenderList, surfID );
		}
	}
}
#endif
//-----------------------------------------------------------------------------
// Converts leaf pointer to index
//-----------------------------------------------------------------------------
inline int LeafToIndex( mleaf_t* pLeaf )
{
	return pLeaf - host_state.worldbrush->leafs;
}


//-----------------------------------------------------------------------------
// Structures to help out with enumeration
//-----------------------------------------------------------------------------
enum
{
	ENUM_SPHERE_TEST_X = 0x1,
	ENUM_SPHERE_TEST_Y = 0x2,
	ENUM_SPHERE_TEST_Z = 0x4,

	ENUM_SPHERE_TEST_ALL = 0x7
};

struct EnumLeafBoxInfo_t
{
	VectorAligned m_vecBoxMax;
	VectorAligned m_vecBoxMin;
	VectorAligned m_vecBoxCenter;
	VectorAligned m_vecBoxHalfDiagonal;
	ISpatialLeafEnumerator *m_pIterator;
	int	m_nContext;
};

struct EnumLeafSphereInfo_t
{
	Vector m_vecCenter;
	float m_flRadius;
	Vector m_vecBoxCenter;
	Vector m_vecBoxHalfDiagonal;
	ISpatialLeafEnumerator *m_pIterator;
	int	m_nContext;
};

//-----------------------------------------------------------------------------
// Finds all leaves of the BSP tree within a particular volume
//-----------------------------------------------------------------------------
static bool EnumerateLeafInBox_R(mnode_t *node, EnumLeafBoxInfo_t& info )
{
	// no polygons in solid nodes (don't report these leaves either)
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid

	// rough cull...
	if (!IsBoxIntersectingBoxExtents(node->m_vecCenter, node->m_vecHalfDiagonal, 
		info.m_vecBoxCenter, info.m_vecBoxHalfDiagonal))
	{
		return true;
	}
	
	if (node->contents >= 0)
	{
		// if a leaf node, report it to the iterator...
		return info.m_pIterator->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), info.m_nContext ); 
	}

	// Does the node plane split the box?
	// find which side of the node we are on
	cplane_t* plane = node->plane;
	if ( plane->type <= PLANE_Z )
	{
		if (info.m_vecBoxMax[plane->type] <= plane->dist)
		{
			return EnumerateLeafInBox_R( node->children[1], info );
		}
		else if (info.m_vecBoxMin[plane->type] >= plane->dist)
		{
			return EnumerateLeafInBox_R( node->children[0], info );
		}
		else
		{
			// Here the box is split by the node
			bool ret = EnumerateLeafInBox_R( node->children[0], info );
			if (!ret)
				return false;

			return EnumerateLeafInBox_R( node->children[1], info );
		}
	}

	// Arbitrary split plane here
	Vector cornermin, cornermax;
	for (int i = 0; i < 3; ++i)
	{
		if (plane->normal[i] >= 0)
		{
			cornermin[i] = info.m_vecBoxMin[i];
			cornermax[i] = info.m_vecBoxMax[i];
		}
		else
		{
			cornermin[i] = info.m_vecBoxMax[i];
			cornermax[i] = info.m_vecBoxMin[i];
		}
	}

	if (DotProduct( plane->normal, cornermax ) <= plane->dist)
	{
		return EnumerateLeafInBox_R( node->children[1], info );
	}
	else if (DotProduct( plane->normal, cornermin ) >= plane->dist)
	{
		return EnumerateLeafInBox_R( node->children[0], info );
	}
	else
	{
		// Here the box is split by the node
		bool ret = EnumerateLeafInBox_R( node->children[0], info );
		if (!ret)
			return false;

		return EnumerateLeafInBox_R( node->children[1], info );
	}
}

#ifdef _X360

static fltx4 AlignThatVector(const Vector &vc)
{
	fltx4 out = __loadunalignedvector(vc.Base());

	/*
	out.x = vc.x;
	out.y = vc.y;
	out.z = vc.z;
	*/

	// squelch the w component 
	return __vrlimi( out, __vzero(), 1, 0 );
}

//-----------------------------------------------------------------------------
// Finds all leaves of the BSP tree within a particular volume
//-----------------------------------------------------------------------------
static bool EnumerateLeafInBox_R(mnode_t * RESTRICT node, const EnumLeafBoxInfo_t * RESTRICT pInfo )
{
	// no polygons in solid nodes (don't report these leaves either)
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid

	// speculatively get the children into the cache
	__dcbt(0,node->children[0]);
	__dcbt(0,node->children[1]);

	// constructing these here prevents LHS if we spill.
	// it's not quite a quick enough operation to do extemporaneously.
	fltx4 infoBoxCenter = LoadAlignedSIMD(pInfo->m_vecBoxCenter);
	fltx4 infoBoxHalfDiagonal = LoadAlignedSIMD(pInfo->m_vecBoxHalfDiagonal);

	Assert(IsBoxIntersectingBoxExtents(AlignThatVector(node->m_vecCenter), AlignThatVector(node->m_vecHalfDiagonal), 
			LoadAlignedSIMD(pInfo->m_vecBoxCenter), LoadAlignedSIMD(pInfo->m_vecBoxHalfDiagonal)) ==
			IsBoxIntersectingBoxExtents((node->m_vecCenter), node->m_vecHalfDiagonal,
			pInfo->m_vecBoxCenter, pInfo->m_vecBoxHalfDiagonal));


	// rough cull...
	if (!IsBoxIntersectingBoxExtents(LoadAlignedSIMD(node->m_vecCenter), LoadAlignedSIMD(node->m_vecHalfDiagonal), 
		infoBoxCenter, infoBoxHalfDiagonal))
	{
		return true;
	}

	if (node->contents >= 0)
	{
		// if a leaf node, report it to the iterator...
		return pInfo->m_pIterator->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), pInfo->m_nContext ); 
	}

	// Does the node plane split the box?
	// find which side of the node we are on
	cplane_t* RESTRICT plane = node->plane;
	if ( plane->type <= PLANE_Z )
	{
		if (pInfo->m_vecBoxMax[plane->type] <= plane->dist)
		{
			return EnumerateLeafInBox_R( node->children[1], pInfo );
		}
		else if (pInfo->m_vecBoxMin[plane->type] >= plane->dist)
		{
			return EnumerateLeafInBox_R( node->children[0], pInfo );
		}
		else
		{
			// Here the box is split by the node
			return EnumerateLeafInBox_R( node->children[0], pInfo ) && 
				   EnumerateLeafInBox_R( node->children[1], pInfo );
		}
	}

	// Arbitrary split plane here
	/*
	Vector cornermin, cornermax;
	for (int i = 0; i < 3; ++i)
	{
		if (plane->normal[i] >= 0)
		{
			cornermin[i] = info.m_vecBoxMin[i];
			cornermax[i] = info.m_vecBoxMax[i];
		}
		else
		{
			cornermin[i] = info.m_vecBoxMax[i];
			cornermax[i] = info.m_vecBoxMin[i];
		}
	}
	*/
	
	// take advantage of high throughput/high latency
	fltx4 planeNormal = LoadUnaligned3SIMD( plane->normal.Base() );
	fltx4 vecBoxMin = LoadAlignedSIMD(pInfo->m_vecBoxMin);
	fltx4 vecBoxMax = LoadAlignedSIMD(pInfo->m_vecBoxMax);
	fltx4 cornermin, cornermax;
	// by now planeNormal is ready...
	fltx4 control = XMVectorGreaterOrEqual( planeNormal, __vzero() );
	// now control[i] = planeNormal[i] > 0 ? 0xFF : 0x00
	cornermin = XMVectorSelect( vecBoxMax, vecBoxMin, control); // cornermin[i] = control[i] ? vecBoxMin[i] : vecBoxMax[i]
	cornermax = XMVectorSelect( vecBoxMin, vecBoxMax, control);

	// compute dot products
	fltx4 dotCornerMax = __vmsum3fp(planeNormal, cornermax); // vsumfp ignores w component
	fltx4 dotCornerMin = __vmsum3fp(planeNormal, cornermin);
	fltx4 vPlaneDist = ReplicateX4(plane->dist);
	UINT conditionRegister;
	XMVectorGreaterR(&conditionRegister,vPlaneDist,dotCornerMax);
	if (XMComparisonAllTrue(conditionRegister)) // plane->normal . cornermax <= plane->dist
		return EnumerateLeafInBox_R( node->children[1], pInfo );

	XMVectorGreaterOrEqualR(&conditionRegister,dotCornerMin,vPlaneDist);
	if ( XMComparisonAllTrue(conditionRegister) )
		return EnumerateLeafInBox_R( node->children[0], pInfo );

	return EnumerateLeafInBox_R( node->children[0], pInfo ) &&
		   EnumerateLeafInBox_R( node->children[1], pInfo );

	/*
	if (DotProduct( plane->normal, cornermax ) <= plane->dist)
	{
		return EnumerateLeafInBox_R( node->children[1], info, infoBoxCenter, infoBoxHalfDiagonal );
	}
	else if (DotProduct( plane->normal, cornermin ) >= plane->dist)
	{
		return EnumerateLeafInBox_R( node->children[0], info, infoBoxCenter, infoBoxHalfDiagonal );
	}
	else
	{
		// Here the box is split by the node
		bool ret = EnumerateLeafInBox_R( node->children[0], info, infoBoxCenter, infoBoxHalfDiagonal );
		if (!ret)
			return false;

		return EnumerateLeafInBox_R( node->children[1], info, infoBoxCenter, infoBoxHalfDiagonal );
	}
	*/
}
#endif

//-----------------------------------------------------------------------------
// Returns all leaves that lie within a spherical volume
//-----------------------------------------------------------------------------
bool EnumerateLeafInSphere_R( mnode_t *node, EnumLeafSphereInfo_t& info, int nTestFlags )
{
	while (true)
	{
		// no polygons in solid nodes (don't report these leaves either)
		if (node->contents == CONTENTS_SOLID)
			return true;		// solid

		if (node->contents >= 0)
		{
			// leaf cull...
			// NOTE: using nTestFlags here means that we may be passing in some 
			// leaves that don't actually intersect the sphere, but instead intersect
			// the box that surrounds the sphere.
			if (nTestFlags)
			{
				if (!IsBoxIntersectingSphereExtents (node->m_vecCenter, node->m_vecHalfDiagonal, info.m_vecCenter, info.m_flRadius))
					return true;
			}

			// if a leaf node, report it to the iterator...
			return info.m_pIterator->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), info.m_nContext ); 
		}
		else if (nTestFlags)
		{
			if (node->contents == -1)
			{
				// faster cull...
				if (nTestFlags & ENUM_SPHERE_TEST_X)
				{
					float flDelta = FloatMakePositive( node->m_vecCenter.x - info.m_vecBoxCenter.x );
					float flSize = node->m_vecHalfDiagonal.x + info.m_vecBoxHalfDiagonal.x;
					if ( flDelta > flSize )
						return true;

					// This checks for the node being completely inside the box...
					if ( flDelta + node->m_vecHalfDiagonal.x < info.m_vecBoxHalfDiagonal.x )
						nTestFlags &= ~ENUM_SPHERE_TEST_X;
				}

				if (nTestFlags & ENUM_SPHERE_TEST_Y)
				{
					float flDelta = FloatMakePositive( node->m_vecCenter.y - info.m_vecBoxCenter.y );
					float flSize = node->m_vecHalfDiagonal.y + info.m_vecBoxHalfDiagonal.y;
					if ( flDelta > flSize )
						return true;

					// This checks for the node being completely inside the box...
					if ( flDelta + node->m_vecHalfDiagonal.y < info.m_vecBoxHalfDiagonal.y )
						nTestFlags &= ~ENUM_SPHERE_TEST_Y;
				}

				if (nTestFlags & ENUM_SPHERE_TEST_Z)
				{
					float flDelta = FloatMakePositive( node->m_vecCenter.z - info.m_vecBoxCenter.z );
					float flSize = node->m_vecHalfDiagonal.z + info.m_vecBoxHalfDiagonal.z;
					if ( flDelta > flSize )
						return true;
														   
					if ( flDelta + node->m_vecHalfDiagonal.z < info.m_vecBoxHalfDiagonal.z )
						nTestFlags &= ~ENUM_SPHERE_TEST_Z;
				}
			}
			else if (node->contents == -2)
			{
				// If the box is too small to bother with testing, then blat out the flags
				nTestFlags = 0;
			}
		}

		// Does the node plane split the sphere?
		// find which side of the node we are on
		float flNormalDotCenter;
		cplane_t* plane = node->plane;
		if ( plane->type <= PLANE_Z )
		{
			flNormalDotCenter = info.m_vecCenter[plane->type];
		}
		else
		{
			// Here, we've got a plane which is not axis aligned, so we gotta do more work
			flNormalDotCenter = DotProduct( plane->normal, info.m_vecCenter );
		}

		if (flNormalDotCenter + info.m_flRadius <= plane->dist)
		{
			node = node->children[1];
		}
		else if (flNormalDotCenter - info.m_flRadius >= plane->dist)
		{
			node = node->children[0];
		}
		else
		{
			// Here the box is split by the node
			if (!EnumerateLeafInSphere_R( node->children[0], info, nTestFlags ))
				return false;

			node = node->children[1];
		}
	}
}


//-----------------------------------------------------------------------------
// Enumerate leaves along a non-extruded ray
//-----------------------------------------------------------------------------

static bool EnumerateLeavesAlongRay_R( mnode_t *node, Ray_t const& ray, 
	float start, float end, ISpatialLeafEnumerator* pEnum, int context )
{
	// no polygons in solid nodes (don't report these leaves either)
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid, keep recursing

	// didn't hit anything
	if (node->contents >= 0)
	{
		// if a leaf node, report it to the iterator...
		return pEnum->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), context ); 
	}
	
	// Determine which side of the node plane our points are on
	cplane_t* plane = node->plane;

	float startDotN,deltaDotN;
	if (plane->type <= PLANE_Z)
	{
		startDotN = ray.m_Start[plane->type];
		deltaDotN = ray.m_Delta[plane->type];
	}
	else
	{
		startDotN = DotProduct( ray.m_Start, plane->normal );
		deltaDotN = DotProduct( ray.m_Delta, plane->normal );
	}

	float front = startDotN + start * deltaDotN - plane->dist;
	float back = startDotN + end * deltaDotN - plane->dist;

	int side = front < 0;
	
	// If they're both on the same side of the plane, don't bother to split
	// just check the appropriate child
	if ( (back < 0) == side )
	{
		return EnumerateLeavesAlongRay_R (node->children[side], ray, start, end, pEnum, context );
	}
	
	// calculate mid point
	float frac = front / (front - back);
	float mid = start * (1.0f - frac) + end * frac;
	
	// go down front side	
	bool ok = EnumerateLeavesAlongRay_R (node->children[side], ray, start, mid, pEnum, context );
	if (!ok)
		return ok;

	// go down back side
	return EnumerateLeavesAlongRay_R (node->children[!side], ray, mid, end, pEnum, context );
}


//-----------------------------------------------------------------------------
// Enumerate leaves along a non-extruded ray
//-----------------------------------------------------------------------------

static bool EnumerateLeavesAlongExtrudedRay_R( mnode_t *node, Ray_t const& ray, 
	float start, float end, ISpatialLeafEnumerator* pEnum, int context )
{
	// no polygons in solid nodes (don't report these leaves either)
	if (node->contents == CONTENTS_SOLID)
		return true;		// solid, keep recursing

	// didn't hit anything
	if (node->contents >= 0)
	{
		// if a leaf node, report it to the iterator...
		return pEnum->EnumerateLeaf( LeafToIndex( (mleaf_t *)node ), context ); 
	}
	
	// Determine which side of the node plane our points are on
	cplane_t* plane = node->plane;

	//
	float t1, t2, offset;
	float startDotN,deltaDotN;
	if (plane->type <= PLANE_Z)
	{
		startDotN = ray.m_Start[plane->type];
		deltaDotN = ray.m_Delta[plane->type];
		offset = ray.m_Extents[plane->type] + DIST_EPSILON;
	}
	else
	{
		startDotN = DotProduct( ray.m_Start, plane->normal );
		deltaDotN = DotProduct( ray.m_Delta, plane->normal );
		offset = fabs(ray.m_Extents[0]*plane->normal[0]) +
				fabs(ray.m_Extents[1]*plane->normal[1]) +
				fabs(ray.m_Extents[2]*plane->normal[2]) + DIST_EPSILON;
	}
	t1 = startDotN + start * deltaDotN - plane->dist;
	t2 = startDotN + end * deltaDotN - plane->dist;

	// If they're both on the same side of the plane (further than the trace
	// extents), don't bother to split, just check the appropriate child
    if (t1 > offset && t2 > offset )
//	if (t1 >= offset && t2 >= offset)
	{
		return EnumerateLeavesAlongExtrudedRay_R( node->children[0], ray,
			start, end, pEnum, context );
	}
	if (t1 < -offset && t2 < -offset)
	{
		return EnumerateLeavesAlongExtrudedRay_R( node->children[1], ray,
			start, end, pEnum, context );
	}

	// For the segment of the line that we are going to use
	// to test against the back side of the plane, we're going
	// to use the part that goes from start to plane + extent
	// (which causes it to extend somewhat into the front halfspace,
	// since plane + extent is in the front halfspace).
	// Similarly, front the segment which tests against the front side,
	// we use the entire front side part of the ray + a portion of the ray that
	// extends by -extents into the back side.

	if (fabs(t1-t2) < DIST_EPSILON)
	{
		// Parallel case, send entire ray to both children...
		bool ret = EnumerateLeavesAlongExtrudedRay_R( node->children[0], 
			ray, start, end, pEnum, context );
		if (!ret)
			return false;
		return EnumerateLeavesAlongExtrudedRay_R( node->children[1],
			ray, start, end, pEnum, context );
	}
	
	// Compute the two fractions...
	// We need one at plane + extent and another at plane - extent.
	// put the crosspoint DIST_EPSILON pixels on the near side
	float idist, frac2, frac;
	int side;
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset) * idist;
		frac = (t1 - offset) * idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset) * idist;
		frac = (t1 + offset) * idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = clamp( frac, 0.f, 1.f );
	float midf = start + (end - start)*frac;
	bool ret = EnumerateLeavesAlongExtrudedRay_R( node->children[side], ray, start, midf, pEnum, context );
	if (!ret)
		return ret;

	// go past the node
	frac2 = clamp( frac2, 0.f, 1.f );
	midf = start + (end - start)*frac2;
	return EnumerateLeavesAlongExtrudedRay_R( node->children[!side], ray, midf, end, pEnum, context );
}


//-----------------------------------------------------------------------------
//
// Helper class to iterate over leaves
//
//-----------------------------------------------------------------------------

class CEngineBSPTree : public IEngineSpatialQuery
{
public:
	// Returns the number of leaves
	int LeafCount() const;

	// Enumerates the leaves along a ray, box, etc.
	bool EnumerateLeavesAtPoint( const Vector& pt, ISpatialLeafEnumerator* pEnum, int context );
	bool EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, ISpatialLeafEnumerator* pEnum, int context );
	bool EnumerateLeavesInSphere( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, int context );
	bool EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, int context );
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------

static CEngineBSPTree s_ToolBSPTree;
IEngineSpatialQuery* g_pToolBSPTree = &s_ToolBSPTree;


//-----------------------------------------------------------------------------
// Returns the number of leaves
//-----------------------------------------------------------------------------

int CEngineBSPTree::LeafCount() const
{
	return host_state.worldbrush->numleafs;
}

//-----------------------------------------------------------------------------
// Enumerates the leaves at a point
//-----------------------------------------------------------------------------

bool CEngineBSPTree::EnumerateLeavesAtPoint( const Vector& pt, 
									ISpatialLeafEnumerator* pEnum, int context )
{
	int leaf = CM_PointLeafnum( pt );
	return pEnum->EnumerateLeaf( leaf, context );
}


static ConVar opt_EnumerateLeavesFastAlgorithm( "opt_EnumerateLeavesFastAlgorithm", "1", FCVAR_NONE, "Use the new SIMD version of CEngineBSPTree::EnumerateLeavesInBox." ); 


bool CEngineBSPTree::EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, 
									ISpatialLeafEnumerator* pEnum, int context )
{
	if ( !host_state.worldmodel )
		return false;

	EnumLeafBoxInfo_t info;
	VectorAdd( mins, maxs, info.m_vecBoxCenter );
	info.m_vecBoxCenter *= 0.5f;
	VectorSubtract( maxs, info.m_vecBoxCenter, info.m_vecBoxHalfDiagonal );
	info.m_pIterator = pEnum;
	info.m_nContext = context;
	info.m_vecBoxMax = maxs;
	info.m_vecBoxMin = mins;
#ifdef _X360
	if (opt_EnumerateLeavesFastAlgorithm.GetBool())
		return EnumerateLeafInBox_R( host_state.worldbrush->nodes, &info );
	else
		return EnumerateLeafInBox_R( host_state.worldbrush->nodes, info );
#else
	return EnumerateLeafInBox_R( host_state.worldbrush->nodes, info );
#endif
}


bool CEngineBSPTree::EnumerateLeavesInSphere( const Vector& center, float radius, 
									ISpatialLeafEnumerator* pEnum, int context )
{
	EnumLeafSphereInfo_t info;
	info.m_vecCenter = center;
	info.m_flRadius = radius;
	info.m_pIterator = pEnum;
	info.m_nContext = context;
	info.m_vecBoxCenter = center;
	info.m_vecBoxHalfDiagonal.Init( radius, radius, radius );

	return EnumerateLeafInSphere_R( host_state.worldbrush->nodes, info, ENUM_SPHERE_TEST_ALL );
}


bool CEngineBSPTree::EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, int context )
{
	if (!ray.m_IsSwept)
	{
		Vector mins, maxs;
		VectorAdd( ray.m_Start, ray.m_Extents, maxs );
		VectorSubtract( ray.m_Start, ray.m_Extents, mins );

		return EnumerateLeavesInBox( mins, maxs, pEnum, context );
	}

	Vector end;
	VectorAdd( ray.m_Start, ray.m_Delta, end );

	if ( ray.m_IsRay )
	{
		return EnumerateLeavesAlongRay_R( host_state.worldbrush->nodes, ray, 0.0f, 1.0f, pEnum, context );
	}
	else
	{
		return EnumerateLeavesAlongExtrudedRay_R( host_state.worldbrush->nodes, ray, 0.0f, 1.0f, pEnum, context );
	}
}

