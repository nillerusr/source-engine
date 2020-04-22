//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"
#include "sysexternal.h"
#include "cmodel_engine.h"
#include "dispcoll_common.h"
#include "modelloader.h"
#include "common.h"
#include "zone.h"

// UNDONE: Abstract the texture/material lookup stuff and all of this goes away
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
extern IMaterialSystem *materials;

#include "vphysics_interface.h"
#include "sys_dll.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int g_iServerGameDLLVersion;
IPhysicsSurfaceProps *physprop = NULL;
IPhysicsCollision	 *physcollision = NULL;

// local forward declarations
void CollisionBSPData_LoadTextures( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadTexinfo( CCollisionBSPData *pBSPData, CUtlVector<unsigned short> &map_texinfo );
void CollisionBSPData_LoadLeafs_Version_0( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadLeafs_Version_1( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadLeafs( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadLeafBrushes( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadPlanes( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadBrushes( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadBrushSides( CCollisionBSPData *pBSPData, CUtlVector<unsigned short> &map_texinfo );
void CollisionBSPData_LoadSubmodels( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadNodes( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadAreas( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadAreaPortals( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadVisibility( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadEntityString( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadPhysics( CCollisionBSPData *pBSPData );
void CollisionBSPData_LoadDispInfo( CCollisionBSPData *pBSPData );


//=============================================================================
//
// Initialization/Destruction Functions
//


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CollisionBSPData_Init( CCollisionBSPData *pBSPData )
{
	pBSPData->numleafs = 1;
	pBSPData->map_vis = NULL;
	pBSPData->numareas = 1;
	pBSPData->numclusters = 1;
	pBSPData->map_nullname = "**empty**";
	pBSPData->numtextures = 0;

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_Destroy( CCollisionBSPData *pBSPData )
{
	for ( int i = 0; i < pBSPData->numcmodels; i++ )
	{
		physcollision->VCollideUnload( &pBSPData->map_cmodels[i].vcollisionData );
	}

	// free displacement data
	DispCollTrees_FreeLeafList( pBSPData );
	CM_DestroyDispPhysCollide();
	DispCollTrees_Free( g_pDispCollTrees );
	g_pDispCollTrees = NULL;
	g_pDispBounds = NULL;
	g_DispCollTreeCount = 0;

	if ( pBSPData->map_planes.Base() )
	{
		pBSPData->map_planes.Detach();
	}

	if ( pBSPData->map_texturenames )
	{
		pBSPData->map_texturenames = NULL;
	}

	if ( pBSPData->map_surfaces.Base() )
	{
		pBSPData->map_surfaces.Detach();
	}

	if ( pBSPData->map_areaportals.Base() )
	{
		pBSPData->map_areaportals.Detach();
	}

	if ( pBSPData->portalopen.Base() )
	{
		pBSPData->portalopen.Detach();
	}

	if ( pBSPData->map_areas.Base() )
	{
		pBSPData->map_areas.Detach();
	}

	pBSPData->map_entitystring.Discard();

	if ( pBSPData->map_brushes.Base() )
	{
		pBSPData->map_brushes.Detach();
	}

	if ( pBSPData->map_dispList.Base() )
	{
		pBSPData->map_dispList.Detach();
	}

	if ( pBSPData->map_cmodels.Base() )
	{
		pBSPData->map_cmodels.Detach();
	}

	if ( pBSPData->map_leafbrushes.Base() )
	{
		pBSPData->map_leafbrushes.Detach();
	}

	if ( pBSPData->map_leafs.Base() )
	{
		pBSPData->map_leafs.Detach();
	}

	if ( pBSPData->map_nodes.Base() )
	{
		pBSPData->map_nodes.Detach();
	}

	if ( pBSPData->map_brushsides.Base() )
	{
		pBSPData->map_brushsides.Detach();
	}

	if ( pBSPData->map_vis )
	{
		pBSPData->map_vis = NULL;
	}

	pBSPData->numplanes = 0;
	pBSPData->numbrushsides = 0;
	pBSPData->emptyleaf = pBSPData->solidleaf =0;
	pBSPData->numnodes = 0;
	pBSPData->numleafs = 0;
	pBSPData->numbrushes = 0;
	pBSPData->numdisplist = 0;
	pBSPData->numleafbrushes = 0;
	pBSPData->numareas = 0;
	pBSPData->numtextures = 0;
	pBSPData->floodvalid = 0;
	pBSPData->numareaportals = 0;
	pBSPData->numclusters = 0;
	pBSPData->numcmodels = 0;
	pBSPData->numvisibility = 0;
	pBSPData->numentitychars = 0;
	pBSPData->numportalopen = 0;
	pBSPData->map_name[0] = 0;
	pBSPData->map_rootnode = NULL;
}

//-----------------------------------------------------------------------------
// Returns the collision tree associated with the ith displacement
//-----------------------------------------------------------------------------

CDispCollTree* CollisionBSPData_GetCollisionTree( int i )
{
	if ((i < 0) || (i >= g_DispCollTreeCount))
		return 0;

	return &g_pDispCollTrees[i];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LinkPhysics( void )
{
	//
	// initialize the physics surface properties -- if necessary!
	//
	if( !physprop )
	{
		physprop = ( IPhysicsSurfaceProps* )g_AppSystemFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL );
		physcollision = ( IPhysicsCollision* )g_AppSystemFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );

		if ( !physprop || !physcollision )
		{
			Sys_Error( "CollisionBSPData_PreLoad: Can't link physics" );
		}
	}
}


//=============================================================================
//
// Loading Functions
//

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_PreLoad( CCollisionBSPData *pBSPData )
{
	// initialize the collision bsp data
	CollisionBSPData_Init( pBSPData ); 
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CollisionBSPData_Load( const char *pName, CCollisionBSPData *pBSPData )
{
	// This is a table that maps texinfo references to csurface_t
	// It is freed after the map has been loaded
	CUtlVector<unsigned short> 	map_texinfo;

	// copy map name
	Q_strncpy( pBSPData->map_name, pName, sizeof( pBSPData->map_name ) );

	//
	// load bsp file data
	//
	COM_TimestampedLog( "  CollisionBSPData_LoadTextures" );
	CollisionBSPData_LoadTextures( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadTexinfo" );
	CollisionBSPData_LoadTexinfo( pBSPData, map_texinfo );

	COM_TimestampedLog( "  CollisionBSPData_LoadLeafs" );
	CollisionBSPData_LoadLeafs( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadLeafBrushes" );
	CollisionBSPData_LoadLeafBrushes( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadPlanes" );
	CollisionBSPData_LoadPlanes( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadBrushes" );
	CollisionBSPData_LoadBrushes( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadBrushSides" );
	CollisionBSPData_LoadBrushSides( pBSPData, map_texinfo );

	COM_TimestampedLog( "  CollisionBSPData_LoadSubmodels" );
	CollisionBSPData_LoadSubmodels( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadPlanes" );
	CollisionBSPData_LoadNodes( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadAreas" );
	CollisionBSPData_LoadAreas( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadAreaPortals" );
	CollisionBSPData_LoadAreaPortals( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadVisibility" );
	CollisionBSPData_LoadVisibility( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadEntityString" );
	CollisionBSPData_LoadEntityString( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadPhysics" );
	CollisionBSPData_LoadPhysics( pBSPData );

	COM_TimestampedLog( "  CollisionBSPData_LoadDispInfo" );
    CollisionBSPData_LoadDispInfo( pBSPData );

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadTextures( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_TEXDATA );

	CMapLoadHelper lhStringData( LUMP_TEXDATA_STRING_DATA );
	const char *pStringData = ( const char * )lhStringData.LumpBase();

	CMapLoadHelper lhStringTable( LUMP_TEXDATA_STRING_TABLE );
	if( lhStringTable.LumpSize() % sizeof( int ) )
		Sys_Error( "CMod_LoadTextures: funny lump size");
	int *pStringTable = ( int * )lhStringTable.LumpBase();

	dtexdata_t	*in;
	int			i, count;
	IMaterial	*material;

	in = (dtexdata_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CMod_LoadTextures: funny lump size");
	}
	count = lh.LumpSize() / sizeof(*in);
	if (count < 1)
	{
		Sys_Error( "Map with no textures");
	}
	if (count > MAX_MAP_TEXDATA)
	{
		Sys_Error( "Map has too many textures");
	}

	int nSize = count * sizeof(csurface_t);
	pBSPData->map_surfaces.Attach( count, (csurface_t*)Hunk_Alloc( nSize ) );

	pBSPData->numtextures = count;

	pBSPData->map_texturenames = (char *)Hunk_Alloc( lhStringData.LumpSize() * sizeof(char), false );
	memcpy( pBSPData->map_texturenames, pStringData, lhStringData.LumpSize() );
 
	for ( i=0 ; i<count ; i++, in++ )
	{
		Assert( in->nameStringTableID >= 0 );
		Assert( pStringTable[in->nameStringTableID] >= 0 );

		const char *pInName = &pStringData[pStringTable[in->nameStringTableID]];
		int index = pInName - pStringData;
		
		csurface_t *out = &pBSPData->map_surfaces[i];
		out->name = &pBSPData->map_texturenames[index];
		out->surfaceProps = 0;
		out->flags = 0;

		material = materials->FindMaterial( pBSPData->map_surfaces[i].name, TEXTURE_GROUP_WORLD, true );
		if ( !IsErrorMaterial( material ) )
		{
			IMaterialVar *var;
			bool varFound;
			var = material->FindVar( "$surfaceprop", &varFound, false );
			if ( varFound )
			{
				const char *pProps = var->GetStringValue();
				pBSPData->map_surfaces[i].surfaceProps = physprop->GetSurfaceIndex( pProps );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadTexinfo( CCollisionBSPData *pBSPData, 
									CUtlVector<unsigned short> &map_texinfo )
{
	CMapLoadHelper lh( LUMP_TEXINFO );

	texinfo_t	*in;
	unsigned short	out;
	int			i, count;

	in = (texinfo_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Sys_Error( "CollisionBSPData_LoadTexinfo: funny lump size");
	count = lh.LumpSize() / sizeof(*in);
	if (count < 1)
		Sys_Error( "Map with no texinfo");
	if (count > MAX_MAP_TEXINFO)
		Sys_Error( "Map has too many surfaces");

	MEM_ALLOC_CREDIT();
	map_texinfo.RemoveAll();
	map_texinfo.EnsureCapacity( count );

	for ( i=0 ; i<count ; i++, in++ )
	{
		out = in->texdata;
		
		if ( out >= pBSPData->numtextures )
			out = 0;

		// HACKHACK: Copy this over for the whole material!!!
		pBSPData->map_surfaces[out].flags |= in->flags;
		map_texinfo.AddToTail(out);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadLeafs_Version_0( CCollisionBSPData *pBSPData, CMapLoadHelper &lh )
{
	int			i;
	dleaf_version_0_t 	*in;
	int			count;
	
	in = (dleaf_version_0_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CollisionBSPData_LoadLeafs: funny lump size");
	}

	count = lh.LumpSize() / sizeof(*in);

	if (count < 1)
	{
		Sys_Error( "Map with no leafs");
	}

	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
	{
		Sys_Error( "Map has too many planes");
	}

	// Need an extra one for the emptyleaf below
	int nSize = (count + 1) * sizeof(cleaf_t);
	pBSPData->map_leafs.Attach( count + 1, (cleaf_t*)Hunk_Alloc( nSize ) );

	pBSPData->numleafs = count;
	pBSPData->numclusters = 0;

	for ( i=0 ; i<count ; i++, in++ )
	{
		cleaf_t	*out = &pBSPData->map_leafs[i];	
		out->contents = in->contents;
		out->cluster = in->cluster;
		out->area = in->area;
		out->flags = in->flags;
		out->firstleafbrush = in->firstleafbrush;
		out->numleafbrushes = in->numleafbrushes;

		out->dispCount = 0;

		if (out->cluster >= pBSPData->numclusters)
		{
			pBSPData->numclusters = out->cluster + 1;
		}
	}

	if (pBSPData->map_leafs[0].contents != CONTENTS_SOLID)
	{
		Sys_Error( "Map leaf 0 is not CONTENTS_SOLID");
	}

	pBSPData->solidleaf = 0;
	pBSPData->emptyleaf = pBSPData->numleafs;
	memset( &pBSPData->map_leafs[pBSPData->emptyleaf], 0, sizeof(pBSPData->map_leafs[pBSPData->emptyleaf]) );
	pBSPData->numleafs++;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadLeafs_Version_1( CCollisionBSPData *pBSPData, CMapLoadHelper &lh )
{
	int			i;
	dleaf_t 	*in;
	int			count;
	
	in = (dleaf_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CollisionBSPData_LoadLeafs: funny lump size");
	}

	count = lh.LumpSize() / sizeof(*in);

	if (count < 1)
	{
		Sys_Error( "Map with no leafs");
	}

	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
	{
		Sys_Error( "Map has too many planes");
	}

	// Need an extra one for the emptyleaf below
	int nSize = (count + 1) * sizeof(cleaf_t);
	pBSPData->map_leafs.Attach( count + 1, (cleaf_t*)Hunk_Alloc( nSize ) );

	pBSPData->numleafs = count;
	pBSPData->numclusters = 0;

	for ( i=0 ; i<count ; i++, in++ )
	{
		cleaf_t	*out = &pBSPData->map_leafs[i];	
		out->contents = in->contents;
		out->cluster = in->cluster;
		out->area = in->area;
		out->flags = in->flags;
		out->firstleafbrush = in->firstleafbrush;
		out->numleafbrushes = in->numleafbrushes;

		out->dispCount = 0;

		if (out->cluster >= pBSPData->numclusters)
		{
			pBSPData->numclusters = out->cluster + 1;
		}
	}

	if (pBSPData->map_leafs[0].contents != CONTENTS_SOLID)
	{
		Sys_Error( "Map leaf 0 is not CONTENTS_SOLID");
	}

	pBSPData->solidleaf = 0;
	pBSPData->emptyleaf = pBSPData->numleafs;
	memset( &pBSPData->map_leafs[pBSPData->emptyleaf], 0, sizeof(pBSPData->map_leafs[pBSPData->emptyleaf]) );
	pBSPData->numleafs++;
}

void CollisionBSPData_LoadLeafs( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_LEAFS );
	switch( lh.LumpVersion() )
	{
	case 0:
		CollisionBSPData_LoadLeafs_Version_0( pBSPData, lh );
		break;
	case 1:
		CollisionBSPData_LoadLeafs_Version_1( pBSPData, lh );
		break;
	default:
		Assert( 0 );
		Error( "Unknown LUMP_LEAFS version\n" );
		break;
	}

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadLeafBrushes( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_LEAFBRUSHES );

	int			i;
	unsigned short 	*in;
	int			count;
	
	in = (unsigned short *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CMod_LoadLeafBrushes: funny lump size");
	}

	count = lh.LumpSize() / sizeof(*in);
	if (count < 1)
	{
		Sys_Error( "Map with no planes");
	}

	// need to save space for box planes
	if (count > MAX_MAP_LEAFBRUSHES)
	{
		Sys_Error( "Map has too many leafbrushes");
	}

	pBSPData->map_leafbrushes.Attach( count, (unsigned short*)Hunk_Alloc( count * sizeof(unsigned short), false ) );
	pBSPData->numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		pBSPData->map_leafbrushes[i] = *in;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadPlanes( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_PLANES );

	int			i, j;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (dplane_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CollisionBSPData_LoadPlanes: funny lump size");
	}

	count = lh.LumpSize() / sizeof(*in);

	if (count < 1)
	{
		Sys_Error( "Map with no planes");
	}

	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
	{
		Sys_Error( "Map has too many planes");
	}

	int nSize = count * sizeof(cplane_t);
	pBSPData->map_planes.Attach( count, (cplane_t*)Hunk_Alloc( nSize ) );

	pBSPData->numplanes = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		cplane_t *out = &pBSPData->map_planes[i];	
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = in->normal[j];
			if (out->normal[j] < 0)
			{
				bits |= 1<<j;
			}
		}

		out->dist = in->dist;
		out->type = in->type;
		out->signbits = bits;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadBrushes( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_BRUSHES );

	dbrush_t	*in;
	int			i, count;
	
	in = (dbrush_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CMod_LoadBrushes: funny lump size");
	}

	count = lh.LumpSize() / sizeof(*in);
	if (count > MAX_MAP_BRUSHES)
	{
		Sys_Error( "Map has too many brushes");
	}

	int nSize = count * sizeof(cbrush_t);
	pBSPData->map_brushes.Attach( count, (cbrush_t*)Hunk_Alloc( nSize ) );

	pBSPData->numbrushes = count;

	for (i=0 ; i<count ; i++, in++)
	{
		cbrush_t *out = &pBSPData->map_brushes[i];
		out->firstbrushside = in->firstside;
		out->numsides = in->numsides;
		out->contents = in->contents;
	}
}

inline bool IsBoxBrush( const cbrush_t &brush, dbrushside_t *pSides, cplane_t *pPlanes )
{
	int countAxial = 0;
	if ( brush.numsides == 6 )
	{
		for ( int i = 0; i < brush.numsides; i++ )
		{
			cplane_t *plane = pPlanes + pSides[brush.firstbrushside+i].planenum;
			if ( plane->type > PLANE_Z )
				break;
			countAxial++;
		}
	}
	return (countAxial == brush.numsides) ? true : false;
}

inline void ExtractBoxBrush( cboxbrush_t *pBox, const cbrush_t &brush, dbrushside_t *pSides, cplane_t *pPlanes, CUtlVector<unsigned short> &map_texinfo )
{
	// brush.numsides is no longer valid.  Assume numsides == 6
	for ( int i = 0; i < 6; i++ )
	{
		dbrushside_t *side = pSides + i + brush.firstbrushside;
		cplane_t *plane = pPlanes + side->planenum;
		int t = side->texinfo;
		Assert(t<map_texinfo.Count());
		int surfaceIndex = (t<0) ? SURFACE_INDEX_INVALID : map_texinfo[t];
		int axis = plane->type;
		Assert(fabs(plane->normal[axis])==1.0f);
		if ( plane->normal[axis] == 1.0f )
		{
			pBox->maxs[axis] = plane->dist;
			pBox->surfaceIndex[axis+3] = surfaceIndex;
		}
		else if ( plane->normal[axis] == -1.0f )
		{
			pBox->mins[axis] = -plane->dist;
			pBox->surfaceIndex[axis] = surfaceIndex;
		}
		else
		{
			Assert(0);
		}
	}
	pBox->pad2[0] = 0;
	pBox->pad2[1] = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadBrushSides( CCollisionBSPData *pBSPData, CUtlVector<unsigned short> &map_texinfo )
{
	CMapLoadHelper lh( LUMP_BRUSHSIDES );

	int				i, j;
	dbrushside_t 	*in;

	in = (dbrushside_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CMod_LoadBrushSides: funny lump size");
	}

	int inputSideCount = lh.LumpSize() / sizeof(*in);

	// need to save space for box planes
	if (inputSideCount > MAX_MAP_BRUSHSIDES)
	{
		Sys_Error( "Map has too many planes");
	}


	// Brushes are compressed on load to remove any AABB brushes.  The brushsides for those are removed
	// and those brushes are stored as cboxbrush_t.  But the texinfo/surface data needs to be copied
	// So the algorithm is:
	//
	// count box brushes
	// count total brush sides
	// allocate
	// iterate brushes and copy sides or fill out box brushes
	// done
	//

	int boxBrushCount = 0;
	int brushSideCount = 0;
	for ( i = 0; i < pBSPData->numbrushes; i++ )
	{
		if ( IsBoxBrush(pBSPData->map_brushes[i], in, pBSPData->map_planes.Base()) )
		{
			// mark as axial
			pBSPData->map_brushes[i].numsides = NUMSIDES_BOXBRUSH;
			boxBrushCount++;
		}
		else
		{
			brushSideCount += pBSPData->map_brushes[i].numsides;
		}
	}

	int nSize = brushSideCount * sizeof(cbrushside_t);
	pBSPData->map_brushsides.Attach( brushSideCount, (cbrushside_t*)Hunk_Alloc( nSize, false ) );
	pBSPData->map_boxbrushes.Attach( boxBrushCount, (cboxbrush_t*)Hunk_Alloc( boxBrushCount*sizeof(cboxbrush_t), false ) );

	pBSPData->numbrushsides = brushSideCount;
	pBSPData->numboxbrushes = boxBrushCount;

	int outBoxBrush = 0;
	int outBrushSide = 0;
	for ( i = 0; i < pBSPData->numbrushes; i++ )
	{
		cbrush_t *pBrush = &pBSPData->map_brushes[i];

		if ( pBrush->IsBox() )
		{
			// fill out the box brush - extract from the input sides
			cboxbrush_t *pBox = &pBSPData->map_boxbrushes[outBoxBrush];
			ExtractBoxBrush( pBox, *pBrush, in, pBSPData->map_planes.Base(), map_texinfo );
			pBrush->SetBox(outBoxBrush);
			outBoxBrush++;
		}
		else
		{
			// copy each side into the output array
			int firstInputSide = pBrush->firstbrushside;
			pBrush->firstbrushside = outBrushSide;
			for ( j = 0; j < pBrush->numsides; j++ )
			{
				cbrushside_t * RESTRICT pSide = &pBSPData->map_brushsides[outBrushSide];
				dbrushside_t * RESTRICT pInputSide = in + firstInputSide + j;
				pSide->plane = &pBSPData->map_planes[pInputSide->planenum];
				int t = pInputSide->texinfo;
				if (t >= map_texinfo.Size())
				{
					Sys_Error( "Bad brushside texinfo");
				}

				// BUGBUG: Why is vbsp writing out -1 as the texinfo id?  (TEXINFO_NODE ?)
				pSide->surfaceIndex = (t < 0) ? SURFACE_INDEX_INVALID : map_texinfo[t];
				pSide->bBevel = pInputSide->bevel ? true : false;
				outBrushSide++;
			}
		}
	}
	Assert( outBrushSide == pBSPData->numbrushsides && outBoxBrush == pBSPData->numboxbrushes );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadSubmodels( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_MODELS );

	dmodel_t	*in;
	int			i, j, count;

	in = (dmodel_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Sys_Error("CMod_LoadSubmodels: funny lump size");
	count = lh.LumpSize() / sizeof(*in);

	if (count < 1)
		Sys_Error( "Map with no models" );
	if (count > MAX_MAP_MODELS)
		Sys_Error( "Map has too many models" );

	int nSize = count * sizeof(cmodel_t);
	pBSPData->map_cmodels.Attach( count, (cmodel_t*)Hunk_Alloc( nSize ) );
	pBSPData->numcmodels = count;

	for ( i=0 ; i<count ; i++, in++ )
	{
		cmodel_t *out = &pBSPData->map_cmodels[i];

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j] - 1;
			out->maxs[j] = in->maxs[j] + 1;
			out->origin[j] = in->origin[j];
		}
		out->headnode = in->headnode;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadNodes( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_NODES );

	dnode_t		*in;
	int			i, j, count;
	
	in = (dnode_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Sys_Error( "CollisionBSPData_LoadNodes: funny lump size");
	count = lh.LumpSize() / sizeof(*in);

	if (count < 1)
		Sys_Error( "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Sys_Error( "Map has too many nodes");

	// 6 extra for box hull
	int nSize = ( count + 6 ) * sizeof(cnode_t);
	pBSPData->map_nodes.Attach( count + 6, (cnode_t*)Hunk_Alloc( nSize ) );

	pBSPData->numnodes = count;
	pBSPData->map_rootnode = pBSPData->map_nodes.Base();

	for (i=0; i<count; i++, in++)
	{
		cnode_t	*out = &pBSPData->map_nodes[i];
		out->plane = &pBSPData->map_planes[ in->planenum ];
		for (j=0; j<2; j++)
		{
			out->children[j] = in->children[j];
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadAreas( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_AREAS );

	int			i;
	darea_t 	*in;
	int			count;

	in = (darea_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CMod_LoadAreas: funny lump size");
	}

	count = lh.LumpSize() / sizeof(*in);
	if (count > MAX_MAP_AREAS)
	{
		Sys_Error( "Map has too many areas");
	}

	int nSize = count * sizeof(carea_t);
	pBSPData->map_areas.Attach( count, (carea_t*)Hunk_Alloc( nSize ) );

	pBSPData->numareas = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		carea_t	*out = &pBSPData->map_areas[i];
		out->numareaportals = in->numareaportals;
		out->firstareaportal = in->firstareaportal;
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadAreaPortals( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_AREAPORTALS );

	dareaportal_t 	*in;
	int				count;

	in = (dareaportal_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
	{
		Sys_Error( "CMod_LoadAreaPortals: funny lump size");
	}
		   
	count = lh.LumpSize() / sizeof(*in);
	if (count > MAX_MAP_AREAPORTALS)
	{
		Sys_Error( "Map has too many area portals");
	}

	// Need to add one more in owing to 1-based instead of 0-based data!
	++count;

	pBSPData->numportalopen = count;
	pBSPData->portalopen.Attach( count, (bool*)Hunk_Alloc( pBSPData->numportalopen * sizeof(bool), false ) );
	for ( int i=0; i < pBSPData->numportalopen; i++ )
	{
		pBSPData->portalopen[i] = false;
	}

	pBSPData->numareaportals = count;
	int nSize = count * sizeof(dareaportal_t);
	pBSPData->map_areaportals.Attach( count, (dareaportal_t*)Hunk_Alloc( nSize ) );

	Assert( nSize >= lh.LumpSize() ); 
	memcpy( pBSPData->map_areaportals.Base(), in, lh.LumpSize() );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadVisibility( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_VISIBILITY );

	pBSPData->numvisibility = lh.LumpSize();
	if (lh.LumpSize() > MAX_MAP_VISIBILITY)
		Sys_Error( "Map has too large visibility lump");

	int visDataSize = lh.LumpSize();
	if ( visDataSize == 0 )
	{
		pBSPData->map_vis = NULL;
	}
	else
	{
		pBSPData->map_vis = (dvis_t *) Hunk_Alloc( visDataSize, false );
		memcpy( pBSPData->map_vis, lh.LumpBase(), visDataSize );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadEntityString( CCollisionBSPData *pBSPData )
{
	CMapLoadHelper lh( LUMP_ENTITIES );

	pBSPData->numentitychars = lh.LumpSize();
	MEM_ALLOC_CREDIT();
	char szMapName[MAX_PATH] = { 0 };
	V_strncpy( szMapName, lh.GetMapName(), sizeof( szMapName ) );
	pBSPData->map_entitystring.Init( szMapName, lh.LumpOffset(), lh.LumpSize(), lh.LumpBase() );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadPhysics( CCollisionBSPData *pBSPData )
{
#ifdef _WIN32
	CMapLoadHelper lh( LUMP_PHYSCOLLIDE );
#else
	int nLoadLump = LUMP_PHYSCOLLIDE;
	// backwards compat support for older game dlls
	// they crash if they don't have vcollide data for terrain 
	// even though the new engine ignores it
	if ( g_iServerGameDLLVersion >= 5 )
	{
		// if there's a linux lump present, go ahead and load it
		// otherwise, the win32 lump will work as long as it doesn't have any
		// mopp surfaces in it (if compiled with the current vbsp.exe or a map without any displacements).  
		// The legacy server game DLLs will crash when mopps are present but since 
		// they also crash with a NULL lump there's nothing lost in that case.
		if ( CMapLoadHelper::LumpSize(LUMP_PHYSCOLLIDESURFACE) > 0 )
		{
			nLoadLump = LUMP_PHYSCOLLIDESURFACE;
		}
		else
		{
			DevWarning("Legacy game DLL may not support terrain vphysics collisions with this BSP!\n");
		}
	}

	CMapLoadHelper lh( nLoadLump );
#endif
	if ( !lh.LumpSize() )
		return;

	byte *ptr = lh.LumpBase();
	byte *basePtr = ptr;

	dphysmodel_t physModel;

	// physics data is variable length.  The last physmodel is a NULL pointer
	// with modelIndex -1, dataSize -1
	do
	{
		memcpy( &physModel, ptr, sizeof(physModel) );
		ptr += sizeof(physModel);

		if ( physModel.dataSize > 0 )
		{
			cmodel_t *pModel = &pBSPData->map_cmodels[ physModel.modelIndex ];
			physcollision->VCollideLoad( &pModel->vcollisionData, physModel.solidCount, (const char *)ptr, physModel.dataSize + physModel.keydataSize );
			ptr += physModel.dataSize;
			ptr += physModel.keydataSize;
		}
		
		// avoid infinite loop on badly formed file
		if ( (int)(ptr - basePtr) > lh.LumpSize() )
			break;

	} while ( physModel.dataSize > 0 );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionBSPData_LoadDispInfo( CCollisionBSPData *pBSPData )
{
	// How many displacements in the map?
	int coreDispCount = CMapLoadHelper::LumpSize( LUMP_DISPINFO ) / sizeof( ddispinfo_t );
	if ( coreDispCount == 0 )
		return;	

    //
    // get the vertex data
    //
 	CMapLoadHelper lhv( LUMP_VERTEXES );
	dvertex_t *pVerts = ( dvertex_t* )lhv.LumpBase();
	if ( lhv.LumpSize() % sizeof( dvertex_t ) )
		Sys_Error( "CMod_LoadDispInfo: bad vertex lump size!" );

    //
    // get the edge data
    //
 	CMapLoadHelper lhe( LUMP_EDGES );
    dedge_t *pEdges = ( dedge_t* )lhe.LumpBase();
    if ( lhe.LumpSize() % sizeof( dedge_t ) )
        Sys_Error( "CMod_LoadDispInfo: bad edge lump size!" );

    //
    // get surf edges data
    //
 	CMapLoadHelper lhs( LUMP_SURFEDGES );
    int *pSurfEdges = ( int* )lhs.LumpBase();
    if ( lhs.LumpSize() % sizeof( int ) )
        Sys_Error( "CMod_LoadDispInfo: bad surf edge lump size!" );

    //
    // get face data
    //
	int face_lump_to_load = LUMP_FACES;
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
		CMapLoadHelper::LumpSize( LUMP_FACES_HDR ) > 0 )
	{
		face_lump_to_load = LUMP_FACES_HDR;
	}
	CMapLoadHelper lhf( face_lump_to_load );
    dface_t *pFaces = ( dface_t* )lhf.LumpBase();
    if ( lhf.LumpSize() % sizeof( dface_t ) )
        Sys_Error( "CMod_LoadDispInfo: bad face lump size!" );
    int faceCount = lhf.LumpSize() / sizeof( dface_t );

	dface_t *pFaceList = pFaces;
	if ( !pFaceList )
		return;

    //
    // get texinfo data
    //
 	CMapLoadHelper lhti( LUMP_TEXINFO );
    texinfo_t *pTexinfoList = ( texinfo_t* )lhti.LumpBase();
    if ( lhti.LumpSize() % sizeof( texinfo_t ) )
        Sys_Error( "CMod_LoadDispInfo: bad texinfo lump size!" );

	// allocate displacement collision trees
	g_DispCollTreeCount = coreDispCount;
	g_pDispCollTrees = DispCollTrees_Alloc( g_DispCollTreeCount );
	g_pDispBounds = (alignedbbox_t *)Hunk_Alloc( g_DispCollTreeCount * sizeof(alignedbbox_t), false );

	// Build the inverse mapping from disp index to face
	int nMemSize = coreDispCount * sizeof(unsigned short);
	unsigned short *pDispIndexToFaceIndex = (unsigned short*)stackalloc( nMemSize );
	memset( pDispIndexToFaceIndex, 0xFF, nMemSize );
	
	int i;
    for ( i = 0; i < faceCount; ++i, ++pFaces )
    {
        // check face for displacement data
        if ( pFaces->dispinfo == -1 )
            continue;

        // get the current displacement build surface
		if ( pFaces->dispinfo >= coreDispCount )
			continue;

		pDispIndexToFaceIndex[pFaces->dispinfo] = (unsigned short)i;
    }

	// Load one dispinfo from disk at a time and set it up.
	int iCurVert = 0;
	int iCurTri = 0;
	CDispVert tempVerts[MAX_DISPVERTS];
	CDispTri  tempTris[MAX_DISPTRIS];

	int nSize = 0;
	int nCacheSize = 0;
	int nPowerCount[3] = { 0, 0, 0 };

	CMapLoadHelper lhDispInfo( LUMP_DISPINFO );
	CMapLoadHelper lhDispVerts( LUMP_DISP_VERTS );
	CMapLoadHelper lhDispTris( LUMP_DISP_TRIS );

	for ( i = 0; i < coreDispCount; ++i )
	{
		// Find the face associated with this dispinfo
		unsigned short nFaceIndex = pDispIndexToFaceIndex[i];
		if ( nFaceIndex == 0xFFFF )
			continue;

		// Load up the dispinfo and create the CCoreDispInfo from it.
		ddispinfo_t dispInfo;
		lhDispInfo.LoadLumpElement( i, sizeof(ddispinfo_t), &dispInfo );

		// Read in the vertices.
		int nVerts = NUM_DISP_POWER_VERTS( dispInfo.power );
		lhDispVerts.LoadLumpData( iCurVert * sizeof(CDispVert), nVerts*sizeof(CDispVert), tempVerts );
		iCurVert += nVerts;
		
		// Read in the tris.
		int nTris = NUM_DISP_POWER_TRIS( dispInfo.power );
		lhDispTris.LoadLumpData( iCurTri * sizeof( CDispTri ), nTris*sizeof( CDispTri), tempTris );
		iCurTri += nTris;

		CCoreDispInfo coreDisp;
		CCoreDispSurface *pDispSurf = coreDisp.GetSurface();
		pDispSurf->SetPointStart( dispInfo.startPosition );
		pDispSurf->SetContents( dispInfo.contents );
	
		coreDisp.InitDispInfo( dispInfo.power, dispInfo.minTess, dispInfo.smoothingAngle, tempVerts, tempTris );

		// Hook the disp surface to the face
		pFaces = &pFaceList[ nFaceIndex ];
		pDispSurf->SetHandle( nFaceIndex );

		// get points
		if ( pFaces->numedges > 4 )
			continue;

		Vector surfPoints[4];
		pDispSurf->SetPointCount( pFaces->numedges );
		int j;
		for ( j = 0; j < pFaces->numedges; j++ )
		{
			int eIndex = pSurfEdges[pFaces->firstedge+j];
			if ( eIndex < 0 )
			{
				VectorCopy( pVerts[pEdges[-eIndex].v[1]].point, surfPoints[j] );
			}
			else
			{
				VectorCopy( pVerts[pEdges[eIndex].v[0]].point, surfPoints[j] );
			}
		}

		for ( j = 0; j < 4; j++ )
		{
			pDispSurf->SetPoint( j, surfPoints[j] );
		}

		pDispSurf->FindSurfPointStartIndex();
		pDispSurf->AdjustSurfPointData();

		//
		// generate the collision displacement surfaces
		//
		CDispCollTree *pDispTree = &g_pDispCollTrees[i];
		pDispTree->SetPower( 0 );

		//
		// check for null faces, should have been taken care of in vbsp!!!
		//
		int pointCount = pDispSurf->GetPointCount();
		if ( pointCount != 4 )
			continue;

		coreDisp.Create();

		// new collision
		pDispTree->Create( &coreDisp );
		g_pDispBounds[i].Init(pDispTree->m_mins, pDispTree->m_maxs, pDispTree->m_iCounter, pDispTree->GetContents());
		nSize += pDispTree->GetMemorySize();
		nCacheSize += pDispTree->GetCacheMemorySize();
		nPowerCount[pDispTree->GetPower()-2]++;

		// Surface props.
		texinfo_t *pTex = &pTexinfoList[pFaces->texinfo];
		if ( pTex->texdata >= 0 )
		{
			IMaterial *pMaterial = materials->FindMaterial( pBSPData->map_surfaces[pTex->texdata].name, TEXTURE_GROUP_WORLD, true );
			if ( !IsErrorMaterial( pMaterial ) )
			{
				IMaterialVar *pVar;
				bool bVarFound;
				pVar = pMaterial->FindVar( "$surfaceprop", &bVarFound, false );
				if ( bVarFound )
				{
					const char *pProps = pVar->GetStringValue();
					pDispTree->SetSurfaceProps( 0, physprop->GetSurfaceIndex( pProps ) );
					pDispTree->SetSurfaceProps( 1, physprop->GetSurfaceIndex( pProps ) );
				}

				pVar = pMaterial->FindVar( "$surfaceprop2", &bVarFound, false );
				if ( bVarFound )
				{
					const char *pProps = pVar->GetStringValue();
					pDispTree->SetSurfaceProps( 1, physprop->GetSurfaceIndex( pProps ) );
				}
			}
		}
	}

	CMapLoadHelper lhDispPhys( LUMP_PHYSDISP );
	dphysdisp_t *pDispPhys = (dphysdisp_t *)lhDispPhys.LumpBase();
	// create the vphysics collision models for each displacement
	CM_CreateDispPhysCollide( pDispPhys, lhDispPhys.LumpSize() );
}


//=============================================================================
//
// Collision Count Functions
//

#ifdef COUNT_COLLISIONS
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CollisionCounts_Init( CCollisionCounts *pCounts )
{
	pCounts->m_PointContents = 0;
	pCounts->m_Traces = 0;
	pCounts->m_BrushTraces = 0;
	pCounts->m_DispTraces = 0;
	pCounts->m_Stabs = 0;
}
#endif
