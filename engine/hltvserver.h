//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVSERVER_H
#define HLTVSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseserver.h"
#include "hltvclient.h"
#include "hltvdemo.h"
#include "hltvclientstate.h"
#include "clientframe.h"
#include "networkstringtable.h"
#include <ihltv.h>
#include <convar.h>

#define HLTV_BUFFER_DIRECTOR		0	// director commands
#define	HLTV_BUFFER_RELIABLE		1	// reliable messages
#define HLTV_BUFFER_UNRELIABLE		2	// unreliable messages
#define HLTV_BUFFER_VOICE			3	// player voice data
#define HLTV_BUFFER_SOUNDS			4	// unreliable sounds
#define HLTV_BUFFER_TEMPENTS		5	// temporary/event entities
#define HLTV_BUFFER_MAX				6	// end marker

// proxy dispatch modes
#define DISPATCH_MODE_OFF			0
#define DISPATCH_MODE_AUTO			1
#define DISPATCH_MODE_ALWAYS		2

extern ConVar tv_debug;

class CHLTVFrame : public CClientFrame
{
public:
	CHLTVFrame();
	virtual ~CHLTVFrame();

	void	Reset(); // resets all data & buffers
	void	FreeBuffers();
	void	AllocBuffers();
	bool	HasData();
	void	CopyHLTVData( CHLTVFrame &frame );
	virtual bool IsMemPoolAllocated() { return false; }

public:

	// message buffers:
	bf_write	m_Messages[HLTV_BUFFER_MAX];
};

struct CFrameCacheEntry_s
{
	CClientFrame* pFrame;
	int	nTick;
};

class CDeltaEntityCache
{
	struct DeltaEntityEntry_s
	{
		DeltaEntityEntry_s *pNext;
		int	nDeltaTick;
		int nBits;
	};

public:
	CDeltaEntityCache();
	~CDeltaEntityCache();

	void SetTick( int nTick, int nMaxEntities );
	unsigned char* FindDeltaBits( int nEntityIndex, int nDeltaTick, int &nBits );
	void AddDeltaBits( int nEntityIndex, int nDeltaTick, int nBits, bf_write *pBuffer );
	void Flush();

protected:
	int	m_nTick;	// current tick
	int	m_nMaxEntities;	// max entities = length of cache
	int m_nCacheSize;
	DeltaEntityEntry_s* m_Cache[MAX_EDICTS]; // array of pointers to delta entries
};


class CGameClient;
class CGameServer;
class IHLTVDirector;

class CHLTVServer : public IGameEventListener2, public CBaseServer, public CClientFrameManager, public IHLTVServer, public IDemoPlayer
{
friend class CHLTVClientState;

public:
	CHLTVServer();
	virtual ~CHLTVServer();

public: // CBaseServer interface:
	bool	ProcessConnectionlessPacket( netpacket_t * packet );

	void	Init (bool bIsDedicated);
	void	Shutdown( void );
	void	Clear( void );
	bool	IsHLTV( void ) const { return true; };
	bool	IsMultiplayer( void ) const { return true; };
	void	FillServerInfo(SVC_ServerInfo &serverinfo);
	void	GetNetStats( float &avgIn, float &avgOut );
	int		GetChallengeType ( netadr_t &adr );
	const char *GetName( void ) const;
	const char *GetPassword() const;
	IClient *ConnectClient ( netadr_t &adr, int protocol, int challenge, int clientChallenge, int authProtocol, 
		const char *name, const char *password, const char *hashedCDkey, int cdKeyLen );

public:
	void	FireGameEvent(IGameEvent *event);

public: // IHLTVServer interface:
	IServer	*GetBaseServer( void );
	IHLTVDirector *GetDirector( void );
	int		GetHLTVSlot( void ); // return entity index-1 of HLTV in game
	float	GetOnlineTime( void ); // seconds since broadcast started
	void	GetLocalStats( int &proxies, int &slots, int &clients ); 
	void	GetGlobalStats( int &proxies, int &slots, int &clients );
	void	GetRelayStats( int &proxies, int &slots, int &clients );

	bool	IsMasterProxy( void ); // true, if this is the HLTV master proxy
	bool	IsTVRelay();			// true if we're running a relay (i.e. this is the opposite of IsMasterProxy()).
	bool	IsDemoPlayback( void ); // true if this is a HLTV demo

	const netadr_t *GetRelayAddress( void ); // returns relay address

	void	BroadcastEvent(IGameEvent *event);

public: // IDemoPlayer interface
	CDemoFile *GetDemoFile();
	int		GetPlaybackStartTick( void );
	int		GetPlaybackTick( void );
	int		GetTotalTicks( void );
	
	bool	StartPlayback( const char *filename, bool bAsTimeDemo );

	bool	IsPlayingBack( void ); // true if demo loaded and playing back
	bool	IsPlaybackPaused( void ); // true if playback paused
	bool	IsPlayingTimeDemo( void ) { return false; } // true if playing back in timedemo mode
	bool	IsSkipping( void ) { return false; }; // true, if demo player skiiping trough packets
	bool	CanSkipBackwards( void ) { return true; } // true if demoplayer can skip backwards

	void	SetPlaybackTimeScale( float timescale ); // sets playback timescale
	float	GetPlaybackTimeScale( void ); // get playback timescale

	void	PausePlayback( float seconds ) {}
	void	SkipToTick( int tick, bool bRelative, bool bPause ) {}
	void	SetEndTick( int tick) {}
	void	ResumePlayback( void ) {}
	void	StopPlayback( void ) {}
	void	InterpolateViewpoint() {}
	netpacket_t *ReadPacket( void ) { return NULL; }

	void	ResetDemoInterpolation( void ) {}
	int		GetProtocolVersion();

	bool	ShouldLoopDemos() { return true; }
	void	OnLastDemoInLoopPlayed() {}

	bool	IsLoading( void ) { return false; } // true if demo is currently loading

public:
	void	StartMaster(CGameClient *client); // start HLTV server as master proxy
	void	ConnectRelay(const char *address); // connect to other HLTV proxy
	void	StartDemo(const char *filename); // starts playing back a demo file
	void    StartRelay( void ); // start HLTV server as relay proxy
	bool	SendNetMsg( INetMessage &msg, bool bForceReliable = false );
	void	RunFrame();
	void	SetMaxClients( int number );
	void	Changelevel( void );
	
	void	UserInfoChanged( int nClientIndex );
	void	SendClientMessages ( bool bSendSnapshots );
	CClientFrame *AddNewFrame( CClientFrame * pFrame ); // add new frame, returns HLTV's copy
	void	SignonComplete( void );
	void	LinkInstanceBaselines( void );
	void	BroadcastEventLocal( IGameEvent *event, bool bReliable ); // broadcast event but not to relay proxies
	void	BroadcastLocalChat( const char *pszChat, const char *pszGroup ); // broadcast event but not to relay proxies
	void	BroadcastLocalTitle( CHLTVClient *client = NULL ); // NULL = broadcast to all
	bool	DispatchToRelay( CHLTVClient *pClient);
	bf_write *GetBuffer( int nBuffer);
	CClientFrame *GetDeltaFrame( int nTick );
		
	inline  CHLTVClient* Client( int i ) { return static_cast<CHLTVClient*>(m_Clients[i]); }


protected:
	virtual bool ShouldUpdateMasterServer();
	
private:
	CBaseClient *CreateNewClient( int slot );
	void		UpdateTick( void );
	void		UpdateStats( void );
	void		InstallStringTables( void );
	void		RestoreTick( int tick );
	void		EntityPVSCheck( CClientFrame *pFrame );
	void		InitClientRecvTables();
	void		FreeClientRecvTables();
	void		ReadCompleteDemoFile();
	void		ResyncDemoClock();

#ifndef NO_STEAM
	void		ReplyInfo( const netadr_t &adr );
#endif
		

	// Vector		GetOriginFromPackedEntity(PackedEntity* pe);

public:

	CGameClient		*m_MasterClient;		// if != NULL, this is the master HLTV 
	CHLTVClientState m_ClientState;
	CHLTVDemoRecorder m_DemoRecorder;			// HLTV demo object for recording and playback
	CGameServer		*m_Server;		// pointer to source server (sv.)
	IHLTVDirector	*m_Director;	// HTLV director exported by game.dll	
	int				m_nFirstTick;	// first known server tick;
	int				m_nLastTick;	// last tick from AddFrame()
	CHLTVFrame		*m_CurrentFrame; // current delayed HLTV frame
	int				m_nViewEntity;	// the current entity HLTV is tracking
	int				m_nPlayerSlot;	// slot of HLTV client on game server
	CHLTVFrame		m_HLTVFrame;	// all incoming messages go here until Snapshot is made

	bool			m_bSignonState;	// true if connecting to server
	float			m_flStartTime;
	float			m_flFPS;		// FPS the proxy is running;
	int				m_nGameServerMaxClients; // max clients on game server
	float			m_fNextSendUpdateTime;	// time to send next HLTV status messages 
	RecvTable		*m_pRecvTables[MAX_DATATABLES];
	int				m_nRecvTables;
	Vector			m_vPVSOrigin; 
	bool			m_bMasterOnlyMode;

	netadr_t		m_RootServer;		// HLTV root server
	int				m_nGlobalSlots;
	int				m_nGlobalClients;
	int				m_nGlobalProxies;

	CNetworkStringTableContainer m_NetworkStringTables;

	CDeltaEntityCache				m_DeltaCache;
	CUtlVector<CFrameCacheEntry_s>	m_FrameCache;

	// demoplayer stuff:
	CDemoFile		m_DemoFile;		// for demo playback
	int				m_nStartTick;
	democmdinfo_t	m_LastCmdInfo;
	bool			m_bPlayingBack;
	bool			m_bPlaybackPaused; // true if demo is paused right now
	float			m_flPlaybackRateModifier;
	int				m_nSkipToTick;	// skip to tick ASAP, -1 = off
};

extern CHLTVServer *hltv;	// The global HLTV server/object. NULL on xbox.

#endif // HLTVSERVER_H
