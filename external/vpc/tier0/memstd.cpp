//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"


#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)


//#include <malloc.h>

#include <algorithm>

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "mem_helpers.h"
#include "memstd.h"
#include "tier0/stacktools.h"
#include "tier0/minidump.h"
#ifdef _X360
#include "xbox/xbox_console.h"
#endif

#ifdef _PS3
#include "memoverride_ps3.h"
#endif

#ifndef _WIN32
#define IsDebuggerPresent() false
#endif

#ifdef USE_LIGHT_MEM_DEBUG
#undef USE_MEM_DEBUG
#pragma message("*** USE_LIGHT_MEM_DEBUG is ON ***")
#pragma optimize( "", off )
#endif

#define DEF_REGION 0

#if defined( _WIN32 ) || defined( _PS3 )
#define USE_DLMALLOC
#define MEMALLOC_SEGMENT_MIXED
#define MBH_SIZE_MB ( 45 + MBYTES_STEAM_MBH_USAGE )
//#define MEMALLOC_REGIONS
#endif // _WIN32 || _PS3

#ifndef USE_DLMALLOC
#ifdef _PS3
#define malloc_internal( region, bytes ) (g_pMemOverrideRawCrtFns->pfn_malloc)(bytes)
#define malloc_aligned_internal( region, bytes, align ) (g_pMemOverrideRawCrtFns->pfn_memalign)(align, bytes)
#define realloc_internal (g_pMemOverrideRawCrtFns->pfn_realloc)
#define realloc_aligned_internal (g_pMemOverrideRawCrtFns->pfn_reallocalign)
#define free_internal (g_pMemOverrideRawCrtFns->pfn_free)
#define msize_internal (g_pMemOverrideRawCrtFns->pfn_malloc_usable_size)
#define compact_internal() (0)
#define heapstats_internal(p) (void)(0)
#else // _PS3
#define malloc_internal( region, bytes) malloc(bytes)
#define malloc_aligned_internal( region, bytes, align ) memalign(align, bytes)
#define realloc_internal realloc
#define realloc_aligned_internal realloc
#define free_internal free
#ifdef POSIX
#define msize_internal malloc_usable_size
#else  // POSIX
#define msize_internal _msize
#endif // POSIX
#define compact_internal() (0)
#define heapstats_internal(p) (void)(0)
#endif // _PS3
#else // USE_DLMALLOC
#define MSPACES 1
#include "dlmalloc/malloc-2.8.3.h"

void *g_AllocRegions[] = 
{
#ifndef MEMALLOC_REGIONS
#ifdef MEMALLOC_SEGMENT_MIXED
	create_mspace( 0, 1 ), // unified
	create_mspace( MBH_SIZE_MB*1024*1024, 1 ),
#else
	create_mspace( 100*1024*1024, 1 ),
#endif
#else  // MEMALLOC_REGIONS
	// @TODO: per DLL regions didn't work out very well. flux of usage left too much overhead. need to try lifetime-based management [6/9/2009 tom]
	create_mspace( 82*1024*1024, 1 ), // unified
#endif // MEMALLOC_REGIONS
};

#ifndef MEMALLOC_REGIONS
#ifndef MEMALLOC_SEGMENT_MIXED
#define SelectRegion( region, bytes ) 0
#else
// NOTE: this split is designed to force the 'large block' heap to ONLY perform virtual allocs (see
//       DEFAULT_MMAP_THRESHOLD in malloc.cpp), to avoid ANY fragmentation or waste in an internal arena
#define REGION_SPLIT (256*1024)
#define SelectRegion( region, bytes ) g_AllocRegions[bytes < REGION_SPLIT]
#endif
#else  // MEMALLOC_REGIONS
#define SelectRegion( region, bytes ) g_AllocRegions[region]
#endif // MEMALLOC_REGIONS

#define malloc_internal( region, bytes ) mspace_malloc(SelectRegion(region,bytes), bytes)
#define malloc_aligned_internal( region, bytes, align ) mspace_memalign(SelectRegion(region,bytes), align, bytes)
FORCEINLINE void *realloc_aligned_internal( void *mem, size_t bytes, size_t align )
{
	// TODO: implement realloc_aligned inside dlmalloc (requires splitting realloc's existing
	//       'grow in-place' code into a new function, then call that w/ alloc_align/copy/free on failure)
	byte *newMem = (byte *)dlrealloc( mem, bytes );
	if ( ((size_t)newMem&(align-1)) == 0 )
		return newMem;
	// realloc broke alignment...
	byte *fallback = (byte *)malloc_aligned_internal( DEF_REGION, bytes, align );
	if ( !fallback )
		return NULL;
	memcpy( fallback, newMem, bytes );
	dlfree( newMem );
	return fallback;
}

inline size_t compact_internal()
{
	size_t start = 0, end = 0;

	for ( int i = 0; i < ARRAYSIZE(g_AllocRegions); i++ )
	{
		start += mspace_footprint( g_AllocRegions[i] );
		mspace_trim( g_AllocRegions[i], 0 );
		end += mspace_footprint( g_AllocRegions[i] );
	}
	
	return ( start - end );
}

inline void heapstats_internal( FILE *pFile )
{
	// @TODO: improve this presentation, as a table [6/1/2009 tom]
	char buf[1024];
	for ( int i = 0; i < ARRAYSIZE( g_AllocRegions ); i++ )
	{
		struct mallinfo info = mspace_mallinfo(      g_AllocRegions[ i ] );
		size_t footPrint     = mspace_footprint(     g_AllocRegions[ i ] );
		size_t maxFootPrint  = mspace_max_footprint( g_AllocRegions[ i ] );
		_snprintf( buf, sizeof(buf),
			"\ndlmalloc mspace %d (%s)\n"
				"     %d:footprint     -%10d (total space used by the mspace)\n"
				"     %d:footprint_max -%10d (maximum total space used by the mspace)\n"
				"     %d:arena         -%10d (non-mmapped space allocated from system)\n"
				"     %d:ordblks       -%10d (number of free chunks)\n"
				"     %d:hblkhd        -%10d (space in mmapped regions)\n"
				"     %d:usmblks       -%10d (maximum total allocated space)\n"
				"     %d:uordblks      -%10d (total allocated space)\n"
				"     %d:fordblks      -%10d (total free space)\n"
				"     %d:keepcost      -%10d (releasable (via malloc_trim) space)\n",
				i, i?"medium-block":"large-block", i,footPrint, i,maxFootPrint, i,info.arena, i,info.ordblks, i,info.hblkhd, i,info.usmblks, i,info.uordblks, i,info.fordblks, i,info.keepcost );
		if ( pFile )
			fprintf( pFile, "%s", buf );
		else
			Msg( "%s", buf );
	}
}

#define realloc_internal dlrealloc
#define free_internal dlfree
#define msize_internal dlmalloc_usable_size
#endif // USE_DLMALLOC

#ifdef TIME_ALLOC
CAverageCycleCounter g_MallocCounter;
CAverageCycleCounter g_ReallocCounter;
CAverageCycleCounter g_FreeCounter;

#define PrintOne( name ) \
	Msg("%-48s: %6.4f avg (%8.1f total, %7.3f peak, %5d iters)\n",  \
		#name, \
		g_##name##Counter.GetAverageMilliseconds(), \
		g_##name##Counter.GetTotalMilliseconds(), \
		g_##name##Counter.GetPeakMilliseconds(), \
		g_##name##Counter.GetIters() ); \
	memset( &g_##name##Counter, 0, sizeof(g_##name##Counter) )

void PrintAllocTimes()
{
	PrintOne( Malloc );
	PrintOne( Realloc );
	PrintOne( Free );
}

#define PROFILE_ALLOC(name) CAverageTimeMarker name##_ATM( &g_##name##Counter )

#else  // TIME_ALLOC
#define PROFILE_ALLOC( name ) ((void)0)
#define PrintAllocTimes() ((void)0)
#endif // TIME_ALLOC

#if _MSC_VER < 1400 && defined( MSVC ) && !defined(_STATIC_LINKED) && (defined(_DEBUG) || defined(USE_MEM_DEBUG))
void *operator new( unsigned int nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return ::operator new( nSize );
}

void *operator new[] ( unsigned int nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return ::operator new[]( nSize );
}
#endif

#include "mem_impl_type.h"
#if MEM_IMPL_TYPE_STD

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
#pragma warning( disable:4074 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( compiler )

#if MEM_SBH_ENABLED
CSmallBlockPool< CStdMemAlloc::CFixedAllocator< MBYTES_PRIMARY_SBH, true> >::SharedData_t CSmallBlockPool< CStdMemAlloc::CFixedAllocator< MBYTES_PRIMARY_SBH, true> >::gm_SharedData CONSTRUCT_EARLY;
#ifdef MEMALLOC_USE_SECONDARY_SBH
CSmallBlockPool< CStdMemAlloc::CFixedAllocator< MBYTES_SECONDARY_SBH, false> >::SharedData_t CSmallBlockPool< CStdMemAlloc::CFixedAllocator< MBYTES_SECONDARY_SBH, false> >::gm_SharedData CONSTRUCT_EARLY;
#endif
#ifndef MEMALLOC_NO_FALLBACK
CSmallBlockPool< CStdMemAlloc::CVirtualAllocator >::SharedData_t CSmallBlockPool< CStdMemAlloc::CVirtualAllocator >::gm_SharedData CONSTRUCT_EARLY;
#endif
#endif // MEM_SBH_ENABLED

static CStdMemAlloc s_StdMemAlloc CONSTRUCT_EARLY;

#ifdef _PS3

MemOverrideRawCrtFunctions_t *g_pMemOverrideRawCrtFns;
IMemAlloc *g_pMemAllocInternalPS3 = &s_StdMemAlloc;
PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3_IMPL

#else // !_PS3

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc = &s_StdMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_StdMemAlloc;
#endif

#endif // _PS3

CStdMemAlloc::CStdMemAlloc()
:	m_pfnFailHandler( DefaultFailHandler ),
	m_sMemoryAllocFailed( (size_t)0 ),
	m_bInCompact( false )
{
#ifdef _PS3
	g_pMemAllocInternalPS3 = &s_StdMemAlloc;
	PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3.m_pMemAllocCached = &s_StdMemAlloc;
	malloc_managed_size mms;
	mms.current_inuse_size = 0x12345678;
	mms.current_system_size = 0x09ABCDEF;
	mms.max_system_size = reinterpret_cast< size_t >( this );
	int iResult = malloc_stats( &mms );
	g_pMemOverrideRawCrtFns = reinterpret_cast< MemOverrideRawCrtFunctions_t * >( iResult );
#endif
}

#if MEM_SBH_ENABLED
//-----------------------------------------------------------------------------
// Small block heap (multi-pool)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <typename T>
inline T MemAlign( T val, unsigned alignment )
{
	return (T)( ( (unsigned)val + alignment - 1 ) & ~( alignment - 1 ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

template <typename CAllocator>
void CSmallBlockPool<CAllocator>::Init( unsigned nBlockSize )
{
	SharedData_t *pSharedData = GetSharedData();
	if ( !pSharedData->m_pBase )
	{
		pSharedData->m_pBase = pSharedData->m_Allocator.AllocatePoolMemory();
		pSharedData->m_pLimit = pSharedData->m_pBase + CAllocator::TOTAL_BYTES;
		pSharedData->m_pNextBlock = pSharedData->m_pBase;
	}

	if ( !( nBlockSize % MIN_SBH_ALIGN == 0 && nBlockSize >= MIN_SBH_BLOCK && nBlockSize >= sizeof(TSLNodeBase_t) ) )
		DebuggerBreak();

	m_nBlockSize = nBlockSize;
	m_pNextAlloc = NULL;
	m_nCommittedPages = 0;
}

template <typename CAllocator>
size_t CSmallBlockPool<CAllocator>::GetBlockSize()
{
	return m_nBlockSize;
}

// Define VALIDATE_SBH_FREE_LIST to a given block size to validate that pool's freelist (it'll crash on the next alloc/free after the list is corrupted)
// NOTE: this may affect perf more than USE_LIGHT_MEM_DEBUG
//#define VALIDATE_SBH_FREE_LIST 320
template <typename CAllocator>
void CSmallBlockPool<CAllocator>::ValidateFreelist( SharedData_t *pSharedData )
{
#ifdef VALIDATE_SBH_FREE_LIST
	if ( m_nBlockSize != VALIDATE_SBH_FREE_LIST )
		return;
	static int count = 0;
	count++; // Track when the corruption occurs, if repeatable
	pSharedData->m_Lock.LockForWrite();
#ifdef USE_NATIVE_SLIST
	TSLNodeBase_t *pNode = (TSLNodeBase_t *)(m_FreeList.AccessUnprotected()->Next.Next);
#else
	TSLNodeBase_t *pNode = (TSLNodeBase_t *)(m_FreeList.AccessUnprotected()->value.Next);
#endif
	while( pNode )
		pNode = pNode->Next;
	pSharedData->m_Lock.UnlockWrite();
#endif // VALIDATE_SBH_FREE_LIST
}

template <typename CAllocator>
void *CSmallBlockPool<CAllocator>::Alloc()
{
	SharedData_t *pSharedData = GetSharedData();

	ValidateFreelist( pSharedData );

	CThreadSpinRWLock &sharedLock = pSharedData->m_Lock;
	if ( !sharedLock.TryLockForRead() )
	{
		sharedLock.LockForRead();
	}
	byte *pResult;
	intp iPage = -1;
	int iThreadPriority = INT_MAX;

	while (1)
	{
		pResult = m_FreeList.Pop();
		if ( !pResult )
		{
			int nBlockSize = m_nBlockSize;
			byte *pNextAlloc;
			while (1)
			{
				pResult = m_pNextAlloc;
				if ( pResult )
				{
					pNextAlloc = pResult + nBlockSize;
					if ( ( ( (uintp)(pNextAlloc) - 1 ) % BYTES_PAGE ) + nBlockSize > BYTES_PAGE  )
					{
						// Crossed a page boundary
						pNextAlloc = 0;
					}
					if ( m_pNextAlloc.AssignIf( pResult, pNextAlloc ) )
					{
						iPage = (size_t)((byte *)pResult - pSharedData->m_pBase) / BYTES_PAGE;
						break;
					}
				}
				else if ( m_CommitMutex.TryLock() )
				{
					if ( !m_pNextAlloc )
					{
						PageStatus_t *pAllocatedPageStatus = (PageStatus_t *)pSharedData->m_FreePages.Pop();
						if ( pAllocatedPageStatus )
						{
							iPage = pAllocatedPageStatus - &pSharedData->m_PageStatus[0];
						}
						else
						{
							while (1)
							{
								byte *pBlock = pSharedData->m_pNextBlock;
								if ( pBlock >= pSharedData->m_pLimit )
								{
									break;
								}
								if ( ThreadInterlockedAssignPointerIf( (void **)&pSharedData->m_pNextBlock, (void *)( pBlock + BYTES_PAGE ), (void *)pBlock ) )
								{
									iPage = (size_t)((byte *)pBlock - pSharedData->m_pBase) / BYTES_PAGE;
									pAllocatedPageStatus = &pSharedData->m_PageStatus[iPage];
									break;
								}
							}
						}

						if ( pAllocatedPageStatus )
						{
							byte *pBlock = pSharedData->m_pBase + ( iPage * BYTES_PAGE );
							if ( pAllocatedPageStatus->m_nAllocated == NOT_COMMITTED )
							{
								pSharedData->m_Allocator.Commit( pBlock );
							}

							pAllocatedPageStatus->m_pPool = this;
							pAllocatedPageStatus->m_nAllocated = 0;
							pAllocatedPageStatus->m_pNextPageInPool = m_pFirstPage;
							m_pFirstPage = pAllocatedPageStatus;
#ifdef TRACK_SBH_COUNTS
							m_nFreeBlocks += ( BYTES_PAGE / m_nBlockSize );
#endif
							m_nCommittedPages++;
							m_pNextAlloc = pBlock;
						}
						else
						{
							m_pNextAlloc = NULL;
							m_CommitMutex.Unlock();
							sharedLock.UnlockRead();
							return NULL;
						}
					}
					m_CommitMutex.Unlock();
				}
				else
				{
					if ( iThreadPriority == INT_MAX)
					{
						iThreadPriority = ThreadGetPriority();
					}

					if ( iThreadPriority > 0 )
					{
						ThreadSleep( 0 );
					}
				}
			}

			if ( pResult )
			{
				break;
			}
		}
		else
		{
			iPage = (size_t)((byte *)pResult - pSharedData->m_pBase) / BYTES_PAGE;
			break;
		}
	}

#ifdef TRACK_SBH_COUNTS
	--m_nFreeBlocks;
#endif
	++pSharedData->m_PageStatus[iPage].m_nAllocated;
	sharedLock.UnlockRead();

	return pResult;
}

template <typename CAllocator>
void CSmallBlockPool<CAllocator>::Free( void *p )
{
	SharedData_t *pSharedData = GetSharedData();
	size_t iPage = (size_t)((byte *)p - pSharedData->m_pBase) / BYTES_PAGE;

	CThreadSpinRWLock &sharedLock = pSharedData->m_Lock;
	if ( !sharedLock.TryLockForRead() )
	{
		sharedLock.LockForRead();
	}
	--pSharedData->m_PageStatus[iPage].m_nAllocated;
#ifdef TRACK_SBH_COUNTS
	++m_nFreeBlocks;
#endif
	m_FreeList.Push( p );
	pSharedData->m_Lock.UnlockRead();

	ValidateFreelist( pSharedData );
}

// Count the free blocks.  
template <typename CAllocator>
int CSmallBlockPool<CAllocator>::CountFreeBlocks()
{
#ifdef TRACK_SBH_COUNTS
	return m_nFreeBlocks;
#else
	return 0;
#endif
}

// Size of committed memory managed by this heap:
template <typename CAllocator>
int CSmallBlockPool<CAllocator>::GetCommittedSize()
{
	return m_nCommittedPages * BYTES_PAGE;
}

// Return the total blocks memory is committed for in the heap
template <typename CAllocator>
int CSmallBlockPool<CAllocator>::CountCommittedBlocks()
{		 
	return m_nCommittedPages * ( BYTES_PAGE / m_nBlockSize );
}

// Count the number of allocated blocks in the heap:
template <typename CAllocator>
int CSmallBlockPool<CAllocator>::CountAllocatedBlocks()
{
#ifdef TRACK_SBH_COUNTS
	return CountCommittedBlocks() - CountFreeBlocks();
#else
	return 0;
#endif
}

template <typename CAllocator>
int CSmallBlockPool<CAllocator>::PageSort( const void *p1, const void *p2 ) 
{
	SharedData_t *pSharedData = GetSharedData();
	return pSharedData->m_PageStatus[*((int *)p1)].m_SortList.Count() - pSharedData->m_PageStatus[*((int *)p2)].m_SortList.Count();
}


template <typename CAllocator>
bool CSmallBlockPool<CAllocator>::RemovePagesFromFreeList( byte **pPages, int nPages, bool bSortList )
{
	// Since we don't use the depth of the tslist, and sequence is only used for push, we can remove in-place
	int i;
	byte **pLimits = (byte **)stackalloc( nPages * sizeof(byte *) );
	int nBlocksNotInFreeList = 0;
	for ( i = 0; i < nPages; i++ )
	{
		pLimits[i] = pPages[i] + BYTES_PAGE;

		if ( m_pNextAlloc >= pPages[i] && m_pNextAlloc < pLimits[i] )
		{
			nBlocksNotInFreeList = ( pLimits[i] - m_pNextAlloc ) / m_nBlockSize;
			m_pNextAlloc = NULL;
		}
	}

	int iTarget = ( ( BYTES_PAGE/m_nBlockSize ) * nPages ) - nBlocksNotInFreeList;
	int iCount = 0;

	TSLHead_t *pRawFreeList = m_FreeList.AccessUnprotected();
	bool bRemove;
	if ( !bSortList || m_nCommittedPages - nPages == 1 )
	{
#ifdef USE_NATIVE_SLIST
		TSLNodeBase_t **ppPrevNext = (TSLNodeBase_t **)&(pRawFreeList->Next);
#else
		TSLNodeBase_t **ppPrevNext = (TSLNodeBase_t **)&(pRawFreeList->value.Next);
#endif
		TSLNodeBase_t *pNode = *ppPrevNext;
		while ( pNode && iCount != iTarget )
		{
			bRemove = false;
			for ( i = 0; i < nPages; i++ )
			{
				if ( (byte *)pNode >= pPages[i] && (byte *)pNode < pLimits[i] )
				{
					bRemove = true;
					break;
				}
			}

			if ( bRemove )
			{
				iCount++;
				*ppPrevNext = pNode->Next;
			}
			else
			{
				*ppPrevNext = pNode;
				ppPrevNext = &pNode->Next;
			}
			pNode = pNode->Next;
		}
	}
	else
	{
		SharedData_t *pSharedData = GetSharedData();
		byte *pSharedBase = pSharedData->m_pBase;
		TSLNodeBase_t *pNode = m_FreeList.Detach();
		TSLNodeBase_t *pNext;
		int iSortPage;

		int nSortPages = 0;
		int *sortPages = (int *)stackalloc( m_nCommittedPages * sizeof(int) );
		while ( pNode )
		{
			pNext = pNode->Next;
			bRemove = false;
			for ( i = 0; i < nPages; i++ )
			{
				if ( (byte *)pNode >= pPages[i] && (byte *)pNode < pLimits[i] )
				{
					iCount++;
					bRemove = true;
					break;
				}
			}

			if ( !bRemove )
			{
				iSortPage = ( (byte *)pNode - pSharedBase ) / BYTES_PAGE;
				if ( !pSharedData->m_PageStatus[iSortPage].m_SortList.Count() )
				{
					sortPages[nSortPages++] = iSortPage;
				}
				pSharedData->m_PageStatus[iSortPage].m_SortList.Push( pNode );
			}

			pNode = pNext;
		}
	
		if ( nSortPages > 1 )
		{
			qsort( sortPages, nSortPages, sizeof(int), &PageSort );
		}
		for ( i = 0; i < nSortPages; i++ )
		{
			while ( ( pNode = pSharedData->m_PageStatus[sortPages[i]].m_SortList.Pop() ) != NULL )
			{
				m_FreeList.Push( pNode );
			}
		}
	}
	if ( iTarget != iCount )
	{
		DebuggerBreakIfDebugging();
	}

	return ( iTarget == iCount );
}


template <typename CAllocator>
size_t CSmallBlockPool<CAllocator>::Compact( bool bIncremental )
{
	static bool bWarnedCorruption;
	bool bIsCorrupt = false;
	int i;
	size_t nFreed = 0;
	SharedData_t *pSharedData = GetSharedData();
	pSharedData->m_Lock.LockForWrite();

	if ( m_pFirstPage )
	{
		PageStatus_t **pReleasedPages = (PageStatus_t **)stackalloc( m_nCommittedPages * sizeof(PageStatus_t *) );
		PageStatus_t **pReleasedPagesPrevs = (PageStatus_t **)stackalloc( m_nCommittedPages * sizeof(PageStatus_t *) );
		byte **pPageBases = (byte **)stackalloc( m_nCommittedPages * sizeof(byte *) );
		int nPages = 0;
		
		// Gather the pages to return to the backing pool
		PageStatus_t *pPage = m_pFirstPage;
		PageStatus_t *pPagePrev = NULL;
		while ( pPage )
		{
			if ( pPage->m_nAllocated == 0 )
			{
				pReleasedPages[nPages] = pPage;
				pPageBases[nPages] = pSharedData->m_pBase + ( pPage - &pSharedData->m_PageStatus[0] ) * BYTES_PAGE;
				pReleasedPagesPrevs[nPages] = pPagePrev;
				nPages++;

				if ( bIncremental )
				{
					break;
				}
			}
			pPagePrev = pPage;
			pPage = pPage->m_pNextPageInPool;
		}

		if ( nPages )
		{
			// Remove the pages from the pool's free list
			if ( !RemovePagesFromFreeList( pPageBases, nPages, !bIncremental ) && !bWarnedCorruption )
			{
				// We don't know which of the pages encountered an incomplete free list
				// so we'll just push them all back in and hope for the best. This isn't
				// ventilator control software!
				bWarnedCorruption = true;
				bIsCorrupt = true;
			}

			nFreed = nPages * BYTES_PAGE;
			m_nCommittedPages -= nPages;

#ifdef TRACK_SBH_COUNTS
			m_nFreeBlocks -= nPages * ( BYTES_PAGE / m_nBlockSize );
#endif

			// Unlink the pages
			for ( i = nPages - 1; i >= 0; --i )
			{
				if ( pReleasedPagesPrevs[i] )
				{
					pReleasedPagesPrevs[i]->m_pNextPageInPool = pReleasedPages[i]->m_pNextPageInPool;
				}
				else
				{
					m_pFirstPage = pReleasedPages[i]->m_pNextPageInPool;
				}
				pReleasedPages[i]->m_pNextPageInPool = NULL;
				pReleasedPages[i]->m_pPool = NULL;
			}

			// Push them onto the backing free lists
			if ( !pSharedData->m_Allocator.IsVirtual() )
			{
				for ( i = 0; i < nPages; i++ )
				{
					pSharedData->m_FreePages.Push( pReleasedPages[i] );
				}
			}
			else
			{
				int nMinReserve = ( bIncremental ) ? CAllocator::MIN_RESERVE_PAGES * 8 : CAllocator::MIN_RESERVE_PAGES;
				int nReserveNeeded = nMinReserve - pSharedData->m_FreePages.Count();
				if ( nReserveNeeded > 0 )
				{
					int nToKeepCommitted = MIN( nReserveNeeded, nPages );
					while ( nToKeepCommitted-- )
					{
						nPages--;
						pSharedData->m_FreePages.Push( pReleasedPages[nPages] );
					}
				}

				if ( nPages )
				{
					// Detach the list, push the decommitted page on, iterate up to previous 
					// decommits, but them on, then push the committed pages on
					TSLNodeBase_t *pNodes = pSharedData->m_FreePages.Detach();
					for ( i = 0; i < nPages; i++ )
					{
						pReleasedPages[i]->m_nAllocated = NOT_COMMITTED;
						pSharedData->m_Allocator.Decommit( pPageBases[i] );
						pSharedData->m_FreePages.Push( pReleasedPages[i] );
					}

					TSLNodeBase_t *pCur, *pTemp = NULL;
					pCur = pNodes;
					while ( pCur )
					{
						if ( ((PageStatus_t *)pCur)->m_nAllocated == NOT_COMMITTED )
						{
							if ( pTemp )
							{
								pTemp->Next = NULL;
							}
							else
							{
								pNodes = NULL; // The list only has decommitted pages, don't go circular
							}

							while ( pCur )
							{
								pTemp = pCur->Next;
								pSharedData->m_FreePages.Push( pCur );
								pCur = pTemp;
							}
							break;
						}
						pTemp = pCur;
						pCur = pCur->Next;
					}

					while ( pNodes )
					{
						pTemp = pNodes->Next;
						pSharedData->m_FreePages.Push( pNodes );
						pNodes = pTemp;
					}
				}
			}
		}
	}
	pSharedData->m_Lock.UnlockWrite();
	if ( bIsCorrupt )
	{
		Warning( "***** HEAP IS CORRUPT (free compromised for block size %d,in %s heap, possible write after free *****)\n", m_nBlockSize, ( pSharedData->m_Allocator.IsVirtual() ) ? "virtual" : "physical" );
	}
	return nFreed;
}

template <typename CAllocator>
bool CSmallBlockPool<CAllocator>::Validate()
{
#ifdef NO_SBH
	return true;
#else
	int invalid = 0;

	SharedData_t *pSharedData = GetSharedData();
	pSharedData->m_Lock.LockForWrite();

	byte **pPageBases = (byte **)stackalloc( m_nCommittedPages * sizeof(byte *) );
	unsigned *pageCounts = (unsigned *)stackalloc( m_nCommittedPages * sizeof(unsigned) );
	memset( pageCounts, 0, m_nCommittedPages * sizeof(int) );
	unsigned nPages = 0;
	unsigned sumAllocated = 0;
	unsigned freeNotInFreeList = 0;

	// Validate page list is consistent
	if ( !m_pFirstPage )
	{
		if ( m_nCommittedPages != 0 )
		{
			invalid = __LINE__;
			goto notValid;
		}
	}
	else
	{
		PageStatus_t *pPage = m_pFirstPage;
		while ( pPage )
		{
			pPageBases[nPages] = pSharedData->m_pBase + ( pPage - &pSharedData->m_PageStatus[0] ) * BYTES_PAGE;
			if ( pPage->m_pPool != this )
			{
				invalid = __LINE__;
				goto notValid;
			}
			if ( nPages > m_nCommittedPages )
			{
				invalid = __LINE__;
				goto notValid;
			}
			sumAllocated += pPage->m_nAllocated;
			if ( m_pNextAlloc >= pPageBases[nPages] && m_pNextAlloc < pPageBases[nPages] + BYTES_PAGE )
			{
				freeNotInFreeList = pageCounts[nPages] = ( ( pPageBases[nPages] + BYTES_PAGE ) - m_pNextAlloc ) / m_nBlockSize;
			}

			nPages++;
			pPage = pPage->m_pNextPageInPool;
		};

		if ( nPages != m_nCommittedPages )
		{
			invalid = __LINE__;
			goto notValid;
		}
	}

	// Validate block counts
	{
		unsigned blocksPerPage = ( BYTES_PAGE / m_nBlockSize );
#ifdef USE_NATIVE_SLIST
		TSLNodeBase_t *pNode = (TSLNodeBase_t *)(m_FreeList.AccessUnprotected()->Next.Next);
#else
		TSLNodeBase_t *pNode = (TSLNodeBase_t *)(m_FreeList.AccessUnprotected()->value.Next);
#endif
		unsigned i;
		while ( pNode )
		{
			for ( i = 0; i < nPages; i++ )
			{
				if ( (byte *)pNode >= pPageBases[i] && (byte *)pNode < pPageBases[i] + BYTES_PAGE )
				{
					pageCounts[i]++;
					break;
				}
			}

			if ( i == nPages )
			{
				invalid = __LINE__;
				goto notValid;
			}

			pNode = pNode->Next;
		}

		PageStatus_t *pPage = m_pFirstPage;
		i = 0;
		while ( pPage )
		{
			unsigned nFreeOnPage = blocksPerPage - pPage->m_nAllocated;
			if ( nFreeOnPage != pageCounts[i++] )
			{
				invalid = __LINE__;
				goto notValid;
			}
			pPage = pPage->m_pNextPageInPool;
		}
	}

notValid:
	pSharedData->m_Lock.UnlockWrite();

	if ( invalid != 0 )
	{
		return false;
	}

	return true;
#endif
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <typename CAllocator>
CSmallBlockHeap<CAllocator>::CSmallBlockHeap()
{
	m_pSharedData = CPool::GetSharedData();

	// Build a lookup table used to find the correct pool based on size
	const int MAX_TABLE = MAX_SBH_BLOCK >> 2;
	int i = 0;
	int nBytesElement = 0;
	CPool *pCurPool = NULL;
	int iCurPool = 0;

	// Blocks sized 0 - 128 are in pools in increments of 8
	for ( ; i < 32; i++ )
	{
		if ( (i + 1) % 2 == 1)
		{
			nBytesElement += 8;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 129 - 256 are in pools in increments of 16
	for ( ; i < 64; i++ )
	{
		if ( (i + 1) % 4 == 1)
		{
			nBytesElement += 16;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}


	// Blocks sized 257 - 512 are in pools in increments of 32
	for ( ; i < 128; i++ )
	{
		if ( (i + 1) % 8 == 1)
		{
			nBytesElement += 32;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 513 - 768 are in pools in increments of 64
	for ( ; i < 192; i++ )
	{
		if ( (i + 1) % 16 == 1)
		{
			nBytesElement += 64;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 769 - 1024 are in pools in increments of 128
	for ( ; i < 256; i++ )
	{
		if ( (i + 1) % 32 == 1)
		{
			nBytesElement += 128;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	// Blocks sized 1025 - 2048 are in pools in increments of 256
	for ( ; i < MAX_TABLE; i++ )
	{
		if ( (i + 1) % 64 == 1)
		{
			nBytesElement += 256;
			pCurPool = &m_Pools[iCurPool];
			pCurPool->Init( nBytesElement );
			iCurPool++;
			m_PoolLookup[i] = pCurPool;
		}
		else
		{
			m_PoolLookup[i] = pCurPool;
		}
	}

	Assert( iCurPool == NUM_POOLS );
}

template <typename CAllocator>
bool CSmallBlockHeap<CAllocator>::ShouldUse( size_t nBytes )
{
	return ( nBytes <= MAX_SBH_BLOCK );
}

template <typename CAllocator>
bool CSmallBlockHeap<CAllocator>::IsOwner( void * p )
{
	if ( uintp(p) >= uintp(m_pSharedData->m_pBase) )
	{
		intp index = (intp)((byte *)p - m_pSharedData->m_pBase) / BYTES_PAGE;
		return ( index < ARRAYSIZE(m_pSharedData->m_PageStatus) );
	}
	return false;
}

template <typename CAllocator>
void *CSmallBlockHeap<CAllocator>::Alloc( size_t nBytes )
{
	if ( nBytes == 0)
	{
		nBytes = 1;
	}
	Assert( ShouldUse( nBytes ) );
	CPool *pPool = FindPool( nBytes );
	void *p = pPool->Alloc();
	return p;
}

template <typename CAllocator>
void *CSmallBlockHeap<CAllocator>::Realloc( void *p, size_t nBytes )
{
	if ( nBytes == 0)
	{
		nBytes = 1;
	}

	CPool *pOldPool = FindPool( p );
	CPool *pNewPool = ( ShouldUse( nBytes ) ) ? FindPool( nBytes ) : NULL;

	if ( pOldPool == pNewPool )
	{
		return p;
	}

	void *pNewBlock = NULL;

	if ( !pNewBlock )
	{
		pNewBlock = MemAlloc_Alloc( nBytes ); // Call back out so blocks can move from the secondary to the primary pools
	}

	if ( !pNewBlock )
	{
		pNewBlock = malloc_internal( DEF_REGION, nBytes );
	}

	if ( pNewBlock )
	{
		size_t nBytesCopy = MIN( nBytes, pOldPool->GetBlockSize() );
		memcpy( pNewBlock, p, nBytesCopy );
	} 
	else if ( nBytes < pOldPool->GetBlockSize() )
	{
		return p;
	}

	pOldPool->Free( p );

	return pNewBlock;
}

template <typename CAllocator>
void CSmallBlockHeap<CAllocator>::Free( void *p )
{
	CPool *pPool = FindPool( p );
	if ( pPool )
	{
		pPool->Free( p );
	}
	else
	{
		// we probably didn't hook some allocation and now we're freeing it or the heap has been trashed!
		DebuggerBreakIfDebugging();
	}
}

template <typename CAllocator>
size_t CSmallBlockHeap<CAllocator>::GetSize( void *p )
{
	CPool *pPool = FindPool( p );
	return pPool->GetBlockSize();
}

template <typename CAllocator>
void CSmallBlockHeap<CAllocator>::Usage( size_t &bytesCommitted, size_t &bytesAllocated )
{
	bytesCommitted = 0;
	bytesAllocated = 0;
	for ( int i = 0; i < NUM_POOLS; i++ )
	{
		bytesCommitted += m_Pools[i].GetCommittedSize();
		bytesAllocated += ( m_Pools[i].CountAllocatedBlocks() * m_Pools[i].GetBlockSize() );
	}
}

template <typename CAllocator>
void CSmallBlockHeap<CAllocator>::DumpStats( const char *pszTag, FILE *pFile )
{
	size_t bytesCommitted, bytesAllocated;
	Usage( bytesCommitted, bytesAllocated );

	if ( pFile )
	{

		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			// output for vxconsole parsing
			fprintf( pFile, "Pool %2i: (size: %4u) blocks: allocated:%5i free:%5i committed:%5i (committed size:%4u kb)\n", 
				i, 
				m_Pools[i].GetBlockSize(), 
				m_Pools[i].CountAllocatedBlocks(), 
				m_Pools[i].CountFreeBlocks(),
				m_Pools[i].CountCommittedBlocks(), 
				m_Pools[i].GetCommittedSize() );
		}
		fprintf( pFile, "Totals (%s): Committed:%5u kb Allocated:%5u kb\n", pszTag, bytesCommitted / 1024, bytesAllocated / 1024 );
	}
	else
	{
		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			Msg( "Pool %2i: (size: %4u) blocks: allocated:%5i free:%5i committed:%5i (committed size:%4u kb)\n",i, m_Pools[i].GetBlockSize(),m_Pools[i].CountAllocatedBlocks(), m_Pools[i].CountFreeBlocks(),m_Pools[i].CountCommittedBlocks(), m_Pools[i].GetCommittedSize() / 1024);
		}

		Msg( "Totals (%s): Committed:%5u kb Allocated:%5u kb\n", pszTag, bytesCommitted / 1024, bytesAllocated / 1024 );
	}
}

template <typename CAllocator>
CSmallBlockPool<CAllocator> *CSmallBlockHeap<CAllocator>::FindPool( size_t nBytes )
{
	return m_PoolLookup[(nBytes - 1) >> 2];
}

template <typename CAllocator>
CSmallBlockPool<CAllocator> *CSmallBlockHeap<CAllocator>::FindPool( void *p )
{
	// NOTE: If p < m_pBase, cast to unsigned size_t will cause it to be large
	size_t index = (size_t)((byte *)p - m_pSharedData->m_pBase) / BYTES_PAGE;
	if ( index < ARRAYSIZE(m_pSharedData->m_PageStatus) )
		return m_pSharedData->m_PageStatus[index].m_pPool;
	return NULL;
}

template <typename CAllocator>
size_t CSmallBlockHeap<CAllocator>::Compact( bool bIncremental )
{
	size_t nRecovered = 0;
	if ( bIncremental )
	{
		static int iLastIncremental;

		iLastIncremental++;
		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			int idx = ( i + iLastIncremental ) % NUM_POOLS;
			nRecovered = m_Pools[idx].Compact( bIncremental );
			if ( nRecovered )
			{
				iLastIncremental = idx;
				break;
			}

		}
	}
	else
	{
		for ( int i = 0; i < NUM_POOLS; i++ )
		{
			nRecovered += m_Pools[i].Compact( bIncremental );
		}
	}
	return nRecovered;
}

template <typename CAllocator>
bool CSmallBlockHeap<CAllocator>::Validate()
{
	bool valid = true;
	for ( int i = 0; i < NUM_POOLS; i++ )
	{
		valid = m_Pools[i].Validate() && valid;
	}
	return valid;
}

#endif // MEM_SBH_ENABLED


//-----------------------------------------------------------------------------
// Lightweight memory tracking
//-----------------------------------------------------------------------------

#ifdef USE_LIGHT_MEM_DEBUG

#ifndef LIGHT_MEM_DEBUG_REQUIRES_CMD_LINE_SWITCH
#define UsingLMD() true
#else // LIGHT_MEM_DEBUG_REQUIRES_CMD_LINE_SWITCH
bool g_bUsingLMD = ( Plat_GetCommandLineA() ) ? ( strstr( Plat_GetCommandLineA(), "-uselmd" ) != NULL ) : false;
#define UsingLMD() g_bUsingLMD
#if defined( _PS3 )
#error "Plat_GetCommandLineA() not implemented on PS3"
#endif
#endif // LIGHT_MEM_DEBUG_REQUIRES_CMD_LINE_SWITCH

const char *g_pszUnknown = "unknown";

struct Sentinal_t
{
	DWORD value[4];
};

Sentinal_t g_HeadSentinel = 
{
	0xdeadbeef,
	0xbaadf00d,
	0xbd122969,
	0xdeadbeef,
};

Sentinal_t g_TailSentinel = 
{
	0xbaadf00d,
	0xbd122969,
	0xdeadbeef,
	0xbaadf00d,
};

const byte g_FreeFill = 0xdd;

static const uint LWD_FREE = 0;
static const uint LWD_ALLOCATED = 1;

#define LMD_STATUS_BITS ( 1 )
#define LMD_ALIGN_BITS  ( 32 - LMD_STATUS_BITS )
#define LMD_MAX_ALIGN   ( 1 << ( LMD_ALIGN_BITS - 1) )

struct AllocHeader_t
{
	const char *pszModule;
	int line;
	size_t nBytes;
	uint status : LMD_STATUS_BITS;
	uint align : LMD_ALIGN_BITS;
	Sentinal_t sentinal;
};

const int g_nRecentFrees = ( IsPC() ) ? 8192 : 512;
AllocHeader_t **g_pRecentFrees = (AllocHeader_t **)calloc( g_nRecentFrees, sizeof(AllocHeader_t *) );
int g_iNextFreeSlot;

#define INTERNAL_INLINE

#define LMDToHeader( pUserPtr )		( ((AllocHeader_t *)(pUserPtr)) - 1 )
#define LMDFromHeader( pHeader )	( (byte *)((pHeader) + 1) )

CThreadFastMutex g_LMDMutex;

const char *g_pLMDFileName = NULL;
int g_nLMDLine;
int g_iLMDDepth;

void LMDPushAllocDbgInfo( const char *pFileName, int nLine )
{
	if ( ThreadInMainThread() )
	{
		if ( !g_iLMDDepth )
		{
			g_pLMDFileName = pFileName;
			g_nLMDLine = nLine;
		}
		g_iLMDDepth++;
	}
}

void LMDPopAllocDbgInfo()
{
	if ( ThreadInMainThread() && g_iLMDDepth > 0 )
	{
		g_iLMDDepth--;
		if ( g_iLMDDepth == 0 )
		{
			g_pLMDFileName = NULL;
			g_nLMDLine = 0;
		}
	}
}


void LMDReportInvalidBlock( AllocHeader_t *pHeader, const char *pszMessage )
{
	char szMsg[256];
	if ( pHeader )
	{
		sprintf( szMsg, "HEAP IS CORRUPT: %s (block 0x%x, size %d, alignment %d)\n", pszMessage, (size_t)LMDFromHeader( pHeader ), pHeader->nBytes, pHeader->align );
	}
	else
	{
		sprintf( szMsg, "HEAP IS CORRUPT: %s\n", pszMessage );
	}
	if ( Plat_IsInDebugSession() )
	{
		DebuggerBreak();
	}
	else
	{
		WriteMiniDump();
	}
#ifdef IS_WINDOWS_PC
	::MessageBox( NULL, szMsg, "Error", MB_SYSTEMMODAL | MB_OK );
#else
	Warning( szMsg );
#endif
}

void LMDValidateBlock( AllocHeader_t *pHeader, bool bFreeList )
{
	if ( !pHeader )
		return;

	if ( memcmp( &pHeader->sentinal, &g_HeadSentinel, sizeof(Sentinal_t) ) != 0 )
	{
		LMDReportInvalidBlock( pHeader, "Head sentinel corrupt" );
	}
	if ( memcmp( ((Sentinal_t *)(LMDFromHeader( pHeader ) + pHeader->nBytes)), &g_TailSentinel, sizeof(Sentinal_t) ) != 0 )
	{
		LMDReportInvalidBlock( pHeader, "Tail sentinel corrupt" );
	}
	if ( bFreeList )
	{
		byte *pCur = (byte *)pHeader + sizeof(AllocHeader_t);
		byte *pLimit = pCur + pHeader->nBytes;
		while ( pCur != pLimit )
		{
			if ( *pCur++ != g_FreeFill )
			{
				LMDReportInvalidBlock( pHeader, "Write after free" );
			}
		}
	}
}


size_t LMDComputeHeaderSize( size_t align = 0 )
{
	if ( !align )
		return sizeof(AllocHeader_t);
	// For aligned allocs, the header is preceded by padding which maintains alignment
	if ( align > LMD_MAX_ALIGN )
		s_StdMemAlloc.SetCRTAllocFailed( align ); // TODO: could convert alignment to exponent to get around this, or use a flag for alignments over 1KB or 1MB...
	return ( ( sizeof( AllocHeader_t ) + (align-1) ) & ~(align-1) );
}

size_t LMDAdjustSize( size_t &nBytes, size_t align = 0 )
{
	if ( !UsingLMD() )
		return nBytes;
	// Add data before+after each alloc
	return ( nBytes + LMDComputeHeaderSize( align ) + sizeof(Sentinal_t) );
}

void *LMDNoteAlloc( void *p, size_t nBytes, size_t align = 0, const char *pszModule = g_pszUnknown, int line = 0 )
{
	if ( !UsingLMD() )
	{
		return p;
	}

	if ( g_pLMDFileName )
	{
		pszModule = g_pLMDFileName;
		line = g_nLMDLine;
	}

	if ( p )
	{
		byte *pUserPtr = ((byte*)p) + LMDComputeHeaderSize( align );
		AllocHeader_t *pHeader = LMDToHeader( pUserPtr );
		pHeader->pszModule = pszModule;
		pHeader->line = line;
		pHeader->status = LWD_ALLOCATED;
		pHeader->nBytes = nBytes;
		pHeader->align = (uint)align;
		pHeader->sentinal = g_HeadSentinel;
		*((Sentinal_t *)(pUserPtr + pHeader->nBytes)) = g_TailSentinel;
		LMDValidateBlock( pHeader, false );
		return pUserPtr;
	}
	return NULL;

	// Some SBH clients rely on allocations > 16 bytes being 16-byte aligned, so we mustn't break that assumption:
	MEMSTD_COMPILE_TIME_ASSERT( sizeof( AllocHeader_t ) % 16 == 0 );
}

void *LMDNoteFree( void *p )
{
	if ( !UsingLMD() )
	{
		return p;
	}

	AUTO_LOCK( g_LMDMutex );
	if ( !p )
	{
		return NULL;
	}

	AllocHeader_t *pHeader = LMDToHeader( p );
	if ( pHeader->status == LWD_FREE )
	{
		LMDReportInvalidBlock( pHeader, "Double free" );
	}
	LMDValidateBlock( pHeader, false );

	AllocHeader_t *pToReturn;
	if ( pHeader->nBytes < 16*1024 )
	{
		pToReturn = g_pRecentFrees[g_iNextFreeSlot];
		LMDValidateBlock( pToReturn, true );

		g_pRecentFrees[g_iNextFreeSlot] = pHeader;
		g_iNextFreeSlot = (g_iNextFreeSlot + 1 ) % g_nRecentFrees;
	}
	else
	{
		pToReturn = pHeader;
		LMDValidateBlock( g_pRecentFrees[rand() % g_nRecentFrees], true );
	}

	pHeader->status = LWD_FREE;
	memset( pHeader + 1, g_FreeFill, pHeader->nBytes );

	if ( pToReturn && ( pToReturn->align ) )
	{
		// For aligned allocations, the actual system allocation starts *before* the LMD header:
		size_t headerPadding = LMDComputeHeaderSize( pToReturn->align ) - sizeof( AllocHeader_t );
		return ( ((byte*)pToReturn) - headerPadding );
	}

	return pToReturn;
}

size_t LMDGetSize( void *p )
{
	if ( !UsingLMD() )
	{
		return (size_t)(-1);
	}

	AllocHeader_t *pHeader = LMDToHeader( p );
	return pHeader->nBytes;
}

bool LMDValidateHeap()
{
	if ( !UsingLMD() )
	{
		return true;
	}

	AUTO_LOCK( g_LMDMutex );
	for ( int i = 0; i < g_nRecentFrees && g_pRecentFrees[i]; i++ )
	{
		LMDValidateBlock( g_pRecentFrees[i], true );
	}
	return true;
}

void *LMDRealloc( void *pMem, size_t nSize, size_t align = 0, const char *pszModule = g_pszUnknown, int line = 0 )
{
	if ( nSize == 0 )
	{
		s_StdMemAlloc.Free( pMem );
		return NULL;
	}
	void *pNew;
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	if ( align )
		pNew = s_StdMemAlloc.AllocAlign( nSize, align, pszModule, line );
	else
#endif // MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
		pNew = s_StdMemAlloc.Alloc( nSize, pszModule, line );
	if ( !pMem )
	{
		return pNew;
	}
	AllocHeader_t *pHeader = LMDToHeader( pMem );
	if ( align != pHeader->align )
	{
		LMDReportInvalidBlock( pHeader, "Realloc changed alignment!" );
	}
	size_t nCopySize = MIN( nSize, pHeader->nBytes );
	memcpy( pNew, pMem, nCopySize );
	s_StdMemAlloc.Free( pMem, pszModule, line );
	return pNew;
}

#else // USE_LIGHT_MEM_DEBUG

#define INTERNAL_INLINE FORCEINLINE
#define UsingLMD() false
FORCEINLINE size_t LMDAdjustSize( size_t &nBytes, size_t align = 0 ) { return nBytes; }
#define LMDNoteAlloc( pHeader, ... ) (pHeader)
#define LMDNoteFree( pHeader, ... ) (pHeader)
#define LMDGetSize( pHeader ) (size_t)(-1)
#define LMDToHeader( pHeader ) (pHeader)
#define LMDFromHeader( pHeader ) (pHeader)
#define LMDValidateHeap() (true)
#define LMDPushAllocDbgInfo( pFileName, nLine ) ((void)0)
#define LMDPopAllocDbgInfo() ((void)0)
FORCEINLINE void *LMDRealloc( void *pMem, size_t nSize, size_t align = 0, const char *pszModule = NULL, int line = 0 ) { return NULL; }

#endif // USE_LIGHT_MEM_DEBUG

//-----------------------------------------------------------------------------
// Internal versions
//-----------------------------------------------------------------------------

INTERNAL_INLINE void *CStdMemAlloc::InternalAllocFromPools( size_t nSize )
{
#if MEM_SBH_ENABLED
	void *pMem;

	pMem = m_PrimarySBH.Alloc( nSize );
	if ( pMem )
	{
		return pMem;
	}

#ifdef MEMALLOC_USE_SECONDARY_SBH
	pMem = m_SecondarySBH.Alloc( nSize );
	if ( pMem )
	{
		return pMem;
	}
#endif // MEMALLOC_USE_SECONDARY_SBH

#ifndef MEMALLOC_NO_FALLBACK
	pMem = m_FallbackSBH.Alloc( nSize );
	if ( pMem )
	{
		return pMem;
	}
#endif // MEMALLOC_NO_FALLBACK

	CallAllocFailHandler( nSize );
#endif // MEM_SBH_ENABLED
	return NULL;
}

INTERNAL_INLINE void *CStdMemAlloc::InternalAlloc( int region, size_t nSize )
{
	PROFILE_ALLOC(Malloc);
	
	void *pMem;

#if MEM_SBH_ENABLED
	if ( m_PrimarySBH.ShouldUse( nSize ) ) // test valid for either pool
	{
		pMem = InternalAllocFromPools( nSize );
		if ( !pMem )
		{
			CompactOnFail();
			pMem = InternalAllocFromPools( nSize );
		}
		if ( pMem )
		{
			ApplyMemoryInitializations( pMem, nSize );
			return pMem;
		}

		ExecuteOnce( DevWarning( "\n\nDRASTIC MEMORY OVERFLOW: Fell out of small block heap!\n\n\n") );
	}
#endif // MEM_SBH_ENABLED

	pMem = malloc_internal( region, nSize );
	if ( !pMem )
	{
		CompactOnFail();
		pMem = malloc_internal( region, nSize );
		if ( !pMem )
		{
			SetCRTAllocFailed( nSize );
			return NULL;
		}
	}

	ApplyMemoryInitializations( pMem, nSize );
	return pMem;
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
INTERNAL_INLINE void *CStdMemAlloc::InternalAllocAligned( int region, size_t nSize, size_t align )
{
	PROFILE_ALLOC(MallocAligned);
	
	void *pMem;

#if MEM_SBH_ENABLED
	size_t nSizeAligned = ( nSize + align - 1 ) & ~( align - 1 );
	if ( m_PrimarySBH.ShouldUse( nSizeAligned ) ) // test valid for either pool
	{
		pMem = InternalAllocFromPools( nSizeAligned  );
		if ( !pMem )
		{
			CompactOnFail();
			pMem = InternalAllocFromPools( nSizeAligned  );
		}
		if ( pMem )
		{
			ApplyMemoryInitializations( pMem, nSizeAligned  );
			return pMem;
		}

		ExecuteOnce( DevWarning( "Warning: Fell out of small block heap!\n") );
	}
#endif // MEM_SBH_ENABLED

	pMem = malloc_aligned_internal( region, nSize, align );
	if ( !pMem )
	{
		CompactOnFail();
		pMem = malloc_aligned_internal( region, nSize, align );
		if ( !pMem )
		{
			SetCRTAllocFailed( nSize );
			return NULL;
		}
	}

	ApplyMemoryInitializations( pMem, nSize );
	return pMem;
}
#endif // MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

INTERNAL_INLINE void *CStdMemAlloc::InternalRealloc( void *pMem, size_t nSize )
{
	if ( !pMem )
	{
		return RegionAlloc( DEF_REGION, nSize );
	}

	PROFILE_ALLOC(Realloc);

#if MEM_SBH_ENABLED
	if ( m_PrimarySBH.IsOwner( pMem ) )
	{
		return m_PrimarySBH.Realloc( pMem, nSize );
	}

#ifdef MEMALLOC_USE_SECONDARY_SBH
	if ( m_SecondarySBH.IsOwner( pMem ) )
	{
		return m_SecondarySBH.Realloc( pMem, nSize );
	}

#endif // MEMALLOC_USE_SECONDARY_SBH

#ifndef MEMALLOC_NO_FALLBACK
	if ( m_FallbackSBH.IsOwner( pMem ) )
	{
		return m_FallbackSBH.Realloc( pMem, nSize );
	}
#endif // MEMALLOC_NO_FALLBACK

#endif // MEM_SBH_ENABLED

	void *pRet = realloc_internal( pMem, nSize );
	if ( !pRet )
	{
		CompactOnFail();
		pRet = realloc_internal( pMem, nSize );
		if ( !pRet )
		{
			SetCRTAllocFailed( nSize );
		}
	}

	return pRet;
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
INTERNAL_INLINE void *CStdMemAlloc::InternalReallocAligned( void *pMem, size_t nSize, size_t align )
{
	if ( !pMem )
	{
		return InternalAllocAligned( DEF_REGION, nSize, align );
	}

	PROFILE_ALLOC(ReallocAligned);

#if MEM_SBH_ENABLED
	if ( m_PrimarySBH.IsOwner( pMem ) )
	{
		return m_PrimarySBH.Realloc( pMem, nSize );
	}

#ifdef MEMALLOC_USE_SECONDARY_SBH
	if ( m_SecondarySBH.IsOwner( pMem ) )
	{
		return m_SecondarySBH.Realloc( pMem, nSize );
	}
#endif // MEMALLOC_USE_SECONDARY_SBH

#ifndef MEMALLOC_NO_FALLBACK
	if ( m_FallbackSBH.IsOwner( pMem ) )
	{
		return m_FallbackSBH.Realloc( pMem, nSize );
	}
#endif // MEMALLOC_NO_FALLBACK

#endif // MEM_SBH_ENABLED

	void *pRet = realloc_aligned_internal( pMem, nSize, align );
	if ( !pRet )
	{
		CompactOnFail();
		pRet = realloc_aligned_internal( pMem, nSize, align );
		if ( !pRet )
		{
			SetCRTAllocFailed( nSize );
		}
	}

	return pRet;
}
#endif

INTERNAL_INLINE void CStdMemAlloc::InternalFree( void *pMem )
{
	if ( !pMem )
	{
		return;
	}

	PROFILE_ALLOC(Free);

#if MEM_SBH_ENABLED
	if ( m_PrimarySBH.IsOwner( pMem ) )
	{
		m_PrimarySBH.Free( pMem );
		return;
	}

#ifdef MEMALLOC_USE_SECONDARY_SBH
	if ( m_SecondarySBH.IsOwner( pMem ) )
	{
		return m_SecondarySBH.Free( pMem );
	}
#endif // MEMALLOC_USE_SECONDARY_SBH

#ifndef MEMALLOC_NO_FALLBACK
	if ( m_FallbackSBH.IsOwner( pMem ) )
	{
		m_FallbackSBH.Free( pMem );
		return;
	}
#endif // MEMALLOC_NO_FALLBACK

#endif // MEM_SBH_ENABLED

	free_internal( pMem );
}

void CStdMemAlloc::CompactOnFail()
{
	CompactHeap();
}

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CStdMemAlloc::Alloc( size_t nSize )
{
	size_t nAdjustedSize = LMDAdjustSize( nSize );
	return LMDNoteAlloc( CStdMemAlloc::InternalAlloc( DEF_REGION, nAdjustedSize ), nSize );
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void * CStdMemAlloc::AllocAlign( size_t nSize, size_t align )
{
	size_t nAdjustedSize = LMDAdjustSize( nSize, align );
	return LMDNoteAlloc( CStdMemAlloc::InternalAllocAligned( DEF_REGION, nAdjustedSize, align ), nSize, align );
}
#endif // MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize )
{
	if ( UsingLMD() )
		return LMDRealloc( pMem, nSize );
	return CStdMemAlloc::InternalRealloc( pMem, nSize );
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void * CStdMemAlloc::ReallocAlign( void *pMem, size_t nSize, size_t align )
{
	if ( UsingLMD() )
		return LMDRealloc( pMem, nSize, align );
	return CStdMemAlloc::InternalReallocAligned( pMem, nSize, align );
}
#endif // MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

void  CStdMemAlloc::Free( void *pMem )
{
	pMem = LMDNoteFree( pMem );
	CStdMemAlloc::InternalFree( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CStdMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	size_t nAdjustedSize = LMDAdjustSize( nSize );
	return LMDNoteAlloc( CStdMemAlloc::InternalAlloc( DEF_REGION, nAdjustedSize ), nSize, 0, pFileName, nLine );
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void *CStdMemAlloc::AllocAlign( size_t nSize, size_t align, const char *pFileName, int nLine )
{
	size_t nAdjustedSize = LMDAdjustSize( nSize, align );
	return LMDNoteAlloc( CStdMemAlloc::InternalAllocAligned( DEF_REGION, nAdjustedSize, align ), nSize, align, pFileName, nLine );
}
#endif // MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

void *CStdMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	if ( UsingLMD() )
		return LMDRealloc( pMem, nSize, 0, pFileName, nLine );
	return CStdMemAlloc::InternalRealloc( pMem, nSize );
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void * CStdMemAlloc::ReallocAlign( void *pMem, size_t nSize, size_t align, const char *pFileName, int nLine )
{
	if ( UsingLMD() )
		return LMDRealloc( pMem, nSize, align, pFileName, nLine );
	return CStdMemAlloc::InternalReallocAligned( pMem, nSize, align );
}
#endif // MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

void  CStdMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	pMem = LMDNoteFree( pMem );
	CStdMemAlloc::InternalFree( pMem );
}

void *CStdMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Region support
//-----------------------------------------------------------------------------
void *CStdMemAlloc::RegionAlloc( int region, size_t nSize ) 
{
	size_t nAdjustedSize = LMDAdjustSize( nSize );
	return LMDNoteAlloc( CStdMemAlloc::InternalAlloc( region, nAdjustedSize ), nSize );
}

void *CStdMemAlloc::RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine )
{
	size_t nAdjustedSize = LMDAdjustSize( nSize );
	return LMDNoteAlloc( CStdMemAlloc::InternalAlloc( region, nAdjustedSize ), nSize, 0, pFileName, nLine );
}

#if defined (LINUX)
#include <malloc.h>
#elif defined (OSX)
#define malloc_usable_size( ptr ) malloc_size( ptr )
extern "C" {
	extern size_t malloc_size( const void *ptr );
}
#endif // LINUX/OSX

//-----------------------------------------------------------------------------
// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
//-----------------------------------------------------------------------------
size_t CStdMemAlloc::GetSize( void *pMem )
{
	if ( !pMem )
		return CalcHeapUsed();

	if ( UsingLMD() )
	{
		return LMDGetSize( pMem );
	}

#if MEM_SBH_ENABLED
	if ( m_PrimarySBH.IsOwner( pMem ) )
	{
		return m_PrimarySBH.GetSize( pMem );
	}

#ifdef MEMALLOC_USE_SECONDARY_SBH
	if ( m_SecondarySBH.IsOwner( pMem ) )
	{
		return m_SecondarySBH.GetSize( pMem );
	}
#endif // MEMALLOC_USE_SECONDARY_SBH

#ifndef MEMALLOC_NO_FALLBACK
	if ( m_FallbackSBH.IsOwner( pMem ) )
	{
		return m_FallbackSBH.GetSize( pMem );
	}
#endif // MEMALLOC_NO_FALLBACK

#endif // MEM_SBH_ENABLED

	return msize_internal( pMem );
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CStdMemAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
	LMDPushAllocDbgInfo( pFileName, nLine );
}

void CStdMemAlloc::PopAllocDbgInfo()
{
	LMDPopAllocDbgInfo();
}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
int32 CStdMemAlloc::CrtSetBreakAlloc( int32 lNewBreakAlloc )
{
	return 0;
}

int CStdMemAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
	return 0;
}

int CStdMemAlloc::CrtIsValidHeapPointer( const void *pMem )
{
	return 1;
}

int CStdMemAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
	return 1;
}

int CStdMemAlloc::CrtCheckMemory( void )
{
#ifndef _CERT
	LMDValidateHeap();
#if MEM_SBH_ENABLED
	if ( !m_PrimarySBH.Validate() )
	{
		ExecuteOnce( Msg( "Small block heap is corrupt (primary)\n " ) );
	}
#ifdef MEMALLOC_USE_SECONDARY_SBH
	if ( !m_SecondarySBH.Validate() )
	{
		ExecuteOnce( Msg( "Small block heap is corrupt (secondary)\n " ) );
	}
#endif // MEMALLOC_USE_SECONDARY_SBH
#ifndef MEMALLOC_NO_FALLBACK
	if ( !m_FallbackSBH.Validate() )
	{
		ExecuteOnce( Msg( "Small block heap is corrupt (fallback)\n " ) );
	}
#endif // MEMALLOC_NO_FALLBACK
#endif // MEM_SBH_ENABLED
#endif // _CERT
	return 1;
}

int CStdMemAlloc::CrtSetDbgFlag( int nNewFlag )
{
	return 0;
}

void CStdMemAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
}

// FIXME: Remove when we have our own allocator
void* CStdMemAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
	return 0;
}

void* CStdMemAlloc::CrtSetReportHook( void* pfnNewHook )
{
	return 0;
}

int CStdMemAlloc::CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg )
{
	return 0;
}

int CStdMemAlloc::heapchk()
{
#ifdef _WIN32
	CrtCheckMemory();
	return _HEAPOK;
#else
	return 1;
#endif
}

void CStdMemAlloc::DumpStats() 
{ 
	DumpStatsFileBase( "memstats" );
}

void CStdMemAlloc::DumpStatsFileBase( char const *pchFileBase )
{
#if defined( _WIN32 ) || defined( _GAMECONSOLE )
	char filename[ 512 ];
	_snprintf( filename, sizeof( filename ) - 1,
#ifdef _X360
		"D:\\%s.txt",
#elif defined( _PS3 )
		"/app_home/%s.txt",
#else
		"%s.txt",
#endif
		pchFileBase );
	filename[ sizeof( filename ) - 1 ] = 0;
	FILE *pFile = ( IsGameConsole() ) ? NULL : fopen( filename, "wt" );

#if MEM_SBH_ENABLED
	if ( pFile )
		fprintf( pFile, "Fixed Page SBH:\n" );
	else
		Msg( "Fixed Page SBH:\n" );
	m_PrimarySBH.DumpStats("Fixed Page SBH", pFile);
#ifdef MEMALLOC_USE_SECONDARY_SBH
	if ( pFile )
		fprintf( pFile, "Secondary Fixed Page SBH:\n" );
	else
		Msg( "Secondary Page SBH:\n" );
	m_SecondarySBH.DumpStats("Secondary Page SBH", pFile);
#endif // MEMALLOC_USE_SECONDARY_SBH
#ifndef MEMALLOC_NO_FALLBACK
	if ( pFile )
		fprintf( pFile, "\nFallback SBH:\n" );
	else
		Msg( "\nFallback SBH:\n" );
	m_FallbackSBH.DumpStats("Fallback SBH", pFile);	// Dump statistics to small block heap
#endif // MEMALLOC_NO_FALLBACK
#endif // MEM_SBH_ENABLED

#ifdef _PS3
	malloc_managed_size mms;
	(g_pMemOverrideRawCrtFns->pfn_malloc_stats)( &mms );
	Msg( "PS3 malloc_stats: %u / %u / %u \n", mms.current_inuse_size, mms.current_system_size, mms.max_system_size );
#endif // _PS3

	heapstats_internal( pFile );
#if defined( _X360 )
	XBX_rMemDump( filename );
#endif

	if ( pFile )
		fclose( pFile );
#endif // _WIN32 || _GAMECONSOLE
}

IVirtualMemorySection * CStdMemAlloc::AllocateVirtualMemorySection( size_t numMaxBytes )
{
#if defined( _GAMECONSOLE ) || defined( _WIN32 )
	extern IVirtualMemorySection * VirtualMemoryManager_AllocateVirtualMemorySection( size_t numMaxBytes );
	return VirtualMemoryManager_AllocateVirtualMemorySection( numMaxBytes );
#else
	return NULL;
#endif
}

size_t CStdMemAlloc::ComputeMemoryUsedBy( char const *pchSubStr )
{
	return 0;//dbg heap only.
}

static inline size_t ExtraDevkitMemory( void )
{
#if defined( _PS3 )
	// 213MB are available in retail mode, so adjust free mem to reflect that even if we're in devkit mode
	const size_t RETAIL_SIZE = 213*1024*1024;
	static sys_memory_info stat;
	sys_memory_get_user_memory_size( &stat );
	if ( stat.total_user_memory > RETAIL_SIZE )
		return ( stat.total_user_memory - RETAIL_SIZE );
#elif defined( _X360 )
	// TODO: detect the new 1GB devkit...
#endif // _PS3/_X360
	return 0;
}

void CStdMemAlloc::GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory )
{
	if ( !pUsedMemory || !pFreeMemory )
		return;

	size_t dlMallocFree = 0;
#if defined( USE_DLMALLOC )
	// Account for free memory contained within DLMalloc's FIRST region. The rationale is as follows:
	//  - the first region is supposed to service large allocations via virtual allocation, and to grow as
	//    needed (until all physical pages are used), so true 'out of memory' failures should occur there.
	//  - other regions (the 2-256kb 'medium block heap', or per-DLL heaps, and the Small Block Heap)
	//    are sized to a pre-determined high watermark, and not intended to grow. Free memory within
	//    those regions is not available for large allocations, so adding that to the 'free memory'
	//    yields confusing data which does not correspond well with out-of-memory failures.
	mallinfo info = mspace_mallinfo( g_AllocRegions[ 0 ] );
	dlMallocFree += info.fordblks;
#endif // USE_DLMALLOC

#if defined ( _X360 )

	// GlobalMemoryStatus tells us how much physical memory is free
	MEMORYSTATUS stat;
	::GlobalMemoryStatus( &stat );
	*pFreeMemory  = stat.dwAvailPhys;
	*pFreeMemory += dlMallocFree;
	// Adjust free mem to reflect a retail box, even if we're using a devkit with extra memory
	*pFreeMemory -= ExtraDevkitMemory();

	// Used is total minus free (discount the 32MB system reservation)
	*pUsedMemory = ( stat.dwTotalPhys - 32*1024*1024 ) - *pFreeMemory;

#elif defined( _PS3 )

	// NOTE: we use dlmalloc instead of the system heap, so we do NOT count the system heap's free space!
	//static malloc_managed_size mms;
	//(g_pMemOverrideRawCrtFns->pfn_malloc_stats)( &mms );
	//int heapFree = mms.current_system_size - mms.current_inuse_size;

	// sys_memory_get_user_memory_size tells us how much PPU memory is used/free
	static sys_memory_info stat;
	sys_memory_get_user_memory_size( &stat );
	*pFreeMemory  = stat.available_user_memory;
	*pFreeMemory += dlMallocFree;
	*pUsedMemory  = stat.total_user_memory - *pFreeMemory;
	// Adjust free mem to reflect a retail box, even if we're using a devkit with extra memory
	*pFreeMemory -= ExtraDevkitMemory();

#else // _X360/_PS3/other

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;

#endif // _X360/_PS3//other
}

#define MAX_GENERIC_MEMORY_STATS 64
GenericMemoryStat_t g_MemStats[MAX_GENERIC_MEMORY_STATS];
int g_nMemStats = 0;
static inline int AddGenericMemoryStat( const char *name, int value )
{
	Assert( g_nMemStats < MAX_GENERIC_MEMORY_STATS );
	if ( g_nMemStats < MAX_GENERIC_MEMORY_STATS )
	{
		g_MemStats[ g_nMemStats ].name  = name;
		g_MemStats[ g_nMemStats ].value = value;
		g_nMemStats++;
	}
	return g_nMemStats;
}

int CStdMemAlloc::GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats )
{
	if ( !ppMemoryStats )
		return 0;
	g_nMemStats = 0;

#if MEM_SBH_ENABLED
	{
		// Small block heap
		size_t SBHCommitted = 0, SBHAllocated = 0;
		size_t commitTmp, allocTmp;
#if MEM_SBH_ENABLED
		m_PrimarySBH.Usage( commitTmp, allocTmp );
		SBHCommitted += commitTmp; SBHAllocated += allocTmp;
#ifdef MEMALLOC_USE_SECONDARY_SBH
		m_SecondarySBH.Usage( commitTmp, allocTmp );
		SBHCommitted += commitTmp; SBHAllocated += allocTmp;
#endif // MEMALLOC_USE_SECONDARY_SBH
#ifndef MEMALLOC_NO_FALLBACK
		m_FallbackSBH.Usage( commitTmp, allocTmp );
		SBHCommitted += commitTmp; SBHAllocated += allocTmp;
#endif // MEMALLOC_NO_FALLBACK
#endif // MEM_SBH_ENABLED

		static size_t SBHMaxCommitted = 0; SBHMaxCommitted = MAX( SBHMaxCommitted, SBHCommitted );
		AddGenericMemoryStat( "SBH_cur", (int)SBHCommitted );
		AddGenericMemoryStat( "SBH_max", (int)SBHMaxCommitted );
	}
#endif // MEM_SBH_ENABLED

#if defined( USE_DLMALLOC )
#if !defined( MEMALLOC_REGIONS ) && defined( MEMALLOC_SEGMENT_MIXED )
	{
		// Medium block heap
		mallinfo infoMBH = mspace_mallinfo( g_AllocRegions[ 1 ] );
		size_t nMBHCurUsed = infoMBH.uordblks;// nMBH_WRONG_MaxUsed = infoMBH.usmblks; // TODO: figure out why dlmalloc mis-reports MBH max usage (it just returns the footprint)
		static size_t nMBHMaxUsed = 0; nMBHMaxUsed = MAX( nMBHMaxUsed, nMBHCurUsed );
		AddGenericMemoryStat( "MBH_cur", (int)nMBHCurUsed );
		AddGenericMemoryStat( "MBH_max", (int)nMBHMaxUsed );

		// Large block heap
		mallinfo infoLBH = mspace_mallinfo( g_AllocRegions[ 0 ] );
		size_t nLBHCurUsed = mspace_footprint( g_AllocRegions[ 0 ] ), nLBHMaxUsed = mspace_max_footprint( g_AllocRegions[ 0 ] ), nLBHArenaSize = infoLBH.arena, nLBHFree = infoLBH.fordblks;
		AddGenericMemoryStat( "LBH_cur", (int)nLBHCurUsed );
		AddGenericMemoryStat( "LBH_max", (int)nLBHMaxUsed );
		// LBH arena used+free (these are non-virtual allocations - there should be none, since we only allocate 256KB+ items in the LBH)
		// TODO: I currently see the arena grow to 320KB due to a larger allocation being realloced down... if this gets worse, add an 'ALWAYS use VMM' flag to the mspace.
		AddGenericMemoryStat( "LBH_arena", (int)nLBHArenaSize );
		AddGenericMemoryStat( "LBH_free",  (int)nLBHFree );
	}
#else // (!MEMALLOC_REGIONS && MEMALLOC_SEGMENT_MIXED)
	{
		// Single dlmalloc heap (TODO: per-DLL heap stats, if we resurrect that)
		mallinfo info = mspace_mallinfo(  g_AllocRegions[ 0 ] );
		AddGenericMemoryStat( "mspace_cur",  (int)info.uordblks );
		AddGenericMemoryStat( "mspace_max",  (int)info.usmblks );
		AddGenericMemoryStat( "mspace_size", (int)mspace_footprint( g_AllocRegions[ 0 ] ) );
	}
#endif // (!MEMALLOC_REGIONS && MEMALLOC_SEGMENT_MIXED)
#endif // USE_DLMALLOC

	size_t nMaxPhysMemUsed_Delta;
	nMaxPhysMemUsed_Delta = 0;
#ifdef _PS3
	{
		// System heap (should not exist!)
		static malloc_managed_size mms;
		(g_pMemOverrideRawCrtFns->pfn_malloc_stats)( &mms );
		if ( mms.current_system_size )
			AddGenericMemoryStat( "sys_heap",		(int)mms.current_system_size );

		// Virtual Memory Manager
		size_t nReserved = 0, nReservedMax = 0, nCommitted = 0, nCommittedMax = 0;
		extern void VirtualMemoryManager_GetStats( size_t &nReserved, size_t &nReservedMax, size_t &nCommitted, size_t &nCommittedMax );
		VirtualMemoryManager_GetStats( nReserved, nReservedMax, nCommitted, nCommittedMax );
		AddGenericMemoryStat( "VMM_reserved",		(int)nReserved );
		AddGenericMemoryStat( "VMM_reserved_max",	(int)nReservedMax );
		AddGenericMemoryStat( "VMM_committed",		(int)nCommitted );
		AddGenericMemoryStat( "VMM_committed_max",	(int)nCommittedMax );

		// Estimate memory committed by memory stacks (these account for all VMM allocations other than the SBH/MBH/LBH)
		size_t nHeapTotal = 1024*1024*MBYTES_PRIMARY_SBH;
#if defined( USE_DLMALLOC )
		for ( int i = 0; i < ARRAYSIZE(g_AllocRegions); i++ )
		{
			nHeapTotal += mspace_footprint( g_AllocRegions[i] );
		}
#endif // USE_DLMALLOC
		size_t nMemStackTotal = nCommitted - nHeapTotal;
		AddGenericMemoryStat( "MemStacks",	(int)nMemStackTotal );

		// On PS3, we can more accurately determine 'phys_free_min', since we know nCommittedMax
		// (otherwise nPhysFreeMin is only updated intermittently; when this function is called):
		nMaxPhysMemUsed_Delta = nCommittedMax - nCommitted;
	}
#endif // _PS3

#if defined( _GAMECONSOLE )
	// Total/free/min-free physical pages
	{
#if defined( _X360 )
		MEMORYSTATUS stat;
		::GlobalMemoryStatus( &stat );
		size_t nPhysTotal = stat.dwTotalPhys,       nPhysFree = stat.dwAvailPhys           - ExtraDevkitMemory();
#elif defined( _PS3 )
		static sys_memory_info stat;
		sys_memory_get_user_memory_size( &stat );
		size_t nPhysTotal = stat.total_user_memory, nPhysFree = stat.available_user_memory - ExtraDevkitMemory();
#endif // _X360/_PS3
		static size_t nPhysFreeMin = nPhysTotal;
		nPhysFreeMin = MIN( nPhysFreeMin, ( nPhysFree - nMaxPhysMemUsed_Delta ) );
		AddGenericMemoryStat( "phys_total",		(int)nPhysTotal );
		AddGenericMemoryStat( "phys_free",		(int)nPhysFree );
		AddGenericMemoryStat( "phys_free_min",	(int)nPhysFreeMin );
	}
#endif // _GAMECONSOLE

	*ppMemoryStats = &g_MemStats[0];
	return g_nMemStats;
}

void CStdMemAlloc::CompactHeap()
{
#if MEM_SBH_ENABLED
	if ( !m_CompactMutex.TryLock() )
	{
		return;
	}
	if ( m_bInCompact )
	{
		m_CompactMutex.Unlock();
		return;
	}

	m_bInCompact = true;
	size_t nBytesRecovered;
#ifndef MEMALLOC_NO_FALLBACK
	nBytesRecovered = m_FallbackSBH.Compact( false );
	if ( nBytesRecovered && IsGameConsole() )
	{
		Msg( "Compact freed %d bytes from virtual heap (up to 256k still committed)\n", nBytesRecovered );
	}
#endif // MEMALLOC_NO_FALLBACK
	nBytesRecovered = m_PrimarySBH.Compact( false );
#ifdef MEMALLOC_USE_SECONDARY_SBH
	nBytesRecovered += m_SecondarySBH.Compact( false );
#endif
	if ( nBytesRecovered && IsGameConsole() )
	{
		Msg( "Compact released %d bytes from the SBH\n", nBytesRecovered );
	}

	nBytesRecovered = compact_internal();
	if ( nBytesRecovered && IsGameConsole() )
	{
		Msg( "Compact released %d bytes from the mixed block heap\n", nBytesRecovered );
	}

	m_bInCompact = false;
	m_CompactMutex.Unlock();
#endif // MEM_SBH_ENABLED
}

void CStdMemAlloc::CompactIncremental()
{
#if MEM_SBH_ENABLED
	if ( !m_CompactMutex.TryLock() )
	{
		return;
	}
	if ( m_bInCompact )
	{
		m_CompactMutex.Unlock();
		return;
	}

	m_bInCompact = true;
#ifndef MEMALLOC_NO_FALLBACK
	m_FallbackSBH.Compact( true );
#endif
	m_PrimarySBH.Compact( true );
#ifdef MEMALLOC_USE_SECONDARY_SBH
	m_SecondarySBH.Compact( true );
#endif
	m_bInCompact = false;
	m_CompactMutex.Unlock();
#endif // MEM_SBH_ENABLED
}

MemAllocFailHandler_t CStdMemAlloc::SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler )
{
	MemAllocFailHandler_t pfnPrevious = m_pfnFailHandler;
	m_pfnFailHandler = pfnMemAllocFailHandler;
	return pfnPrevious;
}

size_t CStdMemAlloc::DefaultFailHandler( size_t nBytes )
{
	if ( IsX360() )
	{
#ifdef _X360 
		ExecuteOnce(
		{
			char buffer[256];
			_snprintf( buffer, sizeof( buffer ), "***** Memory pool overflow, attempted allocation size: %u (not a critical error)\n", nBytes );
			XBX_OutputDebugString( buffer ); 
		}
		);
#endif // _X360
	}
	return 0;
}

void CStdMemAlloc::SetStatsExtraInfo( const char *pMapName, const char *pComment )
{
}

void CStdMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed = nSize;

	DebuggerBreakIfDebugging();
#if defined( _PS3 ) && defined( _DEBUG )
	DebuggerBreak();
#endif // _PS3

	char buffer[256];
#ifdef COMPILER_GCC
	_snprintf( buffer, sizeof( buffer ), "***** OUT OF MEMORY! attempted allocation size: %u ****\n", nSize );
#else
	_snprintf( buffer, sizeof( buffer ), "***** OUT OF MEMORY! attempted allocation size: %u ****\n", nSize );
#endif // COMPILER_GCC

#ifdef _X360 
	XBX_OutputDebugString( buffer );
	if ( !Plat_IsInDebugSession() )
	{
		XBX_CrashDump( true );
#if defined( _DEMO )
		XLaunchNewImage( XLAUNCH_KEYWORD_DEFAULT_APP, 0 );
#else
		XLaunchNewImage( "default.xex", 0 );
#endif // _DEMO
	}
#elif defined(_WIN32 )
	OutputDebugString( buffer );
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
		abort();
	}
#else // _X360/_WIN32/other
	printf( "%s\n", buffer );
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
#if defined( _PS3 )
		DumpStats();
#endif
		Plat_ExitProcess( 0 );
	}
#endif // _X360/_WIN32/other

}

size_t CStdMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}

#endif // MEM_IMPL_TYPE_STD

#endif // STEAM
