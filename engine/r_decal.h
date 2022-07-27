//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef R_DECAL_H
#define R_DECAL_H
#ifdef _WIN32
#pragma once
#endif

#include "decal_private.h"
#include "decal_clip.h"
#include "utllinkedlist.h"
#include "utlrbtree.h"

#include "tier0/memdbgon.h"

// Initialize and shutdown the decal stuff.
// R_DecalTerm unlinks all the active decals (which frees their counterparts in displacements).
void R_DecalInit();
void R_DecalTerm( worldbrushdata_t *pBrushData, bool term_permanent_decals );
void R_DecalTermAll();
float ComputeDecalLightmapOffset( SurfaceHandle_t surfID );
// get the max vertex/index to lock in a dynamic VB
void R_DecalsGetMaxMesh( IMatRenderContext *pRenderContext, int &nDecalSortMaxVerts, int &nDecalSortMaxIndices );

// --------------------------------------------------------------- //
// Decal functions used for displacements.
// --------------------------------------------------------------- //

// Figure out where the decal maps onto the surface.
void R_SetupDecalClip( 
	CDecalVert* &pOutVerts,
	decal_t *pDecal,
	Vector &vSurfNormal,
	IMaterial *pMaterial,
	Vector textureSpaceBasis[3],
	float decalWorldScale[2] );

//-----------------------------------------------------------------------------
// Gets the decal material and radius based on the decal index
//-----------------------------------------------------------------------------
void R_DecalGetMaterialAndSize( int decalIndex, IMaterial*& pDecalMaterial, float& w, float& h );

//=============================================================================
//
// Decal Sort Structures.
//
#define DECALSORT_RBTREE_SIZE	16

enum 
{
	PERMANENT_LIGHTMAP = 0,
	LIGHTMAP,
	NONLIGHTMAP,

	DECALSORT_TYPE_COUNT,
};

struct DecalSortVertexFormat_t
{
	VertexFormat_t		m_VertexFormat;
	int					m_iSortTree;			// Sort tree index.
};

struct DecalMaterialSortData_t
{
	IMaterial	*m_pMaterial;
	int			m_iLightmapPage; 
	int			m_iBucket;			// Index into the s_aDecalMaterialHead list.
};

struct DecalMaterialBucket_t
{
	intp			m_iHead;
	int			m_nCheckCount;
};

inline bool DecalSortTreeSortLessFunc( const DecalMaterialSortData_t &decal1, const DecalMaterialSortData_t &decal2 )
{
	if ( ( decal1.m_iLightmapPage == -1 ) || ( decal2.m_iLightmapPage == -1 ) )
	{
		return ( ( intp )decal1.m_pMaterial < ( intp )decal2.m_pMaterial );
	}

	if ( ( intp )decal1.m_pMaterial == ( intp )decal2.m_pMaterial )
	{
		return ( decal1.m_iLightmapPage < decal2.m_iLightmapPage );
	}
	else
	{
		return ( ( intp )decal1.m_pMaterial < ( intp )decal2.m_pMaterial );
	}
}

struct DecalSortTrees_t
{
	CUtlRBTree<DecalMaterialSortData_t, int>	*m_pTrees[DECALSORT_TYPE_COUNT];
	CUtlVector<DecalMaterialBucket_t>			m_aDecalSortBuckets[MAX_MAT_SORT_GROUPS+1][DECALSORT_TYPE_COUNT];

	DecalSortTrees_t()
	{
		for ( int iSort = 0; iSort < DECALSORT_TYPE_COUNT; ++iSort )
		{
			m_pTrees[iSort] = new CUtlRBTree<DecalMaterialSortData_t, int>( DECALSORT_RBTREE_SIZE, DECALSORT_RBTREE_SIZE, DecalSortTreeSortLessFunc );
		}
	}

	~DecalSortTrees_t()
	{
		for ( int iSort = 0; iSort < DECALSORT_TYPE_COUNT; ++iSort )
		{
			if ( m_pTrees[iSort] )
			{
				m_pTrees[iSort]->RemoveAll();
				delete m_pTrees[iSort];
				m_pTrees[iSort] = NULL;
			}
		}

		for ( int iGroup = 0; iGroup < ( MAX_MAT_SORT_GROUPS + 1 ); ++iGroup )
		{
			for ( int iSort = 0; iSort < DECALSORT_TYPE_COUNT; ++iSort )
			{
				m_aDecalSortBuckets[iGroup][iSort].Purge();
			}
		}
	}
};

extern CUtlVector<DecalSortVertexFormat_t>	g_aDecalFormats;

// Surface decals.
extern CUtlFixedLinkedList<decal_t*>		g_aDecalSortPool;
extern CUtlVector<DecalSortTrees_t>			g_aDecalSortTrees;
extern int									g_nDecalSortCheckCount;
extern int									g_nBrushModelDecalSortCheckCount;

// Displacement decals.
extern CUtlFixedLinkedList<decal_t*>		g_aDispDecalSortPool;
extern CUtlVector<DecalSortTrees_t>			g_aDispDecalSortTrees;
extern int									g_nDispDecalSortCheckCount;

struct DecalBatchList_t
{
	IMaterial		*m_pMaterial;
	void			*m_pProxy;
	int				m_iLightmapPage;
	unsigned short	m_iStartIndex;
	unsigned short	m_nIndexCount;
};

struct DecalMeshList_t
{
	IMesh									*m_pMesh;
	CUtlVectorFixed<DecalBatchList_t, 128>	m_aBatches;
};

void DecalSurfacesInit( bool bBrushModel );
void DecalSurfaceAdd( SurfaceHandle_t surfID, int renderGroup );
void DecalSurfaceDraw( IMatRenderContext *pRenderContext, int renderGroup, float flFade = 1.0f );
void DrawDecalsOnSingleSurface( IMatRenderContext *pRenderContext, SurfaceHandle_t surfID );

void R_DecalReSortMaterials( void );
void R_DecalFlushDestroyList( void );

extern VMatrix g_BrushToWorldMatrix;
#include "tier0/memdbgoff.h"

#endif // R_DECAL_H
