//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
// TODO:  Should all of this Map_Vis* stuff be an interface?
//

#include "quakedef.h"
#include "gl_model_private.h"
#include "view_shared.h"
#include "cmodel_engine.h"
#include "tier0/vprof.h"
#include "utllinkedlist.h"
#include "ivrenderview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar	r_novis( "r_novis","0", FCVAR_CHEAT	, "Turn off the PVS." );
static ConVar	r_lockpvs( "r_lockpvs", "0", FCVAR_CHEAT, "Lock the PVS so you can fly around and inspect what is being drawn." );

// ----------------------------------------------------------------------
// Renderer interface to vis
// ----------------------------------------------------------------------
int  r_visframecount = 0;

//-----------------------------------------------------------------------------
// Purpose: For each cluster to be OR'd into vis, remember the origin, the last viewcluster
//  for that origin and the current one, so we can tell when vis is dirty and needs to be
//  recomputed
//-----------------------------------------------------------------------------
typedef struct
{
	Vector	origin;
	int		viewcluster;
	int		oldviewcluster;
} VISCLUSTER;

//-----------------------------------------------------------------------------
// Purpose: Stores info for updating vis data for the map
//-----------------------------------------------------------------------------
typedef struct
{
	// Number of relevant vis clusters
	int			nClusters;
	// Last number ( if != nClusters, recompute vis )
	int			oldnClusters;
	// List of clusters to merge together for final vis
	VISCLUSTER	rgVisClusters[ MAX_VIS_LEAVES ];
	// Composite vis data
	byte		rgCurrentVis[ MAX_MAP_LEAFS/8 ];
	bool		bSkyVisible;
	bool		bForceFullSky;
} VISINFO;

static VISINFO vis;

// I think this is enough.  We should have enough here to cover what we might have in a frame, including:
// 1) water reflection
// 2) camera/monitor (actually, this is merged with the regular world)
// 3) 3dskybox
// 4) regular world
const int VISCACHE_SIZE = 8;

class VisCacheEntry
{
public:
	VisCacheEntry() { nClusters = 0; }

	int nClusters;
	int originclusters[MAX_VIS_LEAVES];
	CUtlVector< unsigned short > leaflist;
	CUtlVector< unsigned short > nodelist;
};


static CUtlLinkedList< VisCacheEntry > viscache( 0, VISCACHE_SIZE );


static void SortVisViewClusters()
{
	for (int i = 1; i < vis.nClusters; ++i)
	{
		int t = vis.rgVisClusters[i].viewcluster;
		int j = i;
		while (j > 0 && vis.rgVisClusters[j-1].viewcluster > t)
		{
			vis.rgVisClusters[j].viewcluster = vis.rgVisClusters[j-1].viewcluster;
			--j;
		}
		vis.rgVisClusters[j].viewcluster = t;
	}
}

static void VisMark_Cached( const VisCacheEntry &cache, const worldbrushdata_t &worldbrush )
{
	int count, visframe;

	visframe = r_visframecount;

	count = cache.leaflist.Count();
	const unsigned short * RESTRICT pSrc = cache.leaflist.Base();

#if _X360
	const int offsetLeaf = offsetof(mleaf_t, visframe);
	const int offsetNode = offsetof(mnode_t, visframe);
#endif

	while ( count >= 8 )
	{
#if _X360
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[0]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[1]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[2]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[3]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[4]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[5]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[6]) );
		__dcbt( offsetLeaf, (void *)(worldbrush.leafs + pSrc[7]) );
#endif
		worldbrush.leafs[pSrc[0]].visframe = visframe;
		worldbrush.leafs[pSrc[1]].visframe = visframe;
		worldbrush.leafs[pSrc[2]].visframe = visframe;
		worldbrush.leafs[pSrc[3]].visframe = visframe;
		worldbrush.leafs[pSrc[4]].visframe = visframe;
		worldbrush.leafs[pSrc[5]].visframe = visframe;
		worldbrush.leafs[pSrc[6]].visframe = visframe;
		worldbrush.leafs[pSrc[7]].visframe = visframe;
		pSrc += 8;
		count -= 8;
	}
	while ( count )
	{
		worldbrush.leafs[pSrc[0]].visframe = visframe;
		count--;
		pSrc++;
	}

	count = cache.nodelist.Count();
	pSrc = cache.nodelist.Base();

	while ( count >= 8 )
	{
#if _X360
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[0]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[1]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[2]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[3]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[4]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[5]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[6]) );
		__dcbt( offsetNode, (void *)(worldbrush.nodes + pSrc[7]) );
#endif
		worldbrush.nodes[pSrc[0]].visframe = visframe;
		worldbrush.nodes[pSrc[1]].visframe = visframe;
		worldbrush.nodes[pSrc[2]].visframe = visframe;
		worldbrush.nodes[pSrc[3]].visframe = visframe;
		worldbrush.nodes[pSrc[4]].visframe = visframe;
		worldbrush.nodes[pSrc[5]].visframe = visframe;
		worldbrush.nodes[pSrc[6]].visframe = visframe;
		worldbrush.nodes[pSrc[7]].visframe = visframe;
		pSrc += 8;
		count -= 8;
	}
	while ( count )
	{
		worldbrush.nodes[pSrc[0]].visframe = visframe;
		count--;
		pSrc++;
	}
}

static void VisCache_Build( VisCacheEntry &cache, const worldbrushdata_t &worldbrush )
{
	VPROF_INCREMENT_COUNTER( "VisCache misses", 1 );
	int			i;
	mleaf_t		*leaf;
	int			cluster;

	cache.nClusters = vis.nClusters;
	for (i = 0; i < vis.nClusters; ++i)
	{
		cache.originclusters[i] = vis.rgVisClusters[i].viewcluster;
	}
	
	cache.leaflist.RemoveAll();
	cache.nodelist.RemoveAll();

	int visframe = r_visframecount;

	for ( i = 0, leaf = worldbrush.leafs ; i < worldbrush.numleafs ; i++, leaf++)
	{
		MEM_ALLOC_CREDIT();
		cluster = leaf->cluster;
		if ( cluster == -1 )
			continue;

		if (vis.rgCurrentVis[cluster>>3] & (1<<(cluster&7)))
		{
			leaf->visframe = visframe;
			cache.leaflist.AddToTail( i );
			mnode_t *node = leaf->parent;
			while (node && node->visframe != visframe)
			{
				cache.nodelist.AddToTail( node - worldbrush.nodes );
				node->visframe = visframe;
				node = node->parent;
			}
		}
	} 
}


bool Map_AreAnyLeavesVisible( const worldbrushdata_t &worldbrush, int *leafList, int nLeaves )
{
	for ( int i=0; i < nLeaves; i++ )
	{
		const mleaf_t *leaf = &worldbrush.leafs[leafList[i]];
		int cluster = leaf->cluster;
		if ( cluster == -1 )
			continue;
		
		if ( vis.rgCurrentVis[cluster>>3] & (1<<(cluster&7)) )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Mark the leaves and nodes that are in the PVS for the current
//  cluster(s)
// Input  : *worldmodel - 
//-----------------------------------------------------------------------------
void Map_VisMark( bool forcenovis, model_t *worldmodel )
{
	VPROF( "Map_VisMark" );
	int			i, c;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if ( r_lockpvs.GetInt() )
	{
		return;
	}

	SortVisViewClusters();

	bool outsideWorld = false;
	for ( i = 0; i < vis.nClusters; i++ )
	{
		if ( vis.rgVisClusters[ i ].viewcluster != vis.rgVisClusters[ i ].oldviewcluster )
		{
			break;
		}
	}

	// No changes
	if ( i >= vis.nClusters && !forcenovis && ( vis.nClusters == vis.oldnClusters ) )
	{
		return;
	}

	// Update vis frame marker
	r_visframecount++;

	// Update cluster history
	vis.oldnClusters = vis.nClusters;
	for ( i = 0; i < vis.nClusters; i++ )
	{
		vis.rgVisClusters[ i ].oldviewcluster = vis.rgVisClusters[ i ].viewcluster;
		// Outside world?
		if ( vis.rgVisClusters[ i ].viewcluster == -1 )
		{
			outsideWorld = true;
			break;
		}
	}

#ifdef USE_CONVARS
	if ( r_novis.GetInt() || forcenovis || outsideWorld )
	{
		// mark everything
		for (i=0 ; i<worldmodel->brush.pShared->numleafs ; i++)
		{
			worldmodel->brush.pShared->leafs[i].visframe = r_visframecount;
		}
		for (i=0 ; i<worldmodel->brush.pShared->numnodes ; i++)
		{
			worldmodel->brush.pShared->nodes[i].visframe = r_visframecount;
		}
		return;
	}
#endif

	// There should always be at least one origin and that's the default render origin in most cases
	assert( vis.nClusters >= 1 );

	CM_Vis( vis.rgCurrentVis, sizeof( vis.rgCurrentVis ), vis.rgVisClusters[ 0 ].viewcluster, DVIS_PVS );

	// Get cluster count
	c = ( CM_NumClusters() + 31 ) / 32 ;

	// Merge in any extra clusters
	for ( i = 1; i < vis.nClusters; i++ )
	{
		byte	mapVis[ MAX_MAP_CLUSTERS/8 ];
		
		CM_Vis( mapVis, sizeof( mapVis ), vis.rgVisClusters[ i ].viewcluster, DVIS_PVS );
				
		// Copy one dword at a time ( could use memcpy )
		for ( int j = 0 ; j < c ; j++ )
		{
			((int *)vis.rgCurrentVis)[ j ] |= ((int *)mapVis)[ j ];
		}
	}
	

	// search the cache for a pre-built list of leaves and nodes that matches
	// the desired vis setup, and use that to mark the map if found
	for (i = viscache.Head(); i != viscache.InvalidIndex(); i = viscache.Next(i))
	{
		VisCacheEntry &cache = viscache[i];
		if (cache.nClusters != vis.nClusters) continue;
		for (c = 0; c < cache.nClusters; ++c)
		{
			if (cache.originclusters[c] != vis.rgVisClusters[c].viewcluster) 
			{
				// NJS: This is a nasty goto, but avoids a nasty branch mispredict below 
				// (if a break and if are used instead)
				goto next_cache_check;
			}
		}

		viscache.LinkToHead( i );
		VisMark_Cached( cache, *worldmodel->brush.pShared );

		return;

next_cache_check:;
	}

	// if we get here, we need to update the cache with a new entry
	if (viscache.Count() < VISCACHE_SIZE)
	{
		viscache.AddToHead();
	}
	else
	{
		viscache.LinkToHead( viscache.Tail() );
	}

	// this also will mark the visleafs in order to build the cache data
	VisCache_Build( viscache[viscache.Head()], *worldmodel->brush.pShared );
}

//-----------------------------------------------------------------------------
// Purpose: Purpose: Setup vis for the specified map
// Input  : *worldmodel - the map
//			visorigincount - how many origins to merge together ( usually 1, can be 0 if forcenovis is true )
//			origins[][3] - array of origins to merge together
//			forcenovis - if set to true, ignore all origins and just mark everything as visible ( SLOW rendering!!! )
//-----------------------------------------------------------------------------
void Map_VisSetup( model_t *worldmodel, int visorigincount, const Vector origins[], bool forcenovis /*=false*/, unsigned int &returnFlags )
{
	assert( visorigincount <= MAX_VIS_LEAVES );

	// Don't crash if the client .dll tries to do something weird/dumb
	vis.nClusters = min( visorigincount, MAX_VIS_LEAVES );
	vis.bForceFullSky = false;
	vis.bSkyVisible = false;
	returnFlags = 0;
	for ( int i = 0; i < vis.nClusters; i++ )
	{
		int leafIndex = CM_PointLeafnum( origins[ i ] );
		int flags = CM_LeafFlags( leafIndex );
		if ( flags & ( LEAF_FLAGS_SKY | LEAF_FLAGS_SKY2D ) )
		{
			vis.bSkyVisible = true;
		}
		if ( flags & LEAF_FLAGS_RADIAL )
		{
			vis.bForceFullSky = true;
			returnFlags |= IVRenderView::VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS;
		}
		vis.rgVisClusters[ i ].viewcluster = CM_LeafCluster( leafIndex );
		VectorCopy( origins[ i ], vis.rgVisClusters[ i ].origin );
	}

	if ( !vis.bSkyVisible )
	{
		vis.bForceFullSky = false;
	}
	
	Map_VisMark( forcenovis, worldmodel );
}

//-----------------------------------------------------------------------------
// Purpose: Clear / reset vis data
//-----------------------------------------------------------------------------
void Map_VisClear( void )
{
	vis.nClusters = 1;
	vis.oldnClusters = 1;
	for ( int i = 0; i < MAX_VIS_LEAVES; i++ )
	{
		vis.rgVisClusters[ i ].oldviewcluster = -2;
		VectorClear( vis.rgVisClusters[ i ].origin );
		vis.rgVisClusters[ i ].viewcluster = -2;
	}
	viscache.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the current vis bitfield
// Output : byte
//-----------------------------------------------------------------------------
byte *Map_VisCurrent( void )
{
	return vis.rgCurrentVis;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the first viewcluster ( usually it's the only )
// Output : int
//-----------------------------------------------------------------------------
int Map_VisCurrentCluster( void )
{
	// BUGBUG: The client DLL can hit this assert during a level transition
	// because the temporary entities do visibility calculations during the 
	// wrong part of the frame loop (i.e. before a view has been set up!)
	Assert( vis.rgVisClusters[ 0 ].viewcluster >= 0 );
	if ( vis.rgVisClusters[ 0 ].viewcluster < 0 )
	{
		static int visclusterwarningcount = 0;

		if ( ++visclusterwarningcount <= 5 )
		{
			ConDMsg( "Map_VisCurrentCluster() < 0!\n" ); 
		}
	}
	return vis.rgVisClusters[ 0 ].viewcluster;
}

bool Map_VisSkyVisible()
{
	return vis.bSkyVisible;
}

bool Map_VisForceFullSky()
{
	return vis.bForceFullSky;
}
