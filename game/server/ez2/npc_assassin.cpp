//=============================================================================//
//
// Purpose:		Combine assassins restored in Source 2013. Created mostly from scratch
//				as a new Combine soldier variant focused on stealth, flanking, and fast
//				movements. Initially inspired by Black Mesa's female assassins.
// 
//				This was originally created for Entropy : Zero 2.
// 
//				--------------------------------------------------------------
// 
//				This code originates from a replication of Black Mesa's female assassins
//				for a mod called Reversion Catalyst. I've kept its code open-source so
//				that it can be used or viewed by anyone:
// 
//	https://github.com/Blixibon/source-sdk-2013/blob/reversioncatalyst/main/sp/src/game/server/reversioncatalyst/npc_bm_human_assassin.cpp
// 
//				Some components (mainly the vantage point perching and the preference to
//				pathfind away from enemies) come from the original npc_assassin.cpp
//				included in the SDK.
// 
//				Unlike the original, this version of the Combine assassin NPC can cloak
//				and shoot at enemies while moving, similar to Black Mesa's assassins.
//				However, since this NPC is based directly on Combine soldier AI, it can
//				(and will) do anything a Combine soldier can do, including throw grenades
//				and use slotted squad tactics. Assassins can also seamlessly integrate with
//				regular soldier squads if needed. Entropy : Zero 2's changes to Combine soldiers
//				apply to assassins as well, additionally giving them the ability to deploy manhacks,
//				join the player's squad, and destroy obstructions.
// 
//				A variety of inputs, outputs, and keyvalues are available to adjust and
//				monitor an assassin's behavior. They use dual pistols by default, but they
//				can be given any weapon that a Combine soldier can use. See CBaseHLCombatWeapon
//				and CWeaponPistol/CWeapon357 for examples on how dual weapons work.
// 
//				--------------------------------------------------------------
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "ammodef.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "npc_assassin.h"
#include "npcevent.h"
#include "ai_squad.h"
#include "ai_moveprobe.h"
#include "collisionutils.h"
#include "RagdollBoogie.h"
#include "IEffects.h"
#include "ai_interactions.h"
#include "basehlcombatweapon_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_assassin_health( "sk_assassin_health", "125" );
ConVar	sk_assassin_kick( "sk_assassin_kick", "15" );

ConVar	sk_assassin_jump_rise( "sk_assassin_jump_rise", "224" );
ConVar	sk_assassin_jump_distance( "sk_assassin_jump_distance", "420" );
ConVar	sk_assassin_jump_drop( "sk_assassin_jump_drop", "756" );

ConVar	sk_assassin_flip_hratio( "sk_assassin_flip_hratio", "256" );
ConVar	sk_assassin_flip_cooldown( "sk_assassin_flip_cooldown", "10" );
ConVar	sk_assassin_flip_min_dist( "sk_assassin_flip_min_dist", "50" );
ConVar	sk_assassin_flip_max_dist( "sk_assassin_flip_max_dist", "150" );
ConVar	sk_assassin_flip_dist_threshold( "sk_assassin_flip_dist_threshold", "16" );

ConVar	sk_assassin_dodge_warning( "sk_assassin_dodge_warning", "1.1" );
ConVar	sk_assassin_dodge_warning_width( "sk_assassin_dodge_warning_width", "180" );
ConVar	sk_assassin_dodge_warning_cone( "sk_assassin_dodge_warning_cone", ".5" );

ConVar	sk_assassin_cloak_speed( "sk_assassin_cloak_speed", "0.05" );
ConVar	sk_assassin_cloak_time_min( "sk_assassin_cloak_time_min", "20" );
ConVar	sk_assassin_cloak_time_max( "sk_assassin_cloak_time_max", "30" );
ConVar	sk_assassin_cloak_cooldown( "sk_assassin_cloak_cooldown", "10" );
ConVar	sk_assassin_cloak_invisible_to_npcs_distance( "sk_assassin_cloak_invisible_to_npcs_distance", "2000" );
ConVar	sk_assassin_cloak_invisible_to_npcs_cancel_distance( "sk_assassin_cloak_invisible_to_npcs_cancel_distance", "32", FCVAR_NONE, "NPCs will ignore cloaking at this distance" );
ConVar	sk_assassin_cloak_max_move_speed( "sk_assassin_cloak_max_move_speed", "250", FCVAR_NONE, "Beyond this speed, the cloak will slightly fade" );
ConVar	sk_assassin_cloak_cone_nerf_base( "sk_assassin_cloak_cone_nerf_base", "0.7" );
ConVar	sk_assassin_cloak_cone_nerf_multiplier( "sk_assassin_cloak_cone_nerf_multiplier", "2.0" );
ConVar	sk_assassin_cloak_cone_nerf_bias( "sk_assassin_cloak_cone_nerf_bias", "0.75" );
ConVar	sk_assassin_decloak_speed( "sk_assassin_decloak_speed", "0.075" );
ConVar	sk_assassin_decloak_damage( "sk_assassin_decloak_damage", "10" );
ConVar	sk_assassin_decloak_flip( "sk_assassin_decloak_flip", "9" );
ConVar	sk_assassin_decloak_melee( "sk_assassin_decloak_melee", "4" );
ConVar	sk_assassin_decloak_shoot( "sk_assassin_decloak_shoot", "7.5" );
ConVar	sk_assassin_force_cloak( "sk_assassin_force_cloak", "0", FCVAR_NONE, "Set to 1 to force all assassins to cloak, set to -1 to force them to uncloak" );

ConVar	sk_assassin_vantage_max_dist( "sk_assassin_vantage_max_dist", "2048" );
ConVar	sk_assassin_vantage_min_dist_from_player( "sk_assassin_vantage_min_dist_from_player", "256" );
ConVar	sk_assassin_vantage_min_dist_from_self( "sk_assassin_vantage_min_dist_from_self", "256" );
ConVar	sk_assassin_vantage_min_dist_from_squad( "sk_assassin_vantage_min_dist_from_squad", "128" );
ConVar	sk_assassin_vantage_max_time( "sk_assassin_vantage_max_time", "300" );
ConVar	sk_assassin_vantage_cooldown( "sk_assassin_vantage_cooldown", "45" );

ConVar	g_debug_assassin( "g_debug_assassin", "0" );
ConVar	g_debug_assassin_dodge( "g_debug_assassin_dodge", "0" );

ConVar	npc_assassin_use_new_vantage_type( "npc_assassin_use_new_vantage_type", "1" );

#define ASSASSIN_MAX_HEADSHOT_DISTANCE 1000.0f
#define ASSASSIN_MAX_RANGE	2048.0f //4096.0f

#define TLK_ASSASSIN_CLOAK "TLK_CLOAK"
#define TLK_ASSASSIN_UNCLOAK "TLK_UNCLOAK"

//=========================================================
// Anim Events	
//=========================================================
int AE_PISTOL_FIRE_LEFT;
int AE_PISTOL_FIRE_RIGHT;
int AE_ASSASSIN_KICK_HIT;

//=========================================================
// Assassin activities
//=========================================================
Activity ACT_ASSASSIN_FLIP;
Activity ACT_ASSASSIN_FLIP_DUAL_PISTOLS;
Activity ACT_ASSASSIN_FLIP_PISTOL;
Activity ACT_ASSASSIN_FLIP_RIFLE;

Activity ACT_ASSASSIN_FLIP_LOOP;
Activity ACT_ASSASSIN_FLIP_LOOP_DUAL_PISTOLS;
Activity ACT_ASSASSIN_FLIP_LOOP_PISTOL;
Activity ACT_ASSASSIN_FLIP_LOOP_RIFLE;

Activity ACT_ASSASSIN_ROLL_JUMP;
Activity ACT_ASSASSIN_ROLL_GLIDE;
Activity ACT_ASSASSIN_ROLL_LAND;

Activity ACT_ASSASSIN_KICK;
Activity ACT_ASSASSIN_DODGE_KICK;

// TODO: Make into global activities?
Activity ACT_ARM_DUAL_PISTOLS;
Activity ACT_DISARM_DUAL_PISTOLS;

//-----------------------------------------------------------------------------
// Purpose: Class Constructor
//-----------------------------------------------------------------------------
CNPC_Assassin::CNPC_Assassin( void )
{
	m_bDualWeapons = true;
	m_bUseEyeTrail = true;
	m_bCanCloakDuringAI = true;
	m_iNumGrenades = 10; // Assassins start with more grenades
}

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_assassin, CNPC_Assassin );

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CNPC_Assassin, DT_NPC_Assassin)
	SendPropFloat( SENDINFO( m_flCloakFactor ), 16, 0, 0.0f, 1.0f ),
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Assassin )
	DEFINE_KEYFIELD( m_bDualWeapons, FIELD_BOOLEAN, "DualWeapons" ),
	DEFINE_FIELD( m_hLeftHandGun, FIELD_EHANDLE ),

#ifndef EZ
	DEFINE_FIELD( m_pEyeGlow,	FIELD_EHANDLE ),
#endif
	DEFINE_FIELD( m_pEyeTrail,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_flEyeBrightness,	FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_bUseEyeTrail, FIELD_BOOLEAN, "UseEyeTrail" ),

	DEFINE_FIELD( m_flLastFlipEndTime, FIELD_TIME ),

	DEFINE_INPUT( m_bCanCloakDuringAI, FIELD_BOOLEAN, "SetCloakDuringAI" ),
	DEFINE_INPUT( m_bNoCloakSound, FIELD_BOOLEAN, "SetSuppressCloakSound" ),
	DEFINE_FIELD( m_flMaxCloakTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextCloakTime, FIELD_TIME ),
	DEFINE_FIELD( m_bCloaking, FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_bStartCloaked, FIELD_BOOLEAN, "StartCloaked" ),

	DEFINE_FIELD( m_flNextPerchTime, FIELD_TIME ),
	DEFINE_FIELD( m_flEndPerchTime, FIELD_TIME ),

	DEFINE_INPUTFUNC( FIELD_VOID, "StartCloaking", InputStartCloaking ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopCloaking", InputStopCloaking ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnEyeTrail", InputTurnOnEyeTrail ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffEyeTrail", InputTurnOffEyeTrail ),

	DEFINE_OUTPUT( m_OnDualWeaponDrop, "OnDualWeaponDrop" ),
	DEFINE_OUTPUT( m_OnStartCloaking, "OnStartCloaking" ),
	DEFINE_OUTPUT( m_OnStopCloaking, "OnStopCloaking" ),
	DEFINE_OUTPUT( m_OnFullyCloaked, "OnFullyCloaked" ),
	DEFINE_OUTPUT( m_OnFullyUncloaked, "OnFullyUncloaked" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Assassin::Precache( void )
{
	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/combine_assassin.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	PrecacheModel( "sprites/bluelaser1.vmt" );

	PrecacheScriptSound( "NPC_Assassin.Jump" );
	PrecacheScriptSound( "NPC_Assassin.Land" );
	PrecacheScriptSound( "NPC_Assassin.Cloak" );
	PrecacheScriptSound( "NPC_Assassin.Uncloak" );

	//PrecacheScriptSound( "DoSpark" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Assassin::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	SetHealth( sk_assassin_health.GetFloat() );
	SetMaxHealth( sk_assassin_health.GetFloat() );
	SetKickDamage( sk_assassin_kick.GetFloat() );

	if (m_bStartCloaked)
	{
		m_bCloaking = true;
		m_flCloakFactor = 1.0f;
		m_flMaxCloakTime = FLT_MAX;
	}

	BaseClass::Spawn();

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );
	CapabilitiesAdd( bits_CAP_MOVE_CLIMB | bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::StartEye(void)
{
#ifdef EZ
	BaseClass::StartEye();
#else
	// TODO: Create eye glow for non-E:Z contexts
#endif

	StartEyeTrail();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::KillSprites(float flDelay)
{
	BaseClass::KillSprites( flDelay );

	StopEyeTrail( flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::StartEyeTrail()
{
	if ( m_pEyeTrail == NULL && m_bUseEyeTrail )
	{
		// Start up the eye trail
		m_pEyeTrail = CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );

		if (m_pEyeGlow)
		{
			// Base parameters directly off of our eye glow
			m_pEyeTrail->SetAttachment( this, m_pEyeGlow->m_nAttachment );

			color32 renderClr = m_pEyeGlow->GetRenderColor();
			m_pEyeTrail->SetTransparency( kRenderTransAdd, renderClr.r, renderClr.g, renderClr.b, m_flEyeBrightness, kRenderFxNone);
		}
		else
		{
			// Fall back to default for no eye glow
			m_pEyeTrail->SetAttachment( this, LookupAttachment( "eyes" ) );
			m_pEyeTrail->SetTransparency( kRenderTransAdd, 255, 0, 0, 200, kRenderFxNone );
		}

		m_pEyeTrail->SetStartWidth( 8.0f );
		m_pEyeTrail->SetLifeTime( 0.75f );

		m_flEyeBrightness = m_pEyeGlow->GetBrightness();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::StopEyeTrail( float flDelay )
{
	if (m_pEyeTrail)
	{
		m_pEyeTrail->FadeAndDie( flDelay );
		m_pEyeTrail = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Assassin::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
	const float MAX_JUMP_RISE = sk_assassin_jump_rise.GetFloat();
	const float MAX_JUMP_DISTANCE = sk_assassin_jump_distance.GetFloat();
	const float MAX_JUMP_DROP = sk_assassin_jump_drop.GetFloat();

	return BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Assassin::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->event == AE_PISTOL_FIRE_LEFT)
	{
		if (m_hLeftHandGun)
		{
			int sequence = m_hLeftHandGun->SelectWeightedSequence( ACT_RANGE_ATTACK1 );
			if (sequence != ACTIVITY_NOT_AVAILABLE)
			{
				m_hLeftHandGun->SetSequence( sequence );
				m_hLeftHandGun->SetCycle( 0 );
				m_hLeftHandGun->ResetSequenceInfo();
			}
		}
		Weapon_HandleAnimEvent( pEvent );
		return;
	}
	else if (pEvent->event == AE_PISTOL_FIRE_RIGHT)
	{
		Weapon_HandleAnimEvent( pEvent );
		return;
	}
	else if ( pEvent->event == AE_ASSASSIN_KICK_HIT )
	{
		KickAttack( false );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Causes the assassin to prefer to run away, rather than towards her target
//-----------------------------------------------------------------------------
bool CNPC_Assassin::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	if ( GetEnemy() == NULL )
		return true;

	bool bResult = BaseClass::MovementCost( moveType, vecStart, vecEnd, pCost );

	Vector	moveDir = ( vecEnd - vecStart );
	VectorNormalize( moveDir );

	Vector	enemyDir = ( GetEnemy()->GetAbsOrigin() - vecStart );
	VectorNormalize( enemyDir );

	// If we're moving towards our enemy, then the cost is based on how much we're cloaked
	// (the more cloaked we are, the closer we can get to our enemy)
	if ( DotProduct( enemyDir, moveDir ) > 0.5f && m_flCloakFactor != 1.0f )
	{
		*pCost *= ((1.0f - m_flCloakFactor) * 16.0f);
		bResult = true;
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::CanBeSeenBy( CAI_BaseNPC *pNPC )
{
	// NPCs shouldn't see cloaked assassins if they're too far away and haven't attacked in the past half second
	float flDist = EnemyDistance( pNPC );
	if (m_flCloakFactor > 0.0f && gpGlobals->curtime - GetLastAttackTime() > 0.5f && flDist > (sk_assassin_cloak_invisible_to_npcs_distance.GetFloat() * (1.0f - m_flCloakFactor)) && flDist > sk_assassin_cloak_invisible_to_npcs_cancel_distance.GetFloat())
		return false;

	return BaseClass::CanBeSeenBy( pNPC );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::CanBeAnEnemyOf( CBaseEntity *pEnemy )
{ 
	// If we're *completely* cloaked, and the enemy has other targets to choose from, then make them acquire someone else
	if ( m_flCloakFactor == 1.0f && pEnemy->IsNPC() && !CanBeSeenBy( pEnemy->MyNPCPointer() ) && pEnemy->MyNPCPointer()->GetEnemies()->NumEnemies() > 1)
		return false;

	return BaseClass::CanBeAnEnemyOf( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::PrescheduleThink( void )
{
	if ( (m_bCloaking || sk_assassin_force_cloak.GetInt()) && sk_assassin_force_cloak.GetInt() != -1 )
	{
		float flDest = 1.0f;
		float flSpeed = sk_assassin_cloak_speed.GetFloat();

		float flVelocity = GetInstantaneousVelocity();
		if (flVelocity > sk_assassin_cloak_max_move_speed.GetFloat())
		{
			flDest = sk_assassin_cloak_max_move_speed.GetFloat() / flVelocity;
		}

		// Damage
		if (gpGlobals->curtime - GetLastDamageTime() <= 2.0f)
		{
			flDest *= sk_assassin_decloak_damage.GetFloat() / 10.0f;
		}
		// Acrobatics
		if (V_strncmp(GetActivityName( GetActivity() ), "ACT_ASSASSIN_FLIP", 18) == 0 ||
			GetIdealActivity() == ACT_GLIDE)
		{
			flDest *= sk_assassin_decloak_flip.GetFloat() / 10.0f;
		}
		// Melee
		if (GetActivity() == ACT_MELEE_ATTACK1 || GetActivity() == ACT_MELEE_ATTACK2 || GetActivity() == ACT_DUCK_DODGE || GetActivity() == ACT_ASSASSIN_DODGE_KICK)
		{
			flDest *= sk_assassin_decloak_melee.GetFloat() / 10.0f;
		}
		// Shoot
		if (gpGlobals->curtime - GetLastAttackTime() <= 1.0f)
		{
			flDest *= sk_assassin_decloak_shoot.GetFloat() / 10.0f;
		}

		// Faster when decloaking
		if (flDest < m_flCloakFactor)
			flSpeed = sk_assassin_decloak_speed.GetFloat();

		if (flDest != m_flCloakFactor)
		{
			m_flCloakFactor = UTIL_Approach( flDest, m_flCloakFactor, flSpeed );

			if (m_flCloakFactor == 1.0f)
				m_OnFullyCloaked.FireOutput( this, this );
		}
	}
	else
	{
		if (m_flCloakFactor != 0.0f)
		{
			m_flCloakFactor = UTIL_Approach( 0.0f, m_flCloakFactor, sk_assassin_decloak_speed.GetFloat() );

			if (m_flCloakFactor == 0.0f)
				m_OnFullyUncloaked.FireOutput( this, this );
		}
	}

	float flCloakInv = (1.0f - m_flCloakFactor);

	// Set sprite transparency
	byte nSpriteAlpha = (m_flEyeBrightness * flCloakInv);
	if (m_pEyeGlow)
		m_pEyeGlow->SetBrightness( nSpriteAlpha );
	if (m_pEyeTrail)
		m_pEyeTrail->SetBrightness( nSpriteAlpha );

	if (GetActiveWeapon())
	{
		// Set weapon transparency
		byte nWeaponAlpha = (255.0f * flCloakInv);

		GetActiveWeapon()->SetRenderColorA( nWeaponAlpha );
		nWeaponAlpha == 255 ? GetActiveWeapon()->RemoveEffects( EF_NOSHADOW ) : GetActiveWeapon()->AddEffects( EF_NOSHADOW );

		if (m_hLeftHandGun)
		{
			m_hLeftHandGun->SetRenderColorA( nWeaponAlpha );
			nWeaponAlpha == 255 ? m_hLeftHandGun->RemoveEffects( EF_NOSHADOW ) : m_hLeftHandGun->AddEffects( EF_NOSHADOW );
		}
	}

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Assassin::TranslateSchedule( int scheduleType )
{
	scheduleType = BaseClass::TranslateSchedule( scheduleType );

	switch( scheduleType )
	{
	case SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE:
		{
			if (HasCondition(COND_ASSASSIN_CLOAK_RETREAT))
			{
				// Just stay in cover
				return HasMemory( bits_MEMORY_INCOVER ) ? SCHED_STANDOFF : SCHED_ASSASSIN_EVADE;
			}

			if (HasMemory( bits_MEMORY_INCOVER ))
			{
				// If we're already in cover, then wait to get cloaked again before establishing LOF
				if (m_bCloaking)
				{
					if (m_flCloakFactor < 1.0f)
					{
						VacateStrategySlot();
						return SCHED_ASSASSIN_WAIT_FOR_CLOAK;
					}
				}
				else if (m_bCanCloakDuringAI)
				{
					VacateStrategySlot();
					return SCHED_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE;
				}
			}

			if (m_flNextPerchTime <= gpGlobals->curtime)
			{
				if (CAI_Hint *pHint = FindVantagePoint())
				{
					// If there's a vantage point available, go to it
					SetHintNode( pHint );
					pHint->NPCHandleStartNav( this, true );
					return SCHED_ASSASSIN_GO_TO_VANTAGE_POINT;
				}
			}

			if (gpGlobals->curtime - GetLastDamageTime() <= 10.0f)
			{
				// We've taken damage recently, so try to flank
				return SCHED_ASSASSIN_FLANK_RANDOM;
			}
		} // Fall through
	case SCHED_COMBINE_RANGE_ATTACK1:
		{
			// Don't stop to attack if in view
			if (HasCondition( COND_ASSASSIN_ENEMY_TARGETING_ME ) || gpGlobals->curtime - GetEnemies()->TimeAtFirstHand(GetEnemy()) < 5.0f)
			{
				if (HasMemory( bits_MEMORY_INCOVER ) || ((GetHealth() / GetMaxHealth()) > 0.5f))
				{
					return SCHED_ASSASSIN_FLANK_RANDOM;
				}
				else
				{
					return SCHED_ASSASSIN_EVADE;
				}
			}

			if (!HasCondition(COND_ENEMY_UNREACHABLE))
			{
				// If this is our last enemy, they're low on health, we're in the primary attack slot, and we haven't thrown any grenades recently, then go for the kill
				if (GetEnemies()->NumEnemies() == 1 && GetEnemy()->GetHealth() < m_nMeleeDamage * 2 && HasDualWeapons() && HasStrategySlot(SQUAD_SLOT_ATTACK1) && m_flNextGrenadeCheck < gpGlobals->curtime)
				{
					// Can't melee tiny enemies
					if (GetEnemy()->IsNPC() && GetEnemy()->MyNPCPointer()->GetHullType() == HULL_TINY)
						return SCHED_COMBINE_PRESS_ATTACK;

					return SCHED_COMBINE_MOVE_TO_MELEE;
				}
			}

			// Only return range attack if not from a fallen through case
			if (scheduleType == SCHED_COMBINE_RANGE_ATTACK1)
				return SCHED_ASSASSIN_RANGE_ATTACK1;
		} break;

	case SCHED_PC_MELEE_AND_MOVE_AWAY:
	case SCHED_MELEE_ATTACK1:
		{
			return SCHED_ASSASSIN_MELEE_ATTACK1;
		} break;

	case SCHED_COMBINE_SIGNAL_SUPPRESS:
		{
			// Don't do signals
			return SCHED_COMBINE_SUPPRESS;
		} break;

	case SCHED_COWER:
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			// Avoid falling for grenades
			if (GetEnemy())
				return SCHED_ASSASSIN_EVADE;
		} break;

	case SCHED_COMBINE_PRESS_ATTACK:
	case SCHED_COMBINE_ASSAULT:
		{
			// Do not assault if we're retreating
			if (HasCondition(COND_ASSASSIN_CLOAK_RETREAT))
				return SCHED_ASSASSIN_EVADE;
		} break;

	case SCHED_ALERT_STAND:
	case SCHED_COMBINE_PATROL:
	case SCHED_COMBINE_PATROL_ENEMY:
	case SCHED_COMBINE_WAIT_IN_COVER:
		{
			if (m_flNextPerchTime <= gpGlobals->curtime && (GetState() == NPC_STATE_ALERT || GetState() == NPC_STATE_COMBAT))
			{
				if (CAI_Hint *pHint = FindVantagePoint())
				{
					// If there's a vantage point anywhere, perch there
					SetHintNode( pHint );
					pHint->NPCHandleStartNav( this, true );
					return SCHED_ASSASSIN_GO_TO_VANTAGE_POINT;
				}
			}
		} break;

	case SCHED_DUCK_DODGE:
		{
			if (GetEnemy() && HasDualWeapons() && HasCondition(COND_CAN_MELEE_ATTACK1))
				return SCHED_ASSASSIN_DODGE_KICK;
		} break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Assassin::SelectSchedule()
{
	CheckCloak();

#ifdef EZ2
	if ( HasCondition( COND_ASSASSIN_INCOMING_PROJECTILE ) )
	{
		if ( FVisible( m_hDodgeTarget ) )
		{
			if ( g_debug_assassin_dodge.GetBool() )
			{
				Msg( "Assassin %d try dodge projectile\n", entindex() );
			}
			return SCHED_ASSASSIN_DODGE_FLIP;
		}
	}
#endif

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Assassin::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	// Catch the LOF failure and choose another route to take
	if ( failedSchedule == SCHED_ESTABLISH_LINE_OF_FIRE )
		return SCHED_ASSASSIN_FLANK_RANDOM;

	if ( failedSchedule == SCHED_ASSASSIN_EVADE && GetEnemy() )
	{
		if (!CanBeAnEnemyOf( GetEnemy() ))
		{
			// If I stand very very still, maybe they won't see me
			return SCHED_STANDOFF;
		}

		if (CanGrenadeEnemy())
		{
			// Try throwing a grenade
			return SCHED_COMBINE_TOSS_GRENADE_COVER1;
		}
		else if (EnemyDistance(GetEnemy()) <= 64.0f)
		{
			// Hope for the best
			return SCHED_COMBINE_MOVE_TO_MELEE;
		}
	}
	else if (failedSchedule == SCHED_ASSASSIN_FLANK_FALLBACK)
	{
		// Unable to move, just attack
		if (HasCondition(COND_CAN_RANGE_ATTACK1))
			return SCHED_ASSASSIN_RANGE_ATTACK1;
		else
			return SCHED_COMBINE_MOVE_TO_MELEE;
	}

	if ( failedSchedule == SCHED_ASSASSIN_GO_TO_VANTAGE_POINT )
	{
		if ( GetEnemy() )
		{
			return SCHED_ASSASSIN_FLANK_RANDOM;
		}
		else
		{
			return SCHED_COMBINE_PATROL;
		}
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//---------------------------------------------------------
// Purpose: 
//---------------------------------------------------------
Vector CNPC_Assassin::GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy )
{
	CBaseEntity *pEnemy = GetEnemy();

	// This is copied from CAI_BaseNPC::GetShootEnemyDir to shoot at the head instead
	if ( pEnemy )
	{
		Vector vecEnemyLKP = GetEnemyLKP();
		Vector vecEnemyOffset;

		if (pEnemy->IsNPC())
		{
			float flDist = EnemyDistance( pEnemy );
			if (flDist < ASSASSIN_MAX_HEADSHOT_DISTANCE && flDist > 32.0f)
			{
				// Aim for the head
				vecEnemyOffset = pEnemy->HeadTarget( shootOrigin ) - pEnemy->GetAbsOrigin();
			}
		}
		else
		{
			vecEnemyOffset = pEnemy->BodyTarget( shootOrigin, bNoisy ) - pEnemy->GetAbsOrigin();
		}

		Vector retval = vecEnemyOffset + vecEnemyLKP - shootOrigin;
		VectorNormalize( retval );
		return retval;
	}
	else
	{
		Vector forward;
		AngleVectors( GetLocalAngles(), &forward );
		return forward;
	}
}

extern ConVar ai_lead_time;

//---------------------------------------------------------
// Purpose: 
//---------------------------------------------------------
Vector CNPC_Assassin::GetActualShootPosition( const Vector &shootOrigin )
{
	if ( GetEnemy() && GetEnemy()->IsNPC() )
	{
		float flDist = EnemyDistance( GetEnemy() );
		if (flDist < ASSASSIN_MAX_HEADSHOT_DISTANCE && flDist > 32.0f)
		{
			// Aim for the head
			Vector vecEnemyLKP = GetEnemyLKP();
			Vector vecEnemyOffset = GetEnemy()->HeadTarget( shootOrigin ) - GetEnemy()->GetAbsOrigin();

			// Scale down towards the torso the closer the target is
			if (flDist < 192.0f)
			{
				vecEnemyOffset *= ( ((flDist / 192.0f) * 0.5f) + 0.5f );
			}

			Vector vecTargetPosition = vecEnemyOffset + vecEnemyLKP;

			// lead for some fraction of a second.
			return (vecTargetPosition + ( GetEnemy()->GetSmoothedVelocity() * ai_lead_time.GetFloat() ));
		}
	}

	return BaseClass::GetActualShootPosition( shootOrigin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::KickAttack( bool bLow )
{
	// Does no damage, because damage is applied based upon whether the target can handle the interaction
	CBaseEntity *pHurt = CheckTraceHullAttack( 70, -Vector(16,16,18), Vector(16,16,18), 0, DMG_CLUB );
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter( pHurt );
	if (pBCC)
	{
		Vector forward, up;
		AngleVectors( GetLocalAngles(), &forward, NULL, &up );

		if ( !pBCC->DispatchInteraction( g_interactionCombineBash, NULL, this ) )
		{
			if ( pBCC->IsPlayer() )
			{
				pBCC->ViewPunch( bLow ? QAngle(12,7,0) : QAngle(-12,-7,0) );
				pHurt->ApplyAbsVelocityImpulse( forward * 100 + up * 50 );
			}

			CTakeDamageInfo info( this, this, m_nMeleeDamage, DMG_CLUB );
			CalculateMeleeDamageForce( &info, forward, pBCC->GetAbsOrigin() );
			pBCC->TakeDamage( info );

			EmitSound( "NPC_Assassin.Kick" );
		}
	}
	else
	{
		EmitSound( "NPC_Assassin.KickMiss" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::ShouldFlip()
{
	// Don't flip again for a while
	if (gpGlobals->curtime - m_flLastFlipEndTime <= sk_assassin_flip_cooldown.GetFloat())
		return false;

	// Don't flip too far, or too little
	if (GetNavigator()->GetPathDistToGoal() < sk_assassin_flip_min_dist.GetFloat() || GetNavigator()->GetPathDistToGoal() > sk_assassin_flip_max_dist.GetFloat())
		return false;

	// No need to flip if we're mostly invisible
	if (m_flCloakFactor >= 0.9f)
		return false;

	// Make sure the flip sequence aligns with the path
	float flMoveDist = GetSequenceMoveDist( SelectWeightedSequence( ACT_ASSASSIN_FLIP ) );
	int nAlignment = (((int)roundf(GetNavigator()->GetPathDistToGoal())) % (int)flMoveDist);

	if (nAlignment > sk_assassin_flip_dist_threshold.GetInt() && ((int)flMoveDist) - nAlignment > sk_assassin_flip_dist_threshold.GetFloat())
		return false;

	// If we're taking cover while the enemy is facing me, and the cover isn't far away, start flipping
	if (HasCondition( COND_ASSASSIN_ENEMY_TARGETING_ME ) && IsCurSchedule( SCHED_COMBINE_TAKE_COVER1, false ))
		return true;

	// If we've recently taken damage, start flipping
	if (gpGlobals->curtime - GetLastDamageTime() <= 3.0f)
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::CheckCloak()
{
	if (m_bCanCloakDuringAI && !IsInAScript())
	{
		if (ShouldCloak())
		{
			StartCloaking();

			if (m_bStartCloaked)
			{
				// If we started cloaked, set a time to stop cloaking
				m_flMaxCloakTime = gpGlobals->curtime + RandomFloat( sk_assassin_cloak_time_min.GetFloat(), sk_assassin_cloak_time_max.GetFloat() );
				m_bStartCloaked = false;
			}
		}
		else
		{
			StopCloaking();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::StartCloaking()
{
	if (m_bCloaking)
		return;

	m_bCloaking = true;
	m_flMaxCloakTime = gpGlobals->curtime + RandomFloat( sk_assassin_cloak_time_min.GetFloat(), sk_assassin_cloak_time_max.GetFloat() );

	// Remove any new decals when cloaking
	RemoveAllDecals();

	if (!m_bNoCloakSound)
	{
		EmitSound( "NPC_Assassin.Cloak" );

		if (m_bCanCloakDuringAI && !IsSpeaking() && !IsCurSchedule(SCHED_ASSASSIN_PERCH, false))
		{
			SpeakIfAllowed( TLK_ASSASSIN_CLOAK );
		}
	}

	m_OnStartCloaking.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::StopCloaking()
{
	if (!m_bCloaking)
		return;

	m_bCloaking = false;

	// If we stopped cloaking early, reduce the cooldown
	// If we stopped cloaking late, increase the cooldown
	float flEnergyMult = RemapValClamped( (gpGlobals->curtime - m_flMaxCloakTime), -sk_assassin_cloak_time_max.GetFloat(), sk_assassin_cloak_time_max.GetFloat(), 0.25f, 1.75f );
	m_flNextCloakTime = gpGlobals->curtime + (sk_assassin_cloak_cooldown.GetFloat() * flEnergyMult);
	
	if (!m_bNoCloakSound)
	{
		EmitSound( "NPC_Assassin.Uncloak" );

		if (m_bCanCloakDuringAI && !IsSpeaking() && !IsCurSchedule( SCHED_ASSASSIN_PERCH, false ))
		{
			SpeakIfAllowed( TLK_ASSASSIN_UNCLOAK );
		}
	}

	m_OnStopCloaking.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::ShouldCloak()
{
	// Stop cloaking if we've passed our limit
	if (m_bCloaking && gpGlobals->curtime > m_flMaxCloakTime)
		return false;

	// Don't cloak again for a while
	if (gpGlobals->curtime < m_flNextCloakTime)
		return false;

	// Don't cloak if we're not in combat
	if (GetState() != NPC_STATE_COMBAT)
		return false;

	// UNDONE: Unless we're an ally, don't cloak on easy mode
	//if (!IsPlayerAlly() && GameRules()->IsSkillLevel( SKILL_EASY ))
	//	return false;

	// If the enemy isn't capable of attacking me from afar, only cloak when they're getting close and can reach me
	if (GetEnemy() && GetEnemy()->IsNPC())
	{
		CAI_BaseNPC *pNPC = GetEnemy()->MyNPCPointer();
		if (!pNPC->GetActiveWeapon() && !(pNPC->CapabilitiesGet() & (bits_CAP_INNATE_RANGE_ATTACK1)))
		{
			if (EnemyDistance( pNPC ) > 384.0f || pNPC->HasCondition( COND_ENEMY_UNREACHABLE ))
				return false;
		}
	}

	// Can't order a surrender if the enemy can't see me
	if (HasCondition( COND_COMBINE_CAN_ORDER_SURRENDER ))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_Assassin::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	if ( pWeapon->ClassMatches( gm_isz_class_Pistol ) )
	{
		return WEAPON_PROFICIENCY_PERFECT;
	}
	else if ( pWeapon->ClassMatches( "weapon_pulsepistol" ) )
	{
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//---------------------------------------------------------
// Purpose: 
//---------------------------------------------------------
void CNPC_Assassin::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_ASSASSIN_WAIT_FOR_CLOAK:
		case TASK_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE:
			break;

		case TASK_ASSASSIN_CHECK_CLOAK:
		{
			CheckCloak();
			TaskComplete();
			break;
		}

		case TASK_ASSASSIN_PERCH:
		{
			if (GetHintNode())
			{
				ChainStartTask( TASK_PLAY_HINT_ACTIVITY );
			}
			m_flEndPerchTime = gpGlobals->curtime + sk_assassin_vantage_max_time.GetFloat();
			break;
		}

		case TASK_ASSASSIN_FIND_DODGE_POSITION:
		{
			TaskFindDodgeActivity();
			break;
		}

		case TASK_ASSASSIN_DODGE_FLIP:
		{
			if ( g_debug_assassin_dodge.GetBool() )
			{
				Msg( "Assassin %d dodging\n", entindex() );
			}
			SetActivity( ACT_ASSASSIN_FLIP );
			break;
		}

		case TASK_DEFER_DODGE:
			// Assassins can dodge again sooner
			m_flNextDodgeTime = gpGlobals->curtime + (pTask->flTaskData * 0.25f);
			TaskComplete();
			break;

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Assassin::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_ASSASSIN_WAIT_FOR_CLOAK:
			{
				if ( !m_bCloaking || m_flCloakFactor >= 1.0f )
				{
					TaskComplete();
				}
				break;
			}

		case TASK_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE:
			{
				if (!m_bCanCloakDuringAI)
				{
					TaskComplete();
					break;
				}

				if ( m_flNextCloakTime <= gpGlobals->curtime )
				{
					TaskComplete();
				}
				break;
			}
			
		case TASK_ASSASSIN_PERCH:
		{
			if (!GetHintNode())
			{
				TaskFail(FAIL_NO_HINT_NODE);
			}
			else if ( GetState() == NPC_STATE_COMBAT && GetEnemy() == NULL )
			{
				// Enemy died while we were going to perch; Change state, but stay there and reset timer
				SetState( NPC_STATE_ALERT );
			}

			if (m_flEndPerchTime <= gpGlobals->curtime)
			{
				// Stop perching when we've reached our maximum time
				TaskComplete();
			}

			// Just check cloak and wait for the schedule to be interrupted
			AutoMovement();
			CheckCloak();
			break;
		}

		case TASK_ASSASSIN_DODGE_FLIP:
			{
				AutoMovement();

				if ( IsActivityFinished() )
				{
					TaskComplete();
				}
				break;
			}

		default:
			BaseClass::RunTask( pTask );
			break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
CAI_Hint *CNPC_Assassin::FindVantagePoint()
{
	Assert( GetEnemy() != NULL );

	// Assassins following something (e.g. a commanding player) shouldn't look for a vantage point
	if (GetFollowBehavior().CanSelectSchedule())
		return NULL;

	CHintCriteria	hint;

	// Find a disadvantage node near the player, but away from ourselves
	hint.SetHintType( npc_assassin_use_new_vantage_type.GetBool() ? HINT_TACTICAL_VANTAGE_POINT : HINT_TACTICAL_ENEMY_DISADVANTAGED );
	hint.AddIncludePosition( GetAbsOrigin(), sk_assassin_vantage_max_dist.GetFloat() );
	hint.AddExcludePosition( GetAbsOrigin(), sk_assassin_vantage_min_dist_from_self.GetFloat() );

	Vector vecSearchPos;
	if (GetEnemy())
	{
		vecSearchPos = GetEnemy()->GetAbsOrigin();
		hint.AddExcludePosition( GetEnemy()->GetAbsOrigin(), sk_assassin_vantage_min_dist_from_player.GetFloat() );
	}
	else
	{
		// If there's no enemy, just search for any vantage point nearby
		vecSearchPos = GetAbsOrigin();
	}

	if ( ( m_pSquad != NULL ) && ( m_pSquad->NumMembers() > 1 ) )
	{
		AISquadIter_t iter;
		for ( CAI_BaseNPC *pSquadMember = m_pSquad->GetFirstMember( &iter ); pSquadMember; pSquadMember = m_pSquad->GetNextMember( &iter ) )
		{
			if ( pSquadMember == NULL )
				continue;

			hint.AddExcludePosition( pSquadMember->GetAbsOrigin(), sk_assassin_vantage_min_dist_from_squad.GetFloat() );
		}
	}
	
	hint.SetFlag( bits_HINT_NODE_NEAREST );
	hint.SetFlag( bits_HINT_NODE_CLEAR );
	hint.SetFlag( bits_HINT_NODE_USE_GROUP );

	CAI_Hint *pHint = CAI_HintManager::FindHint( this, vecSearchPos, hint );

	return pHint;
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CNPC_Assassin::FValidateHintType ( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
		case HINT_TACTICAL_ENEMY_DISADVANTAGED:
			if (npc_assassin_use_new_vantage_type.GetBool())
				return BaseClass::FValidateHintType( pHint );
		case HINT_TACTICAL_VANTAGE_POINT:
			{
				if (GetEnemy())
				{
					CBaseEntity *pEnemy = GetEnemy();

					Vector	hintPos;
					pHint->GetPosition( this, &hintPos );

					// Verify that we can see the target from that position
					hintPos += GetViewOffset();

					Vector enemyPos = GetEnemies()->LastKnownPosition( pEnemy );
					enemyPos += pEnemy->GetViewOffset();

					trace_t	tr;
					UTIL_TraceLine( hintPos, enemyPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

					// Check for seeing our target at the new location
					// New vantage points require the target to be seen, old ones require the opposite
					if ( ( tr.fraction == 1.0f ) || ( tr.m_pEnt == pEnemy) )
						return npc_assassin_use_new_vantage_type.GetBool() ? true : false;
				}

				return true;
				break;
			}

		default:
			return BaseClass::FValidateHintType( pHint );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Assassin::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	return BaseClass::Weapon_TranslateActivity( baseAct, pRequired );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Assassin::NPC_TranslateActivity( Activity eNewActivity )
{
	if (eNewActivity == ACT_RUN || eNewActivity == ACT_RUN_AIM)
	{
		if (ShouldFlip())
		{
			if (HasDualWeapons())
			{
				eNewActivity = ACT_ASSASSIN_FLIP_LOOP_DUAL_PISTOLS;
			}
			else if (GetActiveWeapon())
			{
				WeaponClass_t iWepClass = GetActiveWeapon()->WeaponClassify();
				switch (iWepClass)
				{
					case WEPCLASS_HANDGUN:		eNewActivity = ACT_ASSASSIN_FLIP_LOOP_PISTOL; break;

					case WEPCLASS_HEAVY:
					case WEPCLASS_RIFLE:		eNewActivity = ACT_ASSASSIN_FLIP_LOOP_RIFLE; break;
				}
			}
			else
			{
				eNewActivity = ACT_ASSASSIN_FLIP_LOOP;
			}
		}
	}
	else if (eNewActivity == ACT_ASSASSIN_FLIP)
	{
		if (HasDualWeapons())
		{
			eNewActivity = ACT_ASSASSIN_FLIP_DUAL_PISTOLS;
		}
		else if (GetActiveWeapon())
		{
			WeaponClass_t iWepClass = GetActiveWeapon()->WeaponClassify();
			switch (iWepClass)
			{
				case WEPCLASS_HANDGUN:		eNewActivity = ACT_ASSASSIN_FLIP_PISTOL; break;

				case WEPCLASS_HEAVY:
				case WEPCLASS_RIFLE:		eNewActivity = ACT_ASSASSIN_FLIP_RIFLE; break;
			}
		}
	}
	else if (!GetActiveWeapon() || GetActiveWeapon()->WeaponClassify() == WEPCLASS_HANDGUN || HasDualWeapons())
	{
		switch (eNewActivity)
		{
			case ACT_JUMP:
				eNewActivity = ACT_ASSASSIN_ROLL_JUMP;
				break;
			case ACT_GLIDE:
				eNewActivity = ACT_ASSASSIN_ROLL_GLIDE;
				break;
			case ACT_LAND:
				eNewActivity = ACT_ASSASSIN_ROLL_LAND;
				break;
			case ACT_MELEE_ATTACK1:
				eNewActivity = ACT_ASSASSIN_KICK;
				break;
		}

		if (HasDualWeapons())
		{
			switch (eNewActivity)
			{
				case ACT_ARM:
					eNewActivity = ACT_ARM_DUAL_PISTOLS;
					break;
				case ACT_DISARM:
					eNewActivity = ACT_DISARM_DUAL_PISTOLS;
					break;
			}
		}
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::OnChangeActivity( Activity eNewActivity )
{
	BaseClass::OnChangeActivity( eNewActivity );

	switch (eNewActivity)
	{
		case ACT_JUMP:
			EmitSound( "NPC_Assassin.Jump" );
			break;

		case ACT_LAND:
			EmitSound( "NPC_Assassin.Land" );
			break;
	}
}

#ifdef EZ2
// From npc_hunter.cpp
extern CUtlVector<CBaseEntity *> g_pDodgeableProjectiles;

extern void HunterTraceHull_SkipPhysics_Extern( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin,
	const Vector &hullMax, unsigned int mask, const CBaseEntity *ignore,
	int collisionGroup, trace_t *ptr, float minMass );

//-----------------------------------------------------------------------------
// Purpose: Extremely experimental code for dodging numerous types of entities
//-----------------------------------------------------------------------------
bool CNPC_Assassin::CheckDodgeConditions( bool bSeeEnemy )
{
	// Attempt to dodge relevant projectiles, e.g. Combine balls
	for (int i = 0; i < g_pDodgeableProjectiles.Count(); i++)
	{
		CBaseEntity *pProjectile = g_pDodgeableProjectiles[i];
		if (pProjectile /*&& pProjectile->GetOwnerEntity() && IRelationType(pProjectile->GetOwnerEntity()) <= D_FR*/)
		{
			if (pProjectile->GetAbsOrigin().AsVector2D().DistToSqr(GetAbsOrigin().AsVector2D()) <= Square(1024))
			{
				if (CheckDodge(pProjectile))
					return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::CheckDodge( CBaseEntity *pVehicle )
{
	static float timeDrawnArrow;

	// Extrapolate the position of the vehicle and see if it's heading toward us.
	float predictTime = sk_assassin_dodge_warning.GetFloat();
	Vector2D vecFuturePos = pVehicle->GetAbsOrigin().AsVector2D() + pVehicle->GetSmoothedVelocity().AsVector2D() * predictTime;
	if ( pVehicle->GetSmoothedVelocity().LengthSqr() > Square( 200 ) )
	{
		float t = 0;
		Vector2D vDirMovement = pVehicle->GetSmoothedVelocity().AsVector2D();
		if ( g_debug_assassin_dodge.GetBool() )
		{
			NDebugOverlay::Line( pVehicle->GetAbsOrigin(), pVehicle->GetAbsOrigin() + pVehicle->GetSmoothedVelocity(), 255, 255, 255, true, .1 );
		}
		vDirMovement.NormalizeInPlace();
		Vector2D vDirToHunter = GetAbsOrigin().AsVector2D() - pVehicle->GetAbsOrigin().AsVector2D();
		vDirToHunter.NormalizeInPlace();
		if ( DotProduct2D( vDirMovement, vDirToHunter ) > sk_assassin_dodge_warning_cone.GetFloat() &&
			 CalcDistanceSqrToLine2D( GetAbsOrigin().AsVector2D(), pVehicle->GetAbsOrigin().AsVector2D(), vecFuturePos, &t ) < Square( sk_assassin_dodge_warning_width.GetFloat() * .5 ) &&
			 t > 0.0 && t < 1.0 )
		{
			if ( fabs( predictTime - sk_assassin_dodge_warning.GetFloat() ) < .05 || random->RandomInt( 0, 3 ) )
			{
				SetCondition( COND_ASSASSIN_INCOMING_PROJECTILE );
				m_hDodgeTarget = pVehicle;
			}
			else
			{
				if ( g_debug_assassin_dodge. GetBool() )
				{
					Msg( "Assassin %d failing dodge (ignore)\n", entindex() );
				}
			}

			if ( g_debug_assassin_dodge. GetBool() )
			{
				NDebugOverlay::Cross3D( EyePosition(), 100, 255, 255, 255, true, .1 );
				if ( timeDrawnArrow != gpGlobals->curtime )
				{
					timeDrawnArrow = gpGlobals->curtime;
					Vector vEndpoint( vecFuturePos.x, vecFuturePos.y, UTIL_GetLocalPlayer()->WorldSpaceCenter().z - 24 );
					NDebugOverlay::HorzArrow( UTIL_GetLocalPlayer()->WorldSpaceCenter() - Vector(0, 0, 24), vEndpoint, sk_assassin_dodge_warning_width.GetFloat(), 255, 0, 0, 64, true, .1 );
				}
			}
		}
		else if ( g_debug_assassin_dodge.GetBool() )
		{
			if ( t <= 0 )
			{
				NDebugOverlay::Cross3D( EyePosition(), 100, 0, 0, 255, true, .1 );
			}
			else
			{
				NDebugOverlay::Cross3D( EyePosition(), 100, 0, 255, 255, true, .1 );
			}
		}
	}
	else if ( g_debug_assassin_dodge.GetBool() )
	{
		NDebugOverlay::Cross3D( EyePosition(), 100, 0, 255, 0, true, .1 );
	}
	if ( g_debug_assassin_dodge.GetBool() )
	{
		if ( timeDrawnArrow != gpGlobals->curtime )
		{
			timeDrawnArrow = gpGlobals->curtime;
			Vector vEndpoint( vecFuturePos.x, vecFuturePos.y, UTIL_GetLocalPlayer()->WorldSpaceCenter().z - 24 );
			NDebugOverlay::HorzArrow( UTIL_GetLocalPlayer()->WorldSpaceCenter() - Vector(0, 0, 24), vEndpoint, sk_assassin_dodge_warning_width.GetFloat(), 127, 127, 127, 64, true, .1 );
		}
	}

	return m_hDodgeTarget.Get() != NULL;
}

//-----------------------------------------------------------------------------
// The player is speeding toward us in a vehicle! Find a good activity for dodging.
//-----------------------------------------------------------------------------
void CNPC_Assassin::TaskFindDodgeActivity()
{
	CBaseEntity *pDodgeTarget = m_hDodgeTarget;
	Vector vecDodgeTargetVel;
	if (pDodgeTarget == NULL)
	{
		// See if there's an AI target to jump away from instead
		// In this situation, direction is used and velocity is ignored
		pDodgeTarget = GetTarget();
		if (pDodgeTarget)
		{
			pDodgeTarget->GetVectors( &vecDodgeTargetVel, NULL, NULL );
			vecDodgeTargetVel *= 100.0f;
		}
	}
	else
	{
		vecDodgeTargetVel = pDodgeTarget->GetSmoothedVelocity();
	}
	
	if (pDodgeTarget == NULL )
	{
		TaskFail( "No target to dodge" );
		return;
	}

	Vector vecUp;
	Vector vecRight;
	GetVectors( NULL, &vecRight, &vecUp );

	// TODO: find most perpendicular 8-way dodge when we get the anims
	Vector vecEnemyDir = pDodgeTarget->GetAbsOrigin() - GetAbsOrigin();
	//Vector vecDir = CrossProduct( vecEnemyDir, vecUp );
	VectorNormalize( vecEnemyDir );
	/*
	if ( fabs( DotProduct( vecEnemyDir, vecRight ) ) > 0.7 )
	{
		TaskFail( "Can't dodge, enemy approaching perpendicularly" );
		return;
	}
	*/

	float flDodgeDirection;
	bool bStartLeft = false;
	{
		Ray_t enemyRay;
		Ray_t perpendicularRay;
		enemyRay.Init( pDodgeTarget->GetAbsOrigin(), pDodgeTarget->GetAbsOrigin() + vecDodgeTargetVel );
		Vector vPerpendicularPt = vecEnemyDir;
		vPerpendicularPt.y = -vPerpendicularPt.y;
		perpendicularRay.Init( GetAbsOrigin(), GetAbsOrigin() + vPerpendicularPt );

		enemyRay.m_Start.z = enemyRay.m_Delta.z = enemyRay.m_StartOffset.z;
		perpendicularRay.m_Start.z = perpendicularRay.m_Delta.z = perpendicularRay.m_StartOffset.z;

		float t, s;

		IntersectRayWithRay( perpendicularRay, enemyRay, t, s );

		QAngle angDodgeAngle;
		VectorAngles( vecEnemyDir, angDodgeAngle );

		flDodgeDirection = GetAbsAngles().y - angDodgeAngle.y;
		if (t > 0)
		{
			flDodgeDirection += 90.0f;
			bStartLeft = true;
		}
		else
			flDodgeDirection -= 90.0f;

		if (g_debug_assassin_dodge.GetBool())
			Msg( "Dodge direction is %f\n", flDodgeDirection );
	}

	bool bFoundDir = false;
	int nTries = 0;

	while ( !bFoundDir && ( nTries < 3 ) )
	{
		SetPoseParameter( LookupPoseMoveYaw(), flDodgeDirection );

		// See where the dodge will put us.
		Vector vecLocalDelta;
		int nSeq = SelectWeightedSequence( ACT_ASSASSIN_FLIP );
		GetSequenceLinearMotion( nSeq, &vecLocalDelta );

		// Transform the sequence delta into local space.
		matrix3x4_t fRotateMatrix;
		AngleMatrix( GetLocalAngles(), fRotateMatrix );
		Vector vecDelta;
		VectorRotate( vecLocalDelta, fRotateMatrix, vecDelta );

		// Trace a bit high so this works better on uneven terrain.
		Vector testHullMins = GetHullMins();
		testHullMins.z += ( StepHeight() * 2 );

		// See if all is clear in that direction.
		trace_t tr;
		HunterTraceHull_SkipPhysics_Extern( GetAbsOrigin(), GetAbsOrigin() + vecDelta, testHullMins, GetHullMaxs(), MASK_NPCSOLID, this, GetCollisionGroup(), &tr, VPhysicsGetObject()->GetMass() * 0.5f );

		// TODO: dodge anyway if we'll make it a certain percentage of the way through the dodge?
		if ( tr.fraction == 1.0f )
		{
			//NDebugOverlay::SweptBox( GetAbsOrigin(), GetAbsOrigin() + vecDelta, testHullMins, GetHullMaxs(), QAngle( 0, 0, 0 ), 0, 255, 0, 128, 5 );
			bFoundDir = true;
			TaskComplete();
		}
		else
		{
			//NDebugOverlay::SweptBox( GetAbsOrigin(), GetAbsOrigin() + vecDelta, testHullMins, GetHullMaxs(), QAngle( 0, 0, 0 ), 255, 0, 0, 128, 5 );
			nTries++;
			flDodgeDirection += (bStartLeft ? 90.0f : -90.0f);
		}
	}

	if ( nTries < 3 )
	{
		TaskComplete();
	}
	else
	{
		TaskFail( "Couldn't find dodge position\n" );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::GatherConditions()
{
	BaseClass::GatherConditions();

#ifdef EZ2
	ClearCondition( COND_ASSASSIN_INCOMING_PROJECTILE );

	m_hDodgeTarget = NULL;
	if ( m_IgnoreProjectileTimer.Expired() )
	{
		bool bEnemyCondition = (GetEnemy() && HasCondition( COND_SEE_ENEMY ));
		CheckDodgeConditions( bEnemyCondition );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	ClearCondition( COND_ASSASSIN_ENEMY_TARGETING_ME );
	
	if ( m_bCloaking && gpGlobals->curtime - GetLastDamageTime() < 5.0f && (HasCondition(COND_HEAVY_DAMAGE) || ((GetHealth() / GetMaxHealth()) < 0.35f)))
	{
		// If cloaked, stop attacking when under heavy damage
		SetCondition( COND_ASSASSIN_CLOAK_RETREAT );
		VacateStrategySlot();
	}
	else
	{
		ClearCondition( COND_ASSASSIN_CLOAK_RETREAT );
	}

	BaseClass::GatherEnemyConditions( pEnemy );

	// See if we're being targeted specifically
	if ( HasCondition( COND_ENEMY_FACING_ME ) && m_flCloakFactor < 1.0f )
	{
		Vector	enemyDir = GetAbsOrigin() - pEnemy->GetAbsOrigin();
		VectorNormalize( enemyDir );

		Vector	enemyBodyDir;
		CBaseCombatCharacter *pBCC = ToBaseCombatCharacter( pEnemy );

		if ( pBCC != NULL )
		{
			enemyBodyDir = pBCC->BodyDirection3D();

			float	enemyDot = DotProduct( enemyBodyDir, enemyDir );

			if (enemyDot > 0.994f)
			{
				SetCondition( COND_ASSASSIN_ENEMY_TARGETING_ME );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	// Become interrupted if we're targeted when shooting an enemy
	if ( IsCurSchedule( SCHED_ASSASSIN_RANGE_ATTACK1, false ) && GetSquad() && GetSquad()->NumMembers() > 3 )
	{
		SetCustomInterruptCondition( COND_ASSASSIN_ENEMY_TARGETING_ME );
	}

	// Assassins are more averse to damage than regular Combine soldiers
	if ( IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK, false ) || IsCurSchedule( SCHED_COMBINE_ASSAULT, false ) )
	{
		SetCustomInterruptCondition( COND_ASSASSIN_CLOAK_RETREAT );
	}

	// Interrupt everything if we need to dodge.
	if ( !IsCurSchedule( SCHED_ASSASSIN_DODGE_FLIP, false ) )
	{
		SetCustomInterruptCondition( COND_ASSASSIN_INCOMING_PROJECTILE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Assassin::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();

	if (IsCurSchedule( SCHED_ASSASSIN_PERCH, false ))
	{
		m_flNextPerchTime = gpGlobals->curtime + sk_assassin_vantage_cooldown.GetFloat();
	}
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CNPC_Assassin::DecalTrace( trace_t *pTrace, char const *decalName )
{
	// Do not decal while cloaked
	if ( m_bCloaking )
		return;

	BaseClass::DecalTrace( pTrace, decalName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::FCanCheckAttacks( void )
{
	if ( GetEnemy() )
	{
		if ( HasCondition( COND_ASSASSIN_CLOAK_RETREAT ) && EnemyDistance(GetEnemy()) > 64.0f )
		{
			// Don't try to attack while retreating
			return false;
		}

		if ( IsCurSchedule( SCHED_ASSASSIN_EVADE, false ) && m_flCloakFactor == 1.0f )
		{
			// Don't draw attention to yourself if you don't have to
			return false;
		}
	}

	return BaseClass::FCanCheckAttacks();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::CanBeHitByMeleeAttack( CBaseEntity *pAttacker )
{
	if( IsCurSchedule(SCHED_DUCK_DODGE) || IsCurSchedule(SCHED_ASSASSIN_DODGE_FLIP) )
	{
		return false;
	}

	return BaseClass::CanBeHitByMeleeAttack( pAttacker );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Assassin::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if( (interactionType == g_interactionZombieMeleeWarning || interactionType == g_interactionGenericMeleeWarning) && IsAllowedToDodge() )
	{
		// If a zombie is attacking, ditch my current schedule and duck if I'm running a schedule that will
		// be interrupted if I'm hit.
		if( !IsInAScript() && IsInterruptable() && (ConditionInterruptsCurSchedule(COND_LIGHT_DAMAGE) || ConditionInterruptsCurSchedule( COND_HEAVY_DAMAGE) || IsCurSchedule( SCHED_ASSASSIN_MELEE_ATTACK1, false )) )
		{
			if( sourceEnt /*&& FInViewCone(sourceEnt)*/ )
			{
				UpdateEnemyMemory( sourceEnt, sourceEnt->GetAbsOrigin(), this );
				SetSchedule( SCHED_DUCK_DODGE );
			}
		}

		return true;
	}
#ifdef EZ2
	else if (interactionType == g_interactionBadCopKickWarn)
	{
		// Flip away if I'm running a schedule that will be interrupted if I'm hit.
		if (!IsInAScript() && IsInterruptable() && (ConditionInterruptsCurSchedule(COND_LIGHT_DAMAGE) || ConditionInterruptsCurSchedule(COND_HEAVY_DAMAGE) || IsCurSchedule(SCHED_ASSASSIN_MELEE_ATTACK1, false)))
		{
			if (sourceEnt /*&& FInViewCone(sourceEnt)*/)
			{
				UpdateEnemyMemory( sourceEnt, sourceEnt->GetAbsOrigin(), this );
				SetTarget( sourceEnt );
				SetSchedule( SCHED_ASSASSIN_DODGE_FLIP );
			}
		}

		return true;
	}
#endif

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Assassin::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "dual_wield", (m_hLeftHandGun && !(m_hLeftHandGun->GetEffects() & EF_NODRAW)) ? "1" : "0" );

	if (m_bCloaking)
	{
		set.AppendCriteria( "cloak_factor", UTIL_VarArgs( "%f", m_flCloakFactor.Get() ) );
	}
	else
	{
		set.AppendCriteria( "cloak_factor", "0.0" );
	}

	set.AppendCriteria( "cloak_time_left", UTIL_VarArgs( "%f", m_flMaxCloakTime - gpGlobals->curtime ) );

	set.AppendCriteria( "perching", IsCurSchedule( SCHED_ASSASSIN_PERCH, false ) ? "1" : "0" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CNPC_Assassin::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	// At this point, m_hLeftHandGun refers to the dropped weapon. We do this so that we can dissolve it
	if (m_hLeftHandGun)
	{
		if ( info.GetDamageType() & DMG_DISSOLVE )
		{
			m_hLeftHandGun->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}
	}

	// If we were sufficiently cloaked when killed, make the suit malfunction
	if (m_flCloakFactor >= 0.8f && m_hDeathRagdoll)
	{
		Vector vecSparkDir = info.GetDamageForce();
		g_pEffects->Sparks( info.GetDamagePosition(), 2, RandomInt(2, 3), &vecSparkDir );
		//EmitSound( "DoSpark" );

		// Now handled by proxy on client
		//m_hDeathRagdoll->m_nRenderMode = kRenderTransColor;

		m_hDeathRagdoll->SetRenderColorA( (255.0f * (1.0f - (m_flCloakFactor*0.5f))) );
		m_hDeathRagdoll->m_nRenderFX = kRenderFxDistort;

		CRagdollBoogie::Create( m_hDeathRagdoll, 50, gpGlobals->curtime, RandomFloat(3.0f, 4.0f), SF_RAGDOLL_BOOGIE_ELECTRICAL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Assassin::IsLightDamage( const CTakeDamageInfo &info )
{
	return BaseClass::IsLightDamage( info );
}

extern ConVar sk_plr_dmg_buckshot;
extern ConVar sk_plr_num_shotgun_pellets;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Assassin::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;

	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) && hl2_episodic.GetBool() )
	{
		return true;
	}

	// Heavy damage when hit directly by shotgun flechettes
	if ( info.GetDamageType() & (DMG_DISSOLVE | DMG_NEVERGIB) && info.GetInflictor() && FClassnameIs( info.GetInflictor(), "shotgun_flechette" ) )
		return true;

	return BaseClass::IsHeavyDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Assassin::AddLeftHandGun( CBaseCombatWeapon *pWeapon )
{
	// Cancel out and show warning if we already have a dual weapon
	if (m_hLeftHandGun)
	{
		Warning("%s cannot dual wield %s because it already has a left-handed weapon (%s)\n",
			GetDebugName(), pWeapon->GetClassname(), m_hLeftHandGun->GetOwnerEntity() ? m_hLeftHandGun->GetOwnerEntity()->GetClassname() : "<null>");
		return;
	}

	// Create a fake second pistol
	CBaseEntity *pEnt = CBaseEntity::CreateNoSpawn( "prop_dynamic_override", this->GetLocalOrigin(), this->GetLocalAngles(), this );
	if (pEnt)
	{
		// HACKHACK: Just add "_left" to the end of the model name
		char szLeftModel[MAX_PATH];
		V_StripExtension( pWeapon->GetWorldModel(), szLeftModel, sizeof( szLeftModel ) );
		V_strncat( szLeftModel, "_left.mdl", sizeof( szLeftModel ) );

		pEnt->SetModelName( MAKE_STRING( szLeftModel ) );
		pEnt->SetRenderMode( kRenderTransColor );
		DispatchSpawn( pEnt );
		pEnt->FollowEntity( this, true );
		pEnt->SetOwnerEntity( pWeapon );

		m_hLeftHandGun = static_cast<CBaseAnimating *>(pEnt);

		// Make it dual-wielded
		assert_cast<CBaseHLCombatWeapon*>(pWeapon)->SetLeftHandGun( m_hLeftHandGun );
	}
}

//-----------------------------------------------------------------------------
// Update weapon ranges
//-----------------------------------------------------------------------------
void CNPC_Assassin::Weapon_HandleEquip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_HandleEquip( pWeapon );

	if (pWeapon)
	{
		pWeapon->SetRenderMode( kRenderTransColor );

		// Expand range
		pWeapon->m_fMaxRange1 = ASSASSIN_MAX_RANGE;
		pWeapon->m_fMaxRange2 = ASSASSIN_MAX_RANGE;

		if (m_bDualWeapons)
		{
			// Check if it can be dual-wielded
			CBaseHLCombatWeapon *pHLWeapon = dynamic_cast<CBaseHLCombatWeapon*>(pWeapon);
			if (pHLWeapon && pHLWeapon->CanDualWield())
			{
				AddLeftHandGun( pWeapon );
			}
			else
			{
				Warning( "%s: %s cannot be dual-wielded\n", GetDebugName(), pWeapon->GetClassname() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Puts a new weapon in the inventory
// Input  : New weapon
//-----------------------------------------------------------------------------
void CNPC_Assassin::Weapon_EquipHolstered( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_EquipHolstered( pWeapon );

	if (m_hLeftHandGun && m_hLeftHandGun->GetOwnerEntity() == pWeapon)
	{
		m_hLeftHandGun->AddEffects( EF_NODRAW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Drop the active weapon, optionally throwing it at the given target position.
// Input  : pWeapon - Weapon to drop/throw.
//			pvecTarget - Position to throw it at, NULL for none.
//-----------------------------------------------------------------------------
void CNPC_Assassin::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	if ( !pWeapon )
		return;

	if ( GetTask() && GetTask()->iTask == TASK_WEAPON_PICKUP && GetTarget() && pWeapon->GetClassname() == GetTarget()->GetClassname() && IsAlive() )
	{
		// HACKHACK: If this is the same weapon we have, and it can be dual-wielded, don't drop our current one
		CBaseHLCombatWeapon *pHLWeapon = dynamic_cast<CBaseHLCombatWeapon*>(pWeapon);
		if ( pHLWeapon && pHLWeapon->CanDualWield() && !m_bDualWeapons )
			return;
	}

	pWeapon->SetRenderColorA( 255 );
	pWeapon->RemoveEffects( EF_NOSHADOW );

	// Spawn left hand gun when regular one is dropped
	if (m_hLeftHandGun && !(m_hLeftHandGun->GetEffects() & EF_NODRAW))
	{
		CBaseHLCombatWeapon *pHLWeapon = assert_cast<CBaseHLCombatWeapon *>(pWeapon);
		if (pHLWeapon)
		{
			pHLWeapon->SetLeftHandGun( NULL );
		}

		if (!HasSpawnFlags(SF_NPC_NO_WEAPON_DROP))
		{
			// Drop the fake gun
			CBaseEntity *pRealGun = CreateNoSpawn( pWeapon->GetClassname(), m_hLeftHandGun->GetAbsOrigin(), m_hLeftHandGun->GetAbsAngles(), this );
			DispatchSpawn( pRealGun );

			m_OnDualWeaponDrop.FireOutput( pRealGun, this );

			UTIL_Remove( m_hLeftHandGun );

			// If we're dying, store this real gun and dissolve it later
			if (m_lifeState == LIFE_DYING)
			{
				m_hLeftHandGun = static_cast<CBaseAnimating *>(pRealGun);
			}
			else
			{
				m_hLeftHandGun = NULL;
			}
		}
		else
			UTIL_Remove( m_hLeftHandGun );

		// For now, don't dual wield again
		// (prevents assassin from making infinite pistols if she picks one up again)
		m_bDualWeapons = false;
	}

	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );

	if (m_hLeftHandGun && m_hLeftHandGun->VPhysicsGetObject() && pWeapon->VPhysicsGetObject())
	{
		// Copy velocity
		Vector vecVelocity;
		AngularImpulse vecAngVelocity;
		pWeapon->VPhysicsGetObject()->GetVelocity( &vecVelocity, &vecAngVelocity );
		m_hLeftHandGun->VPhysicsGetObject()->SetVelocity( &vecVelocity, &vecAngVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CNPC_Assassin::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	if ( GetActiveWeapon() && GetActiveWeapon()->GetClassname() == pWeapon->GetClassname() )
	{
		CBaseHLCombatWeapon *pHLWeapon = dynamic_cast<CBaseHLCombatWeapon*>(pWeapon);
		if ( pHLWeapon && pHLWeapon->CanDualWield() && !m_bDualWeapons )
		{
			// Add left hand gun for weapon that already exists
			AddLeftHandGun( GetActiveWeapon() );
			m_bDualWeapons = true;
			UTIL_Remove( pWeapon );
			return;
		}
	}

	BaseClass::Weapon_Equip( pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
Vector CNPC_Assassin::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	Vector baseResult = BaseClass::GetAttackSpread( pWeapon, pTarget );

	if (m_flCloakFactor >= sk_assassin_cloak_cone_nerf_base.GetFloat())
	{
		// Assassins are less accurate when invisible
		float multiplier = RemapVal( m_flCloakFactor, sk_assassin_cloak_cone_nerf_base.GetFloat(), 1.0f, 1.0f, sk_assassin_cloak_cone_nerf_multiplier.GetFloat());
		baseResult *= multiplier;
	}

	return baseResult;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
float CNPC_Assassin::GetSpreadBias( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	float bias = BaseClass::GetSpreadBias( pWeapon, pTarget );

	if (m_flCloakFactor >= sk_assassin_cloak_cone_nerf_base.GetFloat())
	{
		// Assassins are less accurate when invisible
		float multiplier = 1.0f - RemapVal( m_flCloakFactor, sk_assassin_cloak_cone_nerf_base.GetFloat(), 1.0f, 0.0f, sk_assassin_cloak_cone_nerf_bias.GetFloat());
		bias *= multiplier;
	}

	return bias;
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CNPC_Assassin::DrawDebugTextOverlays(void)
{
	int text_offset = 0;

	// ---------------------
	// Print Baseclass text
	// ---------------------
	text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];

		// --------------
		// Print Speed
		// --------------
		Q_snprintf( tempstr, sizeof( tempstr ), "Speed: %f", GetInstantaneousVelocity() );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;

		// --------------
		// Print Cloak Factor
		// --------------
		Q_snprintf( tempstr,sizeof(tempstr),"Cloak Factor: %f", m_flCloakFactor.Get() );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug geometry overlays
//-----------------------------------------------------------------------------
void CNPC_Assassin::DrawDebugGeometryOverlays(void)
{
	BaseClass::DrawDebugGeometryOverlays();

	if (g_debug_assassin.GetInt() && m_debugOverlays & OVERLAY_NPC_SELECTED_BIT)
	{
		if (m_flCloakFactor > 0.0f)
		{
			if (GetActiveWeapon() && GetEnemy())
			{
				float flSpread = GetAttackSpread( GetActiveWeapon(), GetEnemy() ).x * 96.0f;
				float flSpreadBias = GetSpreadBias( GetActiveWeapon(), GetEnemy() );

				Vector vecShootPos = Weapon_ShootPosition();
				Vector vecToEnemy = GetShootEnemyDir( vecShootPos, false );

				QAngle angToEnemy;
				VectorAngles( vecToEnemy, angToEnemy );

				if (g_debug_assassin.GetInt() >= 2)
				{
					// Shoot Direction
					NDebugOverlay::Line( vecShootPos, vecShootPos + (vecToEnemy * EnemyDistance( GetEnemy() )), 255, 0, 0, true, 1.0f );
				}
				else if (g_debug_assassin.GetInt() != 2)
				{
					// Spread + Bias
					NDebugOverlay::Circle( vecShootPos, angToEnemy, flSpread, 0, 255, 255, 255, true, 1.0f );
					NDebugOverlay::Circle( vecShootPos, angToEnemy, flSpread * flSpreadBias, 255, 0, 0, 255, true, 1.0f );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_assassin, CNPC_Assassin )

	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_DUAL_PISTOLS )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_PISTOL )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_RIFLE )

	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_LOOP )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_LOOP_DUAL_PISTOLS )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_LOOP_PISTOL )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLIP_LOOP_RIFLE )

	DECLARE_ACTIVITY( ACT_ASSASSIN_ROLL_JUMP )
	DECLARE_ACTIVITY( ACT_ASSASSIN_ROLL_GLIDE )
	DECLARE_ACTIVITY( ACT_ASSASSIN_ROLL_LAND )

	DECLARE_ACTIVITY( ACT_ASSASSIN_KICK )
	DECLARE_ACTIVITY( ACT_ASSASSIN_DODGE_KICK )

	DECLARE_ACTIVITY( ACT_ARM_DUAL_PISTOLS )
	DECLARE_ACTIVITY( ACT_DISARM_DUAL_PISTOLS )

	DECLARE_ANIMEVENT( AE_PISTOL_FIRE_LEFT )
	DECLARE_ANIMEVENT( AE_PISTOL_FIRE_RIGHT )
	DECLARE_ANIMEVENT( AE_ASSASSIN_KICK_HIT )

	DECLARE_CONDITION( COND_ASSASSIN_ENEMY_TARGETING_ME )
	DECLARE_CONDITION( COND_ASSASSIN_CLOAK_RETREAT )
	DECLARE_CONDITION( COND_ASSASSIN_INCOMING_PROJECTILE )

	DECLARE_TASK( TASK_ASSASSIN_WAIT_FOR_CLOAK )
	DECLARE_TASK( TASK_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE )
	DECLARE_TASK( TASK_ASSASSIN_CHECK_CLOAK )
	DECLARE_TASK( TASK_ASSASSIN_START_PERCHING )
	DECLARE_TASK( TASK_ASSASSIN_PERCH )
	DECLARE_TASK( TASK_ASSASSIN_FIND_DODGE_POSITION )
	DECLARE_TASK( TASK_ASSASSIN_DODGE_FLIP )

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_ANNOUNCE_ATTACK			1"	// 1 = primary attack
		"		TASK_WAIT_RANDOM				0.2"
		"		TASK_RANGE_ATTACK1				0"
		"		TASK_COMBINE_IGNORE_ATTACKS		0.5"
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
		"		COND_COMBINE_NO_FIRE"
		""
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_MELEE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
		//"		COND_NEW_ENEMY"
		//"		COND_ENEMY_DEAD"
		//"		COND_LIGHT_DAMAGE"
		//"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		//"		COND_ASSASSIN_CLOAK_RETREAT"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_WAIT_FOR_CLOAK,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_WAIT_RANDOM				0.3"
		"		TASK_ASSASSIN_WAIT_FOR_CLOAK	0"
		""
		"	Interrupts"
		//"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_PROVOKED"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_WAIT_RANDOM				0.3"
		"		TASK_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE	0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_GO_TO_VANTAGE_POINT,

		"	Tasks"
		"		TASK_GET_PATH_TO_HINTNODE				0"
		"		TASK_LOCK_HINTNODE						0"
		"		TASK_REMEMBER							MEMORY:LOCKED_HINT"
		"		TASK_RUN_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_COMBINE_SET_STANDING				0"
		"		TASK_SET_SCHEDULE						SCHEDULE:SCHED_ASSASSIN_PERCH"
		""
		"	Interrupts"
		//"		COND_ENEMY_DEAD"
		"		COND_HEAR_DANGER"
		"		COND_CAN_MELEE_ATTACK1"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_PERCH,

		"	Tasks"
		"		TASK_STOP_MOVING						0"
		"		TASK_ASSASSIN_PERCH						0"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_EVADE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASSASSIN_FLANK_FALLBACK"
		//"		TASK_STOP_MOVING				0"
		//"		TASK_WAIT						0.2"
	//	"		TASK_SET_TOLERANCE_DISTANCE		24"
		"		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY		250"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_REMEMBER					MEMORY:INCOVER"
		//"		TASK_FACE_ENEMY					0"
		//"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"	// Translated to cover
		//"		TASK_WAIT						1"
		""
		"	Interrupts"
		//"		COND_NEW_ENEMY"
		"		COND_HEAR_DANGER"
		"		COND_CAN_MELEE_ATTACK1"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_FLANK_RANDOM,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_ASSASSIN_RANGE_ATTACK1"
		"		TASK_SET_TOLERANCE_DISTANCE				256"
		"		TASK_SET_ROUTE_SEARCH_TIME				1"	// Spend 1 second trying to build a path if stuck
		"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	80"
		"		TASK_RUN_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
		//"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_ASSASSIN_CLOAK_RETREAT"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_FLANK_FALLBACK,

		"	Tasks"
		"		TASK_SET_TOLERANCE_DISTANCE				256"
		"		TASK_SET_ROUTE_SEARCH_TIME				1"	// Spend 1 second trying to build a path if stuck
		"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	80"
		"		TASK_RUN_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
		//"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_DODGE_KICK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_ASSASSIN_DODGE_KICK"
		"		TASK_DEFER_DODGE	30"
		""
		"	Interrupts"
		//"		COND_NEW_ENEMY"
		//"		COND_ENEMY_DEAD"
		//"		COND_LIGHT_DAMAGE"
		//"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		//"		COND_ASSASSIN_CLOAK_RETREAT"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_DODGE_FLIP,

		"	Tasks"
		"		TASK_ASSASSIN_FIND_DODGE_POSITION		0"
		"		TASK_ASSASSIN_DODGE_FLIP				0"
		""
		"	Interrupts"
	)

AI_END_CUSTOM_NPC()
