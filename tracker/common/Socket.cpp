//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#if !defined( _X360 )
#define FD_SETSIZE 1024
#endif

#include <assert.h>
#include "winlite.h"
#if !defined( _X360 )
#include "winsock.h"
#else
#include "winsockx.h"
#endif
#include "msgbuffer.h"
#include "socket.h"
#include "inetapi.h"
#include "tier0/vcrmode.h"

#include <VGUI/IVGui.h>

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: All socket I/O occurs on a thread
//-----------------------------------------------------------------------------
class CSocketThread
{
public:
	typedef struct threadsocket_s
	{
		struct threadsocket_s	*next;
		CSocket					*socket;
	} threadsocket_t;

	// Construction
							CSocketThread( void );
	virtual 				~CSocketThread( void );

	// Sockets add/remove themselves via their constructor
	virtual void			AddSocketToThread( CSocket *socket );
	virtual void			RemoveSocketFromThread( CSocket *socket );

	// Lock changes to socket list, etc.
	virtual void			Lock( void );
	// Unlock socket list, etc.
	virtual void			Unlock( void );

	// Retrieve handle to shutdown event
	virtual HANDLE			GetShutdownHandle( void );
	// Get head of socket list
	virtual threadsocket_t	*GetSocketList( void );

	// Sample clock for socket thread
	virtual float			GetClock( void );

private:
	// Initialize the clock
	void					InitTimer( void );

private:
	// Critical section used for synchronizing access to socket list
	CRITICAL_SECTION		cs;
	// List of sockets we are listening on
	threadsocket_t			*m_pSocketList;
	// Thread handle
	HANDLE					m_hThread;
	// Thread id
	DWORD					m_nThreadId;
	// Event to set when we want to tell the thread to shut itself down
	HANDLE					m_hShutdown;

	// High performance clock frequency
	double					m_dClockFrequency;
	// Current accumulated time
	double					m_dCurrentTime;
	// How many bits to shift raw 64 bit sample count by
	int						m_nTimeSampleShift;
	// Previous 32 bit sample count
	unsigned int			m_uiPreviousTime;
};

// Singleton handler
static CSocketThread *GetSocketThread()
{
	static CSocketThread g_SocketThread;
	return &g_SocketThread;
}

//-----------------------------------------------------------------------------
// Purpose: Main winsock processing thread
// Input  : threadobject - 
// Output : static DWORD WINAPI
//-----------------------------------------------------------------------------
static DWORD WINAPI SocketThreadFunc( LPVOID threadobject )
{
	// Get pointer to CSocketThread object
	CSocketThread *socketthread = ( CSocketThread * )threadobject;
	assert( socketthread );
	if ( !socketthread )
	{
		return 0;
	}

	// Keep looking for data until shutdown event is triggered
	while ( 1 )
	{
		// List of sockets
		CSocketThread::threadsocket_t *sockets;
		// file descriptor set for sockets
		fd_set		fdset;
		// number of sockets with messages ready
		int			number;
		// number of sockets added to fd_set
		int			count;

		// Check for shutdown event
		if ( WAIT_OBJECT_0 == VCRHook_WaitForSingleObject( socketthread->GetShutdownHandle(), 0 ) )
		{
			break;
		}

		// Clear the set
		FD_ZERO(&fdset);

		// No changes to list right now
		socketthread->Lock();

		// Add all active sockets to the fdset
		count = 0;
		for ( sockets = socketthread->GetSocketList(); sockets; sockets = sockets->next )
		{
			FD_SET( static_cast<u_int>( sockets->socket->GetSocketNumber() ), &fdset );
			count = max( count, sockets->socket->GetSocketNumber() );
		}

		// Done
		socketthread->Unlock();

		if ( count )
		{
			struct timeval tv;
			tv.tv_sec	= 0;
			tv.tv_usec	= 100000; // 100 millisecond == 100000 usec

			// Block for 100000 usec, or until a message is in the queue
			number = select( count + 1, &fdset, NULL, NULL, &tv );
#if !defined( NO_VCR )
			VCRGenericValue( "", &number, sizeof( number ) );
#endif
			if ( number > 0 )
			{
				// Iterate through socket list and see who has data waiting				//
				// No changes to list right now
				socketthread->Lock();

				// Check FD_SET for incoming network messages
				for ( sockets = socketthread->GetSocketList(); sockets; sockets = sockets->next )
				{
					bool bSet = FD_ISSET( sockets->socket->GetSocketNumber(), &fdset );
#if !defined( NO_VCR )
					VCRGenericValue( "", &bSet, sizeof( bSet ) );
#endif
					if ( bSet )
					{
						// keep reading as long as there is data on the socket
						while (sockets->socket->ReceiveData())
						{
						}
					}
				}

				// Done
				socketthread->Unlock();
			}
		}

		// no need to sleep here, much better let it sleep in the select
	}

	ExitThread( 0 );

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
CSocketThread::CSocketThread( void )
{
	InitTimer();

	m_pSocketList = NULL;

	InitializeCriticalSection( &cs );

	m_hShutdown	= CreateEvent( NULL, TRUE, FALSE, NULL );
	assert( m_hShutdown );

	m_hThread = 0;
	m_nThreadId = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSocketThread::~CSocketThread( void )
{
	Lock();
	if ( m_hThread )
	{
		SetEvent( m_hShutdown );
		Sleep( 2 );
		TerminateThread( m_hThread, 0 );
	}
	Unlock();

	// Kill the socket
//!! need to validate this line
//	assert( !m_pSocketList );

	if ( m_hThread )
	{
		CloseHandle( m_hThread );
	}

	CloseHandle( m_hShutdown );

	DeleteCriticalSection( &cs );
}
	
//-----------------------------------------------------------------------------
// Purpose: Initialize socket thread timer
//-----------------------------------------------------------------------------
void CSocketThread::InitTimer( void )
{
	BOOL success;
	LARGE_INTEGER	PerformanceFreq;
	unsigned int	lowpart, highpart;

	// Start clock at zero
	m_dCurrentTime			= 0.0;

	success = QueryPerformanceFrequency( &PerformanceFreq );
	assert( success );

	// get 32 out of the 64 time bits such that we have around
	// 1 microsecond resolution
	lowpart		= (unsigned int)PerformanceFreq.LowPart;
	highpart	= (unsigned int)PerformanceFreq.HighPart;
	
	m_nTimeSampleShift	= 0;

	while ( highpart || ( lowpart > 2000000.0 ) )
	{
		m_nTimeSampleShift++;
		lowpart >>= 1;
		lowpart |= (highpart & 1) << 31;
		highpart >>= 1;
	}
	
	m_dClockFrequency = 1.0 / (double)lowpart;

	// Get initial sample
	unsigned int		temp;
	LARGE_INTEGER		PerformanceCount;
	QueryPerformanceCounter( &PerformanceCount );
	if ( !m_nTimeSampleShift )
	{
		temp = (unsigned int)PerformanceCount.LowPart;
	}
	else
	{
		// Rotate counter to right by m_nTimeSampleShift places
		temp = ((unsigned int)PerformanceCount.LowPart >> m_nTimeSampleShift) |
			   ((unsigned int)PerformanceCount.HighPart << (32 - m_nTimeSampleShift));
	}

	// Set first time stamp
	m_uiPreviousTime = temp;
}

//-----------------------------------------------------------------------------
// Purpose: Thread local timer function
// Output : float
//-----------------------------------------------------------------------------
float CSocketThread::GetClock( void )
{
	LARGE_INTEGER		PerformanceCount;
	unsigned int		temp, t2;
	double				time;

	// Get sample counter
	QueryPerformanceCounter( &PerformanceCount );

	if ( !m_nTimeSampleShift )
	{
		temp = (unsigned int)PerformanceCount.LowPart;
	}
	else
	{
		// Rotate counter to right by m_nTimeSampleShift places
		temp = ((unsigned int)PerformanceCount.LowPart >> m_nTimeSampleShift) |
			   ((unsigned int)PerformanceCount.HighPart << (32 - m_nTimeSampleShift));
	}

	// check for turnover or backward time
	if ( ( temp <= m_uiPreviousTime ) && 
		( ( m_uiPreviousTime - temp ) < 0x10000000) )
	{
		m_uiPreviousTime = temp;	// so we can't get stuck
	}
	else
	{
		// gap in performance clocks
		t2 = temp - m_uiPreviousTime;

		// Convert to time using frequencey of clock
		time = (double)t2 * m_dClockFrequency;

		// Remember old time
		m_uiPreviousTime = temp;

		// Increment clock
		m_dCurrentTime += time;
	}
#if !defined( NO_VCR )
	VCRGenericValue( "", &m_dCurrentTime, sizeof( m_dCurrentTime ) );
#endif
	// Convert to float
    return (float)m_dCurrentTime;
}

//-----------------------------------------------------------------------------
// Purpose: Returns handle of shutdown event
// Output : HANDLE
//-----------------------------------------------------------------------------
HANDLE CSocketThread::GetShutdownHandle( void )
{
	return m_hShutdown;
}

//-----------------------------------------------------------------------------
// Purpose: Returns head of socket list
// Output : CSocketThread::threadsocket_t
//-----------------------------------------------------------------------------
CSocketThread::threadsocket_t *CSocketThread::GetSocketList( void )
{
	return m_pSocketList;
}

int socketCount = 0;

//-----------------------------------------------------------------------------
// Purpose: Locks object and adds socket to thread
//-----------------------------------------------------------------------------
void CSocketThread::AddSocketToThread( CSocket *socket )
{
	// create the thread if it isn't there
	if (!m_hThread)
	{
		m_hThread = VCRHook_CreateThread( NULL, 0, SocketThreadFunc, (void *)this, 0, &m_nThreadId );
		assert( m_hThread );
	}

	socketCount++;

	threadsocket_t *p = new threadsocket_t;
	p->socket = socket;

	Lock();
	p->next = m_pSocketList;
	m_pSocketList = p;
	Unlock();
}

//-----------------------------------------------------------------------------
// Purpose: Locks list and removes specified socket from thread
//-----------------------------------------------------------------------------
void CSocketThread::RemoveSocketFromThread( CSocket *socket )
{
	if (!m_hThread)
		return;

	socketCount--;

	Lock();
	if ( m_pSocketList )
	{
		threadsocket_t *p, *n;
		p = m_pSocketList;
		if ( p->socket == socket )
		{
			m_pSocketList = m_pSocketList->next;
			delete p;
		}
		else
		{
			while ( p->next )
			{
				n = p->next;
				if ( n->socket == socket )
				{
					p->next = n->next;
					delete n;
					break;
				}
				p = n;
			}
		}
	}
	Unlock();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSocketThread::Lock( void )
{
	VCRHook_EnterCriticalSection( &cs );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSocketThread::Unlock( void )
{
	LeaveCriticalSection( &cs );
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a message handler for incoming socket messages
//-----------------------------------------------------------------------------
CMsgHandler::CMsgHandler( HANDLERTYPE type, void *typeinfo /*=NULL*/ )
{
	m_Type	= type;
	m_pNext = NULL;
	
	// Assume no socket
	SetSocket( NULL );

	// Assume no special checking
	m_ByteCode		= 0;
	m_szString[ 0 ] = 0;

	switch ( m_Type )
	{
	default:
	case MSGHANDLER_ALL:
		break;
	case MSGHANDLER_BYTECODE:
		m_ByteCode = *(unsigned char *)typeinfo;
		break;
	case MSGHANDLER_STRING:
		strcpy( m_szString, (char *)typeinfo );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMsgHandler::~CMsgHandler( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Default message handler for received messages
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMsgHandler::Process( netadr_t *from, CMsgBuffer *msg )
{
	// Swallow message by default
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check for special handling
// Input  : *from - 
//			*msg - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMsgHandler::ProcessMessage( netadr_t *from, CMsgBuffer *msg )
{
	bool bret = false;
	unsigned char ch;
	const char *str;

	// Crack bytecode or string code
	switch( m_Type )
	{
	case MSGHANDLER_BYTECODE:
		msg->Push();
		ch = (unsigned char)msg->ReadByte();
		msg->Pop();
		if ( ch == m_ByteCode )
		{
			bret = Process( from, msg );
		}
		break;
	case MSGHANDLER_STRING:
		msg->Push();
		str = msg->ReadString();
		msg->Pop();
		if ( str && str[ 0 ] && !stricmp( m_szString, str ) )
		{
			bret = Process( from, msg );
		}
		break;
	default:
	case MSGHANDLER_ALL:
		bret = Process( from, msg );
		break;
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: Get next in chain of handlers
//-----------------------------------------------------------------------------
CMsgHandler	*CMsgHandler::GetNext( void ) const
{
	return m_pNext;
}

//-----------------------------------------------------------------------------
// Purpose: Set next in handler chain
// Input  : *next - 
//-----------------------------------------------------------------------------
void CMsgHandler::SetNext( CMsgHandler *next )
{
	m_pNext = next;
}

//-----------------------------------------------------------------------------
// Purpose: Get underlying socket object
// Output : CSocket
//-----------------------------------------------------------------------------
CSocket *CMsgHandler::GetSocket( void ) const
{
	return m_pSocket;
}

//-----------------------------------------------------------------------------
// Purpose: Set underlying socket object
// Input  : *socket - 
//-----------------------------------------------------------------------------
void CMsgHandler::SetSocket( CSocket *socket )
{
	m_pSocket = socket;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a non-blocking, broadcast capable, UDP socket.  If port is
//  specified, binds it to listen on that port, otherwise, chooses a random port.
//-----------------------------------------------------------------------------
CSocket::CSocket( const char *socketname, int port /*= -1*/ ) : m_SendBuffer(socketname)
{
	struct sockaddr_in	address;
	unsigned long _true = 1;
	int i = 1;

	m_pSocketName		= socketname;

	m_bValid			= false;
	m_bResolved			= false;
	m_pMessageHandlers	= NULL;
	m_nUserData			= 0;
	m_bBroadcastSend	= false;
	m_iTotalPackets		= 0;
	m_iCurrentPackets	= 0;
	m_iRetries			= 0;

	m_pBufferCS = new CRITICAL_SECTION;
	InitializeCriticalSection((CRITICAL_SECTION *)m_pBufferCS);

	// ensure the socketthread singleton has been created
	GetSocketThread();

	// Set up the socket
	m_Socket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( m_Socket == -1 )
	{
		//int err = WSAGetLastError();
		// WSANOTINITIALISED
		return;
	}

	// Set it to non-blocking
	if ( ioctlsocket ( m_Socket, FIONBIO, &_true ) == -1 )
	{
		closesocket( m_Socket );
		m_Socket = 0;
		return;
	}

	// Allow broadcast packets
	if ( setsockopt( m_Socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i) ) == -1 )
	{
		closesocket( m_Socket );
		m_Socket = 0;
		return;
	}

	// LATER: Support specifying interface name
	//if (!net_interface || !net_interface[0] || !stricmp(net_interface, "localhost"))
	address.sin_addr.s_addr = INADDR_ANY;
	//else
	//	NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);

	if ( port == -1 )
	{
		address.sin_port = 0;
	}
	else
	{
		address.sin_port = htons( (short)port );
	}

	address.sin_family = AF_INET;

	// only bind if we're required to be on a certain port
	if ( address.sin_port > 0)
	{
		// Bind the socket to specified port
		if ( bind( m_Socket, (struct sockaddr *)&address, sizeof(address) ) == -1 )
		{
			closesocket (m_Socket);
			m_Socket = 0;
			return;
		}
	}

	// Mark as valid
	m_bValid = true;

	// Only add valid sockets to thread
	GetSocketThread()->AddSocketToThread( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSocket::~CSocket( void )
{
	DeleteCriticalSection((CRITICAL_SECTION *)m_pBufferCS);
	delete (CRITICAL_SECTION *)m_pBufferCS;

	// Try to remove socket from thread
	GetSocketThread()->RemoveSocketFromThread( this );

	// Ask message handlers to remove selves?
	if ( m_bValid )
	{
		::shutdown(m_Socket, 0x01);
		::shutdown(m_Socket, 0x02);
		closesocket( m_Socket );
		m_Socket = 0;
	}

	// Remove handlers
	CMsgHandler *handler = m_pMessageHandlers;
	while ( handler )
	{
		RemoveMessageHandler( handler );
		delete handler;
		handler = m_pMessageHandlers;
	}
	m_pMessageHandlers = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Add hander to head of chain
// Input  : *handler - 
//-----------------------------------------------------------------------------
void CSocket::AddMessageHandler( CMsgHandler *handler )
{
	handler->SetNext( m_pMessageHandlers );
	m_pMessageHandlers = handler;

	// Set the socket pointer
	handler->SetSocket( this );
}

//-----------------------------------------------------------------------------
// Purpose: Removed indicated handler
// Input  : *handler - 
//-----------------------------------------------------------------------------
void CSocket::RemoveMessageHandler( CMsgHandler *handler )
{
	if ( !handler )
	{
		return;
	}

	CMsgHandler *list = m_pMessageHandlers;
	if ( list == handler )
	{
		m_pMessageHandlers = m_pMessageHandlers->GetNext();
		return;
	}

	while ( list )
	{
		if ( list->GetNext() == handler )
		{
			list->SetNext( handler->GetNext() );
			handler->SetNext( NULL );
			return;
		}
		list = list->GetNext();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send message to specified address
// Input  : *to - 
// Output : int - number of bytes sent
//-----------------------------------------------------------------------------
int CSocket::SendMessage( netadr_t *to, CMsgBuffer *msg /*= NULL*/ )
{
	m_bBroadcastSend = false;
	m_ToAddress = *to;

	if ( !m_bValid )
	{
		return 0;
	}

	if ( !msg )
	{
		msg = GetSendBuffer();
	}

	struct sockaddr	addr;
	net->NetAdrToSockAddr ( to, &addr );

	int bytessent = sendto( m_Socket, (const char *)msg->GetData(), msg->GetCurSize(), 0, &addr, sizeof( addr ) );
	if ( bytessent == msg->GetCurSize() )
	{
		return bytessent;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Send broadcast message on specified port
// Input  : port - 
// Output : int - number of bytes sent
//-----------------------------------------------------------------------------
int CSocket::Broadcast( int port, CMsgBuffer *msg /*= NULL*/ )
{
	m_bBroadcastSend = true;
	memset( &m_ToAddress, 0, sizeof( m_ToAddress ) );

	if ( !m_bValid )
	{
		return 0;
	}

	if ( !msg )
	{
		msg = GetSendBuffer();
	}

	struct sockaddr	addr;
	netadr_t to;

	to.port = (unsigned short)htons( (unsigned short)port );
	to.type = NA_BROADCAST;

	net->NetAdrToSockAddr ( &to, &addr );

	int bytessent = sendto( m_Socket, (const char *)msg->GetData(), msg->GetCurSize(), 0, &addr, sizeof( addr ) );
	if ( bytessent == msg->GetCurSize() )
	{
		return bytessent;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve internal message buffer
// Output : CMsgBuffer
//-----------------------------------------------------------------------------
CMsgBuffer *CSocket::GetSendBuffer( void )
{
	return &m_SendBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame (outside of the socket thread) to allow socket to receive incoming messages
//  and route them as appropriate
//-----------------------------------------------------------------------------
void CSocket::Frame( void )
{
	// No data waiting
	if (!m_MsgBuffers.Size())
		return;

	VCRHook_EnterCriticalSection( (CRITICAL_SECTION *)m_pBufferCS );

	// pass up all the receive buffers
	for (int i = 0; i < m_MsgBuffers.Size(); i++)
	{
		// See if there's a handler for this message
		CMsgHandler *handler = m_pMessageHandlers;
		netadr_t addr = m_MsgBuffers[i].GetNetAddress();
		while ( handler )
		{
			// Swallow message?
			if ( handler->ProcessMessage( &addr, &m_MsgBuffers[i] ) )
				break;

			handler = handler->GetNext();
		}
	}

	// free the buffer list
	m_MsgBuffers.RemoveAll();

	LeaveCriticalSection((CRITICAL_SECTION *)m_pBufferCS);
}

//-----------------------------------------------------------------------------
// Purpose: Is socket set up correctly
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSocket::IsValid( void ) const
{
	return m_bValid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CSocket::GetClock( void )
{
	return GetSocketThread()->GetClock();
}

//-----------------------------------------------------------------------------
// Purpose: Resolves the socket address
// Output : const netadr_t
//-----------------------------------------------------------------------------
const netadr_t *CSocket::GetAddress( void )
{
	assert( m_bValid );

	if ( !m_bResolved )
	{
		m_bResolved = true;
		// Determine resulting socket address
		net->GetSocketAddress( m_Socket, &m_Address );
	}

	return &m_Address;
}

//-----------------------------------------------------------------------------
// Purpose: Let the user store/retrieve a 32 bit value
// Input  : userData - 
//-----------------------------------------------------------------------------
void CSocket::SetUserData( unsigned int userData )
{
	m_nUserData = userData;
}

//-----------------------------------------------------------------------------
// Purpose: Let the user store/retrieve a 32 bit value
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CSocket::GetUserData(void ) const
{
	return m_nUserData;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the underlying socket id number for setting up the fd_set
//-----------------------------------------------------------------------------
int CSocket::GetSocketNumber( void ) const
{
	return m_Socket;
}

//-----------------------------------------------------------------------------
// Purpose: Called once FD_ISSET is detected
//-----------------------------------------------------------------------------
bool CSocket::ReceiveData( void )
{
	// Check for data
	struct sockaddr	from;
	int			fromlen;
	int			bytes;
	unsigned char buffer[ CMsgBuffer::NET_MAXMESSAGE ];

	fromlen = sizeof( from );
	bytes = VCRHook_recvfrom( m_Socket, (char *)buffer, CMsgBuffer::NET_MAXMESSAGE, 0, (struct sockaddr *)&from, &fromlen );

	//int port = ntohs( ((struct sockaddr_in *)&from)->sin_port);

	// Socket error
	if ( bytes == -1 )
	{
		return false;
	}

	// Too much data, ignore it
	if ( bytes >= CMsgBuffer::NET_MAXMESSAGE )
	{
		return false;
	}

	// Packets must have -1 tag
	if ( bytes < 4 )
	{
		return false;
	}

	// Mark the time no matter what since FD_SET said there was data and we should have it now
	float recvTime = GetClock();
	
	if( *(int *)&buffer[0] == -2 ) // its a split packet :)
	{
		int curPacket=0,offset=0;
		SPLITPACKET *pak =reinterpret_cast<SPLITPACKET *>(&buffer[0]);

		if(m_iTotalPackets==0)  // this is the first in the series
		{	
			m_iTotalPackets = (pak->packetID & 0x0f);
			m_iSeqNo = pak->sequenceNumber;
			m_iRetries=0;

			m_iCurrentPackets=1;// packet numbers start at zero, total is the total number (i.e =2 for packet 0,1)
			curPacket= (pak->packetID & 0xf0)>>4;
		} 
		else if (m_iSeqNo == pak->sequenceNumber) 
		{
			m_iCurrentPackets++;
			curPacket= (pak->packetID & 0xf0)>>4;
		}
		else 
		{
			m_iRetries++;
			if(m_iRetries>MAX_RETRIES)  // make sure we give up eventually on fragments
			{
				m_iTotalPackets=0;
			}
			return false; // TODO: add support for multiple fragments at one time?
		}


		if(curPacket==0) 
		{
			offset=4; // strip the "-1" at the front of the first packet
		}

		if(curPacket<MAX_PACKETS)  // just in case...
		{
			m_CurPacket[curPacket].Clear(); // new packet, clear the buffer out
			m_CurPacket[curPacket].WriteBuf(bytes-offset-sizeof(SPLITPACKET),&buffer[offset+sizeof(SPLITPACKET)]);
		}

		if(m_iCurrentPackets==m_iTotalPackets) 
		{

			VCRHook_EnterCriticalSection((CRITICAL_SECTION *)m_pBufferCS);

			// Get from address
			netadr_t addr;
			net->SockAddrToNetAdr( &from, &addr );
			
			// append to the receive buffer
			int idx = m_MsgBuffers.AddToTail();
			CMsgBuffer &msgBuffer = m_MsgBuffers[idx];
			
			msgBuffer.Clear();

			// copy all our fragments together
			for(int i=0;i<m_iTotalPackets;i++)
			{
				// buffer must be big enough for us to use, that is where the data originally came from :)
				m_CurPacket[i].ReadBuf(m_CurPacket[i].GetCurSize(),buffer);
				msgBuffer.WriteBuf(m_CurPacket[i].GetCurSize(),buffer);
			}
			msgBuffer.SetTime(recvTime);
			msgBuffer.SetNetAddress(addr);

			LeaveCriticalSection((CRITICAL_SECTION *)m_pBufferCS);

			m_iTotalPackets = 0;  // we have collected all the fragments for
								  //this packet, we can start on a new one now

		}


	}
	else if ( *(int *)&buffer[0] == -1 )		// Must have 255,255,255,255 oob tag
	{	
		/*
		// Fake packet loss
		if ( rand() % 1000 < 200 )
			return;
		*/

		VCRHook_EnterCriticalSection((CRITICAL_SECTION *)m_pBufferCS);

		// Get from address
		netadr_t addr;
		net->SockAddrToNetAdr( &from, &addr );
		
		// append to the receive buffer
		int idx = m_MsgBuffers.AddToTail();
		CMsgBuffer &msgBuffer = m_MsgBuffers[idx];
		
		// Copy payload minus the -1 tag
		msgBuffer.Clear();
		msgBuffer.WriteBuf( bytes - 4, &buffer[ 4 ] );
		msgBuffer.SetTime(recvTime);
		msgBuffer.SetNetAddress(addr);

		LeaveCriticalSection((CRITICAL_SECTION *)m_pBufferCS);
	} 

	return true;
}
