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
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "shake.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "dod_player.h"
#include "dod_gamerules.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );

extern bool			g_fGameOver;

void FinishClientPutInServer( CDODPlayer *pPlayer )
{
	pPlayer->InitialSpawn();
	pPlayer->Spawn();

	if (!pPlayer->IsBot())
	{
		// When the player first joins the server, they
		pPlayer->m_takedamage = DAMAGE_NO;
		pPlayer->pl.deadflag = true;
		pPlayer->m_lifeState = LIFE_DEAD;
		pPlayer->AddEffects( EF_NODRAW );
		pPlayer->SetThink( NULL );
		
		if ( 1 )
		{
			pPlayer->ChangeTeam( TEAM_UNASSIGNED );

			// Move them to the first intro camera.
			pPlayer->MoveToNextIntroCamera();
			pPlayer->SetMoveType( MOVETYPE_NONE );
		}
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
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>" );
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
	CDODPlayer *pPlayer = CDODPlayer::CreatePlayer( "player", pEdict );

	pPlayer->SetPlayerName( playername );
}


void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CDODPlayer *pPlayer = ToDODPlayer( CBaseEntity::Instance( pEdict ) );
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
		return "Day of Defeat";
}

int g_iHelmetModels[NUM_HELMETS];

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	// Materials used by the client effects
	CBaseEntity::PrecacheModel( "sprites/white.vmt" );
	CBaseEntity::PrecacheModel( "sprites/physbeam.vmt" );

	for( int i=0;i<NUM_HELMETS;i++ )
	{
		g_iHelmetModels[i] = CBaseEntity::PrecacheModel( m_pszHelmetModels[i] );
	}

// Moved to pure_server_minimal.txt
//	// Sniper scope
//	engine->ForceExactFile( "sprites/scopes/scope_spring_ul.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_ul.vtf" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_ur.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_ur.vtf" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_ll.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_ll.vtf" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_lr.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_spring_lr.vtf" );
//
//	engine->ForceExactFile( "sprites/scopes/scope_k43_ul.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_ul.vtf" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_ur.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_ur.vtf" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_ll.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_ll.vtf" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_lr.vmt" );
//	engine->ForceExactFile( "sprites/scopes/scope_k43_lr.vtf" );
//
//	// Smoke grenade-related files
//	engine->ForceExactFile( "particle/particle_smokegrenade1.vmt" );
//	engine->ForceExactFile( "particle/particle_smokegrenade.vtf" );
//	engine->ForceExactFile( "effects/stun.vmt" );
//
//	// DSP presets - don't want people avoiding the deafening + ear ring
//	engine->ForceExactFile( "scripts/dsp_presets.txt" );
//
//	// force weapon scripts because people are dirty cheaters
//	engine->ForceExactFile( "scripts/weapon_30cal.ctx");
//	engine->ForceExactFile( "scripts/weapon_amerknife.ctx");
//	engine->ForceExactFile( "scripts/weapon_bar.ctx");
//	engine->ForceExactFile( "scripts/weapon_bazooka.ctx");
//	engine->ForceExactFile( "scripts/weapon_c96.ctx");
//	engine->ForceExactFile( "scripts/weapon_colt.ctx");
//	engine->ForceExactFile( "scripts/weapon_frag_ger.ctx");
//	engine->ForceExactFile( "scripts/weapon_frag_ger_live.ctx");
//	engine->ForceExactFile( "scripts/weapon_frag_us.ctx");
//	engine->ForceExactFile( "scripts/weapon_frag_us_live.ctx");
//	engine->ForceExactFile( "scripts/weapon_garand.ctx");
//	engine->ForceExactFile( "scripts/weapon_k98.ctx");
//	engine->ForceExactFile( "scripts/weapon_k98_scoped.ctx");
//	engine->ForceExactFile( "scripts/weapon_m1carbine.ctx");
//	engine->ForceExactFile( "scripts/weapon_mg42.ctx");
//	engine->ForceExactFile( "scripts/weapon_mp40.ctx");
//	engine->ForceExactFile( "scripts/weapon_mp44.ctx");
//	engine->ForceExactFile( "scripts/weapon_p38.ctx");
//	engine->ForceExactFile( "scripts/weapon_pschreck.ctx");
//	engine->ForceExactFile( "scripts/weapon_riflegren_ger.ctx");
//	engine->ForceExactFile( "scripts/weapon_riflegren_ger_live.ctx");
//	engine->ForceExactFile( "scripts/weapon_riflegren_us.ctx");
//	engine->ForceExactFile( "scripts/weapon_riflegren_us_live.ctx");
//	engine->ForceExactFile( "scripts/weapon_smoke_ger.ctx");
//	engine->ForceExactFile( "scripts/weapon_smoke_us.ctx");
//	engine->ForceExactFile( "scripts/weapon_spade.ctx");
//	engine->ForceExactFile( "scripts/weapon_spring.ctx");
//	engine->ForceExactFile( "scripts/weapon_thompson.ctx");
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

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.GetInt() ? true : false;
	
	extern void Bot_RunAll();
	Bot_RunAll();
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	CreateGameRulesObject( "CDODGameRules" );
}
