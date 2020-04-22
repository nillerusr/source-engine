//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//
#ifndef SV_RCON_H
#define SV_RCON_H

#ifdef _WIN32
#pragma once
#endif

#include "sv_main.h"
#include "netmessages.h"
#include "net.h"
#include "client.h"
#include "utlvector.h"
#include "utllinkedlist.h"
#include "netadr.h"
#include "sv_remoteaccess.h"
#include "socketcreator.h"
#include "igameserverdata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define RCON_MAX_OUTSTANDING_SENDS 100 // max packets to queue before dropping connection

//-----------------------------------------------------------------------------
// container class to handle network streams
//-----------------------------------------------------------------------------
class CRConServer : public ISocketCreatorListener
{
public:
	CRConServer( const char *netAddress );
	CRConServer();
	~CRConServer();

	void SetAddress( const char *netAddress );
	void SetPassword( const char *pPassword );
	bool HasPassword() const;
	bool IsPassword( const char *pPassword ) const;
	bool CreateSocket();
	void RunFrame();
	bool IsConnected();
	void FinishRedirect( const char *msg, const netadr_t &adr );
	bool HandleFailedRconAuth( const netadr_t &adr );
	void SetRequestID( ra_listener_id listener, int iRequestID );
	bool BCloseAcceptedSocket( ra_listener_id nIndex );

	// Allows a server to request a listening client to connect to it
	bool ConnectToListeningClient( const netadr_t &adr, bool bSingleSocket );

	// Inherited from ISocketCreatorListener
	virtual bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr ); 
	virtual void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData ); 
	virtual void OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData );

private:
	struct ConnectedRConSocket_t
	{
		bool			authed;
		int				lastRequestID;
		ra_listener_id	listenerID;
		CUtlBuffer		packetbuffer; // data buffer containing pending incoming data for this connection
		CUtlLinkedList< CUtlBuffer > m_OutstandingSends; // packets pending to be send (queued because of WSAEWOULDBLOCK )
	};

	struct FailedRCon_t
	{
		int								badPasswordCount;
		netadr_t						adr;
		CUtlLinkedList< float >			badPasswordTimes;
		bool operator==( const FailedRCon_t &rhs ) const { return ( adr.CompareAdr( rhs.adr) == 0 ); }
		bool operator<( const FailedRCon_t &rhs ) const;
	};

	ConnectedRConSocket_t* GetSocketData( int nIndex );
	void ProcessAccept();

	// NOTE: This function can remove elements. If calling it from a loop,
	// always iterate over accepted sockets backwards to avoid problems.
	bool SendRCONResponse( int nIndex, const void *data, int len, bool fromQueue = false );	

	CSocketCreator m_Socket;
	CUtlVector< FailedRCon_t >	m_failedRcons;
	CUtlString m_Password;
	netadr_t m_Address;
	bool m_bSocketDeleted;
};


inline CRConServer::ConnectedRConSocket_t* CRConServer::GetSocketData( int nIndex )
{
	return (ConnectedRConSocket_t*)m_Socket.GetAcceptedSocketData( nIndex );
}

CRConServer & RCONServer();
#ifdef ENABLE_RPT
CRConServer & RPTServer();
#endif // ENABLE_RPT

#endif // SV_RCON_H
