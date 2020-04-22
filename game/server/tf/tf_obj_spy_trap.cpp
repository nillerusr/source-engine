//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//=============================================================================

#include "cbase.h"
#include "tf_obj_spy_trap.h"
#include "tf_team.h"
#include "tf_player.h"
#include "bot/tf_bot.h"
#include "tf_gamerules.h"
#include "tf_fx.h"

#ifdef STAGING_ONLY

#define SPY_TRAP_THINK_CONTEXT	"SpyTrapContext"

#define SPY_TRAP_SAP_MODEL_HOLD		"models/buildables/teleporter_light.mdl"
#define SPY_TRAP_SAP_MODEL_PLACED	"models/buildables/teleporter_light.mdl"

#define SPY_TRAP_RERPOGRAMMER_MODEL_HOLD	"models/buildables/teleporter_light.mdl"
#define SPY_TRAP_REPROGRAMMER_MODEL_PLACED	"models/buildables/teleporter_light.mdl"

#define SPY_TRAP_MAGNET_MODEL_HOLD		"models/buildables/teleporter_light.mdl"
#define SPY_TRAP_MAGNET_MODEL_PLACED	"models/buildables/teleporter_light.mdl"

ConVar tf_spy_trap_duration( "tf_spy_trap_duration", "20.0", FCVAR_CHEAT );
ConVar tf_spy_trap_cloak_duration( "tf_spy_trap_cloak_duration", "10", FCVAR_CHEAT );
ConVar tf_spy_trap_magnet_duration( "tf_spy_trap_magnet_duration", "5", FCVAR_CHEAT );
ConVar tf_spy_trap_magnet_force( "tf_spy_trap_magnet_force", "650", FCVAR_CHEAT );

const Vector TRAP_MINS = Vector( -24, -24, 0);
const Vector TRAP_MAXS = Vector( 24, 24, 12);

BEGIN_DATADESC( CObjectSpyTrap )
	DEFINE_THINKFUNC( SpyTrapThink ),
END_DATADESC()

PRECACHE_REGISTER( obj_spy_trap );

LINK_ENTITY_TO_CLASS( obj_spy_trap, CObjectSpyTrap );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CObjectSpyTrap::CObjectSpyTrap()
{
	SetType( OBJ_SPY_TRAP );
	m_attributeFlags = 0;
	m_bActive = false;
	m_nTrapMode = MODE_SPY_TRAP_RADIUS_STEALTH;
	m_flNextTrapEffectTime = 0.f;
	m_flTrapExpireTime = 0.f;
	m_flNextPulseTime = 0.f;

	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::Spawn()
{
	SetSolid( SOLID_BBOX );
	SetModel( SPY_TRAP_SAP_MODEL_HOLD );
	UTIL_SetSize( this, TRAP_MINS, TRAP_MAXS );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Saxxy.TurnGold" );
	PrecacheScriptSound( "Weapon_Upgrade.ExplosiveHeadshot" );
	PrecacheModel( SPY_TRAP_SAP_MODEL_HOLD );
	PrecacheModel( SPY_TRAP_SAP_MODEL_PLACED );
	PrecacheModel( SPY_TRAP_RERPOGRAMMER_MODEL_HOLD );
	PrecacheModel( SPY_TRAP_REPROGRAMMER_MODEL_PLACED );
	PrecacheModel( SPY_TRAP_MAGNET_MODEL_HOLD );
	PrecacheModel( SPY_TRAP_MAGNET_MODEL_PLACED );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::SpyTrapThink()
{
	// Touch-triggered traps expire after a period of time
	if ( !m_bActive && GetConstructionStartTime() + tf_spy_trap_duration.GetFloat() < gpGlobals->curtime )
	{
		Destroy();
	}

	// Traps that repeat their effect over time
	if ( HasAttribute( TRAP_PULSE_EFFECT ) )
	{
		if ( m_bActive )
		{
			// Still active
			if ( m_flTrapExpireTime > gpGlobals->curtime )
			{
				// Time for another pulse
				if ( m_flNextPulseTime && gpGlobals->curtime > m_flNextPulseTime )
				{
					switch ( GetTrapType() )
					{
					case MODE_SPY_TRAP_RADIUS_STEALTH:
						{
							TriggerTrap_RadiusCloak();
							break;
						}
					case MODE_SPY_TRAP_MAGNET:
						{
							TriggerTrap_Magnet();
							break;
						}
					}
				}
			}
			// Timer expired
			else
			{
				Destroy();
			}
		}
	}

	SetContextThink( &CObjectSpyTrap::SpyTrapThink, gpGlobals->curtime + 0.1f, SPY_TRAP_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::OnGoActive()
{
	BaseClass::OnGoActive();

	switch ( GetTrapType() )
	{
	case MODE_SPY_TRAP_RADIUS_STEALTH:
		{
			SetModel( SPY_TRAP_SAP_MODEL_PLACED );
			SetAttribute( TRAP_TRIGGER_ONBUILD | TRAP_PULSE_EFFECT );
			m_flTrapExpireTime = gpGlobals->curtime + tf_spy_trap_cloak_duration.GetFloat();
			break;
		}
	case MODE_SPY_TRAP_REPROGRAM:
		{
			SetModel( SPY_TRAP_REPROGRAMMER_MODEL_PLACED );
			break;
		}
	case MODE_SPY_TRAP_MAGNET:
		{
			SetModel( SPY_TRAP_MAGNET_MODEL_PLACED );
			SetAttribute( TRAP_TRIGGER_ONBUILD | TRAP_PULSE_EFFECT );
			m_flTrapExpireTime = gpGlobals->curtime + tf_spy_trap_magnet_duration.GetFloat();
			break;
		}
	}
	
	m_takedamage = DAMAGE_NO;

	m_bActive = HasAttribute( TRAP_TRIGGER_ONBUILD );
	if ( m_bActive )
	{
		m_flNextPulseTime = gpGlobals->curtime;
	}

	SetContextThink( &CObjectSpyTrap::SpyTrapThink, gpGlobals->curtime + 0.1, SPY_TRAP_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSpyTrap::SetObjectMode( int iVal )
{
	Assert( iVal >= MODE_SPY_TRAP_RADIUS_STEALTH && iVal <= MODE_SPY_TRAP_MAGNET );
	SetTrapType( (ESpyTrapType_t)iVal );

	BaseClass::SetObjectMode( iVal );
}

//-----------------------------------------------------------------------------
// Traps that trigger via touch activate here
//-----------------------------------------------------------------------------
void CObjectSpyTrap::Activate( CBaseEntity *pTouchEntity )
{
	if ( m_bActive )
		return;

	switch ( GetTrapType() )
	{
	case MODE_SPY_TRAP_REPROGRAM:
		{
			TriggerTrap_Reprogrammer( pTouchEntity );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::Destroy( void )
{
	Explode();
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::TriggerTrapEffects( void )
{
	if ( gpGlobals->curtime < m_flNextTrapEffectTime )
		return;

	Vector vecOrigin = GetAbsOrigin();
	CPVSFilter filter( vecOrigin );
	TE_TFParticleEffect( filter, 0.f, "Explosion_ShockWave_01", vecOrigin, vec3_angle );
	EmitSound( filter, entindex(), "Weapon_Upgrade.ExplosiveHeadshot" );

	m_flNextTrapEffectTime = gpGlobals->curtime + 1.f;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CObjectSpyTrap::IsPlacementPosValid( void )
{
	// This is mostly duplicated from baseobject.  Poop.
	// The alternative is modifying a bunch of call sites
	// and derived classes to handle an object pointer,
	// and having special case code in the base method.
	// It's a "which is poopier" contest.

	CTFPlayer *pPlayer = GetOwner();
	if ( !pPlayer )
		return false;

	bool bValid = CalculatePlacementPos();
	if ( !bValid )
		return false;

#ifndef CLIENT_DLL
	if ( !EstimateValidBuildPos() )
		return false;
#endif

	// Verify that the entire object can fit here - ignoring players
	trace_t tr;
	CTraceFilterIgnorePlayers filter( this, COLLISION_GROUP_PLAYER );
	UTIL_TraceEntity( this, m_vecBuildOrigin, m_vecBuildOrigin, MASK_SOLID, &filter, &tr );
	if ( tr.fraction < 1.0f )
		return false;

	// Make sure we can see the final position
	UTIL_TraceLine( pPlayer->EyePosition(), m_vecBuildOrigin + Vector( 0, 0, m_vecBuildMaxs[2] * 0.5 ), MASK_PLAYERSOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	if ( !m_bActive && pOther->IsPlayer() && !InSameTeam( pOther ) )
	{
		if ( ( InSameTeam( pOther ) && HasAttribute( TRAP_TRIGGER_FRIENDLY ) ) ||
			 ( !InSameTeam( pOther ) ) )
		{
			Activate( pOther );
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::EndTouch( CBaseEntity *pOther )
{
	BaseClass::EndTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CObjectSpyTrap::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	// Ignore player collisions when trap pulses
	if ( HasAttribute( TRAP_PULSE_EFFECT ) )
	{
		if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
		{
			return false;
		}
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CObjectSpyTrap::TriggerTrap_RadiusCloak( void )
{
	int nRadius = 250;
	float flDuration = 2.f;

	for ( int i = 0; i < gpGlobals->maxClients; i++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer )
			continue;

		// Same team, alive, etc
		if ( !IsValidRadiusCloakTarget( pPlayer ) )
			continue;

		// Range check from pTarget
		Vector vecDist = pPlayer->GetAbsOrigin() - GetAbsOrigin();
		if ( vecDist.LengthSqr() > nRadius * nRadius )
			continue;

		// Ignore bots we can't see
		trace_t	trace;
		UTIL_TraceLine( pPlayer->WorldSpaceCenter(), WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1.0f )
			continue;

		// Apply
		pPlayer->m_Shared.AddCond( TF_COND_STEALTHED_USER_BUFF, flDuration, GetBuilder() );
	}

	TriggerTrapEffects();

	m_flNextPulseTime = gpGlobals->curtime + 0.25f;
}

//-----------------------------------------------------------------------------
// Purpose: Valid player to apply cloak effects to?
//-----------------------------------------------------------------------------
bool CObjectSpyTrap::IsValidRadiusCloakTarget( CTFPlayer *pTarget )
{
	if ( !pTarget )
		return false;

	if ( !pTarget->IsAlive() )
		return false;

	if ( !InSameTeam( pTarget ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSpyTrap::TriggerTrap_Reprogrammer( CBaseEntity *pTouchEntity )
{
	if ( !pTouchEntity )
		return;

	if ( pTouchEntity->IsPlayer() )
	{
		CTFPlayer *pTFPlayer = ToTFPlayer( pTouchEntity );
		if ( pTFPlayer && pTFPlayer->IsBot() )
		{
			if ( pTFPlayer->IsMiniBoss() )
				return;

			pTFPlayer->m_Shared.AddCond( TF_COND_REPROGRAMMED );
		}
	}

	CPVSFilter filter( GetAbsOrigin() );
	EmitSound( filter, entindex(), "Saxxy.TurnGold" );
	TriggerTrapEffects();

	Destroy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSpyTrap::TriggerTrap_Magnet( void )
{
	int nRadius = 700;

	for ( int i = 1; i < gpGlobals->maxClients; i++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer )
			continue;

		// Same team, alive, etc
		if ( !pPlayer->IsAlive() )
			continue;

		if ( InSameTeam( pPlayer ) )
			continue;

		// Range check from pTarget
		Vector vecDist = pPlayer->GetAbsOrigin() - GetAbsOrigin();
		if ( vecDist.LengthSqr() > nRadius * nRadius )
			continue;

		// Ignore bots we can't see
		trace_t	trace;
		UTIL_TraceLine( pPlayer->WorldSpaceCenter(), WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1.0f )
			continue;

		// Find where we're going
		Vector vecSourcePos = pPlayer->GetAbsOrigin();
		Vector vecTargetPos = GetAbsOrigin();
		vecTargetPos.z -= 32.0f;  

		Vector vecVelocity = vecTargetPos - vecSourcePos;
		vecVelocity.z += 150.f;

		// Send us flying
		if ( pPlayer->GetFlags() & FL_ONGROUND )
		{
			pPlayer->SetGroundEntity( NULL );
			pPlayer->SetGroundChangeTime( gpGlobals->curtime + 0.5f );
		}

		pPlayer->Teleport( NULL, NULL, &vecVelocity );
		pPlayer->m_Shared.StunPlayer( 0.5, 0.85f, TF_STUN_MOVEMENT, GetOwner() );
	}

	TriggerTrapEffects();

	m_flNextPulseTime = gpGlobals->curtime + 0.2f;
}

#endif // STAGING_ONLY
