/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== tf_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "shake.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "tf_player.h"
#include "tf_gamerules.h"
#include "tier0/vprof.h"
#include "tf_bot_temp.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );

extern bool			g_fGameOver;


void FinishClientPutInServer( CTFPlayer *pPlayer )
{
	{
		bool save = engine->LockNetworkStringTables( false );
	
		pPlayer->InitialSpawn();
		pPlayer->Spawn();
	
		engine->LockNetworkStringTables( save );
	}

	char sName[128];
	Q_strncpy( sName, pPlayer->GetPlayerName(), sizeof( sName ) );
	
	// First parse the name and remove any %'s
	for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
	{
		// Replace it with a space
		if ( *pApersand == '%' )
				*pApersand = ' ';
	}

	// notify other clients of player joining the game
	if ( !pPlayer->IsFakeClient() )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>" );
	}
}

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CTFPlayer *pPlayer = CTFPlayer::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CTFPlayer *pPlayer = ToTFPlayer( CBaseEntity::Instance( pEdict ) );
	FinishClientPutInServer( pPlayer );
}


/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Team Fortress";
}


//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	const char *pFilename = "scripts/client_precache.txt";
	KeyValues *pValues = new KeyValues( "ClientPrecache" );

	if ( !pValues->LoadFromFile( filesystem, pFilename, "GAME" ) )
	{
		Error( "Can't open %s for client precache info.", pFilename );
		pValues->deleteThis();
		return;
	}

	for ( KeyValues *pData = pValues->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		const char *pszType = pData->GetName();
		const char *pszFile = pData->GetString();

		if ( Q_strlen( pszType ) > 0 &&
			 Q_strlen( pszFile ) > 0 )
		{
			if ( !Q_stricmp( pData->GetName(), "model" ) )
			{
				CBaseEntity::PrecacheModel( pszFile );
			}
			else if ( !Q_stricmp( pData->GetName(), "scriptsound" ) )
			{
				CBaseEntity::PrecacheScriptSound( pszFile );
			}
		}
	}

	pValues->deleteThis();
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			dynamic_cast< CBasePlayer* >( pEdict )->CreateCorpse();
		}

		// respawn player
		pEdict->Spawn();
	}
	else
	{       // restart the entire server
		engine->ServerCommand("reload\n");
	}
}

void GameStartFrame( void )
{
	VPROF( "GameStartFrame" );

	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.GetInt() ? true : false;

	Bot_RunAll();
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	CreateGameRulesObject( "CTFGameRules" );
}
