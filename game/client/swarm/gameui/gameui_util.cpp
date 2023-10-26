//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#include <stdarg.h>
#include "gameui_util.h"
#include "strtools.h"
#include "EngineInterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static int g_nGameUIActiveSplitscreenPlayerSlot = 0;

int GetGameUIActiveSplitScreenPlayerSlot()
{
	return g_nGameUIActiveSplitscreenPlayerSlot;
}

void SetGameUIActiveSplitScreenPlayerSlot( int nSlot )
{
	if ( g_nGameUIActiveSplitscreenPlayerSlot != nSlot )
	{
		g_nGameUIActiveSplitscreenPlayerSlot = nSlot;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Scans player names
//Passes the player name to be checked in a KeyValues pointer
//with the keyname "name"
// - replaces '&' with '&&' so they will draw in the scoreboard
// - replaces '#' at the start of the name with '*'
//-----------------------------------------------------------------------------

void GameUI_MakeSafeName( const char *oldName, char *newName, int newNameBufSize )
{
	if ( !oldName )
	{
		newName[0] = 0;
		return;
	}

	int newpos = 0;

	for( const char *p=oldName; *p != 0 && newpos < newNameBufSize-1; p++ )
	{
		//check for a '#' char at the beginning

		/*
		if( p == oldName && *p == '#' )
				{
					newName[newpos] = '*';
					newpos++;
				}
				else */

		if( *p == '%' )
		{
			// remove % chars
			newName[newpos] = '*';
			newpos++;
		}
		else if( *p == '&' )
		{
			//insert another & after this one
			if ( newpos+2 < newNameBufSize )
			{
				newName[newpos] = '&';
				newName[newpos+1] = '&';
				newpos+=2;
			}
		}
		else
		{
			newName[newpos] = *p;
			newpos++;
		}
	}
	newName[newpos] = 0;
}

//-----------------------------------------------------------------------------
// This version is simply used to make reading convars simpler.
// Writing convars isn't allowed in this mode
//-----------------------------------------------------------------------------
class CEmptyGameUIConVar : public ConVar
{
public:
	CEmptyGameUIConVar() : ConVar( "", "0" ) {}
	// Used for optimal read access
	virtual void SetValue( const char *pValue ) {}
	virtual void SetValue( float flValue ) {}
	virtual void SetValue( int nValue ) {}
	virtual const char *GetName( void ) const { return ""; }
	virtual bool IsFlagSet( int nFlags ) const { return false; }
};

static CEmptyGameUIConVar s_EmptyConVar;

// Helper for splitscreen ConVars
CGameUIConVarRef::CGameUIConVarRef( const char *pName )
{
	Init( pName, false );
}

CGameUIConVarRef::CGameUIConVarRef( const char *pName, bool bIgnoreMissing )
{
	Init( pName, bIgnoreMissing );
}

void CGameUIConVarRef::Init( const char *pName, bool bIgnoreMissing )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		cv_t &info = m_Info[ i ];
		char pchName[ 256 ];
		if ( i != 0 )
		{
			Q_snprintf( pchName, sizeof( pchName ), "%s%d", pName, i + 1 );
		}
		else
		{
			Q_strncpy( pchName, pName, sizeof( pchName ) );
		}

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}

	if ( !IsValid() )
	{
		static bool bFirst = true;
		if ( g_pCVar || bFirst )
		{
			if ( !bIgnoreMissing )
			{
				Warning( "CGameUIConVarRef %s doesn't point to an existing ConVar\n", pName );
			}
			bFirst = false;
		}
	}
}

CGameUIConVarRef::CGameUIConVarRef( IConVar *pConVar )
{
	cv_t &info = m_Info[ 0 ];
	info.m_pConVar = pConVar ? pConVar : &s_EmptyConVar;
	info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );

	for ( int i = 1; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		info = m_Info[ i ];
		char pchName[ 256 ];
		Q_snprintf( pchName, sizeof( pchName ), "%s%d", pConVar->GetName(), i + 1 );

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}
}

bool CGameUIConVarRef::IsValid() const
{
	return m_Info[ 0 ].m_pConVar != &s_EmptyConVar;
}

//-----------------------------------------------------------------------------

CGameUiSetActiveSplitScreenPlayerGuard::CGameUiSetActiveSplitScreenPlayerGuard( int slot )
{
	m_nSaveSlot = engine->GetActiveSplitScreenPlayerSlot();
	engine->SetActiveSplitScreenPlayerSlot( slot );
}

CGameUiSetActiveSplitScreenPlayerGuard::~CGameUiSetActiveSplitScreenPlayerGuard()
{
	engine->SetActiveSplitScreenPlayerSlot( m_nSaveSlot );
}

