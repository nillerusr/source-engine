//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASECLIENTSTATE_H
#define BASECLIENTSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include <inetmsghandler.h>
#include <protocol.h>
#include <client_class.h>
#include <cdll_int.h>
#include <netadr.h>
#include "common.h"
#include "clockdriftmgr.h"
#include "convar.h"
#include "cl_bounded_cvars.h"


 // Only send this many requests before timing out.
#define CL_CONNECTION_RETRIES		4 

// Mininum time gap (in seconds) before a subsequent connection request is sent.
#define CL_MIN_RESEND_TIME			1.5f   
// Max time.  The cvar cl_resend is bounded by these.
#define CL_MAX_RESEND_TIME			20.0f   

// In release, send commands at least this many times per second
#define MIN_CMD_RATE				10.0f
#define MAX_CMD_RATE				100.0f

extern ConVar cl_name;

// This represents a server's 
class C_ServerClassInfo
{
public:
				C_ServerClassInfo();
				~C_ServerClassInfo();

public:

	ClientClass	*m_pClientClass;
	char		*m_ClassName;
	char		*m_DatatableName;

	// This is an index into the network string table (cl.GetInstanceBaselineTable()).
	int			m_InstanceBaselineIndex; // INVALID_STRING_INDEX if not initialized yet.
};

#define EndGameAssertMsg( assertion, msg ) \
	if ( !(assertion) )\
		Host_EndGame msg


class CNetworkStringTableContainer;
class PackedEntity;
class INetworkStringTable;
class CEntityReadInfo;	


abstract_class CBaseClientState : public INetChannelHandler, public IConnectionlessPacketHandler, public IServerMessageHandler
{
	
public:
	CBaseClientState();
	virtual ~CBaseClientState();

public: // IConnectionlessPacketHandler interface:
		
	virtual bool ProcessConnectionlessPacket(struct netpacket_s *packet);

public: // INetMsgHandler interface:
		
	virtual void ConnectionStart(INetChannel *chan);
	virtual void ConnectionClosing( const char *reason );
	virtual void ConnectionCrashed(const char *reason);

	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) {};
	virtual void PacketEnd( void ) {};

	virtual void FileReceived( const char *fileName, unsigned int transferID );
	virtual void FileRequested( const char *fileName, unsigned int transferID );
	virtual void FileDenied( const char *fileName, unsigned int transferID );
	virtual void FileSent( const char *fileName, unsigned int transferID );

public: // IServerMessageHandlers
	
	PROCESS_NET_MESSAGE( Tick );
	PROCESS_NET_MESSAGE( StringCmd );
	PROCESS_NET_MESSAGE( SetConVar );
	PROCESS_NET_MESSAGE( SignonState );

	PROCESS_SVC_MESSAGE( Print );
	PROCESS_SVC_MESSAGE( ServerInfo );
	PROCESS_SVC_MESSAGE( SendTable );
	PROCESS_SVC_MESSAGE( ClassInfo );
	PROCESS_SVC_MESSAGE( SetPause );
	PROCESS_SVC_MESSAGE( CreateStringTable );
	PROCESS_SVC_MESSAGE( UpdateStringTable );
	PROCESS_SVC_MESSAGE( SetView );
	PROCESS_SVC_MESSAGE( PacketEntities );
	PROCESS_SVC_MESSAGE( Menu );
	PROCESS_SVC_MESSAGE( GameEventList );
	PROCESS_SVC_MESSAGE( GetCvarValue );
	PROCESS_SVC_MESSAGE( CmdKeyValues );
	PROCESS_SVC_MESSAGE( SetPauseTimed );

	// Returns dem file protocol version, or, if not playing a demo, just returns PROTOCOL_VERSION
	virtual int GetDemoProtocolVersion() const;

public: 
	inline	bool IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	inline	bool IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };
	virtual	void Clear( void );
	virtual void FullConnect( netadr_t &adr ); // a connection was established
	virtual void Connect(const char* adr, const char *pszSourceTag); // start a connection challenge
	virtual bool SetSignonState ( int state, int count );
	virtual void Disconnect( const char *pszReason, bool bShowMainMenu );
	virtual void SendConnectPacket (int challengeNr, int authProtocol, uint64 unGSSteamID, bool bGSSecure );
	virtual const char *GetCDKeyHash() { return "123"; }
	virtual void RunFrame ( void );
	virtual void CheckForResend ( void );
	virtual void InstallStringTableCallback( char const *tableName ) { }
	virtual bool HookClientStringTable( char const *tableName ) { return false; }
	virtual bool LinkClasses( void );
	virtual int  GetConnectionRetryNumber() const { return CL_CONNECTION_RETRIES; }
	virtual const char *GetClientName() { return cl_name.GetString(); }
	
	static ClientClass* FindClientClass(const char *pClassName);

	CClockDriftMgr& GetClockDriftMgr();
	
	int GetClientTickCount() const;	// Get the client tick count.
	void SetClientTickCount( int tick );

	int GetServerTickCount() const;
	void SetServerTickCount( int tick );

	void SetClientAndServerTickCount( int tick );

	INetworkStringTable *GetStringTable( const char * name ) const;
	
	PackedEntity *GetEntityBaseline( int iBaseline, int nEntityIndex );
	void SetEntityBaseline(int iBaseline, ClientClass *pClientClass, int index, char *packedData, int length);
	void CopyEntityBaseline( int iFrom, int iTo );
	void FreeEntityBaselines();
	bool GetClassBaseline( int iClass, void const **pData, int *pDatalen );
	ClientClass *GetClientClass( int i );

	void ForceFullUpdate( void );
	void SendStringCmd(const char * command);
	
	void ReadPacketEntities( CEntityReadInfo &u );

	virtual void ReadEnterPVS( CEntityReadInfo &u ) = 0;
	virtual void ReadLeavePVS( CEntityReadInfo &u ) = 0;
	virtual void ReadDeltaEnt( CEntityReadInfo &u ) = 0;
	virtual void ReadPreserveEnt( CEntityReadInfo &u ) = 0;
	virtual void ReadDeletions( CEntityReadInfo &u ) = 0;

	bool IsClientConnectionViaMatchMaking( void );

	static bool ConnectMethodAllowsRedirects( void );

private:
	bool PrepareSteamConnectResponse( uint64 unGSSteamID, bool bGSSecure, const netadr_t &adr, bf_write &msg );

public:
	// Connection to server.			
	int				m_Socket;		// network socket 
	INetChannel		*m_NetChannel;		// Our sequenced channel to the remote server.
	unsigned int	m_nChallengeNr;	// connection challenge number
	double			m_flConnectTime;	// If gap of connect_time to net_time > 3000, then resend connect packet
	int				m_nRetryNumber;	// number of retry connection attemps
	char			m_szRetryAddress[ MAX_OSPATH ];
	CUtlString		m_sRetrySourceTag; // string that describes why we decided to connect to this server (empty for command line, "serverbrowser", "quickplay", etc)
	int				m_retryChallenge; // challenge we sent to the server
	int				m_nSignonState;    // see SIGNONSTATE_* definitions
	double			m_flNextCmdTime; // When can we send the next command packet?
	int				m_nServerCount;	// server identification for prespawns, must match the svs.spawncount which
									// is incremented on server spawning.  This supercedes svs.spawn_issued, in that
									// we can now spend a fair amount of time sitting connected to the server
									// but downloading models, sounds, etc.  So much time that it is possible that the
									// server might change levels again and, if so, we need to know that.
	uint64			m_ulGameServerSteamID; // Steam ID of the game server we are trying to connect to, or are connected to.  Zero if unknown
	int			m_nCurrentSequence;	// this is the sequence number of the current incoming packet	

	CClockDriftMgr m_ClockDriftMgr;

	int			m_nDeltaTick;		//	last valid received snapshot (server) tick
	bool		m_bPaused;			// send over by server
	float		m_flPausedExpireTime;
	int			m_nViewEntity;		// cl_entitites[cl.viewentity] == player point of view

	int			m_nPlayerSlot;		// own player entity index-1. skips world. Add 1 to get cl_entitites index;

	char		m_szLevelFileName[ 128 ];	// for display on solo scoreboard
	char		m_szLevelBaseName[ 128 ]; // removes maps/ and .bsp extension

	int			m_nMaxClients;		// max clients on server

	PackedEntity	*m_pEntityBaselines[2][MAX_EDICTS];	// storing entity baselines
		
	// This stuff manages the receiving of data tables and instantiating of client versions
	// of server-side classes.
	C_ServerClassInfo	*m_pServerClasses;
	int					m_nServerClasses;
	int					m_nServerClassBits;
	char				m_szEncrytionKey[STEAM_KEYSIZE];
	unsigned int		m_iEncryptionKeySize;

	CNetworkStringTableContainer *m_StringTableContainer;
	
	bool m_bRestrictServerCommands;	// If true, then the server is only allowed to execute commands marked with FCVAR_SERVER_CAN_EXECUTE on the client.
	bool m_bRestrictClientCommands;	// If true, then IVEngineClient::ClientCmd is only allowed to execute commands marked with FCVAR_CLIENTCMD_CAN_EXECUTE on the client.
};


inline CClockDriftMgr& CBaseClientState::GetClockDriftMgr()
{
	return m_ClockDriftMgr;
}


inline void CBaseClientState::SetClientTickCount( int tick )
{
	m_ClockDriftMgr.m_nClientTick = tick;
}

inline int CBaseClientState::GetClientTickCount() const
{
	return m_ClockDriftMgr.m_nClientTick;
}

inline int CBaseClientState::GetServerTickCount() const
{
	return m_ClockDriftMgr.m_nServerTick;
}

inline void CBaseClientState::SetServerTickCount( int tick )
{
	m_ClockDriftMgr.m_nServerTick = tick;
}

inline void CBaseClientState::SetClientAndServerTickCount( int tick )
{
	m_ClockDriftMgr.m_nServerTick = m_ClockDriftMgr.m_nClientTick = tick;
}


#endif // BASECLIENTSTATE_H
