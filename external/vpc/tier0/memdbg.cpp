//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//===========================================================================//


#include "pch_tier0.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

//#include <malloc.h>
#include <string.h>
#include "tier0/dbg.h"
#include "tier0/stackstats.h"
#include "tier0/memalloc.h"
#include "tier0/fasttimer.h"
#include "mem_helpers.h"
#ifdef PLATFORM_WINDOWS_PC
#undef WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <crtdbg.h>
#endif
#ifdef OSX
#include <malloc/malloc.h>
#include <stdlib.h>
#endif

#include <map>
#include <set>
#include <limits.h>
#include "tier0/threadtools.h"
#ifdef _X360
#include "xbox/xbox_console.h"
#endif

#ifdef _PS3
#include "sys/memory.h"
#include "tls_ps3.h"
#include "ps3/ps3_helpers.h"
#include "memoverride_ps3.h"
#endif

#ifdef USE_LIGHT_MEM_DEBUG
#undef USE_MEM_DEBUG
#endif

#if (!defined( POSIX ) && (defined(_DEBUG) || defined(USE_MEM_DEBUG)))
#pragma message ("USE_MEM_DEBUG is enabled in a release build. Don't check this in!")
#endif

#include "mem_impl_type.h"

#if MEM_IMPL_TYPE_DBG

#if defined(_WIN32) && ( !defined(_X360) && !defined(_WIN64) )
//be sure to disable frame pointer omission for all projects. "vpc /nofpo" when using stack traces
//#define USE_STACK_TRACES 
// or:
//#define USE_STACK_TRACES_DETAILED
const size_t STACK_TRACE_LENGTH = 32;
#endif

//prevent stupid bugs from checking one and not the other
#if defined( USE_STACK_TRACES_DETAILED ) && !defined( USE_STACK_TRACES )
#define USE_STACK_TRACES //don't comment me. I'm a safety check
#endif

#if defined( USE_STACK_TRACES )
#define SORT_STACK_TRACE_DESCRIPTION_DUMPS
#endif

#if (defined( USE_STACK_TRACES )) && !(defined( TIER0_FPO_DISABLED ) || defined( _DEBUG ))
#error Stack traces will not work unless FPO is disabled for every function traced through. Rebuild everything with FPO disabled "vpc /nofpo"
#endif

//-----------------------------------------------------------------------------

#ifdef _PS3
MemOverrideRawCrtFunctions_t *g_pMemOverrideRawCrtFns;
#define DebugAlloc	(g_pMemOverrideRawCrtFns->pfn_malloc)
#define DebugFree	(g_pMemOverrideRawCrtFns->pfn_free)
#elif defined( _X360 )
#define DebugAlloc	DmAllocatePool
#define DebugFree	DmFreePool
#else
#define DebugAlloc	malloc
#define DebugFree	free
#endif

#ifdef _WIN32
int g_DefaultHeapFlags = _CrtSetDbgFlag( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF );
#else
int g_DefaultHeapFlags = 0;
#endif // win32

#if defined( _MEMTEST )
static char s_szStatsMapName[32];
static char s_szStatsComment[256];
#endif

#pragma optimize( "", off )
//-----------------------------------------------------------------------------

#if defined( USE_STACK_TRACES )

bool GetModuleFromAddress( void *address, char *pResult, int iLength )
{
	return GetModuleNameFromAddress( address, pResult, iLength );	
}

bool GetCallerModule( char *pDest, int iLength )
{
	void *pCaller;
	GetCallStack_Fast( &pCaller, 1, 2 );

	return ( pCaller != 0 && GetModuleFromAddress( pCaller, pDest, iLength ) );
}

//
// Note: StackDescribe function is non-reentrant:
//		Reason:   Stack description is stored in a static buffer.
//		Solution: Passing caller-allocated buffers would allow the
//		function to become reentrant, however the current only client (FindOrCreateFilename)
//		is synchronized with a heap mutex, after retrieving stack description the
//		heap memory will be allocated to copy the text.
//

char * StackDescribe( void * const *ppAddresses, int nMaxAddresses )
{
	static char s_chStackDescription[ 32 * 1024 ];
	char *pchBuffer = s_chStackDescription;

#if defined( SORT_STACK_TRACE_DESCRIPTION_DUMPS ) //Assuming StackDescribe is called iteratively on a sorted set of stacks (as in DumpStackStats()). We can save work by skipping unchanged parts at the beginning of the string.
	static void *LastCallStack[STACK_TRACE_LENGTH] = { NULL };
	static char *pEndPos[STACK_TRACE_LENGTH] = { NULL };
	bool bUseExistingString = true;
#else
	s_chStackDescription[ 0 ] = 0;
#endif

	int k;
	for ( k = 0; k < nMaxAddresses; ++ k )
	{
		if ( !ppAddresses[k] )
			break;

#if defined( SORT_STACK_TRACE_DESCRIPTION_DUMPS )
		if( bUseExistingString && (k < STACK_TRACE_LENGTH) )
		{
			if( ppAddresses[k] == LastCallStack[k] )
			{
				pchBuffer = pEndPos[k];
				continue;
			}
			else
			{
				//everything from here on is invalidated
				bUseExistingString = false;
				for( int clearEntries = k; clearEntries < STACK_TRACE_LENGTH; ++clearEntries ) //wipe out unused entries
				{
					LastCallStack[clearEntries] = NULL;
					pEndPos[clearEntries] = NULL;
				}
				//fall through to existing code

				if( k == 0 )
					*pchBuffer = '\0';
				else
					sprintf( pchBuffer, "<--" );
			}
		}
#endif
		{
			pchBuffer += strlen( pchBuffer );

			char szTemp[MAX_PATH];
			szTemp[0] = '\0';
			uint32 iLine = 0;
			uint32 iLineDisplacement = 0;
			uint64 iSymbolDisplacement = 0;
			if ( GetFileAndLineFromAddress( ppAddresses[k], szTemp, MAX_PATH, iLine, &iLineDisplacement ) )
			{
				char const *pchFileName = szTemp + strlen( szTemp );
				for ( size_t numSlashesAllowed = 2; pchFileName > szTemp; --pchFileName )
				{
					if ( *pchFileName == '\\' )
					{
						if ( numSlashesAllowed-- )
							continue;
						else
							break;
					}
				}
				sprintf( pchBuffer, iLineDisplacement ? "%s:%d+0x%I32X" : "%s:%d", pchFileName, iLine, iLineDisplacement );
			}
			else if ( GetSymbolNameFromAddress( ppAddresses[k], szTemp, MAX_PATH, &iSymbolDisplacement ) )
			{
				sprintf( pchBuffer, ( iSymbolDisplacement > 0 && !( iSymbolDisplacement >> 63 ) ) ? "%s+0x%llX" : "%s", szTemp, iSymbolDisplacement );
			}
			else
			{
				sprintf( pchBuffer, "#0x%08p", ppAddresses[k] );
			}

			pchBuffer += strlen( pchBuffer );
			sprintf( pchBuffer, "<--" );

#if defined( SORT_STACK_TRACE_DESCRIPTION_DUMPS )
			if( k < STACK_TRACE_LENGTH )
			{
				LastCallStack[k] = ppAddresses[k];
				pEndPos[k] = pchBuffer;
			}
#endif
		}
	}
	*pchBuffer = 0;

#if defined( SORT_STACK_TRACE_DESCRIPTION_DUMPS )
	for( ; k < STACK_TRACE_LENGTH; ++k ) //wipe out unused entries
	{
		LastCallStack[k] = NULL;
		pEndPos[k] = NULL;
	}
#endif

	return s_chStackDescription;
}

#else

#define GetModuleFromAddress( address, pResult, iLength ) ( ( *pResult = 0 ), 0)
#define GetCallerModule( pDest, iLength ) false

#endif


//-----------------------------------------------------------------------------

// NOTE: This exactly mirrors the dbg header in the MSDEV crt
// eventually when we write our own allocator, we can kill this
struct CrtDbgMemHeader_t
{
	unsigned char m_Reserved[8];
	const char *m_pFileName;
	int			m_nLineNumber;
	unsigned char m_Reserved2[16];
};

struct Sentinal_t
{
	DWORD value[4];
};

Sentinal_t g_HeadSentinelAllocated = 
{
	0xeee1beef,
	0xeee1f00d,
	0xbd122969,
	0xbeefbeef,
};

Sentinal_t g_HeadSentinelFree = 
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

enum DbgMemHeaderBlockType_t
{
	BLOCKTYPE_FREE,
	BLOCKTYPE_ALLOCATED
};

struct DbgMemHeader_t
#if !defined( _DEBUG ) || defined( _PS3 )
	: CrtDbgMemHeader_t
#endif
{
	size_t nLogicalSize;
#if defined( USE_STACK_TRACES )
	unsigned int nStatIndex;
	byte reserved[16 - (sizeof(unsigned int) * 2)];	// MS allocator always returns mem aligned on 16 bytes, which some of our code depends on
#else
	byte reserved[16 - sizeof(unsigned int)]; // MS allocator always returns mem aligned on 16 bytes, which some of our code depends on
#endif
	Sentinal_t sentinal;
};

const int g_nRecentFrees = ( IsPC() ) ? 8192 : 512;
DbgMemHeader_t ** GetRecentFrees() { static DbgMemHeader_t **g_pRecentFrees = (DbgMemHeader_t**)
#ifdef _PS3
g_pMemOverrideRawCrtFns->pfn_calloc
#else
calloc
#endif
( g_nRecentFrees, sizeof(DbgMemHeader_t *) );
return g_pRecentFrees; }
uint32 volatile g_iNextFreeSlot;

uint32 volatile g_break_BytesFree = 0xffffffff;

void LMDReportInvalidBlock( DbgMemHeader_t *pHeader, const char *pszMessage )
{
	char szMsg[256];
	if ( pHeader )
	{
		sprintf( szMsg, "HEAP IS CORRUPT: %s (block 0x%x, %d bytes)\n", pszMessage, (size_t)( ((byte*) pHeader) + sizeof( DbgMemHeader_t ) ), pHeader->nLogicalSize );
	}
	else
	{
		sprintf( szMsg, "HEAP IS CORRUPT: %s\n", pszMessage );
	}
	Assert( !"HEAP IS CORRUPT!" );
	DebuggerBreak();
}

void LMDValidateBlock( DbgMemHeader_t *pHeader, bool bFreeList )
{
	if ( memcmp( &pHeader->sentinal, bFreeList ? &g_HeadSentinelFree : &g_HeadSentinelAllocated, sizeof(Sentinal_t) ) != 0 )
	{
		LMDReportInvalidBlock( pHeader, "Head sentinel corrupt" );
	}
	if ( memcmp( ((Sentinal_t *)(( ((byte*) pHeader) + sizeof( DbgMemHeader_t ) + pHeader->nLogicalSize ))), &g_TailSentinel, sizeof(Sentinal_t) ) != 0 )
	{
		LMDReportInvalidBlock( pHeader, "Tail sentinel corrupt" );
	}
	if ( bFreeList )
	{
		byte *pCur = (byte *)pHeader + sizeof(DbgMemHeader_t);
		byte *pLimit = pCur + pHeader->nLogicalSize;
		while ( pCur != pLimit )
		{
			if ( *pCur++ != g_FreeFill )
			{
				LMDReportInvalidBlock( pHeader, "Write after free" );
			}
		}
	}
}


//-----------------------------------------------------------------------------

#if defined( _DEBUG ) && !defined( POSIX )
#define GetCrtDbgMemHeader( pMem ) ((CrtDbgMemHeader_t*)((DbgMemHeader_t*)pMem - 1) - 1)
#elif defined( OSX )
DbgMemHeader_t *GetCrtDbgMemHeader( void *pMem );
#else
#define GetCrtDbgMemHeader( pMem ) ((DbgMemHeader_t*)(pMem) - 1)
#endif

#if defined( USE_STACK_TRACES )
#define GetAllocationStatIndex_Internal( pMem ) ( ((DbgMemHeader_t*)pMem - 1)->nStatIndex )
#endif

#ifdef OSX
DbgMemHeader_t *GetCrtDbgMemHeader( void *pMem )
{
	size_t msize = malloc_size( pMem );
	return (DbgMemHeader_t *)( (char *)pMem + msize - sizeof(DbgMemHeader_t) );
}
#endif


inline void *InternalMalloc( size_t nSize, const char *pFileName, int nLine )
{
#if defined( POSIX ) || defined( _PS3 )
	void *pAllocedMem = NULL;
#ifdef OSX
	pAllocedMem = malloc_zone_malloc( malloc_default_zone(), nSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );	
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pAllocedMem );
#elif defined( _PS3 )
	pAllocedMem = (g_pMemOverrideRawCrtFns->pfn_malloc)( nSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pAllocedMem;
	*((void**)pInternalMem->m_Reserved2) = pAllocedMem;
#else
	pAllocedMem = malloc( nSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pAllocedMem;
#endif
	
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = nSize;
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;
	
	pInternalMem->sentinal = g_HeadSentinelAllocated;
	*( (Sentinal_t *)( ((byte*)pInternalMem) + sizeof( DbgMemHeader_t ) + nSize ) ) = g_TailSentinel;
	LMDValidateBlock( pInternalMem, false );

#ifdef OSX
	return pAllocedMem;
#else
	return pInternalMem + 1;	
#endif
	
#else // WIN32
	DbgMemHeader_t *pInternalMem;
#if !defined( _DEBUG ) 
	pInternalMem = (DbgMemHeader_t *)malloc( nSize + sizeof(DbgMemHeader_t) );
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
#else
	pInternalMem = (DbgMemHeader_t *)_malloc_dbg( nSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif

	pInternalMem->nLogicalSize = nSize;
	return pInternalMem + 1;
#endif // WIN32
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
inline void *InternalMallocAligned( size_t nSize, size_t align, const char *pFileName, int nLine )
{
#if defined( POSIX ) || defined( _PS3 )
	void *pAllocedMem = NULL;
#ifdef OSX
	pAllocedMem = malloc_zone_malloc( malloc_default_zone(), nSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );	
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pAllocedMem );
#elif defined( _PS3 )
	size_t numWastedAlignPages = ( sizeof( DbgMemHeader_t ) / align );
	if ( align * numWastedAlignPages < sizeof( DbgMemHeader_t ) )
		++ numWastedAlignPages;
	size_t nSizeRequired = nSize + numWastedAlignPages*align + sizeof( Sentinal_t );
	pAllocedMem = (g_pMemOverrideRawCrtFns->pfn_memalign)( align, nSizeRequired );
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( ((char*)pAllocedMem) + numWastedAlignPages*align );
	*((void**)pInternalMem->m_Reserved2) = pAllocedMem;
#else
	pAllocedMem = malloc( nSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pAllocedMem;
#endif
	
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = nSize;
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	pInternalMem->sentinal = g_HeadSentinelAllocated;
	*( (Sentinal_t *)( ((byte*)pInternalMem) + sizeof( DbgMemHeader_t ) + nSize ) ) = g_TailSentinel;
	LMDValidateBlock( pInternalMem, false );

#ifdef OSX
	return pAllocedMem;
#else
	return pInternalMem + 1;	
#endif
	
#else // WIN32
	DbgMemHeader_t *pInternalMem;
#if !defined( _DEBUG ) 
	pInternalMem = (DbgMemHeader_t *)malloc( nSize + sizeof(DbgMemHeader_t) );
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
#else
	pInternalMem = (DbgMemHeader_t *)_malloc_dbg( nSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif

	pInternalMem->nLogicalSize = nSize;
	return pInternalMem + 1;
#endif // WIN32
}
#endif

inline void *InternalRealloc( void *pMem, size_t nNewSize, const char *pFileName, int nLine )
{
	if ( !pMem )
		return InternalMalloc( nNewSize, pFileName, nLine );

#ifdef POSIX
	void *pNewAllocedMem = NULL;
#ifdef OSX
	pNewAllocedMem = (DbgMemHeader_t *)malloc_zone_realloc( malloc_default_zone(), pMem, nNewSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pNewAllocedMem );
#elif defined( _PS3 )
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	pNewAllocedMem = (DbgMemHeader_t *)(g_pMemOverrideRawCrtFns->pfn_realloc)( *((void**)pInternalMem->m_Reserved2), nNewSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	pInternalMem = (DbgMemHeader_t *)pNewAllocedMem;
	*((void**)pInternalMem->m_Reserved2) = pNewAllocedMem;
#else
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	pNewAllocedMem = (DbgMemHeader_t *)realloc( pInternalMem, nNewSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	pInternalMem = (DbgMemHeader_t *)pNewAllocedMem;
#endif
	
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = static_cast<unsigned int>( nNewSize );
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	pInternalMem->sentinal = g_HeadSentinelAllocated;
	*( (Sentinal_t *)( ((byte*)pInternalMem) + sizeof( DbgMemHeader_t ) + nNewSize ) ) = g_TailSentinel;
	LMDValidateBlock( pInternalMem, false );
	
#ifdef OSX
	return pNewAllocedMem;
#else
	return pInternalMem + 1;
#endif
	
#else // WIN32
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#if !defined( _DEBUG )
	pInternalMem = (DbgMemHeader_t *)realloc( pInternalMem, nNewSize + sizeof(DbgMemHeader_t) );
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
#else
	pInternalMem = (DbgMemHeader_t *)_realloc_dbg( pInternalMem, nNewSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif

	pInternalMem->nLogicalSize = nNewSize;
	return pInternalMem + 1;
#endif // WIN32
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
inline void *InternalReallocAligned( void *pMem, size_t nNewSize, size_t align, const char *pFileName, int nLine )
{
	if ( !pMem )
		return InternalMallocAligned( nNewSize, align, pFileName, nLine );

#ifdef POSIX
	void *pNewAllocedMem = NULL;
#ifdef OSX
	pNewAllocedMem = (DbgMemHeader_t *)malloc_zone_realloc( malloc_default_zone(), pMem, nNewSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pNewAllocedMem );
#elif defined( _PS3 )
	size_t numWastedAlignPages = ( sizeof( DbgMemHeader_t ) / align );
	if ( align * numWastedAlignPages < sizeof( DbgMemHeader_t ) )
		++ numWastedAlignPages;
	size_t nSizeRequired = nNewSize + numWastedAlignPages*align + sizeof( Sentinal_t );
	
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	pNewAllocedMem = (DbgMemHeader_t *)(g_pMemOverrideRawCrtFns->pfn_reallocalign)( *((void**)pInternalMem->m_Reserved2), nSizeRequired, align );
	pInternalMem = GetCrtDbgMemHeader( ((char*)pNewAllocedMem) + numWastedAlignPages*align );
	*((void**)pInternalMem->m_Reserved2) = pNewAllocedMem;
#else
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	pNewAllocedMem = (DbgMemHeader_t *)realloc( pInternalMem, nNewSize + sizeof(DbgMemHeader_t) + sizeof( Sentinal_t ) );
	pInternalMem = (DbgMemHeader_t *)pNewAllocedMem;
#endif
	
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = static_cast<unsigned int>( nNewSize );
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	pInternalMem->sentinal = g_HeadSentinelAllocated;
	*( (Sentinal_t *)( ((byte*)pInternalMem) + sizeof( DbgMemHeader_t ) + nNewSize ) ) = g_TailSentinel;
	LMDValidateBlock( pInternalMem, false );
	
#ifdef OSX
	return pNewAllocedMem;
#else
	return pInternalMem + 1;
#endif
	
#else // WIN32
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#if !defined( _DEBUG )
	pInternalMem = (DbgMemHeader_t *)realloc( pInternalMem, nNewSize + sizeof(DbgMemHeader_t) );
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
#else
	pInternalMem = (DbgMemHeader_t *)_realloc_dbg( pInternalMem, nNewSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif

	pInternalMem->nLogicalSize = nNewSize;
	return pInternalMem + 1;
#endif // WIN32
}
#endif

inline void InternalFree( void *pMem )
{
	if ( !pMem )
		return;

	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;

#if defined( POSIX )
	// Record it in recent free blocks list
	DbgMemHeader_t **pRecentFrees = GetRecentFrees();
	uint32 iNextSlot = ThreadInterlockedIncrement( &g_iNextFreeSlot );
	iNextSlot %= g_nRecentFrees;

	if ( memcmp( &pInternalMem->sentinal, &g_HeadSentinelAllocated, sizeof( Sentinal_t ) ) != 0 )
	{
		Assert( !"Double Free or Corrupt Block Header!" );
		DebuggerBreak();
	}
	LMDValidateBlock( pInternalMem, false );
	if ( g_break_BytesFree == pInternalMem->nLogicalSize )
	{
		DebuggerBreak();
	}
	pInternalMem->sentinal = g_HeadSentinelFree;
	memset( pMem, g_FreeFill, pInternalMem->nLogicalSize );

	DbgMemHeader_t *pToFree = pInternalMem;
	if ( pInternalMem->nLogicalSize < 16*1024 )
	{
		pToFree = pRecentFrees[iNextSlot];
		pRecentFrees[iNextSlot] = pInternalMem;

		if ( pToFree )
		{
			LMDValidateBlock( pToFree, true );
		}
	}

	// Validate several last frees
	for ( uint32 k = iNextSlot - 1, iteration = 0; iteration < 10; ++ iteration, -- k )
	{
		if ( DbgMemHeader_t *pLastFree = pRecentFrees[ k % g_nRecentFrees ] )
		{
			LMDValidateBlock( pLastFree, true );
		}
	}

	if ( !pToFree )
		return;

#ifdef OSX
	malloc_zone_free( malloc_default_zone(), pToFree );
#elif defined( _PS3 )
	(g_pMemOverrideRawCrtFns->pfn_free)( *((void**)pToFree->m_Reserved2) );
#elif LINUX
	free( pToFree );
#else
	free( pToFree );	
#endif
#elif defined( _DEBUG )
	_free_dbg( pInternalMem, _NORMAL_BLOCK );
#else
	free( pInternalMem );
#endif
}

inline size_t InternalMSize( void *pMem )
{
#if defined( _PS3 )
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	return pInternalMem->nLogicalSize;
#elif defined(POSIX)
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	return pInternalMem->nLogicalSize;
#elif !defined(_DEBUG)
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
	return _msize( pInternalMem ) - sizeof(DbgMemHeader_t);
#else
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
	return _msize_dbg( pInternalMem, _NORMAL_BLOCK ) - sizeof(DbgMemHeader_t);
#endif	
}

inline size_t InternalLogicalSize( void *pMem )
{
#if defined(POSIX)
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pMem );
#elif !defined(_DEBUG)
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#else
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#endif
	return pInternalMem->nLogicalSize;
}

#ifndef _DEBUG
#define _CrtDbgReport( nRptType, szFile, nLine, szModule, pMsg ) 0
#endif

//-----------------------------------------------------------------------------


// Custom allocator protects this module from recursing on operator new
template <class T>
class CNoRecurseAllocator
{
public:
	// type definitions
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;

	CNoRecurseAllocator() {}
	CNoRecurseAllocator(const CNoRecurseAllocator&) {}
	template <class U> CNoRecurseAllocator(const CNoRecurseAllocator<U>&) {}
	~CNoRecurseAllocator(){}

	// rebind allocator to type U
	template <class U > struct rebind { typedef CNoRecurseAllocator<U> other; };

	// return address of values
	pointer address (reference value) const { return &value; }

	const_pointer address (const_reference value) const { return &value;}
	size_type max_size() const { return INT_MAX; }

	pointer allocate(size_type num, const void* = 0)  { return (pointer)DebugAlloc(num * sizeof(T)); }
	void deallocate (pointer p, size_type num) { DebugFree(p); }
	void construct(pointer p, const T& value) {	new((void*)p)T(value); }
	void destroy (pointer p) { p->~T(); }
};

template <class T1, class T2>
bool operator==(const CNoRecurseAllocator<T1>&, const CNoRecurseAllocator<T2>&)
{
	return true;
}

template <class T1, class T2>
bool operator!=(const CNoRecurseAllocator<T1>&, const CNoRecurseAllocator<T2>&)
{
	return false;
}

class CStringLess
{
public:
	bool operator()(const char *pszLeft, const char *pszRight ) const 
	{
		return ( V_tier0_stricmp( pszLeft, pszRight ) < 0 );
	}
};

//-----------------------------------------------------------------------------

#pragma warning( disable:4074 ) // warning C4074: initializers put in compiler reserved initialization area
#pragma init_seg( compiler )

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
class CDbgMemAlloc : public IMemAlloc
{
public:
	CDbgMemAlloc();
	virtual ~CDbgMemAlloc();

	// Release versions
	virtual void *Alloc( size_t nSize );
	virtual void *Realloc( void *pMem, size_t nSize );
	virtual void  Free( void *pMem );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize );

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	virtual void *AllocAlign( size_t nSize, size_t align );
	virtual void *AllocAlign( size_t nSize, size_t align, const char *pFileName, int nLine );
	virtual void *ReallocAlign( void *pMem, size_t nSize, size_t align );
	virtual void *ReallocAlign( void *pMem, size_t nSize, size_t align, const char *pFileName, int nLine );
#endif

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, int nLine );
    virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine );
    virtual void  Free( void *pMem, const char *pFileName, int nLine );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine );

	virtual void *RegionAlloc( int region, size_t nSize ) { return Alloc( nSize ); }
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine ) { return Alloc( nSize, pFileName, nLine ); }

	// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
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

	// handles storing allocation info for coroutines
	virtual uint32 GetDebugInfoSize();
	virtual void SaveDebugInfo( void *pvDebugInfo );
	virtual void RestoreDebugInfo( const void *pvDebugInfo );	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine );

	// FIXME: Remove when we have our own allocator
	virtual void* CrtSetReportFile( int nRptType, void* hFile );
	virtual void* CrtSetReportHook( void* pfnNewHook );
	virtual int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * szFormat );

	virtual int heapchk();

	virtual bool IsDebugHeap() { return true; }

	virtual int GetVersion() { return MEMALLOC_VERSION; }

	virtual void CompactHeap() 
	{
#if defined( _X360 ) && defined( _DEBUG )
		HeapCompact( GetProcessHeap(), 0 );
#endif
	}

	virtual void CompactIncremental() {}
	virtual void OutOfMemory( size_t nBytesAttempted = 0 ) {}

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) { return NULL; } // debug heap doesn't attempt retries

	void SetStatsExtraInfo( const char *pMapName, const char *pComment )
	{
#if defined( _MEMTEST )
		strncpy( s_szStatsMapName, pMapName, sizeof( s_szStatsMapName ) );
		s_szStatsMapName[sizeof( s_szStatsMapName ) - 1] = '\0';

		strncpy( s_szStatsComment, pComment, sizeof( s_szStatsComment ) );
		s_szStatsComment[sizeof( s_szStatsComment ) - 1] = '\0';
#endif
	}

	virtual size_t MemoryAllocFailed();
	void		SetCRTAllocFailed( size_t nMemSize );

	enum
	{
		BYTE_COUNT_16 = 0,
		BYTE_COUNT_32,
		BYTE_COUNT_128,
		BYTE_COUNT_2048,
		BYTE_COUNT_GREATER,

		NUM_BYTE_COUNT_BUCKETS
	};

private:
	struct MemInfo_t
	{
#if defined( USE_STACK_TRACES )
		DECLARE_CALLSTACKSTATSTRUCT();
		DECLARE_CALLSTACKSTATSTRUCT_FIELDDESCRIPTION();
#endif

		MemInfo_t()
		{
			memset( this, 0, sizeof(*this) );
		}

		// Size in bytes
		size_t m_nCurrentSize;
		size_t m_nPeakSize;
		size_t m_nTotalSize;
		size_t m_nOverheadSize;
		size_t m_nPeakOverheadSize;

		// Count in terms of # of allocations
		int m_nCurrentCount;
		int m_nPeakCount;
		int m_nTotalCount;

		int m_nSumTargetRange;
		int m_nCurTargetRange;
		int m_nMaxTargetRange;

		// Count in terms of # of allocations of a particular size
		int m_pCount[NUM_BYTE_COUNT_BUCKETS];

		// Time spent allocating + deallocating	(microseconds)
		int64 m_nTime;
	};

	struct MemInfoKey_FileLine_t
	{
		MemInfoKey_FileLine_t( const char *pFileName, int line ) : m_pFileName(pFileName), m_nLine(line) {}
		bool operator<( const MemInfoKey_FileLine_t &key ) const
		{
			int iret = V_tier0_stricmp( m_pFileName, key.m_pFileName );
			if ( iret < 0 )
				return true;

			if ( iret > 0 )
				return false;

			return m_nLine < key.m_nLine;
		}

		const char *m_pFileName;
		int			m_nLine;
	};	

	// NOTE: Deliberately using STL here because the UTL stuff
	// is a client of this library; want to avoid circular dependency

	// Maps file name to info
	typedef std::map< MemInfoKey_FileLine_t, MemInfo_t, std::less<MemInfoKey_FileLine_t>, CNoRecurseAllocator<std::pair<const MemInfoKey_FileLine_t, MemInfo_t> > > StatMap_FileLine_t;
	typedef StatMap_FileLine_t::iterator StatMapIter_FileLine_t;
	typedef StatMap_FileLine_t::value_type StatMapEntry_FileLine_t;

	typedef std::set<const char *, CStringLess, CNoRecurseAllocator<const char *> > Filenames_t;

	// Heap reporting method
	typedef void (*HeapReportFunc_t)( char const *pFormat, ... );

private:
	// Returns the actual debug info
	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine );

	// Finds the file in our map
	MemInfo_t &FindOrCreateEntry( const char *pFileName, int line );
	const char *FindOrCreateFilename( const char *pFileName );

#if defined( USE_STACK_TRACES )
	int GetCallStackForIndex( unsigned int index, void **pCallStackOut, int iMaxEntriesOut );
	friend int GetAllocationCallStack( void *mem, void **pCallStackOut, int iMaxEntriesOut );
#endif

	// Updates stats
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime );
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime );
#if defined( USE_STACK_TRACES )
	void RegisterAllocation( unsigned int nStatIndex, size_t nLogicalSize, size_t nActualSize, unsigned nTime );
	void RegisterDeallocation( unsigned int nStatIndex, size_t nLogicalSize, size_t nActualSize, unsigned nTime );
#endif
	void RegisterAllocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime );
	void RegisterDeallocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime );

	// Gets the allocation file name
	const char *GetAllocatonFileName( void *pMem );
	int GetAllocatonLineNumber( void *pMem );

	// FIXME: specify a spew output func for dumping stats
	// Stat output
	void DumpMemInfo( const char *pAllocationName, int line, const MemInfo_t &info );
	void DumpFileStats();
#if defined( USE_STACK_TRACES )
	void DumpMemInfo( void * const CallStack[STACK_TRACE_LENGTH], const MemInfo_t &info );
	void DumpCallStackFlow( char const *pchFileBase );
#endif
	virtual void DumpStats();
	virtual void DumpStatsFileBase( char const *pchFileBase );
	virtual void DumpBlockStats( void *p );
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory );
	
	virtual size_t ComputeMemoryUsedBy( char const *pchSubStr );

	virtual IVirtualMemorySection * AllocateVirtualMemorySection( size_t numMaxBytes )
	{
#if defined( _GAMECONSOLE ) || defined( _WIN32 )
		extern IVirtualMemorySection * VirtualMemoryManager_AllocateVirtualMemorySection( size_t numMaxBytes );
		return VirtualMemoryManager_AllocateVirtualMemorySection( numMaxBytes );
#else
		return NULL;
#endif
	}

	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats )
	{
		// TODO: reuse code from GlobalMemoryStatus (though this is only really useful when using CStdMemAlloc...)
		return 0;
	}

private:
	StatMap_FileLine_t m_StatMap_FileLine;
#if defined( USE_STACK_TRACES )
	typedef CCallStackStatsGatherer<MemInfo_t, STACK_TRACE_LENGTH, GetCallStack_Fast, CCallStackStatsGatherer_StatMutexPool<128>, CNoRecurseAllocator> CallStackStatsType_t;
	CallStackStatsType_t m_CallStackStats;
#endif

	MemInfo_t m_GlobalInfo;
	CFastTimer m_Timer;
	bool		m_bInitialized;
	Filenames_t m_Filenames;

	HeapReportFunc_t m_OutputFunc;

	static size_t s_pCountSizes[NUM_BYTE_COUNT_BUCKETS];
	static const char *s_pCountHeader[NUM_BYTE_COUNT_BUCKETS];

	size_t				m_sMemoryAllocFailed;
};

static char const *g_pszUnknown = "unknown";

#if defined( USE_STACK_TRACES )
BEGIN_STATSTRUCTDESCRIPTION( CDbgMemAlloc::MemInfo_t )
	WRITE_STATSTRUCT_FIELDDESCRIPTION();
END_STATSTRUCTDESCRIPTION()


BEGIN_STATSTRUCTFIELDDESCRIPTION( CDbgMemAlloc::MemInfo_t )
	DEFINE_STATSTRUCTFIELD( m_nCurrentSize, BasicStatStructFieldDesc, ( BSSFT_SIZE_T, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nPeakSize, BasicStatStructFieldDesc, ( BSSFT_SIZE_T, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nTotalSize, BasicStatStructFieldDesc, ( BSSFT_SIZE_T, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nOverheadSize, BasicStatStructFieldDesc, ( BSSFT_SIZE_T, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nPeakOverheadSize, BasicStatStructFieldDesc, ( BSSFT_SIZE_T, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nCurrentCount, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nPeakCount, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nTotalCount, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nSumTargetRange, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nCurTargetRange, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nMaxTargetRange, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD_ARRAYENTRY( m_pCount, BYTE_COUNT_16, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD_ARRAYENTRY( m_pCount, BYTE_COUNT_32, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD_ARRAYENTRY( m_pCount, BYTE_COUNT_128, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD_ARRAYENTRY( m_pCount, BYTE_COUNT_2048, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD_ARRAYENTRY( m_pCount, BYTE_COUNT_GREATER, BasicStatStructFieldDesc, ( BSSFT_INT, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( m_nTime, BasicStatStructFieldDesc, ( BSSFT_INT64, BSSFCM_ADD ) )
END_STATSTRUCTFIELDDESCRIPTION()
#endif


//-----------------------------------------------------------------------------

const int DBG_INFO_STACK_DEPTH = 32;

struct DbgInfoStack_t
{
	const char *m_pFileName;
	int m_nLine;
};

#ifdef _PS3
#ifndef _CERT
extern TLSGlobals * ( *g_pfnElfGetTlsGlobals )();
#define IfDbgInfoIsReady() if ( TLSGlobals *IfDbgInfoIsReady_pTlsGlobals = g_pfnElfGetTlsGlobals ? g_pfnElfGetTlsGlobals() : NULL )
#else
#define IfDbgInfoIsReady() if ( TLSGlobals *IfDbgInfoIsReady_pTlsGlobals = GetTLSGlobals() )
#endif
#define g_DbgInfoStack ( ( DbgInfoStack_t *& ) IfDbgInfoIsReady_pTlsGlobals->pMallocDbgInfoStack )
#define g_nDbgInfoStackDepth ( IfDbgInfoIsReady_pTlsGlobals->nMallocDbgInfoStackDepth )
#else
CTHREADLOCALPTR( DbgInfoStack_t)	g_DbgInfoStack CONSTRUCT_EARLY;
CTHREADLOCALINT						g_nDbgInfoStackDepth CONSTRUCT_EARLY;
#define IfDbgInfoIsReady() if (true)
#endif

#ifdef _PS3
struct CDbgMemAlloc_GetRawCrtMemOverrideFuncs_Early
{
	CDbgMemAlloc_GetRawCrtMemOverrideFuncs_Early()
	{
		malloc_managed_size mms;
		mms.current_inuse_size = 0x12345678;
		mms.current_system_size = 0x09ABCDEF;
		mms.max_system_size = 0;
		int iResult = malloc_stats( &mms );
		g_pMemOverrideRawCrtFns = reinterpret_cast< MemOverrideRawCrtFunctions_t * >( iResult );
	}
}
g_CDbgMemAlloc_GetRawCrtMemOverrideFuncs_Early CONSTRUCT_EARLY;
#endif

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
static CDbgMemAlloc s_DbgMemAlloc CONSTRUCT_EARLY;

#ifdef _PS3

IMemAlloc *g_pMemAllocInternalPS3 = &s_DbgMemAlloc;
PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3_IMPL

#else // !_PS3

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc CONSTRUCT_EARLY = &s_DbgMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_DbgMemAlloc;
#endif

#endif // _PS3


//-----------------------------------------------------------------------------

CThreadMutex g_DbgMemMutex CONSTRUCT_EARLY;

#define HEAP_LOCK() AUTO_LOCK( g_DbgMemMutex )


//-----------------------------------------------------------------------------
// Byte count buckets
//-----------------------------------------------------------------------------
size_t CDbgMemAlloc::s_pCountSizes[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = 
{
	16, 32, 128, 2048, INT_MAX
};

const char *CDbgMemAlloc::s_pCountHeader[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = 
{
	"<=16 byte allocations", 
	"17-32 byte allocations",
	"33-128 byte allocations", 
	"129-2048 byte allocations",
	">2048 byte allocations"
};


size_t g_TargetCountRangeMin = 0, g_TargetCountRangeMax = 0;

//-----------------------------------------------------------------------------
// Standard output
//-----------------------------------------------------------------------------
static FILE* s_DbgFile;

static void DefaultHeapReportFunc( char const *pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );
	vfprintf( s_DbgFile, pFormat, args );
	va_end( args );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDbgMemAlloc::CDbgMemAlloc() : m_sMemoryAllocFailed( (size_t)0 )
{
	CClockSpeedInit::Init();

	m_OutputFunc = DefaultHeapReportFunc;
	m_bInitialized = true;

	if ( !IsDebug() && !IsX360() )
	{
		Plat_DebugString( "USE_MEM_DEBUG is enabled in a release build. Don't check this in!\n" );
	}

#ifdef _PS3
	g_pMemAllocInternalPS3 = &s_DbgMemAlloc;
	PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3.m_pMemAllocCached = &s_DbgMemAlloc;
	malloc_managed_size mms;
	mms.current_inuse_size = 0x12345678;
	mms.current_system_size = 0x09ABCDEF;
	mms.max_system_size = reinterpret_cast< size_t >( this );
	int iResult = malloc_stats( &mms );
	g_pMemOverrideRawCrtFns = reinterpret_cast< MemOverrideRawCrtFunctions_t * >( iResult );
#endif
}

CDbgMemAlloc::~CDbgMemAlloc()
{
	Filenames_t::const_iterator iter = m_Filenames.begin();
	while(iter != m_Filenames.end())
	{
		char *pFileName = (char*)(*iter);
		free( pFileName );
		iter++;
	}
	m_bInitialized = false;
}


//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CDbgMemAlloc::Alloc( size_t nSize )
{
/*
	// NOTE: Uncomment this to find unknown allocations
	const char *pFileName = g_pszUnknown;
	int nLine;
	GetActualDbgInfo( pFileName, nLine );
	if (pFileName == g_pszUnknown)
	{
		int x = 3;
	}
*/
	char szModule[MAX_PATH];
	if ( GetCallerModule( szModule, MAX_PATH ) )
	{
		return Alloc( nSize, szModule, 0 );
	}
	else
	{
		return Alloc( nSize, g_pszUnknown, 0 );
	}
//	return malloc( nSize );
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void *CDbgMemAlloc::AllocAlign( size_t nSize, size_t align )
{
/*
	// NOTE: Uncomment this to find unknown allocations
	const char *pFileName = g_pszUnknown;
	int nLine;
	GetActualDbgInfo( pFileName, nLine );
	if (pFileName == g_pszUnknown)
	{
		int x = 3;
	}
*/
	char szModule[MAX_PATH];
	if ( GetCallerModule( szModule, MAX_PATH ) )
	{
		return AllocAlign( nSize, align, szModule, 0 );
	}
	else
	{
		return AllocAlign( nSize, align, g_pszUnknown, 0 );
	}
//	return malloc( nSize );
}
#endif

void *CDbgMemAlloc::Realloc( void *pMem, size_t nSize )
{
/*
	// NOTE: Uncomment this to find unknown allocations
	const char *pFileName = g_pszUnknown;
	int nLine;
	GetActualDbgInfo( pFileName, nLine );
	if (pFileName == g_pszUnknown)
	{
		int x = 3;
	}
*/
	// FIXME: Should these gather stats?
	char szModule[MAX_PATH];
	if ( GetCallerModule( szModule, MAX_PATH ) )
	{
		return Realloc( pMem, nSize, szModule, 0 );
	}
	else
	{
		return Realloc( pMem, nSize, g_pszUnknown, 0 );
	}
//	return realloc( pMem, nSize );
}

void CDbgMemAlloc::Free( void *pMem )
{
	// FIXME: Should these gather stats?
	Free( pMem, g_pszUnknown, 0 );
//	free( pMem );
}

void *CDbgMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CDbgMemAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
	IfDbgInfoIsReady()
	{

		if ( g_DbgInfoStack == NULL )
		{
			g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
			g_nDbgInfoStackDepth = -1;
		}

		++g_nDbgInfoStackDepth;
		Assert( g_nDbgInfoStackDepth < DBG_INFO_STACK_DEPTH );
		g_DbgInfoStack[g_nDbgInfoStackDepth].m_pFileName = FindOrCreateFilename( pFileName );
		g_DbgInfoStack[g_nDbgInfoStackDepth].m_nLine = nLine;

	}
}

void CDbgMemAlloc::PopAllocDbgInfo()
{
	IfDbgInfoIsReady()
	{

		if ( g_DbgInfoStack == NULL )
		{
			g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
			g_nDbgInfoStackDepth = -1;
		}

		--g_nDbgInfoStackDepth;
		Assert( g_nDbgInfoStackDepth >= -1 );

	}
}


//-----------------------------------------------------------------------------
// handles storing allocation info for coroutines
//-----------------------------------------------------------------------------
uint32 CDbgMemAlloc::GetDebugInfoSize()
{
	return sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH + sizeof( int32 );
}

void CDbgMemAlloc::SaveDebugInfo( void *pvDebugInfo )
{
	IfDbgInfoIsReady()
	{
		if ( g_DbgInfoStack == NULL )
		{
			g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
			g_nDbgInfoStackDepth = -1;
		}

		int32 *pnStackDepth = (int32*) pvDebugInfo;
		*pnStackDepth = g_nDbgInfoStackDepth;
		memcpy( pnStackDepth+1, &g_DbgInfoStack[0], sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH );
	}
}

void CDbgMemAlloc::RestoreDebugInfo( const void *pvDebugInfo )
{
	IfDbgInfoIsReady()
	{
		if ( g_DbgInfoStack == NULL )
		{
			g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
			g_nDbgInfoStackDepth = -1;
		}

		const int32 *pnStackDepth = (const int32*) pvDebugInfo;
		g_nDbgInfoStackDepth = *pnStackDepth;
		memcpy( &g_DbgInfoStack[0], pnStackDepth+1, sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH );
	}
}

void CDbgMemAlloc::InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine )
{
	int32 *pnStackDepth = (int32*) pvDebugInfo;
		
	if( pchRootFileName )
	{
		*pnStackDepth = 0;

		DbgInfoStack_t *pStackRoot = (DbgInfoStack_t *)(pnStackDepth + 1);
		pStackRoot->m_pFileName = FindOrCreateFilename( pchRootFileName );
		pStackRoot->m_nLine = nLine;
	}
	else
	{
		*pnStackDepth = -1;
	}

}


//-----------------------------------------------------------------------------
// Returns the actual debug info
//-----------------------------------------------------------------------------
void CDbgMemAlloc::GetActualDbgInfo( const char *&pFileName, int &nLine )
{
#if defined( USE_STACK_TRACES_DETAILED )
	return;
#endif

	IfDbgInfoIsReady()
	{

		if ( g_DbgInfoStack == NULL )
		{
			g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
			g_nDbgInfoStackDepth = -1;
		}

		if ( g_nDbgInfoStackDepth >= 0 && g_DbgInfoStack[0].m_pFileName)
		{
			pFileName = g_DbgInfoStack[0].m_pFileName;
			nLine = g_DbgInfoStack[0].m_nLine;
		}

	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDbgMemAlloc::FindOrCreateFilename( const char *pFileName )
{
	// If we created it for the first time, actually *allocate* the filename memory
	HEAP_LOCK();
	// This is necessary for shutdown conditions: the file name is stored
	// in some piece of memory in a DLL; if that DLL becomes unloaded,
	// we'll have a pointer to crap memory

	if ( !pFileName )
	{
		pFileName = g_pszUnknown;
	}

#if defined( USE_STACK_TRACES_DETAILED )
{

	// Walk the stack to determine what's causing the allocation
	void *arrStackAddresses[ 10 ] = { 0 };
	int numStackAddrRetrieved = GetCallStack_Fast( arrStackAddresses, 10, 2 ); //Skip this function, and either CDbgMemAlloc::Alloc() or CDbgMemAlloc::Realloc()
	char *szStack = StackDescribe( arrStackAddresses, numStackAddrRetrieved );
	if ( szStack && *szStack )
	{
		pFileName = szStack;		// Use the stack description for the allocation
	}

}
#endif // #if defined( USE_STACK_TRACES_DETAILED )

	char *pszFilenameCopy;
	Filenames_t::const_iterator iter = m_Filenames.find( pFileName );
	if ( iter == m_Filenames.end() )
	{
		size_t nLen = strlen(pFileName) + 1;
		pszFilenameCopy = (char *)DebugAlloc( nLen );
		memcpy( pszFilenameCopy, pFileName, nLen );
		m_Filenames.insert( pszFilenameCopy );
	}
	else
	{
		pszFilenameCopy = (char *)(*iter);
	}

	return pszFilenameCopy;
}

//-----------------------------------------------------------------------------
// Finds the file in our map
//-----------------------------------------------------------------------------
CDbgMemAlloc::MemInfo_t &CDbgMemAlloc::FindOrCreateEntry( const char *pFileName, int line )
{
	// Oh how I love crazy STL. retval.first == the StatMapIter_t in the std::pair
	// retval.first->second == the MemInfo_t that's part of the StatMapIter_t 
	std::pair<StatMapIter_FileLine_t, bool> retval;
	retval = m_StatMap_FileLine.insert( StatMapEntry_FileLine_t( MemInfoKey_FileLine_t( pFileName, line ), MemInfo_t() ) );
	return retval.first->second;
}

#if defined( USE_STACK_TRACES )
int CDbgMemAlloc::GetCallStackForIndex( unsigned int index, void **pCallStackOut, int iMaxEntriesOut )
{
	if( iMaxEntriesOut > STACK_TRACE_LENGTH )
		iMaxEntriesOut = STACK_TRACE_LENGTH;

	CallStackStatsType_t::StackReference stackRef = m_CallStackStats.GetCallStackForIndex( index );

	memcpy( pCallStackOut, stackRef, iMaxEntriesOut * sizeof( void * ) );
	for( int i = 0; i != iMaxEntriesOut; ++i )
	{
		if( pCallStackOut[i] == NULL )
			return i;
	}
	return iMaxEntriesOut;
}
#endif


//-----------------------------------------------------------------------------
// Updates stats
//-----------------------------------------------------------------------------
void CDbgMemAlloc::RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterAllocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	RegisterAllocation( FindOrCreateEntry( pFileName, nLine ), nLogicalSize, nActualSize, nTime );
}

void CDbgMemAlloc::RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterDeallocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	RegisterDeallocation( FindOrCreateEntry( pFileName, nLine ), nLogicalSize, nActualSize, nTime );
}

#if defined( USE_STACK_TRACES )
void CDbgMemAlloc::RegisterAllocation( unsigned int nStatIndex, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterAllocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	CCallStackStatsGatherer_StructAccessor_AutoLock<MemInfo_t> entryAccessor = m_CallStackStats.GetEntry( nStatIndex );
	RegisterAllocation( *entryAccessor.GetStruct(), nLogicalSize, nActualSize, nTime );
}

void CDbgMemAlloc::RegisterDeallocation( unsigned int nStatIndex, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterDeallocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	CCallStackStatsGatherer_StructAccessor_AutoLock<MemInfo_t> entryAccessor = m_CallStackStats.GetEntry( nStatIndex );
	RegisterDeallocation( *entryAccessor.GetStruct(), nLogicalSize, nActualSize, nTime );
}
#endif

void CDbgMemAlloc::RegisterAllocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	++info.m_nCurrentCount;
	++info.m_nTotalCount;
	if (info.m_nCurrentCount > info.m_nPeakCount)
	{
		info.m_nPeakCount = info.m_nCurrentCount;
	}

	info.m_nCurrentSize += nLogicalSize;
	info.m_nTotalSize += nLogicalSize;
	if (info.m_nCurrentSize > info.m_nPeakSize)
	{
		info.m_nPeakSize = info.m_nCurrentSize;
	}

	if ( nLogicalSize > g_TargetCountRangeMin && nLogicalSize <= g_TargetCountRangeMax )
	{
		info.m_nSumTargetRange++;
		info.m_nCurTargetRange++;
		if ( info.m_nCurTargetRange > info.m_nMaxTargetRange )
		{
			info.m_nMaxTargetRange = info.m_nCurTargetRange;
		}	
	}

	for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
	{
		if (nLogicalSize <= s_pCountSizes[i])
		{
			++info.m_pCount[i];
			break;
		}
	}

	Assert( info.m_nPeakCount >= info.m_nCurrentCount );
	Assert( info.m_nPeakSize >= info.m_nCurrentSize );

	info.m_nOverheadSize += (nActualSize - nLogicalSize);
	if (info.m_nOverheadSize > info.m_nPeakOverheadSize)
	{
		info.m_nPeakOverheadSize = info.m_nOverheadSize;
	}

	info.m_nTime += nTime;
}

void CDbgMemAlloc::RegisterDeallocation( MemInfo_t &info, size_t nLogicalSize, size_t nActualSize, unsigned nTime )
{
	--info.m_nCurrentCount;
	info.m_nCurrentSize -= nLogicalSize;

	for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
	{
		if (nLogicalSize <= s_pCountSizes[i])
		{
			--info.m_pCount[i];
			break;
		}
	}

	if ( nLogicalSize > g_TargetCountRangeMin && nLogicalSize <= g_TargetCountRangeMax )
	{
		info.m_nCurTargetRange--;
	}

	Assert( info.m_nPeakCount >= info.m_nCurrentCount );
	Assert( info.m_nPeakSize >= info.m_nCurrentSize );
	Assert( info.m_nCurrentCount >= 0 );
	Assert( info.m_nCurrentSize >= 0 );

	info.m_nOverheadSize -= (nActualSize - nLogicalSize);

	info.m_nTime += nTime;
}


//-----------------------------------------------------------------------------
// Gets the allocation file name
//-----------------------------------------------------------------------------

const char *CDbgMemAlloc::GetAllocatonFileName( void *pMem )
{
	if (!pMem)
		return "";

	CrtDbgMemHeader_t *pHeader = GetCrtDbgMemHeader( pMem );
	if ( pHeader->m_pFileName )
		return pHeader->m_pFileName;
	else
		return g_pszUnknown;
}

//-----------------------------------------------------------------------------
// Gets the allocation file name
//-----------------------------------------------------------------------------
int CDbgMemAlloc::GetAllocatonLineNumber( void *pMem )
{
	if ( !pMem )
		return 0;

	CrtDbgMemHeader_t *pHeader = GetCrtDbgMemHeader( pMem );
	return pHeader->m_nLineNumber;
}

//-----------------------------------------------------------------------------
// Debug versions of the main allocation methods
//-----------------------------------------------------------------------------
void *CDbgMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	HEAP_LOCK();

#if defined( USE_STACK_TRACES )
	unsigned int iStatEntryIndex = m_CallStackStats.GetEntryIndex( CCallStackStorage( m_CallStackStats.StackFunction, 1 ) );
#endif

	if ( !m_bInitialized )
	{
		void *pRetval = InternalMalloc( nSize, pFileName, nLine );

#if defined( USE_STACK_TRACES )
		if( pRetval )
		{
			GetAllocationStatIndex_Internal( pRetval ) = iStatEntryIndex;
		}
#endif

		return pRetval;
	}



	if ( pFileName != g_pszUnknown )
		pFileName = FindOrCreateFilename( pFileName );

	GetActualDbgInfo( pFileName, nLine );

	/*
	if ( strcmp( pFileName, "class CUtlVector<int,class CUtlMemory<int> >" ) == 0)
	{
		GetActualDbgInfo( pFileName, nLine );
	}
	*/

	m_Timer.Start();
	void *pMem = InternalMalloc( nSize, pFileName, nLine );
	m_Timer.End();

#if defined( USE_STACK_TRACES )
	if( pMem )
	{
		GetAllocationStatIndex_Internal( pMem ) = iStatEntryIndex;
	}
#endif

	ApplyMemoryInitializations( pMem, nSize );

#if defined( USE_STACK_TRACES )
	RegisterAllocation( GetAllocationStatIndex_Internal( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#else
	RegisterAllocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#endif

	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void *CDbgMemAlloc::AllocAlign( size_t nSize, size_t align, const char *pFileName, int nLine )
{
	HEAP_LOCK();

#if defined( USE_STACK_TRACES )
	unsigned int iStatEntryIndex = m_CallStackStats.GetEntryIndexForCurrentCallStack( 1 );
#endif

	if ( !m_bInitialized )
	{
		void *pRetval = InternalMalloc( nSize, pFileName, nLine );

#if defined( USE_STACK_TRACES )
		if( pRetval )
		{
			GetAllocationStatIndex_Internal( pRetval ) = iStatEntryIndex;
		}
#endif

		return pRetval;
	}



	if ( pFileName != g_pszUnknown )
		pFileName = FindOrCreateFilename( pFileName );

	GetActualDbgInfo( pFileName, nLine );

	/*
	if ( strcmp( pFileName, "class CUtlVector<int,class CUtlMemory<int> >" ) == 0)
	{
		GetActualDbgInfo( pFileName, nLine );
	}
	*/

	m_Timer.Start();
	void *pMem = InternalMallocAligned( nSize, align, pFileName, nLine );
	m_Timer.End();

#if defined( USE_STACK_TRACES )
	if( pMem )
	{
		GetAllocationStatIndex_Internal( pMem ) = iStatEntryIndex;
	}
#endif

	ApplyMemoryInitializations( pMem, nSize );

#if defined( USE_STACK_TRACES )
	RegisterAllocation( GetAllocationStatIndex_Internal( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#else
	RegisterAllocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#endif

	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}
#endif

void *CDbgMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	HEAP_LOCK();

	pFileName = FindOrCreateFilename( pFileName );

#if defined( USE_STACK_TRACES )
	unsigned int iStatEntryIndex = m_CallStackStats.GetEntryIndex( CCallStackStorage( m_CallStackStats.StackFunction, 1 ) );
#endif

	if ( !m_bInitialized )
	{
		pMem = InternalRealloc( pMem, nSize, pFileName, nLine );

#if defined( USE_STACK_TRACES )
		if( pMem )
		{
			GetAllocationStatIndex_Internal( pMem ) = iStatEntryIndex;
		}
#endif
		return pMem;
	}

	if ( pMem != 0 )
	{
#if defined( USE_STACK_TRACES )
		RegisterDeallocation( GetAllocationStatIndex_Internal( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), 0 );
#else
		RegisterDeallocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), 0 );
#endif
	}

	GetActualDbgInfo( pFileName, nLine );

	m_Timer.Start();
	pMem = InternalRealloc( pMem, nSize, pFileName, nLine );
	m_Timer.End();

#if defined( USE_STACK_TRACES )
	if( pMem )
	{
		GetAllocationStatIndex_Internal( pMem ) = iStatEntryIndex;
	}
#endif
	
#if defined( USE_STACK_TRACES )
	RegisterAllocation( GetAllocationStatIndex_Internal( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#else
	RegisterAllocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#endif
	
	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}

#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
void *CDbgMemAlloc::ReallocAlign( void *pMem, size_t nSize, size_t align )
{
/*
	// NOTE: Uncomment this to find unknown allocations
	const char *pFileName = g_pszUnknown;
	int nLine;
	GetActualDbgInfo( pFileName, nLine );
	if (pFileName == g_pszUnknown)
	{
		int x = 3;
	}
*/
	char szModule[MAX_PATH];
	if ( GetCallerModule( szModule, MAX_PATH ) )
	{
		return ReallocAlign( pMem, nSize, align, szModule, 0 );
	}
	else
	{
		return ReallocAlign( pMem, nSize, align, g_pszUnknown, 0 );
	}
//	return malloc( nSize );
}
void *CDbgMemAlloc::ReallocAlign( void *pMem, size_t nSize, size_t align, const char *pFileName, int nLine )
{
	HEAP_LOCK();

	pFileName = FindOrCreateFilename( pFileName );

#if defined( USE_STACK_TRACES )
	unsigned int iStatEntryIndex = m_CallStackStats.GetEntryIndexForCurrentCallStack( 1 );
#endif

	if ( !m_bInitialized )
	{
		pMem = InternalReallocAligned( pMem, nSize, align, pFileName, nLine );

#if defined( USE_STACK_TRACES )
		if( pMem )
		{
			GetAllocationStatIndex_Internal( pMem ) = iStatEntryIndex;
		}
#endif
		return pMem;
	}

	if ( pMem != 0 )
	{
#if defined( USE_STACK_TRACES )
		RegisterDeallocation( GetAllocationStatIndex_Internal( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), 0 );
#else
		RegisterDeallocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), 0 );
#endif
	}

	GetActualDbgInfo( pFileName, nLine );

	m_Timer.Start();
	pMem = InternalReallocAligned( pMem, nSize, align, pFileName, nLine );
	m_Timer.End();

#if defined( USE_STACK_TRACES )
	if( pMem )
	{
		GetAllocationStatIndex_Internal( pMem ) = iStatEntryIndex;
	}
#endif
	
#if defined( USE_STACK_TRACES )
	RegisterAllocation( GetAllocationStatIndex_Internal( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#else
	RegisterAllocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
#endif
	
	if ( !pMem )
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}
#endif

void  CDbgMemAlloc::Free( void *pMem, const char * /*pFileName*/, int nLine )
{
	if ( !pMem )
		return;

	HEAP_LOCK();

	if ( !m_bInitialized )
	{
		InternalFree( pMem );
		return;
	}

	size_t nOldLogicalSize = InternalLogicalSize( pMem );
	size_t nOldSize = InternalMSize( pMem );	

#if defined( USE_STACK_TRACES )
	unsigned int oldStatIndex = GetAllocationStatIndex_Internal( pMem );
#else
	const char *pOldFileName = GetAllocatonFileName( pMem );
	int oldLine = GetAllocatonLineNumber( pMem );
#endif


	m_Timer.Start();
	InternalFree( pMem );
 	m_Timer.End();

#if defined( USE_STACK_TRACES )
	RegisterDeallocation( oldStatIndex, nOldLogicalSize, nOldSize, m_Timer.GetDuration().GetMicroseconds() );
#else
	RegisterDeallocation( pOldFileName, oldLine, nOldLogicalSize, nOldSize, m_Timer.GetDuration().GetMicroseconds() );
#endif
}

void *CDbgMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Returns the size of a particular allocation (NOTE: may be larger than the size requested!)
//-----------------------------------------------------------------------------
size_t CDbgMemAlloc::GetSize( void *pMem )
{
	HEAP_LOCK();

	if ( !pMem )
		return m_GlobalInfo.m_nCurrentSize;

	return InternalMSize( pMem );
}


//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
int32 CDbgMemAlloc::CrtSetBreakAlloc( int32 lNewBreakAlloc )
{
#ifdef POSIX
	return 0;
#else
	return _CrtSetBreakAlloc( lNewBreakAlloc );
#endif
}

int CDbgMemAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
#ifdef POSIX
	return 0;
#else
	return _CrtSetReportMode( nReportType, nReportMode );
#endif
}

int CDbgMemAlloc::CrtIsValidHeapPointer( const void *pMem )
{
#ifdef POSIX
	return 0;
#else
	return _CrtIsValidHeapPointer( pMem );
#endif
}

int CDbgMemAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
#ifdef POSIX
	return 0;
#else
	return _CrtIsValidPointer( pMem, size, access );
#endif
}

#define DBGMEM_CHECKMEMORY 1

int CDbgMemAlloc::CrtCheckMemory( void )
{
#if !defined( DBGMEM_CHECKMEMORY ) || defined( POSIX )
	return 1;
#elif defined( _WIN32 )
	if ( !_CrtCheckMemory())
	{
		Msg( "Memory check failed!\n" );
		return 0;
	}
	return 1;
#else
	return 1;
#endif
}

int CDbgMemAlloc::CrtSetDbgFlag( int nNewFlag )
{
#ifdef POSIX
	return 0;
#else
	return _CrtSetDbgFlag( nNewFlag );
#endif
}

void CDbgMemAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
#ifndef POSIX
	_CrtMemCheckpoint( pState );
#endif
}

// FIXME: Remove when we have our own allocator
void* CDbgMemAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
#ifdef POSIX
	return 0;
#else
	return (void*)_CrtSetReportFile( nRptType, (_HFILE)hFile );
#endif
}

void* CDbgMemAlloc::CrtSetReportHook( void* pfnNewHook )
{
#ifdef POSIX
	return 0;
#else
	return (void*)_CrtSetReportHook( (_CRT_REPORT_HOOK)pfnNewHook );
#endif
}

int CDbgMemAlloc::CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg )
{
#ifdef POSIX
	return 0;
#else
	return _CrtDbgReport( nRptType, szFile, nLine, szModule, pMsg );
#endif
}

int CDbgMemAlloc::heapchk()
{
#ifdef POSIX
	return 0;
#else
	if ( CrtCheckMemory() )
		return _HEAPOK;
	else
		return _HEAPBADPTR;
#endif
}

void CDbgMemAlloc::DumpBlockStats( void *p )
{
	DbgMemHeader_t *pBlock = (DbgMemHeader_t *)p - 1;
	if ( !CrtIsValidHeapPointer( pBlock ) )
	{
		Msg( "0x%x is not valid heap pointer\n", p );
		return;
	}

	const char *pFileName = GetAllocatonFileName( p );
	int line = GetAllocatonLineNumber( p );

	Msg( "0x%x allocated by %s line %d, %d bytes\n", p, pFileName, line, GetSize( p ) );
}

//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpMemInfo( const char *pAllocationName, int line, const MemInfo_t &info )
{
	m_OutputFunc("%s, line %i\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
		pAllocationName,
		line,
		info.m_nCurrentSize / 1024.0f,
		info.m_nPeakSize / 1024.0f,
		info.m_nTotalSize / 1024.0f,
		info.m_nOverheadSize / 1024.0f,
		info.m_nPeakOverheadSize / 1024.0f,
		(int)(info.m_nTime / 1000),
		info.m_nCurrentCount,
		info.m_nPeakCount,
		info.m_nTotalCount,
		info.m_nSumTargetRange,
		info.m_nCurTargetRange,
		info.m_nMaxTargetRange
		);

	for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
	{
		m_OutputFunc( "\t%d", info.m_pCount[i] );
	}

	m_OutputFunc("\n");
}


//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
size_t CDbgMemAlloc::ComputeMemoryUsedBy( char const *pchSubStr)
{
	size_t total = 0;
	StatMapIter_FileLine_t iter = m_StatMap_FileLine.begin();
	while(iter != m_StatMap_FileLine.end())
	{
		if(!pchSubStr || strstr(iter->first.m_pFileName,pchSubStr))
		{
			total += iter->second.m_nCurrentSize;
		}
		iter++;
	}
	return total;
}

void CDbgMemAlloc::DumpFileStats()
{
	StatMapIter_FileLine_t iter = m_StatMap_FileLine.begin();
	while(iter != m_StatMap_FileLine.end())
	{
		DumpMemInfo( iter->first.m_pFileName, iter->first.m_nLine, iter->second );
		iter++;
	}
}

void CDbgMemAlloc::DumpStatsFileBase( char const *pchFileBase )
{
	char szFileName[MAX_PATH];
	static int s_FileCount = 0;
	if (m_OutputFunc == DefaultHeapReportFunc)
	{
		char *pPath = "";
#ifdef _X360
		pPath = "D:\\";
#elif defined( _PS3 )
		pPath = "/app_home/";
#endif
		

#if defined( _MEMTEST ) && defined( _WIN32 )
		char szXboxName[32];
		strcpy( szXboxName, "xbox" );
		DWORD numChars = sizeof( szXboxName );
		DmGetXboxName( szXboxName, &numChars ); 
		char *pXboxName = strstr( szXboxName, "_360" );
		if ( pXboxName )
		{
			*pXboxName = '\0';
		}

		SYSTEMTIME systemTime;
		GetLocalTime( &systemTime );
		//_snprintf( szFileName, sizeof( szFileName ), "%s%s_%2.2d%2.2d_%2.2d%2.2d%2.2d_%d.txt", pPath, s_szStatsMapName, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond, s_FileCount );
		_snprintf( szFileName, sizeof( szFileName ), "%s%s_%d.txt", pPath, s_szStatsMapName, s_FileCount );
#else
		_snprintf( szFileName, sizeof( szFileName ), "%s%s_%d.txt", pPath, pchFileBase, s_FileCount );
#endif

		++s_FileCount;

		s_DbgFile = fopen(szFileName, "wt");
		if (!s_DbgFile)
			return;
	}

	{
		HEAP_LOCK();

		m_OutputFunc("Allocation type\tCurrent Size(k)\tPeak Size(k)\tTotal Allocations(k)\tOverhead Size(k)\tPeak Overhead Size(k)\tTime(ms)\tCurrent Count\tPeak Count\tTotal Count\tTNum\tTCur\tTMax");

		for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
		{
			m_OutputFunc( "\t%s", s_pCountHeader[i] );
		}

		m_OutputFunc("\n");

		MemInfo_t totals = m_GlobalInfo;
#ifdef _PS3
		{
			// Add a line for system heap stats
			static malloc_managed_size mms;
			(g_pMemOverrideRawCrtFns->pfn_malloc_stats)( &mms );

			MemInfo_t info;
			info.m_nCurrentSize		= mms.current_inuse_size;
			info.m_nPeakSize		= mms.max_system_size;
			info.m_nOverheadSize	= mms.current_system_size - mms.current_inuse_size;
			DumpMemInfo( "||PS3 malloc_stats||", 0, info );

			// Add a line for PRXs
			char prxFilename[256];
			sys_prx_id_t prxIDs[256];
			sys_prx_segment_info_t prxSegments[32];
			sys_prx_get_module_list_t prxList = { sizeof( sys_prx_get_module_list_t ), ARRAYSIZE( prxIDs ), 0, prxIDs, NULL };
			sys_prx_get_module_list( 0, &prxList );
			Assert( prxList.count < ARRAYSIZE( prxIDs ) );
			memset( &info, 0, sizeof( info ) );
			for ( int i = 0; i < prxList.count; i++ )
			{
				sys_prx_module_info_t prxInfo;
				prxInfo.size          = sizeof( sys_prx_module_info_t );
				prxInfo.filename      = prxFilename;
				prxInfo.filename_size = sizeof( prxFilename );
				prxInfo.segments      = prxSegments;
				prxInfo.segments_num  = ARRAYSIZE( prxSegments );
				sys_prx_get_module_info( prxList.idlist[i], 0, &prxInfo );
				Assert( prxInfo.segments_num < ARRAYSIZE( prxSegments ) );
				for ( int j = 0; j < prxInfo.segments_num; j++ )
				{
					info.m_nCurrentSize += prxInfo.segments[j].memsz;
				}
			}
			DumpMemInfo( "PS3 PRXs", 0, info );

			// Add PRX sizes to our global tracked total:
			totals.m_nCurrentSize += info.m_nCurrentSize;
		}
#endif // _PS3

		// The total of all memory usage we know about:
		DumpMemInfo( "||Totals||", 0, totals );

		if ( IsGameConsole() )
		{
			// Add a line showing total system memory usage from the OS (if this is more than
			// "||Totals||", then there is unknown memory usage that we need to track down):
			size_t usedMemory, freeMemory;
			GlobalMemoryStatus( &usedMemory, &freeMemory );
			MemInfo_t info;
			info.m_nCurrentSize = usedMemory;
			DumpMemInfo( "||Used Memory||", 0, info );
		}

#ifdef _MEMTEST
		{
			// Add lines for GPU allocations
			int nGPUMemSize, nGPUMemFree, nTextureSize, nRTSize, nVBSize, nIBSize, nUnknown;
			if ( 7 == sscanf( s_szStatsComment, "%d %d %d %d %d %d %d", &nGPUMemSize, &nGPUMemFree, &nTextureSize, &nRTSize, &nVBSize, &nIBSize, &nUnknown ) )
			{
				int nTotalUsed = nTextureSize + nRTSize + nVBSize + nIBSize + nUnknown;
				int nOverhead  = ( nGPUMemSize - nTotalUsed ) - nGPUMemFree;
				m_OutputFunc( "||PS3 RSX: total used||, line 0\t%.1f\n",	nTotalUsed		/ 1024.0f );
				m_OutputFunc( "PS3 RSX: textures, line 0\t%.1f\n",			nTextureSize	/ 1024.0f );
				m_OutputFunc( "PS3 RSX: render targets, line 0\t%.1f\n",	nRTSize			/ 1024.0f );
				m_OutputFunc( "PS3 RSX: vertex buffers, line 0\t%.1f\n",	nVBSize			/ 1024.0f );
				m_OutputFunc( "PS3 RSX: index buffers, line 0\t%.1f\n",		nIBSize			/ 1024.0f );
				m_OutputFunc( "PS3 RSX: unknown, line 0\t%.1f\n",			nUnknown		/ 1024.0f );
				m_OutputFunc( "PS3 RSX: overhead, line 0\t%.1f\n",			nOverhead		/ 1024.0f );
			}
		}
#endif

		//m_OutputFunc("File/Line Based\n");
		DumpFileStats();
	}

	if (m_OutputFunc == DefaultHeapReportFunc)
	{
		fclose(s_DbgFile);

#if defined( _X360 )
		XBX_rMemDump( szFileName );
#endif
	}
}

void CDbgMemAlloc::GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory )
{
	if ( !pUsedMemory || !pFreeMemory )
		return;

#if defined ( _X360 )

	// GlobalMemoryStatus tells us how much physical memory is free
	MEMORYSTATUS stat;
	::GlobalMemoryStatus( &stat );
	*pFreeMemory = stat.dwAvailPhys;

	// Used is total minus free (discount the 32MB system reservation)
	*pUsedMemory = ( stat.dwTotalPhys - 32*1024*1024 ) - *pFreeMemory;

#elif defined( _PS3 )

	// need to factor in how much empty space there is in the heap
	// (since it NEVER returns pages back to the OS after hitting a high-watermark)
	static malloc_managed_size mms;
	(g_pMemOverrideRawCrtFns->pfn_malloc_stats)( &mms );
	int heapFree = mms.current_system_size - mms.current_inuse_size;
	Assert( heapFree >= 0 );

	// sys_memory_get_user_memory_size tells us how much PPU memory is used/free
	static sys_memory_info stat;
	sys_memory_get_user_memory_size( &stat );
	*pFreeMemory  = stat.available_user_memory;
	*pFreeMemory += heapFree;
	*pUsedMemory  = stat.total_user_memory - *pFreeMemory;
	// 213MB are available in retail mode, so adjust free mem to reflect that even if we're in devkit mode
	const size_t RETAIL_SIZE = 213*1024*1024;
	if ( stat.total_user_memory > RETAIL_SIZE )
		*pFreeMemory -= stat.total_user_memory - RETAIL_SIZE;

#else

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;

#endif
}

#ifdef USE_STACK_TRACES
void CDbgMemAlloc::DumpCallStackFlow( char const *pchFileBase )
{
	HEAP_LOCK();

	char szFileName[MAX_PATH];
	static int s_FileCount = 0;
	
	char *pPath = "";
	if ( IsX360() )
	{
		pPath = "D:\\";
	}

#if defined( _MEMTEST ) && defined( _WIN32 )
	char szXboxName[32];
	strcpy( szXboxName, "xbox" );
	DWORD numChars = sizeof( szXboxName );
	DmGetXboxName( szXboxName, &numChars ); 
	char *pXboxName = strstr( szXboxName, "_360" );
	if ( pXboxName )
	{
		*pXboxName = '\0';
	}

	SYSTEMTIME systemTime;
	GetLocalTime( &systemTime );
	_snprintf( szFileName, sizeof( szFileName ), "%s%s_%2.2d%2.2d_%2.2d%2.2d%2.2d_%d.csf", pPath, s_szStatsMapName, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond, s_FileCount );
#else
	_snprintf( szFileName, sizeof( szFileName ), "%s%s%d.vcsf", pPath, pchFileBase, s_FileCount );
#endif

	++s_FileCount;
	m_CallStackStats.DumpToFile( szFileName, false );
}
#endif

//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpStats()
{
	DumpStatsFileBase( "memstats" );
#ifdef USE_STACK_TRACES
	DumpCallStackFlow( "memflow" );
#endif
}

void CDbgMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed = nSize;
	DebuggerBreakIfDebugging();
	char buffer[256];
	_snprintf( buffer, sizeof( buffer ), "***** OUT OF MEMORY! attempted allocation size: %u ****\n", nSize );
#if defined( _PS3 ) && defined( _DEBUG )
	DebuggerBreak();
#endif // _PS3

#ifdef _X360 
	XBX_OutputDebugString( buffer );
	if ( !Plat_IsInDebugSession() )
	{
		XBX_CrashDump( true );
#if defined( _DEMO )
		XLaunchNewImage( XLAUNCH_KEYWORD_DEFAULT_APP, 0 );
#else
		XLaunchNewImage( "default.xex", 0 );
#endif
	}
#elif defined(_WIN32 )
	OutputDebugString( buffer );
	if ( !Plat_IsInDebugSession() )
	{
		AssertFatalMsg( false, buffer );
		abort();
	}
#else
	printf( "%s\n", buffer );
	if ( !Plat_IsInDebugSession() )
	{
		AssertFatalMsg( false, buffer );
		exit( 0 );
	}
#endif
}

size_t CDbgMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}



#ifdef LINUX
//
// Under linux we can ask GLIBC to override malloc for us
//   Base on code from Ryan, http://hg.icculus.org/icculus/mallocmonitor/file/29c4b0d049f7/monitor_client/malloc_hook_glibc.c
//
//
static void *glibc_malloc_hook = NULL;
static void *glibc_realloc_hook = NULL;
static void *glibc_memalign_hook = NULL;
static void *glibc_free_hook = NULL;

/* convenience functions for setting the hooks... */
static inline void save_glibc_hooks(void);
static inline void set_glibc_hooks(void);
static inline void set_override_hooks(void);

CThreadMutex g_HookMutex;
/*
 * Our overriding hooks...they call through to the original C runtime
 *  implementations and report to the monitoring daemon.
 */

static void *override_malloc_hook(size_t s, const void *caller)
{
    void *retval;
    AUTO_LOCK( g_HookMutex );
    set_glibc_hooks();  /* put glibc back in control. */
    retval = InternalMalloc( s, NULL, 0 );
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */

    return(retval);
} /* override_malloc_hook */


static void *override_realloc_hook(void *ptr, size_t s, const void *caller)
{
    void *retval;
    AUTO_LOCK( g_HookMutex );

    set_glibc_hooks();  /* put glibc back in control. */
    retval = InternalRealloc(ptr, s, NULL, 0);  /* call glibc version. */
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */

    return(retval);
} /* override_realloc_hook */


static void *override_memalign_hook(size_t a, size_t s, const void *caller)
{
    void *retval;
    AUTO_LOCK( g_HookMutex );

    set_glibc_hooks();  /* put glibc back in control. */
    retval = memalign(a, s);  /* call glibc version. */
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */

    return(retval);
} /* override_memalign_hook */


static void override_free_hook(void *ptr, const void *caller)
{
    AUTO_LOCK( g_HookMutex );

    set_glibc_hooks();  /* put glibc back in control. */
    InternalFree(ptr);  /* call glibc version. */
    save_glibc_hooks();  /* update in case glibc changed them. */

    set_override_hooks(); /* only restore hooks if daemon is listening */
} /* override_free_hook */



/*
 * Convenience functions for swapping the hooks around...
 */

/*
 * Save a copy of the original allocation hooks, so we can call into them
 *  from our overriding functions. It's possible that glibc might change
 *  these hooks under various conditions (so the manual's examples seem
 *  to suggest), so we update them whenever we finish calling into the
 *  the originals.
 */
static inline void save_glibc_hooks(void)
{
    glibc_malloc_hook = (void *)__malloc_hook;
    glibc_realloc_hook = (void *)__realloc_hook;
    glibc_memalign_hook = (void *)__memalign_hook;
    glibc_free_hook = (void *)__free_hook;
} /* save_glibc_hooks */

/*
 * Restore the hooks to the glibc versions. This is needed since, say,
 *  their realloc() might call malloc() or free() under the hood, etc, so
 *  it's safer to let them have complete control over the subsystem, which
 *  also makes our logging saner, too.
 */
static inline void set_glibc_hooks(void)
{
    __malloc_hook = (void* (*)(size_t, const void*))glibc_malloc_hook;
    __realloc_hook = (void* (*)(void*, size_t, const void*))glibc_realloc_hook;
    __memalign_hook = (void* (*)(size_t, size_t, const void*))glibc_memalign_hook;
    __free_hook = (void (*)(void*, const void*))glibc_free_hook;
} /* set_glibc_hooks */


/*
 * Put our hooks back in place. This should be done after the original
 *  glibc version has been called and we've finished any logging (which
 *  may call glibc functions, too). This sets us up for the next calls from
 *  the application.
 */
static inline void set_override_hooks(void)
{
    __malloc_hook = override_malloc_hook;
    __realloc_hook = override_realloc_hook;
    __memalign_hook = override_memalign_hook;
    __free_hook = override_free_hook;
} /* set_override_hooks */



/*
 * The Hook Of All Hooks...how we get in there in the first place.
 */

/*
 * glibc will call this when the malloc subsystem is initializing, giving
 *  us a chance to install hooks that override the functions.
 */
static void override_init_hook(void)
{
    AUTO_LOCK( g_HookMutex );

    /* install our hooks. Will connect to daemon on first malloc, etc. */
    save_glibc_hooks();
    set_override_hooks();
} /* override_init_hook */


/*
 * __malloc_initialize_hook is apparently a "weak variable", so you can
 *  define and assign it here even though it's in glibc, too. This lets
 *  us hook into malloc as soon as the runtime initializes, and before
 *  main() is called. Basically, this whole trick depends on this.
 */
void (*__malloc_initialize_hook)(void) __attribute__((visibility("default")))= override_init_hook;

#elif defined( OSX )
//
// pointers to the osx versions of these functions
static void *osx_malloc_hook = NULL;
static void *osx_realloc_hook = NULL;
static void *osx_free_hook = NULL;

// convenience functions for setting the hooks... 
static inline void save_osx_hooks(void);
static inline void set_osx_hooks(void);
static inline void set_override_hooks(void);

CThreadMutex g_HookMutex;
//
// Our overriding hooks...they call through to the original C runtime
//  implementations and report to the monitoring daemon.
//

static void *override_malloc_hook(struct _malloc_zone_t *zone, size_t s)
{
    void *retval;
    set_osx_hooks(); 
    retval = InternalMalloc( s, NULL, 0 );
    set_override_hooks(); 
	
    return(retval);
} 


static void *override_realloc_hook(struct _malloc_zone_t *zone, void *ptr, size_t s)
{
    void *retval;
	
    set_osx_hooks();  
    retval = InternalRealloc(ptr, s, NULL, 0);	
    set_override_hooks(); 
	
    return(retval);
} 


static void override_free_hook(struct _malloc_zone_t *zone, void *ptr)
{
	// sometime they pass in a null pointer from higher level calls, just ignore it
	if ( !ptr )
		return;
	
    set_osx_hooks(); 
	
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( ptr );
	if ( *((int*)pInternalMem->m_Reserved) == 0xf00df00d )
	{
		InternalFree( ptr );
	}
    
    set_override_hooks(); 
} 


/*
 
 These are func's we could optionally override right now on OSX but don't need to
 
 static size_t override_size_hook(struct _malloc_zone_t *zone, const void *ptr)
 {
 set_osx_hooks();  
 DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( (void *)ptr );
 set_override_hooks(); 
 if ( *((int*)pInternalMem->m_Reserved) == 0xf00df00d )
 {
 return pInternalMem->nLogicalSize;
 }
 return 0;
 } 
 
 
 static void *override_calloc_hook(struct _malloc_zone_t *zone, size_t num_items, size_t size )
 {
 void *ans = override_malloc_hook( zone, num_items*size );
 if ( !ans )
 return 0;
 memset( ans, 0x0, num_items*size );
 return ans;
 }
 
 static void *override_valloc_hook(struct _malloc_zone_t *zone, size_t size )
 {
 return override_calloc_hook( zone, 1, size );
 }
 
 static void override_destroy_hook(struct _malloc_zone_t *zone)
 {
 }
 */



//
//  Save a copy of the original allocation hooks, so we can call into them
//   from our overriding functions. It's possible that osx might change
//   these hooks under various conditions (so the manual's examples seem
//   to suggest), so we update them whenever we finish calling into the
//   the originals.
//
static inline void save_osx_hooks(void)
{ 
	malloc_zone_t *malloc_zone = malloc_default_zone();
	
    osx_malloc_hook = (void *)malloc_zone->malloc;
    osx_realloc_hook = (void *)malloc_zone->realloc;
    osx_free_hook = (void *)malloc_zone->free;
	
	// These are func's we could optionally override right now on OSX but don't need to
	// osx_size_hook = (void *)malloc_zone->size;
	// osx_calloc_hook = (void *)malloc_zone->calloc;
	// osx_valloc_hook = (void *)malloc_zone->valloc;
	// osx_destroy_hook = (void *)malloc_zone->destroy;
} 

//
//  Restore the hooks to the osx versions. This is needed since, say,
//   their realloc() might call malloc() or free() under the hood, etc, so
//   it's safer to let them have complete control over the subsystem, which
//   also makes our logging saner, too.
// 
static inline void set_osx_hooks(void)
{
	malloc_zone_t *malloc_zone = malloc_default_zone();
	malloc_zone->malloc = (void* (*)(_malloc_zone_t*, size_t))osx_malloc_hook;
    malloc_zone->realloc = (void* (*)(_malloc_zone_t*, void*, size_t))osx_realloc_hook;
    malloc_zone->free = (void (*)(_malloc_zone_t*, void*))osx_free_hook;
	
	// These are func's we could optionally override right now on OSX but don't need to
	
	//malloc_zone->size = (size_t (*)(_malloc_zone_t*, const void *))osx_size_hook;
    //malloc_zone->calloc = (void* (*)(_malloc_zone_t*, size_t, size_t))osx_calloc_hook;
    //malloc_zone->valloc = (void* (*)(_malloc_zone_t*, size_t))osx_valloc_hook;
    //malloc_zone->destroy = (void (*)(_malloc_zone_t*))osx_destroy_hook;
} 


/*
 * Put our hooks back in place. This should be done after the original
 *  osx version has been called and we've finished any logging (which
 *  may call osx functions, too). This sets us up for the next calls from
 *  the application.
 */
static inline void set_override_hooks(void)
{
	malloc_zone_t *malloc_zone = malloc_default_zone();
	
	malloc_zone->malloc = override_malloc_hook;
    malloc_zone->realloc = override_realloc_hook;
    malloc_zone->free = override_free_hook;
	
	// These are func's we could optionally override right now on OSX but don't need to
	//malloc_zone->size = override_size_hook;
    //malloc_zone->calloc = override_calloc_hook;
	// malloc_zone->valloc = override_valloc_hook;
    //malloc_zone->destroy = override_destroy_hook;
} 


//
// The Hook Of All Hooks...how we get in there in the first place.
//
// osx will call this when the malloc subsystem is initializing, giving
// us a chance to install hooks that override the functions.
//

void __attribute__ ((constructor)) mem_init(void)
{
    AUTO_LOCK( g_HookMutex );
	save_osx_hooks();
    set_override_hooks();
}

void *operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	set_osx_hooks(); 
	void *pMem = g_pMemAlloc->Alloc(nSize, pFileName, nLine);
	set_override_hooks(); 
	return pMem;
}

void *operator new[] ( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	set_osx_hooks(); 
	void *pMem = g_pMemAlloc->Alloc(nSize, pFileName, nLine);
	set_override_hooks(); 
	return pMem;
}

#endif // OSX

int GetAllocationCallStack( void *mem, void **pCallStackOut, int iMaxEntriesOut )
{
#if defined( USE_MEM_DEBUG ) && (defined( USE_STACK_TRACES ))
	return s_DbgMemAlloc.GetCallStackForIndex( GetAllocationStatIndex_Internal( mem ), pCallStackOut, iMaxEntriesOut );
#else
	return 0;
#endif
}


#endif // MEM_IMPL_TYPE_DBG

#endif // !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
