//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef CLIENT_H
#define CLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "NetChannel.h"

class CNetworkClient : public IConnectionlessPacketHandler, public INetworkMessageHandler, public ILookupChannel
{
public:
	CNetworkClient();
	virtual ~CNetworkClient();

	bool	Init( int nListenPort );
	void	Shutdown();

	bool	Connect( const char *server, int port );
	void	Disconnect();

	// IConnectionlessPacketHandler
	virtual bool ProcessConnectionlessPacket( CNetPacket *packet );	// process a connectionless packet

	// INetworkMessageHandler
	virtual void	OnConnectionClosing( INetChannel *channel, char const *reason );
	virtual void	OnConnectionStarted( INetChannel *channel );

	virtual void	OnPacketStarted( int inseq, int outseq );
	virtual void	OnPacketFinished();

	// ILookupChannel
    virtual INetChannel *FindNetChannel( const netadr_t& from ) ;

	INetChannel *GetNetChannel()
	{
		return &m_NetChan;
	}

	void ReadPackets();
	void SendUpdate();

	CUDPSocket *m_pSocket;
	
	CNetChannel		m_NetChan;
	bool			m_bConnected;
};

#endif // CLIENT_H
