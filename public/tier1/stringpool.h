//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef STRINGPOOL_H
#define STRINGPOOL_H

#if defined( _WIN32 )
#pragma once
#endif

#include "utlrbtree.h"
#include "utlvector.h"
#include "utlbuffer.h"
#include "generichash.h"

//-----------------------------------------------------------------------------
// Purpose: Allocates memory for strings, checking for duplicates first,
//			reusing exising strings if duplicate found.
//-----------------------------------------------------------------------------

enum StringPoolCase_t
{
	StringPoolCaseInsensitive,
	StringPoolCaseSensitive
};

class CStringPool
{
public:
	CStringPool( StringPoolCase_t caseSensitivity = StringPoolCaseInsensitive );
	~CStringPool();

	unsigned int Count() const;

	const char * Allocate( const char *pszValue );
	// This feature is deliberately not supported because it's pretty dangerous
	// given current uses of CStringPool, which assume they can copy string pointers without
	// any refcounts.
	//void Free( const char *pszValue );
	void FreeAll();

	// searches for a string already in the pool
	const char * Find( const char *pszValue );

protected:
	typedef CUtlRBTree<const char *, unsigned short> CStrSet;

	CStrSet m_Strings;
};

//-----------------------------------------------------------------------------
// Purpose: A reference counted string pool.  
//
// Elements are stored more efficiently than in the conventional string pool, 
// quicker to look up, and storage is tracked via reference counts.  
//
// At some point this should replace CStringPool
//-----------------------------------------------------------------------------
template<class T>
class CCountedStringPoolBase
{
public: // HACK, hash_item_t structure should not be public.

	struct hash_item_t
	{
		char*			pString;
		T				nNextElement;
		unsigned char	nReferenceCount;
		unsigned char	pad;
	};

	enum
	{
		INVALID_ELEMENT = 0,
		MAX_REFERENCE   = 0xFF,
		HASH_TABLE_SIZE = 1024
	};

	CUtlVector<T>				m_HashTable;	// Points to each element
	CUtlVector<hash_item_t>		m_Elements;
	T							m_FreeListStart;
	StringPoolCase_t 			m_caseSensitivity;

public:
	CCountedStringPoolBase( StringPoolCase_t caseSensitivity = StringPoolCaseInsensitive );
	virtual ~CCountedStringPoolBase();

	void			FreeAll();

	char			*FindString( const char* pIntrinsic ); 
	char			*ReferenceString( const char* pIntrinsic );
	void			DereferenceString( const char* pIntrinsic );

	// These are only reliable if there are less than 64k strings in your string pool
	T				FindStringHandle( const char* pIntrinsic ); 
	T				ReferenceStringHandle( const char* pIntrinsic );
	char			*HandleToString( T handle );
	void			SpewStrings();
	unsigned		Hash( const char *pszKey );

	bool			SaveToBuffer( CUtlBuffer &buffer );
	bool			RestoreFromBuffer( CUtlBuffer &buffer );

	// Debug helper method to validate that we didn't overflow
	void			VerifyNotOverflowed( unsigned int value );
};

typedef CCountedStringPoolBase<unsigned short> CCountedStringPool;

template<class T>
inline CCountedStringPoolBase<T>::CCountedStringPoolBase( StringPoolCase_t caseSensitivity )
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

template<class T>
inline CCountedStringPoolBase<T>::~CCountedStringPoolBase()
{
	FreeAll();
}

template<class T>
inline void CCountedStringPoolBase<T>::FreeAll()
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

template<class T>
inline unsigned CCountedStringPoolBase<T>::Hash( const char *pszKey )
{
	if ( m_caseSensitivity == StringPoolCaseInsensitive )
	{
		return HashStringCaseless( pszKey );
	}
	return HashString( pszKey ); 
}

template<class T>
inline T CCountedStringPoolBase<T>::FindStringHandle( const char* pIntrinsic )
{
	if( pIntrinsic == NULL )
		return INVALID_ELEMENT;

	T nHashBucketIndex = ( Hash( pIntrinsic ) %HASH_TABLE_SIZE);
	T nCurrentBucket = m_HashTable[ nHashBucketIndex ];

	// Does the bucket already exist?
	if( nCurrentBucket != INVALID_ELEMENT  )
	{
		for( ; nCurrentBucket != INVALID_ELEMENT  ; nCurrentBucket = m_Elements[nCurrentBucket].nNextElement )
		{
			if( !Q_stricmp( pIntrinsic, m_Elements[nCurrentBucket].pString ) )
			{
				return nCurrentBucket;
			}
		}
	}
	
	return 0;

}

template<class T>
inline char* CCountedStringPoolBase<T>::FindString( const char* pIntrinsic )
{
	if( pIntrinsic == NULL )
		return NULL;

	// Yes, this will be NULL on failure.
	return m_Elements[FindStringHandle(pIntrinsic)].pString;
}

template<class T>
inline T CCountedStringPoolBase<T>::ReferenceStringHandle( const char* pIntrinsic )
{
	if( pIntrinsic == NULL )
		return INVALID_ELEMENT;

	T nHashBucketIndex = ( Hash( pIntrinsic ) % HASH_TABLE_SIZE);
	T nCurrentBucket =  m_HashTable[ nHashBucketIndex ];

	// Does the bucket already exist?
	if( nCurrentBucket != INVALID_ELEMENT )
	{
		for( ; nCurrentBucket != INVALID_ELEMENT ; nCurrentBucket = m_Elements[nCurrentBucket].nNextElement )
		{
			if( !Q_stricmp( pIntrinsic, m_Elements[nCurrentBucket].pString ) )
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
		unsigned int newElement = m_Elements.AddToTail();
		VerifyNotOverflowed( newElement );
		nCurrentBucket = newElement;
	}

	m_Elements[nCurrentBucket].nReferenceCount = 1;

	// Insert at the beginning of the bucket:
	m_Elements[nCurrentBucket].nNextElement = m_HashTable[ nHashBucketIndex ];
	m_HashTable[ nHashBucketIndex ] = nCurrentBucket;

	m_Elements[nCurrentBucket].pString = new char[Q_strlen( pIntrinsic ) + 1];
	Q_strcpy( m_Elements[nCurrentBucket].pString, pIntrinsic );
	
    return nCurrentBucket;
}

template<>
inline void CCountedStringPoolBase<unsigned short>::VerifyNotOverflowed( unsigned int value ) { Assert( value < 0xffff ); }

template<>
inline void CCountedStringPoolBase<unsigned int>::VerifyNotOverflowed( unsigned int value ) {}

template<class T>
inline char* CCountedStringPoolBase<T>::ReferenceString( const char* pIntrinsic )
{
	if(!pIntrinsic)
		return NULL;

	return m_Elements[ReferenceStringHandle( pIntrinsic)].pString; 
}

template<class T>
inline void CCountedStringPoolBase<T>::DereferenceString( const char* pIntrinsic )
{
	// If we get a NULL pointer, just return
	if (!pIntrinsic)
		return;

	T nHashBucketIndex = (Hash( pIntrinsic ) % m_HashTable.Count());
	T nCurrentBucket =  m_HashTable[ nHashBucketIndex ];

	// If there isn't anything in the bucket, just return.
	if ( nCurrentBucket == INVALID_ELEMENT )
		return;

	for( T previous = INVALID_ELEMENT; nCurrentBucket != INVALID_ELEMENT ; nCurrentBucket = m_Elements[nCurrentBucket].nNextElement )
	{
		if( !Q_stricmp( pIntrinsic, m_Elements[nCurrentBucket].pString ) )
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

template<class T>
inline char* CCountedStringPoolBase<T>::HandleToString( T handle )
{
	return m_Elements[handle].pString;
}

template<class T>
inline void CCountedStringPoolBase<T>::SpewStrings()
{
	int i;
	for ( i = 0; i < m_Elements.Count(); i++ )
	{
		char* string = m_Elements[i].pString;

		Msg("String %d: ref:%d %s\n", i, m_Elements[i].nReferenceCount, string == NULL? "EMPTY - ok for slot zero only!" : string);
	}

	Msg("\n%d total counted strings.", m_Elements.Count());
}

#define STRING_POOL_VERSION		MAKEID( 'C', 'S', 'P', '1' )
#define MAX_STRING_SAVE			1024

template<>
inline bool CCountedStringPoolBase<unsigned short>::SaveToBuffer( CUtlBuffer &buffer )
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

template<>
inline bool CCountedStringPoolBase<unsigned short>::RestoreFromBuffer( CUtlBuffer &buffer )
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

template<>
inline bool CCountedStringPoolBase<unsigned int>::SaveToBuffer( CUtlBuffer &buffer )
{
	if ( m_Elements.Count() <= 1 )
	{
		// pool is empty, saving nothing
		// caller can check put position of buffer to detect
		return true;
	}

	// signature/version
	buffer.PutInt( STRING_POOL_VERSION );

	buffer.PutUnsignedInt( m_FreeListStart );

	buffer.PutInt( m_HashTable.Count() );
	for ( int i = 0; i < m_HashTable.Count(); i++ )
	{
		buffer.PutUnsignedInt( m_HashTable[i] );
	}

	buffer.PutInt( m_Elements.Count() );
	for ( int i = 1; i < m_Elements.Count(); i++ )
	{
		buffer.PutUnsignedInt( m_Elements[i].nNextElement );
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

template<>
inline bool CCountedStringPoolBase<unsigned int>::RestoreFromBuffer( CUtlBuffer &buffer )
{
	int signature = buffer.GetInt();
	if ( signature != STRING_POOL_VERSION )
	{
		// wrong version
		return false;
	}

	FreeAll();

	m_FreeListStart = buffer.GetUnsignedInt();

	int hashCount = buffer.GetInt();
	m_HashTable.SetCount( hashCount );

	for ( int i = 0; i < hashCount; i++ )
	{
		m_HashTable[i] = buffer.GetUnsignedInt();
	}

	int tableCount = buffer.GetInt();
	if ( tableCount > 1 )
	{
		m_Elements.AddMultipleToTail( tableCount-1 );
	}

	char tempString[MAX_STRING_SAVE];
	for ( int i = 1; i < tableCount; i++ )
	{
		m_Elements[i].nNextElement = buffer.GetUnsignedInt();
		m_Elements[i].nReferenceCount = buffer.GetUnsignedChar();
		buffer.GetString( tempString, sizeof( tempString ) );
		m_Elements[i].pString = strdup( tempString );
	}

	return buffer.IsValid();
}
#endif // STRINGPOOL_H
