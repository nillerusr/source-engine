//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"

#include "tier1/utlvector.h"
#include "tier1/utlpriorityqueue.h"

#include "tier0/etwprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar net_queued_packet_thread( "net_queued_packet_thread", "1", 0, "Use a high priority thread to send queued packets out instead of sending them each frame." );
ConVar net_queue_trace( "net_queue_trace", "0", 0 );

class CQueuedPacketSender : public CThread, public IQueuedPacketSender
{
public:
	CQueuedPacketSender();
	~CQueuedPacketSender();

	// IQueuedPacketSender

	virtual bool Setup();
	virtual void Shutdown();
	virtual bool IsRunning() { return CThread::IsAlive(); }

	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan );
	virtual void QueuePacket( INetChannel *pChan, SOCKET s, const char FAR *buf, int len, const struct sockaddr FAR * to, int tolen, uint32 msecDelay );
	virtual bool HasQueuedPackets( const INetChannel *pChan ) const;
private:

	// CThread Overrides
	virtual bool Start( unsigned int nBytesStack = 0 );
	virtual int Run();

private:

	class CQueuedPacket
	{
	public:
		uint32				m_unSendTime;
		const void 			*m_pChannel;  // We don't actually use the channel
		SOCKET				m_Socket;
		CUtlVector<char>	to;	// sockaddr
		CUtlVector<char>	buf;

		// We want the list sorted in ascending order, so note that we return > rather than <
		static bool LessFunc( CQueuedPacket * const &lhs, CQueuedPacket * const &rhs )
		{
			return lhs->m_unSendTime > rhs->m_unSendTime;
		}
	};

	CUtlPriorityQueue< CQueuedPacket * > m_QueuedPackets;
	CThreadMutex m_QueuedPacketsCS;
	CThreadEvent m_hThreadEvent;
	volatile bool m_bThreadShouldExit;
};

static CQueuedPacketSender g_QueuedPacketSender;
IQueuedPacketSender *g_pQueuedPackedSender = &g_QueuedPacketSender;


CQueuedPacketSender::CQueuedPacketSender() :
	m_QueuedPackets( 0, 0, CQueuedPacket::LessFunc )
{
	SetName( "QueuedPacketSender" );
	m_bThreadShouldExit = false;
}

CQueuedPacketSender::~CQueuedPacketSender()
{
	Shutdown();
}

bool CQueuedPacketSender::Setup()
{
	return Start();
}

bool CQueuedPacketSender::Start( unsigned nBytesStack )
{
	Shutdown();

	if ( CThread::Start( nBytesStack ) )
	{
		// Ahhh the perfect cross-platformness of the threads library.
#ifdef IS_WINDOWS_PC
		SetPriority( THREAD_PRIORITY_HIGHEST );
#elif POSIX
		//SetPriority( PRIORITY_MAX );
#endif
		m_bThreadShouldExit = false;
		return true;
	}
	else
	{
		return false;
	}
}

void CQueuedPacketSender::Shutdown()
{
	if ( !IsAlive() )
		return;
		
	m_bThreadShouldExit = true;
	m_hThreadEvent.Set();
	
	Join(); // Wait for the thread to exit.

	while ( m_QueuedPackets.Count() > 0 )
	{
		delete m_QueuedPackets.ElementAtHead();
		m_QueuedPackets.RemoveAtHead();
	}
	m_QueuedPackets.Purge();
}

void CQueuedPacketSender::ClearQueuedPacketsForChannel( INetChannel *pChan )
{
	AUTO_LOCK( m_QueuedPacketsCS );

	for ( int i = m_QueuedPackets.Count()-1; i >= 0; i-- )
	{
		CQueuedPacket *p = m_QueuedPackets.Element( i );
		if ( p->m_pChannel == pChan )
		{
			m_QueuedPackets.RemoveAt( i );
			delete p;
		}
	}
}

bool CQueuedPacketSender::HasQueuedPackets( const INetChannel *pChan ) const
{
	AUTO_LOCK( m_QueuedPacketsCS );

	for ( int i = 0; i < m_QueuedPackets.Count(); ++i )
	{
		const CQueuedPacket *p = m_QueuedPackets.Element( i );
		if ( p->m_pChannel == pChan )
		{
			return true;
		}
	}

	return false;
}
void CQueuedPacketSender::QueuePacket( INetChannel *pChan, SOCKET s, const char FAR *buf, int len, const struct sockaddr FAR * to, int tolen, uint32 msecDelay )
{
	AUTO_LOCK( m_QueuedPacketsCS );

	// We'll pull all packets we should have sent by now and send them out right away
	uint32 msNow = Plat_MSTime();

	int nMaxQueuedPackets = 1024;
	if ( m_QueuedPackets.Count() < nMaxQueuedPackets )
	{
		// Add this packet to the queue.
		CQueuedPacket *pPacket = new CQueuedPacket;
		pPacket->m_unSendTime = msNow + msecDelay;
		pPacket->m_Socket = s;
		pPacket->m_pChannel = pChan;
		pPacket->buf.CopyArray( (char*)buf, len );
		pPacket->to.CopyArray( (char*)to, tolen );
		m_QueuedPackets.Insert( pPacket );
	}
	else
	{
		static int nWarnings = 5;
		if ( --nWarnings > 0 )
		{
			Warning( "CQueuedPacketSender: num queued packets >= nMaxQueuedPackets. Not queueing anymore.\n" );
		}
	}

	// Tell the thread that we have a queued packet.
	m_hThreadEvent.Set();
}

extern int NET_SendToImpl( SOCKET s, const char FAR * buf, int len, const struct sockaddr FAR * to, int tolen, int iGameDataLength );

int CQueuedPacketSender::Run()
{
	 // Normally TT_INFINITE but we wakeup every 50ms just in case.
	uint32 waitIntervalNoPackets = 50;
	uint32 waitInterval = waitIntervalNoPackets;
	while ( 1 )
	{
		if ( m_hThreadEvent.Wait( waitInterval ) )
		{
			// Someone signaled the thread. Either we're being told to exit or 
			// we're being told that a packet was just queued.
			if ( m_bThreadShouldExit )
				return 0;
		}

		// Assume nothing to do and that we'll sleep again
		waitInterval = waitIntervalNoPackets;

		// OK, now send a packet.
		{
			AUTO_LOCK( m_QueuedPacketsCS );
		
			// We'll pull all packets we should have sent by now and send them out right away
			uint32 msNow = Plat_MSTime();

			bool bTrace = net_queue_trace.GetInt() == NET_QUEUED_PACKET_THREAD_DEBUG_VALUE;

			while ( m_QueuedPackets.Count() > 0 )
			{
				CQueuedPacket *pPacket = m_QueuedPackets.ElementAtHead();
				if ( pPacket->m_unSendTime > msNow )
				{
					// Sleep until next we need this packet
					waitInterval = pPacket->m_unSendTime - msNow;
					// Emit ETW events to help with diagnosing network throttling issues as
					// these often have a severe effect on load times in Dota.
					ETWMark1I( "CQueuedPacketSender::Run sleeping (ms)", waitInterval );
					if ( bTrace )
					{
						Warning( "SQ:  sleeping for %u msecs at %f\n", waitInterval, Plat_FloatTime() );
					}
					break;
				}

				// If it's a bot, don't do anything. Note: we DO want this code deep here because bots only
				// try to send packets when sv_stressbots is set, in which case we want it to act as closely
				// as a real player as possible.
				sockaddr_in *pInternetAddr = (sockaddr_in*)pPacket->to.Base();
			#ifdef _WIN32
				if ( pInternetAddr->sin_addr.S_un.S_addr != 0
			#else
				if ( pInternetAddr->sin_addr.s_addr != 0 
			#endif
					&& pInternetAddr->sin_port != 0 )
				{		
					if ( bTrace )
					{
						Warning( "SQ:  sending %d bytes at %f\n", pPacket->buf.Count(), Plat_FloatTime() );
					}

					NET_SendToImpl
					( 
						pPacket->m_Socket, 
						pPacket->buf.Base(), 
						pPacket->buf.Count(), 
						(sockaddr*)pPacket->to.Base(),
						pPacket->to.Count(), 
						-1 
					);
				}	
				
				delete pPacket;
				m_QueuedPackets.RemoveAtHead();
			}
		}
	}
}

