//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: BSP collision!
//
// $NoKeywords: $
//=============================================================================//

#include "cmodel_engine.h"
#include "cmodel_private.h"
#include "dispcoll_common.h"
#include "coordsize.h"

#include "quakedef.h"
#include <string.h>
#include <stdlib.h>
#include "mathlib/mathlib.h"
#include "common.h"
#include "sysexternal.h"
#include "zone.h"
#include "utlvector.h"
#include "const.h"
#include "gl_model_private.h"
#include "vphysics_interface.h"
#include "icliententity.h"
#include "engine/ICollideable.h"
#include "enginethreads.h"
#include "sys_dll.h"
#include "collisionutils.h"
#include "tier0/tslist.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCollisionBSPData g_BSPData;								// the global collision bsp
#define g_BSPData dont_use_g_BSPData_directly

#ifdef COUNT_COLLISIONS
CCollisionCounts  g_CollisionCounts;						// collision test counters
#endif

csurface_t CCollisionBSPData::nullsurface = { "**empty**", 0, 0 };				// generic null collision model surface

csurface_t *CCollisionBSPData::GetSurfaceAtIndex( unsigned short surfaceIndex )
{
	if ( surfaceIndex == SURFACE_INDEX_INVALID )
	{
		return &nullsurface;
	}
	return &map_surfaces[surfaceIndex];
}

#if TEST_TRACE_POOL
CTSPool<TraceInfo_t> g_TraceInfoPool;
#else
class CTraceInfoPool : public CTSList<TraceInfo_t *>
{
public:
	CTraceInfoPool()
	{
	}
};

CTraceInfoPool g_TraceInfoPool;
#endif

TraceInfo_t *BeginTrace()
{
#if TEST_TRACE_POOL
	TraceInfo_t *pTraceInfo = g_TraceInfoPool.GetObject();
#else
	TraceInfo_t *pTraceInfo;
	if ( !g_TraceInfoPool.PopItem( &pTraceInfo ) )
	{
		pTraceInfo = new TraceInfo_t;
	}
#endif
	if ( pTraceInfo->m_BrushCounters[0].Count() != GetCollisionBSPData()->numbrushes + 1 )
	{
		memset( pTraceInfo->m_Count, 0, sizeof( pTraceInfo->m_Count ) );
		pTraceInfo->m_nCheckDepth = -1;

		for ( int i = 0; i < MAX_CHECK_COUNT_DEPTH; i++ )
		{
			pTraceInfo->m_BrushCounters[i].SetCount( GetCollisionBSPData()->numbrushes + 1 );
			pTraceInfo->m_DispCounters[i].SetCount( g_DispCollTreeCount );

			memset( pTraceInfo->m_BrushCounters[i].Base(), 0, pTraceInfo->m_BrushCounters[i].Count() * sizeof(TraceCounter_t) );
			memset( pTraceInfo->m_DispCounters[i].Base(), 0, pTraceInfo->m_DispCounters[i].Count() * sizeof(TraceCounter_t) );
		}
	}

	PushTraceVisits( pTraceInfo );

	return pTraceInfo;
}

void PushTraceVisits( TraceInfo_t *pTraceInfo )
{
	++pTraceInfo->m_nCheckDepth;
	Assert( (pTraceInfo->m_nCheckDepth >= 0) && (pTraceInfo->m_nCheckDepth < MAX_CHECK_COUNT_DEPTH) );

	int i = pTraceInfo->m_nCheckDepth;
	pTraceInfo->m_Count[i]++;
	if ( pTraceInfo->m_Count[i] == 0 )
	{
		pTraceInfo->m_Count[i]++;
		memset( pTraceInfo->m_BrushCounters[i].Base(), 0, pTraceInfo->m_BrushCounters[i].Count() * sizeof(TraceCounter_t) );
		memset( pTraceInfo->m_DispCounters[i].Base(), 0, pTraceInfo->m_DispCounters[i].Count() * sizeof(TraceCounter_t) );
	}
}

void PopTraceVisits( TraceInfo_t *pTraceInfo )
{
	--pTraceInfo->m_nCheckDepth;
	Assert( pTraceInfo->m_nCheckDepth >= -1 );
}

void EndTrace( TraceInfo_t *&pTraceInfo )
{
	PopTraceVisits( pTraceInfo );
	Assert( pTraceInfo->m_nCheckDepth == -1 );
#if TEST_TRACE_POOL
	g_TraceInfoPool.PutObject( pTraceInfo );
#else
	g_TraceInfoPool.PushItem( pTraceInfo );
#endif
	pTraceInfo = NULL;
}

static ConVar map_noareas( "map_noareas", "0", 0, "Disable area to area connection testing." );

void	FloodAreaConnections (CCollisionBSPData *pBSPData);

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
vcollide_t *CM_GetVCollide( int modelIndex )
{
	cmodel_t *pModel = CM_InlineModelNumber( modelIndex );
	if( !pModel )
		return NULL;

	// return the model's collision data
	return &pModel->vcollisionData;
} 


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
cmodel_t *CM_InlineModel( const char *name )
{
	// error checking!
	if( !name )
		return NULL; 

	// JAYHL2: HACKHACK Get rid of this
	if( !strncmp( name, "maps/", 5 ) )
		return CM_InlineModelNumber( 0 );

	// check for valid name
	if( name[0] != '*' )
		Sys_Error( "CM_InlineModel: bad model name!" );

	// check for valid model
	int ndxModel = atoi( name + 1 );
	if( ( ndxModel < 1 ) || ( ndxModel >= GetCollisionBSPData()->numcmodels ) )
		Sys_Error( "CM_InlineModel: bad model number!" );

	return CM_InlineModelNumber( ndxModel );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
cmodel_t *CM_InlineModelNumber( int index )
{
	CCollisionBSPData *pBSPDataData = GetCollisionBSPData();

	if( ( index < 0 ) || ( index > pBSPDataData->numcmodels ) )
		return NULL;

	return ( &pBSPDataData->map_cmodels[ index ] );
}


int CM_BrushContents_r( CCollisionBSPData *pBSPData, int nodenum )
{
	int contents = 0;

	while (1)
	{
		if (nodenum < 0)
		{
			int leafIndex = -1 - nodenum;
			cleaf_t &leaf = pBSPData->map_leafs[leafIndex];
			
			for ( int i = 0; i < leaf.numleafbrushes; i++ )
			{
				unsigned short brushIndex = pBSPData->map_leafbrushes[ leaf.firstleafbrush + i ];
				contents |= pBSPData->map_brushes[brushIndex].contents;
			}

			return contents;
		}

		cnode_t &node = pBSPData->map_rootnode[nodenum];
		contents |= CM_BrushContents_r( pBSPData, node.children[0] );
		nodenum = node.children[1];
	}

	return contents;
}


int CM_InlineModelContents( int index )
{
	cmodel_t *pModel = CM_InlineModelNumber( index );
	if ( !pModel )
		return 0;

	return CM_BrushContents_r( GetCollisionBSPData(), pModel->headnode );
}
			

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_NumClusters( void )
{
	return GetCollisionBSPData()->numclusters;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
char *CM_EntityString( void )
{
	return GetCollisionBSPData()->map_entitystring.Get();
}

void CM_DiscardEntityString( void )
{
	GetCollisionBSPData()->map_entitystring.Discard();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_LeafContents( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].contents;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_LeafCluster( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].cluster;
}


int	CM_LeafFlags( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].flags;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CM_LeafArea( int leafnum )
{
	const CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( leafnum >= 0 );
	Assert( leafnum < pBSPData->numleafs );

	return pBSPData->map_leafs[leafnum].area;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CM_FreeMap(void)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// free the collision bsp data
	CollisionBSPData_Destroy( pBSPData );
}


// This turns on all the area portals that are "always on" in the map.
void CM_InitPortalOpenState( CCollisionBSPData *pBSPData )
{
	for ( int i=0; i < pBSPData->numportalopen; i++ )
	{
		pBSPData->portalopen[i] = false;
	}
}


/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
cmodel_t *CM_LoadMap( const char *name, bool allowReusePrevious, unsigned *checksum )
{
	static unsigned	int last_checksum = 0xFFFFFFFF;

	// get the current bsp -- there is currently only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Assert( physcollision );

	if( !strcmp( pBSPData->map_name, name ) && allowReusePrevious )
	{
		*checksum = last_checksum;
		return &pBSPData->map_cmodels[0];		// still have the right version
	}

	// only pre-load if the map doesn't already exist
	CollisionBSPData_PreLoad( pBSPData );

	if ( !name || !name[0] )
	{
		*checksum = 0;
		return &pBSPData->map_cmodels[0];			// cinematic servers won't have anything at all
	}

	// read in the collision model data
	CMapLoadHelper::Init( 0, name );
	CollisionBSPData_Load( name, pBSPData );
	CMapLoadHelper::Shutdown( );

    // Push the displacement bounding boxes down the tree and set leaf data.
    CM_DispTreeLeafnum( pBSPData );

	CM_InitPortalOpenState( pBSPData );
	FloodAreaConnections(pBSPData);

#ifdef COUNT_COLLISIONS
	// initialize counters
	CollisionCounts_Init( &g_CollisionCounts );
#endif

	return &pBSPData->map_cmodels[0];
}


//-----------------------------------------------------------------------------
//
// Methods associated with colliding against the world + models
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// returns a vcollide that can be used to collide against this model
//-----------------------------------------------------------------------------
vcollide_t* CM_VCollideForModel( int modelindex, const model_t* pModel )
{
	if ( pModel )
	{
		switch( pModel->type )
		{
		case mod_brush:
			return CM_GetVCollide( modelindex-1 );
		case mod_studio:
			Assert( modelloader->IsLoaded( pModel ) );
			return g_pMDLCache->GetVCollide( pModel->studio );
		}
	}

	return 0;
}




//=======================================================================

/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnumMinDistSqr_r( CCollisionBSPData *pBSPData, const Vector& p, int num, float &minDistSqr )
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		
		minDistSqr = fpmin( d*d, minDistSqr );
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_PointContents++;			// optimize counter
#endif

	return -1 - num;
}

int CM_PointLeafnum_r( CCollisionBSPData *pBSPData, const Vector& p, int num)
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_PointContents++;			// optimize counter
#endif

	return -1 - num;
}

int CM_PointLeafnum (const Vector& p)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (!pBSPData->numplanes)
		return 0;		// sound may call this without map loaded
	return CM_PointLeafnum_r (pBSPData, p, 0);
}

void CM_SnapPointToReferenceLeaf_r( CCollisionBSPData *pBSPData, const Vector& p, int num, float tolerance, Vector *pSnapPoint )
{
	float		d, snapDist;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;
		
		if (plane->type < 3)
		{
			d = p[plane->type] - plane->dist;
			snapDist = (*pSnapPoint)[plane->type] - plane->dist;
		}
		else
		{
			d = DotProduct (plane->normal, p) - plane->dist;
			snapDist = DotProduct (plane->normal, *pSnapPoint) - plane->dist;
		}

		if (d < 0)
		{
			num = node->children[1];
			if ( snapDist > 0 )
			{
				*pSnapPoint -= plane->normal * (snapDist + tolerance);
			}
		}
		else
		{
			num = node->children[0];
			if ( snapDist < 0 )
			{
				*pSnapPoint += plane->normal * (-snapDist + tolerance);
			}
		}
	}
}

void CM_SnapPointToReferenceLeaf(const Vector &referenceLeafPoint, float tolerance, Vector *pSnapPoint)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (pBSPData->numplanes)
	{
		CM_SnapPointToReferenceLeaf_r(pBSPData, referenceLeafPoint, 0, tolerance, pSnapPoint);
	}
}


/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
struct leafnums_t
{
	int		leafTopNode;
	int		leafMaxCount;
	int		*pLeafList;
	CCollisionBSPData *pBSPData;
};



int CM_BoxLeafnums( leafnums_t &context, const Vector &center, const Vector &extents, int nodenum )
{
	int leafCount = 0;
	const int NODELIST_MAX = 1024;
	int nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;
	cplane_t	*plane;
	cnode_t		*node;
	int prev_topnode = -1;

	while (1)
	{
		if (nodenum < 0)
		{
			// This handles the case when the box lies completely
			// within a single node. In that case, the top node should be
			// the parent of the leaf
			if (context.leafTopNode == -1)
				context.leafTopNode = prev_topnode;

			if (leafCount < context.leafMaxCount)
			{
				context.pLeafList[leafCount] = -1 - nodenum;
				leafCount++;
			}
			if ( nodeReadIndex == nodeWriteIndex )
				return leafCount;
			nodenum = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			node = &context.pBSPData->map_rootnode[nodenum];
			plane = node->plane;
			//		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);
			//		s = BOX_ON_PLANE_SIDE(*leaf_mins, *leaf_maxs, plane);
			float d0 = DotProduct( plane->normal, center ) - plane->dist;
			float d1 = DotProductAbs( plane->normal, extents );
			prev_topnode = nodenum;
			if (d0 >= d1)
				nodenum = node->children[0];
			else if (d0 < -d1)
				nodenum = node->children[1];
			else
			{	// go down both
				if (context.leafTopNode == -1)
					context.leafTopNode = nodenum;
				nodeList[nodeWriteIndex] = node->children[0];
				nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
				// check for overflow of the ring buffer
				Assert(nodeWriteIndex != nodeReadIndex);
				nodenum = node->children[1];
			}
		}
	}
}

int	CM_BoxLeafnums ( const Vector& mins, const Vector& maxs, int *list, int listsize, int *topnode)
{
	leafnums_t context;
	context.pLeafList = list;
	context.leafTopNode = -1;
	context.leafMaxCount = listsize;
	// get the current collision bsp -- there is only one!
	context.pBSPData = GetCollisionBSPData();
	Vector center = (mins+maxs)*0.5f;
	Vector extents = maxs - center;
	int leafCount = CM_BoxLeafnums(context, center, extents, context.pBSPData->map_cmodels[0].headnode );

	if (topnode)
		*topnode = context.leafTopNode;

	return leafCount;
}

// UNDONE: This is a version that returns only leaves with valid clusters
// UNDONE: Use this in the PVS calcs for networking
#if 0
int CM_BoxClusters( leafnums_t * RESTRICT pContext, const Vector &center, const Vector &extents, int nodenum )
{
	const int NODELIST_MAX = 1024;
	int nodeList[NODELIST_MAX];
	int nodeReadIndex = 0;
	int nodeWriteIndex = 0;
	cplane_t *RESTRICT plane;
	cnode_t	 *RESTRICT node;
	int prev_topnode = -1;
	int leafCount = 0;
	while (1)
	{
		if (nodenum < 0)
		{
			int leafIndex = -1 - nodenum;
			// This handles the case when the box lies completely
			// within a single node. In that case, the top node should be
			// the parent of the leaf
			if (pContext->leafTopNode == -1)
				pContext->leafTopNode = prev_topnode;

			if (leafCount < pContext->leafMaxCount)
			{
				cleaf_t *RESTRICT pLeaf = &pContext->pBSPData->map_leafs[leafIndex];
				if ( pLeaf->cluster >= 0 )
				{
					pContext->pLeafList[leafCount] = leafIndex;
					leafCount++;
				}
			}
			if ( nodeReadIndex == nodeWriteIndex )
				return leafCount;
			nodenum = nodeList[nodeReadIndex];
			nodeReadIndex = (nodeReadIndex+1) & (NODELIST_MAX-1);
		}
		else
		{
			node = &pContext->pBSPData->map_rootnode[nodenum];
			plane = node->plane;
			float d0 = DotProduct( plane->normal, center ) - plane->dist;
			float d1 = DotProductAbs( plane->normal, extents );
			prev_topnode = nodenum;
			if (d0 >= d1)
				nodenum = node->children[0];
			else if (d0 < -d1)
				nodenum = node->children[1];
			else
			{	// go down both
				if (pContext->leafTopNode == -1)
					pContext->leafTopNode = nodenum;
				nodenum = node->children[0];
				nodeList[nodeWriteIndex] = node->children[1];
				nodeWriteIndex = (nodeWriteIndex+1) & (NODELIST_MAX-1);
				// check for overflow of the ring buffer
				Assert(nodeWriteIndex != nodeReadIndex);
			}
		}
	}
}

int	CM_BoxClusters_headnode ( CCollisionBSPData *pBSPData, const Vector& mins, const Vector& maxs, int *list, int listsize, int nodenum, int *topnode)
{
	leafnums_t context;
	context.pLeafList = list;
	context.leafTopNode = -1;
	context.leafMaxCount = listsize;
	Vector center = 0.5f * (mins + maxs);
	Vector extents = maxs - center;
	context.pBSPData = pBSPData;

	int leafCount = CM_BoxClusters( &context, center, extents, nodenum );
	if (topnode)
		*topnode = context.leafTopNode;

	return leafCount;
}
#endif

static int FASTCALL CM_BrushBoxContents( CCollisionBSPData *pBSPData, const Vector &vMins, const Vector &vMaxs, cbrush_t *pBrush )
{
	if ( pBrush->IsBox())
	{
		cboxbrush_t *pBox = &pBSPData->map_boxbrushes[pBrush->GetBox()];
		if ( !IsBoxIntersectingBox( vMins, vMaxs, pBox->mins, pBox->maxs ) )
			return 0;
	}
	else
	{
		if (!pBrush->numsides)
			return 0;
		Vector vCenter = 0.5f *(vMins + vMaxs);
		Vector vExt = vMaxs - vCenter;
		int			i, j;

		cplane_t	*plane;
		float		dist;
		Vector		vOffset;
		float		d1;
		cbrushside_t	*side;

		for (i=0 ; i<pBrush->numsides ; i++)
		{
			side = &pBSPData->map_brushsides[pBrush->firstbrushside+i];
			plane = side->plane;

			// FIXME: special case for axial

			// general box case

			// push the plane out appropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					vOffset[j] = vExt[j];
				else
					vOffset[j] = -vExt[j];
			}
			dist = DotProduct (vOffset, plane->normal);
			dist = plane->dist - dist;

			d1 = DotProduct (vCenter, plane->normal) - dist;

			// if completely in front of face, no intersection
			if (d1 > 0)
				return 0;

		}
	}

	// inside this brush
	return pBrush->contents;
}

static int FASTCALL CM_BrushPointContents( CCollisionBSPData *pBSPData, const Vector &vPos, cbrush_t *pBrush )
{
	if ( pBrush->IsBox())
	{
		cboxbrush_t *pBox = &pBSPData->map_boxbrushes[pBrush->GetBox()];
		if ( !IsPointInBox( vPos, pBox->mins, pBox->maxs ) )
			return 0;
	}
	else
	{
		if (!pBrush->numsides)
			return 0;

		cplane_t	*plane;
		cbrushside_t	*side;

		for ( int i = 0 ; i < pBrush->numsides; i++ )
		{
			side = &pBSPData->map_brushsides[pBrush->firstbrushside+i];
			plane = side->plane;

			float flDist = DotProduct (vPos, plane->normal) - plane->dist;

			// if completely in front of face, no intersection
			if (flDist > 0)
				return 0;
		}
	}

	// inside this brush
	return pBrush->contents;
}
/*
==================
CM_PointContents

==================
*/

int CM_PointContents ( const Vector &p, int headnode)
{
	int		l;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (!pBSPData->numnodes)	// map not loaded
		return 0;

	l = CM_PointLeafnum_r (pBSPData, p, headnode);

	cleaf_t *pLeaf = &pBSPData->map_leafs[l];
	int nContents = pLeaf->contents;
	for ( int i = 0; i < pLeaf->numleafbrushes; i++ )
	{
		int nBrush = pBSPData->map_leafbrushes[pLeaf->firstleafbrush + i];
		cbrush_t * RESTRICT pBrush = &pBSPData->map_brushes[nBrush];
		nContents |= CM_BrushPointContents( pBSPData, p, pBrush );
	}
	
	return nContents;
}


/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents ( const Vector& p, int headnode, const Vector& origin, QAngle const& angles)
{
	Vector		p_l;
	Vector		temp;
	Vector		forward, right, up;
	int			l;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if ( angles[0] || angles[1] || angles[2] )
	{
		AngleVectors (angles, &forward, &right, &up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	l = CM_PointLeafnum_r (pBSPData, p_l, headnode);

	return pBSPData->map_leafs[l].contents;
}

/*
===============================================================================

BOX TRACING

===============================================================================
*/

// Custom SIMD implementation for box brushes

const fltx4 Four_DistEpsilons={DIST_EPSILON,DIST_EPSILON,DIST_EPSILON,DIST_EPSILON};
const int32 ALIGN16 g_CubeFaceIndex0[4] ALIGN16_POST = {0,1,2,-1};
const int32 ALIGN16 g_CubeFaceIndex1[4] ALIGN16_POST = {3,4,5,-1};
bool IntersectRayWithBoxBrush( TraceInfo_t *pTraceInfo, const cbrush_t *pBrush, cboxbrush_t *pBox )
{
	// Suppress floating-point exceptions in this function because invDelta's
	// components can get arbitrarily large -- up to FLT_MAX -- and overflow
	// when multiplied. Only applicable when FP_EXCEPTIONS_ENABLED is defined.
	FPExceptionDisabler hideExceptions;

	// Load the unaligned ray/box parameters into SIMD registers
	fltx4 start = LoadUnaligned3SIMD(pTraceInfo->m_start.Base());
	fltx4 extents = LoadUnaligned3SIMD(pTraceInfo->m_extents.Base());
	fltx4 delta = LoadUnaligned3SIMD(pTraceInfo->m_delta.Base());
	fltx4 boxMins = LoadAlignedSIMD( pBox->mins.Base() );
	fltx4 boxMaxs = LoadAlignedSIMD( pBox->maxs.Base() );

	// compute the mins/maxs of the box expanded by the ray extents
	// relocate the problem so that the ray start is at the origin.
	fltx4 offsetMins = SubSIMD(boxMins, start);
	fltx4 offsetMaxs = SubSIMD(boxMaxs, start);
	fltx4 offsetMinsExpanded = SubSIMD(offsetMins, extents);
	fltx4 offsetMaxsExpanded = AddSIMD(offsetMaxs, extents);

	// Check to see if both the origin (start point) and the end point (delta) are on the front side
	// of any of the box sides - if so there can be no intersection
	fltx4 startOutMins = CmpLtSIMD(Four_Zeros, offsetMinsExpanded);
	fltx4 endOutMins = CmpLtSIMD(delta,offsetMinsExpanded);
	fltx4 minsMask = AndSIMD( startOutMins, endOutMins );
	fltx4 startOutMaxs = CmpGtSIMD(Four_Zeros, offsetMaxsExpanded);
	fltx4 endOutMaxs = CmpGtSIMD(delta,offsetMaxsExpanded);
	fltx4 maxsMask = AndSIMD( startOutMaxs, endOutMaxs );
	if ( IsAnyNegative(SetWToZeroSIMD(OrSIMD(minsMask,maxsMask))))
		return false;

	fltx4 crossPlane = OrSIMD(XorSIMD(startOutMins,endOutMins), XorSIMD(startOutMaxs,endOutMaxs));
	// now build the per-axis interval of t for intersections
	fltx4 invDelta = LoadUnaligned3SIMD(pTraceInfo->m_invDelta.Base());
	fltx4 tmins = MulSIMD( offsetMinsExpanded, invDelta );
	fltx4 tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );
	// now sort the interval per axis
	fltx4 mint = MinSIMD( tmins, tmaxs );
	fltx4 maxt = MaxSIMD( tmins, tmaxs );
	// only axes where we cross a plane are relevant
	mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
	maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

	// now find the intersection of the intervals on all axes
	fltx4 firstOut = FindLowestSIMD3(maxt);
	fltx4 lastIn = FindHighestSIMD3(mint);
	// NOTE: This is really a scalar quantity now [t0,t1] == [lastIn,firstOut]
	firstOut = MinSIMD(firstOut, Four_Ones);
	lastIn = MaxSIMD(lastIn, Four_Zeros);

	// If the final interval is valid lastIn<firstOut, check for separation
	fltx4 separation = CmpGtSIMD(lastIn, firstOut);

	if ( IsAllZeros(separation) )
	{
		bool bStartOut = IsAnyNegative(SetWToZeroSIMD(OrSIMD(startOutMins,startOutMaxs)));
		offsetMinsExpanded = SubSIMD(offsetMinsExpanded, Four_DistEpsilons);
		offsetMaxsExpanded = AddSIMD(offsetMaxsExpanded, Four_DistEpsilons);

		tmins = MulSIMD( offsetMinsExpanded, invDelta );
		tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );

		fltx4 minface0 = LoadAlignedSIMD( (float *) g_CubeFaceIndex0 );
		fltx4 minface1 = LoadAlignedSIMD( (float *) g_CubeFaceIndex1 );
		fltx4 faceMask = CmpLeSIMD( tmins, tmaxs );
		mint = MinSIMD( tmins, tmaxs );
		maxt = MaxSIMD( tmins, tmaxs );
		fltx4 faceId = MaskedAssign( faceMask, minface0, minface1 );
		// only axes where we cross a plane are relevant
		mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
		maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

		fltx4 firstOutTmp = FindLowestSIMD3(maxt);

		// implement FindHighest of 3, but use intermediate masks to find the 
		// corresponding index in faceId to the highest at the same time
		fltx4 compareOne = RotateLeft( mint );
		faceMask = CmpGtSIMD(mint, compareOne);
		// compareOne is [y,z,G,x]
		fltx4 max_xy = MaxSIMD( mint, compareOne );
		fltx4 faceRot = RotateLeft(faceId);
		fltx4 faceId_xy = MaskedAssign(faceMask, faceId, faceRot);
		// max_xy is [max(x,y), ... ]
		compareOne = RotateLeft2( mint );
		faceRot = RotateLeft2(faceId);
		// compareOne is [z, G, x, y]
		faceMask = CmpGtSIMD( max_xy, compareOne );
		fltx4 max_xyz = MaxSIMD( max_xy, compareOne );
		faceId = MaskedAssign( faceMask, faceId_xy, faceRot );
		fltx4 lastInTmp = SplatXSIMD( max_xyz );

		firstOut = MinSIMD(firstOutTmp, Four_Ones);
		lastIn = MaxSIMD(lastInTmp, Four_Zeros);
		separation = CmpGtSIMD(lastIn, firstOut);
		Assert(IsAllZeros(separation));
		if ( IsAllZeros(separation) )
		{
			uint32 faceIndex = SubInt(faceId, 0);
			Assert(faceIndex<6);
			float t1 = SubFloat(lastIn,0);
			trace_t * RESTRICT pTrace = &pTraceInfo->m_trace;
			
			// this condition is copied from the brush case to avoid hitting an assert and
			// overwriting a previous start solid with a new shorter fraction
			if ( bStartOut && pTraceInfo->m_ispoint && pTrace->fractionleftsolid > t1 )
			{
				bStartOut = false;
			}

			if ( !bStartOut )
			{
				float t2 = SubFloat(firstOut,0);
				pTrace->startsolid = true;
				pTrace->contents = pBrush->contents;
				if ( t2 >= 1.0f )
				{
					pTrace->allsolid = true;
					pTrace->fraction = 0.0f;
				}
				else if ( t2 > pTrace->fractionleftsolid )
				{
					pTrace->fractionleftsolid = t2;
					if (pTrace->fraction <= t2)
					{
						pTrace->fraction = 1.0f;
						pTrace->surface = pTraceInfo->m_pBSPData->nullsurface;
					}
				}
			}
			else
			{
				static const int signbits[3]={1,2,4};
				if ( t1 < pTrace->fraction )
				{
					pTraceInfo->m_bDispHit = false;
					pTrace->fraction = t1;
					pTrace->plane.normal = vec3_origin;
					pTrace->surface = *pTraceInfo->m_pBSPData->GetSurfaceAtIndex( pBox->surfaceIndex[faceIndex] );
					if ( faceIndex >= 3 )
					{
						faceIndex -= 3;
						pTrace->plane.dist = pBox->maxs[faceIndex];
						pTrace->plane.normal[faceIndex] = 1.0f;
						pTrace->plane.signbits = 0;
					}
					else
					{
						pTrace->plane.dist = -pBox->mins[faceIndex];
						pTrace->plane.normal[faceIndex] = -1.0f;
						pTrace->plane.signbits = signbits[faceIndex];
					}
					pTrace->plane.type = faceIndex;
					pTrace->contents = pBrush->contents;
					return true;
				}
			}
		}
	}
	return false;
}

// slightly different version of the above.  This folds in more of the trace_t output because CM_ComputeTraceEndpts() isn't called after this
// so this routine needs to properly compute start/end points and fractions in all cases
bool IntersectRayWithBox( const Ray_t &ray, const VectorAligned &inInvDelta, const VectorAligned &inBoxMins, const VectorAligned &inBoxMaxs, trace_t *RESTRICT pTrace )
{
	// mark trace as not hitting
	pTrace->startsolid = false;
	pTrace->allsolid = false;
	pTrace->fraction = 1.0f;

	// Load the unaligned ray/box parameters into SIMD registers
	fltx4 start = LoadUnaligned3SIMD(ray.m_Start.Base());
	fltx4 extents = LoadUnaligned3SIMD(ray.m_Extents.Base());
	fltx4 delta = LoadUnaligned3SIMD(ray.m_Delta.Base());
	fltx4 boxMins = LoadAlignedSIMD( inBoxMins.Base() );
	fltx4 boxMaxs = LoadAlignedSIMD( inBoxMaxs.Base() );

	// compute the mins/maxs of the box expanded by the ray extents
	// relocate the problem so that the ray start is at the origin.
	fltx4 offsetMins = SubSIMD(boxMins, start);
	fltx4 offsetMaxs = SubSIMD(boxMaxs, start);
	fltx4 offsetMinsExpanded = SubSIMD(offsetMins, extents);
	fltx4 offsetMaxsExpanded = AddSIMD(offsetMaxs, extents);

	// Check to see if both the origin (start point) and the end point (delta) are on the front side
	// of any of the box sides - if so there can be no intersection
	fltx4 startOutMins = CmpLtSIMD(Four_Zeros, offsetMinsExpanded);
	fltx4 endOutMins = CmpLtSIMD(delta,offsetMinsExpanded);
	fltx4 minsMask = AndSIMD( startOutMins, endOutMins );
	fltx4 startOutMaxs = CmpGtSIMD(Four_Zeros, offsetMaxsExpanded);
	fltx4 endOutMaxs = CmpGtSIMD(delta,offsetMaxsExpanded);
	fltx4 maxsMask = AndSIMD( startOutMaxs, endOutMaxs );
	if ( IsAnyNegative(SetWToZeroSIMD(OrSIMD(minsMask,maxsMask))))
		return false;

	fltx4 crossPlane = OrSIMD(XorSIMD(startOutMins,endOutMins), XorSIMD(startOutMaxs,endOutMaxs));
	// now build the per-axis interval of t for intersections
	fltx4 invDelta = LoadAlignedSIMD(inInvDelta.Base());
	fltx4 tmins = MulSIMD( offsetMinsExpanded, invDelta );
	fltx4 tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );
	// now sort the interval per axis
	fltx4 mint = MinSIMD( tmins, tmaxs );
	fltx4 maxt = MaxSIMD( tmins, tmaxs );
	// only axes where we cross a plane are relevant
	mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
	maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

	// now find the intersection of the intervals on all axes
	fltx4 firstOut = FindLowestSIMD3(maxt);
	fltx4 lastIn = FindHighestSIMD3(mint);
	// NOTE: This is really a scalar quantity now [t0,t1] == [lastIn,firstOut]
	firstOut = MinSIMD(firstOut, Four_Ones);
	lastIn = MaxSIMD(lastIn, Four_Zeros);

	// If the final interval is valid lastIn<firstOut, check for separation
	fltx4 separation = CmpGtSIMD(lastIn, firstOut);

	if ( IsAllZeros(separation) )
	{
		bool bStartOut = IsAnyNegative(SetWToZeroSIMD(OrSIMD(startOutMins,startOutMaxs)));
		offsetMinsExpanded = SubSIMD(offsetMinsExpanded, Four_DistEpsilons);
		offsetMaxsExpanded = AddSIMD(offsetMaxsExpanded, Four_DistEpsilons);

		tmins = MulSIMD( offsetMinsExpanded, invDelta );
		tmaxs = MulSIMD( offsetMaxsExpanded, invDelta );

		fltx4 minface0 = LoadAlignedSIMD( (float *) g_CubeFaceIndex0 );
		fltx4 minface1 = LoadAlignedSIMD( (float *) g_CubeFaceIndex1 );
		fltx4 faceMask = CmpLeSIMD( tmins, tmaxs );
		mint = MinSIMD( tmins, tmaxs );
		maxt = MaxSIMD( tmins, tmaxs );
		fltx4 faceId = MaskedAssign( faceMask, minface0, minface1 );
		// only axes where we cross a plane are relevant
		mint = MaskedAssign( crossPlane, mint, Four_Negative_FLT_MAX );
		maxt = MaskedAssign( crossPlane, maxt, Four_FLT_MAX );

		fltx4 firstOutTmp = FindLowestSIMD3(maxt);

		//fltx4 lastInTmp = FindHighestSIMD3(mint);
		// implement FindHighest of 3, but use intermediate masks to find the 
		// corresponding index in faceId to the highest at the same time
		fltx4 compareOne = RotateLeft( mint );
		faceMask = CmpGtSIMD(mint, compareOne);
		// compareOne is [y,z,G,x]
		fltx4 max_xy = MaxSIMD( mint, compareOne );
		fltx4 faceRot = RotateLeft(faceId);
		fltx4 faceId_xy = MaskedAssign(faceMask, faceId, faceRot);
		// max_xy is [max(x,y), ... ]
		compareOne = RotateLeft2( mint );
		faceRot = RotateLeft2(faceId);
		// compareOne is [z, G, x, y]
		faceMask = CmpGtSIMD( max_xy, compareOne );
		fltx4 max_xyz = MaxSIMD( max_xy, compareOne );
		faceId = MaskedAssign( faceMask, faceId_xy, faceRot );
		fltx4 lastInTmp = SplatXSIMD( max_xyz );

		firstOut = MinSIMD(firstOutTmp, Four_Ones);
		lastIn = MaxSIMD(lastInTmp, Four_Zeros);
		separation = CmpGtSIMD(lastIn, firstOut);
		Assert(IsAllZeros(separation));
		if ( IsAllZeros(separation) )
		{
			uint32 faceIndex = SubInt(faceId, 0);
			Assert(faceIndex<6);
			float t1 = SubFloat(lastIn,0);

			// this condition is copied from the brush case to avoid hitting an assert and
			// overwriting a previous start solid with a new shorter fraction
			if ( bStartOut && ray.m_IsRay && pTrace->fractionleftsolid > t1 )
			{
				bStartOut = false;
			}

			if ( !bStartOut )
			{
				float t2 = SubFloat(firstOut,0);
				pTrace->startsolid = true;
				pTrace->contents = CONTENTS_SOLID;
				pTrace->fraction = 0.0f;
				pTrace->startpos = ray.m_Start + ray.m_StartOffset;
				pTrace->endpos = pTrace->startpos;
				if ( t2 >= 1.0f )
				{
					pTrace->allsolid = true;
				}
				else if ( t2 > pTrace->fractionleftsolid )
				{
					pTrace->fractionleftsolid = t2;
					pTrace->startpos += ray.m_Delta * pTrace->fractionleftsolid;
				}
				return true;
			}
			else
			{
				static const int signbits[3]={1,2,4};
				if ( t1 <= 1.0f )
				{
					pTrace->fraction = t1;
					pTrace->plane.normal = vec3_origin;
					if ( faceIndex >= 3 )
					{
						faceIndex -= 3;
						pTrace->plane.dist = inBoxMaxs[faceIndex];
						pTrace->plane.normal[faceIndex] = 1.0f;
						pTrace->plane.signbits = 0;
					}
					else
					{
						pTrace->plane.dist = -inBoxMins[faceIndex];
						pTrace->plane.normal[faceIndex] = -1.0f;
						pTrace->plane.signbits = signbits[faceIndex];
					}
					pTrace->plane.type = faceIndex;
					pTrace->contents = CONTENTS_SOLID;
					Vector startVec;
					VectorAdd( ray.m_Start, ray.m_StartOffset, startVec );

					if (pTrace->fraction == 1)
					{
						VectorAdd( startVec, ray.m_Delta, pTrace->endpos);
					}
					else
					{
						VectorMA( startVec, pTrace->fraction, ray.m_Delta, pTrace->endpos );
					}
					return true;
				}
			}
		}
	}
	return false;
}

/*
================
CM_ClipBoxToBrush
================
*/
template <bool IS_POINT>
void FASTCALL CM_ClipBoxToBrush( TraceInfo_t * RESTRICT pTraceInfo, const cbrush_t * RESTRICT brush )
{
	if ( brush->IsBox() )
	{
		cboxbrush_t *pBox = &pTraceInfo->m_pBSPData->map_boxbrushes[brush->GetBox()];
		IntersectRayWithBoxBrush( pTraceInfo, brush, pBox );
		return;
	}
	if (!brush->numsides)
		return;

	trace_t * RESTRICT trace = &pTraceInfo->m_trace;
	const Vector& p1 = pTraceInfo->m_start;
	const Vector& p2= pTraceInfo->m_end;
	int brushContents = brush->contents;

#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_BrushTraces++;
#endif

	float enterfrac = NEVER_UPDATED;
	float leavefrac = 1.f;

	bool getout = false;
	bool startout = false;
	cbrushside_t* leadside = NULL;

	float dist;

	cbrushside_t *  RESTRICT side = &pTraceInfo->m_pBSPData->map_brushsides[brush->firstbrushside];
	for ( const cbrushside_t * const sidelimit = side + brush->numsides; side < sidelimit; side++ )
	{
		cplane_t *plane = side->plane;
		const Vector &planeNormal = plane->normal;

		if (!IS_POINT)
		{	
			// general box case
			// push the plane out apropriately for mins/maxs

			dist = DotProductAbs( planeNormal, pTraceInfo->m_extents );
			dist = plane->dist + dist;
		}
		else
		{
			// special point case
			dist = plane->dist;
			// don't trace rays against bevel planes 
			if ( side->bBevel )
				continue;
		}

		float d1 = DotProduct (p1, planeNormal) - dist;
		float d2 = DotProduct (p2, planeNormal) - dist;

		// if completely in front of face, no intersection
		if( d1 > 0.f )
		{
			startout = true;

			// d1 > 0.f && d2 > 0.f
			if( d2 > 0.f )
				return;

		} 
		else
		{
			// d1 <= 0.f && d2 <= 0.f
			if( d2 <= 0.f )
				continue;
 
			// d2 > 0.f
			getout = true;
		}

		// crosses face
		if (d1 > d2)
		{	// enter
			// NOTE: This could be negative if d1 is less than the epsilon.
			// If the trace is short (d1-d2 is small) then it could produce a large
			// negative fraction. 
			float f = (d1-DIST_EPSILON);
			if ( f < 0.f )
				f = 0.f;
			f = f / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				leadside = side;
			}
		}
		else
		{	// leave
			float f = (d1+DIST_EPSILON) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	// when this happens, we entered the brush *after* leaving the previous brush.
	// Therefore, we're still outside!

	// NOTE: We only do this test against points because fractionleftsolid is
	// not possible to compute for brush sweeps without a *lot* more computation
	// So, client code will never get fractionleftsolid for box sweeps
	if (IS_POINT && startout)
	{ 
		// Add a little sludge.  The sludge should already be in the fractionleftsolid
		// (for all intents and purposes is a leavefrac value) and enterfrac values.  
		// Both of these values have +/- DIST_EPSILON values calculated in.  Thus, I 
		// think the test should be against "0.0."  If we experience new "left solid"
		// problems you may want to take a closer look here!
//		if ((trace->fractionleftsolid - enterfrac) > -1e-6)
		if ((trace->fractionleftsolid - enterfrac) > 0.0f )
			startout = false;
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		// return starting contents
		trace->contents = brushContents;

		if (!getout)
		{
			trace->allsolid = true;
			trace->fraction = 0.0f;
			trace->fractionleftsolid = 1.0f;
		}
		else
		{
			// if leavefrac == 1, this means it's never been updated or we're in allsolid
			// the allsolid case was handled above
			if ((leavefrac != 1) && (leavefrac > trace->fractionleftsolid))
			{
				trace->fractionleftsolid = leavefrac;

				// This could occur if a previous trace didn't start us in solid
				if (trace->fraction <= leavefrac)
				{
					trace->fraction = 1.0f;
					trace->surface = pTraceInfo->m_pBSPData->nullsurface;
				}
			}
		}
		return;
	}

	// We haven't hit anything at all until we've left...
	if (enterfrac < leavefrac)
	{
		if (enterfrac > NEVER_UPDATED && enterfrac < trace->fraction)
		{
			// WE HIT SOMETHING!!!!!
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			pTraceInfo->m_bDispHit = false;
			trace->plane = *(leadside->plane);
			trace->surface = *pTraceInfo->m_pBSPData->GetSurfaceAtIndex( leadside->surfaceIndex );
			trace->contents = brushContents;
		}
	}
}

inline bool IsTraceBoxIntersectingBoxBrush( TraceInfo_t *pTraceInfo, cboxbrush_t *pBox )
{
	fltx4 start = LoadUnaligned3SIMD(pTraceInfo->m_start.Base());
	fltx4 mins = LoadUnaligned3SIMD(pTraceInfo->m_mins.Base());
	fltx4 maxs = LoadUnaligned3SIMD(pTraceInfo->m_maxs.Base());

	fltx4 boxMins = LoadAlignedSIMD( pBox->mins.Base() );
	fltx4 boxMaxs = LoadAlignedSIMD( pBox->maxs.Base() );
	fltx4 offsetMins = AddSIMD(mins, start);
	fltx4 offsetMaxs = AddSIMD(maxs,start);
	fltx4 minsOut = MaxSIMD(boxMins, offsetMins);
	fltx4 maxsOut = MinSIMD(boxMaxs, offsetMaxs);
	fltx4 separated = CmpGtSIMD(minsOut, maxsOut);
	fltx4 sep3 = SetWToZeroSIMD(separated);
	return IsAllZeros(sep3);
}
/*
================
CM_TestBoxInBrush
================
*/
static void FASTCALL CM_TestBoxInBrush( TraceInfo_t *pTraceInfo, const cbrush_t *brush )
{
	if ( brush->IsBox())
	{
		cboxbrush_t *pBox = &pTraceInfo->m_pBSPData->map_boxbrushes[brush->GetBox()];
		if ( !IsTraceBoxIntersectingBoxBrush( pTraceInfo, pBox ) )
			return;
	}
	else
	{
		if (!brush->numsides)
			return;
		const Vector& mins = pTraceInfo->m_mins;
		const Vector& maxs = pTraceInfo->m_maxs;
		const Vector& p1 = pTraceInfo->m_start;
		int			i, j;

		cplane_t	*plane;
		float		dist;
		Vector		ofs(0,0,0);
		float		d1;
		cbrushside_t	*side;

		for (i=0 ; i<brush->numsides ; i++)
		{
			side = &pTraceInfo->m_pBSPData->map_brushsides[brush->firstbrushside+i];
			plane = side->plane;

			// FIXME: special case for axial

			// general box case

			// push the plane out appropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;

			d1 = DotProduct (p1, plane->normal) - dist;

			// if completely in front of face, no intersection
			if (d1 > 0)
				return;

		}
	}

	// inside this brush
	trace_t *trace = &pTraceInfo->m_trace;
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->fractionleftsolid = 1.0f;
	trace->contents = brush->contents;
}

#if defined(_X360)
#define PREFETCH_ELEMENT(ofs,base) __dcbt(ofs,(void*)base)
#else
#define PREFETCH_ELEMENT(a,b)
#endif
/*
================
CM_TraceToLeaf
================
*/

template <bool IS_POINT>
void FASTCALL CM_TraceToLeaf( TraceInfo_t * RESTRICT pTraceInfo, int ndxLeaf, float startFrac, float endFrac )
{
	VPROF("CM_TraceToLeaf");
	// get the leaf
	cleaf_t * RESTRICT pLeaf = &pTraceInfo->m_pBSPData->map_leafs[ndxLeaf];

	//
	// trace ray/box sweep against all brushes in this leaf
	//
	const int numleafbrushes = pLeaf->numleafbrushes;
	const int lastleafbrush = pLeaf->firstleafbrush + numleafbrushes;
	const CRangeValidatedArray<unsigned short> &map_leafbrushes = pTraceInfo->m_pBSPData->map_leafbrushes;
	CRangeValidatedArray<cbrush_t> & 			map_brushes = pTraceInfo->m_pBSPData->map_brushes;
	TraceCounter_t * RESTRICT pCounters = pTraceInfo->GetBrushCounters();
	TraceCounter_t count = pTraceInfo->GetCount();
	for( int ndxLeafBrush = pLeaf->firstleafbrush; ndxLeafBrush < lastleafbrush; ndxLeafBrush++ )
	{
		// get the current brush
		int ndxBrush = map_leafbrushes[ndxLeafBrush];

		cbrush_t * RESTRICT pBrush = &map_brushes[ndxBrush];

		// make sure we only check this brush once per trace/stab
		if ( !pTraceInfo->Visit( pBrush, ndxBrush, count, pCounters ) )
			continue;

		const int traceContents = pTraceInfo->m_contents;
		const int releventContents = ( pBrush->contents & traceContents );

		// only collide with objects you are interested in
		if( !releventContents )
			continue;

		// Many traces rely on CONTENTS_OPAQUE always being hit, even if it is nodraw.  AI blocklos brushes
		// need this, for instance.  CS and Terror visibility checks don't want this behavior, since
		// blocklight brushes also are CONTENTS_OPAQUE and SURF_NODRAW, and are actually in the playable
		// area in several maps.
		// NOTE: This is no longer true - no traces should rely on hitting CONTENTS_OPAQUE unless they
		// actually want to hit blocklight brushes.  No other brushes are marked with those bits
		// so it should be renamed CONTENTS_BLOCKLIGHT.  CONTENTS_BLOCKLOS has its own field now
		// so there is no reason to ignore nodraw opaques since you can merely remove CONTENTS_OPAQUE to
		// get that behavior
		if ( releventContents == CONTENTS_OPAQUE && (traceContents & CONTENTS_IGNORE_NODRAW_OPAQUE) )
		{
			// if the only reason we hit this brush is because it is opaque, make sure it isn't nodraw
			bool isNoDraw = false;

			if ( pBrush->IsBox())
			{
				cboxbrush_t *pBox = &pTraceInfo->m_pBSPData->map_boxbrushes[pBrush->GetBox()];
				for (int i=0 ; i<6 && !isNoDraw ;i++)
				{
					csurface_t *surface = pTraceInfo->m_pBSPData->GetSurfaceAtIndex( pBox->surfaceIndex[i] );
					if ( surface->flags & SURF_NODRAW )
					{
						isNoDraw = true;
						break;
					}
				}
			}
			else
			{
				cbrushside_t *side = &pTraceInfo->m_pBSPData->map_brushsides[pBrush->firstbrushside];
				for (int i=0 ; i<pBrush->numsides && !isNoDraw ;i++, side++)
				{
					csurface_t *surface = pTraceInfo->m_pBSPData->GetSurfaceAtIndex( side->surfaceIndex );
					if ( surface->flags & SURF_NODRAW )
					{
						isNoDraw = true;
						break;
					}
				}
			}

			if ( isNoDraw )
			{
				continue;
			}
		}

		// trace against the brush and find impact point -- if any?
		// NOTE: pTraceInfo->m_trace.fraction == 0.0f only when trace starts inside of a brush!
		CM_ClipBoxToBrush<IS_POINT>( pTraceInfo, pBrush );
		if( !pTraceInfo->m_trace.fraction )
			return;
	}

	// TODO: this may be redundant
	if( pTraceInfo->m_trace.startsolid )
		return;

	// Collide (test) against displacement surfaces in this leaf.
	if( pLeaf->dispCount )
	{
		VPROF("CM_TraceToLeafDisps");
		//
		// trace ray/swept box against all displacement surfaces in this leaf
		//
		pCounters = pTraceInfo->GetDispCounters();
		count = pTraceInfo->GetCount();

		if (IsX360())
		{
			// set up some relatively constant variables we'll use in the loop below
			fltx4 traceStart = LoadUnaligned3SIMD(pTraceInfo->m_start.Base());
			fltx4 traceDelta = LoadUnaligned3SIMD(pTraceInfo->m_delta.Base());
			fltx4 traceInvDelta = LoadUnaligned3SIMD(pTraceInfo->m_invDelta.Base());
			static const fltx4 vecEpsilon = {DISPCOLL_DIST_EPSILON,DISPCOLL_DIST_EPSILON,DISPCOLL_DIST_EPSILON,DISPCOLL_DIST_EPSILON};
			// only used in !IS_POINT version:
			fltx4 extents;
			if (!IS_POINT)
			{
				extents = LoadUnaligned3SIMD(pTraceInfo->m_extents.Base());
			}

			// TODO: this loop probably ought to be unrolled so that we can make a more efficient
			// job of intersecting rays against boxes. The simple SIMD version used here,
			// though about 6x faster than the fpu version, is slower still than intersecting
			// against four boxes simultaneously.
			for( int i = 0; i < pLeaf->dispCount; i++ )
			{
				int dispIndex = pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart + i];
				alignedbbox_t * RESTRICT pDispBounds = &g_pDispBounds[dispIndex];

				// only collide with objects you are interested in
				if( !( pDispBounds->GetContents() & pTraceInfo->m_contents ) )
					continue;

				if( pTraceInfo->m_isswept )
				{
					// make sure we only check this brush once per trace/stab
					if ( !pTraceInfo->Visit( pDispBounds->GetCounter(), count, pCounters ) )
						continue;
				}

				if ( IS_POINT )
				{
					if (!IsBoxIntersectingRay( LoadAlignedSIMD(pDispBounds->mins.Base()), LoadAlignedSIMD(pDispBounds->maxs.Base()), 
											   traceStart, traceDelta, traceInvDelta, vecEpsilon ))
						continue;
				}
				else
				{
					fltx4 mins = SubSIMD(LoadAlignedSIMD(pDispBounds->mins.Base()),extents);
					fltx4 maxs = AddSIMD(LoadAlignedSIMD(pDispBounds->maxs.Base()),extents);
					if (!IsBoxIntersectingRay( mins, maxs, 
											   traceStart, traceDelta, traceInvDelta, vecEpsilon ))
						continue;
				}

				CDispCollTree * RESTRICT pDispTree = &g_pDispCollTrees[dispIndex];
				CM_TraceToDispTree<IS_POINT>( pTraceInfo, pDispTree, startFrac, endFrac );
				if( !pTraceInfo->m_trace.fraction )
					break;
			}
		}
		else
		{
			// utterly nonoptimal FPU pathway
			for( int i = 0; i < pLeaf->dispCount; i++ )
			{
				int dispIndex = pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart + i];
				alignedbbox_t * RESTRICT pDispBounds = &g_pDispBounds[dispIndex];

				// only collide with objects you are interested in
				if( !( pDispBounds->GetContents() & pTraceInfo->m_contents ) )
					continue;

				if( pTraceInfo->m_isswept )
				{
					// make sure we only check this brush once per trace/stab
					if ( !pTraceInfo->Visit( pDispBounds->GetCounter(), count, pCounters ) )
						continue;
				}
		
				if ( IS_POINT && !IsBoxIntersectingRay( pDispBounds->mins, pDispBounds->maxs, pTraceInfo->m_start, pTraceInfo->m_delta, pTraceInfo->m_invDelta, DISPCOLL_DIST_EPSILON ) )
				{
					continue;
				}

				if ( !IS_POINT && !IsBoxIntersectingRay( pDispBounds->mins - pTraceInfo->m_extents, pDispBounds->maxs + pTraceInfo->m_extents, 
					pTraceInfo->m_start, pTraceInfo->m_delta, pTraceInfo->m_invDelta, DISPCOLL_DIST_EPSILON ) )
				{
					continue;
				}
				
				CDispCollTree * RESTRICT pDispTree = &g_pDispCollTrees[dispIndex];
				CM_TraceToDispTree<IS_POINT>( pTraceInfo, pDispTree, startFrac, endFrac );
				if( !pTraceInfo->m_trace.fraction )
					break;
			}
		}
		
		CM_PostTraceToDispTree( pTraceInfo );
	}
}


/*
================
CM_TestInLeaf
================
*/
static void FASTCALL CM_TestInLeaf( TraceInfo_t *pTraceInfo, int ndxLeaf )
{
	// get the leaf
	cleaf_t *pLeaf = &pTraceInfo->m_pBSPData->map_leafs[ndxLeaf];

	//
	// trace ray/box sweep against all brushes in this leaf
	//
	TraceCounter_t *pCounters = pTraceInfo->GetBrushCounters();
	TraceCounter_t count = pTraceInfo->GetCount();
	for( int ndxLeafBrush = 0; ndxLeafBrush < pLeaf->numleafbrushes; ndxLeafBrush++ )
	{
		// get the current brush
		int ndxBrush = pTraceInfo->m_pBSPData->map_leafbrushes[pLeaf->firstleafbrush+ndxLeafBrush];

		cbrush_t *pBrush = &pTraceInfo->m_pBSPData->map_brushes[ndxBrush];

		// make sure we only check this brush once per trace/stab
		if ( !pTraceInfo->Visit( pBrush, ndxBrush, count, pCounters ) )
			continue;

		// only collide with objects you are interested in
		if( !( pBrush->contents & pTraceInfo->m_contents ) )
			continue;

		//
		// test to see if the point/box is inside of any solid
		// NOTE: pTraceInfo->m_trace.fraction == 0.0f only when trace starts inside of a brush!
		//
		CM_TestBoxInBrush( pTraceInfo, pBrush );
		if( !pTraceInfo->m_trace.fraction )
			return;
	}

	// TODO: this may be redundant
	if( pTraceInfo->m_trace.startsolid )
		return;

	// if there are no displacement surfaces in this leaf -- we are done testing
	if( pLeaf->dispCount )
	{
		// test to see if the point/box is inside of any of the displacement surface
		CM_TestInDispTree( pTraceInfo, pLeaf, pTraceInfo->m_start, pTraceInfo->m_mins, pTraceInfo->m_maxs, pTraceInfo->m_contents, &pTraceInfo->m_trace );
	}
}

//-----------------------------------------------------------------------------
// Computes the ray endpoints given a trace.
//-----------------------------------------------------------------------------
static inline void CM_ComputeTraceEndpoints( const Ray_t& ray, trace_t& tr )
{
	// The ray start is the center of the extents; compute the actual start
	Vector start;
	VectorAdd( ray.m_Start, ray.m_StartOffset, start );

	if (tr.fraction == 1)
		VectorAdd(start, ray.m_Delta, tr.endpos);
	else
		VectorMA( start, tr.fraction, ray.m_Delta, tr.endpos );

	if (tr.fractionleftsolid == 0)
	{
		VectorCopy (start, tr.startpos);
	}
	else
	{
		if (tr.fractionleftsolid == 1.0f)
		{
			tr.startsolid = tr.allsolid = 1;
			tr.fraction = 0.0f;
			VectorCopy( start, tr.endpos );
		}

		VectorMA( start, tr.fractionleftsolid, ray.m_Delta, tr.startpos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get a list of leaves for a trace.
//-----------------------------------------------------------------------------
void CM_RayLeafnums_r( const Ray_t &ray, CCollisionBSPData *pBSPData, int iNode, 
					  float p1f, float p2f, const Vector &vecPoint1, const Vector &vecPoint2,
					  int *pLeafList, int nMaxLeafCount, int &nLeafCount )
{
	cnode_t		*pNode = NULL;
	cplane_t	*pPlane = NULL;
	float		flDist1 = 0.0f, flDist2 = 0.0f;
	float		flOffset = 0.0f;
	float		flDist;
	float		flFrac1, flFrac2;
	int			nSide;
	float		flMid;
	Vector		vecMid;

	// A quick check so we don't flood the message on overflow - or keep testing beyond our means!
	if ( nLeafCount >= nMaxLeafCount )
		return;

	// Find the point distances to the seperating plane and the offset for the size of the box.
	// NJS: Hoisted loop invariant comparison to pTraceInfo->m_ispoint
	if( ray.m_IsRay )
	{
		while( iNode >= 0 )
		{
			pNode = pBSPData->map_rootnode + iNode;
			pPlane = pNode->plane;

			if ( pPlane->type < 3 )
			{
				flDist1 = vecPoint1[pPlane->type] - pPlane->dist;
				flDist2 = vecPoint2[pPlane->type] - pPlane->dist;
				flOffset = ray.m_Extents[pPlane->type];
			}
			else
			{
				flDist1 = DotProduct( pPlane->normal, vecPoint1 ) - pPlane->dist;
				flDist2 = DotProduct( pPlane->normal, vecPoint2 ) - pPlane->dist;
				flOffset = 0.0f;
			}

			// See which sides we need to consider
			if ( flDist1 > flOffset && flDist2 > flOffset )
			{
				iNode = pNode->children[0];
				continue;
			}

			if ( flDist1 < -flOffset && flDist2 < -flOffset )
			{
				iNode = pNode->children[1];
				continue;
			}

			break;
		}
	} 
	else
	{
		while( iNode >= 0 )
		{
			pNode = pBSPData->map_rootnode + iNode;
			pPlane = pNode->plane;

			if ( pPlane->type < 3 )
			{
				flDist1 = vecPoint1[pPlane->type] - pPlane->dist;
				flDist2 = vecPoint2[pPlane->type] - pPlane->dist;
				flOffset = ray.m_Extents[pPlane->type];
			}
			else
			{
				flDist1 = DotProduct( pPlane->normal, vecPoint1 ) - pPlane->dist;
				flDist2 = DotProduct( pPlane->normal, vecPoint2 ) - pPlane->dist;
				flOffset = fabs( ray.m_Extents[0] * pPlane->normal[0] ) +
					fabs( ray.m_Extents[1] * pPlane->normal[1] ) +
					fabs( ray.m_Extents[2] * pPlane->normal[2] );
			}

			// See which sides we need to consider
			if ( flDist1 > flOffset && flDist2 > flOffset )
			{
				iNode = pNode->children[0];
				continue;
			}

			if ( flDist1 < -flOffset && flDist2 < -flOffset )
			{
				iNode = pNode->children[1];
				continue;
			}

			break;
		}
	}

	// If < 0, we are in a leaf node.
	if ( iNode < 0 )
	{
		if ( nLeafCount < nMaxLeafCount )
		{
			pLeafList[nLeafCount] = -1 - iNode;
			nLeafCount++;
		}
		else
		{
			DevMsg( 1, "CM_RayLeafnums_r: Max leaf count along ray exceeded!\n" );
		}

		return;
	}

	// Put the crosspoint DIST_EPSILON pixels on the near side.
	if ( flDist1 < flDist2 )
	{
		flDist = 1.0 / ( flDist1 - flDist2 );
		nSide = 1;
		flFrac2 = ( flDist1 + flOffset + DIST_EPSILON ) * flDist;
		flFrac1 = ( flDist1 - flOffset - DIST_EPSILON ) * flDist;
	}
	else if ( flDist1 > flDist2 )
	{
		flDist = 1.0 / ( flDist1-flDist2 );
		nSide = 0;
		flFrac2 = ( flDist1 - flOffset - DIST_EPSILON ) * flDist;
		flFrac1 = ( flDist1 + flOffset + DIST_EPSILON ) * flDist;
	}
	else
	{
		nSide = 0;
		flFrac1 = 1.0f;
		flFrac2 = 0.0f;
	}

	// Move up to the node
	flFrac1 = clamp( flFrac1, 0.0f, 1.0f );
	flMid = p1f + ( p2f - p1f ) * flFrac1;
	VectorLerp( vecPoint1, vecPoint2, flFrac1, vecMid );
	CM_RayLeafnums_r( ray, pBSPData, pNode->children[nSide], p1f, flMid, vecPoint1, vecMid, pLeafList, nMaxLeafCount, nLeafCount );

	// Go past the node
	flFrac2 = clamp( flFrac2, 0.0f, 1.0f );
	flMid = p1f + ( p2f - p1f ) * flFrac2;
	VectorLerp( vecPoint1, vecPoint2, flFrac2, vecMid );
	CM_RayLeafnums_r( ray, pBSPData, pNode->children[nSide^1], flMid, p2f, vecMid, vecPoint2, pLeafList, nMaxLeafCount, nLeafCount );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CM_RayLeafnums( const Ray_t &ray, int *pLeafList, int nMaxLeafCount, int &nLeafCount  )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();
	if ( !pBSPData->numnodes )
		return;

	Vector vecEnd;
	VectorAdd( ray.m_Start, ray.m_Delta, vecEnd );
	CM_RayLeafnums_r( ray, pBSPData, 0/*headnode*/, 0.0f, 1.0f, ray.m_Start, vecEnd, pLeafList, nMaxLeafCount, nLeafCount );
}



/*
==================
CM_RecursiveHullCheck

==================
Attempt to do whatever is nessecary to get this function to unroll at least once
*/
template <bool IS_POINT>
static void FASTCALL CM_RecursiveHullCheckImpl( TraceInfo_t *pTraceInfo, int num, const float p1f, const float p2f, const Vector& p1, const Vector& p2)
{
	if (pTraceInfo->m_trace.fraction <= p1f)
		return;		// already hit something nearer

	cnode_t		*node = NULL;
	cplane_t	*plane;
	float		t1 = 0, t2 = 0, offset = 0;
	float		frac, frac2;
	float		idist;
	Vector		mid;
	int			side;
	float		midf;


	// find the point distances to the seperating plane
	// and the offset for the size of the box

	while( num >= 0 )
	{
		node = pTraceInfo->m_pBSPData->map_rootnode + num;
		plane = node->plane;
		byte type = plane->type;
		float dist = plane->dist;

		if (type < 3)
		{
			t1 = p1[type] - dist;
			t2 = p2[type] - dist;
			offset = pTraceInfo->m_extents[type];
		}
		else
		{
			t1 = DotProduct (plane->normal, p1) - dist;
			t2 = DotProduct (plane->normal, p2) - dist;
			if( IS_POINT )
			{
				offset = 0;
			}
			else
			{
				offset = fabsf(pTraceInfo->m_extents[0]*plane->normal[0]) +
					fabsf(pTraceInfo->m_extents[1]*plane->normal[1]) +
					fabsf(pTraceInfo->m_extents[2]*plane->normal[2]);
			}
		}

		// see which sides we need to consider
		if (t1 > offset && t2 > offset )
//		if (t1 >= offset && t2 >= offset)
		{
			num = node->children[0];
			continue;
		}
		if (t1 < -offset && t2 < -offset)
		{
			num = node->children[1];
			continue;
		}
		break;
	}

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf<IS_POINT>(pTraceInfo, -1-num, p1f, p2f);
		return;
	}
	
	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset - DIST_EPSILON)*idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = clamp( frac, 0.f, 1.f );
	midf = p1f + (p2f - p1f)*frac;
	VectorLerp( p1, p2, frac, mid );

	CM_RecursiveHullCheckImpl<IS_POINT>(pTraceInfo, node->children[side], p1f, midf, p1, mid);

	// go past the node
	frac2 = clamp( frac2, 0.f, 1.f );
	midf = p1f + (p2f - p1f)*frac2;
	VectorLerp( p1, p2, frac2, mid );

	CM_RecursiveHullCheckImpl<IS_POINT>(pTraceInfo, node->children[side^1], midf, p2f, mid, p2);
}

void FASTCALL CM_RecursiveHullCheck ( TraceInfo_t *pTraceInfo, int num, const float p1f, const float p2f )
{
	const Vector& p1 = pTraceInfo->m_start;
	const Vector& p2 =  pTraceInfo->m_end;

	if( pTraceInfo->m_ispoint )
	{
		CM_RecursiveHullCheckImpl<true>( pTraceInfo, num, p1f, p2f, p1, p2);
	}
	else
	{
		CM_RecursiveHullCheckImpl<false>( pTraceInfo, num, p1f, p2f, p1, p2);
	}
}

void CM_ClearTrace( trace_t *trace )
{
	memset( trace, 0, sizeof(*trace));
	trace->fraction = 1.f;
	trace->fractionleftsolid = 0;
	trace->surface = CCollisionBSPData::nullsurface;
}


//-----------------------------------------------------------------------------
//
// The following versions use ray... gradually I'm gonna remove other versions
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Test an unswept box
//-----------------------------------------------------------------------------

static inline void CM_UnsweptBoxTrace( TraceInfo_t *pTraceInfo, const Ray_t& ray, int headnode, int brushmask )
{
	int		leafs[1024];
	int		i, numleafs;

	leafnums_t context;
	context.pLeafList = leafs;
	context.leafTopNode = -1;
	context.leafMaxCount = ARRAYSIZE(leafs);
	context.pBSPData = pTraceInfo->m_pBSPData;

	bool bFoundNonSolidLeaf = false;
	numleafs = CM_BoxLeafnums ( context, ray.m_Start, ray.m_Extents+Vector(1,1,1), headnode);
	for (i=0 ; i<numleafs ; i++)
	{
		if ((pTraceInfo->m_pBSPData->map_leafs[leafs[i]].contents & CONTENTS_SOLID) == 0)
		{
			bFoundNonSolidLeaf = true;
		}

		CM_TestInLeaf ( pTraceInfo, leafs[i] );
		if (pTraceInfo->m_trace.allsolid)
			break;
	}

	if (!bFoundNonSolidLeaf)
	{
		pTraceInfo->m_trace.allsolid = pTraceInfo->m_trace.startsolid = 1;
		pTraceInfo->m_trace.fraction = 0.0f;
		pTraceInfo->m_trace.fractionleftsolid = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ray/Hull trace against the world without the RecursiveHullTrace
//-----------------------------------------------------------------------------
void CM_BoxTraceAgainstLeafList( const Ray_t &ray, int *pLeafList, int nLeafCount, int nBrushMask,
							     bool bComputeEndpoint, trace_t &trace )
{
	// For multi-check avoidance.
	TraceInfo_t *pTraceInfo = BeginTrace();		

	// Setup trace data.
	CM_ClearTrace( &pTraceInfo->m_trace );

	// Get the collision bsp tree.
	pTraceInfo->m_pBSPData = GetCollisionBSPData();

	// Check if the map is loaded.
	if ( !pTraceInfo->m_pBSPData->numnodes )	
	{
		trace = pTraceInfo->m_trace;
		EndTrace( pTraceInfo );
		return;
	}

	// Setup global trace data. (This is nasty! I hate this.)
	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir.Init();
	pTraceInfo->m_contents = nBrushMask;
	VectorCopy( ray.m_Start, pTraceInfo->m_start );
	VectorAdd( ray.m_Start, ray.m_Delta, pTraceInfo->m_end );
	VectorMultiply( ray.m_Extents, -1.0f, pTraceInfo->m_mins );
	VectorCopy( ray.m_Extents, pTraceInfo->m_maxs );
	VectorCopy( ray.m_Extents, pTraceInfo->m_extents );
	pTraceInfo->m_delta = ray.m_Delta;
	pTraceInfo->m_invDelta = ray.InvDelta();
	pTraceInfo->m_ispoint = ray.m_IsRay;
	pTraceInfo->m_isswept = ray.m_IsSwept;

	if ( !ray.m_IsSwept )
	{
		Vector vecBoxMin( ( ray.m_Start.x - ray.m_Extents.x - 1 ), ( ray.m_Start.y - ray.m_Extents.y - 1 ), ( ray.m_Start.z - ray.m_Extents.z - 1 ) );
		Vector vecBoxMax( ( ray.m_Start.x + ray.m_Extents.x + 1 ), ( ray.m_Start.y + ray.m_Extents.y + 1 ), ( ray.m_Start.z + ray.m_Extents.z + 1 ) );

		bool bFoundNonSolidLeaf = false;
		for ( int iLeaf = 0; iLeaf < nLeafCount; ++iLeaf )
		{
			if ( ( pTraceInfo->m_pBSPData->map_leafs[pLeafList[iLeaf]].contents & CONTENTS_SOLID ) == 0 )
			{
				bFoundNonSolidLeaf = true;
			}
			
			CM_TestInLeaf( pTraceInfo, pLeafList[iLeaf] );
			if ( pTraceInfo->m_trace.allsolid )
				break;
		}

		if ( !bFoundNonSolidLeaf )
		{
			pTraceInfo->m_trace.allsolid = pTraceInfo->m_trace.startsolid = 1;
			pTraceInfo->m_trace.fraction = 0.0f;
			pTraceInfo->m_trace.fractionleftsolid = 1.0f;
		}
	}
	else
	{
		for ( int iLeaf = 0; iLeaf < nLeafCount; ++iLeaf )
		{
			// NOTE: startFrac and endFrac are not really used.
			if ( pTraceInfo->m_ispoint )
				CM_TraceToLeaf<true>( pTraceInfo, pLeafList[iLeaf], 1.0f, 1.0f );
			else
				CM_TraceToLeaf<false>( pTraceInfo, pLeafList[iLeaf], 1.0f, 1.0f );
		}
	}

	// Compute the trace start and end points.
	if ( bComputeEndpoint )
	{
		CM_ComputeTraceEndpoints( ray, pTraceInfo->m_trace );
	}

	// Copy off the results
	trace = pTraceInfo->m_trace;
	EndTrace( pTraceInfo );

	Assert( !ray.m_IsRay || trace.allsolid || ( trace.fraction >= trace.fractionleftsolid ) );
}

void CM_BoxTrace( const Ray_t& ray, int headnode, int brushmask, bool computeEndpt, trace_t& tr )
{
	VPROF("BoxTrace");
	// for multi-check avoidance
	TraceInfo_t *pTraceInfo = BeginTrace();		

#ifdef COUNT_COLLISIONS
	// for statistics, may be zeroed
	g_CollisionCounts.m_Traces++;		
#endif

	// fill in a default trace
	CM_ClearTrace( &pTraceInfo->m_trace );

	pTraceInfo->m_pBSPData = GetCollisionBSPData();

	// check if the map is not loaded
	if (!pTraceInfo->m_pBSPData->numnodes)	
	{
		tr = pTraceInfo->m_trace;
		EndTrace( pTraceInfo );
		return;
	}

	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir.Init();
	pTraceInfo->m_contents = brushmask;
	VectorCopy (ray.m_Start, pTraceInfo->m_start);
	VectorAdd  (ray.m_Start, ray.m_Delta, pTraceInfo->m_end);
	VectorMultiply (ray.m_Extents, -1.0f, pTraceInfo->m_mins);
	VectorCopy (ray.m_Extents, pTraceInfo->m_maxs);
	VectorCopy (ray.m_Extents, pTraceInfo->m_extents);
	pTraceInfo->m_delta = ray.m_Delta;
	pTraceInfo->m_invDelta = ray.InvDelta();
	pTraceInfo->m_ispoint = ray.m_IsRay;
	pTraceInfo->m_isswept = ray.m_IsSwept;

	if (!ray.m_IsSwept)
	{
		// check for position test special case
		CM_UnsweptBoxTrace( pTraceInfo, ray, headnode, brushmask );
	}
	else
	{
		// general sweeping through world
		CM_RecursiveHullCheck( pTraceInfo, headnode, 0, 1 );
	}
	// Compute the trace start + end points
	if (computeEndpt)
	{
		CM_ComputeTraceEndpoints( ray, pTraceInfo->m_trace );
	}

	// Copy off the results
	tr = pTraceInfo->m_trace;
	EndTrace( pTraceInfo );
	Assert( !ray.m_IsRay || tr.allsolid || (tr.fraction >= tr.fractionleftsolid) );
}


void CM_TransformedBoxTrace( const Ray_t& ray, int headnode, int brushmask,
							const Vector& origin, QAngle const& angles, trace_t& tr )
{
	matrix3x4_t	localToWorld;
	Ray_t ray_l;

	// subtract origin offset
	VectorCopy( ray.m_StartOffset, ray_l.m_StartOffset );
	VectorCopy( ray.m_Extents, ray_l.m_Extents );

	// Are we rotated?
	bool rotated = (angles[0] || angles[1] || angles[2]);

	// rotate start and end into the models frame of reference
	if (rotated)
	{
		// NOTE: In this case, the bbox is rotated into the space of the BSP as well
		// to insure consistency at all orientations, we must rotate the origin of the ray
		// and reapply the offset to the center of the box.  That way all traces with the 
		// same box centering will have the same transformation into local space
		Vector worldOrigin = ray.m_Start + ray.m_StartOffset;
		AngleMatrix( angles, origin, localToWorld );
		VectorIRotate( ray.m_Delta, localToWorld, ray_l.m_Delta );
		VectorITransform( worldOrigin, localToWorld, ray_l.m_Start );
		ray_l.m_Start -= ray.m_StartOffset;
	}
	else
	{
		VectorSubtract( ray.m_Start, origin, ray_l.m_Start );
		VectorCopy( ray.m_Delta, ray_l.m_Delta );
	}

	ray_l.m_IsRay = ray.m_IsRay;
	ray_l.m_IsSwept = ray.m_IsSwept;

	// sweep the box through the model, don't compute endpoints
	CM_BoxTrace( ray_l, headnode, brushmask, false, tr );

	// If we hit, gotta fix up the normal...
	if (( tr.fraction != 1 ) && rotated )
	{
		// transform the normal from the local space of this entity to world space
		Vector temp;
		VectorCopy (tr.plane.normal, temp);
		VectorRotate( temp, localToWorld, tr.plane.normal );
	}

	CM_ComputeTraceEndpoints( ray, tr );
}



/*
===============================================================================

PVS / PAS

===============================================================================
*/

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pBSPData - 
//			*out - 
//-----------------------------------------------------------------------------
void CM_NullVis( CCollisionBSPData *pBSPData, byte *out )
{
	int numClusterBytes = (pBSPData->numclusters+7)>>3;	
	byte *out_p = out;

	while (numClusterBytes)
	{
		*out_p++ = 0xff;
		numClusterBytes--;
	}
}

/*
===================
CM_DecompressVis
===================
*/
void CM_DecompressVis( CCollisionBSPData *pBSPData, int cluster, int visType, byte *out )
{
	int		c;
	byte	*out_p;
	int		numClusterBytes;

	if ( !pBSPData )
	{
		Assert( false ); // Shouldn't ever happen.
	}

	if ( cluster > pBSPData->numclusters || cluster < 0 )
	{
		// This can happen if this is called while the level is loading. See Map_VisCurrentCluster.
		CM_NullVis( pBSPData, out );
		return;
	}

	// no vis info, so make all visible
	if ( !pBSPData->numvisibility || !pBSPData->map_vis )
	{	
		CM_NullVis( pBSPData, out );
		return;		
	}

	byte *in = ((byte *)pBSPData->map_vis) + pBSPData->map_vis->bitofs[cluster][visType];
	numClusterBytes = (pBSPData->numclusters+7)>>3;	
	out_p = out;

	// no vis info, so make all visible
	if ( !in )
	{	
		CM_NullVis( pBSPData, out );
		return;		
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if ((out_p - out) + c > numClusterBytes)
		{
			c = numClusterBytes - (out_p - out);
			ConMsg( "warning: Vis decompression overrun\n" );
		}
		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < numClusterBytes);
}

//-----------------------------------------------------------------------------
// Purpose: Decompress the RLE bitstring for PVS or PAS of one cluster
// Input  : *dest - buffer to store the decompressed data
//			cluster - index of cluster of interest
//			visType - DVIS_PAS or DVIS_PAS
// Output : byte * - pointer to the filled buffer
//-----------------------------------------------------------------------------
const byte *CM_Vis( byte *dest, int destlen, int cluster, int visType )
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if ( !dest || visType > 2 || visType < 0 )
	{
		Sys_Error( "CM_Vis: error");
		return NULL;
	}

	if ( cluster == -1 )
	{
		int len = (pBSPData->numclusters+7)>>3;
		if ( len > destlen )
		{
			Sys_Error( "CM_Vis:  buffer not big enough (%i but need %i)\n",
				destlen, len );
		}
		memset( dest, 0, (pBSPData->numclusters+7)>>3 );
	}
	else
	{
		CM_DecompressVis( pBSPData, cluster, visType, dest );
	}

	return dest;
}

static byte	pvsrow[MAX_MAP_LEAFS/8];

int CM_ClusterPVSSize()
{
	return sizeof( pvsrow );
}

const byte	*CM_ClusterPVS (int cluster)
{
	return CM_Vis( pvsrow, CM_ClusterPVSSize(), cluster, DVIS_PVS );
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

void FloodArea_r (CCollisionBSPData *pBSPData, carea_t *area, int floodnum)
{
	int		i;
	dareaportal_t	*p;

	if (area->floodvalid == pBSPData->floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Sys_Error( "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = pBSPData->floodvalid;
	p = &pBSPData->map_areaportals[area->firstareaportal];
	for (i=0 ; i<area->numareaportals ; i++, p++)
	{
		if (pBSPData->portalopen[p->m_PortalKey])
		{
			FloodArea_r (pBSPData, &pBSPData->map_areas[p->otherarea], floodnum);
		}
	}
}

/*
====================
FloodAreaConnections


====================
*/
void	FloodAreaConnections ( CCollisionBSPData *pBSPData )
{
	int		i;
	carea_t	*area;
	int		floodnum;

	// all current floods are now invalid
	pBSPData->floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for (i=1 ; i<pBSPData->numareas ; i++)
	{
		area = &pBSPData->map_areas[i];
		if (area->floodvalid == pBSPData->floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (pBSPData, area, floodnum);
	}

}

void CM_SetAreaPortalState( int portalnum, int isOpen )
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// Portalnums in the BSP file are 1-based instead of 0-based
	if (portalnum > pBSPData->numareaportals)
	{
		Sys_Error( "portalnum > numareaportals");
	}

	pBSPData->portalopen[portalnum] = (isOpen != 0);
	FloodAreaConnections (pBSPData);
}

void CM_SetAreaPortalStates( const int *portalnums, const int *isOpen, int nPortals )
{
	if ( nPortals == 0 )
		return;

	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// get the current collision bsp -- there is only one!
	for ( int i=0; i < nPortals; i++ )
	{
		// Portalnums in the BSP file are 1-based instead of 0-based
		if (portalnums[i] > pBSPData->numareaportals)
			Sys_Error( "portalnum > numareaportals");

		pBSPData->portalopen[portalnums[i]] = (isOpen[i] != 0);
	}

	FloodAreaConnections( pBSPData );
}

bool	CM_AreasConnected (int area1, int area2)
{
	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (map_noareas.GetInt())
		return true;

	if (area1 >= pBSPData->numareas || area2 >= pBSPData->numareas)
	{
		Sys_Error( "area(1==%i, 2==%i) >= numareas (%i):  Check if engine->ResetPVS() was called from ClientSetupVisibility", area1, area2,  pBSPData->numareas );
	}

	return (pBSPData->map_areas[area1].floodnum == pBSPData->map_areas[area2].floodnum);
}


/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits ( byte *buffer, int buflen, int area )
{
	int		i;
	int		floodnum;
	int		bytes;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	bytes = (pBSPData->numareas+7)>>3;

	if ( map_noareas.GetInt() )
	{	
		// for debugging, send everything
		Q_memset( buffer, 255, 3 );
	}
	else
	{
		if ( buflen < 32 )
		{
			Sys_Error( "CM_WriteAreaBits with buffer size < 32\n" );
		}

		Q_memset( buffer, 0, 32 );

		floodnum = pBSPData->map_areas[area].floodnum;
		for (i=0 ; i<pBSPData->numareas ; i++)
		{
			if (pBSPData->map_areas[i].floodnum == floodnum || !area)
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}


bool CM_GetAreaPortalPlane( const Vector &vViewOrigin, int portalKey, VPlane *pPlane )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	// First, find the leaf and area the viewer is in.
	int iLeaf = CM_PointLeafnum( vViewOrigin );
	if( iLeaf < 0 || iLeaf >= pBSPData->numleafs )
		return false;

	int iArea = pBSPData->map_leafs[iLeaf].area;
	if( iArea < 0 || iArea >= pBSPData->numareas )
		return false;

	carea_t *pArea = &pBSPData->map_areas[iArea];
	for( int i=0; i < pArea->numareaportals; i++ )
	{
		dareaportal_t *pPortal = &pBSPData->map_areaportals[pArea->firstareaportal + i];

		if( pPortal->m_PortalKey == portalKey )
		{
			cplane_t *pMapPlane = &pBSPData->map_planes[pPortal->planenum];
			pPlane->m_Normal = pMapPlane->normal;
			pPlane->m_Dist = pMapPlane->dist;
			return true;
		}
	}

	return false;
}


/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
bool CM_HeadnodeVisible (int nodenum, const byte *visbits, int vissize )
{
	int		leafnum;
	int		cluster;
	cnode_t	*node;

	// get the current collision bsp -- there is only one!
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = pBSPData->map_leafs[leafnum].cluster;
		if (cluster == -1)
			return false;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	node = &pBSPData->map_rootnode[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits, vissize ))
		return true;
	return CM_HeadnodeVisible(node->children[1], visbits, vissize );
}



//-----------------------------------------------------------------------------
// Purpose: returns true if the box is in a cluster that is visible in the visbits
// Input  : mins - box extents
//			maxs - 
//			*visbits - pvs or pas of some cluster
// Output : true if visible, false if not
//-----------------------------------------------------------------------------
#define MAX_BOX_LEAVES	256
int	CM_BoxVisible( const Vector& mins, const Vector& maxs, const byte *visbits, int vissize )
{
	int leafList[MAX_BOX_LEAVES];
	int topnode;

	// FIXME: Could save a loop here by traversing the tree in this routine like the code above
	int count = CM_BoxLeafnums( mins, maxs, leafList, MAX_BOX_LEAVES, &topnode );
	for ( int i = 0; i < count; i++ )
	{
		int cluster = CM_LeafCluster( leafList[i] );
		int offset = cluster>>3;

		if ( offset == -1 )
		{
			return false;
		}

		if ( offset > vissize || offset < 0 )
		{
			Sys_Error( "CM_BoxVisible:  cluster %i, offset %i out of bounds %i\n", cluster, offset, vissize );
		}

		if (visbits[cluster>>3] & (1<<(cluster&7)))
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Returns the world-space center of an entity
//-----------------------------------------------------------------------------
void CM_WorldSpaceCenter( ICollideable *pCollideable, Vector *pCenter )
{
	Vector vecLocalCenter;
	VectorAdd( pCollideable->OBBMins(), pCollideable->OBBMaxs(), vecLocalCenter );
	vecLocalCenter *= 0.5f;

	if ( ( pCollideable->GetCollisionAngles() == vec3_angle ) || ( vecLocalCenter == vec3_origin ) )
	{
		VectorAdd( vecLocalCenter, pCollideable->GetCollisionOrigin(), *pCenter );
	}
	else
	{
		VectorTransform( vecLocalCenter, pCollideable->CollisionToWorldTransform(), *pCenter );
	}
}


//-----------------------------------------------------------------------------
// Returns the world-align bounds of an entity
//-----------------------------------------------------------------------------
void CM_WorldAlignBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs )
{
	if ( pCollideable->GetCollisionAngles() == vec3_angle )
	{
		*pMins = pCollideable->OBBMins();
		*pMaxs = pCollideable->OBBMaxs();
	}
	else
	{
		ITransformAABB( pCollideable->CollisionToWorldTransform(), pCollideable->OBBMins(), pCollideable->OBBMaxs(), *pMins, *pMaxs );
		*pMins -= pCollideable->GetCollisionOrigin();
		*pMaxs -= pCollideable->GetCollisionOrigin();
	}
}


//-----------------------------------------------------------------------------
// Returns the world-space bounds of an entity
//-----------------------------------------------------------------------------
void CM_WorldSpaceBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs )
{
	if ( pCollideable->GetCollisionAngles() == vec3_angle )
	{
		VectorAdd( pCollideable->GetCollisionOrigin(), pCollideable->OBBMins(), *pMins );
		VectorAdd( pCollideable->GetCollisionOrigin(), pCollideable->OBBMaxs(), *pMaxs );
	}
	else
	{
		TransformAABB( pCollideable->CollisionToWorldTransform(), pCollideable->OBBMins(), pCollideable->OBBMaxs(), *pMins, *pMaxs );
	}
}


void CM_SetupAreaFloodNums( byte areaFloodNums[MAX_MAP_AREAS], int *pNumAreas )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	*pNumAreas = pBSPData->numareas;
	if ( pBSPData->numareas > MAX_MAP_AREAS )
		Error( "pBSPData->numareas > MAX_MAP_AREAS" );

	for ( int i=0; i < pBSPData->numareas; i++ )
	{
		Assert( pBSPData->map_areas[i].floodnum < MAX_MAP_AREAS );
		areaFloodNums[i] = (byte)pBSPData->map_areas[i].floodnum;
	}
}


// -----------------------------------------------------------------------------
// CFastLeafAccessor implementation.
// -----------------------------------------------------------------------------

CFastPointLeafNum::CFastPointLeafNum()
{
	m_flDistToExitLeafSqr = -1;
	m_vCachedPos.Init();
}


int CFastPointLeafNum::GetLeaf( const Vector &vPos )
{
	if ( vPos.DistToSqr( m_vCachedPos ) > m_flDistToExitLeafSqr )
	{
		m_vCachedPos = vPos;

		CCollisionBSPData *pBSPData = GetCollisionBSPData();
		m_flDistToExitLeafSqr = 1e24;
		m_iCachedLeaf = CM_PointLeafnumMinDistSqr_r( pBSPData, vPos, 0, m_flDistToExitLeafSqr );
	}

	return m_iCachedLeaf;
}


bool FASTCALL IsBoxIntersectingRayNoLowest( fltx4 boxMin, fltx4  boxMax, 
								   const fltx4 & origin, const fltx4 & delta, const fltx4 & invDelta, // ray parameters
								   const fltx4 & vTolerance ///< eg from ReplicateX4(flTolerance)
								   )
{
	/*
	Assert( boxMin[0] <= boxMax[0] );
	Assert( boxMin[1] <= boxMax[1] );
	Assert( boxMin[2] <= boxMax[2] );
	*/
#if defined(_X360) && defined(DBGFLAG_ASSERT)
	unsigned int r;
	AssertMsg( (XMVectorGreaterOrEqualR(&r, SetWToZeroSIMD(boxMax),SetWToZeroSIMD(boxMin)), XMComparisonAllTrue(r)), "IsBoxIntersectingRay : boxmax < boxmin" );
#endif

	// test if delta is tiny along any dimension
	fltx4 bvDeltaTinyComponents = CmpInBoundsSIMD( delta, Four_Epsilons );

	// push box extents out by tolerance (safe to do because pass by copy, not ref)
	boxMin = SubSIMD(boxMin, vTolerance);
	boxMax = AddSIMD(boxMax, vTolerance);


	// for the very  short components of the ray, check if the origin is inside the box;
	// if not, then it doesn't intersect.
	fltx4 bvOriginOutsideBox = OrSIMD( CmpLtSIMD(origin,boxMin), CmpGtSIMD(origin,boxMax) );
	bvDeltaTinyComponents = SetWToZeroSIMD(bvDeltaTinyComponents);

	// work out entry and exit points for the ray. This may produce strange results for 
	// very short delta components, but those will be masked out by bvDeltaTinyComponents
	// anyway. We could early-out on bvOriginOutsideBox, but it won't be ready to branch
	// on for fourteen cycles.
	fltx4 vt1 = SubSIMD( boxMin, origin );
	fltx4 vt2 = SubSIMD( boxMax, origin );
	vt1 = MulSIMD( vt1, invDelta );
	vt2 = MulSIMD( vt2, invDelta );

	// ensure that vt1<vt2
	{
		fltx4 temp = MaxSIMD( vt1, vt2 );
		vt1 = MinSIMD( vt1, vt2 );
		vt2 = temp;
	}

	// Non-parallel case
	// Find the t's corresponding to the entry and exit of
	// the ray along x, y, and z. The find the furthest entry
	// point, and the closest exit point. Once that is done,
	// we know we don't collide if the closest exit point
	// is behind the starting location. We also don't collide if
	// the closest exit point is in front of the furthest entry point
	fltx4 closestExit,furthestEntry;
	{
		VectorAligned temp;
		StoreAlignedSIMD(temp.Base(),vt2);
		closestExit = ReplicateX4( min( min(temp.x,temp.y), temp.z) );

		StoreAlignedSIMD(temp.Base(),vt1);
		furthestEntry = ReplicateX4( max( max(temp.x,temp.y), temp.z) );
	}


	// now start testing. We bail out if:
	// any component with tiny delta has origin outside the box
	if (!IsAllZeros(AndSIMD(bvOriginOutsideBox, bvDeltaTinyComponents)))
		return false;
	else
	{
		// however if there are tiny components inside the box, we 
		// know that they are good. (we didn't really need to run
		// the other computations on them, but it was faster to do
		// so than branching around them).

		// now it's the origin INSIDE box (eg, tiny components & ~outside box)
		bvOriginOutsideBox = AndNotSIMD(bvOriginOutsideBox,bvDeltaTinyComponents);
	}

	// closest exit is in front of furthest entry
	fltx4 tminOverTmax = CmpGtSIMD( furthestEntry, closestExit );
	// closest exit is behind start, or furthest entry after end
	fltx4 outOfBounds = OrSIMD( CmpGtSIMD(furthestEntry, LoadOneSIMD()), CmpGtSIMD( LoadZeroSIMD(), closestExit ) );
	fltx4 failedComponents = OrSIMD(tminOverTmax, outOfBounds); // any 1's here mean return false
	// but, if a component is tiny and has its origin inside the box, ignore the computation against bogus invDelta.
	failedComponents = AndNotSIMD(bvOriginOutsideBox,failedComponents); 
	return ( IsAllZeros( SetWToZeroSIMD( failedComponents ) ) );
}

// function to time IsBoxIntersectingRay 
#if 0
/*
//-----------------------------------------------------------------------------
bool FASTCALL IsBoxIntersectingRay( fltx4 boxMin, fltx4 boxMax, 
fltx4 origin, fltx4 delta, fltx4 invDelta, // ray parameters
fltx4 vTolerance ///< eg from ReplicateX4(flTolerance)
)
{
*/
CON_COMMAND( opt_test_collision, "Quick timing test in IsBoxIntersectingRay" )
{
	int numIters = 100000;
	if (args.ArgC() >= 1)
	{
		numIters = Q_atoi(args.Arg(1));
	}
	{
		fltx4 boxMin = {1,1,1,0};
		fltx4 boxMax = {2,2,2,0};
		fltx4 origin = {0,0,0,0};
		fltx4 delta = {3,4,3,0};
		fltx4 invdelta = {1.0f/3.0f, 1.0f/4.0f, 1.0f/3.0f,0};
		fltx4 flTolerance = ReplicateX4(.0001f);

		double startTime = Plat_FloatTime();
		for (int i = numIters ; i > 0 ; --i)
			IsBoxIntersectingRayNoLowest(boxMin,boxMax,origin,delta,invdelta,flTolerance);
		double endTime = Plat_FloatTime();
		Msg("without FindLowest algorithm: %.4f secs for %d runs\n",endTime - startTime,numIters);
	}

	{
		fltx4 boxMin = {1,1,1,0};
		fltx4 boxMax = {2,2,2,0};
		fltx4 origin = {0,0,0,0};
		fltx4 delta = {3,4,3,0};
		fltx4 invdelta = {1.0f/3.0f, 1.0f/4.0f, 1.0f/3.0f,0};
		fltx4 flTolerance = ReplicateX4(.0001f);

		double startTime = Plat_FloatTime();
		for (int i = numIters ; i > 0 ; --i)
			IsBoxIntersectingRay(boxMin,boxMax,origin,delta,invdelta,flTolerance);
		double endTime = Plat_FloatTime();
		Msg("using FindLowest algorithm: %.4f secs for %d runs\n",endTime - startTime,numIters);
	}

}

CON_COMMAND( opt_test_rotation, "Quick timing test of vector rotation my m3x4" )
{
	int numIters = 100000;
	if (args.ArgC() >= 1)
	{
		numIters = Q_atoi(args.Arg(1));
	}

	// construct an array of 1024 random vectors
	FourVectors testData[1024];
	SeedRandSIMD(Plat_MSTime());
	for (int i = 0 ; i < 1024 ; ++i)
	{
		testData[i].x = RandSIMD();
		testData[i].y = RandSIMD();
		testData[i].z = RandSIMD();
	}

	// for also testing store latency
	FourVectors outScratch[16];

	matrix3x4_t rota;
	AngleIMatrix(QAngle(30,60,90), rota);

	// THREE DOT PRODUCTS
	{
		double startTime = Plat_FloatTime();
		for (int i = numIters ; i > 0 ; --i)
		{
			int in = i & 1023;
			int out = i & 15;
			
			outScratch[out].x = testData[in] * *reinterpret_cast<Vector *>(rota[0]);
			outScratch[out].y = testData[in] * *reinterpret_cast<Vector *>(rota[1]);
			outScratch[out].z = testData[in] * *reinterpret_cast<Vector *>(rota[2]);
		}
		double endTime = Plat_FloatTime();
		Msg("THREE DOT PRODUCTS: %.4f secs for %d runs\n",endTime - startTime,numIters);
	}

	// REPEATED CALLS TO ROTATEBY
	{

		double startTime = Plat_FloatTime();
		for (int i = numIters ; i > 0 ; --i)
		{
			int in = i & 1023;
			int out = i & 15;

			outScratch[out] = testData[in];
			outScratch[out].RotateBy(rota);
		}
		double endTime = Plat_FloatTime();
		Msg("REPEATED CALLS TO ROTATEBY: %.4f secs for %d runs\n",endTime - startTime,numIters);
	}

	// ROTATEBYMANY
	{

		double startTime = Plat_FloatTime();

		int lastBatch = numIters - 1023;
		int i;
		for (i = 0 ; i < lastBatch ; i+=1024 )
		{
			FourVectors::RotateManyBy(testData, 1024, rota);
		}
		if (i < numIters)
		{
			FourVectors::RotateManyBy(testData, numIters-i, rota);
		}

		double endTime = Plat_FloatTime();
		Msg("ROTATEBYMANY: %.4f secs for %d runs\n",endTime - startTime,numIters);
	}

	// test
	FourVectors res1, res2;
	res2 = testData[0];
	res1.x = testData[0] * *reinterpret_cast<Vector *>(rota[0]);
	res1.y = testData[0] * *reinterpret_cast<Vector *>(rota[1]);
	res1.z = testData[0] * *reinterpret_cast<Vector *>(rota[2]);

	res2.RotateBy(rota);

	Msg("%.3f %.3f %.3f %.3f   \t%.3f %.3f %.3f %.3f\n", SubFloat(res1.x, 0),  SubFloat(res1.x, 1), SubFloat(res1.x, 2), SubFloat(res1.x, 3),
		SubFloat(res2.x, 0),  SubFloat(res2.x, 1), SubFloat(res2.x, 2), SubFloat(res2.x, 3));

	Msg("%.3f %.3f %.3f %.3f   \t%.3f %.3f %.3f %.3f\n", SubFloat(res1.y, 0),  SubFloat(res1.y, 1), SubFloat(res1.y, 2), SubFloat(res1.y, 3),
		SubFloat(res2.y, 0),  SubFloat(res2.y, 1), SubFloat(res2.y, 2), SubFloat(res2.y, 3));

	Msg("%.3f %.3f %.3f %.3f   \t%.3f %.3f %.3f %.3f\n", SubFloat(res1.z, 0),  SubFloat(res1.z, 1), SubFloat(res1.z, 2), SubFloat(res1.z, 3),
		SubFloat(res2.z, 0),  SubFloat(res2.z, 1), SubFloat(res2.z, 2), SubFloat(res2.z, 3));

}

#endif
