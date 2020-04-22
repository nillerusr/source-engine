//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Purpose: misc networking utilities
//
//=============================================================================

#ifndef STEAMNETWORKINGTYPES
#define STEAMNETWORKINGTYPES
#ifdef _WIN32
#pragma once
#endif

#include "steam/steamtypes.h"
#include "steam/steamclientpublic.h"
#include "tier0/platform.h"

#if defined( STEAMDATAGRAMLIB_STATIC_LINK )
	#define STEAMDATAGRAMLIB_INTERFACE extern
#elif defined( STEAMDATAGRAMLIB_FOREXPORT )
	#define STEAMDATAGRAMLIB_INTERFACE DLL_EXPORT
#else
	#define STEAMDATAGRAMLIB_INTERFACE DLL_IMPORT
#endif

#pragma pack( push, 8 )

struct SteamNetworkPingLocation_t;
class ISteamNetworkingMessage;
struct SteamDatagramLinkStats;
struct SteamDatagramLinkLifetimeStats;
struct SteamDatagramLinkInstantaneousStats;
struct SteamNetworkingDetailedConnectionStatus;

/// Handle used to identify a connection to a remote host.
typedef uint32 HSteamNetConnection;
const HSteamNetConnection k_HSteamNetConnection_Invalid = 0;

/// Handle used to identify a "listen socket".
typedef uint32 HSteamListenSocket;
const HSteamListenSocket k_HSteamListenSocket_Invalid = 0;

/// Configuration values for Steam networking. 
///  
/// Most of these are for controlling extend logging or features 
/// of various subsystems 
enum ESteamNetworkingConfigurationValue
{
	// Set to true (non-zero) to use Steam Network Protocol for message 
	// delivery.
	// SNP is a protocol for sending a mix of reliable and unreliable message 
	// payloads and handles retransmission of reliable messages and also 
	// manages transfer rate congestion control.  Defaults to 1 (on).
	k_ESteamNetworkingConfigurationValue_SNP = 0,

	// 0-100 Randomly discard N pct of unreliable messages instead of sending
	// Defaults to 0 (no loss).
	k_ESteamNetworkingConfigurationValue_FakeMessageLoss_Send = 1,

	// 0-100 Randomly discard N pct of unreliable messages upon receive
	// Defaults to 0 (no loss).
	k_ESteamNetworkingConfigurationValue_FakeMessageLoss_Recv = 2,

	// 0-100 Randomly discard N pct of packets instead of sending
	k_ESteamNetworkingConfigurationValue_FakePacketLoss_Send = 3,

	// 0-100 Randomly discard N pct of packets received
	k_ESteamNetworkingConfigurationValue_FakePacketLoss_Recv = 4,

	// Set to true (non-zero) to open up a seperate debug window showing SNP 
	// state for all current connections.  Only works on Windows.
	// Defaults to 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_DebugWindow = 5,

	// Upper limit of buffered pending bytes to be sent, if this is reached
	// SendMessage will return k_EResultPending
	// Default is 512k (524288 bytes)
	k_ESteamNetworkingConfigurationValue_SNP_SendBufferSize = 6,

	// Maximum send rate clamp, 0 is no limit
	// This value will control the maximum allowed sending rate that congestion 
	// is allowed to reach.  Default is 0 (no-limit)
	k_ESteamNetworkingConfigurationValue_SNP_MaxRate = 7,

	// Minimum send rate clamp, 0 is no limit
	// This value will control the minimum allowed sending rate that congestion 
	// is allowed to reach.  Default is 0 (no-limit)
	k_ESteamNetworkingConfigurationValue_SNP_MinRate = 8,

	// Set to true (non-zero) to enable logging of SNP RTT
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_RTT = 9,

	// Set to true (non-zero) to enable logging of SNP Packet
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_Packet = 10,

	// Set to true (non-zero) to enable logging of SNP Segments
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_Segments = 11,

	// Set to true (non-zero) to enable logging of SNP Feedback
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_Feedback = 12,

	// Set to true (non-zero) to enable logging of SNP Relible
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_Reliable = 13,

	// Set to true (non-zero) to enable logging of SNP Messages
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_Message = 14,

	// Set to true (non-zero) to enable logging of SNP Loss
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_Loss = 15,

	// Set to true (non-zero) to enable logging of SNP Throughput
	// Default is 0 (off)
	k_ESteamNetworkingConfigurationValue_SNP_Log_X = 16,

	// If the first N pings to a port all fail, mark that port as unavailable for
	// a while, and try a different one.  Some ISPs and routers may drop the first
	// packet, so setting this to 1 may greatly disrupt communications.
	k_ESteamNetworkingConfigurationValue_ClientConsecutitivePingTimeoutsFailInitial = 17,

	// If N consecutive pings to a port fail, after having received successful 
	// communication, mark that port as unavailable for a while, and try a 
	// different one.
	k_ESteamNetworkingConfigurationValue_ClientConsecutitivePingTimeoutsFail = 18,

	/// Minimum number of lifetime pings we need to send, before we think our estimate
	/// is solid.  The first ping to each cluster is very often delayed because of NAT,
	/// routers not having the best route, etc.  Until we've sent a sufficient number
	/// of pings, our estimate is often inaccurate.  Keep pinging until we get this
	/// many pings.
	k_ESteamNetworkingConfigurationValue_ClientMinPingsBeforePingAccurate = 19,

	// Set all steam datagram traffic to originate from the same local port.  
	// By default, we open up a new UDP socket (on a different local port) 
	// for each relay.  This is not optimal, but it works around some 
	// routers that don't implement NAT properly.  If you have intermittent 
	// problems talking to relays that might be NAT related, try toggling 
	// this flag
	k_ESteamNetworkingConfigurationValue_ClientSingleSocket = 20,

	// Globally delay all outbound packets by N ms before sending
	k_ESteamNetworkingConfigurationValue_FakePacketLag_Send = 21,

	// Globally delay all received packets by N ms before processing
	k_ESteamNetworkingConfigurationValue_FakePacketLag_Recv = 22,

	// Don't automatically fail IP connections that don't have strong auth.
	// On clients, this means we will attempt the connection even if we don't
	// know our SteamID or can't get a cert.  On the server, it means that we won't
	// automatically reject a connection due to a failure to authenticate.
	// (You can examine the incoming connection and decide whether to accept it.)
	k_ESteamNetworkingConfigurationValue_IP_Allow_Without_Auth = 23,

	// Number of k_ESteamNetworkingConfigurationValue defines
	k_ESteamNetworkingConfigurationValue_Count,
};

/// Configuration strings for Steam networking. 
///  
/// Most of these are for controlling extend logging or features 
/// of various subsystems 
enum ESteamNetworkingConfigurationString
{
	// Code of relay cluster to use.  If not empty, we will only use relays in that cluster.  E.g. 'iad'
	k_ESteamNetworkingConfigurationString_ClientForceRelayCluster = 0,

	// For debugging, generate our own (unsigned) ticket, using the specified 
	// gameserver address.  Router must be configured to accept unsigned tickets.
	k_ESteamNetworkingConfigurationString_ClientDebugTicketAddress = 1,

	// For debugging.  Override list of relays from the config with this set
	// (maybe just one).  Comma-separated list.
	k_ESteamNetworkingConfigurationString_ClientForceProxyAddr = 2,

	// Number of k_ESteamNetworkingConfigurationString defines
	k_ESteamNetworkingConfigurationString_Count = k_ESteamNetworkingConfigurationString_ClientForceProxyAddr + 1,
};

/// Basically a duplicate of EP2PSend.
enum ESteamNetworkingSendType
{
	// Send an unreliable message. Can be lost.
	// The sending API does have some knowledge of the underlying connection, so if there is no NAT-traversal accomplished or
	// there is a recognized adjustment happening on the connection, the packet will be batched until the connection is open again.
	k_ESteamNetworkingSendType_Unreliable = 0,

	// Reliable message send. Can send up to 512kb of data in a single message. 
	// Does fragmentation/re-assembly of messages under the hood, as well as a sliding window for efficient sends of large chunks of data. 
	k_ESteamNetworkingSendType_Reliable = 1,
};

/// High level connection status
enum ESteamNetworkingConnectionState
{

	/// Dummy value used to indicate an error condition in the API.
	/// Specified connection doesn't exist or has already been closed.
	k_ESteamNetworkingConnectionState_None = 0,

	/// - For connections on the "client" side (initiated locally):
	///   We're in the process of trying to establish a connection.
	///   Depending on the socket type, we might not even know who they are.
	///   Note that it is not possible to tell if we are waiting on the
	///   network to complete handshake packets, or for the application layer
	///   to accept the connection.
	///
	/// - For connections on the "server" side (accepted through listen socket):
	///   We have completed a series of basic handshake packets, and the connection
	///   is ready to be accepted using AcceptConnection.
	///
	/// In either case, any unreliable packets sent now are almost certain
	/// to be dropped.  Attempts to receive packets are guaranteed to fail.
	/// You may send messages if the send mode allows for them to be queued.
	/// but if you close the connection before the connection is actually
	/// established, any queued messages will be discarded immediately.
	/// (We will not attempt to flush the queue and confirm delivery to the
	/// remote host, which ordinarily happens when a connection is closed.)
	k_ESteamNetworkingConnectionState_Connecting = 1,

	/// We've received communications from our peer (and we know
	/// who they are) and are all good.  If you close the connection now,
	/// we will make our best effort to flush out any reliable sent data that
	/// has not been acknowledged by the peer.  (But note that this happens
	/// from within the application process, so unlike a TCP connection, you are
	/// not totally handing it off to the operating system to deal with it.)
	k_ESteamNetworkingConnectionState_Connected = 2,

	/// Connection has been closed by our peer, but not closed locally.
	/// The connection still exists from an API perspective.  You must close the
	/// handle to free up resources.  If there are any messages in the inbound queue,
	/// you may retrieve them.  Otherwise, nothing may be done with the connection
	/// except to close it.
	///
	/// This stats is similar to CLOSE_WAIT in the TCP state machine.
	k_ESteamNetworkingConnectionState_ClosedByPeer = 3,

	/// A disruption in the connection has been detected locally.  (E.g. timeout,
	/// local internet connection disrupted, etc.)
	///
	/// The connection still exists from an API perspective.  You must close the
	/// handle to free up resources.
	///
	/// Attempts to send further messages will fail.  Any remaining received messages
	/// in the queue are available.
	k_ESteamNetworkingConnectionState_ProblemDetectedLocally = 4,

//
// The following values are used internally and will not be returned by any API.
// We document them here to provide a little insight into the state machine that is used
// under the hood.
//

	/// We've disconnected on our side, and from an API perspective the connection is closed.
	/// No more data may be sent or received.  All reliable data has been flushed, or else
	/// we've given up and discarded it.  We do not yet know for sure that the client knows
	/// the connection has been closed, however, so we're just hanging around so that if we do
	/// get a packet from them, we can send them the appropriate packets so that they can
	/// know why the connection was closed (and not have to rely on a timeout, which makes
	/// it appear as if something is wrong).
	///
	/// Note that, unlike TCP connections, Steam networking sessions have additional
	/// session identifiers that make the TIME_WAIT state unnecessary.  (If another connection
	/// is established on the same port, and we get packets from an old session, we are able to
	/// identify this and either ignore those packets, or inform the remote host that the old
	/// session has been closed, if appropriate).
	k_ESteamNetworkingConnectionState_FinWait = -1,

	/// We've disconnected on our side, and from an API perspective the connection is closed.
	/// No more data may be sent or received.  From a network perspective, however, on the wire,
	/// we have not yet given any indication to the peer that the connection is closed.
	/// We are in the process of flushing out the last bit of reliable data.  Once that is done,
	/// we will inform the peer that the connection has been closed, and transition to the
	/// FinWait state.
	///
	/// Note that no indication is given to the remote host that we have closed the connection,
	/// until the data has been flushed.  If the remote host attempts to send us data, we will
	/// do whatever is necessary to keep the connection alive until it can be closed properly.
	/// But in fact the data will be discarded, since there is no way for the application to
	/// read it back.  Typically this is not a problem, as application protocols that utilize
	/// the lingering functionality are designed for the remote host to wait for the response
	/// before sending any more data.
	k_ESteamNetworkingConnectionState_Linger = -2, 

	/// Connection is completely inactive and ready to be destroyed
	k_ESteamNetworkingConnectionState_Dead = -3,
};

/// Identifier used for a network location point of presence.
/// Typically you won't need to directly manipulate these.
typedef uint32 SteamNetworkingPOPID;

/// Convert 3- or 4-character ID to 32-bit int.
inline SteamNetworkingPOPID CalculateSteamNetworkingPOPIDFromString( const char *pszCode )
{
	// OK we made a bad decision when we decided how to pack 3-character codes into a uint32.  We'd like to support
	// 4-character codes, but we don't want to break compatibility.  The migration path has some subtleties that make
	// this nontrivial, and there are already some IDs stored in SQL.  Ug, so the 4 character code "abcd" will
	// be encoded with the digits like "0xddaabbcc"
	return
		( uint32(pszCode[3]) << 24U ) 
		| ( uint32(pszCode[0]) << 16U ) 
		| ( uint32(pszCode[1]) << 8U )
		| uint32(pszCode[2]);
}

/// Unpack integer to string representation, including terminating '\0'
template <int N>
inline void GetSteamNetworkingLocationPOPStringFromID( SteamNetworkingPOPID id, char (&szCode)[N] )
{
	COMPILE_TIME_ASSERT( N >= 5 );
	szCode[0] = ( id >> 16U );
	szCode[1] = ( id >> 8U );
	szCode[2] = ( id );
	szCode[3] = ( id >> 24U ); // See comment above about deep regret and sadness
	szCode[4] = 0;
}

/// A local timestamp.  You can subtract two timestamps to get the number of elapsed
/// microseconds.  This is guaranteed to increase over time during the lifetime
/// of a process, but not globally across runs.  You don't need to worry about
/// the value wrapping around.  Note that the underlying clock might not actually have
/// microsecond *resolution*.
typedef int64 SteamNetworkingMicroseconds;

/// A sequence number.  Used to indentify packets and messages
/// and other segments.  Typically this is 64 bit internally and
/// has the last 16 bits networked
typedef int64 SteamNetworkingSequenceNumber;

// maximum amount of pending buffered messages allows
const int k_cbMaxSteamDatagramMessageSize = 512 * 1024;

/// A message that has been received.
class ISteamNetworkingMessage
{
public:	

	/// You MUST call this when you're done with the object,
	/// to free up memory, etc.
	virtual void Release() = 0;

	/// Get size of the payload.
	inline uint32 GetSize() const { return m_cbSize; }

	/// Get message payload
	inline const void *GetData() const { return m_pData; }

	/// Return SteamID that sent this to us.
	inline CSteamID GetSenderSteamID() const { return m_steamIDSender; }

	/// Return the channel number the message was received on.
	/// (Not used for messages received on "connections"
	inline int GetChannel() const { return m_nChannel; }

	/// The socket this came from.  (Not used when using the P2P calls)
	inline HSteamNetConnection GetConnection() const { return m_conn; }

	/// Get the user data associated with the connection.
	///
	/// This is *usually* the same as calling GetConnection() and then
	/// fetching the user data associated with that connection, but for
	/// the following subtle differences:
	///
	/// - This user data will match the connection's user data at the time
	///   is captured at the time the message is returned by the API.
	///   If you subsequently change the userdata on the connection,
	///   this won't be updated.
	/// - This is an inline call, so it's *much* faster.
	/// - You might have closed the connection, so fetching the user data
	///   would not be possible.
	inline int64 GetConnectionUserData() const { return m_nConnUserData; }

	/// Get the time that it was received
	inline SteamNetworkingMicroseconds GetTimeReceived() const { return m_usecTimeReceived; }

protected:
	CSteamID m_steamIDSender;
	void *m_pData;
	uint32 m_cbSize;
	int m_nChannel;
	HSteamNetConnection m_conn;
	int64 m_nConnUserData;
	SteamNetworkingMicroseconds m_usecTimeReceived;

	inline ~ISteamNetworkingMessage() {}; // Destructor hidden - use Release()!  But make it inline and empty, in case you want to derive your own type that satisfies this interface for use in your code.
};

/// Object that describes a "location" on the Internet with sufficient
/// detail that we can reasonably estimate an upper bound on the ping between
/// the two hosts, even if a direct route between the hosts is not possible,
/// and the connection must be routed through the Steam Datagram Relay network.
/// This does not contain any information that identifies the host.  Indeed,
/// if two hosts are in the same building or otherwise have nearly identical
/// networking characteristics, then it's valid to use the same location
/// object for both of them.
///
/// NOTE: This object should only be used in memory.  If you need to persist
/// it or send it over the wire, convert it to a string representation using
/// the methods in ISteamNetworkingUtils()
struct SteamNetworkPingLocation_t
{
	uint8 m_data[ 64 ];
};

/// Special values that are returned by some functions that return a ping.
const int k_nSteamNetworkingPing_Failed = -1;
const int k_nSteamNetworkingPing_Unknown = -2;

/// Enumerate various causes of connection termination.  These are designed
/// to work sort of like HTTP error codes, in that the numeric range gives you
/// a ballpark as to where the problem is.
enum ESteamNetConnectionEnd
{
	// Invalid/sentinel value
	k_ESteamNetConnectionEnd_Invalid = 0,

	//
	// Application codes.  You can use these codes if you want to
	// plumb through application-specific error codes.  If you don't need this
	// facility, feel free to always use 0, which is a generic
	// application-initiated closure.
	//
	// The distinction between "normal" and "exceptional" termination is
	// one you may use if you find useful, but it's not necessary for you
	// to do so.  The only place where we distinguish between normal and
	// exceptional is in connection analytics.  If a significant
	// proportion of connections terminates in an exceptional manner,
	// this can trigger an alert.
	//

	// 1xxx: Application ended the connection in a "usual" manner.
	//       E.g.: user intentionally disconnected from the server,
	//             gameplay ended normally, etc
	k_ESteamNetConnectionEnd_App_Min = 1000,
		k_ESteamNetConnectionEnd_App_Generic = k_ESteamNetConnectionEnd_App_Min,
		// Use codes in this range for "normal" disconnection
	k_ESteamNetConnectionEnd_App_Max = 1999,

	// 2xxx: Application ended the connection in some sort of exceptional
	//       or unusual manner that might indicate a bug or configuration
	//       issue.
	// 
	k_ESteamNetConnectionEnd_AppException_Min = 2000,
		k_ESteamNetConnectionEnd_AppException_Generic = k_ESteamNetConnectionEnd_AppException_Min,
		// Use codes in this range for "unusual" disconnection
	k_ESteamNetConnectionEnd_AppException_Max = 2999,

	//
	// System codes:
	//

	// 3xxx: Connection failed or ended because of problem with the
	//       local host or their connection to the Internet.
	k_ESteamNetConnectionEnd_Local_Min = 3000,

		// You cannot do what you want to do because you're running in offline mode.
		k_ESteamNetConnectionEnd_Local_OfflineMode = 3001,

		// We're having trouble contacting many (perhaps all) relays.
		// Since it's unlikely that they all went offline at once, the best
		// explanation is that we have a problem on our end.  Note that we don't
		// bother distinguishing between "many" and "all", because in practice,
		// it takes time to detect a connection problem, and by the time
		// the connection has timed out, we might not have been able to
		// actively probe all of the relay clusters, even if we were able to
		// contact them at one time.  So this code just means that:
		//
		// * We don't have any recent successful communication with any relay.
		// * We have evidence of recent failures to communicate with multiple relays.
		k_ESteamNetConnectionEnd_Local_ManyRelayConnectivity = 3002,

		// A hosted server is having trouble talking to the relay
		// that the client was using, so the problem is most likely
		// on our end
		k_ESteamNetConnectionEnd_Local_HostedServerPrimaryRelay = 3003,

		// We're not able to get the network config.  This is
		// *almost* always a local issue, since the network config
		// comes from the CDN, which is pretty darn reliable.
		k_ESteamNetConnectionEnd_Local_NetworkConfig = 3004,

	k_ESteamNetConnectionEnd_Local_Max = 3999,

	// 4xxx: Connection failed or ended, and it appears that the
	//       cause does NOT have to do with the local host or their
	//       connection to the Internet.  It could be caused by the
	//       remote host, or it could be somewhere in between.
	k_ESteamNetConnectionEnd_Remote_Min = 4000,

		// The connection was lost, and as far as we can tell our connection
		// to relevant services (relays) has not been disrupted.  This doesn't
		// mean that the problem is "their fault", it just means that it doesn't
		// appear that we are having network issues on our end.
		k_ESteamNetConnectionEnd_Remote_Timeout = 4001,

		// Something was invalid with the cert or crypt handshake
		// info you gave me, I don't understand or like your key types,
		// etc.
		k_ESteamNetConnectionEnd_Remote_BadCrypt = 4002,

		// You presented me with a cert that was technically
		// valid.  But the CA key was missing (and I don't accept
		// unsigned certs), or the key isn't one that I trust.
		k_ESteamNetConnectionEnd_Remote_CertNotTrusted = 4003,

	k_ESteamNetConnectionEnd_Remote_Max = 4999,

	// 5xxx: Connection failed for some other reason.
	k_ESteamNetConnectionEnd_Misc_Min = 5000,

		// A failure that isn't necessarily the result of a software bug,
		// but that should happen rarely enough that it isn't worth specifically
		// writing UI or making a localized message for.
		// The debug string should contain further details.
		k_ESteamNetConnectionEnd_Misc_Generic = 5001,

		// Generic failure that is most likely a software bug.
		k_ESteamNetConnectionEnd_Misc_InternalError = 5002,

		// The connection to the remote host timed out, but we
		// don't know if the problem is on our end, in the middle,
		// or on their end.
		k_ESteamNetConnectionEnd_Misc_Timeout = 5003,

		// We're having trouble talking to the relevant relay.
		// We don't have enough information to say whether the
		// problem is on our end or not.
		k_ESteamNetConnectionEnd_Misc_RelayConnectivity = 5004,

		// There's some trouble talking to Steam.
		k_ESteamNetConnectionEnd_Misc_SteamConnectivity = 5005,

		// A server in a dedicated hosting situation has no relay sessions
		// active with which to talk back to a client.  (It's the client's
		// job to open and maintain those sessions.)
		k_ESteamNetConnectionEnd_Misc_NoRelaySessionsToClient = 5006,

	k_ESteamNetConnectionEnd_Misc_Max = 5999,
};

/// Max length of diagnostic error message
const int k_cchMaxSteamDatagramErrMsg = 1024;

/// Used to return English-language diagnostic error messages to caller.
/// (For debugging or spewing to a console, etc.  Not intended for UI.)
typedef char SteamDatagramErrMsg[ k_cchMaxSteamDatagramErrMsg ];

/// Max length, in bytes (including null terminator) of the reason string
/// when a connection is closed.
const int k_cchSteamNetworkingMaxConnectionCloseReason = 128;

struct SteamNetConnectionInfo_t
{

	/// Handle to listen socket this was connected on, or k_HSteamListenSocket_Invalid if we initiated the connection
	HSteamListenSocket m_hListenSocket;

	/// Who is on the other end.  Depending on the connection type and phase of the connection, we might not know
	CSteamID m_steamIDRemote;

	// FIXME - some sort of connection type enum?

	/// Arbitrary user data set by the local application code
	int64 m_nUserData;

	/// Remote address.  Might be 0 if we don't know it
	uint32 m_unIPRemote;
	uint16 m_unPortRemote;
	uint16 m__pad1;

	/// What data center is the remote host in?  (0 if we don't know.)
	SteamNetworkingPOPID m_idPOPRemote;

	/// What relay are we using to communicate with the remote host?
	/// (0 if not applicable.)
	SteamNetworkingPOPID m_idPOPRelay;

	/// Local port that we're bound to for this connection.  Might not be applicable
	/// for all connection types.
	//uint16 m_unPortLocal;

	/// High level state of the connection
	int /* ESteamNetworkingConnectionState */ m_eState;

	/// Basic cause of the connection termination or problem.
	/// One of ESteamNetConnectionEnd
	int /* ESteamNetConnectionEnd */ m_eEndReason;

	/// Human-readable, but non-localized explanation for connection
	/// termination or problem.  This is intended for debugging /
	/// diagnostic purposes only, not to display to users.  It might
	/// have some details specific to the issue.
	char m_szEndDebug[ k_cchSteamNetworkingMaxConnectionCloseReason ];
};

/// Quick connection state, pared down to something you could call
/// more frequently without it being too big of a perf hit.
struct SteamNetworkingQuickConnectionStatus
{

	/// High level state of the connection
	int /* ESteamNetworkingConnectionState */ m_eState;

	/// Current ping (ms)
	int m_nPing;

	/// Connection quality measured locally, 0...1.  (Percentage of packets delivered
	/// end-to-end in order)
	float m_flConnectionQualityLocal;

	/// Packet delivery success rate as observed from remote host
	float m_flConnectionQualityRemote;

	/// Actual current data rates
	float m_flOutPacketsPerSec;
	float m_flOutBytesPerSec;
	float m_flInPacketsPerSec;
	float m_flInBytesPerSec;

	/// Estimate bandwidth of the connection (bytes per second)
	int m_nSendRate;

	/// Number of bytes pending to be sent
	int m_nPendingBytes;
};

#pragma pack( pop )

enum ESteamDatagramPartner
{
	k_ESteamDatagramPartner_None = -1,
	k_ESteamDatagramPartner_Steam = 0,
	k_ESteamDatagramPartner_China = 1,
};

#endif // #ifndef STEAMNETWORKINGTYPES
