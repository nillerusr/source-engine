//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"

#ifndef STEAM

#ifdef TIER0_VALIDATE_HEAP

#include <malloc.h>
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "mem_helpers.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


extern IMemAlloc *g_pActualAlloc;

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
class CValidateAlloc : public IMemAlloc
{
public:
	enum
	{
		HEAP_PREFIX_BUFFER_SIZE = 12,
		HEAP_SUFFIX_BUFFER_SIZE = 8,
	};

	CValidateAlloc();

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
	void* CrtSetReportFile( int nRptType, void* hFile );
	void* CrtSetReportHook( void* pfnNewHook );
	int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg );
	virtual int heapchk();

	virtual void DumpStats() {}
	virtual void DumpStatsFileBase( char const *pchFileBase ) {}

	virtual bool IsDebugHeap()
	{
		return true;
	}

	virtual int GetVersion() { return MEMALLOC_VERSION; }

	virtual void CompactHeap(); 
	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler );

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

private:
	struct HeapPrefix_t
	{
		HeapPrefix_t *m_pPrev;
		HeapPrefix_t *m_pNext;
		int m_nSize;
		unsigned char m_Prefix[HEAP_PREFIX_BUFFER_SIZE];
	};

	struct HeapSuffix_t
	{
		unsigned char m_Suffix[HEAP_SUFFIX_BUFFER_SIZE];
	};

private:
	// Returns the actual debug info
	void GetActualDbgInfo( const char *&pFileName, int &nLine );

	// Updates stats
	void RegisterAllocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime );
	void RegisterDeallocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime );

	HeapSuffix_t *Suffix( HeapPrefix_t *pPrefix );
	void *AllocationStart( HeapPrefix_t *pBase );
	HeapPrefix_t *PrefixFromAllocation( void *pAlloc );
	const HeapPrefix_t *PrefixFromAllocation( const void *pAlloc );

	// Add to the list!
	void AddToList( HeapPrefix_t *pHeap, int nSize );

	// Remove from the list!
	void RemoveFromList( HeapPrefix_t *pHeap );

	// Validate the allocation
	bool ValidateAllocation( HeapPrefix_t *pHeap );

private:
	HeapPrefix_t *m_pFirstAllocation;
	char m_pPrefixImage[HEAP_PREFIX_BUFFER_SIZE];
	char m_pSuffixImage[HEAP_SUFFIX_BUFFER_SIZE];
};


//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
static CValidateAlloc s_ValidateAlloc;

#ifdef _PS3

IMemAlloc *g_pMemAllocInternalPS3 = &s_ValidateAlloc;

#else // !_PS3

IMemAlloc *g_pMemAlloc = &s_ValidateAlloc;

#endif // _PS3


//-----------------------------------------------------------------------------
// Constructor.
//-----------------------------------------------------------------------------
CValidateAlloc::CValidateAlloc()
{
	m_pFirstAllocation = 0;
	memset( m_pPrefixImage, 0xBE, HEAP_PREFIX_BUFFER_SIZE );
	memset( m_pSuffixImage, 0xAF, HEAP_SUFFIX_BUFFER_SIZE );
}


//-----------------------------------------------------------------------------
// Accessors...
//-----------------------------------------------------------------------------
inline CValidateAlloc::HeapSuffix_t *CValidateAlloc::Suffix( HeapPrefix_t *pPrefix )
{
	return reinterpret_cast<HeapSuffix_t *>( (unsigned char*)( pPrefix + 1 ) + pPrefix->m_nSize );
}

inline void *CValidateAlloc::AllocationStart( HeapPrefix_t *pBase )
{
	return static_cast<void *>( pBase + 1 );
}

inline CValidateAlloc::HeapPrefix_t *CValidateAlloc::PrefixFromAllocation( void *pAlloc )
{
	if ( !pAlloc )
		return NULL;

	return ((HeapPrefix_t*)pAlloc) - 1;
}

inline const CValidateAlloc::HeapPrefix_t *CValidateAlloc::PrefixFromAllocation( const void *pAlloc )
{
	return ((const HeapPrefix_t*)pAlloc) - 1;
}


//-----------------------------------------------------------------------------
// Add to the list!
//-----------------------------------------------------------------------------
void CValidateAlloc::AddToList( HeapPrefix_t *pHeap, int nSize )
{
	pHeap->m_pPrev = NULL;
	pHeap->m_pNext = m_pFirstAllocation;
	if ( m_pFirstAllocation )
	{
		m_pFirstAllocation->m_pPrev = pHeap;
	}
	pHeap->m_nSize = nSize;

	m_pFirstAllocation = pHeap;

	HeapSuffix_t *pSuffix = Suffix( pHeap );
	memcpy( pHeap->m_Prefix, m_pPrefixImage, HEAP_PREFIX_BUFFER_SIZE );
	memcpy( pSuffix->m_Suffix, m_pSuffixImage, HEAP_SUFFIX_BUFFER_SIZE );
}


//-----------------------------------------------------------------------------
// Remove from the list!
//-----------------------------------------------------------------------------
void CValidateAlloc::RemoveFromList( HeapPrefix_t *pHeap )
{
	if ( !pHeap )
		return;

	ValidateAllocation( pHeap );
	if ( pHeap->m_pPrev )
	{
		pHeap->m_pPrev->m_pNext = pHeap->m_pNext;
	}
	else
	{
		m_pFirstAllocation = pHeap->m_pNext;
	}

	if ( pHeap->m_pNext )
	{
		pHeap->m_pNext->m_pPrev = pHeap->m_pPrev;
	}
}


//-----------------------------------------------------------------------------
// Validate the allocation
//-----------------------------------------------------------------------------
bool CValidateAlloc::ValidateAllocation( HeapPrefix_t *pHeap )
{
	HeapSuffix_t *pSuffix = Suffix( pHeap );

	bool bOk = true;
	if ( memcmp( pHeap->m_Prefix, m_pPrefixImage, HEAP_PREFIX_BUFFER_SIZE ) )
	{
		bOk = false;
	}

	if ( memcmp( pSuffix->m_Suffix, m_pSuffixImage, HEAP_SUFFIX_BUFFER_SIZE ) )
	{
		bOk = false;
	}

	if ( !bOk )
	{
		Warning("Memory trash detected in allocation %X!\n", (void*)(pHeap+1) );
		Assert( 0 );
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------
void *CValidateAlloc::Alloc( size_t nSize )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	int nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
	HeapPrefix_t *pHeap = (HeapPrefix_t*)g_pActualAlloc->Alloc( nActualSize );
	AddToList( pHeap, nSize );
	return AllocationStart( pHeap );
}

void *CValidateAlloc::Realloc( void *pMem, size_t nSize )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	RemoveFromList( pHeap );

	int nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
	pHeap = (HeapPrefix_t*)g_pActualAlloc->Realloc( pHeap, nActualSize );
	AddToList( pHeap, nSize );

	return AllocationStart( pHeap );
}

void CValidateAlloc::Free( void *pMem )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	RemoveFromList( pHeap );

	g_pActualAlloc->Free( pHeap );
}

void *CValidateAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	RemoveFromList( pHeap );

	int nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
	pHeap = (HeapPrefix_t*)g_pActualAlloc->Expand_NoLongerSupported( pHeap, nActualSize );
	AddToList( pHeap, nSize );

	return AllocationStart( pHeap );
}


//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CValidateAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	int nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
	HeapPrefix_t *pHeap = (HeapPrefix_t*)g_pActualAlloc->Alloc( nActualSize, pFileName, nLine );
	AddToList( pHeap, nSize );
	return AllocationStart( pHeap );
}

void *CValidateAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	RemoveFromList( pHeap );

	int nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
	pHeap = (HeapPrefix_t*)g_pActualAlloc->Realloc( pHeap, nActualSize, pFileName, nLine );
	AddToList( pHeap, nSize );

	return AllocationStart( pHeap );
}

void  CValidateAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	RemoveFromList( pHeap );

	g_pActualAlloc->Free( pHeap, pFileName, nLine );
}

void *CValidateAlloc::Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	Assert( heapchk() == _HEAPOK );
	Assert( CrtCheckMemory() );
	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	RemoveFromList( pHeap );

	int nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
	pHeap = (HeapPrefix_t*)g_pActualAlloc->Expand_NoLongerSupported( pHeap, nActualSize, pFileName, nLine );
	AddToList( pHeap, nSize );

	return AllocationStart( pHeap );
}


//-----------------------------------------------------------------------------
// Returns size of a particular allocation
//-----------------------------------------------------------------------------
size_t CValidateAlloc::GetSize( void *pMem )
{
	if ( !pMem )
		return CalcHeapUsed();

	HeapPrefix_t *pHeap	= PrefixFromAllocation( pMem );
	return pHeap->m_nSize;
}


//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CValidateAlloc::PushAllocDbgInfo( const char *pFileName, int nLine )
{
	g_pActualAlloc->PushAllocDbgInfo( pFileName, nLine );
}

void CValidateAlloc::PopAllocDbgInfo()
{
	g_pActualAlloc->PopAllocDbgInfo( );
}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
long CValidateAlloc::CrtSetBreakAlloc( long lNewBreakAlloc )
{
	return g_pActualAlloc->CrtSetBreakAlloc( lNewBreakAlloc );
}

int CValidateAlloc::CrtSetReportMode( int nReportType, int nReportMode )
{
	return g_pActualAlloc->CrtSetReportMode( nReportType, nReportMode );
}

int CValidateAlloc::CrtIsValidHeapPointer( const void *pMem )
{
	const HeapPrefix_t *pHeap = PrefixFromAllocation( pMem );
	return g_pActualAlloc->CrtIsValidHeapPointer( pHeap );
}

int CValidateAlloc::CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
	const HeapPrefix_t *pHeap = PrefixFromAllocation( pMem );
	return g_pActualAlloc->CrtIsValidPointer( pHeap, size, access );
}

int CValidateAlloc::CrtCheckMemory( void )
{
	return g_pActualAlloc->CrtCheckMemory( );
}

int CValidateAlloc::CrtSetDbgFlag( int nNewFlag )
{
	return g_pActualAlloc->CrtSetDbgFlag( nNewFlag );
}

void CValidateAlloc::CrtMemCheckpoint( _CrtMemState *pState )
{
	g_pActualAlloc->CrtMemCheckpoint( pState );
}

void* CValidateAlloc::CrtSetReportFile( int nRptType, void* hFile )
{
	return g_pActualAlloc->CrtSetReportFile( nRptType, hFile );
}

void* CValidateAlloc::CrtSetReportHook( void* pfnNewHook )
{
	return g_pActualAlloc->CrtSetReportHook( pfnNewHook );
}

int CValidateAlloc::CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg )
{
	return g_pActualAlloc->CrtDbgReport( nRptType, szFile, nLine, szModule, pMsg );
}

int CValidateAlloc::heapchk()
{
	bool bOk = true;

	// Validate the heap
	HeapPrefix_t *pHeap = m_pFirstAllocation;
	for( pHeap = m_pFirstAllocation; pHeap; pHeap = pHeap->m_pNext )
	{
		if ( !ValidateAllocation( pHeap ) )
		{
			bOk = false;
		}
	}

#ifdef _WIN32
	return bOk ? _HEAPOK : 0;
#elif POSIX
	return bOk;
#else
#error
#endif
}

// Returns the actual debug info
void CValidateAlloc::GetActualDbgInfo( const char *&pFileName, int &nLine )
{
	g_pActualAlloc->GetActualDbgInfo( pFileName, nLine );
}

// Updates stats
void CValidateAlloc::RegisterAllocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime )
{
	g_pActualAlloc->RegisterAllocation( pFileName, nLine, nLogicalSize, nActualSize, nTime );
}

void CValidateAlloc::RegisterDeallocation( const char *pFileName, int nLine, int nLogicalSize, int nActualSize, unsigned nTime )
{
	g_pActualAlloc->RegisterDeallocation( pFileName, nLine, nLogicalSize, nActualSize, nTime );
}

void CValidateAlloc::CompactHeap()
{
	g_pActualAlloc->CompactHeap();
}

MemAllocFailHandler_t CValidateAlloc::SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler )
{
	return g_pActualAlloc->SetAllocFailHandler( pfnMemAllocFailHandler );
}

#endif // TIER0_VALIDATE_HEAP

#endif // STEAM
