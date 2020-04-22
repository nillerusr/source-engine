//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/mem.h"
//#include <malloc.h>
#include "tier0/dbg.h"

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

//-----------------------------------------------------------------------------
// Other DLL-exported methods for particular kinds of memory
//-----------------------------------------------------------------------------
void *MemAllocScratch( int nMemSize )
{	
	// Minimally allocate 1M scratch
	if (s_nBufAllocSize < s_nBufCurSize + nMemSize)
	{
		s_nBufAllocSize = s_nBufCurSize + nMemSize;
		if (s_nBufAllocSize < 2 * 1024)
		{
			s_nBufAllocSize = 2 * 1024;
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
