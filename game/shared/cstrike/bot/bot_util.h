//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BOT_UTIL_H
#define BOT_UTIL_H


#include "convar.h"
#include "util.h"

//--------------------------------------------------------------------------------------------------------------
enum PriorityType
{
	PRIORITY_LOW, PRIORITY_MEDIUM, PRIORITY_HIGH, PRIORITY_UNINTERRUPTABLE
};


extern ConVar cv_bot_traceview;
extern ConVar cv_bot_stop;
extern ConVar cv_bot_show_nav;
extern ConVar cv_bot_walk;
extern ConVar cv_bot_difficulty;
extern ConVar cv_bot_debug;
extern ConVar cv_bot_debug_target;
extern ConVar cv_bot_quota;
extern ConVar cv_bot_quota_mode;
extern ConVar cv_bot_prefix;
extern ConVar cv_bot_allow_rogues;
extern ConVar cv_bot_allow_pistols;
extern ConVar cv_bot_allow_shotguns;
extern ConVar cv_bot_allow_sub_machine_guns;
extern ConVar cv_bot_allow_rifles;
extern ConVar cv_bot_allow_machine_guns;
extern ConVar cv_bot_allow_grenades;
extern ConVar cv_bot_allow_snipers;
extern ConVar cv_bot_allow_shield;
extern ConVar cv_bot_join_team;
extern ConVar cv_bot_join_after_player;
extern ConVar cv_bot_auto_vacate;
extern ConVar cv_bot_zombie;
extern ConVar cv_bot_defer_to_human;
extern ConVar cv_bot_chatter;
extern ConVar cv_bot_profile_db;
extern ConVar cv_bot_dont_shoot;
extern ConVar cv_bot_eco_limit;
extern ConVar cv_bot_auto_follow;
extern ConVar cv_bot_flipout;

#define RAD_TO_DEG( deg ) ((deg) * 180.0 / M_PI)
#define DEG_TO_RAD( rad ) ((rad) * M_PI / 180.0)

#define SIGN( num )	      (((num) < 0) ? -1 : 1)
#define ABS( num )        (SIGN(num) * (num))


#define CREATE_FAKE_CLIENT		( *g_engfuncs.pfnCreateFakeClient )
#define GET_USERINFO			( *g_engfuncs.pfnGetInfoKeyBuffer )
#define SET_KEY_VALUE			( *g_engfuncs.pfnSetKeyValue )
#define SET_CLIENT_KEY_VALUE	( *g_engfuncs.pfnSetClientKeyValue )

class BotProfile;

extern void   BotPrecache( void );
extern int		UTIL_ClientsInGame( void );

extern bool UTIL_IsNameTaken( const char *name, bool ignoreHumans = false );		///< return true if given name is already in use by another player

#define IS_ALIVE true
extern int UTIL_HumansOnTeam( int teamID, bool isAlive = false );

extern int		UTIL_BotsInGame( void );
extern bool		UTIL_IsTeamAllBots( int team );
extern void		UTIL_DrawBeamFromEnt( int iIndex, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue );
extern void		UTIL_DrawBeamPoints( Vector vecStart, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue );
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, float *distance = NULL );
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, int team, float *distance = NULL );
extern bool UTIL_KickBotFromTeam( int kickTeam ); ///< kick a bot from the given team. If no bot exists on the team, return false.

extern bool UTIL_IsVisibleToTeam( const Vector &spot, int team ); ///< return true if anyone on the given team can see the given spot

/// return true if moving from "start" to "finish" will cross a player's line of fire.
extern bool IsCrossingLineOfFire( const Vector &start, const Vector &finish, CBaseEntity *ignore = NULL, int ignoreTeam = 0 );

extern void UTIL_ConstructBotNetName(char *name, int nameLength, const BotProfile *bot);	///< constructs a complete name including prefix

/**
 * Echos text to the console, and prints it on the client's screen.  This is NOT tied to the developer cvar.
 * If you are adding debugging output in cstrike, use UTIL_DPrintf() (debug.h) instead.
 */
extern void CONSOLE_ECHO( PRINTF_FORMAT_STRING const char * pszMsg, ... );

extern void InitBotTrig( void );
extern float BotCOS( float angle );
extern float BotSIN( float angle );

extern void HintMessageToAllPlayers( const char *message );

bool WildcardMatch( const char *query, const char *test );	///< Performs a simple case-insensitive string comparison, honoring trailing * wildcards

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given entity is valid
 */
inline bool IsEntityValid( CBaseEntity *entity )
{
	if (entity == NULL)
		return false;

	if (FNullEnt( entity->edict() ))
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given two line segments: startA to endA, and startB to endB, return true if they intesect
 * and put the intersection point in "result".
 * Note that this computes the intersection of the 2D (x,y) projection of the line segments.
 */
inline bool IsIntersecting2D( const Vector &startA, const Vector &endA, 
															const Vector &startB, const Vector &endB, 
															Vector *result = NULL )
{
	float denom = (endA.x - startA.x) * (endB.y - startB.y) - (endA.y - startA.y) * (endB.x - startB.x);
	if (denom == 0.0f)
	{
		// parallel
		return false;
	}

	float numS = (startA.y - startB.y) * (endB.x - startB.x) - (startA.x - startB.x) * (endB.y - startB.y);
	if (numS == 0.0f)
	{
		// coincident
		return true;
	}

	float numT = (startA.y - startB.y) * (endA.x - startA.x) - (startA.x - startB.x) * (endA.y - startA.y);

	float s = numS / denom;
	if (s < 0.0f || s > 1.0f)
	{
		// intersection is not within line segment of startA to endA
		return false;
	}

	float t = numT / denom;
	if (t < 0.0f || t > 1.0f)
	{
		// intersection is not within line segment of startB to endB
		return false;
	}

	// compute intesection point
	if (result)
		*result = startA + s * (endA - startA);

	return true;
}


#endif
