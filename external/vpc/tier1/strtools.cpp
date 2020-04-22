//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: String Tools
//
//===========================================================================//

// These are redefined in the project settings to prevent anyone from using them.
// We in this module are of a higher caste and thus are privileged in their use.
#ifdef strncpy
	#undef strncpy
#endif

#ifdef _snprintf
	#undef _snprintf
#endif

#if defined( sprintf )
	#undef sprintf
#endif

#if defined( vsprintf )
	#undef vsprintf
#endif

#ifdef _vsnprintf
#ifdef _WIN32
	#undef _vsnprintf
#endif
#endif

#ifdef vsnprintf
#ifndef _WIN32
	#undef vsnprintf
#endif
#endif

#if defined( strcat )
	#undef strcat
#endif

#ifdef strncat
	#undef strncat
#endif

// NOTE: I have to include stdio + stdarg first so vsnprintf gets compiled in
#include <stdio.h>
#include <stdarg.h>

#include "tier0/basetypes.h"
#include "tier0/platform.h"

#ifdef stricmp
#undef stricmp
#endif

#ifdef POSIX

#ifndef _PS3
#include <iconv.h>
#endif // _PS3

#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#define stricmp strcasecmp
#elif _WIN32
#include <direct.h>
#if !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#ifdef _WIN32
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#endif
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include <string.h>
#include <stdlib.h>
#include "tier1/utldict.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#elif defined( _PS3 )
#include "ps3_pathinfo.h"
#include <cell/l10n.h> // for UCS-2 to UTF-8 conversion
#endif
#include "tier0/vprof.h"
#include "tier0/memdbgon.h"

#ifndef NDEBUG
static volatile char const *pDebugString;
#define DEBUG_LINK_CHECK pDebugString = "tier1.lib built debug!"
#else
#define DEBUG_LINK_CHECK
#endif

void _V_memset (void *dest, int fill, int count)
{
	DEBUG_LINK_CHECK;
	Assert( count >= 0 );
	AssertValidWritePtr( dest, count );

	memset(dest,fill,count);
}

void _V_memcpy (void *dest, const void *src, int count)
{
	Assert( count >= 0 );
	AssertValidReadPtr( src, count );
	AssertValidWritePtr( dest, count );

	memcpy( dest, src, count );
}

void _V_memmove(void *dest, const void *src, int count)
{
	Assert( count >= 0 );
	AssertValidReadPtr( src, count );
	AssertValidWritePtr( dest, count );

	memmove( dest, src, count );
}

int _V_memcmp (const void *m1, const void *m2, int count)
{
	DEBUG_LINK_CHECK;
	Assert( count >= 0 );
	AssertValidReadPtr( m1, count );
	AssertValidReadPtr( m2, count );

	return memcmp( m1, m2, count );
}

int	_V_strlen(const char *str)
{
	AssertValidStringPtr(str);
#ifdef POSIX
	if ( !str )
		return 0;
#endif
	return strlen( str );
}

void _V_strcpy (char *dest, const char *src)
{
	DEBUG_LINK_CHECK;
	AssertValidWritePtr(dest);
	AssertValidStringPtr(src);

	strcpy( dest, src );
}

int	_V_wcslen(const wchar_t *pwch)
{
	return wcslen( pwch );
}

char *_V_strrchr(const char *s, char c)
{
	AssertValidStringPtr( s );
    int len = V_strlen(s);
    s += len;
    while (len--)
	if (*--s == c) return (char *)s;
    return 0;
}

int _V_strcmp (const char *s1, const char *s2)
{
	AssertValidStringPtr( s1 );
	AssertValidStringPtr( s2 );
	VPROF_2( "V_strcmp", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_ALL );

	return strcmp( s1, s2 );
}

int _V_wcscmp (const wchar_t *s1, const wchar_t *s2)
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;              // strings not equal    
		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}
	
	return -1;
}



#define TOLOWERC( x )  (( ( x >= 'A' ) && ( x <= 'Z' ) )?( x + 32 ) : x )
int	_V_stricmp( const char *s1, const char *s2 )
{
	VPROF_2( "V_stricmp", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_ALL );
#ifdef POSIX
	if ( s1 == NULL && s2 == NULL )
		return 0;
	if ( s1 == NULL )
		return -1;
	if ( s2 == NULL )
		return 1;
	
	return stricmp( s1, s2 );
#else	
	uint8 const *pS1 = ( uint8 const * ) s1;
	uint8 const *pS2 = ( uint8 const * ) s2;
	for(;;)
	{
		int c1 = *( pS1++ );
		int c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
		c1 = *( pS1++ );
		c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
	}
#endif
}

// A special high-performance case-insensitive compare function
// returns 0 if strings match exactly
// returns >0 if strings match in a case-insensitive way, but do not match exactly
// returns <0 if strings do not match even in a case-insensitive way
int	_V_stricmp_NegativeForUnequal( const char *s1, const char *s2 )
{
	VPROF_2( "V_stricmp", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_ALL );
	uint8 const *pS1 = ( uint8 const * ) s1;
	uint8 const *pS2 = ( uint8 const * ) s2;
	int iExactMatchResult = 1;
	for(;;)
	{
		int c1 = *( pS1++ );
		int c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			// strings are case-insensitive equal, coerce accumulated
			// case-difference to 0/1 and return it
			if ( !c1 ) return !iExactMatchResult;
		}
		else
		{
			if ( ! c2 )
			{
				// c2=0 and != c1  =>  not equal
				return -1;
			}
			iExactMatchResult = 0;
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				// strings are not equal
				return -1;
			}
		}
		c1 = *( pS1++ );
		c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			// strings are case-insensitive equal, coerce accumulated
			// case-difference to 0/1 and return it
			if ( !c1 ) return !iExactMatchResult;
		}
		else
		{
			if ( ! c2 )
			{
				// c2=0 and != c1  =>  not equal
				return -1;
			}
			iExactMatchResult = 0;
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				// strings are not equal
				return -1;
			}
		}
	}
}


char *_V_strstr( const char *s1, const char *search )
{
	AssertValidStringPtr( s1 );
	AssertValidStringPtr( search );

#if defined( _X360 )
	return (char *)strstr( (char *)s1, search );
#else
	return (char *)strstr( s1, search );
#endif
}

char *_V_strupr (char *start)
{
	AssertValidStringPtr( start );
	return strupr( start );
}


char *_V_strlower (char *start)
{
	AssertValidStringPtr( start );
	return strlwr(start);
}

wchar_t *_V_wcsupr (const char* file, int line, wchar_t *start)
{
	return _wcsupr( start );
}


wchar_t *_V_wcslower (const char* file, int line, wchar_t *start)
{
	return _wcslwr(start);
}


int V_strncmp (const char *s1, const char *s2, int count)
{
	Assert( count >= 0 );
	AssertValidStringPtr( s1, count );
	AssertValidStringPtr( s2, count );
	VPROF_2( "V_strcmp", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_ALL );

	while ( count-- > 0 )
	{
		if ( *s1 != *s2 )
			return *s1 < *s2 ? -1 : 1; // string different
		if ( *s1 == '\0' )
			return 0; // null terminator hit - strings the same
		s1++;
		s2++;
	}

	return 0; // count characters compared the same
}


char *V_strnlwr(char *s, size_t count)
{
	Assert( count >= 0 );
	AssertValidStringPtr( s, count );

	char* pRet = s;
	if ( !s || !count )
		return s;

	while ( -- count > 0 )
	{
		if ( !*s )
			return pRet; // reached end of string

		*s = tolower( *s );
		++s;
	}

	*s = 0; // null-terminate original string at "count-1"
	return pRet;
}


int V_strncasecmp (const char *s1, const char *s2, int n)
{
	Assert( n >= 0 );
	AssertValidStringPtr( s1 );
	AssertValidStringPtr( s2 );
	VPROF_2( "V_strcmp", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_ALL );
	
	while ( n-- > 0 )
	{
		int c1 = *s1++;
		int c2 = *s2++;

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return c1 < c2 ? -1 : 1;
		}
		if ( c1 == '\0' )
			return 0; // null terminator hit - strings the same
	}
	
	return 0; // n characters compared the same
}

int V_strcasecmp( const char *s1, const char *s2 )
{
	AssertValidStringPtr( s1 );
	AssertValidStringPtr( s2 );
	VPROF_2( "V_strcmp", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_ALL );

	return V_stricmp( s1, s2 );
}

int V_strnicmp (const char *s1, const char *s2, int n)
{
	DEBUG_LINK_CHECK;
	Assert( n >= 0 );
	AssertValidStringPtr(s1);
	AssertValidStringPtr(s2);

	return V_strncasecmp( s1, s2, n );
}


const char *StringAfterPrefix( const char *str, const char *prefix )
{
	AssertValidStringPtr( str );
	AssertValidStringPtr( prefix );
	do
	{
		if ( !*prefix )
			return str;
	}
	while ( tolower( *str++ ) == tolower( *prefix++ ) );
	return NULL;
}

const char *StringAfterPrefixCaseSensitive( const char *str, const char *prefix )
{
	AssertValidStringPtr( str );
	AssertValidStringPtr( prefix );
	do
	{
		if ( !*prefix )
			return str;
	}
	while ( *str++ == *prefix++ );
	return NULL;
}

uint64 V_atoui64( const char *str )
{
	AssertValidStringPtr( str );

	uint64             val;
	uint64             c;

	Assert( str );

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val<<4) + c - 'A' + 10;
			else
				return val;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
	{
		return str[1];
	}

	//
	// assume decimal
	//
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val;
		val = val*10 + c - '0';
	}

	return 0;
}

int64 V_atoi64( const char *str )
{
	AssertValidStringPtr( str );

	int64             val;
	int64             sign;
	int64             c;
	
	Assert( str );
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;
		
	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val<<4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}
	
//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}
	
//
// assume decimal
//
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val*10 + c - '0';
	}
	
	return 0;
}

int V_atoi( const char *str )
{ 
	return (int)V_atoi64( str );
}

float V_atof (const char *str)
{
	DEBUG_LINK_CHECK;
	AssertValidStringPtr( str );
	double			val;
	int             sign;
	int             c;
	int             decimal, total;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;
		
	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val*16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val*16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val*16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}
	
//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}
	
//
// assume decimal
//
	decimal = -1;
	total = 0;
	int exponent = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
		{
			if ( c == 'e' )
			{
				exponent = V_atoi(str);
			}
			break;
		}
		val = val*10 + c - '0';
		total++;
	}

	if ( exponent != 0 )
	{
		val *= pow( 10.0, exponent );
	}
	if (decimal == -1)
		return val*sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}
	
	return val*sign;
}

//-----------------------------------------------------------------------------
// Normalizes a float string in place.  
//
// (removes leading zeros, trailing zeros after the decimal point, and the decimal point itself where possible)
//-----------------------------------------------------------------------------
void V_normalizeFloatString( char* pFloat )
{
	// If we have a decimal point, remove trailing zeroes:
	if( strchr( pFloat,'.' ) )
	{
		int len = V_strlen(pFloat);

		while( len > 1 && pFloat[len - 1] == '0' )
		{
			pFloat[len - 1] = '\0';
			len--;
		}

		if( len > 1 && pFloat[ len - 1 ] == '.' )
		{
			pFloat[len - 1] = '\0';
			len--;
		}
	}

	// TODO: Strip leading zeros

}

FORCEINLINE unsigned char tolower_fast(unsigned char c)
{
	if ( (c >= 'A') && (c <= 'Z') )
		return c + ('a' - 'A');
	return c;
}

//-----------------------------------------------------------------------------
// Finds a string in another string with a case insensitive test
//-----------------------------------------------------------------------------
char const* V_stristr( char const* pStr, char const* pSearch )
{
	AssertValidStringPtr(pStr);
	AssertValidStringPtr(pSearch);

	if (!pStr || !pSearch) 
		return 0;

	char const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower_fast((unsigned char)*pLetter) == tolower_fast((unsigned char)*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (tolower_fast((unsigned char)*pMatch) != tolower_fast((unsigned char)*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}

char* V_stristr( char* pStr, char const* pSearch )
{
	AssertValidStringPtr( pStr );
	AssertValidStringPtr( pSearch );

	return (char*)V_stristr( (char const*)pStr, pSearch );
}

//-----------------------------------------------------------------------------
// Finds a string in another string with a case insensitive test w/ length validation
//-----------------------------------------------------------------------------
char const* V_strnistr( char const* pStr, char const* pSearch, int n )
{
	AssertValidStringPtr(pStr);
	AssertValidStringPtr(pSearch);

	if (!pStr || !pSearch) 
		return 0;

	char const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		if ( n <= 0 )
			return 0;

		// Skip over non-matches
		if (tolower_fast(*pLetter) == tolower_fast(*pSearch))
		{
			int n1 = n - 1;

			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				if ( n1 <= 0 )
					return 0;

				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (tolower_fast(*pMatch) != tolower_fast(*pTest))
					break;

				++pMatch;
				++pTest;
				--n1;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
		--n;
	}

	return 0;
}

const char* V_strnchr( const char* pStr, char c, int n )
{
	char const* pLetter = pStr;
	char const* pLast = pStr + n;

	// Check the entire string
	while ( (pLetter < pLast) && (*pLetter != 0) )
	{
		if (*pLetter == c)
			return pLetter;
		++pLetter;
	}
	return NULL;
}



void V_strncpy( char *pDest, char const *pSrc, int maxLen )
{
	Assert( maxLen >= 0 );
	AssertValidWritePtr( pDest, maxLen );
	AssertValidStringPtr( pSrc );

	DEBUG_LINK_CHECK;

	strncpy( pDest, pSrc, maxLen );
	if ( maxLen > 0 )
	{
		pDest[maxLen-1] = 0;
	}
}

void V_wcsncpy( wchar_t *pDest, wchar_t const *pSrc, int maxLenInBytes )
{
	Assert( maxLenInBytes >= 0 );
	AssertValidWritePtr( pDest, maxLenInBytes );
	AssertValidReadPtr( pSrc );

	int maxLen = maxLenInBytes / sizeof(wchar_t);

	wcsncpy( pDest, pSrc, maxLen );
	if( maxLen )
	{
		pDest[maxLen-1] = 0;
	}
}



int V_snwprintf( wchar_t *pDest, int maxLenInNumWideCharacters, const wchar_t *pFormat, ... )
{
	Assert( maxLenInNumWideCharacters >= 0 );
	AssertValidWritePtr( pDest, maxLenInNumWideCharacters );
	AssertValidReadPtr( pFormat );

	va_list marker;

	va_start( marker, pFormat );
#ifdef _WIN32
	int len = _vsnwprintf( pDest, maxLenInNumWideCharacters, pFormat, marker );
#elif POSIX
	int len = vswprintf( pDest, maxLenInNumWideCharacters, pFormat, marker );
#else
#error "define vsnwprintf type."
#endif
	va_end( marker );

	// Len < 0 represents an overflow
	if ( ( len < 0 ) ||
		 ( maxLenInNumWideCharacters > 0 && len >= maxLenInNumWideCharacters ) )
	{
		len = maxLenInNumWideCharacters;
		pDest[maxLenInNumWideCharacters-1] = 0;
	}
	
	return len;
}


int V_snprintf( char *pDest, int maxLen, char const *pFormat, ... )
{
	Assert( maxLen >= 0 );
	AssertValidWritePtr( pDest, maxLen );
	AssertValidStringPtr( pFormat );

	va_list marker;

	va_start( marker, pFormat );
#ifdef _WIN32
	int len = _vsnprintf( pDest, maxLen, pFormat, marker );
#elif POSIX
	int len = vsnprintf( pDest, maxLen, pFormat, marker );
#else
	#error "define vsnprintf type."
#endif
	va_end( marker );

	// Len < 0 represents an overflow
	if( len < 0 )
	{
		len = maxLen;
		pDest[maxLen-1] = 0;
	}

	return len;
}


int V_vsnprintf( char *pDest, int maxLen, char const *pFormat, va_list params )
{
	Assert( maxLen > 0 );
	AssertValidWritePtr( pDest, maxLen );
	AssertValidStringPtr( pFormat );

	int len = _vsnprintf( pDest, maxLen, pFormat, params );

	if( len < 0 )
	{
		len = maxLen;
		pDest[maxLen-1] = 0;
	}

	return len;
}


int V_vsnprintfRet( char *pDest, int maxLen, const char *pFormat, va_list params, bool *pbTruncated )
{
	Assert( maxLen > 0 );
	AssertValidWritePtr( pDest, maxLen );
	AssertValidStringPtr( pFormat );

	int len = _vsnprintf( pDest, maxLen, pFormat, params );

	if ( pbTruncated )
	{
		*pbTruncated = len < 0;
	}

	if( len < 0 )
	{
		len = maxLen;
		pDest[maxLen-1] = 0;
	}

	return len;
}



//-----------------------------------------------------------------------------
// Purpose: If COPY_ALL_CHARACTERS == max_chars_to_copy then we try to add the whole pSrc to the end of pDest, otherwise
//  we copy only as many characters as are specified in max_chars_to_copy (or the # of characters in pSrc if thats's less).
// Input  : *pDest - destination buffer
//			*pSrc - string to append
//			destBufferSize - sizeof the buffer pointed to by pDest
//			max_chars_to_copy - COPY_ALL_CHARACTERS in pSrc or max # to copy
// Output : char * the copied buffer
//-----------------------------------------------------------------------------
char *V_strncat(char *pDest, const char *pSrc, size_t maxLenInBytes, int max_chars_to_copy )
{
	DEBUG_LINK_CHECK;
	size_t charstocopy = (size_t)0;

	Assert( maxLenInBytes >= 0 );
	AssertValidStringPtr( pDest);
	AssertValidStringPtr( pSrc );
	
	size_t len = strlen(pDest);
	size_t srclen = strlen( pSrc );
	if ( max_chars_to_copy <= COPY_ALL_CHARACTERS )
	{
		charstocopy = srclen;
	}
	else
	{
		charstocopy = (size_t)MIN( max_chars_to_copy, (int)srclen );
	}

	if ( len + charstocopy >= maxLenInBytes )
	{
		charstocopy = maxLenInBytes - len - 1;
	}

	if ( !charstocopy )
	{
		return pDest;
	}

	char *pOut = strncat( pDest, pSrc, charstocopy );
	pOut[maxLenInBytes-1] = 0;
	return pOut;
}

//-----------------------------------------------------------------------------
// Purpose: If COPY_ALL_CHARACTERS == max_chars_to_copy then we try to add the whole pSrc to the end of pDest, otherwise
//  we copy only as many characters as are specified in max_chars_to_copy (or the # of characters in pSrc if thats's less).
// Input  : *pDest - destination buffer
//			*pSrc - string to append
//			maxLenInCharacters - sizeof the buffer in characters pointed to by pDest
//			max_chars_to_copy - COPY_ALL_CHARACTERS in pSrc or max # to copy
// Output : char * the copied buffer
//-----------------------------------------------------------------------------
wchar_t *V_wcsncat( wchar_t *pDest, const wchar_t *pSrc, int maxLenInBytes, int max_chars_to_copy )
{
	DEBUG_LINK_CHECK;
	size_t charstocopy = (size_t)0;

    Assert( maxLenInBytes >= 0 );
    
	int maxLenInCharacters = maxLenInBytes / sizeof( wchar_t );

	size_t len = wcslen(pDest);
	size_t srclen = wcslen( pSrc );
	if ( max_chars_to_copy <= COPY_ALL_CHARACTERS )
	{
		charstocopy = srclen;
	}
	else
	{
		charstocopy = (size_t)MIN( max_chars_to_copy, (int)srclen );
	}

	if ( len + charstocopy >= (size_t)maxLenInCharacters )
	{
		charstocopy = maxLenInCharacters - len - 1;
	}

	if ( !charstocopy )
	{
		return pDest;
	}

	wchar_t *pOut = wcsncat( pDest, pSrc, charstocopy );
	pOut[maxLenInCharacters-1] = 0;
	return pOut;
}



//-----------------------------------------------------------------------------
// Purpose: Converts value into x.xx MB/ x.xx KB, x.xx bytes format, including commas
// Input  : value - 
//			2 - 
//			false - 
// Output : char
//-----------------------------------------------------------------------------
#define NUM_PRETIFYMEM_BUFFERS 8
char *V_pretifymem( float value, int digitsafterdecimal /*= 2*/, bool usebinaryonek /*= false*/ )
{
	static char output[ NUM_PRETIFYMEM_BUFFERS ][ 32 ];
	static int  current;

	float		onekb = usebinaryonek ? 1024.0f : 1000.0f;
	float		onemb = onekb * onekb;

	char *out = output[ current ];
	current = ( current + 1 ) & ( NUM_PRETIFYMEM_BUFFERS -1 );

	char suffix[ 8 ];

	// First figure out which bin to use
	if ( value > onemb )
	{
		value /= onemb;
		V_snprintf( suffix, sizeof( suffix ), " MB" );
	}
	else if ( value > onekb )
	{
		value /= onekb;
		V_snprintf( suffix, sizeof( suffix ), " KB" );
	}
	else
	{
		V_snprintf( suffix, sizeof( suffix ), " bytes" );
	}

	char val[ 32 ];

	// Clamp to >= 0
	digitsafterdecimal = MAX( digitsafterdecimal, 0 );

	// If it's basically integral, don't do any decimals
	if ( FloatMakePositive( value - (int)value ) < 0.00001 )
	{
		V_snprintf( val, sizeof( val ), "%i%s", (int)value, suffix );
	}
	else
	{
		char fmt[ 32 ];

		// Otherwise, create a format string for the decimals
		V_snprintf( fmt, sizeof( fmt ), "%%.%if%s", digitsafterdecimal, suffix );
		V_snprintf( val, sizeof( val ), fmt, value );
	}

	// Copy from in to out
	char *i = val;
	char *o = out;

	// Search for decimal or if it was integral, find the space after the raw number
	char *dot = strstr( i, "." );
	if ( !dot )
	{
		dot = strstr( i, " " );
	}

	// Compute position of dot
	int pos = dot - i;
	// Don't put a comma if it's <= 3 long
	pos -= 3;

	while ( *i )
	{
		// If pos is still valid then insert a comma every third digit, except if we would be
		//  putting one in the first spot
		if ( pos >= 0 && !( pos % 3 ) )
		{
			// Never in first spot
			if ( o != out )
			{
				*o++ = ',';
			}
		}

		// Count down comma position
		pos--;

		// Copy rest of data as normal
		*o++ = *i++;
	}

	// Terminate
	*o = 0;

	return out;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string representation of an integer with commas
//			separating the 1000s (ie, 37,426,421)
// Input  : value -		Value to convert
// Output : Pointer to a static buffer containing the output
//-----------------------------------------------------------------------------
#define NUM_PRETIFYNUM_BUFFERS 8
char *V_pretifynum( int64 value )
{
	static char output[ NUM_PRETIFYMEM_BUFFERS ][ 32 ];
	static int  current;

	char *out = output[ current ];
	current = ( current + 1 ) & ( NUM_PRETIFYMEM_BUFFERS -1 );

	*out = 0;

	// Render the leading -, if necessary
	if ( value < 0 )
	{
		char *pchRender = out + V_strlen( out );
		V_snprintf( pchRender, 32, "-" );
		value = -value;
	}

	// Render quadrillions
	if ( value >= 1000000000000ll )
	{
		char *pchRender = out + V_strlen( out );
		V_snprintf( pchRender, 32, "%d,", ( int ) ( value / 1000000000000ll ) );
	}

	// Render trillions
	if ( value >= 1000000000000ll )
	{
		char *pchRender = out + V_strlen( out );
		V_snprintf( pchRender, 32, "%d,", ( int ) ( value / 1000000000000ll ) );
	}

	// Render billions
	if ( value >= 1000000000 )
	{
		char *pchRender = out + V_strlen( out );
		V_snprintf( pchRender, 32, "%d,", ( int ) ( value / 1000000000 ) );
	}

	// Render millions
	if ( value >= 1000000 )
	{
		char *pchRender = out + V_strlen( out );
		if ( value >= 1000000000 )
			V_snprintf( pchRender, 32, "%03d,", ( int ) ( value / 1000000 ) % 1000 );
		else
			V_snprintf( pchRender, 32, "%d,", ( int ) ( value / 1000000 ) % 1000 );
	}

	// Render thousands
	if ( value >= 1000 )
	{
		char *pchRender = out + V_strlen( out );
		if ( value >= 1000000 )
			V_snprintf( pchRender, 32, "%03d,", ( int ) ( value / 1000 ) % 1000 );
		else
			V_snprintf( pchRender, 32, "%d,", ( int ) ( value / 1000 ) % 1000 );
	}

	// Render units
	char *pchRender = out + V_strlen( out );
	if ( value > 1000 )
		V_snprintf( pchRender, 32, "%03d", ( int ) ( value % 1000 ) );
	else
		V_snprintf( pchRender, 32, "%d", ( int ) ( value % 1000 ) );

	return out;
}



//-----------------------------------------------------------------------------
// Purpose: Returns the 4 bit nibble for a hex character
// Input  : c - 
// Output : unsigned char
//-----------------------------------------------------------------------------
static unsigned char V_nibble( char c )
{
	if ( ( c >= '0' ) &&
		 ( c <= '9' ) )
	{
		 return (unsigned char)(c - '0');
	}

	if ( ( c >= 'A' ) &&
		 ( c <= 'F' ) )
	{
		 return (unsigned char)(c - 'A' + 0x0a);
	}

	if ( ( c >= 'a' ) &&
		 ( c <= 'f' ) )
	{
		 return (unsigned char)(c - 'a' + 0x0a);
	}

	return '0';
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *in - 
//			numchars - 
//			*out - 
//			maxoutputbytes - 
//-----------------------------------------------------------------------------
void V_hextobinary( char const *in, int numchars, byte *out, int maxoutputbytes )
{
	int len = V_strlen( in );
	numchars = MIN( len, numchars );
	// Make sure it's even
	numchars = ( numchars ) & ~0x1;

	// Must be an even # of input characters (two chars per output byte)
	Assert( numchars >= 2 );

	memset( out, 0x00, maxoutputbytes );

	byte *p;
	int i;

	p = out;
	for ( i = 0; 
		 ( i < numchars ) && ( ( p - out ) < maxoutputbytes ); 
		 i+=2, p++ )
	{
		*p = ( V_nibble( in[i] ) << 4 ) | V_nibble( in[i+1] );		
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *in - 
//			inputbytes - 
//			*out - 
//			outsize - 
//-----------------------------------------------------------------------------
void V_binarytohex( const byte *in, int inputbytes, char *out, int outsize )
{
	Assert( outsize >= 1 );
	char doublet[10];
	int i;

	out[0]=0;

	for ( i = 0; i < inputbytes; i++ )
	{
		unsigned char c = in[i];
		V_snprintf( doublet, sizeof( doublet ), "%02x", c );
		V_strncat( out, doublet, outsize, COPY_ALL_CHARACTERS );
	}
}

#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')


//-----------------------------------------------------------------------------
// Purpose: Extracts the base name of a file (no path, no extension, assumes '/' or '\' as path separator)
// Input  : *in - 
//			*out - 
//			maxlen - 
//-----------------------------------------------------------------------------
void V_FileBase( const char *in, char *out, int maxlen )
{
	Assert( maxlen >= 1 );
	Assert( in );
	Assert( out );

	if ( !in || !in[ 0 ] )
	{
		*out = 0;
		return;
	}

	int len, start, end;

	len = V_strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end&& in[end] != '.' && !PATHSEPARATOR( in[end] ) )
	{
		end--;
	}
	
	if ( in[end] != '.' )		// no '.', copy to end
	{
		end = len-1;
	}
	else 
	{
		end--;					// Found ',', copy to left of '.'
	}

	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && !PATHSEPARATOR( in[start] ) )
	{
		start--;
	}

	if ( start < 0 || !PATHSEPARATOR( in[start] ) )
	{
		start = 0;
	}
	else 
	{
		start++;
	}

	// Length of new sting
	len = end - start + 1;

	int maxcopy = MIN( len + 1, maxlen );

	// Copy partial string
	V_strncpy( out, &in[start], maxcopy );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ppath - 
//-----------------------------------------------------------------------------
void V_StripTrailingSlash( char *ppath )
{
	Assert( ppath );

	int len = V_strlen( ppath );
	if ( len > 0 )
	{
		if ( PATHSEPARATOR( ppath[ len - 1 ] ) )
		{
			ppath[ len - 1 ] = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *in - 
//			*out - 
//			outSize - 
//-----------------------------------------------------------------------------
void V_StripExtension( const char *in, char *out, int outSize )
{
	// Find the last dot. If it's followed by a dot or a slash, then it's part of a 
	// directory specifier like ../../somedir/./blah.

	// scan backward for '.'
	int end = V_strlen( in ) - 1;
	while ( end > 0 && in[end] != '.' && !PATHSEPARATOR( in[end] ) )
	{
		--end;
	}

	if (end > 0 && !PATHSEPARATOR( in[end] ) && end < outSize)
	{
		int nChars = MIN( end, outSize-1 );
		if ( out != in )
		{
			memcpy( out, in, nChars );
		}
		out[nChars] = 0;
	}
	else
	{
		// nothing found
		if ( out != in )
		{
			V_strncpy( out, in, outSize );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*extension - 
//			pathStringLength - 
//-----------------------------------------------------------------------------
void V_DefaultExtension( char *path, const char *extension, int pathStringLength )
{
	Assert( path );
	Assert( pathStringLength >= 1 );
	Assert( extension );

	char    *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + V_strlen(path) - 1;

	while ( !PATHSEPARATOR( *src ) && ( src > path ) )
	{
		if (*src == '.')
		{
			// it has an extension
			return;                 
		}
		src--;
	}

	// Concatenate the desired extension
	char pTemp[MAX_PATH];
	if ( extension[0] != '.' )
	{
		pTemp[0] = '.';
		V_strncpy( &pTemp[1], extension, sizeof(pTemp) - 1 );
		extension = pTemp;
	}
	V_strncat( path, extension, pathStringLength, COPY_ALL_CHARACTERS );
}

//-----------------------------------------------------------------------------
// Purpose: Force extension...
// Input  : *path - 
//			*extension - 
//			pathStringLength - 
//-----------------------------------------------------------------------------
void V_SetExtension( char *path, const char *extension, int pathStringLength )
{
	V_StripExtension( path, path, pathStringLength );
	V_DefaultExtension( path, extension, pathStringLength );
}

//-----------------------------------------------------------------------------
// Purpose: Remove final filename from string
// Input  : *path - 
// Output : void  V_StripFilename
//-----------------------------------------------------------------------------
void  V_StripFilename (char *path)
{
	int             length;

	length = V_strlen( path )-1;
	if ( length <= 0 )
		return;

	while ( length > 0 && 
		!PATHSEPARATOR( path[length] ) )
	{
		length--;
	}

	path[ length ] = 0;
}

#ifdef _WIN32
#define CORRECT_PATH_SEPARATOR '\\'
#define INCORRECT_PATH_SEPARATOR '/'
#elif POSIX
#define CORRECT_PATH_SEPARATOR '/'
#define INCORRECT_PATH_SEPARATOR '\\'
#endif

//-----------------------------------------------------------------------------
// Purpose: Changes all '/' or '\' characters into separator
// Input  : *pname - 
//			separator - 
//-----------------------------------------------------------------------------
void V_FixSlashes( char *pname, char separator /* = CORRECT_PATH_SEPARATOR */ )
{
	while ( *pname )
	{
		if ( *pname == INCORRECT_PATH_SEPARATOR || *pname == CORRECT_PATH_SEPARATOR )
		{
			*pname = separator;
		}
		pname++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function fixes cases of filenames like materials\\blah.vmt or somepath\otherpath\\ and removes the extra double slash.
//-----------------------------------------------------------------------------
void V_FixDoubleSlashes( char *pStr )
{
	int len = V_strlen( pStr );

	for ( int i=1; i < len-1; i++ )
	{
		if ( (pStr[i] == '/' || pStr[i] == '\\') && (pStr[i+1] == '/' || pStr[i+1] == '\\') )
		{
			// This means there's a double slash somewhere past the start of the filename. That 
			// can happen in Hammer if they use a material in the root directory. You'll get a filename 
			// that looks like 'materials\\blah.vmt'
			V_memmove( &pStr[i], &pStr[i+1], len - i );
			--len;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Strip off the last directory from dirName
// Input  : *dirName - 
//			maxlen - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool V_StripLastDir( char *dirName, int maxlen )
{
	if( dirName[0] == 0 || 
		!V_stricmp( dirName, "./" ) || 
		!V_stricmp( dirName, ".\\" ) )
		return false;
	
	int len = V_strlen( dirName );

	Assert( len < maxlen );

	// skip trailing slash
	if ( PATHSEPARATOR( dirName[len-1] ) )
	{
		len--;
	}

	bool bHitColon = false;
	while ( len > 0 )
	{
		if ( PATHSEPARATOR( dirName[len-1] ) )
		{
			dirName[len] = 0;
			V_FixSlashes( dirName, CORRECT_PATH_SEPARATOR );
			return true;
		}
		else if ( dirName[len-1] == ':' )
		{
			bHitColon = true;
		}

		len--;
	}

	// If we hit a drive letter, then we're done.
	// Ex: If they passed in c:\, then V_StripLastDir should return "" and false.
	if ( bHitColon )
	{
		dirName[0] = 0;
		return false;
	}

	// Allow it to return an empty string and true. This can happen if something like "tf2/" is passed in.
	// The correct behavior is to strip off the last directory ("tf2") and return true.
	if ( len == 0 && !bHitColon )
	{
		V_snprintf( dirName, maxlen, ".%c", CORRECT_PATH_SEPARATOR );
		return true;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the beginning of the unqualified file name 
//			(no path information)
// Input:	in - file name (may be unqualified, relative or absolute path)
// Output:	pointer to unqualified file name
//-----------------------------------------------------------------------------
const char * V_UnqualifiedFileName( const char * in )
{
	if ( !in || !in[0] )
		return in;

	// back up until the character after the first path separator we find,
	// or the beginning of the string
	const char * out = in + strlen( in ) - 1;
	while ( ( out > in ) && ( !PATHSEPARATOR( *( out-1 ) ) ) )
		out--;
	return out;
}

char *V_UnqualifiedFileName( char *in )
{
	return const_cast<char *>( V_UnqualifiedFileName( const_cast<const char *>(in) ) );
}


//-----------------------------------------------------------------------------
// Purpose: Composes a path and filename together, inserting a path separator
//			if need be
// Input:	path - path to use
//			filename - filename to use
//			dest - buffer to compose result in
//			destSize - size of destination buffer
//-----------------------------------------------------------------------------
void V_ComposeFileName( const char *path, const char *filename, char *dest, int destSize )
{
	V_strncpy( dest, path, destSize );
	V_FixSlashes( dest );
	V_AppendSlash( dest, destSize );
	V_strncat( dest, filename, destSize, COPY_ALL_CHARACTERS );
	V_FixSlashes( dest );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*dest - 
//			destSize - 
// Output : void V_ExtractFilePath
//-----------------------------------------------------------------------------
bool V_ExtractFilePath (const char *path, char *dest, int destSize )
{
	Assert( destSize >= 1 );
	if ( destSize < 1 )
	{
		return false;
	}

	// Last char
	int len = V_strlen(path);
	const char *src = path + (len ? len-1 : 0);

	// back up until a \ or the start
	while ( src != path && !PATHSEPARATOR( *(src-1) ) )
	{
		src--;
	}

	int copysize = MIN( src - path, destSize - 1 );
	memcpy( dest, path, copysize );
	dest[copysize] = 0;

	return copysize != 0 ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*dest - 
//			destSize - 
// Output : void V_ExtractFileExtension
//-----------------------------------------------------------------------------
void V_ExtractFileExtension( const char *path, char *dest, int destSize )
{
	*dest = 0;
	const char * extension = V_GetFileExtension( path );
	if ( NULL != extension )
		V_strncpy( dest, extension, destSize );
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the file extension within a file name string
// Input:	in - file name 
// Output:	pointer to beginning of extension (after the "."), or NULL
//				if there is no extension
//-----------------------------------------------------------------------------
const char * V_GetFileExtension( const char * path )
{
	const char    *src;

	src = path + strlen(path) - 1;

//
// back up until a . or the start
//
	while (src != path && !PATHSEPARATOR( *src ) && *(src-1) != '.'  )
		src--;

	// check to see if the '.' is part of a pathname
	if (src == path || PATHSEPARATOR( *src ) )
	{		
		return NULL;  // no extension
	}

	return src;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the filename part of a path string
// Input:	in - file name 
// Output:	pointer to beginning of filename (after the "/"). If there were no /, 
//          output is identical to input
//-----------------------------------------------------------------------------
const char *V_GetFileName( const char *pPath )
{
	if ( !pPath || !pPath[0] )
		return pPath;

	const char *pSrc;
	pSrc = pPath + strlen( pPath ) - 1;

	// back up until a / or the start
	while ( pSrc > pPath && !PATHSEPARATOR( *( pSrc-1 ) ) )
		--pSrc;

	return pSrc;
}

bool V_RemoveDotSlashes( char *pFilename, char separator )
{
	// Remove '//' or '\\'
	char *pIn = pFilename;
	char *pOut = pFilename;
	bool bPrevPathSep = false;
	while ( *pIn )
	{
		bool bIsPathSep = PATHSEPARATOR( *pIn );
		if ( !bIsPathSep || !bPrevPathSep )
		{
			*pOut++ = *pIn;
		}
		bPrevPathSep = bIsPathSep;
		++pIn;
	}
	*pOut = 0;

	// Get rid of "./"'s
	pIn = pFilename;
	pOut = pFilename;
	while ( *pIn )
	{
		// The logic on the second line is preventing it from screwing up "../"
		if ( pIn[0] == '.' && PATHSEPARATOR( pIn[1] ) &&
			(pIn == pFilename || pIn[-1] != '.') )
		{
			pIn += 2;
		}
		else
		{
			*pOut = *pIn;
			++pIn;
			++pOut;
		}
	}
	*pOut = 0;

	// Get rid of a trailing "/." (needless).
	int len = strlen( pFilename );
	if ( len > 2 && pFilename[len-1] == '.' && PATHSEPARATOR( pFilename[len-2] ) )
	{
		pFilename[len-2] = 0;
	}

	// Each time we encounter a "..", back up until we've read the previous directory name,
	// then get rid of it.
	pIn = pFilename;
	while ( *pIn )
	{
		if ( pIn[0] == '.' && 
			 pIn[1] == '.' && 
			 (pIn == pFilename || PATHSEPARATOR(pIn[-1])) &&	// Preceding character must be a slash.
			 (pIn[2] == 0 || PATHSEPARATOR(pIn[2])) )			// Following character must be a slash or the end of the string.
		{
			char *pEndOfDots = pIn + 2;
			char *pStart = pIn - 2;

			// Ok, now scan back for the path separator that starts the preceding directory.
			while ( 1 )
			{
				if ( pStart < pFilename )
					return false;

				if ( PATHSEPARATOR( *pStart ) )
					break;

				--pStart;
			}

			// Now slide the string down to get rid of the previous directory and the ".."
			memmove( pStart, pEndOfDots, strlen( pEndOfDots ) + 1 );

			// Start over.
			pIn = pFilename;
		}
		else
		{
			++pIn;
		}
	}
	
	V_FixSlashes( pFilename, separator );	
	return true;
}


void V_AppendSlash( char *pStr, int strSize )
{
	int len = V_strlen( pStr );
	if ( len > 0 && !PATHSEPARATOR(pStr[len-1]) )
	{
		if ( len+1 >= strSize )
			Error( "V_AppendSlash: ran out of space on %s.", pStr );
		
		pStr[len] = CORRECT_PATH_SEPARATOR;
		pStr[len+1] = 0;
	}
}


void V_MakeAbsolutePath( char *pOut, int outLen, const char *pPath, const char *pStartingDir )
{
	if ( V_IsAbsolutePath( pPath ) )
	{
		// pPath is not relative.. just copy it.
		V_strncpy( pOut, pPath, outLen );
	}
	else
	{
		// Make sure the starting directory is absolute..
		if ( pStartingDir && V_IsAbsolutePath( pStartingDir ) )
		{
			V_strncpy( pOut, pStartingDir, outLen );
		}
		else
		{
#ifdef _PS3 
			{
				V_strncpy( pOut, g_pPS3PathInfo->GameImagePath(), outLen );
			}
#else
			{
				if ( !_getcwd( pOut, outLen ) )
					Error( "V_MakeAbsolutePath: _getcwd failed." );
			}
#endif

			if ( pStartingDir )
			{
				V_AppendSlash( pOut, outLen );
				V_strncat( pOut, pStartingDir, outLen, COPY_ALL_CHARACTERS );
			}
		}

		// Concatenate the paths.
		V_AppendSlash( pOut, outLen );
		V_strncat( pOut, pPath, outLen, COPY_ALL_CHARACTERS );
	}

	if ( !V_RemoveDotSlashes( pOut ) )
		Error( "V_MakeAbsolutePath: tried to \"..\" past the root." );

	V_FixSlashes( pOut );
}


//-----------------------------------------------------------------------------
// Makes a relative path
//-----------------------------------------------------------------------------
bool V_MakeRelativePath( const char *pFullPath, const char *pDirectory, char *pRelativePath, int nBufLen )
{
	pRelativePath[0] = 0;

	const char *pPath = pFullPath;
	const char *pDir = pDirectory;

	// Strip out common parts of the path
	const char *pLastCommonPath = NULL;
	const char *pLastCommonDir = NULL;
	while ( *pPath && ( tolower( *pPath ) == tolower( *pDir ) || 
						( PATHSEPARATOR( *pPath ) && ( PATHSEPARATOR( *pDir ) || (*pDir == 0) ) ) ) )
	{
		if ( PATHSEPARATOR( *pPath ) )
		{
			pLastCommonPath = pPath + 1;
			pLastCommonDir = pDir + 1;
		}
		if ( *pDir == 0 )
		{
			--pLastCommonDir;
			break;
		}
		++pDir; ++pPath;
	}

	// Nothing in common
	if ( !pLastCommonPath )
		return false;

	// For each path separator remaining in the dir, need a ../
	int nOutLen = 0;
	bool bLastCharWasSeparator = true;
	for ( ; *pLastCommonDir; ++pLastCommonDir )
	{
		if ( PATHSEPARATOR( *pLastCommonDir ) )
		{
			pRelativePath[nOutLen++] = '.';
			pRelativePath[nOutLen++] = '.';
			pRelativePath[nOutLen++] = CORRECT_PATH_SEPARATOR;
			bLastCharWasSeparator = true;
		}
		else
		{
			bLastCharWasSeparator = false;
		}
	}

	// Deal with relative paths not specified with a trailing slash
	if ( !bLastCharWasSeparator )
	{
		pRelativePath[nOutLen++] = '.';
		pRelativePath[nOutLen++] = '.';
		pRelativePath[nOutLen++] = CORRECT_PATH_SEPARATOR;
	}

	// Copy the remaining part of the relative path over, fixing the path separators
	for ( ; *pLastCommonPath; ++pLastCommonPath )
	{
		if ( PATHSEPARATOR( *pLastCommonPath ) )
		{
			pRelativePath[nOutLen++] = CORRECT_PATH_SEPARATOR;
		}
		else
		{
			pRelativePath[nOutLen++] = *pLastCommonPath;
		}

		// Check for overflow
		if ( nOutLen == nBufLen - 1 )
			break;
	}

	pRelativePath[nOutLen] = 0;
	return true;
}


//-----------------------------------------------------------------------------
// small helper function shared by lots of modules
//-----------------------------------------------------------------------------
bool V_IsAbsolutePath( const char *pStr )
{
	bool bIsAbsolute = ( pStr[0] && pStr[1] == ':' ) || pStr[0] == '/' || pStr[0] == '\\';
	if ( IsX360() && !bIsAbsolute )
	{
		bIsAbsolute = ( V_stristr( pStr, ":" ) != NULL );
	}
	return bIsAbsolute;
}


//-----------------------------------------------------------------------------
// Fixes up a file name, removing dot slashes, fixing slashes, converting to lowercase, etc.
//-----------------------------------------------------------------------------
void V_FixupPathName( char *pOut, size_t nOutLen, const char *pPath )
{
	V_strncpy( pOut, pPath, nOutLen );
	V_FixSlashes( pOut );
	V_RemoveDotSlashes( pOut );
	V_FixDoubleSlashes( pOut );
	V_strlower( pOut );
}


// Copies at most nCharsToCopy bytes from pIn into pOut.
// Returns false if it would have overflowed pOut's buffer.
static bool CopyToMaxChars( char *pOut, int outSize, const char *pIn, int nCharsToCopy )
{
	if ( outSize == 0 )
		return false;

	int iOut = 0;
	while ( *pIn && nCharsToCopy > 0 )
	{
		if ( iOut == (outSize-1) )
		{
			pOut[iOut] = 0;
			return false;
		}
		pOut[iOut] = *pIn;
		++iOut;
		++pIn;
		--nCharsToCopy;
	}
	
	pOut[iOut] = 0;
	return true;
}


// Returns true if it completed successfully.
// If it would overflow pOut, it fills as much as it can and returns false.
bool V_StrSubst( 
	const char *pIn, 
	const char *pMatch,
	const char *pReplaceWith,
	char *pOut,
	int outLen,
	bool bCaseSensitive
	)
{
	int replaceFromLen = strlen( pMatch );
	int replaceToLen = strlen( pReplaceWith );

	const char *pInStart = pIn;
	char *pOutPos = pOut;
	pOutPos[0] = 0;

	while ( 1 )
	{
		int nRemainingOut = outLen - (pOutPos - pOut);

		const char *pTestPos = ( bCaseSensitive ? strstr( pInStart, pMatch ) : V_stristr( pInStart, pMatch ) );
		if ( pTestPos )
		{
			// Found an occurence of pMatch. First, copy whatever leads up to the string.
			int copyLen = pTestPos - pInStart;
			if ( !CopyToMaxChars( pOutPos, nRemainingOut, pInStart, copyLen ) )
				return false;
			
			// Did we hit the end of the output string?
			if ( copyLen > nRemainingOut-1 )
				return false;

			pOutPos += strlen( pOutPos );
			nRemainingOut = outLen - (pOutPos - pOut);

			// Now add the replacement string.
			if ( !CopyToMaxChars( pOutPos, nRemainingOut, pReplaceWith, replaceToLen ) )
				return false;

			pInStart += copyLen + replaceFromLen;
			pOutPos += replaceToLen;			
		}
		else
		{
			// We're at the end of pIn. Copy whatever remains and get out.
			int copyLen = strlen( pInStart );
			V_strncpy( pOutPos, pInStart, nRemainingOut );
			return ( copyLen <= nRemainingOut-1 );
		}
	}
}


char* AllocString( const char *pStr, int nMaxChars )
{
	int allocLen;
	if ( nMaxChars == -1 )
		allocLen = strlen( pStr ) + 1;
	else
		allocLen = MIN( (int)strlen(pStr), nMaxChars ) + 1;

	char *pOut = new char[allocLen];
	V_strncpy( pOut, pStr, allocLen );
	return pOut;
}




void V_SplitString2( const char *pString, const char **pSeparators, int nSeparators, CUtlVector<char*> &outStrings )
{
	outStrings.Purge();
	const char *pCurPos = pString;
	while ( 1 )
	{
		int iFirstSeparator = -1;
		const char *pFirstSeparator = 0;
		for ( int i=0; i < nSeparators; i++ )
		{
			const char *pTest = V_stristr( pCurPos, pSeparators[i] );
			if ( pTest && (!pFirstSeparator || pTest < pFirstSeparator) )
			{
				iFirstSeparator = i;
				pFirstSeparator = pTest;
			}
		}

		if ( pFirstSeparator )
		{
			// Split on this separator and continue on.
			int separatorLen = strlen( pSeparators[iFirstSeparator] );
			if ( pFirstSeparator > pCurPos )
			{
				outStrings.AddToTail( AllocString( pCurPos, pFirstSeparator-pCurPos ) );
			}

			pCurPos = pFirstSeparator + separatorLen;
		}
		else
		{
			// Copy the rest of the string
			if ( strlen( pCurPos ) )
			{
				outStrings.AddToTail( AllocString( pCurPos, -1 ) );
			}
			return;
		}
	}
}


void V_SplitString( const char *pString, const char *pSeparator, CUtlVector<char*> &outStrings )
{
	V_SplitString2( pString, &pSeparator, 1, outStrings );
}



bool V_GetCurrentDirectory( char *pOut, int maxLen )
{
#if defined( _PS3 )
	Assert( 0 );
	return false; // not supported
#else // !_PS3
    return _getcwd( pOut, maxLen ) == pOut;
#endif // _PS3
}


bool V_SetCurrentDirectory( const char *pDirName )
{
#if defined( _PS3 )
	Assert( 0 );
	return false; // not supported
#else // !_PS3
    return _chdir( pDirName ) == 0;
#endif // _PS3
}


// This function takes a slice out of pStr and stores it in pOut.
// It follows the Python slice convention:
// Negative numbers wrap around the string (-1 references the last character).
// Numbers are clamped to the end of the string.
void V_StrSlice( const char *pStr, int firstChar, int lastCharNonInclusive, char *pOut, int outSize )
{
	if ( outSize == 0 )
		return;
	
	int length = strlen( pStr );

	// Fixup the string indices.
	if ( firstChar < 0 )
	{
		firstChar = length - (-firstChar % length);
	}
	else if ( firstChar >= length )
	{
		pOut[0] = 0;
		return;
	}

	if ( lastCharNonInclusive < 0 )
	{
		lastCharNonInclusive = length - (-lastCharNonInclusive % length);
	}
	else if ( lastCharNonInclusive > length )
	{
		lastCharNonInclusive %= length;
	}

	if ( lastCharNonInclusive <= firstChar )
	{
		pOut[0] = 0;
		return;
	}

	int copyLen = lastCharNonInclusive - firstChar;
	if ( copyLen <= (outSize-1) )
	{
		memcpy( pOut, &pStr[firstChar], copyLen );
		pOut[copyLen] = 0;
	}
	else
	{
		memcpy( pOut, &pStr[firstChar], outSize-1 );
		pOut[outSize-1] = 0;
	}
}


void V_StrLeft( const char *pStr, int nChars, char *pOut, int outSize )
{
	if ( nChars == 0 )
	{
		if ( outSize != 0 )
			pOut[0] = 0;

		return;
	}

	V_StrSlice( pStr, 0, nChars, pOut, outSize );
}


void V_StrRight( const char *pStr, int nChars, char *pOut, int outSize )
{
	int len = strlen( pStr );
	if ( nChars >= len )
	{
		V_strncpy( pOut, pStr, outSize );
	}
	else
	{
		V_StrSlice( pStr, -nChars, strlen( pStr ), pOut, outSize );
	}
}

//-----------------------------------------------------------------------------
// Convert multibyte to wchar + back
//-----------------------------------------------------------------------------
void V_strtowcs( const char *pString, int nInSize, wchar_t *pWString, int nOutSize )
{
#ifdef _WIN32
	if ( !MultiByteToWideChar( CP_UTF8, 0, pString, nInSize, pWString, nOutSize ) )
	{
		*pWString = L'\0';
	}
#elif POSIX
	if ( mbstowcs( pWString, pString, nOutSize / sizeof(wchar_t) ) <= 0 )
	{
		*pWString = 0;
	}
#endif
}

void V_wcstostr( const wchar_t *pWString, int nInSize, char *pString, int nOutSize )
{
#ifdef _WIN32
	if ( !WideCharToMultiByte( CP_UTF8, 0, pWString, nInSize, pString, nOutSize, NULL, NULL ) )
	{
		*pString = '\0';
	}
#elif POSIX
	if ( wcstombs( pString, pWString, nOutSize ) <= 0 )
	{
		*pString = '\0';
	}
#endif
}



//--------------------------------------------------------------------------------
// backslashification
//--------------------------------------------------------------------------------

static char s_BackSlashMap[]="\tt\nn\rr\"\"\\\\";

char *V_AddBackSlashesToSpecialChars( char const *pSrc )
{
	// first, count how much space we are going to need
	int nSpaceNeeded = 0;
	for( char const *pScan = pSrc; *pScan; pScan++ )
	{
		nSpaceNeeded++;
		for(char const *pCharSet=s_BackSlashMap; *pCharSet; pCharSet += 2 )
		{
			if ( *pCharSet == *pScan )
				nSpaceNeeded++;								// we need to store a bakslash
		}
	}
	char *pRet = new char[ nSpaceNeeded + 1 ];				// +1 for null
	char *pOut = pRet;
	
	for( char const *pScan = pSrc; *pScan; pScan++ )
	{
		bool bIsSpecial = false;
		for(char const *pCharSet=s_BackSlashMap; *pCharSet; pCharSet += 2 )
		{
			if ( *pCharSet == *pScan )
			{
				*( pOut++ ) = '\\';
				*( pOut++ ) = pCharSet[1];
				bIsSpecial = true;
				break;
			}
		}
		if (! bIsSpecial )
		{
			*( pOut++ ) = *pScan;
		}
	}
	*( pOut++ ) = 0;
	return pRet;
}

void V_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	V_strncpy( tempString, pString, sizeof(tempString) );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

void V_StringToColor32( color32 *color, const char *pString )
{
	int tmp[4];
	V_StringToIntArray( tmp, 4, pString );
	color->r = tmp[0];
	color->g = tmp[1];
	color->b = tmp[2];
	color->a = tmp[3];
}



// 3d memory copy
void CopyMemory3D( void *pDest, void const *pSrc,		
				   int nNumCols, int nNumRows, int nNumSlices, // dimensions of copy
				   int nSrcBytesPerRow, int nSrcBytesPerSlice, // strides for source.
				   int nDestBytesPerRow, int nDestBytesPerSlice // strides for dest
	)
{
	if ( nNumSlices && nNumRows && nNumCols )
	{
		uint8 *pDestAdr = reinterpret_cast<uint8 *>( pDest );
		uint8 const *pSrcAdr = reinterpret_cast<uint8 const *>( pSrc );
		// first check for optimized cases
		if ( ( nNumCols == nSrcBytesPerRow ) && ( nNumCols == nDestBytesPerRow ) ) // no row-to-row stride?
		{
			int n2DSize = nNumCols * nNumRows;
			if ( nSrcBytesPerSlice == nDestBytesPerSlice  ) // can we do one memcpy?
			{
				memcpy( pDestAdr, pSrcAdr, n2DSize * nNumSlices );
			}
			else
			{
				// there might be some slice-to-slice stride
				do
				{
					memcpy( pDestAdr, pSrcAdr, n2DSize );
					pDestAdr += nDestBytesPerSlice;
					pSrcAdr += nSrcBytesPerSlice;
				} while( nNumSlices-- );
			}
		}
		else
		{
			// there is row-by-row stride - we have to do the full nested loop
			do
			{
				int nRowCtr = nNumRows;
				uint8 const *pSrcRow = pSrcAdr;
				uint8 *pDestRow = pDestAdr;
				do
				{
					memcpy( pDestRow, pSrcRow, nNumCols );
					pDestRow += nDestBytesPerRow;
					pSrcRow += nSrcBytesPerRow;
				} while( --nRowCtr );
				pSrcAdr += nSrcBytesPerSlice;
				pDestAdr += nDestBytesPerSlice;
			} while( nNumSlices-- );
		}
	}
}

void V_TranslateLineFeedsToUnix( char *pStr )
{
	char *pIn = pStr;
	char *pOut = pStr;
	while ( *pIn )
	{
		if ( pIn[0] == '\r' && pIn[1] == '\n' )
		{
			++pIn;
		}
		*pOut++ = *pIn++;
	}
	*pOut = 0;
}

static char s_hex[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

int HexToValue( char hex )
{
	if( hex >= '0' && hex <= '9' )
	{
		return hex - '0';
	}
	if( hex >= 'A' && hex <= 'F' )
	{
		return hex - 'A' + 10;
	}
	if( hex >= 'a' && hex <= 'f' )
	{
		return hex - 'a' + 10;
	}
	// report error here
	return -1;
}

bool V_StringToBin( const char*pString, void *pBin, uint nBinSize )
{
	if ( (uint)V_strlen( pString ) != nBinSize * 2 )
	{
		return false;
	}

	for ( uint i = 0; i < nBinSize; ++i )
	{
		int high = HexToValue( pString[i*2+0] );
		int low  = HexToValue( pString[i*2+1] ) ;
		if( high < 0 || low < 0 )
		{
			return false;
		}

		( ( uint8* )pBin )[i] = uint8( ( high << 4 ) | low );
	}
	return true;
}


bool V_BinToString( char*pString, void *pBin, uint nBinSize )
{
	for ( uint i = 0; i < nBinSize; ++i )
	{
		pString[i*2+0] = s_hex[( ( uint8* )pBin )[i] >> 4 ];
		pString[i*2+1] = s_hex[( ( uint8* )pBin )[i] & 0xF];
	}
	pString[nBinSize*2] = '\0';
	return true;
}

// The following characters are not allowed to begin a line for Asian language line-breaking
// purposes.  They include the right parenthesis/bracket, space character, period, exclamation, 
// question mark, and a number of language-specific characters for Chinese, Japanese, and Korean
static const wchar_t wszCantBeginLine[] =
{
	0x0020, 0x0021, 0x0025, 0x0029,	0x002c, 0x002e, 0x003a, 0x003b,
	0x003e, 0x003f, 0x005d, 0x007d,	0x00a2, 0x00a8, 0x00b0, 0x00b7, 
	0x00bb,	0x02c7, 0x02c9,	0x2010, 0x2013, 0x2014, 0x2015,	0x2016, 
	0x2019, 0x201d, 0x201e,	0x201f, 0x2020, 0x2021, 0x2022,	0x2025, 
	0x2026, 0x2027, 0x203a, 0x203c,	0x2047, 0x2048, 0x2049, 0x2103,
	0x2236, 0x2574, 0x3001, 0x3002,	0x3003, 0x3005, 0x3006, 0x3009,
	0x300b, 0x300d, 0x300f, 0x3011,	0x3015, 0x3017, 0x3019, 0x301b,
	0x301c,	0x301e, 0x301f, 0x303b, 0x3041, 0x3043, 0x3045, 0x3047, 
	0x3049,	0x3063, 0x3083, 0x3085, 0x3087,	0x308e, 0x3095, 0x3096, 
	0x30a0,	0x30a1, 0x30a3, 0x30a5, 0x30a7,	0x30a9, 0x30c3, 0x30e3, 
	0x30e5,	0x30e7, 0x30ee, 0x30f5, 0x30f6,	0x30fb, 0x30fd, 0x30fe, 
	0x30fc,	0x31f0, 0x31f1, 0x31f2, 0x31f3,	0x31f4, 0x31f5, 0x31f6, 
	0x31f7,	0x31f8, 0x31f9, 0x31fa, 0x31fb,	0x31fc, 0x31fd, 0x31fe, 
	0x31ff,	0xfe30, 0xfe31, 0xfe32, 0xfe33,	0xfe36, 0xfe38, 0xfe3a,	
	0xfe3c, 0xfe3e, 0xfe40, 0xfe42, 0xfe44,	0xfe4f, 0xfe50, 0xfe51, 
	0xfe52,	0xfe53, 0xfe54, 0xfe55, 0xfe56,	0xfe57, 0xfe58, 0xfe5a, 
	0xfe5c, 0xfe5e, 0xff01,	0xff02, 0xff05, 0xff07, 0xff09,	0xff0c, 
	0xff0e, 0xff1a, 0xff1b,	0xff1f, 0xff3d, 0xff40, 0xff5c,	0xff5d, 
	0xff5e, 0xff60, 0xff64
};

// The following characters are not allowed to end a line for Asian Language line-breaking
// purposes.  They include left parenthesis/bracket, currency symbols, and an number
// of language-specific characters for Chinese, Japanese, and Korean
static const wchar_t wszCantEndLine[] =
{
	0x0024, 0x0028, 0x002a, 0x003c, 0x005b, 0x005c, 0x007b, 0x00a3,	
	0x00a5, 0x00ab, 0x00ac, 0x00b7, 0x02c6, 0x2018,	0x201c, 0x201f, 
	0x2035, 0x2039, 0x3005, 0x3007,	0x3008, 0x300a, 0x300c, 0x300e, 
	0x3010,	0x3014, 0x3016, 0x3018, 0x301a, 0x301d, 0xfe34, 0xfe35, 
	0xfe37, 0xfe39, 0xfe3b, 0xfe3d, 0xfe3f,	0xfe41, 0xfe43, 0xfe59, 
	0xfe5b,	0xfe5d, 0xff04, 0xff08, 0xff0e,	0xff3b, 0xff5b, 0xff5f, 
	0xffe1,	0xffe5, 0xffe6
};

// Can't break between some repeated punctuation patterns ("--", "...", "<asian period repeated>")
static const wchar_t wszCantBreakRepeated[] =
{
	0x002d, 0x002e, 0x3002
};

bool AsianWordWrap::CanEndLine( wchar_t wcCandidate )
{
	for( int i = 0; i < SIZE_OF_ARRAY( wszCantEndLine ); ++i )
	{
		if( wcCandidate == wszCantEndLine[i] )
			return false;
	}

	return true;
}

bool AsianWordWrap::CanBeginLine( wchar_t wcCandidate )
{
	for( int i = 0; i < SIZE_OF_ARRAY( wszCantBeginLine ); ++i )
	{
		if( wcCandidate == wszCantBeginLine[i] )
			return false;
	}

	return true;
}

bool AsianWordWrap::CanBreakRepeated( wchar_t wcCandidate )
{
	for( int i = 0; i < SIZE_OF_ARRAY( wszCantBreakRepeated ); ++i )
	{
		if( wcCandidate == wszCantBreakRepeated[i] )
			return false;
	}

	return true;
}

#if defined( _PS3 ) || defined( LINUX )
inline int __cdecl iswascii(wchar_t c) { return ((unsigned)(c) < 0x80); } // not defined in wctype.h on the PS3
#endif

// Used to determine if we can break a line between the first two characters passed
bool AsianWordWrap::CanBreakAfter( const wchar_t* wsz )
{
	if( wsz == NULL || wsz[0] == '\0' || wsz[1] == '\0' )
	{
		return false;
	}

	wchar_t first_char = wsz[0];
	wchar_t second_char = wsz[1];
 	if( ( iswascii( first_char ) && iswascii( second_char ) ) // If not both CJK, return early
 		|| ( iswalnum( first_char ) && iswalnum( second_char ) ) ) // both characters are alphanumeric - Don't split a number or a word!
	{
		return false;
	}

	if( !CanEndLine( first_char ) )
	{
		return false;
	}

	if( !CanBeginLine( second_char) )
	{
		return false;
	}

	// don't allow line wrapping in the middle of "--" or "..."
	if( ( first_char == second_char ) && ( !CanBreakRepeated( first_char ) ) )
	{
		return false;
	}

	// If no rules would prevent us from breaking, assume it's safe to break here
	return true;
}

// We use this function to determine where it is permissible to break lines
// of text while wrapping them. On some platforms, the native iswspace() function
// returns FALSE for the "non-breaking space" characters 0x00a0 and 0x202f, and so we don't
// break on them. On others (including the X360 and PC), iswspace returns TRUE for them.
// We get rid of the platform dependency by defining this wrapper which returns false
// for &nbsp; and calls through to the library function for everything else.
int isbreakablewspace( wchar_t ch )
{
	// 0x00a0 and 0x202f are the wide and narrow non-breaking space UTF-16 values, respectively
	return ch != 0x00a0 && ch != 0x202f && iswspace(ch);
}
