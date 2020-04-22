//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== tf_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "tf_gamerules.h"
#include "EntityList.h"
#include "physics.h"
#include "game.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "tf_player.h"
#include "menu_base.h"
#include "shake.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void Host_Say( edict_t *pEdict, bool teamonly );
extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );
void InitializeMenus( void );
void DestroyMenus( void );
void Bot_RunAll( void );

extern bool			g_fGameOver;

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CBaseTFPlayer *pPlayer = CBaseTFPlayer::CreatePlayer( "player", pEdict );
	pPlayer->InitialSpawn();
	pPlayer->SetPlayerName( playername );
	pPlayer->Spawn();
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
		return "TeamFortress 2";
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	// Materials used by the client effects
	CBaseEntity::PrecacheModel( "sprites/white.vmt" );
	CBaseEntity::PrecacheModel( "sprites/physbeam.vmt" );
	CBaseEntity::PrecacheModel( "effects/human_object_glow.vmt" );

	// Precache player models
	CBaseEntity::PrecacheModel( "models/player/alien_commando.mdl" );
	CBaseEntity::PrecacheModel( "models/player/human_commando.mdl" );
	CBaseEntity::PrecacheModel( "models/player/human_defender.mdl");
	CBaseEntity::PrecacheModel( "models/player/medic.mdl" );
	CBaseEntity::PrecacheModel( "models/player/recon.mdl" );
	CBaseEntity::PrecacheModel( "models/player/human_medic.mdl" );
	CBaseEntity::PrecacheModel( "models/player/alien_escort.mdl" );
	CBaseEntity::PrecacheModel( "models/player/defender.mdl" );
	CBaseEntity::PrecacheModel( "models/player/technician.mdl" );
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			((CBaseTFPlayer *)pEdict)->CreateCorpse();
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
	VPROF("GameStartFrame()");

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
	InitializeMenus();

	// teamplay
	CreateGameRulesObject( "CTeamFortress" );
}

void FinishClientPutInServer( CBaseTFPlayer *pPlayer )
{
	pPlayer->InitialSpawn();
	pPlayer->Spawn();

	// When the player first joins the server, they
	pPlayer->m_lifeState = LIFE_DEAD;
	pPlayer->AddEffects( EF_NODRAW );
	pPlayer->ChangeTeam( TEAM_UNASSIGNED );
	pPlayer->SetThink( NULL );

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
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>" );
}

void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( CBaseEntity::Instance( pEdict ) );
	FinishClientPutInServer( pPlayer );
}
