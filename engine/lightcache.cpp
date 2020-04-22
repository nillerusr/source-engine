//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "quakedef.h"
#include "lightcache.h"
#include "cmodel_engine.h"
#include "istudiorender.h"
#include "studio_internal.h"
#include "bspfile.h"
#include "cdll_engine_int.h"
#include "tier1/mempool.h"
#include "gl_model_private.h"
#include "r_local.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "l_studio.h"
#include "debugoverlay.h"
#include "worldsize.h"
#include "ispatialpartitioninternal.h"
#include "staticpropmgr.h"
#include "cmodel_engine.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "enginetrace.h"
#include "client.h"
#include "cl_main.h"
#include "collisionutils.h"
#include "tier0/vprof.h"
#include "filesystem_engine.h"
#include "mathlib/anorms.h"
#include "gl_matsysiface.h"
#include "materialsystem/materialsystem_config.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// EMIT_SURFACE LIGHTS:
//
// Dim emit_surface lights go in the ambient cube because there are a ton of them and they are often so dim that 
// they get filtered out by r_worldlightmin.
//
// (Dim) emit_surface lights only get calculated at runtime for static props because static props
// do the full calculation of ambient lighting at runtime instead of using vrad's per-leaf
// calculation. Vrad's calculation includes the emit_surfaces, so if we're NOT using it, then
// we want to include emit_surface lights here.


// this should be prime to make the hash better
#define MAX_CACHE_ENTRY		200
#define MAX_CACHE_BUCKETS	MAX_CACHE_ENTRY

// number of bits per grid in x, y, z
#define HASH_GRID_SIZEX		5
#define	HASH_GRID_SIZEY		5
#define	HASH_GRID_SIZEZ		7

#define LIGHTCACHE_SNAP_EPSILON	0.5f

float Engine_WorldLightDistanceFalloff( const dworldlight_t *wl, const Vector& delta, bool bNoRadiusCheck = false );
float Engine_WorldLightAngle( const dworldlight_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta );

#define MAX_LIGHTSTYLE_BITS	 MAX_LIGHTSTYLES
#define MAX_LIGHTSTYLE_BYTES ( (MAX_LIGHTSTYLE_BITS + 7) / 8 )

static byte g_FrameMissCount = 0;
static int g_FrameIndex = 0;
ConVar lightcache_maxmiss("lightcache_maxmiss","2", FCVAR_CHEAT);

#define NUMRANDOMNORMALS	162
static Vector	s_raddir[NUMRANDOMNORMALS] = {
#include "randomnormals.h"
};

static ConVar r_lightcache_numambientsamples( "r_lightcache_numambientsamples", "162", FCVAR_CHEAT, 
											 "number of random directions to fire rays when computing ambient lighting",
											 true, 1.0f, true, ( float )NUMRANDOMNORMALS );

ConVar r_ambientlightingonly( 
	"r_ambientlightingonly", 
	"0", 
	FCVAR_CHEAT, 
	"Set this to 1 to light models with only ambient lighting (and no static lighting)." );

ConVar r_oldlightselection("r_oldlightselection", "0", FCVAR_CHEAT, "Set this to revert to HL2's method of selecting lights"); 

static void ComputeAmbientFromSphericalSamples( const Vector& start, 
						Vector* lightBoxColor );


//-----------------------------------------------------------------------------
// Cache used to compute which lightcache entries computed this frame
// may be able to be used temporarily for lighting other objects in the 
// case where we've got too many new lightcache samples in a single frame
//-----------------------------------------------------------------------------
struct CacheInfo_t
{
	int x;
	int y;
	int z;
	int leaf;
};


//-----------------------------------------------------------------------------
// Flags to pass into LightIntensityAndDirectionAtPoint + LightIntensityAndDirectionInBox
//-----------------------------------------------------------------------------
enum LightIntensityFlags_t
{
	LIGHT_NO_OCCLUSION_CHECK = 0x1,
	LIGHT_NO_RADIUS_CHECK = 0x2,
	LIGHT_OCCLUDE_VS_PROPS = 0x4,
	LIGHT_IGNORE_LIGHTSTYLE_VALUE = 0x8,
};


//-----------------------------------------------------------------------------
// Lightcache entry
//-----------------------------------------------------------------------------
enum
{
	HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE = 0x1,
	HACKLIGHTCACHEFLAGS_HASNONSWITCHABLELIGHTSTYLE = 0x2,	// flickering lights
	HACKLIGHTCACHEFLAGS_HASDONESTATICLIGHTING = 0x4,		// for static props
};


struct LightingStateInfo_t
{
	float	m_pIllum[MAXLOCALLIGHTS];
	bool 	m_LightingStateHasSkylight;
	LightingStateInfo_t()
	{
		memset( this, 0, sizeof( *this ) );
	}
	void Clear()
	{
		memset( this, 0, sizeof( *this ) );
	}
};
 

// This holds the shared data between lightcache_t and PropLightcache_t.
// This way, PropLightcache_t can be about half the size, since it doesn't need a bunch of data in lightcache_t.
class CBaseLightCache : public LightingStateInfo_t
{
public:
	CBaseLightCache()
	{
		m_pEnvCubemapTexture = NULL;
		memset( m_pLightstyles, 0, sizeof( m_pLightstyles ) );
		m_LightingFlags = 0;
		m_LastFrameUpdated_LightStyles = -1;
	}

	bool HasLightStyle() 
	{
		return ( m_LightingFlags & ( HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE | HACKLIGHTCACHEFLAGS_HASNONSWITCHABLELIGHTSTYLE ) ) ? true : false;
	}

	bool HasSwitchableLightStyle() 
	{
		return ( m_LightingFlags & HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE ) ? true : false;
	}

	bool HasNonSwitchableLightStyle() 
	{
		return ( m_LightingFlags & HACKLIGHTCACHEFLAGS_HASNONSWITCHABLELIGHTSTYLE ) ? true : false;
	}

public:
	// cache for static lighting . . never changes after cache creation
	// preserved because static prop's color meshes are under cache control
	LightingState_t	m_StaticLightingState;		

	// cache for light styles
	LightingState_t m_LightStyleLightingState;	// This includes m_StaticLightingState
	int				m_LastFrameUpdated_LightStyles;

	LightingState_t m_DynamicLightingState; // This includes m_LightStyleLightingState
	int				m_LastFrameUpdated_DynamicLighting;


	// FIXME: could just use m_LightStyleWorldLights.Count() if we are a static prop
	int	m_LightingFlags; /* LightCacheFlags_t */
	int leaf;

	unsigned char	m_pLightstyles[MAX_LIGHTSTYLE_BYTES];

	// for a dynamic prop, ideally the cache center if valid space, otherwise initial origin
	// for a static prop, the provided origin
	Vector			m_LightingOrigin;

	// env_cubemap texture associated with this entry.
	ITexture *		m_pEnvCubemapTexture;
};

class lightcache_t : public CBaseLightCache
{
public:
	lightcache_t()
	{
		m_LastFrameUpdated_DynamicLighting = -1;
	}

public:

	// Precalculated for the static lighting from AddWorldLightToLightingState.
	dworldlight_t		*m_StaticPrecalc_LocalLight[MAXLOCALLIGHTS];
	unsigned short		m_StaticPrecalc_NumLocalLights;
	LightingStateInfo_t m_StaticPrecalc_LightingStateInfo;
	// the boxcolor is stored in m_StaticLightingState.


	// bucket singly linked list.
	unsigned short	next;	// index into lightcache
	unsigned short	bucket;	// index into lightbuckets

	// lru links
	unsigned short	lru_prev;
	unsigned short	lru_next;

	int				x,y,z;
};

struct PropLightcache_t : public CBaseLightCache
{
public:
	// Linked into s_pAllStaticProps.
	PropLightcache_t *m_pNextPropLightcache;

	unsigned int	m_Flags; // corresponds to LIGHTCACHEFLAGS_*
	// stuff for pushing lights onto static props
	int				m_DLightActive; // bit field for which dlights currently affect us.
									// recomputed by AddDlightsForStaticProps
	int				m_DLightMarkFrame;	// last frame in which a dlight was marked on this prop (helps detect lights that are marked but have moved away from this prop)
	CUtlVector<short> m_LightStyleWorldLights;	// This is a list of lights that affect this static prop cache entry.
	int				m_SwitchableLightFrame;		// This is the last frame that switchable lights were calculated.
	Vector			mins; // fixme: make these smaller
	Vector			maxs; // fixme: make these smaller

	bool HasDlights() { return m_DLightActive ? true : false; }
	PropLightcache_t()
	{
		m_Flags = 0;
		m_SwitchableLightFrame = -1;
		m_DLightActive = 0;
		m_DLightMarkFrame = 0;
	}
};


ConVar r_worldlights	("r_worldlights", "4", 0, "number of world lights to use per vertex" );
ConVar r_radiosity		("r_radiosity", "4", FCVAR_CHEAT, "0: no radiosity\n1: radiosity with ambient cube (6 samples)\n2: radiosity with 162 samples\n3: 162 samples for static props, 6 samples for everything else" );
ConVar r_worldlightmin	("r_worldlightmin", "0.0002" );
ConVar r_avglight		("r_avglight", "1", FCVAR_CHEAT);
static ConVar r_drawlightcache		("r_drawlightcache", "0", FCVAR_CHEAT, "0: off\n1: draw light cache entries\n2: draw rays\n");
static ConVar r_minnewsamples		("r_minnewsamples", "3");
static ConVar r_maxnewsamples		("r_maxnewsamples", "6");
static ConVar r_maxsampledist		("r_maxsampledist", "128");
static ConVar r_lightcachecenter	("r_lightcachecenter", "1", FCVAR_CHEAT );

// head and tail sentinels of the LRU
#define LIGHT_LRU_HEAD_INDEX MAX_CACHE_ENTRY
#define LIGHT_LRU_TAIL_INDEX (MAX_CACHE_ENTRY+1)

static lightcache_t lightcache[MAX_CACHE_ENTRY + 2]; // the extra 2 are the head and tail
static unsigned short lightbuckets[MAX_CACHE_BUCKETS];

static CClassMemoryPool<PropLightcache_t>	s_PropCache( 256, CClassMemoryPool<lightcache_t>::GROW_SLOW );
 
// A memory pool of lightcache entries that is 
static int cached_r_worldlights = -1;
static int cached_r_radiosity = -1;
static int cached_r_avglight = -1;
static int cached_mat_fullbright = -1;
static int cached_r_lightcache_numambientsamples = -1;
static PropLightcache_t* s_pAllStaticProps = NULL;


// Used to convert RGB colors to greyscale intensity
static Vector s_Grayscale( 0.299f, 0.587f, 0.114f ); 

#define BIT_SET( a, b ) ((a)[(b)>>3] & (1<<((b)&7)))


inline unsigned short GetLightCacheIndex( const lightcache_t *pCache )
{
	return pCache - lightcache;
}

inline lightcache_t& GetLightLRUHead()
{
	return lightcache[LIGHT_LRU_HEAD_INDEX];
}

inline lightcache_t& GetLightLRUTail()
{
	return lightcache[LIGHT_LRU_TAIL_INDEX];
}


//-----------------------------------------------------------------------------
// Purpose: Set up the LRU
//-----------------------------------------------------------------------------
void R_StudioInitLightingCache( void )
{
	unsigned short i;

	memset( lightcache, 0, sizeof(lightcache) );

	for ( i=0; i < ARRAYSIZE( lightcache ); i++ )
		lightcache[i].bucket = 0xFFFF;

	for ( i=0; i < ARRAYSIZE( lightbuckets ); i++ )
			lightbuckets[i] = 0xFFFF;

	unsigned short last = LIGHT_LRU_HEAD_INDEX;
	// Link every node into the LRU
	for ( i = 0; i < MAX_CACHE_ENTRY-1; i++)
	{
		lightcache[i].lru_prev = last;
		lightcache[i].lru_next = i + 1;
		last = i;
	}
	// terminate the lru list
	lightcache[i].lru_prev = last;
	lightcache[i].lru_next = LIGHT_LRU_TAIL_INDEX;

	// link the sentinels
	lightcache[LIGHT_LRU_HEAD_INDEX].lru_next = 0;
	lightcache[LIGHT_LRU_TAIL_INDEX].lru_prev = i;

	// Lower number of lights on older hardware
	if ( g_pMaterialSystemHardwareConfig->MaxNumLights() < r_worldlights.GetInt() )
	{
		r_worldlights.SetValue(	g_pMaterialSystemHardwareConfig->MaxNumLights() );
	}

	cached_r_worldlights = r_worldlights.GetInt();
	cached_r_radiosity = r_radiosity.GetInt();
	cached_r_avglight = r_avglight.GetInt();
	cached_mat_fullbright = g_pMaterialSystemConfig->nFullbright;
	cached_r_lightcache_numambientsamples = r_lightcache_numambientsamples.GetInt();

	// Recompute all static lighting
	InvalidateStaticLightingCache();
}


void R_StudioCheckReinitLightingCache()
{
	// Make sure this stays clamped to match hardware capabilities
	if ( g_pMaterialSystemHardwareConfig->MaxNumLights() < r_worldlights.GetInt() )
	{
		r_worldlights.SetValue(	g_pMaterialSystemHardwareConfig->MaxNumLights() );
	}

	// Flush the lighting cache, if necessary
	if (cached_r_worldlights != r_worldlights.GetInt() ||
		cached_r_radiosity != r_radiosity.GetInt() ||
		cached_r_avglight != r_avglight.GetInt() ||
		cached_mat_fullbright != g_pMaterialSystemConfig->nFullbright ||
		cached_r_lightcache_numambientsamples != r_lightcache_numambientsamples.GetInt() )
	{
		R_StudioInitLightingCache();
	}
}	


//-----------------------------------------------------------------------------
// Purpose: Moves this cache entry to the end of the lru, i.e. marks it recently used
// Input  : *pcache - 
//-----------------------------------------------------------------------------
static void LightcacheMark( lightcache_t *pcache )
{
	// don't link in static lighting
	if ( !pcache->lru_next && !pcache->lru_prev )
		return;

	// already at tail
	if ( GetLightCacheIndex( pcache ) == lightcache[LIGHT_LRU_TAIL_INDEX].lru_prev )
		return;

	// unlink pcache
	lightcache[pcache->lru_prev].lru_next = pcache->lru_next;
	lightcache[pcache->lru_next].lru_prev = pcache->lru_prev;
	
	// link to tail
	// patch backward link
	lightcache[GetLightLRUTail().lru_prev].lru_next = GetLightCacheIndex( pcache );
	pcache->lru_prev = GetLightLRUTail().lru_prev;
	
	// patch forward link
	pcache->lru_next = LIGHT_LRU_TAIL_INDEX;
	GetLightLRUTail().lru_prev = GetLightCacheIndex( pcache );
}


//-----------------------------------------------------------------------------
// Purpose: Unlink a cache entry from its current bucket
// Input  : *pcache - 
//-----------------------------------------------------------------------------
static void LightcacheUnlink( lightcache_t *pcache )
{
	unsigned short iBucket = pcache->bucket;
	
	// not used yet?
	if ( iBucket == 0xFFFF )
		return;

	unsigned short iCache = GetLightCacheIndex( pcache );

	// unlink it
	unsigned short plist = lightbuckets[iBucket];

	if ( plist == iCache )
	{
		// head of bucket?  move bucket down
		lightbuckets[iBucket] = pcache->next;
	}
	else
	{
		bool found = false;
		// walk the bucket
		while ( plist != 0xFFFF )
		{
			// if next is pcache, unlink pcache
			if ( lightcache[plist].next == iCache )
			{
				lightcache[plist].next = pcache->next;
				found = true;
				break;
			}
			plist = lightcache[plist].next;
		}
		assert(found);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the least recently used cache entry
//-----------------------------------------------------------------------------
static lightcache_t *LightcacheGetLRU( void )
{
	// grab head
	lightcache_t *pcache = &lightcache[GetLightLRUHead().lru_next];

	// move to tail
	LightcacheMark( pcache );

	// unlink from the bucket
	LightcacheUnlink( pcache );

	pcache->leaf = -1;
	return pcache;
}


//-----------------------------------------------------------------------------
// Purpose: Quick & Dirty hashing function to bucket the cube in 4d parameter space
//-----------------------------------------------------------------------------
static int LightcacheHashKey( int x, int y, int z, int leaf )
{
	unsigned int key = (((x<<20) + (y<<8) + z) ^ (leaf));
	key =  key % MAX_CACHE_BUCKETS;
	return (int)key;
}


//-----------------------------------------------------------------------------
// Compute the lightcache bucket given a position
//-----------------------------------------------------------------------------
static lightcache_t* FindInCache( int bucket, int x, int y, int z, int leaf )
{
	// loop over the entries in this bucket
	unsigned short iCache;
	for ( iCache = lightbuckets[bucket]; iCache != 0xFFFF; iCache = lightcache[iCache].next )
	{
		lightcache_t *pCache = &lightcache[iCache];

		// hit?
		if (pCache->x == x && pCache->y == y && pCache->z == z && pCache->leaf == leaf )
		{
			return pCache;
		}
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Links to a bucket
//-----------------------------------------------------------------------------
static inline void LinkToBucket( int bucket, lightcache_t* pcache )
{
	pcache->next = lightbuckets[bucket];
	lightbuckets[bucket] = GetLightCacheIndex( pcache );

	// point back to the bucket
	pcache->bucket = (unsigned short)bucket;
}


//-----------------------------------------------------------------------------
// Links in a new lightcache entry
//-----------------------------------------------------------------------------
static lightcache_t* NewLightcacheEntry( int bucket )
{
	// re-use the LRU cache entry
	lightcache_t* pcache = LightcacheGetLRU();
	LinkToBucket( bucket, pcache );
	return pcache;
}


//-----------------------------------------------------------------------------
// Compute the lightcache origin
//-----------------------------------------------------------------------------
#if 0
static inline void ComputeLightcacheOrigin( int x, int y, int z, Vector& org )
{
	// this is suspicious and *maybe* wrong
	// the bucket origin can't re-establish the correct negative numbers
	// because of the non-arithmetic shift down?
	int ix = x << HASH_GRID_SIZEX;
	int iy = y << HASH_GRID_SIZEY;
	int iz = z << HASH_GRID_SIZEZ;
	org.Init( ix, iy, iz );
}
#endif

//-----------------------------------------------------------------------------
// Compute the lightcache bounds given a point
//-----------------------------------------------------------------------------
void ComputeLightcacheBounds( const Vector &vecOrigin, Vector *pMins, Vector *pMaxs )
{
	bool bXPos = (vecOrigin[0] >= 0);
	bool bYPos = (vecOrigin[1] >= 0);
	bool bZPos = (vecOrigin[2] >= 0);

	// can't snap and shift negative values
	// truncate positive number and shift
	int ix = ((int)(fabs(vecOrigin[0]))) >> HASH_GRID_SIZEX;
	int iy = ((int)(fabs(vecOrigin[1]))) >> HASH_GRID_SIZEY;
	int iz = ((int)(fabs(vecOrigin[2]))) >> HASH_GRID_SIZEZ;

	// mins is floored as fixup depending on <0 or >0
	pMins->x = (bXPos ? ix : -(ix + 1)) << HASH_GRID_SIZEX;	
	pMins->y = (bYPos ? iy : -(iy + 1)) << HASH_GRID_SIZEY;	
	pMins->z = (bZPos ? iz : -(iz + 1)) << HASH_GRID_SIZEZ;	
	
	// maxs is exactly one grid increasing from mins
	pMaxs->x = pMins->x + (1 << HASH_GRID_SIZEX );
	pMaxs->y = pMins->y + (1 << HASH_GRID_SIZEY );
	pMaxs->z = pMins->z + (1 << HASH_GRID_SIZEZ );

	Assert( (pMins->x <= vecOrigin.x) && (pMins->y <= vecOrigin.y) && (pMins->z <= vecOrigin.z) );
	Assert( (pMaxs->x >= vecOrigin.x) && (pMaxs->y >= vecOrigin.y) && (pMaxs->z >= vecOrigin.z) );
}

//-----------------------------------------------------------------------------
// Compute the cache origin suitable for key
//-----------------------------------------------------------------------------
static inline void OriginToCacheOrigin( const Vector &origin, int &x, int &y, int &z )
{
	x = ((int)origin[0] + 32768) >> HASH_GRID_SIZEX;
	y = ((int)origin[1] + 32768) >> HASH_GRID_SIZEY;
	z = ((int)origin[2] + 32768) >> HASH_GRID_SIZEZ;
}


//-----------------------------------------------------------------------------
// Finds ambient lights
//-----------------------------------------------------------------------------
dworldlight_t* FindAmbientLight()
{
	// find any ambient lights
	for (int i = 0; i < host_state.worldbrush->numworldlights; i++)
	{
		if (host_state.worldbrush->worldlights[i].type == emit_skyambient)
		{
			return &host_state.worldbrush->worldlights[i];
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Computes the ambient term from a particular surface
//-----------------------------------------------------------------------------
static void ComputeAmbientFromSurface( SurfaceHandle_t surfID, dworldlight_t* pSkylight, 
									   Vector& radcolor )
{
	if (IS_SURF_VALID( surfID ) )
	{
		// If we hit the sky, use the sky ambient
		if (MSurf_Flags( surfID ) & SURFDRAW_SKY)
		{
			if (pSkylight)
			{
				// add in sky ambient
				VectorCopy( pSkylight->intensity, radcolor );
			}
		}
		else
		{
			Vector reflectivity;
			MSurf_TexInfo( surfID )->material->GetReflectivity( reflectivity );
			VectorMultiply( radcolor, reflectivity, radcolor );
		}
	}
}

//-----------------------------------------------------------------------------
// Computes the ambient term from a large number of spherical samples
//-----------------------------------------------------------------------------

static void ComputeAmbientFromSphericalSamples( const Vector& start, 
						Vector* lightBoxColor )
{
	VPROF( "ComputeAmbientFromSphericalSamples" );
	// find any ambient lights
	dworldlight_t *pSkylight = FindAmbientLight();

	Vector radcolor[NUMRANDOMNORMALS];
	Assert( cached_r_lightcache_numambientsamples <= ARRAYSIZE( radcolor ) );

	// sample world by casting N rays distributed across a sphere
	Vector upend;
	int i;
	for ( i = 0; i < cached_r_lightcache_numambientsamples; i++)
	{
		// FIXME: a good optimization would be to scale this per leaf
		VectorMA( start, COORD_EXTENT * 1.74, g_anorms[i], upend );

		// Now that we've got a ray, see what surface we've hit
		SurfaceHandle_t surfID = R_LightVec (start, upend, false, radcolor[i] );
		if (!IS_SURF_VALID(surfID) )
			continue;

		ComputeAmbientFromSurface( surfID, pSkylight, radcolor[i] );
	}

	// accumulate samples into radiant box
	const Vector* pBoxDirs = g_pStudioRender->GetAmbientLightDirections();
	for (int j = g_pStudioRender->GetNumAmbientLightSamples(); --j >= 0; )
	{
		float c, t;
		t = 0;

		lightBoxColor[j][0] = 0;
		lightBoxColor[j][1] = 0;
		lightBoxColor[j][2] = 0;

		for (i = 0; i < cached_r_lightcache_numambientsamples; i++)
		{
			c = DotProduct( g_anorms[i], pBoxDirs[j] );
			if (c > 0)
			{
				t += c;
				VectorMA( lightBoxColor[j], c, radcolor[i], lightBoxColor[j] );
			}
		}
		VectorMultiply( lightBoxColor[j], 1/t, lightBoxColor[j] );
	}
}


static void ComputeAmbientFromLeaf( const Vector &start, int leafID, Vector *lightBoxColor, bool *bAddedLeafAmbientCube )
{
	if( leafID >= 0 )
	{
		Mod_LeafAmbientColorAtPos( lightBoxColor, start, leafID );
		// The ambient lighting in the leaves has the emit_surface lights factored in.
		*bAddedLeafAmbientCube = true;
	}
	else
	{
		int i;
		for( i = 0; i < 6; i++ )
		{
			lightBoxColor[i].Init( 0.0f, 0.0f, 0.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Computes the ambient term from 6 cardinal directions
//-----------------------------------------------------------------------------
static void ComputeAmbientFromAxisAlignedSamples( const Vector& start, 
						Vector* lightBoxColor )
{
	Vector upend;

	// find any ambient lights
	dworldlight_t *pSkylight = FindAmbientLight();

	// sample world only along cardinal axes
	const Vector* pBoxDirs = g_pStudioRender->GetAmbientLightDirections();
	for (int i = 0; i < 6; i++)
	{
		VectorMA( start, COORD_EXTENT * 1.74, pBoxDirs[i], upend );

		// Now that we've got a ray, see what surface we've hit
		SurfaceHandle_t surfID = R_LightVec (start, upend, false, lightBoxColor[i] );
		if (!IS_SURF_VALID( surfID ) )
			continue;

		ComputeAmbientFromSurface( surfID, pSkylight, lightBoxColor[i] );

	}
}


//-----------------------------------------------------------------------------
// Computes the ambient lighting at a point, and sets the lightstyles bitfield
//-----------------------------------------------------------------------------
static void R_StudioGetAmbientLightForPoint( 
	int leafID, 
	const Vector& start, 
	Vector* pLightBoxColor, 
	bool bIsStaticProp,
	bool *bAddedLeafAmbientCube )
{
	*bAddedLeafAmbientCube = false;

	VPROF( "R_StudioGetAmbientLightForPoint" );
	int i;
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		for (i = g_pStudioRender->GetNumAmbientLightSamples(); --i >= 0; )
		{
			VectorFill( pLightBoxColor[i], 1.0 );
		}
		return;
	}

	switch( r_radiosity.GetInt() )
	{
	case 1:
		ComputeAmbientFromAxisAlignedSamples( start, pLightBoxColor );
		break;

	case 2:
		ComputeAmbientFromSphericalSamples( start, pLightBoxColor );
		break;

	case 3:
		if (bIsStaticProp)
			ComputeAmbientFromSphericalSamples( start, pLightBoxColor );
		else
			ComputeAmbientFromAxisAlignedSamples( start, pLightBoxColor );
		break;

	case 4:
		if (bIsStaticProp)
			ComputeAmbientFromSphericalSamples( start, pLightBoxColor );
		else
			ComputeAmbientFromLeaf( start, leafID, pLightBoxColor, bAddedLeafAmbientCube );
		break;

	default:
		// assume no bounced light from the world
		for (i = g_pStudioRender->GetNumAmbientLightSamples(); --i >= 0; )
		{
			VectorFill( pLightBoxColor[i], 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// This filter bumps against the world + all but one prop
//-----------------------------------------------------------------------------
class CTraceFilterWorldAndProps : public ITraceFilter
{
public:
	CTraceFilterWorldAndProps( IHandleEntity *pHandleEntity ) : m_pIgnoreProp( pHandleEntity ) {}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		// We only bump against props + we ignore one particular prop
		if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
			return false;

		return ( pHandleEntity != m_pIgnoreProp );
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING_FILTER_PROPS;
	}

private:
	IHandleEntity *m_pIgnoreProp;
};



static float LightIntensityAndDirectionAtPointOld( dworldlight_t* pLight, 
	const Vector& mid, int fFlags, IHandleEntity *pIgnoreEnt, Vector *pDirection ) 
{
	CTraceFilterWorldOnly worldTraceFilter;
	CTraceFilterWorldAndProps propTraceFilter( pIgnoreEnt );
	ITraceFilter *pTraceFilter = &worldTraceFilter;
	if (fFlags & LIGHT_OCCLUDE_VS_PROPS)
	{
		pTraceFilter = &propTraceFilter;
	}

	// Special case lights
	switch (pLight->type)
	{
	case emit_skylight:
		{
			// There can be more than one skylight, but we should only
			// ever be affected by one of them (multiple ones are created from
			// a single light in vrad)

			VectorFill( *pDirection, 0 );
			// check to see if you can hit the sky texture
			Vector end;
			VectorMA( mid, -COORD_EXTENT * 1.74f, pLight->normal, end ); // max_range * sqrt(3)

			trace_t tr;
			Ray_t ray;
			ray.Init( mid, end );
			g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, pTraceFilter, &tr );

			// Here, we didn't hit the sky, so we must be in shadow
			if ( !(tr.surface.flags & SURF_SKY) )
				return 0.0f;

			// fudge delta and dist for skylights
			*pDirection = -pLight->normal;
			return 1.0f;
		}

	case emit_skyambient:
		// always ignore these
		return 0.0f;
	}

	// all other lights

	// check distance
	VectorSubtract( pLight->origin, mid, *pDirection );
	float ratio = Engine_WorldLightDistanceFalloff( pLight, *pDirection, (fFlags & LIGHT_NO_RADIUS_CHECK) != 0 );

	// Add in light style component
	if( !( fFlags & LIGHT_IGNORE_LIGHTSTYLE_VALUE ) )
	{
		ratio *= LightStyleValue( pLight->style );
	}

	// Early out for really low-intensity lights
	// That way we don't need to ray-cast or normalize
	float intensity = max( pLight->intensity[0], pLight->intensity[1] );
	intensity = max(intensity, pLight->intensity[2] );

	// This is about 1/256
	// See the comment titled "EMIT_SURFACE LIGHTS" at the top for info about why we don't 
	// test emit_surface lights here.
	if ( pLight->type != emit_surface )
	{
		if (intensity * ratio < r_worldlightmin.GetFloat() )
			return 0.0f;
	}

	float dist = VectorNormalize( *pDirection );

	if ( fFlags & LIGHT_NO_OCCLUSION_CHECK )
		return ratio;

	trace_t pm;
	Ray_t ray;
	ray.Init( mid, pLight->origin );
	g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, pTraceFilter, &pm );

	// hack
	if ( (1.f-pm.fraction) * dist > 8 )
	{
#ifndef SWDS
		if (r_drawlightcache.GetInt() == 2)
		{
			CDebugOverlay::AddLineOverlay( mid, pm.endpos, 255, 0, 0, 255, true, 3 );
		}
#endif
		return 0.f;
	}

	return ratio;
}



//-----------------------------------------------------------------------------
// This method returns the effective intensity of a light as seen from
// a particular point. PVS is used to speed up the task.
//-----------------------------------------------------------------------------
static float LightIntensityAndDirectionAtPointNew( dworldlight_t* pLight, lightzbuffer_t *pZBuf,
	const Vector& mid, int fFlags, IHandleEntity *pIgnoreEnt, Vector *pDirection ) 
{
	CTraceFilterWorldOnly worldTraceFilter;
	CTraceFilterWorldAndProps propTraceFilter( pIgnoreEnt );
	ITraceFilter *pTraceFilter = &worldTraceFilter;
	if (fFlags & LIGHT_OCCLUDE_VS_PROPS)
	{
		pTraceFilter = &propTraceFilter;
	}

	// Special case lights
	switch (pLight->type)
	{
	case emit_skylight:
		{
			// There can be more than one skylight, but we should only
			// ever be affected by one of them (multiple ones are created from
			// a single light in vrad)

			VectorFill( *pDirection, 0 );
			// check to see if you can hit the sky texture
			Vector end;
			VectorMA( mid, -COORD_EXTENT * 1.74f, pLight->normal, end ); // max_range * sqrt(3)

			trace_t tr;
			Ray_t ray;
			ray.Init( mid, end );
			g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, pTraceFilter, &tr );

			// Here, we didn't hit the sky, so we must be in shadow
			if ( !(tr.surface.flags & SURF_SKY) )
				return 0.0f;

			// fudge delta and dist for skylights
			*pDirection = -pLight->normal;
			return 1.0f;
		}

	case emit_skyambient:
		// always ignore these
		return 0.0f;
	}

	// all other lights

	// check distance
	VectorSubtract( pLight->origin, mid, *pDirection );
	float ratio = Engine_WorldLightDistanceFalloff( pLight, *pDirection, (fFlags & LIGHT_NO_RADIUS_CHECK) != 0 );

	// Add in light style component
	if( !( fFlags & LIGHT_IGNORE_LIGHTSTYLE_VALUE ) )
	{
		ratio *= LightStyleValue( pLight->style );
	}

	// Early out for really low-intensity lights
	// That way we don't need to ray-cast or normalize
	float intensity = fpmax( pLight->intensity[0], pLight->intensity[1] );
	intensity = fpmax(intensity, pLight->intensity[2] );

	// This is about 1/256
	// See the comment titled "EMIT_SURFACE LIGHTS" at the top for info about why we don't 
	// test emit_surface lights here.
	if ( pLight->type != emit_surface )
	{
		if (intensity * ratio < r_worldlightmin.GetFloat() )
			return 0.0f;
	}

	float dist = VectorNormalize( *pDirection );

	if ( fFlags & LIGHT_NO_OCCLUSION_CHECK )
		return ratio;

	float flTraceDistance = dist;
	// check if we are so close to the light that we shouldn't use our coarse z buf
	if ( dist - ( 1 << HASH_GRID_SIZEZ ) < 8 * SHADOW_ZBUF_RES )
		pZBuf = NULL;

	Vector epnt = mid;

	LightShadowZBufferSample_t *pSample = NULL;
	if ( pZBuf )
	{
		pSample = &( pZBuf->GetSample( *pDirection ) );
		if ( ( pSample->m_flHitDistance < pSample->m_flTraceDistance ) || ( pSample->m_flTraceDistance >= dist ) )
		{
			// hit!
			if ( dist > pSample->m_flHitDistance + 8  )		// shadow hit
			{
#ifndef SWDS
				if (r_drawlightcache.GetInt() == 2 )
				{
					CDebugOverlay::AddLineOverlay( mid, pLight->origin, 0, 0, 0, 255, true, 3 );
				}
#endif
				return 0;
			}
			else
			{
#ifndef SWDS
				if (r_drawlightcache.GetInt() == 2 )
				{
					CDebugOverlay::AddLineOverlay( mid, pLight->origin, 0, 255, 0, 255, true, 3 );
				}
#endif
				return ratio;
			}

		}

		// cache miss
		flTraceDistance = max( 100.0, 2.0 * dist );		// trace a little further for better caching
		epnt += ( dist - flTraceDistance ) * ( *pDirection );
	}
	
	trace_t pm;
	Ray_t ray;
	ray.Init( pLight->origin, epnt );	// trace from light to object
	g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, pTraceFilter, &pm );
	float flHitDistance = ( pm.startsolid ) ? FLT_EPSILON : ( pm.fraction ) * flTraceDistance;
	
	if ( pSample )
	{
		pSample->m_flTraceDistance = flTraceDistance;
		pSample->m_flHitDistance = ( pm.fraction >= 1.0 ) ? 1.0e23 : flHitDistance;
	}
	if ( dist > flHitDistance + 8)
	{
#ifndef SWDS
		if (r_drawlightcache.GetInt() == 2 )
		{
			CDebugOverlay::AddLineOverlay( mid, pLight->origin, 255, 0, 0, 255, true, 3 );
		}
#endif
		return 0.f;
	}
	return ratio;
}


static float LightIntensityAndDirectionAtPoint( dworldlight_t* pLight, lightzbuffer_t *pZBuf,
	const Vector& mid, int fFlags, IHandleEntity *pIgnoreEnt, Vector *pDirection ) 
{
	if ( pZBuf )
		return LightIntensityAndDirectionAtPointNew( pLight, pZBuf, mid, fFlags, pIgnoreEnt, pDirection );
	else
		return LightIntensityAndDirectionAtPointOld( pLight,  mid, fFlags, pIgnoreEnt, pDirection );
#if 0
	float old = LightIntensityAndDirectionAtPointOld( pLight,  mid, fFlags, pIgnoreEnt, pDirection );
	float newf = LightIntensityAndDirectionAtPointNew( pLight, pZBuf, mid, fFlags, pIgnoreEnt, pDirection );
	if ( old != newf )
	{
		float old2 = LightIntensityAndDirectionAtPointOld( pLight,  mid, fFlags, pIgnoreEnt, pDirection );
		float newf2 = LightIntensityAndDirectionAtPointNew( pLight, pZBuf, mid, fFlags, pIgnoreEnt, pDirection );
	}
	return newf;
#endif
}

//-----------------------------------------------------------------------------
// This method returns the effective intensity of a light as seen within
// a particular box...
// a particular point. PVS is used to speed up the task.
//-----------------------------------------------------------------------------
static float LightIntensityAndDirectionInBox( dworldlight_t* pLight, 
											  lightzbuffer_t *pZBuf,
											  const Vector &mid, const Vector &mins, const Vector &maxs, int fFlags, 
											  Vector *pDirection ) 
{
	// Choose the point closest on the box to the light to get max intensity
	// within the box....

	if ( !r_oldlightselection.GetBool() )
	{
		switch (pLight->type)
		{
		case emit_spotlight:	// directional & positional
			{
				float sphereRadius = (maxs-mid).Length();
				// first do a sphere/sphere check
				float dist = (pLight->origin - mid).Length();
				if ( dist > (sphereRadius + pLight->radius) )
					return 0;
				// PERFORMANCE: precalc this and store in the light?
				float angle = acos(pLight->stopdot2);
				float sinAngle = sin(angle);
				if ( !IsSphereIntersectingCone( mid, sphereRadius, pLight->origin, pLight->normal, sinAngle, pLight->stopdot2 ) )
					return 0;
			}
			// NOTE: fall through to radius check in point case
		case emit_point:
			{
				float distSqr = CalcSqrDistanceToAABB( mins, maxs, pLight->origin );
				if ( distSqr > pLight->radius * pLight->radius )
					return 0;
			}

			break;
		case emit_surface:	// directional & positional, fixed cone size
			{
				float sphereRadius = (maxs-mid).Length();
				// first do a sphere/sphere check
				float dist = (pLight->origin - mid).Length();
				if ( dist > (sphereRadius + pLight->radius) )
					return 0;
				// PERFORMANCE: precalc this and store in the light?
				if ( !IsSphereIntersectingCone( mid, sphereRadius, pLight->origin, pLight->normal, 1.0f, 0.0f ) )
					return 0;
			}
			break;
		}
	}
	else
	{

		// NOTE: Here, we do radius check to check to see if we should even care about the light
		// because we want to check the closest point in the box
		switch (pLight->type)
		{
		case emit_point:
		case emit_spotlight:	// directional & positional
			{
				Vector vecClosestPoint;
				vecClosestPoint.Init();
				for ( int i = 0; i < 3; ++i )
				{
					vecClosestPoint[i] = clamp( pLight->origin[i], mins[i], maxs[i] );
				}

				vecClosestPoint -= pLight->origin;
				if ( vecClosestPoint.LengthSqr() > pLight->radius * pLight->radius )
					return 0;
			}
			break;
		}
	}

	return LightIntensityAndDirectionAtPoint( pLight, pZBuf, mid, fFlags | LIGHT_NO_RADIUS_CHECK, NULL, pDirection );
}

	
//-----------------------------------------------------------------------------
// Computes the static vertex lighting term from a large number of spherical samples
//-----------------------------------------------------------------------------
bool ComputeVertexLightingFromSphericalSamples( const Vector& vecVertex, 
	const Vector &vecNormal, IHandleEntity *pIgnoreEnt, Vector *pLinearColor )
{
	if ( IsX360() )
		return false;

	// Check to see if this vertex is in solid
	trace_t tr;
	CTraceFilterWorldAndProps filter( pIgnoreEnt );
	Ray_t ray;
	ray.Init( vecVertex, vecVertex );
	g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, &filter, &tr );
	if ( tr.startsolid || tr.allsolid )
		return false;

	pLinearColor->Init( 0, 0, 0 );

	// find any ambient lights
	dworldlight_t *pSkylight = FindAmbientLight();

	// sample world by casting N rays distributed across a sphere
	float t = 0.0f;
	Vector upend, color;
	int i;
	for ( i = 0; i < cached_r_lightcache_numambientsamples; i++)
	{
		float flDot = DotProduct( vecNormal, s_raddir[i] );
		if ( flDot < 0.0f )
			continue;

		// FIXME: a good optimization would be to scale this per leaf
		VectorMA( vecVertex, COORD_EXTENT * 1.74, s_raddir[i], upend );

		// Now that we've got a ray, see what surface we've hit
		SurfaceHandle_t surfID = R_LightVec( vecVertex, upend, false, color );
		if ( !IS_SURF_VALID(surfID) )
			continue;

		// FIXME: Maybe make sure we aren't obstructed by static props?
		// To do this, R_LightVec would need to return distance of hit...
		// Or, we need another arg to R_LightVec to return black when hitting a static prop
		ComputeAmbientFromSurface( surfID, pSkylight, color );

		t += flDot;
		VectorMA( *pLinearColor, flDot, color, *pLinearColor );
	}

	if (t != 0.0f)
	{
		*pLinearColor /= t;
	}

	// Now deal with direct lighting
	bool bHasSkylight = false;

	// Figure out the PVS info for this location
 	int leaf = CM_PointLeafnum( vecVertex );
	const byte* pVis = CM_ClusterPVS( CM_LeafCluster( leaf ) );

	// Now add in the direct lighting
	Vector vecDirection;
	for (i = 0; i < host_state.worldbrush->numworldlights; ++i)
	{
		dworldlight_t *wl = &host_state.worldbrush->worldlights[i];

		// FIXME: This is sort of a hack; only one skylight is allowed in the
		// lighting...
		if ((wl->type == emit_skylight) && bHasSkylight)
			continue;

		// only do it if the entity can see into the lights leaf
		if ((wl->cluster < 0) || (!BIT_SET( pVis, wl->cluster )) )
			continue;

		float flRatio = LightIntensityAndDirectionAtPoint( wl, NULL, vecVertex, LIGHT_OCCLUDE_VS_PROPS, pIgnoreEnt, &vecDirection );

		// No light contribution? Get outta here!
		if ( flRatio <= 0.0f )
			continue;

		// Check if we've got a skylight
		if ( wl->type == emit_skylight )
			bHasSkylight = true;

		// Figure out spotlight attenuation
		float flAngularRatio = Engine_WorldLightAngle( wl, wl->normal, vecNormal, vecDirection );

		// Add in the direct lighting
		VectorMAInline( *pLinearColor, flAngularRatio * flRatio, wl->intensity, *pLinearColor );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Finds the minimum light
//-----------------------------------------------------------------------------
static int FindDarkestWorldLight( int numLights, float* pLightIllum, float newIllum )
{
	// FIXME: make the list sorted?
	int minLightIndex = -1;
	float minillum = newIllum;
	for (int j = 0; j < numLights; ++j)
	{
		// only check ones dimmer than have already been checked
		if (pLightIllum[j] < minillum)
		{
			minillum = pLightIllum[j];
			minLightIndex = j;
		}
	}

	return minLightIndex;
}


//-----------------------------------------------------------------------------
// Adds a world light to the ambient cube
//-----------------------------------------------------------------------------
static void	AddWorldLightToLightCube( dworldlight_t* pWorldLight, 
										Vector* pBoxColor,
										const Vector& direction,
										float ratio )
{
	if (ratio == 0.0f)
		return;

	// add whatever didn't stay in the list to lightBoxColor
	// FIXME: This method is a guess, I don't know how it should be done
	const Vector* pBoxDir = g_pStudioRender->GetAmbientLightDirections();

	for (int j = g_pStudioRender->GetNumAmbientLightSamples(); --j >= 0; )
	{
		float t = DotProduct( pBoxDir[j], direction );
		if (t > 0)
		{
			VectorMAInline( pBoxColor[j], ratio * t, pWorldLight->intensity, pBoxColor[j] );
		}
	}
}


//-----------------------------------------------------------------------------
// Adds a world light to the ambient cube
//-----------------------------------------------------------------------------
void AddWorldLightToAmbientCube( dworldlight_t* pWorldLight, const Vector &vecLightingOrigin, AmbientCube_t &ambientCube )
{
	Vector vecDirection;
	float ratio = LightIntensityAndDirectionAtPoint( pWorldLight, NULL, vecLightingOrigin, 0, NULL, &vecDirection );
	float angularRatio = Engine_WorldLightAngle( pWorldLight, pWorldLight->normal, vecDirection, vecDirection );
	AddWorldLightToLightCube( pWorldLight, ambientCube, vecDirection, ratio * angularRatio );
}


static inline const byte* FastRejectLightSource( 
	bool bIgnoreVis, 
	const byte *pVis, 
	const Vector &bucketOrigin,
	int lightType,
	int lightCluster,
	bool &bReject )
{
	bReject = false;
	if( !bIgnoreVis )
	{
		// This is an optimization to avoid decompressing Vis twice
		if (!pVis)
		{
			// Figure out the PVS info for this location
			int bucketOriginLeaf = CM_PointLeafnum( bucketOrigin );
 			pVis = CM_ClusterPVS( CM_LeafCluster( bucketOriginLeaf ) );
		}
		if ( lightType == emit_skylight )
		{
 			int  bucketOriginLeaf = CM_PointLeafnum( bucketOrigin );
			mleaf_t *pLeaf = &host_state.worldbrush->leafs[bucketOriginLeaf];
			if ( pLeaf && !( pLeaf->flags & ( LEAF_FLAGS_SKY | LEAF_FLAGS_SKY2D ) ) )
			{
				bReject = true;
			}
		}
		else
		{
			if ((lightCluster < 0) || (!BIT_SET( pVis, lightCluster )) )
				bReject = true;
		}
	}
	return pVis;
}


//-----------------------------------------------------------------------------
// Adds a world light to the list of lights
//-----------------------------------------------------------------------------
static const byte *AddWorldLightToLightingState( dworldlight_t* pWorldLight, 
												 lightzbuffer_t* pZBuf,
												 LightingState_t& lightingState, LightingStateInfo_t& info,
												 const Vector& bucketOrigin, const byte* pVis, bool dynamic = false, 
												 bool bIgnoreVis = false,
												 bool bIgnoreVisTest = false )
{
	Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );

	// FIXME: This is sort of a hack; only one skylight is allowed in the
	// lighting...
	if ((pWorldLight->type == emit_skylight) && info.m_LightingStateHasSkylight)
		return pVis;

	// only do it if the entity can see into the lights leaf
	if ( !bIgnoreVisTest )
	{
		bool bReject;
		pVis = FastRejectLightSource( bIgnoreVis, pVis, bucketOrigin, pWorldLight->type, pWorldLight->cluster, bReject );
		if ( bReject )
			return pVis;
	}

	// Get the lighting ratio
	Vector direction;

	float ratio;

	if (!dynamic && r_oldlightselection.GetBool())
	{
		ratio = LightIntensityAndDirectionAtPoint( pWorldLight, pZBuf, bucketOrigin, 0, NULL, &direction );
	}
	else
	{
		Vector mins, maxs;
		ComputeLightcacheBounds( bucketOrigin, &mins, &maxs );
		ratio = LightIntensityAndDirectionInBox( pWorldLight, pZBuf, bucketOrigin, mins, maxs, dynamic ? LIGHT_NO_OCCLUSION_CHECK : 0, &direction );
	}

	// No light contribution? Get outta here!
	if ( ratio <= 0.0f )
		return pVis;

	// Check if we've got a skylight
	if (pWorldLight->type == emit_skylight)
		info.m_LightingStateHasSkylight = true;

	// Figure out spotlight attenuation
	float angularRatio = Engine_WorldLightAngle( pWorldLight, pWorldLight->normal, direction, direction );

	// Use standard RGB to gray conversion
	float illum = ratio * DotProduct( pWorldLight->intensity, s_Grayscale );	// Don't multiply by cone angle?

	// It can't be a local light if it's too dark
	// See the comment titled "EMIT_SURFACE LIGHTS" at the top for info.
	if (pWorldLight->type == emit_surface || illum >= r_worldlightmin.GetFloat()) // FIXME: tune this value
	{
		int nWorldLights = min( g_pMaterialSystemHardwareConfig->MaxNumLights(), r_worldlights.GetInt() );

		// if remaining slots, add to list
		if ( lightingState.numlights < nWorldLights )
		{
			// save pointer to world light
			lightingState.locallight[lightingState.numlights] = pWorldLight;
			info.m_pIllum[lightingState.numlights] = illum;
			++lightingState.numlights;
			return pVis;
		}

		// no remaining slots
		// find the dimmest existing light and replace 
		// If dynamic, make sure that it stays as a local light if possible.
		int minLightIndex = FindDarkestWorldLight( lightingState.numlights, info.m_pIllum, dynamic ? 100000 : illum );
		if (minLightIndex != -1)
		{
			// FIXME: We're sorting by ratio here instead of illum cause we either
			// have to store more memory or do more computations; I'm not
			// convinced it's any better of a metric though, so ratios for now...

			// found a light was was dimmer, swap it with the current light
			V_swap( pWorldLight, lightingState.locallight[minLightIndex] );
			V_swap( illum, info.m_pIllum[minLightIndex] );

			// FIXME: Avoid these recomputations 
			// But I don't know how to do it without storing a ton of data
			// per cache entry... yuck!

			// NOTE: We know the dot product can't be zero or illum would have been 0 to start with!
			ratio = illum / DotProduct( pWorldLight->intensity, s_Grayscale );

			if (pWorldLight->type == emit_skylight)
			{
				VectorFill( direction, 0 );
				angularRatio = 1.0f;
			}
			else
			{
				VectorSubtract( pWorldLight->origin, bucketOrigin, direction );
				VectorNormalize( direction );

				// Recompute the ratios
				angularRatio = Engine_WorldLightAngle( pWorldLight, pWorldLight->normal, direction, direction );
			}
		}
	}

	// Add the low light to the ambient box color
	AddWorldLightToLightCube( pWorldLight, lightingState.r_boxcolor, direction, ratio * angularRatio );
	return pVis;
}


//-----------------------------------------------------------------------------
// Construct a world light from a dynamic light
//-----------------------------------------------------------------------------
static void WorldLightFromDynamicLight( dlight_t const& dynamicLight, 
									   dworldlight_t& worldLight )
{
	VectorCopy( dynamicLight.origin, worldLight.origin );
	worldLight.type = emit_point;

	worldLight.intensity[0] = TexLightToLinear( dynamicLight.color.r, dynamicLight.color.exponent );
	worldLight.intensity[1] = TexLightToLinear( dynamicLight.color.g, dynamicLight.color.exponent );
	worldLight.intensity[2] = TexLightToLinear( dynamicLight.color.b, dynamicLight.color.exponent );
	worldLight.style = dynamicLight.style;

	// Compute cluster associated with the dynamic light
	worldLight.cluster = CM_LeafCluster( CM_PointLeafnum(worldLight.origin) );

	// Assume a quadratic attenuation factor; atten so we hit minlight
	// at radius away...
	float minlight = fpmax( dynamicLight.minlight, g_flMinLightingValue );
	// NOTE: Previous implementation turned off attenuation at radius zero.
	//		clamping is more continuous
	float radius = dynamicLight.GetRadius();
	if ( radius < 0.1f )
		radius = 0.1f;

	worldLight.constant_attn = 0;
	worldLight.linear_attn = 0; 
	worldLight.quadratic_attn = 1.0f / (minlight * radius * radius);

	// Set the max radius
	worldLight.radius = radius;

	// Spotlights...
	if (dynamicLight.m_OuterAngle > 0.0f)
	{
		worldLight.type = emit_spotlight;
		VectorCopy( dynamicLight.m_Direction, worldLight.normal );
		worldLight.stopdot = cos( dynamicLight.m_InnerAngle * M_PI / 180.0f );
		worldLight.stopdot2 = cos( dynamicLight.m_OuterAngle * M_PI / 180.0f );
	}
}


//-----------------------------------------------------------------------------
// Add in dynamic worldlights (lightstyles)
//-----------------------------------------------------------------------------
static const byte *ComputeLightStyles( lightcache_t* pCache, LightingState_t& lightingState,
									  const Vector& origin, int leaf, const byte* pVis )
{
	VPROF_INCREMENT_COUNTER( "ComputeLightStyles", 1 );
	LightingStateInfo_t info;

	lightingState.ZeroLightingState();
	// Next, add each world light with a lightstyle into the lighting state,
	// ejecting less relevant local lights + folding them into the ambient cube
	for ( int i = 0; i < host_state.worldbrush->numworldlights; ++i)
	{
		dworldlight_t *wl = &host_state.worldbrush->worldlights[i];
		if (wl->style == 0)
			continue;

		int byte = wl->style >> 3;
		int bit = wl->style & 0x7;
		if( !( pCache->m_pLightstyles[byte] & ( 1 << bit ) ) )
		{
			continue;
		}

		// This is an optimization to avoid decompressing Vis twice
		if (!pVis)
		{
			// Figure out the PVS info for this location
 			pVis = CM_ClusterPVS( CM_LeafCluster( leaf ) );
		}

		// Now add that world light into our list of worldlights
		AddWorldLightToLightingState( wl, NULL, lightingState, info, origin, pVis );
	}
	pCache->m_LastFrameUpdated_LightStyles = r_framecount;
	return pVis;
}


//-----------------------------------------------------------------------------
// Add in dynamic worldlights (lightstyles)
//-----------------------------------------------------------------------------
static void AddLightStylesForStaticProp( PropLightcache_t *pcache, LightingState_t& lightingState )
{
	// Next, add each world light with a lightstyle into the lighting state,
	// ejecting less relevant local lights + folding them into the ambient cube
	for( int i = 0; i < pcache->m_LightStyleWorldLights.Count(); ++i )
	{
		Assert( pcache->m_LightStyleWorldLights[i] >= 0 );
		Assert( pcache->m_LightStyleWorldLights[i] < host_state.worldbrush->numworldlights );
		dworldlight_t *wl = &host_state.worldbrush->worldlights[pcache->m_LightStyleWorldLights[i]];
		Assert( wl->style != 0 );

		// Now add that world light into our list of worldlights
		AddWorldLightToLightingState( wl, NULL, lightingState, *pcache, pcache->m_LightingOrigin, NULL, 
			false /*dynamic*/, true /*ignorevis*/ );
	}
}


//-----------------------------------------------------------------------------
// Add DLights + ELights to the dynamic lighting
//-----------------------------------------------------------------------------
static dworldlight_t s_pDynamicLight[MAX_DLIGHTS + MAX_ELIGHTS];

static const byte* AddDLights( LightingStateInfo_t& info, LightingState_t& lightingState, 
			const Vector& origin, int leaf, const byte* pVis )
{
	if ( !g_bActiveDlights )
		return pVis;

	const bool bIgnoreVis = false;
	const bool bIgnoreVisTest = true;

	// Next, add each world light with a lightstyle into the lighting state,
	// ejecting less relevant local lights + folding them into the ambient cube
	dlight_t* dl = cl_dlights;
	for ( int i=0; i<MAX_DLIGHTS; ++i, ++dl )
	{
		// If the light's not active, then continue
		if ( (r_dlightactive & (1 << i)) == 0 )
			continue;

		// If the light doesn't affect models, then continue
		if (dl->flags & (DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_DISPLACEMENT_MASK)) 
			continue;

		// Fast reject. If we can reject it here, then we don't have to call WorldLightFromDynamicLight..
		bool bReject;
		int lightCluster = CM_LeafCluster( g_DLightLeafAccessors[i].GetLeaf( dl->origin ) );
		pVis = FastRejectLightSource( bIgnoreVis, pVis, origin, emit_point, lightCluster, bReject );
		if ( bReject )
			continue;

		// Construct a world light representing the dynamic light
		// we're making a static list here because the lighting state
		// contains a set of pointers to dynamic lights
		WorldLightFromDynamicLight( *dl, s_pDynamicLight[i] );

		// Now add that world light into our list of worldlights
		pVis = AddWorldLightToLightingState( &s_pDynamicLight[i], NULL, lightingState,
			info, origin, pVis, true, bIgnoreVis, bIgnoreVisTest );
	}
	return pVis;
}

static const byte* AddELights( LightingStateInfo_t& info, LightingState_t& lightingState, 
			const Vector& origin, int leaf, const byte* pVis )
{
	if ( !g_bActiveElights )
		return pVis;
	
	const bool bIgnoreVis = false;
	const bool bIgnoreVisTest = true;
	
	// Next, add each world light with a lightstyle into the lighting state,
	// ejecting less relevant local lights + folding them into the ambient cube
	dlight_t* dl = cl_elights;
	for ( int i=0; i<MAX_ELIGHTS; ++i, ++dl )
	{
		// If the light's not active, then continue
		if ( !dl->IsRadiusGreaterThanZero() )
			continue;

		// If the light doesn't affect models, then continue
		if (dl->flags & (DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_DISPLACEMENT_MASK))
			continue;

		// Fast reject. If we can reject it here, then we don't have to call WorldLightFromDynamicLight..
		bool bReject;
		int lightCluster = CM_LeafCluster( g_ELightLeafAccessors[i].GetLeaf( dl->origin ) );
		pVis = FastRejectLightSource( bIgnoreVis, pVis, origin, emit_point, lightCluster, bReject );
		if ( bReject )
			continue;

		// Construct a world light representing the dynamic light
		// we're making a static list here because the lighting state
		// contains a set of pointers to dynamic lights
		WorldLightFromDynamicLight( *dl, s_pDynamicLight[i+MAX_DLIGHTS] );

		// Now add that world light into our list of worldlights
		pVis = AddWorldLightToLightingState( &s_pDynamicLight[i+MAX_DLIGHTS], NULL, lightingState,
			info, origin, pVis, true, bIgnoreVis, bIgnoreVisTest );
	}
	return pVis;
}


//-----------------------------------------------------------------------------
// Given static + dynamic lighting, figure out the total light
//-----------------------------------------------------------------------------
static const byte *ComputeDynamicLighting( lightcache_t* pCache, LightingState_t& lightingState, 
			const Vector& origin, int leaf, const byte* pVis = 0 )
{
	if (pCache->m_LastFrameUpdated_DynamicLighting != r_framecount)
	{
		VPROF_INCREMENT_COUNTER( "ComputeDynamicLighting", 1 );

		// First factor in the cache into the current lighting state..
		LightingStateInfo_t info;

		pCache->m_DynamicLightingState.ZeroLightingState();

		// Next, add each dlight one at a time 
		pVis = AddDLights( info, pCache->m_DynamicLightingState, origin, leaf, pVis );

		// Finally, add in elights
		// FIXME: Do we actually use these?
 		pVis = AddELights( info, pCache->m_DynamicLightingState, origin, leaf, pVis );

		pCache->m_LastFrameUpdated_DynamicLighting = r_framecount;
	}

	Assert( pCache->m_DynamicLightingState.numlights >= 0 && pCache->m_DynamicLightingState.numlights <= MAXLOCALLIGHTS );
	memcpy( &lightingState, &pCache->m_DynamicLightingState, sizeof(LightingState_t) );
	return pVis;
}

//-----------------------------------------------------------------------------
// Adds a world light to the list of lights
//-----------------------------------------------------------------------------
static void	AddWorldLightToLightingStateForStaticProps( dworldlight_t* pWorldLight, 
	LightingState_t& lightingState, LightingStateInfo_t& info, PropLightcache_t *pCache,
	bool dynamic = false )
{
	// FIXME: This is sort of a hack; only one skylight is allowed in the
	// lighting...
	if ((pWorldLight->type == emit_skylight) && info.m_LightingStateHasSkylight)
		return;

	// Get the lighting ratio
	float ratio;
	Vector direction;

	if (!dynamic)
	{
		ratio = LightIntensityAndDirectionAtPoint( pWorldLight, NULL, pCache->m_LightingOrigin, 0, NULL, &direction );
	}
	else
	{
		Vector mins, maxs;
		ComputeLightcacheBounds( pCache->m_LightingOrigin, &mins, &maxs );
		ratio = LightIntensityAndDirectionInBox( pWorldLight, NULL, pCache->m_LightingOrigin, 
			pCache->mins, pCache->maxs, LIGHT_NO_OCCLUSION_CHECK, &direction );
	}

	// No light contribution? Get outta here!
	if ( ratio <= 0.0f )
		return;

	// Check if we've got a skylight
	if (pWorldLight->type == emit_skylight)
		info.m_LightingStateHasSkylight = true;

	// Figure out spotlight attenuation
	float angularRatio = Engine_WorldLightAngle( pWorldLight, pWorldLight->normal, direction, direction );

	// Use standard RGB to gray conversion
	float illum = ratio * DotProduct( pWorldLight->intensity, s_Grayscale );	// Don't multiply by cone angle?

	// It can't be a local light if it's too dark
	// See the comment titled "EMIT_SURFACE LIGHTS" at the top for info.
	if (pWorldLight->type == emit_surface || illum >= r_worldlightmin.GetFloat()) // FIXME: tune this value
	{
		int nWorldLights = min( g_pMaterialSystemHardwareConfig->MaxNumLights(), r_worldlights.GetInt() );

		// if remaining slots, add to list
		if ( lightingState.numlights < nWorldLights )
		{
			// save pointer to world light
			lightingState.locallight[lightingState.numlights] = pWorldLight;
			info.m_pIllum[lightingState.numlights] = illum;
			++lightingState.numlights;
			return;
		}

		// no remaining slots
		// find the dimmest existing light and replace 
		int minLightIndex = FindDarkestWorldLight( lightingState.numlights, info.m_pIllum, dynamic ? 100000 : illum );
		if (minLightIndex != -1)
		{
			// FIXME: We're sorting by ratio here instead of illum cause we either
			// have to store more memory or do more computations; I'm not
			// convinced it's any better of a metric though, so ratios for now...

			// found a light was was dimmer, swap it with the current light
			V_swap( pWorldLight, lightingState.locallight[minLightIndex] );
			V_swap( illum, info.m_pIllum[minLightIndex] );

			// FIXME: Avoid these recomputations 
			// But I don't know how to do it without storing a ton of data
			// per cache entry... yuck!

			// NOTE: We know the dot product can't be zero or illum would have been 0 to start with!
			ratio = illum / DotProduct( pWorldLight->intensity, s_Grayscale );

			if (pWorldLight->type == emit_skylight)
			{
				VectorFill( direction, 0 );
				angularRatio = 1.0f;
			}
			else
			{
				VectorSubtract( pWorldLight->origin, pCache->m_LightingOrigin, direction );
				VectorNormalize( direction );

				// Recompute the ratios
				angularRatio = Engine_WorldLightAngle( pWorldLight, pWorldLight->normal, direction, direction );
			}
		}
	}

	// Add the low light to the ambient box color
	AddWorldLightToLightCube( pWorldLight, lightingState.r_boxcolor, direction, ratio * angularRatio );
}

static void AddDLightsForStaticProps( LightingStateInfo_t& info, LightingState_t& lightingState, 
			PropLightcache_t *pCache  )
{
	// mask off any dlights that have gone inactive
	pCache->m_DLightActive &= r_dlightactive;
	if ( pCache->m_DLightMarkFrame != r_framecount )
	{
		pCache->m_DLightActive = 0;
	}

	if ( !pCache->m_DLightActive )
		return;

	// Iterate the relevant dlights and add them to the lighting state
	dlight_t *dl = cl_dlights;
	for ( int i=0; i<MAX_DLIGHTS; ++i, ++dl )
	{
		// If the light doesn't affect this model, then continue.
		if( !( pCache->m_DLightActive & ( 1 << i ) ) )
			continue;

		// Construct a world light representing the dynamic light
		// we're making a static list here because the lighting state
		// contains a set of pointers to dynamic lights
		WorldLightFromDynamicLight( *dl, s_pDynamicLight[i] );

		// Now add that world light into our list of worldlights
		AddWorldLightToLightingStateForStaticProps( &s_pDynamicLight[i], lightingState,
			info, pCache, true );
	}
}

//-----------------------------------------------------------------------------
// Add static lighting to the lighting state
//-----------------------------------------------------------------------------


ConVar r_lightcache_zbuffercache( "r_lightcache_zbuffercache", "0", FCVAR_ALLOWED_IN_COMPETITIVE );

static void AddStaticLighting( 
	CBaseLightCache* pCache, 
	const Vector& origin, 
	const byte* pVis, 
	bool bStaticProp,
	bool bAddedLeafAmbientCube )
{
	VPROF( "AddStaticLighting" );
	// First, blat out the lighting state
	int i;
	pCache->m_StaticLightingState.numlights = 0;
	pCache->m_LightingStateHasSkylight = false;

	// NOTE: for static props, we mark lightstyles elsewhere (BuildStaticLightingCacheLightStyleInfo)
	if( !bStaticProp )
	{
		pCache->m_LightingFlags &= ~( HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE | 
							  HACKLIGHTCACHEFLAGS_HASNONSWITCHABLELIGHTSTYLE );
		memset( pCache->m_pLightstyles, 0, sizeof( pCache->m_pLightstyles ) );
	}
	
	// Next, add each static light one at a time into the lighting state,
	// ejecting less relevant local lights + folding them into the ambient cube
	// Also, we need to add *all* new lights into the total box color
	for (i = 0; i < host_state.worldbrush->numworldlights; ++i)
	{
		dworldlight_t *wl = &host_state.worldbrush->worldlights[i];
		lightzbuffer_t *pZBuf;
		if ( r_lightcache_zbuffercache.GetInt() )
			pZBuf = &host_state.worldbrush->shadowzbuffers[i];
		else
			pZBuf = NULL;

		// See the comment titled "EMIT_SURFACE LIGHTS" at the top for info.
		if ( bAddedLeafAmbientCube && (wl->flags & DWL_FLAGS_INAMBIENTCUBE) )
		{
			Assert( wl->type == emit_surface );
			continue;
		}

		// Don't add lights without lightstyles... we cache static lighting + lightstyles separately from static lighting
		if (wl->style == 0)
		{
			// Now add that world light into our list of worldlights
			AddWorldLightToLightingState( wl, pZBuf, pCache->m_StaticLightingState, *pCache, origin, pVis,  
				false, false );
		}
		else
		{
			// This is a lighstyle (flickering or switchable light)
			// NOTE: for static props, we mark lightstyles elsewhere. (BuildStaticLightingCacheLightStyleInfo)
			if( !bStaticProp )
			{
				int byte = wl->style >> 3;
				int bit = wl->style & 0x7;
				if( !( pCache->m_pLightstyles[byte] & ( 1 << bit ) ) )
				{
					Vector mins, maxs;
					Vector dummyDirection;
					ComputeLightcacheBounds( origin, &mins, &maxs );
					float ratio = LightIntensityAndDirectionInBox( wl, NULL, origin, mins, maxs, 
						LIGHT_NO_OCCLUSION_CHECK | LIGHT_IGNORE_LIGHTSTYLE_VALUE, &dummyDirection );
					// See if this light has any contribution on this cache entry.
					if( ratio > 0.0f )
					{
						if( d_lightstylenumframes[wl->style] <= 1 )
						{
							pCache->m_LightingFlags |= HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE;
						}
						else
						{
							pCache->m_LightingFlags |= HACKLIGHTCACHEFLAGS_HASNONSWITCHABLELIGHTSTYLE;
						}
						pCache->m_pLightstyles[byte] |= (1 << bit);
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Checks to see if the lightstyles are valid for this cache entry
//-----------------------------------------------------------------------------
static bool IsCachedLightStylesValid( CBaseLightCache* pCache )
{
	if (!pCache->HasLightStyle())
		return true;

	// FIXME: Can this start at 1, or is 0 required?
	for (int i = 1; i < MAX_LIGHTSTYLES; ++i)
	{
		int byte = i >> 3;
		int bit = i & 0x7;
		if (pCache->m_pLightstyles[byte] & ( 1 << bit ))
		{
			if (d_lightstyleframe[i] > pCache->m_LastFrameUpdated_LightStyles)
				return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Find a lightcache entry within the requested radius from a point
//-----------------------------------------------------------------------------
#if 0
static int FindRecentCacheEntryWithinRadius( int count, CacheInfo_t* pCache, const Vector& origin, float radius )
{
	radius *= radius;

	// Try to find something within the radius of an existing new sample
	int minIndex = -1;
	for (int i = 0; i < count; ++i)
	{
		Vector delta;
		ComputeLightcacheOrigin( pCache[i].x, pCache[i].y, pCache[i].z, delta );
		delta -= origin;
		float distSq = delta.LengthSqr();
		if (distSq < radius )
		{
			minIndex = i;
			radius = distSq;
		}
	}

	return minIndex;
}
#endif


//-----------------------------------------------------------------------------
// Draw the lightcache box for debugging
//-----------------------------------------------------------------------------
static void DebugRenderLightcache( Vector &sampleOrigin, LightingState_t& lightingState, bool bDebugModel )
{
#ifndef SWDS
	// draw the cache entry defined by the sampling origin
	Vector cacheOrigin, cacheMins, cacheMaxs, lightMins, lightMaxs;
	ComputeLightcacheBounds( sampleOrigin, &cacheMins, &cacheMaxs );
	cacheOrigin  = ( cacheMins + cacheMaxs ) * 0.5f;
	cacheMins   -= cacheOrigin;
	cacheMaxs   -= cacheOrigin;

	// For drawing irradiance light probes as shown in [Greger98]
	if( r_drawlightcache.GetInt() == 5 )
	{
		if ( bDebugModel )
		{
			CDebugOverlay::AddSphereOverlay( sampleOrigin, 2.5f, 32, 32, 255, 255, 255, 255, 0.0f );	// 8 inch solid white sphere

			for ( int j = 0; j < lightingState.numlights; ++j )
			{
				Vector vLightPosition;
				int r, g, b;

				if ( lightingState.locallight[j]->type == emit_skylight )
				{
					vLightPosition = sampleOrigin - lightingState.locallight[j]->normal * 10000.0f;
					r = 255;
					g = 50;
					b = 50;
				}
				else
				{
					vLightPosition = lightingState.locallight[j]->origin;
					r = 255;
					g = 255;
					b = 255;
				}

				CDebugOverlay::AddLineOverlay( sampleOrigin, vLightPosition, r, g, b, 255, true, 0.0f );
			}
		}
	}
	else
	{
		// draw cache entry
		CDebugOverlay::AddBoxOverlay( cacheOrigin, cacheMins, cacheMaxs, vec3_angle, 255, 255, 255, 0, 0.0f );

		// draw boxes at light ray terminals to visualize endpoints
		if ( lightingState.numlights > 0 )
		{
			lightMins.Init( -2, -2, -2 );
			lightMaxs.Init( 2, 2, 2);

			CDebugOverlay::AddBoxOverlay( sampleOrigin, lightMins, lightMaxs, vec3_angle, 100, 255, 100, 0, 0.0f );
		}

		int nLineColor[4] = {255, 170, 85, 0};

		for (int j = 0; j < lightingState.numlights; ++j)
		{
			Vector vLightPosition;
			int r, g, b;

			if ( lightingState.locallight[j]->type == emit_skylight )
			{
				vLightPosition = sampleOrigin - lightingState.locallight[j]->normal * 10000.0f;
				r = 255;
				g = 50;
				b = 50;
			}
			else
			{
				vLightPosition = lightingState.locallight[j]->origin;
				r = g = b = nLineColor[j];
			}

			// draw lines from sampling point to light
			CDebugOverlay::AddLineOverlay( sampleOrigin, vLightPosition, r, g, b, 255, true, 0.0f );
			CDebugOverlay::AddBoxOverlay( lightingState.locallight[j]->origin, lightMins, lightMaxs, vec3_angle, 255, 255, 100, 0, 0.0f );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Identify lighting errors
//-----------------------------------------------------------------------------
bool IdentifyLightingErrors( int leaf, LightingState_t& lightingState )
{
	if (r_drawlightcache.GetInt() == 3)
	{
		if (CM_LeafContents(leaf) == CONTENTS_SOLID)
		{
			// Try another choice...
			lightingState.r_boxcolor[0].Init( 1, 0, 0 );
			lightingState.r_boxcolor[1].Init( 1, 0, 0 );
			lightingState.r_boxcolor[2].Init( 1, 0, 0 );
			lightingState.r_boxcolor[3].Init( 1, 0, 0 );
			lightingState.r_boxcolor[4].Init( 1, 0, 0 );
			lightingState.r_boxcolor[5].Init( 1, 0, 0 );
			lightingState.numlights = 0;
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Compute the cache...
//-----------------------------------------------------------------------------
static const byte* ComputeStaticLightingForCacheEntry( CBaseLightCache *pcache, const Vector& origin, int leaf, bool bStaticProp = false )
{
	VPROF_INCREMENT_COUNTER( "ComputeStaticLightingForCacheEntry", 1 );
	
	VPROF( "ComputeStaticLightingForCacheEntry" );
	// Figure out the PVS info for this location
 	const byte* pVis = CM_ClusterPVS( CM_LeafCluster( leaf ) );

	bool bAddedLeafAmbientCube;

	R_StudioGetAmbientLightForPoint( 
		leaf, 
		origin, 
		pcache->m_StaticLightingState.r_boxcolor,
		bStaticProp,
		&bAddedLeafAmbientCube );

	// get direct lighting from world light sources (point lights, etc.)
	if ( !r_ambientlightingonly.GetInt() )
	{
		AddStaticLighting( pcache, origin, pVis, bStaticProp, bAddedLeafAmbientCube );
	}

	return pVis;
}


static void BuildStaticLightingCacheLightStyleInfo( PropLightcache_t* pcache, const Vector& mins, const Vector& maxs )
{
	const byte *pVis = NULL;
	Assert( pcache->m_LightStyleWorldLights.Count() == 0 );
	pcache->m_LightingFlags &= ~( HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE | HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE );
	// clear lightstyles
	memset( pcache->m_pLightstyles, 0, MAX_LIGHTSTYLE_BYTES );
	for ( short i = 0; i < host_state.worldbrush->numworldlights; ++i)
	{
		dworldlight_t *wl = &host_state.worldbrush->worldlights[i];
		if (wl->style == 0)
			continue;

		// This is an optimization to avoid decompressing Vis twice
		if (!pVis)
		{
			// Figure out the PVS info for this static prop
 			pVis = CM_ClusterPVS( CM_LeafCluster( pcache->leaf ) );
		}
		// FIXME: Could do better here if we had access to the list of leaves that this
		// static prop is in.  For now, we use the lighting origin.
		if( pVis[ wl->cluster >> 3 ] & ( 1 << ( wl->cluster & 7 ) ) )
		{
			// Use the maximum illumination to cull out lights that are far away.
			dworldlight_t tmpLight = *wl;
			tmpLight.style = 0;
			Vector dummyDirection;
			float ratio = LightIntensityAndDirectionInBox( &tmpLight, NULL, pcache->m_LightingOrigin, mins, maxs, 
				LIGHT_NO_OCCLUSION_CHECK | LIGHT_IGNORE_LIGHTSTYLE_VALUE, &dummyDirection );
			// See if this light has any contribution on this cache entry.
			if( ratio <= 0.0f )
			{
				continue;
			}

			{
			MEM_ALLOC_CREDIT();
			pcache->m_LightStyleWorldLights.AddToTail( i );
			}

			int byte = wl->style >> 3;
			int bit = wl->style & 0x7;
			pcache->m_pLightstyles[byte] |= ( 1 << bit );
			if( d_lightstylenumframes[wl->style] <= 1 )
			{
				pcache->m_LightingFlags |= HACKLIGHTCACHEFLAGS_HASSWITCHABLELIGHTSTYLE;
			}
			else
			{
				pcache->m_LightingFlags |= HACKLIGHTCACHEFLAGS_HASNONSWITCHABLELIGHTSTYLE;
			}
		}
	}
}

static ITexture *FindEnvCubemapForPoint( const Vector& origin )
{
	worldbrushdata_t *pBrushData = host_state.worldbrush;
	if( pBrushData && pBrushData->m_nCubemapSamples > 0 )
	{
		int smallestIndex = 0;
		Vector blah = origin - pBrushData->m_pCubemapSamples[0].origin;
		float smallestDist = DotProduct( blah, blah );
		for( int i = 1; i < pBrushData->m_nCubemapSamples; i++ )
		{
			Vector ign = origin - pBrushData->m_pCubemapSamples[i].origin;
			float dist = DotProduct( ign, ign );
			if( dist < smallestDist )
			{
				smallestDist = dist;
				smallestIndex = i;
			}
		}
		
		return pBrushData->m_pCubemapSamples[smallestIndex].pTexture;
	}
	else
	{
		return NULL;
	}
}

//-----------------------------------------------------------------------------
// Create static light cache entry
//-----------------------------------------------------------------------------
LightCacheHandle_t CreateStaticLightingCache( const Vector& origin, const Vector& mins, const Vector& maxs )
{
	PropLightcache_t* pcache = s_PropCache.Alloc();

	pcache->m_LightingOrigin = origin;
	pcache->m_Flags = 0;

	pcache->mins = mins;
	pcache->maxs = maxs;

	// initialize this to point to our current origin
	pcache->leaf = CM_PointLeafnum(origin);

	// Add the prop to the list of props
	pcache->m_pNextPropLightcache = s_pAllStaticProps;
	s_pAllStaticProps = pcache;
	
	pcache->m_Flags = 0; // must set this to zero so that this cache entry will be invalid.
	pcache->m_pEnvCubemapTexture = FindEnvCubemapForPoint( origin );
	BuildStaticLightingCacheLightStyleInfo( pcache, mins, maxs );
	return (LightCacheHandle_t)pcache;
}

bool StaticLightCacheAffectedByDynamicLight( LightCacheHandle_t handle )
{
	PropLightcache_t *pcache = ( PropLightcache_t *)handle;
	return pcache->HasDlights();
}

bool StaticLightCacheAffectedByAnimatedLightStyle( LightCacheHandle_t handle )
{
	PropLightcache_t *pcache = ( PropLightcache_t *)handle;
	if( !pcache->HasLightStyle() )
	{
		return false;
	}
	else
	{
		for( int i = 0; i < pcache->m_LightStyleWorldLights.Count(); ++i )
		{
			Assert( pcache->m_LightStyleWorldLights[i] >= 0 );
			Assert( pcache->m_LightStyleWorldLights[i] < host_state.worldbrush->numworldlights );
			dworldlight_t *wl = &host_state.worldbrush->worldlights[pcache->m_LightStyleWorldLights[i]];
			Assert( wl->style != 0 );
			if( d_lightstylenumframes[wl->style] > 1 )
			{
				return true;
			}
		}
		return false;
	}
}

bool StaticLightCacheNeedsSwitchableLightUpdate( LightCacheHandle_t handle )
{
	PropLightcache_t *pcache = ( PropLightcache_t *)handle;
	if( !pcache->HasSwitchableLightStyle() )
	{
		return false;
	}
	else
	{
		for( int i = 0; i < pcache->m_LightStyleWorldLights.Count(); ++i )
		{
			Assert( pcache->m_LightStyleWorldLights[i] >= 0 );
			Assert( pcache->m_LightStyleWorldLights[i] < host_state.worldbrush->numworldlights );
			dworldlight_t *wl = &host_state.worldbrush->worldlights[pcache->m_LightStyleWorldLights[i]];
			Assert( wl->style != 0 );
			// Is it a switchable light?
			if( d_lightstylenumframes[wl->style] <= 1 )
			{
				// Has it changed since the last time we updated our cached static VB version?
				if( pcache->m_SwitchableLightFrame < d_lightstyleframe[wl->style] )
				{
					pcache->m_SwitchableLightFrame = r_framecount;
					// return true since our static vb is dirty
					return true;
				}
			}
		}
		return false;
	}
}

//-----------------------------------------------------------------------------
// Clears the prop lighting cache
//-----------------------------------------------------------------------------
void ClearStaticLightingCache()
{
	s_PropCache.Clear();
	s_pAllStaticProps = NULL;
}


//-----------------------------------------------------------------------------
// Recomputes all static prop lighting
//-----------------------------------------------------------------------------
void InvalidateStaticLightingCache(void)
{
	for ( PropLightcache_t *pCur=s_pAllStaticProps; pCur; pCur=pCur->m_pNextPropLightcache )
	{
		// Compute the static lighting
		pCur->m_Flags = 0;
		pCur->m_LightingFlags &=~HACKLIGHTCACHEFLAGS_HASDONESTATICLIGHTING;
		
		LightcacheGetStatic( ( LightCacheHandle_t )pCur, NULL, LIGHTCACHEFLAGS_STATIC );
	}
}

//-----------------------------------------------------------------------------
// Gets the lightcache entry for a static prop
//-----------------------------------------------------------------------------
LightingState_t *LightcacheGetStatic( LightCacheHandle_t cache, ITexture **pEnvCubemapTexture, unsigned int flags )
{
	PropLightcache_t *pcache = ( PropLightcache_t * )cache;
	Assert( pcache );

	// get the cubemap texture
	if ( pEnvCubemapTexture )
	{
		*pEnvCubemapTexture = pcache->m_pEnvCubemapTexture;
	}

	bool bRecalcStaticLighting = false;
	bool bRecalcLightStyles = (pcache->HasLightStyle() && pcache->m_LastFrameUpdated_LightStyles != r_framecount) && !IsCachedLightStylesValid(pcache);
	bool bRecalcDLights = pcache->HasDlights() && pcache->m_LastFrameUpdated_DynamicLighting != r_framecount;

	if ( flags != pcache->m_Flags )
	{
		// This should not happen often, but if the flags change, blow away all of the lighting state.
		// This cache entry's state must be regenerated.
		bRecalcStaticLighting = true;
		bRecalcLightStyles = true;
		bRecalcDLights = true;

		pcache->m_Flags = flags;
	}
	else if ( !bRecalcDLights && !bRecalcLightStyles )
	{
		// already have expected lighting state
		// get out of here since we already did this this frame.
		return &pcache->m_DynamicLightingState;
	}
	else
	{
		// the dlight cache includes lightstyles
		// we have to recalc the dlight cache if lightstyles change.
		if ( bRecalcLightStyles )
		{
			bRecalcDLights = true;
		}
	}

	// must need to recalc, do so
	
	LightingState_t	accumulatedState;

	// static lighting state gets preserved because its expensive to generate
	// it gets re-requested for static props that rebake
	if ( flags & LIGHTCACHEFLAGS_STATIC )
	{
		// want static lighting data
		if ( bRecalcStaticLighting && !(pcache->m_LightingFlags & HACKLIGHTCACHEFLAGS_HASDONESTATICLIGHTING) )
		{	
			ComputeStaticLightingForCacheEntry( pcache, pcache->m_LightingOrigin, pcache->leaf, true );
			pcache->m_LightingFlags |= HACKLIGHTCACHEFLAGS_HASDONESTATICLIGHTING;
		}

		// set as start values for accumulation
		accumulatedState = pcache->m_StaticLightingState;
	}
	else
	{
		// set as zero for accumulation
		accumulatedState.ZeroLightingState();
	}

	// lightstyle lighting state gets preserved when there is no lightstyle change
	if ( flags & LIGHTCACHEFLAGS_LIGHTSTYLE )
	{
		if ( bRecalcLightStyles )
		{
			// accumulate lightstyles
			AddLightStylesForStaticProp( pcache, accumulatedState );
			pcache->m_LightStyleLightingState = accumulatedState;
			pcache->m_LastFrameUpdated_LightStyles = r_framecount;
		}
		else
		{
			accumulatedState = pcache->m_LightStyleLightingState;
		}
	}

	if ( flags & LIGHTCACHEFLAGS_DYNAMIC )
	{
		if ( bRecalcDLights )
		{	
			// accumulate dynamic lights
			AddDLightsForStaticProps( *( LightingStateInfo_t *)pcache, accumulatedState, pcache );
			pcache->m_DynamicLightingState = accumulatedState;
			pcache->m_LastFrameUpdated_DynamicLighting = r_framecount;
		}
		else
		{
			accumulatedState = pcache->m_DynamicLightingState;
		}
	}
	else
	{
		// hold the current state
		pcache->m_DynamicLightingState = accumulatedState;
	}

	// caller gets requested data
	return &pcache->m_DynamicLightingState;
}


inline const byte *AddLightingState( 
	LightingState_t &dst, 
	const LightingState_t &src, 
	LightingStateInfo_t &info,
	const Vector& bucketOrigin, 
	const byte *pVis, 
	bool bDynamic, 
	bool bIgnoreVis )
{
	int i;
	for( i = 0; i < src.numlights; i++ )
	{
		pVis = AddWorldLightToLightingState( src.locallight[i], NULL, dst, info, bucketOrigin, pVis, 
			bDynamic, bIgnoreVis );
	}
	for( i = 0; i < 6; i++ )
	{
		dst.r_boxcolor[i] += src.r_boxcolor[i];
	}
	return pVis;
}

//-----------------------------------------------------------------------------
// Get or create the lighting information for this point
// This is the version for dynamic objects.
//-----------------------------------------------------------------------------

const byte* PrecalcLightingState( lightcache_t *pCache, const byte *pVis )
{
	LightingState_t lightingState;
	lightingState.ZeroLightingState();
	
	pCache->m_StaticPrecalc_LightingStateInfo.Clear();

	int i;
	for( i = 0; i < pCache->m_StaticLightingState.numlights; i++ )
	{
		pVis = AddWorldLightToLightingState( 
			pCache->m_StaticLightingState.locallight[i], 
			NULL,
			lightingState, 
			pCache->m_StaticPrecalc_LightingStateInfo, 
			pCache->m_LightingOrigin, 
			pVis, 
			true, // bDynamic
			false // bIgnoreVis
			);
	}

	for ( i=0; i < 6; i++ )
		pCache->m_StaticLightingState.r_boxcolor[i] += lightingState.r_boxcolor[i];
		
	pCache->m_StaticPrecalc_NumLocalLights = lightingState.numlights;
	for ( i=0; i < pCache->m_StaticPrecalc_NumLocalLights; i++ )
		pCache->m_StaticPrecalc_LocalLight[i] = lightingState.locallight[i];
		
	return pVis;	
}


void CopyPrecalcedLightingState( lightcache_t *pCache, LightingState_t &lightingState, LightingStateInfo_t &info )
{
	info = pCache->m_StaticPrecalc_LightingStateInfo;
	
	int i;
	for ( i=0; i < 6; i++ )
		lightingState.r_boxcolor[i] = pCache->m_StaticLightingState.r_boxcolor[i];
	
	lightingState.numlights = pCache->m_StaticPrecalc_NumLocalLights;
	for ( i=0; i < lightingState.numlights; i++ )
		lightingState.locallight[i] = pCache->m_StaticPrecalc_LocalLight[i];
}


void AdjustLightCacheOrigin( lightcache_t *pCache, const Vector &origin, int originLeaf )
{
	Vector cacheMins;
	Vector cacheMaxs;
	Vector center;
	trace_t tr;
	Ray_t ray;
	CTraceFilterWorldOnly worldTraceFilter;
	ITraceFilter *pTraceFilter = &worldTraceFilter;

	// quiet compiler
	tr.startsolid = false;
	tr.fraction   = 0;

	// prefer to use the center of the light cache for all sampling
	// which helps consistent stable cache entries
	ComputeLightcacheBounds( origin, &cacheMins, &cacheMaxs );
	center  = cacheMins + cacheMaxs;
	center *= 0.5f;

	bool bTraceToCenter = true;
	int centerLeaf = CM_PointLeafnum(center);
	if (centerLeaf != originLeaf)
	{
		// preferred center resides in a different leaf
		if (CM_LeafContents(centerLeaf) & MASK_OPAQUE)
		{
			// preferred center is invalid
			bTraceToCenter = false;
		}
		else
		{	
			// ensure the desired center resides in the leaf that the provided origin is in
			CM_SnapPointToReferenceLeaf(origin, LIGHTCACHE_SNAP_EPSILON, &center);
		}
	}


	if (bTraceToCenter)
	{
		// if the center is unavailable, fallback to provided origin
		ray.Init( origin, center );
		g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, pTraceFilter, &tr );
	}

	if (bTraceToCenter && tr.startsolid)
	{
		// origin is in solid, can't trace anywhere, use bad origin as provided
		VectorCopy(origin, pCache->m_LightingOrigin);
	}
	else if (!bTraceToCenter || tr.fraction < 1)
	{
		// center is occluded
		// trace again, recompute alternate x-y center, substitute z
		// ensure the desired center resides in the leaf that the provided origin is in
		center.x = (cacheMins.x + cacheMaxs.x) * 0.5f;
		center.y = (cacheMins.y + cacheMaxs.y) * 0.5f;		
		center.z = origin.z;
		CM_SnapPointToReferenceLeaf(origin, LIGHTCACHE_SNAP_EPSILON, &center);

		ray.Init( origin, center );
		g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, pTraceFilter, &tr );
		if (tr.fraction < 1)
		{
			// no further fallback, use origin as provided
			VectorCopy(origin, pCache->m_LightingOrigin);
		}
		else
		{
			// trace succeeded
			VectorCopy(center, pCache->m_LightingOrigin);
		}
	}
	else
	{
		// trace succeeded
		VectorCopy(center, pCache->m_LightingOrigin);
	}
}

bool AllowFullCacheMiss(int flags)
{
	if ( r_framecount < 60 || r_framecount != g_FrameIndex )
	{
		g_FrameMissCount = 0;
		g_FrameIndex = r_framecount;
	}
	if ( g_FrameMissCount < lightcache_maxmiss.GetInt() )
	{
		g_FrameMissCount++;
		return true;
	}
	
	if ( flags & LIGHTCACHEFLAGS_ALLOWFAST )
		return false;

	return true;
}

lightcache_t *FindNearestCache( int x, int y, int z, int leafIndex )
{
	int bestDist = INT_MAX;
	lightcache_t *pBest = NULL;
	short current = GetLightLRUTail().lru_prev;
	int dx, dy, dz;
	while ( current != LIGHT_LRU_HEAD_INDEX )
	{
		lightcache_t *pCache = &lightcache[current];
		int dist = 0;
		dx = pCache->x - x;
		dx = abs(dx);
		dy = pCache->y - y;
		dy = abs(dy);
		dz = pCache->z - z;
		dz = abs(dz);
		if ( leafIndex != pCache->leaf )
		{
			dist += 2;
		}
		dist = max(dist, dx);
		dist = max(dist, dy);
		dist = max(dist, dz);
		if ( dist < bestDist )
		{
			pBest = pCache;
			bestDist = dist;
			if ( dist <= 1 )
				break;
		}
		current = pCache->lru_prev;
	}
	return pBest;
}


ITexture *LightcacheGetDynamic( const Vector& origin, LightingState_t& lightingState, 
							   LightcacheGetDynamic_Stats &stats, unsigned int flags, bool bDebugModel )
{
	VPROF_BUDGET( "LightcacheGet", VPROF_BUDGETGROUP_LIGHTCACHE );

	LightingStateInfo_t info;

	// generate the hashing vars
	int originLeaf = CM_PointLeafnum(origin);

	/*
	if (IdentifyLightingErrors(leaf, lightingState))
	return false;
	*/
	
	int x, y, z;
	OriginToCacheOrigin( origin, x, y, z );

	// convert vars to hash key / bucket id
	int bucket = LightcacheHashKey( x, y, z, originLeaf );
	
	const byte* pVis = NULL;
	bool bComputeLightStyles = ( flags & LIGHTCACHEFLAGS_LIGHTSTYLE ) != 0;

	// See if we've already computed the light in this location
	lightcache_t *pCache = FindInCache(bucket, x, y, z, originLeaf);

	if ( pCache )
	{
		// cache hit, move to tail of LRU
		LightcacheMark( pCache );
				
		if ( bComputeLightStyles && IsCachedLightStylesValid( pCache ) )
		{
			bComputeLightStyles = false;
		}
	}
	else if ( !AllowFullCacheMiss(flags) )
	{
		pCache = FindNearestCache( x, y, z, originLeaf );
		originLeaf = pCache->leaf;

		x = pCache->x;
		y = pCache->y;
		z = pCache->z;
	}
	if ( !pCache )
	{
		VPROF_INCREMENT_COUNTER( "lightcache miss", 1 );

		// cache miss, nothing appropriate from the frame cache, make a new entry
		pCache = NewLightcacheEntry(bucket);
		
		// initialize the cache entry based on provided origin
		pCache->x    = x;
		pCache->y    = y;
		pCache->z    = z;
		pCache->leaf = originLeaf;

		if ( r_lightcachecenter.GetBool() )
		{
			AdjustLightCacheOrigin( pCache, origin, originLeaf );
		}
		else
		{
			// old behavior, use provided origin
			VectorCopy(origin, pCache->m_LightingOrigin);
		}

		// Figure out which env_cubemap is used for this cache entry.
		pCache->m_pEnvCubemapTexture = FindEnvCubemapForPoint( pCache->m_LightingOrigin );
		
		// Compute the static portion of the cache
		pVis = ComputeStaticLightingForCacheEntry( pCache, pCache->m_LightingOrigin, originLeaf );	
		pVis = PrecalcLightingState( pCache, pVis );
	}

	// NOTE: On a cache miss, this has to be after ComputeStaticLightingForCacheEntry since these flags are computed there.
	stats.m_bHasNonSwitchableLightStyles = pCache->HasNonSwitchableLightStyle();
	stats.m_bHasSwitchableLightStyles    = pCache->HasSwitchableLightStyle();
	
	if ( bComputeLightStyles )
	{
		pVis = ComputeLightStyles( pCache, pCache->m_LightStyleLightingState, pCache->m_LightingOrigin, originLeaf, pVis );
		stats.m_bNeedsSwitchableLightStyleUpdate = true;
	}
	else
	{
		stats.m_bNeedsSwitchableLightStyleUpdate = false;
	}
	
	stats.m_bHasDLights = false;
	if ( flags & LIGHTCACHEFLAGS_DYNAMIC )
	{
		pVis = ComputeDynamicLighting( pCache, pCache->m_DynamicLightingState, pCache->m_LightingOrigin, originLeaf, pVis );
		if( pCache->m_DynamicLightingState.numlights > 0 )
		{
			stats.m_bHasDLights = true;
		}
	}

	if ( flags & LIGHTCACHEFLAGS_STATIC )
	{
		CopyPrecalcedLightingState( pCache, lightingState, info );
	}
	else
	{
		lightingState.ZeroLightingState();
	}
	
	if ( flags & LIGHTCACHEFLAGS_LIGHTSTYLE )
	{
		pVis = AddLightingState( lightingState, pCache->m_LightStyleLightingState, info, pCache->m_LightingOrigin, pVis,
			true /*bDynamic*/, false /*bIgnoreVis*/ );
	}
	
	if ( flags & LIGHTCACHEFLAGS_DYNAMIC )
	{
		pVis = AddLightingState( lightingState, pCache->m_DynamicLightingState, info, pCache->m_LightingOrigin, pVis,
			true /*bDynamic*/, false /*bIgnoreVis*/ );
	}
	
	if ( r_drawlightcache.GetBool() )
	{
		DebugRenderLightcache( pCache->m_LightingOrigin, lightingState, bDebugModel );
	}

	return pCache->m_pEnvCubemapTexture;
}

//-----------------------------------------------------------------------------
// Compute the contribution of D- and E- lights at a point + normal
//-----------------------------------------------------------------------------
void ComputeDynamicLighting( const Vector& pt, const Vector* pNormal, Vector& color )
{
	if ( !g_bActiveDlights && !g_bActiveElights )
	{
		VectorFill( color, 0 );
		return;
	}

	// Next, add each world light with a lightstyle into the lighting state,
	// ejecting less relevant local lights + folding them into the ambient cube
	static Vector ambientTerm[6] = 
	{
		Vector(0,0,0),
		Vector(0,0,0),
		Vector(0,0,0),
		Vector(0,0,0),
		Vector(0,0,0),
		Vector(0,0,0)
	};

	int lightCount = 0;
	LightDesc_t pLightDesc[MAX_DLIGHTS + MAX_ELIGHTS];

	int i;
	dlight_t* dl = cl_dlights;
	if ( g_bActiveDlights )
	{
		for ( i=0; i<MAX_DLIGHTS; ++i, ++dl )
		{
			// If the light's not active, then continue
			if ( (r_dlightactive & (1 << i)) == 0 )
				continue;

			// If the light doesn't affect models, then continue
			if (dl->flags & (DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_DISPLACEMENT_MASK))
				continue;

			// Construct a world light representing the dynamic light
			// we're making a static list here because the lighting state
			// contains a set of pointers to dynamic lights
			dworldlight_t worldLight;
			WorldLightFromDynamicLight( *dl, worldLight );
			WorldLightToMaterialLight( &worldLight, pLightDesc[lightCount] );
			++lightCount;
		}
	}

	if ( g_bActiveElights )
	{
		// Next, add each world light with a lightstyle into the lighting state,
		// ejecting less relevant local lights + folding them into the ambient cube
		dl = cl_elights;
		for ( i=0; i<MAX_ELIGHTS; ++i, ++dl )
		{
			// If the light's not active, then continue
			if ( !dl->IsRadiusGreaterThanZero() )
				continue;

			// If the light doesn't affect models, then continue
			if (dl->flags & (DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_DISPLACEMENT_MASK))
				continue;

			// Construct a world light representing the dynamic light
			// we're making a static list here because the lighting state
			// contains a set of pointers to dynamic lights
			dworldlight_t worldLight;
			WorldLightFromDynamicLight( *dl, worldLight );
			WorldLightToMaterialLight( &worldLight, pLightDesc[lightCount] );
			++lightCount;
		}
	}

	if ( lightCount )
	{
		g_pStudioRender->ComputeLighting( ambientTerm, lightCount,
			pLightDesc, pt, *pNormal, color );
	}
	else
	{
		VectorFill( color, 0 );
	}
}


//-----------------------------------------------------------------------------
// Is Dynamic Light?
//-----------------------------------------------------------------------------
static bool IsDynamicLight( dworldlight_t *pWorldLight )
{
	// NOTE: This only works because we're using some implementation details
	// that the dynamic lights are stored in a little static array
	return ( pWorldLight >= s_pDynamicLight && pWorldLight < &s_pDynamicLight[ARRAYSIZE(s_pDynamicLight)] );
}


//-----------------------------------------------------------------------------
// Computes an average color (of sorts) at a particular point + optional normal
//-----------------------------------------------------------------------------
void ComputeLighting( const Vector& pt, const Vector* pNormal, bool bClamp, Vector& color, Vector *pBoxColors )
{
	LightingState_t lightingState;
	LightcacheGetDynamic_Stats stats;
	LightcacheGetDynamic( pt, lightingState, stats, LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST );
	int i;
	if ( pNormal )
	{
		LightDesc_t* pLightDesc = (LightDesc_t*)stackalloc( lightingState.numlights * sizeof(LightDesc_t) );
		
		for ( i=0; i < lightingState.numlights; ++i )
		{
			// Construct a world light representing the dynamic light
			// we're making a static list here because the lighting state
			// contains a set of pointers to dynamic lights
			WorldLightToMaterialLight( lightingState.locallight[i], pLightDesc[i] );
		}

		g_pStudioRender->ComputeLighting( lightingState.r_boxcolor, lightingState.numlights, pLightDesc, pt, *pNormal, color );
	}
	else
	{
		Vector direction;

		for ( i = 0; i < lightingState.numlights; ++i )
		{
			if ( IsDynamicLight( lightingState.locallight[i] ) )
				continue;

			float ratio = LightIntensityAndDirectionAtPoint( lightingState.locallight[i], NULL, pt, LIGHT_NO_OCCLUSION_CHECK, NULL, &direction );
			float angularRatio = Engine_WorldLightAngle( lightingState.locallight[i], lightingState.locallight[i]->normal, direction, direction );
			AddWorldLightToLightCube( lightingState.locallight[i], lightingState.r_boxcolor, direction, ratio * angularRatio );
		}

		color.Init( 0, 0, 0 );
		for ( i = 0; i < 6; ++i )
		{
			color += lightingState.r_boxcolor[i];
		}
		color /= 6.0f;
	}

	// If they want the colors for each box side, give it to them.
	if ( pBoxColors )
	{
		memcpy( pBoxColors, lightingState.r_boxcolor, sizeof( lightingState.r_boxcolor ) );
	}

	if (bClamp)
	{
		if (color.x > 1.0f)
			color.x = 1.0f;
		if (color.y > 1.0f)
			color.y = 1.0f;
		if (color.z > 1.0f)
			color.z = 1.0f;
	}
}


static const byte *s_pDLightVis = NULL;

// All dlights that affect a static prop must mark that static prop every frame.
class MarkStaticPropLightsEmumerator : public IPartitionEnumerator
{
public:
	void SetLightID( int nLightID )
	{
		m_nLightID = nLightID;
	}

	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		Assert( StaticPropMgr()->IsStaticProp( pHandleEntity ) );

		PropLightcache_t *pCache = 
			( PropLightcache_t * )StaticPropMgr()->GetLightCacheHandleForStaticProp( pHandleEntity );

		if ( !pCache )
		{
			return ITERATION_CONTINUE;
		}

		if( !s_pDLightVis )
		{
			s_pDLightVis = CM_ClusterPVS( CM_LeafCluster( CM_PointLeafnum( cl_dlights[m_nLightID].origin ) ) );
		}

		if( !StaticPropMgr()->IsPropInPVS( pHandleEntity, s_pDLightVis ) )
		{
			return ITERATION_CONTINUE;
		}

		if ( !pCache )
		{
			return ITERATION_CONTINUE;
		}

#ifdef _DEBUG
		if( r_drawlightcache.GetInt() == 4 )
		{
			Vector mins( -5, -5, -5 );
			Vector maxs( 5, 5, 5 );
			CDebugOverlay::AddLineOverlay( cl_dlights[m_nLightID].origin, pCache->m_LightingOrigin, 0, 0, 255, 255, true, 0.001f );
			CDebugOverlay::AddBoxOverlay( pCache->m_LightingOrigin, mins, maxs, vec3_angle, 255, 0, 0, 0, 0.001f );
		}
#endif


		pCache->m_DLightActive |= ( 1 << m_nLightID );
		pCache->m_DLightMarkFrame = r_framecount;
		return ITERATION_CONTINUE;
	}

private:
	int m_nLightID;
};

static MarkStaticPropLightsEmumerator s_MarkStaticPropLightsEnumerator;

void MarkDLightsOnStaticProps( void )
{
	if ( !g_bActiveDlights )
		return;

	dlight_t	*l = cl_dlights;
	for (int i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->flags & (DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_DISPLACEMENT_MASK))
			continue;
		if (l->die < cl.GetTime() || !l->IsRadiusGreaterThanZero() )
			continue;
		// If the light's not active, then continue
		if ( (r_dlightactive & (1 << i)) == 0 )
			continue;
		
#ifdef _DEBUG
		if( r_drawlightcache.GetInt() == 4 )
		{
			Vector mins( -5, -5, -5 );
			Vector maxs( 5, 5, 5 );
			CDebugOverlay::AddBoxOverlay( l->origin, mins, maxs, vec3_angle, 255, 255, 255, 0, 0.001f );
		}
#endif
		s_pDLightVis = NULL;
		s_MarkStaticPropLightsEnumerator.SetLightID( i );
		SpatialPartition()->EnumerateElementsInSphere( PARTITION_ENGINE_STATIC_PROPS, 
			l->origin, l->GetRadius(), true, &s_MarkStaticPropLightsEnumerator );
	}
}

float g_flMinLightingValue = 1.0f;

void InitDLightGlobals( int nMapVersion )
{
	if( nMapVersion >= 20 )
	{
		// The light level at which we are close enough to black to treat as black for 
		// culling purposes.
		g_flMinLightingValue = 1.0f / 256.0f;
	}
	else
	{
		// This is the broken value from HL2.  It is supposed to be
		// the light level at which we are close enough to black to treat as black for 
		// culling purposes.  We leave it at the broken value here for old bsp files
		// Since HL2 maps were compiled with this bsp version.
		g_flMinLightingValue = 20.0f / 256.0f;
	}
}
