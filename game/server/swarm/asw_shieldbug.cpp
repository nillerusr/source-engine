#include "cbase.h"
#include "asw_shieldbug.h"
#include "asw_marine.h"
#include "asw_shareddefs.h"
#include "npc_bullseye.h"
#include "npcevent.h"
#include "asw_marine.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "soundenvelope.h"
#include "te_effect_dispatch.h"
#include "asw_fx_shared.h"
#include "asw_util_shared.h"
#include "IEffects.h"
#include "asw_marine_speech.h"
#include "asw_gamerules.h"
#include "asw_weapon_assault_shotgun_shared.h"
#include "te.h"
#include "props_shared.h"
#include "asw_fail_advice.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// asw todo: push any blocking sparks out to the claws/head, even if the damage hits inside

#define ASW_SHIELDBUG_MELEE1_START_ATTACK_RANGE 100.0f
#define ASW_SHIELDBUG_MELEE1_RANGE 100.0f
#define ASW_SHIELDBUG_FLINCH_INTERVAL 2.0f
#define ASW_SHIELDBUG_TRANSITION_SPEED 1600.0f

// activities
int ACT_SHIELDBUG_PRE_LEAP;
int ACT_START_DEFENDING;
int ACT_LEAVE_DEFENDING;
int ACT_RUN_DEFEND;
int ACT_IDLE_DEFEND;
int ACT_FLINCH_LEFTARM_DEFEND;
int ACT_FLINCH_RIGHTARM_DEFEND;
int ACT_MELEE_ATTACK1_DEFEND;
int ACT_TURN_LEFT_DEFEND;
int ACT_TURN_RIGHT_DEFEND;
int ACT_ALIEN_SHOVER_ROAR_DEFEND;

// sb anim events
int AE_SHIELDBUG_WALK_FOOTSTEP;
int AE_SHIELDBUG_FOOTSTEP_SOFT;
int AE_SHIELDBUG_FOOTSTEP_HEAVY;
int AE_SHIELDBUG_MELEE_HIT1;
int AE_SHIELDBUG_MELEE_HIT2;
int AE_SHIELDBUG_MELEE1_SOUND;
int AE_SHIELDBUG_MELEE2_SOUND;
int AE_SHIELDBUG_MOUTH_BLEED;
int AE_SHIELDBUG_ALERT_SOUND;
int AE_SHIELDBUG_SCREENSHAKE;
int AE_SHIELDBUG_START_DEFEND;
int AE_SHIELDBUG_LEAVE_DEFEND;

ConVar sk_asw_shieldbug_damage( "sk_asw_shieldbug_damage", "25.0", FCVAR_CHEAT, "Damage per swipe from the shieldbug");
ConVar asw_shieldbug_speedboost( "asw_shieldbug_speedboost", "1.0",FCVAR_CHEAT , "boost speed for the shieldbug" );
ConVar asw_shieldbug_defending_speedboost( "asw_shieldbug_defending_speedboost", "1.0", FCVAR_CHEAT, "Boost speed for the shieldbug when defending");
ConVar asw_debug_shieldbug("asw_debug_shieldbug", "0", FCVAR_CHEAT, "Display shieldbug debug messages");
ConVar asw_shieldbug_screen_shake("asw_shieldbug_screen_shake", "1", FCVAR_CHEAT, "Should the shieldbug cause screen shake?");
ConVar asw_shieldbug_melee_force("asw_shieldbug_melee_force", "2.0", FCVAR_CHEAT, "Melee force of the shieldbug");
ConVar asw_sb_gallop_min_range("asw_sb_gallop_min_range", "50.0", FCVAR_CHEAT, "Min range to do ram attack");
ConVar asw_sb_gallop_max_range("asw_sb_gallop_max_range", "130.0", FCVAR_CHEAT, "Max range to do ram attack");
ConVar asw_old_shieldbug ("asw_old_shieldbug", "0", FCVAR_CHEAT, "1= old shield bug, 0 = new model");
ConVar asw_shieldbug_force_defend("asw_shieldbug_force_defend", "0", FCVAR_CHEAT, "0 = no force, 1 = force open, 2 = force defend");
extern ConVar sv_gravity;
extern ConVar asw_debug_marine_chatter;

IMPLEMENT_AUTO_LIST( IShieldbugAutoList );

float CASW_Shieldbug::s_fNextSpottedChatterTime = 0;

CASW_Shieldbug::CASW_Shieldbug( void )
{
	m_fMarineBlockCounter = 0;
	m_fLastMarineBlockTime = 0;
	m_flDefendDuration = RandomFloat( 6.0f, 10.0f );
	if ( asw_old_shieldbug.GetBool() )
	{
		m_pszAlienModelName = SWARM_SHIELDBUG_MODEL;
	}
	else
	{
		m_pszAlienModelName = SWARM_NEW_SHIELDBUG_MODEL;
	}
	m_bLastShouldDefend = false;  
	m_nDeathStyle = kDIE_FANCY;
}

LINK_ENTITY_TO_CLASS( asw_shieldbug, CASW_Shieldbug );

IMPLEMENT_SERVERCLASS_ST( CASW_Shieldbug, DT_ASW_Shieldbug )
	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Shieldbug )
	DEFINE_FIELD(m_bDefending, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fNextFlinchTime, FIELD_FLOAT),
	DEFINE_FIELD(m_fLastHurtTime, FIELD_TIME),
	DEFINE_FIELD(m_fNextHeadhitAttack, FIELD_TIME),
	DEFINE_KEYFIELD(m_nExtraHeath, FIELD_INTEGER, "extrahealth"),
END_DATADESC()

void CASW_Shieldbug::Spawn( void )
{
	SetHullType(HULL_WIDE_SHORT);

	BaseClass::Spawn();

	m_bHasBeenHurt = false;
	
	SetHullType(HULL_WIDE_SHORT);

	SetViewOffset( Vector(6, 0, 11) ) ;		// Position of the eyes relative to NPC's origin.

	SetHealthByDifficultyLevel();	
	m_bDefending = false;

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2);	// | bits_CAP_MOVE_JUMP
	CapabilitiesRemove( bits_CAP_MOVE_JUMP );

	SetCollisionGroup( ASW_COLLISION_GROUP_BIG_ALIEN );

	m_takedamage = DAMAGE_NO;	// alien is invulnerable until she finds her first enemy

	m_bNeverInstagib = true;
}

CASW_Shieldbug::~CASW_Shieldbug()
{
	
}

void CASW_Shieldbug::Precache( void )
{
	PrecacheScriptSound("ASW_Drone.Alert");
	PrecacheScriptSound("ASW_Drone.Attack");	
	PrecacheScriptSound("ASW_Parasite.Death");
	PrecacheScriptSound("ASW_Parasite.Idle");
	PrecacheScriptSound("ASW_Parasite.Attack");
	
	PrecacheScriptSound("ASW_ShieldBug.StepLight");
	PrecacheScriptSound( "ASW_ShieldBug.Pain" );
	PrecacheScriptSound( "ASW_ShieldBug.Alert" );
	PrecacheScriptSound( "ASW_ShieldBug.Death" );
	PrecacheScriptSound( "ASW_ShieldBug.Attack" );
	PrecacheScriptSound( "ASW_ShieldBug.Circle" );
	PrecacheScriptSound( "ASW_ShieldBug.Idle" );

	// these are all his breakables
	PrecacheModel( "models/aliens/shieldbug/gib_back_leg.mdl");
	PrecacheModel( "models/aliens/shieldbug/gib_leg_claw.mdl");
	PrecacheModel( "models/aliens/shieldbug/gib_leg_middle.mdl");
	PrecacheModel( "models/aliens/shieldbug/gib_leg_upper.mdl");
	PrecacheModel( "models/aliens/shieldbug/gib_leg_l.mdl");
	PrecacheModel( "models/aliens/shieldbug/gib_leg_r.mdl");

	// particles
	PrecacheParticleSystem ( "shieldbug_brain_explode" );
	PrecacheParticleSystem ( "shieldbug_fountain" );   
	PrecacheParticleSystem ( "shieldbug_body_explode" );

	BaseClass::Precache();
}

float CASW_Shieldbug::GetIdealSpeed() const
{
	float boost = asw_shieldbug_speedboost.GetFloat();
	if ( m_bDefending )
	{
		boost = 1.0f;	 //asw_shieldbug_defending_speedboost.GetFloat();
	}

	return boost * BaseClass::GetIdealSpeed() * m_flPlaybackRate;
}

float CASW_Shieldbug::GetSequenceGroundSpeed( int iSequence )
{
	float t = SequenceDuration( iSequence );

	if (t > 0)
	{
		float move_dist = GetSequenceMoveDist( iSequence );
		if (move_dist == 0)
		{
//			return 200;
			/*
			if (iSequence == 2)
				return 150;
			else if (iSequence == 3)
				return 300;
			else if (iSequence == 6)
				return 300;
			else if (iSequence == 7)
				return 300;*/
		}
		return move_dist / t;
	}
	else
	{
		return 0;
	}
}


float CASW_Shieldbug::GetIdealAccel( ) const
{
	return GetIdealSpeed() * 1.5f;
}

float CASW_Shieldbug::MaxYawSpeed( void )
{
	Activity eActivity = GetActivity();

	// Stay still while attacking
	if ( eActivity == ACT_MELEE_ATTACK1 )
		return 0.1f;

	if ( m_bElectroStunned.Get() )
		return 0.1f;

	switch( eActivity )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 20.0;
		break;
	
	case ACT_RUN:
	default:
		return 20.0f;
		break;
	}
}

void CASW_Shieldbug::AlertSound()
{
	EmitSound("ASW_ShieldBug.Alert");
}

void CASW_Shieldbug::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound)
	{
		EmitSound("ASW_ShieldBug.Pain");
		m_fNextPainSound = gpGlobals->curtime + RandomFloat(0.5f, 1.5f);
	}
}

void CASW_Shieldbug::AttackSound()
{
	EmitSound("ASW_ShieldBug.Attack");
}

void CASW_Shieldbug::IdleSound()
{
	EmitSound("ASW_ShieldBug.Idle");
}

void CASW_Shieldbug::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "ASW_ShieldBug.Death" );
}


void CASW_Shieldbug::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
	
	if (( m_NPCState == NPC_STATE_COMBAT ) && ( random->RandomFloat( 0, 5 ) < 0.1 ))
	{
		IdleSound();
	}
}

bool CASW_Shieldbug::ShouldDefend()
{
	if ( asw_shieldbug_force_defend.GetInt() == 1 )
		return false;

	if ( asw_shieldbug_force_defend.GetInt() == 2 )
		return true;

	if (!GetEnemy())	// no enemy
		return false;

	float fEnemyDist = GetAbsOrigin().DistTo(GetEnemy()->GetAbsOrigin());
	if (fEnemyDist > 800.0f)		// enemy is too far away
		return false;

	// if enemy is reasonably far away and we haven't been hurt recently
	if (fEnemyDist > 200.0f && gpGlobals->curtime - m_fLastHurtTime > m_flDefendDuration )
		return false;

	// randomly leave defending to surprise people if we haven't done a headhit attack in a while
	//if (gpGlobals->curtime > m_fNextHeadhitAttack && random->RandomFloat() > 0.8f)
		//return false;

	// if we're on full health, don't defend
	if ( !m_bHasBeenHurt )
		return false;

	return true;
}


void CASW_Shieldbug::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_SHIELDBUG_MELEE_HIT1 )
	{
		MeleeAttack( ASW_SHIELDBUG_MELEE1_RANGE, ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_shieldbug_damage.GetFloat()), QAngle( 20.0f, 0.0f, -12.0f ), Vector( -250.0f, 1.0f, 1.0f ) );
		return;
	}

	if ( nEvent == AE_SHIELDBUG_SCREENSHAKE )
	{
		if (asw_shieldbug_screen_shake.GetBool())
		{
			// if we're running forwards, do a screen rumble
			float my = GetPoseParameter("move_yaw");
			float speed = GetAbsVelocity().Length2D();
			if (abs(my) < 30 && speed > 200)
				UTIL_ASW_ScreenShake( GetAbsOrigin(), 4.0, 1.0, 0.5, 1500, SHAKE_START, false );
		}
		return;
	}

	if ( nEvent == AE_SHIELDBUG_MELEE_HIT2 )
	{
		MeleeAttack( ASW_SHIELDBUG_MELEE1_RANGE, ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_shieldbug_damage.GetFloat()), QAngle( 20.0f, 0.0f, 0.0f ), Vector( -350.0f, 1.0f, 1.0f ) );
		return;
	}	
	
//	if ( nEvent == AE_SHIELDBUG_MELEE1_SOUND )
//	{
//		EmitSound( "ASW_ShieldBug.Attack" );
//		return;
//	}
	
//	if ( nEvent == AE_SHIELDBUG_MELEE2_SOUND )
//	{
//		EmitSound( "ASW_ShieldBug.Attack" );
//		return;
//	}

	if ( nEvent == AE_SHIELDBUG_START_DEFEND )
	{
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg( "Shieldbug started defending from anim event\n" );
		}
		m_bDefending = true;
		return;
	}
	else if ( nEvent == AE_SHIELDBUG_LEAVE_DEFEND )
	{
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg( "Shieldbug stopped defending from anim event\n" );
		}
		m_bDefending = false;
		return;
	}
	BaseClass::HandleAnimEvent( pEvent );
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CASW_Shieldbug::GetDeathActivity ( void )
{
	Activity	deathActivity;
	deathActivity = ( Activity ) ACT_DIE_FANCY;

	if ( m_nDeathStyle == kDIE_FANCY )
	{
		if ( m_bOnFire )
			deathActivity = ( Activity ) ACT_DEATH_FIRE;
		else if ( m_bElectroStunned )
			deathActivity = ( Activity ) ACT_DEATH_ELEC;	  
		else
			deathActivity = ( Activity ) ACT_DIE_FANCY;
	}

	BaseClass::GetDeathActivity();
	return deathActivity; 
}


void CASW_Shieldbug::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	ASWFailAdvice()->OnShiedbugKilled();
}

// NOTE: This function doesn't currently get used
bool CASW_Shieldbug::CorpseGib( const CTakeDamageInfo &info )
{
	CEffectData	data;

	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = Vector(0,0,1);
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );
	data.m_nColor = 1;

	if ( asw_debug_shieldbug.GetBool() )
	{
		Msg("Dispatching harvester gibs, colour %d\n", data.m_nColor);
	}
	DispatchEffect( "HarvesterGib", data );

	UTIL_ASW_BloodDrips( GetAbsOrigin(), Vector(0,0,1), BLOOD_COLOR_GREEN, 4 );

	//EmitSound( "ASW_ShieldBug.Death" );

	//CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, GetAbsOrigin(), 256, 0.5f, this );

	return true;
}

void CASW_Shieldbug::BuildScheduleTestBits( void )
{
	//Don't allow any modifications when scripted
	if ( m_NPCState == NPC_STATE_SCRIPT )
		return;

	//Make sure we interrupt a run schedule if we can jump
	if ( IsCurSchedule( SCHED_CHASE_ENEMY ) || IsCurSchedule( SCHED_SHOVER_CHASE_ENEMY ) )
	{
		SetCustomInterruptCondition( COND_ENEMY_UNREACHABLE );
		SetCustomInterruptCondition( COND_SHIELDBUG_CHANGE_DEFEND );
	}

	//Interrupt any schedule unless already fleeing, burrowing, burrowed, or unburrowing.
	if( GetFlags() & FL_ONGROUND )
	{
		if ( GetEnemy() == NULL )
		{
			SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
		}					
	}
}

void CASW_Shieldbug::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	// Do the base class
	BaseClass::GatherEnemyConditions( pEnemy );

	// If we're not already too far away, check again
	//TODO: Check to make sure we don't already have a condition set that removes the need for this
	if ( HasCondition( COND_ENEMY_UNREACHABLE ) == false )
	{
		Vector	predPosition;
		UTIL_PredictedPosition( GetEnemy(), 1.0f, &predPosition );

		Vector	predDir = ( predPosition - GetAbsOrigin() );
		float	predLength = VectorNormalize( predDir );

		// See if we'll be outside our effective target range
		if ( predLength > 2000 ) // m_flEludeDistance
		{
			Vector	predVelDir = ( predPosition - GetEnemy()->GetAbsOrigin() );
			float	predSpeed  = VectorNormalize( predVelDir );

			// See if the enemy is moving mostly away from us
			if ( ( predSpeed > 512.0f ) && ( DotProduct( predVelDir, predDir ) > 0.0f ) )
			{
				// Mark the enemy as eluded and burrow away
				ClearEnemyMemory();
				SetEnemy( NULL );
				SetIdealState( NPC_STATE_ALERT );
				SetCondition( COND_ENEMY_UNREACHABLE );
			}
		}
	}
}

void CASW_Shieldbug::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
		case TASK_MELEE_ATTACK1:
		{
			RemoveAllGestures();
			BaseClass::StartTask(pTask);
			break;
		}
		
		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}

void CASW_Shieldbug::RunTask( const Task_t *pTask )
{
	if (GetActivity() == ACT_RUN) //  || GetActivity() == ACT_MELEE_ATTACK2
	{
		if (!m_bDefending)
			m_flPlaybackRate = asw_shieldbug_speedboost.GetFloat();
		else
			m_flPlaybackRate = asw_shieldbug_defending_speedboost.GetFloat();
	}

	BaseClass::RunTask( pTask );
}

int CASW_Shieldbug::TranslateSchedule( int scheduleType )
{	
	if ( scheduleType == SCHED_MELEE_ATTACK1 )
	{
		//RemoveAllGestures();
		return SCHED_ASW_SHIELDBUG_MELEE_ATTACK1;
	}
	else if ( scheduleType == SCHED_MELEE_ATTACK2 )
	{
		// this var is used to leave attacking mode (so we'll do a charge if we haven't done one for a while)
		m_fNextHeadhitAttack = gpGlobals->curtime + random->RandomFloat(7.0f, 15.0f);
	
		return SCHED_ASW_SHIELDBUG_MELEE_ATTACK2;
	}
		
	return BaseClass::TranslateSchedule( scheduleType );
}

int CASW_Shieldbug::SelectSchedule()
{
	if ( HasCondition( COND_NEW_ENEMY ) && GetHealth() > 0 )
	{
		m_takedamage = DAMAGE_YES;
	}

	if ( HasCondition( COND_SHIELDBUG_CHANGE_DEFEND ) && GetHealth() > 0 )
	{
		if ( m_bLastShouldDefend )
		{
			if ( asw_debug_shieldbug.GetBool() )
			{
				Msg( "Doing SCHED_SHIELDBUG_START_DEFENDING\n" );
			}
			return SCHED_SHIELDBUG_START_DEFENDING;
		}
		else
		{
			if ( asw_debug_shieldbug.GetBool() )
			{
				Msg( "Doing SCHED_SHIELDBUG_LEAVE_DEFENDING\n" );
			}
			return SCHED_SHIELDBUG_LEAVE_DEFENDING;
		}
	}

	return BaseClass::SelectSchedule();
}

Activity CASW_Shieldbug::NPC_TranslateActivity( Activity baseAct )
{
	Activity translated = BaseClass::NPC_TranslateActivity( baseAct );

	if ( m_bDefending )
	{
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg( "Translating to defend activity\n" );
		}
		if ( translated == ACT_RUN ) return (Activity) ACT_RUN_DEFEND;
		if ( translated == ACT_IDLE ) return (Activity) ACT_IDLE_DEFEND;
		if ( translated == ACT_FLINCH_LEFTARM ) return (Activity) ACT_FLINCH_LEFTARM_DEFEND;
		if ( translated == ACT_FLINCH_RIGHTARM ) return (Activity) ACT_FLINCH_RIGHTARM_DEFEND;
		if ( translated == ACT_MELEE_ATTACK1 ) return (Activity) ACT_MELEE_ATTACK1_DEFEND;
		if ( translated == ACT_TURN_LEFT ) return (Activity) ACT_TURN_LEFT_DEFEND;
		if ( translated == ACT_TURN_RIGHT ) return (Activity) ACT_TURN_RIGHT_DEFEND;
		if ( translated == ACT_ALIEN_SHOVER_ROAR ) return (Activity) ACT_ALIEN_SHOVER_ROAR_DEFEND;
	}

	return translated;
}

int CASW_Shieldbug::MeleeAttack1Conditions( float flDot, float flDist )
{
#if 1 //NOTENOTE: Use predicted position melee attacks

	float		flPrDist, flPrDot;
	Vector		vecPrPos;
	Vector2D	vec2DPrDir;

	//Get our likely position in one half second
	UTIL_PredictedPosition( GetEnemy(), 0.5f, &vecPrPos );

	//Get the predicted distance and direction
	flPrDist	= ( vecPrPos - GetAbsOrigin() ).Length();
	vec2DPrDir	= ( vecPrPos - GetAbsOrigin() ).AsVector2D();

	Vector vecBodyDir = BodyDirection2D();

	Vector2D vec2DBodyDir = vecBodyDir.AsVector2D();
	
	flPrDot	= DotProduct2D ( vec2DPrDir, vec2DBodyDir );

	if ( flPrDist > ASW_SHIELDBUG_MELEE1_START_ATTACK_RANGE )
		return COND_TOO_FAR_TO_ATTACK;

	//if ( flPrDot < 0.5f )
	if ( flPrDot < 0 )	// try generous way
		return COND_NOT_FACING_ATTACK;

#else

	if ( flDot < 0.5f )
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = ASW_SHIELDBUG_MELEE1_START_ATTACK_RANGE;

	if ( GetEnemy() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );

		if ( pPlayer && pPlayer->IsInAVehicle() )
		{
			flAdjustedDist *= 2.0f;
		}
	}

	if ( flDist > flAdjustedDist )
		return COND_TOO_FAR_TO_ATTACK;

	trace_t	tr;
	AI_TraceHull( WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8,8,8), Vector(8,8,8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction < 1.0f )
		return 0;

#endif

	return COND_CAN_MELEE_ATTACK1;
}

ConVar asw_shieldbug_knockdown( "asw_shieldbug_knockdown", "1", FCVAR_CHEAT, "If set shieldbug will knock marines down with his melee attacks" );
ConVar asw_shieldbug_knockdown_force( "asw_shieldbug_knockdown_force", "500", FCVAR_CHEAT, "Magnitude of knockdown force for shieldbug's melee attack" );
ConVar asw_shieldbug_knockdown_lift( "asw_shieldbug_knockdown_lift", "300", FCVAR_CHEAT, "Upwards force for shieldbug's melee attack" );

void CASW_Shieldbug::MeleeAttack( float distance, float damage, QAngle &viewPunch, Vector &shove )
{
	Vector vecForceDir;

	// Always hurt bullseyes for now
	if ( ( GetEnemy() != NULL ) && ( GetEnemy()->Classify() == CLASS_BULLSEYE ) )
	{
		vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		CTakeDamageInfo info( this, this, damage, DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForceDir, GetEnemy()->GetAbsOrigin() );
		GetEnemy()->TakeDamage( info );
		return;
	}

	CBaseEntity *pHurt = CheckTraceHullAttack( distance, -Vector(16,16,32), Vector(16,16,32), damage, DMG_SLASH, asw_shieldbug_melee_force.GetFloat() );

	if ( pHurt && asw_shieldbug_knockdown.GetBool() )
	{
		CASW_Marine *pMarine = CASW_Marine::AsMarine( pHurt );
		if ( pMarine )
		{
			vecForceDir = ( pHurt->WorldSpaceCenter() - WorldSpaceCenter() );
			vecForceDir.NormalizeInPlace();
			vecForceDir *= asw_shieldbug_knockdown_force.GetFloat();
			vecForceDir += Vector( 0, 0, asw_shieldbug_knockdown_lift.GetFloat() );
			pMarine->Knockdown( this, vecForceDir  );
		}
	}
}


void CASW_Shieldbug::CheckForShieldbugHint( const CTakeDamageInfo &info )
{
	if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
	{
		// reduce our block counter by the number of seconds that have passed since a marine last shot us in the front
		if (m_fLastMarineBlockTime != 0 && m_fMarineBlockCounter > 0)
		{
			if (asw_debug_marine_chatter.GetBool())
				Msg("Reducing block counter by %f\n", (gpGlobals->curtime - m_fLastMarineBlockTime) * 20.0f);
			m_fMarineBlockCounter -= (gpGlobals->curtime - m_fLastMarineBlockTime) * 20.0f;	// counter ticks completely down after 30 seconds
			if (m_fMarineBlockCounter < 0)
				m_fMarineBlockCounter = 0;
		}
		m_fMarineBlockCounter += info.GetDamage();
		if (asw_debug_marine_chatter.GetBool())
			Msg("m_fMarineBlockCounter = %f\n", m_fMarineBlockCounter);
		if (m_fMarineBlockCounter > 600)		// 686 = burning a whole rifle clip no normal difficulty
		{
			if (asw_debug_marine_chatter.GetBool())
				Msg("Doing shieldbug hint\n");
			// try to find a nearby marine to shout out a shieldbug hint
			if ( ASWGameResource() )
			{
				CASW_Game_Resource *pGameResource = ASWGameResource();
				// count how many are nearby
				int iNearby = 0;						
				for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
				{
					CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
					CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
					if (pMarine && (GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
								&& pMarine->GetHealth() > 0)
								iNearby++;
				}
				int iChatter = random->RandomInt(0, iNearby-1);
				for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
				{
					CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
					CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
					if (pMarine && (GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
								&& pMarine->GetHealth() > 0)
					{
						if (iChatter <= 0)
						{
							pMarine->GetMarineSpeech()->QueueChatter(CHATTER_SHIELDBUG_HINT, gpGlobals->curtime + 1.0f, gpGlobals->curtime + 3.0f);
							break;
						}
						iChatter--;
					}
				}						
			}
			m_fMarineBlockCounter = -1200;	// set the block counter way below so we don't shout about shooting from behind again so soon
		}
		m_fLastMarineBlockTime = gpGlobals->curtime;
	}
}

int CASW_Shieldbug::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	m_fLastHurtTime = gpGlobals->curtime;

	int result = 0;

	CTakeDamageInfo newInfo(info);
	float damage = info.GetDamage();

	// reduce damage from shotguns and mining laser
	if (info.GetDamageType() & DMG_ENERGYBEAM)
	{
		damage *= 0.4f;
	}
	if (info.GetDamageType() & DMG_BUCKSHOT)
	{
		// hack to reduce vindicator damage (not reducing normal shotty as much as it's not too strong)
		if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
		{
			CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
			if (pMarine)
			{
				CASW_Weapon_Assault_Shotgun *pVindicator = dynamic_cast<CASW_Weapon_Assault_Shotgun*>(pMarine->GetActiveASWWeapon());
				if ( pVindicator )
					damage *= 0.45f;
				else
					damage *= 0.6f;
			}
		}		
	}

	newInfo.SetDamage( damage );

	// Get facing direction
	Vector vecFacing;
	QAngle angFacing = GetAbsAngles();
	AngleVectors( angFacing, &vecFacing );
	vecFacing.z = 0;
	VectorNormalize( vecFacing );

	// Get the dot of the attack
	float fForwardDot = -1.0f;

	Vector damageNormal;
	Vector vecDamagePos = newInfo.GetDamagePosition();
	if ( newInfo.GetAttacker() && vecDamagePos != vec3_origin )
	{
		vecDamagePos = newInfo.GetAttacker()->GetAbsOrigin();	// use the attacker's position when determining block, to stop marines shooting through gaps and hurting the bug from any angle
	}

	bool bDirectional = ( vecDamagePos != vec3_origin );

	if ( bDirectional )
	{
		damageNormal = vecDamagePos - GetAbsOrigin();	// should be head pos
		damageNormal.z = 0;	

		fForwardDot = vecFacing.Dot( damageNormal );
	}
	
	if ( m_bDefending && bDirectional && fForwardDot > 0.66f )
	{
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg("Defending, check damage for block\n");
		}

		// is the attack in front
 		if ( ( newInfo.GetDamageType() & DMG_BURN ) != 0 &&		// extra dot block check is only for flames
			 ( newInfo.GetDamageType() & DMG_DIRECT ) == 0 )	// burning DoT doesn't get blocked
 		{
 			m_fLastHurtTime = gpGlobals->curtime;
 			CheckForShieldbugHint(newInfo);

 			return 0;
 		}
	}

	result = BaseClass::OnTakeDamage_Alive(newInfo);
	
	if ( result > 0 )
	{
		m_bHasBeenHurt = true;

		if ( gpGlobals->curtime > m_fNextFlinchTime )
		{
			// was the attacker on the left or the right?
			bool bLeft = true;			
			if ( bDirectional && m_hCine == NULL )		// non-locational damage doesn't cause flinch + don't flinch if scripting
			{
				// never flinch if we're being shot from the front
				if ( fForwardDot < 0.66f )
				{
					angFacing = GetAbsAngles();
					angFacing[YAW] += 90;
					AngleVectors(angFacing, &vecFacing);
					vecFacing.z = 0;
					VectorNormalize(vecFacing);
					bLeft = vecFacing.Dot(damageNormal) > 0;

					if (bLeft)
						SetSchedule(SCHED_SHIELDBUG_FLINCH_LEFT);
					else
						SetSchedule(SCHED_SHIELDBUG_FLINCH_RIGHT);

					if (asw_debug_shieldbug.GetBool())
						Msg("Flinching! %f forwarddot=%f\n", gpGlobals->curtime, fForwardDot);

					m_fNextFlinchTime = gpGlobals->curtime + ASW_SHIELDBUG_FLINCH_INTERVAL;
				}
			}			
		}		
	}

	return result;
}

bool CASW_Shieldbug::ShouldGib( const CTakeDamageInfo &info )
{
	// don't gib if we burnt to death
	if (info.GetDamageType() & DMG_BURN)
		return false;

	return false;
}

bool CASW_Shieldbug::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
  	Vector		vecFacePosition = vec3_origin;
	CBaseEntity	*pFaceTarget = NULL;
	bool		bFaceTarget = false;

	if ( GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN )
  	{
		Vector vecEnemyLKP = GetEnemyLKP();
		
		// Only start facing when we're close enough
		if ( ( UTIL_DistApprox( vecEnemyLKP, GetAbsOrigin() ) < 512 ) )
		{
			vecFacePosition = vecEnemyLKP;
			pFaceTarget = GetEnemy();
			bFaceTarget = true;
		}
	}

	// Face
	if ( bFaceTarget )
	{
		AddFacingTarget( pFaceTarget, vecFacePosition, 1.0, 0.2 );
	}

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

void CASW_Shieldbug::SetTurnActivity ( void )
{	
	if (asw_debug_shieldbug.GetBool())
		Msg("Shieldbug set turn activity\n");

	//RemoveAllGestures();
	BaseClass::SetTurnActivity();	

	return;
}

// don't use the HL2 method of starting flinches, we only want them to occur when the player is shooting the 'weak spots'
bool CASW_Shieldbug::CanFlinch( void )
{
	return false;
}

void CASW_Shieldbug::SetHealthByDifficultyLevel()
{		
	SetHealth( ASWGameRules()->ModifyAlienHealthBySkillLevel( 1000 ) + m_nExtraHeath ); // was 500 - 2/19/10		
}

void CASW_Shieldbug::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon /*= NULL */ )
{
	BaseClass::ASW_Ignite(MIN(flFlameLifetime, 3.0f), flSize, pAttacker, pDamagingWeapon );
}

void CASW_Shieldbug::NPCThink()
{
	BaseClass::NPCThink();
	if ( GetEfficiency() < AIE_DORMANT && GetSleepState() == AISS_AWAKE 
		&& gpGlobals->curtime > s_fNextSpottedChatterTime && GetEnemy())
	{
		CASW_Marine *pMarine = UTIL_ASW_Marine_Can_Chatter_Spot(this);
		if (pMarine)
		{
			pMarine->GetMarineSpeech()->Chatter(CHATTER_SHIELDBUG);
			s_fNextSpottedChatterTime = gpGlobals->curtime + 30.0f;
		}
		else
			s_fNextSpottedChatterTime = gpGlobals->curtime + 1.0f;
	}		
}

// is the enemy near enough to left slash at?
int CASW_Shieldbug::MeleeAttack2Conditions ( float flDot, float flDist )
{
	if ( flDist > asw_sb_gallop_max_range.GetFloat())
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDist < asw_sb_gallop_min_range.GetFloat())
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}

	if (m_bDefending)
		return false;
	
	return COND_CAN_MELEE_ATTACK2;
}

int CASW_Shieldbug::GetMoneyCount( const CTakeDamageInfo &info )
{
	return RandomInt( 3, 5 );
}

void CASW_Shieldbug::GatherConditions()
{
	BaseClass::GatherConditions();

	m_bLastShouldDefend = ShouldDefend();
	if ( m_bDefending != m_bLastShouldDefend )
	{
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg( "Setting COND_SHIELDBUG_CHANGE_DEFEND\n" );
		}
		SetCondition( COND_SHIELDBUG_CHANGE_DEFEND );
	}
	else
	{
		ClearCondition( COND_SHIELDBUG_CHANGE_DEFEND );
	}
}

int	CASW_Shieldbug::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "Defending = %d", m_bDefending ),0);
		text_offset++;
		NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "ShouldDefend = %d", ShouldDefend() ),0);
		text_offset++;
		NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "COND_SHIELDBUG_CHANGE_DEFEND = %d", HasCondition( COND_SHIELDBUG_CHANGE_DEFEND ) ),0);
		text_offset++;
	}
	return text_offset;
}

AI_BEGIN_CUSTOM_NPC( asw_shieldbug, CASW_Shieldbug )
	// Activities
	DECLARE_ACTIVITY( ACT_SHIELDBUG_PRE_LEAP )
	DECLARE_ACTIVITY( ACT_START_DEFENDING );
	DECLARE_ACTIVITY( ACT_LEAVE_DEFENDING );
	DECLARE_ACTIVITY( ACT_RUN_DEFEND );
	DECLARE_ACTIVITY( ACT_IDLE_DEFEND );
	DECLARE_ACTIVITY( ACT_FLINCH_LEFTARM_DEFEND );;
	DECLARE_ACTIVITY( ACT_FLINCH_RIGHTARM_DEFEND );
	DECLARE_ACTIVITY( ACT_MELEE_ATTACK1_DEFEND );
	DECLARE_ACTIVITY( ACT_TURN_LEFT_DEFEND );
	DECLARE_ACTIVITY( ACT_TURN_RIGHT_DEFEND );
	DECLARE_ACTIVITY( ACT_ALIEN_SHOVER_ROAR_DEFEND );

	// Events
	DECLARE_ANIMEVENT( AE_SHIELDBUG_WALK_FOOTSTEP )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_FOOTSTEP_SOFT )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_FOOTSTEP_HEAVY )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_MELEE_HIT1 )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_MELEE_HIT2 )	
	DECLARE_ANIMEVENT( AE_SHIELDBUG_MELEE1_SOUND )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_MELEE2_SOUND )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_MOUTH_BLEED )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_ALERT_SOUND )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_SCREENSHAKE )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_START_DEFEND )
	DECLARE_ANIMEVENT( AE_SHIELDBUG_LEAVE_DEFEND )

	DECLARE_CONDITION( COND_SHIELDBUG_CHANGE_DEFEND )

	DEFINE_SCHEDULE
	(
		SCHED_SHIELDBUG_FLINCH_LEFT,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_FLINCH_LEFTARM"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_SHIELDBUG_FLINCH_RIGHT,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_FLINCH_RIGHTARM"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_SHIELDBUG_START_DEFENDING,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_START_DEFENDING"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_SHIELDBUG_LEAVE_DEFENDING,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_LEAVE_DEFENDING"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_SHIELDBUG_MELEE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"	// asw comment test
	//	"		TASK_STOP_MOVING		1"	// asw 1 = just clear goal
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_SHIELDBUG_MELEE_ATTACK2,

		"	Tasks"
		"		TASK_STOP_MOVING		0"	// asw comment test
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		//"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_SHIELDBUG_PRE_LEAP"
		"		TASK_MELEE_ATTACK2		0"
		""
		"	Interrupts"
	)
AI_END_CUSTOM_NPC()


