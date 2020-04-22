//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TFC_SHAREDDEFS_H
#define TFC_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif


#include "shareddefs.h"


// Using MAP_DEBUG mode?
#ifdef MAP_DEBUG
	#define MDEBUG(x) x
#else
	#define MDEBUG(x)
#endif


#define TFC_PLAYER_VIEW_OFFSET	Vector( 0, 0, 53.5 )


// Team IDs.
//	#define TEAM_SPECTATOR	0 // This is set in shareddefs.h
#define	TEAM_BLUE		1
#define TEAM_RED		2
#define TEAM_YELLOW		3
#define TEAM_GREEN		4
#define TEAM_MAXCOUNT	5	// update this if we ever add teams (unlikely)


// Defines for the playerclass
#define PC_UNDEFINED	0 

#define PC_SCOUT		1 
#define PC_SNIPER		2 
#define PC_SOLDIER		3 
#define PC_DEMOMAN		4 
#define PC_MEDIC		5 
#define PC_HWGUY		6 
#define PC_PYRO			7
#define PC_SPY			8
#define PC_ENGINEER		9
#define PC_LAST_NORMAL_CLASS	9

#define PC_CIVILIAN		10		// Civilians are a special class. They cannot
								// be chosen by players, only enforced by maps
#define PC_LASTCLASS	11 		// Use this as the high-boundary for any loops
								// through the playerclass.


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


// Building types
#define BUILD_DISPENSER				1
#define BUILD_SENTRYGUN				2
#define BUILD_MORTAR				3
#define BUILD_TELEPORTER_ENTRY		4
#define BUILD_TELEPORTER_EXIT		5


// Grenade types.
enum GrenadeType_t
{
	GR_TYPE_NONE=0,
	GR_TYPE_NORMAL,
	GR_TYPE_CONCUSSION,
	GR_TYPE_NAIL,
	GR_TYPE_MIRV,
	GR_TYPE_NAPALM,
	GR_TYPE_GAS,
	GR_TYPE_EMP,
	GR_TYPE_CALTROP,
	NUM_GRENADE_TYPES
};

// Max amount of each grenade each player can carry.
// This is in addition to the player's
extern int g_nMaxGrenades[NUM_GRENADE_TYPES];


// These are the names of the ammo types that go in the CAmmoDefs and that the 
// weapon script files reference.
// These directly correspond to ammo_X in the goldsrc tfc code.
#define TFC_AMMO_DUMMY			0	// This is a dummy index, to make the CAmmoDef indices correct for the other ammo types.

#define TFC_AMMO_SHELLS			1
#define TFC_AMMO_SHELLS_NAME	"TFC_AMMO_SHELLS"

#define TFC_AMMO_NAILS			2
#define TFC_AMMO_NAILS_NAME		"TFC_AMMO_NAILS"

#define TFC_AMMO_ROCKETS		3
#define TFC_AMMO_ROCKETS_NAME	"TFC_AMMO_ROCKETS"

#define TFC_AMMO_CELLS			4
#define TFC_AMMO_CELLS_NAME		"TFC_AMMO_CELLS"

#define TFC_AMMO_MEDIKIT		5
#define TFC_AMMO_MEDIKIT_NAME	"TFC_AMMO_MEDIKIT"

#define TFC_AMMO_DETPACK		6
#define TFC_AMMO_DETPACK_NAME	"TFC_AMMO_DETPACK"

#define TFC_AMMO_GRENADES1		7
#define TFC_AMMO_GRENADES1_NAME "TFC_AMMO_GRENADES1"

#define TFC_AMMO_GRENADES2		8
#define TFC_AMMO_GRENADES2_NAME	"TFC_AMMO_GRENADES2"

#define TFC_NUM_AMMO_TYPES		9	// NOTE: KEEP THESE UP TO DATE WITH g_AmmoTypeNames[]
extern const char* g_AmmoTypeNames[TFC_NUM_AMMO_TYPES];



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
#define TFSTATE_CANT_MOVE		0x010000  // set when player is setting a detpack
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
#define IT_NAILS				(1<<9) 
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


enum TFCTimer_t
{
	TF_TIMER_ANY=0,
	TF_TIMER_CONCUSSION,
	TF_TIMER_INFECTION,
	TF_TIMER_HALLUCINATION,
	TF_TIMER_TRANQUILISATION,
	TF_TIMER_ROTHEALTH,
	TF_TIMER_REGENERATION,
	TF_TIMER_GRENPRIME,
	TF_TIMER_CELLREGENERATION,
	TF_TIMER_DETPACKSET,
	TF_TIMER_DETPACKDISARM,
	TF_TIMER_BUILD,
	TF_TIMER_CHECKBUILDDISTANCE,
	TF_TIMER_DISGUISE,
	TF_TIMER_DISPENSERREFILL,

	// Non player timers.
	TF_TIMER_RETURNITEM,
	TF_TIMER_DELAYEDGOAL,
	TF_TIMER_ENDROUND
};



/*==================================================*/
/* New Weapon Related Defines		                */
/*==================================================*/

// Medikit
#define WEAP_MEDIKIT_OVERHEAL 50 // Amount of superhealth over max_health the medikit will dispense
#define WEAP_MEDIKIT_HEAL	200  // Amount medikit heals per hit



//--------------
// TFC Specific damage flags
//--------------
#define DMG_IGNORE_MAXHEALTH	(DMG_LASTGENERICFLAG<<1)
#define DMG_IGNOREARMOR			(DMG_LASTGENERICFLAG<<2)


// ------------------------------------------------------------------------------ //
// Info for each player class.
// ------------------------------------------------------------------------------ //

class CTFCPlayerClassInfo
{
public:
	const char *m_pClassName;
	const char *m_pModelName;	// What model this class uses.
	
	float m_flMaxSpeed;
	int m_iMaxHealth;
	
	int m_iInitArmor;
	int m_iMaxArmor;
	float m_flInitArmorType;
	float m_flMaxArmorType;
	int m_iInitArmorClass;
	int m_nArmorClasses;

	// What types of grenades does this guy carry?
	// GR_TYPE_ defines.
	GrenadeType_t m_iGrenadeType1;
	GrenadeType_t m_iGrenadeType2;
	
	int m_InitAmmo[TFC_NUM_AMMO_TYPES]; // These are in the same order as g_AmmoTypeNames.
	int m_MaxAmmo[TFC_NUM_AMMO_TYPES]; // These are in the same order as g_AmmoTypeNames.
};

const CTFCPlayerClassInfo* GetTFCClassInfo( int iClass );


// The various states the player can be in during the join game process.
enum TFCPlayerState
{
	// Happily running around in the game.
	// You can't move though if CSGameRules()->IsFreezePeriod() returns true.
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

	STATE_OBSERVER_MODE,

	STATE_DYING,

	TFC_NUM_PLAYER_STATES
};


#endif // TFC_SHAREDDEFS_H
