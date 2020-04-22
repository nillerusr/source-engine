//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The lobby shared object for gameservers, managed by CTFLobby
//
//=============================================================================

#ifndef TF_LOBBY_SERVER_H
#define TF_LOBBY_SERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"
#include "tf_matchmaking_shared.h"
#include "playergroup.h"

class CTFGSLobby : public GCSDK::CProtoBufSharedObject<CSOTFGameServerLobby, k_EProtoObjectTFGameServerLobby>
{
	typedef GCSDK::CProtoBufSharedObject<CSOTFGameServerLobby, k_EProtoObjectTFGameServerLobby> BaseClass;
public:
	virtual ~CTFGSLobby() {}

	// Debug
	void SpewDebug();

	// Member helpers
	const CTFLobbyMember* GetMemberDetails( CSteamID steamID ) const;
	const CTFLobbyMember* GetMemberDetails( int i ) const;
	const CSteamID GetMember( int i ) const;
	int GetNumMembers() const { return Obj().members_size(); }
	CTFLobbyMember_ConnectState GetMemberConnectState( int iMemberIndex ) const;
	bool BAssertValidMemberIndex( int iMemberIndex ) const;

	// Inline helpers
	CSOTFGameServerLobby::State GetState() const { return Obj().state(); }
	const char *GetMissionName() const { return Obj().mission_name().c_str(); }
	EMatchGroup GetMatchGroup() const { return Obj().has_match_group() ? (EMatchGroup)Obj().match_group() : k_nMatchGroup_Invalid; }
	uint64 GetMatchID( void ) const { return Obj().match_id(); }
	uint32 GetFlags( void ) const { return Obj().flags(); }
	const char *GetMapName() const { return Obj().map_name().c_str(); }
	GCSDK::PlayerGroupID_t GetGroupID() const { return Obj().lobby_id(); }
	bool GetLateJoinEligible() const { return Obj().late_join_eligible(); }
	CSteamID GetServerID() const { return Obj().server_id(); }
	const char *GetConnect() const { return Obj().connect().c_str(); }
	uint32_t GetLobbyMMVersion() const { return Obj().lobby_mm_version(); }

#ifdef USE_MVM_TOUR
	// Returns name of tour that we are playing for.  Returns NULL if we are not playing for bragging rights!
	const char *GetMannUpTourName() const;
#endif // USE_MVM_TOUR

};

#endif // TF_LOBBY_SERVER_H
