//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "server_pch.h"
#include <utllinkedlist.h>

#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "framesnapshot.h"
#include "sys_dll.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR( CFrameSnapshot, 64, 64 );


static ConVar sv_creationtickcheck( "sv_creationtickcheck", "1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Do extended check for encoding of timestamps against tickcount" );
extern	CGlobalVars g_ServerGlobalVariables;

// Expose interface
static CFrameSnapshotManager g_FrameSnapshotManager;
CFrameSnapshotManager *framesnapshotmanager = &g_FrameSnapshotManager;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFrameSnapshotManager::CFrameSnapshotManager( void ) : m_PackedEntitiesPool( MAX_EDICTS / 16, CUtlMemoryPool::GROW_SLOW )
{
	COMPILE_TIME_ASSERT( INVALID_PACKED_ENTITY_HANDLE == 0 );
	Q_memset( m_pPackedData, 0x00, MAX_EDICTS * sizeof(PackedEntityHandle_t) );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFrameSnapshotManager::~CFrameSnapshotManager( void )
{
	AssertMsg1( m_FrameSnapshots.Count() == 0 || IsInErrorExit(), "Expected m_FrameSnapshots to be empty. It had %i items.", m_FrameSnapshots.Count() );

	// TODO: This assert has been failing. HenryG says it's a valid assert and that we're probably leaking memory.
	AssertMsg1( m_PackedEntitiesPool.Count() == 0 || IsInErrorExit(), "Expected m_PackedEntitiesPool to be empty. It had %i items.", m_PackedEntitiesPool.Count() );
}

//-----------------------------------------------------------------------------
// Called when a level change happens
//-----------------------------------------------------------------------------

void CFrameSnapshotManager::LevelChanged()
{
	// Clear all lists...
	Assert( m_FrameSnapshots.Count() == 0 );

	// Release the most recent snapshot...
	m_PackedEntityCache.RemoveAll();
	COMPILE_TIME_ASSERT( INVALID_PACKED_ENTITY_HANDLE == 0 );
	Q_memset( m_pPackedData, 0x00, MAX_EDICTS * sizeof(PackedEntityHandle_t) );
}

CFrameSnapshot*	CFrameSnapshotManager::NextSnapshot( const CFrameSnapshot *pSnapshot )
{
	if ( !pSnapshot || ((unsigned short)pSnapshot->m_ListIndex == m_FrameSnapshots.InvalidIndex()) )
		return NULL;

	int next = m_FrameSnapshots.Next(pSnapshot->m_ListIndex);

	if ( next == m_FrameSnapshots.InvalidIndex() )
		return NULL;

	// return next element in list
	return m_FrameSnapshots[ next ];
}

CFrameSnapshot*	CFrameSnapshotManager::CreateEmptySnapshot( int tickcount, int maxEntities )
{
	CFrameSnapshot *snap = new CFrameSnapshot;
	snap->AddReference();
	snap->m_nTickCount = tickcount;
	snap->m_nNumEntities = maxEntities;
	snap->m_nValidEntities = 0;
	snap->m_pValidEntities = NULL;
	snap->m_pHLTVEntityData = NULL;
	snap->m_pReplayEntityData = NULL;
	snap->m_pEntities = new CFrameSnapshotEntry[maxEntities];

	CFrameSnapshotEntry *entry = snap->m_pEntities;
	
	// clear entries
	for ( int i=0; i < maxEntities; i++)
	{
		entry->m_pClass = NULL;
		entry->m_nSerialNumber = -1;
		entry->m_pPackedData = INVALID_PACKED_ENTITY_HANDLE;
		entry++;
	}

	snap->m_ListIndex = m_FrameSnapshots.AddToTail( snap );
	return snap;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : framenumber - 
//-----------------------------------------------------------------------------
CFrameSnapshot* CFrameSnapshotManager::TakeTickSnapshot( int tickcount )
{
	unsigned short nValidEntities[MAX_EDICTS];

	CFrameSnapshot *snap = CreateEmptySnapshot( tickcount, sv.num_edicts );
	
	int maxclients = sv.GetClientCount();

	CFrameSnapshotEntry *entry = snap->m_pEntities - 1;
	edict_t *edict= sv.edicts - 1;
	
	// Build the snapshot.
	for ( int i = 0; i < sv.num_edicts; i++ )
	{
		edict++;
		entry++;

		IServerUnknown *pUnk = edict->GetUnknown();

		if ( !pUnk )
			continue;
																		  
		if ( edict->IsFree() )
			continue;
		
		// We don't want entities from inactive clients in the fullpack,
		if ( i > 0 && i <= maxclients )
		{
			// this edict is a client
			if ( !sv.GetClient(i-1)->IsActive() )
				continue;
		}
		
		// entity exists and is not marked as 'free'
		Assert( edict->m_NetworkSerialNumber != -1 );
		Assert( edict->GetNetworkable() );
		Assert( edict->GetNetworkable()->GetServerClass() );

		entry->m_nSerialNumber	= edict->m_NetworkSerialNumber;
		entry->m_pClass			= edict->GetNetworkable()->GetServerClass();
		nValidEntities[snap->m_nValidEntities++] = i;
	}

	// create dynamic valid entities array and copy indices
	snap->m_pValidEntities = new unsigned short[snap->m_nValidEntities];
	Q_memcpy( snap->m_pValidEntities, nValidEntities, snap->m_nValidEntities * sizeof(unsigned short) );

	if ( hltv && hltv->IsActive() )
	{
		snap->m_pHLTVEntityData = new CHLTVEntityData[snap->m_nValidEntities];
		Q_memset( snap->m_pHLTVEntityData, 0, snap->m_nValidEntities * sizeof(CHLTVEntityData) );
	}

#if defined( REPLAY_ENABLED )
	if ( replay && replay->IsActive() )
	{
		snap->m_pReplayEntityData = new CReplayEntityData[snap->m_nValidEntities];
		Q_memset( snap->m_pReplayEntityData, 0, snap->m_nValidEntities * sizeof(CReplayEntityData) );
	}
#endif

	snap->m_iExplicitDeleteSlots.CopyArray( m_iExplicitDeleteSlots.Base(), m_iExplicitDeleteSlots.Count() );
	m_iExplicitDeleteSlots.Purge();

	return snap;
}

//-----------------------------------------------------------------------------
// Cleans up packed entity data
//-----------------------------------------------------------------------------

void CFrameSnapshotManager::DeleteFrameSnapshot( CFrameSnapshot* pSnapshot )
{
	// Decrement reference counts of all packed entities
	for (int i = 0; i < pSnapshot->m_nNumEntities; ++i)
	{
		if ( pSnapshot->m_pEntities[i].m_pPackedData != INVALID_PACKED_ENTITY_HANDLE )
		{
			RemoveEntityReference( pSnapshot->m_pEntities[i].m_pPackedData );
		}
	}

	m_FrameSnapshots.Remove( pSnapshot->m_ListIndex );
	delete pSnapshot;
}

void CFrameSnapshotManager::RemoveEntityReference( PackedEntityHandle_t handle )
{
	Assert( handle != INVALID_PACKED_ENTITY_HANDLE );

	PackedEntity *packedEntity = reinterpret_cast< PackedEntity * >( handle );

	if ( --packedEntity->m_ReferenceCount <= 0)
	{
		AUTO_LOCK( m_WriteMutex );

		m_PackedEntitiesPool.Free( packedEntity );

		// if we have a uncompression cache, remove reference too
		FOR_EACH_VEC( m_PackedEntityCache, i )
		{
			UnpackedDataCache_t &pdc = m_PackedEntityCache[i];
			if ( pdc.pEntity == packedEntity )
			{
				pdc.pEntity = NULL;
				pdc.counter = 0;
				break;
			}
		}
	}
}

void CFrameSnapshotManager::AddEntityReference( PackedEntityHandle_t handle )
{
	Assert( handle != INVALID_PACKED_ENTITY_HANDLE );
	reinterpret_cast< PackedEntity * >( handle )->m_ReferenceCount++;
}

void CFrameSnapshotManager::AddExplicitDelete( int iSlot )
{
	AUTO_LOCK( m_WriteMutex );

	if ( m_iExplicitDeleteSlots.Find(iSlot) == m_iExplicitDeleteSlots.InvalidIndex() )
	{
		m_iExplicitDeleteSlots.AddToTail( iSlot );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the "basis" for encoding m_flAnimTime, m_flSimulationTime has changed 
//  since the time this entity was packed to the time we're trying to re-use the packing.
//-----------------------------------------------------------------------------
bool CFrameSnapshotManager::ShouldForceRepack( CFrameSnapshot* pSnapshot, int entity, PackedEntityHandle_t handle )
{
	if ( sv_creationtickcheck.GetBool() )
	{
		PackedEntity *pe = reinterpret_cast< PackedEntity * >( handle );
		Assert( pe );
		if ( pe && pe->ShouldCheckCreationTick() )
		{
			int nCurrentNetworkBase			= g_ServerGlobalVariables.GetNetworkBase( pSnapshot->m_nTickCount, entity );
			int nPackedEntityNetworkBase	= g_ServerGlobalVariables.GetNetworkBase( pe->GetSnapshotCreationTick(), entity );
			if ( nCurrentNetworkBase != nPackedEntityNetworkBase )
			{
				return true;
			}
		}
	}

	return false;
}

bool CFrameSnapshotManager::UsePreviouslySentPacket( CFrameSnapshot* pSnapshot, 
											int entity, int entSerialNumber )
{
	PackedEntityHandle_t handle = m_pPackedData[entity]; 
	if ( handle != INVALID_PACKED_ENTITY_HANDLE )
	{
		// NOTE: We can't use the previously sent packet if there was a 
		// serial number change....
		if ( m_pSerialNumber[entity] == entSerialNumber )
		{
			// Check if we need to re-pack entity due to encoding against gpGlobals->tickcount
			if ( framesnapshotmanager->ShouldForceRepack( pSnapshot, entity, handle ) )
			{
				return false;
			}

			Assert( entity < pSnapshot->m_nNumEntities );
			pSnapshot->m_pEntities[entity].m_pPackedData = handle;
			reinterpret_cast< PackedEntity * >( handle )->m_ReferenceCount++;
			return true;
		}
		else
		{
			return false;
		}
	}
	
	return false;
}


PackedEntity* CFrameSnapshotManager::GetPreviouslySentPacket( int iEntity, int iSerialNumber )
{
	PackedEntityHandle_t handle = m_pPackedData[iEntity]; 
	if ( handle != INVALID_PACKED_ENTITY_HANDLE )
	{
		// NOTE: We can't use the previously sent packet if there was a 
		// serial number change....
		if ( m_pSerialNumber[iEntity] == iSerialNumber )
		{
			return reinterpret_cast< PackedEntity * >( handle );
		}
		else
		{
			return NULL;
		}
	}
	
	return NULL;
}

CThreadFastMutex &CFrameSnapshotManager::GetMutex()
{
	return m_WriteMutex;
}

//-----------------------------------------------------------------------------
// Returns the pack data for a particular entity for a particular snapshot
//-----------------------------------------------------------------------------

PackedEntity* CFrameSnapshotManager::CreatePackedEntity( CFrameSnapshot* pSnapshot, int entity )
{
	m_WriteMutex.Lock();
	PackedEntity *packedEntity = m_PackedEntitiesPool.Alloc();
	PackedEntityHandle_t handle = reinterpret_cast< PackedEntityHandle_t >( packedEntity );
	m_WriteMutex.Unlock();
	
	Assert( entity < pSnapshot->m_nNumEntities );

	// Referenced twice: in the mru 
	packedEntity->m_ReferenceCount = 2;
	packedEntity->m_nEntityIndex = entity;
	pSnapshot->m_pEntities[entity].m_pPackedData = handle;

	// Add a reference into the global list of last entity packets seen...
	// and remove the reference to the last entity packet we saw
	if (m_pPackedData[entity] != INVALID_PACKED_ENTITY_HANDLE )
	{
		RemoveEntityReference( m_pPackedData[entity] );
	}
	
	m_pPackedData[entity] = handle;
	m_pSerialNumber[entity] = pSnapshot->m_pEntities[entity].m_nSerialNumber;

	packedEntity->SetSnapshotCreationTick( pSnapshot->m_nTickCount );

	return packedEntity;
}

//-----------------------------------------------------------------------------
// Returns the pack data for a particular entity for a particular snapshot
//-----------------------------------------------------------------------------

PackedEntity* CFrameSnapshotManager::GetPackedEntity( CFrameSnapshot* pSnapshot, int entity )
{
	if ( !pSnapshot )
		return NULL;

	Assert( entity < pSnapshot->m_nNumEntities );
		
	PackedEntityHandle_t index = pSnapshot->m_pEntities[entity].m_pPackedData;

	if ( index == INVALID_PACKED_ENTITY_HANDLE )
		return NULL;

	PackedEntity *packedEntity = reinterpret_cast< PackedEntity * >( index );
	Assert( packedEntity->m_nEntityIndex == entity );
	return packedEntity;
}



// ------------------------------------------------------------------------------------------------ //
// purpose: lookup cache if we have an uncompressed version of this packed entity
// ------------------------------------------------------------------------------------------------ //
UnpackedDataCache_t *CFrameSnapshotManager::GetCachedUncompressedEntity( PackedEntity *packedEntity )
{
	if ( m_PackedEntityCache.Count() == 0 )
	{
		// ops, we have no cache yet, create one and reset counter
		m_nPackedEntityCacheCounter = 0;
		m_PackedEntityCache.SetCount( 128 );

		FOR_EACH_VEC( m_PackedEntityCache, i )
		{
			m_PackedEntityCache[i].pEntity = NULL;
			m_PackedEntityCache[i].counter = 0;
		}
	}

	m_nPackedEntityCacheCounter++;

	// remember oldest cache entry
	UnpackedDataCache_t *pdcOldest = NULL;
	int oldestValue = m_nPackedEntityCacheCounter;


	FOR_EACH_VEC( m_PackedEntityCache, i )
	{
		UnpackedDataCache_t *pdc = &m_PackedEntityCache[i];

		if ( pdc->pEntity == packedEntity )
		{
			// hit, found it, update counter
			pdc->counter = m_nPackedEntityCacheCounter;
			return pdc;
		}

		if( pdc->counter < oldestValue )
		{
			oldestValue = pdc->counter;
			pdcOldest = pdc;
		}
	}

	Assert ( pdcOldest );

	// hmm, not in cache, clear & return oldest one
	pdcOldest->counter = m_nPackedEntityCacheCounter;
	pdcOldest->bits = -1;	// important, this is the signal for the caller to fill this structure
	pdcOldest->pEntity = packedEntity;
	return pdcOldest;
}




// ------------------------------------------------------------------------------------------------ //
// CFrameSnapshot
// ------------------------------------------------------------------------------------------------ //

#if defined( _DEBUG )
	int g_nAllocatedSnapshots = 0;
#endif


CFrameSnapshot::CFrameSnapshot()
{
	m_nTempEntities = 0;
	m_pTempEntities = NULL;
	m_pValidEntities = NULL;
	m_nReferences = 0;
#if defined( _DEBUG )
	++g_nAllocatedSnapshots;
	Assert( g_nAllocatedSnapshots < 80000 ); // this probably would indicate a memory leak.
#endif
}


CFrameSnapshot::~CFrameSnapshot()
{
	delete [] m_pValidEntities;
	delete [] m_pEntities;

	if ( m_pTempEntities )
	{
		Assert( m_nTempEntities>0 );
		for (int i = 0; i < m_nTempEntities; i++ )
		{
			delete m_pTempEntities[i];
		}

		delete [] m_pTempEntities;
	}

	if ( m_pHLTVEntityData )
	{
		delete [] m_pHLTVEntityData;
	}

	if ( m_pReplayEntityData )
	{
		delete [] m_pReplayEntityData;
	}
	Assert ( m_nReferences == 0 );

#if defined( _DEBUG )
	--g_nAllocatedSnapshots;
	Assert( g_nAllocatedSnapshots >= 0 );
#endif
}


void CFrameSnapshot::AddReference()
{
	Assert( m_nReferences < 0xFFFF );
	++m_nReferences;
}

void CFrameSnapshot::ReleaseReference()
{
	Assert( m_nReferences > 0 );

	--m_nReferences;
	if ( m_nReferences == 0 )
	{
		g_FrameSnapshotManager.DeleteFrameSnapshot( this );
	}
}

CFrameSnapshot* CFrameSnapshot::NextSnapshot() const
{
	return g_FrameSnapshotManager.NextSnapshot( this );
}


