#include "cbase.h"
#include "asw_ranger.h"
#include "npcevent.h"
#include "asw_gamerules.h"
#include "asw_shareddefs.h"
#include "asw_fx_shared.h"
#include "asw_grenade_cluster.h"
#include "world.h"
#include "particle_parse.h"
#include "asw_util_shared.h"
#include "ai_squad.h"
#include "asw_marine.h"
#include "gib.h"
#include "te_effect_dispatch.h"
#include "asw_ai_behavior.h"
#include "props_shared.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_ranger, CASW_Ranger );

IMPLEMENT_SERVERCLASS_ST( CASW_Ranger, DT_ASW_Ranger )
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Ranger )
DEFINE_EMBEDDEDBYREF( m_pExpresser ),
END_DATADESC()

ConVar asw_ranger_health( "asw_ranger_health", "101.5", FCVAR_CHEAT );
extern ConVar asw_debug_alien_damage;

extern int AE_MORTARBUG_LAUNCH;		// actual launch of the projectile
extern ConVar asw_drone_death_force_pitch;

//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
CASW_Ranger::CASW_Ranger()
{
	m_pszAlienModelName = "models/aliens/mortar3/mortar3.mdl";
}

void CASW_Ranger::SetupRangerShot( CASW_AlienShot &shot )
{
	shot.m_flSize = 4;
	shot.m_flDamage_direct = 12;
	shot.m_flDamage_splash = 0;
	shot.m_flSeek_strength = 0;
	shot.m_flGravity = 0;
	shot.m_flFuse = 5;
	shot.m_flBounce = 0;
	shot.m_bShootable = false;
	shot.m_strModel = "models/aliens/rangerSpit/rangerspit.mdl";
	shot.m_strSound_spawn = "ASW_Ranger_Projectile.Spawned";
	shot.m_strSound_hitNPC = "Ranger.projectileImpactPlayer";
	shot.m_strSound_hitWorld = "Ranger.projectileImpactWorld";
	shot.m_strParticles_trail = "ranger_projectile_main_trail";
	shot.m_strParticles_hit = "ranger_projectile_hit";
	CreateShot( "shot1", &shot );
}

//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
void CASW_Ranger::Spawn( void )
{
	SetHullType( HULL_MEDIUMBIG );

	BaseClass::Spawn();

	SetHullType( HULL_MEDIUMBIG );
	SetHealthByDifficultyLevel();
	SetBloodColor( BLOOD_COLOR_GREEN );
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK1 );

	SetIdealState( NPC_STATE_ALERT );

	m_bNeverRagdoll = true;

	//
	// Firing patterns
	// 

	// 3 shots
	CASW_AlienVolley volley;
	volley.m_rounds.SetCount( 3 );
	volley.m_rounds[0].m_flTime				= 0;
	volley.m_rounds[0].m_nShot_type			= GetShotIndex( "shot1" );
	volley.m_rounds[0].m_flStartAngle		= 0;
	volley.m_rounds[0].m_flEndAngle			= 0;
	volley.m_rounds[0].m_nNumShots			= 1;
	volley.m_rounds[0].m_flShotDelay		= 0;
	volley.m_rounds[0].m_flSpeed			= 425;
	volley.m_rounds[0].m_flHorizontalOffset = 0;

	volley.m_rounds[1].m_flTime				= 0.1;
	volley.m_rounds[1].m_nShot_type			= GetShotIndex( "shot1" );
	volley.m_rounds[1].m_flStartAngle		= -4;
	volley.m_rounds[1].m_flEndAngle			= 0;
	volley.m_rounds[1].m_nNumShots			= 1;
	volley.m_rounds[1].m_flShotDelay		= 0;
	volley.m_rounds[1].m_flSpeed			= 425;
	volley.m_rounds[1].m_flHorizontalOffset = 0;

	volley.m_rounds[2].m_flTime				= 0.2;
	volley.m_rounds[2].m_nShot_type			= GetShotIndex( "shot1" );
	volley.m_rounds[2].m_flStartAngle		= 4;
	volley.m_rounds[2].m_flEndAngle			= 0;
	volley.m_rounds[2].m_nNumShots			= 1;
	volley.m_rounds[2].m_flShotDelay		= 0;
	volley.m_rounds[2].m_flSpeed			= 425;
	volley.m_rounds[2].m_flHorizontalOffset = 0;
	CreateVolley( "volley1", &volley );
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
void CASW_Ranger::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/aliens/rangerSpit/rangerspit.mdl" );

	// precache shot model and fx, add shot to list
	CASW_AlienShot shot;
	SetupRangerShot( shot );
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
void CASW_Ranger::SetHealthByDifficultyLevel()
{
	int iHealth = MAX( 25, ASWGameRules()->ModifyAlienHealthBySkillLevel( asw_ranger_health.GetInt() ) );
	if ( asw_debug_alien_damage.GetBool() )
		Msg( "Setting ranger's initial health to %d\n", iHealth );
	SetHealth( iHealth );
	SetMaxHealth( iHealth );
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
float CASW_Ranger::MaxYawSpeed( void )
{
	if ( GetActivity() == ACT_STRAFE_LEFT || GetActivity() == ACT_STRAFE_RIGHT )
	{
		return 0.1f;
	}

	return 32.0f;// * GetMovementSpeedModifier();
}

//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
void CASW_Ranger::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_MORTARBUG_LAUNCH )
	{
		m_RangedAttackBehavior.HandleBehaviorEvent( this, BEHAVIOR_EVENT_MORTAR_FIRE, 0 );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
int CASW_Ranger::SelectDeadSchedule()
{
	if ( m_lifeState == LIFE_DEAD )
	{
		 return SCHED_NONE;
	}

	CleanupOnDeath();

	return SCHED_DIE;
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
void CASW_Ranger::BuildScheduleTestBits()
{
	// Ignore damage if we were recently damaged or we're attacking.
	if ( GetActivity() == ACT_MELEE_ATTACK1 )
	{
		ClearCustomInterruptCondition( COND_LIGHT_DAMAGE );
		ClearCustomInterruptCondition( COND_HEAVY_DAMAGE );
	}

	BaseClass::BuildScheduleTestBits();
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
bool CASW_Ranger::CreateBehaviors()
{
	AddBehavior( &m_CombatStunBehavior );
	m_CombatStunBehavior.Init();

	AddBehavior( &m_FlinchBehavior );
	m_FlinchBehavior.Init();

	//m_PrepareToEngageBehavior.KeyValue( "prepare_radius_min", "300" );
	//m_PrepareToEngageBehavior.KeyValue( "prepare_radius_max", "600" );
	//AddBehavior( &m_PrepareToEngageBehavior );
	//m_PrepareToEngageBehavior.Init();

	AddBehavior( &m_RetreatBehavior );
	m_RetreatBehavior.Init();

	m_RangedAttackBehavior.KeyValue( "minRange", "0" );
	m_RangedAttackBehavior.KeyValue( "maxRange", "600" );
	m_RangedAttackBehavior.KeyValue( "rate", "4.0" );
	m_RangedAttackBehavior.KeyValue( "global_shot_delay", "1" );
	m_RangedAttackBehavior.KeyValue( "volley_type", "volley1" );
	AddBehavior( &m_RangedAttackBehavior );
	m_RangedAttackBehavior.Init();

	m_ChaseEnemyBehavior.KeyValue( "chase_distance", "200" );
	AddBehavior( &m_ChaseEnemyBehavior );
	m_ChaseEnemyBehavior.Init();

	AddBehavior( &m_IdleBehavior );
	m_IdleBehavior.Init();

	return BaseClass::CreateBehaviors();
}

void CASW_Ranger::DeathSound( const CTakeDamageInfo &info )
{
	// if we are playing a fancy death animation, don't play death sounds from code
	// all death sounds are played from anim events inside the fancy death animation
	if ( m_nDeathStyle == kDIE_FANCY )
		return;

	//EmitSound( "ASW_Drone.Death" );

	if ( m_bOnFire )
		EmitSound( "ASW_Drone.DeathFireSizzle" );
	else
		EmitSound( "Ranger.GibSplatHeavy" );

}

//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
void CASW_Ranger::Event_Killed( const CTakeDamageInfo &info )
{
	CTakeDamageInfo newInfo(info);

	// scale up the force if we're shot by a marine, to make our ragdolling more interesting
	if (newInfo.GetAttacker() && newInfo.GetAttacker()->Classify() == CLASS_ASW_MARINE)
	{
		// scale based on the weapon used
		if (info.GetAmmoType() == GetAmmoDef()->Index("ASW_R")
			|| info.GetAmmoType() == GetAmmoDef()->Index("ASW_AG")
			|| info.GetAmmoType() == GetAmmoDef()->Index("ASW_P"))
			newInfo.ScaleDamageForce(22.0f);
		else if (info.GetAmmoType() == GetAmmoDef()->Index("ASW_PDW")
			|| info.GetAmmoType() == GetAmmoDef()->Index("ASW_SG"))
			newInfo.ScaleDamageForce(30.0f);
		else if (info.GetAmmoType() == GetAmmoDef()->Index("ASW_ASG"))
			newInfo.ScaleDamageForce(35.0f);

		// tilt the angle up a bit?
		Vector vecForceDir = newInfo.GetDamageForce();
		float force = vecForceDir.NormalizeInPlace();
		QAngle angForce;
		VectorAngles(vecForceDir, angForce);
		angForce[PITCH] += asw_drone_death_force_pitch.GetFloat();
		AngleVectors(angForce, &vecForceDir);
		vecForceDir *= force;
		newInfo.SetDamageForce(vecForceDir);
	}

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin() - Vector( 0, 0, 64 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	UTIL_DecalTrace( &tr, "GreenBloodBig" );

	BaseClass::Event_Killed( newInfo );
}


//-----------------------------------------------------------------------------
// Purpose:	
// Input:	
// Output:	
//-----------------------------------------------------------------------------
bool CASW_Ranger::CorpseGib( const CTakeDamageInfo &info )
{
	CEffectData	data;

	m_LagCompensation.UndoLaggedPosition();
	
	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = data.m_vOrigin - info.GetDamagePosition();
	VectorNormalize( data.m_vNormal );
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );
	data.m_nColor = m_nSkin;
	data.m_fFlags = IsOnFire() ? ASW_GIBFLAG_ON_FIRE : 0;

	//DispatchEffect( "DroneGib", data );

	//CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, GetAbsOrigin(), 256, 0.5f, this );	
	//EmitSound( "ASW_Drone.Death" );

	return true;
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( asw_ranger, CASW_Ranger )
	DECLARE_ANIMEVENT( AE_MORTARBUG_LAUNCH )
AI_END_CUSTOM_NPC()

