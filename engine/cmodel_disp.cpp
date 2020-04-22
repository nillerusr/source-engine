//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <assert.h>
#include <float.h>
#include "cmodel_engine.h"
#include "dispcoll_common.h"
#include "cmodel_private.h"
#include "builddisp.h"
#include "collisionutils.h"
#include "vstdlib/random.h"
#include "tier0/fasttimer.h"
#include "vphysics_interface.h"
#include "vphysics/virtualmesh.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int g_DispCollTreeCount = 0;
CDispCollTree *g_pDispCollTrees = NULL;
alignedbbox_t *g_pDispBounds = NULL;
class CVirtualTerrain;

//csurface_t dispSurf = { "terrain", 0, 0 };

void CM_PreStab( TraceInfo_t *pTraceInfo, cleaf_t *pLeaf, Vector &vStabDir, int collisionMask, int &contents );
void CM_Stab( TraceInfo_t *pTraceInfo, Vector const &start, Vector const &vStabDir, int contents );
void CM_PostStab( TraceInfo_t *pTraceInfo );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void SetDispTraceSurfaceProps( trace_t *pTrace, CDispCollTree *pDisp )
{
	// use the default surface properties
	pTrace->surface.name = "**displacement**";
	pTrace->surface.flags = 0;
	if ( pTrace->IsDispSurfaceProp2() )
	{
		pTrace->surface.surfaceProps = pDisp->GetSurfaceProps( 1 );
	}
	else
	{
		pTrace->surface.surfaceProps = pDisp->GetSurfaceProps( 0 );
	}
}

class CDispLeafBuilder
{
public:
	CDispLeafBuilder( CCollisionBSPData *pBSPData )
	{
		m_pBSPData = pBSPData;
		// don't want this to resize much, so make the backing buffer large
		m_dispList.EnsureCapacity( MAX_MAP_DISPINFO * 2 );

		// size both of these to the size of the array since there is exactly one per element
		m_leafCount.SetCount( g_DispCollTreeCount );
		m_firstIndex.SetCount( g_DispCollTreeCount );
		for ( int i = 0; i < g_DispCollTreeCount; i++ )
		{
			m_leafCount[i] = 0;
			m_firstIndex[i] = -1;
		}
	}

	void BuildLeafListForDisplacement( int index )
	{
		// get tree and see if it is real (power != 0)
		CDispCollTree *pDispTree = &g_pDispCollTrees[index];
		if( !pDispTree || ( pDispTree->GetPower() == 0 ) )
			return;
		m_firstIndex[index] = m_dispList.Count();
		m_leafCount[index] = 0;
		const int MAX_NODES = 1024;
		int nodeList[MAX_NODES];
		int listRead = 0;
		int listWrite = 1;
		nodeList[0] = m_pBSPData->map_cmodels[0].headnode;
		Vector mins, maxs;
		pDispTree->GetBounds( mins, maxs );

		// UNDONE: The rendering code did this, do we need it?
//		mins -= Vector( 0.5, 0.5, 0.5 );
//		maxs += Vector( 0.5, 0.5, 0.5 );

		while( listRead != listWrite )
		{
			int nodeIndex = nodeList[listRead];
			listRead = (listRead+1)%MAX_NODES;

			// if this is a leaf, add it to the array
			if( nodeIndex < 0 )
			{
				int leafIndex = -1 - nodeIndex;
				m_dispList.AddToTail(leafIndex);
				m_leafCount[index]++;
			}
			else
			{
				//
				// choose side(s) to traverse
				//
				cnode_t *pNode = &m_pBSPData->map_rootnode[nodeIndex];
				cplane_t *pPlane = pNode->plane;

				int sideResult = BOX_ON_PLANE_SIDE( mins, maxs, pPlane );

				// front side
				if( sideResult & 1 )
				{
					nodeList[listWrite] = pNode->children[0];
					listWrite = (listWrite+1)%MAX_NODES;
					Assert(listWrite != listRead);
				}
				// back side
				if( sideResult & 2 )
				{
					nodeList[listWrite] = pNode->children[1];
					listWrite = (listWrite+1)%MAX_NODES;
					Assert(listWrite != listRead);
				}
			}
		}
	}
	int GetDispListCount() { return m_dispList.Count(); }
	void WriteLeafList( unsigned short *pLeafList )
	{
		// clear current count if any
		for ( int i = 0; i < m_pBSPData->numleafs; i++ )
		{
			cleaf_t *pLeaf = &m_pBSPData->map_leafs[i];
			pLeaf->dispCount = 0;
		}
		// compute new count per leaf
		for ( int i = 0; i < m_dispList.Count(); i++ )
		{
			int leafIndex = m_dispList[i];
			cleaf_t *pLeaf = &m_pBSPData->map_leafs[leafIndex];
			pLeaf->dispCount++;
		}
		// point each leaf at the start of it's output range in the output array
		unsigned short firstDispIndex = 0;
		for ( int i = 0; i < m_pBSPData->numleafs; i++ )
		{
			cleaf_t *pLeaf = &m_pBSPData->map_leafs[i];
			pLeaf->dispListStart = firstDispIndex;
			firstDispIndex += pLeaf->dispCount;
			pLeaf->dispCount = 0;
		}
		// now iterate the references in disp order adding to each leaf's (now compact) list
		// for each displacement with leaves
		for ( int i = 0; i < m_leafCount.Count(); i++ )
		{
			// for each leaf in this disp's list
			int count = m_leafCount[i];
			for ( int j = 0; j < count; j++ )
			{
				int listIndex = m_firstIndex[i] + j;					// index to per-disp list
				int leafIndex = m_dispList[listIndex];					// this reference is for one leaf
				cleaf_t *pLeaf = &m_pBSPData->map_leafs[leafIndex];
				int outListIndex = pLeaf->dispListStart + pLeaf->dispCount;	// output position for this leaf
				pLeafList[outListIndex] = i;							// write the reference there
				Assert(outListIndex < GetDispListCount());
				pLeaf->dispCount++;										// move this leaf's output pointer
			}
		}
	}

private:
	CCollisionBSPData *m_pBSPData;
	// this is a list of all of the leaf indices for each displacement
	CUtlVector<unsigned short> m_dispList;
	// this is the first entry into dispList for each displacement
	CUtlVector<int> m_firstIndex;
	// this is the # of leaf entries for each displacement
	CUtlVector<unsigned short> m_leafCount;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CM_DispTreeLeafnum( CCollisionBSPData *pBSPData )
{
	// check to see if there are any displacement trees to push down the bsp tree??
	if( g_DispCollTreeCount == 0 )
		return;

	for ( int i = 0; i < pBSPData->numleafs; i++ )
	{
		pBSPData->map_leafs[i].dispCount = 0;
	}
	//
	// get the number of displacements per leaf
	//
	CDispLeafBuilder leafBuilder( pBSPData );

	for ( int i = 0; i < g_DispCollTreeCount; i++ )
	{
		leafBuilder.BuildLeafListForDisplacement( i );
	}
	int count = leafBuilder.GetDispListCount();
	pBSPData->map_dispList.Attach( count, (unsigned short*)Hunk_Alloc( sizeof(unsigned short) * count, false ) );
	leafBuilder.WriteLeafList( pBSPData->map_dispList.Base() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void DispCollTrees_FreeLeafList( CCollisionBSPData *pBSPData )
{
	if ( pBSPData->map_dispList.Base() )
	{
		pBSPData->map_dispList.Detach();
		pBSPData->numdisplist = 0;
	}
}

// Virtual collision models for terrain
class CVirtualTerrain : public IVirtualMeshEvent
{
public:
	CVirtualTerrain()
	{
		m_pDispHullData =  NULL;
	}
	// Fill out the meshlist for this terrain patch
	virtual void GetVirtualMesh( void *userData, virtualmeshlist_t *pList )
	{
		int index = (int)userData;
		Assert(index >= 0 && index < g_DispCollTreeCount );
		g_pDispCollTrees[index].GetVirtualMeshList( pList );
		pList->pHull = NULL;
		if ( m_pDispHullData )
		{
			if ( m_dispHullOffset[index] >= 0 )
			{
				pList->pHull = m_pDispHullData + m_dispHullOffset[index];
			}
		}
	}
	// returns the bounds for the terrain patch
	virtual void GetWorldspaceBounds( void *userData, Vector *pMins, Vector *pMaxs )
	{
		int index = (int)userData;
		*pMins = g_pDispBounds[index].mins;
		*pMaxs = g_pDispBounds[index].maxs;
	}
	// Query against the AABB tree to find the list of triangles for this patch in a sphere
	virtual void GetTrianglesInSphere( void *userData, const Vector &center, float radius, virtualmeshtrianglelist_t *pList )
	{
		int index = (int)userData;
		pList->triangleCount = g_pDispCollTrees[index].AABBTree_GetTrisInSphere( center, radius, pList->triangleIndices, ARRAYSIZE(pList->triangleIndices) );
	}
	void LevelInit( dphysdisp_t *pLump, int lumpSize )
	{
		if ( !pLump )
		{
			m_pDispHullData = NULL;
			return;
		}
		int totalHullData = 0;
		m_dispHullOffset.SetCount(g_DispCollTreeCount);
		Assert(pLump->numDisplacements==g_DispCollTreeCount);
		// count the size of the lump
		unsigned short *pDataSize = (unsigned short *)(pLump+1);
		for ( int i = 0; i < pLump->numDisplacements; i++ )
		{
			if ( pDataSize[i] == (unsigned short)-1 )
			{
				m_dispHullOffset[i] = -1;
				continue;
			}
			m_dispHullOffset[i] = totalHullData;
			totalHullData += pDataSize[i];
		}
		m_pDispHullData = new byte[totalHullData];
		byte *pData = (byte *)(pDataSize + pLump->numDisplacements);
		memcpy( m_pDispHullData, pData, totalHullData );
#if _DEBUG
		int offset = pData - ((byte *)pLump);
		Assert( offset + totalHullData == lumpSize );
#endif
	}
	void LevelShutdown()
	{
		m_dispHullOffset.Purge();
		delete[] m_pDispHullData;
		m_pDispHullData = NULL;
	}

private:
	byte			*m_pDispHullData;
	CUtlVector<int> m_dispHullOffset;
};

// Singleton to implement the callbacks
static CVirtualTerrain g_VirtualTerrain;
// List of terrain collision models for the currently loaded level, indexed by terrain patch index
static CUtlVector<CPhysCollide *> g_TerrainList;

// Find or create virtual terrain collision model.  Note that these will be shared by client & server
CPhysCollide *CM_PhysCollideForDisp( int index )
{
	if ( index < 0 || index >= g_DispCollTreeCount )
		return NULL;

	return g_TerrainList[index];
}

int CM_SurfacepropsForDisp( int index )
{
	return g_pDispCollTrees[index].GetSurfaceProps(0);
}

void CM_CreateDispPhysCollide( dphysdisp_t *pDispLump, int dispLumpSize )
{
	g_VirtualTerrain.LevelInit(pDispLump, dispLumpSize);
	g_TerrainList.SetCount( g_DispCollTreeCount );
	for ( int i = 0; i < g_DispCollTreeCount; i++ )
	{
		// Don't create a physics collision model for displacements that have been tagged as such.
		CDispCollTree *pDispTree = &g_pDispCollTrees[i];
		if ( pDispTree && pDispTree->CheckFlags( CCoreDispInfo::SURF_NOPHYSICS_COLL ) )
		{
			g_TerrainList[i] = NULL;
			continue;
		}
		virtualmeshparams_t params;
		params.pMeshEventHandler = &g_VirtualTerrain;
		params.userData = (void *)i;
		params.buildOuterHull = dispLumpSize > 0 ? false : true;

		g_TerrainList[i] = physcollision->CreateVirtualMesh( params );
	}
}

// End of level, free the collision models
void CM_DestroyDispPhysCollide()
{
	g_VirtualTerrain.LevelShutdown();
	for ( int i = g_TerrainList.Count()-1; i>=0; --i )
	{
		physcollision->DestroyCollide( g_TerrainList[i] );
	}
	g_TerrainList.Purge();
}

//-----------------------------------------------------------------------------
// New Collision!
//-----------------------------------------------------------------------------
void CM_TestInDispTree( TraceInfo_t *pTraceInfo, cleaf_t *pLeaf, const Vector &traceStart,
					   const Vector &boxMin, const Vector &boxMax, int collisionMask, trace_t *pTrace )
{
	bool bIsBox = ( ( boxMin.x != 0.0f ) || ( boxMin.y != 0.0f ) || ( boxMin.z != 0.0f ) ||
		( boxMax.x != 0.0f ) || ( boxMax.y != 0.0f ) || ( boxMax.z != 0.0f ) );

	// box test
	if( bIsBox )
	{
		// Box/Tree intersection test.
		Vector absMins = traceStart + boxMin;
		Vector absMaxs = traceStart + boxMax;

		// Test box against all displacements in the leaf.
		TraceCounter_t *pCounters = pTraceInfo->GetDispCounters();
		int count = pTraceInfo->GetCount();
		for( int i = 0; i < pLeaf->dispCount; i++ )
		{
			int dispIndex = pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart + i];
			alignedbbox_t * RESTRICT pDispBounds = &g_pDispBounds[dispIndex];

			// Respect trace contents
			if( !(pDispBounds->GetContents() & collisionMask) )
				continue;

			if ( !pTraceInfo->Visit( pDispBounds->GetCounter(), count, pCounters ) )
				continue;

			if ( IsBoxIntersectingBox( absMins, absMaxs, pDispBounds->mins, pDispBounds->maxs ) )
			{
				CDispCollTree *pDispTree = &g_pDispCollTrees[dispIndex];
				if( pDispTree->AABBTree_IntersectAABB( absMins, absMaxs ) )
				{
					pTrace->startsolid = true;
					pTrace->allsolid = true;
					pTrace->fraction = 0.0f;
					pTrace->fractionleftsolid = 0.0f;
					pTrace->contents = pDispTree->GetContents();
					return;
				}
			}
		}
	}

	//
	// need to stab if is was a point test or the box test yeilded no intersection
	//
	Vector stabDir;
	int    contents;
	CM_PreStab( pTraceInfo, pLeaf, stabDir, collisionMask, contents );
	CM_Stab( pTraceInfo, traceStart, stabDir, contents );
	CM_PostStab( pTraceInfo );
}


//-----------------------------------------------------------------------------
// New Collision!
//-----------------------------------------------------------------------------
void CM_PreStab( TraceInfo_t *pTraceInfo, cleaf_t *pLeaf, Vector &vStabDir, int collisionMask, int &contents )
{
	if( !pLeaf->dispCount )
		return;

	// if the point wasn't in the bounded area of any of the displacements -- stab in any
	// direction and set contents to "solid"
	int dispIndex = pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart];
	CDispCollTree *pDispTree = &g_pDispCollTrees[dispIndex];
	pDispTree->GetStabDirection( vStabDir );
	contents = CONTENTS_SOLID;

	//
	// if the point is inside a displacement's (in the leaf) bounded area
	// then get the direction to stab from it
	//
	for( int i = 0; i < pLeaf->dispCount; i++ )
	{
		dispIndex = pTraceInfo->m_pBSPData->map_dispList[pLeaf->dispListStart + i];
		pDispTree = &g_pDispCollTrees[dispIndex];

		// Respect trace contents
		if( !(pDispTree->GetContents() & collisionMask) )
			continue;

		if( pDispTree->PointInBounds( pTraceInfo->m_start, pTraceInfo->m_mins, pTraceInfo->m_maxs, pTraceInfo->m_ispoint ) )
		{
			pDispTree->GetStabDirection( vStabDir );
			contents = pDispTree->GetContents();
			return;
		}
	}
}

static Vector InvDelta(const Vector &delta)
{
	Vector vecInvDelta;
	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		if ( delta[iAxis] != 0.0f )
		{
			vecInvDelta[iAxis] = 1.0f / delta[iAxis];
		}
		else
		{
			vecInvDelta[iAxis] = FLT_MAX;
		}
	}
	return vecInvDelta;
}

//-----------------------------------------------------------------------------
// New Collision!
//-----------------------------------------------------------------------------
void CM_Stab( TraceInfo_t *pTraceInfo, const Vector &start, const Vector &vStabDir, int contents )
{
	//
	// initialize the displacement trace parameters
	//
	pTraceInfo->m_trace.fraction = 1.0f;
	pTraceInfo->m_trace.fractionleftsolid = 0.0f;
	pTraceInfo->m_trace.surface = pTraceInfo->m_pBSPData->nullsurface;

	pTraceInfo->m_trace.startsolid = false;
	pTraceInfo->m_trace.allsolid = false;

	pTraceInfo->m_bDispHit = false;
	pTraceInfo->m_DispStabDir = vStabDir;

	Vector end = pTraceInfo->m_end;

	pTraceInfo->m_start = start;
	pTraceInfo->m_end = start + ( vStabDir * /* world extents * 2*/99999.9f );
	pTraceInfo->m_delta = pTraceInfo->m_end - pTraceInfo->m_start;
	pTraceInfo->m_invDelta = InvDelta(pTraceInfo->m_delta);

	// increment the checkcount -- so we can retest objects that may have been tested
	// previous to the stab
	PushTraceVisits( pTraceInfo );

	// increment the stab count -- statistics
#ifdef COUNT_COLLISIONS
	g_CollisionCounts.m_Stabs++;
#endif

	// stab
	CM_RecursiveHullCheck( pTraceInfo, 0 /*root*/, 0.0f, 1.0f );

	PopTraceVisits( pTraceInfo );

	pTraceInfo->m_end = end;
}

//-----------------------------------------------------------------------------
// New Collision!
//-----------------------------------------------------------------------------
void CM_PostStab( TraceInfo_t *pTraceInfo )
{
	//
	// only need to resolve things that impacted against a displacement surface,
	// this is partially resolved in the post trace phase -- so just use that
	// data to determine
	//
	if( pTraceInfo->m_bDispHit && pTraceInfo->m_trace.startsolid )
	{
		pTraceInfo->m_trace.allsolid = true;
		pTraceInfo->m_trace.fraction = 0.0f;
		pTraceInfo->m_trace.fractionleftsolid = 0.0f;
	}
	else
	{
		pTraceInfo->m_trace.startsolid = false;
		pTraceInfo->m_trace.allsolid = false;
		pTraceInfo->m_trace.contents = 0;
		pTraceInfo->m_trace.fraction = 1.0f;
		pTraceInfo->m_trace.fractionleftsolid = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// New Collision!
//-----------------------------------------------------------------------------
void CM_PostTraceToDispTree( TraceInfo_t *pTraceInfo )
{
	// only resolve things that impacted against a displacement surface
	if( !pTraceInfo->m_bDispHit )
		return;

	//
	// determine whether or not we are in solid
	//	
	if( DotProduct( pTraceInfo->m_trace.plane.normal, pTraceInfo->m_delta ) > 0.0f )
	{
		pTraceInfo->m_trace.startsolid = true;
		pTraceInfo->m_trace.allsolid = true;
	}
}

//-----------------------------------------------------------------------------
// New Collision!
//-----------------------------------------------------------------------------
template <bool IS_POINT>
void FASTCALL CM_TraceToDispTree( TraceInfo_t *pTraceInfo, CDispCollTree *pDispTree, float startFrac, float endFrac )
{
	Ray_t ray;
	ray.m_Start = pTraceInfo->m_start;
	ray.m_Delta = pTraceInfo->m_delta;
	ray.m_IsSwept = true;

	trace_t *pTrace = &pTraceInfo->m_trace;

	// ray cast
	if( IS_POINT )
	{
		ray.m_Extents.Init();
		ray.m_IsRay = true;

		if( pDispTree->AABBTree_Ray( ray, pTraceInfo->m_invDelta, pTrace ) )
		{
			pTraceInfo->m_bDispHit = true;
			pTrace->contents = pDispTree->GetContents();
			SetDispTraceSurfaceProps( pTrace, pDispTree );
		}
	}
	// box sweep
	else
	{
		ray.m_Extents = pTraceInfo->m_extents;
		ray.m_IsRay = false;
		if( pDispTree->AABBTree_SweepAABB( ray, pTraceInfo->m_invDelta, pTrace ) )
		{
			pTraceInfo->m_bDispHit = true;
			pTrace->contents = pDispTree->GetContents();
			SetDispTraceSurfaceProps( pTrace, pDispTree );
		}
	}

	//	CM_TraceToDispTreeTest( pDispTree, traceStart, traceEnd, boxMin, boxMax, startFrac, endFrac, pTrace, bRayCast );
}

template void FASTCALL CM_TraceToDispTree<true>( TraceInfo_t *pTraceInfo, CDispCollTree *pDispTree, float startFrac, float endFrac );

template void FASTCALL CM_TraceToDispTree<false>( TraceInfo_t *pTraceInfo, CDispCollTree *pDispTree, float startFrac, float endFrac );
