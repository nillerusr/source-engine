//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GAMEUI_UTIL_H
#define GAMEUI_UTIL_H
#ifdef _WIN32
#pragma once
#endif

char	*VarArgs( const char *format, ... );

// Set by the player who "owns" the gameui/settings/etc.
void SetGameUIActiveSplitScreenPlayerSlot( int nSlot );
int GetGameUIActiveSplitScreenPlayerSlot();

#include "tier1/convar.h"

void GameUI_MakeSafeName( const char *oldName, char *newName, int newNameBufSize );

//-----------------------------------------------------------------------------
// Useful for game ui since game ui has a single active "splitscreen" owner and since
//  it can gracefully handle non-FCVAR_SS vars without code changes required.
//-----------------------------------------------------------------------------
class CGameUIConVarRef
{
public:
	CGameUIConVarRef( const char *pName );
	CGameUIConVarRef( const char *pName, bool bIgnoreMissing );
	CGameUIConVarRef( IConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;

	// Get/Set value
	float GetFloat() const;
	int GetInt() const;
	bool GetBool() const { return !!GetInt(); }
	const char *GetString() const;

	void SetValue( const char *pValue );
	void SetValue( float flValue );
	void SetValue( int nValue );
	void SetValue( bool bValue );

	const char *GetName() const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

protected:
	int		GetActiveSplitScreenPlayerSlot() const;
private:
	struct cv_t
	{
		IConVar *m_pConVar;
		ConVar *m_pConVarState;
	};

	cv_t	m_Info[ MAX_SPLITSCREEN_CLIENTS ];
};

// In GAMUI we should never use the regular ConVarRef
#define ConVarRef CGameUIConVarRef

FORCEINLINE int CGameUIConVarRef::GetActiveSplitScreenPlayerSlot() const
{
	return GetGameUIActiveSplitScreenPlayerSlot();
}

//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
FORCEINLINE bool CGameUIConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_Info[ 0 ].m_pConVar->IsFlagSet( nFlags ) != 0 );
}

FORCEINLINE const char *CGameUIConVarRef::GetName() const
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	return m_Info[ nSlot ].m_pConVar->GetName();
}

FORCEINLINE const char *CGameUIConVarRef::GetBaseName() const
{
	return m_Info[ 0 ].m_pConVar->GetBaseName();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
FORCEINLINE float CGameUIConVarRef::GetFloat() const
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	return m_Info[ nSlot ].m_pConVarState->GetRawValue().m_fValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
FORCEINLINE int CGameUIConVarRef::GetInt() const 
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	return m_Info[ nSlot ].m_pConVarState->GetRawValue().m_nValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
FORCEINLINE const char *CGameUIConVarRef::GetString() const 
{
	Assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	int nSlot = GetActiveSplitScreenPlayerSlot();
	return m_Info[ nSlot ].m_pConVarState->GetRawValue().m_pszString;
}


FORCEINLINE void CGameUIConVarRef::SetValue( const char *pValue )
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	m_Info[ nSlot ].m_pConVar->SetValue( pValue );
}

FORCEINLINE void CGameUIConVarRef::SetValue( float flValue )
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	m_Info[ nSlot ].m_pConVar->SetValue( flValue );
}

FORCEINLINE void CGameUIConVarRef::SetValue( int nValue )
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	m_Info[ nSlot ].m_pConVar->SetValue( nValue );
}

FORCEINLINE void CGameUIConVarRef::SetValue( bool bValue )
{
	int nSlot = GetActiveSplitScreenPlayerSlot();
	m_Info[ nSlot ].m_pConVar->SetValue( bValue ? 1 : 0 );
}

FORCEINLINE const char *CGameUIConVarRef::GetDefault() const
{
	return m_Info[ 0 ].m_pConVarState->GetDefault();
}

//-----------------------------------------------------------------------------

class CGameUiSetActiveSplitScreenPlayerGuard
{
public:
	explicit CGameUiSetActiveSplitScreenPlayerGuard( int slot );
	~CGameUiSetActiveSplitScreenPlayerGuard();
private:
	int	 m_nSaveSlot;
};

#define GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )	CGameUiSetActiveSplitScreenPlayerGuard g_SSGuard( slot );

#endif // GAMEUI_UTIL_H
