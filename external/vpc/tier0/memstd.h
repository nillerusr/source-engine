//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
#include "pch_tier0.h"

#if IS_WINDOWS_PC
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>
#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#elif defined( _X360 )
#undef Verify
#define _XBOX
#include <xtl.h>
#undef _XBOX
#include "xbox/xbox_win32stubs.h"
#define VA_COMMIT_FLAGS (MEM_COMMIT|MEM_NOZERO|MEM_LARGE_PAGES)
#define VA_RESERVE_FLAGS (MEM_RESERVE|MEM_LARGE_PAGES)
#elif defined( _PS3 )
#include "sys/memory.h"
#include "sys/mempool.h"
#include "sys/process.h"
#include <sys/vm.h>

#endif

//#include <malloc.h>
#include <algorithm>
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"
#include "mem_helpers.h"

#ifndef _PS3
#pragma pack(4)
#endif

#define MIN_SBH_BLOCK	8
#define MIN_SBH_ALIGN	8
#define MAX_SBH_BLOCK	2048
#define MAX_POOL_REGION (4*1024*1024)


#define NUM_POOLS		42

#if defined( _WIN32 ) || defined( _PS3 )
// FIXME: Disable small block heap on win64 for now; it's busted because
// it's expecting SLIST_HEADER to look different than it does on win64
#if !defined( PLATFORM_WINDOWS_PC64 )
#define MEM_SBH_ENABLED 1
#endif
#endif

#if !defined(_CERT) && ( defined(_X360) || defined(_PS3) )
#define TRACK_SBH_COUNTS
#endif

#if defined(_X360)

// 360 uses a 48MB primary (physical) SBH and 10MB secondary (virtual) SBH, with no fallback
#define MBYTES_PRIMARY_SBH 48
#define MEMALLOC_USE_SECONDARY_SBH
#define MBYTES_SECONDARY_SBH 10
#define MEMALLOC_NO_FALLBACK

#elif defined(_PS3)

// PS3 uses just a 32MB SBH - this was enough to avoid overflow when Portal 2 shipped.
// NOTE: when Steam uses the game's tier0 allocator (see memalloc.h), we increase the size
//       of the SBH and MBH (see memstd.cpp) to accommodate those extra allocations.
#define MBYTES_PRIMARY_SBH ( 32 + MBYTES_STEAM_SBH_USAGE )
#define MEMALLOC_NO_FALLBACK

#else // _X360 | _PS3

// Other platforms use a 48MB primary SBH and a (32MB) fallback SBH
#define MBYTES_PRIMARY_SBH 48

#endif // _X360 | _PS3

#define MEMSTD_COMPILE_TIME_ASSERT( pred )	switch(0){case 0:case pred:;}

//-----------------------------------------------------------------------------
// Small block pool
//-----------------------------------------------------------------------------

class CFreeList : public CTSListBase
{
public:
	void Push( void *p )	{ CTSListBase::Push( (TSLNodeBase_t *)p ); }
	byte *Pop()				{ return (byte *)CTSListBase::Pop(); }
};

template <typename CAllocator>
class CSmallBlockHeap;

template <typename CAllocator>
class CSmallBlockPool
{
public:
	CSmallBlockPool()
	{
		m_nBlockSize = 0;
		m_nCommittedPages = 0;
		m_pFirstPage = NULL;
	}

	void Init( unsigned nBlockSize );
	size_t GetBlockSize();
	void *Alloc();
	void Free( void *p );
	int CountFreeBlocks();
	int GetCommittedSize();
	int CountCommittedBlocks();
	int CountAllocatedBlocks();
	size_t Compact( bool bIncremental );
	bool Validate();

	enum
	{
		BYTES_PAGE = CAllocator::BYTES_PAGE,
		NOT_COMMITTED = -1
	};

private:
	typedef CSmallBlockHeap<CAllocator> CHeap;
	friend class CSmallBlockHeap<CAllocator>;

	struct PageStatus_t : public TSLNodeBase_t
	{
		PageStatus_t()
		{
			m_pPool = NULL;
			m_nAllocated = NOT_COMMITTED;
			m_pNextPageInPool = NULL;
		}

		CSmallBlockPool<CAllocator> *	m_pPool;
		PageStatus_t *					m_pNextPageInPool;
		CInterlockedInt					m_nAllocated;
		CTSListBase						m_SortList;
	};

	struct SharedData_t
	{
		CAllocator			m_Allocator;
		CTSListBase			m_FreePages;
		CThreadSpinRWLock	m_Lock;
		PageStatus_t		m_PageStatus[CAllocator::TOTAL_BYTES/CAllocator::BYTES_PAGE];
		byte *				m_pNextBlock;
		byte *				m_pBase;
		byte *				m_pLimit;
	};

	static int PageSort( const void *p1, const void *p2 ) ;
	bool RemovePagesFromFreeList( byte **pPages, int nPages, bool bSortList );

	void ValidateFreelist( SharedData_t *pSharedData );

	CFreeList				m_FreeList;

	CInterlockedPtr<byte>	m_pNextAlloc;

	PageStatus_t *			m_pFirstPage;
	unsigned				m_nBlockSize;
	unsigned				m_nCommittedPages;

	CThreadFastMutex		m_CommitMutex;

#ifdef TRACK_SBH_COUNTS
	CInterlockedInt			m_nFreeBlocks;
#endif

	static SharedData_t *GetSharedData()
	{
		return &gm_SharedData;
	}

	static SharedData_t gm_SharedData;
};

//-----------------------------------------------------------------------------
// Small block heap (multi-pool)
//-----------------------------------------------------------------------------

template <typename CAllocator>
class CSmallBlockHeap
{
public:
	CSmallBlockHeap();
	bool ShouldUse( size_t nBytes );
	bool IsOwner( void * p );
	void *Alloc( size_t nBytes );
	void *Realloc( void *p, size_t nBytes );
	void Free( void *p );
	size_t GetSize( void *p );
	void DumpStats( const char *pszTag, FILE *pFile = NULL );
	void Usage( size_t &bytesCommitted, size_t &bytesAllocated );
	size_t Compact( bool bIncremental );
	bool Validate();

	enum
	{
		BYTES_PAGE = CAllocator::BYTES_PAGE
	};

private:
	typedef CSmallBlockPool<CAllocator> CPool;
	typedef struct CSmallBlockPool<CAllocator>::SharedData_t SharedData_t;

	CPool *FindPool( size_t nBytes );
	CPool *FindPool( void *p );

	// Map size to a pool address to a pool
	CPool *m_PoolLookup[MAX_SBH_BLOCK >> 2];
	CPool m_Pools[NUM_POOLS];

	SharedData_t *m_pSharedData;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CStdMemAlloc : public IMemAlloc
{
public:
	CStdMemAlloc();

	// Internal versions
	void *InternalAlloc( int region, size_t nSize );
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	void *InternalAllocAligned( int region, size_t nSize, size_t align );
#endif
	void *InternalAllocFromPools( size_t nSize );
	void *InternalRealloc( void *pMem, size_t nSize );
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	void *InternalReallocAligned( void *pMem, size_t nSize, size_t align );
#endif
	void  InternalFree( void *pMem );

	void CompactOnFail();

	// Release versions
	virtual void *Alloc( size_t nSize );
	virtual void *Realloc( void *pMem, size_t nSize );
	virtual void  Free( void *pMem );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize );

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, int nLine );
    virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine );
    virtual void  Free( void *pMem, const char *pFileName, int nLine );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine );

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	virtual void *AllocAlign( size_t nSize, size_t align );
	virtual void *AllocAlign( size_t nSize, size_t align, const char *pFileName, int nLine );
	virtual void *ReallocAlign( void *pMem, size_t nSize, size_t align );
	virtual void *ReallocAlign( void *pMem, size_t nSize, size_t align, const char *pFileName, int nLine );
#endif

	virtual void *RegionAlloc( int region, size_t nSize );
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine );

	// Returns size of a particular allocation
	virtual size_t GetSize( void *pMem );

    // Force file + line information for an allocation
    virtual void PushAllocDbgInfo( const char *pFileName, int nLine );
    virtual void PopAllocDbgInfo();

	virtual int32 CrtSetBreakAlloc( int32 lNewBreakAlloc );
	virtual	int CrtSetReportMode( int nReportType, int nReportMode );
	virtual int CrtIsValidHeapPointer( const void *pMem );
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access );
	virtual int CrtCheckMemory( void );
	virtual int CrtSetDbgFlag( int nNewFlag );
	virtual void CrtMemCheckpoint( _CrtMemState *pState );
	void* CrtSetReportFile( int nRptType, void* hFile );
	void* CrtSetReportHook( void* pfnNewHook );
	int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg );
	virtual int heapchk();

	virtual void DumpStats();
	virtual void DumpStatsFileBase( char const *pchFileBase );
	virtual size_t ComputeMemoryUsedBy( char const *pchSubStr );
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory );

	virtual bool IsDebugHeap() { return false; }

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) {}
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) {}
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) {}

	virtual int GetVersion() { return MEMALLOC_VERSION; }

	virtual void OutOfMemory( size_t nBytesAttempted = 0 ) { SetCRTAllocFailed( nBytesAttempted ); }

	virtual IVirtualMemorySection * AllocateVirtualMemorySection( size_t numMaxBytes );

	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats );

	virtual void CompactHeap();
	virtual void CompactIncremental(); 

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler );
	size_t CallAllocFailHandler( size_t nBytes ) { return (*m_pfnFailHandler)( nBytes); }

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	static size_t DefaultFailHandler( size_t );
	void DumpBlockStats( void *p ) {}

#if MEM_SBH_ENABLED
	class CVirtualAllocator
	{
	public:
		enum
		{
			BYTES_PAGE			= (64*1024),
			TOTAL_BYTES			= (32*1024*1024),
			MIN_RESERVE_PAGES	= 4,
		};

		byte *AllocatePoolMemory()
		{
#ifdef _WIN32
			return (byte *)VirtualAlloc( NULL, TOTAL_BYTES, VA_RESERVE_FLAGS, PAGE_NOACCESS );
#elif defined( _PS3 )
			Error( "" );
			return NULL;
#else
#error
#endif
		}

		bool IsVirtual()
		{
			return true;
		}

		bool Decommit( void *pPage )
		{
#ifdef _WIN32
			return ( VirtualFree( pPage, BYTES_PAGE, MEM_DECOMMIT ) != 0 );
#elif defined( _PS3 )
			return false;
#else
#error
#endif
		}

		bool Commit( void *pPage )
		{
#ifdef _WIN32
			return ( VirtualAlloc( pPage, BYTES_PAGE, VA_COMMIT_FLAGS, PAGE_READWRITE ) != NULL );
#elif defined( _PS3 )
			return false;
#else
#error
#endif
		}
	};

	typedef CSmallBlockHeap<CVirtualAllocator> CVirtualSmallBlockHeap;

	template <size_t SIZE_MB, bool bPhysical>
	class CFixedAllocator
	{
	public:
		enum
		{
			BYTES_PAGE			= (16*1024),
			TOTAL_BYTES			= (SIZE_MB*1024*1024),
			MIN_RESERVE_PAGES	= TOTAL_BYTES/BYTES_PAGE,
		};

		byte *AllocatePoolMemory()
		{
#ifdef _WIN32
#ifdef _X360
			if ( bPhysical )
				return (byte *)XPhysicalAlloc( TOTAL_BYTES, MAXULONG_PTR, 4096, PAGE_READWRITE | MEM_16MB_PAGES );
#endif
			return (byte *)VirtualAlloc( NULL, TOTAL_BYTES, VA_COMMIT_FLAGS, PAGE_READWRITE );
#elif defined( _PS3 )
			// TODO: release this section on shutdown (use GetMemorySectionForAddress)
			extern IVirtualMemorySection * VirtualMemoryManager_AllocateVirtualMemorySection( size_t numMaxBytes );
			IVirtualMemorySection *pSection = VirtualMemoryManager_AllocateVirtualMemorySection( TOTAL_BYTES );
			if ( !pSection )
				Error( "CFixedAllocator::AllocatePoolMemory() failed in VirtualMemoryManager_AllocateVirtualMemorySection\n" );
			if ( !pSection->CommitPages( pSection->GetBaseAddress(), TOTAL_BYTES ) )
				Error( "CFixedAllocator::AllocatePoolMemory() failed in IVirtualMemorySection::CommitPages\n" );
			return reinterpret_cast<byte *>( pSection->GetBaseAddress() );
#else
#error
#endif
		}

		bool IsVirtual()
		{
			return false;
		}

		bool Decommit( void *pPage )
		{
			return false;
		}

		bool Commit( void *pPage )
		{
			return false;
		}
	};

	typedef CSmallBlockHeap<CFixedAllocator< MBYTES_PRIMARY_SBH, true> > CFixedSmallBlockHeap;
#ifdef MEMALLOC_USE_SECONDARY_SBH
	typedef CSmallBlockHeap<CFixedAllocator< MBYTES_SECONDARY_SBH, false> > CFixedVirtualSmallBlockHeap; // @TODO: move back into above heap if number stays at 16 [7/15/2009 tom]
#endif

	CFixedSmallBlockHeap m_PrimarySBH;
#ifdef MEMALLOC_USE_SECONDARY_SBH
	CFixedVirtualSmallBlockHeap m_SecondarySBH;
#endif
#ifndef MEMALLOC_NO_FALLBACK
	CVirtualSmallBlockHeap m_FallbackSBH;
#endif

#endif // MEM_SBH_ENABLED


	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment );

	virtual size_t MemoryAllocFailed();

	void		SetCRTAllocFailed( size_t nMemSize );

	MemAllocFailHandler_t m_pfnFailHandler;
	size_t				m_sMemoryAllocFailed;
	CThreadFastMutex	m_CompactMutex;
	bool				m_bInCompact;
};

#ifndef _PS3
#pragma pack()
#endif
