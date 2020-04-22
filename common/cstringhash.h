//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTRINGHASH_H
#define CSTRINGHASH_H
#pragma once

#include "string.h"

#define STRING_HASH_TABLE_SIZE 701

template <class T> class CStringHash
{
public:
	CStringHash() 
	{
		memset( m_HashTable, 0, sizeof( StringHashNode_t * ) * STRING_HASH_TABLE_SIZE );
	}
	~CStringHash()
	{
		int i;

		for( i = 0; i < STRING_HASH_TABLE_SIZE; i++ )
		{
			StringHashNode_t *curEntry;
			curEntry = m_HashTable[i];
			if( curEntry )
			{
				StringHashNode_t *next;
				next = curEntry->next;
				delete curEntry;
				curEntry = next;
			}
		}
	}
	// return false if it already exists
	// there can only be one entry for each string.
	bool Insert( const char *string, T val )
	{
		unsigned int hashID = HashString( string );
		StringHashNode_t *newEntry;
		if( !m_HashTable[hashID] )
		{
			// first on at this hashID
			// fixme: need to make the allocation function configurable.
			newEntry = m_HashTable[hashID] = new StringHashNode_t;
			newEntry->next = NULL;
		}
		else
		{
			StringHashNode_t *curEntry;
			curEntry = m_HashTable[hashID];
			while( curEntry )
			{
				if( stricmp( curEntry->string, string ) == 0 )
				{
					// replace the data at the current entry with the enw data.
					curEntry->data = val;
					return false;
				}
				curEntry = curEntry->next;
			}
			newEntry = new StringHashNode_t;
			newEntry->next = m_HashTable[hashID];
			m_HashTable[hashID] = newEntry;
		}
		int len = strlen( string ) + 1;
		newEntry->string = new char[len];
		Q_strncpy( newEntry->string, string, len );
		newEntry->data = val;
		return true;
	}

	T Find( const char *string )
	{
		int hashID = HashString( string );
		StringHashNode_t *curEntry;
		curEntry = m_HashTable[hashID];
		while( curEntry )
		{
			if( stricmp( curEntry->string, string ) == 0 )
			{
				return curEntry->data;
			}
			curEntry = curEntry->next;
		}
		return NULL;
	}

private:
	unsigned int HashString( const char *string )
	{
		const char *s = string;
		unsigned int result = 0;

		while( *s )
		{
			result += tolower( ( int )*s ) * 6029;
			result *= 5749;
			s++;
		}
		return result % STRING_HASH_TABLE_SIZE;
	}
	typedef struct StringHashNode_s
	{
		char *string;
		T data;
		struct StringHashNode_s *next;
	} StringHashNode_t;
	StringHashNode_t *m_HashTable[STRING_HASH_TABLE_SIZE];
};

#endif // CSTRINGHASH_H
