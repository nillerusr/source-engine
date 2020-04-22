//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"

#include "msgprotobuf.h"
#include "smartptr.h"
#include "rtime.h"
#include "gcsdk/gcreportprinter.h"
#include <winsock.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

GCConVar cv_webapi_result_size_limit( "webapi_result_size_limit", "50000000" );
GCConVar cv_webapi_serialize_threads( "webapi_serialize_threads", "1", 0, "Switch for serializing webAPI responses on threads" );
static GCConVar webapi_account_tracking( "webapi_account_tracking", "1", "Controls whether or not account tracking stats are collected for web api usage" );
static GCConVar webapi_kill_switch( "webapi_kill_switch", "0", "When set to zero this will block no web api calls, when set to 1 this will block all web api except those sent by steam. To block steam, use the webapi_enable_steam_<priority> controls" );
static GCConVar webapi_kill_switch_error_response( "webapi_kill_switch_error_response", "1", "Determines if a response should be sent when the kill switch kills a web api request" );
static GCConVar webapi_rate_limit_calls_per_min( "webapi_rate_limit_calls_per_min", "600", "Determines how many messages can be sent from an account via the web api per minute. <0 disables this limiting" );
static GCConVar webapi_rate_limit_mb_per_min( "webapi_rate_limit_mb_per_min", "20", "Determines how many megabytes of data can be sent to an account via the web api per minute. <0 disables this limiting" );
static GCConVar webapi_elevated_rate_limit_calls_per_min( "webapi_elevated_rate_limit_calls_per_min", "-1", "Determines how many messages can be sent from an account via the web api per minute for elevated accounts. <0 disables this limiting" );
static GCConVar webapi_elevated_rate_limit_mb_per_min( "webapi_elevated_rate_limit_mb_per_min", "-1", "Determines how many megabytes of data can be sent to an account via the web api per minute for elevated accounts. <0 disables this limiting" );
static GCConVar webapi_ip_rate_limit( "webapi_ip_rate_limit", "1", "Controls whether or not we rate limit based upon IPs or just accounts" );

//-----------------------------------------------------------------------------
// CWebAPIAccountTracker
//
// Utility that tracks web api calls based upon accesses made by various users
//-----------------------------------------------------------------------------
class CWebAPIAccountTracker
{
public:
	//called when a web api request is made to track the call. A return value of true indicates that it should be allowed,
	//false indicates it should be blocked
	bool TrackUser( AccountID_t nID, uint32 nIP );
	//called once the size of the response is known and will track bandwidth and caller attributed to the provided function
	void TrackFunction( AccountID_t nID, uint32 nIP, const char* pszFunction, uint32 nResponseSize );

	//called to reset all permissions to default
	void ResetAccountPermissions();
	//called to associate a permission level with an account
	void SetAccountPermission( AccountID_t nID, EWebAPIAccountLevel eLevel );

	//completely resets accumulated stats
	void ResetStats();
	//resets just the profile stats
	void ResetProfileStats();

	//different caller report filters that can be used
	enum EDumpCaller
	{
		eDumpCaller_All,
		eDumpCaller_Blocked,
		eDumpCaller_Status,
		eDumpCaller_Calls,
	};

	//different reports that can be provided
	void DumpTotalCallers( EDumpCaller eFilter, const char* pszFunctionFilter = NULL ) const;
	void DumpTotalIPs( EDumpCaller eFilter, const char* pszFunctionFilter = NULL ) const;
	void DumpCaller( AccountID_t nID ) const;
	void DumpIP( uint32 nIP ) const;
	void DumpFunctions() const;
	void DumpProfile( bool bAllTime ) const;
	void DumpSteamServers() const;

	//given a steam server name, this will return the identifier of that server
	uint32 GetSteamServerID( const char* pszServer );

	//for steam level requests, we have a priority provided, and to track stats separately we break them up to different account IDs instead of just zero
	static const uint32	k_nSteamIP_High		= 2;
	static const uint32	k_nSteamIP_Normal	= 1;
	static const uint32	k_nSteamIP_Low		= 0;

private:

	//called to get the starting time of this rate interval
	RTime32 GetRateIntervalStart( AccountID_t nCaller ) const;

	struct SCallerStats
	{
		SCallerStats() :
			m_nBlockedCalls( 0 ),
			m_eLevel( eWebAPIAccountLevel_RateLimited )
		{
			ResetRateInterval( 0 );
		}

		void ResetRateInterval( RTime32 nStart )
		{
			m_nRateIntervalStartTime = nStart;
			m_nRateIntervalCalls = 0;
			m_nRateIntervalBytes = 0;
		}

		//the most recent rate interval that we received a message from the user (used to expire old counts)
		RTime32		m_nRateIntervalStartTime;
		//how many messages have been sent within this rate interval (used for rate limiting)
		uint32		m_nRateIntervalCalls;
		//how many bytes have been sent for this account during this interval
		uint32		m_nRateIntervalBytes;
		//total number of blocked calls
		uint32		m_nBlockedCalls;
		//flags associated with this caller, used to block/whitelist, etc
		EWebAPIAccountLevel		m_eLevel;
	};

	struct SFunctionStats
	{
		//track the number of calls and bandwidth. The profile versions are separate and used for displaying profiles over a window
		uint32	m_nTotalCalls;
		uint32	m_nProfileCalls;
		uint32	m_nMaxBytes;
		uint32	m_nProfileMaxBytes;
		uint64	m_nTotalBytes;
		uint64	m_nProfileBytes;		

		//a map of who has called us, which consists of the caller account and IP as the key
		struct SCaller
		{
			bool operator==( const SCaller& rhs ) const			{ return m_nAccountID == rhs.m_nAccountID && m_nIP == rhs.m_nIP; }
			AccountID_t	m_nAccountID;
			uint32		m_nIP;
		};
		struct SCalls
		{
			uint32	m_nCalls;
			uint64	m_nBytes;
		};
		CUtlHashMapLarge< SCaller, SCalls >	m_Callers;
	};

	//a structure used to simplify reporting so a vector can just be built of these, and then provided to the report function which will handle sorting it and displaying
	struct SReportRow
	{
		SReportRow( const char* pszFunction, uint32 nCalls, uint64 nSize ) :
			m_pszFunction( pszFunction ),
			m_nCalls( nCalls ),
			m_nSize( nSize )
		{}

		const char*		m_pszFunction;
		uint32			m_nCalls;
		uint64			m_nSize;
	};

	struct SSteamServer
	{
		CUtlString		m_sName;
		uint32			m_nID;
	};

	//called to find an existing user, or create one if not in the list already
	SCallerStats* CreateAccountUser( AccountID_t nID, RTime32 nRateIntervalStart );
	SCallerStats* CreateIPUser( uint32 nIP, RTime32 nRateIntervalStart );

	//called to print a report of the provided report rows as either an ID list or a function list. This will re-sort the provided vector
	static void PrintReport( const CUtlVector< SReportRow >& vec );

	//how many seconds are in a rate interval
	static const uint32 knRateIntervalTimeS = 60;

	CUtlHashMapLarge< AccountID_t, SCallerStats >	m_AccountCallers;
	CUtlHashMapLarge< uint32, SCallerStats >		m_IPCallers;
	CUtlHashMapLarge< uintp, SFunctionStats* >		m_Functions;
	CUtlHashMapLarge< const char*, SSteamServer*, CaseSensitiveStrEquals, MurmurHash3ConstCharPtr >	m_SteamServers;
	CJobTime	m_ProfileTime;
};

//our global profiler
static CWebAPIAccountTracker g_WebAPIAccountTracker;

void WebAPIAccount_ResetAllPermissions()
{
	g_WebAPIAccountTracker.ResetAccountPermissions();
}

void WebAPIAccount_SetPermission( AccountID_t nID, EWebAPIAccountLevel eLevel )
{
	g_WebAPIAccountTracker.SetAccountPermission( nID, eLevel );
}

bool WebAPIAccount_BTrackUserAndValidate( AccountID_t nID, uint32 unIP )
{
	return g_WebAPIAccountTracker.TrackUser( nID, unIP );
}

RTime32 CWebAPIAccountTracker::GetRateIntervalStart( AccountID_t nCaller ) const		
{ 
	//we shift the time by the account ID so that all users don't wrap at the same time which can cause a temporary surge in web API requests
	RTime32 curTime = CRTime::RTime32TimeCur() + ( nCaller % knRateIntervalTimeS );
	return curTime - ( curTime % knRateIntervalTimeS ); 
}

CWebAPIAccountTracker::SCallerStats* CWebAPIAccountTracker::CreateAccountUser( AccountID_t nID, RTime32 nRateIntervalStart )
{
	int nIndex = m_AccountCallers.Find( nID );
	if( nIndex == m_AccountCallers.InvalidIndex() )
	{
		nIndex = m_AccountCallers.Insert( nID );
		SCallerStats& caller = m_AccountCallers[ nIndex ];
		caller.m_nRateIntervalStartTime = nRateIntervalStart;

		//account ID is always unrestricted!
		if( nID == 0 )
			caller.m_eLevel = eWebAPIAccountLevel_Unlimited;
	}

	return &m_AccountCallers[ nIndex ];
}

CWebAPIAccountTracker::SCallerStats* CWebAPIAccountTracker::CreateIPUser( uint32 nIP, RTime32 nRateIntervalStart )
{
	int nIndex = m_IPCallers.Find( nIP );
	if( nIndex == m_IPCallers.InvalidIndex() )
	{
		nIndex = m_IPCallers.Insert( nIP );
		SCallerStats& caller = m_IPCallers[ nIndex ];
		caller.m_nRateIntervalStartTime = nRateIntervalStart;
	}

	return &m_IPCallers[ nIndex ];
}

uint32 CWebAPIAccountTracker::GetSteamServerID( const char* pszServer )
{
	int nIndex = m_SteamServers.Find( pszServer );
	if( nIndex == m_SteamServers.InvalidIndex() )
	{
		SSteamServer* pServer = new SSteamServer;
		pServer->m_sName = pszServer;
		pServer->m_nID = m_SteamServers.Count();
		m_SteamServers.Insert( pServer->m_sName, pServer );
		return pServer->m_nID;
	}

	return m_SteamServers[ nIndex ]->m_nID;
}

void CWebAPIAccountTracker::DumpSteamServers() const
{
	CGCReportPrinter rp;
	rp.AddStringColumn( "ID" );
	rp.AddStringColumn( "Server" );

	FOR_EACH_MAP_FAST( m_SteamServers, nServer )
	{
		const uint32 nID = m_SteamServers[ nServer ]->m_nID;
		rp.StrValue( CFmtStr( "%u.%u.%u.%u", iptod( nID << 8 ) ) );
		rp.StrValue( m_SteamServers[ nServer ]->m_sName );
		rp.CommitRow();
	}

	rp.SortReport( "ID", false );
	rp.PrintReport( SPEW_CONSOLE );
}

//determines what the resulting account level access should be based upon the access rights of the IP address and the account
static EWebAPIAccountLevel DetermineAccessLevel( EWebAPIAccountLevel eAccount, EWebAPIAccountLevel eIP )
{
	//unrestricted users should always be allowed, regardless of the IP range that they are making requests from, same with unlimited IP addresses
	if( ( eAccount == eWebAPIAccountLevel_Unlimited ) || ( eIP == eWebAPIAccountLevel_Unlimited ) )
		return eWebAPIAccountLevel_Unlimited;

	//otherwise, if either is blocked, then block
	if( ( eAccount == eWebAPIAccountLevel_Blocked ) || ( eIP == eWebAPIAccountLevel_Blocked ) )
		return eWebAPIAccountLevel_Blocked;

	//now we are dealing with default case versus elevated. Elevated wins over default
	if( ( eAccount == eWebAPIAccountLevel_Elevated ) || ( eIP == eWebAPIAccountLevel_Elevated ) )
		return eWebAPIAccountLevel_Elevated;

	//default
	return eWebAPIAccountLevel_RateLimited;
}

bool CWebAPIAccountTracker::TrackUser( AccountID_t nID, uint32 nIP )
{
	if( !webapi_account_tracking.GetBool() )
		return true;

	//first off update their aggregate caller stats
	{
		//what is our current time, and at what time did this rate interval start
		const RTime32 rateIntervalStart = GetRateIntervalStart( nID );
		
		//see if this account is completely blocked
		SCallerStats*  pAccountCaller	= CreateAccountUser( nID, rateIntervalStart );
		SCallerStats*  pIPCaller		= CreateIPUser( nIP, rateIntervalStart );

		//determine what our policy should be based upon the access level of the IP and the user
		EWebAPIAccountLevel eAccessLevel = DetermineAccessLevel( pAccountCaller->m_eLevel, pIPCaller->m_eLevel );

		//if we are blocked, just bail now
		if( eAccessLevel == eWebAPIAccountLevel_Blocked )
		{
			pAccountCaller->m_nBlockedCalls++;
			pIPCaller->m_nBlockedCalls++;
			return false;
		}

		//reset the rate interval tracking
		if( pAccountCaller->m_nRateIntervalStartTime < rateIntervalStart )
			pAccountCaller->ResetRateInterval( rateIntervalStart );
		if( pIPCaller->m_nRateIntervalStartTime < rateIntervalStart )
			pIPCaller->ResetRateInterval( rateIntervalStart );

		//now handle rate limiting
		if( ( eAccessLevel == eWebAPIAccountLevel_RateLimited ) || ( eAccessLevel == eWebAPIAccountLevel_Elevated ) )
		{
			//determine the rate we want to limit
			int32 nCallsPerMin = ( eAccessLevel == eWebAPIAccountLevel_RateLimited ) ? webapi_rate_limit_calls_per_min.GetInt() : webapi_elevated_rate_limit_calls_per_min.GetInt();
			int32 nBytesPerMin = ( ( eAccessLevel == eWebAPIAccountLevel_RateLimited ) ? webapi_rate_limit_mb_per_min.GetInt() : webapi_elevated_rate_limit_mb_per_min.GetInt() ) * 1024 * 1024;

			//see if this account is rate limited
			if( ( eAccessLevel == eWebAPIAccountLevel_RateLimited ) )
			{
				bool bAllow = true;

				//see if we are being limited based upon call rate limiting (tracking based upon ip and account) Note that
				//we don't return until we've dones stat tracking for both so the reports are accurate and capture it at both levels
				if( ( nCallsPerMin >= 0 && pAccountCaller->m_nRateIntervalCalls >= ( uint32 )nCallsPerMin ) ||
					( nBytesPerMin >= 0 && pAccountCaller->m_nRateIntervalBytes >= ( uint32 )nBytesPerMin ) )
				{
					pAccountCaller->m_nBlockedCalls++;
					bAllow = false;
				}

				if( webapi_ip_rate_limit.GetBool() )
				{
					if( ( nCallsPerMin >= 0 && pIPCaller->m_nRateIntervalCalls >= ( uint32 )nCallsPerMin ) ||
						( nBytesPerMin >= 0 && pIPCaller->m_nRateIntervalBytes >= ( uint32 )nBytesPerMin ) )
					{
						pIPCaller->m_nBlockedCalls++;
						bAllow = false;
					}
				}

				if( !bAllow )
					return false;
			}
		}
	}

	return true;
}

void CWebAPIAccountTracker::TrackFunction( AccountID_t nID, uint32 nIP, const char* pszFunction, uint32 nResponseSize )
{
	if( !webapi_account_tracking.GetBool() )
		return;

	//update the bytes for that user
	{
		int nCallerIndex = m_AccountCallers.Find( nID );
		if( nCallerIndex != m_AccountCallers.InvalidIndex() )
		{
			SCallerStats& caller = m_AccountCallers[ nCallerIndex ];
			caller.m_nRateIntervalBytes += nResponseSize;
			caller.m_nRateIntervalCalls++;
		}
	}

	//update the bytes for that user and for their IP
	{
		int nCallerIndex = m_IPCallers.Find( nIP );
		if( nCallerIndex != m_IPCallers.InvalidIndex() )
		{
			SCallerStats& caller = m_IPCallers[ nCallerIndex ];
			caller.m_nRateIntervalBytes += nResponseSize;
			caller.m_nRateIntervalCalls++;
		}
	}

	//now update the function specific stats
	{
		int nFunctionIndex = m_Functions.Find( ( uintp )pszFunction );
		if( nFunctionIndex == m_Functions.InvalidIndex() )
		{
			SFunctionStats* pNewStats = new SFunctionStats;
			pNewStats->m_nTotalCalls = 0;
			pNewStats->m_nProfileCalls = 0;
			pNewStats->m_nTotalBytes = 0;
			pNewStats->m_nProfileBytes = 0;
			pNewStats->m_nMaxBytes = 0;
			pNewStats->m_nProfileMaxBytes = 0;
			nFunctionIndex = m_Functions.Insert( ( uintp )pszFunction, pNewStats );
		}
		
		//update our stats
		SFunctionStats& function = *m_Functions[ nFunctionIndex ];
		function.m_nTotalCalls++;
		function.m_nProfileCalls++;
		function.m_nTotalBytes += nResponseSize;
		function.m_nProfileBytes += nResponseSize;
		function.m_nMaxBytes = MAX( function.m_nMaxBytes, nResponseSize );
		function.m_nProfileMaxBytes = MAX( function.m_nProfileMaxBytes, nResponseSize );

		//update caller stats
		{
			struct SFunctionStats::SCaller caller;
			caller.m_nAccountID = nID;
			caller.m_nIP = nIP;
			
			int nCallerIndex = function.m_Callers.Find( caller );
			if( nCallerIndex == function.m_Callers.InvalidIndex() )
			{
				nCallerIndex = function.m_Callers.Insert( caller );
				function.m_Callers[ nCallerIndex ].m_nCalls = 1;
				function.m_Callers[ nCallerIndex ].m_nBytes = nResponseSize;
			}
			else
			{
				function.m_Callers[ nCallerIndex ].m_nCalls++;
				function.m_Callers[ nCallerIndex ].m_nBytes += nResponseSize;
			}
		}
	}
}

void CWebAPIAccountTracker::SetAccountPermission( AccountID_t nID, EWebAPIAccountLevel eLevel )
{
	SCallerStats*  pCaller = CreateAccountUser( nID, GetRateIntervalStart( nID ) );
	pCaller->m_eLevel = eLevel;
}

void CWebAPIAccountTracker::ResetAccountPermissions()
{
	FOR_EACH_MAP_FAST( m_AccountCallers, nCaller )
	{
		m_AccountCallers[ nCaller ].m_eLevel = eWebAPIAccountLevel_RateLimited;
	}
	FOR_EACH_MAP_FAST( m_IPCallers, nCaller )
	{
		m_IPCallers[ nCaller ].m_eLevel = eWebAPIAccountLevel_RateLimited;
	}
}

void CWebAPIAccountTracker::ResetStats()
{
	FOR_EACH_MAP_FAST( m_AccountCallers, nCaller )
	{
		m_AccountCallers[ nCaller ].ResetRateInterval( GetRateIntervalStart( m_AccountCallers.Key( nCaller ) ) );
		m_AccountCallers[ nCaller ].m_nBlockedCalls = 0;
	}
	FOR_EACH_MAP_FAST( m_IPCallers, nCaller )
	{
		m_IPCallers[ nCaller ].ResetRateInterval( GetRateIntervalStart( m_IPCallers.Key( nCaller ) ) );
		m_IPCallers[ nCaller ].m_nBlockedCalls = 0;
	}
	m_Functions.PurgeAndDeleteElements();
}

void CWebAPIAccountTracker::ResetProfileStats()
{
	FOR_EACH_MAP_FAST( m_Functions, nFunction )
	{
		m_Functions[ nFunction ]->m_nProfileCalls = 0;
		m_Functions[ nFunction ]->m_nProfileBytes = 0;
		m_Functions[ nFunction ]->m_nProfileMaxBytes = 0;
	}
	m_ProfileTime.SetToJobTime();
}

static const int k_cSteamIDRenderedMaxLen = 36;

//-----------------------------------------------------------------------------
// Purpose: Renders the steam ID to a buffer with an admin console link.  NOTE: for convenience of
//			calling code, this code returns a pointer to a static buffer and is NOT thread-safe.
// Output:  buffer with rendered Steam ID
//-----------------------------------------------------------------------------
static const char * CSteamID_RenderLink( const CSteamID & steamID )
{
	// longest length of returned string is k_cBufLen
	//	<link cmd="steamid64 %llu"></link> => 30 + 20 == 50
	//	50 + k_cSteamIDRenderedMaxLen + 1
	const int k_cBufLen = 50 + k_cSteamIDRenderedMaxLen + 1;

	const int k_cBufs = 4;	// # of static bufs to use (so people can compose output with multiple calls to RenderLink() )
	static char rgchBuf[k_cBufs][k_cBufLen];
	static int nBuf = 0;
	char * pchBuf = rgchBuf[nBuf];	// get pointer to current static buf
	nBuf++;	// use next buffer for next call to this method
	nBuf %= k_cBufs;

	Q_snprintf( pchBuf, k_cBufLen, "<link cmd=\"steamid64 %llu\">%s</link>", steamID.ConvertToUint64(), steamID.Render() );
	return pchBuf;
}


//-----------------------------------------------------------------------------
// Purpose: Renders the passed-in steam ID to a buffer with admin console link.  NOTE: for convenience
//			of calling code, this code returns a pointer to a static buffer and is NOT thread-safe.
// Input:	64-bit representation of Steam ID to render
// Output:  buffer with rendered Steam ID link
//-----------------------------------------------------------------------------
static const char * CSteamID_RenderLink( uint64 ulSteamID )
{
	CSteamID steamID( ulSteamID );
	return CSteamID_RenderLink( steamID );
}



void CWebAPIAccountTracker::DumpCaller( AccountID_t nID ) const
{
	const CSteamID steamID = GGCInterface()->ConstructSteamIDForClient( nID );
	//cache the account name here so we don't yield while we have indices
	CUtlString sPersona = GGCBase()->YieldingGetPersonaName( steamID, "[Unknown]" );

	//dump high level user stats
	int nCallerIndex = m_AccountCallers.Find( nID );
	if( nCallerIndex == m_AccountCallers.InvalidIndex() )
	{
		EG_MSG( SPEW_CONSOLE, "User %u not found in any web api calls\n", nID );
		return;
	}
	
	//a map of IP addresses that have been used by this account
	CUtlHashMapLarge< uint32, SFunctionStats::SCalls >	ipCalls;

	//now each function they called
	uint64 nTotalBytes = 0;
	uint32 nTotalCalls = 0;
	CUtlVector< SReportRow > vFuncs;
	FOR_EACH_MAP_FAST( m_Functions, nFunc )
	{
		//add up how many calls they made to this function across all IPs
		uint64 nFnBytes = 0;
		uint32 nFnCalls = 0;
		FOR_EACH_MAP_FAST( m_Functions[ nFunc ]->m_Callers, nCaller )
		{
			const CWebAPIAccountTracker::SFunctionStats::SCaller& caller = m_Functions[ nFunc ]->m_Callers.Key( nCaller );
			if( caller.m_nAccountID == nID )
			{
				const CWebAPIAccountTracker::SFunctionStats::SCalls& calls = m_Functions[ nFunc ]->m_Callers[ nCaller ];
				nFnBytes += calls.m_nBytes;
				nFnCalls += calls.m_nCalls;

				int nIPIndex = ipCalls.Find( caller.m_nIP );
				if( nIPIndex == ipCalls.InvalidIndex() )
				{
					SFunctionStats::SCalls toAdd;
					toAdd.m_nBytes = calls.m_nBytes;
					toAdd.m_nCalls = calls.m_nCalls;
					ipCalls.Insert( caller.m_nIP, toAdd );
				}
				else
				{
					ipCalls[ nIPIndex ].m_nBytes += calls.m_nBytes;
					ipCalls[ nIPIndex ].m_nBytes += calls.m_nCalls;
				}				
			}
		}

		if( nFnCalls > 0 )
		{
			vFuncs.AddToTail( SReportRow( ( const char* )m_Functions.Key( nFunc ), nFnCalls, nFnBytes ) );
		}
		nTotalBytes += nFnBytes;
		nTotalCalls += nFnCalls;
	}

	const SCallerStats& caller = m_AccountCallers[ nCallerIndex ];
	EG_MSG( SPEW_CONSOLE, "---------------------------------------------------\n" );
	EG_MSG( SPEW_CONSOLE, "User %s: \"%s\"\n", CSteamID_RenderLink( steamID ), sPersona.String() );
	double fTotalMB = nTotalBytes / ( 1024.0 * 1024.0 );
	double fMBPerHour = fTotalMB / ( GGCBase()->GetGCUpTime() / 3600.0 );
	double fCallsPerHour = nTotalCalls / ( GGCBase()->GetGCUpTime() / 3600.0 );
	EG_MSG( SPEW_CONSOLE, "\tAccess: %u, Total Calls: %u, Blocked calls: %u, Total: %.2fMB, MB/h: %.2f, Calls/h: %.0f\n", caller.m_eLevel, nTotalCalls, caller.m_nBlockedCalls, fTotalMB, fMBPerHour, fCallsPerHour );
	//don't let someone accidentally change Steam's access!
	if( nID != 0 )
	{
		if( caller.m_eLevel == eWebAPIAccountLevel_RateLimited )
		{
			EG_MSG( SPEW_CONSOLE, "\t<link cmd=\"webapi_account_set_access %u %d\">[Block Account]</link>", nID, eWebAPIAccountLevel_Blocked );
			EG_MSG( SPEW_CONSOLE, "\t<link cmd=\"webapi_account_set_access %u %d\">[Elevate Account]</link>\n", nID, eWebAPIAccountLevel_Elevated );
		}
		else if( caller.m_eLevel == eWebAPIAccountLevel_Blocked )
		{
			EG_MSG( SPEW_CONSOLE, "\t<link cmd=\"webapi_account_set_access %u %d\">[Unblock Account]</link>\n", nID, eWebAPIAccountLevel_RateLimited );
		}
		else if( caller.m_eLevel == eWebAPIAccountLevel_Elevated )
		{
			EG_MSG( SPEW_CONSOLE, "\t<link cmd=\"webapi_account_set_access %u %d\">[Demote Account]</link>\n", nID, eWebAPIAccountLevel_RateLimited );
		}
	}

	//print a report of the IP addresses that they are calling from
	{
		CGCReportPrinter rp;
		rp.AddStringColumn( "IP" );
		rp.AddIntColumn( "Calls", CGCReportPrinter::eSummary_Total );
		rp.AddIntColumn( "MB", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );

		FOR_EACH_MAP_FAST( ipCalls, nIP )
		{
			rp.StrValue( CFmtStr( "%u.%u.%u.%u", iptod( ipCalls.Key( nIP ) ) ), CFmtStr( "webapi_account_dump_ip %u", ipCalls.Key( nIP ) ) );
			rp.IntValue( ipCalls[ nIP ].m_nCalls );
			rp.IntValue( ipCalls[ nIP ].m_nBytes );
			rp.CommitRow();
		}

		rp.SortReport( "MB" );
		rp.PrintReport( SPEW_CONSOLE );
	}

	//and print a report of all the functions that they've called
	PrintReport( vFuncs );
}

void CWebAPIAccountTracker::DumpIP( uint32 nIP ) const
{
	//dump high level user stats
	int nCallerIndex = m_IPCallers.Find( nIP );
	if( nCallerIndex == m_IPCallers.InvalidIndex() )
	{
		EG_MSG( SPEW_CONSOLE, "IP %u not found in any web api calls\n", nIP );
		return;
	}

	//a map of IP addresses that have been used by this account
	CUtlHashMapLarge< AccountID_t, SFunctionStats::SCalls >	accountCalls;

	//now each function they called
	uint64 nTotalBytes = 0;
	uint32 nTotalCalls = 0;
	CUtlVector< SReportRow > vFuncs;
	FOR_EACH_MAP_FAST( m_Functions, nFunc )
	{
		//add up how many calls they made to this function across all IPs
		uint64 nFnBytes = 0;
		uint32 nFnCalls = 0;
		FOR_EACH_MAP_FAST( m_Functions[ nFunc ]->m_Callers, nCaller )
		{
			const CWebAPIAccountTracker::SFunctionStats::SCaller& caller = m_Functions[ nFunc ]->m_Callers.Key( nCaller );
			if( caller.m_nIP == nIP )
			{
				const CWebAPIAccountTracker::SFunctionStats::SCalls& calls = m_Functions[ nFunc ]->m_Callers[ nCaller ];
				nFnBytes += calls.m_nBytes;
				nFnCalls += calls.m_nCalls;

				int nAccountIndex = accountCalls.Find( caller.m_nAccountID );
				if( nAccountIndex == accountCalls.InvalidIndex() )
				{
					SFunctionStats::SCalls toAdd;
					toAdd.m_nBytes = calls.m_nBytes;
					toAdd.m_nCalls = calls.m_nCalls;
					accountCalls.Insert( caller.m_nAccountID, toAdd );
					GGCBase()->PreloadPersonaName( GGCInterface()->ConstructSteamIDForClient( caller.m_nAccountID ) );
				}
				else
				{
					accountCalls[ nAccountIndex ].m_nBytes += calls.m_nBytes;
					accountCalls[ nAccountIndex ].m_nBytes += calls.m_nCalls;
				}				
			}
		}

		if( nFnCalls > 0 )
		{
			vFuncs.AddToTail( SReportRow( ( const char* )m_Functions.Key( nFunc ), nFnCalls, nFnBytes ) );
		}
		nTotalBytes += nFnBytes;
		nTotalCalls += nFnCalls;
	}

	const SCallerStats& caller = m_IPCallers[ nCallerIndex ];
	EG_MSG( SPEW_CONSOLE, "---------------------------------------------------\n" );
	EG_MSG( SPEW_CONSOLE, "IP %u.%u.%u.%u\n", iptod( nIP ) );
	double fTotalMB = nTotalBytes / ( 1024.0 * 1024.0 );
	double fMBPerHour = fTotalMB / ( GGCBase()->GetGCUpTime() / 3600.0 );
	double fCallsPerHour = nTotalCalls / ( GGCBase()->GetGCUpTime() / 3600.0 );
	EG_MSG( SPEW_CONSOLE, "\tAccess: %u, Total Calls: %u, Blocked calls: %u, Total: %.2fMB, MB/h: %.2f, Calls/h: %.0f\n", caller.m_eLevel, nTotalCalls, caller.m_nBlockedCalls, fTotalMB, fMBPerHour, fCallsPerHour );
	
	//print a report of the accounts that they are calling from
	{
		CGCReportPrinter rp;
		rp.AddSteamIDColumn( "Account" );
		rp.AddStringColumn( "Persona" );
		rp.AddIntColumn( "Calls", CGCReportPrinter::eSummary_Total );
		rp.AddIntColumn( "MB", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );

		FOR_EACH_MAP_FAST( accountCalls, nAccount )
		{
			CSteamID steamID = GGCInterface()->ConstructSteamIDForClient( accountCalls.Key( nAccount ) );
			rp.SteamIDValue( steamID, CFmtStr( "webapi_account_dump_caller %u", steamID.GetAccountID() ) );
			rp.StrValue( GGCBase()->YieldingGetPersonaName( steamID, "[unknown]" ), CFmtStr( "webapi_account_dump_caller %u", steamID.GetAccountID() ) );
			rp.IntValue( accountCalls[ nAccount ].m_nCalls );
			rp.IntValue( accountCalls[ nAccount ].m_nBytes );
			rp.CommitRow();
		}

		rp.SortReport( "MB" );
		rp.PrintReport( SPEW_CONSOLE );
	}

	//and print a report of all the functions that they've called
	PrintReport( vFuncs );
}

struct SCallerReportStats
{
	uint32		m_nFunctions;
	uint32		m_nCalls;
	uint64		m_nBytes;
};

void CWebAPIAccountTracker::DumpTotalCallers( EDumpCaller eFilter, const char* pszFunctionFilter ) const
{
	//accumulate stats for each unique caller
	CUtlHashMapLarge< AccountID_t, SCallerReportStats > mapCallers;
	FOR_EACH_MAP_FAST( m_Functions, nCurrFunction )
	{
		//handle filtering out functions we don't care about
		const char* pszFunctionName = ( const char* )m_Functions.Key( nCurrFunction );
		if( pszFunctionFilter && ( V_stristr( pszFunctionName, pszFunctionFilter ) == NULL ) )
			continue;

		const SFunctionStats& function = *m_Functions[ nCurrFunction ];
		FOR_EACH_MAP_FAST( function.m_Callers, nCurrCaller )
		{
			const AccountID_t key = function.m_Callers.Key( nCurrCaller ).m_nAccountID;

			//add this account
			int nStatIndex = mapCallers.Find( key );
			if( nStatIndex == mapCallers.InvalidIndex() )
			{
				SCallerReportStats stats;
				stats.m_nFunctions = 0;
				stats.m_nCalls = 0;
				stats.m_nBytes = 0;
				nStatIndex = mapCallers.Insert( key, stats );
				GGCBase()->PreloadPersonaName( GGCInterface()->ConstructSteamIDForClient( key ) );
			}

			mapCallers[ nStatIndex ].m_nFunctions	+= 1;
			mapCallers[ nStatIndex ].m_nCalls		+= function.m_Callers[ nCurrCaller ].m_nCalls;
			mapCallers[ nStatIndex ].m_nBytes		+= function.m_Callers[ nCurrCaller ].m_nBytes;
		}
	}

	CGCReportPrinter rp;
	rp.AddStringColumn( "Account" );
	rp.AddIntColumn( "Calls", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "MB", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );
	rp.AddIntColumn( "Blocked", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "Access", CGCReportPrinter::eSummary_None );
	rp.AddIntColumn( "APIs", CGCReportPrinter::eSummary_None );
	rp.AddIntColumn( "MB/h", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );
	rp.AddIntColumn( "Calls/h", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "KB/c", CGCReportPrinter::eSummary_None );
	rp.AddStringColumn( "UserName" );
	
	const double fUpTimeHrs = MAX( GGCBase()->GetGCUpTime() / 3600.0, 0.01 );

	//now show the report
	FOR_EACH_MAP_FAST( m_AccountCallers, nCurrCaller )
	{
		//apply filters to our results
		if( ( eFilter == eDumpCaller_Blocked ) && ( m_AccountCallers[ nCurrCaller ].m_nBlockedCalls == 0 ) )
			continue;
		if( ( eFilter == eDumpCaller_Status ) && ( m_AccountCallers[ nCurrCaller ].m_eLevel == eWebAPIAccountLevel_RateLimited ) )
			continue;

		const AccountID_t accountID = m_AccountCallers.Key( nCurrCaller );
		const SCallerReportStats* pStats = NULL;
		int nCollectedStats = mapCallers.Find( accountID );
		if( nCollectedStats != mapCallers.InvalidIndex() )
		{
			pStats = &mapCallers[ nCollectedStats ];
		}

		//filter out users that didn't make any calls if appropriate
		if( ( eFilter == eDumpCaller_Calls ) && ( !pStats || ( pStats->m_nCalls == 0 ) ) )
			continue;

		const CSteamID steamID( GGCInterface()->ConstructSteamIDForClient( accountID ) );
		rp.StrValue( steamID.Render() , CFmtStr( "webapi_account_dump_caller %u", accountID ) );
		rp.IntValue( ( pStats ) ? pStats->m_nCalls : 0 );
		rp.IntValue( ( pStats ) ? pStats->m_nBytes : 0 );
		rp.IntValue( m_AccountCallers[ nCurrCaller ].m_nBlockedCalls );
		rp.IntValue( m_AccountCallers[ nCurrCaller ].m_eLevel );
		rp.IntValue( ( pStats ) ? pStats->m_nFunctions : 0 );
		rp.IntValue( ( int64 )( ( ( pStats ) ? pStats->m_nBytes : 0 ) / fUpTimeHrs ) );
		rp.IntValue( ( int64 )( ( ( pStats ) ? pStats->m_nCalls : 0 ) / fUpTimeHrs ) );
		rp.IntValue( ( pStats && pStats->m_nCalls > 0 ) ? ( pStats->m_nBytes / pStats->m_nCalls ) / 1024 : 0 );
		rp.StrValue( GGCBase()->YieldingGetPersonaName( steamID, "[unknown]" ) );
		rp.CommitRow();
	}

	const char* pszSort = "MB/h";
	if( eFilter == eDumpCaller_Blocked )
		pszSort = "Blocked";
	else if( eFilter == eDumpCaller_Status )
		pszSort = "Access";

	rp.SortReport( pszSort );
	rp.PrintReport( SPEW_CONSOLE );
}

void CWebAPIAccountTracker::DumpTotalIPs( EDumpCaller eFilter, const char* pszFunctionFilter ) const
{
	//accumulate stats for each unique caller
	CUtlHashMapLarge< uint32, SCallerReportStats > mapIPs;
	FOR_EACH_MAP_FAST( m_Functions, nCurrFunction )
	{
		//handle filtering out functions we don't care about
		const char* pszFunctionName = ( const char* )m_Functions.Key( nCurrFunction );
		if( pszFunctionFilter && ( V_stristr( pszFunctionName, pszFunctionFilter ) == NULL ) )
			continue;

		const SFunctionStats& function = *m_Functions[ nCurrFunction ];
		FOR_EACH_MAP_FAST( function.m_Callers, nCurrCaller )
		{
			const uint32 key = function.m_Callers.Key( nCurrCaller ).m_nIP;

			//add this account
			int nStatIndex = mapIPs.Find( key );
			if( nStatIndex == mapIPs.InvalidIndex() )
			{
				SCallerReportStats stats;
				stats.m_nFunctions = 0;
				stats.m_nCalls = 0;
				stats.m_nBytes = 0;
				nStatIndex = mapIPs.Insert( key, stats );
			}

			mapIPs[ nStatIndex ].m_nFunctions	+= 1;
			mapIPs[ nStatIndex ].m_nCalls		+= function.m_Callers[ nCurrCaller ].m_nCalls;
			mapIPs[ nStatIndex ].m_nBytes		+= function.m_Callers[ nCurrCaller ].m_nBytes;
		}
	}

	CGCReportPrinter rp;
	rp.AddStringColumn( "IP" );
	rp.AddIntColumn( "Calls", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "MB", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );
	rp.AddIntColumn( "Blocked", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "Access", CGCReportPrinter::eSummary_None );
	rp.AddIntColumn( "APIs", CGCReportPrinter::eSummary_None );
	rp.AddIntColumn( "MB/h", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );
	rp.AddIntColumn( "Calls/h", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "KB/c", CGCReportPrinter::eSummary_None );
	
	const double fUpTimeHrs = MAX( GGCBase()->GetGCUpTime() / 3600.0, 0.01 );

	//now show the report
	FOR_EACH_MAP_FAST( m_IPCallers, nCurrCaller )
	{
		//apply filters to our results
		if( ( eFilter == eDumpCaller_Blocked ) && ( m_IPCallers[ nCurrCaller ].m_nBlockedCalls == 0 ) )
			continue;
		if( ( eFilter == eDumpCaller_Status ) && ( m_IPCallers[ nCurrCaller ].m_eLevel == eWebAPIAccountLevel_RateLimited ) )
			continue;

		const uint32 nIP = m_IPCallers.Key( nCurrCaller );
		const SCallerReportStats* pStats = NULL;
		int nCollectedStats = mapIPs.Find( nIP );
		if( nCollectedStats != mapIPs.InvalidIndex() )
		{
			pStats = &mapIPs[ nCollectedStats ];
		}

		//filter out users that didn't make any calls if appropriate
		if( ( eFilter == eDumpCaller_Calls ) && ( !pStats || ( pStats->m_nCalls == 0 ) ) )
			continue;

		rp.StrValue( CFmtStr( "%u.%u.%u.%u", iptod( nIP ) ), CFmtStr( "webapi_account_dump_ip %u", nIP ) );
		rp.IntValue( ( pStats ) ? pStats->m_nCalls : 0 );
		rp.IntValue( ( pStats ) ? pStats->m_nBytes : 0 );
		rp.IntValue( m_IPCallers[ nCurrCaller ].m_nBlockedCalls );
		rp.IntValue( m_IPCallers[ nCurrCaller ].m_eLevel );
		rp.IntValue( ( pStats ) ? pStats->m_nFunctions : 0 );
		rp.IntValue( ( int64 )( ( ( pStats ) ? pStats->m_nBytes : 0 ) / fUpTimeHrs ) );
		rp.IntValue( ( int64 )( ( ( pStats ) ? pStats->m_nCalls : 0 ) / fUpTimeHrs ) );
		rp.IntValue( ( pStats && pStats->m_nCalls > 0 ) ? ( pStats->m_nBytes / pStats->m_nCalls ) / 1024 : 0 );
		rp.CommitRow();
	}

	const char* pszSort = "MB/h";
	if( eFilter == eDumpCaller_Blocked )
		pszSort = "Blocked";
	else if( eFilter == eDumpCaller_Status )
		pszSort = "Access";

	rp.SortReport( pszSort );
	rp.PrintReport( SPEW_CONSOLE );
}

void CWebAPIAccountTracker::DumpFunctions() const
{
	CUtlVector< SReportRow > vFuncs;
	vFuncs.EnsureCapacity( m_Functions.Count() );

	FOR_EACH_MAP_FAST( m_Functions, i )
	{
		vFuncs.AddToTail( SReportRow( ( const char* )m_Functions.Key( i ), m_Functions[ i ]->m_nTotalCalls, m_Functions[ i ]->m_nTotalBytes ) );
	}
	PrintReport( vFuncs );	
}

void CWebAPIAccountTracker::DumpProfile( bool bAllTime ) const
{
	//accumulate totals so we can do percentage
	uint32 nTotalCalls = 0;
	uint64 nTotalBytes = 0;
	FOR_EACH_MAP_FAST( m_Functions, nFunction )
	{
		if( bAllTime )
		{
			nTotalCalls += m_Functions[ nFunction ]->m_nTotalCalls;
			nTotalBytes += m_Functions[ nFunction ]->m_nTotalBytes;
		}
		else
		{
			nTotalCalls += m_Functions[ nFunction ]->m_nProfileCalls;
			nTotalBytes += m_Functions[ nFunction ]->m_nProfileBytes;
		}
	}

	//determine how much time we are covering, and come up with a scale to normalize the values
	uint64 nSampleMicroS = ( bAllTime ) ? ( uint64 )GGCBase()->GetGCUpTime() * 1000000 : m_ProfileTime.CServerMicroSecsPassed();
	double fToPS = ( nSampleMicroS > 0 ) ? 1000000.0 / ( double )nSampleMicroS : 1.0;

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Web API Profile: Sampled %.2f seconds\n", nSampleMicroS / ( 1000.0 * 1000.0 ) );

	CGCReportPrinter rp;
	rp.AddStringColumn( "Web API Name" );
	rp.AddIntColumn( "MB", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );
	rp.AddFloatColumn( "%", CGCReportPrinter::eSummary_Total, 1 );
	rp.AddFloatColumn( "KBPS", CGCReportPrinter::eSummary_Total, 1 );
	rp.AddIntColumn( "Calls", CGCReportPrinter::eSummary_Total );
	rp.AddFloatColumn( "%", CGCReportPrinter::eSummary_Total, 1 );
	rp.AddFloatColumn( "CallsPS", CGCReportPrinter::eSummary_Total, 1 );
	rp.AddIntColumn( "MaxKB", CGCReportPrinter::eSummary_Max );

	FOR_EACH_MAP_FAST( m_Functions, nFunction )
	{
		const SFunctionStats* pFunc = m_Functions[ nFunction ];
		uint32 nCalls = ( bAllTime ) ? pFunc->m_nTotalCalls : pFunc->m_nProfileCalls;
		uint32 nMax	  = ( bAllTime ) ? pFunc->m_nMaxBytes   : pFunc->m_nProfileMaxBytes;
		uint64 nBytes = ( bAllTime ) ? pFunc->m_nTotalBytes : pFunc->m_nProfileBytes;

		rp.StrValue( ( const char* )m_Functions.Key( nFunction ) );
		rp.IntValue( nBytes );
		rp.FloatValue( ( 100.0 * nBytes ) / nTotalBytes );
		rp.FloatValue( ( nBytes / 1024.0 ) * fToPS );
		rp.IntValue( nCalls );
		rp.FloatValue( ( 100.0 * nCalls ) / nTotalCalls );
		rp.FloatValue( nCalls * fToPS );
		rp.IntValue( nMax / 1024 );
		rp.CommitRow();
	}

	rp.SortReport( "KBPS" );
	rp.PrintReport( SPEW_CONSOLE );
}

void CWebAPIAccountTracker::PrintReport( const CUtlVector< SReportRow >& vec )
{
	CGCReportPrinter rp;

	//now print it out based upon the type
	rp.AddStringColumn( "Function" );
	rp.AddIntColumn( "Calls", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "MB", CGCReportPrinter::eSummary_Total, CGCReportPrinter::eIntDisplay_Memory_MB );
	rp.AddFloatColumn( "Calls/h", CGCReportPrinter::eSummary_Total, 0 );
	rp.AddFloatColumn( "MB/h", CGCReportPrinter::eSummary_Total );

	const double fUpTimeHrs = MAX( GGCBase()->GetGCUpTime() / 3600.0, 0.01 );

	for( int i = 0; i < vec.Count(); i++ )
	{
		rp.StrValue( vec[ i ].m_pszFunction, CFmtStr( "webapi_account_dump_function_callers %s", vec[ i ].m_pszFunction ) );
		rp.IntValue( vec[ i ].m_nCalls );
		rp.IntValue( vec[ i ].m_nSize );
		rp.FloatValue( vec[ i ].m_nCalls / fUpTimeHrs );
		rp.FloatValue( ( vec[ i ].m_nSize / ( 1024.0 * 1024.0 ) ) / fUpTimeHrs );
		rp.CommitRow();
	}

	rp.SortReport( "MB/h" );
	rp.PrintReport( SPEW_CONSOLE );
}


GC_CON_COMMAND( webapi_account_dump_steam_servers, "Dumps the ID listings of the various steam servers encoded in the IP address of Steam requests" )
{
	g_WebAPIAccountTracker.DumpSteamServers();
}

GC_CON_COMMAND( webapi_account_dump_callers, "Dumps the most frequent callers of web api's for the current run of the GC" )
{
	g_WebAPIAccountTracker.DumpTotalCallers( CWebAPIAccountTracker::eDumpCaller_Calls );
}

GC_CON_COMMAND( webapi_account_dump_ips, "Dumps the most frequent ip callers of web api's for the current run of the GC" )
{
	g_WebAPIAccountTracker.DumpTotalIPs( CWebAPIAccountTracker::eDumpCaller_Calls );
}

GC_CON_COMMAND( webapi_account_dump_blocked_callers, "Dumps the callers that have been blocked and how many calls have been blocked" )
{
	g_WebAPIAccountTracker.DumpTotalCallers( CWebAPIAccountTracker::eDumpCaller_Blocked );
}

GC_CON_COMMAND( webapi_account_dump_caller_access, "Dumps the access rights of any caller that is not the default rate limiting" )
{
	g_WebAPIAccountTracker.DumpTotalCallers( CWebAPIAccountTracker::eDumpCaller_Status );
}

GC_CON_COMMAND( webapi_account_dump_functions, "Dumps the most frequently called web api functions" )
{
	g_WebAPIAccountTracker.DumpFunctions();
}

GC_CON_COMMAND_PARAMS( webapi_account_dump_function_callers, 1, "<function name> - Dumps the most frequent callers of functions that match the provided substring" )
{
	g_WebAPIAccountTracker.DumpTotalCallers( CWebAPIAccountTracker::eDumpCaller_Calls, args[ 1 ] );
}

GC_CON_COMMAND_PARAMS( webapi_account_dump_caller, 1, "<caller account> - Dumps the functions that the provided account ID has been calling the most" )
{
	g_WebAPIAccountTracker.DumpCaller( ( AccountID_t )V_atoui64( args[ 1 ] ) );
}

GC_CON_COMMAND_PARAMS( webapi_account_dump_ip, 1, "<ip> - Dumps the functions that the provided ip has been calling the most" )
{
	g_WebAPIAccountTracker.DumpIP( ( AccountID_t )V_atoui64( args[ 1 ] ) );
}

GC_CON_COMMAND( webapi_account_reset_stats, "Forces a reset of all stats collected for web api account stats" )
{
	g_WebAPIAccountTracker.ResetStats();
}

//utility class for dumping out the profile results after time has expired
static void DumpWebAPIProfile()
{
	g_WebAPIAccountTracker.DumpProfile( false );
}

GC_CON_COMMAND_PARAMS( webapi_profile, 1, "<seconds to profile> Turns on web api profiling for N seconds and dumps the results" )
{
	float fSeconds = MAX( 1.0f, atof( args[ 1 ] ) );
	g_WebAPIAccountTracker.ResetProfileStats();
	static CGlobalScheduledFunction s_DumpProfile;
	s_DumpProfile.ScheduleMS( DumpWebAPIProfile, fSeconds * 1000.0f );
}

//console commands to control web API profiling
GC_CON_COMMAND( webapi_profile_reset, "Turns on web api profiling" )
{
	g_WebAPIAccountTracker.ResetProfileStats();
}

GC_CON_COMMAND( webapi_profile_dump, "Displays stats collected while web api profiling was enabled" )
{
	g_WebAPIAccountTracker.DumpProfile( false );
}

GC_CON_COMMAND( webapi_profile_dump_total, "Displays stats collected while web api profiling was enabled" )
{
	g_WebAPIAccountTracker.DumpProfile( true );
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message and waits for a response
// Input:	steamIDTarget - The entity this message is going to
//			msgOut - The message to send
//			nTimeoutSec - Number of seconds to wait for a response
//			pMsgIn - Pointer to the message that will contain the response
//			eMsg - The type of message the response should be
// Returns:	True is the response was received, false otherwise. The contents
//	of pMsgIn will be valid only if the function returns true.
//-----------------------------------------------------------------------------
bool CGCJob::BYldSendMessageAndGetReply( CSteamID &steamIDTarget, CGCMsgBase &msgOut, uint nTimeoutSec, CGCMsgBase *pMsgIn, MsgType_t eMsg )
{
	IMsgNetPacket *pNetPacket = NULL;

	if ( !BYldSendMessageAndGetReply( steamIDTarget, msgOut, nTimeoutSec, &pNetPacket ) )
		return false;

	pMsgIn->SetPacket( pNetPacket );

	if ( pMsgIn->Hdr().m_eMsg != eMsg )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message and waits for a response
// Input:	steamIDTarget - The entity this message is going to
//			msgOut - The message to send
//			nTimeoutSec - Number of seconds to wait for a response
//			ppNetPackets - Pointer to a IMsgNetPacket pointer which will contain
//				the response
// Returns:	True is the response was received, false otherwise. *ppNetPacket
//	will point to a valid packet only if the function returns true.
//-----------------------------------------------------------------------------
bool CGCJob::BYldSendMessageAndGetReply( CSteamID &steamIDTarget, CGCMsgBase &msgOut, uint nTimeoutSec, IMsgNetPacket **ppNetPacket )
{
	msgOut.ExpectingReply( GetJobID() );

	if ( !m_pGC->BSendGCMsgToClient( steamIDTarget, msgOut ) )
		return false;

	SetJobTimeout( nTimeoutSec );
	return BYieldingWaitForMsg( ppNetPacket );
}

//-----------------------------------------------------------------------------
// Purpose: BYldSendMessageAndGetReply, ProtoBuf edition
//-----------------------------------------------------------------------------
bool CGCJob::BYldSendMessageAndGetReply( CSteamID &steamIDTarget, CProtoBufMsgBase &msgOut, uint nTimeoutSec, CProtoBufMsgBase *pMsgIn, MsgType_t eMsg )
{
	IMsgNetPacket *pNetPacket = NULL;

	if ( !BYldSendMessageAndGetReply( steamIDTarget, msgOut, nTimeoutSec, &pNetPacket ) )
		return false;

	pMsgIn->InitFromPacket( pNetPacket );

	if ( pMsgIn->GetEMsg() != eMsg )
		return false;

	return true;
}

bool CGCJob::BYldSendMessageAndGetReply( CSteamID &steamIDTarget, CProtoBufMsgBase &msgOut, uint nTimeoutSec, IMsgNetPacket **ppNetPacket )
{
	msgOut.ExpectingReply( GetJobID() );

	if ( !m_pGC->BSendGCMsgToClient( steamIDTarget, msgOut ) )
		return false;

	SetJobTimeout( nTimeoutSec );
	return BYieldingWaitForMsg( ppNetPacket );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCWGJob::CGCWGJob( CGCBase *pGCBase ) 
: CGCJob( pGCBase ), 
	m_steamID( k_steamIDNil ),
	m_pWebApiFunc( NULL )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCWGJob::~CGCWGJob()
{

}


//-----------------------------------------------------------------------------
// Purpose: Receive a k_EMsgWGRequest and manage keyvalues serialization/deserialization
//-----------------------------------------------------------------------------
bool CGCWGJob::BYieldingRunGCJob( IMsgNetPacket * pNetPacket )
{
	CGCMsg<MsgGCWGRequest_t> msg( pNetPacket );

	CUtlString strRequestName;
	KeyValuesAD pkvRequest( "request" );
	KeyValuesAD pkvResponse( "response" );

	m_steamID = CSteamID( msg.Body().m_ulSteamID );

	msg.BReadStr( &strRequestName );

	//deserialize KV
	m_bufRequest.Clear();
	m_bufRequest.Put( msg.PubReadCur(), msg.Body().m_cubKeyValues );
	KVPacker packer;
	if ( !packer.ReadAsBinary( pkvRequest, m_bufRequest ) )
	{
		AssertMsg( false, "Failed to deserialize key values from WG request" );
		CGCWGJobMgr::SendErrorMessage( msg, "Failed to deserialize key values from WG request", k_EResultInvalidParam );
		return false;
	}

	if( !BVerifyParams( msg, pkvRequest, m_pWebApiFunc ) )
		return false;

	bool bRet = BYieldingRunJobFromRequest( pkvRequest, pkvResponse );


	// request failed, set success for wg
	if ( pkvResponse->IsEmpty( "success" ) )
	{
		pkvResponse->SetInt( "success", bRet ? k_EResultOK : k_EResultFail );
	}

	// send response msg
	CGCWGJobMgr::SendResponse( msg, pkvResponse, bRet );

	return true;
}

bool CGCWGJob::BVerifyParams( const CGCMsg<MsgGCWGRequest_t> & msg, KeyValues *pkvRequest, const WebApiFunc_t * pWebApiFunc )
{
	// we've found the function; now validate the call
	for ( int i = 0; i < Q_ARRAYSIZE( pWebApiFunc->m_rgParams ); i++ )
	{
		if ( !pWebApiFunc->m_rgParams[i].m_pchParam )
			break;

		// just simple validation for now; make sure the key exists
		if ( !pWebApiFunc->m_rgParams[i].m_bOptional && !pkvRequest->FindKey( pWebApiFunc->m_rgParams[i].m_pchParam ) )
		{
			CGCWGJobMgr::SendErrorMessage( msg,
				CFmtStr( "Error: Missing parameter '%s' from web request '%s'\n", pWebApiFunc->m_rgParams[i].m_pchParam, pWebApiFunc->m_pchRequestName ),
				k_EResultInvalidParam	);
			EmitWarning( SPEW_GC, 2, "Error: Missing parameter '%s' from web request '%s'\n", pWebApiFunc->m_rgParams[i].m_pchParam, pWebApiFunc->m_pchRequestName );
			return false;
		}
	}

	return true;
}


void CGCWGJob::SetErrorMessage( KeyValues *pkvErr, const char *pchErrorMsg, int32 nResult )
{
	CGCWGJobMgr::SetErrorMessage( pkvErr, pchErrorMsg, nResult );
}


//-----------------------------------------------------------------------------
// CGCJobVerifySession - A job that asks steam if a given user is still connected
//  and cleans up the session if the user is gone
//-----------------------------------------------------------------------------
bool CGCJobVerifySession::BYieldingRunGCJob()
{
 	if ( !m_pGC->BYieldingLockSteamID( m_steamID, __FILE__, __LINE__ ) )
		return false;

	m_pGC->YieldingRequestSession( m_steamID );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWebAPIJob::CWebAPIJob( CGCBase *pGC, EWebAPIOutputFormat eDefaultOutputFormat ) 
: CGCJob( pGC ), m_eDefaultOutputFormat( eDefaultOutputFormat )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWebAPIJob::~CWebAPIJob()
{
}

//-----------------------------------------------------------------------------
// Called to handle converting a web api response to a serialized result and free the object as well on a background thread

class CEmitWebAPIData
{
public:
	CEmitWebAPIData( CMsgHttpRequest* pRequest) : m_bResult( false ), m_Request( pRequest )	{}

	//inputs
	CHTTPRequest	m_Request;

	//outputs
	CHTTPResponse	m_Response;
	std::string		m_sSerializedResponse;
	bool			m_bResult;
};

//the worker thread function that is responsible for serializing the response from web api values to a text buffer, freeing the web api value tree, and serializing the message to a protobuf for
//direct sending. If this function fails (a false response value) the message will not be serialized
static void ThreadedEmitFormattedOutputWrapperAndFreeResponse( CWebAPIResponse *pResponse, CEmitWebAPIData* pEmitData, EWebAPIOutputFormat eDefaultFormat, size_t unMaxResultSize )
{
	// parse the output type that we want the result to be in
	EWebAPIOutputFormat eOutputFormat = eDefaultFormat; 
	const char *pszParamOutput = pEmitData->m_Request.GetGETParamString( "format", NULL ); 
	if ( !pszParamOutput ) 
		pszParamOutput = pEmitData->m_Request.GetPOSTParamString( "format", NULL ); 

	if  ( pszParamOutput ) 
	{ 
		if ( Q_stricmp( pszParamOutput, "xml" ) == 0 ) 
			eOutputFormat = k_EWebAPIOutputFormat_XML; 
		else if ( Q_stricmp( pszParamOutput, "vdf" ) == 0 ) 
			eOutputFormat = k_EWebAPIOutputFormat_VDF; 
		else if ( Q_stricmp( pszParamOutput, "json" ) == 0 ) 
			eOutputFormat = k_EWebAPIOutputFormat_JSON; 
	} 

	pEmitData->m_bResult = pResponse->BEmitFormattedOutput( eOutputFormat, *( pEmitData->m_Response.GetBodyBuffer() ), unMaxResultSize );
	delete pResponse;

	//update the response code on the output (can probably be done elsewhere), but must be done before we pack the message below
	switch( eOutputFormat ) 
	{ 
	case k_EWebAPIOutputFormat_JSON: 
		pEmitData->m_Response.SetResponseHeaderValue( "content-type",  "application/json; charset=UTF-8" ); 
		break; 
	case k_EWebAPIOutputFormat_XML: 
		pEmitData->m_Response.SetResponseHeaderValue( "content-type", "text/xml; charset=UTF-8" ); 
		break; 
	case k_EWebAPIOutputFormat_VDF: 
		pEmitData->m_Response.SetResponseHeaderValue( "content-type", "text/vdf; charset=UTF-8" ); 
		break; 
	default: 
		break; 
	}

	//if successful, we can go ahead and convert this all the way into a completely serialized form for sending over the wire
	if( pEmitData->m_bResult )
	{
		CMsgHttpResponse msgResponse;
		pEmitData->m_Response.SerializeIntoProtoBuf( msgResponse );
		msgResponse.SerializeToString( &pEmitData->m_sSerializedResponse );
	}
}

//called to respond to a web api request with the specified response value
static void WebAPIRespondToRequest( const char* pszName, uint32 nSenderIP, const CHTTPResponse& response, const CProtoBufMsg< CMsgWebAPIRequest >& msg )
{
	VPROF_BUDGET( "WebAPI - sending result", VPROF_BUDGETGROUP_STEAM );
	CProtoBufMsg<CMsgHttpResponse> msgResponse( k_EGCMsgWebAPIJobRequestHttpResponse, msg );
	response.SerializeIntoProtoBuf( msgResponse.Body() );
	GGCBase()->BReplyToMessage( msgResponse, msg );

	//track this message in the web API response
	g_WebAPIAccountTracker.TrackFunction( msg.Body().api_key().account_id(), nSenderIP, pszName, msgResponse.Body().body().size() );
}

//responses to the web api message with an error code
static void WebAPIRespondWithError( const char* pszName, uint32 nSenderIP, const CProtoBufMsg< CMsgWebAPIRequest >& msg, EHTTPStatusCode statusCode )
{
	CHTTPResponse response;
	response.SetStatusCode( statusCode );
	WebAPIRespondToRequest( pszName, nSenderIP, response, msg );
}

//parses an IP address out of the provided string. This will be the last IP address in the list
static uint32 ParseIPAddrFromForwardHeader( const char* pszHeader )
{
	//find the last comma in the string, our IP address follows that
	const char* pszStart = V_strrchr( pszHeader, ',' );
	//if no match, then we just have the ip address, otherwise advance past the comma
	if( !pszStart )
		pszStart = pszHeader;
	else
		pszStart++;

	//skip leading spaces
	while( V_isspace( *pszStart ) )
		pszStart++;

	return ntohl( inet_addr( pszStart ) );
}

//-----------------------------------------------------------------------------
// Purpose: Receive a k_EMsgWebAPIJobRequest and manage serialization/deserialization to 
// web request/response objects
//-----------------------------------------------------------------------------
bool CWebAPIJob::BYieldingRunJobFromMsg( IMsgNetPacket * pNetPacket )
{
	VPROF_BUDGET( "WebAPI", VPROF_BUDGETGROUP_STEAM );

	CProtoBufMsg<CMsgWebAPIRequest> msg( pNetPacket );

	//make sure all the required parameters were present
	bool bMsgParsedOK = msg.Body().has_api_key()
		&& msg.Body().has_interface_name()
		&& msg.Body().has_method_name()
		&& msg.Body().has_request()
		&& msg.Body().has_version();

	if( !bMsgParsedOK )
	{
		WebAPIRespondWithError( GetName(), 0, msg, k_EHTTPStatusCode400BadRequest );
		return true;
	}

	//determine the account that sent this request
	const AccountID_t nSenderAccountID = msg.Body().api_key().account_id();
	uint32 nSenderIP = 0;

	//if this isn't a system request, try and identify the IP address of the sender so we can rate limit accordingly
	if( nSenderAccountID != 0 )
	{
		const int nNumHeaders = msg.Body().request().headers_size();
		for( int nHeader = 0; nHeader < nNumHeaders; nHeader++ )
		{
			if( strcmp( msg.Body().request().headers( nHeader ).name().c_str(), "X-Forwarded-For" ) != 0 )
				continue;

			//this is our IP address
			nSenderIP = ParseIPAddrFromForwardHeader( msg.Body().request().headers( nHeader ).value().c_str() );			
		}

		//see if we have the kill switch turned on
		if( webapi_kill_switch.GetBool() )
		{
			if( webapi_kill_switch_error_response.GetBool() )
				WebAPIRespondWithError( GetName(), nSenderIP, msg, k_EHTTPStatusCode503ServiceUnavailable );
			return true;
		}
	}
	else
	{
// !FIXME! DOTAMERGE
//		//determine the priority of this steam request
//		uint32 nPriority;
//		nSenderIP = GetSteamRequestIPAddress( msg.Body().request(), nPriority );
//		//and allow for a kill switch based upon this priority
//		if( ( ( nPriority == CWebAPIAccountTracker::k_nSteamIP_Low ) && !webapi_enable_steam_low.GetBool() ) ||
//			( ( nPriority == CWebAPIAccountTracker::k_nSteamIP_Normal ) && !webapi_enable_steam_normal.GetBool() ) ||
//			( ( nPriority == CWebAPIAccountTracker::k_nSteamIP_High ) && !webapi_enable_steam_high.GetBool() ) )
//		{
//			if( webapi_kill_switch_error_response.GetBool() )
//				WebAPIRespondWithError( GetName(), 0, msg );
//			return true;
//		}
	}

	//track stats for this account, and handle rate limiting
	if( !g_WebAPIAccountTracker.TrackUser( nSenderAccountID, nSenderIP ) )
	{
		if( webapi_kill_switch_error_response.GetBool() )
			WebAPIRespondWithError( GetName(), nSenderIP, msg, k_EHTTPStatusCode429TooManyRequests );
		return true;
	}

	//allocate the data that we'll use to fill out the request and send it to the background thread for work
	CPlainAutoPtr< CEmitWebAPIData > pEmitData( new CEmitWebAPIData( const_cast< CMsgHttpRequest* >( &msg.Body().request() ) ) );
	CHTTPRequest& request = pEmitData->m_Request;
	CHTTPResponse& response = pEmitData->m_Response;

	{
		VPROF_BUDGET( "WebAPI - Prepare msg", VPROF_BUDGETGROUP_STEAM );

		m_webAPIKey.DeserializeFromProtoBuf( msg.Body().api_key() );
	}

	CPlainAutoPtr< CWebAPIResponse > pwebAPIResponse( new CWebAPIResponse() );

	{
		VPROF_BUDGET( "WebAPI - Process msg", VPROF_BUDGETGROUP_STEAM );
		if( !BYieldingRunJobFromAPIRequest( msg.Body().interface_name().c_str(), msg.Body().method_name().c_str(), msg.Body().version(), &request, &response, pwebAPIResponse.Get() ) )
		{
			//error executing our job
			WebAPIRespondWithError( GetName(), nSenderIP, msg, k_EHTTPStatusCode500InternalServerError );
			return false;
		}
	}

// !FIXME! DOTAMERGE
//	//see if they want to re-route this request
//	if( m_nRerouteRequest >= 0 )
//	{
//		if( ( uint32 )m_nRerouteRequest == m_pGC->GetGCDirIndex() )
//		{
//			AssertMsg( false, "Error: WebAPI %s attempting to re-route a web api message to itself (%d)", GetName(), m_nRerouteRequest );
//		}
//		else
//		{
//			//route to the other GC and discard this message
//			CProtoBufMsg< CMsgGCMsgWebAPIJobRequestForwardResponse > msgRoute( k_EGCMsgWebAPIJobRequestForwardResponse );
//			msgRoute.Body().set_dir_index( m_nRerouteRequest );
//			m_pGC->BReplyToMessage( msgRoute, msg );
//		}
//		return true;
//	}

	VPROF_BUDGET( "WebAPI - Emitting result", VPROF_BUDGETGROUP_STEAM );

	response.SetStatusCode( pwebAPIResponse->GetStatusCode() ); 
	response.SetExpirationHeaderDeltaFromNow( pwebAPIResponse->GetExpirationSeconds() ); 
	if( pwebAPIResponse->GetLastModified() )
		response.SetHeaderTimeValue( "last-modified", pwebAPIResponse->GetLastModified() );

	// if we aren't allowed to have a message body on this, simply send the result back now
	if( !CHTTPUtil::BStatusCodeAllowsBody( pwebAPIResponse->GetStatusCode() ) )
	{
		AssertMsg( pwebAPIResponse->GetRootValue() == NULL, "Response HTTP status code %d doesn't allow a body, but one was present", pwebAPIResponse->GetStatusCode() );
		//since we didn't have a body to serialize, we need to handle sending just the response
		WebAPIRespondToRequest( GetName(), nSenderIP, response, msg );
		return true;
	}

	//let the job convert the formatting and free our response for us (quite costly). Note that we detach the web API response since this job will free it
	bool bThreadFuncSucceeded = BYieldingWaitForThreadFunc( CreateFunctor( ThreadedEmitFormattedOutputWrapperAndFreeResponse, pwebAPIResponse.Detach(), pEmitData.Get(), m_eDefaultOutputFormat, (size_t)cv_webapi_result_size_limit.GetInt() ) );

	//if we called the function successfully and had a valid result, just send back the preserialized body
	if( bThreadFuncSucceeded && pEmitData->m_bResult )
	{
		m_pGC->BReplyToMessageWithPreSerializedBody( k_EGCMsgWebAPIJobRequestHttpResponse, msg, ( const byte* )pEmitData->m_sSerializedResponse.c_str(), pEmitData->m_sSerializedResponse.size() );
		g_WebAPIAccountTracker.TrackFunction( nSenderAccountID, nSenderIP, GetName(), pEmitData->m_sSerializedResponse.size() );
	}
	else
	{
		//we failed to generate a response, see if we ran out of space
		if( response.GetBodyBuffer()->TellMaxPut() > cv_webapi_result_size_limit.GetInt() ) 
		{
			// !FIXME! DOTAMERGE 
			//CGCAlertInfo alert( "WebAPIResponseSize", "WebAPI request %s failed to emit because it exceeded %d characters", request.GetURL(), cv_webapi_result_size_limit.GetInt() );
			//
			//switch( request.GetEHTTPMethod() )
			//{
			//case k_EHTTPMethodGET:
			//	{
			//		const uint32 nNumParams = request.GetGETParamCount();
			//		for( uint32 nParam = 0; nParam < nNumParams; nParam++ )
			//		{
			//			alert.AddExtendedInfoLine( "%s=%s", request.GetGETParamName( nParam ), request.GetGETParamValue( nParam ) );
			//		}
			//	}
			//	break;
			//case k_EHTTPMethodPOST:
			//	{
			//		const uint32 nNumParams = request.GetPOSTParamCount();
			//		for( uint32 nParam = 0; nParam < nNumParams; nParam++ )
			//		{
			//			alert.AddExtendedInfoLine( "%s=%s", request.GetPOSTParamName( nParam ), request.GetPOSTParamValue( nParam ) );
			//		}
			//	}
			//	break;
			//}
			//
			//GGCBase()->PostAlert( alert );
			GGCBase()->PostAlert( k_EAlertTypeInfo, false, CFmtStr( "WebAPI request %s failed to emit because it exceeded %d characters", request.GetURL(), cv_webapi_result_size_limit.GetInt() ) );
		}
		else
		{
			EG_WARNING( SPEW_GC, "WebAPI %s - request %s failed to emit for unknown reason\n", GetName(), request.GetURL() );
		}
		//make sure that they get an error code back
		WebAPIRespondWithError( GetName(), nSenderIP, msg, k_EHTTPStatusCode500InternalServerError );
	}

	return true;
}

void CWebAPIJob::AddLocalizedString( CWebAPIValues *pOutDefn, const char *pchFieldName, const char *pchKeyName, ELanguage eLang, bool bReturnTokenIfNotFound )
{
	// NULL keys we just skip
	if( !pchKeyName )
		return;

	const char *pchValue;
	if( eLang == k_Lang_None )
	{
		pchValue = pchKeyName;
	}
	else
	{
		pchValue = GGCBase()->LocalizeToken( pchKeyName, eLang, bReturnTokenIfNotFound );
	}

	if( pchValue )
	{
		pOutDefn->CreateChildObject( pchFieldName )->SetStringValue( pchValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose: A wrapper to call BEmitFormattedOutput and pass out a return value
//	since functors don't make return values available.
//-----------------------------------------------------------------------------
/*static*/ void CWebAPIJob::ThreadedEmitFormattedOutputWrapper( CWebAPIResponse *pResponse, EWebAPIOutputFormat eFormat, CUtlBuffer *poutputBuffer, size_t unMaxResultSize, bool *pbResult )
{
	*pbResult = pResponse->BEmitFormattedOutput( eFormat, *poutputBuffer, unMaxResultSize );
}


} // namespace GCSDK
