//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include "gcbase.h"
#include "tier1/interface.h"
#include "tier0/minidump.h"
#include "tier0/icommandline.h"
#include "gcjob.h"
#include "sqlaccess/schemaupdate.h"
#include "gcsystemmsgs.h"
#include "rtime.h"
#include "msgprotobuf.h"
#include "gcsdk_gcmessages.pb.h"
#include "gcsdk/gcparalleljobfarm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//----------------------------------------------------------------------
// Emit groups
//----------------------------------------------------------------------
DECLARE_GC_EMIT_GROUP( g_EGHTTPRequest, http_request );

CGCBase *g_pGCBase = NULL;

// Thread pool size convar
static void OnConVarChangeJobMgrThreadPoolSize( IConVar *pConVar, const char *pOldString, float flOldValue );
GCConVar jobmgr_threadpool_size( "jobmgr_threadpool_size", "-1", 0,
                                 "Maximum threads in the job manager thread pool. Values <= 0 mean number_logical_cpus - this.",
                                 OnConVarChangeJobMgrThreadPoolSize );

static uint32 GetThreadPoolSizeFromConVar()
{
	int nVal = jobmgr_threadpool_size.GetInt();
	int nRet = ( nVal > 0 ) ? nVal : GetCPUInformation()->m_nLogicalProcessors + nVal;
	return (uint32)Clamp( nRet, 1, INT_MAX );
}

static void OnConVarChangeJobMgrThreadPoolSize( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( GGCBase()->GetIsShuttingDown() )
		return;

	GGCBase()->GetJobMgr().SetThreadPoolSize( GetThreadPoolSizeFromConVar() );
}

GCConVar cv_concurrent_start_playing_limit( "concurrent_start_playing_limit", "1000" );
GCConVar cv_logon_surge_start_playing_limit( "logon_surge_start_playing_limit", "2000" );
GCConVar cv_logon_surge_request_session_jobs( "logon_surge_request_session_jobs", "1000" );
GCConVar cv_webapi_throttle_job_threshold( "webapi_throttle_job_threshold", "2000", 0, "If the job count exceeds this threshold, reject low-priority webapi jobs" );
GCConVar enable_startplaying_gameserver_creation_spew( "enable_startplaying_gameserver_creation_spew", "0" );
// Enable the restore-version-from-memcache machinery. Disabled because it assumes reloading an SOCache is
// deterministic, which is no longer true for us, resulting in clients with stale versions believing themselves to be in
// sync.
//
// This probably needs a look -- ideally we'd delineate deterministic objects that can be assumed to remain in sync in
// GC reboots, and dynamic objects that cannot.
//
// Note that we already removed hacks for this in player groups and started using lazy-loaded objects in SOCaches that
// violate the assumptions this was making, so re-enabling it requires work. We probably really want to split type
// caches into deterministic-between-GC-reboots and not, and resend based on said flag.
GCConVar socache_persist_version_via_memcached( "socache_persist_version_via_memcached", "0" );

static GCConVar cv_assert_minidump_window( "assert_minidump_window", "28800", 0, "Size of the minidump window in seconds. Each unique assert will dump at most assert_max_minidumps_in_window times in this many seconds" );
static GCConVar cv_assert_max_minidumps_in_window( "assert_max_minidumps_in_window", "5", 0, "The amount of times each unique assert will write a dump in assert_minidump_window seconds" );

static GCConVar cv_debug_steam_startplaying( "cv_debug_steam_startplaying", "0", 0, "Turn this ON to debug the stream of startplaying messages we get from Steam" );

static GCConVar temp_list_mismatched_replies( "temp_list_mismatched_replies", "0", "When set to 1, this report all replies that fail because the incoming message didn't expect a response. Temporary to help track down some failed state" );

static GCConVar writeback_queue_max_accumulate_time( "writeback_queue_max_accumulate_time", "10", 0, "The maximum amount of time in seconds that the writeback queue will accumulate database writes before performing queries. This is the time *before* the queries are executed, which is unbounded." );
static GCConVar writeback_queue_max_caches( "writeback_queue_max_caches", "0", 0, "The maximum amount of caches to write back in a single transaction. Set to zero to remove this restriction." );
static GCConVar geolocation_spewlevel( "geolocation_spewlevel", "4", 0, "Spewlevel to use for geolocation debug spew" );
static GCConVar geolocation_loglevel( "geolocation_loglevel", "4", 0, "Spewlevel to use for geolocation debug spew" );

extern GCConVar max_user_messages_per_second;

// There is also a GCConVar writeback_delay to control how frequently we do writebacks.

// !KLUDGE! Temp shim.  Will get rid of this when we bring over the real gcinterface stuff from DOTA.
CGCInterface g_GCInterface;
CGCInterface *GGCInterface() { return &g_GCInterface; }
CSteamID CGCInterface::ConstructSteamIDForClient( AccountID_t unAccountID ) const
{
	return CSteamID( unAccountID, GetUniverse(), k_EAccountTypeIndividual );
}


//-----------------------------------------------------------------------------
// Purpose: Overrides the spew func used by Msg and DMsg to print to the console
//-----------------------------------------------------------------------------
SpewRetval_t ConsoleSpewFunc( SpewType_t type, const tchar *pMsg )
{
	const char *fmt = ( sizeof( tchar ) == sizeof( char ) ) ? "%hs" : "%ls";
	switch (type )
	{
		default:
		case SPEW_MESSAGE:
		case SPEW_LOG:
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, fmt, pMsg );
			break;
		case SPEW_WARNING:
			EmitWarning( SPEW_CONSOLE, SPEW_ALWAYS, fmt, pMsg );
			break;
		case SPEW_ASSERT:
			if ( ThreadInMainThread() && ( g_pJobCur != NULL ) )
			{
				fmt = ( sizeof( tchar ) == sizeof( char ) ) ? "[Job %s] %hs" : "[Job %s] %ls";
				EmitError( SPEW_CONSOLE, fmt, g_pJobCur->GetName(), pMsg );
			}
			else
			{
				EmitError( SPEW_CONSOLE, fmt, pMsg );
			}
			break;
		case SPEW_ERROR:
			EmitError( SPEW_CONSOLE, fmt, pMsg );
			break;
	}

	if ( type == SPEW_ASSERT )
	{
#ifndef WIN32
		// Non-win32
		bool bRaiseOnAssert = getenv( "RAISE_ON_ASSERT" ) || !!CommandLine()->FindParm( "-raiseonassert" );
#elif defined( _DEBUG )
		// Win32 debug
		bool bRaiseOnAssert = true;
#else
		// Win32 release
		bool bRaiseOnAssert = !!CommandLine()->FindParm( "-raiseonassert" );
#endif

		return bRaiseOnAssert ? SPEW_DEBUGGER : SPEW_CONTINUE;
	}
	else if ( type == SPEW_ERROR )
		return SPEW_ABORT;
	else
		return SPEW_CONTINUE;
}


class CGCShutdownJob : public CGCJob
{
public:
	CGCShutdownJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob()
	{
		m_pGC->SetIsShuttingDown();

		// Log off all of the game servers and users, so that if something
		// in the log off dirties caches they can be written back
		CUtlVector<CSteamID> vecIDsToStop;
		for( CGCGSSession **ppSession = m_pGC->GetFirstGSSession(); ppSession != NULL; ppSession = m_pGC->GetNextGSSession( ppSession ) )
		{
			vecIDsToStop.AddToTail( (*ppSession)->GetSteamID() );
		}

		FOR_EACH_VEC( vecIDsToStop, i )
		{
			m_pGC->YieldingStopGameserver( vecIDsToStop[i] );
			ShouldNotHoldAnyLocks();
		}

		vecIDsToStop.RemoveAll();

		for( CGCUserSession **ppSession = m_pGC->GetFirstUserSession(); ppSession != NULL; ppSession = m_pGC->GetNextUserSession( ppSession ) )
		{
			vecIDsToStop.AddToTail( (*ppSession)->GetSteamID() );
		}

		FOR_EACH_VEC( vecIDsToStop, i )
		{
			m_pGC->YieldingStopPlaying( vecIDsToStop[i] );
			ShouldNotHoldAnyLocks();
		}

		// wait for jobs to finish (except this one!)
		const int kMaxIterations = 100;
		int cIter = 0;
		while ( cIter++ < kMaxIterations && m_pGC->GetJobMgr().CountJobs() > 1 )
		{
			BYieldingWaitOneFrame();
		}

		m_pGC->YieldingGracefulShutdown();
		GGCHost()->ShutdownComplete();

		return false; 
	}

};


class CPreTestSetupJob : public CGCJob
{
public:
	CPreTestSetupJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( GCSDK::CNetPacket *pNetPacket )
	{
		CGCMsg<MsgGCEmpty_t> msg( pNetPacket );
		m_pGC->YieldingPreTestSetup();

		return true; 
	}
};

GC_REG_JOB( CGCBase, CPreTestSetupJob, "CPreTestSetupJob", k_EGCMsgPreTestSetup, k_EServerTypeGC );

static void SpewSerializedKeyValues( const byte *pubVarData, uint32 cubVarData )
{
	if ( pubVarData == NULL || cubVarData == 0 )
	{
		EmitInfo( SPEW_GC, 1, 1, "  No KV data\n" );
		return;
	}
	char szLine[512] = "";
	for ( uint32 i = 0 ; i < cubVarData ; ++i )
	{
		char szByteVal[32];
		V_sprintf_safe( szByteVal, "%02X", pubVarData[ i ] );
		if ( i % 32 )
		{
			V_strcat_safe( szLine, ", " );
			V_strcat_safe( szLine, szByteVal );
		}
		else
		{
			if ( szLine[0] )
				EmitInfo( SPEW_GC, 1, 1, "  %s\n", szLine );
			V_strcpy_safe( szLine, szByteVal );
		}
	}
	if ( szLine[0] )
		EmitInfo( SPEW_GC, 1, 1, "  %s\n", szLine );
	KeyValuesAD pkvDetails( "SessionDetails" );
	CUtlBuffer buf;
	buf.Put( pubVarData, cubVarData );
	if( pkvDetails->ReadAsBinary( buf ) )
	{
		FOR_EACH_VALUE( pkvDetails, v )
		{
			EmitInfo( SPEW_GC, 1, 1, "  %s = %s\n", v->GetName(), v->GetString( NULL, "??" ) );
		}
	}
	else
	{
		EmitInfo( SPEW_GC, 1, 1, "  KV data failed parse\n" );
	}
}

class CStartPlayingJob : public CGCJob
{
public:
	CStartPlayingJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		CGCMsg<MsgGCStartPlaying_t> msg( pNetPacket );

		// @note Tom Bui/Joe Ludwig: This can happen for PS3 Steam accounts
		if ( !msg.Body().m_steamID.IsValid() )
			return true;

		if ( cv_debug_steam_startplaying.GetBool() )
		{
			netadr_t serverAdr( msg.Body().m_unServerAddr, msg.Body().m_usServerPort );
			EmitInfo( SPEW_GC, 1, 1, "Received StartPlaying( user = %s, GS = %s @ %s )\n", msg.Body().m_steamID.Render(), msg.Body().m_steamIDGS.Render(), serverAdr.ToString() );
			SpewSerializedKeyValues( msg.PubVarData(), msg.CubVarData() );
		}
		m_pGC->QueueStartPlaying( msg.Body().m_steamID, msg.Body().m_steamIDGS, msg.Body().m_unServerAddr, msg.Body().m_usServerPort, msg.PubVarData(), msg.CubVarData() );

		return true; 
	}
};

GC_REG_JOB(CGCBase, CStartPlayingJob, "CStartPlayingJob", k_EGCMsgStartPlaying, k_EServerTypeGC);

class CExecuteStartPlayingJob : public CGCJob
{
public:
	CExecuteStartPlayingJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( )
	{
		m_pGC->YieldingExecuteNextStartPlaying();
		return true; 
	}
};


class CStopPlayingJob : public CGCJob
{
public:
	CStopPlayingJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		CGCMsg<MsgGCStopSession_t> msg( pNetPacket );

		// @note Tom Bui/Joe Ludwig: This can happen for PS3 Steam accounts
		if ( !msg.Body().m_steamID.IsValid() )
			return true;

		if ( cv_debug_steam_startplaying.GetBool() )
		{
			EmitInfo( SPEW_GC, 1, 1, "Received StopPlaying( user = %s )\n", msg.Body().m_steamID.Render() );
		}

		m_pGC->YieldingStopPlaying( msg.Body().m_steamID );

		return true; 
	}
};

GC_REG_JOB(CGCBase, CStopPlayingJob, "CStopPlayingJob", k_EGCMsgStopPlaying, k_EServerTypeGC);

class CStartGameserverJob : public CGCJob
{
public:
	CStartGameserverJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		CGCMsg<MsgGCStartGameserver_t> msg( pNetPacket );
		m_pGC->QueueStartPlaying( msg.Body().m_steamID, CSteamID(), msg.Body().m_unServerAddr, msg.Body().m_usServerPort, msg.PubVarData(), msg.CubVarData() );

		return true; 
	}
};

GC_REG_JOB(CGCBase, CStartGameserverJob, "CStartGameserverJob", k_EGCMsgStartGameserver, k_EServerTypeGC);

class CStopGameserverJob : public CGCJob
{
public:
	CStopGameserverJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		CGCMsg<MsgGCStopSession_t> msg( pNetPacket );
		m_pGC->YieldingStopGameserver( msg.Body().m_steamID );

		return true; 
	}
};

GC_REG_JOB(CGCBase, CStopGameserverJob, "CStopGameserverJob", k_EGCMsgStopGameserver, k_EServerTypeGC);

class CGetSystemStatsJob : public CGCJob
{
public:
	CGetSystemStatsJob( CGCBase *pGC ) : CGCJob( pGC ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CGCMsgGetSystemStats> msg( pNetPacket );

		CProtoBufMsg<CGCMsgGetSystemStatsResponse> msgResponse( k_EGCMsgGetSystemStatsResponse );
		msgResponse.Body().set_gc_app_id( m_pGC->GetAppID() );

		// @note Tom Bui: we don't support dynamic stats yet, but once we do, we can use the KV stuff
		m_pGC->SystemStats_Update( msgResponse.Body() );

		//		KVPacker packer;
		//		KeyValuesAD pKVStats( "GCStats" );
		//		CUtlBuffer buffer;
		//		if ( packer.WriteAsBinary( pKVStats, buffer ) )
		//		{			
		//			msgResponse.Body().set_stats_kv( buffer.Base(), buffer.TellPut() );			
		//		}
		return m_pGC->BSendSystemMessage( msgResponse );
	}
};

GC_REG_JOB(CGCBase, CGetSystemStatsJob, "CGetSystemStatsJob", k_EGCMsgGetSystemStats, k_EServerTypeGC);

//-----------------------------------------------------------------------------
class CGCJobAccountVacStatusChange : public CGCJob
{
public:
	CGCJobAccountVacStatusChange( CGCBase *pGC ) : CGCJob( pGC )	{}

	bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgGCHAccountVacStatusChange> msg( pNetPacket );
		
		if ( GGCBase()->GetAppID() != msg.Body().app_id() )
			return true;

		CSteamID steamID( msg.Body().steam_id() );
		bool bIsVacBanned = msg.Body().is_banned_now();

		// Fetch app details, but force them to be re-loaded
		bool bForceReload = true;
		const CAccountDetails *pAccountDetails = GGCBase()->YieldingGetAccountDetails( steamID, bForceReload );

		// Account details is up to date so just return
		if ( pAccountDetails && bIsVacBanned != pAccountDetails->BIsVacBanned() )
		{
			EmitWarning( SPEW_GC, 2, "VAC status didn't update for %s afetr receiving VacStatusChange and the force reloading the account details\n", steamID.Render() );
		}
		return true;
	}
};
GC_REG_JOB( CGCBase, CGCJobAccountVacStatusChange, "CGCJobAccountVacStatusChange", k_EGCMsgGCAccountVacStatusChange, k_EServerTypeGC );

//-----------------------------------------------------------------------------
class CGCJobAccountPhoneNumberChange : public CGCJob
{
public:
	CGCJobAccountPhoneNumberChange( CGCBase *pGC ) : CGCJob( pGC )	{}

	bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgGCHAccountPhoneNumberChange> msg( pNetPacket );

		if ( GGCBase()->GetAppID() != msg.Body().appid() )
			return true;

		CSteamID steamID( msg.Body().steamid() );
		CScopedSteamIDLock scopedLock( steamID );
		if ( !scopedLock.BYieldingPerformLock( __FILE__, __LINE__ ) )
		{
			EmitError( SPEW_GC, __FUNCTION__ ": Failed to lock steamid %s\n", steamID.Render() );
			return true;
		}

		bool bHasPhoneVerified = msg.Body().is_verified();
		bool bIsPhoneIdentifying = msg.Body().is_identifying();

		// Fetch app details, but force them to be re-loaded
		bool bForceReload = true;
		const CAccountDetails *pAccountDetails = GGCBase()->YieldingGetAccountDetails( steamID, bForceReload );

		// Account details is up to date so just return
		if ( pAccountDetails && ( bHasPhoneVerified != pAccountDetails->BIsPhoneVerified() ||
		                          bIsPhoneIdentifying != pAccountDetails->BIsPhoneIdentifying() ) )
		{
			EmitWarning( SPEW_GC, 2, "Phone status didn't update for %s afetr receiving PhoneNumberChange and force reloading the account details\n",
			             steamID.Render() );
		}

		GGCBase()->YldOnAccountPhoneVerificationChange( steamID );

		EmitInfo( SPEW_GC, 5, 5, "AccountPhoneVerificationChange for %s\n", steamID.Render() );

		return true;
	}
};
GC_REG_JOB( CGCBase, CGCJobAccountPhoneNumberChange, "CGCJobAccountPhoneNumberChange", k_EGCMsgAccountPhoneNumberChange, k_EServerTypeGC );

//-----------------------------------------------------------------------------
class CGCJobAccountTwoFactorChange : public CGCJob
{
public:
	CGCJobAccountTwoFactorChange( CGCBase *pGC ) : CGCJob( pGC )	{}

	bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgGCHAccountTwoFactorChange> msg( pNetPacket );

		if ( GGCBase()->GetAppID() != msg.Body().appid() )
			return true;

		CSteamID steamID( msg.Body().steamid() );
		bool bHasTwoFactor = msg.Body().twofactor_enabled();

		// Fetch app details, but force them to be re-loaded
		bool bForceReload = true;
		const CAccountDetails *pAccountDetails = GGCBase()->YieldingGetAccountDetails( steamID, bForceReload );

		// Account details is up to date so just return
		if ( pAccountDetails && bHasTwoFactor != pAccountDetails->BIsTwoFactorAuthEnabled() )
		{
			EmitWarning( SPEW_GC, 2, "VAC status didn't update for %s afetr receiving VacStatusChange and the force reloading the account details\n", steamID.Render() );
		}

		GGCBase()->YldOnAccountTwoFactorChange( steamID );

		EmitInfo( SPEW_GC, 5, 5, "AccountTwoFactorChange for %s\n", steamID.Render() );

		return true;
	}
};
GC_REG_JOB( CGCBase, CGCJobAccountTwoFactorChange, "CGCJobAccountTwoFactorChange", k_EGCMsgAccountTwoFactorChange, k_EServerTypeGC );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCBase::CGCBase( )
: 	m_mapSOCache( ),
	m_rbtreeSOCachesBeingLoaded( DefLessFunc( CSteamID ) ),
	m_rbtreeSOCachesWithDirtyVersions( DefLessFunc( CSteamID ) ),
	m_hashUserSessions( k_nUserSessionRunInterval/ k_cMicroSecPerShellFrame ),
	m_hashGSSessions( k_nGSSessionRunInterval/ k_cMicroSecPerShellFrame ),
	m_hashSteamIDLocks( k_nLocksRunInterval / k_cMicroSecPerShellFrame ),
	m_bStartupComplete( false ),
	m_bIsShuttingDown( false ),
	m_bStartProfiling( false ),
	m_bStopProfiling( false ),
	m_bDumpVprofImbalances( false ),
	m_nStartPlayingJobCount( 0 ),
	m_nRequestSessionJobsActive( 0 ),
	m_nLogonSurgeFramesRemaining( k_nMillion * 10 / k_cMicroSecPerShellFrame ), // stay in "logon surge" mode for at least 10 seconds after boot.
	m_mapStartPlayingQueueIndexBySteamID( DefLessFunc( CSteamID ) ),
	m_MsgRateLimit( max_user_messages_per_second ),
	m_nStartupCompleteTime( CRTime::RTime32TimeCur() ),
	m_nInitTime( CRTime::RTime32TimeCur() ),
	m_jobidFlushInventoryCacheAccounts( k_GIDNil ),
	m_numFlushInventoryCacheAccountsLastScheduled( 0 )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCBase::~CGCBase()
{
}


//-----------------------------------------------------------------------------
// Purpose: Remembers the app ID and host
//-----------------------------------------------------------------------------
bool CGCBase::BInit( AppId_t unAppID, const char *pchAppPath, IGameCoordinatorHost *pHost )
{
	VPROF_BUDGET( "CGCBase::BInit", VPROF_BUDGETGROUP_STEAM );

// Make sure we can't deploy debug GCs outside the dev environment
#ifdef _DEBUG
	if ( pHost->GetUniverse() != k_EUniverseDev )
	{
		//pHost->EmitMessage( SPEW_GC, SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, 
		//	CFmtStr( "The GC for App %u is a debug binary. Shutting down.\n", unAppID ) );
		//return false;
		pHost->EmitMessage( SPEW_GC.GetName(), SPEW_WARNING, SPEW_ALWAYS, LOG_ALWAYS, 
			CFmtStr( "The GC for App %u is a debug binary.\n", unAppID ) );
	}
#endif

	m_JobMgr.SetThreadPoolSize( GetThreadPoolSizeFromConVar() );

	MsgRegistrationFromEnumDescriptor( EGCSystemMsg_descriptor(), GCSDK::MT_GC_SYSTEM );
	MsgRegistrationFromEnumDescriptor( EGCBaseClientMsg_descriptor(), GCSDK::MT_GC );
	MsgRegistrationFromEnumDescriptor( EGCToGCMsg_descriptor(), GCSDK::MT_GC_SYSTEM );

	m_unAppID = unAppID;
	m_pHost = pHost;
	m_sPath = pchAppPath;
	SetGCHost( pHost );

	g_pGCBase = this;

	SetMinidumpFilenamePrefix( CFmtStr("dumps\\gc%d", m_unAppID) );

	// Make sure the assert dialog doesn't come up and hang the process in production
	//SetAssertDialogDisabled( pHost->GetUniverse() != k_EUniverseDev );
	SetAssertFailedNotifyFunc( CGCBase::AssertCallbackFunc );

	// init the time very early so CRTime::RTime32TimeCur will return the right thing
	CRTime::UpdateRealTime();

	m_hashUserSessions.Init( k_cGCUserSessionInit, k_cBucketGCUserSession );
	m_hashGSSessions.Init( k_cGCGSSessionInit, k_cBucketGCGSSession );
	m_hashSteamIDLocks.Init( k_cGCLocksInit, k_cBucketGCLocks );

	m_OutputFuncPrev = GetSpewOutputFunc();
	SpewOutputFunc( &ConsoleSpewFunc );
	EmitInfo( SPEW_GC, 1, 1, "CGCBase::BInit( AppID=%d, appPath=%s, sPath=%s )\n", unAppID, pchAppPath, m_sPath.String() );

	if ( !OnInit() )
		return false;

	DbgVerify( g_theMessageList.BInit( ) );

	/* 
	// @note Tom Bui: we don't need dynamic stats...yet.
	// when we do, we'll need to specify the how the values are aggregated over all the same GCs
	// and how the values should be treated
	KeyValuesAD pKVStats( "GCStats" );
	SystemStats_Update( pKVStats );
	CUtlBuffer buffer;
	KVPacker packer;
	if ( packer.WriteAsBinary( pKVStats, buffer ) )
	{
		CProtoBufMsg< CGCMsgSystemStatsSchema > msg( GCSDK::k_EGCMsgSystemStatsSchema );
		msg.Body().set_gc_app_id( GetAppID() );
		msg.Body().set_schema_kv( buffer.Base(), buffer.TellPut() );
		BSendSystemMessage( msg );
	}
	*/

	return BSendWebApiRegistration();
}


//-----------------------------------------------------------------------------
// Purpose: Report back to the host that startup is complete
//-----------------------------------------------------------------------------
void CGCBase::SetStartupComplete( bool bSuccess )
{
	// !KLUDGE! Fatal error messages on startup frequently get lost in the
	// mass of messages.  Let's spray a big error message box if we fail
	// to startup.  Ideally, the cause of the failure will be
	// spewed just above this box.
	if ( !bSuccess )
	{
		EmitError( SPEW_GC, "^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^\n" );
		EmitError( SPEW_GC, "GC failed to startup.  Error mesage is probably directly above\n" );
		EmitError( SPEW_GC, "**************************************************************\n" );
	}

	m_nStartupCompleteTime = CRTime::RTime32TimeCur();
	m_bStartupComplete = true;
	GGCHost()->StartupComplete( bSuccess );
}
 
uint32 CGCBase::GetGCUpTime() const
{ 
	return CRTime::RTime32TimeCur() - m_nInitTime; 
}
 
//-----------------------------------------------------------------------------
// Purpose: Starts a job to perform graceful shutdown
//-----------------------------------------------------------------------------
void CGCBase::Shutdown()
{
	VPROF_BUDGET( "CGCBase::Shutdown", VPROF_BUDGETGROUP_STEAM );
	m_DumpHTTPErrorsSchedule.Cancel();

	CGCShutdownJob *pJob = new CGCShutdownJob( this );
	pJob->StartJob( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Cleans up the GC to prepare for shutdown
//-----------------------------------------------------------------------------
void CGCBase::Uninit( )
{
	VPROF_BUDGET( "CGCBase::Uninit", VPROF_BUDGETGROUP_STEAM );

	OnUninit();

	// clean up all of the sessions and caches here so we can be sure it happens before the memory pools go away at static destruction time
	for( CGCUserSession **ppSession = m_hashUserSessions.PvRecordFirst(); ppSession != NULL; ppSession = m_hashUserSessions.PvRecordNext( ppSession ) )
	{
		delete (*ppSession);
	}
	m_hashUserSessions.RemoveAll();
	for( CGCGSSession **ppSession = m_hashGSSessions.PvRecordFirst(); ppSession != NULL; ppSession = m_hashGSSessions.PvRecordNext( ppSession ) )
	{
		delete (*ppSession);
	}
	m_hashGSSessions.RemoveAll();
	FOR_EACH_MAP_FAST( m_mapSOCache, nIndex )
	{
		// Remove from map before deleting, to prevent some debug
		// code from getting tangled up
		CGCSharedObjectCache *pCache = m_mapSOCache[nIndex];
		m_mapSOCache[nIndex] = NULL;
		m_mapSOCache.RemoveAt( nIndex );
		delete pCache;
	}
	m_mapSOCache.RemoveAll();
	m_rbtreeSOCachesBeingLoaded.RemoveAll();
	m_rbtreeSOCachesWithDirtyVersions.RemoveAll();
	m_hashSteamIDLocks.RemoveAll();
	
	GSchemaFull().Uninit();
	SpewOutputFunc( m_OutputFuncPrev );
}

GCConVar cv_flush_inventory_cache_jobs( "cv_flush_inventory_cache_jobs", "20", 0, "The maximum number of jobs flushing inventory caches that can be in flight at once, zero to disable flushing" );
GCConVar cv_flush_inventory_cache_contextid( "cv_flush_inventory_cache_contextid", "2" /* k_EEconContextBackpack */, 0, "Which context id we flush for Steam web user-facing inventory" );
GCConVar cv_flush_inventory_cache_spew( "cv_flush_inventory_cache_spew", "0", 0, "Controls spew level for jobs flushing inventory cache (0=off; 1=summary; 2=verbose)" );
class CFlushInventoryCacheAccountsJob : public CGCJob, public IYieldingParallelFarmJobHandler
{
public:
	CFlushInventoryCacheAccountsJob( CGCBase *pGC, CUtlRBTree< AccountID_t, int32, CDefLess< AccountID_t > > &rbAccounts ) : CGCJob( pGC )
	{
		m_rbAccounts.Swap( rbAccounts );
	}

	virtual bool BYieldingRunGCJob() OVERRIDE
	{
		if ( !m_rbAccounts.Count() )
		return false;
		if ( cv_flush_inventory_cache_jobs.GetInt() <= 0 )
			return false;

		bool bShouldSpew = ( cv_flush_inventory_cache_spew.GetInt() >= 1 );
		uint32 msTimeStart = 0;
		int numAccountsWorkload = m_rbAccounts.Count();
		if ( bShouldSpew )
		{
			msTimeStart = Plat_MSTime();
		}

		{	// Run parallel processing of the workload
			int numJobs = numAccountsWorkload;
			numJobs = MIN( cv_flush_inventory_cache_jobs.GetInt(), numJobs );
			numJobs = MAX( 1, numJobs );

			( void ) BYieldingExecuteParallel( numJobs, "YieldingFlushInventoryCacheAccountsJob" );
		}

		if ( bShouldSpew )
		{
			EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "IEconService/FlushInventoryCache: Batch for %d accounts completed in %u ms\n",
				numAccountsWorkload, Plat_MSTime() - msTimeStart );
		}

		return true;
	}

		virtual bool BYieldingRunWorkload( int iJobSequenceCounter, bool *pbWorkloadCompleted ) OVERRIDE
	{
		if ( m_rbAccounts.Count() )
		{
			int32 idxElement = m_rbAccounts.FirstInorder();
			AccountID_t unAccountID = m_rbAccounts.Element( idxElement );
			m_rbAccounts.RemoveAt( idxElement );

			( void ) BYieldingFlushRequest( unAccountID );
		}

		if ( !m_rbAccounts.Count() )
		{
			*pbWorkloadCompleted = true;
		}

		return true;
	}

		bool BYieldingFlushRequest( AccountID_t unAccountID )
	{
		bool bShouldSpew = ( cv_flush_inventory_cache_spew.GetInt() >= 2 );
		uint32 msTimeStart = 0;
		if ( bShouldSpew )
		{
			msTimeStart = Plat_MSTime();
		}

		CSteamID steamID( GGCInterface()->ConstructSteamIDForClient( unAccountID ) );
		CSteamAPIRequest apiRequest( k_EHTTPMethodPOST, "IEconService", "FlushInventoryCache", 1 );
		apiRequest.SetPOSTParamUInt32( "appid", GGCBase()->GetAppID() );
		apiRequest.SetPOSTParamUInt64( "steamid", steamID.ConvertToUint64() );
		apiRequest.SetPOSTParamUInt32( "contextid", 2 );

		CHTTPResponse apiResponse;
		bool bSucceededQuery = m_pGC->BYieldingSendHTTPRequest( &apiRequest, &apiResponse );
		if ( !bSucceededQuery )
		{
			EmitErrorRatelimited( SPEW_GC, "IEconService/FlushInventoryCache: Web call did not get a response for %s.\n", steamID.Render() );
		}
		else if ( k_EHTTPStatusCode200OK != apiResponse.GetStatusCode() )
		{
			EmitErrorRatelimited( SPEW_GC, "IEconService/FlushInventoryCache: Web call got failure code %d for %s\n", apiResponse.GetStatusCode(), steamID.Render() );
			bSucceededQuery = false;
		}

		if ( bSucceededQuery )
		{
			// Have a valid response
			KeyValuesAD pKVResponse( "response" );
			pKVResponse->UsesEscapeSequences( true );
			if ( !pKVResponse->LoadFromBuffer( "webResponse", *apiResponse.GetBodyBuffer() ) )
			{
				EmitErrorRatelimited( SPEW_GC, "IEconService/FlushInventoryCache: Web call got code %d for %s, but failed to parse response\n", apiResponse.GetStatusCode(), steamID.Render() );
				bSucceededQuery = false;
			}
			else if ( !pKVResponse->GetBool( "success" ) )
			{
				// We got a response, and it's not success
				EmitErrorRatelimited( SPEW_GC, "IEconService/FlushInventoryCache: Web call got code %d for %s, but not success\n", apiResponse.GetStatusCode(), steamID.Render() );
				bSucceededQuery = false;
			}
		}

		if ( bShouldSpew )
		{
			EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "IEconService/FlushInventoryCache: Web call for %s %s in %u ms\n",
				steamID.Render(), bSucceededQuery ? "succeeded" : "failed", Plat_MSTime() - msTimeStart );
		}

		return bSucceededQuery;
	}

public:
	CUtlRBTree< AccountID_t, int32, CDefLess< AccountID_t > > m_rbAccounts;
};

//-----------------------------------------------------------------------------
// Purpose: Called every frame. Mostly updates times and pulses the job manager
//-----------------------------------------------------------------------------
bool CGCBase::BMainLoopOncePerFrame( uint64 ulLimitMicroseconds )
{
	// if we don't have a GCHost yet, don't do any work per frame
	if( !GGCHost() )
		return false;

#ifndef STEAM
	CRTime::UpdateRealTime();
#endif


#ifdef VPROF_ENABLED
	// Make sure we end the frame at the root node
	if ( !g_VProfCurrentProfile.AtRoot() && m_bDumpVprofImbalances )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "VProf not at root at end of frame. Stack:\n" );
	}

	for( int i = 0; !g_VProfCurrentProfile.AtRoot() && i < 100; i++ )
	{
		if ( m_bDumpVprofImbalances )
		{
			EmitWarning( SPEW_GC, SPEW_ALWAYS, "  %s\n", g_VProfCurrentProfile.GetCurrentNode()->GetName() );
		}
		g_VProfCurrentProfile.ExitScope();
	}

	g_VProfCurrentProfile.MarkFrame();

	if ( m_bStopProfiling || m_bStartProfiling )
	{
		while ( g_VProfCurrentProfile.IsEnabled() )
		{
			g_VProfCurrentProfile.Stop();
		}
		m_bStopProfiling = false;

		if ( m_bStartProfiling )
		{
			g_VProfCurrentProfile.Reset();
			g_VProfCurrentProfile.Start();
			m_bStartProfiling = false;
		}
	}
#endif

	VPROF_BUDGET( "Main Loop", VPROF_BUDGETGROUP_STEAM );

	CLimitTimer limitTimer;
	limitTimer.SetLimit( ulLimitMicroseconds );
	CJobTime::UpdateJobTime( k_cMicroSecPerShellFrame );

	bool bWorkRemaining = m_JobMgr.BFrameFuncRunSleepingJobs( limitTimer );

	//run all of our frame functions
	GFrameFunctionMgr().RunFrame( limitTimer );

	{
		VPROF_BUDGET( "Run Sessions", VPROF_BUDGETGROUP_STEAM );

		m_AccountDetailsManager.MarkFrame();
		m_hashUserSessions.StartFrameSchedule( true );
		m_hashGSSessions.StartFrameSchedule( true );
		m_hashSteamIDLocks.StartFrameSchedule( true );
		bool bUsersFinished = false, bGSFinished = false;
		while( !limitTimer.BLimitReached() && ( !bUsersFinished || !bGSFinished ) )
		{
			if( !bUsersFinished )
			{
				CGCUserSession **ppSession = m_hashUserSessions.PvRecordRun();
				if ( ppSession && *ppSession )
				{
					(*ppSession)->Run();
				}	
				else
				{
					bUsersFinished = true;
				}
				if ( m_hashUserSessions.BCompletedPass() )
				{
					FinishedMainLoopUserSweep();
				}
			}

			if( !bGSFinished )
			{
				CGCGSSession **ppSession = m_hashGSSessions.PvRecordRun();
				if ( ppSession && *ppSession )
				{
					(*ppSession)->Run();
				}	
				else
				{
					bGSFinished = true;
				}
			}
		}
	}

	{
		VPROF_BUDGET( "UpdateSOCacheVersions", VPROF_BUDGETGROUP_STEAM );
		UpdateSOCacheVersions();
	}

	if( m_llStartPlaying.Count() > 0 )
	{
		VPROF_BUDGET( "StartStartPlayingJobs", VPROF_BUDGETGROUP_STEAM );

		int nJobsNeeded = min( m_llStartPlaying.Count(), cv_concurrent_start_playing_limit.GetInt() - m_nStartPlayingJobCount );
		while( nJobsNeeded > 0 )
		{
			nJobsNeeded--;
			m_nStartPlayingJobCount++;

			CExecuteStartPlayingJob *pJob = new CExecuteStartPlayingJob( this );
			pJob->StartJob( NULL );
		}
	}

	// Decide if we should be in logon surge
	bool bShouldBeInlogonSurge = 
		m_llStartPlaying.Count() >= cv_logon_surge_start_playing_limit.GetInt();
		// This might be a good idea, but let's see what the real numbers are during logon surge.
		//|| m_nRequestSessionJobsActive >= cv_logon_surge_request_session_jobs.GetInt();

	// Check if we're already in logon surge, is it time to check if we should leave,
	// and should we dump our status periodically?
	const int k_nLogonSurgeFrameInterval = k_nMillion * 10 / k_cMicroSecPerShellFrame;
	if ( m_nLogonSurgeFramesRemaining > 0 )
	{

		// Currently in logon surge
		--m_nLogonSurgeFramesRemaining;
		if ( m_nLogonSurgeFramesRemaining == 0 )
		{

			// Time to check for leaving logon surge mode.
			// Should I flip the flag off?
			if ( bShouldBeInlogonSurge )
			{
				// We're still in logon surge.  Schedule another check
				// a few frames from now, and dump our status.
				m_nLogonSurgeFramesRemaining = k_nLogonSurgeFrameInterval;
				Dump();
			}
			else
			{
				// We're over the hump!
				EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "** LOGON SURGE COMPLETED **\n" );
			}
		}
	}
	else if ( bShouldBeInlogonSurge )
	{
		// We finished logon surge one, but now we are re-entering it.
		// This usually doesn't happen.  This is suspicious.
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "RE-ENTERING logon surge mode!\n" );
		m_nLogonSurgeFramesRemaining = k_nLogonSurgeFrameInterval;
	}
	else
	{
		// Not in logon surge.  make sure flag is slammed to zero
		m_nLogonSurgeFramesRemaining = 0;
	}

	// Flush inventory cache for accounts
	if ( m_rbFlushInventoryCacheAccounts.Count() && ( ( m_jobidFlushInventoryCacheAccounts == k_GIDNil ) ||
		!GetJobMgr().BJobExists( m_jobidFlushInventoryCacheAccounts ) ) )
	{
		m_numFlushInventoryCacheAccountsLastScheduled = m_rbFlushInventoryCacheAccounts.Count();
		m_jobidFlushInventoryCacheAccounts = StartNewJobDelayed( new CFlushInventoryCacheAccountsJob( this, m_rbFlushInventoryCacheAccounts ) )->GetJobID();
	}

	bool bSubRet = OnMainLoopOncePerFrame( limitTimer );
	return bWorkRemaining || bSubRet;
}

bool CGCBase::BShouldThrottleLowServiceLevelWebAPIJobs() const
{

	// Always throttle them during logon surge.
	if ( BIsInLogonSurge() )
		return true;

	// Check threshold
	if ( m_JobMgr.CountJobs() > cv_webapi_throttle_job_threshold.GetInt() )
		return true;

	// We are not too busy, we can service the request
	return false;
}


bool CGCBase::BMainLoopUntilFrameCompletion( uint64 ulLimitMicroseconds )
{
	VPROF_BUDGET( "Main Loop", VPROF_BUDGETGROUP_STEAM );

	CLimitTimer limitTimer;
	limitTimer.SetLimit( ulLimitMicroseconds );
	bool bRet = m_JobMgr.BFrameFuncRunYieldingJobs( limitTimer );

	bRet |= GSDOCache().BFrameFuncRunJobsUntilCompleted( limitTimer );
	bRet |= GSDOCache().BFrameFuncRunMemcachedQueriesUntilCompleted( limitTimer );
	bRet |= GSDOCache().BFrameFuncRunSQLQueriesUntilCompleted( limitTimer );
	bRet |= m_AccountDetailsManager.BExpireRecords( limitTimer );

	bool bSubRet = OnMainLoopUntilFrameCompletion( limitTimer );

	bRet |= GFrameFunctionMgr().RunFrameTick( limitTimer );

	{
		VPROF_BUDGET( "Expire locks", VPROF_BUDGETGROUP_STEAM );

		for ( CLock *pLock = m_hashSteamIDLocks.PvRecordRun(); NULL != pLock; pLock = m_hashSteamIDLocks.PvRecordRun() )
		{
			if ( !pLock->BIsLocked() && pLock->GetMicroSecondsSinceLock() > k_cMicroSecLockLifetime )
			{
				m_hashSteamIDLocks.Remove( pLock );
			}

			if ( limitTimer.BLimitReached() )
				return true;
		}
	}

	return bRet || bSubRet;
 }

//-----------------------------------------------------------------------------
// Purpose: Called when we get to the end of a user session Run() sweep, and
//			are about to start over with the first session in the list.
//-----------------------------------------------------------------------------
void CGCBase::FinishedMainLoopUserSweep()
{
	// Base class does nothing
}


//-----------------------------------------------------------------------------
// Purpose: Queues up a start playing request that we should process when we
//			get a chance.
//-----------------------------------------------------------------------------
void CGCBase::QueueStartPlaying( const CSteamID & steamID, const CSteamID & gsSteamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData )
{
	MEM_ALLOC_CREDIT_( "QueueStartPlaying" );

	Assert( steamID.BIndividualAccount() || steamID.BGameServerAccount() );
	Assert( steamID.IsValid() );

	// Should be one-to-one correspondence in these data structures
	Assert( (size_t)m_mapStartPlayingQueueIndexBySteamID.Count() == (size_t)m_llStartPlaying.Count() );

	// !FIXME! Here we really should check whether they already have a session.
	// if so, we've already gone through all the startplaying work and shouldn't
	// repeat it.  We might just need to kick the communications or make
	// sure they are on the right game server.

	// Check if we already have an entry in the queue for this guy.
	StartPlayingWork_t *pWork = NULL;
	int nMapIndex = m_mapStartPlayingQueueIndexBySteamID.Find( steamID );
	if ( nMapIndex != m_mapStartPlayingQueueIndexBySteamID.InvalidIndex() )
	{
		// We already have an entry for this guy, let's update this one, rather than creating a new one
		int nQueueIndex = m_mapStartPlayingQueueIndexBySteamID[ nMapIndex ];
		pWork = &m_llStartPlaying[ nQueueIndex ];

		// Sanity check data structures.  I'd use an assert,
		// but this is going live in an environment without
		// asserts enabled, so I need to use spew.
		if ( pWork->m_steamID == steamID )
		{
			// Don't leak user data, if we had any
			delete pWork->m_pVarData;
			pWork->m_pVarData = NULL;

//			// This could definitely happen occasionally, but if it happens with massive frequency,
//			// something is wrong
//			if ( gsSteamID == pWork->m_gsSteamID )
//			{
//				EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "Got StartPlaying message for %s, who was already in the startplaying queue for the same gameserver %s.\n", steamID.Render(), gsSteamID.Render() );
//			}
//			else
//			{
//				EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "Got StartPlaying message for %s, who was already in the startplaying queue; changing gameserver %s -> %s.\n", steamID.Render(), pWork->m_gsSteamID.Render(), gsSteamID.Render() );
//			}
		}
		else
		{
			EmitWarning( SPEW_GC, SPEW_ALWAYS, "Start playing queue corruption!  Map entry points to wrong queue entry!\n" );
			pWork = NULL;
			m_mapStartPlayingQueueIndexBySteamID.RemoveAt( nMapIndex );
		}
	}
	else
	{
//		EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "Got StartPlaying message for %s, new queue for gameserver %s.\n", steamID.Render(), gsSteamID.Render() );
	}

	// Need to create a new entry?
	if ( pWork == NULL )
	{
		// Create a new queue entry
		int nQueueIndex = m_llStartPlaying.AddToTail();
		pWork = &m_llStartPlaying[ nQueueIndex ];

		// Add it to the steam ID map, so we can locate this guy quickly in the future
		m_mapStartPlayingQueueIndexBySteamID.Insert( steamID, nQueueIndex );
	}

	// Fill in the queue entry with the latest details
	pWork->m_steamID = steamID;
	pWork->m_gsSteamID = gsSteamID;
	pWork->m_unServerAddr = unServerAddr;
	pWork->m_usServerPort = usServerPort;

	if( cubVarData )
	{
		pWork->m_pVarData = new CUtlBuffer;
		pWork->m_pVarData->Put( pubVarData, cubVarData );
	}
	else
	{
		pWork->m_pVarData = NULL;
	}

	// Should be one-to-one correspondence in these data structures
	Assert( (size_t)m_mapStartPlayingQueueIndexBySteamID.Count() == (size_t)m_llStartPlaying.Count() );
}

//-----------------------------------------------------------------------------
bool CGCBase::BRemoveStartPlayingQueueEntry( const CSteamID & steamID )
{
	int nMapIndex = m_mapStartPlayingQueueIndexBySteamID.Find( steamID );
	if ( nMapIndex == m_mapStartPlayingQueueIndexBySteamID.InvalidIndex() )
	{
		return false;
	}

	//EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "Removed startplaying queue entry for %s.\n", steamID.Render() );

	// Locate queue entry, make sure it matches, and remote it
	int nQueueIndex = m_mapStartPlayingQueueIndexBySteamID[ nMapIndex ];
	if ( m_llStartPlaying[ nQueueIndex ].m_steamID == steamID )
	{
		delete m_llStartPlaying[ nQueueIndex ].m_pVarData;
		m_llStartPlaying.Remove( nQueueIndex );
	}
	else
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Start playing queue corruption!  Map entry doesn't point to matching queue index (found while removing entry in BRemoveStartPlayingQueueEntry)!\n" );
	}

	// Remove from map
	m_mapStartPlayingQueueIndexBySteamID.RemoveAt( nQueueIndex );

	// Found and removed
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Pull the next startplaying job off the queue and executes it
//-----------------------------------------------------------------------------
void CGCBase::YieldingExecuteNextStartPlaying()
{
	// maybe we have nothing to do!
	if( m_llStartPlaying.Count() > 0 )
	{
		// Execute the entry at the head
		YieldingExecuteStartPlayingQueueEntryByIndex( m_llStartPlaying.Head() );
	}
	m_nStartPlayingJobCount--;
}

//-----------------------------------------------------------------------------
// Purpose: Executes a single entry from the start playing queue, given the linked list handle
//-----------------------------------------------------------------------------
void CGCBase::YieldingExecuteStartPlayingQueueEntryByIndex( int idxStartPlayingQueue )
{
	VPROF_BUDGET( "CGCBase::YieldingExecuteStartPlayingQueueEntryByIndex - LinkedList", VPROF_BUDGETGROUP_STEAM );
	// Remove the entry from the queue
	StartPlayingWork_t work = m_llStartPlaying[ idxStartPlayingQueue ];
	m_llStartPlaying.Remove( idxStartPlayingQueue );

	VPROF_BUDGET( "CGCBase::YieldingExecuteStartPlayingQueueEntryByIndex", VPROF_BUDGETGROUP_STEAM );
	// Remove it from the Steam ID map, too.
	int nMapIndex = m_mapStartPlayingQueueIndexBySteamID.Find( work.m_steamID );
	if ( nMapIndex == m_mapStartPlayingQueueIndexBySteamID.InvalidIndex() )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Start playing queue corruption!  Queue entry is not in map!\n" );
	}
	else if ( m_mapStartPlayingQueueIndexBySteamID[ nMapIndex ] != idxStartPlayingQueue )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Start playing queue corruption!  Map entry doesn't have proper queue index!\n" );
	}
	else
	{
		m_mapStartPlayingQueueIndexBySteamID.RemoveAt( nMapIndex );
	}

	// Do the work.
	if ( work.m_steamID.BIndividualAccount() )
	{
		YieldingStartPlaying( work.m_steamID, work.m_gsSteamID, work.m_unServerAddr, work.m_usServerPort, work.m_pVarData );
	}
	else if ( work.m_steamID.BGameServerAccount() )
	{
		const uint8 *pVarData = NULL;
		uint32 cubVarData = 0;
		if ( work.m_pVarData != NULL )
		{
			pVarData = (const uint8 *)work.m_pVarData->Base();
			cubVarData = work.m_pVarData->TellMaxPut();
		}
		YieldingStartGameserver( work.m_steamID, work.m_unServerAddr, work.m_usServerPort, pVarData, cubVarData );
	}
	else
	{
		AssertMsg1( false, "Bogus steam ID %s in start playing queue", work.m_steamID.Render() );
	}

	// Clean up
	delete work.m_pVarData;
}

void CGCBase::SetUserSessionDetails( CGCUserSession *pUserSession, KeyValues *pkvDetails )
{
	if( pkvDetails )
	{
		pUserSession->m_unIPPublic = pkvDetails->GetInt( "ip", 0 );
		pUserSession->m_osType = static_cast<EOSType>( pkvDetails->GetInt( "osType", k_eOSUnknown ) );
		pUserSession->m_bIsTestSession = pkvDetails->GetInt( "isTestSession", 0 ) != 0;
		pUserSession->m_bIsSecure = pkvDetails->GetInt( "secure", 0 ) != 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Does the real work when a player starts playing (inside a job)
//-----------------------------------------------------------------------------
void CGCBase::YieldingStartPlaying( const CSteamID & steamID, const CSteamID & gsSteamID, uint32 unServerAddr, uint16 usServerPort, CUtlBuffer *pVarData )
{
	VPROF_BUDGET( "CGCBase::YieldingStartPlaying", VPROF_BUDGETGROUP_STEAM );
	if ( m_bIsShuttingDown )
		return;

	if( !BYieldingLockSteamID( steamID, __FILE__, __LINE__ ) )
	{
		EmitError( SPEW_GC, "Failed to lock steamID %s in YieldingStartPlaying\n", steamID.Render() );
		return;
	}

	// if var data came with this StartPlaying message, parse it into a KV and stick it on the session
	KeyValues *pkvDetails = NULL;
	if( pVarData )
	{
		MEM_ALLOC_CREDIT_("StartPlaying - SessionDetails" );
		pkvDetails = new KeyValues( "SessionDetails" );
		if( !pkvDetails->ReadAsBinary( *pVarData ) )
		{
			EmitError( SPEW_GC, "Unable to parse session details for %s\n", steamID.Render() );
			pkvDetails->deleteThis();
			pkvDetails = NULL;
		}
	}

	CGCUserSession *pSession = FindUserSession( steamID );
	if( !pSession )
	{
		// Load their SO cache.  Remember, we already have their steam ID locked.
		VPROF_BUDGET( "CGCBase::YieldingStartPlaying - Load SOCache", VPROF_BUDGETGROUP_STEAM );
		CGCSharedObjectCache *pSOCache = YieldingFindOrLoadSOCache( steamID );
		if ( !pSOCache )
		{
			EmitError( SPEW_GC, "Failed to get cache for user %s\n", steamID.Render() );
			return;
		}

		// Create session of app-specific type
		VPROF_BUDGET( "CGCBase::YieldingStartPlaying - CreateUserSession", VPROF_BUDGETGROUP_STEAM );
		pSession = CreateUserSession( steamID, pSOCache );
		if ( !pSession )
		{
			EmitError( SPEW_GC, "Failed to create user session for %s\n", steamID.Render() );
			return;
		}

		VPROF_BUDGET( "CGCBase::YieldingStartPlaying - LRU Update", VPROF_BUDGETGROUP_STEAM );
		RemoveCacheFromLRU( pSOCache );

		CGCUserSession **ppSession = m_hashUserSessions.PvRecordInsert( steamID.ConvertToUint64() );
		*ppSession = pSession;

		SetUserSessionDetails( pSession, pkvDetails );

		// Do game-specific logic here.  Note that we're still holding the game server
		// lock...
		VPROF_BUDGET( "CGCBase::YieldingStartPlaying - Game-specific start playing", VPROF_BUDGETGROUP_STEAM );
		YieldingSessionStartPlaying( pSession );
	}
	else if ( pSession->BIsShuttingDown() )
	{
		pkvDetails->deleteThis();
		pkvDetails = NULL;
		return;
	}
	else
	{
		// Update secure flag, etc from KV details, if any
		SetUserSessionDetails( pSession, pkvDetails );
	}

	if ( pkvDetails )
	{
		pkvDetails->deleteThis();
		pkvDetails = NULL;
	}

	VPROF_BUDGET( "CGCBase::YieldingStartPlaying - Game Server binding", VPROF_BUDGETGROUP_STEAM );
	// Make sure the server exists and then try to join it
	if ( gsSteamID.IsValid() && gsSteamID.BGameServerAccount() && BYieldingLockSteamID( gsSteamID, __FILE__, __LINE__ ) )
	{

		// First, try to obtain a session through ordinary means, by validating
		// the session
		if ( YieldingGetLockedGSSession( gsSteamID, __FILE__, __LINE__ ) != NULL )
		{
			// Maintain lock balance
			UnlockSteamID( gsSteamID );
		}
		else
		{
			// Failed to get a session --- probably an AM is down.
			// This is hopefully relatively rare, as it's not ideal.
			// log it
			if ( enable_startplaying_gameserver_creation_spew.GetBool() )
			{
				netadr_t serverAdr( unServerAddr, usServerPort );
				EmitInfo( SPEW_GC, 2, LOG_ALWAYS, "Creating gameserver session %s @ %s as a result of user %s StartPlaying.\n", gsSteamID.Render(), serverAdr.ToString(), steamID.Render() );
			}
			YieldingFindOrCreateGSSession( gsSteamID, unServerAddr, usServerPort, NULL, 0 );
		}

		// Mark that we are joined to this server
		pSession->BSetServer( gsSteamID );

		// Done, clean up lock
		UnlockSteamID( gsSteamID );
	}
	else
	{
		// Steam was sometimes sending us messages with zero Steam ID, even when we're on a server.
		if ( cv_debug_steam_startplaying.GetBool() )
			EmitInfo( SPEW_GC, 1, 1, "YieldingStartPlaying ( user = %s ) with invalid GS steam ID %s, calling LeaveServer\n", steamID.Render(), gsSteamID.Render() );

		pSession->BLeaveServer();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a player stops playing our game
//-----------------------------------------------------------------------------
void CGCBase::YieldingStopPlaying( const CSteamID & steamID )
{
	// Should be one-to-one correspondence in these data structures
	Assert( (size_t)m_mapStartPlayingQueueIndexBySteamID.Count() == (size_t)m_llStartPlaying.Count() );

	// Check if they have an entry in the startplaying queue, then get rid of it!
	BRemoveStartPlayingQueueEntry( steamID );

	if ( !BLockSteamIDImmediate( steamID ) )
	{
		CGCUserSession *pSession = FindUserSession( steamID );
		if ( !pSession )
		{
			return;
		}

		pSession->SetIsShuttingDown( true );
		if( !BYieldingLockSteamID( steamID, __FILE__, __LINE__ ) )
		{
			EmitError( SPEW_GC, "Unable to lock steamID %s in YieldingStopPlaying\n", steamID.Render() );
			return;
		}
	}

	CGCUserSession *pSession = FindUserSession( steamID );
	if( pSession )
	{
		pSession->BLeaveServer();
		YieldingSessionStopPlaying( pSession );
		if( pSession->GetSOCache() )
		{
			AddCacheToLRU( pSession->GetSOCache() );
		}
		m_hashUserSessions.Remove( steamID.ConvertToUint64() );
		delete pSession;
	}

	// Clean up lock.  Even if the session is gone and there's nothing
	// for the lock to protect, we need this to avoid spurious asserts that check
	// lock imbalance
	UnlockSteamID( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a gameserver stops running for our game
//-----------------------------------------------------------------------------
void CGCBase::YieldingStartGameserver( const CSteamID & steamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData )
{
	VPROF_BUDGET( "CGCBase::YieldingStartGameserver", VPROF_BUDGETGROUP_STEAM );
	if ( m_bIsShuttingDown )
		return;

	if( !BYieldingLockSteamID( steamID, __FILE__, __LINE__ ) )
	{
		EmitError( SPEW_GC, "Failed to lock steamID %s in YieldingStartGameserver\n", steamID.Render() );
		return;
	}

	YieldingFindOrCreateGSSession( steamID, unServerAddr, usServerPort, pubVarData, cubVarData );

	// Clean up
	UnlockSteamID( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a gameserver stops running for our game
//-----------------------------------------------------------------------------
void CGCBase::YieldingStopGameserver( const CSteamID & steamID )
{
	// Should be one-to-one correspondence in these data structures
	Assert( (size_t)m_mapStartPlayingQueueIndexBySteamID.Count() == (size_t)m_llStartPlaying.Count() );

	// Check if they have an entry in the startplaying queue, then get rid of it!
	BRemoveStartPlayingQueueEntry( steamID );

	if ( !BLockSteamIDImmediate( steamID ) )
	{
		CGCGSSession *pSession = FindGSSession( steamID );
		if ( !pSession )
		{
			return;
		}

		pSession->SetIsShuttingDown( true );
		if( !BYieldingLockSteamID( steamID, __FILE__, __LINE__ ) )
		{
			EmitError( SPEW_GC, "Unable to lock steamID %s in YieldingStopGameserver\n", steamID.Render() );
			return;
		}
	}
	
	CGCGSSession *pSession = FindGSSession( steamID );
	if( pSession )
	{
		pSession->RemoveAllUsers();
		YieldingSessionStopServer( pSession );
		if( pSession->GetSOCache() )
		{
			AddCacheToLRU( pSession->GetSOCache() );
		}
		m_hashGSSessions.Remove( steamID.ConvertToUint64() );
		delete pSession;
	}

	// Clean up lock.  Even if the session is gone and there's nothing
	// for the lock to protect, we need this to avoid spurious asserts that check
	// lock imbalance
	UnlockSteamID( steamID );
}

IMsgNetPacket *CreateIMsgNetPacket( GCProtoBufMsgSrc eReplyType, const CSteamID senderID, uint32 nGCDirIndex, uint32 unMsgType, void *pubData, uint32 cubData )
{
	VPROF_BUDGET( "CreateIMsgNetPacket", VPROF_BUDGETGROUP_STEAM );

	if( 0 != ( unMsgType & k_EMsgProtoBufFlag ) )
	{
		if ( cubData < sizeof( ProtoBufMsgHeader_t ) )
		{
			uint32 unMsgTypeNoFlag = unMsgType & (~k_EMsgProtoBufFlag);
			AssertMsg3( false, "Received packet %s(%u) from %s less than the minimum protobuf size", PchMsgNameFromEMsg( unMsgTypeNoFlag ), unMsgTypeNoFlag, senderID.Render() );
			return NULL;
		}

		// make a new packet for the message so we can dispatch it
		// The CNetPacket takes ownership of the buffer allocated above
		CNetPacket *pGCPacket = CNetPacketPool::AllocNetPacket();
		pGCPacket->Init( cubData );

		// copy the bits for the message over to the full size buffer
		Q_memcpy( pGCPacket->PubData(), pubData, cubData );

		CProtoBufNetPacket *pMsgNetPacket = new CProtoBufNetPacket( pGCPacket, eReplyType, senderID, nGCDirIndex, unMsgType & ( ~k_EMsgProtoBufFlag ) );

		// release the inner packet since the wrapper now has a ref to it
		pGCPacket->Release();

		if ( !pMsgNetPacket->IsValid() )
		{
			pMsgNetPacket->Release();
			return NULL;
		}
		
		return pMsgNetPacket;
	}
	else
	{
		//note that we do not currently support reply to GC messages through this pipeline
		AssertMsg( eReplyType != GCProtoBufMsgSrc_FromGC, "Warning: Encountered a message from GC to GC that was not of protobuff type, will be unable to reply to this message. Message type: %d", unMsgType );

		if ( cubData < sizeof( GCMsgHdrEx_t ) - sizeof( GCMsgHdr_t ) )
		{
			AssertMsg( false, "Received packet %s(%u) from %s less than the minimum struct size", PchMsgNameFromEMsg( unMsgType ), unMsgType, senderID.Render() );
			return NULL;
		}

		// Determine the size of the packet. sizeof(GCMsgHdr_t) was not sent as part of the data
		uint32 unFullSize = cubData + sizeof( GCMsgHdr_t );

		// make a new packet for the message so we can dispatch it
		// The CNetPacket takes ownership of the buffer allocated above
		CNetPacket *pGCPacket = CNetPacketPool::AllocNetPacket();
		pGCPacket->Init( unFullSize );

		//fill in our header and copy over the body
		uint8 *pFullPacket = pGCPacket->PubData();

		// get the header so we can fix it up
		GCMsgHdrEx_t *pHdr = (GCMsgHdrEx_t *)pFullPacket;
		//pHdr->m_nSrcGCDirIndex = nGCDirIndex;
		pHdr->m_eMsg = unMsgType;
		pHdr->m_ulSteamID = senderID.ConvertToUint64();

		// copy the bits for the message over to the full size buffer
		Q_memcpy( pFullPacket+sizeof(GCMsgHdr_t), pubData, cubData );

		
		CStructNetPacket *pMsgNetPacket = new CStructNetPacket( pGCPacket );

		// release the packet
		pGCPacket->Release();
		return pMsgNetPacket;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Processes an incoming message from the client by turning it into a 
//			CGCMsg and sending it on to a job.
//-----------------------------------------------------------------------------
void CGCBase::MessageFromClient( const CSteamID & senderID, uint32 unMsgType, void *pubData, uint32 cubData )
{
	VPROF_BUDGET( "CGCBase::MessageFromClient", VPROF_BUDGETGROUP_STEAM );
 
	// if we don't have a GCHost yet, we won't be able to do much with this message
	if( !GGCHost() )
		return;

	if ( OnMessageFromClient( senderID, unMsgType, pubData, cubData ) )
		return;

	// Rate limit messages from ordinary clients
	if ( senderID.IsValid() )
	{
		MsgType_t eMsg = unMsgType & ~k_EMsgProtoBufFlag;
		if ( m_MsgRateLimit.BIsRateLimited( senderID, eMsg ) )
		{
			g_RateLimitTracker.TrackRateLimitedMsg( senderID, eMsg );
			return;
		}
	}

	// !FIXME! DOTAMERGE
	uint32 nGCDirIndex = 0; // GetGCDirIndex()
	IMsgNetPacket *pMsgNetPacket = CreateIMsgNetPacket( GCProtoBufMsgSrc_FromSteamID, senderID, nGCDirIndex, unMsgType, pubData, cubData );
	if ( NULL == pMsgNetPacket )
		return;

	// dispatch the packet (some messages require special consideration) 
	switch( unMsgType )
	{
		case k_EGCMsgWGRequest:
			m_wgJobMgr.BHandleMsg( pMsgNetPacket );
			g_theMessageList.TallySendMessage( pMsgNetPacket->GetEMsg(), cubData );
			break;

		default:
			GetJobMgr().BRouteMsgToJob( this, pMsgNetPacket, JobMsgInfo_t( pMsgNetPacket->GetEMsg(), pMsgNetPacket->GetSourceJobID(), pMsgNetPacket->GetTargetJobID(), k_EServerTypeGC ) );
			g_theMessageList.TallySendMessage( pMsgNetPacket->GetEMsg(), cubData );
			break;
	}

	// release the packet
	pMsgNetPacket->Release();
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message to the given SteamID
//-----------------------------------------------------------------------------
bool CGCBase::BSendGCMsgToClient( const CSteamID & steamIDTarget, const CGCMsgBase& msg )
{
	g_theMessageList.TallySendMessage( msg.Hdr().m_eMsg, msg.CubPkt() - sizeof(GCMsgHdr_t) );
	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - SendMessageToClient", VPROF_BUDGETGROUP_STEAM );
		return m_pHost->BSendMessageToClient( m_unAppID, steamIDTarget, msg.Hdr().m_eMsg, msg.PubPkt() + sizeof(GCMsgHdr_t), msg.CubPkt() - sizeof(GCMsgHdr_t) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used to send protobuf system messages to a client
//-----------------------------------------------------------------------------
class CProtoBufClientSendHandler : public CProtoBufMsgBase::IProtoBufSendHandler
{
public:
	CProtoBufClientSendHandler( const CSteamID & steamIDTarget ) 
		: m_steamIDTarget( steamIDTarget ), m_cubSent( 0 ) {}
	virtual bool BAsyncSend( MsgType_t eMsg, const uint8 *pubMsgBytes, uint32 cubSize ) OVERRIDE
	{	
		m_cubSent = cubSize;
		// !FIXME! DOTAMERGE
		//return GGCInterface()->BProcessSystemMessage( eMsg | k_EMsgProtoBufFlag, pubMsgBytes, cubSize );
		g_theMessageList.TallySendMessage( eMsg & ~k_EMsgProtoBufFlag, cubSize );
		VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
		{
			VPROF_BUDGET( "GCHost - SendMessageToClient (ProtoBuf)", VPROF_BUDGETGROUP_STEAM );
			return GGCHost()->BSendMessageToClient( GGCBase()->GetAppID(), m_steamIDTarget, eMsg | k_EMsgProtoBufFlag, pubMsgBytes, cubSize );
		}
	}
	uint32 GetCubSent() const { return m_cubSent; }
private:
	uint32 m_cubSent;
	CSteamID m_steamIDTarget;
};

//-----------------------------------------------------------------------------
// Purpose: Used to send protobuf system messages into the GC
//-----------------------------------------------------------------------------
class CProtoBufSystemSendHandler : public CProtoBufMsgBase::IProtoBufSendHandler
{
public:
	CProtoBufSystemSendHandler() 
		: m_cubSent( 0 ) {}
	virtual bool BAsyncSend( MsgType_t eMsg, const uint8 *pubMsgBytes, uint32 cubSize ) OVERRIDE
	{	
		m_cubSent = cubSize;
		// !FIXME! DOTAMERGE
		//return GGCInterface()->BProcessSystemMessage( eMsg | k_EMsgProtoBufFlag, pubMsgBytes, cubSize );
		g_theMessageList.TallySendMessage( eMsg & ~k_EMsgProtoBufFlag, cubSize );
		VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
		{
			VPROF_BUDGET( "GCHost - SendMessageToSystem (ProtoBuf)", VPROF_BUDGETGROUP_STEAM );
			return GGCHost()->BSendMessageToClient( GGCBase()->GetAppID(), CSteamID(), eMsg | k_EMsgProtoBufFlag, pubMsgBytes, cubSize );
		}
	}
	uint32 GetCubSent() const { return m_cubSent; }
private:
	uint32 m_cubSent;
};


//-----------------------------------------------------------------------------
// Purpose: Sends a message to the given SteamID
//-----------------------------------------------------------------------------
bool CGCBase::BSendGCMsgToClient( const CSteamID & steamIDTarget, const CProtoBufMsgBase& msg )
{
	CProtoBufClientSendHandler sender( steamIDTarget );
	return msg.BAsyncSend( sender );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a system message to the GC Host
//-----------------------------------------------------------------------------
bool CGCBase::BSendSystemMessage( const CGCMsgBase& msg, uint32 *pcubSent )
{
	uint32 cubSent = msg.CubPkt() - sizeof(GCMsgHdr_t);
	if ( NULL != pcubSent )
	{
		*pcubSent = cubSent;
	}

	// !FIXME! DOTAMERGE
	//return GGCInterface()->BProcessSystemMessage( msg.Hdr().m_eMsg, msg.PubPkt() + sizeof(GCMsgHdr_t), cubSent );
	return BSendGCMsgToClient( CSteamID(), msg );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a system message to the GC Host
//-----------------------------------------------------------------------------
bool CGCBase::BSendSystemMessage( const CProtoBufMsgBase & msg, uint32 *pcubSent )
{
	CProtoBufSystemSendHandler sender;
	bool bRet = msg.BAsyncSend( sender );
	if ( NULL != pcubSent )
	{
		*pcubSent = sender.GetCubSent();
	}
	return bRet;
}

bool CGCBase::BSendSystemMessage( const ::google::protobuf::Message &msgOut, MsgType_t eSendMsg )
{
	CProtoBufSystemSendHandler sender;
	CMsgProtoBufHeader hdr;
	return CProtoBufMsgBase::BAsyncSendProto( sender, eSendMsg, hdr, msgOut );
}

//-----------------------------------------------------------------------------
// Purpose: send msgOut to the place that msgIn came from
//-----------------------------------------------------------------------------
bool CGCBase::BReplyToMessage( CGCMsgBase &msgOut, const CGCMsgBase &msgIn )
{
	// Don't reply if the source is not expecting it
	if ( !msgIn.BIsExpectingReply() )
		return true;

	msgOut.Hdr().m_JobIDTarget = msgIn.Hdr().m_JobIDSource;
	return BSendGCMsgToClient( msgIn.Hdr().m_ulSteamID, msgOut );
}


//-----------------------------------------------------------------------------
// Purpose: send msgOut to the place that msgIn came from
//-----------------------------------------------------------------------------
bool CGCBase::BReplyToMessage( CProtoBufMsgBase &msgOut, const CProtoBufMsgBase &msgIn )
{
	// Don't reply if the source is not expecting it
	if ( !msgIn.GetJobIDSource() )
		return true;

	msgOut.SetJobIDTarget( msgIn.GetJobIDSource() );
	return BSendGCMsgToClient( msgIn.GetClientSteamID(), msgOut );
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message to the given SteamID
//-----------------------------------------------------------------------------
bool CGCBase::BSendGCMsgToClientWithPreSerializedBody( const CSteamID & steamIDTarget, MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, const byte *pubBody, uint32 cubBody ) const
{
	CProtoBufClientSendHandler sender( steamIDTarget );
	return CProtoBufMsgBase::BAsyncSendWithPreSerializedBody( sender, eMsgType, hdr, pubBody, cubBody );
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message that has already been packed to the system handler
//-----------------------------------------------------------------------------
bool CGCBase::BSendGCMsgToSystemWithPreSerializedBody( MsgType_t eMsgType, const CMsgProtoBufHeader& hdr, const byte *pubBody, uint32 cubBody ) const
{
	CProtoBufSystemSendHandler sender;
	return CProtoBufMsgBase::BAsyncSendWithPreSerializedBody( sender, eMsgType, hdr, pubBody, cubBody );
}

//-----------------------------------------------------------------------------
// Purpose: send msgOut to the place that msgIn came from
//-----------------------------------------------------------------------------
bool CGCBase::BReplyToMessageWithPreSerializedBody( MsgType_t eMsgType, const CProtoBufMsgBase &msgIn, const byte *pubBody, uint32 cubBody ) const
{
	// Don't reply if the source is not expecting it
	if ( !msgIn.GetJobIDSource() )
		return true;

	if( temp_list_mismatched_replies.GetBool() && !msgIn.BIsExpectingReply() )
	{
		EG_MSG( g_EGMessages, "Message %s was sent to client %s which did not expect a reply\n", PchMsgNameFromEMsg( eMsgType ), msgIn.GetClientSteamID().Render() );
	}

	CMsgProtoBufHeader hdr;
	hdr.set_job_id_target( msgIn.GetJobIDSource() );

	//is this a system message or a client message we are responding to?
	bool bSystemReply = ( msgIn.GetClientSteamID() == k_steamIDNil );

	if( bSystemReply )
	{
		return BSendGCMsgToSystemWithPreSerializedBody( eMsgType, hdr, pubBody, cubBody );
	}
	else
	{
		return BSendGCMsgToClientWithPreSerializedBody( msgIn.GetClientSteamID(), eMsgType, hdr, pubBody, cubBody );
	}
}

//-----------------------------------------------------------------------------
// Purpose: send msgOut to the place that msgIn came from
//-----------------------------------------------------------------------------

bool CGCBase::BYldSendMessageAndGetReply( const CSteamID &steamIDTarget, CProtoBufMsgBase &msgOut, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg )
{
	CJob& curJob = GJobCur();
	msgOut.ExpectingReply( curJob.GetJobID() );

	if ( !BSendGCMsgToClient( steamIDTarget, msgOut ) )
		return false;

	if( !curJob.BYieldingWaitForMsg( pMsgIn, eMsg, steamIDTarget ) )
		return false;

	return true;
}

//bool CGCBase::BYldSendGCMessageAndGetReply( int32 nGCDirIndex, CProtoBufMsgBase &msgOut, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg )
//{
//	CJob& curJob = GJobCur();
//	msgOut.ExpectingReply( curJob.GetJobID() );
//
//	if ( !BSendGCMessage( nGCDirIndex, msgOut ) )
//		return false;
//
//	if( !curJob.BYieldingWaitForMsg( pMsgIn, eMsg, CSteamID() ) )
//		return false;
//
//	return true;
//}

bool CGCBase::BYldSendSystemMessageAndGetReply( CGCMsgBase &msgOut, CGCMsgBase *pMsgIn, MsgType_t eMsg )
{
	CJob& curJob = GJobCur();
	msgOut.ExpectingReply( curJob.GetJobID() );

	if ( !BSendSystemMessage( msgOut ) )
		return false;

	if( !curJob.BYieldingWaitForMsg( pMsgIn, eMsg, CSteamID() ) )
		return false;

	return true;
}

bool CGCBase::BYldSendSystemMessageAndGetReply( CProtoBufMsgBase &msgOut, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg )
{
	CJob& curJob = GJobCur();
	msgOut.ExpectingReply( curJob.GetJobID() );

	if ( !BSendSystemMessage( msgOut ) )
		return false;

	if( !curJob.BYieldingWaitForMsg( pMsgIn, eMsg, CSteamID() ) )
		return false;

	return true;
}

bool CGCBase::BYldSendSystemMessageAndGetReply( const ::google::protobuf::Message &msgSend, MsgType_t eSendMsg, ::google::protobuf::Message *pMsgResponse, MsgType_t eRespondMsg )
{
	CJob& curJob = GJobCur();

	CMsgProtoBufHeader hdr;
	hdr.set_job_id_source( curJob.GetJobID() );

	CProtoBufSystemSendHandler sender;
	CProtoBufMsgBase::BAsyncSendProto( sender, eSendMsg, hdr, msgSend );

	CProtoBufPtrMsg protoMsg( pMsgResponse );
	//return curJob.BYieldingWaitForMsg( &protoMsg, eRespondMsg, CSteamID() );
	return curJob.BYieldingWaitForMsg( &protoMsg, eRespondMsg ); // !FIXME! For some reason system replies are coming back with a universe and instance set (but account ID zero).
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new session for the steam ID
//-----------------------------------------------------------------------------
CGCUserSession *CGCBase::CreateUserSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache ) const
{
	return new CGCUserSession( steamID, pSOCache );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new session for the steam ID
//-----------------------------------------------------------------------------
CGCGSSession *CGCBase::CreateGSSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache, uint32 unServerAddr, uint16 usServerPort ) const
{
	return new CGCGSSession( steamID, pSOCache, unServerAddr, usServerPort );
}


//-----------------------------------------------------------------------------
// Purpose: Locks the session for this steam ID and returns it.  Returns NULL
//			if the lock could not be granted or if the session could not be
//			found.
//-----------------------------------------------------------------------------
CGCUserSession *CGCBase::YieldingGetLockedUserSession( const CSteamID & steamID, const char *pszFilename, int nLineNum )
{
	if( !steamID.BIndividualAccount() )
		return NULL;

	if( !BYieldingLockSteamID( steamID, pszFilename, nLineNum ) )
		return NULL;

	CGCUserSession *pSession = FindUserSession( steamID );
	if( !pSession )
	{
		//EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "Unable to find session %s to lock it. Attempting to fetch it from the AM\n", steamID.Render() );
		pSession = (CGCUserSession *)YieldingRequestSession( steamID );
		if( !pSession )
		{
			UnlockSteamID( steamID );
		}
	}

	return pSession;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if a user is in the start playing queue
//-----------------------------------------------------------------------------
bool CGCBase::BUserSessionPending( const CSteamID & steamID ) const
{
	int nStartPlayingMapIndex = m_mapStartPlayingQueueIndexBySteamID.Find( steamID );
	return ( nStartPlayingMapIndex != m_mapStartPlayingQueueIndexBySteamID.InvalidIndex() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the session for this steamID or NULL if that session could
//			not be found.
//-----------------------------------------------------------------------------
CGCUserSession *CGCBase::FindUserSession( const CSteamID & steamID ) const
{
	// we should only call this on individual ids
	if ( !steamID.IsValid() )
	{
		AssertMsg1( steamID.IsValid(), "CGCBase::FindUserSession was passed invalid Steam ID %s", steamID.Render() );
		return NULL;
	}
	if ( !steamID.BIndividualAccount() )
	{
		AssertMsg1( steamID.BIndividualAccount(), "CGCBase::FindUserSession was passed non-individual Steam ID %s", steamID.Render() );
		return NULL;
	}

	CGCUserSession **ppSession = m_hashUserSessions.PvRecordFind( steamID.ConvertToUint64() );
	if( ppSession )
	{
		(*ppSession)->MarkAccess();
		return *ppSession;
	}
	else
	{
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the session associated with the steam id is online, false otherwise
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingIsOnline( const CSteamID & steamID )
{
	CGCMsg< MsgGCValidateSession_t > msg( k_EGCMsgValidateSession );
	msg.Body().m_ulSteamID = steamID.ConvertToUint64();
	msg.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msg ) )
		return false;

	CGCMsg< MsgGCValidateSessionResponse_t > msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgValidateSessionResponse ) )
	{
		EmitWarning( SPEW_GC, LOG_ALWAYS, "Didn't get reply from AM for %s in YieldingRequestSession\n", steamID.Render() );
		return false;
	}

	return msgReply.Body().m_bOnline;
}


//-----------------------------------------------------------------------------
// Purpose: Looks up a session from the AM for the provided steam ID.
//-----------------------------------------------------------------------------
template <typename T >
class CScopedIncrement
{
public:
	inline CScopedIncrement( T & counter) : m_counter(counter) { ++m_counter; }
	inline ~CScopedIncrement() { --m_counter; }
private:
	T &m_counter;
};

CGCSession *CGCBase::YieldingRequestSession( const CSteamID & steamID )
{
	AssertRunningJob();
	if( !steamID.BIndividualAccount() && !steamID.BGameServerAccount() )
		return NULL;
	Assert( IsSteamIDUnlockedOrLockedByCurJob( steamID ) );

	// Check if we already have info in the logon queue for this SteamID
	int nStartPlayingMapIndex = m_mapStartPlayingQueueIndexBySteamID.Find( steamID );
	if ( nStartPlayingMapIndex != m_mapStartPlayingQueueIndexBySteamID.InvalidIndex() )
	{

		// Sanity
		int idxStartPlayingQueue = m_mapStartPlayingQueueIndexBySteamID[ nStartPlayingMapIndex ];
		Assert( m_llStartPlaying[ idxStartPlayingQueue ].m_steamID == steamID );

		// Pull the logon out of the queue and execute it NOW
		YieldingExecuteStartPlayingQueueEntryByIndex( idxStartPlayingQueue );

		// Now return the session that was created, if any
		return FindUserOrGSSession( steamID );
	}

	CGCMsg< MsgGCValidateSession_t > msg( k_EGCMsgValidateSession );
	msg.Body().m_ulSteamID = steamID.ConvertToUint64();
	msg.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msg ) )
		return NULL;

	CScopedIncrement<int> increment( m_nRequestSessionJobsActive );

	CGCMsg< MsgGCValidateSessionResponse_t > msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgValidateSessionResponse ) )
	{
		EmitWarning( SPEW_GC, LOG_ALWAYS, "Didn't get reply from AM for %s in YieldingRequestSession\n", steamID.Render() );
		return NULL;
	}

	if( steamID.BIndividualAccount() )
	{
		if( msgReply.Body().m_bOnline )
		{
			CUtlBuffer bufVarData;
			if( msgReply.CubVarData() )
			{
				bufVarData.Put( msgReply.PubVarData(), msgReply.CubVarData() );
			}

			// Check if they have an entry in the startplaying queue, then get rid of it!
			// They data we just received is the most up-to-date we have.  We should
			// prefer this data over anything in the queue for sure.
			BRemoveStartPlayingQueueEntry( steamID );

			YieldingStartPlaying( steamID, msgReply.Body().m_ulSteamIDGS, msgReply.Body().m_unServerAddr, msgReply.Body().m_usServerPort, msgReply.CubVarData() ? &bufVarData : NULL );
			return FindUserSession( steamID );
		}
		else
		{
			//EmitWarning( SPEW_GC, LOG_ALWAYS, "Reply from AM is logging off %s in YieldingRequestSession\n", steamID.Render() );
			YieldingStopPlaying( steamID );
			return NULL;
		}
	}
	else 
	{
		if( msgReply.Body().m_bOnline )
		{
			YieldingStartGameserver( steamID, msgReply.Body().m_unServerAddr, msgReply.Body().m_usServerPort, msgReply.PubVarData(), msgReply.CubVarData() );
			return FindGSSession( steamID );
		}
		else
		{
			//EmitWarning( SPEW_GC, LOG_ALWAYS, "Reply from AM is stopping %s in YieldingRequestSession\n", steamID.Render() );
			YieldingStopGameserver( steamID );
			return NULL;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Send outgoing HTTP request to some other server. Probably a WebAPI
//			request to steam itself, but it could be a request on a more
//			remote server.
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingSendHTTPRequest( const CHTTPRequest *pRequest, CHTTPResponse *pResponse )
{
	if ( !pRequest || !pResponse )
	{
		AssertMsg( false, "Bad parameters for BYieldingSendHTTPRequest" );
		return false;
	}

	CMsgHttpResponse msgResponse;
	if( !BYldSendSystemMessageAndGetReply( pRequest->GetProtoObj(), k_EGCMsgSendHTTPRequest, &msgResponse, k_EGCMsgSendHTTPRequestResponse ) )
	{
		ReportHTTPError( CFmtStr( "No response to HTTP system message for %s", pRequest->GetURL() ), CGCEmitGroup::kMsg_Error );
		return false;
	}

	if ( !msgResponse.has_status_code() )
	{
		ReportHTTPError( CFmtStr( "No status code on HTTP response for %s", pRequest->GetURL() ), CGCEmitGroup::kMsg_Error );
		return false;
	}

	//log the result of this request
	if( msgResponse.status_code() != k_EHTTPStatusCode200OK )
	{
		ReportHTTPError( CFmtStr( "Invalid status code %u for %s", msgResponse.status_code(), pRequest->GetURL() ), CGCEmitGroup::kMsg_Warning );
	}
	else
	{
		ReportHTTPError( CFmtStr( "Success status code for %s", pRequest->GetURL() ), CGCEmitGroup::kMsg_Verbose );
	}

	pResponse->DeserializeFromProtoBuf( msgResponse );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Send an outgoing HTTP request and parse the result into KeyValues.
//-----------------------------------------------------------------------------
EResult CGCBase::YieldingSendHTTPRequestKV( const CHTTPRequest *pRequest, KeyValues *pKVResponse )
{
	CHTTPResponse apiResponse;
	if ( !BYieldingSendHTTPRequest( pRequest, &apiResponse ) )
	{
		EmitError( SPEW_GC, __FUNCTION__ ": web call to %s timed out\n", pRequest->GetURL() );
		return k_EResultTimeout;
	}

	if ( k_EHTTPStatusCode200OK != apiResponse.GetStatusCode() )
	{
		EmitError( SPEW_GC, __FUNCTION__ ": web call to %s got failure code %d\n", pRequest->GetURL(), apiResponse.GetStatusCode() );
		return k_EResultRemoteCallFailed;
	}

	pKVResponse->UsesEscapeSequences( true );
	if ( !pKVResponse->LoadFromBuffer( "webResponse", *apiResponse.GetBodyBuffer() ) )
	{
		EmitError( SPEW_GC, "Web call to %s could not parse response\n", pRequest->GetURL() );
		return k_EResultRemoteCallFailed;
	}

	return k_EResultOK;
}


//-----------------------------------------------------------------------------
// Purpose: Locks the session for this steam ID and returns it.  Returns NULL
//			if the lock could not be granted or if the session could not be
//			found.
//-----------------------------------------------------------------------------
CGCGSSession *CGCBase::YieldingGetLockedGSSession( const CSteamID & steamID, const char *pszFilename, int nLineNum )
{
	if( !steamID.BGameServerAccount() )
		return NULL;

	if( !BYieldingLockSteamID( steamID, pszFilename, nLineNum ) )
		return NULL;

	CGCGSSession *pSession = FindGSSession( steamID );
	if( !pSession )
	{
		pSession = (CGCGSSession *)YieldingRequestSession( steamID );
		if( !pSession )
		{
			UnlockSteamID( steamID );
		}
	}

	return pSession;
}

void CGCBase::ReportHTTPError( const char* pszError, CGCEmitGroup::EMsgLevel eLevel )
{
	//see if we can find a match
	int nIndex = m_HTTPErrors.Find( pszError );
	if( nIndex != m_HTTPErrors.InvalidIndex() )
	{
		//just increment our count
		m_HTTPErrors[ nIndex ]->m_nCount++;
		m_HTTPErrors[ nIndex ]->m_eSeverity = MIN( eLevel, m_HTTPErrors[ nIndex ]->m_eSeverity );
	}
	else
	{
		//add one
		SHTTPError* pError = new SHTTPError;
		pError->m_sStr = pszError;
		pError->m_nCount = 1;
		pError->m_eSeverity = eLevel;
		m_HTTPErrors.Insert( pError->m_sStr, pError );
	}

	if( !m_DumpHTTPErrorsSchedule.BIsScheduled() )
	{
		m_DumpHTTPErrorsSchedule.ScheduleMS( this, &CGCBase::DumpHTTPErrors, 1000 );
	}
}

void CGCBase::DumpHTTPErrors()
{
	FOR_EACH_MAP_FAST( m_HTTPErrors, nCurrError )
	{
		SHTTPError* pError = m_HTTPErrors[ nCurrError ];
		EG_EMIT( g_EGHTTPRequest, m_HTTPErrors[ nCurrError ]->m_eSeverity, "%s - %d times\n", pError->m_sStr.String(), pError->m_nCount );
		delete pError;
	}
	m_HTTPErrors.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the session for this steamID or NULL if that session could
//			not be found.
//-----------------------------------------------------------------------------
CGCGSSession *CGCBase::YieldingFindOrCreateGSSession( const CSteamID & steamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData )
{
	Assert( IsSteamIDLockedByJob( steamID, &GJobCur() ) );

	// If it's not a game server ID, then we shouldn't make a session for it.
	if( !steamID.BGameServerAccount() )
		return NULL;

	MEM_ALLOC_CREDIT_( "YieldingFindOrCreateGSSession" );

	// if var data came with this StartPlaying message, parse it into a KV and stick it on the session
	KeyValues *pkvDetails = NULL;
	if( pubVarData && cubVarData )
	{
		CUtlBuffer bufDetails;
		bufDetails.Put( pubVarData, cubVarData );
		pkvDetails = new KeyValues( "SessionDetails" );
		if( !pkvDetails->ReadAsBinary( bufDetails ) )
		{
			EmitError( SPEW_GC, "Unable to parse session details for %s\n", steamID.Render() );
			pkvDetails->deleteThis();
			pkvDetails = NULL;
		}
	}

//	// Since we might have to lock the session in some cases, let's just always grab the lock here,
//	// to keep things simpler.
//	if ( !BYieldingLockSteamID( steamID, __FILE__, __LINE__ ) )
//		return NULL;

	CGCGSSession *pSession = FindGSSession( steamID );
	CGCSharedObjectCache *pSOCache = NULL;
	if( !pSession )
	{
		pSOCache = YieldingFindOrLoadSOCache( steamID );

		// Did anybody create a session while we held the lock?
		// We hold the lock, and you must hold the lock to create
		// the session, so this race condition should be impossible
		pSession = FindGSSession( steamID );
		Assert( pSession == NULL );
	}
	if( !pSession )
	{

		// Create session of app-specific type
		pSession = CreateGSSession( steamID, pSOCache, unServerAddr, usServerPort );
		Assert( pSession );
		if ( !pSession )
		{
			AssertMsg1( false, "Failed creating GC GS session for %llu", steamID.ConvertToUint64() );
			if ( pkvDetails )
			{
				pkvDetails->deleteThis();
			}
			//UnlockSteamID( steamID ); // I like to clean up after myself
			return NULL;
		}
		RemoveCacheFromLRU( pSOCache );

		CGCGSSession **ppSession = m_hashGSSessions.PvRecordInsert( steamID.ConvertToUint64() );
		*ppSession = pSession;

		// Do game-specific work
		YieldingSessionStartServer( pSession );
	}
	else
	{
		if ( unServerAddr != 0 && usServerPort != 0 && ( unServerAddr != pSession->GetAddr() || usServerPort != pSession->GetPort() ) )
		{
			UpdateGSSessionAddress( pSession, unServerAddr, usServerPort );
		}
	}

	if( pkvDetails )
	{
		uint32 ip = pkvDetails->GetInt( "ip", 0 );
		if ( ip != 0 )
			pSession->m_unIPPublic = ip;
		pSession->m_osType = static_cast<EOSType>( pkvDetails->GetInt( "osType", k_eOSUnknown ) );
		pSession->m_bIsTestSession = pkvDetails->GetInt( "isTestSession", 0 ) != 0;
		pkvDetails->deleteThis();
	}

	//UnlockSteamID( steamID ); // I like to clean up after myself
	return pSession;
}

//-----------------------------------------------------------------------------
// Purpose: Called when a Session is moved to a different address.
//-----------------------------------------------------------------------------
void CGCBase::UpdateGSSessionAddress( CGCGSSession *pSession, uint32 unServerAddr, uint16 usServerPort )
{
	pSession->SetIPAndPort( unServerAddr, usServerPort );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the session for this steamID or NULL if that session could
//			not be found.
//-----------------------------------------------------------------------------
CGCGSSession *CGCBase::FindGSSession( const CSteamID & steamID ) const
{
	// we should only call this on server ids
	if ( !steamID.IsValid() || steamID.GetAccountID() == 0 )
	{
		AssertMsg1( false, "CGCBase::FindGSSession was passed invalid Steam ID %s", steamID.Render() );
		return NULL;
	}
	if ( !steamID.BGameServerAccount() )
	{
		AssertMsg1( steamID.BGameServerAccount(), "CGCBase::FindGSSession was passed non-gameserver Steam ID %s", steamID.Render() );
		return NULL;
	}

	CGCGSSession **ppSession = m_hashGSSessions.PvRecordFind( steamID.ConvertToUint64() );
	if( ppSession )
	{
		(*ppSession)->MarkAccess();
		return *ppSession;
	}
	else
	{
		return NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Locate session from appropriate table, depending on if it's
//			an individual or gameserver ID
//-----------------------------------------------------------------------------
CGCSession *CGCBase::FindUserOrGSSession( const CSteamID & steamID ) const
{
	if ( steamID.BIndividualAccount() )
		return FindUserSession( steamID );
	if ( steamID.BGameServerAccount() )
		return FindGSSession( steamID );
	AssertMsg1( false, "CGCBase::FindUserOrGSSession, steam ID %s isn't an individual or a gameserver ID", steamID.Render() );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Wakes up the job waiting for this SQL result
//-----------------------------------------------------------------------------
void CGCBase::SQLResults( GID_t gidContextID )
{
	VPROF_BUDGET( "CGCBase::SQLResults", VPROF_BUDGETGROUP_STEAM );
	m_JobMgr.BResumeSQLJob( gidContextID );
}


//-----------------------------------------------------------------------------
// Purpose: Finds the cache in the map for a new session
//-----------------------------------------------------------------------------
CGCSharedObjectCache *CGCBase::FindSOCache( const CSteamID & steamID )
{
	CUtlMap< CSteamID, CGCSharedObjectCache *, int >::IndexType_t nCache = m_mapSOCache.Find( steamID );
	if( m_mapSOCache.IsValidIndex( nCache ) )
		return m_mapSOCache[nCache];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingLoadSOCache( CGCSharedObjectCache *pSOCache )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCBase::YieldingSOCacheLoaded( CGCSharedObjectCache *pSOCache )
{
	// remove it, so we don't stomp the copy in memcached
	m_rbtreeSOCachesWithDirtyVersions.Remove( pSOCache->GetOwner() );

	// stomp the version with the one we set in memcached previously if possible, otherwise, re-add it to the set
	if ( !BYieldingRetrieveCacheVersion( pSOCache ) )
	{
		m_rbtreeSOCachesWithDirtyVersions.InsertIfNotFound( pSOCache->GetOwner() );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Removes the cache for this steamID
//-----------------------------------------------------------------------------
void CGCBase::RemoveSOCache( const CSteamID & steamID )
{	
	CUtlMap< CSteamID, CGCSharedObjectCache *, int >::IndexType_t nCache = m_mapSOCache.Find( steamID );
	if( m_mapSOCache.IsValidIndex( nCache ) )
	{
		CGCSharedObjectCache *pSOCache = m_mapSOCache[nCache];
		pSOCache->RemoveAllSubscribers();

		if( pSOCache->BIsDatabaseDirty() )
		{
			EmitError( SPEW_GC, "Attempting to remove SO Cache %s while it was dirty. Adding to Writeback instead\n", steamID.Render() );
			pSOCache->DumpDirtyObjects();
			AddCacheToWritebackQueue( pSOCache );

			// adding the cache to the LRU list too, just so it will go away once writeback does its thing
			if( !m_listCachesToUnload.IsValidIndex( pSOCache->GetLRUHandle() ) )
			{
				AddCacheToLRU( pSOCache );
			}
		}
		else
		{
			RemoveCacheFromLRU(pSOCache);

			delete pSOCache;
			m_mapSOCache.RemoveAt( nCache );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enqueues a flush instruction to Econ service for Web Inventory to update
//-----------------------------------------------------------------------------
void CGCBase::FlushInventoryCache( AccountID_t unAccountID )
{
	VPROF_BUDGET( "FlushInventoryCache - enqueue", VPROF_BUDGETGROUP_STEAM );
	m_rbFlushInventoryCacheAccounts.InsertIfNotFound( unAccountID );
}

//-----------------------------------------------------------------------------
// Purpose: Finds the cache in the map for a new session and locks it
//-----------------------------------------------------------------------------
bool CGCBase::UnloadUnusedCaches( uint32 unMaxCacheCount, CLimitTimer *pLimitTimer )
{
	VPROF_BUDGET( "UnloadUnusedCaches", VPROF_BUDGETGROUP_STEAM );

	uint32 unCachesUnloaded = 0;
	for( uint32 unCache = m_listCachesToUnload.Head(), unNextCache = m_listCachesToUnload.InvalidIndex(); unCache != m_listCachesToUnload.InvalidIndex(); unCache = unNextCache )
	{
		unNextCache = m_listCachesToUnload.Next( unCache );

		// only remove caches until we are under our limit
		if( (uint32)m_mapSOCache.Count() <= unMaxCacheCount )
			return false;

		// only loop until we need to stop consuming heartbeat time. We'll finish in later frames
		if( pLimitTimer && pLimitTimer->BLimitReached() )
			return true;

		CSteamID ownerID = m_listCachesToUnload[ unCache ];
		CGCSharedObjectCache *pSOCache = FindSOCache( ownerID );
		Assert( pSOCache );
		if( !pSOCache )
		{
			EmitError( SPEW_GC, "Cache for %s could not be found even though it is in the LRU list\n", ownerID.Render() );
			m_listCachesToUnload.Remove( unCache );
			continue;
		}

		// make sure there's no session using this cache
		if( ( ownerID.BIndividualAccount() && FindUserSession( ownerID ) ) 
			|| ( ownerID.BGameServerAccount() && FindGSSession( ownerID ) ) )
		{
			EmitError( SPEW_GC, "Cache for %s has a session even though it is in the LRU list\n", ownerID.Render() );
			Assert( pSOCache->GetLRUHandle() == unCache );
			if ( pSOCache->GetLRUHandle() != unCache )
			{
				EmitError( SPEW_GC, "Cache for %s has a different LRU handle than the one retrieved from the iterator! 0x%08x vs 0x%08x\n", ownerID.Render(), pSOCache->GetLRUHandle(), unCache );
			}

			RemoveCacheFromLRU( pSOCache );
			continue;
		}

		// Locked steam IDs mean someone is using the cache. 
		// Being in the writeback queue means that you haven't actually been unused for very long.
		// Just move on to the next one in those cases.
		if( IsSteamIDLocked( ownerID ) || pSOCache->GetInWriteback() )
			continue;

		// either count down by one or still in LRU?
		int iPreRemoveCount = m_listCachesToUnload.Count();

		// remove and delete the cache (which will remove it from the LRU list too.)
		RemoveSOCache( ownerID );
		unCachesUnloaded++;

		if ( iPreRemoveCount != m_listCachesToUnload.Count() + 1 &&
			 iPreRemoveCount != m_listCachesToUnload.Count() )
		{
			EmitError( SPEW_GC, "CGCBase::UnloadUnusedCaches() sanity check failed! List size changed dramatically removing 0x%08x; delta %i\n", unCache, iPreRemoveCount - m_listCachesToUnload.Count() );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Does some sanity checks on the SO cache LRU
//-----------------------------------------------------------------------------
void CGCBase::VerifySOCacheLRU()
{
	CUtlRBTree<CSteamID, int> rbTreeUsersEncountered( 0, m_listCachesToUnload.Count(), DefLessFunc( CSteamID ) );

	for( uint32 unCache = m_listCachesToUnload.Head(), unNextCache = m_listCachesToUnload.InvalidIndex(); unCache != m_listCachesToUnload.InvalidIndex(); unCache = unNextCache )
	{
		unNextCache = m_listCachesToUnload.Next( unCache );
		CSteamID ownerID = m_listCachesToUnload[ unCache ];
		CGCSharedObjectCache *pSOCache = FindSOCache( ownerID );
		if ( !pSOCache )
		{
			EmitError( SPEW_GC, "CGCBase::UnloadUnusedCaches() sanity[0] check failed! Empty cache in list in slot 0x%08x\n", unCache );
			continue;
		}

		if ( pSOCache->GetLRUHandle() != unCache )
		{
			EmitError( SPEW_GC, "CGCBase::UnloadUnusedCaches() sanity[1] check failed! Cache entry mismatch [ 0x%08x vs 0x%08x ] (owner: %s)\n", pSOCache->GetLRUHandle(), unCache, pSOCache->GetOwner().Render() );
		}

		if ( !rbTreeUsersEncountered.IsValidIndex( rbTreeUsersEncountered.InsertIfNotFound( ownerID ) ) )
		{
			EmitError( SPEW_GC, "CGCBase::UnloadUnusedCaches() sanity[2] check failed! Duplicate entry in list for 0x%08x (owner: %s)\n", unCache, pSOCache->GetOwner().Render() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds the cache to the LRU list
//-----------------------------------------------------------------------------
void CGCBase::AddCacheToLRU( CGCSharedObjectCache * pSOCache )
{
	Assert( pSOCache->GetLRUHandle() == m_listCachesToUnload.InvalidIndex() );
#if WITH_SOCACHE_LRU_DEBUGGING
	if ( pSOCache->GetLRUHandle() != m_listCachesToUnload.InvalidIndex() )
	{
		EmitError( SPEW_GC, "CGCBase::AddCacheToLRU() sanity[4] check failed! Adding SO Cache with existing LRU handle: 0x%08x\n", pSOCache->GetLRUHandle() );
	}
#endif
	
	// remove it just in case. Crashes are bad.
	RemoveCacheFromLRU( pSOCache );

#if WITH_SOCACHE_LRU_DEBUGGING
	Assert( pSOCache->GetLRUHandle() == m_listCachesToUnload.InvalidIndex() );
	if ( pSOCache->GetLRUHandle() != m_listCachesToUnload.InvalidIndex() )
	{
		EmitError( SPEW_GC, "CGCBase::AddCacheToLRU() sanity[5] check failed! Adding SO Cache with existing LRU handle: 0x%08x\n", pSOCache->GetLRUHandle() );
	}
#endif

	pSOCache->SetLRUHandle( m_listCachesToUnload.AddToTail( pSOCache->GetOwner() ) );
}


//-----------------------------------------------------------------------------
// Purpose: Removes the cache from the LRU list
//-----------------------------------------------------------------------------
void CGCBase::RemoveCacheFromLRU( CGCSharedObjectCache * pSOCache )
{
#if WITH_SOCACHE_LRU_DEBUGGING
	if ( m_listCachesToUnload.IsValidIndex( pSOCache->GetLRUHandle() ) == ( pSOCache->GetLRUHandle() == m_listCachesToUnload.InvalidIndex() ) )
	{
		EmitError( SPEW_GC, "CGCBase::RemoveCacheFromLRU() sanity[6] check failed! SO Cache has an invalid index, but IsValidIndex() is returning true: 0x%08x\n", pSOCache->GetLRUHandle() );
	}
#endif
	if( m_listCachesToUnload.IsValidIndex( pSOCache->GetLRUHandle() ) )
	{
		if( m_listCachesToUnload[ pSOCache->GetLRUHandle() ] != pSOCache->GetOwner() )
		{
			EmitError( SPEW_GC, "CGCBase::RemoveCacheFromLRU() Attempting to remove SOCache LRU index %d for %s, which really holds %s\n",
					   pSOCache->GetLRUHandle(), pSOCache->GetOwner().Render(), m_listCachesToUnload[ pSOCache->GetLRUHandle() ].Render() );
		}
		else
		{
			m_listCachesToUnload.Remove( pSOCache->GetLRUHandle() );
		}
	}
	pSOCache->SetLRUHandle( m_listCachesToUnload.InvalidIndex() );
}


//-----------------------------------------------------------------------------
// Purpose: Finds the cache in the map for a new session and locks it
//-----------------------------------------------------------------------------
CGCSharedObjectCache *CGCBase::YieldingGetLockedSOCache( const CSteamID &steamID, const char *pszFilename, int nLineNum )
{
	if( !BYieldingLockSteamID( steamID, pszFilename, nLineNum ) )
		return NULL;

	return YieldingFindOrLoadSOCache( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: Finds the cache in the map for a new session
//-----------------------------------------------------------------------------
CGCSharedObjectCache *CGCBase::YieldingFindOrLoadSOCache( const CSteamID &steamID )
{
	AssertRunningJob();

	if( !steamID.IsValid() )
	{
		AssertMsg1( false, "Unable to load SO cache for invalid steam ID %s", steamID.Render() );
		EmitError( SPEW_GC, "Unable to load SO cache for invalid steam ID %s (instance: %d)\n", steamID.Render(), steamID.GetUnAccountInstance() );
		return NULL;
	}

	// check to see if the SO cache is being loaded--if so, then we yield until it is done
	// the reason we are not just locking the steam id is because the current job may have
	// a lock on something else, and jobs can only have one lock active at a time.
	CJobTime timeStartedWaiting;
	timeStartedWaiting.SetToJobTime();
	while ( m_rbtreeSOCachesBeingLoaded.Find( steamID ) != m_rbtreeSOCachesBeingLoaded.InvalidIndex() )
	{

		// !TEST! Looks like we might have a bug where we're spinning here waiting forever.
		// Add a timeout just in case.
		if ( timeStartedWaiting.CServerMicroSecsPassed() > 180 * k_nMillion )
		{
			AssertMsg1( false, "Timed out waiting for SO cache %s to finish loading", steamID.Render() );
			return false;
		}
		GJobCur().BYieldingWaitOneFrame();
	}

	CGCSharedObjectCache *pSOCache = FindSOCache( steamID );
	if( !pSOCache )
	{
		m_rbtreeSOCachesBeingLoaded.Insert( steamID );
		pSOCache = CreateSOCache( steamID );
		CJobTime timeStartedLoading;
		timeStartedLoading.SetToJobTime();
		if( BYieldingLoadSOCache( pSOCache ) )
		{
			if ( FindSOCache( steamID ) != NULL )
			{
				EmitError( SPEW_GC, "HOLY FUCKING SHIT WE ARE DUPLICATING SO CACHES [%s]\n", steamID.Render() );
			}
			m_mapSOCache.Insert( steamID, pSOCache );

			float flSecondsToLoad = (float)timeStartedLoading.CServerMicroSecsPassed() / (float)k_nMillion;
			if ( flSecondsToLoad > 10.0f )
			{
				EmitInfo( SPEW_GC, 4, 1, "Loading of SO cache for %s took %.1fs\n", steamID.Render(), flSecondsToLoad );
			}

			//mark this cache as loaded so that it's version can change again
			pSOCache->SetDetectVersionChanges( false );

			CJobTime timeStartedNotify;
			timeStartedNotify.SetToJobTime();
			YieldingSOCacheLoaded( pSOCache );
			float flSecondsToNotify = (float)timeStartedNotify.CServerMicroSecsPassed() / (float)k_nMillion;
			if ( flSecondsToNotify > 10.0f )
			{
				EmitInfo( SPEW_GC, 1, 1, "YieldingSOCacheLoaded for %s took %.1fs\n", steamID.Render(), flSecondsToNotify );
			}

			AddCacheToLRU( pSOCache ); // in case the cache isn't about to be attached to a session
			m_rbtreeSOCachesBeingLoaded.Remove( steamID );
		}
		else
		{
			AssertMsg1( false, "Unable to load SO cache for %llu", steamID.ConvertToUint64() );
			EmitError( SPEW_GC, "Unable to load SO cache for %llu\n", steamID.ConvertToUint64() );
			delete pSOCache;
			m_rbtreeSOCachesBeingLoaded.Remove( steamID );
			return NULL;
		}
	}
	else
	{
		// if the cache is in the LRU, move it to the end of the list
		if( m_listCachesToUnload.IsValidIndex( pSOCache->GetLRUHandle() ) )
		{
			RemoveCacheFromLRU( pSOCache );
			AddCacheToLRU( pSOCache );
		}
	}

	return pSOCache;
}


//-----------------------------------------------------------------------------
// Purpose: Reloads the SO cache
//-----------------------------------------------------------------------------
void CGCBase::YieldingReloadCache( CGCSharedObjectCache *pSOCache )
{
	Assert( IsSteamIDLockedByJob( pSOCache->GetOwner(), &GJobCur() ) );
	if( !IsSteamIDLockedByJob( pSOCache->GetOwner(), &GJobCur() ) )
		return;

	// Flush all pending writes
	CSQLAccess sqlAccess;
	sqlAccess.BBeginTransaction( "CGCBase::YieldingReloadCache - Flush writes" );
	pSOCache->YieldingStageAllWrites( sqlAccess );
	if ( !sqlAccess.BCommitTransaction( true ) )
	{
		EmitError( SPEW_SHAREDOBJ, "%s: Unable to flush pending writes for %s, reload failed",
		           __FUNCTION__, pSOCache->GetOwner().Render() );
		return;
	}

	// load the data into a new cache
	CGCSharedObjectCache *pNewCache = CreateSOCache( pSOCache->GetOwner() );
	if( !BYieldingLoadSOCache( pNewCache ) )
	{
		EmitError( SPEW_SHAREDOBJ, "Unable to reload cache for %s because of a SQL error", pSOCache->GetOwner().Render() );
		return;
	}

	// process every object in the new cache and move it to the old one if necessary
	FOR_EACH_MAP_FAST( CSharedObject::GetFactories(), nType )
	{
		int nTypeID = CSharedObject::GetFactories().Key( nType );

		// remove all the old items of this type
		CSharedObjectTypeCache *pOldTypeCache = pSOCache->FindTypeCache( nTypeID );
		if( pOldTypeCache )
		{
			for( uint32 nCurrObj = 0; nCurrObj < pOldTypeCache->GetCount(); )
			{
				//not all objects should be deleted (for example lobbies/parties), so for those objects
				//don't delete and instead just skip over them
				if( pOldTypeCache->GetObject( nCurrObj )->BShouldDeleteByCache() )
				{
					pSOCache->RemoveObject( *pOldTypeCache->GetObject( nCurrObj ) );
				}
				else
				{
					nCurrObj++;
				}
			}
		}

		// add all the new objects of this type
		CSharedObjectTypeCache *pNewTypeCache = pNewCache->FindTypeCache( nTypeID );
		if( pNewTypeCache )
		{
			for( uint unObject = 0; unObject < pNewTypeCache->GetCount(); unObject++ )
			{
				pSOCache->AddObject( pNewTypeCache->GetObject( unObject ) );
			}
		}
	}

	// remove all the objects in the new cache
	pNewCache->RemoveAllObjectsWithoutDeleting();
	delete pNewCache;

	// if there's a session for this cache, tell it about the reload
	if( pSOCache->GetOwner().BIndividualAccount() )
	{
		CGCUserSession *pUserSession = FindUserSession( pSOCache->GetOwner() );
		if( pUserSession )
			pUserSession->YieldingSOCacheReloaded();
	}
	else if( pSOCache->GetOwner().BGameServerAccount() )
	{
		CGCGSSession *pGSSession = FindGSSession( pSOCache->GetOwner() );
		if( pGSSession )
			pGSSession->YieldingSOCacheReloaded();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Factory method to create a CGCSharedObjectCache
// Input  : &steamID - steamID that will own the CGCSharedObjectCache
// Output : Returns a new CGCSharedObjectCache
//-----------------------------------------------------------------------------
CGCSharedObjectCache *CGCBase::CreateSOCache( const CSteamID &steamID )
{
	return new CGCSharedObjectCache( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: yields until the lock on the specified steamID is taken
// Input  : &steamID - steamID to lock
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingLockSteamID( const CSteamID &steamID, const char *pszFilename, int nLineNum )
{
	AssertRunningJob();
	Assert( steamID.GetEAccountType() != k_EAccountTypePending );

	// lookup
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	if ( !pLock )
	{
		// no lock yet, insert one
		pLock = m_hashSteamIDLocks.PvRecordInsert( steamID );
		pLock->SetName( steamID );
		pLock->SetLockSubType( steamID.GetAccountID() );
		if ( steamID.BIndividualAccount() )
		{
			pLock->SetLockType( k_nLockTypeIndividual );
		}
		else if ( steamID.BGameServerAccount() )
		{
			pLock->SetLockType( k_nLockTypeGameServer );
		}
		else
		{
			AssertMsg1( false, "Lock taken for unexpected steamID: %s", steamID.Render() );
		}
	}

	Assert( pLock );
	if ( !pLock )
	{
		EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "Unable to create lock for %s\n", steamID.Render() );
		return false;
	}

	return GJobCur()._BYieldingAcquireLock( pLock, pszFilename, nLineNum );
}


//-----------------------------------------------------------------------------
// Purpose: locks a pair of steam IDs, grabbing the highest account ID first
//			to satisfy the deadlock-avoidance code in the job system
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingLockSteamIDPair( const CSteamID &steamIDA, const CSteamID &steamIDB, const char *pszFilename, int nLineNum )
{
	if( steamIDA == steamIDB )
		return BYieldingLockSteamID( steamIDA, pszFilename, nLineNum );

	//
	// !FIXME! This is really not the correct sort criteron to use.  The correct
	//         criteria is to use the full lock priority.  For example,
	//         what if we pass a gameserver ID and a user ID.  The whole
	//         concept of locking two SteamID's is probably broken when we split up
	//         things on the GC, though, so this might not be worth fixing.
	//

	if( steamIDA.GetAccountID() < steamIDB.GetAccountID() )
	{
		if( !BYieldingLockSteamID( steamIDB, pszFilename, nLineNum ) )
			return false;
		if( !BYieldingLockSteamID( steamIDA, pszFilename, nLineNum ) )
		{
			UnlockSteamID( steamIDB );
			return false;
		}
	}
	else
	{
		if( !BYieldingLockSteamID( steamIDA, pszFilename, nLineNum ) )
			return false;
		if( !BYieldingLockSteamID( steamIDB, pszFilename, nLineNum ) )
		{
			UnlockSteamID( steamIDA );
			return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: locks the specified steamID
// Input  : &steamID - steamID to unlock
//-----------------------------------------------------------------------------
bool CGCBase::BLockSteamIDImmediate( const CSteamID &steamID )
{
	AssertRunningJob();
	Assert( steamID.GetEAccountType() != k_EAccountTypePending );

	// lookup
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	if ( pLock == NULL )
	{
		// no lock yet, insert one
		pLock = m_hashSteamIDLocks.PvRecordInsert( steamID );
		Assert( pLock != NULL );
		if ( pLock == NULL )
		{
			return false;
		}
		
		if ( steamID.BIndividualAccount() )
		{
			pLock->SetLockType( k_nLockTypeIndividual );
		}
		else if ( steamID.BGameServerAccount() )
		{
			pLock->SetLockType( k_nLockTypeGameServer );
		}
		else
		{
			AssertMsg1( false, "Lock taken for unexpected steamID: %s", steamID.Render() );
		}
		
		pLock->SetName( steamID );
		pLock->SetLockSubType( steamID.GetAccountID() );
	}

	return GJobCur().BAcquireLockImmediate( pLock );
}


//-----------------------------------------------------------------------------
// Purpose: unlocks the specified steamID
// Input  : &steamID - steamID to unlock
//-----------------------------------------------------------------------------
void CGCBase::UnlockSteamID( const CSteamID &steamID )
{
	AssertRunningJob();
	Assert( steamID.GetEAccountType() != k_EAccountTypePending );

	// lookup
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	Assert( pLock );
	if ( !pLock )
	{
		AssertMsg2( false, "UnlockSteamID( '%s' ) called by %s but unable to find lock in map", steamID.Render(), GJobCur().GetName() );
		return;
	}

	if ( pLock->GetJobLocking() != &GJobCur() )
	{
		AssertMsg2( false, "UnlockSteamID( '%s' ) called when job %s doesn't own the lock", steamID.Render(), GJobCur().GetName() );
		return;
	}

	GJobCur().ReleaseLock( pLock );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the specified steamID is locked
//-----------------------------------------------------------------------------
bool CGCBase::IsSteamIDLocked( const CSteamID &steamID )
{
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	if ( pLock )
		return pLock->BIsLocked();

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the specified steamID is locked by the specified job
//-----------------------------------------------------------------------------
bool CGCBase::IsSteamIDLockedByJob( const CSteamID &steamID, const CJob *pJob ) const
{
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	if ( pLock )
		return ( pLock->GetJobLocking() == pJob );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the specified steamID is locked by the current job
//-----------------------------------------------------------------------------
bool CGCBase::IsSteamIDLockedByCurJob( const CSteamID &steamID ) const
{
	AssertRunningJob();

	return IsSteamIDLockedByJob( steamID, &GJobCur() );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the specified steamID is unlocked, or locked by the current job
//-----------------------------------------------------------------------------
bool CGCBase::IsSteamIDUnlockedOrLockedByCurJob( const CSteamID &steamID )
{
	AssertRunningJob();

	// lookup
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	if ( !pLock )
	{
		// Unlocked
		return true;
	}

	// It is in the hash of locks and is locked return true only if it is locked by the current job
	if ( pLock->BIsLocked() )
	{
		return ( pLock->GetJobLocking() == &GJobCur() );
	}
	else
	{
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns a pointer to the lock for the steamID, or NULL if none
//-----------------------------------------------------------------------------
const CLock *CGCBase::FindSteamIDLock( const CSteamID &steamID )
{
	// lookup
	return m_hashSteamIDLocks.PvRecordFind( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: returns a pointer to the job holding the lock for this steamID, NULL if none
//-----------------------------------------------------------------------------
CJob *CGCBase::PJobHoldingLock( const CSteamID &steamID )
{
	AssertRunningJob();

	// lookup
	CLock *pLock = m_hashSteamIDLocks.PvRecordFind( steamID );
	if ( !pLock || !pLock->BIsLocked() )
	{
		// Unlocked
		return NULL;
	}

	// Return the job holding the lock
	return pLock->GetJobLocking();
}


//-----------------------------------------------------------------------------
// Purpose: returns a pointer to the job holding the lock for this steamID, NULL if none
//-----------------------------------------------------------------------------
bool CGCBase::YieldingWritebackDirtyCaches( uint32 unSecondToDelayWrite )
{
	CSQLAccess sqlAccess;
	CUtlVector< CGCSharedObjectCache * > vecCachesWritten;
	uint32 unWrittenCount = 0;
	sqlAccess.BBeginTransaction( "CGCBase::YieldingWritebackDirtyCaches()" );
	RTime32 unFirstTimeToWrite = time( NULL ) - unSecondToDelayWrite;
	FOR_EACH_VEC( m_vecCacheWritebacks, nCache )
	{
		CGCSharedObjectCache *pSOCache = m_vecCacheWritebacks[ nCache ];

		// if this cache entered the writeback list too frequently, skip it for now
		if( unSecondToDelayWrite > 0 && pSOCache->GetWritebackTime() > unFirstTimeToWrite )
		{
			continue;
		}

		// if we can't get the lock for ourselves, catch it on the next time around
		if( !BLockSteamIDImmediate( pSOCache->GetOwner() ) )
		{
			continue;
		}

		unWrittenCount += pSOCache->YieldingStageAllWrites( sqlAccess );
		vecCachesWritten.AddToTail( pSOCache );
		m_vecCacheWritebacks.Remove( nCache );
		nCache--;

		// don't hog all the CPU. Yield and wait for the next frame if
		// we've been running for too long. Go ahead and write these
		// caches so we don't hold their locks forever though.
		if( GJobCur().GetMicrosecondsRun() > (uint64)(writeback_queue_max_accumulate_time.GetInt() * k_nThousand) ||
			( writeback_queue_max_caches.GetInt() > 0 && vecCachesWritten.Count() > writeback_queue_max_caches.GetInt() ) )
		{
			// We've spent enough time accumulating work. Time to run some SQL
			// queries.
			break;
		}
	}

	// Commit the transaction
	if( !sqlAccess.BCommitTransaction( true ) )
	{
		// the transaction failed.  Put those caches back on the TODO list
		EmitError( SPEW_GC, "CGCBase::YieldingWritebackDirtyCaches() - Writeback failed\n" );

		m_vecCacheWritebacks.AddMultipleToTail( vecCachesWritten.Count(), vecCachesWritten.Base() );
		FOR_EACH_VEC( vecCachesWritten, nCache )
		{
			CGCSharedObjectCache *pSOCache = vecCachesWritten[nCache];
			UnlockSteamID( pSOCache->GetOwner() );
		}
		return false;
	}
	else
	{
		// the transaction was successful. Tell those caches to forget their dirtiness
		FOR_EACH_VEC( vecCachesWritten, nCache )
		{
			CGCSharedObjectCache *pSOCache = vecCachesWritten[nCache];
			pSOCache->SetInWriteback( false );
			UnlockSteamID( pSOCache->GetOwner() );
		}
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCBase::AddCacheToWritebackQueue( CGCSharedObjectCache *pSOCache )
{
	Assert( pSOCache );
	if ( ( g_pJobCur != NULL ) && PJobHoldingLock( pSOCache->GetOwner() ) != g_pJobCur && !GGCBase()->BIsSOCacheBeingLoaded( pSOCache->GetOwner() ) )
	{
		AssertMsg2( false, "CGCBase::AddCacheToWritebackQueue called by job %s for %s, but job does not own lock", g_pJobCur->GetName(), pSOCache->GetOwner().Render() );
	}
	if( !pSOCache->GetInWriteback() )
	{
		m_vecCacheWritebacks.AddToTail( pSOCache );
		pSOCache->SetInWriteback( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingRetrieveCacheVersion( CGCSharedObjectCache *pSOCache )
{
	if ( !socache_persist_version_via_memcached.GetBool() )
	{
		// We'll keep doing the updates, but fail to restore it if not requested.
		return false;
	}

	CFmtStr1024 key( "SOCacheVersionV2_%llu", pSOCache->GetOwner().ConvertToUint64() );

	GCMemcachedGetResult_t data;
	if ( !BYieldingMemcachedGet( key.Access(), data ) || !data.m_bKeyFound || sizeof( uint64 ) != data.m_bufValue.Count() )
	{
#ifdef _DEBUG
		EmitInfo( SPEW_CONSOLE, 3, 3, "SOCacheVersion - Failed to retrieve SO Cache version for: %s\n", pSOCache->GetOwner().Render() );
#endif
		return false;
	}

	//we have a memcached version, so make sure that our version matches what was stored in memcache
	uint64 unVersion = *( (uint64 *)data.m_bufValue.Base() );
	pSOCache->SetVersion( unVersion );
#ifdef _DEBUG
	EmitInfo( SPEW_CONSOLE, 3, 3, "SOCacheVersion::Load - Loaded version from memcached for %s (%llu)\n", pSOCache->GetOwner().Render(), pSOCache->GetVersion() );
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCBase::AddCacheToVersionChangedList( CGCSharedObjectCache *pSOCache )
{
	m_rbtreeSOCachesWithDirtyVersions.InsertIfNotFound( pSOCache->GetOwner() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCBase::UpdateSOCacheVersions()
{
	CUtlVector<CUtlString> vecSetKeys( 0, m_rbtreeSOCachesWithDirtyVersions.Count() );
	CUtlVector<GCMemcachedBuffer_t> vecSetValues( 0, m_rbtreeSOCachesWithDirtyVersions.Count() );
	CUtlBuffer bufData( 0, ( sizeof( uint64 ) * m_rbtreeSOCachesWithDirtyVersions.Count() ) + 1 );

	CUtlVector<CUtlString> vecDeleteKeys( 0, m_rbtreeSOCachesWithDirtyVersions.Count() );

	for ( int idx = 0; idx < m_rbtreeSOCachesWithDirtyVersions.MaxElement(); ++idx )
	{
		if ( !m_rbtreeSOCachesWithDirtyVersions.IsValidIndex( idx ) ) 
			continue;

		const CSteamID &steamID = m_rbtreeSOCachesWithDirtyVersions[idx];

		// if the SO Cache is being loaded, ignore
		if ( m_rbtreeSOCachesBeingLoaded.Find( steamID ) != m_rbtreeSOCachesBeingLoaded.InvalidIndex() )
			continue;

		CSharedObjectCache *pSOCache = FindSOCache( steamID );
		if ( pSOCache )
		{
			CUtlString &strKey = vecSetKeys[ vecSetKeys.AddToTail() ];
			strKey.Format( "SOCacheVersionV2_%llu", steamID.ConvertToUint64() );

			GCMemcachedBuffer_t &bufVal = vecSetValues[ vecSetValues.AddToTail() ];
			bufVal.m_pubData = (byte *)bufData.Base() + bufData.TellPut();
			bufVal.m_cubData = sizeof( uint64 );

			bufData.PutInt64( pSOCache->GetVersion() );

#ifdef _DEBUG
			EmitInfo( SPEW_CONSOLE, 3, 3, "SOCacheVersion - storing version in memcached for %s (%llu).\n", steamID.Render(), pSOCache->GetVersion() );
#endif
		}
		else
		{
			// SO Cache is gone, so to be safe, remove the cached version number from memcached
			CUtlString &strKey = vecDeleteKeys[ vecDeleteKeys.AddToTail() ];
			strKey.Format( "SOCacheVersionV2_%llu", steamID.ConvertToUint64() );

#ifdef _DEBUG
			EmitInfo( SPEW_CONSOLE, 3, 3, "SOCacheVersion - no SO Cache, removing version in memcached for %s.\n", steamID.Render() );
#endif
		}
	}

	if ( vecSetKeys.Count() > 0 )
	{
		BMemcachedSet( vecSetKeys, vecSetValues );
	}

	if ( vecDeleteKeys.Count() > 0 )
	{
		BMemcachedDelete( vecDeleteKeys );
	}

	m_rbtreeSOCachesWithDirtyVersions.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the publisher access key for Steam Web APIs. This is just
//	a stub and must be implimented by a child class if they want this 
//	funtionality.
//-----------------------------------------------------------------------------
const char *CGCBase::GetSteamAPIKey()
{
	AssertMsg( false, "GetWebAPIKey(): Implement me!" );
	EmitError( SPEW_CONSOLE, "GetWebAPIKey(): Implement me!\n" );

	return "InvalidKey";
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the protobuf object was stored successfully, false otherwise
//-----------------------------------------------------------------------------
bool CGCBase::BMemcachedSet( const char *pKey, const ::google::protobuf::Message &protoBufObj )
{
	// build key
	CUtlVector< CUtlString > vecKeys;
	int idx = vecKeys.AddToTail();
	vecKeys[idx].Set( pKey );

	// allocate buffer we will use to stuff into the memcached buffer
	CUtlVector< CGCBase::GCMemcachedBuffer_t > vecValues;
	uint32 unSize = protoBufObj.ByteSize();
	void *pvBuf = stackalloc( unSize );
	protoBufObj.SerializeWithCachedSizesToArray( (uint8*)pvBuf );

	// stuff the data into the memcached buffer
	CGCBase::GCMemcachedBuffer_t buffer;
	buffer.m_pubData = pvBuf;
	buffer.m_cubData = unSize;
	vecValues.AddToTail( buffer );

	return BMemcachedSet( vecKeys, vecValues );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the memcached value stored via pKey was removed succesfully, false otherwise
//-----------------------------------------------------------------------------
bool CGCBase::BMemcachedDelete( const char *pKey )
{
	CUtlVector< CUtlString > vecKeys;
	int idx = vecKeys.AddToTail();
	vecKeys[idx].Set( pKey );
	return BMemcachedDelete( vecKeys );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the protobuf object was retrieved from memcached successfully, false otherwise
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingMemcachedGet( const char *pKey, ::google::protobuf::Message &protoBufMsg )
{
	// build key
	CUtlVector< CUtlString > vecKeys;
	int idx = vecKeys.AddToTail();
	vecKeys[idx].Set( pKey );

	// get results
	CUtlVector< CGCBase::GCMemcachedGetResult_t > vecResults;
	if ( !BYieldingMemcachedGet( vecKeys, vecResults ) || vecResults.Count() != 1 || vecResults[0].m_bKeyFound == false )
	{
		return false;
	}
	if ( !protoBufMsg.ParseFromArray( vecResults[0].m_bufValue.Base(), vecResults[0].m_bufValue.Count() ) )
	{
		return false;
	}
	if ( !protoBufMsg.IsInitialized() )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the keys and values into memcached
//-----------------------------------------------------------------------------
bool CGCBase::BMemcachedSet( const CUtlVector<CUtlString> &vecKeys, const CUtlVector<GCMemcachedBuffer_t> &vecValues )
{
	Assert( vecKeys.Count() == vecValues.Count() );
	if ( vecKeys.Count() != vecValues.Count() )
		return false;

	CProtoBufMsg<CGCMsgMemCachedSet> msgRequest( k_EGCMsgMemCachedSet );
	for ( int i = 0; i < vecKeys.Count(); ++i )
	{
		CGCMsgMemCachedSet_KeyPair *keypair = msgRequest.Body().add_keys();
		keypair->set_name( vecKeys[i].String() );
		keypair->set_value( vecValues[i].m_pubData, vecValues[i].m_cubData );
	}

	if( !BSendSystemMessage( msgRequest ) )
		return false;

	// There is no reply to setting in memcached
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Overload for a single key/value
//-----------------------------------------------------------------------------
bool CGCBase::BMemcachedSet( const CUtlString &strKey, const CUtlBuffer &buf )
{
	CUtlVector<CUtlString> memcachedMemberKeys( 0, 1 );
	CUtlVector<CGCBase::GCMemcachedBuffer_t> memcachedMemberValues( 0, 1 );

	memcachedMemberKeys.AddToTail( strKey );

	CGCBase::GCMemcachedBuffer_t &memcachedBuffer = memcachedMemberValues[ memcachedMemberValues.AddToTail() ];
	memcachedBuffer.m_pubData = buf.Base();
	memcachedBuffer.m_cubData = buf.TellPut();

	return BMemcachedSet( memcachedMemberKeys, memcachedMemberValues );
}


//-----------------------------------------------------------------------------
// Purpose: Delete the keys in memcached
//-----------------------------------------------------------------------------
bool CGCBase::BMemcachedDelete( const CUtlVector<CUtlString> &vecKeys )
{
	CProtoBufMsg<CGCMsgMemCachedDelete> msgRequest( k_EGCMsgMemCachedDelete );
	for ( int i = 0; i < vecKeys.Count(); ++i )
	{
		msgRequest.Body().add_keys( vecKeys[i].String() );
	}

	if( !BSendSystemMessage( msgRequest ) )
		return false;

	// There is no reply to deleting in memcached
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Overload for a single key/value
//-----------------------------------------------------------------------------
bool CGCBase::BMemcachedDelete( const CUtlString &strKey )
{
	CUtlVector<CUtlString> vecKeys( 0, 1 );
	vecKeys.AddToTail( strKey );
	return BMemcachedDelete( vecKeys );
}


//-----------------------------------------------------------------------------
// Purpose: Get the key's values from memcached
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingMemcachedGet( const CUtlVector<CUtlString> &vecKeys, CUtlVector<GCMemcachedGetResult_t> &vecResults )
{
	CProtoBufMsg<CGCMsgMemCachedGet> msgRequest( k_EGCMsgMemCachedGet );
	for ( int i = 0; i < vecKeys.Count(); ++i )
	{
		msgRequest.Body().add_keys( vecKeys[i].String() );
	}
	msgRequest.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msgRequest ) )
		return false;

	CProtoBufMsg<CGCMsgMemCachedGetResponse> msgResponse;
	if( !GJobCur().BYieldingWaitForMsg( &msgResponse, k_EGCMsgMemCachedGetResponse ) )
	{
		EmitWarning( SPEW_GC, LOG_ALWAYS, "Didn't get reply from IS for BYieldingMemcachedGet\n" );
		return false;
	}

	Assert( msgRequest.Body().keys_size() == msgResponse.Body().values_size() );
	if ( msgRequest.Body().keys_size() != msgResponse.Body().values_size() )
	{
		EmitWarning( SPEW_GC, LOG_ALWAYS, "Mismatched reply from IS for BYieldingMemcachedGet, asked for %d keys, got %d back\n", (int)msgRequest.Body().keys_size(), (int)msgResponse.Body().values_size() );
		return false; // Doesn't match what we asked for!
	}

	vecResults.Purge();
	vecResults.EnsureCapacity( msgResponse.Body().values_size() );
	for ( int i = 0; i < msgResponse.Body().values_size(); ++i )
	{
		GCMemcachedGetResult_t &result = vecResults[ vecResults.AddToTail() ];
		result.m_bKeyFound = msgResponse.Body().values(i).found();
		if ( result.m_bKeyFound )
		{
			result.m_bufValue.Copy( &(*msgResponse.Body().values(i).value().begin()), msgResponse.Body().values(i).value().size() );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Overload for a single key/value
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingMemcachedGet( const CUtlString &strKeys, GCMemcachedGetResult_t &result )
{
	CUtlVector<CUtlString> memcachedMemberKeys( 0, 1 );
	CUtlVector<GCMemcachedGetResult_t> memcachedResults;

	memcachedMemberKeys.AddToTail( strKeys );
	bool bRet = BYieldingMemcachedGet( memcachedMemberKeys, memcachedResults );
	if ( !bRet )
		return false;

	Assert( 1 == memcachedResults.Count() );
	if ( 1 != memcachedResults.Count() )
		return false;

	result.m_bKeyFound = memcachedResults[0].m_bKeyFound;
	result.m_bufValue.Swap( memcachedResults[0].m_bufValue );
	return true;
}


//-----------------------------------------------------------------------------
bool CGCBase::BYieldingGetIPLocations( CUtlVector<uint32> &vecIPs, CUtlVector<CIPLocationInfo> &infos )
{
	CProtoBufMsg<CGCMsgGetIPLocation> msgRequest( k_EGCMsgGetIPLocation );
	FOR_EACH_VEC( vecIPs, i )
	{
		msgRequest.Body().add_ips( vecIPs[i] );
	}

	msgRequest.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msgRequest ) )
		return false;

	// We don't need to worry about a reply mismatch in this case.  The message
	// has sufficient data so that we can match up the reply properly.
	GJobCur().ClearFailedToReceivedMsgType( k_EGCMsgGetIPLocationResponse );

	CProtoBufMsg<CGCMsgGetIPLocationResponse> msgResponse;
	if( !GJobCur().BYieldingWaitForMsg( &msgResponse, k_EGCMsgGetIPLocationResponse ) )
	{
		EmitWarning( SPEW_GC, LOG_ALWAYS, "Didn't get reply from IS for BYieldingGetIPLocation\n" );
		return false;
	}

	for ( int i = 0; i < msgResponse.Body().infos_size(); i++ )
	{
		infos.AddToTail( msgResponse.Body().infos( i ) );
	}

	return true;
}

//-----------------------------------------------------------------------------
bool CGCBase::BYieldingUpdateGeoLocation( CUtlVector<CSteamID> const &requestedVecSteamIds )
{
	CUtlVector<uint32> vecIPs;
	CUtlVector<CSteamID> vecSteamIds;

	FOR_EACH_VEC( requestedVecSteamIds, i )
	{
		const CSteamID memberSteamID = requestedVecSteamIds[i];
		CGCSession *pSession = FindUserOrGSSession( memberSteamID );
		if( pSession )
		{
			if ( !pSession->GetIPPublic() )
			{
				EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYieldingUpdateGeoLocation Session %s IP == 0, unable to retrieve\n", memberSteamID.Render() ) ;
				continue;
			}
			if ( !pSession->HasGeoLocation() )
			{
				vecIPs.AddToTail( pSession->GetIPPublic() );
				vecSteamIds.AddToTail( memberSteamID );
			}
		}
	}

	if (!vecIPs.Count())
		return true;

#define iptod(x) ((x)>>24&0xff), ((x)>>16&0xff), ((x)>>8&0xff), ((x)&0xff)

	FOR_EACH_VEC( vecIPs, i )
	{
		EmitInfo( SPEW_GC, geolocation_spewlevel.GetInt(), geolocation_loglevel.GetInt(), "BYieldingUpdateGeoLocation GetIPLocation[%d] = (%s,%u.%u.%u.%u)\n", i, vecSteamIds[i].Render(), iptod( vecIPs[i] ) ) ;
	}

	CUtlVector<CIPLocationInfo> infos;
	if ( BYieldingGetIPLocations( vecIPs, infos ) )
	{
		// The current IS has a bug where the IP will be blank/zero in the replies. If infos.Count() == vecIPs.Count() assume the order is correct
		if ( vecSteamIds.Count() == vecIPs.Count() && vecIPs.Count() == infos.Count() )
		{
			FOR_EACH_VEC( vecSteamIds, i )
			{
				CGCSession *pSession = FindUserOrGSSession( vecSteamIds[i] );
				if ( pSession )
				{
					EmitInfo( SPEW_GC, geolocation_spewlevel.GetInt(), geolocation_loglevel.GetInt(), "BYieldingUpdateGeoLocation[MATCHED] SetIPLocation[%s(%u.%u.%u.%u)] = (%6.3f,%6.3f)\n", pSession->GetSteamID().Render(), iptod( vecIPs[i] ), infos[i].latitude(), infos[i].longitude() );
					pSession->SetGeoLocation( infos[i].latitude(), infos[i].longitude() );
				}
			}
		}
		else
		{
			FOR_EACH_VEC( vecSteamIds, i )
			{
				FOR_EACH_VEC( infos, j )
				{
					if ( infos[j].ip() == vecIPs[i] )
					{
						CGCSession *pSession = FindUserOrGSSession( vecSteamIds[i] );
						if ( pSession )
						{
							EmitInfo( SPEW_GC, 4, LOG_ALWAYS, "BYieldingUpdateGeoLocation[SEARCHED] SetIPLocation[%s(%u.%u.%u.%u)] = (%6.3f,%6.3f)\n", pSession->GetSteamID().Render(), iptod( vecIPs[i] ), infos[j].latitude(), infos[j].longitude() );
							pSession->SetGeoLocation( infos[j].latitude(), infos[j].longitude() );
						}
					}
				}
			}
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Populate the KeyValues with the stats
//-----------------------------------------------------------------------------
void CGCBase::SystemStats_Update( CGCMsgGetSystemStatsResponse &msgStats )
{
	msgStats.set_active_jobs( m_JobMgr.CountJobs() );
	msgStats.set_yielding_jobs( m_JobMgr.CountYieldingJobs() );
	msgStats.set_user_sessions( m_hashUserSessions.Count() );
	msgStats.set_game_server_sessions( m_hashGSSessions.Count() );
	msgStats.set_socaches( m_mapSOCache.Count() );
	msgStats.set_socaches_to_unload( m_listCachesToUnload.Count() );
	msgStats.set_socaches_loading( m_rbtreeSOCachesBeingLoaded.Count() );
	msgStats.set_writeback_queue( m_vecCacheWritebacks.Count() );
	msgStats.set_steamid_locks( m_hashSteamIDLocks.Count() );
	msgStats.set_logon_queue( m_llStartPlaying.Count() );
	msgStats.set_logon_jobs( m_nStartPlayingJobCount );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the singleton GC object
//-----------------------------------------------------------------------------
CGCBase *GGCBase()
{
	return g_pGCBase;
}


//-----------------------------------------------------------------------------
// Purpose: Spews information about the active locks on the GC
//-----------------------------------------------------------------------------
int LockSortFunc( CLock  * const  *lhs, CLock * const  *rhs )
{
	return (*rhs)->GetWaitingCount() - (*lhs)->GetWaitingCount();
}

void CGCBase::DumpSteamIDLocks( bool bFull, int nMax )
{
	CUtlVector<CLock *> vecLocks;
	for( CLock *pLock = m_hashSteamIDLocks.PvRecordFirst(); NULL != pLock; pLock = m_hashSteamIDLocks.PvRecordNext( pLock ) )
	{
		if( pLock->BIsLocked() )
		{
			vecLocks.AddToTail( pLock );
		}
	}

	vecLocks.Sort( LockSortFunc );

	if( nMax > vecLocks.Count() || bFull )
	{
		nMax = vecLocks.Count();
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%d locks total. %d locked, %d displayed\n", m_hashSteamIDLocks.Count(), vecLocks.Count(), nMax );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Lock                     Holding Job            First Waiting Job      Wait Count  Lock Time\n" );

	for( int nLock = 0; nLock < nMax; nLock++ )
	{
		CLock *pLock = vecLocks[nLock];
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%-24s %-22s %-22s %-11d %d\n", 
			pLock->GetName(), 
			pLock->GetJobLocking() ? pLock->GetJobLocking()->GetName() : "--",
			pLock->GetJobWaitingQueueHead() ? pLock->GetJobWaitingQueueHead()->GetName() : "--",
			pLock->GetWaitingCount(),
			(int) ( pLock->GetMicroSecondsSinceLock() / k_nMillion ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Dumps informations about currently running jobs
//-----------------------------------------------------------------------------
void CGCBase::DumpJobs( const char *pszJobName, int nMax, int nPrintLocksMax ) const
{
	m_JobMgr.DumpJobs( pszJobName, nMax, nPrintLocksMax );
}


//-----------------------------------------------------------------------------
// Purpose: Dumps information about a specific job
//-----------------------------------------------------------------------------
void CGCBase::DumpJob( JobID_t jobID, int nPrintLocksMax ) const
{
	m_JobMgr.DumpJob( jobID, nPrintLocksMax );
}


//-----------------------------------------------------------------------------
// Purpose: Returns counts of core objects
//-----------------------------------------------------------------------------
int CGCBase::GetSOCacheCount() const
{
	return m_mapSOCache.Count();
}

bool CGCBase::IsSOCached( const CSharedObject *pObj, uint32 nTypeID ) const
{
	// OPT: If there are many caches, this is very slow - it would be faster have a ref count on the shared object to track this.
	// However this is debug only code.
#if defined( DEBUG )
	FOR_EACH_MAP_FAST( m_mapSOCache, i )
	{
		CGCSharedObjectCache *pCache = m_mapSOCache[ i ];
		if ( pCache->IsObjectCached( pObj, nTypeID ) )
		{
			return true;
		}
		if ( pCache->IsObjectDirty( pObj ) )
		{
			Assert( false );
			return true;
		}
	}
#else
	AssertMsg( false, "Calling IsSOCached() in release mode. This is a debug only function" );
#endif
	return false;
}

int CGCBase::GetUserSessionCount() const
{
	return m_hashUserSessions.Count();
}

int CGCBase::GetGSSessionCount() const
{
	return m_hashGSSessions.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Mark that we are shutting down
//-----------------------------------------------------------------------------
void CGCBase::SetIsShuttingDown()
{
	m_bIsShuttingDown = true;
	GetJobMgr().SetIsShuttingDown();
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether we are profiling or not
//-----------------------------------------------------------------------------
void CGCBase::SetProfilingEnabled( bool bEnabled )
{
	if ( bEnabled )
	{
		m_bStartProfiling = true;
	}
	else
	{
		m_bStopProfiling = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether to spew about vprof imbalances
//-----------------------------------------------------------------------------
void CGCBase::SetDumpVprofImbalances( bool bEnabled )
{
	m_bDumpVprofImbalances = bEnabled;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether we are spewing vprof imbalances
//-----------------------------------------------------------------------------
bool CGCBase::GetVprofImbalances()
{
	return m_bDumpVprofImbalances;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a steam ID for a user-provided input. Works with accountID,
//			steam account name, or steam ID.
//-----------------------------------------------------------------------------
CSteamID CGCBase::YieldingGuessSteamIDFromInput( const char *pchInput )
{
	AssertRunningJob();

	if( !pchInput )
	{
		EmitError( SPEW_CONSOLE, "Invalid NULL string passed to YieldingGuessSteamIDFromInput\n" );
		return CSteamID();
	}

	EUniverse localUniverse = m_pHost->GetUniverse();

	// Is it a 64 bit Steam ID?
	if ( pchInput[0] >= '0' && pchInput[0] <= '9' )
	{
		CSteamID steamID( V_atoui64( pchInput ) );
		if ( steamID.IsValid() )
			return steamID;
	}

	// quoted 

	// See if it's a profile link. If it is, clip the SteamID from it.
	const char *pszProfilePrepend = "steamcommunity.com/profiles/";
	int iInputLen = Q_strlen(pchInput);
	int iProfilePrependLen = Q_strlen(pszProfilePrepend);
	const char *pszFound = NULL;
	if ( (pszFound = Q_stristr( pchInput, pszProfilePrepend )) != NULL )
	{
		if ( iInputLen > ((pszFound + iProfilePrependLen) - pchInput) )
		{
			CSteamID steamID;
			steamID.SetFromString( (pszFound + iProfilePrependLen), localUniverse );
			if ( steamID.IsValid() )
				return steamID;
		}
	}

	// See if it's an id link.
	const char *pszIDPrepend = "steamcommunity.com/id/";
	int iIDPrependLen = Q_strlen(pszIDPrepend);
	if ( (pszFound = Q_stristr( pchInput, pszIDPrepend )) != NULL )
	{
		if ( iInputLen > ((pszFound + iIDPrependLen) - pchInput) )
		{
			char szMaxURL[512];
			Q_strncpy( szMaxURL, (pszFound + iIDPrependLen), sizeof(szMaxURL) );

			// Trim off a trailing slash
			int iURLLen = Q_strlen(szMaxURL);
			if ( szMaxURL[iURLLen-1] == '/' || pchInput[iURLLen-1] == '\\' )
			{
				szMaxURL[iURLLen-1] = '\0';
			}

			CUtlVector< CSteamID > vecIDs;
			if ( BYieldingLookupAccount( k_EFindAccountTypeURL, szMaxURL, &vecIDs ) )
			{
				// Should only ever find a single account for a URL
				if ( vecIDs.Count() == 1 )
					return vecIDs[0];
			}
		}
	}

	CGCMsg< MsgGCEmpty_t > msg( k_EGCMsgLookupAccountFromInput );
	msg.AddStrData( pchInput );
	msg.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msg ) )
	{
		EmitError( SPEW_CONSOLE, "Unable to query GCHost in YieldingGuessSteamIDFromInput\n" );
		return CSteamID();
	}

	CGCMsg< MsgGCLookupAccountResponse > msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgGenericReply ) )
	{
		EmitError( SPEW_CONSOLE, "No response from GCHost in YieldingGuessSteamIDFromInput\n" );
		return CSteamID();
	}

	return CSteamID( msgReply.Body().m_ulSteamID );
}


//-----------------------------------------------------------------------------
// Purpose: Returns all matching Steam IDs for the specified query.
// Returns: true if a response was received from Steam. The list may still be
//			empty in that case.
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingLookupAccount( EAccountFindType eFindType, const char *pchInput, CUtlVector< CSteamID > *prSteamIDs )
{
	if ( eFindType == k_EFindAccountTypeURL )
	{
		CSteamAPIRequest apiRequest( k_EHTTPMethodGET, "ISteamUser", "ResolveVanityURL", 1 );
		apiRequest.SetGETParamString( "vanityurl", pchInput );

		KeyValuesAD kvAPIResponse( "response" );
		CUtlString sWebApiErrMsg;
		EResult eResult = YieldingSendWebAPIRequest( apiRequest, kvAPIResponse, sWebApiErrMsg, false );
		if ( k_EResultOK != eResult )
		{
			// Emit an error on the less-common errors
			if ( k_EResultNoMatch != eResult )
			{
				EmitError( SPEW_GC, "WebAPI error looking up vanity URL by GC.  %s\n", sWebApiErrMsg.String() );
			}

			return false;
		}

		prSteamIDs->AddToTail( CSteamID( kvAPIResponse->GetUint64( "steamid" ) ) );

		return true;
	}
	else
	{
		CProtoBufMsg< CMsgAMFindAccounts > msg( k_EGCMsgFindAccounts );
		msg.Body().set_search_type( eFindType );
		msg.Body().set_search_string( pchInput );
		msg.ExpectingReply( GJobCur().GetJobID() );

		if( !BSendSystemMessage( msg ) )
		{
			EmitError( SPEW_GC, "Unable to send GCMsgFindAccounts\n" );
			return false;
		}

		CProtoBufMsg< CMsgAMFindAccountsResponse > response;
		if( !GJobCur().BYieldingWaitForMsg( &response, k_EGCMsgGenericReply ) )
		{
			EmitError( SPEW_GC, "No response to GCMsgFindAccounts\n" );
			return false;
		}

		for( int i=0; i<response.Body().steam_id_size(); i++ )
		{
			prSteamIDs->AddToTail( CSteamID( response.Body().steam_id( i ) ) );
		}

		return true;
	}
}

GC_CON_COMMAND( gc_search_vanityurl, "Tests searching for an account by vanity URL" )
{
	CUtlVector< CSteamID > vecIDs;
	if ( GGCBase()->BYieldingLookupAccount( k_EFindAccountTypeURL, args[1], &vecIDs ) )
	{
		Msg( "Search success.\n" );
		FOR_EACH_VEC( vecIDs, i )
		{
			CSteamID result = vecIDs[i];
			Msg( "Result: %llu\n", result.ConvertToUint64() );
		}
	}
	else
	{
		Msg( "Search failure.\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dumps a summary of the GC's status
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingRecordSupportAction( const CSteamID & actorID, const CSteamID & targetID, const char *pchData, const char *pchNote )
{
	CGCMsg< MsgGCRecordSupportAction_t > msgRecordSupportAction( k_EGCMsgRecordSupportAction );
	msgRecordSupportAction.Body().m_unAccountID = targetID.GetAccountID();
	msgRecordSupportAction.Body().m_unActorID = actorID.GetAccountID();
	msgRecordSupportAction.AddStrData( pchData );
	msgRecordSupportAction.AddStrData( pchNote );
	msgRecordSupportAction.ExpectingReply( GJobCur().GetJobID() );
	GGCBase()->BSendSystemMessage( msgRecordSupportAction );

	CGCMsg< MsgGCEmpty_t > msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgGenericReply ) )
	{
		EmitError( SPEW_GC, "No reply received to support action message\n" );
		return false;
	}
	else
	{
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Posts a steam alert to the alert alias for this GC's app.
//-----------------------------------------------------------------------------
void CGCBase::PostAlert( EAlertType eAlertType, bool bIsCritical, const char *pchAlertText, const CUtlVector< CUtlString > *pvecExtendedInfo, bool bAlsoSpew )
{
	CProtoBufMsg< CMsgNotifyWatchdog > msg( k_EGCMsgPostAlert );
	msg.Body().set_alert_type( eAlertType );
	msg.Body().set_critical( bIsCritical );

	if( !pvecExtendedInfo )
	{
		msg.Body().set_text( pchAlertText );
	}
	else
	{
		// put all the messages in one giant string and set that as the text

		// figure out how big "giant" is
		uint32 unSize = Q_strlen( pchAlertText ) + 2; // header + \n + null
		FOR_EACH_VEC( *pvecExtendedInfo, nLine )
		{
			unSize += pvecExtendedInfo->Element( nLine ).Length();
		}

		// walk the strings again to assemble the buffer
		CUtlBuffer bufMessage( 0, unSize, CUtlBuffer::TEXT_BUFFER );
		bufMessage.PutString( pchAlertText );
		bufMessage.PutString( "\n" );
		FOR_EACH_VEC( *pvecExtendedInfo, nLine )
		{
			bufMessage.PutString( pvecExtendedInfo->Element( nLine ).Get() );
		}

		msg.Body().set_text( (const char *)bufMessage.Base() );
	}

	if( bAlsoSpew )
	{
		EmitError( SPEW_GC, "%s", msg.Body().text().c_str() );
	}

	BSendSystemMessage( msg );
}


//-----------------------------------------------------------------------------
// Purpose: Fills the vector with all package IDs this account has a license to
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingGetAccountLicenses( const CSteamID & steamID, CUtlVector< PackageLicense_t > & vecPackages )
{
	CProtoBufMsg< CMsgAMGetLicenses > msg( k_EGCMsgGetLicenses );
	msg.Body().set_steamid( steamID.ConvertToUint64() );
	msg.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msg ) )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Unable to send GetAccountLicenses system message\n" );
		return false;
	}

	CProtoBufMsg< CMsgAMGetLicensesResponse > msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgGenericReply ) )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Timeout waiting for GetAccountLicenses reply\n" );
		return false;
	}

	if( msgReply.Body().result() != k_EResultOK )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "GetAccountLicenses for %s failed with %d\n", steamID.Render(), msgReply.Body().result() );
		return false;
	}

	vecPackages.RemoveAll();
	vecPackages.EnsureCapacity( msgReply.Body().license_size() );

	for( int i=0; i < msgReply.Body().license_size(); i++ )
	{
		const CMsgPackageLicense &msgPackage = msgReply.Body().license( i );

		//skip packages that they directly don't own (they may be lent to them via library sharing, and we don't want to grant based on that).
		//we count account ID of zero as matching so we can deal with old Steam versions that didn't provide this field
		if( ( msgPackage.owner_id() != steamID.GetAccountID() ) && ( msgPackage.owner_id() != 0 ) )
			continue;

		PackageLicense_t package;
		package.m_unPackageID = msgPackage.package_id();
		package.m_rtimeCreated = msgPackage.time_created();
		vecPackages.AddToTail( package );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Fills the vector with all package IDs this account has a license to
//-----------------------------------------------------------------------------
bool CGCBase::BYieldingAddFreeLicense( const CSteamID & steamID, uint32 unPackageID, uint32 unIPPublic, const char *pchStoreCountryCode )
{
	CProtoBufMsg< CMsgAMAddFreeLicense > msg( k_EGCMsgAddFreeLicense );
	msg.Body().set_steamid( steamID.ConvertToUint64() );
	msg.Body().set_packageid( unPackageID );
	if( unIPPublic )
		msg.Body().set_ip_public( unIPPublic );
	if( pchStoreCountryCode )
		msg.Body().set_store_country_code( pchStoreCountryCode );
	msg.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msg ) )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Unable to send GetAccountLicenses system message\n" );
		return false;
	}

	CProtoBufMsg< CMsgAMAddFreeLicenseResponse > msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgAddFreeLicenseResponse ) )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Timeout waiting for GetAccountLicenses reply\n" );
		return false;
	}

	if( msgReply.Body().eresult() != k_EResultOK )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "BYieldingAddFreeLicense for %s failed with %d\n", steamID.Render(), msgReply.Body().eresult() );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Fills the vector with all package IDs this account has a license to
//-----------------------------------------------------------------------------
int CGCBase::YieldingGrantGuestPass( const CSteamID & steamID, uint32 unPackageID, uint32 unPassesToGrant, int32 nDaysToExpiration )
{
	CProtoBufMsg<CMsgAMGrantGuestPasses2> msg( k_EGCMsgGrantGuestPass );
	msg.Body().set_steam_id( steamID.ConvertToUint64() );
	msg.Body().set_package_id( unPackageID );
	msg.Body().set_passes_to_grant( unPassesToGrant );
	msg.Body().set_days_to_expiration( nDaysToExpiration );
	msg.ExpectingReply( GJobCur().GetJobID() );
	if( !BSendSystemMessage( msg ) )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Unable to send GrantGuestPass system message\n" );
		return 0;
	}

	CProtoBufMsg<CMsgAMGrantGuestPasses2Response> msgReply;
	if( !GJobCur().BYieldingWaitForMsg( &msgReply, k_EGCMsgGrantGuestPassResponse ) )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "Timeout waiting for GrantGuestPass reply\n" );
		return 0;
	}

	if( msgReply.Body().eresult() != k_EResultOK )
	{
		EmitWarning( SPEW_GC, SPEW_ALWAYS, "YieldingGrantGuestPass for %s failed with %d\n", steamID.Render(), msgReply.Body().eresult() );
		return 0;
	}

	return msgReply.Body().passes_granted();
}


//-----------------------------------------------------------------------------
// Purpose: Gets data for an account
//-----------------------------------------------------------------------------
const CAccountDetails *CGCBase::YieldingGetAccountDetails( const CSteamID & steamID, bool bForceReload )
{
	return m_AccountDetailsManager.YieldingGetAccountDetails( steamID, bForceReload );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the current persona name for an account
//-----------------------------------------------------------------------------
const char *CGCBase::YieldingGetPersonaName( const CSteamID & steamID, const char *szUnknownName )
{
	const char *szPersonaName = m_AccountDetailsManager.YieldingGetPersonaName( steamID );
	return szPersonaName ? szPersonaName : szUnknownName;
}

//-----------------------------------------------------------------------------
// Purpose: Clears a persona name from the cache
//-----------------------------------------------------------------------------
void CGCBase::ClearCachedPersonaName( const CSteamID & steamID )
{
	m_AccountDetailsManager.ClearCachedPersonaName( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: Tells us to load the persona name for a user, but not wait on it
//-----------------------------------------------------------------------------
void CGCBase::PreloadPersonaName( const CSteamID & steamID )
{
	m_AccountDetailsManager.PreloadPersonaName( steamID );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message to the web API servers letting them know what the
//			methods and interfaces are for this GC.
//-----------------------------------------------------------------------------
bool CGCBase::BSendWebApiRegistration()
{
	// if we aren't initialized enough to have a GCHost, just skip this 
	// registration request. We'll register later in our init process.
	if( !m_pHost )
		return false;

	if( CGCWebAPIInterfaceMapRegistrar::VecInstance().Count() > 0 )
	{
		CGCMsg< MsgGCWebAPIRegisterInterfaces_t > msgWebRegistration( k_EGCMsgWebAPIRegisterInterfaces );
		msgWebRegistration.Body().m_cInterfaces = CGCWebAPIInterfaceMapRegistrar::VecInstance().Count();
		CUtlBuffer bufRegistrations;
		FOR_EACH_VEC( CGCWebAPIInterfaceMapRegistrar::VecInstance(), nInterface )
		{
			KeyValues *pkvInterface = CGCWebAPIInterfaceMapRegistrar::VecInstance()[ nInterface ]();
			Assert( pkvInterface );
			if( !pkvInterface )
				return false;

			KVPacker packer;
			packer.WriteAsBinary( pkvInterface, bufRegistrations );
			pkvInterface->deleteThis();
		}
		msgWebRegistration.AddVariableLenData( bufRegistrations.Base(), bufRegistrations.TellPut() );
		if( !BSendSystemMessage( msgWebRegistration ) )
			return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Dumps a summary of the GC's status
//-----------------------------------------------------------------------------
void CGCBase::Dump() const
{
	char rtimeBuf[k_RTimeRenderBufferSize];

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "GC Status for %d: path=%s\n", m_unAppID, m_sPath.Get() );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tLogon Surge: %s\n", BIsInLogonSurge() ? "Yes" : "No" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tStartPlaying: waiting=%d, jobs running=%d of %d\n", m_llStartPlaying.Count(), m_nStartPlayingJobCount, cv_concurrent_start_playing_limit.GetInt() );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tJobs: active=%d, yielding=%d\n", m_JobMgr.CountJobs(), m_JobMgr.CountYieldingJobs() );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tSessions: user=%d, gameserver=%d\n", m_hashUserSessions.Count(), m_hashGSSessions.Count() );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tCaches: %d (%d waiting to unload, %d currently loading, %s %d /+ %d)\n", m_mapSOCache.Count(), m_listCachesToUnload.Count(), m_rbtreeSOCachesBeingLoaded.Count(),
		( ( ( m_jobidFlushInventoryCacheAccounts == k_GIDNil ) || !m_JobMgr.BJobExists( m_jobidFlushInventoryCacheAccounts ) ) ? "last flushed" : "currently flushing" ),
		m_numFlushInventoryCacheAccountsLastScheduled, m_rbFlushInventoryCacheAccounts.Count() );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tWriteback Queue: %d (oldest: %s)\n", m_vecCacheWritebacks.Count(), m_vecCacheWritebacks.Count() > 0 ? CRTime::Render( m_vecCacheWritebacks[0]->GetWritebackTime(), rtimeBuf ) : "none" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tYieldingRequestSession: %d active\n", m_nRequestSessionJobsActive );
	m_AccountDetailsManager.Dump();
}


//-----------------------------------------------------------------------------
// Purpose: Dumps a summary of the GC's status
//-----------------------------------------------------------------------------
const char *CGCBase::GetCDNURL() const
{
	if( m_sCDNURL.IsEmpty() )
	{
		switch( m_pHost->GetUniverse() )
		{
		case k_EUniverseDev:
		case k_EUniverseBeta:
			m_sCDNURL.Format( "http://cdn.beta.steampowered.com/apps/%d/", GetAppID() );
			break;
		case k_EUniversePublic:
		default:
			m_sCDNURL.Format( "http://media.steampowered.com/apps/%d/", GetAppID() );
			break;
		}
	}

	return m_sCDNURL.Get();
}


//-----------------------------------------------------------------------------
// Purpose: Prints an assert to the console
//-----------------------------------------------------------------------------


void CGCBase::AssertCallbackFunc( const char *pchFile, int nLine, const char *pchMessage )
{
	if ( !ThreadInMainThread() ) // !KLUDGE!
	{
		EmitWarning( SPEW_GC, 4, "Thread assert %s(%d): %s\n", pchFile, nLine, pchMessage );
		return;
	}

	// Our spew handler should have already spewed this once, no need to spew it again
	//EmitError( SPEW_CONSOLE, "%s (%d): %s\n", V_GetFileName( pchFile ), nLine, pchMessage );
	if ( !Plat_IsInDebugSession() )
	{

		char rchCleanedJobName[48] = "";
		if ( ThreadInMainThread() && g_pJobCur != NULL )
		{
			const char *pszJobName = g_pJobCur->GetName();
			int l = 0;
			while ( l < sizeof(rchCleanedJobName)-1 )
			{
				char c = pszJobName[l];
				if ( c == '\0' )
					break;
				if ( !V_isalnum( c ) )
				{
					c = '_';
				}
				rchCleanedJobName[l] = c;
				++l;
			}
			rchCleanedJobName[l] = 0;
		}

		// Throttle writing of minidumps on a file / line / job basis
		CFmtStr sFileAndLine( "assert_%s(%d)%s%s",
			V_GetFileName( pchFile ),
			nLine,
			rchCleanedJobName[0] ? "_" : "",
			rchCleanedJobName
		);

		static CUtlDict< CCopyableUtlVector< RTime32 > > s_dictAsserts;

		int iDict = s_dictAsserts.Find( sFileAndLine.Access() );
		if ( !s_dictAsserts.IsValidIndex( iDict ) )
		{
			iDict = s_dictAsserts.Insert( sFileAndLine.Access() );
		}

		CCopyableUtlVector< RTime32 > &vecTimes = s_dictAsserts[iDict];

		int nStale = 0;
		while ( nStale < vecTimes.Count() && ( CRTime::RTime32TimeCur() - vecTimes[nStale] ) > (uint32)cv_assert_minidump_window.GetInt() )
		{
			nStale++;
		}
		vecTimes.RemoveMultipleFromHead( nStale );

		bool bWriteDump = ( vecTimes.Count() < cv_assert_max_minidumps_in_window.GetInt() );
		if ( bWriteDump )
		{
			vecTimes.AddToTail( CRTime::RTime32TimeCur() );

			CUtlString sCurJob;
			if ( ThreadInMainThread() && g_pJobCur != NULL )
			{
				sCurJob.Format( "[From job %s]\n", g_pJobCur->GetName() );
			}

			// Write the dump
			CUtlString sDumpComment;
			sDumpComment.Format( "%s%s%s(%d): %s",
				GGCBase()->GetIsShuttingDown() ? "[During shutdown]\n" : "", // Asserts during shutdown are much more often spurious.  Let's make it clear if a shutdown happens during shutdown
				sCurJob.String(), // The name of the current job name is often an incredibly useful piece of info.  If the dumps are not valid, this can narrow the search space immensely
				pchFile,
				nLine,
				pchMessage
			);
			SetMinidumpComment( sDumpComment.String() );
			WriteMiniDump( sFileAndLine.Access() );
			SetMinidumpComment( "" ); // just for grins
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Claims all the memory for the GC
//-----------------------------------------------------------------------------
void CGCBase::Validate( CValidator &validator, const char *pchName )
{
	VPROF_BUDGET( "CGCBase::Validate", VPROF_BUDGETGROUP_STEAM );

	// these are INSIDE the function instead of outside so the interface 
	// doesn't change
#ifdef DBGFLAG_VALIDATE
	VALIDATE_SCOPE();

	// Validate the global message list
	g_theMessageList.Validate( validator, "g_theMessageList" );

	// Validate the network global memory pool
	g_MemPoolMsg.Validate( validator, "g_MemPoolMsg" );

	CNetPacketPool::ValidateGlobals( validator );

	CJobMgr::ValidateStatics( validator, "CJobMgr" );
	CJob::ValidateStatics( validator, "CJob" );
	ValidateTempTextBuffers( validator );

	ValidateObj( m_JobMgr );
	ValidateObj( m_sPath );

	ValidateObj( m_hashUserSessions );
	for( CGCUserSession **ppSession = m_hashUserSessions.PvRecordFirst(); ppSession != NULL; ppSession = m_hashUserSessions.PvRecordNext( ppSession ) )
	{

		ValidatePtr( *ppSession );
	}
	ValidateObj( m_hashGSSessions );
	for( CGCGSSession **ppSession = m_hashGSSessions.PvRecordFirst(); ppSession != NULL; ppSession = m_hashGSSessions.PvRecordNext( ppSession ) )
	{
		ValidatePtr( *ppSession );
	}

	// validate the SQL access layer
	CRecordBase::ValidateStatics( validator, "CRecordBase" );
	GSchemaFull().Validate( validator, "GSchemaFull" );
	CRecordInfo::ValidateStatics( validator, "CRecordInfo" );
	CSharedObject::ValidateStatics( validator );

	OnValidate( validator, pchName );
#endif // DBGFLAG_VALIDATE
}

EResult YieldingSendWebAPIRequest( CSteamAPIRequest &request, KeyValues *pKVResponse, CUtlString &errMsg, bool b200MeansSuccess )
{
	CHTTPResponse apiResponse;
	if ( !GGCBase()->BYieldingSendHTTPRequest( &request, &apiResponse ) )
	{
		errMsg.Format( "Did not get a response" );
		return k_EResultTimeout;
	}

	if ( k_EHTTPStatusCode200OK != apiResponse.GetStatusCode() )
	{
		errMsg.Format( "HTTP status code %d", apiResponse.GetStatusCode() );

		//		if ( k_EResultOK != pKVResponse->GetInt( "result", k_EResultFail ) )
		//		{
		//			EmitError( SPEW_GC, "Web call to %s failed with error %d: %s\n", 
		//					   request.GetURL(),
		//					   pKVResponse->GetInt( "error/errorcode", k_EResultFail ), 
		//					   pKVResponse->GetString( "error/errordesc" ) );
		//			return pKVResponse->GetInt( "error/errorcode", k_EResultFail );
		//		}

		return k_EResultFail;
	}

	
	if ( apiResponse.GetBodyBuffer() )
	{
		pKVResponse->UsesEscapeSequences( true );
		if ( !pKVResponse->LoadFromBuffer( "webResponse", *apiResponse.GetBodyBuffer() ) )
		{
			errMsg.Format( "Failed to parse keyvalues result" );
			return k_EResultFail;
		}
	}

	if ( b200MeansSuccess )
	{
		return k_EResultOK;
	}

	int result = pKVResponse->GetInt( "success", -1 );
	if ( result < 0 )
	{
		errMsg = "Reply missing result code";
		return k_EResultFail;
	}
	errMsg = pKVResponse->GetString( "message", "" );
	if ( result != k_EResultOK && errMsg.IsEmpty() )
	{
		errMsg = "(Unknown error)";
	}
	return (EResult)result;
}

GC_CON_COMMAND( ip_geolocation, "<a.b.c.d> Perform geolocation lookup" )
{
	if ( args.ArgC() < 2 )
	{
		EmitError( SPEW_GC, "Pass at least one IP to lookup\n" );
		return;
	}

	// Get List of IP's to query
	CUtlVector<uint32> vecIPs;
	for ( int i = 1 ; i < args.ArgC() ; ++i )
	{
		netadr_t adr;
		adr.SetFromString( args[i] );
		if ( adr.GetIPHostByteOrder() == 0 )
		{
			EmitInfo( SPEW_GC, 1, 1, "%s is not a valid IP\n", args[i] );
		}
		else
		{
			vecIPs.AddToTail( adr.GetIPHostByteOrder() );
		}
	}
	if ( vecIPs.Count() <= 0 )
		return;

	// Do the query
	CUtlVector<CIPLocationInfo> vecInfos;
	vecInfos.SetCount( vecIPs.Count() );
	GGCBase()->BYieldingGetIPLocations( vecIPs, vecInfos );
	for ( int i = 0 ; i < vecInfos.Count() ; ++i )
	{
		netadr_t adr( vecInfos[i].ip(), 0 );
		EmitInfo( SPEW_GC, 1, 1, "%s:  %.1f, %.1f\n", adr.ToString( true ), vecInfos[i].latitude(), vecInfos[i].longitude() );
	}
}

} // namespace GCSDK



