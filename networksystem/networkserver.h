//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef NETWORKSERVER_H
#define NETWORKSERVER_H

#include "networksystem/inetworksystem.h"
#include "netchannel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPlayer;


//-----------------------------------------------------------------------------
// Server class
//-----------------------------------------------------------------------------
class CNetworkServer : public IConnectionlessPacketHandler, public INetworkMessageHandler, public ILookupChannel
{
public:
	CNetworkServer( );
	virtual ~CNetworkServer();

	bool Init( int nServerPort );
	void Shutdown();

	// IConnectionlessPacketHandler
	virtual bool	ProcessConnectionlessPacket( CNetPacket *packet );	// process a connectionless packet
	
	// INetworkMessageHandler
	virtual void	OnConnectionClosing( INetChannel *channel, char const *reason );
	virtual void	OnConnectionStarted( INetChannel *channel );

	virtual void	OnPacketStarted( int inseq, int outseq );
	virtual void	OnPacketFinished();

	// ILookupChannel
    virtual CNetChannel *FindNetChannel( const netadr_t& from ) ;

	void ReadPackets();
	void SendUpdates();
	void AcceptConnection( const netadr_t& remote );

	CPlayer *FindPlayerByAddress( const netadr_t& adr );
	CPlayer *FindPlayerByNetChannel( INetChannel *chan );

	CUDPSocket *m_pSocket;

	CUtlVector< CPlayer * >	m_Players;
};


//-----------------------------------------------------------------------------
// Represents a connected player to the server
//-----------------------------------------------------------------------------
class CPlayer
{
public:
	CPlayer( CNetworkServer *server, netadr_t& remote );

	void			SendUpdate();

	const netadr_t		&GetRemoteAddress()
	{
		return m_NetChan.GetRemoteAddress();
	}

	char const *GetName()
	{
		return "Foozle";
	}

	void			Shutdown();

	CNetChannel		m_NetChan;
	bool			m_bMarkedForDeletion;
};


#endif // NETWORKSERVER_H

