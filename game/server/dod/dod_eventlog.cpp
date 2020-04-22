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
#include "KeyValues.h"
#include "dod_shareddefs.h"
#include "dod_team.h"

#define LOG_DETAIL_ENEMY_ATTACKS		1
#define LOG_DETAIL_TEAMMATE_ATTACKS		2

ConVar mp_logdetail( "mp_logdetail", "0", FCVAR_NONE, "Logs attacks.  Values are: 0=off, 1=enemy, 2=teammate, 3=both)", true, 0.0f, true, 3.0f );

class CDODEventLog : public CEventLog
{
private:
	typedef CEventLog BaseClass;

public:
	bool PrintEvent( IGameEvent * event )	// override virtual function
	{
		if ( !PrintDodEvent( event ) ) // allow DOD to override logging
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

		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "player_hurt" );
		ListenForGameEvent( "player_changeclass" );
		ListenForGameEvent( "dod_warmup_begins" );
		ListenForGameEvent( "dod_warmup_ends" );
		ListenForGameEvent( "dod_round_start" );
		ListenForGameEvent( "dod_restart_round" );
		ListenForGameEvent( "dod_ready_restart" );
		ListenForGameEvent( "dod_allies_ready" );
		ListenForGameEvent( "dod_axis_ready" );
		ListenForGameEvent( "dod_round_restart_seconds" );
		ListenForGameEvent( "dod_team_scores" );
		ListenForGameEvent( "dod_round_win" );
		ListenForGameEvent( "dod_tick_points" );
		ListenForGameEvent( "dod_game_over" );
		ListenForGameEvent( "dod_point_captured" );
		ListenForGameEvent( "dod_capture_blocked" );
		ListenForGameEvent( "dod_bomb_planted" );
		ListenForGameEvent( "dod_bomb_exploded" );
		ListenForGameEvent( "dod_bomb_defused" );
		ListenForGameEvent( "dod_kill_planter" );
		ListenForGameEvent( "dod_kill_defuser" );

		return true;
	}

protected:

	bool PrintDodEvent( IGameEvent * event )	// print Mod specific logs
	{
		const char *eventName = event->GetName();
	
		if ( !Q_strncmp( eventName, "server_", strlen("server_")) )
		{
			return false; // ignore server_ messages
		}
		
		if ( FStrEq( eventName, "player_death" ) )
		{
			const int userid = event->GetInt( "userid" );
			CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );

			if ( !pPlayer )
			{
				return false;
			}

			const int attackerid = event->GetInt("attacker" );
			const char *weapon = event->GetString( "weapon" );
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
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
								pAttacker->GetPlayerName(),
								attackerid,
								pAttacker->GetNetworkIDString(),
								pAttacker->GetTeam()->GetName(),
								pPlayer->GetPlayerName(),
								userid,
								pPlayer->GetNetworkIDString(),
								pPlayer->GetTeam()->GetName(),
								weapon
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

			// Domination and Revenge
			// pAttacker //int attackerid = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
			// pPlayer //int userid = engine->GetPlayerForUserID( event->GetInt( "userid" ) );

			if ( event->GetInt( "dominated" ) > 0 && pAttacker )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"domination\" against \"%s<%i><%s><%s>\"\n",  
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName()
					);
			}
			if ( event->GetInt( "revenge" ) > 0 && pAttacker ) 
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"revenge\" against \"%s<%i><%s><%s>\"\n",  
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName()
					);
			}

			return true;
		}
		else if ( FStrEq( eventName, "player_hurt" ) )
		{
			const int userid = event->GetInt( "userid" );
			CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );

			if ( !pPlayer )
			{
				return false;
			}

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

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" attacked \"%s<%i><%s><%s>\" with \"%s\" (damage \"%d\") (health \"%d\") (hitgroup \"%s\")\n",  
					pAttacker->GetPlayerName(),
					attackerid,
					pAttacker->GetNetworkIDString(),
					pAttacker->GetTeam()->GetName(),
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName(),
					weapon,
					event->GetInt( "damage" ),
					event->GetInt( "health" ),
					hitgroupStr );
			}
			return true;
		}
		else if ( FStrEq( eventName, "player_changeclass" ) )
		{
			const int userid = event->GetInt( "userid" );
			CBasePlayer *pPlayer = UTIL_PlayerByUserId( userid );

			if ( !pPlayer )
			{
				return false;
			}

			int iClass = event->GetInt("class");
			int iTeam = pPlayer->GetTeamNumber();

			if ( iTeam != TEAM_ALLIES && iTeam != TEAM_AXIS )
				return true;

			CDODTeam *pTeam = GetGlobalDODTeam( iTeam );

			if ( iClass == PLAYERCLASS_RANDOM )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed role to \"Random\"\n",  
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pTeam->GetName()
					);
			}
			else if ( iClass < GetGlobalDODTeam(iTeam)->GetNumPlayerClasses() )
			{
				const CDODPlayerClassInfo &pInfo = GetGlobalDODTeam(iTeam)->GetPlayerClassInfo( iClass );

				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed role to \"%s\"\n",  
					pPlayer->GetPlayerName(),
					userid,
					pPlayer->GetNetworkIDString(),
					pTeam->GetName(),
					pInfo.m_szPrintName
					);
			}
			return true;
		}
		else if ( FStrEq( eventName, "dod_warmup_begins" ) )
		{
			UTIL_LogPrintf( "World triggered \"Warmup_Begin\"\n" );
			return true;
		}
		else if ( FStrEq( eventName, "dod_warmup_ends" ) )
		{
			UTIL_LogPrintf( "World triggered \"Warmup_Ends\"\n" );
			return true;
		}
		else if ( FStrEq( eventName, "dod_round_start" ) )
		{
			UTIL_LogPrintf("World triggered \"Round_Start\"\n");
			return true;
		}
		else if ( FStrEq( eventName, "dod_restart_round" ) )
		{
			UTIL_LogPrintf("World triggered \"Round_Restart\"\n");
			return true;
		}
		else if ( FStrEq( eventName, "dod_ready_restart" ) )
		{
			UTIL_LogPrintf( "World triggered \"Ready_Restart_Begin\"\n" );
			return true;
		}
		else if ( FStrEq( eventName, "dod_allies_ready" ) )
		{
			UTIL_LogPrintf("World triggered \"Allies Ready\"\n");
			return true;
		}
		else if ( FStrEq( eventName, "dod_axis_ready" ) )
		{
			UTIL_LogPrintf("World triggered \"Axis Ready\"\n");
			return true;
		}
		else if ( FStrEq( eventName, "dod_round_restart_seconds" ) )
		{
			UTIL_LogPrintf( "World triggered \"Round_Restart_In\" (delay \"%d\")\n", event->GetInt("seconds") );
			return true;
		}
		else if ( FStrEq( eventName, "dod_team_scores" ) )
		{
			int iAlliesRoundsWon = event->GetInt( "allies_caps" );
			int iAlliesTickPoints = event->GetInt( "allies_tick" );
			int iNumAllies = event->GetInt( "allies_players" );
			int iAxisRoundsWon = event->GetInt( "axis_caps" );
			int iAxisTickPoints = event->GetInt( "axis_tick" );
			int iNumAxis = event->GetInt( "axis_players" );

			UTIL_LogPrintf( "Team \"Allies\" triggered \"team_scores\" (roundswon \"%d\") (tickpoints \"%d\") (numplayers \"%d\")\n", iAlliesRoundsWon, iAlliesTickPoints, iNumAllies );
			UTIL_LogPrintf( "Team \"Allies\" triggered \"team_scores\" (roundswon \"%d\") (tickpoints \"%d\") (numplayers \"%d\")\n", iAxisRoundsWon, iAxisTickPoints, iNumAxis );
			return true;
		}
		else if ( FStrEq( eventName, "dod_round_win" ) )
		{
			CDODTeam *pTeam = GetGlobalDODTeam( event->GetInt( "team" ) );

			UTIL_LogPrintf( "Team \"%s\" triggered \"round_win\" (rounds_won \"%d\") (numplayers \"%d\")\n",
				pTeam->GetName(),
				pTeam->GetRoundsWon(),
				pTeam->GetNumPlayers() );

			return true;
		}
		else if ( FStrEq( eventName, "dod_tick_points" ) )
		{
			CDODTeam *pTeam = GetGlobalDODTeam( event->GetInt( "team" ) );

			int iScore = event->GetInt( "score" );
			int iTotalScore = event->GetInt( "totalscore" );

			UTIL_LogPrintf( "Team \"%s\" triggered \"tick_score\" (score \"%i\") (totalscore \"%d\") (numplayers \"%d\")\n",
				pTeam->GetName(),
				iScore,
				iTotalScore,
				pTeam->GetNumPlayers() );

			return true;		
		}
		else if ( FStrEq( eventName, "dod_game_over" ) )
		{
			UTIL_LogPrintf( "World triggered \"Game_Over\" reason \"%s\"\n", event->GetString( "reason" ) );
			return true;		
		}
		else if ( FStrEq( eventName, "dod_point_captured" ) )
		{			
			// identifier of the area	"cp"	"cpname"
			int iPointIndex = event->GetInt( "cp" );
			const char *szPointName = event->GetString( "cpname" );

			const char *szCappers = event->GetString( "cappers" );

			int iNumCappers = Q_strlen( szCappers );

			if ( iNumCappers <= 0 )
				return true;

			//"Team "Allies" captured location "<3><#map_flag_2nd_axis>" with "2"
            //    players (1 "Matt<UID><STEAMID><TEAM>") (2 "Player2<2><STEAMID><TEAM>")

			char buf[512];

			for ( int i=0;i<iNumCappers;i++ )
			{
				int iPlayerIndex = szCappers[i];

				Assert( iPlayerIndex != '\0' && iPlayerIndex > 0 && iPlayerIndex < MAX_PLAYERS );

				CBasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );

				if ( i == 0 )
				{
					Q_snprintf( buf, sizeof(buf), "Team \"%s\" triggered \"captured_loc\" (flagindex \"%d\") (flagname \"%s\") (numplayers \"%d\") ",
						pPlayer->GetTeam()->GetName(),
						iPointIndex,
						szPointName,
						iNumCappers );
				}

				char playerBuf[256];
				Q_snprintf( playerBuf, sizeof(playerBuf), "(player \"%s<%i><%s><%s>\") ", 
					pPlayer->GetPlayerName(),
					pPlayer->GetUserID(),
					pPlayer->GetNetworkIDString(),
					pPlayer->GetTeam()->GetName() );

				Q_strncat( buf, playerBuf, sizeof(buf), COPY_ALL_CHARACTERS );
			}

			UTIL_LogPrintf( "%s\n", buf );
		}
		else if ( FStrEq( eventName, "dod_capture_blocked" ) )
		{			
			int iPointIndex = event->GetInt( "cp" );
			const char *szPointName = event->GetString( "cpname" );

			int iBlocker = event->GetInt( "blocker" );

			CBasePlayer *pPlayer = UTIL_PlayerByIndex( iBlocker );

			if ( !pPlayer )
				return false;

			// "Matt<2><UNKNOWN><Allies>" triggered "capblock" (flagindex "2") (flagname "#map_flag_2nd_axis")

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"capblock\" (flagindex \"%d\") (flagname \"%s\")\n", 
				pPlayer->GetPlayerName(),
				pPlayer->GetUserID(),
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				iPointIndex,
				szPointName );
		}
		else if ( FStrEq( eventName, "dod_bomb_planted" ) )
		{			
			int iPointIndex = event->GetInt( "cp" );
			const char *szPointName = event->GetString( "cpname" );

			int iPlanter = event->GetInt( "userid" );

			CBasePlayer *pPlayer = UTIL_PlayerByUserId( iPlanter );

			if ( !pPlayer )
				return false;

			// "Matt<2><UNKNOWN><Allies>" triggered "bomb_plant" (flagindex "2") (flagname "#map_flag_2nd_axis")

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"bomb_plant\" (flagindex \"%d\") (flagname \"%s\")\n", 
				pPlayer->GetPlayerName(),
				pPlayer->GetUserID(),
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				iPointIndex,
				szPointName );

			return true;
		}
		else if ( FStrEq( eventName, "dod_bomb_defused" ) )
		{			
			int iPointIndex = event->GetInt( "cp" );
			const char *szPointName = event->GetString( "cpname" );

			int iDefuser = event->GetInt( "userid" );

			CBasePlayer *pPlayer = UTIL_PlayerByUserId( iDefuser );

			if ( !pPlayer )
				return false;

			// "Matt<2><UNKNOWN><Allies>" triggered "bomb_defuse" (flagindex "2") (flagname "#map_flag_2nd_axis")

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"bomb_defuse\" (flagindex \"%d\") (flagname \"%s\")\n", 
				pPlayer->GetPlayerName(),
				pPlayer->GetUserID(),
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				iPointIndex,
				szPointName );

			return true;
		}
		else if ( FStrEq( eventName, "dod_kill_planter" ) )
		{			
			int iKiller = event->GetInt( "userid" );

			CBasePlayer *pPlayer = UTIL_PlayerByUserId( iKiller );

			if ( !pPlayer )
				return false;

			int iVictim = event->GetInt( "victimid" );

			CBasePlayer *pVictim = UTIL_PlayerByUserId( iVictim );

			if ( !pVictim )
				return false;

			// "Matt<2><UNKNOWN><Allies>" triggered "kill_planter" against "Fred<3><UNKNOWN><Axis>"

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"kill_planter\" against \"%s<%i><%s><%s>\"\n", 
				pPlayer->GetPlayerName(),
				pPlayer->GetUserID(),
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				pVictim->GetPlayerName(),
				pVictim->GetUserID(),
				pVictim->GetNetworkIDString(),
				pVictim->GetTeam()->GetName() );

			return true;
		}
		else if ( FStrEq( eventName, "dod_kill_defuser" ) )
		{			
			int iKiller = event->GetInt( "userid" );

			CBasePlayer *pPlayer = UTIL_PlayerByUserId( iKiller );

			if ( !pPlayer )
				return false;

			int iVictim = event->GetInt( "victimid" );

			CBasePlayer *pVictim = UTIL_PlayerByUserId( iVictim );

			if ( !pVictim )
				return false;

			// "Matt<2><UNKNOWN><Allies>" triggered "kill_defuser" against "Fred<3><UNKNOWN><Axis>"

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"kill_defuser\" against \"%s<%i><%s><%s>\"\n", 
				pPlayer->GetPlayerName(),
				pPlayer->GetUserID(),
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				pVictim->GetPlayerName(),
				pVictim->GetUserID(),
				pVictim->GetNetworkIDString(),
				pVictim->GetTeam()->GetName() );

			return true;
		}

		return false;
	}

};

CDODEventLog g_DODEventLog;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* GameLogSystem()
{
	return &g_DODEventLog;
}

