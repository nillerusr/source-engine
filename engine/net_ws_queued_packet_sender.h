//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef NET_WS_QUEUED_PACKET_SENDER_H
#define NET_WS_QUEUED_PACKET_SENDER_H
#ifdef _WIN32
#pragma once
#endif

// Used to match against certain debug values of cvars.
#define NET_QUEUED_PACKET_THREAD_DEBUG_VALUE 581304 

class INetChannel;

class IQueuedPacketSender
{
public:
	virtual bool Setup() = 0;
	virtual void Shutdown() = 0;
	virtual bool IsRunning() = 0;
	virtual void ClearQueuedPacketsForChannel( INetChannel *pChan ) =  0;
	virtual void QueuePacket( INetChannel *pChan, SOCKET s, const char FAR *buf, int len, const struct sockaddr FAR * to, int tolen, uint32 msecDelay ) = 0;
	virtual bool HasQueuedPackets( const INetChannel *pChan ) const = 0;
};

extern IQueuedPacketSender *g_pQueuedPackedSender;
extern ConVar net_queued_packet_thread;

#endif // NET_WS_QUEUED_PACKET_SENDER_H
