//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "stringpool.h"
#include "igamesystem.h"
#include "gamestringpool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: The actual storage for pooled per-level strings
//-----------------------------------------------------------------------------
class CGameStringPool : public CStringPool,	public CBaseGameSystem
{
	virtual char const *Name() { return "CGameStringPool"; }

	virtual void LevelShutdownPostEntity() 
	{
		FreeAll();
		CGameString::IncrementSerialNumber();
	}

public:
	void Dump( void )
	{
		for ( int i = m_Strings.FirstInorder(); i != m_Strings.InvalidIndex(); i = m_Strings.NextInorder(i) )
		{
			DevMsg( "  %d (0x%p) : %s\n", i, m_Strings[i], m_Strings[i] );
		}
		DevMsg( "\n" );
		DevMsg( "Size:  %d items\n", m_Strings.Count() );
	}
};

static CGameStringPool g_GameStringPool;


//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
IGameSystem *GameStringSystem()
{
	return &g_GameStringPool;
}


//-----------------------------------------------------------------------------
// Purpose: The public accessor for the level-global pooled strings
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char * pszValue )
{
	if (pszValue && *pszValue)
		return MAKE_STRING( g_GameStringPool.Allocate( pszValue ) );
	return NULL_STRING;
}

string_t FindPooledString( const char *pszValue )
{
	return MAKE_STRING( g_GameStringPool.Find( pszValue ) );
}

int CGameString::gm_iSerialNumber = 1;

#ifndef CLIENT_DLL
//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CC_DumpGameStringTable( void )
{
	g_GameStringPool.Dump();
}
static ConCommand dumpgamestringtable("dumpgamestringtable", CC_DumpGameStringTable, "Dump the contents of the game string table to the console.", FCVAR_CHEAT);
#endif
