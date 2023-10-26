#include "cbase.h"
#include "asw_rifle_grenade.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "asw_gamerules.h"
#include "iasw_spawnable_npc.h"
#include "world.h"
#include "asw_util_shared.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_fx_shared.h"
#include "particle_parse.h"
#include "asw_achievements.h"
#include "asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_plr_dmg_asw_r_g;
ConVar asw_grenade_radius("asw_grenade_radius", "350", FCVAR_CHEAT, "Radius of the rifle's grenade explosion");

const float GRENADE_COEFFICIENT_OF_RESTITUTION = 0.2f;

#define GRENADE_MODEL "models/swarm/grenades/RifleGrenadeProjectile.mdl"

LINK_ENTITY_TO_CLASS( asw_rifle_grenade, CASW_Rifle_Grenade );

PRECACHE_REGISTER(asw_rifle_grenade);

BEGIN_DATADESC( CASW_Rifle_Grenade )
	DEFINE_FUNCTION( GrenadeTouch ),
		
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_DmgRadius, FIELD_FLOAT ),
END_DATADESC()

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Rifle_Grenade::~CASW_Rifle_Grenade( void )
{
	KillEffects();
}

void CASW_Rifle_Grenade::Spawn( void )
{
	Precache( );

	SetModel( GRENADE_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );

	m_flDamage		= sk_plr_dmg_asw_r_g.GetFloat();
	m_DmgRadius		= asw_grenade_radius.GetFloat();

	m_takedamage	= DAMAGE_NO;

	SetSize( -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.05f );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	//CreateVPhysics();

	SetTouch( &CASW_Rifle_Grenade::GrenadeTouch );

	CreateEffects();

	//AddSolidFlags( FSOLID_NOT_STANDABLE );

	//BaseClass::Spawn();
	// projectile only lasts 1 second
	SetThink( &CASW_Rifle_Grenade::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );

	//AddSolidFlags(FSOLID_TRIGGER);
	//m_pLastHit = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Rifle_Grenade::OnRestore( void )
{
	CreateEffects();

	BaseClass::OnRestore();
}


void CASW_Rifle_Grenade::KillEffects()
{
	
}

ConVar asw_grentrail_brightness("asw_grentrail_brightness", "128");
ConVar asw_grentrail_width("asw_grentrail_width", "6.0f");
ConVar asw_grentrail_widthend("asw_grentrail_widthend", "1.0f");
ConVar asw_grentrail_lifetime("asw_grentrail_lifetime", "0.2f");
ConVar asw_grentrail_fade("asw_grentrail_fade", "100");

void CASW_Rifle_Grenade::CreateEffects( void )
{
	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	DispatchParticleEffect( "rifle_grenade_fx", PATTACH_ABSORIGIN_FOLLOW, this, "fuse", false, -1, &filter );

	/*
	int	nAttachment = LookupAttachment( "fuse" );
	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "swarm/sprites/greylaser1.vmt", GetLocalOrigin(), false );

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

bool CASW_Rifle_Grenade::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CASW_Rifle_Grenade::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX );
	//return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) | CONTENTS_GRATE;
}

void CASW_Rifle_Grenade::Precache( void )
{
	PrecacheModel( GRENADE_MODEL );	

	BaseClass::Precache();
}

void CASW_Rifle_Grenade::GrenadeTouch( CBaseEntity *pOther )
{
	if (!pOther)
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	// make sure we don't die on things we shouldn't
	if (!ASWGameRules() || !ASWGameRules()->ShouldCollide(GetCollisionGroup(), pOther->GetCollisionGroup()))
		return;

	Detonate();
	return;

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize( vecNormalizedVel );

		if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_NEVERGIB );
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}
		else
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_BULLET | DMG_NEVERGIB );
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}

		ApplyMultiDamage();

		
		// play body "thwack" sound
		EmitSound( "Weapon_Crossbow.BoltHitBody" );

		Vector vForward;

		AngleVectors( GetAbsAngles(), &vForward );
		VectorNormalize ( vForward );

		UTIL_TraceLine( GetAbsOrigin(),	GetAbsOrigin() + vForward * 128, MASK_OPAQUE, pOther, COLLISION_GROUP_NONE, &tr2 );

		if ( tr2.fraction != 1.0f )
		{
//			NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
//			NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;
			
				//DispatchEffect( "BoltImpact", data );
			}
		}		
		
		SetTouch( NULL );
		SetThink( NULL );

		KillEffects();
		UTIL_Remove( this );
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			//EmitSound( "Weapon_Crossbow.BoltHitWorld" );

			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();
			VectorNormalize( vecDir );
								
			SetMoveType( MOVETYPE_NONE );
		
			Vector vForward;

			AngleVectors( GetAbsAngles(), &vForward );
			VectorNormalize ( vForward );

			CEffectData	data;

			data.m_vOrigin = tr.endpos;
			data.m_vNormal = vForward;
			data.m_nEntIndex = 0;
		
			//DispatchEffect( "BoltImpact", data );
			
			UTIL_ImpactTrace( &tr, DMG_BULLET );

			AddEffects( EF_NODRAW );
			SetTouch( NULL );
			KillEffects();
			SetThink( &CASW_Rifle_Grenade::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 2.0f );
			
			// Shoot some sparks
			if ( UTIL_PointContents( GetAbsOrigin(), CONTENTS_WATER ) != CONTENTS_WATER)
			{
				g_pEffects->Sparks( GetAbsOrigin() );
			}
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_BULLET );
			}
			KillEffects();
			UTIL_Remove( this );
		}
	}
}

CASW_Rifle_Grenade* CASW_Rifle_Grenade::Rifle_Grenade_Create( float flDamage, float fRadius, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreator )
{
	CASW_Rifle_Grenade *pGrenade = (CASW_Rifle_Grenade *)CreateEntityByName( "asw_rifle_grenade" );	
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->m_flDamage = flDamage;
	pGrenade->m_DmgRadius = fRadius;
	pGrenade->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->m_hCreatorWeapon.Set( pCreator );

	return pGrenade;
}

extern ConVar asw_medal_explosive_kills;

void CASW_Rifle_Grenade::Detonate()
{
	m_takedamage	= DAMAGE_NO;	

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
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

	UTIL_ASW_GrenadeExplosion( GetAbsOrigin(), m_DmgRadius );

	EmitSound( "ASWGrenade.Explode" );

	int iPreExplosionKills = 0;
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOwnerEntity());
	if (pMarine && pMarine->GetMarineResource())
		iPreExplosionKills = pMarine->GetMarineResource()->m_iAliensKilled;

	CTakeDamageInfo info( this, GetOwnerEntity(), m_flDamage, DMG_BLAST );
	info.SetWeapon( m_hCreatorWeapon );

	ASWGameRules()->RadiusDamage ( info, GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	if (pMarine && pMarine->GetMarineResource())
	{
		int iKilledByExplosion = pMarine->GetMarineResource()->m_iAliensKilled - iPreExplosionKills;
		if (iKilledByExplosion > pMarine->GetMarineResource()->m_iSingleGrenadeKills)
		{
			pMarine->GetMarineResource()->m_iSingleGrenadeKills = iKilledByExplosion;
			if ( iKilledByExplosion > asw_medal_explosive_kills.GetInt() && pMarine->GetCommander() && pMarine->IsInhabited() )
			{
				pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_GRENADE_MULTI_KILL );
			}
		}

		// count as a shot fired
		pMarine->GetMarineResource()->UsedWeapon(NULL, 1);
	}

	UTIL_Remove( this );
}