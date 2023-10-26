//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_force_transmit_ents;

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = defaultMaxPlayers = 1; 
	maxplayers = MAX_PLAYERS;
}


// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
}

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
			int numSlots = 1;





			pKV->SetInt( "members/numSlots", numSlots );
			return;
		}




	}






	//
	// Change map as the last command
	//
	if ( szBspName && szBspName[0] && !bAlreadyLoadingMap )
	{
		// Vitaliy: Disable cheats as part of reservation in case they were enabled (unless we are on Steam Beta)
		if ( sv_force_transmit_ents.IsFlagSet( FCVAR_DEVELOPMENTONLY ) &&	// any convar with FCVAR_DEVELOPMENTONLY flag will be sufficient here
			sv_cheats && sv_cheats->GetBool() )
		{
			sv_cheats->SetValue( 0 );
		}

		char const *szMapCommand = pKV->GetString( "map/mapcommand", "map" );
		DevMsg( "            starting: %s %s...\n", szMapCommand, szBspName );
		engine->ServerCommand( CFmtStr( "%s %s reserved\n",
			szMapCommand,
			szBspName ) );
	}
}