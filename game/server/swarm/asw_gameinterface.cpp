//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "asw_shareddefs.h"
#include "fmtstr.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "matchmaking/swarm/imatchext_swarm.h"

extern ConVar sv_force_transmit_ents;

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = 1; 
	defaultMaxPlayers = 4;
	maxplayers = ASW_MAX_PLAYERS;
}

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	// precache even if not in the level, for onslaught mode
	UTIL_PrecacheOther( "asw_shieldbug" );
	UTIL_PrecacheOther( "asw_parasite" );
}

bool g_bOfflineGame = false;

extern const char *COM_GetModDirectory( void );

//-----------------------------------------------------------------------------
// Purpose: Called to apply lobby settings to a dedicated server
//-----------------------------------------------------------------------------
void CServerGameDLL::ApplyGameSettings( KeyValues *pKV )
{
	if ( !pKV )
		return;

	// Fix the game settings request when a generic request for
	// map launch comes in (it might be nested under reservation
	// processing)
	bool bAlreadyLoadingMap = false;
	char const *szBspName = NULL;
	const char *pGameDir = COM_GetModDirectory();
	if ( !Q_stricmp( pKV->GetName(), "::ExecGameTypeCfg" ) )
	{
		if ( !engine )
			return;

		char const *szNewMap = pKV->GetString( "map/mapname", "" );
		if ( !szNewMap || !*szNewMap )
			return;

		KeyValues *pLaunchOptions = engine->GetLaunchOptions();
		if ( !pLaunchOptions )
			return;

		if ( FindLaunchOptionByValue( pLaunchOptions, "changelevel" ) ||
			FindLaunchOptionByValue( pLaunchOptions, "changelevel2" ) )
			return;

		if ( FindLaunchOptionByValue( pLaunchOptions, "map_background" ) )
		{

			return;
		}

		bAlreadyLoadingMap = true;

		if ( FindLaunchOptionByValue( pLaunchOptions, "reserved" ) )
		{
			// We are already reserved, but we still need to let the engine
			// baseserver know how many human slots to allocate
			pKV->SetInt( "members/numSlots", g_bOfflineGame ? 1 : 4 );
			return;
		}

		pKV->SetName( pGameDir );
	}

	if ( Q_stricmp( pKV->GetName(), pGameDir ) || bAlreadyLoadingMap )
		return;

	//g_bOfflineGame = pKV->GetString( "map/offline", NULL ) != NULL;
	g_bOfflineGame = !Q_stricmp( pKV->GetString( "system/network", "LIVE" ), "offline" );

	//Msg( "GameInterface reservation payload:\n" );
	//KeyValuesDumpAsDevMsg( pKV );

	// Vitaliy: Disable cheats as part of reservation in case they were enabled (unless we are on Steam Beta)
	if ( sv_force_transmit_ents.IsFlagSet( FCVAR_DEVELOPMENTONLY ) &&	// any convar with FCVAR_DEVELOPMENTONLY flag will be sufficient here
		sv_cheats && sv_cheats->GetBool() )
	{
		sv_cheats->SetValue( 0 );
	}

	static ConVarRef asw_skill( "asw_skill", true );
	const char *szDifficulty = pKV->GetString( "game/difficulty", "normal" );
	if ( !Q_stricmp( szDifficulty, "easy" ) )
	{
		asw_skill.SetValue( 1 );
	}
	else if ( !Q_stricmp( szDifficulty, "normal" ) )
	{
		asw_skill.SetValue( 2 );
	}
	else if ( !Q_stricmp( szDifficulty, "hard" ) )
	{
		asw_skill.SetValue( 3 );
	}
	else if ( !Q_stricmp( szDifficulty, "insane" ) )
	{
		asw_skill.SetValue( 4 );
	}
	else if ( !Q_stricmp( szDifficulty, "imba" ) )
	{
		asw_skill.SetValue( 5 );
	}

	extern ConVar asw_sentry_friendly_fire_scale;
	extern ConVar asw_marine_ff_absorption;
	int nHardcoreFF = pKV->GetInt( "game/hardcoreFF", 0 );
	if ( nHardcoreFF == 1 )
	{
		asw_sentry_friendly_fire_scale.SetValue( 1.0f );
		asw_marine_ff_absorption.SetValue( 0 );
	}
	else
	{
		asw_sentry_friendly_fire_scale.SetValue( 0.0f );
		asw_marine_ff_absorption.SetValue( 1 );
	}

	extern ConVar asw_horde_override;
	extern ConVar asw_wanderer_override;
	int nOnslaught = pKV->GetInt( "game/onslaught", 0 );
	if ( nOnslaught == 1 )
	{
		asw_horde_override.SetValue( 1 );
		asw_wanderer_override.SetValue( 1 );
	}
	else
	{
		asw_horde_override.SetValue( 0 );
		asw_wanderer_override.SetValue( 0 );
	}

	char const *szMapCommand = pKV->GetString( "map/mapcommand", "map" );

	const char *szMode = pKV->GetString( "game/mode", "campaign" );

	char const *szGameMode = pKV->GetString( "game/mode", "" );
	if ( szGameMode && *szGameMode )
	{
		extern ConVar mp_gamemode;
		mp_gamemode.SetValue( szGameMode );
	}

	if ( !Q_stricmp( szMode, "campaign" ) )
	{
		// TODO: Handle loading a game instead of starting a new one
		const char *szCampaignName = pKV->GetString( "game/campaign", NULL );
		if ( !szCampaignName )
			return;		

		IASW_Mission_Chooser_Source* pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
		if ( !pSource )
			return;

		char szSaveFilename[ MAX_PATH ];
		szSaveFilename[ 0 ] = 0;
		const char *szStartingMission = pKV->GetString( "game/mission", NULL );
		if ( !pSource->ASW_Campaign_CreateNewSaveGame( &szSaveFilename[0], sizeof( szSaveFilename ), szCampaignName, !g_bOfflineGame, szStartingMission ) )
		{
			Msg( "Unable to create new save game.\n" );
			return;
		}

		engine->ServerCommand( CFmtStr( "%s %s campaign %s reserved\n",
			szMapCommand,
			szStartingMission ? szStartingMission : "lobby",
			szSaveFilename ) );
	}
	else if ( !Q_stricmp( szMode, "single_mission" ) )
	{
		szBspName = pKV->GetString( "game/mission", NULL );
		if ( !szBspName )
			return;

		engine->ServerCommand( CFmtStr( "%s %s reserved\n",
			szMapCommand,
			szBspName ) );
	}
	else
	{
		Warning( "ApplyGameSettings: Unknown game mode!\n" );
	}
}