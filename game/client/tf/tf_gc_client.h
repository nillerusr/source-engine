//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef _INCLUDED_TF_GC_CLIENT_H
#define _INCLUDED_TF_GC_CLIENT_H
#ifdef _WIN32
#pragma once
#endif

#if !defined( _X360 ) && !defined( NO_STEAM )
#include "steam/steam_api.h"
#endif

//#include "dota_gc_common.h"
#include "gcsdk/gcclientsdk.h"
//#include "dota_gamerules.h"
#include "tf_gcmessages.pb.h"
#include "../clientsteamcontext.h"
#include "../gc_clientsystem.h"
#include "GameEventListener.h"
#include "tf_quickplay_shared.h"
#include "confirm_dialog.h"
#include "econ_game_account_client.h"
#include "tf_matchmaking_shared.h"
#include "tf_match_join_handlers.h"
#include "netadr.h"

class CTFParty;
class CTFGSLobby;
class CMvMMissionSet;
class IMatchJoiningHandler;
//class CDOTAGameAccountClient;
//class CDOTABetaParticipation;

#if !defined( TF_GC_PING_DEBUG ) && ( defined( STAGING_ONLY ) || defined( _DEBUG ) )
	#define TF_GC_PING_DEBUG
#endif

namespace GCSDK
{
	typedef uint64 PlayerGroupID_t;
}

/// High level matchmaking UI flow.  This represents the state that we show to the player,
/// and might not reflect all underlying asynchronous operations.
enum EMatchmakingUIState
{
	eMatchmakingUIState_Inactive,		//< At the main menu or in a regular game.  No lobby exists
	eMatchmakingUIState_Chat,			//< Setting options, chatting, not in search queue
	eMatchmakingUIState_InQueue,		//< In matchmaking queue, awaiting to be matched with compatible players and a gameserver.  Game could start at any moment
	eMatchmakingUIState_Connecting,		//< Matched with other players and assigned a gameerver, trying to connect to a game server
	eMatchmakingUIState_InGame,			//< In a game
};

enum EAbandonGameStatus
{
	k_EAbandonGameStatus_Safe,					//< It's totally safe to leave
	k_EAbandonGameStatus_AbandonWithoutPenalty,	//< Leaving right now would be considered "abandoning", but there will be no penalty right now
	k_EAbandonGameStatus_AbandonWithPenalty,	//< Leaving right now would be considered "abandoning", and you will be penalized
};

class CLoalPlayerSOCacheListener;

class CSendCreateOrUpdatePartyMsgJob;

class CTFGCClientSystem : public CGCClientSystem, public GCSDK::ISharedObjectListener, public CGameEventListener
{
	friend class CTFMatchmakingPopup;
	friend class CLoalPlayerSOCacheListener;
	friend class CSendCreateOrUpdatePartyMsgJob;
	DECLARE_CLASS_GAMEROOT( CTFGCClientSystem, CGCClientSystem );
public:
	CTFGCClientSystem( void );
	~CTFGCClientSystem( void );

	// CAutoGameSystemPerFrame
	virtual bool Init() OVERRIDE;
	virtual void PostInit() OVERRIDE;
	virtual void LevelInitPreEntity() OVERRIDE;
	virtual void LevelShutdownPostEntity() OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void Update( float frametime ) OVERRIDE;

	// Force discard all current ping data, forcing it to be refreshed, and causing BHavePingData to be false until it
	// completes.
	//
	// Normally, the client think will idly refresh this data, so this is only valuable for debug or cases where we know
	// the network changed and our previous data is worse than no data.
	void InvalidatePingData();

	bool BHavePingData() { return m_rtLastPingFix > 0; }
	// If !BHavePingData() this will have no datacenters in it.
	CMsgGCDataCenterPing_Update GetPingData() { return m_msgCachedPingUpdate; }

	// ISharedObjectListener
	virtual void	SOCreated( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void	PreSOUpdate( const CSteamID & steamIDOwner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { /* do nothing */ }
	virtual void	SOUpdated( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void	PostSOUpdate( const CSteamID & steamIDOwner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { /* do nothing */ }
	virtual void	SODestroyed( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void	SOCacheSubscribed( const CSteamID & steamIDOwner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void	SOCacheUnsubscribed( const CSteamID & steamIDOwner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { m_pSOCache = NULL; }

	// IGameEventListener2
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	enum SOChangeType_t
	{
		SOChanged_Create,
		SOChanged_Update,
		SOChanged_Destroy
	};
	void SOChanged( const GCSDK::CSharedObject *pObject, SOChangeType_t changeType,  GCSDK::ESOCacheEvent eEvent );
//	uint32 GetWins() { return m_unWinCount; }
//	uint32 GetLosses() { return m_unLossCount; }

//	int GetHeroRecordCount() { return m_aHeroRecords.Count(); }
//	GCHeroRecord_t* GetHeroRecord( int nIndex ) { return &m_aHeroRecords[ nIndex ]; }
//	KeyValues* GetNewsKeys() { return m_pNewsKeys; }
//	KeyValues* GetNewsStory( uint64 unNewsID );
//	KeyValues* GetNewsStoryByIndex( int nNewsIndex );
//	void SetGetNewsTime( float flGetNewsTime ) { m_flGetNewsTime = flGetNewsTime; }

//	void GameRules_State_Enter( DOTA_GameState newState );

//	void SetCurrentMatchID( uint32 unMatchID ) { m_unCurrentMatchID = unMatchID; }
//	uint32 GetCurrentMatchID() { return m_unCurrentMatchID; }

	void OnGCUserSessionCreated() {  }
	bool HasGCUserSessionBeenCreated();

//	CDOTAGameAccountClient* GetGameAccountClient();
//	void DumpGameAccountClient();
//	CDOTABetaParticipation* GetBetaParticipation();
//	void DumpBetaParticipation();

	CTFParty* GetParty();
	void CreateNewParty();

	CTFGSLobby* GetLobby();
	void DumpParty();
	void DumpLobby();
	void DumpInvites();
	void DumpPing();

	/// Request to jump to a particular step
	void RequestSelectWizardStep( TF_Matchmaking_WizardStep eWizardStep );

	/// Fetch current high-level logical UI state
	EMatchmakingUIState GetMatchmakingUIState();

	/// Fetch current wizard step.
	TF_Matchmaking_WizardStep GetWizardStep() const { return m_eLocalWizardStep; }

	/// Activate matchmaking system.  Doesn't necessarily do any network activity
	void BeginMatchmaking( TF_MatchmakingMode mode );
	bool BAllowMatchMakingInGame( void ) const;
	/// Quit the current game and lobby, if any and go back to the main menu
	void EndMatchmaking( bool bSendAbandonLobby = false );
	bool BExitMatchmakingAfterDisconnect( void );

	/// Called to active the invite UI
	void RequestActivateInvite();

	void ConnectToServer( const char *connect );

//	void StopFindingMatch();
//	void StartWatchingGame( const CSteamID &gameServerSteamID );
//	void StartWatchingGame( const CSteamID &gameServerSteamID, const CSteamID &watchServerSteamID );
//	void CancelWatchGameRequest();

//	GCSDK::CGCClientSharedObjectCache	*GetSOCache() { return m_pSOCache; }

//	void SetAutoSpectateCheckTime( float flAutoSpectateCheckTime ) { m_flAutoSpectateCheckTime = flAutoSpectateCheckTime; }
//
//	// downloading files
//
//	struct CDownloadingFile
//	{
//		uint32 m_unFileID;
//		HTTPRequestHandle m_hRequestHandle;
//		CCallResult< CTFGCClientSystem, HTTPRequestCompleted_t > m_Callback;
//		char m_szLocalFilename[MAX_PATH];
//		char m_szRemoteURL[MAX_PATH];
//	};
//	CUtlVector<CDownloadingFile*> m_DownloadingFiles;
//
//	void DownloadFile( const char *pszRemoteURL, const char *pszLocalFilename, bool bForceDownload = false );
//	void OnDownloadCompleted( HTTPRequestCompleted_t *arg, bool bFailed );
//
//	void SetTodayMessages( CMsgDOTATodayMessages *pMessages ) { m_TodayMessages = *pMessages; }
//	CMsgDOTATodayMessages* GetTodayMessages() { return &m_TodayMessages; }
//	void StartWatchingGameResponse( const CMsgWatchGameResponse &response );

	//
	// Search criteria
	//
	TF_MatchmakingMode GetSearchMode();

	// What MvM challenges?
	void GetSearchChallenges( CMvMMissionSet &challenges );
	void SetSearchChallenges( const CMvMMissionSet &challenges );

	// Willing to join the game late?
	bool GetSearchJoinLate();
	void SetSearchJoinLate( bool bJoinLate );

	// Quickplay
	EGameCategory GetQuickplayGameType();
	void SetQuickplayGameType( EGameCategory type );

	// "Play for loot" - requires a ticket.  If the challenge is beaten, then the winners
	// get some loot
	bool GetSearchPlayForBraggingRights();
	void SetSearchPlayForBraggingRights( bool bPlayForBraggingRights );

#ifdef USE_MVM_TOUR
	int GetSearchMannUpTourIndex();
	void SetSearchMannUpTourIndex( int idxTour );
#endif // USE_MVM_TOUR

	// Casual matchmaking groups and categories
	void SelectCasualMap( uint32 nMapDefIndex, bool bSelected );
	bool IsCasualMapSelected( uint32 nMapDefIndex ) const;
	void ClearCasualSearchCriteria();
	void SaveCasualSearchCriteriaToDisk();
	void LoadCasualSearchCriteria();

	// Update custom MM ping setting from the convar
	void UpdateCustomPingTolerance();

	// Check if the local player is doubling down
	bool GetLocalPlayerSquadSurplus();
	void SetLocalPlayerSquadSurplus( bool bSquadSurplus );

	// Ladders
	uint32 GetLadderType();
	void SetLadderType( uint32 nType );

	// World status
	const CMsgTFWorldStatus &WorldStatus() const { return m_WorldStatus; }

	static const char *k_pszSteamLobbyKey_PartyID;

	/// Accept the invite, join the specified lobby
	void AcceptFriendInviteToJoinLobby( const CSteamID &steamIDLobby );

	/// Return true if we're the leader of the party.
	/// NOTE: Returns true if we don't have a party!
	bool BIsPartyLeader();

	bool BHasOutstandingMatchmakingPartyMessage() const;

	enum ELobbyMsgType
	{
		k_eLobbyMsg_UserChat,
		k_eLobbyMsg_SystemMsgFromLeader,
	};

	/// Chat (though the steam lobby)
	void SendSteamLobbyChat( ELobbyMsgType eType, const char *pszText );

	/// See if we've got a ticket
	static bool BLocalPlayerInventoryHasMvmTicket( void );
	static int GetLocalPlayerInventoryMvmTicketCount( void );

	/// See if we've got a double-down
	static bool BLocalPlayerInventoryHasSquadSurplusVoucher( void );
	static int GetLocalPlayerInventorySquadSurplusVoucherCount( void );

#ifdef USE_MVM_TOUR
	/// Get info about the local player's badge.  Returns false if we can't
	/// find his inventory or he doesn't own a badge
	static bool BGetLocalPlayerBadgeInfoForTour( int iTourIndex, uint32 *pnBadgeLevel, uint32 *pnCompletedChallenges );
#endif // USE_MVM_TOUR

	struct MatchMakerHealthData_t
	{
		float m_flRatio;
		Color m_colorBar;
		CUtlString m_strLocToken;
	};

	MatchMakerHealthData_t GetHealthBracketForRatio( float flRatio ) const;

	uint32 GetMostSearchedCount() const { return m_nMostSearchedMapCount; }
	MatchMakerHealthData_t GetOverallHealthDataForLocalCriteria() const;
	MatchMakerHealthData_t GetHealthDataForMap( uint32 nMapIndex ) const;
	void RequestMatchMakerStats() const;
	void SetMatchMakerStats( const CMsgGCMatchMakerStatsResponse newStats );
	const CMsgGCMatchMakerStatsResponse &GetMatchMakerStats() { return m_MatchMakerStats; }
	const CUtlDict< float > &GetDataCenterPopulationRatioDict( EMatchGroup eMatchGroup ) { return m_dictDataCenterPopulationRatio[ eMatchGroup ]; }

	void AcknowledgePendingXPSources( EMatchGroup eMatchGroup ) const;
	void AcknowledgeNotification( uint32 nAccountID, uint64 ulNotificationID ) const;

	void SetSurveyRequest( const CMsgGCSurveyRequest& msgSurveyRequest );
	const CMsgGCSurveyRequest& GetSurveyRequest() const { return m_msgSurveyRequest; }
	void SendSurveyResponse( int32 nResponse );
	void ClearSurveyRequest();

	/// Most recent matchmaking progress stats received
	CMsgMatchmakingProgress m_msgMatchmakingProgress;

	bool BConnectedToMatchServer( bool bLiveMatch );

	// !! Does NOT mean you're *in* this match. See Above.
	bool BHaveLiveMatch() const;
	EAbandonGameStatus GetAssignedMatchAbandonStatus();
	bool BUserWantsToBeInMatchmaking() const { return m_bUserWantsToBeInMatchmaking; }

	EMatchGroup GetLiveMatchGroup() const;

	// Helper that combines GetMatchAbandonStatus and BConnectedToMatch as this is usually what you're asking.
	EAbandonGameStatus GetCurrentServerAbandonStatus()
	{
		return BConnectedToMatchServer( true ) ? GetAssignedMatchAbandonStatus() : k_EAbandonGameStatus_Safe;
	}

	void RejoinLobby( bool bConfirmed );
	bool JoinMMMatch();

	void LeaveGameAndPrepareToJoinParty( GCSDK::PlayerGroupID_t nPartyID );
	bool BIsPhoneVerified( void );
	bool BIsPhoneIdentifying( void );
	bool BHasCompetitiveAccess( void );

	bool BIsIPRecentMatchServer( netadr_t ip ) { return m_vecMatchServerHistory.Find( ip ) != m_vecMatchServerHistory.InvalidIndex(); }

	void AddLocalPlayerSOListener( ISharedObjectListener* pListener, bool bImmedately = true );
	void RemoveLocalPlayerSOListener( ISharedObjectListener* pListener );

#ifdef TF_GC_PING_DEBUG
	void SetPingOverride( const char *pszDataCenter, uint32 nPing, CMsgGCDataCenterPing_Update_Status eStatus );
	void ClearPingOverrides();
#endif

protected:

	// CGCClientSystem
	virtual void PreInitGC() OVERRIDE;
	virtual void PostInitGC() OVERRIDE;

	virtual void ReceivedClientWelcome( const CMsgClientWelcome &msg ) OVERRIDE;

private:
	friend class CGCClientAcceptInviteResponse;
	friend class CGCWorldStatusBroadcast;
//	void CreateSourceTVProxy( uint32 source_tv_public_addr, uint32 source_tv_private_addr, uint32 source_tv_port );

	void PingThink();

	CMsgCreateOrUpdateParty *GetCreateOrUpdatePartyMsg();
	CSendCreateOrUpdatePartyMsgJob *m_pPendingCreateOrUpdatePartyMsg;
	float m_flSendPartyUpdateMessageTime;

	void SetWorldStatus( CMsgTFWorldStatus &status ) { m_WorldStatus = status; }

	CMsgGCMatchMakerStatsResponse m_MatchMakerStats;
	uint32 m_nMostSearchedMapCount;

	CMsgTFWorldStatus m_WorldStatus;

//	uint32 m_unCurrentMatchID;
	bool m_bRegisteredSharedObjects;
	bool m_bInittedGC;

	EMatchmakingUIState m_eMatchmakingUIState;

	/// The lobby we joined/created (presumably) for matchmaking purposes
	CSteamID m_steamIDLobby;

	/// The lobby we have accepted the invite for, but not yet joined.
	/// (We'll do it when there's a good opportunity)
	CSteamID m_steamIDLobbyInviteAccepted;

	enum EAcceptInviteStep
	{
		eAcceptInviteStep_None,
		eAcceptInviteStep_ReadyToJoinSteamLobby,
		eAcceptInviteStep_JoinSteamLobby,
		eAcceptInviteStep_GetLobbyMetadata,
		eAcceptInviteStep_JoinParty,
	};
	EAcceptInviteStep m_eAcceptInviteStep;

	/// Status of creating lobby.
	int m_eCreateLobbyStatus;

	/// Check if we're in a steam lobby, then leave it
	void LeaveSteamLobby();

	/// Should we active the invite UI at the next opportunity?
	bool m_bWantToActivateInviteUI;

	// The gameserver is authoritative on matches once we are assigned, so even if the lobby is lost or stale, these
	// control: Where our assigned match is, and if we consider ourselves absolved of it.
	CSteamID m_steamIDGCAssignedMatch;
	// So we can consider the match over, based on the gameserver telling us so (or us abandoning). Once the lobby state
	// via the GC agrees, SOChanged will clear.
	bool m_bAssignedMatchEnded;
	EMatchGroup m_eAssignedMatchGroup;
	uint64 m_uAssignedMatchID;
	// History of assigned matches so things like the server browser can reason about our connect history.
	CUtlVector< netadr_t > m_vecMatchServerHistory;

	// Set when m_steamIDAssignedServer changes for the next Update()
	bool m_bServerAssignmentChanged;

	// SDR ping system
	RTime32 m_rtLastPingFix;
	bool    m_bPendingPingRefresh;
	bool    m_bSentInitialPingFix;
	// Cached ping data message as of rtLastPingFix
	CMsgGCDataCenterPing_Update m_msgCachedPingUpdate;

#ifdef TF_GC_PING_DEBUG
	CMsgGCDataCenterPing_Update m_msgPingOverrides;
#endif

	// Asks user if they want to rejoin an existing lobby
	float m_flCheckForRejoinTime;						// Due to network race conditions, delay for a bit before we respond
	void RejoinActiveMatch( void );

//	float m_flGetNewsTime;
//	float m_flAutoSpectateCheckTime;

	GCSDK::CGCClientSharedObjectCache	*m_pSOCache;
//	uint32 m_unWinCount;
//	uint32 m_unLossCount;
//	int m_nSignOnState;

	enum EConnectState
	{
		eConnectState_Disconnected,
		eConnectState_ConnectingToMatchmade,
		eConnectState_ConnectedToMatchmade,
		eConnectState_NonmatchmadeServer,
	};
	EConnectState m_eConnectState;

	bool IsConnectStateDisconnected()
	{
		if ( BAllowMatchMakingInGame() )
		{
			return m_eConnectState != eConnectState_ConnectingToMatchmade &&
				   m_eConnectState != eConnectState_ConnectedToMatchmade;
		}

		return m_eConnectState == eConnectState_Disconnected;
	}
//	CUtlSortVector<GCHeroRecord_t, CGCHeroRecordLess> m_aHeroRecords;

//	KeyValues *m_pNewsKeys;
	bool m_bGCUserSessionCreated;
	bool m_bUserWantsToBeInMatchmaking;
	GCSDK::PlayerGroupID_t m_nPendingAutoJoinPartyID;

	// Are we connected, and to whom
	CSteamID m_steamIDCurrentServer;


//	CMsgDOTATodayMessages m_TodayMessages;
//
//	DOTAGameVersion m_GameVersion;

	void SendCreateOrUpdatePartyMsg( TF_Matchmaking_WizardStep eWizardStep );
	void SendExitMatchmaking( bool bExplicitAbandon );
	void FireGameEventLobbyUpdated();
	void FireGameEventPartyUpdated();

	CMsgMatchSearchCriteria m_msgLocalSearchCriteria;
	TF_Matchmaking_WizardStep m_eLocalWizardStep;
	bool m_bLocalSquadSurplus;
//	void CheckSendAdjustSearchCriteria();

	void AssertMakesSenseToReadSearchCriteria();
	bool BAllowMatchmakingSearch();
#ifdef USE_MVM_TOUR
	bool BInternalSetSearchMannUpTourIndex( int idxTour );
#endif // USE_MVM_TOUR
	bool BInternalSetSearchChallenges( const CMvMMissionSet &challenges );

	CCallback<CTFGCClientSystem, LobbyCreated_t, false> m_callbackSteamLobbyCreated;
	CCallback<CTFGCClientSystem, LobbyEnter_t, false> m_callbackSteamLobbyEnter;
	CCallback<CTFGCClientSystem, LobbyChatMsg_t, false> m_callbackSteamLobbyChatMsg;
	CCallback<CTFGCClientSystem, GameLobbyJoinRequested_t, false> m_callbackSteamGameLobbyJoinRequested;
	CCallback<CTFGCClientSystem, LobbyDataUpdate_t, false > m_callbackSteamLobbyDataUpdate;
	CCallback<CTFGCClientSystem, LobbyChatUpdate_t, false > m_callbackSteamLobbyChatUpdate;

	void OnSteamLobbyCreated( LobbyCreated_t *pInfo );
	void OnSteamLobbyEnter( LobbyEnter_t *pInfo );
	void OnSteamLobbyChatMsg( LobbyChatMsg_t *pInfo );
	void OnSteamGameLobbyJoinRequested( GameLobbyJoinRequested_t *pInfo );
	void OnSteamLobbyDataUpdate( LobbyDataUpdate_t *pInfo );
	void OnSteamLobbyChatUpdate( LobbyChatUpdate_t *pInfo );

	/// Check if we have a steam lobby.  If we have one (and it's not the wrong one!) then return true.
	/// Otherwise, initiate creation, if possible
	///
	/// Returns:
	/// -1 error
	/// 0 in progress
	/// 1 OK
	int CheckSteamLobbyCreated();

	/// Check if we need to associate the party and steam lobby with each other
	void CheckAssociatePartyAndSteamLobby();

	/// if we want to active the invite UI, and we're ready, then do it now!
	void CheckReadyToActivateInvite();

	/// Called when we fail to accept the invite
	void OnFailedToAcceptInvite();

	CUtlVector< ISharedObjectListener* > m_vecDelayedLocalPlayerSOListenersToAdd;

	CTFMatchMakingPopupPrompJoinHandler m_PromptJoinHandler;
	CTFImmediateAutoJoinHandler m_AutoJoinHandler;

	CMsgGCSurveyRequest m_msgSurveyRequest;

	CUtlDict< float >	m_dictDataCenterPopulationRatio[ k_nMatchGroup_Count ];
};

CTFGCClientSystem* GTFGCClientSystem();

#endif // _INCLUDED_TF_GC_CLIENT_H
