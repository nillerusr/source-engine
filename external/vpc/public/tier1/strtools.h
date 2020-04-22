//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef TIER1_STRTOOLS_H
#define TIER1_STRTOOLS_H

#include "tier0/basetypes.h"

#ifdef _WIN32
#pragma once
#elif POSIX
#include <ctype.h>
#include <wchar.h>
#include <math.h>
#include <wctype.h>
#endif

#include <string.h>
#include <stdlib.h>


// 3d memcpy. Copy (up-to) 3 dimensional data with arbitrary source and destination
// strides. Optimizes to just a single memcpy when possible. For 2d data, set numslices to 1.
void CopyMemory3D( void *pDestAdr, void const *pSrcAdr,		
				   int nNumCols, int nNumRows, int nNumSlices, // dimensions of copy
				   int nSrcBytesPerRow, int nSrcBytesPerSlice, // strides for source.
				   int nDestBytesPerRow, int nDestBytesPerSlice // strides for dest
	);

	


template< class T, class I > class CUtlMemory;
template< class T, class A > class CUtlVector;


//-----------------------------------------------------------------------------
// Portable versions of standard string functions
//-----------------------------------------------------------------------------
void	_V_memset	( void *dest, int fill, int count );
void	_V_memcpy	( void *dest, const void *src, int count );
void	_V_memmove	( void *dest, const void *src, int count );
int		_V_memcmp	( const void *m1, const void *m2, int count );
int		_V_strlen	( const char *str );
void	_V_strcpy	( char *dest, const char *src );
char*	_V_strrchr	( const char *s, char c );
int		_V_strcmp	( const char *s1, const char *s2 );
int		_V_wcscmp	( const wchar_t *s1, const wchar_t *s2 );
int		_V_stricmp	( const char *s1, const char *s2 );
char*	_V_strstr	( const char *s1, const char *search );
char*	_V_strupr	( char *start );
char*	_V_strlower	( char *start );
int		_V_wcslen	( const wchar_t *pwch );

wchar_t*	_V_wcslower (const char* file, int line, wchar_t *start);
wchar_t*	_V_wcsupr (const char* file, int line, wchar_t *start);

#ifdef POSIX
inline char *strupr( char *start )
{
      char *str = start;
      while( str && *str )
      {
              *str = (char)toupper(*str);
              str++;
      }
      return start;
}

inline char *strlwr( char *start )
{
      char *str = start;
      while( str && *str )
      {
              *str = (char)tolower(*str);
              str++;
      }
      return start;
}

inline wchar_t *_wcslwr( wchar_t *start )
{
	wchar_t *str = start;
	while( str && *str )
	{
		*str = (wchar_t)towlower(static_cast<wint_t>(*str));
		str++;
	}
	return start;
};

inline wchar_t *_wcsupr( wchar_t *start )
{
	wchar_t *str = start;
	while( str && *str )
	{
		*str = (wchar_t)towupper(static_cast<wint_t>(*str));
		str++;
	}
	return start;
};

#endif // POSIX

// there are some users of these via tier1 templates in used in tier0. but tier0 can't depend on vstdlib which means in tier0 we always need the inlined ones
#if ( !defined( TIER0_DLL_EXPORT ) )

#if !defined( _DEBUG ) && defined( _PS3 )

#include "tier1/strtools_inlines.h"

// To avoid cross-prx calls, making the V_* fucntions that don't do anything but debug checks and call through to the non V_* function
// go ahead and call the non-V_* functions directly.
#define V_memset(dest, fill, count)		memset   ((dest), (fill), (count))	
#define V_memcpy(dest, src, count)		memcpy	((dest), (src), (count))	
#define V_memmove(dest, src, count)		memmove	((dest), (src), (count))	
#define V_memcmp(m1, m2, count)			memcmp	((m1), (m2), (count))		
#define V_strcpy(dest, src)				strcpy	((dest), (src))			
#define V_strcmp(s1, s2)				strcmp	((s1), (s2))			
#define V_strupr(start)					strupr	((start))				
#define V_strlower(start)				strlwr ((start))		
#define V_wcslen(pwch)					wcslen	((pwch))		
// To avoid cross-prx calls, using inline versions of these custom functions:
#define V_strlen(str)					_V_strlen_inline	((str))				
#define V_strrchr(s, c)					_V_strrchr_inline	((s), (c))				
#define V_wcscmp(s1, s2)				_V_wcscmp_inline	((s1), (s2))			
#define V_stricmp(s1, s2 )				_V_stricmp_inline	((s1), (s2) )			
#define V_strstr(s1, search )			_V_strstr_inline	((s1), (search) )		

#else

#define V_memset(dest, fill, count)		_V_memset   ((dest), (fill), (count))	
#define V_memcpy(dest, src, count)		_V_memcpy	((dest), (src), (count))	
#define V_memmove(dest, src, count)		_V_memmove	((dest), (src), (count))	
#define V_memcmp(m1, m2, count)			_V_memcmp	((m1), (m2), (count))		
#define V_strlen(str)					_V_strlen	((str))				
#define V_strcpy(dest, src)				_V_strcpy	((dest), (src))			
#define V_strrchr(s, c)					_V_strrchr	((s), (c))				
#define V_strcmp(s1, s2)				_V_strcmp	((s1), (s2))			
#define V_wcscmp(s1, s2)				_V_wcscmp	((s1), (s2))			
#define V_stricmp(s1, s2 )				_V_stricmp	((s1), (s2) )			
#define V_strstr(s1, search )			_V_strstr	((s1), (search) )		
#define V_strupr(start)					_V_strupr	((start))				
#define V_strlower(start)				_V_strlower ((start))		
#define V_wcslen(pwch)					_V_wcslen	((pwch))		

#endif

#else

inline void		V_memset (void *dest, int fill, int count)			{ memset( dest, fill, count ); }
inline void		V_memcpy (void *dest, const void *src, int count)	{ memcpy( dest, src, count ); }
inline void		V_memmove (void *dest, const void *src, int count)	{ memmove( dest, src, count ); }
inline int		V_memcmp (const void *m1, const void *m2, int count){ return memcmp( m1, m2, count ); } 
inline int		V_strlen (const char *str)							{ return (int) strlen ( str ); }
inline void		V_strcpy (char *dest, const char *src)				{ strcpy( dest, src ); }
inline int		V_wcslen(const wchar_t *pwch)						{ return (int)wcslen(pwch); }
inline char*	V_strrchr (const char *s, char c)					{ return (char*)strrchr( s, c ); }
inline int		V_strcmp (const char *s1, const char *s2)			{ return strcmp( s1, s2 ); }
inline int		V_wcscmp (const wchar_t *s1, const wchar_t *s2)		{ return wcscmp( s1, s2 ); }
inline int		V_stricmp( const char *s1, const char *s2 )			{ return stricmp( s1, s2 ); }
inline char*	V_strstr( const char *s1, const char *search )		{ return (char*)strstr( s1, search ); }
#ifndef COMPILER_PS3
inline char*	V_strupr (char *start)								{ return strupr( start ); }
inline char*	V_strlower (char *start)							{ return strlwr( start ); }
inline wchar_t*	V_wcsupr (wchar_t *start)							{ return _wcsupr( start ); }
#endif

#endif


int			V_strncmp (const char *s1, const char *s2, int count);
int			V_strcasecmp (const char *s1, const char *s2);
int			V_strncasecmp (const char *s1, const char *s2, int n);
int			V_strnicmp (const char *s1, const char *s2, int n);
int			V_atoi (const char *str);
int64 		V_atoi64(const char *str);
uint64 		V_atoui64(const char *str);
float		V_atof (const char *str);
char*		V_stristr( char* pStr, const char* pSearch );
const char*	V_stristr( const char* pStr, const char* pSearch );
const char*	V_strnistr( const char* pStr, const char* pSearch, int n );
const char*	V_strnchr( const char* pStr, char c, int n );

// returns string immediately following prefix, (ie str+strlen(prefix)) or NULL if prefix not found
const char *StringAfterPrefix             ( const char *str, const char *prefix );
const char *StringAfterPrefixCaseSensitive( const char *str, const char *prefix );
inline bool	StringHasPrefix             ( const char *str, const char *prefix ) { return StringAfterPrefix             ( str, prefix ) != NULL; }
inline bool	StringHasPrefixCaseSensitive( const char *str, const char *prefix ) { return StringAfterPrefixCaseSensitive( str, prefix ) != NULL; }


// Normalizes a float string in place.  
// (removes leading zeros, trailing zeros after the decimal point, and the decimal point itself where possible)
void			V_normalizeFloatString( char* pFloat );

inline bool V_isspace(int c)
{
	// The standard white-space characters are the following: space, tab, carriage-return, newline, vertical tab, and form-feed. In the C locale, V_isspace() returns true only for the standard white-space characters. 
	//return c == ' ' || c == 9 /*horizontal tab*/ || c == '\r' || c == '\n' || c == 11 /*vertical tab*/ || c == '\f';
	// codes of whitespace symbols: 9 HT, 10 \n, 11 VT, 12 form feed, 13 \r, 32 space
	
	// easy to understand version, validated:
	// return ((1 << (c-1)) & 0x80001F00) != 0 && ((c-1)&0xE0) == 0;
	
	// 5% faster on Core i7, 35% faster on Xbox360, no branches, validated:
	#ifdef _X360
	return ((1 << (c-1)) & 0x80001F00 & ~(-int((c-1)&0xE0))) != 0;
	#else
	// this is 11% faster on Core i7 than the previous, VC2005 compiler generates a seemingly unbalanced search tree that's faster
	switch(c)
	{
	case ' ':
	case 9:
	case '\r':
	case '\n':
	case 11:
	case '\f':
		return true;
	default:
		return false;
	}
	#endif
}


// These are versions of functions that guarantee NULL termination.
//
// maxLen is the maximum number of bytes in the destination string.
// pDest[maxLen-1] is always NULL terminated if pSrc's length is >= maxLen.
//
// This means the last parameter can usually be a sizeof() of a string.
void V_strncpy( char *pDest, const char *pSrc, int maxLen );
int V_snprintf( char *pDest, int destLen, const char *pFormat, ... ) FMTFUNCTION( 3, 4 );
void V_wcsncpy( wchar_t *pDest, wchar_t const *pSrc, int maxLenInBytes );
int V_snwprintf( wchar_t *pDest, int maxLenInNumWideCharacters, const wchar_t *pFormat, ... );

#define COPY_ALL_CHARACTERS -1
char *V_strncat(char *, const char *, size_t maxLenInBytes, int max_chars_to_copy=COPY_ALL_CHARACTERS );
wchar_t *V_wcsncat(wchar_t *, const wchar_t *, int maxLenInBytes, int max_chars_to_copy=COPY_ALL_CHARACTERS );
char *V_strnlwr(char *, size_t);


// UNDONE: Find a non-compiler-specific way to do this
#ifdef _WIN32
#ifndef _VA_LIST_DEFINED

#ifdef  _M_ALPHA

struct va_list 
{
    char *a0;       /* pointer to first homed integer argument */
    int offset;     /* byte offset of next parameter */
};

#else  // !_M_ALPHA

typedef char *  va_list;

#endif // !_M_ALPHA

#define _VA_LIST_DEFINED

#endif   // _VA_LIST_DEFINED

#elif POSIX
#include <stdarg.h>
#endif

#ifdef _WIN32
#define CORRECT_PATH_SEPARATOR '\\'
#define CORRECT_PATH_SEPARATOR_S "\\"
#define INCORRECT_PATH_SEPARATOR '/'
#define INCORRECT_PATH_SEPARATOR_S "/"
#elif POSIX || defined( _PS3 )
#define CORRECT_PATH_SEPARATOR '/'
#define CORRECT_PATH_SEPARATOR_S "/"
#define INCORRECT_PATH_SEPARATOR '\\'
#define INCORRECT_PATH_SEPARATOR_S "\\"
#endif

int V_vsnprintf( char *pDest, int maxLen, const char *pFormat, va_list params );
int V_vsnprintfRet( char *pDest, int maxLen, const char *pFormat, va_list params, bool *pbTruncated );

// Prints out a pretified memory counter string value ( e.g., 7,233.27 Mb, 1,298.003 Kb, 127 bytes )
char *V_pretifymem( float value, int digitsafterdecimal = 2, bool usebinaryonek = false );

// Prints out a pretified integer with comma separators (eg, 7,233,270,000)
char *V_pretifynum( int64 value );

// Functions for converting hexidecimal character strings back into binary data etc.
//
// e.g., 
// int output;
// V_hextobinary( "ffffffff", 8, &output, sizeof( output ) );
// would make output == 0xfffffff or -1
// Similarly,
// char buffer[ 9 ];
// V_binarytohex( &output, sizeof( output ), buffer, sizeof( buffer ) );
// would put "ffffffff" into buffer (note null terminator!!!)
void V_hextobinary( char const *in, int numchars, byte *out, int maxoutputbytes );
void V_binarytohex( const byte *in, int inputbytes, char *out, int outsize );

// Tools for working with filenames
// Extracts the base name of a file (no path, no extension, assumes '/' or '\' as path separator)
void V_FileBase( const char *in, char *out,int maxlen );
// Remove the final characters of ppath if it's '\' or '/'.
void V_StripTrailingSlash( char *ppath );
// Remove any extension from in and return resulting string in out
void V_StripExtension( const char *in, char *out, int outLen );
// Make path end with extension if it doesn't already have an extension
void V_DefaultExtension( char *path, const char *extension, int pathStringLength );
// Strips any current extension from path and ensures that extension is the new extension.
// NOTE: extension string MUST include the . character
void V_SetExtension( char *path, const char *extension, int pathStringLength );
// Removes any filename from path ( strips back to previous / or \ character )
void V_StripFilename( char *path );
// Remove the final directory from the path
bool V_StripLastDir( char *dirName, int maxlen );
// Returns a pointer to the unqualified file name (no path) of a file name
const char * V_UnqualifiedFileName( const char * in );
char * V_UnqualifiedFileName( char * in );
// Given a path and a filename, composes "path\filename", inserting the (OS correct) separator if necessary
void V_ComposeFileName( const char *path, const char *filename, char *dest, int destSize );

// Copy out the path except for the stuff after the final pathseparator
bool V_ExtractFilePath( const char *path, char *dest, int destSize );
// Copy out the file extension into dest
void V_ExtractFileExtension( const char *path, char *dest, int destSize );

const char *V_GetFileExtension( const char * path );

// returns a pointer to just the filename part of the path
// (everything after the last path seperator)
const char *V_GetFileName( const char * path );

// This removes "./" and "../" from the pathname. pFilename should be a full pathname.
// Returns false if it tries to ".." past the root directory in the drive (in which case 
// it is an invalid path).
bool V_RemoveDotSlashes( char *pFilename, char separator = CORRECT_PATH_SEPARATOR );

// If pPath is a relative path, this function makes it into an absolute path
// using the current working directory as the base, or pStartingDir if it's non-NULL.
// Returns false if it runs out of room in the string, or if pPath tries to ".." past the root directory.
void V_MakeAbsolutePath( char *pOut, int outLen, const char *pPath, const char *pStartingDir = NULL );

// Creates a relative path given two full paths
// The first is the full path of the file to make a relative path for.
// The second is the full path of the directory to make the first file relative to
// Returns false if they can't be made relative (on separate drives, for example)
bool V_MakeRelativePath( const char *pFullPath, const char *pDirectory, char *pRelativePath, int nBufLen );

// Fixes up a file name, removing dot slashes, fixing slashes, converting to lowercase, etc.
void V_FixupPathName( char *pOut, size_t nOutLen, const char *pPath );

// Adds a path separator to the end of the string if there isn't one already. Returns false if it would run out of space.
void V_AppendSlash( char *pStr, int strSize );

// Returns true if the path is an absolute path.
bool V_IsAbsolutePath( const char *pPath );

// Scans pIn and replaces all occurences of pMatch with pReplaceWith.
// Writes the result to pOut.
// Returns true if it completed successfully.
// If it would overflow pOut, it fills as much as it can and returns false.
bool V_StrSubst( const char *pIn, const char *pMatch, const char *pReplaceWith,
	char *pOut, int outLen, bool bCaseSensitive=false );


// Split the specified string on the specified separator.
// Returns a list of strings separated by pSeparator.
// You are responsible for freeing the contents of outStrings (call outStrings.PurgeAndDeleteElements).
void V_SplitString( const char *pString, const char *pSeparator, CUtlVector<char*, CUtlMemory<char*, int> > &outStrings );

// Just like V_SplitString, but it can use multiple possible separators.
void V_SplitString2( const char *pString, const char **pSeparators, int nSeparators, CUtlVector<char*, CUtlMemory<char*, int> > &outStrings );

// Returns false if the buffer is not large enough to hold the working directory name.
bool V_GetCurrentDirectory( char *pOut, int maxLen );

// Set the working directory thus.
bool V_SetCurrentDirectory( const char *pDirName );


// This function takes a slice out of pStr and stores it in pOut.
// It follows the Python slice convention:
// Negative numbers wrap around the string (-1 references the last character).
// Large numbers are clamped to the end of the string.
void V_StrSlice( const char *pStr, int firstChar, int lastCharNonInclusive, char *pOut, int outSize );

// Chop off the left nChars of a string.
void V_StrLeft( const char *pStr, int nChars, char *pOut, int outSize );

// Chop off the right nChars of a string.
void V_StrRight( const char *pStr, int nChars, char *pOut, int outSize );

// change "special" characters to have their c-style backslash sequence. like \n, \r, \t, ", etc.
// returns a pointer to a newly allocated string, which you must delete[] when finished with.
char *V_AddBackSlashesToSpecialChars( char const *pSrc );

// Force slashes of either type to be = separator character
void V_FixSlashes( char *pname, char separator = CORRECT_PATH_SEPARATOR );

// This function fixes cases of filenames like materials\\blah.vmt or somepath\otherpath\\ and removes the extra double slash.
void V_FixDoubleSlashes( char *pStr );

// Convert multibyte to wchar + back
// Specify -1 for nInSize for null-terminated string
void V_strtowcs( const char *pString, int nInSize, wchar_t *pWString, int nOutSize );
void V_wcstostr( const wchar_t *pWString, int nInSize, char *pString, int nOutSize );

// buffer-safe strcat
inline void V_strcat( char *dest, const char *src, int maxLenInBytes )
{
	V_strncat( dest, src, maxLenInBytes, COPY_ALL_CHARACTERS );
}

// buffer-safe strcat
inline void V_wcscat( wchar_t *dest, const wchar_t *src, int maxLenInBytes )
{
	V_wcsncat( dest, src, maxLenInBytes, COPY_ALL_CHARACTERS );
}

// Convert from a string to an array of integers.
void V_StringToIntArray( int *pVector, int count, const char *pString );

// Convert from a string to a 4 byte color value.
void V_StringToColor32( color32 *color, const char *pString );

// Convert \r\n (Windows linefeeds) to \n (Unix linefeeds).
void V_TranslateLineFeedsToUnix( char *pStr );

//-----------------------------------------------------------------------------
// generic unique name helper functions
//-----------------------------------------------------------------------------

// returns -1 if no match, nDefault if pName==prefix, and N if pName==prefix+N
inline int V_IndexAfterPrefix( const char *pName, const char *prefix, int nDefault = 0 )
{
	if ( !pName || !prefix )
		return -1;

	const char *pIndexStr = StringAfterPrefix( pName, prefix );
	if ( !pIndexStr )
		return -1;

	if ( !*pIndexStr )
		return nDefault;

	return atoi( pIndexStr );
}

// returns startindex if none found, 2 if "prefix" found, and n+1 if "prefixn" found
template < class NameArray >
int V_GenerateUniqueNameIndex( const char *prefix, const NameArray &nameArray, int startindex = 0 )
{
	if ( !prefix )
		return 0;

	int freeindex = startindex;

	int nNames = nameArray.Count();
	for ( int i = 0; i < nNames; ++i )
	{
		int index = V_IndexAfterPrefix( nameArray[ i ], prefix, 1 ); // returns -1 if no match, 0 for exact match, N for 
		if ( index >= freeindex )
		{
			// TODO - check that there isn't more junk after the index in pElementName
			freeindex = index + 1;
		}
	}

	return freeindex;
}

template < class NameArray >
bool V_GenerateUniqueName( char *name, int memsize, const char *prefix, const NameArray &nameArray )
{
	if ( name == NULL || memsize == 0 )
		return false;

	if ( prefix == NULL )
	{
		name[ 0 ] = '\0';
		return false;
	}

	int prefixLength = V_strlen( prefix );
	if ( prefixLength + 1 > memsize )
	{
		name[ 0 ] = '\0';
		return false;
	}

	int i = V_GenerateUniqueNameIndex( prefix, nameArray );
	if ( i <= 0 )
	{
		V_strncpy( name, prefix, memsize );
		return true;
	}

	int newlen = prefixLength + ( int )log10( ( float )i ) + 1;
	if ( newlen + 1 > memsize )
	{
		V_strncpy( name, prefix, memsize );
		return false;
	}

	V_snprintf( name, memsize, "%s%d", prefix, i );
	return true;
}


extern bool V_StringToBin( const char*pString, void *pBin, uint nBinSize );
extern bool V_BinToString( char*pString, void *pBin, uint nBinSize );

template<typename T>
struct BinString_t
{
	BinString_t(){}
	BinString_t( const char *pStr )
	{
		V_strncpy( m_string, pStr, sizeof(m_string) );
		ToBin();
	}
	BinString_t( const T & that )
	{
		m_bin = that;
		ToString();
	}
	bool ToBin()
	{
		return V_StringToBin( m_string, &m_bin, sizeof( m_bin ) );
	}
	void ToString()
	{
		V_BinToString( m_string, &m_bin, sizeof( m_bin ) );
	}
	T m_bin;
	char m_string[sizeof(T)*2+2]; // 0-terminated string representing the binary data in hex
};

template <typename T>
inline BinString_t<T> MakeBinString( const T& that )
{
	return BinString_t<T>( that );
}






#if defined(_PS3) || defined(POSIX)
#define PRI_WS_FOR_WS L"%ls"
#define PRI_WS_FOR_S "%ls"
#define PRI_S_FOR_WS L"%s"
#define PRI_S_FOR_S "%s"
#else
#define PRI_WS_FOR_WS L"%s"
#define PRI_WS_FOR_S "%S"
#define PRI_S_FOR_WS L"%S"
#define PRI_S_FOR_S "%s"
#endif

namespace AsianWordWrap
{
	// Functions used by Asian language line wrapping to determine if a character can end a line, begin a line, or be broken up when repeated (eg: "...")
	bool CanEndLine( wchar_t wcCandidate );
	bool CanBeginLine( wchar_t wcCandidate );
	bool CanBreakRepeated( wchar_t wcCandidate );

	// Used to determine if we can break a line between the first two characters passed; calls the above functions on each character
	bool CanBreakAfter( const wchar_t* wsz );
}

// We use this function to determine where it is permissible to break lines
// of text while wrapping them. On most platforms, the native iswspace() function
// returns FALSE for the "non-breaking space" characters 0x00a0 and 0x202f, and so we don't
// break on them. On the 360, however, iswspace returns TRUE for them. So, on that
// platform, we work around it by defining this wrapper which returns false
// for &nbsp; and calls through to the library function for everything else.
int isbreakablewspace( wchar_t ch );

#endif	// TIER1_STRTOOLS_H
