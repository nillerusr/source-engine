//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NET_SYNCTAGS_H
#define NET_SYNCTAGS_H
#ifdef _WIN32
#pragma once
#endif

#ifdef _DEBUG

#include <bitbuf.h>

extern ConVar net_synctags;

// SyncTags are used as a debugging tool. If net_synctags is set to 1, then the tags
// are put into the bit streams and verified on the client.
inline void SyncTag_Write( bf_write *pBuf, const char *pTag )
{
	if ( net_synctags.GetInt() )
	{
		pBuf->WriteString( pTag );
	}
}

inline void SyncTag_Read( bf_read *pBuf, const char *pWantedTag )
{
	if ( net_synctags.GetInt() )
	{
		char testTag[512];
		pBuf->ReadString( testTag, sizeof( testTag ) );
		
		if ( stricmp( testTag, pWantedTag ) != 0 )
		{
			Error( "SyncTag_Read: out-of-sync at tag %s", pWantedTag );
		}
	}	
}

#else

	inline void SyncTag_Write( bf_write *pBuf, const char *pTag ) {}
	inline void SyncTag_Read( bf_read *pBuf, const char *pWantedTag ) {}

#endif




#endif // NET_SYNCTAGS_H
