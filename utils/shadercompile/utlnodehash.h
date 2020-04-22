//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: hashed intrusive linked list.
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#ifndef UTLNODEHASH_H
#define UTLNODEHASH_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlmemory.h"
#include "tier1/byteswap.h"
#include "tier1/utlintrusivelist.h"

#include <stdarg.h>

// to use this class, your list node class must have a Key() function defined which returns an
// integer type. May add this class to main utl tier when i'm happy w/ it.
template<class T, int HASHSIZE = 7907, class K = int > class CUtlNodeHash
{

	int m_nNumNodes;

public:

	CUtlIntrusiveDList<T> m_HashChains[HASHSIZE];

	CUtlNodeHash( void )
	{
		m_nNumNodes = 0;
	}


	T *FindByKey(K nMatchKey, int *pChainNumber = NULL)
	{
		unsigned int nChain=(unsigned int) nMatchKey ;
		nChain %= HASHSIZE;
		if ( pChainNumber )
			*( pChainNumber ) = nChain;
		for( T * pNode = m_HashChains[ nChain ].m_pHead; pNode; pNode = pNode->m_pNext )
			if ( pNode->Key() == nMatchKey )
				return pNode;
		return NULL;
	}

	void Add( T * pNode )
	{
		unsigned int nChain=(unsigned int) pNode->Key();
		nChain %= HASHSIZE;
		m_HashChains[ nChain ].AddToHead( pNode );
		m_nNumNodes++;
	}


	void Purge( void )
	{
		m_nNumNodes = 0;
		// delete all nodes
		for( int i=0; i < HASHSIZE; i++)
			m_HashChains[i].Purge();
	}

	int Count( void ) const
	{
		return m_nNumNodes;
	}

	void DeleteByKey( K nMatchKey )
	{
		int nChain;
		T *pSearch = FindByKey( nMatchKey, &nChain );
		if ( pSearch )
		{
			m_HashChains[ nChain ].RemoveNode( pSearch );
			m_nNumNodes--;
		}
	}

	~CUtlNodeHash( void )
	{
		// delete all lists
		Purge();
	}
};


#endif
