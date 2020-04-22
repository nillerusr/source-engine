//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#if !defined( SOCKET_H )
#define SOCKET_H
#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"
#include "MsgBuffer.h"
#include "utlvector.h"

#include <stdio.h>

class CMsgBuffer;
class CSocket;
class IGameList;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	char	packetID;
} SPLITPACKET;
#pragma pack()

#define MAX_PACKETS 16 // 4 bits for the packet count, so only 
#define MAX_RETRIES 2 // the number of fragments from other packets to drop before we declare the outstanding
					  // fragment lost :)

//-----------------------------------------------------------------------------
// Purpose: Instances a message handler for incoming messages.
//-----------------------------------------------------------------------------
class CMsgHandler
{
public:
	enum
	{
		MAX_HANDLER_STRING = 64
	};

	typedef enum
	{
		MSGHANDLER_ALL = 0,
		MSGHANDLER_BYTECODE,
		MSGHANDLER_STRING
	} HANDLERTYPE;

	// Construction
							CMsgHandler( HANDLERTYPE type, void *typeinfo = 0 );
	virtual					~CMsgHandler( void );

	// Message received, process it
	virtual	bool			Process( netadr_t *from, CMsgBuffer *msg ) = 0;

	// For linking togethr handler chains
	virtual CMsgHandler		*GetNext( void ) const;
	virtual void			SetNext( CMsgHandler *next );

	// Access/set associated socket
	virtual CSocket			*GetSocket( void ) const;
	virtual void			SetSocket( CSocket *socket );

private:
	// Internal message received, crack type info and check it before calling process
	bool					ProcessMessage( netadr_t *from, CMsgBuffer *msg );

	// Opaque pointer to underlying recipient class
	IGameList				*m_pBaseObject;

	// Next handler in chain
	HANDLERTYPE				m_Type;
	unsigned char			m_ByteCode;
	char					m_szString[ MAX_HANDLER_STRING ];

	// Next handler in chain
	CMsgHandler				*m_pNext;
	// Associated socket
	CSocket					*m_pSocket;

	friend CSocket;
};

//-----------------------------------------------------------------------------
// Purpose: Creates a non-blocking, broadcast capable, UDP socket.  If port is
//  specified, binds it to listen on that port, otherwise, chooses a random port.
//-----------------------------------------------------------------------------
class CSocket
{
public:
	// Construction/destruction
							CSocket( const char *socketname, int port = -1 );
	virtual					~CSocket( void );

	// Adds the message hander to the head of the sockets handler chain
	virtual void			AddMessageHandler( CMsgHandler *handler );
	// Removes the specified message handler
	virtual void			RemoveMessageHandler( CMsgHandler *handler );

	// Send the message to the recipient, if msg == NULL, use the internal message buffer
	virtual int				SendMessage( netadr_t *to, CMsgBuffer *msg = NULL );
	// Broadcast the message on the specified port, if msg == NULL use the internal message buffer
	virtual int				Broadcast( int port, CMsgBuffer *msg = NULL );
	// Get access to the internal message buffer
	virtual CMsgBuffer		*GetSendBuffer( void );
	// Called once per frame to check for new data
	virtual void			Frame( void );
	// Check whether the socket was created and set up properly
	virtual bool			IsValid( void ) const;
	// Get the address this socket is bound to
	virtual const netadr_t	*GetAddress( void );

	// Allow creating object to store a 32 bit value and retrieve it
	virtual void			SetUserData( unsigned int userData );
	virtual unsigned int	GetUserData(void ) const;

	// Allow other objects to get the raw socket interger
	virtual int				GetSocketNumber( void ) const;
	// Called when FD_ISSET noted that the socket has incoming data
	virtual bool			ReceiveData( void );
	// Called to get current time
	static float			GetClock( void );

private:
	const char				*m_pSocketName;
	// Socket listen address
	bool					m_bValid;
	// Socket IP address
	netadr_t				m_Address;
	// Has the IP address been resolved
	bool					m_bResolved;
	// Internal message buffers
	CUtlVector<CMsgBuffer>	m_MsgBuffers;
	CMsgBuffer				m_SendBuffer;
	// critical section for accessing buffers
	void					*m_pBufferCS;
	// One or more listeners for the incoming message
	CMsgHandler				*m_pMessageHandlers;
	// Winsock socket number
	int						m_Socket;
	// User 32 bit value
	unsigned int			m_nUserData;
	// Socket to which non Broadcast SendMessage was directed.  The socket will wait for a response
	//  from that exact address
	netadr_t				m_ToAddress;
	// Set to true if the send was a Broadcast, and therefore from != to address is okay
	bool					m_bBroadcastSend;

	int m_iTotalPackets; // total number of packets in a fragment
	int m_iCurrentPackets; // current packet count
	int m_iSeqNo; // the sequence number of the packet
	int m_iRetries;
	CMsgBuffer m_CurPacket[MAX_PACKETS]; // store for the packet
};

#endif // SOCKET_H