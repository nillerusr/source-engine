//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.
//
// $NoKeywords: $
//===========================================================================//

#include "LoadScreenUpdate.h"
#include "tier0/memalloc.h"
#include "tier1/delegates.h"
#include "tier0/threadtools.h"
#include "tier2/tier2.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/dbg.h"

#ifdef _X360

#define LOADING_UPDATE_INTERVAL 0.015f
#define UNINITIALIZED_LAST_TIME -1000.0f

//-----------------------------------------------------------------------------
// Used to tick the loading screen every so often
//-----------------------------------------------------------------------------
class CLoaderMemAlloc : public IMemAlloc
{
	// Methods of IMemAlloc
public:
	virtual void *Alloc( size_t nSize );
	virtual void *Realloc( void *pMem, size_t nSize );
	virtual void Free( void *pMem );
	DELEGATE_TO_OBJECT_2( void *,	Expand_NoLongerSupported, void *, size_t, m_pMemAlloc );
	virtual void *Alloc( size_t nSize, const char *pFileName, int nLine );
	virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine );
	virtual void  Free( void *pMem, const char *pFileName, int nLine );
	DELEGATE_TO_OBJECT_4( void*,	Expand_NoLongerSupported, void *, size_t, const char *, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( size_t,	GetSize, void *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			PushAllocDbgInfo, const char *, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			PopAllocDbgInfo, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( long,		CrtSetBreakAlloc, long, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2( int,		CrtSetReportMode, int, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( int,		CrtIsValidHeapPointer, const void *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_3( int,		CrtIsValidPointer, const void *, unsigned int, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( int,		CrtCheckMemory, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( int,		CrtSetDbgFlag, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1V(			CrtMemCheckpoint, _CrtMemState *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			DumpStats, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1V(			DumpStatsFileBase, const char *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2( void*,	CrtSetReportFile, int, void*, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( void*,	CrtSetReportHook, void*, m_pMemAlloc );
	DELEGATE_TO_OBJECT_5( int,		CrtDbgReport, int, const char *, int, const char *, const char *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( int,		heapchk, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( bool,		IsDebugHeap, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			GetActualDbgInfo, const char *&, int &, m_pMemAlloc );
	DELEGATE_TO_OBJECT_5V(			RegisterAllocation, const char *, int, int, int, unsigned, m_pMemAlloc );
	DELEGATE_TO_OBJECT_5V(			RegisterDeallocation, const char *, int, int, int, unsigned, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( int,		GetVersion, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			CompactHeap, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( MemAllocFailHandler_t, SetAllocFailHandler, MemAllocFailHandler_t, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1V(			DumpBlockStats, void *, m_pMemAlloc );
#if defined( _MEMTEST )	
	DELEGATE_TO_OBJECT_2V(			SetStatsExtraInfo, const char *, const char *, m_pMemAlloc );
#endif
	DELEGATE_TO_OBJECT_0(size_t,	MemoryAllocFailed, m_pMemAlloc );
	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	// Other public methods
public:
	CLoaderMemAlloc();
	void Start( MaterialNonInteractiveMode_t mode );
	void Stop();

	// Check if we need to call swap. Do so if necessary.
	void CheckSwap( );

private:
	IMemAlloc *m_pMemAlloc;
	float m_flLastUpdateTime;
	bool m_bInSwap;
};


//-----------------------------------------------------------------------------
// Activate, deactivate loadermemalloc
//-----------------------------------------------------------------------------
static CLoaderMemAlloc s_LoaderMemAlloc;

void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode )
{
	if ( IsX360() )
	{
		s_LoaderMemAlloc.Start( mode );
	}
}

void RefreshScreenIfNecessary()
{
	if ( IsX360() )
	{
		s_LoaderMemAlloc.CheckSwap();
	}
}

void EndLoadingUpdates()
{
	if ( IsX360() )
	{
		s_LoaderMemAlloc.Stop();
	}
}

static int LoadLibraryThreadFunc()
{
	RefreshScreenIfNecessary();
	return 15;
}


//-----------------------------------------------------------------------------
// Used to tick the loading screen every so often
//-----------------------------------------------------------------------------
CLoaderMemAlloc::CLoaderMemAlloc() 
{
	m_pMemAlloc = 0;
}

void CLoaderMemAlloc::Start( MaterialNonInteractiveMode_t mode )
{
	if ( m_pMemAlloc || ( mode == MATERIAL_NON_INTERACTIVE_MODE_NONE ) )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->EnableNonInteractiveMode( mode );

	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_STARTUP )
	{
		SetThreadedLoadLibraryFunc( LoadLibraryThreadFunc );
	}

	// NOTE: This is necessary to avoid a one-frame black flash
	// since Present is what copies the back buffer into the temp buffer
	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD )
	{
		extern void V_RenderVGuiOnly( void );
		V_RenderVGuiOnly();
	}

	m_flLastUpdateTime = UNINITIALIZED_LAST_TIME;
	m_bInSwap = false;
	m_pMemAlloc = g_pMemAlloc;
	g_pMemAlloc = this;
}

void CLoaderMemAlloc::Stop()
{
	if ( !m_pMemAlloc )
		return;

	g_pMemAlloc = m_pMemAlloc;
	m_pMemAlloc = 0;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->EnableNonInteractiveMode( MATERIAL_NON_INTERACTIVE_MODE_NONE );
	SetThreadedLoadLibraryFunc( NULL );
}


//-----------------------------------------------------------------------------
// Check if we need to call swap. Do so if necessary.
//-----------------------------------------------------------------------------
void CLoaderMemAlloc::CheckSwap( )
{
	if ( !m_pMemAlloc )
		return;

	float t = Plat_FloatTime();
	float dt = t - m_flLastUpdateTime;
	if ( dt >= LOADING_UPDATE_INTERVAL )
	{
		if ( ThreadInMainThread() && !m_bInSwap && !g_pMaterialSystem->IsInFrame() )
		{
			m_bInSwap = true;
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			pRenderContext->RefreshFrontBufferNonInteractive();
			m_bInSwap = false;

			// NOTE: It is necessary to re-read time, since Refresh
			// may block, and if it does, it'll force a refresh every allocation
			// if we don't resample time after the block
			m_flLastUpdateTime = Plat_FloatTime();
		}
	}
}


//-----------------------------------------------------------------------------
// Hook allocations, render when appropriate
//-----------------------------------------------------------------------------
void *CLoaderMemAlloc::Alloc( size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->Alloc( nSize );
}

void *CLoaderMemAlloc::Realloc( void *pMem, size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->Realloc( pMem, nSize );
}

void CLoaderMemAlloc::Free( void *pMem )
{
	CheckSwap();
	m_pMemAlloc->Free( pMem );
}

void *CLoaderMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->Alloc( nSize, pFileName, nLine );
}

void *CLoaderMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->Realloc( pMem, nSize, pFileName, nLine );
}

void  CLoaderMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	CheckSwap();
	m_pMemAlloc->Free( pMem, pFileName, nLine );
}

#endif // _X360
