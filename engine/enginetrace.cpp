//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "engine/IEngineTrace.h"
#include "icliententitylist.h"
#include "ispatialpartitioninternal.h"
#include "icliententity.h"
#include "cmodel_engine.h"
#include "dispcoll_common.h"
#include "staticpropmgr.h"
#include "server.h"
#include "edict.h"
#include "gl_model_private.h"
#include "world.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "server_class.h"
#include "debugoverlay.h"
#include "collisionutils.h"
#include "tier0/vprof.h"
#include "convar.h"
#include "mathlib/polyhedron.h"
#include "sys_dll.h"
#include "vphysics/virtualmesh.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Various statistics to gather
//-----------------------------------------------------------------------------
enum
{
	TRACE_STAT_COUNTER_TRACERAY = 0,
	TRACE_STAT_COUNTER_POINTCONTENTS,
	TRACE_STAT_COUNTER_ENUMERATE,
	NUM_TRACE_STAT_COUNTER
};


//-----------------------------------------------------------------------------
// Used to visualize raycasts going on
//-----------------------------------------------------------------------------
#ifdef _DEBUG

ConVar debugrayenable( "debugrayenable", "0", NULL, "Use this to enable ray testing.  To reset: bind \"F1\" \"clearalloverlays; debugrayreset  0; host_framerate 66.66666667\"" );
ConVar debugrayreset( "debugrayreset", "0" );
ConVar debugraylimit( "debugraylimit", "500", NULL, "number of rays per frame that you have to hit before displaying them all" );
static CUtlVector<Ray_t> s_FrameRays;
#endif

#define BENCHMARK_RAY_TEST 0

#if BENCHMARK_RAY_TEST
static CUtlVector<Ray_t> s_BenchmarkRays;
#endif



//-----------------------------------------------------------------------------
// Implementation of IEngineTrace
//-----------------------------------------------------------------------------
abstract_class CEngineTrace : public IEngineTrace
{
public:
	CEngineTrace() { m_pRootMoveParent = NULL; }
	// Returns the contents mask at a particular world-space position
	virtual int		GetPointContents( const Vector &vecAbsPosition, IHandleEntity** ppEntity );

	virtual int		GetPointContents_Collideable( ICollideable *pCollide, const Vector &vecAbsPosition );

	// Traces a ray against a particular edict
	virtual void	ClipRayToEntity( const Ray_t &ray, unsigned int fMask, IHandleEntity *pEntity, trace_t *pTrace );

	// A version that simply accepts a ray (can work as a traceline or tracehull)
	virtual void	TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );

	// A version that sets up the leaf and entity lists and allows you to pass those in for collision.
	virtual void	SetupLeafAndEntityListRay( const Ray_t &ray, CTraceListData &traceData );
	virtual void    SetupLeafAndEntityListBox( const Vector &vecBoxMin, const Vector &vecBoxMax, CTraceListData &traceData );
	virtual void	TraceRayAgainstLeafAndEntityList( const Ray_t &ray, CTraceListData &traceData, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );

	// A version that sweeps a collideable through the world
	// abs start + abs end represents the collision origins you want to sweep the collideable through
	// vecAngles represents the collision angles of the collideable during the sweep
	virtual void	SweepCollideable( ICollideable *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
		const QAngle &vecAngles, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );

	// Enumerates over all entities along a ray
	// If triggers == true, it enumerates all triggers along a ray
	virtual void	EnumerateEntities( const Ray_t &ray, bool triggers, IEntityEnumerator *pEnumerator );

	// Same thing, but enumerate entitys within a box
	virtual void	EnumerateEntities( const Vector &vecAbsMins, const Vector &vecAbsMaxs, IEntityEnumerator *pEnumerator );

	// FIXME: Different versions for client + server. Eventually we need to make these go away
	virtual void HandleEntityToCollideable( IHandleEntity *pHandleEntity, ICollideable **ppCollide, const char **ppDebugName ) = 0;
	virtual ICollideable *GetWorldCollideable() = 0;

	// Traces a ray against a particular edict
	virtual void ClipRayToCollideable( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace );

	// HACKHACK: Temp
	virtual int GetStatByIndex( int index, bool bClear );

	//finds brushes in an AABB, prone to some false positives
	virtual void GetBrushesInAABB( const Vector &vMins, const Vector &vMaxs, CUtlVector<int> *pOutput, int iContentsMask = 0xFFFFFFFF );

	//Creates a CPhysCollide out of all displacements wholly or partially contained in the specified AABB
	virtual CPhysCollide* GetCollidableFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs );

	//retrieve brush planes and contents, returns true if data is being returned in the output pointers, false if the brush doesn't exist
	virtual bool GetBrushInfo( int iBrush, CUtlVector<Vector4D> *pPlanesOut, int *pContentsOut );

	virtual bool PointOutsideWorld( const Vector &ptTest ); //Tests a point to see if it's outside any playable area


	// Walks bsp to find the leaf containing the specified point
	virtual int GetLeafContainingPoint( const Vector &ptTest );

private:
	// FIXME: Different versions for client + server. Eventually we need to make these go away
	virtual void SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace ) = 0;
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity ) = 0;
	virtual int SpatialPartitionMask() const = 0;
	virtual int SpatialPartitionTriggerMask() const = 0;

	// Figure out point contents for entities at a particular position
	int EntityContents( const Vector &vecAbsPosition );

	// Should we perform the custom raytest?
	bool ShouldPerformCustomRayTest( const Ray_t& ray, ICollideable *pCollideable ) const;

	// Performs the custom raycast
	bool ClipRayToCustom( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace );

	// Perform vphysics trace
	bool ClipRayToVPhysics( const Ray_t &ray, unsigned int fMask, ICollideable *pCollideable, studiohdr_t *pStudioHdr, trace_t *pTrace );

	// Perform hitbox trace
	bool ClipRayToHitboxes( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace );

	// Perform bsp trace
	bool ClipRayToBSP( const Ray_t &ray, unsigned int fMask, ICollideable *pCollideable, trace_t *pTrace );

	// bbox
	bool ClipRayToBBox( const Ray_t &ray, unsigned int fMask, ICollideable *pCollideable, trace_t *pTrace );

	// OBB
	bool ClipRayToOBB( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace );

	// Clips a trace to another trace
	bool ClipTraceToTrace( trace_t &clipTrace, trace_t *pFinalTrace );
private:
	int m_traceStatCounters[NUM_TRACE_STAT_COUNTER];
	const matrix3x4_t *m_pRootMoveParent;
	friend void RayBench( const CCommand &args );

};

class CEngineTraceServer : public CEngineTrace
{
private:
	virtual void HandleEntityToCollideable( IHandleEntity *pEnt, ICollideable **ppCollide, const char **ppDebugName );
	virtual void SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace );
	virtual int SpatialPartitionMask() const;
	virtual int SpatialPartitionTriggerMask() const;
	virtual ICollideable *GetWorldCollideable();
	friend void RayBench( const CCommand &args );
public:
	// IEngineTrace
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity );
};

#ifndef SWDS
class CEngineTraceClient : public CEngineTrace
{
private:
	virtual void HandleEntityToCollideable( IHandleEntity *pEnt, ICollideable **ppCollide, const char **ppDebugName );
	virtual void SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace );
	virtual int SpatialPartitionMask() const;
	virtual int SpatialPartitionTriggerMask() const;
	virtual ICollideable *GetWorldCollideable();
public:
	// IEngineTrace
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity );
};
#endif

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the game + client DLLs
//-----------------------------------------------------------------------------
static CEngineTraceServer	s_EngineTraceServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEngineTraceServer, IEngineTrace, INTERFACEVERSION_ENGINETRACE_SERVER, s_EngineTraceServer);

#ifndef SWDS
static CEngineTraceClient	s_EngineTraceClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEngineTraceClient, IEngineTrace, INTERFACEVERSION_ENGINETRACE_CLIENT, s_EngineTraceClient);
#endif

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the engine.
//-----------------------------------------------------------------------------
IEngineTrace *g_pEngineTraceServer = &s_EngineTraceServer;
#ifndef SWDS
IEngineTrace *g_pEngineTraceClient = &s_EngineTraceClient;
#endif

//-----------------------------------------------------------------------------
// Client-server neutral method of getting at collideables
//-----------------------------------------------------------------------------
#ifndef SWDS
ICollideable *CEngineTraceClient::GetCollideable( IHandleEntity *pEntity )
{
	Assert( pEntity );

	ICollideable *pProp = StaticPropMgr()->GetStaticProp( pEntity );
	if ( pProp )
		return pProp;

	IClientUnknown *pUnk = entitylist->GetClientUnknownFromHandle( pEntity->GetRefEHandle() );
	return pUnk->GetCollideable(); 
}
#endif

ICollideable *CEngineTraceServer::GetCollideable( IHandleEntity *pEntity )
{
	Assert( pEntity );

	ICollideable *pProp = StaticPropMgr()->GetStaticProp( pEntity );
	if ( pProp )
		return pProp;

	IServerUnknown *pNetUnknown = static_cast<IServerUnknown*>(pEntity);
	return pNetUnknown->GetCollideable();
}


//-----------------------------------------------------------------------------
// Spatial partition masks for iteration
//-----------------------------------------------------------------------------
#ifndef SWDS
int CEngineTraceClient::SpatialPartitionMask() const
{
	return PARTITION_CLIENT_SOLID_EDICTS;
}
#endif

int CEngineTraceServer::SpatialPartitionMask() const
{
	return PARTITION_ENGINE_SOLID_EDICTS;
}

#ifndef SWDS
int CEngineTraceClient::SpatialPartitionTriggerMask() const
{
	return 0;
}
#endif

int CEngineTraceServer::SpatialPartitionTriggerMask() const
{
	return PARTITION_ENGINE_TRIGGER_EDICTS;
}


//-----------------------------------------------------------------------------
// Spatial partition enumerator looking for entities that we may lie within
//-----------------------------------------------------------------------------
class CPointContentsEnum : public IPartitionEnumerator
{
public:
	CPointContentsEnum( CEngineTrace *pEngineTrace, const Vector &pos ) : m_Contents(CONTENTS_EMPTY) 
	{
		m_pEngineTrace = pEngineTrace;
		m_Pos = pos; 
		m_pCollide = NULL;
	}

	static inline bool TestEntity( 
		CEngineTrace *pEngineTrace,
		ICollideable *pCollide, 
		const Vector &vPos, 
		int *pContents, 
		ICollideable **pWorldCollideable )
	{
		// Deal with static props
		// NOTE: I could have added static props to a different list and
		// enumerated them separately, but that would have been less efficient
		if ( StaticPropMgr()->IsStaticProp( pCollide->GetEntityHandle() ) )
		{
			Ray_t ray;
			trace_t trace;
			ray.Init( vPos, vPos );
			pEngineTrace->ClipRayToCollideable( ray, MASK_ALL, pCollide, &trace );
			if (trace.startsolid)
			{
				// We're in a static prop; that's solid baby
				// Pretend we hit the world
				*pContents = CONTENTS_SOLID;
				*pWorldCollideable = pEngineTrace->GetWorldCollideable();
				return true;
			}
			return false;
		}
		
		// We only care about solid volumes
		if ((pCollide->GetSolidFlags() & FSOLID_VOLUME_CONTENTS) == 0)
			return false;

		model_t* pModel = (model_t*)pCollide->GetCollisionModel();
		if ( pModel && pModel->type == mod_brush )
		{
			Assert( pCollide->GetCollisionModelIndex() < MAX_MODELS && pCollide->GetCollisionModelIndex() >= 0 );
			int nHeadNode = GetModelHeadNode( pCollide );
			int contents = CM_TransformedPointContents( vPos, nHeadNode, 
				pCollide->GetCollisionOrigin(), pCollide->GetCollisionAngles() );

			if (contents != CONTENTS_EMPTY)
			{
				// Return the contents of the first thing we hit
				*pContents = contents;
				*pWorldCollideable = pCollide;
				return true;
			}
		}

		return false;
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		ICollideable *pCollide;
		const char *pDbgName;
		m_pEngineTrace->HandleEntityToCollideable( pHandleEntity, &pCollide, &pDbgName );
		if (!pCollide)
			return ITERATION_CONTINUE;

		if ( CPointContentsEnum::TestEntity( m_pEngineTrace, pCollide, m_Pos, &m_Contents, &m_pCollide ) )
			return ITERATION_STOP;
		else
			return ITERATION_CONTINUE;
	}

private:
	static int GetModelHeadNode( ICollideable *pCollide )
	{
		int modelindex = pCollide->GetCollisionModelIndex();
		if(modelindex >= MAX_MODELS || modelindex < 0)
			return -1;

		model_t *pModel = (model_t*)pCollide->GetCollisionModel();
		if(!pModel)
			return -1;

		if(cmodel_t *pCModel = CM_InlineModelNumber(modelindex-1))
			return pCModel->headnode;
		else
			return -1;
	}

public:
	int m_Contents;
	ICollideable *m_pCollide;

private:
	CEngineTrace *m_pEngineTrace;
	Vector m_Pos;
};


//-----------------------------------------------------------------------------
// Returns the contents mask at a particular world-space position
//-----------------------------------------------------------------------------
int	CEngineTrace::GetPointContents( const Vector &vecAbsPosition, IHandleEntity** ppEntity )
{
	VPROF( "CEngineTrace_GetPointContents" );
//	VPROF_BUDGET( "CEngineTrace_GetPointContents", "CEngineTrace_GetPointContents" );
	
	m_traceStatCounters[TRACE_STAT_COUNTER_POINTCONTENTS]++;
	// First check the collision model
	int nContents = CM_PointContents( vecAbsPosition, 0 );
	if ( nContents & MASK_CURRENT )
	{
		nContents = CONTENTS_WATER;
	}
	
	if ( nContents != CONTENTS_SOLID )
	{
		CPointContentsEnum contentsEnum(this, vecAbsPosition);
		SpatialPartition()->EnumerateElementsAtPoint( SpatialPartitionMask(),
			vecAbsPosition, false, &contentsEnum );

		int nEntityContents = contentsEnum.m_Contents;
		if ( nEntityContents & MASK_CURRENT )
			nContents = CONTENTS_WATER;
		if ( nEntityContents != CONTENTS_EMPTY )
		{
			if (ppEntity)
			{
				*ppEntity = contentsEnum.m_pCollide->GetEntityHandle();
			}

			return nEntityContents;
		}
	}

	if (ppEntity)
	{
		*ppEntity = GetWorldCollideable()->GetEntityHandle();
	}

	return nContents;
}


int CEngineTrace::GetPointContents_Collideable( ICollideable *pCollide, const Vector &vecAbsPosition )
{
	int contents = CONTENTS_EMPTY;
	ICollideable *pDummy;
	CPointContentsEnum::TestEntity( this, pCollide, vecAbsPosition, &contents, &pDummy );
	return contents;
}


//-----------------------------------------------------------------------------
// Should we perform the custom raytest?
//-----------------------------------------------------------------------------
inline bool CEngineTrace::ShouldPerformCustomRayTest( const Ray_t& ray, ICollideable *pCollideable ) const
{
	// No model? The entity's got its own collision detector maybe
	// Does the entity force box or ray tests to go through its code?
	return( (pCollideable->GetSolid() == SOLID_CUSTOM) ||
			(ray.m_IsRay && (pCollideable->GetSolidFlags() & FSOLID_CUSTOMRAYTEST )) || 
			(!ray.m_IsRay && (pCollideable->GetSolidFlags() & FSOLID_CUSTOMBOXTEST )) );
}


//-----------------------------------------------------------------------------
// Performs the custom raycast
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToCustom( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace )
{
	if ( pCollideable->TestCollision( ray, fMask, *pTrace ))
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Performs the hitbox raycast, returns true if the hitbox test was made
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToHitboxes( const Ray_t& ray, unsigned int fMask, ICollideable *pCollideable, trace_t* pTrace )
{
	trace_t hitboxTrace;
	CM_ClearTrace( &hitboxTrace );

	// Keep track of the contents of what was hit initially
	hitboxTrace.contents = pTrace->contents;
	VectorAdd( ray.m_Start, ray.m_StartOffset, hitboxTrace.startpos );
	VectorAdd( hitboxTrace.startpos, ray.m_Delta, hitboxTrace.endpos );

	// At the moment, it has to be a true ray to work with hitboxes
	if ( !ray.m_IsRay )
		return false;

	// If the hitboxes weren't even tested, then just use the original trace
	if (!pCollideable->TestHitboxes( ray, fMask, hitboxTrace ))
		return false;

	// If they *were* tested and missed, clear the original trace
	if (!hitboxTrace.DidHit())
	{
		CM_ClearTrace( pTrace );
		pTrace->startpos = hitboxTrace.startpos;
		pTrace->endpos = hitboxTrace.endpos;
	}
	else if ( pCollideable->GetSolid() != SOLID_VPHYSICS )
	{
		// If we also hit the hitboxes, maintain fractionleftsolid +
		// startpos because those are reasonable enough values and the
		// hitbox code doesn't set those itself.
		Vector vecStartPos = pTrace->startpos;
		float flFractionLeftSolid = pTrace->fractionleftsolid;

		*pTrace = hitboxTrace;

		if (hitboxTrace.startsolid)
		{
			pTrace->startpos = vecStartPos;
			pTrace->fractionleftsolid = flFractionLeftSolid;
		}
	}
	else
	{
		// Fill out the trace hitbox details
		pTrace->contents = hitboxTrace.contents;
		pTrace->hitgroup = hitboxTrace.hitgroup;
		pTrace->hitbox = hitboxTrace.hitbox;
		pTrace->physicsbone = hitboxTrace.physicsbone;
		pTrace->surface = hitboxTrace.surface;
		Assert( pTrace->physicsbone >= 0 );
		// Fill out the surfaceprop details from the hitbox. Use the physics bone instead of the hitbox bone
		Assert(pTrace->surface.flags == SURF_HITBOX);
	}

	return true;
}

int CEngineTrace::GetStatByIndex( int index, bool bClear )
{
	if ( index >= NUM_TRACE_STAT_COUNTER )
		return 0;
	int out = m_traceStatCounters[index];
	if ( bClear )
	{
		m_traceStatCounters[index] = 0;
	}
	return out;
}



static void FASTCALL GetBrushesInAABB_ParseLeaf( const Vector *pExtents, CCollisionBSPData *pBSPData, cleaf_t *pLeaf, CUtlVector<int> *pOutput, int iContentsMask, int *pCounters )
{
	for( unsigned int i = 0; i != pLeaf->numleafbrushes; ++i )
	{
		int iBrushNumber = pBSPData->map_leafbrushes[pLeaf->firstleafbrush + i];
		cbrush_t *pBrush = &pBSPData->map_brushes[iBrushNumber];

		if( pCounters[iBrushNumber] ) 
			continue;

		pCounters[iBrushNumber] = 1;

		if( (pBrush->contents & iContentsMask) == 0 )
			continue;

		if ( pBrush->IsBox() )
		{
			cboxbrush_t *pBox = &pBSPData->map_boxbrushes[pBrush->GetBox()];
			if ( IsBoxIntersectingBox(pBox->mins, pBox->maxs, pExtents[0], pExtents[7]) )
			{
				pOutput->AddToTail(iBrushNumber);
			}
		}
		else
		{
			unsigned int j;
			for( j = 0; j != pBrush->numsides; ++j )
			{
				cplane_t *pPlane = pBSPData->map_brushsides[pBrush->firstbrushside + j].plane;

				if( (pExtents[pPlane->signbits].Dot( pPlane->normal ) - pPlane->dist) > 0.0f )
					break; //the bounding box extent that was most likely to be encapsulated by the plane is outside the halfspace, brush not in bbox
			}

			if( j == pBrush->numsides )
				pOutput->AddToTail( iBrushNumber ); //brush was most likely in bbox
		}
	}
}


void CEngineTrace::GetBrushesInAABB( const Vector &vMins, const Vector &vMaxs, CUtlVector<int> *pOutput, int iContentsMask )
{
	if( pOutput == NULL ) return;
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	Vector ptBBoxExtents[8]; //for fast plane checking
	for( int i = 0; i != 8; ++i )
	{
		//set these up to be opposite that of cplane_t's signbits for it's normal
		ptBBoxExtents[i].x = (i & (1<<0)) ? (vMaxs.x) : (vMins.x);
		ptBBoxExtents[i].y = (i & (1<<1)) ? (vMaxs.y) : (vMins.y);
		ptBBoxExtents[i].z = (i & (1<<2)) ? (vMaxs.z) : (vMins.z);
	}	

	int *pLeafList = (int *)stackalloc( pBSPData->numleafs * 2 * sizeof( int ) ); // *2 just in case
	int iNumLeafs = CM_BoxLeafnums( vMins, vMaxs, pLeafList, pBSPData->numleafs * 2, NULL );

	CUtlVector<int> counters;
	counters.SetSize( pBSPData->numbrushes );
	memset( counters.Base(), 0, pBSPData->numbrushes * sizeof(int) );
	for( int i = 0; i != iNumLeafs; ++i )
		GetBrushesInAABB_ParseLeaf( ptBBoxExtents, pBSPData, &pBSPData->map_leafs[pLeafList[i]], pOutput, iContentsMask, counters.Base() );
}



//-----------------------------------------------------------------------------
// Purpose: Used to copy the collision information of all displacement surfaces in a specified box
// Input  : vMins - min vector of the AABB
//			vMaxs - max vector of the AABB
// Output : CPhysCollide* the collision mesh created from all the displacements partially contained in the specified box
//					Note: We're not clipping to the box. Collidable may be larger than the box provided.
//-----------------------------------------------------------------------------
CPhysCollide* CEngineTrace::GetCollidableFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	int *pLeafList = (int *)stackalloc( pBSPData->numleafs * sizeof( int ) ); 
	int iLeafCount = CM_BoxLeafnums( vMins, vMaxs, pLeafList, pBSPData->numleafs, NULL );

	// Get all the triangles for displacement surfaces in this box, add them to a polysoup
	CPhysPolysoup *pDispCollideSoup = physcollision->PolysoupCreate();

	// Count total triangles added to this poly soup- Can't support more than 65435.
	int iTriCount = 0;

	TraceInfo_t *pTraceInfo = BeginTrace();

	TraceCounter_t *pCounters = pTraceInfo->GetDispCounters();
	int count = pTraceInfo->GetCount();

	// For each leaf in which the box lies, Get all displacements in that leaf and use their triangles to create the mesh
	for ( int i = 0; i < iLeafCount; ++i )
	{
		// Current leaf
		cleaf_t curLeaf = pBSPData->map_leafs[ pLeafList[i] ];

		// Test box against all displacements in the leaf.
		for( int k = 0; k < curLeaf.dispCount; k++ )
		{
			int dispIndex = pBSPData->map_dispList[curLeaf.dispListStart + k];
			CDispCollTree *pDispTree = &g_pDispCollTrees[dispIndex];
		
			// make sure we only check this brush once per trace/stab
			if ( !pTraceInfo->Visit( pDispTree->m_iCounter, count, pCounters ) )
				continue;

			// If this displacement doesn't touch our test box, don't add it to the list.
			if ( !IsBoxIntersectingBox( vMins, vMaxs, pDispTree->m_mins, pDispTree->m_maxs) )
				continue;

			// The the triangle mesh for this displacement surface
			virtualmeshlist_t meshTriList;
			pDispTree->GetVirtualMeshList( &meshTriList );

			Assert ( meshTriList.indexCount%3 == 0 );
			Assert ( meshTriList.indexCount != 0 );
			Assert ( meshTriList.indexCount/3 == meshTriList.triangleCount );

			// Don't allow more than 64k triangles in a collision model
			// TODO: Return a list of collidables? How often do we break 64k triangles?
			iTriCount += meshTriList.triangleCount;
			if ( iTriCount > 65535 )
			{
				AssertMsg ( 0, "Displacement surfaces have too many triangles to duplicate in GetCollidableFromDisplacementsInBox." );
				EndTrace( pTraceInfo );
				return NULL;
			}

			for ( int j = 0; j < meshTriList.indexCount; j+=3 )
			{
				// Don't index past the index list
				Assert( j+2 < meshTriList.indexCount );

				if ( j+2 >= meshTriList.indexCount )
				{
					EndTrace( pTraceInfo );
					physcollision->PolysoupDestroy( pDispCollideSoup );
					return NULL;
				}

				unsigned short i0 = meshTriList.indices[j+0];
				unsigned short i1 = meshTriList.indices[j+1];
				unsigned short i2 = meshTriList.indices[j+2];

				// Don't index past the end of the vert list
				Assert ( i0 < meshTriList.vertexCount && i1 < meshTriList.vertexCount && i2 < meshTriList.vertexCount );

				if ( i0 >= meshTriList.vertexCount || i1 >= meshTriList.vertexCount || i2 >= meshTriList.vertexCount )
				{
					EndTrace( pTraceInfo );
					physcollision->PolysoupDestroy( pDispCollideSoup );
					return NULL;
				}

				Vector v0 = meshTriList.pVerts[ i0 ];
				Vector v1 = meshTriList.pVerts[ i1 ];
				Vector v2 = meshTriList.pVerts[ i2 ];

				Assert ( v0.IsValid() && v1.IsValid() && v2.IsValid() );

				// We don't need exact clipping to the box... Include any triangle that has at least one vert on the inside
				if ( IsPointInBox( v0, vMins, vMaxs ) || IsPointInBox( v1, vMins, vMaxs ) || IsPointInBox( v2, vMins, vMaxs ) )
				{
					// This is for collision only, so we don't need to worry about blending-- Use the first surface prop.
					int nProp = pDispTree->GetSurfaceProps(0);
					physcollision->PolysoupAddTriangle( pDispCollideSoup, v0, v1, v2, nProp );
				}
 
			}// triangle loop

		}// for each displacement in leaf
		
	}// for each leaf

	EndTrace( pTraceInfo );

	CPhysCollide* pCollide = physcollision->ConvertPolysoupToCollide ( pDispCollideSoup, false );

	// clean up poly soup
	physcollision->PolysoupDestroy( pDispCollideSoup );

	return pCollide;
}

bool CEngineTrace::GetBrushInfo( int iBrush, CUtlVector<Vector4D> *pPlanesOut, int *pContentsOut )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if( iBrush < 0 || iBrush >= pBSPData->numbrushes )
		return false;

	cbrush_t *pBrush = &pBSPData->map_brushes[iBrush];

	if( pPlanesOut )
	{
		pPlanesOut->RemoveAll();
		Vector4D p;
		if ( pBrush->IsBox() )
		{
			cboxbrush_t *pBox = &pBSPData->map_boxbrushes[pBrush->GetBox()];

			for ( int i = 0; i < 6; i++ )
			{
				p.Init(0,0,0,0);
				if ( i < 3 )
				{
					p[i] = 1.0f;
					p[3] = pBox->maxs[i];
				}
				else
				{
					p[i-3] = -1.0f;
					p[3] = -pBox->mins[i-3];
				}
				pPlanesOut->AddToTail( p );
			}
		}
		else
		{
			cbrushside_t *stopside = &pBSPData->map_brushsides[pBrush->firstbrushside];
			// Note:  Don't do this in the [] since the final one on the last brushside will be past the end of the array end by one index
			stopside += pBrush->numsides;
			for( cbrushside_t *side = &pBSPData->map_brushsides[pBrush->firstbrushside]; side != stopside; ++side )
			{
				Vector4D pVec( side->plane->normal.x, side->plane->normal.y, side->plane->normal.z, side->plane->dist );
				pPlanesOut->AddToTail( pVec );
			}
		}
	}

	if( pContentsOut )
		*pContentsOut = pBrush->contents;

	return true;
}

//Tests a point to see if it's outside any playable area
bool CEngineTrace::PointOutsideWorld( const Vector &ptTest )
{
	int iLeaf = CM_PointLeafnum( ptTest );
	Assert( iLeaf >= 0 );

	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if( pBSPData->map_leafs[iLeaf].cluster == -1 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Expose to the game dll a method for finding the leaf which contains a given point
// Input  : &vPos - Returns the leaf which contains this point
// Output : int - The handle to the leaf
//-----------------------------------------------------------------------------
int CEngineTrace::GetLeafContainingPoint( const Vector &vPos )
{
	return CM_PointLeafnum( vPos );
}



//-----------------------------------------------------------------------------
// Convex info for studio + brush models
//-----------------------------------------------------------------------------
class CBrushConvexInfo : public IConvexInfo
{
public:
	CBrushConvexInfo()
	{
		m_pBSPData = GetCollisionBSPData();
	}
	virtual unsigned int GetContents( int convexGameData )
	{
		return m_pBSPData->map_brushes[convexGameData].contents;
	}

private:
	CCollisionBSPData *m_pBSPData;
};

class CStudioConvexInfo : public IConvexInfo
{
public:
	CStudioConvexInfo( studiohdr_t *pStudioHdr )
	{
		m_pStudioHdr = pStudioHdr;
	}

	virtual unsigned int GetContents( int convexGameData )
	{
		if ( convexGameData == 0 )
		{
			return m_pStudioHdr->contents;
		}

		Assert( convexGameData <= m_pStudioHdr->numbones );
		mstudiobone_t *pBone = m_pStudioHdr->pBone(convexGameData - 1);
		return pBone->contents;
	}

private:
	studiohdr_t *m_pStudioHdr;
};


//-----------------------------------------------------------------------------
// Perform vphysics trace
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToVPhysics( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, studiohdr_t *pStudioHdr, trace_t *pTrace )
{
	if ( pEntity->GetSolid() != SOLID_VPHYSICS )
		return false;

	bool bTraced = false;

	// use the vphysics model for rotated brushes and vphysics simulated objects
	const model_t *pModel = pEntity->GetCollisionModel();

	if ( !pModel )
		return false;

	if ( pStudioHdr )
	{
		CStudioConvexInfo studioConvex( pStudioHdr );
		vcollide_t *pCollide = g_pMDLCache->GetVCollide( pModel->studio );
		if ( pCollide && pCollide->solidCount )
		{
			physcollision->TraceBox( 
				ray,
				fMask,
				&studioConvex,
				pCollide->solids[0], // UNDONE: Support other solid indices?!?!?!? (forced zero)
				pEntity->GetCollisionOrigin(), 
				pEntity->GetCollisionAngles(), 
				pTrace );
			bTraced = true;
		}
	}
	else
	{
		Assert(pModel->type != mod_studio);
		// use the regular code for raytraces against brushes
		// do ray traces with normal code, but use vphysics to do box traces
		if ( !ray.m_IsRay || pModel->type != mod_brush )
		{
			int nModelIndex = pEntity->GetCollisionModelIndex();

			// BUGBUG: This only works when the vcollide in question is the first solid in the model
			vcollide_t *pCollide = CM_VCollideForModel( nModelIndex, (model_t*)pModel );

			if ( pCollide && pCollide->solidCount )
			{
				CBrushConvexInfo brushConvex;

				IConvexInfo *pConvexInfo = (pModel->type) == mod_brush ? &brushConvex : NULL;
				physcollision->TraceBox( 
					ray,
					fMask,
					pConvexInfo,
					pCollide->solids[0], // UNDONE: Support other solid indices?!?!?!? (forced zero)
					pEntity->GetCollisionOrigin(), 
					pEntity->GetCollisionAngles(), 
					pTrace );
				bTraced = true;
			}
		}
	}

	return bTraced;
}


//-----------------------------------------------------------------------------
// Perform bsp trace
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipRayToBSP( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	int nModelIndex = pEntity->GetCollisionModelIndex();
	cmodel_t *pCModel = CM_InlineModelNumber( nModelIndex - 1 );
	int nHeadNode = pCModel->headnode;

	CM_TransformedBoxTrace( ray, nHeadNode, fMask, pEntity->GetCollisionOrigin(), pEntity->GetCollisionAngles(), *pTrace );
	return true;
}


// NOTE: Switched over to SIMD ray/box test since there is a bug we haven't hunted down yet in the scalar version
bool CEngineTrace::ClipRayToBBox( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	extern bool IntersectRayWithBox( const Ray_t &ray, const VectorAligned &inInvDelta, const VectorAligned &inBoxMins, const VectorAligned &inBoxMaxs, trace_t *RESTRICT pTrace );

	if ( pEntity->GetSolid() != SOLID_BBOX )
		return false;

	// We can't use the OBBMins/Maxs unless the collision angles are world-aligned
	Assert( pEntity->GetCollisionAngles() == vec3_angle );

	VectorAligned vecAbsMins, vecAbsMaxs;
	VectorAligned vecInvDelta;
	// NOTE: If m_pRootMoveParent is set, then the boxes should be rotated into the root parent's space
	if ( !ray.m_IsRay && m_pRootMoveParent )
	{
		Ray_t ray_l;

		ray_l.m_Extents = ray.m_Extents;

		VectorIRotate( ray.m_Delta, *m_pRootMoveParent, ray_l.m_Delta );
		ray_l.m_StartOffset.Init();
		VectorITransform( ray.m_Start, *m_pRootMoveParent, ray_l.m_Start );

		vecInvDelta = ray_l.InvDelta();
		Vector localEntityOrigin;
		VectorITransform( pEntity->GetCollisionOrigin(), *m_pRootMoveParent, localEntityOrigin );
		ray_l.m_IsRay = ray.m_IsRay;
		ray_l.m_IsSwept = ray.m_IsSwept;

		VectorAdd( localEntityOrigin,  pEntity->OBBMins(), vecAbsMins );
		VectorAdd( localEntityOrigin, pEntity->OBBMaxs(), vecAbsMaxs );
		IntersectRayWithBox( ray_l, vecInvDelta, vecAbsMins, vecAbsMaxs, pTrace );

		if ( pTrace->DidHit() )
		{
			Vector temp;
			VectorCopy (pTrace->plane.normal, temp);
			VectorRotate( temp, *m_pRootMoveParent, pTrace->plane.normal );
			VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );

			if (pTrace->fraction == 1)
			{
				VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos);
			}
			else
			{
				VectorMA( pTrace->startpos, pTrace->fraction, ray.m_Delta, pTrace->endpos );
			}
			pTrace->plane.dist = DotProduct( pTrace->endpos, pTrace->plane.normal );
			if ( pTrace->fractionleftsolid < 1 )
			{
				pTrace->startpos += ray.m_Delta * pTrace->fractionleftsolid;
			}
		}
		else
		{
			VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		}

		return true;
	}

	vecInvDelta = ray.InvDelta();
	VectorAdd( pEntity->GetCollisionOrigin(), pEntity->OBBMins(), vecAbsMins );
	VectorAdd( pEntity->GetCollisionOrigin(), pEntity->OBBMaxs(), vecAbsMaxs );
	IntersectRayWithBox( ray, vecInvDelta, vecAbsMins, vecAbsMaxs, pTrace); 
	return true;
}

bool CEngineTrace::ClipRayToOBB( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	if ( pEntity->GetSolid() != SOLID_OBB )
		return false;

	// NOTE: This is busted because it doesn't compute fractionleftsolid, which at the
	// moment is required for the engine trace system.
	IntersectRayWithOBB( ray, pEntity->GetCollisionOrigin(), pEntity->GetCollisionAngles(), 
		pEntity->OBBMins(), pEntity->OBBMaxs(), DIST_EPSILON, pTrace );
	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for clipping rays to entities
//-----------------------------------------------------------------------------
#ifndef SWDS
void CEngineTraceClient::SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace )
{
	if ( !pTrace->DidHit() )
		return;

	// FIXME: This is only necessary because of traces occurring during
	// LevelInit (a suspect time to be tracing)
	if (!pCollideable)
	{
		pTrace->m_pEnt = NULL;
		return;
	}

	IClientUnknown *pUnk = (IClientUnknown*)pCollideable->GetEntityHandle();
	if ( !StaticPropMgr()->IsStaticProp( pUnk ) )
	{
		pTrace->m_pEnt = (CBaseEntity*)(pUnk->GetIClientEntity());
	}
	else
	{
		// For static props, point to the world, hitbox is the prop index
		pTrace->m_pEnt = (CBaseEntity*)(entitylist->GetClientEntity(0));
		pTrace->hitbox = StaticPropMgr()->GetStaticPropIndex( pUnk ) + 1;
	}
}
#endif

void CEngineTraceServer::SetTraceEntity( ICollideable *pCollideable, trace_t *pTrace )
{
	if ( !pTrace->DidHit() )
		return;

	IHandleEntity *pHandleEntity = pCollideable->GetEntityHandle();
	if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
	{
		pTrace->m_pEnt = (CBaseEntity*)(pHandleEntity);
	}
	else
	{
		// For static props, point to the world, hitbox is the prop index
		pTrace->m_pEnt = (CBaseEntity*)(sv.edicts->GetIServerEntity());
		pTrace->hitbox = StaticPropMgr()->GetStaticPropIndex( pHandleEntity ) + 1;
	}
}


//-----------------------------------------------------------------------------
// Traces a ray against a particular edict
//-----------------------------------------------------------------------------
void CEngineTrace::ClipRayToCollideable( const Ray_t &ray, unsigned int fMask, ICollideable *pEntity, trace_t *pTrace )
{
	CM_ClearTrace( pTrace );
	VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
	VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos );

	const model_t *pModel = pEntity->GetCollisionModel();
	bool bIsStudioModel = false;
	studiohdr_t *pStudioHdr = NULL;
	if ( pModel && pModel->type == mod_studio )
	{
		bIsStudioModel = true;
		pStudioHdr = (studiohdr_t *)modelloader->GetExtraData( (model_t*)pModel );
		// Cull if the collision mask isn't set + we're not testing hitboxes.
		if ( (( fMask & CONTENTS_HITBOX ) == 0) )
		{
			if ( ( fMask & pStudioHdr->contents ) == 0)
				return;
		}
	}

	const matrix3x4_t *pOldRoot = m_pRootMoveParent;
	if ( pEntity->GetSolidFlags() & FSOLID_ROOT_PARENT_ALIGNED )
	{
		m_pRootMoveParent = pEntity->GetRootParentToWorldTransform();
	}
	bool bTraced = false;
	bool bCustomPerformed = false;
	if ( ShouldPerformCustomRayTest( ray, pEntity ) )
	{
		ClipRayToCustom( ray, fMask, pEntity, pTrace );
		bTraced = true;
		bCustomPerformed = true;
	}
	else
	{
		bTraced = ClipRayToVPhysics( ray, fMask, pEntity, pStudioHdr, pTrace );	
	}

	// FIXME: Why aren't we using solid type to check what kind of collisions to test against?!?!
	if ( !bTraced && pModel && pModel->type == mod_brush )
	{
		bTraced = ClipRayToBSP( ray, fMask, pEntity, pTrace );
	}

	if ( !bTraced )
	{
		bTraced = ClipRayToOBB( ray, fMask, pEntity, pTrace );
	}

	// Hitboxes..
	bool bTracedHitboxes = false;
	if ( bIsStudioModel && (fMask & CONTENTS_HITBOX) )
	{
		// Until hitboxes are no longer implemented as custom raytests,
		// don't bother to do the work twice
		if (!bCustomPerformed)
		{
			bTraced = ClipRayToHitboxes( ray, fMask, pEntity, pTrace );
			if ( bTraced )
			{
				// Hitboxes will set the surface properties
				bTracedHitboxes = true;
			}
		}
	}

	if ( !bTraced )
	{
		ClipRayToBBox( ray, fMask, pEntity, pTrace );
	}

	if ( bIsStudioModel && !bTracedHitboxes && pTrace->DidHit() && (!bCustomPerformed || pTrace->surface.surfaceProps == 0) )
	{
		pTrace->contents = pStudioHdr->contents;
		// use the default surface properties
		pTrace->surface.name = "**studio**";
		pTrace->surface.flags = 0;
		pTrace->surface.surfaceProps = physprop->GetSurfaceIndex( pStudioHdr->pszSurfaceProp() );
	}

	if (!pTrace->m_pEnt && pTrace->DidHit())
	{
		SetTraceEntity( pEntity, pTrace );
	}

#ifdef _DEBUG
	Vector vecOffset, vecEndTest;
	VectorAdd( ray.m_Start, ray.m_StartOffset, vecOffset );
	VectorMA( vecOffset, pTrace->fractionleftsolid, ray.m_Delta, vecEndTest );
	Assert( VectorsAreEqual( vecEndTest, pTrace->startpos, 0.1f ) );
	VectorMA( vecOffset, pTrace->fraction, ray.m_Delta, vecEndTest );
	Assert( VectorsAreEqual( vecEndTest, pTrace->endpos, 0.1f ) );
#endif
	m_pRootMoveParent = pOldRoot;
}


//-----------------------------------------------------------------------------
// Main entry point for clipping rays to entities
//-----------------------------------------------------------------------------
void CEngineTrace::ClipRayToEntity( const Ray_t &ray, unsigned int fMask, IHandleEntity *pEntity, trace_t *pTrace )
{
	ClipRayToCollideable( ray, fMask, GetCollideable(pEntity), pTrace );
}


//-----------------------------------------------------------------------------
// Grabs all entities along a ray
//-----------------------------------------------------------------------------
class CEntitiesAlongRay : public IPartitionEnumerator
{
public:
	CEntitiesAlongRay( ) : m_EntityHandles(0, 32) {}

	void Reset()
	{
		m_EntityHandles.RemoveAll();
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		m_EntityHandles.AddToTail( pHandleEntity );
		return ITERATION_CONTINUE;
	}

	CUtlVector< IHandleEntity * >	m_EntityHandles;
};

class CEntityListAlongRay : public IPartitionEnumerator
{
public:

	enum { MAX_ENTITIES_ALONGRAY = 1024 };

	CEntityListAlongRay() 
	{
		m_nCount = 0;
	}

	void Reset()
	{
		m_nCount = 0;
	}

	int Count()
	{
		return m_nCount;
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		if ( m_nCount < MAX_ENTITIES_ALONGRAY )
		{
			m_EntityHandles[m_nCount] = pHandleEntity;
			m_nCount++;
		}
		else
		{
			DevMsg( 1, "Max entity count along ray exceeded!\n" );
		}

		return ITERATION_CONTINUE;
	}

	int m_nCount;
	IHandleEntity	*m_EntityHandles[MAX_ENTITIES_ALONGRAY];
};

//-----------------------------------------------------------------------------
// Makes sure the final trace is clipped to the clip trace
// Returns true if clipping occurred
//-----------------------------------------------------------------------------
bool CEngineTrace::ClipTraceToTrace( trace_t &clipTrace, trace_t *pFinalTrace )
{
	if (clipTrace.allsolid || clipTrace.startsolid || (clipTrace.fraction < pFinalTrace->fraction))
	{
		if (pFinalTrace->startsolid)
		{
			float flFractionLeftSolid = pFinalTrace->fractionleftsolid;
			Vector vecStartPos = pFinalTrace->startpos;

			*pFinalTrace = clipTrace;
			pFinalTrace->startsolid = true;

			if ( flFractionLeftSolid > clipTrace.fractionleftsolid )
			{
				pFinalTrace->fractionleftsolid = flFractionLeftSolid;
				pFinalTrace->startpos = vecStartPos;
			}
		}
		else
		{
			*pFinalTrace = clipTrace;
		}
		return true;
	}

	if (clipTrace.startsolid)
	{
		pFinalTrace->startsolid = true;

		if ( clipTrace.fractionleftsolid > pFinalTrace->fractionleftsolid )
		{
			pFinalTrace->fractionleftsolid = clipTrace.fractionleftsolid;
			pFinalTrace->startpos = clipTrace.startpos;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Converts a user id to a collideable + username
//-----------------------------------------------------------------------------
void CEngineTraceServer::HandleEntityToCollideable( IHandleEntity *pHandleEntity, ICollideable **ppCollide, const char **ppDebugName )
{
	*ppCollide = StaticPropMgr()->GetStaticProp( pHandleEntity );
	if ( *ppCollide	)
	{
		*ppDebugName = "static prop";
		return;
	}

	IServerUnknown *pServerUnknown = static_cast<IServerUnknown*>(pHandleEntity);
	if ( !pServerUnknown || ! pServerUnknown->GetNetworkable())
	{
		*ppCollide = NULL;
		*ppDebugName = "<null>";
		return;
	}

	*ppCollide = pServerUnknown->GetCollideable();
	*ppDebugName = pServerUnknown->GetNetworkable()->GetClassName();
}

#ifndef SWDS
void CEngineTraceClient::HandleEntityToCollideable( IHandleEntity *pHandleEntity, ICollideable **ppCollide, const char **ppDebugName )
{
	*ppCollide = StaticPropMgr()->GetStaticProp( pHandleEntity );
	if ( *ppCollide	)
	{
		*ppDebugName = "static prop";
		return;
	}

	IClientUnknown *pUnk = static_cast<IClientUnknown*>(pHandleEntity);
	if ( !pUnk )
	{
		*ppCollide = NULL;
		*ppDebugName = "<null>";
		return;
	}
	
	*ppCollide = pUnk->GetCollideable();
	*ppDebugName = "client entity";
	IClientNetworkable *pNetwork = pUnk->GetClientNetworkable();
	if (pNetwork)
	{
		if (pNetwork->GetClientClass())
		{
			*ppDebugName = pNetwork->GetClientClass()->m_pNetworkName;
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Returns the world collideable for trace setting
//-----------------------------------------------------------------------------
#ifndef SWDS
ICollideable *CEngineTraceClient::GetWorldCollideable()
{
	IClientEntity *pUnk = entitylist->GetClientEntity( 0 );
	AssertOnce( pUnk );
	return pUnk ? pUnk->GetCollideable() : NULL;
}
#endif

ICollideable *CEngineTraceServer::GetWorldCollideable()
{
	if (!sv.edicts)
		return NULL;
	return sv.edicts->GetCollideable();
}


//-----------------------------------------------------------------------------
// Debugging code to render all ray casts since the last time this call was made
//-----------------------------------------------------------------------------
void EngineTraceRenderRayCasts()
{
#if defined _DEBUG && !defined SWDS
	if( debugrayenable.GetBool() && s_FrameRays.Count() > debugraylimit.GetInt() && !debugrayreset.GetInt() )
	{
		Warning( "m_FrameRays.Count() == %d\n", s_FrameRays.Count() );
		debugrayreset.SetValue( 1 );
		int i;
		for( i = 0; i < s_FrameRays.Count(); i++ )
		{
			Ray_t &ray = s_FrameRays[i];
			if( ray.m_Extents.x != 0.0f || ray.m_Extents.y != 0.0f || ray.m_Extents.z != 0.0f )
			{
				CDebugOverlay::AddLineOverlay( ray.m_Start, ray.m_Start + ray.m_Delta, 255, 0, 0, 255, true, 3600.0f );
			}
			else
			{
				CDebugOverlay::AddLineOverlay( ray.m_Start, ray.m_Start + ray.m_Delta, 255, 255, 0, 255, true, 3600.0f );
			}
		}
	}

	s_FrameRays.RemoveAll( );
#endif
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEngineTrace::SetupLeafAndEntityListRay( const Ray_t &ray, CTraceListData &traceData )
{
	if ( !ray.m_IsSwept )
	{
		Vector vecMin, vecMax;
		VectorSubtract( ray.m_Start, ray.m_Extents, vecMin );
		VectorAdd( ray.m_Start, ray.m_Extents, vecMax );
		SetupLeafAndEntityListBox( vecMin, vecMax, traceData );
		return;
	}

	// Get the leaves that intersect the ray.
	traceData.LeafCountReset();
	CM_RayLeafnums( ray, traceData.m_aLeafList.Base(), traceData.LeafCountMax(), traceData.m_nLeafCount ); 

	// Find all the entities in the voxels that intersect this ray.
	traceData.EntityCountReset();
	SpatialPartition()->EnumerateElementsAlongRay( SpatialPartitionMask(), ray, false, &traceData );
}

//-----------------------------------------------------------------------------
// Purpose: Gives an AABB and returns a leaf and entity list.
//-----------------------------------------------------------------------------
void CEngineTrace::SetupLeafAndEntityListBox( const Vector &vecBoxMin, const Vector &vecBoxMax, CTraceListData &traceData )
{
	// Get the leaves that intersect this box.
	int iTopNode = -1;
	traceData.LeafCountReset();
	traceData.m_nLeafCount = CM_BoxLeafnums( vecBoxMin, vecBoxMax, traceData.m_aLeafList.Base(), traceData.LeafCountMax(), &iTopNode );
	
	// Find all entities in the voxels that intersect this box.
	traceData.EntityCountReset();
	SpatialPartition()->EnumerateElementsInBox( SpatialPartitionMask(), vecBoxMin, vecBoxMax, false, &traceData );
}

//-----------------------------------------------------------------------------
// Purpose:
// NOTE: the fMask is redundant with the stuff below, what do I want to do???
//-----------------------------------------------------------------------------
void CEngineTrace::TraceRayAgainstLeafAndEntityList( const Ray_t &ray, CTraceListData &traceData,
										             unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	// Setup the trace data.
	CM_ClearTrace ( pTrace );

	// Make sure we have some kind of trace filter.
	CTraceFilterHitAll traceFilter;
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilter;
	}

	// Collide with the world.
	if ( pTraceFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
	{
		ICollideable *pCollide = GetWorldCollideable();

		// Make sure the world entity is unrotated
		// FIXME: BAH! The !pCollide test here is because of
		// CStaticProp::PrecacheLighting.. it's occurring too early
		// need to fix that later
		Assert( !pCollide || pCollide->GetCollisionOrigin() == vec3_origin );
		Assert( !pCollide || pCollide->GetCollisionAngles() == vec3_angle );

		CM_BoxTraceAgainstLeafList( ray, traceData.m_aLeafList.Base(), traceData.LeafCount(), fMask, true, *pTrace );
		SetTraceEntity( pCollide, pTrace );

		// Blocked by the world or early out because we only are tracing against the world.
		if ( ( pTrace->fraction == 0 ) || ( pTraceFilter->GetTraceType() == TRACE_WORLD_ONLY ) )
			return;
	}
	else
	{
		// Set initial start and endpos.  This is necessary if the world isn't traced against,
		// because we may not trace against anything below.
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos );
	}

	// Save the world collision fraction.
	float flWorldFraction = pTrace->fraction;

	// Create a ray that extends only until we hit the world and adjust the trace accordingly
	Ray_t entityRay = ray;
	VectorScale( entityRay.m_Delta, pTrace->fraction, entityRay.m_Delta );

	// We know this is safe because if pTrace->fraction == 0, we would have exited above.
	pTrace->fractionleftsolid /= pTrace->fraction;
 	pTrace->fraction = 1.0;

	// Collide with entities.
	bool bNoStaticProps = pTraceFilter->GetTraceType() == TRACE_ENTITIES_ONLY;
	bool bFilterStaticProps = pTraceFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS;

	trace_t trace;
	ICollideable *pCollideable;
	const char *pDebugName;
	for ( int iEntity = 0; iEntity < traceData.m_nEntityCount; ++iEntity )
	{
		// Generate a collideable.
		IHandleEntity *pHandleEntity = traceData.m_aEntityList[iEntity];
		HandleEntityToCollideable( pHandleEntity, &pCollideable, &pDebugName );

		// Check for error condition.
		if ( !IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
		{
			Assert( 0 );
			Msg("%s in solid list (not solid)\n", pDebugName );
			continue;
		}

		if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
		{
			if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
				continue;
		}
		else
		{
			// FIXME: Could remove this check here by
			// using a different spatial partition mask. Look into it
			// if we want more speedups here.
			if ( bNoStaticProps )
				continue;

			if ( bFilterStaticProps )
			{
				if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
					continue;
			}
		}

		ClipRayToCollideable( entityRay, fMask, pCollideable, &trace );

		// Make sure the ray is always shorter than it currently is
		ClipTraceToTrace( trace, pTrace );

		// Stop if we're in allsolid
		if ( pTrace->allsolid )
			break;
	}

	// Fix up the fractions so they are appropriate given the original unclipped-to-world ray.
	pTrace->fraction *= flWorldFraction;
	pTrace->fractionleftsolid *= flWorldFraction;

	if ( !ray.m_IsRay )
	{
		// Make sure no fractionleftsolid can be used with box sweeps.
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		pTrace->fractionleftsolid = 0;
	}
}

#if BENCHMARK_RAY_TEST

CON_COMMAND( ray_save, "Save the rays" )
{
	int count = s_BenchmarkRays.Count();
	if ( count )
	{
		FileHandle_t hFile = g_pFileSystem->Open("rays.bin", "wb");
		if ( hFile )
		{
			g_pFileSystem->Write( &count, sizeof(count), hFile );
			g_pFileSystem->Write( s_BenchmarkRays.Base(), sizeof(s_BenchmarkRays[0])*count, hFile );
			g_pFileSystem->Close( hFile );
		}
	}

	Msg("Saved %d rays\n", count );
}

CON_COMMAND( ray_load, "Load the rays" )
{
	s_BenchmarkRays.RemoveAll();
	FileHandle_t hFile = g_pFileSystem->Open("rays.bin", "rb");
	if ( hFile )
	{
		int count = 0;
		g_pFileSystem->Read( &count, sizeof(count), hFile );
		if ( count )
		{
			s_BenchmarkRays.EnsureCount( count );
			g_pFileSystem->Read( s_BenchmarkRays.Base(), sizeof(s_BenchmarkRays[0])*count, hFile );
		}
		g_pFileSystem->Close( hFile );
	}

	Msg("Loaded %d rays\n", s_BenchmarkRays.Count() );
}

CON_COMMAND( ray_clear, "Clear the current rays" )
{
	s_BenchmarkRays.RemoveAll();
	Msg("Reset rays!\n");
}


CON_COMMAND_EXTERN( ray_bench, RayBench, "Time the rays" )
{
#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.Start();
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.ResetPeaks();
#endif
	{
		double tStart = Plat_FloatTime();
		trace_t trace;
		int hit = 0;
		int miss = 0;
		int rayVsProp = 0;
		int boxVsProp = 0;
		for ( int i = 0; i < s_BenchmarkRays.Count(); i++ )
		{
			CM_BoxTrace( s_BenchmarkRays[i], 0, MASK_SOLID, true, trace );
			if ( 0 )
			{
				VPROF("QueryStaticProps");
				// Create a ray that extends only until we hit the world and adjust the trace accordingly
				Ray_t entityRay = s_BenchmarkRays[i];
				VectorScale( entityRay.m_Delta, trace.fraction, entityRay.m_Delta );
				CEntityListAlongRay enumerator;
				enumerator.Reset();
				SpatialPartition()->EnumerateElementsAlongRay( PARTITION_ENGINE_SOLID_EDICTS, entityRay, false, &enumerator );
				trace_t tr;
				ICollideable *pCollideable;
				int nCount = enumerator.Count();
				const char *pDebugName = NULL;
				//float flWorldFraction = trace.fraction;
				if ( 0 )
				{

					VPROF("IntersectStaticProps");
				for ( int i = 0; i < nCount; ++i )
				{
					// Generate a collideable
					IHandleEntity *pHandleEntity = enumerator.m_EntityHandles[i];

					if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
						continue;
					if ( entityRay.m_IsRay )
						rayVsProp++;
					else
						boxVsProp++;
					s_EngineTraceServer.HandleEntityToCollideable( pHandleEntity, &pCollideable, &pDebugName );
					s_EngineTraceServer.ClipRayToCollideable( entityRay, MASK_SOLID, pCollideable, &tr );

					// Make sure the ray is always shorter than it currently is
					s_EngineTraceServer.ClipTraceToTrace( tr, &trace );
				}
				}
			}
			if ( trace.DidHit() )
				hit++;
			else
				miss++;
#if VPROF_LEVEL > 0 
			g_VProfCurrentProfile.MarkFrame();
#endif
		}
		double tEnd = Plat_FloatTime();
		float ms = (tEnd - tStart) * 1000.0f;
		int swept = 0;
		int point = 0;
		for ( int i = 0; i < s_BenchmarkRays.Count(); i++ )
		{
			swept += s_BenchmarkRays[i].m_IsSwept ? 1 : 0;
			point += s_BenchmarkRays[i].m_IsRay ? 1 : 0;
		}
		Msg("RAY TEST: %d hits, %d misses, %.2fms   (%d rays, %d sweeps) (%d ray/prop, %d box/prop)\n", hit, miss, ms, point, swept, rayVsProp, boxVsProp );
	}
#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.MarkFrame();
	g_VProfCurrentProfile.Stop();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
#endif
}
#endif

//-----------------------------------------------------------------------------
// A version that simply accepts a ray (can work as a traceline or tracehull)
//-----------------------------------------------------------------------------
void CEngineTrace::TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{	
#if defined _DEBUG && !defined SWDS
	if( debugrayenable.GetBool() )
	{
		s_FrameRays.AddToTail( ray );
	}
#endif

#if BENCHMARK_RAY_TEST
	if( s_BenchmarkRays.Count() < 15000 )
	{
		s_BenchmarkRays.EnsureCapacity(15000);
		s_BenchmarkRays.AddToTail( ray );
	}
#endif

	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s:%d", __FUNCTION__, __LINE__ );
	VPROF_INCREMENT_COUNTER( "TraceRay", 1 );
	m_traceStatCounters[TRACE_STAT_COUNTER_TRACERAY]++;
//	VPROF_BUDGET( "CEngineTrace::TraceRay", "Ray/Hull Trace" );
	
	CTraceFilterHitAll traceFilter;
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilter;
	}

	CM_ClearTrace( pTrace );

	// Collide with the world.
	if ( pTraceFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
	{
		ICollideable *pCollide = GetWorldCollideable();
		Assert( pCollide );

		// Make sure the world entity is unrotated
		// FIXME: BAH! The !pCollide test here is because of
		// CStaticProp::PrecacheLighting.. it's occurring too early
		// need to fix that later
		Assert(!pCollide || pCollide->GetCollisionOrigin() == vec3_origin );
		Assert(!pCollide || pCollide->GetCollisionAngles() == vec3_angle );

		CM_BoxTrace( ray, 0, fMask, true, *pTrace );
		SetTraceEntity( pCollide, pTrace );

		// inside world, no need to check being inside anything else
		if ( pTrace->startsolid )
			return;

		// Early out if we only trace against the world
		if ( pTraceFilter->GetTraceType() == TRACE_WORLD_ONLY )
			return;
	}
	else
	{
		// Set initial start + endpos, necessary if the world isn't traced against 
		// because we may not trace against *anything* below.
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		VectorAdd( pTrace->startpos, ray.m_Delta, pTrace->endpos );
	}

	// Save the world collision fraction.
	float flWorldFraction = pTrace->fraction;
	float flWorldFractionLeftSolidScale = flWorldFraction;

	// Create a ray that extends only until we hit the world
	// and adjust the trace accordingly
	Ray_t entityRay = ray;

	if ( pTrace->fraction == 0 )
	{
		entityRay.m_Delta.Init();
		flWorldFractionLeftSolidScale = pTrace->fractionleftsolid;
		pTrace->fractionleftsolid = 1.0f;
		pTrace->fraction = 1.0f;
	}
	else
	{
		// Explicitly compute end so that this computation happens at the quantization of
		// the output (endpos).  That way we won't miss any intersections we would get
		// by feeding these results back in to the tracer
		// This is not the same as entityRay.m_Delta *= pTrace->fraction which happens 
		// at a quantization that is more precise as m_Start moves away from the origin
		Vector end;
		VectorMA( entityRay.m_Start, pTrace->fraction, entityRay.m_Delta, end );
		VectorSubtract(end, entityRay.m_Start, entityRay.m_Delta);
		// We know this is safe because pTrace->fraction != 0
		pTrace->fractionleftsolid /= pTrace->fraction;
		pTrace->fraction = 1.0;
	}

	// Collide with entities along the ray
	// FIXME: Hitbox code causes this to be re-entrant for the IK stuff.
	// If we could eliminate that, this could be static and therefore
	// not have to reallocate memory all the time
	CEntityListAlongRay enumerator;
	enumerator.Reset();
	SpatialPartition()->EnumerateElementsAlongRay( SpatialPartitionMask(), entityRay, false, &enumerator );

	bool bNoStaticProps = pTraceFilter->GetTraceType() == TRACE_ENTITIES_ONLY;
	bool bFilterStaticProps = pTraceFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS;

	trace_t tr;
	ICollideable *pCollideable;
	const char *pDebugName;
	int nCount = enumerator.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// Generate a collideable
		IHandleEntity *pHandleEntity = enumerator.m_EntityHandles[i];
		HandleEntityToCollideable( pHandleEntity, &pCollideable, &pDebugName );

		// Check for error condition
		if ( IsPC() && IsDebug() && !IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
		{
			Assert( 0 );
			Msg( "%s in solid list (not solid)\n", pDebugName );
			continue;
		}

		if ( !StaticPropMgr()->IsStaticProp( pHandleEntity ) )
		{
			if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
				continue;
		}
		else
		{
			// FIXME: Could remove this check here by
			// using a different spatial partition mask. Look into it
			// if we want more speedups here.
			if ( bNoStaticProps )
				continue;

			if ( bFilterStaticProps )
			{
				if ( !pTraceFilter->ShouldHitEntity( pHandleEntity, fMask ) )
					continue;
			}
		}

		ClipRayToCollideable( entityRay, fMask, pCollideable, &tr );

		// Make sure the ray is always shorter than it currently is
		ClipTraceToTrace( tr, pTrace );

		// Stop if we're in allsolid
		if (pTrace->allsolid)
			break;
	}

	// Fix up the fractions so they are appropriate given the original
	// unclipped-to-world ray
	pTrace->fraction *= flWorldFraction;
	pTrace->fractionleftsolid *= flWorldFractionLeftSolidScale;

#ifdef _DEBUG
	Vector vecOffset, vecEndTest;
	VectorAdd( ray.m_Start, ray.m_StartOffset, vecOffset );
	VectorMA( vecOffset, pTrace->fractionleftsolid, ray.m_Delta, vecEndTest );
	Assert( VectorsAreEqual( vecEndTest, pTrace->startpos, 0.1f ) );
	VectorMA( vecOffset, pTrace->fraction, ray.m_Delta, vecEndTest );
	Assert( VectorsAreEqual( vecEndTest, pTrace->endpos, 0.1f ) );
//	Assert( !ray.m_IsRay || pTrace->allsolid || pTrace->fraction >= pTrace->fractionleftsolid );
#endif

	if ( !ray.m_IsRay )
	{
		// Make sure no fractionleftsolid can be used with box sweeps
		VectorAdd( ray.m_Start, ray.m_StartOffset, pTrace->startpos );
		pTrace->fractionleftsolid = 0;

#ifdef _DEBUG
		pTrace->fractionleftsolid = VEC_T_NAN;
#endif
	}
}


//-----------------------------------------------------------------------------
// A version that sweeps a collideable through the world
//-----------------------------------------------------------------------------
void CEngineTrace::SweepCollideable( ICollideable *pCollide, 
		const Vector &vecAbsStart, const Vector &vecAbsEnd, const QAngle &vecAngles,
		unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace )
{
	const matrix3x4_t *pOldRoot = m_pRootMoveParent;
	Ray_t ray;
	Assert( vecAngles == vec3_angle );
	if ( pCollide->GetSolidFlags() & FSOLID_ROOT_PARENT_ALIGNED )
	{
		m_pRootMoveParent = pCollide->GetRootParentToWorldTransform();
	}
	ray.Init( vecAbsStart, vecAbsEnd, pCollide->OBBMins(), pCollide->OBBMaxs() );
	TraceRay( ray, fMask, pTraceFilter, pTrace );
	m_pRootMoveParent = pOldRoot;
}


//-----------------------------------------------------------------------------
// Lets clients know about all edicts along a ray
//-----------------------------------------------------------------------------
class CEnumerationFilter : public IPartitionEnumerator
{
public:
	CEnumerationFilter( CEngineTrace *pEngineTrace, IEntityEnumerator* pEnumerator ) : 
		m_pEngineTrace(pEngineTrace), m_pEnumerator(pEnumerator) {}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		// Don't enumerate static props
		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
			return ITERATION_CONTINUE;

		if ( !m_pEnumerator->EnumEntity( pHandleEntity ) )
		{
			return ITERATION_STOP;
		}

		return ITERATION_CONTINUE;
	}

private:
	IEntityEnumerator* m_pEnumerator;
	CEngineTrace *m_pEngineTrace;
};


//-----------------------------------------------------------------------------
// Enumerates over all entities along a ray
// If triggers == true, it enumerates all triggers along a ray
//-----------------------------------------------------------------------------
void CEngineTrace::EnumerateEntities( const Ray_t &ray, bool bTriggers, IEntityEnumerator *pEnumerator )
{
	m_traceStatCounters[TRACE_STAT_COUNTER_ENUMERATE]++;
	// FIXME: If we store CBaseHandles directly in the spatial partition, this method
	// basically becomes obsolete. The spatial partition can be queried directly.
	CEnumerationFilter enumerator( this, pEnumerator );

	int fMask = !bTriggers ? SpatialPartitionMask() : SpatialPartitionTriggerMask();

	// NOTE: Triggers currently don't exist on the client
	if (fMask)
	{
		SpatialPartition()->EnumerateElementsAlongRay( fMask, ray, false, &enumerator );
	}
}


//-----------------------------------------------------------------------------
// Lets clients know about all entities in a box
//-----------------------------------------------------------------------------
void CEngineTrace::EnumerateEntities( const Vector &vecAbsMins, const Vector &vecAbsMaxs, IEntityEnumerator *pEnumerator )
{
	m_traceStatCounters[TRACE_STAT_COUNTER_ENUMERATE]++;
	// FIXME: If we store CBaseHandles directly in the spatial partition, this method
	// basically becomes obsolete. The spatial partition can be queried directly.
	CEnumerationFilter enumerator( this, pEnumerator );
	SpatialPartition()->EnumerateElementsInBox( SpatialPartitionMask(),
		vecAbsMins, vecAbsMaxs, false, &enumerator );
}


class CEntList : public IEntityEnumerator
{
public:
	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		IServerUnknown *pNetEntity = static_cast<IServerUnknown*>(pHandleEntity);
		ICollideable *pCollide = pNetEntity->GetCollideable();
		if ( !pCollide )
			return true;

		Vector vecCenter;
		VectorMA( MainViewOrigin(), 100.0f, MainViewForward(), vecCenter );
		float flDist = (vecCenter - pCollide->GetCollisionOrigin()).LengthSqr();
		if (flDist < m_flClosestDist)
		{
			m_flClosestDist = flDist;
			m_pClosest = pCollide;
		}

		return true;
	}

	ICollideable *m_pClosest;
	float m_flClosestDist;


};

#ifdef _DEBUG

	//-----------------------------------------------------------------------------
	// A method to test out sweeps
	//-----------------------------------------------------------------------------
	CON_COMMAND( test_sweepaabb, "method to test out sweeps" )
	{
		Vector vecStartPoint;
		VectorMA( MainViewOrigin(), 50.0f, MainViewForward(), vecStartPoint );

		Vector endPoint;
		VectorMA( MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(), endPoint );

		Ray_t ray;
		ray.Init( vecStartPoint, endPoint );

		trace_t tr;
	//	CTraceFilterHitAll traceFilter;
	//	g_pEngineTraceServer->TraceRay( ray, MASK_ALL, &traceFilter, &tr );

		CEntList list;
		list.m_pClosest = NULL;
		list.m_flClosestDist = FLT_MAX;
		g_pEngineTraceServer->EnumerateEntities( MainViewOrigin() - Vector( 200, 200, 200 ), MainViewOrigin() + Vector( 200, 200, 200 ), &list );

		if ( !list.m_pClosest )
			return;

		// Visualize the intersection test
		ICollideable *pCollide = list.m_pClosest;
		if ( pCollide->GetCollisionOrigin() == vec3_origin )
			return;

		QAngle test( 0, 45, 0 );
	#ifndef SWDS
		CDebugOverlay::AddBoxOverlay( pCollide->GetCollisionOrigin(),
			pCollide->OBBMins(), pCollide->OBBMaxs(),
			test /*pCollide->GetCollisionAngles()*/, 0, 0, 255, 128, 5.0f );
	#endif

		VectorMA( MainViewOrigin(), 200.0f, MainViewForward(), endPoint );
		ray.Init( vecStartPoint, endPoint, Vector( -10, -20, -10 ), Vector( 30, 30, 20 ) );

		bool bIntersect = IntersectRayWithOBB( ray, pCollide->GetCollisionOrigin(), test, pCollide->OBBMins(),
			pCollide->OBBMaxs(), 0.0f, &tr );
		unsigned char r, g, b, a;
		b = 0;
		a = 255;
		r = bIntersect ? 255 : 0;
		g = bIntersect ? 0 : 255;

	#ifndef SWDS
		CDebugOverlay::AddSweptBoxOverlay( tr.startpos, tr.endpos,
			Vector( -10, -20, -10 ), Vector( 30, 30, 20 ), vec3_angle, r, g, b, a, 5.0 );
	#endif
	}


#endif
