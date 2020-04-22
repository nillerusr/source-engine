//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#ifdef _WIN32
#pragma once
#pragma warning(disable : 4100)	// unreferenced formal parameter
#endif

#include <inetmessage.h>
#include <checksum_crc.h>
#include <checksum_md5.h>
#include <const.h>
#include <utlvector.h>
#include "qlimits.h"
#include "mathlib/vector.h"
#include <soundflags.h>
#include <bitbuf.h>
#include <inetchannel.h>
#include "protocol.h"
#include <inetmsghandler.h>
#include <igameevents.h>
#include <bitvec.h>
#include <engine/iserverplugin.h>
#include <Color.h>
#include "proto_version.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

class SendTable;
class KeyValue;
class KeyValues;
class INetMessageHandler;
class IServerMessageHandler;
class IClientMessageHandler;

#define DECLARE_BASE_MESSAGE( msgtype )						\
	public:													\
		bool			ReadFromBuffer( bf_read &buffer );	\
		bool			WriteToBuffer( bf_write &buffer );	\
		const char		*ToString() const;					\
		int				GetType() const { return msgtype; } \
		const char		*GetName() const { return #msgtype;}\
			
#define DECLARE_NET_MESSAGE( name )			\
	DECLARE_BASE_MESSAGE( net_##name );		\
	INetMessageHandler *m_pMessageHandler;	\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\
	
#define DECLARE_SVC_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( svc_##name );	\
	IServerMessageHandler *m_pMessageHandler;\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\
	
#define DECLARE_CLC_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( clc_##name );	\
	IClientMessageHandler *m_pMessageHandler;\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\
		
#define DECLARE_MM_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( mm_##name );	\
	IMatchmakingMessageHandler *m_pMessageHandler;\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\

class CNetMessage : public INetMessage
{
public:
	CNetMessage() {	m_bReliable = true;
					m_NetChannel = NULL; }

	virtual ~CNetMessage() {};

	virtual int		GetGroup() const { return INetChannelInfo::GENERIC; }
	INetChannel		*GetNetChannel() const { return m_NetChannel; }
		
	virtual void	SetReliable( bool state) {m_bReliable = state;};
	virtual bool	IsReliable() const { return m_bReliable; };
	virtual void    SetNetChannel(INetChannel * netchan) { m_NetChannel = netchan; }	
	virtual bool	Process() { Assert( 0 ); return false; };	// no handler set

protected:
	bool				m_bReliable;	// true if message should be send reliable
	INetChannel			*m_NetChannel;	// netchannel this message is from/for
};


///////////////////////////////////////////////////////////////////////////////////////
// bidirectional net messages:
///////////////////////////////////////////////////////////////////////////////////////


class NET_SetConVar : public CNetMessage
{
	DECLARE_NET_MESSAGE( SetConVar );

	int	GetGroup() const { return INetChannelInfo::STRINGCMD; }

	NET_SetConVar() {}
	NET_SetConVar( const char * name, const char * value)
	{
		cvar_t localCvar;
		Q_strncpy( localCvar.name, name, MAX_OSPATH );
		Q_strncpy( localCvar.value, value, MAX_OSPATH );
		m_ConVars.AddToTail( localCvar );	
	}

public:	
	
	typedef struct cvar_s
	{
		char	name[MAX_OSPATH];
		char	value[MAX_OSPATH];
	} cvar_t;

	CUtlVector<cvar_t> m_ConVars;
};

class NET_StringCmd : public CNetMessage
{
	DECLARE_NET_MESSAGE( StringCmd );

	int	GetGroup() const { return INetChannelInfo::STRINGCMD; }

	NET_StringCmd() { m_szCommand = NULL; };
	NET_StringCmd(const char *cmd) { m_szCommand = cmd; };

public:	
	const char	*m_szCommand;	// execute this command
	
private:
	char		m_szCommandBuffer[1024];	// buffer for received messages
	
};

class NET_Tick : public CNetMessage
{
	DECLARE_NET_MESSAGE( Tick );

	NET_Tick() 
	{ 
		m_bReliable = false; 
#if PROTOCOL_VERSION > 10
		m_flHostFrameTime				= 0;
		m_flHostFrameTimeStdDeviation	= 0;
#endif
	};

	NET_Tick( int tick, float hostFrametime, float hostFrametime_stddeviation ) 
	{ 
		m_bReliable = false; 
		m_nTick = tick; 
#if PROTOCOL_VERSION > 10
		m_flHostFrameTime			= hostFrametime;
		m_flHostFrameTimeStdDeviation	= hostFrametime_stddeviation;
#else
		NOTE_UNUSED( hostFrametime );
		NOTE_UNUSED( hostFrametime_stddeviation );
#endif
	};
	
public:
	int			m_nTick; 
#if PROTOCOL_VERSION > 10
	float		m_flHostFrameTime;
	float		m_flHostFrameTimeStdDeviation;
#endif
};

class NET_SignonState : public CNetMessage
{
	DECLARE_NET_MESSAGE( SignonState );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }

	NET_SignonState() {};
	NET_SignonState( int state, int spawncount ) { m_nSignonState = state; m_nSpawnCount = spawncount; };

public:
	int			m_nSignonState;			// See SIGNONSTATE_ defines
	int			m_nSpawnCount;			// server spawn count (session number)
};


///////////////////////////////////////////////////////////////////////////////////////
// Client messages:
///////////////////////////////////////////////////////////////////////////////////////

class CLC_ClientInfo : public CNetMessage
{
	DECLARE_CLC_MESSAGE( ClientInfo );

public:
	CRC32_t			m_nSendTableCRC;
	int				m_nServerCount;
	bool			m_bIsHLTV;
#if defined( REPLAY_ENABLED )
	bool			m_bIsReplay;
#endif
	uint32			m_nFriendsID;
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];
	CRC32_t			m_nCustomFiles[MAX_CUSTOM_FILES];
};



class CLC_Move : public CNetMessage
{
	DECLARE_CLC_MESSAGE( Move );

	int	GetGroup() const { return INetChannelInfo::MOVE; }

	CLC_Move() { m_bReliable = false; }

public:
	int				m_nBackupCommands;
	int				m_nNewCommands;
	int				m_nLength;
	bf_read			m_DataIn;
	bf_write		m_DataOut;
};

class CLC_VoiceData : public CNetMessage
{
	DECLARE_CLC_MESSAGE( VoiceData );

	int	GetGroup() const { return INetChannelInfo::VOICE; }

	CLC_VoiceData() { m_bReliable = false; };

public:
	int				m_nLength;
	bf_read			m_DataIn;
	bf_write		m_DataOut;
	uint64			m_xuid;
};

class CLC_BaselineAck : public CNetMessage
{
	DECLARE_CLC_MESSAGE( BaselineAck );

	CLC_BaselineAck() {};
	CLC_BaselineAck(int tick, int baseline ) { m_nBaselineTick = tick; m_nBaselineNr = baseline; }

	int	GetGroup() const { return INetChannelInfo::ENTITIES; }

public:
	int		m_nBaselineTick;	// sequence number of baseline
	int		m_nBaselineNr;		// 0 or 1 		
};

class CLC_ListenEvents : public CNetMessage
{
	DECLARE_CLC_MESSAGE( ListenEvents );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }

public:
	CBitVec<MAX_EVENT_NUMBER> m_EventArray;
};

#if defined( REPLAY_ENABLED )
class CLC_SaveReplay : public CNetMessage
{
	DECLARE_CLC_MESSAGE( SaveReplay );

	CLC_SaveReplay() {}

	int		m_nStartSendByte;
	char	m_szFilename[ MAX_OSPATH ];
	float	m_flPostDeathRecordTime;
};
#endif

class CLC_RespondCvarValue : public CNetMessage
{
public:
	DECLARE_CLC_MESSAGE( RespondCvarValue );

	QueryCvarCookie_t		m_iCookie;
	
	const char				*m_szCvarName;
	const char				*m_szCvarValue;	// The sender sets this, and it automatically points it at m_szCvarNameBuffer when receiving.

	EQueryCvarValueStatus	m_eStatusCode;

private:
	char		m_szCvarNameBuffer[256];
	char		m_szCvarValueBuffer[256];
};

class CLC_FileCRCCheck : public CNetMessage
{
public:
	DECLARE_CLC_MESSAGE( FileCRCCheck );
	char		m_szPathID[MAX_PATH];
	char		m_szFilename[MAX_PATH];
	MD5Value_t	m_MD5;
	CRC32_t		m_CRCIOs;
	int			m_eFileHashType;
	int			m_cbFileLen;
	int			m_nPackFileNumber;
	int			m_PackFileID;
	int			m_nFileFraction;
};

class CLC_FileMD5Check : public CNetMessage
{
public:
	DECLARE_CLC_MESSAGE( FileMD5Check );

	char		m_szPathID[MAX_PATH];
	char		m_szFilename[MAX_PATH];
	MD5Value_t	m_MD5;
};

class Base_CmdKeyValues : public CNetMessage
{
protected:
	explicit Base_CmdKeyValues( KeyValues *pKeyValues = NULL ); // takes ownership
	~Base_CmdKeyValues();

public:
	KeyValues * GetKeyValues() const { return m_pKeyValues; }

public:
	bool ReadFromBuffer( bf_read &buffer );
	bool WriteToBuffer( bf_write &buffer );
	const char * ToString() const;

protected:
	KeyValues *m_pKeyValues;
};

class CLC_CmdKeyValues : public Base_CmdKeyValues
{
public:
	DECLARE_CLC_MESSAGE( CmdKeyValues );

public:
	explicit CLC_CmdKeyValues( KeyValues *pKeyValues = NULL );	// takes ownership
};

class SVC_CmdKeyValues : public Base_CmdKeyValues
{
public:
	DECLARE_SVC_MESSAGE( CmdKeyValues );

public:
	explicit SVC_CmdKeyValues( KeyValues *pKeyValues = NULL );	// takes ownership
};

///////////////////////////////////////////////////////////////////////////////////////
// server messages:
///////////////////////////////////////////////////////////////////////////////////////



class SVC_Print : public CNetMessage
{
	DECLARE_SVC_MESSAGE( Print );

	SVC_Print() { m_bReliable = false; m_szText = NULL; };

	SVC_Print(const char * text) { m_bReliable = false; m_szText = text; };

public:	
	const char	*m_szText;	// show this text
	
private:
	char		m_szTextBuffer[2048];	// buffer for received messages
};

class SVC_ServerInfo : public CNetMessage
{
	DECLARE_SVC_MESSAGE( ServerInfo );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }

public:	// member vars are public for faster handling
	int			m_nProtocol;	// protocol version
	int			m_nServerCount;	// number of changelevels since server start
	bool		m_bIsDedicated;  // dedicated server ?	
	bool		m_bIsHLTV;		// HLTV server ?
#if defined( REPLAY_ENABLED )
	bool		m_bIsReplay;	// Replay server ?
#endif
	char		m_cOS;			// L = linux, W = Win32
	CRC32_t		m_nMapCRC;		// server map CRC (only used by older demos)
	MD5Value_t	m_nMapMD5;		// server map MD5
	int			m_nMaxClients;	// max number of clients on server
	int			m_nMaxClasses;	// max number of server classes
	int			m_nPlayerSlot;	// our client slot number
	float		m_fTickInterval;// server tick interval
	const char	*m_szGameDir;	// game directory eg "tf2"
	const char	*m_szMapName;	// name of current map 
	const char	*m_szSkyName;	// name of current skybox 
	const char	*m_szHostName;	// server name

private:
	char		m_szGameDirBuffer[MAX_OSPATH];// game directory eg "tf2"
	char		m_szMapNameBuffer[MAX_OSPATH];// name of current map 
	char		m_szSkyNameBuffer[MAX_OSPATH];// name of current skybox 
	char		m_szHostNameBuffer[MAX_OSPATH];// name of current skybox 
};

class SVC_SendTable : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SendTable );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }
		
public:
	bool			m_bNeedsDecoder;
	int				m_nLength;
	bf_read			m_DataIn;
	bf_write		m_DataOut;
};

class SVC_ClassInfo : public CNetMessage
{
	DECLARE_SVC_MESSAGE( ClassInfo );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }

	SVC_ClassInfo() {};
	SVC_ClassInfo( bool createFromSendTables, int numClasses ) 
		{ m_bCreateOnClient = createFromSendTables; 
		  m_nNumServerClasses = numClasses; };

public:
		
	typedef struct class_s
	{
		int		classID;
		char	datatablename[256];
		char	classname[256];
	} class_t;

	bool					m_bCreateOnClient;	// if true, client creates own SendTables & classinfos from game.dll
	CUtlVector<class_t>		m_Classes;			
	int						m_nNumServerClasses;
};
	

class SVC_SetPause : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SetPause );
	
	SVC_SetPause() {}
	SVC_SetPause( bool state, float end = -1.f ) { m_bPaused = state; }
	
public:
	bool		m_bPaused;		// true or false, what else
};

class SVC_SetPauseTimed : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SetPauseTimed );

	SVC_SetPauseTimed() {}
	SVC_SetPauseTimed( bool bState, float flExpireTime = -1.f ) { m_bPaused = bState; m_flExpireTime = flExpireTime; }

public:
	bool		m_bPaused;
	float		m_flExpireTime;
};


class CNetworkStringTable;

class SVC_CreateStringTable : public CNetMessage
{
	DECLARE_SVC_MESSAGE( CreateStringTable );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }

public:

	SVC_CreateStringTable();

public:	
	
	const char	*m_szTableName;
	int			m_nMaxEntries;
	int			m_nNumEntries;
	bool		m_bUserDataFixedSize;
	int			m_nUserDataSize;
	int			m_nUserDataSizeBits;
	bool		m_bIsFilenames;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
	bool		m_bDataCompressed;

private:
	char		m_szTableNameBuffer[256];
};

class SVC_UpdateStringTable : public CNetMessage
{
	DECLARE_SVC_MESSAGE( UpdateStringTable );

	int	GetGroup() const { return INetChannelInfo::STRINGTABLE; }

public:
	int				m_nTableID;	// table to be updated
	int				m_nChangedEntries; // number of how many entries has changed
	int				m_nLength;	// data length in bits
	bf_read			m_DataIn;
	bf_write		m_DataOut;
};

// SVC_VoiceInit
//   v2 - 2017/02/07
//     - Can detect v2 packets by nLegacyQuality == 255 and presence of additional nSampleRate field.
//     - Added nSampleRate field. Previously, nSampleRate was hard-coded per codec type. ::ReadFromBuffer does a
//       one-time conversion of these old types (which can no longer change)
//     - Marked quality field as deprecated. This was already being ignored. v2 clients send 255
//     - Prior to this the sv_use_steam_voice convar was used to switch to steam voice. With this, we properly set
//       szVoiceCodec to "steam".  See ::ReadFromBuffer for shim to fallback to the convar for old streams.
//     - We no longer pass "svc_voiceinit NULL" as szVoiceCodec if it is not selected, just the empty string.  Nothing
//       used this that I could find.
class SVC_VoiceInit : public CNetMessage
{
	DECLARE_SVC_MESSAGE( VoiceInit );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }

	SVC_VoiceInit()
		: m_nSampleRate( 0 )
	{
		V_memset( m_szVoiceCodec, 0, sizeof( m_szVoiceCodec ) );
	}

	SVC_VoiceInit( const char * codec, int nSampleRate )
		: m_nSampleRate( nSampleRate )
	{
		V_strncpy( m_szVoiceCodec, codec ? codec : "", sizeof( m_szVoiceCodec ) );
	}


public:
	// Used voice codec for voice_init.
	//
	// This used to be a DLL name, then became a whitelisted list of codecs.
	char		m_szVoiceCodec[MAX_OSPATH];

	// DEPRECATED:
	//
	// This field used to be a custom quality setting, but it was not honored for a long time: codecs use their own
	// pre-configured quality settings. We never sent anything besides 5, which was then ignored for some codecs.
	//
	// New clients always set 255 here, old clients probably send 5. This could be re-purposed in the future, but beware
	// that very old demos may have non-5 values. It would take more archaeology to determine how to properly interpret
	// those packets -- they're probably using settings we simply don't support any longer.
	//
	// int m_nQuality;

	// The sample rate we are using
	int			m_nSampleRate;
};

class SVC_VoiceData : public CNetMessage
{
	DECLARE_SVC_MESSAGE( VoiceData );

	int	GetGroup() const { return INetChannelInfo::VOICE; }

	SVC_VoiceData() { m_bReliable = false; }

public:	
	int				m_nFromClient;	// client who has spoken
	bool			m_bProximity;
	int				m_nLength;		// data length in bits
	uint64			m_xuid;			// X360 player ID

	bf_read			m_DataIn;
	void			*m_DataOut;
};

class SVC_Sounds : public CNetMessage
{
	DECLARE_SVC_MESSAGE( Sounds );

	int	GetGroup() const { return INetChannelInfo::SOUNDS; }

public:	

	bool		m_bReliableSound;
	int			m_nNumSounds;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_Prefetch : public CNetMessage
{
	DECLARE_SVC_MESSAGE( Prefetch );

	int	GetGroup() const { return INetChannelInfo::SOUNDS; }

	enum
	{
		SOUND = 0,
	};

public:	

	unsigned short	m_fType;
	unsigned short	m_nSoundIndex;
};

class SVC_SetView : public CNetMessage
{
	DECLARE_SVC_MESSAGE( SetView );

	SVC_SetView() {}
	SVC_SetView( int entity ) { m_nEntityIndex = entity; }

public:	
	int				m_nEntityIndex;
		
};

class SVC_FixAngle: public CNetMessage
{
	DECLARE_SVC_MESSAGE( FixAngle );

	SVC_FixAngle() { m_bReliable = false; };
	SVC_FixAngle( bool bRelative, QAngle angle ) 
		{ m_bReliable = false; m_bRelative = bRelative; m_Angle = angle; }

public:	
	bool			m_bRelative; 
	QAngle			m_Angle;
};

class SVC_CrosshairAngle : public CNetMessage
{
	DECLARE_SVC_MESSAGE( CrosshairAngle );

	SVC_CrosshairAngle() {}
	SVC_CrosshairAngle( QAngle angle ) { m_Angle = angle; }
	
public:
	QAngle			m_Angle;
};

class SVC_BSPDecal : public CNetMessage
{
	DECLARE_SVC_MESSAGE( BSPDecal );
	
public:
	Vector		m_Pos;
	int			m_nDecalTextureIndex;
	int			m_nEntityIndex;
	int			m_nModelIndex;
	bool		m_bLowPriority;
};

class SVC_GameEvent : public CNetMessage
{
	DECLARE_SVC_MESSAGE( GameEvent );

	int	GetGroup() const { return INetChannelInfo::EVENTS; }
	
public:
	int			m_nLength;	// data length in bits
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_UserMessage: public CNetMessage
{
	DECLARE_SVC_MESSAGE( UserMessage );

	SVC_UserMessage() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::USERMESSAGES; }
	
public:
	int			m_nMsgType;
	int			m_nLength;	// data length in bits
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_EntityMessage : public CNetMessage
{
	DECLARE_SVC_MESSAGE( EntityMessage );

	SVC_EntityMessage() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::ENTMESSAGES	; }

public:
	int			m_nEntityIndex;
	int			m_nClassID;
	int			m_nLength;	// data length in bits
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

/* class SVC_SpawnBaseline: public CNetMessage
{
	DECLARE_SVC_MESSAGE( SpawnBaseline );

	int	GetGroup() const { return INetChannelInfo::SIGNON; }
	
public:
	int			m_nEntityIndex;
	int			m_nClassID;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
	
}; */

class SVC_PacketEntities: public CNetMessage
{
	DECLARE_SVC_MESSAGE( PacketEntities );
	
	int	GetGroup() const { return INetChannelInfo::ENTITIES	; }
	
public:

	int			m_nMaxEntries;
	int			m_nUpdatedEntries;
	bool		m_bIsDelta;	
	bool		m_bUpdateBaseline;
	int			m_nBaseline;
	int			m_nDeltaFrom;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_TempEntities: public CNetMessage
{
	DECLARE_SVC_MESSAGE( TempEntities );

	SVC_TempEntities() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::EVENTS; }

	int			m_nNumEntries;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

class SVC_Menu : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( Menu );

	SVC_Menu() { m_bReliable = true; m_Type = DIALOG_MENU; m_MenuKeyValues = NULL; };
	SVC_Menu( DIALOG_TYPE type, KeyValues *data ); 
	~SVC_Menu();

	KeyValues	*m_MenuKeyValues;
	DIALOG_TYPE m_Type;
	int			m_iLength;
};

class SVC_GameEventList : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( GameEventList );

	int			m_nNumEvents;
	int			m_nLength;
	bf_read		m_DataIn;
	bf_write	m_DataOut;
};

///////////////////////////////////////////////////////////////////////////////////////
// Matchmaking messages:
///////////////////////////////////////////////////////////////////////////////////////

class MM_Heartbeat : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Heartbeat );
};

class MM_ClientInfo : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( ClientInfo );

	XNADDR	m_xnaddr;	// xbox net address
	uint64	m_id;		// machine ID
	uint64	m_xuids[MAX_PLAYERS_PER_CLIENT];
	byte	m_cVoiceState[MAX_PLAYERS_PER_CLIENT];
	bool	m_bInvited;
	char	m_cPlayers;
	char	m_iControllers[MAX_PLAYERS_PER_CLIENT];
	char	m_iTeam[MAX_PLAYERS_PER_CLIENT];
	char	m_szGamertags[MAX_PLAYERS_PER_CLIENT][MAX_PLAYER_NAME_LENGTH];
};

class MM_RegisterResponse : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( RegisterResponse );
};

class MM_Mutelist : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Mutelist );

	uint64	m_id;
	byte	m_cPlayers;
	byte	m_cRemoteTalkers[MAX_PLAYERS_PER_CLIENT];
	XUID	m_xuid[MAX_PLAYERS_PER_CLIENT];
	byte	m_cMuted[MAX_PLAYERS_PER_CLIENT];
	CUtlVector< XUID >	m_Muted[MAX_PLAYERS_PER_CLIENT];
};

class MM_Checkpoint : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Checkpoint );

	enum eCheckpoint
	{
		CHECKPOINT_CHANGETEAM,
		CHECKPOINT_GAME_LOBBY,
		CHECKPOINT_PREGAME,
		CHECKPOINT_LOADING_COMPLETE,
		CHECKPOINT_CONNECT,
		CHECKPOINT_SESSION_DISCONNECT,
		CHECKPOINT_REPORT_STATS,
		CHECKPOINT_REPORTING_COMPLETE,
		CHECKPOINT_POSTGAME,
	};

	byte m_Checkpoint;
};

// NOTE: The following messages are not network-endian compliant, due to the
// transmission of structures instead of their component parts
class MM_JoinResponse : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( JoinResponse );

	MM_JoinResponse()
	{
		m_ContextCount = 0;
		m_PropertyCount = 0;
	}

	enum
	{
		JOINRESPONSE_APPROVED,
		JOINRESPONSE_APPROVED_JOINGAME,
		JOINRESPONSE_SESSIONFULL,
		JOINRESPONSE_NOTHOSTING,
		JOINRESPONSE_MODIFY_SESSION,
	};
	uint	m_ResponseType;

	// host info
	uint64	m_id;				// Host's machine ID
	uint64	m_Nonce;			// Session nonce
	uint	m_SessionFlags;
	uint	m_nOwnerId;
	int		m_iTeam;
	int		m_nTotalTeams;
	int		m_PropertyCount;
	int		m_ContextCount;
	CUtlVector< XUSER_PROPERTY >m_SessionProperties;
	CUtlVector< XUSER_CONTEXT >m_SessionContexts;
};

class MM_Migrate : public CNetMessage
{
public:
	DECLARE_MM_MESSAGE( Migrate );

	enum eMsgType
	{
		MESSAGE_HOSTING,
		MESSAGE_MIGRATED,
		MESSAGE_STANDBY,
	};

	byte	m_MsgType;
	uint64	m_Id;
	XNKID	m_sessionId; 
	XNADDR	m_xnaddr;  
	XNKEY	m_key;
};

class SVC_GetCvarValue : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( GetCvarValue );

	QueryCvarCookie_t	m_iCookie;
	const char			*m_szCvarName;	// The sender sets this, and it automatically points it at m_szCvarNameBuffer when receiving.

private:
	char		m_szCvarNameBuffer[256];
};

#endif // NETMESSAGES_H
