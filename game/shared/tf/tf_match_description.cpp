//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_match_description.h"
#include "tf_ladder_data.h"
#include "tf_rating_data.h"

#ifdef GC_DLL
#include "tf_lobby.h"
#include "tf_partymanager.h"
#endif

#if defined CLIENT_DLL || defined GAME_DLL
#include "tf_gamerules.h"
#endif

#ifdef CLIENT_DLL
#include "tf_gc_client.h"
#include "animation.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "tf_lobby_server.h"
#endif

#ifdef GAME_DLL
#include "tf_lobby_server.h"
#include "tf_gc_server.h"
#include "tf_objective_resource.h"
#include "team_control_point_master.h"
#endif

#ifdef GC_DLL
GCConVar tf_mm_ladder_force_map_by_name( "tf_mm_ladder_force_map_by_name", "", "If specified, force any matches that form for 6v6, 9v9, 12v12 to use the specified map (e.g. cp_sunshine)." );
GCConVar tf_mm_casual_rejoin_cooldown_secs( "tf_mm_casual_rejoin_cooldown_secs", "180",
                                            "How many seconds must pass before anyone can re-match into a casual or MvM lobby they have left. "
                                            "Setting this too low may allow someone to rejoin a lobby while a vote-kick for them is still occuring, but before it has passed." );
GCConVar tf_mm_casual_rejoin_cooldown_votekick_secs( "tf_mm_casual_rejoin_cooldown_votekick_secs", "10800", // 3 hours
                                                     "How many seconds must pass before a vote-kicked player can re-match into a casual or MvM lobby. "
                                                     "This is not infinite as casual lobbies may persist for days." );
// These are in matchmaking shared
extern GCConVar tf_mm_match_size_mvm;
extern GCConVar tf_mm_match_size_ladder_6v6;
extern GCConVar tf_mm_match_size_ladder_9v9;
extern GCConVar tf_mm_match_size_ladder_12v12;
extern GCConVar tf_mm_match_size_ladder_12v12_minimum;
#else
extern ConVar tf_mm_match_size_mvm;
extern ConVar tf_mm_match_size_ladder_6v6;
extern ConVar tf_mm_match_size_ladder_9v9;
extern ConVar tf_mm_match_size_ladder_12v12;
extern ConVar tf_mm_match_size_ladder_12v12_minimum;
extern ConVar servercfgfile;
extern ConVar lservercfgfile;
extern ConVar mp_tournament_stopwatch;
extern ConVar tf_gamemode_payload;
extern ConVar tf_gamemode_ctf;
#endif

#ifdef GAME_DLL
extern ConVar tf_mvm_allow_abandon_after_seconds;
extern ConVar tf_mvm_allow_abandon_below_players;
#endif

#ifdef GC_DLL
	#define MVM_REQUIRED_SCORE &tf_mm_required_score_mvm
	#define LADDER_REQUIRED_SCORE &tf_mm_required_score_ladder
	#define CASUAL_REQUIRED_SCORE &tf_mm_required_score_ladder
#else
	#define MVM_REQUIRED_SCORE (ConVar*)NULL
	#define LADDER_REQUIRED_SCORE (ConVar*)NULL
	#define CASUAL_REQUIRED_SCORE (ConVar*)NULL
#endif

#ifdef GC_DLL
#include "tf_matchmaker.h"
#include "tf_party.h"

using namespace GCSDK;
extern GCConVar tf_mm_scoring_ladder_skillrating_delta_max_high;
extern GCConVar tf_mm_scoring_ladder_skillrating_delta_max;
extern GCConVar tf_mm_required_score_mvm;
extern GCConVar tf_mm_required_score_ladder;
extern GCConVar tf_mm_required_score_pvp;

// Predicate for below filters
typedef std::function< bool(const CTFMemcachedLobbyFormerMember &) > FilterLeftMember_t;

// Helper that finds any party members that appear in the given match's m_mapFormerMembersByAccountID as having left the
// match, and calls the given predicate on them.
//
// Returns true if no party members are in the former member list with a match-leave flag, or they all pass the
// predicate.
//
// Returns false and stops iterating if predicate fails.
static bool BCheckLeftMatchMembersAgainstParty( const MatchDescription_t *pMatch,
                                                const MatchParty_t *pParty,
                                                const FilterLeftMember_t &predicate )
{
	FOR_EACH_VEC( pParty->m_vecMembers, idxMember )
	{
		const MatchParty_t::Member_t &member = pParty->m_vecMembers[ idxMember ];
		UtlHashHandle_t idx = pMatch->m_mapFormerMembersByAccountID.Find( member.m_steamID.GetAccountID() );
		if ( pMatch->m_mapFormerMembersByAccountID.IsValidHandle( idx ) )
		{
			const CTFMemcachedLobbyFormerMember &formerMember = pMatch->m_mapFormerMembersByAccountID[ idx ];
			// Only care about former members that left a match this lobby was visiting
			if ( !formerMember.has_left_match_time() )
				{ continue; }

			if ( !predicate( formerMember ) )
			{
				return false;
			}
		}
	}

	return true;
}

// Helper that finds any newly-added (not existing incomplete-match-party) members of the given match description that
// appear in  m_mapFormerMembersByAccountID as having left the match previously, and calls the given predicate on them.
//
// Returns true if no new match members appear in the former member list with a match-leave flag, or they all pass the
// predicate.
//
// Returns false and stops iterating if predicate fails.
static bool BCheckLeftMatchMembersAgainstNewlyAddedMembers( const MatchDescription_t *pMatch,
                                                            const FilterLeftMember_t &predicate )
{
	FOR_EACH_VEC( pMatch->m_vecParties, idx )
	{
		auto *pParty = pMatch->m_vecParties[ idx ];
		// If this party is not an incomplete match party (and thus already in the match), run the check
		if ( !pParty->m_bIncompleteMatchParty && !BCheckLeftMatchMembersAgainstParty( pMatch, pParty, predicate ) )
		{
			return false;
		}
	}

	return true;
}

// Shared helper for the casual modes to have a consistent rejoin policy
static bool BThreadedFormerMemberMayRejoinJoinCasualOrMvMMatch( const CTFMemcachedLobbyFormerMember& member )
{
	RTime32 rtNow = CRTime::RTime32TimeCur();
	RTime32 rtLeftLobby = member.left_lobby_time();
	uint32_t unRejoinTimeout = (uint32_t)Max( tf_mm_casual_rejoin_cooldown_secs.GetInt(), 0 );

	if ( unRejoinTimeout && rtLeftLobby != 0 &&
	     rtLeftLobby + unRejoinTimeout > rtNow )
	{
		// Rejoin too soon after leaving lobby
		return false;
	}

	uint32_t unVoteKickTimeout = (uint32_t)Max( tf_mm_casual_rejoin_cooldown_votekick_secs.GetInt(), 0 );
	RTime32 rtLeftMatch = member.left_match_time();
	TFMatchLeaveReason eLeftMatchReason = member.left_match_reason();
	if ( unVoteKickTimeout &&
	     rtLeftMatch != 0 &&
	     ( eLeftMatchReason == TFMatchLeaveReason_VOTE_KICK ||
	       eLeftMatchReason == TFMatchLeaveReason_ADMIN_KICK ) &&
	     rtLeftMatch + unVoteKickTimeout > rtNow )
	{
		// Too soon after votekick
		return false;
	}

	return true;
}

int IMatchGroupDescription::GetServerPoolIndex( EMatchGroup eGroup, EMMServerMode eMode ) const
{
	int nResult = (int)eGroup;
	switch ( eMode )
	{
	case eMMServerMode_Idle:
	{
		nResult = k_nGameServerPool_Idle;
	}
	break;

	case eMMServerMode_Full:
	{
		COMPILE_TIME_ASSERT( (int)k_nMatchGroup_MvM_Practice + (int)k_nGameServerPool_Full_First == (int)k_nGameServerPool_MvM_Practice_Full );
		nResult += k_nGameServerPool_MvM_Practice_Full;
		Assert( nResult >= k_nGameServerPool_Full_First );
		Assert( nResult <= k_nGameServerPool_Full_Last );
		return nResult;
	}
	break;

	case eMMServerMode_Incomplete_Match:
	{
		COMPILE_TIME_ASSERT( (int)k_nMatchGroup_MvM_Practice + (int)k_nGameServerPool_Incomplete_Match_First == (int)k_nGameServerPool_MvM_Practice_Incomplete_Match );
		nResult += k_nGameServerPool_Incomplete_Match_First;
		Assert( nResult >= k_nGameServerPool_Incomplete_Match_First );
		Assert( nResult <= k_nGameServerPool_Incomplete_Match_Last );
	}
	break;

	default:
		Assert( false );
	}

	return nResult;
}
#endif

#ifdef GAME_DLL

bool IMatchGroupDescription::InitServerSettingsForMatch( const CTFGSLobby* pLobby ) const
{
	// Setting servercfgfile to our mode-specific config causes the server to exec it once it finishes
	// loading the map from the changelevel below
	servercfgfile.SetValue( m_params.m_pszExecFileName );
	lservercfgfile.SetValue( m_params.m_pszExecFileName );

	return TFGameRules()->StartManagedMatch();
}
#endif

#ifdef CLIENT_DLL

#ifdef STAGING_ONLY
void cc_tf_test_pvp_rank_xp_change( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "experience_changed" );
	if ( pEvent )
	{
		gameeventmanager->FireEventClientSide( pEvent );
	}
}
ConVar tf_test_pvp_rank_xp_change( "tf_test_pvp_rank_xp_change", "-1", 0, "Force your experience to a specific value", cc_tf_test_pvp_rank_xp_change );


CON_COMMAND_F( tf_progression_set_xp_to_level, "Overrides your XP to be within the range the level specified", FCVAR_CHEAT )
{
	if ( args.ArgC() != 3 )
	{
		Msg( "Usage tf_progression_set_xp_to_level <matchgroup> <level>\n" );
		return;
	}

	const IMatchGroupDescription* pMatch = GetMatchGroupDescription( (EMatchGroup)atoi( args[1] ) );
	if ( !pMatch || !pMatch->m_pProgressionDesc )
		return;

	const LevelInfo_t& level = pMatch->m_pProgressionDesc->GetLevelByNumber( atoi( args[2] ) );
	tf_test_pvp_rank_xp_change.SetValue( RandomInt( level.m_nStartXP, level.m_nEndXP ) );

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "experience_changed" );
	if ( pEvent )
	{
		gameeventmanager->FireEventClientSide( pEvent );
	}

	pEvent = gameeventmanager->CreateEvent( "begin_xp_lerp" );
	if ( pEvent )
	{
		gameeventmanager->FireEventClientSide( pEvent );
	}
}

CON_COMMAND_F( tf_progression_set_xp_to_value, "Overrides your XP to be a specific value", FCVAR_CHEAT )
{
	if ( args.ArgC() != 3 )
	{
		Msg( "Usage tf_progression_set_xp_to_level <matchgroup> <value>\n" );
		return;
	}

	const IMatchGroupDescription* pMatch = GetMatchGroupDescription( (EMatchGroup)atoi( args[1] ) );
	if ( !pMatch || !pMatch->m_pProgressionDesc )
		return;

	tf_test_pvp_rank_xp_change.SetValue( atoi( args[2] ) );

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "experience_changed" );
	if ( pEvent )
	{
		gameeventmanager->FireEventClientSide( pEvent );
	}

	pEvent = gameeventmanager->CreateEvent( "begin_xp_lerp" );
	if ( pEvent )
	{
		gameeventmanager->FireEventClientSide( pEvent );
	}
}
#endif // STAGING_ONLY
#endif // CLIENT_DLL

// Casual XP constants
const float flAverageXPPerGame = 500.f;
const float flAverageMinutesPerGame = 30;

// The target XP per minute
const float flTargetXPPM = (float)flAverageXPPerGame / (float)flAverageMinutesPerGame;

// The target breakdown at the end of a match
const float flScoreXPScale				= 0.4485f;
const float flObjectiveXPScale			= 0.15f;
const float flMatchCompletionXPScale	= 0.3f;

// These come from the first 4 weeks of MyM match data
const float flAvgPPMPM = 27.f;	// Points per minute per match
const float flAvgPPMPP = 1.15f;	// Points per minute per player (above / 24)

const XPSourceDef_t g_XPSourceDefs[] = { { "MatchMaking.XPChime", "TF_XPSource_PositiveFormat", "#TF_XPSource_Score", flTargetXPPM * flScoreXPScale / flAvgPPMPP					/* 6.5  */ }	// SOURCE_SCORE = 0;
									   , { "MatchMaking.XPChime", "TF_XPSource_PositiveFormat", "#TF_XPSource_ObjectiveBonus", flTargetXPPM * flObjectiveXPScale / flAvgPPMPM		/* 0.0926  */ }	// SOURCE_OBJECTIVE_BONUS = 1;
									   , { "MatchMaking.XPChime", "TF_XPSource_PositiveFormat", "#TF_XPSource_CompletedMatch", flTargetXPPM * flMatchCompletionXPScale / flAvgPPMPM /* 0.185 */ }	// SOURCE_COMPLETED_MATCH = 2;
									   , { "MVM.PlayerDied", "TF_XPSource_NoValueFormat", "#TF_XPSource_Comp_Abandon", 1.f }																// SOURCE_COMPETITIVE_ABANDON = 3;
									   , { "MatchMaking.XPChime", "TF_XPSource_NoValueFormat", "#TF_XPSource_Comp_Win", 1.f }																// SOURCE_COMPETITIVE_WIN = 4;
									   , { NULL, "TF_XPSource_NoValueFormat", "#TF_XPSource_Comp_Loss", 1.f }																			// SOURCE_COMPETITIVE_LOSS = 5;
									   , { "MatchMaking.XPChime", "TF_XPSource_PositiveFormat", "#TF_XPSource_Autobalance_Bonus", 1.f } };														// SOURCE_AUTOBALANCE_BONUS = 6;

IProgressionDesc::IProgressionDesc( EMatchGroup eMatchGroup
								  , const char* pszBadgeName
								  , const char* pszProgressionResFile 
								  , const char* pszLevelToken )
	: m_eMatchGroup( eMatchGroup )
	, m_strBadgeName( pszBadgeName )
	, m_pszProgressionResFile( pszProgressionResFile )
	, m_pszLevelToken( pszLevelToken )
{}


#ifdef CLIENT_DLL
void IProgressionDesc::EnsureBadgePanelModel( CBaseModelPanel *pModelPanel ) const
{
	studiohdr_t* pHDR = pModelPanel->GetStudioHdr();
	if ( !pHDR || !(CUtlString( pHDR->name ).UnqualifiedFilename() == m_strBadgeName.UnqualifiedFilename()) )
	{
		pModelPanel->SetMDL( m_strBadgeName );
	}
}

const LevelInfo_t& IProgressionDesc::YieldingGetLevelForSteamID( const CSteamID& steamID ) const
{
	return GetLevelForExperience( GetPlayerExperienceBySteamID( steamID ) );
}
#endif // CLIENT_DLL

const LevelInfo_t& IProgressionDesc::GetLevelByNumber( uint32 nNumber ) const
{
	int nIndex = nNumber;
	nIndex = Clamp( nIndex - 1, 0, m_vecLevels.Count() - 1 );
	Assert( nIndex >= 0 && nIndex < m_vecLevels.Count() );
	return m_vecLevels[ nIndex ];
};

const LevelInfo_t& IProgressionDesc::GetLevelForExperience( uint32 nExperience ) const
{
	uint32 nNumLevels = (uint32)m_vecLevels.Count();
	// Walk the levels to find where the passed in experience value falls
	for( uint32 i=0; i<nNumLevels; ++i )
	{
		if ( nExperience >= m_vecLevels[ i ].m_nStartXP && ( nExperience < m_vecLevels[ i ].m_nEndXP || (i + 1) == nNumLevels ) )
		{
			return m_vecLevels[ i ];
		}
	}

	Assert( false );
	return m_vecLevels[ 0 ];
}

class CMvMMatchGroupDescription : public IMatchGroupDescription
{
	public:
		CMvMMatchGroupDescription( EMatchGroup eMatchGroup, const char* pszConfig, bool bTrustedOnly )
			: IMatchGroupDescription( eMatchGroup
									  , { eMatchMode_MatchMaker_LateJoinDropIn // m_eLateJoinMode;
									  , eMMPenaltyPool_Casual // m_ePenaltyPool
									  , false				  // m_bUsesSkillRatings;
									  , false				  // m_bSupportsLowPriorityQueue;
									  , false				  // m_bRequiresMatchID;
									  , MVM_REQUIRED_SCORE	  // m_pmm_required_score;
									  , false				  // m_bUseMatchHud;
									  , pszConfig			  // m_pszExecFileName;
									  , &tf_mm_match_size_mvm // m_pmm_match_group_size;
									  , NULL				  // m_pmm_match_group_size_minimum;
									  , MATCH_TYPE_MVM		  // m_eMatchType;
									  , false				  // m_bShowPreRoundDoors;
									  , false				  // m_bShowPostRoundDoors;
									  , NULL				  // m_pszMatchEndKickWarning;
									  , NULL				  // m_pszMatchStartSound;
									  , false				  // m_bAutoReady;
									  , false				  // m_bShowRankIcons;
									  , false				  // m_bUseMatchSummaryStage;
									  , false				  // m_bDistributePerformanceMedals;
									  , false				  // m_bIsCompetitiveMode;
									  , false				  // m_bUseFirstBlood;
									  , false				  // m_bUseReducedBonusTime;
									  , false				  // m_bUseAutoBalance;
									  , false				  // m_bAllowTeamChange;
									  , true				  // m_bRandomWeaponCrits;
									  , false				  // m_bFixedWeaponSpread;
									  , false				  // m_bRequireCompleteMatch;
									  , bTrustedOnly		  // m_bTrustedServersOnly;
									  , false				  // m_bForceClientSettings;
									  , false				  // m_bAllowDrawingAtMatchSummary
									  , true				  // m_bAllowSpecModeChange
									  , false				  // m_bAutomaticallyRequeueAfterMatchEnds
									  , false				  // m_bUsesMapVoteOnRoundEnd
									  , false				  // m_bUsesXP
									  , false				  // m_bUsesDashboardOnRoundEnd
									  , false				  // m_bUsesSurveys
									  , false } )			  // m_bStrictMatchmakerScoring
		{}

#ifdef GC_DLL
	virtual EMMRating PrimaryMMRatingBackend() const OVERRIDE { return k_nMMRating_Invalid; }
	virtual const std::vector< EMMRating > &MatchResultRatingBackends() const OVERRIDE
	{
		static std::vector< EMMRating > mvmRatings = { /* crickets */ };
		return mvmRatings;
	}

	// Copy the party's search challenges
	bool InitMatchFromParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const OVERRIDE
	{
		pMatch->m_setAcceptableChallenges = pParty->m_setSearchChallenges;

#ifdef USE_MVM_TOUR
		if ( pMatch->m_eMatchGroup == k_nMatchGroup_MvM_MannUp )
		{
			Assert( pParty->m_iMannUpTourOfDuty >= 0 );
			pMatch->m_iMannUpTourOfDuty = pParty->m_iMannUpTourOfDuty;
		}
		else
		{
			Assert( pParty->m_iMannUpTourOfDuty == k_iMvmTourIndex_NotMannedUp );
			pMatch->m_iMannUpTourOfDuty = k_iMvmTourIndex_NotMannedUp;
		}
#endif // USE_MVM_TOUR
		return true;
	}

	virtual bool InitMatchFromLobby( MatchDescription_t* pMatch, CTFLobby* pLobby ) const OVERRIDE 
	{ 
		pMatch->m_setAcceptableChallenges.Clear();
		pMatch->m_setAcceptableChallenges.SetMissionBySchemaIndex( pLobby->GetMissionIndex(), true );

#ifdef USE_MVM_TOUR
		if ( pLobby->GetMatchGroup() == k_nMatchGroup_MvM_MannUp )
		{
			pMatch->m_iMannUpTourOfDuty = pLobby->GetMannUpTourIndex();
		}
		else
		{
			Assert( pLobby->GetMannUpTourIndex() == k_iMvmTourIndex_NotMannedUp );
		}
#endif // USE_MVM_TOUR

		return true;
	}

	// Sync selected MvM challenges
	virtual void SyncMatchParty( const CTFParty *pParty, MatchParty_t *pMatchParty ) const OVERRIDE
	{
		pParty->GetSearchChallenges( pMatchParty->m_setSearchChallenges );
	}

	// Go through our selected challenges and pick a random challenge then a random popfile from the chosen challenge
	virtual void SelectModeSpecificParameters( const MatchDescription_t* pMatch, CTFLobby* pLobby ) const OVERRIDE
	{
		if ( !pMatch )
			return;

		int nCountPops = GetItemSchema()->GetMvmMissions().Count();
		int nSelectedChallenge = -1;
		nSelectedChallenge = -1;

		int n = 0;
		for ( int i = 0 ; i < nCountPops ; ++i )
		{
			if ( pMatch->m_setAcceptableChallenges.GetMissionBySchemaIndex( i ) )
			{
				int r = RandomInt( 0, n );
				if ( r == 0 )
				{
					nSelectedChallenge = i;
				}
				++n;
			}
		}

		// We *should* have chosen one by now, unless the schema is hosed.
		// But if we haven't, force it to be selected now
		if ( nSelectedChallenge < 0 )
		{
			Assert( nSelectedChallenge >= 0 );
			nSelectedChallenge = RandomInt( 0, nCountPops-1 );
		}

		Assert( nSelectedChallenge >= 0 );
		pLobby->SetMapName( GetItemSchema()->GetMvmMissions()[ nSelectedChallenge ].m_sMapNameActual.Get() );
		pLobby->SetMissionName( GetItemSchema()->GetMvmMissions()[ nSelectedChallenge ].m_sPop.Get() );
#ifdef USE_MVM_TOUR
		if ( pMatch->m_eMatchGroup == k_nMatchGroup_MvM_MannUp )
		{
			Assert( pMatch->m_iMannUpTourOfDuty >= 0 );
			pLobby->SetMannUpTourName( GetItemSchema()->GetMvmTours()[pMatch->m_iMannUpTourOfDuty].m_sTourInternalName.Get() );
		}
		else
		{
			Assert( pMatch->m_iMannUpTourOfDuty == k_iMvmTourIndex_NotMannedUp );
		}
#endif // USE_MVM_TOUR
	}

	// Check MvM challenges have an intersection between the current and searching parties
	virtual bool BThreadedPartyCompatibleWithMatch( const MatchDescription_t* pMatch, const MatchParty_t *pCandidateParty ) const OVERRIDE
	{
		// Check for blacklisted former members that tank compatibility
		if ( !BCheckLeftMatchMembersAgainstParty( pMatch, pCandidateParty, &BThreadedFormerMemberMayRejoinJoinCasualOrMvMMatch) )
			{ return false; }

#ifdef USE_MVM_TOUR
		return pCandidateParty->m_iMannUpTourOfDuty == pMatch->m_iMannUpTourOfDuty;
#endif // USE_MVM_TOUR

		return pMatch->m_setAcceptableChallenges.HasIntersection( pCandidateParty->m_setSearchChallenges );
	}

	virtual bool BThreadedPartiesCompatible( const MatchParty_t *pLeftParty, const MatchParty_t *pRightParty ) const OVERRIDE
	{
#ifdef USE_MVM_TOUR
		if ( pLeftParty->m_iMannUpTourOfDuty != pRightParty->m_iMannUpTourOfDuty )
			return false;
#endif // USE_MVM_TOUR

		return !pLeftParty->m_setSearchChallenges.HasIntersection( pRightParty->m_setSearchChallenges );
	}

	// Intersect MvM challenges
	bool BThreadedIntersectMatchWithParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const OVERRIDE
	{
		// Check for blacklisted former members that tank compatibility
		if ( !BCheckLeftMatchMembersAgainstParty( pMatch, pParty, &BThreadedFormerMemberMayRejoinJoinCasualOrMvMMatch) )
			{ return false; }

		pMatch->m_setAcceptableChallenges.Intersect( pParty->m_setSearchChallenges );
		if ( pMatch->m_setAcceptableChallenges.IsEmpty() )
			return false;

#ifdef USE_MVM_TOUR
		if ( pMatch->m_eMatchGroup == k_nMatchGroup_MvM_MannUp )
		{
			Assert( pParty->m_iMannUpTourOfDuty >= 0 );
			if ( pMatch->m_iMannUpTourOfDuty != pParty->m_iMannUpTourOfDuty )
				return false;
		}
		else
		{
			Assert( pParty->m_iMannUpTourOfDuty == k_iMvmTourIndex_NotMannedUp );
		}
#endif // USE_MVM_TOUR

		return true;
	}

	virtual void GetServerDetails( const CMsgGameServerMatchmakingStatus& msg, int& nChallengeIndex, const char* pszMap ) const OVERRIDE
	{
		if ( msg.matchmaking_state() == ServerMatchmakingState_EMPTY ) // if we're empty, we can switch the challenge, so the current value doesn't matter
		{
			pszMap = "";
		}
		else
		{
			nChallengeIndex = GetItemSchema()->FindMvmMissionByName( pszMap );
		}
	}

	virtual const char* GetUnauthorizedPartyReason( CTFParty* pParty ) const OVERRIDE
	{
		pParty->CheckRemoveInvalidSearchChallenges();
		CMvMMissionSet searchChallenges;
		pParty->GetSearchChallenges( searchChallenges );
		if ( searchChallenges.IsEmpty() )
		{
			return "They want to play MvM, but set of search challenges is empty.";
		}

		if ( pParty->GetMatchGroup() == k_nMatchGroup_MvM_MannUp )
		{
			TFPartyManager()->YldUpdatePartyMemberData( pParty );
			if ( pParty->BAnyMemberWithoutTicket() )
			{
				return "They want to play MannUp, but somebody doesn't have a ticket.";
			}

#ifdef USE_MVM_TOUR
			// Make sure we know what tour of duty the want to play
			if ( pParty->GetSearchMannUpTourIndex() < 0 )
			{
				return "They want to play MannUp, but no tour of duty specified.";
			}
#endif // USE_MVM_TOUR
		}
		
		return NULL;
	}

	virtual void Dump( const char *pszLeader, int nSpewLevel, int nLogLevel, const MatchParty_t* pMatch ) const OVERRIDE
	{
		CUtlString sSelectedPops;
		int n = 0;
		for ( int i = 0 ; i < GetItemSchema()->GetMvmMissions().Count() ; ++i )
		{
			if ( pMatch->m_setSearchChallenges.GetMissionBySchemaIndex( i ) )
			{
				if ( n > 0 )
					sSelectedPops += ", ";
				if ( n >= 5  )
				{
					sSelectedPops += "...";
					break;
				}
				sSelectedPops += GetItemSchema()->GetMvmMissionName( i );
				++n;
			}
		}
		EmitInfo( SPEW_GC, nSpewLevel, nLogLevel, "%s  MvM Type: %s    Search Pop: %s\n", pszLeader,
		          ( m_eMatchGroup == k_nMatchGroup_MvM_MannUp ? "Mann Up" : ( m_eMatchGroup == k_nMatchGroup_MvM_Practice ? "Bootcamp" : "UNKNOWN" ) ),
		          sSelectedPops.String() );
		EmitInfo( SPEW_GC, nSpewLevel, nLogLevel, "%s  Best valve data center ping: %.0fms\n", pszLeader, pMatch->m_flPingClosestServer );
	}
#endif

#ifdef CLIENT_DLL
	virtual bool BGetRoundStartBannerParameters( int& nSkin, int& nBodyGroup ) const OVERRIDE
	{
		// Dont show in MvM...for now
		return false;
	}

	virtual bool BGetRoundDoorParameters( int& nSkin, int& nLogoBodyGroup ) const OVERRIDE
	{
		// Don't show in MvM...for now
		return false;
	}

	virtual const char *GetMapLoadBackgroundOverride( bool bWidescreen ) const OVERRIDE
	{
		if ( bWidescreen )
		{
			return NULL;
		}

		return "mvm_background_map";
	}
#endif

#ifdef GAME_DLL
	virtual bool InitServerSettingsForMatch( const CTFGSLobby* pLobby ) const OVERRIDE
	{
		bool bRet = IMatchGroupDescription::InitServerSettingsForMatch( pLobby );

		if ( *pLobby->GetMissionName() != '\0' )
		{
			TFGameRules()->SetNextMvMPopfile( pLobby->GetMissionName() );
		}

		return bRet;
	}

	virtual void PostMatchClearServerSettings() const OVERRIDE
	{

	}

	virtual void InitGameRulesSettings() const OVERRIDE
	{
	}

	virtual void InitGameRulesSettingsPostEntity() const OVERRIDE
	{
	}

	bool ShouldRequestLateJoin() const OVERRIDE
	{
		if ( !TFGameRules() || !TFGameRules()->IsMannVsMachineMode() )
			return false;

		// Check game state
		switch ( TFGameRules()->State_Get() )
		{
		case GR_STATE_INIT:
		case GR_STATE_PREGAME:
		case GR_STATE_STARTGAME:
		case GR_STATE_PREROUND:
		case GR_STATE_TEAM_WIN:
		case GR_STATE_RESTART:
		case GR_STATE_STALEMATE:
		case GR_STATE_BONUS:
		case GR_STATE_BETWEEN_RNDS:
			return true;

		case GR_STATE_RND_RUNNING:
			if ( TFObjectiveResource() &&
			     !TFObjectiveResource()->GetMannVsMachineIsBetweenWaves() &&
			     TFObjectiveResource()->GetMannVsMachineWaveCount() == TFObjectiveResource()->GetMannVsMachineMaxWaveCount() )
			{
				int nMaxEnemyCountNoSupport = TFObjectiveResource()->GetMannVsMachineWaveEnemyCount();
				if ( nMaxEnemyCountNoSupport <= 0 )
				{
					Assert( false ); // no enemies in wave?!
					return false;
				}

				// calculate number of remaining enemies
				int nNumEnemyRemaining = 0;

				for ( int i = 0; i < MVM_CLASS_TYPES_PER_WAVE_MAX_NEW; ++i )
				{
					int nClassCount = TFObjectiveResource()->GetMannVsMachineWaveClassCount( i );
					unsigned int iFlags = TFObjectiveResource()->GetMannVsMachineWaveClassFlags( i );

					if ( iFlags & MVM_CLASS_FLAG_MINIBOSS )
					{
						nNumEnemyRemaining += nClassCount;
					}

					if ( iFlags & MVM_CLASS_FLAG_NORMAL )
					{
						nNumEnemyRemaining += nClassCount;
					}
				}

				// if less then 40% of the last wave remains, lock people out from MM
				if ( (float)nNumEnemyRemaining / (float)nMaxEnemyCountNoSupport < 0.4f )
					return false;
			}
			return true;

		case GR_STATE_GAME_OVER:
			return false;
		}

		Assert( false );
		return false;
	}

	bool BMatchIsSafeToLeaveForPlayer( const CMatchInfo* pMatchInfo, const CMatchInfo::PlayerMatchData_t *pMatchPlayer ) const
	{
		bool bSafe = false;
		// Allow safe leaving after you have played for N seconds or if the match drops below N players, even if it is
		// still active.
		int nAllowAfterSeconds = tf_mvm_allow_abandon_after_seconds.GetInt();
		int nAllowBelowPlayers = tf_mvm_allow_abandon_below_players.GetInt();
		RTime32 now = CRTime::RTime32TimeCur();
		bSafe = bSafe || ( nAllowAfterSeconds > 0 && (uint32)nAllowAfterSeconds < ( now - pMatchPlayer->rtJoinedMatch ) );
		bSafe = bSafe || ( nAllowBelowPlayers > 0 && pMatchInfo->GetNumActiveMatchPlayers() < nAllowBelowPlayers );

		// Bootcamp is a magical nevar-abandon land
		bSafe = bSafe || ( m_eMatchGroup == k_nMatchGroup_MvM_Practice );

		return bSafe;
	}

	virtual bool BPlayWinMusic( int nWinningTeam, bool bGameOver ) const OVERRIDE 
	{
		// Not handled
		return false; 
	} 
#endif
};

class CLadderMatchGroupDescription : public IMatchGroupDescription
{
public:

	class CLadderProgressionDesc : public IProgressionDesc
	{
	public:
		CLadderProgressionDesc( EMatchGroup eMatchGroup )
			: IProgressionDesc( eMatchGroup
							  , "models/vgui/competitive_badge.mdl"
							  , "resource/ui/PvPCompRankPanel.res"
							  , "TF_Competitive_Rank" )
		{
			// Bucket 1
			m_vecLevels.AddToTail( { 1,		k_unDrilloRating_Ladder_Min,		11500, "competitive/competitive_badge_rank001", "#TF_Competitive_Rank_1",  "MatchMaking.RankOneAchieved",	"competitive/comp_background_tier001a" } );
			m_vecLevels.AddToTail( { 2,		m_vecLevels.Tail().m_nEndXP,		13000, "competitive/competitive_badge_rank002", "#TF_Competitive_Rank_2",  "MatchMaking.RankOneAchieved",	"competitive/comp_background_tier001a" } );
			m_vecLevels.AddToTail( { 3,		m_vecLevels.Tail().m_nEndXP,		14500, "competitive/competitive_badge_rank003", "#TF_Competitive_Rank_3",  "MatchMaking.RankOneAchieved",	"competitive/comp_background_tier001a" } );
			m_vecLevels.AddToTail( { 4,		m_vecLevels.Tail().m_nEndXP,		16000, "competitive/competitive_badge_rank004", "#TF_Competitive_Rank_4",  "MatchMaking.RankOneAchieved",	"competitive/comp_background_tier002a" } );
			m_vecLevels.AddToTail( { 5,		m_vecLevels.Tail().m_nEndXP,		17500, "competitive/competitive_badge_rank005", "#TF_Competitive_Rank_5",  "MatchMaking.RankOneAchieved",	"competitive/comp_background_tier002a" } );
			m_vecLevels.AddToTail( { 6,		m_vecLevels.Tail().m_nEndXP,		19500, "competitive/competitive_badge_rank006", "#TF_Competitive_Rank_6",  "MatchMaking.RankOneAchieved",	"competitive/comp_background_tier002a" } );
			// Bucket 2
			m_vecLevels.AddToTail( { 7,		m_vecLevels.Tail().m_nEndXP,		21500, "competitive/competitive_badge_rank007", "#TF_Competitive_Rank_7",  "MatchMaking.RankTwoAchieved",	"competitive/comp_background_tier003a" } );
			m_vecLevels.AddToTail( { 8,		m_vecLevels.Tail().m_nEndXP,		23500, "competitive/competitive_badge_rank008", "#TF_Competitive_Rank_8",  "MatchMaking.RankTwoAchieved",	"competitive/comp_background_tier003a" } );
			m_vecLevels.AddToTail( { 9,		m_vecLevels.Tail().m_nEndXP,		25500, "competitive/competitive_badge_rank009", "#TF_Competitive_Rank_9",  "MatchMaking.RankTwoAchieved",	"competitive/comp_background_tier003a" } );
			m_vecLevels.AddToTail( { 10,	m_vecLevels.Tail().m_nEndXP,		28000, "competitive/competitive_badge_rank010", "#TF_Competitive_Rank_10", "MatchMaking.RankTwoAchieved",	"competitive/comp_background_tier004a" } );
			m_vecLevels.AddToTail( { 11,	m_vecLevels.Tail().m_nEndXP,		30500, "competitive/competitive_badge_rank011", "#TF_Competitive_Rank_11", "MatchMaking.RankTwoAchieved",	"competitive/comp_background_tier004a" } );
			m_vecLevels.AddToTail( { 12,	m_vecLevels.Tail().m_nEndXP,		33000, "competitive/competitive_badge_rank012", "#TF_Competitive_Rank_12", "MatchMaking.RankTwoAchieved",	"competitive/comp_background_tier004a" } );
			// Bucket 3
			m_vecLevels.AddToTail( { 13,	m_vecLevels.Tail().m_nEndXP,		35500, "competitive/competitive_badge_rank013", "#TF_Competitive_Rank_13", "MatchMaking.RankThreeAchieved",	"competitive/comp_background_tier005a" } );
			m_vecLevels.AddToTail( { 14,	m_vecLevels.Tail().m_nEndXP,		38000, "competitive/competitive_badge_rank014", "#TF_Competitive_Rank_14", "MatchMaking.RankThreeAchieved",	"competitive/comp_background_tier005a" } );
			m_vecLevels.AddToTail( { 15,	m_vecLevels.Tail().m_nEndXP,		40500, "competitive/competitive_badge_rank015", "#TF_Competitive_Rank_15", "MatchMaking.RankThreeAchieved",	"competitive/comp_background_tier005a" } );
			m_vecLevels.AddToTail( { 16,	m_vecLevels.Tail().m_nEndXP,		43500, "competitive/competitive_badge_rank016", "#TF_Competitive_Rank_16", "MatchMaking.RankThreeAchieved",	"competitive/comp_background_tier006a" } );
			m_vecLevels.AddToTail( { 17,	m_vecLevels.Tail().m_nEndXP,		46500, "competitive/competitive_badge_rank017", "#TF_Competitive_Rank_17", "MatchMaking.RankThreeAchieved",	"competitive/comp_background_tier006a" } );
			// Bucket 4
			m_vecLevels.AddToTail( { 18,	m_vecLevels.Tail().m_nEndXP,		50000, "competitive/competitive_badge_rank018", "#TF_Competitive_Rank_18", "MatchMaking.RankFourAchieved",	"competitive/comp_background_tier006a" } );
		}

		const LevelInfo_t& GetLevelForExperience( uint32 nExperience ) const OVERRIDE
		{
			FixmeMMRatingBackendSwapping(); // Hard-coded drillo

			// The client may not have a rating yet, in which case they see 0 until they've been in a match. For level
			// purposes, return minimum.
			return IProgressionDesc::GetLevelForExperience( nExperience == 0 ? k_unDrilloRating_Ladder_Min : nExperience );
		}

#ifdef CLIENT_DLL
		virtual void SetupBadgePanel( CBaseModelPanel *pModelPanel, const LevelInfo_t& level ) const OVERRIDE
		{
			if ( !pModelPanel )
				return;

			int nLevelIndex = level.m_nLevelNum - 1;
			int nSkin = nLevelIndex;
			int nSkullsBodygroup	= ( nLevelIndex % 6 );
			int nSparkleBodygroup	= 0;
			if ( level.m_nLevelNum == 18 ) nSparkleBodygroup = 1;
			EnsureBadgePanelModel( pModelPanel );

			int nBody = 0;
			CStudioHdr studioHDR( pModelPanel->GetStudioHdr(), g_pMDLCache );

			::SetBodygroup( &studioHDR, nBody, ::FindBodygroupByName( &studioHDR, "skulls" ), nSkullsBodygroup );
			::SetBodygroup( &studioHDR, nBody, ::FindBodygroupByName( &studioHDR, "sparkle" ), nSparkleBodygroup );

			pModelPanel->SetBody( nBody );
			pModelPanel->SetSkin( nSkin );
		}

		virtual const uint32 GetLocalPlayerLastAckdExperience() const OVERRIDE
		{
			// This is bad and hard-coding a match group. We should just make XP a rating type and these functions
			// should just say "use this rating for XP"/"use this rating for acked XP"
			FixmeMMRatingBackendSwapping();
#if defined STAGING_ONLY
			if ( tf_test_pvp_rank_xp_change.GetInt() != -1 )
			{
				return tf_test_pvp_rank_xp_change.GetInt();
			}
#endif
			if ( !steamapicontext || !steamapicontext->SteamUser() )
			{
				return 0u;
			}

#ifndef CLIENT_DLL
			#error Make this call a yielding call if you are removing it from client ifdefs
#endif
			CTFRatingData *pRating = CTFRatingData::YieldingGetPlayerRatingDataBySteamID( steamapicontext->SteamUser()->GetSteamID(),
			                                                                              k_nMMRating_6v6_DRILLO_PlayerAcknowledged );
			return pRating ? pRating->GetRatingData().unRatingPrimary : 0u;
		}

		virtual const uint32 GetPlayerExperienceBySteamID( CSteamID steamid ) const OVERRIDE
		{
			// This is bad and hard-coding a match group. We should just make XP a rating type and these functions
			// should just say "use this rating for XP"/"use this rating for acked XP"
			FixmeMMRatingBackendSwapping();
#if defined CLIENT_DLL && defined STAGING_ONLY
			if ( tf_test_pvp_rank_xp_change.GetInt() != -1 )
			{
				return tf_test_pvp_rank_xp_change.GetInt();
			}
#endif

#ifndef CLIENT_DLL
			#error Make this call a yielding call if you are removing it from client ifdefs
#endif
			CTFRatingData *pRating = CTFRatingData::YieldingGetPlayerRatingDataBySteamID( steamapicontext->SteamUser()->GetSteamID(),
			                                                                              k_nMMRating_6v6_DRILLO );

			return pRating ? pRating->GetRatingData().unRatingPrimary : 0u;
		}
#endif // CLIENT_DLL

#if defined GC
		virtual bool BYldAcknowledgePlayerXPOnTransaction( CSQLAccess &transaction,
		                                                   CTFSharedObjectCache *pLockedSOCache ) const OVERRIDE
		{
			// This is bad and a result of XP being just a rating in some places but a magic field elsewhere.
			FixmeMMRatingBackendSwapping();

			MMRatingData_t ratingData = pLockedSOCache->GetPlayerRatingData( k_nMMRating_6v6_DRILLO );
			MMRatingData_t ackedRatingData = pLockedSOCache->GetPlayerRatingData( k_nMMRating_6v6_DRILLO_PlayerAcknowledged );
			if ( ratingData == ackedRatingData )
				{ return true; }

			// Feed it to acknowledged rating
			return pLockedSOCache->BYieldingUpdatePlayerRating( transaction,
			                                                    k_nMMRating_6v6_DRILLO_PlayerAcknowledged,
			                                                    k_nMMRatingSource_PlayerAcknowledge,
			                                                    0,
			                                                    ratingData );
		}

		virtual const bool BRankXPIsActuallyPrimaryMMRating() const OVERRIDE
		{
			// Gross hack due to XP being magically used differently in 6v6 right now.
			FixmeMMRatingBackendSwapping();
			return true;
		}
#endif // defined GC

#if defined GC_DLL || ( defined STAGING_ONLY && defined CLIENT_DLL )
		virtual void DebugSpewLevels() const OVERRIDE
		{
			Msg( "Spewing comp levels:\n" );
	
			// Walk the levels to find where the passed in experience value falls
			for( int i=0; i< m_vecLevels.Count(); ++i )
			{
				Msg( "Level %d:\t%d - %d. (+%d)\n", m_vecLevels[i].m_nLevelNum, m_vecLevels[i].m_nStartXP, m_vecLevels[i].m_nEndXP, ( m_vecLevels[i].m_nEndXP - m_vecLevels[i].m_nStartXP ) );
			}
		}
#endif
	};

	CLadderMatchGroupDescription( EMatchGroup eMatchGroup, ConVar* pmm_match_group_size )
		:	IMatchGroupDescription( eMatchGroup
								  , { eMatchMode_MatchMaker_LateJoinMatchBased // m_eLateJoinMode;
								  , eMMPenaltyPool_Ranked	   // m_ePenaltyPool
								  , true					   // m_bUsesSkillRatings;
								  , true					   // m_bSupportsLowPriorityQueue;
								  , true					   // m_bRequiresMatchID;
								  , LADDER_REQUIRED_SCORE	   // m_pmm_required_score;
								  , true					   // m_bUseMatchHud;
								  , "server_competitive.cfg"   // m_pszExecFileName;
								  , pmm_match_group_size	   // m_pmm_match_group_size;
								  , NULL					   // m_pmm_match_group_size_minimum;
								  , MATCH_TYPE_COMPETITIVE	   // m_eMatchType;
								  , true					   // m_bShowPreRoundDoors;
								  , true					   // m_bShowPostRoundDoors;
								  , "#TF_Competitive_GameOver" // m_pszMatchEndKickWarning;
								  , "MatchMaking.RoundStart"   // m_pszMatchStartSound;
								  , false					   // m_bAutoReady;
								  , true					   // m_bShowRankIcons;
								  , true					   // m_bUseMatchSummaryStage;
								  , true					   // m_bDistributePerformanceMedals;
								  , true					   // m_bIsCompetitiveMode;
								  , true					   // m_bUseFirstBlood;
								  , true					   // m_bUseReducedBonusTime;
								  , false					   // m_bUseAutoBalance;
								  , false					   // m_bAllowTeamChange;
								  , false					   // m_bRandomWeaponCrits;
								  , true					   // m_bFixedWeaponSpread;
								  , true					   // m_bRequireCompleteMatch;
								  , true					   // m_bTrustedServersOnly;
								  , true					   // m_bForceClientSettings;
								  , true					   // m_bAllowDrawingAtMatchSummary
								  , false					   // m_bAllowSpecModeChange
								  , false					   // m_bAutomaticallyRequeueAfterMatchEnds
								  , false					   // m_bUsesMapVoteOnRoundEnd
								  , false					   // m_bUsesXP
								  , true					   // m_bUsesDashboardOnRoundEnd
								  , true					   // m_bUsesSurveys
								  , true } )				   // m_bStrictMatchmakerScoring
	{
		m_pProgressionDesc = new CLadderProgressionDesc( eMatchGroup );
	}

#ifdef GC_DLL
	virtual EMMRating PrimaryMMRatingBackend() const OVERRIDE
	{
		// Right now all ladders have this hard-coded. Live-swapping won't work either, since lobbies don't know what
		// they were formed with in Match_Result
		FixmeMMRatingBackendSwapping();
		return k_nMMRating_6v6_DRILLO;
	}

	virtual const std::vector< EMMRating > &MatchResultRatingBackends() const OVERRIDE
	{
		FixmeMMRatingBackendSwapping(); // Shouldn't be hard-coded for 6v6, param
		static std::vector< EMMRating > ladderRatings = { k_nMMRating_6v6_DRILLO, k_nMMRating_6v6_GLICKO };
		return ladderRatings;
	}

	virtual bool InitMatchFromParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const OVERRIDE { return true; }
	virtual bool InitMatchFromLobby( MatchDescription_t* pMatch, CTFLobby* pLobby ) const OVERRIDE { return true; }
	virtual void SyncMatchParty( const CTFParty *pParty, MatchParty_t *pMatchParty ) const OVERRIDE {};
	virtual void SelectModeSpecificParameters( const MatchDescription_t* pMatch, CTFLobby* pLobby ) const OVERRIDE
	{
		// Forced to use the competitive category
		const SchemaGameCategory_t* pCategory = GetItemSchema()->GetGameCategory( kGameCategory_Competitive_6v6 );
		const char *pszMap = ( *tf_mm_ladder_force_map_by_name.GetString() ) ? tf_mm_ladder_force_map_by_name.GetString() : pCategory->GetRandomMap()->pszMapName;
		pLobby->SetMapName( pszMap );
	}

	virtual void GetServerDetails( const CMsgGameServerMatchmakingStatus& msg, int& nChallengeIndex, const char* pszMap ) const OVERRIDE
	{}

	virtual bool BThreadedPartiesCompatible( const MatchParty_t *pLeftParty, const MatchParty_t *pRightParty ) const OVERRIDE
	{
		return true;
	}

	virtual bool BThreadedPartyCompatibleWithMatch( const MatchDescription_t* pMatch, const MatchParty_t *pCurrentParty ) const OVERRIDE
	{
		// Right now there's no criteria on ladder matches, but leavers are never allowed to rejoin
		return BCheckLeftMatchMembersAgainstParty( pMatch, pCurrentParty,
		                                           [](const CTFMemcachedLobbyFormerMember& member) { return false; } );
	}

	virtual bool BThreadedIntersectMatchWithParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const OVERRIDE
	{
		// No action on match, just check that they're compatible
		return BThreadedPartyCompatibleWithMatch( pMatch, pParty );
	}

	virtual const char* GetUnauthorizedPartyReason( CTFParty* pParty ) const OVERRIDE
	{
		TFPartyManager()->YldUpdatePartyMemberData( pParty );
		if ( pParty->BAnyMemberWithoutCompetitiveAccess() )
		{
			return "They want to play a competitive game, but somebody doesn't have competitive access";
		}

		return NULL;
	}

	virtual void Dump( const char *pszLeader, int nSpewLevel, int nLogLevel, const MatchParty_t* pMatch ) const OVERRIDE
	{
		EmitInfo( SPEW_GC, nSpewLevel, nLogLevel, "%s  Best Ladder ping: %.0fms\n", pszLeader, pMatch->m_flPingClosestServer );
	}
#endif

#ifdef CLIENT_DLL
	virtual bool BGetRoundStartBannerParameters( int& nSkin, int& nBodyGroup ) const OVERRIDE
	{
		// The comp skins start at skin 8
		nSkin = 8 + TFGameRules()->GetRoundsPlayed();
		nBodyGroup = 1;
		return true;
	}

	virtual bool BGetRoundDoorParameters( int& nSkin, int& nLogoBodyGroup ) const OVERRIDE
	{
		nLogoBodyGroup = 0;
		nSkin = 0;
		if( GTFGCClientSystem()->GetLobby() &&
			GTFGCClientSystem()->GetLobby()->Obj().average_rank() >= k_unDrilloRating_Ladder_HighSkill )
		{
			// High skill has a different skin
			nSkin = 1;
		}

		return true;
	}
	virtual const char *GetMapLoadBackgroundOverride( bool bWidescreen ) const OVERRIDE
	{
		return ( bWidescreen ? "ranked_background_widescreen" : "ranked_background" );
	}
#endif

#ifdef GAME_DLL

	virtual void PostMatchClearServerSettings() const OVERRIDE
	{
		Assert( TFGameRules() );
		TFGameRules()->EndCompetitiveMatch();
	}

	virtual void InitGameRulesSettings() const OVERRIDE
	{
		TFGameRules()->SetCompetitiveMode( true );
		TFGameRules()->SetAllowBetweenRounds( true );
	}

	virtual void InitGameRulesSettingsPostEntity() const OVERRIDE
	{
		CTeamControlPointMaster *pMaster = ( g_hControlPointMasters.Count() ) ? g_hControlPointMasters[0] : NULL;
		bool bMultiStagePLR = ( tf_gamemode_payload.GetBool() && pMaster && pMaster->PlayingMiniRounds() && TFGameRules()->HasMultipleTrains() );
		bool bUseStopWatch = TFGameRules()->MatchmakingShouldUseStopwatchMode();
		bool bCTF = tf_gamemode_ctf.GetBool();
		bool bHighSkill = GTFGCClientSystem()->GetMatch() && GTFGCClientSystem()->GetMatch()->m_uAverageRank >= k_unDrilloRating_Ladder_HighSkill;

		// Exec our match settings
		const char *pszExecFile = ( bHighSkill ) ? "server_competitive_rounds_win_conditions_high_skill.cfg" : "server_competitive_rounds_win_conditions.cfg";
		
		if ( bUseStopWatch )
		{
			pszExecFile = ( bHighSkill ) ? "server_competitive_stopwatch_win_conditions_high_skill.cfg" : "server_competitive_stopwatch_win_conditions.cfg";
		}
		else if ( bMultiStagePLR || bCTF )
		{
			pszExecFile = ( bHighSkill ) ? "server_competitive_max_rounds_win_conditions_high_skill.cfg" : "server_competitive_max_rounds_win_conditions.cfg";
		}

		engine->ServerCommand( CFmtStr( "exec %s\n", pszExecFile ) );

		TFGameRules()->SetInStopWatch( bUseStopWatch );
		mp_tournament_stopwatch.SetValue( bUseStopWatch );
	}

	bool ShouldRequestLateJoin() const OVERRIDE
	{
		auto pTFGameRules = TFGameRules();
		if ( !pTFGameRules || !pTFGameRules->IsCompetitiveMode() || pTFGameRules->IsManagedMatchEnded() )
		{
			return false;
		}

		const CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();

		int nPlayers = pMatch->GetNumActiveMatchPlayers();
		int nMissingPlayers = pMatch->GetCanonicalMatchSize() - nPlayers;

		// Allow late-join if we're not started yet, have missing players, and have not lost everyone.
		return nMissingPlayers && nPlayers &&
				(	pTFGameRules->State_Get() == GR_STATE_BETWEEN_RNDS ||
					pTFGameRules->State_Get() == GR_STATE_PREGAME ||
					pTFGameRules->State_Get() == GR_STATE_STARTGAME );
	}

	bool BMatchIsSafeToLeaveForPlayer( const CMatchInfo* pMatchInfo, const CMatchInfo::PlayerMatchData_t *pMatchPlayer ) const OVERRIDE
	{
		// It's only safe if the match is over
		return pMatchInfo->BMatchTerminated();
	}

	virtual bool BPlayWinMusic( int nWinningTeam, bool bGameOver ) const OVERRIDE
	{
		if ( bGameOver )
		{
			TFGameRules()->BroadcastSound( 255, ( nWinningTeam == TF_TEAM_RED ) ? "Announcer.CompMatchWinRed" : "Announcer.CompMatchWinBlu"  );
			TFGameRules()->BroadcastSound( 255, ( nWinningTeam == TF_TEAM_RED ) ? "MatchMaking.MatchEndRedWinMusic" : "MatchMaking.MatchEndBlueWinMusic"  );
		}
		else
		{
			if ( nWinningTeam == TF_TEAM_RED )
			{
				TFGameRules()->BroadcastSound( 255, "Announcer.CompRoundWinRed" );
				TFGameRules()->BroadcastSound( 255, "MatchMaking.RoundEndRedWinMusic" );
			}
			else if ( nWinningTeam == TF_TEAM_BLUE )
			{
				TFGameRules()->BroadcastSound( 255, "Announcer.CompRoundWinBlu" );
				TFGameRules()->BroadcastSound( 255, "MatchMaking.RoundEndBlueWinMusic" );
			}
			else
			{
				TFGameRules()->BroadcastSound( 255, "Announcer.CompRoundStalemate" );
				TFGameRules()->BroadcastSound( 255, "MatchMaking.RoundEndStalemateMusic" );
			}
		}

		return true;
	}
#endif

};

class CCasualMatchGroupDescription : public IMatchGroupDescription
{
public:

	class CCasualProgressionDesc : public IProgressionDesc
	{
	public:

		CCasualProgressionDesc( EMatchGroup eMatchGroup )
			: IProgressionDesc( eMatchGroup
			                    , "models/vgui/12v12_badge.mdl"
			                    , "resource/ui/PvPCasualRankPanel.res"
			                    , "TF_Competitive_Level" )
			, m_nLevelsPerStep( 25 )
			, m_nSteps( 6 )
			, m_nAverageXPPerGame( 500 )
			, m_nAverageMinutesPerGame( 30 )
		{
			struct StepInfo_t
			{
				float m_flAvgGamerPerLevel;
				const char* m_pszLevelUpSound;
			};

			const StepInfo_t stepInfo[] = { { 1.5f, "MatchMaking.LevelOneAchieved" }
										  , { 2.5f, "MatchMaking.LevelTwoAchieved" }
										  , { 4.f, "MatchMaking.LevelThreeAchieved" }
										  , { 6.f, "MatchMaking.LevelFourAchieved" }
										  , { 9.f, "MatchMaking.LevelFiveAchieved" }
										  , { 14.f, "MatchMaking.LevelSixAchieved" } };

			uint32 nNumLevels = m_nLevelsPerStep * m_nSteps;

			uint32 nEndXPForLevel = 0;
			for( uint32 i=0; i<nNumLevels; ++i )
			{
				const uint32 nStep = i / m_nLevelsPerStep;
				LevelInfo_t& level = m_vecLevels[ m_vecLevels.AddToTail() ];
				level.m_nLevelNum = i + 1;	// This loop is 0-based, but users will start at level 1
				level.m_nStartXP = nEndXPForLevel; // Use the previous level's end as our start

				// We want the last level to have the same starting and ending XP value so the progress bar looks filled
				// as soon as you hit max level.
				if ( level.m_nLevelNum != nNumLevels )
				{
					nEndXPForLevel += stepInfo[ nStep ].m_flAvgGamerPerLevel * m_nAverageXPPerGame;
				}

				level.m_nEndXP = nEndXPForLevel;
				level.m_pszLevelTitle = NULL; // Casual levels dont have titles
				level.m_pszLevelUpSound = stepInfo[ nStep ].m_pszLevelUpSound;
				level.m_pszLobbyBackgroundImage = "competitive/12v12_background001"; // All the same in casual
			}
		}

#ifdef CLIENT_DLL
		virtual void SetupBadgePanel( CBaseModelPanel *pModelPanel, const LevelInfo_t& level ) const OVERRIDE
		{
			if ( !pModelPanel )
				return;

			int nLevelIndex = level.m_nLevelNum - 1;
			int nSkin = nLevelIndex / m_nLevelsPerStep;
			int nStarsBodyGroup	= ( ( nLevelIndex ) % 5 ) + 1;
			int nBulletsBodyGroup = 0;
			int nPlatesBodyGroup = 0;
			int nBannerBodyGroup = 0;

			switch( ( ( nLevelIndex ) / 5 ) % 5 )
			{
				case 0:
					nBulletsBodyGroup	= 0;
					nPlatesBodyGroup	= 0;
					nBannerBodyGroup	= 0;
					break;
				case 1:
					nBulletsBodyGroup	= 1;
					nPlatesBodyGroup	= 0;
					nBannerBodyGroup	= 0;
					break;
				case 2:
					nBulletsBodyGroup	= 2;
					nPlatesBodyGroup	= 1;
					nBannerBodyGroup	= 0;
					break;
				case 3:
					nBulletsBodyGroup	= 3;
					nPlatesBodyGroup	= 2;
					nBannerBodyGroup	= 1;
					break;
				case 4:
					nBulletsBodyGroup	= 4;
					nPlatesBodyGroup	= 3;
					nBannerBodyGroup	= 1;
					break;
			}

			EnsureBadgePanelModel( pModelPanel );

			int nBody = 0;
			CStudioHdr studioHDR( pModelPanel->GetStudioHdr(), g_pMDLCache );

			::SetBodygroup( &studioHDR, nBody, ::FindBodygroupByName( &studioHDR, "bullets" ), nBulletsBodyGroup );
			::SetBodygroup( &studioHDR, nBody, ::FindBodygroupByName( &studioHDR, "plates" ), nPlatesBodyGroup );
			::SetBodygroup( &studioHDR, nBody, ::FindBodygroupByName( &studioHDR, "banner" ), nBannerBodyGroup );
			::SetBodygroup( &studioHDR, nBody, ::FindBodygroupByName( &studioHDR, "stars" ), nStarsBodyGroup );

			pModelPanel->SetBody( nBody );
			pModelPanel->SetSkin( nSkin );
		}

		virtual const uint32 GetLocalPlayerLastAckdExperience() const OVERRIDE
		{
			// This is bad and hard-coding a match group. We should just make XP a rating type and these functions
			// should just say "use this rating for XP"/"use this rating for acked XP"
			FixmeMMRatingBackendSwapping();
#if defined CLIENT_DLL && defined STAGING_ONLY
			if ( tf_test_pvp_rank_xp_change.GetInt() != -1 )
			{
				return tf_test_pvp_rank_xp_change.GetInt();
			}
#endif

			CSOTFLadderData *pLadderData = GetLocalPlayerLadderData( k_nMatchGroup_Casual_12v12 );
			if ( pLadderData )
			{
				return pLadderData->Obj().last_ackd_experience();
			}

			return 0u;
		}

		virtual const uint32 GetPlayerExperienceBySteamID( CSteamID steamid ) const OVERRIDE
		{
			// This is bad and hard-coding a match group. We should just make XP a rating type and these functions
			// should just say "use this rating for XP"/"use this rating for acked XP"
			FixmeMMRatingBackendSwapping();
#if defined CLIENT_DLL && defined STAGING_ONLY
			if ( tf_test_pvp_rank_xp_change.GetInt() != -1 )
			{
				return tf_test_pvp_rank_xp_change.GetInt();
			}
#endif

#ifndef CLIENT_DLL
#error This function is only okay on the client due to calling yielding stuff
#endif
			CSOTFLadderData *pLadderData = YieldingGetPlayerLadderDataBySteamID( steamid, k_nMatchGroup_Casual_12v12 );
			if ( pLadderData )
			{
				return pLadderData->Obj().experience();
			}

			return 0u;
		}
#endif // CLIENT_DLL

#if defined GC
		virtual bool BYldAcknowledgePlayerXPOnTransaction( CSQLAccess &transaction,
		                                                   CTFSharedObjectCache *pLockedSOCache ) const OVERRIDE
		{
			// This is bad and a result of XP being just a rating in some places but a magic field elsewhere.
			FixmeMMRatingBackendSwapping();
			Assert( GGCTF()->IsSteamIDLockedByCurJob( pLockedSOCache->GetOwner() ) );

			CSOTFLadderData *pLadderData = NULL;

			// Find their ladder data
			CSharedObjectTypeCache *pItemTypeCache = pLockedSOCache->FindTypeCache( CSOTFLadderData::k_nTypeID );
			if ( pItemTypeCache )
			{
				CSOTFLadderData queryItem( pLockedSOCache->GetOwner().GetAccountID(), k_nMatchGroup_Casual_12v12 );
				pLadderData = pLockedSOCache->FindTypedSharedObject< CSOTFLadderData >( queryItem );
			}

			if ( !pLadderData )
			{
				return false;
			}

			// Update the last ack'd to the current
			//
			// XXX(JohnS): This function needs to be destroyed, but if kept, we would probably want to fix
			//             CSharedObjectTransactionEx to work properly with multiple SOCaches, such that
			//             BYieldingUpdatePlayerRating could work with it, such that this function could just require a
			//             sharedobject transaction...
			CSOTFLadderData dataCopy;
			dataCopy.Copy( *pLadderData );
			uint32_t nAck = pLadderData->Obj().experience();
			dataCopy.Obj().set_last_ackd_experience( nAck );
			CUtlVector<int> dirtyFields( 0, 1 );
			dirtyFields.AddToTail( CSOTFLadderPlayerStats::kLastAckdExperienceFieldNumber );
			bool bRet = dataCopy.BYieldingAddWriteToTransaction( transaction, dirtyFields );
			if ( bRet )
			{
				transaction.AddCommitListener( [pLockedSOCache, nAck, pLadderData]() {
					Assert( GGCTF()->IsSteamIDLockedByCurJob( pLockedSOCache->GetOwner() ) );
					pLadderData->Obj().set_last_ackd_experience( nAck );
					pLockedSOCache->DirtyNetworkObject( pLadderData );
				});
			}
			return bRet;
		}

		virtual const bool BRankXPIsActuallyPrimaryMMRating() const OVERRIDE { return false; }
#endif // defined GC

#if defined GC_DLL || ( defined STAGING_ONLY && defined CLIENT_DLL )
		virtual void DebugSpewLevels() const OVERRIDE
		{
			Msg( "Spewing casual levels:\n" );
			Msg( "Assuming average %d XP per game and average %d minutes per game\n", m_nAverageXPPerGame, m_nAverageMinutesPerGame );
	
			uint32 nNumGamesToAchieve = 0;
			// Walk the levels to find where the passed in experience value falls
			for( int i=0; i< m_vecLevels.Count(); ++i )
			{
				nNumGamesToAchieve += ( m_vecLevels[ i ].m_nEndXP - m_vecLevels[ i ].m_nStartXP ) / m_nAverageXPPerGame;
				uint32 nExpectedMinutesToAchieve = nNumGamesToAchieve * m_nAverageMinutesPerGame;
#ifdef CLIENT_DLL
				int nStep = i / m_nLevelsPerStep;
				const CEconItemRarityDefinition* pRarity = GetItemSchema()->GetRarityDefinition( nStep + 1 );
				vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
				vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( scheme );
				Color color = pScheme->GetColor( GetColorNameForAttribColor( pRarity->GetAttribColor() ), Color( 255, 255, 255, 255 ) );

				ConColorMsg( color, "Level %d:\t%d - %d.  Expected games required: %d  Expected hours required: %.2f\n", m_vecLevels[ i ].m_nLevelNum, m_vecLevels[ i ].m_nStartXP, m_vecLevels[ i ].m_nEndXP, nNumGamesToAchieve, ( nExpectedMinutesToAchieve / 60.f ) );
#else
				DevMsg( "Level %d:\t%d - %d.  Expected games required: %d  Expected hours required: %.2f\n", m_vecLevels[ i ].m_nLevelNum, m_vecLevels[ i ].m_nStartXP, m_vecLevels[ i ].m_nEndXP, nNumGamesToAchieve, ( nExpectedMinutesToAchieve / 60.f ) );
#endif
			}
		}
#endif

	private:
		const uint32 m_nLevelsPerStep;
		const uint32 m_nSteps;
		const uint32 m_nAverageXPPerGame;
		const uint32 m_nAverageMinutesPerGame;
	};

	CCasualMatchGroupDescription( EMatchGroup eMatchGroup, ConVar* pmm_match_group_size, ConVar* pmm_match_group_size_minimum )
		: IMatchGroupDescription( eMatchGroup
								  , { eMatchMode_MatchMaker_LateJoinMatchBased	   // m_eLateJoinMode;
								  , eMMPenaltyPool_Casual		   // m_ePenaltyPool
								  , true						   // m_bUsesSkillRatings;
								  , true						   // m_bSupportsLowPriorityQueue;
								  , true						   // m_bRequiresMatchID;
								  , CASUAL_REQUIRED_SCORE		   // m_pmm_required_score;
								  , true						   // m_bUseMatchHud;
								  , "server_casual.cfg"			   // m_pszExecFileName;
								  , pmm_match_group_size		   // m_pmm_match_group_size;
								  , pmm_match_group_size_minimum   // m_pmm_match_group_size_minimum;
								  , MATCH_TYPE_CASUAL			   // m_eMatchType;
								  , true						   // m_bShowPreRoundDoors;
								  , true						   // m_bShowPostRoundDoors;
								  , "#TF_Competitive_GameOver"	   // m_pszMatchEndKickWarning;
								  , "MatchMaking.RoundStartCasual" // m_pszMatchStartSound;
								  , true						   // m_bAutoReady;
								  , false						   // m_bShowRankIcons;
								  , false						   // m_bUseMatchSummaryStage;
								  , false						   // m_bDistributePerformanceMedals;
								  , true						   // m_bIsCompetitiveMode;
								  , false						   // m_bUseFirstBlood;
								  , false						   // m_bUseReducedBonusTime;
								  , true						   // m_bUseAutoBalance;
								  , false						   // m_bAllowTeamChange;
								  , true						   // m_bRandomWeaponCrits;
								  , false						   // m_bFixedWeaponSpread;
								  , false						   // m_bRequireCompleteMatch;
								  , true						   // m_bTrustedServersOnly;
								  , false						   // m_bForceClientSettings;
								  , false						   // m_bAllowDrawingAtMatchSummary
								  , true						   // m_bAllowSpecModeChange
								  , true						   // m_bAutomaticallyRequeueAfterMatchEnds
								  , true						   // m_bUsesMapVoteOnRoundEnd
								  , true						   // m_bUsesXP
								  , true						   // m_bUsesDashboardOnRoundEnd
								  , true						   // m_bUsesSurveys
								  , false } )					   // m_bStrictMatchmakerScoring
	{
		m_pProgressionDesc = new CCasualProgressionDesc( eMatchGroup );
	}

#ifdef GC_DLL
	virtual EMMRating PrimaryMMRatingBackend() const OVERRIDE
	{
		// Hard-coded for casual at the moment. Live-swapping won't work either, since lobbies don't know what they were
		// formed with in Match_Result
		FixmeMMRatingBackendSwapping();
		return k_nMMRating_12v12_DRILLO;
	}

	virtual const std::vector< EMMRating > &MatchResultRatingBackends() const OVERRIDE
	{
		FixmeMMRatingBackendSwapping(); // Shouldn't be hard-coded for 12v12, param
		static std::vector< EMMRating > casualRatings = { k_nMMRating_12v12_DRILLO, k_nMMRating_12v12_GLICKO };
		return casualRatings;
	}

	// Copy party's casual criteria
	virtual bool InitMatchFromParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const OVERRIDE
	{
		pMatch->m_acceptableCasualCriteria.Clear();
		pMatch->m_acceptableCasualCriteria.CopyFrom( pParty->m_casualCriteria );

		CCasualCriteriaHelper helper( pMatch->m_acceptableCasualCriteria );
		return helper.AnySelected();
	}

	virtual bool InitMatchFromLobby( MatchDescription_t* pMatch, CTFLobby* pLobby ) const OVERRIDE
	{
		pMatch->m_acceptableCasualCriteria.Clear();
		CCasualCriteriaHelper helper( pMatch->m_acceptableCasualCriteria );

		const MapDef_t* pMap = GetItemSchema()->GetMasterMapDefByName( pLobby->GetMapName() );
		if ( pMap )
		{
			helper.SetMapSelected( pMap->m_nDefIndex, true );
		}

		pMatch->m_acceptableCasualCriteria = helper.GetCasualCriteria();

		return helper.AnySelected();
	}

	// Set the match party's criteria to the TFParty's criteria
	virtual void SyncMatchParty( const CTFParty *pParty, MatchParty_t *pMatchParty ) const OVERRIDE
	{
		pMatchParty->m_casualCriteria.CopyFrom( pParty->Obj().search_casual() );
	}

	// Get all the valid categories and randomly choose a category to play
	virtual void SelectModeSpecificParameters( const MatchDescription_t* pMatch, CTFLobby* pLobby  ) const OVERRIDE
	{
		if ( *tf_mm_ladder_force_map_by_name.GetString() )
		{
			pLobby->SetMapName( tf_mm_ladder_force_map_by_name.GetString() );
			return;
		}

		CUtlVector< const MapDef_t* > vecValidMaps;
		int nNumMaps = GetItemSchema()->GetMasterMapsList().Count();
		CCasualCriteriaHelper helper( pMatch->m_acceptableCasualCriteria );
		for( int i=0; i < nNumMaps; ++ i )
		{
			const MapDef_t* pMapDef = GetItemSchema()->GetMasterMapsList()[ i ];

			if ( helper.IsMapSelected( pMapDef ) )
			{
				vecValidMaps.AddToTail( pMapDef );
			}
		}

		if ( vecValidMaps.Count() == 0 )
		{
			// We should have SOMETHING valid.
			Assert( vecValidMaps.Count() > 0 );
			pLobby->SetMapName( "ctf_2fort" ); // Everybody loves 2fort
			return; 
		}

		const char *pszMap = vecValidMaps[ RandomInt( 0, vecValidMaps.Count() - 1 ) ]->pszMapName;
		pLobby->SetMapName( pszMap );
	}

	// Private helper to check an entire party's join permissions and criteria, for both PartyCompatible and Intersect
	// below, since they do the same computation, one just keeps it.
	bool BThreadedCheckPartyAllowedToJoinMatchAndIntersectCriteria( const MatchDescription_t* pMatch,
	                                                                const MatchParty_t *pParty,
	                                                                CCasualCriteriaHelper &criteria ) const
	{
		if ( *tf_mm_ladder_force_map_by_name.GetString() )
		{
			// If a map is forced, we don't care about compatibility.  We're obviously testing something.
			return true;
		}

		// Check for blacklisted former members that tank compatibility
		if ( !BCheckLeftMatchMembersAgainstParty( pMatch, pParty, &BThreadedFormerMemberMayRejoinJoinCasualOrMvMMatch) )
			{ return false; }

		// Intersect criteria with new party
		criteria = CCasualCriteriaHelper( pMatch->m_acceptableCasualCriteria );
		criteria.Intersect( pParty->m_casualCriteria );

		return criteria.AnySelected();
	}

	virtual bool BThreadedPartyCompatibleWithMatch( const MatchDescription_t* pMatch, const MatchParty_t *pCandidateParty ) const OVERRIDE
	{
		CCasualCriteriaHelper helper( pMatch->m_acceptableCasualCriteria );
		return BThreadedCheckPartyAllowedToJoinMatchAndIntersectCriteria( pMatch, pCandidateParty, helper );
	}

	virtual bool BThreadedIntersectMatchWithParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const OVERRIDE
	{
		CCasualCriteriaHelper helper( pMatch->m_acceptableCasualCriteria );
		bool bRet = BThreadedCheckPartyAllowedToJoinMatchAndIntersectCriteria( pMatch, pParty, helper );

		// Update the match
		if ( bRet )
		{
			pMatch->m_acceptableCasualCriteria = helper.GetCasualCriteria();
			return true;
		}

		return false;

	}

	virtual bool BThreadedPartiesCompatible( const MatchParty_t *pLeftParty, const MatchParty_t *pRightParty ) const OVERRIDE
	{
		CCasualCriteriaHelper helper( pLeftParty->m_casualCriteria );
		helper.Intersect( pRightParty->m_casualCriteria );
		return helper.AnySelected();
	}

	virtual void GetServerDetails( const CMsgGameServerMatchmakingStatus& msg, int& nChallengeIndex, const char* pszMap ) const OVERRIDE
	{}

	virtual const char* GetUnauthorizedPartyReason( CTFParty* pParty ) const OVERRIDE
	{
		// Don't need no credit card to ride this train
		return NULL;
	}

	virtual void Dump( const char *pszLeader, int nSpewLevel, int nLogLevel, const MatchParty_t* pMatch ) const OVERRIDE
	{
		CUtlString strSelectedMaps;
		
		CUtlVector< const MapDef_t* > vecValidMaps;
		int nCount = 0;
		int nNumMaps = GetItemSchema()->GetMasterMapsList().Count();
		CCasualCriteriaHelper helper( pMatch->m_casualCriteria );
		for( int i=0; i < nNumMaps; ++ i )
		{
			const MapDef_t* pMapDef = GetItemSchema()->GetMasterMapsList()[ i ];
			if ( helper.IsMapSelected( pMapDef ) )
			{
				if ( nCount > 0 )
				{
					strSelectedMaps += ", ";
				}

				strSelectedMaps += pMapDef->pszMapName;
				++nCount;
			}
		}

		EmitInfo( SPEW_GC, nSpewLevel, nLogLevel, "%s  Search casual maps: %s\n", pszLeader, strSelectedMaps.String() );
		EmitInfo( SPEW_GC, nSpewLevel, nLogLevel, "%s  Best casual server ping: %.0fms\n", pszLeader, pMatch->m_flPingClosestServer );
	}
#endif

#ifdef CLIENT_DLL
	virtual bool BGetRoundStartBannerParameters( int& nSkin, int& nBodyGroup ) const OVERRIDE
	{
		nBodyGroup = 0;
		nSkin = TFGameRules()->GetRoundsPlayed();
		return true;
	}

	virtual bool BGetRoundDoorParameters( int& nSkin, int& nLogoBodyGroup ) const OVERRIDE
	{
		nLogoBodyGroup = 1;
		nSkin = 3;
		return true;
	}

	virtual const char *GetMapLoadBackgroundOverride( bool bWidescreen ) const OVERRIDE
	{
		return NULL;
	}
#endif

#ifdef GAME_DLL
	virtual void PostMatchClearServerSettings() const OVERRIDE
	{
		Assert( TFGameRules() );
		TFGameRules()->MatchSummaryEnd();
	}

	virtual void InitGameRulesSettings() const OVERRIDE
	{
		TFGameRules()->SetAllowBetweenRounds( true );
	}

	virtual void InitGameRulesSettingsPostEntity() const OVERRIDE
	{
		CTeamControlPointMaster *pMaster = ( g_hControlPointMasters.Count() ) ? g_hControlPointMasters[0] : NULL;
		bool bMultiStagePLR = ( tf_gamemode_payload.GetBool() && pMaster && pMaster->PlayingMiniRounds() && 
								pMaster->GetNumRounds() > 1 && TFGameRules()->HasMultipleTrains() );
		bool bCTF = tf_gamemode_ctf.GetBool();
		bool bUseStopWatch = TFGameRules()->MatchmakingShouldUseStopwatchMode();

		// Exec our match settings
		const char *pszExecFile = bUseStopWatch ? "server_casual_stopwatch_win_conditions.cfg" : 
								  ( ( bMultiStagePLR || bCTF ) ? "server_casual_max_rounds_win_conditions.cfg" : "server_casual_rounds_win_conditions.cfg" );

		if ( TFGameRules()->IsPowerupMode() )
		{
			pszExecFile = "server_casual_max_rounds_win_conditions_mannpower.cfg";
		}

		engine->ServerCommand( CFmtStr( "exec %s\n", pszExecFile ) );

		// leave stopwatch off for now
		TFGameRules()->SetInStopWatch( false );//bUseStopWatch );
		mp_tournament_stopwatch.SetValue( false );//bUseStopWatch );
	}

	bool ShouldRequestLateJoin() const OVERRIDE
	{
		auto pTFGameRules = TFGameRules();
		if ( !pTFGameRules || !pTFGameRules->IsCompetitiveMode() || pTFGameRules->IsManagedMatchEnded() )
		{
			Assert( false );
			return false;
		}

		if ( pTFGameRules->BIsManagedMatchEndImminent() )
			return false;

		const CMatchInfo *pMatch = GTFGCClientSystem()->GetMatch();

		int nPlayers = pMatch->GetNumActiveMatchPlayers();
		int nMissingPlayers = pMatch->GetCanonicalMatchSize() - nPlayers;

		// Allow late-join if we have missing players, have not lost everyone
		return nMissingPlayers && nPlayers;
	}

	bool BMatchIsSafeToLeaveForPlayer( const CMatchInfo* pMatchInfo, const CMatchInfo::PlayerMatchData_t *pMatchPlayer ) const
	{
		return true;
	}

	virtual bool BPlayWinMusic( int nWinningTeam, bool bGameOver ) const OVERRIDE
	{
		// Custom for game over
		if ( bGameOver )
		{
			TFGameRules()->BroadcastSound( TF_TEAM_RED, nWinningTeam == TF_TEAM_RED ? "MatchMaking.MatchEndWinMusicCasual" : "MatchMaking.MatchEndLoseMusicCasual" );
			TFGameRules()->BroadcastSound( TF_TEAM_BLUE, nWinningTeam == TF_TEAM_BLUE ? "MatchMaking.MatchEndWinMusicCasual" : "MatchMaking.MatchEndLoseMusicCasual" );
			return true;
		}
		else
		{
			// Let non-match logic handle round wins
			return false;
		}
	}
#endif
};

#if defined STAGING_ONLY && defined CLIENT_DLL
CON_COMMAND( spew_match_group_levels, "Spew all casual levels" )
{
	if ( args.ArgC() < 2 )
		return;

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( (EMatchGroup)atoi( args[1] ) );
	if ( !pMatchDesc || !pMatchDesc->m_pProgressionDesc )
		return;

	pMatchDesc->m_pProgressionDesc->DebugSpewLevels();
}
#endif

const IMatchGroupDescription* GetMatchGroupDescription( const EMatchGroup& eGroup )
{
	static CMvMMatchGroupDescription descBootcamp		( k_nMatchGroup_MvM_Practice, "server_bootcamp.cfg", /* bTrustedOnly */ false );
	static CMvMMatchGroupDescription descMannup			( k_nMatchGroup_MvM_MannUp,   "server_mannup.cfg", /* bTrustedOnly */ true );
	static CLadderMatchGroupDescription descLadder6v6	( k_nMatchGroup_Ladder_6v6,   &tf_mm_match_size_ladder_6v6 );
	static CLadderMatchGroupDescription descLadder9v9	( k_nMatchGroup_Ladder_9v9,   &tf_mm_match_size_ladder_9v9 );
	static CLadderMatchGroupDescription descLadder12v12	( k_nMatchGroup_Ladder_12v12, &tf_mm_match_size_ladder_12v12 );
	static CCasualMatchGroupDescription descCasual6v6	( k_nMatchGroup_Casual_6v6,   &tf_mm_match_size_ladder_6v6, NULL );
	static CCasualMatchGroupDescription descCasual9v9	( k_nMatchGroup_Casual_9v9,   &tf_mm_match_size_ladder_9v9, NULL );
	static CCasualMatchGroupDescription descCasual12v12	( k_nMatchGroup_Casual_12v12, &tf_mm_match_size_ladder_12v12, \
	                                                                                  &tf_mm_match_size_ladder_12v12_minimum );

	switch( eGroup )
	{
	case k_nMatchGroup_MvM_Practice:
		return &descBootcamp;
	case k_nMatchGroup_MvM_MannUp:
		return &descMannup;

	case k_nMatchGroup_Ladder_6v6:
		return &descLadder6v6;
	case k_nMatchGroup_Ladder_9v9:
		return &descLadder9v9;
	case k_nMatchGroup_Ladder_12v12:
		return &descLadder12v12;

	case k_nMatchGroup_Casual_6v6:
		return &descCasual6v6;
	case k_nMatchGroup_Casual_9v9:
		return &descCasual9v9;
	case k_nMatchGroup_Casual_12v12:
		return &descCasual12v12;

	default:
#ifdef GC_DLL
		// We're going to expectedly hit this many times on the client/server.
		// Only complain on the GC
		Assert( false );
#endif
		break;
	}

	return NULL;
}
// If you add a matchmaking group, handle it in the above switch
COMPILE_TIME_ASSERT( k_nMatchGroup_Casual_12v12 == ( k_nMatchGroup_Count - 1 ) );
