//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: A fast stack memory allocator that uses virtual memory if available
//
//===========================================================================//

#ifndef MEMSTACK_H
#define MEMSTACK_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier1/utlvector.h"

#if defined( _WIN32 ) || defined( _PS3 )
#define MEMSTACK_VIRTUAL_MEMORY_AVAILABLE
#endif

//-----------------------------------------------------------------------------

typedef unsigned MemoryStackMark_t;

class CMemoryStack
{
public:
	CMemoryStack();
	~CMemoryStack();

	bool Init( const char *pszAllocOwner, unsigned maxSize = 0, unsigned commitSize = 0, unsigned initialCommit = 0, unsigned alignment = 16 );
#ifdef _GAMECONSOLE
	bool InitPhysical( const char *pszAllocOwner, uint size, uint nBaseAddrAlignment, uint alignment = 16, uint32 nAdditionalFlags = 0 );
#endif
	void Term();

	int GetSize();
	int GetMaxSize();
	int	GetUsed();
	
	void *Alloc( unsigned bytes, bool bClear = false ) RESTRICT;

	MemoryStackMark_t GetCurrentAllocPoint();
	void FreeToAllocPoint( MemoryStackMark_t mark, bool bDecommit = true );
	void FreeAll( bool bDecommit = true );
	
	void Access( void **ppRegion, unsigned *pBytes );

	void PrintContents();

	void *GetBase();
	const void *GetBase() const {  return const_cast<CMemoryStack *>(this)->GetBase(); }

	bool CommitSize( int );

	void SetAllocOwner( const char *pszAllocOwner );

private:
	bool CommitTo( byte * ) RESTRICT;
	void RegisterAllocation();
	void RegisterDeallocation( bool bShouldSpew );

	byte *m_pNextAlloc;
	byte *m_pCommitLimit;
	byte *m_pAllocLimit;
	
	byte *m_pBase;
	bool m_bRegisteredAllocation;
	bool m_bPhysical;
	char *m_pszAllocOwner;

	unsigned m_maxSize;
	unsigned m_alignment;
#ifdef MEMSTACK_VIRTUAL_MEMORY_AVAILABLE
	unsigned m_commitSize;
	unsigned m_minCommit;
#endif
#if defined( MEMSTACK_VIRTUAL_MEMORY_AVAILABLE ) && defined( _PS3 )
	IVirtualMemorySection *m_pVirtualMemorySection;
#endif
};

//-------------------------------------

FORCEINLINE void *CMemoryStack::Alloc( unsigned bytes, bool bClear ) RESTRICT
{
	Assert( m_pBase );

	bytes = MAX( bytes, m_alignment );
	bytes = AlignValue( bytes, m_alignment );

	void *pResult = m_pNextAlloc;
	byte *pNextAlloc = m_pNextAlloc + bytes;

	if ( pNextAlloc > m_pCommitLimit )
	{
		if ( !CommitTo( pNextAlloc ) )
		{
			return NULL;
		}
	}

	if ( bClear )
	{
		memset( pResult, 0, bytes );
	}

	m_pNextAlloc = pNextAlloc;

	return pResult;
}

//-------------------------------------

inline bool CMemoryStack::CommitSize( int nBytes )
{
	if ( GetSize() != nBytes )
	{
		return CommitTo( m_pBase + nBytes );
	}
	return true;
}

//-------------------------------------

inline int CMemoryStack::GetMaxSize()
{ 
	return m_maxSize;
}

//-------------------------------------

inline int CMemoryStack::GetUsed() 
{ 
	return ( m_pNextAlloc - m_pBase ); 
}

//-------------------------------------

inline void *CMemoryStack::GetBase()
{
	return m_pBase;
}

//-------------------------------------

inline MemoryStackMark_t CMemoryStack::GetCurrentAllocPoint()
{
	return ( m_pNextAlloc - m_pBase );
}


//-----------------------------------------------------------------------------
// The CUtlMemoryStack class:
// A fixed memory class
//-----------------------------------------------------------------------------
template< typename T, typename I, size_t MAX_SIZE, size_t COMMIT_SIZE = 0, size_t INITIAL_COMMIT = 0 >
class CUtlMemoryStack
{
public:
	// constructor, destructor
	CUtlMemoryStack( int nGrowSize = 0, int nInitSize = 0 )	{ m_MemoryStack.Init( "CUtlMemoryStack", MAX_SIZE * sizeof(T), COMMIT_SIZE * sizeof(T), INITIAL_COMMIT * sizeof(T), 4 ); COMPILE_TIME_ASSERT( sizeof(T) % 4 == 0 );	}
	CUtlMemoryStack( T* pMemory, int numElements )			{ Assert( 0 ); 										}

	// Can we use this index?
	bool IsIdxValid( I i ) const							{ long x=i; return (x >= 0) && (x < m_nAllocated); }

	// Specify the invalid ('null') index that we'll only return on failure
	static const I INVALID_INDEX = ( I )-1; // For use with COMPILE_TIME_ASSERT
	static I InvalidIndex() { return INVALID_INDEX; }

	class Iterator_t
	{
		Iterator_t( I i ) : index( i ) {}
		I index;
		friend class CUtlMemoryStack<T,I,MAX_SIZE, COMMIT_SIZE, INITIAL_COMMIT>;
	public:
		bool operator==( const Iterator_t it ) const		{ return index == it.index; }
		bool operator!=( const Iterator_t it ) const		{ return index != it.index; }
	};
	Iterator_t First() const								{ return Iterator_t( m_nAllocated ? 0 : InvalidIndex() ); }
	Iterator_t Next( const Iterator_t &it ) const			{ return Iterator_t( it.index < m_nAllocated ? it.index + 1 : InvalidIndex() ); }
	I GetIndex( const Iterator_t &it ) const				{ return it.index; }
	bool IsIdxAfter( I i, const Iterator_t &it ) const		{ return i > it.index; }
	bool IsValidIterator( const Iterator_t &it ) const		{ long x=it.index; return x >= 0 && x < m_nAllocated; }
	Iterator_t InvalidIterator() const						{ return Iterator_t( InvalidIndex() ); }

	// Gets the base address
	T* Base()												{ return (T*)m_MemoryStack.GetBase(); }
	const T* Base() const									{ return (const T*)m_MemoryStack.GetBase(); }

	// element access
	T& operator[]( I i )									{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& operator[]( I i ) const						{ Assert( IsIdxValid(i) ); return Base()[i];	}
	T& Element( I i )										{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& Element( I i ) const							{ Assert( IsIdxValid(i) ); return Base()[i];	}

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T* pMemory, int numElements )	{ Assert( 0 ); }

	// Size
	int NumAllocated() const								{ return m_nAllocated; }
	int Count() const										{ return m_nAllocated; }

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( int num = 1 )								{ Assert( num > 0 ); m_nAllocated += num; m_MemoryStack.Alloc( num * sizeof(T) ); }

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num )							{ Assert( num <= MAX_SIZE ); if ( m_nAllocated < num ) Grow( num - m_nAllocated ); }

	// Memory deallocation
	void Purge()											{ m_MemoryStack.FreeAll(); m_nAllocated = 0; }

	// is the memory externally allocated?
	bool IsExternallyAllocated() const						{ return false; }

	// Set the size by which the memory grows
	void SetGrowSize( int size )							{ Assert( 0 ); }

	// Identify the owner of this memory stack's memory
	void SetAllocOwner( const char *pszAllocOwner )			{ m_MemoryStack.SetAllocOwner( pszAllocOwner ); }

private:
	CMemoryStack m_MemoryStack;
	int m_nAllocated;
};


#ifdef _X360
//-----------------------------------------------------------------------------
// A memory stack used for allocating physical memory on the 360
// Usage pattern anticipates we usually never go over the initial allocation
// When we do so, we're ok with slightly slower allocation
//-----------------------------------------------------------------------------
class CPhysicalMemoryStack
{
public:
	CPhysicalMemoryStack();
	~CPhysicalMemoryStack();

	// The physical memory stack is allocated in chunks. We will initially
	// allocate nInitChunkCount chunks, which will always be in memory.
	// When FreeAll() is called, it will free down to the initial chunk count
	// but not below it.
	bool	Init( size_t nChunkSizeInBytes, size_t nAlignment, int nInitialChunkCount, uint32 nAdditionalFlags );
	void	Term();

	size_t	GetSize() const;
	size_t	GetPeakUsed() const;
	size_t	GetUsed() const;
	size_t	GetFramePeakUsed() const;

	MemoryStackMark_t GetCurrentAllocPoint() const;
	void	FreeToAllocPoint( MemoryStackMark_t mark, bool bUnused = true ); // bUnused is for interface compat with CMemoryStack
	void	*Alloc( size_t nSizeInBytes, bool bClear = false ) RESTRICT;
	void	FreeAll( bool bUnused = true ); // bUnused is for interface compat with CMemoryStack

	void	PrintContents();

private:
	void *AllocFromOverflow( size_t nSizeInBytes );

	struct PhysicalChunk_t
	{
		uint8 *m_pBase;
		uint8 *m_pNextAlloc;
		uint8 *m_pAllocLimit;
	};

	PhysicalChunk_t m_InitialChunk;
	CUtlVector< PhysicalChunk_t > m_ExtraChunks; 
	size_t m_nUsage;
	size_t m_nFramePeakUsage;
	size_t m_nPeakUsage;
	size_t m_nAlignment;
	size_t m_nChunkSizeInBytes;
	int m_nFirstAvailableChunk;
	int m_nAdditionalFlags;
	PhysicalChunk_t *m_pLastAllocedChunk;
};

//-------------------------------------

FORCEINLINE void *CPhysicalMemoryStack::Alloc( size_t nSizeInBytes, bool bClear ) RESTRICT
{
	if ( nSizeInBytes )
	{
		nSizeInBytes = AlignValue( nSizeInBytes, m_nAlignment );
	}
	else
	{
		nSizeInBytes = m_nAlignment;
	}

	// Can't do an allocation bigger than the chunk size
	Assert( nSizeInBytes <= m_nChunkSizeInBytes );

	void *pResult = m_InitialChunk.m_pNextAlloc;
	uint8 *pNextAlloc = m_InitialChunk.m_pNextAlloc + nSizeInBytes;
	if ( pNextAlloc <= m_InitialChunk.m_pAllocLimit )
	{
		m_InitialChunk.m_pNextAlloc = pNextAlloc;
		m_pLastAllocedChunk = &m_InitialChunk;
	}
	else
	{
		pResult = AllocFromOverflow( nSizeInBytes );
	}

	m_nUsage += nSizeInBytes;
	m_nFramePeakUsage = MAX( m_nUsage, m_nFramePeakUsage ); 
	m_nPeakUsage = MAX( m_nUsage, m_nPeakUsage );

	if ( bClear )
	{
		memset( pResult, 0, nSizeInBytes );
	}

	return pResult;
}

//-------------------------------------

inline size_t CPhysicalMemoryStack::GetPeakUsed() const
{ 
	return m_nPeakUsage;
}

//-------------------------------------

inline size_t CPhysicalMemoryStack::GetUsed() const
{ 
	return m_nUsage; 
}

inline size_t CPhysicalMemoryStack::GetFramePeakUsed() const
{ 
	return m_nFramePeakUsage; 
}

inline MemoryStackMark_t CPhysicalMemoryStack::GetCurrentAllocPoint() const
{
	Assert( m_pLastAllocedChunk );
	return ( m_pLastAllocedChunk->m_pNextAlloc - m_pLastAllocedChunk->m_pBase );
}

#endif // _X360

#endif // MEMSTACK_H
