//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: simple TCP socket API for communicating as a TCP client over a TEXT
//			connection
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include <winsock.h>

#include "simplesocket.h"


static REPORTFUNCTION g_SocketReport = NULL;

//-----------------------------------------------------------------------------
// Purpose: intialize sockets
//-----------------------------------------------------------------------------
void SocketInit( void )
{
	WSADATA wsData;

	WORD wVersionRequested = MAKEWORD(1, 1);
	WSAStartup(wVersionRequested, &wsData);
}


//-----------------------------------------------------------------------------
// Purpose: cleanup all socket resources
//-----------------------------------------------------------------------------
void SocketExit( void )
{
	WSACleanup();
}


//-----------------------------------------------------------------------------
// Purpose: sets up a reporting function
// Input  : *pReportFunction - 
//-----------------------------------------------------------------------------
void SocketReport( REPORTFUNCTION pReportFunction )
{
	g_SocketReport = pReportFunction;
}


//-----------------------------------------------------------------------------
// Purpose: Open a TCP socket & connect to a given server
// Input  : *pServerName - server name (text or ip)
//			port - port number of the server
// Output : HSOCKET
//-----------------------------------------------------------------------------
HSOCKET SocketOpen( const char *pServerName, int port )
{
	SOCKADDR_IN sockAddr;
	SOCKET s;

	memset(&sockAddr,0,sizeof(sockAddr));

	s = socket( AF_INET, SOCK_STREAM, 0 );

	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(pServerName);

	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost;
		lphost = gethostbyname(pServerName);
		if (lphost != NULL)
		{
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		}
		else
		{
			WSASetLastError(WSAEINVAL);
			return FALSE;
		}
	}

	sockAddr.sin_port = htons((u_short)port);

	if ( connect( s, (SOCKADDR *)&sockAddr, sizeof(sockAddr) ) == SOCKET_ERROR )
	{
//		printf("Socket error:%d\n", WSAGetLastError()) ;
		return NULL;
	}

	return (HSOCKET)s;
}


//-----------------------------------------------------------------------------
// Purpose: close the socket opened with SocketOpen()
// Input  : socket - 
//-----------------------------------------------------------------------------
void SocketClose( HSOCKET socket )
{
	SOCKET s = (SOCKET)socket;
	closesocket( s );
}


//-----------------------------------------------------------------------------
// Purpose: Write a string to the socket.  String is NULL terminated on input,
//			but terminator is NOT written to the socket
// Input  : socket - 
//			*pString - string to write
//-----------------------------------------------------------------------------
void SocketSendString( HSOCKET socket, const char *pString )
{
	if ( !pString )
		return;

	int len = (int)strlen( pString );

	if ( !len )
		return;

	if ( send( (SOCKET)socket, pString, len, 0 ) != SOCKET_ERROR )
	{
		if ( g_SocketReport )
		{
			g_SocketReport( socket, pString );
		}
	}
	else
	{
//		printf("Send failed\n");
	}
}


//-----------------------------------------------------------------------------
// Purpose: receive input from a socket until a certain string is received
//			ASSUME: socket data is all text
// Input  : socket - 
//			*pString - string to match, if NULL, just poll the socket once
//-----------------------------------------------------------------------------
void SocketWait( HSOCKET socket, const char *pString )
{
	char buf[1024];

	bool done = false;
	while ( !done )
	{
		int len = recv( (SOCKET)socket, buf, sizeof(buf)-1, 0 );
		buf[len] = 0;
		if ( g_SocketReport )
		{
			g_SocketReport( socket, buf );
		}
		if ( !pString || strstr( buf, pString ) )
			return;
	}
}

