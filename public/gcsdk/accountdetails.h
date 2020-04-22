//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CAccountDetails class.
//
//=============================================================================

#ifndef ACCOUNTDETAILS_H
#define ACCOUNTDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/thash.h"
#include "tier1/utlhashmaplarge.h"

namespace GCSDK
{

class CAccountDetails
{
public:
	CAccountDetails();

	void Init( CGCSystemMsg_GetAccountDetails_Response &msgResponse );
	void Reset();
	bool BIsExpired() const;
	bool BIsValid() const { return m_bValid; }

	const char *GetAccountName() const { return m_sAccountName.Get(); }
	bool BHasPublicProfile() const { return m_bPublicProfile; }
	bool BHasPublicInventory() const { return m_bPublicInventory; }
	bool BIsVacBanned() const { return m_bVacBanned; }
	bool BIsCyberCafe() const { return m_bCyberCafe; }
	bool BIsSchoolAccount() const { return m_bSchoolAccount; }
	bool BIsFreeTrialAccount() const { return m_bFreeTrialAccount; }
	bool BIsFreeTrialAccountOrDemo() const { return m_bFreeTrialAccount || m_unPackage == 0; }
	bool BIsSubscribed() const { return m_bSubscribed; }
	bool BIsLowViolence() const { return m_bLowViolence; }
	bool BIsLimitedAccount() const { return m_bLimited; }
	bool BIsAccountLocked() const { return m_bAccountLocked; }
	bool BIsCommunityBanned() const { return m_bCommunityBanned; }
	bool BIsTradeBanned() const { return m_bTradeBanned; }
	bool BIsSteamGuardEnabled() const { return m_bIsSteamGuardEnabled; }
	bool BIsPhoneVerified() const { return m_bIsPhoneVerified; }
	bool BIsTwoFactorAuthEnabled() const { return m_bIsTwoFactorAuthEnabled; }
	bool BIsPhoneIdentifying() const { return m_bIsPhoneIdentifying; }
	uint32  GetPackage() const { return m_unPackage; }
	RTime32 GetTimeVACBanEnd() const { return m_rtimeVACBanEnd; }
	uint32 GetSteamLevel() const { return m_unSteamLevel; }
 	uint32 GetFriendCount() const { return m_unFriendCount; }
	RTime32 GetTimeAccountCreated() const { return m_rtimeAccountCreated; }
	RTime32 GetTimeTwoFactorEnabled() const { return m_rtimeTwoFactorEnabled; }
	RTime32 GetTimePhoneVerified() const { return m_rtimePhoneVerified; }
	uint64 GetPhoneID() const { return m_unPhoneID; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	CUtlConstString m_sAccountName;
	CUtlConstString m_sCurrency;
	RTime32 m_rtimeCached;
	uint32 m_unPackage;
	RTime32 m_rtimeVACBanEnd;
	uint32 m_unSteamLevel;
	uint32 m_unFriendCount;
	RTime32 m_rtimeAccountCreated;
	RTime32 m_rtimeTwoFactorEnabled;
	RTime32 m_rtimePhoneVerified;
	uint64 m_unPhoneID;
	bool 
		m_bValid:1,
		m_bPublicProfile:1,
		m_bPublicInventory:1,
		m_bVacBanned:1,
		m_bCyberCafe:1,
		m_bSchoolAccount:1,
		m_bFreeTrialAccount:1,
		m_bSubscribed:1,
		m_bLowViolence:1,
		m_bLimited:1,
		m_bAccountLocked:1,
		m_bCommunityBanned:1,
		m_bTradeBanned:1,
		m_bIsSteamGuardEnabled:1,
		m_bIsPhoneVerified:1,
		m_bIsTwoFactorAuthEnabled:1,
		m_bIsPhoneIdentifying:1;
};


class CCachedPersonaName
{
public:
	CCachedPersonaName();
	~CCachedPersonaName();

	void Init( const char *pchPersonaName );
	void Reset();
	bool BIsExpired() const;
	bool BIsValid() const;

	bool BIsLoading() const;
	void SetPreloading();
	void AddLoadingRef();
	void ReleaseLoadingRef();

	const char *GetPersonaName() const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	CUtlConstString m_sPersonaName;
	RTime32 m_rtimeCached;
	int32 m_nLoading;
	bool  m_bPreloading;
};


//-----------------------------------------------------------------------------
// Purpose: Manages requests for CAccountDetails objects
//-----------------------------------------------------------------------------
class CAccountDetailsManager
{
public:
	CAccountDetailsManager();
	CAccountDetails *YieldingGetAccountDetails( const CSteamID &steamID, bool bForceReload = false );

	void PreloadPersonaName( const CSteamID &steamID );
	const char *YieldingGetPersonaName( const CSteamID &steamID );
	void ClearCachedPersonaName( const CSteamID &steamID );

	void MarkFrame();
	bool BExpireRecords( CLimitTimer &limitTimer );

	void Dump() const;

private:
	friend class CGCJobSendGetAccountDetailsRequest;

	bool BFindAccountDetailsInLocalCache( const CSteamID &steamID, CAccountDetails **ppAccount );
	void WakeWaitingAccountDetailsJobs( const CSteamID &steamID );

	CTHash<CAccountDetails, uint32> m_hashAccountDetailsCache;
	CUtlHashMapLarge<CSteamID, CCopyableUtlVector<JobID_t> > m_mapQueuedAccountDetailsRequests;

	friend class CGCJobSendGetPersonaNamesRequest;

	void SendBatchedPersonaNamesRequest();
	CCachedPersonaName *FindOrCreateCachedPersonaName( const CSteamID &steamID );
	void CachePersonaName( const CSteamID &steamID, const char *pchPersonaName );
	void CachePersonaNameFailure( const CSteamID &steamID );
	void WakeWaitingPersonaNameJobs( const CSteamID &steamID );

	CUtlVector<CSteamID> m_vecPendingPersonaNameLookups;
	CTHash<CCachedPersonaName, uint32> m_hashPersonaNameCache;
	CUtlHashMapLarge<CSteamID, CCopyableUtlVector<JobID_t> > m_mapQueuedPersonaNameRequests;
};


} // namespace GCSDK
#endif // ACCOUNTDETAILS_H
