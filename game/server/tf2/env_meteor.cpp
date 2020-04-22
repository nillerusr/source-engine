//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_player.h"
#include "Env_Meteor.h"
#include "entitylist.h"
#include "vphysics_interface.h"
#include "tier1/strtools.h"
#include "mapdata_shared.h"
#include "sharedinterface.h"
#include "skycamera.h"
#include "ispatialpartition.h"
#include "gameinterface.h"
#include "props.h"
#include "tf_func_resource.h"
#include "resource_chunk.h"


#include "ndebugoverlay.h"

//=============================================================================
//
// Enumerator for swept bbox collision.
//
class CCollideList : public IEntityEnumerator
{
public:
	CCollideList( Ray_t *pRay, CBaseEntity* pIgnoreEntity, int nContentsMask ) : 
		m_Entities( 0, 32 ), m_pIgnoreEntity( pIgnoreEntity ),
		m_nContentsMask( nContentsMask ), m_pRay(pRay) {}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		trace_t tr;
		enginetrace->ClipRayToEntity( *m_pRay, m_nContentsMask, pHandleEntity, &tr );
		if (( tr.fraction < 1.0f ) || (tr.startsolid) || (tr.allsolid))
		{
			CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
			m_Entities.AddToTail( pEntity );
		}

		return true;
	}

	CUtlVector<CBaseEntity*>	m_Entities;

private:
	CBaseEntity		*m_pIgnoreEntity;
	int				m_nContentsMask;
	Ray_t			*m_pRay;
};


//=============================================================================
//
// Meteor Factory Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMeteorFactory::CreateMeteor( int nID, int iType,
								   const Vector &vecPosition, const Vector &vecDirection, 
		                           float flSpeed, float flStartTime, float flDamageRadius,
								   const Vector &vecTriggerMins, const Vector &vecTriggerMaxs )
{
	CEnvMeteor::Create( nID, iType, vecPosition, vecDirection, flSpeed, flStartTime, flDamageRadius,
		                vecTriggerMins, vecTriggerMaxs );
}

//=============================================================================
//
// Meteor Spawner Functions
//

void SendProxy_MeteorTargetPositions( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CEnvMeteorSpawnerShared *pMeteorSpawner = ( CEnvMeteorSpawnerShared* )pData;
	pOut->m_Vector[0] = pMeteorSpawner->m_aTargets[iElement].m_vecPosition.x;
	pOut->m_Vector[1] = pMeteorSpawner->m_aTargets[iElement].m_vecPosition.y;
	pOut->m_Vector[2] = pMeteorSpawner->m_aTargets[iElement].m_vecPosition.z;
}

void SendProxy_MeteorTargetRadii( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CEnvMeteorSpawnerShared *pMeteorSpawner = ( CEnvMeteorSpawnerShared* )pData;
	pOut->m_Float = pMeteorSpawner->m_aTargets[iElement].m_flRadius;
}

int SendProxyArrayLength_MeteorTargets( const void *pStruct, int objectID )
{
	CEnvMeteorSpawnerShared *pMeteorSpawner = ( CEnvMeteorSpawnerShared* )pStruct;
	return pMeteorSpawner->m_aTargets.Count();
}

// Link the name "env_meteorspawner" to the CMeteorSpawner class.  This
// links the WC entity with the game code.
LINK_ENTITY_TO_CLASS( env_meteorspawner, CEnvMeteorSpawner );

BEGIN_DATADESC( CEnvMeteorSpawner )

	// Key Fields.
	DEFINE_KEYFIELD( m_SpawnerShared.m_iMeteorType, FIELD_INTEGER, "MeteorType" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_bSkybox, FIELD_INTEGER, "SpawnInSkybox" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_flMinSpawnTime, FIELD_FLOAT, "SpawnIntervalMin" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_flMaxSpawnTime, FIELD_FLOAT, "SpawnIntervalMax" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_nMinSpawnCount, FIELD_INTEGER, "SpawnCountMin" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_nMaxSpawnCount, FIELD_INTEGER, "SpawnCountMax" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_flMinSpeed, FIELD_FLOAT, "MeteorSpeedMin" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_flMaxSpeed, FIELD_FLOAT, "MeteorSpeedMax" ),
	DEFINE_KEYFIELD( m_SpawnerShared.m_flMeteorDamageRadius, FIELD_FLOAT, "MeteorDamageRadius" ),
	DEFINE_KEYFIELD( m_fDisabled, FIELD_BOOLEAN,	"StartDisabled" ),

	// Function Pointers.
	DEFINE_FUNCTION( MeteorSpawnerThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()

BEGIN_SEND_TABLE_NOBASE( CEnvMeteorSpawnerShared, DT_EnvMeteorSpawnerShared )
	// Setup (read from) Worldcraft.
	SendPropInt		( SENDINFO( m_iMeteorType ),		 8, SPROP_UNSIGNED ),
	SendPropInt     ( SENDINFO( m_bSkybox ),             4, SPROP_UNSIGNED ),
	SendPropFloat	( SENDINFO( m_flMinSpawnTime ),      0, SPROP_NOSCALE ),	
	SendPropFloat	( SENDINFO( m_flMaxSpawnTime ),      0, SPROP_NOSCALE ),	
	SendPropInt     ( SENDINFO( m_nMinSpawnCount ),     16, SPROP_UNSIGNED ),
	SendPropInt     ( SENDINFO( m_nMaxSpawnCount ),     16, SPROP_UNSIGNED ),
	SendPropFloat	( SENDINFO( m_flMinSpeed ),          0, SPROP_NOSCALE ),	
	SendPropFloat	( SENDINFO( m_flMaxSpeed ),          0, SPROP_NOSCALE ),
	
	// Setup through Init.
	SendPropFloat	( SENDINFO( m_flStartTime ),	    -1, SPROP_NOSCALE ),
	SendPropInt		( SENDINFO( m_nRandomSeed ),		-1, SPROP_UNSIGNED ),
	SendPropVector	( SENDINFO( m_vecMinBounds ),       -1, SPROP_NOSCALE ),
	SendPropVector  ( SENDINFO( m_vecMaxBounds ),		-1, SPROP_NOSCALE ),
	SendPropVector  ( SENDINFO( m_vecTriggerMins ),     -1, SPROP_NOSCALE ),
	SendPropVector  ( SENDINFO( m_vecTriggerMaxs ),		-1, SPROP_NOSCALE ),

	// Target List
	SendPropArray2( SendProxyArrayLength_MeteorTargets, 
	                SendPropVector( "meteortargetposition_array_element", 0, 0, 0, SPROP_NOSCALE, 0, 0, SendProxy_MeteorTargetPositions ), 
		            16, 0, "meteortargetposition_array" ),

	SendPropArray2( SendProxyArrayLength_MeteorTargets,
		            SendPropFloat( "meteortargetradius_array_element", 0, 0, 0, SPROP_NOSCALE, 0, 0, SendProxy_MeteorTargetRadii ), 
		            16, 0, "meteortargetradius_array" )
END_SEND_TABLE()

// This table encodes the CBaseEntity data.
IMPLEMENT_SERVERCLASS_ST_NOBASE( CEnvMeteorSpawner, DT_EnvMeteorSpawner )
	SendPropDataTable	( SENDINFO_DT( m_SpawnerShared ), &REFERENCE_SEND_TABLE( DT_EnvMeteorSpawnerShared ) ),
	SendPropInt			( SENDINFO( m_fDisabled ),		1, SPROP_UNSIGNED ),
END_SEND_TABLE()

// Meteor Models
char *strResourceMeteorModels[2] =
{
	"models/props/common/meteorites/meteor04.mdl",
	"models/props/common/meteorites/meteor05.mdl",
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CEnvMeteorSpawner::CEnvMeteorSpawner()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::Spawn( void )
{
	// Pre-cache.
	Precache();

	// Server-side is not visible -- for collision only.
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );

	// Set the "brush model" size and link into the world.
	SetModel( STRING( GetModelName() ) );

	// Set the think function and time.
	if ( !m_fDisabled )
	{
		SetThink( MeteorSpawnerThink );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::InputEnable( inputdata_t &inputdata )
{
	m_fDisabled = false;

	m_SpawnerShared.m_flStartTime = gpGlobals->curtime;
	m_SpawnerShared.m_flNextSpawnTime = m_SpawnerShared.m_flStartTime + m_SpawnerShared.m_flMaxSpawnTime;

	// Probably should set this as a message begin, etc..... will get to this later!!
//
//  CEntityMessageFilter filter( this, "CEnvMeteorSpawner" );
//	MessageBegin( filter, 0 );
//		WRITE_LONG( m_SpawnerShared.m_flStartTime );
//		WRITE_LONG( m_SpawnerShared.m_flNextSpawnTime );
//	MessageEnd();

	// Set the think function and time.
	SetThink( MeteorSpawnerThink );
	SetNextThink( gpGlobals->curtime + m_SpawnerShared.m_flNextSpawnTime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::InputDisable( inputdata_t &inputdata )
{
	m_fDisabled = true;
	SetThink( NULL );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::Get3DSkyboxWorldBounds( Vector &vecTriggerMins, 
											    Vector &vecTriggerMaxs )
{
	CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, "trigger_skybox2world" );
	if ( pEntity && pEntity->edict() )
	{
		pEntity->CollisionProp()->WorldSpaceAABB( &vecTriggerMins, &vecTriggerMaxs );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::Precache( void )
{
	// Precache the meteor models!
	for ( int iType = 0; iType < 2; iType++ )
	{
		PrecacheModel( strResourceMeteorModels[iType] );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::MeteorSpawnerThink( void )
{
	SetNextThink( gpGlobals->curtime + m_SpawnerShared.MeteorThink( gpGlobals->curtime ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CEnvMeteorSpawner::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	if ( m_SpawnerShared.m_bSkybox )
		return FL_EDICT_ALWAYS;

	return BaseClass::ShouldTransmit( pInfo );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorSpawner::Activate( void )
{
	// Parse the entity list looking for targets!
	int nEntityCount = engine->GetEntityCount();
	for ( int iEntity = 0; iEntity < nEntityCount; ++iEntity )
	{
		edict_t *pEdict = engine->PEntityOfEntIndex( iEntity );
		if ( !pEdict || pEdict->IsFree() )
			continue;

		CBaseEntity *pEntity = GetContainingEntity( pEdict );
		if ( !pEntity )
			continue;

		if ( pEntity->GetFlags()& FL_STATICPROP ) 
			continue;

		if ( !Q_strcmp( pEntity->GetClassname(), "env_meteortarget" ) )
		{
			CEnvMeteorTarget *pMeteorTarget = static_cast<CEnvMeteorTarget*>( pEntity );
			if ( pMeteorTarget && pMeteorTarget->m_target != NULL_STRING )
			{
				if ( !Q_strcmp( STRING( pMeteorTarget->m_target ), STRING( GetEntityName() ) ) )
				{
					m_SpawnerShared.AddToTargetList( pMeteorTarget->GetLocalOrigin(), pMeteorTarget->m_flRadius );
				}
			}
		}
	}

	// Get 3d skybox world trigger bounds.
	Vector vecTriggerMins, vecTriggerMaxs;
	Get3DSkyboxWorldBounds( vecTriggerMins, vecTriggerMaxs );

	// Initialize the spawner.
	float flTime = gpGlobals->curtime;
	m_SpawnerShared.Init( &m_Factory, 0/* seed */, flTime,
		                  WorldAlignMins(), WorldAlignMaxs(), vecTriggerMins, vecTriggerMaxs );

	// Setup next think.
	if ( !m_fDisabled )
	{
		SetNextThink( gpGlobals->curtime + m_SpawnerShared.m_flNextSpawnTime );
	}
}

//=============================================================================
//
// Meteor Target Functions
//

LINK_ENTITY_TO_CLASS( env_meteortarget, CEnvMeteorTarget );

BEGIN_DATADESC( CEnvMeteorTarget )

	// Key Fields.
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "EffectRadius" ),

END_DATADESC()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CEnvMeteorTarget::CEnvMeteorTarget()
{
	m_iTargetID = -1;
	m_flRadius = 1.0f;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteorTarget::Spawn( void )
{
	BaseClass::Spawn();
}
	
//=============================================================================
//
// Meteor Functions
//

//
// NOTE: The server-side meteor code has not really been tested.  I do not
//       trust that is works correctly and/or cleans itself up nicely!
//

LINK_ENTITY_TO_CLASS( env_meteor, CEnvMeteor );

BEGIN_DATADESC( CEnvMeteor )

	// Function Pointers.
	DEFINE_FUNCTION( MeteorSkyboxThink ),
	DEFINE_FUNCTION( MeteorWorldThink ),

END_DATADESC()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CEnvMeteor::CEnvMeteor()
{
	m_vecMin.Init( -10.0f, -10.0f, -10.0f );
	m_vecMax.Init( 10.0f, 10.0f, 10.0f );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CEnvMeteor *CEnvMeteor::Create( int nID, int iMeteorType, 
							    const Vector &vecOrigin, const Vector &vecDirection,
								float flSpeed, float flStartTime, float flDamageRadius,
								const Vector &vecTriggerMins, const Vector &vecTriggerMaxs )
{
	CEnvMeteor *pMeteor = ( CEnvMeteor* )CreateEntityByName( "env_meteor" );
	if ( pMeteor )
	{
		pMeteor->m_Meteor.Init( nID, flStartTime, METEOR_PASSIVE_TIME, vecOrigin, vecDirection, flSpeed,
			                    flDamageRadius, vecTriggerMins, vecTriggerMaxs );

		// If the meteor will never enter the world, then don't bother with a server-side version.
		if ( pMeteor->m_Meteor.m_flWorldEnterTime == METEOR_INVALID_TIME )
		{
			UTIL_Remove( pMeteor );
		}

		// Handle forward simulation.
		if ( ( pMeteor->m_Meteor.m_flStartTime + METEOR_MAX_LIFETIME ) < gpGlobals->curtime )
		{
			UTIL_Remove( pMeteor );
		}

		pMeteor->Spawn();
		pMeteor->SetNextThink( gpGlobals->curtime + pMeteor->m_Meteor.m_flWorldEnterTime );
	}

	return pMeteor;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvMeteor::Spawn( void )
{
	// Pass data.
	BaseClass::Spawn();

	int iModel = modelinfo->GetModelIndex( "models/props/common/meteorites/meteor04.mdl" );
	if ( iModel > 0 )
	{
		const model_t *pModel = modelinfo->GetModel( iModel );
		modelinfo->GetModelBounds( pModel, m_vecMin, m_vecMax );
	}

	// Assumes we start life in a skybox!
	SetThink( MeteorSkyboxThink );

	m_bPrevInSkybox = true;
}

//-----------------------------------------------------------------------------
// Purpose: This think function should be called at the time when the meteor 
//          will be leaving the skybox and entering the world.
//-----------------------------------------------------------------------------
void CEnvMeteor::MeteorSkyboxThink( void )
{
	SetThink( MeteorWorldThink );
	SetNextThink( gpGlobals->curtime + 0.2f );
}

//-----------------------------------------------------------------------------
// Purpose: This think function simulates (moves/collides) the meteor while in
//          the world.
//-----------------------------------------------------------------------------
void CEnvMeteor::MeteorWorldThink( void )
{
	// Get the current time.
	float flTime = gpGlobals->curtime;

	// Convert if need be!
	if ( m_bPrevInSkybox )
	{
		m_Meteor.ConvertFromSkyboxToWorld();
		UTIL_SetOrigin( this, m_Meteor.m_vecStartPosition );

		m_bPrevInSkybox = false;
	}

	// Update meteor position for swept collision test.
	Vector vecEndPosition;
	m_Meteor.GetPositionAtTime( flTime, vecEndPosition );

	// Debugging!!
//	NDebugOverlay::Box( GetAbsOrigin(), m_vecMin * 0.5f, m_vecMax * 0.5f, 255, 255, 0, 0, 5 );
//	NDebugOverlay::Box( vecEndPosition, m_vecMin, m_vecMax, 255, 0, 0, 0, 5 );

	Ray_t ray;
	ray.Init( GetAbsOrigin(), vecEndPosition, m_vecMin, m_vecMax );

	CCollideList collideList( &ray, this, MASK_SOLID );
	enginetrace->EnumerateEntities( ray, false, &collideList );

	// Now get each entity and react accordinly!
	for( int iEntity = collideList.m_Entities.Count(); --iEntity >= 0; )
	{
		CBaseEntity *pEntity = collideList.m_Entities[iEntity];

		if  ( pEntity )
		{
			Vector vecForceDir = m_Meteor.m_vecDirection;

			// Check for a physics object and apply force!
			IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
			if ( pPhysObject )
			{
//				float flMass = pPhysObject->GetMass();
				
				// Send it flying!!!
				vecForceDir *= 5000000000000.0f;
				pPhysObject->ApplyForceCenter( vecForceDir );
			}

			if ( pEntity->m_takedamage )
			{
				CTakeDamageInfo info( this, this, 200.0f, DMG_CLUB );
				CalculateExplosiveDamageForce( &info, vecForceDir, pEntity->GetAbsOrigin() );
				pEntity->TakeDamage( info );
			}
		}
	}

	trace_t trace;
	UTIL_TraceHull( GetAbsOrigin(), vecEndPosition, m_vecMin, m_vecMax,
		            MASK_NPCWORLDSTATIC, this, COLLISION_GROUP_NONE, &trace );
	if( ( trace.fraction < 1.0f ) && !( trace.surface.flags & SURF_SKY ) )
	{
		CBaseEntity *pEntity = trace.m_pEnt;
		if ( pEntity )
		{
			// Hit the world? The meteor is destroyed!
			if ( pEntity->GetSolid() == SOLID_BSP )
			{
#if 0
				// Suppress resources for now!!

				// Create a random number or resource chunks.
				int nChunkCount = random->RandomInt( 0, 4 );
				for( int iChunk = 0; iChunk < nChunkCount; ++iChunk )
				{
					// Generate a random velocity vector.
					Vector vVelocity = Vector( random->RandomFloat( -20,20 ), random->RandomFloat( -20,20 ), random->RandomFloat( 100,150 ) );
					CResourceChunk::Create( false, GetAbsOrigin(), vVelocity );
				}
#endif

				// Splash damage!
				Vector vecImpactPoint;
				vecImpactPoint = GetAbsOrigin() + ( ( vecEndPosition - GetAbsOrigin() ) * trace.fraction ); 

				// Debugging!!
//				NDebugOverlay::Box( vecImpactPoint, m_vecMin, m_vecMax, 0, 255, 0, 0, 5 );

				//Iterate on all entities in the vicinity.
				for ( CEntitySphereQuery sphere( vecImpactPoint, m_Meteor.GetDamageRadius() ); pEntity = sphere.GetCurrentEntity(); sphere.NextEntity() )
				{
					// Get distance to object and use it as a scale value.
					Vector vecSegment;
					vecSegment = pEntity->GetAbsOrigin() - vecImpactPoint;
					float flDistance = vecSegment.Length();

					float flScale = flDistance / ( m_Meteor.GetDamageRadius() * 0.75f );
					if ( flScale > 1.0f ) 
					{ 
						flScale = 1.0f; 
					}
					
					Vector vecForceDir = m_Meteor.m_vecDirection;

					// Check for a physics object and apply force!
					IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
					if ( pPhysObject )
					{
//						float flMass = pPhysObject->GetMass();

						// Send it flying!!!
						vecForceDir *= 5000000000000.0f * flScale;
						pPhysObject->ApplyForceCenter( vecForceDir );
					}

					if ( pEntity->m_takedamage )
					{
						CTakeDamageInfo info( this, this, 300.0f * flScale, DMG_CLUB );
						CalculateExplosiveDamageForce( &info, vecForceDir, pEntity->GetAbsOrigin() );
						pEntity->TakeDamage( info );
					}
				}

				UTIL_Remove( this );
				return;
			}
		}
	}

	// Always move full movement.
	UTIL_SetOrigin( this, vecEndPosition );
	SetNextThink( gpGlobals->curtime + 0.2f );

	// Check for death.
	if ( flTime >= m_Meteor.m_flWorldExitTime )
	{
		UTIL_Remove( this );
		return;
	}
}

//=============================================================================
// 
// Shooting Star Spawner Functionality.
//

// Link the name "env_meteorspawner" to the CMeteorSpawner class.  This
// links the WC entity with the game code.
LINK_ENTITY_TO_CLASS( env_shootingstarspawner, CShootingStarSpawner );

BEGIN_DATADESC( CShootingStarSpawner )

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_flSpawnInterval, FIELD_FLOAT, "SpawnInterval" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_bSkybox, FIELD_INTEGER, "SpawnInSkybox" ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CShootingStarSpawner, DT_ShootingStarSpawner )
	SendPropFloat( SENDINFO( m_flSpawnInterval ), -1, SPROP_NOSCALE ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CShootingStarSpawner::CShootingStarSpawner()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CShootingStarSpawner::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Always send shooting star spawners if they are in the skybox!
	if ( m_bSkybox )
		return FL_EDICT_ALWAYS ;

	return BaseClass::ShouldTransmit( pInfo );
}
