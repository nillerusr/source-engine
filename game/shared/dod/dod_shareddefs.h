//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_SHAREDDEFS_H
#define DOD_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_playeranimstate.h"


#define DOD_PLAYER_VIEW_OFFSET	Vector( 0, 0, 54 )

// DOD Team IDs.
#define TEAM_ALLIES			2
#define	TEAM_AXIS			3
#define TEAM_MAXCOUNT		4	// update this if we ever add teams (unlikely)

enum SubTeam
{
	SUBTEAM_NORMAL = 0,
	SUBTEAM_PARA,
	SUBTEAM_ALT_NATION,
	NUM_SUBTEAMS
};

#define MAX_CONTROL_POINTS 8
#define MAX_CONTROL_POINT_GROUPS	8

#define DEATH_CAM_TIME	5.0f

#define MAX_WAVE_RESPAWN_TIME	20.0f

#define DOD_BOMB_TIMER_LENGTH	20
#define DOD_BOMB_DEFUSE_TIME	3.0f
#define DOD_BOMB_PLANT_TIME		2.0f
#define DOD_BOMB_PLANT_RADIUS	80
#define DOD_BOMB_DEFUSE_MAXDIST 96.0f

enum
{
	CAP_EVENT_NONE,
	CAP_EVENT_BOMB,
	CAP_EVENT_FLAG,
	CAP_EVENT_TIMER_EXPIRE
};

enum
{
	WINPANEL_TOP3_NONE,
	WINPANEL_TOP3_BOMBERS,
	WINPANEL_TOP3_CAPPERS,
	WINPANEL_TOP3_DEFENDERS,
	WINPANEL_TOP3_KILLERS
};

//--------------
// DoD Specific damage flags
//--------------	

// careful when reusing HL2 DMG_ flags, some of them cancel out the damage, eg if they are in DMG_TIMEBASED

#define DMG_STUN			(DMG_PARALYZE)				//(1<<15)
#define DMG_MACHINEGUN		(DMG_LASTGENERICFLAG<<1)	//(1<<30)
#define DMG_BOMB			(DMG_LASTGENERICFLAG<<2)	//(1<<31)

//END OF THE EXTENDABLE LIST!! Start reusing HL2 specific flags

#define MELEE_DMG_SECONDARYATTACK	(1<<0)
#define MELEE_DMG_FIST				(1<<1)
#define MELEE_DMG_EDGE				(1<<2)
#define MELEE_DMG_STRONGATTACK		(1<<3)

#define SANDBAG_NOT_TOUCHING		0
#define SANDBAG_TOUCHING			1
#define SANDBAG_TOUCHING_ALIGNED	2
#define SANDBAG_DEPLOYED			3

#define PRONE_DEPLOY_HEIGHT	-1

#define STANDING_DEPLOY_HEIGHT	58
#define CROUCHING_DEPLOY_HEIGHT	28
#define TIME_TO_DEPLOY 0.3

#define MIN_DEPLOY_PITCH	45
#define MAX_DEPLOY_PITCH	-60

#define VEC_DUCK_MOVING_HULL_MIN	Vector(-16, -16, 0 )
#define VEC_DUCK_MOVING_HULL_MAX	Vector( 16,  16, 55 )

#define VEC_PRONE_HULL_MIN	DODGameRules()->GetDODViewVectors()->m_vProneHullMin
#define VEC_PRONE_HULL_MAX	DODGameRules()->GetDODViewVectors()->m_vProneHullMax	// MUST be shorter than duck hull for deploy check

#define VEC_PRONE_HULL_MIN_SCALED( player )	( DODGameRules()->GetDODViewVectors()->m_vProneHullMin * player->GetModelScale() )
#define VEC_PRONE_HULL_MAX_SCALED( player )	( DODGameRules()->GetDODViewVectors()->m_vProneHullMax * player->GetModelScale() )

#define VEC_PRONE_HELPER_HULL_MIN	Vector(-48, -48, 0 )
#define VEC_PRONE_HELPER_HULL_MAX	Vector( 48,  48,  24 )

#define GRENADE_FUSE_LENGTH	5.0
#define RIFLEGRENADE_FUSE_LENGTH 3.5

#define CONTENTS_PRONE_HELPER			0x80000000

#define PASS_OUT_CHANGE_TIME	1.5
#define PASS_OUT_GET_UP_TIME	1.5

// DOD-specific viewport panels
#define PANEL_TEAM				"team"
#define PANEL_CLASS_ALLIES		"class_us"
#define PANEL_CLASS_AXIS		"class_ger"
#define PANEL_BACKGROUND		"background"

#define COLOR_DOD_GREEN	Color(77, 121, 66, 255) //	Color(76, 102, 76, 255)
#define COLOR_DOD_RED	Color(255, 64, 64, 255)

#define DOD_HUD_HEALTH_IMAGE_LENGTH 64

// The various states the player can be in during the join game process.
enum DODPlayerState
{
	// Happily running around in the game.
	// This state can jump to a bunch of other states like STATE_PICKINGCLASS or STATE_DEATH_ANIM.
	STATE_ACTIVE=0,
	
	// This is the state you're in when you first enter the server.
	// It's switching between intro cameras every few seconds, and there's a level info 
	// screen up.
	STATE_WELCOME,			// Show the level intro screen.
	
	// During these states, you can either be a new player waiting to join, or
	// you can be a live player in the game who wants to change teams.
	// Either way, you can't move while choosing team or class (or while any menu is up).
	STATE_PICKINGTEAM,			// Choosing team.
	STATE_PICKINGCLASS,			// Choosing class.
	
	STATE_DEATH_ANIM,			// Playing death anim, waiting for that to finish.
	STATE_DEATH_OBSERVING,		// Done playing death anim. Waiting for keypress to go into observer mode.
	STATE_OBSERVER_MODE,		// Noclipping around, watching players, etc.

	NUM_PLAYER_STATES
};

enum DODRoundState
{
	// initialize the game, create teams
	STATE_INIT=0,

	//Before players have joined the game. Periodically checks to see if enough players are ready
	//to start a game. Also reverts to this when there are no active players
	STATE_PREGAME,

	//The game is about to start, wait a bit and spawn everyone
	STATE_STARTGAME,

	//All players are respawned, frozen in place
	STATE_PREROUND,

	//Round is on, playing normally
	STATE_RND_RUNNING,

	//Someone has won the round
	STATE_ALLIES_WIN,
	STATE_AXIS_WIN,

	//Noone has won, manually restart the game, reset scores
	STATE_RESTART,

	//Game is over, showing the scoreboard etc
	STATE_GAME_OVER,

	NUM_ROUND_STATES
};

#define PLAYERCLASS_RANDOM		-2
#define PLAYERCLASS_UNDEFINED	-1

#define DOD_PLAYERMODEL_AXIS_RIFLEMAN	"models/player/german_rifleman.mdl"	
#define DOD_PLAYERMODEL_AXIS_ASSAULT	"models/player/german_assault.mdl"	
#define DOD_PLAYERMODEL_AXIS_SUPPORT	"models/player/german_support.mdl"
#define DOD_PLAYERMODEL_AXIS_SNIPER		"models/player/german_sniper.mdl"
#define DOD_PLAYERMODEL_AXIS_MG			"models/player/german_mg.mdl"	
#define DOD_PLAYERMODEL_AXIS_ROCKET		"models/player/german_rocket.mdl"	

#define DOD_PLAYERMODEL_US_RIFLEMAN		"models/player/american_rifleman.mdl"
#define DOD_PLAYERMODEL_US_ASSAULT		"models/player/american_assault.mdl"
#define DOD_PLAYERMODEL_US_SUPPORT		"models/player/american_support.mdl"
#define DOD_PLAYERMODEL_US_SNIPER		"models/player/american_sniper.mdl"	
#define DOD_PLAYERMODEL_US_MG			"models/player/american_mg.mdl"
#define DOD_PLAYERMODEL_US_ROCKET		"models/player/american_rocket.mdl"

typedef struct DodClassInfo_s
{
	char selectcmd[32];
	char classname[128];
	char modelname[128];

	int team;		//which team. 0 == allies, 1 == axis

	int primarywpn;
	int secondarywpn;
	int meleewpn;

	int numgrenades;
	int armskin;	//what skin does this class show in grenades / knives

	int headgroup;	//bodygroups
	int helmetgroup;
	int geargroup;
	int bodygroup;
	int hairgroup;	//what helmet group to switch to when the helmet comes off
	
} DodClassInfo_t;

extern DodClassInfo_t g_ClassInfo[];	//a structure to hold all of the classes
extern DodClassInfo_t g_ParaClassInfo[];

// Voice Commands
typedef struct DodVoiceCommand_s
{
	const char *pszCommandName;	// console command that will produce the voice command

	const char *pszSoundName;		// name of sound to play

	PlayerAnimEvent_t iHandSignal;		//index into the hand signal array

	const char *pszAlliedSubtitle;	// subtitles for each nationality
	const char *pszAxisSubtitle;
	const char *pszBritishSubtitle;
	
} DodVoiceCommand_t;

extern DodVoiceCommand_t g_VoiceCommands[];

// Hand Signals
typedef struct DodHandSignal_s
{
	const char *pszCommandName;	// console command that will produce the voice command

	PlayerAnimEvent_t iHandSignal;		//index into the hand signal array

	const char *pszSubtitle;	// subtitles for each nationality

} DodHandSignal_t;

extern DodHandSignal_t g_HandSignals[];

#define ARM_SKIN_UNDEFINED 0

enum
{
	HEAD_GROUP_0 = 0,
	HEAD_GROUP_1,
	HEAD_GROUP_2,
	HEAD_GROUP_3,
	HEAD_GROUP_4,
	HEAD_GROUP_5,
	HEAD_GROUP_6,
};

enum
{
	HELMET_GROUP_0 = 0,
	HELMET_GROUP_1,
	HELMET_GROUP_2,
	HELMET_GROUP_3,
	HELMET_GROUP_4,
	HELMET_GROUP_5,
	HELMET_GROUP_6,
	HELMET_GROUP_7,
};

enum
{
	BODY_GROUP_0 = 0,
	BODY_GROUP_1,
	BODY_GROUP_2,
	BODY_GROUP_3,
	BODY_GROUP_4,
	BODY_GROUP_5,
};

enum
{
	GEAR_GROUP_0 = 0,
	GEAR_GROUP_1,
	GEAR_GROUP_2,
	GEAR_GROUP_3,
	GEAR_GROUP_4,
	GEAR_GROUP_5,
	GEAR_GROUP_6,
};

enum
{
	BODYGROUP_BODY = 0,
	BODYGROUP_HELMET,
	BODYGROUP_HEAD,
	BODYGROUP_GEAR,
	BODYGROUP_JUMPGEAR
};

enum
{
	BODYGROUP_HEAD1 = 0,
	BODYGROUP_HEAD2,
	BODYGROUP_HEAD3,
	BODYGROUP_HEAD4,
	BODYGROUP_HEAD5,
	BODYGROUP_HEAD6,
	BODYGROUP_HEAD7
};

//helmet groups
#define BODYGROUP_HELMET_ON 0

#define BODYGROUP_ALLIES_HELMET_HELMET1 0
#define BODYGROUP_ALLIES_HELMET_HELMET2 1
#define BODYGROUP_ALLIES_HELMET_OFF 2

#define BODYGROUP_AXIS_HAIR0	1
#define BODYGROUP_AXIS_HAIR1	2
#define BODYGROUP_AXIS_HAIR2	3
#define BODYGROUP_AXIS_HAIR3	4
#define BODYGROUP_AXIS_HAIR4	5
#define BODYGROUP_AXIS_HAIR5	6
#define BODYGROUP_AXIS_HAIR6	7

//battle gear groups
#define BODYGROUP_TOMMYGEAR			0
#define BODYGROUP_SPRINGGEAR		1
#define BODYGROUP_GARANDGEAR		2
#define BODYGROUP_MGGEAR			3
#define BODYGROUP_BARGEAR			4
#define BODYGROUP_CARBGEAR			5
#define BODYGROUP_GREASEGUNGEAR		6

//jump gear
#define BODYGROUP_JUMPGEAR_OFF 0
#define BODYGROUP_JUMPGEAR_ON 1

enum
{
	HELMET_ALLIES = 0,
	HELMET_AXIS,

	NUM_HELMETS
};

extern const char *m_pszHelmetModels[NUM_HELMETS];

//Materials
/*
#define CHAR_TEX_CONCRETE	'C'			// texture types
#define CHAR_TEX_METAL		'M'
#define CHAR_TEX_DIRT		'D'
#define CHAR_TEX_GRATE		'G'
#define CHAR_TEX_TILE		'T'
#define CHAR_TEX_WOOD		'W'
#define CHAR_TEX_GLASS		'Y'
#define CHAR_TEX_FLESH		'F'
#define CHAR_TEX_WATER		'S'
#define CHAR_TEX_ROCK		'R'
#define CHAR_TEX_SAND		'A'
#define CHAR_TEX_GRAVEL		'L'
#define CHAR_TEX_STUCCO		'Z'
#define CHAR_TEX_BRICK		'B'
#define CHAR_TEX_SNOW		'N'
#define CHAR_TEX_HEAVYMETAL	'H'
#define CHAR_TEX_LEAVES		'E'
#define CHAR_TEX_SKY		'K'
#define CHAR_TEX_GRASS		'P'
*/


#define WPN_SLOT_PRIMARY	0
#define WPN_SLOT_SECONDARY	1
#define WPN_SLOT_MELEE		2
#define WPN_SLOT_GRENADES	3
#define WPN_SLOT_BOMB		4

#define SLEEVE_AXIS		0
#define SLEEVE_ALLIES	1

#define VM_BODYGROUP_GUN	0
#define VM_BODYGROUP_SLEEVE	1

#define PLAYER_SPEED_FROZEN				1
#define PLAYER_SPEED_PRONE				50
#define PLAYER_SPEED_PRONE_ZOOMED		30
#define PLAYER_SPEED_PRONE_BAZOOKA_DEPLOYED		30
#define PLAYER_SPEED_ZOOMED				42
#define PLAYER_SPEED_BAZOOKA_DEPLOYED	50
#define PLAYER_SPEED_NORMAL				600.0f

#define PLAYER_SPEED_SLOWED				120
#define PLAYER_SPEED_RUN				220
#define PLAYER_SPEED_SPRINT				330

#define PUSHAWAY_THINK_INTERVAL		(1.0f / 20.0f)

#define VEC_PRONE_VIEW			Vector(0,0,10)
#define VEC_PRONE_VIEW_SCALED( player )			( Vector(0,0,10) * player->GetModelScale() )

#define TIME_TO_PRONE	1.2f	// should be 1.5!

#define INITIAL_SPRINT_STAMINA_PENALTY 15
#define LOW_STAMINA_THRESHOLD	35

// changed to 80% of goldsrc values, gives the same end result
#define ZOOM_SWAY_PRONE				0.1
#define ZOOM_SWAY_DUCKING			0.2
#define ZOOM_SWAY_STANDING			0.5
#define ZOOM_SWAY_MOVING_PENALTY	0.4

extern const char * s_WeaponAliasInfo[];

enum
{
	//Dod hint messages					
	HINT_FRIEND_SEEN = 0,				// #Hint_spotted_a_friend
	HINT_ENEMY_SEEN,					// #Hint_spotted_an_enemy
	HINT_FRIEND_INJURED,				// #Hint_try_not_to_injure_teammates
	HINT_FRIEND_KILLED,					// #Hint_careful_around_teammates
	HINT_ENEMY_KILLED,					// #Hint_killing_enemies_is_good
	HINT_IN_AREA_CAP,					// #Hint_touched_area_capture
	HINT_FLAG_TOUCH,					// #Hint_touched_control_point
	HINT_OBJECT_PICKUP,					// #Hint_picked_up_object
	HINT_MG_FIRE_UNDEPLOYED,			// #Hint_mgs_fire_better_deployed
	HINT_SANDBAG_AREA,					// #Hint_sandbag_area_touch
	HINT_BAZOOKA_PICKUP,				// #Hint_rocket_weapon_pickup
	HINT_AMMO_EXHAUSTED,				// #Hint_out_of_ammo
	HINT_PRONE,							// #Hint_prone
	HINT_LOW_STAMINA,					// #Hint_low_stamina
	HINT_OBJECT_REQUIRED,				// #Hint_area_requires_object	
	HINT_PLAYER_KILLED_WAVETIME,		// #Hint_player_killed_wavetime 
	HINT_WEAPON_OVERHEAT,				// #Hint_mg_overheat
	HINT_SHOULDER_WEAPON,				// #game_shoulder_rpg

	HINT_PICK_UP_WEAPON,				// #Hint_pick_up_weapon
	HINT_PICK_UP_GRENADE,				// #Hint_pick_up_grenade
	HINT_DEATHCAM,						// #Hint_death_cam
	HINT_CLASSMENU,						// #Hint_class_menu

	HINT_USE_MELEE,						// #Hint_use_2e_melee
	HINT_USE_ZOOM,						// #Hint_use_zoom
	HINT_USE_IRON_SIGHTS,				// #Hint_use_iron_sights
	HINT_USE_SEMI_AUTO,					// #Hint_use_semi_auto
	HINT_USE_SPRINT,					// #Hint_use_sprint
	HINT_USE_DEPLOY,					// #Hint_use_deploy
	HINT_USE_PRIME,						// #Hint_use_prime

	HINT_MG_DEPLOY_USAGE,				// #Hint_mg_deploy_usage

	HINT_MG_DEPLOY_TO_RELOAD,			// #Dod_mg_reload
	HINT_GARAND_RELOAD,					// #Hint_garand_reload

	HINT_TURN_OFF_HINTS,				// #Hint_turn_off_hints

	HINT_NEED_BOMB,						// #Hint_need_bomb_to_plant
	HINT_BOMB_PLANTED,					// #Hint_bomb_planted
	HINT_DEFUSE_BOMB,					// #Hint_defuse_bomb
	HINT_BOMB_TARGET,					// #Hint_bomb_target
	HINT_BOMB_PICKUP,					// #Hint_bomb_pickup
	HINT_BOMB_DEFUSE_ONGROUND,			// #Hint_bomb_defuse_onground

	HINT_BOMB_PLANT_MAP,				// #Hint_bomb_plant_map
	HINT_BOMB_FIRST_SELECT,				// #Hint_bomb_first_select

	NUM_HINTS
};

extern const char *g_pszHintMessages[];

// HINT_xxx bits to clear when the round restarts
#define HINT_MASK_SPAWN_CLEAR ( 1 << HINT_FRIEND_KILLED )

// criteria for when a weapon model wants to use an alt model
#define ALTWPN_CRITERIA_NONE			0
#define ALTWPN_CRITERIA_FIRING			(1 << 0)
#define ALTWPN_CRITERIA_RELOADING		(1 << 1)
#define ALTWPN_CRITERIA_DEPLOYED		(1 << 2)
#define ALTWPN_CRITERIA_DEPLOYED_RELOAD	(1 << 3)
#define ALTWPN_CRITERIA_PRONE			(1 << 4)	// player is prone
#define ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD (1 << 5)	// player should use special alt model when prone deployed reloading

// eject brass shells
#define EJECTBRASS_PISTOL		0
#define EJECTBRASS_RIFLE		1
#define EJECTBRASS_MG			2
#define EJECTBRASS_MG_2			3	// ?
#define EJECTBRASS_GARANDCLIP	4

extern const char *pszTeamAlliesClasses[];
extern const char *pszTeamAxisClasses[];

enum
{
	DOD_COLLISIONGROUP_SHELLS = LAST_SHARED_COLLISION_GROUP,
	DOD_COLLISIONGROUP_BLOCKERWALL,
};

enum
{
	PROGRESS_BAR_BANDAGER = 0,
	PROGRESS_BAR_BANDAGEE,
	PROGRESS_BAR_CAP,		// done by objective resource

	NUM_PROGRESS_BAR_TYPES
};

// used for the corner cut panels in the HUD
enum
{
	DOD_CORNERCUT_PANEL_BOTTOMRIGHT = 0,
	DOD_CORNERCUT_PANEL_BOTTOMLEFT,
	DOD_CORNERCUT_PANEL_TOPRIGHT,
	DOD_CORNERCUT_PANEL_TOPLEFT,
};

enum ViewAnimationType {
	VIEW_ANIM_LINEAR_Z_ONLY,
	VIEW_ANIM_SPLINE_Z_ONLY,
	VIEW_ANIM_EXPONENTIAL_Z_ONLY,
};

enum BombTargetState
{
	// invisible, not active
	BOMB_TARGET_INACTIVE=0,

	// visible, accepts planting +use
	BOMB_TARGET_ACTIVE,

	// visible, accepts disarm +use, counts down to explosion
	// if disarmed, returns to BOMB_TARGET_ACTIVE
	// if explodes, returns to BOMB_TARGET_INACTIVE
	BOMB_TARGET_ARMED,

	NUM_BOMB_TARGET_STATES
};

extern const char *pszWinPanelCategoryHeaders[];

enum
{
	// Season 1
	ACHIEVEMENT_DOD_THROW_BACK_GREN = 0,
	ACHIEVEMENT_DOD_CONSECUTIVE_HEADSHOTS,
	ACHIEVEMENT_DOD_MG_POSITION_STREAK,
	ACHIEVEMENT_DOD_WIN_KNIFE_FIGHT,
	ACHIEVEMENT_DOD_PLAY_CUSTOM_MAPS,
	ACHIEVEMENT_DOD_KILLS_WITH_GRENADE,
	ACHIEVEMENT_DOD_LONG_RANGE_ROCKET,
	ACHIEVEMENT_DOD_END_ROUND_KILLS,
	ACHIEVEMENT_DOD_CAP_LAST_FLAG,
	ACHIEVEMENT_DOD_USE_ENEMY_WEAPONS,
	ACHIEVEMENT_DOD_KILL_DOMINATING_MG,
	ACHIEVEMENT_DOD_COLMAR_DEFENSE,
	ACHIEVEMENT_DOD_BLOCK_CAPTURES,
	ACHIEVEMENT_DOD_JAGD_OVERTIME_CAP,
	ACHIEVEMENT_DOD_WEAPON_MASTERY,

	// grinds
	ACHIEVEMENT_DOD_KILLS_AS_ALLIES,
	ACHIEVEMENT_DOD_KILLS_AS_AXIS,

	ACHIEVEMENT_DOD_KILLS_AS_RIFLEMAN,
	ACHIEVEMENT_DOD_KILLS_AS_ASSAULT,
	ACHIEVEMENT_DOD_KILLS_AS_SUPPORT,
	ACHIEVEMENT_DOD_KILLS_AS_SNIPER,
	ACHIEVEMENT_DOD_KILLS_AS_MG,
	ACHIEVEMENT_DOD_KILLS_AS_BAZOOKAGUY,

	ACHIEVEMENT_DOD_KILLS_WITH_GARAND,
	ACHIEVEMENT_DOD_KILLS_WITH_THOMPSON,
	ACHIEVEMENT_DOD_KILLS_WITH_BAR,
	ACHIEVEMENT_DOD_KILLS_WITH_SPRING,
	ACHIEVEMENT_DOD_KILLS_WITH_30CAL,
	ACHIEVEMENT_DOD_KILLS_WITH_BAZOOKA,
	ACHIEVEMENT_DOD_KILLS_WITH_K98,
	ACHIEVEMENT_DOD_KILLS_WITH_MP40, 
	ACHIEVEMENT_DOD_KILLS_WITH_MP44, 
	ACHIEVEMENT_DOD_KILLS_WITH_K98SCOPED,
	ACHIEVEMENT_DOD_KILLS_WITH_MG42,
	ACHIEVEMENT_DOD_KILLS_WITH_PSCHRECK,
	ACHIEVEMENT_DOD_KILLS_WITH_COLT, 
	ACHIEVEMENT_DOD_KILLS_WITH_P38,
	ACHIEVEMENT_DOD_KILLS_WITH_C96,
	ACHIEVEMENT_DOD_KILLS_WITH_M1CARBINE,
	ACHIEVEMENT_DOD_KILLS_WITH_AMERKNIFE,
	ACHIEVEMENT_DOD_KILLS_WITH_SPADE,
	ACHIEVEMENT_DOD_KILLS_WITH_PUNCH,
	ACHIEVEMENT_DOD_KILLS_WITH_FRAG_US,
	ACHIEVEMENT_DOD_KILLS_WITH_FRAG_GER,
	ACHIEVEMENT_DOD_KILLS_WITH_RIFLEGREN_US,
	ACHIEVEMENT_DOD_KILLS_WITH_RIFLEGREN_GER,

	ACHIEVEMENT_DOD_CAPTURE_GRIND,
	ACHIEVEMENT_DOD_BLOCK_CAPTURES_GRIND,
	ACHIEVEMENT_DOD_ROUNDS_WON_GRIND,

	ACHIEVEMENT_DOD_BOMBS_PLANTED_GRIND,
	ACHIEVEMENT_DOD_BOMBS_DEFUSED_GRIND,

	ACHIEVEMENT_DOD_ALL_PACK_1,

	ACHIEVEMENT_DOD_BEAT_THE_HEAT,

	// Winter 2011
	ACHIEVEMENT_DOD_COLLECT_HOLIDAY_GIFTS,

	NUM_DOD_ACHIEVEMENTS
};

#define ACHIEVEMENT_NUM_CONSECUTIVE_HEADSHOTS 5
#define ACHIEVEMENT_MG_STREAK_IS_DOMINATING	8
#define ACHIEVEMENT_NUM_ENEMY_WPN_KILLS 5
#define ACHIEVEMENT_LONG_RANGE_ROCKET_DIST	1200
// other magic numbers exist inside the achievements themselves in achievements_dod.cpp

enum
{
	DOD_MUZZLEFLASH_PISTOL = 0,
	DOD_MUZZLEFLASH_AUTO,
	DOD_MUZZLEFLASH_RIFLE,
	DOD_MUZZLEFLASH_MG,
	DOD_MUZZLEFLASH_ROCKET,
	DOD_MUZZLEFLASH_MG42
};

#define DOD_KILLS_DOMINATION	4

// Death notice flags
#define DOD_DEATHFLAG_DOMINATION			0x0001	// killer is dominating victim
#define DOD_DEATHFLAG_REVENGE				0x0002	// killer got revenge on victim

enum
{
	ACHIEVEMENT_AWARDS_NONE = 0,
	ACHIEVEMENT_AWARDS_RIFLEMAN,
	ACHIEVEMENT_AWARDS_ASSAULT,	
	ACHIEVEMENT_AWARDS_SUPPORT,		
	ACHIEVEMENT_AWARDS_SNIPER,		
	ACHIEVEMENT_AWARDS_MG,		
	ACHIEVEMENT_AWARDS_ROCKET,
	ACHIEVEMENT_AWARDS_ALL_PACK_1,

	NUM_ACHIEVEMENT_AWARDS
};

extern const char *g_pszAchievementAwards[NUM_ACHIEVEMENT_AWARDS];
extern const char *g_pszAchievementAwardMaterials_Allies[NUM_ACHIEVEMENT_AWARDS];
extern const char *g_pszAchievementAwardMaterials_Axis[NUM_ACHIEVEMENT_AWARDS];

enum DODStatType_t
{
	DODSTAT_PLAYTIME = 0,
	DODSTAT_ROUNDSWON,
	DODSTAT_ROUNDSLOST,
	DODSTAT_KILLS,
	DODSTAT_DEATHS,
	DODSTAT_CAPTURES,
	DODSTAT_BLOCKS,
	DODSTAT_BOMBSPLANTED,
	DODSTAT_BOMBSDEFUSED,
	DODSTAT_DOMINATIONS,
	DODSTAT_REVENGES,
	DODSTAT_SHOTS_HIT,
	DODSTAT_SHOTS_FIRED,
	DODSTAT_HEADSHOTS,

	DODSTAT_MAX
};

#define DODSTAT_FIRST	DODSTAT_PLAYTIME

typedef struct 
{
	int m_iStat[DODSTAT_MAX];
	bool m_bDirty[DODSTAT_MAX];

} dod_stat_accumulator_t;

#define NUM_DOD_PLAYERCLASSES	6

#endif // DOD_SHAREDDEFS_H
