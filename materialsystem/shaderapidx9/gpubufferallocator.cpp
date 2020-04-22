//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: See gpubufferallocator.h
//
// $NoKeywords: $
//
//===========================================================================//

#include "gpubufferallocator.h"
#include "dynamicvb.h"
#include "dynamicib.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#if defined( _X360 )



//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

#include "utlmap.h"
MEMALLOC_DEFINE_EXTERNAL_TRACKING( XMem_CGPUBufferPool );

// Track non-pooled VB/IB physical allocations (used by CGPUBufferAllocator::SpewStats)
CInterlockedInt g_NumIndividualVBPhysAllocs  = 0;
CInterlockedInt g_SizeIndividualVBPhysAllocs = 0;
CInterlockedInt g_NumIndividualIBPhysAllocs  = 0;
CInterlockedInt g_SizeIndividualIBPhysAllocs = 0;



//=============================================================================
//=============================================================================
// CGPUBufferAllocator
//=============================================================================
//=============================================================================

CGPUBufferAllocator::CGPUBufferAllocator( void )
 :	m_nBufferPools( 0 ),
	m_bEnabled( true )
{
	memset( &( m_BufferPools[ 0 ] ), 0, sizeof( m_BufferPools ) );

	m_bEnabled = USE_GPU_BUFFER_ALLOCATOR && !CommandLine()->FindParm( "-no_gpu_buffer_allocator" );
	if ( m_bEnabled )
	{
		// Start with one pool (the size should be the lowest-common-denominator for all maps)
		AllocatePool( INITIAL_POOL_SIZE );
	}
}

CGPUBufferAllocator::~CGPUBufferAllocator( void )
{
	for ( int i = 0; i < m_nBufferPools; i++ )
	{
		delete m_BufferPools[ i ];
	}
}

//-----------------------------------------------------------------------------
// Allocate a new memory pool
//-----------------------------------------------------------------------------
bool CGPUBufferAllocator::AllocatePool( int nPoolSize )
{
	if ( m_nBufferPools == MAX_POOLS )
		return false;

	m_BufferPools[ m_nBufferPools ] = new CGPUBufferPool( nPoolSize );
	if ( m_BufferPools[ m_nBufferPools ]->m_pMemory == NULL )
	{
		// Physical alloc failed! Continue without crashing, we *might* get away with it...
		ExecuteOnce( DebuggerBreakIfDebugging() );
		ExecuteNTimes( 15, Warning( "CGPUBufferAllocator::AllocatePool - physical allocation failed! Physical fragmentation is in bad shape... falling back to non-pooled VB/IB allocations. Brace for a crash :o/\n" ) );
		delete m_BufferPools[ m_nBufferPools ];
		m_BufferPools[ m_nBufferPools ] = NULL;
		return false;
	}
	m_nBufferPools++;
	return true;
}

//-----------------------------------------------------------------------------
// Make a new GPUBufferHandle_t to represent a given buffer allocation
//-----------------------------------------------------------------------------
inline GPUBufferHandle_t CGPUBufferAllocator::MakeGPUBufferHandle( int nPoolNum, int nPoolEntry )
{
	GPUBufferHandle_t newHandle;
	newHandle.nPoolNum   = nPoolNum;
	newHandle.nPoolEntry = nPoolEntry;
	newHandle.pMemory    = m_BufferPools[ nPoolNum ]->m_pMemory + m_BufferPools[ nPoolNum ]->m_PoolEntries[ nPoolEntry ].nOffset;
	return newHandle;
}

//-----------------------------------------------------------------------------
// Try to allocate a block of the given size from one of our pools
//-----------------------------------------------------------------------------
bool CGPUBufferAllocator::AllocateBuffer( GPUBufferHandle_t *pHandle, int nBufferSize, void *pObject, bool bIsVertexBuffer )
{
	if ( m_bEnabled && ( nBufferSize <= MAX_BUFFER_SIZE ) )
	{
		// Try to allocate at the end of one of our pools
		for ( int nPool = 0; nPool < m_nBufferPools; nPool++ )
		{
			int nPoolEntry = m_BufferPools[ nPool ]->Allocate( nBufferSize, bIsVertexBuffer, pObject );
			if ( nPoolEntry >= 0 )
			{
				// Tada.
				*pHandle = MakeGPUBufferHandle( nPool, nPoolEntry );
				return true;
			}
			if ( nPool == ( m_nBufferPools - 1 ) )
			{
				// Allocate a new pool (in which this buffer should DEFINITELY fit!)
				COMPILE_TIME_ASSERT( ADDITIONAL_POOL_SIZE >= MAX_BUFFER_SIZE );
				AllocatePool( ADDITIONAL_POOL_SIZE );
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Clear the given allocation from our pools (NOTE: the memory cannot be reused until Defrag() is called)
//-----------------------------------------------------------------------------
void CGPUBufferAllocator::DeallocateBuffer( const GPUBufferHandle_t *pHandle )
{
	Assert( pHandle );
	if ( pHandle )
	{
		Assert( ( pHandle->nPoolNum >= 0 ) && ( pHandle->nPoolNum < m_nBufferPools ) );
		if ( ( pHandle->nPoolNum >= 0 ) && ( pHandle->nPoolNum < m_nBufferPools ) )
		{
			m_BufferPools[ pHandle->nPoolNum ]->Deallocate( pHandle );
		}
	}
}

//-----------------------------------------------------------------------------
// If appropriate, allocate this VB's memory from one of our pools
//-----------------------------------------------------------------------------
bool CGPUBufferAllocator::AllocateVertexBuffer( CVertexBuffer *pVertexBuffer, int nBufferSize )
{
	AUTO_LOCK( m_mutex );

	bool bIsVertexBuffer = true;
	GPUBufferHandle_t handle;
	if ( AllocateBuffer( &handle, nBufferSize, (void *)pVertexBuffer, bIsVertexBuffer ) )
	{
		// Success - give the VB the handle to this allocation
		pVertexBuffer->SetBufferAllocationHandle( handle );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Deallocate this VB's memory from our pools
//-----------------------------------------------------------------------------
void CGPUBufferAllocator::DeallocateVertexBuffer( CVertexBuffer *pVertexBuffer )
{
	AUTO_LOCK( m_mutex );

	// Remove the allocation from the pool and clear the VB's handle
	DeallocateBuffer( pVertexBuffer->GetBufferAllocationHandle() );
	pVertexBuffer->SetBufferAllocationHandle( GPUBufferHandle_t() );
}

//-----------------------------------------------------------------------------
// If appropriate, allocate this IB's memory from one of our pools
//-----------------------------------------------------------------------------
bool CGPUBufferAllocator::AllocateIndexBuffer( CIndexBuffer *pIndexBuffer, int nBufferSize )
{
	AUTO_LOCK( m_mutex );

	bool bIsNOTVertexBuffer = false;
	GPUBufferHandle_t handle;
	if ( AllocateBuffer( &handle, nBufferSize, (void *)pIndexBuffer, bIsNOTVertexBuffer ) )
	{
		// Success - give the IB the handle to this allocation
		pIndexBuffer->SetBufferAllocationHandle( handle );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Deallocate this IB's memory from our pools
//-----------------------------------------------------------------------------
void CGPUBufferAllocator::DeallocateIndexBuffer( CIndexBuffer *pIndexBuffer )
{
	AUTO_LOCK( m_mutex );

	// Remove the allocation from the pool and clear the IB's handle
	DeallocateBuffer( pIndexBuffer->GetBufferAllocationHandle() );
	pIndexBuffer->SetBufferAllocationHandle( GPUBufferHandle_t() );
}

//-----------------------------------------------------------------------------
// Move a buffer from one location to another (could be movement within the same pool)
//-----------------------------------------------------------------------------
void CGPUBufferAllocator::MoveBufferMemory( int nDstPool, int *pnDstEntry, int *pnDstOffset, CGPUBufferPool &srcPool, GPUBufferPoolEntry_t &srcEntry )
{
	// Move the data
	CGPUBufferPool &dstPool = *m_BufferPools[ nDstPool ];
	byte *pDest   = dstPool.m_pMemory + *pnDstOffset;
	byte *pSource = srcPool.m_pMemory + srcEntry.nOffset;
	if ( pDest != pSource )
		V_memmove( pDest, pSource, srcEntry.nSize );

	// Update the destination pool's allocation entry (NOTE: this could be srcEntry, so srcEntry.nOffset would change)
	dstPool.m_PoolEntries[ *pnDstEntry ]         = srcEntry;
	dstPool.m_PoolEntries[ *pnDstEntry ].nOffset = *pnDstOffset;

	// Tell the VB/IB about the updated allocation
	GPUBufferHandle_t newHandle = MakeGPUBufferHandle( nDstPool, *pnDstEntry );
	if ( srcEntry.bIsVertexBuffer )
		srcEntry.pVertexBuffer->SetBufferAllocationHandle( newHandle );
	else
		srcEntry.pIndexBuffer->SetBufferAllocationHandle( newHandle );

	// Move the write address past this entry and increment the pool high water mark
	*pnDstOffset += srcEntry.nSize;
	*pnDstEntry  += 1;
	dstPool.m_nBytesUsed += srcEntry.nSize;
}

//-----------------------------------------------------------------------------
// Reclaim space freed by destroyed buffers and compact our pools ready for new allocations
//-----------------------------------------------------------------------------
void CGPUBufferAllocator::Compact( void )
{
	// NOTE: this must only be called during map transitions, no rendering must be in flight and everything must be single-threaded!
	AUTO_LOCK( m_mutex );

	// SpewStats(); // pre-compact state

	CFastTimer timer;
	timer.Start();

	// Shuffle all pools to get rid of the empty space occupied by freed buffers.
	// We just walk the pools and entries in order, moving each buffer down within the same pool,
	// or to the end of a previous pool (if, after compaction, it now has free space).
	// Each pool should end up with contiguous, usable free space (may be zero bytes) at the end.
	int nDstPool = 0, nDstEntry = 0, nDstOffset = 0;
	for ( int nSrcPool = 0; nSrcPool < m_nBufferPools; nSrcPool++ )
	{
		CGPUBufferPool &srcPool = *m_BufferPools[ nSrcPool ];
		srcPool.m_nBytesUsed = 0; // Re-fill each pool from scratch
		int nEntriesRemainingInPool = 0;
		for ( int nSrcEntry = 0; nSrcEntry < srcPool.m_PoolEntries.Count(); nSrcEntry++ )
		{
			GPUBufferPoolEntry_t &srcEntry = srcPool.m_PoolEntries[ nSrcEntry ];
			if ( srcEntry.pVertexBuffer )
			{
				// First, try to move the buffer into one of the previous (already-compacted) pools
				bool bDone = false;
				while ( nDstPool < nSrcPool )
				{
					CGPUBufferPool &dstPool = *m_BufferPools[ nDstPool ];
					if ( ( nDstOffset + srcEntry.nSize ) <= dstPool.m_nSize )
					{
						// Add this buffer to the end of dstPool
						Assert( nDstEntry == dstPool.m_PoolEntries.Count() );
						dstPool.m_PoolEntries.AddToTail();
						MoveBufferMemory( nDstPool, &nDstEntry, &nDstOffset, srcPool, srcEntry );
						bDone = true;
						break;
					}
					else
					{
						// This pool is full, start writing into the next one
						nDstPool++;
						nDstEntry = 0;
						nDstOffset = 0;
					}
				}

				// If that fails, just shuffle the entry down within srcPool
				if ( !bDone )
				{
					Assert( nSrcPool == nDstPool );
					MoveBufferMemory( nDstPool, &nDstEntry, &nDstOffset, srcPool, srcEntry );
					nEntriesRemainingInPool++;
				}
			}
		}

		// Discard unused entries from the end of the pool (freed buffers, or buffers moved to other pools)
		srcPool.m_PoolEntries.SetCountNonDestructively( nEntriesRemainingInPool );
	}

	// Now free empty pools (keep the first (very large) one around, since fragmentation makes freeing+reallocing it a big risk)
	int nBytesFreed = 0;
	for ( int nPool = ( m_nBufferPools - 1 ); nPool > 0; nPool-- )
	{
		if ( m_BufferPools[ nPool ]->m_PoolEntries.Count() )
			break;

		nBytesFreed += m_BufferPools[ nPool ]->m_nSize;
		Assert( m_BufferPools[ nPool ]->m_nBytesUsed == 0 );
		delete m_BufferPools[ nPool ];
		m_nBufferPools--;
	}

	if ( m_nBufferPools > 1 )
	{
		// The above compaction algorithm could waste space due to large allocs causing nDstPool to increment before that pool
		// is actually full. With our current usage pattern (total in-use memory is less than INITIAL_POOL_SIZE, whenever Compact
		// is called), that doesn't matter. If that changes (i.e. the below warning fires), then the fix would be:
		//  - for each pool, sort its entries by size (largest first) and try to allocate them on the end of prior (already-compacted) pools
		//  - pack whatever remains in the pool down, and proceed to the next pool
		ExecuteOnce( Warning( "CGPUBufferAllocator::Compact may be wasting memory due to changed usage patterns (see code for suggested fix)." ) );
	}

#ifdef _X360
	// Invalidate the GPU caches for all pooled memory, since stuff has moved around
	for ( int nPool = 0; nPool < m_nBufferPools; nPool++ )
	{
		Dx9Device()->InvalidateGpuCache( m_BufferPools[ nPool ]->m_pMemory, m_BufferPools[ nPool ]->m_nSize, 0 );
	}
#endif

	timer.End();
	float compactTime = (float)timer.GetDuration().GetSeconds();
	Msg( "CGPUBufferAllocator::Compact took %.2f seconds, and freed %.1fkb\n", compactTime, ( nBytesFreed / 1024.0f ) );

	// SpewStats(); // post-compact state
}

//-----------------------------------------------------------------------------
// Spew statistics about pool usage, so we can tune our constant values
//-----------------------------------------------------------------------------
void CGPUBufferAllocator::SpewStats( bool bBrief )
{
	AUTO_LOCK( m_mutex );

	int nMemAllocated   = 0;
	int nMemUsed        = 0;
	int nOldMemWasted   = 0;
	int nVBsInPools     = 0;
	int nIBsInPools     = 0;
	int nFreedBuffers   = 0;
	int nFreedBufferMem = 0;
	for ( int i = 0; i < m_nBufferPools; i++ )
	{
		CGPUBufferPool *pool = m_BufferPools[ i ];
		nMemAllocated += pool->m_nSize;
		nMemUsed      += pool->m_nBytesUsed;
		for ( int j = 0; j < pool->m_PoolEntries.Count(); j++ )
		{
			GPUBufferPoolEntry_t &poolEntry = pool->m_PoolEntries[ j ];
			if ( poolEntry.pVertexBuffer )
			{
				// Figure out how much memory we WOULD have allocated for this buffer, if we'd allocated it individually:
				nOldMemWasted += ALIGN_VALUE( poolEntry.nSize, 4096 ) - poolEntry.nSize;
				if (  poolEntry.bIsVertexBuffer ) nVBsInPools++;
				if ( !poolEntry.bIsVertexBuffer ) nIBsInPools++;
			}
			else
			{
				nFreedBuffers++;
				nFreedBufferMem += poolEntry.nSize;
			}
		}
	}

	// NOTE: 'unused' memory doesn't count memory used by freed buffers, which should be zero during gameplay. The purpose is
	//       to measure wastage at the END of a pool, to help determine ideal values for ADDITIONAL_POOL_SIZE and MAX_BUFFER_SIZE.
	int nMemUnused = nMemAllocated - nMemUsed;

	const float KB = 1024.0f, MB = KB*KB;
	if ( bBrief )
	{
		ConMsg( "[GPUBUFLOG] Pools:%2d | Size:%5.1fMB | Unused:%5.1fMB | Freed:%5.1fMB | Unpooled:%5.1fMB\n",
				m_nBufferPools, nMemAllocated / MB, nMemUnused / MB, nFreedBufferMem / MB, ( g_SizeIndividualVBPhysAllocs + g_SizeIndividualIBPhysAllocs ) / MB );
	}
	else
	{
		Msg( "\nGPU Buffer Allocator stats:\n" );
		Msg( " -- %5d     -- Num Pools allocated\n", m_nBufferPools );
		Msg( " -- %7.1fMB -- Memory allocated to pools\n", nMemAllocated / MB );
		Msg( " -- %7.1fkb -- Unused memory at tail-end of pools\n", nMemUnused / KB );
		Msg( " -- %7.1fkb -- Memory saved by allocating buffers from pools\n", nOldMemWasted / KB );
		Msg( " -- %5d     -- Number of VBs allocated from pools\n", nVBsInPools );
		Msg( " -- %5d     -- Number of IBs allocated from pools\n", nIBsInPools );
		Msg( " -- %5d     -- Number of freed buffers in pools (should be zero during gameplay)\n", nFreedBuffers );
		Msg( " -- %7.1fkb -- Memory used by freed buffers in pools\n", nFreedBufferMem / KB );
		Msg( " -- %7.1fkb -- Mem allocated for NON-pooled VBs (%d VBs)\n", g_SizeIndividualVBPhysAllocs / KB, g_NumIndividualVBPhysAllocs );
		Msg( " -- %7.1fkb -- Mem allocated for NON-pooled IBs (%d IBs)\n", g_SizeIndividualIBPhysAllocs / KB, g_NumIndividualVBPhysAllocs );
		Msg( "\n" );
	}
}


//=============================================================================
//=============================================================================
// CGPUBufferPool
//=============================================================================
//=============================================================================

CGPUBufferPool::CGPUBufferPool( int nSize )
 :	m_PoolEntries( POOL_ENTRIES_GROW_SIZE, POOL_ENTRIES_INIT_SIZE ),
	m_nSize( 0 ),
	m_nBytesUsed( 0 )
{
	// NOTE: write-combining (PAGE_WRITECOMBINE) is deliberately not used, since it slows down 'Compact' hugely (and doesn't noticeably benefit load times)
	m_pMemory = (byte*)XPhysicalAlloc( nSize, MAXULONG_PTR, 0, PAGE_READWRITE );
	if ( m_pMemory )
	{
		MemAlloc_RegisterExternalAllocation( XMem_CGPUBufferPool, m_pMemory, XPhysicalSize( m_pMemory ) );
		m_nSize = nSize;
	}
}

CGPUBufferPool::~CGPUBufferPool( void )
{
	for ( int i = 0; i < m_PoolEntries.Count(); i++ )
	{
		if ( m_PoolEntries[ i ].pVertexBuffer )
		{
			// Buffers should be cleaned up before the CGPUBufferAllocator is shut down!
			Assert( 0 );
			Warning( "ERROR: Un-freed %s in CGPUBufferPool on shut down! (%6.1fKB\n",
					( m_PoolEntries[ i ].bIsVertexBuffer ? "VB" : "IB" ), ( m_PoolEntries[ i ].nSize / 1024.0f ) );
			break;
		}
	}

	if ( m_pMemory )
	{
		MemAlloc_RegisterExternalDeallocation( XMem_CGPUBufferPool, m_pMemory, XPhysicalSize( m_pMemory ) );
		XPhysicalFree( m_pMemory );
		m_pMemory = 0;
	}

	m_nSize = m_nBytesUsed = 0;
}

//-----------------------------------------------------------------------------
// Attempt to allocate a buffer of the given size in this pool
//-----------------------------------------------------------------------------
int CGPUBufferPool::Allocate( int nBufferSize, bool bIsVertexBuffer, void *pObject )
{
	// Align the buffer size
	nBufferSize = ALIGN_VALUE( nBufferSize, POOL_ENTRY_ALIGNMENT );

	// Check available space
	if ( ( m_nBytesUsed + nBufferSize ) > m_nSize )
		return -1;

	int nPoolEntry = m_PoolEntries.AddToTail();
	GPUBufferPoolEntry_t &poolEntry = m_PoolEntries[ nPoolEntry ];
	poolEntry.nOffset			= m_nBytesUsed;
	poolEntry.nSize				= nBufferSize;
	poolEntry.bIsVertexBuffer	= bIsVertexBuffer;
	poolEntry.pVertexBuffer		= (CVertexBuffer *)pObject;

	// Update 'used space' high watermark
	m_nBytesUsed += nBufferSize;

	return nPoolEntry;
}

//-----------------------------------------------------------------------------
// Deallocate the given entry from this pool
//-----------------------------------------------------------------------------
void CGPUBufferPool::Deallocate( const GPUBufferHandle_t *pHandle )
{
	Assert( m_PoolEntries.IsValidIndex( pHandle->nPoolEntry ) );
	if ( m_PoolEntries.IsValidIndex( pHandle->nPoolEntry ) )
	{
		Assert( m_PoolEntries[ pHandle->nPoolEntry ].pVertexBuffer );
		m_PoolEntries[ pHandle->nPoolEntry ].pVertexBuffer = NULL;
	}
}

#endif // _X360
