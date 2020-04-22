//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CAccountDetails class.
//
//=============================================================================

#include "stdafx.h"
#include "accountdetails.h"
#include "rtime.h"
#include "gcsdk_gcmessages.pb.h"

#include "memdbgon.h" // needs to be the last include in the file

namespace GCSDK
{
GCConVar cv_account_details_cache_time( "account_details_cache_time", "600" );
GCConVar cv_account_details_failure_cache_time( "account_details_failure_cache_time", "10" );
GCConVar account_details_timeout( "account_details_timeout", "10" );
GCConVar cv_persona_name_cache_time( "persona_name_cache_time", "60" );
GCConVar cv_persona_name_failure_cache_time( "persona_name_failure_cache_time", "10" );
GCConVar cv_persona_name_batch_size( "persona_name_batch_size", "100" );
GCConVar persona_name_timeout( "persona_name_timeout", "10" );

const char *kszAccountDetailsKey = "AccountDetails-v001";

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAccountDetails::CAccountDetails()
:	m_rtimeCached( CRTime::RTime32TimeCur() ),
	m_bValid( false ),
	m_bPublicProfile( false ),
	m_bVacBanned( false ),
	m_bCyberCafe( false ),
	m_bSchoolAccount( false ),
	m_bFreeTrialAccount( false ),
	m_bSubscribed( false ),
	m_bLowViolence( false ),
	m_bLimited( false ),
	m_bAccountLocked( false ),
	m_bCommunityBanned( false ),
	m_bTradeBanned( false ),
	m_bIsSteamGuardEnabled( false ),
	m_bIsPhoneVerified( false ),
	m_bIsTwoFactorAuthEnabled( false ),
	m_bIsPhoneIdentifying( false ),
	m_unPackage( 0 ),
	m_rtimeVACBanEnd( 0 ),
	m_unSteamLevel( 0 ),
	m_unFriendCount( 0 ),
	m_rtimeAccountCreated( 0 ),
	m_rtimeTwoFactorEnabled( 0 ),
	m_rtimePhoneVerified( 0 ),
	m_unPhoneID( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: Initialize a fresh CAccountDetails with data from Steam
//-----------------------------------------------------------------------------
void CAccountDetails::Init( CGCSystemMsg_GetAccountDetails_Response &msgResponse )
{
	m_bValid = true;
	m_sAccountName = msgResponse.account_name().c_str();
	m_bPublicProfile = msgResponse.is_profile_public();
	m_bPublicInventory = msgResponse.is_inventory_public();
	m_bVacBanned = msgResponse.is_vac_banned(); 
	m_bCyberCafe = msgResponse.is_cyber_cafe(); 
	m_bSchoolAccount = msgResponse.is_school_account(); 
	m_bFreeTrialAccount = msgResponse.is_free_trial_account(); 
	m_bSubscribed = msgResponse.is_subscribed(); 
	m_bLowViolence = msgResponse.is_low_violence(); 
	m_bLimited = msgResponse.is_limited(); 
	m_bAccountLocked = msgResponse.is_account_locked_down();
	m_bCommunityBanned = msgResponse.is_community_banned();
	m_bTradeBanned = msgResponse.is_trade_banned();
	m_unPackage = msgResponse.package();
	m_rtimeVACBanEnd = msgResponse.suspension_end_time();
	m_sCurrency = msgResponse.currency().c_str();
	m_unSteamLevel = msgResponse.steam_level();
	m_unFriendCount = msgResponse.friend_count();
	m_rtimeAccountCreated = msgResponse.account_creation_time();
	m_bIsSteamGuardEnabled = msgResponse.is_steamguard_enabled();
	m_bIsPhoneVerified = msgResponse.is_phone_verified();
	m_bIsTwoFactorAuthEnabled = msgResponse.is_two_factor_auth_enabled();
	m_bIsPhoneIdentifying = msgResponse.is_phone_identifying();
	m_rtimeTwoFactorEnabled = msgResponse.two_factor_enabled_time();
	m_rtimePhoneVerified = msgResponse.phone_verification_time();
	m_unPhoneID = msgResponse.phone_id();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if it's time to remove this entry from the cache
//-----------------------------------------------------------------------------
bool CAccountDetails::BIsExpired() const
{
	int nCacheSeconds = BIsValid() ? cv_account_details_cache_time.GetInt() : cv_account_details_failure_cache_time.GetInt();

	return m_rtimeCached + nCacheSeconds < CRTime::RTime32TimeCur();
}


//-----------------------------------------------------------------------------
// Purpose: Reverts this to an invalid record
//-----------------------------------------------------------------------------
void CAccountDetails::Reset()
{
	m_bValid = false;
	m_rtimeCached = CRTime::RTime32TimeCur();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Claims all the memory for the AccountDetails object
//-----------------------------------------------------------------------------
void CAccountDetails::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_sAccountName );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: Sends a message to Steam to get a CAccountDetails object
//-----------------------------------------------------------------------------
class CGCJobSendGetAccountDetailsRequest : public CGCJob
{
	CAccountDetailsManager *m_pManager;
	CSteamID m_SteamID;

public:
	CGCJobSendGetAccountDetailsRequest( CGCBase *pGC, CAccountDetailsManager *pManager, const CSteamID &steamID )
		: CGCJob( pGC ), m_pManager( pManager ), m_SteamID( steamID ) {}
	virtual bool BYieldingRunGCJob()
	{
		// Yield immediately to be sure that the calling job gets in the wakeup list
		BYield();

		// These requests should come back very quickly, so if they don't we shouldn't wait very long
		// jamming up the system
		SetJobTimeout( account_details_timeout.GetInt() );

		// Get an empty account details object
		CAccountDetails *pAccount = m_pManager->m_hashAccountDetailsCache.PvRecordFind( m_SteamID.GetAccountID() );
		if ( NULL == pAccount )
		{
			pAccount = m_pManager->m_hashAccountDetailsCache.PvRecordInsert( m_SteamID.GetAccountID() );
		}
		else
		{
			// If the record isn't expired, why is it there?
			Assert( pAccount->BIsExpired() );
			pAccount->Reset();
		}		

		CProtoBufMsg< CGCSystemMsg_GetAccountDetails > msgReqeust( k_EGCMsgGetAccountDetails );
		CProtoBufMsg< CGCSystemMsg_GetAccountDetails_Response > msgReply;
		msgReqeust.Body().set_steamid( m_SteamID.ConvertToUint64() );
		msgReqeust.Body().set_appid( GGCBase()->GetAppID() );
		msgReqeust.ExpectingReply( GJobCur().GetJobID() );

		// try to get the account details at most 2 times
		const int kMaxTries = 2;
		for ( int iTries = 0; iTries < kMaxTries; iTries++ )
		{
			if( !m_pGC->BSendSystemMessage( msgReqeust ) )
			{
				EmitWarning( SPEW_GC, 2, "Unable to send GetAccountDetails system message\n" );
				continue;
			}

			// All of our request messages are identical, so if we get our replies
			// mixed up, it's OK.  Bypass the system used to protect us against
			// mismatched replies.
			ClearFailedToReceivedMsgType( k_EGCMsgGetAccountDetailsResponse );

			// Wait for the reply
			if( !BYieldingWaitForMsg( &msgReply, k_EGCMsgGetAccountDetailsResponse ) )
			{
				EmitWarning( SPEW_GC, 2, "Timeout waiting for GetAccountDetails reply for SteamID %s\n", m_SteamID.Render() );
				continue;
			}

			if ( k_EResultOK != msgReply.GetEResult() )
			{
				EmitInfo( SPEW_GC, 4, 4, "GetAccountDetails request failed with result %d for SteamID %s\n", msgReply.GetEResult(), m_SteamID.Render() );
				break;
			}

			Assert( msgReply.Body().eresult_deprecated() == k_EResultOK );

			// Sanity check the response
			if ( msgReply.Body().has_accountid() && msgReply.Body().accountid() != m_SteamID.GetAccountID() )
			{
				static bool bHasAlerted = false;
				if ( !bHasAlerted )
				{
					GGCBase()->PostAlert( k_EAlertTypeInfo, true, CFmtStr( "GetAccountDetails got a response for account %d, but we were expecting a response for account %s\n", msgReply.Body().accountid(), m_SteamID.Render() ) );
					bHasAlerted = true;
				}

				EmitError( SPEW_GC, "GetAccountDetails got a response for account %d, but we were expecting a response for account %s\n", msgReply.Body().accountid(), m_SteamID.Render() );
				break;
			}

			// All responses should have this
			if ( !msgReply.Body().has_account_name() )
			{
				EmitError( SPEW_GC, "GetAccountDetails got a response with missing fields for SteamID %s\n", m_SteamID.Render() );
				break;
			}

			pAccount->Init( msgReply.Body() );
			m_pManager->CachePersonaName( CSteamID( m_SteamID ), msgReply.Body().persona_name().c_str() );

			// We got a response, so we shouldn't try again
			break;
		}

		m_pManager->WakeWaitingAccountDetailsJobs( m_SteamID );
		return true;
	}
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCachedPersonaName::CCachedPersonaName()
	:	m_rtimeCached( 0 )	// This initialization value is important because it makes it expired on creation, 
							// and the rest of the code will only ask Steam for a name if the cached entry is expired
	,	m_nLoading( 0 )
	,	m_bPreloading( false )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCachedPersonaName::~CCachedPersonaName()
{
	Assert( !BIsLoading() );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the newly retrieved persona name
//-----------------------------------------------------------------------------
void CCachedPersonaName::Init( const char *pchPersonaName )
{
	m_sPersonaName = pchPersonaName;
	m_rtimeCached = CRTime::RTime32TimeCur();
	m_bPreloading = false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if it's time to remove this entry from the cache
//-----------------------------------------------------------------------------
bool CCachedPersonaName::BIsExpired() const
{
	int nCacheSeconds = BIsValid() ? cv_persona_name_cache_time.GetInt() : cv_persona_name_failure_cache_time.GetInt();

	return m_rtimeCached + nCacheSeconds < CRTime::RTime32TimeCur();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if there is an actual cached name
//-----------------------------------------------------------------------------
bool CCachedPersonaName::BIsValid() const
{
	return !m_sPersonaName.IsEmpty();
}


//-----------------------------------------------------------------------------
// Purpose: Reverts this to an invalid record
//-----------------------------------------------------------------------------
void CCachedPersonaName::Reset()
{
	m_sPersonaName.Clear();
	m_rtimeCached = CRTime::RTime32TimeCur();
	m_bPreloading = false;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the cached string
//-----------------------------------------------------------------------------
const char *CCachedPersonaName::GetPersonaName() const
{
	return BIsValid() ? m_sPersonaName.Get() : "[unknown]";
}


//-----------------------------------------------------------------------------
// Purpose: Returns if this name is loading
//-----------------------------------------------------------------------------
bool CCachedPersonaName::BIsLoading() const
{
	return m_nLoading > 0 || m_bPreloading;
}


//-----------------------------------------------------------------------------
// Purpose: Sets that this name has been preloaded
//-----------------------------------------------------------------------------
void CCachedPersonaName::SetPreloading()
{
	m_bPreloading = true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets that a job is waiting for this name to be loaded
//-----------------------------------------------------------------------------
void CCachedPersonaName::AddLoadingRef()
{
	m_nLoading++;
}


//-----------------------------------------------------------------------------
// Purpose: Releases the loading ref
//-----------------------------------------------------------------------------
void CCachedPersonaName::ReleaseLoadingRef()
{
	DbgVerify( --m_nLoading >= 0 );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Claims all the memory for the CCachedPersonaName object
//-----------------------------------------------------------------------------
void CCachedPersonaName::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_sPersonaName );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: Sends a message to Steam to get a CAccountDetails object
//-----------------------------------------------------------------------------
class CGCJobSendGetPersonaNamesRequest : public CGCJob
{
	CAccountDetailsManager *m_pManager;
	CUtlVector<CSteamID> m_vecSteamIDs;

public:
	CGCJobSendGetPersonaNamesRequest( CGCBase *pGC, CAccountDetailsManager *pManager, CUtlVector<CSteamID> &vecSteamIDs )
		: CGCJob( pGC ), m_pManager( pManager )
	{
		m_vecSteamIDs.Swap( vecSteamIDs );
	}

	virtual bool BYieldingRunGCJob()
	{
		// Yield immediately to be sure that the calling job gets in the wakeup list
		BYield();

		// These requests should come back very quickly, so if they don't we shouldn't wait very long
		// jamming up the system
		SetJobTimeout( persona_name_timeout.GetInt() );

		CProtoBufMsg< CMsgGCGetPersonaNames > msgReqeust( k_EGCMsgGetPersonaNames );
		msgReqeust.ExpectingReply( GJobCur().GetJobID() );

		FOR_EACH_VEC( m_vecSteamIDs, i )
		{
			msgReqeust.Body().add_steamids( m_vecSteamIDs[i].ConvertToUint64() );
		}
		
		CProtoBufMsg< CMsgGCGetPersonaNames_Response > msgReply;
		if( !m_pGC->BSendSystemMessage( msgReqeust ) ||
			!BYieldingWaitForMsg( &msgReply, k_EGCMsgGetPersonaNamesResponse ) )
		{
			FOR_EACH_VEC( m_vecSteamIDs, i )
			{
				m_pManager->CachePersonaNameFailure( m_vecSteamIDs[i] );
			}

			//if we are shutting down, don't bother reporting this issue, we know we won't be able to get persona names
			if( !GGCBase()->GetIsShuttingDown() )
			{
				EmitWarning( SPEW_GC, 2, "GetPersonaNames request failed for %d IDs:", m_vecSteamIDs.Count() );
				FOR_EACH_VEC( m_vecSteamIDs, nID )
				{
					EmitWarning( SPEW_GC, 2, " %s", m_vecSteamIDs[ nID ].Render() );
				}
				EmitWarning( SPEW_GC, 2, "\n" );
			}
		}
		else
		{
			for ( int i = 0; i < msgReply.Body().succeeded_lookups_size(); i++ )
			{
				const CMsgGCGetPersonaNames_Response_PersonaName &result = msgReply.Body().succeeded_lookups( i );
				m_pManager->CachePersonaName( CSteamID( result.steamid() ), result.persona_name().c_str() );
			}
			for ( int i = 0; i < msgReply.Body().failed_lookup_steamids_size(); i++ )
			{
				m_pManager->CachePersonaNameFailure( CSteamID( msgReply.Body().failed_lookup_steamids( i ) ) );
			}
		}

		FOR_EACH_VEC( m_vecSteamIDs, i )
		{
			m_pManager->WakeWaitingPersonaNameJobs( m_vecSteamIDs[i] );
		}
		return true;
	}
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAccountDetailsManager::CAccountDetailsManager()
	: m_hashAccountDetailsCache( k_nAccountDetailsRunInterval / k_cMicroSecPerShellFrame )
	, m_hashPersonaNameCache( k_nAccountDetailsRunInterval / k_cMicroSecPerShellFrame )
{
	m_hashAccountDetailsCache.Init( k_cAccountDetailsInit, k_cBucketAccountDetails );
	m_hashPersonaNameCache.Init( k_cAccountDetailsInit, k_cBucketAccountDetails );
}


//-----------------------------------------------------------------------------
// Purpose: Work to be done once per frame
//-----------------------------------------------------------------------------
void CAccountDetailsManager::MarkFrame()
{
	m_hashAccountDetailsCache.StartFrameSchedule( true );
	m_hashPersonaNameCache.StartFrameSchedule( true );
	SendBatchedPersonaNamesRequest();
}


//-----------------------------------------------------------------------------
// Purpose: Gets a CAccountDetails object
//-----------------------------------------------------------------------------
CAccountDetails *CAccountDetailsManager::YieldingGetAccountDetails( const CSteamID &steamID, bool bForceReload )
{
	AssertRunningJob();
	if( !steamID.IsValid() || !steamID.BIndividualAccount() )
		return NULL;

	// Check the local cache
	CAccountDetails *pAccountDetails = NULL;
	if ( BFindAccountDetailsInLocalCache( steamID, &pAccountDetails ) )
	{
		if ( pAccountDetails && bForceReload )
		{
			// Clear it, continue with fresh load
			m_hashAccountDetailsCache.Remove( pAccountDetails );
		}
		else
		{
			return pAccountDetails;
		}
	}

	// Not in the local cache, ask Steam
	int iMapIndex = m_mapQueuedAccountDetailsRequests.Find( steamID );
	if ( !m_mapQueuedAccountDetailsRequests.IsValidIndex( iMapIndex ) )
	{
		iMapIndex = m_mapQueuedAccountDetailsRequests.Insert( steamID );
		CGCJobSendGetAccountDetailsRequest *pJob = new CGCJobSendGetAccountDetailsRequest( GGCBase(), this, steamID );
		pJob->StartJob( NULL );
	}

	m_mapQueuedAccountDetailsRequests[iMapIndex].AddToTail( GJobCur().GetJobID() );
	GJobCur().BYieldingWaitForWorkItem();

	// Check again, if it's not there then it's not there
	BFindAccountDetailsInLocalCache( steamID, &pAccountDetails );
	return pAccountDetails;
}


//-----------------------------------------------------------------------------
// Purpose: Finds an AccountDetails object in the local cache. Returns true
//	if it was found, false if it should be checked remotely
//-----------------------------------------------------------------------------
bool CAccountDetailsManager::BFindAccountDetailsInLocalCache( const CSteamID &steamID, CAccountDetails **ppAccount )
{
	CAccountDetails *pAccountLocal = m_hashAccountDetailsCache.PvRecordFind( steamID.GetAccountID() );
	if( NULL == pAccountLocal || pAccountLocal->BIsExpired() )
		return false;

	*ppAccount = pAccountLocal->BIsValid() ? pAccountLocal : NULL;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Wakes any jobs waiting on this result
//-----------------------------------------------------------------------------
void CAccountDetailsManager::WakeWaitingAccountDetailsJobs( const CSteamID &steamID )
{
	int iMapIndex = m_mapQueuedAccountDetailsRequests.Find( steamID );
	if ( !m_mapQueuedAccountDetailsRequests.IsValidIndex( iMapIndex ) )
		return; 

	CCopyableUtlVector<JobID_t> &vecJobsWaiting = m_mapQueuedAccountDetailsRequests[iMapIndex];
	FOR_EACH_VEC( vecJobsWaiting, i )
	{
		GGCBase()->GetJobMgr().BRouteWorkItemCompletedDelayed( vecJobsWaiting[i], false );
	}
	m_mapQueuedAccountDetailsRequests.RemoveAt( iMapIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Gets a persona name for a user
//-----------------------------------------------------------------------------
const char *CAccountDetailsManager::YieldingGetPersonaName( const CSteamID &steamID )
{
	VPROF_BUDGET( "CAccountDetailsManager::YieldingGetPersonaName", VPROF_BUDGETGROUP_STEAM );

	AssertRunningJob();

	if( !steamID.IsValid() || !steamID.BIndividualAccount() )
		return "[unknown]";

	// Check the local cache
	CCachedPersonaName *pPersonaName = FindOrCreateCachedPersonaName( steamID );
	if ( !pPersonaName->BIsExpired() )
		return pPersonaName->GetPersonaName(); 

	// Not in the local cache, ask Steam
	pPersonaName->AddLoadingRef();

	// Queue the request and start a lookup job if we have enough pending
	int iMapIndex = m_mapQueuedPersonaNameRequests.Find( steamID );
	if ( !m_mapQueuedPersonaNameRequests.IsValidIndex( iMapIndex ) )
	{
		iMapIndex = m_mapQueuedPersonaNameRequests.Insert( steamID );
		m_vecPendingPersonaNameLookups.AddToTail( steamID );
		if ( m_vecPendingPersonaNameLookups.Count() >= cv_persona_name_batch_size.GetInt() )
		{
			SendBatchedPersonaNamesRequest();
		}
	}

	m_mapQueuedPersonaNameRequests[iMapIndex].AddToTail( GJobCur().GetJobID() );
	GJobCur().BYieldingWaitForWorkItem();

	// At this point we'll either have a persona name or we won't
	pPersonaName->ReleaseLoadingRef();
	return pPersonaName->GetPersonaName();
}


//-----------------------------------------------------------------------------
// Purpose: Let's the system know that we should load the persona name for this
//	user, but does not block on it
//-----------------------------------------------------------------------------
void CAccountDetailsManager::PreloadPersonaName( const CSteamID &steamID )
{
	if( !steamID.IsValid() || !steamID.BIndividualAccount() )
		return;

	// See if we already have it
	CCachedPersonaName *pCachedName = FindOrCreateCachedPersonaName( steamID );
	if ( !pCachedName->BIsExpired() || pCachedName->BIsLoading() )
		return;

	// Queue the request and start a lookup job if we have enough pending
	pCachedName->SetPreloading();
	m_mapQueuedPersonaNameRequests.Insert( steamID );
	m_vecPendingPersonaNameLookups.AddToTail( steamID );
	if ( m_vecPendingPersonaNameLookups.Count() >= cv_persona_name_batch_size.GetInt() )
	{
		SendBatchedPersonaNamesRequest();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sends a batch of persona name requests
//-----------------------------------------------------------------------------
void CAccountDetailsManager::SendBatchedPersonaNamesRequest()
{
	if ( 0 == m_vecPendingPersonaNameLookups.Count() )
		return;

	// Start the job. This swaps out our buffer with an empty one
	CGCJobSendGetPersonaNamesRequest *pJob = new CGCJobSendGetPersonaNamesRequest( GGCBase(), this, m_vecPendingPersonaNameLookups );
	pJob->StartJob( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Caches a persona name
//-----------------------------------------------------------------------------
void CAccountDetailsManager::CachePersonaName( const CSteamID &steamID, const char *pchPersonaName )
{
	CCachedPersonaName *pCachedPersonaName = FindOrCreateCachedPersonaName( steamID );
	pCachedPersonaName->Init( pchPersonaName );
}


//-----------------------------------------------------------------------------
// Purpose: Remembers that we failed to cache a persona name
//-----------------------------------------------------------------------------
void CAccountDetailsManager::CachePersonaNameFailure( const CSteamID &steamID )
{
	CCachedPersonaName *pCachedPersonaName = FindOrCreateCachedPersonaName( steamID );
	pCachedPersonaName->Reset();
}


//-----------------------------------------------------------------------------
// Purpose: Purges a specific persona name from the cache
//-----------------------------------------------------------------------------
void CAccountDetailsManager::ClearCachedPersonaName( const CSteamID &steamID )
{
	CCachedPersonaName *pPersonaNameLocal = m_hashPersonaNameCache.PvRecordFind( steamID.GetAccountID() );
	if( NULL != pPersonaNameLocal && !pPersonaNameLocal->BIsLoading() )
	{
		m_hashPersonaNameCache.Remove( pPersonaNameLocal );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets a CCachedPersonaName record
//-----------------------------------------------------------------------------
CCachedPersonaName *CAccountDetailsManager::FindOrCreateCachedPersonaName( const CSteamID &steamID )
{
	CCachedPersonaName *pPersonaName = m_hashPersonaNameCache.PvRecordFind( steamID.GetAccountID() );
	if ( NULL != pPersonaName )
		return pPersonaName;
	else
		return m_hashPersonaNameCache.PvRecordInsert( steamID.GetAccountID() );
}


//-----------------------------------------------------------------------------
// Purpose: Wakes any jobs waiting on this result
//-----------------------------------------------------------------------------
void CAccountDetailsManager::WakeWaitingPersonaNameJobs( const CSteamID &steamID )
{
	int iMapIndex = m_mapQueuedPersonaNameRequests.Find( steamID );
	if ( !m_mapQueuedPersonaNameRequests.IsValidIndex( iMapIndex ) )
		return; 

	CCopyableUtlVector<JobID_t> &vecJobsWaiting = m_mapQueuedPersonaNameRequests[iMapIndex];
	FOR_EACH_VEC( vecJobsWaiting, i )
	{
		GGCBase()->GetJobMgr().BRouteWorkItemCompletedDelayed( vecJobsWaiting[i], false );
	}
	m_mapQueuedPersonaNameRequests.RemoveAt( iMapIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Purges old data from the cache. Returns true if there is more
//	work to do
//-----------------------------------------------------------------------------
bool CAccountDetailsManager::BExpireRecords( CLimitTimer &limitTimer )
{
	VPROF_BUDGET( "Expire account details", VPROF_BUDGETGROUP_STEAM );

	for ( CAccountDetails *pDetails = m_hashAccountDetailsCache.PvRecordRun(); NULL != pDetails; pDetails = m_hashAccountDetailsCache.PvRecordRun() )
	{
		if ( pDetails->BIsExpired() )
		{
			m_hashAccountDetailsCache.Remove( pDetails );
		}

		if ( limitTimer.BLimitReached() )
			return true;
	}

	for ( CCachedPersonaName *pPersonaName = m_hashPersonaNameCache.PvRecordRun(); NULL != pPersonaName; pPersonaName = m_hashPersonaNameCache.PvRecordRun() )
	{
		if ( pPersonaName->BIsExpired() && !pPersonaName->BIsLoading() )
		{
			m_hashPersonaNameCache.Remove( pPersonaName );
		}

		if ( limitTimer.BLimitReached() )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Prints status
//-----------------------------------------------------------------------------
void CAccountDetailsManager::Dump() const
{
	int nJobsWaiting = 0;
	FOR_EACH_MAP_FAST( m_mapQueuedAccountDetailsRequests, iMap )
	{
		nJobsWaiting += m_mapQueuedAccountDetailsRequests[iMap].Count();
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tAccount Details: %d cached, %d lookups in flight, %d jobs waiting\n", m_hashAccountDetailsCache.Count(), m_mapQueuedAccountDetailsRequests.Count(), nJobsWaiting );

	nJobsWaiting = 0;
	FOR_EACH_MAP_FAST( m_mapQueuedPersonaNameRequests, iMap )
	{
		nJobsWaiting += m_mapQueuedPersonaNameRequests[iMap].Count();
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tPersona Names: %d cached, %d lookups in flight, %d jobs waiting\n", m_hashPersonaNameCache.Count(), m_mapQueuedPersonaNameRequests.Count(), nJobsWaiting );
}

} // namespace GCSDK
