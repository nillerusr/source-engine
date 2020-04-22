//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Dota specific GC based party
//			
//=============================================================================

#ifndef DOTA_PARTY_H
#define DOTA_PARTY_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"
#include "party.h"
#include "tf_matchmaking_shared.h"

#ifdef GC
namespace GCSDK
{
	CWebAPIValues;
}
#endif

const int k_nTFPartyMaxSize = 6;

class CTFParty : public GCSDK::CProtoBufSharedObject<CSOTFParty, k_EProtoObjectTFParty>, public GCSDK::IParty
{
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CTFParty );
#endif
	typedef GCSDK::CProtoBufSharedObject<CSOTFParty, k_EProtoObjectTFParty> BaseClass;

public:
	CTFParty();
	virtual ~CTFParty();

	const static int k_nTypeID = k_EProtoObjectTFParty;

	virtual bool BShouldDeleteByCache() const { return false; }
	virtual GCSDK::PlayerGroupID_t GetGroupID() const { return Obj().party_id(); }

	// Parties are their own SharedObject for all involved
	virtual CSharedObject* GetSharedObjectForMember( const CSteamID &steamID ) OVERRIDE { return this; }
	// Ambiguous between ISharedObject and IPlayerGroup.
	virtual void Dump() const OVERRIDE { return BaseClass::Dump(); }

	virtual const CSteamID GetLeader() const;

	virtual int GetNumMembers() const { return Obj().member_ids_size(); }
	virtual const CSteamID GetMember( int i ) const;
	virtual int GetMemberIndexBySteamID( const CSteamID &steamID ) const;

	virtual int GetNumPendingInvites() const { return Obj().pending_invites_size(); }
	virtual const CSteamID GetPendingInvite( int i ) const;
	virtual int GetPendingInviteIndexBySteamID( const CSteamID &steamID ) const;

	RTime32 GetStartedMatchmakingTime() const;
	CSOTFParty_State GetState() const
	{
		CSOTFParty_State result = Obj().state();
		#ifndef GC
			// The client really never needs to see this state.  Let's
			// treat it as equivalent to the finding match state
			if ( result == CSOTFParty_State_AWAITING_RESERVATION_CONFIRMATION )
			{
				result = CSOTFParty_State_FINDING_MATCH;
			}
		#endif
		return result;
	}
	CSteamID GetSteamLobbyID() const { return CSteamID( Obj().steam_lobby_id() ); }
	int GetNumSearchingPlayers( int group );
	int GetMaxNumSearchingPlayers(); // Returns the group with the largest number of players searching

	virtual bool AllowInvites() const { return false; } // we are using Steam invites

	void SpewDebug();

	bool BAnyMemberWithoutTicket() const;
	bool BAnyMemberWithoutCompetitiveAccess() const;
	bool BAnyMemberWithLowPriority() const;

	inline bool GetSearchLateJoinOK() const { return Obj().search_late_join_ok(); }
	inline bool GetSearchPlayForBraggingRights() const { return Obj().search_play_for_bragging_rights(); }
	TF_MatchmakingMode GetMatchmakingMode() const { return Obj().matchmaking_mode(); }
	uint32 GetSearchQuickplayGameType() const { return Obj().search_quickplay_game_type(); }
	EMatchGroup GetMatchGroup() const;
	void GetSearchChallenges( CMvMMissionSet &challenges ) const;
	uint32 GetSearchLadderGameType() const { return Obj().search_ladder_game_type(); }

#ifdef USE_MVM_TOUR
	/// Return name of tour we are searching for.  Returns NULL if we are not manned up
	const char *GetSearchMannUpTourName() const;

	/// Return index of tour we are searching for.  Might return one of the k_iMvmTourIndex_xxx values
	int GetSearchMannUpTourIndex() const;
#endif // USE_MVM_TOUR

#ifdef GC
	virtual void SetGroupID( GCSDK::PlayerGroupID_t nGroupID );
	virtual void SetLeader( const CSteamID &steamID );
	virtual void AddMember( const CSteamID &steamID );
	virtual void RemoveMember( const CSteamID &steamID );
	//virtual void SetSearchKey( const char *key );
	void SetMatchmakingMode( TF_MatchmakingMode mode );
	void SetSearchChallenges( const ::google::protobuf::RepeatedPtrField< ::std::string> &challenges );
#ifdef USE_MVM_TOUR
	void SetSearchMannUpTourName( const char *pszTourName );
#endif // USE_MVM_TOUR
	void CheckRemoveInvalidSearchChallenges();
	void SetSearchQuickplayGameType( uint32 nType );
	void SetSearchPlayForBraggingRights( bool bPlayForBraggingRights );
	void SetSearchLadderGameType( uint32 nType );
	void SetCasualSearchCriteria( const CMsgCasualMatchmakingSearchCriteria& msg );
	void SetCustomPingTolerance( uint32 nPingTolerance );
	uint32 GetCustomPingTolerance() const { return Obj().custom_ping_tolerance(); }
	void SetState( CSOTFParty_State newState );
	//virtual void SetMatchingPlayers( uint32 unMatchingPlayers );
	//virtual void SetSearchFraction( float flFraction );
	virtual void SetLateJoinOK( bool bLateJoinOK );
	virtual void SetMemberUseSquadSurplus( int nMemberIndex, bool bUseVoucher );

	virtual void AddPendingInvite( const CSteamID &steamID );
	virtual void RemovePendingInvite( const CSteamID &steamID );

	void SetStartedMatchmakingTime( RTime32 time );
	
	void AssociatePartyWithLobby( CTFLobby* pLobby );
	void DissociatePartyFromLobby( CTFLobby* pLobby );
	CTFLobby* GetAssociatedLobby() const;
	void SetUIStateAndWizardStep( TF_Matchmaking_WizardStep eWizardStep );
	void SetNumSearchingPlayers( int group, uint32 nPlayers );
	void SetSteamLobbyID( const CSteamID &steamIDLobby );

	bool IsIntroModeEligible();
	bool IsProModeEligible();
	void UpdatePreventMatchmakingDate();					// sets prevent_match variables to match the party member blocked from matchmaking the longest

	void YldUpdateMemberData( int nMemberIndex );

	void SetWebAPIDebugValues( GCSDK::CWebAPIValues *pPartyObject );

protected:
	void SetStateInternal( CSOTFParty_State newState );
	void DirtyParty();
#endif

#ifdef CLIENT_DLL
	// We mark a party as offline when we dont get any response from the GC regarding
	// changes to the party while messing with the MM UI.  If we get an update for an
	// offline party and we no longer want to be in MM, then we leave matchmaking to 
	// match the user's intent.  If it comes back and we're still in the MM UI, it'll
	// just update like normal.
	void SetOffline( bool bOffLine ) { m_bOffLine = bOffLine; }
	bool BOffline() const { return m_bOffLine; }
	private:
	bool m_bOffLine;
#endif
};

class CTFPartyInvite : public GCSDK::CProtoBufSharedObject<CSOTFPartyInvite, k_EProtoObjectTFPartyInvite>, public GCSDK::IPlayerGroupInvite
{
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CTFPartyInvite );
#endif
	typedef GCSDK::CProtoBufSharedObject<CSOTFPartyInvite, k_EProtoObjectTFPartyInvite> BaseClass;

public:
	const static int k_nTypeID = k_EProtoObjectTFPartyInvite;

	virtual const CSteamID GetSenderID() const { return Obj().sender_id(); }
	virtual GCSDK::PlayerGroupID_t GetGroupID() const { return Obj().group_id(); }
	virtual const char* GetSenderName() const { return Obj().sender_name().c_str(); }

	virtual int GetNumMembers() const { return Obj().members_size(); }
	virtual const CSteamID GetMember( int i ) const;
	virtual const char* GetMemberName( int i ) const;
	virtual uint16 GetMemberAvatar( int i ) const;
	virtual GCSDK::CSharedObject* GetSharedObject() { return this; }

#ifdef GC
	virtual void YldInitFromPlayerGroup( GCSDK::IPlayerGroup *pPlayerGroup );

	// NOTE: These do not dirty fields
	virtual void SetSenderID( const CSteamID &steamID ) { Obj().set_sender_id( steamID.ConvertToUint64() ); }
	virtual void SetGroupID( GCSDK::PlayerGroupID_t nGroupID ) { Obj().set_group_id( nGroupID ); }
	virtual void SetSenderName( const char *szName ) { Obj().set_sender_name( szName ); }
	virtual void AddMember( const CSteamID &steamID, const char *szPersonaName, uint16 unAvatar );
#endif
};
#endif
