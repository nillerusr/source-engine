//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//						ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks, and there will never be two
// contiguous free memblocks.
//
// The rover can be left pointing at a non-empty block
//
// The zone calls are pretty much only used for small strings and structures,
// all big things are allocated on the hunk.
//=============================================================================//

#include "basetypes.h"
#include "zone.h"
#include "host.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "memstack.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "tier0/memalloc.h"

#define MINIMUM_WIN_MEMORY			0x03000000	// FIXME: copy from sys_dll.cpp, find a common header at some point

#ifdef _X360
#define HUNK_USE_16MB_PAGE
#endif

CMemoryStack g_HunkMemoryStack;
#ifdef HUNK_USE_16MB_PAGE
CMemoryStack g_HunkOverflow;
static bool g_bWarnedOverflow;
#endif

static int GetTargetCacheSize()
{
	int nMemLimit = host_parms.memsize - Hunk_Size();
	if ( nMemLimit < 0x100000 )
	{
		nMemLimit = 0x100000;
	}
	return nMemLimit;
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName (int size, const char *name, bool bClear)
{
	MEM_ALLOC_CREDIT();
	void * p = g_HunkMemoryStack.Alloc( size, bClear );
	if ( p )
		return p;
#ifdef HUNK_USE_16MB_PAGE
	if ( !g_bWarnedOverflow )
	{
		g_bWarnedOverflow = true;
		DevMsg( "Note: Hunk base page exhausted\n" );
	}

	p = g_HunkOverflow.Alloc( size, bClear );
	if ( p )
		return p;
#endif
	Error( "Engine hunk overflow!\n" );
	return NULL;
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc(int size, bool bClear )
{
	MEM_ALLOC_CREDIT();
	return Hunk_AllocName( size, NULL, bClear );
}

int	Hunk_LowMark(void)
{
	return (int)( g_HunkMemoryStack.GetCurrentAllocPoint() );
}

void Hunk_FreeToLowMark(int mark)
{
	Assert( mark < g_HunkMemoryStack.GetSize() );
#ifdef HUNK_USE_16MB_PAGE
	g_HunkOverflow.FreeAll();
	g_bWarnedOverflow = false;
#endif
	g_HunkMemoryStack.FreeToAllocPoint( mark );
}

int Hunk_MallocSize()
{
#ifdef HUNK_USE_16MB_PAGE
	return g_HunkMemoryStack.GetSize() + g_HunkOverflow.GetSize();
#else
	return g_HunkMemoryStack.GetSize();
#endif
}

int Hunk_Size()
{
#ifdef HUNK_USE_16MB_PAGE
	return g_HunkMemoryStack.GetUsed() + g_HunkOverflow.GetUsed();
#else
	return g_HunkMemoryStack.GetUsed();
#endif
}

void Hunk_Print()
{
#ifdef HUNK_USE_16MB_PAGE
	Msg( "Total used memory:      %d (%d/%d)\n", Hunk_Size(), g_HunkMemoryStack.GetUsed(), g_HunkOverflow.GetUsed() );
	Msg( "Total committed memory: %d (%d/%d)\n", Hunk_MallocSize(), g_HunkMemoryStack.GetSize(), g_HunkOverflow.GetSize() );
#else
	Msg( "Total used memory:      %d\n", Hunk_Size() );
	Msg( "Total committed memory: %d\n", Hunk_MallocSize() );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Init( void )
{
	MEM_ALLOC_CREDIT();
	int nMaxBytes = 48*1024*1024;
	const int nMinCommitBytes = 0x8000;
#ifndef HUNK_USE_16MB_PAGE
	const int nInitialCommit = 0x280000;
	while ( !g_HunkMemoryStack.Init( nMaxBytes, nMinCommitBytes, nInitialCommit ) )	 
	{
		Warning( "Unable to allocate %d MB of memory, trying %d MB instead\n", nMaxBytes, nMaxBytes/2 );
		nMaxBytes /= 2;
		if ( nMaxBytes < MINIMUM_WIN_MEMORY )
		{
			Error( "Failed to allocate minimum memory requirement for game (%d MB)\n", MINIMUM_WIN_MEMORY/(1024*1024));
		}
	}
#else
	if ( !g_HunkMemoryStack.InitPhysical( 16*1024*1024 ) || !g_HunkOverflow.Init( nMaxBytes - 16*1024*1024, nMinCommitBytes ) )
	{
		Error( "Failed to allocate minimum memory requirement for game (%d MB)\n", nMaxBytes );
	}

#endif
	g_pDataCache->SetSize( GetTargetCacheSize() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Shutdown( void )
{
	g_HunkMemoryStack.FreeAll();

	// This disconnects the engine data cache
	g_pDataCache->SetSize( 0 );
}
