//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// @TODO: The features and implementation of these class classes require overly
//		  broad mutexing. Needs to be addressed. (toml 3-20-06)
//
//===========================================================================//

#include "mathlib/vector.h"
#include "utlhash.h"
#include "utllinkedlist.h"
#include "utllinkedlist.h"
#include "ispatialpartitioninternal.h"
#include "bsptreedata.h"
#include "worldsize.h"
#include "cmodel.h"
#include "sys_dll.h"
#include "collisionutils.h"
#include "debugoverlay.h"
#include "tier0/vprof.h"
#include "tier1/utlbuffer.h"
#include "filesystem_engine.h"
#include "filesystem.h"
#include "tier1/convar.h"
#include "tier1/memstack.h"
#include "enginethreads.h"
#include "datacache/imdlcache.h"
#include "tier2/renderutils.h"
#include "bitvec.h"
#include "tier1/mempool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CVoxelTree;
class CIntersectSweptBox;

#define SPHASH_LEVEL_SKIP	2

#define SPHASH_VOXEL_SIZE	256			// must power of 2
#define SPHASH_VOXEL_SHIFT	8			// shift for voxel size

#define SPHASH_VOXEL_LARGE	65536.0f

#define SPHASH_HANDLELIST_BLOCK		256
#define SPHASH_LEAFLIST_BLOCK		512
#define SPHASH_ENTITYLIST_BLOCK		256
#define SPHASH_BUCKET_COUNT			512

#define SPHASH_EPS					0.03125f

enum PartitionTrees_t
{
	CLIENT_TREE,
	SERVER_TREE,
	NUM_TREES,
};

class CPartitionVisitor;

#if defined( _X360 )
#pragma bitfield_order( push, lsb_to_msb )
#endif
union Voxel_t
{
	struct
	{
		unsigned int x:11;
		unsigned int y:11;
		unsigned int z:10;
	} bitsVoxel;
	unsigned int uiVoxel;  
};
#if defined( _X360 )
#pragma bitfield_order( pop )
#endif

enum EntityInfoFlags_t
{
	ENTITY_HIDDEN	=	( 1 << 0 ),
	IN_CLIENT_TREE	=	( 1 << 1 ),
	IN_SERVER_TREE	=	( 1 << 2 ),
};


struct EntityInfo_t
{
	Vector						m_vecMin;			// Min/Max of entity
	Vector						m_vecMax;
	IHandleEntity *				m_pHandleEntity;	// Entity handle.
	unsigned short				m_fList;			// Which lists is it in?
	uint8						m_flags;
	char						m_nLevel[NUM_TREES];	// Which level voxel tree is it in?
	unsigned short				m_nVisitBit[NUM_TREES];
	int							m_iLeafList[NUM_TREES];	// Index into the leaf pool - leaf list for entity (m_aLeafList).
};


struct LeafListData_t
{
	UtlHashFastHandle_t		m_hVoxel;	// Voxel handle the entity is in.
	int						m_iEntity;	// Entity list index for voxel
};

typedef CUtlFixedLinkedList<LeafListData_t>	CLeafList;

typedef CVarBitVec CPartitionVisits;

//-----------------------------------------------------------------------------
// Used when rendering the various levels of the voxel hash
//-----------------------------------------------------------------------------
static Color s_pVoxelColor[9] = 
{
	Color( 255, 0, 0, 255 ),
	Color( 0, 255, 0, 255 ),
	Color( 0, 0, 255, 255 ),
	Color( 255, 0, 255, 255 ),
	Color( 255, 255, 0, 255 ),
	Color( 0, 255, 255, 255 ),
	Color( 255, 255, 255, 255 ),
	Color( 192, 192, 0, 255 ),
	Color( 128, 128, 128, 255 ),
};


// bounds of the spatial partition
static Vector s_PartitionMin( MIN_COORD_FLOAT, MIN_COORD_FLOAT, MIN_COORD_FLOAT );
static Vector s_PartitionMax( MAX_COORD_FLOAT, MAX_COORD_FLOAT, MAX_COORD_FLOAT );

//-----------------------------------------------------------------------------
// Divide voxel coordinates by 2
//-----------------------------------------------------------------------------
inline Voxel_t ConvertToNextLevel( Voxel_t v )
{
	// Just need to divide by 2 and eliminate the low bits of y and z that shifted
	// into the x and y fields
	Voxel_t res;
	res.uiVoxel = ( ( v.uiVoxel >> SPHASH_LEVEL_SKIP ) & 0xFFCFF9FF );
	return res;
}


//-----------------------------------------------------------------------------
// A single voxel hash
//-----------------------------------------------------------------------------
class CVoxelHash
{
public:
	// Constructor, destructor
	CVoxelHash();
	~CVoxelHash();

	// Call this to clear out the spatial partition and to re-initialize it given a particular world size (ISpatialPartitionInternal)
	void Init( CVoxelTree *pPartition, const Vector& worldmin, const Vector& worldmax, int nLevel );
	void Shutdown();

	// Gets all entities in a particular volume...
	// returns false if the enumerator broke early
	bool EnumerateElementsInBox( SpatialPartitionListMask_t listMask, Voxel_t vmin, Voxel_t vmax, const Vector& mins, const Vector& maxs, IPartitionEnumerator* pIterator );
	bool EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, const Ray_t& ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator* pIterator );
	bool EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask, Voxel_t v, const Vector& pt, IPartitionEnumerator* pIterator );
	
	// Inserts/Removes a handle from the tree.
	void InsertIntoTree( SpatialPartitionHandle_t hPartition, const Vector &vecMin, const Vector &vecMax );
	void RemoveFromTree( SpatialPartitionHandle_t hPartition );

	// Debug!
	void RenderAllObjectsInTree( float flTime );
	void RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime );
	void RenderVoxel( Voxel_t voxel, float flTime );
	void RenderObjectInVoxel( SpatialPartitionHandle_t hPartition, CPartitionVisitor *pVisitor, float flTime );
	void RenderObjectsInVoxel( Voxel_t voxel, CPartitionVisitor *pVisitor, bool bRenderVoxel, float flTime );

	// Computes the voxel count in 1 dimension at a particular level of the tree
	static int ComputeVoxelCountAtLevel( int nLevel );

	// Gets the voxel size for this hash
	int VoxelSize( ) const;

	int EntityCount();

	// Rendering methods
	void RenderGrid();

	// Converts point into voxel index
	inline Voxel_t VoxelIndexFromPoint( const Vector &vecWorldPoint );
	inline void VoxelIndexFromPoint( const Vector &vecWorldPoint, int pPoint[3] );

	// Setup ray for iteration
	void LeafListRaySetup( const Ray_t &ray, const Vector &vecEnd, const Vector &vecInvDelta, Voxel_t voxel, int *pStep, float *pMax, float *pDelta );
	void LeafListExtrudedRaySetup( const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecMin, const Vector &vecMax, int iVoxelMin[3], int iVoxelMax[3], int *pStep, float *pMin, float *pMax, float *pDelta );

	// Main enumeration method
	template <class T> bool EnumerateElementsInVoxel( Voxel_t voxel, const T &intersectTest, SpatialPartitionListMask_t listMask, IPartitionEnumerator* pIterator );

	// Enumeration method when only 1 voxel is ever visited
	template <class T> bool EnumerateElementsInSingleVoxel( Voxel_t voxel, const T &intersectTest, SpatialPartitionListMask_t listMask, IPartitionEnumerator* pIterator );

	bool EnumerateElementsAlongRay_ExtrudedRaySlice( SpatialPartitionListMask_t listMask, IPartitionEnumerator *pIterator, const CIntersectSweptBox &intersectSweptBox,	int voxelMin[3], int voxelMax[3], int iAxis, int *pStep );
private:
	bool EnumerateElementsAlongRay_Ray( SpatialPartitionListMask_t listMask, const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator* pIterator );
	bool EnumerateElementsAlongRay_ExtrudedRay( SpatialPartitionListMask_t listMask, const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator* pIterator );

	inline void PackVoxel( int iX, int iY, int iZ, Voxel_t &voxel );

	typedef CUtlHashFixed<int, SPHASH_BUCKET_COUNT, CUtlHashFixedGenericHash<SPHASH_BUCKET_COUNT> > CHashTable;

	Vector											m_vecVoxelOrigin;	// Voxel space (hash) origin.
	CHashTable										m_aVoxelHash;		// Voxel tree (hash) - data = entity list head handle (m_aEntityList)
	int												m_nVoxelDelta[3];	// Voxel world - width(Dx), height(Dy), depth(Dz)
	CUtlFixedLinkedList<SpatialPartitionHandle_t>	m_aEntityList;	// Pool - Linked list(multilist) of entities per leaf.
	CVoxelTree										*m_pTree;
	int												m_nLevel;
};

class CSpatialPartition;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class CVoxelTree
{
public:
	// constructor, destructor
	CVoxelTree();
	virtual ~CVoxelTree();

	// Inherited from ISpatialPartition
	virtual void Init(  CSpatialPartition *pOwner, int iTree, const Vector& worldmin, const Vector& worldmax );

	virtual void ElementMoved( SpatialPartitionHandle_t handle, const Vector& mins, const Vector& maxs );
	virtual void EnumerateElementsInBox( SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs, bool coarseTest, IPartitionEnumerator* pIterator );
	virtual void EnumerateElementsInSphere( SpatialPartitionListMask_t listMask, const Vector& origin, float radius, bool coarseTest, IPartitionEnumerator* pIterator );
	virtual void EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, const Ray_t& ray, bool coarseTest, IPartitionEnumerator* pIterator );
	virtual void EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask, const Vector& pt, bool coarseTest, IPartitionEnumerator* pIterator );

	virtual void RenderAllObjectsInTree( float flTime );
	virtual void RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime );

	virtual void ReportStats( const char *pFileName );
	virtual void DrawDebugOverlays();

	EntityInfo_t &EntityInfo( SpatialPartitionHandle_t hPartition );
	CLeafList &LeafList();

	int GetTreeId() const;

	CPartitionVisits *BeginVisit();
	CPartitionVisits *GetVisits();
	void EndVisit( CPartitionVisits * );

	// Shut down the allocated memory
	void Shutdown( void );

	// Insert into the appropriate tree
	void InsertIntoTree( SpatialPartitionHandle_t hPartition, const Vector& mins, const Vector& maxs );

	// Remove from appropriate tree
	void RemoveFromTree( SpatialPartitionHandle_t hPartition );

	void LockForWrite()		{ m_lock.LockForWrite(); }
	void UnlockWrite()		{ m_lock.UnlockWrite(); }

	void LockForRead()		{ m_lock.LockForRead(); }
	void UnlockRead()		{ m_lock.UnlockRead(); }

	// Ray casting
	bool EnumerateElementsAlongRay_Ray( SpatialPartitionListMask_t listMask, const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator );
	bool EnumerateElementsAlongRay_ExtrudedRay( SpatialPartitionListMask_t listMask, 
		const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator );

	// Purpose:
	void ComputeSweptRayBounds( const Ray_t &ray, const Vector &vecStartMin, const Vector &vecStartMax, Vector *pVecMin, Vector *pVecMax );

private:

	int									m_nLevelCount;
	CVoxelHash*							m_pVoxelHash;
	CLeafList							m_aLeafList;								// Pool - Linked list(multilist) of leaves per entity.
	int									m_TreeId;
	CThreadLocalPtr<CPartitionVisits>	m_pVisits;
	CSpatialPartition *					m_pOwner;
	CUtlVector<unsigned short>			m_AvailableVisitBits;
	unsigned short						m_nNextVisitBit;
#if TEST_TRACE_POOL
	CTSPool<CPartitionVisits>			m_FreeVisits;
#else
	CObjectPool<CPartitionVisits, 2>	m_FreeVisits;
#endif
	CThreadSpinRWLock					m_lock;
};

//-----------------------------------------------------------------------------
// The spatial partition
//-----------------------------------------------------------------------------

class CSpatialPartition : public ISpatialPartitionInternal
{
public:
	CSpatialPartition();
	~CSpatialPartition();

	enum 
	{ 
		MAX_QUERY_CALLBACK = 3 
	};

	// Inherited from ISpatialPartition
	virtual void Init( const Vector& worldmin, const Vector& worldmax );
	void Shutdown( void );

	virtual SpatialPartitionHandle_t CreateHandle( IHandleEntity *pHandleEntity );
	virtual SpatialPartitionHandle_t CreateHandle( IHandleEntity *pHandleEntity, SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs ); 
	virtual void DestroyHandle( SpatialPartitionHandle_t handle );

	virtual void Insert( SpatialPartitionListMask_t listMask, SpatialPartitionHandle_t handle );
	virtual void Remove( SpatialPartitionListMask_t listMask, SpatialPartitionHandle_t handle );
	virtual void RemoveAndInsert( SpatialPartitionListMask_t removeMask, SpatialPartitionListMask_t insertMask, SpatialPartitionHandle_t handle );
	virtual void Remove( SpatialPartitionHandle_t handle );

	virtual SpatialTempHandle_t HideElement( SpatialPartitionHandle_t handle );
	virtual void UnhideElement( SpatialPartitionHandle_t handle, SpatialTempHandle_t tempHandle );

	virtual void InstallQueryCallback( IPartitionQueryCallback *pCallback );
	virtual void InstallQueryCallback_V1( IPartitionQueryCallback *pCallback );
	virtual void RemoveQueryCallback( IPartitionQueryCallback *pCallback );

	virtual void SuppressLists( SpatialPartitionListMask_t nListMask, bool bSuppress );
	virtual SpatialPartitionListMask_t GetSuppressedLists( void );

	virtual void RenderLeafsForRayTraceStart( float flTime )	{ }
	virtual void RenderLeafsForRayTraceEnd( void )				{ }
	virtual void RenderLeafsForHullTraceStart( float flTime )	{ }
	virtual void RenderLeafsForHullTraceEnd( void )				{ }
	virtual void RenderLeafsForBoxStart( float flTime )			{ }
	virtual void RenderLeafsForBoxEnd( void )					{ }
	virtual void RenderLeafsForSphereStart( float flTime )		{ }
	virtual void RenderLeafsForSphereEnd( void )				{ }

	virtual void RenderObjectsInBox( const Vector &vecMin, const Vector &vecMax, float flTime );
	virtual void RenderObjectsInSphere( const Vector &vecCenter, float flRadius, float flTime );
	virtual void RenderObjectsAlongRay( const Ray_t& ray, float flTime );

	virtual void ElementMoved( SpatialPartitionHandle_t handle, const Vector& mins, const Vector& maxs );
	virtual void EnumerateElementsInBox( SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs, bool coarseTest, IPartitionEnumerator* pIterator );
	virtual void EnumerateElementsInSphere( SpatialPartitionListMask_t listMask, const Vector& origin, float radius, bool coarseTest, IPartitionEnumerator* pIterator );
	virtual void EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, const Ray_t& ray, bool coarseTest, IPartitionEnumerator* pIterator );
	virtual void EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask, const Vector& pt, bool coarseTest, IPartitionEnumerator* pIterator );

	virtual void RenderAllObjectsInTree( float flTime );
	virtual void RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime );
	virtual void ReportStats( const char *pFileName );
	virtual void DrawDebugOverlays();

	// Gets entity info (for enumerations).
	EntityInfo_t &EntityInfo( SpatialPartitionHandle_t hPartition );

	virtual void InsertIntoTree( SpatialPartitionHandle_t hPartition, const Vector& mins, const Vector& maxs );
	virtual void RemoveFromTree( SpatialPartitionHandle_t hPartition );

	CVoxelTree * VoxelTree( SpatialPartitionListMask_t listMask );
	CVoxelTree * VoxelTreeForHandle( SpatialPartitionHandle_t handle );

protected:
	// Invokes the pre-query callbacks.
	void InvokeQueryCallbacks( SpatialPartitionListMask_t listMask, bool = false );

	typedef CUtlLinkedList<EntityInfo_t, SpatialPartitionHandle_t, false, SpatialPartitionHandle_t, CUtlMemoryStack<UtlLinkedListElem_t< EntityInfo_t, SpatialPartitionHandle_t >, SpatialPartitionHandle_t, 0xffff, 1024> > CHandleList;

private:
	CHandleList												m_aHandles;  								// Stores all unique elements (1 per entity in tree).
	CThreadFastMutex										m_HandlesMutex;

	CVoxelTree												m_VoxelTrees[NUM_TREES];

	IPartitionQueryCallback									*m_pQueryCallback[MAX_QUERY_CALLBACK];		// Query callbacks.
	bool													m_bUseOldQueryCallback[MAX_QUERY_CALLBACK];
	int														m_nQueryCallbackCount;						// Number of query callbacks.

	// Debug!
	SpatialPartitionListMask_t								m_nSuppressedListMask;
};

//-----------------------------------------------------------------------------
// Spatial partition inline methods
//-----------------------------------------------------------------------------
// Gets entity info (for enumerations).
inline EntityInfo_t &CSpatialPartition::EntityInfo( SpatialPartitionHandle_t hPartition )		
{ 
	return m_aHandles[hPartition]; 
}

inline EntityInfo_t &CVoxelTree::EntityInfo( SpatialPartitionHandle_t hPartition )
{
	return m_pOwner->EntityInfo( hPartition );
}

inline CLeafList &CVoxelTree::LeafList() 
{ 
	return m_aLeafList; 
}

inline int CVoxelTree::GetTreeId() const
{
	return m_TreeId;
}

inline CPartitionVisits *CVoxelTree::GetVisits()
{
	return m_pVisits;
}

inline CPartitionVisits *CVoxelTree::BeginVisit()
{
	CPartitionVisits *pPrev = m_pVisits;
	CPartitionVisits *pVisits = m_FreeVisits.GetObject();
	pVisits->Resize( m_nNextVisitBit, true );
	m_pVisits = pVisits;
	return pPrev;
}

inline void CVoxelTree::EndVisit( CPartitionVisits *pPrev )
{
	m_FreeVisits.PutObject( m_pVisits );
	m_pVisits = pPrev;
}

inline CVoxelTree *CSpatialPartition::VoxelTree( SpatialPartitionListMask_t listMask )
{
	int iTree = ( ( listMask & PARTITION_ALL_CLIENT_EDICTS ) == 0 ) ? SERVER_TREE : CLIENT_TREE;
	return &m_VoxelTrees[iTree];
}

inline CVoxelTree *CSpatialPartition::VoxelTreeForHandle( SpatialPartitionHandle_t handle )
{
	return VoxelTree( m_aHandles[handle].m_fList );
}


	
//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CVoxelHash::CVoxelHash( )
{
}

CVoxelHash::~CVoxelHash()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Create a voxel index from a world point - is the hash key.
//   Input: vecWorldPoint - world point to get voxel index for
//  Output: voxel index
//-----------------------------------------------------------------------------
inline void CVoxelHash::VoxelIndexFromPoint( const Vector &vecWorldPoint, int pPoint[3] )
{
	pPoint[0] = static_cast<int>( vecWorldPoint.x - m_vecVoxelOrigin.x ) >> ( SPHASH_VOXEL_SHIFT + SPHASH_LEVEL_SKIP * m_nLevel );
	pPoint[1] = static_cast<int>( vecWorldPoint.y - m_vecVoxelOrigin.y ) >> ( SPHASH_VOXEL_SHIFT + SPHASH_LEVEL_SKIP * m_nLevel );
	pPoint[2] = static_cast<int>( vecWorldPoint.z - m_vecVoxelOrigin.z ) >> ( SPHASH_VOXEL_SHIFT + SPHASH_LEVEL_SKIP * m_nLevel );
}

inline void CVoxelHash::PackVoxel( int iX, int iY, int iZ, Voxel_t &voxel )
{
	Assert( ( iX >= -( 1 << 10 ) ) && ( iX <= ( 1 << 10 ) ) );
	Assert( ( iY >= -( 1 << 10 ) ) && ( iY <= ( 1 << 10 ) ) );
	Assert( ( iZ >= -( 1 << 9 ) ) && ( iZ <= ( 1 << 9 ) ) );
	voxel.bitsVoxel.x = iX;
	voxel.bitsVoxel.y = iY;
	voxel.bitsVoxel.z = iZ;
}

inline Voxel_t CVoxelHash::VoxelIndexFromPoint( const Vector &vecWorldPoint )
{
	Voxel_t voxel;

	voxel.bitsVoxel.x = static_cast<int>( vecWorldPoint.x - m_vecVoxelOrigin.x ) >> ( SPHASH_VOXEL_SHIFT + SPHASH_LEVEL_SKIP * m_nLevel );
	voxel.bitsVoxel.y = static_cast<int>( vecWorldPoint.y - m_vecVoxelOrigin.y ) >> ( SPHASH_VOXEL_SHIFT + SPHASH_LEVEL_SKIP * m_nLevel );
	voxel.bitsVoxel.z = static_cast<int>( vecWorldPoint.z - m_vecVoxelOrigin.z ) >> ( SPHASH_VOXEL_SHIFT + SPHASH_LEVEL_SKIP * m_nLevel );

	return voxel;
}


//-----------------------------------------------------------------------------
// Purpose:	Computes the voxel count at a particular level of the tree
//-----------------------------------------------------------------------------
int CVoxelHash::ComputeVoxelCountAtLevel( int nLevel )
{
	int nVoxelCount = COORD_EXTENT >> SPHASH_VOXEL_SHIFT;
	nVoxelCount >>= ( SPHASH_LEVEL_SKIP * nLevel ); 
	return ( nVoxelCount > 0 ) ? nVoxelCount : 1;
}


//-----------------------------------------------------------------------------
// Gets the voxel size for this hash
//-----------------------------------------------------------------------------
inline int CVoxelHash::VoxelSize( ) const
{
	return SPHASH_VOXEL_SIZE << ( SPHASH_LEVEL_SKIP * m_nLevel );
}


//-----------------------------------------------------------------------------
// Purpose:
//   Input: worldmin - 
//          worldmax - 
//-----------------------------------------------------------------------------
void CVoxelHash::Init( CVoxelTree *pPartition, const Vector &worldmin, const Vector &worldmax, int nLevel )
{
	m_pTree = pPartition;
	m_nLevel = nLevel;

	// Setup the hash.
	MEM_ALLOC_CREDIT();

	int nVoxelCount = ComputeVoxelCountAtLevel( nLevel );
	m_vecVoxelOrigin.Init( MIN_COORD_FLOAT, MIN_COORD_FLOAT, MIN_COORD_FLOAT );
	int nHashBucketCount = SPHASH_BUCKET_COUNT >> nLevel;
	if ( nHashBucketCount < 16 )
	{
		nHashBucketCount = 16;
	}
	m_nVoxelDelta[0] = nVoxelCount;
	m_nVoxelDelta[1] = nVoxelCount;
	m_nVoxelDelta[2] = nVoxelCount;
	Assert( ( m_nVoxelDelta[0] >= 0 ) && ( m_nVoxelDelta[0] <= ( 1 << 10 ) ) );
	Assert( ( m_nVoxelDelta[1] >= 0 ) && ( m_nVoxelDelta[1] <= ( 1 << 10 ) ) );
	Assert( ( m_nVoxelDelta[2] >= 0 ) && ( m_nVoxelDelta[2] <= ( 1 << 9 ) ) );

	m_aVoxelHash.RemoveAll();

	// Setup the entity list pool.
	int nGrowSize = SPHASH_ENTITYLIST_BLOCK >> nLevel;
	if ( nGrowSize < 16 )
	{
		nGrowSize = 16;
	}
	m_aEntityList.Purge();
	m_aEntityList.SetGrowSize( nGrowSize );
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CVoxelHash::Shutdown( void )
{
	m_aEntityList.Purge();
	m_aVoxelHash.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: Insert the object into the voxel hash.
//-----------------------------------------------------------------------------
void CVoxelHash::InsertIntoTree( SpatialPartitionHandle_t hPartition, const Vector &vecMin, const Vector &vecMax )
{
	EntityInfo_t &info = m_pTree->EntityInfo( hPartition );
	CLeafList &leafList = m_pTree->LeafList();
	int treeId = m_pTree->GetTreeId();

	// Set the entity bounding box.
	info.m_vecMin = vecMin;
	info.m_vecMax = vecMax;

	// Set the voxel level
	info.m_nLevel[m_pTree->GetTreeId()] = m_nLevel;

	// Add the object to the tree.
	Voxel_t voxelMin, voxelMax;
	voxelMin = VoxelIndexFromPoint( vecMin );
	voxelMax = VoxelIndexFromPoint( vecMax );
	Assert( (m_nLevel == 4) ||
			( (voxelMax.bitsVoxel.x - voxelMin.bitsVoxel.x <= 1) && 
			(voxelMax.bitsVoxel.y - voxelMin.bitsVoxel.y <= 1) && 
			(voxelMax.bitsVoxel.z - voxelMin.bitsVoxel.z <= 1) ) );

	// Add the object to all the voxels it intersects.
	Voxel_t voxel;
	unsigned int iX, iY, iZ;
	for ( iX = voxelMin.bitsVoxel.x; iX <= voxelMax.bitsVoxel.x; ++iX )
	{
		voxel.bitsVoxel.x = iX;
		for ( iY = voxelMin.bitsVoxel.y; iY <= voxelMax.bitsVoxel.y; ++iY )
		{
			voxel.bitsVoxel.y = iY;
			for ( iZ = voxelMin.bitsVoxel.z; iZ <= voxelMax.bitsVoxel.z; ++iZ )
			{
				voxel.bitsVoxel.z = iZ;

#if 0
				// Debug!
				RenderVoxel( voxel );
#endif

				// Entity list.
				int iEntity = m_aEntityList.Alloc( true );
				m_aEntityList[iEntity] = hPartition;
				
				UtlHashFastHandle_t hHash = m_aVoxelHash.Find( voxel.uiVoxel );
				if ( hHash == m_aVoxelHash.InvalidHandle() )
				{
					// Add voxel(leaf) to hash.
					hHash = m_aVoxelHash.FastInsert( voxel.uiVoxel, iEntity );
				}
				else
				{
					int iHead = m_aVoxelHash.Element( hHash );
					m_aEntityList.LinkBefore( iHead, iEntity );
					m_aVoxelHash[hHash] = iEntity;
				}
				
				// Leaf list.
				int iLeafList = leafList.Alloc( true );
				leafList[iLeafList].m_hVoxel = hHash;
				leafList[iLeafList].m_iEntity = iEntity;
				
				if ( info.m_iLeafList[treeId] == leafList.InvalidIndex() )
				{
					info.m_iLeafList[treeId] = iLeafList;
				}
				else
				{
					int iHead = info.m_iLeafList[treeId];
					leafList.LinkBefore( iHead, iLeafList );
					info.m_iLeafList[treeId] = iLeafList;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes the object into the voxel hash.
//-----------------------------------------------------------------------------
void CVoxelHash::RemoveFromTree( SpatialPartitionHandle_t hPartition )
{
	EntityInfo_t &data = m_pTree->EntityInfo( hPartition );
	CLeafList &leafList = m_pTree->LeafList();
	int treeId = m_pTree->GetTreeId();

	int iLeaf = data.m_iLeafList[treeId];
	int iNext;
	while ( iLeaf != leafList.InvalidIndex() )
	{
		// Get the next voxel - if any.
		iNext = leafList.Next( iLeaf );

		UtlHashFastHandle_t hHash = leafList[iLeaf].m_hVoxel;
		if ( hHash == m_aVoxelHash.InvalidHandle() )
		{
			iLeaf = iNext;
			continue;
		}

		// Get the head of the entity list for the voxel.
		int iEntity = leafList[iLeaf].m_iEntity;
		int iEntityHead = m_aVoxelHash[hHash];

		if ( iEntityHead == iEntity )
		{
			int iEntityNext = m_aEntityList.Next( iEntityHead );
			if ( iEntityNext == m_aEntityList.InvalidIndex() )
			{
				m_aVoxelHash.Remove( hHash );
			}
			else
			{
				m_aVoxelHash[hHash] = iEntityNext;
			}
		}
		
		// Remove the entity from the entity list for the voxel.
		m_aEntityList.Remove( iEntity );

		// Remove from the leaf list.
		leafList.Remove( iLeaf );		
		iLeaf = iNext;
	}
	
	data.m_iLeafList[treeId] = leafList.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline void ClampStartPoint( Ray_t &ray, const Vector &vecEnd )
{
	float flDistStart, flT;
	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		if ( fabs(ray.m_Delta[iAxis]) < 1e-10 )
			continue;

		if ( ray.m_Delta[iAxis] > 0.0f )
		{
			if ( ray.m_Start[iAxis] < MIN_COORD_FLOAT )
			{
				// Add some bloat inward.
				flDistStart = ( MIN_COORD_FLOAT + 5.0f ) - ray.m_Start[iAxis];
				flT = flDistStart / ray.m_Delta[iAxis];
				VectorMA( ray.m_Start, flT, ray.m_Delta, ray.m_Start );
			}
		}
		else
		{
			if ( ray.m_Start[iAxis] > MAX_COORD_FLOAT )
			{
				// Add some bloat inward.
				flDistStart = ray.m_Start[iAxis] - ( MAX_COORD_FLOAT - 5.0f );
				flT = flDistStart / -ray.m_Delta[iAxis];
				VectorMA( ray.m_Start, flT, ray.m_Delta, ray.m_Start );
			}
		}
	}

	VectorSubtract( vecEnd, ray.m_Start, ray.m_Delta );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline void ClampEndPoint( Ray_t &ray, Vector &vecEnd )
{
	float flDistStart, flT;
	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		if ( fabs(ray.m_Delta[iAxis]) < 1e-10 )
			continue;

		if ( ray.m_Delta[iAxis] < 0.0f )
		{
			if ( vecEnd[iAxis] < MIN_COORD_FLOAT )
			{
				// Add some bloat inward.
				flDistStart = ray.m_Start[iAxis] - ( MIN_COORD_FLOAT + 5.0f );
				flT = flDistStart / -ray.m_Delta[iAxis];
				VectorMA( ray.m_Start, flT, ray.m_Delta, vecEnd );
			}
		}
		else
		{
			if ( vecEnd[iAxis] > MAX_COORD_FLOAT )
			{
				// Add some bloat inward.
				flDistStart = ray.m_Start[iAxis] - ( -MAX_COORD_FLOAT + 5.0f );
				flT = flDistStart / ray.m_Delta[iAxis];
				VectorMA( ray.m_Start, flT, ray.m_Delta, vecEnd );
			}
		}
	}

	VectorSubtract( vecEnd, ray.m_Start, ray.m_Delta );
}


//-----------------------------------------------------------------------------
// Intersection classes
//-----------------------------------------------------------------------------
class CPartitionVisitor
{
public:
	CPartitionVisitor( CVoxelTree *pPartition )
	{
		m_pVisits = pPartition->GetVisits();
		m_iTree = pPartition->GetTreeId();
	}

	~CPartitionVisitor()
	{
	}

	bool Visit( SpatialPartitionHandle_t hPartition, EntityInfo_t &hInfo ) const
	{
		int nVisitBit = hInfo.m_nVisitBit[m_iTree];
		if ( m_pVisits->IsBitSet( nVisitBit ) )
		{
			return false;
		}

		m_pVisits->Set( nVisitBit );
		return true;
	}

private:
	CPartitionVisits *m_pVisits;
	int m_iTree;
};


class CIntersectPoint : public CPartitionVisitor
{
public:
	CIntersectPoint( CVoxelTree *pPartition, const Vector &pt ) : CPartitionVisitor( pPartition ), m_vecPoint( pt )
	{
	}

	bool Intersects( const Vector &vecMins, const Vector &vecMaxs ) const
	{
		// Ray intersection test
		Assert( vecMins.x <= vecMaxs.x );
		Assert( vecMins.y <= vecMaxs.y );
		Assert( vecMins.z <= vecMaxs.z );

		// Does the ray intersect the box?
		return IsPointInBox( m_vecPoint, vecMins, vecMaxs );
	}
	 
private:
	const Vector &m_vecPoint;
};


class CIntersectBox : public CPartitionVisitor
{
public:
	CIntersectBox( CVoxelTree *pPartition, const Vector &vecMins, const Vector &vecMaxs ) : CPartitionVisitor( pPartition ), m_vecMins( vecMins ), m_vecMaxs( vecMaxs )
	{
	}

	bool Intersects( const Vector &vecMins, const Vector &vecMaxs ) const
	{
		// Box intersection test
		Assert( vecMins.x <= vecMaxs.x );
		Assert( vecMins.y <= vecMaxs.y );
		Assert( vecMins.z <= vecMaxs.z );

		return ( vecMins.x <= m_vecMaxs.x ) && ( vecMaxs.x >= m_vecMins.x ) &&
				( vecMins.y <= m_vecMaxs.y ) && ( vecMaxs.y >= m_vecMins.y ) &&
				( vecMins.z <= m_vecMaxs.z ) && ( vecMaxs.z >= m_vecMins.z );
	}
	 
private:
	const Vector &m_vecMins;
	const Vector &m_vecMaxs;
};

class CIntersectRay : public CPartitionVisitor
{
public:
	CIntersectRay( CVoxelTree *pPartition, const Ray_t &ray, const Vector &vecInvDelta ) : CPartitionVisitor( pPartition ), m_Ray( ray ), m_vecInvDelta( vecInvDelta )
	{
	}

	bool Intersects( const Vector &vecMins, const Vector &vecMaxs ) const
	{
		// Ray intersection test
		Assert( vecMins.x <= vecMaxs.x );
		Assert( vecMins.y <= vecMaxs.y );
		Assert( vecMins.z <= vecMaxs.z );

		// Does the ray intersect the box?
		return IsBoxIntersectingRay( vecMins, vecMaxs, m_Ray.m_Start, m_Ray.m_Delta, m_vecInvDelta );
	}
	 
private:
	const Ray_t &m_Ray;
	const Vector &m_vecInvDelta;
};


class CIntersectSweptBox : public CPartitionVisitor
{
public:
	CIntersectSweptBox( CVoxelTree *pPartition, const Ray_t &ray, const Vector &vecInvDelta ) : CPartitionVisitor( pPartition ), m_Ray( ray ), m_vecInvDelta( vecInvDelta )
	{
	}

	bool Intersects( const Vector &vecMins, const Vector &vecMaxs ) const
	{
		// Swept box intersection test
		Assert( vecMins.x <= vecMaxs.x );
		Assert( vecMins.y <= vecMaxs.y );
		Assert( vecMins.z <= vecMaxs.z );

		Vector vecTestMin, vecTestMax;
		VectorSubtract( vecMins, m_Ray.m_Extents, vecTestMin );
		VectorAdd( vecMaxs, m_Ray.m_Extents, vecTestMax );

		// Does the ray intersect the box?
		return IsBoxIntersectingRay( vecTestMin, vecTestMax, m_Ray.m_Start, m_Ray.m_Delta, m_vecInvDelta );
	}
	 
private:
	const Ray_t &m_Ray;
	const Vector &m_vecInvDelta;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class T> 
bool CVoxelHash::EnumerateElementsInVoxel( Voxel_t voxel, const T &intersectTest, SpatialPartitionListMask_t listMask, IPartitionEnumerator* pIterator )
{
	// If the voxel doesn't exist, nothing to iterate over
	UtlHashFastHandle_t hHash = m_aVoxelHash.Find( voxel.uiVoxel );
	if ( hHash == m_aVoxelHash.InvalidHandle() )
		return true;

	SpatialPartitionHandle_t hPartition;
	for ( int i = m_aVoxelHash.Element( hHash ); i != m_aEntityList.InvalidIndex(); i = m_aEntityList.Next(i) )
	{
		hPartition = m_aEntityList[i];
		if ( hPartition == PARTITION_INVALID_HANDLE )
			continue;

		EntityInfo_t &hInfo = m_pTree->EntityInfo( hPartition );

		// Keep going if this dude isn't in the list
		if ( !( listMask & hInfo.m_fList ) )
			continue;

		if ( hInfo.m_flags & ENTITY_HIDDEN )
			continue;

		// Has this handle already been visited?
		if ( !intersectTest.Visit( hPartition, hInfo ) )
			continue;

		// Intersection test
		if ( !intersectTest.Intersects( hInfo.m_vecMin, hInfo.m_vecMax ) )
			continue;

		// Okay, this one is good...
		if ( pIterator->EnumElement( hInfo.m_pHandleEntity ) == ITERATION_STOP )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Enumeration method when only 1 voxel is ever visited
//-----------------------------------------------------------------------------
template <class T> 
bool CVoxelHash::EnumerateElementsInSingleVoxel( Voxel_t voxel, const T &intersectTest, 
	SpatialPartitionListMask_t listMask, IPartitionEnumerator* pIterator )
{
	// NOTE: We don't have to do the enum id checking, nor do we have to up the
	// nesting level, since this only visits 1 voxel.
	int iEntityList;
	UtlHashFastHandle_t hHash = m_aVoxelHash.Find( voxel.uiVoxel );
	if ( hHash != m_aVoxelHash.InvalidHandle() )
	{
		iEntityList = m_aVoxelHash.Element( hHash );
		while ( iEntityList != m_aEntityList.InvalidIndex() )
		{
			SpatialPartitionHandle_t hPartition = m_aEntityList[iEntityList];
			iEntityList = m_aEntityList.Next( iEntityList );
			if ( hPartition == PARTITION_INVALID_HANDLE )
				continue;

			EntityInfo_t &hInfo = m_pTree->EntityInfo( hPartition );

			// Keep going if this dude isn't in the list
			if ( !( listMask & hInfo.m_fList ) )
				continue;

			if ( hInfo.m_flags & ENTITY_HIDDEN )
				continue;

			// Keep going if there's no collision
			if ( !intersectTest.Intersects( hInfo.m_vecMin, hInfo.m_vecMax ) )
				continue;

			// Okay, this one is good...
			if ( pIterator->EnumElement( hInfo.m_pHandleEntity ) == ITERATION_STOP )
				return false;
		}
	}
	return true;
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelHash::EnumerateElementsInBox( SpatialPartitionListMask_t listMask, 
	Voxel_t vmin, Voxel_t vmax, const Vector& mins, const Vector& maxs, IPartitionEnumerator* pIterator )
{
	VPROF( "BoxTest/SphereTest" );

	Assert( mins.x <= maxs.x );
	Assert( mins.y <= maxs.y );
	Assert( mins.z <= maxs.z );
	
	// Create the intersection object
	bool bSingleVoxel = ( vmin.uiVoxel == vmax.uiVoxel );
	CIntersectBox rect( m_pTree, mins, maxs );

	// In the same voxel
	if ( bSingleVoxel )
		return EnumerateElementsInSingleVoxel( vmin, rect, listMask, pIterator );

	// Iterate over all voxels
	Voxel_t vdelta;
	vdelta.uiVoxel = vmax.uiVoxel - vmin.uiVoxel;
	int cx = vdelta.bitsVoxel.x;
	int cy = vdelta.bitsVoxel.y;
	int cz = vdelta.bitsVoxel.z;

	Voxel_t voxel;
	voxel.bitsVoxel.x = vmin.bitsVoxel.x;
	for ( int iX = 0; iX <= cx; ++iX, ++voxel.bitsVoxel.x )
	{
		voxel.bitsVoxel.y = vmin.bitsVoxel.y;
		for ( int iY = 0; iY <= cy; ++iY, ++voxel.bitsVoxel.y )
		{
			voxel.bitsVoxel.z = vmin.bitsVoxel.z;
			for ( int iZ = 0; iZ <= cz; ++iZ, ++voxel.bitsVoxel.z )
			{
				if ( !EnumerateElementsInVoxel( voxel, rect, listMask, pIterator ) )
					return false;
			}
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelHash::EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, 
	const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator )
{
	Assert( ray.m_IsSwept );

	// Two different methods
	if ( ray.m_IsRay )
		return EnumerateElementsAlongRay_Ray( listMask, ray, vecInvDelta, vecEnd, pIterator );
	return EnumerateElementsAlongRay_ExtrudedRay( listMask, ray, vecInvDelta, vecEnd, pIterator );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelHash::EnumerateElementsAlongRay_Ray( SpatialPartitionListMask_t listMask, 
	const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator )
{
#ifdef _DEBUG
	// For drawing debug objects.
	static int nRenderRayEnumId = 4;
#endif
	
	// Find the voxel start + end
	Voxel_t voxelStart = VoxelIndexFromPoint( ray.m_Start );
	Voxel_t voxelEnd = VoxelIndexFromPoint( vecEnd );

	bool bSingleVoxel = ( voxelStart.uiVoxel == voxelEnd.uiVoxel );
	CIntersectRay intersectRay( m_pTree, ray, vecInvDelta );

	// Optimization: Look for single voxel rays
	if ( bSingleVoxel )
		return EnumerateElementsInSingleVoxel( voxelStart, intersectRay, listMask, pIterator );

	Voxel_t voxelCurrent;
	voxelCurrent.uiVoxel = voxelStart.uiVoxel;
	
	// Setup.
	int nStep[3];
	float tMax[3];
	float tDelta[3];
	LeafListRaySetup( ray, vecEnd, vecInvDelta, voxelStart, nStep, tMax, tDelta );

	// Walk the voxels and visit all elements in each voxel
	while ( 1 )
	{
		if ( !EnumerateElementsInVoxel( voxelCurrent, intersectRay, listMask, pIterator ) )
			return false;

		if ( tMax[0] >= 1.0f && tMax[1] >= 1.0f && tMax[2] >= 1.0f )
			break;
		
		if ( tMax[0] < tMax[1] )
		{
			if ( tMax[0] < tMax[2] )
			{
				voxelCurrent.bitsVoxel.x += nStep[0];
				tMax[0] += tDelta[0];
			}
			else
			{
				voxelCurrent.bitsVoxel.z += nStep[2];
				tMax[2] += tDelta[2];
			}
		}
		else
		{
			if ( tMax[1] < tMax[2] )
			{
				voxelCurrent.bitsVoxel.y += nStep[1];
				tMax[1] += tDelta[1];
			}
			else
			{
				voxelCurrent.bitsVoxel.z += nStep[2];
				tMax[2] += tDelta[2];
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelHash::LeafListRaySetup( const Ray_t &ray, const Vector &vecEnd, 
	const Vector &vecInvDelta, Voxel_t voxel, int *pStep, float *pMax, float *pDelta )
{
	int iVoxel[3];
	iVoxel[0] = voxel.bitsVoxel.x;
	iVoxel[1] = voxel.bitsVoxel.y;
	iVoxel[2] = voxel.bitsVoxel.z;

	// Setup.
	float flDist, flDistStart, flDistEnd, flRecipDist;
	Vector vecVoxelStart, vecVoxelEnd;
	VectorSubtract( ray.m_Start, m_vecVoxelOrigin, vecVoxelStart );
	VectorSubtract( vecEnd, m_vecVoxelOrigin, vecVoxelEnd );

	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		if ( vecVoxelStart[iAxis] == vecVoxelEnd[iAxis] )
		{
			pStep[iAxis] = 0;
			pMax[iAxis] = SPHASH_VOXEL_LARGE;
			pDelta[iAxis] = SPHASH_VOXEL_LARGE;
			continue;
		}

		if ( ray.m_Delta[iAxis] < 0.0f )
		{
			pStep[iAxis] = -1;
			flDist = ( iVoxel[iAxis] ) * VoxelSize();
			flDistStart = vecVoxelStart[iAxis] - flDist;
			flDistEnd = vecVoxelEnd[iAxis] - flDist;
			flRecipDist = -vecInvDelta[iAxis];
		}
		else
		{
			pStep[iAxis] = 1;
			flDist = ( iVoxel[iAxis] + 1 ) * -VoxelSize();
			flDistStart = -vecVoxelStart[iAxis] - flDist;
			flDistEnd = -vecVoxelEnd[iAxis] - flDist;
			flRecipDist = vecInvDelta[iAxis];
		}

		if ( ( flDistStart > 0.0f ) && ( flDistEnd > 0.0f ) )
		{
			pMax[iAxis] = SPHASH_VOXEL_LARGE;
			pDelta[iAxis] = SPHASH_VOXEL_LARGE;
		}
		else
		{
			pMax[iAxis] = flDistStart * flRecipDist;
			pDelta[iAxis] = VoxelSize() * flRecipDist;
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the min index of 3 numbers
//-----------------------------------------------------------------------------
inline int MinIndex( float v1, float v2, float v3 )
{
	if ( v1 < v2 )
		return ( v1 < v3 ) ? 0 : 2;
	return ( v2 < v3 ) ? 1 : 2;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelHash::EnumerateElementsAlongRay_ExtrudedRay( SpatialPartitionListMask_t listMask, 
	const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator )
{
	// Check the starting position, then proceed with the sweep.
	Vector vecMin, vecMax;
	VectorSubtract( ray.m_Start, ray.m_Extents, vecMin );
	VectorAdd( ray.m_Start, ray.m_Extents, vecMax );

	// Visit each voxel in the box and enumerate its elements.
	int voxelMin[3], voxelMax[3];
	VoxelIndexFromPoint( vecMin, voxelMin );
	VoxelIndexFromPoint( vecMax, voxelMax );

	CIntersectSweptBox intersectSweptBox( m_pTree, ray, vecInvDelta );

	// Iterate over all voxels that intersect the box around the starting ray point
	Voxel_t voxel;
	int iX, iY, iZ;
	for ( iX = voxelMin[0]; iX <= voxelMax[0]; ++iX )
	{
		voxel.bitsVoxel.x = iX;
		for ( iY = voxelMin[1]; iY <= voxelMax[1]; ++iY )
		{
			voxel.bitsVoxel.y = iY;
			for ( iZ = voxelMin[2]; iZ <= voxelMax[2]; ++iZ )
			{
				voxel.bitsVoxel.z = iZ;
				if ( !EnumerateElementsInVoxel( voxel, intersectSweptBox, listMask, pIterator ) )
					return false;
			}
		}
	}

	// Early out: Check to see if the range of voxels at the endpoint
	// is the same as the range at the start point. If so, we're done.
	Vector vecEndMin, vecEndMax;
	VectorSubtract( vecEnd, ray.m_Extents, vecEndMin );
	VectorAdd( vecEnd, ray.m_Extents, vecEndMax );

	int endVoxelMin[3], endVoxelMax[3];
	VoxelIndexFromPoint( vecEndMin, endVoxelMin );
	VoxelIndexFromPoint( vecEndMax, endVoxelMax );
	if ( (endVoxelMin[0] >= voxelMin[0]) && (endVoxelMin[1] >= voxelMin[1]) && (endVoxelMin[2] >= voxelMin[2]) &&
		 (endVoxelMax[0] <= voxelMax[0]) && (endVoxelMax[1] <= voxelMax[1]) && (endVoxelMax[2] <= voxelMax[2]) )
		 return true;
	 
	// Setup.
	int nStep[3];
	float tMax[3];	// amount of change in t along ray until we hit the next new voxel
	float tMin[3];	// amount of change in t along ray until we leave the last voxel
	float tDelta[3];
	LeafListExtrudedRaySetup( ray, vecInvDelta, vecMin, vecMax, voxelMin, voxelMax, nStep, tMin, tMax, tDelta );

	// Walk the voxels and create the leaf list.
	int iAxis, iMinAxis;
	while ( tMax[0] < 1.0f || tMax[1] < 1.0f || tMax[2] < 1.0f )
	{
		iAxis = MinIndex( tMax[0], tMax[1], tMax[2] );
		iMinAxis = MinIndex( tMin[0], tMin[1], tMin[2] );

		if ( tMin[iMinAxis] < tMax[iAxis] )
		{
			tMin[iMinAxis] += tDelta[iMinAxis];
			if ( nStep[iMinAxis] > 0 )
			{
				voxelMin[iMinAxis] += nStep[iMinAxis];
			}
			else
			{
				voxelMax[iMinAxis] += nStep[iMinAxis];
			}
		}
		else
		{
			tMax[iAxis] += tDelta[iAxis];
			if ( nStep[iAxis] > 0 )
			{
				voxelMax[iAxis] += nStep[iAxis];
			}
			else
			{
				voxelMin[iAxis] += nStep[iAxis];
			}
			
			if ( !EnumerateElementsAlongRay_ExtrudedRaySlice( listMask, pIterator, intersectSweptBox, voxelMin, voxelMax, iAxis, nStep ) )
				return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelHash::LeafListExtrudedRaySetup( const Ray_t &ray, const Vector &vecInvDelta,
											const Vector &vecMin, const Vector &vecMax,
											int iVoxelMin[3], int iVoxelMax[3],													 
											int *pStep, float *pMin, float *pMax, float *pDelta )
{
	float flDist, flDistStart, flRecipDist, flDistMinStart;

	Vector vecVoxelMin, vecVoxelMax;
	VectorSubtract( vecMin, m_vecVoxelOrigin, vecVoxelMin );
	VectorSubtract( vecMax, m_vecVoxelOrigin, vecVoxelMax );

	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		if ( ray.m_Delta[iAxis] == 0.0f )
		{
			pMax[iAxis] = SPHASH_VOXEL_LARGE;
			pMin[iAxis] = SPHASH_VOXEL_LARGE;
			pDelta[iAxis] = SPHASH_VOXEL_LARGE;
			continue;
		}

		if ( ray.m_Delta[iAxis] < 0.0f )
		{
			pStep[iAxis] = -1;
			flDistStart = vecVoxelMin[iAxis] - ( ( iVoxelMin[iAxis] ) * VoxelSize() );
			flDistMinStart = vecVoxelMax[iAxis] - ( ( iVoxelMax[iAxis] ) * VoxelSize() );
			flDist = -ray.m_Delta[iAxis];
			flRecipDist = -vecInvDelta[iAxis];
		}
		else
		{
			pStep[iAxis] = 1;
			flDistStart = -vecVoxelMax[iAxis] - ( ( iVoxelMax[iAxis] + 1 ) * -VoxelSize() );
			flDistMinStart = -vecVoxelMin[iAxis] - ( ( iVoxelMin[iAxis] + 1 ) * -VoxelSize() );
			flDist = ray.m_Delta[iAxis];
			flRecipDist = vecInvDelta[iAxis];
		}

		if ( flDistStart > flDist )
		{
			pMax[iAxis] = SPHASH_VOXEL_LARGE;
			pDelta[iAxis] = SPHASH_VOXEL_LARGE;
		}
		else
		{
			pMax[iAxis] = flDistStart * flRecipDist;
			pDelta[iAxis] = VoxelSize() * flRecipDist;
		}

		if ( flDistMinStart > flDist )
		{
			pMin[iAxis] = SPHASH_VOXEL_LARGE;
		}
		else
		{
			pMin[iAxis] = flDistMinStart * flRecipDist;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelHash::EnumerateElementsAlongRay_ExtrudedRaySlice( SpatialPartitionListMask_t listMask,
	IPartitionEnumerator *pIterator, const CIntersectSweptBox &intersectSweptBox,
	int voxelMin[3], int voxelMax[3], int iAxis, int *pStep )
{
	int mins[3] = { voxelMin[0], voxelMin[1], voxelMin[2] };
	int maxs[3] = { voxelMax[0], voxelMax[1], voxelMax[2] };
	if ( pStep[iAxis] < 0.0f )
	{
		maxs[iAxis] = mins[iAxis];
	}
	else
	{
		mins[iAxis] = maxs[iAxis];
	}

	// Create leaf cache.
	Voxel_t voxel;
	int iX, iY, iZ;
	for ( iX = mins[0]; iX <= maxs[0]; ++iX )
	{
		voxel.bitsVoxel.x = iX;
		for ( iY = mins[1]; iY <= maxs[1]; ++iY )
		{
			voxel.bitsVoxel.y = iY;
			for ( iZ = mins[2]; iZ <= maxs[2]; ++iZ )
			{
				voxel.bitsVoxel.z = iZ;
				if ( !EnumerateElementsInVoxel( voxel, intersectSweptBox, listMask, pIterator ) )
					return false;
			}
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelHash::EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask,
	Voxel_t v, const Vector& pt, IPartitionEnumerator* pIterator )
{
	// NOTE: We don't have to do the enum id checking, nor do we have to up the
	// nesting level, since this only visits 1 voxel.
	int iEntityList;
	UtlHashFastHandle_t hHash = m_aVoxelHash.Find( v.uiVoxel );
	if ( hHash != m_aVoxelHash.InvalidHandle() )
	{
		iEntityList = m_aVoxelHash.Element( hHash );
		while ( iEntityList != m_aEntityList.InvalidIndex() )
		{
			SpatialPartitionHandle_t hPartition = m_aEntityList[iEntityList];
			iEntityList = m_aEntityList.Next( iEntityList );
			if ( hPartition == PARTITION_INVALID_HANDLE )
				continue;

			EntityInfo_t &hInfo = m_pTree->EntityInfo( hPartition );

			// Keep going if this dude isn't in the list
			if ( !( listMask & hInfo.m_fList ) )
				continue;

			if ( hInfo.m_flags & ENTITY_HIDDEN )
				continue;

			// Keep going if there's no collision
			if ( !IsPointInBox( pt, hInfo.m_vecMin, hInfo.m_vecMax ) )
				continue;

			// Okay, this one is good...
			if ( pIterator->EnumElement( hInfo.m_pHandleEntity ) == ITERATION_STOP )
				return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Debug! Render a voxel - blue.
//-----------------------------------------------------------------------------
void CVoxelHash::RenderVoxel( Voxel_t voxel, float flTime )
{
#ifndef SWDS
	Vector vecMin, vecMax;
	vecMin.x = ( voxel.bitsVoxel.x * VoxelSize() ) + m_vecVoxelOrigin.x;
	vecMin.y = ( voxel.bitsVoxel.y * VoxelSize() ) + m_vecVoxelOrigin.y;
	vecMin.z = ( voxel.bitsVoxel.z * VoxelSize() ) + m_vecVoxelOrigin.z;

	vecMax.x = ( ( voxel.bitsVoxel.x + 1 ) * VoxelSize() ) + m_vecVoxelOrigin.x;
	vecMax.y = ( ( voxel.bitsVoxel.y + 1 ) * VoxelSize() ) + m_vecVoxelOrigin.y;
	vecMax.z = ( ( voxel.bitsVoxel.z + 1 ) * VoxelSize() ) + m_vecVoxelOrigin.z;

	CDebugOverlay::AddBoxOverlay( vec3_origin, vecMin, vecMax, vec3_angle, s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 30, flTime );

	// Add outline.
	Vector vecPoints[8];
	vecPoints[0].Init( vecMin.x, vecMin.y, vecMin.z );
	vecPoints[1].Init( vecMin.x, vecMax.y, vecMin.z );
	vecPoints[2].Init( vecMax.x, vecMax.y, vecMin.z );
	vecPoints[3].Init( vecMax.x, vecMin.y, vecMin.z );
	vecPoints[4].Init( vecMin.x, vecMin.y, vecMax.z );
	vecPoints[5].Init( vecMin.x, vecMax.y, vecMax.z );
	vecPoints[6].Init( vecMax.x, vecMax.y, vecMax.z );
	vecPoints[7].Init( vecMax.x, vecMin.y, vecMax.z );

	CDebugOverlay::AddLineOverlay( vecPoints[0], vecPoints[1], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[1], vecPoints[2], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[2], vecPoints[3], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[3], vecPoints[0], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );

	CDebugOverlay::AddLineOverlay( vecPoints[4], vecPoints[5], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[5], vecPoints[6], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[6], vecPoints[7], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[7], vecPoints[4], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );

	CDebugOverlay::AddLineOverlay( vecPoints[0], vecPoints[4], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[3], vecPoints[7], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[1], vecPoints[5], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[2], vecPoints[6], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Debug! Render an object in a voxel - green.
//-----------------------------------------------------------------------------
void CVoxelHash::RenderObjectInVoxel( SpatialPartitionHandle_t hPartition, CPartitionVisitor *pVisitor, float flTime )
{
#ifndef SWDS
	// Add outline.
	if ( hPartition == PARTITION_INVALID_HANDLE )
		return;

	EntityInfo_t &info = m_pTree->EntityInfo( hPartition );
	if ( !pVisitor->Visit( hPartition, info ) )
	{
		return;
	}

	CDebugOverlay::AddBoxOverlay( vec3_origin, info.m_vecMin, info.m_vecMax, vec3_angle, s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 75, flTime );

	// Add outline.
	Vector vecMin, vecMax;
	vecMin = info.m_vecMin;
	vecMax = info.m_vecMax;

	Vector vecPoints[8];
	vecPoints[0].Init( vecMin.x, vecMin.y, vecMin.z );
	vecPoints[1].Init( vecMin.x, vecMax.y, vecMin.z );
	vecPoints[2].Init( vecMax.x, vecMax.y, vecMin.z );
	vecPoints[3].Init( vecMax.x, vecMin.y, vecMin.z );
	vecPoints[4].Init( vecMin.x, vecMin.y, vecMax.z );
	vecPoints[5].Init( vecMin.x, vecMax.y, vecMax.z );
	vecPoints[6].Init( vecMax.x, vecMax.y, vecMax.z );
	vecPoints[7].Init( vecMax.x, vecMin.y, vecMax.z );
	
	CDebugOverlay::AddLineOverlay( vecPoints[0], vecPoints[1], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[1], vecPoints[2], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[2], vecPoints[3], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[3], vecPoints[0], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	
	CDebugOverlay::AddLineOverlay( vecPoints[4], vecPoints[5], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[5], vecPoints[6], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[6], vecPoints[7], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[7], vecPoints[4], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	
	CDebugOverlay::AddLineOverlay( vecPoints[0], vecPoints[4], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[3], vecPoints[7], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[1], vecPoints[5], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
	CDebugOverlay::AddLineOverlay( vecPoints[2], vecPoints[6], s_pVoxelColor[m_nLevel][0], s_pVoxelColor[m_nLevel][1], s_pVoxelColor[m_nLevel][2], 255, true, flTime );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Debug! Render the objects in a voxel (optionally, render the voxel!).
//-----------------------------------------------------------------------------
void CVoxelHash::RenderObjectsInVoxel( Voxel_t voxel, CPartitionVisitor *pVisitor, bool bRenderVoxel, float flTime )
{
	UtlHashFastHandle_t hHash = m_aVoxelHash.Find( voxel.uiVoxel );
	if ( hHash == m_aVoxelHash.InvalidHandle() )
		return;

	int iEntityList = m_aVoxelHash.Element( hHash );
	while ( iEntityList != m_aEntityList.InvalidIndex() )
	{
		SpatialPartitionHandle_t hPartition = m_aEntityList[iEntityList];
		RenderObjectInVoxel( hPartition, pVisitor, flTime );					
		iEntityList = m_aEntityList.Next( iEntityList );
	}

	if ( bRenderVoxel )
	{
		RenderVoxel( voxel, flTime );
	}
}


//-----------------------------------------------------------------------------
// Returns number of entities in the hash
//-----------------------------------------------------------------------------
int CVoxelHash::EntityCount()
{
	int nCount = 0;
	int nBucketCount = SPHASH_BUCKET_COUNT;
	for ( int iBucket = 0; iBucket < nBucketCount; ++iBucket )
	{
		if ( m_aVoxelHash.m_aBuckets[iBucket].Count() == 0 )
			continue;

		UtlPtrLinkedListIndex_t hHash = m_aVoxelHash.m_aBuckets[iBucket].Head();
	
		while ( hHash != m_aVoxelHash.m_aBuckets[iBucket].InvalidIndex() )
		{
			int iEntity = m_aVoxelHash.m_aBuckets[iBucket][hHash].m_Data;
			while ( iEntity!= m_aEntityList.InvalidIndex() )
			{
				++nCount;
				iEntity = m_aEntityList.Next( iEntity );
			}

			hHash = m_aVoxelHash.m_aBuckets[iBucket].Next( hHash );
		}
	}
	return nCount;
}


//-----------------------------------------------------------------------------
// Rendering methods
//-----------------------------------------------------------------------------
void CVoxelHash::RenderGrid()
{
#ifndef SWDS
	Vector vecStart, vecEnd;
	for ( int i = 0; i < m_nVoxelDelta[0]; ++i )
	{
		vecStart.x = vecEnd.x = i * VoxelSize() + m_vecVoxelOrigin.x;
		for ( int j = 0; j < m_nVoxelDelta[1]; ++j )
		{
			vecStart.y = vecEnd.y = j * VoxelSize() + m_vecVoxelOrigin.y;
			vecStart.z = m_vecVoxelOrigin.z;
			vecEnd.z = m_nVoxelDelta[2] * VoxelSize() + m_vecVoxelOrigin.z;

			RenderLine( vecStart, vecEnd, s_pVoxelColor[m_nLevel], true );
		}
	}

	for ( int i = 0; i < m_nVoxelDelta[0]; ++i )
	{
		vecStart.x = vecEnd.x = i * VoxelSize() + m_vecVoxelOrigin.x;
		for ( int j = 0; j < m_nVoxelDelta[2]; ++j )
		{
			vecStart.z = vecEnd.z = j * VoxelSize() + m_vecVoxelOrigin.z;
			vecStart.y = m_vecVoxelOrigin.y;
			vecEnd.y = m_nVoxelDelta[2] * VoxelSize() + m_vecVoxelOrigin.y;

			RenderLine( vecStart, vecEnd, s_pVoxelColor[m_nLevel], true );
		}
	}

	for ( int i = 0; i < m_nVoxelDelta[1]; ++i )
	{
		vecStart.y = vecEnd.y = i * VoxelSize() + m_vecVoxelOrigin.y;
		for ( int j = 0; j < m_nVoxelDelta[2]; ++j )
		{
			vecStart.z = vecEnd.z = j * VoxelSize() + m_vecVoxelOrigin.z;
			vecStart.x = m_vecVoxelOrigin.z;
			vecEnd.x = m_nVoxelDelta[2] * VoxelSize() + m_vecVoxelOrigin.x;

			RenderLine( vecStart, vecEnd, s_pVoxelColor[m_nLevel], true );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Debug! Render boxes around objects in tree.
//-----------------------------------------------------------------------------
void CVoxelHash::RenderAllObjectsInTree( float flTime )
{
	int nBucketCount = SPHASH_BUCKET_COUNT;

	CPartitionVisits *pPrevVisits = m_pTree->BeginVisit();
	CPartitionVisitor visitor( m_pTree );

	for ( int iBucket = 0; iBucket < nBucketCount; ++iBucket )
	{
		if ( m_aVoxelHash.m_aBuckets[iBucket].Count() == 0 )
			continue;

		UtlPtrLinkedListIndex_t hHash = m_aVoxelHash.m_aBuckets[iBucket].Head();

		while ( hHash != m_aVoxelHash.m_aBuckets[iBucket].InvalidIndex() )
		{
			int iEntity = m_aVoxelHash.m_aBuckets[iBucket][hHash].m_Data;
			while ( iEntity!= m_aEntityList.InvalidIndex() )
			{
				SpatialPartitionHandle_t hPartition = m_aEntityList[iEntity];
				RenderObjectInVoxel( hPartition, &visitor, flTime );
				iEntity = m_aEntityList.Next( iEntity );
			}

			hHash = m_aVoxelHash.m_aBuckets[iBucket].Next( hHash );
		}
	}

	m_pTree->EndVisit( pPrevVisits );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelHash::RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime )
{
	// Visit each voxel in the box and enumerate its elements.
	Voxel_t voxelMin, voxelMax;
	voxelMin = VoxelIndexFromPoint( vecPlayerMin );
	voxelMax = VoxelIndexFromPoint( vecPlayerMax );
	
	CPartitionVisits *pPrevVisits = m_pTree->BeginVisit();

	// Create leaf cache.
	Voxel_t voxel;
	unsigned int iX, iY, iZ;
	CPartitionVisitor visitor( m_pTree );
	for ( iX = voxelMin.bitsVoxel.x; iX <= voxelMax.bitsVoxel.x; ++iX )
	{
		for ( iY = voxelMin.bitsVoxel.y; iY <= voxelMax.bitsVoxel.y; ++iY )
		{
			for ( iZ = voxelMin.bitsVoxel.z; iZ <= voxelMax.bitsVoxel.z; ++iZ )
			{
				voxel.bitsVoxel.x = iX;
				voxel.bitsVoxel.y = iY;
				voxel.bitsVoxel.z = iZ;
				RenderObjectsInVoxel( voxel, &visitor, false, flTime );
			}
		}
	}

	m_pTree->EndVisit( pPrevVisits );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

CVoxelTree::CVoxelTree() : m_pVoxelHash( NULL ), m_pOwner( NULL ), m_nNextVisitBit( 0 )
{
	// Compute max number of levels
	m_nLevelCount = 0;
	while ( CVoxelHash::ComputeVoxelCountAtLevel( m_nLevelCount ) > 2 )
	{
		++m_nLevelCount;
	}
	++m_nLevelCount;	// Account for the level where count = 2;

	// Various optimizations I've made require 4 levels
	Assert( m_nLevelCount == 4 );

	m_pVoxelHash = new CVoxelHash[m_nLevelCount]; 

	m_AvailableVisitBits.EnsureCapacity( 2048 );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CVoxelTree::~CVoxelTree()
{
	delete[] m_pVoxelHash;
}


void ClampVector( Vector &out, const Vector &mins, const Vector &maxs )
{
	for ( int i = 0; i < 3; i++ )
	{
		out[i] = clamp(out[i], mins[i], maxs[i]);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//   Input: worldmin - 
//          worldmax - 
//-----------------------------------------------------------------------------
void CVoxelTree::Init( CSpatialPartition *pOwner, int iTree, const Vector &worldmin, const Vector &worldmax )
{
	m_pOwner = pOwner;
	m_TreeId = iTree;

	// Reset the enumeration id.
	m_pVisits = NULL;

	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		m_pVoxelHash[i].Init( this, worldmin, worldmax, i );
	}

	// Setup the leaf list pool.
	m_aLeafList.Purge();
	m_aLeafList.SetGrowSize( SPHASH_LEAFLIST_BLOCK );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelTree::Shutdown( void )
{
	m_aLeafList.Purge();
	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		m_pVoxelHash[i].Shutdown();
	}
}


//-----------------------------------------------------------------------------
// Insert into the appropriate tree
//-----------------------------------------------------------------------------
void CVoxelTree::InsertIntoTree( SpatialPartitionHandle_t hPartition, const Vector& mins, const Vector& maxs )
{
	bool bWasReading = ( m_pVisits != NULL );
	if ( bWasReading )
	{
		// If we're recursing in this thread, need to release our read lock to allow ourselves to write
		UnlockRead();
	}

	m_lock.LockForWrite();
	Assert( hPartition != PARTITION_INVALID_HANDLE );

	EntityInfo_t &info = EntityInfo( hPartition );
	if ( m_AvailableVisitBits.Count() )
	{
		info.m_nVisitBit[m_TreeId] = m_AvailableVisitBits.Tail();
		m_AvailableVisitBits.Remove( m_AvailableVisitBits.Count() - 1 );
	}
	else
	{
		info.m_nVisitBit[m_TreeId] = m_nNextVisitBit++;
	}

	// Bloat by an eps before inserting the object into the tree.
	Vector vecMin( mins.x - SPHASH_EPS, mins.y - SPHASH_EPS, mins.z - SPHASH_EPS );
	Vector vecMax( maxs.x + SPHASH_EPS, maxs.y + SPHASH_EPS, maxs.z + SPHASH_EPS );

	ClampVector(vecMin, s_PartitionMin, s_PartitionMax);
	ClampVector(vecMax, s_PartitionMin, s_PartitionMax);
	Vector vecSize;
	VectorSubtract( vecMax, vecMin, vecSize );

	int nLevel;
	for ( nLevel = 0; nLevel < m_nLevelCount - 1; ++nLevel )
	{
		int nVoxelSize = m_pVoxelHash[nLevel].VoxelSize();
		if ( (nVoxelSize > vecSize.x) && (nVoxelSize > vecSize.y) && (nVoxelSize > vecSize.z) )
			break;
	}
	m_pVoxelHash[nLevel].InsertIntoTree( hPartition, vecMin, vecMax );
	m_lock.UnlockWrite();

	if ( bWasReading )
	{
		LockForRead();
	}
}


//-----------------------------------------------------------------------------
// Remove from appropriate tree
//-----------------------------------------------------------------------------
void CVoxelTree::RemoveFromTree( SpatialPartitionHandle_t hPartition )
{
	Assert( hPartition != PARTITION_INVALID_HANDLE );
	EntityInfo_t &info = EntityInfo( hPartition );
	int nLevel = info.m_nLevel[GetTreeId()];
	if ( nLevel >= 0 )
	{
		bool bWasReading = ( m_pVisits != NULL );
		if ( bWasReading )
		{
			// If we're recursing in this thread, need to release our read lock to allow ourselves to write
			UnlockRead();
		}

		m_lock.LockForWrite();
		m_pVoxelHash[nLevel].RemoveFromTree( hPartition );
		m_AvailableVisitBits.AddToTail( info.m_nVisitBit[m_TreeId] );
		info.m_nVisitBit[m_TreeId] = (unsigned short)-1;
		m_lock.UnlockWrite();

		if ( bWasReading )
		{
			LockForRead();
		}
	}
}


//-----------------------------------------------------------------------------
// Called when an element moves
//-----------------------------------------------------------------------------
void CVoxelTree::ElementMoved( SpatialPartitionHandle_t hPartition, const Vector& mins, const Vector& maxs )
{
	if ( hPartition != PARTITION_INVALID_HANDLE )
	{
		// If it doesn't already exist in the tree - add it.
		EntityInfo_t &info = EntityInfo( hPartition );
		if ( info.m_iLeafList[GetTreeId()] == CLeafList::InvalidIndex() )
		{
			InsertIntoTree( hPartition, mins, maxs );
			return;
		} 

		// Bloat by an eps before inserting the object into the tree.
		// Need to do this here to get the test to work
		Vector vecEpsMin( mins.x - SPHASH_EPS, mins.y - SPHASH_EPS, mins.z - SPHASH_EPS );
		Vector vecEpsMax( maxs.x + SPHASH_EPS, maxs.y + SPHASH_EPS, maxs.z + SPHASH_EPS );

		if ( (info.m_vecMin == vecEpsMin) && (info.m_vecMax == vecEpsMax) )
		{
			return;
		}

		// Remove entity from voxel hash.
		RemoveFromTree( hPartition );

		// Re-insert entity into voxel hash.
		InsertIntoTree( hPartition, mins, maxs );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelTree::EnumerateElementsInBox( SpatialPartitionListMask_t listMask, 
										const Vector& vecMins, const Vector& vecMaxs, 
										bool coarseTest, IPartitionEnumerator* pIterator )
{
	VPROF( "BoxTest/SphereTest" );

	// If this assertion fails, you're using a list at a point where the spatial partition elements aren't set up!
	//	Assert( ( listMask & m_nSuppressedListMask ) == 0 );

	// Early-out.
	if ( listMask == 0 )
		return;

	// Clamp bounds to extant space
	Vector mins, maxs;
	VectorMax( vecMins, s_PartitionMin, mins );
	VectorMin( mins, s_PartitionMax, mins );

	VectorMax( vecMaxs, s_PartitionMin, maxs );
	VectorMin( maxs, s_PartitionMax, maxs );

	// Callbacks.
	CPartitionVisits *pPrevVisits = BeginVisit();

	m_lock.LockForRead();
	Voxel_t vs = m_pVoxelHash[0].VoxelIndexFromPoint( mins );
	Voxel_t ve = m_pVoxelHash[0].VoxelIndexFromPoint( maxs );
	if ( !m_pVoxelHash[0].EnumerateElementsInBox( listMask, vs, ve, mins, maxs, pIterator ) )
	{
		m_lock.UnlockRead();
		EndVisit( pPrevVisits );
		return;
	}

	vs = ConvertToNextLevel( vs );
	ve = ConvertToNextLevel( ve );
	if ( !m_pVoxelHash[1].EnumerateElementsInBox( listMask, vs, ve, mins, maxs, pIterator ) )
	{
		m_lock.UnlockRead();
		EndVisit( pPrevVisits );
		return;
	}

	vs = ConvertToNextLevel( vs );
	ve = ConvertToNextLevel( ve );
	if ( !m_pVoxelHash[2].EnumerateElementsInBox( listMask, vs, ve, mins, maxs, pIterator ) )
	{
		m_lock.UnlockRead();
		EndVisit( pPrevVisits );
		return;
	}

	vs = ConvertToNextLevel( vs );
	ve = ConvertToNextLevel( ve );
	m_pVoxelHash[3].EnumerateElementsInBox( listMask, vs, ve, mins, maxs, pIterator );

	m_lock.UnlockRead();
	EndVisit( pPrevVisits );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelTree::EnumerateElementsInSphere( SpatialPartitionListMask_t listMask, 
										   const Vector& origin, float radius, bool coarseTest, IPartitionEnumerator* pIterator )
{
	// Otherwise they might as well just walk the entire ent list!!!
	Assert( radius <= MAX_COORD_FLOAT );

	// If the box test is fast enough - forget about the sphere test?
	Vector vecMin( origin.x - radius, origin.y - radius, origin.z - radius );
	Vector vecMax( origin.x + radius, origin.y + radius, origin.z + radius );
	return EnumerateElementsInBox( listMask, vecMin, vecMax, coarseTest, pIterator );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelTree::EnumerateElementsAlongRay_Ray( SpatialPartitionListMask_t listMask, 
											   const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator )
{
	// Find the voxel start + end
	Voxel_t voxelStart = m_pVoxelHash[0].VoxelIndexFromPoint( ray.m_Start );
	Voxel_t voxelEnd = m_pVoxelHash[0].VoxelIndexFromPoint( vecEnd );

	bool bSingleVoxel = ( voxelStart.uiVoxel == voxelEnd.uiVoxel );
	CIntersectRay intersectRay( this, ray, vecInvDelta );

	// Optimization: Look for single voxel rays
	if ( bSingleVoxel )
	{
		if ( !m_pVoxelHash[0].EnumerateElementsInSingleVoxel( voxelStart, intersectRay, listMask, pIterator ) )
			return false;
		voxelStart = ConvertToNextLevel( voxelStart );
		if ( !m_pVoxelHash[1].EnumerateElementsInSingleVoxel( voxelStart, intersectRay, listMask, pIterator ) )
			return false;
		voxelStart = ConvertToNextLevel( voxelStart );
		if ( !m_pVoxelHash[2].EnumerateElementsInSingleVoxel( voxelStart, intersectRay, listMask, pIterator ) )
			return false;
		voxelStart = ConvertToNextLevel( voxelStart );
		return m_pVoxelHash[3].EnumerateElementsInSingleVoxel( voxelStart, intersectRay, listMask, pIterator );
	}

	Voxel_t voxelCurrent;
	voxelCurrent.uiVoxel = voxelStart.uiVoxel;

	// Setup.
	int nStep[3];
	float tMax[3];
	float tDelta[3];
	m_pVoxelHash[0].LeafListRaySetup( ray, vecEnd, vecInvDelta, voxelStart, nStep, tMax, tDelta );

	// Walk the voxels and visit all elements in each voxel

	// Deal with all levels
	Voxel_t ov1, ov2, ov3;
	ov1.uiVoxel = ov2.uiVoxel = ov3.uiVoxel = 0xFFFFFFFF;

	Voxel_t v1 = ConvertToNextLevel( voxelCurrent );
	Voxel_t v2 = ConvertToNextLevel( v1 );
	Voxel_t v3 = ConvertToNextLevel( v2 );

	while ( 1 )
	{
		if ( !m_pVoxelHash[0].EnumerateElementsInVoxel( voxelCurrent, intersectRay, listMask, pIterator ) )
			return false;

		if ( v1.uiVoxel != ov1.uiVoxel )
		{
			if ( !m_pVoxelHash[1].EnumerateElementsInVoxel( v1, intersectRay, listMask, pIterator ) )
				return false;
		}
		if ( v2.uiVoxel != ov2.uiVoxel )
		{
			if ( !m_pVoxelHash[2].EnumerateElementsInVoxel( v2, intersectRay, listMask, pIterator ) )
				return false;
		}
		if ( v3.uiVoxel != ov3.uiVoxel )
		{
			if ( !m_pVoxelHash[3].EnumerateElementsInVoxel( v3, intersectRay, listMask, pIterator ) )
				return false;
		}

		if ( tMax[0] >= 1.0f && tMax[1] >= 1.0f && tMax[2] >= 1.0f )
			break;

		if ( tMax[0] < tMax[1] )
		{
			if ( tMax[0] < tMax[2] )
			{
				voxelCurrent.bitsVoxel.x += nStep[0];
				tMax[0] += tDelta[0];
			}
			else
			{
				voxelCurrent.bitsVoxel.z += nStep[2];
				tMax[2] += tDelta[2];
			}
		}
		else
		{
			if ( tMax[1] < tMax[2] )
			{
				voxelCurrent.bitsVoxel.y += nStep[1];
				tMax[1] += tDelta[1];
			}
			else
			{
				voxelCurrent.bitsVoxel.z += nStep[2];
				tMax[2] += tDelta[2];
			}
		}

		ov1 = v1; ov2 = v2; ov3 = v3;
		v1 = ConvertToNextLevel( voxelCurrent );
		v2 = ConvertToNextLevel( v1 );
		v3 = ConvertToNextLevel( v2 );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelTree::ComputeSweptRayBounds( const Ray_t &ray, const Vector &vecStartMin, const Vector &vecStartMax, Vector *pVecMin, Vector *pVecMax )
{
	if ( ray.m_Delta.x < 0 )
	{
		pVecMin->x = vecStartMin.x + ray.m_Delta.x;
		pVecMax->x = vecStartMax.x;
	}
	else
	{
		pVecMin->x = vecStartMin.x;
		pVecMax->x = vecStartMax.x + ray.m_Delta.x;
	}

	if ( ray.m_Delta.y < 0 )
	{
		pVecMin->y = vecStartMin.y + ray.m_Delta.y;
		pVecMax->y = vecStartMax.y;
	}
	else
	{
		pVecMin->y = vecStartMin.y;
		pVecMax->y = vecStartMax.y + ray.m_Delta.y;
	}

	if ( ray.m_Delta.z < 0 )
	{
		pVecMin->z = vecStartMin.z + ray.m_Delta.z;
		pVecMax->z = vecStartMax.z;
	}
	else
	{
		pVecMin->z = vecStartMin.z;
		pVecMax->z = vecStartMax.z + ray.m_Delta.z;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoxelTree::EnumerateElementsAlongRay_ExtrudedRay( SpatialPartitionListMask_t listMask, 
													   const Ray_t &ray, const Vector &vecInvDelta, const Vector &vecEnd, IPartitionEnumerator *pIterator )
{
	// Check the starting position, then proceed with the sweep.
	Vector vecMin, vecMax;
	VectorSubtract( ray.m_Start, ray.m_Extents, vecMin );
	VectorAdd( ray.m_Start, ray.m_Extents, vecMax );

	// Visit each voxel in the box and enumerate its elements.
	// Indexed as voxelBounds[level][min/max][x/y/z]
	int voxelBounds[4][2][3];
	m_pVoxelHash[0].VoxelIndexFromPoint( vecMin, voxelBounds[0][0] );
	m_pVoxelHash[0].VoxelIndexFromPoint( vecMax, voxelBounds[0][1] );

	CIntersectSweptBox intersectSweptBox( this, ray, vecInvDelta );

	// Iterate over all voxels that intersect the box around the starting ray point
	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		voxelBounds[i][0][0] = voxelBounds[0][0][0] >> ( i * SPHASH_LEVEL_SKIP );
		voxelBounds[i][0][1] = voxelBounds[0][0][1] >> ( i * SPHASH_LEVEL_SKIP );
		voxelBounds[i][0][2] = voxelBounds[0][0][2] >> ( i * SPHASH_LEVEL_SKIP );

		voxelBounds[i][1][0] = voxelBounds[0][1][0] >> ( i * SPHASH_LEVEL_SKIP );
		voxelBounds[i][1][1] = voxelBounds[0][1][1] >> ( i * SPHASH_LEVEL_SKIP );
		voxelBounds[i][1][2] = voxelBounds[0][1][2] >> ( i * SPHASH_LEVEL_SKIP );

		Voxel_t voxel;
		int iX, iY, iZ;
		for ( iX = voxelBounds[i][0][0]; iX <= voxelBounds[i][1][0]; ++iX )
		{
			voxel.bitsVoxel.x = iX;
			for ( iY = voxelBounds[i][0][1]; iY <= voxelBounds[i][1][1]; ++iY )
			{
				voxel.bitsVoxel.y = iY;
				for ( iZ = voxelBounds[i][0][2]; iZ <= voxelBounds[i][1][2]; ++iZ )
				{
					voxel.bitsVoxel.z = iZ;
					if ( !m_pVoxelHash[i].EnumerateElementsInVoxel( voxel, intersectSweptBox, listMask, pIterator ) )
						return false;
				}
			}
		}
	}

	// Early out: Check to see if the range of voxels at the endpoint
	// is the same as the range at the start point. If so, we're done.
	Vector vecEndMin, vecEndMax;
	VectorSubtract( vecEnd, ray.m_Extents, vecEndMin );
	VectorAdd( vecEnd, ray.m_Extents, vecEndMax );

	int endVoxelMin[3], endVoxelMax[3];
	m_pVoxelHash[0].VoxelIndexFromPoint( vecEndMin, endVoxelMin );
	m_pVoxelHash[0].VoxelIndexFromPoint( vecEndMax, endVoxelMax );
	if ( (endVoxelMin[0] >= voxelBounds[0][0][0]) && (endVoxelMin[1] >= voxelBounds[0][0][1]) && (endVoxelMin[2] >= voxelBounds[0][0][2]) &&
		(endVoxelMax[0] <= voxelBounds[0][1][0]) && (endVoxelMax[1] <= voxelBounds[0][1][1]) && (endVoxelMax[2] <= voxelBounds[0][1][2]) )
		return true;

	// Setup.
	int nStep[3];
	float tMax[3];	// amount of change in t along ray until we hit the next new voxel
	float tMin[3];	// amount of change in t along ray until we leave the last voxel
	float tDelta[3];
	m_pVoxelHash[0].LeafListExtrudedRaySetup( ray, vecInvDelta, vecMin, vecMax, voxelBounds[0][0], voxelBounds[0][1], nStep, tMin, tMax, tDelta );

	int nLastVoxel1[3];
	int nLastVoxel2[3];
	int nLastVoxel3[3];
	for ( int i = 0; i < 3; ++i )
	{
		int nIndex = ( nStep[i] > 0 ) ? 1 : 0;
		nLastVoxel1[i] = voxelBounds[1][nIndex][i];
		nLastVoxel2[i] = voxelBounds[2][nIndex][i];
		nLastVoxel3[i] = voxelBounds[3][nIndex][i];
	}

	// Walk the voxels and create the leaf list.
	int iAxis, iMinAxis;
	while ( tMax[0] < 1.0f || tMax[1] < 1.0f || tMax[2] < 1.0f )
	{
		iAxis = MinIndex( tMax[0], tMax[1], tMax[2] );
		iMinAxis = MinIndex( tMin[0], tMin[1], tMin[2] );

		if ( tMin[iMinAxis] < tMax[iAxis] )
		{
			tMin[iMinAxis] += tDelta[iMinAxis];
			int nIndex = ( nStep[iMinAxis] > 0 ) ? 0 : 1;
			voxelBounds[0][nIndex][iMinAxis] += nStep[iMinAxis];
			voxelBounds[1][nIndex][iMinAxis] = voxelBounds[0][nIndex][iMinAxis] >> SPHASH_LEVEL_SKIP;
			voxelBounds[2][nIndex][iMinAxis] = voxelBounds[0][nIndex][iMinAxis] >> (2 * SPHASH_LEVEL_SKIP);
			voxelBounds[3][nIndex][iMinAxis] = voxelBounds[0][nIndex][iMinAxis] >> (3 * SPHASH_LEVEL_SKIP);
		}
		else
		{
			tMax[iAxis] += tDelta[iAxis];
			int nIndex = ( nStep[iAxis] > 0 ) ? 1 : 0;
			voxelBounds[0][nIndex][iAxis] += nStep[iAxis];
			voxelBounds[1][nIndex][iAxis] = voxelBounds[0][nIndex][iAxis] >> SPHASH_LEVEL_SKIP;
			voxelBounds[2][nIndex][iAxis] = voxelBounds[0][nIndex][iAxis] >> (2 * SPHASH_LEVEL_SKIP);
			voxelBounds[3][nIndex][iAxis] = voxelBounds[0][nIndex][iAxis] >> (3 * SPHASH_LEVEL_SKIP);

			if ( !m_pVoxelHash[0].EnumerateElementsAlongRay_ExtrudedRaySlice( listMask, pIterator, intersectSweptBox, voxelBounds[0][0], voxelBounds[0][1], iAxis, nStep ) )
				return false;

			if ( nLastVoxel1[iAxis] != voxelBounds[1][nIndex][iAxis] )
			{
				nLastVoxel1[iAxis] = voxelBounds[1][nIndex][iAxis];
				if ( !m_pVoxelHash[1].EnumerateElementsAlongRay_ExtrudedRaySlice( listMask, pIterator, intersectSweptBox, voxelBounds[1][0], voxelBounds[1][1], iAxis, nStep ) )
					return false;
			}

			if ( nLastVoxel2[iAxis] != voxelBounds[2][nIndex][iAxis] )
			{
				nLastVoxel2[iAxis] = voxelBounds[2][nIndex][iAxis];
				if ( !m_pVoxelHash[2].EnumerateElementsAlongRay_ExtrudedRaySlice( listMask, pIterator, intersectSweptBox, voxelBounds[2][0], voxelBounds[2][1], iAxis, nStep ) )
					return false;
			}

			if ( nLastVoxel3[iAxis] != voxelBounds[3][nIndex][iAxis] )
			{
				nLastVoxel3[iAxis] = voxelBounds[3][nIndex][iAxis];
				if ( !m_pVoxelHash[3].EnumerateElementsAlongRay_ExtrudedRaySlice( listMask, pIterator, intersectSweptBox, voxelBounds[3][0], voxelBounds[3][1], iAxis, nStep ) )
					return false;
			}
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void CVoxelTree::EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, 
										   const Ray_t &ray, bool coarseTest, IPartitionEnumerator *pIterator )
{
	VPROF("EnumerateElementsAlongRay");

	if ( !ray.m_IsSwept )
	{
		Vector vecMin, vecMax;
		VectorSubtract( ray.m_Start, ray.m_Extents, vecMin );
		VectorAdd( ray.m_Start, ray.m_Extents, vecMax );
		return EnumerateElementsInBox( listMask, vecMin, vecMax, coarseTest, pIterator );
	}

	// If this assertion fails, you're using a list at a point where the spatial partition elements aren't set up!
	//	Assert( ( listMask & m_nSuppressedListMask ) == 0 );

	// Early-out.
	if ( listMask == 0 )
		return;

	// Calculate the end of the ray
	Vector vecEnd;
	Vector vecInvDelta;
	Ray_t clippedRay = ray;
	VectorAdd( clippedRay.m_Start, clippedRay.m_Delta, vecEnd );

	bool bStartIn = IsPointInBox( ray.m_Start, s_PartitionMin, s_PartitionMax );
	bool bEndIn = IsPointInBox( vecEnd, s_PartitionMin, s_PartitionMax );
	if ( !bStartIn && !bEndIn )
		return;

	// Callbacks.
	if ( !bStartIn )
	{
		ClampStartPoint( clippedRay, vecEnd );
	}
	else if ( !bEndIn )
	{
		ClampEndPoint( clippedRay, vecEnd );
	}

	vecInvDelta[0] = ( clippedRay.m_Delta[0] != 0.0f ) ? 1.0f / clippedRay.m_Delta[0] : FLT_MAX;
	vecInvDelta[1] = ( clippedRay.m_Delta[1] != 0.0f ) ? 1.0f / clippedRay.m_Delta[1] : FLT_MAX;
	vecInvDelta[2] = ( clippedRay.m_Delta[2] != 0.0f ) ? 1.0f / clippedRay.m_Delta[2] : FLT_MAX;

	CPartitionVisits *pPrevVisits = BeginVisit();

	m_lock.LockForRead();
	if ( ray.m_IsRay )
	{
		EnumerateElementsAlongRay_Ray( listMask, clippedRay, vecInvDelta, vecEnd, pIterator );
	}
	else
	{
		EnumerateElementsAlongRay_ExtrudedRay( listMask, clippedRay, vecInvDelta, vecEnd, pIterator );
	}

	m_lock.UnlockRead();
	EndVisit( pPrevVisits );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelTree::EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask, 
										  const Vector& pt, bool coarseTest, IPartitionEnumerator* pIterator )
{
	// If this assertion fails, you're using a list at a point where the spatial partition elements aren't set up!
	//	Assert( ( listMask & m_nSuppressedListMask ) == 0 );

	// Early-out.
	if ( listMask == 0 )
		return;

	m_lock.LockForRead();
	// Callbacks.
	Voxel_t v = m_pVoxelHash[0].VoxelIndexFromPoint( pt );
	if ( !m_pVoxelHash[0].EnumerateElementsAtPoint( listMask, v, pt, pIterator ) )
	{
		m_lock.UnlockRead();
		return;
	}

	v = ConvertToNextLevel( v );
	if ( !m_pVoxelHash[1].EnumerateElementsAtPoint( listMask, v, pt, pIterator ) )
	{
		m_lock.UnlockRead();
		return;
	}

	v = ConvertToNextLevel( v );
	if ( !m_pVoxelHash[2].EnumerateElementsAtPoint( listMask, v, pt, pIterator ) )
	{
		m_lock.UnlockRead();
		return;
	}

	v = ConvertToNextLevel( v );
	m_pVoxelHash[3].EnumerateElementsAtPoint( listMask, v, pt, pIterator );
	m_lock.UnlockRead();
}


//-----------------------------------------------------------------------------
// Purpose: Debug! Render boxes around objects in tree.
//-----------------------------------------------------------------------------
void CVoxelTree::RenderAllObjectsInTree( float flTime )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	m_lock.LockForRead();
	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		m_pVoxelHash[i].RenderAllObjectsInTree( flTime );
	}
	m_lock.UnlockRead();
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoxelTree::RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	m_lock.LockForRead();
	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		m_pVoxelHash[i].RenderObjectsInPlayerLeafs( vecPlayerMin, vecPlayerMax, flTime );
	}
	m_lock.UnlockRead();
}


//-----------------------------------------------------------------------------
// Expose CSpatialPartition to the game + client DLL.
//-----------------------------------------------------------------------------
static CSpatialPartition	g_SpatialPartition;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSpatialPartition, ISpatialPartition, INTERFACEVERSION_SPATIALPARTITION, g_SpatialPartition );

//-----------------------------------------------------------------------------
// Expose ISpatialPartitionInternal to the engine.
//-----------------------------------------------------------------------------
ISpatialPartitionInternal *SpatialPartition()
{
	return &g_SpatialPartition;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSpatialPartition::CSpatialPartition()
{
	m_nQueryCallbackCount = 0;
}


CSpatialPartition::~CSpatialPartition()
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose:
//   Input: worldmin - 
//          worldmax - 
//-----------------------------------------------------------------------------
void CSpatialPartition::Init( const Vector &worldmin, const Vector &worldmax )
{
	// Clear the handle list and ensure some new memory.
	m_aHandles.Purge();
	m_aHandles.EnsureCapacity( SPHASH_HANDLELIST_BLOCK );

	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_VoxelTrees[i].Init( this, i, worldmin, worldmax );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::Shutdown( void )
{
	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_VoxelTrees[i].Shutdown();
	}
	m_aHandles.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: Add a callback to the query callback list.  Functions get called 
//          right before a query occurs.
//   Input: pCallback - pointer to the callback function to add
//-----------------------------------------------------------------------------
void CSpatialPartition::InstallQueryCallback( IPartitionQueryCallback *pCallback )
{
	// Verify data.
	Assert( pCallback && m_nQueryCallbackCount < MAX_QUERY_CALLBACK );
	if ( !pCallback || ( m_nQueryCallbackCount >= MAX_QUERY_CALLBACK ) )
		return;

	m_pQueryCallback[m_nQueryCallbackCount] = pCallback;
	m_bUseOldQueryCallback[m_nQueryCallbackCount] = false;
	++m_nQueryCallbackCount;
}

//-----------------------------------------------------------------------------
// Purpose: Add a callback to the query callback list.  Functions get called 
//          right before a query occurs.
//   Input: pCallback - pointer to the callback function to add
//-----------------------------------------------------------------------------
void CSpatialPartition::InstallQueryCallback_V1( IPartitionQueryCallback *pCallback )
{
	// Verify data.
	Assert( pCallback && m_nQueryCallbackCount < MAX_QUERY_CALLBACK );
	if ( !pCallback || ( m_nQueryCallbackCount >= MAX_QUERY_CALLBACK ) )
		return;

	// NOTE: the query callbacks are not mutexed. Only add and remove when threads are joined

	m_pQueryCallback[m_nQueryCallbackCount] = pCallback;
	m_bUseOldQueryCallback[m_nQueryCallbackCount] = true;
	++m_nQueryCallbackCount;
}


//-----------------------------------------------------------------------------
// Purpose: Remove a callback from the query callback list.
//   Input: pCallback - pointer to the callback function to remove
//-----------------------------------------------------------------------------
void CSpatialPartition::RemoveQueryCallback( IPartitionQueryCallback *pCallback )
{
	// Verify data.
	if ( !pCallback )
		return;

	for ( int iQuery = m_nQueryCallbackCount; --iQuery >= 0;  )
	{
		if ( m_pQueryCallback[iQuery] == pCallback )
		{
			--m_nQueryCallbackCount;
			m_pQueryCallback[iQuery] = m_pQueryCallback[m_nQueryCallbackCount];
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Invokes the pre-query callbacks.
//-----------------------------------------------------------------------------
void CSpatialPartition::InvokeQueryCallbacks( SpatialPartitionListMask_t listMask, bool bDone )
{
	for ( int iQuery = 0; iQuery < m_nQueryCallbackCount; ++iQuery )
	{
		if ( !bDone )
		{
			if ( m_bUseOldQueryCallback[iQuery] )
			{
				m_pQueryCallback[iQuery]->OnPreQuery_V1();
			}
			else
			{
				m_pQueryCallback[iQuery]->OnPreQuery( listMask );
			}
		}
		else
		{
			if ( !m_bUseOldQueryCallback[iQuery] )
			{
				m_pQueryCallback[iQuery]->OnPostQuery( listMask );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create spatial partition object handle.
//   Input: pHandleEntity - entity handle of the object to create a spatial partition handle for
//-----------------------------------------------------------------------------
SpatialPartitionHandle_t CSpatialPartition::CreateHandle( IHandleEntity *pHandleEntity )
{
	m_HandlesMutex.Lock();
	SpatialPartitionHandle_t hPartition = m_aHandles.AddToTail();
	m_HandlesMutex.Unlock();
	m_aHandles[hPartition].m_pHandleEntity = pHandleEntity;
	m_aHandles[hPartition].m_vecMin.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	m_aHandles[hPartition].m_vecMax.Init( FLT_MIN, FLT_MIN, FLT_MIN );
	m_aHandles[hPartition].m_fList = 0;
	m_aHandles[hPartition].m_flags = 0;

	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_aHandles[hPartition].m_nVisitBit[i] = 0xffff;
		m_aHandles[hPartition].m_nLevel[i] = (uint8)-1;
		m_aHandles[hPartition].m_iLeafList[i] = CLeafList::InvalidIndex();
	}
	
	return hPartition;
}


//-----------------------------------------------------------------------------
// Purpose: Destroy spatial partition object handle.
//   Input: handle - handle of the spatial partition object handle to destroy
//-----------------------------------------------------------------------------
void CSpatialPartition::DestroyHandle( SpatialPartitionHandle_t hPartition )
{
	if ( hPartition != PARTITION_INVALID_HANDLE )
	{
		RemoveFromTree( hPartition );
		m_HandlesMutex.Lock();
//		memset( &m_aHandles[hPartition], 0xcd, sizeof(EntityInfo_t) );
		m_aHandles.Remove( hPartition );
		m_HandlesMutex.Unlock();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create spatial partition object handle and insert it into the tree.
//   Input: pHandleEntity - entity handle of the object to create a spatial partition handle for
//          listMask -
//          mins - 
//          maxs -
//-----------------------------------------------------------------------------
SpatialPartitionHandle_t CSpatialPartition::CreateHandle( IHandleEntity *pHandleEntity, 
															  SpatialPartitionListMask_t listMask, 
														      const Vector &mins, const Vector &maxs )
{
//	CDebugOverlay::AddBoxOverlay( vec3_origin, mins, maxs, vec3_angle, 0, 255, 0, 75, 3600 );

	SpatialPartitionHandle_t hPartition = CreateHandle( pHandleEntity );
	Insert( listMask, hPartition );
	InsertIntoTree( hPartition, mins, maxs );

	return hPartition;
}

//-----------------------------------------------------------------------------
// Purpose: Insert object handle into group(s).
//   Input: listId - list(s) to insert the object handle into
//          handle - object handle to be inserted into list
//-----------------------------------------------------------------------------
void CSpatialPartition::Insert( SpatialPartitionListMask_t listId, SpatialPartitionHandle_t handle )
{
	Assert( m_aHandles.IsValidIndex( handle ) );
	Assert( listId <= USHRT_MAX );
	m_aHandles[handle].m_fList |= listId;
}

//-----------------------------------------------------------------------------
// Purpose: Remove object handle from group(s).
//   Input: listId - list(s) to remove the object handle from
//          handle - object handle to be removed from list
//-----------------------------------------------------------------------------
void CSpatialPartition::Remove( SpatialPartitionListMask_t listId, SpatialPartitionHandle_t handle )
{
	Assert( m_aHandles.IsValidIndex( handle ) );
	Assert( listId <= USHRT_MAX );
	m_aHandles[handle].m_fList &= ~listId;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::RemoveAndInsert( SpatialPartitionListMask_t removeMask, SpatialPartitionListMask_t insertMask, 
											 SpatialPartitionHandle_t handle )
{
	Assert( m_aHandles.IsValidIndex( handle ) );
	Assert( removeMask <= USHRT_MAX );
	Assert( insertMask <= USHRT_MAX );
	m_aHandles[handle].m_fList &= ~removeMask;
	m_aHandles[handle].m_fList |= insertMask;
}

//-----------------------------------------------------------------------------
// Purpose: Remove object handle from all groups.
//   Input: handle - object handle to be removed from all lists
//-----------------------------------------------------------------------------
void CSpatialPartition::Remove( SpatialPartitionHandle_t handle )
{
	Assert( m_aHandles.IsValidIndex( handle ) );
	m_aHandles[handle].m_fList = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Fast way to re-add (set) a group that was removed (and saved) via the
//          HideElement call.
//-----------------------------------------------------------------------------
void CSpatialPartition::UnhideElement( SpatialPartitionHandle_t handle, SpatialTempHandle_t tempHandle )
{
	Assert( m_aHandles.IsValidIndex( handle ) );

	m_HandlesMutex.Lock();
	m_aHandles[handle].m_flags &= ~ENTITY_HIDDEN;
	m_HandlesMutex.Unlock();
}


//-----------------------------------------------------------------------------
// Purpose: Remove handle quickly saving the old list data, to be restored later
//          via the UnhideElement call.
//-----------------------------------------------------------------------------
SpatialTempHandle_t CSpatialPartition::HideElement( SpatialPartitionHandle_t handle )
{
	Assert( m_aHandles.IsValidIndex( handle ) );
	m_HandlesMutex.Lock();
	m_aHandles[handle].m_flags |= ENTITY_HIDDEN;
	m_HandlesMutex.Unlock();

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: (Debugging) Suppress queries on particular lists.
//   Input: nListMask - lists to suppress/unsuppress
//          bSuppress - (true/false) suppress/unsuppress
//-----------------------------------------------------------------------------
void CSpatialPartition::SuppressLists( SpatialPartitionListMask_t nListMask, bool bSuppress )
{
	if ( bSuppress )
	{
		m_nSuppressedListMask |= nListMask;
	}
	else
	{
		m_nSuppressedListMask &= ~nListMask;
	}
}

//-----------------------------------------------------------------------------
// Purpose: (Debugging) Get the suppression list.
//  Output: spatial partition suppression list
//-----------------------------------------------------------------------------
SpatialPartitionListMask_t CSpatialPartition::GetSuppressedLists( void )
{
	return m_nSuppressedListMask;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::ElementMoved( SpatialPartitionHandle_t handle, const Vector& mins, const Vector& maxs )
{
	EntityInfo_t &entityInfo = EntityInfo( handle );
	SpatialPartitionListMask_t listMask = entityInfo.m_fList;


	if ( CLIENT_TREE != SERVER_TREE )
	{
		if ( listMask & PARTITION_ALL_CLIENT_EDICTS )
		{
			m_VoxelTrees[CLIENT_TREE].ElementMoved( handle, mins, maxs );
			entityInfo.m_flags |= IN_CLIENT_TREE;
		}

		if ( listMask & ~PARTITION_ALL_CLIENT_EDICTS )
		{
			m_VoxelTrees[SERVER_TREE].ElementMoved( handle, mins, maxs );
			entityInfo.m_flags |= IN_SERVER_TREE;
		}
	}
	else
	{
		m_VoxelTrees[CLIENT_TREE].ElementMoved( handle, mins, maxs );
		entityInfo.m_flags |= IN_CLIENT_TREE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::EnumerateElementsInBox( SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs, bool coarseTest, IPartitionEnumerator* pIterator )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	CVoxelTree *pTree = VoxelTree( listMask );
	InvokeQueryCallbacks( listMask );
	pTree->EnumerateElementsInBox( listMask, mins, maxs, coarseTest, pIterator );
	InvokeQueryCallbacks( listMask, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::EnumerateElementsInSphere( SpatialPartitionListMask_t listMask, const Vector& origin, float radius, bool coarseTest, IPartitionEnumerator* pIterator )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	CVoxelTree *pTree = VoxelTree( listMask );
	InvokeQueryCallbacks( listMask );
	pTree->EnumerateElementsInSphere( listMask, origin, radius, coarseTest, pIterator );
	InvokeQueryCallbacks( listMask, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::EnumerateElementsAlongRay( SpatialPartitionListMask_t listMask, const Ray_t& ray, bool coarseTest, IPartitionEnumerator* pIterator )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	CVoxelTree *pTree = VoxelTree( listMask );
	InvokeQueryCallbacks( listMask );
	pTree->EnumerateElementsAlongRay( listMask, ray, coarseTest, pIterator );
	InvokeQueryCallbacks( listMask, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::EnumerateElementsAtPoint( SpatialPartitionListMask_t listMask, const Vector& pt, bool coarseTest, IPartitionEnumerator* pIterator )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	CVoxelTree *pTree = VoxelTree( listMask );
	InvokeQueryCallbacks( listMask );
	pTree->EnumerateElementsAtPoint( listMask, pt, coarseTest, pIterator );
	InvokeQueryCallbacks( listMask, true );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::InsertIntoTree( SpatialPartitionHandle_t hPartition, const Vector& mins, const Vector& maxs ) 
{
	EntityInfo_t &entityInfo = EntityInfo( hPartition );
	SpatialPartitionListMask_t listMask = entityInfo.m_fList;

	if ( CLIENT_TREE != SERVER_TREE )
	{
		if ( ( listMask & PARTITION_ALL_CLIENT_EDICTS ) && !( entityInfo.m_flags & IN_CLIENT_TREE ) )
		{
			m_VoxelTrees[CLIENT_TREE].InsertIntoTree( hPartition, mins, maxs );
			entityInfo.m_flags |= IN_CLIENT_TREE;
		}

		if ( ( listMask & ~PARTITION_ALL_CLIENT_EDICTS ) && !( entityInfo.m_flags & IN_SERVER_TREE ) )
		{
			m_VoxelTrees[SERVER_TREE].InsertIntoTree( hPartition, mins, maxs );
			entityInfo.m_flags |= IN_SERVER_TREE;
		}
	}
	else if ( !( entityInfo.m_flags & IN_CLIENT_TREE ) )
	{
		m_VoxelTrees[CLIENT_TREE].InsertIntoTree( hPartition, mins, maxs );
		entityInfo.m_flags |= IN_CLIENT_TREE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::RemoveFromTree( SpatialPartitionHandle_t hPartition ) 
{ 
	EntityInfo_t &entityInfo = EntityInfo( hPartition );

	if ( entityInfo.m_flags & IN_CLIENT_TREE )
	{
		m_VoxelTrees[CLIENT_TREE].RemoveFromTree( hPartition ); 
		entityInfo.m_flags &= ~IN_CLIENT_TREE;
	}

	if ( entityInfo.m_flags & IN_SERVER_TREE )
	{
		m_VoxelTrees[SERVER_TREE].RemoveFromTree( hPartition ); 
		entityInfo.m_flags &= ~IN_SERVER_TREE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::RenderObjectsInBox( const Vector &vecMin, const Vector &vecMax, float flTime )
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSpatialPartition::RenderObjectsInSphere( const Vector &vecCenter, float flRadius, float flTime )
{
}


void CSpatialPartition::RenderObjectsAlongRay( const Ray_t& ray, float flTime )
{
}

	
//-----------------------------------------------------------------------------
// Report stats
//-----------------------------------------------------------------------------
void CSpatialPartition::RenderAllObjectsInTree( float flTime )
{
	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_VoxelTrees[i].RenderAllObjectsInTree( flTime );
	}
}

void CSpatialPartition::RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime )
{
	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_VoxelTrees[i].RenderObjectsInPlayerLeafs( vecPlayerMin, vecPlayerMax, flTime );
	}
}

//-----------------------------------------------------------------------------
// Report stats
//-----------------------------------------------------------------------------
void CVoxelTree::ReportStats( const char *pFileName )
{
	Msg( "Histogram : Entities per level\n" );
	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		Msg( "\t%d - %d\n", i, m_pVoxelHash[i].EntityCount() );
	}
}

void CSpatialPartition::ReportStats( const char *pFileName )
{
	Msg( "Handle Count %d (%llu bytes)\n", m_aHandles.Count(), (uint64)( m_aHandles.Count() * ( sizeof(EntityInfo_t) + 2 * sizeof(SpatialPartitionHandle_t) ) ) );
	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_VoxelTrees[i].ReportStats( pFileName );
	}
}

static ConVar r_partition_level( "r_partition_level", "-1", FCVAR_CHEAT, "Displays a particular level of the spatial partition system. Use -1 to disable it." );

void CVoxelTree::DrawDebugOverlays()
{
	int nLevel = r_partition_level.GetInt();
	if ( nLevel < 0 )
		return;

	m_lock.LockForRead();
	for ( int i = 0; i < m_nLevelCount; ++i )
	{
		if ( ( nLevel >= 0 ) && ( nLevel != i ) )
			continue;

		m_pVoxelHash[i].RenderGrid();
		m_pVoxelHash[i].RenderAllObjectsInTree( 0.01f );
	}
	m_lock.UnlockRead();
}

void CSpatialPartition::DrawDebugOverlays()
{
	for ( int i = 0; i < NUM_TREES; i++ )
	{
		m_VoxelTrees[i].DrawDebugOverlays();
	}
}

//=============================================================================
ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax )
{
	CSpatialPartition *pResult = new CSpatialPartition;
	pResult->Init( worldmin, worldmax );
	return pResult;
}

void DestroySpatialPartition( ISpatialPartition *pPartition )
{
	Assert( pPartition != (ISpatialPartition*)&g_SpatialPartition );
	delete pPartition;
}


