#include "cbase.h"
#include "asw_grenade_freeze.h"
#include "asw_gamerules.h"
#include "world.h"
#include "asw_util_shared.h"
#include "iasw_spawnable_npc.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_fx_shared.h"
#include "particle_parse.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_grenade_freeze, CASW_Grenade_Freeze );
PRECACHE_REGISTER( asw_grenade_freeze );

BEGIN_DATADESC( CASW_Grenade_Freeze )	
	DEFINE_FIELD( m_flFreezeAmount, FIELD_FLOAT ),
END_DATADESC()

void CASW_Grenade_Freeze::DoExplosion()
{
	// Spawn a freeze explosion effect
	DispatchParticleEffect( "freeze_grenade_explosion", GetAbsOrigin(), Vector( m_DmgRadius, 0, 0 ), QAngle( 0, 0, 0 ) );
	EmitSound( "ASW_Freeze_Grenade.Explode" );

	// Add freeze to all creatures in the radius
	ASWGameRules()->FreezeAliensInRadius( m_hFirer.Get(), m_flFreezeAmount, GetAbsOrigin(), m_DmgRadius );
}

void CASW_Grenade_Freeze::Precache()
{
	BaseClass::Precache();

	PrecacheParticleSystem( "freeze_grenade_explosion" );
	PrecacheParticleSystem( "grenade_freeze_main_trail" );
	PrecacheScriptSound( "ASW_Freeze_Grenade.Explode" );
}

CASW_Grenade_Freeze* CASW_Grenade_Freeze::Freeze_Grenade_Create( float flDamage, float flFreezeAmount, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon )
{
	CASW_Grenade_Freeze *pGrenade = (CASW_Grenade_Freeze *)CreateEntityByName( "asw_grenade_freeze" );
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->m_flDamage = flDamage;
	pGrenade->m_flFreezeAmount = flFreezeAmount;
	pGrenade->m_DmgRadius = fRadius;
	pGrenade->m_hFirer = pOwner;
	pGrenade->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetClusters(iClusters, true);	
	pGrenade->m_hCreatorWeapon.Set( pCreatorWeapon );

	// hack attack!  for some reason, grenades refuse to be affect by damage forces until they're actually dead
	//  so we kill it immediately.
	pGrenade->TakeDamage(CTakeDamageInfo(pGrenade, pGrenade, Vector(0, 0, 1), position, 10, DMG_SLASH));

	pGrenade->SetAbsVelocity( velocity );	

	return pGrenade;
}

void CASW_Grenade_Freeze::CreateEffects()
{
	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull( true );
	DispatchParticleEffect( "grenade_freeze_main_trail", PATTACH_ABSORIGIN_FOLLOW, this, "fuse", false, -1, &filter );
}