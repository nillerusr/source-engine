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
	return ( V_stricmp( pszLeft, pszRight) < 0 );
}

bool StrLessSensitive( const char * const &pszLeft, const char * const &pszRight )
{
	return ( V_strcmp( pszLeft, pszRight) < 0 );
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


CCountedStringPool::CCountedStringPool( StringPoolCase_t caseSensitivity )
{
	MEM_ALLOC_CREDIT();
	m_HashTable.EnsureCount(HASH_TABLE_SIZE);

	for( int i = 0; i < m_HashTable.Count(); i++ )
	{
		m_HashTable[i] = INVALID_ELEMENT;		
	}

	m_FreeListStart = INVALID_ELEMENT;
	m_Elements.AddToTail();
	m_Elements[0].pString = NULL;
	m_Elements[0].nReferenceCount = 0;
	m_Elements[0].nNextElement = INVALID_ELEMENT;

	m_caseSensitivity = caseSensitivity;
}

CCountedStringPool::~CCountedStringPool()
{
	FreeAll();
}

void CCountedStringPool::FreeAll()
{
	int i;

	// Reset the hash table:
	for( i = 0; i < m_HashTable.Count(); i++ )
	{
		m_HashTable[i] = INVALID_ELEMENT;		
	}

	// Blow away the free list:
	m_FreeListStart = INVALID_ELEMENT;

	for( i = 0; i < m_Elements.Count(); i++ )
	{
		if( m_Elements[i].pString )
		{
			delete [] m_Elements[i].pString;
			m_Elements[i].pString = NULL;
			m_Elements[i].nReferenceCount = 0;
			m_Elements[i].nNextElement = INVALID_ELEMENT;
		}
	}

	// Remove all but the invalid element:
	m_Elements.RemoveAll();
	m_Elements.AddToTail();
	m_Elements[0].pString = NULL;
	m_Elements[0].nReferenceCount = 0;
	m_Elements[0].nNextElement = INVALID_ELEMENT;
}

unsigned CCountedStringPool::Hash( const char *pszKey )
{
	if ( m_caseSensitivity == StringPoolCaseInsensitive )
	{
		return HashStringCaseless( pszKey );
	}
	return HashString( pszKey ); 
}

unsigned short CCountedStringPool::FindStringHandle( const char* pIntrinsic )
{
	if( pIntrinsic == NULL )
		return INVALID_ELEMENT;

	unsigned short nHashBucketIndex = ( Hash( pIntrinsic ) %HASH_TABLE_SIZE);
	unsigned short nCurrentBucket = m_HashTable[ nHashBucketIndex ];

	// Does the bucket already exist?
	if( nCurrentBucket != INVALID_ELEMENT  )
	{
		for( ; nCurrentBucket != INVALID_ELEMENT  ; nCurrentBucket = m_Elements[nCurrentBucket].nNextElement )
		{
			if( !V_stricmp( pIntrinsic, m_Elements[nCurrentBucket].pString ) )
			{
				return nCurrentBucket;
			}
		}
	}
	
	return 0;

}

char* CCountedStringPool::FindString( const char* pIntrinsic )
{
	if( pIntrinsic == NULL )
		return NULL;

	// Yes, this will be NULL on failure.
	return m_Elements[FindStringHandle(pIntrinsic)].pString;
}

unsigned short CCountedStringPool::ReferenceStringHandle( const char* pIntrinsic )
{
	if( pIntrinsic == NULL )
		return INVALID_ELEMENT;

	unsigned short nHashBucketIndex = ( Hash( pIntrinsic ) % HASH_TABLE_SIZE);
	unsigned short nCurrentBucket =  m_HashTable[ nHashBucketIndex ];

	// Does the bucket already exist?
	if( nCurrentBucket != INVALID_ELEMENT )
	{
		for( ; nCurrentBucket != INVALID_ELEMENT ; nCurrentBucket = m_Elements[nCurrentBucket].nNextElement )
		{
			if( !V_stricmp( pIntrinsic, m_Elements[nCurrentBucket].pString ) )
			{
				// Anyone who hits 65k references is permanant
				if( m_Elements[nCurrentBucket].nReferenceCount < MAX_REFERENCE )
				{
					m_Elements[nCurrentBucket].nReferenceCount ++ ;
				}
				return nCurrentBucket;
			}
		}
	}

	if( m_FreeListStart != INVALID_ELEMENT )
	{
		nCurrentBucket = m_FreeListStart;
		m_FreeListStart = m_Elements[nCurrentBucket].nNextElement;
	}
	else
	{
		nCurrentBucket = m_Elements.AddToTail();
	}

	m_Elements[nCurrentBucket].nReferenceCount = 1;

	// Insert at the beginning of the bucket:
	m_Elements[nCurrentBucket].nNextElement = m_HashTable[ nHashBucketIndex ];
	m_HashTable[ nHashBucketIndex ] = nCurrentBucket;

	m_Elements[nCurrentBucket].pString = new char[V_strlen( pIntrinsic ) + 1];
	V_strcpy( m_Elements[nCurrentBucket].pString, pIntrinsic );
	
    return nCurrentBucket;
}


char* CCountedStringPool::ReferenceString( const char* pIntrinsic )
{
	if(!pIntrinsic)
		return NULL;

	return m_Elements[ReferenceStringHandle( pIntrinsic)].pString; 
}

void CCountedStringPool::DereferenceString( const char* pIntrinsic )
{
	// If we get a NULL pointer, just return
	if (!pIntrinsic)
		return;

	unsigned short nHashBucketIndex = (Hash( pIntrinsic ) % m_HashTable.Count());
	unsigned short nCurrentBucket =  m_HashTable[ nHashBucketIndex ];

	// If there isn't anything in the bucket, just return.
	if ( nCurrentBucket == INVALID_ELEMENT )
		return;

	for( unsigned short previous = INVALID_ELEMENT; nCurrentBucket != INVALID_ELEMENT ; nCurrentBucket = m_Elements[nCurrentBucket].nNextElement )
	{
		if( !V_stricmp( pIntrinsic, m_Elements[nCurrentBucket].pString ) )
		{
			// Anyone who hits 65k references is permanant
			if( m_Elements[nCurrentBucket].nReferenceCount < MAX_REFERENCE )
			{
				m_Elements[nCurrentBucket].nReferenceCount --;
			}

			if( m_Elements[nCurrentBucket].nReferenceCount == 0 )
			{
				if( previous == INVALID_ELEMENT )
				{
					m_HashTable[nHashBucketIndex] = m_Elements[nCurrentBucket].nNextElement;
				}
				else
				{
					m_Elements[previous].nNextElement = m_Elements[nCurrentBucket].nNextElement;
				}

				delete [] m_Elements[nCurrentBucket].pString;
				m_Elements[nCurrentBucket].pString = NULL;
				m_Elements[nCurrentBucket].nReferenceCount = 0;

				m_Elements[nCurrentBucket].nNextElement = m_FreeListStart;
				m_FreeListStart = nCurrentBucket;
				break;

			}
		}

		previous = nCurrentBucket;
	}
}

char* CCountedStringPool::HandleToString( unsigned short handle )
{
	return m_Elements[handle].pString;
}

void CCountedStringPool::SpewStrings()
{
	int i;
	for ( i = 0; i < m_Elements.Count(); i++ )
	{
		char* string;
		string = m_Elements[i].pString;
		Msg("String %d: ref:%d %s\n", i, m_Elements[i].nReferenceCount, string == NULL? "EMPTY - ok for slot zero only!" : string);
	}

	Msg("\n%d total counted strings.", m_Elements.Count());
}

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

#define STRING_POOL_VERSION		MAKEID( 'C', 'S', 'P', '1' )
#define MAX_STRING_SAVE			1024

bool CCountedStringPool::SaveToBuffer( CUtlBuffer &buffer )
{
	if ( m_Elements.Count() <= 1 )
	{
		// pool is empty, saving nothing
		// caller can check put position of buffer to detect
		return true;
	}

	// signature/version
	buffer.PutInt( STRING_POOL_VERSION );

	buffer.PutUnsignedShort( m_FreeListStart );

	buffer.PutInt( m_HashTable.Count() );
	for ( int i = 0; i < m_HashTable.Count(); i++ )
	{
		buffer.PutUnsignedShort( m_HashTable[i] );
	}

	buffer.PutInt( m_Elements.Count() );
	for ( int i = 1; i < m_Elements.Count(); i++ )
	{
		buffer.PutUnsignedShort( m_Elements[i].nNextElement );
		buffer.PutUnsignedChar( m_Elements[i].nReferenceCount );

		const char *pString = m_Elements[i].pString;
		if ( strlen( pString ) >= MAX_STRING_SAVE )
		{
			return false;
		}
		buffer.PutString( pString ? pString : "" );
	}

	return buffer.IsValid();
}

bool CCountedStringPool::RestoreFromBuffer( CUtlBuffer &buffer )
{
	int signature = buffer.GetInt();
	if ( signature != STRING_POOL_VERSION )
	{
		// wrong version
		return false;
	}

	FreeAll();

	m_FreeListStart = buffer.GetUnsignedShort();

	int hashCount = buffer.GetInt();
	m_HashTable.SetCount( hashCount );

	for ( int i = 0; i < hashCount; i++ )
	{
		m_HashTable[i] = buffer.GetUnsignedShort();
	}

	int tableCount = buffer.GetInt();
	if ( tableCount > 1 )
	{
		m_Elements.AddMultipleToTail( tableCount-1 );
	}

	char tempString[MAX_STRING_SAVE];
	for ( int i = 1; i < tableCount; i++ )
	{
		m_Elements[i].nNextElement = buffer.GetUnsignedShort();
		m_Elements[i].nReferenceCount = buffer.GetUnsignedChar();
		buffer.GetString( tempString, sizeof( tempString ) );
		m_Elements[i].pString = strdup( tempString );
	}

	return buffer.IsValid();
}
