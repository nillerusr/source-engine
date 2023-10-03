//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// Purpose: Definitions that are shared by the game DLL and the client DLL.
//
//===============================================================================


#ifndef SHAREDDEFS_H
#define SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

#include "bittools.h"

#define TICK_INTERVAL			(gpGlobals->interval_per_tick)


#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / TICK_INTERVAL ) )
#define TICKS_TO_TIME( t )		( TICK_INTERVAL *( t ) )
#define ROUND_TO_TICKS( t )		( TICK_INTERVAL * TIME_TO_TICKS( t ) )
#define TICK_NEVER_THINK		(-1)


#define ANIMATION_CYCLE_BITS		15

#define ANIMATION_CYCLE_MINFRAC		(1.0f / (1<<ANIMATION_CYCLE_BITS))

// Matching the high level concept is significantly better than other criteria
// FIXME:  Could do this in the script file by making it required and bumping up weighting there instead...
#define CONCEPT_WEIGHT 5.0f



// Each mod defines these for itself.
class CViewVectors
{
public:
	CViewVectors() {}

	CViewVectors( 
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight )
	{
		m_vView = vView;
		m_vHullMin = vHullMin;
		m_vHullMax = vHullMax;
		m_vDuckHullMin = vDuckHullMin;
		m_vDuckHullMax = vDuckHullMax;
		m_vDuckView = vDuckView;
		m_vObsHullMin = vObsHullMin;
		m_vObsHullMax = vObsHullMax;
		m_vDeadViewHeight = vDeadViewHeight;
	}

	// Height above entity position where the viewer's eye is.
	Vector m_vView;
	
	Vector m_vHullMin;
	Vector m_vHullMax;
	
	Vector m_vDuckHullMin;
	Vector m_vDuckHullMax;
	Vector m_vDuckView;
	
	Vector m_vObsHullMin;
	Vector m_vObsHullMax;
	
	Vector m_vDeadViewHeight;
};

// Height above entity position where the viewer's eye is.
#define VEC_VIEW			g_pGameRules->GetViewVectors()->m_vView
#define VEC_HULL_MIN		g_pGameRules->GetViewVectors()->m_vHullMin
#define VEC_HULL_MAX		g_pGameRules->GetViewVectors()->m_vHullMax

#define VEC_DUCK_HULL_MIN	g_pGameRules->GetViewVectors()->m_vDuckHullMin
#define VEC_DUCK_HULL_MAX	g_pGameRules->GetViewVectors()->m_vDuckHullMax
#define VEC_DUCK_VIEW		g_pGameRules->GetViewVectors()->m_vDuckView

#define VEC_OBS_HULL_MIN	g_pGameRules->GetViewVectors()->m_vObsHullMin
#define VEC_OBS_HULL_MAX	g_pGameRules->GetViewVectors()->m_vObsHullMax

#define VEC_DEAD_VIEWHEIGHT	g_pGameRules->GetViewVectors()->m_vDeadViewHeight


#define WATERJUMP_HEIGHT			8

#define MAX_CLIMB_SPEED		200


	#define TIME_TO_DUCK_MSECS		400
 
#define TIME_TO_UNDUCK_MSECS		200

inline float FractionDucked( int msecs )
{
	return clamp( (float)msecs / (float)TIME_TO_DUCK_MSECS, 0.0f, 1.0f );
}

inline float FractionUnDucked( int msecs )
{
	return clamp( (float)msecs / (float)TIME_TO_UNDUCK_MSECS, 0.0f, 1.0f );
}

#define MAX_WEAPON_SLOTS		6	// hud item selection slots
#define MAX_WEAPON_POSITIONS	20	// max number of items within a slot
#define MAX_ITEM_TYPES			6	// hud item selection slots
#define MAX_WEAPONS				48	// Max number of weapons available

#define MAX_ITEMS				5	// hard coded item types

#define WEAPON_NOCLIP			-1	// clip sizes set to this tell the weapon it doesn't use a clip

#define	MAX_AMMO_TYPES	32		// ???
#define MAX_AMMO_SLOTS  32		// not really slots

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4


//===================================================================================================================
// Close caption flags
#define CLOSE_CAPTION_WARNIFMISSING	( 1<<0 )
#define CLOSE_CAPTION_FROMPLAYER	( 1<<1 )
#define CLOSE_CAPTION_GENDER_MALE	( 1<<2 )
#define CLOSE_CAPTION_GENDER_FEMALE	( 1<<3 )

//===================================================================================================================
// Hud Element hiding flags
#define	HIDEHUD_WEAPONSELECTION		( 1<<0 )	// Hide ammo count & weapon selection
#define	HIDEHUD_FLASHLIGHT			( 1<<1 )
#define	HIDEHUD_ALL					( 1<<2 )
#define HIDEHUD_HEALTH				( 1<<3 )	// Hide health & armor / suit battery
#define HIDEHUD_PLAYERDEAD			( 1<<4 )	// Hide when local player's dead
#define HIDEHUD_NEEDSUIT			( 1<<5 )	// Hide when the local player doesn't have the HEV suit
#define HIDEHUD_MISCSTATUS			( 1<<6 )	// Hide miscellaneous status elements (trains, pickup history, death notices, etc)
#define HIDEHUD_CHAT				( 1<<7 )	// Hide all communication elements (saytext, voice icon, etc)
#define	HIDEHUD_CROSSHAIR			( 1<<8 )	// Hide crosshairs
#define	HIDEHUD_VEHICLE_CROSSHAIR	( 1<<9 )	// Hide vehicle crosshair
#define HIDEHUD_INVEHICLE			( 1<<10 )
#define HIDEHUD_BONUS_PROGRESS		( 1<<11 )	// Hide bonus progress display (for bonus map challenges)

#define HIDEHUD_BITCOUNT			12

//===================================================================================================================
// suit usage bits
#define bits_SUIT_DEVICE_SPRINT		0x00000001
#define bits_SUIT_DEVICE_FLASHLIGHT	0x00000002
#define bits_SUIT_DEVICE_BREATHER	0x00000004

#define MAX_SUIT_DEVICES			3


//===================================================================================================================
// Player Defines

// Max number of players in a game ( see const.h for ABSOLUTE_PLAYER_LIMIT (256 ) )
// The Source engine is really designed for 32 or less players.  If you raise this number above 32, you better know what you are doing
//  and have a good answer for a bunch of perf question related to player simulation, thinking logic, tracelines, networking overhead, etc.
// But if you are brave or are doing something interesting, go for it...   ywb 9/22/03

//You might be wondering why these aren't multiple of 2. Well the reason is that if servers decide to have HLTV or Replay enabled we need the extra slot.
//This is ok since MAX_PLAYERS is used for code specific things like arrays and loops, but it doesn't really means that this is the max number of players allowed
//Since this is decided by the gamerules (and it can be whatever number as long as its less than MAX_PLAYERS).

	#define MAX_PLAYERS				33  // Absolute max players supported


#define MAX_PLACE_NAME_LENGTH		18

//===================================================================================================================
// Team Defines
#define TEAM_ANY				-1	// for some team query methods
#define	TEAM_INVALID			-1
#define TEAM_UNASSIGNED			0	// not assigned to a team
#define TEAM_SPECTATOR			1	// spectator team
// Start your team numbers after this
#define LAST_SHARED_TEAM		TEAM_SPECTATOR

// The first team that's game specific (i.e. not unassigned / spectator)
#define FIRST_GAME_TEAM			(LAST_SHARED_TEAM+1)

#define MAX_TEAMS				32	// Max number of teams in a game
#define MAX_TEAM_NAME_LENGTH	32	// Max length of a team's name

// Weapon m_iState
#define WEAPON_NOT_CARRIED				0	// Weapon is on the ground
#define WEAPON_IS_CARRIED_BY_PLAYER		1	// This client is carrying this weapon.
#define WEAPON_IS_ACTIVE				2	// This client is carrying this weapon and it's the currently held weapon

// -----------------------------------------
// Skill Level
// -----------------------------------------
#define SKILL_EASY		1
#define SKILL_MEDIUM	2
#define SKILL_HARD		3


// Weapon flags
// -----------------------------------------
//	Flags - NOTE: KEEP g_ItemFlags IN WEAPON_PARSE.CPP UPDATED WITH THESE
// -----------------------------------------
#define ITEM_FLAG_SELECTONEMPTY		(1<<0)
#define ITEM_FLAG_NOAUTORELOAD		(1<<1)
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	(1<<2)
#define ITEM_FLAG_LIMITINWORLD		(1<<3)
#define ITEM_FLAG_EXHAUSTIBLE		(1<<4)	// A player can totally exhaust their ammo supply and lose this weapon
#define ITEM_FLAG_DOHITLOCATIONDMG	(1<<5)	// This weapon take hit location into account when applying damage
#define ITEM_FLAG_NOAMMOPICKUPS		(1<<6)	// Don't draw ammo pickup sprites/sounds when ammo is received
#define ITEM_FLAG_NOITEMPICKUP		(1<<7)	// Don't draw weapon pickup when this weapon is picked up by the player
// NOTE: KEEP g_ItemFlags IN WEAPON_PARSE.CPP UPDATED WITH THESE


// Humans only have left and right hands, though we might have aliens with more
//  than two, sigh
#define MAX_VIEWMODELS			2

#define MAX_BEAM_ENTS			10

#define TRACER_TYPE_DEFAULT		0x00000001
#define TRACER_TYPE_GUNSHIP		0x00000002
#define TRACER_TYPE_STRIDER		0x00000004 // Here ya go, Jay!
#define TRACER_TYPE_GAUSS		0x00000008
#define TRACER_TYPE_WATERBULLET	0x00000010

#define MUZZLEFLASH_TYPE_DEFAULT	0x00000001
#define MUZZLEFLASH_TYPE_GUNSHIP	0x00000002
#define MUZZLEFLASH_TYPE_STRIDER	0x00000004

// Muzzle flash definitions (for the flags field of the "MuzzleFlash" DispatchEffect)
enum
{
	MUZZLEFLASH_AR2				= 0,
	MUZZLEFLASH_SHOTGUN,
	MUZZLEFLASH_SMG1,
	MUZZLEFLASH_SMG2,
	MUZZLEFLASH_PISTOL,
	MUZZLEFLASH_COMBINE,
	MUZZLEFLASH_357,
	MUZZLEFLASH_RPG,
	MUZZLEFLASH_COMBINE_TURRET,

	MUZZLEFLASH_FIRSTPERSON		= 0x100,
};

// Tracer Flags
#define TRACER_FLAG_WHIZ			0x0001
#define TRACER_FLAG_USEATTACHMENT	0x0002

#define TRACER_DONT_USE_ATTACHMENT	-1

// Entity Dissolve types
enum
{
	ENTITY_DISSOLVE_NORMAL = 0,
	ENTITY_DISSOLVE_ELECTRICAL,
	ENTITY_DISSOLVE_ELECTRICAL_LIGHT,
	ENTITY_DISSOLVE_CORE,

	// NOTE: Be sure to up the bits if you make more dissolve types
	ENTITY_DISSOLVE_BITS = 3
};

// ---------------------------
//  Hit Group standards
// ---------------------------
#define	HITGROUP_GENERIC	0
#define	HITGROUP_HEAD		1
#define	HITGROUP_CHEST		2
#define	HITGROUP_STOMACH	3
#define HITGROUP_LEFTARM	4	
#define HITGROUP_RIGHTARM	5
#define HITGROUP_LEFTLEG	6
#define HITGROUP_RIGHTLEG	7
#define HITGROUP_GEAR		10			// alerts NPC, but doesn't do damage or bleed (1/100th damage)

//
// Enumerations for setting player animation.
//
enum PLAYER_ANIM
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
	PLAYER_IN_VEHICLE,

	// TF Player animations
	PLAYER_RELOAD,
	PLAYER_START_AIMING,
	PLAYER_LEAVE_AIMING,
};

#ifdef HL2_DLL
// HL2 has 600 gravity by default
// NOTE: The discrete ticks can have quantization error, so these numbers are biased a little to
// make the heights more exact
#define PLAYER_FATAL_FALL_SPEED		922.5f // approx 60 feet sqrt( 2 * gravity * 60 * 12 )
#define PLAYER_MAX_SAFE_FALL_SPEED	526.5f // approx 20 feet sqrt( 2 * gravity * 20 * 12 )
#define PLAYER_LAND_ON_FLOATING_OBJECT	173 // Can fall another 173 in/sec without getting hurt
#define PLAYER_MIN_BOUNCE_SPEED		173
#define PLAYER_FALL_PUNCH_THRESHOLD 303.0f // won't punch player's screen/make scrape noise unless player falling at least this fast - at least a 76" fall (sqrt( 2 * g * 76))
#else
#define PLAYER_FATAL_FALL_SPEED		1024 // approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	580 // approx 20 feet
#define PLAYER_LAND_ON_FLOATING_OBJECT	200 // Can go another 200 units without getting hurt
#define PLAYER_MIN_BOUNCE_SPEED		200
#define PLAYER_FALL_PUNCH_THRESHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.
#endif
#define DAMAGE_FOR_FALL_SPEED		100.0f / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED ) // damage per unit per second.


#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669
#define AUTOAIM_20DEGREES 0.3490658503989

#define AUTOAIM_SCALE_DEFAULT		1.0f
#define AUTOAIM_SCALE_DIRECT_ONLY	0.0f

// instant damage

// For a means of resolving these consts into debug string text, see function
// CTakeDamageInfo::DebugGetDamageTypeString(unsigned int DamageType, char *outbuf, unsigned int outbuflength )
#define DMG_GENERIC			0			// generic damage was done
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object. 
										// NOTE: It's assumed crush damage is occurring as a result of physics collision, so no extra physics force is generated by crush damage.
										// DON'T use DMG_CRUSH when damaging entities unless it's the result of a physics collision. You probably want DMG_CLUB instead.
#define DMG_BULLET			(1 << 1)	// shot
#define DMG_SLASH			(1 << 2)	// cut, clawed, stabbed
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_VEHICLE			(1 << 4)	// hit by a vehicle
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_BLAST			(1 << 6)	// explosive blast damage
#define DMG_CLUB			(1 << 7)	// crowbar, punch, headbutt
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_SONIC			(1 << 9)	// sound pulse shockwave
#define DMG_ENERGYBEAM		(1 << 10)	// laser or other high energy beam 
#define DMG_PREVENT_PHYSICS_FORCE		(1 << 11)	// Prevent a physics force 
#define DMG_NEVERGIB		(1 << 12)	// with this bit OR'd in, no damage type will be able to gib victims upon death
#define DMG_ALWAYSGIB		(1 << 13)	// with this bit OR'd in, any damage type can be made to gib victims upon death.
#define DMG_DROWN			(1 << 14)	// Drowning


#define DMG_PARALYZE		(1 << 15)	// slows affected creature down
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_POISON			(1 << 17)	// blood poisoning - heals over time like drowning damage
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven

#define DMG_REMOVENORAGDOLL	(1<<22)		// with this bit OR'd in, no ragdoll will be created, and the target will be quietly removed.
										// use this to kill an entity that you've already got a server-side ragdoll for

#define DMG_PHYSGUN			(1<<23)		// Hit by manipulator. Usually doesn't do any damage.
#define DMG_PLASMA			(1<<24)		// Shot by Cremator
#define DMG_AIRBOAT			(1<<25)		// Hit by the airboat's gun

#define DMG_DISSOLVE		(1<<26)		// Dissolving!
#define DMG_BLAST_SURFACE	(1<<27)		// A blast on the surface of water that cannot harm things underwater
#define DMG_DIRECT			(1<<28)
#define DMG_BUCKSHOT		(1<<29)		// not quite a bullet. Little, rounder, different.

// NOTE: DO NOT ADD ANY MORE CUSTOM DMG_ TYPES. MODS USE THE DMG_LASTGENERICFLAG BELOW, AND
//		 IF YOU ADD NEW DMG_ TYPES, THEIR TYPES WILL BE HOSED. WE NEED A BETTER SOLUTION.

// TODO: keep this up to date so all the mod-specific flags don't overlap anything.
#define DMG_LASTGENERICFLAG	DMG_BUCKSHOT



// settings for m_takedamage
#define	DAMAGE_NO				0
#define DAMAGE_EVENTS_ONLY		1		// Call damage functions, but don't modify health
#define	DAMAGE_YES				2
#define	DAMAGE_AIM				3

// Spectator Movement modes
enum
{
	OBS_MODE_NONE = 0,	// not in spectator mode
	OBS_MODE_DEATHCAM,	// special mode for death cam animation
	OBS_MODE_FREEZECAM,	// zooms to a target, and freeze-frames on them
	OBS_MODE_FIXED,		// view from a fixed camera position
	OBS_MODE_IN_EYE,	// follow a player in first person view
	OBS_MODE_CHASE,		// follow a player in third person view
	OBS_MODE_ROAMING,	// free roaming

	NUM_OBSERVER_MODES,
};

#define LAST_PLAYER_OBSERVERMODE	OBS_MODE_ROAMING

// Force Camera Restrictions with mp_forcecamera
enum {
	OBS_ALLOW_ALL = 0,	// allow all modes, all targets
	OBS_ALLOW_TEAM,		// allow only own team & first person, no PIP
	OBS_ALLOW_NONE,		// don't allow any spectating after death (fixed & fade to black)

	OBS_ALLOW_NUM_MODES,
};

enum
{
	TYPE_TEXT = 0,	// just display this plain text
	TYPE_INDEX,		// lookup text & title in stringtable
	TYPE_URL,		// show this URL
	TYPE_FILE,		// show this local file
} ;

// VGui Screen Flags
enum
{
	VGUI_SCREEN_ACTIVE = 0x1,
	VGUI_SCREEN_VISIBLE_TO_TEAMMATES = 0x2,
	VGUI_SCREEN_ATTACHED_TO_VIEWMODEL = 0x4,
	VGUI_SCREEN_TRANSPARENT = 0x8,
	VGUI_SCREEN_ONLY_USABLE_BY_OWNER = 0x10,

	VGUI_SCREEN_MAX_BITS = 5
};

typedef enum
{
	USE_OFF = 0, 
	USE_ON = 1, 
	USE_SET = 2, 
	USE_TOGGLE = 3
} USE_TYPE;

// basic team colors
#define COLOR_RED		Color(255, 64, 64, 255)
#define COLOR_BLUE		Color(153, 204, 255, 255)
#define COLOR_YELLOW	Color(255, 178, 0, 255)
#define COLOR_GREEN		Color(153, 255, 153, 255)
#define COLOR_GREY		Color(204, 204, 204, 255)

// All NPCs need this data
enum
{
	DONT_BLEED = -1,

	BLOOD_COLOR_RED = 0,
	BLOOD_COLOR_YELLOW,
	BLOOD_COLOR_GREEN,
	BLOOD_COLOR_MECH,

#if defined( HL2_EPISODIC )
	BLOOD_COLOR_ANTLION,		// FIXME: Move to Base HL2
	BLOOD_COLOR_ZOMBIE,			// FIXME: Move to Base HL2
	BLOOD_COLOR_ANTLION_WORKER,
	BLOOD_COLOR_BLOB,
	BLOOD_COLOR_BLOB_FROZEN,
#endif // HL2_EPISODIC

#if defined( INFESTED_DLL )
	BLOOD_COLOR_BLOB,
	BLOOD_COLOR_BLOB_FROZEN,
#endif // INFESTED_DLL

	BLOOD_COLOR_BRIGHTGREEN,
};

//-----------------------------------------------------------------------------
// Vehicles may have more than one passenger.
// This enum may be expanded by derived classes
//-----------------------------------------------------------------------------
enum PassengerRole_t
{
	VEHICLE_ROLE_NONE = -1,

	VEHICLE_ROLE_DRIVER = 0,	// Only one driver
	
	LAST_SHARED_VEHICLE_ROLE,
};

//-----------------------------------------------------------------------------
// Water splash effect flags
//-----------------------------------------------------------------------------
enum
{
	FX_WATER_IN_SLIME = 0x1,
};


// Shared think context stuff
#define	MAX_CONTEXT_LENGTH		32
#define NO_THINK_CONTEXT	-1

// entity flags, CBaseEntity::m_iEFlags
enum
{
	EFL_KILLME	=				(1<<0),	// This entity is marked for death -- This allows the game to actually delete ents at a safe time
	EFL_DORMANT	=				(1<<1),	// Entity is dormant, no updates to client
	EFL_NOCLIP_ACTIVE =			(1<<2),	// Lets us know when the noclip command is active.
	EFL_SETTING_UP_BONES =		(1<<3),	// Set while a model is setting up its bones.
	EFL_KEEP_ON_RECREATE_ENTITIES = (1<<4), // This is a special entity that should not be deleted when we restart entities only

	EFL_DIRTY_SHADOWUPDATE =	(1<<5),	// Client only- need shadow manager to update the shadow...
	EFL_NOTIFY =				(1<<6),	// Another entity is watching events on this entity (used by teleport)

	// The default behavior in ShouldTransmit is to not send an entity if it doesn't
	// have a model. Certain entities want to be sent anyway because all the drawing logic
	// is in the client DLL. They can set this flag and the engine will transmit them even
	// if they don't have a model.
	EFL_FORCE_CHECK_TRANSMIT =	(1<<7),

	EFL_BOT_FROZEN =			(1<<8),	// This is set on bots that are frozen.
	EFL_SERVER_ONLY =			(1<<9),	// Non-networked entity.
	EFL_NO_AUTO_EDICT_ATTACH =	(1<<10), // Don't attach the edict; we're doing it explicitly
	
	// Some dirty bits with respect to abs computations
	EFL_DIRTY_ABSTRANSFORM =	(1<<11),
	EFL_DIRTY_ABSVELOCITY =		(1<<12),
	EFL_DIRTY_ABSANGVELOCITY =	(1<<13),
	EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS	= (1<<14),
	EFL_DIRTY_SPATIAL_PARTITION = (1<<15),
	EFL_HAS_PLAYER_CHILD=		(1<<16),	// One of the child entities is a player.

	EFL_IN_SKYBOX =				(1<<17),	// This is set if the entity detects that it's in the skybox.
											// This forces it to pass the "in PVS" for transmission.
	EFL_USE_PARTITION_WHEN_NOT_SOLID = (1<<18),	// Entities with this flag set show up in the partition even when not solid
	EFL_TOUCHING_FLUID =		(1<<19),	// Used to determine if an entity is floating

	// FIXME: Not really sure where I should add this...
	EFL_IS_BEING_LIFTED_BY_BARNACLE = (1<<20),
	EFL_NO_ROTORWASH_PUSH =		(1<<21),		// I shouldn't be pushed by the rotorwash
	EFL_NO_THINK_FUNCTION =		(1<<22),
	EFL_NO_GAME_PHYSICS_SIMULATION = (1<<23),

	EFL_CHECK_UNTOUCH =			(1<<24),
	EFL_DONTBLOCKLOS =			(1<<25),		// I shouldn't block NPC line-of-sight
	EFL_DONTWALKON =			(1<<26),		// NPC;s should not walk on this entity
	EFL_NO_DISSOLVE =			(1<<27),		// These guys shouldn't dissolve
	EFL_NO_MEGAPHYSCANNON_RAGDOLL = (1<<28),	// Mega physcannon can't ragdoll these guys.
	EFL_NO_WATER_VELOCITY_CHANGE  =	(1<<29),	// Don't adjust this entity's velocity when transitioning into water
	EFL_NO_PHYSCANNON_INTERACTION =	(1<<30),	// Physcannon can't pick these up or punt them
	EFL_NO_DAMAGE_FORCES =		(1<<31),	// Doesn't accept forces from physics damage
};

//-----------------------------------------------------------------------------
// EFFECTS
//-----------------------------------------------------------------------------
const int FX_BLOODSPRAY_DROPS	= 0x01;
const int FX_BLOODSPRAY_GORE	= 0x02;
const int FX_BLOODSPRAY_CLOUD	= 0x04;
const int FX_BLOODSPRAY_ALL		= 0xFF;

//-----------------------------------------------------------------------------
#define MAX_SCREEN_OVERLAYS		10

// These are the types of data that hang off of CBaseEntities and the flag bits used to mark their presence
enum
{
	GROUNDLINK = 0,
	TOUCHLINK,
	STEPSIMULATION,
	MODELSCALE,
	POSITIONWATCHER,
	PHYSICSPUSHLIST,
	VPHYSICSUPDATEAI,
	VPHYSICSWATCHER,

	// Must be last and <= 32
	NUM_DATAOBJECT_TYPES,
};

class CBaseEntity;

//-----------------------------------------------------------------------------
// Bullet firing information
//-----------------------------------------------------------------------------
class CBaseEntity;

enum FireBulletsFlags_t
{
	FIRE_BULLETS_FIRST_SHOT_ACCURATE = 0x1,		// Pop the first shot with perfect accuracy
	FIRE_BULLETS_DONT_HIT_UNDERWATER = 0x2,		// If the shot hits its target underwater, don't damage it
	FIRE_BULLETS_ALLOW_WATER_SURFACE_IMPACTS = 0x4,	// If the shot hits water surface, still call DoImpactEffect
	FIRE_BULLETS_TEMPORARY_DANGER_SOUND = 0x8,		// Danger sounds added from this impact can be stomped immediately if another is queued

	FIRE_BULLETS_NO_PIERCING_SPARK = 0x16,	// do a piercing spark effect when a bullet penetrates an alien
	FIRE_BULLETS_HULL = 0x32,	// bullet trace is a hull rather than a line
	FIRE_BULLETS_ANGULAR_SPREAD = 0x64,	// bullet spread is based on uniform random change to angles rather than gaussian search
};


struct FireBulletsInfo_t
{
	FireBulletsInfo_t()
	{
		m_iShots = 1;
		m_vecSpread.Init( 0, 0, 0 );
		m_flDistance = 8192;
		m_iTracerFreq = 4;
		m_flDamage = 0.0f;
		m_flPlayerDamage = 0.0f;
		m_pAttacker = NULL;
		m_nFlags = 0;
		m_pAdditionalIgnoreEnt = NULL;
		m_flDamageForceScale = 1.0f;

#ifdef _DEBUG
		m_iAmmoType = -1;
		m_vecSrc.Init( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
		m_vecDirShooting.Init( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
#endif
		m_bPrimaryAttack = true;
	}

	FireBulletsInfo_t( int nShots, const Vector &vecSrc, const Vector &vecDir, const Vector &vecSpread, float flDistance, int nAmmoType, bool bPrimaryAttack = true )
	{
		m_iShots = nShots;
		m_vecSrc = vecSrc;
		m_vecDirShooting = vecDir;
		m_vecSpread = vecSpread;
		m_flDistance = flDistance;
		m_iAmmoType = nAmmoType;
		m_iTracerFreq = 4;
		m_flDamage = 0;
		m_flPlayerDamage = 0;
		m_pAttacker = NULL;
		m_nFlags = 0;
		m_pAdditionalIgnoreEnt = NULL;
		m_flDamageForceScale = 1.0f;
		m_bPrimaryAttack = bPrimaryAttack;
	}

	int m_iShots;
	Vector m_vecSrc;
	Vector m_vecDirShooting;
	Vector m_vecSpread;
	float m_flDistance;
	int m_iAmmoType;
	int m_iTracerFreq;
	float m_flDamage;
	float m_flPlayerDamage;	// Damage to be used instead of m_flDamage if we hit a player
	int m_nFlags;			// See FireBulletsFlags_t
	float m_flDamageForceScale;
	CBaseEntity *m_pAttacker;
	CBaseEntity *m_pAdditionalIgnoreEnt;
	bool m_bPrimaryAttack;
};

//-----------------------------------------------------------------------------
// Purpose: Data for making the MOVETYPE_STEP entities appear to simulate every frame
//  We precompute the simulation and then meter it out each tick during networking of the 
//  entities origin and orientation.  Uses a bit more bandwidth, but it solves the NPCs interacting
//  with elevators/lifts bugs.
//-----------------------------------------------------------------------------
struct StepSimulationStep
{
	int			nTickCount;
	Vector		vecOrigin;
	Quaternion	qRotation;
};

struct StepSimulationData
{
	// Are we using the Step Simulation Data
	bool		m_bOriginActive;
	bool		m_bAnglesActive;

	// This is the pre-pre-Think position, orientation (Quaternion) and tick count
	StepSimulationStep	m_Previous2;

	// This is the pre-Think position, orientation (Quaternion) and tick count
	StepSimulationStep	m_Previous;

	// This is a potential mid-think position, orientation (Quaternion) and tick count
	// Used to mark motion discontinuities that happen between thinks
	StepSimulationStep	m_Discontinuity;

	// This is the goal or post-Think position and orientation (and Quaternion for blending) and next think time tick
	StepSimulationStep	m_Next;
	QAngle		m_angNextRotation;

	// This variable is used so that we only compute networked origin/angles once per tick
	int			m_nLastProcessTickCount;
	// The computed/interpolated network origin/angles to use
	Vector		m_vecNetworkOrigin;
	int			m_networkCell[3];
	QAngle		m_angNetworkAngles;
};

//-----------------------------------------------------------------------------
// Purpose: Simple state tracking for changing model sideways shrinkage during barnacle swallow
//-----------------------------------------------------------------------------
struct ModelScale
{
	float		m_flModelScaleStart;
	float		m_flModelScaleGoal;
	float		m_flModelScaleFinishTime;
	float		m_flModelScaleStartTime;
};

#include "soundflags.h"

struct CSoundParameters;
typedef short HSOUNDSCRIPTHANDLE;
//-----------------------------------------------------------------------------
// Purpose: Aggregates and sets default parameters for EmitSound function calls
//-----------------------------------------------------------------------------
struct EmitSound_t
{
	EmitSound_t() :
		m_nChannel( 0 ),
		m_pSoundName( 0 ),
		m_flVolume( VOL_NORM ),
		m_SoundLevel( SNDLVL_NONE ),
		m_nFlags( 0 ),
		m_nPitch( PITCH_NORM ),
		m_pOrigin( 0 ),
		m_flSoundTime( 0.0f ),
		m_pflSoundDuration( 0 ),
		m_bEmitCloseCaption( true ),
		m_bWarnOnMissingCloseCaption( false ),
		m_bWarnOnDirectWaveReference( false ),
		m_nSpeakerEntity( -1 ),
		m_UtlVecSoundOrigin(),
		m_hSoundScriptHandle( -1 )
	{
	}

	EmitSound_t( const CSoundParameters &src );

	int							m_nChannel;
	char const					*m_pSoundName;
	float						m_flVolume;
	soundlevel_t				m_SoundLevel;
	int							m_nFlags;
	int							m_nPitch;
	const Vector				*m_pOrigin;
	float						m_flSoundTime; ///< NOT DURATION, but rather, some absolute time in the future until which this sound should be delayed
	float						*m_pflSoundDuration;
	bool						m_bEmitCloseCaption;
	bool						m_bWarnOnMissingCloseCaption;
	bool						m_bWarnOnDirectWaveReference;
	int							m_nSpeakerEntity;
	mutable CUtlVector< Vector >	m_UtlVecSoundOrigin;  ///< Actual sound origin(s) (can be multiple if sound routed through speaker entity(ies) )
	mutable HSOUNDSCRIPTHANDLE		m_hSoundScriptHandle;
};

#define MAX_ACTORS_IN_SCENE 16

//-----------------------------------------------------------------------------
// Multiplayer specific defines
//-----------------------------------------------------------------------------
#define MAX_CONTROL_POINTS			8
#define MAX_CONTROL_POINT_GROUPS	8

// Maximum number of points that a control point may need owned to be cappable
#define MAX_PREVIOUS_POINTS			3

// The maximum number of teams the control point system knows how to deal with
#define MAX_CONTROL_POINT_TEAMS		8

// Maximum length of the cap layout string
#define MAX_CAPLAYOUT_LENGTH		32

// Maximum length of the current round printname
#define MAX_ROUND_NAME				32

// Maximum length of the current round name
#define MAX_ROUND_IMAGE_NAME		64

// Score added to the team score for a round win
#define TEAMPLAY_ROUND_WIN_SCORE	1

enum
{
	CP_WARN_NORMAL = 0,
	CP_WARN_FINALCAP,
	CP_WARN_NO_ANNOUNCEMENTS
};

// YWB:  3/12/2007
// Changing the following #define for Prediction Error checking (See gamemovement.cpp for overview) will to 1 or 2 enables the system, 0 turns it off
// Level 1 enables it, but doesn't force "full precision" networking, so you can still get lots of errors in position/velocity/etc.
// Level 2 enables it but also forces origins/angles to be sent full precision, so other fields can be error / tolerance checked
// NOTE:  This stuff only works on a listen server since it punches a hole from the client .dll to server .dll!!!
#define PREDICTION_ERROR_CHECK_LEVEL 0

// Set to 1 to spew a call stack in DiffPrint() for the existing side when the other side is missing
#define PREDICTION_ERROR_CHECK_STACKS_FOR_MISSING 0

//-----------------------------------------------------------------------------
// Round timer states
//-----------------------------------------------------------------------------
enum
{
	RT_STATE_SETUP,		// Timer is in setup mode
	RT_STATE_NORMAL,	// Timer is in normal mode
};

enum
{
	SIMULATION_TIME_WINDOW_BITS = 8,
};

//-----------------------------------------------------------------------------
// Commentary Mode
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Cell origin values
//-----------------------------------------------------------------------------
#define CELL_COUNT( bits ) ( (MAX_COORD_INTEGER*2) / (1 << (bits)) ) // How many cells on an axis based on the bit size of the cell
#define CELL_COUNT_BITS( bits ) MINIMUM_BITS_NEEDED( CELL_COUNT( bits ) ) // How many bits are necessary to respresent that cell
#define CELL_BASEENTITY_ORIGIN_CELL_BITS 5 // default amount of entropy bits for base entity


// The player's method of starting / stopping commentary
#ifdef GAME_HAS_NO_USE_KEY
#define COMMENTARY_BUTTONS		(IN_ATTACK | IN_ATTACK2 | IN_USE)
#else
#define COMMENTARY_BUTTONS		(IN_USE)
#endif

bool IsHeadTrackingEnabled();

// If this is defined, all of the scopeguard objects are NULL'd out to reduce overhead
// #define SPLIT_SCREEN_STUBS


	#define MAX_SPLITSCREEN_PLAYERS 1


inline bool IsSplitScreenSupported()
{
	return ( MAX_SPLITSCREEN_PLAYERS > 1 ) ? true : false;
}

//-----------------------------------------------------------------------------
// For invalidate physics recursive
//-----------------------------------------------------------------------------
enum InvalidatePhysicsBits_t
{
	POSITION_CHANGED	= 0x1,
	ANGLES_CHANGED		= 0x2,
	VELOCITY_CHANGED	= 0x4,
	ANIMATION_CHANGED	= 0x8,		// Means cycle has changed, or any other event which would cause render-to-texture shadows to need to be rerendeded
	BOUNDS_CHANGED		= 0x10,		// Means render bounds have changed, so shadow decal projection is required, etc.
	SEQUENCE_CHANGED	= 0x20,		// Means sequence has changed, only interesting when surrounding bounds depends on sequence																				
};

enum Class_T
{
	CLASS_NONE = 0,
	CLASS_PLAYER,
	CLASS_PLAYER_ALLY,
	CLASS_PLAYER_ALLY_VITAL,
	CLASS_ANTLION,
	CLASS_BARNACLE,
	CLASS_BLOB,
	CLASS_BULLSEYE,
	//CLASS_BULLSQUID,	
	CLASS_CITIZEN_PASSIVE,	
	CLASS_CITIZEN_REBEL,
	CLASS_COMBINE,
	CLASS_COMBINE_GUNSHIP,
	CLASS_CONSCRIPT,
	CLASS_HEADCRAB,
	//CLASS_HOUNDEYE,
	CLASS_MANHACK,
	CLASS_METROPOLICE,		
	CLASS_MILITARY,		
	CLASS_SCANNER,		
	CLASS_STALKER,		
	CLASS_VORTIGAUNT,
	CLASS_ZOMBIE,
	CLASS_PROTOSNIPER,
	CLASS_MISSILE,
	CLASS_FLARE,
	CLASS_EARTH_FAUNA,
	CLASS_HACKED_ROLLERMINE,
	CLASS_COMBINE_HUNTER,

	LAST_SHARED_ENTITY_CLASS,
};

// Factions
#define FACTION_NONE				0					// Not assigned a faction.  Entities not assigned a faction will not do faction tests.
#define LAST_SHARED_FACTION			(FACTION_NONE)
#define NUM_SHARED_FACTIONS			(FACTION_NONE + 1)

#endif // SHAREDDEFS_H
