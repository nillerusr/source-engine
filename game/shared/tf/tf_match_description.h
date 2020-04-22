//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//			
//=============================================================================

#ifndef TF_MATCH_DESCRIPTION_H
#define TF_MATCH_DESCRIPTION_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_matchmaking_shared.h"
#ifdef CLIENT_DLL
#include "basemodel_panel.h"
#endif // CLIENT_DLL

#ifdef GAME_DLL
	// Can't foward declare CMatchInfo::PlayerMatchData_t because C++.  Bummer.
	#include "tf_gc_server.h"
#endif


#ifdef GC_DLL
	class CTFLobby;
	class CTFParty;
	struct MatchDescription_t;
	struct MatchParty_t;
#endif

class CSOTFLadderData;

enum EMatchType_t
{
	MATCH_TYPE_NONE = 0,
	MATCH_TYPE_MVM,
	MATCH_TYPE_COMPETITIVE,
	MATCH_TYPE_CASUAL
};

struct LevelInfo_t
{
	uint32 m_nLevelNum;
	uint32 m_nStartXP;	// Inclusive
	uint32 m_nEndXP;	// Non-inclusive
	const char* m_pszLevelIcon; // Kill this when we do models
	const char* m_pszLevelTitle;
	const char* m_pszLevelUpSound;
	const char* m_pszLobbyBackgroundImage;
};

struct XPSourceDef_t
{
	const char* m_pszSoundName;
	const char* m_pszFormattingLocToken;
	const char* m_pszTypeLocToken;
	float m_flValueMultiplier;
};

extern const XPSourceDef_t g_XPSourceDefs[ CMsgTFXPSource_XPSourceType_NUM_SOURCE_TYPES ];

class IProgressionDesc
{
public:
	IProgressionDesc( EMatchGroup eMatchGroup
					, const char* pszBadgeName
					, const char* pszProgressionResFile 
					, const char* pszLevelToken );

#ifdef CLIENT_DLL
	virtual void SetupBadgePanel( CBaseModelPanel *pModelPanel, const LevelInfo_t& level ) const = 0;
	virtual const uint32 GetLocalPlayerLastAckdExperience() const = 0;
	virtual const uint32 GetPlayerExperienceBySteamID( CSteamID steamid ) const = 0;
	virtual const LevelInfo_t& YieldingGetLevelForSteamID( const CSteamID& steamID ) const;
#endif // CLIENT_DLL
#if defined GC_DLL
	// XXX(JohnS): This should go away once XP is just a rating type, no need for match description to have different
	//             implementations of how the job does it.
	virtual bool BYldAcknowledgePlayerXPOnTransaction( CSQLAccess &transaction, CTFSharedObjectCache *pLockedSOCache ) const = 0;
	// XXX(JohnS): Same, this is super specific and hacky.
	virtual const bool BRankXPIsActuallyPrimaryMMRating() const = 0;
#endif // defined GC_DLL
	virtual const LevelInfo_t& GetLevelForExperience( uint32 nExperience ) const;
	const LevelInfo_t& GetLevelByNumber( uint32 nNumber ) const;
	uint32 GetNumLevels() const { return m_vecLevels.Count(); }

#if defined GC_DLL || ( defined STAGING_ONLY && defined CLIENT_DLL )
	virtual void DebugSpewLevels() const = 0;
#endif

	const CUtlString m_strBadgeName;
	const char* m_pszLevelToken;
	const char* m_pszProgressionResFile;

protected:
#ifdef CLIENT_DLL
	void EnsureBadgePanelModel( CBaseModelPanel *pModelPanel ) const;
#endif

	const EMatchGroup m_eMatchGroup;
	CUtlVector< LevelInfo_t > m_vecLevels;
};

struct MatchDesc_t
{
	EMatchMode		m_eLateJoinMode;
	EMMPenaltyPool  m_ePenaltyPool;
	bool			m_bUsesSkillRatings;
	bool			m_bSupportsLowPriorityQueue;
	bool			m_bRequiresMatchID;
	const ConVar*	m_pmm_required_score;
	bool			m_bUseMatchHud;
	const char*		m_pszExecFileName;
	const ConVar*	m_pmm_match_group_size;
	const ConVar*	m_pmm_match_group_size_minimum; // Optional
	EMatchType_t	m_eMatchType;
	bool			m_bShowPreRoundDoors;
	bool			m_bShowPostRoundDoors;
	const char*		m_pszMatchEndKickWarning;
	const char*		m_pszMatchStartSound;
	bool			m_bAutoReady;
	bool			m_bShowRankIcons;
	bool			m_bUseMatchSummaryStage;
	bool			m_bDistributePerformanceMedals;
	bool			m_bIsCompetitiveMode;
	bool			m_bUseFirstBlood;
	bool			m_bUseReducedBonusTime;
	bool			m_bUseAutoBalance;
	bool			m_bAllowTeamChange;
	bool			m_bRandomWeaponCrits;
	bool			m_bFixedWeaponSpread;
	// If we should not allow match to complete without a complete set of players.
	bool 			m_bRequireCompleteMatch;
	bool			m_bTrustedServersOnly;
	bool			m_bForceClientSettings;
	bool			m_bAllowDrawingAtMatchSummary;
	bool			m_bAllowSpecModeChange;
	bool			m_bAutomaticallyRequeueAfterMatchEnds;
	bool			m_bUsesMapVoteOnRoundEnd;
	bool			m_bUsesXP;
	bool			m_bUsesDashboardOnRoundEnd;
	bool			m_bUsesSurveys;
	// Be strict about finding quality matches, for more-competitive matchgroups that want to prioritize match quality
	// over speed.
	bool			m_bStrictMatchmakerScoring;
};

class IMatchGroupDescription
{
public:

	IMatchGroupDescription( EMatchGroup eMatchGroup, const MatchDesc_t& params )
		: m_eMatchGroup( eMatchGroup )
		, m_params( params )
		, m_pProgressionDesc( NULL )
	{}


#ifdef GC_DLL
	// What rating the matchmaker should use to evaluate players in this matchgroup
	virtual EMMRating PrimaryMMRatingBackend() const = 0;

	// What ratings match results in this ladder group should run updates on
	virtual const std::vector< EMMRating > &MatchResultRatingBackends() const = 0;

	//	When creating a match from the first party, what to copy over
	virtual bool InitMatchFromParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const = 0;

	//	When finding late joiners for an already-in-play lobby
	virtual bool InitMatchFromLobby( MatchDescription_t* pMatch, CTFLobby* pLobby ) const = 0;

	//	Sync a match party with a CTFParty
	virtual void SyncMatchParty( const CTFParty *pParty, MatchParty_t *pMatchParty ) const = 0;

	//	A match has formed, what game mode paremeters do we want to set? (ie. MvM Popfile, 12v12 map, etc)
	virtual void SelectModeSpecificParameters( const MatchDescription_t* pMatch, CTFLobby* pLobby ) const = 0;

	// Get which server pool to use
	virtual int GetServerPoolIndex( EMatchGroup eGroup, EMMServerMode eMode ) const;

	// Get server details
	virtual void GetServerDetails( const CMsgGameServerMatchmakingStatus& msg, int& nChallengeIndex, const char* pszMap ) const = 0;

	virtual const char* GetUnauthorizedPartyReason( CTFParty* pParty ) const = 0;

	virtual void Dump( const char *pszLeader, int nSpewLevel, int nLogLevel, const MatchParty_t* pMatch ) const = 0;

	//
	// Threaded calls. These are called in work items, and should be pure functions
	//

	// Check if pMatch is compatible with pCandidateParty -- that is, if BIntersectMatchWithParty would succeed.
	virtual bool BThreadedPartyCompatibleWithMatch( const MatchDescription_t* pMatch, const MatchParty_t *pCurrentParty ) const = 0;

	// When adding a party to a match, what intersection of current state with the incoming party gets copied to the
	// match.  This returns false if the party isn't compatible, e.g. if !BThreadedPartyCompatibleWithMatch
	virtual bool BThreadedIntersectMatchWithParty( MatchDescription_t* pMatch, const MatchParty_t* pParty ) const = 0;

	// Check if two parties are compatible, that is, if they could be added to the same match absent other criteria
	virtual bool BThreadedPartiesCompatible( const MatchParty_t *pLeftParty, const MatchParty_t *pRightParty ) const = 0;
#endif

#ifdef CLIENT_DLL
	virtual bool BGetRoundStartBannerParameters( int& nSkin, int& nBodyGroup ) const = 0;
	virtual bool BGetRoundDoorParameters( int& nSkin, int& nLogoBodyGroup ) const = 0;
	virtual const char *GetMapLoadBackgroundOverride( bool bWideScreen ) const = 0;
#endif

#ifdef GAME_DLL
	// ! Check return, we might fail to setup
	virtual bool InitServerSettingsForMatch( const CTFGSLobby* pLobby ) const;
	virtual void InitGameRulesSettings() const = 0;
	virtual void InitGameRulesSettingsPostEntity() const = 0;
	virtual void PostMatchClearServerSettings() const = 0;
	virtual bool ShouldRequestLateJoin() const = 0;
	virtual bool BMatchIsSafeToLeaveForPlayer( const CMatchInfo* pMatchInfo, const CMatchInfo::PlayerMatchData_t *pMatchPlayer ) const = 0;
	virtual bool BPlayWinMusic( int nWinningTeam, bool bGameOver ) const = 0;
#endif

	// Accessors for param values
	inline int GetMatchSize() const								{ return m_params.m_pmm_match_group_size->GetInt(); }
	inline bool BShouldAutomaticallyRequeueOnMatchEnd() const	{ return m_params.m_bAutomaticallyRequeueAfterMatchEnds; }
	inline bool BUsesMapVoteAfterMatchEnds() const				{ return m_params.m_bUsesMapVoteOnRoundEnd; }
	inline bool BUsesXP() const									{ return m_params.m_bUsesXP; }
	inline bool BUsesDashboard() const							{ return m_params.m_bUsesDashboardOnRoundEnd; }
	inline bool BUsesStrictMatchmakerScoring() const			{ return m_params.m_bStrictMatchmakerScoring; }
	inline bool BRequiresCompleteMatches() const				{ return m_params.m_bRequireCompleteMatch; }
	inline bool BRequiresMatchID() const						{ return m_params.m_bRequiresMatchID; }

	// Meta-permissions that are based on other set flags
	//
	// Only match-vote modes need this ability right now
	inline bool BCanServerRequestNewMatchForLobby() const		{ return BUsesMapVoteAfterMatchEnds(); }
	// Auto-balance and anything that is allowed to roll new match lobbies needs to have this ability (for speculative
	// matches if the GC is unavailable).  It should be possible to add a mode where we do rolling matches, but only
	// when the GC is responding, which would not need the unilateral-team-assignment ability
	inline bool BCanServerChangeMatchPlayerTeams() const		{ return BCanServerRequestNewMatchForLobby() || m_params.m_bUseAutoBalance; }

#ifdef GC_DLL
	inline bool BUsesSkillRatings() const { return m_params.m_bUsesSkillRatings; }
	inline int GetMinimumMatchSize() const
	{
		int min = m_params.m_pmm_match_group_size_minimum ? m_params.m_pmm_match_group_size_minimum->GetInt() : -1;
		return ( min >= 0 ) ? min : GetMatchSize();
	}
	inline bool BUsesSurveys() const { return m_params.m_bUsesSurveys; }
#endif

	inline bool BIsTrustedServersOnly() const { return m_params.m_bTrustedServersOnly; }

	const EMatchGroup m_eMatchGroup;
	const MatchDesc_t m_params;
	const IProgressionDesc* m_pProgressionDesc;
};

const IMatchGroupDescription* GetMatchGroupDescription( const EMatchGroup& eGroup );

#endif //TF_MATCH_DESCRIPTION_H
