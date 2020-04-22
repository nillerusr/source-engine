//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: net_chan.cpp: implementation of the CNetChan_t struct.
//
//=============================================================================//

#include "../utils/bzip2/bzlib.h"
#include "net_chan.h"
#include "tier1/strtools.h"
#include "filesystem_engine.h"
#include "demo.h"
#include "convar.h"
#include "mathlib/mathlib.h"
#include "protocol.h"
#include "inetmsghandler.h"
#include "host.h"
#include "netmessages.h"
#include "tier0/vcrmode.h"
#include "tier0/etwprof.h"
#include "tier0/vprof.h"
#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"
#include "filesystem_init.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ConVar net_showudp( "net_showudp", "0", 0, "Dump UDP packets summary to console" );
ConVar net_showtcp( "net_showtcp", "0", 0, "Dump TCP stream summary to console" );
ConVar net_blocksize( "net_maxfragments", NETSTRING( MAX_ROUTABLE_PAYLOAD ), 0, "Max fragment bytes per packet", true, FRAGMENT_SIZE, true, MAX_ROUTABLE_PAYLOAD );

static ConVar net_showmsg( "net_showmsg", "0", 0, "Show incoming message: <0|1|name>" );
static ConVar net_showfragments( "net_showfragments", "0", 0, "Show netchannel fragments" );
static ConVar net_showpeaks( "net_showpeaks", "0", 0, "Show messages for large packets only: <size>" );
static ConVar net_blockmsg( "net_blockmsg", "none", FCVAR_CHEAT, "Discards incoming message: <0|1|name>" );
static ConVar net_showdrop( "net_showdrop", "0", 0, "Show dropped packets in console" );
static ConVar net_drawslider( "net_drawslider", "0", 0, "Draw completion slider during signon" );
static ConVar net_chokeloopback( "net_chokeloop", "0", 0, "Apply bandwidth choke to loopback packets" );
static ConVar net_maxfilesize( "net_maxfilesize", "16", 0, "Maximum allowed file size for uploading in MB", true, 0, true, 64 );
static ConVar net_compresspackets( "net_compresspackets", "1", 0, "Use compression on game packets." );
static ConVar net_compresspackets_minsize( "net_compresspackets_minsize", "1024", 0, "Don't bother compressing packets below this size." );
static ConVar net_maxcleartime( "net_maxcleartime", "4.0", 0, "Max # of seconds we can wait for next packets to be sent based on rate setting (0 == no limit)." );
static ConVar net_maxpacketdrop( "net_maxpacketdrop", "5000", 0, "Ignore any packets with the sequence number more than this ahead (0 == no limit)" );

extern ConVar net_maxroutable;

extern int  NET_ConnectSocket( int nSock, const netadr_t &addr );
extern void NET_CloseSocket( int hSocket, int sock = -1 );
extern int  NET_SendStream( int nSock, const char * buf, int len, int flags );
extern int  NET_ReceiveStream( int nSock, char * buf, int len, int flags );


// If the network connection hasn't been active in this many seconds, display some warning text.
#define CONNECTION_PROBLEM_TIME		4.0f	// assume network problem after this time

#define BYTES2FRAGMENTS(i) ((i+FRAGMENT_SIZE-1)/FRAGMENT_SIZE)

#define FLIPBIT(v,b) if (v&b) v &= ~b; else v |= b;

// We only need to checksum packets on the PC and only when we're actually sending them over the network.
static bool ShouldChecksumPackets()
{
	if ( !IsPC() )
		return false;

	return NET_IsMultiplayer();
}

bool CNetChan::IsLoopback() const
{
	return remote_address.IsLoopback();		
}

bool CNetChan::IsNull() const
{
	return remote_address.GetType() == NA_NULL ? true : false;
}

/*
==============================
CNetChan::Clear

==============================
*/
void CNetChan::Clear()
{
	int i;

	// clear waiting lists

	for ( i=0; i<MAX_STREAMS; i++ )
	{
		while ( m_WaitingList[i].Count() )
			RemoveHeadInWaitingList( i );	

		if ( m_ReceiveList[i].buffer )
		{
			delete[] m_ReceiveList[i].buffer;
			m_ReceiveList[i].buffer = NULL;
		}
	}

	for( i=0; i<MAX_SUBCHANNELS; i++ )
	{
		if ( m_SubChannels[i].state == SUBCHANNEL_TOSEND )
		{
			int bit = 1<<i; // flip bit back since data was send yet
			
			FLIPBIT(m_nOutReliableState, bit);

			m_SubChannels[i].Free(); 
		}
		else if ( m_SubChannels[i].state == SUBCHANNEL_WAITING )
		{
			// data is already out, mark channel as dirty
			m_SubChannels[i].state = SUBCHANNEL_DIRTY;
		}
	}

	if ( m_bProcessingMessages )
	{
		// ProcessMessages() needs to know we just nuked the receive list from under it or bad things ensue.
		m_bClearedDuringProcessing = true;
	}

	Reset();
}


void CNetChan::CompressFragments()
{
	// We don't want this to go in the VCR file, because the compressed size can be different. The reason is 
	// that the bf_writes that contributed to this message may have uninitialized bits at the end of the buffer
	// (for example if it uses only the first couple bits of the last byte in the message). If the
	// last few bits are different, it can produce a different compressed size.
	if ( !m_bUseCompression )
		return;

	if ( VCRGetMode() != VCR_Disabled )
		return;

	VPROF_BUDGET( "CNetChan::CompressFragments", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// write fragemnts for both streams
	for ( int i=0; i<MAX_STREAMS; i++ )
	{
		if ( m_WaitingList[i].Count() == 0 )
			continue;

		// get the first fragments block which is send next
		dataFragments_t *data = m_WaitingList[i][0];

		// if data is already compressed or too small, skip it
		if ( data->isCompressed || (int)data->bytes < net_compresspackets_minsize.GetInt() )
			continue;

		// if we already started sending this block, we can't compress it anymore
		if ( data->ackedFragments > 0 || data->pendingFragments > 0 )
			continue;

		//ok, compress it.

		if ( data->buffer )	
		{
			CFastTimer compressTimer;
			compressTimer.Start();

			// fragments data is in memory
			unsigned int compressedSize = COM_GetIdealDestinationCompressionBufferSize_Snappy( data->bytes );
			char * compressedData = new char[ compressedSize ];

			if ( COM_BufferToBufferCompress_Snappy( compressedData, &compressedSize, data->buffer, data->bytes ) &&
				( compressedSize < data->bytes ) )
			{
				compressTimer.End(); 
				DevMsg("Compressing fragments (%d -> %d bytes): %.2fms\n",
						data->bytes, compressedSize, compressTimer.GetDuration().GetMillisecondsF() );

				// copy compressed data but dont reallocate memory
				Q_memcpy( data->buffer, compressedData, compressedSize );

				data->nUncompressedSize = data->bytes;
				data->bytes = compressedSize;
				data->numFragments = BYTES2FRAGMENTS(data->bytes);
				data->isCompressed = true;				
			}

			delete [] compressedData; // free temp buffer
		}
		else // it's a file
		{
			Assert( data->file != FILESYSTEM_INVALID_HANDLE );

			char compressedfilename[ MAX_OSPATH ];
			int compressedFileSize = -1;
			FileHandle_t hZipFile = FILESYSTEM_INVALID_HANDLE;

			// check to see if there is a compressed version of the file
			Q_snprintf( compressedfilename, sizeof(compressedfilename), "%s.ztmp", data->filename);

			// check the timestamps 
			int compressedFileTime = g_pFileSystem->GetFileTime( compressedfilename );
			int fileTime = g_pFileSystem->GetFileTime( data->filename );

			if ( compressedFileTime >= fileTime )
			{
				// compressed file is newer than uncompressed file, use this one
				hZipFile = g_pFileSystem->Open( compressedfilename, "rb", NULL );
			}

			if ( hZipFile != FILESYSTEM_INVALID_HANDLE )
			{
				// use the existing compressed file
				compressedFileSize = g_pFileSystem->Size( hZipFile );
			}
			else
			{
				// create compressed version of source file
				unsigned int uncompressedSize = data->bytes;
				unsigned int compressedSize = COM_GetIdealDestinationCompressionBufferSize_Snappy( uncompressedSize );
				char *uncompressed = new char[uncompressedSize];
				char *compressed = new char[compressedSize];
					
				// read in source file
				g_pFileSystem->Read( uncompressed, data->bytes, data->file );

				// compress into buffer
				if ( COM_BufferToBufferCompress_Snappy( compressed, &compressedSize, uncompressed, uncompressedSize ) )
				{
					// write out to disk compressed version
					hZipFile = g_pFileSystem->Open( compressedfilename, "wb", NULL );

					if ( hZipFile != FILESYSTEM_INVALID_HANDLE )
					{
						DevMsg("Creating compressed version of file %s (%d -> %d)\n", data->filename, data->bytes, compressedSize);
						g_pFileSystem->Write( compressed, compressedSize, hZipFile );
						g_pFileSystem->Close( hZipFile );

						// and open zip file it again for reading
						hZipFile = g_pFileSystem->Open( compressedfilename, "rb", NULL );

						if ( hZipFile != FILESYSTEM_INVALID_HANDLE )
						{
							// ok, now everything if fine
							compressedFileSize = compressedSize;
						}
					}
				}
				
				delete [] uncompressed;
				delete [] compressed;
			}

			if ( compressedFileSize > 0 )
			{
				// use compressed file handle instead of original file
				g_pFileSystem->Close( data->file );
				data->file = hZipFile;
				data->nUncompressedSize = data->bytes;
				data->bytes = compressedFileSize;
				data->numFragments = BYTES2FRAGMENTS(data->bytes);
				data->isCompressed = true;
			}
		}
	}
}

void CNetChan::UncompressFragments( dataFragments_t *data )
{
	if ( !data->isCompressed )
		return;

	 // allocate buffer for uncompressed data, align to 4 bytes boundary
	char *newbuffer = new char[PAD_NUMBER( data->nUncompressedSize, 4 )];
	unsigned int uncompressedSize = data->nUncompressedSize;

	// uncompress data
	COM_BufferToBufferDecompress( newbuffer, &uncompressedSize, data->buffer, data->bytes );

	Assert( uncompressedSize == data->nUncompressedSize );

	// free old buffer and set new buffer
	delete [] data->buffer;
	data->buffer = newbuffer;
	data->bytes = uncompressedSize;
	data->isCompressed = false;
}

unsigned int CNetChan::RequestFile(const char *filename)
{
	m_FileRequestCounter++;

	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg("RequestFile: %s (ID %i)\n", filename, m_FileRequestCounter );
	}

	m_StreamReliable.WriteUBitLong( net_File, NETMSG_TYPE_BITS );
	m_StreamReliable.WriteUBitLong( m_FileRequestCounter, 32 );
	m_StreamReliable.WriteString( filename );
	m_StreamReliable.WriteOneBit( 1 ); // reqest this file

	return m_FileRequestCounter;
}

void CNetChan::RequestFile_OLD(const char *filename, unsigned int transferID)
{
	Error( "Called RequestFile_OLD" );
}

void CNetChan::DenyFile(const char *filename, unsigned int transferID)
{
	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg("DenyFile: %s (ID %i)\n", filename, transferID );
	}

	m_StreamReliable.WriteUBitLong( net_File, NETMSG_TYPE_BITS );
	m_StreamReliable.WriteUBitLong( transferID, 32 );
	m_StreamReliable.WriteString( filename );
	m_StreamReliable.WriteOneBit( 0 ); // deny this file
}

bool CNetChan::SendFile(const char *filename, unsigned int transferID)
{
	// add file to waiting list
	if ( remote_address.GetType() == NA_NULL )
		return true;

	if ( !filename )
		return false;

	const char *sendfile = filename;
	while( sendfile[0] && PATHSEPARATOR( sendfile[0] ) )
	{
		sendfile = sendfile + 1;
	}

	// Don't transfer exe, vbs, com, bat-type files.
	if ( !IsValidFileForTransfer( sendfile ) )
		return false;

	if ( !CreateFragmentsFromFile( sendfile, FRAG_FILE_STREAM, transferID	) )
	{
		DenyFile( sendfile, transferID ); // send host a deny message
		return false;
	}

	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg("SendFile: %s (ID %i)\n", sendfile, transferID );
	}

	return true;
}

void CNetChan::Shutdown(const char *pReason)
{
	// send discconect

	if ( m_Socket < 0 )
		return;

	Clear(); // free all buffers (reliable & unreliable)

	if ( pReason )
	{
		// send disconnect message
		m_StreamUnreliable.WriteUBitLong( net_Disconnect, NETMSG_TYPE_BITS );
		m_StreamUnreliable.WriteString( pReason );
		Transmit();	// push message out
	}

	if ( m_StreamSocket )
	{
		NET_CloseSocket( m_StreamSocket, m_Socket );
		m_StreamSocket = 0;
		m_StreamActive = false;
	}

	m_Socket = -1; // signals that netchannel isn't valid anymore

	remote_address.Clear();

	if ( m_MessageHandler )
	{
		m_MessageHandler->ConnectionClosing( pReason );
		m_MessageHandler = NULL;
	}

	// free new messages

	for (int i=0; i<m_NetMessages.Count(); i++ )
	{
		Assert( m_NetMessages[i] );
		delete m_NetMessages[i];
	}

	m_NetMessages.Purge();

	m_DemoRecorder = NULL;

	if ( m_bProcessingMessages )
	{
		NET_RemoveNetChannel( this, false );	// Delay the deletion or it'll crash in the message-processing loop.
		m_bShouldDelete = true;
	}
	else
	{
		NET_RemoveNetChannel( this, true );
	}
}

CNetChan::CNetChan()
{
	m_nSplitPacketSequence = 1;
	m_nMaxRoutablePayloadSize = MAX_ROUTABLE_PAYLOAD;
	m_bProcessingMessages = false;
	m_bShouldDelete = false;
	m_bClearedDuringProcessing = false;
	m_bStreamContainsChallenge = false;
	m_Socket = -1; // invalid
	remote_address.Clear();
	last_received = 0;
	connect_time = 0;
	m_nProtocolVersion = -1;	// invalid
	
	Q_strncpy( m_Name, "", sizeof(m_Name) ); 

	m_MessageHandler = NULL;
	m_DemoRecorder = NULL;

	m_StreamUnreliable.SetDebugName( "netchan_t::unreliabledata" );
	m_StreamReliable.SetDebugName( "netchan_t::reliabledata" );

	m_Rate		= DEFAULT_RATE;
	m_Timeout	= SIGNON_TIME_OUT;

	// Prevent the first message from getting dropped after connection is set up.

	m_nOutSequenceNr = 1;	// otherwise it looks like a 	
	m_nInSequenceNr = 0;
	m_nOutSequenceNrAck = 0;
	m_nOutReliableState = 0; // our current reliable state
	m_nInReliableState = 0;	// last remote reliable state
	// m_nLostPackets = 0;

	m_ChallengeNr = 0;

	m_StreamSocket = 0;
	m_StreamActive = false;

	ResetStreaming();

	m_MaxReliablePayloadSize = 	NET_MAX_PAYLOAD;

	m_FileRequestCounter = 0;
	m_bFileBackgroundTranmission = true;
	m_bUseCompression = false;
	m_nQueuedPackets = 0;

	m_flRemoteFrameTime = 0;
	m_flRemoteFrameTimeStdDeviation = 0;

	FlowReset();
}

CNetChan::~CNetChan()
{
	Shutdown("NetChannel removed.");
}

/*
==============
CNetChan::Setup

called to open a channel to a remote system
==============
*/
void CNetChan::Setup(int sock, netadr_t *adr, const char * name, INetChannelHandler * handler,
					 int nProtocolVersion )
{
	Assert( name ); 
	Assert ( handler );

	m_Socket = sock;

	if ( m_StreamSocket )
	{
		NET_CloseSocket( m_StreamSocket );
		m_StreamSocket = 0;
	}

	// remote_address may be NULL for fake channels (demo playback etc)
	if ( adr )
	{
		remote_address = *adr;
	}
	else
	{
		remote_address.Clear();	// it's a demo fake channel
		remote_address.SetType( NA_NULL );
	}
	
	last_received		= net_time;
	connect_time		= net_time;
	
	Q_strncpy( m_Name, name, sizeof(m_Name) ); 

	m_MessageHandler = handler;
	m_nProtocolVersion = nProtocolVersion;

	m_DemoRecorder = NULL;

	MEM_ALLOC_CREDIT();

	SetMaxBufferSize( false, NET_MAX_DATAGRAM_PAYLOAD );

	SetMaxBufferSize( false, NET_MAX_DATAGRAM_PAYLOAD, true ); //Set up voice buffer

	SetMaxBufferSize( true, NET_MAX_PAYLOAD );

	m_Rate		= DEFAULT_RATE;
	m_Timeout	= SIGNON_TIME_OUT;

	// Prevent the first message from getting dropped after connection is set up.

	m_nOutSequenceNr = 1;	// otherwise it looks like a 	
	m_nInSequenceNr = 0;
	m_nOutSequenceNrAck = 0;
	m_nOutReliableState = 0; // our current reliable state
	m_nInReliableState = 0;	// last remote reliable state
	m_nChokedPackets = 0;
	m_fClearTime = 0.0;
	
	m_ChallengeNr = 0;

	m_StreamSocket = 0;
	m_StreamActive = false;

	m_ReceiveList[FRAG_NORMAL_STREAM].buffer = NULL;
	m_ReceiveList[FRAG_FILE_STREAM].buffer = NULL;

	// init 8 subchannels
	for ( int i=0; i<MAX_SUBCHANNELS; i++ )
	{
		m_SubChannels[i].index = i; // set index once
		m_SubChannels[i].Free();
	}
	
	ResetStreaming();

	if ( NET_IsMultiplayer() )
	{
		m_MaxReliablePayloadSize = 	net_blocksize.GetInt();
	}
	else
	{
		m_MaxReliablePayloadSize = 	NET_MAX_PAYLOAD;
	}

	FlowReset();

	// tell message handler to register known netmessages
	m_MessageHandler->ConnectionStart( this );
}

void CNetChan::ResetStreaming( void )
{
	m_SteamType = STREAM_CMD_NONE;
	m_StreamLength = 0;	
	m_StreamReceived = 0;
	m_StreamSeqNr = 0;
	m_SteamFile[0] = 0;	
}

bool CNetChan::StartStreaming( unsigned int challengeNr )
{
	// reset stream state maschine
	ResetStreaming();

	m_ChallengeNr = challengeNr;
	
	if ( !NET_IsMultiplayer() )
	{
		m_StreamSocket = 0;
		return true;	// streaming is done via loopback buffers in SP mode
	}

#ifdef _XBOX
	// We don't want to go into here because it'll eat up 192k extra memory in the client and server's m_StreamData.
	Error( "StartStreaming not allowed on XBOX." );
#endif

	MEM_ALLOC_CREDIT();

	m_StreamSocket = NET_ConnectSocket( m_Socket, remote_address );
	m_StreamData.EnsureCapacity( NET_MAX_PAYLOAD );

	return (m_StreamSocket != 0);
}

void CNetChan::SetChallengeNr(unsigned int chnr)
{
	m_ChallengeNr = chnr;
}

unsigned int CNetChan::GetChallengeNr( void )  const
{
	return m_ChallengeNr;
}

void CNetChan::GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck )
{
	nOutSequenceNr = m_nOutSequenceNr;
	nInSequenceNr = m_nInSequenceNr;
	nOutSequenceNrAck = m_nOutSequenceNrAck;
}

void CNetChan::SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck )
{
	Assert( IsPlayback() );

	m_nOutSequenceNr = nOutSequenceNr;
	m_nInSequenceNr = nInSequenceNr;
	m_nOutSequenceNrAck = nOutSequenceNrAck;
}

void CNetChan::SetDemoRecorder(IDemoRecorder * recorder)
{
	m_DemoRecorder = recorder;
}

void CNetChan::SetTimeout(float seconds)
{
	m_Timeout = seconds;

	if ( m_Timeout > 3600.0f )
	{
		m_Timeout = 3600.0f; // 1 hour maximum
	} 
	else if ( m_Timeout <= 0.0f )
	{
		m_Timeout = -1.0f;	// never time out (demo files)
	}
	else if ( m_Timeout < CONNECTION_PROBLEM_TIME )
	{
		m_Timeout = CONNECTION_PROBLEM_TIME; // allow at least this minimum
	}
}

void CNetChan::SetMaxBufferSize(bool bReliable, int nBytes, bool bVoice )
{
	// force min/max sizes 4-96kB
	nBytes = clamp( nBytes, NET_MAX_DATAGRAM_PAYLOAD, NET_MAX_PAYLOAD );

	bf_write *stream;
	CUtlMemory<byte> *buffer;

	if ( bReliable )
	{
		stream = &m_StreamReliable;
		buffer = &m_ReliableDataBuffer;
	}
	else if ( bVoice == true )
	{
		stream = &m_StreamVoice;
		buffer = &m_VoiceDataBuffer;
	}
	else
	{
		stream = &m_StreamUnreliable;
		buffer = &m_UnreliableDataBuffer;
	}

	if ( buffer->Count() == nBytes )
		return;

	byte	copybuf[NET_MAX_DATAGRAM_PAYLOAD];
	int		copybits = stream->GetNumBitsWritten();
	int		copybytes = Bits2Bytes( copybits );

	if ( copybytes >= nBytes )
	{
		ConMsg("CNetChan::SetMaxBufferSize: cant preserve exiting data %i>%i.\n", copybytes, nBytes );
		return;
	}

	if ( copybits > 0 )
	{
		Q_memcpy( copybuf, buffer->Base(), copybytes );
	}

	buffer->Purge();

	MEM_ALLOC_CREDIT();
	buffer->EnsureCapacity( nBytes );

	if ( copybits > 0 )
	{
		Q_memcpy( buffer->Base(), copybuf, copybytes );
	}

	stream->StartWriting( buffer->Base(), nBytes, copybits );
	
}

void CNetChan::SetFileTransmissionMode( bool bBackgroundMode )
{
	m_bFileBackgroundTranmission = bBackgroundMode;
}

void CNetChan::SetCompressionMode( bool bUseCompression )
{
	m_bUseCompression = bUseCompression;
}

void CNetChan::SetDataRate(float rate)
{
	m_Rate = clamp( rate, (float) MIN_RATE, (float) MAX_RATE );
}

const char * CNetChan::GetName() const
{
	return m_Name;
}

const char * CNetChan::GetAddress() const
{
	return remote_address.ToString();
}


int CNetChan::GetDropNumber() const
{
	return m_PacketDrop;
}

/*
===============
CNetChan::CanPacket

Returns true if the bandwidth choke isn't active
================
*/
bool CNetChan::CanPacket () const
{
	// Never choke loopback packets.
	if ( !net_chokeloopback.GetInt() && remote_address.IsLoopback() )
	{
		return true;
	}

	if ( HasQueuedPackets() )
	{
		return false;
	}

	return m_fClearTime < net_time;
}

bool CNetChan::IsPlayback( void ) const
{
#if !defined(SWDS) && !defined(_XBOX)
	return demoplayer->IsPlayingBack();
#else
	return false;
#endif
}

void CNetChan::FlowReset( void )
{
	Q_memset( m_DataFlow, 0, sizeof( m_DataFlow ) );
	Q_memset( m_MsgStats, 0, sizeof( m_MsgStats ) );
}

void CNetChan::FlowNewPacket(int flow, int seqnr, int acknr, int nChoked, int nDropped, int nSize )
{
	netflow_t * pflow = &m_DataFlow[ flow ];
	
	// if frame_number != ( current + 1 ) mark frames between as invalid

	netframe_t *pframe = NULL;

	if ( seqnr > pflow->currentindex )
	{
		//
		// The following loop must execute no more than NET_FRAMES_BACKUP times
		// since that's the amount of storage in frame_headers & frames arrays,
		// a malformed client packet pushing "seqnr" by 1,000,000 can cause this
		// loop to watchdog.
		//
		for ( int i = pflow->currentindex + 1, numPacketFramesOverflow = 0;
			( i <= seqnr ) && ( numPacketFramesOverflow < NET_FRAMES_BACKUP );
			++ i, ++ numPacketFramesOverflow )
		{
			int nBackTrack = seqnr - i;

			pframe = &pflow->frames[ i & NET_FRAMES_MASK ];

			pframe->time = net_time;	// now
			pframe->valid = false;
			pframe->size = 0;
			pframe->latency = -1.0f; // not acknowledged yet
			pframe->avg_latency = GetAvgLatency( FLOW_OUTGOING );
			pframe->choked = 0; // not acknowledged yet
			pframe->dropped = 0;
			pframe->m_flInterpolationAmount = 0.0f;
			Q_memset( &pframe->msggroups, 0, sizeof(pframe->msggroups) );

			if ( nBackTrack < ( nChoked + nDropped ) )
			{
				if ( nBackTrack < nChoked )
				{
					pframe->choked = 1;
				}
				else
				{
					pframe->dropped = 1;
				}
			}
		}

		pframe->dropped = nDropped;
		pframe->choked = nChoked;
		pframe->size = nSize;
		pframe->valid = true;
		pframe->avg_latency = GetAvgLatency( FLOW_OUTGOING );
		pframe->m_flInterpolationAmount = m_flInterpolationAmount;
	}
	else
	{
#if !defined( SWDS )
		Assert( demoplayer->IsPlayingBack() || seqnr > pflow->currentindex );
#endif
	}

	pflow->totalpackets++;
	pflow->currentindex = seqnr;
	pflow->currentframe = pframe;

	// updated ping for acknowledged packet

	int aflow = (flow==FLOW_OUTGOING) ? FLOW_INCOMING : FLOW_OUTGOING;

	if ( acknr <= (m_DataFlow[aflow].currentindex - NET_FRAMES_BACKUP) )
		return;	// acknowledged packet isn't in backup buffer anymore
	
	netframe_t * aframe = &m_DataFlow[aflow].frames[ acknr & NET_FRAMES_MASK ];

	if ( aframe->valid && aframe->latency == -1.0f )
	{
		// update ping for acknowledged packet, if not already acknowledged before
		
		aframe->latency = net_time - aframe->time;

		if ( aframe->latency < 0.0f )
			aframe->latency = 0.0f;
	}
	
}

void CNetChan::FlowUpdate(int flow, int addbytes)
{
	netflow_t * pflow = &m_DataFlow[ flow ];
	pflow->totalbytes += addbytes;

	if ( pflow->nextcompute > net_time )
		return;

	pflow->nextcompute = net_time + FLOW_INTERVAL;

	int		totalvalid = 0;
	int		totalinvalid = 0;
	int		totalbytes = 0;
	float	totallatency = 0.0f;
	int		totallatencycount = 0;
	int		totalchoked = 0;

	float   starttime = FLT_MAX;
	float	endtime = 0.0f;

	netframe_t *pprev = &pflow->frames[ NET_FRAMES_BACKUP-1 ];

	for ( int i = 0; i < NET_FRAMES_BACKUP; i++ )
	{
		// Most recent message then backward from there
		netframe_t * pcurr = &pflow->frames[ i ];

		if ( pcurr->valid )
		{
			if ( pcurr->time < starttime )
				starttime = pcurr->time;

			if ( pcurr->time > endtime )
				endtime = pcurr->time;
		
			totalvalid++;
			totalchoked += pcurr->choked;
			totalbytes += pcurr->size;

			if ( pcurr->latency > -1.0f  )
			{
				totallatency += pcurr->latency; 
				totallatencycount++;
			}
		}
		else
		{
			totalinvalid++;
		}
		
		pprev = pcurr;
	}

	float totaltime = endtime - starttime;

	if ( totaltime > 0.0f )
	{
		pflow->avgbytespersec *= FLOW_AVG;
		pflow->avgbytespersec += ( 1.0f - FLOW_AVG ) * ((float)totalbytes / totaltime);

		pflow->avgpacketspersec *= FLOW_AVG;
		pflow->avgpacketspersec += ( 1.0f - FLOW_AVG ) * ((float)totalvalid / totaltime);
	}

	int totalPackets = totalvalid + totalinvalid;
			
	if ( totalPackets > 0 )
	{
		pflow->avgloss *= FLOW_AVG;
		pflow->avgloss += ( 1.0f - FLOW_AVG ) * ((float)(totalinvalid-totalchoked)/totalPackets);

		if ( pflow->avgloss < 0 )
			pflow->avgloss = 0;
		
		pflow->avgchoke *= FLOW_AVG;
		pflow->avgchoke += ( 1.0f - FLOW_AVG ) * ((float)totalchoked/totalPackets);
	}
	
	if ( totallatencycount>0 )
	{
		float newping = totallatency / totallatencycount ;
		pflow->latency = newping;
		pflow->avglatency*= FLOW_AVG;
		pflow->avglatency += ( 1.0f - FLOW_AVG ) * newping;
	}
}

void CNetChan::SetChoked( void )
{
	m_nOutSequenceNr++;	// sends to be done since move command use sequence number
	m_nChokedPackets++;
}

bool CNetChan::Transmit(bool onlyReliable )
{
	if ( onlyReliable )
		m_StreamUnreliable.Reset();

	return (SendDatagram( NULL ) != 0);
}

bool CNetChan::IsFileInWaitingList( const char *filename )
{
	if ( !filename || !filename[0] )
		return true;

	for ( int stream=0; stream<MAX_STREAMS; stream++)
	{
		for ( int i = 0; i < m_WaitingList[stream].Count(); i++ )
		{
			dataFragments_t * data = m_WaitingList[stream][i]; 

			if ( !Q_strcmp( data->filename, filename ) )
				return true; // alread in list
		}
	}

	return false; // file not found
}

void CNetChan::RemoveHeadInWaitingList( int nList )
{
	Assert( m_WaitingList[nList].Count() );

	dataFragments_t * data = m_WaitingList[nList][0]; // get head

	if ( data->buffer )
		delete [] data->buffer;	// free data buffer

	if ( data->file	!= FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( data->file );
		data->file = FILESYSTEM_INVALID_HANDLE;
	}

	// data->fragments.Purge();

	m_WaitingList[nList].FindAndRemove( data );	// remove from list

	delete data;	//free structure itself
}


bool CNetChan::CreateFragmentsFromBuffer( bf_write *buffer, int stream )
{
	VPROF_BUDGET( "CNetChan::CreateFragmentsFromBuffer", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	bf_write bfwrite;
	dataFragments_t *data = NULL;

	// if we have more than one item in the waiting list, try to add the 
	// reliable data to the last item. that doesn't work with the first item
	// since it may have been already send and is waiting for acknowledge

	int count = m_WaitingList[stream].Count();

	if ( count > 1 )
	{
		// get last item in waiting list
		data = m_WaitingList[stream][count-1];

		int totalBytes = Bits2Bytes( data->bits + buffer->GetNumBitsWritten() );

		totalBytes = PAD_NUMBER( totalBytes, 4 ); // align to 4 bytes boundary

		if ( totalBytes < NET_MAX_PAYLOAD && data->buffer )
		{
			// we have enough space for it, create new larger mem buffer
			char *newBuf = new char[totalBytes];

			Q_memcpy( newBuf, data->buffer, data->bytes );

			delete [] data->buffer; // free old buffer

			data->buffer = newBuf; // set new buffer

			bfwrite.StartWriting( newBuf, totalBytes, data->bits );
		}
		else
		{
			data = NULL; // reset to NULL
		}
	}

	// if not added to existing item, create a new reliable data waiting buffer
	if ( !data )
	{
		int totalBytes = Bits2Bytes( buffer->GetNumBitsWritten());

		totalBytes = PAD_NUMBER( totalBytes, 4 ); // align to 4 bytes boundary

		data = new dataFragments_t;
		data->bytes = 0;	// not filled yet
		data->bits = 0;
		data->buffer = new char[ totalBytes ];
		data->isCompressed = false;
		data->nUncompressedSize = 0;
		data->file = FILESYSTEM_INVALID_HANDLE;
		data->filename[0] = 0;
		
		bfwrite.StartWriting( data->buffer, totalBytes );

		m_WaitingList[stream].AddToTail( data );	// that's it for now
	}

	// write new reliable data to buffer
	bfwrite.WriteBits( buffer->GetData(), buffer->GetNumBitsWritten() );

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = bfwrite.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		bfwrite.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	// update bit length
	data->bits += buffer->GetNumBitsWritten();
	data->bytes = Bits2Bytes(data->bits);
	
	// check if send as stream or with snapshot
	data->asTCP = m_StreamActive && ( data->bytes > m_MaxReliablePayloadSize );

	// calc number of fragments needed
	data->numFragments = BYTES2FRAGMENTS(data->bytes);
	data->ackedFragments = 0;
	data->pendingFragments = 0;

	return true;
}

bool CNetChan::CreateFragmentsFromFile( const char *filename, int stream, unsigned int transferID )
{
	if ( IsFileInWaitingList( filename ) )
		return true; // already scheduled for upload
	
	const char *pPathID = "GAME";

	if ( !g_pFileSystem->FileExists( filename, pPathID ) )
	{
		ConMsg( "CreateFragmentsFromFile: '%s' doesn't exist.\n", filename );
		return false;
	}

	int totalBytes = g_pFileSystem->Size( filename, pPathID );

	if ( totalBytes >= (net_maxfilesize.GetInt()*1024*1024) )
	{
		ConMsg( "CreateFragmentsFromFile: '%s' size exceeds net_maxfilesize limit (%i MB).\n", filename, net_maxfilesize.GetInt() );
		return false;
	}
	
	if ( totalBytes >= MAX_FILE_SIZE )
	{
		ConMsg( "CreateFragmentsFromFile: '%s' too big (max %i bytes).\n", filename, MAX_FILE_SIZE );
		return false;
	}

	dataFragments_t *data = new dataFragments_t;
	data->bytes = totalBytes;
	data->bits = data->bytes * 8;
	data->buffer = NULL;
	data->isCompressed = false;
	data->nUncompressedSize = 0;
	data->file = g_pFileSystem->Open( filename, "rb", pPathID );

	if ( data->file == FILESYSTEM_INVALID_HANDLE )
	{
		ConMsg( "CreateFragmentsFromFile: couldn't open '%s'.\n", filename );
		delete data;
		return false;
	}

	data->transferID = transferID;
	Q_strncpy( data->filename, filename, sizeof(data->filename) );

	m_WaitingList[stream].AddToTail( data );	// that's it for now

	// check if send as stream or with snapshot
	data->asTCP = false; // m_StreamActive && ( Bits2Bytes(data->length) > m_MaxReliablePayloadSize );

	// calc number of fragments needed
	data->numFragments = BYTES2FRAGMENTS(data->bytes);
	data->ackedFragments = 0;
	data->pendingFragments = 0;

	return true;
}

void CNetChan::SendTCPData( void )
{
	if ( m_WaitingList[FRAG_NORMAL_STREAM].Count() == 0 )
		return; // nothing in line

	dataFragments_t *data = m_WaitingList[FRAG_NORMAL_STREAM][0];

	if ( !data->asTCP )
		return; // not as TCP

	if ( data->pendingFragments > 0 )
		return; // already send, wait for ACK

	// OK send it now
	SendReliableViaStream( data );
}

bool CNetChan::SendSubChannelData( bf_write &buf )
{
	VPROF_BUDGET( "CNetChan::SendSubChannelData", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	subChannel_s *subChan = NULL;
	int i;

	CompressFragments();

	SendTCPData();

	UpdateSubChannels();

	// find subchannl with data to send/resend:
	for ( i=0; i<MAX_SUBCHANNELS; i++ )
	{
		subChan = &m_SubChannels[i];

		if ( subChan->state == SUBCHANNEL_TOSEND )
			break;
	}

	if ( i == MAX_SUBCHANNELS )
		return false; // no data to send in any subchannel

	// first write subchannel index
	buf.WriteUBitLong( i, 3 );

	// write fragemnts for both streams
	for ( i=0; i<MAX_STREAMS; i++ )
	{
		if ( subChan->numFragments[i] == 0 )
		{
			buf.WriteOneBit( 0 ); // no data for this stream
			continue;
		}

		dataFragments_t *data = m_WaitingList[i][0];

		buf.WriteOneBit( 1 ); // data follows:

		unsigned int offset = subChan->startFraggment[i]*FRAGMENT_SIZE;
		unsigned int length = subChan->numFragments[i]*FRAGMENT_SIZE;

		if ( (subChan->startFraggment[i]+subChan->numFragments[i]) == data->numFragments )
		{
			// we are sending the last fragment, adjust length
			int rest = FRAGMENT_SIZE - ( data->bytes % FRAGMENT_SIZE );
			if ( rest < FRAGMENT_SIZE )
				length -= rest;
		}

		// if all fragments can be send within a single packet, avoid overhead (if not a file)
		bool bSingleBlock = (subChan->numFragments[i] == data->numFragments) &&
							 ( data->file == FILESYSTEM_INVALID_HANDLE );

		if ( bSingleBlock )
		{	
			Assert( length == data->bytes );
			Assert( length < NET_MAX_PAYLOAD );
			Assert( offset == 0 );

			buf.WriteOneBit( 0 );	// single block bit

			// data compressed ?
			if ( data->isCompressed )
			{
				buf.WriteOneBit( 1 );
				buf.WriteUBitLong( data->nUncompressedSize, MAX_FILE_SIZE_BITS );
			}
			else
			{
				buf.WriteOneBit( 0 ); 
			}

			buf.WriteVarInt32( data->bytes );
		}
		else
		{
			buf.WriteOneBit( 1 ); // uses fragments with start fragment offset byte
			buf.WriteUBitLong( subChan->startFraggment[i], (MAX_FILE_SIZE_BITS-FRAGMENT_BITS) ); 
			buf.WriteUBitLong( subChan->numFragments[i], 3 ); 
		
			if ( offset == 0 )
			{
				// this is the first fragment, write header info
				
				if ( data->file != FILESYSTEM_INVALID_HANDLE )
				{
					buf.WriteOneBit( 1 ); // file transmission net message stream
					buf.WriteUBitLong( data->transferID, 32 );
					buf.WriteString( data->filename );
				}
				else
				{
					buf.WriteOneBit( 0 ); // normal net message stream
				}

				// data compressed ?
				if ( data->isCompressed )
				{
					buf.WriteOneBit( 1 );
					buf.WriteUBitLong( data->nUncompressedSize, MAX_FILE_SIZE_BITS );
				}
				else
				{
					buf.WriteOneBit( 0 ); 
				}

				buf.WriteUBitLong( data->bytes, MAX_FILE_SIZE_BITS ); // 4MB max for files
			}
		}

		// write fragments to buffer
		if ( data->buffer )
		{
			Assert( data->file == FILESYSTEM_INVALID_HANDLE );
			// send from memory block
			buf.WriteBytes( data->buffer+offset, length );
		}
		else // if ( data->file != FILESYSTEM_INVALID_HANDLE )
		{
			// send from file
			Assert( data->file != FILESYSTEM_INVALID_HANDLE );
			char * tmpbuf = (char*)_alloca( length ); // alloc on stack
			g_pFileSystem->Seek( data->file, offset, FILESYSTEM_SEEK_HEAD );
			g_pFileSystem->Read( tmpbuf, length, data->file );
			buf.WriteBytes( tmpbuf, length );
		}

		if ( net_showfragments.GetBool() )
		{
			ConMsg("Sending subchan %i: start %i, num %i\n", subChan->index, subChan->startFraggment[i], subChan->numFragments[i] );
		}

		subChan->sendSeqNr = m_nOutSequenceNr;
		subChan->state = SUBCHANNEL_WAITING;
	}

	return true;
}


bool CNetChan::ReadSubChannelData( bf_read &buf, int stream  )
{
	dataFragments_t * data = &m_ReceiveList[stream]; // get list
	int startFragment = 0;
	int numFragments = 0;
	unsigned int offset = 0;
	unsigned int length = 0;
	
	bool bSingleBlock = buf.ReadOneBit() == 0; // is single block ?

	if ( !bSingleBlock )
	{
		startFragment = buf.ReadUBitLong( MAX_FILE_SIZE_BITS-FRAGMENT_BITS ); // 16 MB max
		numFragments = buf.ReadUBitLong( 3 );  // 8 fragments per packet max
		offset = startFragment * FRAGMENT_SIZE;
		length = numFragments * FRAGMENT_SIZE;
	}

	if ( offset == 0 ) // first fragment, read header info
	{
		data->filename[0] = 0;
		data->isCompressed = false;
		data->transferID = 0;

		if ( bSingleBlock )
		{
			// data compressed ?
			if ( buf.ReadOneBit() )
			{
				data->isCompressed = true;
				data->nUncompressedSize = buf.ReadUBitLong( MAX_FILE_SIZE_BITS );
			}
			else
			{
				data->isCompressed = false;
			}

			data->bytes = buf.ReadVarInt32();
		}
		else
		{
		
			if ( buf.ReadOneBit() ) // is it a file ?
			{
				data->transferID = buf.ReadUBitLong( 32 );
				buf.ReadString( data->filename, MAX_OSPATH );
			}

			// data compressed ?
			if ( buf.ReadOneBit() )
			{
				data->isCompressed = true;
				data->nUncompressedSize = buf.ReadUBitLong( MAX_FILE_SIZE_BITS );
			}
			else
			{
				data->isCompressed = false;
			}
				
			data->bytes = buf.ReadUBitLong( MAX_FILE_SIZE_BITS );
		}

		if ( data->buffer )
		{
			// last transmission was aborted, free data
			delete [] data->buffer;
			data->buffer = NULL;
			ConDMsg( "Fragment transmission aborted at %i/%i from %s.\n", data->ackedFragments, data->numFragments, GetAddress() );
		}

		data->bits = data->bytes * 8;
		data->asTCP = false;
		data->numFragments = BYTES2FRAGMENTS(data->bytes);
		data->ackedFragments = 0;
		data->file = FILESYSTEM_INVALID_HANDLE;

		if ( bSingleBlock )
		{
			numFragments = data->numFragments;
			length = numFragments * FRAGMENT_SIZE;
		}

		if ( data->bytes > MAX_FILE_SIZE )
		{
			// This can happen with the compressed path above, which uses VarInt32 rather than MAX_FILE_SIZE_BITS
			Warning( "Net message exceeds max size (%u / %u)\n", MAX_FILE_SIZE, data->bytes );
			// Subsequent packets for this transfer will treated as invalid since we never setup a buffer.
			return false;
		}

		data->buffer = new char[PAD_NUMBER(data->bytes, 4)];
	}
	else
	{
		if ( data->buffer == NULL )
		{
			// This can occur if the packet containing the "header" (offset == 0) is dropped.  Since we need the header to arrive we'll just wait
			//  for a retry
			// ConDMsg("Received fragment out of order: %i/%i\n", startFragment, numFragments );
			return false;
		}
	}
	
	if ( (startFragment+numFragments) == data->numFragments )
	{
		// we are receiving the last fragment, adjust length
		int rest = FRAGMENT_SIZE - ( data->bytes % FRAGMENT_SIZE );
		if ( rest < FRAGMENT_SIZE )
			length -= rest;
	}
	else if ( ( startFragment + numFragments ) > data->numFragments )
	{
		// a malicious client can send a fragment beyond what was arranged in fragment#0 header
		// old code will overrun the allocated buffer and likely cause a server crash
		// it could also cause a client memory overrun because the offset can be anywhere from 0 to 16MB range
		// drop the packet and wait for client to retry
		ConDMsg( "Received fragment chunk out of bounds: %i+%i>%i from %s\n", startFragment, numFragments, data->numFragments, GetAddress() );
		return false;
	}

	Assert ( (offset + length) <= data->bytes );
	if ( length == 0 || ( offset + length > data->bytes ) )
	{
		delete[] data->buffer;
		data->buffer = NULL;
		ConMsg("Malformed fragment ofs %i len %d, buffer size %d from %s\n", offset, length, PAD_NUMBER(data->bytes, 4), remote_address.ToString() );
		return false;
	}

	buf.ReadBytes( data->buffer + offset, length ); // read data

	data->ackedFragments+= numFragments;

	if ( net_showfragments.GetBool() )
		ConMsg("Received fragments: start %i, num %i\n", startFragment, numFragments );

	return true;
}

void CNetChan::UpdateSubChannels()
{
	// first check if there is a free subchannel
	subChannel_s * freeSubChan = GetFreeSubChannel();

	if ( freeSubChan == NULL )
		return; //all subchannels in use right now

	int i, nSendMaxFragments = m_MaxReliablePayloadSize / FRAGMENT_SIZE;

	bool bSendData = false;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( m_WaitingList[i].Count() <= 0 )
			continue;

		dataFragments_s *data = m_WaitingList[i][0]; // get head

		if ( data->asTCP )
			continue;

		int nSentFragments = data->ackedFragments + data->pendingFragments;

		Assert( nSentFragments <= data->numFragments );

		if ( nSentFragments == data->numFragments )
			continue; // all fragments already send

		// how many fragments can we send ?

		int numFragments = min( nSendMaxFragments, data->numFragments - nSentFragments );

		// if we are in file background transmission mode, just send one fragment per packet
		if ( i == FRAG_FILE_STREAM && m_bFileBackgroundTranmission )
			numFragments = min( 1, numFragments );

		// copy fragment data into subchannel

		freeSubChan->startFraggment[i] = nSentFragments;
		freeSubChan->numFragments[i] = numFragments;
		
		data->pendingFragments += numFragments;

		bSendData = true;

		nSendMaxFragments -= numFragments;

		if ( nSendMaxFragments <= 0 )
			break;
	}

	if ( bSendData )
	{
		// flip channel bit 
		int bit = 1<<freeSubChan->index;

		FLIPBIT(m_nOutReliableState, bit);

		freeSubChan->state = SUBCHANNEL_TOSEND;
		freeSubChan->sendSeqNr = 0;
	}
}

#if 1

inline unsigned short BufferToShortChecksum( const void *pvData, size_t nLength )
{
	CRC32_t crc = CRC32_ProcessSingleBuffer( pvData, nLength );

	unsigned short lowpart = ( crc & 0xffff );
	unsigned short highpart = ( ( crc >> 16 ) & 0xffff );

	return (unsigned short)( lowpart ^ highpart );
}

#else

// If the CRC version ever is deemed to expensive, here's a quick xor version.
//  It's probably not super robust.
inline unsigned short BufferToShortChecksum( const void *pvData, size_t nSize )
{
	const uint32 *pData = (const uint32 *)pvData;

	unsigned short us = 0;
	while ( nSize >= sizeof( uint32 ) )
	{
		us ^= ( *pData & 0xffff );
		us ^= ( ( *pData >> 16 ) & 0xffff );

		nSize -= sizeof( uint32 );
		pData += sizeof( uint32 );
	}

	const byte *pbData = (const byte *)pData;

	while ( nSize > 0 )
	{
		us ^= *pbData;
		++pbData;
		--nSize;
	}

	return us;
}

#endif

// #define MIN_ROUTABLE_TESTING

#if defined( _DEBUG ) || defined( MIN_ROUTABLE_TESTING )
static ConVar net_minroutable( "net_minroutable", "16", 0, "Forces larger payloads." );
#endif

/*
===============
CNetChan::TransmitBits

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
int CNetChan::SendDatagram(bf_write *datagram)
{
	ALIGN4 byte		send_buf[ NET_MAX_MESSAGE ] ALIGN4_POST;

#ifndef NO_VCR
	if ( vcr_verbose.GetInt() && datagram && datagram->GetNumBytesWritten() > 0 )
		VCRGenericValueVerify( "datagram", datagram->GetBasePointer(), datagram->GetNumBytesWritten()-1 );
#endif
	
	// Make sure for the client that the max routable payload size is up to date
	if ( m_Socket == NS_CLIENT )
	{
		if ( net_maxroutable.GetInt() != GetMaxRoutablePayloadSize() )
		{
			SetMaxRoutablePayloadSize( net_maxroutable.GetInt() );
		}
	}

	// first increase out sequence number
	
	// check, if fake client, then fake send also
	if ( remote_address.GetType() == NA_NULL )	
	{
		// this is a demo channel, fake sending all data
		m_fClearTime = 0.0;		// no bandwidth delay
		m_nChokedPackets = 0;	// Reset choke state
		m_StreamReliable.Reset();		// clear current reliable buffer
		m_StreamUnreliable.Reset();		// clear current unrelaible buffer
		m_nOutSequenceNr++;
		return m_nOutSequenceNr-1;
	}

	// process all new and pending reliable data, return true if reliable data should
	// been send with this packet

	if ( m_StreamReliable.IsOverflowed() )
	{
		ConMsg ("%s:send reliable stream overflow\n" ,remote_address.ToString());
		return 0;
	}
	else if ( m_StreamReliable.GetNumBitsWritten() > 0 )
	{
		CreateFragmentsFromBuffer( &m_StreamReliable, FRAG_NORMAL_STREAM );
		m_StreamReliable.Reset();
	}

	bf_write send( "CNetChan_TransmitBits->send", send_buf, sizeof(send_buf) );

	// Prepare the packet header
	// build packet flags
	unsigned char flags = 0;

	// start writing packet

	send.WriteLong ( m_nOutSequenceNr );
	send.WriteLong ( m_nInSequenceNr );

	bf_write flagsPos = send; // remember flags byte position

	send.WriteByte ( 0 ); // write correct flags value later
	if ( ShouldChecksumPackets() )
	{
		send.WriteShort( 0 );  // write correct checksum later
		Assert( !(send.GetNumBitsWritten() % 8 ) );
	}

	// Note, this only matters on the PC
	int nCheckSumStart = send.GetNumBytesWritten();

	send.WriteByte ( m_nInReliableState );

	if ( m_nChokedPackets > 0 )
	{
		flags |= PACKET_FLAG_CHOKED;
		send.WriteByte ( m_nChokedPackets & 0xFF );	// send number of choked packets
	}

	// always append a challenge number
	flags |= PACKET_FLAG_CHALLENGE ;

	// append the challenge number itself right on the end
	send.WriteLong( m_ChallengeNr );

	if ( SendSubChannelData( send ) )
	{
		flags |= PACKET_FLAG_RELIABLE;
	}

	// Is there room for given datagram data. the datagram data 
	// is somewhat more important than the normal unreliable data
	// this is done to allow some kind of snapshot behavior
	// weather all data in datagram is transmitted or none.
	if ( datagram )
	{
		if( datagram->GetNumBitsWritten() < send.GetNumBitsLeft() )
		{
			send.WriteBits( datagram->GetData(), datagram->GetNumBitsWritten() );
		}
		else
		{
			ConDMsg("CNetChan::SendDatagram:  data would overfow, ignoring\n");
		}
	}

	// Is there room for the unreliable payload?
	if ( m_StreamUnreliable.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(m_StreamUnreliable.GetData(), m_StreamUnreliable.GetNumBitsWritten() );
	}
	else
	{
		ConDMsg("CNetChan::SendDatagram:  Unreliable would overfow, ignoring\n");
	}

	m_StreamUnreliable.Reset();	// clear unreliable data buffer

	// On the PC the voice data is in the main packet
	if ( !IsX360() && 
		m_StreamVoice.GetNumBitsWritten() > 0 && m_StreamVoice.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(m_StreamVoice.GetData(), m_StreamVoice.GetNumBitsWritten() );
		m_StreamVoice.Reset();
	}

	int nMinRoutablePayload = MIN_ROUTABLE_PAYLOAD;

#if defined( _DEBUG ) || defined( MIN_ROUTABLE_TESTING )
	if ( m_Socket == NS_SERVER )
	{
		nMinRoutablePayload = net_minroutable.GetInt();
	}
#endif

	// Deal with packets that are too small for some networks
	while ( send.GetNumBytesWritten() < nMinRoutablePayload )		
	{
		// Go ahead and pad some bits as long as needed
		send.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	// Make sure we have enough bits to read a final net_NOP opcode before compressing 
	int nRemainingBits = send.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		send.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	// if ( IsX360() )
	{
		// Now round up to byte boundary
		nRemainingBits = send.GetNumBitsWritten() % 8;
		if ( nRemainingBits > 0 )
		{
			int nPadBits = 8 - nRemainingBits;

			flags |= ENCODE_PAD_BITS( nPadBits );
	
			// Pad with ones
			if ( nPadBits > 0 )
			{
				unsigned int unOnes = GetBitForBitnum( nPadBits ) - 1;
				send.WriteUBitLong( unOnes, nPadBits );
			}
		}
	}


	// FIXME:  This isn't actually correct since compression might make the main payload usage a bit smaller
	bool bSendVoice = IsX360() && ( m_StreamVoice.GetNumBitsWritten() > 0 &&  m_StreamVoice.GetNumBitsWritten() < send.GetNumBitsLeft() );
		
	bool bCompress = false;
	if ( net_compresspackets.GetBool() )
	{
		if ( send.GetNumBytesWritten() >= net_compresspackets_minsize.GetInt() )
		{
			bCompress = true;
		}
	}

	// write correct flags value and the checksum
	flagsPos.WriteByte( flags ); 

	// Compute checksum (must be aligned to a byte boundary!!)
	if ( ShouldChecksumPackets() )
	{
		const void *pvData = send.GetData() + nCheckSumStart;
		Assert( !(send.GetNumBitsWritten() % 8 ) );
		int nCheckSumBytes = send.GetNumBytesWritten() - nCheckSumStart;
		unsigned short usCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
		flagsPos.WriteUBitLong( usCheckSum, 16 );
	}

	// Send the datagram
	int	bytesSent = NET_SendPacket ( this, m_Socket, remote_address, send.GetData(), send.GetNumBytesWritten(), bSendVoice ? &m_StreamVoice : 0, bCompress );

	if ( bSendVoice || !IsX360() )
	{
		m_StreamVoice.Reset();
	}

	if ( net_showudp.GetInt() && net_showudp.GetInt() != 2 )
	{
		int mask = 63;
		char comp[ 64 ] = { 0 };
		if ( net_compresspackets.GetBool() && 
			bytesSent && 
			( bytesSent < send.GetNumBytesWritten() ) )
		{
			Q_snprintf( comp, sizeof( comp ), " compression=%5u [%5.2f %%]", bytesSent, 100.0f * float( bytesSent ) / float( send.GetNumBytesWritten() ) );
		}
	
		ConMsg ("UDP -> %12.12s: sz=%5i seq=%5i ack=%5i rel=%1i ch=%1i tm=%f rt=%f%s\n"
			, GetName()
			, send.GetNumBytesWritten()
			, ( m_nOutSequenceNr ) & mask
			, m_nInSequenceNr & mask
			, (flags & PACKET_FLAG_RELIABLE) ? 1 : 0
			, flags & PACKET_FLAG_CHALLENGE ? 1 : 0
			, (float)net_time
			, (float)Plat_FloatTime()
			, comp );
	}

	// update stats

	int nTotalSize = bytesSent + UDP_HEADER_SIZE;

	FlowNewPacket( FLOW_OUTGOING, m_nOutSequenceNr, m_nInSequenceNr, m_nChokedPackets, 0, nTotalSize );

	FlowUpdate( FLOW_OUTGOING, nTotalSize );
	
	if ( m_fClearTime < net_time )
	{
		m_fClearTime = net_time;
	}

	// calculate net_time when channel will be ready for next packet (throttling)
	// TODO:  This doesn't exactly match size sent when packet is a "split" packet (actual bytes sent is higher, etc.)
	double fAddTime = (float)nTotalSize / (float)m_Rate;

	m_fClearTime += fAddTime;

	if ( net_maxcleartime.GetFloat() > 0.0f )
	{
		double m_flLatestClearTime = net_time + net_maxcleartime.GetFloat();
		if ( m_fClearTime > m_flLatestClearTime )
		{
			m_fClearTime = m_flLatestClearTime;
		}
	}
	
	m_nChokedPackets = 0;
	m_nOutSequenceNr++;

	return m_nOutSequenceNr-1; // return send seq nr
}

bool CNetChan::ProcessControlMessage( int cmd, bf_read &buf)
{
	char string[1024];

	if ( cmd == net_NOP )
	{
		return true;
	}
	
	if ( cmd == net_Disconnect )
	{
		buf.ReadString( string, sizeof(string) );
		m_MessageHandler->ConnectionClosing( string );
		return false;
	}
	
	if ( cmd == net_File )
	{
		unsigned int transferID = buf.ReadUBitLong( 32 );

		buf.ReadString( string, sizeof(string) );
		if ( buf.ReadOneBit() != 0 && IsValidFileForTransfer( string ) )
		{
			m_MessageHandler->FileRequested( string, transferID );
		}
		else
		{
			m_MessageHandler->FileDenied( string, transferID );
		}
		return true;
	}
	
	ConMsg( "Netchannel: received bad control cmd %i from %s.\n", cmd, remote_address.ToString() );
	return false;
	
}

bool CNetChan::ProcessMessages( bf_read &buf  )
{
	VPROF( "CNetChan::ProcessMessages" );

	const char * showmsgname = net_showmsg.GetString();
	const char * blockmsgname = net_blockmsg.GetString();

	if ( !Q_strcmp(showmsgname, "0") )
	{
		showmsgname = NULL;	// dont do strcmp all the time
	}

	if ( !Q_strcmp(blockmsgname, "0") )
	{
		blockmsgname = NULL;	// dont do strcmp all the time
	}

	if ( net_showpeaks.GetInt() > 0 && net_showpeaks.GetInt() < buf.GetNumBytesLeft()	 )
	{
		showmsgname = "1";	// show messages for this packet only
	}

	bf_read democopy = buf; // create a copy of reading buffer state for demo recording
	
	int startbit = buf.GetNumBitsRead();

	while ( true )
	{
		if ( buf.IsOverflowed() )
		{
			m_MessageHandler->ConnectionCrashed( "Buffer overflow in net message" );
			return false;
		}

		// Are we at the end?
		if ( buf.GetNumBitsLeft() < NETMSG_TYPE_BITS )
		{
			break;
		}

		unsigned char cmd = buf.ReadUBitLong( NETMSG_TYPE_BITS );

		if ( cmd <= net_File )
		{
			if ( !ProcessControlMessage( cmd, buf ) )
			{
				return false; // disconnect or error
			}

			continue;
		}

		// see if we have a registered message object for this type
		INetMessage	* netmsg = FindMessage( cmd );
		
		if ( netmsg )
		{
			// let message parse itself from buffe
			const char *msgname = netmsg->GetName();
			
			int nMsgStartBit = buf.GetNumBitsRead();

			if ( !netmsg->ReadFromBuffer( buf ) )
			{
				ConMsg( "Netchannel: failed reading message %s from %s.\n", msgname, remote_address.ToString() );
				Assert ( 0 );
				return false;
			}

			UpdateMessageStats( netmsg->GetGroup(), buf.GetNumBitsRead() - nMsgStartBit );

			if ( showmsgname )
			{
				if ( (*showmsgname == '1') || !Q_stricmp(showmsgname, netmsg->GetName() ) )
				{
					ConMsg("Msg from %s: %s\n", remote_address.ToString(), netmsg->ToString() );
				}
			}

			if ( blockmsgname )
			{
				if ( (*blockmsgname== '1') || !Q_stricmp(blockmsgname, netmsg->GetName() ) )
				{
					ConMsg("Blocking message %s\n", netmsg->ToString() );
					continue;
				}
			}
			
			// netmessage calls the Process function that was registered by it's MessageHandler
			m_bProcessingMessages = true;
			bool bRet = netmsg->Process();
			m_bProcessingMessages = false;

			// This means we were deleted during the processing of that message.
			if ( m_bShouldDelete )
			{
				delete this;
				return false;
			}

			if ( m_bClearedDuringProcessing )
			{
				// Clear() was called during processing, our buffer is no longer valid
				m_bClearedDuringProcessing = false;
				return false;
			}

			if ( !bRet )
			{
				ConDMsg( "Netchannel: failed processing message %s.\n", msgname );
				Assert ( 0 );
				return false;
			}


			if ( IsOverflowed() )
				return false;
		}
		else
		{
			ConMsg( "Netchannel: unknown net message (%i) from %s.\n", cmd, remote_address.ToString() );
			Assert ( 0 );
			return false;
		}
	}

#if !defined(SWDS) && !defined(_XBOX)
	// all messages could be parsed, write packet to demo file
	if ( m_DemoRecorder && !demoplayer->IsPlayingBack() )
	{
		// only record if any message was paresd
		m_DemoRecorder->RecordMessages( democopy, buf.GetNumBitsRead() - startbit );
	}
#endif

	return true; // ok fine
}

void CNetChan::ProcessPlayback( void )
{
#if !defined(SWDS) && !defined(_XBOX)
	netpacket_t * packet;

	while ( ( packet = demoplayer->ReadPacket() ) != NULL )
	{
		// Update data flow stats
		FlowNewPacket( FLOW_INCOMING, m_nInSequenceNr, m_nOutSequenceNrAck, 0, 0, packet->wiresize );

		last_received = net_time;

		m_MessageHandler->PacketStart( m_nInSequenceNr, m_nOutSequenceNrAck );
		
		if ( ProcessMessages( packet->message ) )
		{
			m_MessageHandler->PacketEnd();
		}
		else
		{
			break;
		}
	}
#endif
}

CNetChan::subChannel_s *CNetChan::GetFreeSubChannel()
{
	for ( int i=0; i<MAX_SUBCHANNELS; i++ )
	{
		if ( m_SubChannels[i].state == SUBCHANNEL_FREE )
			return &m_SubChannels[i];
	}

	return NULL;
}

void CNetChan::CheckWaitingList(int nList)
{
	// go thru waiting lists and mark fragments send with this seqnr packet
	if ( m_WaitingList[nList].Count() == 0 || m_nOutSequenceNrAck <= 0 )
		return; // no data in list

	dataFragments_t *data = m_WaitingList[nList][0]; // get head

	if ( data->ackedFragments == data->numFragments )
	{
		// all fragmenst were send successfully
		if ( net_showfragments.GetBool() )
			ConMsg("Sending complete: %i fragments, %i bytes.\n", data->numFragments, data->bytes );

		RemoveHeadInWaitingList( nList );

		return;
	}
	else if ( data->ackedFragments > data->numFragments )
	{
		//ConMsg("CheckWaitingList: invalid acknowledge fragments %i/%i.\n", data->ackedFragments, data->numFragments );
	}
	// else: still pending fragments
}

#ifdef STAGING_ONLY

CON_COMMAND( netchan_test_upload, "[filename]: Uploads a file to server." )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "Usage: netchan_test_upload [filename]\n" );
		return;
	}

	//$ TODO: the con command system is truncating the filenames we're passing in. Need to workaround this...
	const char *filename = args.GetCommandString() + V_strlen( "netchan_test_upload " );

	Msg( "Sending '%s'\n", filename );
	bool bRet = CNetChan::TestUpload( filename );
	Msg( "%s returned %d\n", __FUNCTION__, bRet );
}

bool CNetChan::TestUpload( const char *filename )
{
	dataFragments_t data;
	static char s_buf[] = "The quick brown\nfox\n";

	data.file = FILESYSTEM_INVALID_HANDLE;		// open file handle
	V_strcpy_safe( data.filename, filename );	// filename
	data.buffer = s_buf;						// if NULL it's a file
	data.bytes = sizeof( s_buf ) - 1;			// size in bytes
	data.bits = data.bytes * 8;					// size in bits
	data.transferID = 123;						// only for files
	data.isCompressed = false;					// true if data is bzip compressed
	data.nUncompressedSize = data.bytes; 		// full size in bytes
	data.asTCP = 0;								// send as TCP stream
	data.numFragments = 0;						// number of total fragments
	data.ackedFragments = 0; 					// number of fragments send & acknowledged
	data.pendingFragments = 0; 					// number of fragments send, but not acknowledged yet

	return HandleUpload( &data, NULL );
}

#endif // STAGING_ONLY

bool CNetChan::HandleUpload( dataFragments_t *data, INetChannelHandler *MessageHandler )
{
	const char *szErrorStr = NULL;
	static ConVar *s_pAllowUpload = g_pCVar->FindVar( "sv_allowupload" );

	if ( !s_pAllowUpload || !s_pAllowUpload->GetBool() )
	{
		szErrorStr = "ignored. File uploads are disabled!";
	}
	else
	{
		// Make sure that this file is not being written to a location above the current directory, isn't in 
		// writing to any locations we don't want, isn't an unsupported 
		if ( !CNetChan::IsValidFileForTransfer( data->filename ) )
		{
			szErrorStr = "has invalid path or extension!";
		}
		else
		{
			// There's a special write path for this stuff
			const char *pszPathID = "download";

			// we received a file, write it to disk and notify host
			if ( g_pFileSystem->FileExists( data->filename, pszPathID ) )
			{
				szErrorStr = "already exists!";
			}
			else
			{
				// Make sure path exists
				char szParentDir[ MAX_PATH ];
				if ( !V_ExtractFilePath( data->filename, szParentDir, sizeof( szParentDir ) ) )
					szParentDir[0] = '\0';

				g_pFileSystem->CreateDirHierarchy( szParentDir, pszPathID );

				// Open new file for write binary.
				data->file = g_pFileSystem->Open( data->filename, "wb", pszPathID );

				if ( FILESYSTEM_INVALID_HANDLE == data->file )
				{
					szErrorStr = "failed to write!";
				}
				else
				{
					g_pFileSystem->Write( data->buffer, data->bytes, data->file );
					g_pFileSystem->Close( data->file );

					if ( net_showfragments.GetInt() == 2 )
					{
						DevMsg( "FileReceived: %s, %i bytes (ID %i)\n", data->filename, data->bytes, data->transferID );
					}

					if ( MessageHandler )
					{
						MessageHandler->FileReceived( data->filename, data->transferID );
					}
				}
			}
		}

	}

	if ( szErrorStr )
	{
		ConMsg( "Download file '%s' %s\n", data->filename, szErrorStr );
	}

	return true;
}

bool CNetChan::CheckReceivingList(int nList)
{
	dataFragments_t * data = &m_ReceiveList[nList]; // get list
	
	if ( data->buffer == NULL )
		return true;

	if ( data->ackedFragments < data->numFragments )
		return true;

	if ( data->ackedFragments > data->numFragments )
	{
		ConMsg( "Receiving failed: too many fragments %i/%i from %s\n", data->ackedFragments, data->numFragments, GetAddress() );
		return false;
	}

	// Got all fragments.

	if ( net_showfragments.GetBool() )
		ConMsg("Receiving complete: %i fragments, %i bytes\n", data->numFragments, data->bytes );

	if ( data->isCompressed )
	{
		UncompressFragments( data );
	}

	if ( !data->filename[0] )
	{
		bf_read buffer( data->buffer, data->bytes );

		if ( !ProcessMessages( buffer ) ) // parse net message
		{
			return false; // stop reading any further
		}
	}
	else
	{
		HandleUpload( data, m_MessageHandler );
	}

	// clear receiveList
	if ( data->buffer )
	{
		delete [] data->buffer; 
		data->buffer = NULL;
	}

	return true;

}

int CNetChan::ProcessPacketHeader( netpacket_t * packet )
{
	// get sequence numbers		
	int sequence	= packet->message.ReadLong();
	int sequence_ack= packet->message.ReadLong();
	int flags		= packet->message.ReadByte();

	if ( ShouldChecksumPackets() )
	{
		unsigned short usCheckSum = (unsigned short)packet->message.ReadUBitLong( 16 );

		// Checksum applies to rest of packet
		Assert( !( packet->message.GetNumBitsRead() % 8 ) );
		int nOffset = packet->message.GetNumBitsRead() >> 3;
		int nCheckSumBytes = packet->message.TotalBytesAvailable() - nOffset;
	
		const void *pvData = packet->message.GetBasePointer() + nOffset;
		unsigned short usDataCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
	
		if ( usDataCheckSum != usCheckSum )
		{
			ConMsg ("%s:corrupted packet %i at %i\n"
				, remote_address.ToString ()
				, sequence
				, m_nInSequenceNr);
			return -1;
		}
	}

	int relState	= packet->message.ReadByte();	// reliable state of 8 subchannels
	int nChoked		= 0;	// read later if choked flag is set
	int i,j;

	if ( flags & PACKET_FLAG_CHOKED )
		nChoked = packet->message.ReadByte(); 

	if ( flags & PACKET_FLAG_CHALLENGE )
	{
		unsigned int nChallenge = packet->message.ReadLong();
		if ( nChallenge != m_ChallengeNr )
			return -1;
		// challenge was good, latch we saw a good one
		m_bStreamContainsChallenge = true;
	}
	else if ( m_bStreamContainsChallenge )
		return -1; // what, no challenge in this packet but we got them before?

	// discard stale or duplicated packets
	if (sequence <= m_nInSequenceNr )
	{
		if ( net_showdrop.GetInt() )
		{
			if ( sequence == m_nInSequenceNr )
			{
				ConMsg ("%s:duplicate packet %i at %i\n"
					, remote_address.ToString ()
					, sequence
					, m_nInSequenceNr);
			}
			else
			{
				ConMsg ("%s:out of order packet %i at %i\n"
					, remote_address.ToString ()
					, sequence
					, m_nInSequenceNr);
			}
		}
		
		return -1;
	}

//
// dropped packets don't keep the message from being used
//
	m_PacketDrop = sequence - (m_nInSequenceNr + nChoked + 1);

	if ( m_PacketDrop > 0 )
	{
		if ( net_showdrop.GetInt() )
		{
			ConMsg ("%s:Dropped %i packets at %i\n"
			,remote_address.ToString(), m_PacketDrop, sequence );
		}
	}

	if ( net_maxpacketdrop.GetInt() > 0 && m_PacketDrop > net_maxpacketdrop.GetInt() )
	{
		if ( net_showdrop.GetInt() )
		{
			ConMsg ("%s:Too many dropped packets (%i) at %i\n"
				,remote_address.ToString(), m_PacketDrop, sequence );
		}
		return -1;
	}


	for ( i = 0; i<MAX_SUBCHANNELS; i++ )
	{
		int bitmask = (1<<i);

		// data of channel i has been acknowledged
		subChannel_s * subchan = &m_SubChannels[i];

		Assert( subchan->index == i);

		if ( (m_nOutReliableState & bitmask) == (relState & bitmask) )
		{
			if ( subchan->state == SUBCHANNEL_DIRTY )
			{
				// subchannel was marked dirty during changelevel, waiting list is already cleared
				subchan->Free();
			}
			else if ( subchan->sendSeqNr > sequence_ack )
			{
				ConMsg ("%s:reliable state invalid (%i).\n"	,remote_address.ToString(), i );
				Assert( 0 );
				return -1;
			}
			else if ( subchan->state == SUBCHANNEL_WAITING )
			{
				for ( j=0; j<MAX_STREAMS; j++ )
				{
					if ( subchan->numFragments[j] == 0 )
						continue;

					Assert( m_WaitingList[j].Count() > 0 );
					
					dataFragments_t * data = m_WaitingList[j][0];

					// tell waiting list, that we received the acknowledge
					data->ackedFragments += subchan->numFragments[j]; 
					data->pendingFragments -= subchan->numFragments[j];
				}

				subchan->Free(); // mark subchannel as free again
			}
		}
		else // subchannel doesn't match
		{
			if ( subchan->sendSeqNr <= sequence_ack )
			{
				Assert( subchan->state != SUBCHANNEL_FREE );

				if ( subchan->state == SUBCHANNEL_WAITING )
				{
					if ( net_showfragments.GetBool() )
					{	
						ConMsg("Resending subchan %i: start %i, num %i\n", subchan->index, subchan->startFraggment[0], subchan->numFragments[0] );
					}

					subchan->state = SUBCHANNEL_TOSEND; // schedule for resend
				}
				else if ( subchan->state == SUBCHANNEL_DIRTY )
				{
					// remote host lost dirty channel data, flip bit back
					int bit = 1<<subchan->index; // flip bit back since data was send yet
			
					FLIPBIT(m_nOutReliableState, bit);

					subchan->Free(); 
				}
			}
		}
	}

	m_nInSequenceNr = sequence;
	m_nOutSequenceNrAck = sequence_ack;
	ETWReadPacket( packet->from.ToString(), packet->wiresize, m_nInSequenceNr, m_nOutSequenceNr );

// Update waiting list status
	
	for( i=0; i<MAX_STREAMS;i++)
		CheckWaitingList( i ); 

// Update data flow stats (use wiresize (compressed))
	FlowNewPacket( FLOW_INCOMING, m_nInSequenceNr, m_nOutSequenceNrAck, nChoked, m_PacketDrop, packet->wiresize + UDP_HEADER_SIZE );

	return flags;
}

/*
=================
CNetChan::ProcessPacket

called when a new packet has arrived for this netchannel
sequence numbers are extracted, fragments/file streams stripped 
and then the netmessages processed
=================
*/
void CNetChan::ProcessPacket( netpacket_t * packet, bool bHasHeader )
{
	VPROF( "CNetChan::ProcessPacket" );

	Assert( packet );

	bf_read &msg = packet->message;	// handy shortcut

	if ( remote_address.IsValid() && !packet->from.CompareAdr ( remote_address ) )
	{
		return;
	}

	// Update data flow stats
	FlowUpdate( FLOW_INCOMING, packet->wiresize + UDP_HEADER_SIZE );

	int flags = 0;

	if ( bHasHeader	)
	{
		flags = ProcessPacketHeader( packet );
	}

	if ( flags == -1 )
		return; // invalid header/packet

	if ( net_showudp.GetInt() && net_showudp.GetInt() != 3 )
	{
		ConMsg ("UDP <- %s: sz=%i seq=%i ack=%i rel=%i ch=%d, tm=%f rt=%f wire=%i\n"
			, GetName()
			, packet->size
			, m_nInSequenceNr & 63
			, m_nOutSequenceNrAck & 63 
			, flags & PACKET_FLAG_RELIABLE ? 1 : 0
			, flags & PACKET_FLAG_CHALLENGE ? 1 : 0
			, net_time
			, (float)Plat_FloatTime()
			, packet->wiresize );
	}
	
	last_received = net_time;

// tell message handler that a new packet has arrived
	m_MessageHandler->PacketStart( m_nInSequenceNr, m_nOutSequenceNrAck );

	if ( flags & PACKET_FLAG_RELIABLE )
	{
		int i, bit = 1<<msg.ReadUBitLong( 3 );

		for ( i=0; i<MAX_STREAMS; i++ )
		{
			if ( msg.ReadOneBit() != 0 )
			{
				if ( !ReadSubChannelData( msg, i ) )
					return; // error while reading fragments, drop whole packet
			}
		}

		// flip subChannel bit to signal successful receiving
		FLIPBIT(m_nInReliableState, bit);
		
		for ( i=0; i<MAX_STREAMS; i++ )
		{
			if ( !CheckReceivingList( i ) )
				return; // error while processing 
		}
	}

// Is there anything left to process?
	if ( msg.GetNumBitsLeft() > 0 )
	{
		// parse and handle all messeges 
		if ( !ProcessMessages( msg ) )
		{
			return;	// disconnect or error
		}
	}
	
// tell message handler that packet is completely parsed
	m_MessageHandler->PacketEnd();

#if !defined(SWDS) && !defined(_XBOX)
// tell demo system that packet is completely parsed
	if ( m_DemoRecorder && !demoplayer->IsPlayingBack() )
	{
		m_DemoRecorder->RecordPacket();
	}
#endif
}

int CNetChan::GetNumBitsWritten( bool bReliable )
{
	bf_write *pStream = &m_StreamUnreliable;
	if ( bReliable )
	{
		pStream = &m_StreamReliable;
	}
	return pStream->GetNumBitsWritten();
}

bool CNetChan::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	if ( remote_address.GetType() == NA_NULL )
		return true;

	bf_write *pStream = &m_StreamUnreliable;
	
	if ( msg.IsReliable() || bForceReliable )
	{
		pStream = &m_StreamReliable;
	}

	if ( bVoice )
	{
		pStream = &m_StreamVoice;
	}

	if ( vcr_verbose.GetInt() )
	{
		bool bRet = false;
#ifndef NO_VCR
		int nOldBytes = pStream->GetNumBytesWritten();
		bRet = msg.WriteToBuffer( *pStream );
		int nNewBytes = pStream->GetNumBytesWritten();

		if ( nNewBytes > nOldBytes )
		{
			VCRGenericValueVerify( "NetMsg", &pStream->GetBasePointer()[nOldBytes], nNewBytes-nOldBytes-1 );
		}
#endif
		return bRet;
	}
	else
	{
		return msg.WriteToBuffer( *pStream );
	}
}

INetMessage *CNetChan::FindMessage(int type)
{
	int numtypes = m_NetMessages.Count();

	for (int i=0; i<numtypes; i++ )
	{
		if ( m_NetMessages[i]->GetType() == type )
			return m_NetMessages[i];
	}

	return NULL;
}

bool CNetChan::RegisterMessage(INetMessage *msg)
{
	Assert( msg );

	if ( FindMessage( msg->GetType() ) )
	{
		return false;
	}

	m_NetMessages.AddToTail( msg );
	msg->SetNetChannel( this );

	return true;
}

bool CNetChan::SendData( bf_write &msg, bool bReliable )
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	if ( remote_address.GetType() == NA_NULL )
		return true;

	if ( msg.GetNumBitsWritten() <= 0 )
		return true;

	if ( msg.IsOverflowed() && !bReliable )
		return true;

	bf_write * buf = bReliable ? &m_StreamReliable : &m_StreamUnreliable;


	if ( msg.GetNumBitsWritten() > buf->GetNumBitsLeft() )
	{
		if (  bReliable )
		{
			ConMsg( "ERROR! SendData reliabe data too big (%i)", msg.GetNumBytesWritten() );
		}

		return false;
	}

	return buf->WriteBits( msg.GetData(), msg.GetNumBitsWritten() );
}

bool CNetChan::SendReliableViaStream( dataFragments_t *data)
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	ALIGN4 char		headerBuf[32] ALIGN4_POST;
	bf_write	header( "outDataHeader", headerBuf, sizeof(headerBuf) );

	
	data->transferID = m_nOutSequenceNr; // used for acknowledging
	data->pendingFragments = data->numFragments; // send, but not ACKed yet

	header.WriteByte( STREAM_CMD_DATA );
	header.WriteWord( data->bytes );			// bytes
	header.WriteLong( data->transferID );	

	if ( net_showtcp.GetInt() )
	{
		ConMsg ("TCP -> %s: sz=%i seq=%i\n", remote_address.ToString(), data->bytes, m_nOutSequenceNr );
	}
	
	NET_SendStream( m_StreamSocket, (char*)header.GetData(), header.GetNumBytesWritten(), 0	);

	return NET_SendStream( m_StreamSocket, data->buffer, data->bytes, 0 ) != -1;
}

bool CNetChan::SendReliableAcknowledge(int seqnr)
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	ALIGN4 char		headerBuf[32] ALIGN4_POST;
	bf_write	header( "outAcknHeader", headerBuf, sizeof(headerBuf) );

	header.WriteByte( STREAM_CMD_ACKN );
	header.WriteLong( seqnr );	// used for acknowledging

	if ( net_showtcp.GetInt() )
	{
		ConMsg ("TCP -> %s: ACKN seq=%i\n", remote_address.ToString(), seqnr );
	}

	return NET_SendStream( m_StreamSocket, (char*)header.GetData(), header.GetNumBytesWritten(), 0 ) > 0;
}

bool CNetChan::ProcessStream( void )
{
	char		cmd;
	ALIGN4 char	headerBuf[512] ALIGN4_POST;
	
	if ( !m_StreamSocket )
		return true;

	if ( m_SteamType == STREAM_CMD_NONE )
	{
		// read command byte
		int ret = NET_ReceiveStream( m_StreamSocket, &cmd, 1, 0 );

		if ( ret == 0)
		{
			// nothing received, but ok
			return true;
		}
		else if ( ret == -1 )
		{
			// something failed with the TCP connection
			return false;
		}
		
		ResetStreaming();	// clear all state values

		m_SteamType = cmd;

	}

	bf_read		header( "inDataHeader", headerBuf, sizeof(headerBuf) );

	// now check command type

	if ( m_SteamType==STREAM_CMD_AUTH )
	{
		// server accpeted connection, send challenge nr
		m_StreamActive = true;
		
		ResetStreaming();

		return SendReliableAcknowledge( m_ChallengeNr );
	}

	if ( (m_SteamType==STREAM_CMD_DATA) && (m_StreamLength==0) )
	{
		int ret = NET_ReceiveStream( m_StreamSocket, (char*)&headerBuf, 6, 0 ) ;
		
		if ( ret == 0)
		{
			// nothing received, but ok
			return true;
		}
		else if ( ret == -1 )
		{
			// something failed with the TCP connection
			return false;
		}

		m_StreamLength = header.ReadWord();
		m_StreamSeqNr = header.ReadLong();

		const int cMaxPayload = GetProtocolVersion() > PROTOCOL_VERSION_23 ? NET_MAX_PAYLOAD : NET_MAX_PAYLOAD_V23;
		if ( m_StreamLength	> cMaxPayload )
		{
			ConMsg( "ERROR! Stream indata too big (%i)", m_StreamLength );
			return false;
		}
	}

	if ( (m_SteamType==STREAM_CMD_FILE) && (m_SteamFile[0]==0) )
	{
		Assert ( 0 );
		return false;
	}

	if ( (m_SteamType==STREAM_CMD_ACKN) && (m_StreamSeqNr==0) )
	{
		int ret = NET_ReceiveStream( m_StreamSocket, (char*)&headerBuf, 4, 0 );
		
		if ( ret == 0)
		{
			// nothing received, but ok
			return true;
		}
		else if ( ret == -1 )
		{
			// something failed with the TCP connection
			return false;
		}

		m_StreamSeqNr = header.ReadLong();

		dataFragments_t * data = m_WaitingList[FRAG_NORMAL_STREAM][0]; 

		if ( data->transferID == (unsigned)m_StreamSeqNr )
		{
			if ( net_showtcp.GetInt() )
			{
				ConMsg ("TCP <- %s: ACKN seqnr=%i\n", remote_address.ToString(), m_StreamSeqNr );
			}

			Assert( data->pendingFragments == data->numFragments );

			RemoveHeadInWaitingList( FRAG_NORMAL_STREAM );
		}
		else
		{
			ConMsg ("TCP <- %s: invalid ACKN seqnr=%i\n", remote_address.ToString(), m_StreamSeqNr );
		}

		ResetStreaming();
		return true;
	}
	

	if ( m_StreamReceived < m_StreamLength )
	{
		// read in 4kB chuncks
		int bytesLeft = ( m_StreamLength - m_StreamReceived );	

		int bytesRecv = NET_ReceiveStream( m_StreamSocket, (char*)m_StreamData.Base() + m_StreamReceived, bytesLeft, 0 );
		
		if ( bytesRecv == 0 )
		{
			return true;
		}
		else if ( bytesRecv == -1 )
		{
			return false;
		}

		m_StreamReceived+= bytesRecv;

		if ( m_StreamReceived > m_StreamLength )
		{
			ConMsg( "ERROR! Stream indata oversize." );
			return false;
		}

		if ( m_StreamReceived == m_StreamLength	)
		{
			int ackseqnr =m_StreamSeqNr;

			bf_read buffer( m_StreamData.Base(), m_StreamLength );

			ProcessMessages( buffer );

			// reset stream state
			ResetStreaming();

			return SendReliableAcknowledge( ackseqnr );	// tell sender that we have it
		}
	}

	return true;
}

int CNetChan::GetDataRate() const
{
	return m_Rate;
}

bool CNetChan::HasPendingReliableData( void )
{
	return (m_StreamReliable.GetNumBitsWritten() > 0) || 
		   (m_WaitingList[FRAG_NORMAL_STREAM].Count() > 0) ||
		   (m_WaitingList[FRAG_FILE_STREAM].Count() > 0);
}

float CNetChan::GetTimeConnected() const
{
	float t = net_time - connect_time;
	return (t>0.0f) ? t : 0.0f ;
}

const netadr_t & CNetChan::GetRemoteAddress() const
{
	return remote_address;
}

INetChannelHandler * CNetChan::GetMsgHandler( void ) const
{
	return  m_MessageHandler;
}

bool CNetChan::IsTimedOut() const
{
	if ( m_Timeout == -1.0f )
		return false;
	else
		return (last_received + m_Timeout) < net_time;
}

bool CNetChan::IsTimingOut() const
{
	if ( m_Timeout == -1.0f )
		return false;
	else
		return (last_received + CONNECTION_PROBLEM_TIME) < net_time;
}

float CNetChan::GetTimeoutSeconds() const
{
	return m_Timeout;
}

float CNetChan::GetTimeSinceLastReceived() const
{
	float t = net_time - last_received;
	return (t>0.0f) ? t : 0.0f ;
}

bool CNetChan::IsOverflowed() const
{
	return m_StreamReliable.IsOverflowed();
}

void CNetChan::Reset()
{
	// FlowReset();
	m_StreamUnreliable.Reset();  // clear any pending unreliable data messages
	m_StreamReliable.Reset();	 // clear any pending reliable data messages
	m_fClearTime = 0.0;			 // ready to send
	m_nChokedPackets = 0;

	m_nSplitPacketSequence = 1;
}

int CNetChan::GetSocket() const
{
	return m_Socket;
}

float CNetChan::GetAvgData( int flow ) const
{
	return m_DataFlow[flow].avgbytespersec;
}

float CNetChan::GetAvgPackets( int flow ) const
{
	return m_DataFlow[flow].avgpacketspersec;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *chan - 
//-----------------------------------------------------------------------------
int CNetChan::GetTotalData(int flow ) const
{
	return m_DataFlow[flow].totalbytes;
}

int	CNetChan::GetSequenceNr( int flow ) const
{
	if ( flow == FLOW_OUTGOING )
	{
		return m_nOutSequenceNr;
	}
	else if ( flow == FLOW_INCOMING )
	{
		return m_nInSequenceNr;
	}
	
	return 0;
}

int	CNetChan::GetBufferSize( void ) const
{
	return NET_FRAMES_BACKUP;
}

bool CNetChan::IsValidPacket( int flow, int frame_number ) const 
{
	return m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ].valid;
}

float CNetChan::GetPacketTime( int flow, int frame_number ) const
{
	return m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ].time;
}

void CNetChan::GetPacketResponseLatency( int flow, int frame_number, int *pnLatencyMsecs, int *pnChoke ) const
{
	const netframe_t &nf = m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ];
	if ( pnLatencyMsecs )
	{
		if ( nf.dropped )
		{
			*pnLatencyMsecs = 9999;
		}
		else 
		{
			*pnLatencyMsecs = (int)( 1000.0f * nf.avg_latency );
		}
	}
	if ( pnChoke )
	{
		*pnChoke = nf.choked;
	}
}

void CNetChan::GetRemoteFramerate( float *pflFrameTime, float *pflRemoteFrameTimeStdDeviation ) const
{
	if ( pflFrameTime )
	{
		*pflFrameTime = m_flRemoteFrameTime;
	}
	if ( pflRemoteFrameTimeStdDeviation )
	{
		*pflRemoteFrameTimeStdDeviation = m_flRemoteFrameTimeStdDeviation;
	}
}


float CNetChan::GetLatency( int flow ) const
{
	return m_DataFlow[flow].latency;
}

float CNetChan::GetAvgChoke( int flow ) const
{
	return m_DataFlow[flow].avgchoke;
}

float CNetChan::GetAvgLatency( int flow ) const
{
	return m_DataFlow[flow].avglatency;	
}

float CNetChan::GetAvgLoss( int flow ) const
{
	return m_DataFlow[flow].avgloss;
}

float CNetChan::GetTime( void ) const
{
	return net_time;
}

bool CNetChan::GetStreamProgress( int flow, int *received, int *total ) const
{
	(*total) = 0;
	(*received) = 0;
	
	if ( flow == FLOW_INCOMING )
	{
		for ( int i = 0; i<MAX_STREAMS; i++ )
		{
			if ( m_ReceiveList[i].buffer != NULL )
			{
				(*total) += m_ReceiveList[i].numFragments * FRAGMENT_SIZE;
				(*received) += m_ReceiveList[i].ackedFragments * FRAGMENT_SIZE;
			}
		}

		return ((*total)>0);
	}

	if ( flow == FLOW_OUTGOING )
	{
		for ( int i = 0; i<MAX_STREAMS; i++ )
		{
			if ( m_WaitingList[i].Count() > 0 )
			{
				(*total) += m_WaitingList[i][0]->numFragments * FRAGMENT_SIZE;
				(*received) += m_WaitingList[i][0]->ackedFragments * FRAGMENT_SIZE;
			}
		}

		return ((*total)>0);
	}
	
	return false; // TODO TCP progress
}

float CNetChan::GetCommandInterpolationAmount( int flow, int frame_number ) const
{
	return m_DataFlow[ flow ].frames[ frame_number & NET_FRAMES_MASK ].m_flInterpolationAmount;
}

int	CNetChan::GetPacketBytes( int flow, int frame_number, int group ) const
{
	if ( group >= INetChannelInfo::TOTAL )
	{
		return m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ].size;
	}
	else
	{
		return Bits2Bytes( m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ].msggroups[group] );
	}
}

void CNetChan::UpdateMessageStats( int msggroup, int bits)
{
	netflow_t * pflow = &m_DataFlow[ FLOW_INCOMING ];
	netframe_t *pframe = pflow->currentframe;

	Assert( (msggroup >= INetChannelInfo::GENERIC) && (msggroup < INetChannelInfo::TOTAL) );

	m_MsgStats[msggroup] += bits;

	if ( pframe )
		pframe->msggroups[msggroup] +=bits;
		
}

void CNetChan::IncrementQueuedPackets()
{
	++m_nQueuedPackets;
}

void CNetChan::DecrementQueuedPackets()
{
	--m_nQueuedPackets;
	Assert( m_nQueuedPackets >= 0 );
	if ( m_nQueuedPackets < 0 )
		m_nQueuedPackets = 0;
}

bool CNetChan::HasQueuedPackets() const
{
	if ( g_pQueuedPackedSender->HasQueuedPackets( this ) )
	{
		return true;
	}

	return m_nQueuedPackets > 0;
}

void CNetChan::SetInterpolationAmount( float flInterpolationAmount )
{
	m_flInterpolationAmount = flInterpolationAmount;
}

void CNetChan::SetRemoteFramerate( float flFrameTime, float flFrameTimeStdDeviation )
{
	m_flRemoteFrameTime = flFrameTime;
	m_flRemoteFrameTimeStdDeviation = flFrameTimeStdDeviation;
}

// Max # of payload bytes before we must split/fragment the packet
void CNetChan::SetMaxRoutablePayloadSize( int nSplitSize )
{
	if ( m_nMaxRoutablePayloadSize != nSplitSize )
	{
		DevMsg( "Setting max routable payload size from %d to %d for %s\n",
			m_nMaxRoutablePayloadSize, nSplitSize, GetName() );
	}
	m_nMaxRoutablePayloadSize = nSplitSize;
}

int CNetChan::GetMaxRoutablePayloadSize()
{
	return m_nMaxRoutablePayloadSize;
}

int CNetChan::GetProtocolVersion()
{
	AssertMsg(
		m_nProtocolVersion >= 0 && m_nProtocolVersion <= PROTOCOL_VERSION,
		"This is probably not being initialized somewhere"
	);
	return m_nProtocolVersion;
}

int CNetChan::IncrementSplitPacketSequence()
{
	return ++m_nSplitPacketSequence;
}

bool CNetChan::IsValidFileForTransfer( const char *pszFilename )
{
	if ( !pszFilename || !pszFilename[ 0 ] )
		return false;

	// No absolute paths or weaseling up the tree with ".." allowed.
	if ( !COM_IsValidPath( pszFilename ) || V_IsAbsolutePath( pszFilename ) )
		return false;

	int len = V_strlen( pszFilename );
	if ( len >= MAX_PATH )
		return false;

	char szTemp[ MAX_PATH ];
	V_strcpy_safe( szTemp, pszFilename );

	// Convert so we've got all forward slashes in the path.
	V_FixSlashes( szTemp, '/' );
	V_FixDoubleSlashes( szTemp );
	if ( szTemp[ len - 1 ] == '/' )
		return false;

	int slash_count = 0;
	for ( const char *psz = szTemp; *psz; psz++ )
	{
		if ( *psz == '/' )
			slash_count++;
	}
	// Really no reason to have deeper directory than this?
	if ( slash_count >= 32 )
		return false;

	// Don't allow filenames with unicode whitespace in them.
	if ( Q_RemoveAllEvilCharacters( szTemp ) )
		return false;

	if ( V_stristr( szTemp, "lua/" ) ||
	     V_stristr( szTemp, "gamemodes/" ) ||
	     V_stristr( szTemp, "addons/" ) ||
	     V_stristr( szTemp, "~/" ) ||
	     // V_stristr( szTemp, "//" ) || 		// Don't allow '//'. TODO: Is this check ok?
	     V_stristr( szTemp, "./././" ) ||	// Don't allow folks to make crazy long paths with ././././ stuff.
	     V_stristr( szTemp, "   " ) ||		// Don't allow multiple spaces or tab (was being used for an exploit).
	     V_stristr( szTemp, "\t" ) )
	{
		return false;
	}

	// If .exe or .EXE or these other strings exist _anywhere_ in the filename, reject it.
	if ( V_stristr( szTemp, ".cfg" ) ||
	     V_stristr( szTemp, ".lst" ) ||
	     V_stristr( szTemp, ".exe" ) ||
	     V_stristr( szTemp, ".vbs" ) ||
	     V_stristr( szTemp, ".com" ) ||
	     V_stristr( szTemp, ".bat" ) ||
	     V_stristr( szTemp, ".cmd" ) ||
	     V_stristr( szTemp, ".dll" ) ||
	     V_stristr( szTemp, ".so" ) ||
	     V_stristr( szTemp, ".dylib" ) ||
	     V_stristr( szTemp, ".ini" ) ||
	     V_stristr( szTemp, ".log" ) ||
	     V_stristr( szTemp, ".lua" ) ||
	     V_stristr( szTemp, ".vdf" ) ||
	     V_stristr( szTemp, ".smx" ) ||
	     V_stristr( szTemp, ".gcf" ) ||
	     V_stristr( szTemp, ".lmp" ) ||
	     V_stristr( szTemp, ".sys" ) )
	{
		return false;
	}

	// Search for the first . in the base filename, and bail if not found.
	// We don't want people passing in things like 'cfg/.wp.so'...
	const char *basename = strrchr( szTemp, '/' );
	if ( !basename )
		basename = szTemp;
	const char *extension = strchr( basename, '.' );
	if ( !extension )
		return false;

	// If the extension is not exactly 3 or 4 characters, bail.
	int extension_len = V_strlen( extension );
	if ( ( extension_len != 3 ) &&
	     ( extension_len != 4 ) &&
	     V_stricmp( extension, ".bsp.bz2" ) &&
	     V_stricmp( extension, ".xbox.vtx" ) &&
	     V_stricmp( extension, ".dx80.vtx" ) &&
	     V_stricmp( extension, ".dx90.vtx" ) &&
	     V_stricmp( extension, ".sw.vtx" ) )
	{
		return false;
	}

	// If there are any spaces in the extension, bail. (Windows exploit).
	if ( strchr( extension, ' ' ) )
		return false;

	return true;
}

