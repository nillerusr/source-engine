//===== Copyright 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//
// This file contains code to allow us to associate client data with bsp leaves.
//===========================================================================//

#include "cbase.h"
#include "ClientLeafSystem.h"
#include "UtlBidirectionalSet.h"
#include "model_types.h"
#include "IVRenderView.h"
#include "tier0/vprof.h"
#include "BSPTreeData.h"
#include "DetailObjectSystem.h"
#include "engine/IStaticPropMgr.h"
#include "engine/IVDebugOverlay.h"
#include "vstdlib/jobthread.h"
#include "tier1/utllinkedlist.h"
#include "datacache/imdlcache.h"
#include "view.h"
#include "iviewrender.h"
#include "viewrender.h"
#include "clientalphaproperty.h"
#include "con_nprint.h"
//#include "tier0/miniprofiler.h" 

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class VMatrix;  // forward decl

//extern LinkedMiniProfiler *g_pMiniProfilers;
//LinkedMiniProfiler g_mpRecomputeLeaves("CClientLeafSystem::RecomputeRenderableLeaves", &g_pMiniProfilers);
//LinkedMiniProfiler g_mpComputeBounds("CClientLeafSystem::ComputeBounds", &g_pMiniProfilers);


static ConVar cl_drawleaf("cl_drawleaf", "-1", FCVAR_CHEAT );
static ConVar r_PortalTestEnts( "r_PortalTestEnts", "1", FCVAR_CHEAT, "Clip entities against portal frustums." );
static ConVar r_portalsopenall( "r_portalsopenall", "0", FCVAR_CHEAT, "Open all portals" );

static ConVar r_shadows_on_renderables_enable( "r_shadows_on_renderables_enable", "0", 0, "Support casting RTT shadows onto other renderables" );

static ConVar cl_leafsystemvis( "cl_leafsystemvis", "0", FCVAR_CHEAT );

DEFINE_FIXEDSIZE_ALLOCATOR( CClientRenderablesList, 1, CUtlMemoryPool::GROW_SLOW );

//-----------------------------------------------------------------------------
// Threading helpers
//-----------------------------------------------------------------------------

static void FrameLock()
{
	mdlcache->BeginLock();
}

static void FrameUnlock()
{
	mdlcache->EndLock();
}


//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class CClientLeafSystem : public IClientLeafSystem, public ISpatialLeafEnumerator, public IClientAlphaPropertyMgr
{
public:
	virtual char const *Name() { return "CClientLeafSystem"; }

	// constructor, destructor
	CClientLeafSystem();
	virtual ~CClientLeafSystem();

	// Methods of IClientSystem
	bool Init() { return true; }
	void PostInit() {}
	void Shutdown() {}

	virtual bool IsPerFrame() { return true; }

	void PreRender();
	void PostRender() { }
	void Update( float frametime ) { m_nDebugIndex = 0; }

	void LevelInitPreEntity();
	void LevelInitPostEntity() {}
	void LevelShutdownPreEntity();
	void LevelShutdownPostEntity();

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

// Methods of IClientAlphaPropertyMgr
public:
	virtual IClientAlphaProperty *CreateClientAlphaProperty( IClientUnknown *pUnknown );
	virtual void DestroyClientAlphaProperty( IClientAlphaProperty *pAlphaProperty );

// Methods of IClientLeafSystem
public:
	
	virtual void AddRenderable( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabledFlags );
	virtual bool IsRenderableInPVS( IClientRenderable *pRenderable );
	virtual void CreateRenderableHandle( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled );
	virtual void RemoveRenderable( ClientRenderHandle_t handle );

	virtual void SetSubSystemDataInLeaf( int leaf, int nSubSystemIdx, CClientLeafSubSystemData *pData );
	virtual CClientLeafSubSystemData *GetSubSystemDataInLeaf( int leaf, int nSubSystemIdx );

	// FIXME: There's an incestuous relationship between DetailObjectSystem
	// and the ClientLeafSystem. Maybe they should be the same system?
	virtual void GetDetailObjectsInLeaf( int leaf, int& firstDetailObject, int& detailObjectCount );
 	virtual void SetDetailObjectsInLeaf( int leaf, int firstDetailObject, int detailObjectCount );
	virtual void DrawDetailObjectsInLeaf( int leaf, int frameNumber, int& nFirstDetailObject, int& nDetailObjectCount );
	virtual bool ShouldDrawDetailObjectsInLeaf( int leaf, int frameNumber );
	virtual void RenderableChanged( ClientRenderHandle_t handle );
	virtual void CollateViewModelRenderables( CViewModelRenderablesList *pList );
	virtual void BuildRenderablesList( const SetupRenderInfo_t &info );
	virtual void DrawStaticProps( bool enable );
	virtual void DrawSmallEntities( bool enable );
	virtual void EnableAlternateSorting( ClientRenderHandle_t handle, bool bEnable );
	virtual void RenderWithViewModels( ClientRenderHandle_t handle, bool bEnable );
	virtual bool IsRenderingWithViewModels( ClientRenderHandle_t handle ) const;
	virtual void SetTranslucencyType( ClientRenderHandle_t handle, RenderableTranslucencyType_t nType );
	virtual RenderableTranslucencyType_t GetTranslucencyType( ClientRenderHandle_t handle ) const;
	virtual void SetModelType( ClientRenderHandle_t handle, RenderableModelType_t nType );
	virtual void EnableSplitscreenRendering( ClientRenderHandle_t handle, uint32 nFlags );
	virtual void EnableRendering( ClientRenderHandle_t handle, bool bEnable );
	virtual void EnableBloatedBounds( ClientRenderHandle_t handle, bool bEnable );
	virtual void DisableCachedRenderBounds( ClientRenderHandle_t handle, bool bDisable );

	// Adds a renderable to a set of leaves
	virtual void AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves );
	void AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves, bool bReceiveShadows );

	// The following methods are related to shadows...
	virtual ClientLeafShadowHandle_t AddShadow( ClientShadowHandle_t userId, unsigned short flags );
	virtual void RemoveShadow( ClientLeafShadowHandle_t h );

	virtual void ProjectShadow( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList );
	virtual void ProjectFlashlight( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList );

	// Find all shadow casters in a set of leaves
	virtual void EnumerateShadowsInLeaves( int leafCount, WorldListLeafData_t* pLeaves, IClientLeafShadowEnum* pEnum );
	virtual void RecomputeRenderableLeaves();
	virtual void DisableLeafReinsertion( bool bDisable );

	//Assuming the renderable would be in a properly built render list, generate a render list entry
	virtual RenderGroup_t GenerateRenderListEntry( IClientRenderable *pRenderable, CClientRenderablesList::CEntry &entryOut );

	// methods of ISpatialLeafEnumerator
public:

	bool EnumerateLeaf( int leaf, int context );

	// Adds a shadow to a leaf
	void AddShadowToLeaf( int leaf, ClientLeafShadowHandle_t handle, bool bFlashlight );

	// Fill in a list of the leaves this renderable is in.
	// Returns -1 if the handle is invalid.
	int GetRenderableLeaves( ClientRenderHandle_t handle, int leaves[128] );

	// Get leaves this renderable is in
	virtual bool GetRenderableLeaf ( ClientRenderHandle_t handle, int* pOutLeaf, const int* pInIterator = 0, int* pOutIterator = 0 );

	// Singleton instance...
	static CClientLeafSystem s_ClientLeafSystem;

private:
	enum
	{
		RENDER_FLAGS_DISABLE_RENDERING		= 0x01,
		RENDER_FLAGS_HASCHANGED				= 0x02,
		RENDER_FLAGS_ALTERNATE_SORTING		= 0x04,
		RENDER_FLAGS_RENDER_WITH_VIEWMODELS	= 0x08,
		RENDER_FLAGS_BLOAT_BOUNDS			= 0x10,
		RENDER_FLAGS_BOUNDS_VALID			= 0x20,
		RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE = 0x40,
	};

	// All the information associated with a particular handle
	struct RenderableInfo_t
	{
		IClientRenderable*	m_pRenderable;
		CClientAlphaProperty *m_pAlphaProperty;
		int					m_EnumCount;				// Have I been added to a particular shadow yet?
		int					m_nRenderFrame;
		unsigned short		m_FirstShadow;				// The first shadow caster that cast on it
		unsigned short		m_LeafList;					// What leafs is it in?
		short				m_Area;						// -1 if the renderable spans multiple areas.
		uint16				m_Flags : 10;				// rendering flags
		uint16				m_nSplitscreenEnabled : 2;	// splitscreen rendering flags
		uint16				m_nTranslucencyType : 2;	// RenderableTranslucencyType_t
		uint16				m_nModelType : 2;			// RenderableModelType_t
		Vector				m_vecBloatedAbsMins;		// Use this for tree insertion
		Vector				m_vecBloatedAbsMaxs;
		Vector				m_vecAbsMins;			// NOTE: These members are not threadsafe!!
		Vector				m_vecAbsMaxs;			// They can be updated from any viewpoint (based on RENDER_FLAGS_BOUNDS_VALID)
	};

	// The leaf contains an index into a list of renderables
	struct ClientLeaf_t
	{
		unsigned short	m_FirstElement;
		unsigned short	m_FirstShadow;

		unsigned short	m_FirstDetailProp;
		unsigned short	m_DetailPropCount;
		int				m_DetailPropRenderFrame;
		CClientLeafSubSystemData *m_pSubSystemData[N_CLSUBSYSTEMS];
	};

	// Shadow information
	struct ShadowInfo_t
	{
		unsigned short	m_FirstLeaf;
		unsigned short	m_FirstRenderable;
		int				m_EnumCount;
		ClientShadowHandle_t	m_Shadow;
		unsigned short	m_Flags;
	};

	struct EnumResult_t
	{
		int leaf;
		EnumResult_t *pNext;
	};

	struct EnumResultList_t
	{
		EnumResult_t *pHead;
		ClientRenderHandle_t handle;
	};

	struct BuildRenderListInfo_t
	{
		Vector	m_vecMins;
		Vector	m_vecMaxs;
		short	m_nArea;
		uint8	m_nAlpha;
		bool	m_bPerformOcclusionTest : 1;
		bool	m_bIgnoreZBuffer : 1;
	};

	struct AlphaInfo_t
	{
		CClientAlphaProperty *m_pAlphaProperty;
		Vector m_vecCenter;
		float m_flRadius;
		float m_flFadeFactor;
	};

private:
	// Adds a renderable to the list of renderables
	void AddRenderableToLeaf( int leaf, ClientRenderHandle_t handle, bool bReceiveShadows );

	void SortEntities( const Vector &vecRenderOrigin, const Vector &vecRenderForward, CClientRenderablesList::CEntry *pEntities, int nEntities );

	// Returns -1 if the renderable spans more than one area. If it's totally in one area, then this returns the leaf.
	short GetRenderableArea( ClientRenderHandle_t handle );

	// remove renderables from leaves
	void RemoveFromTree( ClientRenderHandle_t handle );
	void InsertIntoTree( ClientRenderHandle_t &handle, const Vector &absMins, const Vector &absMaxs );

	// Adds, removes renderables from view model list
	void AddToViewModelList( ClientRenderHandle_t handle );
	void RemoveFromViewModelList( ClientRenderHandle_t handle );

	// Insert translucent renderables into list of translucent objects
	void InsertTranslucentRenderable( IClientRenderable* pRenderable,
		int& count, IClientRenderable** pList, float* pDist );

	// Adds a shadow to a leaf/removes shadow from renderable
	void AddShadowToRenderable( ClientRenderHandle_t renderHandle, ClientLeafShadowHandle_t shadowHandle );
	void RemoveShadowFromRenderables( ClientLeafShadowHandle_t handle );

	// Adds a shadow to a leaf/removes shadow from renderable
	bool ShouldRenderableReceiveShadow( ClientRenderHandle_t renderHandle, int nShadowFlags );

	// Adds a shadow to a leaf/removes shadow from leaf
	void RemoveShadowFromLeaves( ClientLeafShadowHandle_t handle );

	// Methods related to renderable list building
	int ExtractStaticProps( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractSplitscreenRenderables( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractTranslucentRenderables( int nCount, RenderableInfo_t **ppRenderables );
	int ExtractDuplicates( int nFrameNumber, int nCount, RenderableInfo_t **ppRenderables );
	void ComputeBounds( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo );
	int ExtractCulledRenderables( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo );
	int ExtractOccludedRenderables( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo );
	void AddRenderablesToRenderLists( const SetupRenderInfo_t &info, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, int nDetailCount, DetailRenderableInfo_t *pDetailInfo );
	void AddDependentRenderables( const SetupRenderInfo_t &info );

	int ComputeTranslucency( int nFrameNumber, int nViewID, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo );
	void ComputeDistanceFade( int nCount, AlphaInfo_t *pAlphaInfo, BuildRenderListInfo_t *pRLInfo );
	void ComputeScreenFade( const ScreenSizeComputeInfo_t &info, float flMinScreenWidth, float flMaxScreenWidth, int nCount, AlphaInfo_t *pAlphaInfo );

	void CalcRenderableWorldSpaceAABB_Bloated( const RenderableInfo_t &info, Vector &absMin, Vector &absMax );

	// Methods associated with the various bi-directional sets
	static unsigned short& FirstRenderableInLeaf( int leaf ) 
	{ 
		return s_ClientLeafSystem.m_Leaf[leaf].m_FirstElement;
	}

	static unsigned short& FirstLeafInRenderable( unsigned short renderable ) 
	{ 
		return s_ClientLeafSystem.m_Renderables[renderable].m_LeafList;
	}

	static unsigned short& FirstShadowInLeaf( int leaf ) 
	{ 
		return s_ClientLeafSystem.m_Leaf[leaf].m_FirstShadow;
	}

	static unsigned short& FirstLeafInShadow( ClientLeafShadowHandle_t shadow ) 
	{ 
		return s_ClientLeafSystem.m_Shadows[shadow].m_FirstLeaf;
	}

	static unsigned short& FirstShadowOnRenderable( unsigned short renderable ) 
	{ 
		return s_ClientLeafSystem.m_Renderables[renderable].m_FirstShadow;
	}

	static unsigned short& FirstRenderableInShadow( ClientLeafShadowHandle_t shadow ) 
	{ 
		return s_ClientLeafSystem.m_Shadows[shadow].m_FirstRenderable;
	}

	void FrameLock()
	{
		mdlcache->BeginLock();
	}

	void FrameUnlock()
	{
		mdlcache->EndLock();
	}

	// Stores data associated with each leaf.
	CUtlVector< ClientLeaf_t >	m_Leaf;

	// Stores all unique non-detail renderables
	CUtlLinkedList< RenderableInfo_t, ClientRenderHandle_t, false, unsigned int >	m_Renderables;

	// Information associated with shadows registered with the client leaf system
	CUtlLinkedList< ShadowInfo_t, ClientLeafShadowHandle_t, false, unsigned int >	m_Shadows;

	// Maintains the list of all renderables in a particular leaf
	CBidirectionalSet< int, ClientRenderHandle_t, unsigned short, unsigned int >	m_RenderablesInLeaf;

	// Maintains a list of all shadows in a particular leaf 
	CBidirectionalSet< int, ClientLeafShadowHandle_t, unsigned short, unsigned int >	m_ShadowsInLeaf;

	// Maintains a list of all shadows cast on a particular renderable
	CBidirectionalSet< ClientRenderHandle_t, ClientLeafShadowHandle_t, unsigned short, unsigned int >	m_ShadowsOnRenderable;

	// Dirty list of renderables
	CUtlVector< ClientRenderHandle_t >	m_DirtyRenderables;

	// List of renderables in view model render groups
	CUtlVector< ClientRenderHandle_t >	m_ViewModels;

	// Should I draw static props?
	bool m_DrawStaticProps;
	bool m_DrawSmallObjects;
	bool m_bDisableLeafReinsertion;

	// A little enumerator to help us when adding shadows to renderables
	int	m_ShadowEnum;

	// Does anything use alternate sorting?
	int m_nAlternateSortCount;

	// Number of alpha properties out there
	int m_nAlphaPropertyCount;

	CUtlMemoryPool m_AlphaPropertyPool;

	int m_nDebugIndex;
};


//-----------------------------------------------------------------------------
// Methods of IClientAlphaPropertyMgr
//-----------------------------------------------------------------------------
IClientAlphaProperty *CClientLeafSystem::CreateClientAlphaProperty( IClientUnknown *pUnk )
{
	++m_nAlphaPropertyCount;
	CClientAlphaProperty *pProperty = (CClientAlphaProperty*)m_AlphaPropertyPool.Alloc( sizeof(CClientAlphaProperty) );
	Construct( pProperty );
	pProperty->Init( pUnk );
	return pProperty;
}

void CClientLeafSystem::DestroyClientAlphaProperty( IClientAlphaProperty *pAlphaProperty )
{
	if ( !pAlphaProperty )
		return;

	Destruct( static_cast<CClientAlphaProperty*>( pAlphaProperty ) );
	m_AlphaPropertyPool.Free( pAlphaProperty );
	Assert( m_nAlphaPropertyCount > 0 );
	if ( --m_nAlphaPropertyCount == 0 )
	{
		m_AlphaPropertyPool.Clear();
	}
}

	
//-----------------------------------------------------------------------------
// Expose IClientLeafSystem to the client dll.
//-----------------------------------------------------------------------------
CClientLeafSystem CClientLeafSystem::s_ClientLeafSystem;
IClientLeafSystem *g_pClientLeafSystem = &CClientLeafSystem::s_ClientLeafSystem;
IClientAlphaPropertyMgr *g_pClientAlphaPropertyMgr = &CClientLeafSystem::s_ClientLeafSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CClientLeafSystem, IClientLeafSystem, CLIENTLEAFSYSTEM_INTERFACE_VERSION, CClientLeafSystem::s_ClientLeafSystem );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CClientLeafSystem, IClientAlphaPropertyMgr, CLIENT_ALPHA_PROPERTY_MGR_INTERFACE_VERSION, CClientLeafSystem::s_ClientLeafSystem );

void CalcRenderableWorldSpaceAABB_Fast( IClientRenderable *pRenderable, Vector &absMin, Vector &absMax );


//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------
void DefaultRenderBoundsWorldspace( IClientRenderable *pRenderable, Vector &absMins, Vector &absMaxs )
{
	// Tracker 37433:  This fixes a bug where if the stunstick is being wielded by a combine soldier, the fact that the stick was
	//  attached to the soldier's hand would move it such that it would get frustum culled near the edge of the screen.
	C_BaseEntity *pEnt = pRenderable->GetIClientUnknown()->GetBaseEntity();
	if ( pEnt && ( pEnt->IsFollowingEntity() || ( pEnt->GetParentAttachment() > 0 ) ) )
	{
		C_BaseEntity *pParent = pEnt->GetMoveParent();
		if ( pParent )
		{
			// Get the parent's abs space world bounds.
			CalcRenderableWorldSpaceAABB_Fast( pParent, absMins, absMaxs );

			// Add the maximum of our local render bounds. This is making the assumption that we can be at any
			// point and at any angle within the parent's world space bounds.
			Vector vAddMins, vAddMaxs;
			pEnt->GetRenderBounds( vAddMins, vAddMaxs );
			// if our origin is actually farther away than that, expand again
			float radius = pEnt->GetLocalOrigin().Length();

			float flBloatSize = MAX( vAddMins.Length(), vAddMaxs.Length() );
			flBloatSize = MAX(flBloatSize, radius);
			absMins -= Vector( flBloatSize, flBloatSize, flBloatSize );
			absMaxs += Vector( flBloatSize, flBloatSize, flBloatSize );
			return;
		}
	}

	Vector mins, maxs;
	pRenderable->GetRenderBounds( mins, maxs );

	// FIXME: Should I just use a sphere here?
	// Another option is to pass the OBB down the tree; makes for a better fit
	// Generate a world-aligned AABB
	const QAngle& angles = pRenderable->GetRenderAngles();
	if (angles == vec3_angle)
	{
		const Vector& origin = pRenderable->GetRenderOrigin();
		VectorAdd( mins, origin, absMins );
		VectorAdd( maxs, origin, absMaxs );
	}
	else
	{
		TransformAABB( pRenderable->RenderableToWorldTransform(), mins, maxs, absMins, absMaxs );
	}
	Assert( absMins.IsValid() && absMaxs.IsValid() );
}

// Figure out a world space bounding box that encloses the entity's local render bounds in world space.
inline void CalcRenderableWorldSpaceAABB( 
	IClientRenderable *pRenderable, 
	Vector &absMins,
	Vector &absMaxs )
{
	pRenderable->GetRenderBoundsWorldspace( absMins, absMaxs );
}


// This gets an AABB for the renderable, but it doesn't cause a parent's bones to be setup.
// This is used for placement in the leaves, but the more expensive version is used for culling.
void CalcRenderableWorldSpaceAABB_Fast( IClientRenderable *pRenderable, Vector &absMin, Vector &absMax )
{
	C_BaseEntity *pEnt = pRenderable->GetIClientUnknown()->GetBaseEntity();
	if ( pEnt && ( pEnt->IsFollowingEntity() || ( pEnt->GetMoveParent() && ( pEnt->GetParentAttachment() > 0 ) ) ) )
	{
		C_BaseEntity *pParent = pEnt->GetMoveParent();
		Assert( pParent );

		// Get the parent's abs space world bounds.
		CalcRenderableWorldSpaceAABB_Fast( pParent, absMin, absMax );

		// Add the maximum of our local render bounds. This is making the assumption that we can be at any
		// point and at any angle within the parent's world space bounds.
		Vector vAddMins, vAddMaxs;
		pEnt->GetRenderBounds( vAddMins, vAddMaxs );
		// if our origin is actually farther away than that, expand again
		float radius = pEnt->GetLocalOrigin().Length();

		float flBloatSize = MAX( vAddMins.Length(), vAddMaxs.Length() );
		flBloatSize = MAX(flBloatSize, radius);
		absMin -= Vector( flBloatSize, flBloatSize, flBloatSize );
		absMax += Vector( flBloatSize, flBloatSize, flBloatSize );
	}
	else
	{
		// Start out with our own render bounds. Since we don't have a parent, this won't incur any nasty 
		CalcRenderableWorldSpaceAABB( pRenderable, absMin, absMax );
	}
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CClientLeafSystem::CClientLeafSystem() : m_DrawStaticProps(true), m_DrawSmallObjects(true), 
	m_AlphaPropertyPool( sizeof( CClientAlphaProperty ), 1024, CUtlMemoryPool::GROW_SLOW, "CClientAlphaProperty" ) 
{
	// Set up the bi-directional lists...
	m_RenderablesInLeaf.Init( FirstRenderableInLeaf, FirstLeafInRenderable );
	m_ShadowsInLeaf.Init( FirstShadowInLeaf, FirstLeafInShadow ); 
	m_ShadowsOnRenderable.Init( FirstShadowOnRenderable, FirstRenderableInShadow );
	m_nAlternateSortCount = 0;
	m_bDisableLeafReinsertion = false;
}

CClientLeafSystem::~CClientLeafSystem()
{
}

//-----------------------------------------------------------------------------
// Activate, deactivate static props
//-----------------------------------------------------------------------------
void CClientLeafSystem::DrawStaticProps( bool enable )
{
	m_DrawStaticProps = enable;
}

void CClientLeafSystem::DrawSmallEntities( bool enable )
{
	m_DrawSmallObjects = enable;
}

void CClientLeafSystem::DisableLeafReinsertion( bool bDisable )
{
	m_bDisableLeafReinsertion = bDisable;
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
void CClientLeafSystem::LevelInitPreEntity()
{
	MEM_ALLOC_CREDIT();

	m_Renderables.EnsureCapacity( 1024 );
	m_RenderablesInLeaf.EnsureCapacity( 1024 );
	m_ShadowsInLeaf.EnsureCapacity( 256 );
	m_ShadowsOnRenderable.EnsureCapacity( 256 );
	m_DirtyRenderables.EnsureCapacity( 256 );

	// Add all the leaves we'll need
	int leafCount = engine->LevelLeafCount();
	m_Leaf.EnsureCapacity( leafCount );

	ClientLeaf_t newLeaf;
	newLeaf.m_FirstElement = m_RenderablesInLeaf.InvalidIndex();
	newLeaf.m_FirstShadow = m_ShadowsInLeaf.InvalidIndex();
	memset( newLeaf.m_pSubSystemData, 0, sizeof( newLeaf.m_pSubSystemData ) );
	newLeaf.m_FirstDetailProp = 0;
	newLeaf.m_DetailPropCount = 0;
	newLeaf.m_DetailPropRenderFrame = -1;
	while ( --leafCount >= 0 )
	{
		m_Leaf.AddToTail( newLeaf );
	}
}

void CClientLeafSystem::LevelShutdownPreEntity()
{
}

void CClientLeafSystem::LevelShutdownPostEntity()
{
	m_nAlternateSortCount = 0;
	m_ViewModels.Purge();
	m_Renderables.Purge();
	m_RenderablesInLeaf.Purge();
	m_Shadows.Purge();

	// delete subsystem data
	for( int i = 0; i < m_Leaf.Count() ; i++ )
	{
		for( int j = 0 ; j < ARRAYSIZE( m_Leaf[i].m_pSubSystemData ) ; j++ )
		{
			if ( m_Leaf[i].m_pSubSystemData[j] )
			{
				delete m_Leaf[i].m_pSubSystemData[j];
				m_Leaf[i].m_pSubSystemData[j] = NULL;
			}
		}
	}
	m_Leaf.Purge();
	m_ShadowsInLeaf.Purge();
	m_ShadowsOnRenderable.Purge();
	m_DirtyRenderables.Purge();
}


//-----------------------------------------------------------------------------
// Computes a bloated bounding box to reduce insertions into the tree
//-----------------------------------------------------------------------------
#define BBOX_GRANULARITY 32.0f
#define MIN_SHRINK_VOLUME ( 32.0f * 32.0f * 32.0f )

void CClientLeafSystem::CalcRenderableWorldSpaceAABB_Bloated( const RenderableInfo_t &info, Vector &absMin, Vector &absMax )
{
	CalcRenderableWorldSpaceAABB_Fast( info.m_pRenderable, absMin, absMax );

	// Bloat bounds to avoid reinsertion into tree
	absMin.x = floor( absMin.x / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMin.y = floor( absMin.y / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMin.z = floor( absMin.z / BBOX_GRANULARITY ) * BBOX_GRANULARITY;

	absMax.x = ceil( absMax.x / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMax.y = ceil( absMax.y / BBOX_GRANULARITY ) * BBOX_GRANULARITY;
	absMax.z = ceil( absMax.z / BBOX_GRANULARITY ) * BBOX_GRANULARITY;

	// Optimization to make particle systems not re-insert themselves
	if ( info.m_Flags & RENDER_FLAGS_BLOAT_BOUNDS )
	{
		Vector vecTempMin, vecTempMax;
		VectorMin( info.m_vecBloatedAbsMins, absMin, vecTempMin );
		VectorMax( info.m_vecBloatedAbsMaxs, absMax, vecTempMax );
		float flTempVolume = ComputeVolume( vecTempMin, vecTempMax );
		float flCurrVolume = ComputeVolume( absMin, absMax );

		if ( ( flTempVolume <= MIN_SHRINK_VOLUME ) || ( flCurrVolume * 2.0f >= flTempVolume ) )
		{
			absMin = vecTempMin;
			absMax = vecTempMax;
		}
	}
}


//-----------------------------------------------------------------------------
// This is what happens before rendering a particular view
//-----------------------------------------------------------------------------
void CClientLeafSystem::PreRender()
{
//	Assert( m_DirtyRenderables.Count() == 0 );

	// FIXME: This should never need to happen here!
	// At the moment, it's necessary because of the horrid viewmodel/combatweapon
	// confusion in the code where a combat weapon changes its rendering model
	// per view.
	RecomputeRenderableLeaves();
}

// Use this to make sure we're not adding the same renderables to the list while we're going through and re-inserting them into the clientleafsystem
static bool s_bIsInRecomputeRenderableLeaves = false;

void CClientLeafSystem::RecomputeRenderableLeaves()
{
//	MiniProfilerGuard mpGuard(&g_mpRecomputeLeaves);
	int i;
	int nIterations = 0;

	bool bDebugLeafSystem = !IsX360() && cl_leafsystemvis.GetBool();

	Vector absMins, absMaxs;
	while ( m_DirtyRenderables.Count() )
	{
		if ( ++nIterations > 10 )
		{
			Warning( "Too many dirty renderables!\n" );
			break;
		}

		s_bIsInRecomputeRenderableLeaves = true;

		int nDirty = m_DirtyRenderables.Count();
		for ( i = nDirty; --i >= 0; )
		{
			ClientRenderHandle_t handle = m_DirtyRenderables[i];
			RenderableInfo_t &info = m_Renderables[ handle ];

			Assert( info.m_Flags & RENDER_FLAGS_HASCHANGED );

			// See note below
			info.m_Flags &= ~RENDER_FLAGS_HASCHANGED;

			if ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS )
				continue;

			CalcRenderableWorldSpaceAABB_Bloated( info, absMins, absMaxs );
			if ( absMins != info.m_vecBloatedAbsMins || absMaxs != info.m_vecBloatedAbsMaxs )
			{
				// Update position in leaf system
				RemoveFromTree( handle );
				InsertIntoTree( m_DirtyRenderables[i], absMins, absMaxs );
				if ( bDebugLeafSystem )
				{
					debugoverlay->AddBoxOverlay( vec3_origin, absMins, absMaxs, QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
				}
			}
		}

		s_bIsInRecomputeRenderableLeaves = false;

		// NOTE: If we get the following error displayed in the console spew
		//       "Re-entrancy found in CClientLeafSystem::RenderableChanged\n"
		//		 We'll have to reenable this code and remove the line that
		//		 removes the RENDER_FLAGS_HASCHANGED in the loop above.
		/*
		for ( i = nDirty; --i >= 0; )
		{
			// Cache off the area it's sitting in.
			ClientRenderHandle_t handle = m_DirtyRenderables[i];
			RenderableInfo_t& renderable = m_Renderables[ handle ];

			renderable.m_Flags &= ~RENDER_FLAGS_HASCHANGED;
		}
		*/

		m_DirtyRenderables.RemoveMultiple( 0, nDirty );
	}
}


//-----------------------------------------------------------------------------
// Creates a new renderable
//-----------------------------------------------------------------------------
void CClientLeafSystem::CreateRenderableHandle( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled )
{
	Assert( pRenderable );
	Assert( pRenderable->RenderHandle() == INVALID_CLIENT_RENDER_HANDLE );

	ClientRenderHandle_t handle = m_Renderables.AddToTail();
	RenderableInfo_t &info = m_Renderables[handle];

	if ( nModelType == RENDERABLE_MODEL_UNKNOWN_TYPE )
	{
		int nType = modelinfo->GetModelType( pRenderable->GetModel() );
		switch( nType )
		{
		default: nModelType = RENDERABLE_MODEL_ENTITY; break;
		case mod_brush: nModelType = RENDERABLE_MODEL_BRUSH; break;
		case mod_studio: nModelType = RENDERABLE_MODEL_STUDIOMDL; break;
		}
	}

#ifdef _DEBUG
	// We need to know if it's a brush model for shadows
	int modelType = modelinfo->GetModelType( pRenderable->GetModel() );
	switch ( modelType )
	{
	case mod_brush: Assert( nModelType == RENDERABLE_MODEL_BRUSH ); break;
	case mod_studio: Assert( nModelType == RENDERABLE_MODEL_STUDIOMDL || nModelType == RENDERABLE_MODEL_STATIC_PROP ); break;
	case mod_sprite: default: Assert( nModelType == RENDERABLE_MODEL_ENTITY ); break;
	}
#endif

	info.m_pRenderable = pRenderable;
	info.m_pAlphaProperty = static_cast< CClientAlphaProperty* >( pRenderable->GetIClientUnknown()->GetClientAlphaProperty() );
	info.m_FirstShadow = m_ShadowsOnRenderable.InvalidIndex();
	info.m_LeafList = m_RenderablesInLeaf.InvalidIndex();
	info.m_Flags = 0;
	info.m_nRenderFrame = -1;
	info.m_EnumCount = 0;
	info.m_nSplitscreenEnabled = nSplitscreenEnabled & 0x3;
	info.m_nTranslucencyType = nType;
	info.m_nModelType = nModelType;
	info.m_vecBloatedAbsMins.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	info.m_vecBloatedAbsMaxs.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	info.m_vecAbsMins.Init();
	info.m_vecAbsMaxs.Init();

	pRenderable->RenderHandle() = handle;
	RenderWithViewModels( handle, bRenderWithViewModels );
}


//-----------------------------------------------------------------------------
// Call this if the model changes
//-----------------------------------------------------------------------------
void CClientLeafSystem::SetTranslucencyType( ClientRenderHandle_t handle, RenderableTranslucencyType_t nType )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	info.m_nTranslucencyType = nType;
}

RenderableTranslucencyType_t CClientLeafSystem::GetTranslucencyType( ClientRenderHandle_t handle ) const
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return RENDERABLE_IS_OPAQUE;

	const RenderableInfo_t &info = m_Renderables[handle];
	return (RenderableTranslucencyType_t)info.m_nTranslucencyType;
}


void CClientLeafSystem::EnableSplitscreenRendering( ClientRenderHandle_t handle, uint32 nFlags )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	info.m_nSplitscreenEnabled = nFlags & 0x3;
}

void CClientLeafSystem::SetModelType( ClientRenderHandle_t handle, RenderableModelType_t nModelType )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( nModelType == RENDERABLE_MODEL_UNKNOWN_TYPE )
	{
		int nType = modelinfo->GetModelType( info.m_pRenderable->GetModel() );
		switch( nType )
		{
		default: nModelType = RENDERABLE_MODEL_ENTITY; break;
		case mod_brush: nModelType = RENDERABLE_MODEL_BRUSH; break;
		case mod_studio: nModelType = RENDERABLE_MODEL_STUDIOMDL; break;
		}
	}

	if ( info.m_nModelType != nModelType )
	{
		info.m_nModelType = nModelType;
		RenderableChanged( handle );
	}
}

void CClientLeafSystem::EnableRendering( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		info.m_Flags &= ~RENDER_FLAGS_DISABLE_RENDERING;
	}
	else
	{
		info.m_Flags |= RENDER_FLAGS_DISABLE_RENDERING;
	}
}

void CClientLeafSystem::EnableBloatedBounds( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		info.m_Flags |= RENDER_FLAGS_BLOAT_BOUNDS;
	}
	else
	{
		if ( info.m_Flags & RENDER_FLAGS_BLOAT_BOUNDS )
		{
			info.m_Flags &= ~RENDER_FLAGS_BLOAT_BOUNDS;

			// Necessary to generate unbloated bounds later
			RenderableChanged( handle );
		}
	}
}

void CClientLeafSystem::DisableCachedRenderBounds( ClientRenderHandle_t handle, bool bDisable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bDisable )
	{
		info.m_Flags |= RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE;
	}
	else
	{
		info.m_Flags &= ~RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE;
	}
}


//-----------------------------------------------------------------------------
// Use alternate translucent sorting algorithm (draw translucent objects in the furthest leaf they lie in)
//-----------------------------------------------------------------------------
void CClientLeafSystem::EnableAlternateSorting( ClientRenderHandle_t handle, bool bEnable )
{
	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		if ( ( info.m_Flags & RENDER_FLAGS_ALTERNATE_SORTING ) == 0 )
		{
			++m_nAlternateSortCount;
			info.m_Flags |= RENDER_FLAGS_ALTERNATE_SORTING;
		}
	}
	else
	{
		if ( ( info.m_Flags & RENDER_FLAGS_ALTERNATE_SORTING ) != 0 )
		{
			--m_nAlternateSortCount;
			info.m_Flags &= ~RENDER_FLAGS_ALTERNATE_SORTING; 
		}
	}
}


//-----------------------------------------------------------------------------
// Should this render with viewmodels?
//-----------------------------------------------------------------------------
void CClientLeafSystem::RenderWithViewModels( ClientRenderHandle_t handle, bool bEnable )
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	RenderableInfo_t &info = m_Renderables[handle];
	if ( bEnable )
	{
		if ( ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) == 0 )
		{
			info.m_Flags |= RENDER_FLAGS_RENDER_WITH_VIEWMODELS;
			AddToViewModelList( handle );
			RemoveFromTree( handle );
		}
	}
	else
	{
		if ( ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) != 0 )
		{
			info.m_Flags &= ~RENDER_FLAGS_RENDER_WITH_VIEWMODELS; 
			RemoveFromViewModelList( handle );
			RenderableChanged( handle );
		}
	}
}


bool CClientLeafSystem::IsRenderingWithViewModels( ClientRenderHandle_t handle ) const
{
	if ( handle == INVALID_CLIENT_RENDER_HANDLE )
		return false;
	return ( m_Renderables[handle].m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) != 0;
}



//-----------------------------------------------------------------------------
// Add/remove renderable
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderable( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled )
{
	// force a relink we we try to draw it for the first time
	CreateRenderableHandle( pRenderable, bRenderWithViewModels, nType, nModelType, nSplitscreenEnabled );
	ClientRenderHandle_t handle = pRenderable->RenderHandle();
	RenderableChanged( handle );
}

void CClientLeafSystem::RemoveRenderable( ClientRenderHandle_t handle )
{
	// This can happen upon level shutdown
	if (!m_Renderables.IsValidIndex(handle))
		return;

	// Reset the render handle in the entity.
	IClientRenderable *pRenderable = m_Renderables[handle].m_pRenderable;
	Assert( handle == pRenderable->RenderHandle() );
	pRenderable->RenderHandle() = INVALID_CLIENT_RENDER_HANDLE;

	int nFlags = m_Renderables[handle].m_Flags;
	if ( nFlags & RENDER_FLAGS_ALTERNATE_SORTING )
	{
		--m_nAlternateSortCount;
	}

	// Remove the renderable from the dirty list
	if ( nFlags & RENDER_FLAGS_HASCHANGED )
	{
		// NOTE: This isn't particularly fast (linear search),
		// but I'm assuming it's an unusual case where we remove 
		// renderables that are changing or that m_DirtyRenderables usually
		// only has a couple entries
		int i = m_DirtyRenderables.Find( handle );
		Assert( i != m_DirtyRenderables.InvalidIndex() );
		m_DirtyRenderables.FastRemove( i ); 
	}

	if ( IsRenderingWithViewModels( handle ) )
	{
		RemoveFromViewModelList( handle );
	}

	RemoveFromTree( handle );
	m_Renderables.Remove( handle );
}


int CClientLeafSystem::GetRenderableLeaves( ClientRenderHandle_t handle, int leaves[128] )
{
	if ( !m_Renderables.IsValidIndex( handle ) )
		return -1;

	RenderableInfo_t *pRenderable = &m_Renderables[handle];
	if ( pRenderable->m_LeafList == m_RenderablesInLeaf.InvalidIndex() )
		return -1;

	int nLeaves = 0;
	for ( int i=m_RenderablesInLeaf.FirstBucket( handle ); i != m_RenderablesInLeaf.InvalidIndex(); i = m_RenderablesInLeaf.NextBucket( i ) )
	{
		leaves[nLeaves++] = m_RenderablesInLeaf.Bucket( i );
		if ( nLeaves >= 128 )
			break;
	}
	return nLeaves;
}


//-----------------------------------------------------------------------------
// Retrieve leaf handles to leaves a renderable is in
// the pOutLeaf parameter is filled with the leaf the renderable is in.
// If pInIterator is not specified, pOutLeaf is the first leaf in the list.
// if pInIterator is specified, that iterator is used to return the next leaf
// in the list in pOutLeaf.
// the pOutIterator parameter is filled with the iterater which index to the pOutLeaf returned.
//
// Returns false on failure cases where pOutLeaf will be invalid. CHECK THE RETURN!
//-----------------------------------------------------------------------------
bool CClientLeafSystem::GetRenderableLeaf(ClientRenderHandle_t handle, int* pOutLeaf, const int* pInIterator /* = 0 */, int* pOutIterator /* = 0  */)
{
	// bail on invalid handle
	if ( !m_Renderables.IsValidIndex( handle ) )
		return false;

	// bail on no output value pointer
	if ( !pOutLeaf )
		return false;

	// an iterator was specified
	if ( pInIterator )
	{
		int iter = *pInIterator;

		// test for invalid iterator
		if ( iter == m_RenderablesInLeaf.InvalidIndex() )
			return false;

		int iterNext =  m_RenderablesInLeaf.NextBucket( iter );

		// test for end of list
		if ( iterNext == m_RenderablesInLeaf.InvalidIndex() )
			return false;

		// Give the caller the iterator used
		if ( pOutIterator )
		{
			*pOutIterator = iterNext;
		}
		
		// set output value to the next leaf
		*pOutLeaf = m_RenderablesInLeaf.Bucket( iterNext );

	}
	else // no iter param, give them the first bucket in the renderable's list
	{
		int iter = m_RenderablesInLeaf.FirstBucket( handle );

		if ( iter == m_RenderablesInLeaf.InvalidIndex() )
			return false;

		// Set output value to this leaf
		*pOutLeaf = m_RenderablesInLeaf.Bucket( iter );

		// give this iterator to caller
		if ( pOutIterator )
		{
			*pOutIterator = iter;
		}
		
	}
	
	return true;
}

bool CClientLeafSystem::IsRenderableInPVS( IClientRenderable *pRenderable )
{
	ClientRenderHandle_t handle = pRenderable->RenderHandle();
	int leaves[128];
	int nLeaves = GetRenderableLeaves( handle, leaves );
	if ( nLeaves == -1 )
		return false;

	// Ask the engine if this guy is visible.
	return render->AreAnyLeavesVisible( leaves, nLeaves );
}

void CClientLeafSystem::SetSubSystemDataInLeaf( int leaf, int nSubSystemIdx, CClientLeafSubSystemData *pData )
{
	assert( nSubSystemIdx < N_CLSUBSYSTEMS );
	if ( m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx] )
		delete m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx];
	m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx] = pData;
}

CClientLeafSubSystemData *CClientLeafSystem::GetSubSystemDataInLeaf( int leaf, int nSubSystemIdx )
{
	assert( nSubSystemIdx < N_CLSUBSYSTEMS );
	return m_Leaf[leaf].m_pSubSystemData[nSubSystemIdx];
}

//-----------------------------------------------------------------------------
// Indicates which leaves detail objects are in
//-----------------------------------------------------------------------------
void CClientLeafSystem::SetDetailObjectsInLeaf( int leaf, int firstDetailObject,
											    int detailObjectCount )
{
	m_Leaf[leaf].m_FirstDetailProp = firstDetailObject;
	m_Leaf[leaf].m_DetailPropCount = detailObjectCount;
	if ( detailObjectCount )
		engine->SetLeafFlag( leaf, LEAF_FLAGS_CONTAINS_DETAILOBJECTS );	// for fast searches
}

//-----------------------------------------------------------------------------
// Returns the detail objects in a leaf
//-----------------------------------------------------------------------------
void CClientLeafSystem::GetDetailObjectsInLeaf( int leaf, int& firstDetailObject,
											    int& detailObjectCount )
{
	firstDetailObject = m_Leaf[leaf].m_FirstDetailProp;
	detailObjectCount = m_Leaf[leaf].m_DetailPropCount;
}


//-----------------------------------------------------------------------------
// Create/destroy shadows...
//-----------------------------------------------------------------------------
ClientLeafShadowHandle_t CClientLeafSystem::AddShadow( ClientShadowHandle_t userId, unsigned short flags )
{
	ClientLeafShadowHandle_t idx = m_Shadows.AddToTail();
	m_Shadows[idx].m_Shadow = userId;
	m_Shadows[idx].m_FirstLeaf = m_ShadowsInLeaf.InvalidIndex();
	m_Shadows[idx].m_FirstRenderable = m_ShadowsOnRenderable.InvalidIndex();
	m_Shadows[idx].m_EnumCount = 0;
	m_Shadows[idx].m_Flags = flags;
	return idx;
}

void CClientLeafSystem::RemoveShadow( ClientLeafShadowHandle_t handle )
{
	// Remove the shadow from all leaves + renderables...
	RemoveShadowFromLeaves( handle );
	RemoveShadowFromRenderables( handle );

	// Blow away the handle
	m_Shadows.Remove( handle );
}


//-----------------------------------------------------------------------------
// Adds a shadow to a leaf/removes shadow from renderable
//-----------------------------------------------------------------------------
inline bool CClientLeafSystem::ShouldRenderableReceiveShadow( ClientRenderHandle_t renderHandle, int nShadowFlags )
{
	RenderableInfo_t &renderable = m_Renderables[renderHandle];
	if ( renderable.m_nModelType == RENDERABLE_MODEL_ENTITY )
		return false;

	return renderable.m_pRenderable->ShouldReceiveProjectedTextures( nShadowFlags );
}


//-----------------------------------------------------------------------------
// Adds a shadow to a leaf/removes shadow from renderable
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddShadowToRenderable( ClientRenderHandle_t renderHandle, 
										ClientLeafShadowHandle_t shadowHandle )
{
	// Check if this renderable receives the type of projected texture that shadowHandle refers to.
	int nShadowFlags = m_Shadows[shadowHandle].m_Flags;
	if ( !ShouldRenderableReceiveShadow( renderHandle, nShadowFlags ) )
		return;

	m_ShadowsOnRenderable.AddElementToBucket( renderHandle, shadowHandle );

	// Also, do some stuff specific to the particular types of renderables
#if 0
	// If the renderable is a brush model, then add this shadow to it
	IClientRenderable* pRenderable = m_Renderables[renderHandle].m_pRenderable;
	switch( m_Renderables[renderHandle].m_nModelType )
	{
	case RENDERABLE_MODEL_BRUSH:
		g_pClientShadowMgr->AddShadowToReceiver( m_Shadows[shadowHandle].m_Shadow,
			pRenderable, SHADOW_RECEIVER_BRUSH_MODEL );
		break;

	case RENDERABLE_MODEL_STATIC_PROP:
		g_pClientShadowMgr->AddShadowToReceiver( m_Shadows[shadowHandle].m_Shadow,
			pRenderable, SHADOW_RECEIVER_STATIC_PROP );
		break;

	case RENDERABLE_MODEL_STUDIOMDL:
		g_pClientShadowMgr->AddShadowToReceiver( m_Shadows[shadowHandle].m_Shadow,
			pRenderable, SHADOW_RECEIVER_STUDIO_MODEL );
		break;
	}
#else
	// Do AddShadowToReceiver to avoid branching
	static const byte arrRecvType[0x4] = {
		0,
		SHADOW_RECEIVER_STUDIO_MODEL,
		SHADOW_RECEIVER_STATIC_PROP,
		SHADOW_RECEIVER_BRUSH_MODEL
	};
	COMPILE_TIME_ASSERT( RENDERABLE_MODEL_STUDIOMDL == 1 );
	COMPILE_TIME_ASSERT( RENDERABLE_MODEL_STATIC_PROP == 2 );
	COMPILE_TIME_ASSERT( RENDERABLE_MODEL_BRUSH == 3 );

	RenderableInfo_t const &ri = m_Renderables[renderHandle];
	if ( ri.m_nModelType < ARRAYSIZE( arrRecvType ) )
	{
		g_pClientShadowMgr->AddShadowToReceiver(
			m_Shadows[shadowHandle].m_Shadow,
			ri.m_pRenderable,
			( ShadowReceiver_t ) arrRecvType[ ri.m_nModelType ] );
	}
#endif
}

void CClientLeafSystem::RemoveShadowFromRenderables( ClientLeafShadowHandle_t handle )
{
	m_ShadowsOnRenderable.RemoveElement( handle );
}


//-----------------------------------------------------------------------------
// Adds a shadow to a leaf/removes shadow from leaf
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddShadowToLeaf( int leaf, ClientLeafShadowHandle_t shadow, bool bFlashlight )
{
	m_ShadowsInLeaf.AddElementToBucket( leaf, shadow ); 

	if ( !( bFlashlight || r_shadows_on_renderables_enable.GetBool() ) )
	{
		return;
	}

	// Add the shadow exactly once to all renderables in the leaf
	unsigned short i = m_RenderablesInLeaf.FirstElement( leaf );
	while ( i != m_RenderablesInLeaf.InvalidIndex() )
	{
		ClientRenderHandle_t renderable = m_RenderablesInLeaf.Element(i);
		RenderableInfo_t& info = m_Renderables[renderable];

		// Add each shadow exactly once to each renderable
		if (info.m_EnumCount != m_ShadowEnum)
		{
			AddShadowToRenderable( renderable, shadow );
			info.m_EnumCount = m_ShadowEnum;
		}

		i = m_RenderablesInLeaf.NextElement(i);
	}
}

void CClientLeafSystem::RemoveShadowFromLeaves( ClientLeafShadowHandle_t handle )
{
	m_ShadowsInLeaf.RemoveElement( handle );
}


//-----------------------------------------------------------------------------
// Adds a shadow to all leaves listed
//-----------------------------------------------------------------------------
void CClientLeafSystem::ProjectShadow( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList )
{
	// Remove the shadow from any leaves it current exists in
	RemoveShadowFromLeaves( handle );
	RemoveShadowFromRenderables( handle );

	Assert( ( m_Shadows[handle].m_Flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ) == SHADOW_FLAGS_SHADOW );

	// This will help us to avoid adding the shadow multiple times to a renderable
	++m_ShadowEnum;

	for ( int i = 0; i < nLeafCount; ++i )
	{
		AddShadowToLeaf( pLeafList[i], handle, false );
	}
}

void CClientLeafSystem::ProjectFlashlight( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList )
{
	VPROF_BUDGET( "CClientLeafSystem::ProjectFlashlight", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	// Remove the shadow from any leaves it current exists in
	RemoveShadowFromLeaves( handle );
	RemoveShadowFromRenderables( handle );

	Assert( ( m_Shadows[handle].m_Flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ) != 0 );
	
	// This will help us to avoid adding the shadow multiple times to a renderable
	++m_ShadowEnum;

	for ( int i = 0; i < nLeafCount; ++i )
	{
		AddShadowToLeaf( pLeafList[i], handle, true );
	}
}


//-----------------------------------------------------------------------------
// Find all shadow casters in a set of leaves
//-----------------------------------------------------------------------------
void CClientLeafSystem::EnumerateShadowsInLeaves( int leafCount, WorldListLeafData_t* pLeaves, IClientLeafShadowEnum* pEnum )
{
	if (leafCount == 0)
		return;

	// This will help us to avoid enumerating the shadow multiple times
	++m_ShadowEnum;

	for (int i = 0; i < leafCount; ++i)
	{
		int leaf = pLeaves[i].leafIndex;

		unsigned short j = m_ShadowsInLeaf.FirstElement( leaf );
		while ( j != m_ShadowsInLeaf.InvalidIndex() )
		{
			ClientLeafShadowHandle_t shadow = m_ShadowsInLeaf.Element(j);
			ShadowInfo_t& info = m_Shadows[shadow];

			if (info.m_EnumCount != m_ShadowEnum)
			{
				pEnum->EnumShadow(info.m_Shadow);
				info.m_EnumCount = m_ShadowEnum;
			}

			j = m_ShadowsInLeaf.NextElement(j);
		}
	}
}


//-----------------------------------------------------------------------------
// Adds a renderable to a leaf
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderableToLeaf( int leaf, ClientRenderHandle_t renderable, bool bReceiveShadows )
{
#ifdef VALIDATE_CLIENT_LEAF_SYSTEM
	m_RenderablesInLeaf.ValidateAddElementToBucket( leaf, renderable );
#endif
	m_RenderablesInLeaf.AddElementToBucket( leaf, renderable );

	bool bShadowsOnRenderables = r_shadows_on_renderables_enable.GetBool();

	if ( !bReceiveShadows )
	{
		return;
	}

	if ( bShadowsOnRenderables )
	{
		// skipping this code entirely is only safe with single-pass flashlight (i.e. on the 360)

		// Add all shadows in the leaf to the renderable...
		unsigned short i = m_ShadowsInLeaf.FirstElement( leaf );
		while ( i != m_ShadowsInLeaf.InvalidIndex() )
		{
			ClientLeafShadowHandle_t shadow = m_ShadowsInLeaf.Element(i);
			ShadowInfo_t& info = m_Shadows[shadow];

			// Add each shadow exactly once to each renderable
			if ( info.m_EnumCount != m_ShadowEnum )
			{
				AddShadowToRenderable( renderable, shadow );
				info.m_EnumCount = m_ShadowEnum;
			}

			i = m_ShadowsInLeaf.NextElement(i);
		}
	}
	else if ( /*!bShadowsOnRenderables &&*/ IsPC() )
	{
		// for non-singlepass flashlight (i.e. PC) we need to still add all flashlights to the renderable
		
		// Add all flashlights in the leaf to the renderable...
		unsigned short i = m_ShadowsInLeaf.FirstElement( leaf );
		while ( i != m_ShadowsInLeaf.InvalidIndex() )
		{
			ClientLeafShadowHandle_t shadow = m_ShadowsInLeaf.Element(i);
			ShadowInfo_t& info = m_Shadows[shadow];

			// Add each flashlight exactly once to each renderable
			if ( ( info.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) && ( info.m_EnumCount != m_ShadowEnum ) )
			{
				AddShadowToRenderable( renderable, shadow );
				info.m_EnumCount = m_ShadowEnum;
			}

			i = m_ShadowsInLeaf.NextElement(i);
		}
	}
}


//-----------------------------------------------------------------------------
// Adds a renderable to a set of leaves
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves, bool bReceiveShadows )
{ 
	for (int j = 0; j < nLeafCount; ++j)
	{
		AddRenderableToLeaf( pLeaves[j], handle, bReceiveShadows ); 
	}

	m_Renderables[handle].m_Area = engine->GetLeavesArea( pLeaves, nLeafCount );
}

void CClientLeafSystem::AddRenderableToLeaves( ClientRenderHandle_t handle, int nLeafCount, unsigned short *pLeaves )
{ 
	bool bReceiveShadows = ShouldRenderableReceiveShadow( handle, SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );
	AddRenderableToLeaves( handle, nLeafCount, pLeaves, bReceiveShadows );
}


//-----------------------------------------------------------------------------
// Inserts an element into the tree
//-----------------------------------------------------------------------------
bool CClientLeafSystem::EnumerateLeaf( int leaf, int context )
{
	EnumResultList_t *pList = (EnumResultList_t *)context;
	if ( ThreadInMainThread() )
	{
		bool bReceiveShadows = ShouldRenderableReceiveShadow( pList->handle, SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );
		AddRenderableToLeaf( leaf, pList->handle, bReceiveShadows );
	}
	else
	{
		EnumResult_t *p = new EnumResult_t;
		p->leaf = leaf;
		p->pNext = pList->pHead;
		pList->pHead = p;
	}
	return true;
}

void CClientLeafSystem::InsertIntoTree( ClientRenderHandle_t &handle, const Vector &absMins, const Vector &absMaxs )
{
	// NOTE: The render bounds here are relative to the renderable's coordinate system
	RenderableInfo_t &info = m_Renderables[handle];
	Assert( absMins.IsValid() && absMaxs.IsValid() );

	Assert( ( info.m_Flags & RENDER_FLAGS_RENDER_WITH_VIEWMODELS ) == 0 );
	Assert( ThreadInMainThread() );

	info.m_vecBloatedAbsMins = absMins;
	info.m_vecBloatedAbsMaxs = absMaxs;

	// When we insert into the tree, increase the shadow enumerator
	// to make sure each shadow is added exactly once to each renderable
	m_ShadowEnum++;
	unsigned short leafList[1024];
	ISpatialQuery* pQuery = engine->GetBSPTreeQuery();
	int leafCount = pQuery->ListLeavesInBox( absMins, absMaxs, leafList, ARRAYSIZE(leafList) );
	bool bReceiveShadows = ShouldRenderableReceiveShadow( handle, SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( !IsX360() && cl_leafsystemvis.GetBool() )
	{
		char pTemp[256];
		const char *pClassName = "<unknown renderable>";
		C_BaseEntity *pEnt = info.m_pRenderable->GetIClientUnknown()->GetBaseEntity();
		if ( pEnt )
		{
			pClassName = pEnt->GetClassname();
		}
		else
		{
			CNewParticleEffect *pEffect = dynamic_cast< CNewParticleEffect*>( info.m_pRenderable );
			if ( pEffect )
			{
				Q_snprintf( pTemp, sizeof(pTemp), "ps: %s", pEffect->GetName() );
				pClassName = pTemp;
			}
			else if ( dynamic_cast< CParticleEffectBinding* >( info.m_pRenderable ) )
			{
				pClassName = "<old particle system>";
			}
		}

		con_nprint_t np;
		np.time_to_live = 0.1f;
		np.fixed_width_font = true;
		np.color[0] = 1.0;
		np.color[1] = 0.8;
		np.color[2] = 0.1;
		np.index = m_nDebugIndex++;

		engine->Con_NXPrintf( &np, "%s", pClassName );
	}

	AddRenderableToLeaves( handle, leafCount, leafList, bReceiveShadows );
}

//-----------------------------------------------------------------------------
// Removes an element from the tree
//-----------------------------------------------------------------------------
void CClientLeafSystem::RemoveFromTree( ClientRenderHandle_t handle )
{
	m_RenderablesInLeaf.RemoveElement( handle );

	// Remove all shadows cast onto the object
	m_ShadowsOnRenderable.RemoveBucket( handle );

	switch( m_Renderables[handle].m_nModelType )
	{
	case RENDERABLE_MODEL_BRUSH:
		g_pClientShadowMgr->RemoveAllShadowsFromReceiver( 
			m_Renderables[handle].m_pRenderable, SHADOW_RECEIVER_BRUSH_MODEL );
		break;

	case RENDERABLE_MODEL_STATIC_PROP:
		g_pClientShadowMgr->RemoveAllShadowsFromReceiver( 
			m_Renderables[handle].m_pRenderable, SHADOW_RECEIVER_STATIC_PROP );
		break;

	case RENDERABLE_MODEL_STUDIOMDL:
		g_pClientShadowMgr->RemoveAllShadowsFromReceiver( 
			m_Renderables[handle].m_pRenderable, SHADOW_RECEIVER_STUDIO_MODEL );
		break;
	}

}


//-----------------------------------------------------------------------------
// Call this when the renderable moves
//-----------------------------------------------------------------------------
void CClientLeafSystem::RenderableChanged( ClientRenderHandle_t handle )
{
	// This should not be called during view rendering
//	Assert( !m_bDisableLeafReinsertion );

	Assert ( handle != INVALID_CLIENT_RENDER_HANDLE );
	Assert( m_Renderables.IsValidIndex( handle ) );
	if ( !m_Renderables.IsValidIndex( handle ) )
		return;

	RenderableInfo_t &info = m_Renderables[handle]; 
	info.m_Flags &= ~RENDER_FLAGS_BOUNDS_VALID;
	if ( ( info.m_Flags & RENDER_FLAGS_HASCHANGED ) == 0 )
	{
		info.m_Flags |= RENDER_FLAGS_HASCHANGED;
		m_DirtyRenderables.AddToTail( handle );
	}
//#if _DEBUG
	else
	{
		if ( s_bIsInRecomputeRenderableLeaves )
		{
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "Re-entrancy found in CClientLeafSystem::RenderableChanged\n" );
			Warning( "Contact Shanon or Brian\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
			Warning( "------------------------------------------------------------\n" );
		}
		// It had better be in the list
		Assert( m_DirtyRenderables.Find( handle ) != m_DirtyRenderables.InvalidIndex() );
	}
//#endif
}


//-----------------------------------------------------------------------------
// Adds, removes renderables from view model list
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddToViewModelList( ClientRenderHandle_t handle )
{
	MEM_ALLOC_CREDIT();
	Assert( m_ViewModels.Find( handle ) == m_ViewModels.InvalidIndex() );
	m_ViewModels.AddToTail( handle );
}

void CClientLeafSystem::RemoveFromViewModelList( ClientRenderHandle_t handle )
{
	int i = m_ViewModels.Find( handle );
	Assert( i != m_ViewModels.InvalidIndex() );
	m_ViewModels.FastRemove( i );
}


//-----------------------------------------------------------------------------
// Detail system marks 
//-----------------------------------------------------------------------------
void CClientLeafSystem::DrawDetailObjectsInLeaf( int leaf, int nFrameNumber, int& nFirstDetailObject, int& nDetailObjectCount )
{
	ClientLeaf_t &leafInfo = m_Leaf[leaf];
	leafInfo.m_DetailPropRenderFrame = nFrameNumber;
	nFirstDetailObject = leafInfo.m_FirstDetailProp;
	nDetailObjectCount = leafInfo.m_DetailPropCount;
}


//-----------------------------------------------------------------------------
// Are we close enough to this leaf to draw detail props *and* are there any props in the leaf?
//-----------------------------------------------------------------------------
bool CClientLeafSystem::ShouldDrawDetailObjectsInLeaf( int leaf, int frameNumber )
{
	ClientLeaf_t &leafInfo = m_Leaf[leaf];
	return ( (leafInfo.m_DetailPropRenderFrame == frameNumber ) &&
			 ( ( leafInfo.m_DetailPropCount != 0 ) || ( leafInfo.m_pSubSystemData[CLSUBSYSTEM_DETAILOBJECTS] ) ) );
}


//-----------------------------------------------------------------------------
// Compute which leaf the translucent renderables should render in
//-----------------------------------------------------------------------------
#define LeafToMarker( leaf ) reinterpret_cast<RenderableInfo_t *>(( (leaf) << 1 ) | 1)
#define IsLeafMarker( p ) (bool)((reinterpret_cast<size_t>(p)) & 1)
#define MarkerToLeaf( p ) (int)((reinterpret_cast<size_t>(p)) >> 1)


//-----------------------------------------------------------------------------
// Adds a renderable to the list of renderables to render this frame
//-----------------------------------------------------------------------------
inline void AddRenderableToRenderList( CClientRenderablesList &renderList, IClientRenderable *pRenderable, 
	int iLeaf, RenderGroup_t group,	int nModelType, uint8 nAlphaModulation, bool bTwoPass = false )
{
#ifdef _DEBUG
	if (cl_drawleaf.GetInt() >= 0)
	{
		if (iLeaf != cl_drawleaf.GetInt())
			return;
	}
#endif

	Assert( group >= 0 && group < RENDER_GROUP_COUNT );
	
	int &curCount = renderList.m_RenderGroupCounts[group];
	if ( curCount < CClientRenderablesList::MAX_GROUP_ENTITIES )
	{
		Assert( (iLeaf >= 0) && (iLeaf <= 65535) );

		CClientRenderablesList::CEntry *pEntry = &renderList.m_RenderGroups[group][curCount];
		pEntry->m_pRenderable = pRenderable;
		pEntry->m_iWorldListInfoLeaf = iLeaf;
		pEntry->m_nModelType = nModelType;
		pEntry->m_TwoPass = bTwoPass;
		pEntry->m_InstanceData.m_nAlpha = nAlphaModulation;
		curCount++;
	}
	else
	{
		engine->Con_NPrintf( 10, "Warning: overflowed CClientRenderablesList group %d", group );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : renderList - 
//			renderGroup - 
//-----------------------------------------------------------------------------
void CClientLeafSystem::CollateViewModelRenderables( CViewModelRenderablesList *pList )
{
	CViewModelRenderablesList::RenderGroups_t &opaqueList = pList->m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	CViewModelRenderablesList::RenderGroups_t &translucentList = pList->m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_TRANSLUCENT ];

	for ( int i = m_ViewModels.Count()-1; i >= 0; --i )
	{
		ClientRenderHandle_t handle = m_ViewModels[i];
		RenderableInfo_t& renderable = m_Renderables[handle];

		int nAlpha = renderable.m_pAlphaProperty ? renderable.m_pAlphaProperty->ComputeRenderAlpha( ) : 255;
		bool bIsTransparent = ( nAlpha != 255 ) || ( renderable.m_nTranslucencyType != RENDERABLE_IS_OPAQUE );

		// That's why we need to test RENDER_GROUP_OPAQUE_ENTITY - it may have changed in ComputeFXBlend()
		if ( !bIsTransparent )
		{
			int i = opaqueList.AddToTail();
			CViewModelRenderablesList::CEntry *pEntry = &opaqueList[i];
			pEntry->m_pRenderable = renderable.m_pRenderable;
			pEntry->m_InstanceData.m_nAlpha = 255;
		}
		else
		{
			int i = translucentList.AddToTail();
			CViewModelRenderablesList::CEntry *pEntry = &translucentList[i];
			pEntry->m_pRenderable = renderable.m_pRenderable;
			pEntry->m_InstanceData.m_nAlpha = nAlpha;

			if ( renderable.m_nTranslucencyType == RENDERABLE_IS_TWO_PASS )
			{
				int i = opaqueList.AddToTail();
				CViewModelRenderablesList::CEntry *pEntry = &opaqueList[i];
				pEntry->m_pRenderable = renderable.m_pRenderable;
				pEntry->m_InstanceData.m_nAlpha = 255;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sort entities in a back-to-front ordering
//-----------------------------------------------------------------------------
void CClientLeafSystem::SortEntities( const Vector &vecRenderOrigin, const Vector &vecRenderForward, CClientRenderablesList::CEntry *pEntities, int nEntities )
{
	// Don't sort if we only have 1 entity
	if ( nEntities <= 1 )
		return;

	float dists[CClientRenderablesList::MAX_GROUP_ENTITIES];

	// First get a distance for each entity.
	int i;
	for( i=0; i < nEntities; i++ )
	{
		IClientRenderable *pRenderable = pEntities[i].m_pRenderable;

		// Compute the center of the object (needed for translucent brush models)
		Vector boxcenter;
		Vector mins,maxs;
		pRenderable->GetRenderBounds( mins, maxs );
		VectorAdd( mins, maxs, boxcenter );
		VectorMA( pRenderable->GetRenderOrigin(), 0.5f, boxcenter, boxcenter );

		// Compute distance...
		Vector delta;
		VectorSubtract( boxcenter, vecRenderOrigin, delta );
		dists[i] = DotProduct( delta, vecRenderForward );
	}

	// H-sort.
	int stepSize = 4;
	while( stepSize )
	{
		int end = nEntities - stepSize;
		for( i=0; i < end; i += stepSize )
		{
			if( dists[i] > dists[i+stepSize] )
			{
				V_swap( pEntities[i], pEntities[i+stepSize] );
				V_swap( dists[i], dists[i+stepSize] );

				if( i == 0 )
				{
					i = -stepSize;
				}
				else
				{
					i -= stepSize << 1;
				}
			}
		}

		stepSize >>= 1;
	}
}


//-----------------------------------------------------------------------------
// Extracts static props from the list of renderables
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractStaticProps( int nCount, RenderableInfo_t **ppRenderables )
{
	if ( m_DrawStaticProps )
		return nCount;

	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			// Early out on static props if we don't want to render them
			if ( pInfo->m_nModelType == RENDERABLE_MODEL_STATIC_PROP )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}
	return nUniqueCount;
}


//-----------------------------------------------------------------------------
// Extracts renderables that are excluded in splitscreen
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractSplitscreenRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	if ( !IsSplitScreenSupported() )
		return nCount;

	if ( !engine->IsSplitScreenActive() )
		return nCount;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlotMask = 1 << GET_ACTIVE_SPLITSCREEN_SLOT();

	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			// Early out on splitscreen renderables if we don't want to render them
			if ( ( pInfo->m_nSplitscreenEnabled & nSlotMask ) == 0 )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}
	return nUniqueCount;
}


//-----------------------------------------------------------------------------
// Extracts duplicates
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractDuplicates( int nFrameNumber, int nCount, RenderableInfo_t **ppRenderables )
{
	// NOTE: We don't know whether these renderables are translucent or not
	// but we do know if they participate in alternate sorting, which is all we need.
	int nUniqueCount = 0;
	int nLeaf = 0;

	// For better sorting, we're gonna choose the leaf that is closest to the camera.
	// The leaf list passed in here is sorted front to back

	// FIXME: This algorithm won't work in a threaded context since it stores state in renderableinfo_t
	if ( m_nAlternateSortCount == 0 )
	{
		// I expect this is the typical case; nothing needs alternate sorting
		for ( int i = 0; i < nCount; ++i )
		{
			RenderableInfo_t *pInfo = ppRenderables[i];
			if ( !IsLeafMarker( pInfo ) )
			{
				// Skip these bad boys altogether
				if ( pInfo->m_Flags & ( RENDER_FLAGS_RENDER_WITH_VIEWMODELS | RENDER_FLAGS_DISABLE_RENDERING ) )
					continue;

				// If we've seen this already, then we don't need to add it 
				if ( pInfo->m_nRenderFrame == nFrameNumber )
					continue;

				pInfo->m_nRenderFrame = nFrameNumber;
			}
			ppRenderables[nUniqueCount++] = pInfo;
		}
		return nUniqueCount;
	}

	// Here, we have to worry about alternate sorting. I'm not sure if I
	// can do better than 2n unless I cache off counts of each renderable 
	// in the first loop in BuildRenderablesListV2. I'm doing it this way
	// because I don't believe we'll ever use this path.
	int nAlternateSortCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( !IsLeafMarker( pInfo ) )
		{
			// If we've seen this already, then we don't need to add it 
			if ( ( pInfo->m_Flags & RENDER_FLAGS_ALTERNATE_SORTING ) == 0 )
			{
				if( pInfo->m_nRenderFrame == nFrameNumber )
					continue;
				pInfo->m_nRenderFrame = nFrameNumber;
			}
			else
			{
				// A little convoluted, but I don't want to store any unnecessary state
				// Basically, the render frame will == frame number + duplication count by the end
				// NOTE: This will produce a problem for a few frames every 4 billion frames when wraparound happens
				// tough noogies
				++nAlternateSortCount;
				if( pInfo->m_nRenderFrame < nFrameNumber )
					pInfo->m_nRenderFrame = nFrameNumber + 1;
				else
					++pInfo->m_nRenderFrame;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}

	if ( nAlternateSortCount )
	{
		// Extract out the renderables which use alternate sorting
		nCount = nUniqueCount;
		nUniqueCount = 0;
		nLeaf = 0;
		for ( int i = 0; i < nCount; ++i )
		{
			RenderableInfo_t *pInfo = ppRenderables[i];
			if ( !IsLeafMarker( pInfo ) )
			{
				if ( pInfo->m_Flags & RENDER_FLAGS_ALTERNATE_SORTING )
				{
					// Add in the last one we encountered
					if( --pInfo->m_nRenderFrame != nFrameNumber )
						continue;
				}
			}

			ppRenderables[nUniqueCount++] = pInfo;
		}
	}

	return nUniqueCount;
}


//-----------------------------------------------------------------------------
// Extracts static props from the list of renderables
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractTranslucentRenderables( int nCount, RenderableInfo_t **ppRenderables )
{
	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];

		if ( !IsLeafMarker( pInfo ) )
		{
			if ( pInfo->m_nTranslucencyType == RENDERABLE_IS_TRANSLUCENT )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		ppRenderables[nUniqueCount++] = pInfo;
	}
	return nUniqueCount;
}


//-----------------------------------------------------------------------------
// Computes translucency for all renderables
//-----------------------------------------------------------------------------
void CClientLeafSystem::ComputeDistanceFade( int nCount, AlphaInfo_t *pAlphaInfo, BuildRenderListInfo_t *pRLInfo )
{
	// Distance fade computations
	float flDistFactorSq = 1.0f;
	Vector vecViewOrigin = CurrentViewOrigin();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pLocal )
	{
		flDistFactorSq = pLocal->GetFOVDistanceAdjustFactor();
		flDistFactorSq *= flDistFactorSq;
	}

	for ( int i = 0; i < nCount; ++i )
	{
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( !pAlphaProp )
			continue;

		// Distance fade is inactive in this case
		if ( pAlphaProp->m_nDistFadeEnd == 0 )
			continue;

		float flCurrentDistanceSq;
		if ( pAlphaProp->m_nDistanceFadeMode == CLIENT_ALPHA_DISTANCE_FADE_USE_CENTER )
		{
			flCurrentDistanceSq = flDistFactorSq * vecViewOrigin.DistToSqr( pAlphaInfo[i].m_vecCenter );
		}
		else
		{
			flCurrentDistanceSq = flDistFactorSq * CalcSqrDistanceToAABB( pRLInfo[i].m_vecMins, pRLInfo[i].m_vecMaxs, vecViewOrigin );
		}

		float flDistFadeStartSq = pAlphaProp->m_nDistFadeStart;
		flDistFadeStartSq *= flDistFadeStartSq;
		if ( flCurrentDistanceSq <= flDistFadeStartSq )
			continue;

		float flDistFadeEndSq = pAlphaProp->m_nDistFadeEnd;
		flDistFadeEndSq *= flDistFadeEndSq;
		if ( flCurrentDistanceSq >= flDistFadeEndSq )
		{
			pAlphaInfo[i].m_flFadeFactor = 0.0f;
			continue;
		}

		// NOTE: Because of the if-checks above, flDistFadeEndSq != flDistFadeStartSq here
		pAlphaInfo[i].m_flFadeFactor = ( flDistFadeEndSq - flCurrentDistanceSq ) / ( flDistFadeEndSq - flDistFadeStartSq );
	}
}

float ComputeScreenSize( const Vector &vecOrigin, float flRadius, const ScreenSizeComputeInfo_t& info )
{
	// This is sort of faked, but it's faster that way
	// FIXME: Also, there's a much faster way to do this with similar triangles
	// but I want to make sure it exactly matches the current matrices, so
	// for now, I do it this conservative way
	/*
	Vector4D testPoint1, testPoint2;
	VectorMA( vecOrigin, flRadius, info.m_vecViewUp, testPoint1.AsVector3D() );
	VectorMA( vecOrigin, -flRadius, info.m_vecViewUp, testPoint2.AsVector3D() );
	testPoint1.w = testPoint2.w = 1.0f;

	Vector4D clipPos1, clipPos2;
	Vector4DMultiply( info.m_matViewProj, testPoint1, clipPos1 );
	Vector4DMultiply( info.m_matViewProj, testPoint2, clipPos2 );
	if (clipPos1.w >= 0.001f)
	{
		clipPos1.y /= clipPos1.w;
	}
	else
	{
		clipPos1.y *= 1000;
	}
	if (clipPos2.w >= 0.001f)
	{
		clipPos2.y /= clipPos2.w;
	}
	else
	{
		clipPos2.y *= 1000;
	}

	// The divide-by-two here is because y goes from -1 to 1 in projection space
	return info.m_nViewportHeight * fabs( clipPos2.y - clipPos1.y ) * 0.5f;
	*/

	// NOTE: Optimized version of the above algorithm, which only uses y and w components of clip
	// Can also optimize based on clipPos = a +/- b * r
	const float *pViewProjY	= info.m_matViewProj[1];
	const float *pViewProjW	= info.m_matViewProj[3];
	float flODotY		= pViewProjY[0] * vecOrigin.x			+ pViewProjY[1] * vecOrigin.y			+ pViewProjY[2] * vecOrigin.z			+ pViewProjY[3];
	float flViewDotY	= pViewProjY[0] * info.m_vecViewUp.x	+ pViewProjY[1] * info.m_vecViewUp.y	+ pViewProjY[2] * info.m_vecViewUp.z;  
	flViewDotY			*= flRadius;
	float flODotW		= pViewProjW[0] * vecOrigin.x			+ pViewProjW[1] * vecOrigin.y			+ pViewProjW[2] * vecOrigin.z			+ pViewProjW[3];
	float flViewDotW	= pViewProjW[0] * info.m_vecViewUp.x	+ pViewProjW[1] * info.m_vecViewUp.y	+ pViewProjW[2] * info.m_vecViewUp.z;
	flViewDotW			*= flRadius;
	float y0			= flODotY + flViewDotY;
	float w0			= flODotW + flViewDotW;
	y0					*= ( w0 >= 0.001f ) ? ( 1.0f / w0 ) : 1000.0f;
	float y1			= flODotY - flViewDotY;
	float w1			= flODotW - flViewDotW;
	y1					*= ( w1 >= 0.001f ) ? ( 1.0f / w1 ) : 1000.0f;

	// The divide-by-two here is because y goes from -1 to 1 in projection space
	return info.m_nViewportHeight * fabs( y1 - y0 ) * 0.5f;
}

void ComputeScreenSizeInfo( ScreenSizeComputeInfo_t *pInfo )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	VMatrix viewMatrix, projectionMatrix;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projectionMatrix );
	MatrixMultiply( projectionMatrix, viewMatrix, pInfo->m_matViewProj );

	int x, y, w, h;
	pRenderContext->GetViewport( x, y, w, h );
	pInfo->m_nViewportHeight = h;

	pRenderContext->GetWorldSpaceCameraVectors( NULL, NULL, &pInfo->m_vecViewUp );
}

void CClientLeafSystem::ComputeScreenFade( const ScreenSizeComputeInfo_t &info, float flMinScreenWidth, float flMaxScreenWidth, int nCount, AlphaInfo_t *pAlphaInfo )
{	
	if ( flMaxScreenWidth <= flMinScreenWidth )
	{
		flMaxScreenWidth = flMinScreenWidth;
	}
	if ( flMinScreenWidth <= 0 ) 
		return;

	float flFalloffFactor;
	if ( flMaxScreenWidth != flMinScreenWidth )
	{
		flFalloffFactor = 1.0f / ( flMaxScreenWidth - flMinScreenWidth );
	}
	else
	{
		flFalloffFactor = 1.0f;
	}
									    
	for ( int i = 0; i < nCount; ++i )
	{
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( !pAlphaProp )
			continue;

		// Fade is inactive in this case
		if ( pAlphaProp->m_flFadeScale <= 0.0f )
			continue;

		float flPixelWidth = ComputeScreenSize( pAlphaInfo[i].m_vecCenter, pAlphaInfo[i].m_flRadius, info ) / pAlphaProp->m_flFadeScale;

		// NOTE: This is to account for an error in the original screen computations years ago
		flPixelWidth *= 2.0f;

		float flAlpha = 0.0f;
		if ( flPixelWidth > flMinScreenWidth )
		{
			if ( ( flMaxScreenWidth >= 0) && ( flPixelWidth < flMaxScreenWidth ) )
			{
				flAlpha = flFalloffFactor * (flPixelWidth - flMinScreenWidth );
			}
			else
			{
				flAlpha = 1.0f;
			}
		}

		pAlphaInfo[i].m_flFadeFactor = MIN( pAlphaInfo[i].m_flFadeFactor, flAlpha );
	}
}

extern ConVar cl_leveloverview;
#ifdef _DEBUG
extern ConVar r_FadeProps;
#endif

int CClientLeafSystem::ComputeTranslucency( int nFrameNumber, int nViewID, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo )
{
	AlphaInfo_t *pAlphaInfo = (AlphaInfo_t*)stackalloc( nCount * sizeof(AlphaInfo_t) );
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( IsLeafMarker( pInfo ) )
		{
			pAlphaInfo[i].m_pAlphaProperty = NULL;
			continue;
		}

		Vector vecCenter;
		VectorAdd( pRLInfo[i].m_vecMaxs, pRLInfo[i].m_vecMins, vecCenter );
		vecCenter *= 0.5f;
		pAlphaInfo[i].m_vecCenter = vecCenter;
		pAlphaInfo[i].m_flRadius = vecCenter.DistTo( pRLInfo[i].m_vecMaxs );
		pAlphaInfo[i].m_pAlphaProperty = pInfo->m_pAlphaProperty;
		pAlphaInfo[i].m_flFadeFactor = 1.0f;
	}

	for ( int i = 0; i < nCount; ++i )
	{
		// FIXME: Computing the base alpha could potentially be sorted by renderfx type
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( pAlphaProp )
		{
			pRLInfo[i].m_nAlpha = pAlphaProp->ComputeRenderAlpha( );
			pRLInfo[i].m_bIgnoreZBuffer = pAlphaProp->IgnoresZBuffer();
		}
		else
		{
			pRLInfo[i].m_nAlpha = 255;
			pRLInfo[i].m_bIgnoreZBuffer = false;
		}
	}

	// If we're taking devshots, don't fade props at all
	bool bFadeProps = true;
#ifdef _DEBUG
	bFadeProps = r_FadeProps.GetBool();
#endif

	if ( nViewID == VIEW_3DSKY )
	{
		bFadeProps = false;
	}

	if ( !g_MakingDevShots && !cl_leveloverview.GetInt() && bFadeProps )
	{
		ComputeDistanceFade( nCount, pAlphaInfo, pRLInfo );

		ScreenSizeComputeInfo_t info;

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		VMatrix viewMatrix, projectionMatrix;
		pRenderContext->GetMatrix( MATERIAL_VIEW, &viewMatrix );
		pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projectionMatrix );
		MatrixMultiply( projectionMatrix, viewMatrix, info.m_matViewProj );

		int x, y, w, h;
		pRenderContext->GetViewport( x, y, w, h );
		info.m_nViewportHeight = h;

		pRenderContext->GetWorldSpaceCameraVectors( NULL, NULL, &info.m_vecViewUp );

		if ( GetViewRenderInstance()->AllowScreenspaceFade() )
		{
			float flMinLevelFadeArea, flMaxLevelFadeArea;
			modelinfo->GetLevelScreenFadeRange( &flMinLevelFadeArea, &flMaxLevelFadeArea );
			ComputeScreenFade( info, flMinLevelFadeArea, flMaxLevelFadeArea, nCount, pAlphaInfo );

			float flMinViewFadeArea, flMaxViewFadeArea;
			view->GetScreenFadeDistances( &flMinViewFadeArea, &flMaxViewFadeArea );
			ComputeScreenFade( info, flMinViewFadeArea, flMaxViewFadeArea, nCount, pAlphaInfo );
		}

		for ( int i = 0; i < nCount; ++i )
		{
			if ( !pAlphaInfo[i].m_pAlphaProperty )
				continue;

			float flAlpha = pRLInfo[i].m_nAlpha * pAlphaInfo[i].m_flFadeFactor;
			int nAlpha = (int)flAlpha;
			pRLInfo[i].m_nAlpha = clamp( nAlpha, 0, 255 );
		}
	}

	// Update shadows
	for ( int i = 0; i < nCount; ++i )
	{
		CClientAlphaProperty *pAlphaProp = pAlphaInfo[i].m_pAlphaProperty;
		if ( !pAlphaProp || ( pAlphaInfo[i].m_pAlphaProperty->m_hShadowHandle == CLIENTSHADOW_INVALID_HANDLE ) )
			continue;

		int nAlpha = pRLInfo[i].m_nAlpha;
		if ( pAlphaProp->m_bShadowAlphaOverride )
		{
			nAlpha = pAlphaProp->m_pOuter->GetClientRenderable()->OverrideShadowAlphaModulation( nAlpha );
			nAlpha = clamp( nAlpha, 0, 255 );
		}
		g_pClientShadowMgr->SetFalloffBias( pAlphaInfo[i].m_pAlphaProperty->m_hShadowHandle, (255 - nAlpha) );
	}

	// Strip invisible ones out
	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !IsLeafMarker( ppRenderables[i] ) && ( !pRLInfo[i].m_nAlpha ) )
		{
			// Necessary for dependent models to be grabbed
			ppRenderables[i]->m_nRenderFrame--;
			continue;
		}

		ppRenderables[nUniqueCount] = ppRenderables[i];
		pRLInfo[nUniqueCount] = pRLInfo[i];
		++nUniqueCount;
	}
	return nUniqueCount;
}


//-----------------------------------------------------------------------------
// Computes bounds for all renderables
//-----------------------------------------------------------------------------
void CClientLeafSystem::ComputeBounds( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo )
{
//	MiniProfilerGuard mpGuard(&g_mpComputeBounds);

	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( IsLeafMarker( pInfo ) )
			continue;

		// UNDONE: Investigate speed tradeoffs of occlusion culling brush models too?
		pRLInfo[i].m_bPerformOcclusionTest = ( pInfo->m_nModelType == RENDERABLE_MODEL_STATIC_PROP || pInfo->m_nModelType == RENDERABLE_MODEL_STUDIOMDL ); 
		pRLInfo[i].m_nArea = pInfo->m_Area;
		pRLInfo[i].m_nAlpha = 255;	// necessary to set for shadow depth rendering

		// NOTE: This is inherently not threadsafe!!
		if ( ( pInfo->m_Flags & RENDER_FLAGS_BOUNDS_VALID ) == 0 )
		{
			CalcRenderableWorldSpaceAABB( pInfo->m_pRenderable, pInfo->m_vecAbsMins, pInfo->m_vecAbsMaxs );
			if ( ( pInfo->m_Flags & RENDER_FLAGS_BOUNDS_ALWAYS_RECOMPUTE ) == 0 )
			{
				C_BaseEntity *pEnt = pInfo->m_pRenderable->GetIClientUnknown()->GetBaseEntity();
				if ( !pEnt || !pEnt->GetMoveParent() )
				{
					pInfo->m_Flags |= RENDER_FLAGS_BOUNDS_VALID;
				}
			}
		}
#ifdef _DEBUG
		else
		{
			// If these assertions trigger, it means there's some state that GetRenderBounds
			// depends on which, on change, doesn't call ClientLeafSystem::RenderableChanged().
			Vector vecTestMins, vecTestMaxs;
			CalcRenderableWorldSpaceAABB( pInfo->m_pRenderable, vecTestMins, vecTestMaxs );
			Assert( VectorsAreEqual( vecTestMins, pInfo->m_vecAbsMins, 1e-3 ) );
			Assert( VectorsAreEqual( vecTestMaxs, pInfo->m_vecAbsMaxs, 1e-3 ) );
		}
#endif

		pRLInfo[i].m_vecMins = pInfo->m_vecAbsMins;
		pRLInfo[i].m_vecMaxs = pInfo->m_vecAbsMaxs;
	}
}


//-----------------------------------------------------------------------------
// Culls renderables based on view frustum + areaportals
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractCulledRenderables( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo )
{
	bool bPortalTestEnts = r_PortalTestEnts.GetBool() && !r_portalsopenall.GetBool();

	// FIXME: sort by area and inline cull. Should make it a bunch faster
	int nUniqueCount = 0;
	if ( bPortalTestEnts )
	{
		Frustum_t *list[MAX_MAP_AREAS];
		engine->GetFrustumList( list, ARRAYSIZE(list) );
		for ( int i = 0; i < nCount; ++i )
		{
			RenderableInfo_t *pInfo = ppRenderables[i];
			BuildRenderListInfo_t &rlInfo = pRLInfo[i];
			if ( !IsLeafMarker( pInfo ) )
			{
				int frustumIndex = rlInfo.m_nArea + 1;
				if ( list[frustumIndex]->CullBox( rlInfo.m_vecMins, rlInfo.m_vecMaxs ) )
				{
					// Necessary for dependent models to be grabbed
					pInfo->m_nRenderFrame--;
					continue;
				}
			}
			pRLInfo[nUniqueCount] = rlInfo;
			ppRenderables[nUniqueCount] = pInfo;
			++nUniqueCount;
		}
		return nUniqueCount;
	}

	// Debug mode, doesn't need to be fast
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		BuildRenderListInfo_t &rlInfo = pRLInfo[i];
		if ( !IsLeafMarker( pInfo ) )
		{
			// cull with main frustum
			if ( engine->CullBox( rlInfo.m_vecMins, rlInfo.m_vecMaxs ) )
			{
				// Necessary for dependent models to be grabbed
				pInfo->m_nRenderFrame--;
				continue;
			}
		}
		pRLInfo[nUniqueCount] = rlInfo;
		ppRenderables[nUniqueCount] = pInfo;
		++nUniqueCount;
	}
	return nUniqueCount;
}

//-----------------------------------------------------------------------------
// Culls renderables based on occlusion
//-----------------------------------------------------------------------------
int CClientLeafSystem::ExtractOccludedRenderables( int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo )
{
	static ConVarRef r_occlusion("r_occlusion");

	// occlusion is off, just return
	if ( !r_occlusion.GetBool() )
		return nCount;

	int nUniqueCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		BuildRenderListInfo_t &rlInfo = pRLInfo[i];
		if ( !IsLeafMarker( pInfo ) )
		{
			if ( rlInfo.m_bPerformOcclusionTest )
			{
				// test to see if this renderable is occluded by the engine's occlusion system
				if ( engine->IsOccluded( rlInfo.m_vecMins, rlInfo.m_vecMaxs ) )
				{
					// Necessary for dependent models to be grabbed
					pInfo->m_nRenderFrame--;
					continue;
				}
			}
		}
		pRLInfo[nUniqueCount] = rlInfo;
		ppRenderables[nUniqueCount] = pInfo;
		++nUniqueCount;
	}

	return nUniqueCount;
}


//-----------------------------------------------------------------------------
// Adds renderables into their final lists
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddDependentRenderables( const SetupRenderInfo_t &info )
{
	// NOTE: This turns out to have non-zero cost.
	// Remove early out if we actually end up needing to use this
	return;

	CClientRenderablesList *pRenderList = info.m_pRenderList;
	pRenderList->m_nBoneSetupDependencyCount = 0;
	for ( int i = 0; i < RENDER_GROUP_COUNT; ++i )
	{
		int nCount = pRenderList->m_RenderGroupCounts[i];
		for ( int j = 0; j < nCount; ++j )
		{
			IClientRenderable *pRenderable = pRenderList->m_RenderGroups[i][j].m_pRenderable;
			C_BaseEntity *pEnt = pRenderable->GetIClientUnknown()->GetBaseEntity();
			if ( !pEnt )
				continue;

			while ( pEnt->IsFollowingEntity() || ( pEnt->GetMoveParent() && pEnt->GetParentAttachment() > 0 ) )
			{
				pEnt = pEnt->GetMoveParent();
				ClientRenderHandle_t hParent = pEnt->GetRenderHandle();
				Assert( hParent != INVALID_CLIENT_RENDER_HANDLE );
				if ( hParent == INVALID_CLIENT_RENDER_HANDLE )
					continue;
				RenderableInfo_t &parentInfo = m_Renderables[hParent];
				if ( parentInfo.m_nRenderFrame != info.m_nRenderFrame )
				{
					parentInfo.m_nRenderFrame = info.m_nRenderFrame;
					pRenderList->m_pBoneSetupDependency[ pRenderList->m_nBoneSetupDependencyCount++ ] = pEnt->GetClientRenderable();
				}
			}
		}
	}
}



//-----------------------------------------------------------------------------
// Adds renderables into their final lists
//-----------------------------------------------------------------------------
void CClientLeafSystem::AddRenderablesToRenderLists( const SetupRenderInfo_t &info, int nCount, RenderableInfo_t **ppRenderables, BuildRenderListInfo_t *pRLInfo, int nDetailCount, DetailRenderableInfo_t *pDetailInfo )
{
	CClientRenderablesList::CEntry *pTranslucentEntries = info.m_pRenderList->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
	int &nTranslucentEntries = info.m_pRenderList->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT];
	int nTranslucent = 0;
	int nCurDetail = 0;
	int nWorldListLeafIndex = -1;
	for ( int i = 0; i < nCount; ++i )
	{
		RenderableInfo_t *pInfo = ppRenderables[i];
		if ( IsLeafMarker( pInfo ) )
		{
			// Add detail props for this leaf
			for( ; nCurDetail < nDetailCount; ++nCurDetail )
			{
				DetailRenderableInfo_t &detailInfo = pDetailInfo[nCurDetail];
				if ( detailInfo.m_nLeafIndex > nWorldListLeafIndex )
					break;
				Assert( detailInfo.m_nLeafIndex == nWorldListLeafIndex );
				AddRenderableToRenderList( *info.m_pRenderList, detailInfo.m_pRenderable, 
					nWorldListLeafIndex, detailInfo.m_nRenderGroup, RENDERABLE_MODEL_ENTITY, detailInfo.m_InstanceData.m_nAlpha );
			}

			int nNewTranslucent = nTranslucentEntries - nTranslucent;
			if( ( nNewTranslucent != 0 ) && info.m_bDrawTranslucentObjects )
			{
				// Sort the new translucent entities.
				SortEntities( info.m_vecRenderOrigin, info.m_vecRenderForward, &pTranslucentEntries[nTranslucent], nNewTranslucent );
			}
			nTranslucent = nTranslucentEntries;
			nWorldListLeafIndex++;
			continue;
		}

		bool bIsTranslucent = ( pRLInfo[i].m_nAlpha != 255 ) || ( pInfo->m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 
		if ( !bIsTranslucent )
		{
			AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
				nWorldListLeafIndex, RENDER_GROUP_OPAQUE, pInfo->m_nModelType, pRLInfo[i].m_nAlpha );
			continue;
		}

		// FIXME: Remove call to GetFXBlend
		bool bIsTwoPass = ( pInfo->m_nTranslucencyType == RENDERABLE_IS_TWO_PASS ) && ( pRLInfo[i].m_nAlpha == 255 );	// Two pass?

		// Add to appropriate list if drawing translucent objects (shadow depth mapping will skip this)
		if ( info.m_bDrawTranslucentObjects ) 
		{
			RenderGroup_t group = pRLInfo[i].m_bIgnoreZBuffer ? RENDER_GROUP_TRANSLUCENT_IGNOREZ : RENDER_GROUP_TRANSLUCENT;

			AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
				nWorldListLeafIndex, group, pInfo->m_nModelType, pRLInfo[i].m_nAlpha, bIsTwoPass );
		}

		if ( bIsTwoPass )	// Also add to opaque list if it's a two-pass model... 
		{
			AddRenderableToRenderList( *info.m_pRenderList, pInfo->m_pRenderable, 
				nWorldListLeafIndex, RENDER_GROUP_OPAQUE, pInfo->m_nModelType, 255, bIsTwoPass );
		}
	}

	// Add detail props for this leaf
	for( ; nCurDetail < nDetailCount; ++nCurDetail )
	{
		DetailRenderableInfo_t &detailInfo = pDetailInfo[nCurDetail];
		if ( detailInfo.m_nLeafIndex > nWorldListLeafIndex )
			break;
		Assert( detailInfo.m_nLeafIndex == nWorldListLeafIndex );
		AddRenderableToRenderList( *info.m_pRenderList, detailInfo.m_pRenderable, 
			nWorldListLeafIndex, detailInfo.m_nRenderGroup, RENDERABLE_MODEL_ENTITY, detailInfo.m_InstanceData.m_nAlpha );
	}

	int nNewTranslucent = nTranslucentEntries - nTranslucent;
	if( ( nNewTranslucent != 0 ) && info.m_bDrawTranslucentObjects )
	{
		// Sort the new translucent entities.
		SortEntities( info.m_vecRenderOrigin, info.m_vecRenderForward, &pTranslucentEntries[nTranslucent], nNewTranslucent );
	}

	AddDependentRenderables( info );
}

static ConVar r_drawallrenderables( "r_drawallrenderables", "0", FCVAR_CHEAT, "Draw all renderables, even ones inside solid leaves." );

//-----------------------------------------------------------------------------
// Main entry point to build renderable lists
//-----------------------------------------------------------------------------
void CClientLeafSystem::BuildRenderablesList( const SetupRenderInfo_t &info )
{
	ASSERT_NO_REENTRY();

	// Deal with detail objects
	CUtlVectorFixedGrowable< DetailRenderableInfo_t, 2048 > detailRenderables( 2048 );

	// Get the fade information for detail objects
	float flDetailDist = g_pDetailObjectSystem->ComputeDetailFadeInfo( &info.m_pRenderList->m_DetailFade );
	g_pDetailObjectSystem->BuildRenderingData( detailRenderables, info, flDetailDist, info.m_pRenderList->m_DetailFade );

	// First build a non-unique list of renderables, separated by special leaf markers
	CUtlVectorFixedGrowable< RenderableInfo_t *, 2048 > orderedList( 2048 );

	if ( info.m_nViewID != VIEW_3DSKY && r_drawallrenderables.GetBool() )
	{
		// HACK - treat all renderables as being in first visible leaf
		int leaf = info.m_pWorldListInfo->m_pLeafDataList[ 0 ].leafIndex;
		orderedList.AddToTail( LeafToMarker( leaf ) );

		for ( int i = m_Renderables.Head(); i != m_Renderables.InvalidIndex(); i = m_Renderables.Next( i ) )
		{
			orderedList.AddToTail( &m_Renderables[ i ] );
		}
	}
	else
	{
		int leaf = 0;
		for ( int i = 0; i < info.m_pWorldListInfo->m_LeafCount; ++i )
		{
			leaf = info.m_pWorldListInfo->m_pLeafDataList[i].leafIndex;
			orderedList.AddToTail( LeafToMarker( leaf ) );

			// iterate over all elements in this leaf
			unsigned short idx = m_RenderablesInLeaf.FirstElement(leaf);
			for ( ; idx != m_RenderablesInLeaf.InvalidIndex(); idx = m_RenderablesInLeaf.NextElement( idx ) )
			{
				orderedList.AddToTail( &m_Renderables[ m_RenderablesInLeaf.Element(idx) ] );
			}
		}
	}

	// Debugging
	int nCount = orderedList.Count();
	RenderableInfo_t **ppRenderables = orderedList.Base();
	nCount = ExtractDuplicates( info.m_nRenderFrame, nCount, ppRenderables );
	nCount = ExtractStaticProps( nCount, ppRenderables );
	nCount = ExtractSplitscreenRenderables( nCount, ppRenderables );

	if ( !info.m_bDrawTranslucentObjects )
	{
		nCount = ExtractTranslucentRenderables( nCount, ppRenderables );
	}

	BuildRenderListInfo_t* pRLInfo = (BuildRenderListInfo_t*)stackalloc( nCount * sizeof(BuildRenderListInfo_t) );
	ComputeBounds( nCount, ppRenderables, pRLInfo );

	nCount = ExtractCulledRenderables( nCount, ppRenderables, pRLInfo );

	if ( info.m_bDrawTranslucentObjects )
	{
		nCount = ComputeTranslucency( gpGlobals->framecount /*info.m_nRenderFrame*/, info.m_nViewID, nCount, ppRenderables, pRLInfo );
	}

	nCount = ExtractOccludedRenderables( nCount, ppRenderables, pRLInfo );

	AddRenderablesToRenderLists( info, nCount, ppRenderables, pRLInfo, detailRenderables.Count(), detailRenderables.Base() );
	stackfree( pRLInfo );
}


RenderGroup_t CClientLeafSystem::GenerateRenderListEntry( IClientRenderable *pRenderable, CClientRenderablesList::CEntry &entryOut )
{
	ClientRenderHandle_t iter = m_Renderables.Head();
	while( m_Renderables.IsValidIndex( iter ) )
	{
		RenderableInfo_t &info = m_Renderables.Element( iter );
		if( info.m_pRenderable == pRenderable )
		{			
			int nAlpha = info.m_pAlphaProperty ? info.m_pAlphaProperty->ComputeRenderAlpha( ) : 255;
			bool bIsTranslucent = ( nAlpha != 255 ) || ( info.m_nTranslucencyType != RENDERABLE_IS_OPAQUE ); 

			entryOut.m_pRenderable = pRenderable;
			entryOut.m_iWorldListInfoLeaf = 0; //info.m_RenderLeaf;			
			entryOut.m_TwoPass = ( info.m_nTranslucencyType == RENDERABLE_IS_TWO_PASS );
			entryOut.m_nModelType = info.m_nModelType;
			entryOut.m_InstanceData.m_nAlpha = nAlpha;
			if ( !bIsTranslucent )
				return RENDER_GROUP_OPAQUE;
			return info.m_pAlphaProperty->IgnoresZBuffer() ? RENDER_GROUP_TRANSLUCENT_IGNOREZ : RENDER_GROUP_TRANSLUCENT;
		}
		iter = m_Renderables.Next( iter );
	}

	entryOut.m_pRenderable = NULL;
	entryOut.m_iWorldListInfoLeaf = 0;
	entryOut.m_TwoPass = false;
	entryOut.m_nModelType = RENDERABLE_MODEL_ENTITY;
	entryOut.m_InstanceData.m_nAlpha = 255;

	return RENDER_GROUP_COUNT;
}
