//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Pool of all per-level strings. Allocates memory for strings, 
//			consolodating duplicates. The memory is freed on behalf of clients
//			at level transition. Strings are of type string_t.
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMESTRINGPOOL_H
#define GAMESTRINGPOOL_H

#if defined( _WIN32 )
#pragma once
#endif

class IGameSystem;

//-----------------------------------------------------------------------------
// String allocation
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char *pszValue );
string_t FindPooledString( const char *pszValue );

#define AssertIsValidString( s )	AssertMsg( s == NULL_STRING || s == FindPooledString( STRING(s) ), "Invalid string " #s );
		 
//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
IGameSystem *GameStringSystem();

//-----------------------------------------------------------------------------
class CGameString
{
public:
	CGameString() : 
		m_iszString( NULL_STRING ), m_pszString( NULL ), m_iSerial( 0 ) 
	{

	} 

	CGameString( const char *pszString, bool bCopy = false )  : 
		m_iszString( NULL_STRING ), m_pszString( NULL ), m_iSerial( 0 ) 
	{ 
		Set( pszString, bCopy ); 
	}

	~CGameString() 
	{ 
		if ( m_bCopy ) 
			free( (void *)m_pszString ); 
	}

	void Set( const char *pszString, bool bCopy = false )
	{
		if ( m_bCopy && m_pszString )
			free( (void *)m_pszString );
		m_iszString = NULL_STRING;
		m_pszString = ( !bCopy ) ? pszString : strdup( pszString );
	}

	string_t Get() const
	{
		if ( m_iszString == NULL_STRING || m_iSerial != gm_iSerialNumber )
		{
			if ( !m_pszString )
				return NULL_STRING;

			m_iszString = AllocPooledString( m_pszString );
			m_iSerial = gm_iSerialNumber;
		}

		return m_iszString;
	}

	operator const char *() const
	{
		return ( m_pszString ) ? m_pszString : "";
	}

#ifndef NO_STRING_T
	operator string_t() const
	{
		return Get();
	}
#endif

	static void IncrementSerialNumber()
	{
		gm_iSerialNumber++;
	}

private:
	mutable string_t m_iszString;
	const char *m_pszString;
	mutable int m_iSerial;
	bool m_bCopy;


	static int gm_iSerialNumber;

};

#endif // GAMESTRINGPOOL_H
