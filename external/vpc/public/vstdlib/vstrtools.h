//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// 
// Functions for UCS/UTF/Unicode string operations. These functions are in vstdlib
// instead of tier1, because on PS/3 they need to load and initialize a system module,
// which is more frugal to do from a single place rather than multiple times in different PRX'es.
// The functions themselves aren't supposed to be called frequently enough for the DLL/PRX boundary
// marshalling, if any, to have any measureable impact on performance.
//
#ifndef VSTRTOOLS_HDR
#define VSTRTOOLS_HDR

#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "tier1/strtools.h"

#ifdef STATIC_VSTDLIB
#define VSTRTOOLS_INTERFACE
#else
#ifdef VSTDLIB_DLL_EXPORT
#define VSTRTOOLS_INTERFACE DLL_EXPORT
#else
#define VSTRTOOLS_INTERFACE DLL_IMPORT
#endif
#endif

// conversion functions wchar_t <-> char, returning the number of characters converted
VSTRTOOLS_INTERFACE int V_UTF8ToUnicode( const char *pUTF8, wchar_t *pwchDest, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UnicodeToUTF8( const wchar_t *pUnicode, char *pUTF8, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UCS2ToUnicode( const ucs2 *pUCS2, wchar_t *pUnicode, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UCS2ToUTF8( const ucs2 *pUCS2, char *pUTF8, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UnicodeToUCS2( const wchar_t *pUnicode, int cubSrcInBytes, char *pUCS2, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UTF8ToUCS2( const char *pUTF8, int cubSrcInBytes, ucs2 *pUCS2, int cubDestSizeInBytes );

// copy at most n bytes into destination, will not corrupt utf-8 multi-byte sequences
VSTRTOOLS_INTERFACE void * V_UTF8_strncpy( char *pDest, const char *pSrc, size_t nMaxBytes );


//
// This utility class is for performing UTF-8 <-> UTF-16 conversion.
// It is intended for use with function/method parameters.
//
// For example, you can call
//     FunctionTakingUTF16( CStrAutoEncode( utf8_string ).ToWString() )
// or
//     FunctionTakingUTF8( CStrAutoEncode( utf16_string ).ToString() )
//
// The converted string is allocated off the heap, and destroyed when
// the object goes out of scope.
//
// if the string cannot be converted, NULL is returned.
//
// This class doesn't have any conversion operators; the intention is
// to encourage the developer to get used to having to think about which
// encoding is desired.
//
class CStrAutoEncode
{
public:

	// ctor
	explicit CStrAutoEncode( const char *pch )
	{
		m_pch = pch;
		m_pwch = NULL;
#if !defined( WIN32 ) && !defined(_WIN32)
		m_pucs2 = NULL;
		m_bCreatedUCS2 = false;
#endif
		m_bCreatedUTF16 = false;
	}

	// ctor
	explicit CStrAutoEncode( const wchar_t *pwch )
	{
		m_pch = NULL;
		m_pwch = pwch;
#if !defined( WIN32 ) && !defined(_WIN32)
		m_pucs2 = NULL;
		m_bCreatedUCS2 = false;
#endif
		m_bCreatedUTF16 = true;
	}

#if !defined(WIN32) && !defined(_WINDOWS) && !defined(_WIN32) && !defined(_PS3)
	explicit CStrAutoEncode( const ucs2 *pwch )
	{
		m_pch = NULL;
		m_pwch = NULL;
		m_pucs2 = pwch;
		m_bCreatedUCS2 = true;
		m_bCreatedUTF16 = false;
	}
#endif

	// returns the UTF-8 string, converting on the fly.
	const char* ToString()
	{
		PopulateUTF8();
		return m_pch;
	}

	// returns the UTF-8 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	char* ToStringWritable()
	{
		PopulateUTF8();
		return const_cast< char* >( m_pch );
	}

	// returns the UTF-16 string, converting on the fly.
	const wchar_t* ToWString()
	{
		PopulateUTF16();
		return m_pwch;
	}

#if !defined( WIN32 ) && !defined(_WIN32)
	// returns the UTF-16 string, converting on the fly.
	const ucs2* ToUCS2String()
	{
		PopulateUCS2();
		return m_pucs2;
	}
#endif

	// returns the UTF-16 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	wchar_t* ToWStringWritable()
	{
		PopulateUTF16();
		return const_cast< wchar_t* >( m_pwch );
	}

	// dtor
	~CStrAutoEncode()
	{
		// if we're "native unicode" then the UTF-8 string is something we allocated,
		// and vice versa.
		if ( m_bCreatedUTF16 )
		{
			delete [] m_pch;
		}
		else
		{
			delete [] m_pwch;
		}
#if !defined( WIN32 ) && !defined(_WIN32)
		if ( !m_bCreatedUCS2 && m_pucs2 )
			delete [] m_pucs2;
#endif
	}

private:
	// ensure we have done any conversion work required to farm out a
	// UTF-8 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (four bytes per Unicode code point). This is usually quite pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF8()
	{
		if ( !m_bCreatedUTF16 )
			return;					// no work to do
		if ( m_pwch == NULL )
			return;					// don't have a UTF-16 string to convert
		if ( m_pch != NULL )
			return;					// already been converted to UTF-8; no work to do

		// each Unicode code point can expand to as many as four bytes in UTF-8; we
		// also need to leave room for the terminating NUL.
		uint32 cbMax = 4 * static_cast<uint32>( V_wcslen( m_pwch ) ) + 1;
		char *pchTemp = new char[ cbMax ];
		if ( V_UnicodeToUTF8( m_pwch, pchTemp, cbMax ) )
		{
			uint32 cchAlloc = static_cast<uint32>( V_strlen( pchTemp ) ) + 1;
			char *pchHeap = new char[ cchAlloc ];
			V_strncpy( pchHeap, pchTemp, cchAlloc );
			delete [] pchTemp;
			m_pch = pchHeap;
		}
		else
		{
			// do nothing, and leave the UTF-8 string NULL
			delete [] pchTemp;
		}
	}

	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (one code point per UTF-8 byte). This is sometimes pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF16()
	{
		if ( m_bCreatedUTF16 )
			return;					// no work to do
		if ( m_pch == NULL )
			return;					// no UTF-8 string to convert
		if ( m_pwch != NULL )
			return;					// already been converted to UTF-16; no work to do

		uint32 cchMax = static_cast<uint32>( V_strlen( m_pch ) ) + 1;
		wchar_t *pwchTemp = new wchar_t[ cchMax ];
		if ( V_UTF8ToUnicode( m_pch, pwchTemp, cchMax * sizeof( wchar_t ) ) )
		{
			uint32 cchAlloc = static_cast<uint32>( V_wcslen( pwchTemp ) ) + 1;
			wchar_t *pwchHeap = new wchar_t[ cchAlloc ];
			V_wcsncpy( pwchHeap, pwchTemp, cchAlloc * sizeof( wchar_t ) );
			delete [] pwchTemp;
			m_pwch = pwchHeap;
		}
		else
		{
			// do nothing, and leave the UTF-16 string NULL
			delete [] pwchTemp;
		}
	}

#if !defined( WIN32 ) && !defined(_WIN32)
	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (one code point per UTF-8 byte). This is sometimes pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUCS2()
	{
		if ( m_bCreatedUCS2 )
			return;
		if ( m_pch == NULL )
			return;					// no UTF-8 string to convert
		if ( m_pucs2 != NULL )
			return;					// already been converted to UTF-16; no work to do

		uint32 cchMax = static_cast<uint32>( V_strlen( m_pch ) ) + 1;
		ucs2 *pwchTemp = new ucs2[ cchMax ];
		if ( V_UTF8ToUCS2( m_pch, cchMax, pwchTemp, cchMax * sizeof( ucs2 ) ) )
		{
			uint32 cchAlloc = cchMax;
			ucs2 *pwchHeap = new ucs2[ cchAlloc ];
			memcpy( pwchHeap, pwchTemp, cchAlloc * sizeof( ucs2 ) );
			delete [] pwchTemp;
			m_pucs2 = pwchHeap;
		}
		else
		{
			// do nothing, and leave the UTF-16 string NULL
			delete [] pwchTemp;
		}
	}
#endif

	// one of these pointers is an owned pointer; whichever
	// one is the encoding OTHER than the one we were initialized
	// with is the pointer we've allocated and must free.
	const char *m_pch;
	const wchar_t *m_pwch;
#if !defined( WIN32 ) && !defined(_WIN32)
	const ucs2 *m_pucs2;
	bool m_bCreatedUCS2;
#endif
	// "created as UTF-16", means our owned string is the UTF-8 string not the UTF-16 one.
	bool m_bCreatedUTF16;

};


#define V_UTF8ToUnicode			V_UTF8ToUnicode
#define V_UnicodeToUTF8			V_UnicodeToUTF8


#endif 