//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's resupply beacon
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_obj.h"
#include "tf_player.h"
#include "tf_team.h"
#include "techtree.h"
#include "tf_shield.h"
#include "tf_obj_respawn_station.h"

IMPLEMENT_SERVERCLASS_ST(CObjectRespawnStation, DT_ObjectRespawnStation)
END_SEND_TABLE();

BEGIN_DATADESC( CObjectRespawnStation )

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_bIsInitialSpawnPoint,	FIELD_BOOLEAN, "InitialSpawn" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS(obj_respawn_station, CObjectRespawnStation);
PRECACHE_REGISTER(obj_respawn_station);

ConVar	obj_respawnstation_health( "obj_respawnstation_health","300", FCVAR_NONE, "Respawn Station health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectRespawnStation::CObjectRespawnStation()
{
	m_bIsInitialSpawnPoint = false;
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectRespawnStation::Precache()
{
	PrecacheModel( "models/objects/obj_respawn_station.mdl" );
	m_iSpriteTexture = PrecacheModel( "sprites/laserbeam.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectRespawnStation::Spawn()
{
	Precache();
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );

	SetModel( "models/objects/obj_respawn_station.mdl" );

	UTIL_SetSize(this, RESPAWN_STATION_MINS, RESPAWN_STATION_MAXS);

	m_iHealth = m_iMaxHealth = obj_respawnstation_health.GetInt();
	m_takedamage = DAMAGE_YES;
	m_fLastRespawnTime = -99999;

	SetType( OBJ_RESPAWN_STATION );

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Object using!
//-----------------------------------------------------------------------------
void CObjectRespawnStation::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Sapper removal
	if ( RemoveEnemyAttachments( pActivator ) )
		return;

	// See if the activator is a player
	if ( pActivator->IsPlayer() )
	{
		CBaseTFPlayer *player = static_cast< CBaseTFPlayer * >( pActivator );
		player->SetRespawnStation( this );
	}

	BaseClass::Use( pActivator, pCaller, useType, value );
}



//-----------------------------------------------------------------------------
// Gets called when someone respawns on this station
//-----------------------------------------------------------------------------
void CObjectRespawnStation::PerformRespawnEffect()
{
	if (gpGlobals->curtime - m_fLastRespawnTime > RESPAWN_EFFECT_TIME)
	{
		Vector vecEnd;
		VectorAdd( GetAbsOrigin(), Vector( 0, 0, RESPAWN_BEAM_HEIGHT ), vecEnd );

		CBroadcastRecipientFilter filter;
		te->BeamPoints( filter, 0.0,
			&GetAbsOrigin(), 
			&vecEnd, 
			m_iSpriteTexture, 
			0,		// Halo index
			0,		// Start frame
			15,		// Frame rate
			3.0,	// Life
			50,		// Width
			50,		// EndWidth
			0,		// FadeLength
			0,		// Amplitude
			100,	// r
			200,	// g
			255,	// b
			255,	// a
			20	);	// speed

		m_fLastRespawnTime = gpGlobals->curtime;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectRespawnStation* CObjectRespawnStation::Create(const Vector &vOrigin, const QAngle &vAngles )
{
	CObjectRespawnStation *pRet = (CObjectRespawnStation*)CreateEntityByName("obj_respawn_station");
	if(pRet)
	{
		pRet->SetLocalOrigin( vOrigin );
		pRet->SetLocalAngles( vAngles );
		pRet->Spawn();
	}
	
	return pRet;
}


//-----------------------------------------------------------------------------
// Plays a respawn effect on a respawn station...
//-----------------------------------------------------------------------------
void PlayRespawnEffect(CBaseEntity *pRespawnStation)
{
	// ROBIN: Removed this for now
	return;

	// Check last respawn time; wait a couple seconds
	if (!FClassnameIs(pRespawnStation, "obj_respawn_station"))
		return;

	CObjectRespawnStation* pStation = static_cast<CObjectRespawnStation*>(pRespawnStation);
	pStation->PerformRespawnEffect();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this spawnpoint is the map specified initial spawnpoint for its team
//-----------------------------------------------------------------------------
bool CObjectRespawnStation::IsInitialSpawnPoint( void )
{
	return m_bIsInitialSpawnPoint;
}


