//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

//#include "stdafx.h"
#ifdef _WIN32
#include "winlite.h"
#include "winsock.h"
#elif POSIX
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET int
#define LPSOCKADDR struct sockaddr *
#define SOCKADDR_IN struct sockaddr_in
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define closesocket close
#endif
#include "tier1/strtools.h"
#include "KeyValues.h"
#include "utlbuffer.h"
#include "tier1/checksum_crc.h"
#include "tier1/convar.h"
#include "cbase.h"
#include "cs_gamestats.h"
#include "cs_gamerules.h"
#include "cs_urlretrieveprices.h"

#if _DEBUG
#define WEEKLY_PRICE_URL "http://gamestats/weeklyprices.dat"
#else
#define WEEKLY_PRICE_URL "http://www.steampowered.com/stats/csmarket/weeklyprices.dat"
#endif


//-----------------------------------------------------------------------------
// Purpose: request a URL from connection
//-----------------------------------------------------------------------------
bool SendHTTPRequest( const char *pchRequestURL, SOCKET socketHTML )
{
	char szHeader[ MED_BUFFER_SIZE ];
	char szHostName[ SMALL_BUFFER_SIZE ];
	::gethostname( szHostName, sizeof(szHostName) );

	Q_snprintf( szHeader, sizeof(szHeader), "GET %s HTTP/1.0\r\n" \
		"Accept: */*\r\n" \
		"Accept-Language: en-us\r\n" \
		"User-Agent: Steam/3.0\r\n" \
		"Host: %s\r\n" \
		"\r\n",
		pchRequestURL, szHostName );

	return ::send( socketHTML, szHeader, Q_strlen(szHeader) + 1, 0 ) != SOCKET_ERROR ;
}


//-----------------------------------------------------------------------------
// Purpose: Given a previous HTTP request parse the response into a key values buffer
//-----------------------------------------------------------------------------
bool ParseHTTPResponse( SOCKET socketHTML, uint32 *unPageHash = NULL )
{
	char szHeaderBuf[ MED_BUFFER_SIZE ];
	char szBodyBuf[ MED_BUFFER_SIZE ];

	int dwRet = 0;
	bool bFinishedHeaderRead = false;
	int iRecvPosition = 0;
	int cCharsInLine = 0;

	// scan for the end of the header
	while ( !bFinishedHeaderRead && iRecvPosition < sizeof(szHeaderBuf) )
	{
		dwRet = ::recv( socketHTML, &szHeaderBuf[ iRecvPosition ] , 1, 0);
		if ( dwRet < 0 )
		{
			bFinishedHeaderRead = true;
		}

		switch( szHeaderBuf[ iRecvPosition ] )
		{
		case '\r':
			break;
		case '\n':
			if ( cCharsInLine == 0 )
				bFinishedHeaderRead = true;

			cCharsInLine = 0;
			break;
		default:
			cCharsInLine++;
			break;
		}

		iRecvPosition++;
	}

	CUtlBuffer buf;
	buf.SetBufferType( false, false );
	while( 1 )
	{
		dwRet = ::recv( socketHTML, szBodyBuf, sizeof(szBodyBuf)-1, 0);
		if ( dwRet <= 0 )
			break;

		buf.Put( szBodyBuf, sizeof(szBodyBuf)-1 );
	}

	weeklyprice_t weeklyprice;
	Q_memset( &weeklyprice, 0, sizeof( weeklyprice_t) );

	buf.Get( &weeklyprice, sizeof( weeklyprice_t ) );

	if ( weeklyprice.iVersion != PRICE_BLOB_VERSION )
	{
		Msg( "Incorrect price blob version! Update your server!\n" );
		return false;
	}

	CSGameRules()->AddPricesToTable( weeklyprice );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Given http url crack it into an address and the request part
//-----------------------------------------------------------------------------
bool ProcessURL( const char *pchURL, void *pSockAddrIn, char *pchRequest, int cchRequest ) 
{
	char rgchHost[ MAX_DNS_NAME ];
	char rgchRequest[ MED_BUFFER_SIZE ];
	uint16 iPort;

	if ( Q_strnicmp( pchURL, "http://", 7 ) != 0 )
	{
		Assert( !"http protocol only supported" );
		return false;
	}

	const char *pchColon = strchr( pchURL + 7, ':' );
	if ( pchColon )
	{
		Q_strncpy( rgchHost, pchURL + 7, pchColon - ( pchURL + 7 ) + 1 );
		const char *pchForwardSlash = strchr( pchColon + 1, '/' );
		if ( !pchForwardSlash )
			return false;
		Q_strncpy( rgchRequest, pchColon + 1, pchForwardSlash - ( pchColon + 1 ) + 1 );
		iPort = atoi( rgchRequest );
		Q_strncpy( rgchRequest, pchForwardSlash, ( pchURL + Q_strlen(pchURL) ) - pchForwardSlash + 1  );
	}
	else
	{
		const char *pchForwardSlash = strchr( pchURL + 7, '/' );
		if ( !pchForwardSlash )
			return false;

		Q_strncpy( rgchHost, pchURL + 7, pchForwardSlash - ( pchURL + 7 ) + 1 );
		iPort = 80;
		Q_strncpy( rgchRequest, pchForwardSlash, ( pchURL + Q_strlen(pchURL) ) - pchForwardSlash + 1 );
	}

	struct hostent *hp = NULL;
	if ( inet_addr( rgchHost ) == INADDR_NONE )
	{
		hp = gethostbyname( rgchHost );
	}
	else
	{
		uint32 addr = inet_addr( rgchHost );
		hp = gethostbyaddr( ( char* )&addr, sizeof( addr ), AF_INET );
	}

	if(  hp == NULL )
	{
		return false;
	}

	sockaddr_in &sockAddrIn = *((sockaddr_in *)pSockAddrIn);
	sockAddrIn.sin_addr.s_addr = *( ( unsigned long* )hp->h_addr );
	sockAddrIn.sin_family = AF_INET;
	sockAddrIn.sin_port = htons( iPort );

	Q_strncpy( pchRequest, rgchRequest, cchRequest );
	return true;
}

//networkstringtable

bool BlackMarket_DownloadPrices( void )
{
	char szRequest[ MED_BUFFER_SIZE ];
	sockaddr_in server;
	bool bConnected = false;

	if ( ProcessURL( WEEKLY_PRICE_URL, &server, szRequest, sizeof(szRequest) ) )
	{
		SOCKET socketHTML = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if ( socketHTML != INVALID_SOCKET)
		{
			int iRet = ::connect( socketHTML, (LPSOCKADDR)&server, sizeof(SOCKADDR_IN) );

			if ( !iRet )
			{
				bConnected = true;
				if ( SendHTTPRequest( szRequest, socketHTML ) )
				{
					uint32 unHash = 0;
					bool bRet = ParseHTTPResponse( socketHTML, &unHash );

					closesocket( socketHTML );

					return bRet;
				}
				else
				{
					return false;
				}
			}
			else
			{
				closesocket( socketHTML );
				return false;
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}
