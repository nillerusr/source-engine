//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "EventLog.h"
#include "team.h"
#include "KeyValues.h"
#include "nav.h"
#include "nav_area.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CEventLog::CEventLog()
{
}

CEventLog::~CEventLog()
{
}


void CEventLog::FormatPlayer( CBaseEntity *ent, char *str, int len ) const
{
	if ( !str || len <= 0 )
	{
		return;
	}

	CBasePlayer *player = ToBasePlayer( ent );

	const char *playerName = "Unknown";
	int userID = 0;
	const char *networkIDString = "";
	const char *teamName = "";
	int areaID = 0;
	if ( player )
	{
		playerName = player->GetPlayerName();
		userID = player->GetUserID();
		networkIDString = player->GetNetworkIDString();
		CTeam *team = player->GetTeam();
		if ( team )
		{
			teamName = team->GetName();
		}
	}

	if ( ent && ent->MyCombatCharacterPointer() )
	{
		CNavArea *area = ent->MyCombatCharacterPointer()->GetLastKnownArea();
		if ( area )
		{
			areaID = area->GetID();
		}
	}

	V_snprintf( str, len, "\"%s<%i><%s><%s><Area %d>\"", playerName, userID, networkIDString, teamName, areaID );
}

const char *CEventLog::FormatPlayer( CBaseEntity *ent ) const
{
	const int MaxEntries = 4;
	const int BufferLength = PLAYER_LOGINFO_SIZE;
	static char s_buffer[ MaxEntries ][ BufferLength ];
	static int s_index = 0;

	char *ret = s_buffer[ s_index++ ];
	if ( s_index >= MaxEntries )
	{
		s_index = 0;
	}

	FormatPlayer( ent, ret, BufferLength );
	return ret;
}

void CEventLog::FireGameEvent( IGameEvent *event )
{
	if ( !g_bIsLogging )
		return;

	PrintEvent ( event );
}

bool CEventLog::PrintEvent( IGameEvent *event )
{
	const char * name = event->GetName();

	if ( Q_strncmp(name, "server_", strlen("server_")) == 0 )
	{
		return true; // we don't care about server events (engine does)
	}
	else if ( Q_strncmp(name, "player_", strlen("player_")) == 0 )
	{
		return PrintPlayerEvent( event );
	}
	else if ( Q_strncmp(name, "team_", strlen("team_")) == 0 )
	{
		return PrintTeamEvent( event );
	}
	else if ( Q_strncmp(name, "game_", strlen("game_")) == 0 )
	{
		return PrintGameEvent( event );
	}
	else
	{
		return PrintOtherEvent( event ); // bomb_, round_, et al
	}
}

bool CEventLog::PrintGameEvent( IGameEvent *event )
{
//	const char * name = event->GetName() + Q_strlen("game_"); // remove prefix

	return false;
}

bool CEventLog::PrintPlayerEvent( IGameEvent *event )
{
	const char * eventName = event->GetName();
	const int userid = event->GetInt( "userid" );

	if ( !Q_strncmp( eventName, "player_connect", Q_strlen("player_connect") ) ) // player connect is before the CBasePlayer pointer is setup
	{
		const char *name = event->GetString( "name" );
		const char *address = event->GetString( "address" );
		const char *networkid = event->GetString("networkid" );
		UTIL_LogPrintf( "\"%s<%i><%s><>\" connected, address \"%s\"\n", name, userid, networkid, address);
		return true;
	}
	else if ( !Q_strncmp( eventName, "player_disconnect", Q_strlen("player_disconnect")  ) )
	{
		const char *reason = event->GetString("reason" );
		const char *name = event->GetString("name" );
		const char *networkid = event->GetString("networkid" );
		CTeam *team = NULL;
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );

		if ( pPlayer )
		{
			team = pPlayer->GetTeam();
		}

		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected (reason \"%s\")\n", name, userid, networkid, team ? team->GetName() : "", reason );
		return true;
	}

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
	if ( !pPlayer)
	{
		DevMsg( "CEventLog::PrintPlayerEvent: Failed to find player (userid: %i, event: %s)\n", userid, eventName );
		return false;
	}

	if ( !Q_strncmp( eventName, "player_team", Q_strlen("player_team") ) )
	{
		const bool bDisconnecting = event->GetBool( "disconnect" );

		if ( !bDisconnecting )
		{
			const int newTeam = event->GetInt( "team" );
			const int oldTeam = event->GetInt( "oldteam" );
			CTeam *team = GetGlobalTeam( newTeam );
			CTeam *oldteam = GetGlobalTeam( oldTeam );

			const char *playerName = pPlayer->GetPlayerName();
			int userID = pPlayer->GetUserID();
			if ( userID > 0 )
			{
				const char *networkID = pPlayer->GetNetworkIDString();
				const char *oldTeamName = (oldteam) ? oldteam->GetName() : "Unassigned";
				const char *teamName = (team) ? team->GetName() : "Unassigned";

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"%s\"\n", 
				playerName,
				userID,
				networkID,
				oldTeamName,
				teamName );
			}
		}

		return true;
	}
	else if ( !Q_strncmp( eventName, "player_death", Q_strlen("player_death") ) )
	{
		const int attackerid = event->GetInt("attacker" );

		CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
		CTeam *team = pPlayer->GetTeam();
		CTeam *attackerTeam = NULL;
		
		if ( pAttacker )
		{
			attackerTeam = pAttacker->GetTeam();
		}
		if ( pPlayer == pAttacker && pPlayer )  
		{  

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : "",
							pAttacker->GetClassname()
							);
		}
		else if ( pAttacker )
		{
			CTeam *attackerTeam = pAttacker->GetTeam();

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\"\n",  
							pAttacker->GetPlayerName(),
							attackerid,
							pAttacker->GetNetworkIDString(),
							attackerTeam ? attackerTeam->GetName() : "",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : ""
							);								
		}
		else
		{  
			// killed by the world
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"world\"\n",
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString(),
							team ? team->GetName() : ""
							);
		}
		return true;
	}
	else if ( !Q_strncmp( eventName, "player_activate", Q_strlen("player_activate") ) )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><>\" entered the game\n",  
							pPlayer->GetPlayerName(),
							userid,
							pPlayer->GetNetworkIDString()
							);

		return true;
	}
	else if ( !Q_strncmp( eventName, "player_changename", Q_strlen("player_changename") ) )
	{
		const char *newName = event->GetString( "newname" );
		const char *oldName = event->GetString( "oldname" );
		CTeam *team = pPlayer->GetTeam();
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
					oldName,
					userid,
					pPlayer->GetNetworkIDString(),
					team ? team->GetName() : "",
					newName
					);
		return true;
	}
				   
// ignored events
//player_hurt
	return false;
}

bool CEventLog::PrintTeamEvent( IGameEvent *event )
{
//	const char * name = event->GetName() + Q_strlen("team_"); // remove prefix

	return false;
}

bool CEventLog::PrintOtherEvent( IGameEvent *event )
{
	return false;
}


bool CEventLog::Init()
{
	ListenForGameEvent( "player_changename" );
	ListenForGameEvent( "player_activate" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_connect" );

	return true;
}
