//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#ifndef TF_SHAREDDEFS_H
#define TF_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "mp_shareddefs.h"

// Using MAP_DEBUG mode?
#ifdef MAP_DEBUG
	#define MDEBUG(x) x
#else
	#define MDEBUG(x)
#endif

//-----------------------------------------------------------------------------
// Teams.
//-----------------------------------------------------------------------------
enum
{
	TF_TEAM_RED = LAST_SHARED_TEAM+1,
	TF_TEAM_BLUE,
	TF_TEAM_COUNT
};

#define TF_TEAM_AUTOASSIGN (TF_TEAM_COUNT + 1 )

extern const char *g_aTeamNames[TF_TEAM_COUNT];
extern color32 g_aTeamColors[TF_TEAM_COUNT];

#define COLOR_TF_SPECTATOR Color( 245, 229, 196, 255 )

#define CONTENTS_REDTEAM	CONTENTS_TEAM1
#define CONTENTS_BLUETEAM	CONTENTS_TEAM2

// Team roles
enum 
{
	TEAM_ROLE_NONE = 0,
	TEAM_ROLE_DEFENDERS,
	TEAM_ROLE_ATTACKERS,

	NUM_TEAM_ROLES,
};

//-----------------------------------------------------------------------------
// CVar replacements
//-----------------------------------------------------------------------------
#define TF_DAMAGE_CRIT_CHANCE				0.05f
#define TF_DAMAGE_CRIT_CHANCE_RAPID			0.05f
#define TF_DAMAGE_CRIT_DURATION_RAPID		2.0f
#define TF_DAMAGE_CRIT_CHANCE_MELEE			0.15f

#define TF_DAMAGE_CRITMOD_MAXTIME			20
#define TF_DAMAGE_CRITMOD_MINTIME			2
#define TF_DAMAGE_CRITMOD_DAMAGE			800
#define TF_DAMAGE_CRITMOD_MAXMULT			4

#define TF_DAMAGE_CRIT_MULTIPLIER			3.0f


//-----------------------------------------------------------------------------
// TF-specific viewport panels
//-----------------------------------------------------------------------------
#define PANEL_CLASS_BLUE		"class_blue"
#define PANEL_CLASS_RED			"class_red"
#define PANEL_MAPINFO			"mapinfo"
#define PANEL_ROUNDINFO			"roundinfo"

// file we'll save our list of viewed intro movies in
#define MOVIES_FILE				"viewed.res"

//-----------------------------------------------------------------------------
// Used in calculating the health percentage of a player
//-----------------------------------------------------------------------------
#define TF_HEALTH_UNDEFINED		1

//-----------------------------------------------------------------------------
// Used to mark a spy's disguise attribute (team or class) as "unused"
//-----------------------------------------------------------------------------
#define TF_SPY_UNDEFINED		TEAM_UNASSIGNED


#define COLOR_TF_BLUE	Color( 64, 64, 255, 255 )
#define COLOR_TF_RED	Color( 255, 64, 64, 255 )

//-----------------------------------------------------------------------------
// Player Classes.
//-----------------------------------------------------------------------------
#define TF_CLASS_COUNT			( TF_CLASS_COUNT_ALL - 1 )

#define TF_FIRST_NORMAL_CLASS	( TF_CLASS_UNDEFINED + 1 )
#define TF_LAST_NORMAL_CLASS	( TF_CLASS_CIVILIAN - 1 )

#define	TF_CLASS_MENU_BUTTONS	( TF_CLASS_RANDOM + 1 )

enum
{
	TF_CLASS_UNDEFINED = 0,

	TF_CLASS_SCOUT,			// TF_FIRST_NORMAL_CLASS
    TF_CLASS_SNIPER,
    TF_CLASS_SOLDIER,
	TF_CLASS_DEMOMAN,
	TF_CLASS_MEDIC,
	TF_CLASS_HEAVYWEAPONS,
	TF_CLASS_PYRO,
	TF_CLASS_SPY,
	TF_CLASS_ENGINEER,		// TF_LAST_NORMAL_CLASS

	// Add any new classes after Engineer
	TF_CLASS_CIVILIAN,		// Civilians are a special class. It is not a player class.
	TF_CLASS_COUNT_ALL,

	TF_CLASS_RANDOM
};

extern const char *g_aPlayerClassNames[];				// localized class names
extern const char *g_aPlayerClassNames_NonLocalized[];	// non-localized class names

//-----------------------------------------------------------------------------
// For entity_capture_flags to use when placed in the world
//-----------------------------------------------------------------------------
enum
{
	TF_FLAGTYPE_CTF = 0,
	TF_FLAGTYPE_ATTACK_DEFEND,
	TF_FLAGTYPE_TERRITORY_CONTROL,
	TF_FLAGTYPE_INVADE,
	TF_FLAGTYPE_KINGOFTHEHILL,
};

//-----------------------------------------------------------------------------
// For the game rules to determine which type of game we're playing
//-----------------------------------------------------------------------------
enum
{
	TF_GAMETYPE_UNDEFINED = 0,
	TF_GAMETYPE_CTF,
	TF_GAMETYPE_CP,
};
extern const char *g_aGameTypeNames[];	// localized gametype names

//-----------------------------------------------------------------------------
// Buildings.
//-----------------------------------------------------------------------------
enum
{
	TF_BUILDING_SENTRY				= (1<<0),
	TF_BUILDING_DISPENSER			= (1<<1),
	TF_BUILDING_TELEPORT_ENTRY		= (1<<2),
	TF_BUILDING_TELEPORT_EXIT		= (1<<3),
};

//-----------------------------------------------------------------------------
// Items.
//-----------------------------------------------------------------------------
enum
{
	TF_ITEM_UNDEFINED		= 0,
	TF_ITEM_CAPTURE_FLAG	= (1<<0),
	TF_ITEM_HEALTH_KIT		= (1<<1),
	TF_ITEM_ARMOR			= (1<<2),
	TF_ITEM_AMMO_PACK		= (1<<3),
	TF_ITEM_GRENADE_PACK	= (1<<4),
};

//-----------------------------------------------------------------------------
// Ammo.
//-----------------------------------------------------------------------------
enum
{
	TF_AMMO_DUMMY = 0,	// Dummy index to make the CAmmoDef indices correct for the other ammo types.
	TF_AMMO_PRIMARY,
	TF_AMMO_SECONDARY,
	TF_AMMO_METAL,
	TF_AMMO_GRENADES1,
	TF_AMMO_GRENADES2,
	TF_AMMO_COUNT
};

//-----------------------------------------------------------------------------
// Grenade Launcher mode (for pipebombs).
//-----------------------------------------------------------------------------
enum
{
	TF_GL_MODE_REGULAR = 0,
	TF_GL_MODE_REMOTE_DETONATE,
};

//-----------------------------------------------------------------------------
// Weapon Types
//-----------------------------------------------------------------------------
enum
{
	TF_WPN_TYPE_PRIMARY = 0,
	TF_WPN_TYPE_SECONDARY,
	TF_WPN_TYPE_MELEE,
	TF_WPN_TYPE_GRENADE,
	TF_WPN_TYPE_BUILDING,
	TF_WPN_TYPE_PDA,
};

extern const char *g_aAmmoNames[];

//-----------------------------------------------------------------------------
// Weapons.
//-----------------------------------------------------------------------------
#define TF_PLAYER_WEAPON_COUNT		5
#define TF_PLAYER_GRENADE_COUNT		2
#define TF_PLAYER_BUILDABLE_COUNT	4

#define TF_WEAPON_PRIMARY_MODE		0
#define TF_WEAPON_SECONDARY_MODE	1

#define TF_WEAPON_GRENADE_FRICTION						0.6f
#define TF_WEAPON_GRENADE_GRAVITY						0.81f
#define TF_WEAPON_GRENADE_INITPRIME						0.8f
#define TF_WEAPON_GRENADE_CONCUSSION_TIME				15.0f
#define TF_WEAPON_GRENADE_MIRV_BOMB_COUNT				4
#define TF_WEAPON_GRENADE_CALTROP_TIME					8.0f

#define TF_WEAPON_PIPEBOMB_WORLD_COUNT					15
#define TF_WEAPON_PIPEBOMB_COUNT						8
#define TF_WEAPON_PIPEBOMB_INTERVAL						0.6f

#define TF_WEAPON_ROCKET_INTERVAL						0.8f

#define TF_WEAPON_FLAMETHROWER_INTERVAL					0.15f
#define TF_WEAPON_FLAMETHROWER_ROCKET_INTERVAL			0.8f

#define TF_WEAPON_ZOOM_FOV								20

enum
{
	TF_WEAPON_NONE = 0,
	TF_WEAPON_BAT,
	TF_WEAPON_BOTTLE, 
	TF_WEAPON_FIREAXE,
	TF_WEAPON_CLUB,
	TF_WEAPON_CROWBAR,
	TF_WEAPON_KNIFE,
	TF_WEAPON_FISTS,
	TF_WEAPON_SHOVEL,
	TF_WEAPON_WRENCH,
	TF_WEAPON_BONESAW,
	TF_WEAPON_SHOTGUN_PRIMARY,
	TF_WEAPON_SHOTGUN_SOLDIER,
	TF_WEAPON_SHOTGUN_HWG,
	TF_WEAPON_SHOTGUN_PYRO,
	TF_WEAPON_SCATTERGUN,
	TF_WEAPON_SNIPERRIFLE,
	TF_WEAPON_MINIGUN,
	TF_WEAPON_SMG,
	TF_WEAPON_SYRINGEGUN_MEDIC,
	TF_WEAPON_TRANQ,
	TF_WEAPON_ROCKETLAUNCHER,
	TF_WEAPON_GRENADELAUNCHER,
	TF_WEAPON_PIPEBOMBLAUNCHER,
	TF_WEAPON_FLAMETHROWER,
	TF_WEAPON_GRENADE_NORMAL,
	TF_WEAPON_GRENADE_CONCUSSION,
	TF_WEAPON_GRENADE_NAIL,
	TF_WEAPON_GRENADE_MIRV,
	TF_WEAPON_GRENADE_MIRV_DEMOMAN,
	TF_WEAPON_GRENADE_NAPALM,
	TF_WEAPON_GRENADE_GAS,
	TF_WEAPON_GRENADE_EMP,
	TF_WEAPON_GRENADE_CALTROP,
	TF_WEAPON_GRENADE_PIPEBOMB,
	TF_WEAPON_GRENADE_SMOKE_BOMB,
	TF_WEAPON_GRENADE_HEAL,
	TF_WEAPON_PISTOL,
	TF_WEAPON_PISTOL_SCOUT,
	TF_WEAPON_REVOLVER,
	TF_WEAPON_NAILGUN,
	TF_WEAPON_PDA,
	TF_WEAPON_PDA_ENGINEER_BUILD,
	TF_WEAPON_PDA_ENGINEER_DESTROY,
	TF_WEAPON_PDA_SPY,
	TF_WEAPON_BUILDER,
	TF_WEAPON_MEDIGUN,
	TF_WEAPON_GRENADE_MIRVBOMB,
	TF_WEAPON_FLAMETHROWER_ROCKET,
	TF_WEAPON_GRENADE_DEMOMAN,
	TF_WEAPON_SENTRY_BULLET,
	TF_WEAPON_SENTRY_ROCKET,
	TF_WEAPON_DISPENSER,
	TF_WEAPON_INVIS,

	TF_WEAPON_COUNT
};

extern const char *g_aWeaponNames[];
extern int g_aWeaponDamageTypes[];

int GetWeaponId( const char *pszWeaponName );
#ifdef GAME_DLL
int GetWeaponFromDamage( const CTakeDamageInfo &info );
#endif
int GetBuildableId( const char *pszBuildableName );
const char *WeaponIdToAlias( int iWeapon );
const char *WeaponIdToClassname( int iWeapon );

enum
{
	TF_PROJECTILE_NONE,
	TF_PROJECTILE_BULLET,
	TF_PROJECTILE_ROCKET,
	TF_PROJECTILE_PIPEBOMB,
	TF_PROJECTILE_PIPEBOMB_REMOTE,
	TF_PROJECTILE_SYRINGE,

	TF_NUM_PROJECTILES
};

extern const char *g_szProjectileNames[];

//-----------------------------------------------------------------------------
// Attributes.
//-----------------------------------------------------------------------------
#define TF_PLAYER_VIEW_OFFSET	Vector( 0, 0, 64.0 ) //--> see GetViewVectors()

//-----------------------------------------------------------------------------
// TF Player Condition.
//-----------------------------------------------------------------------------

// Burning
#define TF_BURNING_FREQUENCY		0.5f
#define TF_BURNING_FLAME_LIFE		10.0
#define TF_BURNING_FLAME_LIFE_PYRO	0.25		// pyro only displays burning effect momentarily
#define TF_BURNING_DMG				3

// disguising
#define TF_TIME_TO_DISGUISE 2.0
#define TF_TIME_TO_SHOW_DISGUISED_FINISHED_EFFECT 5.0


#define SHOW_DISGUISE_EFFECT 1
#define TF_DISGUISE_TARGET_INDEX_NONE	( MAX_PLAYERS + 1 )
#define TF_PLAYER_INDEX_NONE			( MAX_PLAYERS + 1 )

enum
{
	TF_COND_AIMING = 0,		// Sniper aiming, Heavy minigun.
	TF_COND_ZOOMED,
	TF_COND_DISGUISING,
	TF_COND_DISGUISED,
	TF_COND_STEALTHED,
	TF_COND_INVULNERABLE,
	TF_COND_TELEPORTED,
	TF_COND_TAUNTING,
	TF_COND_INVULNERABLE_WEARINGOFF,
	TF_COND_STEALTHED_BLINK,
	TF_COND_SELECTED_TO_TELEPORT,

	// The following conditions all expire faster when the player is being healed
	// If you add a new condition that shouldn't have this behavior, add it before this section.
	TF_COND_HEALTH_BUFF,
	TF_COND_BURNING,

	// Add new conditions that should be affected by healing here

	TF_COND_LAST
};

//-----------------------------------------------------------------------------
// TF Player State.
//-----------------------------------------------------------------------------
enum 
{
	TF_STATE_ACTIVE = 0,		// Happily running around in the game.
	TF_STATE_WELCOME,			// First entering the server (shows level intro screen).
	TF_STATE_OBSERVER,			// Game observer mode.
	TF_STATE_DYING,				// Player is dying.
	TF_STATE_COUNT
};

//-----------------------------------------------------------------------------
// TF FlagInfo State.
//-----------------------------------------------------------------------------
#define TF_FLAGINFO_NONE		0
#define TF_FLAGINFO_STOLEN		(1<<0)
#define TF_FLAGINFO_DROPPED		(1<<1)

enum {
	TF_FLAGEVENT_PICKUP = 1,
	TF_FLAGEVENT_CAPTURE,
	TF_FLAGEVENT_DEFEND,
	TF_FLAGEVENT_DROPPED
};

//-----------------------------------------------------------------------------
// Class data
//-----------------------------------------------------------------------------
#define TF_MEDIC_REGEN_TIME			1.0		// Number of seconds between each regen.
#define TF_MEDIC_REGEN_AMOUNT		1 		// Amount of health regenerated each regen.

//-----------------------------------------------------------------------------
// Assist-damage constants
//-----------------------------------------------------------------------------
#define TF_TIME_ASSIST_KILL				3.0f	// Time window for a recent damager to get credit for an assist for a kill
#define TF_TIME_SUICIDE_KILL_CREDIT		10.0f	// Time window for a recent damager to get credit for a kill if target suicides

//-----------------------------------------------------------------------------
// Domination/nemesis constants
//-----------------------------------------------------------------------------
#define TF_KILLS_DOMINATION				4		// # of unanswered kills to dominate another player

//-----------------------------------------------------------------------------
// TF Hints
//-----------------------------------------------------------------------------
enum
{
	HINT_FRIEND_SEEN = 0,				// #Hint_spotted_a_friend
	HINT_ENEMY_SEEN,					// #Hint_spotted_an_enemy
	HINT_ENEMY_KILLED,					// #Hint_killing_enemies_is_good
	HINT_AMMO_EXHAUSTED,				// #Hint_out_of_ammo
	HINT_TURN_OFF_HINTS,				// #Hint_turn_off_hints
	HINT_PICKUP_AMMO,					// #Hint_pickup_ammo
	HINT_CANNOT_TELE_WITH_FLAG,			// #Hint_Cannot_Teleport_With_Flag
	HINT_CANNOT_CLOAK_WITH_FLAG,		// #Hint_Cannot_Cloak_With_Flag
	HINT_CANNOT_DISGUISE_WITH_FLAG,		// #Hint_Cannot_Disguise_With_Flag
	HINT_CANNOT_ATTACK_WHILE_CLOAKED,	// #Hint_Cannot_Attack_While_Cloaked
	HINT_CLASSMENU,						// #Hint_ClassMenu

	// Grenades
	HINT_GREN_CALTROPS,					// #Hint_gren_caltrops
	HINT_GREN_CONCUSSION,				// #Hint_gren_concussion
	HINT_GREN_EMP,						// #Hint_gren_emp
	HINT_GREN_GAS,						// #Hint_gren_gas
	HINT_GREN_MIRV,						// #Hint_gren_mirv
	HINT_GREN_NAIL,						// #Hint_gren_nail
	HINT_GREN_NAPALM,					// #Hint_gren_napalm
	HINT_GREN_NORMAL,					// #Hint_gren_normal

	// Weapon alt-fires
	HINT_ALTFIRE_SNIPERRIFLE,			// #Hint_altfire_sniperrifle
	HINT_ALTFIRE_FLAMETHROWER,			// #Hint_altfire_flamethrower
	HINT_ALTFIRE_GRENADELAUNCHER,		// #Hint_altfire_grenadelauncher
	HINT_ALTFIRE_PIPEBOMBLAUNCHER,		// #Hint_altfire_pipebomblauncher
	HINT_ALTFIRE_ROTATE_BUILDING,		// #Hint_altfire_rotate_building

	// Class specific
	// Soldier
	HINT_SOLDIER_RPG_RELOAD,			// #Hint_Soldier_rpg_reload

	// Engineer
	HINT_ENGINEER_USE_WRENCH_ONOWN,		// "#Hint_Engineer_use_wrench_onown",
	HINT_ENGINEER_USE_WRENCH_ONOTHER,	// "#Hint_Engineer_use_wrench_onother",
	HINT_ENGINEER_USE_WRENCH_FRIEND,	// "#Hint_Engineer_use_wrench_onfriend",
	HINT_ENGINEER_BUILD_SENTRYGUN,		// "#Hint_Engineer_build_sentrygun"
	HINT_ENGINEER_BUILD_DISPENSER,		// "#Hint_Engineer_build_dispenser"
	HINT_ENGINEER_BUILD_TELEPORTERS,	// "#Hint_Engineer_build_teleporters"
	HINT_ENGINEER_PICKUP_METAL,			// "#Hint_Engineer_pickup_metal"
	HINT_ENGINEER_REPAIR_OBJECT,		// "#Hint_Engineer_repair_object"
	HINT_ENGINEER_METAL_TO_UPGRADE,		// "#Hint_Engineer_metal_to_upgrade"
	HINT_ENGINEER_UPGRADE_SENTRYGUN,	// "#Hint_Engineer_upgrade_sentrygun"

	HINT_OBJECT_HAS_SAPPER,				// "#Hint_object_has_sapper"

	HINT_OBJECT_YOUR_OBJECT_SAPPED,		// "#Hint_object_your_object_sapped"
	HINT_OBJECT_ENEMY_USING_DISPENSER,	// "#Hint_enemy_using_dispenser"
	HINT_OBJECT_ENEMY_USING_TP_ENTRANCE,	// "#Hint_enemy_using_tp_entrance"
	HINT_OBJECT_ENEMY_USING_TP_EXIT,	// "#Hint_enemy_using_tp_exit"

	NUM_HINTS
};
extern const char *g_pszHintMessages[];



/*======================*/
//      Menu stuff      //
/*======================*/

#define MENU_DEFAULT				1
#define MENU_TEAM 					2
#define MENU_CLASS 					3
#define MENU_MAPBRIEFING			4
#define MENU_INTRO 					5
#define MENU_CLASSHELP				6
#define MENU_CLASSHELP2 			7
#define MENU_REPEATHELP 			8

#define MENU_SPECHELP				9


#define MENU_SPY					12
#define MENU_SPY_SKIN				13
#define MENU_SPY_COLOR				14
#define MENU_ENGINEER				15
#define MENU_ENGINEER_FIX_DISPENSER	16
#define MENU_ENGINEER_FIX_SENTRYGUN	17
#define MENU_ENGINEER_FIX_MORTAR	18
#define MENU_DISPENSER				19
#define MENU_CLASS_CHANGE			20
#define MENU_TEAM_CHANGE			21

#define MENU_REFRESH_RATE 			25

#define MENU_VOICETWEAK				50

// Additional classes
// NOTE: adding them onto the Class_T's in baseentity.h is cheesy, but so is
// having an #ifdef for each mod in baseentity.h.
#define CLASS_TFGOAL				((Class_T)NUM_AI_CLASSES)
#define CLASS_TFGOAL_TIMER			((Class_T)(NUM_AI_CLASSES+1))
#define CLASS_TFGOAL_ITEM			((Class_T)(NUM_AI_CLASSES+2))
#define CLASS_TFSPAWN				((Class_T)(NUM_AI_CLASSES+3))
#define CLASS_MACHINE				((Class_T)(NUM_AI_CLASSES+4))

// TeamFortress State Flags
#define TFSTATE_GRENPRIMED		0x000001 // Whether the player has a primed grenade
#define TFSTATE_RELOADING		0x000002 // Whether the player is reloading
#define TFSTATE_ALTKILL			0x000004 // #TRUE if killed with a weapon not in self.weapon: NOT USED ANYMORE
#define TFSTATE_RANDOMPC		0x000008 // Whether Playerclass is random, new one each respawn
#define TFSTATE_INFECTED		0x000010 // set when player is infected by the bioweapon
#define TFSTATE_INVINCIBLE		0x000020 // Player has permanent Invincibility (Usually by GoalItem)
#define TFSTATE_INVISIBLE		0x000040 // Player has permanent Invisibility (Usually by GoalItem)
#define TFSTATE_QUAD			0x000080 // Player has permanent Quad Damage (Usually by GoalItem)
#define TFSTATE_RADSUIT			0x000100 // Player has permanent Radsuit (Usually by GoalItem)
#define TFSTATE_BURNING			0x000200 // Is on fire
#define TFSTATE_GRENTHROWING	0x000400  // is throwing a grenade
#define TFSTATE_AIMING			0x000800  // is using the laser sight
#define TFSTATE_ZOOMOFF			0x001000  // doesn't want the FOV changed when zooming
#define TFSTATE_RESPAWN_READY	0x002000  // is waiting for respawn, and has pressed fire
#define TFSTATE_HALLUCINATING	0x004000  // set when player is hallucinating
#define TFSTATE_TRANQUILISED	0x008000  // set when player is tranquilised
#define TFSTATE_CANT_MOVE		0x010000  // player isn't allowed to move
#define TFSTATE_RESET_FLAMETIME 0x020000 // set when the player has to have his flames increased in health
#define TFSTATE_HIGHEST_VALUE	TFSTATE_RESET_FLAMETIME

// items
#define IT_SHOTGUN				(1<<0)
#define IT_SUPER_SHOTGUN		(1<<1) 
#define IT_NAILGUN				(1<<2) 
#define IT_SUPER_NAILGUN		(1<<3) 
#define IT_GRENADE_LAUNCHER		(1<<4) 
#define IT_ROCKET_LAUNCHER		(1<<5) 
#define IT_LIGHTNING			(1<<6) 
#define IT_EXTRA_WEAPON			(1<<7) 

#define IT_SHELLS				(1<<8) 
#define IT_BULLETS				(1<<9) 
#define IT_ROCKETS				(1<<10) 
#define IT_CELLS				(1<<11) 
#define IT_AXE					(1<<12) 

#define IT_ARMOR1				(1<<13) 
#define IT_ARMOR2				(1<<14) 
#define IT_ARMOR3				(1<<15) 
#define IT_SUPERHEALTH			(1<<16) 

#define IT_KEY1					(1<<17) 
#define IT_KEY2					(1<<18) 

#define IT_INVISIBILITY			(1<<19) 
#define IT_INVULNERABILITY		(1<<20) 
#define IT_SUIT					(1<<21)
#define IT_QUAD					(1<<22) 
#define IT_HOOK					(1<<23)

#define IT_KEY3					(1<<24)	// Stomp invisibility
#define IT_KEY4					(1<<25)	// Stomp invulnerability
#define IT_LAST_ITEM			IT_KEY4

/*==================================================*/
/* New Weapon Related Defines		                */
/*==================================================*/

// Medikit
#define WEAP_MEDIKIT_OVERHEAL 50 // Amount of superhealth over max_health the medikit will dispense
#define WEAP_MEDIKIT_HEAL	200  // Amount medikit heals per hit

//--------------
// TF Specific damage flags
//--------------
//#define DMG_UNUSED					(DMG_LASTGENERICFLAG<<2)
// We can't add anymore dmg flags, because we'd be over the 32 bit limit.
// So lets re-use some of the old dmg flags in TF
#define DMG_USE_HITLOCATIONS	(DMG_AIRBOAT)
#define DMG_HALF_FALLOFF		(DMG_RADIATION)
#define DMG_CRITICAL			(DMG_ACID)
#define DMG_RADIUS_MAX			(DMG_ENERGYBEAM)
#define DMG_IGNITE				(DMG_PLASMA)
#define DMG_USEDISTANCEMOD		(DMG_SLOWBURN)		// NEED TO REMOVE CALTROPS
#define DMG_NOCLOSEDISTANCEMOD	(DMG_POISON)

#define TF_DMG_SENTINEL_VALUE	0x00512 //0xFFFFFFFF FIXME: wtf?

// This can only ever be used on a TakeHealth call, since it re-uses a dmg flag that means something else
#define DMG_IGNORE_MAXHEALTH	(DMG_BULLET)

// Special Damage types
enum
{
	TF_DMG_CUSTOM_NONE = 0,
	TF_DMG_CUSTOM_HEADSHOT,
	TF_DMG_CUSTOM_BACKSTAB,
	TF_DMG_CUSTOM_BURNING,
	TF_DMG_WRENCH_FIX,
	TF_DMG_CUSTOM_MINIGUN,
	TF_DMG_CUSTOM_SUICIDE,
};

enum
{
	TF_COLLISIONGROUP_GRENADES = LAST_SHARED_COLLISION_GROUP,
	TFCOLLISION_GROUP_OBJECT,
	TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT,
	TFCOLLISION_GROUP_COMBATOBJECT,
	TFCOLLISION_GROUP_ROCKETS,		// Solid to players, but not player movement. ensures touch calls are originating from rocket
	TFCOLLISION_GROUP_RESPAWNROOMS,
};

//-----------------
// TF Objects Info
//-----------------

#define SENTRYGUN_UPGRADE_COST			130
#define SENTRYGUN_UPGRADE_METAL			200
#define SENTRYGUN_EYE_OFFSET_LEVEL_1	Vector( 0, 0, 32 )
#define SENTRYGUN_EYE_OFFSET_LEVEL_2	Vector( 0, 0, 40 )
#define SENTRYGUN_EYE_OFFSET_LEVEL_3	Vector( 0, 0, 46 )
#define SENTRYGUN_MAX_SHELLS_1			100
#define SENTRYGUN_MAX_SHELLS_2			120
#define SENTRYGUN_MAX_SHELLS_3			144
#define SENTRYGUN_MAX_ROCKETS			20

// Dispenser's maximum carrying capability
#define DISPENSER_MAX_METAL_AMMO		400
#define	MAX_DISPENSER_HEALING_TARGETS	32

//--------------------------------------------------------------------------
// OBJECTS
//--------------------------------------------------------------------------
enum
{
	OBJ_DISPENSER=0,
	OBJ_TELEPORTER_ENTRANCE,
	OBJ_TELEPORTER_EXIT,
	OBJ_SENTRYGUN,

	// Attachment Objects
	OBJ_ATTACHMENT_SAPPER,

	// If you add a new object, you need to add it to the g_ObjectInfos array 
	// in tf_shareddefs.cpp, and add it's data to the scripts/object.txt

	OBJ_LAST,
};

// Warning levels for buildings in the building hud, in priority order
typedef enum
{
	BUILDING_HUD_ALERT_NONE = 0,
	BUILDING_HUD_ALERT_LOW_AMMO,
	BUILDING_HUD_ALERT_LOW_HEALTH,
	BUILDING_HUD_ALERT_VERY_LOW_AMMO,
	BUILDING_HUD_ALERT_VERY_LOW_HEALTH,
	BUILDING_HUD_ALERT_SAPPER,	

	MAX_BUILDING_HUD_ALERT_LEVEL
} BuildingHudAlert_t;

typedef enum
{
	BUILDING_DAMAGE_LEVEL_NONE = 0,		// 100%
	BUILDING_DAMAGE_LEVEL_LIGHT,		// 75% - 99%
	BUILDING_DAMAGE_LEVEL_MEDIUM,		// 50% - 76%
	BUILDING_DAMAGE_LEVEL_HEAVY,		// 25% - 49%	
	BUILDING_DAMAGE_LEVEL_CRITICAL,		// 0% - 24%

	MAX_BUILDING_DAMAGE_LEVEL
} BuildingDamageLevel_t;

//--------------
// Scoring
//--------------

#define TF_SCORE_KILL							1
#define TF_SCORE_DEATH							0
#define TF_SCORE_CAPTURE						2
#define TF_SCORE_DEFEND							1
#define TF_SCORE_DESTROY_BUILDING				1
#define TF_SCORE_HEADSHOT						1
#define TF_SCORE_BACKSTAB						1
#define TF_SCORE_INVULN							1
#define TF_SCORE_REVENGE						1
#define TF_SCORE_KILL_ASSISTS_PER_POINT			2
#define TF_SCORE_TELEPORTS_PER_POINT			2	
#define TF_SCORE_HEAL_HEALTHUNITS_PER_POINT		600

//-------------------------
// Shared Teleporter State
//-------------------------
enum
{
	TELEPORTER_STATE_BUILDING = 0,				// Building, not active yet
	TELEPORTER_STATE_IDLE,						// Does not have a matching teleporter yet
	TELEPORTER_STATE_READY,						// Found match, charged and ready
	TELEPORTER_STATE_SENDING,					// Teleporting a player away
	TELEPORTER_STATE_RECEIVING,					
	TELEPORTER_STATE_RECEIVING_RELEASE,
	TELEPORTER_STATE_RECHARGING,				// Waiting for recharge
};

#define TELEPORTER_TYPE_ENTRANCE	0
#define TELEPORTER_TYPE_EXIT		1

#define TELEPORTER_RECHARGE_TIME				10		// seconds to recharge

//-------------------------
// Shared Sentry State
//-------------------------
enum
{
	SENTRY_STATE_INACTIVE = 0,
	SENTRY_STATE_SEARCHING,
	SENTRY_STATE_ATTACKING,
	SENTRY_STATE_UPGRADING,

	SENTRY_NUM_STATES,
};

//--------------------------------------------------------------------------
// OBJECT FLAGS
//--------------------------------------------------------------------------
enum
{
	OF_ALLOW_REPEAT_PLACEMENT				= 0x01,
	OF_MUST_BE_BUILT_ON_ATTACHMENT			= 0x02,

	OF_BIT_COUNT	= 2
};

//--------------------------------------------------------------------------
// Builder "weapon" states
//--------------------------------------------------------------------------
enum 
{
	BS_IDLE = 0,
	BS_SELECTING,
	BS_PLACING,
	BS_PLACING_INVALID
};


//--------------------------------------------------------------------------
// Builder object id...
//--------------------------------------------------------------------------
enum
{
	BUILDER_OBJECT_BITS = 8,
	BUILDER_INVALID_OBJECT = ((1 << BUILDER_OBJECT_BITS) - 1)
};

// Analyzer state
enum
{
	AS_INACTIVE = 0,
	AS_SUBVERTING,
	AS_ANALYZING
};

// Max number of objects a team can have
#define MAX_OBJECTS_PER_PLAYER	4
//#define MAX_OBJECTS_PER_TEAM	128

// sanity check that commands send via user command are somewhat valid
#define MAX_OBJECT_SCREEN_INPUT_DISTANCE	100

//--------------------------------------------------------------------------
// BUILDING
//--------------------------------------------------------------------------
// Build checks will return one of these for a player
enum
{
	CB_CAN_BUILD,			// Player is allowed to build this object
	CB_CANNOT_BUILD,		// Player is not allowed to build this object
	CB_LIMIT_REACHED,		// Player has reached the limit of the number of these objects allowed
	CB_NEED_RESOURCES,		// Player doesn't have enough resources to build this object
	CB_NEED_ADRENALIN,		// Commando doesn't have enough adrenalin to build a rally flag
	CB_UNKNOWN_OBJECT,		// Error message, tried to build unknown object
};

// Build animation events
#define TF_OBJ_ENABLEBODYGROUP			6000
#define TF_OBJ_DISABLEBODYGROUP			6001
#define TF_OBJ_ENABLEALLBODYGROUPS		6002
#define TF_OBJ_DISABLEALLBODYGROUPS		6003
#define TF_OBJ_PLAYBUILDSOUND			6004

#define TF_AE_CIGARETTE_THROW			7000

#define OBJECT_COST_MULTIPLIER_PER_OBJECT			3
#define OBJECT_UPGRADE_COST_MULTIPLIER_PER_LEVEL	3

//--------------------------------------------------------------------------
// Powerups
//--------------------------------------------------------------------------
enum
{
	POWERUP_BOOST,		// Medic, buff station
	POWERUP_EMP,		// Technician
	POWERUP_RUSH,		// Rally flag
	POWERUP_POWER,		// Object power
	MAX_POWERUPS
};

#define	MAX_CABLE_CONNECTIONS 4

bool IsObjectAnUpgrade( int iObjectType );
bool IsObjectAVehicle( int iObjectType );
bool IsObjectADefensiveBuilding( int iObjectType );

class CHudTexture;

#define OBJECT_MAX_GIB_MODELS	9

class CObjectInfo
{
public:
	CObjectInfo( char *pObjectName );	
	~CObjectInfo();

	// This is initialized by the code and matched with a section in objects.txt
	char	*m_pObjectName;

	// This stuff all comes from objects.txt
	char	*m_pClassName;					// Code classname (in LINK_ENTITY_TO_CLASS).
	char	*m_pStatusName;					// Shows up when crosshairs are on the object.
	float	m_flBuildTime;
	int		m_nMaxObjects;					// Maximum number of objects per player
	int		m_Cost;							// Base object resource cost
	float	m_CostMultiplierPerInstance;	// Cost multiplier
	int		m_UpgradeCost;					// Base object resource cost for upgrading
	int		m_MaxUpgradeLevel;				// Max object upgrade level
	char	*m_pBuilderWeaponName;			// Names shown for each object onscreen when using the builder weapon
	char	*m_pBuilderPlacementString;		// String shown to player during placement of this object
	int		m_SelectionSlot;				// Weapon selection slots for objects
	int		m_SelectionPosition;			// Weapon selection positions for objects
	bool	m_bSolidToPlayerMovement;
	char    *m_pViewModel;					// View model to show in builder weapon for this object
	char    *m_pPlayerModel;				// World model to show attached to the player
	int		m_iDisplayPriority;				// Priority for ordering in the hud display ( higher is closer to top )
	bool	m_bVisibleInWeaponSelection;	// should show up and be selectable via the weapon selection?
	char	*m_pExplodeSound;				// gamesound to play when object explodes
	char	*m_pExplosionParticleEffect;	// particle effect to play when object explodes
	bool	m_bAutoSwitchTo;				// should we let players switch back to the builder weapon representing this?

	// HUD weapon selection menu icon ( from hud_textures.txt )
	char	*m_pIconActive;
	char	*m_pIconInactive;

	// HUD building status icon
	char	*m_pHudStatusIcon;

	// gibs
	int		m_iMetalToDropInGibs;
};

// Loads the objects.txt script.
class IBaseFileSystem;
void LoadObjectInfos( IBaseFileSystem *pFileSystem );

// Get a CObjectInfo from a TFOBJ_ define.
const CObjectInfo* GetObjectInfo( int iObject );

// Object utility funcs
bool	ClassCanBuild( int iClass, int iObjectType );
int		CalculateObjectCost( int iObjectType /*, int iNumberOfObjects, int iTeam, bool bLast = false*/ );
int		CalculateObjectUpgrade( int iObjectType, int iObjectLevel );

// Shell ejections
enum
{
	EJECTBRASS_PISTOL,
	EJECTBRASS_MINIGUN,
};

// Win panel styles
enum
{
	WINPANEL_BASIC = 0,
};

#define TF_DEATH_ANIMATION_TIME			2.0


typedef enum
{
	HUD_NOTIFY_YOUR_FLAG_TAKEN,
	HUD_NOTIFY_YOUR_FLAG_DROPPED,
	HUD_NOTIFY_YOUR_FLAG_RETURNED,
	HUD_NOTIFY_YOUR_FLAG_CAPTURED,

	HUD_NOTIFY_ENEMY_FLAG_TAKEN,
	HUD_NOTIFY_ENEMY_FLAG_DROPPED,
	HUD_NOTIFY_ENEMY_FLAG_RETURNED,
	HUD_NOTIFY_ENEMY_FLAG_CAPTURED,

	HUD_NOTIFY_TOUCHING_ENEMY_CTF_CAP,

	HUD_NOTIFY_NO_INVULN_WITH_FLAG,
	HUD_NOTIFY_NO_TELE_WITH_FLAG,

	HUD_NOTIFY_SPECIAL,

	NUM_STOCK_NOTIFICATIONS
} HudNotification_t;

#define TF_DEATH_DOMINATION				0x0001	// killer is dominating victim
#define TF_DEATH_ASSISTER_DOMINATION			0x0002	// assister is dominating victim
#define TF_DEATH_REVENGE				0x0004	// killer got revenge on victim
#define TF_DEATH_ASSISTER_REVENGE			0x0008	// assister got revenge on victim
#define TF_DEATH_FIRST_BLOOD				0x0010  // death triggered a first blood
#define TF_DEATH_FEIGN_DEATH				0x0020  // feign death
#define TF_DEATH_INTERRUPTED				0x0040	// interrupted a player doing an important game event (like capping or carrying flag)
#define TF_DEATH_GIBBED					0x0080	// player was gibbed
#define TF_DEATH_PURGATORY				0x0100	// player died while in purgatory
#define TF_DEATH_MINIBOSS				0x0200	// player killed was a miniboss
#define TF_DEATH_AUSTRALIUM				0x0400	// player killed by a Australium Weapon

#endif // TF_SHAREDDEFS_H
