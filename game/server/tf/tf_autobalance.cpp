//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"

#include "tf_autobalance.h"
#include "tf_gamerules.h"
#include "tf_matchmaking_shared.h"
#include "team.h"
#include "minigames/tf_duel.h"
#include "player_resource.h"
#include "tf_player_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern ConVar mp_developer;
extern ConVar mp_teams_unbalance_limit;
extern ConVar tf_arena_use_queue;
extern ConVar mp_autoteambalance;
extern ConVar tf_autobalance_query_lifetime;
extern ConVar tf_autobalance_xp_bonus;

ConVar tf_autobalance_detected_delay( "tf_autobalance_detected_delay", "30", FCVAR_NONE );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFAutobalance::CTFAutobalance()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFAutobalance::~CTFAutobalance()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::Reset()
{
	m_iCurrentState = AB_STATE_INACTIVE;
	m_iLightestTeam = m_iHeaviestTeam = TEAM_INVALID;
	m_nNeeded = 0;
	m_flBalanceTeamsTime = -1.f;

	if ( m_vecPlayersAsked.Count() > 0 )
	{
		// if we're resetting and we have people we haven't heard from yet, tell them to close their notification
		FOR_EACH_VEC( m_vecPlayersAsked, i )
		{
			if ( m_vecPlayersAsked[i].hPlayer.Get() && ( m_vecPlayersAsked[i].eState == AB_VOLUNTEER_STATE_ASKED ) )
			{
				CSingleUserRecipientFilter filter( m_vecPlayersAsked[i].hPlayer.Get() );
				filter.MakeReliable();
				UserMessageBegin( filter, "AutoBalanceVolunteer_Cancel" );
				MessageEnd();
			}
		}

		m_vecPlayersAsked.Purge();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::Shutdown()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::LevelShutdownPostEntity()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFAutobalance::ShouldBeActive() const
{
	if ( !TFGameRules() )
		return false;

	if ( TFGameRules()->IsInTraining() || TFGameRules()->IsInItemTestingMode() )
		return false;

	if ( TFGameRules()->IsInArenaMode() && tf_arena_use_queue.GetBool() )
		return false;

#if defined( _DEBUG ) || defined( STAGING_ONLY )
	if ( mp_developer.GetBool() )
		return false;
#endif // _DEBUG || STAGING_ONLY

	if ( mp_teams_unbalance_limit.GetInt() <= 0 )
		return false;

	const IMatchGroupDescription *pMatchDesc = GetMatchGroupDescription( TFGameRules()->GetCurrentMatchGroup() );
	if ( pMatchDesc )
	{
		return pMatchDesc->m_params.m_bUseAutoBalance;
	}

	// outside of managed matches, we don't normally do any balancing for tournament mode
	if ( TFGameRules()->IsInTournamentMode() )
		return false;

	return ( mp_autoteambalance.GetInt() == 2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFAutobalance::AreTeamsUnbalanced()
{
	if ( !TFGameRules() )
		return false;

	// don't bother switching teams if the round isn't running
	if ( TFGameRules()->State_Get() != GR_STATE_RND_RUNNING )
		return false;

	if ( mp_teams_unbalance_limit.GetInt() <= 0 )
		return false;

	if ( TFGameRules()->ArePlayersInHell() )
		return false;

	int nDiffBetweenTeams = 0;
	m_iLightestTeam = m_iHeaviestTeam = TEAM_INVALID;
	m_nNeeded = 0;

	CMatchInfo *pMatch = GTFGCClientSystem()->GetLiveMatch();
	if ( pMatch )
	{
		int nNumTeamRed = pMatch->GetNumActiveMatchPlayersForTeam( TFGameRules()->GetGCTeamForGameTeam( TF_TEAM_RED ) );
		int nNumTeamBlue = pMatch->GetNumActiveMatchPlayersForTeam( TFGameRules()->GetGCTeamForGameTeam( TF_TEAM_BLUE ) );

		m_iLightestTeam = ( nNumTeamRed > nNumTeamBlue ) ? TF_TEAM_BLUE : TF_TEAM_RED;
		m_iHeaviestTeam = ( nNumTeamRed > nNumTeamBlue ) ? TF_TEAM_RED : TF_TEAM_BLUE;

		nDiffBetweenTeams = abs( nNumTeamRed - nNumTeamBlue );
	}
	else
	{
		int iMostPlayers = 0;
		int iLeastPlayers = MAX_PLAYERS + 1;
		int i = FIRST_GAME_TEAM;

		for ( CTeam *pTeam = GetGlobalTeam( i ); pTeam != NULL; pTeam = GetGlobalTeam( ++i ) )
		{
			int iNumPlayers = pTeam->GetNumPlayers();

			if ( iNumPlayers < iLeastPlayers )
			{
				iLeastPlayers = iNumPlayers;
				m_iLightestTeam = i;
			}

			if ( iNumPlayers > iMostPlayers )
			{
				iMostPlayers = iNumPlayers;
				m_iHeaviestTeam = i;
			}
		}

		nDiffBetweenTeams = ( iMostPlayers - iLeastPlayers );
	}

	if ( nDiffBetweenTeams > mp_teams_unbalance_limit.GetInt() ) 
	{
		m_nNeeded = ( nDiffBetweenTeams / 2 );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::MonitorTeams()
{
	if ( AreTeamsUnbalanced() )
	{
		if ( m_flBalanceTeamsTime < 0.f )
		{
			// trigger a small waiting period to see if the GC sends us someone before we need to balance the teams 
			m_flBalanceTeamsTime = gpGlobals->curtime + tf_autobalance_detected_delay.GetInt();
		}
		else if ( m_flBalanceTeamsTime < gpGlobals->curtime )
		{
			if ( IsOkayToBalancePlayers() )
			{
				UTIL_ClientPrintAll( HUD_PRINTTALK, "#TF_Autobalance_Start", ( m_iHeaviestTeam == TF_TEAM_RED ) ? "#TF_RedTeam_Name" : "#TF_BlueTeam_Name" );
				m_iCurrentState = AB_STATE_FIND_VOLUNTEERS;
			}
		}
	}
	else
	{
		m_flBalanceTeamsTime = -1.f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFAutobalance::HaveAlreadyAskedPlayer( CTFPlayer *pTFPlayer ) const
{
	FOR_EACH_VEC( m_vecPlayersAsked, i )
	{
		if ( m_vecPlayersAsked[i].hPlayer == pTFPlayer )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFAutobalance::GetTeamAutoBalanceScore( int nTeam ) const
{
	CMatchInfo *pMatch = GTFGCClientSystem()->GetLiveMatch();
	if ( pMatch && TFGameRules() )
	{
		return pMatch->GetTotalSkillRatingForTeam( TFGameRules()->GetGCTeamForGameTeam( nTeam ) );
	}

	int nTotalScore = 0;
	CTFPlayerResource *pTFPlayerResource = dynamic_cast<CTFPlayerResource *>( g_pPlayerResource );
	if ( pTFPlayerResource )
	{
		CTeam *pTeam = GetGlobalTeam( nTeam );
		if ( pTeam )
		{
			for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
			{
				CTFPlayer *pTFPlayer = ToTFPlayer( pTeam->GetPlayer( i ) );
				if ( pTFPlayer )
				{
					nTotalScore += pTFPlayerResource->GetTotalScore( pTFPlayer->entindex() );
				}
			}
		}
	}

	return nTotalScore;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFAutobalance::GetPlayerAutoBalanceScore( CTFPlayer *pTFPlayer ) const
{
	if ( !pTFPlayer )
		return 0;

	CMatchInfo *pMatch = GTFGCClientSystem()->GetLiveMatch();
	if ( pMatch )
	{
		CSteamID steamID;
		pTFPlayer->GetSteamID( &steamID );

		if ( steamID.IsValid() )
		{
			const CMatchInfo::PlayerMatchData_t* pPlayerMatchData = pMatch->GetMatchDataForPlayer( steamID );
			if ( pPlayerMatchData )
			{
				FixmeMMRatingBackendSwapping(); // Make sure this makes sense with arbitrary skill rating values --
												// e.g. maybe we want a smarter glicko-weighting thing.
				return (int)pPlayerMatchData->unMMSkillRating;
			}
		}
	}

	int nTotalScore = 0;
	CTFPlayerResource *pTFPlayerResource = dynamic_cast<CTFPlayerResource *>( g_pPlayerResource );
	if ( pTFPlayerResource )
	{
		nTotalScore = pTFPlayerResource->GetTotalScore( pTFPlayer->entindex() );
	}

	return nTotalScore;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer *CTFAutobalance::FindPlayerToAsk()
{
	CTFPlayer *pRetVal = NULL;

	CUtlVector< CTFPlayer* > vecCandiates;
	CTeam *pTeam = GetGlobalTeam( m_iHeaviestTeam );
	if ( pTeam )
	{
		// loop through and get a list of possible candidates
		for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer( pTeam->GetPlayer( i ) );
			if ( pTFPlayer && !HaveAlreadyAskedPlayer( pTFPlayer ) && pTFPlayer->CanBeAutobalanced() )
			{
				vecCandiates.AddToTail( pTFPlayer );
			}
		}
	}

	// no need to go any further if there's only one candidate
	if ( vecCandiates.Count() == 1 )
	{
		pRetVal = vecCandiates[0];
	}
	else if ( vecCandiates.Count() > 1 )
	{
		int nTotalDiff = abs( GetTeamAutoBalanceScore( m_iHeaviestTeam ) - GetTeamAutoBalanceScore( m_iLightestTeam ) );
		int nAverageNeeded = ( nTotalDiff / 2 ) / m_nNeeded;

		// now look a player on the heaviest team with skillrating closest to that average
		int nClosest = INT_MAX;
		FOR_EACH_VEC( vecCandiates, iIndex )
		{
			int nDiff = abs( nAverageNeeded - GetPlayerAutoBalanceScore( vecCandiates[iIndex] ) );
			if ( nDiff < nClosest )
			{
				nClosest = nDiff;
				pRetVal = vecCandiates[iIndex];
			}
		}
	}

	return pRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::FindVolunteers()
{
	// keep track of the state of things, this will also update our counts if more players drop from the server
	if ( !AreTeamsUnbalanced() || !IsOkayToBalancePlayers() )
	{
		Reset();
		return;
	}

	int nPendingReplies = 0;
	int nRepliedNo = 0;

	FOR_EACH_VEC( m_vecPlayersAsked, i )
	{
		// if the player is valid
		if ( m_vecPlayersAsked[i].hPlayer.Get() )
		{
			switch ( m_vecPlayersAsked[i].eState )
			{
			case AB_VOLUNTEER_STATE_ASKED:
				if ( m_vecPlayersAsked[i].flQueryExpireTime < gpGlobals->curtime )
				{
					// they've timed out the request period without replying
					m_vecPlayersAsked[i].eState = AB_VOLUNTEER_STATE_NO;
					nRepliedNo++;
				}
				else
				{
					nPendingReplies++;
				}
				break;
			case AB_VOLUNTEER_STATE_NO:
				nRepliedNo++;
				break;
			default:
				break;
			}
		}
	}

	int nNumToAsk = ( m_nNeeded * 2 );

	// do we need to ask for more volunteers?
	if ( nPendingReplies < nNumToAsk )
	{
		int nNumNeeded = nNumToAsk - nPendingReplies;
		int nNumAsked = 0;

		while ( nNumAsked < nNumNeeded )
		{
			CTFPlayer *pTFPlayer = FindPlayerToAsk();
			if ( pTFPlayer )
			{
				int iIndex = m_vecPlayersAsked.AddToTail();
				m_vecPlayersAsked[iIndex].hPlayer = pTFPlayer;
				m_vecPlayersAsked[iIndex].eState = AB_VOLUNTEER_STATE_ASKED;
				m_vecPlayersAsked[iIndex].flQueryExpireTime = gpGlobals->curtime + tf_autobalance_query_lifetime.GetInt() + 3; // add 3 seconds to allow for travel time to/from the client

				CSingleUserRecipientFilter filter( pTFPlayer );
				filter.MakeReliable();
				UserMessageBegin( filter, "AutoBalanceVolunteer" );
				MessageEnd();

				nNumAsked++;
				nPendingReplies++;
			}
			else
			{
				// we couldn't find anyone else to ask
				if ( nPendingReplies <= 0 )
				{
					// we're not waiting on anyone else to reply....so we should just reset
					Reset();
				}

				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::FrameUpdatePostEntityThink()
{
	bool bActive = ShouldBeActive();
	if ( !bActive )
	{
		Reset();
		return;
	}

	switch ( m_iCurrentState )
	{
	case AB_STATE_INACTIVE:
		// we should be active if we've made it this far
		m_iCurrentState = AB_STATE_MONITOR;
		break;
	case AB_STATE_MONITOR:
		MonitorTeams();
		break;
	case AB_STATE_FIND_VOLUNTEERS:
		FindVolunteers();
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFAutobalance::IsOkayToBalancePlayers()
{
	if ( GTFGCClientSystem()->GetLiveMatch() && !GTFGCClientSystem()->CanChangeMatchPlayerTeams() ) 
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFAutobalance::ReplyReceived( CTFPlayer *pTFPlayer, bool bResponse )
{
	if ( m_iCurrentState != AB_STATE_FIND_VOLUNTEERS )
		return;

	if ( !AreTeamsUnbalanced() || !IsOkayToBalancePlayers() )
	{
		Reset();
		return;
	}

	FOR_EACH_VEC( m_vecPlayersAsked, i )
	{
		// is this a player we asked?
		if ( m_vecPlayersAsked[i].hPlayer == pTFPlayer )
		{
			m_vecPlayersAsked[i].eState = bResponse ? AB_VOLUNTEER_STATE_YES : AB_VOLUNTEER_STATE_NO;
			if ( bResponse  && pTFPlayer->CanBeAutobalanced() )
			{
				pTFPlayer->ChangeTeam( m_iLightestTeam, false, false, true );
				pTFPlayer->ForceRespawn();
				pTFPlayer->SetLastAutobalanceTime( gpGlobals->curtime );

				CMatchInfo *pMatch = GTFGCClientSystem()->GetLiveMatch();
				if ( pMatch )
				{
					CSteamID steamID;
					pTFPlayer->GetSteamID( &steamID );

					// We're going to give the switching player a bonus pool of XP. This should encourage
					// them to keep playing to earn what's in the pool, rather than just quit after getting
					// a big payout
					if ( !pMatch->BSentResult() )
					{
						pMatch->GiveXPBonus( steamID, CMsgTFXPSource_XPSourceType_SOURCE_AUTOBALANCE_BONUS, 1, tf_autobalance_xp_bonus.GetInt() );
					}

					GTFGCClientSystem()->ChangeMatchPlayerTeam( steamID, TFGameRules()->GetGCTeamForGameTeam( m_iLightestTeam ) );
				}
			}
		}
	}
}

CTFAutobalance gTFAutobalance;
CTFAutobalance *TFAutoBalance(){ return &gTFAutobalance; }
