// Our Swarm Drone - the basic angry fighting alien

#include "cbase.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_moveprobe.h"
#include "ai_route.h"
#include "npcevent.h"
#include "gib.h"
#include "entitylist.h"
#include "ndebugoverlay.h"
#include "antlion_dust.h"
#include "engine/IEngineSound.h"
#include "globalstate.h"
#include "movevars_shared.h"
#include "te_effect_dispatch.h"
#include "vehicle_base.h"
#include "mapentities.h"
#include "antlion_maker.h"
#include "npc_antlion.h"
#include "decals.h"
#include "asw_drone_advanced.h"
#include "asw_player.h"
#include "asw_fx_shared.h"
#include "asw_door.h"
#include "asw_door_padding.h"
#include "asw_drone_navigator.h"
#include "asw_drone_movement.h"
#include "asw_util_shared.h"
#include "asw_gamerules.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "asw_marine_speech.h"
#include "ammodef.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_drone_melee_range("asw_drone_melee_range", "60.0", FCVAR_CHEAT, "Range of the drone's melee attack");
ConVar asw_drone_start_melee_range("asw_drone_start_melee_range", "100.0", FCVAR_CHEAT, "Range at which the drone starts his melee attack");

#define ASW_DRONE_MELEE1_START_ATTACK_RANGE asw_drone_start_melee_range.GetFloat()
#define ASW_DRONE_MELEE1_RANGE asw_drone_melee_range.GetFloat()

extern ConVar ai_sequence_debug;

//todo: 10 seems quite low, given the time delay between swings
//  should do this damage when bumping (maybe a bit less?) and extra damage on each of the 3 swipes of the anim
ConVar sk_asw_drone_damage( "sk_asw_drone_damage", "15.0", FCVAR_CHEAT, "Damage per swipe from the Drone");
ConVar asw_drone_speedboost( "asw_drone_speedboost", "1.0",FCVAR_CHEAT , "boost speed for the alien drones" );
// 0.32 is speedboost equal to movement when movement speedboost is 1.0
ConVar asw_drone_override_speedboost( "asw_drone_override_speedboost", "0.32",FCVAR_CHEAT , "boost speed for the alien drones when in override mode" );
// note: UNUSED - drone doesn't do automovement during melee, he does override move
ConVar asw_drone_auto_speed_scale("asw_drone_auto_speed_scale", "0.5", FCVAR_CHEAT, "Speed scale for the drones while melee attacking");
ConVar asw_drone_health("asw_drone_health", "40", FCVAR_CHEAT, "How much health the Swarm drones have");

// these settings make the drones quite undodgeable and more lethal
//ConVar asw_drone_yaw_speed("asw_drone_yaw_speed", "32.0", FCVAR_CHEAT, "How fast the swarm drone can turn");
//ConVar asw_drone_yaw_speed_attacking("asw_drone_yaw_speed_attacking", "16.0", FCVAR_CHEAT, "How fast the swarm drone can turn while doing a melee attack");
// these settings mean you can dodge aside when a drone starts attacking - makes for better gameplay?
ConVar asw_drone_yaw_speed("asw_drone_yaw_speed", "32.0", FCVAR_CHEAT, "How fast the swarm drone can turn");
ConVar asw_drone_yaw_speed_attackprep("asw_drone_yaw_speed_attackprep", "64.0", FCVAR_CHEAT, "How fast the swarm drone can turn while starting his melee attack");
ConVar asw_drone_yaw_speed_attacking("asw_drone_yaw_speed_attacking", "8.0", FCVAR_CHEAT, "How fast the swarm drone can turn while doing a melee attack");
ConVar asw_drone_acceleration("asw_drone_acceleration", "5", FCVAR_CHEAT, "How fast the swarm drone accelerates, as a multiplier on his ideal speed");
ConVar asw_drone_smooth_speed("asw_drone_smooth_speed", "200", FCVAR_CHEAT, "How fast the swarm drone smooths his current velocity into the ideal, when using overidden movement");
ConVar asw_drone_override_move("asw_drone_override_move", "0", FCVAR_CHEAT, "Enable to make Swarm drones use custom override movement to chase their enemy");
ConVar asw_drone_override_attack("asw_drone_override_attack", "1", FCVAR_CHEAT, "Enable to make Swarm drones use custom override movement to attack their enemy");
ConVar asw_drone_friction("asw_drone_friction", "0.1", FCVAR_CHEAT, "Velocity loss due to friction");
ConVar asw_drone_show_facing("asw_drone_show_facing", "0", FCVAR_CHEAT, "Show which way the drone is facing");
ConVar asw_drone_gib_chance("asw_drone_gib_chance", "0.075", FCVAR_CHEAT, "Chance of drone gibbing instead of ragdoll");
ConVar asw_drone_show_override("asw_drone_show_override", "0", FCVAR_CHEAT, "Show a yellow arrow if drone is using override movement");
ConVar asw_drone_attacks("asw_drone_attacks", "1", FCVAR_CHEAT, "Whether the drones attack or not");
ConVar asw_debug_drone("asw_debug_drone", "0", FCVAR_CHEAT, "Enable drone debug messages");
ConVar asw_drone_door_distance("asw_drone_door_distance", "60", FCVAR_CHEAT, "How near to the door a drone needs to be to bash it");
ConVar asw_drone_door_distance_min("asw_drone_door_distance_min", "40", FCVAR_CHEAT, "Nearest a drone can be to a door when attacking");
ConVar asw_marine_view_cone_cost("asw_marine_view_cone_cost", "5", FCVAR_CHEAT, "Extra pathing cost if a node is visible to a marine");
ConVar asw_drone_zig_zag_length("asw_drone_zig_zag_length", "144", FCVAR_CHEAT, "Length of drone zig zagging");
ConVar asw_drone_zig_zagging("asw_drone_zig_zagging", "0", FCVAR_CHEAT, "If set, aliens will try to zig zag up to their enemy instead of approaching directly");
ConVar asw_drone_melee_force("asw_drone_melee_force", "1.67", FCVAR_CHEAT, "Force of the drone's melee attack");
ConVar asw_drone_touch_damage( "asw_drone_touch_damage", "0",FCVAR_CHEAT , "Damage caused by drones on touch" );
ConVar asw_new_drone("asw_new_drone", "1", FCVAR_CHEAT, "Set to 1 to use the new drone model");
extern ConVar asw_debug_alien_damage;
extern ConVar asw_alien_hurt_speed;
extern ConVar asw_alien_stunned_speed;
extern ConVar asw_springcol;
extern ConVar asw_drone_death_force_pitch;

float CASW_Drone_Advanced::s_fNextTooCloseChatterTime = 0;

// drone anim events
int AE_DRONE_WALK_FOOTSTEP;
int AE_DRONE_FOOTSTEP_SOFT;
int AE_DRONE_FOOTSTEP_HEAVY;
int AE_DRONE_MELEE_HIT1;
int AE_DRONE_MELEE_HIT2;
int AE_DRONE_MELEE1_SOUND;
int AE_DRONE_MELEE2_SOUND;
int AE_DRONE_MOUTH_BLEED;
int AE_DRONE_ALERT_SOUND;
int AE_DRONE_SHADOW_ON;

int ACT_DRONE_RUN_ATTACKING;
int ACT_DRONE_WALLPOUND;

enum
{
	SCHED_DRONE_BASH_DOOR = LAST_ASW_ALIEN_JUMPER_SHARED_SCHEDULE,
	SCHED_DRONE_CHASE_ENEMY,
	SCHED_DRONE_OVERRIDE_MOVE,
	SCHED_DRONE_DOOR_WAIT,
	SCHED_DRONE_BASH_CLOSE_DOOR,
	SCHED_DRONE_CIRCLE_ENEMY,
	SCHED_DRONE_CIRCLE_ENEMY_FAILED,
	SCHED_DRONE_STANDOFF,
	LAST_ASW_DRONE_SHARED_SCHEDULE,
};

CUtlVector<CASW_Drone_Advanced*> g_DroneList;
static CASW_Drone_Movement g_DroneGameMovement;
CASW_Drone_Movement *g_pDroneMovement = &g_DroneGameMovement;

CASW_Drone_Advanced::CASW_Drone_Advanced( void )
	: m_DurationDoorBash( 2)
	   // : CASW_Alien()
{
	g_DroneList.AddToTail(this);
	if ( asw_new_drone.GetBool() )
	{
	  m_pszAlienModelName = SWARM_NEW_DRONE_MODEL;
	}
	else
	{
	   m_pszAlienModelName = SWARM_DRONE_MODEL;
	}
	m_flNextSmallFlinchTime = 0.0f;
	m_nAlienCollisionGroup = ASW_COLLISION_GROUP_ALIEN;
	m_iDeadBodyGroup = 2;
	//m_debugOverlays |= (OVERLAY_TEXT_BIT | OVERLAY_BBOX_BIT); 
}

CASW_Drone_Advanced::~CASW_Drone_Advanced()
{
	g_DroneList.FindAndRemove(this);	
}

LINK_ENTITY_TO_CLASS( asw_drone, CASW_Drone_Advanced );
LINK_ENTITY_TO_CLASS( asw_drone_jumper, CASW_Drone_Advanced );

BEGIN_DATADESC( CASW_Drone_Advanced )
	DEFINE_FIELD( m_hBlockingDoor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDoorBashYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecSavedVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_flSavedSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_fLastLostLOSTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecLastGoodPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_bPerformingOverride, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bJumper, FIELD_BOOLEAN ),
	DEFINE_EMBEDDED( m_DurationDoorBash ),
	DEFINE_FIELD( m_bFailedOverrideMove, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fFailedOverrideTime, FIELD_TIME ),
	DEFINE_FIELD( m_bUsingSmallDoorBashHull, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iDoorPos, FIELD_INTEGER ),
	DEFINE_FIELD( m_bLastSoftLOS, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fLastTouchHurtTime, FIELD_TIME ),
	DEFINE_FIELD( m_bDoneAlienCloseChatter, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecEnemyStandoffPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_bHasAttacked, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_Drone_Advanced, DT_ASW_Drone_Advanced )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropEHandle( SENDINFO( m_hAimTarget ) ),
END_SEND_TABLE()


CAI_Navigator *CASW_Drone_Advanced::CreateNavigator()
{
	//return BaseClass::CreateNavigator();
	return new CASW_Drone_Navigator( this );
}


void CASW_Drone_Advanced::Spawn( void )
{
	BaseClass::Spawn();

	//m_debugOverlays |= OVERLAY_NPC_ROUTE_BIT | OVERLAY_BBOX_BIT | OVERLAY_PIVOT_BIT | OVERLAY_TASK_TEXT_BIT | OVERLAY_TEXT_BIT;

	m_FlinchActivity = ACT_INVALID;
	m_nDeathStyle = RandomFloat() < asw_drone_gib_chance.GetFloat() ? kDIE_INSTAGIB : kDIE_RAGDOLLFADE;

	// randomize the parts on the drone
	if ( asw_new_drone.GetBool() )
	{
		//claws
		SetBodygroup ( 1, RandomInt (0, 2 ) );
		SetBodygroup ( 2, RandomInt (0, 2 ) );
		SetBodygroup ( 3, RandomInt (0, 2 ) );
		SetBodygroup ( 4, RandomInt (0, 2 ) );
		// body
		if ( RandomFloat() < .5 )
		{
		  SetBodygroup ( 0, RandomInt (0, 1 ) );
		}
		// bones
		if ( RandomFloat() < .25)
		{
			SetBodygroup ( 5, RandomInt (0, 1 ) );
		}
	}
	if (FClassnameIs(this, "asw_drone_jumper"))
	{
		m_bJumper = true;
	}
	else
	{
		m_bJumper = false;
		m_bDisableJump = true;
		CapabilitiesRemove( bits_CAP_MOVE_JUMP );
	}	

	SetHullType(HULL_MEDIUMBIG);

	//SetCollisionBounds(Vector(-26,-26,0), Vector(26,26,72));
	UTIL_SetSize(this, Vector(-17,-17,0), Vector(17,17,69));

	//UseClientSideAnimation();	
		
	SetHealthByDifficultyLevel();	
	
	CapabilitiesAdd( bits_CAP_MOVE_CLIMB | bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 );	// removed: bits_CAP_MOVE_JUMP
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );

	m_fFailedOverrideTime = -100.0f;
	m_iDoorPos = 0;
	m_bUsingSmallDoorBashHull = false;
	SetPoseParameter( "idle_move", 0 );
}

void CASW_Drone_Advanced::Precache( void )
{
	PrecacheScriptSound( "ASW_Drone.Land" );
	PrecacheScriptSound( "ASW_Drone.Pain" );
	PrecacheScriptSound( "ASW_Drone.Alert" );
	PrecacheScriptSound( "ASW_Drone.Death" );
	PrecacheScriptSound( "ASW_Drone.Attack" );
	PrecacheScriptSound( "ASW_Drone.Swipe" );

	PrecacheScriptSound( "ASW_Drone.GibSplatHeavy" );
	PrecacheScriptSound( "ASW_Drone.GibSplat" );
	PrecacheScriptSound( "ASW_Drone.GibSplatQuiet" );
	PrecacheScriptSound( "ASW_Drone.DeathFireSizzle" );

	PrecacheModel( "models/aliens/drone/ragdoll_tail.mdl" );
   	PrecacheModel( "models/aliens/drone/ragdoll_uparm.mdl" );
	PrecacheModel( "models/aliens/drone/ragdoll_uparm_r.mdl" );
	PrecacheModel( "models/aliens/drone/ragdoll_leg_r.mdl" );
	PrecacheModel( "models/aliens/drone/ragdoll_leg.mdl" );
	PrecacheModel( "models/aliens/drone/gib_torso.mdl" );
	
	BaseClass::Precache();
}

void CASW_Drone_Advanced::AlertSound()
{
	EmitSound("ASW_Drone.Alert");
}

void CASW_Drone_Advanced::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound)
	{
		EmitSound("ASW_Drone.Pain");
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
	}
}

void CASW_Drone_Advanced::DeathSound( const CTakeDamageInfo &info )
{
	// if we are playing a fancy death animation, don't play death sounds from code
	// all death sounds are played from anim events inside the fancy death animation
	if ( m_nDeathStyle == kDIE_FANCY )
		return;

	EmitSound( "ASW_Drone.Death" );

	if ( m_nDeathStyle == kDIE_INSTAGIB )
		EmitSound( "ASW_Drone.GibSplatHeavy" );
	else if ( m_nDeathStyle == kDIE_TUMBLEGIB || m_nDeathStyle == kDIE_RAGDOLLFADE )
		EmitSound( "ASW_Drone.GibSplatQuiet" );
	else
	{
		if ( m_bOnFire )
			EmitSound( "ASW_Drone.DeathFireSizzle" );
		else
			EmitSound( "ASW_Drone.GibSplat" );
	}
}

float CASW_Drone_Advanced::GetIdealSpeed() const
{
	float boost = asw_drone_speedboost.GetFloat();
	if (IsPerformingOverrideMove())
	{
		boost = asw_drone_override_speedboost.GetFloat();
	}

	switch (ASWGameRules()->GetSkillLevel())
	{
		case 5: boost *= asw_alien_speed_scale_insane.GetFloat(); break;
		case 4: boost *= asw_alien_speed_scale_insane.GetFloat(); break;
		case 3: boost *= asw_alien_speed_scale_hard.GetFloat(); break;
		case 2: boost *= asw_alien_speed_scale_normal.GetFloat(); break;
		default: boost *= asw_alien_speed_scale_easy.GetFloat(); break;
	}

	float flFreezeSpeedScale = 1.0f - m_flFrozen;
	flFreezeSpeedScale = clamp<float>( flFreezeSpeedScale, 0.0f, 1.0f );

	return boost * BaseClass::GetIdealSpeed() * m_flPlaybackRate * flFreezeSpeedScale;
}

float CASW_Drone_Advanced::GetIdealAccel( ) const
{
	// our drone goes FASTER
	return GetIdealSpeed() * asw_drone_acceleration.GetFloat();
}

float CASW_Drone_Advanced::MaxYawSpeed( void )
{
	float fSlowScale = ShouldMoveSlow() ? 0.5f : 1.0f;	
	if (GetActivity() == ACT_MELEE_ATTACK1)	// slow turning when actually swiping
		return asw_drone_yaw_speed_attacking.GetFloat() * fSlowScale;

	if ( IsCurSchedule( SCHED_ASW_ALIEN_MELEE_ATTACK1 ) || IsCurSchedule( SCHED_ASW_ALIEN_SLOW_MELEE_ATTACK1 ) )	// faster turning while trying to face our enemy
		return asw_drone_yaw_speed_attackprep.GetFloat() * fSlowScale;

	//fSlowScale *= ( 1.0f - m_flFrozen );

	return asw_drone_yaw_speed.GetFloat() * fSlowScale;
}

// allow the NPC to modify automovement
bool CASW_Drone_Advanced::ModifyAutoMovement( Vector &vecNewPos )
{
	// melee auto movement on the drones seems way too fast
	float fFactor = asw_drone_auto_speed_scale.GetFloat();
	if ( ShouldMoveSlow() )
	{
		if ( m_bElectroStunned.Get() )
		{
			fFactor *= asw_alien_stunned_speed.GetFloat() * 0.1f;
		}
		else
		{
			fFactor *= asw_alien_hurt_speed.GetFloat() * 0.1f;
		}
	}
	Vector vecRelPos = vecNewPos - GetAbsOrigin();
	vecRelPos *= fFactor;
	vecNewPos = GetAbsOrigin() + vecRelPos;
	return true;
}

bool CASW_Drone_Advanced::IsNavigationUrgent()
{
	return BaseClass::IsNavigationUrgent();
}

float CASW_Drone_Advanced::GetSequenceGroundSpeed( int iSequence )
{	
	return BaseClass::GetSequenceGroundSpeed(iSequence);
}

bool CASW_Drone_Advanced::OverrideMove( float flInterval )
{
	if ( IsMovementFrozen() )
	{
		// override to keep drone still
		SetAbsVelocity( vec3_origin );
		GetMotor()->SetMoveVel( vec3_origin );
		m_vecSavedVelocity = vec3_origin;
		m_flSavedSpeed = 0.0f;
		return true;
	}

	if ( BaseClass::OverrideMove( flInterval ) )
	{
		return true;
	}

	if ( m_lifeState == LIFE_ALIVE && ( m_NPCState != NPC_STATE_SCRIPT ) )
	{		
		if (asw_drone_override_move.GetBool() && IsPerformingOverrideMove() && !FailedOverrideMove())
		{
			// try to move us, set the failed flag if we don't move far enough, so the more reliable pathed movement can take over
			m_bFailedOverrideMove = !MoveExecute_Alive( flInterval );
			if (m_bFailedOverrideMove)
			{
				m_fFailedOverrideTime = gpGlobals->curtime;
				TaskFail(FAIL_NO_ROUTE_BLOCKED);
			}
			return true;
		}		
		else if (asw_drone_override_attack.GetBool() && IsMeleeAttacking())
		{			
			MoveExecute_Alive( flInterval );
			return true;
		}
	}
	m_vecSavedVelocity = GetAbsVelocity();
	m_flSavedSpeed = m_vecSavedVelocity.Length2D();
	//SetPoseParameter( "idle_move", 0 );
	m_bFailedOverrideMove = false;

	return false;
}

bool CASW_Drone_Advanced::IsPerformingOverrideMove() const
{
	return m_bPerformingOverride;	
}

#define ASW_FAILED_OVERRIDE_MOVE_TIME 5.0f
bool CASW_Drone_Advanced::FailedOverrideMove() const 
{
	if ( ( gpGlobals->curtime - m_fFailedOverrideTime ) < ASW_FAILED_OVERRIDE_MOVE_TIME )
		return true;
	return false;
}

bool CASW_Drone_Advanced::IsMoving( void ) 
{ 
	return BaseClass::IsMoving() || IsPerformingOverrideMove();
}

#define ASW_FAILED_MOVE_FRACTION 0.1f
bool CASW_Drone_Advanced::MoveExecute_Alive(float flInterval)
{
	if (!GetEnemy())
		return false;		// if we don't have a target, we shouldn't be doing override move!
	// remove any blended layers from pathed movement
	RemoveAllGestures();

	// save our falling velocity, then remove it from our calcs
	float fZMovement = m_vecSavedVelocity.z;
	m_vecSavedVelocity.z = 0;

	// turn us to face our enemy
	Vector dir = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize(dir);
	float flTargetYaw = UTIL_VecToYaw(dir);
	if (GetGroundEntity() && GetGroundEntity()->Classify() == CLASS_ASW_MARINE)	// if we're standing on a marine's head, just move forward
		flTargetYaw = GetAbsAngles().y;
	GetMotor()->SetIdealYawAndUpdate(flTargetYaw);
	
	// rotate our movement vector a bit too, to stop the slideyness
	float fMovementYaw = VecToYaw(m_vecSavedVelocity);
	float fNewYaw = ASW_ClampYaw(MaxYawSpeed() * 5, fMovementYaw, flTargetYaw, flInterval);
	//Msg("current = %f target = %f new = %f\n", fMovementYaw, flTargetYaw, fNewYaw);
	//NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), fMovementYaw, 64, 16, 0, 0, 255, 0, true, 0.1f );
	//NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), fNewYaw, 64, 16, 128, 128, 255, 0, true, 0.1f );
	//NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), flTargetYaw, 64, 16, 255, 0, 0, 0, true, 0.1f );
	//Vector temp = m_vecSavedVelocity;
	//VectorYawRotate(temp, -UTIL_AngleDiff(fNewYaw, fMovementYaw), m_vecSavedVelocity);

	// find our ideal(max) speed at full run
	//SetPoseParameter( "idle_move", 0 );	
	float fIdealSpeed = GetIdealSpeed();
	if (fIdealSpeed <= 0)
	{
#ifdef INFESTED_DLL
		if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
		{
			DevMsg("MoveExecute_Alive calling SetActivity(ACT_RUN)");
		}
#endif
		SetActivity(ACT_RUN);
		fIdealSpeed = GetIdealSpeed();
	}

	// speed up/down depending on facing
	float fYawDifference = abs(UTIL_AngleDiff(fNewYaw, flTargetYaw));
	float accn = 1;
	if (fYawDifference > 45)
	{
		accn = -1;
	}
	else if (fYawDifference > 15)
	{
		accn = 0;
	}
	float fSpeed = m_vecSavedVelocity.Length();
	float fAfterSpeed = clamp(fSpeed + accn * GetIdealAccel() * flInterval, 0, fIdealSpeed);	

	m_vecSavedVelocity = UTIL_YawToVector(fNewYaw) * fAfterSpeed;

	if (asw_debug_drone.GetBool())
		NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), fNewYaw, 64, 16, 128, 128, 255, 0, true, 0.1f );

	Vector vecRequestedMovement = m_vecSavedVelocity;

	// check for alien pushaway forces
	m_bPushed = false;
	if ( asw_springcol.GetBool() && !IsPerformingOverrideMove() && CanBePushedAway() )
	{
		SetupPushawayVector();
		vecRequestedMovement += m_vecLastPush;

		// cap it again
		float fLength = vecRequestedMovement.Length();
		//Msg("Speed = %f ", fLength2D);
		if (fIdealSpeed <= 0)
		{
			vecRequestedMovement.Init();
		}
		else 
		{
			if (fLength > fIdealSpeed)
			{
				float fSlowDown = fIdealSpeed / fLength;
				vecRequestedMovement *= fSlowDown;
				//Msg("Slowdown = %f", fSlowDown);
			}
		}
	}
	
	// do the movement
	Vector vecOriginalPos = GetAbsOrigin();	
	vecRequestedMovement.z = fZMovement;
	m_vecSavedVelocity.z = fZMovement;
	bool bFailedToMove = false;
	if (GetTask() && (GetTask()->iTask == TASK_MELEE_ATTACK1))		// do a non plane sliding movement for attacking
	{
		if ( WalkMove( vecRequestedMovement * flInterval, MASK_NPCSOLID ) == false )
		{
			//Attempt a half-step
			if ( WalkMove( (vecRequestedMovement*0.5f) * flInterval,  MASK_NPCSOLID) == false )
			{
				
			}
			else
			{

			}
		}
	}
	else		// do sliding movement for normal chasing
	{	
		CMoveData MoveData;
		MoveData.m_vecVelocity = vecRequestedMovement;
		MoveData.SetAbsOrigin( GetAbsOrigin() );
		Vector oldPos = GetAbsOrigin();
		g_pDroneMovement->ProcessMovement(this, &MoveData, flInterval);
		UTIL_SetOrigin(this, MoveData.GetAbsOrigin());						
		Vector movediff = GetAbsOrigin() - oldPos;
		float fRequestedMovementLength = (vecRequestedMovement.Length() * flInterval);
		float fFractionMoved = 0;
		if (fRequestedMovementLength > 0)
		{
			fFractionMoved = movediff.Length() / fRequestedMovementLength;
			if (fFractionMoved <= ASW_FAILED_MOVE_FRACTION)
			{
				// todo: don't set this if the thing blocking us was another drone?
				bFailedToMove = true;
				if (asw_debug_drone.GetBool())
					Msg("Drone failed to move (%f/%f) fraction: %f abs=%f,%f,%f\n", 
						fFractionMoved * fRequestedMovementLength, fRequestedMovementLength, fFractionMoved,
						vecRequestedMovement.x, vecRequestedMovement.y, vecRequestedMovement.z);
			}
		}		
	}

	// find out how much we actually moved
	Vector vecMoveDifference = GetAbsOrigin() - vecOriginalPos;	
	vecRequestedMovement = vecMoveDifference / flInterval;

	// set our saved speed to the actual direction moved, and speed too if it's lower
	float fOriginalRequestSpeed = m_vecSavedVelocity.Length();
	m_vecSavedVelocity = vecRequestedMovement;
	float fNewSpeed = m_vecSavedVelocity.Length();
	if (fNewSpeed > fOriginalRequestSpeed)
		m_vecSavedVelocity *= (fOriginalRequestSpeed/fNewSpeed);	
	m_vecSavedVelocity.z = vecMoveDifference.z;

	// make sure our motor knows we're travelling at this speed, so override movement smoothly disengages
	SetAbsVelocity(m_vecSavedVelocity);
	GetMotor()->SetMoveVel(m_vecSavedVelocity);

	if ( CheckStuck() && m_vecLastGoodPosition != vec3_origin )
	{
		if ( asw_debug_drone.GetBool() )
		{
			NDebugOverlay::Line( GetAbsOrigin(), m_vecLastGoodPosition, 255, 0, 0, true, 0.1f );
		}
		SetAbsOrigin(m_vecLastGoodPosition);
	}
	else
	{
		if ( asw_debug_drone.GetBool() )
		{
			NDebugOverlay::Line( GetAbsOrigin(), m_vecLastGoodPosition, 0, 255, 0, true, 0.1f );
		}
	}
	return !bFailedToMove;
}


void CASW_Drone_Advanced::NPCThink()
{
	if (!CheckStuck())
	{
		m_vecLastGoodPosition = GetAbsOrigin();
	}
	
	m_bPerformingOverride = (GetTask() && 
		( (GetTask()->iTask == TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE) || 
		  (GetTask()->iTask == TASK_MELEE_ATTACK1)	   ) );

	BaseClass::NPCThink();

	m_bPerformingOverride = (GetTask() && 
		( (GetTask()->iTask == TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE) || 
		  (GetTask()->iTask == TASK_MELEE_ATTACK1)	   ) );
	
	if (asw_drone_show_override.GetBool())
	{
		float flYaw = GetAbsAngles().y;		
		if (IsPerformingOverrideMove())
			NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), flYaw, 64, 16, 255, 255, 0, 0, true, 0.1f );
		else
			NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), flYaw, 64, 16, 0, 0, 255, 0, true, 0.1f );

	}
	else if (asw_drone_show_facing.GetBool())
	{
		float flYaw = GetAbsAngles().y;
		NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), flYaw, 64, 16, 128, 128, 255, 0, true, 0.1f );
	}

	float fSpeed = GetAbsVelocity().Length();
	static float fPathingSpeed = 0;
	static float fOverrideSpeed = 0;	
	if (IsPerformingOverrideMove() && GetActivity() == ACT_RUN && GetGroundEntity() && GetAbsVelocity().z == 0)
	{
		if (fSpeed > fOverrideSpeed)
		{
			fOverrideSpeed = fSpeed;
		}
	}
	else
	{
		if (fSpeed > fPathingSpeed && fSpeed<400)
		{
			fPathingSpeed = fSpeed;
		}
	}

	m_hAimTarget = GetEnemy();
}

bool CASW_Drone_Advanced::IsMeleeAttacking()
{
	return BaseClass::IsMeleeAttacking();
}

//ConVar asw_drone_waypoint_push_dist( "asw_drone_waypoint_push_dist", "40.0f", FCVAR_CHEAT, "Distance from next waypoint under which drones will not be pushed away by soft collision." );

bool CASW_Drone_Advanced::CanBePushedAway()
{
	// if doing stationary, well telegraphed melee attacks then don't get pushed away during them
	//bool bMeleeAttack = ( IsCurSchedule( SCHED_ASW_ALIEN_MELEE_ATTACK1, false ) || IsCurSchedule( SCHED_ASW_ALIEN_SLOW_MELEE_ATTACK1, false ) );
			//|| IsCurSchedule( SCHED_DRONE_POUNCE ) );
	bool bMeleeAttack = IsMeleeAttacking();

	if ( GetTask() && (GetTask()->iTask == TASK_DRONE_ATTACK_DOOR) ) 
		return false;
	
	if ( !asw_drone_override_attack.GetBool() && bMeleeAttack )
	{
		return false;
	}

	if ( GetNavType() != NAV_GROUND && GetNavType() != NAV_CRAWL )
		return false;

	if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN || GetActivity() == ACT_CLIMB_DISMOUNT )
		return false;

	CAI_Path *pPath = GetNavigator()->GetPath();
	if ( pPath )
	{
		AI_Waypoint_t *pWaypoint = pPath->GetCurWaypoint();
		if ( pWaypoint )
		{
			if ( pWaypoint->NavType() == NAV_JUMP || pWaypoint->NavType() == NAV_CLIMB )
				return false;

// 			float flDistSqr = pWaypoint->GetPos().DistToSqr( GetAbsOrigin() );
// 			if ( flDistSqr < asw_drone_waypoint_push_dist.GetFloat() )
// 			{
// 				return false;
// 			}
		}
	}

	return BaseClass::CanBePushedAway();
}

// gib the drone if he's not already gibbed and below a certain amount of health (i.e. marines shot a burning body)
int CASW_Drone_Advanced::OnTakeDamage_Dead( const CTakeDamageInfo &info )
{
	int result = BaseClass::OnTakeDamage_Dead(info);
	if (m_iHealth <= -20 && info.GetDamageType() != DMG_BURN)
	{
		Event_Gibbed( info );
		result = 0;
	}
	return result;
}

int CASW_Drone_Advanced::MeleeAttack1Conditions( float flDot, float flDist )
{
#if 1 //NOTENOTE: Use predicted position melee attacks

	float		flPrDist, flPrDot;
	Vector		vecPrPos;
	Vector2D	vec2DPrDir;
	
	//Get our likely position in one half second
	//UTIL_PredictedPosition( GetEnemy(), 0.5f, &vecPrPos );
	if (!GetEnemy())
		return COND_TOO_FAR_TO_ATTACK;
	vecPrPos = GetEnemy()->GetAbsOrigin();

	//Get the predicted distance and direction
	flPrDist	= ( vecPrPos - GetAbsOrigin() ).Length();
	vec2DPrDir	= ( vecPrPos - GetAbsOrigin() ).AsVector2D();

	Vector vecBodyDir = BodyDirection2D();

	Vector2D vec2DBodyDir = vecBodyDir.AsVector2D();
	
	flPrDot	= DotProduct2D ( vec2DPrDir, vec2DBodyDir );

	if (!asw_drone_attacks.GetBool())
		return COND_TOO_FAR_TO_ATTACK;

	// have to get close to attack if stunned
	float fAttackRange = m_bElectroStunned ? ASW_DRONE_MELEE1_START_ATTACK_RANGE * 0.7f : ASW_DRONE_MELEE1_START_ATTACK_RANGE;
	if ( flPrDist > fAttackRange )
		return COND_TOO_FAR_TO_ATTACK;

	//if ( flPrDot < 0.5f )
	if ( flPrDot < 0 )	// try generous way
		return COND_NOT_FACING_ATTACK;

#else

	if ( flDot < 0.5f )
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = ASW_DRONE_MELEE1_START_ATTACK_RANGE;

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

// disable pouncing for the moment
int CASW_Drone_Advanced::MeleeAttack2Conditions( float flDot, float flDist )
{
	return 0;
}

void CASW_Drone_Advanced::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_DRONE_WALK_FOOTSTEP )
	{
		//EmitSound( "NPC_Antlion.Footstep", pEvent->eventtime );
		return;
	}

	if ( nEvent == AE_DRONE_FOOTSTEP_SOFT )
	{
		return;
	}

	if ( nEvent == AE_DRONE_FOOTSTEP_HEAVY )
	{
		//EmitSound( "NPC_Antlion.FootstepHeavy", pEvent->eventtime );
		return;
	}

	if ( nEvent == AE_DRONE_MELEE_HIT1 )
	{
		float fDamage = MAX(3.0f, ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_drone_damage.GetFloat()));
		MeleeAttack( ASW_DRONE_MELEE1_RANGE, fDamage, QAngle( 20.0f, 0.0f, -12.0f ), Vector( -250.0f, 1.0f, 1.0f ) );
		return;
	}

	if ( nEvent == AE_DRONE_MELEE_HIT2 )
	{
		float fDamage = MAX(3.0f, ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_drone_damage.GetFloat()));
		MeleeAttack( ASW_DRONE_MELEE1_RANGE, fDamage, QAngle( 20.0f, 0.0f, 0.0f ), Vector( -350.0f, 1.0f, 1.0f ) );
		return;
	}	
	
	if ( nEvent == AE_DRONE_MELEE1_SOUND )
	{
		EmitSound( "ASW_Drone.Attack" );
		return;
	}
	
	if ( nEvent == AE_DRONE_MELEE2_SOUND )
	{
		EmitSound( "ASW_Drone.Attack" );
		return;
	}

	if ( nEvent == AE_DRONE_MOUTH_BLEED )
	{
		Vector vecOrigin, vecDir;
		if (GetAttachment( LookupAttachment("mouth") , vecOrigin, &vecDir ))
			UTIL_ASW_BloodDrips( vecOrigin+vecDir*3, vecDir, BLOOD_COLOR_RED, 6 );
		return;
	}

	if ( nEvent == AE_DRONE_ALERT_SOUND )
	{
		EmitSound( "ASW_Drone.Alert" );
		return;
	}

	if ( nEvent == AE_DRONE_SHADOW_ON)
	{
		RemoveEffects( EF_NOSHADOW );
		return;
	}

	if ( nEvent == AE_ASW_FOOTSTEP || nEvent == AE_MARINE_FOOTSTEP )
	{
		// footsteps are played clientside
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

void CASW_Drone_Advanced::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	CASW_Marine *pMarine = CASW_Marine::AsMarine( pOther );
	if (pMarine)
	{
		int iTouchDamage = asw_drone_touch_damage.GetInt();
		if (GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DISMOUNT)
		{
			// if we touched the marine while climbing up, deliver a big dose of melee damage to him
			iTouchDamage = 20;
			EmitSound( "ASW_Drone.Attack" );

			// shove him a bit too
			Vector vecDir = pMarine->WorldSpaceCenter() - WorldSpaceCenter();
			//vecDir.z = 0.0; // planar
			VectorNormalize( vecDir );
			vecDir *= 200.0f;
			pMarine->ApplyAbsVelocityImpulse(vecDir);

			// we'll fall down already now?
		}
		// don't hurt him if he was hurt recently
		if (m_fLastTouchHurtTime + 0.6f > gpGlobals->curtime || iTouchDamage <= 0)
		{
			//Msg("already hurt recently: %f (cur=%f\n)", m_fLastTouchHurtTime, gpGlobals->curtime);
			return;
		}
		// hurt the marine
		Vector vecForceDir = ( pMarine->GetAbsOrigin() - GetAbsOrigin() );
		float fTouchDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel(iTouchDamage);
		CTakeDamageInfo info( this, this, fTouchDamage, DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForceDir, pMarine->GetAbsOrigin() );
		pMarine->TakeDamage( info );
		//Msg("HURTING MARINE: %f (cur=%f\n)", m_fLastTouchHurtTime, gpGlobals->curtime);
		m_fLastTouchHurtTime = gpGlobals->curtime;
	}
}

void CASW_Drone_Advanced::MeleeAttack( float distance, float damage, QAngle &viewPunch, Vector &shove )
{
	Vector vecForceDir;

	m_bHasAttacked = true;

	// Always hurt bullseyes for now
	if ( ( GetEnemy() != NULL ) && ( GetEnemy()->Classify() == CLASS_BULLSEYE ) )
	{
		vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		CTakeDamageInfo info( this, this, damage, DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForceDir, GetEnemy()->GetAbsOrigin() );
		GetEnemy()->TakeDamage( info );
		return;
	}

	CBaseEntity *pHurt = CheckTraceHullAttack( distance, -Vector(16,16,32), Vector(16,16,32), damage, DMG_SLASH, asw_drone_melee_force.GetFloat() );

	if ( pHurt )
	{
		vecForceDir = ( pHurt->WorldSpaceCenter() - WorldSpaceCenter() );

		// Play a random attack hit sound
		EmitSound( "ASW_Drone.Attack" );
	}
}

bool CASW_Drone_Advanced::CorpseGib( const CTakeDamageInfo &info )
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

void CASW_Drone_Advanced::BuildScheduleTestBits( void )
{
	//Don't allow any modifications when scripted
	if ( m_NPCState == NPC_STATE_SCRIPT )
		return;

	//Make sure we interrupt a run schedule if we can jump
	if ( IsCurSchedule(SCHED_CHASE_ENEMY) )
	{
		SetCustomInterruptCondition( COND_ENEMY_UNREACHABLE );
	}

	if( GetFlags() & FL_ONGROUND )
	{
		if ( GetEnemy() == NULL )
		{
			SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
		}					
	}

	BaseClass::BuildScheduleTestBits();
}

void CASW_Drone_Advanced::GatherEnemyConditions( CBaseEntity *pEnemy )
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
				// Mark the enemy as eluded
				ClearEnemyMemory();
				Msg("Drone lost enemy in CASW_Drone_Advanced::GatherEnemyConditions\n");
				SetEnemy( NULL );
				SetIdealState( NPC_STATE_ALERT );
				SetCondition( COND_ENEMY_UNREACHABLE );
			}
		}
	}
}

bool CASW_Drone_Advanced::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
  	// FIXME: this will break scripted sequences that walk when they have an enemy
	
  	if ( GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN )
  	{
		Vector vecEnemyLKP = GetEnemyLKP();
		
		// Only start facing when we're close enough
		if ( UTIL_DistApprox( vecEnemyLKP, GetAbsOrigin() ) < 192 )
		{
			AddFacingTarget( GetEnemy(), vecEnemyLKP, 1.0, 0.2 );
		}
	}
	else	// otherwise face wher we're going
	{
		AddFacingTarget(move.target, 1.0, 0.2);
	}

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

Activity CASW_Drone_Advanced::NPC_TranslateActivity( Activity eNewActivity )
{
	//if (eNewActivity == ACT_RUN)
	//{
		//eNewActivity = ( Activity ) ACT_DRONE_RUN_ATTACKING;
		//return eNewActivity;
	//}
	//if ( eNewActivity == ACT_CLIMB_DOWN )
		//return ACT_CLIMB_UP;

	return BaseClass::NPC_TranslateActivity(eNewActivity);
}

void CASW_Drone_Advanced::RunTaskOverlay()
{/*
	if ( IsCurTaskContinuousMove() )
	{
		Activity curActivity = GetNavigator()->GetMovementActivity();
		Activity newActivity = curActivity;

		switch( curActivity )
		{
		case ACT_WALK:
			newActivity = ACT_WALK_AIM;
			break;
		case ACT_RUN:
			newActivity = ACT_RUN_AIM;
			break;
		}		

		if ( curActivity != newActivity )
		{
			GetNavigator()->SetMovementActivity( newActivity );
		}
	}*/
	BaseClass::RunTaskOverlay();
}
/*
void CASW_Drone_Advanced::ReachedEndOfSequence()
{
	BaseClass::ReachedEndOfSequence();

	if (GetTask()->iTask == TASK_MELEE_ATTACK1 && GetActivity() == ACT_DRONE_RUN_ATTACKING)
	{
		Msg("Making task complete early!\n");
		TaskComplete();
		SetActivity( ACT_RUN );
	}
}*/

void CASW_Drone_Advanced::RunTask( const Task_t *pTask )
{
	// make sure the drone is small when bashing a door, or trying to get to a door
	SetSmallDoorBashHull(pTask->iTask == TASK_DRONE_ATTACK_DOOR || pTask->iTask == TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT);

	switch ( pTask->iTask )
	{
	case TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT:
		{
			// see if we're near enough to the door
			if (LinearDistanceToDoor() <= asw_drone_door_distance.GetFloat())
				TaskComplete();
			
			bool fTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );
			
			if (fTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				//if (fTimeExpired)
					//Msg("Finished TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT with fTimeExpired\n");
				//else
					//Msg("Finished TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT with run no goal in runtask\n");
				//TaskComplete();
				//GetNavigator()->StopMoving();		// Stop moving
				//SetSchedule(SCHED_DRONE_DOOR_WAIT);
				TaskFail("Can't get to door");
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();
			}

			break;
		}
	case TASK_DRONE_WAIT_FACE_ENEMY:
		{
			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}

			Vector vecEnemyLKP = vec3_origin;
			bool bUpdateYaw = false;
			if ( GetEnemy() )
			{
				vecEnemyLKP = GetEnemyLKP();
				bUpdateYaw = !FInAimCone( vecEnemyLKP );
			}
			
			if ( bUpdateYaw )
			{
				GetMotor()->SetIdealYawToTargetAndUpdate( vecEnemyLKP , AI_KEEP_YAW_SPEED );
			}

			// stop waiting if our enemy moved
			if ( !GetEnemy() )
			{
				TaskComplete();
			}
			else if ( ( vecEnemyLKP - m_vecEnemyStandoffPosition ).Length() > 120.0f )
			{
				TaskComplete();
			}
			else if ( IsWaitFinished() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE:
		{
			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}

			bool fTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );
			
			if (fTimeExpired || !GetEnemy() || m_lifeState == LIFE_DEAD || GetHealth() <= 0)
			{
				TaskComplete();
				//GetNavigator()->StopMoving();		// Stop moving
			}			
			break;
		}
	case TASK_DRONE_DOOR_WAIT:
		{
			CASW_Door *pDoor = m_hBlockingDoor.Get();
			if ( !pDoor || pDoor->m_bDoorFallen
					|| ( GetEnemy() && !IsBehindDoor( GetEnemy() ) && GetAlienOrders() != AOT_MoveToIgnoringMarines ) )	// or enemy is no longer behind the door
			{
				//if (!pDoor && entindex()==45)
					//Msg("Drone completing door wait since no door\n");
				//else if (entindex() == 45)
					//Msg("Drone completing door wait since door health is 0\n");
				
				m_iDoorPos = 0;
				TaskComplete();
			}
			break;
		}

	case TASK_DRONE_ATTACK_DOOR:
		{
			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}
			//if ( IsActivityFinished() )
			//{
				// periodically check if the door is still ok to bash
				if ( m_DurationDoorBash.Expired() )
				{
					if (!ValidBlockingDoor())
					{
						TaskComplete();
					}
					m_DurationDoorBash.Reset();
				}
				//else
					//ResetIdealActivity( SelectDoorBash() );
			//}
			break;
		}
	case TASK_MELEE_ATTACK1:
		{
			BaseClass::RunTask(pTask);

			if (TaskIsComplete())
			{
				//m_flSpeed = GetIdealSpeed();
#ifdef INFESTED_DLL
				if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
				{
					DevMsg("RunTask TASK_MELEE_ATTACK1 calling SetActivity(ACT_RUN)");
				}
#endif
				SetActivity( ACT_RUN );
				GetNavigator()->ClearGoal();
				m_flSavedSpeed = GetIdealSpeed();

			}
			break;
		}
	default:
		{
			BaseClass::RunTask(pTask);
		}
	}

	//if (!HasCondition(COND_NPC_FREEZE) && !IsCurSchedule(SCHED_NPC_FREEZE))
	//{
		if (GetActivity() == ACT_RUN || GetActivity() == ACT_DRONE_RUN_ATTACKING
			|| GetActivity() == ACT_MELEE_ATTACK1)
			m_flPlaybackRate = 1.25f;
		else
			m_flPlaybackRate = 1.0f;
	//}
}

bool CASW_Drone_Advanced::ShouldGib( const CTakeDamageInfo &info )
{
	// don't gib if we burnt to death
	//if (info.GetDamageType() & DMG_BURN)
		//return false;

	//if (info.GetDamageType() & DMG_ALWAYSGIB)
		//return true;

	//return m_bGibber;

	// force ragdoll so (gibbers will gib clientside from the ragdoll)
	return false;
}

void CASW_Drone_Advanced::Event_Killed( const CTakeDamageInfo &info )
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

// === Door Bashing stuff ========

// does a rough check to see if we have a clear direct path to the target entity
//   (this is used to determine if we should use override movement, or pathed)
bool CASW_Drone_Advanced::HasOverridePathTo(CBaseEntity* pEnt)
{
	trace_t	tr;
	//UTIL_TraceLine( EyePosition(), GetEnemy()->WorldSpaceCenter(), MASK_SOLID, this, GetCollisionGroup(), &tr );
	CDroneTraceFilterLOS traceFilter( this, GetCollisionGroup() );
	UTIL_TraceLine( EyePosition(), pEnt->WorldSpaceCenter(), MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	if (tr.m_pEnt != pEnt && tr.fraction < 1.0f)	// centre of the alien can't see the centre of the enemy
		return false;

	// check our corners
	Vector vecOffset(0,0,0);
	vecOffset.x = GetHullMins().x;
	vecOffset.y = GetHullMins().y;
	UTIL_TraceLine( WorldSpaceCenter() + vecOffset, pEnt->WorldSpaceCenter() + vecOffset, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	if (tr.m_pEnt != pEnt && tr.fraction < 1.0f)	// centre of the alien can't see this corner
		return false;

	vecOffset.x = GetHullMaxs().x;
	UTIL_TraceLine( WorldSpaceCenter() + vecOffset, pEnt->WorldSpaceCenter() + vecOffset, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	if (tr.m_pEnt != pEnt && tr.fraction < 1.0f)	// centre of the alien can't see this corner
		return false;

	vecOffset.x = GetHullMins().x;
	vecOffset.y = GetHullMaxs().y;
	UTIL_TraceLine( WorldSpaceCenter() + vecOffset, pEnt->WorldSpaceCenter() + vecOffset, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	if (tr.m_pEnt != pEnt && tr.fraction < 1.0f)	// centre of the alien can't see this corner
		return false;

	vecOffset.x = GetHullMaxs().x;
	UTIL_TraceLine( WorldSpaceCenter() + vecOffset, pEnt->WorldSpaceCenter() + vecOffset, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	if (tr.m_pEnt != pEnt && tr.fraction < 1.0f)	// centre of the alien can't see this corner
		return false;

		//AIMoveTrace_t moveTrace;
		//unsigned testFlags = AITGM_IGNORE_FLOOR;
		//GetMoveProbe()->TestGroundMove( GetLocalOrigin(), GetEnemy()->WorldSpaceCenter(), MASK_OPAQUE_AND_NPCS, testFlags, &moveTrace );
		//if ( !IsMoveBlocked(moveTrace.fStatus) || 
			//( moveTrace.flTotalDist - moveTrace.flDistObstructed < GetEnemy()->BoundingRadius() * 1.5 )
			//)
	return true;
}

bool CASW_Drone_Advanced::CheckStuck()
{
	trace_t tr;	
	Ray_t ray;
	ray.Init( GetAbsOrigin(), GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs() );
	UTIL_TraceRay( ray, GetAITraceMask(), this, GetCollisionGroup(), &tr );
// 	if ( (traceresult.contents & MASK_NPCSOLID) && traceresult.m_pEnt )
// 	{
// 		return true;
// 	}
	if ( tr.startsolid || tr.fraction < 1.0f || tr.allsolid )
		return true;
	return false;
}

void CASW_Drone_Advanced::GatherConditions( void )
{	
	BaseClass::GatherConditions();

	bool bHadBlocked = HasCondition(COND_DRONE_BLOCKED_BY_DOOR);	

	AI_Waypoint_t *pWaypoint = NULL;
	if ( GetNavigator()->GetPath() )
	{
		pWaypoint = GetNavigator()->GetPath()->GetCurWaypoint();
	}
	bool bOnLadder = ( ( pWaypoint != NULL ) && ( pWaypoint->NavType() == NAV_CLIMB ) );

	// ignore enemy conditions when climbing on a ladder so that we don't fall off
	if ( bOnLadder && IsCurSchedule( SCHED_DRONE_CHASE_ENEMY, false ) )
	{
		static int chaseConditionsToClear[] = 
		{
			COND_NEW_ENEMY,
			COND_ENEMY_DEAD,
			COND_ENEMY_UNREACHABLE,
			COND_CAN_RANGE_ATTACK1,
			COND_CAN_MELEE_ATTACK1,
			COND_CAN_RANGE_ATTACK2,
			COND_CAN_MELEE_ATTACK2,
			COND_TOO_CLOSE_TO_ATTACK,
			COND_DRONE_GAINED_LOS,
			COND_HEAVY_DAMAGE,
			COND_ALIEN_SHOVER_PHYSICS_TARGET,
		};

		ClearConditions( chaseConditionsToClear, ARRAYSIZE( chaseConditionsToClear ) );
	}

	// check if we have a clear view of our enemy, to do simple chasing movement instead of pathed movement
	bool bLOS = false;
	if (GetEnemy())
	{
		// Don't report having line of sight while climbing.  This prevents the
		// alien from falling off ladders due to being interrupted by COND_DRONE_GAINED_LOS.
		if ( !bOnLadder )
		{
			Vector diff = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
			float dist = diff.Length2D();
			if (dist < 1000)
			{
				//Vector eyediff = GetEnemy()->WorldspaceCenter() - EyePosition();
				bLOS = HasOverridePathTo(GetEnemy());
			}
		}
	}
	ClearCondition(COND_DRONE_GAINED_LOS);
	if (!bLOS && m_bLastSoftLOS)
	{
		ClearCondition(COND_DRONE_LOS);
		SetCondition(COND_DRONE_LOST_LOS);
		m_fLastLostLOSTime = gpGlobals->curtime;
	}
	else if (bLOS)
	{		
		if (!m_bLastSoftLOS)
		{
			SetCondition(COND_DRONE_GAINED_LOS);
			//Msg("%f COND_DRONE_GAINED_LOS\n", gpGlobals->curtime);
		}
		SetCondition(COND_DRONE_LOS);
		ClearCondition(COND_DRONE_LOST_LOS);
	}
	else
	{
		ClearCondition( COND_DRONE_LOS );
		ClearCondition( COND_DRONE_LOST_LOS );
	}
	m_bLastSoftLOS = HasCondition( COND_DRONE_LOS );

	static int conditionsToClear[] = 
	{
		COND_DRONE_BLOCKED_BY_DOOR,
		COND_DRONE_DOOR_OPENED,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );

	if ( m_hBlockingDoor == NULL || 
		 ( m_hBlockingDoor->IsDoorOpen() || 
		   m_hBlockingDoor->IsDoorOpening() || m_hBlockingDoor->m_bDoorFallen )  )
	{
		ClearCondition( COND_DRONE_BLOCKED_BY_DOOR );
		if ( m_hBlockingDoor != NULL )
		{
			SetCondition( COND_DRONE_DOOR_OPENED );
			m_hBlockingDoor = NULL;
		}
	}
	else
	{
		if (!bHadBlocked)
		{
			if (asw_debug_drone.GetBool())
				Msg("Setting door blocked condition\n");
		}
		SetCondition( COND_DRONE_BLOCKED_BY_DOOR );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

int CASW_Drone_Advanced::SelectSchedule( void )
{
	if ( HasCondition(COND_NEW_ENEMY) )
	{
		// if we have a new enemy, upgrade our sight range, so we don't lose him so easily while chasing
		SetDistLook( 1768.0f );
		SetDistSwarmSense(1200.0f);
	}

	// check for jumping off marine heads
	if (GetGroundEntity() && GetGroundEntity()->Classify() == CLASS_ASW_MARINE && DoJumpOffHead())
		return SCHED_ASW_ALIEN_JUMP;

	//Msg("Drone Selecting schedule\n");
	if ( HasCondition( COND_DRONE_BLOCKED_BY_DOOR ) && m_hBlockingDoor != NULL )
	{
		//Msg("%d: possible selecting bash schedule\n", entindex());
		ClearCondition( COND_DRONE_BLOCKED_BY_DOOR );
		if ( ValidBlockingDoor() )
		{
			//Msg(" yes i did\n");
			return SCHED_DRONE_BASH_DOOR;
		}
		//Msg("Clearing door as it's not a valid blocking door\n");
		m_hBlockingDoor = NULL;
	}

	int nSched = SelectFlinchSchedule_ASW();
	if ( nSched != SCHED_NONE )
		return nSched;

	return BaseClass::SelectSchedule();
}

int CASW_Drone_Advanced::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( HasCondition( COND_DRONE_BLOCKED_BY_DOOR ) && m_hBlockingDoor != NULL )
	{
		ClearCondition( COND_DRONE_BLOCKED_BY_DOOR );
		if ( ValidBlockingDoor()) // && failedSchedule != SCHED_DRONE_BASH_DOOR )
		{
			if (asw_debug_drone.GetBool())
				Msg("selecting bash from fail schedule\n");
			return SCHED_DRONE_BASH_DOOR;
		}
		//Msg("Clearing door in schedule failure because valid=%d failed=%d\n", ValidBlockingDoor(), (int) failedSchedule);
		m_hBlockingDoor = NULL;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

bool CASW_Drone_Advanced::ValidBlockingDoor()
{
	if (m_hBlockingDoor == NULL)
		return false;

	if (m_hBlockingDoor->IsDoorOpen() || m_hBlockingDoor->IsDoorOpening())
		return false;

	if (m_hBlockingDoor->m_bDoorFallen)
		return false;

	// not a valid door if our enemy isn't behind it
	if ( GetAlienOrders() != AOT_MoveToIgnoringMarines && ( !GetEnemy() || !IsBehindDoor( GetEnemy() ) ) )
		return false;

	return true;
}

void CASW_Drone_Advanced::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_UNBURROW:
		BaseClass::StartTask( pTask );
		m_fFailedOverrideTime = gpGlobals->curtime;	// stop us doing an override move immediately
		break;
	case TASK_MELEE_ATTACK1:
		{
			// check for making a marine shout out in fear
			if (!m_bDoneAlienCloseChatter && gpGlobals->curtime > s_fNextTooCloseChatterTime)
			{
				CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetEnemy());
				if (pMarine)
				{
					pMarine->GetMarineSpeech()->Chatter(CHATTER_ALIEN_TOO_CLOSE);
					m_bDoneAlienCloseChatter = true;
					s_fNextTooCloseChatterTime = gpGlobals->curtime + random->RandomInt(10, 30);
				}
			}
			BaseClass::StartTask( pTask );
			break;
		}
	case TASK_SMALL_FLINCH:
	case TASK_BIG_FLINCH:
		{
			Remember(bits_MEMORY_FLINCHED);
			Activity flinch = GetFlinchActivity( false, false );
			SetIdealActivity( flinch );
			if ( flinch == ACT_ALIEN_FLINCH_SMALL )
				m_flNextFlinchTime = gpGlobals->curtime + 0.45f;
			else if ( flinch == ACT_ALIEN_FLINCH_MEDIUM )
				m_flNextFlinchTime = gpGlobals->curtime + 0.8f;
			else if ( flinch == ACT_ALIEN_FLINCH_BIG )
				m_flNextFlinchTime = gpGlobals->curtime + 1.25f;
			break;
		}
	case TASK_DRONE_YAW_TO_DOOR:
		{
			AssertMsg( m_hBlockingDoor != NULL, "Expected condition handling to break schedule before landing here" );
			if ( m_hBlockingDoor != NULL )
			{
				SetDoorBashYaw();
				GetMotor()->SetIdealYaw( m_flDoorBashYaw );
			}
			TaskComplete();
			break;
		}
	case TASK_DRONE_GET_PATH_TO_DOOR:
		{
			Vector vecGoalPos;
			Vector vecDir;

			float fDistToDoor = LinearDistanceToDoor();
			// if we have no door to bash then just set a path to our current loc
			if (!m_hBlockingDoor.IsValid() || fDistToDoor <= asw_drone_door_distance.GetFloat())
			{
				AI_NavGoal_t goal( GetAbsOrigin() );
				GetNavigator()->SetGoal(goal);
				TaskComplete();
				break;
			}

			vecDir = GetLocalOrigin() - m_hBlockingDoor->GetLocalOrigin();
			VectorNormalize(vecDir);
			vecDir.z = 0;

			Vector vecDoorPos = m_hBlockingDoor->GetAbsOrigin();
			// head to the sides of the door?
			if (m_iDoorPos == 1)
			{
				Vector vecSide;
				QAngle angFacing = m_hBlockingDoor->GetAbsAngles();
				angFacing[YAW] += 90;
				AngleVectors(angFacing, &vecSide);				
				vecDoorPos += vecSide * 50.0f;				
			}
			else if (m_iDoorPos == 2)
			{
				Vector vecSide;
				QAngle angFacing = m_hBlockingDoor->GetAbsAngles();
				angFacing[YAW] += 90;
				AngleVectors(angFacing, &vecSide);				
				vecDoorPos -= vecSide * 50.0f;	
			}

			m_iDoorPos = m_iDoorPos + 1;

			AI_NavGoal_t goal( m_hBlockingDoor->WorldSpaceCenter() );

			goal.pTarget = m_hBlockingDoor;
			GetNavigator()->SetGoal( goal );

			if ( asw_debug_drone.GetInt() == 1 )
			{
				NDebugOverlay::Cross3D( vecGoalPos, Vector(8,8,8), -Vector(8,8,8), 0, 255, 0, true, 2.0f );
				NDebugOverlay::Line( vecGoalPos, m_hBlockingDoor->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
				NDebugOverlay::Line( m_hBlockingDoor->WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
			}

			TaskComplete();
		}
		break;
	case TASK_DRONE_GET_CIRCLE_PATH:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if (!pEnemy)
			{
				TaskFail(FAIL_NO_ROUTE);
				return;
			}
			// We want to move to a different location around our enemy, because the route from here to him is blocked
			// (even with triangulation).   We could pick a random spot around him, but that would probably cause lots
			// of drones to bump into each other.  Let's try and make them move anticlockwise around the enemy

			// rotate our position around the enemy by a random amount
			Vector vecDiff = GetAbsOrigin() - GetEnemy()->GetAbsOrigin();
			float dist = vecDiff.Length();
			float fYaw = UTIL_VecToYaw(vecDiff);
			fYaw -= random->RandomFloat(20.0f, 40.0f);
			QAngle newDir(0, fYaw, 0);
			Vector vecNewPos;
			AngleVectors(newDir, &vecNewPos);
			vecNewPos *= dist * (random->RandomFloat(0.8f, 1.2f));
			vecNewPos += GetEnemy()->GetAbsOrigin();

			// do a trace towards our new picked pos and stop at an obstruction, just to make it less likely we're
			// sending the AI into a solid object
			trace_t tr;			
			UTIL_TraceHull( GetAbsOrigin() + Vector(0, 0, 20),
							vecNewPos  + Vector(0, 0, 20),
							Vector(-5, -5, -5),
							Vector(5, 5, 5),
							MASK_NPCSOLID_BRUSHONLY,
							this,
							COLLISION_GROUP_NONE,
							&tr );

			Vector vecCirclePos;
			if (tr.fraction == 1.0)
				vecCirclePos = vecNewPos;
			else if (tr.fraction > 0)
				vecCirclePos = tr.endpos;
			else
			{
				TaskFail(FAIL_NO_ROUTE);
			}

			AI_NavGoal_t goal( vecCirclePos );

			TranslateNavGoal( pEnemy, goal.dest );

			if ( GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				DevWarning( 2, "TASK_DRONE_GET_CIRCLE_PATH failed!!\n" );				
				TaskFail(FAIL_NO_ROUTE);
			}
			break;
		}
	case TASK_DRONE_WAIT_FACE_ENEMY:
		SetWait( pTask->flTaskData );
		m_vecEnemyStandoffPosition = GetEnemyLKP();
		break;
	case TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT:
		{
			// see if we're near enough to the door
			if (LinearDistanceToDoor() <= asw_drone_door_distance.GetFloat())
				TaskComplete();			

			if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				//Msg("%d: Finished TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT with no goal\n", entindex());
				TaskComplete();
				GetNavigator()->ClearGoal();		// Clear residual state
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();
			}
			break;
		}

	case TASK_DRONE_ATTACK_DOOR:
		{
			// check we're near enough to bash the door
			int door_dist = LinearDistanceToDoor();
			if ( door_dist > asw_drone_door_distance.GetFloat())
				TaskFail("Too far from door");
			else if (door_dist < asw_drone_door_distance_min.GetFloat())
			{
				//MoveAwayFromDoor();
			}
		 	m_DurationDoorBash.Reset();
			ResetIdealActivity( SelectDoorBash() );
			break;
		}
	case TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE:
		{
			if (!GetEnemy() || m_lifeState == LIFE_DEAD || GetHealth()<=0)
			{
				TaskComplete();
			}
			else
			{
#ifdef INFESTED_DLL
		if (ai_sequence_debug.GetBool() == true && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
		{
			DevMsg("StartTask TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE calling SetActivity(ACT_RUN)");
		}
#endif
				SetActivity(ACT_RUN);
				// note: Override move will kick in while we're doing this task and run us straight at our enemy
			}
			break;
		}
	case TASK_DRONE_DOOR_WAIT:
		{
			if (ValidBlockingDoor() && LinearDistanceToDoor() <= asw_drone_door_distance.GetFloat())
			{
				//Msg("Drone wait finishing to do a bash!\n");
				TaskComplete();
				SetSchedule(SCHED_DRONE_BASH_CLOSE_DOOR);
			}			
		}
		break;
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

bool CASW_Drone_Advanced::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal,
										float distClear,
										AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.pObstruction )
	{
		//Msg("Drone blocked by %s\n", pMoveGoal->directTrace.pObstruction->GetClassname());		
		// check if we collide with a door or door padding
		CASW_Door *pDoor = dynamic_cast<CASW_Door *>( pMoveGoal->directTrace.pObstruction );
		if (!pDoor)
		{
			CASW_Door_Padding *pPadding = dynamic_cast<CASW_Door_Padding *>( pMoveGoal->directTrace.pObstruction );
			if (pPadding)
				pDoor = dynamic_cast<CASW_Door *>( pPadding->GetParent() );
		}
		if ( pDoor && OnObstructingASWDoor( pMoveGoal, pDoor, distClear, pResult ) )
		{
			//NDebugOverlay::HorzArrow(WorldSpaceCenter(), pMoveGoal->directTrace.pObstruction->WorldSpaceCenter(), 5, 255,255,0,255,true, 1.0f);
			return true;
		}
		else
		{
			CASW_Drone_Advanced *pDrone = dynamic_cast<CASW_Drone_Advanced *>( pMoveGoal->directTrace.pObstruction );
			if (pDrone && pDrone->m_hBlockingDoor.Get() != NULL)
			{
				if (OnObstructingASWDoor( pMoveGoal, pDrone->m_hBlockingDoor.Get(), distClear, pResult ) )
				{
					//NDebugOverlay::HorzArrow(WorldSpaceCenter(), pMoveGoal->directTrace.pObstruction->WorldSpaceCenter(), 5, 255,255,0,255,true, 1.0f);
					return true;
				}
			}
		}
	}
	//if (ValidBlockingDoor())
		//NDebugOverlay::HorzArrow(WorldSpaceCenter(), pMoveGoal->directTrace.pObstruction->WorldSpaceCenter(), 5, 0,0,255,255,true, 1.0f);
	//else
		//NDebugOverlay::HorzArrow(WorldSpaceCenter(), pMoveGoal->directTrace.pObstruction->WorldSpaceCenter(), 5, 255,0,0,255,true, 1.0f);

	return false;
}

bool CASW_Drone_Advanced::OnObstructingASWDoor( AILocalMoveGoal_t *pMoveGoal, CASW_Door *pDoor, 
							  float distClear, AIMoveResult_t *pResult )
{
	if ( pMoveGoal->maxDist < distClear )
		return false;

	// don't start bashing a door if we're already trying to bash a door
	if (GetTask() && GetTask()->iTask == TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT)
		return false;

	// By default, NPCs don't know how to open doors
	if ( pDoor->IsDoorClosed() || pDoor->IsDoorClosing() )
	{
		if ( distClear < 0.1 )
		{
			*pResult = AIMR_BLOCKED_ENTITY;
		}
		else
		{
			pMoveGoal->maxDist = distClear;
			*pResult = AIMR_OK;
		}

		if  ( IsMoveBlocked( *pResult ) && pMoveGoal->directTrace.vHitNormal != vec3_origin )
		{
			if (asw_debug_drone.GetBool())
				Msg("OnObstructingASWDoor setting m_hBlockingDoor, sched = %s\n", GetCurSchedule()->GetName());
			m_hBlockingDoor = pDoor;
			//m_flDoorBashYaw = UTIL_VecToYaw( pMoveGoal->directTrace.vHitNormal * -1 );	
			SetDoorBashYaw();
		}		
		return true;
	}

	return false;
}

bool CASW_Drone_Advanced::ShouldJump( void )
{
	if (!m_bJumper)
		return false;

	return BaseClass::ShouldJump();
}

// translate our chase schedule into our version which is interrupted by a blocked door
int CASW_Drone_Advanced::TranslateSchedule( int scheduleType )
{
	int i = BaseClass::TranslateSchedule(scheduleType);
	if (i == SCHED_SHOVER_CHASE_ENEMY)
	{
		// decide whether to go with pathed movement or straight override running at the enemy
		if (HasCondition(COND_DRONE_LOS) && gpGlobals->curtime - m_fLastLostLOSTime > 2.0f
				&& asw_drone_override_move.GetBool() && (!FailedOverrideMove() || IsMeleeAttacking()))
			i = SCHED_DRONE_OVERRIDE_MOVE;
		else
		{			
			i = SCHED_DRONE_CHASE_ENEMY;
		}
	}
	if (i == SCHED_DRONE_DOOR_WAIT && m_hBlockingDoor.Get() && !m_hBlockingDoor.Get()->m_bDoorFallen)
	{
		if (m_iDoorPos < 3)
		{
			//Msg("Translated to door bash (doorpos=%d)\n", m_iDoorPos);
			return SCHED_DRONE_BASH_DOOR;
		}
	}
	if (i == SCHED_CHASE_ENEMY_FAILED && GetEnemy() && HasCondition(COND_DRONE_LOS))
	{
		return SCHED_DRONE_CIRCLE_ENEMY;
	}
	if (i == SCHED_STANDOFF)
		return SCHED_DRONE_STANDOFF;

	return i;
}

//---------------------------------------------------------
//---------------------------------------------------------

Activity CASW_Drone_Advanced::SelectDoorBash()
{
	//if ( random->RandomInt( 1, 3 ) == 1 )
		//return ACT_MELEE_ATTACK1;
	return (Activity)ACT_DRONE_WALLPOUND;
}

bool CASW_Drone_Advanced::IsCurTaskContinuousMove()
{
	const Task_t* pTask = GetTask();
	if( !pTask )
		return true;

	switch( pTask->iTask )
	{
	case TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE:
	case TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT:
		return true;
		break;

	default:
		return BaseClass::IsCurTaskContinuousMove();
		break;
	}
}

int	CASW_Drone_Advanced::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		NDebugOverlay::EntityText( entindex(), text_offset, CFmtStr( "m_takedamage = %d", m_takedamage ), 0 );
		text_offset++;
		if (GetEnemy())
		{
			if (HasCondition(COND_CAN_MELEE_ATTACK1))
				NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("VVV CAN ATTACK VV"),0);
			else
				NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("--- CAN'T ATTACK ---"),0);
			text_offset++;
		}
		if (CapabilitiesGet() & bits_CAP_MOVE_JUMP)
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("Can Jump!"),0);
		else
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("Cannot jump."),0);
		text_offset++;
			
		if (HasCondition(COND_DRONE_LOS))
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("Has COND_DRONE_LOS"),0);
		else
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("No COND_DRONE_LOS"),0);
		text_offset++;
		if (HasCondition(COND_DRONE_GAINED_LOS))
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("Has COND_DRONE_GAINED_LOS"),0);
		else
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("No COND_DRONE_GAINED_LOS"),0);
		text_offset++;

		if (GetTask() && ( (GetTask()->iTask == TASK_DRONE_DOOR_WAIT) || (GetTask()->iTask == TASK_DRONE_ATTACK_DOOR)))
		{
			if (GetEnemy() && !IsBehindDoor(GetEnemy()))
				NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("Enemy behind door"),0);
			else
				NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr("Not behind door or no enemy"),0);
			text_offset++;
		}

		NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "Local org = %f %f %f", VectorExpand( GetLocalOrigin() ) ),0);
		text_offset++;
		NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "Abs org = %f %f %f", VectorExpand( GetLocalOrigin() ) ),0);
		text_offset++;
		if ( GetParent() )
		{
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "Parent lorg = %f %f %f", VectorExpand( GetParent()->GetLocalOrigin() ) ),0);
			text_offset++;
			NDebugOverlay::EntityText(entindex(),text_offset,CFmtStr( "Parent org = %f %f %f", VectorExpand( GetParent()->GetAbsOrigin() ) ),0);
			text_offset++;
		}
	}
	return text_offset;
}

bool CASW_Drone_Advanced::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{	
	float multiplier = 1;
	if ( moveType == bits_CAP_MOVE_JUMP )
	{
		*pCost *= 10.0f;	// discourage jumping
	}
	else if ( moveType == bits_CAP_MOVE_CLIMB )
	{
		float delta = vecEnd.z - vecStart.z;
		multiplier = ( delta > 0 ) ? 0.5 : 4.0;		// make it really expensive to climb down
		*pCost *= multiplier;
	}

	// increase cost of marines can see vecend?
	//if (UTIL_ASW_MarineViewCone(vecEnd))
		//multiplier *= asw_marine_view_cone_cost.GetFloat();	

	return ( multiplier != 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for NPC LOS traces
//-----------------------------------------------------------------------------
CDroneTraceFilterLOS::CDroneTraceFilterLOS( IHandleEntity *pHandleEntity, int collisionGroup ) :
		CTraceFilterSimple( pHandleEntity, collisionGroup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDroneTraceFilterLOS::ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
{
	CBaseEntity *pEntity = (CBaseEntity *)pServerEntity;

	if ( !pEntity->BlocksLOS() )
		return false;

	if (pEntity->Classify() == CLASS_ASW_DRONE)
		return false;

	if (pEntity->Classify() == CLASS_ASW_SHIELDBUG)
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pServerEntity, contentsMask );
}

// only shock damage counts as heavy (and thus causes a flinch even during normal running)
bool CASW_Drone_Advanced::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// explosions always cause a flinch
	if (( info.GetDamageType() & DMG_BLAST ) != 0 )
		return true;

	// dots never cause a flinch
	if (( info.GetDamageType() & DMG_DIRECT ) != 0 )
		return false;

	// shock damage always causes large flinch
	if (( info.GetDamageType() & DMG_SHOCK ) != 0 )
	{
		m_FlinchActivity = (Activity) ACT_ALIEN_FLINCH_BIG;
		return true;
	}

	// todo: melee, based on marine's melee skill?

	// check the attacker's weapon, if this is a marine
	//if (info.GetAttacker())
	//{
		//Msg("Drone attacked by %s\n", info.GetAttacker()->GetClassname());
	//}
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
	if (pMarine)
	{
		if (asw_debug_alien_damage.GetBool())
			Msg("Drone checking damage from a marine, of type: %d\n", info.GetDamageType());
		// if we've been kicked by a marine, store the flinch we should do based on that marine's melee skill
		if ((info.GetDamageType() & DMG_CLUB) != 0)
		{
			int i = pMarine->GetAlienMeleeFlinch();
			if (i == 0)
				m_FlinchActivity = (Activity) ACT_ALIEN_FLINCH_SMALL;
			else if (i == 2)
				m_FlinchActivity = (Activity) ACT_ALIEN_FLINCH_BIG;
			else 
				m_FlinchActivity = (Activity) ACT_ALIEN_FLINCH_MEDIUM;
			//Msg("Storing flinch %d (%d)\n", (int) m_FlinchActivity, i);
			return true;
		}
	}
	m_FlinchActivity = ACT_INVALID;
	if (pMarine && pMarine->GetActiveASWWeapon())
	{		
		return pMarine->GetActiveASWWeapon()->ShouldAlienFlinch(this, info);
	}

	return false;
}

int CASW_Drone_Advanced::SelectFlinchSchedule_ASW()
{
	//if ( !HasCondition(COND_HEAVY_DAMAGE) && !HasCondition(COND_LIGHT_DAMAGE) )
		//return SCHED_NONE;

	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
		return SCHED_NONE;

	// only light damage flinch if shot during a melee attack
	//if (!HasCondition(COND_HEAVY_DAMAGE) && !(GetTask() && (GetTask()->iTask == TASK_MELEE_ATTACK1)) )
		//return SCHED_NONE;
	if (!HasCondition(COND_HEAVY_DAMAGE))	// only flinch on heavy damage condition (which is set if a particular marine's weapon + skills cause a flinch)
		return SCHED_NONE;

	if (asw_debug_alien_damage.GetBool())
		Msg("We have COND_HEAVY_DAMAGE, so flinching\n");

	// Heavy damage. Break out of my current schedule and flinch.
	//Activity iFlinchActivity = GetFlinchActivity( true, false );
	//Msg("Clearing flinch activity (%d) in SelectFlinchSchedule_ASW\n", (int) iFlinchActivity);
	//m_FlinchActivity = ACT_INVALID;
	//if ( HaveSequenceForActivity( iFlinchActivity ) )

	// assume our aliens have a schedule
	return SCHED_BIG_FLINCH;

	//return SCHED_NONE;
}

void CASW_Drone_Advanced::CheckFlinches( void )
{
	// If we're currently flinching, don't allow gesture flinches to be overlaid
	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
	{
		ClearCondition( COND_LIGHT_DAMAGE );
		ClearCondition( COND_HEAVY_DAMAGE );
	}

	// If it's been a while since we did a full flinch, forget that we flinched so we'll flinch fully again
	if ( HasMemory(bits_MEMORY_FLINCHED) && gpGlobals->curtime > m_flNextFlinchTime )
	{
		Forget(bits_MEMORY_FLINCHED);
	}

	// check for gesture flinches
	if ( HasCondition( COND_LIGHT_DAMAGE ) && gpGlobals->curtime > m_flNextSmallFlinchTime )
	{
		if ( HaveSequenceForActivity( (Activity) ACT_ALIEN_FLINCH_GESTURE ) )
		{
			m_flNextSmallFlinchTime = gpGlobals->curtime + RandomFloat( 0.3f, 1.0f );
			RestartGesture( (Activity) ACT_ALIEN_FLINCH_GESTURE );
		}
	}
}

Activity CASW_Drone_Advanced::GetFlinchActivity( bool bHeavyDamage, bool bGesture )
{
	static int iFlinchCount = 0;
	iFlinchCount++;
	if (asw_debug_alien_damage.GetBool())
		Msg("Drone flinching (total flinch count = %d)\n", iFlinchCount);
	// todo: if we have a flinch damage type set, use that
	if (m_FlinchActivity != ACT_INVALID)
	{
		if (asw_debug_alien_damage.GetBool())
			Msg("  Returning stored melee flinch %d\n", (int) m_FlinchActivity);
		return m_FlinchActivity;
	}

	//Msg("Returning default small flinch\n");
	//return (Activity) ACT_SMALL_FLINCH;
	// random flinch
	//   todo: use longer flinch for bigger melee hits?
	float f = random->RandomFloat();
	if (f < 0.3f)
		return (Activity) ACT_ALIEN_FLINCH_SMALL;
	else if (f < 0.66f)
		return (Activity) ACT_ALIEN_FLINCH_MEDIUM;
	return (Activity) ACT_ALIEN_FLINCH_BIG;
}

void CASW_Drone_Advanced::SetSmallDoorBashHull( bool bSmall )
{
	// don't change if something else is already messing with our hull
	if (IsUsingSmallHull())
		return;

	if ( bSmall && !m_bUsingSmallDoorBashHull )
	{
		m_bUsingSmallDoorBashHull = true;
		UTIL_SetSize(this, NAI_Hull::SmallMins(GetHullType()),NAI_Hull::SmallMaxs(GetHullType()));		
		if ( VPhysicsGetObject() )
		{
			SetupVPhysicsHull();
		}
	}
	else if (!bSmall && m_bUsingSmallDoorBashHull)
	{
		trace_t tr;
		Vector	vUpBit = GetAbsOrigin();
		vUpBit.z += 1;

		// check we have room to grow
		AI_TraceHull( GetAbsOrigin(), vUpBit, GetHullMins(),
			GetHullMaxs(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		if ( !tr.startsolid && (tr.fraction == 1.0) )
		{				
			m_bUsingSmallDoorBashHull = false;
			UTIL_SetSize(this, GetHullMins(),GetHullMaxs());		
			if ( VPhysicsGetObject() )
			{
				SetupVPhysicsHull();
			}
		}
	}
	
}

// a rough heuristic to see if our enemy is sitting behind a door
bool CASW_Drone_Advanced::IsBehindDoor(CBaseEntity *pOther)
{
	CASW_Door *pDoor = m_hBlockingDoor.Get();
	if (!pOther || !pDoor)
	{
		//Msg("no door or other in behind door test\n");
		return false;
	}

	Vector diff = pOther->GetAbsOrigin() - GetAbsOrigin();
	Vector doorDiff = pDoor->GetAbsOrigin() - GetAbsOrigin();

	// if they're nearer than the door, they're probably not behind it
	if (diff.LengthSqr() < doorDiff.LengthSqr())
	{
		//Msg("Other too near in behind door test\n");
		return false;
	}

	// if they're not in the direction of the door, they're probably not behind it
	VectorNormalize(diff);
	VectorNormalize(doorDiff);
	if (diff.Dot(doorDiff) <= 0)
	{
		//Msg("Other not in door direction in behind door test\n");
		return false;
	}

	// if we can see him, he's not behind a door
	trace_t tr;
	UTIL_TraceLine(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), MASK_NPCSOLID ,this, GetCollisionGroup(), &tr);
	if (tr.fraction >= 1.0f || tr.m_pEnt == GetEnemy())
		return false;

	return true;
}

// moves us back a bit from the door, so we're not attacking through it
void CASW_Drone_Advanced::MoveAwayFromDoor()
{
	CASW_Door *pDoor = m_hBlockingDoor.Get();
	if (!pDoor)
		return;
	
	Vector vecPerp;
	QAngle angFacing = pDoor->GetAbsAngles();
	angFacing[YAW] += 90;
	AngleVectors(angFacing, &vecPerp);
	float fDoorWidth = 75.0f;
	Vector vecDoorA = pDoor->GetAbsOrigin() + vecPerp * fDoorWidth;
	Vector vecDoorB = pDoor->GetAbsOrigin() - vecPerp * fDoorWidth;
	//NDebugOverlay::Line( vecDoorA, vecDoorB, 0, 255, 0, true, 2.0f );
	float dist = CalcDistanceToLine2D(GetAbsOrigin().AsVector2D(), vecDoorA.AsVector2D(), vecDoorB.AsVector2D());
	float move_amount = (asw_drone_door_distance_min.GetFloat()) - dist;
	Vector diff = GetAbsOrigin() - pDoor->GetAbsOrigin();
	angFacing[YAW] -=90;
	Vector vecForward;
	AngleVectors(angFacing, &vecForward);
	if (diff.Dot(vecForward) < 0)
	{
		move_amount *= -1;
	}
	vecForward *= move_amount;
	Msg("Moving alien by %f, %f, %f\n", vecForward.x, vecForward.y, vecForward.z);
	SetAbsOrigin(GetAbsOrigin() + vecForward);	// todo: check this isn't making us stuck in something?
}

float CASW_Drone_Advanced::LinearDistanceToDoor()
{
	CASW_Door *pDoor = m_hBlockingDoor.Get();
	if (!pDoor)
		return -1;
	
	Vector vecPerp;
	QAngle angFacing = pDoor->GetAbsAngles();
	angFacing[YAW] += 90;
	AngleVectors(angFacing, &vecPerp);
	float fDoorWidth = 75.0f;
	Vector vecDoorA = pDoor->GetAbsOrigin() + vecPerp * fDoorWidth;
	Vector vecDoorB = pDoor->GetAbsOrigin() - vecPerp * fDoorWidth;
	//NDebugOverlay::Line( vecDoorA, vecDoorB, 0, 255, 0, true, 2.0f );
	return CalcDistanceToLine2D(GetAbsOrigin().AsVector2D(), vecDoorA.AsVector2D(), vecDoorB.AsVector2D());
}

void CASW_Drone_Advanced::SetDoorBashYaw()
{
	CASW_Door *pDoor = m_hBlockingDoor.Get();
	if (!pDoor)
		return;
	
	// figure out which side of the door we're on
	Vector vecFacing;
	Vector vecDiff = GetAbsOrigin() - pDoor->GetAbsOrigin();
	AngleVectors(pDoor->GetAbsAngles(), &vecFacing);
	float dot = DotProduct(vecFacing, vecDiff);
	if (dot < 0)
	{
		m_flDoorBashYaw = pDoor->GetAbsAngles()[YAW];
	}
	else
	{
		m_flDoorBashYaw = anglemod(pDoor->GetAbsAngles()[YAW] + 180.0f);
	}
}

void CASW_Drone_Advanced::SetHealthByDifficultyLevel()
{
	int iHealth = MAX(25, ASWGameRules()->ModifyAlienHealthBySkillLevel(asw_drone_health.GetInt()));
	if (asw_debug_alien_damage.GetBool())
		Msg("Setting drone's initial health to %d\n", iHealth);
	SetHealth(iHealth);
	SetMaxHealth(iHealth);
	SetHitboxSet(0);
}

// if we arrive at our destination, clear our orders
bool CASW_Drone_Advanced::ShouldClearOrdersOnMovementComplete()
{
	// don't clear our orders if we're bashing a door while traveling a route
	if ( m_AlienOrders == AOT_MoveToIgnoringMarines && IsCurSchedule( SCHED_DRONE_BASH_DOOR ) )
	{
		return false;
	}

	return BaseClass::ShouldClearOrdersOnMovementComplete();
}

AI_BEGIN_CUSTOM_NPC( asw_drone_advanced, CASW_Drone_Advanced )
	DECLARE_CONDITION( COND_DRONE_BLOCKED_BY_DOOR )
	DECLARE_CONDITION( COND_DRONE_DOOR_OPENED )
	DECLARE_CONDITION( COND_DRONE_LOS )
	DECLARE_CONDITION( COND_DRONE_LOST_LOS )
	DECLARE_CONDITION( COND_DRONE_GAINED_LOS )
	DECLARE_TASK( TASK_DRONE_YAW_TO_DOOR )
	DECLARE_TASK( TASK_DRONE_ATTACK_DOOR )
	DECLARE_TASK( TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE )
	DECLARE_TASK( TASK_DRONE_GET_PATH_TO_DOOR )
	DECLARE_TASK( TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT )
	DECLARE_TASK( TASK_DRONE_DOOR_WAIT )
	DECLARE_TASK( TASK_DRONE_GET_CIRCLE_PATH )
	DECLARE_TASK( TASK_DRONE_WAIT_FACE_ENEMY )
	// Activities
	DECLARE_ACTIVITY( ACT_DRONE_RUN_ATTACKING )
	DECLARE_ACTIVITY( ACT_DRONE_WALLPOUND )

	// Events
	DECLARE_ANIMEVENT( AE_DRONE_WALK_FOOTSTEP )
	DECLARE_ANIMEVENT( AE_DRONE_FOOTSTEP_SOFT )
	DECLARE_ANIMEVENT( AE_DRONE_FOOTSTEP_HEAVY )
	DECLARE_ANIMEVENT( AE_DRONE_MELEE_HIT1 )
	DECLARE_ANIMEVENT( AE_DRONE_MELEE_HIT2 )	
	DECLARE_ANIMEVENT( AE_DRONE_MELEE1_SOUND )
	DECLARE_ANIMEVENT( AE_DRONE_MELEE2_SOUND )
	DECLARE_ANIMEVENT( AE_DRONE_MOUTH_BLEED )
	DECLARE_ANIMEVENT( AE_DRONE_ALERT_SOUND )
	DECLARE_ANIMEVENT( AE_DRONE_SHADOW_ON )
	DEFINE_SCHEDULE
	( 
		SCHED_DRONE_BASH_DOOR,

		"	Tasks"
		//"		TASK_SET_ACTIVITY				ACTIVITY:ACT_ALIEN_SHOVER_ROAR"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_DRONE_DOOR_WAIT"
		"		TASK_DRONE_GET_PATH_TO_DOOR		0"
		"		TASK_RUN_PATH					0"
		"		TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT		0"
		"		TASK_DRONE_YAW_TO_DOOR			0"
		"		TASK_FACE_IDEAL					0"
		"		TASK_STOP_MOVING				1"
		"		TASK_DRONE_ATTACK_DOOR			0"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_DRONE_DOOR_OPENED"
	)

	// attack a door we're already near to
	DEFINE_SCHEDULE
	( 
		SCHED_DRONE_BASH_CLOSE_DOOR,

		"	Tasks"		
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_DRONE_DOOR_WAIT"		
		"		TASK_DRONE_YAW_TO_DOOR			0"
		"		TASK_FACE_IDEAL					0"
		"		TASK_STOP_MOVING				1"
		"		TASK_DRONE_ATTACK_DOOR			0"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_DRONE_DOOR_OPENED"
	)

	// drone waiting for a door to be bashed open by some other aliens
	DEFINE_SCHEDULE
	( 
		SCHED_DRONE_DOOR_WAIT,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
		"		TASK_DRONE_DOOR_WAIT			1"
		""
		"	Interrupts"
		"		COND_DRONE_DOOR_OPENED"
	)


	DEFINE_SCHEDULE
	(
		SCHED_DRONE_CHASE_ENEMY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		//"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	300"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		//"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_ALIEN_SHOVER_PHYSICS_TARGET"
		"		COND_DRONE_BLOCKED_BY_DOOR"
		"		COND_DRONE_GAINED_LOS"
		"		COND_HEAVY_DAMAGE"
		//"		COND_DRONE_LOS"		// this is needed for override move, but currently seems to be bugged and always on, causing this schedule to fail constantly
	)
	DEFINE_SCHEDULE
	(
		SCHED_DRONE_CIRCLE_ENEMY,	

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_DRONE_CIRCLE_ENEMY_FAILED"
		//"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_DRONE_GET_CIRCLE_PATH	300"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		//"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_ALIEN_SHOVER_PHYSICS_TARGET"
		"		COND_DRONE_BLOCKED_BY_DOOR"
		"		COND_DRONE_GAINED_LOS"
		"		COND_HEAVY_DAMAGE"
	)
	DEFINE_SCHEDULE
	(
		SCHED_DRONE_CIRCLE_ENEMY_FAILED,	
		// much like the default standoff schedule, but the AI is more agressive at starting chasing again
		// the wait is also much shorter, so the drone will try to circle to a new position fairly quickly
		// reattempts circling once this schedule is done
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"		// Translated to cover
		"		TASK_DRONE_WAIT_FACE_ENEMY	0.4"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_DRONE_CIRCLE_ENEMY"
		""
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_HEAR_DANGER"
		"		COND_HEAVY_DAMAGE"
	)
	DEFINE_SCHEDULE
	(
		SCHED_DRONE_STANDOFF,	
		// much like the default standoff schedule, but the AI is more agressive at starting chasing again
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"		// Translated to cover
		"		TASK_DRONE_WAIT_FACE_ENEMY	2"
		""
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_HEAR_DANGER"
		"		COND_HEAVY_DAMAGE"
	)
	DEFINE_SCHEDULE
	(
		SCHED_DRONE_OVERRIDE_MOVE,	

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_DRONE_CHASE_ENEMY"
		"		 TASK_RUN_PATH					0"
		"		 TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE			0"		
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TASK_FAILED"
		"		COND_ALIEN_SHOVER_PHYSICS_TARGET"
		"		COND_DRONE_BLOCKED_BY_DOOR"
		"		COND_DRONE_LOST_LOS"
		"		COND_HEAVY_DAMAGE"
	)

AI_END_CUSTOM_NPC()