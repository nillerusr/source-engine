#include "cbase.h"
#include "asw_boomer_blob.h"
#include "asw_gamerules.h"
#include "world.h"
#include "asw_util_shared.h"
#include "iasw_spawnable_npc.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "particle_parse.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_BOOMER_BLOB_MODEL "models/aliens/sack/sack.mdl"
#define ASW_BOOMER_WARN_DELAY 2.0f

extern ConVar asw_vindicator_grenade_elasticity;
static ConVar asw_boomer_blob_friction( "asw_boomer_blob_friction", "0.5f", FCVAR_CHEAT, "Friction");
static ConVar asw_boomer_blob_min_detonation_time( "asw_boomer_blob_min_detonation_time", "0.9f", FCVAR_CHEAT, "Min. time before cluster grenade can detonate" );
static ConVar asw_boomer_blob_fuse( "asw_boomer_blob_fuse", "2.75f", FCVAR_CHEAT, "Fuse length of cluster grenade" );
static ConVar asw_boomer_blob_radius_check_interval( "asw_boomer_blob_radius_check_interval", "0.5f", FCVAR_CHEAT, "How often the cluster grenade checks for nearby drones to explode against" );
static ConVar asw_boomer_blob_radius_check_scale( "asw_boomer_blob_radius_check_scale", "0.6f", FCVAR_CHEAT, "What fraction of the grenade's damage radius is used for the early detonation check" );
static ConVar asw_boomer_blob_child_fuse_min( "asw_boomer_blob_child_fuse_min", "0.5", FCVAR_CHEAT, "Cluster grenade child cluster's minimum fuse length" );
static ConVar asw_boomer_blob_child_fuse_max( "asw_boomer_blob_child_fuse_max", "1.0", FCVAR_CHEAT, "Cluster grenade child cluster's maximum fuse length" );
static ConVar asw_boomer_blob_gravity( "asw_boomer_blob_gravity", "0.8f", FCVAR_CHEAT, "Gravity of mortar bug's mortar" );

CUtlVector<CBaseEntity*> g_aExplosiveProjectiles;

LINK_ENTITY_TO_CLASS( asw_boomer_blob, CASW_Boomer_Blob );

PRECACHE_REGISTER( asw_boomer_blob );

BEGIN_DATADESC( CASW_Boomer_Blob )	
	DEFINE_THINKFUNC( CheckNearbyTargets ),
	DEFINE_THINKFUNC( Detonate ),
	DEFINE_FIELD( m_fDetonateTime, FIELD_TIME ),
	DEFINE_FIELD( m_fEarliestAOEDetonationTime, FIELD_TIME ),
END_DATADESC()

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

CASW_Boomer_Blob::CASW_Boomer_Blob()
{
	g_aExplosiveProjectiles.AddToTail( this );
}

CASW_Boomer_Blob::~CASW_Boomer_Blob()
{
	g_aExplosiveProjectiles.FindAndRemove( this );
}

void CASW_Boomer_Blob::Spawn( void )
{
	//d:\dev\main\game\infested\models\swarm\mortarbugprojectile\mortarbugprojectile.mdl
	Precache();
	SetModel( ASW_BOOMER_BLOB_MODEL );
	
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	m_flDamage		= 80;
	m_DmgRadius		= 165.0f;  // NOTE: this gets overriden

	m_takedamage	= DAMAGE_NO;
	m_iHealth = 1;

	m_bModelOpening = false;

	SetSize( -Vector(4,4,4), Vector(4,4,4) );
	SetSolid( SOLID_BBOX );
	SetGravity( asw_boomer_blob_gravity.GetFloat() );
	SetFriction( asw_boomer_blob_friction.GetFloat() );
	SetElasticity( asw_vindicator_grenade_elasticity.GetFloat() );
	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );

	SetTouch( &CASW_Boomer_Blob::Touch );

	//CreateEffects();
	m_hFirer = NULL;

	// Tumble in air
	QAngle vecAngVelocity( random->RandomFloat ( -100, -500 ), 0, 0 );
	SetLocalAngularVelocity( vecAngVelocity );

	m_fEarliestAOEDetonationTime = GetEarliestAOEDetonationTime();
	m_fEarliestTouchDetonationTime = GetEarliestTouchDetonationTime();

	m_iClusters = 0;
	m_bMaster = true;	

	ResetSequence( LookupSequence( "MortarBugProjectile_Closed" ) );

	//EmitSound( "ASWGrenade.Alarm" );
	SetFuseLength( asw_boomer_blob_fuse.GetFloat() + RandomFloat( -0.25, 0.66f ) );	
	

	if ( m_fDetonateTime <= gpGlobals->curtime + asw_boomer_blob_radius_check_interval.GetFloat() )
	{
		SetThink( &CASW_Boomer_Blob::Detonate );
		SetNextThink( m_fDetonateTime );
	}
	else
	{
		SetThink( &CASW_Boomer_Blob::CheckNearbyTargets );
		SetNextThink( gpGlobals->curtime + asw_boomer_blob_radius_check_interval.GetFloat() );
	}
}

void CASW_Boomer_Blob::Precache( )
{
	BaseClass::Precache();

	PrecacheModel( ASW_BOOMER_BLOB_MODEL );
	//PrecacheScriptSound( "ASWGrenade.Alarm" );
	PrecacheScriptSound( "ASW_Boomer_Grenade.Explode" );
	PrecacheScriptSound( "ASW_Boomer_Projectile.Spawned" );
	PrecacheParticleSystem( "boomer_projectile_main_trail" );
	PrecacheParticleSystem( "boomer_drop_explosion" );
	PrecacheParticleSystem( "boomer_grenade_open" );
	PrecacheParticleSystem( "mortar_launch" );
}

void CASW_Boomer_Blob::SetFuseLength( float fSeconds )
{
	m_fDetonateTime = gpGlobals->curtime + fSeconds;	
}

void CASW_Boomer_Blob::Touch( CBaseEntity *pOther )
{
	if ( !pOther || pOther == GetOwnerEntity() )
	{
		return;
	}

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
	{
		return;
	}

/*
	if ( pOther == GetWorldEntity() )
	{
		// lay flat
		QAngle angMortar = GetAbsAngles();
		SetAbsAngles( QAngle( 0, angMortar.x, 0 ) );

		SetAbsVelocity( vec3_origin );
	}
*/

	if ( m_bExplodeOnWorldContact )
	{
		Detonate();
	}

	// make sure we don't die on things we shouldn't
	if ( !ASWGameRules() || !ASWGameRules()->ShouldCollide( GetCollisionGroup(), pOther->GetCollisionGroup() ) )
	{
		return;
	}

	if ( pOther->m_takedamage == DAMAGE_NO )
	{
		if (GetAbsVelocity().Length2D() > 60)
		{
			EmitSound("Grenade.ImpactHard");
		}
	}

	// can't detonate yet?
	if ( gpGlobals->curtime < m_fEarliestTouchDetonationTime )
	{
		return;
	}

	// we don't want the mortar grenade to explode on impact of marine's because 
	// it's more fun to have them avoid the area it landed in than to take blind damage
	/*
	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		if ( pOther->IsNPC() && pOther->Classify() != CLASS_PLAYER_ALLY_VITAL )
		{
			Detonate();
		}
	}*/
}

void CASW_Boomer_Blob::CheckNearbyTargets( )
{	
	// see if an alien is nearby
	if ( gpGlobals->curtime >= m_fEarliestAOEDetonationTime )
	{
		if ( !m_bModelOpening && gpGlobals->curtime >= (m_fDetonateTime - ASW_BOOMER_WARN_DELAY) )
		{
			// we are one second from detonating, commit to detonating and start the opening sequence
			// regardless of anyone nearby
			m_bModelOpening = true;
			ResetSequence( LookupSequence( "MortarBugProjectile_Opening" ) );

			CEffectData	data;
			data.m_vOrigin = GetAbsOrigin();
			CPASFilter filter( data.m_vOrigin );
			filter.SetIgnorePredictionCull(true);
			DispatchParticleEffect( "boomer_grenade_open", PATTACH_ABSORIGIN_FOLLOW, this, -1, false, -1, &filter );
		}

		// if we exceeded the detonation time, just detonate
		if ( gpGlobals->curtime >= m_fDetonateTime )
		{
			Detonate();
			return;
		}

		// if the model is opening, do the animation advance
		if ( m_bModelOpening )
		{
			StudioFrameAdvance();
			SetNextThink( gpGlobals->curtime );
			return;
		}

		float flRadius = asw_boomer_blob_radius_check_scale.GetFloat() * m_DmgRadius;
		Vector vecSrc = GetAbsOrigin();
		CBaseEntity *pEntity = NULL;
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
		{
			if ( !pEntity || !pEntity->IsPlayer() )
				continue;

			// give them a 2 second warning before detonation
			m_fDetonateTime = MIN( m_fDetonateTime, m_fDetonateTime + ASW_BOOMER_WARN_DELAY );
		}
	}
	
	if ( m_fDetonateTime <= gpGlobals->curtime + asw_boomer_blob_radius_check_interval.GetFloat() )
	{
		SetThink( &CASW_Boomer_Blob::Detonate );
		SetNextThink( m_fDetonateTime );
	}
	else
	{
		SetThink( &CASW_Boomer_Blob::CheckNearbyTargets );
		SetNextThink( gpGlobals->curtime + asw_boomer_blob_radius_check_interval.GetFloat() );
	}
}

CASW_Boomer_Blob* CASW_Boomer_Blob::Boomer_Blob_Create( float flDamage, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, 
														const AngularImpulse &angVelocity, CBaseEntity *pOwner )
{
	CASW_Boomer_Blob *pGrenade = ( CASW_Boomer_Blob * )CreateEntityByName( "asw_boomer_blob" );
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->m_flDamage = flDamage;
	pGrenade->m_DmgRadius = fRadius;
	pGrenade->m_hFirer = pOwner;
	pGrenade->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetClusters( iClusters, true );
	pGrenade->CreateEffects();

	// hack attack!  for some reason, grenades refuse to be affect by damage forces until they're actually dead
	//  so we kill it immediately.
//	pGrenade->TakeDamage(CTakeDamageInfo(pGrenade, pGrenade, Vector(0, 0, 1), position, 10, DMG_SLASH));

	return pGrenade;
}

void CASW_Boomer_Blob::DoExplosion( )
{
	// scorch the ground
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -80 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	if ( m_bMaster )
	{
		if ( ( tr.m_pEnt != GetWorldEntity() ) || ( tr.hitbox != 0 ) )
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

		UTIL_ASW_ScreenShake( GetAbsOrigin(), 7.0f, 45.0f, 0.5f, 150, SHAKE_START );
	}

	// explosion effects
	DispatchParticleEffect( "boomer_drop_explosion", GetAbsOrigin(), Vector( m_DmgRadius, 0.0f, 0.0f ), QAngle( 0.0f, 0.0f, 0.0f ) );
	EmitSound( "ASW_Boomer_Grenade.Explode" );

	// damage to nearby things
	ASWGameRules()->RadiusDamage( CTakeDamageInfo( this, m_hFirer.Get(), m_flDamage, DMG_BLAST ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );
}

void CASW_Boomer_Blob::Detonate( )
{
	m_takedamage = DAMAGE_NO;

	DoExplosion();

	while ( m_iClusters > 0 )
	{
		float fYaw = random->RandomFloat( 0.0f, 360.0f );
		QAngle ang( 0.0f, fYaw, 0.0f );
		Vector newVel;
		AngleVectors( ang, &newVel );
		newVel *= random->RandomFloat( 150.0f, 200.0f );
		newVel.z = random->RandomFloat( 200.0f, 400.0f );
		CASW_Boomer_Blob *pGrenade = CASW_Boomer_Blob::Boomer_Blob_Create( m_flDamage, m_DmgRadius, 0,
			GetAbsOrigin(), ang, newVel, AngularImpulse( 0.0f, 0.0f, 0.0f ), m_hFirer.Get() );
		if (pGrenade)
		{
			pGrenade->m_fEarliestAOEDetonationTime = 0;	// children can go whenever they want
			pGrenade->m_fEarliestTouchDetonationTime = 0;
			pGrenade->m_takedamage = DAMAGE_NO;
			pGrenade->SetFuseLength( random->RandomFloat( asw_boomer_blob_child_fuse_min.GetFloat(), asw_boomer_blob_child_fuse_max.GetFloat() ) );
			pGrenade->SetClusters( 0, false );	
			pGrenade->m_bKicked = m_bKicked;
		}
		m_iClusters--;
	}

	UTIL_Remove( this );
}

float CASW_Boomer_Blob::GetEarliestAOEDetonationTime( )
{
	return gpGlobals->curtime + asw_boomer_blob_min_detonation_time.GetFloat();
}

void CASW_Boomer_Blob::SetClusters( int iClusters, bool bMaster )
{
	BaseClass::SetClusters( iClusters, bMaster );
	if ( !bMaster )
	{
		SetThink( &CASW_Boomer_Blob::Detonate );
		SetNextThink( m_fDetonateTime );
	}
}

void CASW_Boomer_Blob::CreateEffects()
{
	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	DispatchParticleEffect( "boomer_projectile_main_trail", PATTACH_ABSORIGIN_FOLLOW, this, -1, false, -1, &filter );
	EmitSound( "ASW_Boomer_Projectile.Spawned" );
}
