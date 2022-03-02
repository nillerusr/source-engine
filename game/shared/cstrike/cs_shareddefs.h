//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared CS definitions.
//
//=============================================================================//

#ifndef CS_SHAREDDEFS_H
#define CS_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

/*======================*/
//      Menu stuff      //
/*======================*/

#include <game/client/iviewport.h>

//=============================================================================
// HPE_BEGIN:
// Including Achievement ID Definitions
//=============================================================================

#include "cs_achievementdefs.h"

//=============================================================================
// HPE_END
//=============================================================================

// CS-specific viewport panels
#define PANEL_CLASS_CT				"class_ct"
#define PANEL_CLASS_TER				"class_ter"

// Buy sub menus
#define MENU_PISTOL					"menu_pistol"
#define MENU_SHOTGUN				"menu_shotgun"
#define MENU_RIFLE					"menu_rifle"
#define MENU_SMG					"menu_smg"
#define MENU_MACHINEGUN				"menu_mg"
#define MENU_EQUIPMENT				"menu_equip"


#define MAX_HOSTAGES				12
#define MAX_HOSTAGE_RESCUES			4



 
#define CSTRIKE_DEFAULT_AVATAR "avatar_default_64"
#define CSTRIKE_DEFAULT_T_AVATAR "avatar_default-t_64"
#define CSTRIKE_DEFAULT_CT_AVATAR "avatar_default_64"

extern const float CS_PLAYER_SPEED_RUN;
extern const float CS_PLAYER_SPEED_VIP;
extern const float CS_PLAYER_SPEED_WALK;
extern const float CS_PLAYER_SPEED_SHIELD;
extern const float CS_PLAYER_SPEED_STOPPED;
extern const float CS_PLAYER_SPEED_OBSERVER;

extern const float CS_PLAYER_SPEED_DUCK_MODIFIER;
extern const float CS_PLAYER_SPEED_WALK_MODIFIER;
extern const float CS_PLAYER_SPEED_CLIMB_MODIFIER;


template< class T >
class CUtlVectorInitialized : public CUtlVector< T >
{
public:
	CUtlVectorInitialized( T* pMemory, int numElements ) : CUtlVector< T >( pMemory, numElements )
	{
		this->SetSize( numElements );
	}
};

extern CUtlVectorInitialized< const char * > CTPlayerModels;
extern CUtlVectorInitialized< const char * > TerroristPlayerModels;


// These go in CCSPlayer::m_iAddonBits and get sent to the client so it can create
// grenade models hanging off players.
#define ADDON_FLASHBANG_1		0x001
#define ADDON_FLASHBANG_2		0x002
#define ADDON_HE_GRENADE		0x004
#define ADDON_SMOKE_GRENADE		0x008
#define ADDON_C4				0x010
#define ADDON_DEFUSEKIT			0x020
#define ADDON_PRIMARY			0x040
#define ADDON_PISTOL			0x080
#define ADDON_PISTOL2			0x100
#define NUM_ADDON_BITS			9


// Indices of each weapon slot.
#define WEAPON_SLOT_RIFLE		0	// (primary slot)
#define WEAPON_SLOT_PISTOL		1	// (secondary slot)
#define WEAPON_SLOT_KNIFE		2
#define WEAPON_SLOT_GRENADES	3
#define WEAPON_SLOT_C4			4

#define WEAPON_SLOT_FIRST		0
#define WEAPON_SLOT_LAST		4


// CS Team IDs.
#define TEAM_TERRORIST			2
#define	TEAM_CT					3
#define TEAM_MAXCOUNT			4	// update this if we ever add teams (unlikely)

#define MAX_CLAN_TAG_LENGTH		16	// max for new tags is actually 12, this allows some backward compat.

//=============================================================================
// HPE_BEGIN:
// [menglish] CS specific death animation time now that freeze cam is implemented
//			in order to linger on the players body less 
// [tj] The number of times you must kill a given player to be dominating them
// [menglish] Flags to use upon player death
//=============================================================================

// [menglish] CS specific death animation time now that freeze cam is implemented
//			in order to linger on the players body less 
#define CS_DEATH_ANIMATION_TIME			0.8
 
// [tj] The number of times you must kill a given player to be dominating them
// Should always be more than 1
#define CS_KILLS_FOR_DOMINATION         4

#define CS_DEATH_DOMINATION				0x0001	// killer is dominating victim
#define CS_DEATH_REVENGE				0x0002	// killer got revenge on victim
//=============================================================================
// HPE_END
//=============================================================================


//--------------
// CSPort Specific damage flags
//--------------
#define DMG_HEADSHOT		(DMG_LASTGENERICFLAG<<1)


// The various states the player can be in during the join game process.
enum CSPlayerState
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

	STATE_DEATH_ANIM,			// Playing death anim, waiting for that to finish.
	STATE_DEATH_WAIT_FOR_KEY,	// Done playing death anim. Waiting for keypress to go into observer mode.
	STATE_OBSERVER_MODE,		// Noclipping around, watching players, etc.
	NUM_PLAYER_STATES
};


enum e_RoundEndReason
{
    Invalid_Round_End_Reason = -1,
    Target_Bombed,
    VIP_Escaped,						
    VIP_Assassinated,
    Terrorists_Escaped,
    CTs_PreventEscape,
    Escaping_Terrorists_Neutralized,
    Bomb_Defused,
    CTs_Win,
    Terrorists_Win,
    Round_Draw,
    All_Hostages_Rescued,
    Target_Saved,
    Hostages_Not_Rescued,
    Terrorists_Not_Escaped,
    VIP_Not_Escaped,
    Game_Commencing,
    RoundEndReason_Count    
};

#define PUSHAWAY_THINK_INTERVAL		(1.0f / 20.0f)

enum
{
	CS_CLASS_NONE=0,

	// Terrorist classes (keep in sync with FIRST_T_CLASS/LAST_T_CLASS).
	CS_CLASS_PHOENIX_CONNNECTION,
	CS_CLASS_L337_KREW,
	CS_CLASS_ARCTIC_AVENGERS,
	CS_CLASS_GUERILLA_WARFARE,

	// CT classes (keep in sync with FIRST_CT_CLASS/LAST_CT_CLASS).
	CS_CLASS_SEAL_TEAM_6,
	CS_CLASS_GSG_9,
	CS_CLASS_SAS,
	CS_CLASS_GIGN,

	CS_NUM_CLASSES
};

//=============================================================================
// HPE_BEGIN:
// [menglish] List of equipment dropped by players with freeze panel callouts
//============================================================================= 
enum
{
	DROPPED_C4,
	DROPPED_DEFUSE,
	DROPPED_WEAPON,
	DROPPED_GRENADE, // This could be an HE greande, flashbang, or smoke
	DROPPED_COUNT
}; 
//=============================================================================
// HPE_END
//=============================================================================


//=============================================================================
//
// MVP reasons
//
enum CSMvpReason_t
{
	CSMVP_UNDEFINED = 0,
	CSMVP_ELIMINATION,
	CSMVP_BOMBPLANT,
	CSMVP_BOMBDEFUSE,
	CSMVP_HOSTAGERESCUE,
};


// Keep these in sync with CSClasses.
#define FIRST_T_CLASS	CS_CLASS_PHOENIX_CONNNECTION
#define LAST_T_CLASS	CS_CLASS_GUERILLA_WARFARE

#define FIRST_CT_CLASS	CS_CLASS_SEAL_TEAM_6
#define LAST_CT_CLASS	CS_CLASS_GIGN

#define CS_MUZZLEFLASH_NONE -1
#define CS_MUZZLEFLASH_NORM	0
#define CS_MUZZLEFLASH_X	1

class CCSClassInfo
{
public:
	const char		*m_pClassName;
};

const CCSClassInfo* GetCSClassInfo( int i );

extern const char *pszWinPanelCategoryHeaders[];

#endif // CS_SHAREDDEFS_H
