#include "tier0/dbg.h"
#include "vstdlib/vstrtools.h"

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif
#if defined(POSIX) && !defined(_PS3)
#include <iconv.h>
#endif

#ifdef _PS3
#include <cell/sysmodule.h>
#include <cell/l10n.h>

class DummyInitL10N
{
public:
	DummyInitL10N()
	{
		int ret = cellSysmoduleLoadModule( CELL_SYSMODULE_L10N );
		if( ret != CELL_OK )
		{
			Warning( "Cannot initialize l10n, unicode services will not work. Error %d\n", ret );
		}
	}
	
	~DummyInitL10N()
	{
		cellSysmoduleUnloadModule( CELL_SYSMODULE_L10N );
	}
}s_dummyInitL10N;
#endif

//-----------------------------------------------------------------------------
// Purpose: Converts a UTF8 string into a unicode string
//-----------------------------------------------------------------------------
int V_UTF8ToUnicode( const char *pUTF8, wchar_t *pwchDest, int cubDestSizeInBytes )
{
	if ( !pUTF8 )
		return 0;

	AssertValidStringPtr(pUTF8);
	AssertValidWritePtr(pwchDest);

	pwchDest[0] = 0;
#ifdef _WIN32
	int cchResult = MultiByteToWideChar( CP_UTF8, 0, pUTF8, -1, pwchDest, cubDestSizeInBytes / sizeof(wchar_t) );
#elif defined( _PS3 )
	size_t cchResult = cubDestSizeInBytes / sizeof( uint16 ), cchSrc = V_strlen( pUTF8 ) + 1;
	L10nResult result = UTF8stoUCS2s( ( const uint8 *) pUTF8, &cchSrc, ( uint16 * ) pwchDest, &cchResult );
	Assert( result == ConversionOK );
	cchResult *= sizeof( uint16 );
#elif POSIX
	iconv_t conv_t = iconv_open( "UTF-32LE", "UTF-8" );
	int cchResult = -1;
	size_t nLenUnicde = cubDestSizeInBytes;
	size_t nMaxUTF8 = strlen(pUTF8) + 1;
	char *pIn = (char *)pUTF8;
	char *pOut = (char *)pwchDest;
	if ( conv_t > 0 )
	{
		cchResult = 0;
		cchResult = iconv( conv_t, &pIn, &nMaxUTF8, &pOut, &nLenUnicde );
		iconv_close( conv_t );
		if ( (int)cchResult < 0 )
			cchResult = 0;
		else
			cchResult = nMaxUTF8;
	}
#endif
	pwchDest[(cubDestSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return cchResult;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a unicode string into a UTF8 (standard) string
//-----------------------------------------------------------------------------
int V_UnicodeToUTF8( const wchar_t *pUnicode, char *pUTF8, int cubDestSizeInBytes )
{
	AssertValidStringPtr(pUTF8, cubDestSizeInBytes);
	AssertValidReadPtr(pUnicode);

	if ( cubDestSizeInBytes > 0 )
	{
		pUTF8[0] = 0;
	}

#ifdef _WIN32
	int cchResult = WideCharToMultiByte( CP_UTF8, 0, pUnicode, -1, pUTF8, cubDestSizeInBytes, NULL, NULL );
#elif defined( _PS3 )
	size_t cchResult = cubDestSizeInBytes, cchSrc = V_wcslen( pUnicode ) + 1;
	L10nResult result = UCS2stoUTF8s( ( const uint16 *) pUnicode, &cchSrc, ( uint8 * ) pUTF8, &cchResult );
	Assert( result == ConversionOK );
#elif POSIX
	int cchResult = 0;
	if ( pUnicode && pUTF8 )
	{
		iconv_t conv_t = iconv_open( "UTF-8", "UTF-32LE" );
		int cchResult = -1;
		size_t nLenUnicde = ( wcslen(pUnicode) + 1 ) * sizeof(wchar_t); // 4 bytes per wchar vs. 1 byte for utf8 for simple english
		size_t nMaxUTF8 = cubDestSizeInBytes;
		char *pIn = (char *)pUnicode;
		char *pOut = (char *)pUTF8;
		if ( conv_t > 0 )
		{
			cchResult = 0;
			cchResult = iconv( conv_t, &pIn, &nLenUnicde, &pOut, &nMaxUTF8 );
			iconv_close( conv_t );
			if ( (int)cchResult < 0 )
				cchResult = 0;
			else
				cchResult = nMaxUTF8;
		}
	}
#endif

	if ( cubDestSizeInBytes > 0 )
	{
		pUTF8[cubDestSizeInBytes - 1] = 0;
	}

	return cchResult;
}


//-----------------------------------------------------------------------------
// Purpose: Converts a ucs2 string to a unicode (wchar_t) one, no-op on win32
//-----------------------------------------------------------------------------
int V_UCS2ToUnicode( const ucs2 *pUCS2, wchar_t *pUnicode, int cubDestSizeInBytes )
{
	AssertValidWritePtr(pUnicode);
	AssertValidReadPtr(pUCS2);
	
	pUnicode[0] = 0;
#if defined( _WIN32 ) || defined( _PS3 )
	int lenUCS2 = V_wcslen( pUCS2 );
	int cchResult = MIN( (lenUCS2+1)*( int )sizeof(ucs2), cubDestSizeInBytes );
	V_wcsncpy( (wchar_t*)pUCS2, pUnicode, cchResult );
#else
	iconv_t conv_t = iconv_open( "UCS-4LE", "UCS-2LE" );
	int cchResult = -1;
	size_t nLenUnicde = cubDestSizeInBytes;
	size_t nMaxUTF8 = cubDestSizeInBytes;
	char *pIn = (char *)pUCS2;
	char *pOut = (char *)pUnicode;
	if ( conv_t > 0 )
	{
		cchResult = 0;
		cchResult = iconv( conv_t, &pIn, &nLenUnicde, &pOut, &nMaxUTF8 );
		iconv_close( conv_t );
		if ( (int)cchResult < 0 )
			cchResult = 0;
		else
			cchResult = nMaxUTF8;
	}
#endif
	pUnicode[(cubDestSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return cchResult;	

}


//-----------------------------------------------------------------------------
// Purpose: Converts a wchar_t string into a UCS2 string -noop on windows
//-----------------------------------------------------------------------------
int V_UnicodeToUCS2( const wchar_t *pUnicode, int cubSrcInBytes, char *pUCS2, int cubDestSizeInBytes )
{
	 // TODO: MACMERGE: Figure out how to convert from 2-byte Win32 wchars to platform wchar_t type that can be 4 bytes
#if defined( _WIN32 ) || defined( _PS3 )
	int cchResult = MIN( cubSrcInBytes, cubDestSizeInBytes );
	V_wcsncpy( (wchar_t*)pUCS2, pUnicode, cchResult );
#elif defined (POSIX)
	iconv_t conv_t = iconv_open( "UCS-2LE", "UTF-32LE" );
	size_t cchResult = -1;
	size_t nLenUnicde = cubSrcInBytes;
	size_t nMaxUCS2 = cubDestSizeInBytes;
	char *pIn = (char*)pUnicode;
	char *pOut = pUCS2;
	if ( conv_t > 0 )
	{
		cchResult = 0;
		cchResult = iconv( conv_t, &pIn, &nLenUnicde, &pOut, &nMaxUCS2 );
		iconv_close( conv_t );
		if ( (int)cchResult < 0 )
			cchResult = 0;
		else
			cchResult = cubSrcInBytes / sizeof( wchar_t );
	}
#endif
	return cchResult;	
}

// UTF-8 encodes each character (code point) in 1 to 4 octets (8-bit bytes).
// The first 128 characters of the Unicode character set (which correspond directly to the ASCII) use a single octet with the same binary value as in ASCII.
// url:http://en.wikipedia.org/wiki/UTF-8
#define MAX_UTF8_CHARACTER_BYTES 4


//-----------------------------------------------------------------------------
// Purpose: Converts a ucs-2 (windows wchar_t) string into a UTF8 (standard) string
//-----------------------------------------------------------------------------
VSTRTOOLS_INTERFACE int V_UCS2ToUTF8( const ucs2 *pUCS2, char *pUTF8, int cubDestSizeInBytes )
{
	AssertValidStringPtr(pUTF8, cubDestSizeInBytes);
	AssertValidReadPtr(pUCS2);
	Assert( cubDestSizeInBytes >= 1 ); // must have at least 1 byte to write the terminator character
	
	pUTF8[0] = '\0';
#ifdef _WIN32
	// under win32 wchar_t == ucs2, sigh
	int cchResult = WideCharToMultiByte( CP_UTF8, 0, pUCS2, -1, pUTF8, cubDestSizeInBytes, NULL, NULL );
#elif defined( _PS3 )
	size_t cchResult = cubDestSizeInBytes, cchSrc = V_wcslen( pUCS2 ) + 1;
	L10nResult result = UCS2stoUTF8s( ( const uint16 *) pUCS2, &cchSrc, ( uint8 * ) pUTF8, &cchResult );
	Assert( result == ConversionOK );
#elif defined(POSIX)
	iconv_t conv_t = iconv_open( "UTF-8", "UCS-2LE" );
	size_t cchResult = -1;
	size_t nLenUnicde = cubDestSizeInBytes;
	size_t nMaxUTF8 = cubDestSizeInBytes;
	char *pIn = (char *)pUCS2;
	char *pOut = (char *)pUTF8;
	if ( conv_t > 0 )
	{
		cchResult = 0;
		cchResult = iconv( conv_t, &pIn, &nLenUnicde, &pOut, &nMaxUTF8 );
		iconv_close( conv_t );
		if ( (int)cchResult < 0 )
			cchResult = 0;
		else
			cchResult = nMaxUTF8;
	}
#endif
	pUTF8[cubDestSizeInBytes - 1] = '\0';
	return cchResult;	
}


//-----------------------------------------------------------------------------
// Purpose: Converts a UTF8 to ucs-2 (windows wchar_t)
//-----------------------------------------------------------------------------
VSTRTOOLS_INTERFACE int V_UTF8ToUCS2( const char *pUTF8, int cubSrcInBytes, ucs2 *pUCS2, int cubDestSizeInBytes )
{
	AssertValidStringPtr(pUTF8, cubDestSizeInBytes);
	AssertValidReadPtr(pUCS2);

	pUCS2[0] = 0;
#ifdef _WIN32
	// under win32 wchar_t == ucs2, sigh
	int cchResult = MultiByteToWideChar( CP_UTF8, 0, pUTF8, -1, pUCS2, cubDestSizeInBytes / sizeof(wchar_t) );
#elif defined( _PS3 )
	size_t cchResult = cubDestSizeInBytes / sizeof( uint16 ), cchSrc = cubSrcInBytes;
	L10nResult result = UTF8stoUCS2s( ( const uint8 *) pUTF8, &cchSrc, ( uint16 * ) pUCS2, &cchResult );
	Assert( result == ConversionOK );
	cchResult *= sizeof( uint16 );
#elif defined(POSIX)
	iconv_t conv_t = iconv_open( "UCS-2LE", "UTF-8" );
	size_t cchResult = -1;
	size_t nLenUnicde = cubSrcInBytes;
	size_t nMaxUTF8 = cubDestSizeInBytes;
	char *pIn = (char *)pUTF8;
	char *pOut = (char *)pUCS2;
	if ( conv_t > 0 )
	{
		cchResult = 0;
		cchResult = iconv( conv_t, &pIn, &nLenUnicde, &pOut, &nMaxUTF8 );
		iconv_close( conv_t );
		if ( (int)cchResult < 0 )
			cchResult = 0;
		else
			cchResult = cubSrcInBytes;

	}
#endif
	pUCS2[ (cubDestSizeInBytes/sizeof(ucs2)) - 1] = 0;
	return cchResult;	
}

//-----------------------------------------------------------------------------
// Purpose: copies at most nMaxBytes of the UTF-8 input data into the destination,
// ensuring that a trailing multi-byte sequence isn't truncated.
//-----------------------------------------------------------------------------
VSTRTOOLS_INTERFACE void * V_UTF8_strncpy( char *pDest, const char *pSrc, size_t nMaxBytes )
{
  strncpy( pDest, pSrc, nMaxBytes );

  // http://en.wikipedia.org/wiki/UTF-8
  int end = nMaxBytes-1;
  pDest[end] = 0;

  int nBytesSeen = 0, nBytesExpected = 0;
  // walk backwards, ignoring nulls
  while ( pDest[end] == 0 )
    --end;
  
  // found a non-null - see if it's part of a multi-byte sequence
  while ( ( pDest[end] & 0x80 ) && !( pDest[end] & 0x40 ) )
  {
    nBytesSeen++;
    --end;
  }
  
  if ( ( pDest[end] & 0xC0 ) == 0xC0 )
  {
    for ( int i = 6; i > 1; --i )
    {
      if ( (char)( pDest[end] >> i ) & 0x1 )
        ++nBytesExpected;
    }
  }
  
  if ( nBytesExpected != nBytesSeen )
    pDest[end] = 0;
    
  return pDest;
}