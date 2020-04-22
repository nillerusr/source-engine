//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_shareddefs.h"
#include "engine/IEngineSound.h"
#include "KeyValues.h"

#include "bot.h"
#include "bot_util.h"
#include "bot_profile.h"

#include "cs_bot.h"
#include <ctype.h>
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int s_iBeamSprite = 0;

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given name is already in use by another player
 */
bool UTIL_IsNameTaken( const char *name, bool ignoreHumans )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (player->IsPlayer() && player->IsBot())
		{
			// bots can have prefixes so we need to check the name
			// against the profile name instead.
			CCSBot *bot = dynamic_cast<CCSBot *>(player);
			if ( bot && bot->GetProfile()->GetName() && FStrEq(name, bot->GetProfile()->GetName()))
			{
				return true;
			}
		}
		else
		{
			if (!ignoreHumans)
			{
				if (FStrEq( name, player->GetPlayerName() ))
					return true;
			}
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
int UTIL_ClientsInGame( void )
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if (player == NULL)
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the number of non-bots on the given team
 */
int UTIL_HumansOnTeam( int teamID, bool isAlive )
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );

		if ( entity == NULL )
			continue;

		CBasePlayer *player = static_cast<CBasePlayer *>( entity );

		if (player->IsBot())
			continue;

		if (player->GetTeamNumber() != teamID)
			continue;

		if (isAlive && !player->IsAlive())
			continue;

		count++;
	}

	return count;
}


//--------------------------------------------------------------------------------------------------------------
int UTIL_BotsInGame( void )
{
	int count = 0;

	for (int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));

		if ( player == NULL )
			continue;

		if ( !player->IsBot() )
			continue;

		count++;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Kick a bot from the given team. If no bot exists on the team, return false.
 */
bool UTIL_KickBotFromTeam( int kickTeam )
{
	int i;

	// try to kick a dead bot first
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (!player->IsBot())
			continue;	

		if (!player->IsAlive() && player->GetTeamNumber() == kickTeam)
		{
			// its a bot on the right team - kick it
			engine->ServerCommand( UTIL_VarArgs( "kick \"%s\"\n", player->GetPlayerName() ) );

			return true;
		}
	}

	// no dead bots, kick any bot on the given team
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (!player->IsBot())
			continue;	

		if (player->GetTeamNumber() == kickTeam)
		{
			// its a bot on the right team - kick it
			engine->ServerCommand( UTIL_VarArgs( "kick \"%s\"\n", player->GetPlayerName() ) );

			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if all of the members of the given team are bots
 */
bool UTIL_IsTeamAllBots( int team )
{
	int botCount = 0;

	for( int i=1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		// skip players on other teams
		if (player->GetTeamNumber() != team)
			continue;

		// if not a bot, fail the test
		if (!player->IsBot())
			return false;

		// is a bot on given team
		++botCount;
	}

	// if team is empty, there are no bots
	return (botCount) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest active player to the given position.
 * If 'distance' is non-NULL, the distance to the closest player is returned in it.
 */
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, float *distance )
{
	CBasePlayer *closePlayer = NULL;
	float closeDistSq = 999999999999.9f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (!IsEntityValid( player ))
			continue;

		if (!player->IsAlive())
			continue;

		Vector playerOrigin = GetCentroid( player );
		float distSq = (playerOrigin - pos).LengthSqr();
		if (distSq < closeDistSq)
		{
			closeDistSq = distSq;
			closePlayer = static_cast<CBasePlayer *>( player );
		}
	}
	
	if (distance)
		*distance = (float)sqrt( closeDistSq );

	return closePlayer;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the closest active player on the given team to the given position.
 * If 'distance' is non-NULL, the distance to the closest player is returned in it.
 */
extern CBasePlayer *UTIL_GetClosestPlayer( const Vector &pos, int team, float *distance )
{
	CBasePlayer *closePlayer = NULL;
	float closeDistSq = 999999999999.9f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (!IsEntityValid( player ))
			continue;

		if (!player->IsAlive())
			continue;

		if (player->GetTeamNumber() != team)
			continue;

		Vector playerOrigin = GetCentroid( player );
		float distSq = (playerOrigin - pos).LengthSqr();
		if (distSq < closeDistSq)
		{
			closeDistSq = distSq;
			closePlayer = static_cast<CBasePlayer *>( player );
		}
	}
	
	if (distance)
		*distance = (float)sqrt( closeDistSq );

	return closePlayer;
}

//--------------------------------------------------------------------------------------------------------------
// Takes the bot pointer and constructs the net name using the current bot name prefix.
void UTIL_ConstructBotNetName( char *name, int nameLength, const BotProfile *profile )
{
	if (profile == NULL)
	{
		name[0] = 0;
		return;
	}

	// if there is no bot prefix just use the profile name.
	if ((cv_bot_prefix.GetString() == NULL) || (strlen(cv_bot_prefix.GetString()) == 0))
	{
		Q_strncpy( name, profile->GetName(), nameLength );
		return;
	}

	// find the highest difficulty
	const char *diffStr = BotDifficultyName[0];
	for ( int i=BOT_EXPERT; i>0; --i )
	{
		if ( profile->IsDifficulty( (BotDifficultyType)i ) )
		{
			diffStr = BotDifficultyName[i];
			break;
		}
	}

	const char *weaponStr = NULL;
	if ( profile->GetWeaponPreferenceCount() )
	{
		weaponStr = profile->GetWeaponPreferenceAsString( 0 );

		const char *translatedAlias = GetTranslatedWeaponAlias( weaponStr );

		char wpnName[128];
		Q_snprintf( wpnName, sizeof( wpnName ), "weapon_%s", translatedAlias );
		WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );
		if ( hWpnInfo != GetInvalidWeaponInfoHandle() )
		{
			CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
			if ( pWeaponInfo )
			{
				CSWeaponType weaponType = pWeaponInfo->m_WeaponType;
				weaponStr = WeaponClassAsString( weaponType );
			}
		}
	}
	if ( !weaponStr )
	{
		weaponStr = "";
	}

	char skillStr[16];
	Q_snprintf( skillStr, sizeof( skillStr ), "%.0f", profile->GetSkill()*100 );

	char temp[MAX_PLAYER_NAME_LENGTH*2];
	char prefix[MAX_PLAYER_NAME_LENGTH*2];
	Q_strncpy( temp, cv_bot_prefix.GetString(), sizeof( temp ) );
	Q_StrSubst( temp, "<difficulty>", diffStr, prefix, sizeof( prefix ) );
	Q_StrSubst( prefix, "<weaponclass>", weaponStr, temp, sizeof( temp ) );
	Q_StrSubst( temp, "<skill>", skillStr, prefix, sizeof( prefix ) );
	Q_snprintf( name, nameLength, "%s %s", prefix, profile->GetName() );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if anyone on the given team can see the given spot
 */
bool UTIL_IsVisibleToTeam( const Vector &spot, int team )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		if (!player->IsAlive())
			continue;

		if (player->GetTeamNumber() != team)
			continue;

		trace_t result;
		UTIL_TraceLine( player->EyePosition(), spot, CONTENTS_SOLID, player, COLLISION_GROUP_NONE, &result );

		if (result.fraction == 1.0f)
			return true;
	}

	return false;
}


//------------------------------------------------------------------------------------------------------------
void UTIL_DrawBeamFromEnt( int i, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue )
{
/* BOTPORT: What is the replacement for MESSAGE_BEGIN?
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecEnd );   // vecEnd = origin???
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( i );
					WRITE_COORD( vecEnd.x );
					WRITE_COORD( vecEnd.y );
					WRITE_COORD( vecEnd.z );
					WRITE_SHORT( s_iBeamSprite );
					WRITE_BYTE( 0 );		 // startframe
					WRITE_BYTE( 0 );		 // framerate
					WRITE_BYTE( iLifetime ); // life
					WRITE_BYTE( 10 );		 // width
					WRITE_BYTE( 0 );		 // noise
					WRITE_BYTE( bRed );		 // r, g, b
					WRITE_BYTE( bGreen );		 // r, g, b
					WRITE_BYTE( bBlue );    // r, g, b
					WRITE_BYTE( 255 );	 // brightness
					WRITE_BYTE( 0 );		 // speed
					MESSAGE_END();
					*/
}


//------------------------------------------------------------------------------------------------------------
void UTIL_DrawBeamPoints( Vector vecStart, Vector vecEnd, int iLifetime, byte bRed, byte bGreen, byte bBlue )
{
	NDebugOverlay::Line( vecStart, vecEnd, bRed, bGreen, bBlue, true, 0.1f );

	/*
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecStart );
					WRITE_BYTE( TE_BEAMPOINTS );
					WRITE_COORD( vecStart.x );
					WRITE_COORD( vecStart.y );
					WRITE_COORD( vecStart.z );
					WRITE_COORD( vecEnd.x );
					WRITE_COORD( vecEnd.y );
					WRITE_COORD( vecEnd.z );
					WRITE_SHORT( s_iBeamSprite );
					WRITE_BYTE( 0 );		 // startframe
					WRITE_BYTE( 0 );		 // framerate
					WRITE_BYTE( iLifetime ); // life
					WRITE_BYTE( 10 );		 // width
					WRITE_BYTE( 0 );		 // noise
					WRITE_BYTE( bRed );		 // r, g, b
					WRITE_BYTE( bGreen );		 // r, g, b
					WRITE_BYTE( bBlue );    // r, g, b
					WRITE_BYTE( 255 );	 // brightness
					WRITE_BYTE( 0 );		 // speed
					MESSAGE_END();
					*/
}


//------------------------------------------------------------------------------------------------------------
void CONSOLE_ECHO( const char * pszMsg, ... )
{
	va_list     argptr;
	static char szStr[1024];

	va_start( argptr, pszMsg );
	vsprintf( szStr, pszMsg, argptr );
	va_end( argptr );

	Msg( "%s", szStr );
}


//------------------------------------------------------------------------------------------------------------
void BotPrecache( void )
{
	s_iBeamSprite = CBaseEntity::PrecacheModel( "sprites/smoke.spr" );
}

//------------------------------------------------------------------------------------------------------------
#define COS_TABLE_SIZE 256
static float cosTable[ COS_TABLE_SIZE ];

void InitBotTrig( void )
{
	for( int i=0; i<COS_TABLE_SIZE; ++i )
	{
		float angle = (float)(2.0f * M_PI * i / (float)(COS_TABLE_SIZE-1));
		cosTable[i] = (float)cos( angle ); 
	}
}

float BotCOS( float angle )
{
	angle = AngleNormalizePositive( angle );
	int i = (int)( angle * (COS_TABLE_SIZE-1) / 360.0f );
	return cosTable[i];
}

float BotSIN( float angle )
{
	angle = AngleNormalizePositive( angle - 90 );
	int i = (int)( angle * (COS_TABLE_SIZE-1) / 360.0f );
	return cosTable[i];
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Send a "hint" message to all players, dead or alive.
 */
void HintMessageToAllPlayers( const char *message )
{
	hudtextparms_t textParms;

	textParms.x = -1.0f;
	textParms.y = -1.0f;
	textParms.fadeinTime = 1.0f;
	textParms.fadeoutTime = 5.0f;
	textParms.holdTime = 5.0f;
	textParms.fxTime = 0.0f;
	textParms.r1 = 100;
	textParms.g1 = 255;
	textParms.b1 = 100;
	textParms.r2 = 255;
	textParms.g2 = 255;
	textParms.b2 = 255;
	textParms.effect = 0;
	textParms.channel = 0;

	UTIL_HudMessageAll( textParms, message );
}

//--------------------------------------------------------------------------------------------------------------------
/**
 * Return true if moving from "start" to "finish" will cross a player's line of fire.
 * The path from "start" to "finish" is assumed to be a straight line.
 * "start" and "finish" are assumed to be points on the ground.
 */
bool IsCrossingLineOfFire( const Vector &start, const Vector &finish, CBaseEntity *ignore, int ignoreTeam  )
{
	for ( int p=1; p <= gpGlobals->maxClients; ++p )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( p ) );

		if (!IsEntityValid( player ))
			continue;

		if (player == ignore)
			continue;

		if (!player->IsAlive())
			continue;

		if (ignoreTeam && player->GetTeamNumber() == ignoreTeam)
			continue;

		// compute player's unit aiming vector 
		Vector viewForward;
		AngleVectors( player->EyeAngles() + player->GetPunchAngle(), &viewForward );

		const float longRange = 5000.0f;
		Vector playerOrigin = GetCentroid( player );
		Vector playerTarget = playerOrigin + longRange * viewForward;

		Vector result( 0, 0, 0 );
		if (IsIntersecting2D( start, finish, playerOrigin, playerTarget, &result ))
		{
			// simple check to see if intersection lies in the Z range of the path
			float loZ, hiZ;

			if (start.z < finish.z)
			{
				loZ = start.z;
				hiZ = finish.z;
			}
			else
			{
				loZ = finish.z;
				hiZ = start.z;
			}

			if (result.z >= loZ && result.z <= hiZ + HumanHeight)
				return true;
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
* Performs a simple case-insensitive string comparison, honoring trailing * wildcards
*/
bool WildcardMatch( const char *query, const char *test )
{
	if ( !query || !test )
		return false;

	while ( *test && *query )
	{
		char nameChar = *test;
		char queryChar = *query;
		if ( tolower(nameChar) != tolower(queryChar) ) // case-insensitive
			break;
		++test;
		++query;
	}

	if ( *query == 0 && *test == 0 )
		return true;

	// Support trailing *
	if ( *query == '*' )
		return true;

	return false;
}



