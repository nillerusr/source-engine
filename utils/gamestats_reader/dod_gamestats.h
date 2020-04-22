//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The dod game stats header
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_GAMESTATS_H
#define DOD_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

// Redefine some things for the stat reader so it doesn't have to include weapon_dodbase.h
#ifndef GAME_DLL

	typedef enum
	{
		WEAPON_NONE = 0,

		//Melee
		WEAPON_AMERKNIFE,
		WEAPON_SPADE,

		//Pistols
		WEAPON_COLT,
		WEAPON_P38,
		WEAPON_C96,

		//Rifles
		WEAPON_GARAND,
		WEAPON_M1CARBINE,
		WEAPON_K98,

		//Sniper Rifles
		WEAPON_SPRING,
		WEAPON_K98_SCOPED,

		//SMG
		WEAPON_THOMPSON,
		WEAPON_MP40,
		WEAPON_MP44,
		WEAPON_BAR,

		//Machine guns
		WEAPON_30CAL,
		WEAPON_MG42,

		//Rocket weapons
		WEAPON_BAZOOKA,
		WEAPON_PSCHRECK,

		//Grenades
		WEAPON_FRAG_US,
		WEAPON_FRAG_GER,

		WEAPON_FRAG_US_LIVE,
		WEAPON_FRAG_GER_LIVE,

		WEAPON_SMOKE_US,
		WEAPON_SMOKE_GER,

		WEAPON_RIFLEGREN_US,
		WEAPON_RIFLEGREN_GER,

		WEAPON_RIFLEGREN_US_LIVE,
		WEAPON_RIFLEGREN_GER_LIVE,

		// not actually separate weapons, but defines used in stats recording
		// find a better way to do this without polluting the list of actual weapons.
		WEAPON_THOMPSON_PUNCH,
		WEAPON_MP40_PUNCH,

		WEAPON_GARAND_ZOOMED,	
		WEAPON_K98_ZOOMED,
		WEAPON_SPRING_ZOOMED,
		WEAPON_K98_SCOPED_ZOOMED,

		WEAPON_30CAL_UNDEPLOYED,
		WEAPON_MG42_UNDEPLOYED,

		WEAPON_BAR_SEMIAUTO,
		WEAPON_MP44_SEMIAUTO,

		WEAPON_MAX,		// number of weapons weapon index

	} DODWeaponID;

#endif // ndef WEAPON_NONE

#define DOD_STATS_BLOB_VERSION 1

#define DOD_NUM_DISTANCE_STAT_WEAPONS	22
#define DOD_NUM_NODIST_STAT_WEAPONS	14
#define DOD_NUM_WEAPON_DISTANCE_BUCKETS	10

extern int iDistanceStatWeapons[DOD_NUM_DISTANCE_STAT_WEAPONS];
extern int iNoDistStatWeapons[DOD_NUM_NODIST_STAT_WEAPONS];
extern int iWeaponBucketDistances[DOD_NUM_WEAPON_DISTANCE_BUCKETS-1];

#ifndef GAME_DLL
	extern const char * s_WeaponAliasInfo[];
#endif

typedef struct
{
	char szGameName[8];
	byte iVersion;
	char szMapName[32];
	char ipAddr[4];
	short port;
	int serverid;
} gamestats_header_t;

// Stats for bullet weapons - includes distance of hits
typedef struct
{
	short iNumAttacks;		// times fired
	short iNumHits;			// times hit

	// distance buckets - distances are defined per-weapon ( 0 is closest, buckets-1 farthest )
	short iDistanceBuckets[DOD_NUM_WEAPON_DISTANCE_BUCKETS];

} dod_gamestats_weapon_distance_t;

// Stats for non-bullet weapons
typedef struct
{
	short iNumAttacks;		// times fired
	short iNumHits;			// times hit	
} dod_gamestats_weapon_nodist_t;

typedef struct
{
	gamestats_header_t	header;

	// Team Scores
	byte iNumAlliesWins;
	byte iNumAxisWins;

	short iAlliesTickPoints;
	short iAxisTickPoints;

	short iMinutesPlayed;			// time spent on the map rotation

	// Player Data
	short iMinutesPlayedPerClass_Allies[7];		// includes random
	short iMinutesPlayedPerClass_Axis[7];		// includes random

	short iKillsPerClass_Allies[6];
	short iKillsPerClass_Axis[6];

	short iSpawnsPerClass_Allies[6];
	short iSpawnsPerClass_Axis[6];

	short iCapsPerClass_Allies[6];
	short iCapsPerClass_Axis[6];

	byte iDefensesPerClass_Allies[6];
	byte iDefensesPerClass_Axis[6];

	// Server Settings
	// assume these class limits don't change through the course of the map
	byte iClassLimits_Allies[6];
	byte iClassLimits_Axis[6];

	// Weapon Data
	dod_gamestats_weapon_distance_t weaponStatsDistance[DOD_NUM_DISTANCE_STAT_WEAPONS];		// 14 * 22 = 308 bytes
	dod_gamestats_weapon_nodist_t weaponStats[DOD_NUM_NODIST_STAT_WEAPONS];					// 4 * 14 = 56 bytes

	// how many times a weapon was picked up ?

} dod_gamestats_t;

#endif // DOD_GAMESTATS_H