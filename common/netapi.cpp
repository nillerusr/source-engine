//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "winsock.h"
typedef int socklen_t;
#elif POSIX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#else
#error "implement me"
#endif

#include "netapi.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>
#pragma warning(disable: 4706) // warning C4706: assignment within conditional expression

//-----------------------------------------------------------------------------
// Purpose: Implements INetAPI
//-----------------------------------------------------------------------------
class CNetAPI : public INetAPI
{
public:
	virtual void		NetAdrToSockAddr( netadr_t *a, struct sockaddr *s );
	virtual void		SockAddrToNetAdr( struct sockaddr *s, netadr_t *a );

	virtual char		*AdrToString( netadr_t *a );
	virtual bool		StringToAdr( const char *s, netadr_t *a );

	virtual void		GetSocketAddress( int socket, netadr_t *a );

	virtual bool		CompareAdr( netadr_t *a, netadr_t *b );

	virtual void	GetLocalIP(netadr_t *a);
};

// Expose interface
static CNetAPI g_NetAPI;
INetAPI *net = ( INetAPI * )&g_NetAPI;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *a - 
//			*s - 
//-----------------------------------------------------------------------------
void CNetAPI::NetAdrToSockAddr (netadr_t *a, struct sockaddr *s)
{
	memset (s, 0, sizeof(*s));

	if (a->type == NA_BROADCAST)
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if (a->type == NA_IP)
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
		((struct sockaddr_in *)s)->sin_port = a->port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*a - 
//-----------------------------------------------------------------------------
void CNetAPI::SockAddrToNetAdr( struct sockaddr *s, netadr_t *a )
{
	if (s->sa_family == AF_INET)
	{
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *a - 
// Output : char
//-----------------------------------------------------------------------------
char *CNetAPI::AdrToString( netadr_t *a )
{
	static	char	s[64];

	memset(s, 0, 64);

	if ( a )
	{
		if ( a->type == NA_LOOPBACK )
		{
			sprintf (s, "loopback");
		}
		else if ( a->type == NA_IP )
		{
			sprintf(s, "%i.%i.%i.%i:%i", a->ip[0], a->ip[1], a->ip[2], a->ip[3], ntohs( a->port ) );
		}
	}
	return s;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*sadr - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];
	struct sockaddr_in *p;
	
	memset (sadr, 0, sizeof(*sadr));
	
	p = ( struct sockaddr_in * )sadr;
	p->sin_family = AF_INET;
	p->sin_port = 0;

	strcpy (copy, s);

	// strip off a trailing :port if present
	for ( colon = copy ; *colon ; colon++ )
	{
		if (*colon == ':')
		{
			// terminate
			*colon = 0;
			// Start at next character
			p->sin_port = htons( ( short )atoi( colon + 1 ) );	
		}
	}
	
	// Numeric IP, no DNS
	if ( copy[0] >= '0' && copy[0] <= '9' && strstr( copy, "." ) )
	{
		*(int *)&p->sin_addr = inet_addr( copy );
	}
	else
	{
		// DNS it
		if ( !( h = gethostbyname( copy ) ) )
		{
			return false;
		}
		// Use first result
		*(int *)&p->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*a - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetAPI::StringToAdr( const char *s, netadr_t *a )
{
	struct sockaddr sadr;
	
	if ( !strcmp ( s, "localhost" ) )
	{
		memset ( a, 0, sizeof( *a ) );
		a->type = NA_LOOPBACK;
		return true;
	}

	if ( !StringToSockaddr (s, &sadr) )
	{
		return false;
	}
	
	SockAddrToNetAdr( &sadr, a );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Lookup the IP address for the specified IP socket
// Input  : socket - 
//			*a - 
//-----------------------------------------------------------------------------
void CNetAPI::GetSocketAddress( int socket, netadr_t *a )
{
	char	buff[512];
	struct sockaddr_in	address;
	int		namelen;
//	int     net_error = 0;

	memset( a, 0, sizeof( *a ) );
	gethostname(buff, 512);
	// Ensure that it doesn't overrun the buffer
	buff[512-1] = 0;

	StringToAdr(buff, a );
		
	namelen = sizeof(address);
	if ( getsockname( socket, (struct sockaddr *)&address, (socklen_t *)&namelen) == 0 )
	{
		a->port = address.sin_port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Full IP address compare
// Input  : *a - 
//			*b - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetAPI::CompareAdr( netadr_t *a, netadr_t *b )
{
	if ( a->type != b->type )
	{
		return false;
	}

	if ( a->type == NA_LOOPBACK )
	{
		return true;
	}

	if ( a->type == NA_IP &&
		 a->ip[0] == b->ip[0] && 
		 a->ip[1] == b->ip[1] && 
		 a->ip[2] == b->ip[2] && 
		 a->ip[3] == b->ip[3] && 
		 a->port == b->port )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Full IP address compare
// Input  : *a - 
//			*b - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

void CNetAPI::GetLocalIP(netadr_t *a)
{
	char s[64];

	if(!::gethostname(s,64))
	{
		struct hostent *localip = ::gethostbyname(s);
		if(localip)
		{
			a->type=NA_IP;
			a->port=0;
			memcpy(a->ip,localip->h_addr_list[0],4);
		}
	}
}
