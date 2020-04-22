//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: This header should never be used directly from leaf code!!!
// Instead, just add the file memoverride.cpp into your project and all this
// will automagically be used
//
// $NoKeywords: $
//=============================================================================//

#ifndef TIER0_MEMALLOC_H
#define TIER0_MEMALLOC_H

#ifdef _WIN32
#pragma once
#endif

// These memory debugging switches aren't relevant under Linux builds since memoverride.cpp
// isn't built into Linux projects
#ifndef LINUX
// Define this in release to get memory tracking even in release builds
//#define USE_MEM_DEBUG 1

// Define this in release to get light memory debugging
//#define USE_LIGHT_MEM_DEBUG

// Define this to require -uselmd to turn light memory debugging on
//#define LIGHT_MEM_DEBUG_REQUIRES_CMD_LINE_SWITCH
#endif

#if defined( _MEMTEST )
#if defined( _WIN32 ) || defined( _PS3 )
#define USE_MEM_DEBUG 1
#endif
#endif


#if defined( _PS3 )
// Define STEAM_SHARES_GAME_ALLOCATOR to make Steam use the game's tier0 memory allocator.
// This adds some memory to the game's Small Block Heap and Medium Block Heap, to compensate.
// This configuration was disabled for Portal 2, as we could not sufficiently test it before ship.
//#define STEAM_SHARES_GAME_ALLOCATOR
#endif

#if defined( STEAM_SHARES_GAME_ALLOCATOR )
#define MBYTES_STEAM_SBH_USAGE 2
#define MBYTES_STEAM_MBH_USAGE 4
#else // STEAM_SHARES_GAME_ALLOCATOR
#define MBYTES_STEAM_SBH_USAGE 0
#define MBYTES_STEAM_MBH_USAGE 0
#endif // STEAM_SHARES_GAME_ALLOCATOR

// Undefine this if using a compiler lacking threadsafe RTTI (like vc6)
#define MEM_DEBUG_CLASSNAME 1

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include <stddef.h>
#ifdef LINUX
#undef offsetof
#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#endif

#ifdef _PS3
#define MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS 1
#endif

#include "tier0/mem.h"

struct _CrtMemState;

#define MEMALLOC_VERSION 1

typedef size_t (*MemAllocFailHandler_t)( size_t );

struct GenericMemoryStat_t
{
	const char *name;
	int value;
};


// Virtual memory interface
#include "tier0/memvirt.h"


//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
abstract_class IMemAlloc
{
public:
	// Release versions
	virtual void *Alloc( size_t nSize ) = 0;
public:
	virtual void *Realloc( void *pMem, size_t nSize ) = 0;

	virtual void Free( void *pMem ) = 0;
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize ) = 0;

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, int nLine ) = 0;
public:
    virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine ) = 0;
    virtual void  Free( void *pMem, const char *pFileName, int nLine ) = 0;
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine ) = 0;

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	virtual void *AllocAlign( size_t nSize, size_t align ) = 0;
	virtual void *AllocAlign( size_t nSize, size_t align, const char *pFileName, int nLine ) = 0;
	virtual void *ReallocAlign( void *pMem, size_t nSize, size_t align ) = 0;
#endif

	inline void *IndirectAlloc( size_t nSize )										{ return Alloc( nSize ); }
	inline void *IndirectAlloc( size_t nSize, const char *pFileName, int nLine )	{ return Alloc( nSize, pFileName, nLine ); }

	// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
	virtual size_t GetSize( void *pMem ) = 0;

    // Force file + line information for an allocation
    virtual void PushAllocDbgInfo( const char *pFileName, int nLine ) = 0;
    virtual void PopAllocDbgInfo() = 0;

	// FIXME: Remove when we have our own allocator
	// these methods of the Crt debug code is used in our codebase currently
	virtual int32 CrtSetBreakAlloc( int32 lNewBreakAlloc ) = 0;
	virtual	int CrtSetReportMode( int nReportType, int nReportMode ) = 0;
	virtual int CrtIsValidHeapPointer( const void *pMem ) = 0;
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) = 0;
	virtual int CrtCheckMemory( void ) = 0;
	virtual int CrtSetDbgFlag( int nNewFlag ) = 0;
	virtual void CrtMemCheckpoint( _CrtMemState *pState ) = 0;

	// FIXME: Make a better stats interface
	virtual void DumpStats() = 0;
	virtual void DumpStatsFileBase( char const *pchFileBase ) = 0;
	virtual size_t ComputeMemoryUsedBy( char const *pchSubStr ) = 0;

	// FIXME: Remove when we have our own allocator
	virtual void* CrtSetReportFile( int nRptType, void* hFile ) = 0;
	virtual void* CrtSetReportHook( void* pfnNewHook ) = 0;
	virtual int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg ) = 0;

	virtual int heapchk() = 0;

	virtual bool IsDebugHeap() = 0;

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) = 0;
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) = 0;
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) = 0;

	virtual int GetVersion() = 0;

	virtual void CompactHeap() = 0;

	// Function called when malloc fails or memory limits hit to attempt to free up memory (can come in any thread)
	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) = 0;

	virtual void DumpBlockStats( void * ) = 0;

	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment ) = 0;

	// Returns 0 if no failure, otherwise the size_t of the last requested chunk
	virtual size_t MemoryAllocFailed() = 0;

	virtual void CompactIncremental() = 0; 

	virtual void OutOfMemory( size_t nBytesAttempted = 0 ) = 0;

	// Region-based allocations
	virtual void *RegionAlloc( int region, size_t nSize ) = 0;
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine ) = 0;

	// Replacement for ::GlobalMemoryStatus which accounts for unused memory in our system
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) = 0;

	// Obtain virtual memory manager interface
	virtual IVirtualMemorySection * AllocateVirtualMemorySection( size_t numMaxBytes ) = 0;

	// Request 'generic' memory stats (returns a list of N named values; caller should assume this list will change over time)
	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats ) = 0;

	virtual ~IMemAlloc() { };

	// handles storing allocation info for coroutines
	virtual uint32 GetDebugInfoSize() = 0;
	virtual void SaveDebugInfo( void *pvDebugInfo ) = 0;
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) = 0;	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) = 0;
};

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
#ifdef _PS3

PLATFORM_INTERFACE IMemAlloc * g_pMemAllocInternalPS3;
#ifndef PLATFORM_INTERFACE_MEM_ALLOC_INTERNAL_PS3_OVERRIDE
#define g_pMemAlloc g_pMemAllocInternalPS3
#else
#define g_pMemAlloc PLATFORM_INTERFACE_MEM_ALLOC_INTERNAL_PS3_OVERRIDE
#endif

#else // !_PS3

MEM_INTERFACE IMemAlloc *g_pMemAlloc;

#endif

//-----------------------------------------------------------------------------

#ifdef MEMALLOC_REGIONS
#ifndef MEMALLOC_REGION
#define MEMALLOC_REGION 0
#endif
inline void *MemAlloc_Alloc( size_t nSize )
{ 
	return g_pMemAlloc->RegionAlloc( MEMALLOC_REGION, nSize );
}

inline void *MemAlloc_Alloc( size_t nSize, const char *pFileName, int nLine )
{ 
	return g_pMemAlloc->RegionAlloc( MEMALLOC_REGION, nSize, pFileName, nLine );
}
#else
#undef MEMALLOC_REGION
inline void *MemAlloc_Alloc( size_t nSize )
{ 
	return g_pMemAlloc->IndirectAlloc( nSize );
}

inline void *MemAlloc_Alloc( size_t nSize, const char *pFileName, int nLine )
{ 
	return g_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine );
}
#endif

//-----------------------------------------------------------------------------

#ifdef MEMALLOC_REGIONS
#else
#endif


inline bool ValueIsPowerOfTwo( size_t value )			// don't clash with mathlib definition
{
	return (value & ( value - 1 )) == 0;
}


inline void *MemAlloc_AllocAlignedUnattributed( size_t size, size_t align )
{
#ifndef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	unsigned char *pAlloc, *pResult;

#endif

	if (!ValueIsPowerOfTwo(align))
		return NULL;

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	return g_pMemAlloc->AllocAlign( size, align );

#else

	align = (align > sizeof(void *) ? align : sizeof(void *)) - 1;

	if ( (pAlloc = (unsigned char*)MemAlloc_Alloc( sizeof(void *) + align + size ) ) == (unsigned char*)NULL)
		return NULL;

	pResult = (unsigned char*)( (size_t)(pAlloc + sizeof(void *) + align ) & ~align );
	((unsigned char**)(pResult))[-1] = pAlloc;

	return (void *)pResult;

#endif
}

inline void *MemAlloc_AllocAlignedFileLine( size_t size, size_t align, const char *pszFile, int nLine )
{
#ifndef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	unsigned char *pAlloc, *pResult;

#endif

	if (!ValueIsPowerOfTwo(align))
		return NULL;

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	return g_pMemAlloc->AllocAlign( size, align, pszFile, nLine );

#else

	align = (align > sizeof(void *) ? align : sizeof(void *)) - 1;

	if ( (pAlloc = (unsigned char*)MemAlloc_Alloc( sizeof(void *) + align + size, pszFile, nLine ) ) == (unsigned char*)NULL)
		return NULL;

	pResult = (unsigned char*)( (size_t)(pAlloc + sizeof(void *) + align ) & ~align );
	((unsigned char**)(pResult))[-1] = pAlloc;

	return (void *)pResult;

#endif
}

#ifdef USE_MEM_DEBUG
#define MemAlloc_AllocAligned( s, a )	MemAlloc_AllocAlignedFileLine( s, a, __FILE__, __LINE__ )
#elif defined(USE_LIGHT_MEM_DEBUG)
extern const char *g_pszModule; 
#define MemAlloc_AllocAligned( s, a )	MemAlloc_AllocAlignedFileLine( s, a, g_pszModule, 0 )
#else
#define MemAlloc_AllocAligned( s, a )	MemAlloc_AllocAlignedUnattributed( s, a )
#endif


inline void *MemAlloc_ReallocAligned( void *ptr, size_t size, size_t align )
{
	if ( !ValueIsPowerOfTwo( align ) )
		return NULL;

	// Don't change alignment between allocation + reallocation.
	if ( ( (size_t)ptr & ( align - 1 ) ) != 0 )
		return NULL;

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	return g_pMemAlloc->ReallocAlign( ptr, size, align );

#else

	if ( !ptr )
		return MemAlloc_AllocAligned( size, align );

	void *pAlloc, *pResult;

	// Figure out the actual allocation point
	pAlloc = ptr;
	pAlloc = (void *)(((size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));
	pAlloc = *( (void **)pAlloc );

	// See if we have enough space
	size_t nOffset = (size_t)ptr - (size_t)pAlloc;
	size_t nOldSize = g_pMemAlloc->GetSize( pAlloc );
	if ( nOldSize >= size + nOffset )
		return ptr;

	pResult = MemAlloc_AllocAligned( size, align );
	memcpy( pResult, ptr, nOldSize - nOffset );
	g_pMemAlloc->Free( pAlloc );
	return pResult;

#endif
}

inline void MemAlloc_FreeAligned( void *pMemBlock )
{
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	g_pMemAlloc->Free( pMemBlock );

#else

	void *pAlloc;

	if ( pMemBlock == NULL )
		return;

	pAlloc = pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (void *)(((size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));

	// pAlloc is the pointer to the start of memory block
	pAlloc = *( (void **)pAlloc );

	g_pMemAlloc->Free( pAlloc );

#endif
}

inline void MemAlloc_FreeAligned( void *pMemBlock, const char *pszFile, int nLine )
{
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	g_pMemAlloc->Free( pMemBlock, pszFile, nLine );

#else

	void *pAlloc;

	if ( pMemBlock == NULL )
		return;

	pAlloc = pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (void *)(((size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));

	// pAlloc is the pointer to the start of memory block
	pAlloc = *( (void **)pAlloc );
	g_pMemAlloc->Free( pAlloc, pszFile, nLine );

#endif
}

inline size_t MemAlloc_GetSizeAligned( void *pMemBlock )
{
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS

	return g_pMemAlloc->GetSize( pMemBlock );

#else

	void *pAlloc;

	if ( pMemBlock == NULL )
		return 0;

	pAlloc = pMemBlock;

	// pAlloc points to the pointer to starting of the memory block
	pAlloc = (void *)(((size_t)pAlloc & ~( sizeof(void *) - 1 ) ) - sizeof(void *));

	// pAlloc is the pointer to the start of memory block
	pAlloc = *((void **)pAlloc );
	return g_pMemAlloc->GetSize( pAlloc ) - ( (byte *)pMemBlock - (byte *)pAlloc );

#endif
}


struct aligned_tmp_t
{
	// empty base class
};

// template here to allow adding alignment at levels of hierarchy that aren't the base
template< int bytesAlignment = 16, class T = aligned_tmp_t >
class CAlignedNewDelete : public T
{
public:
	void *operator new( size_t nSize )
	{
		return MemAlloc_AllocAligned( nSize, bytesAlignment );
	}

	void* operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
	{
		return MemAlloc_AllocAlignedFileLine( nSize, bytesAlignment, pFileName, nLine );
	}

	void operator delete(void *pData)
	{
		if ( pData )
		{
			MemAlloc_FreeAligned( pData );
		}
	}

	void operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
	{
		if ( pData )
		{
			MemAlloc_FreeAligned( pData, pFileName, nLine );
		}
	}
};

//-----------------------------------------------------------------------------

#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
#define MEM_ALLOC_CREDIT_(tag)	CMemAllocAttributeAlloction memAllocAttributeAlloction( tag, __LINE__ )
#define MemAlloc_PushAllocDbgInfo( pszFile, line ) g_pMemAlloc->PushAllocDbgInfo( pszFile, line )
#define MemAlloc_PopAllocDbgInfo() g_pMemAlloc->PopAllocDbgInfo()
#define MemAlloc_RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) g_pMemAlloc->RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime )
#define MemAlloc_RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) g_pMemAlloc->RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime )
#else
#define MEM_ALLOC_CREDIT_(tag)	((void)0)
#define MemAlloc_PushAllocDbgInfo( pszFile, line ) ((void)0)
#define MemAlloc_PopAllocDbgInfo() ((void)0)
#define MemAlloc_RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) ((void)0)
#define MemAlloc_RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime ) ((void)0)
#endif

//-----------------------------------------------------------------------------

class CMemAllocAttributeAlloction
{
public:
	CMemAllocAttributeAlloction( const char *pszFile, int line ) 
	{
		MemAlloc_PushAllocDbgInfo( pszFile, line );
	}
	
	~CMemAllocAttributeAlloction()
	{
		MemAlloc_PopAllocDbgInfo();
	}
};

#define MEM_ALLOC_CREDIT()	MEM_ALLOC_CREDIT_(__FILE__)

//-----------------------------------------------------------------------------

#if defined(MSVC) && ( defined(_DEBUG) || defined(USE_MEM_DEBUG) )

	#pragma warning(disable:4290)
	#pragma warning(push)
	#include <typeinfo.h>

	// MEM_DEBUG_CLASSNAME is opt-in.
	// Note: typeid().name() is not threadsafe, so if the project needs to access it in multiple threads
	// simultaneously, it'll need a mutex.
	#if defined(_CPPRTTI) && defined(MEM_DEBUG_CLASSNAME)

		template <typename T> const char *MemAllocClassName( T *p )
		{
			static const char *pszName = typeid(*p).name(); // @TODO: support having debug heap ignore certain allocations, and ignore memory allocated here [5/7/2009 tom]
			return pszName;
		}

		#define MEM_ALLOC_CREDIT_CLASS()	MEM_ALLOC_CREDIT_( MemAllocClassName( this ) )
		#define MEM_ALLOC_CLASSNAME(type) (typeid((type*)(0)).name())
	#else
		#define MEM_ALLOC_CREDIT_CLASS()	MEM_ALLOC_CREDIT_( __FILE__ )
		#define MEM_ALLOC_CLASSNAME(type) (__FILE__)
	#endif

	// MEM_ALLOC_CREDIT_FUNCTION is used when no this pointer is available ( inside 'new' overloads, for example )
	#ifdef _MSC_VER
		#define MEM_ALLOC_CREDIT_FUNCTION()		MEM_ALLOC_CREDIT_( __FUNCTION__ )
	#else
		#define MEM_ALLOC_CREDIT_FUNCTION() (__FILE__)
	#endif

	#pragma warning(pop)
#else
	#define MEM_ALLOC_CREDIT_CLASS()
	#define MEM_ALLOC_CLASSNAME(type) NULL
	#define MEM_ALLOC_CREDIT_FUNCTION() 
#endif

//-----------------------------------------------------------------------------

#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
struct MemAllocFileLine_t
{
	const char *pszFile;
	int line;
};

#define MEMALLOC_DEFINE_EXTERNAL_TRACKING( tag ) \
	static CUtlMap<void *, MemAllocFileLine_t, int> s_##tag##Allocs( DefLessFunc( void *) ); \
	CUtlMap<void *, MemAllocFileLine_t, int> * g_p##tag##Allocs = &s_##tag##Allocs; \
	static CThreadFastMutex s_##tag##AllocsMutex; \
	CThreadFastMutex * g_p##tag##AllocsMutex = &s_##tag##AllocsMutex; \
	const char * g_psz##tag##Alloc = strcpy( (char *)MemAlloc_Alloc( strlen( #tag "Alloc" ) + 1, "intentional leak", 0 ), #tag "Alloc" );

#define MEMALLOC_DECLARE_EXTERNAL_TRACKING( tag ) \
	extern CUtlMap<void *, MemAllocFileLine_t, int> * g_p##tag##Allocs; \
	extern CThreadFastMutex *g_p##tag##AllocsMutex; \
	extern const char * g_psz##tag##Alloc;

#define MemAlloc_RegisterExternalAllocation( tag, p, size ) \
	if ( !p ) \
		; \
	else \
	{ \
		AUTO_LOCK_FM( *g_p##tag##AllocsMutex ); \
		MemAllocFileLine_t fileLine = { g_psz##tag##Alloc, 0 }; \
		g_pMemAlloc->GetActualDbgInfo( fileLine.pszFile, fileLine.line ); \
		if ( fileLine.pszFile != g_psz##tag##Alloc ) \
		{ \
			g_p##tag##Allocs->Insert( p, fileLine ); \
		} \
		\
		MemAlloc_RegisterAllocation( fileLine.pszFile, fileLine.line, size, size, 0); \
	}

#define MemAlloc_RegisterExternalDeallocation( tag, p, size ) \
	if ( !p ) \
		; \
	else \
	{ \
		AUTO_LOCK_FM( *g_p##tag##AllocsMutex ); \
		MemAllocFileLine_t fileLine = { g_psz##tag##Alloc, 0 }; \
		CUtlMap<void *, MemAllocFileLine_t, int>::IndexType_t iRecordedFileLine = g_p##tag##Allocs->Find( p ); \
		if ( iRecordedFileLine != g_p##tag##Allocs->InvalidIndex() ) \
		{ \
			fileLine = (*g_p##tag##Allocs)[iRecordedFileLine]; \
			g_p##tag##Allocs->RemoveAt( iRecordedFileLine ); \
		} \
		\
		MemAlloc_RegisterDeallocation( fileLine.pszFile, fileLine.line, size, size, 0); \
	}

#else

#define MEMALLOC_DEFINE_EXTERNAL_TRACKING( tag )
#define MEMALLOC_DECLARE_EXTERNAL_TRACKING( tag )
#define MemAlloc_RegisterExternalAllocation( tag, p, size ) ((void)0)
#define MemAlloc_RegisterExternalDeallocation( tag, p, size ) ((void)0)

#endif

//-----------------------------------------------------------------------------

#endif // !STEAM && !NO_MALLOC_OVERRIDE

//-----------------------------------------------------------------------------

#if !defined(STEAM) && defined(NO_MALLOC_OVERRIDE)
#include <malloc.h>

#define MEM_ALLOC_CREDIT_(tag)	((void)0)
#define MEM_ALLOC_CREDIT()	MEM_ALLOC_CREDIT_(__FILE__)
#define MEM_ALLOC_CREDIT_CLASS()
#define MEM_ALLOC_CLASSNAME(type) NULL

#define MemAlloc_PushAllocDbgInfo( pszFile, line )
#define MemAlloc_PopAllocDbgInfo()

#define MemAlloc_RegisterAllocation( a,b,c,d,e ) ((void)0)
#define MemAlloc_RegisterDeallocation( a,b,c,d,e ) ((void)0)

#define MEMALLOC_DEFINE_EXTERNAL_TRACKING( tag )
#define MemAlloc_RegisterExternalAllocation( tag, p, size ) ((void)0)
#define MemAlloc_RegisterExternalDeallocation( tag, p, size ) ((void)0)

inline void *MemAlloc_AllocAligned( size_t size, size_t align )
{
	return (void *)_aligned_malloc( size, align );
}
inline void *MemAlloc_AllocAligned( size_t size, size_t align, const char *pszFile, int nLine )
{
	pszFile = pszFile;
	nLine = nLine;
	return (void *)_aligned_malloc( size, align );
}

inline void MemAlloc_FreeAligned( void *pMemBlock )
{
	_aligned_free( pMemBlock );
}
inline void MemAlloc_FreeAligned( void *pMemBlock, const char *pszFile, int nLine )
{
	pszFile = pszFile;
	nLine = nLine;
	_aligned_free( pMemBlock );
}

#endif // !STEAM && NO_MALLOC_OVERRIDE

//-----------------------------------------------------------------------------



// linux memory tracking via hooks.
#if defined( POSIX ) && !defined( _PS3 )
PLATFORM_INTERFACE void MemoryLogMessage( char const *s );						// throw a message into the memory log
PLATFORM_INTERFACE void EnableMemoryLogging( bool bOnOff );
PLATFORM_INTERFACE void DumpMemoryLog( int nThresh );
PLATFORM_INTERFACE void DumpMemorySummary( void );
PLATFORM_INTERFACE void SetMemoryMark( void );
PLATFORM_INTERFACE void DumpChangedMemory( int nThresh );

// ApproximateProcessMemoryUsage returns the approximate memory footprint of this process.
PLATFORM_INTERFACE size_t ApproximateProcessMemoryUsage( void );
#else
inline void MemoryLogMessage( char const * )
{
}
inline void EnableMemoryLogging( bool )
{
}
inline void DumpMemoryLog( int )
{
}
inline void DumpMemorySummary( void )
{
}
inline void SetMemoryMark( void )
{
}
inline void DumpChangedMemory( int )
{
}
inline size_t ApproximateProcessMemoryUsage( void )
{
	return 0;
}
#endif


#endif /* TIER0_MEMALLOC_H */
