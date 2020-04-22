//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef NETWORKSYSTEM_H
#define NETWORKSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "networksystem/inetworksystem.h"
#include "tier1/utlvector.h"
#include "tier1/bitbuf.h"
#include "sm_protocol.h"
#include "networksystem/inetworkmessage.h"
#include "tier1/netadr.h"
#include "tier1/utlstring.h"
#include "tier2/tier2.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CNetworkServer;
class CNetworkClient;
class IConnectionlessPacketHandler;
class CNetChannel;
enum SystemNetworkMessageType_t;


//-----------------------------------------------------------------------------
// Global interfaces
//-----------------------------------------------------------------------------
class CNetworkSystem;
extern CNetworkSystem *g_pNetworkSystemImp;


//-----------------------------------------------------------------------------
// Implementation of the network system
//-----------------------------------------------------------------------------
class CNetworkSystem : public CTier2AppSystem< INetworkSystem >
{
	typedef CTier2AppSystem< INetworkSystem > BaseClass;

public:
	// Constructor, destructor
	CNetworkSystem();
	virtual ~CNetworkSystem();

	// Inherited from IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from INetworkSystem
	virtual bool RegisterMessage( INetworkMessage *msg );
	virtual bool StartServer( unsigned short nServerListenPort );
	virtual void ShutdownServer( );
	virtual void ServerReceiveMessages();
	virtual void ServerSendMessages();

	virtual bool StartClient( unsigned short nClientListenPort );
	virtual void ShutdownClient( );
	virtual void ClientSendMessages();
	virtual void ClientReceiveMessages();
	virtual INetChannel* ConnectClientToServer( const char *pServer, int nServerListenPort );
	virtual void DisconnectClientFromServer( INetChannel* pChan );
	virtual NetworkEvent_t *FirstNetworkEvent( );
	virtual NetworkEvent_t *NextNetworkEvent( );
	virtual const char* GetLocalHostName( void ) const;
	virtual const char* GetLocalAddress( void ) const;

	// Methods internal for use in networksystem library

	// Method to allow systems to add network events received
	NetworkEvent_t* CreateNetworkEvent( int nSizeInBytes );
	template< class T > T* CreateNetworkEvent();
	bool IsNetworkEventCreated();

	// Finds a network message given a particular message type
	INetworkMessage* FindNetworkMessage( int group, int type );

	// Returns the number of bits to encode the type + group with
	int GetTypeBitCount() const;
	int GetGroupBitCount() const;

	// Returns the current time
	float GetTime( void );

	// Converts a string to a socket address 
	bool StringToSockaddr( const char *s, struct sockaddr *sadr );

	// Queues up a network packet
	void EnqueueConnectionlessNetworkPacket( CNetPacket *pPacket, IConnectionlessPacketHandler *pHandler );
	void EnqueueNetworkPacket( CNetPacket *pPacket, CNetChannel *pNetChannel );

private:
	struct PacketInfo_t
	{
		CNetPacket *m_pPacket;
		IConnectionlessPacketHandler *m_pHandler;
		CNetChannel *m_pNetChannel;
	};

	// Network event iteration helpers
	bool StartProcessingNewPacket();
	bool AdvanceProcessingNetworkPacket( );
	void CleanupNetworkMessages( );

	bool m_bWinsockInitialized : 1;
	bool m_bNetworkEventCreated : 1;
	bool m_bInMidPacket : 1;
	int m_nTypeBits;
	int m_nGroupBits;
	netadr_t m_LocalAddress;
	CUtlString m_LocalAddressString;
	CUtlString m_LocalHostName;
	CNetworkServer *m_pServer;
	CNetworkClient *m_pClient;
	unsigned char m_EventMessageBuffer[256];
	CUtlVector<PacketInfo_t> m_PacketQueue;
	int m_nProcessingPacket;
	CUtlVector<INetworkMessage*> m_NetworkMessages;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
template< class T > 
T* CNetworkSystem::CreateNetworkEvent()
{
	// Increase the size of m_EventMessageBuffer if this assertion fails
	COMPILE_TIME_ASSERT( sizeof(T) <= sizeof( m_EventMessageBuffer ) );
	return (T*)CreateNetworkEvent( sizeof(T) );
}

	
//-----------------------------------------------------------------------------
// Returns the number of bits to encode the type + group with
//-----------------------------------------------------------------------------
inline int CNetworkSystem::GetTypeBitCount() const
{
	return m_nTypeBits;
}

inline int CNetworkSystem::GetGroupBitCount() const
{
	return m_nGroupBits;
}

	
//-----------------------------------------------------------------------------
// Writes a system network message
//-----------------------------------------------------------------------------
inline void WriteSystemNetworkMessage( bf_write &msg, SystemNetworkMessageType_t type )
{
	msg.WriteUBitLong( net_group_networksystem, g_pNetworkSystemImp->GetGroupBitCount() );
	msg.WriteUBitLong( type, g_pNetworkSystemImp->GetTypeBitCount() );
}

inline void WriteNetworkMessage( bf_write &msg, INetworkMessage *pNetworkMessage )
{
	msg.WriteUBitLong( pNetworkMessage->GetGroup(), g_pNetworkSystemImp->GetGroupBitCount() );
	msg.WriteUBitLong( pNetworkMessage->GetType(), g_pNetworkSystemImp->GetTypeBitCount() );
}

#endif // NETWORKSYSTEM_H

