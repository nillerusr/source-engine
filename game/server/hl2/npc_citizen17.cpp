//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The downtrodden citizens of City 17.
//
//=============================================================================//

#include "cbase.h"

#include "npc_citizen17.h"

#include "ammodef.h"
#include "globalstate.h"
#include "soundent.h"
#include "BasePropDoor.h"
#include "weapon_rpg.h"
#include "hl2_player.h"
#include "items.h"


#ifdef HL2MP
#include "hl2mp/weapon_crowbar.h"
#else
#include "weapon_crowbar.h"
#endif

#include "eventqueue.h"

#ifdef MAPBASE
#include "hl2_gamerules.h"
#include "mapbase/GlobalStrings.h"
#include "collisionutils.h"
#endif

#ifdef EZ
#include "IEffects.h"
#include "particle_parse.h"
#include "RagdollBoogie.h"
#endif

#ifdef EZ2
#include "ez2/ez2_player.h"
#include "npc_combine.h"
#endif

#include "ai_squad.h"
#include "ai_pathfinder.h"
#include "ai_route.h"
#include "ai_hint.h"
#include "ai_interactions.h"
#include "ai_looktarget.h"
#include "sceneentity.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INSIGNIA_MODEL "models/chefhat.mdl"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#define CIT_INSPECTED_DELAY_TIME 120  //How often I'm allowed to be inspected

extern ConVar sk_healthkit;
extern ConVar sk_healthvial;

const int MAX_PLAYER_SQUAD = 4;

ConVar	sk_citizen_health				( "sk_citizen_health",					"0");
ConVar	sk_citizen_heal_player			( "sk_citizen_heal_player",				"25");
ConVar	sk_citizen_heal_player_delay	( "sk_citizen_heal_player_delay",		"25");
ConVar	sk_citizen_giveammo_player_delay( "sk_citizen_giveammo_player_delay",	"10");
ConVar	sk_citizen_heal_player_min_pct	( "sk_citizen_heal_player_min_pct",		"0.60");
ConVar	sk_citizen_heal_player_min_forced( "sk_citizen_heal_player_min_forced",		"10.0");
ConVar	sk_citizen_heal_ally			( "sk_citizen_heal_ally",				"30");
ConVar	sk_citizen_heal_ally_delay		( "sk_citizen_heal_ally_delay",			"20");
ConVar	sk_citizen_heal_ally_min_pct	( "sk_citizen_heal_ally_min_pct",		"0.90");
ConVar	sk_citizen_player_stare_time	( "sk_citizen_player_stare_time",		"1.0" );
ConVar  sk_citizen_player_stare_dist	( "sk_citizen_player_stare_dist",		"72" );
ConVar	sk_citizen_stare_heal_time		( "sk_citizen_stare_heal_time",			"5" );
#ifdef EZ
ConVar  sk_citizen_ar2_proficiency("sk_citizen_ar2_proficiency", "2"); // Added by 1upD. Skill rating 0 - 4 of how accurate the AR2 should be
ConVar  sk_citizen_default_proficiency("sk_citizen_default_proficiency", "1"); // Added by 1upD. Skill rating 0 - 4 of how accurate all weapons but the AR2 should be
ConVar  sk_citizen_default_willpower("sk_citizen_default_willpower", "1"); // Added by 1upD. Amount of mental stress damage citizen can take before panic
ConVar  sk_citizen_very_low_willpower( "sk_citizen_very_low_willpower", "-3" ); // Added by 1upD. Amount of willpower below which citizens might flee
ConVar  sk_citizen_head( "sk_citizen_head", "2.5" ); // Added by Blixibon. Multiplies by sk_npc_head
ConVar	sk_citizen_longfall_gear( "sk_citizen_longfall_gear", "1.5" ); // Added by Blixibon. Multiplies longfall gear.
#endif

ConVar	g_ai_citizen_show_enemy( "g_ai_citizen_show_enemy", "0" );

ConVar	npc_citizen_insignia( "npc_citizen_insignia", "0" );
ConVar	npc_citizen_squad_marker( "npc_citizen_squad_marker", "0" );
ConVar	npc_citizen_explosive_resist( "npc_citizen_explosive_resist", "0" );
ConVar	npc_citizen_auto_player_squad( "npc_citizen_auto_player_squad", "1" );
ConVar	npc_citizen_auto_player_squad_allow_use( "npc_citizen_auto_player_squad_allow_use", "0" );
#ifdef MAPBASE
ConVar	npc_citizen_squad_secondary_toggle_use_button("npc_citizen_squad_toggle_use_button", "262144"); // IN_WALK by default
ConVar	npc_citizen_squad_secondary_toggle_use_always( "npc_citizen_squad_secondary_toggle_use_always", "0", FCVAR_NONE, "Allows all citizens not strictly stuck to the player's squad to be toggled via Alt + E." );
#endif


ConVar	npc_citizen_dont_precache_all( "npc_citizen_dont_precache_all", "0" );


ConVar  npc_citizen_medic_emit_sound("npc_citizen_medic_emit_sound", "1" );
#ifdef HL2_EPISODIC
// todo: bake these into pound constants (for now they're not just for tuning purposes)
ConVar  npc_citizen_heal_chuck_medkit("npc_citizen_heal_chuck_medkit" , "1" , FCVAR_ARCHIVE, "Set to 1 to use new experimental healthkit-throwing medic.");
ConVar npc_citizen_medic_throw_style( "npc_citizen_medic_throw_style", "1", FCVAR_ARCHIVE, "Set to 0 for a lobbier trajectory" );
ConVar npc_citizen_medic_throw_speed( "npc_citizen_medic_throw_speed", "650" );
ConVar	sk_citizen_heal_toss_player_delay("sk_citizen_heal_toss_player_delay", "26", FCVAR_NONE, "how long between throwing healthkits" );


#define MEDIC_THROW_SPEED npc_citizen_medic_throw_speed.GetFloat()
#ifdef MAPBASE
// We use a boolean now, so NameMatches("griggs") is handled in CNPC_Citizen::Spawn().
#define USE_EXPERIMENTAL_MEDIC_CODE() (npc_citizen_heal_chuck_medkit.GetBool() && m_bTossesMedkits)
#else
#define USE_EXPERIMENTAL_MEDIC_CODE() (npc_citizen_heal_chuck_medkit.GetBool() && NameMatches("griggs"))
#endif
#endif

#ifdef MAPBASE
ConVar player_squad_autosummon_enabled( "player_squad_autosummon_enabled", "1" );
#endif
ConVar player_squad_autosummon_time( "player_squad_autosummon_time", "5" );
ConVar player_squad_autosummon_move_tolerance( "player_squad_autosummon_move_tolerance", "20" );
ConVar player_squad_autosummon_player_tolerance( "player_squad_autosummon_player_tolerance", "10" );
ConVar player_squad_autosummon_time_after_combat( "player_squad_autosummon_time_after_combat", "8" );
ConVar player_squad_autosummon_debug( "player_squad_autosummon_debug", "0" );
#ifdef EZ
ConVar ai_debug_willpower("ai_debug_willpower", "0"); // 1upD - use this to print messages to the log about Citizen willpower
ConVar ai_debug_rebel_suppressing_fire("ai_debug_rebel_suppressing_fire", "0"); // 1upD - use this to print messages to the log about Citizen willpower
ConVar ai_min_suppression_distance("ai_min_suppression_distance", "256", FCVAR_REPLICATED); // 1upD - minimum distance from object for an NPC to use suppressing fire
ConVar ai_suppression_distance_ratio("ai_suppression_distance_ratio", "0.5", FCVAR_REPLICATED); // 1upD - What percent of distance to suppression target must be covered
ConVar ai_willpower_translate_schedules("ai_willpower_translate_schedules", "1", FCVAR_REPLICATED); // 1upD - Should new EZ2 schedules be used?
ConVar ai_suppression_shoot_props( "ai_suppression_shoot_props", "1", FCVAR_REPLICATED ); // 1upD - Should rebels with suppressing fire shoot at props?
#ifdef EZ1
// Disable this in EZ1
ConVar npc_citizen_gib( "npc_citizen_gib", "0" );
#else
ConVar npc_citizen_gib( "npc_citizen_gib", "1" );
#endif
ConVar npc_citizen_longfall_death_height("npc_citizen_longfall_death_height", "32768");
ConVar npc_citizen_longfall_glow_chest_jump_fade_enter( "npc_citizen_longfall_glow_chest_jump_fade_enter", "0.25" );
ConVar npc_citizen_longfall_glow_chest_jump_fade_exit( "npc_citizen_longfall_glow_chest_jump_fade_exit", "0.5" );
ConVar npc_citizen_longfall_test_high_ground( "npc_citizen_longfall_test_high_ground", "0", FCVAR_NONE, "Should longfall / jump citizens only try to shoot from positions above their target?" );
ConVar npc_citizen_brute_mask_eject_threshold("npc_citizen_brute_mask_eject_threshold", "100");
#ifdef EZ2
ConVar npc_citizen_surrender_auto_distance( "npc_citizen_surrender_auto_distance", "720" );
ConVar npc_citizen_surrender_ammo_scale_min( "npc_citizen_surrender_ammo_scale_min", "0.05" );
ConVar npc_citizen_surrender_ammo_scale_max( "npc_citizen_surrender_ammo_scale_max", "0.10" );
ConVar npc_citizen_surrender_ammo_deplete_multiplier( "npc_citizen_surrender_ammo_deplete_multiplier", "0.80" );
#endif
#endif

#ifdef MAPBASE
ConVar npc_citizen_resupplier_adjust_ammo("npc_citizen_resupplier_adjust_ammo", "1", FCVAR_NONE, "If what ammo we give to the player would go over their max, should we adjust what we give accordingly (1) or cancel it altogether? (0)" );

ConVar npc_citizen_nocollide_player( "npc_citizen_nocollide_player", "0" );
#endif

#define ShouldAutosquad() (npc_citizen_auto_player_squad.GetBool())

enum SquadSlot_T
{
	SQUAD_SLOT_CITIZEN_RPG1	= LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_CITIZEN_RPG2,
#ifdef EZ
	SQUAD_SLOT_CITIZEN_INVESTIGATE,
	SQUAD_SLOT_CITIZEN_ADVANCE
#endif
};

const float HEAL_MOVE_RANGE = 30*12;
const float HEAL_TARGET_RANGE = 120; // 10 feet
#ifdef HL2_EPISODIC
const float HEAL_TOSS_TARGET_RANGE = 480; // 40 feet when we are throwing medkits 
const float HEAL_TARGET_RANGE_Z = 72; // a second check that Gordon isn't too far above us -- 6 feet
#endif

#ifdef EZ
#define BRUTE_MASK_MODEL "models/humans/group03b/welding_mask.mdl"
#define BRUTE_MASK_NUM_SKINS 3
#define BRUTE_MASK_BODYGROUP FindBodygroupByName("mask")

#define LONGFALL_GEAR_BODYGROUP 1
#define LONGFALL_GLOW_CHEST_SPRITE	"sprites/light_glow01.vmt"
#define LONGFALL_GLOW_CHEST_BRIGHT	164
#define LONGFALL_GLOW_CHEST_A		255
#define LONGFALL_GLOW_CHEST_R		255
#define LONGFALL_GLOW_CHEST_G		230
#define LONGFALL_GLOW_CHEST_B		IsMedic() ? 200 : 105
#define LONGFALL_GLOW_CHEST_SCALE	0.5f
#define LONGFALL_GLOW_CHEST_JUMP_BRIGHT	255
#define LONGFALL_GLOW_CHEST_JUMP_A		255
#define LONGFALL_GLOW_CHEST_JUMP_R		100
#define LONGFALL_GLOW_CHEST_JUMP_G		230
#define LONGFALL_GLOW_CHEST_JUMP_B		255
#define LONGFALL_GLOW_CHEST_JUMP_SCALE	0.75f
#define LONGFALL_GLOW_SHOULDER_SPRITE	"sprites/glow1.vmt"
#endif

// player must be at least this distance away from an enemy before we fire an RPG at him
const float RPG_SAFE_DISTANCE = CMissile::EXPLOSION_RADIUS + 64.0;

// Animation events
int AE_CITIZEN_GET_PACKAGE;
int AE_CITIZEN_HEAL;

//-------------------------------------
//-------------------------------------

ConVar	ai_follow_move_commands( "ai_follow_move_commands", "1" );
ConVar	ai_citizen_debug_commander( "ai_citizen_debug_commander", "1" );
#define DebuggingCommanderMode() (ai_citizen_debug_commander.GetBool() && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))

//-----------------------------------------------------------------------------
// Citizen expressions for the citizen expression types
//-----------------------------------------------------------------------------
#define STATES_WITH_EXPRESSIONS		3		// Idle, Alert, Combat
#define EXPRESSIONS_PER_STATE		1

char *szExpressionTypes[CIT_EXP_LAST_TYPE] =
{
	"Unassigned",
	"Scared",
	"Normal",
	"Angry"
};

struct citizen_expression_list_t
{
	char *szExpressions[EXPRESSIONS_PER_STATE];
};
// Scared
citizen_expression_list_t ScaredExpressions[STATES_WITH_EXPRESSIONS] =
{
	{ "scenes/Expressions/citizen_scared_idle_01.vcd" },
	{ "scenes/Expressions/citizen_scared_alert_01.vcd" },
	{ "scenes/Expressions/citizen_scared_combat_01.vcd" },
};
// Normal
citizen_expression_list_t NormalExpressions[STATES_WITH_EXPRESSIONS] =
{
	{ "scenes/Expressions/citizen_normal_idle_01.vcd" },
	{ "scenes/Expressions/citizen_normal_alert_01.vcd" },
	{ "scenes/Expressions/citizen_normal_combat_01.vcd" },
};
// Angry
citizen_expression_list_t AngryExpressions[STATES_WITH_EXPRESSIONS] =
{
	{ "scenes/Expressions/citizen_angry_idle_01.vcd" },
	{ "scenes/Expressions/citizen_angry_alert_01.vcd" },
	{ "scenes/Expressions/citizen_angry_combat_01.vcd" },
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#define COMMAND_POINT_CLASSNAME "info_target_command_point"

class CCommandPoint : public CPointEntity
{
	DECLARE_CLASS( CCommandPoint, CPointEntity );
public:
	CCommandPoint()
		: m_bNotInTransition(false)
	{
		if ( ++gm_nCommandPoints > 1 )
			DevMsg( "WARNING: More than one citizen command point present\n" );
	}

	~CCommandPoint()
	{
		--gm_nCommandPoints;
	}

	int ObjectCaps()
	{
		int caps = ( BaseClass::ObjectCaps() | FCAP_NOTIFY_ON_TRANSITION );

		if ( m_bNotInTransition )
			caps |= FCAP_DONT_SAVE;

		return caps;
	}

	void InputOutsideTransition( inputdata_t &inputdata )
	{
		if ( !AI_IsSinglePlayer() )
			return;

		m_bNotInTransition = true;

		CAI_Squad *pPlayerAISquad = g_AI_SquadManager.FindSquad(AllocPooledString(PLAYER_SQUADNAME));

		if ( pPlayerAISquad )
		{
			AISquadIter_t iter;
			for ( CAI_BaseNPC *pAllyNpc = pPlayerAISquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = pPlayerAISquad->GetNextMember(&iter) )
			{
				if ( pAllyNpc->GetCommandGoal() != vec3_invalid )
				{
					bool bHadGag = pAllyNpc->HasSpawnFlags(SF_NPC_GAG);

					pAllyNpc->AddSpawnFlags(SF_NPC_GAG);
					pAllyNpc->TargetOrder( UTIL_GetLocalPlayer(), &pAllyNpc, 1 );
					if ( !bHadGag )
						pAllyNpc->RemoveSpawnFlags(SF_NPC_GAG);
				}
			}
		}
	}
	DECLARE_DATADESC();

private:
	bool m_bNotInTransition; // does not need to be saved. If this is ever not default, the object is not being saved.
	static int gm_nCommandPoints;
};

int CCommandPoint::gm_nCommandPoints;

LINK_ENTITY_TO_CLASS( info_target_command_point, CCommandPoint );
BEGIN_DATADESC( CCommandPoint )
	
//	DEFINE_FIELD( m_bNotInTransition,	FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"OutsideTransition",	InputOutsideTransition ),

END_DATADESC()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class CMattsPipe : public CWeaponCrowbar
{
	DECLARE_CLASS( CMattsPipe, CWeaponCrowbar );

	const char *GetWorldModel() const	{ return "models/props_canal/mattpipe.mdl"; }
	void SetPickupTouch( void )	{	/* do nothing */ }
};

#ifdef MAPBASE
LINK_ENTITY_TO_CLASS(weapon_mattpipe, CMattsPipe);
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//---------------------------------------------------------
// Citizen models
//---------------------------------------------------------

static const char *g_ppszRandomHeads[] = 
{
	"male_01.mdl",
	"male_02.mdl",
	"female_01.mdl",
	"male_03.mdl",
	"female_02.mdl",
	"male_04.mdl",
	"female_03.mdl",
	"male_05.mdl",
	"female_04.mdl",
	"male_06.mdl",
	"female_06.mdl",
	"male_07.mdl",
	"female_07.mdl",
	"male_08.mdl",
	"male_09.mdl",
};

static const char *g_ppszModelLocs[] =
{
	"Group01",
	"Group01",
	"Group02",
	"Group03%s",
	"Group03%s",
	"Group03b%s",
	"Group03x%s",
	"Group04%s",
	"Group05",
	"Group05b",
};

#define IsExcludedHead( type, bMedic, iHead) false // see XBox codeline for an implementation


//---------------------------------------------------------
// Citizen activities
//---------------------------------------------------------

int ACT_CIT_HANDSUP;
int	ACT_CIT_BLINDED;		// Blinded by scanner photo
int ACT_CIT_SHOWARMBAND;
int ACT_CIT_HEAL;
int	ACT_CIT_STARTLED;		// Startled by sneaky scanner

#ifdef EZ2
// "Panicked" activities normally used by readiness, hijacked for EZ2 for unarmed citizens
int ACT_CROUCH_PANICKED;
int ACT_WALK_PANICKED;
int ACT_RUN_PANICKED;
#endif

//---------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_citizen, CNPC_Citizen );

//---------------------------------------------------------

BEGIN_DATADESC( CNPC_Citizen )

	DEFINE_CUSTOM_FIELD( m_nInspectActivity,		ActivityDataOps() ),
	DEFINE_FIELD( 		m_flNextFearSoundTime, 		FIELD_TIME ),
	DEFINE_FIELD( 		m_flStopManhackFlinch, 		FIELD_TIME ),
	DEFINE_FIELD( 		m_fNextInspectTime, 		FIELD_TIME ),
	DEFINE_FIELD( 		m_flPlayerHealTime, 		FIELD_TIME ),
	DEFINE_FIELD(		m_flNextHealthSearchTime,	FIELD_TIME ),
	DEFINE_FIELD( 		m_flAllyHealTime, 			FIELD_TIME ),
//						gm_PlayerSquadEvaluateTimer
//						m_AssaultBehavior
//						m_FollowBehavior
//						m_StandoffBehavior
//						m_LeadBehavior
//						m_FuncTankBehavior
	DEFINE_FIELD( 		m_flPlayerGiveAmmoTime, 	FIELD_TIME ),
	DEFINE_KEYFIELD(	m_iszAmmoSupply, 			FIELD_STRING,	"ammosupply" ),
	DEFINE_KEYFIELD(	m_iAmmoAmount, 				FIELD_INTEGER,	"ammoamount" ),
	DEFINE_FIELD( 		m_bRPGAvoidPlayer, 			FIELD_BOOLEAN ),
	DEFINE_FIELD( 		m_bShouldPatrol, 			FIELD_BOOLEAN ),
	DEFINE_FIELD( 		m_iszOriginalSquad, 		FIELD_STRING ),
	DEFINE_FIELD( 		m_flTimeJoinedPlayerSquad,	FIELD_TIME ),
	DEFINE_FIELD( 		m_bWasInPlayerSquad, FIELD_BOOLEAN ),
	DEFINE_FIELD( 		m_flTimeLastCloseToPlayer,	FIELD_TIME ),
	DEFINE_EMBEDDED(	m_AutoSummonTimer ),
	DEFINE_FIELD(		m_vAutoSummonAnchor, FIELD_POSITION_VECTOR ),
	DEFINE_KEYFIELD(	m_Type, 					FIELD_INTEGER,	"citizentype" ),
	DEFINE_KEYFIELD(	m_ExpressionType,			FIELD_INTEGER,	"expressiontype" ),
	DEFINE_FIELD(		m_iHead,					FIELD_INTEGER ),
	DEFINE_FIELD(		m_flTimePlayerStare,		FIELD_TIME ),
	DEFINE_FIELD(		m_flTimeNextHealStare,		FIELD_TIME ),
	DEFINE_FIELD( 		m_hSavedFollowGoalEnt,		FIELD_EHANDLE ),
	DEFINE_KEYFIELD(	m_bNotifyNavFailBlocked,	FIELD_BOOLEAN, "notifynavfailblocked" ),
	DEFINE_KEYFIELD(	m_bNeverLeavePlayerSquad,	FIELD_BOOLEAN, "neverleaveplayersquad" ),
	DEFINE_KEYFIELD(	m_iszDenyCommandConcept,	FIELD_STRING, "denycommandconcept" ),
#ifdef EZ
	DEFINE_KEYFIELD(	m_iWillpowerModifier,		FIELD_INTEGER, "willpowermodifier"),
	DEFINE_KEYFIELD(    m_bWillpowerDisabled,		FIELD_BOOLEAN, "willpowerdisabled"),
	DEFINE_KEYFIELD(    m_bSuppressiveFireDisabled, FIELD_BOOLEAN, "suppressivefiredisabled" ),
	DEFINE_KEYFIELD(	m_bUsedBackupWeapon,		FIELD_BOOLEAN, "disablebackupweapon" ),
	DEFINE_FIELD(		m_vecDecoyObjectTarget,		FIELD_VECTOR),
#endif
#ifdef MAPBASE
	DEFINE_INPUT(		m_bTossesMedkits,			FIELD_BOOLEAN, "SetTossMedkits" ),
	DEFINE_KEYFIELD(	m_bAlternateAiming,			FIELD_BOOLEAN, "AlternateAiming" ),
#endif

	DEFINE_OUTPUT(		m_OnJoinedPlayerSquad,	"OnJoinedPlayerSquad" ),
	DEFINE_OUTPUT(		m_OnLeftPlayerSquad,	"OnLeftPlayerSquad" ),
	DEFINE_OUTPUT(		m_OnFollowOrder,		"OnFollowOrder" ),
	DEFINE_OUTPUT(		m_OnStationOrder,		"OnStationOrder" ),
	DEFINE_OUTPUT(		m_OnPlayerUse,			"OnPlayerUse" ),
	DEFINE_OUTPUT(		m_OnNavFailBlocked,		"OnNavFailBlocked" ),
#ifdef MAPBASE
	DEFINE_OUTPUT(		m_OnHealedNPC,			"OnHealedNPC" ),
	DEFINE_OUTPUT(		m_OnHealedPlayer,		"OnHealedPlayer" ),
	DEFINE_OUTPUT(		m_OnThrowMedkit,		"OnTossMedkit" ),
	DEFINE_OUTPUT(		m_OnGiveAmmo,			"OnGiveAmmo" ),
#endif

#ifdef EZ2
	DEFINE_OUTPUT(		m_OnSurrender,			"OnSurrender" ),
	DEFINE_OUTPUT(		m_OnStopSurrendering,			"OnStopSurrendering" ),
#endif

	DEFINE_INPUTFUNC( FIELD_VOID,	"RemoveFromPlayerSquad", InputRemoveFromPlayerSquad ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StartPatrolling",	InputStartPatrolling ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StopPatrolling",	InputStopPatrolling ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetCommandable",	InputSetCommandable ),
#ifdef MAPBASE
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetUnCommandable",	InputSetUnCommandable ),
#endif
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetMedicOn",	InputSetMedicOn ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetMedicOff",	InputSetMedicOff ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetAmmoResupplierOn",	InputSetAmmoResupplierOn ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetAmmoResupplierOff",	InputSetAmmoResupplierOff ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SpeakIdleResponse", InputSpeakIdleResponse ),

#if HL2_EPISODIC
	DEFINE_INPUTFUNC( FIELD_VOID,   "ThrowHealthKit", InputForceHealthKitToss ),
#endif

#ifdef MAPBASE
	DEFINE_INPUTFUNC( FIELD_STRING, "SetPoliceGoal", InputSetPoliceGoal ),
#endif
#ifdef EZ2
	DEFINE_INPUTFUNC( FIELD_VOID, "Surrender", InputSurrender ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetSurrenderFlags", InputSetSurrenderFlags ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddSurrenderFlags", InputAddSurrenderFlags ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "RemoveSurrenderFlags", InputRemoveSurrenderFlags ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetWillpowerModifier", InputSetWillpowerModifier ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetWillpowerDisabled", InputSetWillpowerDisabled ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetSuppressingFireDisabled", InputSetSuppressiveFireDisabled ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "ForcePanic", InputForcePanic ),
#endif

	DEFINE_USEFUNC( CommanderUse ),
	DEFINE_USEFUNC( SimpleUse ),

END_DATADESC()

#ifdef MAPBASE_VSCRIPT
ScriptHook_t	CNPC_Citizen::g_Hook_SelectModel;

BEGIN_ENT_SCRIPTDESC( CNPC_Citizen, CAI_BaseActor, "npc_citizen from Half-Life 2" )

	DEFINE_SCRIPTFUNC( IsAmmoResupplier, "Returns true if this citizen is an ammo resupplier." )
	DEFINE_SCRIPTFUNC( CanHeal, "Returns true if this citizen is a medic or ammo resupplier currently able to heal/give ammo." )

	DEFINE_SCRIPTFUNC( GetCitizenType, "Gets the citizen's type. 1 = Downtrodden, 2 = Refugee, 3 = Rebel, 4 = Unique" )
	DEFINE_SCRIPTFUNC( SetCitizenType, "Sets the citizen's type. 1 = Downtrodden, 2 = Refugee, 3 = Rebel, 4 = Unique" )

	BEGIN_SCRIPTHOOK( CNPC_Citizen::g_Hook_SelectModel, "SelectModel", FIELD_CSTRING, "Called when a citizen is selecting a random model. 'model_path' is the directory of the selected model and 'model_head' is the name. The 'gender' parameter uses the 'GENDER_' constants and is based only on the citizen's random head spawnflags. If a full model path string is returned, it will be used as the model instead." )
		DEFINE_SCRIPTHOOK_PARAM( "model_path", FIELD_CSTRING )
		DEFINE_SCRIPTHOOK_PARAM( "model_head", FIELD_CSTRING )
		DEFINE_SCRIPTHOOK_PARAM( "gender", FIELD_INTEGER )
	END_SCRIPTHOOK()

END_SCRIPTDESC();
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CSimpleSimTimer CNPC_Citizen::gm_PlayerSquadEvaluateTimer;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CNPC_Citizen::CreateBehaviors()
{
	BaseClass::CreateBehaviors();
#ifdef MAPBASE
	AddBehavior( &m_RappelBehavior );
	AddBehavior( &m_PolicingBehavior );
#else // Moved to CNPC_PlayerCompanion
	AddBehavior( &m_FuncTankBehavior );
#endif
#ifdef EZ2
	AddBehavior( &m_SurrenderBehavior );
#endif
	
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::Precache()
{
#ifdef MAPBASE
	// CNPC_PlayerCompanion::Precache() is responsible for calling this now
	BaseClass::Precache();
#else
	SelectModel();
#endif
	SelectExpressionType();

	if ( !npc_citizen_dont_precache_all.GetBool() )
		PrecacheAllOfType( m_Type );
	else
		PrecacheModel( STRING( GetModelName() ) );

#ifdef EZ2
	if ( m_Type >= CT_BRUTE && m_Type <= CT_ARBEIT_SEC ) // CT_BRUTE, CT_LONGFALL, CT_ARCTIC, CT_ARBEIT, CT_ARBEIT_SEC
	{
		// Potential backup weapon
		UTIL_PrecacheOther( "weapon_css_glock" );
	}
#endif

	if ( NameMatches( "matt" ) )
		PrecacheModel( "models/props_canal/mattpipe.mdl" );

	PrecacheModel( INSIGNIA_MODEL );

	PrecacheScriptSound( "NPC_Citizen.FootstepLeft" );
	PrecacheScriptSound( "NPC_Citizen.FootstepRight" );
	PrecacheScriptSound( "NPC_Citizen.Die" );

	PrecacheInstancedScene( "scenes/Expressions/CitizenIdle.vcd" );
	PrecacheInstancedScene( "scenes/Expressions/CitizenAlert_loop.vcd" );
	PrecacheInstancedScene( "scenes/Expressions/CitizenCombat_loop.vcd" );

	for ( int i = 0; i < STATES_WITH_EXPRESSIONS; i++ )
	{
		for ( int j = 0; j < ARRAYSIZE(ScaredExpressions[i].szExpressions); j++ )
		{
			PrecacheInstancedScene( ScaredExpressions[i].szExpressions[j] );
		}
		for ( int j = 0; j < ARRAYSIZE(NormalExpressions[i].szExpressions); j++ )
		{
			PrecacheInstancedScene( NormalExpressions[i].szExpressions[j] );
		}
		for ( int j = 0; j < ARRAYSIZE(AngryExpressions[i].szExpressions); j++ )
		{
			PrecacheInstancedScene( AngryExpressions[i].szExpressions[j] );
		}
	}

#ifndef MAPBASE // See above
	BaseClass::Precache();
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::PrecacheAllOfType( CitizenType_t type )
{
	if ( m_Type == CT_UNIQUE )
		return;

	int nHeads = ARRAYSIZE( g_ppszRandomHeads );
	int i;
	for ( i = 0; i < nHeads; ++i )
	{
		if ( !IsExcludedHead( type, false, i ) )
		{
			PrecacheModel( CFmtStr( "models/Humans/%s/%s", (const char *)(CFmtStr(g_ppszModelLocs[m_Type], "")), g_ppszRandomHeads[i] ) );
		}
	}

#ifdef EZ
	if ( IsMedic() && (m_Type == CT_REBEL || m_Type == CT_ARCTIC || m_Type == CT_BRUTE || m_Type == CT_LONGFALL) )
#else
	if ( m_Type == CT_REBEL )
#endif
	{
		for ( i = 0; i < nHeads; ++i )
		{
			if ( !IsExcludedHead( type, true, i ) )
			{
				PrecacheModel( CFmtStr( "models/Humans/%s/%s", (const char *)(CFmtStr(g_ppszModelLocs[m_Type], "m")), g_ppszRandomHeads[i] ) );
			}
		}
	}

#ifdef EZ
	// Blixibon - Long-fall assets
	if ( m_Type == CT_LONGFALL )
	{
		PrecacheScriptSound("NPC_Citizen_JumpRebel.Jump");
		PrecacheScriptSound("NPC_Citizen_JumpRebel.Jump_Death");
		PrecacheScriptSound("NPC_Citizen_JumpRebel.Land");

		PrecacheModel( LONGFALL_GLOW_CHEST_SPRITE );
		PrecacheModel( LONGFALL_GLOW_SHOULDER_SPRITE );

		PrecacheParticleSystem( "citizen_longfall_jump" );
	}
	else if ( m_Type == CT_BRUTE )
	{
		PrecacheModel( BRUTE_MASK_MODEL );
	}
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::Spawn()
{
	BaseClass::Spawn();

#ifdef _XBOX
	// Always fade the corpse
	AddSpawnFlags( SF_NPC_FADE_CORPSE );
#endif // _XBOX

	if ( ShouldAutosquad() )
	{
		if ( m_SquadName == GetPlayerSquadName() )
		{
			CAI_Squad *pPlayerSquad = g_AI_SquadManager.FindSquad( GetPlayerSquadName() );
			if ( pPlayerSquad && pPlayerSquad->NumMembers() >= MAX_PLAYER_SQUAD )
				m_SquadName = NULL_STRING;
		}
		gm_PlayerSquadEvaluateTimer.Force();
	}

	if ( IsAmmoResupplier() )
		m_nSkin = 2;
	
	m_bRPGAvoidPlayer = false;

	m_bShouldPatrol = false;
	m_iHealth = sk_citizen_health.GetFloat();
	
#ifdef MAPBASE
	// Now only gets citizen_trains.
	if ( GetMoveParent() && FClassnameIs( GetMoveParent(), "func_tracktrain" ) )
	{
		if ( NameMatches("citizen_train_2") )
		{
			CapabilitiesRemove( bits_CAP_MOVE_GROUND );
			SetMoveType( MOVETYPE_NONE );
			SetSequenceByName( "d1_t01_TrainRide_Sit_Idle" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
		else if ( NameMatches("citizen_train_1") )
		{
			CapabilitiesRemove( bits_CAP_MOVE_GROUND );
			SetMoveType( MOVETYPE_NONE );
			SetSequenceByName( "d1_t01_TrainRide_Stand" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
	}
#else
	// Are we on a train? Used in trainstation to have NPCs on trains.
	if ( GetMoveParent() && FClassnameIs( GetMoveParent(), "func_tracktrain" ) )
	{
		CapabilitiesRemove( bits_CAP_MOVE_GROUND );
		SetMoveType( MOVETYPE_NONE );
		if ( NameMatches("citizen_train_2") )
		{
			SetSequenceByName( "d1_t01_TrainRide_Sit_Idle" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
		else
		{
			SetSequenceByName( "d1_t01_TrainRide_Stand" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
	}
#endif

	m_flStopManhackFlinch = -1;

	m_iszIdleExpression = MAKE_STRING("scenes/expressions/citizenidle.vcd");
	m_iszAlertExpression = MAKE_STRING("scenes/expressions/citizenalert_loop.vcd");
	m_iszCombatExpression = MAKE_STRING("scenes/expressions/citizencombat_loop.vcd");

	m_iszOriginalSquad = m_SquadName;

	m_flNextHealthSearchTime = gpGlobals->curtime;

	CWeaponRPG *pRPG = dynamic_cast<CWeaponRPG*>(GetActiveWeapon());
	if ( pRPG )
	{
		CapabilitiesRemove( bits_CAP_USE_SHOT_REGULATOR );
		pRPG->StopGuiding();
	}
#ifdef EZ
	// In Entropy : Zero, armed citizens can use melee attacks if the 'Enable melee' key value is unset
	CapabilitiesAdd( bits_CAP_INNATE_MELEE_ATTACK1 ); 
	
#ifndef EZ2
	if ( m_Type == CT_LONGFALL )
	{
		CapabilitiesAdd( bits_CAP_MOVE_JUMP );
	}
	#endif

	if ( m_Type == CT_BRUTE )
	{
		// Randomize brute mask skin based on entity index
		m_nSkin = entindex() % BRUTE_MASK_NUM_SKINS;
	}
	else if ( m_Type >= CT_BRUTE && m_Type <= CT_ARBEIT_SEC ) // CT_BRUTE, CT_LONGFALL, CT_ARCTIC, CT_ARBEIT, CT_ARBEIT_SEC
	{
		if (GlobalEntity_GetState( "citizens_no_auto_variant" ) != GLOBAL_ON)
		{
			// By default, E:Z2's citizen types should be equivalent to EZ_VARIANT_ARBEIT.
			// This is mainly for dropping Arbeit items.
			if (m_tEzVariant == EZ_VARIANT_DEFAULT)
				m_tEzVariant = EZ_VARIANT_ARBEIT;
		}
	}
#endif
	m_flTimePlayerStare = FLT_MAX;
#ifndef EZ
	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );
#endif
	NPCInit();

	SetUse( &CNPC_Citizen::CommanderUse );
	Assert( !ShouldAutosquad() || !IsInPlayerSquad() );

	m_bWasInPlayerSquad = IsInPlayerSquad();

	// Use render bounds instead of human hull for guys sitting in chairs, etc.
	m_ActBusyBehavior.SetUseRenderBounds( HasSpawnFlags( SF_CITIZEN_USE_RENDER_BOUNDS ) );

#ifdef MAPBASE
	if (NameMatches("griggs"))
	{
		m_bTossesMedkits = true;
	}
#endif

#ifdef EZ2
	if (!IsMedic() && !IsAmmoResupplier() /*&& m_Type == CT_BRUTE*/ && CanSurrender() && ((m_iszAmmoSupply == NULL_STRING && m_iAmmoAmount == 0) || (FStrEq(STRING(m_iszAmmoSupply), "SMG1") && m_iAmmoAmount == 1)))
	{
		// Change the ammo supply and ammo ammount to reflect our equipment (for surrendering purposes)
		if (m_Type == CT_BRUTE && HasGrenades())
		{
			if (IsAltFireCapable())
			{
				m_iAmmoAmount = RandomInt( 1, 2 );
				if (GetActiveWeapon() && GetActiveWeapon()->ClassMatches("weapon_ar2*"))
					m_iszAmmoSupply = AllocPooledString( "AR2AltFire" );
				else
					m_iszAmmoSupply = AllocPooledString( "SMG1_Grenade" );
			}
			else
			{
				m_iszAmmoSupply = AllocPooledString( "Grenade" );
				m_iAmmoAmount = RandomInt( 1, 3 );
			}
		}
		else
		{
			int iAmmoIndex = 0;
			const char *pszAmmoName = NULL;
			if (GetActiveWeapon() && GetActiveWeapon()->UsesPrimaryAmmo())
			{
				iAmmoIndex = GetActiveWeapon()->GetPrimaryAmmoType();
				pszAmmoName = GetAmmoDef()->Name( iAmmoIndex );
			}
			else
			{
				// Citizens with no starting weapon are rare, so for now, they provide 357 ammo
				// (totally not a workaround for the ez2_c4_1 jacket rebel)
				pszAmmoName = "357";
				iAmmoIndex = GetAmmoDef()->Index( pszAmmoName );
			}

			if (pszAmmoName)
			{
				m_iszAmmoSupply = AllocPooledString( pszAmmoName );
				m_iAmmoAmount = ceilf((float)GetAmmoDef()->MaxCarry( iAmmoIndex ) * RandomFloat( npc_citizen_surrender_ammo_scale_min.GetFloat(), npc_citizen_surrender_ammo_scale_max.GetFloat() ));
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::PostNPCInit()
{
	if ( !gEntList.FindEntityByClassname( NULL, COMMAND_POINT_CLASSNAME ) )
	{
		CreateEntityByName( COMMAND_POINT_CLASSNAME );
	}
	
	if ( IsInPlayerSquad() )
	{
		if ( m_pSquad->NumMembers() > MAX_PLAYER_SQUAD )
			DevMsg( "Error: Spawning citizen in player squad but exceeds squad limit of %d members\n", MAX_PLAYER_SQUAD );

		FixupPlayerSquad();
	}
	else
	{
		if ( ( m_spawnflags & SF_CITIZEN_FOLLOW ) && AI_IsSinglePlayer() )
		{
			m_FollowBehavior.SetFollowTarget( UTIL_GetLocalPlayer() );
			m_FollowBehavior.SetParameters( AIF_SIMPLE );
		}
	}

	BaseClass::PostNPCInit();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct HeadCandidate_t
{
	int iHead;
	int nHeads;

	static int __cdecl Sort( const HeadCandidate_t *pLeft, const HeadCandidate_t *pRight )
	{
		return ( pLeft->nHeads - pRight->nHeads );
	}
};

void CNPC_Citizen::SelectModel()
{
	// If making reslists, precache everything!!!
	static bool madereslists = false;

	if ( CommandLine()->CheckParm("-makereslists") && !madereslists )
	{
		madereslists = true;

		PrecacheAllOfType( CT_DOWNTRODDEN );
		PrecacheAllOfType( CT_REFUGEE );
		PrecacheAllOfType( CT_REBEL );
		PrecacheAllOfType( CT_BRUTE );
		PrecacheAllOfType( CT_LONGFALL );
		PrecacheAllOfType( CT_ARCTIC );
	}

	const char *pszModelName = NULL;

	if ( m_Type == CT_DEFAULT )
	{
#ifdef MAPBASE
		if (HL2GameRules()->GetDefaultCitizenType() != CT_DEFAULT)
		{
			m_Type = static_cast<CitizenType_t>(HL2GameRules()->GetDefaultCitizenType());
		}
		else
		{
#endif
		struct CitizenTypeMapping
		{
			const char *pszMapTag;
			CitizenType_t type;
		};

		static CitizenTypeMapping CitizenTypeMappings[] = 
		{
			{ "trainstation",	CT_DOWNTRODDEN	},
			{ "canals",			CT_REFUGEE		},
			{ "town",			CT_REFUGEE		},
			{ "coast",			CT_REFUGEE		},
			{ "prison",			CT_DOWNTRODDEN	},
			{ "c17",			CT_REBEL		},
			{ "citadel",		CT_DOWNTRODDEN	},
		};

		char szMapName[256];
		Q_strncpy(szMapName, STRING(gpGlobals->mapname), sizeof(szMapName) );
		Q_strlower(szMapName);

		for ( int i = 0; i < ARRAYSIZE(CitizenTypeMappings); i++ )
		{
			if ( Q_stristr( szMapName, CitizenTypeMappings[i].pszMapTag ) )
			{
				m_Type = CitizenTypeMappings[i].type;
				break;
			}
		}

		if ( m_Type == CT_DEFAULT )
			m_Type = CT_DOWNTRODDEN;
#ifdef MAPBASE
		}
#endif
	}

	if( HasSpawnFlags( SF_CITIZEN_RANDOM_HEAD | SF_CITIZEN_RANDOM_HEAD_MALE | SF_CITIZEN_RANDOM_HEAD_FEMALE ) || GetModelName() == NULL_STRING )
	{
		Assert( m_iHead == -1 );
		char gender = ( HasSpawnFlags( SF_CITIZEN_RANDOM_HEAD_MALE ) ) ? 'm' : 
					  ( HasSpawnFlags( SF_CITIZEN_RANDOM_HEAD_FEMALE ) ) ? 'f' : 0;

		RemoveSpawnFlags( SF_CITIZEN_RANDOM_HEAD | SF_CITIZEN_RANDOM_HEAD_MALE | SF_CITIZEN_RANDOM_HEAD_FEMALE );
		if( HasSpawnFlags( SF_NPC_START_EFFICIENT ) )
		{
			SetModelName( AllocPooledString("models/humans/male_cheaple.mdl" ) );
			return;
		}
		else
		{
			// Count the heads
			int headCounts[ARRAYSIZE(g_ppszRandomHeads)] = { 0 };
			int i;

			for ( i = 0; i < g_AI_Manager.NumAIs(); i++ )
			{
				CNPC_Citizen *pCitizen = dynamic_cast<CNPC_Citizen *>(g_AI_Manager.AccessAIs()[i]);
				if ( pCitizen && pCitizen != this && pCitizen->m_iHead >= 0 && pCitizen->m_iHead < ARRAYSIZE(g_ppszRandomHeads) )
				{
					headCounts[pCitizen->m_iHead]++;
				}
			}

			// Find all candidates
			CUtlVectorFixed<HeadCandidate_t, ARRAYSIZE(g_ppszRandomHeads)> candidates;

			for ( i = 0; i < ARRAYSIZE(g_ppszRandomHeads); i++ )
			{
				if ( !gender || g_ppszRandomHeads[i][0] == gender )
				{
					if ( !IsExcludedHead( m_Type, IsMedic(), i ) )
					{
						HeadCandidate_t candidate = { i, headCounts[i] };
						candidates.AddToTail( candidate );
					}
				}
			}

			Assert( candidates.Count() );
			candidates.Sort( &HeadCandidate_t::Sort );

			int iSmallestCount = candidates[0].nHeads;
			int iLimit;

			for ( iLimit = 0; iLimit < candidates.Count(); iLimit++ )
			{
				if ( candidates[iLimit].nHeads > iSmallestCount )
					break;
			}

			m_iHead = candidates[random->RandomInt( 0, iLimit - 1 )].iHead;
			pszModelName = g_ppszRandomHeads[m_iHead];
			SetModelName(NULL_STRING);
		}

#ifdef MAPBASE_VSCRIPT
		if (m_ScriptScope.IsInitialized() && g_Hook_SelectModel.CanRunInScope( m_ScriptScope ))
		{
			gender_t scriptGender;
			switch (gender)
			{
				case 'm':
					scriptGender = GENDER_MALE;
					break;
				case 'f':
					scriptGender = GENDER_FEMALE;
					break;
				default:
					scriptGender = GENDER_NONE;
					break;
			}

			const char *pszModelPath = CFmtStr( "models/Humans/%s/", (const char *)(CFmtStr( g_ppszModelLocs[m_Type], (IsMedic()) ? "m" : "" )) );

			// model_path, model_head, gender
			ScriptVariant_t args[] = { pszModelPath, pszModelName, (int)scriptGender };
			ScriptVariant_t returnValue;
			g_Hook_SelectModel.Call( m_ScriptScope, &returnValue, args );

			if (returnValue.m_type == FIELD_CSTRING && returnValue.m_pszString[0] != '\0')
			{
				// Refresh the head if it's different
				const char *pszNewHead = strrchr( returnValue.m_pszString, '/' );
				if ( pszNewHead && Q_stricmp(pszNewHead+1, pszModelName) != 0 )
				{
					pszNewHead++;
					for ( int i = 0; i < ARRAYSIZE(g_ppszRandomHeads); i++ )
					{
						if ( Q_stricmp( g_ppszRandomHeads[i], pszModelName ) == 0 )
						{
							m_iHead = i;
							break;
						}
					}
				}

				// Just set the model right here
				SetModelName( AllocPooledString( returnValue.m_pszString ) );
				return;
			}
		}
#endif
	}

	Assert( pszModelName || GetModelName() != NULL_STRING );

	if ( !pszModelName )
	{
		if ( GetModelName() == NULL_STRING )
			return;
		pszModelName = strrchr(STRING(GetModelName()), '/' );
		if ( !pszModelName )
			pszModelName = STRING(GetModelName());
		else
		{
			pszModelName++;
			if ( m_iHead == -1 )
			{
				for ( int i = 0; i < ARRAYSIZE(g_ppszRandomHeads); i++ )
				{
					if ( Q_stricmp( g_ppszRandomHeads[i], pszModelName ) == 0 )
					{
						m_iHead = i;
						break;
					}
				}
			}
		}
		if ( !*pszModelName )
			return;
	}

	// Unique citizen models are left alone
	if ( m_Type != CT_UNIQUE )
	{
		SetModelName( AllocPooledString( CFmtStr( "models/Humans/%s/%s", (const char *)(CFmtStr(g_ppszModelLocs[ m_Type ], ( IsMedic() ) ? "m" : "" )), pszModelName ) ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::SelectExpressionType()
{
	// If we've got a mapmaker assigned type, leave it alone
	if ( m_ExpressionType != CIT_EXP_UNASSIGNED )
		return;

	switch ( m_Type )
	{
	case CT_DOWNTRODDEN:
		m_ExpressionType = (CitizenExpressionTypes_t)RandomInt( CIT_EXP_SCARED, CIT_EXP_NORMAL );
		break;
	case CT_REFUGEE:
		m_ExpressionType = (CitizenExpressionTypes_t)RandomInt( CIT_EXP_SCARED, CIT_EXP_NORMAL );
		break;
	case CT_REBEL:
		m_ExpressionType = (CitizenExpressionTypes_t)RandomInt( CIT_EXP_SCARED, CIT_EXP_ANGRY );
		break;
	case CT_BRUTE:
		m_ExpressionType = (CitizenExpressionTypes_t)RandomInt(CIT_EXP_NORMAL, CIT_EXP_ANGRY); // Brutes show no fear
	case CT_LONGFALL:
		m_ExpressionType = (CitizenExpressionTypes_t)RandomInt(CIT_EXP_NORMAL, CIT_EXP_ANGRY); // Long fall boot rebels are crazy
	case CT_ARCTIC:
		m_ExpressionType = (CitizenExpressionTypes_t)RandomInt( CIT_EXP_SCARED, CIT_EXP_ANGRY );
	case CT_DEFAULT:
	case CT_UNIQUE:
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::FixupMattWeapon()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
#ifdef MAPBASE
	if ( pWeapon && EntIsClass( pWeapon, gm_isz_class_Crowbar ) && NameMatches( "matt" ) )
#else
	if ( pWeapon && pWeapon->ClassMatches( "weapon_crowbar" ) && NameMatches( "matt" ) )
#endif
	{
		Weapon_Drop( pWeapon );
		UTIL_Remove( pWeapon );
		pWeapon = (CBaseCombatWeapon *)CREATE_UNSAVED_ENTITY( CMattsPipe, "weapon_crowbar" );
		pWeapon->SetName( AllocPooledString( "matt_weapon" ) );
		DispatchSpawn( pWeapon );

#ifdef DEBUG
		extern bool g_bReceivedChainedActivate;
		g_bReceivedChainedActivate = false;
#endif
		pWeapon->Activate();
		Weapon_Equip( pWeapon );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CNPC_Citizen::Activate()
{
	BaseClass::Activate();
	FixupMattWeapon();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnRestore()
{
	gm_PlayerSquadEvaluateTimer.Force();

	BaseClass::OnRestore();

	if ( !gEntList.FindEntityByClassname( NULL, COMMAND_POINT_CLASSNAME ) )
	{
		CreateEntityByName( COMMAND_POINT_CLASSNAME );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
string_t CNPC_Citizen::GetModelName() const
{
	string_t iszModelName = BaseClass::GetModelName();

	//
	// If the model refers to an obsolete model, pretend it was blank
	// so that we pick the new default model.
	//
	if (!Q_strnicmp(STRING(iszModelName), "models/c17_", 11) ||
		!Q_strnicmp(STRING(iszModelName), "models/male", 11) ||
		!Q_strnicmp(STRING(iszModelName), "models/female", 13) ||
		!Q_strnicmp(STRING(iszModelName), "models/citizen", 14))
	{
		return NULL_STRING;
	}

	return iszModelName;
}

//-----------------------------------------------------------------------------
// Purpose: Overridden to switch our behavior between passive and rebel. We
//			become combative after Gordon becomes a criminal.
//-----------------------------------------------------------------------------
Class_T	CNPC_Citizen::Classify()
{
	if (GlobalEntity_GetState("gordon_precriminal") == GLOBAL_ON)
		return CLASS_CITIZEN_PASSIVE;

	if (GlobalEntity_GetState("citizens_passive") == GLOBAL_ON)
		return CLASS_CITIZEN_PASSIVE;

#ifdef EZ2
	if ( IsSurrendered() )
	{
		// If we got a weapon back, revert to rebel
		if ( GetActiveWeapon() != NULL )
		{
			//RemoveContext( "surrendered:1" );
			m_SurrenderBehavior.StopSurrendering();
			return CLASS_PLAYER_ALLY;
		}

		return CLASS_CITIZEN_PASSIVE;
	}
#endif

	return CLASS_PLAYER_ALLY;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldAlwaysThink() 
{ 
	return ( BaseClass::ShouldAlwaysThink() || IsInPlayerSquad() ); 
}
	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define CITIZEN_FOLLOWER_DESERT_FUNCTANK_DIST	45.0f*12.0f
bool CNPC_Citizen::ShouldBehaviorSelectSchedule( CAI_BehaviorBase *pBehavior )
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
			if( vecFollowGoal.DistToSqr( GetAbsOrigin() ) > Square(CITIZEN_FOLLOWER_DESERT_FUNCTANK_DIST) )
			{
				return false;
			}
		}
	}
#ifdef EZ2
	// Don't use func_tanks or passively act busy while surrendered
	else if ( IsSurrendered() && ( pBehavior == &m_FuncTankBehavior || ( pBehavior == &m_ActBusyBehavior /*&& !m_ActBusyBehavior.IsActive()*/ ) ) )
	{
		return false;
	}
#endif

	return BaseClass::ShouldBehaviorSelectSchedule( pBehavior );
}

void CNPC_Citizen::OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior,  CAI_BehaviorBase *pNewBehavior )
{
	if ( pNewBehavior == &m_FuncTankBehavior )
	{
		m_bReadinessCapable = false;
	}
	else if ( pOldBehavior == &m_FuncTankBehavior )
	{
		m_bReadinessCapable = IsReadinessCapable();
	}

	BaseClass::OnChangeRunningBehavior( pOldBehavior, pNewBehavior );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#ifdef EZ
void CNPC_Citizen::GatherWillpowerConditions()
{
	if (m_bWillpowerDisabled || m_NPCState != NPC_STATE_COMBAT)
		return;

	CBaseCombatWeapon * pWeapon = GetActiveWeapon();
	if (pWeapon == NULL) // Unarmed citizens do not use willpower
		return;

	if ( pWeapon->IsMeleeWeapon() ) // Melee citizens do not use willpower
		return;

	int l_iWillpower = sk_citizen_default_willpower.GetInt() + m_iWillpowerModifier;

	bool typeCanPanic = m_Type != CT_BRUTE && m_Type != CT_LONGFALL && !HasCondition( COND_CIT_WILLPOWER_LOW );

	if( pWeapon->UsesClipsForAmmo1() && ((float)pWeapon->m_iClip1 / (float)pWeapon->GetMaxClip1()) > 0.75) // ratio of rounds left in magazine
		l_iWillpower++;

	if (HasCondition(COND_LOW_PRIMARY_AMMO))
		l_iWillpower--;

	if (HasCondition(COND_NO_PRIMARY_AMMO))
		l_iWillpower--;

	if(pWeapon->ClassMatches("weapon_shotgun"))
		l_iWillpower++;

	// Lone wolf bonus - I have no squad
	if (m_pSquad == NULL) {
		l_iWillpower++;
	} 
	else if (m_pSquad && m_pSquad->NumMembers() > 2) // More than 1 squadmate
	{
		l_iWillpower++; // I have a squad! Setting this to number of squadmates made it too influential
	}
	else  // I'm the last one
	{
		l_iWillpower -= 2; // This is a big deal
	}

	if (HasCondition(COND_BEHIND_ENEMY))
		l_iWillpower++;

	if (HasCondition(COND_ENEMY_DEAD))
		l_iWillpower++;

	if (HasCondition(COND_ENEMY_OCCLUDED))
		l_iWillpower++;

	if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE)) // Light damage and heavy damage are the same - This is Entropy: Zero
		l_iWillpower -= 2; // This is a big deal

	if (HasCondition(COND_NEW_ENEMY))
		l_iWillpower--;

	if (HasCondition(COND_HAVE_ENEMY_LOS))
		l_iWillpower--;

	if (HasCondition(COND_HEAR_SPOOKY))
		l_iWillpower--;

	// Bullet impacts make citizens jumpy
	if (HasCondition(COND_HEAR_BULLET_IMPACT))
		l_iWillpower--;

	if (HasCondition(COND_HEAR_DANGER))
		l_iWillpower--;

	if (HasCondition(COND_HEAR_PHYSICS_DANGER))
		l_iWillpower--;

#ifdef EZ2
	// If this citizen is not a brute or a jump rebel, their willpower is below a certain threshold, the enemy can see them, and the enemy is either Bad Cop or a soldier,
	// try to surrender
	if (typeCanPanic && l_iWillpower <= sk_citizen_very_low_willpower.GetInt() && HasCondition( COND_ENEMY_FACING_ME ) && GetEnemy())
	{
		bool bCanSurrender = GetEnemy()->IsPlayer();
		if (!bCanSurrender)
		{
			// Allow surrender if the enemy is a Combine soldier capable of ordering it
			if (CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine *>(GetEnemy()))
				bCanSurrender = pCombine->CanOrderSurrender();
		}

		if (bCanSurrender)
		{
			if (!HasCondition( COND_CIT_WILLPOWER_VERY_LOW ))
			{
				if (m_pSquad && m_pSquad->NumMembers() == 1) // Last one
				{
					MsgWillpower( "%s is now panicked and is the last squadmate!\tWillpower: %i\n", l_iWillpower );
				}
				else
				{
					MsgWillpower( "%s is now panicked!\tWillpower: %i\n", l_iWillpower );
				}
			}
			ClearCondition( COND_CIT_WILLPOWER_HIGH );
			SetCondition( COND_CIT_WILLPOWER_LOW );
			SetCondition( COND_CIT_WILLPOWER_VERY_LOW );
		}
	}
	else
#endif
	if (typeCanPanic && l_iWillpower <= 0 )
	{
		if ( !HasCondition(COND_CIT_WILLPOWER_LOW) )
		{
			if (m_pSquad && m_pSquad->NumMembers() == 1) // Last one
			{
				MsgWillpower("%s is now scared and is the last squadmate!\tWillpower: %i\n", l_iWillpower);
			}
			else 
			{
				MsgWillpower("%s is now scared!\tWillpower: %i\n", l_iWillpower);
			}
		}
		ClearCondition(COND_CIT_WILLPOWER_VERY_LOW);
		ClearCondition(COND_CIT_WILLPOWER_HIGH);
		SetCondition(COND_CIT_WILLPOWER_LOW);
	}
	else if (l_iWillpower > 1)
	{
		if (!HasCondition(COND_CIT_WILLPOWER_HIGH))
			MsgWillpower("%s is now brave!\tWillpower: %i\n", l_iWillpower);
		ClearCondition( COND_CIT_WILLPOWER_VERY_LOW );
		ClearCondition(COND_CIT_WILLPOWER_LOW);
		SetCondition(COND_CIT_WILLPOWER_HIGH);
	}
	else {
		ClearCondition(COND_CIT_WILLPOWER_HIGH);
		ClearCondition(COND_CIT_WILLPOWER_LOW);
		ClearCondition(COND_CIT_WILLPOWER_VERY_LOW);
	}
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: Overridden so that citizens cannot melee attack without a weapon
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Citizen::MeleeAttack1Conditions ( float flDot, float flDist )
{
	if ( GetActiveWeapon() == NULL )
	{
		return COND_NONE;
	}

	return BaseClass::MeleeAttack1Conditions( flDot, flDist );
}
#endif

Disposition_t CNPC_Citizen::IRelationType(CBaseEntity * pTarget)
{
	Disposition_t disposition = BaseClass::IRelationType(pTarget);
	// If the citizen has low willpower, fear an enemy instead of hating them
	if (disposition == D_HT && HasCondition(COND_CIT_WILLPOWER_LOW)) {
		return D_FR;
	}

#ifdef EZ2
	if (m_SurrenderBehavior.IsSurrendered() && pTarget && pTarget->Classify() == CLASS_CITIZEN_PASSIVE && disposition == D_NU)
	{
		// Surrendered citizens like each other (for speech, etc.)
		return D_LI;
	}
#endif

	return disposition;
}

void CNPC_Citizen::MsgWillpower(const tchar * pMsg, int willpower)
{
	if (ai_debug_willpower.GetBool() && m_flLastWillpowerMsgTime <= gpGlobals->curtime) {
		DevMsg(pMsg, GetDebugName(), willpower);
		m_flLastWillpowerMsgTime = gpGlobals->curtime + 1.0;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char* CNPC_Citizen::GetSquadSlotDebugName(int iSquadSlot)
{
	switch (iSquadSlot)
	{
	case SQUAD_SLOT_CITIZEN_RPG1:			return "SQUAD_SLOT_CITIZEN_RPG1";
		break;
	case SQUAD_SLOT_CITIZEN_RPG2:			return "SQUAD_SLOT_CITIZEN_RPG2";
		break;
	case SQUAD_SLOT_CITIZEN_INVESTIGATE:	return "SQUAD_SLOT_CITIZEN_INVESTIGATE";
		break;
	case SQUAD_SLOT_CITIZEN_ADVANCE:		return "SQUAD_SLOT_CITIZEN_ADVANCE";
		break;
	}

	return BaseClass::GetSquadSlotDebugName(iSquadSlot);
}
#endif

#ifdef EZ2
// 
// Blixibon - Needed so the player's speech AI doesn't pick this up as D_FR before it's apparent (e.g. fast, rapid kills)
// 
bool CNPC_Citizen::JustStartedFearing( CBaseEntity *pTarget )
{
	Assert( IRelationType( pTarget ) == D_FR );

	// We don't have a built-in way to figue out when we started panicking,
	// but if we just said TLK_DANGER, our fear sound, don't register.
	if ( gpGlobals->curtime - GetExpresser()->GetTimeSpokeConcept(TLK_DANGER) < 1.0f )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// This rebel dropped their weapons and might need a new one
//-----------------------------------------------------------------------------
bool CNPC_Citizen::GiveBackupWeapon( CBaseCombatWeapon * pWeapon, CBaseEntity * pActivator )
{
	if ( m_iLastHolsteredWeapon > 0)
	{
		DevMsg( "Already have multiple weapons: %i\n", m_iLastHolsteredWeapon );
		// Already have another weapon, don't need a new backup
		// Return true because there is another weapon

		m_bUsedBackupWeapon = true; // Don't give any more backup weapons!

		return true;
	}

	if ( m_bUsedBackupWeapon )
	{
		return false;
	}

	// Don't give any more backup weapons!
	m_bUsedBackupWeapon = true;

	if ( pWeapon != NULL )
	{
		// Is this a melee weapon?
		if ( pWeapon->IsMeleeWeapon() || pWeapon->ClassMatches( "weapon_crowbar" ) )
		{
			// No more backups
			return false;
		}
		// Is this weapon already a side arm?
		else if ( pWeapon->ClassMatches( "weapon_smg1" ) || pWeapon->ClassMatches( "weapon_smg2" ) || pWeapon->WeaponClassify() == WEPCLASS_HANDGUN )
		{
			// Very lucky citizens get crowbars as backup
			if ( RandomInt( 1, 6 ) == 1 )
			{
				CBaseCombatWeapon * pCrowbar = GiveWeaponHolstered( AllocPooledString( "weapon_crowbar" ) );
				pCrowbar->SetName( AllocPooledString( "worthless" ) ); // Bad Cop will say the crowbar pickup line upon interacting with this

				return true;
			}
			// Others usually just get a pistol if they didn't have one already
			else if ( !pWeapon->WeaponClassify() != WEPCLASS_HANDGUN && RandomInt( 1, 4 ) != 1 )
			{
				// If we're an Arbeit type, 50% chance of giving a glock
				if (GetEZVariant() == EZ_VARIANT_ARBEIT && RandomInt(1,2) == 1)
				{
					GiveWeaponHolstered( AllocPooledString( "weapon_css_glock" ) );
				}
				else
				{
					GiveWeaponHolstered( AllocPooledString( "weapon_pistol" ) );
				}
				return true;
			}

			return false;
		}
	}

	GiveWeaponHolstered( AllocPooledString( m_Type == CT_BRUTE ? "weapon_smg2" : "weapon_smg1" ) );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Speak TLK_BEG if the right conditions exist
//		Returns true if the concept is spoken
//-----------------------------------------------------------------------------
bool CNPC_Citizen::TrySpeakBeg()
{
	// If we are unarmed and not actively searching for a weapon to fight with, beg for mercy
	if ( GetEnemy() != NULL && GetActiveWeapon() == NULL && m_flNextWeaponSearchTime > gpGlobals->curtime )
	{
		return SpeakIfAllowed( TLK_BEG );
	}

	return false;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::GatherConditions()
{
	BaseClass::GatherConditions();
#ifdef EZ
	GatherWillpowerConditions();
#endif
	if( IsInPlayerSquad() && hl2_episodic.GetBool() )
	{
		// Leave the player squad if someone has made me neutral to player.
		if( IRelationType(UTIL_GetLocalPlayer()) == D_NU )
		{
			RemoveFromPlayerSquad();
		}
	}

	if ( !SpokeConcept( TLK_JOINPLAYER ) && IsRunningScriptedSceneWithSpeech( this, true ) )
	{
		SetSpokeConcept( TLK_JOINPLAYER, NULL );
		for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			CAI_BaseNPC *pNpc = g_AI_Manager.AccessAIs()[i];
			if ( pNpc != this && pNpc->GetClassname() == GetClassname() && pNpc->GetAbsOrigin().DistToSqr( GetAbsOrigin() ) < Square( 15*12 ) && FVisible( pNpc ) )
			{
				(assert_cast<CNPC_Citizen *>(pNpc))->SetSpokeConcept( TLK_JOINPLAYER, NULL );
			}
		}
	}

	if( ShouldLookForHealthItem() )
	{
		if( FindHealthItem( GetAbsOrigin(), Vector( 240, 240, 240 ) ) )
			SetCondition( COND_HEALTH_ITEM_AVAILABLE );
		else
			ClearCondition( COND_HEALTH_ITEM_AVAILABLE );

		m_flNextHealthSearchTime = gpGlobals->curtime + 4.0;
	}

	// If the player is standing near a medic and can see the medic, 
	// assume the player is 'staring' and wants health.
	if( CanHeal() )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();

		if ( !pPlayer )
		{
			m_flTimePlayerStare = FLT_MAX;
			return;
		}

		float flDistSqr = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length2DSqr();
		float flStareDist = sk_citizen_player_stare_dist.GetFloat();
		float flPlayerDamage = pPlayer->GetMaxHealth() - pPlayer->GetHealth();

		if( pPlayer->IsAlive() && flPlayerDamage > 0 && (flDistSqr <= flStareDist * flStareDist) && pPlayer->FInViewCone( this ) && pPlayer->FVisible( this )
#ifdef EZ2
			&& (!IsSurrendered() /*|| m_SurrenderBehavior.IsSurrenderIdleStanding()*/) // For now, surrendered medics only heal if you +USE them
#endif
			)
		{
			if( m_flTimePlayerStare == FLT_MAX )
			{
				// Player wasn't looking at me at last think. He started staring now.
				m_flTimePlayerStare = gpGlobals->curtime;
			}

			// Heal if it's been long enough since last time I healed a staring player.
			if( gpGlobals->curtime - m_flTimePlayerStare >= sk_citizen_player_stare_time.GetFloat() && gpGlobals->curtime > m_flTimeNextHealStare && !IsCurSchedule( SCHED_CITIZEN_HEAL ) )
			{
				if ( ShouldHealTarget( pPlayer, true ) )
				{
					SetCondition( COND_CIT_PLAYERHEALREQUEST );
				}
				else
				{
					m_flTimeNextHealStare = gpGlobals->curtime + sk_citizen_stare_heal_time.GetFloat() * .5f;
					ClearCondition( COND_CIT_PLAYERHEALREQUEST );
				}
			}

#ifdef HL2_EPISODIC
			// Heal if I'm on an assault. The player hasn't had time to stare at me.
			if( m_AssaultBehavior.IsRunning() && IsMoving() )
			{
				SetCondition( COND_CIT_PLAYERHEALREQUEST );
			}
#endif
		}
		else
		{
			m_flTimePlayerStare = FLT_MAX;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::PredictPlayerPush()
{
	if ( !AI_IsSinglePlayer() )
		return;

	if ( HasCondition( COND_CIT_PLAYERHEALREQUEST ) )
		return;

	bool bHadPlayerPush = HasCondition( COND_PLAYER_PUSHING );

	BaseClass::PredictPlayerPush();

#ifdef EZ2
	if ( IsSurrendered() /*&& !m_SurrenderBehavior.IsSurrenderIdleStanding()*/ ) // For now, surrendered medics only heal if you +USE them
		return;
#endif

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !bHadPlayerPush && HasCondition( COND_PLAYER_PUSHING ) && 
		 pPlayer->FInViewCone( this ) && CanHeal() )
	{
		if ( ShouldHealTarget( pPlayer, true ) )
		{
			ClearCondition( COND_PLAYER_PUSHING );
			SetCondition( COND_CIT_PLAYERHEALREQUEST );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	UpdatePlayerSquad();
	UpdateFollowCommandPoint();

	if ( !npc_citizen_insignia.GetBool() && npc_citizen_squad_marker.GetBool() && IsInPlayerSquad() )
	{
		Vector mins = WorldAlignMins() * .5 + GetAbsOrigin();
		Vector maxs = WorldAlignMaxs() * .5 + GetAbsOrigin();
		
		float rMax = 255;
		float gMax = 255;
		float bMax = 255;

		float rMin = 255;
		float gMin = 128;
		float bMin = 0;

		const float TIME_FADE = 1.0;
		float timeInSquad = gpGlobals->curtime - m_flTimeJoinedPlayerSquad;
		timeInSquad = MIN( TIME_FADE, MAX( timeInSquad, 0 ) );

		float fade = ( 1.0 - timeInSquad / TIME_FADE );

		float r = rMin + ( rMax - rMin ) * fade;
		float g = gMin + ( gMax - gMin ) * fade;
		float b = bMin + ( bMax - bMin ) * fade;

		// THIS IS A PLACEHOLDER UNTIL WE HAVE A REAL DESIGN & ART -- DO NOT REMOVE
		NDebugOverlay::Line( Vector( mins.x, GetAbsOrigin().y, GetAbsOrigin().z+1 ), Vector( maxs.x, GetAbsOrigin().y, GetAbsOrigin().z+1 ), r, g, b, false, .11 );
		NDebugOverlay::Line( Vector( GetAbsOrigin().x, mins.y, GetAbsOrigin().z+1 ), Vector( GetAbsOrigin().x, maxs.y, GetAbsOrigin().z+1 ), r, g, b, false, .11 );
	}
	if( GetEnemy() && g_ai_citizen_show_enemy.GetBool() )
	{
		NDebugOverlay::Line( EyePosition(), GetEnemy()->EyePosition(), 255, 0, 0, false, .1 );
	}
	
	if ( DebuggingCommanderMode() )
	{
		if ( HaveCommandGoal() )
		{
			CBaseEntity *pCommandPoint = gEntList.FindEntityByClassname( NULL, COMMAND_POINT_CLASSNAME );
			
			if ( pCommandPoint )
			{
				NDebugOverlay::Cross3D(pCommandPoint->GetAbsOrigin(), 16, 0, 255, 255, false, 0.1 );
			}
		}
	}

#ifdef EZ
	if( IsOnFire() )
	{
		SetCondition( COND_CIT_ON_FIRE );
	}
	else
	{
		ClearCondition( COND_CIT_ON_FIRE );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Citizen::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

#ifdef EZ
	// Don't stop meleeing to acquire a new target or because of damage taken
	if (IsCurSchedule ( SCHED_MELEE_ATTACK1 ))
	{
		ClearCustomInterruptCondition( COND_NEW_ENEMY );
		ClearCustomInterruptCondition( COND_LIGHT_DAMAGE );
		ClearCustomInterruptCondition( COND_HEAVY_DAMAGE );
		
	}

	if ( ( IsCurSchedule ( SCHED_ESTABLISH_LINE_OF_FIRE ) || IsCurSchedule ( SCHED_ESTABLISH_LINE_OF_FIRE_FALLBACK ) ) && ( m_Type == CT_BRUTE || m_Type == CT_LONGFALL || m_iMySquadSlot == SQUAD_SLOT_CITIZEN_ADVANCE ) )
	{
		ClearCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
	}

	// Being disarmed interrupts any schedule
	SetCustomInterruptCondition( COND_CIT_DISARMED );
#endif

	if ( IsCurSchedule( SCHED_IDLE_STAND ) || IsCurSchedule( SCHED_ALERT_STAND ) )
	{
		SetCustomInterruptCondition( COND_CIT_START_INSPECTION );
	}

#ifdef EZ
	if ( !IsCurSchedule( SCHED_CITIZEN_BURNING_STAND ) && !IsMoving() )
	{
		SetCustomInterruptCondition( COND_CIT_ON_FIRE );
	}
#endif

	if ( IsMedic() && IsCustomInterruptConditionSet( COND_HEAR_MOVE_AWAY ) )
	{
		if( !IsCurSchedule(SCHED_RELOAD, false) )
		{
			// Since schedule selection code prioritizes reloading over requests to heal
			// the player, we must prevent this condition from breaking the reload schedule.
			SetCustomInterruptCondition( COND_CIT_PLAYERHEALREQUEST );
		}

		SetCustomInterruptCondition( COND_CIT_COMMANDHEAL );
	}

	if( !IsCurSchedule( SCHED_NEW_WEAPON ) )
	{
		SetCustomInterruptCondition( COND_RECEIVED_ORDERS );
	}

	if( GetCurSchedule()->HasInterrupt( COND_IDLE_INTERRUPT ) )
	{
		SetCustomInterruptCondition( COND_BETTER_WEAPON_AVAILABLE );
	}

#ifdef HL2_EPISODIC
	if( IsMedic() && m_AssaultBehavior.IsRunning() )
	{
		if( !IsCurSchedule(SCHED_RELOAD, false) )
		{
			SetCustomInterruptCondition( COND_CIT_PLAYERHEALREQUEST );
		}

		SetCustomInterruptCondition( COND_CIT_COMMANDHEAL );
	}
#else
	if( IsMedic() && m_AssaultBehavior.IsRunning() && !IsMoving() )
	{
		if( !IsCurSchedule(SCHED_RELOAD, false) )
		{
			SetCustomInterruptCondition( COND_CIT_PLAYERHEALREQUEST );
		}

		SetCustomInterruptCondition( COND_CIT_COMMANDHEAL );
	}
#endif
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::FInViewCone( CBaseEntity *pEntity )
{
#if 0
	if ( IsMortar( pEntity ) )
	{
		// @TODO (toml 11-20-03): do this only if have heard mortar shell recently and it's active
		return true;
	}
#endif
	return BaseClass::FInViewCone( pEntity );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	switch( failedSchedule )
	{
	case SCHED_NEW_WEAPON:
		// If failed trying to pick up a weapon, try again in one second. This is because other AI code
		// has put this off for 10 seconds under the assumption that the citizen would be able to 
		// pick up the weapon that they found. 
		m_flNextWeaponSearchTime = gpGlobals->curtime + 1.0f;
		break;

	case SCHED_ESTABLISH_LINE_OF_FIRE_FALLBACK:
	case SCHED_MOVE_TO_WEAPON_RANGE:
		if( !IsMortar( GetEnemy() ) )
		{
			if ( GetActiveWeapon() && ( GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK1 ) && random->RandomInt( 0, 1 ) && HasCondition(COND_SEE_ENEMY) && !HasCondition ( COND_NO_PRIMARY_AMMO ) )
				return TranslateSchedule( SCHED_RANGE_ATTACK1 );

			return SCHED_STANDOFF;
		}
		break;
#ifdef EZ
	case SCHED_RANGE_ATTACK1:
		if (!GetShotRegulator()->IsInRestInterval()) // If the reason range attack 1 failed was because of the shot regulator, don't try it again
		{
			int attackSchedule = TranslateSuppressingFireSchedule(failedSchedule);
			if (attackSchedule != failedSchedule)
				return attackSchedule;
		}
		break;
#endif
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectSchedule()
{
#ifdef EZ
#ifdef EZ2
	// TODO - Find a better spot for this!
	TrySpeakBeg();
#endif
	// Reserve strategy slot if I am a rebel brute, otherwise clear
	if ( m_Type == CT_BRUTE && !IsStrategySlotRangeOccupied( SQUAD_SLOT_CITIZEN_ADVANCE, SQUAD_SLOT_CITIZEN_ADVANCE ) )
	{
		OccupyStrategySlot( SQUAD_SLOT_CITIZEN_ADVANCE );
	}
	else if (m_iMySquadSlot == SQUAD_SLOT_CITIZEN_ADVANCE)
	{
		VacateStrategySlot();
	}

	// Clear the disarmed condition if it was set by being kicked
	ClearCondition( COND_CIT_DISARMED );
#endif

#ifdef MAPBASE
	if ( IsWaitingToRappel() && BehaviorSelectSchedule() )
	{
		return BaseClass::SelectSchedule();
	}

	if ( GetMoveType() == MOVETYPE_NONE && !Q_strncmp(STRING(GetEntityName()), "citizen_train_", 14) )
	{
		// Only "sit on train" if we're a citizen_train_
		Assert( GetMoveParent() && FClassnameIs( GetMoveParent(), "func_tracktrain" ) );
		return SCHED_CITIZEN_SIT_ON_TRAIN;
	}
#else
	// If we can't move, we're on a train, and should be sitting.
	if ( GetMoveType() == MOVETYPE_NONE )
	{
		// For now, we're only ever parented to trains. If you hit this assert, you've parented a citizen
		// to something else, and now we need to figure out a better system.
		Assert( GetMoveParent() && FClassnameIs( GetMoveParent(), "func_tracktrain" ) );
		return SCHED_CITIZEN_SIT_ON_TRAIN;
	}
#endif

#ifdef MAPBASE
	if ( GetActiveWeapon() && EntIsClass(GetActiveWeapon(), gm_isz_class_RPG) )
	{
		CWeaponRPG *pRPG = static_cast<CWeaponRPG*>(GetActiveWeapon());
#else
	CWeaponRPG *pRPG = dynamic_cast<CWeaponRPG*>(GetActiveWeapon());
#endif
	if ( pRPG && pRPG->IsGuiding() )
	{
		DevMsg( "Citizen in select schedule but RPG is guiding?\n");
		pRPG->StopGuiding();
	}
#ifdef MAPBASE
	}
#endif

#ifdef EZ
	if ( HasCondition(COND_CIT_ON_FIRE) )
	{
		SpeakIfAllowed(TLK_WOUND);
		return SCHED_CITIZEN_BURNING_STAND;
	}
#endif
	
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectSchedulePriorityAction()
{
#ifdef EZ2
	if ( IsSurrendered() && (!m_SurrenderBehavior.ShouldBeStanding() || m_SurrenderBehavior.IsInterruptable()) )
		return SCHED_NONE;
#endif

	int schedule = SelectScheduleHeal();
	if ( schedule != SCHED_NONE )
		return schedule;

	schedule = BaseClass::SelectSchedulePriorityAction();
	if ( schedule != SCHED_NONE )
		return schedule;

	schedule = SelectScheduleRetrieveItem();
	if ( schedule != SCHED_NONE )
		return schedule;

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Determine if citizen should perform heal action.
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectScheduleHeal()
{
	// episodic medics may toss the healthkits rather than poke you with them
#if HL2_EPISODIC

	if ( CanHeal() )
	{
		float flHealMoveRange = HEAL_MOVE_RANGE;
#ifdef EZ2
		// Surrendered medics have limited range
		if (IsSurrendered())
			flHealMoveRange = 128.0f;
#endif

		CBaseEntity *pEntity = PlayerInRange( GetLocalOrigin(), HEAL_TOSS_TARGET_RANGE );
#ifdef EZ2
		// Ugh, do I really HAVE to heal you?
		if ( pEntity && (!IsSurrendered() || HasCondition( COND_CIT_PLAYERHEALREQUEST )) )
#else
		if ( pEntity )
#endif
		{
			if ( USE_EXPERIMENTAL_MEDIC_CODE() && IsMedic() )
			{
				// use the new heal toss algorithm
				if ( ShouldHealTossTarget( pEntity, HasCondition( COND_CIT_PLAYERHEALREQUEST ) ) )
				{
					SetTarget( pEntity );
					return SCHED_CITIZEN_HEAL_TOSS;
				}
			}
			else if ( PlayerInRange( GetLocalOrigin(), flHealMoveRange ) )
			{
				// use old mechanism for ammo
				if ( ShouldHealTarget( pEntity, HasCondition( COND_CIT_PLAYERHEALREQUEST ) ) )
				{
					SetTarget( pEntity );
					return SCHED_CITIZEN_HEAL;
				}
			}

		}
		
		if ( m_pSquad )
		{
			pEntity = NULL;
			float distClosestSq = flHealMoveRange*flHealMoveRange;
			float distCurSq;
			
			AISquadIter_t iter;
			CAI_BaseNPC *pSquadmate = m_pSquad->GetFirstMember( &iter );
			while ( pSquadmate )
			{
				if ( pSquadmate != this )
				{
					distCurSq = ( GetAbsOrigin() - pSquadmate->GetAbsOrigin() ).LengthSqr();
					if ( distCurSq < distClosestSq && ShouldHealTarget( pSquadmate ) )
					{
						distClosestSq = distCurSq;
						pEntity = pSquadmate;
					}
				}

				pSquadmate = m_pSquad->GetNextMember( &iter );
			}
			
			if ( pEntity )
			{
				SetTarget( pEntity );
				return SCHED_CITIZEN_HEAL;
			}
		}
	}
	else
	{
		if ( HasCondition( COND_CIT_PLAYERHEALREQUEST ) )
			DevMsg( "Would say: sorry, need to recharge\n" );
	}
	
	return SCHED_NONE;

#else

	if ( CanHeal() )
	{
		CBaseEntity *pEntity = PlayerInRange( GetLocalOrigin(), HEAL_MOVE_RANGE );
		if ( pEntity && ShouldHealTarget( pEntity, HasCondition( COND_CIT_PLAYERHEALREQUEST ) ) )
		{
			SetTarget( pEntity );
			return SCHED_CITIZEN_HEAL;
		}

		if ( m_pSquad )
		{
			pEntity = NULL;
			float distClosestSq = HEAL_MOVE_RANGE*HEAL_MOVE_RANGE;
			float distCurSq;

			AISquadIter_t iter;
			CAI_BaseNPC *pSquadmate = m_pSquad->GetFirstMember( &iter );
			while ( pSquadmate )
			{
				if ( pSquadmate != this )
				{
					distCurSq = ( GetAbsOrigin() - pSquadmate->GetAbsOrigin() ).LengthSqr();
					if ( distCurSq < distClosestSq && ShouldHealTarget( pSquadmate ) )
					{
						distClosestSq = distCurSq;
						pEntity = pSquadmate;
					}
				}

				pSquadmate = m_pSquad->GetNextMember( &iter );
			}

			if ( pEntity )
			{
				SetTarget( pEntity );
				return SCHED_CITIZEN_HEAL;
			}
		}
	}
	else
	{
		if ( HasCondition( COND_CIT_PLAYERHEALREQUEST ) )
			DevMsg( "Would say: sorry, need to recharge\n" );
	}

	return SCHED_NONE;

#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectScheduleRetrieveItem()
{
	if ( HasCondition(COND_BETTER_WEAPON_AVAILABLE) )
	{
		CBaseHLCombatWeapon *pWeapon = dynamic_cast<CBaseHLCombatWeapon *>(Weapon_FindUsable( WEAPON_SEARCH_DELTA ));
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
		if( !IsInPlayerSquad() )
		{
			// Been kicked out of the player squad since the time I located the health.
			ClearCondition( COND_HEALTH_ITEM_AVAILABLE );
		}
#ifdef MAPBASE
		else if ( m_FollowBehavior.GetFollowTarget() )
#else
		else
#endif
		{
			CBaseEntity *pBase = FindHealthItem(m_FollowBehavior.GetFollowTarget()->GetAbsOrigin(), Vector( 120, 120, 120 ) );
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
int CNPC_Citizen::SelectScheduleNonCombat()
{
	if ( m_NPCState == NPC_STATE_IDLE )
	{
		// Handle being inspected by the scanner
		if ( HasCondition( COND_CIT_START_INSPECTION ) )
		{
			ClearCondition( COND_CIT_START_INSPECTION );
			return SCHED_CITIZEN_PLAY_INSPECT_ACTIVITY;
		}
	}
	
	ClearCondition( COND_CIT_START_INSPECTION );

	if ( m_bShouldPatrol )
		return SCHED_CITIZEN_PATROL;
	
	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectScheduleManhackCombat()
{
	if ( m_NPCState == NPC_STATE_COMBAT && IsManhackMeleeCombatant() )
	{
		if ( !HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		{
			float distSqEnemy = ( GetEnemy()->GetAbsOrigin() - EyePosition() ).LengthSqr();
			if ( distSqEnemy < 48.0*48.0 &&
				 ( ( GetEnemy()->GetAbsOrigin() + GetEnemy()->GetSmoothedVelocity() * .1 ) - EyePosition() ).LengthSqr() < distSqEnemy )
				return SCHED_COWER;

			int iRoll = random->RandomInt( 1, 4 );
			if ( iRoll == 1 )
				return SCHED_BACK_AWAY_FROM_ENEMY;
			else if ( iRoll == 2 )
				return SCHED_CHASE_ENEMY;
		}
	}
	
	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::SelectScheduleCombat()
{
	int schedule = SelectScheduleManhackCombat();
	if ( schedule != SCHED_NONE )
		return schedule;
		
	return BaseClass::SelectScheduleCombat();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldDeferToFollowBehavior()
{
#if 0
	if ( HaveCommandGoal() )
		return false;
#endif
		
	return BaseClass::ShouldDeferToFollowBehavior();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::TranslateSchedule( int scheduleType ) 
{
#ifdef EZ
	if (!m_bWillpowerDisabled && ai_willpower_translate_schedules.GetBool()) {
		int willpowerSchedule = TranslateWillpowerSchedule(scheduleType);
		if (willpowerSchedule != SCHED_NONE) {
			if (ai_debug_willpower.GetBool())
				DevMsg("Citizen name: %s\tOld Schedule: %s\t New Schedule: %s\tWillpower: %s%s\n", GetDebugName(), GetSchedule(scheduleType)->GetName(), GetSchedule(willpowerSchedule)->GetName(), HasCondition(COND_CIT_WILLPOWER_HIGH) ? "High" : "", HasCondition(COND_CIT_WILLPOWER_LOW) ? "Low" : "");
			return willpowerSchedule;
		}
	}
#endif
	CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();

	switch( scheduleType )
	{
	case SCHED_IDLE_STAND:
	case SCHED_ALERT_STAND:
		if( m_NPCState != NPC_STATE_COMBAT && pLocalPlayer && !pLocalPlayer->IsAlive() && CanJoinPlayerSquad() && IRelationType(UTIL_GetLocalPlayer()) == D_LI)
		{
			// Player is dead! 
			float flDist;
			flDist = ( pLocalPlayer->GetAbsOrigin() - GetAbsOrigin() ).Length();

			if( flDist < 50 * 12 )
			{
				AddSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE );
				return SCHED_CITIZEN_MOURN_PLAYER;
			}
		}
		break;

	case SCHED_ESTABLISH_LINE_OF_FIRE:
	case SCHED_MOVE_TO_WEAPON_RANGE:
#ifdef EZ
		if (GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon())
		{
			return SCHED_CHASE_ENEMY;
		}

		// Per Breadman's changes to ai_basenpc_schedule, fire at where you think the enemy will be instead of advancing
		if ( !HasCondition(COND_SEE_ENEMY) && !HasCondition(COND_TOO_CLOSE_TO_ATTACK)) // We don't want this to use Attack squad slots because it could 'steal' a slot from a squadmate who might need it. 
		{
			int suppressingFireSchedule = TranslateSuppressingFireSchedule(scheduleType);
			if (suppressingFireSchedule != scheduleType)
				return suppressingFireSchedule;
		}
#endif
		if( !IsMortar( GetEnemy() ) && HaveCommandGoal() )
		{
			if ( GetActiveWeapon() && ( GetActiveWeapon()->CapabilitiesGet() & bits_CAP_WEAPON_RANGE_ATTACK1 ) && random->RandomInt( 0, 1 ) && HasCondition(COND_SEE_ENEMY) && !HasCondition ( COND_NO_PRIMARY_AMMO ) )
				return TranslateSchedule( SCHED_RANGE_ATTACK1 );

			return SCHED_STANDOFF;
		}
		break;

	case SCHED_CHASE_ENEMY:
		if( !IsMortar( GetEnemy() ) && HaveCommandGoal() )
		{
			return SCHED_STANDOFF;
		}
#ifdef EZ2
		// Don't chase enemies if you're weaponless!
		if ( GetActiveWeapon() == NULL )
		{
			if (m_SurrenderBehavior.SurrenderAutomatically() &&
				GetEnemy() && (GetEnemy()->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() < Square( npc_citizen_surrender_auto_distance.GetFloat() ))
			{
				CBaseCombatCharacter *pBCC = GetEnemy()->MyCombatCharacterPointer();
				if (pBCC && pBCC->FInViewCone( this ) && (pBCC->IsPlayer() || (pBCC->IsNPC() && pBCC->MyNPCPointer()->IsPlayerAlly())))
				{
					// Surrender automatically if we're allowed to
					if (DispatchInteraction( g_interactionBadCopOrderSurrender, NULL, pBCC ) && m_SurrenderBehavior.CanSelectSchedule())
					{
						Msg("Surrendering automatically!\n");
						return SCHED_STANDOFF;
					}
				}
			}

			return SCHED_STANDOFF;
		}
#endif


		break;

	case SCHED_RANGE_ATTACK1:
		// If we have an RPG, we use a custom schedule for it
#ifdef MAPBASE
		if ( !IsMortar( GetEnemy() ) && GetActiveWeapon() && EntIsClass(GetActiveWeapon(), gm_isz_class_RPG) )
		{
			if ( GetEnemy() && EntIsClass(GetEnemy(), gm_isz_class_Strider) )
#else
		if ( !IsMortar( GetEnemy() ) && GetActiveWeapon() && FClassnameIs( GetActiveWeapon(), "weapon_rpg" ) )
		{
			if ( GetEnemy() && GetEnemy()->ClassMatches( "npc_strider" ) )
#endif
			{
				if (OccupyStrategySlotRange( SQUAD_SLOT_CITIZEN_RPG1, SQUAD_SLOT_CITIZEN_RPG2 ) )
				{
					return SCHED_CITIZEN_STRIDER_RANGE_ATTACK1_RPG;
				}
				else
				{
					return SCHED_STANDOFF;
				}
			}
			else
			{
#ifndef MAPBASE // This has been disabled for now.
				CBasePlayer *pPlayer = AI_GetSinglePlayer();
#ifdef MAPBASE
				// Don't avoid player if notarget is on
				if ( pPlayer && GetEnemy() && !(pPlayer->GetFlags() & FL_NOTARGET) && ( ( GetEnemy()->GetAbsOrigin() - 
#else
				if ( pPlayer && GetEnemy() && ( ( GetEnemy()->GetAbsOrigin() - 
#endif
					pPlayer->GetAbsOrigin() ).LengthSqr() < RPG_SAFE_DISTANCE * RPG_SAFE_DISTANCE ) )
				{
					// Don't fire our RPG at an enemy too close to the player
					return SCHED_STANDOFF;
				}
				else
#endif
				{
					return SCHED_CITIZEN_RANGE_ATTACK1_RPG;
				}
			}
		}
		break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: translate the given scheduleType based on the willpower conditions
// (COND_CIT_WILLPOWER_LOW, COND_CIT_WILLPOWER_HIGH)
//		1upD
//-----------------------------------------------------------------------------
int CNPC_Citizen::TranslateWillpowerSchedule(int scheduleType)
{
	if ( GetActiveWeapon() && HasCondition( COND_CIT_WILLPOWER_VERY_LOW ) && ( random->RandomInt( 1, 4 ) == 1 || HasCondition( COND_NO_PRIMARY_AMMO ) ) )
	{
		// TODO Add a new schedule. For now, just drop it!
		Weapon_Drop( GetActiveWeapon() );

		// Don't look for another weapon for 15 seconds!
		m_flNextWeaponSearchTime = gpGlobals->curtime + 15.0;

		// Never use a backup weapon after this!
		m_bUsedBackupWeapon = true;

		// Don't shoot!
		SpeakIfAllowed( TLK_SURRENDER );

		// If we have LOS to the enemy, run away
		if ( HasCondition( COND_HAVE_ENEMY_LOS ) && random->RandomInt( 1, 2 ) == 1 )
		{
			return SCHED_RUN_FROM_ENEMY;
		}

		// Cower in fear
		return SCHED_PC_COWER;
	}

	switch (scheduleType)
	{
		// Don't investigate sounds if you're scared or unarmed
		case SCHED_INVESTIGATE_SOUND:
		if (HasCondition(COND_CIT_WILLPOWER_LOW) || HasCondition(COND_NO_WEAPON) || !OccupyStrategySlot(SQUAD_SLOT_CITIZEN_INVESTIGATE))
		{
			return SCHED_ALERT_FACE_BESTSOUND;
		}
		break;
	case SCHED_CHASE_ENEMY:
		if (HasCondition(COND_CIT_WILLPOWER_LOW) || HasCondition(COND_NO_WEAPON))
		{
#ifdef EZ2
			if (m_SurrenderBehavior.SurrenderAutomatically() && !GetActiveWeapon() /*&& HasCondition( COND_CIT_WILLPOWER_VERY_LOW )*/ &&
				GetEnemy() && (GetEnemy()->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() < Square( npc_citizen_surrender_auto_distance.GetFloat() ))
			{
				CBaseCombatCharacter *pBCC = GetEnemy()->MyCombatCharacterPointer();
				if (pBCC && pBCC->FInViewCone( this ) && (pBCC->IsPlayer() || (pBCC->IsNPC() && pBCC->MyNPCPointer()->IsPlayerAlly())))
				{
					// Surrender automatically if we're allowed to
					if (DispatchInteraction( g_interactionBadCopOrderSurrender, NULL, pBCC ) && m_SurrenderBehavior.CanSelectSchedule())
					{
						Msg("Surrendering automatically!\n");
						return SCHED_STANDOFF;
					}
				}
			}
#endif

			return SCHED_RUN_FROM_ENEMY; // If we are afraid, take cover!
		}
		break;
	case SCHED_COMBAT_FACE:
		// 1upD - don't just stand there, do something!
		if (HasCondition(COND_CIT_WILLPOWER_HIGH) && !HasCondition(COND_NO_WEAPON) && OccupyStrategySlot(SQUAD_SLOT_CITIZEN_ADVANCE)) // You have to be pretty brave to just rush at the enemy
			return SCHED_CHASE_ENEMY; // If we are brave, advance!
		break;
	case SCHED_RANGE_ATTACK1:
		if (!IsMortar(GetEnemy()) && GetActiveWeapon() && FClassnameIs(GetActiveWeapon(), "weapon_rpg")) 
		{
			return SCHED_NONE;
		}
		// If we are scared, back away while shooting!
		if (HasCondition(COND_CIT_WILLPOWER_LOW))
		{
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}
		// If we are brave, fire then advance!
		else if (HasCondition(COND_CIT_WILLPOWER_HIGH) && OccupyStrategySlot(SQUAD_SLOT_CITIZEN_ADVANCE) && !HasCondition(COND_NO_PRIMARY_AMMO))
		{
			return SCHED_CITIZEN_RANGE_ATTACK1_ADVANCE;
		}
		break;
	case SCHED_RELOAD:
	case SCHED_HIDE_AND_RELOAD:
		CBaseEntity * pEnemy = GetEnemy();

		if (pEnemy && ai_debug_willpower.GetBool())
			DevMsg("%s reloading. Distance to enemy: %f\n", GetDebugName(), (pEnemy->GetAbsOrigin() - GetAbsOrigin()).Length());

		// Interrupt reload with melee if possible
		if (HasCondition(COND_CAN_MELEE_ATTACK1)) {
			return SCHED_MELEE_ATTACK1;
		}

		if (pEnemy
			&& !HasCondition(COND_CIT_WILLPOWER_LOW)
			&& (pEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() < 128
			)
		{
			if (ai_debug_willpower.GetBool())
				DevMsg("%s out of ammo! Charging to enemy at distance: %f\n", GetDebugName(), (pEnemy->GetAbsOrigin() - GetAbsOrigin()).Length());
			return SCHED_CHASE_ENEMY;
		}

		break;
	}

	return SCHED_NONE;
}


 //-----------------------------------------------------------------------------
 // Purpose: trace a firing position to see if this citizen can suppress an enemy's position.
 //		1upD
 //-----------------------------------------------------------------------------
int CNPC_Citizen::TranslateSuppressingFireSchedule(int scheduleType)
{
	// First, try to throw a grenade if possible
	int rangeAttack2Schedule = SelectRangeAttack2Schedule();
	if (rangeAttack2Schedule != SCHED_NONE)
	{
		return rangeAttack2Schedule;
	}

	// If suppressive fire is disabled, stop
	if ( m_bSuppressiveFireDisabled )
	{
		return scheduleType;
	}

	if (HasCondition(COND_NO_PRIMARY_AMMO) || HasCondition(COND_NO_WEAPON)) 
	{
		return scheduleType;
	}

	if ( GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon() )
	{
		return scheduleType;
	}

	CBaseEntity * pEnemy = GetEnemy();

	if (!pEnemy) 
	{
		if(ai_debug_rebel_suppressing_fire.GetBool())
			DevMsg("NPC_Citizen::TranslateSuppressingFireSchedule: %s tried to suppress but couldn't get enemy! \n", GetDebugName());
		return scheduleType;
	}

#ifdef EZ1
	if(m_flLastAttackTime == 0)
	{
		if (ai_debug_rebel_suppressing_fire.GetBool())
			DevMsg("NPC_Citizen::TranslateSuppressingFireSchedule: %s tried to suppress but hasn't shot at any enemies yet! \n", GetDebugName());
		return scheduleType;
	}
#endif

	m_vecDecoyObjectTarget = vec3_invalid;
	if (FindDecoyObject() || FindEnemyCoverTarget()) {
		if (ai_debug_rebel_suppressing_fire.GetBool())
			DevMsg( "NPC_Citizen::TranslateSuppressingFireSchedule: %s is using suppressing fire! \n", GetDebugName() );
		return SCHED_CITIZEN_RANGE_ATTACK1_SUPPRESS;
	}

	return scheduleType;
}

// Overridden to check if we are panicking currently
int CNPC_Citizen::SelectRangeAttack2Schedule()
{
	if ( !m_bWillpowerDisabled && HasCondition( COND_CIT_WILLPOWER_VERY_LOW ) )
	{
		return SCHED_NONE;
	}

	return BaseClass::SelectRangeAttack2Schedule();
}

#define CITIZEN_DECOY_RADIUS 128.0f
#define CITIZEN_NUM_DECOYS 10
#define CITIZEN_DECOY_MAX_MASS 200.0f
bool CNPC_Citizen::FindDecoyObject(void)
{
	if ( !ai_suppression_shoot_props.GetBool() )
		return false;

	if(ai_debug_rebel_suppressing_fire.GetBool())
		DevMsg( "NPC_Citizen::FindDecoyObject: %s searching for decoy objects \n", GetDebugName() );
	#define SEARCH_DEPTH 50

	CBaseEntity *pDecoys[CITIZEN_NUM_DECOYS];
	CBaseEntity *pList[SEARCH_DEPTH];
	CBaseEntity	*pCurrent;
	int			count;
	int			i;
	Vector vecTarget = GetEnemy()->WorldSpaceCenter();
	Vector vecDelta;

	for (i = 0; i < CITIZEN_NUM_DECOYS; i++)
	{
		pDecoys[i] = NULL;
	}

	vecDelta.x = CITIZEN_DECOY_RADIUS;
	vecDelta.y = CITIZEN_DECOY_RADIUS;
	vecDelta.z = CITIZEN_DECOY_RADIUS;

	count = UTIL_EntitiesInBox(pList, SEARCH_DEPTH, vecTarget - vecDelta, vecTarget + vecDelta, 0);

	// Now we have the list of entities near the target. 
	// Dig through that list and build the list of decoys.
	int iIterator = 0;

	for (i = 0; i < count; i++)
	{
		pCurrent = pList[i];

		if (FClassnameIs(pCurrent, "func_breakable") || FClassnameIs(pCurrent, "prop_physics") || FClassnameIs(pCurrent, "func_physbox"))
		{
			if (!pCurrent->VPhysicsGetObject())
				continue;

			if (pCurrent->VPhysicsGetObject()->GetMass() > CITIZEN_DECOY_MAX_MASS)
			{
				// Skip this very heavy object. Probably a car or dumpster.
				continue;
			}

			if (pCurrent->VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
			{
				// Ah! If the player is holding something, try to shoot it!
				if (FVisible(pCurrent))
				{
					if ( ai_debug_rebel_suppressing_fire.GetBool() )
						DevMsg( "NPC_Citizen::FindDecoyObject: %s found player held object! \n", GetDebugName() );
					m_vecDecoyObjectTarget = pCurrent->WorldSpaceCenter();
					SetAimTarget( pCurrent );
					return true;
				}
			}

			pDecoys[iIterator] = pCurrent;
			if (iIterator == CITIZEN_NUM_DECOYS - 1)
			{
				break;
			}
			else
			{
				iIterator++;
			}
		}
	}

	if (iIterator == 0)
	{
		if ( ai_debug_rebel_suppressing_fire.GetBool() )
			DevMsg( "NPC_Citizen::FindDecoyObject: %s couldn't find any decoy targets. :( \n", GetDebugName() );
		return false;
	}

	// If the trace goes off, that's the object!
	for (i = 0; i < CITIZEN_NUM_DECOYS; i++)
	{
		CBaseEntity *pProspect;
		trace_t		tr, blockTr;

		// // Pick one of the decoys at random.
		pProspect = pDecoys[random->RandomInt(0, iIterator - 1)];
		if (pProspect == NULL)
		{
			Msg( "Citizen %s tried to access null decoy target \n", GetDebugName() );
			continue;
		}


		Vector vecDecoyTarget;
		Vector vecBulletOrigin;

		vecBulletOrigin = Weapon_ShootPosition();
		pProspect->CollisionProp()->RandomPointInBounds(Vector(.1, .1, .1), Vector(.6, .6, .6), &vecDecoyTarget);

		// Right now, tracing with MASK_BLOCKLOS and checking the fraction as well as the object the trace
		// has hit makes it possible for the decoy behavior to shoot through glass. 
		UTIL_TraceLine( vecBulletOrigin, vecDecoyTarget,
			MASK_BLOCKLOS, this, COLLISION_GROUP_NONE, &tr );

		if (tr.m_pEnt == pProspect || tr.fraction == 1.0)
		{
			if (ai_debug_rebel_suppressing_fire.GetBool())
			{
				DevMsg( "NPC_Citizen::FindDecoyObject: %s has LOS to decoy target %s\n", GetDebugName(), pProspect->GetDebugName() );
				// Blue line - successful decoy trace
				NDebugOverlay::Line( vecBulletOrigin, vecDecoyTarget, 0, 0, 255, false, 5.0f );
			}

			// One more trace - let's make sure there's nothing immediately in front of the NPC to occlude this firing vector
			AI_TraceLine( vecBulletOrigin, vecDecoyTarget, MASK_SHOT, this, COLLISION_GROUP_NONE, &blockTr );
			if ( blockTr.fraction < ai_suppression_distance_ratio.GetFloat())
			{
				if(ai_debug_rebel_suppressing_fire.GetBool())
					DevMsg( "NPC_Citizen::FindDecoyObject: %s covering fire doesn't cover sufficent ratio at: %f\n", GetDebugName(), blockTr.fraction );
			}
			else
			{
				// Great! A shot will hit this object.
				m_vecDecoyObjectTarget = tr.endpos;
				SetAimTarget( pProspect );

				return true;
			}
		}

		if (ai_debug_rebel_suppressing_fire.GetBool())
		{
			DevMsg( "NPC_Citizen::FindDecoyObject: %s could not trace to decoy target %s\n", GetDebugName(), pProspect->GetDebugName() );
			// Cyan line - failed decoy trace
			NDebugOverlay::Line( vecBulletOrigin, vecDecoyTarget, 0, 255, 255, false, 5.0f );
		}
	}

	if (ai_debug_rebel_suppressing_fire.GetBool())
		DevMsg( "NPC_Citizen::FindDecoyObject: %s could not trace to any decoy target \n", GetDebugName() );
	return false;
}

bool CNPC_Citizen::FindEnemyCoverTarget(void)
{
	if (m_vecDecoyObjectTarget != vec3_invalid)
	{
		if (ai_debug_rebel_suppressing_fire.GetBool())
			DevMsg( "NPC_Citizen::FindEnemyCoverTarget: %s already selected decoy target \n", GetDebugName() );
		return false;
	}

	trace_t tr;
	Vector startpos = this->Weapon_ShootPosition();
	Vector targetpos = GetEnemy()->EyePosition();

	float distance = UTIL_DistApprox(startpos, targetpos);
	if (ai_debug_rebel_suppressing_fire.GetBool() && distance <= ai_min_suppression_distance.GetFloat())
	{
		DevMsg("NPC_Citizen::FindEnemyCoverTarget: %s too close to target to use suppressing fire at range: %f\n", GetDebugName(), distance);
	}

	AI_TraceLine(startpos, targetpos, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	if ( ai_debug_rebel_suppressing_fire.GetBool() )
	{
		// Yellow line - successful enemy cover target trace
		NDebugOverlay::Line( startpos, targetpos, 255, 255, 0, false, 5.0f );
	}

	float traceLength = abs(tr.fraction  * distance);
	if (ai_debug_rebel_suppressing_fire.GetBool() && tr.fraction < ai_suppression_distance_ratio.GetFloat())
	{
		DevMsg("NPC_Citizen::FindEnemyCoverTarget: %s covering fire doesn't cover sufficent ratio at: %f\n", GetDebugName(), tr.fraction);
	}

	if (traceLength > ai_min_suppression_distance.GetFloat() && tr.fraction >= ai_suppression_distance_ratio.GetFloat())
	{
		if (ai_debug_rebel_suppressing_fire.GetBool())
			DevMsg("NPC_Citizen::FindEnemyCoverTarget: %s found enemy cover target at range: %f\n", GetDebugName(), traceLength);
		m_vecDecoyObjectTarget = targetpos;
		m_vecDecoyObjectTarget.x += RandomInt( -16, 16 );
		m_vecDecoyObjectTarget.y += RandomInt( -16, 16 );
		m_vecDecoyObjectTarget.z += RandomInt( -32, 32 );
		return true;
	}
	else if (ai_debug_rebel_suppressing_fire.GetBool()) {
		DevMsg("NPC_Citizen::FindEnemyCoverTarget: %s failed to use suppressing fire at range: %f\n", GetDebugName(), traceLength);
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::AimGun()
{
	Vector vecAimDir;
	if (m_vecDecoyObjectTarget != vec3_invalid && FInAimCone( m_vecDecoyObjectTarget ))
	{
		float flDist;
		flDist = (m_vecDecoyObjectTarget - GetAbsOrigin()).Length2DSqr();

		// Aim at my target if it's in my cone
		vecAimDir = m_vecDecoyObjectTarget - Weapon_ShootPosition();;
		VectorNormalize( vecAimDir );
		SetAim( vecAimDir );
		return;
	}

	BaseClass::AimGun();
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldAcceptGoal( CAI_BehaviorBase *pBehavior, CAI_GoalEntity *pGoal )
{
	if ( BaseClass::ShouldAcceptGoal( pBehavior, pGoal ) )
	{
		CAI_FollowBehavior *pFollowBehavior = dynamic_cast<CAI_FollowBehavior *>(pBehavior );
		if ( pFollowBehavior )
		{
			if ( IsInPlayerSquad() )
			{
				m_hSavedFollowGoalEnt = (CAI_FollowGoal *)pGoal;
				return false;
			}
		}
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnClearGoal( CAI_BehaviorBase *pBehavior, CAI_GoalEntity *pGoal )
{
	if ( m_hSavedFollowGoalEnt == pGoal )
		m_hSavedFollowGoalEnt = NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_CIT_PLAY_INSPECT_SEQUENCE:
		SetIdealActivity( (Activity) m_nInspectActivity );
		break;

	case TASK_CIT_SIT_ON_TRAIN:
		if ( NameMatches("citizen_train_2") )
		{
			SetSequenceByName( "d1_t01_TrainRide_Sit_Idle" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
		else
		{
			SetSequenceByName( "d1_t01_TrainRide_Stand" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
		break;

	case TASK_CIT_LEAVE_TRAIN:
		if ( NameMatches("citizen_train_2") )
		{
			SetSequenceByName( "d1_t01_TrainRide_Sit_Exit" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
		else
		{
			SetSequenceByName( "d1_t01_TrainRide_Stand_Exit" );
			SetIdealActivity( ACT_DO_NOT_DISTURB );
		}
		break;
		
	case TASK_CIT_HEAL:
#if HL2_EPISODIC
	case TASK_CIT_HEAL_TOSS:
#endif
		if ( IsMedic() )
		{
			if ( GetTarget() && GetTarget()->IsPlayer() && GetTarget()->m_iMaxHealth == GetTarget()->m_iHealth )
			{
				// Doesn't need us anymore
				TaskComplete();
				break;
			}

#ifdef MAPBASE
			SetSpeechTarget( GetTarget() );
#endif
			Speak( TLK_HEAL );
		}
		else if ( IsAmmoResupplier() )
		{
#ifdef MAPBASE
			SetSpeechTarget( GetTarget() );
#endif
			Speak( TLK_GIVEAMMO );
		}
		SetIdealActivity( (Activity)ACT_CIT_HEAL );
		break;
	
	case TASK_CIT_RPG_AUGER:
		m_bRPGAvoidPlayer = false;
		SetWait( 15.0 ); // maximum time auger before giving up
		break;

	case TASK_CIT_SPEAK_MOURNING:
		if ( !IsSpeaking() && CanSpeakAfterMyself() )
		{
 			//CAI_AllySpeechManager *pSpeechManager = GetAllySpeechManager();

			//if ( pSpeechManager-> )

			Speak(TLK_PLDEAD);
		}
		TaskComplete();
		break;
#ifdef EZ
	case TASK_CIT_PAINT_SUPPRESSION_TARGET:
		// This task is handled once in RunTask
		break;

	case TASK_CIT_DIE_INSTANTLY:
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
#endif

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_WAIT_FOR_MOVEMENT:
		{
			if ( IsManhackMeleeCombatant() )
			{
				AddFacingTarget( GetEnemy(), 1.0, 0.5 );
			}

			BaseClass::RunTask( pTask );
			break;
		}

		case TASK_MOVE_TO_TARGET_RANGE:
		{
			// If we're moving to heal a target, and the target dies, stop
			if ( IsCurSchedule( SCHED_CITIZEN_HEAL ) && (!GetTarget() || !GetTarget()->IsAlive()) )
			{
				TaskFail(FAIL_NO_TARGET);
				return;
			}

			BaseClass::RunTask( pTask );
			break;
		}

		case TASK_CIT_PLAY_INSPECT_SEQUENCE:
		{
			AutoMovement();
			
			if ( IsSequenceFinished() )
			{
				TaskComplete();
			}
			break;
		}
		case TASK_CIT_SIT_ON_TRAIN:
		{
			// If we were on a train, but we're not anymore, enable movement
			if ( !GetMoveParent() )
			{
				SetMoveType( MOVETYPE_STEP );
				CapabilitiesAdd( bits_CAP_MOVE_GROUND );
				TaskComplete();
			}
			break;
		}

		case TASK_CIT_LEAVE_TRAIN:
		{
			if ( IsSequenceFinished() )
			{
				SetupVPhysicsHull();
				TaskComplete();
			}
			break;
		}

		case TASK_CIT_HEAL:
			if ( IsSequenceFinished() )
			{
				TaskComplete();
			}
			else if (!GetTarget())
			{
				// Our heal target was killed or deleted somehow.
				TaskFail(FAIL_NO_TARGET);
			}
			else
			{
				if ( ( GetTarget()->GetAbsOrigin() - GetAbsOrigin() ).Length2D() > HEAL_MOVE_RANGE/2 )
					TaskComplete();

				GetMotor()->SetIdealYawToTargetAndUpdate( GetTarget()->GetAbsOrigin() );
			}
			break;


#if HL2_EPISODIC
		case TASK_CIT_HEAL_TOSS:
			if ( IsSequenceFinished() )
			{
				TaskComplete();
			}
			else if (!GetTarget())
			{
				// Our heal target was killed or deleted somehow.
				TaskFail(FAIL_NO_TARGET);
			}
			else
			{
				GetMotor()->SetIdealYawToTargetAndUpdate( GetTarget()->GetAbsOrigin() );
			}
			break;

#endif

		case TASK_CIT_RPG_AUGER:
			{
				// Keep augering until the RPG has been destroyed
				CWeaponRPG *pRPG = dynamic_cast<CWeaponRPG*>(GetActiveWeapon());
				if ( !pRPG )
				{
					TaskFail( FAIL_ITEM_NO_FIND );
					return;
				}

				// Has the RPG detonated?
				if ( !pRPG->GetMissile() )
				{
					pRPG->StopGuiding();
					TaskComplete();
					return;
				}

				Vector vecLaserPos = pRPG->GetNPCLaserPosition();

				if ( !m_bRPGAvoidPlayer )
				{
					// Abort if we've lost our enemy
					if ( !GetEnemy() )
					{
						pRPG->StopGuiding();
						TaskFail( FAIL_NO_ENEMY );
						return;
					}

					// Is our enemy occluded?
					if ( HasCondition( COND_ENEMY_OCCLUDED ) )
					{
						// Turn off the laserdot, but don't stop augering
						pRPG->StopGuiding();
						return;
					}
					else if ( pRPG->IsGuiding() == false )
					{
						pRPG->StartGuiding();
					}

					Vector vecEnemyPos = GetEnemy()->BodyTarget(GetAbsOrigin(), false);
#ifndef MAPBASE // This has been disabled for now.
					CBasePlayer *pPlayer = AI_GetSinglePlayer();
#ifdef MAPBASE
					// Don't avoid player if notarget is on
					if ( pPlayer && !(pPlayer->GetFlags() & FL_NOTARGET) && ( ( vecEnemyPos - pPlayer->GetAbsOrigin() ).LengthSqr() < RPG_SAFE_DISTANCE * RPG_SAFE_DISTANCE ) )
#else
					if ( pPlayer && ( ( vecEnemyPos - pPlayer->GetAbsOrigin() ).LengthSqr() < RPG_SAFE_DISTANCE * RPG_SAFE_DISTANCE ) )
#endif
					{
						m_bRPGAvoidPlayer = true;
						Speak( TLK_WATCHOUT );
					}
					else
#endif
					{
						// Pull the laserdot towards the target
						Vector vecToTarget = (vecEnemyPos - vecLaserPos);
						float distToMove = VectorNormalize( vecToTarget );
#ifdef EZ2
						// Decrease distToMove the closer it is to the target
						if ( distToMove > 500 )
							distToMove -= 100;
						else if ( distToMove > 90 )
							distToMove *= 0.8f;
#else
						if ( distToMove > 90 )
							distToMove = 90;
#endif
						vecLaserPos += vecToTarget * distToMove;
					}
				}

				if ( m_bRPGAvoidPlayer )
				{
					// Pull the laserdot up
					vecLaserPos.z += 90;
				}

				if ( IsWaitFinished() )
				{
					pRPG->StopGuiding();
					TaskFail( FAIL_NO_SHOOT );
					return;
				}
				// Add imprecision to avoid obvious robotic perfection stationary targets
				float imprecision = 18*sin(gpGlobals->curtime);
				vecLaserPos.x += imprecision;
				vecLaserPos.y += imprecision;
				vecLaserPos.z += imprecision;
				pRPG->UpdateNPCLaserPosition( vecLaserPos );
			}
			break;
#ifdef EZ
		case TASK_CIT_PAINT_SUPPRESSION_TARGET:
		{
			// Wait for shot regulator to be ready
			if ( !GetShotRegulator()->IsInRestInterval() )
			{
				CBaseEntity *pEnemy = GetEnemy();
				if (!pEnemy)
					TaskFail( FAIL_NO_TARGET );

				CAI_BaseNPC *pTarget = CreateCustomTarget( m_vecDecoyObjectTarget, 2.0f );
				AddEntityRelationship( pTarget, IRelationType( pEnemy ), IRelationPriority( pEnemy ) );

				// Update or Create a memory entry for this target and make Alyx think she's seen this target recently.
				// This prevents the baseclass from not recognizing this target and forcing Alyx into 
				// SCHED_WAKE_ANGRY, which wastes time and causes her to change animation sequences rapidly.
				GetEnemies()->UpdateMemory( GetNavigator()->GetNetwork(), pTarget, pTarget->GetAbsOrigin(), 0.0f, true );
				AI_EnemyInfo_t *pMemory = GetEnemies()->Find( pTarget );

				if (pMemory)
				{
					// Pretend we've known about this target longer than we really have.
					pMemory->timeFirstSeen = gpGlobals->curtime - 10.0f;
				}

				SetEnemy( pTarget );

				if (ai_debug_rebel_suppressing_fire.GetBool())
				{
					// Green line - actual suppressing fire
					NDebugOverlay::Line( Weapon_ShootPosition(), m_vecDecoyObjectTarget, 0, 255, 0, false, 2.0f );
				}

				TaskComplete();
			}
			break;
		}
#endif
		default:
			BaseClass::RunTask( pTask );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : code - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::TaskFail( AI_TaskFailureCode_t code )
{
	// If our heal task has failed, push out the heal time
	if ( IsCurSchedule( SCHED_CITIZEN_HEAL ) )
	{
		m_flPlayerHealTime 	= gpGlobals->curtime + sk_citizen_heal_ally_delay.GetFloat();
	}

	if( code == FAIL_NO_ROUTE_BLOCKED && m_bNotifyNavFailBlocked )
	{
		m_OnNavFailBlocked.FireOutput( this, this );
	}

	BaseClass::TaskFail( code );
}

//-----------------------------------------------------------------------------
// Purpose: Override base class activiites
//-----------------------------------------------------------------------------
Activity CNPC_Citizen::NPC_TranslateActivity( Activity activity )
{
	if ( activity == ACT_MELEE_ATTACK1 )
	{
#ifdef MAPBASE
		// It could be the new weapon punt activity.
		if (GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon())
		{
			return ACT_MELEE_ATTACK_SWING;
		}
#else
		return ACT_MELEE_ATTACK_SWING;
#endif
	}

#ifndef MAPBASE // Covered by the new backup activity system
	// !!!HACK - Citizens don't have the required animations for shotguns, 
	// so trick them into using the rifle counterparts for now (sjb)
	if ( activity == ACT_RUN_AIM_SHOTGUN )
		return ACT_RUN_AIM_RIFLE;
	if ( activity == ACT_WALK_AIM_SHOTGUN )
		return ACT_WALK_AIM_RIFLE;
	if ( activity == ACT_IDLE_ANGRY_SHOTGUN )
		return ACT_IDLE_ANGRY_SMG1;
	if ( activity == ACT_RANGE_ATTACK_SHOTGUN_LOW )
		return ACT_RANGE_ATTACK_SMG1_LOW;
#endif

#ifdef MAPBASE
	if (m_bAlternateAiming)
	{
		if (activity == ACT_RUN_AIM_RIFLE)
			return ACT_RUN_AIM_RIFLE_STIMULATED;
		if (activity == ACT_WALK_AIM_RIFLE)
			return ACT_WALK_AIM_RIFLE_STIMULATED;

		if (activity == ACT_RUN_AIM_AR2)
			return ACT_RUN_AIM_AR2_STIMULATED;
		if (activity == ACT_WALK_AIM_AR2)
			return ACT_WALK_AIM_AR2_STIMULATED;

#if EXPANDED_HL2_WEAPON_ACTIVITIES
		if (activity == ACT_RUN_AIM_PISTOL)
			return ACT_RUN_AIM_PISTOL_STIMULATED;
		if (activity == ACT_WALK_AIM_PISTOL)
			return ACT_WALK_AIM_PISTOL_STIMULATED;
#endif
	}
#endif

#ifdef EZ
	if (IsOnFire())
	{
		switch (activity)
		{
			case ACT_WALK:
			case ACT_RUN:		activity = ACT_RUN_ON_FIRE; break;
			case ACT_IDLE:		activity = ACT_IDLE_ON_FIRE; break;
		}
	}
#endif

#ifdef EZ2
	// Unarmed citizens use 'panic readiness' activities!
	// Only if they have no weapon and either cannot throw grenades or are panicking, and are not jump rebels 
	bool disarmed = GetActiveWeapon() == NULL && m_Type != CT_LONGFALL && ( HasCondition( COND_CIT_WILLPOWER_VERY_LOW ) || !IsGrenadeCapable() || m_iNumGrenades == 0 );
	if ( (!m_bWillpowerDisabled && disarmed && m_NPCState == NPC_STATE_COMBAT) /*|| IsSurrendered()*/ )
	{
		switch (activity)
		{
		case ACT_RUN:
			return (Activity) ACT_RUN_PANICKED;
		case ACT_IDLE:
			return (Activity) ACT_CROUCH_PANICKED;
		case ACT_WALK:
			return (Activity) ACT_WALK_PANICKED;
		}
	}
#endif

	return BaseClass::NPC_TranslateActivity( activity );
}

#ifdef EZ
extern ConVar showhitlocation;

//=========================================================
// TraceAttack is overridden here for jump rebel purposes.
//=========================================================
void CNPC_Citizen::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	m_fNoDamageDecal = false;
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;

	SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	bool bDebug = showhitlocation.GetBool();

	switch ( ptr->hitgroup )
	{
	case HITGROUP_GENERIC:
		if( bDebug ) DevMsg("Hit Location: Generic\n");
		break;

	case HITGROUP_GEAR:
		// Blixibon - Allows jump rebels to use gear hitgroup as a weak point
		if (m_Type == CT_LONGFALL)
			subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		else
		{
			subInfo.SetDamage( 0.01 );
			ptr->hitgroup = HITGROUP_GENERIC;
		}
		if( bDebug ) DevMsg("Hit Location: Gear\n");
		break;

	case HITGROUP_HEAD:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Head\n");
		break;

	case HITGROUP_CHEST:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Chest\n");
		break;

	case HITGROUP_STOMACH:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Stomach\n");
		break;

	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Left/Right Arm\n");
		break
			;
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Left/Right Leg\n");
		break;

	default:
		if( bDebug ) DevMsg("Hit Location: UNKNOWN\n");
		break;
	}

	bool bBloodAllowed = DamageFilterAllowsBlood( info );
	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK ) && bBloodAllowed )
	{
		if (ptr->hitgroup == HITGROUP_GEAR)
		{
			// Blixibon - Jump rebels have weak sparks in response to gear damage
			g_pEffects->Sparks( ptr->endpos, 2, 2 );
		}
		else
			SpawnBlood( ptr->endpos, vecDir, BloodColor(), subInfo.GetDamage() );// a little surface blood.

		TraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );

		if ( ptr->hitgroup == HITGROUP_HEAD && m_iHealth - subInfo.GetDamage() > 0 )
		{
			m_fNoDamageDecal = true;
		}
	}
	else if (!bBloodAllowed)
	{
		m_fNoDamageDecal = true;
	}

	// Airboat gun will impart major force if it's about to kill him....
	if ( info.GetDamageType() & DMG_AIRBOAT )
	{
		if ( subInfo.GetDamage() >= GetHealth() )
		{
			float flMagnitude = subInfo.GetDamageForce().Length();
			if ( (flMagnitude != 0.0f) && (flMagnitude < 400.0f * 65.0f) )
			{
				subInfo.ScaleDamageForce( 400.0f * 65.0f / flMagnitude );
			}
		}
	}

	if( info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetInflictor() );
	}
	else
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::Event_Killed( const CTakeDamageInfo &info )
{
	if (m_Type == CT_BRUTE && GetBodygroup(BRUTE_MASK_BODYGROUP) != 1)
	{
		// Make the mask fly off under high damage
		if (info.GetDamage() >= npc_citizen_brute_mask_eject_threshold.GetFloat() && !(info.GetDamageType() & DMG_REMOVENORAGDOLL))
		{
			Vector vecMaskPos;
			QAngle vecMaskAng;
			GetAttachment( "anim_attachment_head", vecMaskPos, vecMaskAng );

			CBaseEntity *pGib = CreateNoSpawn( "prop_physics", vecMaskPos, vecMaskAng, this );
			if (pGib)
			{
				pGib->SetModelName( AllocPooledString(BRUTE_MASK_MODEL) );
				pGib->GetBaseAnimating()->m_nSkin = m_nSkin;
				pGib->AddSpawnFlags( SF_PHYSPROP_DEBRIS | SF_PHYSPROP_IS_GIB );
				DispatchSpawn( pGib );

				if (VPhysicsGetObject() && pGib->VPhysicsGetObject())
				{
					Vector velocity = info.GetDamageForce() * VPhysicsGetObject()->GetInvMass();

					// Give the mask some extra velocity so it's easier to see
					velocity.z += 20.0f;
					velocity *= 3.0f;

					pGib->VPhysicsGetObject()->AddVelocity(&velocity, NULL);
				}

				if (info.GetDamageType() & DMG_DISSOLVE)
				{
					pGib->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
				}
				else
				{
					pGib->SUB_StartFadeOut( 10.0f, false );
				}
			}

			SetBodygroup( BRUTE_MASK_BODYGROUP, 1 );
		}
	}
	else if (m_Type == CT_LONGFALL)
	{
		// Make the ragdoll bounce up when the gear is shot
		if (LastHitGroup() == HITGROUP_GEAR)
		{
			CTakeDamageInfo myInfo = info;

			Vector vecBackpack;
			GetAttachment( "backpack_mid", vecBackpack );

			g_pEffects->Sparks( vecBackpack, 4, 4 );
			UTIL_Smoke( vecBackpack, 20, 5 );

			myInfo.SetDamageForce( info.GetDamageForce() + Vector(0,0,npc_citizen_longfall_death_height.GetFloat()) );
			EmitSound("NPC_Citizen_JumpRebel.Jump_Death");

			// See if we'll hit a low ceiling
			Vector vecOrigin = WorldSpaceCenter();
			trace_t tr;
			AI_TraceLine(vecOrigin, vecOrigin + (myInfo.GetDamageForce() * VPhysicsGetObject()->GetInvMass() * 0.5f),
				MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

			if (tr.fraction != 1.0f)
			{
				UTIL_DecalTrace( &tr, "Rollermine.Crater" );

				// It seems that the blood decal now overwrites the rollermine crater
				//UTIL_DecalTrace( &tr, "Blood" );
			}

			BaseClass::Event_Killed( myInfo );

			if (m_hDeathRagdoll)
			{
				// Add a jump particle to the death ragdoll
				// TODO: citizen_longfall_jump_death?
				DispatchParticleEffect( "citizen_longfall_jump", PATTACH_POINT_FOLLOW, m_hDeathRagdoll, LookupAttachment( "backpack_end" ) );

				CRagdollBoogie::Create( m_hDeathRagdoll, 50, gpGlobals->curtime, RandomFloat(2.0f, 3.0f), SF_RAGDOLL_BOOGIE_ELECTRICAL );
			}

			return;
		}
	}

#ifdef EZ2
	// Medics ALWAYS drop healthkits. Reward the player for taking them out
	if ( IsMedic() )
	{
		DropItem( "item_healthkit", WorldSpaceCenter() + RandomVector( -4, 4 ), RandomAngle( 0, 360 ) );
	}

#endif

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnChangeActivity( Activity eNewActivity )
{
	BaseClass::OnChangeActivity( eNewActivity );

	if (m_Type == CT_LONGFALL)
	{
		switch (eNewActivity)
		{
			case ACT_JUMP:
			{
				// Do the jump particle
				DispatchParticleEffect( "citizen_longfall_jump", PATTACH_POINT_FOLLOW, this, LookupAttachment( "backpack_end" ) );

				// Make the chest eye a different color while we're jumping
				if (m_pEyeGlow)
				{
					m_pEyeGlow->SetRenderColor( LONGFALL_GLOW_CHEST_JUMP_R, LONGFALL_GLOW_CHEST_JUMP_G, LONGFALL_GLOW_CHEST_JUMP_B, LONGFALL_GLOW_CHEST_JUMP_A );
					m_pEyeGlow->SetBrightness( LONGFALL_GLOW_CHEST_JUMP_BRIGHT, npc_citizen_longfall_glow_chest_jump_fade_enter.GetFloat() );
					m_pEyeGlow->SetScale( LONGFALL_GLOW_CHEST_JUMP_SCALE, npc_citizen_longfall_glow_chest_jump_fade_enter.GetFloat() );
				}

				EmitSound( "NPC_Citizen_JumpRebel.Jump" );
			} break;

			case ACT_GLIDE:
				break;

			case ACT_LAND:
			{
				EmitSound( "NPC_Citizen_JumpRebel.Land" );
			}
			// Need to have a default case here due to jump rebels not always switching to ACT_LAND after jumping
			default:
			{
				// If we were jumping, return the chest eye to its normal color
				if (m_pEyeGlow && m_pEyeGlow->GetBrightness() == LONGFALL_GLOW_CHEST_JUMP_BRIGHT)
				{
					m_pEyeGlow->SetRenderColor( LONGFALL_GLOW_CHEST_R, LONGFALL_GLOW_CHEST_G, LONGFALL_GLOW_CHEST_B, LONGFALL_GLOW_CHEST_A );
					m_pEyeGlow->SetBrightness( LONGFALL_GLOW_CHEST_BRIGHT, npc_citizen_longfall_glow_chest_jump_fade_exit.GetFloat() );
					m_pEyeGlow->SetScale( LONGFALL_GLOW_CHEST_SCALE, npc_citizen_longfall_glow_chest_jump_fade_exit.GetFloat() );
				}
			}
			break;
		}
	}

	if (eNewActivity == ACT_MELEE_ATTACK1 && GetEnemy())
	{
		// Say something when we begin a melee attack
		SpeakIfAllowed( TLK_MELEE );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther( pVictim, info );

	if ( pVictim && pVictim->IsPlayer() )
	{
		// Blixibon - Citizens can shoot at Bad Cop's corpse after he dies.
		if (GetEnemies()->NumEnemies() <= 1 && HasCondition(COND_CIT_WILLPOWER_LOW))
		{
			CAI_BaseNPC *pTarget = CreateCustomTarget( pVictim->GetAbsOrigin(), 5.0f );
			pTarget->SetParent( pVictim );

			AISquadIter_t iter;
			for ( CAI_BaseNPC *pSquadmate = m_pSquad ? m_pSquad->GetFirstMember(&iter) : this; pSquadmate; pSquadmate = m_pSquad ? m_pSquad->GetNextMember(&iter) : NULL )
			{
				if (pSquadmate->GetClassname() != GetClassname())
					continue;

				// Do the Alyx bullseye relationship code.
				pSquadmate->AddEntityRelationship( pTarget, IRelationType(pVictim), IRelationPriority(pVictim) );
				pSquadmate->GetEnemies()->UpdateMemory( GetNavigator()->GetNetwork(), pTarget, pTarget->GetAbsOrigin(), 0.0f, true );
				AI_EnemyInfo_t *pMemory = pSquadmate->GetEnemies()->Find( pTarget );
				if( pMemory )
					pMemory->timeFirstSeen = gpGlobals->curtime - 10.0f;
			}
		}
	}
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_CITIZEN_GET_PACKAGE )
	{
		// Give the citizen a package
		CBaseCombatWeapon *pWeapon = Weapon_Create( "weapon_citizenpackage" );
		if ( pWeapon )
		{
			// If I have a name, make my weapon match it with "_weapon" appended
			if ( GetEntityName() != NULL_STRING )
			{
				pWeapon->SetName( AllocPooledString(UTIL_VarArgs("%s_weapon", STRING(GetEntityName()) )) );
			}
			Weapon_Equip( pWeapon );
		}
		return;
	}
	else if ( pEvent->event == AE_CITIZEN_HEAL )
	{
		// Heal my target (if within range)
#if HL2_EPISODIC
#ifdef MAPBASE
		// Don't throw medkits at NPCs, that's not how it works
		if ( USE_EXPERIMENTAL_MEDIC_CODE() && IsMedic() && GetTarget() && !GetTarget()->IsNPC() )
#else
		if ( USE_EXPERIMENTAL_MEDIC_CODE() && IsMedic() )
#endif
		{
			CBaseCombatCharacter *pTarget = dynamic_cast<CBaseCombatCharacter *>( GetTarget() );
			Assert(pTarget);
			if ( pTarget )
			{
				m_flPlayerHealTime 	= gpGlobals->curtime + sk_citizen_heal_toss_player_delay.GetFloat();;
				TossHealthKit( pTarget, Vector(48.0f, 0.0f, 0.0f)  );
			}
		}
		else
		{
			Heal();
		}
#else
		Heal();
#endif
		return;
	}

	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "NPC_Citizen.FootstepLeft", pEvent->eventtime );
		}
		break;

	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "NPC_Citizen.FootstepRight", pEvent->eventtime );
		}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

#ifndef MAPBASE // Moved to CAI_BaseNPC
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::PickupItem( CBaseEntity *pItem )
{
	Assert( pItem != NULL );
	if( FClassnameIs( pItem, "item_healthkit" ) )
	{
		if ( TakeHealth( sk_healthkit.GetFloat(), DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
		}
	}
	else if( FClassnameIs( pItem, "item_healthvial" ) )
	{
		if ( TakeHealth( sk_healthvial.GetFloat(), DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
		}
	}
	else
	{
		DevMsg("Citizen doesn't know how to pick up %s!\n", pItem->GetClassname() );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IgnorePlayerPushing( void )
{
	// If the NPC's on a func_tank that the player cannot man, ignore player pushing
	if ( m_FuncTankBehavior.IsMounted() )
	{
		CFuncTank *pTank = m_FuncTankBehavior.GetFuncTank();
		if ( pTank && !pTank->IsControllable() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return a random expression for the specified state to play over 
//			the state's expression loop.
//-----------------------------------------------------------------------------
const char *CNPC_Citizen::SelectRandomExpressionForState( NPC_STATE state )
{
	// Hacky remap of NPC states to expression states that we care about
	int iExpressionState = 0;
	switch ( state )
	{
	case NPC_STATE_IDLE:
		iExpressionState = 0;
		break;

	case NPC_STATE_ALERT:
		iExpressionState = 1;
		break;

	case NPC_STATE_COMBAT:
		iExpressionState = 2;
		break;

	default:
		// An NPC state we don't have expressions for
		return NULL;
	}

	// Now pick the right one for our expression type
	switch ( m_ExpressionType )
	{
	case CIT_EXP_SCARED:
		{
			int iRandom = RandomInt( 0, ARRAYSIZE(ScaredExpressions[iExpressionState].szExpressions)-1 );
			return ScaredExpressions[iExpressionState].szExpressions[iRandom];
		}

	case CIT_EXP_NORMAL:
		{
			int iRandom = RandomInt( 0, ARRAYSIZE(NormalExpressions[iExpressionState].szExpressions)-1 );
			return NormalExpressions[iExpressionState].szExpressions[iRandom];
		}

	case CIT_EXP_ANGRY:
		{
			int iRandom = RandomInt( 0, ARRAYSIZE(AngryExpressions[iExpressionState].szExpressions)-1 );
			return AngryExpressions[iExpressionState].szExpressions[iRandom];
		}

	default:
		break;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::SimpleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Under these conditions, citizens will refuse to go with the player.
	// Robin: NPCs should always respond to +USE even if someone else has the semaphore.
	m_bDontUseSemaphore = true;

	// First, try to speak the +USE concept
	if ( !SelectPlayerUseSpeech() )
	{
		if ( HasSpawnFlags(SF_CITIZEN_NOT_COMMANDABLE) || IRelationType( pActivator ) == D_NU )
		{
			// If I'm denying commander mode because a level designer has made that decision,
			// then fire this output in case they've hooked it to an event.
			m_OnDenyCommanderUse.FireOutput( this, this );
		}
	}

#ifdef EZ2
	if (IsSurrendered() && m_SurrenderBehavior.IsSurrenderIdle())
	{
		if (m_SurrenderBehavior.ShouldBeStanding())
		{
			// Check if the player is asking for health
			if (CanHeal() && ShouldHealTarget( pActivator, true ))
			{
				Msg( "Setting heal request\n" );
				SetCondition( COND_CIT_PLAYERHEALREQUEST );
			}
			else
			{
				Msg( "Surrendering to ground\n" );
				m_SurrenderBehavior.SurrenderToGround();
			}
		}
		else
		{
			Msg( "Surrendering to standing\n" );
			m_SurrenderBehavior.SurrenderStand();
		}
	}
#endif

	m_bDontUseSemaphore = false;
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldGib( const CTakeDamageInfo &info )
{
	if ( !npc_citizen_gib.GetBool() )
		return false;

	// If the damage type is "always gib", we better gib!
	if ( info.GetDamageType() & DMG_ALWAYSGIB )
		return true;

	if ( info.GetDamageType() & ( DMG_NEVERGIB | DMG_DISSOLVE ) )
		return false;

	// Jump rebels shouldn't gib because flying ragdolls are amusing
	if (m_Type == CT_LONGFALL)
		return false;

	if ( ( info.GetDamageType() & ( DMG_BLAST | DMG_ACID | DMG_POISON | DMG_CRUSH ) ) && ( GetAbsOrigin() - info.GetDamagePosition() ).LengthSqr() <= 32.0f )
		return true;

	return false;
}

//------------------------------------------------------------------------------
// Purpose: Override to do rebel specific gibs
// Output :
//------------------------------------------------------------------------------
bool CNPC_Citizen::CorpseGib( const CTakeDamageInfo &info )
{
	return BaseClass::CorpseGib( info );
}

//-----------------------------------------------------------------------------
// Purpose: Return the pointer for a given sprite given an index
//-----------------------------------------------------------------------------
CSprite	* CNPC_Citizen::GetGlowSpritePtr( int index )
{
	if (m_Type == CT_LONGFALL)
	{
		switch (index)
		{
			case 0:
				return m_pEyeGlow;
			case 1:
				return m_pShoulderGlow;
		}
	}

	return BaseClass::GetGlowSpritePtr( index );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the glow sprite at the given index
//-----------------------------------------------------------------------------
void CNPC_Citizen::SetGlowSpritePtr( int index, CSprite * sprite )
{
	if (m_Type == CT_LONGFALL)
	{
		switch (index)
		{
			case 0:
				m_pEyeGlow = sprite;
				break;
			case 1:
				m_pShoulderGlow = sprite;
				break;
		}
	}

	BaseClass::SetGlowSpritePtr( index, sprite );
}

//-----------------------------------------------------------------------------
// Purpose: Return the glow attributes for a given index
//-----------------------------------------------------------------------------
EyeGlow_t * CNPC_Citizen::GetEyeGlowData( int index )
{
	if (m_Type == CT_LONGFALL)
	{
		EyeGlow_t * eyeGlow = new EyeGlow_t();

		switch (index){
		case 0:
			eyeGlow->spriteName = AllocPooledString(LONGFALL_GLOW_CHEST_SPRITE);
			eyeGlow->attachment = AllocPooledString("backpack_eye_chest");

			eyeGlow->alpha = LONGFALL_GLOW_CHEST_A;
			eyeGlow->brightness = LONGFALL_GLOW_CHEST_BRIGHT;

			eyeGlow->red = LONGFALL_GLOW_CHEST_R;
			eyeGlow->green = LONGFALL_GLOW_CHEST_G;
			eyeGlow->blue = LONGFALL_GLOW_CHEST_B;

			eyeGlow->renderMode = kRenderWorldGlow;
			eyeGlow->scale = 0.5f;
			eyeGlow->proxyScale = 2.0f;

			return eyeGlow;
		case 1:
			eyeGlow->spriteName = AllocPooledString(LONGFALL_GLOW_SHOULDER_SPRITE);
			eyeGlow->attachment = AllocPooledString("backpack_eye_shoulder");

			eyeGlow->alpha = 192;
			eyeGlow->brightness = 164;

			eyeGlow->red = 255;
			eyeGlow->green = 245;
			eyeGlow->blue = 180;

			eyeGlow->renderMode = kRenderWorldGlow;
			eyeGlow->scale = 0.15f;
			eyeGlow->proxyScale = 1.0f;

			return eyeGlow;
		}
	}

	return BaseClass::GetEyeGlowData(index);
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of glows
//-----------------------------------------------------------------------------
int CNPC_Citizen::GetNumGlows()
{
	if (m_Type == CT_LONGFALL)
		return 2;
	else
		return BaseClass::GetNumGlows();
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::OnBeginMoveAndShoot()
{
	if ( BaseClass::OnBeginMoveAndShoot() )
	{
#ifdef EZ
		if ( HasCondition(COND_NO_PRIMARY_AMMO) )
			return false;

		if( HasAttackSlot() )
#else
		if( m_iMySquadSlot == SQUAD_SLOT_ATTACK1 || m_iMySquadSlot == SQUAD_SLOT_ATTACK2 )
#endif
			return true; // already have the slot I need

		if( m_iMySquadSlot == SQUAD_SLOT_NONE && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnEndMoveAndShoot()
{
#ifdef EZ
	// Don't vacate strategy slot if that slot is 'advance'
	if( m_iMySquadSlot == SQUAD_SLOT_CITIZEN_ADVANCE )
		return
#endif
	VacateStrategySlot();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#ifdef EZ
bool CNPC_Citizen::HasAttackSlot()
{
	return m_iMySquadSlot == SQUAD_SLOT_CITIZEN_ADVANCE || BaseClass::HasAttackSlot();
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// BREADMAN - This is interesting...
// I've commented this back in to see what it does!
void CNPC_Citizen::LocateEnemySound()
{
#ifdef EZ
	if ( !GetEnemy() )
		return;

	float flZDiff = GetLocalOrigin().z - GetEnemy()->GetLocalOrigin().z;

	if( flZDiff < -128 )
	{
		EmitSound( "NPC_Citizen.UpThere" );
	}
	else if( flZDiff > 128 )
	{
		EmitSound( "NPC_Citizen.DownThere" );
	}
	else
	{
		EmitSound( "NPC_Citizen.OverHere" );
	}
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsManhackMeleeCombatant()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	CBaseEntity *pEnemy = GetEnemy();
#ifdef MAPBASE
	// Any melee weapon passes
	return ( pEnemy && pWeapon && pEnemy->Classify() == CLASS_MANHACK && pWeapon->IsMeleeWeapon() );
#else
	return ( pEnemy && pWeapon && pEnemy->Classify() == CLASS_MANHACK && pWeapon->ClassMatches( "weapon_crowbar" ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return the actual position the NPC wants to fire at when it's trying
//			to hit it's current enemy.
//-----------------------------------------------------------------------------
Vector CNPC_Citizen::GetActualShootPosition( const Vector &shootOrigin )
{
	Vector vecTarget = BaseClass::GetActualShootPosition( shootOrigin );

#ifdef MAPBASE
	// The gunship RPG code does not appear to be funcitonal, so only set the laser position.
	if ( GetActiveWeapon() && EntIsClass(GetActiveWeapon(), gm_isz_class_RPG) && GetEnemy() )
	{
		CWeaponRPG *pRPG = static_cast<CWeaponRPG*>(GetActiveWeapon());
		pRPG->SetNPCLaserPosition( vecTarget );
	}
#else
	CWeaponRPG *pRPG = dynamic_cast<CWeaponRPG*>(GetActiveWeapon());
	// If we're firing an RPG at a gunship, aim off to it's side, because we'll auger towards it.
	if ( pRPG && GetEnemy() )
	{
		if ( FClassnameIs( GetEnemy(), "npc_combinegunship" ) )
		{
			Vector vecRight;
			GetVectors( NULL, &vecRight, NULL );
			// Random height
			vecRight.z = 0;

			// Find a clear shot by checking for clear shots around it
			float flShotOffsets[] =
			{
				512,
				-512,
				128,
				-128
			};
			for ( int i = 0; i < ARRAYSIZE(flShotOffsets); i++ )
			{
				Vector vecTest = vecTarget + (vecRight * flShotOffsets[i]);
				// Add some random height to it
				vecTest.z += RandomFloat( -512, 512 );
				trace_t tr;
				AI_TraceLine( shootOrigin, vecTest, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

				// If we can see the point, it's a clear shot
				if ( tr.fraction == 1.0 && tr.m_pEnt != GetEnemy() )
				{
					pRPG->SetNPCLaserPosition( vecTest );
					return vecTest;
				}
			}
		}
		else
		{
			pRPG->SetNPCLaserPosition( vecTarget );
		}

	}
#endif

	return vecTarget;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon )
{
	if ( pNewWeapon )
	{
		GetShotRegulator()->SetParameters( pNewWeapon->GetMinBurst(), pNewWeapon->GetMaxBurst(), pNewWeapon->GetMinRestTime(), pNewWeapon->GetMaxRestTime() );
	}
	BaseClass::OnChangeActiveWeapon( pOldWeapon, pNewWeapon );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define SHOTGUN_DEFER_SEARCH_TIME	20.0f
#define OTHER_DEFER_SEARCH_TIME		FLT_MAX
bool CNPC_Citizen::ShouldLookForBetterWeapon()
{
	if ( BaseClass::ShouldLookForBetterWeapon() )
	{
		if ( IsInPlayerSquad() && (GetActiveWeapon()&&IsMoving()) && ( m_FollowBehavior.GetFollowTarget() && m_FollowBehavior.GetFollowTarget()->IsPlayer() ) )
		{
			// For citizens in the player squad, you must be unarmed, or standing still (if armed) in order to 
			// divert attention to looking for a new weapon.
			return false;
		}

		if ( GetActiveWeapon() && IsMoving() )
			return false;

		if ( GlobalEntity_GetState("gordon_precriminal") == GLOBAL_ON )
		{
			// This stops the NPC looking altogether.
			m_flNextWeaponSearchTime = FLT_MAX;
			return false;
		}

#ifdef DBGFLAG_ASSERT
		// Cached off to make sure you change this if you ask the code to defer.
		float flOldWeaponSearchTime = m_flNextWeaponSearchTime;
#endif

		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if( pWeapon )
		{
			bool bDefer = false;

#ifdef MAPBASE
			if ( EntIsClass(pWeapon, gm_isz_class_AR2) )
#else
			if( FClassnameIs( pWeapon, "weapon_ar2" ) )
#endif
			{
				// Content to keep this weapon forever
				m_flNextWeaponSearchTime = OTHER_DEFER_SEARCH_TIME;
				bDefer = true;
			}
#ifdef MAPBASE
			else if( EntIsClass(pWeapon, gm_isz_class_RPG) )
#else
			else if( FClassnameIs( pWeapon, "weapon_rpg" ) )
#endif
			{
				// Content to keep this weapon forever
				m_flNextWeaponSearchTime = OTHER_DEFER_SEARCH_TIME;
				bDefer = true;
			}
#ifdef MAPBASE
			else if ( EntIsClass(pWeapon, gm_isz_class_Shotgun) )
#else
			else if( FClassnameIs( pWeapon, "weapon_shotgun" ) )
#endif
			{
				// Shotgunners do not defer their weapon search indefinitely.
				// If more than one citizen in the squad has a shotgun, we force
				// some of them to trade for another weapon.
				if( NumWeaponsInSquad("weapon_shotgun") > 1 )
				{
					// Check for another weapon now. If I don't find one, this code will
					// retry in 2 seconds or so.
					bDefer = false;
				}
				else
				{
					// I'm the only shotgunner in the group right now, so I'll check
					// again in 3 0seconds or so. This code attempts to distribute
					// the desire to reduce shotguns amongst squadmates so that all 
					// shotgunners do not discard their weapons when they suddenly realize
					// the squad has too many.
					if( random->RandomInt( 0, 1 ) == 0 )
					{
						m_flNextWeaponSearchTime = gpGlobals->curtime + SHOTGUN_DEFER_SEARCH_TIME;
					}
					else
					{
						m_flNextWeaponSearchTime = gpGlobals->curtime + SHOTGUN_DEFER_SEARCH_TIME + 10.0f;
					}

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
int CNPC_Citizen::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if( (info.GetDamageType() & DMG_BURN) && (info.GetDamageType() & DMG_DIRECT) )
	{
#ifdef EZ
		// Blixibon - Citizens scorch more, even faster than zombies; makes it more brutal
#define CITIZEN_SCORCH_RATE		10
#define CITIZEN_SCORCH_FLOOR	75
#else
#define CITIZEN_SCORCH_RATE		6
#define CITIZEN_SCORCH_FLOOR	75
#endif

		Scorch( CITIZEN_SCORCH_RATE, CITIZEN_SCORCH_FLOOR );
	}

	CTakeDamageInfo newInfo = info;

	if( IsInSquad() && (info.GetDamageType() & DMG_BLAST) && info.GetInflictor() )
	{
		if( npc_citizen_explosive_resist.GetBool() )
		{
			// Blast damage. If this kills a squad member, give the 
			// remaining citizens a resistance bonus to this inflictor
			// to try to avoid having the entire squad wiped out by a
			// single explosion.
			if( m_pSquad->IsSquadInflictor( info.GetInflictor() ) )
			{
				newInfo.ScaleDamage( 0.5 );
			}
			else
			{
				// If this blast is going to kill me, designate the inflictor
				// so that the rest of the squad can enjoy a damage resist.
				if( info.GetDamage() >= GetHealth() )
				{
					m_pSquad->SetSquadInflictor( info.GetInflictor() );
				}
			}
		}
	}

	return BaseClass::OnTakeDamage_Alive( newInfo );
}

#ifdef EZ
extern ConVar sk_npc_head;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_Citizen::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
	case HITGROUP_HEAD:
		{
			// Multiplied by sk_npc_head
		if (m_Type != CT_BRUTE) // Brutes do normal headshot damage
			return sk_citizen_head.GetFloat() * sk_npc_head.GetFloat();
		} break;

	case HITGROUP_GEAR:
		{
			if (m_Type == CT_LONGFALL)
				return sk_citizen_longfall_gear.GetFloat();
		} break;
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}
#endif

#ifdef MAPBASE
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	// No need to tell me.
	set.AppendCriteria("medic", IsMedic() ? "1" : "0");

	set.AppendCriteria("citizentype", UTIL_VarArgs("%i", m_Type));

#ifdef EZ
	GatherWillpowerConditions();

	int iWillpower = 0;
	if (HasCondition(COND_CIT_WILLPOWER_VERY_LOW))
		iWillpower = -2;
	else if (HasCondition(COND_CIT_WILLPOWER_LOW))
		iWillpower = -1;
	else if (HasCondition(COND_CIT_WILLPOWER_HIGH))
		iWillpower = 1;

	set.AppendCriteria("willpower", UTIL_VarArgs("%i", iWillpower));
#endif
}
#endif

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::ModifyOrAppendCriteriaForPlayer( CBasePlayer *pPlayer, AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteriaForPlayer( pPlayer, set );

	set.AppendCriteria("citizentype", UTIL_VarArgs("%i", m_Type));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Citizen::GetGameTextSpeechParams( hudtextparms_t &params )
{
	if (NameMatches( "*RadioGuy*" ))
	{
		// Radio guy has a specific color
		params.channel = 3;
		params.x = -1;
		params.y = 0.6;
		params.effect = 0;

		params.r1 = 250;
		params.g1 = 245;
		params.b1 = 215;

		return true;
	}

	return false;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsCommandable() 
{
	return ( !HasSpawnFlags(SF_CITIZEN_NOT_COMMANDABLE) && IsInPlayerSquad() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsPlayerAlly( CBasePlayer *pPlayer )											
{ 
	if ( Classify() == CLASS_CITIZEN_PASSIVE && GlobalEntity_GetState("gordon_precriminal") == GLOBAL_ON )
	{
		// Robin: Citizens use friendly speech semaphore in trainstation
		return true;
	}

	return BaseClass::IsPlayerAlly( pPlayer );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::CanJoinPlayerSquad()
{
	if ( !AI_IsSinglePlayer() )
		return false;

	if ( m_NPCState == NPC_STATE_SCRIPT || m_NPCState == NPC_STATE_PRONE )
		return false;

	if ( HasSpawnFlags(SF_CITIZEN_NOT_COMMANDABLE) )
		return false;

	if ( IsInAScript() )
		return false;

	// Don't bother people who don't want to be bothered
	if ( !CanBeUsedAsAFriend() )
		return false;

	if ( IRelationType( UTIL_GetLocalPlayer() ) != D_LI )
		return false;

#ifdef MAPBASE
	if ( IsWaitingToRappel() )
		return false;
#endif

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::WasInPlayerSquad()
{
	return m_bWasInPlayerSquad;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::HaveCommandGoal() const			
{	
	if (GetCommandGoal() != vec3_invalid)
		return true;
	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsCommandMoving()
{
	if ( AI_IsSinglePlayer() && IsInPlayerSquad() )
	{
		if ( m_FollowBehavior.GetFollowTarget() == UTIL_GetLocalPlayer() ||
			 IsFollowingCommandPoint() )
		{
			return ( m_FollowBehavior.IsMovingToFollowTarget() );
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldAutoSummon()
{
	if ( !AI_IsSinglePlayer() || !IsFollowingCommandPoint() || !IsInPlayerSquad() )
		return false;

	CHL2_Player *pPlayer = (CHL2_Player *)UTIL_GetLocalPlayer();
	
	float distMovedSq = ( pPlayer->GetAbsOrigin() - m_vAutoSummonAnchor ).LengthSqr();
	float moveTolerance = player_squad_autosummon_move_tolerance.GetFloat() * 12;
	const Vector &vCommandGoal = GetCommandGoal();

	if ( distMovedSq < Square(moveTolerance * 10) && (GetAbsOrigin() - vCommandGoal).LengthSqr() > Square(10*12) && IsCommandMoving() )
	{
		m_AutoSummonTimer.Set( player_squad_autosummon_time.GetFloat() );
		if ( player_squad_autosummon_debug.GetBool() )
			DevMsg( "Waiting for arrival before initiating autosummon logic\n");
	}
	else if ( m_AutoSummonTimer.Expired() )
	{
		bool bSetFollow = false;
		bool bTestEnemies = true;
		
		// Auto summon unconditionally if a significant amount of time has passed
		if ( gpGlobals->curtime - m_AutoSummonTimer.GetNext() > player_squad_autosummon_time.GetFloat() * 2 )
		{
			bSetFollow = true;
			if ( player_squad_autosummon_debug.GetBool() )
				DevMsg( "Auto summoning squad: long time (%f)\n", ( gpGlobals->curtime - m_AutoSummonTimer.GetNext() ) + player_squad_autosummon_time.GetFloat() );
		}
			
		// Player must move for autosummon
		if ( distMovedSq > Square(12) )
		{
			bool bCommandPointIsVisible = pPlayer->FVisible( vCommandGoal + pPlayer->GetViewOffset() );

			// Auto summon if the player is close by the command point
			if ( !bSetFollow && bCommandPointIsVisible && distMovedSq > Square(24) )
			{
				float closenessTolerance = player_squad_autosummon_player_tolerance.GetFloat() * 12;
				if ( (pPlayer->GetAbsOrigin() - vCommandGoal).LengthSqr() < Square( closenessTolerance ) &&
					 ((m_vAutoSummonAnchor - vCommandGoal).LengthSqr() > Square( closenessTolerance )) )
				{
					bSetFollow = true;
					if ( player_squad_autosummon_debug.GetBool() )
						DevMsg( "Auto summoning squad: player close to command point (%f)\n", (GetAbsOrigin() - vCommandGoal).Length() );
				}
			}
			
			// Auto summon if moved a moderate distance and can't see command point, or moved a great distance
			if ( !bSetFollow )
			{
				if ( distMovedSq > Square( moveTolerance * 2 ) )
				{
					bSetFollow = true;
					bTestEnemies = ( distMovedSq < Square( moveTolerance * 10 ) );
					if ( player_squad_autosummon_debug.GetBool() )
						DevMsg( "Auto summoning squad: player very far from anchor (%f)\n", sqrt(distMovedSq) );
				}
				else if ( distMovedSq > Square( moveTolerance ) )
				{
					if ( !bCommandPointIsVisible )
					{
						bSetFollow = true;
						if ( player_squad_autosummon_debug.GetBool() )
							DevMsg( "Auto summoning squad: player far from anchor (%f)\n", sqrt(distMovedSq) );
					}
				}
			}
		}
		
		// Auto summon only if there are no readily apparent enemies
		if ( bSetFollow && bTestEnemies )
		{
			for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
			{
				CAI_BaseNPC *pNpc = g_AI_Manager.AccessAIs()[i];
				float timeSinceCombatTolerance = player_squad_autosummon_time_after_combat.GetFloat();
				
				if ( pNpc->IsInPlayerSquad() )
				{
					if ( gpGlobals->curtime - pNpc->GetLastAttackTime() > timeSinceCombatTolerance || 
						 gpGlobals->curtime - pNpc->GetLastDamageTime() > timeSinceCombatTolerance )
						continue;
				}
				else if ( pNpc->GetEnemy() )
				{
					CBaseEntity *pNpcEnemy = pNpc->GetEnemy();
					if ( !IsSniper( pNpc ) && ( gpGlobals->curtime - pNpc->GetEnemyLastTimeSeen() ) > timeSinceCombatTolerance )
						continue;

					if ( pNpcEnemy == pPlayer )
					{
						if ( pNpc->CanBeAnEnemyOf( pPlayer ) )
						{
							bSetFollow = false;
							break;
						}
					}
					else if ( pNpcEnemy->IsNPC() && ( pNpcEnemy->MyNPCPointer()->GetSquad() == GetSquad() || pNpcEnemy->Classify() == CLASS_PLAYER_ALLY_VITAL ) )
					{
						if ( pNpc->CanBeAnEnemyOf( this ) )
						{
							bSetFollow = false;
							break;
						}
					}
				}
			}
			if ( !bSetFollow && player_squad_autosummon_debug.GetBool() )
				DevMsg( "Auto summon REVOKED: Combat recent \n");
		}
		
		return bSetFollow;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Is this entity something that the citizen should interact with (return true)
// or something that he should try to get close to (return false)
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsValidCommandTarget( CBaseEntity *pTarget )
{
	return false;
}

//-----------------------------------------------------------------------------
bool CNPC_Citizen::SpeakCommandResponse( AIConcept_t concept, const char *modifiers )
{
	return SpeakIfAllowed( concept, 
						   CFmtStr( "numselected:%d,"
									"useradio:%d%s",
									( GetSquad() ) ? GetSquad()->NumMembers() : 1,
									ShouldSpeakRadio( AI_GetSinglePlayer() ),
									( modifiers ) ? CFmtStr(",%s", modifiers).operator const char *() : "" ) );
}

#ifdef MAPBASE
extern ConVar ai_debug_avoidancebounds;

//-----------------------------------------------------------------------------
// Purpose: Implements player nocollide.
//-----------------------------------------------------------------------------
void CNPC_Citizen::SetPlayerAvoidState( void )
{
	bool bShouldPlayerAvoid = false;
	Vector vNothing;

	GetSequenceLinearMotion( GetSequence(), &vNothing );
	bool bIsMoving = ( IsMoving() || ( vNothing != vec3_origin ) );

	m_bPlayerAvoidState = ShouldPlayerAvoid();
	bool bSquadNoCollide = (IsInPlayerSquad() && npc_citizen_nocollide_player.GetBool());

	// If we are coming out of a script, check if we are stuck inside the player.
	if ( m_bPerformAvoidance || ( m_bPlayerAvoidState && bIsMoving ) || bSquadNoCollide )
	{
		trace_t trace;
		Vector vMins, vMaxs;

		GetPlayerAvoidBounds( &vMins, &vMaxs );

		CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();

		if ( pLocalPlayer )
		{
			bShouldPlayerAvoid = (!bSquadNoCollide || !pLocalPlayer->IsMoving()) && IsBoxIntersectingBox( GetAbsOrigin() + vMins, GetAbsOrigin() + vMaxs, 
				pLocalPlayer->GetAbsOrigin() + pLocalPlayer->WorldAlignMins(), pLocalPlayer->GetAbsOrigin() + pLocalPlayer->WorldAlignMaxs() );
		}

		if ( ai_debug_avoidancebounds.GetBool() )
		{
			int iRed = ( bShouldPlayerAvoid == true ) ? 255 : 0;

			NDebugOverlay::Box( GetAbsOrigin(), vMins, vMaxs, iRed, 0, 255, 64, 0.1 );
		}
	}

	m_bPerformAvoidance = bShouldPlayerAvoid;

	if ( GetCollisionGroup() == COLLISION_GROUP_NPC || GetCollisionGroup() == COLLISION_GROUP_NPC_ACTOR )
	{
		if ( m_bPerformAvoidance == true ||
			(bSquadNoCollide && !m_bPlayerAvoidState))
		{
			SetCollisionGroup( COLLISION_GROUP_NPC_ACTOR );
		}
		else
		{
			SetCollisionGroup( COLLISION_GROUP_NPC );
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: return TRUE if the commander mode should try to give this order
//			to more people. return FALSE otherwise. For instance, we don't
//			try to send all 3 selectedcitizens to pick up the same gun.
//-----------------------------------------------------------------------------
bool CNPC_Citizen::TargetOrder( CBaseEntity *pTarget, CAI_BaseNPC **Allies, int numAllies )
{
	if ( pTarget->IsPlayer() )
	{
		// I'm the target! Toggle follow!
		if( m_FollowBehavior.GetFollowTarget() != pTarget )
		{
			ClearFollowTarget();
			SetCommandGoal( vec3_invalid );

			// Turn follow on!
			m_AssaultBehavior.Disable();
			m_FollowBehavior.SetFollowTarget( pTarget );
			m_FollowBehavior.SetParameters( AIF_SIMPLE );			
			SpeakCommandResponse( TLK_STARTFOLLOW );

			m_OnFollowOrder.FireOutput( this, this );
		}
		else if ( m_FollowBehavior.GetFollowTarget() == pTarget )
		{
			// Stop following.
			m_FollowBehavior.SetFollowTarget( NULL );
			SpeakCommandResponse( TLK_STOPFOLLOW );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Turn off following before processing a move order.
//-----------------------------------------------------------------------------
void CNPC_Citizen::MoveOrder( const Vector &vecDest, CAI_BaseNPC **Allies, int numAllies )
{
	if ( !AI_IsSinglePlayer() )
		return;

#ifdef MAPBASE
	if ( m_iszDenyCommandConcept != NULL_STRING )
#else
	if( hl2_episodic.GetBool() && m_iszDenyCommandConcept != NULL_STRING )
#endif
	{
		SpeakCommandResponse( STRING(m_iszDenyCommandConcept) );
		return;
	}

	CHL2_Player *pPlayer = (CHL2_Player *)UTIL_GetLocalPlayer();

	m_AutoSummonTimer.Set( player_squad_autosummon_time.GetFloat() );
	m_vAutoSummonAnchor = pPlayer->GetAbsOrigin();

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

	bool spoke = false;

	CAI_BaseNPC *pClosest = NULL;
	float closestDistSq = FLT_MAX;

	for( int i = 0 ; i < numAllies ; i++ )
	{
		if( Allies[i]->IsInPlayerSquad() )
		{
			Assert( Allies[i]->IsCommandable() );
			float distSq = ( pPlayer->GetAbsOrigin() - Allies[i]->GetAbsOrigin() ).LengthSqr();
			if( distSq < closestDistSq )
			{
				pClosest = Allies[i];
				closestDistSq = distSq;
			}
		}
	}

	if( m_FollowBehavior.GetFollowTarget() && !IsFollowingCommandPoint() )
	{
		ClearFollowTarget();
#if 0
		if ( ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() < Square( 180 ) &&
			 ( ( vecDest - pPlayer->GetAbsOrigin() ).LengthSqr() < Square( 120 ) || 
			   ( vecDest - GetAbsOrigin() ).LengthSqr() < Square( 120 ) ) )
		{
			if ( pClosest == this )
				SpeakIfAllowed( TLK_STOPFOLLOW );
			spoke = true;
		}
#endif
	}

	if ( !spoke && pClosest == this )
	{
		float destDistToPlayer = ( vecDest - pPlayer->GetAbsOrigin() ).Length();
		float destDistToClosest = ( vecDest - GetAbsOrigin() ).Length();
		CFmtStr modifiers( "commandpoint_dist_to_player:%.0f,"
						   "commandpoint_dist_to_npc:%.0f",
						   destDistToPlayer,
						   destDistToClosest );

		SpeakCommandResponse( TLK_COMMANDED, modifiers );
	}

	m_OnStationOrder.FireOutput( this, this );

	BaseClass::MoveOrder( vecDest, Allies, numAllies );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnMoveOrder()
{
	SetReadinessLevel( AIRL_STIMULATED, false, false );
	BaseClass::OnMoveOrder();
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CNPC_Citizen::ShouldAllowSquadToggleUse( CBasePlayer *pPlayer )
{
	if (HasSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ))
		return false;

	//if (!HL2GameRules() || !HL2GameRules()->AllowSquadToggleUse())
	if (!HasSpawnFlags( SF_CITIZEN_PLAYER_TOGGLE_SQUAD ))
	{
		if (!npc_citizen_squad_secondary_toggle_use_always.GetBool() || m_bNeverLeavePlayerSquad)
			return false;

		// npc_citizen_squad_secondary_toggle_use_always was invoked
		AddSpawnFlags( SF_CITIZEN_PLAYER_TOGGLE_SQUAD );
	}

	return (pPlayer->m_nButtons & npc_citizen_squad_secondary_toggle_use_button.GetInt()) != 0;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::CommanderUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_OnPlayerUse.FireOutput( pActivator, pCaller );

	// Under these conditions, citizens will refuse to go with the player.
	// Robin: NPCs should always respond to +USE even if someone else has the semaphore.
	if ( !AI_IsSinglePlayer() || !CanJoinPlayerSquad() )
	{
		SimpleUse( pActivator, pCaller, useType, value );
		return;
	}
	
	if ( pActivator == UTIL_GetLocalPlayer() )
	{
		// Don't say hi after you've been addressed by the player
		SetSpokeConcept( TLK_HELLO, NULL );	

#ifdef MAPBASE
		if ( ShouldAllowSquadToggleUse(UTIL_GetLocalPlayer()) || npc_citizen_auto_player_squad_allow_use.GetBool() )
		{
			// Version of TogglePlayerSquadState() that has "used" as a modifier
			static const char *szSquadUseModifier = "used:1";
			if ( !IsInPlayerSquad() )
			{
				AddToPlayerSquad();

				if ( HaveCommandGoal() )
				{
					SpeakCommandResponse( TLK_COMMANDED, szSquadUseModifier );
				}
				else if ( m_FollowBehavior.GetFollowTarget() == UTIL_GetLocalPlayer() )
				{
					SpeakCommandResponse( TLK_STARTFOLLOW, szSquadUseModifier );
				}
			}
			else
			{
				SpeakCommandResponse( TLK_STOPFOLLOW, szSquadUseModifier );
				RemoveFromPlayerSquad();
			}
		}
#else
		if ( npc_citizen_auto_player_squad_allow_use.GetBool() )
		{
			if ( !ShouldAutosquad() )
				TogglePlayerSquadState();
			else if ( !IsInPlayerSquad() && npc_citizen_auto_player_squad_allow_use.GetBool() )
				AddToPlayerSquad();
		}
#endif
		else if ( GetCurSchedule() && ConditionInterruptsCurSchedule( COND_IDLE_INTERRUPT ) )
		{
#ifdef MAPBASE
			// Just do regular idle question behavior so question groups, etc. work on +USE.
			if ( IsAllowedToSpeak( TLK_QUESTION, true ) )
			{
				// 1 = Old "SpeakIdleResponse" behavior
				// 2, 3 = AskQuestion() for QA groups, etc.
				// 4 = Just speak
				int iRandom = random->RandomInt(1, 4);
				if ( iRandom == 1 )
				{
					CBaseEntity *pRespondant = FindSpeechTarget( AIST_NPCS );
					if ( pRespondant )
					{
						g_EventQueue.AddEvent( pRespondant, "SpeakIdleResponse", ( GetTimeSpeechComplete() - gpGlobals->curtime ) + .2, this, this );
					}
				}
				if ( iRandom < 4 )
				{
					// Ask someone else
					AskQuestionNow();
				}
				else
				{
					// Just speak
					Speak( TLK_QUESTION );
				}
			}
#else
			if ( SpeakIfAllowed( TLK_QUESTION, NULL, true ) )
			{
				if ( random->RandomInt( 1, 4 ) < 4 )
				{
					CBaseEntity *pRespondant = FindSpeechTarget( AIST_NPCS );
					if ( pRespondant )
					{
						g_EventQueue.AddEvent( pRespondant, "SpeakIdleResponse", ( GetTimeSpeechComplete() - gpGlobals->curtime ) + .2, this, this );
					}
				}
			}
#endif
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldSpeakRadio( CBaseEntity *pListener )
{
	if ( !pListener )
		return false;

	const float		radioRange = 384 * 384;
	Vector			vecDiff;

	vecDiff = WorldSpaceCenter() - pListener->WorldSpaceCenter();

	if( vecDiff.LengthSqr() > radioRange )
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::OnMoveToCommandGoalFailed()
{
	// Clear the goal.
	SetCommandGoal( vec3_invalid );

	// Announce failure.
	SpeakCommandResponse( TLK_COMMAND_FAILED );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::AddToPlayerSquad()
{
	Assert( !IsInPlayerSquad() );

	AddToSquad( AllocPooledString(PLAYER_SQUADNAME) );
	m_hSavedFollowGoalEnt = m_FollowBehavior.GetFollowGoal();
	m_FollowBehavior.SetFollowGoalDirect( NULL );

	FixupPlayerSquad();

	SetCondition( COND_PLAYER_ADDED_TO_SQUAD );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::RemoveFromPlayerSquad()
{
	Assert( IsInPlayerSquad() );

	ClearFollowTarget();
	ClearCommandGoal();
	if ( m_iszOriginalSquad != NULL_STRING && strcmp( STRING( m_iszOriginalSquad ), PLAYER_SQUADNAME ) != 0 )
		AddToSquad( m_iszOriginalSquad );
	else
		RemoveFromSquad();
	
	if ( m_hSavedFollowGoalEnt )
		m_FollowBehavior.SetFollowGoal( m_hSavedFollowGoalEnt );

	SetCondition( COND_PLAYER_REMOVED_FROM_SQUAD );

	// Don't evaluate the player squad for 2 seconds. 
	gm_PlayerSquadEvaluateTimer.Set( 2.0 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::TogglePlayerSquadState()
{
	if ( !AI_IsSinglePlayer() )
		return;

	if ( !IsInPlayerSquad() )
	{
		AddToPlayerSquad();

		if ( HaveCommandGoal() )
		{
			SpeakCommandResponse( TLK_COMMANDED );
		}
		else if ( m_FollowBehavior.GetFollowTarget() == UTIL_GetLocalPlayer() )
		{
			SpeakCommandResponse( TLK_STARTFOLLOW );
		}
	}
	else
	{
		SpeakCommandResponse( TLK_STOPFOLLOW );
		RemoveFromPlayerSquad();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct SquadCandidate_t
{
	CNPC_Citizen *pCitizen;
	bool		  bIsInSquad;
	float		  distSq;
	int			  iSquadIndex;
};

void CNPC_Citizen::UpdatePlayerSquad()
{
	if ( !AI_IsSinglePlayer() )
		return;

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( ( pPlayer->GetAbsOrigin().AsVector2D() - GetAbsOrigin().AsVector2D() ).LengthSqr() < Square(20*12) )
		m_flTimeLastCloseToPlayer = gpGlobals->curtime;

	if ( !gm_PlayerSquadEvaluateTimer.Expired() )
		return;

	gm_PlayerSquadEvaluateTimer.Set( 2.0 );

	// Remove stragglers
	CAI_Squad *pPlayerSquad = g_AI_SquadManager.FindSquad( MAKE_STRING( PLAYER_SQUADNAME ) );
	if ( pPlayerSquad )
	{
		CUtlVectorFixed<CNPC_Citizen *, MAX_PLAYER_SQUAD> squadMembersToRemove;
		AISquadIter_t iter;

		for ( CAI_BaseNPC *pPlayerSquadMember = pPlayerSquad->GetFirstMember(&iter); pPlayerSquadMember; pPlayerSquadMember = pPlayerSquad->GetNextMember(&iter) )
		{
			if ( pPlayerSquadMember->GetClassname() != GetClassname() )
				continue;

			CNPC_Citizen *pCitizen = assert_cast<CNPC_Citizen *>(pPlayerSquadMember);

			if ( !pCitizen->m_bNeverLeavePlayerSquad &&
				 pCitizen->m_FollowBehavior.GetFollowTarget() &&
				 !pCitizen->m_FollowBehavior.FollowTargetVisible() && 
				 pCitizen->m_FollowBehavior.GetNumFailedFollowAttempts() > 0 && 
				 gpGlobals->curtime - pCitizen->m_FollowBehavior.GetTimeFailFollowStarted() > 20 &&
				 ( fabsf(( pCitizen->m_FollowBehavior.GetFollowTarget()->GetAbsOrigin().z - pCitizen->GetAbsOrigin().z )) > 196 ||
				   ( pCitizen->m_FollowBehavior.GetFollowTarget()->GetAbsOrigin().AsVector2D() - pCitizen->GetAbsOrigin().AsVector2D() ).LengthSqr() > Square(50*12) ) )
			{
				if ( DebuggingCommanderMode() )
				{
					DevMsg( "Player follower is lost (%d, %f, %d)\n", 
						 pCitizen->m_FollowBehavior.GetNumFailedFollowAttempts(), 
						 gpGlobals->curtime - pCitizen->m_FollowBehavior.GetTimeFailFollowStarted(), 
						 (int)((pCitizen->m_FollowBehavior.GetFollowTarget()->GetAbsOrigin().AsVector2D() - pCitizen->GetAbsOrigin().AsVector2D() ).Length()) );
				}

				squadMembersToRemove.AddToTail( pCitizen );
			}
		}

		for ( int i = 0; i < squadMembersToRemove.Count(); i++ )
		{
			squadMembersToRemove[i]->RemoveFromPlayerSquad();
		}
	}

	// Autosquadding
	const float JOIN_PLAYER_XY_TOLERANCE_SQ = Square(36*12);
	const float UNCONDITIONAL_JOIN_PLAYER_XY_TOLERANCE_SQ = Square(12*12);
	const float UNCONDITIONAL_JOIN_PLAYER_Z_TOLERANCE = 5*12;
	const float SECOND_TIER_JOIN_DIST_SQ = Square(48*12);
	if ( pPlayer && ShouldAutosquad() && !(pPlayer->GetFlags() & FL_NOTARGET ) && pPlayer->IsAlive() )
	{
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		CUtlVector<SquadCandidate_t> candidates;
		const Vector &vPlayerPos = pPlayer->GetAbsOrigin();
		bool bFoundNewGuy = false;
		int i;

		for ( i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			if ( ppAIs[i]->GetState() == NPC_STATE_DEAD )
				continue;

			if ( ppAIs[i]->GetClassname() != GetClassname() )
				continue;

			CNPC_Citizen *pCitizen = assert_cast<CNPC_Citizen *>(ppAIs[i]);
			int iNew;

			if ( pCitizen->IsInPlayerSquad() )
			{
				iNew = candidates.AddToTail();
				candidates[iNew].pCitizen = pCitizen;
				candidates[iNew].bIsInSquad = true;
				candidates[iNew].distSq = 0;
				candidates[iNew].iSquadIndex = pCitizen->GetSquad()->GetSquadIndex( pCitizen );
			}
			else
			{
				float distSq = (vPlayerPos.AsVector2D() - pCitizen->GetAbsOrigin().AsVector2D()).LengthSqr(); 
				if ( distSq > JOIN_PLAYER_XY_TOLERANCE_SQ && 
					( pCitizen->m_flTimeJoinedPlayerSquad == 0 || gpGlobals->curtime - pCitizen->m_flTimeJoinedPlayerSquad > 60.0 ) && 
					( pCitizen->m_flTimeLastCloseToPlayer == 0 || gpGlobals->curtime - pCitizen->m_flTimeLastCloseToPlayer > 15.0 ) )
					continue;

				if ( !pCitizen->CanJoinPlayerSquad() )
					continue;

#ifdef MAPBASE
				if ( pCitizen->HasSpawnFlags(SF_CITIZEN_PLAYER_TOGGLE_SQUAD) )
					continue;
#endif

				bool bShouldAdd = false;

				if ( pCitizen->HasCondition( COND_SEE_PLAYER ) )
					bShouldAdd = true;
				else
				{
					bool bPlayerVisible = pCitizen->FVisible( pPlayer );
					if ( bPlayerVisible )
					{
						if ( pCitizen->HasCondition( COND_HEAR_PLAYER ) )
							bShouldAdd = true;
						else if ( distSq < UNCONDITIONAL_JOIN_PLAYER_XY_TOLERANCE_SQ && fabsf(vPlayerPos.z - pCitizen->GetAbsOrigin().z) < UNCONDITIONAL_JOIN_PLAYER_Z_TOLERANCE )
							bShouldAdd = true;
					}
				}

				if ( bShouldAdd )
				{
					// @TODO (toml 05-25-04): probably everyone in a squad should be a candidate if one of them sees the player
					AI_Waypoint_t *pPathToPlayer = pCitizen->GetPathfinder()->BuildRoute( pCitizen->GetAbsOrigin(), vPlayerPos, pPlayer, 5*12, NAV_NONE, true );
					GetPathfinder()->UnlockRouteNodes( pPathToPlayer );

					if ( !pPathToPlayer )
						continue;

					CAI_Path tempPath;
					tempPath.SetWaypoints( pPathToPlayer ); // path object will delete waypoints

					iNew = candidates.AddToTail();
					candidates[iNew].pCitizen = pCitizen;
					candidates[iNew].bIsInSquad = false;
					candidates[iNew].distSq = distSq;
					candidates[iNew].iSquadIndex = -1;
					
					bFoundNewGuy = true;
				}
			}
		}
		
		if ( bFoundNewGuy )
		{
			// Look for second order guys
			int initialCount = candidates.Count();
			for ( i = 0; i < initialCount; i++ )
				candidates[i].pCitizen->AddSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ); // Prevents double-add
			for ( i = 0; i < initialCount; i++ )
			{
				if ( candidates[i].iSquadIndex == -1 )
				{
					for ( int j = 0; j < g_AI_Manager.NumAIs(); j++ )
					{
						if ( ppAIs[j]->GetState() == NPC_STATE_DEAD )
							continue;

						if ( ppAIs[j]->GetClassname() != GetClassname() )
							continue;

#ifdef MAPBASE
						if ( ppAIs[j]->HasSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE | SF_CITIZEN_PLAYER_TOGGLE_SQUAD ) )
#else
						if ( ppAIs[j]->HasSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ) )
#endif
							continue; 

						CNPC_Citizen *pCitizen = assert_cast<CNPC_Citizen *>(ppAIs[j]);

						float distSq = (vPlayerPos - pCitizen->GetAbsOrigin()).Length2DSqr(); 
						if ( distSq > JOIN_PLAYER_XY_TOLERANCE_SQ )
							continue;

						distSq = (candidates[i].pCitizen->GetAbsOrigin() - pCitizen->GetAbsOrigin()).Length2DSqr(); 
						if ( distSq > SECOND_TIER_JOIN_DIST_SQ )
							continue;

						if ( !pCitizen->CanJoinPlayerSquad() )
							continue;

						if ( !pCitizen->FVisible( pPlayer ) )
							continue;

						int iNew = candidates.AddToTail();
						candidates[iNew].pCitizen = pCitizen;
						candidates[iNew].bIsInSquad = false;
						candidates[iNew].distSq = distSq;
						candidates[iNew].iSquadIndex = -1;
						pCitizen->AddSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ); // Prevents double-add
					}
				}
			}
			for ( i = 0; i < candidates.Count(); i++ )
				candidates[i].pCitizen->RemoveSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE );

			if ( candidates.Count() > MAX_PLAYER_SQUAD )
			{
				candidates.Sort( PlayerSquadCandidateSortFunc );

				for ( i = MAX_PLAYER_SQUAD; i < candidates.Count(); i++ )
				{
					if ( candidates[i].pCitizen->IsInPlayerSquad() )
					{
						candidates[i].pCitizen->RemoveFromPlayerSquad();
					}
				}
			}

			if ( candidates.Count() )
			{
				CNPC_Citizen *pClosest = NULL;
				float closestDistSq = FLT_MAX;
				int nJoined = 0;

				for ( i = 0; i < candidates.Count() && i < MAX_PLAYER_SQUAD; i++ )
				{
					if ( !candidates[i].pCitizen->IsInPlayerSquad() )
					{
						candidates[i].pCitizen->AddToPlayerSquad();
						nJoined++;

						if ( candidates[i].distSq < closestDistSq )
						{
							pClosest = candidates[i].pCitizen;
							closestDistSq = candidates[i].distSq;
						}
					}
				}

				if ( pClosest )
				{
					if ( !pClosest->SpokeConcept( TLK_JOINPLAYER ) )
					{
						pClosest->SpeakCommandResponse( TLK_JOINPLAYER, CFmtStr( "numjoining:%d", nJoined ) );
					}
					else
					{
						pClosest->SpeakCommandResponse( TLK_STARTFOLLOW );
					}

					for ( i = 0; i < candidates.Count() && i < MAX_PLAYER_SQUAD; i++ )
					{
						candidates[i].pCitizen->SetSpokeConcept( TLK_JOINPLAYER, NULL ); 
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Citizen::PlayerSquadCandidateSortFunc( const SquadCandidate_t *pLeft, const SquadCandidate_t *pRight )
{
	// "Bigger" means less approprate 
	CNPC_Citizen *pLeftCitizen = pLeft->pCitizen;
	CNPC_Citizen *pRightCitizen = pRight->pCitizen;

	// Medics are better than anyone
	if ( pLeftCitizen->IsMedic() && !pRightCitizen->IsMedic() )
		return -1;

	if ( !pLeftCitizen->IsMedic() && pRightCitizen->IsMedic() )
		return 1;

	CBaseCombatWeapon *pLeftWeapon = pLeftCitizen->GetActiveWeapon();
	CBaseCombatWeapon *pRightWeapon = pRightCitizen->GetActiveWeapon();
	
	// People with weapons are better than those without
	if ( pLeftWeapon && !pRightWeapon )
		return -1;
		
	if ( !pLeftWeapon && pRightWeapon )
		return 1;
	
	// Existing squad members are better than non-members
	if ( pLeft->bIsInSquad && !pRight->bIsInSquad )
		return -1;

	if ( !pLeft->bIsInSquad && pRight->bIsInSquad )
		return 1;

	// New squad members are better than older ones
	if ( pLeft->bIsInSquad && pRight->bIsInSquad )
		return pRight->iSquadIndex - pLeft->iSquadIndex;

	// Finally, just take the closer
	return (int)(pRight->distSq - pLeft->distSq);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::FixupPlayerSquad()
{
	if ( !AI_IsSinglePlayer() )
		return;

	m_flTimeJoinedPlayerSquad = gpGlobals->curtime;
	m_bWasInPlayerSquad = true;
	if ( m_pSquad->NumMembers() > MAX_PLAYER_SQUAD )
	{
		CAI_BaseNPC *pFirstMember = m_pSquad->GetFirstMember(NULL);
		m_pSquad->RemoveFromSquad( pFirstMember );
		pFirstMember->ClearCommandGoal();

		CNPC_Citizen *pFirstMemberCitizen = dynamic_cast< CNPC_Citizen * >( pFirstMember );
		if ( pFirstMemberCitizen )
		{
			pFirstMemberCitizen->ClearFollowTarget();
		}
		else
		{
			CAI_FollowBehavior *pOldMemberFollowBehavior;
			if ( pFirstMember->GetBehavior( &pOldMemberFollowBehavior ) )
			{
				pOldMemberFollowBehavior->SetFollowTarget( NULL );
			}
		}
	}

	ClearFollowTarget();

	CAI_BaseNPC *pLeader = NULL;
	AISquadIter_t iter;
	for ( CAI_BaseNPC *pAllyNpc = m_pSquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pSquad->GetNextMember(&iter) )
	{
		if ( pAllyNpc->IsCommandable() )
		{
			pLeader = pAllyNpc;
			break;
		}
	}

	if ( pLeader && pLeader != this )
	{
		const Vector &commandGoal = pLeader->GetCommandGoal();
		if ( commandGoal != vec3_invalid )
		{
			SetCommandGoal( commandGoal );
			SetCondition( COND_RECEIVED_ORDERS ); 
			OnMoveOrder();
		}
		else
		{
			CAI_FollowBehavior *pLeaderFollowBehavior;
			if ( pLeader->GetBehavior( &pLeaderFollowBehavior ) )
			{
				m_FollowBehavior.SetFollowTarget( pLeaderFollowBehavior->GetFollowTarget() );
				m_FollowBehavior.SetParameters( m_FollowBehavior.GetFormation() );
			}

		}
	}
	else
	{
		m_FollowBehavior.SetFollowTarget( UTIL_GetLocalPlayer() );
		m_FollowBehavior.SetParameters( AIF_SIMPLE );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::ClearFollowTarget()
{
	m_FollowBehavior.SetFollowTarget( NULL );
	m_FollowBehavior.SetParameters( AIF_SIMPLE );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::UpdateFollowCommandPoint()
{
	if ( !AI_IsSinglePlayer() )
		return;

	if ( IsInPlayerSquad() )
	{
		if ( HaveCommandGoal() )
		{
			CBaseEntity *pFollowTarget = m_FollowBehavior.GetFollowTarget();
			CBaseEntity *pCommandPoint = gEntList.FindEntityByClassname( NULL, COMMAND_POINT_CLASSNAME );
			
			if( !pCommandPoint )
			{
				DevMsg("**\nVERY BAD THING\nCommand point vanished! Creating a new one\n**\n");
				pCommandPoint = CreateEntityByName( COMMAND_POINT_CLASSNAME );
			}

			if ( pFollowTarget != pCommandPoint )
			{
				pFollowTarget = pCommandPoint;
				m_FollowBehavior.SetFollowTarget( pFollowTarget );
				m_FollowBehavior.SetParameters( AIF_COMMANDER );
			}
			
			if ( ( pCommandPoint->GetAbsOrigin() - GetCommandGoal() ).LengthSqr() > 0.01 )
			{
				UTIL_SetOrigin( pCommandPoint, GetCommandGoal(), false );
			}
		}
		else
		{
			if ( IsFollowingCommandPoint() )
				ClearFollowTarget();
			if ( m_FollowBehavior.GetFollowTarget() != UTIL_GetLocalPlayer() )
			{
				DevMsg( "Expected to be following player, but not\n" );
				m_FollowBehavior.SetFollowTarget( UTIL_GetLocalPlayer() );
				m_FollowBehavior.SetParameters( AIF_SIMPLE );
			}
		}
	}
	else if ( IsFollowingCommandPoint() )
		ClearFollowTarget();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsFollowingCommandPoint()
{
	CBaseEntity *pFollowTarget = m_FollowBehavior.GetFollowTarget();
	if ( pFollowTarget )
		return FClassnameIs( pFollowTarget, COMMAND_POINT_CLASSNAME );
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#ifndef EZ
struct SquadMemberInfo_t
{
	CNPC_Citizen *	pMember;
	bool			bSeesPlayer;
	float			distSq;
};

int __cdecl SquadSortFunc( const SquadMemberInfo_t *pLeft, const SquadMemberInfo_t *pRight )
{
	if ( pLeft->bSeesPlayer && !pRight->bSeesPlayer )
	{
		return -1;
	}

	if ( !pLeft->bSeesPlayer && pRight->bSeesPlayer )
	{
		return 1;
	}

	return ( pLeft->distSq - pRight->distSq );
}
#endif

CAI_BaseNPC *CNPC_Citizen::GetSquadCommandRepresentative()
{
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
					if ( pAllyNpc->IsCommandable() && dynamic_cast<CNPC_Citizen *>(pAllyNpc) )
					{
						int i = candidates.AddToTail();
						candidates[i].pMember = (CNPC_Citizen *)(pAllyNpc);
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
			Assert( dynamic_cast<CNPC_Citizen *>(hCurrent.Get()) && hCurrent->IsInPlayerSquad() );
			return hCurrent;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::SetSquad( CAI_Squad *pSquad )
{
	bool bWasInPlayerSquad = IsInPlayerSquad();

	BaseClass::SetSquad( pSquad );

	if( IsInPlayerSquad() && !bWasInPlayerSquad )
	{
		m_OnJoinedPlayerSquad.FireOutput(this, this);
		if ( npc_citizen_insignia.GetBool() )
			AddInsignia();
	}
	else if ( !IsInPlayerSquad() && bWasInPlayerSquad )
	{
		if ( npc_citizen_insignia.GetBool() )
			RemoveInsignia();
		m_OnLeftPlayerSquad.FireOutput(this, this);
	}
}

#ifdef EZ
// External convars to handle bullsquid bites
extern ConVar sk_gib_carcass_smell;
extern ConVar sk_bullsquid_dmg_bite;
#endif

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_Citizen::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	// TODO:  As citizen gets more complex, we will have to only allow
	//		  these interruptions to happen from certain schedules
	if (interactionType ==	g_interactionScannerInspect)
	{
		if ( gpGlobals->curtime > m_fNextInspectTime )
		{
			//SetLookTarget(sourceEnt);

			// Don't let anyone else pick me for a couple seconds
			SetNextScannerInspectTime( gpGlobals->curtime + 5.0 );
			return true;
		}
		return false;
	}
	else if (interactionType ==	g_interactionScannerInspectBegin)
	{
		// Don't inspect me again for a while
		SetNextScannerInspectTime( gpGlobals->curtime + CIT_INSPECTED_DELAY_TIME );
		
		Vector	targetDir = ( sourceEnt->WorldSpaceCenter() - WorldSpaceCenter() );
		VectorNormalize( targetDir );

		// If we're hit from behind, startle
		if ( DotProduct( targetDir, BodyDirection3D() ) < 0 )
		{
			m_nInspectActivity = ACT_CIT_STARTLED;
		}
		else
		{
			// Otherwise we're blinded by the flash
			m_nInspectActivity = ACT_CIT_BLINDED;
		}
		
		SetCondition( COND_CIT_START_INSPECTION );
		return true;
	}
	else if (interactionType ==	g_interactionScannerInspectHandsUp)
	{
		m_nInspectActivity = ACT_CIT_HANDSUP;
		SetSchedule(SCHED_CITIZEN_PLAY_INSPECT_ACTIVITY);
		return true;
	}
	else if (interactionType ==	g_interactionScannerInspectShowArmband)
	{
		m_nInspectActivity = ACT_CIT_SHOWARMBAND;
		SetSchedule(SCHED_CITIZEN_PLAY_INSPECT_ACTIVITY);
		return true;
	}
	else if (interactionType ==	g_interactionScannerInspectDone)
	{
		SetSchedule(SCHED_IDLE_WANDER);
		return true;
	}
	else if (interactionType == g_interactionHitByPlayerThrownPhysObj )
	{
		if ( IsOkToSpeakInResponseToPlayer() )
		{
			Speak( TLK_PLYR_PHYSATK );
		}
		return true;
	}
#ifdef EZ
	else if ( interactionType == g_interactionBullsquidMonch && npc_citizen_gib.GetBool() )
	{
		OnTakeDamage ( CTakeDamageInfo( sourceEnt, sourceEnt, sk_bullsquid_dmg_bite.GetFloat() * 1.5f, DMG_CRUSH | DMG_ALWAYSGIB ) );
		// Give the predator health if gibs do not count as food sources, otherwise don't
		return !sk_gib_carcass_smell.GetBool();
	}
#ifdef EZ2
	else if ( interactionType == g_interactionBadCopKick )
	{
		KickInfo_t * pInfo = static_cast< KickInfo_t *>(data);

		// Only continue if our damage filter allows us to
		if (pInfo->dmgInfo && !PassesDamageFilter( *pInfo->dmgInfo ))
			return false;

		// If we have directional information, see if this kick knocked a weapon free
		trace_t * pTr = pInfo->tr;
		CBaseCombatWeapon * pWeapon = GetActiveWeapon();
		if ( pTr && pWeapon )
		{
			Vector pWeaponPos = Weapon_ShootPosition();
			const Vector weaponTarget = pWeaponPos + (pTr->endpos - pTr->startpos);
			const Vector weaponVelocity = ( (pTr->endpos - pTr->startpos) * 1024.0f );

			SetCondition( COND_CIT_DISARMED );

			Weapon_Drop( pWeapon, &weaponTarget, &weaponVelocity );
		}

		// Oof
		SetCondition( COND_HEAVY_DAMAGE );

		if (GetActiveWeapon() == NULL)
		{
			// If I don't have any weapons to switch to, lose willpower and stop attacking
			if ( pWeapon == NULL || !GiveBackupWeapon( pWeapon, sourceEnt ) )
			{
				ClearCondition( COND_CIT_WILLPOWER_HIGH );
				if(!m_bWillpowerDisabled)
					SetCondition( COND_CIT_WILLPOWER_LOW );
				SetCondition( COND_TOO_CLOSE_TO_ATTACK );
			}
		}

		// Do normal kick handling
		return false;
	}
	// Set the context for surrender
	else if ( interactionType == g_interactionBadCopOrderSurrender && m_SurrenderBehavior.CanSurrender() && GetActiveWeapon() == NULL )
	{
		//AddContext( "surrendered:1" );
		m_SurrenderBehavior.Surrender( sourceEnt );
		//m_OnSurrender.FireOutput( sourceEnt, sourceEnt );

		if (sourceEnt)
		{
			AddLookTarget( sourceEnt, 1.0f, 3.0f, 1.0f );
		}

		return true;
	}
#endif
#endif

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::FValidateHintType( CAI_Hint *pHint )
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

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::CanHeal()
{ 
	if ( !IsMedic() && !IsAmmoResupplier() )
		return false;

	if( !hl2_episodic.GetBool() )
	{
		// If I'm not armed, my priority should be to arm myself.
		if ( IsMedic() && !GetActiveWeapon() )
			return false;
	}

	if ( IsInAScript() || (m_NPCState == NPC_STATE_SCRIPT) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldHealTarget( CBaseEntity *pTarget, bool bActiveUse )
{
	Disposition_t disposition;
	
	if (!pTarget)
		return false;

#ifdef MAPBASE
	if ( pTarget && ( ( disposition = IRelationType( pTarget ) ) != D_LI && disposition != D_NU ) )
#else
	if ( !pTarget && ( ( disposition = IRelationType( pTarget ) ) != D_LI && disposition != D_NU ) )
#endif
		return false;

	// Don't heal if I'm in the middle of talking
	if ( IsSpeaking() )
		return false;

	bool bTargetIsPlayer = pTarget->IsPlayer();

	// Don't heal or give ammo to targets in vehicles
	CBaseCombatCharacter *pCCTarget = pTarget->MyCombatCharacterPointer();
	if ( pCCTarget != NULL && pCCTarget->IsInAVehicle() )
		return false;

	if ( IsMedic() )
	{
		Vector toPlayer = ( pTarget->GetAbsOrigin() - GetAbsOrigin() );
	 	if (( bActiveUse || !HaveCommandGoal() || toPlayer.Length() < HEAL_TARGET_RANGE) 
#ifdef HL2_EPISODIC
			&& fabs(toPlayer.z) < HEAL_TARGET_RANGE_Z
#endif
			)
	 	{
			if ( pTarget->m_iHealth > 0 )
			{
	 			if ( bActiveUse )
				{
					// Ignore heal requests if we're going to heal a tiny amount
					float timeFullHeal = m_flPlayerHealTime;
					float timeRecharge = sk_citizen_heal_player_delay.GetFloat();
					float maximumHealAmount = sk_citizen_heal_player.GetFloat();
					float healAmt = ( maximumHealAmount * ( 1.0 - ( timeFullHeal - gpGlobals->curtime ) / timeRecharge ) );
					if ( healAmt > pTarget->m_iMaxHealth - pTarget->m_iHealth )
						healAmt = pTarget->m_iMaxHealth - pTarget->m_iHealth;
					if ( healAmt < sk_citizen_heal_player_min_forced.GetFloat() )
						return false;

	 				return ( pTarget->m_iMaxHealth > pTarget->m_iHealth );
				}
	 				
				// Are we ready to heal again?
				bool bReadyToHeal = ( ( bTargetIsPlayer && m_flPlayerHealTime <= gpGlobals->curtime ) || 
									  ( !bTargetIsPlayer && m_flAllyHealTime <= gpGlobals->curtime ) );

				// Only heal if we're ready
				if ( bReadyToHeal )
				{
					int requiredHealth;

					if ( bTargetIsPlayer )
						requiredHealth = pTarget->GetMaxHealth() - sk_citizen_heal_player.GetFloat();
					else
						requiredHealth = pTarget->GetMaxHealth() * sk_citizen_heal_player_min_pct.GetFloat();

					if ( ( pTarget->m_iHealth <= requiredHealth ) && IRelationType( pTarget ) == D_LI )
						return true;
				}
			}
		}
	}

	// Only players need ammo
	if ( IsAmmoResupplier() && bTargetIsPlayer )
	{
		if ( m_flPlayerGiveAmmoTime <= gpGlobals->curtime )
		{
			int iAmmoType = GetAmmoDef()->Index( STRING(m_iszAmmoSupply) );
			if ( iAmmoType == -1 )
			{
				DevMsg("ERROR: Citizen attempting to give unknown ammo type (%s)\n", STRING(m_iszAmmoSupply) );
			}
			else
			{

#ifdef EZ2
				// No ammo remaining
				if (m_iAmmoAmount <= 0)
					return false;
#endif
				// Does the player need the ammo we can give him?
				int iMax = GetAmmoDef()->MaxCarry(iAmmoType);
				int iCount = ((CBasePlayer*)pTarget)->GetAmmoCount(iAmmoType);
				if ( !iCount || ((iMax - iCount) >= m_iAmmoAmount) )
				{
					// Only give the player ammo if he has a weapon that uses it
					if ( ((CBasePlayer*)pTarget)->Weapon_GetWpnForAmmo( iAmmoType ) )
						return true;
				}
#ifdef MAPBASE
				else if ( (iMax - iCount) < m_iAmmoAmount && (iMax - iCount) != 0 )
				{
					// If we're allowed to adjust our ammo, the amount of ammo we give may be reduced, but that's better than not giving any at all!
					if (npc_citizen_resupplier_adjust_ammo.GetBool() == true && ((CBasePlayer*)pTarget)->Weapon_GetWpnForAmmo( iAmmoType ))
						return true;
				}
#endif
			}
		}
	}
	return false;
}

#ifdef HL2_EPISODIC
//-----------------------------------------------------------------------------
// Determine if the citizen is in a position to be throwing medkits
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldHealTossTarget( CBaseEntity *pTarget, bool bActiveUse )
{
	Disposition_t disposition;

	Assert( IsMedic() );
	if ( !IsMedic() )
		return false;
	
#ifdef MAPBASE
	if ( pTarget && ( ( disposition = IRelationType( pTarget ) ) != D_LI && disposition != D_NU ) )
#else
	if ( !pTarget && ( ( disposition = IRelationType( pTarget ) ) != D_LI && disposition != D_NU ) )
#endif
		return false;

	// Don't heal if I'm in the middle of talking
	if ( IsSpeaking() )
		return false;

#ifdef MAPBASE
	// NPCs cannot be healed by throwing medkits at them.
	// I don't think NPCs even pass through this function anyway, it's just the actual heal event that's the problem.
	if (!pTarget->IsPlayer())
		return false;
#else
	bool bTargetIsPlayer = pTarget->IsPlayer();
#endif

	// Don't heal or give ammo to targets in vehicles
	CBaseCombatCharacter *pCCTarget = pTarget->MyCombatCharacterPointer();
	if ( pCCTarget != NULL && pCCTarget->IsInAVehicle() )
		return false;

	Vector toPlayer = ( pTarget->GetAbsOrigin() - GetAbsOrigin() );
	if ( bActiveUse || !HaveCommandGoal() || toPlayer.Length() < HEAL_TOSS_TARGET_RANGE )
	{
		if ( pTarget->m_iHealth > 0 )
		{
			if ( bActiveUse )
			{
				// Ignore heal requests if we're going to heal a tiny amount
				float timeFullHeal = m_flPlayerHealTime;
				float timeRecharge = sk_citizen_heal_player_delay.GetFloat();
				float maximumHealAmount = sk_citizen_heal_player.GetFloat();
				float healAmt = ( maximumHealAmount * ( 1.0 - ( timeFullHeal - gpGlobals->curtime ) / timeRecharge ) );
				if ( healAmt > pTarget->m_iMaxHealth - pTarget->m_iHealth )
					healAmt = pTarget->m_iMaxHealth - pTarget->m_iHealth;
				if ( healAmt < sk_citizen_heal_player_min_forced.GetFloat() )
					return false;

				return ( pTarget->m_iMaxHealth > pTarget->m_iHealth );
			}

			// Are we ready to heal again?
#ifdef MAPBASE
			bool bReadyToHeal = m_flPlayerHealTime <= gpGlobals->curtime;
#else
			bool bReadyToHeal = ( ( bTargetIsPlayer && m_flPlayerHealTime <= gpGlobals->curtime ) || 
				( !bTargetIsPlayer && m_flAllyHealTime <= gpGlobals->curtime ) );
#endif

			// Only heal if we're ready
			if ( bReadyToHeal )
			{
				int requiredHealth;

#ifdef MAPBASE
				requiredHealth = pTarget->GetMaxHealth() - sk_citizen_heal_player.GetFloat();
#else
				if ( bTargetIsPlayer )
					requiredHealth = pTarget->GetMaxHealth() - sk_citizen_heal_player.GetFloat();
				else
					requiredHealth = pTarget->GetMaxHealth() * sk_citizen_heal_player_min_pct.GetFloat();
#endif

				if ( ( pTarget->m_iHealth <= requiredHealth ) && IRelationType( pTarget ) == D_LI )
					return true;
			}
		}
	}
	
	return false;
}
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::Heal()
{
	if ( !CanHeal() )
		  return;

	CBaseEntity *pTarget = GetTarget();

#ifdef MAPBASE
	if ( !pTarget )
		return;
#endif

	Vector target = pTarget->GetAbsOrigin() - GetAbsOrigin();
	if ( target.Length() > HEAL_TARGET_RANGE * 2 )
		return;

	// Don't heal a player that's staring at you until a few seconds have passed.
	m_flTimeNextHealStare = gpGlobals->curtime + sk_citizen_stare_heal_time.GetFloat();

	if ( IsMedic() )
	{
		float timeFullHeal;
		float timeRecharge;
		float maximumHealAmount;
		if ( pTarget->IsPlayer() )
		{
			timeFullHeal 		= m_flPlayerHealTime;
			timeRecharge 		= sk_citizen_heal_player_delay.GetFloat();
			maximumHealAmount 	= sk_citizen_heal_player.GetFloat();
			m_flPlayerHealTime 	= gpGlobals->curtime + timeRecharge;
		}
		else
		{
			timeFullHeal 		= m_flAllyHealTime;
			timeRecharge 		= sk_citizen_heal_ally_delay.GetFloat();
			maximumHealAmount 	= sk_citizen_heal_ally.GetFloat();
			m_flAllyHealTime 	= gpGlobals->curtime + timeRecharge;
		}
		
		float healAmt = ( maximumHealAmount * ( 1.0 - ( timeFullHeal - gpGlobals->curtime ) / timeRecharge ) );
		
		if ( healAmt > maximumHealAmount )
			healAmt = maximumHealAmount;
		else
			healAmt = RoundFloatToInt( healAmt );
		
		if ( healAmt > 0 )
		{
			if ( pTarget->IsPlayer() && npc_citizen_medic_emit_sound.GetBool() )
			{
				CPASAttenuationFilter filter( pTarget, "HealthKit.Touch" );
				EmitSound( filter, pTarget->entindex(), "HealthKit.Touch" );
			}

#ifdef MAPBASE
			pTarget->IsPlayer() ? m_OnHealedPlayer.FireOutput(pTarget, this) : m_OnHealedNPC.FireOutput(pTarget, this);
#endif

			pTarget->TakeHealth( healAmt, DMG_GENERIC );
			pTarget->RemoveAllDecals();

#ifdef EZ2
			if (pTarget->IsPlayer())
			{
				// Fire the event (used for surrender behavior)
				IGameEvent *event = gameeventmanager->CreateEvent( "medic_heal_player" );
				if (event)
				{
					event->SetInt( "userid", ToBasePlayer( pTarget )->GetUserID() );
					event->SetInt( "medic", entindex() );
					gameeventmanager->FireEvent( event );
				}
			}
#endif
		}
	}

	if ( IsAmmoResupplier() )
	{
		// Non-players don't use ammo
		if ( pTarget->IsPlayer() )
		{
			int iAmmoType = GetAmmoDef()->Index( STRING(m_iszAmmoSupply) );
			if ( iAmmoType == -1 )
			{
				DevMsg("ERROR: Citizen attempting to give unknown ammo type (%s)\n", STRING(m_iszAmmoSupply) );
			}
			else
			{
				((CBasePlayer*)pTarget)->GiveAmmo( m_iAmmoAmount, iAmmoType, false );

#ifdef MAPBASE
				m_OnGiveAmmo.FireOutput(pTarget, this);
#endif

#ifdef EZ2
				if (IsSurrendered())
				{
					// Surrendered citizens slowly deplete their ammo supply.
					m_iAmmoAmount = (((float)m_iAmmoAmount) * npc_citizen_surrender_ammo_deplete_multiplier.GetFloat());
				}
#endif
			}

			m_flPlayerGiveAmmoTime = gpGlobals->curtime + sk_citizen_giveammo_player_delay.GetFloat();
		}
	}
}



#if HL2_EPISODIC
//-----------------------------------------------------------------------------
// Like Heal(), but tosses a healthkit in front of the player rather than just juicing him up.
//-----------------------------------------------------------------------------
void	CNPC_Citizen::TossHealthKit(CBaseCombatCharacter *pThrowAt, const Vector &offset)
{
	Assert( pThrowAt );

	Vector forward, right, up;
	GetVectors( &forward, &right, &up );
	Vector medKitOriginPoint = WorldSpaceCenter() + ( forward * 20.0f );
	Vector destinationPoint;
	// this doesn't work without a moveparent: pThrowAt->ComputeAbsPosition( offset, &destinationPoint );
	VectorTransform( offset, pThrowAt->EntityToWorldTransform(), destinationPoint );
	// flatten out any z change due to player looking up/down
	destinationPoint.z = pThrowAt->EyePosition().z;

	Vector tossVelocity;

	if (npc_citizen_medic_throw_style.GetInt() == 0)
	{
		CTraceFilterSkipTwoEntities tracefilter( this, pThrowAt, COLLISION_GROUP_NONE );
		tossVelocity = VecCheckToss( this, &tracefilter, medKitOriginPoint, destinationPoint, 0.233f, 1.0f, false );
	}
	else
	{
		tossVelocity = VecCheckThrow( this, medKitOriginPoint, destinationPoint, MEDIC_THROW_SPEED, 1.0f );

		if (vec3_origin == tossVelocity)
		{
			// if out of range, just throw it as close as I can
			tossVelocity = destinationPoint - medKitOriginPoint;

			// rotate upwards against gravity
			float len = VectorLength(tossVelocity);
			tossVelocity *= (MEDIC_THROW_SPEED / len);
			tossVelocity.z += 0.57735026918962576450914878050196 * MEDIC_THROW_SPEED;
		}
	}

	// create a healthkit and toss it into the world
	CBaseEntity *pHealthKit = CreateEntityByName( "item_healthkit" );
	Assert(pHealthKit);
	if (pHealthKit)
	{
		pHealthKit->SetAbsOrigin( medKitOriginPoint );
		pHealthKit->SetOwnerEntity( this );
		// pHealthKit->SetAbsVelocity( tossVelocity );
		DispatchSpawn( pHealthKit );

		{
			IPhysicsObject *pPhysicsObject = pHealthKit->VPhysicsGetObject();
			Assert( pPhysicsObject );
			if ( pPhysicsObject )
			{
				unsigned int cointoss = random->RandomInt(0,0xFF); // int bits used for bools

				// some random precession
				Vector angDummy(random->RandomFloat(-200,200), random->RandomFloat(-200,200), 
					cointoss & 0x01 ? random->RandomFloat(200,600) : -1.0f * random->RandomFloat(200,600));
				pPhysicsObject->SetVelocity( &tossVelocity, &angDummy );
			}
		}

#ifdef MAPBASE
		m_OnThrowMedkit.Set(pHealthKit, pHealthKit, this);
#endif
	}
	else
	{
		Warning("Citizen tried to heal but could not spawn item_healthkit!\n");
	}

}

//-----------------------------------------------------------------------------
// cause an immediate call to TossHealthKit with some default numbers
//-----------------------------------------------------------------------------
void	CNPC_Citizen::InputForceHealthKitToss( inputdata_t &inputdata )
{
	TossHealthKit( UTIL_GetLocalPlayer(), Vector(48.0f, 0.0f, 0.0f)  );
}

#endif



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Citizen::ShouldLookForHealthItem()
{
	// Definitely do not take health if not in the player's squad.
	if( !IsInPlayerSquad() )
		return false;

	if( gpGlobals->curtime < m_flNextHealthSearchTime )
		return false;

	// I'm fully healthy.
	if( GetHealth() >= GetMaxHealth() )
		return false;

	// Player is hurt, don't steal his health.
	if( AI_IsSinglePlayer() && UTIL_GetLocalPlayer()->GetHealth() <= UTIL_GetLocalPlayer()->GetHealth() * 0.75f )
		return false;

	// Wait till you're standing still.
	if( IsMoving() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputStartPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputStopPatrolling( inputdata_t &inputdata )
{
	m_bShouldPatrol = false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::OnGivenWeapon( CBaseCombatWeapon *pNewWeapon )
{
	FixupMattWeapon();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::InputSetCommandable( inputdata_t &inputdata )
{
	RemoveSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE );
	gm_PlayerSquadEvaluateTimer.Force();
}

#ifdef MAPBASE
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::InputSetUnCommandable( inputdata_t &inputdata )
{
	AddSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE );
	RemoveFromPlayerSquad();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputSetMedicOn( inputdata_t &inputdata )
{
	AddSpawnFlags( SF_CITIZEN_MEDIC );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputSetMedicOff( inputdata_t &inputdata )
{
	RemoveSpawnFlags( SF_CITIZEN_MEDIC );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputSetAmmoResupplierOn( inputdata_t &inputdata )
{
	AddSpawnFlags( SF_CITIZEN_AMMORESUPPLIER );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputSetAmmoResupplierOff( inputdata_t &inputdata )
{
	RemoveSpawnFlags( SF_CITIZEN_AMMORESUPPLIER );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::InputSpeakIdleResponse( inputdata_t &inputdata )
{
	SpeakIfAllowed( TLK_ANSWER, NULL, true );
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::InputSetPoliceGoal( inputdata_t &inputdata )
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

#ifdef EZ2
void CNPC_Citizen::InputSurrender( inputdata_t & inputdata )
{
	//AddContext( "surrendered:1" );
	m_SurrenderBehavior.Surrender( ToBaseCombatCharacter( inputdata.pActivator ) );
	//m_OnSurrender.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CNPC_Citizen::InputSetSurrenderFlags( inputdata_t & inputdata )
{
	m_SurrenderBehavior.SetFlags( inputdata.value.Int() );
}

void CNPC_Citizen::InputAddSurrenderFlags( inputdata_t & inputdata )
{
	m_SurrenderBehavior.AddFlags( inputdata.value.Int() );
}

void CNPC_Citizen::InputRemoveSurrenderFlags( inputdata_t & inputdata )
{
	m_SurrenderBehavior.RemoveFlags( inputdata.value.Int() );
}

void CNPC_Citizen::InputSetWillpowerModifier( inputdata_t & inputdata )
{
	m_iWillpowerModifier = inputdata.value.Int();
}

void CNPC_Citizen::InputSetWillpowerDisabled( inputdata_t & inputdata )
{
	m_bWillpowerDisabled = inputdata.value.Bool();
}

void CNPC_Citizen::InputSetSuppressiveFireDisabled( inputdata_t & inputdata )
{
	m_bSuppressiveFireDisabled = inputdata.value.Bool();
}

void CNPC_Citizen::InputForcePanic( inputdata_t & inputdata )
{
	m_iWillpowerModifier = -99999999;
	m_bWillpowerDisabled = false;

	m_iNumGrenades = 0;

	// Immediately drop weapon
	Weapon_Drop( GetActiveWeapon() );

	// Don't look for another weapon for 15 seconds!
	m_flNextWeaponSearchTime = gpGlobals->curtime + 15.0;

	// Never use a backup weapon after this!
	m_bUsedBackupWeapon = true;

	SetCondition( COND_CIT_WILLPOWER_VERY_LOW );
	SetCondition( COND_CIT_WILLPOWER_LOW );
	ClearCondition( COND_CIT_WILLPOWER_HIGH );
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Citizen::DeathSound( const CTakeDamageInfo &info )
{
	// Sentences don't play on dead NPCs
	SentenceStop();

#ifdef MAPBASE
	AI_CriteriaSet set;
	ModifyOrAppendDamageCriteria(set, info);
	Speak( TLK_DEATH, set );
#else
	EmitSound( "NPC_Citizen.Die" );
#endif
#ifdef EZ
	// Blixibon - Don't drop blood decal under these damage types
	if (!(info.GetDamageType() & (DMG_DISSOLVE | DMG_DROWN | DMG_RADIATION | DMG_POISON | DMG_NERVEGAS)))
	{
		// BREADMAN - hijacking this
		// dump a decal beneath us.
		trace_t tr;
		AI_TraceLine(GetAbsOrigin() + Vector(0, 0, 1), GetAbsOrigin() - Vector(0, 0, 64), MASK_SOLID_BRUSHONLY | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP, this, COLLISION_GROUP_NONE, &tr);
		UTIL_DecalTrace(&tr, "Blood"); // my bits - trying to dump a big splat on the floor when we die.
	}
#endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_Citizen::FearSound( void )
{
#ifdef EZ1
	SpeakIfAllowed(TLK_DANGER); // Say something when afraid
#endif
#ifdef EZ2
	if ( TrySpeakBeg() == false )
	{
		SpeakIfAllowed( TLK_FEAR ); // Say something when afraid
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Citizen::UseSemaphore( void )
{
	// Ignore semaphore if we're told to work outside it
	if ( HasSpawnFlags(SF_CITIZEN_IGNORE_SEMAPHORE) )
		return false;

	return BaseClass::UseSemaphore();
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::CCitizenSurrenderBehavior::Surrender( CBaseCombatCharacter *pCaptor )
{
	BaseClass::Surrender( pCaptor );

	// "You surrendered even though you had a weapon the whole time???"
	// TODO: Maybe keep this and turn backup weapons into a resistance modifier? (i.e. citizens have a weapon concealed and are just waiting for the right time to use it)
	GetOuterCit()->m_bUsedBackupWeapon = true;

	// Brutes become very, very annoying ammo resuppliers
	// NEW: Now applies to all non-medic citizens
	if (/*GetOuterCit()->m_Type == CT_BRUTE &&*/ !GetOuterCit()->IsMedic() && !GetOuterCit()->IsAmmoResupplier() && GetOuterCit()->m_iszAmmoSupply != NULL_STRING)
	{
		GetOuterCit()->AddSpawnFlags( SF_CITIZEN_AMMORESUPPLIER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Citizen::CCitizenSurrenderBehavior::SelectSchedule()
{
	int schedule = SCHED_NONE;
	if (IsSurrenderIdleStanding() || IsSurrenderMovingToIdle())
	{
		//if (HasCondition( COND_CIT_PLAYERHEALREQUEST ))
		{
			// Heal if needed
			schedule = GetOuterCit()->SelectScheduleHeal();
			if (schedule != SCHED_NONE)
				return schedule;
		}

		if (GetOuterCit()->m_Type == CT_BRUTE)
		{
			// Look for a weapon immediately
			schedule = GetOuterCit()->SelectScheduleRetrieveItem();
			if (schedule != SCHED_NONE)
				return schedule;
		}

		schedule = GetOuterCit()->SelectSchedulePlayerPush();
		if (schedule != SCHED_NONE)
			return schedule;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::CCitizenSurrenderBehavior::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	if (IsSurrenderIdleStanding())
	{
		GetOuter()->SetCustomInterruptCondition( COND_CIT_PLAYERHEALREQUEST );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_Citizen::CCitizenSurrenderBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_SURRENDER_IDLE_LOOP:
		if (NextSurrenderIdleCheck() < gpGlobals->curtime)
		{
			if (IsSurrenderIdleStanding())
			{
				// Check if there's anyone in our squad nearby who should be healed
				// (bit of a hack, but there's no other way to interrupt the schedule)
				if ( GetOuter()->IsInSquad() )
				{
					CBaseEntity *pEntity = NULL;
					float distClosestSq = Square( 128.0f );
					float distCurSq;
			
					AISquadIter_t iter;
					CAI_BaseNPC *pSquadmate = GetOuter()->GetSquad()->GetFirstMember( &iter );
					while ( pSquadmate )
					{
						if ( pSquadmate != GetOuter() )
						{
							distCurSq = ( GetAbsOrigin() - pSquadmate->GetAbsOrigin() ).LengthSqr();
							if ( distCurSq < distClosestSq && GetOuterCit()->ShouldHealTarget( pSquadmate ) )
							{
								distClosestSq = distCurSq;
								pEntity = pSquadmate;
							}
						}

						pSquadmate = GetOuter()->GetSquad()->GetNextMember( &iter );
					}
			
					if ( pEntity )
					{
						// Assume the heal schedule will be selected
						TaskComplete();
					}
				}
			}

			BaseClass::RunTask( pTask );
		}
		break;

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Citizen::CCitizenSurrenderBehavior::ModifyResistanceValue( int iVal )
{
	iVal = BaseClass::ModifyResistanceValue( iVal );

	iVal += GetOuterCit()->m_iWillpowerModifier;

	// Brutes give resistance a natural bonus
	if (GetOuterCit()->m_Type == CT_BRUTE)
		iVal += 3;

	return iVal;
}
#endif

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_citizen, CNPC_Citizen )

	DECLARE_TASK( TASK_CIT_HEAL )
	DECLARE_TASK( TASK_CIT_RPG_AUGER )
	DECLARE_TASK( TASK_CIT_PLAY_INSPECT_SEQUENCE )
	DECLARE_TASK( TASK_CIT_SIT_ON_TRAIN )
	DECLARE_TASK( TASK_CIT_LEAVE_TRAIN )
	DECLARE_TASK( TASK_CIT_SPEAK_MOURNING )
#if HL2_EPISODIC
	DECLARE_TASK( TASK_CIT_HEAL_TOSS )
#endif
#ifdef EZ
	DECLARE_TASK( TASK_CIT_DIE_INSTANTLY )
	DECLARE_TASK( TASK_CIT_PAINT_SUPPRESSION_TARGET )
#endif

	DECLARE_ACTIVITY( ACT_CIT_HANDSUP )
	DECLARE_ACTIVITY( ACT_CIT_BLINDED )
	DECLARE_ACTIVITY( ACT_CIT_SHOWARMBAND )
	DECLARE_ACTIVITY( ACT_CIT_HEAL )
	DECLARE_ACTIVITY( ACT_CIT_STARTLED )

#ifdef EZ2
	DECLARE_ACTIVITY( ACT_CROUCH_PANICKED )
	DECLARE_ACTIVITY( ACT_WALK_PANICKED )
	DECLARE_ACTIVITY( ACT_RUN_PANICKED )
#endif


	DECLARE_CONDITION( COND_CIT_PLAYERHEALREQUEST )
	DECLARE_CONDITION( COND_CIT_COMMANDHEAL )
	DECLARE_CONDITION( COND_CIT_START_INSPECTION )
#ifdef EZ
	DECLARE_CONDITION(COND_CIT_WILLPOWER_VERY_LOW)
	DECLARE_CONDITION(COND_CIT_WILLPOWER_LOW)
	DECLARE_CONDITION(COND_CIT_WILLPOWER_HIGH)
	DECLARE_CONDITION(COND_CIT_ON_FIRE)
	DECLARE_CONDITION(COND_CIT_DISARMED)

	DECLARE_SQUADSLOT(SQUAD_SLOT_CITIZEN_RPG1) // Previously, RPG squad slots were undeclared
	DECLARE_SQUADSLOT(SQUAD_SLOT_CITIZEN_RPG2)
	DECLARE_SQUADSLOT(SQUAD_SLOT_CITIZEN_ADVANCE)
	DECLARE_SQUADSLOT(SQUAD_SLOT_CITIZEN_INVESTIGATE)
#endif

	//Events
	DECLARE_ANIMEVENT( AE_CITIZEN_GET_PACKAGE )
	DECLARE_ANIMEVENT( AE_CITIZEN_HEAL )

	//=========================================================
	// > SCHED_SCI_HEAL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_HEAL,

		"	Tasks"
		"		TASK_GET_PATH_TO_TARGET				0"
		"		TASK_MOVE_TO_TARGET_RANGE			50"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_IDEAL						0"
//		"		TASK_SAY_HEAL						0"
//		"		TASK_PLAY_SEQUENCE_FACE_TARGET		ACTIVITY:ACT_ARM"
		"		TASK_CIT_HEAL							0"
//		"		TASK_PLAY_SEQUENCE_FACE_TARGET		ACTIVITY:ACT_DISARM"
		"	"
		"	Interrupts"
	)

#if HL2_EPISODIC
	//=========================================================
	// > SCHED_CITIZEN_HEAL_TOSS
	// this is for the episodic behavior where the citizen hurls the medkit
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_HEAL_TOSS,

	"	Tasks"
//  "		TASK_GET_PATH_TO_TARGET				0"
//  "		TASK_MOVE_TO_TARGET_RANGE			50"
	"		TASK_STOP_MOVING					0"
	"		TASK_FACE_IDEAL						0"
//	"		TASK_SAY_HEAL						0"
//	"		TASK_PLAY_SEQUENCE_FACE_TARGET		ACTIVITY:ACT_ARM"
	"		TASK_CIT_HEAL_TOSS							0"
//	"		TASK_PLAY_SEQUENCE_FACE_TARGET		ACTIVITY:ACT_DISARM"
	"	"
	"	Interrupts"
	)
#endif

	//=========================================================
	// > SCHED_CITIZEN_RANGE_ATTACK1_RPG
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_RANGE_ATTACK1_RPG,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_ANNOUNCE_ATTACK		1"	// 1 = primary attack
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_CIT_RPG_AUGER			1"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_CITIZEN_RANGE_ATTACK1_RPG
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_STRIDER_RANGE_ATTACK1_RPG,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_ANNOUNCE_ATTACK		1"	// 1 = primary attack
		"		TASK_WAIT					1"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_CIT_RPG_AUGER			1"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		""
		"	Interrupts"
	)


	//=========================================================
	// > SCHED_CITIZEN_PATROL
	//=========================================================
	DEFINE_SCHEDULE	
	(
		SCHED_CITIZEN_PATROL,
		  
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						901024"		// 90 to 1024 units
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT						3"
		"		TASK_WAIT_RANDOM				3"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_CITIZEN_PATROL" // keep doing it
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_CITIZEN_MOURN_PLAYER,
		  
		"	Tasks"
		"		TASK_GET_PATH_TO_PLAYER		0"
		"		TASK_RUN_PATH_WITHIN_DIST	180"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_STOP_MOVING			0"
		"		TASK_TARGET_PLAYER			0"
		"		TASK_FACE_TARGET			0"
		"		TASK_CIT_SPEAK_MOURNING		0"
		"		TASK_SUGGEST_STATE			STATE:IDLE"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_CITIZEN_PLAY_INSPECT_ACTIVITY,
		  
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_CIT_PLAY_INSPECT_SEQUENCE	0"	// Play the sequence the scanner requires
		"		TASK_WAIT						2"
		""
		"	Interrupts"
		"		"
	)

	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_SIT_ON_TRAIN,

		"	Tasks"
		"		TASK_CIT_SIT_ON_TRAIN		0"
		"		TASK_WAIT_RANDOM			1"
		"		TASK_CIT_LEAVE_TRAIN		0"
		""
		"	Interrupts"
	)
#ifdef EZ
	//=========================================================
	// > RangeAttack1Advance
	//	New schedule by 1upD to fire and then charge towards the player
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_RANGE_ATTACK1_ADVANCE,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_RANGE_ATTACK1		0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_CHASE_ENEMY"
		""
		"	Interrupts"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_LOST_ENEMY"
		"		COND_BETTER_WEAPON_AVAILABLE"
		"		COND_HEAR_DANGER"
		"		COND_CIT_WILLPOWER_LOW"
	);

	//===============================================
	//	> RangeAttack1Suppress
	//	1upD - Currently essentially the same
	//		as SCHED_RANGE_ATTACK1, but will
	//		be updated with new tasks
	//===============================================
		DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_RANGE_ATTACK1_SUPPRESS,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_CIT_PAINT_SUPPRESSION_TARGET	0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_RANGE_ATTACK1					0"
		""
		"	Interrupts"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_WEAPON_BLOCKED_BY_FRIEND"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
	);

	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CITIZEN_BURNING_STAND,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE_ON_FIRE"
		"		TASK_WAIT						3.5" // Blixibon - Metrocops die after 1.5, but 3.5 gives citizens more scorch time and players more traumatization time
		"		TASK_CIT_DIE_INSTANTLY			DMG_BURN"
		"		TASK_WAIT						1.0"
		"	"
		"	Interrupts"
	);
#endif
AI_END_CUSTOM_NPC()



//==================================================================================================================
// CITIZEN PLAYER-RESPONSE SYSTEM
//
// NOTE: This system is obsolete, and left here for legacy support.
//		 It has been superseded by the ai_eventresponse system.
//
//==================================================================================================================
CHandle<CCitizenResponseSystem>	g_pCitizenResponseSystem = NULL;

CCitizenResponseSystem	*GetCitizenResponse()
{
	return g_pCitizenResponseSystem;
}

char *CitizenResponseConcepts[MAX_CITIZEN_RESPONSES] = 
{
	"TLK_CITIZEN_RESPONSE_SHOT_GUNSHIP",
	"TLK_CITIZEN_RESPONSE_KILLED_GUNSHIP",
	"TLK_VITALNPC_DIED",
};

LINK_ENTITY_TO_CLASS( ai_citizen_response_system, CCitizenResponseSystem );

BEGIN_DATADESC( CCitizenResponseSystem )
	DEFINE_ARRAY( m_flResponseAddedTime, FIELD_FLOAT, MAX_CITIZEN_RESPONSES ),
	DEFINE_FIELD( m_flNextResponseTime, FIELD_FLOAT ),

	DEFINE_INPUTFUNC( FIELD_VOID,	"ResponseVitalNPC",	InputResponseVitalNPC ),

	DEFINE_THINKFUNC( ResponseThink ),
END_DATADESC()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCitizenResponseSystem::Spawn()
{
	if ( g_pCitizenResponseSystem )
	{
		Warning("Multiple citizen response systems in level.\n");
		UTIL_Remove( this );
		return;
	}
	g_pCitizenResponseSystem = this;

	// Invisible, non solid.
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW );
	SetThink( &CCitizenResponseSystem::ResponseThink );

	m_flNextResponseTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCitizenResponseSystem::OnRestore()
{
	BaseClass::OnRestore();

	g_pCitizenResponseSystem = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCitizenResponseSystem::AddResponseTrigger( citizenresponses_t	iTrigger )
{
	m_flResponseAddedTime[ iTrigger ] = gpGlobals->curtime;

	SetNextThink( gpGlobals->curtime + 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCitizenResponseSystem::InputResponseVitalNPC( inputdata_t &inputdata )
{
	AddResponseTrigger( CR_VITALNPC_DIED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCitizenResponseSystem::ResponseThink()
{
	bool bStayActive = false;
	if ( AI_IsSinglePlayer() )
	{
		for ( int i = 0; i < MAX_CITIZEN_RESPONSES; i++ )
		{
			if ( m_flResponseAddedTime[i] )
			{
				// Should it have expired by now?
				if ( (m_flResponseAddedTime[i] + CITIZEN_RESPONSE_GIVEUP_TIME) < gpGlobals->curtime )
				{
					m_flResponseAddedTime[i] = 0;
				}
				else if ( m_flNextResponseTime < gpGlobals->curtime )
				{
					// Try and find the nearest citizen to the player
					float flNearestDist = (CITIZEN_RESPONSE_DISTANCE * CITIZEN_RESPONSE_DISTANCE);
					CBaseEntity *pNearestCitizen = NULL;
					CBaseEntity *pCitizen = NULL;
					CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
					while ( (pCitizen = gEntList.FindEntityByClassname( pCitizen, "npc_citizen" ) ) != NULL)
					{
						float flDistToPlayer = (pPlayer->WorldSpaceCenter() - pCitizen->WorldSpaceCenter()).LengthSqr();
						if ( flDistToPlayer < flNearestDist )
						{
							flNearestDist = flDistToPlayer;
							pNearestCitizen = pCitizen;
						}
					}

					// Found one?
					if ( pNearestCitizen && ((CNPC_Citizen*)pNearestCitizen)->RespondedTo( CitizenResponseConcepts[i], false, false ) )
					{
						m_flResponseAddedTime[i] = 0;
						m_flNextResponseTime = gpGlobals->curtime + CITIZEN_RESPONSE_REFIRE_TIME;

						// Don't issue multiple responses
						break;
					}
				}
				else
				{
					bStayActive = true;
				}
			}
		}
	}

	// Do we need to keep thinking?
	if ( bStayActive )
	{
		SetNextThink( gpGlobals->curtime + 0.1 );
	}
}

void CNPC_Citizen::AddInsignia()
{
	CBaseEntity *pMark = CreateEntityByName( "squadinsignia" );
	pMark->SetOwnerEntity( this );
	pMark->Spawn();
}

void CNPC_Citizen::RemoveInsignia()
{
	// This is crap right now.
	CBaseEntity *FirstEnt();
	CBaseEntity *pEntity = gEntList.FirstEnt();

	while( pEntity )
	{
		if( pEntity->GetOwnerEntity() == this )
		{
			// Is this my insignia?
			CSquadInsignia *pInsignia = dynamic_cast<CSquadInsignia *>(pEntity);

			if( pInsignia )
			{
				UTIL_Remove( pInsignia );
				return;
			}
		}

		pEntity = gEntList.NextEnt( pEntity );
	}
}

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( squadinsignia, CSquadInsignia );

void CSquadInsignia::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "eyes" );
		if ( attachment )
		{
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			vecPosition.Init( -2.5, 0, 3.9 );
			SetLocalOrigin( vecPosition );
		}
	}

	SetModel( INSIGNIA_MODEL );
	SetSolid( SOLID_NONE );	
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CNPC_Citizen::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		Q_snprintf(tempstr,sizeof(tempstr),"Expression type: %s", szExpressionTypes[m_ExpressionType]);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}
#ifdef EZ
//------------------------------------------------------------------------------
// Added by 1upD. Citizen weapon proficiency should be configurable
//------------------------------------------------------------------------------
WeaponProficiency_t CNPC_Citizen::CalcWeaponProficiency(CBaseCombatWeapon *pWeapon)
{
	int proficiency;

	if (FClassnameIs(pWeapon, "weapon_ar2"))
	{
		proficiency = sk_citizen_ar2_proficiency.GetInt();
	}
	else {
		proficiency = sk_citizen_default_proficiency.GetInt();
	}
	
	// Clamp the upper range of the ConVar to "perfect"
	// Could also cast the int to the enum if you're feeling dangerous
	if (proficiency > 4) {
		return WEAPON_PROFICIENCY_PERFECT;
	}
	
	// Switch values to enum. You could cast the int to the enum here,
	// but I'm not sure how 'safe' that is since the enum values are
	// not explicity defined. This also clamps the range.
	switch (proficiency) {
		case 4:
			return WEAPON_PROFICIENCY_PERFECT;
		case 3:
			return WEAPON_PROFICIENCY_VERY_GOOD;
		case 2:
			return WEAPON_PROFICIENCY_GOOD;
		case 1:
			return WEAPON_PROFICIENCY_AVERAGE;
		default:
			return WEAPON_PROFICIENCY_POOR;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
//		1upD - If this rebel is using Aperture Science tech, he should be able
//				to jump incredible distances
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Citizen::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
#ifdef EZ2
	// Jump rebels can jump incredible distances, but not if they have surrendered
	if ( m_Type == CT_LONGFALL && !m_SurrenderBehavior.IsSurrendered() )
	{
		const float MAX_JUMP_RISE = 1024.0f; // This one might need some adjustment - how high is too high?
		const float MAX_JUMP_DISTANCE = 1024.0f;
		const float MAX_JUMP_DROP = 8192.0f; // Basically any height

		return BaseClass::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE);
	}
#endif
	return BaseClass::IsJumpLegal(startPos, apex, endPos);
}

//-----------------------------------------------------------------------------
// Override TestShootPosition for jump rebels - if the potential shoot position is not above the target, don't select it
//-----------------------------------------------------------------------------
bool CNPC_Citizen::TestShootPosition( const Vector &vecShootPos, const Vector &targetPos )
{
	if ( m_Type == CT_LONGFALL && npc_citizen_longfall_test_high_ground.GetBool() &&  vecShootPos.z <= targetPos.z )
	{
		return false;
	}

	return BaseClass::TestShootPosition( vecShootPos, targetPos );
}
#endif
