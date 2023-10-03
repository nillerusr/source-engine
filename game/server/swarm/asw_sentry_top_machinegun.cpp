#include "cbase.h"
#include "props.h"
#include "asw_sentry_base.h"
#include "asw_sentry_top_machinegun.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "ammodef.h"
#include "asw_gamerules.h"
#include "beam_shared.h"
#include "effect_dispatch_data.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SENTRY_TOP_MODEL "models/sentry_gun/machinegun_top.mdl"

LINK_ENTITY_TO_CLASS( asw_sentry_top_machinegun, CASW_Sentry_Top_Machinegun );
PRECACHE_REGISTER( asw_sentry_top_machinegun );

/*
IMPLEMENT_SERVERCLASS_ST(CASW_Sentry_Top_Machinegun, DT_ASW_Sentry_Top_Machinegun )
END_SEND_TABLE()
*/

BEGIN_DATADESC( CASW_Sentry_Top_Machinegun )
END_DATADESC()



#define ASW_SENTRY_FIRE_RATE 0.08f		// time in seconds between each shot
#define ASW_SENTRY_OVERFIRE 0.45f		// keep firing for this long after killing someone, because it's more badass


void CASW_Sentry_Top_Machinegun::Spawn( void )
{
	BaseClass::Spawn();

	if ( GetSentryBase() )
	{
		m_bHasHysteresis = true;
	}
}

void CASW_Sentry_Top_Machinegun::SetTopModel()
{
	SetModel(SENTRY_TOP_MODEL);
}

void CASW_Sentry_Top_Machinegun::Fire()
{
	if ( !HasAmmo() )
		return;

	BaseClass::Fire();

	Vector diff; 
	if ( !m_hEnemy )
	{
		if ( gpGlobals->curtime > m_flFireHysteresisTime )
			return; // stop firing altogether
		else
		{
			// we're overfiring
			GetVectors( &diff, NULL, NULL );
		}
	}
	else
	{
		diff = m_hEnemy->WorldSpaceCenter() - GetFiringPosition();
		m_flFireHysteresisTime = gpGlobals->curtime + ASW_SENTRY_OVERFIRE ;
	}

	// if we haven't fired in a few ticks, assume this is the first bullet in a salvo,
	// and reset the next fire time such that we fire only one bullet this tick.
	// m_fNextFireTime = curtime - m_fNextFireTime >= gpGlobals->interval_per_tick * 3 ? curtime : m_fNextFireTime
	m_fNextFireTime = fsel( gpGlobals->curtime - m_fNextFireTime - gpGlobals->interval_per_tick * 3.0f, gpGlobals->curtime, m_fNextFireTime ); 
	CASW_Sentry_Base* const pBase = GetSentryBase();

	const float fPriorTickTime = gpGlobals->curtime - gpGlobals->interval_per_tick;
	do 
	{
		FireBulletsInfo_t info(1, GetFiringPosition(), diff, GetBulletSpread(),
		GetRange(), m_iAmmoType);
		info.m_pAttacker = this;
		info.m_pAdditionalIgnoreEnt = GetSentryBase();	
		info.m_flDamage = GetSentryDamage();
		info.m_iTracerFreq = 1;
		FireBullets(info);
		// because we may emit more than one bullet per server tick, space the play time
		// of the sounds out so they are at a regular interval. technically what's happening
		// here is that we are "queuing up" the bullets that are supposed to be fired this
		// frame.
		EmitSound( "ASW_Sentry.Fire", gpGlobals->curtime + m_fNextFireTime - fPriorTickTime );
		
		CEffectData	data;
		data.m_vOrigin = GetAbsOrigin();
		//data.m_vNormal = dir;
		//data.m_flScale = (float)amount;
		CPASFilter filter( data.m_vOrigin );
		filter.SetIgnorePredictionCull(true);
		DispatchParticleEffect( "muzzle_sentrygun", PATTACH_POINT_FOLLOW, this, "muzzle", false, -1, &filter );

		// advance by consistent interval (may cause more than one bullet to be fired per frame)
		m_fNextFireTime += ASW_SENTRY_FIRE_RATE;

		// use ammo
		if ( pBase )
		{
			pBase->OnFiredShots();
		}
	} while ( m_fNextFireTime < gpGlobals->curtime );

}

/*
int CASW_Sentry_Top_Machinegun::GetSentryDamage()
{
	float flDamage = 10.0f;
	if ( ASWGameRules() )
	{
		ASWGameRules()->ModifyAlienDamageBySkillLevel( flDamage );
	}

	return flDamage * GetSentryBase()->m_fDamageScale;
}
*/