//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_behavior.h"
#include "ai_baseactor.h" 
#include "ai_behavior_lead.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_assault.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "activitylist.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "sceneentity.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_weapon.h"
#include "asw_marine_speech.h"
//#include "asw_drone.h"
#include "asw_pickup.h"
#include "asw_pickup_weapon.h"
#include "asw_gamerules.h"
#include "asw_gamestats.h"
#include "asw_mission_manager.h"
#include "asw_fail_advice.h"
#include "ammodef.h"
#include "asw_shareddefs.h"
#include "asw_sentry_base.h"
#include "asw_button_area.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "asw_fx_shared.h"
#include "asw_parasite.h"
#include "shareddefs.h"
#include "iasw_vehicle.h"
#include "obstacle_pushaway.h"
#include "asw_computer_area.h"
#include "asw_remote_turret_shared.h"
#include "asw_util_shared.h"
#include "EntityFlame.h"
#include "physics_prop_ragdoll.h"
#include "asw_weapon_flashlight_shared.h"
#include "beam_shared.h"
#include "iasw_server_usable_entity.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "datacache/imdlcache.h"
#include "asw_weapon_autogun_shared.h"
#include "asw_burning.h"
#include "asw_door.h"
#include "asw_hack.h"
#include "te_effect_dispatch.h"
#include "asw_trace_filter_melee.h"
#include "ai_moveprobe.h"
#include "asw_ai_senses.h"
#include "particle_parse.h"
#include "asw_director.h"
#include "asw_melee_system.h"
#include "ai_network.h"
#include "asw_weapon_normal_armor.h"
#include "asw_fire.h"
#include "asw_achievements.h"
#include "asw_objective_escape.h"
#include "sendprop_priorities.h"
#include "asw_marine_gamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_DEFAULT_MARINE_MODEL "models/swarm/marine/marine.mdl"

//#define ASW_MARINE_ALWAYS_VPHYSICS

//=========================================================
// Marine activities
//=========================================================

LINK_ENTITY_TO_CLASS( asw_marine, CASW_Marine );

//---------------------------------------------------------
// 
//---------------------------------------------------------

void SendProxy_CropMarineFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID)
{
	int mask = (1<<PLAYER_FLAG_BITS) - 1;
	int data = *(int *)pVarData;

	pOut->m_Int = ( data & mask );
	//CBaseEntity *pEntity = (CBaseEntity *)pProp;

	//if (pEntity)
	//{
		//if (( data & mask ) & FL_ONGROUND)
		//{
			//Msg("  [S] Transmitting FL_ONGROUND (flags=%d)\n", pEntity->GetFlags());
		//}
		//else
		//{
			//Msg("  [S] Not Transmitting FL_ONGROUND (flags=%d)\n", pEntity->GetFlags());
		//}
	//}
	//else
	//{
		//Msg(" WARNING updated flags without an ent\n");
	//}
}

IMPLEMENT_SERVERCLASS_ST(CASW_Marine, DT_ASW_Marine)

	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),	
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseCombatCharacter", "bcc_localdata" ),
		
	// asw_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	// We need to send a hi-res origin to avoid prediction errors sliding along walls
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	// send a hi-res origin to the local player for use in prediction
	//SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),	// , SENDPROP_LOCALPLAYER_ORIGINZ_PRIORITY
	
	SendPropFloat		( SENDINFO_VECTORELEM(m_vecVelocity, 0), 32, SPROP_NOSCALE|SPROP_CHANGES_OFTEN ),
	SendPropFloat		( SENDINFO_VECTORELEM(m_vecVelocity, 1), 32, SPROP_NOSCALE|SPROP_CHANGES_OFTEN ),
	SendPropFloat		( SENDINFO_VECTORELEM(m_vecVelocity, 2), 32, SPROP_NOSCALE|SPROP_CHANGES_OFTEN ),

#if PREDICTION_ERROR_CHECK_LEVEL > 1 
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 0), 13, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesX ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 1), 13, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesY ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 2), 13, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesZ ),
#else
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 0), 13, SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesX ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 1), 13, SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesY ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 2), 13, SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesZ ),
#endif

	SendPropFloat		( SENDINFO(m_fAIPitch), 0, SPROP_NOSCALE),
	SendPropInt		(SENDINFO(m_fFlags), PLAYER_FLAG_BITS, SPROP_UNSIGNED|SPROP_CHANGES_OFTEN, SendProxy_CropMarineFlagsToPlayerFlagBitsLength ),
	SendPropInt		(SENDINFO(m_iHealth), 10 ),
	SendPropInt( SENDINFO(m_iMaxHealth), 10 ),	
	SendPropFloat	(SENDINFO(m_fInfestedTime),		6,	SPROP_UNSIGNED,	0.0f,	64.0f ),
	SendPropFloat	(SENDINFO(m_fInfestedStartTime), 0, SPROP_NOSCALE ),
	SendPropInt		(SENDINFO(m_ASWOrders), 4),
	SendPropEHandle( SENDINFO ( m_Commander ) ),
	SendPropArray3		( SENDINFO_ARRAY3(m_iAmmo), SendPropInt( SENDINFO_ARRAY(m_iAmmo), 10, SPROP_UNSIGNED ) ),
	SendPropBool	(SENDINFO(m_bSlowHeal)),
	SendPropInt	(SENDINFO(m_iSlowHealAmount), 10 ),
	SendPropBool	(SENDINFO(m_bPreventMovement)),
	SendPropBool	(SENDINFO(m_bWalking) ),
	SendPropFloat	(SENDINFO(m_fFFGuardTime), 0, SPROP_NOSCALE ),
	
	SendPropEHandle( SENDINFO ( m_hUsingEntity ) ),
	SendPropEHandle( SENDINFO ( m_hCurrentHack ) ),
	
	SendPropVector( SENDINFO ( m_vecFacingPointFromServer ), 0, SPROP_NOSCALE ),
	SendPropEHandle		( SENDINFO( m_hGroundEntity ), SPROP_CHANGES_OFTEN ),

	SendPropEHandle( SENDINFO ( m_hMarineFollowTarget ) ),

	SendPropTime( SENDINFO(m_fStopMarineTime) ),
	SendPropTime( SENDINFO(m_fNextMeleeTime) ),	
	SendPropTime( SENDINFO( m_flNextAttack ) ),
	SendPropInt		( SENDINFO( m_iMeleeAttackID ), 7, SPROP_UNSIGNED ),
	
	SendPropBool	(SENDINFO(m_bOnFire)),

	// emotes
	SendPropBool	(SENDINFO(bEmoteMedic)),
	SendPropBool	(SENDINFO(bEmoteAmmo)),
	SendPropBool	(SENDINFO(bEmoteSmile)),
	SendPropBool	(SENDINFO(bEmoteStop)),
	SendPropBool	(SENDINFO(bEmoteGo)),
	SendPropBool	(SENDINFO(bEmoteExclaim)),
	SendPropBool	(SENDINFO(bEmoteAnimeSmile)),	
	SendPropBool	(SENDINFO(bEmoteQuestion)),	

	// driving
	SendPropEHandle( SENDINFO ( m_hASWVehicle ) ),
	SendPropBool	(SENDINFO(m_bDriving)),	
	SendPropBool	(SENDINFO(m_bIsInVehicle)),	

	// knocked out
	SendPropBool	(SENDINFO(m_bKnockedOut)),

	// turret	
	SendPropEHandle( SENDINFO ( m_hRemoteTurret ) ),

	// We want to send all the marine's weapons to all the other marines
	SendPropArray3( SENDINFO_ARRAY3(m_hMyWeapons), SendPropEHandle( SENDINFO_ARRAY(m_hMyWeapons) ) ),

	SendPropFloat	( SENDINFO_VECTORELEM(m_vecViewOffset, 0), 0, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_VECTORELEM(m_vecViewOffset, 1), 0, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_VECTORELEM(m_vecViewOffset, 2), 0, SPROP_NOSCALE ),
#ifdef MELEE_CHARGE_ATTACKS
	SendPropFloat	( SENDINFO(m_flMeleeHeavyKeyHoldStart), 0, SPROP_NOSCALE ),
#endif
	SendPropInt		( SENDINFO( m_iForcedActionRequest ) ),
	SendPropBool	( SENDINFO( m_bReflectingProjectiles ) ),
	SendPropTime	( SENDINFO( m_flDamageBuffEndTime ) ),
	SendPropTime	( SENDINFO( m_flElectrifiedArmorEndTime ) ),
	SendPropInt		( SENDINFO( m_iPowerupType ) ),
	SendPropTime	( SENDINFO( m_flPowerupExpireTime ) ),
	SendPropBool	( SENDINFO( m_bPowerupExpires ) ),
	SendPropFloat	( SENDINFO(m_flKnockdownYaw) ),
	SendPropFloat	( SENDINFO( m_flMeleeYaw ) ),
	SendPropBool	( SENDINFO( m_bFaceMeleeYaw ) ),
	SendPropFloat	( SENDINFO( m_flPreventLaserSightTime ) ),
	SendPropBool	( SENDINFO( m_bAICrouch ) ),
	SendPropInt		( SENDINFO( m_iJumpJetting )),
	
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Marine )
	DEFINE_FIELD( m_bSlowHeal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iSlowHealAmount, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecFacingPointFromServer, FIELD_VECTOR ),
	DEFINE_FIELD( m_hRemoteTurret, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hASWVehicle, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bDriving, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsInVehicle, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteMedic, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteAmmo, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteStop, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteGo, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteExclaim, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteAnimeSmile, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteQuestion, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_Commander, FIELD_EHANDLE),	
	DEFINE_FIELD( m_hRemoteTurret, FIELD_EHANDLE ),
	DEFINE_FIELD( m_fHoldingYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFirstBurnTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastBurnTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastBurnSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_fNextPainSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_fStartedFiringTime, FIELD_TIME ),
	DEFINE_FIELD( m_fNextSlowHealTick, FIELD_FLOAT ),
	DEFINE_FIELD( m_fInfestedTime, FIELD_TIME ),
	DEFINE_FIELD( m_fInfestedStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_fStopFacingPointTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_fLastASWThink, FIELD_TIME ),
	DEFINE_FIELD( m_iSlowHealAmount, FIELD_INTEGER ),
	DEFINE_FIELD( m_iInfestCycle, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastASWThink, FIELD_INTEGER ),
	DEFINE_FIELD( m_Commander, FIELD_EHANDLE),
	DEFINE_FIELD( m_bHacking, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hCurrentHack, FIELD_EHANDLE),
	DEFINE_FIELD( m_hUsingEntity, FIELD_EHANDLE),
	DEFINE_FIELD( m_bKnockedOut, FIELD_BOOLEAN),
	DEFINE_FIELD( m_fKickTime, FIELD_TIME ),
	DEFINE_FIELD( m_fNextMeleeTime, FIELD_TIME ),
	DEFINE_FIELD( m_fStopMarineTime, FIELD_TIME ),
	DEFINE_FIELD( m_hLastWeaponSwitchedTo, FIELD_EHANDLE ),
	// m_PlayerAnimState - recreated
	// m_MarineSpeech - recreated
	DEFINE_FIELD( m_MarineResource, FIELD_EHANDLE ),
	DEFINE_FIELD( m_nOldButtons, FIELD_INTEGER ),
	DEFINE_FIELD( m_bWantsToFire, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bWantsToFire2, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fMarineAimError, FIELD_FLOAT ),
	DEFINE_FIELD( m_ASWOrders, FIELD_INTEGER ),
	DEFINE_FIELD( m_fOverkillShootTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecOverkillPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_fUnfreezeTime, FIELD_TIME ),

	DEFINE_FIELD( m_fFFGuardTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_fLastStillTime, FIELD_TIME ),
	DEFINE_FIELD( m_bDoneWoundedRebuke, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecMoveToOrderPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_fFriendlyFireDamage, FIELD_FLOAT ),
	DEFINE_ARRAY( m_pRecentAttackers, FIELD_INTEGER, ASW_MOB_VICTIM_SIZE ),
	DEFINE_FIELD( m_fLastMobDamageTime, FIELD_TIME ),
	DEFINE_FIELD( m_bHasBeenMobAttacked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hInfestationCurer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fLastShotAlienTime, FIELD_TIME ),
	DEFINE_FIELD( m_fLastShotJunkTime, FIELD_TIME ),
	DEFINE_FIELD( m_fUsingEngineeringAura, FIELD_TIME ),
	DEFINE_FIELD( m_fCachedIdealSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_fNextAlienWalkDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_iLightLevel, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastFriendlyFireTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_fLastAmmoCheckTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_fFriendlyFireAbsorptionTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecMeleeStartPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_bFaceMeleeYaw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flMeleeStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMeleeLastCycle, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMeleeYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_bMeleeCollisionDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeComboKeypressAllowed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeComboKeyPressed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeComboTransitionAllowed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeMadeContact, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iUsableItemsOnMeleePress, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMeleeAllowMovement, FIELD_INTEGER ),
	DEFINE_FIELD( m_bMeleeKeyReleased, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPlayedMeleeHitSound, FIELD_BOOLEAN ),
#ifdef MELEE_CHARGE_ATTACKS
	DEFINE_FIELD( m_flMeleeHeavyKeyHoldStart, FIELD_FLOAT ),
	DEFINE_FIELD( m_bMeleeHeavyKeyHeld, FIELD_BOOLEAN ),
#endif
	DEFINE_FIELD( m_bMeleeChargeActivate, FIELD_BOOLEAN ),
	DEFINE_AUTO_ARRAY( m_iPredictedEvent, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_flPredictedEventTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_iNumPredictedEvents, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOnLandMeleeAttackID, FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextStumbleTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDamageBuffEndTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flElectrifiedArmorEndTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastSquadEnemyTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastSquadShotAlienTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastHurtAlienTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastAttributeExplosionSound, FIELD_TIME ),
	DEFINE_FIELD( m_iPowerupType, FIELD_INTEGER ),
	DEFINE_FIELD( m_flPowerupExpireTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bPowerupExpires, FIELD_BOOLEAN ),
END_DATADESC()

extern ConVar weapon_showproficiency;
extern ConVar asw_leadership_radius;
extern ConVar asw_buzzer_poison_duration;
extern ConVar asw_debug_marine_chatter;
extern ConVar asw_debug_medals;
extern ConVar ai_show_hull_attacks;
extern ConVar asw_medal_melee_hits;
extern int AE_MARINE_KICK;
extern int AE_MARINE_UNFREEZE;
ConVar asw_marine_stumble_on_damage( "asw_marine_stumble_on_damage", "1", FCVAR_CHEAT, "Marine stumbles when he takes damage" );
ConVar asw_stumble_interval( "asw_stumble_interval", "2.0", FCVAR_CHEAT, "Min time between stumbles" );
ConVar asw_knockdown_interval( "asw_knockdown_interval", "3.0", FCVAR_CHEAT, "Min time between knockdowns" );
ConVar asw_marine_fall_damage( "asw_marine_fall_damage", "0", FCVAR_CHEAT, "Marines take falling damage" );
ConVar asw_screenflash("asw_screenflash", "0", FCVAR_CHEAT, "Alpha of damage screen flash");
ConVar asw_damage_indicator("asw_damage_indicator", "1", FCVAR_CHEAT, "If set, directional damage indicator is shown");
ConVar asw_marine_server_ragdoll("asw_marine_server_ragdoll", "0", FCVAR_CHEAT, "If set, marines will have server ragdolls instead of clientside ones.");
ConVar asw_marine_death_protection("asw_marine_death_protection", "1", FCVAR_CHEAT, "Prevents marines from dying in one hit, unless on 1 health");
ConVar asw_marine_melee_distance("asw_marine_melee_distance", "50", FCVAR_CHEAT, "How far the marine can kick");
ConVar asw_marine_melee_damage("asw_marine_melee_damage", "20", FCVAR_CHEAT, "How much damage the marine's kick does");
ConVar asw_marine_melee_force("asw_marine_melee_force", "200000", FCVAR_CHEAT, "Marine kick force = this / dist");
ConVar asw_marine_melee_max_force("asw_marine_melee_max_force", "10000", FCVAR_CHEAT, "Maximum force allowed");
ConVar asw_marine_melee_kick_lift("asw_marine_melee_kick_lift", "0.2", FCVAR_CHEAT, "Upwards Z-Force given to kicked objects");
ConVar asw_marine_ai_acceleration("asw_marine_ai_acceleration", "4.0f", FCVAR_CHEAT, "Acceleration boost for marine AI");
ConVar asw_marine_scan_beams("asw_marine_scan_beams", "0", 0, "Draw scan beams for marines holding position");
ConVar asw_debug_marine_damage("asw_debug_marine_damage", "0", 0, "Show damage marines are taking");
ConVar asw_marine_ff("asw_marine_ff", "1", FCVAR_CHEAT, "Marine friendly fire setting (0 = FFGuard, 1 = Normal (based on mission difficulty), 2 = Always max)", true, 0, true, 2);
ConVar asw_marine_ff_guard_time("asw_marine_ff_guard_time", "5.0", FCVAR_CHEAT, "Amount of time firing is disabled for when activating friendly fire guard");
ConVar asw_marine_ff_dmg_base("asw_marine_ff_dmg_base", "1.0", FCVAR_CHEAT, "Amount of friendly fire damage on mission difficulty 5");
ConVar asw_marine_ff_dmg_step("asw_marine_ff_dmg_step", "0.2", FCVAR_CHEAT, "Amount friendly fire damage is modified per mission difficuly level away from 5");
ConVar asw_marine_ff_absorption_decay_rate("asw_marine_ff_absorption_decay_rate", "0.33f", FCVAR_CHEAT, "Rate of FF absorption decay");
ConVar asw_marine_ff_absorption_build_rate("asw_marine_ff_absorption_build_rate", "0.25f", FCVAR_CHEAT, "Rate of FF absorption decay build up when being shot by friendlies");
ConVar asw_marine_burn_time_easy("asw_marine_burn_time_easy", "6", FCVAR_CHEAT, "Amount of time marine burns for when ignited on easy difficulty");
ConVar asw_marine_burn_time_normal("asw_marine_burn_time_normal", "8", FCVAR_CHEAT, "Amount of time marine burns for when ignited on normal difficulty");
ConVar asw_marine_burn_time_hard("asw_marine_burn_time_hard", "12", FCVAR_CHEAT, "Amount of time marine burns for when ignited on hard difficulty");
ConVar asw_marine_burn_time_insane("asw_marine_burn_time_insane", "15", FCVAR_CHEAT, "Amount of time marine burns for when ignited on insane difficulty");
ConVar asw_marine_time_until_ignite("asw_marine_time_until_ignite", "0.7f", FCVAR_CHEAT, "Amount of time before a marine ignites from taking repeated burn damage");
ConVar asw_mad_firing_break("asw_mad_firing_break", "4", 0, "Point at which the mad firing counter triggers the mad firing speech");
ConVar asw_mad_firing_decay("asw_mad_firing_decay", "0.15", 0, "Tick down rate of the mad firing counter");
ConVar asw_marine_special_idle_chatter_chance("asw_marine_special_idle_chatter_chance", "0.25", 0, "Chance of marine doing a special idle chatter");
ConVar asw_force_ai_fire("asw_force_ai_fire", "0", FCVAR_CHEAT, "Forces all AI marines to fire constantly");
ConVar asw_realistic_death_chatter("asw_realistic_death_chatter", "0", FCVAR_NONE, "If true, only 1 nearby marine will shout about marine deaths");
ConVar asw_god( "asw_god", "0", FCVAR_CHEAT, "Set to 1 to make marines invulnerable" );
extern ConVar asw_sentry_friendly_fire_scale;
extern ConVar asw_marine_ff_absorption;
ConVar asw_movement_direction_tolerance( "asw_movement_direction_tolerance", "30.0", FCVAR_CHEAT );
ConVar asw_movement_direction_interval( "asw_movement_direction_interval", "0.5", FCVAR_CHEAT );

float CASW_Marine::s_fNextMadFiringChatter = 0;
float CASW_Marine::s_fNextIdleChatterTime = 0;

void CASW_Marine::DoAnimationEvent( PlayerAnimEvent_t event )
{
	if (gpGlobals->maxClients > 1 && 
				(event == PLAYERANIMEVENT_RELOAD || event == PLAYERANIMEVENT_JUMP || event == PLAYERANIMEVENT_WEAPON_SWITCH
				|| event == PLAYERANIMEVENT_HEAL || event == PLAYERANIMEVENT_KICK || event == PLAYERANIMEVENT_THROW_GRENADE
				|| event == PLAYERANIMEVENT_BAYONET || event == PLAYERANIMEVENT_PICKUP
				|| ( event >= PLAYERANIMEVENT_MELEE && event <= PLAYERANIMEVENT_MELEE_LAST ) ) )
	{
		TE_MarineAnimEventExceptCommander( this, event );	// Send to any clients other than my commander who can see this guy.
	}
	else
	{
		TE_MarineAnimEvent( this, event );	// Send to all clients who can see this guy.
	}
	MDLCACHE_CRITICAL_SECTION();
	m_PlayerAnimState->DoAnimationEvent(event);
}

void CASW_Marine::DoAnimationEventToAll( PlayerAnimEvent_t event )
{	
	TE_MarineAnimEvent( this, event );	// Send to all clients who can see this guy.
	m_PlayerAnimState->DoAnimationEvent(event);
	Msg("CASW_Marine::DoAnimationEventToAll %d\n", (int) event);
}

void CASW_Marine::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if( !IsInhabited() && nEvent == AE_MELEE_DAMAGE )
	{
		float flYawStart, flYawEnd;

		flYawStart = flYawEnd = 0.0f;

		const char* options = pEvent->options;
		const char *p = options;
		char token[256];		
		if ( options[0] )
		{
			// Read in yaw start
			p = nexttoken( token, p, ' ' );

			if( token )
			{
				flYawStart = atof( token );
			}

			// Read in yaw end
			p = nexttoken( token, p, ' ' );

			if( token )
			{
				flYawEnd = atof( token );
			}
		}

		DoMeleeDamageTrace( flYawStart, flYawEnd );
		return;
	}

	if ( GetCurrentMeleeAttack() )
	{
		if ( !GetCurrentMeleeAttack()->AllowNormalAnimEvent( this, nEvent ) )
			return;
	}
	if ( nEvent == AE_MARINE_UNFREEZE )
	{
		Msg("AE_MARINE_UNFREEZE\n");
		RemoveFlag(FL_FROZEN);
		return;
	}
	else if ( nEvent == AE_ASW_FOOTSTEP  || nEvent == AE_MARINE_FOOTSTEP )
	{
		// footsteps are played clientside
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

CASW_Marine::CASW_Marine() : m_RecentMeleeHits( 16, 16 )
{
	m_flLastEnemyYaw = 0;
	m_flLastEnemyYawTime = 0;;
	m_flAIYawOffset = 0;
	m_flNextYawOffsetTime = 0;
	m_bAICrouch = false;
	m_MarineResource = NULL;
	m_fUnfreezeTime = 0;
	m_PlayerAnimState = CreatePlayerAnimState(this, this, LEGANIM_9WAY, false);
	UseClientSideAnimation();
	m_HackedGunPos = Vector ( 0, 0, ASW_MARINE_GUN_OFFSET_Z );
	m_nOldButtons = 0;
	m_MarineSpeech = new CASW_MarineSpeech(this);
	m_flHealRateScale = 1.0f;
	m_fNextSlowHealTick = 0;
	m_fLastASWThink = gpGlobals->curtime;
	m_fInfestedTime = 0;
	m_fInfestedStartTime = 0;
	m_iInfestCycle = 0;
	m_flFirstBurnTime = 0;
	m_flLastBurnTime = 0;
	m_flLastBurnSoundTime = 0;	
	m_fNextPainSoundTime = 0;
	m_fStopFacingPointTime = 0;
	m_fHoldingYaw = 0;
	m_hKnockedOutRagdoll = NULL;
	m_fKickTime = 0;
	m_fNextMeleeTime = 0;
#ifdef MELEE_CHARGE_ATTACKS
	m_flMeleeHeavyKeyHoldStart = 0;
#endif
	m_fMadFiringCounter = 0;
	m_fUsingEngineeringAura = 0;
	m_bDoneOrderChatter = false;
	m_szInitialCommanderNetworkID[0] = '\0';
	m_bWaitingForWeld = false;
	m_flBeginWeldTime = 0.0f;

	// ai control of firing
	m_bWantsToFire = false;
	m_bWantsToFire2 = false;
	m_fMarineAimError = 0;
	m_fStopMarineTime = 0;
	m_flTimeNextScanPing = 0;
	m_flPreventLaserSightTime = 0;
	m_fLastStuckTime = 0;
	m_flFirstStuckTime = 0;
	m_fIdleChatterDelay = random->RandomInt(20, 30);
	m_fRandomFacing = 0;
	m_fNewRandomFacingTime = 0;
	m_fCachedIdealSpeed = 300.0f;

	m_flNextBreadcrumbTime = 0;
	m_flNextAmmoScanTime = 0;
	m_flResetAmmoIgnoreListTime = 0;

	m_iPowerupType = -1;
	m_flPowerupExpireTime = -1;
	m_bPowerupExpires = false;

	m_flLastSquadEnemyTime = 0.0f;
	m_flLastSquadShotAlienTime = 0.0f;
	m_flLastHurtAlienTime = 0.0f;

	m_flLastGooScanTime = 0.0f;
	m_fLastAmmoCheckTime = 0.0f;

	m_nFastReloadsInARow = 0;

	for ( int i = 0; i < ASW_MARINE_HISTORY_POSITIONS; i++ )
	{
		m_PositionHistory[ i ].vecPosition = vec3_origin;
		m_PositionHistory[ i ].flTime = 0.0f;
	}
	m_nPositionHistoryTail = -1;

	// rappel
	m_hLine = NULL;
	m_bWaitingToRappel = false;
	m_bOnGround = true;
	m_vecRopeAnchor = vec3_origin;
}


CASW_Marine::~CASW_Marine()
{
	if (GetMarineResource())
		GetMarineResource()->SetMarineEntity(NULL);
	m_PlayerAnimState->Release();
	delete m_MarineSpeech;
}

// create our custom senses class
CAI_Senses *CASW_Marine::CreateSenses()
{
	CAI_Senses *pSenses = new CASW_Marine_AI_Senses;
	pSenses->SetOuter( this );
	return pSenses;
}

void CASW_Marine::SetHeightLook( float flHeightLook )
{
	CAI_Senses *pSenses = GetSenses();
	Assert(pSenses);

	if ( pSenses )
	{
		CASW_Marine_AI_Senses *pMarineSenses = dynamic_cast<CASW_Marine_AI_Senses*>( GetSenses() );

		if ( pMarineSenses )
			pMarineSenses->SetHeightLook( flHeightLook );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Marine::SelectModel()
{
	SelectModelFromProfile();
}

void CASW_Marine::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	StopUsing();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Marine::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	SelectModel();
	SetModel( STRING( GetModelName() ) );
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetBloodColor( BLOOD_COLOR_RED );
	m_flFieldOfView		= 0.02;
	m_NPCState		= NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND );
	SetMoveType( MOVETYPE_STEP );

	m_HackedGunPos = Vector( 0, 0, ASW_MARINE_GUN_OFFSET_Z );

	// Marines collide/moveprobe as players.
	m_nAITraceMask = MASK_PLAYERSOLID;

	CapabilitiesRemove( bits_CAP_FRIENDLY_DMG_IMMUNE | bits_CAP_NO_HIT_PLAYER );

	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	// join the marines team
	ChangeFaction( FACTION_MARINES );

	SetHealth( 100 );

	NPCInit();
		 
	SetInhabited(false);
	m_Commander = NULL;
	
	m_ASWOrders = ASW_ORDER_FOLLOW;
	m_bWasFollowing = true;
	m_flFieldOfView = ASW_HOLD_POSITION_FOV_DOT;

	SetDistLook( ASW_HOLD_POSITION_SIGHT_RANGE );
	m_flDistTooFar = 1024.0f;

	SetAIWalkable(false);

	// bias the box south a bit - this feels better for FF collisions with the tilted cam
	Vector vecSurroundingMins( -16, -18, 0 );
	Vector vecSurroundingMaxs( 16, 14, 70 );
	CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &vecSurroundingMins, &vecSurroundingMaxs );

	// make sure his move_x/y pose parameters are at full moving forwards, so the AI follow movement will detect some sequence motion when calculating goal speed
	SetPoseParameter( "move_x", 1.0f );
	SetPoseParameter( "move_y", 0.0f );
}

void CASW_Marine::NPCInit()
{
	BaseClass::NPCInit();

	m_LagCompensation.Init(this);
}

unsigned int CASW_Marine::PhysicsSolidMaskForEntity( void ) const 
{ 
	return MASK_PLAYERSOLID;
}

void CASW_Marine::Precache()
{
	SelectModel();

	BaseClass::Precache();
	
	PrecacheSpeech();
	PrecacheModel("models/swarm/shouldercone/shouldercone.mdl");
	PrecacheModel("models/swarm/shouldercone/lasersight.mdl");	
	PrecacheModel( "cable/cable.vmt" );
	PrecacheScriptSound( "ASW.MarineMeleeAttack" );
	PrecacheScriptSound( "ASW.MarineMeleeAttackFP" );
	PrecacheScriptSound( "ASW.MarinePowerFistAttack" );
	PrecacheScriptSound( "ASW.MarinePowerFistAttackFP" );
	PrecacheScriptSound( "ASW_Weapon_Flamer.FlameLoop" );
	PrecacheScriptSound( "ASW_Weapon_Flamer.FlameStop" );	
	PrecacheScriptSound( "ASWFlashlight.FlashlightToggle" );
	PrecacheScriptSound( "ASW_Flare.IgniteFlare" );
	PrecacheScriptSound( "ASWScanner.Idle1" );
	PrecacheScriptSound( "ASWScanner.Idle2" );
	PrecacheScriptSound( "ASWScanner.Idle3" );
	PrecacheScriptSound( "ASWScanner.Idle4" );
	PrecacheScriptSound( "ASWScanner.Idle5" );
	PrecacheScriptSound( "ASWScanner.Idle6" );
	PrecacheScriptSound( "ASWScanner.Idle7" );
	PrecacheScriptSound( "ASWScanner.Idle8" );
	PrecacheScriptSound( "ASWScanner.Idle9" );
	PrecacheScriptSound( "ASWScanner.Idle10" );
	PrecacheScriptSound( "ASWScanner.Idle11" );
	PrecacheScriptSound( "ASWScanner.Idle12" );
	PrecacheScriptSound( "ASWScanner.Idle13" );
	PrecacheScriptSound( "ASWScanner.Warning1" );
	PrecacheScriptSound( "ASWScanner.Warning2" );
	PrecacheScriptSound( "ASWScanner.Warning3" );
	PrecacheScriptSound( "ASWScanner.Warning4" );
	PrecacheScriptSound( "ASWScanner.Warning5" );
	PrecacheScriptSound( "ASWScanner.Warning6" );
	PrecacheScriptSound( "ASWScanner.Warning7" );
	PrecacheScriptSound( "ASWScanner.Warning8" );
	PrecacheScriptSound( "ASWScanner.Warning9" );
	PrecacheScriptSound( "ASWScanner.Drawing" );
	PrecacheScriptSound( "ASW_Weapon.Reload3" );
	PrecacheScriptSound( "ASWInterface.Button3" );	
	PrecacheScriptSound( "Marine.DeathBeep" );
	PrecacheScriptSound( "ASW.MarineImpactFP" );
	PrecacheScriptSound( "ASW.MarineImpact" );
	PrecacheScriptSound( "ASW.MarineImpactHeavyFP" );
	PrecacheScriptSound( "ASW.MarineImpactHeavy" );
	PrecacheScriptSound( "ASW.MarineMeleeAttack" );
	PrecacheScriptSound( "ASW_Weapon.LowAmmoClick" );	
	PrecacheScriptSound( "ASW_ElectrifiedSuit.TurnOn" );
	PrecacheScriptSound( "ASW_ElectrifiedSuit.Loop" );
	PrecacheScriptSound( "ASW_ElectrifiedSuit.LoopFP" );
	PrecacheScriptSound( "ASW_ElectrifiedSuit.OffFP" );
	PrecacheScriptSound( "ASW.MarineBurnPain_NoIgnite" );
	PrecacheScriptSound( "ASW_Extinguisher.OnLoop" );
	PrecacheScriptSound( "ASW_Extinguisher.Stop" );
	PrecacheScriptSound( "ASW_JumpJet.Activate" );
	PrecacheScriptSound( "ASW_JumpJet.Loop" );
	PrecacheScriptSound( "ASW_JumpJet.Impact" );
	PrecacheScriptSound( "ASW_Blink.Blink" );
	PrecacheScriptSound( "ASW_Blink.Teleport" );
	PrecacheScriptSound( "ASW_XP.LevelUp" );

	PrecacheScriptSound( "ASW_Weapon.InvalidDestination" );
	PrecacheParticleSystem( "smallsplat" );						// shot
	PrecacheParticleSystem( "marine_bloodsplat_light" );		// small shot
	PrecacheParticleSystem( "marine_bloodsplat_heavy" );		// heavy shot
	PrecacheParticleSystem( "marine_hit_blood_ff" );
	PrecacheParticleSystem( "marine_hit_blood" );
	PrecacheParticleSystem( "thorns_marine_buff" );
	PrecacheParticleSystem( "marine_gib" );
	PrecacheParticleSystem( "marine_death_ragdoll" );
	PrecacheParticleSystem( "piercing_spark" );
	PrecacheParticleSystem( "jj_trail_small" );
	PrecacheParticleSystem( "jj_ground_pound" );
	PrecacheParticleSystem( "invalid_destination" );
	PrecacheParticleSystem( "Blink" );
}

void CASW_Marine::PrecacheSpeech()
{
	if (m_MarineSpeech && GetMarineResource())
		m_MarineSpeech->Precache();
}

void CASW_Marine::PhysicsSimulate( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CASW_Player *player = ToASW_Player( UTIL_PlayerByIndex( i ) );

		if ( player && player->GetMarine() == this)
		{
			InhabitedPhysicsSimulate();
			return;
		}
	}
	
	BaseClass::PhysicsSimulate();

	CASW_Weapon *pWeapon = GetActiveASWWeapon();
	if (pWeapon)
		pWeapon->ItemPostFrame();

	// check if offhand weapon needs postframe
	CASW_Weapon *pExtra = GetASWWeapon(2);
	if (pExtra && pExtra != pWeapon && pExtra->m_bShotDelayed)
	{
		pExtra->ItemPostFrame();
	}
}

#define ASW_BREADCRUMB_INTERVAL 4.0f

// think only occurs when uninhabited  (in singleplayer at least, not sure about multi)
// this is because we're not calling BaseClass::PhysicsSimulate
void CASW_Marine::Think( void )
{			
	if (!IsInhabited())
	{
		BaseClass::Think();

		ASWThinkEffects();

		m_fCachedIdealSpeed = MaxSpeed();
	}
}

CBaseCombatWeapon* CASW_Marine::ASWAnim_GetActiveWeapon()
{
	return GetActiveWeapon();
}

CASW_Marine_Profile* CASW_Marine::GetMarineProfile()
{
	CASW_Marine_Resource* pMR = GetMarineResource();
	if ( !pMR )
	{
		return NULL;
	}

	return pMR->GetProfile();
}

bool CASW_Marine::IsCurTaskContinuousMove()
{
	const Task_t* pTask = GetTask();

	// This bit of logic strikes me funny, but the case does exist. (sjb)
	if( !pTask )
		return true;

	switch( pTask->iTask )
	{
	case TASK_ASW_WAIT_FOR_FOLLOW_MOVEMENT:
	case TASK_ASW_MOVE_TO_GIVE_AMMO:
		return true;
		break;

	default:
		return BaseClass::IsCurTaskContinuousMove();
		break;
	}
}

bool CASW_Marine::ASWAnim_CanMove()
{
	return true;
}

// sets which player commands this marine
void CASW_Marine::SetCommander( CASW_Player *player )
{
	if ( m_Commander.Get() == player )
	{
		return;
	}

	m_Commander = player;

	if ( player )
	{
		player->OnMarineCommanded( this );
	}
}

// store ASWNetworkID of first commander
void CASW_Marine::SetInitialCommander(CASW_Player *player)
{
	Q_snprintf( m_szInitialCommanderNetworkID, sizeof(m_szInitialCommanderNetworkID), "%s", player ? player->GetASWNetworkID() : "None" );
	Msg( " Marine %d:%s SetInitialCommander id to %s\n", entindex(), GetEntityName(), m_szInitialCommanderNetworkID );
}

// called when a player takes direct control of this marine
void CASW_Marine::InhabitedBy(CASW_Player *player)
{
	m_vecSmoothedVelocity = vec3_origin;
	if (!IsInhabited())
	{
		m_vecSmoothedVelocity.z = GetAbsVelocity().z;
	}
	SetInhabited(true);
	

	// stop the AI firing
	m_bWantsToFire = m_bWantsToFire2 = false;
	m_fMarineAimError = 0;

	ClearForcedActionRequest();
	GetSquadFormation()->Remove( this, true );

	// always interrupt our current task when inhabiting a marine
	TaskFail(FAIL_NO_PLAYER);
	// set his schedule to holding position
	SetSchedule(SCHED_ASW_HOLD_POSITION);
	MDLCACHE_CRITICAL_SECTION();
	NPCThink();
	GetNavigator()->StopMoving( false );
	SetPoseParameter( "move_x", 0.0f );
	SetPoseParameter( "move_y", 0.0f );
	SetMoveType( MOVETYPE_WALK );
	Teleport(&GetAbsOrigin(), NULL, &m_vecSmoothedVelocity);

	m_iLightLevel = 255;	// reset light level - will get set correctly by first usercmd after this marine is inhabited
}

// called when a player stops direct control of this marine
void CASW_Marine::UninhabitedBy(CASW_Player *player)
{
	SetInhabited(false);
	if (GetActiveWeapon())
		OnUpdateShotRegulator();

	// clear any forward momentum
	Vector vel = GetLocalVelocity();	
	vel.x *= 0.8f;
	vel.y *= 0.8f;
	SetLocalVelocity(vel);

	// stop the AI firing
	m_bWantsToFire = m_bWantsToFire2 = false;
	m_fMarineAimError = 0;

	// tell the marine to hold position here
	OrdersFromPlayer(player, ASW_ORDER_HOLD_POSITION, this, true, GetLocalAngles().y);
	//SetCondition( COND_ASW_NEW_ORDERS );

	// make sure his move_x/y pose parameters are at full moving fowards, so the AI follow movement will detect some sequence motion when calculating goal speed
	MDLCACHE_CRITICAL_SECTION();
	SetPoseParameter( "move_x", 1.0f );
	SetPoseParameter( "move_y", 0.0f );

	// make sure he thinks almost immediately
	if (GetHealth() > 0)
	{
		SetThink ( &CAI_BaseNPC::CallNPCThink );
		SetNextThink( gpGlobals->curtime );
	}

	// set his schedule to holding position
	SetSchedule(SCHED_ASW_HOLD_POSITION);

	//Teleport(&GetAbsOrigin(), NULL, &vec3_origin);

	ClearForcedActionRequest();
	SetMoveType( MOVETYPE_STEP );

	m_iLightLevel = 255;	// reset light level - will get set correctly by first usercmd after this marine is inhabited

	m_nLastThinkTick = gpGlobals->tickcount - 1;	
}

void CASW_Marine::SetInhabited(bool bInhabited)
{
	if (!GetMarineResource())
		return;
	GetMarineResource()->SetInhabited(bInhabited);
}

void CASW_Marine::SetMarineResource(CASW_Marine_Resource *pMR)
{
	if (pMR != m_MarineResource)
	{
		m_MarineResource = pMR;

		PrecacheSpeech();
	}
}


void CASW_Marine::DoImpactEffect( trace_t &tr, int nDamageType )
{
	// don't do impact effects, they're simulated clientside by the tracer usermessage
	return;
	BaseClass::DoImpactEffect(tr, nDamageType);
}

void CASW_Marine::DoMuzzleFlash()
{
	return;	 // asw - muzzle flashes are triggered by tracer usermessages instead to save bandwidth

	// Our weapon takes our muzzle flash command
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->DoMuzzleFlash();
		//NOTENOTE: We do not chain to the base here
		//m_nMuzzleFlashParity = (m_nMuzzleFlashParity+1) & ((1 << EF_MUZZLEFLASH_BITS) - 1);
		BaseClass::BaseClass::DoMuzzleFlash();
	}
	else
	{
		BaseClass::DoMuzzleFlash();
	}
}

int CASW_Marine::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// make marines immune to crush damage
	if ( info.GetDamageType() & DMG_CRUSH || asw_god.GetBool() )
	{
		return 0;
	}

	CTakeDamageInfo newInfo(info);

	if ( asw_debug_marine_damage.GetBool() )
		Msg( "Marine taking premodified damage of %f\n", newInfo.GetDamage() );

	// scale sentry gun damage
	if ( newInfo.GetAttacker() && IsSentryClass( newInfo.GetAttacker()->Classify() ) )
	{
		if ( asw_sentry_friendly_fire_scale.GetFloat() <= 0 )
			return 0;

		newInfo.ScaleDamage( asw_sentry_friendly_fire_scale.GetFloat() );
	}

	// AI marines take much less damage from explosive barrels since they're too dumb to not get near them
	if ( !IsInhabited() && info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_EXPLOSIVE_BARREL )
	{
		newInfo.ScaleDamage( 0.1f );
		if ( asw_debug_marine_damage.GetBool() )
			Msg( "  Scaled AI taking damage from explosive barrel down to %f\n", newInfo.GetDamage() );
	}
	
	// don't allow FF from melee attacks
	bool bFriendlyFire = newInfo.GetAttacker() && newInfo.GetAttacker()->Classify() == CLASS_ASW_MARINE;
	if ( bFriendlyFire )
	{
		CASW_Marine *pOtherMarine = dynamic_cast<CASW_Marine*>(newInfo.GetAttacker());
		if ( pOtherMarine && pOtherMarine->GetMarineResource() )
		{
			pOtherMarine->GetMarineResource()->m_iAliensKilledSinceLastFriendlyFireIncident = 0;
		}

		if ( pOtherMarine && !pOtherMarine->IsInhabited() && !(newInfo.GetDamageType() & DMG_DIRECT) )
		{
			// don't allow any damage if it's an AI firing:  NOTE: This isn't 100% accurate, since the AI could've fired the shot, then a player switched into the marine while the projectile was in the air
			if (asw_debug_marine_damage.GetBool())
				Msg("  but all ignored, since it's from an AI\n");
			return 0;
		}
		else
		{
			if (newInfo.GetDamageType() & DMG_CLUB)
			{
				if (asw_debug_marine_damage.GetBool())
					Msg("  but all ignored, since it's FF meleee dmg\n");
				return 0;
			}			

			// drop the damage down by our absorption buffer
			bool bFlamerDot = !!(newInfo.GetDamageType() & ( DMG_BURN | DMG_DIRECT ) );
			if ( newInfo.GetDamage() > 0 && newInfo.GetAttacker() != this && !bFlamerDot )
			{
				bool bHardcoreMode = ASWGameRules() && ASWGameRules()->IsHardcoreMode();
				if ( !bHardcoreMode && asw_marine_ff_absorption.GetInt() != 0 )
				{
					float flNewDamage = info.GetDamage() * GetFFAbsorptionScale();
					newInfo.SetDamage(flNewDamage);

					if ( asw_debug_marine_damage.GetBool() )
					{
						Msg(" FF damage (%f) reduced to %f from FF absorption (%f)\n", newInfo.GetDamage(), flNewDamage, GetFFAbsorptionScale());
					}
				}
				
				GetMarineSpeech()->QueueChatter( CHATTER_FRIENDLY_FIRE, gpGlobals->curtime + 0.4f, gpGlobals->curtime + 1.0f );

				m_fLastFriendlyFireTime = gpGlobals->curtime;
			}
		}
	}

	// don't kill the marine in one hit unless he's on 1 health
	bool bKillProtection = false;
	if (asw_marine_death_protection.GetBool() && !(ASWGameRules() && ASWGameRules()->IsHardcoreMode()))	// no 1 hit protection in hardcore mode
	{
		if (newInfo.GetDamageType() != DMG_CRUSH && newInfo.GetDamageType() != DMG_FALL
				&& newInfo.GetDamageType() != DMG_INFEST && GetHealth() > 1)
		{
			if (newInfo.GetDamage() > GetHealth())
				bKillProtection = true;
		}
	}

	if ( newInfo.GetInflictor() && newInfo.GetInflictor()->Classify() == CLASS_ASW_DOOR && ASWGameRules()->GetSkillLevel() < 3 )
	{
		// Don't crush the player on easier difficulties
		Vector vDir = newInfo.GetDamageForce();
		VectorNormalize( vDir );
		newInfo.SetDamageForce( vDir * 20.0f );
		Vector vNewPos = newInfo.GetDamagePosition() + newInfo.GetDamageForce();
		vNewPos.z = GetAbsOrigin().z + 15.0f;
		Vector vNewVel = vDir * 5.0f;
		Teleport( &vNewPos, NULL, &vNewVel );
		newInfo.SetDamage( 5.0f );
	}

	// reduce damage thanks to leadership
	// see if we pass the chance
	float fChance = MarineSkills()->GetHighestSkillValueNearby(GetAbsOrigin(),
		asw_leadership_radius.GetFloat(),
		ASW_MARINE_SKILL_LEADERSHIP, ASW_MARINE_SUBSKILL_LEADERSHIP_DAMAGE_RESIST );
	static int iLeadershipResCount = 0;
	if (random->RandomFloat() < fChance)
	{
		float fNewDamage = newInfo.GetDamage() * 0.5f;
		if (fNewDamage <= 0)
			return 0;
		newInfo.SetDamage(fNewDamage);
		
		iLeadershipResCount++;

		if (asw_debug_marine_damage.GetBool())
		{			
			Msg("  Damage reduced by nearby leadership to %f (leadership resistance applied %d times so far)\n", fNewDamage, iLeadershipResCount);
		}
	}

	if ( newInfo.GetAttacker() )
	{
		// store FF damage dealt based on adjusted damage
		if (newInfo.GetAttacker()->Classify() == CLASS_ASW_MARINE)
		{
			CASW_Marine *pOtherMarine = dynamic_cast<CASW_Marine*>(newInfo.GetAttacker());
			if ( pOtherMarine && pOtherMarine->GetMarineResource() )
			{
				CASW_Marine_Resource *pMR = pOtherMarine->GetMarineResource();
				if ( pMR )
				{
					pMR->m_fFriendlyFireDamageDealt += newInfo.GetDamage();
					pMR->m_TimelineFriendlyFire.RecordValue( newInfo.GetDamage() );
				}
			}

			if ( newInfo.GetAttacker() != this )
			{
				ASWFailAdvice()->OnFriendlyFire( newInfo.GetDamage() );
			}
		}

		ApplyPassiveArmorEffects( newInfo );

		// reduce damage and shock alien if we have electrified armour on
		if ( newInfo.GetDamageType() & DMG_SLASH )
		{
			if ( IsElectrifiedArmorActive() )
			{
				CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( newInfo.GetAttacker() );
				if ( pAlien )
				{
					const float flDamageReturn = 20.0f;
					Vector vecToTarget = pAlien->WorldSpaceCenter() - WorldSpaceCenter();
					vecToTarget.z = 0;
					VectorNormalize( vecToTarget );
					Vector vecForce = vecToTarget * 20 + Vector( 0, 0, 1 ) * 10;
					CTakeDamageInfo returninfo( this, this,
						vecForce, newInfo.GetAttacker()->WorldSpaceCenter(), flDamageReturn, DMG_SHOCK | DMG_BLAST );

					pAlien->TakeDamage( returninfo );

					//pAlien->ElectroShockBig( vecForce, WorldSpaceCenter() );

					CRecipientFilter filter;
					filter.AddAllPlayers();

					UserMessageBegin( filter, "ASWEnemyZappedByThorns" );
					WRITE_SHORT( entindex() );
					WRITE_SHORT( pAlien->entindex() );
					MessageEnd();

					newInfo.ScaleDamage( 0.25f );

					if (asw_debug_marine_damage.GetBool())
					{			
						Msg("  Damage reduced by electrified armor to %f\n", newInfo.GetDamage() );
					}
				}
			}
			else // if ( newInfo.GetAttacker() )
			{
				if ( !m_bHasBeenMobAttacked )
				{
					if ( m_fLastMobDamageTime > 0.0f && m_fLastMobDamageTime + 1.5f < gpGlobals->curtime )
					{
						// It's been a while, reset
						m_pRecentAttackers[ 0 ] = newInfo.GetAttacker()->entindex();

						for ( int nAttackers = 1; nAttackers < ASW_MOB_VICTIM_SIZE; ++nAttackers )
						{
							m_pRecentAttackers[ nAttackers ] = 0;
						}
					}
					else
					{
						int nAttackers;
						for ( nAttackers = 0; nAttackers < ASW_MOB_VICTIM_SIZE; ++nAttackers )
						{
							if ( m_pRecentAttackers[ nAttackers ] == newInfo.GetAttacker()->entindex() )
							{
								break;
							}

							if ( m_pRecentAttackers[ nAttackers ] == 0 )
							{
								m_pRecentAttackers[ nAttackers ] = newInfo.GetAttacker()->entindex();
								break;
							}
						}

						if ( nAttackers >= ASW_MOB_VICTIM_SIZE )
						{
							ASWFailAdvice()->OnMarineMobAttacked();
							m_bHasBeenMobAttacked = true;
							m_fLastMobDamageTime = 0.0f;
						}
					}

					m_fLastMobDamageTime = gpGlobals->curtime;
				}
			}
		}
	}

	int iPreDamageHealth = GetHealth();
	CASW_GameStats.Event_MarineTookDamage( this, newInfo );
	int result = BaseClass::OnTakeDamage_Alive(newInfo);
	int iDamageTaken = iPreDamageHealth - GetHealth();

	if (asw_debug_marine_damage.GetBool() && result > 0)
		Msg("  Marine took final damage: %f of type %d\n", newInfo.GetDamage(), newInfo.GetDamageType());

	// notify weapons of damage
	if (GetASWWeapon(0))
		GetASWWeapon(0)->OnMarineDamage(newInfo);
	if (GetASWWeapon(1))
		GetASWWeapon(1)->OnMarineDamage(newInfo);
	if (GetASWWeapon(2))
		GetASWWeapon(2)->OnMarineDamage(newInfo);

	if ( ASWDirector() )
		ASWDirector()->MarineTookDamage( this, newInfo, bFriendlyFire );
	
	if (m_iHealth <= 0)
	{
		CASW_Door *pDoor = dynamic_cast<CASW_Door*>(newInfo.GetInflictor());
		if (!pDoor)	// can't survive damage from a falling door, even with kill protection or die hard
		{
			if (bKillProtection)
				m_iHealth = 1;
		}
	}	 

	if ( newInfo.GetDamage() > 0 )
	{
		if ( m_hCurrentHack.Get() )
		{
			ASWFailAdvice()->OnHackerHurt( newInfo.GetDamage() );
		}

		bool bShowFFIcon = bFriendlyFire;
		if ( newInfo.GetAttacker() )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "marine_hurt" );
			if ( event )
			{
				CBasePlayer *pPlayer = GetCommander();
				event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
				event->SetInt( "entindex", entindex() );
				event->SetFloat( "health", static_cast< float >( MAX( 0.0f, GetHealth() ) ) / GetMaxHealth() );

				CBasePlayer *pAttackPlayer = ToBasePlayer( newInfo.GetAttacker() );
				if ( !pAttackPlayer )
				{
					CASW_Marine *pAttackMarine = dynamic_cast< CASW_Marine* >( newInfo.GetAttacker() );
					if ( pAttackMarine )
					{
						pAttackPlayer = pAttackMarine->GetCommander();
						if ( pAttackMarine == this )
						{
							bShowFFIcon = false;
						}
					}
				}

				if ( pAttackPlayer )
				{
					event->SetInt( "attacker", pAttackPlayer->GetUserID() ); // hurt by other player
					event->SetInt( "attackerindex", newInfo.GetAttacker()->entindex() ); // hurt by entity
				}
				else
				{
					event->SetInt( "attacker", 0 ); // hurt by entity
					event->SetInt( "attackerindex", newInfo.GetAttacker()->entindex() ); // hurt by entity
				}

				gameeventmanager->FireEvent( event );
			}
		}

		if ( IsInhabited() )
		{
			//Msg("took %f damage\n", info.GetDamage());
			CASW_Player *player = GetCommander();
			if (player)
			{
				if ( asw_screenflash.GetInt() > 0 )
				{
					color32 flash_col = {128,0,0,192};
					flash_col.a = asw_screenflash.GetInt();
					UTIL_ScreenFade( player, flash_col, 1.0f, 0.1f, FFADE_IN );				
				}
				if ( asw_damage_indicator.GetBool() )
				{
					// Tell the player's client that he's been hurt.
					CSingleUserRecipientFilter user( player );
					UserMessageBegin( user, "Damage" );
					WRITE_SHORT( clamp( (int)newInfo.GetDamage(), 0, 32000 ) );
					WRITE_LONG( newInfo.GetDamageType() );
					// Tell the client whether they should show it in the indicator
					if ( !(newInfo.GetDamageType() & (DMG_DROWN | DMG_FALL | DMG_BURN | DMG_INFEST | DMG_RADIATION) ) )
					{
						WRITE_BOOL( true );
						WRITE_VEC3COORD( newInfo.GetDamagePosition() );
						WRITE_BOOL( bShowFFIcon );
					}
					else
					{
						WRITE_BOOL( false );
					}
					MessageEnd();
				}
				if (info.GetDamageType() & DMG_BLURPOISON)
				{
					float duration = asw_buzzer_poison_duration.GetFloat();
					// affect duration by mission difficulty
					if (ASWGameRules())
					{
						duration = duration + (duration * (ASWGameRules()->GetMissionDifficulty() / 10.0f));
					}
					if (duration > 0)
						UTIL_ASW_PoisonBlur( player, duration );
				}
				bool bBurnDamage = ( info.GetDamageType() & ( DMG_BURN | DMG_DIRECT ) ) != 0;
				bool bElectrifiedArmorAbsorbed = ( ( info.GetDamageType() & DMG_SLASH ) && IsElectrifiedArmorActive() );

				// play a meaty hit sound when attacked by aliens or FF
				if ( ( !bBurnDamage && !bElectrifiedArmorAbsorbed ) ) //|| bFriendlyFire )
				{
					//UTIL_ASW_ScreenShake( GetAbsOrigin(), 4.0f, 30.0f, 0.25f, 128.0f, SHAKE_START );
					if ( !info.GetInflictor() )
					{
						ASW_TransmitShakeEvent( player, 2.0f, 40.0f, 0.2f, SHAKE_START );
					}
					else
					{
						ASW_TransmitShakeEvent( player, 20.0f, 1.0f, 0.3f, SHAKE_START, (info.GetInflictor()->GetAbsOrigin() - GetAbsOrigin()).Normalized() );
					}

					// this is the sound that's played for the local player only
					CSingleUserRecipientFilter localfilter( player );
					localfilter.MakeReliable();

					// this is the sound that's played for all other players, but the local player
					CPASAttenuationFilter othersfilter( this, "ASW.MarineImpact" );
					othersfilter.RemoveRecipient( player );				

					// if they take more than 10% damage in one hit or their health is below 20%, play a bigger sound
					if ( float(iDamageTaken) / float(GetMaxHealth()) > 0.1 || float(m_iHealth) / float(GetMaxHealth()) < 0.25 )
					{
						CBaseEntity::EmitSound( localfilter, entindex(), "ASW.MarineImpactHeavyFP" );
						CBaseEntity::EmitSound( othersfilter, entindex(), "ASW.MarineImpactHeavy" );
					}
					else
					{
						CBaseEntity::EmitSound( localfilter, entindex(), "ASW.MarineImpactFP" );
						CBaseEntity::EmitSound( othersfilter, entindex(), "ASW.MarineImpact" );
					}
				}
			}
		}
		else
		{
			// AI's being hurt, check he has a weapon (and maybe switch if it's not selected)
			CheckAutoWeaponSwitch();

			// if they take more than 10% damage in one hit or their health is below 20%, play a bigger sound
			if ( float(iDamageTaken) / float(GetMaxHealth()) > 0.1 || float(m_iHealth) / float(GetMaxHealth()) < 0.25 )
			{
				EmitSound( "ASW.MarineImpactHeavy" );
			}
			else
			{
				EmitSound( "ASW.MarineImpact" );
			}
		}
	}	
	
	if (result > 0)
	{		
		// update our stats
		CASW_Marine_Resource *pMR = GetMarineResource();
		if ( pMR )
		{
			if (newInfo.GetDamage() > 0)
			{
				pMR->m_bHurt = true;

				if ( asw_debug_marine_damage.GetBool() )
				{
					Msg( "Total damage taken is %f and taking damage %f ", pMR->m_fDamageTaken, newInfo.GetDamage() );
				}

				pMR->m_fDamageTaken += iDamageTaken;
				pMR->m_TimelineHealth.RecordValue( MAX( 0, GetHealth() ) );

				if ( asw_debug_marine_damage.GetBool() )
				{
					Msg( "so new total is %f (%d/%d) %d lost\n", pMR->m_fDamageTaken, GetHealth(), GetMaxHealth(), GetMaxHealth() - GetHealth() );
				}
			}

			if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
				m_fFriendlyFireDamage += iDamageTaken;

			// check for flagging as wounded
			float fWoundDamageThreshold = 0.5f;
			if (ASWGameRules())
			{
				if (ASWGameRules()->GetSkillLevel() == 1)
					fWoundDamageThreshold = 0.4f;
				if (ASWGameRules()->GetSkillLevel() >= 3)
					fWoundDamageThreshold = 0.6f;
			}
			float fWoundPoint = GetMaxHealth() * fWoundDamageThreshold;
			if (m_iHealth < fWoundPoint)
			{
				pMR->m_bTakenWoundDamage = true;
			}
		}
		if (asw_debug_marine_damage.GetBool())
			Msg("marine took damage %f (total taken %f, ff taken %f)\n",
					newInfo.GetDamage(), pMR->m_fDamageTaken, m_fFriendlyFireDamage);

		// if we take fire damage, catch on fire
		float fPainInterval = 0.7f;

		if ( info.GetDamageType() & DMG_BURN )
		{
			ASW_Ignite( 1.0f, 0, newInfo.GetAttacker(), info.GetWeapon() );
		}

		// short stumbles on damage
		if ( !(newInfo.GetDamageType() & (DMG_BURN | DMG_DIRECT | DMG_RADIATION) ) && asw_marine_stumble_on_damage.GetBool() )
		{
			Stumble( newInfo.GetAttacker(), newInfo.GetDamageForce(), true );
		}

		// flinch
		if (gpGlobals->curtime > m_fNextPainSoundTime)
		{
			DoAnimationEvent( PLAYERANIMEVENT_FLINCH );

			if (info.GetDamage() > 35.0f)
				GetMarineSpeech()->PersonalChatter(CHATTER_PAIN_LARGE);
			else
				GetMarineSpeech()->PersonalChatter(CHATTER_PAIN_SMALL);

			m_fNextPainSoundTime = gpGlobals->curtime + fPainInterval;
		}		
	}

	return result;
}


// you can assume there is an attacker when this function is called.
void CASW_Marine::ApplyPassiveArmorEffects( CTakeDamageInfo &dmgInfo ) RESTRICT
{
	Assert( dmgInfo.GetAttacker() );
	if ( dmgInfo.GetDamageType() & (DMG_CRUSH|DMG_FALL|DMG_DROWN|DMG_PARALYZE|DMG_DROWNRECOVER|DMG_DISSOLVE) )
		return;

	// do I have ordinary (non zappy) armor?
	CASW_Weapon_Normal_Armor *pArmor = NULL;
	for (int i=0; i<ASW_MAX_MARINE_WEAPONS; ++i) 
	{
		CBaseCombatWeapon *pWep = m_hMyWeapons[i].Get();
		if ( pWep && pWep->Classify() == CLASS_ASW_NORMAL_ARMOR )
		{
			pArmor = assert_cast<CASW_Weapon_Normal_Armor *>(pWep);
			break;
		}
	}
	if ( pArmor )
	{
		dmgInfo.ScaleDamage( pArmor->GetDamageScaleFactor() );
	}
}

CASW_Weapon* CASW_Marine::GetActiveASWWeapon( void ) const
{
	return dynamic_cast<CASW_Weapon*>(GetActiveWeapon());
}

bool CASW_Marine::IsWounded() const
{
	return ( ( float(GetHealth()) / float(GetMaxHealth()) ) < 0.6f );
}

// asw todo: fix this!
bool CASW_Marine::IsAlienNear()
{
	/*
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_drone" )) != NULL)
	{
		CASW_Drone* drone = dynamic_cast<CASW_Drone*>(pEntity);
		if ( drone && drone->GetAbsOrigin().DistTo(GetAbsOrigin()) < 1220.0f)
			return true;
	}*/
	int count = AimTarget_ListCount();
	if ( count )
	{
		CBaseEntity **pList = (CBaseEntity **)stackalloc( sizeof(CBaseEntity *) * count );
		AimTarget_ListCopy( pList, count );

		for ( int i = 0; i < count; i++ )
		{			
			CBaseEntity *pEntity = pList[i];

			// Don't shoot yourself
			if ( pEntity == this )
				continue;

			if (!pEntity->IsAlive() || !pEntity->edict() )
				continue;

			// is it something we dislike?
			Disposition_t rel = CBaseCombatCharacter::IRelationType( pEntity );
			if ( rel != D_HT && rel != D_FR )
				continue;

			// if it's nearby then let's do a reload shout
			if (pEntity->GetAbsOrigin().DistToSqr(GetAbsOrigin()) < 1440000)
				return true;
		}
	}
	return false;
}

void CASW_Marine::HurtJunkItem(CBaseEntity *pEnt, const CTakeDamageInfo &info)
{
	// increase our shot timer, so shooting barrels/crates etc doesn't reduce our accuracy
	if (!(info.GetDamageType() & DMG_DIRECT))	// ignore flame DoT
	{
		m_fLastShotJunkTime = gpGlobals->curtime;
	}
}

void CASW_Marine::HurtAlien(CBaseEntity *pAlien, const CTakeDamageInfo &info)
{
	bool bMeleeDamage = ( info.GetDamageType() & DMG_CLUB ) != 0;
	if ( !bMeleeDamage )
	{
		if ( GetMarineResource() )
		{
			GetMarineResource()->m_bDealtNonMeleeDamage = true;
		}
	}
	m_flLastHurtAlienTime = gpGlobals->curtime;
	CASW_Weapon *pWeapon = GetActiveASWWeapon();
	if ( pWeapon && pAlien )
	{
		IASW_Spawnable_NPC *pNPC = dynamic_cast<IASW_Spawnable_NPC*>(pAlien);
		if ( pNPC && !(info.GetDamageType() & (DMG_BURN | DMG_BLAST | DMG_SHOCK | DMG_DIRECT) ) )
		{
			// TODO: Make sure flamer and stun grenades still work
			/*
			float flIgniteChance = 0;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flIgniteChance, mod_ignite );
			if ( RandomFloat() < flIgniteChance )
			{
				pNPC->ASW_Ignite(5.0f, 0, info.GetAttacker());
			}

			float flElectroChance = 0;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flElectroChance, mod_electro_stun );
			if ( RandomFloat() < flElectroChance )
			{
				pNPC->ElectroStun( 5.0f );
			}
			*/
		}
	}

	// don't do any chatter effects if this alien is being hurt by a burn DoT
	CASW_Burning* pBurning = dynamic_cast<CASW_Burning*>(info.GetInflictor());
	if (pBurning)
		return;

	if (!(info.GetDamageType() & DMG_DIRECT))	// ignore flame DoT
	{
		m_fLastShotAlienTime = gpGlobals->curtime;
	}

	// don't do any hurt alien affects against the little grubs or eggs
	if (pAlien && 
		(pAlien->Classify() == CLASS_ASW_GRUB ||
			pAlien->Classify()== CLASS_ASW_ALIEN_GOO ||
			pAlien->Classify() == CLASS_ASW_EGG ))
		return;

	bool bMadFiring = false;
	if (gpGlobals->curtime > s_fNextMadFiringChatter)
	{
		if (!(info.GetDamageType() & (DMG_BURN | DMG_BLAST | DMG_SHOCK)))
		{
			if (pWeapon)
			{
				//Msg("Alien hurt by a %s\n", pWeapon->GetClassname());
				m_fMadFiringCounter += pWeapon->GetMadFiringBias() * (pWeapon->GetFireRate() / pWeapon->GetNumPellets());		// add more per shot for slower weapons
				if (m_fMadFiringCounter >= asw_mad_firing_break.GetInt())
				{
					bMadFiring = true;
					m_fMadFiringCounter = 0;
					//s_fNextMadFiringChatter = gpGlobals->curtime +		// need this?
				}
			}
		}
	}

	if (bMadFiring)
	{		
		// check for autogun kill convos
		bool bSkipChatter = false;
		bool bAutogun = false;
		if (GetMarineProfile())
		{
			if ( GetMarineProfile()->m_VoiceType == ASW_VOICE_WILDCAT || GetMarineProfile()->m_VoiceType == ASW_VOICE_WOLFE )	// wildcat or wolfe
			{
				// check we're using an autogun
				bAutogun = (dynamic_cast<CASW_Weapon_Autogun*>(GetActiveASWWeapon()) != NULL);
				if (bAutogun)
				{
					if (CASW_MarineSpeech::StartConversation(CONV_AUTOGUN, this))
						bSkipChatter = true;
					if (GetMarineResource())
						GetMarineResource()->m_iMadFiringAutogun++;
				}
			}
		}
		if (!bSkipChatter)
		{
			GetMarineSpeech()->Chatter(CHATTER_MAD_FIRING);
			if (GetMarineResource() && !bAutogun)
				GetMarineResource()->m_iMadFiring++;
		}
	}
	else
		GetMarineSpeech()->Chatter(CHATTER_FIRING_AT_ALIEN);
	
	if (info.GetDamageType() & DMG_CLUB && GetMarineResource())
	{
		GetMarineResource()->m_iAliensKicked++;
		if ( IsInhabited() && GetCommander() && GetMarineResource()->m_iAliensKicked > asw_medal_melee_hits.GetInt() )
		{
			GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_MELEE_KILLS );
		}
	}
}

/*
// don't send angles when this marine is inhabited
void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	if (IsInhabited())
		return NULL;

	return BaseClass::SendProxy_Angles(pProp, pStruct, pData, pOut, iElement, objectID);
}
*/

void CASW_Marine::InhabitedPhysicsSimulate()
{
	BaseClass::InhabitedPhysicsSimulate();

	// check for deleting this
	if ( m_pfnThink == (BASEPTR)&CBaseEntity::SUB_Remove )
	{
		if (GetNextThink() > 0 && GetNextThink() < gpGlobals->curtime)
		{
			SUB_Remove();
		}
	}
}

// post think only happens when inhabited
void CASW_Marine::PostThink()
{
	//Msg("[S] Marine Postthink, sl=%d amount=%d tick=%f\n",
		//m_bSlowHeal, m_iSlowHealAmount, m_fNextSlowHealTick);
	BaseClass::PostThink();
	
	DispatchAnimEvents( this );		

	// is this correct here? should happen during/before think, not after?
	m_PlayerAnimState->Update( GetAbsAngles()[YAW], GetAbsAngles()[PITCH] );

	if (IsInhabited())
	{
		StudioFrameAdvance();
	}
		
	ASWThinkEffects();

	if ( NeedToUpdateSquad() )
	{
		GetSquadFormation()->UpdateFollowPositions();
	}

	// check for pushing out phys props jammed inside us
	if ( VPhysicsGetObject() )
	{
#ifdef ASW_MARINE_ALWAYS_VPHYSICS
		VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), vec3_angle, false, gpGlobals->frametime );
#else		
		//if ( !VectorCompare( oldOrigin, GetAbsOrigin() ) )
		if (gpGlobals->curtime <= m_fLastStuckTime + 1.0f)	// if we were stuck in the last second, make sure our shadow is on
		{
			VPhysicsGetObject()->SetVelocity( &vec3_origin, NULL );
			VPhysicsGetObject()->SetVelocityInstantaneous( &vec3_origin, &vec3_origin );
			//IPhysicsShadowController *pController = VPhysicsGetObject()->GetShadowController();
			//float teleport_dist = pController ? VPhysicsGetObject()->GetTeleportDistance() : 0;
			//if (pController)
				//VPhysicsGetObject()->SetTeleportDistance(0.1f);	// make sure the shadow teleports to its new position
			// move the marine's vphys shadow to our current position
			VPhysicsGetObject()->SetPosition( GetAbsOrigin(), vec3_angle, true );
			//VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), vec3_angle, false, gpGlobals->frametime );
			VPhysicsGetObject()->EnableCollisions(true);
			// clear its velocity, so the marine doesn't wake up phys objects - doesn't work :/
			VPhysicsGetObject()->SetVelocityInstantaneous( &vec3_origin, &vec3_origin );
			//if (pController)
				//VPhysicsGetObject()->SetTeleportDistance(teleport_dist);	
		}
		else	// no vphysics shadow enabled if we're not stuck
		{
			VPhysicsGetObject()->EnableCollisions(false);
		}
#endif		
	}
	if ( m_bCheckContacts )
	{
		CheckPhysicsContacts();
	}
}

void CASW_Marine::ASWThinkEffects()
{
	float fDeltaTime = gpGlobals->curtime - m_fLastASWThink;

	if ( gpGlobals->curtime > m_flNextPositionHistoryTime )
	{
		CASW_Marine_Resource *pMR = GetMarineResource();
		if ( pMR )
		{
			pMR->m_TimelinePosX.RecordValue( GetAbsOrigin().x );
			pMR->m_TimelinePosY.RecordValue( GetAbsOrigin().y );
		}

		AddPositionHistory();
		m_flNextPositionHistoryTime = gpGlobals->curtime + asw_movement_direction_interval.GetFloat();
	}

	if ( gpGlobals->maxClients > 1 )
	{
		m_LagCompensation.StorePositionHistory();
	}

	UpdatePowerupDuration();
	UpdateCombatStatus();

	// general timer for healing/infestation
	if ( (m_bSlowHeal || IsInfested()) && GetHealth() > 0 )
	{
		while (gpGlobals->curtime >= m_fNextSlowHealTick)
		{
			if ( m_fNextSlowHealTick == 0 )
			{
				m_fNextSlowHealTick = gpGlobals->curtime;
			}
			m_fNextSlowHealTick += ( ASW_MARINE_HEALTICK_RATE * ( 1.0f / m_flHealRateScale ) );
			// check slow heal isn't over out cap
			if (m_bSlowHeal)
			{
					//m_iSlowHealAmount = GetMaxHealth() - GetHealth();
				if ( GetHealth() >= GetMaxHealth() && !IsInfested() )		// clear all slow healing once we're at max health
				{
					ASWFailAdvice()->OnMarineOverhealed( m_iSlowHealAmount );
					m_iSlowHealAmount = 0;								//    (and not infested - infestation means we'll be constantly dropping health, so we can keep the heal around)
				}
				int amount = MIN(4, m_iSlowHealAmount);

				if (GetHealth() + amount > GetMaxHealth())
					amount = GetMaxHealth() - GetHealth();

				if (asw_debug_marine_damage.GetBool())
					Msg("SH %f: marine applied slow heal of %d\n", gpGlobals->curtime, amount);
				// change the health
				SetHealth(GetHealth() + amount);

				if ( GetMarineResource() )
				{
					GetMarineResource()->m_TimelineHealth.RecordValue( GetHealth() );
				}

				m_iSlowHealAmount -= amount;
				if (m_iSlowHealAmount <= 0)
				{
					m_bSlowHeal = false;
				}
			}

			if ( IsInfested() )
			{
				m_iInfestCycle++;
				if ( m_iInfestCycle >= 3 )	// only do the infest damage once per second
				{
					float DamagePerTick = ASWGameRules()->TotalInfestDamage() / 20.0f;  // this is also damage per second (based on standard 20 second infest time, slow heal interval of 0.33f and us only applying this every 1 in 3)
					if ( asw_debug_marine_damage.GetBool() )
						Msg("SH %f: Infest DamagePerTick %f (infest time left = %f)\n", gpGlobals->curtime, DamagePerTick, m_fInfestedTime);
					CTakeDamageInfo info(NULL, NULL, Vector(0,0,0), GetAbsOrigin(), DamagePerTick, DMG_INFEST);
					TakeDamage( info );

					if ( IsElectrifiedArmorActive() )
					{
						// Do some serious hurt to that bug each time he bites!
						CureInfestation( NULL, 0.4f );
					}

					m_iInfestCycle = 0;

					m_fInfestedTime -= 1.0f;
					if ( m_fInfestedTime <= 0 )
					{
						m_fInfestedTime = 0;

						if ( !IsInhabited() )
						{
							DoEmote( 2 );
						}

						GetMarineResource()->SetInfested(false);

						if (!m_bPlayedCureScream)	// play some effects of the parasite dying
						{
							m_bPlayedCureScream = true;
							CSoundParameters params;
							if ( CBaseEntity::GetParametersForSound( "ASW_Parasite.Death", params, NULL ) )
							{
								Vector vecOrigin = WorldSpaceCenter();
								CPASAttenuationFilter filter( vecOrigin, params.soundlevel );
								EmitSound_t ep;
								ep.m_nChannel = CHAN_AUTO;
								ep.m_pSoundName = params.soundname;
								ep.m_flVolume = params.volume;
								ep.m_SoundLevel = params.soundlevel;
								ep.m_nPitch = params.pitch;
								ep.m_pOrigin = &vecOrigin;
								CBaseEntity::EmitSound( filter, 0 /*sound.entityIndex*/, ep );
							}
							UTIL_ASW_DroneBleed( WorldSpaceCenter(), Vector(0, 0, 1), 4 );
						}

						if (m_hInfestationCurer.Get() && m_hInfestationCurer->GetMarineResource())
						{
							if ( m_hInfestationCurer->GetCommander() && m_hInfestationCurer->IsInhabited() )
							{
								m_hInfestationCurer->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_INFESTATION_CURING );
							}
							m_hInfestationCurer->GetMarineResource()->m_iCuredInfestation++;
							m_hInfestationCurer = NULL;
						}

						IGameEvent * event = gameeventmanager->CreateEvent( "marine_infested_cured" );
						if ( event )
						{
							CASW_Player *pPlayer = NULL;
							if ( m_hInfestationCurer.Get() )
							{
								pPlayer = m_hInfestationCurer->GetCommander();
							}

							event->SetInt( "entindex", entindex() );
							event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
							gameeventmanager->FireEvent( event );
						}
					}
				}				
			}
		}
	}
	// check for FFGuard time running out
	if (m_fFFGuardTime != 0 && gpGlobals->curtime > m_fFFGuardTime)
	{
		// power up weapons again (play a sound?)
	}
	if (m_hUsingEntity.Get())
	{
		IASW_Server_Usable_Entity* pUsable = dynamic_cast<IASW_Server_Usable_Entity*>(m_hUsingEntity.Get());
		if (pUsable)
		{		
			if (!pUsable->IsUsable(this))
			{
				StopUsing();				
			}
			else
			{
				pUsable->MarineUsing(this, fDeltaTime);
			}
		}
	}
	if (m_vecFacingPointFromServer.Get()!=vec3_origin && gpGlobals->curtime > m_fStopFacingPointTime)
	{
		m_vecFacingPointFromServer = vec3_origin;
	}

	// update emotes
	TickEmotes(fDeltaTime);

	// update this marine's resource
	if (GetMarineResource() && GetActiveASWWeapon())
	{
		//Msg("Updating firing for %s\n", GetMarineResource()->GetProfile()->m_ShortName);
		if (GetActiveASWWeapon()->IsFiring())
		{
			if (GetActiveASWWeapon()->IsRapidFire())
				GetMarineResource()->SetFiring(1);
			else
				GetMarineResource()->SetFiring(2);
		}
		else
		{
			GetMarineResource()->SetFiring(0);
		}
	}
	else if (GetMarineResource())
	{
		GetMarineResource()->SetFiring(0);
	}

	// unfreeze the marine sometime after he's started getting up
	if ((GetFlags() & FL_FROZEN) && m_fUnfreezeTime != 0)
	{
		if (gpGlobals->curtime >= m_fUnfreezeTime)
		{
			m_fUnfreezeTime = 0;
			RemoveFlag(FL_FROZEN);
		}
	}

	// check for kicking
	if (m_fKickTime !=0 && gpGlobals->curtime >= m_fKickTime)
	{
		m_fKickTime = 0;
		//DoKickEffect();
	}

	if ( gpGlobals->curtime > m_fLastFriendlyFireTime + 2.0f )
	{
		m_fFriendlyFireAbsorptionTime = MAX( 0, m_fFriendlyFireAbsorptionTime - fDeltaTime * asw_marine_ff_absorption_decay_rate.GetFloat() );
	}
	else
	{
		m_fFriendlyFireAbsorptionTime = MIN( 1.0f, m_fFriendlyFireAbsorptionTime + fDeltaTime * asw_marine_ff_absorption_build_rate.GetFloat() );
	}

	if ( gpGlobals->curtime > m_fLastAmmoCheckTime + 2.0f )
	{
		CheckAndRequestAmmo();
	}

	if (m_fMadFiringCounter > 0 && GetActiveASWWeapon() && !GetActiveASWWeapon()->IsReloading())	// don't tick down the mad firing counter while you're reloading
	{
		m_fMadFiringCounter -= fDeltaTime * asw_mad_firing_decay.GetFloat();

		if (asw_debug_marine_chatter.GetBool())
		{
			float fFraction = m_fMadFiringCounter / asw_mad_firing_break.GetFloat();
			//char buffer[256];
			//Q_snprintf(buffer, sizeof(buffer), );			
			engine->Con_NPrintf( 1, "MadFiringCounter: (%f) %f/%f", fFraction,
				m_fMadFiringCounter, asw_mad_firing_break.GetFloat() );
		}
	}
	//if (GetAbsVelocity() != vec3_origin || (GetMarineResource() && GetMarineResource()->IsFiring())
	bool bMoving = IsInhabited() ? GetAbsVelocity() != vec3_origin : GetSmoothedVelocity().Length()>1.0f;
	if (bMoving || (GetMarineResource() && GetMarineResource()->IsFiring())
		|| m_hUsingEntity.Get())
	
	{
		m_fLastStillTime = gpGlobals->curtime;
	}
	else
	{		
		if (gpGlobals->curtime > m_fLastStillTime + m_fIdleChatterDelay && m_fLastStillTime != 0)
		{			
			if (asw_debug_marine_chatter.GetBool())
				Msg("%s trying to idle chatter (cur=%f snextidle=%f idlechatdelay=%f(%f)\n", 
					GetMarineProfile()->m_ShortName,
					gpGlobals->curtime, s_fNextIdleChatterTime, m_fIdleChatterDelay, m_fLastStillTime + m_fIdleChatterDelay
					);
			if (gpGlobals->curtime > s_fNextIdleChatterTime && GetMarineSpeech()->AllowCalmConversations(-1))	// check no-one else idle chatted recently
			{
				if (asw_debug_marine_chatter.GetBool())
					Msg("  and there's a free gap, woot!\n");
				bool bDidIdle = false;

				// do a special idle?
				if (random->RandomFloat() < asw_marine_special_idle_chatter_chance.GetFloat())
				{
					int iTryConversation = CONV_NONE;
					if (GetMarineProfile()->m_VoiceType == ASW_VOICE_SARGE)
					{
						if (random->RandomFloat() < 0.5f)
							iTryConversation = CONV_SARGE_IDLE;
						else
							iTryConversation = CONV_SARGE_JAEGER_CONV1;
					}
					else if (GetMarineProfile()->m_VoiceType == ASW_VOICE_JAEGER)
					{
						if (random->RandomFloat() < 0.5f)
							iTryConversation = CONV_STILL_BREATHING;
						else
							iTryConversation = CONV_SARGE_JAEGER_CONV2;
					}
					else if (GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH)
					{
						if (random->RandomFloat() < 0.5f)
							iTryConversation = CONV_CRASH_COMPLAIN;
						else
							iTryConversation = CONV_CRASH_IDLE;
					}
					else if (GetMarineProfile()->m_VoiceType == ASW_VOICE_WOLFE)
					{
						iTryConversation = CONV_WOLFE_BEST;
					}
					else if (GetMarineProfile()->m_VoiceType == ASW_VOICE_VEGAS)
					{
						iTryConversation = CONV_TEQUILA;
					}
					if (iTryConversation != CONV_NONE && CASW_MarineSpeech::StartConversation(iTryConversation, this))
						bDidIdle = true;
					else if (GetMarineSpeech()->Chatter(CHATTER_IDLE))	// fall back to regular idle if the conversation failed to start
						bDidIdle = true;
				}
				else if (GetMarineSpeech()->Chatter(CHATTER_IDLE))
					bDidIdle = true;

				if (bDidIdle)
					s_fNextIdleChatterTime = gpGlobals->curtime + random->RandomInt(15, 25);
					//s_fNextIdleChatterTime = gpGlobals->curtime + random->RandomInt(30, 60);
				//m_fIdleChatterDelay = random->RandomInt(10, 20);
				m_fIdleChatterDelay = random->RandomInt(25, 40);
			}
			else
			{
				if (asw_debug_marine_chatter.GetBool())
					Msg(" but we can't cos someone else did an idle chat recently\n");
				//m_fIdleChatterDelay = random->RandomInt(5, 10);
				m_fIdleChatterDelay = random->RandomInt(20, 35);
			}
			m_fLastStillTime = gpGlobals->curtime;
			
		}	
	}
	GetMarineSpeech()->Update();

	if (asw_debug_medals.GetBool() && IsInhabited() && gpGlobals->maxClients <= 1)
	{
		GetMarineResource()->DebugMedalStatsOnScreen();
	}

	// network down if we're applying our engineering aura
	if (GetMarineResource())
		GetMarineResource()->m_bUsingEngineeringAura = (m_fUsingEngineeringAura >= gpGlobals->curtime - 0.2f);

	if (!IsInhabited())
	{
		// uninhabited ticking of the hacking puzzles
		if (m_hCurrentHack.Get())
		{
			CUserCmd ucmd;
			m_hCurrentHack->ASWPostThink(NULL, this, &ucmd, fDeltaTime);		// todo send deltatime parameter
		}

		// uninhabited post frame for offhand equip
		CASW_Weapon *pExtra = GetASWWeapon(2);
		if (pExtra && pExtra != GetActiveWeapon() && pExtra->WantsOffhandPostFrame() )
		{
			float flSavedFrameTime = gpGlobals->frametime;
			gpGlobals->frametime = fDeltaTime;
			pExtra->ItemPostFrame();
			gpGlobals->frametime = flSavedFrameTime;
		}
	}

	if ( gpGlobals->curtime > m_flNextBreadcrumbTime )
	{
		CASW_GameStats.Event_MarineBreadcrumb( this );
		m_flNextBreadcrumbTime = gpGlobals->curtime + ASW_BREADCRUMB_INTERVAL;
	}

	// we've been burned relatively recently
	if ( m_flFirstBurnTime > 0 )
	{
		float flGraceTime = asw_marine_time_until_ignite.GetFloat();
		// if we haven't been burned in the last chunk of the total time-to-ignite, reset the timer
		// additionally, if our initial burn time is over the time-to-ignite time, reset
		if ( (gpGlobals->curtime - m_flFirstBurnTime) > flGraceTime + 1.0f )
			m_flFirstBurnTime = 0;
	}

	m_fLastASWThink = gpGlobals->curtime;
}
/*
void CASW_Marine::Activate( void )
{
	BaseClass::Activate();

	// Find all drones 
	CBaseEntity *pObject = NULL;
	while ( ( pObject = gEntList.FindEntityByClassname( pObject, "asw_drone" ) ) != NULL )
	{
		// Tell the AI sensing list that we want to consider this
		g_AI_SensedObjectsManager.AddEntity( pObject );		
	}
}
*/

bool CASW_Marine::AIWantsToFire()
{
	return m_bWantsToFire || (asw_force_ai_fire.GetBool());
}

bool CASW_Marine::AIWantsToFire2()
{
	return m_bWantsToFire2;
}

bool CASW_Marine::AIWantsToReload()
{
	return false;
}

void CASW_Marine::FlashlightToggle()
{
	if (IsEffectActive( EF_DIMLIGHT ))
		FlashlightTurnOff();
	else
		FlashlightTurnOn();
}

void CASW_Marine::FlashlightTurnOn( void )
{
	if (!IsEffectActive( EF_DIMLIGHT ))
	{
		AddEffects( EF_DIMLIGHT );
		EmitSound( "ASWFlashlight.FlashlightToggle" );	
	}
}

void CASW_Marine::FlashlightTurnOff( void )
{
	if (IsEffectActive( EF_DIMLIGHT ))
	{
		EmitSound( "ASWFlashlight.FlashlightToggle");
		RemoveEffects( EF_DIMLIGHT );
	}	
}

bool CASW_Marine::HasFlashlight()
{
	for (int i=0; i<MAX_WEAPONS; ++i) 
	{
		CASW_Weapon_Flashlight *pFlashlight = dynamic_cast<CASW_Weapon_Flashlight*>(m_hMyWeapons[i].Get());
		if (pFlashlight)
			return true;
	}
	return false;
}	

// player (and player controlled marines) always avoid marines
bool CASW_Marine::ShouldPlayerAvoid( void )
{
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// as basecombat char, but we don't allow picking up of ammo unless we are
//  holding a gun that uses it
// also allow double the max if we're carrying two guns of that type
int CASW_Marine::GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound)
{
	int iGuns = GetNumberOfWeaponsUsingAmmo(iAmmoIndex);
	if (iGuns <= 0)
		return 0;

	if (iCount <= 0)
		return 0;

	//if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	//{
		// game rules say I can't have any more of this ammo type.
		//return 0;
	//}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
		return 0;

	int iMax = GetAmmoDef()->MaxCarry(iAmmoIndex, this) * iGuns;
	int iAdd = MIN( iCount, iMax - m_iAmmo[iAmmoIndex] );
	if ( iAdd < 1 )
		return 0;

	// Ammo pickup sound
	if ( !bSuppressSound )
	{
		EmitSound( "BaseCombatCharacter.AmmoPickup" );
	}

	m_iAmmo.Set( iAmmoIndex, m_iAmmo[iAmmoIndex] + iAdd );

	return iAdd;
}

int CASW_Marine::GiveAmmoToAmmoBag( int iCount, int iAmmoIndex, bool bSuppressSound)
{
	if (iCount <= 0)
		return 0;

	//if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	//{
		// game rules say I can't have any more of this ammo type.
//		return 0;
	//}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
		return 0;

	CASW_Weapon_Ammo_Bag *pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(GetWeapon(0));
	if (!pBag || !pBag->HasRoomForAmmo(iAmmoIndex))
	{
		pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(GetWeapon(1));
		if (!pBag || !pBag->HasRoomForAmmo(iAmmoIndex))
			return 0;
	}

	// Ammo pickup sound
	if ( !bSuppressSound )
	{
		EmitSound( "BaseCombatCharacter.AmmoPickup" );
	}

	int iAdd = pBag->AddAmmo(iCount, iAmmoIndex);

	return iAdd;
}

bool CASW_Marine::CanGiveAmmoTo( CASW_Marine* pMarine )
{
	// iterate over my weapons to find ammo bag(s)
	for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
	{
		CASW_Weapon_Ammo_Bag *pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>( GetASWWeapon( iWeapon ) );
		if (pAmmoBag)
		{
			// see if the ammo bag can give ammo for any weapons the recipient has
			for ( int iRecipientWeapon = 0; iRecipientWeapon < ASW_NUM_INVENTORY_SLOTS; iRecipientWeapon++ )
			{
				CASW_Weapon *pRecipientWeapon = pMarine->GetASWWeapon( iRecipientWeapon );
				if (pAmmoBag->CanGiveAmmoToWeapon(pRecipientWeapon))
					return true;
			}
		}
	}

	return false;
}

bool CASW_Marine::CarryingAGunThatUsesAmmo( int iAmmoIndex)
{
	int n = WeaponCount();
	for (int i=0;i<n;i++)
	{
		CBaseCombatWeapon* pWeapon = GetWeapon(i);
		if (!pWeapon)
			continue;
		if (pWeapon->GetPrimaryAmmoType() == iAmmoIndex
			|| pWeapon->GetSecondaryAmmoType() == iAmmoIndex)
			return true;
	}
	return false;
}

void CASW_Marine::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	// Add the weapon to my weapon inventory
	for (int i=0;i<MAX_WEAPONS;i++) 
	{
		if (!m_hMyWeapons[i]) 
		{
			m_hMyWeapons.Set( i, pWeapon );
			break;
		}
	}

	Weapon_Equip_Post(pWeapon);
}

void CASW_Marine::Weapon_Equip_In_Index( CBaseCombatWeapon *pWeapon, int index )
{
	if (GetWeapon(index)==NULL)
	{
		m_hMyWeapons.Set(index, pWeapon);

		Weapon_Equip_Post(pWeapon);
	}		
}

// some things to do on the weapon after it's been put into the array
void CASW_Marine::Weapon_Equip_Post( CBaseCombatWeapon *pWeapon)
{
	// Weapon is now on my team
	pWeapon->ChangeTeam( GetTeamNumber() );

	// ----------------------
	//  Give Primary Ammo
	// ----------------------
	// If gun doesn't use clips, just give ammo
	if (pWeapon->GetMaxClip1() == -1)
	{
		GiveAmmo(pWeapon->GetDefaultClip1(), pWeapon->m_iPrimaryAmmoType); 
	}
	// If default ammo given is greater than clip
	// size, fill clips and give extra ammo
	else if (pWeapon->GetDefaultClip1() >  pWeapon->GetMaxClip1() )
	{
		pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		GiveAmmo( (pWeapon->GetDefaultClip1() - pWeapon->GetMaxClip1()), pWeapon->m_iPrimaryAmmoType); 
	}

	// ----------------------
	//  Give Secondary Ammo
	// ----------------------
	// If gun doesn't use clips, just give ammo
	if (pWeapon->GetMaxClip2() == -1)
	{
		GiveAmmo(pWeapon->GetDefaultClip2(), pWeapon->m_iSecondaryAmmoType); 
	}
	// If default ammo given is greater than clip
	// size, fill clips and give extra ammo
	else if ( pWeapon->GetDefaultClip2() > pWeapon->GetMaxClip2() )
	{
		pWeapon->m_iClip2 = pWeapon->GetMaxClip2();
		GiveAmmo( (pWeapon->GetDefaultClip2() - pWeapon->GetMaxClip2()), pWeapon->m_iSecondaryAmmoType); 
	}

	pWeapon->Equip( this );

	// Gotta do this *after* Equip because it may whack maxRange
	if ( IsPlayer() == false )
	{
		// If SF_NPC_LONG_RANGE spawn flags is set let weapon work from any distance
		if ( HasSpawnFlags(SF_NPC_LONG_RANGE) )
		{
			m_hActiveWeapon->m_fMaxRange1 = 999999999;
			m_hActiveWeapon->m_fMaxRange2 = 999999999;
		}
	}

	WeaponProficiency_t proficiency;
	proficiency = CalcWeaponProficiency( pWeapon );
	
	if( weapon_showproficiency.GetBool() != 0 )
	{
		Msg("%s equipped with %s, proficiency is %s\n", GetClassname(), pWeapon->GetClassname(), GetWeaponProficiencyName( proficiency ) );
	}

	SetCurrentWeaponProficiency( proficiency );

	// Pass the lighting origin over to the weapon if we have one
	pWeapon->SetLightingOrigin( GetLightingOrigin() );
}

// all marines will send all their weapons to everyone
//   since it's only 3 weapons per marine and knowing the other players' stuff will be handy
void CASW_Marine::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Skip this work if we're already marked for transmission.
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	for ( int i=0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
		if ( !pWeapon )
			continue;

		// The local player is sent all of his weapons.
		pWeapon->SetTransmit( pInfo, bAlways );
	}
}

void CASW_Marine::TookAmmoPickup( CBaseEntity* pAmmoPickup )
{
	//DoAnimationEvent(PLAYERANIMEVENT_PICKUP);

	CASW_GameStats.Event_MarineTookPickup( this, pAmmoPickup, NULL );
}

bool CASW_Marine::TakeWeaponPickup( CASW_Weapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	// find the index this weapon is meant to go in
	int index = GetWeaponPositionForPickup(pWeapon->GetClassname());
	// is there already a weapon in this slot?
	CASW_Weapon* pOldWeapon = GetASWWeapon(index);

	// check we're allowed to take this item
	bool bAllowed = true;
	if (pOldWeapon)	// we're swapping with an existing weapon
		bAllowed = ASWGameRules()->MarineCanPickup(GetMarineResource(), pWeapon->GetClassname(), pOldWeapon->GetClassname());
	else	// we're putting it into an empty slot
		bAllowed = ASWGameRules()->MarineCanPickup(GetMarineResource(), pWeapon->GetClassname());
	if (!bAllowed)
		return false;

	CASW_GameStats.Event_MarineTookPickup( this, pWeapon, pOldWeapon );

	bool bReplace = ( pOldWeapon != NULL );

	// if we're swapping with a current weapon, drop it
	if ( bReplace )
	{
		if (!DropWeapon(index, true))		// todo: set the pickup denial error
			return false;
	}		

	// If I have a name, make my weapon match it with "_weapon" appended
	if ( GetEntityName() != NULL_STRING )
	{
		const char* weapon_name = UTIL_VarArgs("%s_weapon", STRING(GetEntityName()));
		pWeapon->SetName( AllocPooledString(weapon_name) );
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
	if ( event )
	{
		CASW_Player *pPlayer = GetCommander();
		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
		event->SetInt( "entindex", pWeapon->entindex() );
		event->SetString( "classname", pWeapon->GetWeaponInfo()->szClassName );
		event->SetInt( "slot", index );
		event->SetBool( "replace", bReplace );
		event->SetBool( "offhand", pWeapon->GetWeaponInfo()->m_bOffhandActivate );

		gameeventmanager->FireEvent( event );
	}

	// equip the weapon
	Weapon_Equip_In_Index( pWeapon, index );

	// set the number of clips
	if (pWeapon->GetPrimaryAmmoType()!=-1)
		GiveAmmo(pWeapon->GetPrimaryAmmoCount(), pWeapon->GetPrimaryAmmoType());

	//maybe switch to this weapon, if current is none
	if (GetActiveWeapon()==NULL)
	{
		Weapon_Switch( pWeapon );
	}
	else
	{
		pWeapon->SetWeaponVisible(false);
	}

	CheckAndRequestAmmo();

	GetMarineResource()->UpdateWeaponIndices();

	return true;
}

bool CASW_Marine::TakeWeaponPickup(CASW_Pickup_Weapon* pPickup)
{
	// find the index this weapon is meant to go in
	int index = GetWeaponPositionForPickup(pPickup->GetWeaponClass());
	// is there already a weapon in this slot?
	CASW_Weapon* pWeapon = GetASWWeapon( index );

	// check we're allowed to take this item
	bool bAllowed = true;
	if ( pWeapon )	// we're swapping with an existing weapon
	{
		bAllowed = ASWGameRules()->MarineCanPickup( GetMarineResource(), pPickup->GetWeaponClass(), pWeapon->GetClassname() );
	}
	else	// we're putting it into an empty slot
	{
		bAllowed = ASWGameRules()->MarineCanPickup( GetMarineResource(), pPickup->GetWeaponClass() );
	}

	if ( !bAllowed )
	{
		return false;
	}

	CASW_GameStats.Event_MarineTookPickup( this, pPickup, pWeapon );

	// if we're swapping with a current weapon, drop it
	bool bReplace = ( pWeapon != NULL );
	if ( bReplace )
	{
		if ( !DropWeapon( index, true ) )		// todo: set the pickup denial error
		{
			return false;
		}
	}		

	// give ourselves this weapon, in the right slot
	//Msg("CASW_Marine::TakeWeaponPickup calling Weapon_Create %s\n", pPickup->GetWeaponClass());
	pWeapon = dynamic_cast<CASW_Weapon*>(Weapon_Create(pPickup->GetWeaponClass()));

	if ( pWeapon )
	{
		// If I have a name, make my weapon match it with "_weapon" appended
		if ( GetEntityName() != NULL_STRING )
		{
			const char* weapon_name = UTIL_VarArgs("%s_weapon", STRING(GetEntityName()));
			pWeapon->SetName( AllocPooledString(weapon_name) );
		}

		// set + take ammo accordingly		
		pPickup->InitWeapon(this, pWeapon);

		//maybe switch to this weapon, if current is none
		if ( GetActiveWeapon() == NULL )
		{
			Weapon_Switch( pWeapon );
		}
		else
		{
			pWeapon->SetWeaponVisible(false);
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
		if ( event )
		{
			CASW_Player *pPlayer = GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", pWeapon->entindex() );
			event->SetString( "classname", pWeapon->GetWeaponInfo()->szClassName );
			event->SetInt( "slot", index );
			event->SetBool( "replace", bReplace );
			event->SetBool( "offhand", pWeapon->GetWeaponInfo()->m_bOffhandActivate );

			gameeventmanager->FireEvent( event );
		}

		// destroy the pickup
		pPickup->Remove();

		GetMarineResource()->UpdateWeaponIndices();

		CheckAndRequestAmmo();

		return true;
	}
	if ( !GetASWWeapon( index ) )
	{
		SwitchToNextBestWeapon( NULL );
	}

	return false;		// todo: clear the pickup denial error
}

bool CASW_Marine::DropWeapon(int iWeaponIndex, bool bNoSwap)
{
	CASW_Weapon* pWeapon = GetASWWeapon(iWeaponIndex);
	if (!pWeapon)
		return false;

	RemoveWeaponPowerup( pWeapon );

	return DropWeapon(pWeapon, bNoSwap);
}

bool CASW_Marine::DropWeapon(CASW_Weapon* pWeapon, bool bNoSwap, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	RemoveWeaponPowerup( pWeapon );
	
	// dropping the weapon entity itself

	// set clips in the dropped weapon
	int iAmmoIndex = pWeapon->GetPrimaryAmmoType();
	int bullets_on_player = GetAmmoCount(iAmmoIndex);
	int iClips = bullets_on_player / pWeapon->GetMaxClip1();

	if (GetNumberOfWeaponsUsingAmmo(iAmmoIndex) > 1)
	{
		// need to leave at least X clips with the marine, since he has a gun using this ammo
		int iMax = GetAmmoDef()->MaxCarry(iAmmoIndex, this);
		int iKeep = MAX(0, (bullets_on_player - iMax));
		iClips = iKeep / pWeapon->GetMaxClip1();
	}
	pWeapon->SetPrimaryAmmoCount( iClips * pWeapon->GetMaxClip1() );

	// remove ammo from the marine correspondingly
	if (iAmmoIndex != -1)
	{
		int current_bullets = GetAmmoCount(pWeapon->GetPrimaryAmmoType());
		current_bullets -= pWeapon->GetPrimaryAmmoCount();
		if (current_bullets < 0)
			current_bullets = 0;
		SetAmmoCount(current_bullets, pWeapon->GetPrimaryAmmoType());
	}

	// throw the weapon a bit
	Vector vecForward = BodyDirection2D();

	QAngle gunAngles;
	VectorAngles( vecForward, gunAngles );

	//=========================================
	// Teleport the weapon to the player's hand
	//=========================================
	int iBIndex = -1;
	int iWeaponBoneIndex = -1;

	MDLCACHE_CRITICAL_SECTION();
	CStudioHdr *hdr = pWeapon->GetModelPtr();
	// If I have a hand, set the weapon position to my hand bone position.
	if ( hdr && hdr->numbones() > 0 )
	{
		// Assume bone zero is the root
		for ( iWeaponBoneIndex = 0; iWeaponBoneIndex < hdr->numbones(); ++iWeaponBoneIndex )
		{
			iBIndex = LookupBone( hdr->pBone( iWeaponBoneIndex )->pszName() );
			// Found one!
			if ( iBIndex != -1 )
			{
				break;
			}
		}

		if ( iBIndex == -1 )
		{
			iBIndex = LookupBone( "ValveBiped.bip01_R_Hand" );
		}
	}
	else
	{
		iBIndex = LookupBone( "ValveBiped.bip01_R_Hand" );
	}

	if ( iBIndex != -1)  
	{
		Vector origin;
		QAngle angles;
		matrix3x4_t transform;

		// Get the transform for the weapon bonetoworldspace in the NPC
		GetBoneTransform( iBIndex, transform );

		// find offset of root bone from origin in local space
		// Make sure we're detached from hierarchy before doing this!!!
		pWeapon->StopFollowingEntity();
		pWeapon->SetAbsOrigin( Vector( 0, 0, 0 ) );
		pWeapon->SetAbsAngles( QAngle( 0, 0, 0 ) );
		pWeapon->InvalidateBoneCache();

		matrix3x4_t rootLocal;
		if ( iWeaponBoneIndex < hdr->numbones() )
		{
			pWeapon->GetBoneTransform( iWeaponBoneIndex, rootLocal );
		}
		else
		{
			SetIdentityMatrix( rootLocal );
		}

		// invert it
		matrix3x4_t rootInvLocal;
		MatrixInvert( rootLocal, rootInvLocal );

		matrix3x4_t weaponMatrix;
		ConcatTransforms( transform, rootInvLocal, weaponMatrix );
		MatrixAngles( weaponMatrix, angles, origin );

		// Ensure this position isn't through a wall.
		Vector vMarineCenter = WorldSpaceCenter();
		trace_t tr;
		Ray_t ray;
		ray.Init( vMarineCenter, origin, pWeapon->ScriptGetBoundingMins(), pWeapon->ScriptGetBoundingMaxs() );
		UTIL_TraceRay( ray, MASK_SOLID, this, COLLISION_GROUP_WEAPON, &tr );

		if ( tr.DidHit() )
		{
			// We hit something... shove it back toward the marine
			Vector vTraceDir = origin - vMarineCenter;
			float fLength = VectorNormalize( vTraceDir );
			origin = GetAbsOrigin() + vTraceDir * fLength * tr.fraction - vTraceDir * 0.5f * pWeapon->ScriptGetBoundingMaxs().DistTo( pWeapon->ScriptGetBoundingMins() );
		}

		if ( origin.z < vMarineCenter.z )
		{
			// Prevent stuff falling out of the world when the marines hands are very low (they're doing a bending down anim)
			origin.z = vMarineCenter.z;
		}

		pWeapon->Teleport( &origin, &angles, NULL );

		//Have to teleport the physics object as well

		IPhysicsObject *pWeaponPhys = pWeapon->VPhysicsGetObject();

		if( pWeaponPhys )
		{
			Vector vPos;
			QAngle vAngles;
			pWeaponPhys->GetPosition( &vPos, &vAngles );
			pWeaponPhys->SetPosition( vPos, angles, true );

			AngularImpulse	angImp(0,0,0);
			Vector vecAdd = GetAbsVelocity();
			pWeaponPhys->AddVelocity( &vecAdd, &angImp );
		}
	}
	else
	{
		pWeapon->SetAbsOrigin( GetAbsOrigin() + vecForward * 80.0f );
	}

	Vector vecThrow;
	ThrowDirForWeaponStrip( pWeapon, vecForward, &vecThrow );

	// apply the desired velocity, if any
	if ( pvecTarget )
	{
		// I've been told to throw it somewhere specific.
		vecThrow = VecCheckToss( this, pWeapon->GetAbsOrigin(), *pvecTarget, 0.2, 1.0, false );
	}
	else
	{
		if ( pVelocity )
		{
			vecThrow = *pVelocity;
			float flLen = vecThrow.Length();
			if ( flLen > 400 )
			{
				VectorNormalize( vecThrow );
				vecThrow *= 400;
			}
		}
		else
		{
			// Nowhere in particular; just drop it.
			float throwForce = ( IsPlayer() ) ? 400.0f : random->RandomInt( 64, 128 );
			vecThrow = BodyDirection3D() * throwForce;
		}
	}

	pWeapon->Drop( vecThrow );
	pWeapon->MarineDropped( this );
	Weapon_Detach( pWeapon );

	// switch to the next weapon, if any
	if ( !bNoSwap )
	{
		if ( SwitchToNextBestWeapon( NULL ) )
		{
			// explicitly tell this client to play the weapon switch anim, since he didn't predict this change
			TE_MarineAnimEventJustCommander(this, PLAYERANIMEVENT_WEAPON_SWITCH);
		}
	}

	GetMarineResource()->UpdateWeaponIndices();
		
	return true;
}

void CASW_Marine::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	CASW_Weapon* pASWWeapon = dynamic_cast<CASW_Weapon*>(pWeapon);
	if (!pASWWeapon)
		return;

	DropWeapon(pASWWeapon, false, pvecTarget, pVelocity);
}

// healing
void CASW_Marine::AddSlowHeal( int iHealAmount, float flHealRateScale, CASW_Marine *pMedic, CBaseEntity* pHealingWeapon /*= NULL */ )
{
	if ( GetHealth() <= 0 )
		return;

	if (iHealAmount > 0)
	{
		m_flHealRateScale = flHealRateScale;
		if (!m_bSlowHeal)
		{
			m_bSlowHeal = true;
			m_fNextSlowHealTick = gpGlobals->curtime + ( ASW_MARINE_HEALTICK_RATE * ( 1.0f / m_flHealRateScale ) );
		}
		m_iSlowHealAmount += iHealAmount;

		//if (!m_bSlowHeal)
		//{
		//	m_bSlowHeal = true;
		//	m_fNextSlowHealTick = gpGlobals->curtime + 0.33f;
		//}
		//m_iSlowHealAmount += iHealAmount;

		// subtract FF from the amount healed, before storing it in the medic's healing stats
		int iMedicMedalHealed = iHealAmount;
		// don't give credit for healing more than the marine's max health
		if (GetHealth() + iHealAmount > GetMaxHealth())
		{
			iMedicMedalHealed = GetMaxHealth() - GetHealth();
			//Msg("  healing more than we have health for, so cutting down to %d\n", iMedicMedalHealed);
		}

		//Msg("healing marine for %d, ff damage is %f\n", iMedicMedalHealed, m_fFriendlyFireDamage);
		if (m_fFriendlyFireDamage > 0)
		{
			iMedicMedalHealed -= m_fFriendlyFireDamage;

			m_fFriendlyFireDamage -= iHealAmount;
			if (m_fFriendlyFireDamage < 0)
				m_fFriendlyFireDamage = 0;
				
			//Msg(" So new medic medal healed is %d and new m_fFriendlyFireDamage is %f\n", iMedicMedalHealed, m_fFriendlyFireDamage);
		}
		if (iMedicMedalHealed > 0 && pMedic != this && pMedic && pMedic->GetMarineResource())
		{
			pMedic->GetMarineResource()->m_iMedicHealing += iMedicMedalHealed;
		}

		// healing puts out fires
		if (IsOnFire())
		{
			Extinguish();
			/*
			CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( GetEffectEntity() );
			if ( pFireChild )
			{
				SetEffectEntity( NULL );
				UTIL_Remove( pFireChild );				
				Extinguish();
			}
			*/
		}

		// count heal for stats
		if (GetMarineResource())
			GetMarineResource()->m_iHealCount++;

		// Fire heal event for stat tracking
		CASW_GameStats.Event_MarineHealed( this , iHealAmount, pHealingWeapon );
	}
}	

// using entities over time
bool CASW_Marine::StartUsing( CBaseEntity* pEntity )
{	
	if ( GetHealth() <= 0 )
		return false;

	IASW_Server_Usable_Entity* pUsable = dynamic_cast< IASW_Server_Usable_Entity* >( pEntity );
	if ( pUsable )
	{
		if ( !pUsable->IsUsable( this ) )
			return false;

		if ( !pUsable->RequirementsMet( this ) )
			return false;
		
		pUsable->MarineStartedUsing( this );
		m_hUsingEntity = pEntity;
		return true;
	}
	return false;
}

void CASW_Marine::StopUsing()
{	
	if (!m_hUsingEntity)
		return;

	IASW_Server_Usable_Entity* pUsable = dynamic_cast<IASW_Server_Usable_Entity*>(m_hUsingEntity.Get());
	if (pUsable)
	{		
		pUsable->MarineStoppedUsing(this);
	}

	m_hUsingEntity = NULL;
	m_hAreaToUse = NULL;		// FIXME: This might accidently clear new orders if the marine was just ordered to a new area!
}

// marine has been hit by a melee attack
void CASW_Marine::MeleeBleed(CTakeDamageInfo* info)
{

	Vector vecDir = vec3_origin;
	if (info->GetAttacker())
	{
		// don't bleed from melee coming from other marines, as they can't hurt us
		if (info->GetAttacker()->Classify() == CLASS_ASW_MARINE)
			return;
		vecDir = info->GetAttacker()->GetAbsOrigin() - GetAbsOrigin();
		VectorNormalize(vecDir);
	}
	else
	{
		vecDir = RandomVector(-1, 1);
	}
	

	//UTIL_ASW_BloodDrips( GetAbsOrigin()+Vector(0,0,60)+vecDir*3, vecDir, BloodColor(), 5 );
	Vector vecInflictorPos = info->GetDamagePosition();
	if ( info->GetInflictor() )
	{
		vecInflictorPos = info->GetInflictor()->GetAbsOrigin();
	}

	CRecipientFilter filter;
	filter.AddAllPlayers();
	UserMessageBegin( filter, "ASWMarineHitByMelee" );
	WRITE_SHORT( entindex() );
	WRITE_FLOAT( vecInflictorPos.x );
	WRITE_FLOAT( vecInflictorPos.y );
	WRITE_FLOAT( vecInflictorPos.z );
	MessageEnd();
}

/// issue any special effects or sounds on resurrection
void CASW_Marine::PerformResurrectionEffect( void ) RESTRICT
{
	DispatchParticleEffect( "marine_resurrection", PATTACH_ABSORIGIN_FOLLOW, this );
	this->EmitSound( "Marine.Resurrect" );
}

void CASW_Marine::BecomeInfested(CASW_Alien* pAlien)
{
	if ( !GetMarineResource() )
		return;

	m_fInfestedTime = 20.0f;
	GetMarineSpeech()->ForceChatter( CHATTER_INFESTED, ASW_CHATTER_TIMER_TEAM );

	if ( !IsInfested() )
	{
		// do some damage to us immediately (unless we were already infested)
		float DamagePerTick = ASWGameRules()->TotalInfestDamage() / 20.0f;
		if ( asw_debug_marine_damage.GetBool() )
		{
			Msg("%f: Infest DamagePerTick %f (infest time left = %f)\n", gpGlobals->curtime, DamagePerTick, m_fInfestedTime);
		}

		CTakeDamageInfo info( NULL, NULL, Vector(0,0,0), GetAbsOrigin(), DamagePerTick, DMG_INFEST );
		TakeDamage( info );

		m_fInfestedStartTime = gpGlobals->curtime;

		// Give them 3 free cycles (.9 seconds) to panic before we do the first real bite!
		m_iInfestCycle = -3;

		GetMarineResource()->SetInfested( true );

		ASWFailAdvice()->OnMarineInfested();

		IGameEvent * event = gameeventmanager->CreateEvent( "marine_infested" );
		if ( event )
		{
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}

		if ( !IsInhabited() )
		{
			DoEmote( 0 );
		}
	}

	if ( m_fNextSlowHealTick < gpGlobals->curtime )
	{
		m_fNextSlowHealTick = gpGlobals->curtime + 0.33f;
	}

	m_bPlayedCureScream = false;
}

void CASW_Marine::CureInfestation(CASW_Marine *pHealer, float fCureFraction)
{
	if ( !GetMarineResource() )
		return;

	if ( m_fInfestedTime != 0 )
	{
		m_fInfestedTime = m_fInfestedTime * fCureFraction;
		if ( pHealer )
			m_hInfestationCurer = pHealer;	

		if ( m_fInfestedTime < 0.0f )
		{
			m_fInfestedTime = 0.0f;

			if ( !IsInhabited() )
			{
				DoEmote( 2 );
			}
		}
	}
}

// if we died from infestation, then gib
bool CASW_Marine::ShouldGib( const CTakeDamageInfo &info )
{
	if (info.GetDamageType() & DMG_INFEST)
		return true;

	return BaseClass::ShouldGib(info);
}

bool CASW_Marine::CorpseGib( const CTakeDamageInfo &info )
{
	EmitSound( "BaseCombatCharacter.CorpseGib" );

	QAngle	vecAngles;
	VectorAngles( -info.GetDamageForce(), vecAngles );
	CBaseEntity *pHelpHelpImBeingSupressed = (CBaseEntity*) te->GetSuppressHost();
	te->SetSuppressHost( NULL );
	DispatchParticleEffect( "marine_gib", PATTACH_ABSORIGIN_FOLLOW, this );
	te->SetSuppressHost( pHelpHelpImBeingSupressed );

	return true;
}

// if we gibbed from infestation damage, spawn some parasites
bool  CASW_Marine::Event_Gibbed( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_INFEST )
	{
		if ( asw_debug_marine_damage.GetBool() )
		{
			Msg("marine infest gibbed at loc %f, %f, %f\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
		}

		if ( asw_debug_marine_damage.GetBool())
		{
			NDebugOverlay::EntityBounds(this, 255,0,0, 255, 15.0f);
		}

		ASWFailAdvice()->OnMarineInfestedGibbed();

		int iNumParasites = 3 + RandomInt(0,2);
		QAngle angParasiteFacing[5];
		float fJumpDistance[5];
		// for some reason if we calculate these inside the loop, the random numbers all come out the same.  Worrying.
		angParasiteFacing[0] = GetAbsAngles(); angParasiteFacing[0].y = RandomFloat( -180.0f, 180.0f );
		angParasiteFacing[1] = GetAbsAngles(); angParasiteFacing[1].y = RandomFloat( -180.0f, 180.0f );
		angParasiteFacing[2] = GetAbsAngles(); angParasiteFacing[2].y = RandomFloat( -180.0f, 180.0f );
		angParasiteFacing[3] = GetAbsAngles(); angParasiteFacing[3].y = RandomFloat( -180.0f, 180.0f );
		angParasiteFacing[4] = GetAbsAngles(); angParasiteFacing[4].y = RandomFloat( -180.0f, 180.0f );
		fJumpDistance[0] = RandomFloat( 30.0f, 70.0f );
		fJumpDistance[1] = RandomFloat( 30.0f, 70.0f );
		fJumpDistance[2] = RandomFloat( 30.0f, 70.0f );
		fJumpDistance[3] = RandomFloat( 30.0f, 70.0f );
		fJumpDistance[4] = RandomFloat( 30.0f, 70.0f );

		for ( int i = 0; i < iNumParasites; i++ )
		{
			bool bBlocked = true;			
			int k = 0;

			Vector vecSpawnPos = GetAbsOrigin();
			float fCircleDegree = ( static_cast< float >( i ) / iNumParasites ) * 2.0f * M_PI;
			vecSpawnPos.x += sinf( fCircleDegree ) * RandomFloat( 3.0f, 20.0f );
			vecSpawnPos.y += cosf( fCircleDegree ) * RandomFloat( 3.0f, 20.0f );
			vecSpawnPos.z += RandomFloat( 20.0f, 40.0f );
			
			while ( bBlocked && k < 6 )
			{
				if ( k > 0 )
				{
					// Scooch it up
					vecSpawnPos.z += NAI_Hull::Maxs( HULL_TINY ).z - NAI_Hull::Mins( HULL_TINY ).z;
				}
						
				// check if there's room at this position
				trace_t tr;
				UTIL_TraceHull( vecSpawnPos, vecSpawnPos + Vector( 0.0f, 0.0f, 1.0f ), 
					NAI_Hull::Mins(HULL_TINY) + Vector( -4.0f, -4.0f, -4.0f ),NAI_Hull::Maxs(HULL_TINY) + Vector( 4.0f, 4.0f, 4.0f ),
					MASK_NPCSOLID, this, ASW_COLLISION_GROUP_PARASITE, &tr );	

				if ( asw_debug_marine_damage.GetBool() )
				{
					NDebugOverlay::Box(vecSpawnPos, NAI_Hull::Mins(HULL_TINY),NAI_Hull::Maxs(HULL_TINY), 255,255,0,255,15.0f);
				}

				if ( tr.fraction == 1.0 )
				{
					bBlocked = false;
				}

				k++;				
			}

			if (bBlocked)
				continue;	// couldn't find room for parasites

			if ( asw_debug_marine_damage.GetBool() )
			{
				Msg("Found an unblocked pos for this entity, trying to spawn it there %f, %f, %f\n", vecSpawnPos.x, 
					vecSpawnPos.y, vecSpawnPos.z);
			}

			CASW_Parasite *pParasite = dynamic_cast< CASW_Parasite* >( CreateNoSpawn( "asw_parasite",
				vecSpawnPos, angParasiteFacing[i], this));

			if ( pParasite )
			{
				PhysDisableEntityCollisions( pParasite, this );
				DispatchSpawn( pParasite );
				pParasite->SetSleepState(AISS_WAITING_FOR_INPUT);
				pParasite->SetJumpFromEgg(true, fJumpDistance[i]);
				pParasite->Wake();
			}
		}
	}

	AddEffects( EF_NODRAW ); // make the model invisible.
	SetSolid( SOLID_NONE );
	SetNextThink( gpGlobals->curtime + 2.0f );
	SetThink( &CASW_Marine::SUB_Remove );
	return CorpseGib( info );
}

float CASW_Marine::GetIdealSpeed() const
{
	return m_fCachedIdealSpeed;	
}

CRagdollProp* CASW_Marine::GetRagdollProp()
{
	return dynamic_cast<CRagdollProp*>(m_hKnockedOutRagdoll.Get());
}

void CASW_Marine::Event_Killed( const CTakeDamageInfo &info )
{
	bool bAllDead = false;

	if ( ASWGameRules() && ASWGameRules()->GetMissionManager() )
	{
		bAllDead = ASWGameRules()->GetMissionManager()->AllMarinesDead();
	}

	CASW_GameStats.Event_MarineKilled( this, info );

	ASWFailAdvice()->OnMarineKilled();

	float flPosition = -1.0f;
	UTIL_ASW_NearestMarine( this, flPosition );

	if ( !bAllDead )
	{
		if ( flPosition != -1.0f && flPosition > 1000.0f )
		{
			ASWFailAdvice()->OnMarineKilledAlone();
		}

		if ( m_hCurrentHack.Get() )
		{
			ASWFailAdvice()->OnHackerDied();
		}

		CASW_Player *pPlayer = GetCommander();
		if ( pPlayer && pPlayer->GetMarine() == this )
		{
			if ( UTIL_ASW_NumCommandedMarines( pPlayer ) >= 1 )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "player_should_switch" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
		}

		if ( IsInfested() )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "marine_infested_killed" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer ? pPlayer->GetUserID() : 0 );
				event->SetInt( "marine", entindex() );
				gameeventmanager->FireEvent( event );
			}
		}
	}

	if ( ASWGameRules() )
	{
		ASWGameRules()->MarineKilled( this, info );

		// Start up the death cam
		ASWGameRules()->m_fMarineDeathTime = gpGlobals->curtime;
		ASWGameRules()->m_vMarineDeathPos = GetAbsOrigin();
		ASWGameRules()->m_nMarineForDeathCam = ASWGameResource()->GetMarineResourceIndex( GetMarineResource() );	// TODO: Is this ok!?

		// Check mission status
		if ( ASWGameRules()->GetMissionManager() )
		{
			ASWGameRules()->GetMissionManager()->MarineKilled( this );
		}
	}

	if ( m_hUsingEntity.Get() )
	{
		StopUsing();
	}

	// store off our death position
	CASW_Marine_Resource *pMR = GetMarineResource();
	if ( pMR )
	{
		pMR->m_vecDeathPosition = GetAbsOrigin();
		pMR->m_fDeathTime = gpGlobals->curtime;

		pMR->SetFiring( 0 );

		pMR->m_TimelineAmmo.RecordValue( 0.0f );
		pMR->m_TimelineHealth.RecordValue( 0.0f );
	}

	// drop all of our equipment
	// Calculate death force
	Vector forceVector = CalcDeathForceVector( info );
	float flMagnitude = forceVector.Length();
	if ( flMagnitude > 20000 )
	{
		forceVector *= 20000 / flMagnitude;
	}

	for ( int i = 0; i < ASW_MAX_MARINE_WEAPONS; i++ )
	{
		CBaseCombatWeapon *pDroppedWeapon = GetWeapon( i );
		if ( pDroppedWeapon )
		{
			// Drop any weapon that I own
			if ( VPhysicsGetObject() )
			{
				Vector weaponForce = forceVector * VPhysicsGetObject()->GetInvMass();
				Weapon_Drop( pDroppedWeapon, NULL, &weaponForce );
			}
			else
			{
				Weapon_Drop( pDroppedWeapon, NULL, &forceVector );
			}
		}
	}

	// see if any other marines are nearby to shout out about us
	if ( ASWGameResource() )
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if (pGameResource)
		{
			if ( asw_realistic_death_chatter.GetBool() )
			{
				int iNumNearby = 0;
				for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
				{
					CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
					CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
					if (pOtherMarine && pOtherMarine != this 
						&& pOtherMarine->GetHealth() > 0
						&& GetAbsOrigin().DistTo(pOtherMarine->GetAbsOrigin()) < 800)
						iNumNearby++;
				}
				if (iNumNearby > 0)
				{
					int iChosen = random->RandomInt(1, iNumNearby);
					for (int i=0;i<pGameResource->GetMaxMarineResources() && iChosen > 0;i++)
					{
						CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
						CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
						if (pOtherMarine && pOtherMarine != this 
							&& pOtherMarine->GetHealth() > 0
							&& GetAbsOrigin().DistTo(pOtherMarine->GetAbsOrigin()) < 800)
						{
							iChosen--;
							if (iChosen <= 0)
							{
								if (asw_debug_marine_chatter.GetBool())
									Msg("making marine CHATTER_MARINE_DOWN %s\n", pOtherMarine->GetMarineProfile()->m_ShortName);
								pOtherMarine->GetMarineSpeech()->QueueChatter(CHATTER_MARINE_DOWN, gpGlobals->curtime+0.5f, gpGlobals->curtime+1.50f);
							}
						}
					}
				}
			}
			else
			{
				// pick one marine for each player to shout about the marine death
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
				{
					CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));
					if ( !pOtherPlayer)
						continue;

					Vector vecPlayerPos = vec3_origin;
					if ( pOtherPlayer->GetMarine() )
					{
						vecPlayerPos = pOtherPlayer->GetMarine()->GetAbsOrigin();
					}

					// tell all other marines to shout about this death
					CASW_Marine *pChosenMarine = NULL;

					// count how many marines this player has
					int iNumMarines = 0;
					for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
					{
						CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
						CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
						if (pOtherMarine && pOtherMarine != this 
							&& pOtherMarine->GetHealth() > 0
							&& ( vecPlayerPos == vec3_origin || pOtherMarine->GetAbsOrigin().DistTo( vecPlayerPos ) < 800 ) )
						{
							iNumMarines++;
						}
					}

					// now choose one to play the sound
					int iChosen = random->RandomInt(1, iNumMarines);
					for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
					{
						CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
						CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
						if (pOtherMarine && pOtherMarine != this 
							&& pOtherMarine->GetHealth() > 0
							&& ( vecPlayerPos == vec3_origin || pOtherMarine->GetAbsOrigin().DistTo( vecPlayerPos ) < 800 ) )
						{
							iChosen--;
							if ( iChosen <= 0 )
							{
								pChosenMarine = pOtherMarine;
								break;
							}						
						}
					}

					if ( pChosenMarine )
					{
						// do private full volume chatter
						pChosenMarine->GetMarineSpeech()->QueueChatter(CHATTER_MARINE_DOWN, gpGlobals->curtime+0.5f, gpGlobals->curtime+1.50f, pOtherPlayer);
					}
				}
			}
		}
	}	
	BaseClass::Event_Killed(info);

	if (asw_debug_marine_chatter.GetBool())
		Msg("making marine CHATTER_DIE %s\n", GetMarineProfile()->m_ShortName);
	GetMarineSpeech()->ForceChatter(CHATTER_DIE, ASW_CHATTER_TIMER_NONE);

	if ( IsInhabited() && GetCommander() )
	{
		// play death beep to the person controlling this marine
		GetCommander()->EmitPrivateSound( "Marine.DeathBeep" );
	}

	// check if this mission has a tech req
	if (ASWGameRules() && ASWGameRules()->m_bMissionRequiresTech)
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if ( pGameResource )
		{
			// count number of live techs
			bool bTech = false;
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				if (pMR && pMR->GetHealthPercent() > 0 && pMR->GetProfile() && pMR->GetProfile()->CanHack())
				{
					bTech = true;
					break;
				}
			}
			if ( !bTech && pGameResource->CountAllAliveMarines() > 0 )
			{
				ASWGameRules()->ScheduleTechFailureRestart( gpGlobals->curtime + 1.5f );
			}
		}
	}
	// print a message if marine was killed by another marine
	if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		CASW_Marine *pOtherMarine = dynamic_cast< CASW_Marine* >( info.GetAttacker() );
		if ( pOtherMarine && GetMarineProfile() && pOtherMarine->GetMarineProfile() )
		{
			CASW_Marine_Resource *pMR = GetMarineResource();
			if ( pMR )
			{
				char szName[ 256 ];
				pMR->GetDisplayName( szName, sizeof( szName ) );

				if ( pOtherMarine == this )
				{
					if ( GetMarineProfile()->m_bFemale )
						UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_suicide_female", szName );
					else
						UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_suicide_male", GetMarineProfile()->m_ShortName );
				}
				else
				{
					CASW_Marine_Resource *pMROther = pOtherMarine->GetMarineResource();
					if ( pMROther )
					{
						char szNameOther[ 256 ];
						pMROther->GetDisplayName( szNameOther, sizeof( szNameOther ) );

						UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_team_killed", szName, szNameOther );
					}
				}
			}
		}
	}

	m_bSlowHeal = false;	// no healing if we're dead!
}

void CASW_Marine::AimGun()
{
	BaseClass::AimGun();

	if (!IsInhabited())
	{
		m_fAIPitch = GetPoseParameter( "aim_pitch" );
	}	
}

void CASW_Marine::DoEmote(int iEmote)
{
	if (GetFlags() & FL_FROZEN || !GetMarineSpeech())	// don't allow this if the marine is frozen
		return;

	switch (iEmote)
	{
		case 0:
		{
			GetMarineSpeech()->Chatter(CHATTER_MEDIC);
			bEmoteMedic = true;
			break;
		}
		case 1:
		{
			GetMarineSpeech()->Chatter(CHATTER_NEED_AMMO);
			bEmoteAmmo = true;
			break;
		}
		case 2:
		{
			bEmoteSmile = true;
			break;
		}
		case 3:
		{
			GetMarineSpeech()->Chatter(CHATTER_HOLD_POSITION);
			DoAnimationEvent(PLAYERANIMEVENT_HALT);
			bEmoteStop = true;
			break;
		}
		case 4:
		{
			GetMarineSpeech()->Chatter(CHATTER_FOLLOW_ME);
			DoAnimationEvent(PLAYERANIMEVENT_GO);
			bEmoteGo = true;
			break;
		}
		case 5:
		{
			GetMarineSpeech()->Chatter(CHATTER_WATCH_OUT);
			bEmoteExclaim = true;
			break;
		}
		case 7:
		{
			GetMarineSpeech()->Chatter(CHATTER_QUESTION);
			bEmoteQuestion = true;
			break;
		}
		default:
		{
			bEmoteAnimeSmile = true;
			break;
		}
	}
}

bool CASW_Marine::IsPlayerAlly( CBasePlayer *pPlayer )
{
	return true;
}

IASW_Vehicle* CASW_Marine::GetASWVehicle()
{
	//IASW_Vehicle* pEnt = m_hASWVehicle.Get();
	//return dynamic_cast<IASW_Vehicle*>(pEnt);
	return dynamic_cast<IASW_Vehicle*>(m_hASWVehicle.Get());
}

// make the marine start driving a particular vehicle
void CASW_Marine::StartDriving(IASW_Vehicle* pVehicle)
{
	if (!pVehicle || IsDriving() || IsInVehicle() || pVehicle->ASWGetDriver()!=NULL)
		return;

	CBaseEntity* pEnt = pVehicle->GetEntity();
	if (!pEnt)
		return;

	//Must be able to stow our weapon
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();	
	if ( ( pWeapon != NULL ) && ( pWeapon->Holster( NULL ) == false ) )
		return;

	pVehicle->ASWSetDriver(this);
	pVehicle->ASWStartEngine();

	m_bDriving = true;
	m_bIsInVehicle = true;
	m_hASWVehicle = pEnt;

	AddEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_IN_VEHICLE );
	m_takedamage = DAMAGE_NO;
}

void CASW_Marine::StopDriving(IASW_Vehicle* pVehicle)
{
	if (!pVehicle || !IsDriving())
		return;

	CBaseEntity* pEnt = pVehicle->GetEntity();
	if (!pEnt)
		return;

	// try and place the marine outside the vehicle
	Vector v = pEnt->GetAbsOrigin() - UTIL_YawToVector(pEnt->GetAbsAngles().y) * 50;
	trace_t tr;
	Ray_t ray;
	ray.Init( v + Vector(0,0,1), v, CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs() );
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	if ( tr.fraction < 1.0 )
		return;	// blocked
	SetAbsOrigin(v);

	// todo: get weapon out again?

	pVehicle->ASWStopEngine();
	pVehicle->ASWSetDriver(NULL);

	m_bDriving = false;
	m_bIsInVehicle = false;
	m_hASWVehicle = NULL;

	

	RemoveEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );
	m_takedamage = DAMAGE_YES;
}

int	CASW_Marine::UpdateTransmitState()
{
	// always call ShouldTransmit() for maines
	//return SetTransmitState( FL_EDICT_FULLCHECK );
	return SetTransmitState( FL_EDICT_ALWAYS );
}

int CASW_Marine::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// asw temp
	return FL_EDICT_ALWAYS;

	// always transmit if we're inhabited by the target client
	if ( GetCommander() && IsInhabited() && pInfo->m_pClientEnt == GetCommander()->edict() )
	{
		return FL_EDICT_ALWAYS;
	}	

	return BaseClass::ShouldTransmit( pInfo );
}

void CASW_Marine::PhysicsShove()
{
	Vector forward, up, right;
	AngleVectors( EyeAngles(), &forward, &right, &up );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = WorldSpaceCenter();

	UTIL_TraceLine( searchCenter, searchCenter + forward * 96.0f, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if (!tr.m_pEnt)
	{
		UTIL_TraceLine( GetAbsOrigin() + Vector(0,0,25), searchCenter + forward * 96.0f, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	}
	//UTIL_AddDebugLine(searchCenter, searchCenter + forward * 96.0f, false, false);

	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *entity = tr.m_pEnt;

	if (entity && entity->VPhysicsGetObject() )
	{
		IPhysicsObject *pObj = entity->VPhysicsGetObject();

		Vector vPushAway = (entity->WorldSpaceCenter() - WorldSpaceCenter());
		vPushAway.z = 0;

		float flDist = VectorNormalize( vPushAway );
		flDist = MAX( flDist, 1 );

		float flForce = MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_FORCE);
		//flForce /= flDist;
		flForce = MIN( flForce, asw_marine_melee_max_force.GetFloat() );
		if (asw_debug_marine_damage.GetBool())
		{
			Msg(" Kicking with force %f\n", flForce);
		}

		pObj->ApplyForceOffset( vPushAway * flForce, WorldSpaceCenter() );
	}
}

CASW_Marine_Resource* CASW_Marine::GetMarineResource() const
{
	return dynamic_cast<CASW_Marine_Resource*>(m_MarineResource.Get());
}

void CASW_Marine::Suicide()
{
	if (GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return;
	m_iHealth = 1;
	if (IsInfested())
	{
		CTakeDamageInfo info(this, this, Vector(0,0,0), GetAbsOrigin(), 100,
					DMG_INFEST);
		TakeDamage(info);
	}
	else
	{
		CTakeDamageInfo info(this, this, Vector(0,0,0), GetAbsOrigin(), 100,
					DMG_NEVERGIB);
		TakeDamage(info);
	}

	SetThink(&CBaseEntity::SUB_Remove);
	SetNextThink(gpGlobals->curtime + 2.0f);
}

bool CASW_Marine::BecomeRagdollOnClient( const Vector &force )
{
	if ( !CanBecomeRagdoll() ) 
		return false;

	// Become server-side ragdoll if we're flagged to do it
	//if ( m_spawnflags & SF_ANTLIONGUARD_SERVERSIDE_RAGDOLL )
	if (asw_marine_server_ragdoll.GetBool())
	{
		CTakeDamageInfo	info;

		// Fake the info
		info.SetDamageType( DMG_GENERIC );
		info.SetDamageForce( force );
		info.SetDamagePosition( WorldSpaceCenter() );		
		IPhysicsObject *pPhysics = VPhysicsGetObject();
		if ( pPhysics )
		{
			VPhysicsDestroyObject();
		}
		CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
		FixupBurningServerRagdoll( pRagdoll );
		PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );

		//CBaseEntity *pRagdoll = CreateServerRagdoll( this, 0, info, COLLISION_GROUP_NONE );

		// Transfer our name to the new ragdoll
		pRagdoll->SetName( GetEntityName() );
		//pRagdoll->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		
		// Get rid of our old body
		//UTIL_Remove(this);
		RemoveDeferred();

		return true;
	}

	return BaseClass::BecomeRagdollOnClient( force );
}

void CASW_Marine::SelectModelFromProfile()
{
	CASW_Marine_Profile *pProfile = GetMarineProfile();
	if (pProfile)
	{
		SetModelName(MAKE_STRING(pProfile->m_ModelName));
		m_nSkin = pProfile->m_SkinNum;
		//Msg("%s Setting skin number to %d\n", pProfile->m_ShortName, m_nSkin);
	}
	else
	{
		SetModelName( AllocPooledString( ASW_DEFAULT_MARINE_MODEL ) );
		//Msg("Warning (SelectModelFromProfile) couldn't get model from profile as profile doesn't exist yet\n");
	}
}

void CASW_Marine::SetModelFromProfile()
{
	CASW_Marine_Profile *pProfile = GetMarineProfile();
	if (pProfile)
	{
		SetModelName(MAKE_STRING(pProfile->m_ModelName));
		SetModel(pProfile->m_ModelName);
		m_nSkin = pProfile->m_SkinNum;
		//set the backpack bodygroup
		SetBodygroup ( 1, m_nSkin );
		
		//Msg("%s Setting skin number to %d\n", pProfile->m_ShortName, m_nSkin);
	}
	else
	{
		SetModelName( AllocPooledString( ASW_DEFAULT_MARINE_MODEL ) );
		SetModel(ASW_DEFAULT_MARINE_MODEL);
		Msg("Warning (SetModelFromProfile) couldn't get model from profile as profile doesn't exist yet\n");
	}
}

void CASW_Marine::SetKnockedOut(bool bKnockedOut)
{
	if (m_bKnockedOut == bKnockedOut)
		return;

	m_bKnockedOut = bKnockedOut;
	if (m_bKnockedOut)		// make the marine fall over
	{
		FlashlightTurnOff();
		InvalidateBoneCache();
		AddSolidFlags( FSOLID_NOT_SOLID );			
		CTakeDamageInfo	info;
		info.SetDamageType( DMG_GENERIC );
		info.SetDamageForce( vec3_origin );
		info.SetDamagePosition( WorldSpaceCenter() );
		m_hKnockedOutRagdoll = (CRagdollProp*) CreateServerRagdoll( this, 0, info, COLLISION_GROUP_NONE );
		if ( GetRagdollProp() )
		{
			GetRagdollProp()->DisableAutoFade();
			GetRagdollProp()->SetThink( NULL );
			GetRagdollProp()->SetUnragdoll( this );
		}			
		AddEffects( EF_NODRAW );
		AddFlag( FL_FROZEN );		

		Msg("%s has been knocked unconcious!\n", GetMarineProfile() ? GetMarineProfile()->m_ShortName : "UnknownMarine");
	}
	else		// marine is already knocked out, let's make him get up again
	{
		Assert(IsEffectActive(EF_NODRAW));
		Assert(GetRagdollProp());

		SetStopTime(gpGlobals->curtime + 2.0f);	// make sure he can't move for a while
		DoAnimationEvent(PLAYERANIMEVENT_GETUP);	// animate him standing up

		//Calcs the diff between ragdoll worldspace center and victim worldspace center, moves the victim by this diff.
		//Sets the victim's angles to 0, ragdoll yaw, 0
		QAngle newAngles( 0, GetRagdollProp()->GetAbsAngles()[YAW], 0 );

		Vector centerDelta = GetRagdollProp()->WorldSpaceCenter() - WorldSpaceCenter();
		centerDelta.z = 0;	// don't put us in the floor
		Vector newOrigin = GetAbsOrigin() + centerDelta;
		SetAbsOrigin( newOrigin );
		SetAbsAngles( newAngles );		
		//GetRagdollProp()->AddEffects( EF_NODRAW );
		RemoveEffects( EF_NODRAW );
		RemoveSolidFlags( FSOLID_NOT_SOLID );		
		if (HasFlashlight())
			FlashlightTurnOn();
		m_fUnfreezeTime = gpGlobals->curtime + 3.0f;
		UTIL_Remove( GetRagdollProp() );
		m_hKnockedOutRagdoll = NULL;

		Msg("%s has got back up.\n", GetMarineProfile() ? GetMarineProfile()->m_ShortName : "UnknownMarine");
	}
}
/*
void CASW_Marine::DoKickEffect()
{
	//Msg("CASW_Marine::DoKickEffect\n");
	bool bHasBayonet = false;
		// bayonet disabled at this time
		//GetActiveASWWeapon() && GetActiveASWWeapon()->SupportsBayonet() &&
			//(MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_EDGED) > 0);
	//CBaseEntity *pHurt = 	
	float flForce = MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_FORCE);
	// add a bit of randomness
//	flForce *= random->RandomFloat(0.8f, 1.2f);	
	int iDamage = 1;
	if (bHasBayonet)
	{
		// bayonet disabled at this time
		//int iDamage = MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_EDGED);
	}
	else
	{
		iDamage = MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_DMG);		
	}	

	const CBaseEntity * ent = NULL;
	if ( g_pGameRules->IsMultiplayer() )
	{
		// temp remove suppress host
		ent = te->GetSuppressHost();
		te->SetSuppressHost( NULL );
	}
	
	CheckTraceHullAttack( asw_marine_melee_distance.GetFloat(), -Vector(16,16,32), Vector(16,16,32), iDamage, DMG_CLUB, flForce, true );

	if ( g_pGameRules->IsMultiplayer() )
	{
		te->SetSuppressHost( (CBaseEntity *) ent );
	}
}

CBaseEntity *CASW_Marine::CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float forceScale, bool bDamageAnyNPC )
{
	// If only a length is given assume we want to trace in our facing direction
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vStart = GetAbsOrigin();

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );

	// asw - make melee attacks trace below us too, so it's possible hard to hit things just below you on a slope
	Vector low_mins = mins;
	low_mins.z -= 30;
	return CheckTraceHullAttack( vStart, vEnd, low_mins, maxs, iDamage, iDmgType, forceScale, bDamageAnyNPC );
}

// asw note: same as CBaseCombatCharacter version, but we use our custom melee trace filter so the victim can bleed and we can kick our own grenades
CBaseEntity *CASW_Marine::CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float flForceScale, bool bDamageAnyNPC )
{
	// Handy debuging tool to visualize HullAttack trace
	if ( ai_show_hull_attacks.GetBool() )
	{
		float length	 = (vEnd - vStart ).Length();
		Vector direction = (vEnd - vStart );
		VectorNormalize( direction );
		Vector hullMaxs = maxs;
		hullMaxs.x = length + hullMaxs.x;
		NDebugOverlay::BoxDirection(vStart, mins, hullMaxs, direction, 100,255,255,20,1.0);
		NDebugOverlay::BoxDirection(vStart, mins, maxs, direction, 255,0,0,20,1.0);
	}

	CTakeDamageInfo	dmgInfo( this, this, iDamage, iDmgType );
	
	CASW_Trace_Filter_Melee traceFilter( this, COLLISION_GROUP_NONE, &dmgInfo, flForceScale, bDamageAnyNPC );

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

	CBaseEntity *pEntity = traceFilter.m_pHit;

	// do an impact effect for kicking some things
	if (traceFilter.m_hBestHit.Get())
	{
		trace_t tr;
		UTIL_TraceLine(WorldSpaceCenter(), traceFilter.m_hBestHit->WorldSpaceCenter(),	// check center to center
				MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
		if (tr.DidHit())
		{
			CASW_Door *pDoor = dynamic_cast<CASW_Door*>(tr.m_pEnt);		// doors make their own bash sounds, so skip an impact trace vs them
			if (!pDoor && !traceFilter.m_hBestHit->IsNPC())	
				UTIL_ImpactTrace( &tr, iDmgType );
		}
	}
	else	// didn't hit anything, just do a general trace
	{
		Vector forward, right, up, v;
		v = GetAbsOrigin();
		QAngle ang = GetAbsAngles();
		AngleVectors( ang, &forward, &right, &up );
		v = v + up * 45;
		Vector vecKickSrc = v
						- forward * 1
						+ right * 1;
		trace_t tr;
		UTIL_TraceLine(vecKickSrc, vecKickSrc + forward * 50,
			MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
		if (tr.DidHit())
		{
			CASW_Door *pDoor = dynamic_cast<CASW_Door*>(tr.m_pEnt);		// doors make their own bash sounds, so skip an impact trace vs them
			if (!pDoor && !tr.m_pEnt->IsNPC())	
				UTIL_ImpactTrace( &tr, iDmgType );
		}
	}

	return pEntity;
}*/

// marines always move efficiently`
void CASW_Marine::UpdateEfficiency( bool bInPVS )
{
	// Sleeping NPCs always dormant
	if ( GetSleepState() != AISS_AWAKE )
	{
		SetEfficiency( AIE_DORMANT );
		return;
	}

	SetEfficiency( AIE_NORMAL );
	SetMoveEfficiency( AIME_NORMAL );
}

float CASW_Marine::GetIdealAccel( ) const
{
	return GetIdealSpeed() * asw_marine_ai_acceleration.GetFloat();
}

float CASW_Marine::MaxYawSpeed( void )
{ 
	if ( GetEnemy() )
		return 45.0f;

	if ( m_vecFacingPointFromServer.Get() != vec3_origin || m_hUsingEntity.Get() )
		return 24.0f;

	return 8.0f;
}


#define GROUNDTURRET_VIEWCONE 60.0f
#define GROUNDTURRET_BEAM_SPRITE "materials/effects/bluelaser2.vmt"
void CASW_Marine::Scan()
{
	if (IsInhabited() || GetASWOrders() != ASW_ORDER_HOLD_POSITION || !asw_marine_scan_beams.GetBool())
		return;

	if( gpGlobals->curtime >= m_flTimeNextScanPing )
	{
		m_flTimeNextScanPing = gpGlobals->curtime + 1.0f;
	}

	QAngle	scanAngle;
	Vector	forward;
	Vector	vecEye = GetAbsOrigin();// + m_vecLightOffset;

	// Draw the outer extents
	scanAngle = GetAbsAngles();
	scanAngle.y += (GROUNDTURRET_VIEWCONE / 2.0f);
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.1 );

	scanAngle = GetAbsAngles();
	scanAngle.y -= (GROUNDTURRET_VIEWCONE / 2.0f);
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.1 );

	// Draw a sweeping beam
	scanAngle = GetAbsAngles();
	scanAngle.y += (GROUNDTURRET_VIEWCONE / 2.0f) * sin( gpGlobals->curtime * 3.0f );
	
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.3 );
}

void CASW_Marine::ProjectBeam( const Vector &vecStart, const Vector &vecDir, int width, int brightness, float duration )
{
	CBeam *pBeam;
	pBeam = CBeam::BeamCreate( GROUNDTURRET_BEAM_SPRITE, width );
	if ( !pBeam )
		return;

	trace_t tr;
	AI_TraceLine( vecStart, vecStart + vecDir * 768.0f, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	
	pBeam->SetStartPos( tr.endpos );
	pBeam->SetEndPos( tr.startpos );
	pBeam->SetWidth( width );
	pBeam->SetEndWidth( 0.1 );
	pBeam->SetFadeLength( 16 );

	pBeam->SetBrightness( brightness );
	pBeam->SetColor( 0, 145+random->RandomInt( -16, 16 ), 255 );
	pBeam->RelinkBeam();
	pBeam->LiveForTime( duration );
}

void CASW_Marine::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon /*= NULL */ )
{
	if (!ASWGameRules())
		return;
	// set flame life time by the game difficulty
	int iDiff = ASWGameRules()->GetSkillLevel();
	if (iDiff == 1)
		flFlameLifetime *= asw_marine_burn_time_easy.GetFloat();
	else if (iDiff == 2)
		flFlameLifetime *= asw_marine_burn_time_normal.GetFloat();
	else if (iDiff == 3)
		flFlameLifetime *= asw_marine_burn_time_hard.GetFloat();
	else if (iDiff == 4 || iDiff == 5)
		flFlameLifetime *= asw_marine_burn_time_insane.GetFloat();	

	if ( m_flFirstBurnTime == 0 )
		m_flFirstBurnTime = gpGlobals->curtime;

	// if this is an env_fire trying to burn us, ignore the grace period that the AllowedToIgnite function does
	// we want env_fires to always ignite the marine immediately so they can be used as dangerous blockers in levels
	CFire *pFire = dynamic_cast<CFire*>(pAttacker);
	if ( AllowedToIgnite() || pFire )
	{
		if( IsOnFire() )
			return;

		// scream about being on fire
		GetMarineSpeech()->PersonalChatter(CHATTER_ON_FIRE);

		AddFlag( FL_ONFIRE );
		m_bOnFire = true;
		if ( ASWBurning() )
		{
			ASWBurning()->BurnEntity(this, pAttacker, flFlameLifetime, 0.4f, 10.0f * 0.4f, pDamagingWeapon );	// 10 dps, applied every 0.4 seconds
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "marine_ignited" );
		if ( event )
		{
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}

		m_OnIgnite.FireOutput( this, this );
	}

	m_flLastBurnTime = gpGlobals->curtime;
}

void CASW_Marine::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	return;	// use ASW_Ignite instead;
}

void CASW_Marine::Extinguish()
{
	if ( m_bOnFire )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "marine_extinguished" );
		if ( event )
		{
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}
	}

	m_bOnFire = false;

	if ( ASWBurning() )
	{
		ASWBurning()->Extinguish(this);
	}

	RemoveFlag( FL_ONFIRE );
}

bool CASW_Marine::AllowedToIgnite( void ) 
{ 
	if ( m_iJumpJetting.Get() != 0 )
		return false;

	float flBurnTime = ( asw_marine_ff_absorption.GetInt() > 0 ) ? asw_marine_time_until_ignite.GetFloat() : 0.2f;
	if ( m_flFirstBurnTime > 0 && (gpGlobals->curtime - m_flFirstBurnTime) >= flBurnTime )
		return true;

	// don't ignite, but play a flesh burn sound if we aren't on fire already
	if ( !m_bOnFire && (gpGlobals->curtime - m_flLastBurnSoundTime) > 1.0f )
	{
		CASW_Player *player = GetCommander();
		if ( player )
		{
			CSingleUserRecipientFilter localfilter( player );
			localfilter.MakeReliable();

			CBaseEntity::EmitSound( localfilter, entindex(), "ASW.MarineBurnPain_NoIgnite" );
			m_flLastBurnSoundTime = gpGlobals->curtime;
		}
	}

	return false;
}

int	CASW_Marine::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "Using: %d (%s)\n",
			m_hUsingEntity.Get(), m_hUsingEntity.Get() ? m_hUsingEntity->GetClassname() : " ");
		NDebugOverlay::EntityText(entindex(),text_offset,buffer,0);		
		text_offset++;

		if (GetASWOrders() == ASW_ORDER_HOLD_POSITION)
			Q_snprintf(buffer, sizeof(buffer), "ASWOrders: ASW_ORDER_HOLD_POSITION\n");
		else if (GetASWOrders() == ASW_ORDER_FOLLOW)
			Q_snprintf(buffer, sizeof(buffer), "ASWOrders: ASW_ORDER_FOLLOW\n");
		else if (GetASWOrders() == ASW_ORDER_MOVE_TO)
			Q_snprintf(buffer, sizeof(buffer), "ASWOrders: ASW_ORDER_MOVE_TO\n");
		else if (GetASWOrders() == ASW_ORDER_USE_OFFHAND_ITEM)
			Q_snprintf(buffer, sizeof(buffer), "ASWOrders: ASW_ORDER_USE_OFFHAND_ITEM\n");
		else
			Q_snprintf(buffer, sizeof(buffer), "ASWOrders: Unknown\n");
		NDebugOverlay::EntityText(entindex(),text_offset,buffer,0);
		text_offset++;

		Q_snprintf(buffer, sizeof(buffer), "FF scale: %f\n",m_fFriendlyFireAbsorptionTime);
		NDebugOverlay::EntityText(entindex(),text_offset,buffer,0);	
		text_offset++;		
	}
	return text_offset;
}

float CASW_Marine::GetReceivedDamageScale( CBaseEntity *pAttacker )
{
	float flScale = 1;

	// if we've been shot by another marine...
	if (pAttacker && pAttacker->Classify() == CLASS_ASW_MARINE)
	{
		CASW_Marine *pMarine = CASW_Marine::AsMarine( pAttacker );
		if (pMarine)
		{
			if (ASWGameRules() && ASWGameRules()->IsHardcoreMode())
			{
				// full damage in hardcore mode
				flScale = 1;
			}
			else if (asw_marine_ff.GetInt() == 0)		// FF Guard
			{
				flScale = 0.01f;
				pMarine->ActivateFriendlyFireGuard(this);
			}
			else if (asw_marine_ff.GetInt() == 1)	// normal
			{
				// allow friendly fire through based on difficulty level
				int diff = ASWGameRules()->GetMissionDifficulty() - 5;
				flScale = (asw_marine_ff_dmg_base.GetFloat() + asw_marine_ff_dmg_step.GetFloat() * diff);
			}
			else		// full
			{
				// allow max friendly fire damage through, as though on mission difficulty +5
				flScale = (asw_marine_ff_dmg_base.GetFloat() + asw_marine_ff_dmg_step.GetFloat() * 5.0f);
			}
		}
	}
	
	return flScale * BaseClass::GetReceivedDamageScale(pAttacker);
}

void CASW_Marine::ActivateFriendlyFireGuard(CASW_Marine *pVictim)
{
	// stops the marine from being able to fire
	//	todo: make wepaons check this time isn't 0, to prevent firing
	m_fFFGuardTime = gpGlobals->curtime + asw_marine_ff_guard_time.GetFloat();
	// todo: play a sound warning the player of FF
	
}

int CASW_Marine::GetAlienMeleeFlinch()
{
	return MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_FLINCH);
}

// POWERUPS!
void CASW_Marine::AddPowerup( int iType, float flExpireTime ) 
{ 
	RemoveAllPowerups();

	// if a powerup doesn't expire, we tell the current weapon that it has the powerup
	// if we want powerups that don't expire and aren't tied to a weapon's clip, we'll need to rethink this
	if ( flExpireTime > gpGlobals->curtime )
	{
		m_bPowerupExpires = true;
		m_flPowerupExpireTime = flExpireTime;
	}
	else
	{
		CASW_Weapon* pWeapon = GetActiveASWWeapon();
		if ( !pWeapon )
			return;

		pWeapon->MakePoweredUp( true );
	}

	m_iPowerupType = iType; 
}

bool CASW_Marine::HasPowerup( int iType ) 
{ 
	if ( m_iPowerupType == iType )
		return true;

	return false;
}

void CASW_Marine::RemoveWeaponPowerup( CASW_Weapon* pWeapon )
{
	if ( !pWeapon )
		return;

	if ( pWeapon->m_bPoweredUp )
	{
		m_bPowerupExpires = false;
		pWeapon->MakePoweredUp( false );
		m_iPowerupType = -1;
	}
}

void CASW_Marine::RemoveAllPowerups( void )
{
	m_bPowerupExpires = false;
	m_iPowerupType = -1;
	m_flPowerupExpireTime = -1;
	for ( int i = 0; i < 3; i++ )
	{
		CASW_Weapon* pWeapon = GetASWWeapon(i);
		if ( !pWeapon )
			continue;

		pWeapon->MakePoweredUp( false );
	}
}

void CASW_Marine::UpdatePowerupDuration( void )
{
	if ( m_iPowerupType >= 0 )
	{
		if ( m_bPowerupExpires && m_flPowerupExpireTime <= gpGlobals->curtime )
		{
			RemoveAllPowerups();
		}
	}
}


// test: always avoid..
void CASW_Marine::SetPlayerAvoidState()
{
	m_bPlayerAvoidState = ShouldPlayerAvoid();
	m_bPerformAvoidance = true;
}

void CASW_Marine::CheckAndRequestAmmo()
{
	m_fLastAmmoCheckTime = gpGlobals->curtime;

	bool bAllWeaponsOutOfAmmo = true;
	CASW_Marine *pClosestAmmoBagSquadmate = NULL;
	CASW_Marine *pActiveWpnAmmoBagSquadmate = NULL;	// send high-priority message to player that can resupply active
	for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
	{
		CASW_Weapon *pWeapon = GetASWWeapon(iWeapon);

		if ( !pWeapon || !pWeapon->IsOffensiveWeapon() )
			continue;

		bool bWeaponHasAmmo = ( pWeapon->Clip1() > 0 || GetAmmoCount ( pWeapon->GetPrimaryAmmoType() ) > 0 );
		bool bWeaponLowOnAmmo = ( pWeapon->Clip1() == 0 && GetAmmoCount ( pWeapon->GetPrimaryAmmoType() ) <= 1) ||
								( GetAmmoCount ( pWeapon->GetPrimaryAmmoType() ) == 0 );
		bool bActiveWeapon = ( GetActiveASWWeapon() == pWeapon );

		if ( bWeaponHasAmmo )
		{
			bAllWeaponsOutOfAmmo = false;
		}

		// if we have some ammo, only request it if marine is player controlled, the weapon is active, and we're low on ammo
		if ( bWeaponHasAmmo && ( !IsInhabited() || !bActiveWeapon || !bWeaponLowOnAmmo ) )
		{
			continue;
		}

		float fClosestAmmoBagDistSqr = FLT_MAX;

		// find the closest marine who can resupply this weapon
		CASW_Game_Resource *pGameResource = ASWGameResource();
		for ( int i=0; i<pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource *pMarineResource = pGameResource->GetMarineResource(i);
			if ( !pMarineResource )
				continue;

			CASW_Marine *pSquadmate = pMarineResource->GetMarineEntity();
			if ( !pSquadmate || ( pSquadmate == this ) )
				continue;

			if ( pSquadmate->CanGiveAmmoTo( this ) )
			{
				float fAmmoBagDistSqr = pSquadmate->GetAbsOrigin().DistToSqr( GetAbsOrigin() );
				if ( fAmmoBagDistSqr < fClosestAmmoBagDistSqr )
				{
					fClosestAmmoBagDistSqr = fAmmoBagDistSqr;
					pClosestAmmoBagSquadmate = pSquadmate;	// need to save for later use
				}
			}
		}

		if ( pClosestAmmoBagSquadmate )
		{
			// if player controlled, we NEED ammo rather than simply wanting it
			pClosestAmmoBagSquadmate->SetCondition( IsInhabited() ? COND_SQUADMATE_NEEDS_AMMO : COND_SQUADMATE_WANTS_AMMO );

			if ( bActiveWeapon )
				pActiveWpnAmmoBagSquadmate = pClosestAmmoBagSquadmate;
		}
	}

	if ( pActiveWpnAmmoBagSquadmate )
	{
		// if anyone can resupply our active weapon, they get the higher priority NEED condition
		pActiveWpnAmmoBagSquadmate->SetCondition( COND_SQUADMATE_NEEDS_AMMO );
	}
	else if ( pClosestAmmoBagSquadmate && bAllWeaponsOutOfAmmo )
	{
		// otherwise, if we're all out of ammo, send a higher priority NEED request
		pClosestAmmoBagSquadmate->SetCondition( COND_SQUADMATE_NEEDS_AMMO );
	}
}

bool CASW_Marine::IsOutOfAmmo()
{
	for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
	{
		CASW_Weapon *pWeapon = GetASWWeapon(iWeapon);
		if (pWeapon && pWeapon->IsOffensiveWeapon() && (pWeapon->Clip1() > 0 || GetAmmoCount(pWeapon->GetPrimaryAmmoType()) > 0))
		{
			return false;
		}
	}

	return true;
}

void CASW_Marine::OnWeaponOutOfAmmo(bool bChatter)
{
	if (bChatter && GetMarineSpeech())
	{
		GetMarineSpeech()->Chatter(CHATTER_NO_AMMO);
		bEmoteAmmo = true;

		CASW_Marine_Resource *pMR = GetMarineResource();
		if ( pMR )
		{
			char szName[ 256 ];
			pMR->GetDisplayName( szName, sizeof( szName ) );
			UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_out_of_ammo", szName );
		}
	}

	CheckAndRequestAmmo();

	// check to see if completely out of ammo on all weapons
	if ( !IsOutOfAmmo() )
		return;

	IGameEvent * event = gameeventmanager->CreateEvent( "marine_no_ammo" );
	if ( event )
	{
		CASW_Player *pPlayer = GetCommander();
		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
		event->SetInt( "entindex", entindex() );
		event->SetInt( "count", ( pPlayer ? UTIL_ASW_NumCommandedMarines( pPlayer ) : 0 ) );

		gameeventmanager->FireEvent( event );
	}

	ASWFailAdvice()->OnMarineOutOfAmmo();

	// if marine has no ammo in any offensive weapon, log the position for stats
	if ( GetMarineResource() )
	{
		GetMarineResource()->m_vecOutOfAmmoSpot = GetAbsOrigin();
	}
}

void CASW_Marine::PhysicsLandedOnGround( float fFallSpeed )
{
	float fFallVel = fabs(fFallSpeed) * 1.17f;			// add 17% onto the fall speed - this makes the fall speeds of AI roughly match up with the ones done by player movement

	if ( GetGroundEntity() != NULL && GetHealth() > 0 && fFallVel >= PLAYER_FALL_PUNCH_THRESHOLD )
	{			
		bool bAlive = true;
		float fvol = 0.5;

		if ( GetWaterLevel() > 0 )
		{
			// They landed in water.
		}
		else
		{
			// Scale it down if we landed on something that's floating...
			if ( GetGroundEntity()->IsFloating() )
			{
				fFallVel -= PLAYER_LAND_ON_FLOATING_OBJECT;
			}

			// They hit the ground.
			if ( fFallVel > PLAYER_MAX_SAFE_FALL_SPEED )
			{

				// If they hit the ground going this fast they may take damage (and die).
				//bAlive = MoveHelper( )->PlayerFallingDamage();
#ifndef CLIENT_DLL
				float fFallVelMod = fFallVel;
				fFallVelMod -= PLAYER_MAX_SAFE_FALL_SPEED;
				float flFallDamage = fFallVelMod * DAMAGE_FOR_FALL_SPEED;
				//Msg("Marine fell with speed %f modded to %f damage is %f\n", fFallVel, fFallVelMod, flFallDamage);
				if ( flFallDamage > 0 )
				{
					TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), flFallDamage, DMG_FALL ) ); 
					CRecipientFilter filter;
					filter.AddRecipientsByPAS( GetAbsOrigin() );

					CBaseEntity::EmitSound( filter, entindex(), "Player.FallDamage" );
				}
				bAlive = GetHealth() > 0;
#endif
				fvol = 1.0;
			}
			else if ( fFallVel > PLAYER_MAX_SAFE_FALL_SPEED / 2 )
			{
				fvol = 0.85;
			}
			else if ( fFallVel < PLAYER_MIN_BOUNCE_SPEED )
			{
				fvol = 0;
			}				
		}

		if ( fvol > 0.0 )
		{
			// asw todo?
			// Play landing sound right away.
			//player->m_flStepSoundTime = 400;

			// Play step sound for current texture.
			//PlayStepSound( mv->m_vecAbsOrigin, m_pSurfaceData, fvol, true );
		}			
	}
}

float CASW_Marine::GetFFAbsorptionScale()
{
	float fScale = 1.0f;
	if ( asw_marine_ff_absorption.GetInt() == 1 )			// ramp damage up over time
	{
		fScale = m_fFriendlyFireAbsorptionTime * m_fFriendlyFireAbsorptionTime;
	}
	else if ( asw_marine_ff_absorption.GetInt() == 2 )		// ramp damage down over time
	{
		fScale = 1.0f - ( m_fFriendlyFireAbsorptionTime * m_fFriendlyFireAbsorptionTime );
	}

	fScale = 0.05f + 0.95f * fScale;		// always do a minimum % damage

	return fScale;
}
void CASW_Marine::Stumble( CBaseEntity *pSource, const Vector &vecStumbleDir, bool bShort )
{
	if ( !pSource || GetForcedActionRequest() != 0 )
		return;

	if ( pSource->Classify() == CLASS_ASW_SHIELDBUG )		// don't stumble from shieldbugs, they do knockdowns instead
		return;

	if ( pSource->Classify() == CLASS_ASW_MARINE )			// don't stumble from friendly fire
		return;

	if ( gpGlobals->curtime < m_flNextStumbleTime )
		return;

	//vecStumbleDir.z = 0;
	//vecStumbleDir.NormalizeInPlace();
	
	QAngle staggerAngles;
	VectorAngles( vecStumbleDir, staggerAngles );
	float yawDelta = AngleNormalize( GetAbsAngles()[YAW] - staggerAngles[YAW] );

	if ( yawDelta <= 45 && yawDelta >= -45 )
		m_iForcedActionRequest = bShort ? FORCED_ACTION_STUMBLE_SHORT_FORWARD : FORCED_ACTION_STUMBLE_FORWARD;
	else if ( yawDelta > 45 && yawDelta < 135 )
		m_iForcedActionRequest = bShort ? FORCED_ACTION_STUMBLE_SHORT_RIGHT : FORCED_ACTION_STUMBLE_RIGHT;
	else if ( yawDelta < -45 && yawDelta > -135 )
		m_iForcedActionRequest = bShort ? FORCED_ACTION_STUMBLE_SHORT_LEFT : FORCED_ACTION_STUMBLE_LEFT;
	else
		m_iForcedActionRequest = bShort ? FORCED_ACTION_STUMBLE_SHORT_BACKWARD : FORCED_ACTION_STUMBLE_BACKWARD;

	SetNextStumbleTime( gpGlobals->curtime + asw_stumble_interval.GetFloat() );
}

void CASW_Marine::Knockdown( CBaseEntity *pSource, const Vector &vecImpulse, bool bForce )
{
	if ( !pSource )
		return;

	// already knocked down
	if ( GetForcedActionRequest() >= FORCED_ACTION_KNOCKDOWN_FORWARD && GetForcedActionRequest() <= FORCED_ACTION_KNOCKDOWN_BACKWARD )
		return;

	if ( gpGlobals->curtime < m_flNextStumbleTime && !bForce )
		return;

	Vector vecKnockdownDir = vecImpulse.Normalized();
	QAngle staggerAngles;
	VectorAngles( vecKnockdownDir, staggerAngles );
	float yawDelta = AngleNormalize( GetAbsAngles()[YAW] - staggerAngles[YAW] );
	//Msg( "yawDelta = %f marine angles = %f staggerangles = %f\n", yawDelta, GetAbsAngles()[YAW], staggerAngles[YAW] );

	if ( yawDelta <= 90 && yawDelta >= -90 )
		m_iForcedActionRequest = FORCED_ACTION_KNOCKDOWN_FORWARD;
	else
		m_iForcedActionRequest = FORCED_ACTION_KNOCKDOWN_BACKWARD;

	ApplyAbsVelocityImpulse( vecImpulse );
	
	m_flKnockdownYaw = UTIL_VecToYaw( vecKnockdownDir );

	SetNextStumbleTime( gpGlobals->curtime + asw_knockdown_interval.GetFloat() );
}

void CASW_Marine::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria(set);
	set.AppendCriteria( "who", GetResponseRulesName() );
}

const char * CASW_Marine::GetResponseRulesName()
{
	// a little roundabout for now because we amateurishly
	// have to store criteria values as strings (argh)
	return AI_CriteriaSet::SymbolToStr(GetMarineProfile()->m_nResponseRulesName);
}


CASW_Marine * CASW_Marine::GetSquadLeader()
{
	CASW_SquadFormation * RESTRICT psquad = GetSquadFormation();
	return ( psquad ? psquad->Leader() : NULL );
}

void CASW_Marine::OnWeaponFired( const CBaseEntity *pWeapon, int nShotsFired, bool bIsSecondary /*= false */ )
{
	if( !pWeapon )
		return;

	// Fire weapon fired event for gamestats
	CASW_GameStats.Event_MarineWeaponFired( pWeapon, this, nShotsFired, bIsSecondary );
}

ConVar asw_marine_debug_movement( "asw_marine_debug_movement", "0", FCVAR_CHEAT, "Debug overall marine movement direction" );

void CASW_Marine::AddPositionHistory()
{
	const float flToleranceSqr = asw_movement_direction_tolerance.GetFloat() * asw_movement_direction_tolerance.GetFloat();
	// check we don't have an entry for this spot already
	for ( int i = 0; i < ASW_MARINE_HISTORY_POSITIONS; i++ )
	{
		if ( m_PositionHistory[ i ].flTime != 0.0f && m_PositionHistory[ i ].vecPosition.DistToSqr( GetAbsOrigin() ) < flToleranceSqr )
		{
			if ( asw_marine_debug_movement.GetBool() )
			{
				Msg( "too near pos history %d distsq %f\n", i, m_PositionHistory[ i ].vecPosition.DistToSqr( GetAbsOrigin() ) );
				float flMoveYaw = GetOverallMovementDirection();
				NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 10 ), flMoveYaw, 64, 16, 255, 255, 64, 0, true, asw_movement_direction_interval.GetFloat() );			
				Msg( "Moveyaw = %f\n", flMoveYaw );
			}
			return;
		}
	}
	m_nPositionHistoryTail++;
	if ( m_nPositionHistoryTail >= ASW_MARINE_HISTORY_POSITIONS )
	{
		m_nPositionHistoryTail = 0;
	}

	m_PositionHistory[ m_nPositionHistoryTail ].vecPosition = GetAbsOrigin();
	m_PositionHistory[ m_nPositionHistoryTail ].flTime = gpGlobals->curtime;

	if ( asw_marine_debug_movement.GetBool() )
	{
		Msg( "Stored positioned [%d] = %f %f %f\n", m_nPositionHistoryTail, VectorExpand( GetAbsOrigin() ) );

		NDebugOverlay::Cross( GetAbsOrigin(), 10, 255, 255, 0, false, asw_movement_direction_interval.GetFloat() * 5.0f );

		float flMoveYaw = GetOverallMovementDirection();
		NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 10 ), flMoveYaw, 64, 16, 255, 255, 64, 0, true, asw_movement_direction_interval.GetFloat() );
		Msg( "Moveyaw = %f\n", flMoveYaw );
	}
}

float CASW_Marine::GetOverallMovementDirection()
{
	// take average of position histories
	int nCount = 0;
	Vector vecPos = vec3_origin;
	for ( int i = 0; i < ASW_MARINE_HISTORY_POSITIONS; i++ )
	{
		if ( m_PositionHistory[ i ].flTime != 0.0f )
		{
			vecPos += m_PositionHistory[ i ].vecPosition;
			nCount++;
		}
	}

	if ( nCount == 0 )
	{
		return 90.0f;
	}

	Vector vecSmoothedPosition = vecPos / (float) nCount;

	if ( asw_marine_debug_movement.GetBool() )
	{
		Msg( "Found %d positions in history. total vec = %f %f %f\n", nCount, VectorExpand( vecPos ) );

		NDebugOverlay::Line( WorldSpaceCenter(), vecSmoothedPosition, 255, 0, 0, false, 1.0f );
	}
	
	return UTIL_VecToYaw( ( GetAbsOrigin() - vecSmoothedPosition ).Normalized() );
}

bool CASW_Marine::TeleportStuckMarine()
{
	// Aim in the direction of the last saved position
	Vector vToPrev = m_PositionHistory[ m_nPositionHistoryTail ].vecPosition - GetAbsOrigin();
	VectorNormalize( vToPrev );

	// Add some upward push
	vToPrev.z = 1.0f;
	VectorNormalize( vToPrev );

	Vector vOldPos = GetAbsOrigin();
	bool bSuccess = UTIL_FindClosestPassableSpace( this, vToPrev, MASK_PLAYERSOLID, NULL, FL_AXIS_DIRECTION_NZ );
	if ( bSuccess )
	{
		DevMsg( "Unstuck marine from (%.2f %.2f %.2f) to (%.2f %.2f %.2f).\n", vOldPos.x, vOldPos.y, vOldPos.z, GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
	}

	trace_t tr;
	Ray_t ray;
	ray.Init( GetAbsOrigin(), GetAbsOrigin(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs() );
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	if ( ( tr.contents & MASK_PLAYERSOLID ) && tr.m_pEnt )
	{
		// still stuck
		return TeleportToFreeNode();
	}

	return bSuccess;
}

bool CASW_Marine::TeleportToFreeNode()
{
	// now find the nearest clear info node
	CAI_Node *pNode = NULL;
	CAI_Node *pNearest = NULL;
	float fNearestDist = -1;

	for (int i=0;i<GetNavigator()->GetNetwork()->NumNodes();i++)
	{
		pNode = GetNavigator()->GetNetwork()->GetNode(i);
		if (!pNode)
			continue;
		float dist = GetAbsOrigin().DistTo(pNode->GetOrigin());
		if (dist < fNearestDist || fNearestDist == -1)
		{
			// check the spot is clear
			Vector vecPos = pNode->GetOrigin();
			trace_t tr;			
			UTIL_TraceHull( vecPos,
				vecPos + Vector( 0, 0, 1 ),
				CollisionProp()->OBBMins(),
				CollisionProp()->OBBMaxs(),
				MASK_PLAYERSOLID,
				this,
				COLLISION_GROUP_NONE,
				&tr );
			if( tr.fraction == 1.0 )
			{
				fNearestDist = dist;
				pNearest = pNode;
			}
		}
	}
	// found a valid node, teleport there
	if (pNearest)
	{
		Vector vecPos = pNearest->GetOrigin();
		Teleport( &vecPos, NULL, &vec3_origin );
		return true;
	}
	return false;
}

CBaseTrigger* CASW_Marine::IsInEscapeVolume()
{
	for ( int i = 0; i < g_aEscapeObjectives.Count(); i++ )
	{
		CBaseTrigger *pTrigger = g_aEscapeObjectives[ i ]->GetTrigger();
		if ( pTrigger && pTrigger->IsTouching( this ) )
		{
			return pTrigger;
		}
	}
	return NULL;
}
