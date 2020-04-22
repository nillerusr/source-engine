//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  A group of players stored on the GC.
//			 Implementation and networking via shared objects is done in game specific derived classes.
//			
//=============================================================================

#ifndef PLAYERGROUP_H
#define PLAYERGROUP_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"

namespace GCSDK
{
typedef uint64 PlayerGroupID_t;
class CSharedObject;
class IPlayerGroup;

class IPlayerGroupInvite
{
public:
	virtual ~IPlayerGroupInvite() { }

	virtual const CSteamID GetSenderID() const = 0;
	virtual PlayerGroupID_t GetGroupID() const = 0;
	virtual const char* GetSenderName() const = 0;

	virtual CSharedObject* GetSharedObject() = 0;

#ifdef GC
	virtual void SetSenderID( const CSteamID &steamID ) = 0;
	virtual void SetGroupID( PlayerGroupID_t nGroupID ) = 0;
	virtual void SetSenderName( const char *szName ) = 0;

	virtual void YldInitFromPlayerGroup( IPlayerGroup *pPlayerGroup ) = 0;
#endif
};

class IPlayerGroup
{
public:
	virtual ~IPlayerGroup() {}

	virtual PlayerGroupID_t GetGroupID() const = 0;

	virtual int GetNumMembers() const = 0;
	virtual const CSteamID GetMember( int i ) const = 0;
	virtual int GetMemberIndexBySteamID( const CSteamID &steamID ) const = 0;

	// Can be NULL if this player group should not put shared objects on members.
	// If this object exists, it should do so for the duration of the player's membership in the group.
	virtual CSharedObject* GetSharedObjectForMember( const CSteamID &steamID ) = 0;

	// Called to debug-dump a player group
	virtual void Dump() const = 0;

	virtual const CSteamID GetLeader() const = 0;

	virtual int GetNumPendingInvites() const = 0;
	virtual const CSteamID GetPendingInvite( int i ) const = 0;
	virtual int GetPendingInviteIndexBySteamID( const CSteamID &steamID ) const = 0;

	virtual bool AllowInvites() const = 0;

#ifdef GC
	virtual void SetGroupID( PlayerGroupID_t nPartyID ) = 0;
	virtual void AddMember( const CSteamID &steamID ) = 0;
	virtual void RemoveMember( const CSteamID &steamID ) = 0;

	virtual void SetLeader( const CSteamID &steamID ) = 0;
	virtual void AddPendingInvite( const CSteamID &steamID ) = 0;
	virtual void RemovePendingInvite( const CSteamID &steamID ) = 0;
#endif
};

}

#endif
