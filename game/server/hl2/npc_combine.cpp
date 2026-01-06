//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "ai_route.h"
#include "ai_interactions.h"
#include "ai_tacticalservices.h"
#ifdef EZ
#include "ai_pathfinder.h"
#endif
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "npc_combine.h"
#include "activitylist.h"
#include "player.h"
#include "basecombatweapon.h"
#include "basegrenade_shared.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "globals.h"
#include "grenade_frag.h"
#include "ndebugoverlay.h"
#include "weapon_physcannon.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "npc_headcrab.h"
#ifdef MAPBASE
#include "mapbase/GlobalStrings.h"
#include "globalstate.h"
#include "sceneentity.h"
#endif
#ifdef EZ
#include "npc_antlion.h"
#include "hl2_player.h"

#include "npc_manhack.h"
#include "items.h"
#endif
#ifdef EZ2
#include "ez2/ez2_player.h"
#include "npc_citizen17.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int g_fCombineQuestion;				// true if an idle grunt asked a question. Cleared when someone answers. YUCK old global from grunt code

#ifdef MAPBASE
ConVar npc_combine_idle_walk_easy( "npc_combine_idle_walk_easy", "1", FCVAR_NONE, "Mapbase: Allows Combine soldiers to use ACT_WALK_EASY as a walking animation when idle." );
ConVar npc_combine_unarmed_anims( "npc_combine_unarmed_anims", "1", FCVAR_NONE, "Mapbase: Allows Combine soldiers to use unarmed idle/walk animations when they have no weapon." );
ConVar npc_combine_protected_run( "npc_combine_protected_run", "0", FCVAR_NONE, "Mapbase: Allows Combine soldiers to use \"protected run\" animations." );
ConVar npc_combine_altfire_not_allies_only( "npc_combine_altfire_not_allies_only", "1", FCVAR_NONE, "Mapbase: Elites are normally only allowed to fire their alt-fire attack at the player and the player's allies; This allows elites to alt-fire at other enemies too." );

ConVar npc_combine_new_cover_behavior( "npc_combine_new_cover_behavior", "1", FCVAR_NONE, "Mapbase: Toggles small patches for parts of npc_combine AI related to soldiers failing to take cover. These patches are minimal and only change cases where npc_combine would otherwise look at an enemy without shooting or run up to the player to melee attack when they don't have to. Consult the Mapbase wiki for more information." );

ConVar npc_combine_fixed_shootpos( "npc_combine_fixed_shootpos", "0", FCVAR_NONE, "Mapbase: Toggles fixed Combine soldier shoot position." );
#endif

#ifdef EZ
ConVar npc_combine_fear_near_zombies( "npc_combine_fear_near_zombies", "0", FCVAR_NONE, "Makes soldiers afraid of groups of zombies getting too close to them." );

ConVar npc_combine_give_enabled( "npc_combine_give_enabled", "1", FCVAR_NONE, "Allows players to \"give\" weapons to Combine soldiers in their squad by holding one in front of them for a few seconds." );
ConVar npc_combine_give_stare_dist( "npc_combine_give_stare_dist", "112", FCVAR_NONE, "The distance needed for soldiers to consider the possibility the player wants to give them a weapon." );
ConVar npc_combine_give_stare_time( "npc_combine_give_stare_time", "1", FCVAR_NONE, "The amount of time the player needs to be staring at a soldier in order for them to pick up a weapon they're holding." );

ConVar npc_combine_order_surrender_max_dist( "npc_combine_order_surrender_max_dist", "224", FCVAR_NONE, "The maximum distance a soldier can order surrenders from. Beyond that, they will only pursue." );
ConVar npc_combine_order_surrender_min_tlk_surrender( "npc_combine_order_surrender_min_tlk_surrender", "2.0", FCVAR_NONE, "The minimum amount of time that must pass after a citizen speaks TLK_SURRENDER before being considered for surrenders, assuming they haven't already spoken TLK_BEG." );
ConVar npc_combine_order_surrender_min_tlk_beg( "npc_combine_order_surrender_min_tlk_beg", "0.5", FCVAR_NONE, "The minimum amount of time that must pass after a citizen speaks TLK_BEG before being considered for surrenders, assuming they haven't already spoken TLK_SURRENDER." );

//--------------------------------------------------------------

void CV_SquadmateGlowUpdate( IConVar *var, const char *pOldValue, float flOldValue )
{
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		// Update glows on all soldier squadmates
		if (pNPC->IsCommandable() && pNPC->IsInPlayerSquad())
		{
			CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine *>(pNPC);
			pCombine->UpdateSquadGlow();
		}
	}
}

ConVar	sv_squadmate_glow( "sv_squadmate_glow", "1", FCVAR_ARCHIVE, "If 1, Combine soldier squadmates will glow when they are in the player's squad. The color of the glow represents their HP.", CV_SquadmateGlowUpdate );
ConVar	sv_squadmate_glow_style( "sv_squadmate_glow_style", "1", FCVAR_ARCHIVE, "Different colors for Combine squadmate glows. 0: Green means 100 HP, red means 0 HP\t1: White means 100 HP, red means 0 HP", CV_SquadmateGlowUpdate );
ConVar	sv_squadmate_glow_alpha( "sv_squadmate_glow_alpha", "0.6", FCVAR_ARCHIVE, "On a scale of 0-1, how much alpha should the squadmate glow have", CV_SquadmateGlowUpdate );

#define COMBINE_GLOW_STYLE_REDGREEN	0
#define COMBINE_GLOW_STYLE_REDWHITE	1
#endif

#define COMBINE_SKIN_DEFAULT		0
#define COMBINE_SKIN_SHOTGUNNER		1


#ifndef MAPBASE
#define COMBINE_GRENADE_THROW_SPEED 650
#define COMBINE_GRENADE_TIMER		3.5
#define COMBINE_GRENADE_FLUSH_TIME	3.0		// Don't try to flush an enemy who has been out of sight for longer than this.
#define COMBINE_GRENADE_FLUSH_DIST	256.0	// Don't try to flush an enemy who has moved farther than this distance from the last place I saw him.
#endif

#define COMBINE_LIMP_HEALTH				20
#ifndef MAPBASE
#define	COMBINE_MIN_GRENADE_CLEAR_DIST	250
#endif

#define COMBINE_EYE_STANDING_POSITION	Vector( 0, 0, 66 )
#define COMBINE_GUN_STANDING_POSITION	Vector( 0, 0, 57 )
#define COMBINE_EYE_CROUCHING_POSITION	Vector( 0, 0, 40 )
#define COMBINE_GUN_CROUCHING_POSITION	Vector( 0, 0, 36 )
#define COMBINE_SHOTGUN_STANDING_POSITION	Vector( 0, 0, 36 )
#define COMBINE_SHOTGUN_CROUCHING_POSITION	Vector( 0, 0, 36 )
#define COMBINE_MIN_CROUCH_DISTANCE		256.0

#ifdef EZ
// Added by 1upD - Time to next regeneration sound in seconds
#define COMBINE_REGEN_SOUND_TIME	10.0;

// 
// Blixibon - Combine manhack support
// 
// Soldiers can now carry and deploy manhacks like metrocops do.
// 

#define	COMBINE_BODYGROUP_MANHACK	1

// Metrocops use their own squad slot, but for soldiers we can get away with re-using the "special attack" squad slot elites use.
#define SQUAD_SLOT_COMBINE_DEPLOY_MANHACK SQUAD_SLOT_SPECIAL_ATTACK
#endif

ConVar sv_combine_eye_tracers("sv_combine_eye_tracers", "1", FCVAR_REPLICATED);

//-----------------------------------------------------------------------------
// Static stuff local to this file.
//-----------------------------------------------------------------------------
// This is the index to the name of the shotgun's classname in the string pool
// so that we can get away with an integer compare rather than a string compare.
#ifdef MAPBASE
#define s_iszShotgunClassname gm_isz_class_Shotgun
#else
string_t	s_iszShotgunClassname;
#endif

//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int	g_interactionCombineBash		= 0; // melee bash attack

//=========================================================
// Combines's Anim Events Go Here
//=========================================================
#define COMBINE_AE_RELOAD			( 2 )
#define COMBINE_AE_KICK				( 3 )
#define COMBINE_AE_AIM				( 4 )
#ifndef MAPBASE
#define COMBINE_AE_GREN_TOSS		( 7 )
#endif
#define COMBINE_AE_GREN_LAUNCH		( 8 )
#define COMBINE_AE_GREN_DROP		( 9 )
#define COMBINE_AE_CAUGHT_ENEMY		( 10) // grunt established sight with an enemy (player only) that had previously eluded the squad.

#ifndef MAPBASE
int COMBINE_AE_BEGIN_ALTFIRE;
int COMBINE_AE_ALTFIRE;
#endif
#ifdef EZ
static int AE_METROPOLICE_START_DEPLOY;
static int AE_METROPOLICE_DEPLOY_MANHACK;
#endif

//=========================================================
// Combine activities
//=========================================================
//Activity ACT_COMBINE_STANDING_SMG1;
//Activity ACT_COMBINE_CROUCHING_SMG1;
//Activity ACT_COMBINE_STANDING_AR2;
//Activity ACT_COMBINE_CROUCHING_AR2;
//Activity ACT_COMBINE_WALKING_AR2;
//Activity ACT_COMBINE_STANDING_SHOTGUN;
//Activity ACT_COMBINE_CROUCHING_SHOTGUN;
#if !SHARED_COMBINE_ACTIVITIES
Activity ACT_COMBINE_THROW_GRENADE;
#endif
Activity ACT_COMBINE_LAUNCH_GRENADE;
Activity ACT_COMBINE_BUGBAIT;
#if !SHARED_COMBINE_ACTIVITIES
Activity ACT_COMBINE_AR2_ALTFIRE;
#endif
Activity ACT_WALK_EASY;
Activity ACT_WALK_MARCH; 
#ifdef MAPBASE
Activity ACT_TURRET_CARRY_IDLE;
Activity ACT_TURRET_CARRY_WALK;
Activity ACT_TURRET_CARRY_RUN;
#endif
#ifdef EZ
extern int ACT_METROPOLICE_DEPLOY_MANHACK;
#endif

// -----------------------------------------------
//	> Squad slots
// -----------------------------------------------
enum SquadSlot_T
{	
	SQUAD_SLOT_GRENADE1 = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_GRENADE2,
	SQUAD_SLOT_ATTACK_OCCLUDER,
	SQUAD_SLOT_OVERWATCH,
};

enum TacticalVariant_T
{
	TACTICAL_VARIANT_DEFAULT = 0,
	TACTICAL_VARIANT_PRESSURE_ENEMY,				// Always try to close in on the player.
	TACTICAL_VARIANT_PRESSURE_ENEMY_UNTIL_CLOSE,	// Act like VARIANT_PRESSURE_ENEMY, but go to VARIANT_DEFAULT once within 30 feet
#ifdef MAPBASE
	TACTICAL_VARIANT_GRENADE_HAPPY,					// Throw grenades as if you're fighting a turret
#endif
};

enum PathfindingVariant_T
{
	PATHFINDING_VARIANT_DEFAULT = 0,
};


#define bits_MEMORY_PAIN_LIGHT_SOUND		bits_MEMORY_CUSTOM1
#define bits_MEMORY_PAIN_HEAVY_SOUND		bits_MEMORY_CUSTOM2
#define bits_MEMORY_PLAYER_HURT				bits_MEMORY_CUSTOM3
#ifdef EZ2
// Blixibon - Soldiers sometimes stare at things like objects the player is carrying (e.g. Wilson).
#define bits_MEMORY_SAW_PLAYER_WEIRDNESS	bits_MEMORY_CUSTOM4
#endif

LINK_ENTITY_TO_CLASS( npc_combine, CNPC_Combine );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Combine )

DEFINE_FIELD( m_nKickDamage, FIELD_INTEGER ),
DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
#ifndef MAPBASE
DEFINE_FIELD( m_hForcedGrenadeTarget, FIELD_EHANDLE ),
#endif
DEFINE_FIELD( m_bShouldPatrol, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bFirstEncounter, FIELD_BOOLEAN ),
DEFINE_FIELD( m_flNextPainSoundTime, FIELD_TIME ),
#ifdef EZ
DEFINE_FIELD( m_flNextRegenSoundTime, FIELD_TIME),
#endif
DEFINE_FIELD( m_flNextAlertSoundTime, FIELD_TIME ),
#ifndef MAPBASE
DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
#endif
DEFINE_FIELD( m_flNextLostSoundTime, FIELD_TIME ),
DEFINE_FIELD( m_flAlertPatrolTime, FIELD_TIME ),
#ifndef MAPBASE
DEFINE_FIELD( m_flNextAltFireTime, FIELD_TIME ),
#endif
DEFINE_FIELD( m_nShots, FIELD_INTEGER ),
DEFINE_FIELD( m_flShotDelay, FIELD_FLOAT ),
DEFINE_FIELD( m_flStopMoveShootTime, FIELD_TIME ),
#ifdef EZ
DEFINE_FIELD( m_iszOriginalSquad, FIELD_STRING ), // Added by Blixibon to save original squad
DEFINE_FIELD( m_bHoldPositionGoal, FIELD_BOOLEAN ),
DEFINE_FIELD( m_flTimePlayerStare, FIELD_TIME ),
DEFINE_FIELD( m_bTemporarilyNeedWeapon, FIELD_BOOLEAN ),
DEFINE_FIELD( m_flNextHealthSearchTime, FIELD_TIME ),
DEFINE_INPUT( m_bLookForItems, FIELD_BOOLEAN, "SetLookForItems" ),
#endif
#ifdef EZ2
DEFINE_FIELD( m_hObstructor, FIELD_EHANDLE ),
DEFINE_FIELD( m_flTimeSinceObstructed, FIELD_TIME ),
#endif
#ifndef MAPBASE // See ai_grenade.h
DEFINE_KEYFIELD( m_iNumGrenades, FIELD_INTEGER, "NumGrenades" ),
#else
DEFINE_INPUT( m_bUnderthrow, FIELD_BOOLEAN, "UnderthrowGrenades" ),
DEFINE_INPUT( m_bAlternateCapable, FIELD_BOOLEAN, "SetAlternateCapable" ),
#endif
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
DEFINE_EMBEDDED( m_Sentences ),
#endif

//							m_AssaultBehavior (auto saved by AI)
//							m_StandoffBehavior (auto saved by AI)
//							m_FollowBehavior (auto saved by AI)
//							m_FuncTankBehavior (auto saved by AI)
//							m_RappelBehavior (auto saved by AI)
//							m_ActBusyBehavior (auto saved by AI)

DEFINE_INPUTFUNC( FIELD_VOID,	"LookOff",	InputLookOff ),
DEFINE_INPUTFUNC( FIELD_VOID,	"LookOn",	InputLookOn ),

DEFINE_INPUTFUNC( FIELD_VOID,	"StartPatrolling",	InputStartPatrolling ),
DEFINE_INPUTFUNC( FIELD_VOID,	"StopPatrolling",	InputStopPatrolling ),

DEFINE_INPUTFUNC( FIELD_STRING,	"Assault", InputAssault ),

DEFINE_INPUTFUNC( FIELD_VOID,	"HitByBugbait",		InputHitByBugbait ),

#ifndef MAPBASE
DEFINE_INPUTFUNC( FIELD_STRING,	"ThrowGrenadeAtTarget",	InputThrowGrenadeAtTarget ),
#else
DEFINE_INPUTFUNC( FIELD_BOOLEAN,	"SetElite",	InputSetElite ),

DEFINE_INPUTFUNC( FIELD_VOID,	"DropGrenade",	InputDropGrenade ),

DEFINE_INPUTFUNC( FIELD_INTEGER,	"SetTacticalVariant",	InputSetTacticalVariant ),

DEFINE_INPUTFUNC( FIELD_STRING, "SetPoliceGoal", InputSetPoliceGoal ),

DEFINE_AIGRENADE_DATADESC()
#endif

#ifdef EZ
DEFINE_INPUTFUNC(FIELD_STRING, "SetCommandable", InputSetCommandable), // New inputs to toggle player commands on / off
DEFINE_INPUTFUNC(FIELD_STRING, "SetNonCommandable", InputSetNonCommandable),

DEFINE_INPUTFUNC( FIELD_VOID,	"RemoveFromPlayerSquad", InputRemoveFromPlayerSquad ),
DEFINE_INPUTFUNC( FIELD_VOID,	"AddToPlayerSquad", InputAddToPlayerSquad ),

DEFINE_KEYFIELD( m_iManhacks, FIELD_INTEGER, "manhacks" ),
DEFINE_FIELD( m_hManhack, FIELD_EHANDLE ),
DEFINE_INPUTFUNC( FIELD_VOID, "EnableManhackToss", InputEnableManhackToss ),
DEFINE_INPUTFUNC( FIELD_VOID, "DisableManhackToss", InputDisableManhackToss ),
DEFINE_INPUTFUNC( FIELD_VOID, "DeployManhack", InputDeployManhack ),
DEFINE_INPUTFUNC( FIELD_INTEGER, "AddManhacks", InputAddManhacks ),
DEFINE_INPUTFUNC( FIELD_INTEGER, "SetManhacks", InputSetManhacks ),
DEFINE_OUTPUT( m_OutManhack, "OutManhack" ),

DEFINE_USEFUNC( Use ),
DEFINE_OUTPUT( m_OnPlayerUse, "OnPlayerUse" ),

DEFINE_KEYFIELD( m_bDisablePlayerUse, FIELD_BOOLEAN, "DisablePlayerUse" ),
DEFINE_INPUTFUNC( FIELD_VOID, "EnablePlayerUse", InputEnablePlayerUse ),
DEFINE_INPUTFUNC( FIELD_VOID, "DisablePlayerUse", InputDisablePlayerUse ),

DEFINE_KEYFIELD( m_iCanOrderSurrender, FIELD_INTEGER, "CanOrderSurrender" ),
DEFINE_INPUTFUNC( FIELD_VOID, "EnableOrderSurrender", InputEnableOrderSurrender ),
DEFINE_INPUTFUNC( FIELD_VOID, "DisableOrderSurrender", InputDisableOrderSurrender ),

DEFINE_KEYFIELD( m_iCanPlayerGive, FIELD_INTEGER, "CanPlayerGive" ),
DEFINE_INPUTFUNC( FIELD_VOID, "EnablePlayerGive", InputEnablePlayerGive ),
DEFINE_INPUTFUNC( FIELD_VOID, "DisablePlayerGive", InputDisablePlayerGive ),
#endif

#ifndef MAPBASE
DEFINE_FIELD( m_iLastAnimEventHandled, FIELD_INTEGER ),
#endif
DEFINE_FIELD( m_fIsElite, FIELD_BOOLEAN ),
#ifndef MAPBASE
DEFINE_FIELD( m_vecAltFireTarget, FIELD_VECTOR ),
#endif

DEFINE_KEYFIELD( m_iTacticalVariant, FIELD_INTEGER, "tacticalvariant" ),
DEFINE_KEYFIELD( m_iPathfindingVariant, FIELD_INTEGER, "pathfindingvariant" ),

END_DATADESC()


//------------------------------------------------------------------------------
// Constructor.
//------------------------------------------------------------------------------
CNPC_Combine::CNPC_Combine()
{
	m_vecTossVelocity = vec3_origin;
#ifdef EZ
	// For players who use npc_create
	AddSpawnFlags( SF_COMBINE_COMMANDABLE | SF_COMBINE_REGENERATE );
	m_iNumGrenades = 5;

	m_bDontPickupWeapons = true;

	m_iCanOrderSurrender = TRS_NONE;
	m_iCanPlayerGive = TRS_NONE;
#endif
}

#ifdef EZ
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::ClearFollowTarget()
{
	m_FollowBehavior.SetFollowTarget(NULL);
	m_FollowBehavior.SetParameters(AIF_SIMPLE);
}

//-----------------------------------------------------------------------------
// BREADMAN - lets you 'use' the soldiers - OMG IT WORKS
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	m_OnPlayerUse.FireOutput(pActivator, pCaller);

#ifdef EZ
	if ( m_bDisablePlayerUse )
	{
		return;
	}
#endif

	if (pActivator == UTIL_GetLocalPlayer() && IsCommandable())
	{
		bool isInPlayerSquad = IsInPlayerSquad();
		bool bSpoken = false;
#ifdef EZ2
		CEZ2_Player *badcop = assert_cast<CEZ2_Player *>(pActivator);
		if (badcop)
		{
			if (isInPlayerSquad)
			{
				bSpoken = badcop->HandleRemoveFromPlayerSquad(this);
			}
			else
			{
				bSpoken = badcop->HandleAddToPlayerSquad(this);
			}
		}
#endif

		// I'm the target! Toggle follow!
		if (isInPlayerSquad)
		{
			// Blixibon - This soldier should stay in the area
			// in case the player just wants them to hold down
			m_bHoldPositionGoal = true;

			if (!bSpoken)
				StopFollowSound();

			RemoveFromPlayerSquad();
		}
		else
		{
			// Blixibon -- Look at the player when asked to follow. (needs head-turning model)
			AddLookTarget(pActivator, 0.75, 3.0);

			if (!bSpoken)
				FollowSound();

			AddToPlayerSquad();
		}
	}
}

//-----------------------------------------------------------------------------
// 1upD - Determine if this NPC can be commanded
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsCommandable()
{
	return HasSpawnFlags(SF_COMBINE_COMMANDABLE) || BaseClass::IsCommandable();
}

//-----------------------------------------------------------------------------
// 1upD - Should this NPC always think?
//		Combine soldiers always think if they are commandable.
//		Thanks Blixibon!
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldAlwaysThink()
{
	return IsCommandable() || BaseClass::ShouldAlwaysThink();
}

//-----------------------------------------------------------------------------
// Blixibon - Ported citizen code which allows squadmates to use func_tanks
//-----------------------------------------------------------------------------
#define COMBINE_FOLLOWER_DESERT_FUNCTANK_DIST	45.0f*12.0f
bool CNPC_Combine::ShouldBehaviorSelectSchedule( CAI_BehaviorBase *pBehavior )
{
	if( pBehavior == &m_FollowBehavior )
	{
		// Suppress follow behavior if I have a func_tank and the func tank is near
		// what I'm supposed to be following.
		if( m_FuncTankBehavior.CanSelectSchedule() )
		{
			// Is the tank close to the follow target?
			Vector vecTank = m_FuncTankBehavior.GetFuncTank()->WorldSpaceCenter();
			Vector vecFollowGoal = m_FollowBehavior.GetFollowGoalInfo().position;

			float flTankDistSqr = (vecTank - vecFollowGoal).LengthSqr();
			float flAllowDist = m_FollowBehavior.GetFollowGoalInfo().followPointTolerance * 2.0f;
			float flAllowDistSqr = flAllowDist * flAllowDist;
			if( flTankDistSqr < flAllowDistSqr )
			{
				// Deny follow behavior so the tank can go.
				return false;
			}
		}
	}
	else if( IsInPlayerSquad() && pBehavior == &m_FuncTankBehavior && m_FuncTankBehavior.IsMounted() )
	{
		if( m_FollowBehavior.GetFollowTarget() )
		{
			Vector vecFollowGoal = m_FollowBehavior.GetFollowTarget()->GetAbsOrigin();
			if( vecFollowGoal.DistToSqr( GetAbsOrigin() ) > Square(COMBINE_FOLLOWER_DESERT_FUNCTANK_DIST) )
			{
				return false;
			}
		}
	}

	return BaseClass::ShouldBehaviorSelectSchedule( pBehavior );
}

//-----------------------------------------------------------------------------
// 1upD - Get a pointer to the NPC representing this squad. Used by HUD
//-----------------------------------------------------------------------------
CAI_BaseNPC * CNPC_Combine::GetSquadCommandRepresentative()
{
    // Blixibon - Port of npc_citizen code
	if ( !AI_IsSinglePlayer() )
		return NULL;

	if ( IsInPlayerSquad() )
	{
		static float lastTime;
		static AIHANDLE hCurrent;

		if ( gpGlobals->curtime - lastTime > 2.0 || !hCurrent || !hCurrent->IsInPlayerSquad() ) // hCurrent will be NULL after level change
		{
			lastTime = gpGlobals->curtime;
			hCurrent = NULL;

			CUtlVectorFixed<SquadMemberInfo_t, MAX_SQUAD_MEMBERS> candidates;
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

			if ( pPlayer )
			{
				AISquadIter_t iter;
				for ( CAI_BaseNPC *pAllyNpc = m_pSquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pSquad->GetNextMember(&iter) )
				{
					if ( pAllyNpc->IsCommandable() && dynamic_cast<CNPC_PlayerCompanion*>(pAllyNpc) )
					{
						int i = candidates.AddToTail();
						candidates[i].pMember = (CNPC_PlayerCompanion*)(pAllyNpc);
						candidates[i].bSeesPlayer = pAllyNpc->HasCondition( COND_SEE_PLAYER );
						candidates[i].distSq = ( pAllyNpc->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr();
					}
				}

				if ( candidates.Count() > 0 )
				{
					candidates.Sort( SquadSortFunc );
					hCurrent = candidates[0].pMember;
				}
			}
		}

		if ( hCurrent != NULL )
		{
			Assert( dynamic_cast<CNPC_PlayerCompanion*>(hCurrent.Get()) && hCurrent->IsInPlayerSquad() );
			return hCurrent;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// 1upD - Does this soldier have a command goal?
//-----------------------------------------------------------------------------
bool CNPC_Combine::HaveCommandGoal() const
{
	if (GetCommandGoal() != vec3_invalid)
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: return TRUE if the commander mode should try to give this order
//			to more people. return FALSE otherwise. For instance, we don't
//			try to send all 3 selectedcitizens to pick up the same gun.
//-----------------------------------------------------------------------------
bool CNPC_Combine::TargetOrder(CBaseEntity *pTarget, CAI_BaseNPC **Allies, int numAllies)
{
	if (pTarget && pTarget->IsPlayer())
	{
		ToggleSquadCommand();
	}

	return true;
}

//-----------------------------------------------------------------------------
// 1upD	-	Order this soldier to move to a point or regroup
//-----------------------------------------------------------------------------
void CNPC_Combine::MoveOrder(const Vector & vecDest, CAI_BaseNPC ** Allies, int numAllies)
{
	if( m_StandoffBehavior.IsRunning() )
	{
		m_StandoffBehavior.SetStandoffGoalPosition( vecDest );
	}

	// If in assault, cancel and move.
	if( m_AssaultBehavior.HasHitRallyPoint() && !m_AssaultBehavior.HasHitAssaultPoint() )
	{
		m_AssaultBehavior.Disable();
		ClearSchedule( "Moving from rally point to assault point" );
	}

	// Stop following.
	m_FollowBehavior.SetFollowTarget(NULL);

	BaseClass::MoveOrder( vecDest, Allies, numAllies );
}

//-----------------------------------------------------------------------------
// 1upD	-	Handle soldier toggle follow player / command
//			TODO Rename this method - toggles squad follow state
//-----------------------------------------------------------------------------
void CNPC_Combine::ToggleSquadCommand()
{
	// Get the player
	CBasePlayer * pPlayer = UTIL_GetLocalPlayer();

	// I'm the target! Toggle follow!
	if (IsInPlayerSquad() && m_FollowBehavior.GetFollowTarget() != pPlayer)
	{
		ClearFollowTarget();
		SetCommandGoal(vec3_invalid);

		// Turn follow on!
		m_AssaultBehavior.Disable();
		m_FollowBehavior.SetFollowTarget(pPlayer);
		m_FollowBehavior.SetParameters(AIF_SIMPLE);

		m_OnFollowOrder.FireOutput(this, this);
	}
	else
	{
		// Stop following.
		m_FollowBehavior.SetFollowTarget(NULL);
	}
}

#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
// This is how we make the soldier actually talk with captions. SWEET!
void CNPC_Combine::FollowSound()
{
	m_Sentences.Speak("COMBINE_EZ2_FOLLOWING", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS);
}

// This is how we make the soldier actually talk with captions. SWEET!
void CNPC_Combine::StopFollowSound()
{
	m_Sentences.Speak("COMBINE_EZ2_STOPFOLLOWING", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS);
}
#else
void CNPC_Combine::FollowSound()
{
	SpeakIfAllowed( TLK_STARTFOLLOW );
}
void CNPC_Combine::StopFollowSound()
{
	SpeakIfAllowed( TLK_STOPFOLLOW );
}
#endif

//-----------------------------------------------------------------------------
// 1upD	-	Add this soldier to the player's squad
//			Based on Breadman's use function code
//-----------------------------------------------------------------------------
void CNPC_Combine::AddToPlayerSquad()
{
	if ( IsInPlayerSquad() )
	{
		// Already in the player's squad
		return;
	}

	m_iszOriginalSquad = m_SquadName;
	AddToSquad(AllocPooledString(PLAYER_SQUADNAME));

	// Blixibon - If we have a manhack, update its squad status
	if (m_hManhack != NULL)
		m_pSquad->AddToSquad(m_hManhack.Get());

	// Blixibon - Soldiers are much better at shooting if they don't have to account for this
	CapabilitiesRemove( bits_CAP_NO_HIT_SQUADMATES );
	
	FixupPlayerSquad();
}

//-----------------------------------------------------------------------------
// Blixibon	-	Perform squad fixup methods
//				Based on 1upD and Breadman's AddToPlayerSquad and use function code, respectively
//-----------------------------------------------------------------------------
void CNPC_Combine::FixupPlayerSquad()
{
	// Get the current goal from squadmates
	GetSquadGoal();

	// Don't handle move order if this NPC already has a command goal - ie on load game
	if (HaveCommandGoal() && !m_bHoldPositionGoal)
		return;

	m_bHoldPositionGoal = false;

	ToggleSquadCommand();

	UpdateSquadGlow();
}

//-----------------------------------------------------------------------------
// Purpose: Add or remove glow effects as necessary, then update the color
//-----------------------------------------------------------------------------
void CNPC_Combine::UpdateSquadGlow()
{
	bool shouldGlow = ShouldSquadGlow();
	if ( !m_bGlowEnabled.Get() && shouldGlow )
	{
		AddGlowEffect();
	}
	else if ( m_bGlowEnabled.Get() && !shouldGlow )
	{
		RemoveGlowEffect();
	}

	if ( m_bGlowEnabled.Get() )
	{
		float healthPercentage = ((float)m_iHealth / ((float)m_iMaxHealth));

		float red = 0;
		float blue = 0;
		float green = 0;

		switch (sv_squadmate_glow_style.GetInt())
		{
		case COMBINE_GLOW_STYLE_REDGREEN:
			red = 1.0f - healthPercentage;
			green = healthPercentage;
			blue = 0.0f;
			break;
		case COMBINE_GLOW_STYLE_REDWHITE:
			red = 1.0f;
			green = healthPercentage;
			blue = healthPercentage;
			break;
		}

		SetGlowColor( red, green, blue, sv_squadmate_glow_alpha.GetFloat() );
	}
}

bool CNPC_Combine::ShouldSquadGlow()
{
	return sv_squadmate_glow.GetBool() && IsCommandable() && IsInPlayerSquad();
}

//-----------------------------------------------------------------------------
// 1upD	-	Retrieve the squad's current command goal
//-----------------------------------------------------------------------------
void CNPC_Combine::GetSquadGoal()
{
	CAI_Squad *pPlayerSquad = g_AI_SquadManager.FindSquad(MAKE_STRING(PLAYER_SQUADNAME));
	if (pPlayerSquad)
	{
		AISquadIter_t iter;

		for (CAI_BaseNPC *pPlayerSquadMember = pPlayerSquad->GetFirstMember(&iter); pPlayerSquadMember; pPlayerSquadMember = pPlayerSquad->GetNextMember(&iter))
		{
			if (pPlayerSquadMember->GetCommandGoal() != vec3_invalid) {
				SetCommandGoal(pPlayerSquadMember->GetCommandGoal());
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// 1upD	-	Add this soldier to the player's squad
//			Based on Breadman's use function code
//-----------------------------------------------------------------------------
void CNPC_Combine::RemoveFromPlayerSquad()
{
	if ( !IsInPlayerSquad() )
	{
		// Blixibon - Stop holding position
		if ( m_bHoldPositionGoal )
			SetCommandGoal( vec3_invalid );

		// Already outside of the player's squad
		return;
	}

	if ( m_iszOriginalSquad != NULL_STRING && strcmp( STRING( m_iszOriginalSquad ), PLAYER_SQUADNAME ) != 0 )
		AddToSquad( m_iszOriginalSquad );
	else
		RemoveFromSquad();

	// Blixibon - If we have a manhack, update its squad status
	if (m_hManhack != NULL)
		m_pSquad ? m_pSquad->AddToSquad(m_hManhack.Get()) : m_hManhack->RemoveFromSquad();

	CapabilitiesAdd( bits_CAP_NO_HIT_SQUADMATES );

	// Stop following.
	ToggleSquadCommand();

	// Blixibon - When players remove soldiers from their squad, they will stay in their area
	if ( m_bHoldPositionGoal )
	{
		SetCommandGoal( GetAbsOrigin() );
	}
	else
	{
		SetCommandGoal( vec3_invalid );
	}

	UpdateSquadGlow();
}

//-----------------------------------------------------------------------------
// 1upD	-	Override PlayerAlly's ShouldRegenerateHealth() to check spawnflag
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldRegenerateHealth(void)
{
	return HasSpawnFlags(SF_COMBINE_REGENERATE);
}

//-----------------------------------------------------------------------------
// 1upD	-	Copied from citizen
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsFollowingCommandPoint()
{
	CBaseEntity *pFollowTarget = m_FollowBehavior.GetFollowTarget();
	if (pFollowTarget)
		return FClassnameIs(pFollowTarget, "info_target_command_point");
	return false;
}

//-----------------------------------------------------------------------------
// 1upD	-	Copied from citizen
//-----------------------------------------------------------------------------
void CNPC_Combine::UpdateFollowCommandPoint()
{
	if (!AI_IsSinglePlayer())
		return;

	if (IsInPlayerSquad())
	{
		if (HaveCommandGoal())
		{
			CBaseEntity *pFollowTarget = m_FollowBehavior.GetFollowTarget();
			CBaseEntity *pCommandPoint = gEntList.FindEntityByClassname(NULL, "info_target_command_point");

			if (!pCommandPoint)
			{
				DevMsg("**\nVERY BAD THING\nCommand point vanished! Creating a new one\n**\n");
				pCommandPoint = CreateEntityByName("info_target_command_point");
			}

			if (pFollowTarget != pCommandPoint)
			{
				pFollowTarget = pCommandPoint;
				m_FollowBehavior.SetFollowTarget(pFollowTarget);
				m_FollowBehavior.SetParameters(AIF_COMMANDER);
			}

			if ((pCommandPoint->GetAbsOrigin() - GetCommandGoal()).LengthSqr() > 0.01)
			{
				UTIL_SetOrigin(pCommandPoint, GetCommandGoal(), false);
			}
		}
		else
		{
			if (IsFollowingCommandPoint())
				ClearFollowTarget();
			if (m_FollowBehavior.GetFollowTarget() != UTIL_GetLocalPlayer())
			{
				DevMsg("Expected to be following player, but not\n");
				m_FollowBehavior.SetFollowTarget(UTIL_GetLocalPlayer());
				m_FollowBehavior.SetParameters(AIF_SIMPLE);
			}
		}
	}
	else if (IsFollowingCommandPoint())
		ClearFollowTarget();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define OTHER_DEFER_SEARCH_TIME		FLT_MAX
bool CNPC_Combine::ShouldLookForBetterWeapon()
{
	if ( BaseClass::ShouldLookForBetterWeapon() )
	{
		if ( IsInPlayerSquad() && (GetActiveWeapon()&&IsMoving()) && ( m_FollowBehavior.GetFollowTarget() && m_FollowBehavior.GetFollowTarget()->IsPlayer() ) )
		{
			// For soldiers in the player squad, you must be unarmed, or standing still (if armed) in order to 
			// divert attention to looking for a new weapon.
			return false;
		}

		if ( GetActiveWeapon() && IsMoving() )
			return false;

#ifdef DBGFLAG_ASSERT
		// Cached off to make sure you change this if you ask the code to defer.
		float flOldWeaponSearchTime = m_flNextWeaponSearchTime;
#endif

		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if( pWeapon )
		{
			bool bDefer = false;

			if ( EntIsClass(pWeapon, gm_isz_class_AR2) )
			{
				// Content to keep this weapon forever
				m_flNextWeaponSearchTime = OTHER_DEFER_SEARCH_TIME;
				bDefer = true;
			}
			else if( EntIsClass(pWeapon, gm_isz_class_RPG) )
			{
				// Content to keep this weapon forever
				m_flNextWeaponSearchTime = OTHER_DEFER_SEARCH_TIME;
				bDefer = true;
			}
			else if ( EntIsClass(pWeapon, gm_isz_class_Shotgun) )
			{
				if (m_nSkin == COMBINE_SKIN_SHOTGUNNER)
				{
					// Shotgunners are content to keep this weapon forever
					m_flNextWeaponSearchTime = OTHER_DEFER_SEARCH_TIME;
					bDefer = true;
				}
			}

			if( bDefer )
			{
				// I'm happy with my current weapon. Don't search now.
				// If you ask the code to defer, you must have set m_flNextWeaponSearchTime to when
				// you next want to try to search.
				Assert( m_flNextWeaponSearchTime != flOldWeaponSearchTime );
				return false;
			}
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldLookForHealthItem()
{
	if( !m_bLookForItems )
		return false;

	if( gpGlobals->curtime < m_flNextHealthSearchTime )
		return false;

	// Wait till you're standing still.
	if( IsMoving() )
		return false;

	if (IsCommandable())
	{
		// Commandable soldiers cannot get health unless in the player squad.
		if (!IsInPlayerSquad())
			return false;

		// Only begin considering this when low on health.
		if ((GetHealth() / GetMaxHealth()) > 0.5f)
			return false;

		// Player is hurt, don't steal their health.
		if( AI_IsSinglePlayer() && UTIL_GetLocalPlayer()->GetHealth() <= UTIL_GetLocalPlayer()->GetHealth() * 0.75f )
			return false;
	}
	else
	{
		// Always consider health when not fully healthy.
		if (GetHealth() >= GetMaxHealth())
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Combine::FindHealthItem( const Vector &vecPosition, const Vector &range )
{
	CBaseEntity *list[1024];
	int count = UTIL_EntitiesInBox( list, 1024, vecPosition - range, vecPosition + range, 0 );

	for ( int i = 0; i < count; i++ )
	{
		CItem *pItem = dynamic_cast<CItem *>(list[ i ]);

		if( pItem )
		{
			if (pItem->HasSpawnFlags(SF_ITEM_NO_NPC_PICKUP))
				continue;

			// Healthkits, healthvials, and batteries
			if( (pItem->ClassMatches( "item_health*" ) || pItem->ClassMatches( "item_battery" )) && FVisible( pItem ) )
			{
				return pItem;
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CNPC_Combine::PickupItem( CBaseEntity *pItem )
{
	BaseClass::PickupItem( pItem );

	// Combine soldiers can pick up batteries and use them as health kits
	if (FClassnameIs( pItem, "item_battery" ))
	{
		if ( TakeHealth( 25, DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
			EmitSound( "ItemBattery.Touch" );
		}
	}

	// Update squad glow in case our health changed
	if (ShouldSquadGlow())
	{
		UpdateSquadGlow();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectSchedulePriorityAction()
{
	int schedule = BaseClass::SelectSchedulePriorityAction();
	if ( schedule != SCHED_NONE )
		return schedule;

	schedule = SelectScheduleRetrieveItem();
	if ( schedule != SCHED_NONE )
		return schedule;

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectScheduleRetrieveItem()
{
	if ( HasCondition(COND_BETTER_WEAPON_AVAILABLE) )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(Weapon_FindUsable( WEAPON_SEARCH_DELTA ));
		if ( pWeapon )
		{
			m_flNextWeaponSearchTime = gpGlobals->curtime + 10.0;
			// Now lock the weapon for several seconds while we go to pick it up.
			pWeapon->Lock( 10.0, this );
			SetTarget( pWeapon );
			return SCHED_NEW_WEAPON;
		}
	}

	if( HasCondition(COND_HEALTH_ITEM_AVAILABLE) )
	{
		if( !IsInPlayerSquad() && IsCommandable() )
		{
			// Been kicked out of the player squad since the time I located the health.
			ClearCondition( COND_HEALTH_ITEM_AVAILABLE );
		}
		else
		{
			CBaseEntity *pBase = FindHealthItem(
				m_FollowBehavior.GetFollowTarget() ? m_FollowBehavior.GetFollowTarget()->GetAbsOrigin() : GetAbsOrigin(),
				m_FollowBehavior.GetFollowTarget() ? Vector( 120, 120, 120 ) : Vector( 240, 240, 240 ) );
			CItem *pItem = dynamic_cast<CItem *>(pBase);

			if( pItem )
			{
				SetTarget( pItem );
				return SCHED_GET_HEALTHKIT;
			}
		}
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::IgnorePlayerPushing( void )
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer )
	{
		if ( IsCommandable() && !IsInAScript() && npc_combine_give_enabled.GetBool() )
		{
			// Don't push if the player is carrying a weapon
			CBaseEntity *pHeld = GetPlayerHeldEntity( pPlayer );
			if (pHeld && (pHeld->IsBaseCombatWeapon() || (pHeld->IsCombatItem() && IsGiveableItem(pHeld))))
				return true;
		}
	}

	return BaseClass::IgnorePlayerPushing();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );

	if (m_bDontPickupWeapons && !IsInAScript() && IsCurSchedule( SCHED_NEW_WEAPON ))
	{
		// A non-autonomous soldier has dropped their weapon when trying to pick one up.
		// Make sure they're allowed to re-arm themselves in case they fail to pick up this weapon.
		m_bTemporarilyNeedWeapon = true;
		m_bDontPickupWeapons = false;
		m_flNextWeaponSearchTime = gpGlobals->curtime + 1.0f;

		// Check if we don't have other weapons in our inventory
		if ( !m_hMyWeapons[1].Get() && GetBackupWeaponClass() != NULL )
		{
			// In case we get into combat while unarmed, have a backup weapon on-hand
			GiveWeaponHolstered( MAKE_STRING( GetBackupWeaponClass() ) );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::PickupWeapon( CBaseCombatWeapon *pWeapon )
{
	BaseClass::PickupWeapon( pWeapon );

	if (m_bTemporarilyNeedWeapon)
	{
		// We have our weapon now, so turn off weapon pickup
		m_bTemporarilyNeedWeapon = false;
		m_bDontPickupWeapons = true;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CNPC_Combine::GetBackupWeaponClass()
{
	// Elites can pull out pulse pistols
	if (IsElite())
		return "weapon_pulsepistol";

	return "weapon_pistol";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::CanOrderSurrender()
{
#ifdef EZ2
	if (m_iCanOrderSurrender == TRS_NONE && IsPlayerAlly())
	{
		// Order surrenders if the player has ordered one before
		return GlobalEntity_GetState( GLOBAL_PLAYER_ORDER_SURRENDER ) == GLOBAL_ON;
	}

	return m_iCanOrderSurrender == TRS_TRUE;
#endif
	
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsGiveableItem( CBaseEntity *pItem )
{
	if (pItem->IsCombatItem())
	{
		if (pItem->ClassMatches( "item_health*" ) || pItem->ClassMatches( "item_battery" ))
		{
			// Only take it if we need the health
			if (GetHealth() >= GetMaxHealth())
				return false;
			else
				return true;
		}

		return pItem->ClassMatches( "item_ammo_ar2_altfire" ) || pItem->ClassMatches( "item_ammo_smg1_grenade" );
	}
	else if (pItem->ClassMatches( "weapon_frag" ))
	{
		return true;
	}

	return false;
}

ConVar npc_combine_gib( "npc_combine_gib", "0" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldGib( const CTakeDamageInfo &info )
{
	if ( !npc_combine_gib.GetBool() )
		return false;
	
	// Don't gib if we're temporal
	if (m_tEzVariant == EZ_VARIANT_TEMPORAL)
		return false;

	// If the damage type is "always gib", we better gib!
	if ( info.GetDamageType() & DMG_ALWAYSGIB )
		return true;

	if ( info.GetDamageType() & ( DMG_NEVERGIB | DMG_DISSOLVE ) )
		return false;

	if ( ( info.GetDamageType() & ( DMG_BLAST | DMG_ACID | DMG_POISON | DMG_CRUSH ) ) && ( GetAbsOrigin() - info.GetDamagePosition() ).LengthSqr() <= 32.0f )
		return true;

	return false;
}

//------------------------------------------------------------------------------
// Purpose: Override to do rebel specific gibs
// Output :
//------------------------------------------------------------------------------
bool CNPC_Combine::CorpseGib( const CTakeDamageInfo &info )
{
	return BaseClass::CorpseGib( info );
}
#endif

//-----------------------------------------------------------------------------
// Create components
//-----------------------------------------------------------------------------
bool CNPC_Combine::CreateComponents()
{
	if ( !BaseClass::CreateComponents() )
		return false;

#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	m_Sentences.Init( this, "NPC_Combine.SentenceParameters" );
#endif
	return true;
}


//------------------------------------------------------------------------------
// Purpose: Don't look, only get info from squad.
//------------------------------------------------------------------------------
void CNPC_Combine::InputLookOff( inputdata_t &inputdata )
{
	m_spawnflags |= SF_COMBINE_NO_LOOK;
}

//------------------------------------------------------------------------------
// Purpose: Enable looking.
//------------------------------------------------------------------------------
void CNPC_Combine::InputLookOn( inputdata_t &inputdata )
{
	m_spawnflags &= ~SF_COMBINE_NO_LOOK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputStartPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputStopPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputAssault( inputdata_t &inputdata )
{
	m_AssaultBehavior.SetParameters( AllocPooledString(inputdata.value.String()), CUE_DONT_WAIT, RALLY_POINT_SELECT_DEFAULT );
}

//-----------------------------------------------------------------------------
// We were hit by bugbait
//-----------------------------------------------------------------------------
void CNPC_Combine::InputHitByBugbait( inputdata_t &inputdata )
{
#ifdef EZ
	// Ignore if the activator likes us
	if (inputdata.pActivator && inputdata.pActivator->IsCombatCharacter() &&
		inputdata.pActivator->MyCombatCharacterPointer()->IRelationType( this ) == D_LI)
		return;
#endif

	SetCondition( COND_COMBINE_HIT_BY_BUGBAIT );
}

#ifndef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Force the combine soldier to throw a grenade at the target
//			If I'm a combine elite, fire my combine ball at the target instead.
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputThrowGrenadeAtTarget( inputdata_t &inputdata )
{
	// Ignore if we're inside a scripted sequence
	if ( m_NPCState == NPC_STATE_SCRIPT && m_hCine )
		return;

#ifdef MAPBASE
	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
#else
	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, inputdata.value.String(), NULL, inputdata.pActivator, inputdata.pCaller );
#endif
	if ( !pEntity )
	{
		DevMsg("%s (%s) received ThrowGrenadeAtTarget input, but couldn't find target entity '%s'\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	m_hForcedGrenadeTarget = pEntity;
	m_flNextGrenadeCheck = 0;

	ClearSchedule( "Told to throw grenade via input" );
}
#endif

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Instant transformation of arsenal from grenades to energy balls, or vice versa
//-----------------------------------------------------------------------------
void CNPC_Combine::InputSetElite( inputdata_t &inputdata )
{
	m_fIsElite = inputdata.value.Bool();
}

//-----------------------------------------------------------------------------
// We were told to drop a grenade
//-----------------------------------------------------------------------------
void CNPC_Combine::InputDropGrenade( inputdata_t &inputdata )
{
	SetCondition( COND_COMBINE_DROP_GRENADE );

	ClearSchedule( "Told to drop grenade via input" );
}

//-----------------------------------------------------------------------------
// Changes our tactical variant easily
//-----------------------------------------------------------------------------
void CNPC_Combine::InputSetTacticalVariant( inputdata_t &inputdata )
{
	m_iTacticalVariant = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputSetPoliceGoal( inputdata_t &inputdata )
{
	if (/*!inputdata.value.String() ||*/ inputdata.value.String()[0] == 0)
	{
		m_PolicingBehavior.Disable();
		return;
	}

	CBaseEntity *pGoal = gEntList.FindEntityByName( NULL, inputdata.value.String() );

	if ( pGoal == NULL )
	{
		DevMsg( "SetPoliceGoal: %s (%s) unable to find ai_goal_police: %s\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	CAI_PoliceGoal *pPoliceGoal = dynamic_cast<CAI_PoliceGoal *>(pGoal);

	if ( pPoliceGoal == NULL )
	{
		DevMsg( "SetPoliceGoal: %s (%s)'s target %s is not an ai_goal_police entity!\n", GetClassname(), GetDebugName(), inputdata.value.String() );
		return;
	}

	m_PolicingBehavior.Enable( pPoliceGoal );
}
#endif

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: Turn this Combine soldier into a player commandable soldier
// 1upD
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputSetCommandable(inputdata_t & inputdata)
{
	this->AddSpawnFlags(SF_COMBINE_COMMANDABLE);
}

//-----------------------------------------------------------------------------
// Purpose: Turn this Combine soldier into a non-commandable soldier
// 1upD
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputSetNonCommandable(inputdata_t & inputdata)
{
	this->RemoveSpawnFlags(SF_COMBINE_COMMANDABLE);
	RemoveFromPlayerSquad();
}

//-----------------------------------------------------------------------------
// Purpose: Enables manhack toss
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputEnableManhackToss( inputdata_t &inputdata )
{
	if ( HasSpawnFlags( SF_COMBINE_NO_MANHACK_DEPLOY ) )
	{
		RemoveSpawnFlags( SF_COMBINE_NO_MANHACK_DEPLOY );
	}
}

void CNPC_Combine::InputDisableManhackToss( inputdata_t &inputdata )
{
	if ( !HasSpawnFlags( SF_COMBINE_NO_MANHACK_DEPLOY ) )
	{
		AddSpawnFlags( SF_COMBINE_NO_MANHACK_DEPLOY );
	}
}

void CNPC_Combine::InputDeployManhack( inputdata_t &inputdata )
{
	// I am aware this bypasses regular deployment conditions, but the mapper wants us to deploy a manhack, damn it!
	// We do have to have one, though.
	if ( m_iManhacks > 0 )
	{
		SetSchedule(SCHED_COMBINE_DEPLOY_MANHACK);
	}
}

void CNPC_Combine::InputAddManhacks( inputdata_t &inputdata )
{
	m_iManhacks += inputdata.value.Int();

	SetBodygroup( COMBINE_BODYGROUP_MANHACK, (m_iManhacks > 0) );
}

void CNPC_Combine::InputSetManhacks( inputdata_t &inputdata )
{
	m_iManhacks = inputdata.value.Int();

	SetBodygroup( COMBINE_BODYGROUP_MANHACK, (m_iManhacks > 0) );
}

//-----------------------------------------------------------------------------
// Purpose: Disables player use
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputDisablePlayerUse( inputdata_t &inputdata )
{
	m_bDisablePlayerUse = true;
}

void CNPC_Combine::InputEnablePlayerUse( inputdata_t &inputdata )
{
	m_bDisablePlayerUse = false;
}

//-----------------------------------------------------------------------------
// Purpose: Toggles soldier surrendering
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputEnableOrderSurrender( inputdata_t &inputdata )
{
	m_iCanOrderSurrender = TRS_TRUE;
}

void CNPC_Combine::InputDisableOrderSurrender( inputdata_t &inputdata )
{
	m_iCanOrderSurrender = TRS_FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Toggles soldier giving
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Combine::InputEnablePlayerGive( inputdata_t &inputdata )
{
	m_iCanPlayerGive = TRS_TRUE;
}

void CNPC_Combine::InputDisablePlayerGive( inputdata_t &inputdata )
{
	m_iCanPlayerGive = TRS_FALSE;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Combine::Precache()
{
	PrecacheModel("models/Weapons/w_grenade.mdl");
	UTIL_PrecacheOther( "npc_handgrenade" );

	PrecacheScriptSound( "NPC_Combine.GrenadeLaunch" );
	PrecacheScriptSound( "NPC_Combine.WeaponBash" );
#ifndef MAPBASE // Now that we use WeaponSound(SPECIAL1), this isn't necessary
	PrecacheScriptSound( "Weapon_CombineGuard.Special1" );
#endif
#ifdef EZ
	PrecacheScriptSound( "NPC_Combine.Following" );
	PrecacheScriptSound( "NPC_Combine.StopFollowing" );
#endif
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Activate()
{
#ifndef MAPBASE
	s_iszShotgunClassname = FindPooledString( "weapon_shotgun" );
#endif
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Combine::Spawn( void )
{
#ifdef EZ
	// Use CNPC_PlayerCompanion::Spawn
	BaseClass::Spawn();

	m_flFieldOfView = -0.2;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	CapabilitiesRemove( bits_CAP_FRIENDLY_DMG_IMMUNE );	// Soldiers have their own friendly fire handling
#else
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );
	m_flFieldOfView			= -0.2;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState				= NPC_STATE_NONE;
#endif
	m_flNextGrenadeCheck	= gpGlobals->curtime + 1;
	m_flNextPainSoundTime	= 0;
	m_flNextAlertSoundTime	= 0;
	m_bShouldPatrol			= false;

	//	CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_MOVE_CLIMB);
	// JAY: Disabled jump for now - hard to compare to HL1
	CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_MOVE_GROUND );

#ifdef EZ2
	if ( IsMajorCharacter() ) // Non-commandable combine soldiers in Chapter 1 should not be able to jump!
		CapabilitiesAdd(bits_CAP_MOVE_JUMP);
#endif

	CapabilitiesAdd( bits_CAP_AIM_GUN );

	// Innate range attack for grenade
	// CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2 );

	// Innate range attack for kicking
	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1 );

	// Can be in a squad
	CapabilitiesAdd( bits_CAP_SQUAD);
	CapabilitiesAdd( bits_CAP_USE_WEAPONS );

	CapabilitiesAdd( bits_CAP_DUCK );				// In reloading and cover

	CapabilitiesAdd( bits_CAP_NO_HIT_SQUADMATES );

	m_bFirstEncounter	= true;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector ( 0, 0, 55 );

	m_flStopMoveShootTime = FLT_MAX; // Move and shoot defaults on.
	m_MoveAndShootOverlay.SetInitialDelay( 0.75 ); // But with a bit of a delay.

	m_flNextLostSoundTime		= 0;
	m_flAlertPatrolTime			= 0;

	m_flNextAltFireTime = gpGlobals->curtime;

	NPCInit();

#ifdef EZ
	// Start us with a visible manhack if we have one
	if ( m_iManhacks )
	{
		SetBodygroup( COMBINE_BODYGROUP_MANHACK, true );
	}
#endif

#ifdef MAPBASE
	// This was moved from CalcWeaponProficiency() so soldiers don't change skin unnaturally and uncontrollably
	if ( GetActiveWeapon() && EntIsClass(GetActiveWeapon(), gm_isz_class_Shotgun) && m_nSkin != COMBINE_SKIN_SHOTGUNNER )
	{
		m_nSkin = COMBINE_SKIN_SHOTGUNNER;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Combine::CreateBehaviors()
{
#ifdef EZ // Blixibon - We inherit most of our behaviors from CNPC_PlayerCompanion now.
	AddBehavior( &m_RappelBehavior );
	AddBehavior( &m_StandoffBehavior );
	AddBehavior( &m_FollowBehavior );
	AddBehavior( &m_FuncTankBehavior );
#else
	AddBehavior( &m_RappelBehavior );
	AddBehavior( &m_ActBusyBehavior );
	AddBehavior( &m_AssaultBehavior );
	AddBehavior( &m_StandoffBehavior );
	AddBehavior( &m_FollowBehavior );
	AddBehavior( &m_FuncTankBehavior );
#endif
#ifdef MAPBASE
	AddBehavior( &m_PolicingBehavior );
#endif

	return BaseClass::CreateBehaviors();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::PostNPCInit()
{
#ifndef MAPBASE
	if( IsElite() )
	{
		// Give a warning if a Combine Soldier is equipped with anything other than
		// an AR2. 
		if( !GetActiveWeapon() || !FClassnameIs( GetActiveWeapon(), "weapon_ar2" ) )
		{
			DevWarning("**Combine Elite Soldier MUST be equipped with AR2\n");
		}
	}
#endif

#ifdef EZ
	// Originally added by 1upD, modified by Blixibon and moved to PostNPCInit() - Save restore squad fixup
	if (IsInPlayerSquad())
	{
		FixupPlayerSquad();
	}
#endif

	BaseClass::PostNPCInit();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_COMBINE_ATTACK_SLOT_AVAILABLE );

	if( GetState() == NPC_STATE_COMBAT )
	{
#ifdef MAPBASE
		// Don't override the standoff
		if( IsCurSchedule( SCHED_COMBINE_WAIT_IN_COVER, false ) && !m_StandoffBehavior.IsActive() )
#else
		if( IsCurSchedule( SCHED_COMBINE_WAIT_IN_COVER, false ) )
#endif
		{
			// Soldiers that are standing around doing nothing poll for attack slots so
			// that they can respond quickly when one comes available. If they can 
			// occupy a vacant attack slot, they do so. This holds the slot until their
			// schedule breaks and schedule selection runs again, essentially reserving this
			// slot. If they do not select an attack schedule, then they'll release the slot.
			if( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				SetCondition( COND_COMBINE_ATTACK_SLOT_AVAILABLE );
			}
		}

		if( IsUsingTacticalVariant(TACTICAL_VARIANT_PRESSURE_ENEMY_UNTIL_CLOSE) )
		{
			if( GetEnemy() != NULL && !HasCondition(COND_ENEMY_OCCLUDED) )
			{
				// Now we're close to our enemy, stop using the tactical variant.
				if( GetAbsOrigin().DistToSqr(GetEnemy()->GetAbsOrigin()) < Square(30.0f * 12.0f) )
					m_iTacticalVariant = TACTICAL_VARIANT_DEFAULT;
			}
		}
	}

#ifdef EZ
	if( ShouldLookForHealthItem() )
	{
		if( FindHealthItem( GetAbsOrigin(), Vector( 240, 240, 240 ) ) )
			SetCondition( COND_HEALTH_ITEM_AVAILABLE );
		else
			ClearCondition( COND_HEALTH_ITEM_AVAILABLE );

		m_flNextHealthSearchTime = gpGlobals->curtime + 4.0;
	}

	if ( HasCondition( COND_TALKER_PLAYER_STARING ) && ShouldAllowPlayerGive() && !IsInAScript() && IsInterruptable() && npc_combine_give_enabled.GetBool() )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		if (pPlayer /*&& (!m_pSquad || m_pSquad->GetSquadMemberNearestTo( pPlayer->GetAbsOrigin() ) == this)*/)
		{
			bool bStareRelevant = false;

			float flDistSqr = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length2DSqr();
			float flStareDist = npc_combine_give_stare_dist.GetFloat();

			// Be extra certain that they're staring
			Vector facingDir = pPlayer->EyeDirection2D();
			Vector los = ( EyePosition() - pPlayer->EyePosition() );
			los.z = 0;
			VectorNormalize( los );

			if( !IsMoving() && pPlayer->IsAlive() && (flDistSqr <= flStareDist * flStareDist) && DotProduct( los, facingDir ) > 0.8f && pPlayer->FVisible( this ) )
			{
				// If the player is holding an item in front of us, they might want us to pick it up.
				CBaseEntity *pHeld = GetPlayerHeldEntity( pPlayer );

				if (pHeld)
				{
					bStareRelevant = true;

					if ( gpGlobals->curtime - m_flTimePlayerStare >= npc_combine_give_stare_time.GetFloat() &&
						!IsCurSchedule(SCHED_NEW_WEAPON) && !IsCurSchedule(SCHED_GET_HEALTHKIT) )
					{
						bool bAccepted = false;

						// Is it a weapon?
						if (pHeld->IsBaseCombatWeapon() && Weapon_CanUse( pHeld->MyCombatWeaponPointer() ) && IsGiveableWeapon( pHeld->MyCombatWeaponPointer() ))
						{
							//SetCondition( COND_BETTER_WEAPON_AVAILABLE );

							CBaseCombatWeapon *pWeapon = pHeld->MyCombatWeaponPointer();
							if (!pWeapon->IsLocked( this ))
							{
								Msg( "%s picking up %s held by player\n", GetDebugName(), pHeld->GetDebugName() );

								// Now lock the weapon for a few seconds while we go to pick it up.
								m_flNextWeaponSearchTime = gpGlobals->curtime + 10.0;
								pWeapon->Lock( 5.0, this );
								SetTarget( pWeapon );
								SetSchedule( SCHED_NEW_WEAPON );
								StartPlayerGive( pPlayer );
								bAccepted = true;
							}
						}

						// Is it an item?
						else if (IsGiveableItem(pHeld))
						{
							//SetCondition( COND_HEALTH_ITEM_AVAILABLE );

							// See CAI_BaseNPC::PickupItem for items soldiers can absorb
							SetTarget( pHeld );
							SetSchedule( SCHED_GET_HEALTHKIT );
							StartPlayerGive( pPlayer );
							bAccepted = true;
						}

						if (!bAccepted)
							OnCantBeGivenObject( pHeld );
					}
				}
			}

			if (!bStareRelevant)
			{
				m_flTimePlayerStare = FLT_MAX;
			}
			else if (m_flTimePlayerStare == FLT_MAX)
			{
				// Player wasn't looking at me at last think. They started staring now.
				m_flTimePlayerStare = gpGlobals->curtime;
			}
		}
	}
#endif

#ifdef EZ2
	ClearCondition( COND_COMBINE_CAN_ORDER_SURRENDER );

	if (HasCondition(COND_CAN_RANGE_ATTACK1))
	{
		if (CanOrderSurrender())
		{
			if ( GetEnemy() /*&& GetEnemies()->NumEnemies() <= 2*/ && GetEnemy()->ClassMatches("npc_citizen") && GetEnemy()->MyNPCPointer()->GetActiveWeapon() == NULL )
			{
				CNPC_Citizen *pCitizen = static_cast<CNPC_Citizen*>(GetEnemy());
				if (pCitizen && pCitizen->CanSurrender())
				{
					// Make sure they're not about to pick up a weapon
					if (!pCitizen->IsCurSchedule( SCHED_NEW_WEAPON, false ))
					{
						// Finally, check if the citizen has spoken beg or surrender concepts already
						float flTimeSpeakSurrender = pCitizen->GetTimeSpokeConcept( TLK_SURRENDER );
						float flTimeSpeakBeg = pCitizen->GetTimeSpokeConcept( TLK_BEG );
						if ((flTimeSpeakSurrender != -1 && gpGlobals->curtime - flTimeSpeakSurrender >= npc_combine_order_surrender_min_tlk_surrender.GetFloat()) || (flTimeSpeakBeg != -1 && gpGlobals->curtime - flTimeSpeakBeg >= npc_combine_order_surrender_min_tlk_beg.GetFloat()))
						{
							SetCondition( COND_COMBINE_CAN_ORDER_SURRENDER );

							// Don't attack while ordering to surrender
							ClearCondition( COND_CAN_RANGE_ATTACK1 );
							ClearCondition( COND_CAN_RANGE_ATTACK2 );
							ClearCondition( COND_CAN_MELEE_ATTACK1 );
						}
					}
				}
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

#ifdef EZ
	// 1upD - Copied from citizen
	UpdateFollowCommandPoint();


	// Update glow if we are commandable
	if ( ShouldSquadGlow() && ( !m_bGlowEnabled.Get() || ( GetHealth() < GetMaxHealth() && ShouldRegenerateHealth() ) ) )
	{
		UpdateSquadGlow();
	}
#endif

	// Speak any queued sentences
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	m_Sentences.UpdateSentenceQueue();
#endif

	if ( IsOnFire() )
	{
		SetCondition( COND_COMBINE_ON_FIRE );
	}
	else
	{
		ClearCondition( COND_COMBINE_ON_FIRE );
	}

	extern ConVar ai_debug_shoot_positions;
	if ( ai_debug_shoot_positions.GetBool() )
		NDebugOverlay::Cross3D( EyePosition(), 16, 0, 255, 0, false, 0.1 );

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
}

#ifdef EZ
extern int ACT_ANTLION_FLIP;
extern int ACT_ANTLION_ZAP_FLIP;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Disposition_t CNPC_Combine::IRelationType( CBaseEntity *pTarget )
{
	Disposition_t disposition = BaseClass::IRelationType( pTarget );

	if ( pTarget == NULL )
		return disposition;

	if (npc_combine_fear_near_zombies.GetBool())
	{
		// Zombie fearing behavior based partially on Alyx's code
		// -Blixibon
		if( pTarget->Classify() == CLASS_ZOMBIE && disposition == D_HT )
		{
			if( GetEnemies()->NumEnemies() > 1 && GetAbsOrigin().DistToSqr(pTarget->GetAbsOrigin()) < Square(96) )
			{
				// Be afraid of a zombie that's near if he's not alone. This will make soldiers back away.
				return D_FR;
			}
		}
	}

	return disposition;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Combine::IRelationPriority( CBaseEntity *pTarget )
{
	int priority = BaseClass::IRelationPriority( pTarget );

	// Alyx's antlion priority behavior that prioritizes flipped antlions
	// -Blixibon
	if( pTarget->Classify() == CLASS_ANTLION )
	{
		// Prefer Antlions that are flipped onto their backs.
		// UNLESS we have a different enemy that could melee attack while our back is turned.
		CAI_BaseNPC *pNPC = pTarget->MyNPCPointer();
		if ( pNPC && ( pNPC->GetActivity() == ACT_ANTLION_FLIP || pNPC->GetActivity() == ACT_ANTLION_ZAP_FLIP  ) )
		{
			if( GetEnemy() && GetEnemy() != pTarget )
			{
				// I have an enemy that is not this thing. If that enemy is near, I shouldn't
				// become distracted.
				if( GetAbsOrigin().DistToSqr(GetEnemy()->GetAbsOrigin()) < Square(180) )
				{
					return priority;
				}
			}

			priority += 1;
		}
	}

	CAI_BaseNPC *pNPC = pTarget->MyNPCPointer();
	if (pNPC && pNPC->GetActiveWeapon() == NULL && pNPC->ClassMatches( "npc_citizen" ) &&
		(!pNPC->GetRunningBehavior() || pNPC->GetRunningBehavior()->GetName() != m_FuncTankBehavior.GetName()))
	{
		// Unarmed citizens have less priority
		priority -= 2;
	}

	return priority;
}
#endif

#ifndef MAPBASE
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::DelayAltFireAttack( float flDelay )
{
	float flNextAltFire = gpGlobals->curtime + flDelay;

	if( flNextAltFire > m_flNextAltFireTime )
	{
		// Don't let this delay order preempt a previous request to wait longer.
		m_flNextAltFireTime = flNextAltFire;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::DelaySquadAltFireAttack( float flDelay )
{
	// Make sure to delay my own alt-fire attack.
	DelayAltFireAttack( flDelay );

	AISquadIter_t iter;
	CAI_BaseNPC *pSquadmate = m_pSquad ? m_pSquad->GetFirstMember( &iter ) : NULL;
	while ( pSquadmate )
	{
		CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine*>(pSquadmate);

#ifdef MAPBASE
		if( pCombine && pCombine->IsAltFireCapable() )
#else
		if( pCombine && pCombine->IsElite() )
#endif
		{
			pCombine->DelayAltFireAttack( flDelay );
		}

		pSquadmate = m_pSquad->GetNextMember( &iter );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: degrees to turn in 0.1 seconds
//-----------------------------------------------------------------------------
float CNPC_Combine::MaxYawSpeed( void )
{
	switch( GetActivity() )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 45;
		break;
	case ACT_RUN:
	case ACT_RUN_HURT:
		return 15;
		break;
	case ACT_WALK:
	case ACT_WALK_CROUCH:
		return 25;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		return 35;
	default:
		return 35;
		break;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldMoveAndShoot()
{
	// Set this timer so that gpGlobals->curtime can't catch up to it. 
	// Essentially, we're saying that we're not going to interfere with 
	// what the AI wants to do with move and shoot. 
	//
	// If any code below changes this timer, the code is saying 
	// "It's OK to move and shoot until gpGlobals->curtime == m_flStopMoveShootTime"
	m_flStopMoveShootTime = FLT_MAX;

#ifdef EZ
	// 1upD - Commandable Combine should always move and shoot
	if ( IsMajorCharacter() && !HasCondition(COND_NO_PRIMARY_AMMO, false ) )
		return true;
#endif

	if( IsCurSchedule( SCHED_COMBINE_HIDE_AND_RELOAD, false ) )
		m_flStopMoveShootTime = gpGlobals->curtime + random->RandomFloat( 0.4f, 0.6f );

	if( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND, false ) )
		return false;

	if( IsCurSchedule( SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND, false ) )
		return false;

	if( IsCurSchedule( SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND, false ) )
		return false;

	if( HasCondition( COND_NO_PRIMARY_AMMO, false ) )
		m_flStopMoveShootTime = gpGlobals->curtime + random->RandomFloat( 0.4f, 0.6f );

	if( m_pSquad && IsCurSchedule( SCHED_COMBINE_TAKE_COVER1, false ) )
		m_flStopMoveShootTime = gpGlobals->curtime + random->RandomFloat( 0.4f, 0.6f );

	return BaseClass::ShouldMoveAndShoot();
}

//-----------------------------------------------------------------------------
// Purpose: turn in the direction of movement
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Combine::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
	return BaseClass::OverrideMoveFacing( move, flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
Class_T	CNPC_Combine::Classify ( void )
{
	return CLASS_COMBINE;
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Function for gauging whether we're capable of alt-firing.
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsAltFireCapable( void )
{
	// The base class tells us if we're carrying an alt-fire-able weapon.
#ifdef EZ
	// HACKHACK: Skip CNPC_PlayerCompanion's grenade capabilities.
	return (IsElite() || m_bAlternateCapable) && CAI_GrenadeUser<CAI_PlayerAlly>::IsAltFireCapable();
#else
	return (IsElite() || m_bAlternateCapable) && BaseClass::IsAltFireCapable();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Function for gauging whether we're capable of throwing grenades.
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsGrenadeCapable( void )
{
	return !IsElite() || m_bAlternateCapable;
}
#endif


//-----------------------------------------------------------------------------
// Continuous movement tasks
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsCurTaskContinuousMove()
{
	const Task_t* pTask = GetTask();
	if ( pTask && (pTask->iTask == TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY) )
		return true;

	return BaseClass::IsCurTaskContinuousMove();
}


//-----------------------------------------------------------------------------
// Chase the enemy, updating the target position as the player moves
//-----------------------------------------------------------------------------
void CNPC_Combine::StartTaskChaseEnemyContinuously( const Task_t *pTask )
{
	CBaseEntity *pEnemy = GetEnemy();
	if ( !pEnemy )
	{
		TaskFail( FAIL_NO_ENEMY );
		return;
	}

	// We're done once we get close enough
	if ( WorldSpaceCenter().DistToSqr( pEnemy->WorldSpaceCenter() ) <= pTask->flTaskData * pTask->flTaskData )
	{
		TaskComplete();
		return;
	}

	// TASK_GET_PATH_TO_ENEMY
	if ( IsUnreachable( pEnemy ) )
	{
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	if ( !GetNavigator()->SetGoal( GOALTYPE_ENEMY, AIN_NO_PATH_TASK_FAIL ) )
	{
		// no way to get there =( 
		DevWarning( 2, "GetPathToEnemy failed!!\n" );
		RememberUnreachable( pEnemy );
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	// NOTE: This is TaskRunPath here.
	if ( TranslateActivity( ACT_RUN ) != ACT_INVALID )
	{
		GetNavigator()->SetMovementActivity( ACT_RUN );
	}
	else
	{
		GetNavigator()->SetMovementActivity(ACT_WALK);
	}

	// Cover is void once I move
	Forget( bits_MEMORY_INCOVER );

	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
	{
		TaskComplete();
		GetNavigator()->ClearGoal();		// Clear residual state
		return;
	}

	// No shooting delay when in this mode
	m_MoveAndShootOverlay.SetInitialDelay( 0.0 );

	if (!GetNavigator()->IsGoalActive())
	{
		SetIdealActivity( GetStoppedActivity() );
	}
	else
	{
		// Check validity of goal type
		ValidateNavGoal();
	}

	// set that we're probably going to stop before the goal
	GetNavigator()->SetArrivalDistance( pTask->flTaskData );
	m_vSavePosition = GetEnemy()->WorldSpaceCenter();
}

void CNPC_Combine::RunTaskChaseEnemyContinuously( const Task_t *pTask )
{
	if (!GetNavigator()->IsGoalActive())
	{
		SetIdealActivity( GetStoppedActivity() );
	}
	else
	{
		// Check validity of goal type
		ValidateNavGoal();
	}

	CBaseEntity *pEnemy = GetEnemy();
	if ( !pEnemy )
	{
		TaskFail( FAIL_NO_ENEMY );
		return;
	}

	// We're done once we get close enough
	if ( WorldSpaceCenter().DistToSqr( pEnemy->WorldSpaceCenter() ) <= pTask->flTaskData * pTask->flTaskData )
	{
		GetNavigator()->StopMoving();
		TaskComplete();
		return;
	}

	// Recompute path if the enemy has moved too much
	if ( m_vSavePosition.DistToSqr( pEnemy->WorldSpaceCenter() ) < (pTask->flTaskData * pTask->flTaskData) )
		return;

	if ( IsUnreachable( pEnemy ) )
	{
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	if ( !GetNavigator()->RefindPathToGoal() )
	{
		TaskFail(FAIL_NO_ROUTE);
		return;
	}

	m_vSavePosition = pEnemy->WorldSpaceCenter();
}

//=========================================================
// start task
//=========================================================
void CNPC_Combine::StartTask( const Task_t *pTask )
{
	// NOTE: This reset is required because we change it in TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY
	m_MoveAndShootOverlay.SetInitialDelay( 0.75 );

	switch ( pTask->iTask )
	{
	case TASK_COMBINE_SET_STANDING:
		{
			if ( pTask->flTaskData == 1.0f)
			{
				Stand();
			}
			else
			{
				Crouch();
			}
			TaskComplete();
		}
		break;

	case TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY:
		StartTaskChaseEnemyContinuously( pTask );
		break;

	case TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET:
		SetIdealActivity( (Activity)(int)pTask->flTaskData );
		GetMotor()->SetIdealYawToTargetAndUpdate( m_vecAltFireTarget, AI_KEEP_YAW_SPEED );
		break;

	case TASK_COMBINE_SIGNAL_BEST_SOUND:
		if( IsInSquad() && GetSquad()->NumMembers() > 1 )
		{
			CBasePlayer *pPlayer = AI_GetSinglePlayer();

			if( pPlayer && OccupyStrategySlot( SQUAD_SLOT_EXCLUSIVE_HANDSIGN ) && pPlayer->FInViewCone( this ) )
			{
				CSound *pSound;
				pSound = GetBestSound();

				Assert( pSound != NULL );

				if ( pSound )
				{
					Vector right, tosound;

					GetVectors( NULL, &right, NULL );

					tosound = pSound->GetSoundReactOrigin() - GetAbsOrigin();
					VectorNormalize( tosound);

					tosound.z = 0;
					right.z = 0;

#ifdef EZ
					if ( DotProduct( right, tosound ) > 0 )
					{
						// Right
						AddGesture( ACT_GESTURE_SIGNAL_RIGHT );
					}
					else
					{
						// Left
						AddGesture( ACT_GESTURE_SIGNAL_LEFT );
					}
#else
					if( DotProduct( right, tosound ) > 0 )
					{
						// Right
						SetIdealActivity( ACT_SIGNAL_RIGHT );
					}
					else
					{
						// Left
						SetIdealActivity( ACT_SIGNAL_LEFT );
					}
#endif

					break;
				}
			}
		}

		// Otherwise, just skip it.
		TaskComplete();
		break;

	case TASK_ANNOUNCE_ATTACK:
		{
			// If Primary Attack
			if ((int)pTask->flTaskData == 1)
			{
				// -----------------------------------------------------------
				// If enemy isn't facing me and I haven't attacked in a while
				// annouce my attack before I start wailing away
				// -----------------------------------------------------------
				CBaseCombatCharacter *pBCC = GetEnemyCombatCharacterPointer();

				if	(pBCC && pBCC->IsPlayer() && (!pBCC->FInViewCone ( this )) &&
					(gpGlobals->curtime - m_flLastAttackTime > 3.0) )
				{
					m_flLastAttackTime = gpGlobals->curtime;

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
					SpeakIfAllowed( TLK_CMB_ANNOUNCE, SENTENCE_PRIORITY_HIGH );
#else
					m_Sentences.Speak( "COMBINE_ANNOUNCE", SENTENCE_PRIORITY_HIGH );
#endif

					// Wait two seconds
					SetWait( 2.0 );

					if ( !IsCrouching() )
					{
						SetActivity(ACT_IDLE);
					}
					else
					{
						SetActivity(ACT_COWER); // This is really crouch idle
					}
				}
				// -------------------------------------------------------------
				//  Otherwise move on
				// -------------------------------------------------------------
				else
				{
					TaskComplete();
				}
			}
			else
			{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
				SpeakIfAllowed( TLK_CMB_THROWGRENADE, SENTENCE_PRIORITY_MEDIUM );
#else
				m_Sentences.Speak( "COMBINE_THROW_GRENADE", SENTENCE_PRIORITY_MEDIUM );
#endif
				SetActivity(ACT_IDLE);

				// Wait two seconds
				SetWait( 2.0 );
			}
			break;
		}	

	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		BaseClass::StartTask( pTask );
		break;

	case TASK_COMBINE_FACE_TOSS_DIR:
		break;

	case TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS:
#ifdef MAPBASE
		StartTask_GetPathToForced(pTask);
#else
		{
			if ( !m_hForcedGrenadeTarget )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}

			float flMaxRange = 2000;
			float flMinRange = 0;

			Vector vecEnemy = m_hForcedGrenadeTarget->GetAbsOrigin();
			Vector vecEnemyEye = vecEnemy + m_hForcedGrenadeTarget->GetViewOffset();

			Vector posLos;
			bool found = false;

			if ( GetTacticalServices()->FindLateralLos( vecEnemyEye, &posLos ) )
			{
				float dist = ( posLos - vecEnemyEye ).Length();
				if ( dist < flMaxRange && dist > flMinRange )
					found = true;
			}

			if ( !found && GetTacticalServices()->FindLos( vecEnemy, vecEnemyEye, flMinRange, flMaxRange, 1.0, &posLos ) )
			{
				found = true;
			}

			if ( !found )
			{
				TaskFail( FAIL_NO_SHOOT );
			}
			else
			{
				// else drop into run task to offer an interrupt
				m_vInterruptSavePosition = posLos;
			}
		}
#endif
		break;

	case TASK_COMBINE_IGNORE_ATTACKS:
		// must be in a squad
		if (m_pSquad && m_pSquad->NumMembers() > 2)
		{
			// the enemy must be far enough away
			if (GetEnemy() && (GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter()).Length() > 512.0 )
			{
				m_flNextAttack	= gpGlobals->curtime + pTask->flTaskData;
			}
		}
		TaskComplete( );
		break;

	case TASK_COMBINE_DEFER_SQUAD_GRENADES:
		{
#ifdef MAPBASE
			StartTask_DeferSquad(pTask);
#else
			if ( m_pSquad )
			{
				// iterate my squad and stop everyone from throwing grenades for a little while.
				AISquadIter_t iter;

				CAI_BaseNPC *pSquadmate = m_pSquad ? m_pSquad->GetFirstMember( &iter ) : NULL;
				while ( pSquadmate )
				{
#ifdef MAPBASE
					pSquadmate->DelayGrenadeCheck(5);
#else
					CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine*>(pSquadmate);

					if( pCombine )
					{
						pCombine->m_flNextGrenadeCheck = gpGlobals->curtime + 5;
					}
#endif

					pSquadmate = m_pSquad->GetNextMember( &iter );
				}
			}

			TaskComplete();
#endif
			break;
		}

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		{
			if( pTask->iTask == TASK_FACE_ENEMY && HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				TaskComplete();
				return;
			}

			BaseClass::StartTask( pTask );
			bool bIsFlying = (GetMoveType() == MOVETYPE_FLY) || (GetMoveType() == MOVETYPE_FLYGRAVITY);
			if (bIsFlying)
			{
				SetIdealActivity( ACT_GLIDE );
			}

		}
		break;

	case TASK_FIND_COVER_FROM_ENEMY:
		{
			if (GetHintGroup() == NULL_STRING)
			{
				CBaseEntity *pEntity = GetEnemy();

				// FIXME: this should be generalized by the schedules that are selected, or in the definition of 
				// what "cover" means (i.e., trace attack vulnerability vs. physical attack vulnerability
				if ( pEntity )
				{
					// NOTE: This is a good time to check to see if the player is hurt.
					// Have the combine notice this and call out
					if ( !HasMemory(bits_MEMORY_PLAYER_HURT) && pEntity->IsPlayer() && pEntity->GetHealth() <= 20 )
					{
						if ( m_pSquad )
						{
							m_pSquad->SquadRemember(bits_MEMORY_PLAYER_HURT);
						}

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
						SpeakIfAllowed( TLK_CMB_PLAYERHIT, SENTENCE_PRIORITY_INVALID );
#else
						m_Sentences.Speak( "COMBINE_PLAYERHIT", SENTENCE_PRIORITY_INVALID );
#endif
						JustMadeSound( SENTENCE_PRIORITY_HIGH );
					}
					if ( pEntity->MyNPCPointer() )
					{
						if ( !(pEntity->MyNPCPointer()->CapabilitiesGet( ) & bits_CAP_WEAPON_RANGE_ATTACK1) && 
							!(pEntity->MyNPCPointer()->CapabilitiesGet( ) & bits_CAP_INNATE_RANGE_ATTACK1) )
						{
							TaskComplete();
							return;
						}
					}
				}
			}
			BaseClass::StartTask( pTask );
		}
		break;
	case TASK_RANGE_ATTACK1:
		{
#ifdef MAPBASE
			// The game can crash if a soldier's weapon is removed while they're shooting
			if (!GetActiveWeapon())
			{
				TaskFail( "No weapon" );
				break;
			}
#endif

			m_nShots = GetActiveWeapon()->GetRandomBurst();
			m_flShotDelay = GetActiveWeapon()->GetFireRate();

			m_flNextAttack = gpGlobals->curtime + m_flShotDelay - 0.1;
			ResetIdealActivity( ACT_RANGE_ATTACK1 );
			m_flLastAttackTime = gpGlobals->curtime;
		}
		break;

	case TASK_COMBINE_DIE_INSTANTLY:
		{
			CTakeDamageInfo info;

			info.SetAttacker( this );
			info.SetInflictor( this );
			info.SetDamage( m_iHealth );
			info.SetDamageType( pTask->flTaskData );
			info.SetDamageForce( Vector( 0.1, 0.1, 0.1 ) );

			TakeDamage( info );

			TaskComplete();
		}
		break;

	default: 
		BaseClass:: StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_Combine::RunTask( const Task_t *pTask )
{
	/*
	{
	CBaseEntity *pEnemy = GetEnemy();
	if (pEnemy)
	{
	NDebugOverlay::Line(Center(), pEnemy->Center(), 0,255,255, false, 0.1);
	}

	}
	*/

	/*
	if (m_iMySquadSlot != SQUAD_SLOT_NONE)
	{
	char text[64];
	Q_snprintf( text, strlen( text ), "%d", m_iMySquadSlot );

	NDebugOverlay::Text( Center() + Vector( 0, 0, 72 ), text, false, 0.1 );
	}
	*/

	switch ( pTask->iTask )
	{
	case TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY:
		RunTaskChaseEnemyContinuously( pTask );
		break;

	case TASK_COMBINE_SIGNAL_BEST_SOUND:
		AutoMovement( );
#ifndef EZ
		if ( IsActivityFinished() )
#endif
		{
			TaskComplete();
		}
		break;

	case TASK_ANNOUNCE_ATTACK:
		{
			// Stop waiting if enemy facing me or lost enemy
			CBaseCombatCharacter* pBCC = GetEnemyCombatCharacterPointer();
			if	(!pBCC || pBCC->FInViewCone( this ))
			{
				TaskComplete();
			}

			if ( IsWaitFinished() )
			{
				TaskComplete();
			}
		}
		break;

#ifdef MAPBASE
	case TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET:
		RunTask_FaceAltFireTarget(pTask);
		break;

	case TASK_COMBINE_FACE_TOSS_DIR:
		RunTask_FaceTossDir(pTask);
		break;

	case TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS:
		RunTask_GetPathToForced(pTask);
		break;
#else
	case TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET:
		GetMotor()->SetIdealYawToTargetAndUpdate( m_vecAltFireTarget, AI_KEEP_YAW_SPEED );

		if ( IsActivityFinished() )
		{
			TaskComplete();
		}
		break;

	case TASK_COMBINE_FACE_TOSS_DIR:
		{
			// project a point along the toss vector and turn to face that point.
			GetMotor()->SetIdealYawToTargetAndUpdate( GetLocalOrigin() + m_vecTossVelocity * 64, AI_KEEP_YAW_SPEED );

			if ( FacingIdeal() )
			{
				TaskComplete( true );
			}
			break;
		}

	case TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS:
		{
			if ( !m_hForcedGrenadeTarget )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}

			if ( GetTaskInterrupt() > 0 )
			{
				ClearTaskInterrupt();

				Vector vecEnemy = m_hForcedGrenadeTarget->GetAbsOrigin();
				AI_NavGoal_t goal( m_vInterruptSavePosition, ACT_RUN, AIN_HULL_TOLERANCE );

				GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET );
				GetNavigator()->SetArrivalDirection( vecEnemy - goal.dest );
			}
			else
			{
				TaskInterrupt();
			}
		}
		break;
#endif

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
						// DevMsg("ACT_RANGE_ATTACK1\n");
						ResetIdealActivity( ACT_RANGE_ATTACK1 );
						m_flLastAttackTime = gpGlobals->curtime;
						m_flNextAttack = gpGlobals->curtime + m_flShotDelay - 0.1;
					}
					else
					{
						// DevMsg("TASK_RANGE_ATTACK1 complete\n");
						TaskComplete();
					}
				}
			}
			else
			{
				// DevMsg("Wait\n");
			}
		}
		break;

	default:
		{
			BaseClass::RunTask( pTask );
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Override to always shoot at eyes (for ducking behind things)
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CNPC_Combine::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{
	Vector result = BaseClass::BodyTarget( posSrc, bNoisy );

	// @TODO (toml 02-02-04): this seems wrong. Isn't this already be accounted for 
	// with the eye position used in the base BodyTarget()
	if ( GetFlags() & FL_DUCKING )
		result -= Vector(0,0,24);

	return result;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
bool CNPC_Combine::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	if( m_spawnflags & SF_COMBINE_NO_LOOK )
	{
		// When no look is set, if enemy has eluded the squad, 
		// he's always invisble to me
		if (GetEnemies()->HasEludedMe(pEntity))
		{
			return false;
		}
	}
	return BaseClass::FVisible(pEntity, traceMask, ppBlocker);
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::OnSeeEntity( CBaseEntity *pEntity )
{
	BaseClass::OnSeeEntity(pEntity);

	if ( pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer*>(pEntity);
		if ( pPlayer->GetUseEntity() && !HasMemory(bits_MEMORY_SAW_PLAYER_WEIRDNESS) && m_NPCState != NPC_STATE_COMBAT && !IsRunningScriptedSceneAndNotPaused(this, false) )
		{
			CBaseEntity *pUseEntity = GetPlayerHeldEntity( pPlayer );
			if (!pUseEntity)
				PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );

			if ( pUseEntity && FVisible(pUseEntity) )
			{
				AddLookTarget( pUseEntity, 1.0f, 5.0f, 1.0f );
				Remember( bits_MEMORY_SAW_PLAYER_WEIRDNESS );
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Event_Killed( const CTakeDamageInfo &info )
{
#ifdef EZ
	// Blixibon - Release the manhack if we're in the middle of deploying him
	if ( m_hManhack && m_hManhack->IsAlive() )
	{
		ReleaseManhack();
		m_hManhack = NULL;
	}
#endif

	// if I was killed before I could finish throwing my grenade, drop
	// a grenade item that the player can retrieve.
	if( GetActivity() == ACT_RANGE_ATTACK2 )
	{
		if( m_iLastAnimEventHandled != COMBINE_AE_GREN_TOSS )
		{
			// Drop the grenade as an item.
			Vector vecStart;
			GetAttachment( "lefthand", vecStart );

			CBaseEntity *pItem = DropItem( "weapon_frag", vecStart, RandomAngle(0,360) );

			if ( pItem )
			{
				IPhysicsObject *pObj = pItem->VPhysicsGetObject();

				if ( pObj )
				{
					Vector			vel;
					vel.x = random->RandomFloat( -100.0f, 100.0f );
					vel.y = random->RandomFloat( -100.0f, 100.0f );
					vel.z = random->RandomFloat( 800.0f, 1200.0f );
					AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

					vel[2] = 0.0f;
					pObj->AddVelocity( &vel, &angImp );
				}

				// In the Citadel we need to dissolve this
#ifdef MAPBASE
				if ( PlayerHasMegaPhysCannon() && GlobalEntity_GetCounter("super_phys_gun") != 1 )
#else
				if ( PlayerHasMegaPhysCannon() )
#endif
				{
					CBaseCombatWeapon *pWeapon = static_cast<CBaseCombatWeapon *>(pItem);

					pWeapon->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
				}
			}
		}
	}

	BaseClass::Event_Killed( info );
}

#ifdef EZ
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther(pVictim, info);

	// Alyx often brutally shoots the corpses of enemies she kills.
	// This would look cool on regular Combine soldiers, but it just doesn't work as well with them
	// due to the way they handle weapons, increased damage sending NPCs farther away, etc.
	// Instead of shooting corpses, soldiers merely aim at them instead.
	// This doesn't suffer from the same problems and makes the soldiers feel more natural.
	// -Blixibon
	if( GetEnemies()->NumEnemies() == 1 )
	{
		if (pVictim->GetAbsOrigin().DistTo(GetAbsOrigin()) > 48.0f &&
			pVictim->GetFlags() & FL_ONGROUND && pVictim->GetMoveType() == MOVETYPE_STEP)
		{
			CAI_BaseNPC *pTarget = CreateCustomTarget( pVictim->GetAbsOrigin(), 2.0f );
			SetAimTarget(pTarget);
		}
	}
}

ConVar npc_combine_friendly_fire("npc_combine_friendly_fire", "2");

//-----------------------------------------------------------------------------
// Purpose: Combine friendly fire prevention prototype -Blixibon
//-----------------------------------------------------------------------------
bool CNPC_Combine::PassesDamageFilter( const CTakeDamageInfo &info )
{
	// 0 = No friendly fire prevention
	// 1 = NPCs that like us can't damage us (e.g. fellow soldiers, turrets)
	// 2 = Players and NPCs that like us can't damage us, but players can damage us outside of combat
	// 3 = Players and NPCs that like us can't damage us at all
	int iFriendlyFireMode = npc_combine_friendly_fire.GetInt();

	if ( iFriendlyFireMode > 0 && info.GetAttacker() && info.GetAttacker() != this )
	{
		CBaseCombatCharacter *pBCC = info.GetAttacker()->MyCombatCharacterPointer();
		if (pBCC && pBCC->IRelationType(this) == D_LI)
		{
			m_fNoDamageDecal = true;
			if (pBCC->IsPlayer())
			{
				if (iFriendlyFireMode > 1)
				{
					if (iFriendlyFireMode == 3)
						return false;
					else if (iFriendlyFireMode == 2 && (gpGlobals->curtime - GetLastEnemyTime()) <= 2.5f)
						return false;
				}
				m_fNoDamageDecal = false;
			}
			else
			{
				return false;
			}
		}
	}

	// Crude ragdoll damage prevention
	// Prevents server ragdolls from flopping on Combine and killing them
	if ( IsCommandable() && info.GetAttacker() && info.GetAttacker()->ClassMatches( AllocPooledString( "prop_ragdoll" ) ))
	{
		return false;
	}

	return BaseClass::PassesDamageFilter(info);
}

// Overridden to update glow effects when taking damage
int CNPC_Combine::OnTakeDamage_Alive( const CTakeDamageInfo & info )
{
	int ret = BaseClass::OnTakeDamage_Alive(info);
	UpdateSquadGlow();
	return ret;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Override.  Don't update if I'm not looking
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
bool CNPC_Combine::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if( m_spawnflags & SF_COMBINE_NO_LOOK )
	{
		return false;
	}

	return BaseClass::UpdateEnemyMemory( pEnemy, position, pInformer );
}


//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Combine::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();
	
#ifdef EZ
	if( !IsCurSchedule( SCHED_NEW_WEAPON ) )
	{
		SetCustomInterruptCondition( COND_RECEIVED_ORDERS );
	}
#endif

#ifdef EZ2
	if ( !m_StandoffBehavior.IsRunning() )
	{
		if ( !IsCurSchedule( SCHED_COMBINE_ORDER_SURRENDER ) && !IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK ) /*&& !IsCurSchedule( SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE ) && !IsCurSchedule( SCHED_MOVE_TO_WEAPON_RANGE )*/ )
		{
			SetCustomInterruptCondition( COND_COMBINE_CAN_ORDER_SURRENDER );
		}

		SetCustomInterruptCondition( COND_COMBINE_OBSTRUCTED );
	}
#endif

	if (gpGlobals->curtime < m_flNextAttack)
	{
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK2 );
	}

	SetCustomInterruptCondition( COND_COMBINE_HIT_BY_BUGBAIT );

	if ( !IsCurSchedule( SCHED_COMBINE_BURNING_STAND ) )
	{
		SetCustomInterruptCondition( COND_COMBINE_ON_FIRE );
	}

#ifdef MAPBASE
	if (npc_combine_new_cover_behavior.GetBool())
	{
		if ( IsCurSchedule( SCHED_COMBINE_COMBAT_FAIL ) )
		{
			SetCustomInterruptCondition( COND_NEW_ENEMY );
			SetCustomInterruptCondition( COND_LIGHT_DAMAGE );
			SetCustomInterruptCondition( COND_HEAVY_DAMAGE );
		}
		else if ( IsCurSchedule( SCHED_COMBINE_MOVE_TO_MELEE ) )
		{
			SetCustomInterruptCondition( COND_HEAR_DANGER );
			SetCustomInterruptCondition( COND_HEAR_MOVE_AWAY );
		}
	}
#endif
}


#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eNewActivity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CNPC_Combine::Weapon_TranslateActivity( Activity eNewActivity, bool *pRequired )
{
	return BaseClass::Weapon_TranslateActivity(eNewActivity, pRequired);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Combine::NPC_BackupActivity( Activity eNewActivity )
{
	// Some models might not contain ACT_COMBINE_BUGBAIT, which the soldier model uses instead of ACT_IDLE_ON_FIRE.
	// Contrariwise, soldiers may be called to use ACT_IDLE_ON_FIRE in other parts of the AI and need to translate to ACT_COMBINE_BUGBAIT.
	if (eNewActivity == ACT_COMBINE_BUGBAIT)
		return ACT_IDLE_ON_FIRE;
	else if (eNewActivity == ACT_IDLE_ON_FIRE)
		return ACT_COMBINE_BUGBAIT;

	return BaseClass::NPC_BackupActivity( eNewActivity );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Translate base class activities into combot activites
//-----------------------------------------------------------------------------
Activity CNPC_Combine::NPC_TranslateActivity( Activity eNewActivity )
{
	//Slaming this back to ACT_COMBINE_BUGBAIT since we don't want ANYTHING to change our activity while we burn.
	if ( HasCondition( COND_COMBINE_ON_FIRE ) )
		return BaseClass::NPC_TranslateActivity( ACT_COMBINE_BUGBAIT );

	if (eNewActivity == ACT_RANGE_ATTACK2)
	{
#ifndef MAPBASE
		// grunt is going to a secondary long range attack. This may be a thrown 
		// grenade or fired grenade, we must determine which and pick proper sequence
		if (Weapon_OwnsThisType( "weapon_grenadelauncher" ) )
		{
			return ( Activity )ACT_COMBINE_LAUNCH_GRENADE;
		}
		else
#else
		if (m_bUnderthrow)
		{
			return ACT_SPECIAL_ATTACK1;
		}
		else
#endif
		{
#if SHARED_COMBINE_ACTIVITIES
			return ACT_COMBINE_THROW_GRENADE;
#else
			return ( Activity )ACT_COMBINE_THROW_GRENADE;
#endif
		}
	}
	else if (eNewActivity == ACT_IDLE)
	{
		if ( !IsCrouching() && ( m_NPCState == NPC_STATE_COMBAT || m_NPCState == NPC_STATE_ALERT ) )
		{
			eNewActivity = ACT_IDLE_ANGRY;
		}
	}
#ifdef EZ
	// Blixibon - Visually interesting aiming support
	else if (!GetEnemy() && GetAimTarget() && !m_bReadinessCapable)
	{
		switch (eNewActivity)
		{
		case ACT_IDLE:		eNewActivity = ACT_IDLE_ANGRY; break;
		case ACT_WALK:		eNewActivity = ACT_WALK_AIM; break;
		case ACT_RUN:		eNewActivity = ACT_RUN_AIM; break;
		}
	}
#endif


	if ( m_AssaultBehavior.IsRunning() )
	{
		switch ( eNewActivity )
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
#ifdef MAPBASE
	else if (!GetActiveWeapon() && !npc_combine_unarmed_anims.GetBool())
	{
		if (eNewActivity == ACT_IDLE || eNewActivity == ACT_IDLE_ANGRY)
			eNewActivity = ACT_IDLE_SMG1;
		else if (eNewActivity == ACT_WALK)
			eNewActivity = ACT_WALK_RIFLE;
		else if (eNewActivity == ACT_RUN)
			eNewActivity = ACT_RUN_RIFLE;
	}
	else if (m_NPCState == NPC_STATE_IDLE && eNewActivity == ACT_WALK)
	{
		if (npc_combine_idle_walk_easy.GetBool())
		{
			// ACT_WALK_EASY has been replaced with ACT_WALK_RELAXED for weapon translation purposes
			eNewActivity = ACT_WALK_RELAXED;
		}
		else if (GetActiveWeapon())
		{
			eNewActivity = ACT_WALK_RIFLE;
		}
	}

	if ( eNewActivity == ACT_RUN && ( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND ) || IsCurSchedule( SCHED_FLEE_FROM_BEST_SOUND ) ) )
	{
		if ( random->RandomInt( 0, 1 ) && npc_combine_protected_run.GetBool() && HaveSequenceForActivity( ACT_RUN_PROTECTED ) )
			eNewActivity = ACT_RUN_PROTECTED;
	}
#endif

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


//-----------------------------------------------------------------------------
// Purpose: Overidden for human grunts because they  hear the DANGER sound
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::GetSoundInterests( void )
{
	return	SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER | SOUND_PHYSICS_DANGER | SOUND_BULLET_IMPACT | SOUND_MOVE_AWAY;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CNPC_Combine::QueryHearSound( CSound *pSound )
{
	if ( pSound->SoundContext() & SOUND_CONTEXT_COMBINE_ONLY )
		return true;

	if ( pSound->SoundContext() & SOUND_CONTEXT_EXCLUDE_COMBINE )
		return false;

#ifdef EZ
	if (pSound->m_hOwner)
	{
		// Blixibon - Don't be afraid of allies
		if (IRelationType( pSound->m_hOwner ) == D_LI)
			return false;

		// Blixibon - Manhack danger causes soldiers to look stupid
		if (pSound->m_hOwner->ClassMatches("npc_manhack"))
			return false;
	}
#endif

	return BaseClass::QueryHearSound( pSound );
}


//-----------------------------------------------------------------------------
// Purpose: Announce an assault if the enemy can see me and we are pretty 
//			close to him/her
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine::AnnounceAssault(void)
{
	if (random->RandomInt(0,5) > 1)
		return;

	// If enemy can see me make assualt sound
	CBaseCombatCharacter* pBCC = GetEnemyCombatCharacterPointer();

	if (!pBCC)
		return;

	if (!FOkToMakeSound())
		return;

	// Make sure we are pretty close
	if ( WorldSpaceCenter().DistToSqr( pBCC->WorldSpaceCenter() ) > (2000 * 2000))
		return;

	// Make sure we are in view cone of player
	if (!pBCC->FInViewCone ( this ))
		return;

	// Make sure player can see me
	if ( FVisible( pBCC ) )
	{
#ifdef EZ2
		if (GetSquad() && GetSquad()->NumMembers() > 1)
			AddGesture( ACT_GESTURE_SIGNAL_ADVANCE );
#endif
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
		SpeakIfAllowed( TLK_CMB_ASSAULT );
#else
		m_Sentences.Speak( "COMBINE_ASSAULT" );
#endif
	}
}


void CNPC_Combine::AnnounceEnemyType( CBaseEntity *pEnemy )
{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	SpeakIfAllowed( TLK_CMB_ENEMY, SENTENCE_PRIORITY_HIGH );
#else
	const char *pSentenceName = "COMBINE_MONST";
	switch ( pEnemy->Classify() )
	{
	case CLASS_PLAYER:
		pSentenceName = "COMBINE_ALERT";
		break;

	case CLASS_PLAYER_ALLY:
	case CLASS_CITIZEN_REBEL:
	case CLASS_CITIZEN_PASSIVE:
	case CLASS_VORTIGAUNT:
		pSentenceName = "COMBINE_MONST_CITIZENS";
		break;

	case CLASS_PLAYER_ALLY_VITAL:
		pSentenceName = "COMBINE_MONST_CHARACTER";
		break;

	case CLASS_ANTLION:
		pSentenceName = "COMBINE_MONST_BUGS";
		break;

	case CLASS_ZOMBIE:
		pSentenceName = "COMBINE_MONST_ZOMBIES";
		break;

	case CLASS_HEADCRAB:
	case CLASS_BARNACLE:
		pSentenceName = "COMBINE_MONST_PARASITES";
		break;
	}

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
#endif
}

void CNPC_Combine::AnnounceEnemyKill( CBaseEntity *pEnemy )
{
	if (!pEnemy )
		return;

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	AI_CriteriaSet set;
	ModifyOrAppendEnemyCriteria(set, pEnemy);
	SpeakIfAllowed( TLK_CMB_KILLENEMY, set, SENTENCE_PRIORITY_HIGH );
#else
	const char *pSentenceName = "COMBINE_KILL_MONST";
	switch ( pEnemy->Classify() )
	{
	case CLASS_PLAYER:
		pSentenceName = "COMBINE_PLAYER_DEAD";
		break;

		// no sentences for these guys yet
	case CLASS_PLAYER_ALLY:
	case CLASS_CITIZEN_REBEL:
	case CLASS_CITIZEN_PASSIVE:
	case CLASS_VORTIGAUNT:
		break;

	case CLASS_PLAYER_ALLY_VITAL:
		break;

	case CLASS_ANTLION:
		break;

	case CLASS_ZOMBIE:
		break;

	case CLASS_HEADCRAB:
	case CLASS_BARNACLE:
		break;
	}

	m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_HIGH );
#endif
}

//-----------------------------------------------------------------------------
// Select the combat schedule
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectCombatSchedule()
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
		bool bFirstContact = false;
		float flTimeSinceFirstSeen = gpGlobals->curtime - GetEnemies()->FirstTimeSeen( pEnemy );

		if( flTimeSinceFirstSeen < 3.0f )
			bFirstContact = true;

		if ( m_pSquad && pEnemy )
		{
			if ( HasCondition( COND_SEE_ENEMY ) )
			{
				AnnounceEnemyType( pEnemy );
			}

#ifdef EZ
			if( CanDeployManhack() && OccupyStrategySlot( SQUAD_SLOT_COMBINE_DEPLOY_MANHACK ) )
				return SCHED_COMBINE_DEPLOY_MANHACK;
#endif

			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ) )
			{
				// Start suppressing if someone isn't firing already (SLOT_ATTACK1). This means
				// I'm the guy who spotted the enemy, I should react immediately.
				return SCHED_COMBINE_SUPPRESS;
			}

			if ( m_pSquad->IsLeader( this ) || ( m_pSquad->GetLeader() && m_pSquad->GetLeader()->GetEnemy() != pEnemy ) )
			{
				// I'm the leader, but I didn't get the job suppressing the enemy. We know this because
				// This code only runs if the code above didn't assign me SCHED_COMBINE_SUPPRESS.
				if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					return SCHED_RANGE_ATTACK1;
				}

				if( HasCondition(COND_WEAPON_HAS_LOS) && IsStrategySlotRangeOccupied( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					// If everyone else is attacking and I have line of fire, wait for a chance to cover someone.
					if( OccupyStrategySlot( SQUAD_SLOT_OVERWATCH ) )
					{
						return SCHED_COMBINE_ENTER_OVERWATCH;
					}
				}
			}
			else
			{
				if ( m_pSquad->GetLeader() && FOkToMakeSound( SENTENCE_PRIORITY_MEDIUM ) )
				{
					JustMadeSound( SENTENCE_PRIORITY_MEDIUM );	// squelch anything that isn't high priority so the leader can speak
				}

				// First contact, and I'm solo, or not the squad leader.
				if( HasCondition( COND_SEE_ENEMY ) && CanGrenadeEnemy() )
				{
					if( OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
					{
#ifdef EZ // Blixibon - Soldiers announce grenades more often
						SpeakIfAllowed( TLK_CMB_THROWGRENADE );
#endif
						return SCHED_RANGE_ATTACK2;
					}
				}

				if( !bFirstContact && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					if( random->RandomInt(0, 100) < 60 )
					{
						return SCHED_ESTABLISH_LINE_OF_FIRE;
					}
					else
					{
						return SCHED_COMBINE_PRESS_ATTACK;
					}
				}

				return SCHED_TAKE_COVER_FROM_ENEMY;
			}
		}
	}

#ifdef EZ2
	// ---------------------
	// surrender
	// ---------------------
	if (HasCondition( COND_COMBINE_CAN_ORDER_SURRENDER ))
	{
		if (HasCondition( COND_SEE_ENEMY ) && GetEnemy() && GetAbsOrigin().DistToSqr( GetEnemy()->GetAbsOrigin() ) <= Square( npc_combine_order_surrender_max_dist.GetFloat() ))
		{
			return SCHED_COMBINE_ORDER_SURRENDER;
		}
		else
		{
			// Get closer
			return SCHED_COMBINE_PRESS_ATTACK;
		}
	}
#endif

	// ---------------------
	// no ammo
	// ---------------------
	if ( ( HasCondition ( COND_NO_PRIMARY_AMMO ) || HasCondition ( COND_LOW_PRIMARY_AMMO ) ) && !HasCondition( COND_CAN_MELEE_ATTACK1) )
	{
		return SCHED_HIDE_AND_RELOAD;
	}

	// ----------------------
	// LIGHT DAMAGE
	// ----------------------
	if ( HasCondition( COND_LIGHT_DAMAGE ) )
	{
		if ( GetEnemy() != NULL )
		{
			// only try to take cover if we actually have an enemy!

			// FIXME: need to take cover for enemy dealing the damage

			// A standing guy will either crouch or run.
			// A crouching guy tries to stay stuck in.
			if( !IsCrouching() )
			{
				if( GetEnemy() && random->RandomFloat( 0, 100 ) < 50 && CouldShootIfCrouching( GetEnemy() ) )
				{
					Crouch();
				}
				else
				{
					//!!!KELLY - this grunt was hit and is going to run to cover.
					// m_Sentences.Speak( "COMBINE_COVER" );
					return SCHED_TAKE_COVER_FROM_ENEMY;
				}
			}
		}
		else
		{
			// How am I wounded in combat with no enemy?
			Assert( GetEnemy() != NULL );
		}
	}

	// If I'm scared of this enemy run away
#ifdef EZ2
	if ( IRelationType( GetEnemy() ) == D_FR && !HasCondition( COND_CAN_MELEE_ATTACK1 ) )
#else
	if ( IRelationType( GetEnemy() ) == D_FR )
#endif
	{
		if (HasCondition( COND_SEE_ENEMY )	|| 
			HasCondition( COND_SEE_FEAR )	|| 
			HasCondition( COND_LIGHT_DAMAGE ) || 
			HasCondition( COND_HEAVY_DAMAGE ))
		{
			FearSound();
			//ClearCommandGoal();
			return SCHED_RUN_FROM_ENEMY;
		}

		// If I've seen the enemy recently, cower. Ignore the time for unforgettable enemies.
		AI_EnemyInfo_t *pMemory = GetEnemies()->Find( GetEnemy() );
		if ( (pMemory && pMemory->bUnforgettable) || (GetEnemyLastTimeSeen() > (gpGlobals->curtime - 5.0)) )
		{
			// If we're facing him, just look ready. Otherwise, face him.
			if ( FInAimCone( GetEnemy()->EyePosition() ) )
				return SCHED_COMBAT_STAND;

			return SCHED_FEAR_FACE;
		}
	}

	int attackSchedule = SelectScheduleAttack();
	if ( attackSchedule != SCHED_NONE )
		return attackSchedule;

	if (HasCondition(COND_ENEMY_OCCLUDED))
	{
		// stand up, just in case
		Stand();
		DesireStand();

#ifdef EZ
		// If you can't attack, but you can deploy a manhack, do it!
		if( CanDeployManhack() && OccupyStrategySlot( SQUAD_SLOT_COMBINE_DEPLOY_MANHACK ) )
			return SCHED_COMBINE_DEPLOY_MANHACK;
#endif

		if( GetEnemy() && !(GetEnemy()->GetFlags() & FL_NOTARGET) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
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
		return SCHED_COMBINE_WAIT_IN_COVER;
	}

	// --------------------------------------------------------------
	// Enemy not occluded but isn't open to attack
	// --------------------------------------------------------------
	if ( HasCondition( COND_SEE_ENEMY ) && !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
#ifdef EZ
		// If I have a melee weapon and I'm the primary attacker, just chase head-on
		if (GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon() && OccupyStrategySlot( SQUAD_SLOT_ATTACK1 ))
		{
			return SCHED_CHASE_ENEMY;
		}
		else
#endif
		if ( (HasCondition( COND_TOO_FAR_TO_ATTACK ) || IsUsingTacticalVariant(TACTICAL_VARIANT_PRESSURE_ENEMY) ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ))
		{
			return SCHED_COMBINE_PRESS_ATTACK;
		}

		AnnounceAssault(); 
		return SCHED_COMBINE_ASSAULT;
	}

	return SCHED_NONE;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectSchedule( void )
{
	if ( IsWaitingToRappel() && BehaviorSelectSchedule() )
	{
		return BaseClass::SelectSchedule();
	}

	if ( HasCondition(COND_COMBINE_ON_FIRE) )
		return SCHED_COMBINE_BURNING_STAND;

	int nSched = SelectFlinchSchedule();
	if ( nSched != SCHED_NONE )
		return nSched;

	if ( m_hForcedGrenadeTarget )
	{
		if ( m_flNextGrenadeCheck < gpGlobals->curtime )
		{
			Vector vecTarget = m_hForcedGrenadeTarget->WorldSpaceCenter();

#ifdef MAPBASE
			// This was switched to IsAltFireCapable() before, but m_bAlternateCapable makes it necessary to use IsElite() again.
#endif
			if ( IsElite() )
			{
				if ( FVisible( m_hForcedGrenadeTarget ) )
				{
					m_vecAltFireTarget = vecTarget;
					m_hForcedGrenadeTarget = NULL;
					return SCHED_COMBINE_AR2_ALTFIRE;
				}
			}
			else
			{
				// If we can, throw a grenade at the target. 
				// Ignore grenade count / distance / etc
				if ( CheckCanThrowGrenade( vecTarget ) )
				{
					m_hForcedGrenadeTarget = NULL;
					return SCHED_COMBINE_FORCED_GRENADE_THROW;
				}
			}
		}

		// Can't throw at the target, so lets try moving to somewhere where I can see it
		if ( !FVisible( m_hForcedGrenadeTarget ) )
		{
			return SCHED_COMBINE_MOVE_TO_FORCED_GREN_LOS;
		}
	}

#ifdef MAPBASE
	// Drop a grenade?
	if ( HasCondition( COND_COMBINE_DROP_GRENADE ) )
		return SCHED_COMBINE_DROP_GRENADE;
#endif

	if ( m_NPCState != NPC_STATE_SCRIPT)
	{
		// If we're hit by bugbait, thrash around
		if ( HasCondition( COND_COMBINE_HIT_BY_BUGBAIT ) )
		{
			// Don't do this if we're mounting a func_tank
			if ( m_FuncTankBehavior.IsMounted() == true )
			{
				m_FuncTankBehavior.Dismount();
			}

			ClearCondition( COND_COMBINE_HIT_BY_BUGBAIT );
			return SCHED_COMBINE_BUGBAIT_DISTRACTION;
		}

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
						// I hear something dangerous, probably need to take cover.
						// dangerous sound nearby!, call it out
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
						const char *pSentenceName = "COMBINE_DANGER";
#else
						bool bGrenade = false;
#endif

						CBaseEntity *pSoundOwner = pSound->m_hOwner;
						if ( pSoundOwner )
						{
							CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>(pSoundOwner);
							if ( pGrenade && pGrenade->GetThrower() )
							{
								if ( IRelationType( pGrenade->GetThrower() ) != D_LI )
								{
									// special case call out for enemy grenades
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
									pSentenceName = "COMBINE_GREN";
#else
									bGrenade = true;
#endif
								}
							}
						}

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
						SpeakIfAllowed( TLK_CMB_DANGER, UTIL_VarArgs( "grenade:%d", bGrenade ) );
#else
						m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_NORMAL, SENTENCE_CRITERIA_NORMAL );
#endif

						// If the sound is approaching danger, I have no enemy, and I don't see it, turn to face.
						if( !GetEnemy() && pSound->IsSoundType(SOUND_CONTEXT_DANGER_APPROACH) && pSound->m_hOwner && !FInViewCone(pSound->GetSoundReactOrigin()) )
						{
							GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
							return SCHED_COMBINE_FACE_IDEAL_YAW;
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

#ifdef EZ2
		// If we have an obstruction, just plow through
		if ( HasCondition( COND_COMBINE_OBSTRUCTED ) )
		{
			SetTarget( m_hObstructor );
			m_hObstructor = NULL;
			return SCHED_COMBINE_ATTACK_TARGET;
		}
#endif

#ifdef EZ // Blixibon - Follow behavior deferring
		if ( ShouldDeferToFollowBehavior() )
		{
			DeferSchedulingToBehavior( &(GetFollowBehavior()) );
			return BaseClass::SelectSchedule();
		}
		else
#endif
		if( BehaviorSelectSchedule() )
		{
			return BaseClass::SelectSchedule();
		}
	}

	switch	( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			if ( m_bShouldPatrol )
				return SCHED_COMBINE_PATROL;
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

			// Don't patrol if I'm in the middle of an assault, because I'll never return to the assault. 
			if ( !m_AssaultBehavior.HasAssaultCue() )
			{
				if( m_bShouldPatrol || HasCondition( COND_COMBINE_SHOULD_PATROL ) )
					return SCHED_COMBINE_PATROL;
			}
		}
		break;

	case NPC_STATE_COMBAT:
		{
			int nSched = SelectCombatSchedule();
			if ( nSched != SCHED_NONE )
				return nSched;
		}
		break;
	}

	// no special cases here, call the base class
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Combine::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
#ifdef EZ2
	// If we can swat physics objects, see if we can swat our obstructor
	if ( IsPathTaskFailure( taskFailCode ) && 
		 m_hObstructor != NULL && m_hObstructor->VPhysicsGetObject() )
	{
		SetTarget( m_hObstructor );
		m_hObstructor = NULL;
		return SCHED_COMBINE_ATTACK_TARGET;
	}
	m_hObstructor = NULL;
#endif

	if( failedSchedule == SCHED_COMBINE_TAKE_COVER1 )
	{
#ifdef MAPBASE
		if( IsInSquad() && IsStrategySlotRangeOccupied(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2) && HasCondition(COND_SEE_ENEMY)
			&& ( !npc_combine_new_cover_behavior.GetBool() || (taskFailCode == FAIL_NO_COVER) ) )
#else
		if( IsInSquad() && IsStrategySlotRangeOccupied(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2) && HasCondition(COND_SEE_ENEMY) )
#endif
		{
			// This eases the effects of an unfortunate bug that usually plagues shotgunners. Since their rate of fire is low,
			// they spend relatively long periods of time without an attack squad slot. If you corner a shotgunner, usually 
			// the other memebers of the squad will hog all of the attack slots and pick schedules to move to establish line of
			// fire. During this time, the shotgunner is prevented from attacking. If he also cannot find cover (the fallback case)
			// he will stand around like an idiot, right in front of you. Instead of this, we have him run up to you for a melee attack.
			return SCHED_COMBINE_MOVE_TO_MELEE;
		}
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

#ifdef EZ2
ConVar npc_combine_obstruction_behavior( "npc_combine_obstruction_behavior", "0" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Combine::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult )
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
						SetCondition( COND_COMBINE_OBSTRUCTED );
					}
					break;

					case NPC_STATE_IDLE:
					case NPC_STATE_ALERT:
					default:
					{
						// Interrupt if we've been obstructed for a bit or we have a command goal
						if (gpGlobals->curtime - m_flTimeSinceObstructed >= 2.0f || HaveCommandGoal())
							SetCondition( COND_COMBINE_OBSTRUCTED );
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
bool CNPC_Combine::ShouldAttackObstruction( CBaseEntity *pEntity )
{
	if (!npc_combine_obstruction_behavior.GetBool())
		return false;

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
#endif

//-----------------------------------------------------------------------------
// Should we charge the player?
//-----------------------------------------------------------------------------
bool CNPC_Combine::ShouldChargePlayer()
{
	return GetEnemy() && GetEnemy()->IsPlayer() && PlayerHasMegaPhysCannon() && !IsLimitingHintGroups();
}


//-----------------------------------------------------------------------------
// Select attack schedules
//-----------------------------------------------------------------------------
#define COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE	192
#define COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE_SQ	(COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE*COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE)

int CNPC_Combine::SelectScheduleAttack()
{
#ifndef MAPBASE // Moved to SelectSchedule()
	// Drop a grenade?
	if ( HasCondition( COND_COMBINE_DROP_GRENADE ) )
		return SCHED_COMBINE_DROP_GRENADE;
#endif

	// Kick attack?
	if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		return SCHED_MELEE_ATTACK1;
	}

#ifdef EZ
	if( GetEnemy() && CanDeployManhack() && OccupyStrategySlot( SQUAD_SLOT_COMBINE_DEPLOY_MANHACK ) )
		return SCHED_COMBINE_DEPLOY_MANHACK;
#endif

	// If I'm fighting a combine turret (it's been hacked to attack me), I can't really
	// hurt it with bullets, so become grenade happy.
#ifdef MAPBASE
	if ( GetEnemy() && ( (IsUsingTacticalVariant(TACTICAL_VARIANT_GRENADE_HAPPY)) || GetEnemy()->ClassMatches(gm_isz_class_FloorTurret) ) )
#else
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_COMBINE && FClassnameIs(GetEnemy(), "npc_turret_floor") )
#endif
	{
		// Don't do this until I've been fighting the turret for a few seconds
		float flTimeAtFirstHand = GetEnemies()->TimeAtFirstHand(GetEnemy());
		if ( flTimeAtFirstHand != AI_INVALID_TIME )
		{
			float flTimeEnemySeen = gpGlobals->curtime - flTimeAtFirstHand;
			if ( flTimeEnemySeen > 4.0 )
			{
				if ( CanGrenadeEnemy() && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
					return SCHED_RANGE_ATTACK2;
			}
		}

		// If we're not in the viewcone of the turret, run up and hit it. Do this a bit later to
		// give other squadmembers a chance to throw a grenade before I run in.
#ifdef MAPBASE
		// Don't do turret charging of we're just grenade happy.
		if ( !IsUsingTacticalVariant(TACTICAL_VARIANT_GRENADE_HAPPY) && !GetEnemy()->MyNPCPointer()->FInViewCone( this ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
#else
		if ( !GetEnemy()->MyNPCPointer()->FInViewCone( this ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
#endif
			return SCHED_COMBINE_CHARGE_TURRET;
	}

	// When fighting against the player who's wielding a mega-physcannon, 
	// always close the distance if possible
	// But don't do it if you're in a nav-limited hint group
	if ( ShouldChargePlayer() )
	{
		float flDistSq = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
		if ( flDistSq <= COMBINE_MEGA_PHYSCANNON_ATTACK_DISTANCE_SQ )
		{
			if( HasCondition(COND_SEE_ENEMY) )
			{
				if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
					return SCHED_RANGE_ATTACK1;
			}
			else
			{
				if ( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
					return SCHED_COMBINE_PRESS_ATTACK;
			}
		}

		if ( HasCondition(COND_SEE_ENEMY) && !IsUnreachable( GetEnemy() ) )
		{
			return SCHED_COMBINE_CHARGE_PLAYER;
		}
	}

	// Can I shoot?
	if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
	{

		// JAY: HL1 behavior missing?
#if 0
		if ( m_pSquad )
		{
			// if the enemy has eluded the squad and a squad member has just located the enemy
			// and the enemy does not see the squad member, issue a call to the squad to waste a 
			// little time and give the player a chance to turn.
			if ( MySquadLeader()->m_fEnemyEluded && !HasConditions ( bits_COND_ENEMY_FACING_ME ) )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				return SCHED_GRUNT_FOUND_ENEMY;
			}
		}
#endif

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
#ifdef EZ // Blixibon - Soldiers announce grenades more often
				SpeakIfAllowed( TLK_CMB_THROWGRENADE );
#endif
				return SCHED_RANGE_ATTACK2;
			}
		}

		DesireCrouch();
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

		//Msg("Time: %f   Dist: %f\n", flTime, flDist );
		if ( flTime <= COMBINE_GRENADE_FLUSH_TIME && flDist <= COMBINE_GRENADE_FLUSH_DIST && CanGrenadeEnemy( false ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
		{
#ifdef EZ // Blixibon - Soldiers announce grenades more often
			SpeakIfAllowed( TLK_CMB_THROWGRENADE );
#endif
			return SCHED_RANGE_ATTACK2;
		}
	}

	if (HasCondition(COND_WEAPON_SIGHT_OCCLUDED))
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
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
#ifdef EZ
	// 1upD - Don't move away if you have a command goal
	case SCHED_MOVE_TO_WEAPON_RANGE:
		if (!IsMortar(GetEnemy()) && GetCommandGoal() != vec3_invalid)
		{
			if (GetActiveWeapon() && (GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK1) && random->RandomInt(0, 1) && HasCondition(COND_SEE_ENEMY) && !HasCondition(COND_NO_PRIMARY_AMMO))
				return TranslateSchedule(SCHED_RANGE_ATTACK1);

			return SCHED_STANDOFF;
		}
		break;

	case SCHED_CHASE_ENEMY:
		if (!IsMortar(GetEnemy()) && GetCommandGoal() != vec3_invalid)
		{
			return SCHED_STANDOFF;
		}
		break;
	// End command schedule translations
#endif
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			if ( m_pSquad )
			{
				// Have to explicitly check innate range attack condition as may have weapon with range attack 2
				if (	g_pGameRules->IsSkillLevel( SKILL_HARD )	&& 
					HasCondition(COND_CAN_RANGE_ATTACK2)		&&
					OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
				{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
					SpeakIfAllowed( TLK_CMB_THROWGRENADE );
#else
					m_Sentences.Speak( "COMBINE_THROW_GRENADE" );
#endif
					return SCHED_COMBINE_TOSS_GRENADE_COVER1;
				}
				else
				{
					if ( ShouldChargePlayer() && !IsUnreachable( GetEnemy() ) )
						return SCHED_COMBINE_CHARGE_PLAYER;

					return SCHED_COMBINE_TAKE_COVER1;
				}
			}
			else
			{
				// Have to explicitly check innate range attack condition as may have weapon with range attack 2
				if ( random->RandomInt(0,1) && HasCondition(COND_CAN_RANGE_ATTACK2) )
				{
					return SCHED_COMBINE_GRENADE_COVER1;
				}
				else
				{
					if ( ShouldChargePlayer() && !IsUnreachable( GetEnemy() ) )
						return SCHED_COMBINE_CHARGE_PLAYER;

					return SCHED_COMBINE_TAKE_COVER1;
				}
			}
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
#ifdef EZ
			// Blixibon - This means we fall back to SCHED_PC_MOVE_TOWARDS_COVER_FROM_BEST_SOUND instead
			// Needed for preventing stupidity from worker acid in the antlion standoff at the end of Chapter 1
			if ( !HaveCommandGoal() )
#endif
			return SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND;
		}
		break;
	case SCHED_COMBINE_TAKECOVER_FAILED:
		{
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				return TranslateSchedule( SCHED_RANGE_ATTACK1 );
			}

#ifdef MAPBASE
			if ( npc_combine_new_cover_behavior.GetBool() && HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) )
			{
				return TranslateSchedule( SCHED_RANGE_ATTACK2 );
			}
#endif

			// Run somewhere randomly
			return TranslateSchedule( SCHED_FAIL ); 
			break;
		}
		break;
	case SCHED_FAIL_ESTABLISH_LINE_OF_FIRE:
		{
			if( !IsCrouching() )
			{
				if( GetEnemy() && CouldShootIfCrouching( GetEnemy() ) )
				{
					Crouch();
					return SCHED_COMBAT_FACE;
				}
			}

			if( HasCondition( COND_SEE_ENEMY ) )
			{
				return TranslateSchedule( SCHED_TAKE_COVER_FROM_ENEMY );
			}
			else if ( !m_AssaultBehavior.HasAssaultCue() )
			{
				// Don't patrol if I'm in the middle of an assault, because 
				// I'll never return to the assault. 
				if ( GetEnemy() )
				{
					RememberUnreachable( GetEnemy() );
				}

				return TranslateSchedule( SCHED_COMBINE_PATROL );
			}
		}
		break;
	case SCHED_COMBINE_ASSAULT:
		{
			CBaseEntity *pEntity = GetEnemy();

			// FIXME: this should be generalized by the schedules that are selected, or in the definition of 
			// what "cover" means (i.e., trace attack vulnerability vs. physical attack vulnerability
			if (pEntity && pEntity->MyNPCPointer())
			{
				if ( !(pEntity->MyNPCPointer()->CapabilitiesGet( ) & bits_CAP_WEAPON_RANGE_ATTACK1))
				{
					return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
				}
			}
			// don't charge forward if there's a hint group
			if (GetHintGroup() != NULL_STRING)
			{
				return TranslateSchedule( SCHED_ESTABLISH_LINE_OF_FIRE );
			}
			return SCHED_COMBINE_ASSAULT;
		}
	case SCHED_ESTABLISH_LINE_OF_FIRE:
		{
			// always assume standing
			// Stand();

			if( CanAltFireEnemy(true) && OccupyStrategySlot(SQUAD_SLOT_SPECIAL_ATTACK) )
			{
				// If an elite in the squad could fire a combine ball at the player's last known position,
				// do so!
				return SCHED_COMBINE_AR2_ALTFIRE;
			}
#ifdef EZ
			// 1upD - Don't move from point if there is a command goal
			if (!IsMortar(GetEnemy()) && HaveCommandGoal())
			{
				if (GetActiveWeapon() && (GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK1) && random->RandomInt(0, 1) && HasCondition(COND_SEE_ENEMY) && !HasCondition(COND_NO_PRIMARY_AMMO))
					return TranslateSchedule(SCHED_RANGE_ATTACK1);

				return SCHED_STANDOFF;
			}
			// End command goal
#endif
			if( IsUsingTacticalVariant( TACTICAL_VARIANT_PRESSURE_ENEMY ) && !IsRunningBehavior() )
			{
				if( OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
				{
					return SCHED_COMBINE_PRESS_ATTACK;
				}
			}

			return SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE;
		}
		break;
	case SCHED_HIDE_AND_RELOAD:
		{
			// stand up, just in case
			// Stand();
			// DesireStand();
			if( CanGrenadeEnemy() && OccupyStrategySlot( SQUAD_SLOT_GRENADE1 ) && random->RandomInt( 0, 100 ) < 20 )
			{
				// If I COULD throw a grenade and I need to reload, 20% chance I'll throw a grenade before I hide to reload.
				return SCHED_COMBINE_GRENADE_AND_RELOAD;
			}

			// No running away in the citadel!
			if ( ShouldChargePlayer() )
				return SCHED_RELOAD;

			return SCHED_COMBINE_HIDE_AND_RELOAD;
		}
		break;
	case SCHED_RANGE_ATTACK1:
		{
			if ( HasCondition( COND_NO_PRIMARY_AMMO ) || HasCondition( COND_LOW_PRIMARY_AMMO ) )
			{
				// Ditch the strategy slot for attacking (which we just reserved!)
				VacateStrategySlot();
				return TranslateSchedule( SCHED_HIDE_AND_RELOAD );
			}

			if( CanAltFireEnemy(true) && OccupyStrategySlot(SQUAD_SLOT_SPECIAL_ATTACK) )
			{
				// Since I'm holding this squadslot, no one else can try right now. If I die before the shot 
				// goes off, I won't have affected anyone else's ability to use this attack at their nearest
				// convenience.
				return SCHED_COMBINE_AR2_ALTFIRE;
			}

			if ( IsCrouching() || ( CrouchIsDesired() && !HasCondition( COND_HEAVY_DAMAGE ) ) )
			{
				// See if we can crouch and shoot
				if (GetEnemy() != NULL)
				{
					float dist = (GetLocalOrigin() - GetEnemy()->GetLocalOrigin()).Length();

					// only crouch if they are relatively far away
					if (dist > COMBINE_MIN_CROUCH_DISTANCE)
					{
						// try crouching
						Crouch();

						Vector targetPos = GetEnemy()->BodyTarget(GetActiveWeapon()->GetLocalOrigin());

						// if we can't see it crouched, stand up
						if (!WeaponLOSCondition(GetLocalOrigin(),targetPos,false))
						{
							Stand();
						}
					}
				}
			}
			else
			{
				// always assume standing
				Stand();
			}

#ifdef MAPBASE
			// SCHED_COMBINE_WAIT_IN_COVER uses INCOVER, but only gets out of it when the soldier moves.
			// That seems to mess up shooting, so this Forget() attempts to fix that.
			Forget( bits_MEMORY_INCOVER );
#endif

			return SCHED_COMBINE_RANGE_ATTACK1;
		}
	case SCHED_RANGE_ATTACK2:
		{
			// If my weapon can range attack 2 use the weapon
			if (GetActiveWeapon() && GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK2)
			{
				return SCHED_RANGE_ATTACK2;
			}
			// Otherwise use innate attack
			else
			{
				return SCHED_COMBINE_RANGE_ATTACK2;
			}
		}
		// SCHED_COMBAT_FACE:
		// SCHED_COMBINE_WAIT_FACE_ENEMY:
		// SCHED_COMBINE_SWEEP:
		// SCHED_COMBINE_COVER_AND_RELOAD:
		// SCHED_COMBINE_FOUND_ENEMY:

	case SCHED_VICTORY_DANCE:
		{
			return SCHED_COMBINE_VICTORY_DANCE;
		}
	case SCHED_COMBINE_SUPPRESS:
		{
#define MIN_SIGNAL_DIST	256
			if ( GetEnemy() != NULL && GetEnemy()->IsPlayer() && m_bFirstEncounter )
			{
				float flDistToEnemy = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).Length();

				if( flDistToEnemy >= MIN_SIGNAL_DIST )
				{
					m_bFirstEncounter = false;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
					return SCHED_COMBINE_SIGNAL_SUPPRESS;
				}
			}

			return SCHED_COMBINE_SUPPRESS;
		}
	case SCHED_FAIL:
		{
			if ( GetEnemy() != NULL )
			{
				return SCHED_COMBINE_COMBAT_FAIL;
			}
			return SCHED_FAIL;
		}

	case SCHED_COMBINE_PATROL:
		{
			// If I have an enemy, don't go off into random patrol mode.
			if ( GetEnemy() && GetEnemy()->IsAlive() )
				return SCHED_COMBINE_PATROL_ENEMY;

			return SCHED_COMBINE_PATROL;
		}
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//=========================================================
//=========================================================
void CNPC_Combine::OnStartSchedule( int scheduleType ) 
{
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Combine::HandleAnimEvent( animevent_t *pEvent )
{
	Vector vecShootDir;
	Vector vecShootOrigin;
	bool handledEvent = false;

	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if ( pEvent->event == COMBINE_AE_BEGIN_ALTFIRE )
		{
#ifdef MAPBASE
			if (GetActiveWeapon())
				GetActiveWeapon()->WeaponSound(SPECIAL1);
#else
			EmitSound( "Weapon_CombineGuard.Special1" );
#endif
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
			SpeakIfAllowed( TLK_CMB_THROWGRENADE, "altfire:1", SENTENCE_PRIORITY_MEDIUM );
#endif
			handledEvent = true;
		}
		else if ( pEvent->event == COMBINE_AE_ALTFIRE )
		{
#ifdef MAPBASE
			if ( IsAltFireCapable() && GetActiveWeapon() )
#else
			if ( IsElite() )
#endif
			{
				animevent_t fakeEvent;

				fakeEvent.pSource = this;
				fakeEvent.event = EVENT_WEAPON_AR2_ALTFIRE;
				GetActiveWeapon()->Operator_HandleAnimEvent( &fakeEvent, this );

				// Stop other squad members from combine balling for a while.
				DelaySquadAltFireAttack( 10.0f );

				// I'm disabling this decrementor. At the time of this change, the elites
				// don't bother to check if they have grenades anyway. This means that all
				// elites have infinite combine balls, even if the designer marks the elite
				// as having 0 grenades. By disabling this decrementor, yet enabling the code
				// that makes sure the elite has grenades in order to fire a combine ball, we
				// preserve the legacy behavior while making it possible for a designer to prevent
				// elites from shooting combine balls by setting grenades to '0' in hammer. (sjb) EP2_OUTLAND_10
#ifdef MAPBASE
				// 
				// Here's a tip: In Mapbase, "OnThrowGrenade" is fired during alt-fire as well, fired by the weapon so it could pass its alt-fire projectile.
				// So if you want elites to decrement on each grenade again, you could fire "!self > AddGrenades -1" every time an elite fires OnThrowGrenade.
				// 
				// AddGrenades(-1);
#else
				// m_iNumGrenades--;
#endif
			}

			handledEvent = true;
		}
#ifdef EZ
		else if ( pEvent->event == AE_METROPOLICE_START_DEPLOY )
		{
			OnAnimEventStartDeployManhack();
			return;
		}
		else if ( pEvent->event == AE_METROPOLICE_DEPLOY_MANHACK )
		{
			OnAnimEventDeployManhack( pEvent );
			return;
		}
#endif
		else
		{
			BaseClass::HandleAnimEvent( pEvent );
		}
	}
	else
	{
		switch( pEvent->event )
		{
		case COMBINE_AE_AIM:	
			{
				handledEvent = true;
				break;
			}
		case COMBINE_AE_RELOAD:

			// We never actually run out of ammo, just need to refill the clip
			if (GetActiveWeapon())
			{
#ifdef MAPBASE
				GetActiveWeapon()->Reload_NPC();
#else
				GetActiveWeapon()->WeaponSound( RELOAD_NPC );
				GetActiveWeapon()->m_iClip1 = GetActiveWeapon()->GetMaxClip1(); 
#endif
				GetActiveWeapon()->m_iClip2 = GetActiveWeapon()->GetMaxClip2();  
			}
			ClearCondition(COND_LOW_PRIMARY_AMMO);
			ClearCondition(COND_NO_PRIMARY_AMMO);
			ClearCondition(COND_NO_SECONDARY_AMMO);
			handledEvent = true;
			break;

		case COMBINE_AE_GREN_TOSS:
			{
				Vector vecSpin;
				vecSpin.x = random->RandomFloat( -1000.0, 1000.0 );
				vecSpin.y = random->RandomFloat( -1000.0, 1000.0 );
				vecSpin.z = random->RandomFloat( -1000.0, 1000.0 );

				Vector vecStart;
				GetAttachment( "lefthand", vecStart );

				if( m_NPCState == NPC_STATE_SCRIPT )
				{
					// Use a fixed velocity for grenades thrown in scripted state.
					// Grenades thrown from a script do not count against grenades remaining for the AI to use.
					Vector forward, up, vecThrow;

					GetVectors( &forward, NULL, &up );
					vecThrow = forward * 750 + up * 175;
#ifdef MAPBASE
#ifdef EZ2
					CBaseEntity *pGrenade = NULL;
					if ( m_bThrowXenGrenades )
					{
						pGrenade = HopWire_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER );
					}
					else
					{
						pGrenade = Fraggrenade_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER, true );
					}
#else
					CBaseEntity *pGrenade = Fraggrenade_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER, true );
#endif
					m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
#else
					Fraggrenade_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER, true );
#endif
				}
				else
				{
					// Use the Velocity that AI gave us.
#ifdef MAPBASE
#ifdef EZ2
					CBaseEntity *pGrenade = NULL;
					if ( m_bThrowXenGrenades )
					{
						pGrenade = HopWire_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER );
					}
					else
					{
						pGrenade = Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER, !this->IsPlayerAlly() );
					}
#else
					CBaseEntity *pGrenade = Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER, true );
#endif
					m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
					AddGrenades(-1, pGrenade);
#else
					Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER, true );
					m_iNumGrenades--;
#endif
				}

				// wait six seconds before even looking again to see if a grenade can be thrown.
				m_flNextGrenadeCheck = gpGlobals->curtime + 6;
			}
			handledEvent = true;
			break;

		case COMBINE_AE_GREN_LAUNCH:
			{
				EmitSound( "NPC_Combine.GrenadeLaunch" );

				CBaseEntity *pGrenade = CreateNoSpawn( "npc_contactgrenade", Weapon_ShootPosition(), vec3_angle, this );
				pGrenade->KeyValue( "velocity", m_vecTossVelocity );
				pGrenade->Spawn( );

				if ( g_pGameRules->IsSkillLevel(SKILL_HARD) )
					m_flNextGrenadeCheck = gpGlobals->curtime + random->RandomFloat( 2, 5 );// wait a random amount of time before shooting again
				else
					m_flNextGrenadeCheck = gpGlobals->curtime + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
			}
			handledEvent = true;
			break;

		case COMBINE_AE_GREN_DROP:
			{
				Vector vecStart;
#ifdef MAPBASE
				QAngle angStart;
				m_vecTossVelocity.x = 15;
				m_vecTossVelocity.y = 0;
				m_vecTossVelocity.z = 0;

				GetAttachment( "lefthand", vecStart, angStart );

				CBaseEntity *pGrenade = NULL;
				if (m_NPCState == NPC_STATE_SCRIPT)
				{
					// While scripting, have the grenade face upwards like it was originally and also don't decrement grenade count.
					pGrenade = Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vec3_origin, this, COMBINE_GRENADE_TIMER, true );
				}
				else
				{
					pGrenade = Fraggrenade_Create( vecStart, angStart, m_vecTossVelocity, vec3_origin, this, COMBINE_GRENADE_TIMER, true );
					AddGrenades(-1);
				}

				// Well, technically we're not throwing, but...still.
				m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
#else
				GetAttachment( "lefthand", vecStart );

				Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vec3_origin, this, COMBINE_GRENADE_TIMER, true );
				m_iNumGrenades--;
#endif
			}
			handledEvent = true;
			break;

		case COMBINE_AE_KICK:
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
							pBCC->ViewPunch( QAngle(-12,-7,0) );
							pHurt->ApplyAbsVelocityImpulse( forward * 100 + up * 50 );
						}

						CTakeDamageInfo info( this, this, m_nKickDamage, DMG_CLUB );
						CalculateMeleeDamageForce( &info, forward, pBCC->GetAbsOrigin() );
#ifdef EZ					
						// 1upD - Commandable Combine soldiers should 1-hit rebels with their melee attack
						// Blixibon - Damage is changed after damage force is calculated so citizens aren't launched across the room (more natural-looking)
						if (IsCommandable() && pBCC->ClassMatches("npc_citizen"))
						{
							info.SetDamage(pBCC->GetHealth());
						}
#endif				
						pBCC->TakeDamage( info );

						BashSound();
					}
				}	
#ifdef EZ2
				else
				{
					// Apply a velocity to hit entity if it has a physics movetype
					if ( pHurt && pHurt->GetMoveType() == MOVETYPE_VPHYSICS )
					{
						Vector forward, up;
						AngleVectors( GetLocalAngles(), &forward, NULL, &up );
						pHurt->ApplyAbsVelocityImpulse( forward * 100 + up * 50 );

						CTakeDamageInfo info( this, this, m_nKickDamage, DMG_CLUB );
						CalculateMeleeDamageForce( &info, forward, pHurt->GetAbsOrigin() );
						pHurt->TakeDamage( info );

						BashSound();
					}
				}
#endif

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
				SpeakIfAllowed( TLK_CMB_KICK );
#else
				m_Sentences.Speak( "COMBINE_KICK" );
#endif
				handledEvent = true;
				break;
			}

		case COMBINE_AE_CAUGHT_ENEMY:
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
			SpeakIfAllowed( TLK_CMB_ENEMY ); //SpeakIfAllowed( "TLK_CMB_ALERT" );
#else
			m_Sentences.Speak( "COMBINE_ALERT" );
#endif
			handledEvent = true;
			break;

#ifdef EZ2
		case NPC_EVENT_ITEM_PICKUP:
			{
				// HACKHACK: Because weapon_frag is technically a weapon, Combine soldiers will attempt
				// to pick it up as a weapon instead of as an item.
				// A long-term solution to this should be implemented in CAI_BaseNPC in the future.
				if (GetTarget() && GetTarget()->ClassMatches("weapon_frag"))
				{
					// Picking up an item.
					PickupItem( GetTarget() );
					TaskComplete();
				}
				else
				{
					BaseClass::HandleAnimEvent( pEvent );
				}
				break;
			}
#endif

		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
		}
	}

	if( handledEvent )
	{
		m_iLastAnimEventHandled = pEvent->event;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get shoot position of BCC at an arbitrary position
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CNPC_Combine::Weapon_ShootPosition( )
{
	bool bStanding = !IsCrouching();
	Vector right;
	GetVectors( NULL, &right, NULL );

	if ((CapabilitiesGet() & bits_CAP_DUCK) )
	{
		if ( IsCrouchedActivity( GetActivity() ) )
		{
			bStanding = false;
		}
	}

	// FIXME: rename this "estimated" since it's not based on animation
	// FIXME: the orientation won't be correct when testing from arbitary positions for arbitary angles

#ifdef MAPBASE
	// HACKHACK: This weapon shoot position code does not work properly when in close range, causing the aim
	// to drift to the left as the enemy gets closer to it.
	// This problem is usually bearable for regular combat, but it causes dynamic interaction yaw to be offset
	// as well, preventing most from ever being triggered.
	// Ideally, this should be fixed from the root cause, but due to the sensitivity of such a change, this is
	// currently being tied to a cvar which is off by default.
	// 
	// If the cvar is disabled but the soldier has valid interactions on its current enemy, then a separate hack
	// will still attempt to correct the drift as the enemy gets closer.
	if ( npc_combine_fixed_shootpos.GetBool() )
	{
		right *= 0.0f;
	}
	else if ( HasValidInteractionsOnCurrentEnemy() )
	{
		float flDistSqr = GetEnemy()->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() );
		if (flDistSqr < Square( 128.0f ))
			right *= (flDistSqr / Square( 128.0f ));
	}
#endif

	if  ( bStanding )
	{
		if( HasShotgun() )
		{
			return GetAbsOrigin() + COMBINE_SHOTGUN_STANDING_POSITION + right * 8;
		}
		else
		{
			return GetAbsOrigin() + COMBINE_GUN_STANDING_POSITION + right * 8;
		}
	}
	else
	{
		if( HasShotgun() )
		{
			return GetAbsOrigin() + COMBINE_SHOTGUN_CROUCHING_POSITION + right * 8;
		}
		else
		{
			return GetAbsOrigin() + COMBINE_GUN_CROUCHING_POSITION + right * 8;
		}
	}
}


//=========================================================
// Speak Sentence - say your cued up sentence.
//
// Some grunt sentences (take cover and charge) rely on actually
// being able to execute the intended action. It's really lame
// when a grunt says 'COVER ME' and then doesn't move. The problem
// is that the sentences were played when the decision to TRY
// to move to cover was made. Now the sentence is played after 
// we know for sure that there is a valid path. The schedule
// may still fail but in most cases, well after the grunt has 
// started moving.
//=========================================================
void CNPC_Combine::SpeakSentence( int sentenceType )
{
	switch( sentenceType )
	{
	case 0: // assault
		AnnounceAssault();
		break;

	case 1: // Flanking the player
		// If I'm moving more than 20ft, I need to talk about it
		if ( GetNavigator()->GetPath()->GetPathLength() > 20 * 12.0f )
		{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
			SpeakIfAllowed( TLK_CMB_FLANK );
#else
			m_Sentences.Speak( "COMBINE_FLANK" );
#endif
		}
		break;

#ifdef EZ2
	case 6: // Ordering enemy to surrender
		// Make sure the 'order surrender' concept is always dispatched as it is vital to telegraph when a soldier is ordering a citizen to surrender
		SpeakIfAllowed( TLK_USE, "order_surrender:1", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
		if (GetEnemy())
			GetEnemy()->DispatchInteraction( g_interactionBadCopOrderSurrender, NULL, this );
		break;
#endif
	}
}

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
//=========================================================
bool CNPC_Combine::SpeakIfAllowed( const char *concept, const char *modifiers, SentencePriority_t sentencepriority, SentenceCriteria_t sentencecriteria )
{
	AI_CriteriaSet set;
	if (modifiers)
	{
#ifdef NEW_RESPONSE_SYSTEM
		GatherCriteria( &set, concept, modifiers );
#else
		GetExpresser()->MergeModifiers(set, modifiers);
#endif
	}
	return SpeakIfAllowed( concept, set, sentencepriority, sentencecriteria );
}

//=========================================================
//=========================================================
bool CNPC_Combine::SpeakIfAllowed( const char *concept, AI_CriteriaSet& modifiers, SentencePriority_t sentencepriority, SentenceCriteria_t sentencecriteria )
{
	if ( sentencepriority != SENTENCE_PRIORITY_INVALID && !FOkToMakeSound( sentencepriority ) )
		return false;

	if ( !GetExpresser()->CanSpeakConcept( concept ) )
		return false;

	// Don't interrupt scripted VCD dialogue
	if ( IsRunningScriptedSceneWithSpeechAndNotPaused( this, true ) )
		return false;

	if ( Speak( concept, modifiers ) )
	{
		JustMadeSound( sentencepriority, 2.0f /*GetTimeSpeechComplete()*/ );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "numgrenades", UTIL_VarArgs("%d", m_iNumGrenades) );
	
	if (IsElite())
	{
		set.AppendCriteria( "elite", "1" );
	}
	else
	{
		set.AppendCriteria( "elite", "0" );
	}

#ifdef EZ
	if (IsInSquad())
		set.AppendCriteria( "squad", GetSquad()->GetName() );
#endif
}
#endif

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::ModifyOrAppendCriteriaForPlayer( CBasePlayer *pPlayer, AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteriaForPlayer( pPlayer, set );

	set.AppendCriteria( "elite", IsElite() ? "1" : "0" );
}
#endif

//=========================================================
// PainSound
//=========================================================
#ifdef MAPBASE
void CNPC_Combine::PainSound ( const CTakeDamageInfo &info )
#else
void CNPC_Combine::PainSound ( void )
#endif
{
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	if ( gpGlobals->curtime > m_flNextPainSoundTime )
	{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
		AI_CriteriaSet set;
		ModifyOrAppendDamageCriteria(set, info);
		SpeakIfAllowed( TLK_CMB_PAIN, set, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
#else
		const char *pSentenceName = "COMBINE_PAIN";
		float healthRatio = (float)GetHealth() / (float)GetMaxHealth();
		if ( !HasMemory(bits_MEMORY_PAIN_LIGHT_SOUND) && healthRatio > 0.9 )
		{
			Remember( bits_MEMORY_PAIN_LIGHT_SOUND );
			pSentenceName = "COMBINE_TAUNT";
		}
		else if ( !HasMemory(bits_MEMORY_PAIN_HEAVY_SOUND) && healthRatio < 0.25 )
		{
			Remember( bits_MEMORY_PAIN_HEAVY_SOUND );
			pSentenceName = "COMBINE_COVER";
		}

		m_Sentences.Speak( pSentenceName, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
#endif
		m_flNextPainSoundTime = gpGlobals->curtime + 1;
	}
}

#ifdef EZ
//=========================================================
// RegenSound - 1upD
//=========================================================
void CNPC_Combine::RegenSound()
{
	// Don't call out regeneration immediately after taking damage 
	if (gpGlobals->curtime <= m_flNextPainSoundTime)
	{
		m_flNextRegenSoundTime = gpGlobals->curtime + COMBINE_REGEN_SOUND_TIME;
	}

	if (gpGlobals->curtime > m_flNextRegenSoundTime )
	{
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
		m_Sentences.Speak("COMBINE_EZ2_REGEN", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS);
#else
		SpeakIfAllowed( TLK_HEAL );
#endif
		m_flNextRegenSoundTime = gpGlobals->curtime + COMBINE_REGEN_SOUND_TIME;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: implemented by subclasses to give them an opportunity to make
//			a sound when they lose their enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine::LostEnemySound( void)
{
	if ( gpGlobals->curtime <= m_flNextLostSoundTime )
		return;

#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	if (SpeakIfAllowed( TLK_CMB_LOSTENEMY, UTIL_VarArgs("lastseenenemy:%d", GetEnemyLastTimeSeen()) ))
	{
		m_flNextLostSoundTime = gpGlobals->curtime + random->RandomFloat(5.0,15.0);
	}
#else
	const char *pSentence;
	if (!(CBaseEntity*)GetEnemy() || gpGlobals->curtime - GetEnemyLastTimeSeen() > 10)
	{
		pSentence = "COMBINE_LOST_LONG"; 
	}
	else
	{
		pSentence = "COMBINE_LOST_SHORT";
	}

	if ( m_Sentences.Speak( pSentence ) >= 0 )
	{
		m_flNextLostSoundTime = gpGlobals->curtime + random->RandomFloat(5.0,15.0);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: implemented by subclasses to give them an opportunity to make
//			a sound when they lose their enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine::FoundEnemySound( void)
{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	SpeakIfAllowed( TLK_CMB_REFINDENEMY, SENTENCE_PRIORITY_HIGH );
#else
	m_Sentences.Speak( "COMBINE_REFIND_ENEMY", SENTENCE_PRIORITY_HIGH );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Implemented by subclasses to give them an opportunity to make
//			a sound before they attack
// Input  :
// Output :
//-----------------------------------------------------------------------------

// BUGBUG: It looks like this is never played because combine don't do SCHED_WAKE_ANGRY or anything else that does a TASK_SOUND_WAKE
void CNPC_Combine::AlertSound( void)
{
	if ( gpGlobals->curtime > m_flNextAlertSoundTime )
	{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
		SpeakIfAllowed( TLK_CMB_GOALERT, SENTENCE_PRIORITY_HIGH );
#else
		m_Sentences.Speak( "COMBINE_GO_ALERT", SENTENCE_PRIORITY_HIGH );
#endif
		m_flNextAlertSoundTime = gpGlobals->curtime + 10.0f;
	}
}

//=========================================================
// NotifyDeadFriend
//=========================================================
void CNPC_Combine::NotifyDeadFriend ( CBaseEntity* pFriend )
{
#ifdef EZ
	// Blixibon - Manhack handling
	if ( pFriend == m_hManhack )
	{
		//m_Sentences.Speak( "METROPOLICE_MANHACK_KILLED", SENTENCE_PRIORITY_NORMAL, SENTENCE_CRITERIA_NORMAL );

		// This uses a "COP" concept from npc_metropolice.h
		SpeakIfAllowed( TLK_COP_MANHACKKILLED, "my_manhack:1", SENTENCE_PRIORITY_NORMAL, SENTENCE_CRITERIA_NORMAL );

		DevMsg("My manhack died!\n");
		m_hManhack = NULL;
		return;
	}

	// No notifications for squadmates' dead manhacks
	if ( FClassnameIs( pFriend, "npc_manhack" ) )
		return;
#endif
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	if ( GetSquad()->NumMembers() < 2 )
	{
		m_Sentences.Speak( "COMBINE_LAST_OF_SQUAD", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_NORMAL );
		JustMadeSound();
		return;
	}
#endif
	// relaxed visibility test so that guys say this more often
	//if( FInViewCone( pFriend ) && FVisible( pFriend ) )
	{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
#ifdef EZ2
		SetSpeechTarget( pFriend );
#endif
		SpeakIfAllowed( TLK_CMB_MANDOWN );
#else
		m_Sentences.Speak( "COMBINE_MAN_DOWN" );
#endif
	}
	BaseClass::NotifyDeadFriend(pFriend);
}

//=========================================================
// DeathSound 
//=========================================================
#ifdef MAPBASE
void CNPC_Combine::DeathSound ( const CTakeDamageInfo &info )
#else
void CNPC_Combine::DeathSound ( void )
#endif
{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	AI_CriteriaSet set;
	ModifyOrAppendDamageCriteria(set, info);
	SpeakIfAllowed(TLK_CMB_DIE, set, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS);
#else
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	m_Sentences.Speak( "COMBINE_DIE", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
#endif
}

//=========================================================
// IdleSound 
//=========================================================
void CNPC_Combine::IdleSound( void )
{
	if (g_fCombineQuestion || random->RandomInt(0,1))
	{
		if (!g_fCombineQuestion)
		{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
			int iRandom = random->RandomInt(0, 2);
			SpeakIfAllowed( TLK_CMB_QUESTION, UTIL_VarArgs("combinequestion:%d", iRandom) );
			g_fCombineQuestion = iRandom + 1;
#else
			// ask question or make statement
			switch (random->RandomInt(0,2))
			{
			case 0: // check in
				if ( m_Sentences.Speak( "COMBINE_CHECK" ) >= 0 )
				{
					g_fCombineQuestion = 1;
				}
				break;

			case 1: // question
				if ( m_Sentences.Speak( "COMBINE_QUEST" ) >= 0 )
				{
					g_fCombineQuestion = 2;
				}
				break;

			case 2: // statement
				m_Sentences.Speak( "COMBINE_IDLE" );
				break;
			}
#endif
		}
		else
		{
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
			SpeakIfAllowed( TLK_CMB_ANSWER, UTIL_VarArgs("combinequestion:%d", g_fCombineQuestion) );
			g_fCombineQuestion = 0;
#else
			switch (g_fCombineQuestion)
			{
			case 1: // check in
				if ( m_Sentences.Speak( "COMBINE_CLEAR" ) >= 0 )
				{
					g_fCombineQuestion = 0;
				}
				break;
			case 2: // question 
				if ( m_Sentences.Speak( "COMBINE_ANSWER" ) >= 0 )
				{
					g_fCombineQuestion = 0;
				}
				break;
			}
#endif
		}
	}
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
int	CNPC_Combine::RangeAttack2Conditions( float flDot, float flDist ) 
{
	return COND_NONE;
}

#ifndef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Return true if the combine has grenades, hasn't checked lately, and
//			can throw a grenade at the target point.
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Combine::CanThrowGrenade( const Vector &vecTarget )
{
	if( m_iNumGrenades < 1 )
	{
		// Out of grenades!
		return false;
	}

	if (gpGlobals->curtime < m_flNextGrenadeCheck )
	{
		// Not allowed to throw another grenade right now.
		return false;
	}

	float flDist;
	flDist = ( vecTarget - GetAbsOrigin() ).Length();

	if( flDist > 1024 || flDist < 128 )
	{
		// Too close or too far!
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return false;
	}

	// -----------------------
	// If moving, don't check.
	// -----------------------
	if ( m_flGroundSpeed != 0 )
		return false;

#if 0
	Vector vecEnemyLKP = GetEnemyLKP();
	if ( !( GetEnemy()->GetFlags() & FL_ONGROUND ) && GetEnemy()->GetWaterLevel() == 0 && vecEnemyLKP.z > (GetAbsOrigin().z + WorldAlignMaxs().z)  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		return COND_NONE;
	}
#endif

	// ---------------------------------------------------------------------
	// Are any of my squad members near the intended grenade impact area?
	// ---------------------------------------------------------------------
	if ( m_pSquad )
	{
		if (m_pSquad->SquadMemberInRange( vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST ))
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.

			// Tell my squad members to clear out so I can get a grenade in
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY | SOUND_CONTEXT_COMBINE_ONLY, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST, 0.1 );
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
bool CNPC_Combine::CheckCanThrowGrenade( const Vector &vecTarget )
{
	//NDebugOverlay::Line( EyePosition(), vecTarget, 0, 255, 0, false, 5 );

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	// FIXME: this is only valid for hand grenades, not RPG's
	Vector vecToss;
	Vector vecMins = -Vector(4,4,4);
	Vector vecMaxs = Vector(4,4,4);
	if( FInViewCone( vecTarget ) && CBaseEntity::FVisible( vecTarget ) )
	{
		vecToss = VecCheckThrow( this, EyePosition(), vecTarget, COMBINE_GRENADE_THROW_SPEED, 1.0, &vecMins, &vecMaxs );
	}
	else
	{
		// Have to try a high toss. Do I have enough room?
		trace_t tr;
		AI_TraceLine( EyePosition(), EyePosition() + Vector( 0, 0, 64 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction != 1.0 )
		{
			return false;
		}

		vecToss = VecCheckToss( this, EyePosition(), vecTarget, -1, 1.0, true, &vecMins, &vecMaxs );
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
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::CanAltFireEnemy( bool bUseFreeKnowledge )
{
#ifdef MAPBASE
	if ( !IsAltFireCapable() )
#else
	if ( !IsElite() )
#endif
		return false;

	if (IsCrouching())
		return false;

	if( gpGlobals->curtime < m_flNextAltFireTime )
		return false;

	if( !GetEnemy() )
		return false;

	if (gpGlobals->curtime < m_flNextGrenadeCheck )
		return false;

	// See Steve Bond if you plan on changing this next piece of code!! (SJB) EP2_OUTLAND_10
	if (m_iNumGrenades < 1)
		return false;

	CBaseEntity *pEnemy = GetEnemy();

#ifdef MAPBASE
	// "Our weapons alone cannot take down the antlion guard!"
	// "Wait, you're an elite, don't you have, like, disintegration balls or somethi--"
	// "SHUT UP!"
	if ( !npc_combine_altfire_not_allies_only.GetBool() && !pEnemy->IsPlayer() && (!pEnemy->IsNPC() || !pEnemy->MyNPCPointer()->IsPlayerAlly()) )
#else
	if( !pEnemy->IsPlayer() && (!pEnemy->IsNPC() || !pEnemy->MyNPCPointer()->IsPlayerAlly()) )
#endif
		return false;

	Vector vecTarget;

	// Determine what point we're shooting at
	if( bUseFreeKnowledge )
	{
		vecTarget = GetEnemies()->LastKnownPosition( pEnemy ) + (pEnemy->GetViewOffset()*0.75);// approximates the chest
	}
	else
	{
		vecTarget = GetEnemies()->LastSeenPosition( pEnemy ) + (pEnemy->GetViewOffset()*0.75);// approximates the chest
	}

	// Trace a hull about the size of the combine ball (don't shoot through grates!)
	trace_t tr;

	Vector mins( -12, -12, -12 );
	Vector maxs( 12, 12, 12 );

	Vector vShootPosition = EyePosition();

	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->GetAttachment( "muzzle", vShootPosition );
	}

	// Trace a hull about the size of the combine ball.
#ifdef MAPBASE
	UTIL_TraceHull( vShootPosition, vecTarget, mins, maxs, MASK_COMBINE_BALL_LOS, this, COLLISION_GROUP_NONE, &tr );
#else
	UTIL_TraceHull( vShootPosition, vecTarget, mins, maxs, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
#endif

	float flLength = (vShootPosition - vecTarget).Length();

	flLength *= tr.fraction;

	//If the ball can travel at least 65% of the distance to the player then let the NPC shoot it.
#ifdef MAPBASE
	// (unless it hit the world)
	if( tr.fraction >= 0.65 && (!tr.m_pEnt || !tr.m_pEnt->IsWorld()) && flLength > 128.0f )
#else
	if( tr.fraction >= 0.65 && flLength > 128.0f )
#endif
	{
		// Target is valid
		m_vecAltFireTarget = vecTarget;
		return true;
	}


	// Check again later
	m_vecAltFireTarget = vec3_origin;
	m_flNextGrenadeCheck = gpGlobals->curtime + 1.0f;
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::CanGrenadeEnemy( bool bUseFreeKnowledge )
{
#ifdef MAPBASE
	if ( !IsGrenadeCapable() )
#else
	if ( IsElite() )
#endif
		return false;

	CBaseEntity *pEnemy = GetEnemy();

	Assert( pEnemy != NULL );

	if( pEnemy )
	{
		// I'm not allowed to throw grenades during dustoff
		if ( IsCurSchedule(SCHED_DROPSHIP_DUSTOFF) )
			return false;

#ifdef EZ2
		if (GetEnemy()->ClassMatches( "npc_citizen" ) && GetEnemy()->MyNPCPointer())
		{
			// Grenading unarmed citizens isn't necessary unless they're using a func_tank
			CAI_BaseNPC *pNPC = GetEnemy()->MyNPCPointer();
			if (pNPC->GetActiveWeapon() == NULL &&
				(!pNPC->GetRunningBehavior() || pNPC->GetRunningBehavior()->GetName() != m_FuncTankBehavior.GetName()))
				return false;
		}
#endif

		if( bUseFreeKnowledge )
		{
			// throw to where we think they are.
			return CanThrowGrenade( GetEnemies()->LastKnownPosition( pEnemy ) );
		}
		else
		{
			// hafta throw to where we last saw them.
			return CanThrowGrenade( GetEnemies()->LastSeenPosition( pEnemy ) );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: For combine melee attack (kick/hit)
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Combine::MeleeAttack1Conditions ( float flDot, float flDist )
{
	if (flDist > 64)
	{
		return COND_NONE; // COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return COND_NONE; // COND_NOT_FACING_ATTACK;
	}

	// Check Z
	if ( GetEnemy() && fabs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) > 64 )
		return COND_NONE;

#ifdef EZ
	// Soldiers are incapable of kicking the babies
	if ( GetEnemy() && GetEnemy()->IsNPC() && GetEnemy()->MyNPCPointer()->GetHullType() == HULL_TINY )
#else
	if ( dynamic_cast<CBaseHeadcrab *>(GetEnemy()) != NULL )
#endif
	{
		return COND_NONE;
	}

	// Make sure not trying to kick through a window or something. 
	trace_t tr;
	Vector vecSrc, vecEnd;

	vecSrc = WorldSpaceCenter();
	vecEnd = GetEnemy()->WorldSpaceCenter();

	AI_TraceLine(vecSrc, vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	if( tr.m_pEnt != GetEnemy() )
	{
		return COND_NONE;
	}

	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Combine::EyePosition( void ) 
{
	if ( !IsCrouching() )
	{
		return GetAbsOrigin() + COMBINE_EYE_STANDING_POSITION;
	}
	else
	{
		return GetAbsOrigin() + COMBINE_EYE_CROUCHING_POSITION;
	}

	/*
	Vector m_EyePos;
	GetAttachment( "eyes", m_EyePos );
	return m_EyePos;
	*/
}

#ifndef MAPBASE
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Vector CNPC_Combine::GetAltFireTarget()
{
	Assert( IsElite() );

	return m_vecAltFireTarget;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nActivity - 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Combine::EyeOffset( Activity nActivity )
{
	if (CapabilitiesGet() & bits_CAP_DUCK)
	{
		if ( IsCrouchedActivity( nActivity ) )
			return COMBINE_EYE_CROUCHING_POSITION;

	}
	// if the hint doesn't tell anything, assume current state
	if ( !IsCrouching() )
	{
		return COMBINE_EYE_STANDING_POSITION;
	}
	else
	{
		return COMBINE_EYE_CROUCHING_POSITION;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CNPC_Combine::GetCrouchEyeOffset( void )
{
	return COMBINE_EYE_CROUCHING_POSITION;
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsCrouchedActivity( Activity activity )
{
	if (BaseClass::IsCrouchedActivity( activity ))
		return true;

	Activity realActivity = TranslateActivity(activity);

	// Soldiers need to consider these crouched activities, but not all NPCs should.
	switch ( realActivity )
	{
		case ACT_RANGE_AIM_LOW:
		case ACT_RANGE_AIM_AR2_LOW:
		case ACT_RANGE_AIM_SMG1_LOW:
		case ACT_RANGE_AIM_PISTOL_LOW:
		case ACT_RANGE_ATTACK1_LOW:
		case ACT_RANGE_ATTACK_AR2_LOW:
		case ACT_RANGE_ATTACK_SMG1_LOW:
		case ACT_RANGE_ATTACK_SHOTGUN_LOW:
		case ACT_RANGE_ATTACK_PISTOL_LOW:
		case ACT_RANGE_ATTACK2_LOW:
			return true;
	}

	return false;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::SetActivity( Activity NewActivity )
{
	BaseClass::SetActivity( NewActivity );

#ifndef MAPBASE // CAI_GrenadeUser
	m_iLastAnimEventHandled = -1;
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
NPC_STATE CNPC_Combine::SelectIdealState( void )
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
					SetCondition( COND_COMBINE_SHOULD_PATROL );
				}
				return NPC_STATE_ALERT;
			}
			else if ( HasCondition( COND_ENEMY_DEAD ) )
			{
				AnnounceEnemyKill(GetEnemy());
			}
		}

	default:
		{
			return BaseClass::SelectIdealState();
		}
	}

	return GetIdealState();
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: I want to deploy a manhack. Can I?
//-----------------------------------------------------------------------------
bool CNPC_Combine::CanDeployManhack( void )
{
	// Nope, don't have any!
	if( m_iManhacks < 1 )
		return false;

	if ( HasSpawnFlags( SF_COMBINE_NO_MANHACK_DEPLOY ) )
		return false;

	// Nope, already have one out.
	if( m_hManhack != NULL )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the held manhack solid
//-----------------------------------------------------------------------------
void CNPC_Combine::ReleaseManhack( void )
{
	Assert( m_hManhack );

	// Make us physical
	m_hManhack->RemoveSpawnFlags( SF_MANHACK_CARRIED );
	m_hManhack->CreateVPhysics();

	// Release us
	m_hManhack->RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_hManhack->SetMoveType( MOVETYPE_VPHYSICS );
	m_hManhack->SetParent( NULL );

	// Make us active
	m_hManhack->RemoveSpawnFlags( SF_NPC_WAIT_FOR_SCRIPT );
	m_hManhack->ClearSchedule( "Manhack released by metropolice" );
	
	// Start him with knowledge of our current enemy
	if ( GetEnemy() )
	{
		m_hManhack->SetEnemy( GetEnemy() );
		m_hManhack->SetState( NPC_STATE_COMBAT );

		m_hManhack->UpdateEnemyMemory( GetEnemy(), GetEnemy()->GetAbsOrigin() );
	}

	// Place him into our squad so we can communicate
	if ( m_pSquad )
	{
		m_pSquad->AddToSquad( m_hManhack );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Combine::OnAnimEventStartDeployManhack( void )
{
	Assert( m_iManhacks );
	
	if ( m_iManhacks <= 0 )
	{
		DevMsg( "Error: Throwing manhack but out of manhacks!\n" );
		return;
	}

	m_iManhacks--;

	// Turn off the manhack on our body
	if ( m_iManhacks <= 0 )
	{
		SetBodygroup( COMBINE_BODYGROUP_MANHACK, false );
	}

	// Create the manhack to throw
	CNPC_Manhack *pManhack = (CNPC_Manhack *)CreateEntityByName( "npc_manhack" );
	
	Vector	vecOrigin;
	QAngle	vecAngles;

	int handAttachment = LookupAttachment( "lefthand" );
	GetAttachment( handAttachment, vecOrigin, vecAngles );

	pManhack->SetLocalOrigin( vecOrigin );
	pManhack->SetLocalAngles( vecAngles );
	pManhack->AddSpawnFlags( (SF_MANHACK_PACKED_UP|SF_MANHACK_CARRIED|SF_NPC_WAIT_FOR_SCRIPT) );
	
	// Also fade if our parent is marked to do it
	if ( HasSpawnFlags( SF_NPC_FADE_CORPSE ) )
	{
		pManhack->AddSpawnFlags( SF_NPC_FADE_CORPSE );
	}

	// Allow modification of the manhack
	HandleManhackSpawn( pManhack );

	DispatchSpawn( pManhack );

	// Make us move with his hand until we're deployed
	pManhack->SetParent( this, handAttachment );

	m_hManhack = pManhack;

	m_OutManhack.Set(m_hManhack, pManhack, this);
}

//-----------------------------------------------------------------------------
// Anim event handlers
//-----------------------------------------------------------------------------
void CNPC_Combine::OnAnimEventDeployManhack( animevent_t *pEvent )
{
	if (!m_hManhack)
		return;

	// Let it go
	ReleaseManhack();

	Vector forward, right;
	GetVectors( &forward, &right, NULL );

	IPhysicsObject *pPhysObj = m_hManhack->VPhysicsGetObject();

	if ( pPhysObj )
	{
		Vector	yawOff = right * random->RandomFloat( -1.0f, 1.0f );

		Vector	forceVel = ( forward + yawOff * 16.0f ) + Vector( 0, 0, 250 );
		Vector	forceAng = vec3_origin;

		// Give us velocity
		pPhysObj->AddVelocity( &forceVel, &forceAng );
	}

	// Stop dealing with this manhack
	//m_hManhack = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine::OnScheduleChange()
{
	BaseClass::OnScheduleChange();

	// Ensures we deploy the manhack
	if ( m_hManhack && m_hManhack->GetParent() == this )
	{
		OnAnimEventDeployManhack( NULL );
	}

#ifdef EZ2
	if (!HasCondition( COND_COMBINE_OBSTRUCTED ))
	{
		m_hObstructor = NULL;
	}
#endif
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::OnBeginMoveAndShoot()
{
	if ( BaseClass::OnBeginMoveAndShoot() )
	{
#ifdef EZ
		if ( IsMajorCharacter() )
			return true; // Commandable Combine soldiers don't need a squad slot to move and shoot
#endif

		if( HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true; // already have the slot I need

		if( !HasStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_ATTACK_OCCLUDER ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Combine::OnEndMoveAndShoot()
{
	VacateStrategySlot();
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Blixibon - Soldiers need to be locked onto their enemies or else they end up looking in odd-looking directions.
//-----------------------------------------------------------------------------
bool CNPC_Combine::PickTacticalLookTarget( AILookTargetArgs_t *pArgs )
{
	if( GetState() == NPC_STATE_COMBAT )
	{
		CBaseEntity *pEnemy = GetEnemy();
		if ( pEnemy && FVisible( pEnemy ) && ValidHeadTarget(pEnemy->EyePosition()) )
		{
			// Look at the enemy if possible.
			pArgs->hTarget = pEnemy;
			pArgs->flInfluence = random->RandomFloat( 0.8, 1.0 );
			pArgs->flRamp = 0;
		}
		else
		{
			// Look at yourself instead. We can't be looking in random directions.
			pArgs->hTarget = this;
			pArgs->flInfluence = random->RandomFloat( 0.8, 1.0 );
			pArgs->flRamp = 0;
		}

		return true;
	}
	else
	{
		return BaseClass::PickTacticalLookTarget( pArgs );
	}
}

//-----------------------------------------------------------------------------
// Blixibon - Hack to kick in the above look target code as soon as we're in combat
//-----------------------------------------------------------------------------
void CNPC_Combine::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );

	if (NewState == NPC_STATE_COMBAT)
	{
		// Pick a new look target
		ExpireCurrentRandomLookTarget();
	}
}

//-----------------------------------------------------------------------------
// Blixibon - Part of getting Combine soldiers to look at visually interesting hints
//-----------------------------------------------------------------------------
void CNPC_Combine::AimGun()
{
	BaseClass::AimGun();

	// CNPC_PlayerCompanion's AimGun() handling makes this safe.
	if (!GetEnemy() && GetAimTarget())
	{
		// There's probably a better way to do this I'm missing, but this allows soldiers to face their aim targets.
		// This has 0.75 importance so other facing calls take priority.
		AddFacingTarget(GetAimTarget(), 0.75f, 0.75f);
	}
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_Combine::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
#ifdef MAPBASE
	if( pWeapon->ClassMatches( gm_isz_class_AR2 ) )
#else
	if( FClassnameIs( pWeapon, "weapon_ar2" ) )
#endif
	{
		if( hl2_episodic.GetBool() )
		{
			return WEAPON_PROFICIENCY_VERY_GOOD;
		}
		else
		{
			return WEAPON_PROFICIENCY_GOOD;
		}
	}
#ifdef MAPBASE
	else if( pWeapon->ClassMatches( gm_isz_class_Shotgun ) )
#else
	else if( FClassnameIs( pWeapon, "weapon_shotgun" )	)
#endif
	{
#ifndef MAPBASE // Moved so soldiers don't change skin unnaturally and uncontrollably
		if( m_nSkin != COMBINE_SKIN_SHOTGUNNER )
		{
			m_nSkin = COMBINE_SKIN_SHOTGUNNER;
		}
#endif

		return WEAPON_PROFICIENCY_PERFECT;
	}
#ifdef MAPBASE
	else if( pWeapon->ClassMatches( gm_isz_class_SMG1 ) )
#else
	else if( FClassnameIs( pWeapon, "weapon_smg1" ) )
#endif
	{
		return WEAPON_PROFICIENCY_GOOD;
	}
#ifdef MAPBASE
	else if ( pWeapon->ClassMatches( gm_isz_class_Pistol ) )
	{
		// Mods which need a lower soldier pistol accuracy can either change this value or use proficiency override in Hammer.
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}
#endif

#ifdef EZ
	else if (FClassnameIs( pWeapon, "weapon_smg2" ))
	{
		return WEAPON_PROFICIENCY_GOOD;
	}
	else if (FClassnameIs( pWeapon, "weapon_pulsepistol" ))
	{
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}
#endif

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::HasShotgun()
{
	if( GetActiveWeapon() && GetActiveWeapon()->m_iClassname == s_iszShotgunClassname )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Only supports weapons that use clips.
//-----------------------------------------------------------------------------
bool CNPC_Combine::ActiveWeaponIsFullyLoaded()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();

	if( !pWeapon )
		return false;

	if( !pWeapon->UsesClipsForAmmo1() )
		return false;

	return ( pWeapon->Clip1() >= pWeapon->GetMaxClip1() );
}


//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  The type of interaction, extra info pointer, and who started it
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_Combine::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt)
{
	if ( interactionType == g_interactionTurretStillStanding )
	{
		// A turret that I've kicked recently is still standing 5 seconds later. 
		if ( sourceEnt == GetEnemy() )
		{
			// It's still my enemy. Time to grenade it.
			Vector forward, up;
			AngleVectors( GetLocalAngles(), &forward, NULL, &up );
			m_vecTossVelocity = forward * 10;
			SetCondition( COND_COMBINE_DROP_GRENADE );
			ClearSchedule( "Failed to kick over turret" );
		}
		return true;
	}
#ifdef EZ
	// Blixibon - Combine soldiers should act weird if you bound stuff off of them
	else if (interactionType == g_interactionHitByPlayerThrownPhysObj)
	{
		CBaseEntity *pTarget = UTIL_GetLocalPlayer();
		if (pTarget)
		{
			CSoundEnt::InsertSound(SOUND_PLAYER | SOUND_CONTEXT_COMBINE_ONLY, pTarget->GetAbsOrigin(), 128, 0.1, pTarget);
		}
	}
#endif
	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}
#ifdef EZ
//-----------------------------------------------------------------------------
// Blixibon - Allows Combine soldiers to use visually interesting hints.
//-----------------------------------------------------------------------------
bool CNPC_Combine::FValidateHintType( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
	case HINT_WORLD_VISUALLY_INTERESTING:
		return true;
		break;

	default:
		break;
	}

	return BaseClass::FValidateHintType( pHint );
}
#endif
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char* CNPC_Combine::GetSquadSlotDebugName( int iSquadSlot )
{
	switch( iSquadSlot )
	{
	case SQUAD_SLOT_GRENADE1:			return "SQUAD_SLOT_GRENADE1";	
		break;
	case SQUAD_SLOT_GRENADE2:			return "SQUAD_SLOT_GRENADE2";	
		break;
	case SQUAD_SLOT_ATTACK_OCCLUDER:	return "SQUAD_SLOT_ATTACK_OCCLUDER";	
		break;
	case SQUAD_SLOT_OVERWATCH:			return "SQUAD_SLOT_OVERWATCH";
		break;
	}

	return BaseClass::GetSquadSlotDebugName( iSquadSlot );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsUsingTacticalVariant( int variant )
{
	if( variant == TACTICAL_VARIANT_PRESSURE_ENEMY && m_iTacticalVariant == TACTICAL_VARIANT_PRESSURE_ENEMY_UNTIL_CLOSE )
	{
		// Essentially, fib. Just say that we are a 'pressure enemy' soldier.
		return true;
	}

	return m_iTacticalVariant == variant;
}

//-----------------------------------------------------------------------------
// For the purpose of determining whether to use a pathfinding variant, this
// function determines whether the current schedule is a schedule that 
// 'approaches' the enemy. 
//-----------------------------------------------------------------------------
bool CNPC_Combine::IsRunningApproachEnemySchedule()
{
	if( IsCurSchedule( SCHED_CHASE_ENEMY ) )
		return true;

	if( IsCurSchedule( SCHED_ESTABLISH_LINE_OF_FIRE ) )
		return true;

	if( IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK, false ) )
		return true;

	return false;
}

bool CNPC_Combine::ShouldPickADeathPose( void ) 
{ 
#ifdef MAPBASE
	// Check base class as well
	return !IsCrouching() && BaseClass::ShouldPickADeathPose();
#else
	return !IsCrouching(); 
#endif
}

#ifdef EZ
ConVar npc_combine_new_follow_behavior( "npc_combine_new_follow_behavior", "1" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Combine::CCombineFollowBehavior::SelectSchedule( void )
{
	int result = SCHED_NONE;

	// We could just make them use SelectScheduleAttack() like standoff behavior does, which makes soldiers
	// only use special schedules like grenade-throwing or manhack deployment, avoiding AI which would
	// otherwise interfere with the running behavior. 
	// 
	// This is what we were doing before, with the logic being that soldiers should always follow the player.
	// 
	// However, players were disappointed that they weren't using their signature "Combine AI" and were basically
	// just acting like reskinned citizens. As a result, we've expanded follow behavior to utilize all Combine
	// combat schedules, including those that would take cover and otherwise give them plenty of freedom over
	// the player's orders.
	// 
	// -Blixibon
	if (npc_combine_new_follow_behavior.GetBool())
	{
		if ( GetOuterS()->GetState() == NPC_STATE_COMBAT && IsFollowTargetInRange(1.5f) )
			result = GetOuterS()->SelectCombatSchedule();
	}
	else
	{
		result = GetOuterS()->SelectScheduleAttack();
	}

	if ( result == SCHED_NONE || result == SCHED_RANGE_ATTACK1 )
		result = BaseClass::SelectSchedule();
	return result;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Combine::CCombineFollowBehavior::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_COMBINE_TAKE_COVER1:

			if ( ShouldMoveToFollowTarget() && !IsFollowGoalInRange( GetGoalRange(), GetGoalZRange(), GetGoalFlags() ) )
			{
				return SCHED_MOVE_TO_FACE_FOLLOW_TARGET;			
			}
			
			break;
	}
	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Combine::CCombineFollowBehavior::PlayerIsPushing()
{
	if (!BaseClass::PlayerIsPushing())
		return false;

	// HACKHACK: By default, follow behavior uses its own push code
	// that acts independent of CNPC_PlayerCompanion.
	// Combine soldiers in the player's squad need to avoid moving
	// away when players are trying to give them a new weapon.
	if (GetOuterS()->IgnorePlayerPushing())
		return false;

	return true;
}
#endif

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_combine, CNPC_Combine )

//Tasks
DECLARE_TASK( TASK_COMBINE_FACE_TOSS_DIR )
DECLARE_TASK( TASK_COMBINE_IGNORE_ATTACKS )
DECLARE_TASK( TASK_COMBINE_SIGNAL_BEST_SOUND )
DECLARE_TASK( TASK_COMBINE_DEFER_SQUAD_GRENADES )
DECLARE_TASK( TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY )
DECLARE_TASK( TASK_COMBINE_DIE_INSTANTLY )
DECLARE_TASK( TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET )
DECLARE_TASK( TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS )
DECLARE_TASK( TASK_COMBINE_SET_STANDING )

//Activities
#if !SHARED_COMBINE_ACTIVITIES
DECLARE_ACTIVITY( ACT_COMBINE_THROW_GRENADE )
#endif
DECLARE_ACTIVITY( ACT_COMBINE_LAUNCH_GRENADE )
DECLARE_ACTIVITY( ACT_COMBINE_BUGBAIT )
#if !SHARED_COMBINE_ACTIVITIES
DECLARE_ACTIVITY( ACT_COMBINE_AR2_ALTFIRE )
#endif
DECLARE_ACTIVITY( ACT_WALK_EASY )
DECLARE_ACTIVITY( ACT_WALK_MARCH )
#ifdef MAPBASE
DECLARE_ACTIVITY( ACT_TURRET_CARRY_IDLE )
DECLARE_ACTIVITY( ACT_TURRET_CARRY_WALK )
DECLARE_ACTIVITY( ACT_TURRET_CARRY_RUN )
#endif
#ifdef EZ
DECLARE_ACTIVITY( ACT_METROPOLICE_DEPLOY_MANHACK )
#endif

DECLARE_ANIMEVENT( COMBINE_AE_BEGIN_ALTFIRE )
DECLARE_ANIMEVENT( COMBINE_AE_ALTFIRE )
#ifdef EZ
DECLARE_ANIMEVENT( AE_METROPOLICE_START_DEPLOY );
DECLARE_ANIMEVENT( AE_METROPOLICE_DEPLOY_MANHACK );
#endif

DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE1 )
DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE2 )

DECLARE_CONDITION( COND_COMBINE_NO_FIRE )
DECLARE_CONDITION( COND_COMBINE_DEAD_FRIEND )
DECLARE_CONDITION( COND_COMBINE_SHOULD_PATROL )
DECLARE_CONDITION( COND_COMBINE_HIT_BY_BUGBAIT )
DECLARE_CONDITION( COND_COMBINE_DROP_GRENADE )
DECLARE_CONDITION( COND_COMBINE_ON_FIRE )
DECLARE_CONDITION( COND_COMBINE_ATTACK_SLOT_AVAILABLE )
#ifdef EZ2
DECLARE_CONDITION( COND_COMBINE_CAN_ORDER_SURRENDER )
DECLARE_CONDITION( COND_COMBINE_OBSTRUCTED )
#endif

DECLARE_INTERACTION( g_interactionCombineBash );
#ifdef EZ2
DECLARE_INTERACTION( g_interactionBadCopKick );
#endif


//=========================================================
// SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND
//
//	hide from the loudest sound source (to run from grenade)
//=========================================================
DEFINE_SCHEDULE	
(
 SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND,

 "	Tasks"
 "		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND"
 "		 TASK_STOP_MOVING					0"
 "		 TASK_COMBINE_SIGNAL_BEST_SOUND		0"
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
 SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND,

 "	Tasks"
 "		 TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_COWER"
 "		 TASK_GET_PATH_AWAY_FROM_BEST_SOUND		600"
 "		 TASK_RUN_PATH_TIMED					2"
 "		 TASK_STOP_MOVING						0"
 ""
 "	Interrupts"
 )
 //=========================================================
 //	SCHED_COMBINE_COMBAT_FAIL
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_COMBAT_FAIL,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE "
 "		TASK_WAIT_FACE_ENEMY		2"
 "		TASK_WAIT_PVS				0"
 ""
 "	Interrupts"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 )

 //=========================================================
 // SCHED_COMBINE_VICTORY_DANCE
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_VICTORY_DANCE,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_FACE_ENEMY						0"
 "		TASK_WAIT							1.5"
 "		TASK_GET_PATH_TO_ENEMY_CORPSE		0"
 "		TASK_WALK_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_FACE_ENEMY						0"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_VICTORY_DANCE"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 )

 //=========================================================
 // SCHED_COMBINE_ASSAULT
 //=========================================================
 DEFINE_SCHEDULE 
 (
 SCHED_COMBINE_ASSAULT,

 "	Tasks "
 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE"
 "		TASK_SET_TOLERANCE_DISTANCE		48"
 "		TASK_GET_PATH_TO_ENEMY_LKP		0"
 "		TASK_COMBINE_IGNORE_ATTACKS		0.2"
 "		TASK_SPEAK_SENTENCE				0"
 "		TASK_RUN_PATH					0"
 //		"		TASK_COMBINE_MOVE_AND_AIM		0"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
 ""
 "	Interrupts "
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TOO_FAR_TO_ATTACK"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 )

 DEFINE_SCHEDULE 
 (
 SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE,

 "	Tasks "
 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_ESTABLISH_LINE_OF_FIRE"
 "		TASK_SET_TOLERANCE_DISTANCE		48"
 "		TASK_GET_PATH_TO_ENEMY_LKP_LOS	0"
 "		TASK_COMBINE_SET_STANDING		1"
 "		TASK_SPEAK_SENTENCE				1"
 "		TASK_RUN_PATH					0"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
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
 // SCHED_COMBINE_PRESS_ATTACK
 //=========================================================
 DEFINE_SCHEDULE 
 (
 SCHED_COMBINE_PRESS_ATTACK,

 "	Tasks "
 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE"
 "		TASK_SET_TOLERANCE_DISTANCE		72"
 "		TASK_GET_PATH_TO_ENEMY_LKP		0"
 "		TASK_COMBINE_SET_STANDING		1"
 "		TASK_RUN_PATH					0"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 ""
 "	Interrupts "
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
 // SCHED_COMBINE_COMBAT_FACE
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_COMBAT_FACE,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
 "		TASK_FACE_ENEMY				0"
 "		 TASK_WAIT					1.5"
 //"		 TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_SWEEP"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_CAN_RANGE_ATTACK2"
 )

 //=========================================================
 // 	SCHED_HIDE_AND_RELOAD	
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_HIDE_AND_RELOAD,

 "	Tasks"
 "		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_RELOAD"
 "		TASK_FIND_COVER_FROM_ENEMY	0"
 "		TASK_RUN_PATH				0"
 "		TASK_WAIT_FOR_MOVEMENT		0"
 "		TASK_REMEMBER				MEMORY:INCOVER"
 "		TASK_FACE_ENEMY				0"
 "		TASK_RELOAD					0"
 ""
 "	Interrupts"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_HEAVY_DAMAGE"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 )

 //=========================================================
 // SCHED_COMBINE_SIGNAL_SUPPRESS
 //	don't stop shooting until the clip is
 //	empty or combine gets hurt.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_SIGNAL_SUPPRESS,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_IDEAL					0"
 "		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_SIGNAL_GROUP"
 "		TASK_COMBINE_SET_STANDING		0"
 "		TASK_RANGE_ATTACK1				0"
 ""
 "	Interrupts"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_WEAPON_BLOCKED_BY_FRIEND"
 "		COND_WEAPON_SIGHT_OCCLUDED"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_COMBINE_NO_FIRE"
 )

 //=========================================================
 // SCHED_COMBINE_SUPPRESS
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_SUPPRESS,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_FACE_ENEMY				0"
 "		TASK_COMBINE_SET_STANDING	0"
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
 // 	SCHED_COMBINE_ENTER_OVERWATCH
 //
 // Parks a combine soldier in place looking at the player's
 // last known position, ready to attack if the player pops out
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_ENTER_OVERWATCH,

 "	Tasks"
 "		TASK_STOP_MOVING			0"
 "		TASK_COMBINE_SET_STANDING	0"
 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
 "		TASK_FACE_ENEMY				0"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_OVERWATCH"
 ""
 "	Interrupts"
 "		COND_HEAR_DANGER"
 "		COND_NEW_ENEMY"
 )

 //=========================================================
 // 	SCHED_COMBINE_OVERWATCH
 //
 // Parks a combine soldier in place looking at the player's
 // last known position, ready to attack if the player pops out
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_OVERWATCH,

 "	Tasks"
 "		TASK_WAIT_FACE_ENEMY		10"
 ""
 "	Interrupts"
 "		COND_CAN_RANGE_ATTACK1"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 "		COND_NO_PRIMARY_AMMO"
 "		COND_HEAR_DANGER"
 "		COND_HEAR_MOVE_AWAY"
 "		COND_NEW_ENEMY"
 )

 //=========================================================
 // SCHED_COMBINE_WAIT_IN_COVER
 //	we don't allow danger or the ability
 //	to attack to break a combine's run to cover schedule but
 //	when a combine is in cover we do want them to attack if they can.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_WAIT_IN_COVER,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_COMBINE_SET_STANDING		0"
 "		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"	// Translated to cover
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
 "		COND_COMBINE_ATTACK_SLOT_AVAILABLE"
 )

 //=========================================================
 // SCHED_COMBINE_TAKE_COVER1
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_TAKE_COVER1  ,

 "	Tasks"
 "		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_COMBINE_TAKECOVER_FAILED"
 "		TASK_STOP_MOVING				0"
 "		TASK_WAIT					0.2"
 "		TASK_FIND_COVER_FROM_ENEMY	0"
 "		TASK_RUN_PATH				0"
 "		TASK_WAIT_FOR_MOVEMENT		0"
 "		TASK_REMEMBER				MEMORY:INCOVER"
 "		TASK_SET_SCHEDULE			SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"
 ""
 "	Interrupts"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_TAKECOVER_FAILED,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_GRENADE_COVER1
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_GRENADE_COVER1,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_FIND_COVER_FROM_ENEMY			99"
 "		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY	384"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SPECIAL_ATTACK2"
 "		TASK_CLEAR_MOVE_WAIT				0"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_TOSS_GRENADE_COVER1
 //
 //	 drop grenade then run to cover.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_TOSS_GRENADE_COVER1,

 "	Tasks"
 "		TASK_FACE_ENEMY						0"
 "		TASK_RANGE_ATTACK2 					0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_RANGE_ATTACK1
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_RANGE_ATTACK1,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_ENEMY					0"
 "		TASK_ANNOUNCE_ATTACK			1"	// 1 = primary attack
 "		TASK_WAIT_RANDOM				0.3"
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
 // Enemy_Occluded				Don't interrupt on this.  Means
 //								comibine will fire where player was after
 //								he has moved for a little while.  Good effect!!
 // WEAPON_SIGHT_OCCLUDED		Don't block on this! Looks better for railings, etc.
 )

 //=========================================================
 // AR2 Alt Fire Attack
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_AR2_ALTFIRE,

 "	Tasks"
 "		TASK_STOP_MOVING									0"
 "		TASK_ANNOUNCE_ATTACK								1"
 "		TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET		ACTIVITY:ACT_COMBINE_AR2_ALTFIRE"
 ""
 "	Interrupts"
 )

 //=========================================================
 // Mapmaker forced grenade throw
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_FORCED_GRENADE_THROW,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 ""
 "	Interrupts"
 )

 //=========================================================
 // Move to LOS of the mapmaker's forced grenade throw target
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_MOVE_TO_FORCED_GREN_LOS,

 "	Tasks "
 "		TASK_SET_TOLERANCE_DISTANCE					48"
 "		TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS	0"
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
 // 	SCHED_COMBINE_RANGE_ATTACK2	
 //
 //	secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
 //	combines's grenade toss requires the enemy be occluded.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_RANGE_ATTACK2,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_COMBINE_WAIT_IN_COVER"	// don't run immediately after throwing grenade.
 ""
 "	Interrupts"
 )


 //=========================================================
 // Throw a grenade, then run off and reload.
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_GRENADE_AND_RELOAD,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_COMBINE_FACE_TOSS_DIR			0"
 "		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_HIDE_AND_RELOAD"	// don't run immediately after throwing grenade.
 ""
 "	Interrupts"
 )

 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_PATROL,

 "	Tasks"
 "		TASK_STOP_MOVING				0"
 "		TASK_WANDER						900540" 
 "		TASK_WALK_PATH					0"
 "		TASK_WAIT_FOR_MOVEMENT			0"
 "		TASK_STOP_MOVING				0"
 "		TASK_FACE_REASONABLE			0"
 "		TASK_WAIT						3"
 "		TASK_WAIT_RANDOM				3"
 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBINE_PATROL" // keep doing it
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

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_BUGBAIT_DISTRACTION,

 "	Tasks"
 "		TASK_STOP_MOVING		0"
 "		TASK_RESET_ACTIVITY		0"
 "		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_COMBINE_BUGBAIT"
 ""
 "	Interrupts"
 ""
 )

 //=========================================================
 // SCHED_COMBINE_CHARGE_TURRET
 //
 //	Used to run straight at enemy turrets to knock them over.
 //  Prevents squadmates from throwing grenades during.
 //=========================================================
#ifdef EZ
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_CHARGE_TURRET,

 "	Tasks"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_STOP_MOVING					0"
 "		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
 "		TASK_GET_CHASE_PATH_TO_ENEMY		300"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_FACE_ENEMY						0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TOO_CLOSE_TO_ATTACK"
 "		COND_TASK_FAILED"
 "		COND_LOST_ENEMY"
 "		COND_BETTER_WEAPON_AVAILABLE"
 "		COND_HEAR_DANGER"
 "		COND_ENEMY_FACING_ME" // Blixibon - Makes soldiers stop charging a turret if the turret starts facing them (fixes a problem in ez2_c5_2)
 )
#else
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_CHARGE_TURRET,

 "	Tasks"
 "		TASK_COMBINE_DEFER_SQUAD_GRENADES	0"
 "		TASK_STOP_MOVING					0"
 "		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
 "		TASK_GET_CHASE_PATH_TO_ENEMY		300"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 "		TASK_FACE_ENEMY						0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TOO_CLOSE_TO_ATTACK"
 "		COND_TASK_FAILED"
 "		COND_LOST_ENEMY"
 "		COND_BETTER_WEAPON_AVAILABLE"
 "		COND_HEAR_DANGER"
 )
#endif

 //=========================================================
 // SCHED_COMBINE_CHARGE_PLAYER
 //
 //	Used to run straight at enemy player since physgun combat
 //  is more fun when the enemies are close
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_CHARGE_PLAYER,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
 "		TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY		192"
 "		TASK_FACE_ENEMY						0"
 ""
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_ENEMY_UNREACHABLE"
 "		COND_CAN_MELEE_ATTACK1"
 "		COND_CAN_MELEE_ATTACK2"
 "		COND_TASK_FAILED"
 "		COND_LOST_ENEMY"
 "		COND_HEAR_DANGER"
 )

 //=========================================================
 // SCHED_COMBINE_DROP_GRENADE
 //
 //	Place a grenade at my feet
 //=========================================================
 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_DROP_GRENADE,

 "	Tasks"
 "		TASK_STOP_MOVING					0"
 "		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SPECIAL_ATTACK2"
 "		TASK_FIND_COVER_FROM_ENEMY			99"
 "		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY	384"
 "		TASK_CLEAR_MOVE_WAIT				0"
 "		TASK_RUN_PATH						0"
 "		TASK_WAIT_FOR_MOVEMENT				0"
 ""
 "	Interrupts"
 )

 //=========================================================
 // SCHED_COMBINE_PATROL_ENEMY
 //
 // Used instead if SCHED_COMBINE_PATROL if I have an enemy.
 // Wait for the enemy a bit in the hopes of ambushing him.
 //=========================================================
 DEFINE_SCHEDULE	
 (
 SCHED_COMBINE_PATROL_ENEMY,

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

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_BURNING_STAND,

 "	Tasks"
 "		TASK_SET_ACTIVITY				ACTIVITY:ACT_COMBINE_BUGBAIT"
 "		TASK_RANDOMIZE_FRAMERATE		20"
 "		TASK_WAIT						2"
 "		TASK_WAIT_RANDOM				3"
 "		TASK_COMBINE_DIE_INSTANTLY		DMG_BURN"
 "		TASK_WAIT						1.0"
 "	"
 "	Interrupts"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_FACE_IDEAL_YAW,

 "	Tasks"
 "		TASK_FACE_IDEAL				0"
 "	"
 "	Interrupts"
 )

 DEFINE_SCHEDULE
 (
 SCHED_COMBINE_MOVE_TO_MELEE,

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

#ifdef EZ
 //=========================================================
 // The uninterruptible portion of this behavior, whereupon 
 // the soldier actually releases the manhack.
 //=========================================================
 DEFINE_SCHEDULE
 (
 	SCHED_COMBINE_DEPLOY_MANHACK,
 
 	"	Tasks"
 	"		TASK_SPEAK_SENTENCE					5"	// METROPOLICE_SENTENCE_DEPLOY_MANHACK
 	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_METROPOLICE_DEPLOY_MANHACK"
 	"	"
 	"	Interrupts"
 	"		COND_RECEIVED_ORDERS"
 )
#endif

#ifdef EZ2
 //=========================================================
 // Soldier orders surrender and plays a sequence
 //=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_COMBINE_ORDER_SURRENDER,
 
 	"	Tasks"
	"		TASK_STOP_MOVING					0"
	"		TASK_FACE_IDEAL						0"
 	"		TASK_SPEAK_SENTENCE					6"	// Order surrender
	"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_SIGNAL_TAKECOVER"
 	"	"
 	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_WENT_NULL"
 )

 //=========================================================
 // Soldier attacks a prop
 //=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_COMBINE_ATTACK_TARGET,
 
 	"	Tasks"
 	"		TASK_GET_PATH_TO_TARGET		0"
 	"		TASK_MOVE_TO_TARGET_RANGE	50"
 	"		TASK_STOP_MOVING			0"
 	"		TASK_FACE_TARGET			0"
 	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_MELEE_ATTACK2"
 	"	"
 	"	Interrupts"
 	"		COND_NEW_ENEMY"
 	"		COND_ENEMY_DEAD"
 )
#endif

 AI_END_CUSTOM_NPC()
