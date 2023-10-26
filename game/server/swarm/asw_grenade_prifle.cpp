#include "cbase.h"
#include "asw_grenade_prifle.h"
#include "asw_gamerules.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "world.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "asw_util_shared.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "particle_parse.h"
#include "asw_player.h"
#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define STUN_GRENADE_MODEL "models/swarm/grenades/StunGrenadeProjectile.mdl"

ConVar asw_stun_grenade_time("asw_stun_grenade_time", "6.0", FCVAR_CHEAT, "How long stun grenades stun aliens for");

LINK_ENTITY_TO_CLASS( asw_grenade_prifle, CASW_Grenade_PRifle );

PRECACHE_REGISTER(asw_grenade_prifle);

CASW_Grenade_PRifle* CASW_Grenade_PRifle::PRifle_Grenade_Create( float flDamage, float fRadius, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon )
{
	CASW_Grenade_PRifle *pGrenade = (CASW_Grenade_PRifle *)CreateEntityByName( "asw_grenade_prifle" );	
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->m_flDamage = flDamage;
	pGrenade->m_DmgRadius = fRadius;
	pGrenade->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->m_hCreatorWeapon.Set( pCreatorWeapon );

	return pGrenade;
}

void CASW_Grenade_PRifle::Spawn()
{
	BaseClass::Spawn();
	SetModel( STUN_GRENADE_MODEL );
}

void CASW_Grenade_PRifle::Detonate()
{
	if ( !ASWGameResource() )
		return;

	m_takedamage	= DAMAGE_NO;	

	CPASFilter filter( GetAbsOrigin() );
						
	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;

	Vector vecDir = -vecForward;
	//te->GaussExplosion( filter, 0.0,
				//GetAbsOrigin(), vecDir, 0 );
	CEffectData data;
	data.m_vOrigin = GetAbsOrigin();
	DispatchEffect( "aswstunexplo", data );

	EmitSound("ASW_Weapon_PRifle.StunGrenadeExplosion");

	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	UTIL_ASW_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	int nPreviousStunnedAliens = ASWGameResource()->m_iElectroStunnedAliens;

	// do just 1 damage...
	CTakeDamageInfo info( this, GetOwnerEntity(), 1, DMG_SHOCK );
	info.SetWeapon( m_hCreatorWeapon );
	RadiusDamage ( info , GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	// count as a shot fired
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOwnerEntity());
	if ( pMarine && pMarine->GetMarineResource() )
	{
		CASW_Marine_Resource *pMR = pMarine->GetMarineResource();
		pMR->UsedWeapon(NULL, 1);

		int nAliensStunned = ASWGameResource()->m_iElectroStunnedAliens - nPreviousStunnedAliens;
		if ( nAliensStunned >= 6 && pMR->IsInhabited() && pMR->GetCommander() )
		{
			 pMR->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_STUN_GRENADE );
			 pMR->m_bStunGrenadeMedal = true;
		}
	}

	UTIL_Remove( this );
}

void CASW_Grenade_PRifle::Precache( void )
{
	PrecacheModel( STUN_GRENADE_MODEL );

	BaseClass::Precache();
}

extern ConVar asw_grentrail_brightness;
extern ConVar asw_grentrail_width;
extern ConVar asw_grentrail_widthend;
extern ConVar asw_grentrail_lifetime;
extern ConVar asw_grentrail_fade;

void CASW_Grenade_PRifle::CreateEffects( void )
{
	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	DispatchParticleEffect( "prifle_grenade_fx", PATTACH_ABSORIGIN_FOLLOW, this, "fuse", false, -1, &filter );

	/*
	int	nAttachment = LookupAttachment( "fuse" );

	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->FollowEntity( this );
		m_pGlowTrail->SetAttachment( this, nAttachment );
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 255, 255, 255, asw_grentrail_brightness.GetInt(), kRenderFxNone );
		m_pGlowTrail->SetStartWidth( asw_grentrail_width.GetFloat() );
		m_pGlowTrail->SetEndWidth( asw_grentrail_widthend.GetFloat() );
		m_pGlowTrail->SetLifeTime( asw_grentrail_lifetime.GetFloat() );
		m_pGlowTrail->SetMinFadeLength(asw_grentrail_fade.GetFloat());
	}
	*/
}