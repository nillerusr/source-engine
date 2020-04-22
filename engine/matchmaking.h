//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#ifndef MATCHMAKING_H
#define MATCHMAKING_H
#ifdef _WIN32
#pragma once
#endif

#ifdef _WIN32
#include "winerror.h"
#endif
#include "utlmap.h"
#include "inetmsghandler.h"
#include "netmessages.h"
#include "Session.h"
#include "engine/imatchmaking.h"

enum MMPACKETS
{
	PTH_CONNECT,
	PTH_SYSTEMLINK_SEARCH,
	HTP_SYSTEMLINK_REPLY
};

enum MMSTATE
{
	MMSTATE_INITIAL,
	MMSTATE_IDLE,
	MMSTATE_CREATING,
	MMSTATE_MODIFYING,
	MMSTATE_ACCEPTING_CONNECTIONS,
	MMSTATE_SEARCHING,
	MMSTATE_WAITING_QOS,
	MMSTATE_BROWSING,
	MMSTATE_SESSION_CONNECTING,
	MMSTATE_SESSION_CONNECTED,
	MMSTATE_SESSION_DISCONNECTING,

	MMSTATE_HOSTMIGRATE_STARTINGMIGRATION,
	MMSTATE_HOSTMIGRATE_MIGRATING,
	MMSTATE_HOSTMIGRATE_WAITINGFORCLIENTS,
	MMSTATE_HOSTMIGRATE_WAITINGFORHOST,

	MMSTATE_GAME_LOCKED,	// in ranked games, clients can't join after this point

	MMSTATE_PREGAME,
	MMSTATE_REPORTING_STATS,
	MMSTATE_POSTGAME,

	MMSTATE_GAME_ACTIVE,	// clients are no longer in the lobby

	MMSTATE_LOADING,
	MMSTATE_CONNECTED_TO_SERVER,
	MMSTATE_INGAME,
};

// For sending host data back in search results
enum eGameState
{
	GAMESTATE_INLOBBY,
	GAMESTATE_INPROGRESS
};

#define HEARTBEAT_INTERVAL_LONG		1.0		// send a heartbeat every second during gameplay
#define HEARTBEAT_INTERVAL_SHORT	0.1		// send a heartbeat ten times a second in the lobby
#define HEARTBEAT_TIMEOUT			10.0	// time out if a heartbeat isn't recieved for ten seconds 
#if defined( _DEBUG )
#define HEARTBEAT_TIMEOUT_LOADING	300		// in debug loads take much longer
#else
#define HEARTBEAT_TIMEOUT_LOADING	100		// allow for longer communication gaps during map load
#endif

#define STARTGAME_COUNTDOWN			15.0	// start game countdown timer
#define DISCONNECT_WAITTIME			1.0		// wait for the server to reply to our disconnect notification
#define QOSLOOKUP_WAITTIME			20.0	// wait to get quality of service data about session hosts
#define JOINREPLY_WAITTIME			15.0	// time to wait for the host to reply to our join request
#define REPORTSTATS_WAITTIME		20.0	// time to wait for clients to report their stats to live

// 360 TCR's require system link searches complete in less than 3 seconds
#define SYSTEMLINK_RETRYINTERVAL	1.f		// in seconds
#define SYSTEMLINK_MAXRETRIES		3		// number of tries before giving up

#define SESSIONMODIRY_MAXWAITTIME	10		// max time for clients to update their session properties
#define REGISTRATION_MAXWAITTIME	10		// max time for clients to register

#define HOSTMIGRATION_RETRYINTERVAL	1.0		// in seconds
#define HOSTMIGRATION_MAXRETRIES	10		// max migrate sends to clients
#define HOSTMIGRATION_MAXWAITTIME	10		// time to wait for a new host to contact us

#define MAX_SEARCHRESULTS			20		// Maximum number of results when searching for a session.

#define PING_MAX_GREEN				70
#define PING_MAX_YELLOW				140
#define PING_MAX_RED				250

#define VOICE_STATUS_OFF			0
#define VOICE_STATUS_IDLE			1
#define VOICE_STATUS_TALKING		2

// HACK: For simplicity, we know TF has two teams plus spectator.
#define MAX_TEAMS		3
#define MAX_PLAYERS		16

#define VOICE_ICON_BLINK_TIME 0.5

class CMatchmaking : public IMatchmaking, public IMatchmakingMessageHandler, public IClientMessageHandler, public INetChannelHandler, public IConnectionlessPacketHandler
{
public:
	CMatchmaking();
	~CMatchmaking();

	// IMatchmaking implementation
	virtual void	SessionNotification( const SESSION_NOTIFY notification, const int param = 0 );
	virtual void	AddSessionProperty( const uint nType, const char *pID, const char *pValue, const char *pValueType );
	virtual void	SetSessionProperties( KeyValues *pPropertyKeys );
	virtual void	SelectSession( uint sessionIdx );
	virtual void	ModifySession();
	virtual void	UpdateMuteList();
	virtual void	StartHost( bool bSystemLink = false );
	virtual void	StartClient( bool bSystemLink = false );
	virtual bool	StartGame();
	virtual bool	CancelStartGame();
	virtual void	ChangeTeam( const char *pTeamName );
	virtual void	TellClientsToConnect();
	virtual void	CancelCurrentOperation();
	virtual void	EndStatsReporting();

	virtual void	JoinInviteSessionByID( XNKID nSessionID );
	virtual void	JoinInviteSession( XSESSION_INFO *pHostInfo );
	virtual void	KickPlayerFromSession( uint64 id );
	
	// For GameUI
	virtual KeyValues *GetSessionProperties();

	// For voice chat
	virtual uint64	PlayerIdToXuid( int playerId );
	virtual bool	IsPlayerMuted( int iUserId, XUID id );

	// To determine host Quality-of-Service
	virtual MM_QOS_t GetQosWithLIVE();

	virtual bool	PreventFullServerStartup();

	// IConnectionlessPacketHandler implementation (Host/Client shared)
	virtual bool	ProcessConnectionlessPacket( netpacket_t * packet );

	// INetChannelHandler implementation
	virtual void	ConnectionStart(INetChannel *chan);	// called first time network channel is established
	virtual void	PacketEnd();						// all messages have been parsed

	// NetChannel message handlers
 	PROCESS_NET_MESSAGE( Tick ) { return true; }
 	PROCESS_NET_MESSAGE( SetConVar ) { return true; }
	PROCESS_NET_MESSAGE( StringCmd ) { return true; }
	PROCESS_NET_MESSAGE( SignonState ) { return true; }

	PROCESS_CLC_MESSAGE( VoiceData );
	PROCESS_CLC_MESSAGE( ClientInfo ) { return true; }
	PROCESS_CLC_MESSAGE( Move ) { return true; }
	PROCESS_CLC_MESSAGE( BaselineAck ) { return true; }
	PROCESS_CLC_MESSAGE( ListenEvents ) { return true; }
	PROCESS_CLC_MESSAGE( RespondCvarValue ) { return true; }
	PROCESS_CLC_MESSAGE( FileCRCCheck ) { return true; }
	PROCESS_CLC_MESSAGE( FileMD5Check ) { return true; }
	PROCESS_CLC_MESSAGE( SaveReplay ) { return true; }
	PROCESS_CLC_MESSAGE( CmdKeyValues ) { return true; }


	PROCESS_MM_MESSAGE( JoinResponse );
	PROCESS_MM_MESSAGE( ClientInfo );
	PROCESS_MM_MESSAGE( RegisterResponse );
	PROCESS_MM_MESSAGE(	Migrate );
	PROCESS_MM_MESSAGE( Mutelist );
	PROCESS_MM_MESSAGE( Checkpoint );
	PROCESS_MM_MESSAGE( Heartbeat ) { return true; }

	// (Not used)
	virtual void ConnectionClosing(const char *reason) {};							// network channel is being closed by remote site
	virtual void ConnectionCrashed(const char *reason) {};							// network error occurred
	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) {};	// called each time a new packet arrived
	virtual void FileRequested(const char *fileName, unsigned int transferID ) {};	// other side request a file for download
	virtual void FileReceived(const char *fileName, unsigned int transferID ) {};	// we received a file
	virtual void FileDenied(const char *fileName, unsigned int transferID ) {};		// a file request was denied by other side
	virtual void FileSent(const char *fileName, unsigned int transferID ) {};		// a file was sent

	// Debugging helpers
	void	ShowSessionInfo();
	void	SendDevMessage( const char *message );

	void	SetSessionSlots( const uint nSlotsTotal, const uint nSlotsPrivate );
	void	RunFrame();
	void	EndGame();

	void	AddLocalPlayersToTeams();
	void	OnLevelLoadingFinished();

	void	TestSendMessage();
	void	TestStats();

	bool	GameIsActive();
	
	void	PrintVoiceStatus( void );

private:
	// NetChannel send
	void	SendMessage( INetMessage *msg, netadr_t *adr, bool bVoice = false );
	void	SendMessage( INetMessage *msg, CClientInfo *pClient, bool bVoice = false );
	void	SendToRemoteClients( INetMessage *msg, bool bVoice = false, XUID excludeXUID = -1 );

	// Session Host
	void	OnHostSessionCreated();
	void	UpdateAcceptingConnections();
	void	SendModifySessionMessage();
	void	EndSessionModify();
	bool	IsAcceptingConnections();
	void	HandleSystemLinkSearch( netpacket_t *pPacket );
	void	HandleJoinRequest( netpacket_t *pPacket );
	void	StartCountdown();
	void	CancelCountdown();
	void	UpdatePregame();
	void	UpdateRegistration();
	void	UpdateSessionModify();
	void	ProcessRegistrationResults();
	void	UpdateServerNegotiation();
	void	UpdateSessionReplyData( uint flags );
	void	SwitchToNextOpenTeam( CClientInfo *pClient );
	void	SetupTeams();
	int		ChooseTeam();
	int		GetPlayersNeeded();

	// Session Client
	bool	StartSystemLinkSearch();
	void	HandleSystemLinkReply( netpacket_t *pPacket );
	bool	SearchForSession();
	void	UpdateSearch();
	void	UpdateQosLookup();
	void	CancelSearch();
	void	CancelQosLookup();
	void	ClearSearchResults();
	void	SendJoinRequest( netadr_t *adr );
	bool	ConnectToHost();
	void	UpdateConnecting();
	void	ApplySessionProperties( int numContexts, int numProperties, XUSER_CONTEXT *pContexts, XUSER_PROPERTY *pProperties );

	// Host/Client shared
	bool	InitializeLocalClient( bool bIsHost );
	void	AddPlayersToSession( CClientInfo *pClient );
	void	SendPlayerInfoToLobby( CClientInfo *pClient, int iHostIdx = -1 );
	void	RemovePlayersFromSession( CClientInfo *pClient );
	void	ClientDropped( CClientInfo *pClient );
	void	PerformDisconnect();
	void	GenerateMutelist( MM_Mutelist *pMsg );
	int		FindOrCreateContext( const uint id );
	int		FindOrCreateProperty( const uint id );
	void	AddSessionPropertyInternal( KeyValues *pProperty );

	// Host Migration
	CClientInfo *SelectNewHost();
	void		StartHostMigration();
	void		BeginHosting();
	void		TellClientsToMigrate();
	void		SwitchToNewHost();
	void		EndMigration();

	// General utility functions
	void		Cleanup();
	void		SwitchToState( int newState );
	double		GetTime();
	void		SendHeartbeat();
	bool		SendHeartbeat( CClientInfo *pClient );
	void		ClientInfoToNetMessage( MM_ClientInfo *pInfo, const CClientInfo *pClient );
	void		NetMessageToClientInfo( CClientInfo *pClient, const MM_ClientInfo *pInfo );
	bool		GameIsLocked();
	bool		IsInMigration();
	bool		IsServer();
	bool		ConnectedToServer();

	// Netchannel handling
	INetChannel *CreateNetChannel( netadr_t *adr );
	INetChannel *AddRemoteChannel( netadr_t *adr );
	INetChannel *FindChannel( const unsigned int ip );
	CClientInfo *FindClient( netadr_t *adr ); 
	CClientInfo *FindClientByXUID( XUID xuid );
	void		SetChannelTimeout( netadr_t *adr, int timeout );
	void		RemoveRemoteChannel( netadr_t *adr, const char *pReason );
	void		MarkChannelForRemoval( netadr_t *adr );
	void		CleanupMarkedChannels();

	void		UpdateVoiceStatus( void );

	void		SetPreventFullServerStartup( bool bState, PRINTF_FORMAT_STRING char const *fmt, ... );

private:

	// Used by a systemlink host to reply to broadcast searches
	struct systemLinkInfo_s
	{
		char					szHostName[MAX_PLAYER_NAME_LENGTH];
		char					szScenario[MAX_MAP_NAME];
		int						gameState;
		int						gameTime;
		int						iScenarioIndex;
		XUID					xuid;
		XSESSION_SEARCHRESULT	Result;
	};

	hostData_s						m_HostData;				// pass host info back to searching clients
	CClientInfo						m_Host;					// the session host
	CClientInfo						m_Local;				// the local client
	CClientInfo						*m_pNewHost;			// new host when migrating the session
	CClientInfo						*m_pGameServer;			// the client that will act as the game server
	CUtlVector< CClientInfo* >		m_Remote;				// remote clients
	CUtlVector< char* >				m_pSystemLinkResults;	// results from a system link search
	XSESSION_SEARCHRESULT_HEADER	*m_pSearchResults;		// dynamic buffer to hold session search results

	// Arbitration registration
	XSESSION_REGISTRATION_RESULTS	*m_pRegistrationResults;

	// QoS Data
	BOOL               m_bQoSTesting;
	XNQOS              m_QoSResult;
	XNQOS*             m_pQoSResult;
	const XNADDR*      m_QoSxnaddr[MAX_SEARCHRESULTS];
	const XNKID*       m_QoSxnkid[MAX_SEARCHRESULTS];
	const XNKEY*       m_QoSxnkey[MAX_SEARCHRESULTS];

	CUtlMap< unsigned int, INetChannel* >m_Channels;
	CUtlVector< unsigned int > m_ChannelsToRemove;

	AsyncHandle_t	m_hSearchHandle;
	CSession		m_Session;
	int				m_CurrentState;
	int				m_PreMigrateState;
	double			m_fNextHeartbeatTime;
	double			m_fCountdownStartTime;
	double			m_fRegistrationTimer;
	bool			m_bCreatedLocalTalker;
	bool			m_bInitialized;
	bool			m_bCleanup;
	bool			m_bPreventFullServerStartup;
	bool			m_bEnteredLobby;
	int				m_nHostOwnerId;
	int				m_nGameSize;
	int				m_nTotalTeams;
	int				m_nPrivateSlots;
	int				m_nQOSProbeCount;
	double			m_fQOSProbeTimer;
	int				m_nSendCount;
	double			m_fSendTimer;
	double			m_fWaitTimer;
	double			m_fHeartbeatInterval;
	uint64			m_Nonce;		// used in system link queries

	int				CountPlayersOnTeam( int idxTeam );	// players on each team

	XUID				m_Mutelist[MAX_PLAYERS_PER_CLIENT][MAX_PLAYERS];
	CUtlVector< XUID >	m_MutedBy[MAX_PLAYERS_PER_CLIENT];

	// Contexts and properties
	CUtlVector< XUSER_CONTEXT >		m_SessionContexts;		// for session creation
	CUtlVector< XUSER_PROPERTY >	m_SessionProperties;	// for session creation
	CUtlVector< XUSER_PROPERTY >	m_PlayerStats;			
	KeyValues						*m_pSessionKeys;		// for GameUI lobby setup

	double			m_flVoiceBlinkTime;

	XSESSION_INFO	m_InviteSessionInfo;

	enum InviteState_t
	{
		INVITE_NONE,
		INVITE_PENDING,
		INVITE_VALIDATING,
		INVITE_AWAITING_STORAGE,
		INVITE_ACCEPTING
	} m_InviteState;
	
	struct InviteWaitingInfo_t
	{
		DWORD				m_UserIdx;
#if defined( _X360 )
		XUSER_SIGNIN_INFO	m_SignInInfo;
#endif
		DWORD				m_SignInState;
		BOOL				m_PrivilegeMultiplayer;
		int					m_InviteStorageDeviceSelected;
		int					m_bAcceptingInvite;
	} m_InviteWaitingInfo;
	
	void RunFrameInvite();
	void InviteCancel();
};
extern CMatchmaking *g_pMatchmaking;

#endif // MATCHMAKING_H
