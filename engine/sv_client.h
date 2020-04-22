//========= Copyright Valve Corporation, All rights reserved. ============//
//
// CGameClient: represents a player client in a game server
//
//===========================================================================//

#ifndef SV_CLIENT_H
#define SV_CLIENT_H

#ifdef _WIN32
#pragma once
#endif

#include "const.h"
#include "bitbuf.h"
#include "net.h"
#include "checksum_engine.h"
#include "event_system.h"
#include "utlvector.h"
#include "bitvec.h"
#include "protocol.h"
#include <inetmsghandler.h>
#include "baseclient.h"
#include "clientframe.h"
#include <soundinfo.h>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class	INetChannel;
class	CClientFrame;
class	CFrameSnapshot;
class	CClientMsgHandler;
struct	edict_t;
struct	SoundInfo_t;
class	KeyValues;
class	CHLTVServer;
class	CReplayServer;
class	CPerClientLogoInfo;
class	CCommand;


//-----------------------------------------------------------------------------
// CGameClient: represents a player client in a game server
//-----------------------------------------------------------------------------
class CGameClient : public CBaseClient, public CClientFrameManager
{
public:
	CGameClient(int slot, CBaseServer *pServer);
	~CGameClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);
	
	void PacketStart	(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );
	
	void FileReceived( const char *fileName, unsigned int transferID );
	void FileRequested(const char *fileName, unsigned int transferID );
	void FileDenied( const char *fileName, unsigned int transferID );
	void FileSent( const char *fileName, unsigned int transferID );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );

	// IClient interface
	void	Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, int clientChallenge );
	void	Inactivate( void );
	void	Reconnect( void );
	void	Disconnect( PRINTF_FORMAT_STRING const char *reason, ... );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate( int nUpdateRate, bool bForce );
	
	virtual	bool	IsHearingClient( int index ) const;
	virtual	bool	IsProximityHearingClient( int index ) const;

	void	Clear( void );

	bool	SendNetMsg(INetMessage &msg, bool bForceReliable = false);
	bool	ExecuteStringCommand( const char *s );

public: // IClientMessageHandlers
	
	PROCESS_CLC_MESSAGE( ClientInfo );
	PROCESS_CLC_MESSAGE( Move );
	PROCESS_CLC_MESSAGE( VoiceData );
	PROCESS_CLC_MESSAGE( RespondCvarValue );
	PROCESS_CLC_MESSAGE( FileCRCCheck );
	PROCESS_CLC_MESSAGE( FileMD5Check );
#if defined( REPLAY_ENABLED )
	PROCESS_CLC_MESSAGE( SaveReplay );
#endif
	PROCESS_CLC_MESSAGE( CmdKeyValues );
	
public:

	void	UpdateUserSettings( void );
	bool	UpdateAcknowledgedFramecount(int tick);

	void	WriteGameSounds( bf_write &buf );
	bool	SetSignonState(int state, int spawncount);
	void	SendSnapshot( CClientFrame *pFrame );
	bool	ShouldSendMessages( void );
	bool	CheckConnect( void );
	void	SpawnPlayer( void );
	bool	SendSignonData( void );
	void	ActivatePlayer( void );
	
	void	SetupPackInfo( CFrameSnapshot *pSnapshot );
	void	SetupPrevPackInfo();
	
	void	DownloadCustomizations();
	void	WriteViewAngleUpdate( void );
	CClientFrame *GetDeltaFrame( int nTick );
	CClientFrame *GetSendFrame();
	void	SendSound( SoundInfo_t &sound, bool isReliable );
	void	GetReplayData( int& ticks, int& entity);
	bool	IgnoreTempEntity( CEventInfo *event );
	const CCheckTransmitInfo* GetPrevPackInfo();
			

private:
	bool	IsEngineClientCommand( const CCommand &args ) const;
	int		FillSoundsMessage( SVC_Sounds &msg );
				
public:

	bool								m_bVoiceLoopback; // if true, client wants own voice loopback
	CBitVec< ABSOLUTE_PLAYER_LIMIT >	m_VoiceStreams;	  // Which other clients does this guy's voice stream go to?
	CBitVec< ABSOLUTE_PLAYER_LIMIT >	m_VoiceProximity; // Should we use proximity for this guy?

	int						m_LastMovementTick;	// for move commands

	int						m_nSoundSequence;	// increases with each reliable sound

	// Identity information.
	edict_t					*edict;				// EDICT_NUM(clientnum+1)
	CUtlVector<SoundInfo_t>	m_Sounds;			// game sounds
		
	const edict_t			*m_pViewEntity;		// View Entity (camera or the client itself)

	CClientFrame			*m_pCurrentFrame;	// last added frame

	CCheckTransmitInfo		m_PackInfo;

	bool					m_bIsInReplayMode;
	CCheckTransmitInfo		m_PrevPackInfo;		// Used to speed up CheckTransmit.
	CBitVec<MAX_EDICTS>		m_PrevTransmitEdict;

#if defined( REPLAY_ENABLED )
	float					m_flLastSaveReplayTime;
#endif
};

#endif // SV_CLIENT_H
