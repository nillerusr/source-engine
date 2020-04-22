//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Purpose: A low level API similar to Berkeley socket, to send messages
// between hosts over the Steam network and addressed using Steam IDs.
//
//=============================================================================

#ifndef ISTEAMNETWORKINGSOCKETS
#define ISTEAMNETWORKINGSOCKETS
#ifdef _WIN32
#pragma once
#endif

#include "steamnetworkingtypes.h"

// #KLUDGE! This is so we don't have to link with steam_api.lib
#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>

//-----------------------------------------------------------------------------
/// Lower level networking interface that more closely mirrors the standard
/// Berkeley sockets model.  Sockets are hard!  You should probably only use
/// this interface under the existing circumstances:
///
/// - You have an existing socket-based codebase you want to port, or coexist with.
/// - You want to be able to connect based on IP address, rather than (just) Steam ID.
/// - You need low-level control of bandwidth utilization, when to drop packets, etc.
///
/// Note that neither of the terms "connection" and "socket" will correspond
/// one-to-one with an underlying UDP socket.  An attempt has been made to
/// keep the semantics as similar to the standard socket model when appropriate,
/// but some deviations do exist.
class ISteamSocketNetworking
{
public:

	/// Creates a "server" socket that listens for clients to connect to, either by calling
	/// ConnectSocketBySteamID or ConnectSocketByIPv4Address.
	///
	/// nSteamConnectVirtualPort specifies how clients can connect to this socket using
	/// ConnectBySteamID.  A negative value indicates that this functionality is
	/// disabled and clients must connect by IP address.  It's very common for applications
	/// to only have one listening socket; in that case, use zero.  If you need to open
	/// multiple listen sockets and have clients be able to connect to one or the other, then
	/// nSteamConnectVirtualPort should be a small integer constant unique to each listen socket
	/// you create.
	///
	/// If you want clients to connect to you by your IPv4 addresses using
	/// ConnectByIPv4Address, then you must set nPort to be nonzero.  Steam will
	/// bind a UDP socket to the specified local port, and clients will send packets using
	/// ordinary IP routing.  It's up to you to take care of NAT, protecting your server
	/// from DoS, etc.  If you don't need clients to connect to you by IP, then set nPort=0.
	/// Use nIP if you wish to bind to a particular local interface.  Typically you will use 0,
	/// which means to listen on all interfaces, and accept the default outbound IP address.
	/// If nPort is zero, then nIP must also be zero.
	///
	/// A SocketStatusCallback_t callback when another client attempts a connection.
	virtual HSteamListenSocket CreateListenSocket( int nSteamConnectVirtualPort, uint32 nIP, uint16 nPort ) = 0;

	/// Creates a connection and begins talking to a remote destination.  The remote host
	/// must be listening with the appropriate call to CreateListenSocket.
	///
	/// Use ConnectBySteamID to connect using the SteamID (client or game server) as the network address.
	/// Use ConnectByIPv4Address to connect by IP address.
	///
	/// On success, a SocketStatusCallback_t callback is triggered.
	/// On failure or timeout, a SocketStatusCallback_t callback with a failure code in m_eSNetSocketState
	virtual HSteamNetConnection ConnectBySteamID( CSteamID steamIDTarget, int nVirtualPort, int nTimeoutSec ) = 0;
	virtual HSteamNetConnection ConnectByIPv4Address( uint32 nIP, uint16 nPort, int nTimeoutSec ) = 0;

	/// Accept an incoming connection that has been received on a listen socket.
	///
	/// When a connection attempt is received (perhaps after a few basic handshake
	/// packets have been exchanged to prevent trivial spoofing), a connection interface
	/// object is created in the k_ESteamNetworkingConnectionState_Connecting state
	/// and a SteamNetConnectionStatusChangedCallback_t is posted.  At this point, your
	/// application MUST either accept or close the connection.  (It may not ignore it.)
	/// Accepting the connection will transition it into the connected state.
	///
	/// You should take action within a few seconds, because accepting the connection is
	/// what actually sends the reply notifying the client that they are connected.  If you
	/// delay taking action, from the client's perspective it is the same as the network
	/// being unresponsive, and the client may timeout the connection attempt.  In other
	/// words, the client cannot distinguish between a delay caused by network problems
	/// and a delay caused by the application.
	///
	/// This means that if your application goes for more than a few seconds without
	/// processing callbacks, then there is a chance that a client may attempt to connect
	/// in that interval, and timeout.
	///
	/// If the application does not respond to the connection attempt in a timely manner,
	/// and we stop receiving communication from the client, the connection attempt will
	/// be timed out locally, transitioning the connection to the
	/// k_ESteamNetworkingConnectionState_ProblemDetectedLocally state.  The client may also
	/// close the connection before it is accepted and a transition to the
	/// k_ESteamNetworkingConnectionState_ClosedByPeer is also possible.
	///
	/// Returns k_EResultInvalidParam if the handle is invalid.
	/// Returns k_EResultInvalidState if the connection is not in the appropriate state.
	/// (Remember that the connection state could change in between the time that the
	/// notification being posted to the queue and when it is received by the application.)
	virtual EResult AcceptConnection( HSteamNetConnection hConn ) = 0;

	/// Disconnects from the remote host and invalidates the connection handle.
	/// Any unread data on the connection is discarded.
	///
	/// nReason is an application defined code that will be received on the other
	/// end and recorded (when possible) in backend analytics.  The value should
	/// come from a restricted range.  (See ESteamNetConnectionEnd.)  If you don't need
	/// to communicate any information to the remote host, and do not want analytics to
	/// be able to distinguish "normal" connection terminations from "exceptional" ones,
	/// You may pass zero, in which case the generic value of
	/// k_ESteamNetConnectionEnd_App_Generic will be used.
	///
	/// pszDebug is an optional human-readable diagnostic string that will be received
	/// by the remote host and recorded (when possible) in backend analytics.
	///
	/// If you wish to put the socket into a "linger" state, where an attempt is made to
	/// flush any remaining sent data, use bEnableLinger=true.  Otherwise reliable data
	/// is not flushed.
	///
	/// If the connection has already ended and you are just freeing up the
	/// connection interface, the reason code, debug string, and linger flag are
	/// ignored.
	virtual bool CloseConnection( HSteamNetConnection hPeer, int nReason, const char *pszDebug, bool bEnableLinger ) = 0;

	/// Destroy a listen socket, and all the client sockets generated by accepting connections
	/// on the listen socket.
	///
	/// pszNotifyRemoteReason determines what cleanup actions are performed on the client
	/// sockets being destroyed.  (See DestroySocket for more details.)
	///
	/// Note that if cleanup is requested and you have requested the listen socket bound to a
	/// particular local port to facilitate direct UDP/IPv4 connections, then the underlying UDP
	/// socket must remain open until all clients have been cleaned up.
	virtual bool CloseListenSocket( HSteamListenSocket hSocket, const char *pszNotifyRemoteReason ) = 0;

	/// Set connection user data.  Returns false if the handle is invalid.
	virtual bool SetConnectionUserData( HSteamNetConnection hPeer, int64 nUserData ) = 0;

	/// Fetch connection user data.  Returns -1 if handle is invalid
	/// or if you haven't set any userdata on the connection.
	virtual int64 GetConnectionUserData( HSteamNetConnection hPeer ) = 0;

	/// Set a name for the connection, used mostly for debugging
	virtual void SetConnectionName( HSteamNetConnection hPeer, const char *pszName ) = 0;

	/// Fetch connection user data.  Returns -1 if handle is invalid
	/// or if you haven't set any userdata on the connection.
	virtual void GetConnectionName( HSteamNetConnection hPeer, char *ppszName, int nMaxLen ) = 0;

	/// Send a message to the remote host on the connected socket.
	///
	/// eSendType determines the delivery guarantees that will be provided,
	/// when data should be buffered, etc.
	///
	/// Note that the semantics we use for messages are not precisely
	/// the same as the semantics of a standard "stream" socket.
	/// (SOCK_STREAM)  For an ordinary stream socket, the boundaries
	/// between chunks are not considered relevant, and the sizes of
	/// the chunks of data written will not necessarily match up to
	/// the sizes of the chunks that are returned by the reads on
	/// the other end.  The remote host might read a partial chunk,
	/// or chunks might be coalesced.  For the message semantics 
	/// used here, however, the sizes WILL match.  Each send call 
	/// will match a successful read call on the remote host 
	/// one-for-one.  If you are porting existing stream-oriented 
	/// code to the semantics of reliable messages, your code should 
	/// work the same, since reliable message semantics are more 
	/// strict than stream semantics.  The only caveat is related to 
	/// performance: there is per-message overhead to retain the 
	/// messages sizes, and so if your code sends many small chunks 
	/// of data, performance will suffer. Any code based on stream 
	/// sockets that does not write excessively small chunks will 
	/// work without any changes. 
	virtual EResult SendMessageToConnection( HSteamNetConnection hConn, const void *pData, uint32 cbData, ESteamNetworkingSendType eSendType ) = 0;

	/// Fetch the next available message(s) from the socket, if any.
	/// Returns the number of messages returned into your array, up to nMaxMessages.
	/// If the connection handle is invalid, -1 is returned.
	///
	/// The order of the messages returned in the array is relevant.
	/// Reliable messages will be received in the order they were sent (and with the
	/// same sizes --- see SendMessageToConnection for on this subtle difference from a stream socket).
	///
	/// FIXME - We're still debating the exact set of guarantees for unreliable, so this might change.
	/// Unreliable messages may not be received.  The order of delivery of unreliable messages
	/// is NOT specified.  They may be received out of order with respect to each other or
	/// reliable messages.  They may be received multiple times!
	///
	/// If any messages are returned, you MUST call Release() to each of them free up resources
	/// after you are done.  It is safe to keep the object alive for a little while (put it
	/// into some queue, etc), and you may call Release() from any thread.
	virtual int ReceiveMessagesOnConnection( HSteamNetConnection hConn, ISteamNetworkingMessage **ppOutMessages, int nMaxMessages ) = 0; 

	/// Same as ReceiveMessagesOnConnection, but will return the next message available
	/// on any client socket that was accepted through the specified listen socket.  Use
	/// ISteamNetworkingMessage::GetConnection to know which client connection.
	///
	/// Delivery order of messages among different clients is not defined.  They may
	/// be returned in an order different from what they were actually received.  (Delivery
	/// order of messages from the same client is well defined, and thus the order of the
	/// messages is relevant!)
	virtual int ReceiveMessagesOnListenSocket( HSteamListenSocket hSocket, ISteamNetworkingMessage **ppOutMessages, int nMaxMessages ) = 0; 

	/// Returns information about the specified connection.
	virtual bool GetConnectionInfo( HSteamNetConnection hConn, SteamNetConnectionInfo_t *pInfo ) = 0;

	/// Returns brief set of connection status that you might want to display
	/// to the user in game.
	virtual bool GetQuickConnectionStatus( HSteamNetConnection hConn, SteamNetworkingQuickConnectionStatus *pStats ) = 0;

	/// Returns detailed connection stats in text format.  Useful
	/// for dumping to a log, etc.
	///
	/// Returns:
	/// -1 failure (bad connection handle)
	/// 0 OK, your buffer was filled in and '\0'-terminated
	/// >0 Your buffer was either nullptr, or it was too small and the text got truncated.  Try again with a buffer of at least N bytes.
	virtual int GetDetailedConnectionStatus( HSteamNetConnection hConn, char *pszBuf, int cbBuf ) = 0;

	/// Returns information about the listen socket.
	///
	/// *pnIP and *pnPort will be 0 if the socket is set to listen for connections based
	/// on SteamID only.  If your listen socket accepts connections on IPv4, then both
	/// fields will return nonzero, even if you originally passed a zero IP.  However,
	/// note that the address returned may be a private address (e.g. 10.0.0.x or 192.168.x.x),
	/// and may not be reachable by a general host on the Internet.
	virtual bool GetListenSocketInfo( HSteamListenSocket hSocket, uint32 *pnIP, uint16 *pnPort ) = 0;

	//
	// Special SDR connections involved with servers hosted in Valve data centers
	//
	virtual bool SetHostedDedicatedServerCertificate( const void *pCert, int cbCert, void *pPrivateKey, int cbPrivateKey ) = 0;
	virtual HSteamListenSocket CreateHostedDedicatedServerListenSocket( uint16 nPort ) = 0;
	virtual HSteamNetConnection ConnectToHostedDedicatedServer( CSteamID steamIDTarget ) = 0;

	//
	// Gets some debug text from the connection
	//
	virtual bool GetConnectionDebugText( HSteamNetConnection hConn, char *pOut, int nOutCCH ) = 0;

	//
	// Set and get configuration values, see ESteamNetworkingConfigurationValue for individual descriptions.
	//
	// Returns the value or -1 is eConfigValue is invalid
	virtual int32 GetConfigurationValue( ESteamNetworkingConfigurationValue eConfigValue ) = 0;
	// Returns true if successfully set
	virtual bool SetConfigurationValue( ESteamNetworkingConfigurationValue eConfigValue, int32 nValue ) = 0;

	// Return the name of an int configuration value, or NULL if config value isn't known
	virtual const char *GetConfigurationValueName( ESteamNetworkingConfigurationValue eConfigValue ) = 0;

	//
	// Set and get configuration strings, see ESteamNetworkingConfigurationString for individual descriptions.
	//
	// Get the configuration string, returns length of string needed if pDest is nullpr or destSize is 0
	// returns -1 if the eConfigValue is invalid
	virtual int32 GetConfigurationString( ESteamNetworkingConfigurationString eConfigString, char *pDest, int32 destSize ) = 0;
	virtual bool SetConfigurationString( ESteamNetworkingConfigurationString eConfigString, const char *pString ) = 0;

	// Return the name of a string configuration value, or NULL if config value isn't known
	virtual const char *GetConfigurationStringName( ESteamNetworkingConfigurationString eConfigString ) = 0;

};
#define STEAMSOCKETNETWORKING_VERSION "SteamSocketNetworking001"

// Notification struct used to notify when a connection has changed state
struct SteamNetConnectionStatusChangedCallback_t
{ 
	HSteamNetConnection m_hConn;		//< Connection handle
	SteamNetConnectionInfo_t m_info;	//< Full connection info
	int m_eOldState;					//< ESNetSocketState.  (Current stats is in m_info)
};

// Temporary global accessor.   This will be moved to steam_api.h
STEAMDATAGRAMLIB_INTERFACE ISteamSocketNetworking *SteamSocketNetworking();

typedef void * ( S_CALLTYPE *FSteamInternal_CreateInterface )( const char *);
typedef void ( S_CALLTYPE *FSteamAPI_RegisterCallResult)( class CCallbackBase *pCallback, SteamAPICall_t hAPICall );
typedef void ( S_CALLTYPE *FSteamAPI_UnregisterCallResult)( class CCallbackBase *pCallback, SteamAPICall_t hAPICall );

/// !KLUDGE! Glue code that will go away when we move everything into
/// the ISteamNetwork interfaces
STEAMDATAGRAMLIB_INTERFACE void SteamDatagramClient_Internal_SteamAPIKludge( FSteamAPI_RegisterCallResult fnRegisterCallResult, FSteamAPI_UnregisterCallResult fnUnregisterCallResult );
STEAMDATAGRAMLIB_INTERFACE bool SteamDatagramClient_Init_Internal( const char *pszCacheDirectory, /* ESteamDatagramPartner */ int ePartner, int iPartnerMask, SteamDatagramErrMsg &errMsg, ISteamClient *pClient, HSteamUser hSteamUser, HSteamPipe hSteamPipe );
inline bool SteamDatagramClient_Init( const char *pszCacheDirectory, /* ESteamDatagramPartner */ int ePartner, int iPartnerMask, SteamDatagramErrMsg &errMsg )
{
	SteamDatagramClient_Internal_SteamAPIKludge( &::SteamAPI_RegisterCallResult, &::SteamAPI_UnregisterCallResult );
	return SteamDatagramClient_Init_Internal( pszCacheDirectory, ePartner, iPartnerMask, errMsg, ::SteamClient(), ::SteamAPI_GetHSteamUser(), ::SteamAPI_GetHSteamPipe() );
}


/// Shutdown all clients and close all sockets
STEAMDATAGRAMLIB_INTERFACE void SteamDatagramClient_Kill();

/// Initialize the game server interface
STEAMDATAGRAMLIB_INTERFACE bool SteamDatagramServer_Init_Internal( SteamDatagramErrMsg &errMsg, ISteamClient *pClient, HSteamUser hSteamUser, HSteamPipe hSteamPipe );
// KLUDGE TF is using an old version of the SDK, which doesn't have this.  TF doesn't need this, so just comment it out.
// We'll need to upgrade the Steamworks SDK if we want to actually use SDR
//inline bool SteamDatagramServer_Init( SteamDatagramErrMsg &errMsg )
//{
//	SteamDatagramClient_Internal_SteamAPIKludge( &::SteamAPI_RegisterCallResult, &::SteamAPI_UnregisterCallResult );
//	return SteamDatagramServer_Init_Internal( errMsg, ::SteamGameServerClient(), ::SteamGameServer_GetHSteamUser(), ::SteamGameServer_GetHSteamPipe() );
//}

/// Shutdown the game server interface
STEAMDATAGRAMLIB_INTERFACE void SteamDatagramServer_Kill( );

// !KLUDGE! Check for connections that have changed status, and post callbacks.
// This is temporary we can hook this up using the ordinary steam CCallback mechanism
typedef void (*FSteamNetConnectionStatusChangedCallback)( SteamNetConnectionStatusChangedCallback_t *pInfo );
STEAMDATAGRAMLIB_INTERFACE void Temp_DispatchsSteamNetConnectionStatusChangedCallbacks( FSteamNetConnectionStatusChangedCallback fnCallback );

enum ESteamDatagramDebugOutputType
{
	k_ESteamDatagramDebugOutputType_None,
	k_ESteamDatagramDebugOutputType_Error,
	k_ESteamDatagramDebugOutputType_Important, // Nothing is wrong, but this is an important notification
	k_ESteamDatagramDebugOutputType_Warning,
	k_ESteamDatagramDebugOutputType_Msg, // Recommended amount
	k_ESteamDatagramDebugOutputType_Verbose, // Quite a bit
	k_ESteamDatagramDebugOutputType_Debug, // Practically everything
};

/// Setup callback for debug output, and the desired verbosity you want.
typedef void (*FSteamDatagramDebugOutput)( /* ESteamDatagramDebugOutputType */ int nType, const char *pszMsg );
STEAMDATAGRAMLIB_INTERFACE void SteamDatagram_SetDebugOutputFunction( /* ESteamDatagramDebugOutputType */ int eDetailLevel, FSteamDatagramDebugOutput pfnFunc );

#endif // ISTEAMNETWORKINGSOCKETS
