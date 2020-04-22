//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Manages CPlayerGroups (A group of players stored on the GC)
//			
//=============================================================================

#ifndef PLAYERGROUPMANAGER_H
#define PLAYERGROUPMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "playergroup.h"

const int k_NoGroupMemberLimit = -1;

namespace GCSDK
{

	class CGCJobDestroyPlayerGroup;
	class CGCJobFindGroupFromMemcached;

	class CPlayerGroupManager
	{
	public:
		CPlayerGroupManager();

		static const int kGroupIDGenerationLockType = -1;

		virtual void YieldingSessionStartPlaying( CGCUserSession *pSession );
		virtual void YieldingSessionStopPlaying( CGCUserSession *pSession ) { }
		virtual void YieldingSessionStartServer( CGCGSSession *pSession );
		virtual void YieldingSessionStopServer( CGCGSSession *pSession );

		IPlayerGroup* YldFindAndLockGroup( PlayerGroupID_t nPlayerGroupID );
		IPlayerGroup* YldFindAndLockGroupByMemberID( const CSteamID &steamID );
		IPlayerGroup* FindGroup( PlayerGroupID_t nPlayerGroupID );
		IPlayerGroup* FindGroupByMemberID( const CSteamID &steamID );
		virtual int GetGroupLockType() = 0;

		bool IsPlayerWaitingForMemcache( const CSteamID &steamID ) const;

		bool BYieldingLockGroupID( PlayerGroupID_t nPlayerGroupID );
		void UnlockGroupID( PlayerGroupID_t nPlayerGroupID );
		bool IsGroupIDLocked( PlayerGroupID_t nPlayerGroupID );
		void StartFrameSchedule();
		bool BExpireLocks( CLimitTimer &limitTimer );
		void DumpGroups();

		virtual void SendGroupStorageAndNetworkUpdate( IPlayerGroup *pPlayerGroup );

		// invites
		void YldInviteToGroup( const CSteamID &steamIDLeader, const CSteamID &steamIDNewMember );
		void YldGroupInviteResponse( const CSteamID &steamID, const PlayerGroupID_t nPlayerGroupID, bool bAccepted );
		void YldRequestKickFromGroup( const CSteamID &steamIDLeader, const CSteamID &steamIDNewMember );
		bool BYldRequestLeaveGroup( const CSteamID &steamID );

		void YldDestroyGroup( PlayerGroupID_t nPlayerGroupID );

		virtual int GetMaxGroupMembers() { return k_NoGroupMemberLimit; }

	protected:
		bool BYldAddMemberToGroup( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDNewMember );
		void YldRemoveMemberFromGroup( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDRemovingMember );

		void YldDoFindGroupFromMemcached( const CSteamID &memberSteamID );

		virtual IPlayerGroup* YldCreateAndLockPlayerGroup() = 0;					// game specific derived class will implement this to create a game specific playergroup
		virtual IPlayerGroup* YldCreateAndLockPlayerGroupFromMemcached( const CUtlBuffer &buf ) = 0;		// game specific derived class will implement this to create a game specific playergroup
		PlayerGroupID_t GeneratePlayerGroupID();
		void YldDestroyGroupIfEmpty( PlayerGroupID_t nPlayerGroupID );		

		void MemcachedUpdateAllMemberAssocation( IPlayerGroup *pPlayerGroup );
		void MemcachedRemoveAllMemberAssocation( IPlayerGroup *pPlayerGroup );
		void MemcachedUpdateMemberAssocation( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamID );
		void MemcachedRemoveMemberAssocation( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamID );
		void YldFindGroupFromMemcached( const CSteamID &memberSteamID );
		virtual const char *GetMemcachedIdentityKey() const = 0;

		// notifications for derived classes
		virtual void YldOnPlayerJoinedGroup( IPlayerGroup *pPlayerGroup, const CSteamID& steamIDNewMember ) { }
		virtual void YldOnPlayerLeftGroup( IPlayerGroup *pPlayerGroup, const CSteamID& steamIDRemovingMember );
		virtual void YldOnGroupDestroyed( IPlayerGroup *pPlayerGroup );
		virtual void YldOnGroupLoadedFromMemcached( IPlayerGroup *pPlayerGroup );

		// invites
		virtual IPlayerGroupInvite* CreateInvite() = 0;
		void YldAddPendingInvite( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDNewMember );
		virtual void YldRemovePendingInvite( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDNewMember );
		virtual bool YldHasGroupInvite( const CSteamID &steamID, const PlayerGroupID_t nPlayerGroupID ) = 0;
		void YldCreateInvitesForGroup( PlayerGroupID_t nPlayerGroupID );

		typedef CUtlMap<PlayerGroupID_t, IPlayerGroup*, int32> mapPlayerGroups_t;
		typedef CUtlHashMapLarge<PlayerGroupID_t, IPlayerGroup*> hashPlayerGroups_t;
		hashPlayerGroups_t m_mapGroups;											// map from PlayerGroupID_t to IPlayerGroup

		typedef CUtlHashMapLarge<CSteamID, IPlayerGroup*> mapMembersToPlayerGroups_t;
		mapMembersToPlayerGroups_t m_mapMemberToGroup;							// map from any member's SteamID to IPlayerGroup

		CTHash<CLock, PlayerGroupID_t>	m_hashPlayerGroupIDLocks;
		friend class CGCJobDestroyGroup;
		friend class CGCJobFindGroupFromMemcached;

		CLock m_GroupIDGenerationLock;

	private:
		typedef CUtlHashMapLarge<CSteamID, int> mapPlayersMemcacheJobCount_t;
		static mapPlayersMemcacheJobCount_t sm_mapPlayersMemcacheJobCount;
	};

	class CGCJobDestroyGroup : public CGCJob
	{
	public:
		CGCJobDestroyGroup( CGCBase *pGC, CPlayerGroupManager *pGroupManager, PlayerGroupID_t nPlayerGroupID ) : CGCJob( pGC ), m_pGroupManager( pGroupManager ), m_nPlayerGroupID( nPlayerGroupID )  { }
		virtual bool BYieldingRunGCJob();
	private:
		CPlayerGroupManager *m_pGroupManager;
		PlayerGroupID_t m_nPlayerGroupID;
	};

	class CGCJobLeaveGroup : public CGCJob
	{
	public:
		CGCJobLeaveGroup( CGCBase *pGC, CPlayerGroupManager *pGroupManager, const CSteamID &steamID ) : CGCJob( pGC ), m_pGroupManager( pGroupManager ), m_SteamID( steamID )  { }
		virtual bool BYieldingRunGCJob();
	private:
		CPlayerGroupManager *m_pGroupManager;
		CSteamID m_SteamID;
	};

	class CGCJobFindGroupFromMemcached : public CGCJob
	{
	public:
		CGCJobFindGroupFromMemcached( CGCBase *pGC, CPlayerGroupManager *pGroupManager, const CSteamID& memberSteamID ) : CGCJob( pGC ), m_pGroupManager( pGroupManager ), m_memberSteamID( memberSteamID )  { }
		virtual bool BYieldingRunGCJob();
	private:
		CPlayerGroupManager *m_pGroupManager;
		CSteamID m_memberSteamID;
	};
}

#endif
