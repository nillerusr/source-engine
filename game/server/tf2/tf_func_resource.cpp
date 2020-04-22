//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "tf_player.h"
#include "tf_func_resource.h"
#include "tf_team.h"
#include "tf_basecombatweapon.h"
#include "gamerules.h"
#include "ammodef.h"
#include "tf_obj.h"
#include "resource_chunk.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "team_messages.h"
#include "tf_stats.h"

LINK_ENTITY_TO_CLASS( trigger_resourcezone, CResourceZone);

BEGIN_DATADESC( CResourceZone )

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_nResourcesLeft, FIELD_INTEGER, "ResourceAmount" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_iMaxChunks, FIELD_INTEGER, "ResourceChunks" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flResourceRate, FIELD_FLOAT, "ResourceRate" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flChunkValueMin, FIELD_FLOAT, "ChunkValueMin" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flChunkValueMax, FIELD_FLOAT, "ChunkValueMax" ),

	// inputs
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetAmount", InputSetAmount ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetAmount", InputResetAmount ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetActive", InputSetActive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetInactive", InputSetInactive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleActive", InputToggleActive ),

	// outputs
	DEFINE_OUTPUT( m_OnEmpty, "OnEmpty" ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CResourceZone, DT_ResourceZone)
	SendPropFloat( SENDINFO( m_flClientResources ),	8,	SPROP_UNSIGNED,	0.0f,	1.0f ),
	SendPropInt( SENDINFO( m_nResourcesLeft ),	20,	SPROP_UNSIGNED ),
END_SEND_TABLE();

PRECACHE_REGISTER( trigger_resourcezone );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CResourceZone::CResourceZone()
{
#ifdef _DEBUG
	m_vecGatherPoint.Init();
	m_angGatherPoint.Init();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the resource zone
//-----------------------------------------------------------------------------
void CResourceZone::Spawn( void )
{
	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetModel( STRING( GetModelName() ) );

	if ( !m_nResourcesLeft )
	{
		m_nResourcesLeft = 10000;
	}

	m_nMaxResources = m_nResourcesLeft;
	m_flClientResources = 1;

	SetActive( false );
	m_vecGatherPoint = WorldSpaceCenter();
	m_angGatherPoint = vec3_angle;
	m_iTeamGathering = -1;
	m_aSpawners.Purge();
	m_hResourcePump = NULL;

	if ( !m_iMaxChunks )
		m_iMaxChunks = 5;
	if ( !m_flChunkValueMin )
		m_flChunkValueMin = 20;
	if ( !m_flChunkValueMax )
		m_flChunkValueMax = 60;

	m_flBaseResourceRate = m_flResourceRate;
	
	m_flRespawnTimeModifier = 1.0f;

	m_flTestTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: See if we've got a gather point specified 
//-----------------------------------------------------------------------------
void CResourceZone::Activate( void )
{
	BaseClass::Activate();

	if (m_target != NULL_STRING)
	{
		CBaseEntity	*pEnt = gEntList.FindEntityByName( NULL, m_target );
		if ( pEnt )
		{
			m_vecGatherPoint = pEnt->GetLocalOrigin();
			m_angGatherPoint = pEnt->GetLocalAngles();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::InputSetAmount( inputdata_t &inputdata )
{
	m_nMaxResources = m_nResourcesLeft = inputdata.value.Int();
	RecomputeClientResources();

	// We may have just been reactivated
	if ( m_nResourcesLeft )
	{
		SetActive( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::InputResetAmount( inputdata_t &inputdata )
{
	m_nResourcesLeft = m_nMaxResources;
	m_flClientResources = 1;

	// We may have just been reactivated
	if ( m_nResourcesLeft )
	{
		SetActive( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::InputSetActive( inputdata_t &inputdata )
{
	SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::InputSetInactive( inputdata_t &inputdata )
{
	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::InputToggleActive( inputdata_t &inputdata )
{
	if ( GetActive() )
	{
		SetActive( false );
	}
	else
	{
		SetActive( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::SetActive( bool bActive )
{
	// Going active?
	if ( !m_bActive && bActive )
	{
		// Start our ambient sound
		//EmitAmbientSound( this, Center(), "ResourceZone.AmbientActiveSound" );
	}
	else if ( m_bActive && !bActive )
	{
		// Going inactive

		// Stop our sound loop
		//StopSound( "ResourceZone.AmbientActiveSound" );
	}

	m_bActive = bActive;

	// Tell all my spawners
	for ( int i = 0; i < m_aSpawners.Size(); i++ )
	{
		m_aSpawners[i]->SetActive( bActive );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CResourceZone::GetActive() const
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Zone increasing....
//-----------------------------------------------------------------------------
void CResourceZone::AddZoneIncreaser( float rate )
{
	Assert( rate != 0.0f );

	m_flRespawnTimeModifier *= rate;
	m_flResourceRate = m_flBaseResourceRate / m_flRespawnTimeModifier;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::RemoveZoneIncreaser( float rate )
{
	Assert( rate != 0.0f );

	m_flRespawnTimeModifier /= rate;
	m_flResourceRate = m_flBaseResourceRate / m_flRespawnTimeModifier;
}

//-----------------------------------------------------------------------------
// Purpose: Removes resources from the zone, and returns true if it's now empty
//-----------------------------------------------------------------------------
bool CResourceZone::RemoveResources( int nResourcesRemoved )
{
	if ( IsEmpty() )
		return true;

	m_nResourcesLeft = MAX(0, m_nResourcesLeft - nResourcesRemoved);
	RecomputeClientResources();

	// If I'm out of resources, destroy my resource spawners
	if ( IsEmpty() )
	{
		// Tell all existing chunks to stay forever
		int i;
		for ( i = 0; i < m_aChunks.Size(); i++ )
		{
			// Clear them removal think
			m_aChunks[i]->SetThink( NULL );
		}

		SetActive( false );
		
		// Tell teams about it
		for ( i = 0; i < GetNumberOfTeams(); i++ )
		{
			CTFTeam *pTeam = GetGlobalTFTeam( i );
			pTeam->PostMessage( TEAMMSG_RESOURCE_ZONE_EMPTIED );
		}

		// Fire my output
		m_OnEmpty.FireOutput( NULL,this );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this zone is empty
//-----------------------------------------------------------------------------
bool CResourceZone::IsEmpty( void )
{ 
	// Inactive zones pretend to be empty, so nothing tries to do anything with them
	if ( !GetActive() )
		return true;

	return (m_nResourcesLeft <= 0);
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified point is within this zone
//-----------------------------------------------------------------------------
bool CResourceZone::PointIsWithin( const Vector &vecPoint )
{
	return CollisionProp()->IsPointInBounds( vecPoint );
}

//-----------------------------------------------------------------------------
// Purpose: Resource zones have 1 build point
//-----------------------------------------------------------------------------
int CResourceZone::GetNumBuildPoints( void ) const
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified object type can be built on this point
//-----------------------------------------------------------------------------
bool CResourceZone::CanBuildObjectOnBuildPoint( int iPoint, int iObjectType )
{
	ASSERT( iPoint <= GetNumBuildPoints() );

	// Don't allow more than one pump
	if ( m_hResourcePump.Get() )
		return false;

	// Only pumps can be built on zones
	return ( iObjectType == OBJ_RESOURCEPUMP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CResourceZone::GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles )
{
	ASSERT( iPoint <= GetNumBuildPoints() );

	// If we have a gather point, return it
	if ( m_vecGatherPoint != WorldSpaceCenter() )
	{
		vecOrigin = m_vecGatherPoint;
	}
	else if ( m_aSpawners.Size() )
	{
		// Return the first resource spawner
		vecOrigin = m_aSpawners[0]->GetAbsOrigin();
	}
	else
	{
		vecOrigin = GetAbsOrigin();
	}

	vecAngles = QAngle(0,0,0);
	return true;
}

int CResourceZone::GetBuildPointAttachmentIndex( int iPoint ) const
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject )
{
	m_hResourcePump = pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CResourceZone::GetNumObjectsOnMe( void )
{
	if ( m_hResourcePump.Get() )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity	*CResourceZone::GetFirstObjectOnMe( void )
{
	return m_hResourcePump;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject *CResourceZone::GetObjectOfTypeOnMe( int iObjectType )
{
	if ( GetNumObjectsOnMe() == 1 )
	{
		CBaseObject *pObject = dynamic_cast<CBaseObject*>( m_hResourcePump.Get() );
		if ( pObject )
		{
			if ( pObject->GetType() == iObjectType )
				return pObject;
		}
	}

	return NULL;
}

int	CResourceZone::FindObjectOnBuildPoint( CBaseObject *pObject )
{
	if (m_hResourcePump == pObject)
		return 0;
	return -1;
}

void CResourceZone::GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	Assert(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::RemoveAllObjects( void )
{
	UTIL_Remove( m_hResourcePump );
}

//-----------------------------------------------------------------------------
// Purpose: Get the team that's gathering from this point
//-----------------------------------------------------------------------------
CTFTeam *CResourceZone::GetOwningTeam( void )
{
	if ( m_iTeamGathering == -1 )
		return NULL;

	return (CTFTeam*)GetGlobalTeam(m_iTeamGathering);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceZone::SetOwningTeam( int iTeamNumber )
{
	m_iTeamGathering = iTeamNumber;
}

//-----------------------------------------------------------------------------
// Purpose: Transmit this to all players who are in commander mode
//-----------------------------------------------------------------------------
int CResourceZone::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Team rules may tell us that we should
	CBaseEntity* pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
	Assert( pRecipientEntity->IsPlayer() );
	
	CBasePlayer *pPlayer = (CBasePlayer*)pRecipientEntity;
	if ( pPlayer->GetTeam() )
	{
		if (pPlayer->GetTeam()->ShouldTransmitToPlayer( pPlayer, this ))
			return FL_EDICT_ALWAYS;
	}

	return FL_EDICT_DONTSEND;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we should create any more resource chunks
//-----------------------------------------------------------------------------
bool CResourceZone::ShouldSpawnChunk( void )
{
	// Don't spawn chunks if we're outta resources
	if ( IsEmpty() )
		return false;

	// Create a chunk if we're below our max
	if ( m_aChunks.Size() >= m_iMaxChunks )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CResourceZone::SpawnChunk( const Vector &vecOrigin )
{
	// ROBIN: Disabled for now
	return;

	TFStats()->IncrementStat( TF_STAT_RESOURCE_CHUNKS_SPAWNED, 1 );

	// Create a resource chunk and add it to our list
	Vector vecVelocity = Vector( random->RandomFloat( -100,100 ), random->RandomFloat( -100,100 ), random->RandomFloat( 300,600 ));
	CResourceChunk *pChunk = CResourceChunk::Create( false, vecOrigin, vecVelocity );
	pChunk->m_hZone = this;

	// Add it to our list
	m_aChunks.AddToTail( pChunk );

	// Remove it's value from the zone
	RemoveResources( pChunk->GetResourceValue() );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CResourceZone::RecomputeClientResources( )
{
	m_flClientResources = clamp( (float)m_nResourcesLeft / (float)m_nMaxResources, 0.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CResourceZone::RemoveChunk( CResourceChunk *pChunk, bool bReturn )
{
	if (bReturn)
	{
		TFStats()->IncrementStat( TF_STAT_RESOURCE_CHUNKS_RETIRED, 1 );
	}

	m_aChunks.FindAndRemove( pChunk );

	// If I'm being returned, re-add my value to the resource level of the zone
	if ( bReturn )
	{
		m_nResourcesLeft += pChunk->GetResourceValue();
		RecomputeClientResources();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CResourceZone::AddSpawner( CResourceSpawner *pSpawner )
{
	m_aSpawners.AddToTail( pSpawner );
	pSpawner->SetActive( GetActive() );
}

//========================================================================================================================
// RESOURCE CHUNK SPAWNER
//========================================================================================================================
LINK_ENTITY_TO_CLASS( env_resourcespawner, CResourceSpawner );
PRECACHE_REGISTER( env_resourcespawner );

BEGIN_DATADESC( CResourceSpawner )

	// functions
	DEFINE_FUNCTION( SpawnChunkThink ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CResourceSpawner, DT_ResourceSpawner)
	SendPropInt( SENDINFO( m_bActive ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE();

// Resource Spawner Models
char *sResourceSpawnerModel = "models/resources/resource_spawner_B.mdl";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceSpawner::Spawn( void )
{
	m_hZone = NULL;
	m_bActive = false;
	SetModel( sResourceSpawnerModel );

	// Create the object in the physics system
	/*
	VPhysicsInitStatic();
	*/
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceSpawner::Precache( void )
{
	PrecacheModel( sResourceSpawnerModel );
}

//-----------------------------------------------------------------------------
// Purpose: Find my resource point
//-----------------------------------------------------------------------------
void CResourceSpawner::Activate( void )
{
	if ( m_target != NULL_STRING )
	{
		// Find my resource zone
		CResourceZone *pZone = (CResourceZone*)gEntList.FindEntityByName( NULL, m_target );
		if ( pZone )
		{
			m_hZone = pZone;
			SetModel( sResourceSpawnerModel );
			m_hZone->AddSpawner( this );
			return;
		}
	}

	Warning( "ERROR: Resource Spawner without a target resource zone specified.\n" );
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceSpawner::SetActive( bool bActive )
{
	// Going active?
	if ( !m_bActive && bActive )
	{
		// Randomize the thinks a little to reduce network usage ong chunk spawning 
		SetNextThink( gpGlobals->curtime + m_hZone->GetResourceRate() + random->RandomFloat( 0.0, 1.0 ) );
		SetThink( SpawnChunkThink );
		RemoveEffects( EF_NODRAW );
	}
	else if ( m_bActive && !bActive )
	{
		// Going inactive
		SetThink( NULL );
		AddEffects( EF_NODRAW );
	}

	m_bActive = bActive;
}

//-----------------------------------------------------------------------------
// Purpose: Spawn a chunk from this spawner
//-----------------------------------------------------------------------------
void CResourceSpawner::SpawnChunkThink( void )
{
	// Lost our zone?
	if ( !m_hZone )
	{
		SetActive( false );
		return;
	}

	if ( m_hZone->ShouldSpawnChunk() )
	{
		// Start spawning events
		EntityMessageBegin( this );
		MessageEnd();

		m_hZone->SpawnChunk( GetAbsOrigin() + Vector(0,0,64) );
	}

	// Randomize the thinks a little to reduce network usage on chunk spawning 
	SetNextThink( gpGlobals->curtime + m_hZone->GetResourceRate() + random->RandomFloat( 0.0, 1.0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Convert an amount of resources into a number of processed & unprocessed resource chunks
//-----------------------------------------------------------------------------
void ConvertResourceValueToChunks( int iResources, int *iNumProcessed, int *iNumNormal )
{
	*iNumProcessed = *iNumNormal = 0;

	while ( iResources >= resource_chunk_processed_value.GetFloat() )
	{
		iResources -= resource_chunk_processed_value.GetFloat();
		*iNumProcessed += 1;
	}

	while ( iResources >= resource_chunk_value.GetFloat() )
	{
		iResources -= resource_chunk_value.GetFloat();
		*iNumNormal += 1;
	}

	// Round up
	if ( iResources )
	{
		*iNumNormal++;
	}
}