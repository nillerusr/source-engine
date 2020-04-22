//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/mem.h"
#include <malloc.h>
#include "tier0/dbg.h"
#include "tier0/minidump.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#define PvExpand _expand
#endif

enum 
{
	MAX_STACK_DEPTH = 32
};

static uint8 *s_pBuf = NULL;
static int s_pBufStackDepth[MAX_STACK_DEPTH];
static int s_nBufDepth = -1;
static int s_nBufCurSize = 0;
static int s_nBufAllocSize = 0;
static bool s_oomerror_called = false;

void MemAllocOOMError( size_t nSize )
{
	if ( !s_oomerror_called )
	{
		s_oomerror_called = true;

		MinidumpUserStreamInfoAppend( "MemAllocOOMError: %u bytes\n", (uint)nSize );

		//$ TODO: Need a good error message here.
		// A basic advice to try lowering texture settings is just most-likely to help users who are exhausting address
		// space, but not necessarily the cause.  Ideally the engine wouldn't let you get here because of too-high settings.
		Error( "Out of memory or address space.  Texture quality setting may be too high.\n" );
	}
}

//-----------------------------------------------------------------------------
// Other DLL-exported methods for particular kinds of memory
//-----------------------------------------------------------------------------
void *MemAllocScratch( int nMemSize )
{	
	// Minimally allocate 1M scratch
	if (s_nBufAllocSize < s_nBufCurSize + nMemSize)
	{
		s_nBufAllocSize = s_nBufCurSize + nMemSize;
		if (s_nBufAllocSize < 1024 * 1024)
		{
			s_nBufAllocSize = 1024 * 1024;
		}

		if (s_pBuf)
		{
			s_pBuf = (uint8*)PvRealloc( s_pBuf, s_nBufAllocSize );
			Assert( s_pBuf );	
		}
		else
		{
			s_pBuf = (uint8*)PvAlloc( s_nBufAllocSize );
		}
	}

	int nBase = s_nBufCurSize;
	s_nBufCurSize += nMemSize;
	++s_nBufDepth;
	Assert( s_nBufDepth < MAX_STACK_DEPTH );
	s_pBufStackDepth[s_nBufDepth] = nMemSize;

	return &s_pBuf[nBase];
}

void MemFreeScratch()
{
	Assert( s_nBufDepth >= 0 );
	s_nBufCurSize -= s_pBufStackDepth[s_nBufDepth];
	--s_nBufDepth;
}

#ifdef POSIX
void ZeroMemory( void *mem, size_t length )
{
	memset( mem, 0x0, length );
}
#endif
