//=============================================================================//
//
// Purpose:		Crab Synth NPC created from scratch in Source 2013 as a new type
//				of Combine armored unit, filling a role somewhere between a standard
//				soldier and a strider.
// 
//				--------------------------------------------------------------
// 
//				This Crab Synth NPC can move while shooting, aim its gun in any
//				direction, launch grenades from its exhaust port, and charge at
//				enemies. It can kind of be described as a giant version of the
//				standard Combine soldier.
// 
//				This uses a blend of code from Combine soldiers and antlion guards
//				for tactical and charging abilities respectively, as well as the
//				bone follower collision code used by striders.
// 
//				While the original cut NPC was impervious to bullets and needed
//				to be destroyed by grenades from below, this version of the NPC
//				is only heavily armored. Damage taken from below is still boosted
//				and is usually the best way to attack this NPC, but it is no longer
//				the only way for it to take damage.
// 
//				Inputs and keyvalues from soldiers and antlion guards for
//				controlling their respective AI functions (i.e. grenades or
//				charging) have been ported to this NPC as well.
// 
//				--------------------------------------------------------------
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_crabsynth.h"
#include "ammodef.h"
#include "npcevent.h"
#include "ai_squad.h"
#include "ai_moveprobe.h"
#include "player_pickup.h"
#include "props.h"
#include "particle_parse.h"
#include "te_effect_dispatch.h"
#include "prop_combine_ball.h"
#include "IEffects.h"
#include "basegrenade_shared.h"
#include "SpriteTrail.h"

ConVar	sk_crabsynth_health( "sk_crabsynth_health", "900" );
ConVar	sk_crabsynth_dmgscale_bullet( "sk_crabsynth_dmgscale_bullet", "0.15" );
ConVar	sk_crabsynth_dmgscale_buckshot( "sk_crabsynth_dmgscale_buckshot", "0.3" );
ConVar	sk_crabsynth_dmgscale_below( "sk_crabsynth_dmgscale_below", "2.0" );
ConVar	sk_crabsynth_combineball_damage( "sk_crabsynth_combineball_damage", "150" );
ConVar	sk_crabsynth_hitgroup_head( "sk_crabsynth_hitgroup_head", "1" );
ConVar	sk_crabsynth_hitgroup_chest( "sk_crabsynth_hitgroup_chest", "1" );
ConVar	sk_crabsynth_hitgroup_stomach( "sk_crabsynth_hitgroup_stomach", "1" );
ConVar	sk_crabsynth_hitgroup_arm( "sk_crabsynth_hitgroup_arm", "0.5" );
ConVar	sk_crabsynth_hitgroup_leg( "sk_crabsynth_hitgroup_leg", "0.8" );
ConVar	sk_crabsynth_dmg_gun( "sk_crabsynth_dmg_gun", "8" );
ConVar	sk_crabsynth_dmg_charge( "sk_crabsynth_dmg_charge", "80" );
ConVar	sk_crabsynth_dmg_claw_strike( "sk_crabsynth_dmg_claw_strike", "40" );
ConVar	sk_crabsynth_dmg_claw_swipe( "sk_crabsynth_dmg_claw_swipe", "30" );
ConVar	sk_crabsynth_gun_min_range( "sk_crabsynth_gun_min_range", "32" );
ConVar	sk_crabsynth_gun_max_range( "sk_crabsynth_gun_max_range", "4096" );

ConVar	g_debug_crabsynth( "g_debug_crabsynth", "0" );

#define CRABSYNTH_FIRE_RATE 0.1
#define CRABSYNTH_FIRE_BURST_MIN 5
#define CRABSYNTH_FIRE_BURST_MAX 8
#define CRABSYNTH_FIRE_REST_MIN 0.3
#define CRABSYNTH_FIRE_REST_MAX 0.4

#define CRABSYNTH_BARREL_SPIN_APPROACH 24.0f
#define CRABSYNTH_BARREL_SPIN_INCREASE 36.0f
#define CRABSYNTH_BARREL_DECAY_RATE 0.04f

// Crab synth will try to keep combat within this range and height
#define CRABSYNTH_ATTACK_DIST_MIN 256.0f
#define CRABSYNTH_ATTACK_DIST_MAX 2048.0f
#define CRABSYNTH_ATTACK_PITCH_MIN -15.0f
#define CRABSYNTH_ATTACK_PITCH_MAX 35.0f

#define	CRABSYNTH_CHARGE_MIN 256.0f
#define	CRABSYNTH_CHARGE_MAX 2048.0f
#define	CRABSYNTH_CHARGE_MAX_HEIGHT_DIFF 64.0f

#define CRABSYNTH_GRENADE_TIMER	3.5
#define CRABSYNTH_GRENADE_THROW_SPEED 1250
#define CRABSYNTH_GRENADE_THROW_SPEED_DIST_VARIANCE_MIN 500
#define CRABSYNTH_GRENADE_THROW_SPEED_DIST_VARIANCE_MAX 1500

// Think contexts
static const char *CRABSYNTH_BLEED_THINK = "CrabSynthBleed";

void ApplyCrabSynthChargeDamage( CBaseEntity *pCrabSynth, CBaseEntity *pTarget, float flDamage );

// Due to the crab synth's unique shape and size, we must use bone follower collisions like what striders use
static const char *pFollowerBoneNames[] =
{
	// Body
	"Bip02 Spine1",
	"Bip02 Tail",

	// No limbs for now
	/*
	// Back legs
	"Bip02 L Thigh",
	"Bip02 L Calf",
	"Bip02 L Horselink",
	"Bip02 R Thigh",
	"Bip02 R Calf",
	"Bip02 R Horselink",
	
	// Front legs/arms
	"Bip02 L UpperArm",
	"Bip02 L Forearm",
	"Bip02 R UpperArm",
	"Bip02 R Forearm",
	*/
};

//=========================================================
// Anim Events	
//=========================================================
int AE_CRABSYNTH_SHOOT;
int AE_CRABSYNTH_LAUNCH_GRENADE;
int AE_CRABSYNTH_CLAW_CRUSH;
int AE_CRABSYNTH_CLAW_SWIPE;
int AE_CRABSYNTH_CLAW_STEP_LEFT;
int AE_CRABSYNTH_CLAW_STEP_RIGHT;
int AE_CRABSYNTH_CHARGE_START;
int AE_CRABSYNTH_LAND;

//=========================================================
// Crab Synth activities
//=========================================================
Activity ACT_CRABSYNTH_CHARGE_START;
Activity ACT_CRABSYNTH_CHARGE_STOP;
Activity ACT_CRABSYNTH_CHARGE;
Activity ACT_CRABSYNTH_CHARGE_ANTICIPATION;
Activity ACT_CRABSYNTH_CHARGE_HIT;
Activity ACT_CRABSYNTH_CHARGE_CRASH;
Activity ACT_CRABSYNTH_FRUSTRATED;
Activity ACT_CRABSYNTH_LOOK_AROUND;

//=========================================================
// Crab Synth squad slots
//=========================================================
enum SquadSlot_T
{	
	SQUAD_SLOT_ATTACK_OCCLUDER = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_GRENADE1,
};

//-----------------------------------------------------------------------------
// Purpose: Class Constructor
//-----------------------------------------------------------------------------
CNPC_CrabSynth::CNPC_CrabSynth( void )
{
	m_iNumGrenades = 5;
	m_bProjectedLightEnabled = true;
	m_bProjectedLightShadowsEnabled = false;
	m_flProjectedLightBrightnessScale = 1.0f;
	m_flProjectedLightFOV = 90.0f;
	m_flProjectedLightHorzFOV = 120.0f;
}

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_crabsynth, CNPC_CrabSynth );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_CrabSynth )

	DEFINE_FIELD( m_nShots, FIELD_INTEGER ),
	DEFINE_FIELD( m_flStopMoveShootTime, FIELD_TIME ),
	DEFINE_FIELD( m_flBarrelSpinSpeed, FIELD_FLOAT ),

	DEFINE_FIELD( m_bShouldPatrol, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hObstructor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTimeSinceObstructed, FIELD_TIME ),

	DEFINE_FIELD( m_flNextRoarTime, FIELD_TIME ),

	DEFINE_FIELD( m_flChargeTime, FIELD_TIME ),
	DEFINE_FIELD( m_hChargeTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hChargeTargetPosition, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hOldTarget, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bIsBleeding, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLastDamageBelow, FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_bProjectedLightEnabled, FIELD_BOOLEAN, "ProjectedLightEnabled" ),
	DEFINE_KEYFIELD( m_bProjectedLightShadowsEnabled, FIELD_BOOLEAN, "ProjectedLightShadowsEnabled" ),
	DEFINE_KEYFIELD( m_flProjectedLightBrightnessScale, FIELD_FLOAT, "ProjectedLightBrightnessScale" ),
	DEFINE_KEYFIELD( m_flProjectedLightFOV, FIELD_FLOAT, "ProjectedLightFOV" ),
	DEFINE_KEYFIELD( m_flProjectedLightHorzFOV, FIELD_FLOAT, "ProjectedLightHorzFOV" ),

	DEFINE_KEYFIELD( m_bDisableBoneFollowers, FIELD_BOOLEAN, "disablephysics" ),
	DEFINE_EMBEDDED( m_BoneFollowerManager ),

	DEFINE_THINKFUNC( BleedThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "StartPatrolling", InputStartPatrolling ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopPatrolling", InputStopPatrolling ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetChargeTarget", InputSetChargeTarget ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClearChargeTarget", InputClearChargeTarget ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnProjectedLights", InputTurnOnProjectedLights ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffProjectedLights", InputTurnOffProjectedLights ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetProjectedLightEnableShadows", InputSetProjectedLightEnableShadows ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetProjectedLightBrightnessScale", InputSetProjectedLightBrightnessScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetProjectedLightFOV", InputSetProjectedLightFOV ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetProjectedLightHorzFOV", InputSetProjectedLightHorzFOV ),

	DEFINE_AIGRENADE_DATADESC()

END_DATADESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_CrabSynth, DT_NPC_CrabSynth )
	SendPropBool( SENDINFO( m_bProjectedLightEnabled ) ),
	SendPropBool( SENDINFO( m_bProjectedLightShadowsEnabled ) ),
	SendPropFloat( SENDINFO( m_flProjectedLightBrightnessScale ) ),
	SendPropFloat( SENDINFO( m_flProjectedLightFOV ) ),
	SendPropFloat( SENDINFO( m_flProjectedLightHorzFOV ) ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::Precache( void )
{
	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/crabsynth_npc.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	PrecacheScriptSound( "NPC_CrabSynth.ClawStepLeft" );
	PrecacheScriptSound( "NPC_CrabSynth.ClawStepRight" );
	PrecacheScriptSound( "NPC_CrabSynth.Melee_Crush" );
	PrecacheScriptSound( "NPC_CrabSynth.Melee_Crush_Miss" );
	PrecacheScriptSound( "NPC_CrabSynth.Melee_Swipe" );
	PrecacheScriptSound( "NPC_CrabSynth.Melee_Swipe_Miss" );
	PrecacheScriptSound( "NPC_CrabSynth.Land" );
	PrecacheScriptSound( "NPC_CrabSynth.Shoot" );
	PrecacheScriptSound( "NPC_CrabSynth.GrenadeLaunch" );
	PrecacheScriptSound( "NPC_CrabSynth.Shove" );
	PrecacheScriptSound( "NPC_CrabSynth.ChargeHitWorld" );
	PrecacheScriptSound( "NPC_CrabSynth.Idle" );
	PrecacheScriptSound( "NPC_CrabSynth.Alert" );
	PrecacheScriptSound( "NPC_CrabSynth.Charge" );
	PrecacheScriptSound( "NPC_CrabSynth.Danger" );
	PrecacheScriptSound( "NPC_CrabSynth.Pain" );
	PrecacheScriptSound( "NPC_CrabSynth.Death" );

	PrecacheParticleSystem( "crabsynth_muzzle_flash" );
	PrecacheParticleSystem( "crabsynth_fire_grenade" );

	PrecacheParticleSystem( "blood_impact_synth_01" );
	PrecacheParticleSystem( "blood_impact_synth_01_arc_parent" );
	PrecacheParticleSystem( "blood_spurt_synth_01" );
	PrecacheParticleSystem( "blood_drip_synth_01" );

	UTIL_PrecacheOther( "npc_crab_grenade" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	SetHealth( sk_crabsynth_health.GetFloat() );
	SetMaxHealth( sk_crabsynth_health.GetFloat() );

	BaseClass::Spawn();

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	SetHullType( HULL_LARGE );
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_STEP );
	
	SetBloodColor( DONT_BLEED );

	m_flFieldOfView = 0.2f; // 144 degrees
	m_flNextGrenadeCheck = gpGlobals->curtime + 1;

	// Ironically, having the "correct" gun position causes problems because enemies can get too close to the crab synth for it to aim properly.
	// As a result, we're currently using a point that's *behind* the gun, under the crab synth's belly.
	m_HackedGunPos = Vector( 0.0f, 0.0f, 18.0f ); // Vector( 0.0f, 64.0f, 18.0f );
	SetViewOffset( EyeOffset( ACT_IDLE ) );

	CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_AIM_GUN | bits_CAP_MOVE_SHOOT | bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP );
	CapabilitiesAdd( bits_CAP_FRIENDLY_DMG_IMMUNE );
	CapabilitiesAdd( bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK2 );

	CollisionProp()->SetSurroundingBoundsType( USE_HITBOXES );

	NPCInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::Activate( void )
{
	m_nAttachmentMuzzle = LookupAttachment( "muzzle" );
	m_nAttachmentHead = LookupAttachment( "anim_attachment_head" );
	m_nAttachmentTopHole = LookupAttachment( "top_hole" );
	m_nAttachmentPreLaunchPoint = LookupAttachment( "pre_launch_point" );
	m_nAttachmentBelly = LookupAttachment( "belly" );
	m_nAttachmentMuzzleBehind = LookupAttachment( "muzzle_behind" );
	m_iAmmoType = GetAmmoDef()->Index( "AR2" );

	BaseClass::Activate();
	
	if ( m_bIsBleeding )
	{
		StartBleeding();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::PopulatePoseParameters( void )
{
	m_nPoseBarrelSpin = LookupPoseParameter( "barrel_spin" );

	BaseClass::PopulatePoseParameters();

	m_poseAim_Yaw = LookupPoseParameter( "head_yaw" );
	m_poseAim_Pitch = LookupPoseParameter( "head_pitch" );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CNPC_CrabSynth::CreateVPhysics()
{
	if ( !m_bDisableBoneFollowers )
	{
		InitBoneFollowers();
	}
	else
	{
		return BaseClass::CreateVPhysics();
	}

	return true;
}

//---------------------------------------------------------
//---------------------------------------------------------
inline float CNPC_CrabSynth::GetMass()
{
	// HACKHACK: We don't have a VPhysics object when we have bone followers, so get the first bone follower's mass
	physfollower_t *pFollower = m_BoneFollowerManager.GetBoneFollower( 0 );
	if ( !pFollower )
		return BaseClass::GetMass();

	return pFollower->hFollower->VPhysicsGetObject()->GetMass();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::InitBoneFollowers( void )
{
	// Don't do this if we're already loaded
	if ( m_BoneFollowerManager.GetNumBoneFollowers() != 0 )
		return;

	// Init our followers
	m_BoneFollowerManager.InitBoneFollowers( this, ARRAYSIZE(pFollowerBoneNames), pFollowerBoneNames );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::FInViewCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - EyePosition() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = EyeDirection2D( );

	float flDot = DotProduct( los, facingDir );

	if ( flDot > m_flFieldOfView )
	{
		return true;
	}
	else if ( m_NPCState == NPC_STATE_COMBAT /*&& vecSpot.z < EyePosition().z*/ )
	{
		// Try using our gun as the direction in case the target is under our head
		// We use a "muzzle_behind" attachment instead of the muzzle directly so that enemies too close to the gun can still be seen
		// (Note that this has the side effect of increasing the crab synth's overall field of view during combat, which is okay)
		matrix3x4_t matMuzzleBehindToWorld;
		GetAttachment( m_nAttachmentMuzzleBehind, matMuzzleBehindToWorld );
		MatrixGetColumn( matMuzzleBehindToWorld, 0, facingDir );

		flDot = DotProduct( los, facingDir );
		if ( flDot > m_flFieldOfView )
			return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Purpose: Checks if a position is below our belly and can hurt us more
//------------------------------------------------------------------------------
inline bool CNPC_CrabSynth::IsBelowBelly( const Vector &vecPos )
{
	Vector vecBelly;
	GetAttachment( m_nAttachmentBelly, vecBelly );
	if ( vecPos.z < vecBelly.z && ( vecBelly - vecPos ).Length() <= CollisionProp()->BoundingRadius() )
	{
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
int CNPC_CrabSynth::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// Bleed at 30% health, like hunters
	if ( !m_bIsBleeding && ( GetHealth() <= (GetMaxHealth() * 0.3) ) )
	{
		StartBleeding();
	}

	// Vulnerable when attacked from below
	int nDmgType = info.GetDamageType();
	if ( !(nDmgType & ( DMG_POISON | DMG_DIRECT | DMG_FALL | DMG_NERVEGAS | DMG_RADIATION | DMG_DROWN )) )
	{
		// Bullet damage types have their own check in TraceAttack()
		if ( !(nDmgType & ( DMG_BULLET | DMG_BUCKSHOT | DMG_CLUB | DMG_NEVERGIB )) )
		{
			// Only count if it's below our "belly" attachment point
			m_bLastDamageBelow = IsBelowBelly( info.GetDamagePosition() );

			if (g_debug_crabsynth.GetBool() && m_debugOverlays & OVERLAY_NPC_SELECTED_BIT)
			{
				Vector vecBelly;
				GetAttachment( m_nAttachmentBelly, vecBelly );

				NDebugOverlay::Circle( vecBelly, QAngle( -90, 0, 0 ), CollisionProp()->BoundingRadius(), 255, 0, 0, 255, true, 3.0f);

				if (m_bLastDamageBelow)
					NDebugOverlay::Cross3D( info.GetDamagePosition(), 3.0f, 0, 255, 0, true, 3.0f );
				else
					NDebugOverlay::Cross3D( info.GetDamagePosition(), 3.0f, 255, 0, 0, true, 3.0f );
			}
		}
	}

	// Combine balls stun crab synths, similar to striders.
	// We check for the other collision group because UTIL_IsCombineBall doesn't count NPC balls.
	// TODO: Fix this in Mapbase?
	if ( info.GetInflictor() && UTIL_IsCombineBall( info.GetInflictor() ) || info.GetInflictor()->GetCollisionGroup() == HL2COLLISION_GROUP_COMBINE_BALL_NPC )
	{
		float damage = sk_crabsynth_combineball_damage.GetFloat();

		if( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
		{
			// Route combine ball damage through the regular skill level code.
			damage = g_pGameRules->AdjustPlayerDamageInflicted(damage);
		}

		if ( m_bLastDamageBelow )
		{
			damage *= sk_crabsynth_dmgscale_below.GetFloat();
		}

		AddFacingTarget( info.GetInflictor(), info.GetInflictor()->GetAbsOrigin(), 0.5, 2.0 );
		PainSound( info );

		int nGesture = AddGesture( ACT_GESTURE_BIG_FLINCH );

		// Stop firing while flinching
		m_nShots = 0;
		GetShotRegulator()->SetBurstShotsRemaining( 0 );
		GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + GetLayerDuration(nGesture) );

		info.GetInflictor()->AcceptInput( "Explode", this, this, variant_t(), 0 );
		m_iHealth -= damage;
		return damage;
	}
	else
	{
		CTakeDamageInfo subInfo = info;
		if ( m_bLastDamageBelow && LastHitGroup() != HITGROUP_GEAR )
		{
			// HACKHACK: Crossbow bolt damage too high to be scaled
			if (!info.GetInflictor() || !info.GetInflictor()->ClassMatches("crossbow_bolt"))
				subInfo.ScaleDamage( sk_crabsynth_dmgscale_below.GetFloat() );
		}

		return BaseClass::OnTakeDamage_Alive( subInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	CTakeDamageInfo subInfo = info;

	// Crab synths bleed their own blood and scale down certain types of damage
	int nDmgType = info.GetDamageType();
	//if ( nDmgType & ( DMG_BULLET | DMG_BUCKSHOT | DMG_CLUB | DMG_NEVERGIB ) )
	{
		if (nDmgType & DMG_BULLET)
		{
			subInfo.ScaleDamage( sk_crabsynth_dmgscale_bullet.GetFloat() );
		}
		else if (nDmgType & DMG_BUCKSHOT)
		{
			subInfo.ScaleDamage( sk_crabsynth_dmgscale_buckshot.GetFloat() );
		}

		// Vulnerable when attacked from below
		m_bLastDamageBelow = IsBelowBelly( ptr->endpos );
		if ( (ptr->hitgroup == HITGROUP_HEAD || ptr->hitgroup == HITGROUP_STOMACH) && m_bLastDamageBelow )
		{
			// HACKHACK: Crossbow bolt damage too high to be scaled
			if (!info.GetInflictor() || !info.GetInflictor()->ClassMatches( "crossbow_bolt" ))
				subInfo.ScaleDamage( sk_crabsynth_dmgscale_below.GetFloat() );

			QAngle vecAngles;
			VectorAngles( ptr->plane.normal, vecAngles );
			DispatchParticleEffect( "blood_impact_synth_01", ptr->endpos, vecAngles );
			DispatchParticleEffect( "blood_impact_synth_01_arc_parent", PATTACH_POINT_FOLLOW, this, m_nAttachmentHead );
		}
		else
		{
			// Use sparks when attacked from anywhere else
			g_pEffects->Sparks( ptr->endpos );
			if (random->RandomFloat( 0, 2 ) >= 1)
			{
				UTIL_Smoke( ptr->endpos, random->RandomInt( 10, 15 ), 10 );
			}
		}

		if (g_debug_crabsynth.GetBool() && m_debugOverlays & OVERLAY_NPC_SELECTED_BIT)
		{
			Vector vecBelly;
			GetAttachment( m_nAttachmentBelly, vecBelly );

			NDebugOverlay::Circle( vecBelly, QAngle( -90, 0, 0 ), CollisionProp()->BoundingRadius(), 255, 0, 0, 255, true, 3.0f );

			if (m_bLastDamageBelow)
				NDebugOverlay::Cross3D( info.GetDamagePosition(), 3.0f, 0, 255, 0, true, 3.0f );
			else
				NDebugOverlay::Cross3D( info.GetDamagePosition(), 3.0f, 255, 0, 0, true, 3.0f );
		}
	}

	BaseClass::TraceAttack( subInfo, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Heavy damage tolerance becomes lower and lower
	return ( info.GetDamage() > ( 20.0f * ( (float)GetHealth() / (float)GetMaxHealth() ) ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::StartBleeding()
{
	m_bIsBleeding = true;

	DispatchParticleEffect( "blood_drip_synth_01", PATTACH_POINT_FOLLOW, this, m_nAttachmentHead );

	// Emit spurts of our blood
	SetContextThink( &CNPC_CrabSynth::BleedThink, gpGlobals->curtime + 0.1, CRABSYNTH_BLEED_THINK );
}

//-----------------------------------------------------------------------------
// Our health is low. Show damage effects.
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::BleedThink()
{
	// Spurt blood from random points on the synth's head.
	Vector vecOrigin;
	QAngle angDir;
	GetAttachment( m_nAttachmentHead, vecOrigin, angDir );
	
	Vector vecDir = RandomVector( -1, 1 );
	VectorNormalize( vecDir );
	VectorAngles( vecDir, Vector( 0, 0, 1 ), angDir );

	vecDir *= 32.0f;
	DispatchParticleEffect( "blood_spurt_synth_01", vecOrigin + vecDir, angDir );

	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.6, 1.5 ), CRABSYNTH_BLEED_THINK );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_CrabSynth::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
	case HITGROUP_GENERIC:
		return 1.0f;

	case HITGROUP_HEAD:
		return sk_crabsynth_hitgroup_head.GetFloat();

	case HITGROUP_CHEST:
		return sk_crabsynth_hitgroup_chest.GetFloat();

	case HITGROUP_STOMACH:
		return sk_crabsynth_hitgroup_stomach.GetFloat();

	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		return sk_crabsynth_hitgroup_arm.GetFloat();

	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		return sk_crabsynth_hitgroup_leg.GetFloat();

	default:
		return 1.0f;
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::CanFlinch( void )
{
	// Don't flinch in response to damage that isn't from below unless we're bleeding
	if ( !m_bLastDamageBelow && !m_bIsBleeding )
		return false;

	return BaseClass::CanFlinch();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::Event_Killed( const CTakeDamageInfo &info )
{
	// Stop all our thinks
	SetContextThink( NULL, 0, CRABSYNTH_BLEED_THINK );

	StopParticleEffects( this );

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: Called just before we are deleted.
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::UpdateOnRemove( void )
{
	m_BoneFollowerManager.DestroyBoneFollowers();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Return the pointer for a given sprite given an index
//-----------------------------------------------------------------------------
CSprite	*CNPC_CrabSynth::GetGlowSpritePtr( int index )
{
	switch (index)
	{
		case 0:
			return m_pEyeGlow;
		case 1:
			return m_pEyeGlow2;
		case 2:
			return m_pEyeGlow3;
		case 3:
			return m_pEyeGlow4;
	}

	return BaseClass::GetGlowSpritePtr( index );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the glow sprite at the given index
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::SetGlowSpritePtr( int index, CSprite *sprite )
{
	switch (index)
	{
		case 0:
			m_pEyeGlow = sprite;
			break;
		case 1:
			m_pEyeGlow2 = sprite;
			break;
		case 2:
			m_pEyeGlow3 = sprite;
			break;
		case 3:
			m_pEyeGlow4 = sprite;
			break;
	}

	BaseClass::SetGlowSpritePtr( index, sprite );
}

//-----------------------------------------------------------------------------
// Purpose: Return the glow attributes for a given index
//-----------------------------------------------------------------------------
EyeGlow_t *CNPC_CrabSynth::GetEyeGlowData( int index )
{
	EyeGlow_t * eyeGlow = new EyeGlow_t();

	eyeGlow->spriteName = MAKE_STRING( "sprites/light_glow01.vmt" );

	switch (index){
	case 0:
		eyeGlow->attachment = MAKE_STRING( "eye1" );
		break;
	case 1:
		eyeGlow->attachment = MAKE_STRING( "eye2" );
		break;
	case 2:
		eyeGlow->attachment = MAKE_STRING( "eye3" );
		break;
	case 3:
		eyeGlow->attachment = MAKE_STRING( "eye4" );
		break;
	}

	eyeGlow->alpha = 255;
	eyeGlow->brightness = 224;
	eyeGlow->red = 255;
	eyeGlow->green = 255;
	eyeGlow->blue = 255;

	eyeGlow->renderMode = kRenderWorldGlow;
	eyeGlow->scale = 0.3f;
	eyeGlow->proxyScale = 2.0f;

	return eyeGlow;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::IdleSound( void )
{
	EmitSound( "NPC_CrabSynth.Idle" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::AlertSound( void )
{
	EmitSound( "NPC_CrabSynth.Alert" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::DangerSound( void )
{
	EmitSound( "NPC_CrabSynth.Danger" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::GrenadeWarnSound( void )
{
	EmitSound( "NPC_CrabSynth.GrenadeWarn" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::PainSound( const CTakeDamageInfo &info )
{
	if (LastHitGroup() != HITGROUP_GEAR && (IsHeavyDamage( info ) || m_bLastDamageBelow))
	{
		EmitSound( "NPC_CrabSynth.Pain" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_CrabSynth.Death" );
}

//-----------------------------------------------------------------------------
// Purpose: Fire!
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy, bool bStrict )
{
	FireBulletsInfo_t info;

	if ( !bStrict && GetEnemy() != NULL )
	{
		Vector vecDir = GetActualShootTrajectory( vecSrc );

		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDir;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = VECTOR_CONE_PRECALCULATED;
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
		info.m_flDamage = sk_crabsynth_dmg_gun.GetFloat();
	}
	else
	{
		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDirToEnemy;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = GetAttackSpread( NULL, GetEnemy() );
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
		info.m_flDamage = sk_crabsynth_dmg_gun.GetFloat();
	}

	FireBullets( info );
	EmitSound( "NPC_CrabSynth.Shoot" );
	DoMuzzleFlash();

	// Increase barrel spin speed
	m_flBarrelSpinSpeed += CRABSYNTH_BARREL_SPIN_INCREASE;
}

void CNPC_CrabSynth::UpdateMuzzleMatrix()
{
	if (gpGlobals->tickcount != m_muzzleToWorldTick)
	{
		m_muzzleToWorldTick = gpGlobals->tickcount;
		GetAttachment( m_nAttachmentMuzzle, m_muzzleToWorld );
	}
}

void CNPC_CrabSynth::DoMuzzleFlash()
{
	BaseClass::DoMuzzleFlash();
	
	DispatchParticleEffect( "crabsynth_muzzle_flash", PATTACH_POINT_FOLLOW, this, m_nAttachmentMuzzle );

	// Dispatch the elight	
	CEffectData data;
	data.m_nAttachmentIndex = m_nAttachmentMuzzle;
	data.m_nEntIndex = entindex();
	DispatchEffect( "HunterMuzzleFlash", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::InputStartPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::InputStopPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::InputSetChargeTarget( inputdata_t &inputdata )
{
	if ( !IsAlive() )
		return;

	// Pull the target & position out of the string
	char parseString[255];
	Q_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// Get charge target name
	char *pszParam = strtok(parseString," ");
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, pszParam, NULL, inputdata.pActivator, inputdata.pCaller );
	if ( !pTarget )
	{
		Warning( "ERROR: Crab synth %s cannot find charge target '%s'\n", STRING(GetEntityName()), pszParam );
		return;
	}

	// Get the charge position name
	pszParam = strtok(NULL," ");
	CBaseEntity *pPosition = gEntList.FindEntityByName( NULL, pszParam, NULL, inputdata.pActivator, inputdata.pCaller );
	if ( !pPosition )
	{
		Warning( "ERROR: Crab synth %s cannot find charge position '%s'\nMake sure you've specified the parameters as [target start]!\n", STRING(GetEntityName()), pszParam );
		return;
	}

	// Make sure we don't stack charge targets
	if ( m_hChargeTarget )
	{
		if ( GetEnemy() == m_hChargeTarget )
		{
			SetEnemy( NULL );
		}
	}

	SetCondition( COND_CRABSYNTH_HAS_CHARGE_TARGET );
	m_hChargeTarget = pTarget;
	m_hChargeTargetPosition = pPosition;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::InputClearChargeTarget( inputdata_t &inputdata )
{
	m_hChargeTarget = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::TranslateSchedule( int scheduleType )
{
	switch ( scheduleType )
	{
		case SCHED_RANGE_ATTACK1:
			{
				if (GetEnemy())
				{
					Vector vecDelta = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();

					if (HasStrategySlot(SQUAD_SLOT_ATTACK1))
					{
						// If we're the main attacker, back away or get closer depending on enemy distance
						float flDistSq = vecDelta.LengthSqr();
						if ( flDistSq < Square( CRABSYNTH_ATTACK_DIST_MIN ) )
						{
							return SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY;
						}
						else if ( flDistSq > Square( CRABSYNTH_ATTACK_DIST_MAX ) )
						{
							// Only press attack if we can reach them
							if (HasCondition( COND_ENEMY_UNREACHABLE ))
								return SCHED_ESTABLISH_LINE_OF_FIRE;

							return SCHED_CRABSYNTH_PRESS_ATTACK;
						}
					}

					// Make sure we can actually aim as low/high as we need to
					VectorNormalize( vecDelta );
					float flPitch = UTIL_VecToPitch( vecDelta );					
					if ( flPitch < CRABSYNTH_ATTACK_PITCH_MIN || flPitch > CRABSYNTH_ATTACK_PITCH_MAX )
					{
						return SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY;
					}
				}

				return SCHED_CRABSYNTH_RANGE_ATTACK1;
			}

		case SCHED_RANGE_ATTACK2:
			return SCHED_CRABSYNTH_RANGE_ATTACK2;

		case SCHED_MELEE_ATTACK1:
			return SCHED_CRABSYNTH_MELEE_ATTACK1;

		case SCHED_TAKE_COVER_FROM_ENEMY:
			{
				if ( m_pSquad )
				{
					// Have to explicitly check innate range attack condition as may have weapon with range attack 2
					if (	g_pGameRules->IsSkillLevel( SKILL_HARD )	&& 
						HasCondition(COND_CAN_RANGE_ATTACK2)		&&
						OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
					{
						GrenadeWarnSound();
						return SCHED_CRABSYNTH_TOSS_GRENADE_COVER1;
					}
					else
					{
						return SCHED_CRABSYNTH_TAKE_COVER1;
					}
				}
				else
				{
					// Have to explicitly check innate range attack condition as may have weapon with range attack 2
					if ( random->RandomInt(0,1) && HasCondition(COND_CAN_RANGE_ATTACK2) )
					{
						return SCHED_CRABSYNTH_GRENADE_COVER1;
					}
					else
					{
						return SCHED_CRABSYNTH_TAKE_COVER1;
					}
				}

				break;
			}

		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return SCHED_CRABSYNTH_TAKE_COVER_FROM_BEST_SOUND;

		case SCHED_CRABSYNTH_TAKECOVER_FAILED:
			{
				if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					return TranslateSchedule( SCHED_RANGE_ATTACK1 );
				}
				
				if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
				{
					return TranslateSchedule( SCHED_RANGE_ATTACK2 );
				}

				// Run somewhere randomly
				return TranslateSchedule( SCHED_FAIL ); 
				break;
			}

		case SCHED_FAIL_ESTABLISH_LINE_OF_FIRE:
			{
				if( HasCondition( COND_SEE_ENEMY ) )
				{
					return TranslateSchedule( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				else
				{
					if ( GetEnemy() )
					{
						RememberUnreachable( GetEnemy() );
					}

					return TranslateSchedule( SCHED_CRABSYNTH_PATROL );
				}
			}
			break;

		case SCHED_CRABSYNTH_PRESS_ATTACK:
			{
				// don't charge forward if there's a hint group
				if (GetHintGroup() != NULL_STRING)
				{
					return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
				}
				return SCHED_CRABSYNTH_PRESS_ATTACK;
			}

		case SCHED_ESTABLISH_LINE_OF_FIRE:
			{
				return SCHED_CRABSYNTH_ESTABLISH_LINE_OF_FIRE;
			}
			break;

		case SCHED_CRABSYNTH_PATROL:
			{
				// If I have an enemy, don't go off into random patrol mode.
				if ( GetEnemy() && GetEnemy()->IsAlive() )
					return SCHED_CRABSYNTH_PATROL_ENEMY;

				return SCHED_CRABSYNTH_PATROL;
			}
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	if (gpGlobals->curtime < m_flNextAttack)
	{
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK2 );
	}

	SetCustomInterruptCondition( COND_CRABSYNTH_OBSTRUCTED );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
NPC_STATE CNPC_CrabSynth::SelectIdealState( void )
{
	switch ( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		{
			if ( GetEnemy() == NULL )
			{
				if ( !HasCondition( COND_ENEMY_DEAD ) )
				{
					// Lost track of my enemy. Patrol.
					SetCondition( COND_CRABSYNTH_SHOULD_PATROL );
				}
				return NPC_STATE_ALERT;
			}
		}

	default:
		{
			return BaseClass::SelectIdealState();
		}
	}

	return GetIdealState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::OnScheduleChange()
{
	BaseClass::OnScheduleChange();

	if (!HasCondition( COND_CRABSYNTH_OBSTRUCTED ))
	{
		m_hObstructor = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::OnBeginMoveAndShoot()
{
	if ( BaseClass::OnBeginMoveAndShoot() )
	{
		if( HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true; // already have the slot I need

		if( !HasStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_ATTACK_OCCLUDER ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::OnEndMoveAndShoot()
{
	VacateStrategySlot();
}

//-----------------------------------------------------------------------------
// Set up the shot regulator based on the equipped weapon
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::OnRangeAttack1()
{
	BaseClass::OnRangeAttack1();

	if ( GetShotRegulator()->IsInRestInterval() && IsMoving() )
	{
		// If we're moving and shooting, see if we can fire a grenade during this interval
		if ( CanGrenadeEnemy() && !IsStrategySlotRangeOccupied( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_GRENADE1 ) )
		{
			GrenadeWarnSound();

			int nGesture = AddGesture( ACT_GESTURE_RANGE_ATTACK2 );
			GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + GetLayerDuration(nGesture) + 0.5);
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::CanGrenadeEnemy( bool bUseFreeKnowledge )
{
	CBaseEntity *pEnemy = this->GetEnemy();

	Assert( pEnemy != NULL );

	if( pEnemy )
	{
		if( bUseFreeKnowledge )
		{
			// throw to where we think they are.
			return CanThrowGrenade( this->GetEnemies()->LastKnownPosition( pEnemy ) );
		}
		else
		{
			// hafta throw to where we last saw them.
			return CanThrowGrenade( this->GetEnemies()->LastSeenPosition( pEnemy ) );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the combine has grenades, hasn't checked lately, and
//			can throw a grenade at the target point.
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::CanThrowGrenade( const Vector &vecTarget )
{
	if ( m_iNumGrenades < 1 )
	{
		// Out of grenades!
		return false;
	}

	if ( !IsGrenadeCapable() )
	{
		// Must be capable of throwing grenades
		return false;
	}

	if ( gpGlobals->curtime < m_flNextGrenadeCheck )
	{
		// Not allowed to throw another grenade right now.
		return false;
	}

	float flDist;
	flDist = ( vecTarget - this->GetAbsOrigin() ).Length();

	if( flDist > 1536 || flDist < 256 )
	{
		// Too close or too far!
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return false;
	}

	// ---------------------------------------------------------------------
	// Are any of my squad members near the intended grenade impact area?
	// ---------------------------------------------------------------------
	CAI_Squad *pSquad = this->GetSquad();
	if ( pSquad )
	{
		if (pSquad->SquadMemberInRange( vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST ))
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.

			// Tell my squad members to clear out so I can get a grenade in
			// Mapbase uses a new context here that gets all nondescript allies away since this code is shared between Combine and non-Combine now.
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY | SOUND_CONTEXT_OWNER_ALLIES, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST, 0.1, this );
			return false;
		}
	}

	return CheckCanThrowGrenade( vecTarget );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the combine can throw a grenade at the specified target point
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::CheckCanThrowGrenade( const Vector &vecTarget )
{
	Vector vecTossLocation;
	GetAttachment( m_nAttachmentPreLaunchPoint, vecTossLocation );

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	Vector vecToss;
	Vector vecMins = -Vector(4,4,4);
	Vector vecMaxs = Vector(4,4,4);
	if( this->FInViewCone( vecTarget ) && CBaseEntity::FVisible( vecTarget ) )
	{
		float flTossDistSqr = ( vecTossLocation - vecTarget ).LengthSqr();
		float flThrowSpeed = RemapValClamped( flTossDistSqr,
			Square( CRABSYNTH_GRENADE_THROW_SPEED_DIST_VARIANCE_MIN ), Square( CRABSYNTH_GRENADE_THROW_SPEED_DIST_VARIANCE_MAX ),
			500.0f, CRABSYNTH_GRENADE_THROW_SPEED );

		vecToss = VecCheckThrow( this, vecTossLocation, vecTarget, flThrowSpeed, 1.0, &vecMins, &vecMaxs );
	}
	else
	{
		// Have to try a high toss. Do I have enough room?
		trace_t tr;
		AI_TraceLine( vecTossLocation, vecTossLocation + Vector( 0, 0, 64 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction != 1.0 )
		{
			return false;
		}

		vecToss = VecCheckToss( this, vecTossLocation, vecTarget, -1, 1.0, true, &vecMins, &vecMaxs );
	}

	if ( vecToss != vec3_origin )
	{
		m_vecTossVelocity = vecToss;

		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // 1/3 second.
		return true;
	}
	else
	{
		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Translate base class activities into combot activites
//-----------------------------------------------------------------------------
Activity CNPC_CrabSynth::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_RUN && ( IsCurSchedule( SCHED_CRABSYNTH_TAKE_COVER_FROM_BEST_SOUND, false ) || IsCurSchedule( SCHED_CRABSYNTH_RUN_AWAY_FROM_BEST_SOUND, false ) ) )
	{
		if ( /*random->RandomInt( 0, 1 ) &&*/ HaveSequenceForActivity( ACT_RUN_PROTECTED ) )
			eNewActivity = ACT_RUN_PROTECTED;
	}
	else if ( m_NPCState == NPC_STATE_ALERT || ( m_NPCState == NPC_STATE_COMBAT && ( !HasCondition( COND_NEW_ENEMY ) || !HasCondition( COND_CRABSYNTH_CAN_CHARGE ) ) ) )
	{
		switch (eNewActivity)
		{
			case ACT_IDLE:
				eNewActivity = ACT_IDLE_ANGRY;
				break;
			case ACT_WALK:
				eNewActivity = ACT_WALK_AIM;
				break;
			case ACT_RUN:
				eNewActivity = ACT_RUN_AIM;
				break;
		}
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	const float MAX_JUMP_RISE = 32.0f;
	const float MAX_JUMP_DISTANCE = 250.0f;
	const float MAX_JUMP_DROP = 192.0f;

	return BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_CRABSYNTH_SHOOT )
	{
		Vector vecMuzzle, vecMuzzleDir;
		UpdateMuzzleMatrix();
		MatrixGetColumn( m_muzzleToWorld, 0, vecMuzzleDir );
		MatrixGetColumn( m_muzzleToWorld, 3, vecMuzzle );

		Shoot( vecMuzzle, vecMuzzleDir );
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_CLAW_CRUSH )
	{
		CBaseEntity *pHurt = CheckTraceHullAttack( 128.0f, -Vector( 32, 32, 34 ), Vector( 32, 32, 34 ), sk_crabsynth_dmg_claw_strike.GetInt(), DMG_CLUB | DMG_ALWAYSGIB );
		if ( pHurt )
		{
			if (pHurt->IsPlayer())
			{
				pHurt->ViewPunch( QAngle( 24, 14, 0 ) );
			}

			EmitSound( "NPC_CrabSynth.Melee_Crush" );
		}
		else
		{
			EmitSound( "NPC_CrabSynth.Melee_Crush_Miss" );
		}
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_CLAW_SWIPE )
	{
		CBaseEntity *pHurt = CheckTraceHullAttack( 128.0f, -Vector( 32, 48, 34 ), Vector( 32, 48, 34 ), sk_crabsynth_dmg_claw_swipe.GetInt(), DMG_CLUB, 2.0f );
		if ( pHurt )
		{
			if (pHurt->IsPlayer())
			{
				pHurt->ViewPunch( QAngle( -12, -7, 0 ) );
			}

			// Apply a velocity to hit entity if it has a physics movetype
			if ( pHurt && pHurt->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				Vector forward, right, up;
				AngleVectors( GetLocalAngles(), &forward, &right, &up );
				pHurt->ApplyAbsVelocityImpulse( forward * 5 + right * 150 + up * -15 );
			}

			EmitSound( "NPC_CrabSynth.Melee_Swipe" );
		}
		else
		{
			EmitSound( "NPC_CrabSynth.Melee_Swipe_Miss" );
		}
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_LAUNCH_GRENADE )
	{
		Vector vecSpin;
		vecSpin.x = random->RandomFloat( -1000.0, 1000.0 );
		vecSpin.y = random->RandomFloat( -1000.0, 1000.0 );
		vecSpin.z = random->RandomFloat( -1000.0, 1000.0 );

		Vector vecStart;
		GetAttachment( m_nAttachmentTopHole, vecStart );

		if( m_NPCState == NPC_STATE_SCRIPT )
		{
			// Use a fixed velocity for grenades thrown in scripted state.
			// Grenades thrown from a script do not count against grenades remaining for the AI to use.
			Vector forward, up, vecThrow;

			GetVectors( &forward, NULL, &up );
			vecThrow = forward * 750 + up * 175;
			CBaseEntity *pGrenade = Crabgrenade_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, CRABSYNTH_GRENADE_TIMER );
			m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
		}
		else
		{
			// Use the Velocity that AI gave us.
			CBaseEntity *pGrenade = Crabgrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, CRABSYNTH_GRENADE_TIMER );
			m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
			AddGrenades(-1, pGrenade);
		}

		// wait twelve seconds before even looking again to see if a grenade can be thrown.
		m_flNextGrenadeCheck = gpGlobals->curtime + 12;
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_CLAW_STEP_LEFT )
	{
		EmitSound( "NPC_CrabSynth.ClawStepLeft" );
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_CLAW_STEP_RIGHT )
	{
		EmitSound( "NPC_CrabSynth.ClawStepRight" );
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_CHARGE_START )
	{
		EmitSound( "NPC_CrabSynth.Charge" );
		return;
	}
	else if ( pEvent->event == AE_CRABSYNTH_LAND )
	{
		EmitSound( "NPC_CrabSynth.Land" );

		// Shake the player's screen and nearby physics objects
		ImpactShock( GetAbsOrigin(), 128, 350 );
		UTIL_ScreenShake( GetAbsOrigin(), 8.0f, 4.0f, 1.0f, 400.0f, SHAKE_START );

		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
		case TASK_RANGE_ATTACK1:
			{
				m_nShots = RandomInt( CRABSYNTH_FIRE_BURST_MIN, CRABSYNTH_FIRE_BURST_MAX );

				m_flNextAttack = gpGlobals->curtime + CRABSYNTH_FIRE_RATE - 0.1;
				ResetIdealActivity( ACT_RANGE_ATTACK1 );
				m_flLastAttackTime = gpGlobals->curtime;
			}
			break;

		case TASK_CRABSYNTH_CHARGE:
			{
				GetMotor()->MoveStop();

				SetIdealActivity( ACT_CRABSYNTH_CHARGE_START );
			}
			break;

		case TASK_CRABSYNTH_GET_PATH_TO_CHARGE_POSITION:
			{
				if ( !m_hChargeTargetPosition )
				{
					TaskFail( "Tried to find a charge position without one specified.\n" );
					break;
				}

				// Move to the charge position
				AI_NavGoal_t goal( GOALTYPE_LOCATION, m_hChargeTargetPosition->GetAbsOrigin(), ACT_RUN );
				if ( GetNavigator()->SetGoal( goal ) )
				{
					// We want to face towards the charge target
					Vector vecDir = m_hChargeTarget->GetAbsOrigin() - m_hChargeTargetPosition->GetAbsOrigin();
					VectorNormalize( vecDir );
					vecDir.z = 0;
					GetNavigator()->SetArrivalDirection( vecDir );
					TaskComplete();
				}
				else
				{
					m_hChargeTarget = NULL;
					m_hChargeTargetPosition = NULL;
					TaskFail( FAIL_NO_ROUTE );
				}
			}
			break;

		case TASK_CRABSYNTH_FACE_TOSS_DIR:
			break;

		case TASK_CRABSYNTH_GET_PATH_TO_FORCED_GREN_LOS:
			StartTask_GetPathToForced( pTask );
			break;

		case TASK_CRABSYNTH_DEFER_SQUAD_GRENADES:
			StartTask_DeferSquad( pTask );
			break;

		case TASK_CRABSYNTH_CHECK_STATE:
			{
				// If it's been >20 seconds since we've seen an enemy, become idle
				if (m_NPCState == NPC_STATE_ALERT && !GetEnemy() && GetLastEnemyTime() < gpGlobals->curtime - 20.0f)
					SetIdealState( NPC_STATE_IDLE );
				TaskComplete();
			}
			break;

		default:
			BaseClass::StartTask(pTask);
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_RANGE_ATTACK1:
			{
				AutoMovement( );

				Vector vecEnemyLKP = GetEnemyLKP();
				if (!FInAimCone( vecEnemyLKP ))
				{
					GetMotor()->SetIdealYawToTargetAndUpdate( vecEnemyLKP, AI_KEEP_YAW_SPEED );
				}
				else
				{
					GetMotor()->SetIdealYawAndUpdate( GetMotor()->GetIdealYaw(), AI_KEEP_YAW_SPEED );
				}

				if ( gpGlobals->curtime >= m_flNextAttack )
				{
					if ( IsActivityFinished() )
					{
						if (--m_nShots > 0)
						{
							ResetIdealActivity( ACT_RANGE_ATTACK1 );
							m_flLastAttackTime = gpGlobals->curtime;
							m_flNextAttack = gpGlobals->curtime + CRABSYNTH_FIRE_RATE - 0.1;
						}
						else
						{
							TaskComplete();
						}
					}
				}
			}
			break;

		case TASK_CRABSYNTH_CHARGE:
			{
				Activity eActivity = GetActivity();

				// See if we're trying to stop after hitting/missing our target
				if ( eActivity == ACT_CRABSYNTH_CHARGE_STOP || eActivity == ACT_CRABSYNTH_CHARGE_CRASH ) 
				{
					if ( IsActivityFinished() )
					{
						TaskComplete();
						return;
					}

					// Still in the process of slowing down. Run movement until it's done.
					AutoMovement();
					return;
				}

				// Check for manual transition
				if ( ( eActivity == ACT_CRABSYNTH_CHARGE_START ) && ( IsActivityFinished() ) )
				{
					SetActivity( ACT_CRABSYNTH_CHARGE );
				}

				// See if we're still running
				if ( eActivity == ACT_CRABSYNTH_CHARGE || eActivity == ACT_CRABSYNTH_CHARGE_START )
				{
					if ( HasCondition( COND_NEW_ENEMY ) || HasCondition( COND_LOST_ENEMY ) || HasCondition( COND_ENEMY_DEAD ) )
					{
						SetActivity( ACT_CRABSYNTH_CHARGE_STOP );
						return;
					}
					else 
					{
						if ( GetEnemy() != NULL )
						{
							Vector	goalDir = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() );
							VectorNormalize( goalDir );

							if ( DotProduct( BodyDirection2D(), goalDir ) < 0.25f )
							{
								SetActivity( ACT_CRABSYNTH_CHARGE_STOP );
							}
						}
					}
				}

				// Steer towards our target
				float idealYaw;
				if ( GetEnemy() == NULL )
				{
					idealYaw = GetMotor()->GetIdealYaw();
				}
				else
				{
					idealYaw = CalcIdealYaw( GetEnemy()->GetAbsOrigin() );
				}

				// Add in our steering offset
				idealYaw += ChargeSteer();
			
				// Turn to face
				GetMotor()->SetIdealYawAndUpdate( idealYaw );

				// See if we're going to run into anything soon
				ChargeLookAhead();

				// Let our animations simply move us forward. Keep the result
				// of the movement so we know whether we've hit our target.
				AIMoveTrace_t moveTrace;
				if ( AutoMovement( GetEnemy(), &moveTrace ) == false )
				{
					// Only stop if we hit the world
					if ( HandleChargeImpact( moveTrace.vEndPosition, moveTrace.pObstruction ) )
					{
						// If we're starting up, this is an error
						if ( eActivity == ACT_CRABSYNTH_CHARGE_START )
						{
							TaskFail( "Unable to make initial movement of charge\n" );
							return;
						}

						// Crash unless we're trying to stop already
						if ( eActivity != ACT_CRABSYNTH_CHARGE_STOP )
						{
							if ( moveTrace.fStatus == AIMR_BLOCKED_WORLD && moveTrace.vHitNormal == vec3_origin )
							{
								SetActivity( ACT_CRABSYNTH_CHARGE_STOP );
							}
							else
							{
								// Shake the screen
								if ( moveTrace.fStatus != AIMR_BLOCKED_NPC )
								{
									EmitSound( "NPC_CrabSynth.ChargeHitWorld" );
									UTIL_ScreenShake( GetAbsOrigin(), 16.0f, 4.0f, 1.0f, 400.0f, SHAKE_START );
								}

								SetActivity( ACT_CRABSYNTH_CHARGE_CRASH );
							}
						}
					}
				}
			}
			break;

		case TASK_CRABSYNTH_FACE_TOSS_DIR:
			RunTask_FaceTossDir( pTask );
			break;

		case TASK_CRABSYNTH_GET_PATH_TO_FORCED_GREN_LOS:
			RunTask_GetPathToForced( pTask );
			break;

		default:
			BaseClass::RunTask(pTask);
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::GatherConditions( void )
{
	BaseClass::GatherConditions();

	ClearCondition( COND_CRABSYNTH_ATTACK_SLOT_AVAILABLE );

	if( GetState() == NPC_STATE_COMBAT )
	{
		if( IsCurSchedule( SCHED_CRABSYNTH_WAIT_IN_COVER, false ) )
		{
			// Soldiers that are standing around doing nothing poll for attack slots so
			// that they can respond quickly when one comes available. If they can 
			// occupy a vacant attack slot, they do so. This holds the slot until their
			// schedule breaks and schedule selection runs again, essentially reserving this
			// slot. If they do not select an attack schedule, then they'll release the slot.
			if( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				SetCondition( COND_CRABSYNTH_ATTACK_SLOT_AVAILABLE );
			}
		}
	}

	// See if we can charge the target
	if ( GetEnemy() )
	{
		if ( ShouldCharge( GetAbsOrigin(), GetEnemy()->GetAbsOrigin(), true, false ) )
		{
			SetCondition( COND_CRABSYNTH_CAN_CHARGE );
		}
		else
		{
			ClearCondition( COND_CRABSYNTH_CAN_CHARGE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the appropriate next schedule based on current condition
//			bits.
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::SelectSchedule(void)
{
	if ( m_hForcedGrenadeTarget )
	{
		if ( m_flNextGrenadeCheck < gpGlobals->curtime )
		{
			Vector vecTarget = m_hForcedGrenadeTarget->WorldSpaceCenter();

			// If we can, throw a grenade at the target. 
			// Ignore grenade count / distance / etc
			if ( CheckCanThrowGrenade( vecTarget ) )
			{
				m_hForcedGrenadeTarget = NULL;
				return SCHED_CRABSYNTH_FORCED_GRENADE_THROW;
			}
		}

		// Can't throw at the target, so lets try moving to somewhere where I can see it
		if ( !FVisible( m_hForcedGrenadeTarget ) )
		{
			return SCHED_CRABSYNTH_MOVE_TO_FORCED_GREN_LOS;
		}
	}

	// Charge after a target if it's set
	if ( m_hChargeTarget && m_hChargeTargetPosition )
	{
		ClearCondition( COND_CRABSYNTH_HAS_CHARGE_TARGET );
		ClearHintGroup();
		
		if ( m_hChargeTarget->IsAlive() == false )
		{
			m_hChargeTarget	= NULL;
			m_hChargeTargetPosition = NULL;
			SetEnemy( m_hOldTarget );

			if ( m_hOldTarget == NULL )
			{
				m_NPCState = NPC_STATE_ALERT;
			}
		}
		else
		{
			m_hOldTarget = GetEnemy();
			SetEnemy( m_hChargeTarget );
			UpdateEnemyMemory( m_hChargeTarget, m_hChargeTarget->GetAbsOrigin() );

			//If we can't see the target, run to somewhere we can
			if ( ShouldCharge( GetAbsOrigin(), GetEnemy()->GetAbsOrigin(), false, false ) == false )
				return SCHED_CRABSYNTH_FIND_CHARGE_POSITION;

			return SCHED_CRABSYNTH_CHARGE_TARGET;
		}
	}

	if ( m_NPCState != NPC_STATE_SCRIPT)
	{
		// We've been told to move away from a target to make room for a grenade to be thrown at it
		if ( HasCondition( COND_HEAR_MOVE_AWAY ) )
		{
			return SCHED_MOVE_AWAY;
		}

		// These things are done in any state but dead and prone
		if (m_NPCState != NPC_STATE_DEAD && m_NPCState != NPC_STATE_PRONE )
		{
			// Cower when physics objects are thrown at me
			if ( HasCondition( COND_HEAR_PHYSICS_DANGER ) )
			{
				return SCHED_FLINCH_PHYSICS;
			}

			// grunts place HIGH priority on running away from danger sounds.
			if ( HasCondition(COND_HEAR_DANGER) )
			{
				CSound *pSound;
				pSound = GetBestSound();

				Assert( pSound != NULL );
				if ( pSound)
				{
					if (pSound->m_iType & SOUND_DANGER)
					{
						DangerSound();

						// If the sound is approaching danger, I have no enemy, and I don't see it, turn to face.
						if( !GetEnemy() && pSound->IsSoundType(SOUND_CONTEXT_DANGER_APPROACH) && pSound->m_hOwner && !FInViewCone(pSound->GetSoundReactOrigin()) )
						{
							GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
							return SCHED_CRABSYNTH_FACE_IDEAL_YAW;
						}

						return SCHED_TAKE_COVER_FROM_BEST_SOUND;
					}

					// JAY: This was disabled in HL1.  Test?
					if (!HasCondition( COND_SEE_ENEMY ) && ( pSound->m_iType & (SOUND_PLAYER | SOUND_COMBAT) ))
					{
						GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
					}
				}
			}
		}

		// If we have an obstruction, just plow through
		if ( HasCondition( COND_CRABSYNTH_OBSTRUCTED ) )
		{
			SetTarget( m_hObstructor );
			m_hObstructor = NULL;
			return SCHED_CRABSYNTH_ATTACK_TARGET;
		}

		if( BehaviorSelectSchedule() )
		{
			return BaseClass::SelectSchedule();
		}
	}

	int nSchedule = SCHED_NONE;

	switch (m_NPCState)
	{
		case NPC_STATE_IDLE:
			{
				if ( m_bShouldPatrol )
					return SCHED_CRABSYNTH_PATROL;
			}
			// NOTE: Fall through!

		case NPC_STATE_ALERT:
			{
				if( HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE) )
				{
					AI_EnemyInfo_t *pDanger = GetEnemies()->GetDangerMemory();
					if( pDanger && FInViewCone(pDanger->vLastKnownLocation) && !BaseClass::FVisible(pDanger->vLastKnownLocation) )
					{
						// I've been hurt, I'm facing the danger, but I don't see it, so move from this position.
						return SCHED_TAKE_COVER_FROM_ORIGIN;
					}
				}

				if( HasCondition( COND_HEAR_COMBAT ) )
				{
					CSound *pSound = GetBestSound();

					if( pSound && pSound->IsSoundType( SOUND_COMBAT ) )
					{
						if( m_pSquad && m_pSquad->GetSquadMemberNearestTo( pSound->GetSoundReactOrigin() ) == this && OccupyStrategySlot( SQUAD_SLOT_INVESTIGATE_SOUND ) )
						{
							return SCHED_INVESTIGATE_SOUND;
						}
					}
				}

				if( m_bShouldPatrol || HasCondition( COND_CRABSYNTH_SHOULD_PATROL ) )
					return SCHED_CRABSYNTH_PATROL;
			}
			break;

		case NPC_STATE_COMBAT:
			nSchedule = SelectCombatSchedule();
			break;
	}

	if ( nSchedule != SCHED_NONE )
		return nSchedule;

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::SelectCombatSchedule( void )
{
	// -----------
	// dead enemy
	// -----------
	if ( HasCondition( COND_ENEMY_DEAD ) )
	{
		// call base class, all code to handle dead enemies is centralized there.
		return SCHED_NONE;
	}

	// -----------
	// new enemy
	// -----------
	if ( HasCondition( COND_NEW_ENEMY ) )
	{
		CBaseEntity *pEnemy = GetEnemy();

		if ( m_pSquad && pEnemy )
		{
			bool bFirstContact = false;

			if (gpGlobals->curtime - GetEnemies()->FirstTimeSeen( pEnemy ) < 3.0f)
				bFirstContact = true;

			if ( HasCondition( COND_CRABSYNTH_CAN_CHARGE ) && OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ) )
			{
				// Charge if someone else isn't already
				return SCHED_CRABSYNTH_CHARGE;
			}

			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ) )
				{
					// Start suppressing if someone isn't firing already (SLOT_ATTACK1). This means
					// I'm the guy who spotted the enemy, I should react immediately.
					return SCHED_CRABSYNTH_SUPPRESS;
				}
			}

			if ( m_pSquad->IsLeader( this ) || ( m_pSquad->GetLeader() && m_pSquad->GetLeader()->GetEnemy() != pEnemy ) )
			{
				// I'm the leader, but I didn't get the job suppressing the enemy. We know this because
				// This code only runs if the code above didn't assign me SCHED_COMBINE_SUPPRESS.
				if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					return SCHED_RANGE_ATTACK1;
				}
			}
			else
			{
				// First contact, and I'm solo, or not the squad leader.
				if( HasCondition( COND_SEE_ENEMY ) && CanGrenadeEnemy() )
				{
					if( OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
					{
						GrenadeWarnSound();
						return SCHED_RANGE_ATTACK2;
					}
				}

				if( /*!bFirstContact &&*/ OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					if( random->RandomInt(0, 100) < 60 )
					{
						return SCHED_ESTABLISH_LINE_OF_FIRE;
					}
					else
					{
						return SCHED_CRABSYNTH_PRESS_ATTACK;
					}
				}

				return SCHED_TAKE_COVER_FROM_ENEMY;
			}
		}
	}

	// ----------------------
	// HEAVY DAMAGE
	// ----------------------
	if ( HasCondition( COND_HEAVY_DAMAGE ) )
	{
		if ( GetEnemy() != NULL )
		{
			// only try to take cover if we actually have an enemy!
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}
	}

	int attackSchedule = SelectScheduleAttack();
	if ( attackSchedule != SCHED_NONE )
		return attackSchedule;

	if (HasCondition( COND_ENEMY_OCCLUDED ))
	{
		if ( GetEnemy() && !(GetEnemy()->GetFlags() & FL_NOTARGET) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			// Charge in and break the enemy's cover!
			return SCHED_ESTABLISH_LINE_OF_FIRE;
		}

		// If I'm a long, long way away, establish a LOF anyway. Once I get there I'll
		// start respecting the squad slots again.
		float flDistSq = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
		if ( flDistSq > Square(3000) )
			return SCHED_ESTABLISH_LINE_OF_FIRE;

		// Otherwise tuck in.
		Remember( bits_MEMORY_INCOVER );
		return SCHED_CRABSYNTH_WAIT_IN_COVER;
	}

	// --------------------------------------------------------------
	// Enemy not reachable, not attackable
	// --------------------------------------------------------------
	if ( HasCondition( COND_ENEMY_UNREACHABLE ) )
	{
		return SelectUnreachableSchedule();
	}

	// --------------------------------------------------------------
	// Enemy too close to attack
	// --------------------------------------------------------------
	if ( HasCondition( COND_TOO_CLOSE_TO_ATTACK ) || ( !HasCondition( COND_SEE_ENEMY ) && HasCondition( COND_LIGHT_DAMAGE ) ) )
	{
		return SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY;
	}

	// --------------------------------------------------------------
	// Enemy not occluded but isn't open to attack
	// --------------------------------------------------------------
	if ( HasCondition( COND_SEE_ENEMY ) && !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		return SCHED_CRABSYNTH_PRESS_ATTACK;
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::SelectScheduleAttack()
{
	// Kick attack?
	if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		return SCHED_MELEE_ATTACK1;
	}
	
	// Charge?
	if ( HasCondition( COND_CRABSYNTH_CAN_CHARGE ) && OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ) )
	{
		// Don't let other squad members charge while we're doing it
		return SCHED_CRABSYNTH_CHARGE;
	}

	// Can I shoot?
	if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		// Engage if allowed
		if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			return SCHED_RANGE_ATTACK1;
		}

		// Throw a grenade if not allowed to engage with weapon.
		if ( CanGrenadeEnemy() )
		{
			if ( OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
			{
				GrenadeWarnSound();
				return SCHED_RANGE_ATTACK2;
			}
		}

		return SCHED_TAKE_COVER_FROM_ENEMY;
	}

	if ( GetEnemy() && !HasCondition(COND_SEE_ENEMY) )
	{
		// We don't see our enemy. If it hasn't been long since I last saw him,
		// and he's pretty close to the last place I saw him, throw a grenade in 
		// to flush him out. A wee bit of cheating here...

		float flTime;
		float flDist;

		flTime = gpGlobals->curtime - GetEnemies()->LastTimeSeen( GetEnemy() );
		flDist = ( GetEnemy()->GetAbsOrigin() - GetEnemies()->LastSeenPosition( GetEnemy() ) ).Length();

		if ( flTime <= COMBINE_GRENADE_FLUSH_TIME && flDist <= COMBINE_GRENADE_FLUSH_DIST && CanGrenadeEnemy( false ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
		{
			GrenadeWarnSound();
			return SCHED_RANGE_ATTACK2;
		}
	}

	if ( HasCondition(COND_WEAPON_SIGHT_OCCLUDED) )
	{
		// If they are hiding behind something that we can destroy, start shooting at it.
		CBaseEntity *pBlocker = GetEnemyOccluder();
		if ( pBlocker && pBlocker->GetHealth() > 0 && OccupyStrategySlot( SQUAD_SLOT_ATTACK_OCCLUDER ) )
		{
			return SCHED_SHOOT_ENEMY_COVER;
		}
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Our enemy is unreachable. Select a schedule.
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::SelectUnreachableSchedule( void )
{
	// Deal with a visible player
	if ( HasCondition( COND_SEE_ENEMY ) )
	{
		// If we're under attack, then let's leave for a bit
		if ( GetEnemy() && HasCondition( COND_HEAVY_DAMAGE ) )
		{
			Vector dir = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize(dir);

			GetMotor()->SetIdealYaw( -dir );
			
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}
	}

	// If we're too far away, try to close distance to our target first
	float flDistToEnemySqr = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
	if ( flDistToEnemySqr > Square( 100*12 ) )
		return SCHED_ESTABLISH_LINE_OF_FIRE;

	// Fire that we're unable to reach our target!
	if ( GetEnemy() && GetEnemy()->IsPlayer() )
	{
		m_OnLostPlayer.FireOutput( this, this );
	}

	m_OnLostEnemy.FireOutput( this, this );
	GetEnemies()->MarkAsEluded( GetEnemy() );

	// Roar at the player as show of frustration
	if ( m_flNextRoarTime < gpGlobals->curtime )
	{
		m_flNextRoarTime = gpGlobals->curtime + RandomFloat( 20,40 );
		return SCHED_CRABSYNTH_ROAR;
	}

	// Move randomly for the moment
	return SCHED_CRABSYNTH_CANT_ATTACK;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the appropriate next schedule based on current condition
//			bits.
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( failedSchedule == SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY )
	{
		// Most likely backed into a corner. Just blaze away.
		if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		{
			return SCHED_CRABSYNTH_MELEE_ATTACK1;
		}
		else
		{
			return SCHED_CRABSYNTH_TAKE_COVER1;
		}
	}
	else if ( failedSchedule == SCHED_CRABSYNTH_RUN_AWAY_FROM_BEST_SOUND )
	{
		// Instead of cowering, just ignore the sound and try to select an attack schedule
		int nSched = SelectScheduleAttack();
		if ( nSched != SCHED_NONE )
			return nSched;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::NPCThink( void )
{
	// update follower bones
	m_BoneFollowerManager.UpdateBoneFollowers( this );

	BaseClass::NPCThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	/*if ( IsOnFire() )
	{
		SetCondition( COND_COMBINE_ON_FIRE );
	}
	else
	{
		ClearCondition( COND_COMBINE_ON_FIRE );
	}*/

	if( gpGlobals->curtime >= m_flStopMoveShootTime )
	{
		// Time to stop move and shoot and start facing the way I'm running.
		// This makes the combine look attentive when disengaging, but prevents
		// them from always running around facing you.
		//
		// Only do this if it won't be immediately shut off again.
		if( GetNavigator()->GetPathTimeToGoal() > 1.0f )
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot( 5.0f );
			m_flStopMoveShootTime = FLT_MAX;
		}
	}

	if( m_flGroundSpeed > 0 && GetState() == NPC_STATE_COMBAT && m_MoveAndShootOverlay.IsSuspended() )
	{
		// Return to move and shoot when near my goal so that I 'tuck into' the location facing my enemy.
		if( GetNavigator()->GetPathTimeToGoal() <= 1.0f )
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot( 0 );
		}
	}

	// Spin barrel
	if (m_flBarrelSpinSpeed > 0.1f)
	{
		float flValue = GetPoseParameter( m_nPoseBarrelSpin );
		SetPoseParameter( m_nPoseBarrelSpin, UTIL_Approach( flValue + m_flBarrelSpinSpeed, flValue, CRABSYNTH_BARREL_SPIN_APPROACH ) );

		m_flBarrelSpinSpeed *= 0.95f;
	}

	if (g_debug_crabsynth.GetInt() == 2 && m_debugOverlays & OVERLAY_NPC_SELECTED_BIT)
	{
		// Show our eye attachment
		Vector vecEyePosition = EyePosition();
		QAngle angEyeAngles = EyeAngles();

		NDebugOverlay::Cross3DOriented( vecEyePosition, angEyeAngles, 5.0f, 255, 0, 0, true, 0.5f );

		// Show our muzzle attachment
		Vector vecMuzzle, vecMuzzleDir;
		UpdateMuzzleMatrix();
		MatrixGetColumn( m_muzzleToWorld, 0, vecMuzzleDir );
		MatrixGetColumn( m_muzzleToWorld, 3, vecMuzzle );

		QAngle angMuzzle;
		VectorAngles( vecMuzzleDir, angMuzzle );

		NDebugOverlay::Cross3DOriented( vecMuzzle, angMuzzle, 5.0f, 255, 255, 0, true, 0.5f );

		if (GetEnemy())
		{
			// Show a line for each eye position
			Vector vecEnemyCenter = GetEnemy()->WorldSpaceCenter();

			NDebugOverlay::Line( vecEyePosition, vecEnemyCenter, 255, 0, 0, true, 0.5f );

			Vector los = (vecEnemyCenter - vecEyePosition);
			Vector los2 = (vecEnemyCenter - vecMuzzle);

			// do this in 2D
			los.z = 0;
			VectorNormalize( los );

			Vector facingDir = EyeDirection2D();

			float flDot = DotProduct( los, facingDir );
			EntityText( 0, UTIL_VarArgs( "Dot: %f", flDot ), 0.5f, 255, flDot > m_flFieldOfView ? 255 : 128, flDot > m_flFieldOfView ? 255 : 128, 255 );

			if ( flDot <= m_flFieldOfView && m_NPCState == NPC_STATE_COMBAT /*&& vecSpot.z < EyePosition().z*/ )
			{
				// Try using our gun as the direction in case the target is under our head
				UpdateMuzzleMatrix();
				MatrixGetColumn( m_muzzleToWorld, 0, facingDir );

				flDot = DotProduct( los, facingDir );
				EntityText( 1, UTIL_VarArgs( "Gun Dot: %f", flDot ), 0.5f, 255, flDot > m_flFieldOfView ? 255 : 128, flDot > m_flFieldOfView ? 255 : 128, 255 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.pObstruction && !pMoveGoal->directTrace.pObstruction->IsWorld() && GetGroundEntity() != pMoveGoal->directTrace.pObstruction )
	{
		if ( ShouldAttackObstruction( pMoveGoal->directTrace.pObstruction ) )
		{
			if (m_hObstructor != pMoveGoal->directTrace.pObstruction)
			{
				if (!m_hObstructor)
					m_flTimeSinceObstructed = gpGlobals->curtime;

				m_hObstructor = pMoveGoal->directTrace.pObstruction;
			}
			else
			{
				// Interrupt obstructions based on state
				switch (GetState())
				{
					case NPC_STATE_COMBAT:
					{
						// Always interrupt when in combat
						SetCondition( COND_CRABSYNTH_OBSTRUCTED );
					}
					break;

					case NPC_STATE_IDLE:
					case NPC_STATE_ALERT:
					default:
					{
						// Interrupt if we've been obstructed for a bit
						if (gpGlobals->curtime - m_flTimeSinceObstructed >= 2.0f)
							SetCondition( COND_CRABSYNTH_OBSTRUCTED );
					}
					break;
				}
			}
		}
	}

	return BaseClass::OnObstructionPreSteer( pMoveGoal, distClear, pResult );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::ShouldAttackObstruction( CBaseEntity *pEntity )
{
	// Don't attack obstructions if we can't melee attack
	if ((CapabilitiesGet() & bits_CAP_INNATE_MELEE_ATTACK1) == 0)
		return false;

	// Only attack physics props
	if ( pEntity->MyCombatCharacterPointer() || pEntity->GetMoveType() != MOVETYPE_VPHYSICS )
		return false;

	// Don't attack props which don't have motion enabled or have more than 2x mass than us
	if ( pEntity->VPhysicsGetObject() && (!pEntity->VPhysicsGetObject()->IsMotionEnabled() || pEntity->VPhysicsGetObject()->GetMass()*2 > GetMass()) )
		return false;

	// Do not attack props which could explode
	CBreakableProp *pProp = dynamic_cast<CBreakableProp*>(pEntity);
	if (pProp && pProp->GetExplosiveDamage() > 0)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if our charge target is right in front of the guard
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::EnemyIsRightInFrontOfMe( CBaseEntity **pEntity )
{
	if ( !GetEnemy() )
		return false;

	if ( (GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter()).LengthSqr() < (156*156) )
	{
		Vector vecLOS = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() );
		vecLOS.z = 0;
		VectorNormalize( vecLOS );
		Vector vBodyDir = BodyDirection2D();
		if ( DotProduct( vecLOS, vBodyDir ) > 0.8 )
		{
			// He's in front of me, and close. Make sure he's not behind a wall.
			trace_t tr;
			UTIL_TraceLine( WorldSpaceCenter(), GetEnemy()->EyePosition(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			if ( tr.m_pEnt == GetEnemy() )
			{
				*pEntity = tr.m_pEnt;
				return true;
			}
		}
	}

	return false;
}

extern void HunterTraceHull_SkipPhysics_Extern( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin,
	const Vector &hullMax, unsigned int mask, const CBaseEntity *ignore,
	int collisionGroup, trace_t *ptr, float minMass );

//-----------------------------------------------------------------------------
// Purpose: While charging, look ahead and see if we're going to run into anything.
//			If we are, start the gesture so it looks like we're anticipating the hit.
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::ChargeLookAhead( void )
{
	trace_t	tr;
	Vector vecForward;
	GetVectors( &vecForward, NULL, NULL );
	Vector vecTestPos = GetAbsOrigin() + ( vecForward * m_flGroundSpeed * 0.75 );
	Vector testHullMins = GetHullMins();
	testHullMins.z += (StepHeight() * 2);
	HunterTraceHull_SkipPhysics_Extern( GetAbsOrigin(), vecTestPos, testHullMins, GetHullMaxs(), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr, GetMass() * 0.5 );

	//NDebugOverlay::Box( tr.startpos, testHullMins, GetHullMaxs(), 0, 255, 0, true, 0.1f );
	//NDebugOverlay::Box( vecTestPos, testHullMins, GetHullMaxs(), 255, 0, 0, true, 0.1f );

	if ( tr.fraction != 1.0 )
	{
		// Start playing the hit animation
		AddGesture( ACT_CRABSYNTH_CHARGE_ANTICIPATION );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the guard charging into something. Returns true if it hit the world.
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::HandleChargeImpact( Vector vecImpact, CBaseEntity *pEntity )
{
	// Cause a shock wave from this point which will disrupt nearby physics objects
	ImpactShock( vecImpact, 128, 350 );

	// Did we hit anything interesting?
	if ( !pEntity || pEntity->IsWorld() )
	{
		// Robin: Due to some of the finicky details in the motor, the guard will hit
		//		  the world when it is blocked by our enemy when trying to step up 
		//		  during a moveprobe. To get around this, we see if the enemy's within
		//		  a volume in front of the guard when we hit the world, and if he is,
		//		  we hit him anyway.
		EnemyIsRightInFrontOfMe( &pEntity );

		// Did we manage to find him? If not, increment our charge miss count and abort.
		if ( pEntity->IsWorld() )
		{
			//m_iChargeMisses++;
			return true;
		}
	}

	// Hit anything we don't like
	if ( IRelationType( pEntity ) <= D_FR && ( GetNextAttack() < gpGlobals->curtime ) )
	{
		EmitSound( "NPC_CrabSynth.Shove" );

		if ( !IsPlayingGesture( ACT_CRABSYNTH_CHARGE_HIT ) )
		{
			RestartGesture( ACT_CRABSYNTH_CHARGE_HIT );
		}
		
		ChargeDamage( pEntity );
		
		pEntity->ApplyAbsVelocityImpulse( ( BodyDirection2D() * 400 ) + Vector( 0, 0, 200 ) );

		if ( !pEntity->IsAlive() && GetEnemy() == pEntity )
		{
			SetEnemy( NULL );
		}

		SetNextAttack( gpGlobals->curtime + 2.0f );
		SetActivity( ACT_CRABSYNTH_CHARGE_STOP );

		// We've hit something, so clear our miss count
		//m_iChargeMisses = 0;
		return false;
	}

	// Hit something we don't hate. If it's not moveable, crash into it.
	if ( pEntity->GetMoveType() == MOVETYPE_NONE || pEntity->GetMoveType() == MOVETYPE_PUSH )
		return true;

	// If it's a vphysics object that's too heavy, crash into it too.
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if ( pPhysics )
		{
			// If the object is being held by the player, knock it out of his hands
			if ( pPhysics->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
			{
				Pickup_ForcePlayerToDropThisObject( pEntity );
				return false;
			}

			if ( (!pPhysics->IsMoveable() || pPhysics->GetMass() > GetMass() * 0.5f ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate & apply damage & force for a charge to a target.
//			Done outside of the guard because we need to do this inside a trace filter.
//-----------------------------------------------------------------------------
void ApplyCrabSynthChargeDamage( CBaseEntity *pCrabSynth, CBaseEntity *pTarget, float flDamage )
{
	Vector attackDir = ( pTarget->WorldSpaceCenter() - pCrabSynth->WorldSpaceCenter() );
	VectorNormalize( attackDir );
	Vector offset = RandomVector( -32, 32 ) + pTarget->WorldSpaceCenter();

	// Generate enough force to make a 75kg guy move away at 700 in/sec
	Vector vecForce = attackDir * ImpulseScale( 75, 700 );

	// Deal the damage
	CTakeDamageInfo	info( pCrabSynth, pCrabSynth, vecForce, offset, flDamage, DMG_CLUB );
	pTarget->TakeDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			radius - 
//			magnitude - 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::ImpactShock( const Vector &origin, float radius, float magnitude, CBaseEntity *pIgnored )
{
	// Also do a local physics explosion to push objects away
	float	adjustedDamage, flDist;
	Vector	vecSpot;
	float	falloff = 1.0f / 2.5f;

	CBaseEntity *pEntity = NULL;

	// Find anything within our radius
	while ( ( pEntity = gEntList.FindEntityInSphere( pEntity, origin, radius ) ) != NULL )
	{
		// Don't affect the ignored target
		if ( pEntity == pIgnored )
			continue;
		if ( pEntity == this )
			continue;

		// UNDONE: Ask the object if it should get force if it's not MOVETYPE_VPHYSICS?
		if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS || ( pEntity->VPhysicsGetObject() && pEntity->IsPlayer() == false ) )
		{
			vecSpot = pEntity->BodyTarget( GetAbsOrigin() );
			
			// decrease damage for an ent that's farther from the bomb.
			flDist = ( GetAbsOrigin() - vecSpot ).Length();

			if ( radius == 0 || flDist <= radius )
			{
				adjustedDamage = flDist * falloff;
				adjustedDamage = magnitude - adjustedDamage;
		
				if ( adjustedDamage < 1 )
				{
					adjustedDamage = 1;
				}

				CTakeDamageInfo info( this, this, adjustedDamage, DMG_BLAST );
				CalculateExplosiveDamageForce( &info, (vecSpot - GetAbsOrigin()), GetAbsOrigin() );

				pEntity->VPhysicsTakeDamage( info );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::ChargeDamage( CBaseEntity *pTarget )
{
	if ( pTarget == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pTarget );

	if ( pPlayer != NULL )
	{
		//Kick the player angles
		pPlayer->ViewPunch( QAngle( 20, 20, -30 ) );	

		Vector	dir = pPlayer->WorldSpaceCenter() - WorldSpaceCenter();
		VectorNormalize( dir );
		dir.z = 0.0f;
		
		Vector vecNewVelocity = dir * 250.0f;
		vecNewVelocity[2] += 128.0f;
		pPlayer->SetAbsVelocity( vecNewVelocity );

		color32 red = {128,0,0,128};
		UTIL_ScreenFade( pPlayer, red, 1.0f, 0.1f, FFADE_IN );
	}
	
	// If it's being held by the player, break that bond
	Pickup_ForcePlayerToDropThisObject( pTarget );

	// Calculate the physics force
	ApplyCrabSynthChargeDamage( this, pTarget, sk_crabsynth_dmg_charge.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CNPC_CrabSynth::ChargeSteer( void )
{
	trace_t	tr;
	Vector	testPos, steer, forward, right;
	QAngle	angles;
	const float	testLength = m_flGroundSpeed * 0.15f;

	//Get our facing
	GetVectors( &forward, &right, NULL );

	steer = forward;

	const float faceYaw	= UTIL_VecToYaw( forward );

	//Offset right
	VectorAngles( forward, angles );
	angles[YAW] += 45.0f;
	AngleVectors( angles, &forward );

	//Probe out
	testPos = GetAbsOrigin() + ( forward * testLength );

	//Offset by step height
	Vector testHullMins = GetHullMins();
	testHullMins.z += (StepHeight() * 2);

	//Probe
	HunterTraceHull_SkipPhysics_Extern( GetAbsOrigin(), testPos, testHullMins, GetHullMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr, GetMass() * 0.5f );

	//Add in this component
	steer += ( right * 0.5f ) * ( 1.0f - tr.fraction );

	//Offset left
	angles[YAW] -= 90.0f;
	AngleVectors( angles, &forward );

	//Probe out
	testPos = GetAbsOrigin() + ( forward * testLength );

	// Probe
	HunterTraceHull_SkipPhysics_Extern( GetAbsOrigin(), testPos, testHullMins, GetHullMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr, GetMass() * 0.5f );

	//Add in this component
	steer -= ( right * 0.5f ) * ( 1.0f - tr.fraction );

	return UTIL_AngleDiff( UTIL_VecToYaw( steer ), faceYaw );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::ShouldCharge( const Vector &startPos, const Vector &endPos, bool useTime, bool bCheckForCancel )
{
	// Must have a target
	if ( !GetEnemy() )
		return false;

	// Don't check the distance once we start charging
	if ( !bCheckForCancel )
	{
		// Don't allow use to charge again if it's been too soon
		if ( useTime && ( m_flChargeTime > gpGlobals->curtime ) )
			return false;

		// Must be around the same level
		if ( fabs( startPos.z - endPos.z ) > CRABSYNTH_CHARGE_MAX_HEIGHT_DIFF )
			return false;

		float distance = UTIL_DistApprox2D( startPos, endPos );

		// Must be within our tolerance range
		if ( ( distance < CRABSYNTH_CHARGE_MIN ) || ( distance > CRABSYNTH_CHARGE_MAX ) )
			return false;

		// If we have at least 3 other enemies clustered together at a smaller distance than our own distance to our current enemy,
		// don't charge into disadvantaged territory
		if ( GetEnemies()->NumEnemies() > 3 )
		{
			float flOurDistToEnemySqr = ( GetAbsOrigin() - GetEnemy()->GetAbsOrigin() ).LengthSqr();

			AIEnemiesIter_t iter;
			int nNumInterferingEnemies = 0;
			for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
			{
				if ( pEMemory->hEnemy->IsAlive() && pEMemory->hEnemy != GetEnemy() && (pEMemory->hEnemy->Classify() != CLASS_BULLSEYE))
				{
					float flEnemyToEnemySqr = (pEMemory->hEnemy->GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).LengthSqr();
					if ( flEnemyToEnemySqr < flOurDistToEnemySqr )
					{
						nNumInterferingEnemies++;

						if ( nNumInterferingEnemies >= 3 )
							break;
					}
				}
			}

			if ( nNumInterferingEnemies >= 3 )
				return false;
		}
	}

	if ( GetSquad() )
	{
		// If someone in our squad is closer to the enemy, then don't charge (we end up hitting them more often than not!)
		float flOurDistToEnemySqr = ( GetAbsOrigin() - GetEnemy()->GetAbsOrigin() ).LengthSqr();
		AISquadIter_t iter;
		for ( CAI_BaseNPC *pSquadMember = GetSquad()->GetFirstMember( &iter ); pSquadMember; pSquadMember = GetSquad()->GetNextMember( &iter ) )
		{
			if ( pSquadMember->IsAlive() == false || pSquadMember == this )
				continue;

			if ( ( pSquadMember->GetAbsOrigin() - GetEnemy()->GetAbsOrigin() ).LengthSqr() < flOurDistToEnemySqr )
				return false;
		}
	}

	//FIXME: We'd like to exclude small physics objects from this check!

	// We only need to hit the endpos with the edge of our bounding box
	Vector vecDir = endPos - startPos;
	VectorNormalize( vecDir );
	float flWidth = WorldAlignSize().x * 0.5;
	Vector vecTargetPos = endPos - (vecDir * flWidth);

	// See if we can directly move there
	AIMoveTrace_t moveTrace;
	GetMoveProbe()->MoveLimit( NAV_GROUND, startPos, vecTargetPos, MASK_NPCSOLID_BRUSHONLY, GetEnemy(), &moveTrace );
	
	// If we're not blocked, charge
	if ( IsMoveBlocked( moveTrace ) )
	{
		// Don't allow it if it's too close to us
		if ( UTIL_DistApprox( WorldSpaceCenter(), moveTrace.vEndPosition ) < CRABSYNTH_CHARGE_MIN )
			return false;

		// Allow some special cases to not block us
		if ( moveTrace.pObstruction != NULL )
		{
			// If we've hit the world, see if it's a cliff
			if ( moveTrace.pObstruction == GetContainingEntity( INDEXENT(0) ) )
			{	
				// Can't be too far above/below the target
				if ( fabs( moveTrace.vEndPosition.z - vecTargetPos.z ) > StepHeight() )
					return false;

				// Allow it if we got pretty close
				if ( UTIL_DistApprox( moveTrace.vEndPosition, vecTargetPos ) < 64 )
					return true;
			}

			// Hit things that will take damage
			if ( moveTrace.pObstruction->m_takedamage != DAMAGE_NO )
				return true;

			// Hit things that will move
			if ( moveTrace.pObstruction->GetMoveType() == MOVETYPE_VPHYSICS )
				return true;
		}

		return false;
	}

	// Only update this if we've requested it
	if ( useTime )
	{
		m_flChargeTime = gpGlobals->curtime + 5.0f;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check the innate weapon LOS for an owner at an arbitrary position
//			If bSetConditions is true, LOS related conditions will also be set
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::InnateWeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	return BaseClass::InnateWeaponLOSCondition( ownerPos, targetPos, bSetConditions );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( flDist > sk_crabsynth_gun_max_range.GetFloat() )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if ( flDist < sk_crabsynth_gun_min_range.GetFloat() )
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if ( flDot < 0.8f )
	{
		return COND_NOT_FACING_ATTACK;
	}

	if ( GetEnemy() )
	{
		// Do not shoot our charge target
		if ( GetEnemy() == m_hChargeTarget )
			return COND_NONE;
	}

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//			This is for Grenade attacks.  As the test for grenade attacks
//			is expensive we don't want to do it every frame.  Return true
//			if we meet minimum set of requirements and then test for actual
//			throw later if we actually decide to do a grenade attack.
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CNPC_CrabSynth::RangeAttack2Conditions( float flDot, float flDist )
{
	return COND_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CrabSynth::MeleeAttack1Conditions( float flDot, float flDist )
{
	if (flDist > 128.0f)
	{
		return COND_NONE; // COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.5)
	{
		return COND_NONE; // COND_NOT_FACING_ATTACK;
	}

	if ( GetEnemy() )
	{
		// Check Z
		if ( fabs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) > 64 )
			return COND_NONE;
	}

	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: degrees to turn in 0.1 seconds
//-----------------------------------------------------------------------------
float CNPC_CrabSynth::MaxYawSpeed( void )
{
	switch( GetActivity() )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 35;
		break;
	case ACT_RUN:
	case ACT_RUN_HURT:
		return 10;
		break;
	case ACT_RUN_PROTECTED:
		return 35;
	case ACT_WALK:
	case ACT_WALK_CROUCH:
		return 15;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		return 25;
	default:
		return 25;
		break;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CNPC_CrabSynth::ShouldMoveAndShoot()
{
	// Set this timer so that gpGlobals->curtime can't catch up to it. 
	// Essentially, we're saying that we're not going to interfere with 
	// what the AI wants to do with move and shoot. 
	//
	// If any code below changes this timer, the code is saying 
	// "It's OK to move and shoot until gpGlobals->curtime == m_flStopMoveShootTime"
	m_flStopMoveShootTime = FLT_MAX;

	if( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND, false ) )
		return false;

	if( IsCurSchedule( SCHED_CRABSYNTH_TAKE_COVER_FROM_BEST_SOUND, false ) )
		return false;

	if( IsCurSchedule( SCHED_CRABSYNTH_RUN_AWAY_FROM_BEST_SOUND, false ) )
		return false;

	if( m_pSquad && IsCurSchedule( SCHED_CRABSYNTH_TAKE_COVER1, false ) )
		m_flStopMoveShootTime = gpGlobals->curtime + random->RandomFloat( 0.4f, 0.6f );

	return BaseClass::ShouldMoveAndShoot();
}

//-----------------------------------------------------------------------------
// Set up the shot regulator based on the equipped weapon
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::OnUpdateShotRegulator( )
{
	// Default values
	GetShotRegulator()->SetBurstInterval( CRABSYNTH_FIRE_RATE, CRABSYNTH_FIRE_RATE );
	GetShotRegulator()->SetBurstShotCountRange( CRABSYNTH_FIRE_BURST_MIN, CRABSYNTH_FIRE_BURST_MAX );
	GetShotRegulator()->SetRestInterval( CRABSYNTH_FIRE_REST_MIN, CRABSYNTH_FIRE_REST_MAX );

	// Let the behavior have a whack at it.
	if ( GetRunningBehavior() )
	{
		GetRunningBehavior()->OnUpdateShotRegulator();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::MaintainLookTargets( float flInterval )
{
	// Our gun is our head, so hand it to aiming code when we have an enemy
	if ( GetEnemy() && CapabilitiesGet() & bits_CAP_AIM_GUN )
	{
		SetAccumulatedYawAndUpdate();
		ProcessSceneEvents();
		MaintainTurnActivity();
		DoBodyLean();
		UpdateBodyControl();
		InvalidateBoneCache();
		return;
	}

	BaseClass::MaintainLookTargets( flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: Set direction that the NPC aiming their gun
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::SetAim( const Vector &aimDir )
{
	BaseClass::SetAim( aimDir );
	/*
	Vector vecTarget = EyePosition() + aimDir;
	AddLookTarget( vecTarget, 0.5f, 1.0f, 0.0f );

	// Calculate our interaction yaw.
	// If we've got a small adjustment off our abs yaw, use that. 
	// Otherwise, use our abs yaw.
	float flHeadYaw = GetPoseParameter( m_nPoseHeadYaw );
	if ( fabs( flHeadYaw ) < 20 )
	{
		m_flInteractionYaw = flHeadYaw;
	}
	else
	{
 		m_flInteractionYaw = GetAbsAngles().y;
	}
	*/
}

void CNPC_CrabSynth::RelaxAim( )
{
	// MaintainLookTargets() handles the head poses when there's no enemy
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char* CNPC_CrabSynth::GetSquadSlotDebugName( int iSquadSlot )
{
	switch( iSquadSlot )
	{
	case SQUAD_SLOT_ATTACK_OCCLUDER:	return "SQUAD_SLOT_ATTACK_OCCLUDER";	
		break;
	case SQUAD_SLOT_GRENADE1:			return "SQUAD_SLOT_GRENADE1";
		break;
	}

	return BaseClass::GetSquadSlotDebugName( iSquadSlot );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_crabsynth, CNPC_CrabSynth )

	DECLARE_CONDITION( COND_CRABSYNTH_SHOULD_PATROL )
	DECLARE_CONDITION( COND_CRABSYNTH_ATTACK_SLOT_AVAILABLE )
	DECLARE_CONDITION( COND_CRABSYNTH_OBSTRUCTED )
	DECLARE_CONDITION( COND_CRABSYNTH_CAN_CHARGE )
	DECLARE_CONDITION( COND_CRABSYNTH_HAS_CHARGE_TARGET )

	DECLARE_TASK( TASK_CRABSYNTH_CHARGE )
	DECLARE_TASK( TASK_CRABSYNTH_GET_PATH_TO_CHARGE_POSITION )
	DECLARE_TASK( TASK_CRABSYNTH_GET_PATH_TO_FORCED_GREN_LOS )
	DECLARE_TASK( TASK_CRABSYNTH_DEFER_SQUAD_GRENADES )
	DECLARE_TASK( TASK_CRABSYNTH_FACE_TOSS_DIR )
	DECLARE_TASK( TASK_CRABSYNTH_CHECK_STATE )

	DECLARE_SQUADSLOT( SQUAD_SLOT_ATTACK_OCCLUDER )
	DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE1 )

	DECLARE_ACTIVITY( ACT_CRABSYNTH_CHARGE_START )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_CHARGE_STOP )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_CHARGE )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_CHARGE_ANTICIPATION )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_CHARGE_HIT )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_CHARGE_CRASH )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_FRUSTRATED )
	DECLARE_ACTIVITY( ACT_CRABSYNTH_LOOK_AROUND )

	DECLARE_ANIMEVENT( AE_CRABSYNTH_SHOOT )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_LAUNCH_GRENADE )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_CLAW_CRUSH )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_CLAW_SWIPE )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_CLAW_STEP_LEFT )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_CLAW_STEP_RIGHT )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_CHARGE_START )
	DECLARE_ANIMEVENT( AE_CRABSYNTH_LAND )

	//=========================================================
	// SCHED_CRABSYNTH_PATROL
	//=========================================================
	DEFINE_SCHEDULE	
	(
		SCHED_CRABSYNTH_PATROL,
		
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						900540" 
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_REASONABLE			0"
		"		TASK_WAIT						3"
		"		TASK_WAIT_RANDOM				3"
		"		TASK_CRABSYNTH_CHECK_STATE		0" // Eventually we bring down our gun
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_CRABSYNTH_PATROL" // keep doing it
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)

	//=========================================================
	// SCHED_CRABSYNTH_PATROL_ENEMY
	//=========================================================
	DEFINE_SCHEDULE	
	(
		SCHED_CRABSYNTH_PATROL_ENEMY,
		
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_WAIT_FACE_ENEMY				1" 
		"		TASK_WAIT_FACE_ENEMY_RANDOM			3" 
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)

	//=========================================================
	// SCHED_CRABSYNTH_RANGE_ATTACK1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_RANGE_ATTACK1,
		
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_ANNOUNCE_ATTACK			1"	// 1 = primary attack
		"		TASK_WAIT_RANDOM				0.3"
		"		TASK_RANGE_ATTACK1				0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_HEAVY_DAMAGE"
		"		COND_LIGHT_DAMAGE"
		"		COND_LOW_PRIMARY_AMMO"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_WEAPON_BLOCKED_BY_FRIEND"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_GIVE_WAY"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		""
	)

	//=========================================================
	// SCHED_CRABSYNTH_ESTABLISH_LINE_OF_FIRE
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_CRABSYNTH_ESTABLISH_LINE_OF_FIRE,
	
		"	Tasks "
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_ESTABLISH_LINE_OF_FIRE"
		"		TASK_SET_TOLERANCE_DISTANCE		128"
		"		TASK_GET_PATH_TO_ENEMY_LKP_LOS	0"
		"		TASK_SPEAK_SENTENCE				1"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
		"	"
		"	Interrupts "
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		//"		COND_CAN_RANGE_ATTACK1"
		//"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_HEAVY_DAMAGE"
	)

	//=========================================================
	// SCHED_CRABSYNTH_TAKE_COVER_FROM_BEST_SOUND
	//
	//	hide from the loudest sound source (to run from grenade)
	//=========================================================
	DEFINE_SCHEDULE	
	(
		SCHED_CRABSYNTH_TAKE_COVER_FROM_BEST_SOUND,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CRABSYNTH_RUN_AWAY_FROM_BEST_SOUND"
		"		 TASK_STOP_MOVING					0"
		"		 TASK_FIND_COVER_FROM_BEST_SOUND	0"
		"		 TASK_RUN_PATH						0"
		"		 TASK_WAIT_FOR_MOVEMENT				0"
		"		 TASK_REMEMBER						MEMORY:INCOVER"
		"		 TASK_FACE_REASONABLE				0"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_CRABSYNTH_RUN_AWAY_FROM_BEST_SOUND,

		"	Tasks"
		//"		 TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_COWER"
		"		 TASK_GET_PATH_AWAY_FROM_BEST_SOUND		600"
		"		 TASK_RUN_PATH_TIMED					2"
		"		 TASK_STOP_MOVING						0"
		""
		"	Interrupts"
	)

	//=========================================================
	// SCHED_COMBINE_SUPPRESS
	//=========================================================
	DEFINE_SCHEDULE	
	(
	SCHED_CRABSYNTH_SUPPRESS,
	
	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_ENEMY				0"
	"		TASK_RANGE_ATTACK1			0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
	"		COND_COMBINE_NO_FIRE"
	"		COND_WEAPON_BLOCKED_BY_FRIEND"
	)

	//=========================================================
	// SCHED_COMBINE_PRESS_ATTACK
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_CRABSYNTH_PRESS_ATTACK,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CRABSYNTH_ESTABLISH_LINE_OF_FIRE"
		"		TASK_SET_TOLERANCE_DISTANCE		200"
		"		TASK_GET_PATH_TO_ENEMY_LKP		0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_LOW_PRIMARY_AMMO"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
	)

	//=========================================================
	// SCHED_CRABSYNTH_WAIT_IN_COVER
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_WAIT_IN_COVER,
	
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
		"		TASK_WAIT_FACE_ENEMY			1"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_CRABSYNTH_ATTACK_SLOT_AVAILABLE"
	)

	//=========================================================
	// SCHED_CRABSYNTH_TAKE_COVER1
	//=========================================================
	DEFINE_SCHEDULE	
	(
		SCHED_CRABSYNTH_TAKE_COVER1,
		
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_CRABSYNTH_TAKECOVER_FAILED"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT					0.2"
		"		TASK_FIND_COVER_FROM_ENEMY	0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_REMEMBER				MEMORY:INCOVER"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_CRABSYNTH_WAIT_IN_COVER"
		""
		"	Interrupts"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_TAKECOVER_FAILED,
		
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		""
		"	Interrupts"
	)

	//=========================================================
	// SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY
	//=========================================================
	DEFINE_SCHEDULE	
	(
		SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY,
		
		"	Tasks"
		"		TASK_STOP_MOVING							0"
		"		TASK_STORE_ENEMY_POSITION_IN_SAVEPOSITION	0"
		"		TASK_FIND_BACKAWAY_FROM_SAVEPOSITION		0"
		"		TASK_RUN_PATH_TIMED							5.0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_FACE_IDEAL_YAW,
		
		"	Tasks"
		"		TASK_FACE_IDEAL				0"
		"	"
		"	Interrupts"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_MOVE_TO_MELEE,
		
		"	Tasks"
		"		TASK_STORE_ENEMY_POSITION_IN_SAVEPOSITION	0"
		"		TASK_GET_PATH_TO_SAVEPOSITION				0"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		 "	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_MELEE_ATTACK1"
	)

	//=========================================================
	// Mapmaker forced grenade throw
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_FORCED_GRENADE_THROW,
		
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_CRABSYNTH_FACE_TOSS_DIR		0"
		"		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
		"		TASK_CRABSYNTH_DEFER_SQUAD_GRENADES	0"
		""
		"	Interrupts"
	)
	
	//=========================================================
	// Move to LOS of the mapmaker's forced grenade throw target
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_MOVE_TO_FORCED_GREN_LOS,
		
		"	Tasks "
		"		TASK_SET_TOLERANCE_DISTANCE					48"
		"		TASK_CRABSYNTH_GET_PATH_TO_FORCED_GREN_LOS	0"
		"		TASK_SPEAK_SENTENCE							1"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		"	"
		"	Interrupts "
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_HEAVY_DAMAGE"
	)
	
	//=========================================================
	// 	SCHED_CRABSYNTH_RANGE_ATTACK2	
	//
	//	secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
	//	combines's grenade toss requires the enemy be occluded.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_RANGE_ATTACK2,
		
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_CRABSYNTH_FACE_TOSS_DIR		0"
		"		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
		"		TASK_CRABSYNTH_DEFER_SQUAD_GRENADES		0"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_CRABSYNTH_WAIT_IN_COVER"	// don't run immediately after throwing grenade.
		""
		"	Interrupts"
	)

	//=========================================================
	// SCHED_CRABSYNTH_GRENADE_COVER1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_GRENADE_COVER1,
		
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_FIND_COVER_FROM_ENEMY			99"
		"		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY	384"
		"		TASK_RANGE_ATTACK2 					0"
		"		TASK_CLEAR_MOVE_WAIT				0"
		"		TASK_RUN_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_CRABSYNTH_WAIT_IN_COVER"
		""
		"	Interrupts"
	)
	
	//=========================================================
	// SCHED_CRABSYNTH_TOSS_GRENADE_COVER1
	//
	//	 drop grenade then run to cover.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_TOSS_GRENADE_COVER1,
		
		"	Tasks"
		"		TASK_FACE_ENEMY						0"
		"		TASK_RANGE_ATTACK2 					0"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		""
		"	Interrupts"
	)

	//=========================================================
	// Crab Synth attacks a prop
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_ATTACK_TARGET,
	
		"	Tasks"
		"		TASK_GET_PATH_TO_TARGET		0"
		"		TASK_MOVE_TO_TARGET_RANGE	50"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_TARGET			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_MELEE_ATTACK2"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
	)

	//=========================================================
	// SCHED_CRABSYNTH_ROAR
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_ROAR,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_CRABSYNTH_FRUSTRATED"
		""
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
	)

	//=========================================================
	// SCHED_CRABSYNTH_CANT_ATTACK
	//=========================================================
	DEFINE_SCHEDULE
	( 
		SCHED_CRABSYNTH_CANT_ATTACK,

		"	Tasks"
		"		TASK_SET_ROUTE_SEARCH_TIME		2"	// Spend 5 seconds trying to build a path if stuck
		"		TASK_GET_PATH_TO_RANDOM_NODE	1024"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_WAIT_PVS					0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_HEAVY_DAMAGE"
	)

	//=========================================================
	// SCHED_CRABSYNTH_MELEE_ATTACK1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_MELEE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_ENEMY_OCCLUDED"
	)

	//=========================================================
	// SCHED_CRABSYNTH_CHARGE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_CHARGE,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_FACE_ENEMY						0"
		"		TASK_CRABSYNTH_CHARGE				0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"

		// These are deliberately left out so they can be detected during the 
		// charge Task and correctly play the charge stop animation.
		//"		COND_NEW_ENEMY"
		//"		COND_ENEMY_DEAD"
		//"		COND_LOST_ENEMY"
	)

	//=========================================================
	// SCHED_CRABSYNTH_CHARGE_CANCEL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_CHARGE_CANCEL,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_CRABSYNTH_CHARGE_STOP"
		""
		"	Interrupts"
	)
			
	//=========================================================
	// SCHED_CRABSYNTH_CHARGE_TARGET
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_CHARGE_TARGET,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_CRABSYNTH_CHARGE			0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_ENEMY_DEAD"
		"		COND_HEAVY_DAMAGE"
	)
			
	//=========================================================
	// SCHED_CRABSYNTH_FIND_CHARGE_POSITION
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CRABSYNTH_FIND_CHARGE_POSITION,

		"	Tasks"
		"		TASK_CRABSYNTH_GET_PATH_TO_CHARGE_POSITION		0"
		"		TASK_RUN_PATH									0"
		"		TASK_WAIT_FOR_MOVEMENT							0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_GIVE_WAY"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
	)
	
AI_END_CUSTOM_NPC()

//-----------------------------------------------------------------------------
// 
// Crab Grenade
// 
//-----------------------------------------------------------------------------

#define CRAB_GRENADE_BLIP_FREQUENCY			0.5f
#define CRAB_GRENADE_BLIP_FAST_FREQUENCY	0.1f

#define CRAB_GRENADE_GRACE_TIME_AFTER_PICKUP 1.5f
#define CRAB_GRENADE_WARN_TIME 1.5f

ConVar sk_plr_dmg_crabgrenade	( "sk_plr_dmg_crabgrenade","150");
ConVar sk_npc_dmg_crabgrenade	( "sk_npc_dmg_crabgrenade","100");
ConVar sk_crabgrenade_radius	( "sk_crabgrenade_radius", "300");

class CCrabGrenade : public CBaseGrenade
{
	DECLARE_CLASS( CCrabGrenade, CBaseGrenade );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
					
	//~CCrabGrenade( void );

public:
	void	Spawn( void );
	void	OnRestore( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	void	CreateEffects( void );
	void	SetTimer( float detonateDelay, float warnDelay );
	void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	int		OnTakeDamage( const CTakeDamageInfo &inputInfo );
	void	BlipSound() { EmitSound( "CrabGrenade.Blip" ); }
	void	DelayThink();
	void	Explode( trace_t *pTrace, int bitsDamageType );
	void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	void	SetPunted( bool punt ) { m_punted = punt; }
	bool	WasPunted( void ) const { return m_punted; }

	void	InputSetTimer( inputdata_t &inputdata );

protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;

	float	m_flNextBlipTime;
	bool	m_inSolid;
	bool	m_punted;
	int		m_spriteTexture;

	CNetworkVar( float, m_flStartTime );
	CNetworkVar( float, m_flClientDetonateTime ); // Because m_flDetonateTime is only set on client in MP
};

LINK_ENTITY_TO_CLASS( npc_crab_grenade, CCrabGrenade );

BEGIN_DATADESC( CCrabGrenade )

	// Fields
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flNextBlipTime, FIELD_TIME ),
	DEFINE_FIELD( m_inSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_punted, FIELD_BOOLEAN ),
	
	// Function Pointers
	DEFINE_THINKFUNC( DelayThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTimer", InputSetTimer ),

END_DATADESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CCrabGrenade, DT_CrabGrenade )

	SendPropFloat( SENDINFO( m_flStartTime ) ),
	SendPropFloat( SENDINFO( m_flClientDetonateTime ) ),

END_SEND_TABLE()

void CCrabGrenade::Spawn( void )
{
	Precache( );

	SetModel( STRING( GetModelName() ) );

	if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() )
	{
		m_flDamage		= sk_plr_dmg_crabgrenade.GetFloat();
		m_DmgRadius		= sk_crabgrenade_radius.GetFloat();
	}
	else
	{
		m_flDamage		= sk_npc_dmg_crabgrenade.GetFloat();
		m_DmgRadius		= sk_crabgrenade_radius.GetFloat();
	}

	m_takedamage	= DAMAGE_YES;
	m_iHealth		= 1;

	SetSize( -Vector(4,4,4), Vector(4,4,4) );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	CreateVPhysics();

	BlipSound();
	m_flNextBlipTime = gpGlobals->curtime + CRAB_GRENADE_BLIP_FREQUENCY;

	AddSolidFlags( FSOLID_NOT_STANDABLE );

	m_punted			= false;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrabGrenade::OnRestore( void )
{
	// If we were primed and ready to detonate, put FX on us.
	if (m_flDetonateTime > 0)
		CreateEffects();

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrabGrenade::CreateEffects( void )
{
	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate( "sprites/light_glow03.vmt", GetLocalOrigin(), false );

	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->SetParent( this );
		m_pMainGlow->SetTransparency( kRenderTransAdd, 70, 245, 168, 130, kRenderFxNone );
		m_pMainGlow->SetScale( 0.25f );
	}

	// Start up the eye trail
	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "sprites/laser.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->SetParent( this );
		m_pGlowTrail->SetTransparency( kRenderTransAdd, 70, 245, 168, 130, kRenderFxNone );
		m_pGlowTrail->SetStartWidth( 8.0f );
		m_pGlowTrail->SetLifeTime( 0.75f );
	}
}

bool CCrabGrenade::CreateVPhysics()
{
	// Create the object in the physics system
	//VPhysicsInitNormal( SOLID_BBOX, 0, false );
	SetSolid( SOLID_BBOX );
	SetCollisionBounds( -Vector( 11, 11, 11 ), Vector( 11, 11, 11 ) );
	objectparams_t params = g_PhysDefaultObjectParams;
	params.pGameData = static_cast<void *>(this);

	int nMaterialIndex = physprops->GetSurfaceIndex("slime");
	IPhysicsObject *pPhysicsObject = physenv->CreateSphereObject( 4.0f, nMaterialIndex, GetAbsOrigin(), GetAbsAngles(), &params, false );
	if (pPhysicsObject)
	{
		VPhysicsSetObject( pPhysicsObject );
		SetMoveType( MOVETYPE_VPHYSICS );
		pPhysicsObject->Wake();
	}

	return true;
}

void CCrabGrenade::Precache( void )
{
	if ( GetModelName() == NULL_STRING )
		SetModelName( MAKE_STRING( "models/weapons/crabgrenade.mdl" ) );

	PrecacheModel( STRING( GetModelName() ) );

	PrecacheScriptSound( "CrabGrenade.Blip" );
	PrecacheScriptSound( "CrabGrenade.Explode" );

	PrecacheModel( "sprites/light_glow03.vmt" );
	PrecacheModel( "sprites/laser.vmt" );

	m_spriteTexture = PrecacheModel( "sprites/lgtning.vmt" );

	UTIL_PrecacheOther( "concussiveblast" );

	BaseClass::Precache();
}

void CCrabGrenade::SetTimer( float detonateDelay, float warnDelay )
{
	m_flDetonateTime = gpGlobals->curtime + detonateDelay;
	m_flWarnAITime = gpGlobals->curtime + warnDelay;
	SetThink( &CCrabGrenade::DelayThink );
	SetNextThink( gpGlobals->curtime );

	m_flClientDetonateTime = m_flDetonateTime;
	m_flStartTime = gpGlobals->curtime;
	m_bIsLive = true;

	CreateEffects();
}

void CCrabGrenade::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	SetThrower( pPhysGunUser );

	SetPunted( true );

	BaseClass::OnPhysGunPickup( pPhysGunUser, reason );
}

void CCrabGrenade::DelayThink()
{
	if( gpGlobals->curtime > m_flDetonateTime )
	{
		Detonate();
		return;
	}

	if( !m_bHasWarnedAI && gpGlobals->curtime >= m_flWarnAITime )
	{
		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this );
		m_bHasWarnedAI = true;
	}
	
	if( gpGlobals->curtime > m_flNextBlipTime )
	{
		BlipSound();
		
		if( m_bHasWarnedAI )
		{
			m_flNextBlipTime = gpGlobals->curtime + CRAB_GRENADE_BLIP_FAST_FREQUENCY;
		}
		else
		{
			m_flNextBlipTime = gpGlobals->curtime + CRAB_GRENADE_BLIP_FREQUENCY;
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}

extern void TE_ConcussiveExplosion( IRecipientFilter &filter, float delay,
	const Vector *pos, float scale, int radius, int magnitude, const Vector *normal );

void CCrabGrenade::Explode( trace_t *pTrace, int bitsDamageType )
{
	SetModelName( NULL_STRING );//invisible
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->fraction != 1.0 )
	{
		SetAbsOrigin( pTrace->endpos + (pTrace->plane.normal * 0.6) );
	}

	CSoundEnt::InsertSound ( SOUND_COMBAT, GetAbsOrigin(), BASEGRENADE_EXPLOSION_VOLUME, 3.0 );

	//Create a concussive explosion
	CPASFilter filter( GetAbsOrigin() );

	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );
	TE_ConcussiveExplosion( filter, 0.0,
		&GetAbsOrigin(),//position
		1.0f,	//scale
		256*2.5f,	//radius
		175*5.0f,	//magnitude
		&vecForward );	//normal
	
	int	colorRamp = random->RandomInt( 128, 255 );

	//Shockring
	CBroadcastRecipientFilter filter2;
	te->BeamRingPoint( filter2, 0, 
		GetAbsOrigin(),	//origin
		16,			//start radius
		300*2.5f,	//end radius
		m_spriteTexture, //texture
		0,			//halo index
		0,			//start frame
		2,			//framerate
		0.3f,		//life
		128,		//width
		16,			//spread
		0,			//amplitude
		colorRamp,	//r
		colorRamp,	//g
		255,		//g
		24,			//a
		128			//speed
		);

	RadiusDamage( CTakeDamageInfo( this, GetThrower(), m_flDamage, DMG_BLAST | DMG_DISSOLVE ), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL );

	UTIL_DecalTrace( pTrace, "Scorch" );

	EmitSound( "CrabGrenade.Explode" );

	m_OnDetonate.FireOutput(GetThrower(), this);
	m_OnDetonate_OutPosition.Set(GetAbsOrigin(), GetThrower(), this);

	SetThink( &CBaseGrenade::SUB_Remove );
	SetTouch( NULL );
	SetSolid( SOLID_NONE );
	
	AddEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );

	SetNextThink( gpGlobals->curtime + 0.1 );
}

void CCrabGrenade::SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		pPhysicsObject->AddVelocity( &velocity, &angVelocity );
	}
}

int CCrabGrenade::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	// Manually apply vphysics because BaseCombatCharacter takedamage doesn't call back to CBaseEntity OnTakeDamage
	VPhysicsTakeDamage( inputInfo );

	// Grenades only suffer blast damage and burn damage.
	if( !(inputInfo.GetDamageType() & (DMG_BLAST|DMG_BURN) ) )
		return 0;

	return BaseClass::OnTakeDamage( inputInfo );
}

void CCrabGrenade::InputSetTimer( inputdata_t &inputdata )
{
	SetTimer( inputdata.value.Float(), inputdata.value.Float() - CRAB_GRENADE_WARN_TIME );
}

CBaseGrenade *Crabgrenade_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer )
{
	// Don't set the owner here, or the player can't interact with grenades he's thrown
	CCrabGrenade *pGrenade = (CCrabGrenade *)CBaseEntity::Create( "npc_crab_grenade", position, angles, pOwner );
	
	pGrenade->SetTimer( timer, timer - CRAB_GRENADE_WARN_TIME );
	pGrenade->SetVelocity( velocity, angVelocity );
	pGrenade->SetThrower( ToBaseCombatCharacter( pOwner ) );
	pGrenade->m_takedamage = DAMAGE_EVENTS_ONLY;;

	return pGrenade;
}
