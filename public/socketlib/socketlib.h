//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: master header file for socketlib.lib
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#ifndef SOCKETLIB_H
#define SOCKETLIB_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"
#include "utlbuffer.h"

typedef int64 SocketHandle_t;


//===========================================================================
// Error Codes used by socketlib
//===========================================================================
enum SocketErrorCode_t
{
    SOCKET_SUCCESS = 0,
    SOCKET_ERR_OPERATION_NOT_SUPPORTED,
    SOCKET_ERR_CREATE_FAILED,
    SOCKET_ERR_READ_OPERATION_FAILED,
    SOCKET_ERR_WRITE_OPERATION_FAILED,

    SOCKET_ERR_CONNECT_FAILED,
    SOCKET_ERR_LISTEN_FAILED,
    SOCKET_ERR_ACCEPT_FAILED,
    SOCKET_ERR_POLLING_OPERATION_FAILED,
    SOCKET_ERR_BIND_OPERATION_FAILED,

    SOCKET_ERR_ENABLE_NON_BLOCKING_MODE_FAILED,
    SOCKET_ERR_HOST_NOT_FOUND,
    SOCKET_ERR_GENERAL_SOCKET_ERROR,
    SOCKET_ERR_READ_OPERATION_WOULD_BLOCK,
    SOCKET_ERR_WRITE_OPERATION_WOULD_BLOCK,

    SOCKET_ERR_CONNECTION_CLOSED,
    SOCKET_ERR_CONNECTION_RESET,
    SOCKET_ERR_NO_INCOMING_CONNECTIONS,
    SOCKET_ERR_NO_AVAILABLE_ENDPOINTS,
    
    SOCKET_ERR_BAD_USER_DATA,
    SOCKET_ERR_INVALID_CONNECTION,
    SOCKET_ERR_CANT_WRITE,
    
    SOCKET_ERR_MIXING_PACKET_SENDS,
    SOCKET_ERR_PARTIAL_PACKET_OVERFLOW,
};


const int MAX_SERVER_CONNECTIONS = 4;							// Maximum number of concurrent connections allowed to a server.
															    // In the future, this can be set higher and class CSocketConnection should use a growable array of Sockets.

const int MAX_SERVER_CONNECTION_BACKLOG = 16;					// Maximum number of connections allowed in the listening backlog.

const int INVALID_ENDPOINT_INDEX = -1;							// Represents an invalid endpoint index.



enum ConnectionType_t		    // Type of connection: client or server.
{
    CT_INDETERMINATE = 0,
    CT_CLIENT,
    CT_SERVER,
};

enum SocketProtocol_t			// Protocol to use for the socket.
{
    SP_INDETERMINATE = 0,
    SP_UDP,
    SP_VDP,
    SP_TCP,
};

enum SocketState_t				// State of a given socket.
{
    SSTATE_UNINITIALIZED = 0,		// Socket is in completely uninitialized state.
    SSTATE_LISTENING,				// Server socket is listening for connections.
    SSTATE_CONNECTION_IN_PROGRESS,	// Socket is initialized and in the process of connecting to a client/server.
    SSTATE_CONNECTED,				// Socket is initialized and connected to a client/server.
};


//-----------------------------------------------------------------------
// Platform and build specific defines, change these as needed
//-----------------------------------------------------------------------

#if defined( PLATFORM_X360 )
	#define VDP_SUPPORT_ENABLED 1
#else 
	#define VDP_SUPPORT_ENABLED 0
#endif

#if defined( _DEBUG )
	#define UNSECURE_SOCKETS_ENABLED 1
#else
	#define UNSECURE_SOCKETS_ENABLED 0
#endif




// ----------------------------------------------------------------------------
// Basic platform-specific socket definitions go here to avoid 
// polluting other source files with common #defines.
// ----------------------------------------------------------------------------

#if defined( PLATFORM_X360 )
#include <xtl.h>
#include <winsockx.h>
#elif defined( PLATFORM_WINDOWS_PC )
#include <winsock2.h>
#else 
#error No build platform macro (PLATFORM_*) defined
#endif

inline SocketHandle_t GetSocketHandle( SOCKET socket )
{
    return static_cast<SocketHandle_t>( socket );
}

inline SOCKET GetPlatformSocket( SocketHandle_t handle )
{
    return static_cast<SOCKET>( handle );
}

static const SocketHandle_t InvalidSocketHandle = static_cast<SocketHandle_t>( INVALID_SOCKET );


// ----------------------------------------------------------------------------
// CSocketConnection
// Represents a client or server network connection.
//
// todo: Rework connection class to treat endpoints as standalone objects and unify
// interface with named pipes.
// ----------------------------------------------------------------------------
class CSocketConnection
{
public:
    CSocketConnection();
    ~CSocketConnection();

    SocketErrorCode_t	Init( ConnectionType_t connectionType, SocketProtocol_t socketProtocol );		// Initializes the connection class as either client or server with specified protocol.
																										// Does not acquire any system resources.

    void				Cleanup();																		// Cleans up the class and restores it to the default uninitialized state.
    
    SocketErrorCode_t	Listen( uint16 localPort, int numAllowedConnections );							// (Server-only) Listens for active client connections.

    SocketErrorCode_t	TryAcceptIncomingConnection( int *newEndpointIndex );							// (Server-only) Attempts to accept an incoming connection, if one is available.

    int					GetFirstAvailableListeningEndpoint();											// (Server-only) Gets the index of the first endpoint which is listening for connections.

    SocketState_t		GetListeningSocketState();														// (Server-only) Gets the status of the listening socket on a server.

    SocketErrorCode_t	ConnectToServer( const char *hostName, uint16 hostPort );						// (Client-only) Connects to a remote host using endpoint 0.
																										// In most cases, the status of endpoint 0 will be SSTATE_CONNECTION_IN_PROGRESS.
																										// The caller must periodically call PollClientConnectionState until the socket is connected before it is usable.

    SocketErrorCode_t	PollClientConnectionState( bool *isConnected );									// (Client-only) Checks the status of endpoint 0 to see if it is connected successfully to a server.
    
    SocketState_t		GetEndpointSocketState( int endpointIndex );									// Gets the status of the specified endpoint. (Clients only use endpoint 0.)

    SocketErrorCode_t	CanReadFromEndpoint( int endpointIndex, bool *canRead );						// Gets a value indicating whether the endpoint can be successfully read from.

    SocketErrorCode_t	CanWriteToEndpoint( int endpointIndex, bool *canWrite );						// Gets a value indicating whether the endpoint can be successfully written to.

    SocketErrorCode_t	ReadFromEndpoint( int endpointIndex, byte *destinationBuffer, int bufferSize, int *bytesRead );		// Reads data from the specified endpoint.

    SocketErrorCode_t	WriteToEndpoint( int endpointIndex, byte *sourceBuffer, int bufferSize, int *bytesWritten );		// Writes data to the specified endpoint.

    const char*			GetLastSystemErrorString() const;												// Gets a string containing the last underlying system error reported.
																										// Only valid immediately after a function returns an unsuccessful error code.
    const char*			GetLastErrorString() const;														// Gets a string containing the last underlying system error reported.
    
	SocketHandle_t		GetListeningSocket() const;

	SocketHandle_t		GetEndpointSocket( int nEndpointIndex ) const;

    void				ResetEndpoint( int endpointIndex );

	SocketErrorCode_t	SetSocketOpt(  int endpointIndex, int level, int optname, const void *optval, int optlen );

private:
    SocketHandle_t		CreateNewSocket();

    SocketHandle_t		m_ListeningSocket;
    SocketHandle_t		m_EndpointSockets[ MAX_SERVER_CONNECTIONS ];

    SocketState_t		m_ListeningSocketState;
    SocketState_t		m_EndpointStates[ MAX_SERVER_CONNECTIONS ];

    ConnectionType_t	m_ConnectionType;
    SocketProtocol_t	m_SocketProtocol;

    SocketErrorCode_t	m_LastError;
    int					m_LastSystemError;
};


//-----------------------------------------------------------------------------
// Inline functions from CSocketConnection
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
inline SocketHandle_t CSocketConnection::GetListeningSocket() const
{
	return m_ListeningSocket;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
inline SocketHandle_t CSocketConnection::GetEndpointSocket( int nEndpointIndex ) const
{
    AssertMsg( nEndpointIndex >= 0 && nEndpointIndex < MAX_SERVER_CONNECTIONS, "Endpoint index out of range" );

	return m_EndpointSockets[ nEndpointIndex ];
}


// ----------------------------------------------------------------------------
// Start up and shutdown functions
// Calls to these functions have to bracket any usage of this socket code
// ----------------------------------------------------------------------------
void SocketLibInit();
void SocketLibShutdown();

// Handy conversion to text of those nasty HRESULT codes
const char* ConvertWinsockErrorToString( int errorCode );
const char* ConvertSocketLibErrorToString( SocketErrorCode_t errorCode );



// Bitwise representation of a network message header.
struct MessageHeader_t
{
    uint32 m_nLength;
};

// Byte swaps (in-place) a message header between system byte-order and network byte-order.    
void ByteSwapInPlaceMessageHeader( MessageHeader_t* messageHeader );

// ----------------------------------------------------------------------------
// Signature of a callback invoked once per message parsed by the CSocketMessageBuilder::FeedData function.    
//	header = Header of the message read.
//	message = Message payload (m_nLength is given by the header)
//	userContext = Context provided by caller to FeedData
// ----------------------------------------------------------------------------
typedef void ( *NetworkMessageHandler )( const MessageHeader_t& header, const byte* message, void* userContext );

// ----------------------------------------------------------------------------
// Helper class to parse messages from a stream source.
// This class maintains state between successive calls to FeedData so that partial messages
// can be stored until fully parsed and dispatched.
// ----------------------------------------------------------------------------
class CSocketMessageBuilder
{
public:
	CSocketMessageBuilder( int initialSize = 0, int growSize = 0 );
	~CSocketMessageBuilder();
	
	void SetMaxExpectedMsgSize( int expectedSize );

    // Parses the given stream data and fires a callback for each full message parsed.
    // This function has the side-effect of updating the CSocketMessageBuilder's internal parsing state
    // so that partial messages can be glued together.
    void FeedData( const void *data, int dataLength, NetworkMessageHandler networkMessageHandlerFunc, void *userContext );

	void AssignConnection( CSocketConnection* pConnection, int endpoint );
	SocketErrorCode_t SendDataPacket(  const void* RESTRICT data, int dataLength );
	SocketErrorCode_t SendDataPacket(  CSocketConnection* pConnection, int endpoint, const void* RESTRICT data, int dataLength );

	SocketErrorCode_t BeginSendPartialDataPacket(  uint32 totalSize, const void* RESTRICT data, int dataLength );
	SocketErrorCode_t BeginSendPartialDataPacket(  CSocketConnection* pConnection, int endpoint, uint32 totalSize, const void* RESTRICT data, int dataLength );

	bool WaitForIncomingMessage( DWORD timeOutValue );
	void* GetIncomingMessageData();
	uint32 GetIncomingMessageLen();
	
	void FeedDataManual( const void* RESTRICT data, int dataLength );
	bool HasCompleteMessageManual( );
	bool GetCurrentMessageManual( void*& msgData, uint32& msgSize );
	bool DiscardCurrentMessageManual();
	
	bool IsSendingPartialMessage()			{ return m_bSendingPartialMessage; }
	int32 PartialMessageBytesRemaining()	{ return m_bSendingPartialMessage ? m_PartialMessageBytesTotal - m_PartialMessageBytesSent : 0; }

private:
    MessageHeader_t		m_MessageHeader;
    uint32				m_nHeaderBytesRead;
    uint32				m_nMessageBytesRead;
    
    CSocketConnection*	m_pConnection;
    int					m_nConnectionEndpoint;
    
    CUtlBuffer			m_MessageData;
    
    uint32				m_PartialMessageBytesSent;			// used to combine multiple calls to SendDataPacket/BeginSendPartialDataPacket into one message packet
    uint32				m_PartialMessageBytesTotal;
    bool				m_bSendingPartialMessage;
  
	int					m_nRecvBufSize;
	byte*				m_pRecvBuf;
	bool				m_bSwappedHeader;
  
   
};


#endif // SOCKETLIB_H

