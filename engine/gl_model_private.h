//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef GL_MODEL_PRIVATE_H
#define GL_MODEL_PRIVATE_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector4d.h"
#include "tier0/dbg.h"
#include "tier1/utlsymbol.h"
#include "idispinfo.h"
#include "shadowmgr.h"
#include "vcollide.h"
#include "studio.h"
#include "qlimits.h"
#include "host.h"
#include "gl_model.h"
#include "cmodel.h"
#include "bspfile.h"
#include "Overlay.h"
//#include "datamap.h"
#include "surfacehandle.h"
#include "mathlib/compressed_light_cube.h"
#include "datacache/imdlcache.h"
#include "bitmap/cubemap.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
struct studiomeshdata_t;
struct decal_t;
struct msurface1_t;
struct msurfacelighting_t;
struct msurfacenormal_t;
class ITexture;
class CEngineSprite;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
struct mvertex_t
{
	Vector		position;
};

// !!! if this is changed, it must be changed in asm_draw.h too !!!
struct medge_t
{
	unsigned short	v[2];
//	unsigned int	cachededgeoffset;
};

class IMaterial;
class IMaterialVar;

// This is here for b/w compatibility with world surfaces that use
// WorldVertexTransition. We can get rid of it when we rev the engine.
#define TEXINFO_USING_BASETEXTURE2	0x0001

struct mtexinfo_t
{
	Vector4D	textureVecsTexelsPerWorldUnits[2];	// [s/t] unit vectors in world space. 
							                        // [i][3] is the s/t offset relative to the origin.
	Vector4D	lightmapVecsLuxelsPerWorldUnits[2];
	float		luxelsPerWorldUnit;
	float		worldUnitsPerLuxel;
	unsigned short flags;		// SURF_ flags.
	unsigned short texinfoFlags;// TEXINFO_ flags.
	IMaterial	*material;

	mtexinfo_t( mtexinfo_t const& src )
	{
		// copy constructor needed since Vector4D has no copy constructor
		memcpy( this, &src, sizeof(mtexinfo_t) );
	}
};

struct mnode_t
{
	// common with leaf
	int			contents;		// <0 to differentiate from leafs
	// -1 means check the node for visibility
	// -2 means don't check the node for visibility

	int			visframe;		// node needs to be traversed if current

	mnode_t		*parent;
	short		area;			// If all leaves below this node are in the same area, then
	// this is the area index. If not, this is -1.
	short		flags;

	VectorAligned		m_vecCenter;
	VectorAligned		m_vecHalfDiagonal;

// node specific
	cplane_t	*plane;
	mnode_t		*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
};


struct mleaf_t
{
public:

	// common with node
	int			contents;		// contents mask
	int			visframe;		// node needs to be traversed if current

	mnode_t		*parent;

	short		area;
	short		flags;
	VectorAligned		m_vecCenter;
	VectorAligned		m_vecHalfDiagonal;


// leaf specific
	short		cluster;
	short		leafWaterDataID;

	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;

	short		nummarknodesurfaces;
	short		unused;

	unsigned short	dispListStart;			// index into displist of first displacement
	unsigned short	dispCount;				// number of displacements in the list for this leaf
};


struct mleafwaterdata_t
{
	float	surfaceZ;
	float	minZ;
	short	surfaceTexInfoID;
	short	firstLeafIndex;
};


struct mcubemapsample_t
{
	Vector origin;
	ITexture *pTexture;
	unsigned char size; // default (mat_envmaptgasize) if 0, 1<<(size-1) otherwise.
};


typedef struct mportal_s
{
	unsigned short	*vertList;
	int				numportalverts;
	cplane_t		*plane;
	unsigned short	cluster[2];
//	int				visframe;
} mportal_t;


typedef struct mcluster_s
{
	unsigned short	*portalList;
	int				numportals;
} mcluster_t;


struct mmodel_t
{
	Vector		mins, maxs;
	Vector		origin;		// for sounds or lights
	float		radius;
	int			headnode;
	int			firstface, numfaces;
};

struct mprimitive_t
{
	int	type;
	unsigned short	firstIndex;
	unsigned short	indexCount;
	unsigned short	firstVert;
	unsigned short	vertCount;
};

struct mprimvert_t
{
	Vector		pos;
	float		texCoord[2];
	float		lightCoord[2];
};

typedef dleafambientindex_t mleafambientindex_t;
typedef dleafambientlighting_t mleafambientlighting_t;

struct LightShadowZBufferSample_t
{
	float m_flTraceDistance;								// how far we traced. 0 = invalid
	float m_flHitDistance;									// where we hit
};

#define SHADOW_ZBUF_RES 8									// 6 * 64 * 2 * 4 = 3k bytes per light

typedef CCubeMap< LightShadowZBufferSample_t, SHADOW_ZBUF_RES> lightzbuffer_t;

#include "model_types.h"

#define MODELFLAG_MATERIALPROXY					0x0001	// we've got a material proxy
#define MODELFLAG_TRANSLUCENT					0x0002	// we've got a translucent model
#define MODELFLAG_VERTEXLIT						0x0004	// we've got a vertex-lit model
#define MODELFLAG_TRANSLUCENT_TWOPASS			0x0008	// render opaque part in opaque pass
#define MODELFLAG_FRAMEBUFFER_TEXTURE			0x0010	// we need the framebuffer as a texture
#define MODELFLAG_HAS_DLIGHT					0x0020	// need to check dlights
#define MODELFLAG_STUDIOHDR_USES_FB_TEXTURE		0x0100	// persisted from studiohdr
#define MODELFLAG_STUDIOHDR_USES_BUMPMAPPING	0x0200	// persisted from studiohdr
#define MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP	0x0400	// persisted from studiohdr
#define MODELFLAG_STUDIOHDR_AMBIENT_BOOST		0x0800	// persisted from studiohdr
#define MODELFLAG_STUDIOHDR_DO_NOT_CAST_SHADOWS	0x1000	// persisted from studiohdr

struct worldbrushdata_t
{
	int			numsubmodels;

	int			numplanes;
	cplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numleafwaterdata;
	mleafwaterdata_t *leafwaterdata;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numoccluders;
	doccluderdata_t *occluders;

	int			numoccluderpolys;
	doccluderpolydata_t *occluderpolys;

	int			numoccludervertindices;
	int			*occludervertindices;

	int				numvertnormalindices;	// These index vertnormals.
	unsigned short	*vertnormalindices;

	int			numvertnormals;
	Vector		*vertnormals;

	int			numnodes;
	mnode_t		*nodes;
	unsigned short *m_LeafMinDistToWater;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numtexdata;
	csurface_t	*texdata;

	int         numDispInfos;
	HDISPINFOARRAY	hDispInfos;	// Use DispInfo_Index to get IDispInfos..

	/*
	int         numOrigSurfaces;
	msurface_t  *pOrigSurfaces;
	*/

	int			numsurfaces;
	msurface1_t	*surfaces1;
	msurface2_t	*surfaces2;
	msurfacelighting_t *surfacelighting;
	msurfacenormal_t *surfacenormals;

	bool		unloadedlightmaps;

	int			numvertindices;
	unsigned short *vertindices;

	int nummarksurfaces;
	SurfaceHandle_t *marksurfaces;

	ColorRGBExp32		*lightdata;

	int			numworldlights;
	dworldlight_t *worldlights;

	lightzbuffer_t *shadowzbuffers;

	// non-polygon primitives (strips and lists)
	int			numprimitives;
	mprimitive_t *primitives;

	int			numprimverts;
	mprimvert_t *primverts;

	int			numprimindices;
	unsigned short *primindices;

	int				m_nAreas;
	darea_t			*m_pAreas;

	int				m_nAreaPortals;
	dareaportal_t	*m_pAreaPortals;

	int				m_nClipPortalVerts;
	Vector			*m_pClipPortalVerts;

	mcubemapsample_t  *m_pCubemapSamples;
	int				   m_nCubemapSamples;

	int				m_nDispInfoReferences;
	unsigned short	*m_pDispInfoReferences;

	mleafambientindex_t		*m_pLeafAmbient;
	mleafambientlighting_t	*m_pAmbientSamples;
#if 0
	int			numportals;
	mportal_t	*portals;

	int			numclusters;
	mcluster_t	*clusters;

	int			numportalverts;
	unsigned short *portalverts;

	int			numclusterportals;
	unsigned short *clusterportals;
#endif
};
// only models with type "mod_brush" have this data
struct brushdata_t
{
	worldbrushdata_t	*pShared;
	int			firstmodelsurface, nummodelsurfaces;

	unsigned short	renderHandle;
	unsigned short	firstnode;
};

// only models with type "mod_sprite" have this data
struct spritedata_t
{
	int				numframes;
	int				width;
	int				height;
	CEngineSprite	*sprite;
};

struct model_t
{
	FileNameHandle_t	fnHandle;
	CUtlString			strName;

	int					nLoadFlags;		// mark loaded/not loaded
	int					nServerCount;	// marked at load
	IMaterial			**ppMaterials;	// null-terminated runtime material cache; ((intptr_t*)(ppMaterials))[-1] == nMaterials

	modtype_t			type;
	int					flags;			// MODELFLAG_???

	// volume occupied by the model graphics	
	Vector				mins, maxs;
	float				radius;

	union
	{
		brushdata_t		brush;
		MDLHandle_t		studio;
		spritedata_t	sprite;
	};
};


//-----------------------------------------------------------------------------
// Decals
//-----------------------------------------------------------------------------

struct decallist_t
{
	DECLARE_SIMPLE_DATADESC();

	Vector		position;
	char		name[ 128 ];
	short		entityIndex;
	byte		depth;
	byte		flags;

	// This is the surface plane that we hit so that we can move certain decals across
	//  transitions if they hit similar geometry
	Vector		impactPlaneNormal;
};


inline class IDispInfo *MLeaf_Disaplcement( mleaf_t *pLeaf, int index, worldbrushdata_t *pData = host_state.worldbrush )
{
	Assert(index<pLeaf->dispCount);
	int dispIndex = pData->m_pDispInfoReferences[pLeaf->dispListStart+index];
	return DispInfo_IndexArray( pData->hDispInfos, dispIndex );
}

#define	MAXLIGHTMAPS	4

// drawing surface flags
#define SURFDRAW_NOLIGHT		0x00000001		// no lightmap
#define	SURFDRAW_NODE			0x00000002		// This surface is on a node
#define	SURFDRAW_SKY			0x00000004		// portal to sky
#define SURFDRAW_BUMPLIGHT		0x00000008		// Has multiple lightmaps for bump-mapping
#define SURFDRAW_NODRAW			0x00000010		// don't draw this surface, not really visible
#define SURFDRAW_TRANS			0x00000020		// sort this surface from back to front
#define SURFDRAW_PLANEBACK		0x00000040		// faces away from plane of the node that stores this face
#define SURFDRAW_DYNAMIC		0x00000080		// Don't use a static buffer for this face
#define SURFDRAW_TANGENTSPACE	0x00000100		// This surface needs a tangent space
#define SURFDRAW_NOCULL			0x00000200		// Don't bother backface culling these
#define SURFDRAW_HASLIGHTSYTLES 0x00000400		// has a lightstyle other than 0
#define SURFDRAW_HAS_DISP		0x00000800		// has a dispinfo
#define SURFDRAW_ALPHATEST		0x00001000		// Is alphstested
#define SURFDRAW_NOSHADOWS		0x00002000		// No shadows baby
#define SURFDRAW_NODECALS		0x00004000		// No decals baby
#define SURFDRAW_HAS_PRIMS		0x00008000		// has a list of prims
#define SURFDRAW_WATERSURFACE	0x00010000	// This is a water surface
#define SURFDRAW_UNDERWATER		0x00020000
#define SURFDRAW_ABOVEWATER		0x00040000
#define SURFDRAW_HASDLIGHT		0x00080000	// Has some kind of dynamic light that must be checked
#define SURFDRAW_DLIGHTPASS		0x00100000	// Must be drawn in the dlight pass
#define SURFDRAW_UNUSED2		0x00200000	// unused
#define SURFDRAW_VERTCOUNT_MASK	0xFF000000	// 8 bits of vertex count
#define SURFDRAW_SORTGROUP_MASK	0x00C00000	// 2 bits of sortgroup

#define SURFDRAW_VERTCOUNT_SHIFT	24
#define SURFDRAW_SORTGROUP_SHIFT	22

// NOTE: 16-bytes, preserve size/alignment - we index this alot
struct msurface1_t
{
	// garymct - are these needed? - used by decal projection code
	int		textureMins[2];		// smallest unnormalized s/t position on the surface.
	short	textureExtents[2];	// ?? s/t texture size, 1..512 for all non-sky surfaces

	struct
	{
		unsigned short numPrims;
		unsigned short firstPrimID;			// index into primitive list if numPrims > 0
	} prims;
};

#pragma pack(1)
struct msurfacenormal_t
{
	unsigned int firstvertnormal;
//	unsigned short	firstvertnormal;
	// FIXME: Should I just point to the leaf here since it has this data?????????????
//	short fogVolumeID;			// -1 if not in fog  
};
#pragma pack()

// This is a single cache line (32 bytes)
struct msurfacelighting_t
{
	// You read that minus sign right. See the comment below.
	ColorRGBExp32 *AvgLightColor( int nLightStyleIndex ) { return m_pSamples - (nLightStyleIndex + 1); }

	// Lightmap info
	short m_LightmapMins[2];
	short m_LightmapExtents[2];
	short m_OffsetIntoLightmapPage[2];

	int m_nLastComputedFrame;	// last frame the surface's lightmap was recomputed
	int m_fDLightBits;			// Indicates which dlights illuminates this surface.
	int m_nDLightFrame;			// Indicates the last frame in which dlights illuminated this surface

	unsigned char m_nStyles[MAXLIGHTMAPS];	// index into d_lightstylevalue[] for animated lights 
											// no one surface can be effected by more than 4 
											// animated lights.

	// NOTE: This is tricky. To get this to fit in a single cache line,
	// and to save the extra memory of not having to store average light colors for
	// lightstyles that are not used, I store between 0 and 4 average light colors +before+
	// the samples, depending on how many lightstyles there are. Naturally, accessing
	// an average color for an undefined lightstyle on the surface results in undefined results.
	// 0->4 avg light colors, *in reverse order from m_nStyles* + [numstyles*surfsize]
	ColorRGBExp32 *m_pSamples;
};

const int WORLD_DECAL_HANDLE_INVALID = 0xFFFF;
typedef unsigned short WorldDecalHandle_t;

#pragma pack(1)
// NOTE: 32-bytes.  Aligned/indexed often
struct msurface2_t
{
	unsigned int			flags;			// see SURFDRAW_ #defines (only 22-bits right now)
	// These are packed in to flags now
	//unsigned char			vertCount;		// number of verts for this surface
	//unsigned char			sortGroup;		// only uses 2 bits, subdivide?
	cplane_t				*plane;			// pointer to shared plane	
	int						firstvertindex;	// look up in model->vertindices[] (only uses 17-18 bits?)
	WorldDecalHandle_t		decals;
	ShadowDecalHandle_t		m_ShadowDecals; // unsigned short
	OverlayFragmentHandle_t m_nFirstOverlayFragment;	// First overlay fragment on the surface (short)
	short					materialSortID;
	unsigned short			vertBufferIndex;

	unsigned short			m_bDynamicShadowsEnabled : 1;	// Can this surface receive dynamic shadows?
	unsigned short			texinfo : 15;

	IDispInfo				*pDispInfo;         // displacement map information
	int						visframe;		// should be drawn when node is crossed
};
#pragma pack()

inline unsigned short MSurf_AreDynamicShadowsEnabled( SurfaceHandle_t surfID )
{
	return surfID->m_bDynamicShadowsEnabled;
}

inline int MSurf_Index( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = surfID - pData->surfaces2;
	Assert(surfaceIndex >= 0 && surfaceIndex < pData->numsurfaces);
	return surfaceIndex;
}

inline const SurfaceHandle_t SurfaceHandleFromIndex( int surfaceIndex, const worldbrushdata_t *pData )
{
	return &pData->surfaces2[surfaceIndex];
}

inline SurfaceHandle_t SurfaceHandleFromIndex( int surfaceIndex, worldbrushdata_t *pData = host_state.worldbrush )
{
	return &pData->surfaces2[surfaceIndex];
}

#if _DEBUG
#define ASSERT_SURF_VALID(surfID) MSurf_Index(surfID)
#else
#define ASSERT_SURF_VALID(surfID)
#endif

inline unsigned int& MSurf_Flags( SurfaceHandle_t surfID )
{
	return surfID->flags;
}

inline bool SurfaceHasDispInfo( SurfaceHandle_t surfID )
{
	return ( MSurf_Flags( surfID ) & SURFDRAW_HAS_DISP ) ? true : false;
}

inline bool SurfaceHasPrims( SurfaceHandle_t surfID )
{
	return ( MSurf_Flags( surfID ) & SURFDRAW_HAS_PRIMS ) ? true : false;
}

inline int& MSurf_VisFrame( SurfaceHandle_t surfID )
{
	return surfID->visframe;
}

inline int MSurf_SortGroup( SurfaceHandle_t surfID )
{
	return (surfID->flags & SURFDRAW_SORTGROUP_MASK) >> SURFDRAW_SORTGROUP_SHIFT;
}

inline void MSurf_SetSortGroup( SurfaceHandle_t surfID, int sortGroup )
{
	unsigned int flags = (sortGroup << SURFDRAW_SORTGROUP_SHIFT) & SURFDRAW_SORTGROUP_MASK;
	surfID->flags |= flags;
}

/*
inline int& MSurf_DLightFrame( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfID].dlightframe;
}
*/
inline int& MSurf_DLightBits( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].m_fDLightBits;
}

inline cplane_t& MSurf_Plane( SurfaceHandle_t surfID )
{
	return *surfID->plane;
}

inline int& MSurf_FirstVertIndex( SurfaceHandle_t surfID )
{
	return surfID->firstvertindex;
}

inline int MSurf_VertCount( SurfaceHandle_t surfID )
{
	return (surfID->flags >> SURFDRAW_VERTCOUNT_SHIFT) & 0xFF;
}

inline void MSurf_SetVertCount( SurfaceHandle_t surfID, int vertCount )
{
	int flags = (vertCount << SURFDRAW_VERTCOUNT_SHIFT) & SURFDRAW_VERTCOUNT_MASK;
	surfID->flags |= flags;
}

inline int *MSurf_TextureMins( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfaces1[surfaceIndex].textureMins;
}

inline short *MSurf_TextureExtents( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfaces1[surfaceIndex].textureExtents;
}

inline short *MSurf_LightmapMins( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].m_LightmapMins;
}

inline short *MSurf_LightmapExtents( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].m_LightmapExtents;
}

inline short MSurf_MaxLightmapSizeWithBorder( SurfaceHandle_t surfID )
{
//	ASSERT_SURF_VALID( surfID );
	return SurfaceHasDispInfo( surfID ) ? MAX_DISP_LIGHTMAP_DIM_INCLUDING_BORDER : MAX_BRUSH_LIGHTMAP_DIM_INCLUDING_BORDER;
}

inline short MSurf_MaxLightmapSizeWithoutBorder( SurfaceHandle_t surfID )
{
//	ASSERT_SURF_VALID( surfID );
	return SurfaceHasDispInfo( surfID ) ? MAX_DISP_LIGHTMAP_DIM_WITHOUT_BORDER : MAX_BRUSH_LIGHTMAP_DIM_WITHOUT_BORDER;
}

inline mtexinfo_t *MSurf_TexInfo( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	return &pData->texinfo[surfID->texinfo];
}

inline WorldDecalHandle_t& MSurf_Decals( SurfaceHandle_t surfID )
{
	return surfID->decals;
}

inline bool SurfaceHasDecals( SurfaceHandle_t surfID )
{
	return ( MSurf_Decals( surfID ) != WORLD_DECAL_HANDLE_INVALID ) ? true : false;
}

inline ShadowDecalHandle_t& MSurf_ShadowDecals( SurfaceHandle_t surfID )
{
	return surfID->m_ShadowDecals;
}


inline ColorRGBExp32 *MSurf_AvgLightColor( SurfaceHandle_t surfID, int nIndex, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].AvgLightColor(nIndex);
}

inline byte *MSurf_Styles( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].m_nStyles;
}

/*
inline int *MSurf_CachedLight( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfID].cached_light;
}

inline short& MSurf_CachedDLight( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfID].cached_dlight;
}
*/

inline unsigned short MSurf_NumPrims( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	if ( SurfaceHasDispInfo( surfID ) || !SurfaceHasPrims( surfID ))
		return 0;

	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfaces1[surfaceIndex].prims.numPrims;
}

inline unsigned short MSurf_FirstPrimID( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	if ( SurfaceHasDispInfo( surfID ) )
		return 0;
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfaces1[surfaceIndex].prims.firstPrimID;
}

inline ColorRGBExp32 *MSurf_Samples( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].m_pSamples;
}

inline IDispInfo *MSurf_DispInfo( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	return surfID->pDispInfo;
}

//inline unsigned short &MSurf_FirstVertNormal( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
inline unsigned int &MSurf_FirstVertNormal( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	Assert( pData->surfacenormals[surfaceIndex].firstvertnormal < MAX_MAP_VERTNORMALINDICES );
	return pData->surfacenormals[surfaceIndex].firstvertnormal;
}

inline unsigned short &MSurf_VertBufferIndex( SurfaceHandle_t surfID )
{
	return surfID->vertBufferIndex;
}

inline short& MSurf_MaterialSortID( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	return surfID->materialSortID;
}

inline short *MSurf_OffsetIntoLightmapPage( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return pData->surfacelighting[surfaceIndex].m_OffsetIntoLightmapPage;
}

inline VPlane MSurf_GetForwardFacingPlane( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
//	ASSERT_SURF_VALID( surfID );
	Assert( pData );
	return VPlane( MSurf_Plane( surfID).normal, MSurf_Plane( surfID ).dist );
}


inline OverlayFragmentHandle_t &MSurf_OverlayFragmentList( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	return surfID->m_nFirstOverlayFragment;
}

inline msurfacelighting_t *SurfaceLighting( SurfaceHandle_t surfID, worldbrushdata_t *pData = host_state.worldbrush )
{
	int surfaceIndex = MSurf_Index(surfID,pData);
	Assert( pData );
	return &pData->surfacelighting[surfaceIndex];
}

#endif // GL_MODEL_PRIVATE_H
