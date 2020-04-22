//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"
#include "gcsdk/gcsdk_auto.h"
#include "tf_party.h"
#include "rtime.h"
#include "gcsdk/accountdetails.h"
#include "tf_quickplay_shared.h"
#include "tf_match_description.h"

#ifdef GC
#include "tf_matchmaker.h"
#include "tf_lobbymanager.h"
#endif

using namespace GCSDK;

//---------------------------------------------------------------------------------
#ifdef GC
IMPLEMENT_CLASS_MEMPOOL( CTFParty, 1000, UTLMEMORYPOOL_GROW_SLOW );
IMPLEMENT_CLASS_MEMPOOL( CTFPartyInvite, 1000, UTLMEMORYPOOL_GROW_SLOW );

extern GCConVar tf_mm_abandon_penalty_ignore;
extern GCConVar tf_mm_abandon_penalty_low_priority_max;

// Keeping this because I'm not 100% sure I'm not going to break things removing this.
GCConVar tf_mm_force_competitive_access_update( "tf_mm_force_competitive_access_update", "0", 0, "If set, force a update of party member's competitive status every time we update the party. Expensive, and would only wallpaper issues with the competitive acccess system." );

#endif



//---------------------------------------------------------------------------------
CTFParty::CTFParty()
#ifdef CLIENT_DLL
	: m_bOffLine( false )
#endif
{
}

CTFParty::~CTFParty()
{
}


const CSteamID CTFParty::GetLeader() const
{
	CSteamID steamID( Obj().leader_id() );
	return steamID;
}

const CSteamID CTFParty::GetMember( int i ) const
{
	Assert( i >= 0 && i < Obj().member_ids_size() );
	if ( i < 0 || i >= Obj().member_ids_size() )
		return k_steamIDNil;

	return Obj().member_ids( i );
}

int CTFParty::GetMemberIndexBySteamID( const CSteamID &steamID ) const
{
	for ( int i = 0; i < Obj().member_ids_size(); i++ )
	{
		if ( Obj().member_ids( i ) == steamID.ConvertToUint64() )
			return i;
	}
	return -1;
}

const CSteamID CTFParty::GetPendingInvite( int i ) const
{
	Assert( i >= 0 && i < Obj().pending_invites_size() );
	if ( i < 0 || i >= Obj().pending_invites_size() )
		return k_steamIDNil;

	return Obj().pending_invites( i );
}

int CTFParty::GetPendingInviteIndexBySteamID( const CSteamID &steamID ) const
{
	for ( int i = 0; i < Obj().pending_invites_size(); i++ )
	{
		if ( Obj().pending_invites( i ) == steamID.ConvertToUint64() )
			return i;
	}
	return -1;
}

void CTFParty::SpewDebug()
{
	CRTime time( GetStartedMatchmakingTime() );
	CRTime now( CRTime::RTime32TimeCur() );
	char time_buf[k_RTimeRenderBufferSize];
	char now_buf[k_RTimeRenderBufferSize];

	Msg( "TFParty: ID:%016llx  %d member(s)  LeaderID: %s\n", GetGroupID(), GetNumMembers(), GetLeader().Render() );
	Msg( "  State: %d  Started matchmaking: %s (%d seconds ago, now is %s)\n", (int) GetState(), time.Render( time_buf ), CRTime::RTime32TimeCur() - GetStartedMatchmakingTime(), now.Render( now_buf ) );
	//Msg( "  GameMode: %u\n", Obj().game_mode() );
	for ( int i = 0; i < GetNumMembers(); i++ )
	{
		Msg( "  Member[%d] %s%s\n", i, GetMember( i ).Render(), GetMember( i ) == GetLeader() ? " [Leader]" : "" );
	}
	Dump();
}

#ifdef GC
void CTFParty::SetWebAPIDebugValues( CWebAPIValues *pPartyObject )
{
	pPartyObject->SetChildUInt64Value( "party_id", GetGroupID() );
	pPartyObject->SetChildStringValue( "match_group", GetMatchGroupName( GetMatchGroup() ) );
	pPartyObject->SetChildStringValue( "state", CSOTFParty_State_Name( GetState() ).c_str() );
	CWebAPIValues *pMemberArray = pPartyObject->CreateChildArray( "members", "member" );
	for ( int i = 0 ; i < GetNumMembers() ; ++i )
	{
		pMemberArray->AddChildObjectToArray()->SetUInt64Value( GetMember( i ).ConvertToUint64() );
	}
}
#endif

EMatchGroup CTFParty::GetMatchGroup() const
{
	switch ( GetMatchmakingMode() )
	{
		case TF_Matchmaking_MVM:
			return GetSearchPlayForBraggingRights() ? k_nMatchGroup_MvM_MannUp : k_nMatchGroup_MvM_Practice;

		case TF_Matchmaking_LADDER:
		{
			EMatchGroup result = (EMatchGroup)Obj().search_ladder_game_type();
			Assert( result >= k_nMatchGroup_Ladder_First && result <= k_nMatchGroup_Ladder_Last );
			return result;
		}
		case TF_Matchmaking_CASUAL:
			return k_nMatchGroup_Casual_12v12;
		case TF_Matchmaking_INVALID:
			return k_nMatchGroup_Invalid;
	}

	AssertMsg1( false, "Matchmaking mode %d not yet implemented", GetMatchmakingMode() );
	return k_nMatchGroup_Invalid;
}

void CTFParty::GetSearchChallenges( CMvMMissionSet &challenges ) const
{
	challenges.Clear();

	for ( int i = 0 ; i < Obj().search_mvm_missions_size() ; ++i )
	{
		int idxChallenge = GetItemSchema()->FindMvmMissionByName( Obj().search_mvm_missions( i ).c_str() );
		if ( idxChallenge >= 0 )
			challenges.SetMissionBySchemaIndex( idxChallenge, true );
	}
}

#ifdef USE_MVM_TOUR
const char *CTFParty::GetSearchMannUpTourName() const
{
	if ( !GetSearchPlayForBraggingRights() )
		return NULL;
	return Obj().search_mvm_mannup_tour().c_str();
}

int CTFParty::GetSearchMannUpTourIndex() const
{
	if ( !GetSearchPlayForBraggingRights() )
		return k_iMvmTourIndex_NotMannedUp;
	return GetItemSchema()->FindMvmTourByName( Obj().search_mvm_mannup_tour().c_str() );
}
#endif // USE_MVM_TOUR

bool CTFParty::BAnyMemberWithoutTicket() const
{
	Assert( Obj().members_size() == GetNumMembers() );
	for ( int i = 0 ; i < Obj().members_size() ; ++i )
	{
		if ( !Obj().members(i).owns_ticket() )
			return true;
	}

	return false;
}

bool CTFParty::BAnyMemberWithoutCompetitiveAccess() const
{
	Assert( Obj().members_size() == GetNumMembers() );
	for ( int i = 0; i < Obj().members_size(); ++i )
	{
		if ( !Obj().members( i ).competitive_access() )
			return true;
	}

	return false;
}

bool CTFParty::BAnyMemberWithLowPriority() const
{
	// Wretched hive of scum and villainy

	return Obj().matchmaking_low_priority_time() > CRTime::RTime32TimeCur();
}

RTime32 CTFParty::GetStartedMatchmakingTime() const
{
	return (RTime32) Obj().started_matchmaking_time();
}

int CTFParty::GetNumSearchingPlayers( int group )
{
	if ( Obj().searching_players_by_group_size() <= group )
	{
		return 0;
	}

	return Obj().searching_players_by_group( group );
}

int CTFParty::GetMaxNumSearchingPlayers()
{
	if ( !Obj().searching_players_by_group_size() )
	{
		return 0;
	}

	uint32 n = 0;
	for ( int i = 0; i < Obj().searching_players_by_group_size(); ++i )
	{
		n = MAX( n, Obj().searching_players_by_group( i ) );
	}

	return n;
}

const CSteamID CTFPartyInvite::GetMember( int i ) const
{
	Assert( i >= 0 && i < Obj().members_size() );
	if ( i < 0 || i >= Obj().members_size() )
		return k_steamIDNil;

	return Obj().members( i ).steam_id();
}

const char* CTFPartyInvite::GetMemberName( int i ) const
{
	Assert( i >= 0 && i < Obj().members_size() );
	if ( i < 0 || i >= Obj().members_size() )
		return "Unknown";

	return Obj().members( i ).name().c_str();
}

uint16 CTFPartyInvite::GetMemberAvatar( int i ) const
{
	Assert( i >= 0 && i < Obj().members_size() );
	if ( i < 0 || i >= Obj().members_size() )
		return 0;

	return Obj().members( i ).avatar();
}

#ifdef GC
void CTFParty::AddMember( const CSteamID &steamID )
{
	Assert( Obj().members_size() == Obj().member_ids_size() );

	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx AddMember(%s)\n", Obj().party_id(), steamID.Render()) ;

	Obj().add_member_ids( steamID.ConvertToUint64() );
	Obj().add_members(); // leave all fields blank (default) for now.  We'll populate them in yielding calls later

	DirtyParty();

	UpdatePreventMatchmakingDate();
}

void CTFParty::RemoveMember( const CSteamID &steamID )
{
	Assert( Obj().members_size() == Obj().member_ids_size() );

	int index = GetMemberIndexBySteamID( steamID );
	if ( index == -1 )
		return;

	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx RemoveMember(%s)\n", Obj().party_id(), steamID.Render()) ;

	// protobuf doesn't let you remove elements from the middle of the list so we have to do this dance
	Obj().mutable_member_ids()->SwapElements( index, Obj().member_ids_size() - 1 );
	Obj().mutable_member_ids()->RemoveLast();

	Obj().mutable_members()->SwapElements( index, Obj().members_size() - 1 );
	Obj().mutable_members()->RemoveLast();

	DirtyParty();

	UpdatePreventMatchmakingDate();
}

void CTFParty::AddPendingInvite( const CSteamID &steamID )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx AddPendingInvite(%s)\n", Obj().party_id(), steamID.Render()) ;

	Obj().add_pending_invites( steamID.ConvertToUint64() );

	DirtyParty();
}

void CTFParty::RemovePendingInvite( const CSteamID &steamID )
{
	int index = GetPendingInviteIndexBySteamID( steamID );
	if ( index == -1 )
		return;

	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx RemovePendingInvite(%s)\n", Obj().party_id(), steamID.Render()) ;

	// protobuf doesn't let you remove elements from the middle of the list so we have to do this dance
	Obj().mutable_pending_invites()->SwapElements( index, Obj().pending_invites_size() - 1 );
	Obj().mutable_pending_invites()->RemoveLast();

	DirtyParty();
}

void CTFParty::DirtyParty()
{
	for ( int i = 0; i < GetNumMembers(); i++ )
	{
		CGCUserSession *pMemberSession = GGCBase()->FindUserSession( GetMember( i ) );
		// We may be in the process of mutating this party, don't assume we're in every member's SOCache or we hit
		// asserts all over debug during construction.
		//
		// Ideally, we'd have a better ScopedPartyOperation thing that only dirties a party once per transaction instead
		// of each mutation trying to re-set this flag.
		if ( !pMemberSession || !pMemberSession->GetSOCache()->FindSharedObject( *this ) )
			continue;

		pMemberSession->GetSOCache()->DirtyNetworkObject( this );
	}
}

void CTFParty::SetLeader( const CSteamID &steamID )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetLeader(%s)\n", Obj().party_id(), steamID.Render()) ;

	Obj().set_leader_id( steamID.ConvertToUint64() );

	DirtyParty();
}

void CTFParty::SetCustomPingTolerance( uint32 unCustomPingTolerance )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetCustomPingTolerance(%u)\n",
	          Obj().party_id(), unCustomPingTolerance );

	// TODO We need a template to do this dance for these setters :-/ A lot of them are just dirtying unnecessarily
	if ( !Obj().has_custom_ping_tolerance() || Obj().custom_ping_tolerance() != unCustomPingTolerance )
	{
		Obj().set_custom_ping_tolerance( unCustomPingTolerance );
		DirtyParty();
	}
}

void CTFParty::SetGroupID( GCSDK::PlayerGroupID_t nGroupID )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetGroupID(%016llx)\n", nGroupID, nGroupID );

	Obj().set_party_id( nGroupID );

	DirtyParty();
}

void CTFParty::SetStartedMatchmakingTime( RTime32 time )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetStartedMatchmakingTime(%u)\n", Obj().party_id(), time );

	Obj().set_started_matchmaking_time( (uint32) time );

	DirtyParty();
}

void CTFParty::SetState( CSOTFParty_State newState )
{
	// Setting a party in CSOTFParty_State_IN_MATCH should be done
	// through AssociatePartyWithLobby() down below
	Assert( newState != CSOTFParty_State_IN_MATCH );

	SetStateInternal( newState );
}

void CTFParty::SetStateInternal( CSOTFParty_State newState )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetState(%s)\n", Obj().party_id(), CSOTFParty_State_Name( newState ).c_str() );

	Obj().set_state( newState );

	// Make sure we have a valid wizard step, too
	if ( newState == CSOTFParty_State_UI )
	{
		if ( !Obj().has_wizard_step() || Obj().wizard_step() == TF_Matchmaking_WizardStep_SEARCHING )
		{
			switch ( GetMatchmakingMode() )
			{
				case TF_Matchmaking_MVM:
					Obj().set_wizard_step( TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
					break;

				case TF_Matchmaking_LADDER:
					Obj().set_wizard_step( TF_Matchmaking_WizardStep_LADDER );
					break;

				case TF_Matchmaking_CASUAL:
					Obj().set_wizard_step( TF_Matchmaking_WizardStep_CASUAL );
					break;

				default:
					AssertMsg2( false, "Party %016llx invalid matchmaking mode %d", Obj().party_id(), (int)GetMatchmakingMode() );
			}
		}
	}
	else
	{
		Obj().set_wizard_step( TF_Matchmaking_WizardStep_SEARCHING );
	}

	DirtyParty();
}

void CTFParty::AssociatePartyWithLobby( CTFLobby* pLobby )
{
	Assert( Obj().state() != CSOTFParty_State_IN_MATCH );
	Assert( Obj().associated_lobby_id() == 0 );

	Obj().set_associated_lobby_id( pLobby->GetGroupID() );
	SetStateInternal( CSOTFParty_State_IN_MATCH );

	DirtyParty();
}
void CTFParty::DissociatePartyFromLobby( CTFLobby* pLobby )
{
	Assert( Obj().state() == CSOTFParty_State_IN_MATCH );
	Assert( Obj().associated_lobby_id() == pLobby->GetGroupID() );

	Obj().set_associated_lobby_id( 0 );
	SetStateInternal( CSOTFParty_State_UI );

	DirtyParty();
}

CTFLobby* CTFParty::GetAssociatedLobby() const
{
	if ( Obj().associated_lobby_id() == 0 )
		return NULL;

	CTFLobby* pLobby = TFLobbyManager()->FindTFLobby( Obj().associated_lobby_id() );
	Assert( pLobby );
	return pLobby;
}

void CTFParty::SetUIStateAndWizardStep( TF_Matchmaking_WizardStep eWizardStep )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetUIStateAndWizardStep(%s)\n", Obj().party_id(), TF_Matchmaking_WizardStep_Name( eWizardStep ).c_str() );

	// Make sure the wizard step makes sense
	switch ( GetMatchmakingMode() )
	{
		default:
			AssertMsg2( false, "Party %016llx in invalid matchmaking mode %d", Obj().party_id(), (int)GetMatchmakingMode() );
		case TF_Matchmaking_MVM:
#ifdef USE_MVM_TOUR
			Assert(
				eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS
				|| eWizardStep == TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY
				|| eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE
			);
#else // new mm
			Assert(
				eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS
				|| eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE
			);
#endif // USE_MVM_TOUR
			break;
		case TF_Matchmaking_LADDER:
			Assert( eWizardStep == TF_Matchmaking_WizardStep_LADDER );
			break;

		case TF_Matchmaking_CASUAL:
			Assert( eWizardStep == TF_Matchmaking_WizardStep_CASUAL );
	}

	Obj().set_state( CSOTFParty_State_UI );
	Obj().set_wizard_step( eWizardStep );

	DirtyParty();
}

void CTFParty::SetSteamLobbyID( const CSteamID &steamIDLobby )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSteamLobbyID(%s)\n", Obj().party_id(), steamIDLobby.Render() );

	Obj().set_steam_lobby_id( steamIDLobby.ConvertToUint64() );

	DirtyParty();
}

void CTFParty::SetNumSearchingPlayers( int group, uint32 nPlayers )
{
	bool changed = false;

	while ( Obj().searching_players_by_group_size() < group )
	{
		Obj().add_searching_players_by_group( 0 );
		changed = true;
	}
	if ( Obj().searching_players_by_group_size() == group )
	{
		Obj().add_searching_players_by_group( nPlayers );
		changed = true;
	}
	else
	{
		if ( Obj().searching_players_by_group( group ) != nPlayers )
		{
			Obj().set_searching_players_by_group( group, nPlayers );
			changed = true;
		}
	}

	if ( changed )
	{
		//EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetNumSearchingPlayers(%u,%d)\n", Obj().party_id(), group, nPlayers );

		DirtyParty();
	}
}

//void CTFParty::SetSearchKey( const char *key )
//{
//	if ( key == NULL )
//		key = "";
//
//	// Note: case sensitivity
//	if ( Q_strcmp( Obj().search_key().c_str(), key ) == 0 )
//	{
//		// No change
//		return;
//	}
//
//	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSearchKey(%s)\n", Obj().party_id(), key ) ;
//	Obj().set_search_key( key );
//
//	DirtyParty();
//}

void CTFParty::SetMatchmakingMode( TF_MatchmakingMode mode )
{
	if ( mode != Obj().matchmaking_mode() )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetMatchmakingMode(\"%s\")\n", Obj().party_id(), TF_MatchmakingMode_Name( mode ).c_str() ) ;
		Obj().set_matchmaking_mode( mode );
		DirtyParty();
	}
}

void CTFParty::SetSearchChallenges( const ::google::protobuf::RepeatedPtrField< ::std::string> &challenges )
{
	Obj().mutable_search_mvm_missions()->Clear();
	Obj().mutable_search_mvm_missions()->MergeFrom( challenges );

	DirtyParty();
}

void CTFParty::CheckRemoveInvalidSearchChallenges()
{
	if ( GetMatchmakingMode() != TF_Matchmaking_MVM )
	{
		Obj().mutable_search_mvm_missions()->Clear();
		return;
	}

#ifdef USE_MVM_TOUR
	int idxTour = GetSearchMannUpTourIndex();
#endif // USE_MVM_TOUR

	bool bChanged = false;
	int i = 0;
	while ( i < Obj().search_mvm_missions_size() )
	{
		int idxChallenge = GetItemSchema()->FindMvmMissionByName( Obj().search_mvm_missions( i ).c_str() );
		bool bKeep = true;
		if ( idxChallenge >= 0 )
		{
#ifdef USE_MVM_TOUR
			if ( idxTour != k_iMvmTourIndex_NotMannedUp )
			{
				if ( idxTour < 0 )
				{
					bKeep = false;
				}
				else if ( GetItemSchema()->FindMvmMissionInTour( idxTour, idxChallenge ) < 0 )
				{
					bKeep = false;
				}
			}
#else // new mm
			// check if mission is valid if we're in mannup
			if ( GetSearchPlayForBraggingRights() )
			{
				const MvMMission_t& challenge = GetItemSchema()->GetMvmMissions()[ idxChallenge ];
				bKeep = challenge.m_unMannUpPoints > 0; // make sure mannup mission has valid point
			}
#endif // USE_MVM_TOUR
		}
		else if ( Obj().search_mvm_missions_size() == 1 )
		{
			// Only retain a single "invalid" entry to stand for "no challenges selected"
			Obj().set_search_mvm_missions( 0, "invalid" ); // make sure we don't have a real mission name hanging around that might later actually get used.
		}
		else
		{
			bKeep = false;
		}

		if ( bKeep )
		{
			++i;
		}
		else
		{
			// Remove it.  Note that the order of this list doesn't matter.
			Obj().mutable_search_mvm_missions()->SwapElements( i, Obj().search_mvm_missions_size()-1 );
			Obj().mutable_search_mvm_missions()->RemoveLast();
			bChanged = true;
		}
	}

	if ( bChanged )
	{
		DirtyParty();
	}
}

void CTFParty::SetSearchQuickplayGameType( uint32 nType )
{
	if ( Obj().search_quickplay_game_type() != nType )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSearchQuickplayGameType(%d)\n", Obj().party_id(), nType ) ;
		Obj().set_search_quickplay_game_type( nType );
		DirtyParty();
	}
}

void CTFParty::SetSearchLadderGameType( uint32 nType )
{
	if ( Obj().search_ladder_game_type() != nType )
	{
		if ( !IsLadderGroup( (EMatchGroup)nType ) )
		{
			AssertMsg( false, "Party %016llx attemptting to set invalid ladder search type %d\n", Obj().party_id(), nType );
		}
		else
		{
			EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSearchLadderGameType(%d)\n", Obj().party_id(), nType );
			Obj().set_search_ladder_game_type( nType );
			DirtyParty();
		}
	}
}

void CTFParty::SetCasualSearchCriteria( const CMsgCasualMatchmakingSearchCriteria& msg )
{
	Obj().mutable_search_casual()->CopyFrom( msg );
	DirtyParty();
}

//void CTFParty::SetMatchingPlayers( uint32 unMatchingPlayers )
//{
//	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetMatchingPlayers(%u)\n", Obj().party_id(), unMatchingPlayers ) ;
//
//	Obj().set_matching_players( unMatchingPlayers );
//
//	DirtyParty();
//}
//
//void CTFParty::SetSearchFraction( float flFraction )
//{
//	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSearchFraction(%f)\n", Obj().party_id(), flFraction ) ;
//
//	Obj().set_search_fraction( flFraction );
//
//	DirtyParty();
//}

void CTFParty::SetLateJoinOK( bool bLateJoinOK )
{
	if ( Obj().search_late_join_ok() != bLateJoinOK )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetLateJoinOK(%d)\n", Obj().party_id(), bLateJoinOK ? 1 : 0 ) ;
		Obj().set_search_late_join_ok( bLateJoinOK );
		DirtyParty();
	}
}

void CTFParty::SetSearchPlayForBraggingRights( bool bPlayForBraggingRights )
{
	if ( Obj().search_play_for_bragging_rights() != bPlayForBraggingRights )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSearchPlayForBraggingRights(%d)\n", Obj().party_id(), bPlayForBraggingRights ? 1 : 0 ) ;
		Obj().set_search_play_for_bragging_rights( bPlayForBraggingRights );

		// If not playing for bragging rights, then clear tour of duty
		if ( !bPlayForBraggingRights )
		{
			Obj().clear_search_mvm_mannup_tour();
		}

		DirtyParty();
	}
}

#ifdef USE_MVM_TOUR
void CTFParty::SetSearchMannUpTourName( const char *pszTourName )
{
	if ( pszTourName == NULL )
		pszTourName = "";
	if ( V_stricmp( Obj().search_mvm_mannup_tour().c_str(), pszTourName ) != 0 )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetSearchMannUpTourName(%s)\n", Obj().party_id(), pszTourName );
		Obj().set_search_mvm_mannup_tour( pszTourName );
		DirtyParty();
	}
}
#endif // USE_MVM_TOUR

void CTFParty::SetMemberUseSquadSurplus( int nMemberIndex, bool bUseVoucher )
{
	if ( nMemberIndex < 0 || nMemberIndex >= GetNumMembers() )
	{
		Assert( nMemberIndex >= 0 );
		Assert( nMemberIndex < GetNumMembers() );
		return;
	}

	if ( Obj().members( nMemberIndex ).squad_surplus() != bUseVoucher )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx SetMemberUseSquadSurplus(%d, %d)\n", Obj().party_id(), nMemberIndex, bUseVoucher ? 1 : 0 ) ;
		Obj().mutable_members( nMemberIndex )->set_squad_surplus( bUseVoucher );
		DirtyParty();
	}
}

void CTFParty::YldUpdateMemberData( int nMemberIndex )
{
	VPROF_BUDGET( "CTFParty::YldUpdateMemberData", VPROF_BUDGETGROUP_STEAM );

	Assert( Obj().members_size() == GetNumMembers() );
	if ( nMemberIndex < 0 || nMemberIndex >= GetNumMembers() )
	{
		Assert( nMemberIndex >= 0 );
		Assert( nMemberIndex < GetNumMembers() );
		return;
	}

	CSteamID steamIDMember( GetMember( nMemberIndex ) );
	CScopedSteamIDLock memberLock( steamIDMember );
	if ( !memberLock.BYieldingPerformLock( __FILE__, __LINE__) )
	{
		AssertMsg2( false, "CTFParty::YldUpdateMemberData: Failed to obtain lock for player %s in party %016llx",
		            steamIDMember.Render(), GetGroupID() );
		return;
	}

	CTFSharedObjectCache *pSOCache = GGCTF()->YieldingFindOrLoadTFSOCache( steamIDMember );
	if ( !pSOCache )
	{
		AssertMsg2( false, "Party %016llx contains member %s, but we cannot load his SO cache?", GetGroupID(), steamIDMember.Render() );
		return;
	}

	bool bCompetitiveAccess = false;
	CTFUserSession *pUserSession = GGCTF()->FindTFUserSession( steamIDMember );
	// Don't bother doing this relatively expensive call for non-ladder groups
	if ( pUserSession && IsLadderGroup( GetMatchGroup() ) )
	{
		// Competitive access updates are moderately expensive and queued asynchronously. If one is queued, force it to
		// flush now
		if ( !pUserSession->BCompetitiveAccessUpToDate() || tf_mm_force_competitive_access_update.GetBool() )
		{
			// Ensure competitive beta access is up to date
			pUserSession->YieldingUpdateCompetitiveAccess();
		}
		Assert( pUserSession->BCompetitiveAccessUpToDate() );
		bCompetitiveAccess = pSOCache->GetGameAccountClient() && pSOCache->GetGameAccountClient()->Obj().competitive_access();
	}
	else if ( !pUserSession )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "CTFParty::YldUpdateMemberData: Player %s is in party %016llx, but has no session",
		          steamIDMember.Render(), GetGroupID() );
		// Assume ineligible
	}

	bool bHasTicket = ( pSOCache->FindFirstMvmTicket() != NULL );
	bool bHasSquadSurplusVoucher = ( pSOCache->FindFirstMvmSquadSurplusVoucher() != NULL );

	bool bDirty = false;
	if ( Obj().members( nMemberIndex ).owns_ticket() != bHasTicket )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_owns_ticket(%d)\n", Obj().party_id(), steamIDMember.Render(), bHasTicket ? 1 : 0 ) ;
		Obj().mutable_members( nMemberIndex )->set_owns_ticket( bHasTicket );
		bDirty = true;
	}

	if ( Obj().members( nMemberIndex ).squad_surplus() && !bHasSquadSurplusVoucher )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_squad_surplus(0) --- no voucher in inventory\n", Obj().party_id(), steamIDMember.Render() ) ;
		Obj().mutable_members( nMemberIndex )->set_squad_surplus( false );
		bDirty = true;
	}

	if ( Obj().members( nMemberIndex ).competitive_access() != bCompetitiveAccess )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_competitive_access(0)\n", Obj().party_id(), steamIDMember.Render() );
		Obj().mutable_members( nMemberIndex )->set_competitive_access( bCompetitiveAccess );
		bDirty = true;
	}

#ifdef USE_MVM_TOUR
	int idxTour = GetSearchMannUpTourIndex();
	if ( idxTour >= 0 )
	{
		uint32 nBadgeLevel = 0;
		uint32 nMissionCompletedMask = 0;
		CEconItem *pBadge = pSOCache->FindMvmBadgeForTour( idxTour );
		if ( pBadge != NULL )
		{
			extern uint32 GetItemDescriptionDisplayLevel( const IEconItemInterface *pEconItem );

			nBadgeLevel = GetItemDescriptionDisplayLevel( pBadge );

			static CSchemaAttributeDefHandle pAttribDef_MvmChallengeCompleted( CTFItemSchema::k_rchMvMChallengeCompletedMaskAttribName );
			Assert( pAttribDef_MvmChallengeCompleted );
			if ( pAttribDef_MvmChallengeCompleted )
			{
				if ( !pBadge->FindAttribute( pAttribDef_MvmChallengeCompleted, &nMissionCompletedMask ) )
					nMissionCompletedMask = 0;
			}
		}

		if ( Obj().members( nMemberIndex ).completed_missions() != nMissionCompletedMask )
		{
			EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_completed_missions(0x%X)\n", Obj().party_id(), steamIDMember.Render(), nMissionCompletedMask ) ;
			Obj().mutable_members( nMemberIndex )->set_completed_missions( nMissionCompletedMask );
			bDirty  = true;
		}

		if ( Obj().members( nMemberIndex ).badge_level() != nBadgeLevel )
		{
			EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_badge_level(%d)\n", Obj().party_id(), steamIDMember.Render(), nBadgeLevel ) ;
			Obj().mutable_members( nMemberIndex )->set_badge_level( nBadgeLevel );
			bDirty = true;
		}
	}
	else
#endif // USE_MVM_TOUR
	{
		Obj().mutable_members( nMemberIndex )->clear_badge_level();
		Obj().mutable_members( nMemberIndex )->clear_completed_missions();
	}

	CSOTFLadderData *pData = pSOCache->GetLadderDataForMatchGroup( Obj().search_ladder_game_type() );
	uint32 unRank = ( pData ? pData->Obj().rank() : 1u );

	if ( Obj().members( nMemberIndex ).ladder_rank() != unRank )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_ladder_rank(%d)\n", Obj().party_id(), steamIDMember.Render(), unRank );
		Obj().mutable_members( nMemberIndex )->set_ladder_rank( unRank );
		bDirty = true;
	}

	uint32 unExperience = ( pData ? pData->Obj().experience() : 1u );
	if ( Obj().members( nMemberIndex ).experience() != unExperience )
	{
		EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_experience(%d)\n", Obj().party_id(), steamIDMember.Render(), unExperience );
		Obj().mutable_members( nMemberIndex )->set_experience( unExperience );
		bDirty = true;
	}

	if ( pUserSession )
	{
		uint32 unSkillRating = pUserSession->GetSkillRatingForMatchGroup( GetMatchGroup() );
		if ( unSkillRating != Obj().members( nMemberIndex ).skillrating() )
		{
			EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_skillrating(%d)\n", Obj().party_id(), steamIDMember.Render(), unSkillRating );
			Obj().mutable_members( nMemberIndex )->set_skillrating( unSkillRating );
			bDirty = true;
		}
	}

	if ( bDirty )
	{
		DirtyParty();
	}
}

bool CTFParty::IsIntroModeEligible()
{
	return true;

	/*for ( int m = 0; m < GetNumMembers(); m++ )
	{
		if ( Obj().member_initial_skills( m ) == DOTA_INITIAL_SKILL_NEWBIE ||
			Obj().member_initial_skills( m ) == DOTA_INITIAL_SKILL_FAMILIAR ||
			Obj().member_initial_skills( m ) == 0 )
		{
			return true;
		}
	}
	return false;*/
}

bool CTFParty::IsProModeEligible()
{
	return true;

	/*for ( int m = 0; m < GetNumMembers(); m++ )
	{
		if ( ( Obj().member_initial_skills( m ) == DOTA_INITIAL_SKILL_NEWBIE && Obj().member_wins( m ) > 0 ) ||
			Obj().member_initial_skills( m ) == DOTA_INITIAL_SKILL_FAMILIAR ||
			Obj().member_initial_skills( m ) == DOTA_INITIAL_SKILL_EXPERIENCED ||
			Obj().member_initial_skills( m ) == 0 )
		{
			return true;
		}
	}
	return false;*/
}

void CTFParty::UpdatePreventMatchmakingDate()
{
	uint32 unPreventDate = 0;
	uint32 unPreventAccountID = 0;
	uint32 unLowPriorityTime = 0;
	bool bDirty = false;

	EMMPenaltyPool ePenaltyPool = eMMPenaltyPool_Invalid;
	EMatchGroup matchGroup = GetMatchGroup();
	if ( matchGroup != k_nMatchGroup_Invalid )
	{
		const IMatchGroupDescription *pMatchDesc = GetMatchGroupDescription( matchGroup );
		Assert( pMatchDesc );
		ePenaltyPool = pMatchDesc ? pMatchDesc->m_params.m_ePenaltyPool : eMMPenaltyPool_Invalid;
	}

// !FIXME! tf_mvm_matchmaking
//	// search all party members to find the one banned for the longest
	for ( int i = 0; i < GetNumMembers(); i++ )
	{
		CEconSharedObjectCache *pSOCache = GGCEcon()->YieldingFindOrLoadEconSOCache( GetMember( i ) );
		if ( !pSOCache )
		{
			continue;
		}

		CEconGameAccountClient* pGameAccountClient = pSOCache->GetGameAccountClient();
		uint32 memberPreventDate = 0;
		uint32 memberLowPriorityTime = 0;
		bool bIsBanned = false;
		bool bIsLowPriority = false;
		if ( pGameAccountClient )
		{
			bool bIgnoreAbandoned = tf_mm_abandon_penalty_ignore.GetBool();
			switch ( ePenaltyPool )
			{
			case eMMPenaltyPool_Casual:
				memberPreventDate = ( bIgnoreAbandoned ) ? 0 : pGameAccountClient->Obj().matchmaking_casual_ban_expiration();
				memberLowPriorityTime = bIgnoreAbandoned ? 0 : pGameAccountClient->Obj().matchmaking_casual_low_priority_expiration();
				break;

			case eMMPenaltyPool_Ranked:
				memberPreventDate = ( bIgnoreAbandoned ) ? 0 : pGameAccountClient->Obj().matchmaking_ranked_ban_expiration();
				memberLowPriorityTime = bIgnoreAbandoned ? 0 : pGameAccountClient->Obj().matchmaking_ranked_low_priority_expiration();
				break;
			case eMMPenaltyPool_Invalid:
				// Not in any match group yet, no ban
				Assert( matchGroup == k_nMatchGroup_Invalid );
				break;

			default: Assert( false );
			}

			bIsBanned = memberPreventDate > CRTime::RTime32TimeCur();
			bIsLowPriority = memberLowPriorityTime > CRTime::RTime32TimeCur();

			// mark the member as banned. the ban stays until the party is destroyed
			if ( !Obj().members(i).is_banned() )
			{
				EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS,
				          "Party %016llx Member[%s].set_is_banned(%d)\n", Obj().party_id(), GetMember(i).Render(), bIsBanned ? 1 : 0 ) ;
				Obj().mutable_members(i)->set_is_banned( bIsBanned );
				bDirty = true;
			}

			if ( !Obj().members( i ).is_low_priority() )
			{
				EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "Party %016llx Member[%s].set_is_low_priority(%d)\n",
				          Obj().party_id(), GetMember( i ).Render(), bIsLowPriority ? 1 : 0 );
				Obj().mutable_members( i )->set_is_low_priority( bIsLowPriority );
				bDirty = true;
			}
		}

		if ( memberPreventDate > unPreventDate )
		{
			unPreventDate = memberPreventDate;
			unPreventAccountID = GetMember( i ).GetAccountID();
		}

		if ( memberLowPriorityTime > unLowPriorityTime )
		{
			unLowPriorityTime = memberLowPriorityTime;
		}
	}

	if ( unPreventDate != Obj().matchmaking_ban_time() )
	{
		Obj().set_matchmaking_ban_time( unPreventDate );
		bDirty = true;
	}

	if ( unPreventAccountID != Obj().matchmaking_ban_account_id() )
	{
		Obj().set_matchmaking_ban_account_id( unPreventAccountID );
		bDirty = true;
	}

	if ( unLowPriorityTime != Obj().matchmaking_low_priority_time() )
	{
		Obj().set_matchmaking_low_priority_time( unLowPriorityTime );
		bDirty = true;
	}

	if ( bDirty )
	{
		DirtyParty();
	}
}

void CTFPartyInvite::YldInitFromPlayerGroup( GCSDK::IPlayerGroup *pPlayerGroup )
{
	const char *szAccountName = GGCBase()->YieldingGetPersonaName( pPlayerGroup->GetLeader(), "Unknown Player" );

	SetGroupID( pPlayerGroup->GetGroupID() );
	SetSenderID( pPlayerGroup->GetLeader().ConvertToUint64() );
	SetSenderName( szAccountName );

	CTFParty *pParty = static_cast<CTFParty*>( pPlayerGroup );

	// Fill in party members and their avatars
	int nMembers = pParty->GetNumMembers();
	for ( int i = 0; i < nMembers; i++ )
	{
		const char *szPersonaName = GGCBase()->YieldingGetPersonaName( pParty->GetMember( i ), "Unknown Player" );

		uint16 unAvatar = 0;
// !FIXME! tf_mvm_matchmaking
//		CGCSharedObjectCache *pCache = GGCTF()->YieldingFindOrLoadSOCache( pParty->GetMember( i ) );
//		if ( pCache )
//		{
//			CTFGameAccountClient iSharedObject;
//			iSharedObject.Obj().set_account_id( pParty->GetMember( i ).GetAccountID() );
//			CTFGameAccountClient *pPlayerSummary = static_cast<CTFGameAccountClient*>( pCache->FindSharedObject( iSharedObject ) );
//			if ( pPlayerSummary )
//			{
//				unAvatar = (uint16) pPlayerSummary->GetAvatar();
//			}
//		}

		AddMember( pParty->GetMember( i ), szPersonaName, unAvatar );
	}
}

void CTFPartyInvite::AddMember( const CSteamID &steamID, const char *szPersonaName, uint16 unAvatar )
{
	EmitInfo( SPEW_GC, MATCHMAKING_SPEWLEVEL4, LOG_ALWAYS, "PartyInvite %016llx AddMember(%s, %s, %u)\n", GetGroupID(), steamID.Render(), szPersonaName ? szPersonaName : "", unAvatar );

	  
	CSOTFPartyInvite_PartyMember *pMember = Obj().add_members();
	pMember->set_steam_id( steamID.ConvertToUint64() );
	pMember->set_name( szPersonaName ? szPersonaName : "Unknown" );
	pMember->set_avatar( unAvatar );
}

#endif

