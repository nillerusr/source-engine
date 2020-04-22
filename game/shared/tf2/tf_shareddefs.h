//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_SHAREDDEFS_H
#define TF_SHAREDDEFS_H

#ifdef _WIN32
#pragma once
#endif

#define MAX_TF_TEAMS		4

extern ConVar	inv_demo;
extern ConVar	lod_effect_distance;

#include "const.h"

//--------------------------------------------------------------------------
// Teams
#define TEAM_HUMANS		1
#define TEAM_ALIENS		2

//--------------------------------------------------------------------------
// TF player flags.
#define	TF_PLAYER_HIDDEN		(1<<0)
#define TF_PLAYER_DAMAGE_BOOST	(1<<1)
#define TF_PLAYER_NUMFLAGS		2
//--------------------------------------------------------------------------
// Custom Kill types
#define DMG_KILL_BULLRUSH	1


//--------------
// TF2 SPECIFIC	DAMAGE FLAGS
//--------------
#define DMG_EMP				(DMG_LASTGENERICFLAG<<1)	// Hit by EMP
#define DMG_PROBE			(DMG_LASTGENERICFLAG<<2)	// Doing a shield-aware probe (heal guns, emp guns)


//--------------------------------------------------------------------------
// Zone states
#define ZONE_FRIENDLY		1
#define ZONE_ENEMY			2
#define ZONE_CONTESTED		3

//--------------------------------------------------------------------------
// Loot state
#define LOOT_NOT			0
#define LOOT_CAPABLE		1
#define LOOT_LOOTING		2

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

//--------------------------------------------------------------------------
// Acts
//--------------------------------------------------------------------------
#define MIN_ACT_OVERLAY_TIME		10.0

// C/C_InfoAct spawnflags
#define SF_ACT_INTERMISSION				1
#define SF_ACT_WAITINGFORGAMESTART		2

#define SF_ACT_BITS						2

//--------------------------------------------------------------------------
// Order types
//--------------------------------------------------------------------------
enum
{
	ORDER_NONE = 0,
	ORDER_ATTACK,				// Enemy held resource zone, attack it and capture it
	ORDER_DEFEND,				// Defend a resource zone we own
	ORDER_CAPTURE,				// Resource zone not held by either team, capture it
	ORDER_KILL,					// Kill a specific enemy player
	ORDER_HEAL,					// Heal a specific friendly player
	ORDER_BUILD,				// Order to build something.
								// m_iStructure is one of the OBJ_ defines.
								// If it's a sentry gun order, it's always OBJ_SENTRYGUN_PLASMA.
	ORDER_REPAIR,				// Repair a built item.
	ORDER_MORTAR_ATTACK,		// Build a mortar to shell an enemy object.
	ORDER_ASSIST				// Assist a player who is under attack.
};


//--------------------------------------------------------------------------
// Collision groups
//--------------------------------------------------------------------------
enum
{
	TFCOLLISION_GROUP_SHIELD = LAST_SHARED_COLLISION_GROUP,
	TFCOLLISION_GROUP_WEAPON,
	TFCOLLISION_GROUP_GRENADE,
	TFCOLLISION_GROUP_RESOURCE_CHUNK,
	// Combat objects (override for above)
	TFCOLLISION_GROUP_COMBATOBJECT,
	// Objects in general
	TFCOLLISION_GROUP_OBJECT,
	TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT,
};

//--------------------------------------------------------------------------
// MAP DEFINED OBJECTS
//--------------------------------------------------------------------------
#define MAX_OBJ_CUSTOMNAME_SIZE		128

//--------------------------------------------------------------------------
// OBJECTS
//--------------------------------------------------------------------------
enum
{
	OBJ_POWERPACK=0,
	OBJ_RESUPPLY,
	OBJ_SENTRYGUN_PLASMA,			// Orders always refer to this type of sentry gun.
	OBJ_SENTRYGUN_ROCKET_LAUNCHER,
	OBJ_SHIELDWALL,
	OBJ_RESOURCEPUMP,
	OBJ_RESPAWN_STATION,
	OBJ_RALLYFLAG,
	OBJ_MANNED_PLASMAGUN,
	OBJ_MANNED_MISSILELAUNCHER,
	OBJ_MANNED_SHIELD,
	OBJ_EMPGENERATOR,
	OBJ_BUFF_STATION,
	OBJ_BARBED_WIRE,
	OBJ_MCV_SELECTION_PANEL,
	OBJ_MAPDEFINED,
	OBJ_MORTAR,
	// ADD STANDARD OBJECTS HERE

	// Upgrades
	OBJ_SELFHEAL,
	OBJ_ARMOR_UPGRADE,
	OBJ_VEHICLE_BOOST,
	OBJ_EXPLOSIVES,
	OBJ_DRIVER_MACHINEGUN,
	// ADD UPGRADES HERE

	// Vehicles
	OBJ_BATTERING_RAM,
	OBJ_SIEGE_TOWER,
	OBJ_WAGON,
	OBJ_FLATBED,
	OBJ_VEHICLE_MORTAR,
	OBJ_VEHICLE_TELEPORT_STATION,
	OBJ_VEHICLE_TANK,
	OBJ_VEHICLE_MOTORCYCLE,
	OBJ_WALKER_STRIDER,
	OBJ_WALKER_MINI_STRIDER,
	// ADD VEHICLES HERE

	// Defensive buildings
	OBJ_TOWER,
	OBJ_TUNNEL,
	OBJ_SANDBAG_BUNKER,
	OBJ_BUNKER,
	OBJ_DRAGONSTEETH,
	// ADD DEFENSIVE-ONLY BUILDINGS HERE

	// If you add a new object, you need to add it to the g_ObjectInfos array 
	// in tf_shareddefs.cpp, and add it's data to the scripts/object.txt

	OBJ_LAST,
};

#define OBJECT_COST_MULTIPLIER_PER_OBJECT			3
#define OBJECT_UPGRADE_COST_MULTIPLIER_PER_LEVEL	3

bool IsObjectAnUpgrade( int iObjectType );
bool IsObjectAVehicle( int iObjectType );
bool IsObjectADefensiveBuilding( int iObjectType );

class CHudTexture;

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
	float	m_flSapperAttachTime;			// Time it takes to place a sapper on this object

	// HUD weapon selection menu icon ( from hud_textures.txt )
	char	*m_pIconActive;
};

// Loads the objects.txt script.
class IBaseFileSystem;
void LoadObjectInfos( IBaseFileSystem *pFileSystem );

// Get a CObjectInfo from a TFOBJ_ define.
const CObjectInfo* GetObjectInfo( int iObject );


// Object utility funcs
bool	ClassCanBuild( int iClass, int iObjectType );
int		CalculateObjectCost( int iObjectType, int iNumberOfObjects, int iTeam, bool bLast = false );
int		CalculateObjectUpgrade( int iObjectType, int iObjectLevel );

//--------------------------------------------------------------------------
// OBJECT FLAGS
//--------------------------------------------------------------------------
enum
{
	OF_SUPPRESS_APPEAR_ON_MINIMAP			= 0x0001,
	OF_SUPPRESS_NOTIFY_UNDER_ATTACK			= 0x0002,
	OF_SUPPRESS_VISIBLE_TO_TACTICAL			= 0x0004,
	OF_ALLOW_REPEAT_PLACEMENT				= 0x0008,
	OF_SUPPRESS_TECH_ANALYZER				= 0x0010,
	OF_DONT_AUTO_REPAIR						= 0x0020,
	OF_ALIGN_TO_GROUND						= 0x0040,		// Align my angles to match the ground underneath me
	OF_DONT_PREVENT_BUILD_NEAR_OBJ			= 0x0080,		// Don't prevent building if there's another object nearby
	OF_CAN_BE_PICKED_UP						= 0x0100,
	OF_DOESNT_NEED_POWER					= 0x0200,		// Doesn't need power, even on the human team
	OF_DOESNT_HAVE_A_MODEL					= 0x0400,		// It's built from map placed geometry
	OF_MUST_BE_BUILT_IN_CONSTRUCTION_YARD	= 0x0800,
	OF_MUST_BE_BUILT_IN_RESOURCE_ZONE		= 0x1000,
	OF_MUST_BE_BUILT_ON_ATTACHMENT			= 0x2000,
	OF_CANNOT_BE_DISMANTLED					= 0x4000,

	OF_BIT_COUNT	= 15
};


//--------------------------------------------------------------------------
// Builder "weapon" states
//--------------------------------------------------------------------------
enum 
{
	BS_IDLE = 0,
	BS_SELECTING,
	BS_PLACING,
	BS_PLACING_INVALID,
	BS_BUILDING,
	BS_REPAIR,
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
#define MAX_OBJECTS_PER_TEAM	512
#define MAX_OBJECTS_PER_PLAYER	64


//--------------------------------------------------------------------------
// BUILDING
//--------------------------------------------------------------------------
// Build checks will return one of these for a player
enum
{
	CB_CAN_BUILD,			// Player is allowed to build this object
	CB_NOT_RESEARCHED,		// Player's team hasn't researched this object
	CB_LIMIT_REACHED,		// Player has reached the limit of the number of these objects allowed
	CB_NEED_RESOURCES,		// Player doesn't have enough resources to build this object
	CB_NEED_ADRENALIN,		// Commando doesn't have enough adrenalin to build a rally flag
};

//--------------------------------------------------------------------------
// MORTAR
//--------------------------------------------------------------------------
// Mortar Firing States
enum
{
	MORTAR_IDLE,
	MORTAR_CHARGING_POWER,
	MORTAR_CHARGING_ACCURACY,
};

// Mortar salvos
#define MORTAR_SALVO_SIZE					5
#define MORTAR_RELOAD_TIME					5.0

// Mortar firing details
#define	MORTAR_RANGE_MIN					1024
#define MORTAR_RANGE_MAX_INITIAL			3000
#define MORTAR_RANGE_MAX_UPGRADED			5000

// Inaccuracy Data
// These values are all max inaccuracies. The accuracy used is between 0 & Max, based upon how close the player gets to hitting
// the fire button at the right time on the mortar firing slider bar.
// Perpendicular-to-the-shot inaccuracy
#define MORTAR_INACCURACY_MAX_INITIAL		0.85		// Percentage of distance that a mortar can be wide of the mark
#define MORTAR_INACCURACY_MAX_UPGRADED		0.35		// Percentage of distance that a mortar can be wide of the mark
// Distance inaccuracy
#define MORTAR_DIST_INACCURACY				0.3			// Percentage of distance that the mortar can deviate

// Mortar firing details
#define MORTAR_CHARGE_POWER_RATE			1.5		// Time taken to hit full charge for power
#define MORTAR_CHARGE_ACCURACY_RATE			1.25	// Time taken to hit perfect accuracy, given a half-power shot

// Mortar ammo types
enum MortarAmmoType
{
	MA_SHELL = 0,		// Normal mortar round
	//MA_SMOKE,			// Smoke mortar round
	MA_CLUSTER,			// Mirv mortar round
	MA_STARBURST,		// Starburst / phosphorous mortar round

	MA_LASTAMMOTYPE,
};

extern char *MortarAmmoNames[ MA_LASTAMMOTYPE ];
extern char *MortarAmmoTechs[ MA_LASTAMMOTYPE ];
extern int	MortarAmmoMax[ MA_LASTAMMOTYPE ];

//--------------------------------------------------------------------------
// ROCKET PACK
//--------------------------------------------------------------------------
#define RP_LOCK_TIME			3.0			// Time taken to lock onto a target

//--------------------------------------------------------------------------
// PARTICLE BEAM
//--------------------------------------------------------------------------
#define PB_RECHARGE_TIME		30.0		// Time taken to recharge the particle beam

//--------------------------------------------------------------------------
// PLASMA PROJECTILE TYPES
//--------------------------------------------------------------------------
enum
{
	PLASMATYPE_GATLING,
	PLASMATYPE_EMP,
	PLASMATYPE_GUIDED,
	PLASMATYPE_GUIDED_NOTARGET,
	PLASMATYPE_GUIDED_PARRIED,
	PLASMATYPE_PLASMABALL,
	PLASMATYPE_PLASMABALL_EXPLOSIVE,
};

#define	PLASMA_VELOCITY	( 2500 )

//--------------------------------------------------------------------------
// SCANNERS
//--------------------------------------------------------------------------
// Ranges
#define SCANNER_RANGE			3000
#define LOCAL_PLAYER_SCANNER_RANGE 1500

//--------------------------------------------------------------------------
// EMP
//--------------------------------------------------------------------------
#define EMP_HITSCAN_DURATION	5.0f

//--------------------------------------------------------------------------
// KNOCKDOWN
//--------------------------------------------------------------------------
// Knockdown blend in and out times
#define KNOCKDOWN_BLEND_IN ( 0.3f )
#define KNOCKDOWN_BLEND_OUT ( 0.5f )

//--------------------------------------------------------------------------
// THERMAL VISION
//--------------------------------------------------------------------------
// Thermal vision radius 
// Players outside of this radius will not be sent to the local player
// Player's inside start to fade at the startfade distance and are alpha'd out completely
//  at the full radius
#define THERMAL_VISION_RADIUS		( 1024.0f )
#define THERMAL_VISION_STARTFADE	( THERMAL_VISION_RADIUS / 2.0f )

//--------------------------------------------------------------------------
// CAMO
//--------------------------------------------------------------------------
// Infiltrator Camouflage constants
// # of seconds to go into/back into camo mode
#define CAMO_ENABLETIME				( 3.0f )
// # of seconds to remove
#define CAMO_REMOVETIME				( 1.0f )
// # of seconds to temporarily suppress
#define CAMO_SUPPRESSTIME			( 1.0f )

// Outside this, exclude from PVS
#define CAMO_OUTER_RADIUS			( 2048.0f )
// From here to outer, just alpha to 0
#define CAMO_INNER_RADIUS			( CAMO_OUTER_RADIUS / 2.0f )
// 75 % opacity at the inner_radius
#define CAMO_INNER_ALPHA			( 192 )
// From here to inner fade alpha again and start using invis effect
#define CAMO_INVIS_RADIUS			( CAMO_INNER_RADIUS / 2.0f )

// Infiltrator's phaseout duration
#define INFILTRATOR_PHASEOUT_DURATION 30.0f
// Time required to recharge
#define INFILTRATOR_PHASEOUT_RECHARGETIME 30.0f

// After sitting still, remove from tactical and minimiap starting at this time
#define SNIPER_STATIONARY_FADESTART		2.5f
// After sitting still, remove from tactical and minimiap starting and finishing here
#define SNIPER_STATIONARY_FADEFINISH	7.5f

// Infiltrator Camouflage constants
// # of seconds to go into/back into camo mode
#define CAMO_ENABLETIME				( 3.0f )
// # of seconds to remove
#define CAMO_REMOVETIME				( 1.0f )
// # of seconds to temporarily suppress
#define CAMO_SUPPRESSTIME			( 1.0f )

// Outside this, exclude from PVS
#define CAMO_OUTER_RADIUS			( 2048.0f )
// From here to outer, just alpha to 0
#define CAMO_INNER_RADIUS			( CAMO_OUTER_RADIUS / 2.0f )
// 75 % opacity at the inner_radius
#define CAMO_INNER_ALPHA			( 192 )
// From here to inner fade alpha again and start using invis effect
#define CAMO_INVIS_RADIUS			( CAMO_INNER_RADIUS / 2.0f )

// Infiltrator's phaseout duration
#define INFILTRATOR_PHASEOUT_DURATION 30.0f
// Time required to recharge
#define INFILTRATOR_PHASEOUT_RECHARGETIME 30.0f

// After sitting still, remove from tactical and minimiap starting at this time
#define SNIPER_STATIONARY_FADESTART		2.5f
// After sitting still, remove from tactical and minimiap starting and finishing here
#define SNIPER_STATIONARY_FADEFINISH	7.5f


//--------------------------------------------------------------------------
// ANIM STATEMACHINE DEFINES
//--------------------------------------------------------------------------
// Sniper deploy node
enum
{
	SNIPER_DEPLOY_START = 1,
	SNIPER_DEPLOY_IDLE,
	SNIPER_DEPLOY_LEAVE,
};

//--------------------------------------------------------------------------
// COMBAT SHIELD
//--------------------------------------------------------------------------
#define SHIELD_HITGROUP				1

// Length after raising the shield during which releasing the shield will cause a parry
#define PARRY_DETECTION_TIME			0.5
// Length after a parry detection in which a parry can occur
#define PARRY_OPPORTUNITY_LENGTH		0.3
// Length after a parry has finished before I can do anything again
#define PARRY_VULNERABLE_TIME			0.5

//--------------------------------------------------------------------------
// SENTRYGUNS
//--------------------------------------------------------------------------
// Time it takes to turtle / unturtle
#define	SENTRY_TURTLE_TIME		2.0

//--------------------------------------------------------------------------
// PORTABLE POWER GENERATOR - BUFF STATION
//--------------------------------------------------------------------------
#define BUFF_STATION_MAX_PLAYERS		4
#define BUFF_STATION_MAX_PLAYER_BITS	3
#define BUFF_STATION_MAX_OBJECTS		3
#define BUFF_STATION_MAX_OBJECT_BITS	2

//--------------------------------------------------------------------------
// ADRENALIN
//--------------------------------------------------------------------------
// Animation speed while in adrenalin
#define ADRENALIN_ANIM_SPEED	1.5

//--------------------------------------------------------------------------
// PLASMA RIFLE
//--------------------------------------------------------------------------
#define MAX_RIFLE_POWER		3.0
#define RIFLE_CHARGE_TIME	2.0

//--------------------------------------------------------------------------
// HUMAN POWER PACKS
//--------------------------------------------------------------------------
#define MAX_OBJECTS_PER_PACK		3

//--------------------------------------------------------------------------
// Rally flag defines
//--------------------------------------------------------------------------

#define RALLYFLAG_MINS				Vector(-20, -20, 0)
#define RALLYFLAG_MAXS				Vector( 20,  20, 90)
#define RALLYFLAG_RADIUS			512			
#define RALLYFLAG_LIFETIME			30
#define RALLYFLAG_RATE				2			// Rate at which it looks for friendlies to rally
#define RALLYFLAG_ADRENALIN_TIME	5			// Time an adrenalin rush lasts
#define RALLYFLAG_MODEL				"models/props/common/holo_banner/holo_banner.mdl"


//--------------------------------------------------------------------------
// Resupply-related stuff
//--------------------------------------------------------------------------
enum ResupplyBuyType_t
{
	RESUPPLY_BUY_AMMO = 0,
	RESUPPLY_BUY_HEALTH,
	RESUPPLY_BUY_GRENADES,
	RESUPPLY_BUY_ALL,

	RESUPPLY_BUY_TYPE_COUNT
};

#define RESUPPLY_HEALTH_COST			20
#define RESUPPLY_AMMO_COST				5
#define RESUPPLY_GRENADES_COST			25
#define RESUPPLY_ALL_COST				50
#define RESUPPLY_ROCKET_COST			100

// Build animation events
#define TF_OBJ_ENABLEBODYGROUP			6000
#define TF_OBJ_DISABLEBODYGROUP			6001
#define TF_OBJ_ENABLEALLBODYGROUPS		6002
#define TF_OBJ_DISABLEALLBODYGROUPS		6003
#define TF_OBJ_PLAYBUILDSOUND			6004

//--------------------------------------------------------------------------
// Class id
//--------------------------------------------------------------------------

#include "tfclassdata_shared.h"


//--------------------------------------------------------------------------
//
// Class info tables. Any time a class is added or removed, this is where
// generic data about each class is stored.
//
// The other things you need to add or remove for a class are:
//
// - A class derived from CPlayerClass.
// - Class-specific accessors and data members functions in CTFMoveData.
// - tf_gamemovement_chooser.h and CTFGameMovementChooser::CTFGameMovementChooser().
// - DEFINE_PRED_TYPEDESCRIPTION_PTR entries in c_basetfplayer.cpp
// - Add class_X_health in skill1.cfg.
//
//--------------------------------------------------------------------------

class CPlayerClass;
class CBaseTFPlayer;
typedef CPlayerClass* (*PlayerClassAllocFn_Server)( CBaseTFPlayer *pPlayer );

class C_PlayerClass;
class C_BaseTFPlayer;
typedef C_PlayerClass* (*PlayerClassAllocFn_Client)( C_BaseTFPlayer *pPlayer );


class ConVar;


class CTFClassInfo
{
public:
	char		*m_pClassName;
	
	// Objects that each class can build
	// OBJ_X, OBJ_Y ... terminated with OBJ_LAST.
	int			*m_pClassObjects;

	// This is just to make stats gathering easy... which classes are in the game right now?
	bool		m_pCurrentlyActive;

	PlayerClassAllocFn_Client	m_pClientAlloc;		// Only valid in client.dll
	PlayerClassAllocFn_Server	m_pServerAlloc;		// Only valid in the game dll

	ConVar		*m_pMaxHealthCVar;					// Only valid in game dll
};

const CTFClassInfo* GetTFClassInfo( int i );


#if defined( CLIENT_DLL )

	#define CAllPlayerClasses	C_AllPlayerClasses
	#define PLAYER_CLASS_TYPE	C_PlayerClass
	#define PLAYER_TYPE			C_BaseTFPlayer
	
	EXTERN_RECV_TABLE( DT_AllPlayerClasses );

#else

	#define PLAYER_CLASS_TYPE	CPlayerClass
	#define PLAYER_TYPE			CBaseTFPlayer

	EXTERN_SEND_TABLE( DT_AllPlayerClasses );

#endif


class PLAYER_TYPE;
class PLAYER_CLASS_TYPE;

//
// The player object contains this on both the client and the server.
// It holds a copy of each player class that can be used.
//
class CAllPlayerClasses // (#define as C_AllPlayerClasses on the client)
{
public:
						CAllPlayerClasses( PLAYER_TYPE *pPlayer );
						~CAllPlayerClasses();

	PLAYER_CLASS_TYPE*	GetPlayerClass( int iClass );

public:

	PLAYER_CLASS_TYPE*	m_pClasses[ TFCLASS_CLASS_COUNT ];
};

//--------------------------------------------------------------------------
// Impact data
//--------------------------------------------------------------------------
// This sucks
#define NUM_WOOD_GIBS_SMALL		5
extern const char *ImpactHurtGibs_Wood_Small[ NUM_WOOD_GIBS_SMALL ];

#define PLAYER_MSG_PERSONAL_SHIELD	2

#endif // TF_SHAREDDEFS_H

