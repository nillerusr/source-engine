//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//===========================================================================//

#ifndef MEMPOOL_H
#define MEMPOOL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/memalloc.h"
#include "tier0/tslist.h"
#include "tier0/platform.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"

//-----------------------------------------------------------------------------
// Purpose: Optimized pool memory allocator
//-----------------------------------------------------------------------------

typedef void (*MemoryPoolReportFunc_t)( char const* pMsg, ... );

class CUtlMemoryPool
{
public:
	// Ways the memory pool can grow when it needs to make a new blob.
	enum MemoryPoolGrowType_t
	{
		GROW_NONE=0,		// Don't allow new blobs.
		GROW_FAST=1,		// New blob size is numElements * (i+1)  (ie: the blocks it allocates
							// get larger and larger each time it allocates one).
		GROW_SLOW=2			// New blob size is numElements.
	};

				CUtlMemoryPool( int blockSize, int numElements, int growMode = GROW_FAST, const char *pszAllocOwner = NULL, int nAlignment = 0 );
				~CUtlMemoryPool();

	void*		Alloc();	// Allocate the element size you specified in the constructor.
	void*		Alloc( size_t amount );
	void*		AllocZero();	// Allocate the element size you specified in the constructor, zero the memory before construction
	void*		AllocZero( size_t amount );
	void		Free(void *pMem);
	
	// Frees everything
	void		Clear();

	// Error reporting... 
	static void SetErrorReportFunc( MemoryPoolReportFunc_t func );

	// returns number of allocated blocks
	int Count() const		{ return m_BlocksAllocated; }
	int PeakCount() const	{ return m_PeakAlloc; }
	int BlockSize() const	{ return m_BlockSize; }
	int Size() const		{ return m_NumBlobs * m_BlocksPerBlob * m_BlockSize; }

	bool		IsAllocationWithinPool( void *pMem ) const;

protected:
	class CBlob
	{
	public:
		CBlob	*m_pPrev, *m_pNext;
		int		m_NumBytes;		// Number of bytes in this blob.
		char	m_Data[1];
		char	m_Padding[3]; // to int align the struct
	};

	// Resets the pool
	void		Init();
	void		AddNewBlob();
	void		ReportLeaks();

	int			m_BlockSize;
	int			m_BlocksPerBlob;

	int			m_GrowMode;	// GROW_ enum.

	// FIXME: Change m_ppMemBlob into a growable array?
	void			*m_pHeadOfFreeList;
	int				m_BlocksAllocated;
	int				m_PeakAlloc;
	unsigned short	m_nAlignment;
	unsigned short	m_NumBlobs;
	const char *	m_pszAllocOwner;
	// CBlob could be not a multiple of 4 bytes so stuff it at the end here to keep us otherwise aligned
	CBlob			m_BlobHead;

	static MemoryPoolReportFunc_t g_ReportFunc;
};


//-----------------------------------------------------------------------------
// Multi-thread/Thread Safe Memory Class
//-----------------------------------------------------------------------------
class CMemoryPoolMT : public CUtlMemoryPool
{
public:
	CMemoryPoolMT( int blockSize, int numElements, int growMode = GROW_FAST, const char *pszAllocOwner = NULL, int nAlignment = 0) : CUtlMemoryPool( blockSize, numElements, growMode, pszAllocOwner, nAlignment ) {}


	void*		Alloc()	{ AUTO_LOCK( m_mutex ); return CUtlMemoryPool::Alloc(); }
	void*		Alloc( size_t amount )	{ AUTO_LOCK( m_mutex ); return CUtlMemoryPool::Alloc( amount ); }
	void*		AllocZero()	{ AUTO_LOCK( m_mutex ); return CUtlMemoryPool::AllocZero(); }	
	void*		AllocZero( size_t amount )	{ AUTO_LOCK( m_mutex ); return CUtlMemoryPool::AllocZero( amount ); }
	void		Free(void *pMem) { AUTO_LOCK( m_mutex ); CUtlMemoryPool::Free( pMem ); }

	// Frees everything
	void		Clear() { AUTO_LOCK( m_mutex ); return CUtlMemoryPool::Clear(); }
private:
	CThreadFastMutex m_mutex; // @TODO: Rework to use tslist (toml 7/6/2007)
};


//-----------------------------------------------------------------------------
// Wrapper macro to make an allocator that returns particular typed allocations
// and construction and destruction of objects.
//-----------------------------------------------------------------------------
template< class T >
class CClassMemoryPool : public CUtlMemoryPool
{
public:
	CClassMemoryPool(int numElements, int growMode = GROW_FAST, int nAlignment = 0 ) :
		CUtlMemoryPool( sizeof(T), numElements, growMode, MEM_ALLOC_CLASSNAME(T), nAlignment ) {}

	T*		Alloc();
	T*		AllocZero();
	void	Free( T *pMem );

	void	Clear();
};

//-----------------------------------------------------------------------------
// Specialized pool for aligned data management (e.g., Xbox textures)
//-----------------------------------------------------------------------------
template <int ITEM_SIZE, int ALIGNMENT, int CHUNK_SIZE, class CAllocator, bool GROWMODE = false, int COMPACT_THRESHOLD = 4 >
class CAlignedMemPool
{
	enum
	{
		BLOCK_SIZE = COMPILETIME_MAX( ALIGN_VALUE( ITEM_SIZE, ALIGNMENT ), 8 ),
	};

public:
	CAlignedMemPool();

	void *Alloc();
	void Free( void *p );

	static int __cdecl CompareChunk( void * const *ppLeft, void * const *ppRight );
	void Compact();

	int NumTotal()			{ AUTO_LOCK( m_mutex ); return m_Chunks.Count() * ( CHUNK_SIZE / BLOCK_SIZE ); }
	int NumAllocated()		{ AUTO_LOCK( m_mutex ); return NumTotal() - m_nFree; }
	int NumFree()			{ AUTO_LOCK( m_mutex ); return m_nFree; }

	int BytesTotal()		{ AUTO_LOCK( m_mutex ); return NumTotal() * BLOCK_SIZE; }
	int BytesAllocated()	{ AUTO_LOCK( m_mutex ); return NumAllocated() * BLOCK_SIZE; }
	int BytesFree()			{ AUTO_LOCK( m_mutex ); return NumFree() * BLOCK_SIZE; }

	int ItemSize()			{ return ITEM_SIZE; }
	int BlockSize()			{ return BLOCK_SIZE; }
	int ChunkSize()			{ return CHUNK_SIZE; }

private:
	struct FreeBlock_t
	{
		FreeBlock_t *pNext;
		byte		reserved[ BLOCK_SIZE - sizeof( FreeBlock_t *) ];
	};

	CUtlVector<void *>	m_Chunks;		// Chunks are tracked outside blocks (unlike CUtlMemoryPool) to simplify alignment issues
	FreeBlock_t *		m_pFirstFree;
	int					m_nFree;
	CAllocator			m_Allocator;
	double				m_TimeLastCompact;

	CThreadFastMutex	m_mutex;
};

//-----------------------------------------------------------------------------
// Pool variant using standard allocation
//-----------------------------------------------------------------------------
template <typename T, int nInitialCount = 0, bool bDefCreateNewIfEmpty = true >
class CObjectPool
{
public:
	CObjectPool()
	{
		int i = nInitialCount;
		while ( i-- > 0 )
		{
			m_AvailableObjects.PushItem( new T );
		}
	}

	~CObjectPool()
	{
		Purge();
	}

	int NumAvailable()
	{
		return m_AvailableObjects.Count();
	}

	void Purge()
	{
		T *p = NULL;
		while ( m_AvailableObjects.PopItem( &p ) )
		{
			delete p;
		}
	}

	T *GetObject( bool bCreateNewIfEmpty = bDefCreateNewIfEmpty )
	{
		T *p = NULL;
		if ( !m_AvailableObjects.PopItem( &p )  )
		{
			p = ( bCreateNewIfEmpty ) ? new T : NULL;
		}
		return p;
	}

	void PutObject( T *p )
	{
		m_AvailableObjects.PushItem( p );
	}

private:
	CTSList<T *> m_AvailableObjects;
};

//-----------------------------------------------------------------------------
// Fixed budget pool with overflow to malloc
//-----------------------------------------------------------------------------
template <size_t PROVIDED_ITEM_SIZE, int ITEM_COUNT>
class CFixedBudgetMemoryPool
{
public:
	CFixedBudgetMemoryPool()
	{
		m_pBase = m_pLimit = 0;
		COMPILE_TIME_ASSERT( ITEM_SIZE % 4 == 0 );
	}

	bool Owns( void *p )
	{
		return ( p >= m_pBase && p < m_pLimit );
	}

	void *Alloc()
	{
		MEM_ALLOC_CREDIT_CLASS();
#ifndef USE_MEM_DEBUG
		if ( !m_pBase )
		{
			LOCAL_THREAD_LOCK();
			if ( !m_pBase )
			{
				byte *pMemory = m_pBase = (byte *)malloc( ITEM_COUNT * ITEM_SIZE );
				m_pLimit = m_pBase + ( ITEM_COUNT * ITEM_SIZE );

				for ( int i = 0; i < ITEM_COUNT; i++ )
				{
					m_freeList.Push( (TSLNodeBase_t *)pMemory );
					pMemory += ITEM_SIZE;
				}
			}
		}

		void *p = m_freeList.Pop();
		if ( p )
			return p;
#endif
		return malloc( ITEM_SIZE );
	}

	void Free( void *p )
	{
#ifndef USE_MEM_DEBUG
		if ( Owns( p ) )
			m_freeList.Push( (TSLNodeBase_t *)p );
		else
#endif
			free( p );
	}

	void Clear()
	{
#ifndef USE_MEM_DEBUG
		if ( m_pBase )
		{
			free( m_pBase );
		}
		m_pBase = m_pLimit = 0;
		Construct( &m_freeList );
#endif
	}

	bool IsEmpty()
	{
#ifndef USE_MEM_DEBUG
		if ( m_pBase && m_freeList.Count() != ITEM_COUNT )
			return false;
#endif
		return true;
	}

	enum
	{
		ITEM_SIZE = ALIGN_VALUE( PROVIDED_ITEM_SIZE, TSLIST_NODE_ALIGNMENT )
	};

	CTSListBase m_freeList;
	byte *m_pBase;
	byte *m_pLimit;
};

#define BIND_TO_FIXED_BUDGET_POOL( poolName )									\
	inline void* operator new( size_t size ) { return poolName.Alloc(); }   \
	inline void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine ) { return poolName.Alloc(); }   \
	inline void  operator delete( void* p ) { poolName.Free(p); }		\
	inline void  operator delete( void* p, int nBlockUse, const char *pFileName, int nLine ) { poolName.Free(p); }

//-----------------------------------------------------------------------------


template< class T >
inline T* CClassMemoryPool<T>::Alloc()
{
	T *pRet;

	{
	MEM_ALLOC_CREDIT_CLASS();
	pRet = (T*)CUtlMemoryPool::Alloc();
	}

	if ( pRet )
	{
		Construct( pRet );
	}
	return pRet;
}

template< class T >
inline T* CClassMemoryPool<T>::AllocZero()
{
	T *pRet;

	{
	MEM_ALLOC_CREDIT_CLASS();
	pRet = (T*)CUtlMemoryPool::AllocZero();
	}

	if ( pRet )
	{
		Construct( pRet );
	}
	return pRet;
}

template< class T >
inline void CClassMemoryPool<T>::Free(T *pMem)
{
	if ( pMem )
	{
		Destruct( pMem );
	}

	CUtlMemoryPool::Free( pMem );
}

template< class T >
inline void CClassMemoryPool<T>::Clear()
{
	CUtlRBTree<void *> freeBlocks;
	SetDefLessFunc( freeBlocks );

	void *pCurFree = m_pHeadOfFreeList;
	while ( pCurFree != NULL )
	{
		freeBlocks.Insert( pCurFree );
		pCurFree = *((void**)pCurFree);
	}

	for( CBlob *pCur=m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur=pCur->m_pNext )
	{
		T *p = (T *)pCur->m_Data;
		T *pLimit = (T *)(pCur->m_Data + pCur->m_NumBytes);
		while ( p < pLimit )
		{
			if ( freeBlocks.Find( p ) == freeBlocks.InvalidIndex() )
			{
				Destruct( p );
			}
			p++;
		}
	}

	CUtlMemoryPool::Clear();
}


//-----------------------------------------------------------------------------
// Macros that make it simple to make a class use a fixed-size allocator
// Put DECLARE_FIXEDSIZE_ALLOCATOR in the private section of a class,
// Put DEFINE_FIXEDSIZE_ALLOCATOR in the CPP file
//-----------------------------------------------------------------------------
#define DECLARE_FIXEDSIZE_ALLOCATOR( _class )									\
	public:																		\
		inline void* operator new( size_t size ) { MEM_ALLOC_CREDIT_(#_class " pool"); return s_Allocator.Alloc(size); }   \
		inline void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine ) { MEM_ALLOC_CREDIT_(#_class " pool"); return s_Allocator.Alloc(size); }   \
		inline void  operator delete( void* p ) { s_Allocator.Free(p); }		\
		inline void  operator delete( void* p, int nBlockUse, const char *pFileName, int nLine ) { s_Allocator.Free(p); }   \
	private:																		\
		static   CUtlMemoryPool   s_Allocator
    
#define DEFINE_FIXEDSIZE_ALLOCATOR( _class, _initsize, _grow )					\
	CUtlMemoryPool   _class::s_Allocator(sizeof(_class), _initsize, _grow, #_class " pool")

#define DEFINE_FIXEDSIZE_ALLOCATOR_ALIGNED( _class, _initsize, _grow, _alignment )		\
	CUtlMemoryPool   _class::s_Allocator(sizeof(_class), _initsize, _grow, #_class " pool", _alignment )

#define DECLARE_FIXEDSIZE_ALLOCATOR_MT( _class )									\
	public:																		\
	   inline void* operator new( size_t size ) { MEM_ALLOC_CREDIT_(#_class " pool"); return s_Allocator.Alloc(size); }   \
	   inline void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine ) { MEM_ALLOC_CREDIT_(#_class " pool"); return s_Allocator.Alloc(size); }   \
	   inline void  operator delete( void* p ) { s_Allocator.Free(p); }		\
	   inline void  operator delete( void* p, int nBlockUse, const char *pFileName, int nLine ) { s_Allocator.Free(p); }   \
	private:																		\
		static   CMemoryPoolMT   s_Allocator

#define DEFINE_FIXEDSIZE_ALLOCATOR_MT( _class, _initsize, _grow )					\
	CMemoryPoolMT   _class::s_Allocator(sizeof(_class), _initsize, _grow, #_class " pool")

//-----------------------------------------------------------------------------
// Macros that make it simple to make a class use a fixed-size allocator
// This version allows us to use a memory pool which is externally defined...
// Put DECLARE_FIXEDSIZE_ALLOCATOR_EXTERNAL in the private section of a class,
// Put DEFINE_FIXEDSIZE_ALLOCATOR_EXTERNAL in the CPP file
//-----------------------------------------------------------------------------

#define DECLARE_FIXEDSIZE_ALLOCATOR_EXTERNAL( _class )							\
   public:																		\
      inline void* operator new( size_t size )  { MEM_ALLOC_CREDIT_(#_class " pool"); return s_pAllocator->Alloc(size); }   \
      inline void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine )  { MEM_ALLOC_CREDIT_(#_class " pool"); return s_pAllocator->Alloc(size); }   \
      inline void  operator delete( void* p )   { s_pAllocator->Free(p); }		\
   private:																		\
      static   CUtlMemoryPool*   s_pAllocator

#define DEFINE_FIXEDSIZE_ALLOCATOR_EXTERNAL( _class, _allocator )				\
   CUtlMemoryPool*   _class::s_pAllocator = _allocator


template <int ITEM_SIZE, int ALIGNMENT, int CHUNK_SIZE, class CAllocator, bool GROWMODE, int COMPACT_THRESHOLD >
inline CAlignedMemPool<ITEM_SIZE, ALIGNMENT, CHUNK_SIZE, CAllocator, GROWMODE, COMPACT_THRESHOLD>::CAlignedMemPool()
  : m_pFirstFree( 0 ),
	m_nFree( 0 ),
	m_TimeLastCompact( 0 )
{
	COMPILE_TIME_ASSERT( sizeof( FreeBlock_t ) >= BLOCK_SIZE );
	COMPILE_TIME_ASSERT( ALIGN_VALUE( sizeof( FreeBlock_t ), ALIGNMENT ) == sizeof( FreeBlock_t ) );
}

template <int ITEM_SIZE, int ALIGNMENT, int CHUNK_SIZE, class CAllocator, bool GROWMODE, int COMPACT_THRESHOLD >
inline void *CAlignedMemPool<ITEM_SIZE, ALIGNMENT, CHUNK_SIZE, CAllocator, GROWMODE, COMPACT_THRESHOLD>::Alloc()
{
	AUTO_LOCK( m_mutex );

	if ( !m_pFirstFree )
	{
		if ( !GROWMODE && m_Chunks.Count() )
		{
			return NULL;
		}

		FreeBlock_t *pNew = (FreeBlock_t *)m_Allocator.Alloc( CHUNK_SIZE );
		Assert( (unsigned)pNew % ALIGNMENT == 0 );
		m_Chunks.AddToTail( pNew );
		m_nFree = CHUNK_SIZE / BLOCK_SIZE;
		m_pFirstFree = pNew;
		for ( int i = 0; i < m_nFree - 1; i++ )
		{
			pNew->pNext = pNew + 1;
			pNew++;
		}
		pNew->pNext = NULL;
	}

	void *p = m_pFirstFree;
	m_pFirstFree = m_pFirstFree->pNext;
	m_nFree--;

	return p;
}

template <int ITEM_SIZE, int ALIGNMENT, int CHUNK_SIZE, class CAllocator, bool GROWMODE, int COMPACT_THRESHOLD >
inline void CAlignedMemPool<ITEM_SIZE, ALIGNMENT, CHUNK_SIZE, CAllocator, GROWMODE, COMPACT_THRESHOLD>::Free( void *p )
{
	AUTO_LOCK( m_mutex ); 

	// Insertion sort to encourage allocation clusters in chunks
	FreeBlock_t *pFree = ((FreeBlock_t *)p);
	FreeBlock_t *pCur = m_pFirstFree;
	FreeBlock_t *pPrev = NULL;

	while ( pCur && pFree > pCur )
	{
		pPrev = pCur;
		pCur = pCur->pNext;
	}

	pFree->pNext = pCur;

	if ( pPrev )
	{
		pPrev->pNext = pFree;
	}
	else
	{
		m_pFirstFree = pFree;
	}
	m_nFree++;

	if ( m_nFree >= ( CHUNK_SIZE / BLOCK_SIZE ) * COMPACT_THRESHOLD )
	{
		double time = Plat_FloatTime();
		double compactTime = ( m_nFree >= ( CHUNK_SIZE / BLOCK_SIZE ) * COMPACT_THRESHOLD * 4 ) ? 15.0 : 30.0;
		if ( m_TimeLastCompact > time || m_TimeLastCompact + compactTime < time )
		{
			Compact();
			m_TimeLastCompact = time;
		}
	}
}

template <int ITEM_SIZE, int ALIGNMENT, int CHUNK_SIZE, class CAllocator, bool GROWMODE, int COMPACT_THRESHOLD >
inline int __cdecl CAlignedMemPool<ITEM_SIZE, ALIGNMENT, CHUNK_SIZE, CAllocator, GROWMODE, COMPACT_THRESHOLD>::CompareChunk( void * const *ppLeft, void * const *ppRight )
{
	return ((unsigned)*ppLeft) - ((unsigned)*ppRight);
}

template <int ITEM_SIZE, int ALIGNMENT, int CHUNK_SIZE, class CAllocator, bool GROWMODE, int COMPACT_THRESHOLD >
inline void CAlignedMemPool<ITEM_SIZE, ALIGNMENT, CHUNK_SIZE, CAllocator, GROWMODE, COMPACT_THRESHOLD>::Compact()
{
	FreeBlock_t *pCur = m_pFirstFree;
	FreeBlock_t *pPrev = NULL;

	m_Chunks.Sort( CompareChunk );

#ifdef VALIDATE_ALIGNED_MEM_POOL
	{
		FreeBlock_t *p = m_pFirstFree;
		while ( p )
		{
			if ( p->pNext && p > p->pNext )
			{
				__asm { int 3 }
			}
			p = p->pNext;
		}

		for ( int i = 0; i < m_Chunks.Count(); i++ )
		{
			if ( i + 1 < m_Chunks.Count() )
			{
				if ( m_Chunks[i] > m_Chunks[i + 1] )
				{
					__asm { int 3 }
				}
			}
		}
	}
#endif

	int i;

	for ( i = 0; i < m_Chunks.Count(); i++ )
	{
		int nBlocksPerChunk = CHUNK_SIZE / BLOCK_SIZE;
		FreeBlock_t *pChunkLimit = ((FreeBlock_t *)m_Chunks[i]) + nBlocksPerChunk;
		int nFromChunk = 0;
		if ( pCur == m_Chunks[i] )
		{
			FreeBlock_t *pFirst = pCur;
			while ( pCur && pCur >= m_Chunks[i] && pCur < pChunkLimit )
			{
				pCur = pCur->pNext;
				nFromChunk++;
			}
			pCur = pFirst;

		}

		while ( pCur && pCur >= m_Chunks[i] && pCur < pChunkLimit )
		{
			if ( nFromChunk != nBlocksPerChunk )
			{
				if ( pPrev )
				{
					pPrev->pNext = pCur;
				}
				else
				{
					m_pFirstFree = pCur;
				}
				pPrev = pCur;
			}
			else if ( pPrev )
			{
				pPrev->pNext = NULL;
			}
			else
			{
				m_pFirstFree = NULL;
			}

			pCur = pCur->pNext;
		}

		if ( nFromChunk == nBlocksPerChunk )
		{
			m_Allocator.Free( m_Chunks[i] );
			m_nFree -= nBlocksPerChunk;
			m_Chunks[i] = 0;
		}
	}

	for ( i = m_Chunks.Count() - 1; i >= 0 ; i-- )
	{
		if ( !m_Chunks[i] )
		{
			m_Chunks.FastRemove( i );
		}
	}
}

#endif // MEMPOOL_H
