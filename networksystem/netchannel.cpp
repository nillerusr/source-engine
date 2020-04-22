//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "NetChannel.h"
#include "UDP_Socket.h"
#include "tier1/utlbuffer.h"
#include "networksystem/inetworkmessage.h"
#include "networksystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Construction/Destruction
//-----------------------------------------------------------------------------
CNetChannel::CNetChannel()
{
	m_pSocket = NULL; // invalid
	remote_address.Clear();
	last_received = 0;
	connect_time = 0;
	m_ConnectionState = CONNECTION_STATE_DISCONNECTED;
	Q_strncpy( m_Name, "", sizeof(m_Name) ); 

	m_MessageHandler = NULL;

	m_StreamUnreliable.StartWriting(m_UnreliableDataBuffer, sizeof(m_UnreliableDataBuffer));
	m_StreamUnreliable.SetDebugName( "netchan_t::unreliabledata" );

	m_StreamReliable.StartWriting(m_ReliableDataBuffer, sizeof(m_ReliableDataBuffer));
	m_StreamReliable.SetDebugName( "netchan_t::reliabledata" );

	m_Rate		= DEFAULT_RATE;
	m_Timeout	= SIGNON_TIME_OUT;

	m_PacketDrop = 0;

	// Prevent the first message from getting dropped after connection is set up.

	m_nOutSequenceNr = 1;	// otherwise it looks like a 	
	m_nInSequenceNr = 0;
	m_nOutSequenceNrAck = 0;
	m_nOutReliableState = 0; // our current reliable state
	m_nInReliableState = 0;	// last remote reliable state

	// FlowReset();
}

CNetChannel::~CNetChannel()
{
	Shutdown( "NetChannel removed." );
}


//-----------------------------------------------------------------------------
// called to open a channel to a remote system
//-----------------------------------------------------------------------------
void CNetChannel::Setup( bool serverSide, const netadr_t *adr, CUDPSocket *sendSocket, char const *name, INetworkMessageHandler *handler )
{
	Assert( name ); 
	Assert( handler );
	Assert( adr );

	m_pSocket = sendSocket;

	// remote_address may be NULL for fake channels (demo playback etc)
	remote_address = *adr;
	
	last_received		= g_pNetworkSystemImp->GetTime();
	connect_time		= last_received;
	
	Q_strncpy( m_Name, name, sizeof(m_Name) ); 

	m_MessageHandler = handler;

	m_StreamUnreliable.StartWriting(m_UnreliableDataBuffer, sizeof(m_UnreliableDataBuffer));
	m_StreamUnreliable.SetDebugName( "netchan_t::unreliabledata" );

	m_ReliableDataBufferMP.EnsureCapacity( NET_MAX_PAYLOAD );
	m_StreamReliable.StartWriting( m_ReliableDataBufferMP.Base(), NET_MAX_PAYLOAD );
	m_StreamReliable.SetDebugName( "netchan_t::reliabledata" );

	m_Rate		= DEFAULT_RATE;
	m_Timeout	= SIGNON_TIME_OUT;
	m_PacketDrop = 0;

	// Prevent the first message from getting dropped after connection is set up.

	m_nOutSequenceNr = 1;	// otherwise it looks like a 	
	m_nInSequenceNr = 0;
	m_nOutSequenceNrAck = 0;
	m_nOutReliableState = 0; // our current reliable state
	m_nInReliableState = 0;	// last remote reliable state
	m_nChokedPackets = 0;
	m_fClearTime = 0.0;
	m_ConnectionState = CONNECTION_STATE_CONNECTED;

//	FlowReset();

	// tell message handler to register know netmessages
	m_MessageHandler->OnConnectionStarted( this );
}


void CNetChannel::Shutdown( const char *pReason )
{
	// send discconect
	if ( !m_pSocket )
		return;

	Clear(); // free all buffers (reliable & unreliable)

	if ( pReason )
	{
		// send disconnect message
		WriteSystemNetworkMessage( m_StreamUnreliable, net_disconnect );
		m_StreamUnreliable.WriteString( pReason );
		Transmit();	// push message out
	}

	m_pSocket = NULL; // signals that netchannel isn't valid anymore

	remote_address.Clear();

	m_ConnectionState = CONNECTION_STATE_DISCONNECTED;
	if ( m_MessageHandler )
	{
		m_MessageHandler->OnConnectionClosing( this, pReason );
		m_MessageHandler = NULL;
	}
}


//-----------------------------------------------------------------------------
// Channel connection state
//-----------------------------------------------------------------------------
ConnectionStatus_t CNetChannel::GetConnectionState( )
{
	return m_ConnectionState;
}

void CNetChannel::SetConnectionState( ConnectionStatus_t state )
{
	m_ConnectionState = state;
}


/*
void CNetChannel::GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck )
{
	nOutSequenceNr = m_nOutSequenceNr;
	nInSequenceNr = m_nInSequenceNr;
	nOutSequenceNrAck = m_nOutSequenceNrAck;
}

void CNetChannel::SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck )
{
	Assert( IsPlayback() );

	m_nOutSequenceNr = nOutSequenceNr;
	m_nInSequenceNr = nInSequenceNr;
	m_nOutSequenceNrAck = nOutSequenceNrAck;
}
*/

void CNetChannel::SetTimeout(float seconds)
{
	m_Timeout = seconds;

	if ( m_Timeout > 3600.0f )
	{
		m_Timeout = 3600.0f; // 1 hour maximum
	} 
	else if ( m_Timeout < CONNECTION_PROBLEM_TIME )
	{
		m_Timeout = CONNECTION_PROBLEM_TIME; // allow at least this minimum
	}
}

void CNetChannel::SetDataRate(float rate)
{
	m_Rate = clamp( rate, (float)MIN_RATE, (float)MAX_RATE );
}

const char * CNetChannel::GetName() const
{
	return m_Name;
}

const char * CNetChannel::GetAddress() const
{
	return remote_address.ToString();
}


/*
int CNetChannel::GetDropNumber() const
{
	return m_PacketDrop;
}
*/

/*
===============
CNetChannel::CanSendPacket

Returns true if the bandwidth choke isn't active
================
*/
bool CNetChannel::CanSendPacket () const
{
	return m_fClearTime < g_pNetworkSystemImp->GetTime();
}

/*
void CNetChannel::FlowReset( void )
{
	Q_memset( m_DataFlow, 0, sizeof( m_DataFlow ) );
	Q_memset( m_MsgStats, 0, sizeof( m_MsgStats ) );
}

void CNetChannel::FlowNewPacket(int flow, int seqnr, int acknr, int nChoked, int nSize )
{
	netflow_t * pflow = &m_DataFlow[ flow ];
	
	// if frame_number != ( current + 1 ) mark frames between as invalid

	netframe_t *pframe = NULL;

	if ( seqnr > pflow->currentindex )
	{
		for ( int i = pflow->currentindex+1; i <= seqnr; i++ )
		{
			pframe = &pflow->frames[ i & NET_FRAMES_MASK ];

			pframe->time = GetTime();	// now
			pframe->valid = false;
			pframe->size = 0;
			pframe->latency = -1.0f; // not acknowledged yet
			pframe->choked = 0; // not acknowledged yet
			Q_memset( &pframe->msggroups, 0, sizeof(pframe->msggroups) );
		}

		pframe->choked = nChoked;
		pframe->size = nSize;
		pframe->valid = true;
	}
	else
	{
		Assert( seqnr > pflow->currentindex );
	}

	pflow->totalpackets++;
	pflow->currentindex = seqnr;
	pflow->currentframe = pframe;

	// updated ping for acknowledged packet

	int aflow = (flow==FLOW_OUTGOING) ? FLOW_INCOMING : FLOW_OUTGOING;

	if ( acknr <= (m_DataFlow[aflow].currentindex - NET_FRAMES_BACKUP) )
		return;	// acknowledged packet isn't in backup buffer anymore
	
	netframe_t * aframe = &m_DataFlow[aflow].frames[ acknr  & NET_FRAMES_MASK ];

	if ( aframe->valid && aframe->latency == -1.0f )
	{
		// update ping for acknowledged packet, if not already acknowledged before
		
		aframe->latency = GetTime() - aframe->time;

		if ( aframe->latency < 0.0f )
			aframe->latency = 0.0f;
	}
}

void CNetChannel::FlowUpdate(int flow, int addbytes)
{
	netflow_t * pflow = &m_DataFlow[ flow ];
	pflow->totalbytes += addbytes;

	if ( pflow->nextcompute > GetTime() )
		return;

	pflow->nextcompute = GetTime() + FLOW_INTERVAL;

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
*/

void CNetChannel::SetChoked( void )
{
	m_nOutSequenceNr++;	// sends to be done since move command use sequence number
	m_nChokedPackets++;
}

bool CNetChannel::Transmit( bool onlyReliable /* =false */ )
{
	if ( onlyReliable )
	{
		m_StreamUnreliable.Reset();
	}

	return ( SendDatagram( NULL ) != 0 );
}

/*
===============
CNetChannel::TransmitBits

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
int CNetChannel::SendDatagram( bf_write *datagram )
{
	byte		send_buf[ NET_MAX_MESSAGE ];
		
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
		Msg ("%s:send reliable stream overflow\n" ,remote_address.ToString());
		return 0;
	}

	bf_write send( "CNetChannel_TransmitBits->send", send_buf, sizeof(send_buf) );

	// Prepare the packet header
	// build packet flags
	unsigned char flags = 0;

	// start writing packet
	send.WriteLong ( m_nOutSequenceNr );
	send.WriteLong ( m_nInSequenceNr );

	bf_write flagsPos = send; // remember flags byte position

	send.WriteByte ( 0 ); // write correct flags value later
	send.WriteByte ( m_nInReliableState );

	if ( m_nChokedPackets > 0 )
	{
		flags |= PACKET_FLAG_CHOKED;
		send.WriteByte ( m_nChokedPackets & 0xFF );	// send number of choked packets
	}

	/*
	if ( SendSubChannelData( send ) )
	{
		flags |= PACKET_FLAG_RELIABLE;
	}
	*/

	// Is there room for given datagram data. the datagram data 
	// is somewhat more important than the normal unreliable data
	// this is done to allow some kind of snapshot behaviour
	// weather all data in datagram is transmitted or none.
	if ( datagram )
	{
		if( datagram->GetNumBitsWritten() < send.GetNumBitsLeft() )
		{
			send.WriteBits( datagram->GetData(), datagram->GetNumBitsWritten() );
		}
		else
		{
			DevMsg("CNetChannel::SendDatagram:  data would overfow, ignoring\n");
		}
	}

	// Is there room for the unreliable payload?
	if ( m_StreamUnreliable.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(m_StreamUnreliable.GetData(), m_StreamUnreliable.GetNumBitsWritten() );
	}
	else
	{
		DevMsg("CNetChannel::SendDatagram:  Unreliable would overfow, ignoring\n");
	}

	m_StreamUnreliable.Reset();	// clear unreliable data buffer

	// Deal with packets that are too small for some networks
	while ( send.GetNumBytesWritten() < MIN_ROUTEABLE_PACKET )		
	{
		// Go ahead and pad some bits as long as needed
		WriteSystemNetworkMessage( send, net_nop );
	}

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = send.GetNumBitsWritten() % 8;
	int nHeaderSize = g_pNetworkSystemImp->GetGroupBitCount() + g_pNetworkSystemImp->GetTypeBitCount();
	while ( nRemainingBits > 0 && nRemainingBits <= ( 8 - nHeaderSize ) )
	{
		WriteSystemNetworkMessage( send, net_nop );
		nRemainingBits += nHeaderSize;
	}

	flagsPos.WriteByte( flags ); // write correct flags value

	// Send the datagram
	m_pSocket->SendTo( remote_address, send.GetData(), send.GetNumBytesWritten() );

	// update stats

	int nTotalSize = send.GetNumBytesWritten() + UDP_HEADER_SIZE;

//	FlowNewPacket( FLOW_OUTGOING, m_nOutSequenceNr, m_nInSequenceNr, m_nChokedPackets, nTotalSize );
//	FlowUpdate( FLOW_OUTGOING, nTotalSize );
	
	float flTime = g_pNetworkSystemImp->GetTime();
	if ( m_fClearTime < flTime )
	{
		m_fClearTime = flTime;
	}

	// calc cleantime when channel will be ready for next packet
	Assert( m_Rate != 0.0f );
	m_fClearTime += (float)( nTotalSize ) / (float) m_Rate;
	
	m_nChokedPackets = 0;
	m_nOutSequenceNr++;

	return m_nOutSequenceNr-1; // return send seq nr
}

bool CNetChannel::ProcessControlMessage( int cmd, bf_read &buf )
{
	switch( cmd )
	{
	case net_nop:
		return true;

	case net_disconnect:
		{
			char pReason[1024];
			buf.ReadString( pReason, sizeof(pReason) );
			Shutdown( pReason );
		}
		return false;

	default:
		Msg( "CNetChannel: received bad control cmd %i from %s.\n", cmd, remote_address.ToString() );
		return false;
	}	
}

bool CNetChannel::ProcessMessages( bf_read &buf )
{
	//int startbit = buf.GetNumBitsRead();
	int nGroupCount = g_pNetworkSystemImp->GetGroupBitCount();
	int nTypeCount = g_pNetworkSystemImp->GetTypeBitCount();

	while ( true )
	{
		if ( buf.IsOverflowed() )
			return false;

		// Are we at the end?
		if ( buf.GetNumBitsLeft() < ( nGroupCount + nTypeCount ) )
			break;

		unsigned int group = buf.ReadUBitLong( nGroupCount );
		unsigned int type = buf.ReadUBitLong( nTypeCount );

		if ( group == net_group_networksystem )
		{
			if ( !ProcessControlMessage( type, buf ) )
				return g_pNetworkSystemImp->IsNetworkEventCreated(); // disconnect or error
			continue;
		}

		// see if we have a registered message object for this type
		INetworkMessage	*pNetMessage = g_pNetworkSystemImp->FindNetworkMessage( group, type );
		if ( !pNetMessage )
		{
			Msg( "Netchannel: unknown net message (%i:%i) from %s.\n", group, type, remote_address.ToString() );
			Assert ( 0 );
			return false;
		}

		// Attach it to the correct netchannel
		pNetMessage->SetNetChannel( this );

		// let message parse itself from buffe
		const char *pGroupName = pNetMessage->GetGroupName();
		const char *pMessageName = pNetMessage->GetName();
		
		//int startbit = buf.GetNumBitsRead();

		if ( !pNetMessage->ReadFromBuffer( buf ) )
		{
			Msg( "Netchannel: failed reading message %s [%s] from %s.\n", pMessageName, pGroupName, remote_address.ToString() );
			Assert ( 0 );
			return false;
		}

		// UpdateMessageStats( netmsg->GetGroup(), buf.GetNumBitsRead() - startbit );

		// Create a network event
		NetworkMessageReceivedEvent_t *pReceived = g_pNetworkSystemImp->CreateNetworkEvent< NetworkMessageReceivedEvent_t >( );
		pReceived->m_nType = NETWORK_EVENT_MESSAGE_RECEIVED;
		pReceived->m_pChannel = this;
		pReceived->m_pNetworkMessage = pNetMessage;
		return true; // ok fine
	}

	return false; // ok fine, but don't keep processing this packet
}

int CNetChannel::ProcessPacketHeader( bf_read& message )
{
	// get sequence numbers		
	int sequence	= message.ReadLong();
	int sequence_ack= message.ReadLong();
	int flags		= message.ReadByte();
	int relState	= message.ReadByte();	// reliable state of 8 subchannels
	int nChoked		= 0;	// read later if choked flag is set
	//int i,j;

	NOTE_UNUSED( relState );

	if ( flags & PACKET_FLAG_CHOKED )
		nChoked = message.ReadByte(); 

// discard stale or duplicated packets
	if (sequence <= m_nInSequenceNr )
	{
		/*
		if ( net_showdrop.GetInt() )
		{
			if ( sequence == m_nInSequenceNr )
			{
				Msg ("%s:duplicate packet %i at %i\n"
					, remote_address.ToString ()
					, sequence
					, m_nInSequenceNr);
			}
			else
			{
				Msg ("%s:out of order packet %i at %i\n"
					, remote_address.ToString ()
					, sequence
					, m_nInSequenceNr);
			}
		}
		*/
		
		return -1;
	}

//
// dropped packets don't keep the message from being used
//
	m_PacketDrop = sequence - (m_nInSequenceNr + nChoked + 1);

	if ( m_PacketDrop > 0 )
	{
		/*
		if ( net_showdrop.GetInt() )
		{
			Msg ("%s:Dropped %i packets at %i\n"
			,remote_address.ToString(), m_PacketDrop, sequence );
		}
		*/
	}

	m_nInSequenceNr = sequence;
	m_nOutSequenceNrAck = sequence_ack;

	// Update data flow stats
	// FlowNewPacket( FLOW_INCOMING, m_nInSequenceNr, m_nOutSequenceNrAck, nChoked, packet->size + UDP_HEADER_SIZE );

	return flags;
}


//-----------------------------------------------------------------------------
// CNetChannel::ProcessPacket
//
// called when a new packet has arrived for this netchannel
// sequence numbers are extracted, fragments/file streams stripped 
// and then the netmessages processed
//-----------------------------------------------------------------------------
bool CNetChannel::StartProcessingPacket( CNetPacket *packet )
{
	if ( !m_MessageHandler )
		return false;

	netadr_t from = packet->m_From;
	if ( remote_address.IsValid() && !from.CompareAdr ( remote_address ) )
		return false;

	// Update data flow stats
	//FlowUpdate( FLOW_INCOMING, msg.TellPut() + UDP_HEADER_SIZE );

	int flags = ProcessPacketHeader( packet->m_Message );
	if ( flags == -1 )
		return false; // invalid header/packet

	/*
	if ( net_showudp.GetInt() && net_showudp.GetInt() != 3 )
	{
		Msg ("UDP <- %s: sz=%i seq=%i ack=%i rel=%i tm=%f\n"
			, GetName()
			, packet->m_nSizeInBytes
			, m_nInSequenceNr & 63
			, m_nOutSequenceNrAck & 63 
			, flags & PACKET_FLAG_RELIABLE	 ? 1 : 0
			, GetTime() );
	}
	*/

	last_received = g_pNetworkSystemImp->GetTime();

	// tell message handler that a new packet has arrived
	m_MessageHandler->OnPacketStarted( m_nInSequenceNr, m_nOutSequenceNrAck );

	if ( flags & PACKET_FLAG_RELIABLE )
	{
		/*
		int i, bit = 1<<msg.ReadUBitLong( 3 );

		for ( i=0; i<MAX_STREAMS; i++ )
		{
			if ( msg.ReadOneBit() != 0 )
			{
				if ( !ReadSubChannelData( msg, i ) )
					return false; // error while reading fragments, drop whole packet
			}
		}

		// flip subChannel bit to signal successfull receiving
		FLIPBIT(m_nInReliableState, bit);
		
		for ( i=0; i<MAX_STREAMS; i++ )
		{
			if ( !CheckReceivingList( i ) )
				return false; // error while processing 
		}
		*/
	}

	return true;
}

bool CNetChannel::ProcessPacket( CNetPacket *packet )
{
	return ProcessMessages( packet->m_Message );
}

void CNetChannel::EndProcessingPacket( CNetPacket *packet )
{
	// tell message handler that packet is completely parsed
	if ( m_MessageHandler	)
	{
		m_MessageHandler->OnPacketFinished();
	}
}

bool CNetChannel::AddNetMsg( INetworkMessage *msg, bool bForceReliable )
{
	if ( msg->IsReliable() || bForceReliable )
	{
		WriteNetworkMessage( m_StreamReliable, msg );
		if ( m_StreamReliable.IsOverflowed() )
			return false;
		return msg->WriteToBuffer( m_StreamReliable );
	}

	WriteNetworkMessage( m_StreamUnreliable, msg );
	if ( m_StreamUnreliable.IsOverflowed() )
		return false;
	return msg->WriteToBuffer( m_StreamUnreliable );
}

bool CNetChannel::AddData( bf_write &msg, bool bReliable )
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	if ( msg.GetNumBitsWritten() <= 0 )
		return true;

	bf_write * buf = bReliable ? &m_StreamReliable : &m_StreamUnreliable;


	if ( msg.GetNumBitsWritten() > buf->GetNumBitsLeft() )
	{
		if (  bReliable )
		{
			Msg( "ERROR! SendData reliabe data too big (%i)", msg.GetNumBytesWritten() );
		}

		return false;
	}

	return buf->WriteBits( msg.GetData(), msg.GetNumBitsWritten() );
}

int CNetChannel::GetDataRate() const
{
	return m_Rate;
}

bool CNetChannel::HasPendingReliableData( void )
{
	return ( m_StreamReliable.GetNumBitsWritten() > 0 );
}

float CNetChannel::GetTimeConnected() const
{
	float t = g_pNetworkSystemImp->GetTime() - connect_time;
	return (t>0.0f) ? t : 0.0f ;
}

const netadr_t & CNetChannel::GetRemoteAddress() const
{
	return remote_address;
}

bool CNetChannel::IsTimedOut() const
{
	if ( m_Timeout == -1.0f )
		return false;
	else
		return ( last_received + m_Timeout ) < g_pNetworkSystemImp->GetTime();
}

bool CNetChannel::IsTimingOut() const
{
	if ( m_Timeout == -1.0f )
		return false;
	else
		return (last_received + CONNECTION_PROBLEM_TIME) < g_pNetworkSystemImp->GetTime();
}

float CNetChannel::GetTimeSinceLastReceived() const
{
	float t = g_pNetworkSystemImp->GetTime() - last_received;
	return (t>0.0f) ? t : 0.0f ;
}

bool CNetChannel::IsOverflowed() const
{
	return m_StreamReliable.IsOverflowed();
}

void CNetChannel::Clear()
{
	Reset();
}

void CNetChannel::Reset()
{
	// FlowReset();
	m_StreamUnreliable.Reset();  // clear any pending unreliable data messages
	m_StreamReliable.Reset();	 // clear any pending reliable data messages
	m_fClearTime = 0.0;			 // ready to send
	m_nChokedPackets = 0;
}

CUDPSocket *CNetChannel::GetSocket()
{
	return m_pSocket;
}

float CNetChannel::GetAvgData( int flow ) const
{
	return 0.0f;
//	return m_DataFlow[flow].avgbytespersec;
}

float CNetChannel::GetAvgPackets( int flow ) const
{
	return 0.0f;
//	return m_DataFlow[flow].avgpacketspersec;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *chan - 
//-----------------------------------------------------------------------------
int CNetChannel::GetTotalData(int flow ) const
{
	return 0;
//	return m_DataFlow[flow].totalbytes;
}

/*
int	CNetChannel::GetSequenceNr( int flow ) const
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
*/

float CNetChannel::GetLatency( int flow ) const
{
	return 0.0f;
//	return m_DataFlow[flow].latency;
}

float CNetChannel::GetAvgChoke( int flow ) const
{
	return 0.0f;
	//return m_DataFlow[flow].avgchoke;
}

float CNetChannel::GetAvgLatency( int flow ) const
{
	return 0.0f;
	//return m_DataFlow[flow].avglatency;	
}

float CNetChannel::GetAvgLoss( int flow ) const
{
	return 0.0f;
	//return m_DataFlow[flow].avgloss;
}
