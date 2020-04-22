//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CGCSession class
//
//=============================================================================
#include "stdafx.h"
#include "gcsession.h"
#include "steamextra/rtime.h"
#include "gcsdk_gcmessages.pb.h"
#include "gcsdk/gcreportprinter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Probably this makes more sense true by default, but we're spewing a ton and Fletcher says it
// isn't a big deal for TF so here we go.
GCConVar gs_session_assert_valid_addr_and_port( "gs_session_assert_valid_addr_and_port", "0" );

namespace GCSDK
{

DECLARE_GC_EMIT_GROUP( g_EGSessions, sessions );
DECLARE_GC_EMIT_GROUP_DEFAULTS( g_EGRateLimit, ratelimit, 2, 3 );

GCConVar max_user_messages_per_second( "max_user_messages_per_second", "20", 0, "Maximum number of messages a user can send per second. 0 disables the rate limiting" );
static GCConVar user_message_rate_limit_warning_period( "user_message_rate_limit_warning_period", "30", 0, "Number of seconds between warning about rate limiting for users" );

static GCConVar msg_rate_limit_report_user_bucket_1( "msg_rate_limit_report_user_bucket_1", "10", 0, "These values control where various users are bucketed in rate limiting reports to help identify how frequently users are running into rate limiting"  );
static GCConVar msg_rate_limit_report_user_bucket_2( "msg_rate_limit_report_user_bucket_2", "100", 0, "These values control where various users are bucketed in rate limiting reports to help identify how frequently users are running into rate limiting"  );

static GCConVar msg_rate_limit_list_user( "msg_rate_limit_list_user", "0", 0, "When set to a user account ID, this will report all the messages that are rate limited for that user to the console" );

CMsgRateLimitTracker g_RateLimitTracker;


CMsgRateLimitTracker::CMsgRateLimitTracker() :
	m_StartTime( CRTime::RTime32TimeCur() )
{
}

void CMsgRateLimitTracker::TrackRateLimitedMsg( const CSteamID steamID, MsgType_t eMsgType )
{
	//update message stat
	{
		uint32 nMsgIndex = m_MsgStats.Find( eMsgType );
		if( !m_MsgStats.IsValidIndex( nMsgIndex ) )
		{
			nMsgIndex = m_MsgStats.Insert( eMsgType, 0 );
		}
		m_MsgStats[ nMsgIndex ]++;
	}

	//update user stats
	{
		uint32 nUserIndex = m_UserStats.Find( steamID );
		if( !m_UserStats.IsValidIndex( nUserIndex ) )
		{
			nUserIndex = m_UserStats.Insert( steamID, 0 );
		}
		m_UserStats[ nUserIndex ]++;
	}

	//determine the severity to output the warning at. Assume verbose unless we are tracking a specific account ID (note that no account has 0 so 0 still effectively turns it off)
	CGCEmitGroup::EMsgLevel eMsgLevel = CGCEmitGroup::kMsg_Verbose;
	if( ( uint32 )msg_rate_limit_list_user.GetInt() == steamID.GetAccountID() )
	{
		eMsgLevel = CGCEmitGroup::kMsg_Msg;
	}
	EG_EMIT( g_EGMessages, eMsgLevel, "Dropped message %s (%d) for user %s\n", PchMsgNameFromEMsg( eMsgType ), eMsgType, steamID.Render() );
}

void CMsgRateLimitTracker::ReportMsgStats() const
{
	CGCReportPrinter rp;
	rp.AddStringColumn( "Msg" );
	rp.AddIntColumn( "Count", CGCReportPrinter::eSummary_Total );

	FOR_EACH_MAP_FAST( m_MsgStats, nCurrMsg )
	{
		rp.StrValue( PchMsgNameFromEMsg( m_MsgStats.Key( nCurrMsg ) ) );
		rp.IntValue( m_MsgStats[ nCurrMsg ] );
		rp.CommitRow();
	}

	rp.SortReport( "Count" );
	rp.PrintReport( SPEW_CONSOLE );
}

void CMsgRateLimitTracker::ReportTopUsers( uint32 nMinMsgs, uint32 nListTop ) const
{
	//collect a list of all messages, and sort them into order of frequency
	CGCReportPrinter rp;
	rp.AddSteamIDColumn( "User" );
	rp.AddIntColumn( "Count", CGCReportPrinter::eSummary_Total );

	FOR_EACH_MAP_FAST( m_UserStats, nCurrMsg )
	{
		rp.SteamIDValue( m_UserStats.Key( nCurrMsg ) );
		rp.IntValue( m_UserStats[ nCurrMsg ] );
		rp.CommitRow();
	}

	rp.SortReport( "Count" );
	rp.PrintReport( SPEW_CONSOLE, nListTop );
}

void CMsgRateLimitTracker::ReportUserStats() const
{
	//run through the users and aggregate stats
	const uint32 nBucketLimit1 = ( uint32 )max( 0, min( msg_rate_limit_report_user_bucket_1.GetInt(), msg_rate_limit_report_user_bucket_2.GetInt() ) );
	const uint32 nBucketLimit2 = ( uint32 )max( 0, max( msg_rate_limit_report_user_bucket_1.GetInt(), msg_rate_limit_report_user_bucket_2.GetInt() ) );

	uint32 nTotalMsg = 0;
	uint32 nMaxUser = 0;
	uint32 nBucketCount1 = 0;
	uint32 nBucketCount2 = 0;
	FOR_EACH_MAP_FAST( m_UserStats, nCurrMsg )
	{
		//add user counts to the buckets
		const uint32 nMsgs = m_UserStats[ nCurrMsg ];
		if( nMsgs <= nBucketLimit1 )
			nBucketCount1++;
		else if( nMsgs <= nBucketLimit2 )
			nBucketCount2++;

		//add up our total number of offenses
		nTotalMsg +=  nMsgs;
		nMaxUser = max( nMaxUser, nMsgs );
	}

	EG_MSG( SPEW_CONSOLE, "Capture Duration: %ds\n", CRTime::RTime32TimeCur() - m_StartTime );
	EG_MSG( SPEW_CONSOLE, "Total Dropped Messages: %d\n", nTotalMsg );
	EG_MSG( SPEW_CONSOLE, "Message IDs: %d\n", m_MsgStats.Count() );
	EG_MSG( SPEW_CONSOLE, "Users: %d (peak: %d)\n", m_UserStats.Count(), nMaxUser );
	EG_MSG( SPEW_CONSOLE, "  Below %d msgs: %d\n", nBucketLimit1, nBucketCount1 );
	EG_MSG( SPEW_CONSOLE, "  Below %d msgs: %d\n", nBucketLimit2, nBucketCount2 );
}

void CMsgRateLimitTracker::ClearStats()
{
	m_StartTime = CRTime::RTime32TimeCur();
	m_UserStats.RemoveAll();
	m_MsgStats.RemoveAll();
}

//console command hooks
GC_CON_COMMAND( msg_rate_limit_dump, "Dumps stats about rate limiting of messages" )
{
	g_RateLimitTracker.ReportUserStats();
	g_RateLimitTracker.ReportMsgStats();
	g_RateLimitTracker.ReportTopUsers( 0, 20 );
}

GC_CON_COMMAND( msg_rate_limit_dump_users, "Dumps a list of users that have been rate limited. Optional parameters can specify the number to dump or the minimum number of messages required." )
{
	if( args.ArgC() < 3 )
	{
		EG_MSG( SPEW_CONSOLE, "Proper usage is: %s <min messages> <top users> - Specify 0 for one or both to have it be ignored\n", args[ 0 ] );
		return;
	}
	g_RateLimitTracker.ReportTopUsers( ( uint32 )max( 0, atoi( args[ 1 ] ) ), ( uint32 )max( 0, atoi( args[ 2 ] ) ) );
}

GC_CON_COMMAND( msg_rate_limit_dump_msgs, "Dumps a list of messages that have been rate limited." )
{
	g_RateLimitTracker.ReportMsgStats();
}

GC_CON_COMMAND( msg_rate_limit_clear, "Clears all the accumulated msg rate limit stats" )
{
	g_RateLimitTracker.ClearStats();
}

//------------------------------------------------------------------------------------------
// CSteamIDRateLimit
//------------------------------------------------------------------------------------------

CSteamIDRateLimit::CSteamIDRateLimit( const GCConVar& cvNumPerPeriod, const GCConVar* pcvPeriodS ) :
	m_cvNumPerPeriod( cvNumPerPeriod ),
	m_pcvPeriodS( pcvPeriodS ),
	m_LastClear( CRTime::RTime32TimeCur() ),
	m_FrameFunction( "SteamIDRateLimit", CBaseFrameFunction::k_EFrameType_RunOnce ) 
{
	m_FrameFunction.Register( this, &CSteamIDRateLimit::OnFrameFn );
}

CSteamIDRateLimit::~CSteamIDRateLimit()
{
}

bool CSteamIDRateLimit::BIsRateLimited( CSteamID steamID, uint32 unMsgType )
{
	int nIndex = m_Msgs.FindOrInsert( steamID, 0 );
	if( ++m_Msgs[ nIndex ] >= ( uint32 )m_cvNumPerPeriod.GetInt() )
	{
		g_RateLimitTracker.TrackRateLimitedMsg( steamID, unMsgType );
		return true;
	}
	return false;
}

bool CSteamIDRateLimit::OnFrameFn( const CLimitTimer& timer )
{
	//if no period is specified, assume one second
	int nIntervalS = ( m_pcvPeriodS ) ? MAX( 1, m_pcvPeriodS->GetInt() ) : 1;
	if( CRTime::RTime32TimeCur() >= m_LastClear + nIntervalS )
	{
		m_Msgs.RemoveAll();
		m_LastClear = CRTime::RTime32TimeCur();
	}
	return false;
}



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCSession::CGCSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache )
: m_steamID( steamID ), 
  m_pSOCache( pSOCache ),
  m_bIsShuttingDown( false ),
  m_osType( k_eOSUnknown ),
  m_bIsTestSession( false ),
  m_bIsSecure( false ),
  m_unIPPublic( 0 ),
  m_flLatitude( 0.0f ),
  m_flLongitude( 0.0f ) ,
  m_haveGeoLocation( false ),
  m_bInitialized( false ),
  m_rtLastMessageReceived( 0 )
{
	m_jtLastMessageReceived.SetLTime( 0 );
	m_jtTimeSentPing.SetLTime( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCSession::~CGCSession()
{
}


//-----------------------------------------------------------------------------
// Purpose: Checks the message against rate limiting. Returns true if we should
//	drop the message. False otherwise. This default behavior is a very basic
//	n messages / user / second rate limiting that's only meant to stop the 
//	worse abuses
//-----------------------------------------------------------------------------
bool CGCSession::BRateLimitMessage( MsgType_t unMsgType )
{
	unMsgType &= ~k_EMsgProtoBufFlag;
	if ( max_user_messages_per_second.GetInt() <= 0 )
		return false;

	RTime32 rtCur = CRTime::RTime32TimeCur();
	m_jtLastMessageReceived.SetToJobTime();
	if ( m_rtLastMessageReceived != rtCur )
	{
		m_rtLastMessageReceived = rtCur;
		m_unMessagesRecievedThisSecond = 0;
	}

	m_unMessagesRecievedThisSecond++;
	if ( m_unMessagesRecievedThisSecond > (uint32)max_user_messages_per_second.GetInt() )
	{
		//log this message
		g_RateLimitTracker.TrackRateLimitedMsg( GetSteamID(), unMsgType );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: The run function is called on each session (user and gameserver) 
//			approximately every k_nUserSessionRunInterval microseconds (or 
//			k_nGSSessionRunInterval for GS Sessions)
//-----------------------------------------------------------------------------
void CGCSession::Run()
{
	// These cached subscription messages are very expensive and only needed for a short period of time
	// If we're hitting the run loop, it's been around long enough
	GetSOCache()->ClearCachedSubscriptionMessage();
}


//-----------------------------------------------------------------------------
bool CGCSession::GetGeoLocation( float &latitude, float &longittude ) const
{
	latitude = m_flLatitude;
	longittude = m_flLongitude;

	return m_haveGeoLocation;
}

//-----------------------------------------------------------------------------
void CGCSession::SetGeoLocation( float latitude, float longittude )
{
	m_flLatitude = latitude;
	m_flLongitude = longittude;
	m_haveGeoLocation = true;
}

//-----------------------------------------------------------------------------
// Purpose: Claims all the memory for the session object
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CGCSession::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCUserSession::~CGCUserSession()
{
	if ( m_steamIDGS.BGameServerAccount() )
	{
		EmitError( SPEW_GC, "Destroying user %s while still connected to server %s\n", GetSteamID().Render(), GetSteamIDGS().Render() );
	}
}

bool CGCUserSession::BInit()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the session's game server to the given SteamID. This will
//	cause the session to leave the current server it's on, it any
// Returns: True if the user's session was added to the GS's session.
//			False if the session could not be found or if the user was already
//			on the server.
//-----------------------------------------------------------------------------
bool CGCUserSession::BSetServer( const CSteamID &steamIDGS )
{
	if ( steamIDGS == m_steamIDGS )
		return false;

	BLeaveServer();

	if( steamIDGS.IsValid() )
	{
		CGCGSSession *pGSSession = GGCBase()->FindGSSession( steamIDGS );
		if ( !pGSSession )
		{
			EmitError( SPEW_GC, "User %s attempting to join server %s which has no session\n", GetSteamID().Render(), steamIDGS.Render() );
			return false;
		}

		if ( !pGSSession->BAddUser( GetSteamID() ) )
		{
			EmitWarning( SPEW_GC, SPEW_ALWAYS, "Server %s already had user %s in its user list\n", steamIDGS.Render(), GetSteamID().Render() );
			// Fall through
		}
	}

	m_steamIDGS = steamIDGS;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Removes the session from the given game server
// Returns: True if the user's session was removed from the GS's session.
//			False if the session could not be found or if the user was not found
//			on the server.
//-----------------------------------------------------------------------------
bool CGCUserSession::BLeaveServer()
{
	if( m_steamIDGS.IsValid() )
	{
		// Remember the last server we were connected to
		m_steamIDGSPrev = m_steamIDGS;

		CGCGSSession *pGSSession = GGCBase()->FindGSSession( m_steamIDGS );
		if ( pGSSession )
		{
			pGSSession->BRemoveUser( GetSteamID() );
		}
	}

	m_steamIDGS = CSteamID();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Dumps useful information about this session
//-----------------------------------------------------------------------------
void CGCUserSession::Dump( bool bFull ) const
{
// this is ifdef'd out in Steam because GCSDK can't depend on steamid.cpp
#ifndef STEAM
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "User Session %s (%s)\n", GetSteamID().Render(), BIsShuttingDown() ? "SHUTTING DOWN" : "Active" );
	CJob *pJob = GGCBase()->PJobHoldingLock( GetSteamID() );
	if( pJob )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t LOCKED BY: %s\n", pJob->GetName() );
	}
	if( bFull && GetSOCache() )
		GetSOCache()->Dump();
	if( GetSteamIDGS().BGameServerAccount() )
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tGameserver: %s\n", GetSteamIDGS().Render() );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tOS: %d     Secure: %d\n", GetOSType(), IsSecure() ? 1 : 0 );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCGSSession::CGCGSSession( const CSteamID & steamID, CGCSharedObjectCache *pCache, uint32 unServerAddr, uint16 usServerPort ) 
: CGCSession( steamID, pCache ), m_unServerAddr( unServerAddr ), m_usServerPort( usServerPort )	
{
	if ( gs_session_assert_valid_addr_and_port.GetBool() )
	{
		Assert( unServerAddr );
		Assert( usServerPort );
	}

	// Default our public IP to be the same as our IP address
	m_unIPPublic = unServerAddr;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCGSSession::~CGCGSSession()
{
	if ( m_vecUsers.Count() > 0 )
	{
		EmitError( SPEW_GC, "Destroying game server %s while %d users are still connected\n", GetSteamID().Render(), m_vecUsers.Count() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a user to the list of users active on the game server
// Returns: True if the user was added, false if the user was already on this
//	server.
//-----------------------------------------------------------------------------
bool CGCGSSession::BAddUser( const CSteamID &steamIDUser )
{
	if( m_vecUsers.HasElement( steamIDUser ) )
		return false;

	PreAddUser( steamIDUser );
	m_vecUsers.AddToTail( steamIDUser );
	PostAddUser( steamIDUser );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called if our IP / port changes after session is started
//-----------------------------------------------------------------------------
void CGCGSSession::SetIPAndPort( uint32 unServerAddr, uint16 usServerPort )
{
	// If we didn't have an override for the public IP, then also
	// update the public IP.
	//
	// !KLUDGE! This is gross for two reasons:
	// - First, do we really need two different fields?
	// - Second, why can the IP change after the session is created?
	// Shouldn't we force the session to be destroyed and recreated?
	// It cannot *really* be the same "session", can it?
	if ( m_unIPPublic == m_unServerAddr )
		m_unIPPublic = unServerAddr;
	m_unServerAddr = unServerAddr;
	m_usServerPort = usServerPort;
}

//-----------------------------------------------------------------------------
// Purpose: Removes a user from the list of users active on the game server
// Returns: True if the user was added, false if the user was not already on 
//	this server.
//-----------------------------------------------------------------------------
bool CGCGSSession::BRemoveUser( const CSteamID &steamIDUser )
{
	int nIndex = m_vecUsers.Find( steamIDUser );
	if ( !m_vecUsers.IsValidIndex( nIndex ) )
		return false;

	PreRemoveUser( steamIDUser );
	m_vecUsers.Remove( nIndex );
	PostRemoveUser( steamIDUser );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Removes all users from the list of game server users.
//-----------------------------------------------------------------------------
void CGCGSSession::RemoveAllUsers()
{
	if ( 0 == m_vecUsers.Count() )
		return;

	PreRemoveAllUsers();

	// Iterate all the users and tell them to leave this server.
	// Using back because the users will remove themselves from
	// this list during this function
	FOR_EACH_VEC_BACK( m_vecUsers, i )
	{
		CGCUserSession *pUserSession = GGCBase()->FindUserSession( m_vecUsers[i] );
		if ( pUserSession )
		{
			pUserSession->BLeaveServer();
		}
	}

	// Catch anyone we don't have a session for anymore
	m_vecUsers.RemoveAll();

	PostRemoveAllUsers();
}

#define iptod(x) ((x)>>24&0xff), ((x)>>16&0xff), ((x)>>8&0xff), ((x)&0xff)

//-----------------------------------------------------------------------------
// Purpose: Dumps useful information about this session
//-----------------------------------------------------------------------------
void CGCGSSession::Dump( bool bFull ) const
{
// this is ifdef'd out in Steam because GCSDK can't depend on steamid.cpp
#ifndef STEAM
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "GS Session %s\n", GetSteamID().Render() );
	CJob *pJob = GGCBase()->PJobHoldingLock( GetSteamID() );
	if( pJob )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t LOCKED BY: %s\n", pJob->GetName() );
	}
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t%d users:\n", m_vecUsers.Count() );
	FOR_EACH_VEC( m_vecUsers, nUser )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t\t%s\n", m_vecUsers[nUser].Render() );
	}
	if( GetSOCache() )
	{
		if ( bFull )
		{
			GetSOCache()->Dump();
		}
		else
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t SO Cache Version: %llu\n", GetSOCache()->GetVersion() );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Claims all the memory for the session object
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CGCGSSession::Validate( CValidator &validator, const char *pchName )
{
	CGCSession::Validate( validator, pchName);

	VALIDATE_SCOPE();

	ValidateObj( m_vecUsers );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: Client says it needs the SO Cache
// Input  : pNetPacket - received message
//-----------------------------------------------------------------------------
class CGCCacheSubscriptionRefresh: public CGCJob
{
public:
	CGCCacheSubscriptionRefresh( CGCBase *pGC ) : CGCJob( pGC ) { }
	bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket );
};

bool CGCCacheSubscriptionRefresh::BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
{
	CProtoBufMsg<CMsgSOCacheSubscriptionRefresh> msg( pNetPacket );

	CSteamID steamID( msg.Hdr().client_steam_id() );

	CSteamID steamIDCacheOwner( msg.Body().owner() );
	CGCSharedObjectCache *pCache = m_pGC->FindSOCache( steamIDCacheOwner );
	if ( pCache == NULL || !pCache->BIsSubscribed( steamID ) )
	{
		return false;
	}

	pCache->SendSubscriberMessage( steamID );

	return true;
}

GC_REG_JOB( CGCBase, CGCCacheSubscriptionRefresh, "CGCCacheSubscriptionRefresh", k_ESOMsg_CacheSubscriptionRefresh, k_EServerTypeGC );

} // namespace GCSDK

