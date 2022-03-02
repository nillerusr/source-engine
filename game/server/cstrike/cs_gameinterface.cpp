//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "cs_gameinterface.h"
#include "AI_ResponseSystem.h"

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = 1;  // allow single player for the test maps (but we default to multi)
	maxplayers = MAX_PLAYERS;
	
	defaultMaxPlayers = 32;	// Default to 32 players unless they change it.
}


// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	if ( Q_strcmp( STRING(gpGlobals->mapname), "cs_" ) )
	{
		// don't precache AI responses (hostages) if it's not a hostage rescure map
		extern IResponseSystem *g_pResponseSystem;
		g_pResponseSystem->PrecacheResponses( false );	
	}
}
	

