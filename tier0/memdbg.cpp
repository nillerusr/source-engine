//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//


#include "pch_tier0.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include <malloc.h>
#include <string.h>
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "mem_helpers.h"
#ifdef _WIN32
#include <crtdbg.h>
#endif
#ifdef OSX
#include <malloc/malloc.h>
#include <mach/mach.h>
#include <stdlib.h>
#endif

#include <map>
#include <set>
#include <limits.h>
#include "tier0/threadtools.h"
#ifdef _X360
#include "xbox/xbox_console.h"
#endif
#if ( !defined(_DEBUG) && defined(USE_MEM_DEBUG) )
#pragma message ("USE_MEM_DEBUG is enabled in a release build. Don't check this in!")
#endif
#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))

#if defined(_WIN32) && ( !defined(_X360) && !defined(_WIN64) )
// #define USE_STACK_WALK
// or:
// #define USE_STACK_WALK_DETAILED
#endif

//-----------------------------------------------------------------------------

#ifndef _X360
#define DebugAlloc	malloc
#define DebugFree	free
#else
#define DebugAlloc	DmAllocatePool
#define DebugFree	DmFreePool
#endif

#ifdef WIN32
int g_DefaultHeapFlags = _CrtSetDbgFlag( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF );
#endif

#if defined( _MEMTEST )
static char s_szStatsMapName[32];
static char s_szStatsComment[256];
#endif

//-----------------------------------------------------------------------------

#if defined( USE_STACK_WALK ) || defined( USE_STACK_WALK_DETAILED )
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib" )

#pragma auto_inline(off)
__declspec(naked) DWORD GetEIP()
{
	__asm 
	{
		mov eax, [ebp + 4]
		ret
	}
}

int WalkStack( void **ppAddresses, int nMaxAddresses, int nSkip = 0 )
{
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	STACKFRAME64 frame;

	memset(&frame, 0, sizeof(frame));
	DWORD valEsp, valEbp;
	__asm
	{
		mov [valEsp], esp;
		mov [valEbp], ebp
	}
	frame.AddrPC.Offset    = GetEIP();
	frame.AddrStack.Offset = valEsp;
	frame.AddrFrame.Offset = valEbp;
	frame.AddrPC.Mode      = AddrModeFlat;
	frame.AddrStack.Mode   = AddrModeFlat;
	frame.AddrFrame.Mode   = AddrModeFlat;

	// Walk the stack.
	int nWalked = 0;
	nSkip++;
	while ( nMaxAddresses - nWalked > 0 ) 
	{
		if ( !StackWalk64(IMAGE_FILE_MACHINE_I386, hProcess, hThread, &frame, NULL, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL ) ) 
		{
			break;
		}

		if ( nSkip == 0 )
		{
			if (frame.AddrFrame.Offset == 0) 
			{
				// End of stack.
				break;
			}

			*ppAddresses++ = (void *)frame.AddrPC.Offset;
			nWalked++;
			
			if (frame.AddrPC.Offset == frame.AddrReturn.Offset)
			{
				// Catching a stack loop
				break;
			}
		}
		else
		{
			nSkip--;
		}
	}

	if ( nMaxAddresses )
	{
		memset( ppAddresses, 0, ( nMaxAddresses - nWalked ) * sizeof(*ppAddresses) );
	}

	return nWalked;
}

bool GetModuleFromAddress( void *address, char *pResult )
{
	IMAGEHLP_MODULE   moduleInfo;

	moduleInfo.SizeOfStruct = sizeof(moduleInfo);

	if ( SymGetModuleInfo( GetCurrentProcess(), (DWORD)address, &moduleInfo ) )
	{
		strcpy( pResult, moduleInfo.ModuleName );
		return true;
	}

	return false;
}

bool GetCallerModule( char *pDest )
{
	static bool bInit;
	if ( !bInit )
	{
		PSTR psUserSearchPath = NULL;
		psUserSearchPath = "u:\\data\\game\\bin\\;u:\\data\\game\\episodic\\bin\\;u:\\data\\game\\hl2\\bin\\;\\\\perforce\\symbols";
		SymInitialize( GetCurrentProcess(), psUserSearchPath, true );
		bInit = true;
	}
	void *pCaller;
	WalkStack( &pCaller, 1, 2 );

	return ( pCaller != 0 && GetModuleFromAddress( pCaller, pDest ) );
}


#if defined( USE_STACK_WALK_DETAILED )

//
// Note: StackDescribe function is non-reentrant:
//		Reason:   Stack description is stored in a static buffer.
//		Solution: Passing caller-allocated buffers would allow the
//		function to become reentrant, however the current only client (FindOrCreateFilename)
//		is synchronized with a heap mutex, after retrieving stack description the
//		heap memory will be allocated to copy the text.
//

char * StackDescribe( void **ppAddresses, int nMaxAddresses )
{
	static char s_chStackDescription[ 32 * 1024 ];
	static char s_chSymbolBuffer[ sizeof( IMAGEHLP_SYMBOL64 ) + 1024 ];
	
	IMAGEHLP_SYMBOL64 &hlpSymbol = * ( IMAGEHLP_SYMBOL64 * ) s_chSymbolBuffer;
	hlpSymbol.SizeOfStruct = sizeof( IMAGEHLP_SYMBOL64 );
	hlpSymbol.MaxNameLength = 1024;
	DWORD64 hlpSymbolOffset = 0;

	IMAGEHLP_LINE64 hlpLine;
	hlpLine.SizeOfStruct = sizeof( IMAGEHLP_LINE64 );
	DWORD hlpLineOffset = 0;

	s_chStackDescription[ 0 ] = 0;
	char *pchBuffer = s_chStackDescription;

	for ( int k = 0; k < nMaxAddresses; ++ k )
	{
		if ( !ppAddresses[k] )
			break;

		pchBuffer += strlen( pchBuffer );
		if ( SymGetLineFromAddr64( GetCurrentProcess(), ( DWORD64 ) ppAddresses[k], &hlpLineOffset, &hlpLine ) )
		{
			char const *pchFileName = hlpLine.FileName ? hlpLine.FileName + strlen( hlpLine.FileName ) : NULL;
			for ( size_t numSlashesAllowed = 2; pchFileName > hlpLine.FileName; -- pchFileName )
			{
				if ( *pchFileName == '\\' )
				{
					if ( numSlashesAllowed -- )
						continue;
					else
						break;
				}
			}
			sprintf( pchBuffer, hlpLineOffset ? "%s:%d+0x%I32X" : "%s:%d", pchFileName, hlpLine.LineNumber, hlpLineOffset );
		}
		else if ( SymGetSymFromAddr64( GetCurrentProcess(), ( DWORD64 ) ppAddresses[k], &hlpSymbolOffset, &hlpSymbol ) )
		{
			sprintf( pchBuffer, ( hlpSymbolOffset > 0 && !( hlpSymbolOffset >> 63 ) ) ? "%s+0x%I64X" : "%s", hlpSymbol.Name, hlpSymbolOffset );
		}
		else
		{
			sprintf( pchBuffer, "#0x%08p", ppAddresses[k] );
		}

		pchBuffer += strlen( pchBuffer );
		sprintf( pchBuffer, "<--" );
	}
	*pchBuffer  = 0;

	return s_chStackDescription;
}

#endif // #if defined( USE_STACK_WALK_DETAILED )

#else

inline int WalkStack( void **ppAddresses, int nMaxAddresses, int nSkip = 0 )
{
	memset( ppAddresses, 0, nMaxAddresses * sizeof(*ppAddresses) );
	return 0;
}
#define GetModuleFromAddress( address, pResult ) ( ( *pResult = 0 ), 0)
#define GetCallerModule( pDest ) false
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

struct DbgMemHeader_t
#if !defined( _DEBUG ) || defined( POSIX )
	: CrtDbgMemHeader_t
#endif
{
	unsigned nLogicalSize;
	byte reserved[12];	// MS allocator always returns mem aligned on 16 bytes, which some of our code depends on
};

//-----------------------------------------------------------------------------

#if defined( _DEBUG ) && !defined( POSIX )
#define GetCrtDbgMemHeader( pMem ) ((CrtDbgMemHeader_t*)((DbgMemHeader_t*)pMem - 1) - 1)
#elif defined( OSX )
DbgMemHeader_t *GetCrtDbgMemHeader( void *pMem );
#else
#define GetCrtDbgMemHeader( pMem ) ((DbgMemHeader_t*)pMem - 1)
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
#ifdef OSX
	void *pAllocedMem = malloc_zone_malloc( malloc_default_zone(), nSize + sizeof(DbgMemHeader_t) );
	if (!pAllocedMem)
	{
		return NULL;
	}
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pAllocedMem );

	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = nSize;
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	return pAllocedMem;
#else // LINUX || WIN32
	DbgMemHeader_t *pInternalMem;
#if defined( POSIX ) || !defined( _DEBUG )
	pInternalMem = (DbgMemHeader_t *)malloc( nSize + sizeof(DbgMemHeader_t) );
	if (!pInternalMem)
	{
		return NULL;
	}
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;
#else
	pInternalMem = (DbgMemHeader_t *)_malloc_dbg( nSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif // defined( POSIX ) || !defined( _DEBUG )

	pInternalMem->nLogicalSize = nSize;
	return pInternalMem + 1;
#endif // LINUX || WIN32
}

inline void *InternalRealloc( void *pMem, size_t nNewSize, const char *pFileName, int nLine )
{
	if ( !pMem )
		return InternalMalloc( nNewSize, pFileName, nLine );

#ifdef OSX
	void *pNewAllocedMem = NULL;

	pNewAllocedMem = (void *)malloc_zone_realloc( malloc_default_zone(), pMem, nNewSize + sizeof(DbgMemHeader_t) );
	DbgMemHeader_t *pInternalMem = GetCrtDbgMemHeader( pNewAllocedMem );

	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
	pInternalMem->nLogicalSize = static_cast<unsigned int>( nNewSize );
	*((int*)pInternalMem->m_Reserved) = 0xf00df00d;

	return pNewAllocedMem;
#else // LINUX || WIN32
	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#if defined( POSIX ) || !defined( _DEBUG )
	pInternalMem = (DbgMemHeader_t *)realloc( pInternalMem, nNewSize + sizeof(DbgMemHeader_t) );
	pInternalMem->m_pFileName = pFileName;
	pInternalMem->m_nLineNumber = nLine;
#else
	pInternalMem = (DbgMemHeader_t *)_realloc_dbg( pInternalMem, nNewSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName, nLine );
#endif

	pInternalMem->nLogicalSize = nNewSize;
	return pInternalMem + 1;
#endif // LINUX || WIN32
}

inline void InternalFree( void *pMem )
{
	if ( !pMem )
		return;

	DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#if !defined( _DEBUG ) || defined( POSIX )
#ifdef OSX
	malloc_zone_free( malloc_default_zone(), pMem );
#elif LINUX
	free( pInternalMem );
#else
	free( pInternalMem );	
#endif
#else
	_free_dbg( pInternalMem, _NORMAL_BLOCK );
#endif
}

inline size_t InternalMSize( void *pMem )
{
	//$ TODO. For Linux, we could use 'int size = malloc_usable_size( pMem )'...
#if defined(POSIX)
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
		return ( stricmp( pszLeft, pszRight ) < 0 );
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

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, int nLine );
    virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine );
    virtual void  Free( void *pMem, const char *pFileName, int nLine );
    virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine );

	// Returns size of a particular allocation
	virtual size_t GetSize( void *pMem );

    // Force file + line information for an allocation
    virtual void PushAllocDbgInfo( const char *pFileName, int nLine );
    virtual void PopAllocDbgInfo();

	virtual long CrtSetBreakAlloc( long lNewBreakAlloc );
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

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) { return NULL; } // debug heap doesn't attempt retries

#if defined( _MEMTEST )
	void SetStatsExtraInfo( const char *pMapName, const char *pComment )
	{
		strncpy( s_szStatsMapName, pMapName, sizeof( s_szStatsMapName ) );
		s_szStatsMapName[sizeof( s_szStatsMapName ) - 1] = '\0';

		strncpy( s_szStatsComment, pComment, sizeof( s_szStatsComment ) );
		s_szStatsComment[sizeof( s_szStatsComment ) - 1] = '\0';
	}
#endif

	virtual size_t MemoryAllocFailed();
	void		SetCRTAllocFailed( size_t nMemSize );

	enum
	{
		BYTE_COUNT_16 = 0,
		BYTE_COUNT_32,
		BYTE_COUNT_128,
		BYTE_COUNT_1024,
		BYTE_COUNT_GREATER,

		NUM_BYTE_COUNT_BUCKETS
	};

	void Shutdown();

private:
	struct MemInfo_t
	{
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
		size_t m_nCurrentCount;
		size_t m_nPeakCount;
		size_t m_nTotalCount;

		// Count in terms of # of allocations of a particular size
		size_t m_pCount[NUM_BYTE_COUNT_BUCKETS];

		// Time spent allocating + deallocating	(microseconds)
		int64 m_nTime;
	};

	struct MemInfoKey_t
	{
		MemInfoKey_t( const char *pFileName, int line ) : m_pFileName(pFileName), m_nLine(line) {}
		bool operator<( const MemInfoKey_t &key ) const
		{
			int iret = stricmp( m_pFileName, key.m_pFileName );
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
	typedef std::map< MemInfoKey_t, MemInfo_t, std::less<MemInfoKey_t>, CNoRecurseAllocator<std::pair<const MemInfoKey_t, MemInfo_t> > > StatMap_t;
	typedef StatMap_t::iterator StatMapIter_t;
	typedef StatMap_t::value_type StatMapEntry_t;

	typedef std::set<const char *, CStringLess, CNoRecurseAllocator<const char *> > Filenames_t;

	// Heap reporting method
	typedef void (*HeapReportFunc_t)( char const *pFormat, ... );

private:
	// Returns the actual debug info
	void GetActualDbgInfo( const char *&pFileName, int &nLine );

	void Initialize();

	// Finds the file in our map
	MemInfo_t &FindOrCreateEntry( const char *pFileName, int line );
	const char *FindOrCreateFilename( const char *pFileName );

	// Updates stats
	void RegisterAllocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime );
	void RegisterDeallocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime );

	void RegisterAllocation( MemInfo_t &info, int nLogicalSize, int nActualSize, unsigned nTime );
	void RegisterDeallocation( MemInfo_t &info, int nLogicalSize, int nActualSize, unsigned nTime );

	// Gets the allocation file name
	const char *GetAllocatonFileName( void *pMem );
	int GetAllocatonLineNumber( void *pMem );

	// FIXME: specify a spew output func for dumping stats
	// Stat output
	void DumpMemInfo( const char *pAllocationName, int line, const MemInfo_t &info );
	void DumpFileStats();
	void DumpStats();
	void DumpStatsFileBase( char const *pchFileBase );
	void DumpBlockStats( void *p );
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory );

private:
	StatMap_t *m_pStatMap;
	MemInfo_t m_GlobalInfo;
	CFastTimer m_Timer;
	bool		m_bInitialized;
	Filenames_t *m_pFilenames;

	HeapReportFunc_t m_OutputFunc;

	static int s_pCountSizes[NUM_BYTE_COUNT_BUCKETS];
	static const char *s_pCountHeader[NUM_BYTE_COUNT_BUCKETS];

	size_t				m_sMemoryAllocFailed;
};

static char const *g_pszUnknown = "unknown";

//-----------------------------------------------------------------------------

const int DBG_INFO_STACK_DEPTH = 32;

struct DbgInfoStack_t
{
	const char *m_pFileName;
	int m_nLine;
};

CThreadLocalPtr<DbgInfoStack_t> g_DbgInfoStack CONSTRUCT_EARLY;
CThreadLocalInt<>				g_nDbgInfoStackDepth CONSTRUCT_EARLY;

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
static CDbgMemAlloc s_DbgMemAlloc CONSTRUCT_EARLY;

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc = &s_DbgMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_DbgMemAlloc;
#endif

//-----------------------------------------------------------------------------

CThreadMutex g_DbgMemMutex CONSTRUCT_EARLY;

#define HEAP_LOCK() AUTO_LOCK( g_DbgMemMutex )


//-----------------------------------------------------------------------------
// Byte count buckets
//-----------------------------------------------------------------------------
int CDbgMemAlloc::s_pCountSizes[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = 
{
	16, 32, 128, 1024, INT_MAX
};

const char *CDbgMemAlloc::s_pCountHeader[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = 
{
	"<=16 byte allocations", 
	"17-32 byte allocations",
	"33-128 byte allocations", 
	"129-1024 byte allocations",
	">1024 byte allocations"
};

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
	// Make sure that we return 64-bit addresses in 64-bit builds.
	ReserveBottomMemory();

	m_OutputFunc = DefaultHeapReportFunc;
	m_bInitialized = false;

	if ( !IsDebug() && !IsX360() )
	{
		Plat_DebugString( "USE_MEM_DEBUG is enabled in a release build. Don't check this in!\n" );
	}
}

CDbgMemAlloc::~CDbgMemAlloc()
{
	Shutdown();
}


void CDbgMemAlloc::Initialize()
{
	if ( !m_bInitialized )
	{
		m_pFilenames = new Filenames_t;
		m_pStatMap= new StatMap_t;
		m_bInitialized = true;
	}
}


//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------
void CDbgMemAlloc::Shutdown()
{
	if ( m_bInitialized )
	{
		Filenames_t::const_iterator iter = m_pFilenames->begin();
		while ( iter != m_pFilenames->end() )
		{
			char *pFileName = (char*)(*iter);
			free( pFileName );
			iter++;
		}
		m_pFilenames->clear();

		m_bInitialized = false;

		delete m_pFilenames;
		m_pFilenames = nullptr;

		delete m_pStatMap;
		m_pStatMap = nullptr;
	}

	m_bInitialized = false;
}


#ifdef WIN32
extern "C" BOOL APIENTRY MemDbgDllMain( HMODULE hDll, DWORD dwReason, PVOID pvReserved )
{
	UNREFERENCED_PARAMETER( pvReserved );

	// Check if we are shutting down
	if ( dwReason == DLL_PROCESS_DETACH )
	{
		// CDbgMemAlloc is a global object and destructs after the _Lockit object in the CRT runtime,
		//  so we can't actually operate on the STL object in a normal destructor here as its support libraries have been turned off already
		s_DbgMemAlloc.Shutdown();
	}

	return TRUE;
}
#endif


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
	if ( GetCallerModule( szModule ) )
	{
		return Alloc( nSize, szModule, 0 );
	}
	else
	{
		return Alloc( nSize, g_pszUnknown, 0 );
	}
//	return malloc( nSize );
}

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
	if ( GetCallerModule( szModule ) )
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

void CDbgMemAlloc::PopAllocDbgInfo()
{
	if ( g_DbgInfoStack == NULL )
	{
		g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
		g_nDbgInfoStackDepth = -1;
	}

	--g_nDbgInfoStackDepth;
	Assert( g_nDbgInfoStackDepth >= -1 );
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
	if ( g_DbgInfoStack == NULL )
	{
		g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc( sizeof(DbgInfoStack_t) * DBG_INFO_STACK_DEPTH );
		g_nDbgInfoStackDepth = -1;
	}

	int32 *pnStackDepth = (int32*) pvDebugInfo;
	*pnStackDepth = g_nDbgInfoStackDepth;
	memcpy( pnStackDepth+1, &g_DbgInfoStack[0], sizeof( DbgInfoStack_t ) * DBG_INFO_STACK_DEPTH );
}

void CDbgMemAlloc::RestoreDebugInfo( const void *pvDebugInfo )
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
#if defined( USE_STACK_WALK_DETAILED )
	return;
#endif

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


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDbgMemAlloc::FindOrCreateFilename( const char *pFileName )
{
	Initialize();

	// If we created it for the first time, actually *allocate* the filename memory
	HEAP_LOCK();
	// This is necessary for shutdown conditions: the file name is stored
	// in some piece of memory in a DLL; if that DLL becomes unloaded,
	// we'll have a pointer to crap memory

	if ( !pFileName )
	{
		pFileName = g_pszUnknown;
	}

#if defined( USE_STACK_WALK_DETAILED )
{

	// Walk the stack to determine what's causing the allocation
	void *arrStackAddresses[ 10 ] = { 0 };
	int numStackAddrRetrieved = WalkStack( arrStackAddresses, 10, 0 );
	char *szStack = StackDescribe( arrStackAddresses, numStackAddrRetrieved );
	if ( szStack && *szStack )
	{
		pFileName = szStack;		// Use the stack description for the allocation
	}

}
#endif // #if defined( USE_STACK_WALK_DETAILED )

	char *pszFilenameCopy;
	Filenames_t::const_iterator iter = m_pFilenames->find( pFileName );
	if ( iter == m_pFilenames->end() )
	{
		int nLen = strlen(pFileName) + 1;
		pszFilenameCopy = (char *)DebugAlloc( nLen );
		memcpy( pszFilenameCopy, pFileName, nLen );
		m_pFilenames->insert( pszFilenameCopy );
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
	Initialize();
	// Oh how I love crazy STL. retval.first == the StatMapIter_t in the std::pair
	// retval.first->second == the MemInfo_t that's part of the StatMapIter_t 
	std::pair<StatMapIter_t, bool> retval;
	if ( m_pStatMap )
	{
		retval = m_pStatMap->insert( StatMapEntry_t( MemInfoKey_t( pFileName, line ), MemInfo_t() ) );
	}
	return retval.first->second;
}


//-----------------------------------------------------------------------------
// Updates stats
//-----------------------------------------------------------------------------
void CDbgMemAlloc::RegisterAllocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterAllocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	RegisterAllocation( FindOrCreateEntry( pFileName, nLine ), nLogicalSize, nActualSize, nTime );
}

void CDbgMemAlloc::RegisterDeallocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime )
{
	HEAP_LOCK();
	RegisterDeallocation( m_GlobalInfo, nLogicalSize, nActualSize, nTime );
	RegisterDeallocation( FindOrCreateEntry( pFileName, nLine ), nLogicalSize, nActualSize, nTime );
}

void CDbgMemAlloc::RegisterAllocation( MemInfo_t &info, int nLogicalSize, int nActualSize, unsigned nTime )
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

void CDbgMemAlloc::RegisterDeallocation( MemInfo_t &info, int nLogicalSize, int nActualSize, unsigned nTime )
{
	// Check for decrementing these counters below zero. The checks
	// must be done here because these unsigned counters will wrap-around and
	// still be positive.
	Assert( info.m_nCurrentCount != 0 );

	// It is technically legal for code to request allocations of zero bytes, and there are a number of places in our code
	// that do. So only assert that nLogicalSize >= 0. http://stackoverflow.com/questions/1087042/c-new-int0-will-it-allocate-memory
	Assert( nLogicalSize >= 0 );
	Assert( info.m_nCurrentSize >= (size_t)nLogicalSize );
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

	Assert( info.m_nPeakCount >= info.m_nCurrentCount );
	Assert( info.m_nPeakSize >= info.m_nCurrentSize );

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

	if ( !m_bInitialized )
		return InternalMalloc( nSize, pFileName, nLine );

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

	ApplyMemoryInitializations( pMem, nSize );

	if ( pMem )
	{
		RegisterAllocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem ), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
	}
	else
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}

void *CDbgMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	HEAP_LOCK();

	pFileName = FindOrCreateFilename( pFileName );

	if ( !m_bInitialized )
		return InternalRealloc( pMem, nSize, pFileName, nLine );

	if ( pMem != 0 )
	{
		RegisterDeallocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem), InternalMSize( pMem ), 0 );
	}

	GetActualDbgInfo( pFileName, nLine );

	m_Timer.Start();
	pMem = InternalRealloc( pMem, nSize, pFileName, nLine );
	m_Timer.End();

	if ( pMem )
	{
		RegisterAllocation( GetAllocatonFileName( pMem ), GetAllocatonLineNumber( pMem ), InternalLogicalSize( pMem), InternalMSize( pMem ), m_Timer.GetDuration().GetMicroseconds() );
	}
	else
	{
		SetCRTAllocFailed( nSize );
	}
	return pMem;
}

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

	int nOldLogicalSize = InternalLogicalSize( pMem );
	int nOldSize = InternalMSize( pMem );
	const char *pOldFileName = GetAllocatonFileName( pMem );
	int oldLine = GetAllocatonLineNumber( pMem );

	m_Timer.Start();
	InternalFree( pMem );
 	m_Timer.End();

	RegisterDeallocation( pOldFileName, oldLine, nOldLogicalSize, nOldSize, m_Timer.GetDuration().GetMicroseconds() );
}

void *CDbgMemAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Returns size of a particular allocation
//-----------------------------------------------------------------------------
size_t CDbgMemAlloc::GetSize( void *pMem )
{
	HEAP_LOCK();

	if ( !pMem )
		return CalcHeapUsed();

	return InternalMSize( pMem );
}


//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
long CDbgMemAlloc::CrtSetBreakAlloc( long lNewBreakAlloc )
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
#else
	if ( !_CrtCheckMemory())
	{
		Msg( "Memory check failed!\n" );
		return 0;
	}
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
	return _HEAPOK;
#endif
}

void CDbgMemAlloc::DumpBlockStats( void *p )
{
	DbgMemHeader_t *pBlock = (DbgMemHeader_t *)p - 1;
	if ( !CrtIsValidHeapPointer( pBlock ) )
	{
		Msg( "0x%p is not valid heap pointer\n", p );
		return;
	}

	const char *pFileName = GetAllocatonFileName( p );
	int line = GetAllocatonLineNumber( p );

	Msg( "0x%p allocated by %s line %d, %llu bytes\n", p, pFileName, line, (uint64)GetSize( p ) );
}

//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpMemInfo( const char *pAllocationName, int line, const MemInfo_t &info )
{
	m_OutputFunc("%s, line %i\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\t%d\t%d\t%d",
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
		info.m_nTotalCount
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
void CDbgMemAlloc::DumpFileStats()
{
	if ( !m_pStatMap )
		return;

	StatMapIter_t iter = m_pStatMap->begin();
	while ( iter != m_pStatMap->end() )
	{
		DumpMemInfo( iter->first.m_pFileName, iter->first.m_nLine, iter->second );
		iter++;
	}
}

void CDbgMemAlloc::DumpStatsFileBase( char const *pchFileBase )
{
	HEAP_LOCK();

	char szFileName[MAX_PATH];
	static int s_FileCount = 0;
	if (m_OutputFunc == DefaultHeapReportFunc)
	{
		char *pPath = "";
		if ( IsX360() )
		{
			pPath = "D:\\";
		}

#if defined( _MEMTEST ) && defined( _X360 )
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
		_snprintf( szFileName, sizeof( szFileName ), "%s%s_%2.2d%2.2d_%2.2d%2.2d%2.2d_%d.txt", pPath, s_szStatsMapName, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond, s_FileCount );
#else
		_snprintf( szFileName, sizeof( szFileName ), "%s%s%d.txt", pPath, pchFileBase, s_FileCount );
#endif
		szFileName[ ARRAYSIZE(szFileName) - 1 ] = 0;

		++s_FileCount;

		s_DbgFile = fopen(szFileName, "wt");
		if (!s_DbgFile)
			return;
	}

	m_OutputFunc("Allocation type\tCurrent Size(k)\tPeak Size(k)\tTotal Allocations(k)\tOverhead Size(k)\tPeak Overhead Size(k)\tTime(ms)\tCurrent Count\tPeak Count\tTotal Count");

	for (int i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i)
	{
		m_OutputFunc( "\t%s", s_pCountHeader[i] );
	}

	m_OutputFunc("\n");

	DumpMemInfo( "Totals", 0, m_GlobalInfo );

#ifdef WIN32
	if ( IsX360() )
	{
		// add a line that has free memory
		size_t usedMemory, freeMemory;
		GlobalMemoryStatus( &usedMemory, &freeMemory );
		MemInfo_t info;
		// OS takes 32 MB, report our internal allocations only
		info.m_nCurrentSize = usedMemory;
		DumpMemInfo( "Used Memory", 0, info );
	}
#endif

	DumpFileStats();

	if (m_OutputFunc == DefaultHeapReportFunc)
	{
		fclose(s_DbgFile);

#if defined( _X360 ) && !defined( _RETAIL )
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

#else

	// no data
	*pFreeMemory = 0;
	*pUsedMemory = 0;

#endif
}

//-----------------------------------------------------------------------------
// Stat output
//-----------------------------------------------------------------------------
void CDbgMemAlloc::DumpStats()
{
	DumpStatsFileBase( "memstats" );
}

void CDbgMemAlloc::SetCRTAllocFailed( size_t nSize )
{
	m_sMemoryAllocFailed = nSize;

	MemAllocOOMError( nSize );
}

size_t CDbgMemAlloc::MemoryAllocFailed()
{
	return m_sMemoryAllocFailed;
}



#if defined( LINUX ) && !defined( NO_HOOK_MALLOC )
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
static void __attribute__((constructor)) override_init_hook(void)
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
void (*__MALLOC_HOOK_VOLATILE __malloc_initialize_hook)(void) __attribute__((visibility("default")))= override_init_hook;

#endif // LINUX


#if defined( OSX ) && !defined( NO_HOOK_MALLOC )
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


static inline void unprotect_malloc_zone( malloc_zone_t *malloc_zone )
{
	// Starting in OS X 10.7 the default zone defaults to read-only, version 8.
	// The version check may not be necessary, but we know it was RW before that.
	if ( malloc_zone->version >= 8 )
	{
		vm_protect( mach_task_self(), (uintptr_t)malloc_zone, sizeof( malloc_zone_t ), 0, VM_PROT_READ | VM_PROT_WRITE );
	}
}

static inline void protect_malloc_zone( malloc_zone_t *malloc_zone )
{
	if ( malloc_zone->version >= 8 )
	{
		vm_protect( mach_task_self(), (uintptr_t)malloc_zone, sizeof( malloc_zone_t ), 0, VM_PROT_READ );
	}
}

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

	unprotect_malloc_zone( malloc_zone );
	malloc_zone->malloc = (void* (*)(_malloc_zone_t*, size_t))osx_malloc_hook;
	malloc_zone->realloc = (void* (*)(_malloc_zone_t*, void*, size_t))osx_realloc_hook;
	malloc_zone->free = (void (*)(_malloc_zone_t*, void*))osx_free_hook;
	protect_malloc_zone( malloc_zone );

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
	AssertMsg( malloc_zone, "No malloc zone returned by malloc_default_zone" );

	unprotect_malloc_zone( malloc_zone );
	malloc_zone->malloc = override_malloc_hook;
	malloc_zone->realloc = override_realloc_hook;
	malloc_zone->free = override_free_hook;
	protect_malloc_zone( malloc_zone );

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


#endif // defined( OSX ) && !defined( NO_HOOK_MALLOC )


#endif // (defined(_DEBUG) || defined(USE_MEM_DEBUG))

#endif // !STEAM && !NO_MALLOC_OVERRIDE
