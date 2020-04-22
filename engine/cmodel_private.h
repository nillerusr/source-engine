//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CMODEL_PRIVATE_H
#define CMODEL_PRIVATE_H
#pragma once

#include "qlimits.h"
#include "bitvec.h"
#include "bspfile.h"
#include "utlbuffer.h"

#include "filesystem.h"
#include "filesystem_engine.h"

#include "dispcoll_common.h"

class CDispCollTree;
class CCollisionBSPData;
struct cbrush_t;

#define MAX_CHECK_COUNT_DEPTH 2

struct TraceVisits_t
{
	CVarBitVec m_Brushes;
	CVarBitVec m_Disps;
};

typedef uint32 TraceCounter_t;
typedef CUtlVector<TraceCounter_t> CTraceCounterVec;

struct TraceInfo_t
{
	TraceInfo_t()
	{
		memset( this, 0, offsetof( TraceInfo_t, m_BrushCounters ) );
		m_nCheckDepth = -1;
	}

	Vector m_start;
	Vector m_end;
	Vector m_mins;
	Vector m_maxs;
	Vector m_extents;
	Vector m_delta;
	Vector m_invDelta;

	trace_t m_trace;
	trace_t m_stabTrace;

	int m_contents;
	bool m_ispoint;
	bool m_isswept;

	// BSP Data
	CCollisionBSPData *m_pBSPData;

	// Displacement Data
	Vector m_DispStabDir;		// the direction to stab in
	int m_bDispHit;				// hit displacement surface last

	bool m_bCheckPrimary;
	int m_nCheckDepth;
	TraceCounter_t m_Count[MAX_CHECK_COUNT_DEPTH];

	CTraceCounterVec m_BrushCounters[MAX_CHECK_COUNT_DEPTH];
	CTraceCounterVec m_DispCounters[MAX_CHECK_COUNT_DEPTH];

	TraceCounter_t GetCount()				{ return m_Count[m_nCheckDepth]; }
	TraceCounter_t *GetBrushCounters()		{ return m_BrushCounters[m_nCheckDepth].Base(); }
	TraceCounter_t *GetDispCounters()		{ return m_DispCounters[m_nCheckDepth].Base(); }

	bool Visit( cbrush_t *pBrush, int ndxBrush );
	bool Visit( int dispIndex );

	bool Visit( cbrush_t *pBrush, int ndxBrush, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters );
	bool Visit( int dispIndex, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters );
};

extern TraceInfo_t g_PrimaryTraceInfo;

#define		NEVER_UPDATED		-99999

//=============================================================================
//
// Local CModel Structures (cmodel.cpp and cmodel_disp.cpp)
//

struct cbrushside_t
{
	cplane_t	*plane;
	unsigned short surfaceIndex;
	unsigned short bBevel;							// is the side a bevel plane?
};

#define NUMSIDES_BOXBRUSH	0xFFFF

struct cbrush_t
{
	int				contents;
	unsigned short	numsides;
	unsigned short	firstbrushside;

	inline int GetBox() const { return firstbrushside; }
	inline void SetBox( int boxID )
	{
		numsides = NUMSIDES_BOXBRUSH;
		firstbrushside = boxID;
	}
	inline bool IsBox() const { return numsides == NUMSIDES_BOXBRUSH ? true : false; }
};

// 48-bytes, aligned to 16-byte boundary
// this is a brush that is an AABB.  It's encoded this way instead of with 6 brushsides
struct cboxbrush_t
{
	VectorAligned	mins;
	VectorAligned	maxs;

	unsigned short	surfaceIndex[6];
	unsigned short	pad2[2];
};

struct cleaf_t
{
	int			    contents;
	short			cluster;
	short			area : 9;
	short			flags : 7;
	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
	unsigned short	dispListStart;
	unsigned short	dispCount;
};

struct carea_t
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;							// if two areas have equal floodnums, they are connected
	int		floodvalid;
};


struct cnode_t
{
	cplane_t	*plane;
	int			children[2];		// negative numbers are leafs
};


// global collision checkcount
TraceInfo_t *BeginTrace();
void PushTraceVisits( TraceInfo_t *pTraceInfo );
void PopTraceVisits( TraceInfo_t *pTraceInfo );
void EndTrace( TraceInfo_t *&pTraceInfo );


//-----------------------------------------------------------------------------
// We keep running into overflow errors here. This is to avoid that problem
//-----------------------------------------------------------------------------
template <class T>
class CRangeValidatedArray
{
public:
	void Attach( int nCount, T* pData );
	void Detach();

	T &operator[]( int i );
	const T &operator[]( int i ) const;

	T *Base();

private:
	T *m_pArray;

#ifdef DBGFLAG_ASSERT
	int m_nCount;
#endif
};

template <class T>
inline T &CRangeValidatedArray<T>::operator[]( int i )
{
	Assert( (i >= 0) && (i < m_nCount) );
	return m_pArray[i];
}

template <class T>
inline const T &CRangeValidatedArray<T>::operator[]( int i ) const
{
	Assert( (i >= 0) && (i < m_nCount) );
	return m_pArray[i];
}

template <class T>
inline void CRangeValidatedArray<T>::Attach( int nCount, T* pData )
{
	m_pArray = pData;

#ifdef DBGFLAG_ASSERT
	m_nCount = nCount;
#endif
}

template <class T>
inline void CRangeValidatedArray<T>::Detach()
{
	m_pArray = NULL;

#ifdef DBGFLAG_ASSERT
	m_nCount = 0;
#endif
}

template <class T>
inline T *CRangeValidatedArray<T>::Base()
{
	return m_pArray;
}

//-----------------------------------------------------------------------------
// A dumpable version of RangeValidatedArray
//-----------------------------------------------------------------------------
#include "tier0/memdbgon.h"
template <class T> 
class CDiscardableArray 
{
public:
	CDiscardableArray()
	{
		m_nCount = 0;
		m_nOffset = -1;
		memset( m_pFilename, 0, sizeof( m_pFilename ) );
	}

	~CDiscardableArray()
	{
	}

	void Init( char* pFilename, int nOffset, int nCount, void *pData = NULL )
	{
		if ( m_buf.TellPut() )
		{
			m_buf.Purge();
		}

		m_nCount = nCount;
		V_strcpy_safe( m_pFilename, pFilename );
		m_nOffset = nOffset;

		// can preload as required
		if ( pData )
		{
			m_buf.Put( pData, nCount );
		}
	}

	// Either get or load the array as needed:
	T* Get()
	{
		if ( !m_buf.TellPut() )
		{
			MEM_ALLOC_CREDIT();

			if ( !g_pFileSystem->ReadFile( m_pFilename, NULL, m_buf, sizeof(T) * m_nCount, m_nOffset ) )
			{
				return NULL;
			}
		}

		return (T *)m_buf.PeekGet();
	}

	void Discard()
	{
		// TODO(johns): Neutered -- Tear this class out. This can no longer be discarded with transparently compressed
		//              BSPs. This is on the range of 500k of memory, and is probably overly complex for the savings in
		//              the current era.
		DevMsg("TODO: Refusing to discard %u bytes\n", sizeof(T) * m_nCount);
		// m_buf.Purge();
	}

protected:
	int			m_nCount;
	CUtlBuffer	m_buf;
	char		m_pFilename[256];
	int			m_nOffset;
};
#include "tier0/memdbgoff.h"

const int SURFACE_INDEX_INVALID = 0xFFFF;

//=============================================================================
//
// Collision BSP Data Class
//
class CCollisionBSPData
{
public:
	// This is sort of a hack, but it was a little too painful to do this any other way
	// The goal of this dude is to allow us to override the tree with some
	// other tree (or a subtree)
	cnode_t*					map_rootnode;

	char						map_name[MAX_QPATH];
	static csurface_t			nullsurface;

	int									numbrushsides;
	CRangeValidatedArray<cbrushside_t>	map_brushsides;
	int									numboxbrushes;
	CRangeValidatedArray<cboxbrush_t>	map_boxbrushes;
	int									numplanes;
	CRangeValidatedArray<cplane_t>		map_planes;
	int									numnodes;
	CRangeValidatedArray<cnode_t>		map_nodes;
	int									numleafs;				// allow leaf funcs to be called without a map
	CRangeValidatedArray<cleaf_t>		map_leafs;
	int									emptyleaf, solidleaf;
	int									numleafbrushes;
	CRangeValidatedArray<unsigned short> map_leafbrushes;
	int									numcmodels;
	CRangeValidatedArray<cmodel_t>		map_cmodels;
	int									numbrushes;
	CRangeValidatedArray<cbrush_t>		map_brushes;
	int									numdisplist;
	CRangeValidatedArray<unsigned short> map_dispList;
	
	// this points to the whole block of memory for vis data, but it is used to
	// reference the header at the top of the block.
	int									numvisibility;
	dvis_t								*map_vis;

	int									numentitychars;
	CDiscardableArray<char>				map_entitystring;

	int									numareas;
	CRangeValidatedArray<carea_t>		map_areas;
	int									numareaportals;
	CRangeValidatedArray<dareaportal_t>	map_areaportals;
	int									numclusters;
	char								*map_nullname;
	int									numtextures;
	char								*map_texturenames;
	CRangeValidatedArray<csurface_t>	map_surfaces;
	int									floodvalid;
	int									numportalopen;
	CRangeValidatedArray<bool>			portalopen;

	csurface_t							*GetSurfaceAtIndex( unsigned short surfaceIndex );
};

//=============================================================================
//
// physics collision
//
class IPhysicsSurfaceProps;
class IPhysicsCollision;

extern IPhysicsSurfaceProps	*physprop;
extern IPhysicsCollision	*physcollision;

//=============================================================================
//
// This might eventually become a class/interface when we want multiple instances
// etc.......for now....
//
extern csurface_t nullsurface;

extern bool bStartSolidDisp;

bool CollisionBSPData_Init( CCollisionBSPData *pBSPData );
void CollisionBSPData_Destroy( CCollisionBSPData *pBSPData );
void CollisionBSPData_LinkPhysics( void );

void CollisionBSPData_PreLoad( CCollisionBSPData *pBSPData );
bool CollisionBSPData_Load( const char *pName, CCollisionBSPData *pBSPData );
void CollisionBSPData_PostLoad( void );

//-----------------------------------------------------------------------------
// Returns the collision tree associated with the ith displacement
//-----------------------------------------------------------------------------

CDispCollTree* CollisionBSPData_GetCollisionTree( int i );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CCollisionBSPData *GetCollisionBSPData( void /*int ndxBSP*/ )
{
	extern CCollisionBSPData g_BSPData;								// the global collision bsp
	return &g_BSPData;
}

//=============================================================================
//
// Collision Model Counts
//
#ifdef COUNT_COLLISIONS
class CCollisionCounts
{
public:
	int		m_PointContents;
	int		m_Traces;
	int		m_BrushTraces;
	int		m_DispTraces;
	int		m_Stabs;
};

void CollisionCounts_Init( CCollisionCounts *pCounts );

extern CCollisionCounts g_CollisionCounts;
#endif


//=============================================================================
//
// Older Code That Should Live Here!!!!
// a shared file should contain all the CDispCollTree stuff
//

//=============================================================================
//
// Displacement Collision Functions and Data
//
// UNDONE: Find a way to overlap the counter/contents bits with mins.w/maxs.w without hosing performance
struct alignedbbox_t
{
	VectorAligned	mins;
	VectorAligned	maxs;
	int				dispCounter;
	int				dispContents;
	int				pad[2];
	void SetCounter(int counter)
	{
		dispCounter = counter;
	}
	int GetCounter()
	{
		return dispCounter;
	}
	void SetContents(int contents)
	{
		dispContents = contents;
	}
	int GetContents()
	{
		return dispContents;
	}
	void Init( const Vector &minsIn, const Vector &maxsIn, int counterIn, int contentsIn )
	{
		mins = minsIn;
		SetCounter(counterIn);
		maxs = maxsIn;
		SetContents(contentsIn);
	}
};
extern int g_DispCollTreeCount;
extern CDispCollTree *g_pDispCollTrees;
extern alignedbbox_t *g_pDispBounds;
extern csurface_t dispSurf;

// memory allocation/de-allocation
void DispCollTrees_FreeLeafList( CCollisionBSPData *pBSPData );

// setup
void CM_DispTreeLeafnum( CCollisionBSPData *pBSPData );

// collision
void CM_TestInDispTree( TraceInfo_t *pTraceInfo, cleaf_t *pLeaf, Vector const &traceStart, 
				Vector const &boxMin, Vector const &boxMax, int collisionMask, trace_t *pTrace );
template <bool IS_POINT>
void FASTCALL CM_TraceToDispTree( TraceInfo_t *pTraceInfo, CDispCollTree *pDispTree, float startFrac, float endFrac );
void CM_PostTraceToDispTree( TraceInfo_t *pTraceInfo );

//=============================================================================
//
// profiling purposes only -- remove when done!!!
//

void CM_TestBoxInBrush ( const Vector& mins, const Vector& maxs, const Vector& p1,
					  trace_t *trace, cbrush_t *brush, BOOL bDispSurf );
void FASTCALL CM_RecursiveHullCheck ( TraceInfo_t *pTraceInfo, int num, const float p1f, const float p2f );


//=============================================================================

inline bool TraceInfo_t::Visit( cbrush_t *pBrush, int ndxBrush, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters )
{
	TraceCounter_t * RESTRICT pCounter = pCachedCounters + ndxBrush;

	if ( *pCounter == cachedCount )
	{
		return false;
	}

	*pCounter = cachedCount;
	return true;
}

FORCEINLINE bool TraceInfo_t::Visit( int dispCounter, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters )
{
	TraceCounter_t * RESTRICT pCounter = pCachedCounters + dispCounter;

	if ( *pCounter == cachedCount )
	{
		return false;
	}

	*pCounter = cachedCount;
	return true;
}

FORCEINLINE bool TraceInfo_t::Visit( cbrush_t *pBrush, int ndxBrush )
{
	return Visit( pBrush, ndxBrush, GetCount(), GetBrushCounters() );
}

FORCEINLINE bool TraceInfo_t::Visit( int dispIndex )
{
	return Visit( dispIndex, GetCount(), GetDispCounters() );
}

#endif // CMODEL_PRIVATE_H
