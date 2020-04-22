//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

// Should move to common/networksystem

#ifndef NETCHANNEL_H
#define NETCHANNEL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/bitbuf.h"
#include "tier1/netadr.h"
#include "sm_Protocol.h"
#include "tier1/utlvector.h"
#include "networksystem/inetworksystem.h"
#include "tier1/mempool.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUDPSocket;
class CUtlBuffer;
class CNetPacket;
class CNetChannel;
class INetChannel;

// 0 == regular, 1 == file stream
enum
{
	FRAG_NORMAL_STREAM = 0,
	FRAG_FILE_STREAM,

	MAX_STREAMS
};

#define NET_MAX_DATAGRAM_PAYLOAD 1400
#define NET_MAX_PAYLOAD_BITS	11		// 2^NET_MAX_PALYLOAD_BITS > NET_MAX_PAYLOAD
#define DEFAULT_RATE 10000
#define SIGNON_TIME_OUT	120.0f
#define CONNECTION_PROBLEM_TIME	15.0f

#define MAX_RATE 50000
#define MIN_RATE 100

#define FRAGMENT_BITS		8
#define FRAGMENT_SIZE		(1<<FRAGMENT_BITS)
#define MAX_FILE_SIZE_BITS	26
#define MAX_FILE_SIZE		((1<<MAX_FILE_SIZE_BITS)-1)	// maximum transferable size is	64MB

#define NET_MAX_PAYLOAD			4000
#define NET_MAX_MESSAGE			4096
#define MIN_ROUTEABLE_PACKET	16
#define MAX_ROUTEABLE_PACKET	1400	// Ethernet 1518 - ( CRC + IP + UDP header)
#define UDP_HEADER_SIZE			28

// each channel packet has 1 byte of FLAG bits
#define PACKET_FLAG_RELIABLE			(1<<0)	// packet contains subchannel stream data
#define PACKET_FLAG_CHOKED				(1<<1)  // packet was choked by sender


// shared commands used by all streams, handled by stream layer, TODO

abstract_class INetworkMessageHandler
{
public:
	virtual void	OnConnectionClosing( INetChannel *channel, char const *reason ) = 0;
	virtual void	OnConnectionStarted( INetChannel *channel ) = 0;

	virtual void	OnPacketStarted( int inseq, int outseq ) = 0;
	virtual void	OnPacketFinished() = 0;

protected:
	virtual ~INetworkMessageHandler() {}
};



class INetChannelHandler
{
public:
	virtual	~INetChannelHandler( void ) {};

	virtual void ConnectionStart(INetChannel *chan) = 0;	// called first time network channel is established

	virtual void ConnectionClosing(const char *reason) = 0; // network channel is being closed by remote site

	virtual void ConnectionCrashed(const char *reason) = 0; // network error occured

	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) = 0;	// called each time a new packet arrived

	virtual void PacketEnd( void ) = 0; // all messages has been parsed

	virtual void FileRequested(const char *fileName, unsigned int transferID) = 0; // other side request a file for download

	virtual void FileReceived(const char *fileName, unsigned int transferID) = 0; // we received a file
	
	virtual void FileDenied(const char *fileName, unsigned int transferID) = 0;	// a file request was denied by other side
};


// server to client
class CNetPacket
{
	DECLARE_FIXEDSIZE_ALLOCATOR( CNetPacket );

public:
	CNetPacket();
	~CNetPacket();

	void AddRef();
	void Release();

public:
	netadr_t		m_From;				// sender IP
	CUDPSocket		*m_pSource;			// received source 
	float			m_flReceivedTime;	// received time
	unsigned char	*m_pData;			// pointer to raw packet data
	bf_read			m_Message;			// easy bitbuf data access
	int				m_nSizeInBytes;		// size in bytes

private:
	int				m_nRefCount;// Reference count
};


abstract_class IConnectionlessPacketHandler
{
public:
	virtual bool ProcessConnectionlessPacket( CNetPacket *packet ) = 0;	// process a connectionless packet

protected:
	virtual	~IConnectionlessPacketHandler( void ) {};
};

abstract_class ILookupChannel
{
public:

	virtual INetChannel *FindNetChannel( const netadr_t& from ) = 0;
};

// FIXME: Make an INetChannel
class CNetChannel : public INetChannel
{
public:
	explicit CNetChannel();
	~CNetChannel();

	// Methods of INetChannel
 	virtual ConnectionStatus_t GetConnectionState( );
	virtual const netadr_t &GetRemoteAddress( void ) const;

	void		Setup( bool serverSide, const netadr_t *remote_address, CUDPSocket *sendSocket, char const *name, INetworkMessageHandler *handler );
	void		Reset();
	void		Clear();
	void		Shutdown( const char *reason );

	CUDPSocket *GetSocket();

	void		SetDataRate( float rate );
	void		SetTimeout( float seconds );

	bool		StartProcessingPacket( CNetPacket *packet );
	bool		ProcessPacket( CNetPacket *packet );
	void		EndProcessingPacket( CNetPacket *packet );

	bool		CanSendPacket( void ) const;
	void		SetChoked( void ); // choke a packet
	bool		HasPendingReliableData( void );

	// Queues data for sending:

	// send a net message
	bool		AddNetMsg( INetworkMessage *msg, bool bForceReliable = false );

	// send a chunk of data
	bool		AddData( bf_write &msg, bool bReliable = true ); 

	// Puts data onto the wire:

	int			SendDatagram( bf_write *data ); // Adds data to unreliable payload and then calls transmits the data
	bool		Transmit( bool onlyReliable = false ); // send data from buffers (calls SendDataGram( NULL ) )

	bool		IsOverflowed( void ) const;
	bool		IsTimedOut( void ) const;
	bool		IsTimingOut() const;

// Info:

	const char  *GetName( void ) const;
	const char  *GetAddress( void ) const;
	float		GetTimeConnected( void ) const;
	float		GetTimeSinceLastReceived( void ) const;
	int			GetDataRate( void ) const;

	float		GetLatency( int flow ) const;
	float		GetAvgLatency( int flow ) const;
	float		GetAvgLoss( int flow ) const;
	float		GetAvgData( int flow ) const;
	float		GetAvgChoke( int flow ) const;
	float		GetAvgPackets( int flow ) const;
	int			GetTotalData( int flow ) const;

 	void SetConnectionState( ConnectionStatus_t state );

private:
	int			ProcessPacketHeader( bf_read &buf );
	bool		ProcessControlMessage( int cmd, bf_read &buf );
	bool		ProcessMessages( bf_read &buf );

	ConnectionStatus_t m_ConnectionState;

// last send outgoing sequence number
	int			m_nOutSequenceNr;
	// last received incoming sequnec number
	int			m_nInSequenceNr;
	// last received acknowledge outgoing sequnce number
	int			m_nOutSequenceNrAck;
	
	// state of outgoing reliable data (0/1) flip flop used for loss detection
	int			m_nOutReliableState;
	// state of incoming reliable data
	int			m_nInReliableState;

	int			m_nChokedPackets;	//number of choked packets
	int			m_PacketDrop;

// Reliable data buffer, send wich each packet (or put in waiting list)
	bf_write	m_StreamReliable;
	byte		m_ReliableDataBuffer[8 * 1024];	// In SP, we don't need much reliable buffer, so save the memory (this is mostly for xbox).
	CUtlVector<byte> m_ReliableDataBufferMP;

	// unreliable message buffer, cleared wich each packet
	bf_write	m_StreamUnreliable;
	byte		m_UnreliableDataBuffer[NET_MAX_DATAGRAM_PAYLOAD];

// don't use any vars below this (only in net_ws.cpp)

	CUDPSocket	*m_pSocket;   // NS_SERVER or NS_CLIENT index, depending on channel.
	int			m_StreamSocket;	// TCP socket handle

	unsigned int m_MaxReliablePayloadSize;	// max size of reliable payload in a single packet	

	// Address this channel is talking to.
	netadr_t	remote_address;  
	
	// For timeouts.  Time last message was received.
	float		last_received;		
	// Time when channel was connected.
	float      connect_time;       

	// Bandwidth choke
	// Bytes per second
	int			m_Rate;				
	// If realtime > cleartime, free to send next packet
	float		m_fClearTime;

	float		m_Timeout;		// in seconds 

	char			m_Name[32];		// channel name

// packet history
	// netflow_t		m_DataFlow[ MAX_FLOWS ];  

	INetworkMessageHandler			*m_MessageHandler;	// who registers and processes messages
};


#endif // NETCHANNEL_H