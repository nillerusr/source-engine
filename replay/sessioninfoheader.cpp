//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sessioninfoheader.h"
#include "dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

bool ReadSessionInfoHeader( const void *pBuf, int nBufSize, SessionInfoHeader_t &outHeader )
{
	if ( nBufSize < sizeof( SessionInfoHeader_t ) )
	{
		AssertMsg( 0, "Buffer size too small to read session info header" );
		return false;
	}

	// Read the header
	V_memcpy( &outHeader, pBuf, sizeof( SessionInfoHeader_t ) );

	return true;
}

//----------------------------------------------------------------------------------------
