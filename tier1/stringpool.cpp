//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
// $NoKeywords: $
//===========================================================================//

#include "convar.h"
#include "tier0/dbg.h"
#include "stringpool.h"
#include "tier1/strtools.h"
#include "generichash.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Comparison function for string sorted associative data structures
//-----------------------------------------------------------------------------

bool StrLessInsensitive( const char * const &pszLeft, const char * const &pszRight )
{
	return ( Q_stricmp( pszLeft, pszRight) < 0 );
}

bool StrLessSensitive( const char * const &pszLeft, const char * const &pszRight )
{
	return ( Q_strcmp( pszLeft, pszRight) < 0 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CStringPool::CStringPool( StringPoolCase_t caseSensitivity )
  : m_Strings( 32, 256, caseSensitivity == StringPoolCaseInsensitive ? StrLessInsensitive : StrLessSensitive )
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CStringPool::~CStringPool()
{
	FreeAll();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

unsigned int CStringPool::Count() const
{
	return m_Strings.Count();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char * CStringPool::Find( const char *pszValue )
{
	unsigned short i = m_Strings.Find(pszValue);
	if ( m_Strings.IsValidIndex(i) )
		return m_Strings[i];

	return NULL;
}

const char * CStringPool::Allocate( const char *pszValue )
{
	char	*pszNew;

	unsigned short i 	= m_Strings.Find(pszValue);
	bool		   bNew = (i == m_Strings.InvalidIndex());

	if ( !bNew )
		return m_Strings[i];

	pszNew = strdup( pszValue );
	m_Strings.Insert( pszNew );

	return pszNew;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CStringPool::FreeAll()
{
	unsigned short i = m_Strings.FirstInorder();
	while ( i != m_Strings.InvalidIndex() )
	{
		free( (void *)m_Strings[i] );
		i = m_Strings.NextInorder(i);
	}
	m_Strings.RemoveAll();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


#ifdef _DEBUG
CON_COMMAND( test_stringpool, "Tests the class CStringPool" )
{
	CStringPool pool;

	Assert(pool.Count() == 0);

	pool.Allocate("test");
	Assert(pool.Count() == 1);

	pool.Allocate("test");
	Assert(pool.Count() == 1);

	pool.Allocate("test2");
	Assert(pool.Count() == 2);

	Assert( pool.Find("test2") != NULL );
	Assert( pool.Find("TEST") != NULL );
	Assert( pool.Find("Test2") != NULL );
	Assert( pool.Find("test") != NULL );

	pool.FreeAll();
	Assert(pool.Count() == 0);

	Msg("Pass.");
}
#endif
