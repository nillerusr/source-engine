#include "cbase.h"
#include "asw_grenade_cluster.h"
#include "asw_gamerules.h"
#include "world.h"
#include "asw_util_shared.h"
#include "iasw_spawnable_npc.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_fx_shared.h"
#include "particle_parse.h"
#include "te_effect_dispatch.h"
#include "asw_achievements.h"
#include "asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CLUSTER_GRENADE_MODEL "models/swarm/grenades/HandGrenadeProjectile.mdl"

extern ConVar sk_plr_dmg_asw_r_g;
extern ConVar asw_grenade_vindicator_radius;
extern ConVar asw_vindicator_grenade_friction;
extern ConVar asw_vindicator_grenade_gravity;
extern ConVar asw_vindicator_grenade_elasticity;
ConVar asw_cluster_grenade_min_detonation_time("asw_cluster_grenade_min_detonation_time", "0.9f", FCVAR_CHEAT, "Min. time before cluster grenade can detonate");
ConVar asw_cluster_grenade_fuse("asw_cluster_grenade_fuse", "4.0f", FCVAR_CHEAT, "Fuse length of cluster grenade");
ConVar asw_cluster_grenade_radius_check_interval("asw_cluster_grenade_radius_check_interval", "0.5f", FCVAR_CHEAT, "How often the cluster grenade checks for nearby drones to explode against");
ConVar asw_cluster_grenade_radius_check_scale("asw_cluster_grenade_radius_check_scale", "0.6f", FCVAR_CHEAT, "What fraction of the grenade's damage radius is used for the early detonation check");
ConVar asw_cluster_grenade_child_fuse_min("asw_cluster_grenade_child_fuse_min", "0.5", FCVAR_CHEAT, "Cluster grenade child cluster's minimum fuse length");
ConVar asw_cluster_grenade_child_fuse_max("asw_cluster_grenade_child_fuse_max", "1.0", FCVAR_CHEAT, "Cluster grenade child cluster's maximum fuse length");

LINK_ENTITY_TO_CLASS( asw_grenade_cluster, CASW_Grenade_Cluster );

PRECACHE_REGISTER( asw_grenade_cluster );

BEGIN_DATADESC( CASW_Grenade_Cluster )	
	DEFINE_THINKFUNC( CheckNearbyDrones ),
	DEFINE_THINKFUNC( Detonate ),
	DEFINE_FIELD( m_fDetonateTime, FIELD_TIME ),
	DEFINE_FIELD( m_fEarliestAOEDetonationTime, FIELD_TIME ),
END_DATADESC()

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

void CASW_Grenade_Cluster::Spawn( void )
{
	Precache();
	SetModel(CLUSTER_GRENADE_MODEL);
	
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	m_flDamage		= sk_plr_dmg_asw_r_g.GetFloat();
	m_DmgRadius		= 220.0f;

	m_takedamage	= DAMAGE_YES;
	m_iHealth = 1;

	SetSize( -Vector(4,4,4), Vector(4,4,4) );
	SetSolid( SOLID_BBOX );
	SetGravity( asw_vindicator_grenade_gravity.GetFloat() );
	SetFriction( asw_vindicator_grenade_friction.GetFloat() );
	SetElasticity( asw_vindicator_grenade_elasticity.GetFloat() );
	SetCollisionGroup( ASW_COLLISION_GROUP_GRENADES );

	SetTouch( &CASW_Grenade_Vindicator::VGrenadeTouch );

	CreateEffects();
	m_hFirer = NULL;

	// Tumble in air
	QAngle vecAngVelocity( random->RandomFloat ( -100, -500 ), 0, 0 );
	SetLocalAngularVelocity( vecAngVelocity );

	m_fEarliestAOEDetonationTime = GetEarliestAOEDetonationTime();
	m_fEarliestTouchDetonationTime = GetEarliestTouchDetonationTime();

	m_iClusters = 0;
	m_bMaster = true;	

	//EmitSound( "ASWGrenade.Alarm" );
	SetFuseLength(asw_cluster_grenade_fuse.GetFloat());	

	if (m_fDetonateTime <= gpGlobals->curtime + asw_cluster_grenade_radius_check_interval.GetFloat())
	{
		SetThink( &CASW_Grenade_Cluster::Detonate );
		SetNextThink( m_fDetonateTime );
	}
	else
	{
		SetThink( &CASW_Grenade_Cluster::CheckNearbyDrones );
		SetNextThink( gpGlobals->curtime + asw_cluster_grenade_radius_check_interval.GetFloat() );
	}
	m_CreatorWeaponClass = (Class_T)CLASS_ASW_UNKNOWN;
}

void CASW_Grenade_Cluster::Precache()
{
	BaseClass::Precache();

	PrecacheModel(CLUSTER_GRENADE_MODEL);
	PrecacheScriptSound( "ASWGrenade.Alarm" );
	PrecacheParticleSystem( "explosion_grenade" );
}

void CASW_Grenade_Cluster::SetFuseLength(float fSeconds)
{
	m_fDetonateTime = gpGlobals->curtime + fSeconds;	
}

void CASW_Grenade_Cluster::CheckNearbyDrones()
{	
	// see if an alien is nearby
	if (gpGlobals->curtime >= m_fEarliestAOEDetonationTime)
	{
		float flRadius = asw_cluster_grenade_radius_check_scale.GetFloat() * m_DmgRadius;
		Vector vecSrc = GetAbsOrigin();
		CBaseEntity *pEntity = NULL;
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
		{
			if (!pEntity || !pEntity->IsNPC())
				continue;

			IASW_Spawnable_NPC *pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(pEntity);
			if (!pSpawnable)
				continue;
		
			Detonate();
			return;
		}

		if (gpGlobals->curtime >= m_fDetonateTime)
		{
			Detonate();
			return;
		}
	}
	
	if (m_fDetonateTime <= gpGlobals->curtime + asw_cluster_grenade_radius_check_interval.GetFloat())
	{
		SetThink( &CASW_Grenade_Cluster::Detonate );
		SetNextThink( m_fDetonateTime );
	}
	else
	{
		SetThink( &CASW_Grenade_Cluster::CheckNearbyDrones );
		SetNextThink( gpGlobals->curtime + asw_cluster_grenade_radius_check_interval.GetFloat() );
	}
}

CASW_Grenade_Cluster* CASW_Grenade_Cluster::Cluster_Grenade_Create( float flDamage, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon )
{
	CASW_Grenade_Cluster *pGrenade = (CASW_Grenade_Cluster *)CreateEntityByName( "asw_grenade_cluster" );
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->m_flDamage = flDamage;
	pGrenade->m_DmgRadius = fRadius;
	pGrenade->m_hFirer = pOwner;
	pGrenade->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetClusters(iClusters, true);
	pGrenade->m_hCreatorWeapon = pCreatorWeapon;
	if( pCreatorWeapon )
		pGrenade->m_CreatorWeaponClass = pCreatorWeapon->Classify();

	// hack attack!  for some reason, grenades refuse to be affect by damage forces until they're actually dead
	//  so we kill it immediately.
	pGrenade->TakeDamage(CTakeDamageInfo(pGrenade, pGrenade, Vector(0, 0, 1), position, 10, DMG_SLASH));

	return pGrenade;
}

void CASW_Grenade_Cluster::DoExplosion()
{
	// scorch the ground
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -80 ), MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	if (m_bMaster)
	{
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
	}

	UTIL_ASW_GrenadeExplosion( GetAbsOrigin(), m_DmgRadius );

	EmitSound( "ASWGrenade.Explode" );

	// damage to nearby things
	CTakeDamageInfo info( this, m_hFirer.Get(), m_flDamage, DMG_BLAST );
	info.SetWeapon( m_hCreatorWeapon );
	ASWGameRules()->RadiusDamage( info, GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );
}

extern ConVar asw_medal_explosive_kills;

void CASW_Grenade_Cluster::Detonate()
{
	m_takedamage	= DAMAGE_NO;	

	int iPreExplosionKills = 0;
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(m_hFirer.Get());
	if (pMarine && pMarine->GetMarineResource())
		iPreExplosionKills = pMarine->GetMarineResource()->m_iAliensKilled;
	
	DoExplosion();

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
		if (m_bKicked)
			pMarine->GetMarineResource()->m_iKickedGrenadeKills += iKilledByExplosion;

		// primary cluster counts as a shot fired
		if (m_iClusters > 0)
		{
			pMarine->GetMarineResource()->UsedWeapon(NULL, 1);
		}
	}

	while (m_iClusters > 0)
	{
		float fYaw = random->RandomFloat(0, 360);
		QAngle ang(0, fYaw, 0);
		Vector newVel;
		AngleVectors(ang, &newVel);
		newVel *= random->RandomFloat(150, 200);
		newVel.z = random->RandomFloat(200, 400);
		CASW_Grenade_Cluster *pGrenade = CASW_Grenade_Cluster::Cluster_Grenade_Create( 
			m_flDamage,
			m_DmgRadius,
			0,
			GetAbsOrigin(), ang, newVel, AngularImpulse(0,0,0), m_hFirer.Get() , m_hCreatorWeapon);
		if (pGrenade)
		{
			pGrenade->m_fEarliestAOEDetonationTime = 0;	// children can go whenever they want
			pGrenade->m_fEarliestTouchDetonationTime = 0;
			pGrenade->m_takedamage = DAMAGE_NO;
			pGrenade->SetFuseLength(random->RandomFloat(asw_cluster_grenade_child_fuse_min.GetFloat(), asw_cluster_grenade_child_fuse_max.GetFloat()));
			pGrenade->SetClusters(0, false);	
			pGrenade->m_bKicked = m_bKicked;
		}
		m_iClusters--;
	}

	UTIL_Remove( this );
}

float CASW_Grenade_Cluster::GetEarliestAOEDetonationTime()
{
	return gpGlobals->curtime + asw_cluster_grenade_min_detonation_time.GetFloat();
}

void CASW_Grenade_Cluster::SetClusters(int iClusters, bool bMaster)
{
	BaseClass::SetClusters(iClusters, bMaster);
	if (!bMaster)
	{
		SetThink( &CASW_Grenade_Cluster::Detonate );
		SetNextThink( m_fDetonateTime );
	}
}