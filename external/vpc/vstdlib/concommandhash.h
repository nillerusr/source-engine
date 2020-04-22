//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Special case hash table for console commands
//
// $NoKeywords: $
//
//===========================================================================//

#if !defined( CONCOMMANDHASH_H )
#define CONCOMMANDHASH_H
#ifdef _WIN32
#pragma once
#endif

#include "utllinkedlist.h"
#include "generichash.h"

// This is a hash table class very similar to the CUtlHashFast, but
// modified specifically so that we can look up ConCommandBases
// by string names without having to actually store those strings in
// the dictionary, and also iterate over all of them. 
// It uses separate chaining: each key hashes to a bucket, each
// bucket is a linked list of hashed commands. We store the hash of 
// the command's string name as well as its pointer, so we can do 
// the linked list march part of the Find() operation more quickly.
class CConCommandHash
{
public:
	typedef int CCommandHashHandle_t;
	typedef unsigned int HashKey_t;

	// Constructor/Deconstructor.
	CConCommandHash();
	~CConCommandHash();

	// Memory.
	void Purge( bool bReinitialize );

	// Invalid handle.
	static CCommandHashHandle_t InvalidHandle( void )	{ return ( CCommandHashHandle_t )~0; }
	inline bool IsValidHandle( CCommandHashHandle_t hHash ) const;

	/// Initialize.
	void Init( void ); // bucket count is hardcoded in enum below.

	/// Get hash value for a concommand
	static inline HashKey_t Hash( const ConCommandBase *cmd );

	// Size not available; count is meaningless for multilists.
	// int Count( void ) const;

	// Insertion.
	CCommandHashHandle_t Insert( ConCommandBase *cmd );
	CCommandHashHandle_t FastInsert( ConCommandBase *cmd );

	// Removal.
	void Remove( CCommandHashHandle_t hHash );
	void RemoveAll( void );

	// Retrieval.
	inline CCommandHashHandle_t Find( const char *name ) const;
	CCommandHashHandle_t Find( const ConCommandBase *cmd ) const;
	// A convenience version of Find that skips the handle part
	// and returns a pointer to a concommand, or NULL if none was found.
	inline ConCommandBase * FindPtr( const char *name ) const;

	inline ConCommandBase * &operator[]( CCommandHashHandle_t hHash );
	inline ConCommandBase *const &operator[]( CCommandHashHandle_t hHash ) const;

#ifdef _DEBUG
	// Dump a report to MSG
	void Report( void );
#endif

	// Iteration
	struct CCommandHashIterator_t
	{
		int bucket;
		CCommandHashHandle_t handle;

		CCommandHashIterator_t(int _bucket, const CCommandHashHandle_t &_handle) 
			: bucket(_bucket), handle(_handle) {};
		// inline operator UtlHashFastHandle_t() const { return handle; };
	};
	inline CCommandHashIterator_t First() const;
	inline CCommandHashIterator_t Next( const CCommandHashIterator_t &hHash ) const;
	inline bool IsValidIterator( const CCommandHashIterator_t &iter ) const;
	inline ConCommandBase * &operator[]( const CCommandHashIterator_t &iter ) { return (*this)[iter.handle];  }
	inline ConCommandBase * const &operator[]( const CCommandHashIterator_t &iter ) const { return (*this)[iter.handle];  }
private:
	// a find func where we've already computed the hash for the string.
	// (hidden private in case we decide to invent a custom string hash func
	//  for this class)
	CCommandHashHandle_t Find( const char *name, HashKey_t hash) const;

protected:
	enum 
	{
		kNUM_BUCKETS = 256,
		kBUCKETMASK  = kNUM_BUCKETS - 1,
	};

	struct HashEntry_t
	{
		HashKey_t m_uiKey;
		ConCommandBase *m_Data;

		HashEntry_t(unsigned int _hash, ConCommandBase * _cmd)
			: m_uiKey(_hash), m_Data(_cmd) {};

		HashEntry_t(){};
	};

	typedef CUtlFixedLinkedList<HashEntry_t> datapool_t;

	CUtlVector<CCommandHashHandle_t>	m_aBuckets;
	datapool_t							m_aDataPool;
};

inline bool CConCommandHash::IsValidHandle( CCommandHashHandle_t hHash ) const
{
	return m_aDataPool.IsValidIndex(hHash);
}


inline CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name ) const
{
	return Find( name, HashStringCaseless(name) );
}

inline ConCommandBase * &CConCommandHash::operator[]( CCommandHashHandle_t hHash )
{
	return ( m_aDataPool[hHash].m_Data );
}

inline ConCommandBase *const &CConCommandHash::operator[]( CCommandHashHandle_t hHash ) const
{
	return ( m_aDataPool[hHash].m_Data );
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the index of the first element
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashIterator_t CConCommandHash::First() const
{
	// walk through the buckets to find the first one that has some data
	int bucketCount = m_aBuckets.Count();
	const CCommandHashHandle_t invalidIndex = m_aDataPool.InvalidIndex();
	for ( int bucket = 0 ; bucket < bucketCount ; ++bucket )
	{
		CCommandHashHandle_t iElement = m_aBuckets[bucket]; // get the head of the bucket
		if ( iElement != invalidIndex )
			return CCommandHashIterator_t( bucket, iElement );
	}

	// if we are down here, the list is empty 
	return CCommandHashIterator_t( -1, invalidIndex );
}

//-----------------------------------------------------------------------------
// Purpose: For iterating over the whole hash, return the next element after
// the param one. Or an invalid iterator.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashIterator_t 
CConCommandHash::Next( const CConCommandHash::CCommandHashIterator_t &iter ) const
{
	// look for the next entry in the current bucket
	CCommandHashHandle_t next = m_aDataPool.Next(iter.handle);
	const CCommandHashHandle_t invalidIndex = m_aDataPool.InvalidIndex();
	if ( next != invalidIndex )
	{
		// this bucket still has more elements in it
		return CCommandHashIterator_t(iter.bucket, next);
	}

	// otherwise look for the next bucket with data
	int bucketCount = m_aBuckets.Count();
	for ( int bucket = iter.bucket+1 ; bucket < bucketCount ; ++bucket )
	{
		CCommandHashHandle_t next = m_aBuckets[bucket]; // get the head of the bucket
		if (next != invalidIndex)
			return CCommandHashIterator_t( bucket, next );
	}

	// if we're here, there's no more data to be had
	return CCommandHashIterator_t(-1, invalidIndex);
}

bool CConCommandHash::IsValidIterator( const CCommandHashIterator_t &iter ) const
{
	return ( (iter.bucket >= 0) && (m_aDataPool.IsValidIndex(iter.handle)) );
}

inline CConCommandHash::HashKey_t CConCommandHash::Hash( const ConCommandBase *cmd )
{
	return HashStringCaseless( cmd->GetName() );
}

inline ConCommandBase * CConCommandHash::FindPtr( const char *name ) const
{
	CCommandHashHandle_t handle = Find(name);
	if (handle == InvalidHandle())
	{
		return NULL;
	}
	else
	{
		return (*this)[handle];
	}
}

#endif
