//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: net_chan.h
//
//=============================================================================//

#ifndef NET_CHAN_H
#define NET_CHAN_H
#ifdef _WIN32
#pragma once
#endif

#include "net.h"
#include "netadr.h"
#include "qlimits.h"
#include "bitbuf.h"
#include <inetmessage.h>
#include <filesystem.h>
#include "utlvector.h"
#include "utlbuffer.h"
#include "const.h"
#include "inetchannel.h"

// How fast to converge flow estimates
#define FLOW_AVG ( 3.0 / 4.0 )
 // Don't compute more often than this
#define FLOW_INTERVAL 0.25


#define NET_FRAMES_BACKUP	64		// must be power of 2
#define NET_FRAMES_MASK		(NET_FRAMES_BACKUP-1)
#define MAX_SUBCHANNELS		8		// we have 8 alternative send&wait bits

#define SUBCHANNEL_FREE		0	// subchannel is free to use
#define SUBCHANNEL_TOSEND	1	// subchannel has data, but not send yet
#define SUBCHANNEL_WAITING	2   // sbuchannel sent data, waiting for ACK
#define SUBCHANNEL_DIRTY	3	// subchannel is marked as dirty during changelevel


class CNetChan : public INetChannel
{

private: // netchan structurs

	typedef struct dataFragments_s
	{
		FileHandle_t	file;			// open file handle
		char			filename[MAX_OSPATH]; // filename
		char*			buffer;			// if NULL it's a file
		unsigned int	bytes;			// size in bytes
		unsigned int	bits;			// size in bits
		unsigned int	transferID;		// only for files
		bool			isCompressed;	// true if data is bzip compressed
		unsigned int	nUncompressedSize; // full size in bytes
		bool			asTCP;			// send as TCP stream
		int				numFragments;	// number of total fragments
		int				ackedFragments; // number of fragments send & acknowledged
		int				pendingFragments; // number of fragments send, but not acknowledged yet
	} dataFragments_t;

	struct subChannel_s
	{
		int				startFraggment[MAX_STREAMS];
		int				numFragments[MAX_STREAMS];
		int				sendSeqNr;
		int				state; // 0 = free, 1 = scheduled to send, 2 = send & waiting, 3 = dirty
		int				index; // index in m_SubChannels[]

		void Free()
		{
			state = SUBCHANNEL_FREE;
			sendSeqNr = -1;
			for ( int i = 0; i < MAX_STREAMS; i++ )
			{
				numFragments[i] = 0;
				startFraggment[i] = -1;
			}
		}
	};

	// Client's now store the command they sent to the server and the entire results set of
	//  that command. 
	typedef struct netframe_s
	{
		// Data received from server
		float			time;			// net_time received/send
		int				size;			// total size in bytes
		float			latency;		// raw ping for this packet, not cleaned. set when acknowledged otherwise -1.
		float			avg_latency;	// averaged ping for this packet
		bool			valid;			// false if dropped, lost, flushed
		int				choked;			// number of previously chocked packets
		int				dropped;
		float			m_flInterpolationAmount;
		unsigned short	msggroups[INetChannelInfo::TOTAL];	// received bytes for each message group
	} netframe_t;

	typedef struct
	{
		float		nextcompute;	// Time when we should recompute k/sec data
		float		avgbytespersec;	// average bytes/sec
		float		avgpacketspersec;// average packets/sec
		float		avgloss;		// average packet loss [0..1]
		float		avgchoke;		// average packet choke [0..1]
		float		avglatency;		// average ping, not cleaned
		float		latency;		// current ping, more accurate also more jittering
		int			totalpackets;	// total processed packets
		int			totalbytes;		// total processed bytes
		int			currentindex;		// current frame index
		netframe_t	frames[ NET_FRAMES_BACKUP ]; // frame history
		netframe_t	*currentframe;	// current frame
	} netflow_t;

public: 
	CNetChan();
	~CNetChan();

public:	// INetChannelInfo interface
	
	const char  *GetName( void ) const;
	const char  *GetAddress( void ) const;
	float		GetTime( void ) const;
	float		GetTimeConnected( void ) const;
	float		GetTimeSinceLastReceived( void ) const;
	int			GetDataRate( void ) const;
	int			GetBufferSize( void ) const;
		
	bool		IsLoopback( void ) const;
	bool		IsNull() const; // .dem file playback channel is of type NA_NULL!!!
	bool		IsTimingOut( void ) const;
	bool		IsPlayback( void ) const;

	float		GetLatency( int flow ) const;
	float		GetAvgLatency( int flow ) const;
	float		GetAvgLoss( int flow ) const;
	float		GetAvgData( int flow ) const;
	float		GetAvgChoke( int flow ) const;
	float		GetAvgPackets( int flow ) const;
	int			GetTotalData( int flow ) const;
	int			GetSequenceNr( int flow ) const ;
	bool		IsValidPacket( int flow, int frame_number ) const ;
	float		GetPacketTime( int flow, int frame_number ) const ;
	int			GetPacketBytes( int flow, int frame_number, int group ) const ; 
	bool		GetStreamProgress( int flow, int *received, int *total ) const;
	float		GetCommandInterpolationAmount( int flow, int frame_number ) const;
	void		GetPacketResponseLatency( int flow, int frame_number, int *pnLatencyMsecs, int *pnChoke ) const;
	void		GetRemoteFramerate( float *pflFrameTime, float *pflFrameTimeStdDeviation ) const;
	float		GetTimeoutSeconds() const;

public:	// INetChannel interface

	void		SetDataRate(float rate);
	bool		RegisterMessage(INetMessage *msg);
	bool		StartStreaming( unsigned int challengeNr );
	void		ResetStreaming( void );
	void		SetTimeout(float seconds);
	void		SetDemoRecorder(IDemoRecorder *recorder);
	void		SetChallengeNr(unsigned int chnr);
	
	void		Reset( void );
	void		Clear( void );
	void		Shutdown(const char * reason);
	
	void		ProcessPlayback( void );
	bool		ProcessStream( void );
	void		ProcessPacket( netpacket_t * packet, bool bHasHeader );

	void		SetCompressionMode( bool bUseCompression );
	void		SetFileTransmissionMode(bool bBackgroundMode);
	bool		SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false ); // send a net message
	bool		SendData(bf_write &msg, bool bReliable = true); // send a chunk of data
	bool		SendFile(const char *filename, unsigned int transferID); // transmit a local file
	void		SetChoked( void ); // choke a packet
	int			SendDatagram(bf_write *data); // build and send datagram packet
	unsigned int RequestFile(const char *filename); // request remote file to upload, returns request ID
	void RequestFile_OLD(const char *filename, unsigned int transferID); // request remote file to upload, returns request ID
	void		DenyFile(const char *filename, unsigned int transferID); // deny a file request
	bool		Transmit(bool onlyReliable = false); // send data from buffers
	
	const netadr_t	&GetRemoteAddress( void ) const;
	INetChannelHandler *GetMsgHandler( void ) const;
	int				GetDropNumber( void ) const;
	int				GetSocket( void ) const;
	unsigned int	GetChallengeNr( void ) const;
	void			GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck );
	void			SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck );
		
	void		UpdateMessageStats( int msggroup, int bits);
	bool		CanPacket( void ) const;
	bool		IsOverflowed( void ) const;
	bool		IsTimedOut( void ) const;
	bool		HasPendingReliableData( void );
	void		SetMaxBufferSize(bool bReliable, int nBytes, bool bVoice = false );
	virtual int		GetNumBitsWritten( bool bReliable );
	virtual void	SetInterpolationAmount( float flInterpolationAmount );
	virtual void	SetRemoteFramerate( float flFrameTime, float flFrameTimeStdDeviation );

	// Max # of payload bytes before we must split/fragment the packet
	virtual void	SetMaxRoutablePayloadSize( int nSplitSize );
	virtual int	GetMaxRoutablePayloadSize();

	virtual int		GetProtocolVersion();

	int			IncrementSplitPacketSequence();

public:

	static bool	IsValidFileForTransfer( const char *pFilename );

	void		Setup(int sock, netadr_t *adr, const char * name, INetChannelHandler * handler, int nProtocolVersion);
	// Send queue management
	void		IncrementQueuedPackets();
	void		DecrementQueuedPackets();
	bool		HasQueuedPackets() const;

private:
	
	void	FlowReset( void );
	void	FlowUpdate( int flow, int addbytes  );
	void	FlowNewPacket(int flow, int seqnr, int acknr, int nChoked, int nDropped, int nSize );
	
	bool	ProcessMessages( bf_read &buf );
	bool	ProcessControlMessage( int cmd, bf_read &buf);
	bool	SendReliableViaStream( dataFragments_t *data);
	bool	SendReliableAcknowledge( int seqnr );
	int		ProcessPacketHeader( netpacket_t *packet );
	void	AcknowledgeSubChannel(int seqnr, int list );

	bool	CreateFragmentsFromBuffer( bf_write *buffer, int stream );
	bool	CreateFragmentsFromFile( const char *filename, int stream, unsigned int transferID);

	void	CompressFragments();
	void	UncompressFragments( dataFragments_t *data );

	bool	SendSubChannelData( bf_write &buf );
	bool	ReadSubChannelData( bf_read &buf, int stream );
	void	AcknowledgeSeqNr( int seqnr );
	void	CheckWaitingList(int nList);
	bool	CheckReceivingList(int nList);
	void	RemoveHeadInWaitingList( int nList );
	bool	IsFileInWaitingList( const char *filename );
	subChannel_s *GetFreeSubChannel(); // NULL == all subchannels in use
	void	UpdateSubChannels( void );
	void	SendTCPData( void );

	INetMessage *FindMessage(int type);

	static bool HandleUpload( dataFragments_t *data, INetChannelHandler *MessageHandler );

#ifdef STAGING_ONLY
public:
	static bool TestUpload( const char *filename );
#endif

public:

	bool		m_bProcessingMessages;
	bool		m_bClearedDuringProcessing;
	bool		m_bShouldDelete;

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
	
		
	// Reliable data buffer, send which each packet (or put in waiting list)
	bf_write	m_StreamReliable;
	CUtlMemory<byte> m_ReliableDataBuffer;

	// unreliable message buffer, cleared which each packet
	bf_write	m_StreamUnreliable;
	CUtlMemory<byte> m_UnreliableDataBuffer;

	bf_write	m_StreamVoice;
	CUtlMemory<byte> m_VoiceDataBuffer;

// don't use any vars below this (only in net_ws.cpp)

	int			m_Socket;   // NS_SERVER or NS_CLIENT index, depending on channel.
	int			m_StreamSocket;	// TCP socket handle

	unsigned int m_MaxReliablePayloadSize;	// max size of reliable payload in a single packet	

	// Address this channel is talking to.
	netadr_t	remote_address;  
	
	// For timeouts.  Time last message was received.
	float		last_received;		
	// Time when channel was connected.
	double      connect_time;       

	// Bandwidth choke
	// Bytes per second
	int			m_Rate;				
	// If realtime > cleartime, free to send next packet
	double		m_fClearTime;

 	CUtlVector<dataFragments_t*>	m_WaitingList[MAX_STREAMS];	// waiting list for reliable data and file transfer
	dataFragments_t					m_ReceiveList[MAX_STREAMS]; // receive buffers for streams
	subChannel_s					m_SubChannels[MAX_SUBCHANNELS];

	unsigned int	m_FileRequestCounter;	// increasing counter with each file request
	bool			m_bFileBackgroundTranmission; // if true, only send 1 fragment per packet
	bool			m_bUseCompression;	// if true, larger reliable data will be bzip compressed
	
	// TCP stream state maschine:
	bool		m_StreamActive;		// true if TCP is active
	int			m_SteamType;		// STREAM_CMD_*
	int			m_StreamSeqNr;		// each blob send of TCP as an increasing ID
	int			m_StreamLength;		// total length of current stream blob
	int			m_StreamReceived;	// length of already received bytes
	char		m_SteamFile[MAX_OSPATH];	// if receiving file, this is it's name
	CUtlMemory<byte> m_StreamData;			// Here goes the stream data (if not file). Only allocated if we're going to use it.

	// packet history
	netflow_t		m_DataFlow[ MAX_FLOWS ];  
	int				m_MsgStats[INetChannelInfo::TOTAL];	// total bytes for each message group


	int				m_PacketDrop;	// packets lost before getting last update (was global net_drop)

	char			m_Name[32];		// channel name
	
	unsigned int	m_ChallengeNr;	// unique, random challenge number 

	float		m_Timeout;		// in seconds 

	INetChannelHandler			*m_MessageHandler;	// who registers and processes messages
	CUtlVector<INetMessage*>	m_NetMessages;		// list of registered message
	IDemoRecorder				*m_DemoRecorder;			// if != NULL points to a recording/playback demo object
	int							m_nQueuedPackets;

	float						m_flInterpolationAmount;
	float						m_flRemoteFrameTime;
	float						m_flRemoteFrameTimeStdDeviation;
	int							m_nMaxRoutablePayloadSize;

	int							m_nSplitPacketSequence;
	bool						m_bStreamContainsChallenge;  // true if PACKET_FLAG_CHALLENGE was set when receiving packets from the sender

	int							m_nProtocolVersion;		// PROTOCOL_VERSION if we're not playing a demo - otherwise, whatever was in the demo header's networkprotocol if the CNetChan instance was created by a demo player.
};


#endif // NET_CHAN_H
