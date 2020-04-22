//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CGCSession class
//
//=============================================================================

#ifndef GCSESSION_H
#define GCSESSION_H
#ifdef _WIN32
#pragma once
#endif

#include "scheduledfunction.h"
#include "framefunction.h"
#include "gcsdk/gc_sharedobjectcache.h"

namespace GCSDK
{

class CGCGSSession;

// Spew group for anything related to sessions
extern CGCEmitGroup g_EGSessions;

//-----------------------------------------------------------------------------
// Utility class to handle rate limiting based upon a steam ID and message using two console variables to control rate
//-----------------------------------------------------------------------------

//utility class to handle rate limiting based upon a steam ID
class CSteamIDRateLimit
{
public:
	CSteamIDRateLimit( const GCConVar& cvNumPerPeriod, const GCConVar* pcvPeriodS = NULL );
	~CSteamIDRateLimit();
	//given a steam ID, this will determine if it should be rate limited
	bool BIsRateLimited( CSteamID steamID, uint32 unMsgType );
	//frame function to clear the list after a period of time
	bool OnFrameFn( const CLimitTimer& timer );
private:
	//the last time we cleared our list
	RTime32									m_LastClear;
	//the frame function so we can detect when we need to clear
	CFrameFunction< CSteamIDRateLimit>		m_FrameFunction;
	//the map of messages we have tracked for each user
	CUtlHashMapLarge< CSteamID, uint32 >	m_Msgs;
	//the console variables that track the time window and the messages allowed
	const GCConVar&							m_cvNumPerPeriod;
	const GCConVar*							m_pcvPeriodS;
};

//------------------------------------------------------------------------------------------
// CMsgRateLimitTracker
//   A utility class to track when messages go over so that we can see users/msgs that are being spammed
//------------------------------------------------------------------------------------------
class CMsgRateLimitTracker
{
public:

	CMsgRateLimitTracker();

	//called to track a message that was rate limited
	void TrackRateLimitedMsg( const CSteamID steamID, MsgType_t eMsgType );

	//called to report the collected rate limiting stats
	void ReportMsgStats() const;
	void ReportTopUsers( uint32 nMinMsgs, uint32 nListTop ) const;
	void ReportUserStats() const;

	//called to clear all collected stats
	void ClearStats();

private:

	//the time we started collecting stats at
	RTime32		m_StartTime;

	//map detailing the number of messages of each type that have been dropped
	CUtlHashMapLarge< MsgType_t, uint32 >		m_MsgStats;
	CUtlHashMapLarge< CSteamID, uint32 >		m_UserStats;
};
extern CMsgRateLimitTracker g_RateLimitTracker;

//-----------------------------------------------------------------------------
// Purpose: Base class for sessions in the GC
//-----------------------------------------------------------------------------
class CGCSession
{
public:
	CGCSession( const CSteamID & steamID, CGCSharedObjectCache *pCache );
	virtual ~CGCSession();

	const CSteamID & GetSteamID() const { return m_steamID; }

	const CGCSharedObjectCache *GetSOCache() const { return m_pSOCache; }
	CGCSharedObjectCache *GetSOCache() { return m_pSOCache; }
	void RemoveSOCache() { m_pSOCache = NULL; }

	EOSType GetOSType() const { return m_osType; };
	bool IsTestSession() const { return m_bIsTestSession; }
	uint32 GetIPPublic() const { return m_unIPPublic; }
	bool IsSecure() const { return m_bIsSecure; }

	bool BIsShuttingDown() const { return m_bIsShuttingDown; }
	void SetIsShuttingDown( bool bIsShuttingDown ) { m_bIsShuttingDown = bIsShuttingDown; }

	virtual void Dump( bool bFull = true ) const = 0;

	bool BRateLimitMessage( MsgType_t unMsgType );

	CJobTime GetLastPingSendTime() const { return m_jtTimeSentPing; }
	CJobTime GetLastMessageReceiveTime() const { return m_jtLastMessageReceived; }
	void SendPing() const;

	virtual void MarkAccess() { }
	virtual void Run();
	virtual void YieldingSOCacheReloaded() {}
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE

	// Geolocation
	bool HasGeoLocation() const { return m_haveGeoLocation; }
	bool GetGeoLocation( float &latitude, float &longittude ) const;
	virtual void SetGeoLocation( float latitude, float longittude );

	//track whether or not this session has been initialized or not
	bool	BIsInitialized() const			{ return m_bInitialized; }
	void	SetInitialized( bool b )		{ m_bInitialized = b; }

private:
	CSteamID m_steamID;
	CGCSharedObjectCache *m_pSOCache;

	// Tracks how many messages we've gotten this second so we can block attacks
	RTime32 m_rtLastMessageReceived;
	uint32 m_unMessagesRecievedThisSecond;
	CJobTime m_jtLastMessageReceived;

	// This is mutable because we update it when we send pings, but sending a
	// ping to a user/server isn't really a session changing event, so we don't
	// want to require locking the session to ping it and update its last
	// sent ping time.
	mutable CJobTime m_jtTimeSentPing;

	EOSType m_osType : 16;
	bool m_bIsShuttingDown : 1;
	bool m_bIsTestSession : 1;
	bool m_bIsSecure : 1;
	bool m_bInitialized : 1;
protected:
	bool m_haveGeoLocation : 1;

	float m_flLatitude;
	float m_flLongitude;

	uint32 m_unIPPublic;

	friend class CGCBase;
};


//-----------------------------------------------------------------------------
// Purpose: Base class for user sessions in the GC
//-----------------------------------------------------------------------------
class CGCUserSession : public CGCSession
{
public:
	CGCUserSession( const CSteamID & steamID, CGCSharedObjectCache *pCache ) : CGCSession( steamID, pCache ) { }
	virtual ~CGCUserSession();

	virtual bool BInit();

	const CSteamID &GetSteamIDGS() const { return m_steamIDGS; }
	const CSteamID &GetSteamIDGSPrev() const { return m_steamIDGSPrev; }

	virtual bool BSetServer( const CSteamID &steamIDGS );
	virtual bool BLeaveServer();
	virtual void Dump( bool bFull = true ) const;

private:
	CSteamID m_steamIDGS;
	CSteamID m_steamIDGSPrev;
};


//-----------------------------------------------------------------------------
// Purpose: Base class for gameserver sessions in the GC
//-----------------------------------------------------------------------------
class CGCGSSession : public CGCSession
{
public:

	CGCGSSession( const CSteamID & steamID, CGCSharedObjectCache *pCache, uint32 unServerAddr, uint16 usServerPort ) ;
	virtual ~CGCGSSession();

	uint32 GetAddr() const { return m_unServerAddr; }
	uint16 GetPort() const { return m_usServerPort; }
	void SetIPAndPort( uint32 unServerAddr, uint16 usServerPort );
	int GetUserCount() const { return m_vecUsers.Count(); }
	CSteamID GetUserID( int nIndex ) const { return m_vecUsers[nIndex]; }

	// Manages users on the server. It is very important that these are not
	// virtual and not yielding. For custom behavior override the Pre*() hooks below
	bool BAddUser( const CSteamID &steamIDUser );
	bool BRemoveUser( const CSteamID &steamIDUser );
	void RemoveAllUsers();

	virtual void Dump( bool bFull = true ) const;

protected:
	// Hooks to trigger custom behavior when users are added and removed. It is
	// very important that these do not yield. If you need to yield, start a job instead
	virtual void PreAddUser( const CSteamID &steamIDUser ) {}
	virtual void PostAddUser( const CSteamID &steamIDUser ) {}
	virtual void PreRemoveUser( const CSteamID &steamIDUser ) {}
	virtual void PostRemoveUser( const CSteamID &steamIDUser ) {}
	virtual void PreRemoveAllUsers() {}
	virtual void PostRemoveAllUsers() {}

public:
	float m_lastUpdateTime; // Last time we received a message from the server

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE
protected:
	CUtlVector<CSteamID> m_vecUsers;

	// These are the address of the server as connected to Steam
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
};

} // namespace GCSDK

#endif // GCSESSION_H
