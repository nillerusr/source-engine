//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:	See c_memorylog.h
//
//=============================================================================//

#include "cbase.h"
#include "inetchannelinfo.h"
#include "clientterrorplayer.h"
#include "c_memorylog.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#if ( defined( _X360 ) && !defined( _CERT ) )

ConVar memorylog_tick( "memorylog_tick", "20", FCVAR_CHEAT, "Set to N to spew free memory to the console every N seconds to be captured by logging (0 disables)." );
ConVar memorylog_mem_dump( "memorylog_mem_dump", "0", FCVAR_CHEAT );

// Memory log auto game system instantiation
C_MemoryLog g_MemoryLog;


bool C_MemoryLog::Init( void )
{
	// Spew on the first frame after init
	m_fLastSpewTime = -memorylog_tick.GetFloat();
	// Set up a string we can search for in full heap crashdumps:
	memset( m_nRecentFreeMem, 0, sizeof( m_nRecentFreeMem ) );
	memcpy( m_nRecentFreeMem, "maryhadatinylamb", 16 );
	return true;
}

void C_MemoryLog::Spew( void )
{
	// Spew "date+time | free mem | listen/not | map | bots | players", so we can
	// use console logs to correlate memory leaks with playtest patterns.

	if ( memorylog_tick.GetFloat() <= 0 )
		return; // Disabled

	int time = (int)( Plat_FloatTime() + 0.5f );

	MEMORYSTATUS memStats;
	GlobalMemoryStatus( &memStats );
	int freeMem = memStats.dwAvailPhys;

	// Determine if this machine is the server
	INetChannelInfo *pNetInfo = engine->GetNetChannelInfo();
	bool listen = ( pNetInfo && ( pNetInfo->IsLoopback() || V_strstr( pNetInfo->GetAddress(), "127.0.0.1" ) ) );

	char mapName[32] = "";
	if ( engine->GetLevelNameShort() )
		V_strncpy( mapName, engine->GetLevelNameShort(), sizeof( mapName ) );
	if ( !mapName[ 0 ] )
		V_strncpy( mapName, "none", sizeof( mapName ) );

	char playerNames[ 512 ] = "";
	int  numBots    = 0;
	int  numPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		player_info_s playerInfo;
		C_TerrorPlayer *player = static_cast<C_TerrorPlayer *>( UTIL_PlayerByIndex( i ) );
		if ( player && player->IsPlayer() && engine->GetPlayerInfo( i, &playerInfo ) )
		{
			if ( playerInfo.fakeplayer )
			{
				// Ignore zombie bots
				if ( player->GetCharacter() < NUM_SURVIVOR_CHARACTERS )
					numBots++;
			}
			else
			{
				char playerName[64];
				V_snprintf(	playerName, sizeof( playerName ), ", %s%s%s",
							playerInfo.name,
							player->IsObserver() ? "|SPEC" : "",
							C_BasePlayer::IsLocalPlayer( player ) ? "|LOCAL" : "" );
				V_strcat( playerNames, playerName, sizeof( playerNames ) );
				numPlayers++;
			}
		}
	}

	ConMsg( "[MEMORYLOG] Time:%6d | Free: %6.2f | %s | Map: %-32s | Bots: %2d | Players: %2d%s\n",
			time,
			freeMem / ( 1024.0f*1024.0f ),
			listen ? "Server" : "Client",
			mapName,
			numBots, numPlayers, playerNames );

	// Keep the last N free memory values in an array, for inspection in full heap crashdumps
	for ( int i = 255; i > 4; i-- ) m_nRecentFreeMem[ i ] = m_nRecentFreeMem[ i - 1 ];
	m_nRecentFreeMem[ 4 ] = freeMem;

	if ( memorylog_mem_dump.GetBool() )
	{
		g_pMemAlloc->DumpStats();
	}
}

void C_MemoryLog::Update( float frametime )
{
	float curTime = Plat_FloatTime();
	if ( ( curTime - m_fLastSpewTime ) >= memorylog_tick.GetFloat() )
	{
		m_fLastSpewTime = curTime;
		Spew();
	}
}

void C_MemoryLog::LevelInitPostEntity( void )
{
	// Spew on the first frame after map load
	m_fLastSpewTime = -memorylog_tick.GetFloat();
}

void C_MemoryLog::LevelShutdownPreEntity( void )
{
	// Spew in case we don't make it to the next map
	Spew();
}

#endif // ( defined( _X360 ) && !defined( _CERT ) )
