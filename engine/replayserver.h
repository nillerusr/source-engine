//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef REPLAYSERVER_H
#define REPLAYSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseserver.h"
#include "replaydemo.h"
#include "clientframe.h"
#include "networkstringtable.h"
#include "dt_recv.h"
#include "replay/ireplayserver.h"
#include <convar.h>

#define REPLAY_BUFFER_DIRECTOR		0	// director commands
#define	REPLAY_BUFFER_RELIABLE		1	// reliable messages
#define REPLAY_BUFFER_UNRELIABLE	2	// unreliable messages
#define REPLAY_BUFFER_VOICE			3	// player voice data
#define REPLAY_BUFFER_SOUNDS		4	// unreliable sounds
#define REPLAY_BUFFER_TEMPENTS		5	// temporary/event entities
#define REPLAY_BUFFER_MAX			6	// end marker

// proxy dispatch modes
#define DISPATCH_MODE_OFF			0
#define DISPATCH_MODE_AUTO			1
#define DISPATCH_MODE_ALWAYS		2

class CReplayFrame : public CClientFrame
{
public:
	CReplayFrame();
	virtual ~CReplayFrame();

	void	Reset(); // resets all data & buffers
	void	FreeBuffers();
	void	AllocBuffers();
	bool	HasData();
	void	CopyReplayData( CReplayFrame &frame );
	virtual bool IsMemPoolAllocated() { return false; }

public:

	// message buffers:
	bf_write	m_Messages[REPLAY_BUFFER_MAX];
};

struct CReplayFrameCacheEntry_s
{
	CClientFrame* pFrame;
	int	nTick;
};

class CReplayDeltaEntityCache
{
	struct DeltaEntityEntry_s
	{
		DeltaEntityEntry_s *pNext;
		int	nDeltaTick;
		int nBits;
	};

public:
	CReplayDeltaEntityCache();
	~CReplayDeltaEntityCache();

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
class IReplayDirector;
class IServerReplayContext;

class CReplayServer : public IGameEventListener2,
					  public CBaseServer,
					  public CClientFrameManager,
					  public IReplayServer
{
	typedef CBaseServer BaseClass;

public:
	CReplayServer();
	virtual ~CReplayServer();

public: // CBaseServer interface:
	virtual bool	IsMultiplayer() const { return true; };
	virtual bool	IsReplay() const { return true; };
	virtual void	Init( bool bIsDedicated );
	virtual void	Clear();
	virtual void	Shutdown();
	virtual void	FillServerInfo(SVC_ServerInfo &serverinfo);
	virtual void	GetNetStats( float &avgIn, float &avgOut );
	virtual int		GetChallengeType ( netadr_t &adr );
	virtual const char *GetName() const;
	virtual const char *GetPassword() const;
	IClient *ConnectClient ( netadr_t &adr, int protocol, int challenge, int clientChallenge, int authProtocol, 
		const char *name, const char *password, const char *hashedCDkey, int cdKeyLen );

	void ReplyChallenge(netadr_t &adr, int clientChallenge );
	void ReplyServerChallenge(netadr_t &adr);
	void RejectConnection( const netadr_t &adr, int clientChallenge, const char *s );
	CBaseClient *CreateFakeClient(const char *name);

public: // IGameEventListener2 interface:
	void	FireGameEvent( IGameEvent *event );
	int		m_nDebugID;
	int		GetEventDebugID();

public: // IReplayServer interface:
	virtual IServer	*GetBaseServer();
	virtual IReplayDirector *GetDirector() { return NULL; }
	virtual int		GetReplaySlot(); // return entity index-1 of Replay in game
	virtual float	GetOnlineTime(); // seconds since broadcast started
	virtual void	BroadcastEvent( IGameEvent *event ) { }
	virtual bool	IsRecording()	{ return m_DemoRecorder.IsRecording(); }
	virtual void	StartRecording();
	virtual void	StopRecording();

public: // CBaseServer overrides:
	virtual void	SetMaxClients( int number );
	virtual void	UserInfoChanged( int nClientIndex );

public:
	void	StartMaster(CGameClient *client); // start Replay server as master proxy
	bool	SendNetMsg( INetMessage &msg, bool bForceReliable = false );
	void	RunFrame();
	void	Changelevel();
	CClientFrame *AddNewFrame( CClientFrame * pFrame ); // add new frame, returns Replay's copy
	void	LinkInstanceBaselines();
	bf_write *GetBuffer( int nBuffer);
	CClientFrame *GetDeltaFrame( int nTick );

protected:
	virtual bool ShouldUpdateMasterServer();
	
private:
	void		UpdateTick();
	void		InstallStringTables();
	void		RestoreTick( int tick );
	void		EntityPVSCheck( CClientFrame *pFrame );
	void		InitClientRecvTables();
	void		FreeClientRecvTables();
	void		ResyncDemoClock();
		
public:
	CGameClient		*m_MasterClient;		// if != NULL, this is the master Replay 
	CReplayDemoRecorder m_DemoRecorder;			// Replay demo object for recording and playback
	CGameServer		*m_Server;		// pointer to source server (sv.)
	int				m_nFirstTick;	// first known server tick;
	int				m_nLastTick;	// last tick from AddFrame()
	CReplayFrame	*m_CurrentFrame; // current delayed Replay frame
	int				m_nViewEntity;	// the current entity Replay is tracking
	int				m_nPlayerSlot;	// slot of Replay client on game server
	CReplayFrame	m_ReplayFrame;	// all incoming messages go here until Snapshot is made

	bool			m_bSignonState;	// true if connecting to server
	float			m_flStartTime;
	float			m_flFPS;		// FPS the proxy is running;
	int				m_nGameServerMaxClients; // max clients on game server
	float			m_fNextSendUpdateTime;	// time to send next Replay status messages 
	RecvTable		*m_pRecvTables[MAX_DATATABLES];
	int				m_nRecvTables;
	Vector			m_vPVSOrigin; 
	bool			m_bMasterOnlyMode;

	netadr_t		m_RootServer;		// Replay root server
	int				m_nGlobalSlots;
	int				m_nGlobalClients;
	int				m_nGlobalProxies;

	CNetworkStringTableContainer m_NetworkStringTables;

	CReplayDeltaEntityCache					m_DeltaCache;
	CUtlVector<CReplayFrameCacheEntry_s>	m_FrameCache;

private:
	void			SendPendingEvents();	// Send events to clients regarding start/stop/replays available

	float			m_flStartRecordTime;
	float			m_flStopRecordTime;
};

extern CReplayServer *replay;	// The global Replay server/object. NULL on xbox.

#endif // REPLAYSERVER_H
