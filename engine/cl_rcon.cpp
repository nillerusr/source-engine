//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  implementation of the rcon client
//
//===========================================================================//

// If we are going to include winsock.h then we need to disable protected_things.h
// or else we get many warnings.
#undef PROTECTED_THINGS_ENABLE
#include "tier0/platform.h"
#ifdef POSIX
#include "net_ws_headers.h"
#define WSAGetLastError() errno
#else
#if !defined( _X360 )
#include <winsock.h>
#else
#include "winsockx.h"
#endif
#undef SetPort // winsock screws with the SetPort string... *sigh*8
#endif

#include <tier0/dbg.h>
#include "utlbuffer.h"
#include "cl_rcon.h"
#include "vprof_engine.h"
#include "proto_oob.h" // PORT_RCON define
#include "cmd.h"
#include "tier2/fileutils.h"
#include "zip/XUnzip.h"


#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CRConClient g_RCONClient;
CRConClient & RCONClient()
{
	return g_RCONClient;
}

#ifdef ENABLE_RPT
class CRPTClient : public CRConClient
{
	typedef CRConClient BaseClass;

public:
	virtual void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData )
	{
		BaseClass::OnSocketAccepted( hSocket, netAdr, ppData );

		// Immediately try to start vprofiling
		// Also, enable cheats on this client only
		Cmd_SetRptActive( true );
		StartVProfData();
	}

	virtual void OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData )
	{
		StopVProfData();
		Cmd_SetRptActive( false );
		BaseClass::OnSocketClosed( hSocket, netAdr, pData );
	}
};

static CRPTClient g_RPTClient;
CRConClient & RPTClient()
{
	return g_RPTClient;
}
#endif // ENABLE_RPT

static void RconAddressChanged_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
#ifndef SWDS
	ConVarRef var( pConVar );
	netadr_t to;

	const char *cmdargs = var.GetString(); 
	if ( ( !cmdargs || !cmdargs[ 0 ] ) && cl.m_NetChannel )
	{
		to = cl.m_NetChannel->GetRemoteAddress();
	}
	else if ( !NET_StringToAdr( cmdargs, &to ) )
	{
		Msg( "Unable to resolve rcon address %s\n", var.GetString() );
		return;
	}

	Msg( "Setting rcon_address: %s:%d\n", to.ToString( true ), to.GetPort() );
	RCONClient().SetAddress( to );
#endif
}

static ConVar	rcon_address( "rcon_address", "", FCVAR_SERVER_CANNOT_QUERY|FCVAR_DONTRECORD, "Address of remote server if sending unconnected rcon commands (format x.x.x.x:p) ", RconAddressChanged_f );



//-----------------------------------------------------------------------------
// Implementation of remote vprof
//-----------------------------------------------------------------------------
CRConVProfExport::CRConVProfExport()
{
}

void CRConVProfExport::AddListener()
{
}

void CRConVProfExport::RemoveListener()
{
}

void CRConVProfExport::SetBudgetFlagsFilter( int filter )
{
}

int CRConVProfExport::GetNumBudgetGroups()
{
	return m_Info.Count();
}

void CRConVProfExport::GetBudgetGroupInfos( CExportedBudgetGroupInfo *pInfos )
{
	memcpy( pInfos, m_Info.Base(), GetNumBudgetGroups() * sizeof(CExportedBudgetGroupInfo) );
}

void CRConVProfExport::GetBudgetGroupTimes( float times[IVProfExport::MAX_BUDGETGROUP_TIMES] )
{
	int nGroups = min( m_Times.Count(), (int)IVProfExport::MAX_BUDGETGROUP_TIMES );
	memset( times, 0, nGroups * sizeof(float) );
	nGroups = min( GetNumBudgetGroups(), nGroups );
	memcpy( times, m_Times.Base(), nGroups * sizeof(float) );
}

void CRConVProfExport::PauseProfile()
{
	// NOTE: This only has effect when testing on a listen server
	// it shouldn't do anything in the wild. When drawing the budget panel
	// this will cause the time spent doing so to not be counted
	VProfExport_Pause();
}

void CRConVProfExport::ResumeProfile()
{
	// NOTE: This only has effect when testing on a listen server
	// it shouldn't do anything in the wild
	VProfExport_Resume();
}		

void CRConVProfExport::CleanupGroupData()
{
	int nCount = m_Info.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_Info[i].m_pName;
	}

	m_Info.RemoveAll();
}

void CRConVProfExport::OnRemoteGroupData( const void *data, int len )
{
	CUtlBuffer buf( data, len, CUtlBuffer::READ_ONLY );
	int nFirstGroup = buf.GetInt();

	if ( nFirstGroup == 0 )
	{
		CleanupGroupData();
	}
	else
	{
		Assert( nFirstGroup == m_Info.Count() );
	}

	// NOTE: See WriteRemoteVProfGroupData in vprof_engine.cpp
	// to see the encoding of this data
	int nGroupCount = buf.GetInt();
	int nBase = m_Info.AddMultipleToTail( nGroupCount );
	char temp[1024];
	for ( int i = 0; i < nGroupCount; ++i )
	{
		CExportedBudgetGroupInfo *pInfo = &m_Info[nBase + i];

		unsigned char red, green, blue, alpha;
		red = buf.GetUnsignedChar( );
		green = buf.GetUnsignedChar( );
		blue = buf.GetUnsignedChar( );
		alpha = buf.GetUnsignedChar( );
		buf.GetString( temp );
		int nLen = Q_strlen( temp );

		pInfo->m_Color.SetColor( red, green, blue, alpha );
		char *pBuf = new char[ nLen + 1 ];
		pInfo->m_pName = pBuf;
		memcpy( pBuf, temp, nLen+1 );
		pInfo->m_BudgetFlags = 0;
	}
}

void CRConVProfExport::OnRemoteData( const void *data, int len )
{
	// NOTE: See WriteRemoteVProfData in vprof_engine.cpp
	// to see the encoding of this data
	int nCount = len / sizeof(float);
	Assert( nCount == m_Info.Count() );

	CUtlBuffer buf( data, len, CUtlBuffer::READ_ONLY );
	m_Times.SetCount( nCount );
	memcpy( m_Times.Base(), data, nCount * sizeof(float) );
}


CON_COMMAND( vprof_remote_start, "Request a VProf data stream from the remote server (requires authentication)" )
{
	// TODO: Make this work (it might already!)
//	RCONClient().StartVProfData();
}

CON_COMMAND( vprof_remote_stop, "Stop an existing remote VProf data request" )
{
	// TODO: Make this work (it might already!)
//	RCONClient().StopVProfData();
}

#ifdef ENABLE_RPT
CON_COMMAND_F( rpt_screenshot, "", FCVAR_HIDDEN | FCVAR_DONTRECORD )
{
	RPTClient().TakeScreenshot();
}

CON_COMMAND_F( rpt_download_log, "", FCVAR_HIDDEN | FCVAR_DONTRECORD )
{
	RPTClient().GrabConsoleLog();
}
#endif // ENABLE_RPT

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#pragma warning ( disable : 4355 )

CRConClient::CRConClient() : m_Socket( this )
{
	m_bAuthenticated = false;
	m_iAuthRequestID = 1; // must start at 1
	m_iReqID = 0;
	m_nScreenShotIndex = 0;
	m_nConsoleLogIndex = 0;
}

#pragma warning ( default : 4355 )

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRConClient::~CRConClient()
{
}


//-----------------------------------------------------------------------------
// Changes the password
//-----------------------------------------------------------------------------
void CRConClient::SetPassword( const char *pPassword )
{
	m_Socket.CloseAllAcceptedSockets();
	m_Password = pPassword;
}

void CRConClient::SetRemoteFileDirectory( const char *pDir )
{
	m_RemoteFileDir = pDir;
	m_nScreenShotIndex = 0;
	m_nConsoleLogIndex = 0;
	g_pFullFileSystem->CreateDirHierarchy( pDir, "MOD" );
}


//-----------------------------------------------------------------------------
// Purpose: set the addresss of the remote server
//-----------------------------------------------------------------------------
void CRConClient::SetAddress( const netadr_t &netAdr )
{
	m_Socket.CloseAllAcceptedSockets();
	m_Address = netAdr;
	if ( m_Address.GetPort() == 0  )
	{
		m_Address.SetPort( PORT_SERVER ); // override the port setting, by default rcon tries to bind to the same port as the server
	}
}


//-----------------------------------------------------------------------------
// Inherited from ISocketCreatorListener
//-----------------------------------------------------------------------------
bool CRConClient::ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr )
{
	// Can't connect if we're already connected
	return !IsConnected();
}

void CRConClient::OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData )
{
}

void CRConClient::OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData )
{
	// reset state
	m_bAuthenticated = false;
	m_iReqID = 0;
	m_iAuthRequestID = 1; // must start at 1
	m_SendBuffer.Purge();
	m_RecvBuffer.Purge();
}



//-----------------------------------------------------------------------------
// Connects to the address specified by SetAddress
//-----------------------------------------------------------------------------
bool CRConClient::ConnectSocket()
{
	if ( m_Socket.ConnectSocket( m_Address, true ) < 0 )
	{
		Warning( "Unable to connect to remote server (%s)\n", m_Address.ToString() );
		return false;
	}
	return true;
}

void CRConClient::CloseSocket()
{
	m_Socket.CloseAllAcceptedSockets();
}


//-----------------------------------------------------------------------------
// Are we connected?
//-----------------------------------------------------------------------------
bool CRConClient::IsConnected()	const
{
	return m_Socket.GetAcceptedSocketCount() > 0;
}


//-----------------------------------------------------------------------------
// Creates a listen server, connects to remote machines that connect to it
//-----------------------------------------------------------------------------
void CRConClient::CreateListenSocket( const netadr_t &netAdr )
{
	m_Socket.CreateListenSocket( netAdr );
}

void CRConClient::CloseListenSocket()
{
	m_Socket.CloseListenSocket( );
}


//-----------------------------------------------------------------------------
// Purpose: send queued messages
//-----------------------------------------------------------------------------
void CRConClient::SendQueuedData()
{
	SocketHandle_t hSocket = GetSocketHandle();
	while ( m_SendBuffer.TellMaxPut() - m_SendBuffer.TellGet() > sizeof(int) )
	{
		size_t nSize = *(int*)m_SendBuffer.PeekGet();
		Assert( nSize >= m_SendBuffer.TellMaxPut() - m_SendBuffer.TellGet() - sizeof( int ) );
		int ret = send( hSocket, (const char *)m_SendBuffer.PeekGet(), nSize + sizeof( int ), 0 );
		if ( ret != -1 )
		{
			m_SendBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nSize + sizeof( int ) );
			continue;
		}

		if ( !SocketWouldBlock() )
		{
			Warning( "Lost RCON connection, please retry command.\n"); 
			CloseSocket();
		}
		break;
	}

	int nSizeRemaining = m_SendBuffer.TellMaxPut() - m_SendBuffer.TellGet();
	if ( nSizeRemaining <= sizeof(int) )
	{
		m_SendBuffer.Purge();
		return;
	}

	// In this case, we've still got queued messages to send
	// Keep the portion of the buffer we didn't process for next time
	CUtlBuffer tmpBuf;
	tmpBuf.Put( m_SendBuffer.PeekGet(), nSizeRemaining );
	m_SendBuffer.Purge();
	m_SendBuffer.Put( tmpBuf.Base(), tmpBuf.TellPut() );
}


//-----------------------------------------------------------------------------
// Purpose: parse received data
//-----------------------------------------------------------------------------
void CRConClient::ParseReceivedData()
{
	m_RecvBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	int size = m_RecvBuffer.GetInt();
	while ( size && size <= m_RecvBuffer.TellPut() - m_RecvBuffer.TellGet() )
	{
		//DevMsg( "RCON: got packet %i long\n", size );
		int reqID = m_RecvBuffer.GetInt();
		int cmdID = m_RecvBuffer.GetInt(); // ignore the cmd id
		//		DevMsg( "RCON Cmd: <-- %i %i %i\n", reqID, cmdID, readLen );

		switch( cmdID )
		{
		case SERVERDATA_AUTH_RESPONSE:
			{
				if ( reqID == -1 ) // bad password
				{
					Msg( "Bad RCON password\n" );
					m_bAuthenticated = false;
				}
				else
				{
					Assert( reqID == m_iAuthRequestID );
					m_bAuthenticated = true;
				}
				char dummy[2];
				m_RecvBuffer.GetString( dummy );
				m_RecvBuffer.GetString( dummy );
			}
			break;

		case SERVERDATA_SCREENSHOT_RESPONSE:
			{
				int nDataSize = m_RecvBuffer.GetInt();
				SaveRemoteScreenshot( m_RecvBuffer.PeekGet(), nDataSize );
				m_RecvBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nDataSize );
			}
			break;

		case SERVERDATA_CONSOLE_LOG_RESPONSE:
			{
				int nDataSize = m_RecvBuffer.GetInt();
				SaveRemoteConsoleLog( m_RecvBuffer.PeekGet(), nDataSize );
				m_RecvBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nDataSize );
			}
			break;

		case SERVERDATA_VPROF_DATA:
			{
				int nDataSize = m_RecvBuffer.GetInt();
				m_VProfExport.OnRemoteData( m_RecvBuffer.PeekGet(), nDataSize );
				m_RecvBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nDataSize );
			}
			break;

		case SERVERDATA_VPROF_GROUPS:
			{
				int nDataSize = m_RecvBuffer.GetInt();
				m_VProfExport.OnRemoteGroupData( m_RecvBuffer.PeekGet(), nDataSize );
				m_RecvBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nDataSize );
			}
			break;

		case SERVERDATA_RESPONSE_STRING:
			{
				char pBuf[2048];
				m_RecvBuffer.GetString( pBuf );
				Msg( "%s", pBuf );
			}
			break;

		default:
			{
				// Displays a message from the server
				int strLen = m_RecvBuffer.TellPut() - m_RecvBuffer.TellGet();
				CUtlMemory<char> msg;
				msg.EnsureCapacity( strLen + 1 );
				m_RecvBuffer.GetStringManualCharCount( msg.Base(), msg.Count() );

				msg[ msg.Count() - 1 ] = '\0';
				Msg( "%s", (const char *)msg.Base() );
				m_RecvBuffer.GetStringManualCharCount( msg.Base(), msg.Count() ); // ignore the second string
			}
			break;
		}

		if ( m_RecvBuffer.TellPut() - m_RecvBuffer.TellGet() >= sizeof(int) )
		{
			size = m_RecvBuffer.GetInt(); // read how much is in this packet
		}
		else
		{
			size = 0; // finished the packet
		}
	}

	if ( size || (m_RecvBuffer.TellPut() - m_RecvBuffer.TellGet() > 0) )
	{
		// In this case, we've got a partial message; we didn't get it all.
		// Keep the portion of the buffer we didn't process for next time
		CUtlBuffer tmpBuf;
		if ( m_RecvBuffer.TellPut() - m_RecvBuffer.TellGet() > 0 )
		{
			tmpBuf.Put( m_RecvBuffer.PeekGet(), m_RecvBuffer.TellPut() - m_RecvBuffer.TellGet() );
		}
		m_RecvBuffer.Purge();
		if ( size > 0 )
		{
			m_RecvBuffer.PutInt( size );
		}
		if ( tmpBuf.TellPut() > 0 )
		{
			m_RecvBuffer.Put( tmpBuf.Base(), tmpBuf.TellPut() );
		}
	}
	else
	{
		m_RecvBuffer.Purge();
	}
}


//-----------------------------------------------------------------------------
// Purpose: check for any server responses
//-----------------------------------------------------------------------------
void CRConClient::RunFrame()
{
	m_Socket.RunFrame();

	if ( !IsConnected() )
		return;

	SendQueuedData();

	SocketHandle_t hSocket = GetSocketHandle();
	char ch;
	int pendingLen = recv( hSocket, &ch, sizeof(ch), MSG_PEEK );
	if ( pendingLen == -1 && SocketWouldBlock() )
		return;

	if ( pendingLen == 0 ) // socket got closed
	{
		CloseSocket();
		return;
	}
	
	if ( pendingLen < 0 )
	{
		CloseSocket();
		Warning( "Lost RCON connection, please retry command (%s)\n", NET_ErrorString( WSAGetLastError() ) );
		return;
	}

	// find out how much we have to read
	unsigned long readLen = 0;
	ioctlsocket( hSocket, FIONREAD, &readLen );
	if ( readLen <= sizeof(int) ) 
		return;

	// we have a command to process
	// Read data into a utlbuffer
	m_RecvBuffer.EnsureCapacity( m_RecvBuffer.TellPut() + readLen + 1 );
	char *recvbuffer = (char *)_alloca( min( 1024ul, readLen + 1 ) );
	unsigned int len = 0;
	while ( len < readLen )
	{
		int recvLen = recv( hSocket, recvbuffer , min( 1024ul, readLen - len ) , 0 );
		if ( recvLen == 0 ) // socket was closed
		{
			CloseSocket();
			break;
		}
		
		if ( recvLen < 0 && !SocketWouldBlock() )
		{
			Warning( "RCON Cmd: recv error (%s)\n", NET_ErrorString( WSAGetLastError() ) );
			break;
		}

		m_RecvBuffer.Put( recvbuffer, recvLen );
		len += recvLen;
	}
	
	ParseReceivedData();
}


//-----------------------------------------------------------------------------
// Purpose: send a response to the server
//-----------------------------------------------------------------------------
void CRConClient::SendResponse( CUtlBuffer &response, bool bAutoAuthenticate )
{
	if ( bAutoAuthenticate && !IsAuthenticated() )
	{
		Authenticate();
		if ( IsConnected() )
		{
			m_SendBuffer.Put( response.Base(), response.TellMaxPut() );
		}
		return;
	}

	int ret = send( GetSocketHandle(), (const char *)response.Base(), response.TellMaxPut(), 0 );
	if ( ret == -1 )
	{
		if ( SocketWouldBlock() )
		{
			m_SendBuffer.Put( response.Base(), response.TellMaxPut() );
		}
		else
		{
			Warning( "Lost RCON connection, please retry command\n" ); 
			CloseSocket();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: builds a simple command to send to the server
//-----------------------------------------------------------------------------
void CRConClient::BuildResponse( CUtlBuffer &response, ServerDataRequestType_t msg, const char *pString1, const char *pString2 )
{
	// build the response
	response.PutInt(0); // the size, filled in below
	response.PutInt(m_iReqID++);
	response.PutInt(msg);
	response.PutString(pString1);
	response.PutString(pString2);
	int nSize = response.TellPut() - sizeof(int); 
	response.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	response.PutInt( nSize ); // the size
	response.SeekPut( CUtlBuffer::SEEK_CURRENT, nSize );
}


//-----------------------------------------------------------------------------
// Purpose: authenticate ourselves
//-----------------------------------------------------------------------------
void CRConClient::Authenticate()
{
	CUtlBuffer response;

	// build the response
	response.PutInt(0); // the size, filled in below
	response.PutInt(++m_iAuthRequestID);
	response.PutInt(SERVERDATA_AUTH);
	response.PutString( m_Password.Get() );

	// Use the otherwise-empty second string for the userid.  The server will use this to
	// exec "mp_disable_autokick <userid>" upon successful authentication.
	bool addedUserID = false;
	if ( cl.IsConnected() )
	{
		if ( cl.m_nPlayerSlot < cl.m_nMaxClients && cl.m_nPlayerSlot >= 0 )
		{
			Assert( cl.m_pUserInfoTable );
			if ( cl.m_pUserInfoTable )
			{
				player_info_t *pi = (player_info_t*) cl.m_pUserInfoTable->GetStringUserData( cl.m_nPlayerSlot, NULL );
				if ( pi )
				{
					addedUserID = true;
					// Fixup from network order (little endian)
					response.PutString( va( "%d", LittleLong( pi->userID ) ) );
				}
			}
		}
	}

	if ( !addedUserID )
	{
		response.PutString( "" );
	}
	int size = response.TellPut() - sizeof(int); 
	response.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	response.PutInt(size); // the size
	response.SeekPut( CUtlBuffer::SEEK_CURRENT, size );

	SendResponse( response, false );
}


//-----------------------------------------------------------------------------
// Purpose: send an rcon command to a connected server
//-----------------------------------------------------------------------------
void CRConClient::SendCmd( const char *msg )
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	CUtlBuffer response;
	BuildResponse( response, SERVERDATA_EXECCOMMAND, msg, "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// Purpose: Start vprofiling
//-----------------------------------------------------------------------------
void CRConClient::StartVProfData()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	// Override the vprof export to point to our local profiling data
	OverrideVProfExport( &m_VProfExport );

	CUtlBuffer response;
	BuildResponse( response, SERVERDATA_VPROF, "", "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// Purpose: Stop vprofiling
//-----------------------------------------------------------------------------
void CRConClient::StopVProfData()
{
	// Reset the vprof export to point to the normal profiling data
	ResetVProfExport( &m_VProfExport );

	// Don't bother restarting a connection to turn this off
	if ( !IsConnected() )
		return;

	CUtlBuffer response;
	BuildResponse( response, SERVERDATA_REMOVE_VPROF, "", "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// Purpose: get data from the server
//-----------------------------------------------------------------------------
void CRConClient::TakeScreenshot()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	CUtlBuffer response;
	BuildResponse( response, SERVERDATA_TAKE_SCREENSHOT, "", "" );
	SendResponse( response );
}

void CRConClient::GrabConsoleLog()
{
	if ( !IsConnected() )
	{
		if ( !ConnectSocket() )
			return;
	}

	CUtlBuffer response;
	BuildResponse( response, SERVERDATA_SEND_CONSOLE_LOG, "", "" );
	SendResponse( response );
}


//-----------------------------------------------------------------------------
// We've got data from the server, save it
//-----------------------------------------------------------------------------
void CRConClient::SaveRemoteScreenshot( const void* pBuffer, int nBufLen )
{
	char pScreenshotPath[MAX_PATH];
	do 
	{
		Q_snprintf( pScreenshotPath, sizeof( pScreenshotPath ), "%s/screenshot%04d.jpg", m_RemoteFileDir.Get(), m_nScreenShotIndex++ );	
	} while ( g_pFullFileSystem->FileExists( pScreenshotPath, "MOD" ) );

	char pFullPath[MAX_PATH];
	GetModSubdirectory( pScreenshotPath, pFullPath, sizeof(pFullPath) );
	HZIP hZip = OpenZip( (void*)pBuffer, nBufLen, ZIP_MEMORY );

	int nIndex;
	ZIPENTRY zipInfo;
	FindZipItem( hZip, "screenshot.jpg", true, &nIndex, &zipInfo );
	if ( nIndex >= 0 )
	{
		UnzipItem( hZip, nIndex, pFullPath, 0, ZIP_FILENAME );
	}
	CloseZip( hZip );
}

void CRConClient::SaveRemoteConsoleLog( const void* pBuffer, int nBufLen )
{
	if ( nBufLen == 0 )
		return;

	char pLogPath[MAX_PATH];
	do 
	{
		Q_snprintf( pLogPath, sizeof( pLogPath ), "%s/console%04d.log", m_RemoteFileDir.Get(), m_nConsoleLogIndex++ );	
	} while ( g_pFullFileSystem->FileExists( pLogPath, "MOD" ) );

	char pFullPath[MAX_PATH];
	GetModSubdirectory( pLogPath, pFullPath, sizeof(pFullPath) );
	HZIP hZip = OpenZip( (void*)pBuffer, nBufLen, ZIP_MEMORY );

	int nIndex;
	ZIPENTRY zipInfo;
	FindZipItem( hZip, "console.log", true, &nIndex, &zipInfo );
	if ( nIndex >= 0 )
	{
		UnzipItem( hZip, nIndex, pFullPath, 0, ZIP_FILENAME );
	}
	CloseZip( hZip );
}
