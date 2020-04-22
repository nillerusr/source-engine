//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Code for the CEconGameAccount object
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "econ_game_account_server.h"

using namespace GCSDK;

#ifdef GC_DLL
//---------------------------------------------------------------------------------
// Purpose: 
//---------------------------------------------------------------------------------
IMPLEMENT_CLASS_MEMPOOL( CEconGameServerAccount, 100, UTLMEMORYPOOL_GROW_SLOW );

void GameServerAccount_GenerateIdentityToken( char* pIdentityToken, uint32 unMaxChars )
{
	static const char s_ValidChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890./+!$%^-_+?<>()&~:";
	const int nLastValidIndex = ARRAYSIZE(s_ValidChars) - 2; // last = size - 1, minus another one for null terminator

	// create a randomized token
	for ( uint32 i = 0; i < unMaxChars - 1; ++i )
	{
		pIdentityToken[i] = s_ValidChars[ RandomInt( 0, nLastValidIndex ) ];
	}
	pIdentityToken[unMaxChars - 1] = 0;
}

//---------------------------------------------------------------------------------
// Purpose: Selective account-level data for game servers
//---------------------------------------------------------------------------------
IMPLEMENT_CLASS_MEMPOOL( CEconGameAccountForGameServers, 10 * 1000, UTLMEMORYPOOL_GROW_SLOW );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
