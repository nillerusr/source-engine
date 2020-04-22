//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "server_pch.h"
#include "net.h"
#include "sv_rcon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static redirect_t	sv_redirected;
static netadr_t		sv_redirectto;
static char			sv_redirect_buffer[ 4096 ]; // can't be any bigger because then we hit other static limits in the engine print funcs

//-----------------------------------------------------------------------------
// Purpose: Clears all remaining data from the redirection buffer.
//-----------------------------------------------------------------------------
void SV_RedirectFlush( void )
{
	static bool bInFlush = false; // recursion guard

	Assert( bInFlush == false );

	bInFlush = true;
	if ( sv_redirected == RD_PACKET )   // Print to remote address.
	{
		NET_OutOfBandPrintf( sv.m_Socket, sv_redirectto, "%c%s", A2A_PRINT, sv_redirect_buffer );
	}
	else if ( sv_redirected == RD_CLIENT )   // Send to client on message stream.
	{
		host_client->ClientPrintf( "%s", sv_redirect_buffer );
	}
	else if ( sv_redirected == RD_SOCKET )
	{
		RCONServer().FinishRedirect( sv_redirect_buffer, sv_redirectto );
	}
	
	// clear it
	sv_redirect_buffer[0] = 0;
	bInFlush = false;
}

//-----------------------------------------------------------------------------
// Purpose: Sents console printfs to remote client instead of to console
// Input  : rd - 
//			*addr - 
//-----------------------------------------------------------------------------
void SV_RedirectStart (redirect_t rd, const netadr_t *addr)
{
	sv_redirected = rd;
	sv_redirectto = *addr;
	sv_redirect_buffer[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Flushes buffers to network, and resets mode to inactive
//-----------------------------------------------------------------------------
void SV_RedirectEnd (void)
{
	SV_RedirectFlush ();
	sv_redirected = RD_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : len - 
//-----------------------------------------------------------------------------
void SV_RedirectCheckFlush( int len )
{
	if ( len + Q_strlen( sv_redirect_buffer ) > sizeof(sv_redirect_buffer) - 1)
	{
		SV_RedirectFlush();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : bool
//-----------------------------------------------------------------------------
bool SV_RedirectActive( void )
{	
	return ( sv_redirected != RD_NONE ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *txt - 
//-----------------------------------------------------------------------------
void SV_RedirectAddText( const char *txt )
{
	SV_RedirectCheckFlush( strlen( txt ) );
	Q_strncat( sv_redirect_buffer, (char *)txt, sizeof( sv_redirect_buffer ), COPY_ALL_CHARACTERS );
}