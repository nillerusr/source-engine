//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASECLIENT_H
#define BASECLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include <const.h>
#include <checksum_crc.h>
#include <iclient.h>
#include <protocol.h>
#include <iservernetworkable.h>
#include <bspfile.h>
#include <KeyValues.h>
#include <bitvec.h>
#include <igameevents.h>
#include "smartptr.h"
#include "userid.h"
#include "tier1/bitbuf.h"
#include "steam/steamclientpublic.h"

// class CClientFrame;
class CBaseServer;
class CClientFrame;
struct player_info_s;
class CFrameSnapshot;
class CEventInfo;

struct Spike_t
{
public:
	Spike_t() : 
		m_nBits( 0 )
	{
		m_szDesc[ 0 ] = 0;
	}
	char	m_szDesc[ 64 ];
	int		m_nBits;
};

class CNetworkStatTrace
{
public:
	CNetworkStatTrace() : 
		m_nMinWarningBytes( 0 ), m_nStartBit( 0 ), m_nCurBit( 0 ), m_StartSendTime(0.0)
	{
	}
	int						m_nMinWarningBytes;
	int						m_nStartBit;
	int						m_nCurBit;
	CUtlVector< Spike_t >	m_Records;
	double					m_StartSendTime;
};


class CBaseClient : public IGameEventListener2, public IClient, public IClientMessageHandler
{
	typedef struct CustomFile_s
	{
		CRC32_t			crc;	//file CRC
		unsigned int	reqID;	// download request ID
	} CustomFile_t;

public:
	CBaseClient();
	virtual ~CBaseClient();

public:

	int			GetPlayerSlot() const { return m_nClientSlot; };
	int			GetUserID() const { return m_UserID; };
	const USERID_t	GetNetworkID() const;
	const char		*GetClientName() const { return m_Name; };
	INetChannel		*GetNetChannel() { return m_NetChannel; };
	IServer			*GetServer() { return (IServer*)m_Server; };
	const char		*GetUserSetting(const char *cvar) const;
	const char		*GetNetworkIDString() const;
	uint			GetFriendsID() const { return m_nFriendsID; }
	const char		*GetFriendsName() const { return m_FriendsName; }
	void			UpdateName( const char *pszDefault );
	int				GetClientChallenge() const { return m_clientChallenge; }

	void			SetReportThisFakeClient( bool bReport ) { m_bReportFakeClient = bReport; }
	bool			ShouldReportThisFakeClient( void ) const { return m_bReportFakeClient; }

	virtual	void	Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, int clientChallenge );
	virtual	void	Inactivate( void );
	virtual	void	Reconnect( void );
	virtual	void	Disconnect( PRINTF_FORMAT_STRING const char *reason, ... );

	virtual	void	SetRate( int nRate, bool bForce );
	virtual	int		GetRate( void ) const;
	
	virtual void	SetUpdateRate( int nUpdateRate, bool bForce );
	virtual int		GetUpdateRate( void ) const;

	virtual void	Clear( void );
	virtual void	DemoRestart( void ); // called when client started demo recording

	virtual	int		GetMaxAckTickCount() const;

	virtual bool	ExecuteStringCommand( const char *s );
	virtual bool	SendNetMsg(INetMessage &msg, bool bForceReliable = false);
	
	virtual void	ClientPrintf (PRINTF_FORMAT_STRING const char *fmt, ...);

	virtual	bool	IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };	
	virtual	bool	IsSpawned( void ) const { return m_nSignonState >= SIGNONSTATE_NEW; };	
	virtual	bool	IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	virtual	bool	IsFakeClient( void ) const { return m_bFakePlayer; };

	virtual	bool	IsHLTV( void ) const { return m_bIsHLTV; }
#if defined( REPLAY_ENABLED )
	virtual bool	IsReplay() const { return m_bIsReplay; }
#endif
	void			OnSignonStateFull();

	// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
	virtual	bool	IsHearingClient( int index ) const { return false; };
	virtual	bool	IsProximityHearingClient( int index ) const { return false; };

	virtual void	SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize );

	virtual bool	IsSplitScreenUser( void ) const { return false; } // !KLUDGE! We don't have splitscreen support, but this makes merges easier

public: // IClientMessageHandlers
	
	PROCESS_NET_MESSAGE( Tick );
	PROCESS_NET_MESSAGE( StringCmd );
	PROCESS_NET_MESSAGE( SetConVar );
	PROCESS_NET_MESSAGE( SignonState );
	
	PROCESS_CLC_MESSAGE( ClientInfo );
	PROCESS_CLC_MESSAGE( BaselineAck );
	PROCESS_CLC_MESSAGE( ListenEvents );
	PROCESS_CLC_MESSAGE( CmdKeyValues );
	
	virtual void	ConnectionStart(INetChannel *chan);

public: // IGameEventListener
	virtual void	FireGameEvent( IGameEvent *event );

public:

	virtual	bool	UpdateAcknowledgedFramecount(int tick);
	virtual bool	ShouldSendMessages( void );
	virtual void	UpdateSendState( void );
			void	ForceFullUpdate( void ) { UpdateAcknowledgedFramecount(-1); }
	
	virtual bool	FillUserInfo( player_info_s &userInfo );
	virtual void	UpdateUserSettings();
	virtual bool	SetSignonState(int state, int spawncount);
	virtual void	WriteGameSounds(bf_write &buf);
	
	virtual CClientFrame *GetDeltaFrame( int nTick );
	virtual void	SendSnapshot( CClientFrame *pFrame );
	virtual bool	SendServerInfo( void );
	virtual bool	SendSignonData( void );
	virtual void	SpawnPlayer( void );
	virtual void	ActivatePlayer( void );
	virtual void	SetName( const char * playerName );
	virtual void	SetUserCVar( const char *cvar, const char *value);
	virtual void	FreeBaselines();
	virtual bool	IgnoreTempEntity( CEventInfo *event );
	
	void			SetSteamID( const CSteamID &steamID );

	void			ClientRequestNameChange( const char *pszName );

	bool			IsTracing() const { return m_iTracing > 0; }
	void			TraceNetworkData( bf_write &msg, PRINTF_FORMAT_STRING char const *fmt, ... ) FMTFUNCTION( 3, 4 );
	void			TraceNetworkMsg( int nBits, PRINTF_FORMAT_STRING char const *fmt, ... ) FMTFUNCTION( 3, 4 );

	bool			IsFullyAuthenticated( void ) { return m_bFullyAuthenticated; }
	void			SetFullyAuthenticated( void ) { m_bFullyAuthenticated = true; }

	void			CheckFlushNameChange( bool bShowStatusMessage = false );
	bool			IsNameChangeOnCooldown( bool bShowStatusMessage = false );

	void			SetPlayerNameLocked( bool bValue ) { m_bPlayerNameLocked = bValue; }
	bool			IsPlayerNameLocked( void ) { return m_bPlayerNameLocked; }

private:	

	void			OnRequestFullUpdate();


public:

	// Array index in svs.clients:
	int				m_nClientSlot;	
	// entity index of this client (different from clientSlot+1 in HLTV and Replay mode):
	int				m_nEntityIndex;	
	
	int				m_UserID;			// identifying number on server
	char			m_Name[MAX_PLAYER_NAME_LENGTH];			// for printing to other people
	char			m_GUID[SIGNED_GUID_LEN + 1]; // the clients CD key

	CSteamID		m_SteamID;			// This is valid when the client is authenticated
	
	uint32			m_nFriendsID;		// client's friends' ID
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];

	KeyValues		*m_ConVars;			// stores all client side convars
	bool			m_bConVarsChanged;	// true if convars updated and not changes process yet
	bool			m_bInitialConVarsSet; // Has the client sent their initial set of convars
	bool			m_bSendServerInfo;	// true if we need to send server info packet to start connect
	CBaseServer		*m_Server;			// pointer to server object
	bool			m_bIsHLTV;			// if this a HLTV proxy ?
#if defined( REPLAY_ENABLED )
	bool			m_bIsReplay;		// if this is a Replay proxy ?
#endif
	int				m_clientChallenge;	// client's challenge he sent us, we use to auth replies
	
	// Client sends this during connection, so we can see if
	//  we need to send sendtable info or if the .dll matches
	CRC32_t			m_nSendtableCRC;

	// a client can have couple of cutomized files distributed to all other players
	CustomFile_t	m_nCustomFiles[MAX_CUSTOM_FILES];
	int				m_nFilesDownloaded;	// counter of how many files we downloaded from this client

	//===== NETWORK ============
	INetChannel		*m_NetChannel;		// The client's net connection.
	int				m_nSignonState;		// connection state
	int				m_nDeltaTick;		// -1 = no compression.  This is where the server is creating the
										// compressed info from.
	int				m_nStringTableAckTick; // Highest tick acked for string tables (usually m_nDeltaTick, except when it's -1)
	int				m_nSignonTick;		// tick the client got his signon data
	CSmartPtr<CFrameSnapshot,CRefCountAccessorLongName> m_pLastSnapshot;	// last send snapshot

	CFrameSnapshot	*m_pBaseline;			// current entity baselines as a snapshot
	int				m_nBaselineUpdateTick;	// last tick we send client a update baseline signal or -1
	CBitVec<MAX_EDICTS>	m_BaselinesSent;	// baselines sent with last update
	int				m_nBaselineUsed;		// 0/1 toggling flag, singaling client what baseline to use
	
		
	// This is used when we send out a nodelta packet to put the client in a state where we wait 
	// until we get an ack from them on this packet.
	// This is for 3 reasons:
	// 1. A client requesting a nodelta packet means they're screwed so no point in deluging them with data.
	//    Better to send the uncompressed data at a slow rate until we hear back from them (if at all).
	// 2. Since the nodelta packet deletes all client entities, we can't ever delta from a packet previous to it.
	// 3. It can eat up a lot of CPU on the server to keep building nodelta packets while waiting for
	//    a client to get back on its feet.
	int				m_nForceWaitForTick;
	
	bool			m_bFakePlayer;		// JAC: This client is a fake player controlled by the game DLL
	bool			m_bReportFakeClient; // Should this fake client be reported 
	bool		   m_bReceivedPacket;	// true, if client received a packet after the last send packet

	bool			m_bFullyAuthenticated;

	// Time when last name change was applied
	double			m_fTimeLastNameChange;
	bool			m_bPlayerNameLocked;

	// Does this client have a name change that is pending?
	// (Their 'name' convar differs from our value for their client name.)
	char			m_szPendingNameChange[MAX_PLAYER_NAME_LENGTH];

	// The datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.

	// Time when we should send next world state update ( datagram )
	double         m_fNextMessageTime;   
	// Default time to wait for next message
	float          m_fSnapshotInterval;  

	enum
	{
		SNAPSHOT_SCRATCH_BUFFER_SIZE = 160000,
	};

	unsigned int		m_SnapshotScratchBuffer[ SNAPSHOT_SCRATCH_BUFFER_SIZE / 4 ];

private:
	void				StartTrace( bf_write &msg );
	void				EndTrace( bf_write &msg );


	int					m_iTracing; // 0 = not active, 1 = active for this frame, 2 = forced active
	CNetworkStatTrace	m_Trace;
};



#endif // BASECLIENT_H
