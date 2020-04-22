//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Resource chunks
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_func_resource.h"
#include "tf_team.h"
#include "tf_basecombatweapon.h"
#include "tf_obj.h"
#include "resource_chunk.h"
#include "vstdlib/random.h"
#include "tf_stats.h"
#include "engine/IEngineSound.h"

ConVar	resource_chunk_value( "resource_chunk_value","20", FCVAR_NONE, "Resource value of a single resource chunk." );
ConVar	resource_chunk_processed_value( "resource_chunk_processed_value","80", FCVAR_NONE, "Resource value of a single processed resource chunk." );

// Resource Chunk Models
char *sResourceChunkModel = "models/resources/resource_chunk_B.mdl";
char *sProcessedResourceChunkModel = "models/resources/processed_resource_chunk_B.mdl";

BEGIN_DATADESC( CResourceChunk )

	// functions
	DEFINE_FUNCTION( ChunkTouch ),
	DEFINE_FUNCTION( ChunkRemove ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CResourceChunk, DT_ResourceChunk )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( resource_chunk, CResourceChunk );
PRECACHE_REGISTER( resource_chunk );

//-----------------------------------------------------------------------------
// Purpose: Remove me from any lists I'm in when I'm deleted
//-----------------------------------------------------------------------------
void CResourceChunk::UpdateOnRemove( void )
{
	if ( m_hZone )
	{
		m_hZone->RemoveChunk( this, false );
		m_hZone = NULL;
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceChunk::Spawn( )
{
	// Init model
	if ( IsProcessed() )
	{
		SetModelName( AllocPooledString( sProcessedResourceChunkModel ) );
	}
	else
	{
		SetModelName( AllocPooledString( sResourceChunkModel ) );
	}

	BaseClass::Spawn();

	UTIL_SetSize( this, Vector(-4,-4,-4), Vector(4,4,4) );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );
	CollisionProp()->UseTriggerBounds( true, 24 );
	SetCollisionGroup( TFCOLLISION_GROUP_RESOURCE_CHUNK );
	SetGravity( 1.0 );
	SetFriction( 1 );
	SetTouch( ChunkTouch );
	SetThink( ChunkRemove );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 50.0, 80.0 ) ); // Remove myself the
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceChunk::Precache( void )
{
	PrecacheModel( sResourceChunkModel );
	PrecacheModel( sProcessedResourceChunkModel );
	PrecacheModel( "sprites/redglow1.vmt" );

	PrecacheScriptSound( "ResourceChunk.Pickup" );

}

//-----------------------------------------------------------------------------
// Purpose: Create a resource chunk
//-----------------------------------------------------------------------------
CResourceChunk *CResourceChunk::Create( bool bProcessed, const Vector &vecOrigin, const Vector &vecVelocity )
{
	CResourceChunk *pChunk = (CResourceChunk*)CreateEntityByName("resource_chunk");

	UTIL_SetOrigin( pChunk, vecOrigin );
	pChunk->m_bIsProcessed = bProcessed;
	pChunk->m_bBeingCollected = false;
	pChunk->Spawn();
	pChunk->SetAbsVelocity( vecVelocity );
	pChunk->SetLocalAngularVelocity( RandomAngle( -100, 100 ) );
	pChunk->SetLocalAngles( vec3_angle );

	return pChunk;
}

//-----------------------------------------------------------------------------
// Purpose: If we're picked up by another pla`yer, give resources to that team
//-----------------------------------------------------------------------------
void CResourceChunk::ChunkTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() || pOther->GetServerVehicle() )
	{
		// Give the team the resources
		int iAmountPerPlayer = ((CTFTeam *)pOther->GetTeam())->AddTeamResources( GetResourceValue(), TF_PLAYER_STAT_RESOURCES_ACQUIRED_FROM_CHUNKS );
		TFStats()->IncrementTeamStat( pOther->GetTeamNumber(), TF_TEAM_STAT_RESOURCE_CHUNKS_COLLECTED, GetResourceValue() );

		pOther->EmitSound( "ResourceChunk.Pickup" );

		// Tell the player
		CSingleUserRecipientFilter user( (CBasePlayer*)pOther );
		UserMessageBegin( user, "PickupRes" );
			WRITE_BYTE( iAmountPerPlayer ); 
		MessageEnd();

		// Tell our zone to remove this chunk from it's list
		if ( m_hZone )
		{
			m_hZone->RemoveChunk( this, false );
			m_hZone = NULL;
		}

		// Remove this chunk
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove myself if I'm not being harvested
//-----------------------------------------------------------------------------
void CResourceChunk::ChunkRemove( void )
{
	// Remove this chunk
	if ( m_hZone )
	{
		m_hZone->RemoveChunk( this, true );
		m_hZone = NULL;
	}
	UTIL_Remove( this );
}


//-----------------------------------------------------------------------------
// Purpose: Return the resource value of this chunk
//-----------------------------------------------------------------------------
float CResourceChunk::GetResourceValue( void )
{
	// Init value & model
	if ( IsProcessed() )
		return resource_chunk_processed_value.GetFloat();

	return resource_chunk_value.GetFloat();
}