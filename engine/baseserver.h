//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASESERVER_H
#define BASESERVER_H
#ifdef _WIN32
#pragma once
#endif

#include <iserver.h>
#include <netadr.h>
#include <bitbuf.h>
#include <utlvector.h>
#include "baseclient.h"
#include "netmessages.h"
#include "net.h"
#include "event_system.h"

class CNetworkStringTableContainer;
class PackedEntity;
class ServerClass;
class INetworkStringTable;	

enum server_state_t
{
	ss_dead = 0,	// Dead
	ss_loading,		// Spawning
	ss_active,		// Running
	ss_paused,		// Running, but paused
};

// See baseserver.cpp for #define which controls this
bool AllowDebugDedicatedServerOutsideSteam();

// time a challenge nonce is valid for, in seconds
#define CHALLENGE_NONCE_LIFETIME 6.0f

// MAX_DELTA_TICKS defines the maximum delta difference allowed 
// for delta compression, if clients request on older tick as
// delta baseline, send a full update. 
#define MAX_DELTA_TICKS	192		// this is about 3 seconds

typedef struct
{
	netadr_t    adr;       // Address where challenge value was sent to.
	int			challenge; // To connect, adr IP address must respond with this #
	float		time;      // # is valid for only a short duration.
} challenge_t;


class CBaseServer  : public IServer
{
public:
	CBaseServer();
	virtual ~CBaseServer();

	bool RestartOnLevelChange() { return m_bRestartOnLevelChange; }

public: // IServer implementation

	virtual int		GetNumClients( void ) const; // returns current number of clients
	virtual int		GetNumProxies( void ) const; // returns number of attached HLTV proxies
	virtual int		GetNumFakeClients() const; // returns number of fake clients/bots
	virtual int		GetMaxClients( void ) const { return m_nMaxclients; } // returns current client limit
	virtual int		GetUDPPort( void ) const { return NET_GetUDPPort( m_Socket );	}
	virtual IClient	*GetClient( int index ) { return m_Clients[index]; } // returns interface to client 
	virtual int		GetClientCount() const { return m_Clients.Count(); } // for iteration;
	virtual float	GetTime( void ) const;
	virtual int		GetTick( void ) const { return m_nTickCount; }
	virtual float	GetTickInterval( void ) const { return m_flTickInterval; }
	virtual const char *GetName( void ) const;
	virtual const char *GetMapName( void ) const { return m_szMapname; }
	virtual int		GetSpawnCount( void ) const { return m_nSpawnCount; }
	virtual int		GetNumClasses( void ) const { return serverclasses; }
	virtual int		GetClassBits( void ) const { return serverclassbits; }
	virtual void	GetNetStats( float &avgIn, float &avgOut );
	virtual int		GetNumPlayers();
	virtual	bool	GetPlayerInfo( int nClientIndex, player_info_t *pinfo );
	virtual float	GetCPUUsage( void ) { return m_fCPUPercent; }
		
	virtual bool	IsActive( void ) const { return m_State >= ss_active; }	
	virtual bool	IsLoading( void ) const { return m_State == ss_loading; }
	virtual bool	IsDedicated( void ) const { return m_bIsDedicated; }
	virtual bool	IsPaused( void ) const { return m_State == ss_paused; }
	virtual bool	IsMultiplayer( void ) const { return m_nMaxclients > 1; }
	virtual bool	IsPausable( void ) const { return false; }
	virtual bool	IsHLTV( void ) const { return false; }
	virtual bool	IsReplay( void ) const { return false; }

	virtual void	BroadcastMessage( INetMessage &msg, bool onlyActive = false, bool reliable = false );
	virtual void	BroadcastMessage( INetMessage &msg, IRecipientFilter &filter );
	virtual void	BroadcastPrintf ( PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 );

	virtual const char * GetPassword() const;

	virtual void	SetMaxClients( int number );
	virtual void	SetPaused(bool paused);
	virtual void	SetPassword(const char *password);

	virtual void	DisconnectClient(IClient *client, const char *reason );
	
	virtual void	WriteDeltaEntities( CBaseClient *client, CClientFrame *to, CClientFrame *from,	bf_write &pBuf );
	virtual void	WriteTempEntities( CBaseClient *client, CFrameSnapshot *to, CFrameSnapshot *from, bf_write &pBuf, int nMaxEnts );
	
public: // IConnectionlessPacketHandler implementation

	virtual bool	ProcessConnectionlessPacket( netpacket_t * packet );

	virtual void	Init( bool isDedicated );
	virtual void	Clear( void );
	virtual void	Shutdown( void );
	virtual CBaseClient *CreateFakeClient( const char *name );
	virtual void 	RemoveClientFromGame( CBaseClient *client ) {};
	virtual void	SendClientMessages ( bool bSendSnapshots );
	virtual void	FillServerInfo(SVC_ServerInfo &serverinfo);
	virtual void	UserInfoChanged( int nClientIndex );

	bool	GetClassBaseline( ServerClass *pClass, void const **pData, int *pDatalen );
	void	RunFrame( void );
	void	InactivateClients( void );
	void	ReconnectClients( void );
	void	CheckTimeouts (void);
	void	UpdateUserSettings(void);
	void	SendPendingServerInfo(void);

	const char	*CompressPackedEntity(ServerClass *pServerClass, const char *data, int &bits);
	const char	*UncompressPackedEntity(PackedEntity *pPackedEntity, int &size);

	INetworkStringTable *GetInstanceBaselineTable( void );
	INetworkStringTable *GetLightStyleTable( void );
	INetworkStringTable *GetUserInfoTable( void );

	virtual void	RejectConnection(const netadr_t &adr, int clientChallenge, const char *s );

	float	GetFinalTickTime( void ) const;

	virtual bool CheckIPRestrictions( const netadr_t &adr, int nAuthProtocol );

	void	SetMasterServerRulesDirty();
	void	SendQueryPortToClient( netadr_t &adr );

	void	RecalculateTags( void );
	void	AddTag( const char *pszTag );
	void	RemoveTag( const char *pszTag );

	int		GetNumConnections( ) { return m_nNumConnections; }

	void	SetReportNewFakeClients( bool bReportNewFakeClients ) { m_bReportNewFakeClients = bReportNewFakeClients; }

	void	SetPausedForced( bool bPaused, float flDuration = -1.f );

protected:

	virtual IClient *ConnectClient ( netadr_t &adr, int protocol, int challenge, int clientChallenge, int authProtocol, 
					    const char *name, const char *password, const char *hashedCDkey, int cdKeyLen );
	
	virtual CBaseClient *GetFreeClient( netadr_t &adr );

	virtual CBaseClient *CreateNewClient( int slot ) { AssertMsg( 0, "CBaseServer::CreateNewClient() being called - must be implemented in derived class!" ); return NULL; }; // must be derived

	
	virtual bool	FinishCertificateCheck( netadr_t &adr, int nAuthProtocol, const char *szRawCertificate, int clientChallenge ) { return true; };
	
	virtual int		GetChallengeNr ( netadr_t &adr );
	virtual int		GetChallengeType ( netadr_t &adr );

	virtual bool	CheckProtocol( netadr_t &adr, int nProtocol, int clientChallenge );
	virtual bool	CheckChallengeNr( netadr_t &adr, int nChallengeValue );
	virtual bool	CheckChallengeType( CBaseClient *client, int nNewUserID, netadr_t & adr, int nAuthProtocol, const char *pchLogonCookie, int cbCookie, int clientChallenge );
	virtual bool	CheckPassword( netadr_t &adr, const char *password, const char *name );
	virtual bool	CheckIPConnectionReuse( netadr_t &adr );

	virtual void	ReplyChallenge(netadr_t &adr, int clientChallenge);
	virtual void	ReplyServerChallenge(netadr_t &adr);

	virtual void	CalculateCPUUsage();

	// Keep the master server data updated.
	virtual bool	ShouldUpdateMasterServer();
	
	void			CheckMasterServerRequestRestart();
	void			UpdateMasterServer();
	void			UpdateMasterServerRules();
	virtual void	UpdateMasterServerPlayers() {}
	void			ForwardPacketsFromMasterServerUpdater();

	void SetRestartOnLevelChange(bool state)  { m_bRestartOnLevelChange = state; }

	bool RequireValidChallenge( netadr_t &adr );
	bool ValidChallenge( netadr_t & adr, int challengeNr );
	bool ValidInfoChallenge( netadr_t & adr, const char *nugget );

	// Data
public:

	server_state_t	m_State;		// some actions are only valid during load
	int				m_Socket;		// network socket 
	int				m_nTickCount;	// current server tick
	bool			m_bSimulatingTicks;		// whether or not the server is currently simulating ticks
	char			m_szMapname[64];		// map name
	char			m_szMapFilename[64];	// map filename, may bear no resemblance to map name
	char			m_szSkyname[64];		// skybox name
	char			m_Password[32];		// server password

	MD5Value_t		worldmapMD5;		// For detecting that client has a hacked local copy of map, the client will be dropped if this occurs.
	
	CNetworkStringTableContainer *m_StringTables;	// newtork string table container

	INetworkStringTable *m_pInstanceBaselineTable; 
	INetworkStringTable *m_pLightStyleTable;
	INetworkStringTable *m_pUserInfoTable;
	INetworkStringTable *m_pServerStartupTable;
	INetworkStringTable *m_pDownloadableFileTable;

	// This will get set to NET_MAX_PAYLOAD if the server is MP.
	bf_write			m_Signon;
	CUtlMemory<byte>	m_SignonBuffer;

	int			serverclasses;		// number of unique server classes
	int			serverclassbits;	// log2 of serverclasses


private:

	// Gets the next user ID mod SHRT_MAX and unique (not used by any active clients).
	int			GetNextUserID();
	int			m_nUserid;			// increases by one with every new client


protected:

	int			m_nMaxclients;         // Current max #
	int			m_nSpawnCount;			// Number of servers spawned since start,
									// used to check late spawns (e.g., when d/l'ing lots of
									// data)
	float		m_flTickInterval;		// time for 1 tick in seconds


	CUtlVector<CBaseClient*>	m_Clients;		// array of up to [maxclients] client slots.
	
	bool		m_bIsDedicated;

	uint32		m_CurrentRandomNonce;
	uint32		m_LastRandomNonce;
	float		m_flLastRandomNumberGenerationTime;
	float		m_fCPUPercent;
	float		m_fStartTime;
	float		m_fLastCPUCheckTime;

	// This is only used for Steam's master server updater to refer to this server uniquely.
	bool		m_bRestartOnLevelChange;
	
	bool		m_bMasterServerRulesDirty;
	double		m_flLastMasterServerUpdateTime;

	int			m_nNumConnections;		//Number of successful client connections.

	bool		m_bReportNewFakeClients; // Whether or not newly created fake clients should be included in server browser totals
	float		m_flPausedTimeEnd;
};

extern CThreadFastMutex g_svInstanceBaselineMutex;

#endif // BASESERVER_H
