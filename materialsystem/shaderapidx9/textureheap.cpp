//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "tier1/mempool.h"
#include "tier1/convar.h"
#include "tier1/utlmap.h"
#include "shaderapidx8.h"
#include "texturedx8.h"
#include "textureheap.h"
#include "shaderapidx8_global.h"

#include "tier0/memdbgon.h"

#define USE_STANDARD_ALLOCATOR
#ifdef USE_STANDARD_ALLOCATOR
#define UseStandardAllocator() (true)
#elif !defined(_RETAIL)
bool g_bUseStandardAllocator = false;
bool UseStandardAllocator()
{
	static bool bReadCommandLine;
	if ( !bReadCommandLine )
	{
		bReadCommandLine = true;
		const char *pStr = Plat_GetCommandLine();
		if ( pStr )
		{
			char tempStr[512];
			Q_strncpy( tempStr, pStr, sizeof( tempStr ) - 1 );
			tempStr[ sizeof( tempStr ) - 1 ] = 0;
			_strlwr( tempStr );

			if ( strstr( tempStr, "-notextureheap" ) )
				g_bUseStandardAllocator = true;
		}
	}
	return g_bUseStandardAllocator;
}
#else
#define UseStandardAllocator() (false)
#endif

#if !defined( _RELEASE ) && !defined( _RETAIL )
#define StrongAssert( expr )	if ( (expr) ) ; else { DebuggerBreak(); }
#else
#define StrongAssert( expr )	((void)0)
#endif

//-----------------------------------------------------------------------------
// Get Texture HW base
//-----------------------------------------------------------------------------
void *GetD3DTextureBasePtr( IDirect3DBaseTexture* pTex )
{
	// assumes base and mips are contiguous
	return (void *)( (unsigned int)pTex->Format.BaseAddress << 12 );
}

class CD3DTextureAllocator
{
public:
	static void *Alloc( int bytes )
	{
		DWORD attributes = MAKE_XALLOC_ATTRIBUTES( 
								0, 
								false, 
								TRUE, 
								FALSE, 
								eXALLOCAllocatorId_D3D,
								XALLOC_PHYSICAL_ALIGNMENT_4K, 
								XALLOC_MEMPROTECT_WRITECOMBINE, 
								FALSE,
								XALLOC_MEMTYPE_PHYSICAL );
		m_nTotalAllocations++;
		m_nTotalSize += AlignValue( bytes, 4096 );
		return XMemAlloc( bytes, attributes );
	}

	static void Free( void *p )
	{
		DWORD attributes = MAKE_XALLOC_ATTRIBUTES( 
								0, 
								false, 
								TRUE, 
								FALSE, 
								eXALLOCAllocatorId_D3D,
								XALLOC_PHYSICAL_ALIGNMENT_4K, 
								XALLOC_MEMPROTECT_WRITECOMBINE, 
								FALSE,
								XALLOC_MEMTYPE_PHYSICAL );
		m_nTotalAllocations--;
		m_nTotalSize -= XMemSize( p, attributes );
		XMemFree( p, attributes );
	}

	static int GetAllocations()
	{
		return m_nTotalAllocations;
	}

	static int GetSize()
	{
		return m_nTotalSize;
	}

	static int	m_nTotalSize;
	static int	m_nTotalAllocations;
};

int CD3DTextureAllocator::m_nTotalSize;
int CD3DTextureAllocator::m_nTotalAllocations;

enum TextureAllocator_t
{
	TA_DEFAULT,
	TA_MIXED,
	TA_UNKNOWN,
};

struct THBaseInfo
{
	TextureAllocator_t	m_fAllocator;
	int					m_TextureSize;	// stored for delayed allocations
};

struct THInfo_t : public THBaseInfo
{
	// Mixed heap info
	int nLogicalBytes;
	int nBytes;
	bool bFree:1;
	bool bNonTexture:1;

	THInfo_t *pPrev, *pNext;
};

struct THFreeBlock_t
{
	THInfo_t heapInfo;
	THFreeBlock_t *pPrevFree, *pNextFree;
};

class CXboxTexture : public IDirect3DTexture, public THInfo_t
{
public:
	CXboxTexture() 
	  : bImmobile(false)
	{
	}

	bool bImmobile;
	bool CanRelocate()	{ return ( !bImmobile && !IsBusy() ); }
};

class CXboxCubeTexture : public IDirect3DCubeTexture, public THBaseInfo
{
};

class CXboxVolumeTexture : public IDirect3DVolumeTexture, public THBaseInfo
{
};


void SetD3DTextureImmobile( IDirect3DBaseTexture *pTexture, bool bImmobile )
{
	if ( pTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		(( CXboxTexture *)pTexture)->bImmobile = bImmobile;
	}
}

CXboxTexture *GetTexture( THInfo_t *pInfo )
{
	if ( !pInfo->bFree && !pInfo->bNonTexture )
	{
		return (CXboxTexture *)((byte *)pInfo - offsetof( CXboxTexture, m_fAllocator ));
	}
	return NULL;
}

inline THFreeBlock_t *GetFreeBlock( THInfo_t *pInfo )
{
	if ( pInfo->bFree )
	{
		return (THFreeBlock_t *)((byte *)pInfo - offsetof( THFreeBlock_t, heapInfo ));
	}
	return NULL;
}

class CMixedTextureHeap
{
	enum
	{
		SIZE_ALIGNMENT = XBOX_HDD_SECTORSIZE,
		MIN_BLOCK_SIZE = 1024,
	};
public:

	CMixedTextureHeap() :
		m_nLogicalBytes( 0 ),
		m_nActualBytes( 0 ),
		m_nAllocs( 0 ),
		m_nOldBytes( 0 ),
		m_nNonTextureAllocs( 0 ),
		m_nBytesTotal( 0 ),
		m_pBase( NULL ),
		m_pFirstFree( NULL )
	{
	}

	void Init()
	{
		extern ConVar mat_texturecachesize;
		MEM_ALLOC_CREDIT_("CMixedTextureHeap");

		m_nBytesTotal = ( mat_texturecachesize.GetInt() * 1024 * 1024 );
#if 0
		m_nBytesTotal = AlignValue( m_nBytesTotal, SIZE_ALIGNMENT );
		m_pBase = CD3DTextureAllocator::Alloc( m_nBytesTotal );
#else
		m_nBytesTotal = AlignValue( m_nBytesTotal, 16*1024*1024 );
		m_pBase = XPhysicalAlloc( m_nBytesTotal, MAXULONG_PTR, 4096, PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_16MB_PAGES );
#endif
		m_pFirstFree = (THFreeBlock_t *)m_pBase;


		m_pFirstFree->heapInfo.bFree = true;
		m_pFirstFree->heapInfo.bNonTexture = false;
		m_pFirstFree->heapInfo.nBytes = m_nBytesTotal;
		m_pFirstFree->heapInfo.pNext = NULL;
		m_pFirstFree->heapInfo.pPrev = NULL;
		m_pFirstFree->pNextFree = NULL;
		m_pFirstFree->pPrevFree = NULL;

		m_pLastFree = m_pFirstFree;
	}

	void *Alloc( int bytes, THInfo_t *pInfo, bool bNonTexture = false )
	{
		pInfo->nBytes = AlignValue( bytes, SIZE_ALIGNMENT );

		if ( !m_pBase )
		{
			Init();
		}

		if ( bNonTexture && m_nNonTextureAllocs == 0 )
		{
			Compact();
		}

		void *p = FindBlock( pInfo );

		if ( !p )
		{
			p = ExpandToFindBlock( pInfo );
		}

		if ( p )
		{
			pInfo->nLogicalBytes = bytes;
			pInfo->bNonTexture = bNonTexture;
			m_nLogicalBytes += bytes;
			if ( !IsRetail() )
			{
				m_nOldBytes += AlignValue( bytes, 4096 );
			}
			m_nActualBytes += pInfo->nBytes;
			m_nAllocs++;

			if ( bNonTexture )
			{
				m_nNonTextureAllocs++;
			}
		}
		return p;
	}

	void Free( void *p, THInfo_t *pInfo )
	{
		if ( !p )
		{
			return;
		}

		if ( !IsRetail() )
		{
			m_nOldBytes -= AlignValue( pInfo->nLogicalBytes, 4096 );
		}

		if ( pInfo->bNonTexture )
		{
			m_nNonTextureAllocs--;
		}

		m_nLogicalBytes -= pInfo->nLogicalBytes;
		m_nAllocs--;
		m_nActualBytes -= pInfo->nBytes;

		THFreeBlock_t *pFree = (THFreeBlock_t *)p;
		pFree->heapInfo = *pInfo;
		pFree->heapInfo.bFree = true;

		AddToBlocksList( &pFree->heapInfo, pFree->heapInfo.pPrev, pFree->heapInfo.pNext );

		pFree = MergeLeft( pFree );
		pFree = MergeRight( pFree );

		AddToFreeList( pFree );

		if ( pInfo->bNonTexture && m_nNonTextureAllocs == 0 )
		{
			Compact();
		}
	}

	int Size( void *p, THInfo_t *pInfo )
	{
		return AlignValue( pInfo->nBytes, SIZE_ALIGNMENT );
	}

	bool IsOwner( void *p )
	{
		return ( m_pBase && p >= m_pBase && p < (byte *)m_pBase + m_nBytesTotal );
	}

	//-----------------------------------------------------

	void *FindBlock( THInfo_t *pInfo )
	{
		THFreeBlock_t *pCurrent = m_pFirstFree;

		int nBytesDesired = pInfo->nBytes;

		// Find the first block big enough to hold, then split it if appropriate
		while ( pCurrent && pCurrent->heapInfo.nBytes < nBytesDesired )
		{
			pCurrent = pCurrent->pNextFree;
		}

		if ( pCurrent )
		{
			return ClaimBlock( pCurrent, pInfo );
		}

		return NULL;
	}

	void AddToFreeList( THFreeBlock_t *pFreeBlock )
	{
		if ( !IsRetail() )
		{
			pFreeBlock->heapInfo.nLogicalBytes = 0;
		}

		if ( m_pFirstFree )
		{
			THFreeBlock_t *pPrev = NULL;
			THFreeBlock_t *pNext = m_pFirstFree;

			int nBytes = pFreeBlock->heapInfo.nBytes;

			while ( pNext && pNext->heapInfo.nBytes < nBytes )
			{
				pPrev = pNext;
				pNext = pNext->pNextFree;
			}

			pFreeBlock->pPrevFree = pPrev;
			pFreeBlock->pNextFree = pNext;

			if ( pPrev )
			{
				pPrev->pNextFree = pFreeBlock;
			}
			else
			{
				m_pFirstFree = pFreeBlock;
			}

			if ( pNext )
			{
				pNext->pPrevFree = pFreeBlock;
			}
			else
			{
				m_pLastFree = pFreeBlock;
			}
		}
		else
		{
			pFreeBlock->pPrevFree = pFreeBlock->pNextFree = NULL;
			m_pLastFree = m_pFirstFree = pFreeBlock;
		}
	}

	void RemoveFromFreeList( THFreeBlock_t *pFreeBlock )
	{
		if ( m_pFirstFree == pFreeBlock )
		{
			m_pFirstFree = m_pFirstFree->pNextFree;
		}
		else if ( pFreeBlock->pPrevFree )
		{
			pFreeBlock->pPrevFree->pNextFree = pFreeBlock->pNextFree;
		}

		if ( m_pLastFree == pFreeBlock )
		{
			m_pLastFree = pFreeBlock->pPrevFree;
		}
		else if ( pFreeBlock->pNextFree )
		{
			pFreeBlock->pNextFree->pPrevFree = pFreeBlock->pPrevFree;
		}

		pFreeBlock->pPrevFree = pFreeBlock->pNextFree = NULL;
	}

	THFreeBlock_t *GetLastFree()
	{
		return m_pLastFree;
	}

	void AddToBlocksList( THInfo_t *pBlock, THInfo_t *pPrev, THInfo_t *pNext )
	{
		if ( pPrev )
		{
			pPrev->pNext = pBlock;
		}

		if ( pNext)
		{
			pNext->pPrev = pBlock;
		}

		pBlock->pPrev = pPrev;
		pBlock->pNext = pNext;
	}

	void RemoveFromBlocksList( THInfo_t *pBlock )
	{
		if ( pBlock->pPrev )
		{
			pBlock->pPrev->pNext = pBlock->pNext;
		}

		if ( pBlock->pNext )
		{
			pBlock->pNext->pPrev = pBlock->pPrev;
		}
	}

	//-----------------------------------------------------

	void *ClaimBlock( THFreeBlock_t *pFreeBlock, THInfo_t *pInfo )
	{
		RemoveFromFreeList( pFreeBlock );

		int nBytesDesired = pInfo->nBytes;
		int nBytesRemainder = pFreeBlock->heapInfo.nBytes - nBytesDesired;
		*pInfo = pFreeBlock->heapInfo;
		pInfo->bFree = false;
		pInfo->bNonTexture = false;
		if ( nBytesRemainder >= MIN_BLOCK_SIZE )
		{
			pInfo->nBytes = nBytesDesired;

			THFreeBlock_t *pRemainder = (THFreeBlock_t *)(((byte *)(pFreeBlock)) + nBytesDesired);
			pRemainder->heapInfo.bFree = true;
			pRemainder->heapInfo.nBytes = nBytesRemainder;

			AddToBlocksList( &pRemainder->heapInfo, pInfo, pInfo->pNext );
			AddToFreeList( pRemainder );
		}
		AddToBlocksList( pInfo, pInfo->pPrev, pInfo->pNext );
		return pFreeBlock;
	}

	THFreeBlock_t *MergeLeft( THFreeBlock_t *pFree )
	{
		THInfo_t *pPrev = pFree->heapInfo.pPrev;
		if ( pPrev && pPrev->bFree )
		{
			pPrev->nBytes += pFree->heapInfo.nBytes;
			RemoveFromBlocksList( &pFree->heapInfo );
			pFree = GetFreeBlock( pPrev );
			RemoveFromFreeList( pFree );
		}
		return pFree;
	}

	THFreeBlock_t *MergeRight( THFreeBlock_t *pFree )
	{
		THInfo_t *pNext = pFree->heapInfo.pNext;
		if ( pNext && pNext->bFree )
		{
			pFree->heapInfo.nBytes += pNext->nBytes;
			RemoveFromBlocksList( pNext );
			RemoveFromFreeList( GetFreeBlock( pNext ) );
		}
		return pFree;
	}

	//-----------------------------------------------------

	bool GetExpansionList( THFreeBlock_t *pFreeBlock, THInfo_t **ppStart, THInfo_t **ppEnd, int depth = 1 )
	{
		THInfo_t *pStart;
		THInfo_t *pEnd;
		int i;

		pStart = &pFreeBlock->heapInfo;
		pEnd = &pFreeBlock->heapInfo;

		if ( m_nNonTextureAllocs > 0 )
		{
			return false;
		}

		// Walk backwards to start of expansion
		i = depth;
		while ( i > 0 && pStart->pPrev)
		{
			THInfo_t *pScan = pStart->pPrev;

			while ( i > 0 && pScan && !pScan->bFree && GetTexture( pScan )->CanRelocate() )
			{
				pScan = pScan->pPrev;
				i--;
			}

			if ( !pScan || !pScan->bFree )
			{
				break;
			}

			pStart = pScan;
		}

		// Walk forwards to start of expansion
		i = depth;
		while ( i > 0 && pEnd->pNext)
		{
			THInfo_t *pScan = pStart->pNext;

			while ( i > 0 && pScan && !pScan->bFree && GetTexture( pScan )->CanRelocate() )
			{
				pScan = pScan->pNext;
				i--;
			}

			if ( !pScan || !pScan->bFree )
			{
				break;
			}

			pEnd = pScan;
		}

		*ppStart = pStart;
		*ppEnd = pEnd;

		return ( pStart != pEnd );
	}

	THFreeBlock_t *CompactExpansionList( THInfo_t *pStart, THInfo_t *pEnd )
	{
// X360TBD:
Assert( 0 );
return NULL;
#if 0
#ifdef TH_PARANOID
		Validate();
#endif
		StrongAssert( pStart->bFree );
		StrongAssert( pEnd->bFree );
		byte *pNextBlock = (byte *)pStart;

		THInfo_t *pTextureBlock = pStart;
		THInfo_t *pLastBlock = pStart->pPrev;

		while ( pTextureBlock != pEnd )
		{
			CXboxTexture *pTexture = GetTexture( pTextureBlock );
			// If it's a texture, move it and thread it on. Otherwise, discard it
			if ( pTexture )
			{
				void *pTextureBits = GetD3DTextureBasePtr( pTexture );
				int nBytes = pTextureBlock->nBytes;

				if ( pNextBlock + nBytes <= pTextureBits)
				{
					memcpy( pNextBlock, pTextureBits, nBytes );
				}
				else
				{
					memmove( pNextBlock, pTextureBits, nBytes );
				}

				pTexture->Data = 0;
				pTexture->Register( pNextBlock );

				pNextBlock += nBytes;
				if ( pLastBlock)
				{
					pLastBlock->pNext = pTextureBlock;
				}
				pTextureBlock->pPrev = pLastBlock;
				pLastBlock = pTextureBlock;
			}
			else
			{
				StrongAssert( pTextureBlock->bFree );
				RemoveFromFreeList( GetFreeBlock( pTextureBlock ) );
			}
			pTextureBlock = pTextureBlock->pNext;
		}

		RemoveFromFreeList( GetFreeBlock( pEnd ) );

		// Make a new block and fix up the block lists
		THFreeBlock_t *pFreeBlock = (THFreeBlock_t *)pNextBlock;
		pFreeBlock->heapInfo.pPrev = pLastBlock;
		pLastBlock->pNext = &pFreeBlock->heapInfo;
		pFreeBlock->heapInfo.pNext = pEnd->pNext;
		if ( pEnd->pNext )
		{
			pEnd->pNext->pPrev = &pFreeBlock->heapInfo;
		}
		pFreeBlock->heapInfo.bFree = true;
		pFreeBlock->heapInfo.nBytes = ( (byte *)pEnd - pNextBlock ) + pEnd->nBytes;
		
		AddToFreeList( pFreeBlock );

#ifdef TH_PARANOID
		Validate();
#endif
		return pFreeBlock;
#endif
	}

	THFreeBlock_t *ExpandBlock( THFreeBlock_t *pFreeBlock, int depth = 1 )
	{
		THInfo_t *pStart;
		THInfo_t *pEnd;

		if ( GetExpansionList( pFreeBlock, &pStart, &pEnd, depth ) )
		{
			return CompactExpansionList( pStart, pEnd );
		}

		return pFreeBlock;
	}

	THFreeBlock_t *ExpandBlockToFit( THFreeBlock_t *pFreeBlock, unsigned bytes )
	{
		if ( pFreeBlock )
		{
			THInfo_t *pStart;
			THInfo_t *pEnd;

			if ( GetExpansionList( pFreeBlock, &pStart, &pEnd, 2 ) )
			{
				unsigned sum = 0;
				THInfo_t *pCurrent = pStart;
				while( pCurrent != pEnd->pNext ) 
				{
					if ( pCurrent->bFree )
					{
						sum += pCurrent->nBytes;
					}
					pCurrent = pCurrent->pNext;
				}

				if ( sum >= bytes )
				{
					pFreeBlock = CompactExpansionList( pStart, pEnd );
				}
			}
		}

		return pFreeBlock;
	}

	void *ExpandToFindBlock( THInfo_t *pInfo )
	{
		THFreeBlock_t *pFreeBlock = ExpandBlockToFit( GetLastFree(), pInfo->nBytes );
		if ( pFreeBlock && pFreeBlock->heapInfo.nBytes >= pInfo->nBytes )
		{
			return ClaimBlock( pFreeBlock, pInfo );
		}
		return NULL;
	}

	void Compact()
	{
		if ( m_nNonTextureAllocs > 0 )
		{
			return;
		}

		for (;;)
		{
			THFreeBlock_t *pCurrent = m_pFirstFree;
			THFreeBlock_t *pNew;
			while ( pCurrent )
			{
				int nBytesOld = pCurrent->heapInfo.nBytes;
				pNew = ExpandBlock( pCurrent, 999999 );

				if ( pNew != pCurrent || pNew->heapInfo.nBytes != nBytesOld )
				{
#ifdef TH_PARANOID
					Validate();
#endif
					break;
				}

#ifdef TH_PARANOID
				pNew = ExpandBlock( pCurrent, 999999 );
				StrongAssert( pNew == pCurrent && pNew->heapInfo.nBytes == nBytesOld );
#endif

				pCurrent = pCurrent->pNextFree;
			}

			if ( !pCurrent )
			{
				break;
			}
		}
	}

	void Validate()
	{
		if ( !m_pFirstFree )
		{
			return;
		}

		if ( m_nNonTextureAllocs > 0 )
		{
			return;
		}

		THInfo_t *pLast = NULL;
		THInfo_t *pInfo = &m_pFirstFree->heapInfo;

		while ( pInfo->pPrev )
		{
			pInfo = pInfo->pPrev;
		}

		void *pNextExpectedAddress = m_pBase;

		while ( pInfo )
		{
			byte *pCurrentAddress = (byte *)(( pInfo->bFree ) ? GetFreeBlock( pInfo ) : GetD3DTextureBasePtr( GetTexture( pInfo ) ) );
			StrongAssert( pCurrentAddress == pNextExpectedAddress );
			StrongAssert( pInfo->pPrev == pLast );
			pNextExpectedAddress = pCurrentAddress + pInfo->nBytes;
			pLast = pInfo;
			pInfo = pInfo->pNext;
		}

		THFreeBlock_t *pFree = m_pFirstFree;
		THFreeBlock_t *pLastFree = NULL;
		int nBytesHeap = XPhysicalSize( m_pBase );

		while ( pFree )
		{
			StrongAssert( pFree->pPrevFree == pLastFree );
			StrongAssert( (void *)pFree >= m_pBase && (void *)pFree < (byte *)m_pBase + nBytesHeap );
			StrongAssert( !pFree->pPrevFree || ( (void *)pFree->pPrevFree >= m_pBase && (void *)pFree->pPrevFree < (byte *)m_pBase + nBytesHeap ) );
			StrongAssert( !pFree->pNextFree || ( (void *)pFree->pNextFree >= m_pBase && (void *)pFree->pNextFree < (byte *)m_pBase + nBytesHeap ) );
			StrongAssert( !pFree->pPrevFree || pFree->pPrevFree->heapInfo.nBytes <= pFree->heapInfo.nBytes );
			pLastFree = pFree;
			pFree = pFree->pNextFree;
		}
	}

	//-----------------------------------------------------

	THFreeBlock_t *m_pFirstFree;
	THFreeBlock_t *m_pLastFree;
	void *m_pBase;

	int m_nLogicalBytes;
	int m_nActualBytes;
	int m_nAllocs;
	int m_nOldBytes;
	int m_nNonTextureAllocs;
	int m_nBytesTotal;
};

//-----------------------------------------------------------------------------

inline TextureAllocator_t GetTextureAllocator( IDirect3DBaseTexture9 *pTexture )
{
	return ( pTexture->GetType() == D3DRTYPE_CUBETEXTURE ) ? (( CXboxCubeTexture *)pTexture)->m_fAllocator : (( CXboxTexture *)pTexture)->m_fAllocator;
}

//-----------------------------------------------------------------------------

CMixedTextureHeap g_MixedTextureHeap;

CON_COMMAND( mat_texture_heap_stats, "" )
{
	if ( UseStandardAllocator() )
	{
		Msg( "Texture heap stats: (Standard Allocator)\n" );
		Msg( "Allocations:%d Size:%d\n", CD3DTextureAllocator::GetAllocations(), CD3DTextureAllocator::GetSize() );
	}
	else
	{
		Msg( "Texture heap stats:\n" );
		Msg( "  Mixed textures:    %dk/%dk allocated in %d textures\n", g_MixedTextureHeap.m_nLogicalBytes/1024, g_MixedTextureHeap.m_nActualBytes/1024, g_MixedTextureHeap.m_nAllocs );
		float oldFootprint = g_MixedTextureHeap.m_nOldBytes;
		float newFootprint = g_MixedTextureHeap.m_nActualBytes;
		Msg( "\n  Old: %.3fmb, New: %.3fmb\n", oldFootprint / (1024.0*1024.0), newFootprint / (1024.0*1024.0) );
	}
}

CON_COMMAND( mat_texture_heap_compact, "" )
{
	Msg( "Validating texture heap...\n" );
	g_MixedTextureHeap.Validate();
	Msg( "Compacting texture heap...\n" );
	unsigned oldLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;
	g_MixedTextureHeap.Compact();
	unsigned newLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;

	Msg( "\n  Old largest block: %.3fk, New largest block: %.3fk\n\n", oldLargest / 1024.0, newLargest / 1024.0 );

	Msg( "Validating texture heap...\n" );
	g_MixedTextureHeap.Validate();
	Msg( "Done.\n" );
}

//-----------------------------------------------------------------------------
// Nasty back doors
//-----------------------------------------------------------------------------

void CompactTextureHeap()
{
	unsigned oldLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;
	g_MixedTextureHeap.Compact();
	unsigned newLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;

	DevMsg( "Compacted texture heap. Old largest block: %.3fk, New largest block: %.3fk\n", oldLargest / 1024.0, newLargest / 1024.0 );
}

CTextureHeap g_TextureHeap;

//-----------------------------------------------------------------------------
// Build and alloc a texture resource
//-----------------------------------------------------------------------------
IDirect3DTexture *CTextureHeap::AllocTexture( int width, int height, int levels, DWORD usage, D3DFORMAT d3dFormat, bool bFallback, bool bNoD3DMemory )
{
	CXboxTexture* pD3DTexture = new CXboxTexture;

	// create a texture with contiguous mips and packed tails
	DWORD dwTextureSize = XGSetTextureHeaderEx( 
		width, 
		height, 
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0, 
		XGHEADER_CONTIGUOUS_MIP_OFFSET, 
		0, 
		pD3DTexture, 
		NULL, 
		NULL );

	// based on "Xbox 360 Texture Storage"
	// can truncate the terminal tile using packed tails
	// the terminal tile must be at 32x32 or 16x16 packed
	if ( width == height && levels != 0 )
	{
		int terminalWidth = width >> (levels - 1);
		if ( d3dFormat == D3DFMT_DXT1 )
		{
			if ( terminalWidth <= 32 ) 
			{
				dwTextureSize -= 4*1024;
			}
		}
		else if ( d3dFormat == D3DFMT_DXT5 ) 
		{
			if ( terminalWidth == 32 )
			{
				dwTextureSize -= 8*1024;
			}
			else if ( terminalWidth <= 16 )
			{
				dwTextureSize -= 12*1024;
			}
		}
	}

	pD3DTexture->m_TextureSize = dwTextureSize;

	if ( !bFallback && bNoD3DMemory )
	{
		pD3DTexture->m_fAllocator = TA_UNKNOWN;
		return pD3DTexture;
	}

	void *pBuffer;
	if ( UseStandardAllocator() )
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Standard D3D" );
		pBuffer = CD3DTextureAllocator::Alloc( dwTextureSize );
		pD3DTexture->m_fAllocator = TA_DEFAULT;
	}
	else
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Mixed texture" );
		pBuffer = g_MixedTextureHeap.Alloc( dwTextureSize, pD3DTexture );
		if ( pBuffer )
		{
			pD3DTexture->m_fAllocator = TA_MIXED;
		}
		else
		{
			g_MixedTextureHeap.Compact();
			pBuffer = g_MixedTextureHeap.Alloc( dwTextureSize, pD3DTexture );
			if ( pBuffer )
			{
				pD3DTexture->m_fAllocator = TA_MIXED;
			}
			else
			{
				pBuffer = CD3DTextureAllocator::Alloc( dwTextureSize );
				pD3DTexture->m_fAllocator = TA_DEFAULT;
			}
		}
	}

	if ( !pBuffer )
	{
		delete pD3DTexture;
		return NULL;
	}

	XGOffsetResourceAddress( pD3DTexture, pBuffer );

	return pD3DTexture;
}

//-----------------------------------------------------------------------------
// Build and alloc a cube texture resource
//-----------------------------------------------------------------------------
IDirect3DCubeTexture *CTextureHeap::AllocCubeTexture( int width, int levels, DWORD usage, D3DFORMAT d3dFormat, bool bFallback, bool bNoD3DMemory )
{
	CXboxCubeTexture* pD3DCubeTexture = new CXboxCubeTexture;

	// create a cube texture with contiguous mips and packed tails
	DWORD dwTextureSize = XGSetCubeTextureHeaderEx( 
		width,  
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0,
		XGHEADER_CONTIGUOUS_MIP_OFFSET, 
		pD3DCubeTexture, 
		NULL, 
		NULL );
	pD3DCubeTexture->m_TextureSize = dwTextureSize;

	if ( !bFallback && bNoD3DMemory )
	{
		pD3DCubeTexture->m_fAllocator = TA_UNKNOWN;
		return pD3DCubeTexture;
	}

	void *pBits;
	if ( UseStandardAllocator() )
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Cubemap standard D3D" );
		pBits = CD3DTextureAllocator::Alloc( dwTextureSize );
		pD3DCubeTexture->m_fAllocator = TA_DEFAULT;
	}
	else
	{
		// @todo: switch to texture heap
		MEM_ALLOC_CREDIT_( __FILE__ ": Odd sized cubemap textures" );
		// Really only happens with environment map
		pBits = CD3DTextureAllocator::Alloc( dwTextureSize );
		pD3DCubeTexture->m_fAllocator = TA_DEFAULT;
	}

	if ( !pBits )
	{
		delete pD3DCubeTexture;
		return NULL;
	}

	XGOffsetResourceAddress( pD3DCubeTexture, pBits );

	return pD3DCubeTexture;
}

//-----------------------------------------------------------------------------
// Allocate an Volume Texture
//-----------------------------------------------------------------------------
IDirect3DVolumeTexture *CTextureHeap::AllocVolumeTexture( int width, int height, int depth, int levels, DWORD usage, D3DFORMAT d3dFormat )
{
	CXboxVolumeTexture *pD3DVolumeTexture = new CXboxVolumeTexture;

	// create a cube texture with contiguous mips and packed tails
	DWORD dwTextureSize = XGSetVolumeTextureHeaderEx( 
		width,
		height, 
		depth,
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0,
		XGHEADER_CONTIGUOUS_MIP_OFFSET, 
		pD3DVolumeTexture, 
		NULL, 
		NULL );

	void *pBits;
	
	MEM_ALLOC_CREDIT_( __FILE__ ": Volume standard D3D" );

	pBits = CD3DTextureAllocator::Alloc( dwTextureSize );
	pD3DVolumeTexture->m_fAllocator = TA_DEFAULT;
	pD3DVolumeTexture->m_TextureSize = dwTextureSize;

	if ( !pBits )
	{
		delete pD3DVolumeTexture;
		return NULL;
	}

	XGOffsetResourceAddress( pD3DVolumeTexture, pBits );

	return pD3DVolumeTexture; 
}

//-----------------------------------------------------------------------------
// Get current backbuffer multisample type (used in AllocRenderTargetSurface() )
//-----------------------------------------------------------------------------
D3DMULTISAMPLE_TYPE CTextureHeap::GetBackBufferMultiSampleType()
{
	int backWidth, backHeight;
	ShaderAPI()->GetBackBufferDimensions( backWidth, backHeight );

	// 2xMSAA at 640x480 and 848x480 are the only supported multisample mode on 360 (2xMSAA for 720p would
	// use predicated tiling, which would require a rewrite of *all* our render target code)
	// FIXME: shuffle the EDRAM surfaces to allow 4xMSAA for standard def
	//        (they would overlap & trash each other with the current allocation scheme)
	D3DMULTISAMPLE_TYPE backBufferMultiSampleType = g_pShaderDevice->IsAAEnabled() ? D3DMULTISAMPLE_2_SAMPLES : D3DMULTISAMPLE_NONE;
	Assert( ( g_pShaderDevice->IsAAEnabled() == false ) || (backHeight == 480) );

	return backBufferMultiSampleType;
}

//-----------------------------------------------------------------------------
// Allocate an EDRAM surface
//-----------------------------------------------------------------------------
IDirect3DSurface *CTextureHeap::AllocRenderTargetSurface( int width, int height, D3DFORMAT d3dFormat, bool bMultiSample, int base )
{	
	// render target surfaces don't need to exist simultaneously
	// force their allocations to overlap at the end of back buffer and zbuffer
	// this should leave 3MB (of 10) free assuming 1280x720 (and 5MB with 640x480@2xMSAA)
	D3DMULTISAMPLE_TYPE backBufferMultiSampleType = GetBackBufferMultiSampleType();
	D3DMULTISAMPLE_TYPE multiSampleType = bMultiSample ? backBufferMultiSampleType : D3DMULTISAMPLE_NONE;
	if ( base < 0 )
	{
		int backWidth, backHeight;
		ShaderAPI()->GetBackBufferDimensions( backWidth, backHeight );
		D3DFORMAT backBufferFormat = ImageLoader::ImageFormatToD3DFormat( g_pShaderDevice->GetBackBufferFormat() );
		base = 2*XGSurfaceSize( backWidth, backHeight, backBufferFormat, backBufferMultiSampleType );
	}

	D3DSURFACE_PARAMETERS surfParameters;
	surfParameters.Base = base;
	surfParameters.ColorExpBias = 0;

	if ( ( d3dFormat == D3DFMT_D24FS8 ) || ( d3dFormat == D3DFMT_D24S8 ) || ( d3dFormat == D3DFMT_D16 ) )
	{
		surfParameters.HierarchicalZBase = 0;
		if ( ( surfParameters.HierarchicalZBase + XGHierarchicalZSize( width, height, multiSampleType ) ) > GPU_HIERARCHICAL_Z_TILES )
		{
			// overflow, can't hold the tiles so disable
			surfParameters.HierarchicalZBase = 0xFFFFFFFF; 
		}
	}
	else
	{
		// not using
		surfParameters.HierarchicalZBase = 0xFFFFFFFF;
	}	

	HRESULT hr;
	IDirect3DSurface9 *pSurface = NULL;
	hr = Dx9Device()->CreateRenderTarget( width, height, d3dFormat, multiSampleType, 0, FALSE, &pSurface, &surfParameters );
	Assert( !FAILED( hr ) );

	return pSurface;
}

//-----------------------------------------------------------------------------
// Perform the real d3d allocation, returns true if succesful, false otherwise.
// Only valid for a texture created with no d3d bits, otherwise no-op.
//-----------------------------------------------------------------------------
bool CTextureHeap::AllocD3DMemory( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return false;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_SURFACE )
	{
		// there are no d3d bits for a surface
		return false;
	}

	void *pBits = GetD3DTextureBasePtr( pD3DTexture );
	if ( pBits )
	{
		// already have d3d bits
		return true;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Standard D3D" );
		pBits = CD3DTextureAllocator::Alloc( ((CXboxTexture *)pD3DTexture)->m_TextureSize );
		((CXboxTexture *)pD3DTexture)->m_fAllocator = TA_DEFAULT;
		XGOffsetResourceAddress( (CXboxTexture *)pD3DTexture, pBits );
		return true;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Cubemap standard D3D" );
		pBits = CD3DTextureAllocator::Alloc( ((CXboxCubeTexture *)pD3DTexture)->m_TextureSize );
		((CXboxCubeTexture *)pD3DTexture)->m_fAllocator = TA_DEFAULT;
		XGOffsetResourceAddress( (CXboxCubeTexture *)pD3DTexture, pBits );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Release the allocated store
//-----------------------------------------------------------------------------
void CTextureHeap::FreeTexture( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_SURFACE )
	{
		// texture heap doesn't own render target surfaces
		// allow callers to call through for less higher level detection
		int ref = ((IDirect3DSurface*)pD3DTexture)->Release();
		Assert( ref == 0 );
		ref = ref; // Quiet "unused variable" warning in release
		return;
	}
	else
	{
		byte *pBits = (byte *)GetD3DTextureBasePtr( pD3DTexture );
		if ( pBits )
		{
			switch ( GetTextureAllocator( pD3DTexture ) )
			{
			case TA_DEFAULT:
				CD3DTextureAllocator::Free( pBits );
				break;

			case TA_MIXED:
				g_MixedTextureHeap.Free( pBits, ((CXboxTexture *)pD3DTexture) );
				break;
			}
		}
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		delete (CXboxTexture *)pD3DTexture;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_VOLUMETEXTURE )
	{
		delete (CXboxVolumeTexture *)pD3DTexture;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		delete (CXboxCubeTexture *)pD3DTexture;
	}
}

//-----------------------------------------------------------------------------
// Returns the allocated footprint
//-----------------------------------------------------------------------------
int	CTextureHeap::GetSize( IDirect3DBaseTexture *pD3DTexture )
{
	if( pD3DTexture == NULL )
		return 0;

	if ( pD3DTexture->GetType() == D3DRTYPE_SURFACE )
	{
		D3DSURFACE_DESC surfaceDesc;
		HRESULT hr = ((IDirect3DSurface*)pD3DTexture)->GetDesc( &surfaceDesc );
		Assert( !FAILED( hr ) );
		hr = hr; // Quiet "unused variable" warning in release

		int size = ImageLoader::GetMemRequired( 
			surfaceDesc.Width,
			surfaceDesc.Height,
			0,
			ImageLoader::D3DFormatToImageFormat( surfaceDesc.Format ),
			false );

		return size;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		return ((CXboxTexture *)pD3DTexture)->m_TextureSize;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		return ((CXboxCubeTexture *)pD3DTexture)->m_TextureSize;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_VOLUMETEXTURE )
	{
		return ((CXboxVolumeTexture *)pD3DTexture)->m_TextureSize;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Crunch the pools
//-----------------------------------------------------------------------------
void CTextureHeap::Compact()
{
	g_MixedTextureHeap.Compact();
}
