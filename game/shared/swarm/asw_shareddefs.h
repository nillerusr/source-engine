#ifndef ASW_SHAREDDEFS_H
#define ASW_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

#include "const.h"

#define ASW_MAX_PLAYER_NAME_LENGTH 32
#define ASW_MAX_PLAYER_NAME_LENGTH_3D 20

enum ASW_Inventory_slot_t
{
	ASW_INVENTORY_SLOT_PRIMARY = 0,
	ASW_INVENTORY_SLOT_SECONDARY = 1,
	ASW_INVENTORY_SLOT_EXTRA,

	ASW_NUM_INVENTORY_SLOTS,
};

// Specifies whether a "Use" order utilizes an item, and if not, what kind of order it is
enum ASW_UseOrderTypes_t
{
	ASW_USE_ORDER_WITH_ITEM,	// requires an item be specified in the order message
	ASW_USE_ORDER_HACK,

	ASW_NUM_USE_ORDERS
};

#define ASW_MIN_TUMBLERS_FAST_HACK 5		// how many tumblers a computer hack must have to qualify for the fast medal and the tracking timer bar
#define ASW_MAX_ENTRIES_PER_TUMBLER 8
#define ASW_MIN_ENTRIES_PER_TUMBLER 4

bool MedalMatchesAchievement( int nMedalIndex, int nAchievementIndex );
int GetAchievementIndexForMedal( int nMedalIndex );

enum ASW_Skill_Slot
{
	ASW_SKILL_SLOT_0 = 0,
	ASW_SKILL_SLOT_1,
	ASW_SKILL_SLOT_2,
	ASW_SKILL_SLOT_3,
	ASW_SKILL_SLOT_4,
	ASW_SKILL_SLOT_SPARE,

	ASW_NUM_SKILL_SLOTS
};

#define ASW_SKILL_POINTS_PER_MISSION 2

#define ENGINEERING_AURA_RADIUS 300

#define ASW_MAX_PLAYERS 6			// currently limiting the game to 6 players, more than that feels chaotic

#define ASW_MAX_READY_PLAYERS 8		// max number of slots for ready players in the game resource

#define ASW_MAX_MARINE_RESOURCES 5
#define ASW_MAX_OBJECTIVES 25

#define ASW_PLAYER_MAX_USE_ENTS 3

#define ASW_PLAYER_VIEW_OFFSET	Vector( 0, 0, 53.5 )
#define ASW_MARINE_GUN_OFFSET_X		25.0f
#define ASW_MARINE_GUN_OFFSET_Y		4.0f
#define ASW_MARINE_GUN_OFFSET_Z		34.0f
		// was: 55.0f

#define PUSHAWAY_THINK_INTERVAL		(1.0f / 20.0f)

// Re-use the vehicle crosshair hidehud bit for our remote turrets
#define HIDEHUD_REMOTE_TURRET		( HIDEHUD_VEHICLE_CROSSHAIR )	// player is using a remote turret

// Re-use base damage bits
#define DMG_INFEST					(DMG_AIRBOAT)		// damage from parasite infestation
#define DMG_BLURPOISON				(DMG_POISON)		// damage from buzzers, blurs the player's view

#define DMG_GIB_CORPSE				( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB | DMG_INFEST )

#define ASW_MAX_MELEE_ATTACKS 64
#define ASW_MAX_PREDICTED_MELEE_EVENTS 16

#define MELEE_BUTTON IN_ALT1

enum ASW_Marine_Class
{
	MARINE_CLASS_UNDEFINED = -1,
	MARINE_CLASS_NCO = 0,
	MARINE_CLASS_SPECIAL_WEAPONS,
	MARINE_CLASS_MEDIC,
	MARINE_CLASS_TECH,

	NUM_MARINE_CLASSES
};

enum ASW_Melee_Movement_t
{
	MELEE_MOVEMENT_ANIMATION_ONLY,
	MELEE_MOVEMENT_FALLING_ONLY,
	MELEE_MOVEMENT_FULL,
};

enum ASW_Jump_Jet_t
{
	JJ_NONE = 0,
	JJ_CHARGE = 1,
	JJ_JUMP_JETS = 2,
	JJ_BLINK = 3,
};

// hack options
enum
{
	ASW_HACK_OPTION_ICON_0 = 0,
	ASW_HACK_OPTION_ICON_1,		// computers can display up to 6 customised icons
	ASW_HACK_OPTION_ICON_2,
	ASW_HACK_OPTION_ICON_3,
	ASW_HACK_OPTION_ICON_4,
	ASW_HACK_OPTION_ICON_5,
	ASW_HACK_OPTION_ICON_6,
	ASW_HACK_OPTION_OVERRIDE		// special option to launch the override hacking puzzle
};

// how many slots are in the ammo bag
#define ASW_AMMO_BAG_SLOTS 7

// use icon priorities
#define ASW_USE_PRIORITY_PICKUP 5
#define ASW_USE_PRIORITY_USE_AREA 4

#define ASW_MARINE_USE_RADIUS 100.0f

#define ASW_MARINE_HEALPERTICK_MAX 4
#define ASW_MARINE_HEALTICK_RATE 0.33f

#define ASW_FLARE_AUTOAIM_DOT 0.4f

// movement uses this axis to decide where the marine should go from his forward/sidemove
#define ASW_MOVEMENT_AXIS QAngle(0,90,0)

#define ASW_SCANNER_MAX_BLIPS 32

#define ASW_MAX_EQUIP_SLOTS	3
#define ASW_MAX_MARINE_WEAPONS ASW_MAX_EQUIP_SLOTS // an ASW marine shouldn't have more than this many weapons in its m_hWeapons

// asw light indices
enum
{
	//LIGHT_INDEX_TE_DYNAMIC = 0x10000000,
	//LIGHT_INDEX_PLAYER_BRIGHT = 0x20000000,
	//LIGHT_INDEX_MUZZLEFLASH = 0x40000000,
	ASW_LIGHT_INDEX_FLASHLIGHT = 0x60000000,
	ASW_LIGHT_INDEX_FIRES = 0x64000000,
};

#define	SWARM_DRONE_MODEL		"models/swarm/drone/Drone.mdl"
#define	SWARM_NEW_DRONE_MODEL	"models/aliens/drone/drone.mdl"
#define	SWARM_HARVESTER_MODEL	"models/swarm/harvester/Harvester.mdl"
#define	SWARM_SHIELDBUG_MODEL	"models/swarm/Shieldbug/Shieldbug.mdl"
#define	SWARM_NEW_SHIELDBUG_MODEL	"models/aliens/Shieldbug/Shieldbug.mdl"
#define SWARM_MORTARBUG_MODEL	"models/aliens/mortar/mortar.mdl"
#define	SWARM_NEW_HARVESTER_MODEL	"models/aliens/harvester/harvester.mdl"

#define ASW_BLIP_SPEECH_INTERVAL 10.0f

// when time scale drops below this, the stim cam will appear
#define ASW_STIM_CAM_TIME 0.6f

// asw profiling
#define VPROF_BUDGETGROUP_ASW_CLIENT			_T("Swarm Client")

// how many marine profiles there are (used by the campaign save system)
#define ASW_NUM_MARINE_PROFILES 8

// !!NOTE!! - these numbers are hardcoded into engine's Host_cmd.cpp too :(
#define ASW_MAX_PLAYERS_PER_SAVE 10
#define ASW_MAX_MISSIONS_PER_CAMPAIGN 32
#define ASW_CURRENT_SAVE_VERSION 1

// must follow from last HUD_ entry in shareddefs.h
#define ASW_HUD_PRINTTALKANDCONSOLE 5

enum ASW_Orders
{
	ASW_ORDER_HOLD_POSITION,
	ASW_ORDER_FOLLOW,
	ASW_ORDER_MOVE_TO,
	ASW_ORDER_USE_OFFHAND_ITEM,
};

// Map markers
#define ASW_NUM_MAP_MARKS 32

enum MapMarkType
{
	MMT_BRACKETS,
	MMT_CROSS
};

struct ObjectiveMapMark
{
	MapMarkType type;
	bool bComplete;
	bool bEnabled;
	int x, y, w, h;
};

// impulse commands used to send hack tumbler commands
#define ASW_IMPULSE_REVERSE_TUMBLER_1 150
#define ASW_IMPULSE_REVERSE_TUMBLER_2 151
#define ASW_IMPULSE_REVERSE_TUMBLER_3 152
#define ASW_IMPULSE_REVERSE_TUMBLER_4 153
#define ASW_IMPULSE_REVERSE_TUMBLER_5 154
#define ASW_IMPULSE_REVERSE_TUMBLER_6 155
#define ASW_IMPULSE_REVERSE_TUMBLER_7 156
#define ASW_IMPULSE_REVERSE_TUMBLER_8 157

// max number of tumblers in the computer hack
#define ASW_HACK_COMPUTER_MAX_TUMBLERS 8

// egg gib flags
#define EGG_FLAG_OPEN					0x0001
#define EGG_FLAG_HATCH					0x0002
#define EGG_FLAG_DIE					0x0004
#define EGG_FLAG_GRUBSACK_DIE			0x0008

// door healths and break fractions
#define ASW_DOOR_REINFORCED_HEALTH 2400
#define ASW_DOOR_NORMAL_HEALTH 1800
#define ASW_DOOR_PARTIAL_DENT_HEALTH 0.66f
#define ASW_DOOR_COMPLETE_DENT_HEALTH 0.33f

// DamageCustom flags for CTakeDamageInfo
#define DAMAGE_FLAG_WEAKSPOT			0x0001
#define DAMAGE_FLAG_CRITICAL			0x0002
#define DAMAGE_FLAG_T75					0x0004
#define DAMAGE_FLAG_NO_FALLOFF			0x0008
#define DAMAGE_FLAG_HALF_FALLOFF		0x0010

enum {
	ASW_VOTE_NONE = 0,
	ASW_VOTE_SAVED_CAMPAIGN,
	ASW_VOTE_CHANGE_MISSION,
};

#define ASW_GIBFLAG_ON_FIRE 0x001

#define ASW_NUM_FIRE_EMITTERS 4

enum
{
	ASW_COLLISION_GROUP_GRUBS = LAST_SHARED_COLLISION_GROUP,
	ASW_COLLISION_GROUP_PARASITE,
	ASW_COLLISION_GROUP_ALIEN,

	// HOUNDEYE = grubs
	ASW_COLLISION_GROUP_MARINE_POSITION_PREDICTION,		// asw
	ASW_COLLISION_GROUP_SHOTGUN_PELLET,		// asw	
	ASW_COLLISION_GROUP_EGG,		// asw (alien eggs)	
	ASW_COLLISION_GROUP_BLOCK_DRONES,		// asw
	ASW_COLLISION_GROUP_BIG_ALIEN,			// asw (shieldbug, queen)
	ASW_COLLISION_GROUP_BUZZER,
	ASW_COLLISION_GROUP_BODY_SNATCHER,				// asw (shieldbug, queen)
	ASW_COLLISION_GROUP_GRENADES,					// doesn't collide with marines
	ASW_COLLISION_GROUP_NPC_GRENADES,				// doesn't collide with npcs
	ASW_COLLISION_GROUP_BLOB_TINY,
	ASW_COLLISION_GROUP_ALIEN_MISSILE,				// collides with non-alien NPCs
	ASW_COLLISION_GROUP_PLAYER_MISSILE,				// collides with aliens, not players
	ASW_COLLISION_GROUP_SENTRY,						// collides with aliens
	ASW_COLLISION_GROUP_SENTRY_PROJECTILE,			// collides with aliens, marines and doors - nothing else (this is used for the sentry particles)

	ASW_COLLISION_GROUP_BLOCK_ALIENS,				// asw
	ASW_COLLISION_GROUP_IGNORE_NPCS,		// asw (fire walls spreading, collides with everything but aliens, marines) NOTE: Has to be after any NPC collision groups
	ASW_COLLISION_GROUP_FLAMER_PELLETS,		// the pellets that the flamer shoots.  Doesn not collide with small aliens or marines, DOES collide with doors and shieldbugs
	ASW_COLLISION_GROUP_EXTINGUISHER_PELLETS,	// the pellets that the extinguisher shoots. Hits lots of things, but not other weapons
	ASW_COLLISION_GROUP_PASSABLE,		// asw (stuff you can walk through) NOTE: Has to be LAST!

	HL2COLLISION_GROUP_COMBINE_BALL_NPC,
};


enum FailAdviceMessage
{
	ASW_FAIL_ADVICE_DEFAULT = 0,
	ASW_FAIL_ADVICE_LOW_AMMO,
	ASW_FAIL_ADVICE_INFESTED_LOTS,
	ASW_FAIL_ADVICE_INFESTED,
	ASW_FAIL_ADVICE_SWARMED,
	ASW_FAIL_ADVICE_FRIENDLY_FIRE,
	ASW_FAIL_ADVICE_HACKER_DAMAGED,
	ASW_FAIL_ADVICE_WASTED_HEALS,
	ASW_FAIL_ADVICE_IGNORED_HEALING,
	ASW_FAIL_ADVICE_IGNORED_ADRENALINE,
	ASW_FAIL_ADVICE_IGNORED_SECONDARY,
	ASW_FAIL_ADVICE_IGNORED_WELDER,
	ASW_FAIL_ADVICE_IGNORED_LIGHTING,	// TODO
	ASW_FAIL_ADVICE_SLOW_PROGRESSION,
	ASW_FAIL_ADVICE_SHIELD_BUG,
	ASW_FAIL_ADVICE_DIED_ALONE,
	ASW_FAIL_ADVICE_NO_MEDICS,

	ASW_FAIL_ADVICE_TOTAL
};

enum PowerUpTypes
{
	POWERUP_TYPE_FREEZE_BULLETS = 0,
	POWERUP_TYPE_FIRE_BULLETS,
	POWERUP_TYPE_ELECTRIC_BULLETS,
	//POWERUP_TYPE_CHEMICAL_BULLETS,
	//POWERUP_TYPE_EXPLOSIVE_BULLETS,
	// all gun powerups that get removed on reload should go above here
	POWERUP_TYPE_INCREASED_SPEED,

	NUM_POWERUP_TYPES
};

#define	ASW_MOVEMENT_WALK_SPEED 150
#define	ASW_MOVEMENT_NORM_SPEED 190

#define HL2COLLISION_GROUP_FIRST_NPC ASW_COLLISION_GROUP_GRUBS
#define HL2COLLISION_GROUP_LAST_NPC ASW_COLLISION_GROUP_ALIEN

bool IsAlienCollisionGroup(	int iCollisionGroup );
#define IN_ASW_STOP		(1 << 26)	// asw: this button will stop the marine's x/y motion immediately (used to hold him still while he plays stationary animations, also may be need for clientside avoidance)

bool IsAlienClass( Class_T npc_class );
bool IsDamagingWeaponClass( Class_T entity_class );
bool IsWeaponClass( Class_T entity_class );
bool IsBulletBasedWeaponClass( Class_T entity_class );
bool IsSentryClass( Class_T entity_class );
bool CanMarineGetStuckOnProp( const char *szModelName );
//-----------------------------------------------------------------------------
// Alien projectiles
//-----------------------------------------------------------------------------

class CASW_AlienShot
{
public:
	CASW_AlienShot()
	{
		m_flSize = 0.0f;
		m_flDamage_direct = 0.0f;
		m_flDamage_splash = 0.0f;
		m_flSeek_strength = 0.0f;
		m_flGravity = 0.0f;
		m_flFuse = 0.0f;
		m_flBounce = 0.0f;
		m_bShootable = false;
	}

	CASW_AlienShot( const CASW_AlienShot &shot )
	{
		m_strName				= shot.m_strName;
		m_flSize				= shot.m_flSize;
		m_flDamage_direct		= shot.m_flDamage_direct;
		m_flDamage_splash		= shot.m_flDamage_splash;
		m_flSeek_strength		= shot.m_flSeek_strength;
		m_flGravity				= shot.m_flGravity;
		m_flFuse				= shot.m_flFuse;
		m_flBounce				= shot.m_flBounce;
		m_bShootable			= shot.m_bShootable;
		m_strModel				= shot.m_strModel;
		m_strSound_spawn		= shot.m_strSound_spawn;
		m_strSound_hitNPC		= shot.m_strSound_hitNPC;
		m_strSound_hitWorld 	= shot.m_strSound_hitWorld;
		m_strParticles_trail	= shot.m_strParticles_trail;
		m_strParticles_hit		= shot.m_strParticles_hit;
	}

public:
	CUtlString	m_strName;
	float		m_flSize;
	float		m_flDamage_direct;
	float		m_flDamage_splash;		//FIXME: not implemented yet
	float		m_flSeek_strength;		//FIXME: not implemented yet
	float		m_flGravity;
	float		m_flFuse;
	float		m_flBounce;
	bool		m_bShootable;
	CUtlString	m_strModel;
	CUtlString	m_strSound_spawn;
	CUtlString	m_strSound_hitNPC;
	CUtlString	m_strSound_hitWorld;
	CUtlString	m_strParticles_trail;
	CUtlString	m_strParticles_hit;
};

class CASW_AlienVolleyRound
{
public:
	CASW_AlienVolleyRound()
	{
		m_flTime = 0.0f;
		m_flStartAngle = 0.0f;
		m_flEndAngle = 0.0f;
		m_nNumShots = 0;
		m_flShotDelay = 0.0f;
		m_flSpeed = 0.0f;
		m_flHorizontalOffset = 0.0f;
	}

	CASW_AlienVolleyRound( const CASW_AlienVolleyRound &volley )
	{
		m_flTime				= volley.m_flTime;
		m_nShot_type			= volley.m_nShot_type;
		m_flStartAngle			= volley.m_flStartAngle;
		m_flEndAngle			= volley.m_flEndAngle;
		m_nNumShots				= volley.m_nNumShots;
		m_flShotDelay			= volley.m_flShotDelay;
		m_flSpeed				= volley.m_flSpeed;
		m_flHorizontalOffset	= volley.m_flHorizontalOffset;
	}

public:
	float		m_flTime;
	int			m_nShot_type;
	float		m_flStartAngle;
	float		m_flEndAngle;
	int			m_nNumShots;
	float		m_flShotDelay;
	float		m_flSpeed;
	float		m_flHorizontalOffset;
};

class CASW_AlienVolley
{
public:
	CASW_AlienVolley() {}
	CASW_AlienVolley( const CASW_AlienVolley &volley )
	{
		m_strName = volley.m_strName;
		m_rounds.SetCount( volley.m_rounds.Count() );
		for( int i = 0; i < m_rounds.Count(); i++ )
		{
			m_rounds[ i ] = volley.m_rounds[ i ];
		}
	}

public:
	CUtlString	m_strName;
	CUtlVector<CASW_AlienVolleyRound>	m_rounds;
};

// For CLASSIFY
enum
{
	// Alien Swarm AI
	// Be sure to add appropriate checks in IsAlienClass() in asw_shareddefs.cpp
	//		and s_alienClasses in asw_gamerules.cpp

	//   CLASS_BLOB     = tiny blob

	//////////////////////////////////////////////////////////////////////////
	// IMPORTANT: New classes need to be added to the end of this list otherwise
	// it throws off the stats reporting page (one build has CLASS_ASW_RIFLE = 60,
	// the next has ASW_CLASS_RIFLE = 61, bad juju) Thanks ^_^
	//////////////////////////////////////////////////////////////////////////

	CLASS_ASW_DRONE = LAST_SHARED_ENTITY_CLASS,
	CLASS_ASW_SHIELDBUG,
	CLASS_ASW_PARASITE,
	CLASS_ASW_UNKNOWN,
	CLASS_ASW_HARVESTER,
	CLASS_ASW_GRUB,
	CLASS_ASW_HYDRA,
	CLASS_ASW_BLOB,
	CLASS_ASW_SENTRY_GUN,
	CLASS_ASW_BUTTON_PANEL,
	CLASS_ASW_COMPUTER_AREA,
	CLASS_ASW_DOOR_AREA,
	CLASS_ASW_DOOR,
	CLASS_ASW_MARINE,
	CLASS_ASW_COLONIST,
	CLASS_ASW_BOOMER,
	CLASS_ASW_BOOMERMINI,
	CLASS_ASW_MEATBUG,
	CLASS_ASW_QUEEN,
	CLASS_ASW_BUZZER,
	CLASS_ASW_QUEEN_DIVER,
	CLASS_ASW_QUEEN_GRABBER,
	CLASS_ASW_ALIEN_GOO,
	CLASS_ASW_T75,

	CLASS_ASW_SHAMAN,
	CLASS_ASW_EGG,
	CLASS_ASW_RUNNER,
	CLASS_ASW_MORTAR_BUG,
	CLASS_ASW_RANGER,
	CLASS_ASW_FLOCK,
	CLASS_ASW_SPAWNER,
	CLASS_ASW_HOLDOUT_SPAWNER,
	CLASS_ASW_SPAWN_GROUP,
	CLASS_ASW_NIGHT_VISION,
	CLASS_ASW_SNIPER_RIFLE,
	CLASS_ASW_STATUE,
	CLASS_ASW_BAIT,

	CLASS_ASW_FRIENDLY_ALIEN,

	CLASS_ASW_RIFLE,
	CLASS_ASW_MINIGUN,
	CLASS_ASW_PDW,
	CLASS_ASW_FLECHETTE,
	CLASS_ASW_FIRE_EXTINGUISHER,
	CLASS_ASW_WELDER,
	CLASS_ASW_TESLA_TRAP,
	CLASS_ASW_STIM,
	CLASS_ASW_SHOTGUN,
	CLASS_ASW_SENTRY_FLAMER,
	CLASS_ASW_SENTRY_CANNON,
	CLASS_ASW_SENTRY_FREEZE,
	CLASS_ASW_RICOCHET,
	CLASS_ASW_RAILGUN,
	CLASS_ASW_PRIFLE,
	CLASS_ASW_PISTOL,
	CLASS_ASW_MINING_LASER,
	CLASS_ASW_MINES,
	CLASS_ASW_MEDKIT,
	CLASS_ASW_MEDICAL_SATCHEL,
	CLASS_ASW_LASER_MINES,
	CLASS_ASW_HORNET_BARRAGE,
	CLASS_ASW_HEALGRENADE,
	CLASS_ASW_GRENADES,
	CLASS_ASW_GRENADE_LAUNCHER,
	CLASS_ASW_FREEZE_GRENADES,
	CLASS_ASW_FLASHLIGHT,
	CLASS_ASW_FLARES,
	CLASS_ASW_FLAMER,
	CLASS_ASW_ELECTRIFIED_ARMOR,
	CLASS_ASW_CHAINSAW,
	CLASS_ASW_BUFF_GRENADE,
	CLASS_ASW_AUTOGUN,
	CLASS_ASW_ASSAULT_SHOTGUN,
	CLASS_ASW_AMMO_BAG,
	CLASS_ASW_AMMO_RIFLE,
	CLASS_ASW_AMMO_AUTOGUN,
	CLASS_ASW_AMMO_SHOTGUN,
	CLASS_ASW_AMMO_ASSAULT_SHOTGUN,
	CLASS_ASW_AMMO_FLAMER,
	CLASS_ASW_AMMO_PISTOL,
	CLASS_ASW_AMMO_MINING_LASER,
	CLASS_ASW_AMMO_RAILGUN,
	CLASS_ASW_AMMO_CHAINSAW,
	CLASS_ASW_AMMO_PDW,
	CLASS_ASW_RIFLE_GRENADE,
	CLASS_ASW_GRENADE_VINDICATOR,
	CLASS_ASW_GRENADE_CLUSER,
	CLASS_ASW_FLAMER_PROJECTILE,
	CLASS_ASW_BURNING,
	CLASS_ASW_GRENADE_PRIFLE,
	CLASS_ASW_TESLA_GUN,
	CLASS_ASW_HEAL_GUN,
	CLASS_ASW_SENTRY_GUN_CASE,
	CLASS_ASW_SENTRY_CANNON_CASE,
	CLASS_ASW_SENTRY_FLAMER_CASE,
	CLASS_ASW_SENTRY_FREEZE_CASE,
	CLASS_ASW_NORMAL_ARMOR,
	CLASS_ASW_MORTAR_SHELL,
	CLASS_ASW_SMART_BOMB,
	CLASS_ASW_FIST,
	CLASS_ASW_BLINK,
	CLASS_ASW_JUMP_JET,
	CLASS_ASW_AMMO_SATCHEL,
	CLASS_ASW_TESLA_TRAP_PROJECTILE,
	CLASS_ASW_LASER_MINE_PROJECTILE,
	CLASS_ASW_EXPLOSIVE_BARREL,
	CLASS_ASW_HEALGRENADE_PROJECTILE,
	CLASS_ASW_REMOTE_TURRET,
	CLASS_ASW_FIRE,
	CLASS_ASW_PHYSICS_PROP,
	CLASS_ASW_SENTRY_BASE,		// bottom part of a deployed sentry gun
	
	// Add new classes here ^^^^

	LAST_ASW_ENTITY_CLASS,
};


// Alien Swarm specific hitgroups
enum
{
	HITGROUP_BONE_SHIELD = HITGROUP_GEAR + 1,
};

#define ASW_XP_CAP 42250

extern ConVar asw_visrange_generic;

#ifdef CLIENT_DLL

// interface for entities that want to show health bars
DECLARE_AUTO_LIST( IHealthTrackedAutoList );

abstract_class IHealthTracked : public IHealthTrackedAutoList
{
public:
	IHealthTracked( bool bAutoAdd = true ) : IHealthTrackedAutoList( bAutoAdd ) {}
	virtual void PaintHealthBar( class CASWHud3DMarineNames *pSurface ) = 0;
};

#endif

enum CASW_Flock_Leader_State
{
	ASW_FL_CHASING = 0,
	ASW_FL_CHARGING,
	ASW_FL_FLEEING,

	NUM_FLOCK_LEADER_STATES,
};

#endif // ASW_SHAREDDEFS_H
