//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CGPUBufferAllocator manages allocation of VBs/IBs from shared memory pools.
//          Avoids 4KB physical alloc alignment overhead per VB/IB.
//
// $NoKeywords: $
//
//===========================================================================//


#ifndef GPUBUFFERALLOCATOR_H
#define GPUBUFFERALLOCATOR_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _X360

#include "tier1/utlvector.h"
#include "tier1/convar.h"

class CVertexBuffer;
class CIndexBuffer;


// Only active on X360 atm
#define USE_GPU_BUFFER_ALLOCATOR ( IsX360() )


//-----------------------------------------------------------------------------
// A handle to a buffer pool allocation, held by the allocated VB/IB
//-----------------------------------------------------------------------------
struct GPUBufferHandle_t
{
	GPUBufferHandle_t( void ) : nPoolNum( -1 ), pMemory( NULL ), nPoolEntry( -1 ) {}
	bool IsValid( void ) { return ( pMemory != NULL ); }
	byte *	pMemory;		// Physical address of the allocation
	int		nPoolNum;		// Identifies the pool
	int		nPoolEntry;		// Identifies this allocation within the pool
};

//-----------------------------------------------------------------------------
// Describes an entry in a CGPUBufferPool
//-----------------------------------------------------------------------------
struct GPUBufferPoolEntry_t
{
	int  nOffset;
	int  nSize;
	bool bIsVertexBuffer;
	union
	{
		// These are set to NULL by CGPUBufferPool::Free() (called when the VB/IB is destroyed)
		CVertexBuffer *pVertexBuffer;
		CIndexBuffer  *pIndexBuffer;
	};
};

//-----------------------------------------------------------------------------
// A single memory block out of which individual VBs/IBs are allocated
//-----------------------------------------------------------------------------
class CGPUBufferPool
{
public:
	CGPUBufferPool( int nSize );
	virtual ~CGPUBufferPool( void );

	// Returns the index (-1 on failure) of a new allocation in the pool, for a buffer of the given size.
	int Allocate( int nSize, bool bIsVertexBuffer, void *pObject );
	// Frees a given entry (just marks it as freed, the memory will not be reused by Allocate() until CGPUBufferAllocator::Defrag() is called )
	void Deallocate( const GPUBufferHandle_t *pHandle );

private:
	// NOTE: these values are specialized for X360 and should be #ifdef'd for other target platforms
	static const int POOL_ENTRIES_INIT_SIZE = 256;
	static const int POOL_ENTRIES_GROW_SIZE = 256;
	static const int POOL_ENTRY_ALIGNMENT	= 4;	// 4-byte alignment required for VB/IB data on XBox360

	byte *							m_pMemory;		// Pointer to the (physical) address of the pool's memory
	int								m_nSize;		// Total size of the pool
	int								m_nBytesUsed;	// High watermark of used memory in the pool
	CUtlVector<GPUBufferPoolEntry_t>m_PoolEntries;	// Memory-order array of items allocated in the pool

	// CGPUBufferAllocator is a friend so that CGPUBufferAllocator::Defrag() can shuffle allocations around
	friend class CGPUBufferAllocator;
};

//-----------------------------------------------------------------------------
// Manages a set of memory blocks out of which individual VBs/IBs are allocated
//-----------------------------------------------------------------------------
class CGPUBufferAllocator
{
public:
	CGPUBufferAllocator( void );
	virtual ~CGPUBufferAllocator( void );

	// (De)Allocates memory for a vertex/index buffer:
	bool AllocateVertexBuffer(   CVertexBuffer *pVertexBuffer, int nBufferSize );
	bool AllocateIndexBuffer(    CIndexBuffer  *pIndexBuffer,  int nBufferSize  );
	void DeallocateVertexBuffer( CVertexBuffer *pVertexBuffer );
	void DeallocateIndexBuffer(  CIndexBuffer  *pIndexBuffer  );

	// Compact memory to account for freed buffers
	// NOTE: this must only be called during map transitions, no rendering must be in flight and everything must be single-threaded!
	void Compact();

	// Spew statistics about pooled buffer allocations
	void SpewStats( bool bBrief = false );


private:
	// NOTE: these values are specialized for X360 and should be #ifdef'd for other target platforms
	static const int INITIAL_POOL_SIZE		= 57*1024*1024 + 256*1024;
	static const int ADDITIONAL_POOL_SIZE	= 2*1024*1024;
	static const int MAX_POOLS				= 8;
	static const int MAX_BUFFER_SIZE		= ADDITIONAL_POOL_SIZE; // 256*1024;


	// Allocate a new CGPUBufferPool
	bool AllocatePool( int nPoolSize );
	// Allocate/deallocate a buffer (type-agnostic)
	bool AllocateBuffer(   GPUBufferHandle_t *pHandle, int nBufferSize, void *pObject, bool bIsVertexBuffer );
	void DeallocateBuffer( const GPUBufferHandle_t *pHandle );
	// Make a handle for a given allocation
	GPUBufferHandle_t MakeGPUBufferHandle( int nPoolNum, int nPoolEntry );
	// Helper for Compact
	void MoveBufferMemory( int nDstPool, int *pnDstEntry, int *pnDstOffset, CGPUBufferPool &srcPool, GPUBufferPoolEntry_t &srcEntry );


	CGPUBufferPool *m_BufferPools[ MAX_POOLS ];
	int				m_nBufferPools;
	bool			m_bEnabled;

	CThreadFastMutex m_mutex;
};


// Track non-pooled physallocs, to help tune CGPUBufferAllocator usage:
extern CInterlockedInt g_NumIndividualIBPhysAllocs;
extern CInterlockedInt g_SizeIndividualIBPhysAllocs;
extern CInterlockedInt g_NumIndividualVBPhysAllocs;
extern CInterlockedInt g_SizeIndividualVBPhysAllocs;

#endif // _X360

#endif // GPUBUFFERALLOCATOR_H
