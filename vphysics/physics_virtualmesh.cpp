//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Virtual mesh implementation.  Cached terrain collision model
//
//=============================================================================


#include "cbase.h"
#include "convert.h"
#include "ivp_surface_manager.hxx"
#include "ivp_surman_polygon.hxx"
#include "ivp_template_surbuild.hxx"
#include "ivp_compact_surface.hxx"
#include <ivp_compact_ledge.hxx>
#include <ivp_ray_solver.hxx>
#include <ivp_compact_ledge_solver.hxx>
#include "ivp_surbuild_pointsoup.hxx"
#include "ivp_surbuild_ledge_soup.hxx"
#include "physics_trace.h"
#include "collisionutils.h"
#include "datamanager.h"
#include "utlbuffer.h"
#include "ledgewriter.h"
#include "tier1/mempool.h"
#include "tier0/memdbgon.h"

class CPhysCollideVirtualMesh;

CTSPool< CUtlVector<CPhysCollideVirtualMesh *> > g_MeshFrameLocksPool;
CThreadLocalPtr< CUtlVector<CPhysCollideVirtualMesh *> > g_pMeshFrameLocks;

// This is the surfacemanager class for IVP that implements the required functions by layering CPhysCollideVirtualMesh
class IVP_SurfaceManager_VirtualMesh : public IVP_SurfaceManager
{
public:
	void add_reference_to_ledge(const IVP_Compact_Ledge *ledge);
	void remove_reference_to_ledge(const IVP_Compact_Ledge *ledge);
	void insert_all_ledges_hitting_ray(IVP_Ray_Solver *ray_solver, IVP_Real_Object *object);
	void get_radius_and_radius_dev_to_given_center(const IVP_U_Float_Point *center, IVP_FLOAT *radius, IVP_FLOAT *radius_deviation) const;
	virtual IVP_SURMAN_TYPE get_type() { return IVP_SURMAN_POLYGON; }
	
	// assume mesh is never a single triangle
	virtual const IVP_Compact_Ledge *get_single_convex() const;
	void get_mass_center(IVP_U_Float_Point *mass_center_out) const;
	void get_rotation_inertia( IVP_U_Float_Point *rotation_inertia_out ) const;
	void get_all_ledges_within_radius(const IVP_U_Point *observer_os, IVP_DOUBLE radius,
		const IVP_Compact_Ledge *root_ledge, IVP_Real_Object *other_object, const IVP_Compact_Ledge *other_reference_ledge,
		IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges);

	void get_all_terminal_ledges(IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges);
	IVP_SurfaceManager_VirtualMesh( CPhysCollideVirtualMesh *pMesh );
	virtual ~IVP_SurfaceManager_VirtualMesh();

private:
	CPhysCollideVirtualMesh *m_pMesh;
};

// These are the managed objects for the LRU of terrain collisions
// These get created/destroyed dynamically by a resourcemanager
// These contain the uncompressed collision models for each displacement patch
// The idea is to have only the necessary instances of these in memory at any given time - never all of them
class CMeshInstance
{
public:
	// resourcemanager
	static unsigned int EstimatedSize( const virtualmeshlist_t &list );
	static CMeshInstance *CreateResource( const virtualmeshlist_t &list );
	static unsigned int ComputeRootLedgeSize( const byte *pHull );
	void DestroyResource() { delete this; }
	unsigned int Size() { return m_memSize; }
	CMeshInstance *GetData() { return this; }
	const triangleledge_t	*GetLedges() { return (triangleledge_t *)m_pMemory; }
	inline int HullCount() { return m_hullCount; }
	const IVP_Compact_Ledge *GetOuterHull() { return (m_hullCount==1) ? (const IVP_Compact_Ledge *)(m_pMemory + m_hullOffset) : NULL; }
	int GetRootLedges( IVP_Compact_Ledge **pLedges, int outCount ) 
	{ 
		int hullOffset = m_hullOffset;
		int count = min(outCount, (int)m_hullCount);
		for ( int i = 0; i < count; i++ )
		{
			pLedges[i] = (IVP_Compact_Ledge *)(m_pMemory + hullOffset);
			hullOffset += sizeof(IVP_Compact_Ledge) + (sizeof(IVP_Compact_Triangle) * pLedges[i]->get_n_triangles());
		}
		return count;
	}

	// locals
	CMeshInstance() { m_pMemory = 0; }
	~CMeshInstance();

private:
	void Init( const virtualmeshlist_t &list );

	int		m_memSize;
	char	*m_pMemory;
	unsigned short m_hullOffset;
	byte	m_hullCount;
	byte	m_pad;
};

CMeshInstance::~CMeshInstance()
{
	if ( m_pMemory )
	{
		ivp_free_aligned( m_pMemory );
		m_pMemory = NULL;
	}
}

unsigned int CMeshInstance::EstimatedSize( const virtualmeshlist_t &list )
{
	int ledgeSize = sizeof(triangleledge_t) * list.triangleCount;
	int pointSize = sizeof(IVP_Compact_Poly_Point) * list.vertexCount;

	int hullSize = ComputeRootLedgeSize(list.pHull);
	return ledgeSize + pointSize + hullSize;
}

// computes the unpacked size of the array of root ledges
unsigned int CMeshInstance::ComputeRootLedgeSize( const byte *pData )
{
	if ( !pData )
		return 0;
	virtualmeshhull_t *pHeader = (virtualmeshhull_t *)pData;
	packedhull_t *pHull = (packedhull_t *)(pHeader+1);
	unsigned int size = pHeader->hullCount * sizeof(IVP_Compact_Ledge);
	for ( int i = 0; i < pHeader->hullCount; i++ )
	{
		size += sizeof(IVP_Compact_Triangle) * pHull[i].triangleCount;
	}
	return size;
}

CMeshInstance *CMeshInstance::CreateResource( const virtualmeshlist_t &list )
{
	CMeshInstance *pMesh = new CMeshInstance;
	pMesh->Init( list );
	return pMesh;
}


// flat memory footprint has triangleledges (ledge + 2 triangles for terrain), then has verts, then optional convex hull
void CMeshInstance::Init( const virtualmeshlist_t &list )
{
	int ledgeSize = sizeof(triangleledge_t) * list.triangleCount;
	int pointSize = sizeof(IVP_Compact_Poly_Point) * list.vertexCount;
	int memSize = ledgeSize + pointSize + ComputeRootLedgeSize(list.pHull);
	m_memSize = memSize;
	m_hullCount = 0;
	m_pMemory = (char *)ivp_malloc_aligned( memSize, 16 );
	Assert( (int(m_pMemory) & 15) == 0 );	// make sure it is aligned
	IVP_Compact_Poly_Point *pPoints = (IVP_Compact_Poly_Point *)&m_pMemory[ledgeSize];
	triangleledge_t *pLedges = (triangleledge_t *) m_pMemory;
	memset( m_pMemory, 0, memSize );
	int i;

	for ( i = 0; i < list.vertexCount; i++ )
	{
		ConvertPositionToIVP( list.pVerts[i], pPoints[i] );
	}

	for ( i = 0; i < list.triangleCount; i++ )
	{
		Vector v0 = list.pVerts[list.indices[i*3+0]];
		Vector v1 = list.pVerts[list.indices[i*3+1]];
		Vector v2 = list.pVerts[list.indices[i*3+2]];
		Assert( v0 != v1 && v1 != v2 && v0 != v2 );
		CVPhysicsVirtualMeshWriter::InitTwoSidedTriangleLege( &pLedges[i], pPoints, list.indices[i*3+0], list.indices[i*3+1], list.indices[i*3+2], 0 );
	}
	Assert( list.triangleCount > 0 && list.triangleCount <= MAX_VIRTUAL_TRIANGLES );
	// if there's a hull, build it out too
	if ( list.pHull )
	{
		virtualmeshhull_t *pHeader = (virtualmeshhull_t *)list.pHull;
		m_hullCount = pHeader->hullCount;
		Assert( (ledgeSize + pointSize) < 65536 );
		m_hullOffset = ledgeSize + pointSize;
		byte *pMem = (byte *)m_pMemory + m_hullOffset;
#if _DEBUG
		int hullSize = CVPhysicsVirtualMeshWriter::UnpackLedgeListFromHull( pMem, pHeader, pPoints );
		Assert((m_hullOffset+hullSize)==memSize);
#else
		CVPhysicsVirtualMeshWriter::UnpackLedgeListFromHull( pMem, pHeader, pPoints );
#endif
	}
}

// UNDONE: Tune / expose this constant 512K budget for terrain collision
const int g_MeshSize = (2048 * 1024);
static CDataManager<CMeshInstance, virtualmeshlist_t, CMeshInstance *, CThreadFastMutex> g_MeshManager( g_MeshSize );
static int numIndices = 0, numTriangles = 0, numBaseTriangles = 0, numSplits = 0;
//-----------------------------------------------------------------------------
// Purpose: This allows for just-in-time procedural triangle soup data to be 
//			instanced & cached as IVP collision data (compact ledges)
//-----------------------------------------------------------------------------
// NOTE: This is the permanent in-memory representation.  It holds the compressed data
// and the parameters necessary to request the proxy geometry as needed
class CPhysCollideVirtualMesh : public CPhysCollide
{
public:
	// UNDONE: Unlike other CPhysCollide objects, operations the virtual mesh are
	// non-const because they may instantiate the cache.  This causes problems with the interface.
	// Maybe the cache stuff should be mutable, but it amounts to the same kind of
	// hackery to cast away const.
	
	// get a surface manager 
	virtual IVP_SurfaceManager *CreateSurfaceManager( short &collideType ) const
	{
		collideType = COLLIDE_VIRTUAL;
		// UNDONE: Figure out how to avoid this const_cast
		return new IVP_SurfaceManager_VirtualMesh(const_cast<CPhysCollideVirtualMesh *>(this));
	}
	virtual void GetAllLedges( IVP_U_BigVector<IVP_Compact_Ledge> &ledges ) const
	{
		const triangleledge_t *pLedges = const_cast<CPhysCollideVirtualMesh *>(this)->AddRef()->GetLedges();
		for ( int i = 0; i < m_ledgeCount; i++ )
		{
			ledges.add( const_cast<IVP_Compact_Ledge *>(&pLedges[i].ledge) );
		}
		const_cast<CPhysCollideVirtualMesh *>(this)->Release();
	}
	virtual unsigned int GetSerializationSize() const 
	{ 
		if ( !m_pHull )
			return 0; 
		return m_pHull->TotalSize();
	}

	virtual unsigned int SerializeToBuffer( char *pDest, bool bSwap = false ) const 
	{
		unsigned int size = GetSerializationSize();
		if ( size )
		{
			memcpy( pDest, m_pHull, size );
		}
		return size;
	}
	virtual int GetVCollideIndex() const { return 0; }
	virtual void SetMassCenter( const Vector &massCenter ) {Assert(0); }
	virtual Vector GetOrthographicAreas() const { return Vector(1,1,1);}

	Vector GetMassCenter() const;
	virtual float GetSphereRadius() const;
	float GetSphereRadiusIVP() const;
	void Init( const char *pBuffer, unsigned int size )
	{
	}
	void GetAllLedgesWithinRadius( const IVP_U_Point *observer_os, IVP_DOUBLE radius, IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges, const IVP_Compact_Ledge *pRootLedge = NULL )
	{
		virtualmeshtrianglelist_t list;
		list.triangleCount = 0;
		Vector centerHL;
		ConvertPositionToHL( *observer_os, centerHL );
		float radiusHL = ConvertDistanceToHL(radius);
		m_params.pMeshEventHandler->GetTrianglesInSphere( m_params.userData, centerHL, radiusHL, &list );
		if ( list.triangleCount )
		{
			CMeshInstance *pMesh = AddRef();
			const triangleledge_t *pLedges = pMesh->GetLedges();
			FrameRelease();

			// If we have two root ledges, then each one contains half the triangles
			// only return triangles indexed under the root ledge being queried
			int minTriangle = 0;
			int maxTriangle = m_ledgeCount;
			if ( pMesh->HullCount() > 1 )
			{
				Assert(pMesh->HullCount()==2);
				IVP_Compact_Ledge *pRootNodes[2];
				pMesh->GetRootLedges( pRootNodes, 2 );
				int midTriangle = m_ledgeCount/2;
				if ( pRootLedge == pRootNodes[0] )
				{
					maxTriangle = midTriangle;
				}
				else
				{
					minTriangle = midTriangle;
				}
			}
			IVP_DOUBLE radiusSq = radius * radius;
			for ( int i = 0; i < list.triangleCount; i++ )
			{
				Assert( list.triangleIndices[i] < m_ledgeCount );
				if ( list.triangleIndices[i] < minTriangle || list.triangleIndices[i] >= maxTriangle )
					continue;

				const IVP_Compact_Ledge *ledge = &pLedges[list.triangleIndices[i]].ledge;
				Assert(ledge->get_n_triangles() == 2);
				const IVP_Compact_Triangle *triangle = ledge->get_first_triangle();
				IVP_DOUBLE qdist = IVP_CLS.calc_qlen_PF_F_space(ledge, triangle, observer_os);
				if ( qdist > radiusSq ) 
				{
					continue;
				}

				resulting_ledges->add( const_cast<IVP_Compact_Ledge *>(ledge) );
			}
		}
	}

	virtual void OutputDebugInfo() const
	{
		Msg("Virtual mesh!\n");
	}

	CPhysCollideVirtualMesh(const virtualmeshparams_t &params) : m_params(params), m_hMemory( INVALID_MEMHANDLE ), m_ledgeCount( 0 )
	{
		m_pHull = NULL;
		if ( params.buildOuterHull )
		{
			BuildBoundingLedge();
		}
	}

	virtual ~CPhysCollideVirtualMesh();

	// adds a lock on the collsion memory :: MUST CALL Release() or FrameRelease corresponding to this call!!!
	CMeshInstance *AddRef();

	void BuildBoundingLedge();
	static virtualmeshhull_t *CreateMeshBoundingHull( const virtualmeshlist_t &list );
	static void DestroyMeshBoundingHull(virtualmeshhull_t *pHull) { CVPhysicsVirtualMeshWriter::DestroyPackedHull(pHull); }
	static IVP_Compact_Surface *CreateBoundingSurfaceFromRange( const virtualmeshlist_t &list, int firstIndex, int indexCount );

	int GetRootLedges( IVP_Compact_Ledge **pLedges, int outCount )
	{
		int count = AddRef()->GetRootLedges(pLedges, outCount);
		FrameRelease();
		return count;
	}

	IVP_Compact_Ledge *GetBoundingLedge()
	{
		IVP_Compact_Ledge *pLedge = const_cast<IVP_Compact_Ledge *>(AddRef()->GetOuterHull());
		FrameRelease();
		return pLedge;
	}

	// releases a lock on the collision memory
	void Release();
	// Analagous to Release, but happens at the end of the frame
	void FrameRelease() 
	{ 
		CUtlVector<CPhysCollideVirtualMesh *> *pLocks = g_pMeshFrameLocks;
		if ( !pLocks )
		{
			g_pMeshFrameLocks = pLocks = g_MeshFrameLocksPool.GetObject();
			Assert( pLocks );
		}
		pLocks->AddToTail(this);	
	}
	inline void GetBounds( Vector &mins, Vector &maxs ) const
	{
		m_params.pMeshEventHandler->GetWorldspaceBounds( m_params.userData, &mins, &maxs );
	}

private:
	CMeshInstance *BuildLedges();

	virtualmeshparams_t m_params;
	virtualmeshhull_t *m_pHull;
	memhandle_t		m_hMemory;
	short			m_ledgeCount;
};

static void FlushFrameLocks()
{
	CUtlVector<CPhysCollideVirtualMesh *> *pLocks = g_pMeshFrameLocks;
	if ( pLocks )
	{
		for ( int i = 0; i < pLocks->Count(); i++ )
		{
			Assert( (*pLocks)[i] );
			(*pLocks)[i]->Release();
		}
		pLocks->RemoveAll();
		g_MeshFrameLocksPool.PutObject( g_pMeshFrameLocks );
		g_pMeshFrameLocks = NULL;
	}
}

void VirtualMeshPSI()
{
	FlushFrameLocks();
}


Vector CPhysCollideVirtualMesh::GetMassCenter() const
{
	Vector mins, maxs;
	GetBounds( mins, maxs );
	return 0.5 * (mins + maxs);
}

float CPhysCollideVirtualMesh::GetSphereRadius() const
{
	Vector mins, maxs;
	GetBounds( mins, maxs );
	Vector point = 0.5 * (mins+maxs);
	return (maxs - point).Length();
}

float CPhysCollideVirtualMesh::GetSphereRadiusIVP() const
{
	return ConvertDistanceToIVP( GetSphereRadius() );
}

static CThreadFastMutex s_BuildVirtualMeshMutex;
CMeshInstance *CPhysCollideVirtualMesh::AddRef()
{
	CMeshInstance *pMesh = g_MeshManager.LockResource( m_hMemory );
	if ( !pMesh )
	{
		s_BuildVirtualMeshMutex.Lock();
		pMesh = g_MeshManager.LockResource( m_hMemory );
		if ( !pMesh )
		{
			pMesh = BuildLedges();
		}
		s_BuildVirtualMeshMutex.Unlock();
	}
	Assert( pMesh );
	return pMesh;
}
 
void CPhysCollideVirtualMesh::Release()
{
	g_MeshManager.UnlockResource( m_hMemory );
}

CPhysCollideVirtualMesh::~CPhysCollideVirtualMesh()
{
	CVPhysicsVirtualMeshWriter::DestroyPackedHull(m_pHull);
	g_MeshManager.DestroyResource( m_hMemory );
}

CMeshInstance *CPhysCollideVirtualMesh::BuildLedges()
{
	virtualmeshlist_t list;
	m_params.pMeshEventHandler->GetVirtualMesh( m_params.userData, &list );
	if ( !list.pHull )
	{
		list.pHull = (byte *)m_pHull;
	}
	
	if ( list.triangleCount )
	{
		m_hMemory = g_MeshManager.CreateResource( list );
		m_ledgeCount = list.triangleCount;
		CMeshInstance *pMesh = g_MeshManager.LockResource( m_hMemory );
		return pMesh;
	}
	return NULL;
}

// build the outer ledge, split into two if necessary
void CPhysCollideVirtualMesh::BuildBoundingLedge()
{
	virtualmeshlist_t list;
	m_params.pMeshEventHandler->GetVirtualMesh( m_params.userData, &list );
	m_pHull = CreateMeshBoundingHull(list);
}

virtualmeshhull_t *CPhysCollideVirtualMesh::CreateMeshBoundingHull( const virtualmeshlist_t &list )
{
	virtualmeshhull_t *pHull = NULL;
	if ( list.triangleCount )
	{
		IVP_Compact_Surface *pSurface = CreateBoundingSurfaceFromRange( list, 0, list.indexCount );
		if ( pSurface )
		{
			const IVP_Compact_Ledge *pLedge = pSurface->get_compact_ledge_tree_root()->get_compact_hull();
			if ( CVPhysicsVirtualMeshWriter::LedgeCanBePacked(pLedge, list) )
			{
				pHull = CVPhysicsVirtualMeshWriter::CreatePackedHullFromLedges( list, &pLedge, 1 );
			}
			else
			{
				// too big to pack to 8-bits, split in two
				IVP_Compact_Surface *pSurface0 = CreateBoundingSurfaceFromRange( list, 0, list.indexCount/2 );
				IVP_Compact_Surface *pSurface1 = CreateBoundingSurfaceFromRange( list, list.indexCount/2, list.indexCount/2 );

				const IVP_Compact_Ledge *pLedges[2] = {pSurface0->get_compact_ledge_tree_root()->get_compact_hull(), pSurface1->get_compact_ledge_tree_root()->get_compact_hull()};
				pHull = CVPhysicsVirtualMeshWriter::CreatePackedHullFromLedges( list, pLedges, 2 );
				ivp_free_aligned(pSurface0);
				ivp_free_aligned(pSurface1);
			}
			ivp_free_aligned(pSurface);
		}
	}
	return pHull;
}

IVP_Compact_Surface *CPhysCollideVirtualMesh::CreateBoundingSurfaceFromRange( const virtualmeshlist_t &list, int firstIndex, int indexCount )
{
	Assert( list.triangleCount );
	IVP_U_Point triVerts[3];
	IVP_U_Vector<IVP_U_Point> triList;
	IVP_SurfaceBuilder_Ledge_Soup builder;
	triList.add( &triVerts[0] );
	triList.add( &triVerts[1] );
	triList.add( &triVerts[2] );
	int lastIndex = firstIndex + indexCount;
	int firstTriangle = firstIndex/3;
	int lastTriangle = lastIndex/3;
	for ( int i = firstTriangle; i < lastTriangle; i++ )
	{
		ConvertPositionToIVP( list.pVerts[list.indices[i*3+0]], triVerts[0] );
		ConvertPositionToIVP( list.pVerts[list.indices[i*3+1]], triVerts[1] );
		ConvertPositionToIVP( list.pVerts[list.indices[i*3+2]], triVerts[2] );
		IVP_Compact_Ledge *pLedge = IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge( &triList );
		builder.insert_ledge( pLedge );
	}
	// build a convex hull of those verts
	IVP_Template_Surbuild_LedgeSoup params;
	params.build_root_convex_hull = IVP_TRUE;
	IVP_Compact_Surface *pSurface = builder.compile( &params );

#if _DEBUG
	const IVP_Compact_Ledgetree_Node *node = pSurface->get_compact_ledge_tree_root();
	IVP_Compact_Ledge *pLedge = const_cast<IVP_Compact_Ledge *>(node->get_compact_hull());	// we're going to write into client data on each vert before we throw this away
	Assert(pLedge && !pLedge->is_terminal());
#endif
	return pSurface;
}

CPhysCollide *CreateVirtualMesh( const virtualmeshparams_t &params )
{
	return new CPhysCollideVirtualMesh(params);
}

void DestroyVirtualMesh( CPhysCollide *pMesh )
{
	FlushFrameLocks();
	delete pMesh;
}

//-----------------------------------------------------------------------------
// IVP_SurfaceManager_VirtualMesh
// This hooks the underlying collision model to IVP's surfacemanager interface
//-----------------------------------------------------------------------------

IVP_SurfaceManager_VirtualMesh::IVP_SurfaceManager_VirtualMesh( CPhysCollideVirtualMesh *pMesh ) : m_pMesh(pMesh)
{
}

IVP_SurfaceManager_VirtualMesh::~IVP_SurfaceManager_VirtualMesh()
{
}

void IVP_SurfaceManager_VirtualMesh::add_reference_to_ledge(const IVP_Compact_Ledge *ledge)
{
	m_pMesh->AddRef();
}
void IVP_SurfaceManager_VirtualMesh::remove_reference_to_ledge(const IVP_Compact_Ledge *ledge)
{
	m_pMesh->Release();
}

// Implement the IVP raycast.  This is done by testing each triangle (front & back) - so it's slow
void IVP_SurfaceManager_VirtualMesh::insert_all_ledges_hitting_ray(IVP_Ray_Solver *ray_solver, IVP_Real_Object *object)
{
	IVP_Vector_of_Ledges_256 ledges;
	IVP_Ray_Solver_Os ray_solver_os( ray_solver, object);

	IVP_U_Point center(&ray_solver_os.ray_center_point);
	m_pMesh->GetAllLedgesWithinRadius( &center, ray_solver_os.ray_length * 0.5f, &ledges );

	for (int i=ledges.len()-1;i>=0;i--)
	{
		const IVP_Compact_Ledge *l = ledges.element_at(i);
		ray_solver_os.check_ray_against_compact_ledge_os(l);
	}
}

// Used to predict collision detection needs
void IVP_SurfaceManager_VirtualMesh::get_radius_and_radius_dev_to_given_center(const IVP_U_Float_Point *center, IVP_FLOAT *radius, IVP_FLOAT *radius_deviation) const
{
	// UNDONE: Check radius_deviation to see if there is a useful optimization to be made here
	*radius = m_pMesh->GetSphereRadiusIVP();
	*radius_deviation = *radius;
}

// get a single convex if appropriate
const IVP_Compact_Ledge *IVP_SurfaceManager_VirtualMesh::get_single_convex() const 
{ 
	return m_pMesh->GetBoundingLedge(); 
}

// get a mass center for objects using this collision rep
void IVP_SurfaceManager_VirtualMesh::get_mass_center(IVP_U_Float_Point *mass_center_out) const
{
	Vector center = m_pMesh->GetMassCenter();
	ConvertPositionToIVP( center, *mass_center_out );
}

//-----------------------------------------------------------------------------
// Purpose: Compute a diagonalized inertia tensor.
//-----------------------------------------------------------------------------
void IVP_SurfaceManager_VirtualMesh::get_rotation_inertia( IVP_U_Float_Point *rotation_inertia_out ) const
{
	// HACKHACK: No need for this because we only support static objects for now
	rotation_inertia_out->set(1,1,1);
}

//-----------------------------------------------------------------------------
// Purpose: Query ledges (triangles in this case) in sphere
//-----------------------------------------------------------------------------
void IVP_SurfaceManager_VirtualMesh::get_all_ledges_within_radius(const IVP_U_Point *observer_os, IVP_DOUBLE radius,
	const IVP_Compact_Ledge *root_ledge, IVP_Real_Object *other_object, const IVP_Compact_Ledge *other_reference_ledge,
	IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges)
{
	if ( !root_ledge )
	{
		IVP_Compact_Ledge *pLedges[2];
		int count = m_pMesh->GetRootLedges( pLedges, ARRAYSIZE(pLedges) );
		if ( count )
		{
			for ( int i = 0; i < count; i++ )
			{
				resulting_ledges->add( pLedges[i] ); // return the recursive/virtual outer hull
			}
			return;
		}
	}
	m_pMesh->GetAllLedgesWithinRadius( observer_os, radius, resulting_ledges, root_ledge );
}

//-----------------------------------------------------------------------------
// Purpose: Query all of the ledges (triangles)
//-----------------------------------------------------------------------------
void IVP_SurfaceManager_VirtualMesh::get_all_terminal_ledges(IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges)
{
	m_pMesh->GetAllLedges( *resulting_ledges );
}



