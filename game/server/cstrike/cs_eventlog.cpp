//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "../EventLog.h"
#include "team.h"
#include "cs_gamerules.h"
#include "KeyValues.h"

#define LOG_DETAIL_ENEMY_ATTACKS		0x01
#define LOG_DETAIL_TEAMMATE_ATTACKS		0x02

ConVar mp_logdetail( "mp_logdetail", "0", FCVAR_NONE, "Logs attacks.  Values are: 0=off, 1=enemy, 2=teammate, 3=both)", true, 0.0f, true, 3.0f );

class CCSEventLog : public CEventLog
{
private:
	typedef CEventLog BaseClass;

public:
	bool PrintEvent( IGameEvent *event )	// override virtual function
	{
		if ( !PrintCStrikeEvent( event ) ) // allow CS to override logging
		{
			return BaseClass::PrintEvent( event );
		}
		else
		{
			return true;
		}
	}

	bool Init()
	{
		BaseClass::Init();

		// listen to CS events
		ListenForGameEvent( "round_end" );
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "bomb_pickup" );
		ListenForGameEvent( "bomb_begindefuse" );
		ListenForGameEvent( "bomb_dropped" );
		ListenForGameEvent( "bomb_defused" );
		ListenForGameEvent( "bomb_planted" );
		ListenForGameEvent( "hostage_rescued" );
		ListenForGameEvent( "hostage_killed" );
		ListenForGameEvent( "hostage_follows" );
		ListenForGameEvent( "player_hurt" );

		return true;
	}

protected:

	bool PrintCStrikeEvent( IGameEvent *event )	// print Mod specific logs
	{
		const char *eventName = event->GetName();
	
		// messages that don't have a user associated to them
		if ( !Q_strncmp( eventName, "round_end", Q_strlen("round_end") ) )
		{
			const int winner = event->GetInt( "winner" );
			const int reason = event->GetInt( "reason" );
			const char *msg = event->GetString( "message" );
			msg++; // remove the '#' char

			switch( reason )
			{
			case Game_Commencing:
				UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );
				return true;
				break;
			}

			CTeam *ct = GetGlobalTeam( TEAM_CT );
			CTeam *ter = GetGlobalTeam( TEAM_TERRORIST );
			Assert( ct && ter );

			switch ( winner )
			{
			case WINNER_CT:
				UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", ct->GetName(), msg, ct->GetScore(), ter->GetScore() );
				break;
			case WINNER_TER:
				UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", ter->GetName(), msg, ct->GetScore(), ter->GetScore() );
				break;
			case WINNER_DRAW:
			default:
				UTIL_LogPrintf( "World triggered \"%s\" (CT \"%i\") (T \"%i\")\n", msg, ct->GetScore(), ter->GetScore() );
				break;
			}	

			UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", ct->GetScore(), ct->GetNumPlayers() );
			UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", ter->GetScore(), ter->GetNumPlayers() );
			
			UTIL_LogPrintf("World triggered \"Round_End\"\n");
			return true;
		}
		else if ( !Q_strncmp( eventName, "server_", strlen("server_")) )
		{
			return false; // ignore server_ messages
		}
		
		const int userid = event->GetInt( "userid" );
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );
		if ( !pPlayer )
		{
			return false;
		}

		if ( FStrEq( eventName, "player_hurt" ) )
		{
			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );
			if ( !pAttacker )
			{
				return false;
			}

			bool isTeamAttack = ( (pPlayer->GetTeamNumber() == pAttacker->GetTeamNumber() ) && (pPlayer != pAttacker) );
			int detail = mp_logdetail.GetInt();
			if ( ( isTeamAttack && ( detail & LOG_DETAIL_TEAMMATE_ATTACKS ) ) ||
				( !isTeamAttack && ( detail & LOG_DETAIL_ENEMY_ATTACKS ) ) )
			{
				int hitgroup = event->GetInt( "hitgroup" );
				const char *hitgroupStr = "GENERIC";
				switch ( hitgroup )
				{
				case HITGROUP_GENERIC:
					hitgroupStr = "generic";
					break;
				case HITGROUP_HEAD:
					hitgroupStr = "head";
					break;
				case HITGROUP_CHEST:
					hitgroupStr = "chest";
					break;
				case HITGROUP_STOMACH:
					hitgroupStr = "stomach";
					break;
				case HITGROUP_LEFTARM:
					hitgroupStr = "left arm";
					break;
				case HITGROUP_RIGHTARM:
					hitgroupStr = "right arm";
					break;
				case HITGROUP_LEFTLEG:
					hitgroupStr = "left leg";
					break;
				case HITGROUP_RIGHTLEG:
					hitgroupStr = "right leg";
					break;
				}

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" attacked \"%s<%i><%s><%s>\" with \"%s\" (damage \"%d\") (damage_armor \"%d\") (health \"%d\") (armor \"%d\") (hitgroup \"%s\")\n",  
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName(),
					weapon,
					event->GetInt( "dmg_health" ),
					event->GetInt( "dmg_armor" ),
					event->GetInt( "health" ),
					event->GetInt( "armor" ),
					hitgroupStr );
			}
			return true;
		}
		else if ( !Q_strncmp( eventName, "player_death", Q_strlen("player_death") ) )
		{
			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
			const bool headShot = (event->GetInt( "headshot" ) == 1);
			CBasePlayer *pAttacker = UTIL_PlayerByUserId( attackerid );

			if ( pPlayer == pAttacker )  
			{  
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								weapon
								);
			}
			else if ( pAttacker )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"%s\n",  
								pAttacker->GetPlayerName(),
								attackerid,
								pAttacker->GetNetworkIDString(),
								pAttacker->GetTeam()->GetName(),
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								weapon,
								headShot ? " (headshot)":""
								);								
			}
			else
			{  
				// killed by the world
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"world\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName()
								);
			}
			return true;
		}
		else if ( !Q_strncmp( eventName, "round_start", Q_strlen("round_start") ) )
		{
			UTIL_LogPrintf("World triggered \"Round_Start\"\n");
			return true;
		}
		else if ( !Q_strncmp( eventName, "hostage_follows", Q_strlen("hostage_follows") ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Touched_A_Hostage\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "hostage_killed", Q_strlen("hostage_killed") ) )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Killed_A_Hostage\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName()
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "hostage_rescued", Q_strlen("hostage_rescued") ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"Rescued_A_Hostage\"\n",
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "bomb_planted", Q_strlen("bomb_planted") ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Planted_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "bomb_defused", Q_strlen("bomb_defused") ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"Defused_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "bomb_dropped", Q_strlen("bomb_dropped") ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Dropped_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "bomb_begindefuse", Q_strlen("bomb_begindefuse") ) )
		{
			const bool haskit = (event->GetInt( "haskit" ) == 1);
			UTIL_LogPrintf("\"%s<%i><%s><CT>\" triggered \"%s\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								haskit ? "Begin_Bomb_Defuse_With_Kit" : "Begin_Bomb_Defuse_Without_Kit"
								);
			return true;
		}
		else if ( !Q_strncmp( eventName, "bomb_pickup", Q_strlen("bomb_pickup") ) )
		{
			UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Got_The_Bomb\"\n", 
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString()
								);
			return true;
		}	
		
// unused events:
//hostage_hurt
//bomb_exploded

		return false;
	}

};

CCSEventLog g_CSEventLog;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* GameLogSystem()
{
	return &g_CSEventLog;
}

