//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef GCBASE_H
#define GCBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "gamecoordinator/igamecoordinator.h"
#include "gamecoordinator/igamecoordinatorhost.h"
#include "tier1/utlallocation.h"
#include "gcmsg.h"
#include "jobmgr.h"
#include "tier1/thash.h"
#include "tier1/UtlSortVector.h"
#include "http.h"
#include "language.h"
#include "accountdetails.h"
#include "gcsession.h"

class CGCMsgGetSystemStatsResponse;

extern EUniverse GetUniverse();

const int16 k_nLockTypeLobby             =   3;
const int16 k_nLockTypeParty             =   2;
const int16	k_nLockTypeIndividual        =   1;
const int16	k_nLockTypeGameServer        =   0;
const int16 k_nLockTypeGroupIDGeneration =  -1;
 // For one-off locks of specific resources. The specific game controls the subtype ordering.
const int16	k_nLockTypeGameMisc          = -10;

namespace GCSDK
{
class CGCSession;
class CGCUserSession;
class CGCGSSession;
class CGCSharedObjectCache;
class CSharedObject;
class CAccountDetails;
class CScopedSteamIDLock;

struct PackageLicense_t
{
	uint32 m_unPackageID;
	RTime32 m_rtimeCreated;
};

#define SERVER_KEY_HASH 0x5a4f4944u

class CGCBase : public IGameCoordinator
{
public:
	CGCBase( );
	virtual ~CGCBase();

	// Hooks to extend the base behaviors of the IGameCoordinator interface
	virtual bool OnInit() { return true; }
	virtual bool OnMainLoopOncePerFrame( CLimitTimer &limitTimer ) { return false; }
	virtual bool OnMainLoopUntilFrameCompletion( CLimitTimer &limitTimer ) { return false; }
	virtual void OnUninit() {}
	// returns true if this function handled the message
	virtual bool OnMessageFromClient( const CSteamID & senderID, uint32 unMsgType, void *pubData, uint32 cubData ) { return false; }
	virtual void OnValidate( CValidator &validator, const char *pchName ) {}
	virtual const char *LocalizeToken( const char *pchToken, ELanguage eLanguage, bool bReturnTokenIfNotFound = true ) { return bReturnTokenIfNotFound ? pchToken : NULL; }

	virtual void YldOnAccountPhoneVerificationChange( CSteamID steamID ) {}
	virtual void YldOnAccountTwoFactorChange( CSteamID steamID ) {}

	/// The main loop calls Run() on game servers and user sessions until a certain amount of time
	/// is expired.  Thus the user list is iterated, but the iteration is amortized over multiple frames.
	/// this function is called when a sweep of the user sessions ends and a new one is about to begin.
	virtual void FinishedMainLoopUserSweep();

	// Life cycle management functions

	// Called to do any yielding initialization work before reporting as fully operational
	virtual bool BYieldingFinishStartup() = 0;

	// Called to do any yielding work immediately after becoming full operational
	virtual bool BYieldingPostStartup() = 0;

	// Call to report that we're fully operational
	void SetStartupComplete( bool bSuccess );

	// Called to do any yielding work before reporting as ready to shutdown
	virtual void YieldingGracefulShutdown() = 0;


	virtual CGCUserSession *CreateUserSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache ) const;
	virtual CGCGSSession *CreateGSSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache, uint32 unServerAddr, uint16 usServerPort ) const;
	virtual void YieldingSessionStartPlaying( CGCUserSession *pSession ) {}
	virtual void YieldingSessionStopPlaying( CGCUserSession *pSession ) {}
	virtual void YieldingSessionStartServer( CGCGSSession *pSession ) {}
	virtual void YieldingSessionStopServer( CGCGSSession *pSession ) {}
	virtual void YieldingSOCacheLoaded( CGCSharedObjectCache *pSOCache );
	virtual void UpdateGSSessionAddress( CGCGSSession *pSession, uint32 unServerAddr, uint16 usServerPort );

	virtual void YieldingPreTestSetup()	{}

	// cache management
	CGCSharedObjectCache *YieldingGetLockedSOCache( const CSteamID &steamID, const char *pszFilename, int nLineNum );
	CGCSharedObjectCache *YieldingFindOrLoadSOCache( const CSteamID &steamID );
	CGCSharedObjectCache *FindSOCache( const CSteamID & steamID );					// non-yielding, but may return NULL if the cache exists but is not loaded
	bool UnloadUnusedCaches( uint32 unMaxCacheCount, CLimitTimer *pLimitTimer = NULL );
	void YieldingReloadCache( CGCSharedObjectCache *pSOCache );
	virtual CGCSharedObjectCache *CreateSOCache( const CSteamID &steamID );
	void FlushInventoryCache( AccountID_t unAccountID );

	CGCUserSession *YieldingGetLockedUserSession( const CSteamID & steamID, const char *pszFilename, int nLineNum );
	CGCUserSession *FindUserSession( const CSteamID & steamID ) const;
	bool BUserSessionPending( const CSteamID & steamID ) const;

	CGCGSSession *YieldingGetLockedGSSession( const CSteamID & steamID, const char *pszFilename, int nLineNum );
	CGCGSSession *YieldingFindOrCreateGSSession( const CSteamID & steamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData );
	CGCGSSession *FindGSSession( const CSteamID & steamID ) const;

	/// Lookup the user or GS session, depending on whether the supplied
	/// Steam ID is an individual or gameserver ID
	CGCSession *FindUserOrGSSession( const CSteamID & steamID ) const;

	CGCSession *YieldingRequestSession( const CSteamID & steamID );
	bool BYieldingIsOnline( const CSteamID & steamID );
	bool BSendWebApiRegistration();

	// Will return true, indicating the request was sent, a response was received, and the response had a valid
	// status code, or false, indicating that there was a transport-layer problem -- the request timed out, or
	// a response came back with an invalid status code, etc. All "false" return values are meant to indicate
	// temporary errors.
	bool BYieldingSendHTTPRequest( const CHTTPRequest *pRequest, CHTTPResponse *pResponse );

	// Possible return values:
	//		k_EResultOK					-- everything went fine and pkvResponse has been populated.
	//		k_EResultTimeout			-- there was a temporary/transport-layer problem (see "BYieldingSendHTTPRequest").
	//									   These are temporary errors, or programming errors on our side.
	//		k_EResultRemoteCallFailed	-- we didn't timeout but we got a bad, unparseable, or otherwise invalid response
	//									   back and so we don't have data we can use.
	//		k_EResultFail				-- something has gone catastrophically wrong and no forward progress can be
	//									   expected to be made. May or may not ever be returned.
	EResult YieldingSendHTTPRequestKV( const CHTTPRequest *pRequest, KeyValues *pkvResponse );

	int GetSOCacheCount() const;
	bool IsSOCached( const CSharedObject *pObj, uint32 nTypeID ) const;

	int GetUserSessionCount() const;
	int GetGSSessionCount() const;

	void SetIsShuttingDown();
	bool GetIsShuttingDown() const { return m_bIsShuttingDown; }

	// Iterate over sessions
	// WARNING: Don't yield between GetFirst*/GetNext*.  Use caution when using
	// these any time you might have tens of thousands of sessions to iterate through.
	CGCUserSession **GetFirstUserSession()								{ return m_hashUserSessions.PvRecordFirst(); }
	CGCUserSession **GetNextUserSession( CGCUserSession **pUserSession )		{ return m_hashUserSessions.PvRecordNext( pUserSession ); }
	CGCGSSession **GetFirstGSSession()								{ return m_hashGSSessions.PvRecordFirst(); }
	CGCGSSession **GetNextGSSession( CGCGSSession **pGSSession )		{ return m_hashGSSessions.PvRecordNext( pGSSession ); }

	AppId_t GetAppID() const { return m_unAppID; }
	bool BIsStartupComplete() const { return m_bStartupComplete; }
	CJobMgr &GetJobMgr() { return m_JobMgr; }

	CSteamID YieldingGuessSteamIDFromInput( const char *pchInput );
	bool BYieldingRecordSupportAction( const CSteamID & actorID, const CSteamID & targetID, const char *pchData, const char *pchNote );
	void PostAlert( EAlertType eAlertType, bool bIsCritical, const char *pchAlertText, const CUtlVector< CUtlString > *pvecExtendedInfo = NULL, bool bAlsoSpew = true );
	const CAccountDetails *YieldingGetAccountDetails( const CSteamID & steamID, bool bForceReload = false );
	const char *YieldingGetPersonaName( const CSteamID & steamID, const char *pchUnknownName = NULL );
	void PreloadPersonaName( const CSteamID & steamID );
	void ClearCachedPersonaName( const CSteamID & steamID );
	bool BYieldingGetAccountLicenses( const CSteamID & steamID, CUtlVector< PackageLicense_t > & vecPackages );
	bool BYieldingLookupAccount( EAccountFindType eFindType, const char *pchInput, CUtlVector< CSteamID > *prSteamIDs );
	bool BYieldingAddFreeLicense( const CSteamID & steamID, uint32 unPackageID, uint32 unIPPublic = 0, const char *pchStoreCountryCode = NULL );
	int  YieldingGrantGuestPass( const CSteamID & steamID, uint32 unPackageID, uint32 unPassesToGrant = 1, int32 nDaysToExpiration = -1 );

	bool BSendGCMsgToClient( const CSteamID & steamIDTarget, const CGCMsgBase& msg );
	bool BSendGCMsgToClient( const CSteamID & steamIDTarget, const CProtoBufMsgBase& msg );

	//sends a message to the system (GCH or GC.exe)
	bool BSendSystemMessage( const CGCMsgBase& msg, uint32 *pcubSent = NULL );
	bool BSendSystemMessage( const CProtoBufMsgBase& msg, uint32 *pcubSent = NULL );
	bool BSendSystemMessage( const ::google::protobuf::Message &msgOut, MsgType_t eSendMsg );

	//utilities that will send a message and then wait for the specified reply. This will return false if no reply is sent in the default timeout, or the message or status 
	//don't match what is expected. This can only be called from within a job
	bool BYldSendMessageAndGetReply( const CSteamID &steamIDTarget, CProtoBufMsgBase &msgOut, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg );
	//bool BYldSendGCMessageAndGetReply( int32 nGCDirIndex, CProtoBufMsgBase &msgOut, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg );
	bool BYldSendSystemMessageAndGetReply( CGCMsgBase &msgOut, CGCMsgBase *pMsgIn, MsgType_t eMsg );
	bool BYldSendSystemMessageAndGetReply( CProtoBufMsgBase &msgOut, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg );
	bool BYldSendSystemMessageAndGetReply( const ::google::protobuf::Message &msgSend, MsgType_t eSendMsg, ::google::protobuf::Message *pMsgResponse, MsgType_t eRespondMsg );

	bool BReplyToMessage( CGCMsgBase &msgOut, const CGCMsgBase &msgIn );
	bool BReplyToMessage( CProtoBufMsgBase &msgOut, const CProtoBufMsgBase &msgIn );

	//sending messages to clients or as responses with pre-serialized bodies so that the work to generate the message and body doesn't have to be done multiple times
	bool BSendGCMsgToClientWithPreSerializedBody( const CSteamID & steamIDTarget, MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, const byte *pubBody, uint32 cubBody ) const;
	bool BSendGCMsgToSystemWithPreSerializedBody( MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, const byte *pubBody, uint32 cubBody ) const;
	bool BReplyToMessageWithPreSerializedBody( MsgType_t eMsgType, const CProtoBufMsgBase &msgIn, const byte *pubBody, uint32 cubBody ) const;

	const char *GetPath() const { return m_sPath; }
	virtual const char *GetSteamAPIKey();

	void QueueStartPlaying( const CSteamID & steamID, const CSteamID & gsSteamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData );
	void YieldingExecuteNextStartPlaying();
	bool BIsInLogonSurge() const { return m_nLogonSurgeFramesRemaining > 0; }

	/// Are we too busy to process any "low service level guarantee" WebAPI jobs?
	bool BShouldThrottleLowServiceLevelWebAPIJobs() const;

	/// Removes the entry in the start playing queue for the specified
	/// Steam ID, if one exists.  Returns true if it was found
	/// and removed, false if no entry existed
	bool BRemoveStartPlayingQueueEntry( const CSteamID & steamID );

	void YieldingStopPlaying( const CSteamID & steamID );
	void YieldingStopGameserver( const CSteamID & steamID );
	bool BIsSOCacheBeingLoaded( const CSteamID & steamID ) const { return m_rbtreeSOCachesBeingLoaded.Find( steamID ) != m_rbtreeSOCachesBeingLoaded.InvalidIndex(); }

	bool BYieldingLockSteamID( const CSteamID &steamID, const char *pszFilename, int nLineNum );
	bool BYieldingLockSteamIDPair( const CSteamID &steamIDA, const CSteamID &steamIDB, const char *pszFilename, int nLineNum );
	bool BLockSteamIDImmediate( const CSteamID &steamID );
	void UnlockSteamID( const CSteamID &steamID );
	bool IsSteamIDLocked( const CSteamID &steamID );
	bool IsSteamIDLockedByJob( const CSteamID &steamID, const CJob *pJob ) const;
	bool IsSteamIDLockedByCurJob( const CSteamID &steamID ) const;
	bool IsSteamIDUnlockedOrLockedByCurJob( const CSteamID &steamID );
	CJob *PJobHoldingLock( const CSteamID &steamID );

	const CLock *FindSteamIDLock( const CSteamID &steamID );
	void DumpSteamIDLocks( bool bFull, int nMax = 10 );
	void DumpJobs( const char *pszJobName, int nMax, int nPrintLocksMax ) const;
	void DumpJob( JobID_t jobID, int nPrintLocksMax ) const;
	virtual void Dump() const;
	void VerifySOCacheLRU();

	void SetProfilingEnabled( bool bEnabled );
	void SetDumpVprofImbalances( bool bEnabled );
	bool GetVprofImbalances();

	bool YieldingWritebackDirtyCaches( uint32 unSecondToDelayWrite );
	void AddCacheToWritebackQueue( CGCSharedObjectCache *pSOCache );

	bool BYieldingRetrieveCacheVersion( CGCSharedObjectCache *pSOCache );
	void AddCacheToVersionChangedList( CGCSharedObjectCache *pSOCache );

	const char *GetCDNURL() const;

	struct GCMemcachedBuffer_t
	{
		const void *m_pubData;
		int m_cubData;
	};

	struct GCMemcachedGetResult_t
	{
		bool m_bKeyFound;					// true if the key was found
		CUtlAllocation m_bufValue;			// the value of the key
	};
	
	// Memcached access
	bool BMemcachedSet( const char *pKey, const ::google::protobuf::Message &protoBufObj );
	bool BMemcachedDelete( const char *pKey );
	bool BYieldingMemcachedGet( const char *pKey, ::google::protobuf::Message &protoBufObj );
	bool BMemcachedSet( const CUtlString &strKey, const CUtlBuffer &buf );
	bool BMemcachedSet( const CUtlVector<CUtlString> &vecKeys, const CUtlVector<GCMemcachedBuffer_t> &vecValues );
	bool BMemcachedDelete( const CUtlString &strKey );
	bool BMemcachedDelete( const CUtlVector<CUtlString> &vecKeys );
	bool BYieldingMemcachedGet( const CUtlString &strKey, GCMemcachedGetResult_t &result );
	bool BYieldingMemcachedGet( const CUtlVector<CUtlString> &vecKeys, CUtlVector<GCMemcachedGetResult_t> &vecResults );

	// IP location
	bool BYieldingGetIPLocations( CUtlVector<uint32> &vecIPs, CUtlVector<CIPLocationInfo> &infos );

	/// Check if any of the sessions don't have geolocation info, then fetch
	/// the info we can.  The supplied SteamID's may refer to individuals or
	/// game servers
	bool BYieldingUpdateGeoLocation( CUtlVector<CSteamID> const &requestedVecSteamIds );

	// Stats
	virtual void SystemStats_Update( CGCMsgGetSystemStatsResponse &msgStats );

	//called to determine the amount of uptime this GC has had
	uint32 GetGCUpTime() const;
	RTime32 GetGCInitTime() const		{ return m_nInitTime; }

protected:
	virtual bool BYieldingLoadSOCache( CGCSharedObjectCache *pSOCache );
	void RemoveSOCache( const CSteamID & steamID );

	void AddCacheToLRU( CGCSharedObjectCache * pSOCache );
	void RemoveCacheFromLRU( CGCSharedObjectCache * pSOCache );

	void SetStartupComplete() { m_bStartupComplete = true; }
private:

	static void AssertCallbackFunc( const char *pchFile, int nLine, const char *pchMessage );
	void UpdateSOCacheVersions();

	//
	// Create and initialize player / gameserver sessions.  This should be called only from two places:
	// - YieldingExecuteNextStartPlaying()
	// - YieldingRequestSession(), which can be called ANY TIME we ask for a locked session but don't have one
	//
	void YieldingStartPlaying( const CSteamID & steamID, const CSteamID & gsSteamID, uint32 unServerAddr, uint16 usServerPort, CUtlBuffer *pVarData );
	void YieldingStartGameserver( const CSteamID & steamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData );

	void YieldingExecuteStartPlayingQueueEntryByIndex( int idxStartPlayingQueue );

	// Base behaviors of the IGameCoordinator interface. These are not overridable.
	// They each call the On* version below, which is overridable by subclasses
	virtual bool BInit( AppId_t unAppID, const char *pchAppPath, IGameCoordinatorHost *pHost );
	virtual bool BMainLoopOncePerFrame( uint64 ulLimitMicroseconds );
	virtual bool BMainLoopUntilFrameCompletion( uint64 ulLimitMicroseconds );
	virtual void Shutdown();
	virtual void Uninit();
	virtual void MessageFromClient( const CSteamID & senderID, uint32 unMsgType, void *pubData, uint32 cubData );
	virtual void Validate( CValidator &validator, const char *pchName );
	virtual void SQLResults( GID_t gidContextID );

	static void SetUserSessionDetails( CGCUserSession *pUserSession, KeyValues *pkvDetails );

	// Remembers the console spew func we overrode so we can restore it at uninit
	SpewOutputFunc_t m_OutputFuncPrev;

	// profiling
	bool m_bStartProfiling;
	bool m_bStopProfiling;
	bool m_bDumpVprofImbalances;

	//the time that the GC started so that we can compute uptime
	RTime32	m_nInitTime;
	RTime32 m_nStartupCompleteTime;

	// local job handling
	CJobMgr m_JobMgr;
	CGCWGJobMgr m_wgJobMgr;

	// session tracking
	CTHash<CGCUserSession *, uint64> m_hashUserSessions;
	CTHash<CGCGSSession *, uint64> m_hashGSSessions;
	CUtlHashMapLarge< uint64, CGCGSSession* > m_mapGSSessionsByNetAddress;

	// Shared object caches
	CUtlHashMapLarge<CSteamID, CGCSharedObjectCache *> m_mapSOCache;
	CUtlVector< CGCSharedObjectCache * >m_vecCacheWritebacks;
	CUtlLinkedList< CSteamID, uint32> m_listCachesToUnload;
	CUtlRBTree<CSteamID, int > m_rbtreeSOCachesBeingLoaded;
	CUtlRBTree<CSteamID, int > m_rbtreeSOCachesWithDirtyVersions;
	CUtlRBTree< AccountID_t, int32, CDefLess< AccountID_t > > m_rbFlushInventoryCacheAccounts;
	JobID_t m_jobidFlushInventoryCacheAccounts;
	uint32 m_numFlushInventoryCacheAccountsLastScheduled;

	// steamID locks
	CTHash<CLock, CSteamID>	m_hashSteamIDLocks;

	// Account Details
	CAccountDetailsManager	m_AccountDetailsManager;

	// State
	AppId_t					m_unAppID;
	CUtlString				m_sPath;
	IGameCoordinatorHost	*m_pHost;
	bool					m_bStartupComplete;
	bool					m_bIsShuttingDown;

	struct StartPlayingWork_t 
	{
		CSteamID m_steamID;
		CSteamID m_gsSteamID;
		uint32 m_unServerAddr;
		uint16 m_usServerPort;
		CUtlBuffer *m_pVarData;
	};
	CUtlLinkedList< StartPlayingWork_t, int > m_llStartPlaying;
	CUtlMap< CSteamID, int, int > m_mapStartPlayingQueueIndexBySteamID;

	/// Number of active jobs
	int m_nStartPlayingJobCount;

	// URL to use for our app's CDN'd images
	mutable CUtlString m_sCDNURL;

	/// Number of active jobs currently inside YieldingRequestSession
	int m_nRequestSessionJobsActive;

	/// Number of frames to wait before checking to
	/// see if we're done with logon surge.  This will
	/// be nonzero while in logon surge, and zero
	/// if we're not in logon surge.
	int m_nLogonSurgeFramesRemaining;

	//rate limiting system 
	CSteamIDRateLimit	m_MsgRateLimit;

	//a map of the HTTP error messages so we can batch them up as they occur as they tend to be very spammy
	void ReportHTTPError( const char* pszError, CGCEmitGroup::EMsgLevel eLevel );
	void DumpHTTPErrors();
	struct SHTTPError
	{
		CUtlString				m_sStr;
		uint32					m_nCount;
		CGCEmitGroup::EMsgLevel	m_eSeverity;
	};
	CUtlHashMapLarge< const char*, SHTTPError*, CaseSensitiveStrEquals, MurmurHash3ConstCharPtr >	m_HTTPErrors;
	CScheduledFunction< CGCBase >																	m_DumpHTTPErrorsSchedule;
};

extern CGCBase *GGCBase();

// ----------------------------------------------------------------------------
// BEGIN BLOCK OF PRE-TF-GC-SPLIT SCAFFOLDING
// ----------------------------------------------------------------------------

// Alright, here's why this mess is here: we (TF) are doing some work that depends
// on work that Dota has done, things like the SQL message queue, etc. We want to
// update our code so that it does the right thing and uses their functionaity so
// that they can then integrate our changes.
//
// TF is way out of date, though, and so rather than make that work, some of which
// has significant time pressure, dependent on a massive weeks-long integrate that
// we aren't sure is the right thing to do anyway, we're going to bring over their
// code as-is and then stub in implementations.
//
// This horrific template mess basically says "are we compiling in an environment
// with a spit GC?" (specifically, does CGCBase::GetGCType() exist?). If so, call
// into real implementations for things like GetGCGType(), etc. If not, feed back
// sane default values (ie., we're a master GC and there's only one of us). Doing
// this means that Dota can integrate our code and we can integrate Dota code and
// no callsites have to change. If/when TF *does* split our GC, things will silently
// update their implementation and then we can delete the scaffolding whenever.
template < class tBaseGCType >
struct IsSplitGC
{
	typedef char t_Yes[1];
	typedef char t_No[2];

	template < typename T, T >
	struct InternalTypeCheck;                     \
	
	template < typename T >
	static t_Yes& Test( InternalTypeCheck< uint32(), &T::GetGCType> * );

	template < typename T >
	static t_No& Test( ... );

	enum { kValue = sizeof( Test<tBaseGCType>( NULL ) ) == sizeof( t_Yes ) };
};

template < bool tTest, typename T > struct EnableIf { typedef T Type; };
template < typename T > struct EnableIf< false, T > { };

#define SplitVsSingleGCImpl( returntype_, functionandparams_, splitgcimpl_, singlegcimpl_ ) \
	template < class tBaseGCType = CGCBase > typename EnableIf<IsSplitGC<tBaseGCType>::kValue, returntype_>::Type functionandparams_	{ return splitgcimpl_; } \
	template < class tBaseGCType = CGCBase > typename EnableIf<!IsSplitGC<tBaseGCType>::kValue, returntype_>::Type functionandparams_	{ return singlegcimpl_; }

SplitVsSingleGCImpl( uint32, GGetGCType(), GGCBase()->GetGCType(), 0 );
SplitVsSingleGCImpl( uint32, GGetGCCountForType( uint32 unGCType ), GDirectory()->GetGCCountForType( unGCType ), 1 );
SplitVsSingleGCImpl( uint32, GGetGCInstance(), GGCBase()->GetDirInfo()->GetInstance(), 0 );

#undef SplitVsSingleGCImpl

// ----------------------------------------------------------------------------
// END BLOCK OF TEMPORARY PRE-INTEGRATE SCAFFOLDING
// ----------------------------------------------------------------------------

EResult YieldingSendWebAPIRequest( CSteamAPIRequest &request, KeyValues *pKVResponse, CUtlString &errMsg, bool b200MeansSuccess );

/// Scope object that remembers if a Steam ID is locked, and automatically unlocks it upon destruction
class CScopedSteamIDLock
{
public:

	/// Create object without assigning SteamID
	CScopedSteamIDLock() : m_steamID(), m_bLockedSuccesfully(false), m_iSavedLockCount(-1) {}

	/// Construct object to lock a particular steam ID
	CScopedSteamIDLock( const CSteamID& steamID ) : m_steamID( steamID ), m_bLockedSuccesfully(false), m_iSavedLockCount(-1) {}

	/// Destructor performs an unlock, if we are holing a lock.  That's pretty much the whole purpose of this class.
	~CScopedSteamIDLock() { Unlock(); }

	/// Return true if we're locked
	bool IsLocked() const { return m_bLockedSuccesfully; }

	/// Lock the currently assigned Steam ID.  (Set by constructor)
	bool BYieldingPerformLock( const char *pszFilename, int nLineNumber )
	{
		Assert( !m_bLockedSuccesfully );
		Unlock();

		Assert( m_steamID.IsValid() );

		m_bLockedSuccesfully = GGCBase()->BYieldingLockSteamID( m_steamID, pszFilename, nLineNumber );
		if ( m_bLockedSuccesfully )
		{
			const CLock *pLock = GGCBase()->FindSteamIDLock( m_steamID );
			if ( pLock )
			{
				m_iSavedLockCount = pLock->GetReferenceCount();
			}
			else
			{
				Assert( pLock );
			}
		}
		return m_bLockedSuccesfully;
	}

	/// Lock the specified Steam ID.
	bool BYieldingPerformLock( const CSteamID &steamID, const char *pszFilename, int nLineNumber )
	{

		// Should not already be locked, but unlock just in case
		Assert( !m_bLockedSuccesfully );
		Unlock();

		// Attempt lock
		m_steamID = steamID;
		m_bLockedSuccesfully = GGCBase()->BYieldingLockSteamID( m_steamID, pszFilename, nLineNumber );
		if ( m_bLockedSuccesfully )
		{
			const CLock *pLock = GGCBase()->FindSteamIDLock( m_steamID );
			if ( pLock )
			{
				m_iSavedLockCount = pLock->GetReferenceCount();
			}
			else
			{
				Assert( pLock );
			}
		}

		// Report status
		return m_bLockedSuccesfully;
	}

	/// Mark us as already being locked.  This is used when you already have a lock, and just want to use
	/// the scoped class to do the unlock when you're done
	void MarkLocked( const CSteamID &steamID )
	{

		// Should not already be locked, but unlock just in case
		Assert( !m_bLockedSuccesfully );
		Unlock();

		// Remember state
		m_steamID = steamID;
		m_bLockedSuccesfully = true;
		const CLock *pLock = GGCBase()->FindSteamIDLock( m_steamID );
		if ( pLock )
		{
			m_iSavedLockCount = pLock->GetReferenceCount();
		}
		else
		{
			Assert( pLock );
		}
	}

	/// Stop tracking that we're locking this object. This does *not* unlock anything -- Unlock does
	/// that. This is for "we hit the end of our scope but now want to do something else with the
	/// lock (ie., pass back to a caller)". This is like un-smarting a smart pointer.
	void Release()
	{
		if ( m_bLockedSuccesfully )
		{
			VerifyPreUnlock();
			m_bLockedSuccesfully = false;
			m_iSavedLockCount = -1;
		}
	}

	/// Unlock the lock, if we are holding it.  Usually this is not called directly,
	/// we let the destructor do it.
	void Unlock()
	{
		if ( m_bLockedSuccesfully )
		{
			VerifyPreUnlock();
			GGCBase()->UnlockSteamID( m_steamID );
			m_bLockedSuccesfully = false;
			m_iSavedLockCount = -1;
		}
	}

private:
	void VerifyPreUnlock() const
	{
		Assert( m_bLockedSuccesfully );

		const CLock *pLock = GGCBase()->FindSteamIDLock( m_steamID );
		if ( pLock )
		{
			AssertMsg1( m_iSavedLockCount == pLock->GetReferenceCount(), "Lock imbalance on %s detected by scope lock", pLock->GetName() );
		}
		else
		{
			Assert( pLock );
		}
	}

private:
	CSteamID m_steamID;
	bool m_bLockedSuccesfully;
	int m_iSavedLockCount;

};

/// Scope object that remembers if a specific lock is taken, and releases it
class CScopedGenericLock
{
public:

	/// Create object for specific lock SteamID
	CScopedGenericLock( CLock &lock ) : m_pLock( &lock ), m_bLockedSuccesfully(false), m_iSavedLockCount(-1) {}

	/// Destructor performs an unlock, if we are holing a lock.  That's pretty much the whole purpose of this class.
	~CScopedGenericLock() { Unlock(); }

	/// Return true if we're locked
	bool IsLocked() const { return m_bLockedSuccesfully; }

	/// Lock the current lock.  (Set by constructor)
	bool BYieldingPerformLock( const char *pszFilename, int nLineNumber )
	{
		Assert( !m_bLockedSuccesfully );
		Unlock();

		Assert( m_pLock );

		m_bLockedSuccesfully = GJobCur()._BYieldingAcquireLock( m_pLock, pszFilename, nLineNumber );
		if ( m_bLockedSuccesfully )
		{
			m_iSavedLockCount = m_pLock->GetReferenceCount();
		}
		return m_bLockedSuccesfully;
	}

	/// Stop tracking that we're locking this object. This does *not* unlock anything -- Unlock does
	/// that. This is for "we hit the end of our scope but now want to do something else with the
	/// lock (ie., pass back to a caller)". This is like un-smarting a smart pointer.
	void Release()
	{
		if ( m_bLockedSuccesfully )
		{
			VerifyPreUnlock();
			m_bLockedSuccesfully = false;
			m_iSavedLockCount = -1;
		}
	}

	/// Unlock the lock, if we are holding it.  Usually this is not called directly,
	/// we let the destructor do it.
	void Unlock()
	{
		if ( m_bLockedSuccesfully )
		{
			VerifyPreUnlock();
			if ( m_pLock->GetJobLocking() != &GJobCur() )
			{
				AssertMsg( false, "CScopedGenericLock::Unlock called when job %s doesn't own the lock", GJobCur().GetName() );
				return;
			}

			GJobCur().ReleaseLock( m_pLock );
			m_bLockedSuccesfully = false;
			m_iSavedLockCount = -1;
		}
	}

private:
	void VerifyPreUnlock() const
	{
		Assert( m_bLockedSuccesfully );

		AssertMsg1( m_iSavedLockCount == m_pLock->GetReferenceCount(), "Lock imbalance on %s detected by scope lock", m_pLock->GetName() );
	}

private:
	CLock *m_pLock;
	bool m_bLockedSuccesfully;
	int m_iSavedLockCount;

};

class CTrustedHelper_AssertOnNonPublicUniverseFailures
{
public:
	void operator()( const bool bExpResult, const char *pszExp ) const
	{
		if ( GetUniverse() != k_EUniversePublic )
		{
			AssertMsg1( bExpResult, "%s", pszExp );
		}
	}
};

class CVerifyIfTrustedHelper
{
public:
	template < typename tTrustedCriteriaImpl >
	CVerifyIfTrustedHelper( const tTrustedCriteriaImpl& CriteriaImpl, const bool bExpResult, const char *pszExp )
	{
		CommonConstructor( CriteriaImpl, bExpResult, pszExp );
	}

	// Helper constructor so we can do VerifyIfTrusted( pSomePointer ) without getting a type-
	// conversion-to-bool warning.
	template < typename tTrustedCriteriaImpl >
	CVerifyIfTrustedHelper( const tTrustedCriteriaImpl& CriteriaImpl, const void *pointer, const char *pszExp )
	{
		CommonConstructor( CriteriaImpl, pointer != NULL, pszExp );
	}

	bool GetResult() const { return m_bExpResult; }

private:
	template < typename tTrustedCriteriaImpl >
	void CommonConstructor( const tTrustedCriteriaImpl& CriteriaImpl, const bool bExpResult, const char *pszExp )
	{
		m_bExpResult = bExpResult;

		CriteriaImpl( bExpResult, pszExp );
	}

	bool m_bExpResult;
};

// Examples of things that we don't want to assert on, even during testing:
//		- we got a message from someone who didn't have a session. This could be a Steam
//		  problem, or we could be backlogged processing messages, or we could have an offline
//		  trade get processed in the background.
//		- someone sent up a message involving an item that they don't own. (Same reasons as
//		  a missing session.)
//
// Examples of things that we do want to assert on internally, but not when running public:
//		- our messages match contents fulfill criteria that the client specifies. ie., if we're
//		  passing up a "victim" and a "killer" Steam ID, they won't be identical.
//		- our schema data is set up correctly. ie., necessary fields ("kill eater localization
//		  string") that we expect to fail a schema parse init for if they're absent are present.
//		- our messages are coming from places that make sense. ie., a game server isn't sending
//		  messages we expect to get from a client, or we aren't getting a message from a client
//		  that we expect only to come from elsewhere in GC code.
#define VerifyIfTrusted( exp_ ) \
	CVerifyIfTrustedHelper( CTrustedHelper_AssertOnNonPublicUniverseFailures(), (exp_), #exp_ ).GetResult()

// !KLUDGE! Shim to make it easier to merge over stuff from DOTA
class CGCInterface
{
public:
	CSteamID ConstructSteamIDForClient( AccountID_t unAccountID ) const;
};
extern CGCInterface *GGCInterface();

} // namespace GCSDK

struct CRatelimitedSpewController
{
public:
	CRatelimitedSpewController() : m_rtCooldownStart( 0 ), m_rtLastSpew( 0 ), m_numSpewed( 0 ), m_numSkipped( 0 ) {}
	~CRatelimitedSpewController() {}

	enum ERate_t
	{
		k_ERate_Skip = -1,		// skip printing anything, rate limit too high
		k_ERate_Print = 0,		// print the full message
		// default is to print how many messages total as well
	};

	int RegisterSpew();

private:
	RTime32 m_rtCooldownStart;
	RTime32 m_rtLastSpew;
	int m_numSpewed;
	int m_numSkipped;
};

#define EmitErrorRatelimited( SPEW_GROUP_ID, fmtstring, ... ) \
			static CRatelimitedSpewController s_spewcontroller; \
			int nspewcontroller = s_spewcontroller.RegisterSpew(); \
			switch ( nspewcontroller ) \
			{ \
			case CRatelimitedSpewController::k_ERate_Skip: break; \
			default: \
				EmitError( SPEW_GROUP_ID, nspewcontroller \
					? fmtstring "(... and %d more skipped)\n" \
					: fmtstring \
					, __VA_ARGS__ \
					, nspewcontroller \
					); \
				break; \
			} \


#define EmitInfoRatelimited( SPEW_GROUP_ID, ConsoleLevel, LogLevel, fmtstring, ... ) \
			static CRatelimitedSpewController s_spewcontroller; \
			int nspewcontroller = s_spewcontroller.RegisterSpew(); \
			switch ( nspewcontroller ) \
			{ \
			case CRatelimitedSpewController::k_ERate_Skip: break; \
			default: \
				EmitInfo( SPEW_GROUP_ID, ConsoleLevel, LogLevel, nspewcontroller \
					? fmtstring "(... and %d more skipped)\n" \
					: fmtstring \
					, __VA_ARGS__ \
					, nspewcontroller \
					); \
				break; \
			} \


#define EG_WARNING_RATELIMITED( SPEW_GROUP_ID, fmtstring, ... ) \
			static CRatelimitedSpewController s_spewcontroller; \
			int nspewcontroller = s_spewcontroller.RegisterSpew(); \
			switch ( nspewcontroller ) \
			{ \
			case CRatelimitedSpewController::k_ERate_Skip: break; \
			default: \
				EG_WARNING( SPEW_GROUP_ID, nspewcontroller \
					? fmtstring "(... and %d more skipped)\n" \
					: fmtstring \
					, __VA_ARGS__ \
					, nspewcontroller \
					); \
				break; \
			} \



#endif // GCBASE_H
