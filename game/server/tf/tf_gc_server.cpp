//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"

#include "tf_gc_server.h"
#include "gcsdk/gcsdk_auto.h"
#include "tf_gcmessages.h"
#include "tf_player.h"
#include "rtime.h"
// XXX(JohnS): Eventually, we want to send a smaller lobby object to clients. For now, they use the CTFGSLobby, which is
//             in shared code for that reason.
#include "tf_lobby_server.h"
#include "tf_gamerules.h"
#include "eiface.h"
#include "cdll_int.h"
#include "econ_item_inventory.h"
#include "gameinterface.h"
#include "client.h"
#include "tier1/convar.h"
#include "tf_matchmaking_shared.h"
#include "tf_quickplay_shared.h"
#include "tf_mann_vs_machine_stats.h"
#include "tf_objective_resource.h"
#include "tf_player.h"
#include "tf_voteissues.h"
#include "player_vs_environment/tf_population_manager.h"
#include "quest_objective_manager.h"
#include "player_resource.h"
#include "tf_player_resource.h"
#include "tf_gamestats.h"
#include "tf_player.h"
#include "tf_match_description.h"
#include "util.h"
#include "tier1/utlqueue.h"
#include "tf_player_resource.h"
#include "tf_gc_shared.h"
#include "tf_party.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace GCSDK;

// How many minutes before we assume something is FUBAR and reboot if we're empty and waiting for the GC to acknowledge us.

// With valid match data: wait a while. GC could be having trouble, or connectivity issues, and we want to hold on to
// the results for it to come back up. After three hours, assume its us.
const int k_InvalidState_Timeout_With_Match    = 60 * 2;
const int k_InvalidState_Timeout_Without_Match = 5;

#ifdef ENABLE_GC_MATCHMAKING

/***********************************************************************************************************************
////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

   XXX(JohnS) NOTE The current state of the matchmaking flow through this class is a bit of a mess.  Have been
                   incrementally cleaning things up, but be careful.

   UpdateConnectedPlayersAndServerInfo()
     This is the heug god function that sync's our state with the GC's state via the Lobby shared object:
     - Our actual connected players

     - m_pMatchInfo (via GetMatch()) - this represents our match in progress, and should generally mirror the GC, but
       *MIGHT NOT*.  For instance, when the GC is unavailable this object is locked, and when the GC returns we may be
       desync'd. This function is in charge of managing that.  Outside code should simply look at the MatchInfo object
       and trust that it is the state of the match.

     - m_vecReservationExpiryTime - this should be merged into MatchInfo eventually, but is an array of active
       reservations and when they expire.  This isn't in MatchInfo because in some modes we operate with reservations
       but without running a proper Match.  When we're running a match, anyone in this vector should be in the MatchInfo

     - CTFGSLobby - This is the shared object from the server that represents the match we are hosting.  However, it is
       *NOT* the article of record on the match.  This is due to matches being designed to be resilient to GC connection
       loss.  Essentially, only this function should be looking at CTFGSLobby and negotiating the state of the actual
       match it believes itself to have in MatchInfo.

   == Gameserver / GC Authority
      - GC forms matches, adds players to matches, passes them to servers
      - Servers run matches to completion, have authority on abandons/etc. regardless of GC state
      - Servers pass result, including any abandons, to GC.  Message is queued if GC is unavailable.
      - GC takes match results and does ELO calculation and any stats/etc.
      - GC can request players be kicked from matches or matches be canceled
      - If more players are needed
        - Gameserver requests GC attention with appropriate flag (6v6: Stalled, waiting on complete match, 12v12:
          Non-full match)
        - GC adds players to lobby, making them part of the match
      - If server state is poor (hypothetically: lag, too many abandons, abnormal something or other)
        - Game server sends KickLobby to terminate match, sends failed match result
      - If GC is unavailable
        - Game server still carries out duties, may decide to make changes like end match instead of request late joins
          if it decides GC wont be able to provide them.

   == Match Start
      - GC creates a lobby and hands it to us. UpdateConnectedPlayers tick initializes a MatchInfo struct as
        appropriate, accepts players.

   == Adding Players
      - The GC adds players to the lobby (so, when GC down, matches cannot gain players)
      - UpdateConnectedPlayersAndServerInfo ensures that makes sense (it should, though, we no longer have legacy match
        types where the GC adds players we shouldn't accept)
      - UpdateConnectedPlayers calls AcceptGCReservation, player is added to match and put in reservation list

   == Dropping Players
      - Case 1: Player is not present, but is in the lobby (GC *might* be down, doesn't matter)
        - Player marked missing in MatchInfo by UpdateConnectedPlayers tick
        - After a grace period, player marked dropped, as an abandoner in MatchInfo
        - PlayerLeftMatch message is sent to tell the GC about their leaving.
      - Case 2: Player is dropped from GC lobby
        - UpdateConnectedPlayers assumes GC kicked them, marks them dropped from match and kicks them.
        - TODO: Ideally there'd be a KickThisGuy GC message, and we'd respond with PlayerLeftMatch, rather than the GC
          unilaterally dropping people like this.
      - Case 3: Votekicked
        - PlayerLeftMatch is sent, from server
      - All cases:
        - A reliable GC message player-abandoned (or was kicked or never joined) message queued to reconcile this with
          the lobby state, but if GC is unavailable it will be informed when it returns.
        - Player is marked dropped in MatchInfo

   == Team Assignments
      - The GC delivers an initial team assignment for each player added to the match.  This team assignment does not
        change when game teams change sides, see TFGameRules::GameTeamToLobbyTeam and its inverse to map these to game
        logic teams (vs TF_GC_TEAM objects)

      - All other team changes have to be initiated by a game server message, in modes that allow it, to prevent
        race-conditions.

        - The NewMatchForLobby message expects the GC to shuffle our teams.  We prevent races by not issuing other team
          change messages while this message is pending.  If we time out waiting for the GC, some modes may start a
          speculative server-created match (expecting the GC to come back and respond to that message positively).  In
          this case, we queue a ChangeMatchPlayerTeams message to stomp any assignments back to our known state,
          allowing us to ignore the temporary de-sync (queued messages always get processed in sequence)

        - The ChangeMatchPlayerTeams message allows the gameserver to change match player teams mid-game in match modes
          that allow it.  The game server is in charge of not queuing this message in parallel with NewMatchForLobby
          above, or handling the potential race.

        - When processing either of these messages, the GC cancels any players that are awaiting acceptance by the
          game-server, and re-tries if necessary.  This prevents team changes from racing with player-joins which may
          have been predicated on differing team layouts.
          - The game server does not accept pending players or send any heartbeats until any queued messages have been
            responded to.  See Queued Messages below.

   == Match End
      - Match result message provides canonical record of match, is queued to send to GC when available.
      - GameServerKickingLobby message dissolves live match if GC is available/tracking it. Queued similarly.
        - ** This can happen before or after the match result.
        - In MvM, we send potentially multiple victory messages per match -- they can cycle missions and keep winning.
        - As of right now, in competitive, we end the match coincident with sending a match result.
      - Match ended doesn't necessarily kick players, so a dead/finished match will stick around on our end until
        everyone Disconnects, (or the game logic kicks them, e.g. MatchInfo->BEnded + a timeout)
      - Ended matches have queued a message to dissolve their lobby, though, so further GC interaction with the match is
        not possible, and players are allowed to leave (since they're now allowed to be put in a new match by the GC)

   == Queued Messages And Match State And Race Conditions
      - Since queued messages are sent in order until confirmed, the GC will always see (eventually) a coherent
        story. For instance:
        - PlayerLeftMatch - GC marks this player as leaving match
        - KickingLobby - GC marks match as finished, result pending
        - MatchResult (minus the two players who left) - GC finishes match accounting, marks match complete, missing
          players are already noted as leavers so their absence from the result is expected.
      - While messages are queued, we do not run the UpdateConnectedPlayers() think. This prevents having to worry about
        a fractal of potential edge cases -- we don't look at updated lobby data or send heartbeats while anything we're
        trying to tell the GC hasn't been confirmed.  This also means we won't send a heartbeat until all such actions
        have been confirmed.
        - GC message handlers for queued messages do have to handle possible races -- if the GC sends us players while
          we're sending a "Reassign Player Team" message, this behavior means we'll stubbornly wait for a response to
          the team message before acknowledging any players, allowing the GC to easily resolve the race (in this case,
          by canceling or retrying any attempted add-player-match actions)

   == Gameserver Crashes
      - If GC is available, it handles it, otherwise, match is lost.  Gameservers don't currently try to persist this
        state.

   == Match empties out
      - If the match is still going, it should reach ended as everyone in it gets timed out as an abandon.
      - If the GC is around, it will revoke the lobby once we inform it everyone has dropped.
      - Once the match is marked ended, and the GC concurs and deletes the lobby, we delete MatchInfo
        - If the GC is not around, we hang out on the completed match state until it is.  We can't exactly take new
          matches in the mean time. (but, see k_InvalidState_Timeout_With_Match)

\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\////////////////////////////////////////////////////////////
***********************************************************************************************************************/

static const char g_pszIdleKickString[] = "#TF_Idle_kicked";

//ConVar dota_force_upload_match_stats( "dota_force_upload_match_stats", "0", FCVAR_CHEAT, "If enabled, server will upload match stats even when there aren't human players on each side" );
//extern ConVar dota_force_bot_cycle;
extern CServerGameDLL g_ServerGameDLL;

// How long a player can be missing from a MM match before they are dropped and given an abandon. Set to -1 to disable.
ConVar tf_mm_player_disconnect_time_before_abandon( "tf_mm_player_disconnect_time_before_abandon", "180", FCVAR_DEVELOPMENTONLY );
// How quickly we should forgive a match player's disconnected time after they return. At a ratio of 10, 30 minutes of
// connected time would cancel out 3 minutes of disconnected type. Set to 0 to disable.
ConVar tf_mm_player_disconnect_time_forgive_ratio( "tf_mm_player_disconnect_time_forgive_ratio", "10", FCVAR_DEVELOPMENTONLY );
// Any disconnect, no matter for how long, should count as this many seconds of disconnected time. This is because the
// act of reconnecting can be more disruptive than just the absense -- ready-up timers reset, the game may
// pause/unpause, etc..
//
// Currently at 90 -- two rapid rejoins in a row, even with instant loading, will eat up your DC allowance.  Note that
// if you take at least 90s to rejoin/load anyway, this would have no effect.
ConVar tf_mm_player_disconnect_time_minimum_penalty( "tf_mm_player_disconnect_time_minimum_penalty", "90", FCVAR_DEVELOPMENTONLY );

ConVar tf_mm_next_map_result_hold_time( "tf_mm_next_map_result_hold_time", "7" );

ConVar tf_mvm_allow_abandon_after_seconds( "tf_mvm_allow_abandon_after_seconds", "600", FCVAR_DEVELOPMENTONLY );
ConVar tf_mvm_allow_abandon_below_players( "tf_mvm_allow_abandon_below_players", "5", FCVAR_DEVELOPMENTONLY );

ConVar tf_allow_server_hibernation( "tf_allow_server_hibernation", "1", FCVAR_NONE, "Allow the server to hibernate when empty." );

#ifdef STAGING_ONLY
ConVar tf_debug_xp_changes( "tf_debug_xp_changes", "0" );
#endif

//DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_CONSOLE, "Console" );

static CTFGCServerSystem s_TFGCServerSystem;
CTFGCServerSystem *GTFGCClientSystem() { return &s_TFGCServerSystem; }

//bool g_bServerReceivedGCWelcome = false;
int g_gcServerVersion = 0; // Version from the GC

static bool g_bWarnedAboutMaxplayersInMVM = false;

extern ConVar tf_mm_servermode;
extern ConVar tf_mm_trusted;
extern ConVar tf_mm_strict;

// Some reliable messages don't know the matchID yet when they are queued, but we should have it by time they send. This
// helper takes their current match ID and returns the one they should use, for use in OnPrepare().
//
// Returns the current match's ID if:
// - The msg's match ID is 0, and we now have a match ID
//
// Calls AbortInvalidMatchState if:
// - The msg's match ID is not zero, and different from the current match
// - Or they're both zero and we're in a match group that requires match IDs.
//
// NOTE We always wait for all pending messages before accepting new matches, so the above should hold unless something
//      got badly confused.  Only matches with bServerCreated start without knowing their match ID, and should know it
//      by time any message that needs it gets sent. (a previous message in queue should be requesting it)
static uint64 ReliableMsgCheckUpdateMatchID( uint64 nMsgMatchID )
{
	uint64 nCurrentMatchID = GTFGCClientSystem()->GetMatch()->m_nMatchID;
	Assert( !nMsgMatchID || nMsgMatchID == nCurrentMatchID );

	// If we were queued for a match we didn't know the ID of yet, we can now glom it
	if ( nCurrentMatchID && nMsgMatchID == 0 )
	{
		return nCurrentMatchID;
	}
	else if ( nCurrentMatchID != nMsgMatchID )
	{
		// Something is bad
		GTFGCClientSystem()->AbortInvalidMatchState();
	}
	else if ( !nCurrentMatchID && !nMsgMatchID )
	{
		auto *pMatchDesc = GetMatchGroupDescription( GTFGCClientSystem()->GetMatch()->m_eMatchGroup );
		if ( !pMatchDesc || pMatchDesc->BRequiresMatchID() )
		{
			GTFGCClientSystem()->AbortInvalidMatchState();
		}
	}

	return nMsgMatchID;
}

//-----------------------------------------------------------------------------
// Reliable messages
//-----------------------------------------------------------------------------
class ReliableMsgNewMatchForLobby
	: public CJobReliableMessageBase < ReliableMsgNewMatchForLobby,
	                                   CMsgGCNewMatchForLobbyRequest,  k_EMsgGC_NewMatchForLobbyRequest,
	                                   CMsgGCNewMatchForLobbyResponse, k_EMsgGC_NewMatchForLobbyResponse >
{
public:
	void OnReply( Reply_t &msgReply )
		{ GTFGCClientSystem()->NewMatchForLobbyResponse( msgReply.Body().success() ); }

	void OnPrepare()
		{ Assert( Msg().Body().current_match_id() == GTFGCClientSystem()->GetMatch()->m_nMatchID ); }

	const char *MsgName() { return "NewMatchForLobby"; }
	void InitDebugString( CUtlString &dbgStr )
	{
		dbgStr.Format( "Match %llx, Lobby %llx, Next Map %d",
		               Msg().Body().current_match_id(),
		               Msg().Body().lobby_id(),
		               Msg().Body().next_map_id() );
	}
};

//-----------------------------------------------------------------------------
class ReliableMsgChangeMatchPlayerTeams
	: public CJobReliableMessageBase < ReliableMsgChangeMatchPlayerTeams,
	                                   CMsgGCChangeMatchPlayerTeamsRequest, k_EMsgGC_ChangeMatchPlayerTeamsRequest,
	                                   CMsgGCChangeMatchPlayerTeamsResponse, k_EMsgGC_ChangeMatchPlayerTeamsResponse >
{
public:
	void OnReply( Reply_t &msgReply )
		{ GTFGCClientSystem()->ChangeMatchPlayerTeamsResponse( msgReply.Body().success() ); }

	// May have been queued for a pending match
	void OnPrepare() { Msg().Body().set_match_id( ReliableMsgCheckUpdateMatchID( Msg().Body().match_id() ) ); }

	const char *MsgName() { return "ChangeMatchPlayerTeams"; }
	void InitDebugString( CUtlString &dbgStr )
	{
		dbgStr.Format( "Match %llx, Lobby %llx, %d members",
		               Msg().Body().match_id(), Msg().Body().lobby_id(), Msg().Body().member_size() );
	}
};

//-----------------------------------------------------------------------------
class ReliableMsgMvMVictory
	: public CJobReliableMessageBase < ReliableMsgMvMVictory,
	                                   CMsgMvMVictory,            k_EMsgGCMvMVictory,
	                                   CMsgMvMMannUpVictoryReply, k_EMsgGCMvMVictoryReply >
{
public:
	const char *MsgName() { return "MvMVictory"; }
	void InitDebugString( CUtlString &dbgStr ) { dbgStr.Format( "Lobby %016llx", Msg().Body().lobby_id() ); }
};

//-----------------------------------------------------------------------------
class ReliableMsgGameServerKickingLobby
	: public CJobReliableMessageBase < ReliableMsgGameServerKickingLobby,
	                                   CMsgGameServerKickingLobby,         k_EMsgGCGameServerKickingLobby,
	                                   CMsgGameServerKickingLobbyResponse, k_EMsgGCGameServerKickingLobbyResponse >
{
public:
	// May have been queued for a pending match
	void OnPrepare() { Msg().Body().set_match_id( ReliableMsgCheckUpdateMatchID( Msg().Body().match_id() ) ); }
	const char *MsgName() { return "GameServerKickingLobby"; }
	void InitDebugString( CUtlString &dbgStr ) { dbgStr.Format( "Match %llx, Lobby %llx",
	                                                            Msg().Body().match_id(), Msg().Body().lobby_id() ); }
};

//-----------------------------------------------------------------------------
class ReliableMsgPlayerLeftMatch
	: public CJobReliableMessageBase < ReliableMsgPlayerLeftMatch,
	                                   CMsgPlayerLeftMatch,         k_EMsgGCPlayerLeftMatch,
	                                   CMsgPlayerLeftMatchResponse, k_EMsgGCPlayerLeftMatchResponse >
{
public:
	// May have been queued for a pending match
	void OnPrepare() { Msg().Body().set_match_id( ReliableMsgCheckUpdateMatchID( Msg().Body().match_id() ) ); }
	const char *MsgName() { return "PlayerLeftMatch"; }
	void InitDebugString( CUtlString &dbgStr ) { dbgStr.Format( "Player %s, Match %llx, Lobby %llx",
	                                                            CSteamID( Msg().Body().steam_id() ).Render(),
	                                                            Msg().Body().match_id(), Msg().Body().lobby_id() ); }
};

//-----------------------------------------------------------------------------
// Sent for players who where votekicked after leaving the match
//  - That is, were being votekicked when they left, it later passed, to resolve the race-condition by posthumously
//    upgrading their penalty GC-side)
class ReliableMsgPlayerVoteKickedAfterLeavingMatch
	: public CJobReliableMessageBase < ReliableMsgPlayerVoteKickedAfterLeavingMatch,
	                                   CMsgPlayerVoteKickedAfterLeavingMatch,         k_EMsgGCPlayerVoteKickedAfterLeavingMatch,
	                                   CMsgPlayerVoteKickedAfterLeavingMatchResponse, k_EMsgGCPlayerVoteKickedAfterLeavingMatchResponse >
{
public:
	// May have been queued for a pending match
	void OnPrepare() { Msg().Body().set_match_id( ReliableMsgCheckUpdateMatchID( Msg().Body().match_id() ) ); }
	const char *MsgName() { return "PlayerVoteKickedAfterLeavingMatch"; }
	void InitDebugString( CUtlString &dbgStr ) { dbgStr.Format( "Player %s, Match %llx, Lobby %llx",
	                                                            CSteamID( Msg().Body().steam_id() ).Render(),
	                                                            Msg().Body().match_id(), Msg().Body().lobby_id() ); }
};

//-----------------------------------------------------------------------------
class ReliableMsgMatchResult
	: public CJobReliableMessageBase < ReliableMsgMatchResult,
	                                   CMsgGC_Match_Result,         k_EMsgGC_Match_Result,
	                                   CMsgGC_Match_ResultResponse, k_EMsgGC_Match_ResultResponse >
{
public:
	// May have been queued for a pending match
	void OnPrepare() { Msg().Body().set_match_id( ReliableMsgCheckUpdateMatchID( Msg().Body().match_id() ) ); }
	const char *MsgName() { return "MatchResult"; }
	void InitDebugString( CUtlString &dbgStr ) { dbgStr.Format( "Match %016llx", Msg().Body().match_id() ); }
};

//-----------------------------------------------------------------------------
// CMvMVictoryInfo
//-----------------------------------------------------------------------------
void CMvMVictoryInfo::Init ( CTFGSLobby *pLobby )
{
	if ( !pLobby )
	{
		MMLog( "CTFGCServerSystem::MvMVictory() -- no lobby, so not sending results to GC\n" );
		return;
	}

	m_nLobbyId = pLobby->GetGroupID();
	m_sChallengeName = pLobby->GetMissionName();
#ifdef USE_MVM_TOUR
	if ( IsMannUpGroup( pLobby->GetMatchGroup() ) )
	{
		const char *pszTourName = pLobby->GetMannUpTourName();
		Assert( pszTourName );
		m_sMannUpTourOfDuty = pszTourName;
	}
#endif // USE_MVM_TOUR
	m_tEventTime = CRTime::RTime32TimeCur();

	m_vPlayerIds.RemoveAll();
	m_vSquadSurplus.RemoveAll();

	for ( int iMember = 0; iMember < pLobby->GetNumMembers(); iMember++ )
	{
		m_vPlayerIds.AddToTail( pLobby->GetMember( iMember ).ConvertToUint64() );
		m_vSquadSurplus.AddToTail( pLobby->GetMemberDetails( iMember )->squad_surplus() );
	}
}

//-----------------------------------------------------------------------------
// CCompetitiveMatchInfo
//-----------------------------------------------------------------------------
CMatchInfo::CMatchInfo( const CTFGSLobby *pLobby )
	: m_nMatchID( pLobby->GetMatchID() )
	, m_nLobbyID( pLobby->GetGroupID() )
	, m_eMatchGroup( pLobby->GetMatchGroup() )
	, m_uLobbyFlags( pLobby->GetFlags() )
	, m_uAverageRank( pLobby->Obj().average_rank() )
	, m_rtMatchCreated( CRTime::RTime32TimeCur() )
	, m_unEventTeamStatus( pLobby->Obj().is_war_match() )
	, m_bFirstPersonActive( false )
	, m_nBotsAdded( 0 )
	, m_bServerCreated( false )
	, m_strMapName( pLobby->GetMapName() )
	, m_bMatchEnded( false )
	, m_bSentResult( false )
	, m_nGCMatchSize( pLobby->Obj().has_fixed_match_size() ? pLobby->Obj().fixed_match_size() : 0 )
#ifdef STAGING_ONLY
	, m_flBronzePercentile( 0.5f )
	, m_flSilverPercentile( 0.65f )
	, m_flGoldPercentile( 0.8f )
#else
	, m_flBronzePercentile( 0.6f )
	, m_flSilverPercentile( 0.75f )
	, m_flGoldPercentile( 0.9f )
#endif
{
	uint32 nNumCompLevels = GetMatchGroupDescription( k_nMatchGroup_Casual_6v6 )->m_pProgressionDesc->GetNumLevels();
	m_vDailyStatsRankData.EnsureCapacity( nNumCompLevels );

	RequestGCRankData();
}

CMatchInfo::~CMatchInfo()
{
	m_vMatchRankData.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CMatchInfo::CMatchInfo()
{
	// Don't do this
	Assert( 0 );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CMatchInfo::CMatchInfo( const CMatchInfo &otherinfo )
{
	// Don't do this
	Assert( 0 );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CMatchInfo::PlayerMatchData_t::PlayerMatchData_t( const PlayerMatchData_t& rhs )
	: m_mapXPAccumulation( DefLessFunc( CMsgTFXPSource::XPSourceType ) )
{
	steamID = rhs.steamID;
	uPartyID = rhs.uPartyID;
	eGCTeam = rhs.eGCTeam;
	bDropped = rhs.bDropped;
	bConnected = rhs.bConnected;
	rtJoinedMatch = CRTime::RTime32TimeCur();
	nVoteKickAttempts = rhs.nVoteKickAttempts;
	nDisconnectedSeconds = 0;
	nScoreMedal = rhs.nScoreMedal;
	nKillsMedal = rhs.nKillsMedal;
	nDamageMedal = rhs.nDamageMedal;
	nHealingMedal = rhs.nHealingMedal;
	nSupportMedal = rhs.nSupportMedal;
	bLateJoin = rhs.bLateJoin;
	nScore = rhs.nScore;
	rtLastActiveEvent = CRTime::RTime32TimeCur();
	bAlwaysSafeToLeave = rhs.bAlwaysSafeToLeave;
	bEverConnected = rhs.bEverConnected;
	bDropWasAbandon = rhs.bDropWasAbandon;
	eDropReason = rhs.eDropReason;
	nConnectingButNotActiveIndex = rhs.nConnectingButNotActiveIndex;
	bPlayed = false;
	unMMSkillRating = rhs.unMMSkillRating;
	nDrilloRatingDelta = 0;
	unClassesPlayed = 0u;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
MM_PlayerConnectionState_t CMatchInfo::PlayerMatchData_t::GetConnectionState() const
{
	if ( bConnected )
	{
		return nConnectingButNotActiveIndex == 0 ? MM_CONNECTED : MM_LOADING;
	}
	else
	{
		return bEverConnected ? MM_DISCONNECTED : MM_CONNECTING;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatchInfo::PlayerMatchData_t::UpdateClassesPlayed( int nClass )
{
	Assert( nClass >= TF_FIRST_NORMAL_CLASS && nClass <= TF_LAST_NORMAL_CLASS );

	unClassesPlayed = unClassesPlayed | ( 1 << nClass );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatchInfo::PlayerMatchData_t::OnConnected( int nEntindex )
{
	if ( bConnected )
	{
		// This is before steamID validation, so make sure we don't add a path that would reward spoof connections.
		Assert( !"Player connecting is marked connected" );
		return;
	}

	nConnectingButNotActiveIndex = nEntindex;

	// Mark connected.
	bConnected = true;
	bEverConnected = true;

	RTime32 now = CRTime::RTime32TimeCur();
	MMLog( "Match player %s reconnected into slot %d, last active %u seconds ago.\n",
	       steamID.Render(), nConnectingButNotActiveIndex, now - rtLastActiveEvent );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatchInfo::PlayerMatchData_t::OnActive()
{
	nConnectingButNotActiveIndex = 0;
	CMatchInfo* pMatch = GTFGCClientSystem()->GetMatch();
	Assert( pMatch);
	if ( pMatch && !pMatch->m_bFirstPersonActive )
	{
		MMLog( "Match going active\n" );
		pMatch->m_bFirstPersonActive = true;
	}

	// Disconnected seconds for the time since they were last active, including DC'd time and time spent loading.  This
	// prevents people who crash but rejoin quickly being able to be not-in-game for far longer than intended.  Since we
	// already marked them connected, the abandon think won't touch them if this accumulation goes over the limit, but
	// it will count against them if they drop again.
	RTime32 now = CRTime::RTime32TimeCur();
	RTime32 missing = now - rtLastActiveEvent;
	// See this convar's comment for why we do this.
	RTime32 minimum = (RTime32)Clamp( tf_mm_player_disconnect_time_minimum_penalty.GetInt(), 0, INT_MAX );
	nDisconnectedSeconds += Max( missing, minimum );
	rtLastActiveEvent = now;
}

//-----------------------------------------------------------------------------
// Add a rank bucket stats vector
//-----------------------------------------------------------------------------
void CMatchInfo::SetDailyRankData( DailyStatsRankBucket_t vecRankData )
{
	m_vDailyStatsRankData.AddToTail( vecRankData );
}

//-----------------------------------------------------------------------------
// Request the competitive daily stats rollup from the GC
//-----------------------------------------------------------------------------
bool CMatchInfo::RequestGCRankData( void )
{
	if ( !GetMatchGroupDescription( m_eMatchGroup ) || 
		 !GetMatchGroupDescription( m_eMatchGroup )->m_params.m_bDistributePerformanceMedals )
	{
		return false;
	}

	GCSDK::CProtoBufMsg< CMsgGC_DailyCompetitiveStatsRollup > msg( k_EMsgGC_DailyCompetitiveStatsRollup );
	return GCClientSystem()->BSendMessage( msg );
}

//-----------------------------------------------------------------------------
void CMatchInfo::AddPlayer( const PlayerMatchData_t &player, int nEntIndex, bool bActive )
{
	PlayerMatchData_t* pOldPlayerMatchData = GetMatchDataForPlayer( player.steamID );
	if ( pOldPlayerMatchData )
	{
		// Already have data?
		if ( pOldPlayerMatchData->bDropped )
		{
			// Returning a player that had dropped from the match.  Re-create their entry as a fresh player, so the
			// constructor re-does everything.
			MMLog( "Player %s re-added to match they previously dropped from, replacing existing entry\n",
			       player.steamID.Render() );
			m_vMatchRankData.FindAndRemove( pOldPlayerMatchData );
			delete pOldPlayerMatchData;
			pOldPlayerMatchData = nullptr;
		}
		else
		{
			// This player is already in the match
			Assert( false );
			MMLog( "!! Player %s being added to the match, but they are already present\n",
			       player.steamID.Render() );
			return;
		}
	}

	PlayerMatchData_t* pPlayerMatchData = new PlayerMatchData_t( player );
	m_vMatchRankData.AddToTail( pPlayerMatchData );

	if ( nEntIndex != 0 )
	{
		pPlayerMatchData->OnConnected( nEntIndex );
	}

	if ( bActive )
	{
		pPlayerMatchData->OnActive();
	}
}

//-----------------------------------------------------------------------------
void CMatchInfo::AddPlayer( CSteamID steamID, const CTFLobbyMember *pMemberData, bool bIsLateJoin, int nEntIndex, bool bActive )
{
	PlayerMatchData_t playerMatchData( steamID, pMemberData );
	playerMatchData.unMMSkillRating = pMemberData->skillrating();
	playerMatchData.bLateJoin = bIsLateJoin;

	AddPlayer( playerMatchData, nEntIndex, bActive );
}

//-----------------------------------------------------------------------------
void CMatchInfo::DropPlayer( CSteamID steamID, TFMatchLeaveReason eReason, bool bWasAbandon )
{
	CMatchInfo::PlayerMatchData_t *pPlayerMatchData = GetMatchDataForPlayer( steamID );

	AssertMsg( pPlayerMatchData, "If we have competitive match info, this player should be known" );

	if ( pPlayerMatchData )
	{
		if ( pPlayerMatchData->bDropped )
		{
			MMLog( "!! Double-dropping player %s\n", steamID.Render() );
			Assert( false );
		}
		pPlayerMatchData->bDropped = true;
		pPlayerMatchData->eDropReason = eReason;
		pPlayerMatchData->bDropWasAbandon = bWasAbandon;
	}
}

//-----------------------------------------------------------------------------
const CMatchInfo::PlayerMatchData_t* CMatchInfo::GetMatchDataForPlayer( CSteamID steamID ) const
{
	return const_cast<CMatchInfo*>(this)->GetMatchDataForPlayer( steamID );
}

//-----------------------------------------------------------------------------
CMatchInfo::PlayerMatchData_t* CMatchInfo::GetMatchDataForPlayer( CSteamID steamID )
{
	FOR_EACH_VEC( m_vMatchRankData, i )
	{
		if ( m_vMatchRankData[i]->steamID == steamID )
			return ( m_vMatchRankData[i] );
	}

	return NULL;
}

//-----------------------------------------------------------------------------
CMatchInfo::PlayerMatchData_t* CMatchInfo::GetMatchDataForPlayer( int idx )
{
	return m_vMatchRankData[idx];
}

//-----------------------------------------------------------------------------
int CMatchInfo::GetNumTotalMatchPlayers() const
{
	return m_vMatchRankData.Count();
}

//-----------------------------------------------------------------------------
int CMatchInfo::GetNumActiveMatchPlayers() const
{
	int nActivePlayers = 0;
	FOR_EACH_VEC( m_vMatchRankData, idx )
	{
		nActivePlayers += !m_vMatchRankData[idx]->bDropped;
	}
	return nActivePlayers;
}

//-----------------------------------------------------------------------------
int CMatchInfo::GetNumActiveMatchPlayersForTeam( int nTeam ) const
{
	int nActivePlayers = 0;
	FOR_EACH_VEC( m_vMatchRankData, idx )
	{
		if ( !m_vMatchRankData[idx]->bDropped )
		{
			if ( m_vMatchRankData[idx]->eGCTeam == nTeam )
			{
				nActivePlayers++;
			}
		}
	}
	return nActivePlayers;
}

//-----------------------------------------------------------------------------
int CMatchInfo::GetTotalSkillRatingForTeam( int nTeam ) const
{
	// Re-evaluate this when skillrating might be for other backends
	FixmeMMRatingBackendSwapping();
	int nSkillRating = 0;

	FOR_EACH_VEC( m_vMatchRankData, idx )
	{
		if ( !m_vMatchRankData[idx]->bDropped )
		{
			if ( m_vMatchRankData[idx]->eGCTeam == nTeam )
			{
				nSkillRating += m_vMatchRankData[idx]->unMMSkillRating;
			}
		}
	}

	return nSkillRating;
}

//-----------------------------------------------------------------------------
int CMatchInfo::GetNumConnectedMatchPlayers() const
{
	int nConnectedPlayers = 0;
	FOR_EACH_VEC( m_vMatchRankData, idx )
	{
		nConnectedPlayers += ( m_vMatchRankData[idx]->bConnected && !m_vMatchRankData[idx]->bDropped );
	}
	return nConnectedPlayers;
}

//-----------------------------------------------------------------------------
uint32 CMatchInfo::GetCanonicalMatchSize() const
{
	return m_nGCMatchSize ? m_nGCMatchSize : GetMatchGroupDescription( m_eMatchGroup )->GetMatchSize();
}

//-----------------------------------------------------------------------------
void CMatchInfo::GiveXPRewardToPlayerForAction( CSteamID steamID, CMsgTFXPSource::XPSourceType eType, int nCount )
{
	// Needs to be a positive number!
	if ( nCount <= 0 )
		return;
	GiveXPDirectly( steamID, eType, ceil( (float)nCount * g_XPSourceDefs[ eType ].m_flValueMultiplier ), true );
}

//-----------------------------------------------------------------------------
void CMatchInfo::GiveXPDirectly( CSteamID steamID, CMsgTFXPSource::XPSourceType eType, int nAmount, bool bCanAwardBonusXP )
{
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_eMatchGroup );
	if ( !pMatchDesc || !pMatchDesc->BUsesXP() || nAmount <= 0 )
	{
		return;
	}

	PlayerMatchData_t *pMatchPlayer = GetMatchDataForPlayer( steamID );

	if ( pMatchPlayer && !pMatchPlayer->bDropped )
	{
		CMsgTFXPSource* pSource = NULL;

		auto idx = pMatchPlayer->m_mapXPAccumulation.Find( eType );
		if ( idx == pMatchPlayer->m_mapXPAccumulation.InvalidIndex() )
		{
			idx = pMatchPlayer->m_mapXPAccumulation.Insert( eType, 0.f );
		}

		// You can only draw from the bonus pool if you GAINED xp
		if ( nAmount > 0 && bCanAwardBonusXP )
		{
			FOR_EACH_VEC_BACK( pMatchPlayer->m_vecXPBonusPools, i )
			{
				PlayerMatchData_t::XPBonusPool_t& xpMultiplier = pMatchPlayer->m_vecXPBonusPools[ i ];
			
				// We do this so when specifying the multiplier, you can say you want the multiplier to be
				int nBonusAmount = ceil( nAmount * xpMultiplier.m_flMultiplier );
			
				// If there's a maximum amount to give for this bonus, subtract from the total
				// and remove this bonus if the pool is emptied
				Assert( xpMultiplier.m_nBonusPoolRemaining > 0 );
				nBonusAmount = Min( nBonusAmount, xpMultiplier.m_nBonusPoolRemaining );
				xpMultiplier.m_nBonusPoolRemaining -= nBonusAmount;

				// Save the type so we can recursively pass it below
				CMsgTFXPSource::XPSourceType eBonusType = xpMultiplier.m_eType;
				// If there's no more in the pool, then we can remove this from the list
				if ( xpMultiplier.m_nBonusPoolRemaining <= 0 )
				{
					// We're going backwards, so this is ok
					pMatchPlayer->m_vecXPBonusPools.Remove( i );
				}

				// Give the bonus
				GiveXPDirectly( steamID, eBonusType, nBonusAmount, false );
			}
		}

		// Accumulate in the map
		pMatchPlayer->m_mapXPAccumulation[ idx ] += nAmount;
		int nAccum = pMatchPlayer->m_mapXPAccumulation[ idx ];
		// Don't make a XPSource proto object if there's nothing to even report
		if ( nAccum == 0 )
			return;

		// Find the type if it exists.  
		for( int i=0; i < pMatchPlayer->m_XPBreakdown.sources_size(); ++i )
		{
			if ( pMatchPlayer->m_XPBreakdown.sources( i ).type() == eType )
			{
				pSource = pMatchPlayer->m_XPBreakdown.mutable_sources( i );
				break;
			}
		}

		// Create a new one if we need to
		if ( pSource == NULL )
		{
			pSource = pMatchPlayer->m_XPBreakdown.add_sources();
			pSource->set_account_id( steamID.GetAccountID() );
			pSource->set_match_group( m_eMatchGroup );
			pSource->set_type( eType );
			pSource->set_match_id( m_nMatchID );
			pSource->set_amount( 0 );
		}

#ifdef STAGING_ONLY
		if ( tf_debug_xp_changes.GetBool() && nAccum != pSource->amount() )
		{
			CBasePlayer* pPlayer = UTIL_PlayerBySteamID( steamID );
			if ( pPlayer )
			{
				Msg( "%s received %d %s xp\n", pPlayer->GetPlayerName(), 
											   nAccum - pSource->amount(),
											   CMsgTFXPSource_XPSourceType_descriptor()->value( eType )->name().c_str() );
			}
		}
#endif

		// Update the amount
		pSource->set_amount( nAccum );
	}
}

//-----------------------------------------------------------------------------
void CMatchInfo::GiveXPBonus( CSteamID steamID,
							  CMsgTFXPSource_XPSourceType eType,
							  float flMultipler,
							  int nBonusPool )
{
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_eMatchGroup );
	if ( !pMatchDesc || !pMatchDesc->BUsesXP() )
	{
		return;
	}

	PlayerMatchData_t *pMatchPlayer = GetMatchDataForPlayer( steamID );

	if ( pMatchPlayer && !pMatchPlayer->bDropped )
	{
		// Find existing entry if there is one
		auto idx = pMatchPlayer->m_vecXPBonusPools.InvalidIndex();
		FOR_EACH_VEC( pMatchPlayer->m_vecXPBonusPools, i )
		{
			// Found it
			if( pMatchPlayer->m_vecXPBonusPools[ i ].m_eType == eType )
			{
				idx = i;
				break;
			}
		}

		// Create new entry if we didnt have an existing one
		if ( idx == pMatchPlayer->m_vecXPBonusPools.InvalidIndex() )
		{
			idx = pMatchPlayer->m_vecXPBonusPools.AddToTail();
		}

		// Add bonus
		PlayerMatchData_t::XPBonusPool_t& currentXPMultiplier = pMatchPlayer->m_vecXPBonusPools[ idx ];
		currentXPMultiplier.m_nBonusPoolRemaining += nBonusPool;
		currentXPMultiplier.m_eType = eType;
		currentXPMultiplier.m_flMultiplier = Max( currentXPMultiplier.m_flMultiplier, flMultipler );
	}
}

#ifdef STAGING_ONLY
CON_COMMAND( give_xp_bonus, "Gives the player with the specified name an xp boost.  Usage: give_xp_bonus <name> <type> <multiplier> <bonus_pool>" )
{
	if ( args.ArgC() != 5 )
	{
		Msg( "Incorrect arguments. Usage: give_xp_bonus <name> <type> <multiplier> <bonus_pool>\n" );
		return;
	}

	CBasePlayer* pPlayer = UTIL_PlayerByName( args.Arg( 1 ) );
	if ( !pPlayer )
	{
		Msg( "No player named %s\n", args.Arg( 1 ) );
		return;
	}

	if ( !GTFGCClientSystem()->GetMatch() )
	{
		Msg( "Not running a match\n" );
		return;
	}

	CMsgTFXPSource_XPSourceType nType = (CMsgTFXPSource_XPSourceType)atoi( args.Arg( 2 ) );

	if ( nType < CMsgTFXPSource_XPSourceType_XPSourceType_MIN
		|| nType >= CMsgTFXPSource_XPSourceType_NUM_SOURCE_TYPES )
	{
		Msg( "Type is not a valid type!\n" );
		return;
	}

	CSteamID steamID;
	pPlayer->GetSteamID( &steamID );
	GTFGCClientSystem()->GetMatch()->GiveXPBonus( steamID, 
												  nType,
												  atof( args.Arg( 3 ) ),
												  atoi( args.Arg( 4 ) ) );
}
#endif 

//-----------------------------------------------------------------------------
bool CMatchInfo::BPlayerSafeToLeaveMatch( CSteamID steamID )
{
	PlayerMatchData_t *pMatchPlayer = this->GetMatchDataForPlayer( steamID );

	// Right now, you cannot leave while the match is running
	bool bSafe = m_bMatchEnded || !pMatchPlayer || pMatchPlayer->bDropped || pMatchPlayer->bAlwaysSafeToLeave;

	// The match description might have special exceptions
	if ( !bSafe && pMatchPlayer )
	{
		bSafe = bSafe || GetMatchGroupDescription( m_eMatchGroup )->BMatchIsSafeToLeaveForPlayer( this, pMatchPlayer );
	}

	return bSafe;
}

//-----------------------------------------------------------------------------
// Determine the performance ranking of each player after a competitive match
//-----------------------------------------------------------------------------
bool CMatchInfo::CalculatePlayerMatchRankData( void )
{
	Assert( TFGameRules() );
	if ( !TFGameRules() )
		return false;

	CTFPlayerResource *pTFResource = dynamic_cast< CTFPlayerResource* >( g_pPlayerResource );
	if ( !pTFResource )
		return false;

	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	if ( !pMatch )
		return false;

	if ( !m_vDailyStatsRankData.Count() )
	{
		Warning( "CalculatePlayerMatchRankData(): DailyStatsRankData is empty\n" );
		return false;
	}

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( pMatch->m_eMatchGroup );
	if ( !pMatchDesc || 
		 !pMatchDesc->m_pProgressionDesc || 
		 !pMatchDesc->m_params.m_bDistributePerformanceMedals )
	{
		return false;
	}

	CUtlVector < CTFPlayer* > vecPlayers;
	CollectHumanPlayers( &vecPlayers );
	FOR_EACH_VEC( vecPlayers, i )
	{
		if ( !vecPlayers[i] )
			continue;

		CSteamID steamID;
		if ( !vecPlayers[i]->GetSteamID( &steamID ) )
			continue;

		PlayerStats_t *pStats = CTF_GameStats.FindPlayerStats( vecPlayers[i] );
		CMatchInfo::PlayerMatchData_t *matchData = GetMatchDataForPlayer( steamID );
		if ( !matchData || !pStats )
		{
			Warning( "Missing player data in CalculatePlayerMatchRankData\n" );
			Assert( false );
			continue;
		}

		// Get player's competitive rank
		FixmeMMRatingBackendSwapping(); // This is assuming we're using primary skill rating for rank
		uint32 unRank = pMatchDesc->m_pProgressionDesc->GetLevelForExperience( matchData->unMMSkillRating ).m_nLevelNum;
		int nRankIndex = -1;

		// Let's find the typical stats for your rank
		FOR_EACH_VEC( m_vDailyStatsRankData, j )
		{
			if ( unRank == m_vDailyStatsRankData[j].nRank )
			{
#ifndef STAGING_ONLY
				if ( m_vDailyStatsRankData[j].nRecords < 10 )
				{
					Warning( "CalculatePlayerMatchRankData(): Too few stat entries (%d) for rank %d\n", m_vDailyStatsRankData[j].nRecords, unRank );
					return false;
				}
#endif // !STAGING_ONLY

				nRankIndex = j;
				break;
			}
		}

		uint32 unScoreMedal = GetRankForStat( RankStat_Score, nRankIndex, pTFResource->GetTotalScore( vecPlayers[i]->entindex() ) );
		uint32 unKillsMedal = GetRankForStat( RankStat_Kills, nRankIndex, pStats->statsAccumulated.m_iStat[TFSTAT_KILLS] );
		uint32 unDamageMedal = GetRankForStat( RankStat_Damage, nRankIndex, pStats->statsAccumulated.m_iStat[TFSTAT_DAMAGE] );
		uint32 unHealingMedal = GetRankForStat( RankStat_Healing, nRankIndex, pStats->statsAccumulated.m_iStat[TFSTAT_HEALING] );
		uint32 unSupportMedal = GetRankForStat( RankStat_Support, nRankIndex, TFGameRules()->CalcPlayerSupportScore( &pStats->statsAccumulated, vecPlayers[i]->entindex() ) );

		matchData->nScoreMedal = unScoreMedal;
		matchData->nKillsMedal = unKillsMedal;
		matchData->nDamageMedal = unDamageMedal;
		matchData->nHealingMedal = unHealingMedal;
		matchData->nSupportMedal = unSupportMedal;
	}

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMatchInfo::CalculateMatchSkillRatingAdjustments( int iWinningTeam )
{
	// This is assuming skill rating is drillo,and doing a client-side prediction on it
	FixmeMMRatingBackendSwapping();
	if ( !iWinningTeam )
	{
		Log( "CalculateMatchSkillRatingAdjustments(): Invalid team!\n" );
		return false;
	}

	EMatchGroup matchGroup = m_eMatchGroup;
	if ( !IsLadderGroup( matchGroup ) )
	{
		Assert( false );
		Log( "CalculateMatchSkillRatingAdjustments(): Match %llu has an invalid MatchGroup (%i)\n", m_nMatchID, (int)matchGroup );
		return false;
	}

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( matchGroup );
	if ( !pMatchDesc || !pMatchDesc->m_pProgressionDesc )
	{
		Log( "CalculateMatchSkillRatingAdjustments(): Match has bogus MatchGroupDescription\n" );
		return false;
	}

	CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();
	if ( !pMatch )
	{
		Log( "CalculateMatchSkillRatingAdjustments(): Match has bogus CMatchInfo\n" );
		return false;
	}

	int nWinnerTotal = 0;
	int nLoserTotal = 0;
	uint32 unWinningPlayers = 0u;
	uint32 unLosingPlayers = 0u;

	// Gather data so we can figure out rating adjustments
	for ( int i = 0; i < GetNumTotalMatchPlayers(); i++ )
	{
		CMatchInfo::PlayerMatchData_t *pPlayerInfo = GetMatchDataForPlayer( i );
		Assert( pPlayerInfo );
		if ( !pPlayerInfo || pPlayerInfo->bDropped )
			continue;

		if ( TFGameRules()->GetGameTeamForGCTeam( pPlayerInfo->eGCTeam ) == iWinningTeam )
		{
			nWinnerTotal += pPlayerInfo->unMMSkillRating;
			++unWinningPlayers;
		}
		else
		{
			nLoserTotal += pPlayerInfo->unMMSkillRating;
			++unLosingPlayers;
		}
	}

	if ( pMatchDesc->m_params.m_bRequireCompleteMatch && ( unWinningPlayers + unLosingPlayers != GetCanonicalMatchSize() ) )
	{
		Assert( false );
		Log( "CalculateMatchSkillRatingAdjustments(): Match %llu has invalid team size(s): %d vs %d\n",
			m_nMatchID, unWinningPlayers, unLosingPlayers );
	}

	int nTeamSize = ( pMatch->GetCanonicalMatchSize() % 2 ) ? ( pMatch->GetCanonicalMatchSize() / 2 + 1 ) : ( pMatch->GetCanonicalMatchSize() / 2 );
	int nWinningTeamAverage = (float)nWinnerTotal / Max( nTeamSize, 1 );
	int nLosingTeamAverage = (float)nLoserTotal / Max( nTeamSize, 1 );
	int nRatingDiff = nLosingTeamAverage - nWinningTeamAverage;

	// Determine adjustment based on difference between teams
	const int nChange = RemapValClamped( nRatingDiff, /* from */ -(float)k_unDrilloRating_MaxDifference, (float)k_unDrilloRating_MaxDifference,
													  /* to   */ (float)k_nDrilloRating_MinRatingAdjust, (float)k_nDrilloRating_Ladder_MaxRatingAdjust );

	// Cap loss for low-rated teams, but not low-rated winners.  This breaks the loose "sort-of-zero-sum" system we have, but that's ok in the lower range.
	const int nLoserChange = ( nLosingTeamAverage <= k_unDrilloRating_Ladder_LowSkill ) ? Min( nChange, k_nDrilloRating_Ladder_MaxLossAdjust_LowRank ) : nChange;

	// Rating delta update
	for ( int i = 0; i < GetNumTotalMatchPlayers(); i++ )
	{
		CMatchInfo::PlayerMatchData_t *pPlayerInfo = GetMatchDataForPlayer( i );
		Assert( pPlayerInfo );
		if ( !pPlayerInfo )
			continue;

		int nAmount = nChange;
		if ( pPlayerInfo->BDropWasAbandon() )
		{
			// Abandon
			nAmount = -k_nDrilloRating_Ladder_MaxRatingAdjust;
			if ( m_eMatchGroup == k_nMatchGroup_Ladder_6v6 )
			{
				GiveXPDirectly( pPlayerInfo->steamID, CMsgTFXPSource_XPSourceType::CMsgTFXPSource_XPSourceType_SOURCE_COMPETITIVE_ABANDON, nAmount );
			}
		}
		else if ( TFGameRules()->GetGameTeamForGCTeam( pPlayerInfo->eGCTeam ) != iWinningTeam )
		{
			// Loss
			nAmount = -nLoserChange;
		}

		pPlayerInfo->nDrilloRatingDelta = nAmount;

		// Scoreboard
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "competitive_stats_update" );
		if ( pEvent )
		{
			CBasePlayer *pPlayer = UTIL_PlayerBySteamID( pPlayerInfo->steamID );
			if ( !pPlayer )
				continue;

			pEvent->SetInt( "index", pPlayer->entindex() );
			pEvent->SetInt( "rating", pPlayerInfo->unMMSkillRating );
			// This is the only place this guy is used. We should eventually have the GC send down results and use that
			// instead of running this prediction step here.
			pEvent->SetInt( "delta", pPlayerInfo->nDrilloRatingDelta );
			CMatchInfo::PlayerMatchData_t *pMatchRankData = GetMatchDataForPlayer( pPlayerInfo->steamID );
			pEvent->SetInt( "score_rank", pMatchRankData ? pMatchRankData->nScoreMedal : 0 );		// medal won (if any)
			pEvent->SetInt( "kills_rank", pMatchRankData ? pMatchRankData->nKillsMedal : 0 );		//
			pEvent->SetInt( "damage_rank", pMatchRankData ? pMatchRankData->nDamageMedal : 0 );		//
			pEvent->SetInt( "healing_rank", pMatchRankData ? pMatchRankData->nHealingMedal : 0 );	//
			pEvent->SetInt( "support_rank", pMatchRankData ? pMatchRankData->nSupportMedal : 0 );	//
			gameeventmanager->FireEvent( pEvent );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Returns the medal rank (if any) for this stat
//-----------------------------------------------------------------------------
int CMatchInfo::GetRankForStat( RankStatType_t statType, int nRankIndex, uint32 nValue )
{
	if ( !m_vDailyStatsRankData.IsValidIndex( nRankIndex ) )
		return StatMedal_None;

	// Get match duration, so we can scale values accordingly (total time won't have last round time included yet)
	uint16 nMatchDuration = CTF_GameStats.m_currentMap.m_Header.m_iTotalTime + ( gpGlobals->curtime - TFGameRules()->GetRoundStart() );

	// Assume 9 minute average match duration; TO DO: Use actual values generated from matchresults table
	uint16 nAverageMatchDuration = 9 * 60;

	// Adjusted Value
	float flStatAdjustment = ( float ) nAverageMatchDuration /  ( float ) nMatchDuration;
	flStatAdjustment = clamp( flStatAdjustment, 0.33f, 3.0f );

	nValue = nValue * flStatAdjustment;

	uint32 unStatAvg = m_vDailyStatsRankData[nRankIndex].nAvgScore;
	uint32 unStatStdDev = m_vDailyStatsRankData[nRankIndex].nStDevScore;

	switch ( statType )
	{
	case RankStat_Score:
		break;
	case RankStat_Kills:
		unStatAvg = m_vDailyStatsRankData[nRankIndex].nAvgKills;
		unStatStdDev = m_vDailyStatsRankData[nRankIndex].nStDevKills;
		break;
	case RankStat_Damage:
		unStatAvg = m_vDailyStatsRankData[nRankIndex].nAvgDamage;
		unStatStdDev = m_vDailyStatsRankData[nRankIndex].nStDevDamage;
		break;
	case RankStat_Healing:
		unStatAvg = m_vDailyStatsRankData[nRankIndex].nAvgHealing;
		unStatStdDev = m_vDailyStatsRankData[nRankIndex].nStDevHealing;
		break;
	case RankStat_Support:
		unStatAvg = m_vDailyStatsRankData[nRankIndex].nAvgSupport;
		unStatStdDev = m_vDailyStatsRankData[nRankIndex].nStDevSupport;
		break;
	default:
		Assert( 0 );
		return 0;
	}

	if ( !unStatAvg || !unStatStdDev )
		return 0;

	int nMedalRank = StatMedal_None;

	// Non-zero value?
	if ( unStatAvg  && unStatStdDev )
	{
		int nDelta = nValue - unStatAvg;
		if ( nDelta > 0 )
		{
			float flPercentile = NormalDistributionCDF( (float) nValue, (float) unStatAvg, (float) unStatStdDev );

			if ( flPercentile >= m_flGoldPercentile )
			{
				nMedalRank = StatMedal_Gold;
			}
			else if ( flPercentile >= m_flSilverPercentile )
			{
				nMedalRank = StatMedal_Silver;
			}
			else if ( flPercentile >= m_flBronzePercentile )
			{
				nMedalRank = StatMedal_Bronze;
			}

			// TODO:
			// - Stat must be "n" std deviations above the match average, too (anti-farming)
			// - Match must qualify:
			//   - Less than "n" minutes
			//   - At least "x" of "y" players at match end (no leavers?)
		}
	}

	return clamp( nMedalRank, StatMedal_None, StatMedal_Gold );
}


float CMatchInfo::NormalDistributionCDF( float flValue, float flMu, float flSigma )
{
	if ( flSigma <= 0.f )
		return 0.5f;

	return 0.5f * ( 1.f + erf( ( flValue - flMu ) / ( flSigma * sqrt( 2.f ) ) ) );
}


//-----------------------------------------------------------------------------
// CGCCompetitiveDailyStatsRollupJob
//-----------------------------------------------------------------------------
class CGCCompetitiveDailyStatsRollupJob : public GCSDK::CGCClientJob
{
public:
	CGCCompetitiveDailyStatsRollupJob( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg< CMsgGC_DailyCompetitiveStatsRollup_Response > msg( pNetPacket );

		CMatchInfo *pInfo = GTFGCClientSystem()->GetMatch();
		if ( !pInfo )
			return false;

		// Empty rankdata is valid (GC runs checks that might cause this as people reach new ranks)
		for ( int i = 0; i < msg.Body().rankdata_size(); i++ )
		{
			CMatchInfo::DailyStatsRankBucket_t rankBucket = {
				msg.Body().rankdata( i ).rank(),
				msg.Body().rankdata( i ).records(),
				msg.Body().rankdata( i ).avg_score(),
				msg.Body().rankdata( i ).stdev_score(),
				msg.Body().rankdata( i ).avg_kills(),
				msg.Body().rankdata( i ).stdev_kills(),
				msg.Body().rankdata( i ).avg_damage(),
				msg.Body().rankdata( i ).stdev_damage(),
				msg.Body().rankdata( i ).avg_healing(),
				msg.Body().rankdata( i ).stdev_healing(),
				msg.Body().rankdata( i ).avg_support(),
				msg.Body().rankdata( i ).stdev_support()
			};

			pInfo->SetDailyRankData( rankBucket );
		}

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCCompetitiveDailyStatsRollupJob, "CGCCompetitiveDailyStatsRollupJob", k_EMsgGC_DailyCompetitiveStatsRollup_Response, k_EServerTypeGCClient );

//-----------------------------------------------------------------------------
// CGCVoteSystemVoteKickResponse
//-----------------------------------------------------------------------------
class CGCVoteSystemVoteKickResponse : public GCSDK::CGCClientJob
{
public:
	CGCVoteSystemVoteKickResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) {}

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg< CMsgGC_VoteKickPlayerRequestResponse > msg( pNetPacket );
		if ( g_voteController )
		{
			g_voteController->GCResponseReceived( msg.Body().allowed() );
		}

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCVoteSystemVoteKickResponse, "CGCVoteSystemVoteKickResponse", k_EMsgGCVoteKickPlayerRequestResponse, k_EServerTypeGCClient );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CGCKickPlayerFromLobbyJob : public GCSDK::CGCClientJob
{
public:
	CGCKickPlayerFromLobbyJob( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGC_KickPlayerFromLobby> msg( pNetPacket );

		CSteamID steamID( msg.Body().targetid() );
		if ( steamID.IsValid() )
		{
			GTFGCClientSystem()->EjectMatchPlayer( steamID, TFMatchLeaveReason_ADMIN_KICK );
		}

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCKickPlayerFromLobbyJob, "CGCKickPlayerFromLobbyJob", k_EMsgGC_KickPlayerFromLobby, GCSDK::k_EServerTypeGCClient );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFGCServerSystem::CTFGCServerSystem()
	: m_flTimeRequestedLateJoin( -1.f )
	, m_bLateJoinEligible( false )
	, m_iSavedVisibleMaxPlayers( -1 )
	, m_bOverridingVisibleMaxPlayers( false )
	, m_bWaitingForNewMatchID( false )
	, m_flWaitingForNewMatchTime( 0.f )
{
	// replace base GCClientSystem
	SetGCClientSystem( this );

	m_unGameStartTime = 0;
	m_bSetupSchema = false;
	m_timeLastSendGameServerInfoAndConnectedPlayers = 0;
	//m_flUpdateGCGameTime = 0;
	//m_nUploadingMatchStats = EDOTA_MATCH_STATS_IDLE;
	//m_nParentRelayCount = 0;
	//m_nLastUpdateGCServerType = -1;
	m_eLastGameServerUpdateState = ServerMatchmakingState_NOT_PARTICIPATING;
	m_eLastGameServerUpdateMatchmakingMode = TF_Matchmaking_MVM;
	m_nLastGameServerUpdateBotCount = -1;
	m_nLastGameServerUpdateMaxHumans = -1;
	m_nLastGameServerUpdateSlotsFree = -1;
	m_nLastGameServerUpdateLobbyMMVersion = 0;
	m_flTimeBecameEmptyWithLobby = 0.0f;
	m_timeLastConnectedToGC = 0.f;
	m_pMatchInfo = NULL;

	g_bWarnedAboutMaxplayersInMVM = false;
}


CTFGCServerSystem::~CTFGCServerSystem( void )
{
	// Prevent other system from using this pointer after it's destroyed
	SetGCClientSystem( NULL );

	if ( m_pMatchInfo )
	{
		delete m_pMatchInfo;
	}
}


bool CTFGCServerSystem::Init()
{
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_score_changed" );

	g_bWarnedAboutMaxplayersInMVM = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGCServerSystem::PreInitGC()
{
	BaseClass::PreInitGC();

	if ( !m_bSetupSchema )
	{
//		REG_SHARED_OBJECT_SUBCLASS( CDOTAHeroStandings );
//		REG_SHARED_OBJECT_SUBCLASS( CDOTAGameAccountClient );
		REG_SHARED_OBJECT_SUBCLASS( CTFGSLobby );
		REG_SHARED_OBJECT_SUBCLASS( CTFParty );

		m_bSetupSchema = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGCServerSystem::PostInitGC()
{
	BaseClass::PostInitGC();
}


//-----------------------------------------------------------------------------
void CTFGCServerSystem::LevelShutdownPostEntity()
{
	BaseClass::LevelShutdownPostEntity();
}


//-----------------------------------------------------------------------------
void CTFGCServerSystem::Shutdown()
{
	BaseClass::Shutdown();

	// Remove listener, if we have one
	if ( m_ourSteamID.IsValid() )
	{
		GCClientSystem()->GetGCClient()->RemoveSOCacheListener( m_ourSteamID, this );
	}
}

void CTFGCServerSystem::LevelInitPreEntity()
{
	BaseClass::LevelInitPreEntity();
//	Assert( m_nUploadingMatchStats != EDOTA_MATCH_STATS_UPLOADING );
//	if ( m_nUploadingMatchStats == EDOTA_MATCH_STATS_UPLOADING )
//	{
//		Warning( "Error, changed level while waiting for match stats to upload!\n" );
//		return;
//	}
//	m_nUploadingMatchStats = EDOTA_MATCH_STATS_IDLE;
}


//-----------------------------------------------------------------------------
void CTFGCServerSystem::ClientActive( CSteamID steamIDClient )
{
	if ( !steamIDClient.IsValid() || !steamIDClient.BIndividualAccount() )
	{
		if ( !HushAsserts() )
		{
			Assert( steamIDClient.IsValid() );
			Assert( steamIDClient.BIndividualAccount() );
		}
		return;
	}

	CMatchInfo *pMatch = GetMatch();
	CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch ? pMatch->GetMatchDataForPlayer( steamIDClient ) : NULL;
	if ( !pMatchPlayer )
		return;

	pMatchPlayer->OnActive();

	// Only subscribe to match players' SOCaches.  They're the only ones who will have
	// parties that we care about.
	GetGCClient()->AddSOCacheListener( steamIDClient, this );
}

//-----------------------------------------------------------------------------
void CTFGCServerSystem::ClientConnected( CSteamID steamIDClient, edict_t *pEntity )
{
	// Note that we won't be notified of players connecting with unknown steamIDs, SteamIDAllowedToConnect() should be
	// used to reject those in a strict MM scenario where that is not acceptable.
	CMatchInfo *pMatch = GetMatch();
	CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch ? pMatch->GetMatchDataForPlayer( steamIDClient ) : NULL;
	if ( !pMatchPlayer )
		return;

	pMatchPlayer->OnConnected( pEntity->m_EdictIndex );
}

//-----------------------------------------------------------------------------
void CTFGCServerSystem::ClientDisconnected( CSteamID steamIDClient )
{
	if ( !steamIDClient.IsValid() || !steamIDClient.BIndividualAccount() )
	{
		Assert( steamIDClient.IsValid() );
		Assert( steamIDClient.BIndividualAccount() );
		return;
	}

	GetGCClient()->RemoveSOCacheListener( steamIDClient, this );

	// This is here because ClientDisconnected code is not called on gamerules or player
	// when the game is in state g_fGameOver.  See CServerGameClients::ClientDisconnect.
	CBasePlayer* pPlayer = UTIL_PlayerBySteamID( steamIDClient );
	if ( TFGameRules() && pPlayer )
	{
		TFGameRules()->SetPlayerNextMapVote( pPlayer->entindex(), CTFGameRules::USER_NEXT_MAP_VOTE_UNDECIDED );
	}

	CMatchInfo *pMatch = GetMatch();
	CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch ? pMatch->GetMatchDataForPlayer( steamIDClient ) : NULL;
	if ( !pMatchPlayer )
	{
		return;
	}

	if ( !pMatchPlayer->bConnected )
	{
		Assert( !"Player disconnecting is not marked connected" );
		return;
	}

	// Did they disconnect while still loading in?
	bool bWasActive = pMatchPlayer->nConnectingButNotActiveIndex == 0;

	RTime32 now = CRTime::RTime32TimeCur();
	// Time spent in the active state.
	RTime32 timeSpentActive = bWasActive ? ( now - pMatchPlayer->rtLastActiveEvent ) : 0;

	// Mark disconnected
	pMatchPlayer->bConnected = false;
	pMatchPlayer->nConnectingButNotActiveIndex = 0;

	// If they were active, they now transitioned to inactive. If they were loading, this value is still the last time
	// they went inactive, and shouldn't change.
	if ( bWasActive )
		{ pMatchPlayer->rtLastActiveEvent = now; }

	// Optionally forgive some amount of their disconnected seconds accumulation based on how long they were present.
	int nForgiveRatio = tf_mm_player_disconnect_time_forgive_ratio.GetInt();
	if ( timeSpentActive > 0 && nForgiveRatio > 0 && pMatchPlayer->nDisconnectedSeconds > 0 )
	{
		double dForgiven = (double)pMatchPlayer->nDisconnectedSeconds - ( (double)timeSpentActive / nForgiveRatio );

		int nOldVal = pMatchPlayer->nDisconnectedSeconds;
		pMatchPlayer->nDisconnectedSeconds = Max( 0, (int)dForgiven );

		MMLog("Client %s was connected for %u seconds, disconnect timer lowered from %i to %i\n",
		      steamIDClient.Render(), timeSpentActive, nOldVal, pMatchPlayer->nDisconnectedSeconds );
	}
}

//-----------------------------------------------------------------------------
void CTFGCServerSystem::PreClientUpdate( )
{
	BaseClass::PreClientUpdate();

	CRTime::UpdateRealTime();

	if ( GCClientSystem()->BConnectedtoGC() )
	{
		m_timeLastConnectedToGC = Plat_FloatTime();
	}

	// We want a pause so players can read what the next map is.  Once we've waited
	// long enough, we're doing a map change regardless of if the GC got back to us
	// with a new match ID.
	if ( Plat_FloatTime() > m_flWaitingForNewMatchTime
		&& m_flWaitingForNewMatchTime != 0.f )
	{
		LaunchNewMatchForLobby();
	}

	//
	// Check for updating the caches that we're listening to
	//
	CSteamID const *pSteamID = engine->GetGameServerSteamID();
	if ( pSteamID && m_ourSteamID != *pSteamID )
	{
		Assert( pSteamID->BGameServerAccount() );

		// If we were previously listening to somebody else, stop listening.  This
		// means we were connected, then reconnected and got a different Steam ID,
		// and is weird, but possible
		if ( m_ourSteamID.IsValid() )
		{
			MMLog( "CTFGCServerSystem - removing listener to old Steam ID %s\n", m_ourSteamID.Render() );
			GCClientSystem()->GetGCClient()->RemoveSOCacheListener( m_ourSteamID, this );
		}

		// Remember our new Steam ID
		m_ourSteamID = *pSteamID;

		// And start listening
		GCClientSystem()->GetGCClient()->AddSOCacheListener( m_ourSteamID, this );
	}

	MatchPlayerAbandonThink();
	UpdateConnectedPlayersAndServerInfo( CMsgGameServerMatchmakingStatus_Event_None, false );

	// Check if the game is empty, and we need to shut down our lobby

	CTFGSLobby *pLobby = GetLobby();
	if ( pLobby )
	{
		switch ( pLobby->GetState() )
		{
			case CSOTFGameServerLobby_State_SERVERSETUP:
				// We could most definitely be empty here, waiting for players to join!
				// Don't kill the server just yet
				break;

			case CSOTFGameServerLobby_State_RUN:
				break;

			default:
			case CSOTFGameServerLobby_State_UNKNOWN:
				MMLog( "Lobby in invalid state %d\n", (int)pLobby->GetState() );
				break;
		}
	}

	// Check for slamming visiblemaxplayers
	static ConVarRef sv_visiblemaxplayers( "sv_visiblemaxplayers" );
	if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() )
	{
		// Abort the server if they don't have enough maxplayers
		if ( gpGlobals->maxClients < 32 )
		{
			if( !g_bWarnedAboutMaxplayersInMVM )
			{
				// Prevent this warning from endlessly spamming the console...
				g_bWarnedAboutMaxplayersInMVM = true;
				Warning( "You must set maxplayers to 32 to host Mann vs. Machine\n" );
			}

			if ( engine->IsDedicatedServer() )
			{
				engine->ServerCommand( "exit\n" );
			}
			return;
		}

		// This changes what the server browser displays
		// update sv_visiblemaxplayers for MvM, count only non-bot spectators
		CUtlVector<CTFPlayer *> spectatorVector;
		CollectPlayers( &spectatorVector, TEAM_SPECTATOR );
		int spectatorCount = 0;
		FOR_EACH_VEC ( spectatorVector, iIndex )
		{
			if ( !spectatorVector[iIndex]->IsBot() && !spectatorVector[iIndex]->IsReplay() && !spectatorVector[iIndex]->IsHLTV() )
			{
				spectatorCount++;
			}
		}

		int playerCount = kMVM_DefendersTeamSize + spectatorCount;
		if ( sv_visiblemaxplayers.GetInt() <= 0 || sv_visiblemaxplayers.GetInt() != playerCount )
		{
			MMLog( "Setting sv_visiblemaxplayers to %d for MvM\n", playerCount );

			// save off visible players
			if ( !m_bOverridingVisibleMaxPlayers )
			{
				m_bOverridingVisibleMaxPlayers = true;
				m_iSavedVisibleMaxPlayers = sv_visiblemaxplayers.GetInt();
			}

			sv_visiblemaxplayers.SetValue( playerCount );
		}
	}
	else
	{
		// Not in MvM.  Check for restoring sv_visiblemaxplayers
		if ( m_bOverridingVisibleMaxPlayers )
		{
			MMLog( "Restoring sv_visiblemaxplayers to %d\n", m_iSavedVisibleMaxPlayers );
			sv_visiblemaxplayers.SetValue( m_iSavedVisibleMaxPlayers );
			m_bOverridingVisibleMaxPlayers = false;
			m_iSavedVisibleMaxPlayers = -1;
		}
	}

	// You may not be in matchmaking if you have a password!
	static ConVarRef sv_password( "sv_password" );
	if ( tf_mm_servermode.GetInt() != 0 && *sv_password.GetString() != '\0' )
	{
		Warning( "Setting tf_mm_servermode=0 due to sv_password\n" );
		tf_mm_servermode.SetValue( 0 );
	}

//	TFGameRules()->SetStableMode( IsStableMode() );
//
//	if ( HLTVDirector() && HLTVDirector()->GetHLTVServer() )
//	{
//		gcGameTime = Max( 0.0f, TFGameRules()->GetDOTATime() - HLTVDirector()->GetDelay() );
//	}
//	else
//	{
//		gcGameTime = TFGameRules()->GetDOTATime();
//	}

//	// Slam server region to 255 while in PVE mode
//	static ConVarRef sv_region( "sv_region" );
//	if ( sv_region.GetInt() != 255 )
//	{
//		MMLog( "Setting 'sv_region 255 ' due to tf_mm_servermode\n" );
//		sv_region.SetValue( 255 );
//	}
}

void CTFGCServerSystem::MatchPlayerAbandonThink()
{
	CMatchInfo *pMatchInfo = GetMatch();
	if ( !pMatchInfo || pMatchInfo->m_bMatchEnded )
		{ return; }

	int nAbandonSeconds = tf_mm_player_disconnect_time_before_abandon.GetInt();
	// Disabled
	if ( nAbandonSeconds < 0 )
		{ return; }

	int nPlayers = pMatchInfo->GetNumTotalMatchPlayers();
	bool bDroppedPlayers = false;
	for ( int idx = 0; idx < nPlayers; idx++ )
	{
		CMatchInfo::PlayerMatchData_t *pPlayer = pMatchInfo->GetMatchDataForPlayer( idx );

		// The engine doesn't really tell the game of connected-but-not-active players dropping.  Keep an eye on their
		// entity being quietly cleaned up and note the disconnect.
		if ( pPlayer->nConnectingButNotActiveIndex )
		{
			const CSteamID *pIndexSteamID = engine->GetClientSteamIDByPlayerIndex( pPlayer->nConnectingButNotActiveIndex );
			if ( !pIndexSteamID || *pIndexSteamID != pPlayer->steamID )
			{
				MMLog( "Match player %s dropped before going active\n", pPlayer->steamID.Render() );
				ClientDisconnected( pPlayer->steamID );
			}
		}

		if ( !pPlayer->bConnected && !pPlayer->bDropped )
		{
			// nDisconnectedSeconds is accumulated from previous absences, but doesn't include the current disconnect.
			int nTimeGone = CRTime::RTime32TimeCur() - pPlayer->rtLastActiveEvent + pPlayer->nDisconnectedSeconds;
			if ( nTimeGone > nAbandonSeconds )
			{
				MMLog( "Match player %s has been absent for a combined total of %u seconds, dropping from match\n",
				       pPlayer->steamID.Render(), nTimeGone );
				SetMatchPlayerDropped( pPlayer->steamID, pPlayer->bEverConnected ? TFMatchLeaveReason_AWOL : TFMatchLeaveReason_NO_SHOW );
				bDroppedPlayers = true;
			}
		}
	}
	if ( bDroppedPlayers )
		{ UpdateServerDetails(); }
}

//-----------------------------------------------------------------------------
bool CTFGCServerSystem::EjectMatchPlayer( CSteamID steamID, TFMatchLeaveReason eReason )
{
	CMatchInfo *pMatch = GetLiveMatch();
	CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch ? pMatch->GetMatchDataForPlayer( steamID ) : NULL;
	if ( !pMatchPlayer || pMatchPlayer->bDropped )
		{ return false; }

	SetMatchPlayerDropped( steamID, eReason );
	KickRemovedMatchPlayer( steamID );
	return true;
}

//-----------------------------------------------------------------------------
void CTFGCServerSystem::MatchPlayerVoteKicked( CSteamID steamID )
{
	bool bEjected = EjectMatchPlayer( steamID, TFMatchLeaveReason_VOTE_KICK );
	if ( bEjected )
	{
		// Was part of our match, handled.
		MMLog( "Player %s vote-kicked from live match\n", steamID.Render() );
		return;
	}

	// Not part of our match, check if they used to be
	CMatchInfo *pMatch = GetLiveMatch();
	if ( !pMatch )
		return;

	CMatchInfo::PlayerMatchData_t *pPlayer = pMatch->GetMatchDataForPlayer( steamID );
	if ( !pPlayer || ( pPlayer && !pPlayer->bDropped ) )
	{
		AssertMsg( !pPlayer || pPlayer->bDropped,
		           "Player is still part of our match, so EjectMatchPlayer should have succeeded" );
		return;
	}

	// Previously in this match, but left before kick arrived. Send this message made just for that occasion, update our
	// record to reflect the reason.
	MMLog( "Player %s vote-kicked after departing match\n", steamID.Render() );
	pPlayer->eDropReason = TFMatchLeaveReason_VOTE_KICK;
	ReliableMsgPlayerVoteKickedAfterLeavingMatch *pReliable = new ReliableMsgPlayerVoteKickedAfterLeavingMatch();
	auto &msg = pReliable->Msg().Body();

	msg.set_steam_id( steamID.ConvertToUint64() );
	msg.set_lobby_id( pMatch->m_nLobbyID );
	msg.set_match_id( pMatch->m_nMatchID );

	pReliable->Enqueue();
}

//-----------------------------------------------------------------------------
bool CTFGCServerSystem::KickRemovedMatchPlayer( CSteamID steamIDClient )
{
	CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerBySteamID( steamIDClient ) );
	if ( !pPlayer )
		{ return false; }

	MMLog( "Kicking ejected player %s\n", steamIDClient.Render() );
	engine->ServerCommand( UTIL_VarArgs( "kickid %d %s\n", pPlayer->GetUserID(), "#TF_MM_Generic_Kicked" ) );
	return true;
}

//-----------------------------------------------------------------------------
bool CTFGCServerSystem::CanChangeMatchPlayerTeams()
{
	// Warning: LaunchNewMatchForLobby is counting on being able to do this, so avoid the temptation to forbid this
	//          during match-result phase or similar (this is only for is-our-state-consistent-to-allow-this, not
	//          should-gamerules-be-doing-this, that's on them)
	CMatchInfo *pMatch = GetMatch();
	const IMatchGroupDescription* pMatchDesc = pMatch ? GetMatchGroupDescription( pMatch->m_eMatchGroup ) : NULL;

	if ( !pMatch || !pMatchDesc || pMatch->BMatchTerminated() || !pMatchDesc->BCanServerChangeMatchPlayerTeams() )
		{ return false; }

	// If we're waiting to launch a new match, the team change would be for the new match that the GC is about to send
	// down, which has new teams. We probably are not intending that since we have no idea what this player's current
	// team is.
	//
	// (See the Team Assignments comment at the start of this file for ordering regarding new matches and team changes.)
	if ( BPendingNewMatch() )
		{ return false; }

	return true;
}


//-----------------------------------------------------------------------------
// ChangeMatchPlayerTeams handling
//-----------------------------------------------------------------------------
void CTFGCServerSystem::ChangeMatchPlayerTeam( CSteamID steamID, TF_GC_TEAM eTeam )
{
	// Helper for single member.
	CUtlVectorFixed< PlayerTeamPair_t, 1 > vec;
	vec.AddToTail( { steamID, eTeam } );
	ChangeMatchPlayerTeams( vec );
}

template< typename ANY_ALLOCATOR >
void CTFGCServerSystem::ChangeMatchPlayerTeams( const CUtlVector< PlayerTeamPair_t, ANY_ALLOCATOR > &vecNewTeams )
{
	if ( !CanChangeMatchPlayerTeams() )
	{
		// Some match logic is badly out of sync if it thinks it can do this.
		MMLog( "!! Game server is attempting to change player teams in an invalidate state\n" );
		AbortInvalidMatchState();
		return;
	}

	// Job takes ownership of message
	MMLog( "Sending team assignment request to GC:\n" );

	ReliableMsgChangeMatchPlayerTeams *pReliable = new ReliableMsgChangeMatchPlayerTeams();

	auto &msg = pReliable->Msg().Body();
	msg.set_match_id( GetMatch()->m_nMatchID );
	msg.set_lobby_id( GetMatch()->m_nLobbyID );
	FOR_EACH_VEC( vecNewTeams, idx )
	{
		const CSteamID &steamID = vecNewTeams[idx].steamID;
		const TF_GC_TEAM &eTeam = vecNewTeams[idx].eTeam;

		// Do we know about this guy?
		CMatchInfo::PlayerMatchData_t *pPlayer = m_pMatchInfo->GetMatchDataForPlayer( steamID );
		if ( !pPlayer || pPlayer->bDropped )
		{
			MMLog("!! Got team change request for player not in match %s\n", steamID.Render() );
			continue;
		}

		MMLog("  %37s -> %d\n", steamID.Render(), eTeam );
		auto *member = msg.add_member();
		member->set_member_id( steamID.ConvertToUint64() );
		member->set_new_team( eTeam );

		// Reflect change locally immediately, this message should not fail
		pPlayer->eGCTeam = eTeam;
	}

	pReliable->Enqueue();
}

void CTFGCServerSystem::ChangeMatchPlayerTeamsResponse( bool bSuccess )
{
	if ( !bSuccess && GetLobby() )
	{
		// If the lobby went away prior to the GC responding, it is out of sync and can't do anything meaningful with
		// these updates right now, but we still have authority to finish the match and send a result, so just keep
		// plugging along.  But if we still HAVE the lobby, and the GC said no, something is badly out of sync with this
		// match.
		MMLog( "!! ChangeMatchPlayerTeams rejected, something is confused\n" );
		AbortInvalidMatchState();
		return;
	}
	MMLog( "ChangeMatchPlayerTeams acknowledged\n" );
}

//-----------------------------------------------------------------------------
const MapDef_t* CTFGCServerSystem::GetNextMapVoteByIndex( int nIndex ) const
{
	const CTFGSLobby *pLobby = GetLobby();
	if ( pLobby && nIndex < pLobby->Obj().next_maps_for_vote_size() )
	{
		return GetItemSchema()->GetMasterMapDefByIndex( pLobby->Obj().next_maps_for_vote( nIndex ) );
	}

	Assert( false );
	return GetItemSchema()->GetMasterMapDefByName( "ctf_2fort" );
}

//-----------------------------------------------------------------------------
// Purpose: GC Msg to request starting a new match for an existing lobby
//-----------------------------------------------------------------------------
void CTFGCServerSystem::NewMatchForLobbyResponse( bool bSuccess )
{
	// We should be expecting this
	if ( !m_bWaitingForNewMatchID )
	{
		MMLog( "!! Got a NewMatchForLobbyResponse when not expecting it\n" );
		AbortInvalidMatchState();
	}

	Assert( TFGameRules() );

	MMLog( "NewMatchID response recieved -- %s.\n", bSuccess ? "Success!" : "Failed!" );

	m_bWaitingForNewMatchID = false;

	CMatchInfo *pMatch = GetMatch();
	if ( pMatch && pMatch->m_bServerCreated )
	{
		// We went ahead without a match ID, the new ID should've already arrived in SOUpdated
		if ( bSuccess )
		{
			if ( !pMatch || pMatch->m_bServerCreated || !pMatch->m_nMatchID )
			{
				MMLog( "!! Got a NewMatchForLobby response but have not received a new match ID" );
				AbortInvalidMatchState();
			}
		}
		else
		{
			// Failed, but we already have a running speculative match. It is essentially an unofficial match now.
			MMLog( "!! NewMatchForLobby responded negatively, this match will likely not be acknowledged by the system.\n" );
			// TODO ROLLING MATCHES: Check that the jobs that will now send MatchID 0 do something salient
		}
	}
	else
	{
		// Still waiting to actually kick off the new match. If the response was a failure, we can just abort.
		if ( !bSuccess )
		{
			MMLog( "!! NewMatchForLobby responded negatively.  We haven't launched the match yet, so just shutting down.\n" );
			if ( TFGameRules() )
			{
				TFGameRules()->KickPlayersNewMatchIDRequestFailed();
			}
			else
			{
				AbortInvalidMatchState();
			}
		}
	}
}

bool CTFGCServerSystem::CanRequestNewMatchForLobby()
{
	// If this is a match that is not in sync with the GC, or it's not even a match, then no
	if ( !m_pMatchInfo || !GetLobby() || m_pMatchInfo->BMatchTerminated() )
		{ return false; }

	// If we're waiting on other pending match magic, then no you can't stack them god help your soul.
	if ( m_pMatchInfo->m_bServerCreated || m_bWaitingForNewMatchID || m_flWaitingForNewMatchTime != 0.f )
		{ return false; }

	// Match description allow it?
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_pMatchInfo->m_eMatchGroup );
	if ( !pMatchDesc->BCanServerRequestNewMatchForLobby() )
		{ return false; }

	return true;
}

void CTFGCServerSystem::RequestNewMatchForLobby( const MapDef_t* pNewMap )
{
	// Wat r u doin
	if ( !CanRequestNewMatchForLobby() )
	{
		AbortInvalidMatchState();
	}

	m_flWaitingForNewMatchTime = Plat_FloatTime() + tf_mm_next_map_result_hold_time.GetFloat();
	m_bWaitingForNewMatchID = true;
	m_pMatchInfo->m_strMapName = pNewMap->pszMapName;

	ReliableMsgNewMatchForLobby *pReliable = new ReliableMsgNewMatchForLobby();
	auto &msg = pReliable->Msg().Body();

	msg.set_next_map_id( pNewMap->m_nDefIndex );
	msg.set_lobby_id( GetLobby()->GetGroupID() );
	msg.set_current_match_id( GetMatch()->m_nMatchID );
	MMLog( "Sending request to GC for a new match ID.\n" );

	pReliable->Enqueue();
}

//-----------------------------------------------------------------------------
void CTFGCServerSystem::SetMatchPlayerDropped( CSteamID steamID, TFMatchLeaveReason eReason )
{
	CMatchInfo *pMatch = GetMatch();
	CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch ? pMatch->GetMatchDataForPlayer( steamID ) : NULL;
	Assert( pMatchPlayer );
	if ( !pMatchPlayer )
		{ return; }

	Assert( !pMatchPlayer->bDropped );

	// Determine if this was an abandon
	bool bAbandon = true;
	switch ( eReason )
	{
	case TFMatchLeaveReason_VOTE_KICK:
		// Vote kicks don't penalize you currently.  We need to revisit how these tie in with e.g. abuse reports/etc..
		bAbandon = false;
		break;
	case TFMatchLeaveReason_NO_SHOW:
	case TFMatchLeaveReason_GC_REMOVED:
		// For right now, until we have more confidence in our network connectivity and possibly have SDR hooked up,
		// we'll give no shows the benefit of the doubt if they never made it to connect. ( If they can't connect an
		// give up and click abandon on their end, it will show up as GC_REMOVED )
		bAbandon = pMatchPlayer->bEverConnected;
		break;
	case TFMatchLeaveReason_ADMIN_KICK:
	case TFMatchLeaveReason_AWOL:
	case TFMatchLeaveReason_IDLE:
		break;
	default: AssertMsg( false, "Unhandled TFMatchLeaveReason" );
	}

	bAbandon = bAbandon && !pMatch->BPlayerSafeToLeaveMatch( steamID );

	/// TODO ROLLING MATCHES: Technically if this happens with a rolling match in queue, we'll drop them from the old
	///                       match without record of them in the new?
	pMatch->DropPlayer( steamID, eReason, bAbandon );
	SendPlayerLeftMatch( steamID, eReason, bAbandon );
}

void CTFGCServerSystem::UpdateServerDetails(void)
{
	UpdateConnectedPlayersAndServerInfo( CMsgGameServerMatchmakingStatus_Event_None, false );
}

bool CTFGCServerSystem::ShouldHibernate()
{
	// We only hibernate if we're just sitting there with a freshly loaded map
	return engine->IsDedicatedServer() && tf_allow_server_hibernation.GetBool() && !GetLobby() && !BPendingReliableMessages() && !m_pMatchInfo;
}

void CTFGCServerSystem::FireGameEvent( IGameEvent *event )
{
	// Disconnected from gameserver
	if ( !Q_stricmp( event->GetName(), "player_disconnect" ) )
	{
		const char * pszReason = event->GetString( "reason", "" );
		if ( Q_strstr( pszReason, "kick" ) || Q_strstr( pszReason, "Kick" ) || Q_strstr( pszReason, g_pszVoteKickString ) )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid", 0 ) );
			if ( !pPlayer )
				return;

			CSteamID steamId;
			if ( !pPlayer->GetSteamID( &steamId ) )
				return;

			// Only care if this is a member of a live match
			CMatchInfo *pMatch = GetMatch();
			CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch ? pMatch->GetMatchDataForPlayer( steamId ) : NULL;
			if ( !pMatch || !pMatchPlayer || pMatch->m_bMatchEnded || pMatchPlayer->bDropped )
				{ return; }

			TFMatchLeaveReason eReason = TFMatchLeaveReason_ADMIN_KICK;

			if ( Q_strstr( pszReason, g_pszIdleKickString ) )
			{
				eReason = TFMatchLeaveReason_IDLE;
			}
			// kickid %d You have been voted off;
			// Vote kicks should not trigger abandon
			else if ( Q_strstr( pszReason, g_pszVoteKickString ) )
			{
				eReason = TFMatchLeaveReason_VOTE_KICK;
			}

			SetMatchPlayerDropped( steamId, eReason );
			UpdateServerDetails();
		}
	}
	else if ( FStrEq( event->GetName(), "player_score_changed" ) )
	{
		CMatchInfo *pMatch = GetMatch();
		if ( !pMatch )
			return;

		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( event->GetInt( "player" ) ) );
		if ( !pPlayer )
			return;

		CSteamID steamId;
		if ( !pPlayer->GetSteamID( &steamId ) )
			return;

		const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( pMatch->m_eMatchGroup );
		if ( !pMatchDesc || !pMatchDesc->m_pProgressionDesc )
			return;

		// Add to this player's score XP
		pMatch->GiveXPRewardToPlayerForAction( steamId, CMsgTFXPSource_XPSourceType_SOURCE_SCORE, event->GetInt( "delta", 0 ) );
	}
}

CTFParty* CTFGCServerSystem::GetPartyForPlayer( CSteamID steamID ) const
{
	// Dig up this guy's party
	CGCClientSharedObjectCache* pSOCache = const_cast< CTFGCServerSystem* >( this )->GetSOCache( steamID );
	if ( !pSOCache )
	{
		return NULL;
	}

	CSharedObjectTypeCache* pPartyTypeCache = pSOCache->FindTypeCache( CTFParty::k_nTypeID );
	if ( !pPartyTypeCache || pPartyTypeCache->GetCount() == 0 )
	{
		return NULL;
	}

	return assert_cast< CTFParty* >( pPartyTypeCache->GetObject( 0 ) );
}

const CMatchInfo::PlayerMatchData_t *CTFGCServerSystem::GetLiveMatchPlayer( CSteamID steamID ) const
{
	return const_cast<CTFGCServerSystem*>(this)->GetLiveMatchPlayer( steamID );
}

CMatchInfo::PlayerMatchData_t *CTFGCServerSystem::GetLiveMatchPlayer( CSteamID steamID )
{
	CMatchInfo *pMatch = GetMatch();
	if ( !pMatch || pMatch->m_bMatchEnded )
		{ return NULL; }

	CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch->GetMatchDataForPlayer( steamID );
	if ( !pMatchPlayer || pMatchPlayer->bDropped )
		{ return NULL; }

	return pMatchPlayer;
}

void CTFGCServerSystem::SOCreated( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
//	Msg( "CTFGCServerSystem::SOCreated type = %d owner = %s\n", pObject->GetTypeID(), steamIDOwner.Render() );

	// Lobby handling
	if ( pObject->GetTypeID() == CTFGSLobby::k_nTypeID )
	{
		const CTFGSLobby *pConstLobby = static_cast<const CTFGSLobby*>( pObject );
		CTFGSLobby *pLobby = const_cast<CTFGSLobby *>( pConstLobby ); // GROSS
		Assert( pLobby == GetLobby() ); // There can be only be one...

		MMLog( "Lobby %016llx instanced on this server in state %s\n",
		       pLobby->GetGroupID(), CSOTFGameServerLobby_State_Name( pLobby->GetState() ).c_str() );

		// Check if we need to switch the map or load a pop file.
		CMsgGameServerMatchmakingStatus_Event statusEvent = CMsgGameServerMatchmakingStatus_Event_None;
		bool bNewLobby = ( pLobby->GetState() == CSOTFGameServerLobby_State_SERVERSETUP );

		if ( m_bMMServerMode && bNewLobby )
		{
			MMLog( "  Map: '%s'\n", pLobby->GetMapName() );
			MMLog( "  Mission: '%s'\n", pLobby->GetMissionName() );

			EMatchGroup eMatchGroup = (EMatchGroup)pLobby->Obj().match_group();
	
			// Acknowledge the players that just connected.  (This will create
			// reservations for the players and let the GC we are expecting the
			// players.)
			statusEvent = CMsgGameServerMatchmakingStatus_Event_AcknowledgePlayers;
			
			// Create a record of the match on first connect.
			if ( m_pMatchInfo )
			{
				MMLog( "!! Received new anticipated lobby while running existing match. "
						"Old match ID [ %llu ] ended [ %u ] "
						"New matchID [ %llu ]\n",
						m_pMatchInfo->m_nMatchID, m_pMatchInfo->m_bMatchEnded,
						pLobby->GetMatchID() );
				Assert( false );
				
				delete m_pMatchInfo;

				// In theory the overwritten match will now be forgotten by us, all errant players kicked by the
				// UpdateConnectedPlayers tick...
			}

			m_pMatchInfo = new CMatchInfo( pLobby );
			GTFGCClientSystem()->DumpLobby();

			if ( eMatchGroup == EMatchGroup::k_nMatchGroup_Invalid ||
				 !GetMatchGroupDescription( eMatchGroup )->InitServerSettingsForMatch( pConstLobby ) )
			{
				AbortInvalidMatchState();
			}

	// FIXME We should have some version checking like this.
	//		int engineServerVersion = engine->GetServerVersion();
	//
	//		// Version checking is enforced if both sides do not report zero as their version
	//		if ( engineServerVersion && g_gcServerVersion && engineServerVersion != g_gcServerVersion )
	//		{
	//			// If we're out of date exit
	//			Msg("Version out of date (GC wants %d, we are %d), terminating!\n", g_gcServerVersion, engine->GetServerVersion() );
	//			engine->ServerCommand( "quit\n" );
	//		}
		}
		else
		{
			// We could've just gotten re-sent this lobby, is it the match we think we're running?  If we are running a
			// match for a different lobby, something is super wrong
			uint64 nExistingMatchID = m_pMatchInfo ? m_pMatchInfo->m_nMatchID : 0;
			uint64 nLobbyMatchID = pLobby->Obj().has_match_id() ? pLobby->GetMatchID() : 0;
			if ( m_pMatchInfo && nExistingMatchID == nLobbyMatchID )
			{
				MMLog( "GC refreshed lobby for match ID [ %llu ]\n", m_pMatchInfo->m_nMatchID );
			}
			else
			{
				MMLog( "!! Got assigned a lobby not in server-setup state, or when not accepting lobbies. Rejecting.\n"
				       "Lobby matchID [ %llu ], existing match [ %llu ]\n",
				       pLobby->GetMatchID(), m_pMatchInfo ? m_pMatchInfo->m_nMatchID : 0ull );

				if ( !m_pMatchInfo )
				{
					// Not running a match, don't want this one, just reject the lobby.
					//
					// This can happen when we crash and are handed a stale lobby upon reboot, rejecting it will
					// terminate that match.
					SendRejectLobby();
				}
				else
				{
					// Otherwise, we thought we had a lobby, but the GC sent us a different match? No idea what is going
					// on, probably some bad de-sync happened.
					//
					// No faith we can continue and send authoritative match results about anything.
					AbortInvalidMatchState();
				}
			}

		}

		UpdateConnectedPlayersAndServerInfo( statusEvent, false );
	}
}

void CTFGCServerSystem::SOUpdated( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
	// Don't care if we're not running a match
	CMatchInfo *pMatch = GetMatch();
	if ( !pMatch )
		return;

	// Lobby handling
	if ( pObject->GetTypeID() == CTFGSLobby::k_nTypeID )
	{
		const CTFGSLobby *pConstLobby = static_cast<const CTFGSLobby*>( pObject );
		CTFGSLobby *pLobby = const_cast<CTFGSLobby *>( pConstLobby ); // GROSS
		Assert( pLobby == GetLobby() ); // There can be only be one...

		bool bNeedsToUpdatePlayerAndServer = false;
		// Check if we have new reservations not part of the match
		for ( int i = 0; i < pLobby->GetNumMembers(); i++ )
		{
			const CTFLobbyMember *pMemberDetails = pLobby->GetMemberDetails( i );
			Assert( pMemberDetails );
			if ( !pMemberDetails )
				continue;

			CSteamID steamID( pMemberDetails->id() );
			CTFLobbyMember_ConnectState eLobbyState = pLobby->GetMemberConnectState( i );

			if ( eLobbyState == CTFLobbyMember_ConnectState_RESERVATION_PENDING )
			{
				CMatchInfo::PlayerMatchData_t *pPlayer = pMatch->GetMatchDataForPlayer( pLobby->GetMember( i ) );
				if ( !pPlayer || pPlayer->bDropped )
				{
					// Lobby has a new player we don't think is in our match, force an update to acknowledge them ASAP
					bNeedsToUpdatePlayerAndServer = true;
				}
			}
		}

		if ( bNeedsToUpdatePlayerAndServer )
		{
			UpdateConnectedPlayersAndServerInfo( CMsgGameServerMatchmakingStatus_Event_AcknowledgePlayers, true );
		}

		// If we terminated while the new match ID was pending we're still unwinding the incoming messages
		bool bNewMatchID = m_pMatchInfo && !m_pMatchInfo->BMatchTerminated() && ( m_pMatchInfo->m_nMatchID != pLobby->GetMatchID() );
		if ( bNewMatchID )
		{
			if ( m_bWaitingForNewMatchID && m_pMatchInfo->m_bServerCreated )
			{
				// We sent a request for a new matchID to put in for the match
				// we're running, and it just came back.
				MMLog( "Received new matchID for server-created match. "
						"New matchID [ %llu ]\n",
						pLobby->GetMatchID() );
				m_pMatchInfo->m_nMatchID = pLobby->GetMatchID();
				m_pMatchInfo->m_bServerCreated = false;
			}
			else if ( m_bWaitingForNewMatchID && m_flWaitingForNewMatchTime != 0.f )
			{
				// We're counting down to launching a new match, and the new match ID arrived. We'll pick it up from the
				// lobby in LaunchNewMatchForLobby
				MMLog( "Received new matchID while waiting for new matchID. "
						"Old match ID [ %llu ] ended [ %u ] "
						"New matchID [ %llu ]\n",
						m_pMatchInfo->m_nMatchID, m_pMatchInfo->m_bMatchEnded,
						pLobby->GetMatchID() );
			}
			else if ( !m_bWaitingForNewMatchID && m_flWaitingForNewMatchTime == 0.f )
			{
				// A lobby came in with a match ID that's not what our current
				// one is, and we were not expecting this.
				//
				// Note that we hold on to the stale lobby between NewMatchForLobby and LaunchNewMatchForLobby, so we
				// don't panic if the stale lobby updates.  The only other way out of that state is terminating the
				// match.
				MMLog( "Received new matchID when we weren't expecting one! "
						"Current matchID [ %llu ] "
						"New matchID [ %llu ]\n",
						m_pMatchInfo->m_nMatchID,
						pLobby->GetMatchID() );
				AbortInvalidMatchState();
			}
		}
	}
}

void CTFGCServerSystem::SODestroyed( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
	// Lobby handling
	if ( pObject->GetTypeID() == CTFGSLobby::k_nTypeID )
	{
		// Lobby is gone!  Reset
		UpdateConnectedPlayersAndServerInfo( CMsgGameServerMatchmakingStatus_Event_None, true );
	}
}

const CTFGSLobby *CTFGCServerSystem::GetLobby() const
{
	if ( !m_ourSteamID.IsValid() )
		return NULL;

	GCSDK::CGCClientSharedObjectCache *pSOCache = GCClientSystem()->GetSOCache( m_ourSteamID );
	if ( !pSOCache )
		return NULL;

	CSharedObjectTypeCache *pTypeCache = pSOCache->FindBaseTypeCache( CTFGSLobby::k_nTypeID );
	if ( pTypeCache && pTypeCache->GetCount() > 0 )
	{
		AssertMsg1( pTypeCache->GetCount() == 1, "Server has %d lobby objects in his cache!  He should only have 1.", pTypeCache->GetCount() );
		const CTFGSLobby *pLobby = static_cast<CTFGSLobby*>( pTypeCache->GetObject( pTypeCache->GetCount() - 1 ) );
		return pLobby;
	}

	return NULL;
}

CTFGSLobby *CTFGCServerSystem::GetLobby()
{
	// It's safe to un-constify the returned lobby if we're being called through a non-const reference ourselves.
	return const_cast< CTFGSLobby * >( ((const CTFGCServerSystem *)this)->GetLobby() );
}

void CTFGCServerSystem::DumpLobby()
{
	CTFGSLobby *pLobby = GetLobby();
	if ( !pLobby )
	{
		Msg( "Failed to find lobby shared object\n" );
		return;
	}

	pLobby->SpewDebug();
}

bool CTFGCServerSystem::HasLobby() const
{
	return GetLobby() != NULL;
}

void CTFGCServerSystem::SetHibernation( bool bHibernating )
{
	// !FIXME! Need to get rid of all the hibernation crap.  We don't really need it
}

bool CTFGCServerSystem::ShouldHideServer()
{
// !NO! Don't set this right now.  We'll just pass the "hidden" tag and so the server
// browser wil not list us.
//	if ( m_bMMServerMode && tf_mm_strict.GetBool() )
//		return true;
	return false;
}

bool CTFGCServerSystem::SteamIDAllowedToConnect(const CSteamID &steamID) const
{
	// If we're not in strict mode, anybody can join!
	if ( !m_bMMServerMode || tf_mm_strict.GetInt() != 1 )
		return true;

	// If we don't have a match, nobody can join
	const CMatchInfo *pMatchInfo = GetMatch();
	if ( !pMatchInfo )
	{
		return false;
	}

	const CMatchInfo::PlayerMatchData_t *pMatchData = pMatchInfo->GetMatchDataForPlayer( steamID );
	if ( !pMatchData || pMatchData->bDropped )
	{
		// Not in the match or was dropped, reject
		return false;
	}

	return true;
}

////-----------------------------------------------------------------------------
//int CTFGCServerSystem::GetTeamForLobbyMember( const CSteamID &steamId ) const
//{
//	const CTFGSLobby *pLobby = GetLobby();
//	if ( !pLobby )
//	{
//		return DOTA_TEAM_NOTEAM;
//	}
//
//	int team = pLobby->GetMemberTeam( steamId );
//
//	switch ( team )
//	{
//	case DOTA_GC_TEAM_GOOD_GUYS:
//		return DOTA_TEAM_GOODGUYS;
//
//	case DOTA_GC_TEAM_BAD_GUYS:
//		return DOTA_TEAM_BADGUYS;
//
//	case DOTA_GC_TEAM_BROADCASTER:
//	case DOTA_GC_TEAM_PLAYER_POOL:
//	case DOTA_GC_TEAM_SPECTATOR:
//		return TEAM_SPECTATOR;
//	}
//
//	return DOTA_TEAM_NOTEAM;
//}
//
////-----------------------------------------------------------------------------
//bool CTFGCServerSystem::IsLobbyMemberBroadcaster( const CSteamID &steamId ) const
//{
//	const CTFGSLobby *pLobby = GetLobby();
//	if ( !pLobby )
//	{
//		return false;
//	}
//
//	return pLobby->GetMemberTeam( steamId ) == DOTA_GC_TEAM_BROADCASTER;
//}
//
////-----------------------------------------------------------------------------
//ELanguage CTFGCServerSystem::GetBroadcasterLanguage( const CSteamID &steamId ) const
//{
//	const CTFGSLobby *pLobby = GetLobby();
//	if ( !pLobby )
//	{
//		return k_Lang_English;
//	}
//
//	if ( pLobby->GetMemberTeam( steamId ) != DOTA_GC_TEAM_BROADCASTER )
//		return k_Lang_English;
//
//	int index = pLobby->GetMemberIndexBySteamID( steamId );
//	if ( index < 0 )
//		return k_Lang_English;
//
//	const CTFLobbyMember* pMember = pLobby->GetMemberDetails( index );
//	switch( pMember->slot() )
//	{
//		default:
//		case 1:
//			return k_Lang_English;
//		case 2:
//			return k_Lang_German;
//		case 3:
//			return k_Lang_Simplified_Chinese;
//		case 4:
//			return k_Lang_Russian;
//	}
//
//	return k_Lang_English;
//}

//-----------------------------------------------------------------------------
CON_COMMAND( tf_server_lobby_debug, "Prints server lobby object" )
{
	GTFGCClientSystem()->DumpLobby();
}

ConVar dbg_spew_connected_players_level( "dbg_spew_connected_players_level", "0", FCVAR_NONE, "If enabled, server will spew connected player GC updates\n" );

// Inform the GC of any change in the connected players
void CTFGCServerSystem::UpdateConnectedPlayersAndServerInfo( CMsgGameServerMatchmakingStatus_Event event, bool bForceSendMessages )
{
	VPROF_BUDGET( "CTFGCServerSystem::UpdateConnectedPlayersAndServerInfo", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// Don't bother sending if we aren't initialized yet
	if ( gpGlobals->maxClients == 0 || TFGameRules() == NULL )
		return;

	/// TODO ROLLING MATCH: Remove event field from this message. We might just ignore some events, and they're not
	/// useful.

	// Don't send heartbeats while we're waiting for reliable messages to process, our state is not in sync with what we
	// tried to send to the GC, and sending a new heartbeat before pending messages have been responded to isn't
	// helpful.
	if ( BPendingReliableMessages() )
		{ return; }

	// Or if we're in the waiting period to kick off a new match -- if all pending messages came back, our lobby now
	// reflects the requested match, but we haven't actually launched it yet, so heartbeats would not be valid.
	if ( m_flWaitingForNewMatchTime != 0.f )
		{ return; }

	const CTFGSLobby *pLobby = GetLobby();
	if ( !pLobby || !m_bMMServerMode )
	{
		Assert( event == CMsgGameServerMatchmakingStatus_Event_None );
	}

	double now = Plat_FloatTime();

	if ( dbg_spew_connected_players_level.GetInt() >= 4 ) { Msg( "UpdateConnectedPlayers ======================================\n" ); }

	static ConVarRef sv_visiblemaxplayers( "sv_visiblemaxplayers" );

	CProtoBufMsg<CMsgGameServerMatchmakingStatus> msg( k_EMsgGCGameServerMatchmakingStatus );
	ServerMatchmakingState eGameServerInfoState = ServerMatchmakingState_NOT_PARTICIPATING;
	TF_MatchmakingMode eGameServerInfoMatchmakingMode = TF_Matchmaking_INVALID;
	CUtlString sGameServerInfoMap;
	CUtlString sGameServerInfoTags;
	int nBotCountToSend = -1;
	float flSendInterval = 60.0f;
	int nUnconnectedPlayerReservationRequests = 0;
	bool bLobbyIncorrect = false;
	CUtlVector<CSteamID> vecFailedLoaders;
	TF_GC_GameState gcState = TF_GC_GAMESTATE_DISCONNECT;

	CMatchInfo *pMatch = GetMatch();
	const IMatchGroupDescription* pMatchDesc = pMatch ? GetMatchGroupDescription( pMatch->m_eMatchGroup ) : NULL;

	// Build list of currently connected clients, and classify them according to their role
	struct Reservation_t
	{
		CSteamID m_steamID;
		int m_nEntindex;
		bool m_bActive;
	};
	CUtlVector< Reservation_t > vecReservationRequests;
	CUtlVector<CSteamID> vecConnectedPlayers;
	int nAdminSlots = 0;
	int nAdHocPlayers = 0;
	int nMatchPlayers = 0;
	int nBots = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		const CSteamID *pPlayerSteamID = engine->GetClientSteamIDByPlayerIndex( i );

		// Filter out non-players
		player_info_t sPlayerInfo;
		bool bActive = false;
		if ( engine->GetPlayerInfo( i, &sPlayerInfo ) )
		{
			if ( sPlayerInfo.ishltv || sPlayerInfo.isreplay )
			{
				++nAdminSlots;
				continue;
			}
			if ( sPlayerInfo.fakeplayer )
			{
				++nBots;
				continue;
			}

			if ( pPlayerSteamID == NULL || !pPlayerSteamID->IsValid() )
			{
				// This can occur in lan-mode
				Warning( "Player with no steam ID, counting as ad-hoc\n" );
			}

			bActive = true;
		}
		else
		{
			// Client not "active", but might be connected.
			// this happens during changelevel
			if ( pPlayerSteamID == NULL || !pPlayerSteamID->IsValid() )
			{
				continue;
			}

			// Connected, but not active.
			bActive = false;

			// Shove in a dummy name or debug spew
			V_strcpy_safe( sPlayerInfo.name, pPlayerSteamID->Render() );
		}

		// Some kind of player, add them to match players or ad-hoc
		CSteamID playerSteamID;
		if ( pPlayerSteamID && pPlayerSteamID->IsValid() )
			playerSteamID = *pPlayerSteamID;

		CMatchInfo::PlayerMatchData_t *pMatchPlayer = ( pMatch && playerSteamID.IsValid() ) \
		                                              ? pMatch->GetMatchDataForPlayer( playerSteamID ) \
		                                              : NULL;
		bool bMatchPlayer = pMatchPlayer && !pMatchPlayer->bDropped;
		if ( bMatchPlayer )
			{ ++nMatchPlayers; }
		else
			{ ++nAdHocPlayers; }

		if ( dbg_spew_connected_players_level.GetInt() >= 4 ) { Msg( " Client[%d]: %s '%s':\n", i, playerSteamID.Render(), sPlayerInfo.name ); }

		//
		// !! In lan mode, this player may not have a steamID. They can't be a lobby member or similar, so the below
		// !! code should just assume they're ad-hoc if !playerSteamID.IsValid()
		//
		if ( playerSteamID.IsValid() )
			vecConnectedPlayers.AddToTail( playerSteamID );

		// If we don't have a lobby, then we may still be running a match after a GC crash/reboot, in which case the
		// lobby might've been lost -- but we're still expected to complete the match on our own authority and report
		// the result.

		/// XXX(JohnS): Ideally, in the state where the GC rebooted and the lobby disintegrated, we'd have some way
		///             to tell the GC to recreate the lobby on its end when we re-establish, rather than finishing
		///             out a phantom match -- it doesn't know the user is still in a match until the match result
		///             arrives. However, as we locally track and report the match result and any abandons, the user
		///             can't really exploit this state other than potentially alt-F4ing and requeuing faster than
		///             their abandon timeout.  The GC, however, loses the ability to kick the player from this
		///             lobby. (that it no longer knows about)
		if ( pLobby )
		{

			// If he's in the lobby, them count him as a connected player.
			// Otherwise, he's an ad-hoc join.
			CMsgGameServerMatchmakingStatus_PlayerConnectState sendPlayerConnectState = CMsgGameServerMatchmakingStatus_PlayerConnectState_INVALID;
			const CTFLobbyMember *pMember = pLobby->GetMemberDetails( playerSteamID );
			if ( pMember )
			{
				CTFLobbyMember_ConnectState eLobbyState = pMember->connect_state();
				if ( dbg_spew_connected_players_level.GetInt() >= 4 )
				{
					Msg( "     '%s' In lobby with state %s\n", sPlayerInfo.name,
					     CTFLobbyMember_ConnectState_Name( eLobbyState ).c_str() );
				}
				switch ( eLobbyState )
				{
				case CTFLobbyMember_ConnectState_RESERVATION_PENDING:
					// Check if we have match data for this guy
					if ( !bMatchPlayer )
					{
						bLobbyIncorrect = true;
						vecReservationRequests.AddToTail( { *pPlayerSteamID, i, bActive } );
					}

					break;
				case CTFLobbyMember_ConnectState_RESERVED:

					// Only count them as actually "connected" if they are active.
					// We do not count them as "connected", to make sure we treat a
					// disconnection before they become "active" as a failure to load,
					// but a disconnection after they become active as a "leaver"
					if ( bActive )
					{
						sendPlayerConnectState = CMsgGameServerMatchmakingStatus_PlayerConnectState_CONNECTED;
						bLobbyIncorrect = true;
					}
					else
					{
						sendPlayerConnectState = CMsgGameServerMatchmakingStatus_PlayerConnectState_RESERVED;
						if ( eLobbyState != CTFLobbyMember_ConnectState_RESERVED )
							bLobbyIncorrect = true;
					}
					break;

				case CTFLobbyMember_ConnectState_CONNECTED:
					sendPlayerConnectState = CMsgGameServerMatchmakingStatus_PlayerConnectState_CONNECTED;

					break;
				case CTFLobbyMember_ConnectState_DISCONNECTED:
					sendPlayerConnectState = CMsgGameServerMatchmakingStatus_PlayerConnectState_CONNECTED;
					bLobbyIncorrect = true;
					break;
				default:
					AssertMsg1( false, "Unknown lobby member state %d", eLobbyState );
					break;
				}
			}
			else if ( m_pMatchInfo && !m_pMatchInfo->m_bMatchEnded )
			{
				// Competitive match, player missing from lobby
				if ( bMatchPlayer )
				{
					// Player was part of the match, but GC removed them.
					MMLog( "Removing match player %s -- dropped from lobby, but still in match and game\n",
					       playerSteamID.Render() );
					EjectMatchPlayer( playerSteamID, TFMatchLeaveReason_GC_REMOVED );
					nMatchPlayers--;
				}
				else if ( tf_mm_strict.GetInt() == 1 )
				{
					// A player is present that shouldn't be
					MMLog( "!! Unknown player in managed match %s\n", playerSteamID.Render() );
					KickRemovedMatchPlayer( playerSteamID );
					nAdHocPlayers--;
				}
			}
			else
			{
				// Not a managed match
				if ( dbg_spew_connected_players_level.GetInt() >= 4 )
				{
					Msg( "     '%s' Not in lobby, client is ad-hoc join\n", sPlayerInfo.name );
				}
			}

			if ( sendPlayerConnectState != CMsgGameServerMatchmakingStatus_PlayerConnectState_INVALID )
			{
				CMsgGameServerMatchmakingStatus_Player *pMsgPlayer = msg.Body().add_players();
				pMsgPlayer->set_steam_id( playerSteamID.ConvertToUint64() );
				pMsgPlayer->set_connect_state( sendPlayerConnectState );
			}
		}
	}	// end For each client

	//
	// Now, check match for players that we are tracking but are not connected, and count them in the total and the
	// status message
	//
	if ( pMatch && !pMatch->BMatchTerminated() )
	{
		int nTotalMatch = pMatch->GetNumTotalMatchPlayers();
		for ( int idx = 0; idx < nTotalMatch; idx++ )
		{
			CMatchInfo::PlayerMatchData_t *pPlayer = pMatch->GetMatchDataForPlayer( idx );
			// Don't care if they are now dropped or were handled in the connected players loop above
			if ( pPlayer->bDropped || vecConnectedPlayers.Find( pPlayer->steamID ) != vecConnectedPlayers.InvalidIndex() )
				{ continue; }

			if ( pPlayer->bConnected )
			{
				MMLog( "!! Match player %s not present but marked connected\n", pPlayer->steamID.Render() );
			}

			// Note that if the GC lost our lobby (which should only occur due to system failure on the other end), we
			// just keep dutifully sending status updates for the players we have as long as we have a match
			if ( pLobby && !pLobby->GetMemberDetails( pPlayer->steamID ) )
			{
				// Player was part of the match, but GC removed them.
				MMLog( "Removing player %s, not present in match and dropped from lobby\n",
				       pPlayer->steamID.Render() );
				SetMatchPlayerDropped( pPlayer->steamID, TFMatchLeaveReason_GC_REMOVED );
			}
			else
			{
				// We are holding a valid reservation.  Add this fact to the message, to confirm
				// that we are aware of the player.
				nMatchPlayers++;
				CMsgGameServerMatchmakingStatus_Player *pMsgPlayer = msg.Body().add_players();
				pMsgPlayer->set_steam_id( pPlayer->steamID.ConvertToUint64() );
				if ( dbg_spew_connected_players_level.GetInt() >= 4 )
					{ Msg( " Player[%d]: %s reserved\n", msg.Body().players_size(), pPlayer->steamID.Render() ); }
				pMsgPlayer->set_connect_state( CMsgGameServerMatchmakingStatus_PlayerConnectState_RESERVED );
				bLobbyIncorrect = true;
			}
		}
	}

	//
	// Scan lobby, and check for lobby player entries that don't match our local state.
	//
	if ( pLobby )
	{
		if ( dbg_spew_connected_players_level.GetInt() >= 4 )
			{ Msg( "Checking all connected players are marked connected in lobby:\n" ); }

		for ( int i = 0; i < pLobby->GetNumMembers(); i++ )
		{
			const CTFLobbyMember *pMemberDetails = pLobby->GetMemberDetails( i );
			Assert( pMemberDetails );
			if ( !pMemberDetails )
				continue;
			CSteamID steamID( pMemberDetails->id() );

			CTFLobbyMember_ConnectState eLobbyState = pLobby->GetMemberConnectState( i );
			if ( dbg_spew_connected_players_level.GetInt() >= 4 )
				{ Msg( " Lobby member %s is in state %s\n", steamID.Render(), CTFLobbyMember_ConnectState_Name( eLobbyState ).c_str() ); }

			int iConnectedPlayer = vecConnectedPlayers.Find( steamID );
			if ( iConnectedPlayer >= 0 )
				{ continue; } // we handled them earlier

			// Player is not currently connected.  Check against what the lobby thinks
			switch ( eLobbyState )
			{
				case CTFLobbyMember_ConnectState_RESERVATION_PENDING:
				{
					// Check if we already have a reservation for this guy
					CMatchInfo::PlayerMatchData_t *pMatchPlayer = GetMatch() ? GetMatch()->GetMatchDataForPlayer( steamID ) : NULL;
					if ( GetMatch() && ( !pMatchPlayer || pMatchPlayer->bDropped ) )
					{
						bLobbyIncorrect = true;
						vecReservationRequests.AddToTail( { steamID, 0, false } );
						++nUnconnectedPlayerReservationRequests;
					}
					else
					{
						if ( dbg_spew_connected_players_level.GetInt() >= 4 )
							{ Msg( " Player[%d]: %s requested reservation.  We already had one.\n", msg.Body().players_size(), steamID.Render() ); }
					}
				} break;

				case CTFLobbyMember_ConnectState_RESERVED:
					// We'll handle it below when we process our reservations
					break;

				case CTFLobbyMember_ConnectState_CONNECTED:
					if ( dbg_spew_connected_players_level.GetInt() >= 4 )
						{ Msg( " Lobby member %s no longer connected, lobby is incorrect\n", steamID.Render() ); }
					bLobbyIncorrect = true;
					break;
				case CTFLobbyMember_ConnectState_DISCONNECTED:
					break;
				default:
					AssertMsg1( false, "Unknown lobby member state %d", eLobbyState );
					break;
			}
		}
	}

	// Now we've scanned connected players, our match, and the lobby object. Count up the total taken slots, and how
	// many slots the match could have (0 if no match) These are slots that are spoken for, not necessarily currently
	// connected
	// NOTE: These might be updated by accepting reservations or dropping players in the next section

	bool bLiveMatch = pMatch && pMatchDesc && !pMatch->m_bMatchEnded;
	// TODO ROLLING MATCHES: Need a check for no-latejoins state for after we've sent a match result?
	int nMaxMatchPlayers = bLiveMatch ? pMatch->GetCanonicalMatchSize() : 0;
	int nMaxHumans = gpGlobals->maxClients - nAdminSlots;
	{
		// Maybe cap visible max humans.  Honor the override MvM mode might apply, but if we are accepting arbitrary new
		// matches expose the real value we would allow a new potentially non-mvm match to use.
		int nLimitVisibleSlots = sv_visiblemaxplayers.GetInt();
		if ( m_bOverridingVisibleMaxPlayers && !bLiveMatch && m_bMMServerMode )
			{ nLimitVisibleSlots = m_iSavedVisibleMaxPlayers; }
		// Don't limit visible slots to below the current match
		if ( nMaxMatchPlayers > 0 )
			{ nLimitVisibleSlots = Max( nMaxMatchPlayers, nLimitVisibleSlots ); }
		if ( nLimitVisibleSlots > 0 )
			{ nMaxHumans = Min( nMaxHumans, nLimitVisibleSlots ); }
	}

	int nHumans = nAdHocPlayers + nMatchPlayers;
	int nClients = nHumans + nBots + nAdminSlots;
	// Maximum nHumans should be allowed to be. Max clients - AdminSlots, capped to visiblemaxplayers

	// If we've never added a player to our match this is the first think
	bool bNewMatch = bLiveMatch && pMatch->GetNumTotalMatchPlayers() == 0;
	// If our current state allows us to accept new match players
	bool bRequestMatchLateJoin = bLiveMatch && \
	                             nHumans < nMaxMatchPlayers && \
	                             nClients < gpGlobals->maxClients && \
	                             pMatchDesc->ShouldRequestLateJoin();

	//
	// Check if the GC is requesting us to make some more reservations, and accepting them would not exceed
	// desired match size or engine capabilities.
	//
	if ( pLobby && vecReservationRequests.Count() &&
	     ( bNewMatch || bRequestMatchLateJoin ) &&
	     nUnconnectedPlayerReservationRequests + nHumans <= nMaxMatchPlayers &&
	     nUnconnectedPlayerReservationRequests + nClients <= gpGlobals->maxClients )
	{
		MMLog( "GC is requesting us to reserve %d slots.\n", vecReservationRequests.Count() );

		// Accept one at a time and check if we can handle more
		FOR_EACH_VEC( vecReservationRequests, idx )
		{
			const CTFLobbyMember *pMember = pLobby->GetMemberDetails( vecReservationRequests[ idx ].m_steamID );
			AcceptGCReservation( vecReservationRequests[ idx ].m_steamID, pMember, !bNewMatch,
			                     vecReservationRequests[ idx ].m_nEntindex, vecReservationRequests[ idx ].m_bActive );

			// Add them to our message for this pass
			CMsgGameServerMatchmakingStatus_Player *pMsgPlayer = msg.Body().add_players();
			pMsgPlayer->set_steam_id( vecReservationRequests[ idx ].m_steamID.ConvertToUint64() );
			pMsgPlayer->set_connect_state( CMsgGameServerMatchmakingStatus_PlayerConnectState_RESERVED );
		}

		// We promised more people slots, recompute this
		nMatchPlayers += nUnconnectedPlayerReservationRequests;
		nHumans += nUnconnectedPlayerReservationRequests;
		nClients += nUnconnectedPlayerReservationRequests;
		bRequestMatchLateJoin = bRequestMatchLateJoin && \
		                        nHumans < nMaxMatchPlayers && \
		                        nClients < gpGlobals->maxClients && \
		                        pMatchDesc && pMatchDesc->ShouldRequestLateJoin();
	}
	else if ( nUnconnectedPlayerReservationRequests )
	{
		MMLog( "Refused %d reservations -- not accepting match players or exceeds capacity\n",
		       vecReservationRequests.Count() );
	}

	// Check if they think that they are acknowledging some players, make sure
	// we would have decided to send a message anyway, even without their event
	if ( event == CMsgGameServerMatchmakingStatus_Event_AcknowledgePlayers )
	{
		Assert( bLobbyIncorrect == true );
	}

	//
	// Clean up complete match if all players have left and the GC has dissolved the lobby.
	//
	// Deleting this should clear us up to accept new matches below,
	// where our ready-for-match state depends on !pLobby && !pMatch.
	//
	// Don't clean up if GC hasn't acknowledged dissolution of lobby yet, or we'll have a lobby with no associated
	// match to indicate what state it was in.  If the GC is MIA to clean-up lobbies that's okay, we can't start a
	// new match until it's ready anyway, and the empty-with-lobby below check will kill us if we get stuck in this
	// state.
	if ( vecConnectedPlayers.Count() == 0 &&
	     m_pMatchInfo && !pLobby && m_pMatchInfo->m_bMatchEnded )
	{
		MMLog( "Cleaning out finished match %llu\n", m_pMatchInfo->m_nMatchID );
		delete m_pMatchInfo;
		m_pMatchInfo = NULL;
		bLiveMatch = false;
		pMatch = NULL;
		pMatchDesc = NULL;
	}

	// Check if we're empty with a lobby.  Ordinarily, we shouldn't linger too long in this state.  Either we're in
	// the process of timing out everyone as abandoners (which should take a lot less than this timeout) or the GC
	// is down.  But if that state persists for two hours, assume we're in a bad stuck state and reboot.
	if ( pLobby && vecConnectedPlayers.Count() == 0 )
	{
		if ( m_flTimeBecameEmptyWithLobby == 0.0 )
		{
			m_flTimeBecameEmptyWithLobby = now;
		}
		else
		{
			int nSecondsEmptyWithLobby = int( now - m_flTimeBecameEmptyWithLobby );
			int nTimeoutMinutes = ( BPendingReliableMessages() || m_pMatchInfo ) ? k_InvalidState_Timeout_With_Match \
				: k_InvalidState_Timeout_Without_Match;
			if ( nSecondsEmptyWithLobby > nTimeoutMinutes*60 )
			{
				MMLog( "**** Server has been empty with a lobby for %d seconds.  Quitting\n", nSecondsEmptyWithLobby );
				AbortInvalidMatchState();
			}
		}
	}
	else
	{
		m_flTimeBecameEmptyWithLobby = 0.0;
	}


	// Determine game state
	gcState = TF_GC_GAMESTATE_GAME_IN_PROGRESS;
	switch ( TFGameRules()->State_Get() )
	{
		case GR_STATE_INIT:
			gcState = TF_GC_GAMESTATE_STATE_INIT;
			break;

		case GR_STATE_PREGAME:
		case GR_STATE_STARTGAME:
		case GR_STATE_PREROUND:
		case GR_STATE_RESTART:
			gcState = TF_GC_GAMESTATE_STRATEGY_TIME;
			break;

		default:
			Assert( false );
		case GR_STATE_RND_RUNNING:
		case GR_STATE_BETWEEN_RNDS:
		case GR_STATE_BONUS:
			break;

		case GR_STATE_TEAM_WIN:
		case GR_STATE_STALEMATE:
			if ( TFGameRules()->IsMannVsMachineMode() )
			{
				// *Currently* can only end in victory (or dissolves because everyone leaves)
				if (
					TFGameRules()->State_Get() == GR_STATE_TEAM_WIN
					&& TFGameRules()->GetWinningTeam() == TF_TEAM_PVE_DEFENDERS )
				{
					gcState = TF_GC_GAMESTATE_POST_GAME;
				}
			}
			else if ( TFGameRules()->IsCompetitiveMode() )
			{
				if ( TFGameRules()->State_Get() == GR_STATE_GAME_OVER )
				{
					gcState = TF_GC_GAMESTATE_POST_GAME;
				}
			}
			break;

		case GR_STATE_GAME_OVER:
			gcState = TF_GC_GAMESTATE_GAME_IN_PROGRESS;
			if ( TFGameRules()->IsMannVsMachineMode() ||
			     TFGameRules()->IsCompetitiveMode() ) // right?
			{
				gcState = TF_GC_GAMESTATE_DISCONNECT;
			}
			break;
	}

	// What state are we?
	if ( m_bMMServerMode )
	{
		static ConVarRef sv_tags( "sv_tags" );
		eGameServerInfoMatchmakingMode = TF_Matchmaking_LADDER;
		nBotCountToSend = -1;
		sGameServerInfoMap = STRING( gpGlobals->mapname );
		sGameServerInfoTags = sv_tags.GetString();
		sGameServerInfoTags.Clear();

		// Set the "map" to the current challenge, if in MvM
		if ( TFGameRules()->IsMannVsMachineMode() )
		{
			const char *pszFilenameShort = g_pPopulationManager ? g_pPopulationManager->GetPopulationFilenameShort() : NULL;
			if ( pszFilenameShort && pszFilenameShort[0] )
			{
				sGameServerInfoMap = pszFilenameShort;
			}
		}

		// Determine state
		if ( !m_pMatchInfo && !pLobby )
		{
			// No match, lobby, or players, ready for match
			if ( BPendingReliableMessages() )
			{
				eGameServerInfoState = ServerMatchmakingState_NOT_PARTICIPATING;
				if ( m_eLastGameServerUpdateState != eGameServerInfoState )
					{ MMLog( "No match, but have not finished sending reliable messages, not re-enrolling in MM yet\n" ); }
			}
			else
			{
				eGameServerInfoState = ServerMatchmakingState_EMPTY;
				if ( m_eLastGameServerUpdateState != eGameServerInfoState )
					{ MMLog( "No match, but configured for MM, enrolling in matchmaking\n" ); }
			}

			// Unless we're not setup with no actual usable slots or have random unknown humans in strict mode
			if ( nClients >= gpGlobals->maxClients || nMaxHumans < 1 ||
			     ( nHumans && tf_mm_strict.GetInt() == 1 ) )
			{
				eGameServerInfoState = ServerMatchmakingState_NOT_PARTICIPATING;
				if ( m_eLastGameServerUpdateState != eGameServerInfoState )
				{
					MMLog( "!! No match, but no usable slots or unexpected clients, cannot enroll in matchmaking. "
					       "[ nClients %d, maxClients %d, nHumans %d, nMaxHumans %d ]\n",
					       nClients, gpGlobals->maxClients, nHumans, nMaxHumans );
				}
			}
		}
		else if ( bLiveMatch )
		{
			// Have a running match.
			eGameServerInfoState = bRequestMatchLateJoin ? ServerMatchmakingState_ACTIVE_MATCH_REQUESTING_LATE_JOIN \
			                                             : ServerMatchmakingState_ACTIVE_MATCH;
		}
		else
		{
			// We have a match but it isn't live, or we have no match but the GC hasn't torn down the lobby yet ( we
			// should have either rejected the lobby in SOCreated or sent a cleanup message when ending the match, but
			// our GC connection may be lagged, just stay out of the pool until we reconcile )
			if ( m_eLastGameServerUpdateState != eGameServerInfoState )
				{ MMLog( "Match state is not in sync with GC, remaining out of MM until lobby is cleaned up\n" ); }
			eGameServerInfoState = ServerMatchmakingState_NOT_PARTICIPATING;
		}
	}

// This is probably not worth the risk / reward right now.  We've given instructions
// telling server operators how to avoid this from happening, and it might break something
//		// Check if we have a lobby, and they have switched to/from MvM mode, then don't
//		// put us in matchmaking for now
//		bool bMapIsMvmMap = ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() );
//		if ( ( pLobby != NULL ) && ( bMapIsMvmMap != bIsMvmMode ) )
//		{
//			eGameServerInfoMatchmakingMode = TF_Matchmaking_INVALID;
//			eGameServerInfoState = ServerMatchmakingState_NOT_PARTICIPATING;
//			MMLog( "Sending NOT_PARTICIPATING.  Is MvM Map: %d, tf_mm_servermode=%d\n", bMapIsMvmMap ? 1 : 0, tf_mm_servermode.GetInt() );
//		}

	int nSlotsFree = nMaxHumans - nHumans;

	// Check if number of slots available is changing.  Our urgency to notify the GC about this
	// change depends on which direction it is changing!
	if ( nSlotsFree < m_nLastGameServerUpdateSlotsFree )
	{
		// We currently have fewer slots available than the GC thinks we do.
		// This is an important state change and we need to let the GC know about
		// this immediately, otherwise it might ask us to fill reservations we cannot
		// satisfy.  We want the window for this race condition to be as small as
		// possible.
		bForceSendMessages = true;
	}
	else if ( nSlotsFree > m_nLastGameServerUpdateSlotsFree )
	{
		// We have more slots open than the GC thinks we do.  We should let the GC
		// know relatively soon, but it's really not urgent that we flush this out
		// *immediately*.  Also, because players come and go frequently (especially
		// in PvP), having this timer avoids massive spam if tons of players all decide
		// to leave at once.
		flSendInterval = Min( flSendInterval, 10.0f );
	}

	// Check if we MUST send a message, no matter how recently we sent the last update.
	if ( event == CMsgGameServerMatchmakingStatus_Event_None &&
	     !bForceSendMessages &&
	     ( eGameServerInfoState == m_eLastGameServerUpdateState ) &&
	     ( eGameServerInfoMatchmakingMode == m_eLastGameServerUpdateMatchmakingMode ) &&
	     // map changes are infrequent, and matter quite a bit, so always send them
	     Q_stricmp( m_sLastGameServerUpdateMap, sGameServerInfoMap ) == 0 )
	{

		// No need to send periodic updates if we're not participating and don't think we have a lobby or match at all.
		if ( eGameServerInfoState == ServerMatchmakingState_NOT_PARTICIPATING && !pLobby && !m_pMatchInfo )
			return;

		// Check for certain rules changes.  When they change, we care about them being
		// eventually correct, but it's not urgent
		if ( ( Q_stricmp( m_sLastGameServerUpdateTags, sGameServerInfoTags ) != 0 ) ||
		     ( nMaxHumans != m_nLastGameServerUpdateMaxHumans ) ||
		     ( nBotCountToSend != m_nLastGameServerUpdateBotCount ) )
		{
			flSendInterval = Min( flSendInterval, 20.0f );
		}

		// If lobby is incorrect in an ordinary way (player left, etc),
		// flush the change decently quickly
		if ( pLobby && bLobbyIncorrect )
		{
			// Send updates more quickly if the GC hasn't acknowledged, but don't DDoS.  Ideally the event that made the
			// lobby incorrect triggered a Update( bForce = true );
			flSendInterval = Min( flSendInterval, 10.0f );
		}

		if ( now < m_timeLastSendGameServerInfoAndConnectedPlayers + flSendInterval )
			{ return; }
	}

	// Fill in info about our connection state
	msg.Body().set_server_version( engine->GetServerVersion() );
	msg.Body().set_matchmaking_state( eGameServerInfoState );
	if ( eGameServerInfoState == ServerMatchmakingState_NOT_PARTICIPATING )
	{
		msg.Body().set_match_group( k_nMatchGroup_Invalid );
		if ( dbg_spew_connected_players_level.GetInt() >= 2 )
		{
			MMLog("Sending CMsgGameServerMatchmakingStatus (state=%s)\n",
			      ServerMatchmakingState_Name( msg.Body().matchmaking_state() ).c_str() );
		}
	}
	else
	{
		static ConVarRef sv_region( "sv_region" );
		msg.Body().set_server_region( sv_region.GetInt() );
		msg.Body().set_server_loadavg( GetCPUUsage() );
		msg.Body().set_server_dedicated( engine->IsDedicatedServer() );
		msg.Body().set_server_trusted( tf_mm_trusted.GetBool() );
		msg.Body().set_matchmaking_mode( eGameServerInfoMatchmakingMode );
		msg.Body().set_map( sGameServerInfoMap );
		msg.Body().set_game_state( gcState );
		if ( pLobby )
			msg.Body().set_lobby_mm_version( pLobby->GetLobbyMMVersion() );
		if ( nBotCountToSend >= 0 )
			msg.Body().set_bot_count( (uint32)nBotCountToSend );
		Assert( nMaxHumans > 0 );
		msg.Body().set_max_players( nMaxHumans );
		Assert( nSlotsFree >= 0 );
		msg.Body().set_slots_free( nSlotsFree );
		msg.Body().set_tags( sGameServerInfoTags );
		msg.Body().set_strict( tf_mm_strict.GetInt() );

		if ( event != CMsgGameServerMatchmakingStatus_Event_None )
			{ msg.Body().set_event( event ); }

		if ( ( dbg_spew_connected_players_level.GetInt() >= 2 ) ||
		     ( event != CMsgGameServerMatchmakingStatus_Event_None && dbg_spew_connected_players_level.GetInt() >= 1 ) )
		{
			MMLog("Sending CMsgGameServerMatchmakingStatus (state=%s, slots_free=%d, event=%s, %s)\n",
			      ServerMatchmakingState_Name( msg.Body().matchmaking_state() ).c_str(),
			      msg.Body().slots_free(),
			      CMsgGameServerMatchmakingStatus_Event_Name( msg.Body().event() ).c_str(),
			      ( tf_mm_trusted.GetBool() ? ", trusted=true" : "" )
			);
		}

		if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() )
		{
			msg.Body().set_mvm_credits_acquired( MannVsMachineStats_GetAcquiredCredits( -1 ) );
			msg.Body().set_mvm_credits_dropped( MannVsMachineStats_GetAcquiredCredits( -1 ) );
			msg.Body().set_mvm_wave( MannVsMachineStats_GetCurrentWave() );
		}

		EMatchGroup eCurrentGroup = k_nMatchGroup_Invalid;
		if ( m_pMatchInfo )
		{
			eCurrentGroup = m_pMatchInfo->m_eMatchGroup;
		}

		msg.Body().set_match_group( eCurrentGroup );
	}

	// Check if we MUST send a message, no matter how recently we sent the last update.
	if ( event == CMsgGameServerMatchmakingStatus_Event_None &&
	     !bForceSendMessages &&
	     ( msg.Body().lobby_mm_version() == m_nLastGameServerUpdateLobbyMMVersion ) &&
	     ( msg.Body().matchmaking_state() == m_eLastGameServerUpdateState ) &&
	     ( msg.Body().matchmaking_mode() == m_eLastGameServerUpdateMatchmakingMode ) &&
	     // map changes are infrequent, and matter quite a bit, so always send them
	     Q_stricmp( m_sLastGameServerUpdateMap, msg.Body().map().c_str() ) == 0 )
	{

		// No need to send periodic updates if we're not participating and don't think we have a lobby or match at all.
		if ( msg.Body().matchmaking_state() == ServerMatchmakingState_NOT_PARTICIPATING && !pLobby && !m_pMatchInfo )
			return;

		// Check for certain rules changes.  When they change, we care about them being
		// eventually correct, but it's not urgent
		if ( ( Q_stricmp( m_sLastGameServerUpdateTags, msg.Body().tags().c_str() ) != 0 ) ||
		     ( msg.Body().max_players() != (uint32)m_nLastGameServerUpdateMaxHumans ) ||
		     ( msg.Body().bot_count() != (uint32)m_nLastGameServerUpdateBotCount ) )
		{
			flSendInterval = Min( flSendInterval, 20.0f );
		}

		// If lobby is incorrect in an ordinary way (player left, etc),
		// flush the change decently quickly
		if ( pLobby && bLobbyIncorrect )
		{
			// Send updates more quickly if the GC hasn't acknowledged, but don't DDoS.  Ideally the event that made the
			// lobby incorrect triggered a Update( bForce = true );
			flSendInterval = Min( flSendInterval, 10.0f );
		}

		if ( now < m_timeLastSendGameServerInfoAndConnectedPlayers + flSendInterval )
			{ return; }
	}

	GCClientSystem()->BSendMessage( msg );

	// Remember what/when we sent, so we can tell next time if we need to send
	m_timeLastSendGameServerInfoAndConnectedPlayers = now;
	m_eLastGameServerUpdateMatchmakingMode = msg.Body().matchmaking_mode();
	m_eLastGameServerUpdateState = msg.Body().matchmaking_state();
	m_sLastGameServerUpdateMap = msg.Body().map().c_str();
	m_sLastGameServerUpdateTags = msg.Body().tags().c_str();
	m_nLastGameServerUpdateBotCount = nBotCountToSend;
	m_nLastGameServerUpdateMaxHumans = nMaxHumans;
	m_nLastGameServerUpdateSlotsFree = nSlotsFree;
	m_nLastGameServerUpdateLobbyMMVersion = msg.Body().lobby_mm_version();

	// Remember when we started requesting late join, so we can compare it to our lobby's late-join state to reason
	// about how long we've been waiting.
	if ( eGameServerInfoState == ServerMatchmakingState_ACTIVE_MATCH_REQUESTING_LATE_JOIN )
	{
		if ( m_flTimeRequestedLateJoin == -1.f )
		{
			m_flTimeRequestedLateJoin = CRTime::RTime32TimeCur();
			MMLog( "Requested late join for active match\n" );
		}
	}
	else if ( m_flTimeRequestedLateJoin != -1.f )
	{
		MMLog( "Stopped requesting late join for active match after %.02fs\n",
		       CRTime::RTime32TimeCur() - m_flTimeRequestedLateJoin );
		m_flTimeRequestedLateJoin = -1.f;
	}

	// Only late join eligible when are requesting late join, we have a lobby from the GC, and it has marked itself as
	// late join eligible.  If we've lost our lobby or it hasn't updated to become eligible, there may be GC connection
	// difficulties.

	// We only update this at the end of updates, rather than on the fly, to ensure we don't expose this value prior to
	// processing other updates in the lobby object.  For instance, the lobby might remove us from late join and give us
	// reserved members at the same time, we don't want callers to see one, but not the other.
	m_bLateJoinEligible = m_flTimeRequestedLateJoin != -1.f && GetLobby() && GetLobby()->GetLateJoinEligible();

}


// ***************************************************************************************************************
void CTFGCServerSystem::SendMvMVictoryResult()
{
	// Note that we don't have to have an *ended* match -- MvM code technically allows players to continue in the same
	// match and achieve multiple victories.
	Assert( m_pMatchInfo );

	CTFGSLobby *pLobby = GetLobby();
	if ( !pLobby )
	{
		// FIXME - We should be able to submit this even if the GC reboots and loses our lobby state (though it wont
		// happen that often, as the GC tries to revive lobby state from memcached)
		MMLog( "CTFGCServerSystem::MvMVictory() -- no lobby, so not sending results to GC\n" );
		return;
	}

	if ( IsMannUpGroup( pLobby->GetMatchGroup() ) )
	{
		m_mvmVictoryInfo.Init( pLobby );

		ReliableMsgMvMVictory *pReliable = new ReliableMsgMvMVictory;

		auto &msg = pReliable->Msg().Body();

		msg.set_mission_name( m_mvmVictoryInfo.m_sChallengeName );
#ifdef USE_MVM_TOUR
		if ( !m_mvmVictoryInfo.m_sMannUpTourOfDuty.IsEmpty() )
		{
			msg.set_tour_name_mannup( m_mvmVictoryInfo.m_sMannUpTourOfDuty );
		}
#endif // USE_MVM_TOUR
		msg.set_lobby_id( m_mvmVictoryInfo.m_nLobbyId );
		msg.set_event_time( m_mvmVictoryInfo.m_tEventTime );

		FOR_EACH_VEC( m_mvmVictoryInfo.m_vPlayerIds, iMember )
		{
			CMsgMvMVictory_Player *pMsgPlayer = msg.add_players();
			pMsgPlayer->set_steam_id( m_mvmVictoryInfo.m_vPlayerIds[ iMember ]);
			pMsgPlayer->set_squad_surplus( m_mvmVictoryInfo.m_vSquadSurplus[ iMember ] );
		}

		pReliable->Enqueue();
	}
}

////-----------------------------------------------------------------------------
//// Purpose: Job for being told when the server GC connection is established
////-----------------------------------------------------------------------------
//class CGCClientJobServerWelcome : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobServerWelcome( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		CProtoBufMsg<CMsgServerWelcome> msg( pNetPacket );
//
//		g_bServerReceivedGCWelcome = true;
//
//		GTFGCClientSystem()->UpdateGCServerInfo();
//
//		// Validate version
//		int engineServerVersion = engine->GetServerVersion();
//		g_gcServerVersion = (int)msg.Body().version();
//
//		// Version checking is enforced if both sides do not report zero as their version
//		if ( engineServerVersion && g_gcServerVersion && engineServerVersion != g_gcServerVersion )
//		{
//			// If we're out of date exit
//			Msg("Version out of date (GC wants %d, we are %d)!\n", g_gcServerVersion, engine->GetServerVersion() );
//
//			// If we hibernating, quit now, otherwise we will quit on hibernation
//			if ( g_ServerGameDLL.m_bIsHibernating )
//			{
//				engine->ServerCommand( "quit\n" );
//			}
//		}
//		else
//		{
//			Msg("GC Connection established for server version %d\n", engine->GetServerVersion() );
//		}
//
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientJobServerWelcome, "CGCClientJobServerWelcome", k_EMsgGCServerWelcome, k_EServerTypeGCClient );


//// temp for tracking down machines submitted stats
//#if defined ( _WIN32 )
//#define WIN32_LEAN_AND_MEAN
//#undef INVALID_HANDLE_VALUE
//#undef DECLARE_HANDLE
//#include <windows.h>
//bool DOTA_GetComputerName( char *pszComputerName, DWORD *length )
//{
//	return !!GetComputerName( pszComputerName, length );
//}
//#endif

// **************************************************************************************************
void CTFGCServerSystem::SendRejectLobby()
{
	MMLog( "Sending CMsgGameServerKickingLobby to reject stale lobby\n" );

	ReliableMsgGameServerKickingLobby *pReliable = new ReliableMsgGameServerKickingLobby();

	auto &msg = pReliable->Msg().Body();
	msg.set_create_party( false );
	if ( GetLobby() )
	{
		msg.set_lobby_id( GetLobby()->GetGroupID() );
		msg.set_lobby_id( GetLobby()->GetMatchID() );
	}

	pReliable->Enqueue();
}

// **************************************************************************************************
void CTFGCServerSystem::EndManagedMatch( bool bKickPlayersToParties )
{
	CMatchInfo *pMatch = GetMatch();
	// Sanity
	AssertMsg( !pMatch || !pMatch->m_bMatchEnded, "Ending an already ended match" );
	if ( !pMatch )
		{ return; }

	pMatch->SetEnded();

	// Cancel launching the new match. Leave the rest of the state alone, we'll send a NewMatch -> EndMatch and things
	// will just work out as responses come in.
	m_flWaitingForNewMatchTime = 0.f;

	if ( !m_pMatchInfo->m_bSentResult )
	{
		Warning( "Ending a managed match without sending a result" );
		Assert( false );
	}

	ReliableMsgGameServerKickingLobby *pReliable = new ReliableMsgGameServerKickingLobby();
	auto &msg = pReliable->Msg().Body();

	if ( bKickPlayersToParties )
	{
		CUtlVector<CSteamID> vecConnectedPlayers;
		int total = pMatch->GetNumTotalMatchPlayers();

		for ( int idx = 0; idx < total; idx++ )
		{
			CMatchInfo::PlayerMatchData_t *pMatchPlayer = pMatch->GetMatchDataForPlayer( idx );
			if ( !pMatchPlayer->bDropped && pMatchPlayer->bConnected )
			{
				msg.add_connected_players( pMatchPlayer->steamID.ConvertToUint64() );
			}
		}


		if ( msg.connected_players_size() <= 0 )
		{
			bKickPlayersToParties = false;
		}
	}

	if ( bKickPlayersToParties )
	{
		MMLog( "Sending CMsgGameServerKickingLobby, requesting party with %d connected players\n", msg.connected_players_size() );
	}
	else
	{
		MMLog( "Sending CMsgGameServerKickingLobby, not requesting party\n" );
	}

	msg.set_create_party( bKickPlayersToParties );
	msg.set_lobby_id( pMatch->m_nLobbyID );
	msg.set_match_id( pMatch->m_nMatchID );

	pReliable->Enqueue();
}

// **************************************************************************************************
void CTFGCServerSystem::SendPlayerLeftMatch( CSteamID targetPlayer, TFMatchLeaveReason eReason, bool bIsAbandon )
{
	CMatchInfo *pMatch = GetMatch();
	// Sanity
	AssertMsg( pMatch && !pMatch->m_bMatchEnded, "Don't expect to be sending this without a live match" );
	if ( !pMatch )
		{ return; }

	ReliableMsgPlayerLeftMatch *pReliable = new ReliableMsgPlayerLeftMatch();
	auto &msg = pReliable->Msg().Body();

	msg.set_steam_id( targetPlayer.ConvertToUint64() );
	msg.set_leave_reason( eReason );
	MMLog( "Sending CMsgPlayerLeftMatch with target of %s [ abandon = %d ]\n", targetPlayer.Render(), bIsAbandon );

	msg.set_lobby_id( pMatch->m_nLobbyID );
	msg.set_match_id( pMatch->m_nMatchID );
	msg.set_was_abandon( bIsAbandon );

	pReliable->Enqueue();
}

// **************************************************************************************************
void CTFGCServerSystem::SendCompetitiveMatchResult( GCSDK::CProtoBufMsg< CMsgGC_Match_Result > *pMatchResultMsg )
{
	// We should have matchinfo when completing a ladder match
	if ( !m_pMatchInfo )
	{
		Warning( "Sending competitive match results without match info!\n" );
		Assert( false );
	}

	if ( m_pMatchInfo->m_bSentResult )
	{
		Warning( "Sending competitive match results without an ended match\n" );
		Assert( false );
	}

	ReliableMsgMatchResult *pReliable = new ReliableMsgMatchResult;
	auto &msg = pReliable->Msg().Body();
	/// XXX(JohnS): With refactor this is now kinda silly. Callers should really just be giving us a CMsgGC_Match_Result
	///             instead of the wrapper.
	msg.CopyFrom( pMatchResultMsg->Body() );
	pReliable->Enqueue();

	m_pMatchInfo->m_bSentResult = true;
}

// **************************************************************************************************
bool CTFGCServerSystem::BLateJoinEligible()
{
	return m_bLateJoinEligible;
}

// **************************************************************************************************
void CTFGCServerSystem::AcceptGCReservation( CSteamID steamID, const CTFLobbyMember *pMemberData, bool bIsLateJoin, int nEntindex, bool bActive )
{
	if ( m_pMatchInfo )
	{
		// Accepting new player to competitive match, add to match data
		MMLog( "New match player %s\n", steamID.Render() );
		m_pMatchInfo->AddPlayer( steamID, pMemberData, bIsLateJoin, nEntindex, bActive );
	}
}

// **************************************************************************************************
void CTFGCServerSystem::AbortInvalidMatchState()
{
	// TODO ROLLING MATCHES: SteamAPI_SetMiniDumpComment / SteamAPI_WriteMiniDump
	MMLog( "**** MM Server in invalid match state, terminating\n" );
	engine->ServerCommand( "quit\n" );
}

// **************************************************************************************************
void CTFGCServerSystem::MMServerModeChanged()
{
	// Save old boolean state
	bool bSaveMMServerMode = m_bMMServerMode;

	// Set new state
	m_bMMServerMode = ( tf_mm_servermode.GetInt() != 0 );

	// Check if logical state is changing; output some text no matter what
	if ( m_bMMServerMode )
	{
		if ( bSaveMMServerMode )
		{
			MMLog( "Lobby-based matchmaking is active\n" );
		}
		else
		{
			MMLog( "Entering lobby-based matchmaking mode\n" );
		}

		if ( tf_mm_strict.GetInt() == 0 )
		{
			MMLog( "   Open mode active.  Gameserver will show in server browser and accept ad-hoc joins.\n" );
		}
		else if ( tf_mm_strict.GetInt() == 1 )
		{
			MMLog( "   Strict mode is active.  Gameserver will not show in server browser or accept ad-hoc joins.\n" );
		}
		else
		{
			MMLog( "   Server is hidden from server browser list, but will accept ad-hoc joins.\n" );
		}

		if ( tf_mm_trusted.GetInt() != 0 )
		{
			MMLog( "   Requested trusted server status.\n" );
		}

	}
	else
	{
		if ( bSaveMMServerMode )
		{
			MMLog( "Leaving lobby-based matchmaking mode\n" );
		}
		else
		{
			MMLog( "Lobby-based matchmaking mode not active\n" );
		}
	}

	// Force this major change out immediately
	UpdateConnectedPlayersAndServerInfo( CMsgGameServerMatchmakingStatus_Event_None, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGCServerSystem::LaunchNewMatchForLobby()
{
	/// XXX(JohnS): Technically the lobby might legitimately be gone here -- if we have gotten the NewMatchForLobby
	///             response and the GC then croaks, we might be told it lost our lobby, but have the new match
	///             assignment and be able to proceed without needing the lobby at all (as in normal cases where the GC
	///             loses state after giving us the authority to run a match).
	///
	///             Since the match in question hasn't started yet, and this is nearly impossible given the timing
	///             window, I'm not doing the work to cache the lobby values we need in here just to let the
	///             just-created match survive that edge case.
	const CTFGSLobby* pLobby = GetLobby();

	if ( !pLobby || m_flWaitingForNewMatchTime == 0.f || !m_pMatchInfo || \
	     m_pMatchInfo->BMatchTerminated() || m_pMatchInfo->m_bServerCreated )
	{
		// You need to prepare for the switch with RequestNewMatchForLobby first. Should not have gotten here if we have
		// a terminated or server created match -- Must still be managed by the GC in order to roll into a new match.
		Assert( false );
		MMLog( "!! Attempting to launch a new match for a lobby without valid state\n" );
		AbortInvalidMatchState();
	}

	m_flWaitingForNewMatchTime = 0.f;

	CMatchInfo* pNewMatchInfo = new CMatchInfo( pLobby );
	// The old match info is holding the vote-winning map name
	pNewMatchInfo->m_strMapName = m_pMatchInfo->m_strMapName;
	EMatchGroup eMatchGroup = pLobby->GetMatchGroup();

	// We still need a new match ID from the GC.  Mark that this new match is
	// created by us so that:	1) If we do get a response for a new match ID
	//							   we know what to do with it
	//							2) The GC knows to assign it a match ID if it
	//							   gets a match result for it before (1) occurs
	if ( m_bWaitingForNewMatchID )
	{
		// Mark that we're going rogue
		pNewMatchInfo->m_bServerCreated = true;
		pNewMatchInfo->m_nMatchID = 0; // Don't inherit the stale one from the lobby

		if ( !CanChangeMatchPlayerTeams() )
		{
			// Server created speculative matches are counting on the GC approving this when it wakes up, and also
			// approving our override of player teams below.  If we want a mode that does rolling matches but has no
			// authority to override teams, we'd need to just cancel the pending match here instead of using
			// m_bServerCreated
			AbortInvalidMatchState();
		}
	}

	for( int idx = 0; idx < m_pMatchInfo->GetNumTotalMatchPlayers(); idx++ )
	{
		const CMatchInfo::PlayerMatchData_t* pPlayerMatchData = m_pMatchInfo->GetMatchDataForPlayer( idx );
		// We don't need record of dropped players for the new match
		if ( pPlayerMatchData->bDropped )
			{ continue; }

		// We stop doing maintenance on lobby->match sync during the pending-new-match period, but we don't want to
		// include players who would be dropped on the first think -- we'd have erroneous record that they were
		// officially part of the match for some period, when they were not.
		//
		// XXX(JohnS): Technically, we could create a speculative match, then when the new match ID arrives, some
		//             members vanished -- those members were never actually part of the lobby from the GC
		//             perspective. We might need to cull these people on the first post-new-matchID-think if having
		//             record of them is causing problems. (a bWasEverConfirmedByGC flag?)
		if ( !pLobby->GetMemberDetails( pPlayerMatchData->steamID ) )
			{ continue; }

		// AddPlayer needs to know if they are connected/active right now
		int nEntIndex = 0;
		bool bActive = false;
		if ( pPlayerMatchData->bConnected )
		{
			if ( pPlayerMatchData->nConnectingButNotActiveIndex )
			{
				// Connected, not active
				bActive = false;
				nEntIndex = pPlayerMatchData->nConnectingButNotActiveIndex;
			}
			else
			{
				// Connected and active
				bActive = true;
				// We could null check this but we'd just use the information to call AbortInvalidMatchState().
				nEntIndex = UTIL_PlayerBySteamID( pPlayerMatchData->steamID )->entindex();
			}
		}

		pNewMatchInfo->AddPlayer( *pPlayerMatchData, nEntIndex, bActive );
	}

	delete m_pMatchInfo;
	m_pMatchInfo = pNewMatchInfo;

	// If we are going ahead with a server-created match, queue a ChangeMatchPlayerTeams message in sequence with our
	// pending new match request -- the GC will process, in order:
	//
	// - Give us a new match!
	//   -> Okay here's new match & teams
	// - Set everyone's teams to (the previous match teams)!
	//   -> Okay here's new lobby with teams that match your state
	//
	// ... And since we don't run UpdateConnectedPlayers() while messages are in queue, by time we run our next
	// look-at-the-lobby think, we'll be in sync again.
	CUtlVector< PlayerTeamPair_t > vecPlayerTeams;
	for( int idx = 0; idx < m_pMatchInfo->GetNumTotalMatchPlayers(); idx++ )
	{
		const CMatchInfo::PlayerMatchData_t *pPlayer = m_pMatchInfo->GetMatchDataForPlayer( idx );
		vecPlayerTeams.AddToTail( { pPlayer->steamID, pPlayer->eGCTeam } );
	}
	ChangeMatchPlayerTeams( vecPlayerTeams );

	GTFGCClientSystem()->DumpLobby();

	if ( eMatchGroup == EMatchGroup::k_nMatchGroup_Invalid ||
		!GetMatchGroupDescription( eMatchGroup )->InitServerSettingsForMatch( pLobby ) )
	{
		AbortInvalidMatchState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activate / deactive GC hosting mode
//-----------------------------------------------------------------------------
void OnMMServerModeChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	GTFGCClientSystem()->MMServerModeChanged();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void OnMMServerModeTrustedChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	OnMMServerModeChanged( pConVar, pOldString, flOldValue );
}

ConVar tf_mm_servermode( "tf_mm_servermode", "0", FCVAR_NOTIFY,
	"Activates / deactivates Lobby-based hosting mode.\n"
	"   0 = not active\n"
	"   1 = Put in matchmaking pool (Lobby will control current map)\n",
	true,
	0.f,
	true,
	1.f,
	OnMMServerModeChanged );

ConVar tf_mm_strict( "tf_mm_strict", "0", FCVAR_NOTIFY,
	"   0 = Show in server browser, and allow ad-hoc joins\n"
	"   1 = Hide from server browser and only allow joins coordinated through GC matchmaking\n"
	"   2 = Hide from server browser, but allow ad-hoc joins\n",
	OnMMServerModeChanged );

ConVar tf_mm_trusted( "tf_mm_trusted", "0", FCVAR_NOTIFY | FCVAR_HIDDEN,
	"Set to 1 on Valve servers to requested trusted status.  (Yes, it is authenticated on the backend, and attempts by non-valve servers are logged.)\n",
	OnMMServerModeTrustedChanged );

#endif // #ifdef ENABLE_GC_MATCHMAKING
