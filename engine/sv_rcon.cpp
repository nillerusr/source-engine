//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  implementation of the rcon server 
//
//===========================================================================//


#if defined(_WIN32)
#if !defined(_X360)
#include <winsock.h>
#endif
#undef SetPort // winsock screws with the SetPort string... *sigh*
#define socklen_t int
#define MSG_NOSIGNAL 0
#elif POSIX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <sys/ioctl.h>
#define closesocket close
#define WSAGetLastError() errno
#define ioctlsocket ioctl
#ifdef OSX
#define MSG_NOSIGNAL 0
#endif
#endif
#include <tier0/dbg.h>
#include "utlbuffer.h"
#include "server.h"
#include "sv_rcon.h"
#include "proto_oob.h" // PORT_RCON define
#include "sv_remoteaccess.h"
#include "cl_rcon.h"
#include "sv_filter.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef ENABLE_RPT
class CRPTServer : public CRConServer
{
	typedef CRConServer BaseClass;

public:
	virtual void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData )
	{
		BaseClass::OnSocketAccepted( hSocket, netAdr, ppData );

		// Enable cheats on this client only
		Cmd_SetRptActive( true );
	}

	virtual void OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData )
	{
		Cmd_SetRptActive( false );
		BaseClass::OnSocketClosed( hSocket, netAdr, pData );
	}
};


static CRPTServer g_RPTServer;
CRConServer & RPTServer()
{
	return g_RPTServer;
}
#endif // ENABLE_RPT

static CRConServer g_RCONServer;
CRConServer & RCONServer()
{
	return g_RCONServer;
}

static void RconPasswordChanged_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	const char *pPassword = var.GetString(); 
#ifndef SWDS
	RCONClient().SetPassword( pPassword );
#endif
	RCONServer().SetPassword( pPassword );

}
ConVar  rcon_password	( "rcon_password", "", FCVAR_SERVER_CANNOT_QUERY|FCVAR_DONTRECORD, "remote console password.", RconPasswordChanged_f );

ConVar sv_rcon_banpenalty( "sv_rcon_banpenalty", "0", 0, "Number of minutes to ban users who fail rcon authentication", true, 0, false, 0 );
ConVar sv_rcon_maxfailures( "sv_rcon_maxfailures", "10", 0, "Max number of times a user can fail rcon authentication before being banned", true, 1, true, 20 );
ConVar sv_rcon_minfailures( "sv_rcon_minfailures", "5", 0, "Number of times a user can fail rcon authentication in sv_rcon_minfailuretime before being banned", true, 1, true, 20 );
ConVar sv_rcon_minfailuretime( "sv_rcon_minfailuretime", "30", 0, "Number of seconds to track failed rcon authentications", true, 1, false, 0 );
ConVar sv_rcon_whitelist_address( "sv_rcon_whitelist_address", "", 0, "When set, rcon failed authentications will never ban this address, e.g. '127.0.0.1'" );

ConVar sv_rcon_maxpacketsize( "sv_rcon_maxpacketsize", "1024", 0, "The maximum number of bytes to allow in a command packet", true, 0, false, 0 );
ConVar sv_rcon_maxpacketbans( "sv_rcon_maxpacketbans", "1", 0, "Ban IPs for sending RCON packets exceeding the value specified in sv_rcon_maxpacketsize", true, 0, true, 1 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#pragma warning ( disable : 4355 )

CRConServer::CRConServer() : m_Socket( this )
{
}

CRConServer::CRConServer( const char *pNetAddress ) : m_Socket( this )
{
	SetAddress( pNetAddress );
}

#pragma warning ( default : 4355 )


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRConServer::~CRConServer()
{
}


//-----------------------------------------------------------------------------
// Allows a server to request a listening client to connect to it
//-----------------------------------------------------------------------------
bool CRConServer::ConnectToListeningClient( const netadr_t &adr, bool bSingleSocket )
{
	if ( m_Socket.ConnectSocket( adr, bSingleSocket ) < 0 )
	{
		ConWarning( "Unable to connect to remote client (%s)\n", adr.ToString() );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the listening socket is created and listening
//-----------------------------------------------------------------------------
bool CRConServer::IsConnected()
{
	return m_Socket.IsListening();
}

void CRConServer::SetPassword( const char *pPassword )
{
	m_Socket.CloseAllAcceptedSockets();
	m_Password = pPassword;
}

bool CRConServer::HasPassword() const
{
	return !m_Password.IsEmpty();
}

bool CRConServer::IsPassword( const char *pPassword ) const
{
	// Must have a password set to allow any rconning.
	if ( !HasPassword() )
		return false;

	// If the pw does not match, then not authed
	return ( Q_strcmp( pPassword, m_Password.Get() ) == 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Set the address to bind to
//-----------------------------------------------------------------------------
void CRConServer::SetAddress( const char *pNetAddress )
{
	NET_StringToAdr( pNetAddress, &m_Address );
	if ( m_Address.GetPort() == 0 )
	{
		m_Address.SetPort( PORT_RCON );
	}
}

bool CRConServer::CreateSocket()
{
	return m_Socket.CreateListenSocket( m_Address );
}


//-----------------------------------------------------------------------------
// Inherited from ISocketCreatorListener
//-----------------------------------------------------------------------------
bool CRConServer::ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr )
{
	return !Filter_ShouldDiscard( netAdr );
}

void CRConServer::OnSocketAccepted( SocketHandle_t hSocket, const netadr_t &netAdr, void** ppData )
{
	ConnectedRConSocket_t *pNewSocket = new ConnectedRConSocket_t;
	pNewSocket->lastRequestID = 0;
	pNewSocket->authed = false;
	pNewSocket->listenerID = g_ServerRemoteAccess.GetNextListenerID( true, &netAdr );
	*ppData = pNewSocket;
}

void CRConServer::OnSocketClosed( SocketHandle_t hSocket, const netadr_t &netAdr, void* pData )
{
	m_bSocketDeleted = true;
	ConnectedRConSocket_t *pOldSocket = (ConnectedRConSocket_t*)( pData );
	delete pOldSocket;
}



//-----------------------------------------------------------------------------
// Purpose: accept new connections and walk open sockets and handle any incoming data
//-----------------------------------------------------------------------------
void CRConServer::RunFrame()
{
	m_Socket.RunFrame();
	m_bSocketDeleted = false;

	// handle incoming data
	// NOTE: Have to iterate in reverse since we may be killing sockets
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = nCount - 1; i >= 0; --i )
	{
		// process any outgoing data for this socket
		ConnectedRConSocket_t *pData = GetSocketData( i );
		SocketHandle_t hSocket = m_Socket.GetAcceptedSocketHandle( i );
		const netadr_t& socketAdr = m_Socket.GetAcceptedSocketAddress( i );
		while ( pData->m_OutstandingSends.Count() > 0 )
		{
			CUtlBuffer &packet = pData->m_OutstandingSends[ pData->m_OutstandingSends.Head()];
			bool bSent = SendRCONResponse( i, packet.PeekGet(), packet.TellPut() - packet.TellGet(), true );
			if ( bSent ) // all this packet was sent, remove it
			{
				pData->m_OutstandingSends.Remove( pData->m_OutstandingSends.Head() ); // delete this entry no matter what, SendRCONResponse() will re-queue if needed
			}
			else // must have blocked part way through, SendRCONResponse
			    // fixed up the queued entry
			{
				break;
			}
		}
		
		int sendLen = g_ServerRemoteAccess.GetDataResponseSize( pData->listenerID );
		if ( sendLen > 0 )
		{
			char sendBuf[4096];
			char *pBuf = sendBuf;
			bool bAllocate = ( sendLen + sizeof(int) > sizeof(sendBuf) );
			if ( bAllocate )
			{
				pBuf = new char[sendLen + sizeof(int)];
			}
			memcpy( pBuf, &sendLen, sizeof(sendLen) ); // copy the size of the packet in
			g_ServerRemoteAccess.ReadDataResponse( pData->listenerID, pBuf + sizeof(int), sendLen );
			SendRCONResponse( i, pBuf, sendLen + sizeof(int) );
			if ( bAllocate )
			{
				delete [] pBuf;
			}
		}

		// check for incoming data
		int pendingLen = 0;
		unsigned long readLen = 0;
		char ch;
		pendingLen = recv( hSocket, &ch, sizeof(ch), MSG_PEEK );
		if ( pendingLen == -1 && SocketWouldBlock() )
			continue;

		if ( pendingLen == 0 )
		{
			m_Socket.CloseAcceptedSocket( i );
			continue;
		}

		if ( pendingLen < 0 )
		{
			//DevMsg( "RCON Cmd: peek error %s\n", NET_ErrorString(WSAGetLastError()));
			m_Socket.CloseAcceptedSocket( i );
			continue;
		}

		// find out how much we have to read
		ioctlsocket( hSocket, FIONREAD, &readLen );
		if ( readLen > sizeof(int) ) // we have a command to process
		{
			CUtlBuffer & response = pData->packetbuffer;
			response.EnsureCapacity( response.TellPut() + readLen );
			char *recvBuf = (char *)_alloca( min( 1024ul, readLen ) ); // a buffer used for recv()
			unsigned int len = 0;
			while ( len < readLen )
			{
				int recvLen = recv( hSocket, recvBuf , min(1024ul, readLen - len) , 0 );
				if ( recvLen == 0 ) // socket was closed
				{
					m_Socket.CloseAcceptedSocket( i );
					break;
				}
				
				if ( recvLen < 0 && !SocketWouldBlock() )
				{
					Warning( "RCON Cmd: recv error (%s)\n", NET_ErrorString( WSAGetLastError() ) );
					break;
				}

				response.Put( recvBuf, recvLen );
				len += recvLen;
			}
			
			response.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

			int size = response.GetInt();
		
			if ( sv_rcon_maxpacketsize.GetInt() > 0 && size > sv_rcon_maxpacketsize.GetInt() )
			{
				if ( sv_rcon_maxpacketbans.GetBool() )
				{
					HandleFailedRconAuth( socketAdr );
				}

				m_Socket.CloseAcceptedSocket( i );
				continue;
			}
			
			while ( size > 0 && size <= response.TellPut() - response.TellGet() )
			{
				SV_RedirectStart( RD_SOCKET, &socketAdr );
				g_ServerRemoteAccess.WriteDataRequest( this, pData->listenerID, response.PeekGet(), size );
				SV_RedirectEnd();
				if ( m_bSocketDeleted )
					return;
				response.SeekGet( CUtlBuffer::SEEK_CURRENT, size ); // eat up the buffer we just sent

				if ( response.TellPut() - response.TellGet() >= sizeof(int) )
				{
					size = response.GetInt(); // read how much is in this packet
				}
				else
				{
					size = 0; // finished the packet
				}
			}

			// Check and see if socket was closed as a result of processing - this can happen if the user has entered too many passwords
			int nNewCount = m_Socket.GetAcceptedSocketCount();
			if ( 0 == nNewCount || i > nNewCount || pData != GetSocketData( i )  ) 
			{
				response.Purge();
				break;
			}

			if ( size > 0 || (response.TellPut() - response.TellGet() > 0))
			{
				// trim the bytes that were just processed
				CUtlBuffer tmpBuf;
				if ( response.TellPut() - response.TellGet() > 0 )
				{
					tmpBuf.Put( response.PeekGet(), response.TellPut() - response.TellGet() );
				}

				response.Purge();

				if ( size > 0 )
				{
					response.Put( &size, sizeof(size));
				}

				if ( tmpBuf.TellPut() > 0 )
				{
					response.Put( tmpBuf.Base(), tmpBuf.TellPut() );
				}
			}
			else
			{
				response.Purge();
			}
		}
	} // for each socket
}


//-----------------------------------------------------------------------------
// Purpose: flush the response of a network command back to a user
//-----------------------------------------------------------------------------
void CRConServer::FinishRedirect( const char *msg, const netadr_t &adr )
{	
	// NOTE: Has to iterate in reverse; SendRCONResponse can close sockets
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = nCount - 1; i >= 0; --i )
	{
		const netadr_t& socketAdr = m_Socket.GetAcceptedSocketAddress( i );
		if ( !adr.CompareAdr( socketAdr ) )
			continue;

		CUtlBuffer response;

		// build the response
		ConnectedRConSocket_t *pSocketData = GetSocketData( i );
		response.PutInt(0); // the size, this gets set once we make the packet
		response.PutInt(pSocketData->lastRequestID);
		response.PutInt(SERVERDATA_RESPONSE_VALUE);
		response.PutString(msg);
		response.PutString("");
		int size = response.TellPut() - sizeof(int); 
		response.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
		response.PutInt(size); // the size
		response.SeekPut( CUtlBuffer::SEEK_CURRENT, size );

		
//		OutputDebugString( va("RCON: String is %i long\n", Q_strlen(msg)) ); // can't use DevMsg(), we are potentially inside the RedirectFlush() function
//		printf("RCON: String is %i long, packet size %i\n", Q_strlen(msg), size );

		SendRCONResponse( i, response.Base(), response.TellPut() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: set the current outstanding request ID for this connection, used by the redirect flush above
//-----------------------------------------------------------------------------
void CRConServer::SetRequestID( ra_listener_id listener, int iRequestID )
{
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = 0; i < nCount; i++ )
	{
		ConnectedRConSocket_t *pSocketData = GetSocketData( i );
		if ( pSocketData->listenerID == listener)
		{
			pSocketData->lastRequestID = iRequestID;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: send a buffer to a particular connection
//-----------------------------------------------------------------------------
bool CRConServer::SendRCONResponse( int nIndex, const void *data, int len, bool fromQueue )
{
	SocketHandle_t hSocket = m_Socket.GetAcceptedSocketHandle( nIndex );
	if ( hSocket < 0 )
		return false;

	ConnectedRConSocket_t *pSocketData = GetSocketData( nIndex );

	// if we already have queued data pending then just add this to the end
	// of the queue
	if ( !fromQueue && pSocketData->m_OutstandingSends.Count() > 0 )
	{
		if ( pSocketData->m_OutstandingSends.Count() > RCON_MAX_OUTSTANDING_SENDS )
		{
			m_Socket.CloseAcceptedSocket( nIndex );
			return false;
		}

		int index = pSocketData->m_OutstandingSends.AddToTail();
		pSocketData->m_OutstandingSends[index].Put( data, len );
		return true;
	}

	Assert( !( fromQueue && data != (pSocketData->m_OutstandingSends[pSocketData->m_OutstandingSends.Head()].Base())));

	int sendLen = 0;
	while ( sendLen < len )
	{
		int ret = send( hSocket, (const char *)data + sendLen, len - sendLen, MSG_NOSIGNAL );
		if ( ret == -1 )
		{
			// can't finish sending this right now, push it back
			// on the TOP of the queue to be sent next time around
			if ( !SocketWouldBlock() )
			{
				m_Socket.CloseAcceptedSocket( nIndex );
				return false;
			}

			if ( !fromQueue ) // we don't have an entry for this
					// yet, add a new one
			{
				int index = pSocketData->m_OutstandingSends.AddToHead();
				pSocketData->m_OutstandingSends[index].Put( (void *)((char *)data + sendLen), len - sendLen );
			}
			else // update the existing queued item to show we 
			     // sent some of it (we only ever send the head of the list)
			{
				pSocketData->m_OutstandingSends[pSocketData->m_OutstandingSends.Head()].SeekGet( CUtlBuffer::SEEK_CURRENT, sendLen );
			}
			return false;
		}
		else if ( ret > 0 )
		{
			sendLen += ret;
		}
	}
//	printf("RCON: Sending packet %i in len\n", len);
//	OutputDebugString( va("RCON: Sending packet %i in len\n", len) ); // can't use DevMsg(), we are potentially inside the RedirectFlush() function
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: compares failed rcons based on most recent failure time
//-----------------------------------------------------------------------------
bool CRConServer::FailedRCon_t::operator<(const struct CRConServer::FailedRCon_t &rhs) const
{
	int myTime = 0;
	int rhsTime = 0;

	if ( badPasswordTimes.Count() )
		myTime = badPasswordTimes[ badPasswordTimes.Count() - 1 ];

	if ( rhs.badPasswordTimes.Count() )
		rhsTime = rhs.badPasswordTimes[ rhs.badPasswordTimes.Count() - 1 ];

	return myTime < rhsTime;
}

//-----------------------------------------------------------------------------
// Purpose: tracks failed rcon attempts and bans repeat offenders
//-----------------------------------------------------------------------------
bool CRConServer::HandleFailedRconAuth( const netadr_t & adr )
{
	if ( sv_rcon_whitelist_address.GetString()[0] )
	{
		if ( !V_strcmp( adr.ToString( true ), sv_rcon_whitelist_address.GetString() ) )
		{
			ConMsg( "Rcon auth failed from rcon whitelist address %s\n", adr.ToString() );
			return false;
		}
	}

	int i;
	FailedRCon_t *failedRcon = NULL;
	int nCount = m_failedRcons.Count();
	for ( i=0; i < nCount; ++i )
	{
		if ( adr.CompareAdr( m_failedRcons[i].adr, true ) )
		{
			failedRcon = &m_failedRcons[i];
			break;
		}
	}

	if ( !failedRcon )
	{
		// remove an old rcon if necessary
		if ( nCount >= 32 )
		{
			// look for the one with the oldest failure
			int indexToRemove = -1;
			for ( i=0; i < nCount; ++i )
			{
				if ( indexToRemove < 0 || m_failedRcons[i] < m_failedRcons[indexToRemove] )
				{
					indexToRemove = i;
				}
			}
			if ( indexToRemove >= 0 )
			{
				m_failedRcons.Remove( indexToRemove );
			}
		}

		// add the new rcon
		int index = m_failedRcons.AddToTail();
		failedRcon = &m_failedRcons[index];
		failedRcon->adr = adr;
		failedRcon->badPasswordCount = 0;
		failedRcon->badPasswordTimes.RemoveAll();
	}

	// update this failed rcon
	++failedRcon->badPasswordCount;
	failedRcon->badPasswordTimes.AddToTail( sv.GetTime() );

	// remove old failure times (sv_rcon_maxfailures is limited to 20, so we won't be hurting anything by pruning)
	while ( failedRcon->badPasswordTimes.Count() > 20 )
	{
		failedRcon->badPasswordTimes.Remove( 0 );
	}

	// sanity-check the rcon banning cvars
	if ( sv_rcon_maxfailures.GetInt() < sv_rcon_minfailures.GetInt() )
	{
		int temp = sv_rcon_maxfailures.GetInt();
		sv_rcon_maxfailures.SetValue( sv_rcon_minfailures.GetInt() );
		sv_rcon_minfailures.SetValue( temp );
	}

//	ConMsg( "%d of %d bad password times tracked\n", failedRcon->badPasswordTimes.Count(), failedRcon->badPasswordCount );
//	ConMsg( "min=%d, max=%d, time=%.2f\n", sv_rcon_minfailures.GetInt(), sv_rcon_maxfailures.GetInt(), sv_rcon_minfailuretime.GetFloat() );

	// check if the user should be banned based on total failed attempts
	if ( failedRcon->badPasswordCount > sv_rcon_maxfailures.GetInt() )
	{
		ConMsg( "Banning %s for rcon hacking attempts\n", failedRcon->adr.ToString( true ) );
		Cbuf_AddText( va( "addip %i %s\n", sv_rcon_banpenalty.GetInt(), failedRcon->adr.ToString( true ) ) );
		Cbuf_Execute();
		return true;
	}

	// check if the user should be banned based on recent failed attempts
	int recentFailures = 0;
	for ( i=failedRcon->badPasswordTimes.Count()-1; i>=0; --i )
	{
		if ( failedRcon->badPasswordTimes[i] + sv_rcon_minfailuretime.GetFloat() >= sv.GetTime() )
		{
			++recentFailures;
		}
	}
	if ( recentFailures > sv_rcon_minfailures.GetInt() )
	{
		ConMsg( "Banning %s for rcon hacking attempts\n", failedRcon->adr.ToString( true ) );
		Cbuf_AddText( va( "addip %i %s\n", sv_rcon_banpenalty.GetInt(), failedRcon->adr.ToString( true ) ) );
		Cbuf_Execute();
		return true;
	}

	return false;
}

bool CRConServer::BCloseAcceptedSocket( ra_listener_id listener )
{
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = 0; i < nCount; i++ )
	{
		ConnectedRConSocket_t *pSocketData = GetSocketData( i );
		if ( pSocketData->listenerID == listener )
		{
			m_Socket.CloseAcceptedSocket( i );
			return true;
		}
	}
	return false;
}
