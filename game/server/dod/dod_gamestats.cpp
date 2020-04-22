//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: dod game stats
//
// $NoKeywords: $
//=============================================================================//

// Some tricky business here - we don't want to include the precompiled header for the statreader
// and trying to #ifdef it out does funky things like ignoring the #endif. Define our header file
// separately and include it based on the switch

#include "cbase.h"

#ifdef GAME_DLL
	#include "weapon_dodbase.h"
#endif

#include <tier0/platform.h>
#include "dod_gamestats.h"

int iDistanceStatWeapons[DOD_NUM_DISTANCE_STAT_WEAPONS] = 
{
	WEAPON_COLT,
	WEAPON_P38,
	WEAPON_C96,
	WEAPON_GARAND,
	WEAPON_GARAND_ZOOMED,
	WEAPON_M1CARBINE,
	WEAPON_K98,
	WEAPON_K98_ZOOMED,
	WEAPON_SPRING,
	WEAPON_SPRING_ZOOMED,
	WEAPON_K98_SCOPED,
	WEAPON_K98_SCOPED_ZOOMED,
	WEAPON_THOMPSON,
	WEAPON_MP40,
	WEAPON_MP44,
	WEAPON_MP44_SEMIAUTO,
	WEAPON_BAR,
	WEAPON_BAR_SEMIAUTO,
	WEAPON_30CAL,
	WEAPON_30CAL_UNDEPLOYED,
	WEAPON_MG42,
	WEAPON_MG42_UNDEPLOYED,
};

// Send hit/shots only for the following weapons
int iNoDistStatWeapons[DOD_NUM_NODIST_STAT_WEAPONS] =
{
	WEAPON_AMERKNIFE,
	WEAPON_SPADE,
	WEAPON_BAZOOKA,
	WEAPON_PSCHRECK,
	WEAPON_FRAG_US,
	WEAPON_FRAG_GER,
	WEAPON_FRAG_US_LIVE,
	WEAPON_FRAG_GER_LIVE,
	WEAPON_RIFLEGREN_US,
	WEAPON_RIFLEGREN_GER,
	WEAPON_RIFLEGREN_US_LIVE,
	WEAPON_RIFLEGREN_GER_LIVE,
	WEAPON_THOMPSON_PUNCH,
	WEAPON_MP40_PUNCH,
};

int iWeaponBucketDistances[DOD_NUM_WEAPON_DISTANCE_BUCKETS-1] =
{
	50,
	150,
	300,
	450,
	700,
	1000,
	1300,
	1600,
	2000		
};

#ifndef GAME_DLL

	const char * s_WeaponAliasInfo[] = 
	{
		"none",	//	WEAPON_NONE = 0,

		//Melee
		"amerknife",	//WEAPON_AMERKNIFE,
		"spade",		//WEAPON_SPADE,

		//Pistols
		"colt",			//WEAPON_COLT,
		"p38",			//WEAPON_P38,
		"c96",			//WEAPON_C96

		//Rifles
		"garand",		//WEAPON_GARAND,
		"m1carbine",	//WEAPON_M1CARBINE,
		"k98",			//WEAPON_K98,

		//Sniper Rifles
		"spring",		//WEAPON_SPRING,
		"k98_scoped",	//WEAPON_K98_SCOPED,

		//SMG
		"thompson",		//WEAPON_THOMPSON,
		"mp40",			//WEAPON_MP40,
		"mp44",			//WEAPON_MP44,
		"bar",			//WEAPON_BAR,

		//Machine guns
		"30cal",		//WEAPON_30CAL,
		"mg42",			//WEAPON_MG42,

		//Rocket weapons
		"bazooka",		//WEAPON_BAZOOKA,
		"pschreck",		//WEAPON_PSCHRECK,

		//Grenades
		"frag_us",		//WEAPON_FRAG_US,
		"frag_ger",		//WEAPON_FRAG_GER,

		"frag_us_live",		//WEAPON_FRAG_US_LIVE
		"frag_ger_live",	//WEAPON_FRAG_GER_LIVE

		"smoke_us",			//WEAPON_SMOKE_US
		"smoke_ger",		//WEAPON_SMOKE_GER

		"riflegren_us",	//WEAPON_RIFLEGREN_US
		"riflegren_ger",	//WEAPON_RIFLEGREN_GER

		"riflegren_us_live",	//WEAPON_RIFLEGREN_US_LIVE
		"riflegren_ger_live",		//WEAPON_RIFLEGREN_GER_LIVE

		// not actually separate weapons, but defines used in stats recording
		"thompson_punch",		//WEAPON_THOMPSON_PUNCH
		"mp40_punch",			//WEAPON_MP40_PUNCH
		"garand_zoomed",		//WEAPON_GARAND_ZOOMED,	

		"k98_zoomed",			//WEAPON_K98_ZOOMED
		"spring_zoomed",		//WEAPON_SPRING_ZOOMED
		"k98_scoped_zoomed",	//WEAPON_K98_SCOPED_ZOOMED	

		"30cal_undeployed",		//WEAPON_30CAL_UNDEPLOYED,
		"mg42_undeployed",		//WEAPON_MG42_UNDEPLOYED,

		"bar_semiauto",			//WEAPON_BAR_SEMIAUTO,
		"mp44_semiauto",		//WEAPON_MP44_SEMIAUTO,

		0,		// end of list marker
	};
#endif