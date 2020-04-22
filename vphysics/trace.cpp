//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "cmodel.h"
#include "physics_trace.h"
#include "ivp_surman_polygon.hxx"
#include "ivp_compact_ledge.hxx"
#include "ivp_compact_ledge_solver.hxx"
#include "ivp_compact_surface.hxx"
#include "tier0/vprof.h"
#include "mathlib/ssemath.h"
#include "tier0/tslist.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// this skips the sphere tree stuff for tracing
#define DEBUG_TEST_ALL_LEDGES 0
// this skips the optimization that shrinks the ray as each intersection is encountered
#define DEBUG_KEEP_FULL_RAY 0
// this skips the optimization that looks up the first vert in a cubemap
#define USE_COLLIDE_MAP 1

// objects with small numbers of verts build a cache of pre-transformed verts
#define USE_VERT_CACHE 1
#define USE_RLE_SPANS 1

// UNDONE: This is a boost on PC, but doesn't work yet on x360 - investigate
#define SIMD_MATRIX	0

// turn this on to get asserts in the low-level collision solver
#define CHECK_TOI_CALCS 0

#define BRUTE_FORCE_VERT_COUNT 128

// NOTE: This is in inches (HL units)
#define TEST_EPSILON	(g_PhysicsUnits.collisionSweepIncrementalEpsilon)

struct simplexvert_t
{
	Vector	position;
	unsigned short testIndex : 15;
	unsigned short sweepIndex : 1;
	unsigned short obstacleIndex;
};

struct simplex_t
{
	simplexvert_t	verts[4];
	int				vertCount;

	inline bool PointSimplex( const simplexvert_t &newPoint, Vector *pOut );
	inline bool EdgeSimplex( const simplexvert_t &newPoint, int outIndex, const Vector &edge, Vector *pOut );
	inline bool TriangleSimplex( const simplexvert_t &newPoint, int outIndex, const Vector &faceNormal, Vector *pOut );

	bool SolveGJKSet( const simplexvert_t &newPoint, Vector *pOut );
	bool SolveVoronoiRegion2( const simplexvert_t &newPoint, Vector *pOut );
	bool SolveVoronoiRegion3( const simplexvert_t &newPoint, Vector *pOut );
	bool SolveVoronoiRegion4( const simplexvert_t &newPoint, Vector *pOut );

	Vector ClipRayToTetrahedronBase( const Vector &dir );
	Vector ClipRayToTetrahedron( const Vector &dir );
	float ClipRayToTriangle( const Vector &dir, float epsilon );
};

class CTraceCone : public ITraceObject
{
public:
	CTraceCone( const truncatedcone_t &cone, const Vector &translation )
	{
		m_cone = cone;
		m_cone.origin += translation;
		float cosTheta;
		SinCos( DEG2RAD(m_cone.theta), &m_sinTheta, &cosTheta );
		m_radius = m_cone.h * m_sinTheta / cosTheta;
		m_centerBase = m_cone.origin + m_cone.h * m_cone.normal;
	}

	virtual int SupportMap( const Vector &dir, Vector *pOut ) const
	{
		Vector unitDir = dir;
		VectorNormalize(unitDir);

		float dot = DotProduct( unitDir, m_cone.normal );

		// anti-cone is -normal, angle = 90 - theta
		// If the normal is in the anti-cone, then return the apex

		// not in anti-cone, support map is on the surface of the disc
		if ( dot > -m_sinTheta )
		{
			unitDir -= m_cone.normal * dot;
			float len = VectorNormalize( unitDir );
			if ( len > 1e-4f )
			{
				*pOut = m_centerBase + (unitDir * m_radius);
				return 0;
			}
			*pOut = m_centerBase;
			return 0;


		}
		// outside the cone's angle, support map is on the surface of the cone
		*pOut = m_cone.origin;
		return 0;
	}

	// BUGBUG: Doesn't work!
	virtual Vector GetVertByIndex( int index ) const { return  m_cone.origin; }
	virtual float Radius( void ) const { return m_cone.h + m_radius; }

	truncatedcone_t	m_cone;
	float			m_radius;
	float			m_sinTheta;
	Vector			m_centerBase;
};

 
// really this is indexing a vertex, but the iteration code needs a triangle + edge index.
// edge is always 0-2 so return it in the bottom 2 bits
static unsigned short GetPackedIndex( const IVP_Compact_Ledge *pLedge, const IVP_U_Float_Point &dir )
{
	const IVP_Compact_Poly_Point *RESTRICT pPoints = pLedge->get_point_array();
	const IVP_Compact_Triangle *RESTRICT pTri = pLedge->get_first_triangle();
	const IVP_Compact_Edge *RESTRICT pEdge = pTri->get_edge( 0 );
	int best = pEdge->get_start_point_index();
	float bestDot = pPoints[best].dot_product( &dir );
	int triCount = pLedge->get_n_triangles();
	const IVP_Compact_Triangle *RESTRICT pBestTri = pTri;
	// this loop will early out, but keep it from being infinite
	int i;
	// hillclimbing search to find the best support vert
	for ( i = 0; i < triCount; i++ )
	{
		// get the index to the end vert of this edge (start vert on next edge)
		pEdge = pEdge->get_prev();
		int stopVert = pEdge->get_start_point_index();

		// loop through the verts that can be reached along edges from this vert
		// stop if you get back to the one you're starting on.
		int vert = stopVert;
		do
		{
			float dot = pPoints[vert].dot_product( &dir );
			if ( dot > bestDot )
			{
				bestDot = dot;
				best = vert;
				pBestTri = pEdge->get_triangle();
				break;
			}
			// tri opposite next edge, same starting vert as next edge
			pEdge = pEdge->get_opposite()->get_prev();
			vert = pEdge->get_start_point_index();
		} while ( vert != stopVert );

		// if you exhausted the possibilities for this vert, it must be the best vert
		if ( vert != best )
			break;
	}

	int triIndex = pBestTri - pLedge->get_first_triangle();
	int edgeIndex = 0;
	// just do a search for the edge containing this vert instead of storing it along the way
	for ( i = 0; i < 3; i++ )
	{
		if ( pBestTri->get_edge(i)->get_start_point_index() == best )
		{
			edgeIndex = i;
			break;
		}
	}

	return (unsigned short) ( (triIndex<<2) + edgeIndex );
}


void InitLeafmap( IVP_Compact_Ledge *pLedge, leafmap_t *pLeafmapOut )
{
	pLeafmapOut->pLeaf = pLedge;
	pLeafmapOut->vertCount = 0;
	pLeafmapOut->flags = 0;
	pLeafmapOut->spanCount = 0;
	if ( pLedge && pLedge->is_terminal() )
	{
		// for small numbers of verts it's much faster to simply do dot products with all verts
		// since the best case for hillclimbing is to touch the start vert plus all neighbors (avg_valence+1 dots)
		// in t
		int triCount = pLedge->get_n_triangles();
		// this is a guess that anything with more than brute_force * 4 tris will have at least brute_force verts
		if ( triCount <= BRUTE_FORCE_VERT_COUNT*4 )
		{
			Assert(triCount>0);
			int minV = MAX_CONVEX_VERTS;
			int maxV = 0;
			for ( int i = 0; i < triCount; i++ )
			{
				const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + i;
				for ( int j = 0; j < 3; j++ )
				{
					const IVP_Compact_Edge *pEdge = pTri->get_edge( j );
					int v = pEdge->get_start_point_index();
					if ( v < minV )
					{
						minV = v;
					}
					if ( v > maxV )
					{
						maxV = v;
					}
				}
			}
			int vertCount = (maxV-minV) + 1;
			// max possible verts is < 48, so this is just here for some real failure
			// or vert sharing with a large collection of convexes.  In that case the 
			// number could be high, but this approach to implementing support is invalid
			// because the vert range is polluted
			if ( vertCount < BRUTE_FORCE_VERT_COUNT )
			{
				char hasVert[BRUTE_FORCE_VERT_COUNT];
				memset(hasVert, 0, sizeof(hasVert[0])*vertCount);
				for ( int i = 0; i < triCount; i++ )
				{
					const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + i;
					for ( int j = 0; j < 3; j++ )
					{
						// mark each vert in the list
						const IVP_Compact_Edge *pEdge = pTri->get_edge( j );
						int v = pEdge->get_start_point_index();
						hasVert[v-minV] = true;
					}
				}
				// now find the vertex spans and encode them
				byte spans[BRUTE_FORCE_VERT_COUNT];
				int spanIndex = 0;
				char has = hasVert[0];
				Assert(has);
				byte count = 1;
				for ( int i = 1; i < vertCount && spanIndex < BRUTE_FORCE_VERT_COUNT; i++ )
				{
					// each change of state is a new span
					if ( has != hasVert[i] )
					{
						spans[spanIndex] = count;
						has = hasVert[i];
						count = 0;
						spanIndex++;
					}
					count++;
					Assert(count < 255);
				}

				// rle spans only supported with vertex caching
#if USE_VERT_CACHE && USE_RLE_SPANS
				if ( spanIndex < BRUTE_FORCE_VERT_COUNT )
#else
				if ( spanIndex < 1 )
#endif
				{
					spans[spanIndex] = count;
					spanIndex++;
					pLeafmapOut->SetRLESpans( minV, spanIndex, spans );
				}
			}
		}
	}

	if ( !pLeafmapOut->HasSpans() )
	{
		// otherwise make a 8-way directional map to pick the best start vert for hillclimbing
		pLeafmapOut->SetHasCubemap();
		for ( int i = 0; i < 8; i++ )
		{
			IVP_U_Float_Point tmp;
			tmp.k[0] = ( i & 1 ) ? -1 : 1;
			tmp.k[1] = ( i & 2 ) ? -1 : 1;
			tmp.k[2] = ( i & 4 ) ? -1 : 1;
			pLeafmapOut->startVert[i] = GetPackedIndex( pLedge, tmp );
		}
	}
}


void GetStartVert( const leafmap_t *pLeafmap, const IVP_U_Float_Point &localDirection, int &triIndex, int &edgeIndex )
{
	if ( !pLeafmap || !pLeafmap->HasCubemap() )
		return;

	// map dir to index
	int cacheIndex = (localDirection.k[0] < 0 ? 1 : 0) + (localDirection.k[1] < 0 ? 2 : 0) + (localDirection.k[2] < 0 ? 4 : 0 );
	triIndex = pLeafmap->startVert[cacheIndex] >> 2;
	edgeIndex = pLeafmap->startVert[cacheIndex] & 0x3;
}

CTSPool<CVisitHash> g_VisitHashPool;

CVisitHash *AllocVisitHash()
{
	return g_VisitHashPool.GetObject();
}

void FreeVisitHash(CVisitHash *pFree)
{
	if ( pFree )
	{
		g_VisitHashPool.PutObject(pFree);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Implementation for Trace against an IVP object
//-----------------------------------------------------------------------------
class CTraceIVP : public ITraceObject
{
public:
	CTraceIVP( const CPhysCollide *pCollide, const Vector &origin, const QAngle &angles );
	~CTraceIVP()
	{
		if ( m_pVisitHash )
			FreeVisitHash(m_pVisitHash);
	}
	virtual int SupportMap( const Vector &dir, Vector *pOut ) const;
	virtual Vector GetVertByIndex( int index ) const;

	// UNDONE: Do general ITraceObject center/offset computation and move the ray to account
	// for this delta like we do in TraceSweepIVP()
	// Then we can shrink the radius of objects with mass centers NOT at the origin
	virtual float Radius( void ) const 
	{ 
		return m_radius;
	}

	inline float TransformLengthToLocal( float length )
	{
		return ConvertDistanceToIVP( length );
	}
	// UNDONE: Optimize this by storing 3 matrices? (one for each transform that includes rot/scale for HL/IVP)?
	// UNDONE: Not necessary if we remove the coordinate conversion
	inline void TransformDirectionToLocal( const Vector &dir, IVP_U_Float_Point &local ) const
	{
		IVP_U_Float_Point tmp;
		ConvertDirectionToIVP( dir, tmp );
		m_matrix.vimult3( &tmp, &local );
	}

	inline void RotateRelativePositionToLocal( const Vector &delta, IVP_U_Float_Point &local ) const
	{
		IVP_U_Float_Point tmp;
		ConvertPositionToIVP( delta, tmp );
		m_matrix.vimult3( &tmp, &local );
	}

	inline void TransformPositionToLocal( const Vector &pos, IVP_U_Float_Point &local ) const
	{
		IVP_U_Float_Point tmp;
		ConvertPositionToIVP( pos, tmp );
		m_matrix.vimult4( &tmp, &local );
	}

	inline void TransformPositionFromLocal( const IVP_U_Float_Point &local, Vector &out ) const
	{
		VectorTransform( *(Vector *)&local, *((const matrix3x4_t *)&m_ivpLocalToHLWorld), out );
	}

#if USE_VERT_CACHE
	inline Vector CachedVertByIndex(int index) const
	{
		int subIndex = index & 3;
		return m_vertCache[index>>2].Vec(subIndex);
	}
#endif

	bool IsValid( void ) { return m_pLedge != NULL; }

	void AllocateVisitHash()
	{
		if ( !m_pVisitHash )
			m_pVisitHash = AllocVisitHash();
	}

	void SetLedge( const IVP_Compact_Ledge *pLedge )
	{
		m_pLedge = pLedge;
		m_pLeafmap = NULL;
		if ( !pLedge )
			return;

#if USE_VERT_CACHE
		m_cacheCount = 0;
#endif
		if ( m_pCollideMap )
		{
			for ( int i = 0; i < m_pCollideMap->leafCount; i++ )
			{
				if ( m_pCollideMap->leafmap[i].pLeaf == pLedge )
				{
					m_pLeafmap = &m_pCollideMap->leafmap[i];
					if ( !BuildLeafmapCache( &m_pCollideMap->leafmap[i] ) )
					{
						AllocateVisitHash();
					}
					return;
				}
			}
		}
		AllocateVisitHash();
	}

	bool SetSingleConvex( void )
	{
		const IVP_Compact_Ledgetree_Node *node = m_pSurface->get_compact_ledge_tree_root();
		if ( node->is_terminal() == IVP_TRUE )
		{
			SetLedge( node->get_compact_ledge() );
			return true;
		}
		SetLedge( NULL );
		return false;
	}
	bool BuildLeafmapCache(const leafmap_t * RESTRICT pLeafmap);
	bool BuildLeafmapCacheRLE( const leafmap_t * RESTRICT pLeafmap );
	inline int SupportMapCached( const Vector &dir, Vector *pOut ) const;
	const collidemap_t			*m_pCollideMap;
	const IVP_Compact_Surface	*m_pSurface;

private:
	const leafmap_t				*m_pLeafmap;
	const IVP_Compact_Ledge		*m_pLedge;
	CVisitHash					*m_pVisitHash;
#if SIMD_MATRIX
	FourVectors					m_ivpLocalToHLWorld;
#else
	matrix3x4_t					m_ivpLocalToHLWorld;
#endif
	IVP_U_Matrix				m_matrix;
	// transform that includes scale from IVP to HL coords, do not VectorITransform or VectorRotate with this
	float						m_radius;
	int							m_nPointTest;
	int							m_nStartPoint;
	bool						m_bHasTranslation;
#if USE_VERT_CACHE
	int							m_cacheCount;	// number of FourVectors used
	FourVectors					m_vertCache[BRUTE_FORCE_VERT_COUNT/4];
#endif
};

// GCC 4.2.1 can't handle loading a static const into a m128 register :(
#ifdef WIN32
static const 
#endif
fltx4 g_IVPToHLDir = { 1.0f, -1.0f, 1.0f, 1.0f };

//static const fltx4 g_IVPToHLPosition = { IVP2HL(1.0f), -IVP2HL(1.0f), IVP2HL(1.0f), IVP2HL(1.0f) };

#if defined(_X360)

FORCEINLINE fltx4 ConvertDirectionToIVP( const fltx4 & a )
{
	fltx4 t = __vpermwi( a, VPERMWI_CONST( 0, 2, 1, 3 ) );
	// negate Y
	return MulSIMD( t, g_IVPToHLDir );
}
#else
FORCEINLINE fltx4 ConvertDirectionToIVP( const fltx4 & a )
{
	// swap Z & Y
	fltx4 t = _mm_shuffle_ps( a, a, MM_SHUFFLE_REV( 0, 2, 1, 3 ) );
	// negate Y
	return MulSIMD( t, g_IVPToHLDir );
}
#endif

CTraceIVP::CTraceIVP( const CPhysCollide *pCollide, const Vector &origin, const QAngle &angles )
{
#if USE_COLLIDE_MAP
	m_pCollideMap = pCollide->GetCollideMap();
#else
	m_pCollideMap = NULL;
#endif
	m_pSurface = pCollide->GetCompactSurface();
	m_pLedge = NULL;
	m_pVisitHash = NULL;

	m_bHasTranslation = (origin==vec3_origin) ? false : true;
	// UNDONE: Move this offset calculation into the tracing routines
	// I didn't do this now because it seems to require changes to most of the
	// transform routines - and this would cause bugs.
	float centerOffset = VectorLength( m_pSurface->mass_center.k );
#if SIMD_MATRIX
	VectorAligned forward, right, up;
	IVP_U_Float_Point ivpForward, ivpLeft, ivpUp;

	AngleVectors( angles, &forward, &right, &up );

	Vector left = -right;
	Vector down = -up;

	ConvertDirectionToIVP( forward, ivpForward );
	ConvertDirectionToIVP( left, ivpLeft );
	ConvertDirectionToIVP( down, ivpUp );

	m_matrix.set_col( IVP_INDEX_X, &ivpForward );
	m_matrix.set_col( IVP_INDEX_Z, &ivpLeft );
	m_matrix.set_col( IVP_INDEX_Y, &ivpUp );
	ConvertPositionToIVP( origin, m_matrix.vv );

	forward.w = HL2IVP(origin.x);
	// This vector is supposed to be left, so we'll negate it later, but we don't want to 
	// negate the position, so add another minus to cancel out
	right.w = -HL2IVP(origin.y);
	up.w = HL2IVP(origin.z);
	fltx4 rx = ConvertDirectionToIVP(LoadAlignedSIMD(forward.Base()));
	fltx4 ry = ConvertDirectionToIVP(SubSIMD( Four_Zeros, LoadAlignedSIMD(right.Base())) );
	fltx4 rz = ConvertDirectionToIVP(LoadAlignedSIMD(up.Base()) );

	fltx4 scaleHL = ReplicateX4(IVP2HL(1.0f));
	m_ivpLocalToHLWorld.x = MulSIMD( scaleHL, rx );
	m_ivpLocalToHLWorld.y = MulSIMD( scaleHL, ry );
	m_ivpLocalToHLWorld.z = MulSIMD( scaleHL, rz );
#else
	ConvertRotationToIVP( angles, m_matrix );
	ConvertPositionToIVP( origin, m_matrix.vv );
	float scale = IVP2HL(1.0f);
	float negScale = IVP2HL(-1.0f);
	// copy the existing IVP local->world matrix (swap Y & Z)
	m_ivpLocalToHLWorld.m_flMatVal[0][0] = m_matrix.get_elem(IVP_INDEX_X,0) * scale; 
	m_ivpLocalToHLWorld.m_flMatVal[0][1] = m_matrix.get_elem(IVP_INDEX_X,1) * scale; 
	m_ivpLocalToHLWorld.m_flMatVal[0][2] = m_matrix.get_elem(IVP_INDEX_X,2) * scale; 

	m_ivpLocalToHLWorld.m_flMatVal[1][0] = m_matrix.get_elem(IVP_INDEX_Z,0) * scale; 
	m_ivpLocalToHLWorld.m_flMatVal[1][1] = m_matrix.get_elem(IVP_INDEX_Z,1) * scale; 
	m_ivpLocalToHLWorld.m_flMatVal[1][2] = m_matrix.get_elem(IVP_INDEX_Z,2) * scale; 

	m_ivpLocalToHLWorld.m_flMatVal[2][0] = m_matrix.get_elem(IVP_INDEX_Y,0) * negScale; 
	m_ivpLocalToHLWorld.m_flMatVal[2][1] = m_matrix.get_elem(IVP_INDEX_Y,1) * negScale; 
	m_ivpLocalToHLWorld.m_flMatVal[2][2] = m_matrix.get_elem(IVP_INDEX_Y,2) * negScale; 

	m_ivpLocalToHLWorld.m_flMatVal[0][3] = m_matrix.vv.k[0] * scale;
	m_ivpLocalToHLWorld.m_flMatVal[1][3] = m_matrix.vv.k[2] * scale;
	m_ivpLocalToHLWorld.m_flMatVal[2][3] = m_matrix.vv.k[1] * negScale;

#endif

	m_radius = ConvertDistanceToHL( m_pSurface->upper_limit_radius + centerOffset );
}

bool CTraceIVP::BuildLeafmapCacheRLE( const leafmap_t * RESTRICT pLeafmap )
{
	// iterate the rle spans of verts and output them to a buffer in post-transform space
	int startPoint = pLeafmap->startVert[0];
	int pointCount = pLeafmap->vertCount;
	m_cacheCount = (pointCount + 3)>>2;
	const byte *RESTRICT pSpans = pLeafmap->GetSpans();
	int countThisSpan = pSpans[0];
	int spanIndex = 1;
	int baseVert = 0;
	const VectorAligned * RESTRICT pVerts = (const VectorAligned *)&m_pLedge->get_point_array()[startPoint];
	for ( int i = 0; i < m_cacheCount-1; i++ )
	{
		if ( countThisSpan < 4 )
		{
			// unrolled for perf
			// we need a batch of four verts, but they aren't in a single span
			int v0, v1, v2, v3;
			if ( !countThisSpan )
			{
				baseVert += pSpans[spanIndex];
				countThisSpan = pSpans[spanIndex+1];
				spanIndex += 2;
			}
			v0 = baseVert++;
			countThisSpan--;
			if ( !countThisSpan )
			{
				baseVert += pSpans[spanIndex];
				countThisSpan = pSpans[spanIndex+1];
				spanIndex += 2;
			}
			v1 = baseVert++;
			countThisSpan--;
			if ( !countThisSpan )
			{
				baseVert += pSpans[spanIndex];
				countThisSpan = pSpans[spanIndex+1];
				spanIndex += 2;
			}
			v2 = baseVert++;
			countThisSpan--;
			if ( !countThisSpan )
			{
				baseVert += pSpans[spanIndex];
				countThisSpan = pSpans[spanIndex+1];
				spanIndex += 2;
			}
			v3 = baseVert++;
			countThisSpan--;
			m_vertCache[i].LoadAndSwizzleAligned( pVerts[v0].Base(), pVerts[v1].Base(), pVerts[v2].Base(), pVerts[v3].Base() );
		}
		else
		{
			// we have four verts in this span, just grab the next four
			m_vertCache[i].LoadAndSwizzleAligned( pVerts[baseVert+0].Base(), pVerts[baseVert+1].Base(), pVerts[baseVert+2].Base(), pVerts[baseVert+3].Base() );
			baseVert += 4;
			countThisSpan -= 4;
		}
	}
	// the last iteration needs multiple spans and clamping to the last vert
	int v[4];
	for ( int i = 0; i < 4; i++ )
	{
		if ( spanIndex < pLeafmap->spanCount && !countThisSpan )
		{
			baseVert += pSpans[spanIndex];
			countThisSpan = pSpans[spanIndex+1];
			spanIndex += 2;
		}
		if ( spanIndex < pLeafmap->spanCount )
		{
			v[i] = baseVert;
			baseVert++;
			countThisSpan--;
		}
		else
		{
			v[i] = baseVert;
			if ( countThisSpan > 1 )
			{
				countThisSpan--;
				baseVert++;
			}
		}
	}
	m_vertCache[m_cacheCount-1].LoadAndSwizzleAligned( pVerts[v[0]].Base(), pVerts[v[1]].Base(), pVerts[v[2]].Base(), pVerts[v[3]].Base() );
	FourVectors::RotateManyBy( &m_vertCache[0], m_cacheCount, *((const matrix3x4_t *)&m_ivpLocalToHLWorld) );

	return true;
}

bool CTraceIVP::BuildLeafmapCache( const leafmap_t * RESTRICT pLeafmap )
{
#if !USE_VERT_CACHE
	return false;
#else
	if ( !pLeafmap || !pLeafmap->HasSpans() || m_bHasTranslation )
		return false;
	if ( pLeafmap->HasRLESpans() )
	{
		return BuildLeafmapCacheRLE(pLeafmap);
	}

	// single vertex span, just xform + copy

	// iterate the span of verts and output them to a buffer in post-transform space
	// just iterate the range if one is specified
	int startPoint = pLeafmap->startVert[0];
	int pointCount = pLeafmap->vertCount;
	m_cacheCount = (pointCount + 3)>>2;
	Assert(m_cacheCount>=0 && m_cacheCount<= (BRUTE_FORCE_VERT_COUNT/4));

	const VectorAligned * RESTRICT pVerts = (const VectorAligned *)&m_pLedge->get_point_array()[startPoint];
	for ( int i = 0; i < m_cacheCount-1; i++ )
	{
		m_vertCache[i].LoadAndSwizzleAligned( pVerts[0].Base(), pVerts[1].Base(), pVerts[2].Base(), pVerts[3].Base() );
		pVerts += 4;
	}

	int remIndex = (pointCount-1) & 3;
	int x0 = 0;
	int x1 = min(1,remIndex);
	int x2 = min(2,remIndex);
	int x3 = min(3,remIndex);
	m_vertCache[m_cacheCount-1].LoadAndSwizzleAligned( pVerts[x0].Base(), pVerts[x1].Base(), pVerts[x2].Base(), pVerts[x3].Base() );
	FourVectors::RotateManyBy( &m_vertCache[0], m_cacheCount, *((const matrix3x4_t *)&m_ivpLocalToHLWorld) );
	return true;
#endif
}

static const fltx4 g_IndexBase = {0,1,2,3};
int CTraceIVP::SupportMapCached( const Vector &dir, Vector *pOut ) const
{
	VPROF("SupportMapCached");
#if USE_VERT_CACHE
	FourVectors fourDir;
#if defined(_X360)
	fltx4 vec = LoadUnaligned3SIMD( dir.Base() );
	fourDir.x = SplatXSIMD(vec);
	fourDir.y = SplatYSIMD(vec);
	fourDir.z = SplatZSIMD(vec);
#else
	fourDir.DuplicateVector(dir);
#endif

	fltx4 index = g_IndexBase;
	fltx4 maxIndex = g_IndexBase;
	fltx4 maxDot = fourDir * m_vertCache[0];
	for ( int i = 1; i < m_cacheCount; i++ )
	{
		index = AddSIMD(index, Four_Fours);
		fltx4 dot = fourDir * m_vertCache[i];
		fltx4 cmpMask = CmpGtSIMD(dot,maxDot);
		maxIndex = MaskedAssign( cmpMask, index, maxIndex );
		maxDot = MaxSIMD(dot, maxDot);
	}

	// find highest of 4
	fltx4 rot = RotateLeft2(maxDot);
	fltx4 rotIndex = RotateLeft2(maxIndex);
	fltx4 cmpMask = CmpGtSIMD(rot,maxDot);
	maxIndex = MaskedAssign(cmpMask, rotIndex, maxIndex);
	maxDot = MaxSIMD(rot,maxDot);
	rotIndex = RotateLeft(maxIndex);
	rot = RotateLeft(maxDot);
	cmpMask = CmpGtSIMD(rot,maxDot);
	maxIndex = MaskedAssign(cmpMask, rotIndex, maxIndex);
	// not needed unless we need the actual max dot at the end
	//	maxDot = MaxSIMD(rot,maxDot);

	int bestIndex = SubFloatConvertToInt(maxIndex,0);
	*pOut = CachedVertByIndex(bestIndex);

	return bestIndex;
#else
	Assert(0);
#endif
}

int CTraceIVP::SupportMap( const Vector &dir, Vector *pOut ) const
{
#if USE_VERT_CACHE
	if ( m_cacheCount )
		return SupportMapCached( dir, pOut );
#endif

	if ( m_pLeafmap && m_pLeafmap->HasSingleVertexSpan() )
	{
		VPROF("SupportMap_Leaf");
		const IVP_U_Float_Point *pPoints = m_pLedge->get_point_array();
		IVP_U_Float_Point mapdir;
		TransformDirectionToLocal( dir, mapdir );
		// just iterate the range if one is specified
		int startPoint = m_pLeafmap->startVert[0];
		int pointCount = m_pLeafmap->vertCount;
		float bestDot = pPoints[startPoint].dot_product(&mapdir);
		int best = startPoint;
		for ( int i = 1; i < pointCount; i++ )
		{
			float dot = pPoints[startPoint+i].dot_product(&mapdir);
			if ( dot > bestDot )
			{
				bestDot = dot;
				best = startPoint+i;
			}
		}

		TransformPositionFromLocal( pPoints[best], *pOut ); // transform point position to world space
		return best;
	}
	else
	{
		VPROF("SupportMap_Walk");
		const IVP_U_Float_Point *pPoints = m_pLedge->get_point_array();
		IVP_U_Float_Point mapdir;
		TransformDirectionToLocal( dir, mapdir );
		int triCount = m_pLedge->get_n_triangles();
		Assert( m_pVisitHash );
		m_pVisitHash->NewVisit();

		float dot;
		int triIndex = 0, edgeIndex = 0;
		GetStartVert( m_pLeafmap, mapdir, triIndex, edgeIndex );
		const IVP_Compact_Triangle *RESTRICT pTri = m_pLedge->get_first_triangle() + triIndex;
		const IVP_Compact_Edge *RESTRICT pEdge = pTri->get_edge( edgeIndex );
		int best = pEdge->get_start_point_index();
		float bestDot = pPoints[best].dot_product( &mapdir );
		m_pVisitHash->VisitVert(best);

		// This should never happen.  MAX_CONVEX_VERTS is very large (millions), none of our
		// models have anywhere near this many verts in a convex piece
		Assert(triCount*3<MAX_CONVEX_VERTS);
		// this loop will early out, but keep it from being infinite
		for ( int i = 0; i < triCount; i++ )
		{
			// get the index to the end vert of this edge (start vert on next edge)
			pEdge = pEdge->get_prev();
			int stopVert = pEdge->get_start_point_index();

			// loop through the verts that can be reached along edges from this vert
			// stop if you get back to the one you're starting on.
			int vert = stopVert;
			do
			{
				if ( !m_pVisitHash->WasVisited(vert) )
				{
					// this lets us skip doing dot products on this vert
					m_pVisitHash->VisitVert(vert);
					dot = pPoints[vert].dot_product( &mapdir );
					if ( dot > bestDot )
					{
						bestDot = dot;
						best = vert;
						break;
					}
				}
				// tri opposite next edge, same starting vert as next edge
				pEdge = pEdge->get_opposite()->get_prev();
				vert = pEdge->get_start_point_index();
			} while ( vert != stopVert );

			// if you exhausted the possibilities for this vert, it must be the best vert
			if ( vert != best )
				break;
		}

		// code to do the brute force method with no hill-climbing
#if 0
		for ( i = 0; i < triCount; i++ )
		{
			pTri = m_pLedge->get_first_triangle() + i;
			for ( int j = 0; j < 3; j++ )
			{
				pEdge = pTri->get_edge( j );
				int test = pEdge->get_start_point_index();
				dot = pPoints[test].dot_product( &mapdir );
				if ( dot > bestDot )
				{
					Assert(0);		// shouldn't hit this unless the hill-climb is broken
					bestDot = dot;
					best = test;
				}
			}
		}
#endif
		TransformPositionFromLocal( pPoints[best], *pOut ); // transform point position to world space

		return best;
	}
}

Vector CTraceIVP::GetVertByIndex( int index ) const
{
#if USE_VERT_CACHE
	if ( m_cacheCount )
	{
		return CachedVertByIndex(index);
	}
#endif
	const IVP_Compact_Poly_Point *pPoints = m_pLedge->get_point_array();

	Vector out;
	TransformPositionFromLocal( pPoints[index], out );
	return out;
}

//-----------------------------------------------------------------------------
// Purpose: Implementation for Trace against an AABB
//-----------------------------------------------------------------------------
class CTraceAABB : public ITraceObject
{
public:
	CTraceAABB( const Vector &hlmins, const Vector &hlmaxs, bool isPoint );
	virtual int SupportMap( const Vector &dir, Vector *pOut ) const;
	virtual Vector GetVertByIndex( int index ) const;
	virtual float Radius( void ) const { return m_radius; }

private:
	float	m_x[2];
	float	m_y[2];
	float	m_z[2];
	float	m_radius;
	bool	m_empty;
};


CTraceAABB::CTraceAABB( const Vector &hlmins, const Vector &hlmaxs, bool isPoint )
{
	if ( isPoint )
	{
		m_x[0] = m_x[1] = 0;
		m_y[0] = m_y[1] = 0;
		m_z[0] = m_z[1] = 0;
		m_radius = 0;
		m_empty = true;
	}
	else
	{
		m_x[0] = hlmaxs[0];
		m_x[1] = hlmins[0];
		m_y[0] = hlmaxs[1];
		m_y[1] = hlmins[1];
		m_z[0] = hlmaxs[2];
		m_z[1] = hlmins[2];
		m_radius = hlmaxs.Length();
		m_empty = false;
	}
}


int CTraceAABB::SupportMap( const Vector &dir, Vector *pOut ) const
{
	Vector out;

	if ( m_empty )
	{
		pOut->Init();
		return 0;
	}
	// index is formed by the 3-bit bitfield SzSySx (negative is 1, positive is 0)
	int x = ((*((unsigned int *)&dir.x)) & 0x80000000UL) >> 31;
	int y = ((*((unsigned int *)&dir.y)) & 0x80000000UL) >> 31;
	int z = ((*((unsigned int *)&dir.z)) & 0x80000000UL) >> 31;
	pOut->x = m_x[x];
	pOut->y = m_y[y];
	pOut->z = m_z[z];
	return (z<<2) | (y<<1) | x;
}

Vector CTraceAABB::GetVertByIndex( int index ) const
{
	Vector out;
	out.x = m_x[(index&1)];
	out.y = m_y[(index&2)>>1];
	out.z = m_z[(index&4)>>2];

	return out;
}


//-----------------------------------------------------------------------------
// Purpose: Implementation for Trace against an IVP object
//-----------------------------------------------------------------------------
class CTraceRay
{
public:
	CTraceRay( const Vector &hlstart, const Vector &hlend );
	CTraceRay( const Ray_t &ray );
	CTraceRay( const Ray_t &ray, const Vector &offset );
	void Init( const Vector &hlstart, const Vector &delta );
	int SupportMap( const Vector &dir, Vector *pOut ) const;
	Vector GetVertByIndex( int index ) const { return ( index ) ? m_end : m_start; }
	float Radius( void ) const { return m_length * 0.5f; }

	void Reset( float fraction );

	Vector	m_start;
	Vector	m_end;
	Vector	m_delta;
	Vector	m_dir;

	float	m_length;
	float	m_baseLength;
	float	m_ooBaseLength;
	float	m_bestDist;
};
CTraceRay::CTraceRay( const Vector &hlstart, const Vector &hlend )
{
	Init(hlstart, hlend-hlstart);
}

void CTraceRay::Init( const Vector &hlstart, const Vector &delta )
{
	m_start = hlstart;
	m_end = hlstart + delta;
	m_delta = delta;
	m_dir = delta;
	float len = DotProduct(delta, delta);
	// don't use fast/sse sqrt here we need the precision
	m_length = sqrt(len);
	m_ooBaseLength = 0.0f;
	if ( m_length > 0 )
	{
		m_ooBaseLength = 1.0f / m_length;
		m_dir *= m_ooBaseLength;
	}
	m_baseLength = m_length;
	m_bestDist = 0.f;
}

CTraceRay::CTraceRay( const Ray_t &ray )
{
	Init( ray.m_Start, ray.m_Delta );
}

CTraceRay::CTraceRay( const Ray_t &ray, const Vector &offset )
{
	Vector start;
	VectorAdd( ray.m_Start, offset, start );
	Init( start, ray.m_Delta );
}

void CTraceRay::Reset( float fraction )
{
	// recompute from base values for max precision
	m_length = m_baseLength * fraction;
	m_end = m_start + fraction * m_delta;
	m_bestDist = 0.f;
}

int CTraceRay::SupportMap( const Vector &dir, Vector *pOut ) const
{
	if ( DotProduct( dir, m_delta ) > 0 )
	{
		*pOut = m_end;
		return 1;
	}
	*pOut = m_start;
	return 0;
}

static char			*map_nullname = "**empty**";
static csurface_t	nullsurface = { map_nullname, 0 };

static void CM_ClearTrace( trace_t *trace )
{
	memset( trace, 0, sizeof(*trace));
	trace->fraction = 1.f;
	trace->fractionleftsolid = 0;
	trace->surface = nullsurface;
}

class CDefConvexInfo : public IConvexInfo
{
public:
	IConvexInfo *GetPtr() { return this; }

	virtual unsigned int GetContents( int convexGameData ) { return CONTENTS_SOLID; }
};

class CTraceSolver
{
public:
	CTraceSolver( trace_t *ptr, ITraceObject *sweepobject, CTraceRay *ray, ITraceObject *obstacle, const Vector &axis )
	{
		m_pTotalTrace = ptr;
		m_sweepObject = sweepobject;
		m_sweepObjectRadius = m_sweepObject->Radius();
		m_obstacle = obstacle;
		m_ray = ray;
		m_traceLength = 0;
		m_totalTraceLength = max( ray->m_baseLength, 1e-8f );
		m_pointClosestToIntersection = axis;
		m_epsilon = g_PhysicsUnits.collisionSweepEpsilon;
	}

	bool SweepSingleConvex( void );
	float SolveMeshIntersection( simplex_t &simplex );
	float SolveMeshIntersection2D( simplex_t &simplex );
	virtual void DoSweep( void )
	{
		SweepSingleConvex();
		*m_pTotalTrace = m_trace;
	}


	void SetEpsilon( float epsilon )
	{
		m_epsilon = epsilon;
	}

protected:
	trace_t				m_trace;

	Vector				m_pointClosestToIntersection;
	ITraceObject		*m_sweepObject;
	ITraceObject		*m_obstacle;
	CTraceRay			*m_ray;
	trace_t				*m_pTotalTrace;
	float				m_traceLength;
	float				m_totalTraceLength;
	float				m_sweepObjectRadius;
	float				m_epsilon;
private:
	CTraceSolver( const CTraceSolver & );
};

class CTraceSolverSweptObject : public CTraceSolver
{
public:
	CTraceSolverSweptObject( trace_t *ptr, ITraceObject *sweepobject, CTraceRay *ray, CTraceIVP *obstacle, const Vector &axis, unsigned int contentsMask, IConvexInfo *pConvexInfo );

	void InitOSRay( void );
	void SweepLedgeTree_r( const IVP_Compact_Ledgetree_Node *node );
	inline bool SweepHitsSphereOS( const IVP_U_Float_Point *sphereCenter, float radius );
	virtual void DoSweep( void );
	inline void SweepAgainstNode( const IVP_Compact_Ledgetree_Node *node );

	CTraceIVP			*m_obstacleIVP;
	IConvexInfo			*m_pConvexInfo;
	unsigned int		m_contentsMask;
	CDefConvexInfo		m_fakeConvexInfo;


	IVP_U_Float_Point	m_rayCenterOS;
	IVP_U_Float_Point	m_rayStartOS;
	IVP_U_Float_Point	m_rayDirOS;
	IVP_U_Float_Point	m_rayDeltaOS;
	float				m_rayLengthOS;

private:
	CTraceSolverSweptObject( const CTraceSolverSweptObject & ); // no implementation, quells compiler warning
};

CTraceSolverSweptObject::CTraceSolverSweptObject( trace_t *ptr, ITraceObject *sweepobject, CTraceRay *ray, CTraceIVP *obstacle, const Vector &axis, unsigned int contentsMask, IConvexInfo *pConvexInfo )
: CTraceSolver( ptr, sweepobject, ray, obstacle, axis )
{
	m_obstacleIVP = obstacle;
	m_contentsMask = contentsMask;
	m_pConvexInfo = (pConvexInfo != NULL) ? pConvexInfo : m_fakeConvexInfo.GetPtr();
}


bool CTraceSolverSweptObject::SweepHitsSphereOS( const IVP_U_Float_Point *sphereCenter, float radius )
{
	// disable this to help find bugs
#if DEBUG_TEST_ALL_LEDGES
	return true;
#endif
	// the ray is actually a line-swept-sphere with sweep object's radius
	IVP_U_Float_Point delta_vec; // quick check for ends of ray
	delta_vec.subtract( sphereCenter, &m_rayCenterOS );
	radius += m_sweepObjectRadius;
	// Is the sphere close enough to the ray at the center?
	float qsphere_rad = radius * radius;

	// If this is a 0 length ray, then the conservative test is 100% accurate
	if ( m_rayLengthOS > 0 )
	{
		// Calculate the perpendicular distance to the sphere 
		// The perpendicular forms a right triangle with the vector between the ray/sphere centers
		// and the ray direction vector.  Calculate the projection of the hypoteneuse along the perpendicular
		IVP_U_Float_Point h;
		h.inline_calc_cross_product(&m_rayDirOS, &delta_vec);

		if( h.quad_length() < qsphere_rad )
			return true;
	}
	else
	{
		float quad_center_dist = delta_vec.quad_length();

		if ( quad_center_dist < qsphere_rad )
		{
			return true;
		}

		// Could a ray in any direction away from the ray center intersect this sphere?
		float qrad_sum = m_rayLengthOS * 0.5f + radius;
		qrad_sum *= qrad_sum;
		if ( quad_center_dist >= qrad_sum )
		{
			return false;
		}
	}

	return false;
}

inline void CTraceSolverSweptObject::SweepAgainstNode(const IVP_Compact_Ledgetree_Node *node)
{
	const IVP_Compact_Ledge *ledge = node->get_compact_ledge();
	unsigned int ledgeContents = m_pConvexInfo->GetContents( ledge->get_client_data() );
	if (m_contentsMask & ledgeContents)
	{
		m_obstacleIVP->SetLedge( ledge );
		if ( SweepSingleConvex() )
		{
			if ( m_traceLength < m_totalTraceLength )
			{
				m_pTotalTrace->plane.normal = m_trace.plane.normal;
				m_pTotalTrace->startsolid = m_trace.startsolid;
				m_pTotalTrace->allsolid = m_trace.allsolid;
				m_totalTraceLength = m_traceLength;
				m_pTotalTrace->fraction = m_traceLength * m_ray->m_ooBaseLength;
				Assert(m_pTotalTrace->fraction >= 0 && m_pTotalTrace->fraction <= 1.0f);
#if !DEBUG_KEEP_FULL_RAY
				// shrink the ray to the shortened length, but leave a buffer of collisionSweepEpsilon units
				// at the end to make sure that precision doesn't make you miss something slightly closer
				float testFraction = (m_traceLength + m_epsilon*2) * m_ray->m_ooBaseLength;
				if ( testFraction < 1.0f )
				{
					m_ray->Reset( testFraction );
					// Update OS ray to limit tests
					m_rayLengthOS = m_obstacleIVP->TransformLengthToLocal( m_ray->m_length );
					m_rayCenterOS.add_multiple( &m_rayStartOS, &m_rayDeltaOS, 0.5f * testFraction );
				}
#endif
				m_pTotalTrace->contents = ledgeContents;
			}
		}
	}
}


void CTraceSolverSweptObject::SweepLedgeTree_r( const IVP_Compact_Ledgetree_Node *node )
{
	IVP_U_Float_Point center; 
	center.set(node->center.k);
	if ( !SweepHitsSphereOS( &center, node->radius ) )
		return;

	// fast path for single leaf collision models
	if ( node->is_terminal() == IVP_TRUE )
	{
		SweepAgainstNode(node);
		return;
	}

	// use an array to implement a simple stack
	CUtlVectorFixedGrowable<const IVP_Compact_Ledgetree_Node *, 64> list;

	// pull the last item in the array (top of stack)
	// this is nearly a priority queue, but not actually, but it's cheaper (and faster in the benchmarks)
	// this code is trying to visit the nodes closest to the ray start first - which helps performance
	// since we're only interested in the first intersection of the swept object with the physcollide.
	while ( 1 )
	{
		// don't use the temp storage unless you have to.
loop_without_store:
		if ( node->is_terminal() == IVP_TRUE )
		{
			// leaf, do the test
			SweepAgainstNode(node);
		}
		else
		{
			// check node's children
			const IVP_Compact_Ledgetree_Node *node0 = node->left_son();
			center.set(node0->center.k);
			// if we don't insert, this is larger than any quad distance
			float lastDist = 1e24f;
			if ( SweepHitsSphereOS( &center, node0->radius ) )
			{
				lastDist = m_rayStartOS.quad_distance_to(&center);
			}
			else
			{
				node0 = NULL;
			}
			const IVP_Compact_Ledgetree_Node *node1 = node->right_son();
			center.set(node1->center.k);
			if ( SweepHitsSphereOS( &center, node1->radius ) )
			{
				if ( node0 )
				{
					// can hit, push on stack
					int index = list.AddToTail();
					float dist1 = m_rayStartOS.quad_distance_to(&center);
					if ( lastDist < dist1 )
					{
						node = node0;
						list[index] = node1;
					}
					else
					{
						node = node1;
						list[index] = node0;
					}
				}
				else
				{
					node = node1;
				}
				goto loop_without_store;
			}
			if ( node0 )
			{
				node = node0;
				goto loop_without_store;
			}
		}
		int last = list.Count()-1;
		if ( last < 0 )
			break;
		node = list[last];
		list.FastRemove(last);
	}
}


void CTraceSolverSweptObject::InitOSRay( void )
{
	// transform ray into object space
	m_rayLengthOS = m_obstacleIVP->TransformLengthToLocal( m_ray->m_length );
	m_obstacleIVP->TransformPositionToLocal( m_ray->m_start, m_rayStartOS );

	// no translation on matrix mult because this is a vector
	m_obstacleIVP->RotateRelativePositionToLocal( m_ray->m_delta, m_rayDeltaOS );
	m_rayDirOS.set(&m_rayDeltaOS);
	m_rayDirOS.normize();

	// add_multiple with 3 params assumes no initial value (should be set_add_multiple)
	m_rayCenterOS.add_multiple( &m_rayStartOS, &m_rayDeltaOS, 0.5f );
}


void CTraceSolverSweptObject::DoSweep( void )
{
	VPROF("TraceSolver::DoSweep");
	InitOSRay();

	// iterate ledge tree of obstacle
	const IVP_Compact_Surface *pSurface = m_obstacleIVP->m_pSurface;

	const IVP_Compact_Ledgetree_Node *lt_node_root;
	lt_node_root = pSurface->get_compact_ledge_tree_root();
	SweepLedgeTree_r( lt_node_root );
}

void CPhysicsTrace::SweepBoxIVP( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &surfaceOrigin, const QAngle &surfaceAngles, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( start, end, mins, maxs );
	SweepBoxIVP( ray, MASK_ALL, NULL, pCollide, surfaceOrigin, surfaceAngles, ptr );
}

void CPhysicsTrace::SweepBoxIVP( const Ray_t &raySrc, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &surfaceOrigin, const QAngle &surfaceAngles, trace_t *ptr )
{
	CM_ClearTrace( ptr );

	CTraceAABB box( -raySrc.m_Extents, raySrc.m_Extents, raySrc.m_IsRay );
	CTraceIVP ivp( pCollide, vec3_origin, surfaceAngles );

	// offset the space of this sweep so that the surface is at the origin of the solution space
	CTraceRay ray( raySrc, -surfaceOrigin );

	CTraceSolverSweptObject solver( ptr, &box, &ray, &ivp, ray.m_start, contentsMask, pConvexInfo );
	solver.DoSweep();

	VectorAdd( raySrc.m_Start, raySrc.m_StartOffset, ptr->startpos );
	VectorMA( ptr->startpos, ptr->fraction, raySrc.m_Delta, ptr->endpos );
	// The plane was shifted because we shifted everything over by surfaceOrigin, shift it back
	if ( ptr->DidHit() )
	{
		ptr->plane.dist = DotProduct( ptr->endpos, ptr->plane.normal );
	}
}

void CPhysicsTrace::SweepIVP( const Vector &start, const Vector &end, const CPhysCollide *pSweptSurface, const QAngle &sweptAngles, const CPhysCollide *pSurface, const Vector &surfaceOrigin, const QAngle &surfaceAngles, trace_t *ptr )
{
	CM_ClearTrace( ptr );

	CTraceIVP sweptObject( pSweptSurface, vec3_origin, sweptAngles );

	// offset the space of this sweep so that the surface is at the origin of the solution space
	CTraceIVP ivp( pSurface, vec3_origin, surfaceAngles );
	CTraceRay ray( start - surfaceOrigin, end - surfaceOrigin );

	IVP_U_BigVector<IVP_Compact_Ledge> objectLedges(32);
	IVP_Compact_Ledge_Solver::get_all_ledges( pSweptSurface->GetCompactSurface(), &objectLedges );
	for ( int i = objectLedges.len() - 1; i >= 0; --i )
	{
		trace_t tr;
		CM_ClearTrace( &tr );
		sweptObject.SetLedge( objectLedges.element_at(i) );
		CTraceSolverSweptObject solver( &tr, &sweptObject, &ray, &ivp, start - surfaceOrigin, MASK_ALL, NULL );
		// UNDONE: Need just more than 0.25" tolerance here because the output position will be used by vphysics
		// UNDONE: Really this should be the collision radius from the environment.
		solver.SetEpsilon( g_PhysicsUnits.globalCollisionTolerance );
		solver.DoSweep();
		if ( tr.fraction < ptr->fraction )
		{
			*ptr = tr;
		}
	}
	ptr->endpos = start*(1.f-ptr->fraction) + end * ptr->fraction;
	if ( ptr->DidHit() )
	{
		ptr->plane.dist = DotProduct( ptr->endpos, ptr->plane.normal );
	}
}


static void CalculateSeparatingPlane( trace_t *ptr, ITraceObject *sweepObject, CTraceRay *ray, ITraceObject *obstacle, simplex_t &simplex );


//-----------------------------------------------------------------------------
// What is this doing? It's going to be hard to understand without reading a 
// reference on the GJK algorithm.  But here's a quick overview:
// Basically (remember this is glossing over a ton of details!) the 
// algorithm is building up a simplex that is trying to contain the origin.  
// A simplex is a point, line segment, triangle, or tetrahedron - depending on 
// how many verts you have.  
// Anyway it slowly builds one of these one vert at a time with a directed search. 
// So you start out with a point, then it guesses the next point that would be 
// most likely to form a line through the origin.  If the line doesn't go quite 
// through the origin it tries to find a third point to capture the origin 
// within a triangle.  If that doesn't work it tries to make a 
// tent (tetrahedron) out of the triangle to capture the origin.
//
// But at each step if the origin is not contained within, it tries to 
// find which sub-feature is most likely to be in the solution.  In 
// the point case it's always just the point.  In the line/edge case it 
// can reduce back to a point (origin is closest to one of the points) 
// or be the line (origin is closest to some point between them).  
// Same with the triangle (origin is closest to one vert - vert, origin is 
// closest to one edge - reduce to that edge, origin is closes to some point 
// in the triangle's surface - keep the whole triangle).  With a tetrahedron 
// keeping the whole isn't possible unless the origin is inside and you're 
// done (the origin has been captured).
//
// "You're done"  means that there is an intersection between the two 
// volumes.  Assuming you're testing a sweep, it still has to test whether that 
// sweep can be shrunk back until there is no intersection.  So it checks that.
// If it's a swept test so it does the search with SolveMeshIntersection
// Otherwise, there's nothing to shrink, so you set startsolid and allsolid 
// because it's a point/box in solid test, not a swept box/ray hits solid test.
//
// Why is it trying to capture the origin? Basically GJK sets up a space 
// and a convex hull (the minkowski sum) in that space.  The convex hull
// represents a field of the distances between different features of the pair
// of objects (e.g. for two circles, this minkowski sum is just a circle).  
// So the origin is the point in the field where the distance between the 
// objects is zero.  This means they intersect.
//-----------------------------------------------------------------------------
#if defined(_X360)
inline void VectorNormalize_FastLowPrecision( Vector &a ) 
{
	float quad = (a.x*a.x) + (a.y*a.y) + (a.z*a.z);
	float ilen = __frsqrte(quad);
	a.x *= ilen;
	a.y *= ilen;
	a.z *= ilen;
}
#else
#define VectorNormalize_FastLowPrecision VectorNormalize
#endif

bool CTraceSolver::SweepSingleConvex( void )
{
	VPROF("TraceSolver::SweepSingleConvex");
	simplex_t simplex;
	simplexvert_t	vert;
	Vector tmp;

	simplex.vertCount = 0;

	if ( m_pointClosestToIntersection == vec3_origin )
	{
		m_pointClosestToIntersection.Init(1,0,0);
	}

	float testLen = 1;
	Vector dir = -m_pointClosestToIntersection;
	VectorNormalize_FastLowPrecision(dir);
	// safe loop, max 100 iterations
	for ( int i = 0; i < 100; i++ )
	{
		// map the direction into the minkowski sum, get a new surface point
		vert.testIndex = m_sweepObject->SupportMap( dir, &vert.position );
		vert.sweepIndex = m_ray->SupportMap( dir, &tmp );
		VectorAdd( vert.position, tmp, vert.position );
		vert.obstacleIndex = m_obstacle->SupportMap( -dir, &tmp );
		VectorSubtract( vert.position, tmp, vert.position );

		testLen = DotProduct( dir, vert.position );
		// found a separating axis, no intersection
		if ( testLen < 0 )
		{
			VPROF("SolveSeparation");
			// make sure we're separated by at least m_epsilon
			testLen = fabs(testLen);
			if ( testLen < m_epsilon && m_ray->m_length > 0 )
			{
				// not separated by enough
				Vector normal = dir;
				if ( testLen > 0 )
				{
					// try to find a better separating plane or clip the ray to the current one
					for ( int j = 0; j < 20; j++ )
					{
						Vector lastVert = vert.position;
						simplex.SolveGJKSet( vert, &m_pointClosestToIntersection );
						dir = -m_pointClosestToIntersection;
						VectorNormalize_FastLowPrecision( dir );
						// map the direction into the minkowski sum, get a new surface point
						vert.testIndex = m_sweepObject->SupportMap( dir, &vert.position );
						vert.sweepIndex = m_ray->SupportMap( dir, &tmp );
						VectorAdd( vert.position, tmp, vert.position );
						vert.obstacleIndex = m_obstacle->SupportMap( -dir, &tmp );
						VectorSubtract( vert.position, tmp, vert.position );

						// found a separating axis, no intersection
						float est = -DotProduct( dir, vert.position );
						if ( est > m_epsilon ) // big enough separation, no hit
							return false;
						// take plane with the most separation
						if ( est > testLen )
						{
							testLen = est;
							normal = dir;
						}
						float last = -DotProduct( dir, lastVert );
						// search is not converging, exit.
						if ( (est - last) > -1e-4f )
							break;
					}
				}

				// This trace is going to miss, but not by enough.
				// Hit the separating plane instead 
				float dot = -DotProduct( m_ray->m_delta, normal );
				if ( dot < -(m_epsilon*0.1) || (dot < -1e-4f && testLen < (m_epsilon*0.9)) )
				{
					CM_ClearTrace( &m_trace );
					float backupDistance = m_epsilon - testLen;
					backupDistance = -(backupDistance * m_ray->m_baseLength) / dot;
					m_traceLength = m_ray->m_length - backupDistance;
					if ( m_traceLength < 0 )
					{
						m_traceLength = 0;
						// try sliding along the surface of the minkowski sum
						backupDistance = SolveMeshIntersection2D( simplex );
						if ( m_ray->m_length > backupDistance )
						{
							m_traceLength = m_ray->m_length - backupDistance;
						}
					}
					m_trace.plane.normal = -normal;
					// this is fixed up by the outer code
					//m_trace.endpos = m_ray->m_start*(1.f-m_trace.fraction) + m_ray->m_end*m_trace.fraction;
					m_trace.contents = CONTENTS_SOLID;
					return true;
				}
			}
			return false;
		}

		// contains the origin
		if ( simplex.SolveGJKSet( vert, &m_pointClosestToIntersection ) )
		{
			VPROF("TraceSolver::SolveMeshIntersection");
			CM_ClearTrace( &m_trace );
			// now solve for t along the sweep
			if ( m_ray->m_length != 0 )
			{
				float dist = SolveMeshIntersection( simplex );
				if ( dist < m_ray->m_length && dist > 0.f )
				{
					m_traceLength = (m_ray->m_length - dist);
					CalculateSeparatingPlane( &m_trace, m_sweepObject, m_ray, m_obstacle, simplex );
					float dot = DotProduct( m_ray->m_dir, m_trace.plane.normal );
					if ( dot < 0 )
					{
						m_traceLength += (m_epsilon / dot);
					}

					if ( m_traceLength < 0 )
					{
						m_traceLength = 0;
					}
					//m_trace.fraction = m_traceLength * m_ray->m_ooBaseLength;
					//m_trace.endpos = m_ray->m_start*(1.f-m_trace.fraction) + m_ray->m_end*m_trace.fraction;
					m_trace.contents = CONTENTS_SOLID;
				}
				else
				{
					// UNDONE: This case happens when you start solid as well as when a false 
					// intersection is detected at the very end of the trace
					m_trace.startsolid = true;
					m_trace.allsolid = true;
					m_traceLength = 0;
				}
			}
			else
			{
				m_trace.startsolid = true;
				m_trace.allsolid = true;
				m_traceLength = 0;
			}
			return true;
		}
		dir = -m_pointClosestToIntersection;
		VectorNormalize_FastLowPrecision( dir );
	}

	// BUGBUG: The solution never converged - something is probably wrong!
	AssertMsg( false, "Solution never converged.");
	return false;
}


// NEW SWEPT GJK SOLVER 2/16/2006
// convenience routines - just makes the code a little simpler.
FORCEINLINE bool simplex_t::TriangleSimplex( const simplexvert_t &newPoint, int outIndex, const Vector &faceNormal, Vector *pOut )
{
	vertCount = 3;
	verts[outIndex] = newPoint;
	*pOut = -faceNormal;
	return false;
}
FORCEINLINE bool simplex_t::EdgeSimplex( const simplexvert_t &newPoint, int outIndex, const Vector &edge, Vector *pOut )
{
	vertCount = 2;
	verts[outIndex] = newPoint;
	Vector cross;
	CrossProduct( edge, newPoint.position, cross );
	CrossProduct( cross, edge, *pOut );
	return false;
}

FORCEINLINE bool simplex_t::PointSimplex( const simplexvert_t &newPoint, Vector *pOut )
{
	vertCount = 1;
	verts[0] = newPoint;
	*pOut = newPoint.position;
	return false;
}

// In general the voronoi region routines have comments referring to the verts
// All of the code assumes that vert A is the new vert being added to the set
// verts B, C, D are the previous set.  If BCD is a triangle it is assumed to be
// counter-clockwise winding order.  This must be maintained by the code!

// parametric value for closes point on a line segment (p0->p1) to the origin.
bool simplex_t::SolveVoronoiRegion2( const simplexvert_t &newPoint, Vector *pOut )
{
	// solve the line segment AB (where A is the new point)
	Vector AB = verts[0].position - newPoint.position;
	float d = DotProduct(AB, newPoint.position);
	if ( d < 0 )
	{
		return EdgeSimplex(newPoint, 1, AB, pOut);
	}
	else
	{
		return PointSimplex(newPoint, pOut);
	}
}

// UNDONE: Collapse these routines into a single general routine?
bool simplex_t::SolveVoronoiRegion3( const simplexvert_t &newPoint, Vector *pOut )
{
	// solve the triangle ABC (where A is the new point)
	Vector AB = verts[0].position - newPoint.position;
	Vector AC = verts[1].position - newPoint.position;
	Vector ABC;
	CrossProduct(AB, AC, ABC);
	Vector ABCxAC;
	CrossProduct(ABC, AC, ABCxAC);
	float d = DotProduct(ABCxAC, newPoint.position);
	// edge AC or edgeAB / A?
	if ( d < 0 )
	{
		d = DotProduct(AC, newPoint.position);
		if ( d < 0 )
		{
			// edge AC
			return EdgeSimplex(newPoint, 0, AC, pOut);
		}
	}
	else
	{
		// face or A / edge AB?
		Vector ABxABC;
		CrossProduct(AB, ABC, ABxABC);
		d = DotProduct(ABxABC, newPoint.position);
		if ( d > 0 )
		{
			// closest to face
			vertCount = 3;
			d = DotProduct(ABC, newPoint.position);
			// in front of face, return opposite direction
			if ( d < 0 )
			{
				verts[2] = newPoint;
				*pOut = -ABC;
				return false;
			}
			verts[2] = verts[1]; // swap to keep CCW
			verts[1] = newPoint;
			*pOut = ABC;
			return false;
		}
	}
	// edge AB or A
	d = DotProduct(AB, newPoint.position);
	if ( d < 0 )
	{
		return EdgeSimplex(newPoint, 1, AB, pOut);
	}
	return PointSimplex(newPoint, pOut);
}

bool simplex_t::SolveVoronoiRegion4( const simplexvert_t &newPoint, Vector *pOut )
{
	// solve the tetrahedron ABCD (where A is the new point)

	// compute edge vectors
	Vector AB = verts[0].position - newPoint.position;
	Vector AC = verts[1].position - newPoint.position;
	Vector AD = verts[2].position - newPoint.position;

	// compute face normals
	Vector ABC, ACD, ADB;
	CrossProduct( AB, AC, ABC );
	CrossProduct( AC, AD, ACD );
	CrossProduct( AD, AB, ADB );

	// edge plane normals
	Vector ABCxAC, ABxABC;
	CrossProduct( ABC, AC, ABCxAC );
	CrossProduct( AB, ABC, ABxABC );
	Vector ACDxAD, ACxACD;
	CrossProduct( ACD, AD, ACDxAD );
	CrossProduct( AC, ACD, ACxACD );
	Vector ADBxAB, ADxADB;
	CrossProduct( ADB, AB, ADBxAB );
	CrossProduct( AD, ADB, ADxADB );

	int faceFlags = 0;
	float d;
	d = DotProduct(ABC,newPoint.position);
	if ( d < 0 )
	{
		faceFlags |= 0x1;
	}
	d = DotProduct(ACD,newPoint.position);
	if ( d < 0 )
	{
		faceFlags |= 0x2;
	}
	d = DotProduct(ADB,newPoint.position);
	if ( d < 0 )
	{
		faceFlags |= 0x4;
	}

	switch( faceFlags )
	{
	case 0:
		// inside all 3 faces, we're done
		verts[3] = newPoint;
		vertCount = 4;
		return true;
	case 1:
		// ABC only, solve as a triangle
		return SolveVoronoiRegion3(newPoint, pOut);
	case 2:
		// ACD only, solve as a triangle
		verts[0] = verts[2]; // collapse BCD to DC
		return SolveVoronoiRegion3(newPoint, pOut);
	case 4:
		// ADB only, solve as a triangle
		verts[1] = verts[2]; // collapse BCD to BD
		return SolveVoronoiRegion3(newPoint, pOut);
	case 3:
		{
			// in front of ABC & ACD
			d = DotProduct(ABCxAC, newPoint.position);
			if ( d < 0 )
			{
				d = DotProduct(ACxACD, newPoint.position);
				if ( d < 0 )
				{
					d = DotProduct(AC,newPoint.position);
					if ( d < 0 )
					{
						// edge AC
						return EdgeSimplex( newPoint, 0, AC, pOut);
					}
					// point A
					return PointSimplex(newPoint, pOut);
				}
				else
				{
					d = DotProduct(ACDxAD, newPoint.position);
					if ( d < 0 )
					{
						// edge AD
						verts[0] = verts[2]; // collapse BCD to D
						return EdgeSimplex(newPoint, 1, AD, pOut);
					}
					// face ACD
					return TriangleSimplex(newPoint,0,ACD, pOut);
				}
			}
			else
			{
				d = DotProduct(ABxABC, newPoint.position);
				if ( d < 0 )
				{
					d = DotProduct(AB, newPoint.position);
					if ( d < 0 )
					{
						// edge AB
						return EdgeSimplex(newPoint, 1, AB, pOut);
					}
					return PointSimplex(newPoint, pOut);
				}
				return TriangleSimplex(newPoint,2,ABC,pOut);
			}
		}
		break;
	case 5:
		{
			// in front of ADB & ABC
			d = DotProduct(ADBxAB, newPoint.position);
			if ( d < 0 )
			{
				d = DotProduct(ABxABC, newPoint.position);
				if ( d < 0 )
				{
					d = DotProduct(AB,newPoint.position);
					if ( d < 0 )
					{
						// edge AB
						return EdgeSimplex( newPoint, 1, AB , pOut);
					}
					// point A
					return PointSimplex(newPoint, pOut);
				}
				else
				{
					d = DotProduct(ABCxAC, newPoint.position);
					if ( d < 0 )
					{
						// edge AC
						return EdgeSimplex(newPoint, 0, AC, pOut);
					}
					// face ABC
					return TriangleSimplex(newPoint,2,ABC,pOut);
				}
			}
			else
			{
				d = DotProduct(ADxADB, newPoint.position);
				if ( d < 0 )
				{
					d = DotProduct(AD, newPoint.position);
					if ( d < 0 )
					{
						// edge AD
						verts[0] = verts[2]; // collapse BCD to D
						return EdgeSimplex(newPoint, 1, AD, pOut);
					}
					return PointSimplex(newPoint, pOut);
				}
				return TriangleSimplex(newPoint,1,ADB,pOut);
			}
		}
		break;
	case 6:
		{
			// in front of ACD & ADB
			d = DotProduct(ACDxAD, newPoint.position);
			if ( d < 0 )
			{
				d = DotProduct(ADxADB, newPoint.position);
				if ( d < 0 )
				{
					d = DotProduct(AD,newPoint.position);
					if ( d < 0 )
					{
						// edge AD
						verts[0] = verts[2]; // collapse BCD to D
						return EdgeSimplex(newPoint, 1, AD, pOut);
					}
					// point A
					return PointSimplex(newPoint, pOut);
				}
				else
				{
					d = DotProduct(ADBxAB, newPoint.position);
					if ( d < 0 )
					{
						// edge AB
						return EdgeSimplex(newPoint, 1, AB, pOut);
					}
					// face ADB
					return TriangleSimplex(newPoint,1,ADB, pOut);
				}
			}
			else
			{
				d = DotProduct(ACxACD, newPoint.position);
				if ( d < 0 )
				{
					d = DotProduct(AC, newPoint.position);
					if ( d < 0 )
					{
						// edge AC
						return EdgeSimplex(newPoint, 0, AC, pOut);
					}
					return PointSimplex(newPoint, pOut);
				}
				return TriangleSimplex(newPoint,0,ACD, pOut);
			}
		}
		break;
	case 7:
		{
			d = DotProduct(AB, newPoint.position);
			if ( d < 0 )
			{
				return EdgeSimplex(newPoint, 1, AB, pOut);
			}
			else
			{
				d = DotProduct(AC, newPoint.position);
				if ( d < 0 )
				{
					return EdgeSimplex(newPoint, 0, AC, pOut);
				}
				else
				{
					d = DotProduct(AD, newPoint.position);
					if ( d < 0 )
					{
						verts[0] = verts[2]; // collapse BCD to D
						return EdgeSimplex(newPoint, 1, AD, pOut);
					}
					return PointSimplex(newPoint, pOut);
				}
			}
		}
	}
	verts[3] = newPoint;
	vertCount = 4;
	return true;
}


bool simplex_t::SolveGJKSet( const simplexvert_t &w, Vector *pOut )
{
	VPROF("TraceSolver::simplex::SolveGJKSet");

#if 0
	for ( int v = 0; v < vertCount; v++ )
	{
		for ( int v2 = v+1; v2 < vertCount; v2++ )
		{
			if ( (verts[v].obstacleIndex == verts[v2].obstacleIndex) &&
				(verts[v].sweepIndex == verts[v2].sweepIndex) &&
				(verts[v].testIndex == verts[v2].testIndex) )
			{
				// same vert in the list twice!  degenerate
				Assert(0);
			}
		}
	}
#endif

	switch( vertCount )
	{
	case 0:
		vertCount = 1;
		verts[0] = w;
		*pOut = w.position;
		return false;
	case 1:
		return SolveVoronoiRegion2( w, pOut );
	case 2:
		return SolveVoronoiRegion3( w, pOut );
	case 3:
		return SolveVoronoiRegion4( w, pOut );
	}

	return true;
}


void CalculateSeparatingPlane( trace_t *ptr, ITraceObject *sweepObject, CTraceRay *ray, ITraceObject *obstacle, simplex_t &simplex )
{
	int testCount = 1, obstacleCount = 1;
	unsigned int testIndex[4], obstacleIndex[4];
	testIndex[0] = simplex.verts[0].testIndex;
	obstacleIndex[0] = simplex.verts[0].obstacleIndex;
	Assert( simplex.vertCount <= 4 );
	int i, j;
	for ( i = 1; i < simplex.vertCount; i++ )
	{
		for ( j = 0; j < obstacleCount; j++ )
		{
			if ( obstacleIndex[j] == simplex.verts[i].obstacleIndex )
				break;
		}
		if ( j == obstacleCount )
		{
			obstacleIndex[obstacleCount++] = simplex.verts[i].obstacleIndex;
		}

		for ( j = 0; j < testCount; j++ )
		{
			if ( testIndex[j] == simplex.verts[i].testIndex )
				break;
		}
		if ( j == testCount )
		{
			testIndex[testCount++] = simplex.verts[i].testIndex;
		}
	}

	if ( simplex.vertCount < 3 )
	{
		if ( simplex.vertCount == 2 && testCount == 2 )
		{
			// edge / point
			Vector t0 = sweepObject->GetVertByIndex( testIndex[0] );
			Vector t1 = sweepObject->GetVertByIndex( testIndex[1] );
			Vector edge = t1-t0;
			Vector tangent = CrossProduct( edge, ray->m_delta );
			ptr->plane.normal = CrossProduct( edge, tangent );
			VectorNormalize( ptr->plane.normal );
			ptr->plane.dist = DotProduct( t0 + ptr->endpos, ptr->plane.normal );
			return;
		}
	}
	if ( testCount == 3 )
	{
		// face / xxx
		Vector t0 = sweepObject->GetVertByIndex( testIndex[0] );
		Vector t1 = sweepObject->GetVertByIndex( testIndex[1] );
		Vector t2 = sweepObject->GetVertByIndex( testIndex[2] );
		ptr->plane.normal = CrossProduct( t1-t0, t2-t0 );
		VectorNormalize( ptr->plane.normal );
		if ( DotProduct( ptr->plane.normal, ray->m_delta ) > 0 )
		{
			ptr->plane.normal = -ptr->plane.normal;
		}
		ptr->plane.dist = DotProduct( t0 + ptr->endpos, ptr->plane.normal );
	}
	else if ( testCount == 2 && obstacleCount == 2 )
	{
		// edge / edge
		Vector t0 = sweepObject->GetVertByIndex( testIndex[0] );
		Vector t1 = sweepObject->GetVertByIndex( testIndex[1] );
		Vector t2 = obstacle->GetVertByIndex( obstacleIndex[0] );
		Vector t3 = obstacle->GetVertByIndex( obstacleIndex[1] );
		ptr->plane.normal = CrossProduct( t1-t0, t3-t2 );
		VectorNormalize( ptr->plane.normal );
		if ( DotProduct( ptr->plane.normal, ray->m_delta ) > 0 )
		{
			ptr->plane.normal = -ptr->plane.normal;
		}
		ptr->plane.dist = DotProduct( t0 + ptr->endpos, ptr->plane.normal );
	}
	else if ( obstacleCount == 3 )
	{
		// xxx / face
		Vector t0 = obstacle->GetVertByIndex( obstacleIndex[0] );
		Vector t1 = obstacle->GetVertByIndex( obstacleIndex[1] );
		Vector t2 = obstacle->GetVertByIndex( obstacleIndex[2] );
		ptr->plane.normal = CrossProduct( t1-t0, t2-t0 );
		VectorNormalize( ptr->plane.normal );
		if ( DotProduct( ptr->plane.normal, ray->m_delta ) > 0 )
		{
			ptr->plane.normal = -ptr->plane.normal;
		}
		ptr->plane.dist = DotProduct( t0, ptr->plane.normal );
	}
	else
	{
		ptr->plane.normal = -ray->m_dir;
		if ( simplex.vertCount )
		{
			ptr->plane.dist = DotProduct( ptr->plane.normal, obstacle->GetVertByIndex( simplex.verts[0].obstacleIndex ) );
		}
		else
			ptr->plane.dist = 0.f;
	}
}

// clip a direction vector to a plane (ray begins at the origin)
inline float Clip( const Vector &dir, const Vector &pos, const Vector &normal )
{
	float dist = DotProduct(pos, normal);
	float cosTheta = DotProduct(dir,normal);
	if ( cosTheta > 0 )
		return dist / cosTheta;

	// parallel or not facing the plane
	return 1e24f;
}

// This is the first iteration of solving time of intersection.
// It needs to test all 4 faces of the tetrahedron to find the one the ray leaves through
// this is done by finding the closest plane by clipping the ray to each plane
Vector simplex_t::ClipRayToTetrahedronBase( const Vector &dir )
{
	Vector AB = verts[0].position - verts[3].position;
	Vector AC = verts[1].position - verts[3].position;
	Vector AD = verts[2].position - verts[3].position;
	Vector BC = verts[1].position - verts[0].position;
	Vector BD = verts[2].position - verts[0].position;

	// compute face normals
	Vector ABC, ACD, ADB, BCD;
	CrossProduct( AB, AC, ABC );
	CrossProduct( AC, AD, ACD );
	CrossProduct( AD, AB, ADB );
	CrossProduct( BD, BC, BCD );	// flipped to point out of the tetrahedron

	// NOTE: These cancel out in the clipping equation
	//VectorNormalize(ABC);
	//VectorNormalize(ACD);
	//VectorNormalize(ADB);
	//VectorNormalize(BCD);

	// Assert valid tetrahedron/winding order
#if CHECK_TOI_CALCS
	Assert(DotProduct(verts[2].position, ABC) < DotProduct(verts[3].position, ABC ));
	Assert(DotProduct(verts[0].position, ACD) < DotProduct(verts[3].position, ACD ));
	Assert(DotProduct(verts[1].position, ADB) < DotProduct(verts[3].position, ADB ));
	Assert(DotProduct(verts[3].position, BCD) < DotProduct(verts[0].position, BCD ));
#endif

	float dists[4];
	dists[0] = Clip( dir, verts[3].position, ABC );
	dists[1] = Clip( dir, verts[3].position, ACD );
	dists[2] = Clip( dir, verts[3].position, ADB );
	dists[3] = Clip( dir, verts[0].position, BCD );
	float dmin = dists[3];
	int best = 3;
	for ( int i = 0; i < 3; i++ )
	{
		if ( dists[i] < dmin )
		{
			best = i;
			dmin = dists[i];
		}
	}

	vertCount = 3;
	// push back down to a triangle
	// Note that we need to preserve winding so that the vector order assumptions above are still valid!
	switch( best )
	{
	case 0:
		verts[2] = verts[3];
		return ABC;
	case 1:
		verts[0] = verts[3];
		return ACD;
	case 2:
		verts[1] = verts[3];
		return ADB;
	case 3:
		// swap 1 & 2
		verts[3] = verts[1];
		verts[1] = verts[2];
		verts[2] = verts[3];
		return BCD;
	}
	Assert(0); // failed!
	return vec3_origin;
}

// for subsequent iterations you don't need to test one of the faces (face you previously left through)
// so only test the three faces involving the new point A
Vector simplex_t::ClipRayToTetrahedron( const Vector &dir )
{
	Vector AB = verts[0].position - verts[3].position;
	Vector AC = verts[1].position - verts[3].position;
	Vector AD = verts[2].position - verts[3].position;

	// compute face normals
	Vector ABC, ACD, ADB;
	CrossProduct( AB, AC, ABC );
	CrossProduct( AC, AD, ACD );
	CrossProduct( AD, AB, ADB );

	// NOTE: These cancel out in the clipping equation
	//VectorNormalize(ABC);
	//VectorNormalize(ACD);
	//VectorNormalize(ADB);

	// Assert valid tetrahedron/winding order
#if CHECK_TOI_CALCS
	Assert(DotProduct(verts[2].position, ABC) < DotProduct(verts[3].position, ABC ));
	Assert(DotProduct(verts[0].position, ACD) < DotProduct(verts[3].position, ACD ));
	Assert(DotProduct(verts[1].position, ADB) < DotProduct(verts[3].position, ADB ));
#endif

	float dists[3];
	dists[0] = Clip( dir, verts[3].position, ABC );
	dists[1] = Clip( dir, verts[3].position, ACD );
	dists[2] = Clip( dir, verts[3].position, ADB );

	float dmin = dists[2];
	int best = 2;
	for ( int i = 0; i < 2; i++ )
	{
		if ( dists[i] < dmin )
		{
			best = i;
			dmin = dists[i];
		}
	}

	vertCount = 3;
	// push back down to a triangle
	// Note that we need to preserve winding so that the vector order assumptions above are still valid!
	switch( best )
	{
	case 0:
		verts[2] = verts[3];
		return ABC;
	case 1:
		verts[0] = verts[3];
		return ACD;
	case 2:
		verts[1] = verts[3];
		return ADB;
	}
	return vec3_origin;
}

float simplex_t::ClipRayToTriangle( const Vector &dir, float epsilon )
{
	Vector AB = verts[0].position - verts[2].position;
	Vector AC = verts[1].position - verts[2].position;
	Vector BC = verts[1].position - verts[0].position;

	// compute face normal
	Vector ABC;
	CrossProduct( AB, AC, ABC );  // this points toward the origin
	VectorNormalize(ABC);
	Vector edgeAB, edgeAC, edgeBC;
	// these should point out of the triangle
	CrossProduct( AB, ABC, edgeAB );
	CrossProduct( ABC, AC, edgeAC );
	CrossProduct( BC, ABC, edgeBC );

	// NOTE: These cancel out in the clipping equation
	VectorNormalize(edgeAB);
	VectorNormalize(edgeAC);
	VectorNormalize(edgeBC);

#if CHECK_TOI_CALCS
	Assert(DotProduct(verts[2].position, ABC) < 0); // points toward
	// Assert valid triangle/winding order (all normals point away from the origin)
	Assert(DotProduct(verts[2].position, edgeAB) > 0);
	Assert(DotProduct(verts[2].position, edgeAC) > 0);
	Assert(DotProduct(verts[0].position, edgeBC) > 0);
#endif

	float dists[3];
	dists[0] = Clip( dir, verts[0].position, edgeAB );
	dists[1] = Clip( dir, verts[1].position, edgeAC );
	dists[2] = Clip( dir, verts[1].position, edgeBC );
	Vector *normals[3] = {&edgeAB, &edgeAC, &edgeBC};

	float dmin = dists[0];
	int best = 0;
	if ( dists[1] < dmin )
	{
		dmin = dists[1];
		best = 1;
	}
	if ( dists[2] < dmin )
	{
		best = 2;
		dmin = dists[2];
	}
	float dot = DotProduct( dir, *normals[best] );
	if ( dot <= 0 )
		return 1e24f;
	dmin += epsilon/dot;

	return dmin;
}

// Solve for time of intersection along the sweep
// Do this by iteratively clipping the ray to the tetrahedron containing the origin
// when a triangle is found intersecting the ray, reduce the simplex to that triangle
// and then re-expand it to a tetrahedron using the support point normal to the triangle (away from the origin)
// iterate until no new points can be found.  That's the surface of the sum.
float CTraceSolver::SolveMeshIntersection( simplex_t &simplex )
{
	Vector tmp;
	Assert( simplex.vertCount == 4 );
	if ( simplex.vertCount < 4 )
		return 0.0f;

	Vector v = simplex.ClipRayToTetrahedronBase( m_ray->m_dir );

	simplexvert_t vert;
	// safe loop, max 100 iterations
	for ( int i = 0; i < 100; i++ )
	{
		VectorNormalize(v);
		vert.testIndex = m_sweepObject->SupportMap( v, &vert.position );
		vert.sweepIndex = m_ray->SupportMap( v, &tmp );
		VectorAdd( vert.position, tmp, vert.position );
		vert.obstacleIndex = m_obstacle->SupportMap( -v, &tmp );
		VectorSubtract( vert.position, tmp, vert.position );

		// map the new separating axis (NOTE: This test is inverted from the GJK - we are trying to get out, not in)
		// found a separating axis, we've moved the sweep back enough
		float dist = DotProduct( v, simplex.verts[0].position ) + TEST_EPSILON;
		if ( DotProduct( v, vert.position ) <= dist )
		{
			Vector BC = simplex.verts[1].position - simplex.verts[0].position;
			Vector BD = simplex.verts[2].position - simplex.verts[0].position;

			// compute face normals
			Vector BCD;
			CrossProduct( BC, BD, BCD );
			// NOTE: This cancels out inside Clip()
			//VectorNormalize( BCD );
			// clip ray to triangle
			return Clip( m_ray->m_dir, simplex.verts[0].position, BCD );
		}

		// add the new vert
		simplex.verts[simplex.vertCount] = vert;
		simplex.vertCount++;
		v = simplex.ClipRayToTetrahedron( m_ray->m_dir );
	}

	Assert(0);
	return 0.0f;
}

// similar to SolveMeshIntersection, but solves projected into the 2D triangle simplex remaining
// this is used for the near miss case
float CTraceSolver::SolveMeshIntersection2D( simplex_t &simplex )
{
	AssertMsg( simplex.vertCount == 3, "simplex.vertCount != 3: %d", simplex.vertCount );
	if ( simplex.vertCount != 3 )
		return 0.0f;

	// note: This should really do this iteratively in case the triangle is coplanar with another triangle that 
	// is between this one and the edge of the sum in this plane
	float dist = simplex.ClipRayToTriangle( m_ray->m_dir, m_epsilon );
	return dist;
}


static const Vector g_xpos(1,0,0), g_xneg(-1,0,0);
static const Vector g_ypos(0,1,0), g_yneg(0,-1,0);
static const Vector g_zpos(0,0,1), g_zneg(0,0,-1);

void TraceGetAABB_r( Vector *pMins, Vector *pMaxs, const IVP_Compact_Ledgetree_Node *node, CTraceIVP &ivp )
{
	if ( node->is_terminal() == IVP_TRUE )
	{
		Vector tmp;
		ivp.SetLedge( node->get_compact_ledge() );
		ivp.SupportMap( g_xneg, &tmp );
		AddPointToBounds( tmp, *pMins, *pMaxs );
		ivp.SupportMap( g_yneg, &tmp );
		AddPointToBounds( tmp, *pMins, *pMaxs );
		ivp.SupportMap( g_zneg, &tmp );
		AddPointToBounds( tmp, *pMins, *pMaxs );
		ivp.SupportMap( g_xpos, &tmp );
		AddPointToBounds( tmp, *pMins, *pMaxs );
		ivp.SupportMap( g_ypos, &tmp );
		AddPointToBounds( tmp, *pMins, *pMaxs );
		ivp.SupportMap( g_zpos, &tmp );
		AddPointToBounds( tmp, *pMins, *pMaxs );
		return;
	}

	TraceGetAABB_r( pMins, pMaxs, node->left_son(), ivp );
	TraceGetAABB_r( pMins, pMaxs, node->right_son(), ivp );
}

void CPhysicsTrace::GetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles )
{
	CTraceIVP ivp( pCollide, collideOrigin, collideAngles );

	if ( ivp.SetSingleConvex() )
	{
		Vector tmp;
		ivp.SupportMap( g_xneg, &tmp );
		pMins->x = tmp.x;
		ivp.SupportMap( g_yneg, &tmp );
		pMins->y = tmp.y;
		ivp.SupportMap( g_zneg, &tmp );
		pMins->z = tmp.z;

		ivp.SupportMap( g_xpos, &tmp );
		pMaxs->x = tmp.x;
		ivp.SupportMap( g_ypos, &tmp );
		pMaxs->y = tmp.y;
		ivp.SupportMap( g_zpos, &tmp );
		pMaxs->z = tmp.z;
	}
	else
	{
		const IVP_Compact_Ledgetree_Node *lt_node_root;
		lt_node_root = pCollide->GetCompactSurface()->get_compact_ledge_tree_root();
		ClearBounds( *pMins, *pMaxs );
		TraceGetAABB_r( pMins, pMaxs, lt_node_root, ivp );
	}
	// JAY: Disable this here, do it in the engine instead.  That way the tools get
	// accurate bboxes
#if 0
	const float radius = g_PhysicsUnits.collisionSweepEpsilon;
	mins -= Vector(radius,radius,radius);
	maxs += Vector(radius,radius,radius);
#endif
}

void TraceGetExtent_r( const IVP_Compact_Ledgetree_Node *node, CTraceIVP &ivp, const Vector &dir, float &dot, Vector &point )
{
	if ( node->is_terminal() == IVP_TRUE )
	{
		ivp.SetLedge( node->get_compact_ledge() );
		Vector tmp;
		ivp.SupportMap( dir, &tmp );
		float newDot = DotProduct( tmp, dir );

		if ( newDot > dot )
		{
			dot = newDot;
			point = tmp;
		}
		return;
	}

	TraceGetExtent_r( node->left_son(), ivp, dir, dot, point );
	TraceGetExtent_r( node->right_son(), ivp, dir, dot, point );
}


Vector CPhysicsTrace::GetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction )
{
	CTraceIVP ivp( pCollide, collideOrigin, collideAngles );

	if ( ivp.SetSingleConvex() )
	{
		Vector tmp;
		ivp.SupportMap( direction, &tmp );
		return tmp;
	}
	else
	{
		const IVP_Compact_Ledgetree_Node *lt_node_root;
		lt_node_root = pCollide->GetCompactSurface()->get_compact_ledge_tree_root();
		Vector out = vec3_origin;
		float tmp = -1e6f;
		TraceGetExtent_r( lt_node_root, ivp, direction, tmp, out );
		return out;
	}
}

bool CPhysicsTrace::IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone )
{
	trace_t tr;
	CM_ClearTrace( &tr );
	bool bPoint = (boxAbsMins == boxAbsMaxs) ? true : false;
	CTraceAABB box( boxAbsMins - cone.origin, boxAbsMaxs - cone.origin, bPoint );
	CTraceCone traceCone( cone, -cone.origin );

	// offset the space of this sweep so that the surface is at the origin of the solution space
	CTraceRay ray( vec3_origin, vec3_origin );

	CTraceSolver solver( &tr, &box, &ray, &traceCone, boxAbsMaxs );
	solver.DoSweep();

	return tr.startsolid;
}



CPhysicsTrace::CPhysicsTrace()
{
}

CPhysicsTrace::~CPhysicsTrace()
{
}

CVisitHash::CVisitHash()
{
	m_vertVisitID = 1;
	memset( m_vertVisit, 0, sizeof(m_vertVisit) );
}
