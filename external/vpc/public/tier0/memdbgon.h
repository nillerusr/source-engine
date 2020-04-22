//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: This header, which must be the final include in a .cpp (or .h) file,
// causes all crt methods to use debugging versions of the memory allocators.
// NOTE: Use memdbgoff.h to disable memory debugging.
//
// $NoKeywords: $
//=============================================================================//

// SPECIAL NOTE! This file must *not* use include guards; we need to be able
// to include this potentially multiple times (since we can deactivate debugging
// by including memdbgoff.h)

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

// SPECIAL NOTE #2: This must be the final include in a .cpp or .h file!!!

#if defined(_DEBUG) && !defined(USE_MEM_DEBUG) && !defined( _PS3 )
#define USE_MEM_DEBUG 1
#endif

// If debug build or ndebug and not already included MS custom alloc files, or already included this file
#if (defined(_DEBUG) || !defined(_INC_CRTDBG)) || defined(MEMDBGON_H)

#include "tier0/basetypes.h"

#include "tier0/valve_off.h"
	#ifdef COMPILER_MSVC
		#include <tchar.h>
	#else
		#include <wchar.h>
	#endif
	#include <string.h>
	#ifndef _PS3
		#include <malloc.h>
	#endif
#include "tier0/valve_on.h"

#include "commonmacros.h"
#include "memalloc.h"

#ifdef _WIN32
#ifndef MEMALLOC_REGION
#define MEMALLOC_REGION 0
#endif
#else
#undef MEMALLOC_REGION
#endif

#if defined(USE_MEM_DEBUG)
	#if defined( POSIX ) || defined( _PS3 )
		#define _NORMAL_BLOCK 1
		
		#include "tier0/valve_off.h"
		#include <cstddef>
		#include <new>
		#include <sys/types.h>
		#if !defined( DID_THE_OPERATOR_NEW )
                        #define DID_THE_OPERATOR_NEW
			// posix doesn't have a new of this form, so we impl our own
			void* operator new( size_t nSize, int blah, const char *pFileName, int nLine );
			void* operator new[]( size_t nSize, int blah, const char *pFileName, int nLine );
		#endif
	
	#else // defined(POSIX)
	
		// Include crtdbg.h and make sure _DEBUG is set to 1.
		#if !defined(_DEBUG)
			#define _DEBUG 1
			#include <crtdbg.h>
			#undef _DEBUG
		#else
			#include <crtdbg.h>
		#endif // !defined(_DEBUG)
	
	#endif // defined(POSIX)
#endif

#include "tier0/memdbgoff.h"

// --------------------------------------------------------
// Debug/non-debug agnostic elements

#define MEM_OVERRIDE_ON 1

#undef malloc
#undef realloc
#undef calloc
#undef _expand
#undef free
#undef _msize
#undef _aligned_malloc
#undef _aligned_free

#ifndef MEMDBGON_H
inline void *MemAlloc_InlineCallocMemset( void *pMem, size_t nCount, size_t nElementSize)
{
	memset(pMem, 0, nElementSize * nCount);
	return pMem;
}
#endif

#define calloc(c, s)		MemAlloc_InlineCallocMemset(malloc(c*s), c, s)
#ifndef USE_LIGHT_MEM_DEBUG
#define free(p)				g_pMemAlloc->Free( p )
#define _aligned_free( p )	MemAlloc_FreeAligned( p )
#else
extern const char *g_pszModule; 
#define free(p)				g_pMemAlloc->Free( p, g_pszModule, 0 )
#define _aligned_free( p )	MemAlloc_FreeAligned( p, g_pszModule, 0 )
#endif
#define _msize(p)			g_pMemAlloc->GetSize( p )
#define _expand(p, s)		_expand_NoLongerSupported(p, s)

// --------------------------------------------------------
// Debug path
#if defined(USE_MEM_DEBUG)

#define malloc(s)				MemAlloc_Alloc( s, __FILE__, __LINE__)
#define realloc(p, s)			g_pMemAlloc->Realloc( p, s, __FILE__, __LINE__ )
#define _aligned_malloc( s, a )	MemAlloc_AllocAlignedFileLine( s, a, __FILE__, __LINE__ )

#define _malloc_dbg(s, t, f, l)	WHYCALLINGTHISDIRECTLY(s)

#undef new

#if defined( _PS3 )
	#ifndef PS3_OPERATOR_NEW_WRAPPER_DEFINED
		#define PS3_OPERATOR_NEW_WRAPPER_DEFINED
		inline void* operator new( size_t nSize, int blah, const char *pFileName, int nLine ) { return g_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine ); }
		inline void* operator new[]( size_t nSize, int blah, const char *pFileName, int nLine ) { return g_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine ); }
	#endif
	#define new new( 1, __FILE__, __LINE__ )
#elif !defined( GNUC )
	#if defined(__AFX_H__) && defined(DEBUG_NEW)
		#define new DEBUG_NEW
	#else
		#define MEMALL_DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
		#define new MEMALL_DEBUG_NEW
	#endif
#endif

#undef _strdup
#undef strdup
#undef _wcsdup
#undef wcsdup

#define _strdup(s) MemAlloc_StrDup(s, __FILE__, __LINE__)
#define strdup(s)  MemAlloc_StrDup(s, __FILE__, __LINE__)
#define _wcsdup(s) MemAlloc_WcStrDup(s, __FILE__, __LINE__)
#define wcsdup(s)  MemAlloc_WcStrDup(s, __FILE__, __LINE__)

// Make sure we don't define strdup twice
#if !defined(MEMDBGON_H)

inline char *MemAlloc_StrDup(const char *pString, const char *pFileName, unsigned nLine)
{
	char *pMemory;
	
	if (!pString)
		return NULL;
	
	size_t len = strlen(pString) + 1;
	if ((pMemory = (char *)MemAlloc_Alloc(len, pFileName, nLine)) != NULL)
	{
		return strcpy( pMemory, pString );
	}
	
	return NULL;
}

inline wchar_t *MemAlloc_WcStrDup(const wchar_t *pString, const char *pFileName, unsigned nLine)
{
	wchar_t *pMemory;
	
	if (!pString)
		return NULL;
	
	size_t len = (wcslen(pString) + 1);
	if ((pMemory = (wchar_t *)MemAlloc_Alloc(len * sizeof(wchar_t), pFileName, nLine)) != NULL)
	{
		return wcscpy( pMemory, pString );
	}
	
	return NULL;
}

#endif // DBMEM_DEFINED_STRDUP

#else
// --------------------------------------------------------
// Release path

#ifndef USE_LIGHT_MEM_DEBUG
#define malloc(s)				MemAlloc_Alloc( s )
#define realloc(p, s)			g_pMemAlloc->Realloc( p, s )
#define _aligned_malloc( s, a )	MemAlloc_AllocAligned( s, a )
#else
#define malloc(s)				MemAlloc_Alloc( s, g_pszModule, 0  )
#define realloc(p, s)			g_pMemAlloc->Realloc( p, s, g_pszModule, 0 )
#define _aligned_malloc( s, a )	MemAlloc_AllocAlignedFileLine( s, a, g_pszModule, 0 )
#endif

#ifndef _malloc_dbg
#define _malloc_dbg(s, t, f, l)	WHYCALLINGTHISDIRECTLY(s)
#endif

#undef new

#if defined( _PS3 ) && !defined( _CERT )
	#ifndef PS3_OPERATOR_NEW_WRAPPER_DEFINED
		#define PS3_OPERATOR_NEW_WRAPPER_DEFINED
		inline void* operator new( size_t nSize, int blah, const char *pFileName, int nLine ) { return g_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine ); }
		inline void* operator new[]( size_t nSize, int blah, const char *pFileName, int nLine ) { return g_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine ); }
	#endif
	#define new new( 1, __FILE__, __LINE__ )
#endif

#undef _strdup
#undef strdup
#undef _wcsdup
#undef wcsdup

#define _strdup(s) MemAlloc_StrDup(s)
#define strdup(s)  MemAlloc_StrDup(s)
#define _wcsdup(s) MemAlloc_WcStrDup(s)
#define wcsdup(s)  MemAlloc_WcStrDup(s)

// Make sure we don't define strdup twice
#if !defined(MEMDBGON_H)

inline char *MemAlloc_StrDup(const char *pString)
{
	char *pMemory;

	if (!pString)
		return NULL;

	size_t len = strlen(pString) + 1;
	if ((pMemory = (char *)malloc(len)) != NULL)
	{
		return strcpy( pMemory, pString );
	}

	return NULL;
}

inline wchar_t *MemAlloc_WcStrDup(const wchar_t *pString)
{
	wchar_t *pMemory;

	if (!pString)
		return NULL;

	size_t len = (wcslen(pString) + 1);
	if ((pMemory = (wchar_t *)malloc(len * sizeof(wchar_t))) != NULL)
	{
		return wcscpy( pMemory, pString );
	}

	return NULL;
}

#endif // DBMEM_DEFINED_STRDUP

#endif // USE_MEM_DEBUG

#define MEMDBGON_H // Defined here so can be used above

#else

#if defined(USE_MEM_DEBUG)
#ifndef _STATIC_LINKED
#pragma message ("Note: file includes crtdbg.h directly, therefore will cannot use memdbgon.h in non-debug build")
#else
#error "Error: file includes crtdbg.h directly, therefore will cannot use memdbgon.h in non-debug build. Not recoverable in static build"
#endif
#endif
#endif // _INC_CRTDBG

#endif // !STEAM && !NO_MALLOC_OVERRIDE
