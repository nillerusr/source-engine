//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"
#include "gcsdk/gcsdk.h"
#include "base_gcmessages.h"
#include "playergroupmanager.h"
#include "gcsdk/accountdetails.h"

using namespace GCSDK;

CPlayerGroupManager::mapPlayersMemcacheJobCount_t CPlayerGroupManager::sm_mapPlayersMemcacheJobCount;

CPlayerGroupManager::CPlayerGroupManager() :
m_hashPlayerGroupIDLocks( k_nLocksRunInterval / k_cMicroSecPerShellFrame )
{
	m_hashPlayerGroupIDLocks.Init( k_cGCLocksInit, k_cBucketGCLocks );
	m_GroupIDGenerationLock.SetName( "GroupIDGenerationLock" );
	m_GroupIDGenerationLock.SetLockType( kGroupIDGenerationLockType );
}

PlayerGroupID_t CPlayerGroupManager::GeneratePlayerGroupID()
{
	return GGCHost()->GenerateGID();
}

void CPlayerGroupManager::MemcachedUpdateAllMemberAssocation( IPlayerGroup *pPlayerGroup )
{
	CUtlVector<CUtlString> memcachedMemberKeys;
	CUtlVector<CGCBase::GCMemcachedBuffer_t> memcachedMemberValues;

	CUtlBuffer memcachedMemberValue;
	memcachedMemberValue.PutInt64( (int64) pPlayerGroup->GetGroupID() );

	for ( int i = 0; i < pPlayerGroup->GetNumMembers(); ++i )
	{
		CUtlString &memberKey = memcachedMemberKeys[ memcachedMemberKeys.AddToTail() ];
		memberKey.Format( "GC:%u:%sMEMBER:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), pPlayerGroup->GetMember(i).ConvertToUint64() );

		CGCBase::GCMemcachedBuffer_t &memcachedBuffer = memcachedMemberValues[ memcachedMemberValues.AddToTail() ];
		memcachedBuffer.m_pubData = memcachedMemberValue.Base();
		memcachedBuffer.m_cubData = memcachedMemberValue.TellPut();
	}

	GGCBase()->BMemcachedSet( memcachedMemberKeys, memcachedMemberValues );
}

void CPlayerGroupManager::MemcachedRemoveAllMemberAssocation( IPlayerGroup *pPlayerGroup )
{
	CUtlVector<CUtlString> memcachedMemberKeys;

	for ( int i = 0; i < pPlayerGroup->GetNumMembers(); ++i )
	{
		CUtlString &memberKey = memcachedMemberKeys[ memcachedMemberKeys.AddToTail() ];
		memberKey.Format( "GC:%u:%sMEMBER:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), pPlayerGroup->GetMember(i).ConvertToUint64() );
	}

	GGCBase()->BMemcachedDelete( memcachedMemberKeys );
}

void CPlayerGroupManager::MemcachedUpdateMemberAssocation( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamID )
{
	CUtlBuffer memcachedMemberValue;
	memcachedMemberValue.PutInt64( (int64) nPlayerGroupID );
	CUtlString memberKey;
	memberKey.Format( "GC:%u:%sMEMBER:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), steamID.ConvertToUint64() );

	GGCBase()->BMemcachedSet( memberKey, memcachedMemberValue );
}

void CPlayerGroupManager::MemcachedRemoveMemberAssocation( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamID )
{
	CUtlString memberKey;
	memberKey.Format( "GC:%u:%sMEMBER:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), steamID.ConvertToUint64() );
	GGCBase()->BMemcachedDelete( memberKey );
}

bool CPlayerGroupManager::BYldAddMemberToGroup( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDNewMember )
{
	// If member is already in a group, abort
	IPlayerGroup *pOldPlayerGroup = FindGroupByMemberID( steamIDNewMember );
	if ( pOldPlayerGroup )
	{
		EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYldAddMemberToGroup not adding member: %s to player group: %016llx as he's already in a group\n", steamIDNewMember.Render(), nPlayerGroupID );
		return false;
	}

	// check group exists
	IPlayerGroup *pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	Assert( pPlayerGroup );
	if ( !pPlayerGroup )
	{
		EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYldAddMemberToGroup not adding member: %s to player group: %016llx as that group does not exist\n", steamIDNewMember.Render(), nPlayerGroupID );
		return false;
	}

	// check member isn't already in the group
	if ( pPlayerGroup->GetMemberIndexBySteamID( steamIDNewMember ) != -1 )
	{
		EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYldAddMemberToGroup not adding member: %s to player group: %016llx as he's already in the group\n", steamIDNewMember.Render(), nPlayerGroupID );
		UnlockGroupID( nPlayerGroupID );
		return false;	
	}

	// Lock the user's cache so we can add the group to it
	CGCSharedObjectCache *pCache = GGCBase()->YieldingGetLockedSOCache( steamIDNewMember );
	if ( !pCache )
	{
		EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYldAddMemberToGroup not adding member: %s to player group: %016llx as his cache could not be loaded\n", steamIDNewMember.Render(), nPlayerGroupID );
		GGCBase()->UnlockSteamID( steamIDNewMember );
		UnlockGroupID( nPlayerGroupID );
		return false;
	}

	// If member is already in a group, abort (must do this again after yielding as another job could have put him in a group while we yielded)
	pOldPlayerGroup = FindGroupByMemberID( steamIDNewMember );
	if ( pOldPlayerGroup )
	{
		EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYldAddMemberToGroup not adding member: %s to player group: %016llx as he's already in a group (2)\n", steamIDNewMember.Render(), nPlayerGroupID );
		GGCBase()->UnlockSteamID( steamIDNewMember );
		UnlockGroupID( nPlayerGroupID );
		return false;
	}

	// Add shared object to this member's cache, before dirtying fields
	pCache->AddObject( pPlayerGroup->GetSharedObject() );

	// Add member to group. This will dirty shared object fields.
	pPlayerGroup->AddMember( steamIDNewMember );
	m_mapMemberToGroup.InsertOrReplace( steamIDNewMember, pPlayerGroup );
	GGCBase()->UnlockSteamID( steamIDNewMember );

	// Update memcached
	MemcachedUpdateMemberAssocation( nPlayerGroupID, steamIDNewMember );

	// notify derived classes
	YldOnPlayerJoinedGroup( pPlayerGroup, steamIDNewMember );

	UnlockGroupID( nPlayerGroupID );

	EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "Steam id: %s added to player group: %016llx\n", steamIDNewMember.Render(), nPlayerGroupID );

	return true;
}

void CPlayerGroupManager::YldRemoveMemberFromGroup( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDRemovingMember )
{
	// check group exists
	IPlayerGroup *pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	Assert( pPlayerGroup );
	if ( !pPlayerGroup )
		return;

	// check member is in the group
	if ( pPlayerGroup->GetMemberIndexBySteamID( steamIDRemovingMember ) == -1 )
	{
		UnlockGroupID( nPlayerGroupID );
		return;
	}

	pPlayerGroup->RemoveMember( steamIDRemovingMember );
	m_mapMemberToGroup.Remove( steamIDRemovingMember );

	// Update memcached
	MemcachedRemoveMemberAssocation( nPlayerGroupID, steamIDRemovingMember );

	CGCSharedObjectCache *pCache = GGCBase()->YieldingGetLockedSOCache( steamIDRemovingMember );
	if ( pCache )
	{
		pCache->RemoveObject( *pPlayerGroup->GetSharedObject() );
		pCache->SendAllNetworkUpdates();
	}
	GGCBase()->UnlockSteamID( steamIDRemovingMember );

	YldOnPlayerLeftGroup( pPlayerGroup, steamIDRemovingMember );

	UnlockGroupID( nPlayerGroupID );

	EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "Steam id: %s removed from player group: %016llx\n", steamIDRemovingMember.Render(), nPlayerGroupID );
}

IPlayerGroup* CPlayerGroupManager::FindGroup( PlayerGroupID_t nPlayerGroupID )
{
	hashPlayerGroups_t::IndexType_t nPlayerGroupIndex = m_mapGroups.Find( nPlayerGroupID );
	if ( nPlayerGroupIndex == m_mapGroups.InvalidIndex() )
		return NULL;

	return m_mapGroups.Element( nPlayerGroupIndex );
}

IPlayerGroup* CPlayerGroupManager::YldFindAndLockGroup( PlayerGroupID_t nPlayerGroupID )
{
	hashPlayerGroups_t::IndexType_t nPlayerGroupIndex = m_mapGroups.Find( nPlayerGroupID );
	if ( nPlayerGroupIndex == m_mapGroups.InvalidIndex() )
		return NULL;

	// try to lock it
	if ( !BYieldingLockGroupID( nPlayerGroupID ) )
		return NULL;

	// check the group is still valid
	nPlayerGroupIndex = m_mapGroups.Find( nPlayerGroupID );
	if ( nPlayerGroupIndex == m_mapGroups.InvalidIndex() )
	{
		// group went away, unlock and abort
		UnlockGroupID( nPlayerGroupID );
		return NULL;
	}

	return m_mapGroups.Element( nPlayerGroupIndex );
}

IPlayerGroup* CPlayerGroupManager::FindGroupByMemberID( const CSteamID &steamID )
{
	mapMembersToPlayerGroups_t::IndexType_t nPlayerGroupIndex = m_mapMemberToGroup.Find( steamID );
	if ( nPlayerGroupIndex == m_mapMemberToGroup.InvalidIndex() )
		return NULL;

	return m_mapMemberToGroup.Element( nPlayerGroupIndex );
}

IPlayerGroup* CPlayerGroupManager::YldFindAndLockGroupByMemberID( const CSteamID &steamID )
{
	mapMembersToPlayerGroups_t::IndexType_t nPlayerGroupIndex = m_mapMemberToGroup.Find( steamID );
	if ( nPlayerGroupIndex == m_mapMemberToGroup.InvalidIndex() )
		return NULL;

	IPlayerGroup *pPlayerGroup = m_mapMemberToGroup.Element( nPlayerGroupIndex );

	return YldFindAndLockGroup( pPlayerGroup->GetGroupID() );
}

bool CPlayerGroupManager::BYieldingLockGroupID( PlayerGroupID_t nPlayerGroupID )
{
	AssertRunningJob();

	// lookup
	CLock *pLock = m_hashPlayerGroupIDLocks.PvRecordFind( nPlayerGroupID );
	if ( pLock == NULL )
	{
		// no lock yet, insert one
		pLock = m_hashPlayerGroupIDLocks.PvRecordInsert( nPlayerGroupID );
		Assert( pLock != NULL );
		if ( pLock == NULL )
		{
			EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "Unable to create lock for PlayerGroup:%u\n", nPlayerGroupID );
			return false;
		}
		pLock->SetName( "PlayerGroup:", nPlayerGroupID );
		pLock->SetLockType( GetGroupLockType() );
	}

	return GJobCur().BYieldingAcquireLock( pLock );
}

void CPlayerGroupManager::UnlockGroupID( PlayerGroupID_t nPlayerGroupID )
{
	AssertRunningJob();

	// lookup
	CLock *pLock = m_hashPlayerGroupIDLocks.PvRecordFind( nPlayerGroupID );
	Assert( pLock );
	if ( pLock == NULL )
	{
		return;
	}

	if ( pLock->GetJobLocking() != &GJobCur() )
	{
		AssertMsg1( false, "UnlockGroupID() called when job (%s) doesn't own the lock", GJobCur().GetName() );
		return;
	}

	GJobCur().ReleaseLock( pLock );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the specified PlayerGroupID_t is locked
//-----------------------------------------------------------------------------
bool CPlayerGroupManager::IsGroupIDLocked( PlayerGroupID_t nPlayerGroupID )
{
	CLock *pLock = m_hashPlayerGroupIDLocks.PvRecordFind( nPlayerGroupID );
	if ( pLock )
		return pLock->BIsLocked();

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Marks the start of a frame
//-----------------------------------------------------------------------------
void CPlayerGroupManager::StartFrameSchedule()
{
	m_hashPlayerGroupIDLocks.StartFrameSchedule( true );
}


//-----------------------------------------------------------------------------
// Purpose: Gets rid of any locks that haven't been touched in a while
//-----------------------------------------------------------------------------
bool CPlayerGroupManager::BExpireLocks( CLimitTimer &limitTimer )
{
	VPROF_BUDGET( "Expire PlayerGroup Locks", VPROF_BUDGETGROUP_STEAM );

	for ( CLock *pLock = m_hashPlayerGroupIDLocks.PvRecordRun(); NULL != pLock; pLock = m_hashPlayerGroupIDLocks.PvRecordRun() )
	{
		if ( !pLock->BIsLocked() && pLock->GetMicroSecondsSinceLock() > k_cMicroSecLockLifetime )
		{
			m_hashPlayerGroupIDLocks.Remove( pLock );
		}

		if ( limitTimer.BLimitReached() )
			return true;
	}

	return false;
}


void CPlayerGroupManager::YldDestroyGroupIfEmpty( PlayerGroupID_t nPlayerGroupID )
{
	IPlayerGroup *pGroup = YldFindAndLockGroup( nPlayerGroupID );
	if ( pGroup )
	{
		if ( pGroup->GetNumMembers() <= 0 )
		{
			YldDestroyGroup( nPlayerGroupID );
		}
	}
}

void CPlayerGroupManager::YldDestroyGroup( PlayerGroupID_t nPlayerGroupID )
{
	IPlayerGroup* pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	if ( !pPlayerGroup )
		return;

	// allow derived classes to clean up
	YldOnGroupDestroyed( pPlayerGroup );

	// remove all members
	while( pPlayerGroup->GetNumMembers() > 0 )
	{
		YldRemoveMemberFromGroup( nPlayerGroupID, pPlayerGroup->GetMember( 0 ) );
	}

	m_mapGroups.Remove( nPlayerGroupID );

	// Remove the group from memcached
	GGCBase()->BMemcachedDelete( CFmtStr( "GC:%u:%s:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), nPlayerGroupID ).Access() );

	delete pPlayerGroup;
}

// Update memcached and players when the group changes
void CPlayerGroupManager::SendGroupStorageAndNetworkUpdate( IPlayerGroup *pPlayerGroup )
{
	for ( int i = 0; i < pPlayerGroup->GetNumMembers(); i++ )
	{
		CGCUserSession *pMemberSession = GGCBase()->FindUserSession( pPlayerGroup->GetMember( i ) );
		if ( !pMemberSession )
			continue;

		pMemberSession->GetSOCache()->SendAllNetworkUpdates();
	}

	// Update memcached
	VPROF_BUDGET( "SendGroupStorageAndNetworkUpdate - Memcached", VPROF_BUDGETGROUP_STEAM );

	CUtlString memcachedGroupKey;
	memcachedGroupKey.Format( "GC:%u:%s:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), pPlayerGroup->GetGroupID() );

	CUtlBuffer memcachedGroupValue;
	CSharedObject *pObj = pPlayerGroup->GetSharedObject();
	Assert( pObj );
	if ( !pObj )
	{
		return;
	}
	pObj->BAddFullCreateToMessage( memcachedGroupValue );

	GGCBase()->BMemcachedSet( memcachedGroupKey, memcachedGroupValue );
}

void CPlayerGroupManager::DumpGroups()
{
	// Sort groups so we can dump in-order
	CUtlSortVector<PlayerGroupID_t> vecIDs( 0, m_mapGroups.Count() );
	FOR_EACH_MAP_FAST( m_mapGroups, i )
	{
		vecIDs.InsertNoSort( m_mapGroups.Key( i ) );
	}
	vecIDs.RedoSort( true );

	FOR_EACH_VEC( vecIDs, i )
	{
		IPlayerGroup *pPlayerGroup = m_mapGroups[m_mapGroups.Find( vecIDs[i] )];
		if ( !pPlayerGroup || !pPlayerGroup->GetSharedObject() )
			continue;

		EmitInfo( SPEW_SHAREDOBJ, SPEW_ALWAYS, LOG_ALWAYS, "Group: %llu  NumMembers: %d\n", pPlayerGroup->GetGroupID(), pPlayerGroup->GetNumMembers() );
		pPlayerGroup->GetSharedObject()->Dump();
	}
}

void CPlayerGroupManager::YieldingSessionStartPlaying( CGCUserSession *pSession )
{
	mapMembersToPlayerGroups_t::IndexType_t nPlayerGroupIndex = m_mapMemberToGroup.Find( pSession->GetSteamID() );
	if ( nPlayerGroupIndex == m_mapMemberToGroup.InvalidIndex() )
	{
		mapPlayersMemcacheJobCount_t::IndexType_t mIndex = sm_mapPlayersMemcacheJobCount.Find( pSession->GetSteamID() );
		if ( mIndex == sm_mapPlayersMemcacheJobCount.InvalidIndex() )
		{
			sm_mapPlayersMemcacheJobCount.Insert( pSession->GetSteamID(), 1 );
		}	
		else
		{
			sm_mapPlayersMemcacheJobCount.Element( mIndex )++;
		}

		// could not find a group in memory for this user, see if we can find it in memcached
		CGCJobFindGroupFromMemcached *pJob = new CGCJobFindGroupFromMemcached( GGCBase(), this, pSession->GetSteamID() );
		pJob->StartJob( NULL );
	}
}

void CPlayerGroupManager::YieldingSessionStartServer( CGCGSSession *pSession )
{
}

void CPlayerGroupManager::YieldingSessionStopServer( CGCGSSession *pSession )
{
}

void CPlayerGroupManager::YldDoFindGroupFromMemcached( const CSteamID &memberSteamID )
{
	// See if we can find him in memcached
	CUtlVector<CUtlString> memcachedMemberKeys;
	CUtlVector<CGCBase::GCMemcachedGetResult_t> memcachedMemberResults;
	CUtlString &memberKey = memcachedMemberKeys[ memcachedMemberKeys.AddToTail() ];
	memberKey.Format( "GC:%u:%sMEMBER:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), memberSteamID.ConvertToUint64() );

	if ( !GGCBase()->BYieldingMemcachedGet( memcachedMemberKeys, memcachedMemberResults ) )
	{
		return; // Not found
	}

	if ( memcachedMemberResults.Count() != 1 || !memcachedMemberResults[0].m_bKeyFound )
	{
		return; // Not found
	}
	CUtlBuffer memcachedGroupResult;
	memcachedGroupResult.SetExternalBuffer( memcachedMemberResults[0].m_bufValue.Base(), memcachedMemberResults[0].m_bufValue.Count(), memcachedMemberResults[0].m_bufValue.Count() );
	PlayerGroupID_t nPlayerGroupID = (uint64) memcachedGroupResult.GetInt64();

	if ( !BYieldingLockGroupID( nPlayerGroupID ) )
	{
		EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "YldFindGroupFromMemcached: Unable to create lock for PlayerGroup:%u\n", nPlayerGroupID );
		return;
	}

	// check no-one else recreated this group while we were waiting for the lock
	if ( FindGroup( nPlayerGroupID ) )
		return;

	// We have a group id, try read the group from memcached
	CUtlVector<CUtlString> memcachedGroupKeys;
	CUtlVector<CGCBase::GCMemcachedGetResult_t> memcachedGroupResults;
	CUtlString &memcachedGroupKey = memcachedGroupKeys[ memcachedGroupKeys.AddToTail() ];
	memcachedGroupKey.Format( "GC:%u:%s:%llu", GGCBase()->GetAppID(), GetMemcachedIdentityKey(), nPlayerGroupID );

	if ( !GGCBase()->BYieldingMemcachedGet( memcachedGroupKeys, memcachedGroupResults ) )
	{
		return; // Not found
	}

	if ( memcachedGroupResults.Count() != 1 || !memcachedGroupResults[0].m_bKeyFound )
	{
		return; // Not found
	}


	CUtlBuffer memcachedPartyResult;
	memcachedPartyResult.SetExternalBuffer( memcachedGroupResults[0].m_bufValue.Base(), memcachedGroupResults[0].m_bufValue.Count(), memcachedGroupResults[0].m_bufValue.Count() );
	IPlayerGroup *pPlayerGroup = YldCreateAndLockPlayerGroupFromMemcached( memcachedPartyResult );

	// pPlayerGroup won't be NULL because creating it won't fail. Also, the group id is already locked, so only one player
	// will get to this point. So no checks needed before inserting this group id into the map
	m_mapGroups.Insert( nPlayerGroupID, pPlayerGroup );

	// Group loaded, update all objects

	// Add members to the map
	for ( int i = 0; i < pPlayerGroup->GetNumMembers(); ++i )
	{
		// It's ok to add the Nth steam id even with the yield in this loop,
		// because the group is locked. Any player that stops playing will be
		// blocked on this group id.
		CSteamID memberSteamID = pPlayerGroup->GetMember(i);
		m_mapMemberToGroup.InsertOrReplace( memberSteamID, pPlayerGroup );

		CGCSharedObjectCache *pCache = GGCBase()->YieldingGetLockedSOCache( memberSteamID );
		if ( pCache )
		{
			pCache->AddObject( pPlayerGroup->GetSharedObject() );
		}
		GGCBase()->UnlockSteamID( memberSteamID );
	}

	YldOnGroupLoadedFromMemcached( pPlayerGroup );

	for ( int i = 0; i < pPlayerGroup->GetNumMembers(); i++ )
	{
		CGCUserSession *pMemberSession = GGCBase()->FindUserSession( pPlayerGroup->GetMember( i ) );
		if ( !pMemberSession )
			continue;

		pMemberSession->GetSOCache()->SendAllNetworkUpdates();
	}
}

void CPlayerGroupManager::YldFindGroupFromMemcached( const CSteamID &memberSteamID )
{
	YldDoFindGroupFromMemcached( memberSteamID );

	mapPlayersMemcacheJobCount_t::IndexType_t mIndex = sm_mapPlayersMemcacheJobCount.Find( memberSteamID );
	if ( mIndex != sm_mapPlayersMemcacheJobCount.InvalidIndex() )
	{
		if ( --sm_mapPlayersMemcacheJobCount.Element( mIndex ) <= 0 )
		{
			sm_mapPlayersMemcacheJobCount.RemoveAt( mIndex );
		}
	}	
}

bool CGCJobDestroyGroup::BYieldingRunGCJob()
{
	if ( m_pGroupManager )
	{
		m_pGroupManager->YldDestroyGroup( m_nPlayerGroupID );
	}
	return true;
}

bool CGCJobLeaveGroup::BYieldingRunGCJob()
{
	if ( m_pGroupManager )
	{
		m_pGroupManager->BYldRequestLeaveGroup( m_SteamID );
	}
	return true;
}

bool CGCJobFindGroupFromMemcached::BYieldingRunGCJob()
{
	if ( m_pGroupManager )
	{
		m_pGroupManager->YldFindGroupFromMemcached( m_memberSteamID );
	}
	return true;
}


// === Invites ===


void CPlayerGroupManager::YldInviteToGroup( const CSteamID &steamIDLeader, const CSteamID &steamIDNewMember )
{
	if ( !steamIDLeader.IsValid() || ! steamIDLeader.BIndividualAccount() 
		|| !steamIDNewMember.IsValid() || ! steamIDNewMember.BIndividualAccount() )
		return;

	// create group if this leader isn't in one already
	IPlayerGroup *pPlayerGroup = YldFindAndLockGroupByMemberID( steamIDLeader );
	if ( !pPlayerGroup )
	{
		pPlayerGroup = YldCreateAndLockPlayerGroup();
		m_mapGroups.Insert( pPlayerGroup->GetGroupID(), pPlayerGroup );
		if ( !BYldAddMemberToGroup( pPlayerGroup->GetGroupID(), steamIDLeader ) )
			return;
		pPlayerGroup->SetLeader( steamIDLeader );
	}
	else if ( pPlayerGroup && pPlayerGroup->GetLeader() != steamIDLeader )
	{
		// non-leader attempting to invite
		return;
	}

	// don't send any invite if our group is already full
	if ( GetMaxGroupMembers() != k_NoGroupMemberLimit && pPlayerGroup->GetNumMembers() >= GetMaxGroupMembers() )
	{
		CProtoBufMsg<CMsgGCError> msg( k_EMsgGCError );
		msg.Body().set_error_text( "Your party is full." );
		GGCBase()->BSendGCMsgToClient( steamIDLeader, msg );
		return;
	}

	if ( pPlayerGroup->GetPendingInviteIndexBySteamID( steamIDNewMember ) == -1 )
	{
		pPlayerGroup->AddPendingInvite( steamIDNewMember );
	}
	
	if ( !YldHasGroupInvite( steamIDNewMember, pPlayerGroup->GetGroupID() ) )
	{
		CGCSharedObjectCache *pCache = GGCBase()->YieldingGetLockedSOCache( steamIDNewMember );
		if ( pCache )
		{
			// Send invite to the new member
			IPlayerGroupInvite *pInvite = CreateInvite();
			pInvite->YldInitFromPlayerGroup( pPlayerGroup );
	
			pCache->AddObject( pInvite->GetSharedObject() );
		}
		GGCBase()->UnlockSteamID( steamIDNewMember );
	}

	// Send a message back to the sender telling him his invitation was created
	GCSDK::CProtoBufMsg<CMsgInvitationCreated> msg( k_EMsgGCInvitationCreated );
	msg.Body().set_group_id( pPlayerGroup->GetGroupID() );
	msg.Body().set_steam_id( steamIDNewMember.ConvertToUint64() );
	GGCBase()->BSendGCMsgToClient( steamIDLeader, msg );

	SendGroupStorageAndNetworkUpdate( pPlayerGroup );
}

void CPlayerGroupManager::YldRemovePendingInvite( PlayerGroupID_t nPlayerGroupID, const CSteamID &steamIDNewMember )
{
	IPlayerGroup *pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	if ( pPlayerGroup != NULL )
	{
		pPlayerGroup->RemovePendingInvite( steamIDNewMember );
		UnlockGroupID( nPlayerGroupID );
	}

	// Derived classes will remove the CPartyInvite/CLobbyInvite objects from the user's SO cache
}

void CPlayerGroupManager::YldGroupInviteResponse( const CSteamID &steamID, const PlayerGroupID_t nPlayerGroupID, bool bAccepted )
{
	if ( !steamID.IsValid() || !steamID.BIndividualAccount() )
		return;

	// check if user is already in a group
	if ( bAccepted )
	{
		IPlayerGroup* pOtherGroup = YldFindAndLockGroupByMemberID( steamID );
		if ( pOtherGroup )
		{
			// remove him from the other group
			PlayerGroupID_t nOtherPlayerGroupID = pOtherGroup->GetGroupID();
			YldRemoveMemberFromGroup( nOtherPlayerGroupID, steamID );
			YldDestroyGroupIfEmpty( nOtherPlayerGroupID );
			UnlockGroupID( nOtherPlayerGroupID );

			// If the group wasn't deleted, update others about this person leaving
			pOtherGroup = FindGroup( nOtherPlayerGroupID );
			if ( pOtherGroup )
			{
				SendGroupStorageAndNetworkUpdate( pOtherGroup );
			}
			GJobCur().ReleaseLocks();
		}
		GJobCur().ReleaseLocks();
	}

	IPlayerGroup* pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	if ( !pPlayerGroup )
	{
		CProtoBufMsg<CMsgGCError> msg( k_EMsgGCError );
		msg.Body().set_error_text( CFmtStr( "Couldn't respond to party invite, couldn't find a party with ID %llu.", nPlayerGroupID ) );
		GGCBase()->BSendGCMsgToClient( steamID, msg );
		return;
	}

	// check an invite is pending for this user
	if ( pPlayerGroup->GetPendingInviteIndexBySteamID( steamID ) == -1 )
	{
		CProtoBufMsg<CMsgGCError> msg( k_EMsgGCError );
		msg.Body().set_error_text( CFmtStr( "Couldn't respond to party invite, that party (%llu) doesn't have an invite pending for you.", nPlayerGroupID ) );
		GGCBase()->BSendGCMsgToClient( steamID, msg );
		return;
	}

	YldRemovePendingInvite( nPlayerGroupID, steamID );

	if ( bAccepted )
	{
		// don't exceed the max size for this group
		if ( GetMaxGroupMembers() != k_NoGroupMemberLimit && pPlayerGroup->GetNumMembers() >= GetMaxGroupMembers() )
		{
			CProtoBufMsg<CMsgGCError> msg( k_EMsgGCError );
			msg.Body().set_error_text( "Cannot join party.  Party is full." );
			GGCBase()->BSendGCMsgToClient( steamID, msg );
			return;
		}

		BYldAddMemberToGroup( nPlayerGroupID, steamID );
	}
	else
	{
		// TODO: Praps notify the group that this person rejected the invite?
	}

	SendGroupStorageAndNetworkUpdate( pPlayerGroup );
}

void CPlayerGroupManager::YldRequestKickFromGroup( const CSteamID &steamIDLeader, const CSteamID &steamIDVictim )
{
	if ( !steamIDLeader.IsValid() || ! steamIDLeader.BIndividualAccount() 
		|| !steamIDVictim.IsValid() || ! steamIDVictim.BIndividualAccount() )
		return;

	// find leader's group
	IPlayerGroup* pPlayerGroup = YldFindAndLockGroupByMemberID( steamIDLeader );
	if ( !pPlayerGroup )
		return;

	if ( pPlayerGroup->GetLeader() != steamIDLeader || steamIDLeader == steamIDVictim )
	{
		// non-leader attempting to kick
		return;
	}

	// clear any pending invites for this person
	if ( pPlayerGroup->GetPendingInviteIndexBySteamID( steamIDVictim ) != -1 )
	{
		YldRemovePendingInvite( pPlayerGroup->GetGroupID(), steamIDVictim );
	}

	PlayerGroupID_t nPlayerGroupID = pPlayerGroup->GetGroupID();
	YldRemoveMemberFromGroup( nPlayerGroupID, steamIDVictim );
	YldDestroyGroupIfEmpty( nPlayerGroupID );

	pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	if ( pPlayerGroup )
	{
		SendGroupStorageAndNetworkUpdate( pPlayerGroup );
	}
}

bool CPlayerGroupManager::BYldRequestLeaveGroup( const CSteamID &steamID )
{
	if ( !steamID.IsValid() || ! steamID.BIndividualAccount() )
		return false;

	// find group
	IPlayerGroup *pPlayerGroup = YldFindAndLockGroupByMemberID( steamID );
	if ( !pPlayerGroup )
	{
		CProtoBufMsg<CMsgGCError> msg( k_EMsgGCError );
		msg.Body().set_error_text( CFmtStr( "%s: Failed to remove you from your group.  Couldn't find your group in the group map.", GetMemcachedIdentityKey() ) );
		GGCBase()->BSendGCMsgToClient( steamID, msg );
		return false;
	}

	// clear any pending invites for this person
	if ( pPlayerGroup->GetPendingInviteIndexBySteamID( steamID ) != -1 )
	{
		YldRemovePendingInvite( pPlayerGroup->GetGroupID(), steamID );
	}

	PlayerGroupID_t nPlayerGroupID = pPlayerGroup->GetGroupID();
	YldRemoveMemberFromGroup( nPlayerGroupID, steamID );
	YldDestroyGroupIfEmpty( nPlayerGroupID );

	pPlayerGroup = YldFindAndLockGroup( nPlayerGroupID );
	if ( pPlayerGroup )
	{
		SendGroupStorageAndNetworkUpdate( pPlayerGroup );
	}

	return true;
}

void CPlayerGroupManager::YldOnPlayerLeftGroup( IPlayerGroup *pPlayerGroup, const CSteamID& steamIDRemovingMember )
{
	// if the group leader just left, then set a new leader
	if ( pPlayerGroup->GetLeader() == steamIDRemovingMember && pPlayerGroup->GetNumMembers() > 0 )
	{
		pPlayerGroup->SetLeader( pPlayerGroup->GetMember( 0 ) );
	}
}

void CPlayerGroupManager::YldOnGroupDestroyed( IPlayerGroup *pPlayerGroup )
{
	PlayerGroupID_t nPlayerGroupID = pPlayerGroup->GetGroupID();

	// remove all pending invites
	EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "CPlayerGroupManager::YldOnGroupDestroyed [%s] Removing all pending invites %016llx\n", GetMemcachedIdentityKey(), nPlayerGroupID );
	for ( int i = pPlayerGroup->GetNumPendingInvites() - 1; i >= 0; i-- )		// iterate backwards as pending invites will be removed from the list
	{
		YldRemovePendingInvite( nPlayerGroupID, pPlayerGroup->GetPendingInvite( i ) );
	}
}

void CPlayerGroupManager::YldCreateInvitesForGroup( PlayerGroupID_t nPlayerGroupID )
{
	// Check to see if the group was loaded, if not discard, it should always be loaded first
	IPlayerGroup *pPlayerGroup = FindGroup( nPlayerGroupID );
	if ( !pPlayerGroup )
		return;

	for ( int i = 0; i < pPlayerGroup->GetNumPendingInvites(); ++i )
	{
		CSteamID steamIDNewMember = pPlayerGroup->GetPendingInvite(i);

		const char *szAccountName = "Unknown Player";
		CAccountDetails *pDetails = GGCBase()->YieldingGetAccountDetails( pPlayerGroup->GetLeader() );
		if ( pDetails )
		{
			szAccountName = pDetails->GetPersonaName();
		}

		// Send invite to the new member
		IPlayerGroupInvite *pInvite = CreateInvite();
		pInvite->SetGroupID( pPlayerGroup->GetGroupID() );
		pInvite->SetSenderID( pPlayerGroup->GetLeader().ConvertToUint64() );
		pInvite->SetSenderName( szAccountName );
		CGCSharedObjectCache *pCache = GGCBase()->YieldingGetLockedSOCache( steamIDNewMember );
		if ( pCache )
		{
			pCache->AddObject( pInvite->GetSharedObject() );
		}
		GGCBase()->UnlockSteamID( steamIDNewMember );
	}
}

void CPlayerGroupManager::YldOnGroupLoadedFromMemcached( GCSDK::IPlayerGroup *pPlayerGroup )
{
	YldCreateInvitesForGroup( pPlayerGroup->GetGroupID() );
}

bool CPlayerGroupManager::IsPlayerWaitingForMemcache( const CSteamID &steamID ) const
{
	mapPlayersMemcacheJobCount_t::IndexType_t mIndex = sm_mapPlayersMemcacheJobCount.Find( steamID );
	if ( mIndex != sm_mapPlayersMemcacheJobCount.InvalidIndex() )
	{
		return true;
	}

	return false;
}

