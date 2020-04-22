//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVCLIENT_H
#define HLTVCLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclient.h"

class CHLTVServer;

class CHLTVClient : public CBaseClient
{
public:
	CHLTVClient(int slot, CBaseServer *pServer);
	virtual ~CHLTVClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);

	void PacketStart(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );

	void FileReceived( const char *fileName, unsigned int transferID );
	void FileRequested(const char *fileName, unsigned int transferID );
	void FileDenied(const char *fileName, unsigned int transferID );
	void FileSent(const char *fileName, unsigned int transferID );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );
	
	// IClient interface
	bool	ExecuteStringCommand( const char *s );
	void	SpawnPlayer( void );
	bool	ShouldSendMessages( void );
	void	SendSnapshot( CClientFrame * pFrame );
	bool	SendSignonData( void );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate(int udpaterate, bool bForce);
	void	UpdateUserSettings();
	
public: // IClientMessageHandlers
	
	PROCESS_NET_MESSAGE( SetConVar );
	PROCESS_CLC_MESSAGE( ClientInfo );
	PROCESS_CLC_MESSAGE( Move );
	PROCESS_CLC_MESSAGE( VoiceData );
	PROCESS_CLC_MESSAGE( ListenEvents );
	PROCESS_CLC_MESSAGE( RespondCvarValue );
	PROCESS_CLC_MESSAGE( FileCRCCheck );
	PROCESS_CLC_MESSAGE( FileMD5Check ) { return true; }
	PROCESS_CLC_MESSAGE( SaveReplay );

public:
	CClientFrame *GetDeltaFrame( int nTick );
	
public:
	int		m_nLastSendTick;	// last send tick, don't send ticks twice
	double	m_fLastSendTime;	// last net time we send a packet
	char	m_szPassword[64];	// client password
	double	m_flLastChatTime;	// last time user send a chat text
	bool	m_bNoChat;			// if true don't send chat message to this client
	char	m_szChatGroup[64];	// client password
	CHLTVServer *m_pHLTV;
};


#endif // HLTVCLIENT_H
