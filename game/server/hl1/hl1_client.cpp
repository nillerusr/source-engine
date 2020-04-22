//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== tf_client.cpp ========================================================

  HL1 client/server game specific stuff

*/

#include "cbase.h"
#include "hl1_player.h"
#include "hl1mp_player.h"
#include "hl1_gamerules.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"

#include "tier0/vprof.h"

void Host_Say( edict_t *pEdict, bool teamonly );

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	CHL1_Player *pPlayer = NULL;

	// Allocate a CBasePlayer for pev, and call spawn
    if ( g_pGameRules->IsMultiplayer() )
		pPlayer = CHL1_Player::CreatePlayer( "player_mp", pEdict );
	else
		pPlayer = CHL1_Player::CreatePlayer( "player", pEdict );

	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	CHL1_Player *pPlayer = dynamic_cast< CHL1_Player* >( CBaseEntity::Instance( pEdict ) );

	pPlayer->InitialSpawn();

	if ( !bLoadGame )
	{
		pPlayer->Spawn();
	}
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
		return "Half-Life 1";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
    // Multiplayer uses different models, and more of them.
    if ( g_pGameRules->IsMultiplayer() )
    {
        CBaseEntity::PrecacheModel("models/player/mp/barney/barney.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/gina/gina.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/gman/gman.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/gordon/gordon.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/helmet/helmet.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/hgrunt/hgrunt.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/robo/robo.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/scientist/scientist.mdl");
        CBaseEntity::PrecacheModel("models/player/mp/zombie/zombie.mdl");
        CBaseEntity::PrecacheModel("models/player.mdl" );                
    }
    else
    {
        CBaseEntity::PrecacheModel("models/player.mdl" );
    }

	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );

	CBaseEntity::PrecacheScriptSound( "Player.UseDeny" );
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			((CHL1MP_Player *)pEdict)->CreateCorpse();
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

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = (teamplay.GetInt() != 0);

#ifdef DEBUG
	extern void Bot_RunAll();
	Bot_RunAll();
#endif
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	engine->ServerCommand( "exec game.cfg\n" );
	engine->ServerExecute( );

	if ( !gpGlobals->deathmatch )
	{
		// generic half-life
		CreateGameRulesObject( "CHalfLife1" );
		return;
	}
	else
	{
		CreateGameRulesObject( "CHL1MPRules" );
		return;

		if ( teamplay.GetInt() > 0 )
		{
			// teamplay
			CreateGameRulesObject( "CTeamplayRules" );
			return;
		}

		// vanilla deathmatch
		CreateGameRulesObject( "CMultiplayRules" );
		return;
	}

	CreateGameRulesObject( "CHalfLife1" );
}

