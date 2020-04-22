//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#ifndef UTLBUFFER_H
#define UTLBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#include "unitlib/unitlib.h" // just here for tests - remove before checking in!!!

#include "tier1/utlmemory.h"
#include "tier1/byteswap.h"
#include <stdarg.h>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct characterset_t;

	
//-----------------------------------------------------------------------------
// Description of character conversions for string output
// Here's an example of how to use the macros to define a character conversion
// BEGIN_CHAR_CONVERSION( CStringConversion, '\\' )
//	{ '\n', "n" },
//	{ '\t', "t" }
// END_CHAR_CONVERSION( CStringConversion, '\\' )
//-----------------------------------------------------------------------------
class CUtlCharConversion
{
public:
	struct ConversionArray_t
	{
		char m_nActualChar;
		char *m_pReplacementString;
	};

	CUtlCharConversion( char nEscapeChar, const char *pDelimiter, int nCount, ConversionArray_t *pArray );
	char GetEscapeChar() const;
	const char *GetDelimiter() const;
	int GetDelimiterLength() const;

	const char *GetConversionString( char c ) const;
	int GetConversionLength( char c ) const;
	int MaxConversionLength() const;

	// Finds a conversion for the passed-in string, returns length
	virtual char FindConversion( const char *pString, int *pLength );

protected:
	struct ConversionInfo_t
	{
		int m_nLength;
		char *m_pReplacementString;
	};

	char m_nEscapeChar;
	const char *m_pDelimiter;
	int m_nDelimiterLength;
	int m_nCount;
	int m_nMaxConversionLength;
	char m_pList[256];
	ConversionInfo_t m_pReplacements[256];
};

#define BEGIN_CHAR_CONVERSION( _name, _delimiter, _escapeChar )	\
	static CUtlCharConversion::ConversionArray_t s_pConversionArray ## _name[] = {

#define END_CHAR_CONVERSION( _name, _delimiter, _escapeChar ) \
	}; \
	CUtlCharConversion _name( _escapeChar, _delimiter, sizeof( s_pConversionArray ## _name ) / sizeof( CUtlCharConversion::ConversionArray_t ), s_pConversionArray ## _name );

#define BEGIN_CUSTOM_CHAR_CONVERSION( _className, _name, _delimiter, _escapeChar ) \
	static CUtlCharConversion::ConversionArray_t s_pConversionArray ## _name[] = {

#define END_CUSTOM_CHAR_CONVERSION( _className, _name, _delimiter, _escapeChar ) \
	}; \
	_className _name( _escapeChar, _delimiter, sizeof( s_pConversionArray ## _name ) / sizeof( CUtlCharConversion::ConversionArray_t ), s_pConversionArray ## _name );

//-----------------------------------------------------------------------------
// Character conversions for C strings
//-----------------------------------------------------------------------------
CUtlCharConversion *GetCStringCharConversion();

//-----------------------------------------------------------------------------
// Character conversions for quoted strings, with no escape sequences
//-----------------------------------------------------------------------------
CUtlCharConversion *GetNoEscCharConversion();


//-----------------------------------------------------------------------------
// Macro to set overflow functions easily
//-----------------------------------------------------------------------------
#define SetUtlBufferOverflowFuncs( _get, _put )	\
	SetOverflowFuncs( static_cast <UtlBufferOverflowFunc_t>( _get ), static_cast <UtlBufferOverflowFunc_t>( _put ) )


#define FMSTRRETTYPE static const char *
#ifdef LINUX
// gcc 4.3 is techinically correct and doesn't like the storage specifier,
// unfortunately, that makes gcc on the mac and VS wind up with multiply defined
// symbols at link time
#define FMSTRRETTYPE const char *
#endif

typedef unsigned short ushort;

template < class A >
static const char *GetFmtStr( int nRadix = 10, bool bPrint = true ) { Assert( 0 ); return ""; }

template <> FMSTRRETTYPE GetFmtStr< short >	( int nRadix, bool bPrint ) { Assert( nRadix == 10 ); return "%hd"; }
template <> FMSTRRETTYPE GetFmtStr< ushort >	( int nRadix, bool bPrint ) { Assert( nRadix == 10 ); return "%hu"; }
template <> FMSTRRETTYPE GetFmtStr< int >		( int nRadix, bool bPrint ) { Assert( nRadix == 10 ); return "%d"; }
template <> FMSTRRETTYPE GetFmtStr< uint >	( int nRadix, bool bPrint ) { Assert( nRadix == 10 || nRadix == 16 ); return nRadix == 16 ? "%x" : "%u"; }
template <> FMSTRRETTYPE GetFmtStr< int64 >	( int nRadix, bool bPrint ) { Assert( nRadix == 10 ); return "%lld"; }
template <> FMSTRRETTYPE GetFmtStr< float >	( int nRadix, bool bPrint ) { Assert( nRadix == 10 ); return "%f"; }
template <> FMSTRRETTYPE GetFmtStr< double >	( int nRadix, bool bPrint ) { Assert( nRadix == 10 ); return bPrint ? "%.15lf" : "%lf"; } // force Printf to print DBL_DIG=15 digits of precision for doubles - defaults to FLT_DIG=6


//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------
class CUtlBuffer
{
// Brian has on his todo list to revisit this as there are issues in some cases with CUtlVector using operator = instead of copy construtor in InsertMultiple, etc.
// The unsafe case is something like this:
//  CUtlVector< CUtlBuffer > vecFoo;
// 
//  CUtlBuffer buf;
//  buf.Put( xxx );
//  vecFoo.Insert( buf );
//
//  This will cause memory corruption when vecFoo is cleared
//
//private:
//	// Disallow copying
//	CUtlBuffer( const CUtlBuffer & );// { Assert( 0 ); }
//	CUtlBuffer &operator=( const CUtlBuffer & );//  { Assert( 0 ); return *this; }

public:
	enum SeekType_t
	{
		SEEK_HEAD = 0,
		SEEK_CURRENT,
		SEEK_TAIL
	};

	// flags
	enum BufferFlags_t
	{
		TEXT_BUFFER = 0x1,			// Describes how get + put work (as strings, or binary)
		EXTERNAL_GROWABLE = 0x2,	// This is used w/ external buffers and causes the utlbuf to switch to reallocatable memory if an overflow happens when Putting.
		CONTAINS_CRLF = 0x4,		// For text buffers only, does this contain \n or \n\r?
		READ_ONLY = 0x8,			// For external buffers; prevents null termination from happening.
		AUTO_TABS_DISABLED = 0x10,	// Used to disable/enable push/pop tabs
	};

	// Overflow functions when a get or put overflows
	typedef bool (CUtlBuffer::*UtlBufferOverflowFunc_t)( int nSize );

	// Constructors for growable + external buffers for serialization/unserialization
	CUtlBuffer( int growSize = 0, int initSize = 0, int nFlags = 0 );
	CUtlBuffer( const void* pBuffer, int size, int nFlags = 0 );
	// This one isn't actually defined so that we catch contructors that are trying to pass a bool in as the third param.
	CUtlBuffer( const void *pBuffer, int size, bool crap );

	unsigned char	GetFlags() const;

	// NOTE: This will assert if you attempt to recast it in a way that
	// is not compatible. The only valid conversion is binary-> text w/CRLF
	void			SetBufferType( bool bIsText, bool bContainsCRLF );

	// Makes sure we've got at least this much memory
	void			EnsureCapacity( int num );

	// Access for direct read into buffer
	void *			AccessForDirectRead( int nBytes );

	// Attaches the buffer to external memory....
	void			SetExternalBuffer( void* pMemory, int nSize, int nInitialPut, int nFlags = 0 );
	bool			IsExternallyAllocated() const;
	void			AssumeMemory( void *pMemory, int nSize, int nInitialPut, int nFlags = 0 );
	void			*Detach();
	void*			DetachMemory();

	FORCEINLINE void ActivateByteSwappingIfBigEndian( void )
	{
		if ( ( IsX360() || IsPS3() ) )
			ActivateByteSwapping( true );
	}


	// Controls endian-ness of binary utlbufs - default matches the current platform
	void			ActivateByteSwapping( bool bActivate );
	void			SetBigEndian( bool bigEndian );
	bool			IsBigEndian( void );

	// Resets the buffer; but doesn't free memory
	void			Clear();

	// Clears out the buffer; frees memory
	void			Purge();

	// Dump the buffer to stdout
	void			Spew( );

	// Read stuff out.
	// Binary mode: it'll just read the bits directly in, and characters will be
	//		read for strings until a null character is reached.
	// Text mode: it'll parse the file, turning text #s into real numbers.
	//		GetString will read a string until a space is reached
	char			GetChar( );
	unsigned char	GetUnsignedChar( );
	short			GetShort( );
	unsigned short	GetUnsignedShort( );
	int				GetInt( );
	int64			GetInt64( );
	unsigned int	GetIntHex( );
	unsigned int	GetUnsignedInt( );
	float			GetFloat( );
	double			GetDouble( );
	void *			GetPtr();
	void			GetString( char* pString, int nMaxChars = 0 );
	void			Get( void* pMem, int size );
	void			GetLine( char* pLine, int nMaxChars = 0 );

	// Used for getting objects that have a byteswap datadesc defined
	template <typename T> void GetObjects( T *dest, int count = 1 );

	// This will get at least 1 byte and up to nSize bytes. 
	// It will return the number of bytes actually read.
	int				GetUpTo( void *pMem, int nSize );

	// This version of GetString converts \" to \\ and " to \, etc.
	// It also reads a " at the beginning and end of the string
	void			GetDelimitedString( CUtlCharConversion *pConv, char *pString, int nMaxChars = 0 );
	char			GetDelimitedChar( CUtlCharConversion *pConv );

	// This will return the # of characters of the string about to be read out
	// NOTE: The count will *include* the terminating 0!!
	// In binary mode, it's the number of characters until the next 0
	// In text mode, it's the number of characters until the next space.
	int				PeekStringLength();

	// This version of PeekStringLength converts \" to \\ and " to \, etc.
	// It also reads a " at the beginning and end of the string
	// NOTE: The count will *include* the terminating 0!!
	// In binary mode, it's the number of characters until the next 0
	// In text mode, it's the number of characters between "s (checking for \")
	// Specifying false for bActualSize will return the pre-translated number of characters
	// including the delimiters and the escape characters. So, \n counts as 2 characters when bActualSize == false
	// and only 1 character when bActualSize == true
	int				PeekDelimitedStringLength( CUtlCharConversion *pConv, bool bActualSize = true );

	// Just like scanf, but doesn't work in binary mode
	int				Scanf( const char* pFmt, ... );
 	int				VaScanf( const char* pFmt, va_list list );

	// Eats white space, advances Get index
	void			EatWhiteSpace();

	// Eats C++ style comments
	bool			EatCPPComment();

	// (For text buffers only)
	// Parse a token from the buffer:
	// Grab all text that lies between a starting delimiter + ending delimiter
	// (skipping whitespace that leads + trails both delimiters).
	// If successful, the get index is advanced and the function returns true,
	// otherwise the index is not advanced and the function returns false.
	bool			ParseToken( const char *pStartingDelim, const char *pEndingDelim, char* pString, int nMaxLen );

	// Advance the get index until after the particular string is found
	// Do not eat whitespace before starting. Return false if it failed
	// String test is case-insensitive.
	bool			GetToken( const char *pToken );

	// Parses the next token, given a set of character breaks to stop at
	// Returns the length of the token parsed in bytes (-1 if none parsed)
	int				ParseToken( characterset_t *pBreaks, char *pTokenBuf, int nMaxLen, bool bParseComments = true );

	// Write stuff in
	// Binary mode: it'll just write the bits directly in, and strings will be
	//		written with a null terminating character
	// Text mode: it'll convert the numbers to text versions
	//		PutString will not write a terminating character
	void			PutChar( char c );
	void			PutUnsignedChar( unsigned char uc );
	void			PutShort( short s );
	void			PutUnsignedShort( unsigned short us );
	void			PutInt( int i );
	void			PutInt64( int64 i );
	void			PutUnsignedInt( unsigned int u );
	void			PutFloat( float f );
	void			PutDouble( double d );
	void			PutPtr( void * ); // Writes the pointer, not the pointed to
	void			PutString( const char* pString );
	void			Put( const void* pMem, int size );

	// Used for putting objects that have a byteswap datadesc defined
	template <typename T> void PutObjects( T *src, int count = 1 );

	// This version of PutString converts \ to \\ and " to \", etc.
	// It also places " at the beginning and end of the string
	void			PutDelimitedString( CUtlCharConversion *pConv, const char *pString );
	void			PutDelimitedChar( CUtlCharConversion *pConv, char c );

	// Just like printf, writes a terminating zero in binary mode
	void			Printf( const char* pFmt, ... ) FMTFUNCTION( 2, 3 );
	void			VaPrintf( const char* pFmt, va_list list );

	// What am I writing (put)/reading (get)?
	void* PeekPut( int offset = 0 );
	const void* PeekGet( int offset = 0 ) const;
	const void* PeekGet( int nMaxSize, int nOffset );

	// Where am I writing (put)/reading (get)?
	int TellPut( ) const;
	int TellGet( ) const;

	// What's the most I've ever written?
	int TellMaxPut( ) const;

	// How many bytes remain to be read?
	// NOTE: This is not accurate for streaming text files; it overshoots
	int GetBytesRemaining() const;

	// Change where I'm writing (put)/reading (get)
	void SeekPut( SeekType_t type, int offset );
	void SeekGet( SeekType_t type, int offset );

	// Buffer base
	const void* Base() const;
	void* Base();

	// memory allocation size, does *not* reflect size written or read,
	//	use TellPut or TellGet for that
	int Size() const;

	// Am I a text buffer?
	bool IsText() const;

	// Can I grow if I'm externally allocated?
	bool IsGrowable() const;

	// Am I valid? (overflow or underflow error), Once invalid it stays invalid
	bool IsValid() const;

	// Do I contain carriage return/linefeeds? 
	bool ContainsCRLF() const;

	// Am I read-only
	bool IsReadOnly() const;

	// Converts a buffer from a CRLF buffer to a CR buffer (and back)
	// Returns false if no conversion was necessary (and outBuf is left untouched)
	// If the conversion occurs, outBuf will be cleared.
	bool ConvertCRLF( CUtlBuffer &outBuf );

	// Push/pop pretty-printing tabs
	void PushTab();
	void PopTab();

	// Temporarily disables pretty print
	void EnableTabs( bool bEnable );

#if !defined( _GAMECONSOLE )
	// Swap my internal memory with another buffer,
	// and copy all of its other members
	void SwapCopy( CUtlBuffer &other ) ;
#endif

protected:
	// error flags
	enum
	{
		PUT_OVERFLOW = 0x1,
		GET_OVERFLOW = 0x2,
		MAX_ERROR_FLAG = GET_OVERFLOW,
	};

	void SetOverflowFuncs( UtlBufferOverflowFunc_t getFunc, UtlBufferOverflowFunc_t putFunc );

	bool OnPutOverflow( int nSize );
	bool OnGetOverflow( int nSize );

protected:
	// Checks if a get/put is ok
	bool CheckPut( int size );
	bool CheckGet( int size );

	// NOTE: Pass in nPut here even though it is just a copy of m_Put.  This is almost always called immediately 
	// after modifying m_Put and this lets it stay in a register
	void AddNullTermination( int nPut );

	// Methods to help with pretty-printing
	bool WasLastCharacterCR();
	void PutTabs();

	// Help with delimited stuff
	char GetDelimitedCharInternal( CUtlCharConversion *pConv );
	void PutDelimitedCharInternal( CUtlCharConversion *pConv, char c );

	// Default overflow funcs
	bool PutOverflow( int nSize );
	bool GetOverflow( int nSize );

	// Does the next bytes of the buffer match a pattern?
	bool PeekStringMatch( int nOffset, const char *pString, int nLen );

	// Peek size of line to come, check memory bound
	int	PeekLineLength();

	// How much whitespace should I skip?
	int PeekWhiteSpace( int nOffset );

	// Checks if a peek get is ok
	bool CheckPeekGet( int nOffset, int nSize );

	// Call this to peek arbitrarily long into memory. It doesn't fail unless
	// it can't read *anything* new
	bool CheckArbitraryPeekGet( int nOffset, int &nIncrement );

	template <typename T> void GetType( T& dest );
	template <typename T> void GetTypeBin( T& dest );
	template <typename T> bool GetTypeText( T &value, int nRadix = 10 );
	template <typename T> void GetObject( T *src );

	template <typename T> void PutType( T src );
	template <typename T> void PutTypeBin( T src );
	template <typename T> void PutObject( T *src );

	// be sure to also update the copy constructor
	// and SwapCopy() when adding members.
	CUtlMemory<unsigned char> m_Memory;
	int m_Get;
	int m_Put;

	unsigned char m_Error;
	unsigned char m_Flags;
	unsigned char m_Reserved;
#if defined( _GAMECONSOLE )
	unsigned char pad;
#endif

	int m_nTab;
	int m_nMaxPut;
	int m_nOffset;

	UtlBufferOverflowFunc_t m_GetOverflowFunc;
	UtlBufferOverflowFunc_t m_PutOverflowFunc;

	CByteswap	m_Byteswap;
};


// Stream style output operators for CUtlBuffer
inline CUtlBuffer &operator<<( CUtlBuffer &b, char v )
{
	b.PutChar( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, unsigned char v )
{
	b.PutUnsignedChar( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, short v )
{
	b.PutShort( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, unsigned short v )
{
	b.PutUnsignedShort( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, int v )
{
	b.PutInt( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, unsigned int v )
{
	b.PutUnsignedInt( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, float v )
{
	b.PutFloat( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, double v )
{
	b.PutDouble( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, const char *pv )
{
	b.PutString( pv );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, const Vector &v )
{
	b << v.x << " " << v.y << " " << v.z;
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, const Vector2D &v )
{
	b << v.x << " " << v.y;
	return b;
}


class CUtlInplaceBuffer : public CUtlBuffer
{
public:
	CUtlInplaceBuffer( int growSize = 0, int initSize = 0, int nFlags = 0 );

	//
	// Routines returning buffer-inplace-pointers
	//
public:
	//
	// Upon success, determines the line length, fills out the pointer to the
	// beginning of the line and the line length, advances the "get" pointer
	// offset by the line length and returns "true".
	//
	// If end of file is reached or upon error returns "false".
	//
	// Note:	the returned length of the line is at least one character because the
	//			trailing newline characters are also included as part of the line.
	//
	// Note:	the pointer returned points into the local memory of this buffer, in
	//			case the buffer gets relocated or destroyed the pointer becomes invalid.
	//
	// e.g.:	-------------
	//
	//			char *pszLine;
	//			int nLineLen;
	//			while ( pUtlInplaceBuffer->InplaceGetLinePtr( &pszLine, &nLineLen ) )
	//			{
	//				...
	//			}
	//
	//			-------------
	//
	// @param	ppszInBufferPtr		on return points into this buffer at start of line
	// @param	pnLineLength		on return holds num bytes accessible via (*ppszInBufferPtr)
	//
	// @returns	true				if line was successfully read
	//			false				when EOF is reached or error occurs
	//
	bool InplaceGetLinePtr( /* out */ char **ppszInBufferPtr, /* out */ int *pnLineLength );

	//
	// Determines the line length, advances the "get" pointer offset by the line length,
	// replaces the newline character with null-terminator and returns the initial pointer
	// to now null-terminated line.
	//
	// If end of file is reached or upon error returns NULL.
	//
	// Note:	the pointer returned points into the local memory of this buffer, in
	//			case the buffer gets relocated or destroyed the pointer becomes invalid.
	//
	// e.g.:	-------------
	//
	//			while ( char *pszLine = pUtlInplaceBuffer->InplaceGetLinePtr() )
	//			{
	//				...
	//			}
	//
	//			-------------
	//
	// @returns	ptr-to-zero-terminated-line		if line was successfully read and buffer modified
	//			NULL							when EOF is reached or error occurs
	//
	char * InplaceGetLinePtr( void );
};


//-----------------------------------------------------------------------------
// Where am I reading?
//-----------------------------------------------------------------------------
inline int CUtlBuffer::TellGet( ) const
{
	return m_Get;
}


//-----------------------------------------------------------------------------
// How many bytes remain to be read?
//-----------------------------------------------------------------------------
inline int CUtlBuffer::GetBytesRemaining() const
{
	return m_nMaxPut - TellGet();
}


//-----------------------------------------------------------------------------
// What am I reading?
//-----------------------------------------------------------------------------
inline const void* CUtlBuffer::PeekGet( int offset ) const
{
	return &m_Memory[ m_Get + offset - m_nOffset ];
}


//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------

template <typename T> 
inline void CUtlBuffer::GetObject( T *dest )
{
	if ( CheckGet( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			*dest = *(T *)PeekGet();
		}
		else
		{
			m_Byteswap.SwapFieldsToTargetEndian<T>( dest, (T*)PeekGet() );
		}
		m_Get += sizeof(T);	
	}
	else
	{
		V_memset( &dest, 0, sizeof(T) );
	}
}


template <typename T> 
inline void CUtlBuffer::GetObjects( T *dest, int count )
{
	for ( int i = 0; i < count; ++i, ++dest )
	{
		GetObject<T>( dest );
	}
}


template <typename T> 
inline void CUtlBuffer::GetTypeBin( T &dest )
{
	if ( CheckGet( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			dest = *(T *)PeekGet();
		}
		else
		{
			m_Byteswap.SwapBufferToTargetEndian<T>( &dest, (T*)PeekGet() );
		}
		m_Get += sizeof(T);	
	}		
	else
	{
		dest = 0;
	}					
}

template <>
inline void CUtlBuffer::GetTypeBin< float >( float &dest )
{
	if ( CheckGet( sizeof( float ) ) )
	{
		uintp pData = (uintp)PeekGet();
		if ( ( IsX360() || IsPS3() ) && ( pData & 0x03 ) )
		{
			// handle unaligned read
			((unsigned char*)&dest)[0] = ((unsigned char*)pData)[0];
			((unsigned char*)&dest)[1] = ((unsigned char*)pData)[1];
			((unsigned char*)&dest)[2] = ((unsigned char*)pData)[2];
			((unsigned char*)&dest)[3] = ((unsigned char*)pData)[3];
		}
		else
		{
			// aligned read
			dest = *(float *)pData;
		}
		if ( m_Byteswap.IsSwappingBytes() )
		{
			m_Byteswap.SwapBufferToTargetEndian< float >( &dest, &dest );
		}
		m_Get += sizeof( float );	
	}		
	else
	{
		dest = 0;
	}					
}

template <>
inline void CUtlBuffer::GetTypeBin< double >( double &dest )
{
	if ( CheckGet( sizeof( double ) ) )
	{
		uintp pData = (uintp)PeekGet();
		if ( ( IsX360() || IsPS3() ) && ( pData & 0x07 ) )
		{
			// handle unaligned read
			((unsigned char*)&dest)[0] = ((unsigned char*)pData)[0];
			((unsigned char*)&dest)[1] = ((unsigned char*)pData)[1];
			((unsigned char*)&dest)[2] = ((unsigned char*)pData)[2];
			((unsigned char*)&dest)[3] = ((unsigned char*)pData)[3];
			((unsigned char*)&dest)[4] = ((unsigned char*)pData)[4];
			((unsigned char*)&dest)[5] = ((unsigned char*)pData)[5];
			((unsigned char*)&dest)[6] = ((unsigned char*)pData)[6];
			((unsigned char*)&dest)[7] = ((unsigned char*)pData)[7];
		}
		else
		{
			// aligned read
			dest = *(double *)pData;
		}
		if ( m_Byteswap.IsSwappingBytes() )
		{
			m_Byteswap.SwapBufferToTargetEndian< double >( &dest, &dest );
		}
		m_Get += sizeof( double );	
	}		
	else
	{
		dest = 0;
	}					
}

template < class T >
inline T StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	Assert( 0 );
	*ppEnd = pString;
	return 0;
}

template <>
inline int8 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	return ( int8 )strtol( pString, ppEnd, nRadix );
}

template <>
inline uint8 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	return ( uint8 )strtoul( pString, ppEnd, nRadix );
}

template <>
inline int16 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	return ( int16 )strtol( pString, ppEnd, nRadix );
}

template <>
inline uint16 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	return ( uint16 )strtoul( pString, ppEnd, nRadix );
}

template <>
inline int32 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	return ( int32 )strtol( pString, ppEnd, nRadix );
}

template <>
inline uint32 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	return ( uint32 )strtoul( pString, ppEnd, nRadix );
}

template <>
inline int64 StringToNumber( char *pString, char **ppEnd, int nRadix )
{
#if defined(_PS3) || defined(POSIX)
	return ( int64 )strtoll( pString, ppEnd, nRadix );
#else // !_PS3
	return ( int64 )_strtoi64( pString, ppEnd, nRadix );
#endif // _PS3
}

template <>
inline float StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	NOTE_UNUSED( nRadix );
	return ( float )strtod( pString, ppEnd );
}

template <>
inline double StringToNumber( char *pString, char **ppEnd, int nRadix )
{
	NOTE_UNUSED( nRadix );
	return ( double )strtod( pString, ppEnd );
}

template <typename T>
inline bool CUtlBuffer::GetTypeText( T &value, int nRadix /*= 10*/ )
{
	// NOTE: This is not bullet-proof; it assumes numbers are < 128 characters
	int nLength = 128;
	if ( !CheckArbitraryPeekGet( 0, nLength ) )
	{
		value = 0;
		return false;
	}

	char *pStart = (char*)PeekGet();
	char* pEnd = pStart;
	value = StringToNumber< T >( pStart, &pEnd, nRadix );

	int nBytesRead = (int)( pEnd - pStart );
	if ( nBytesRead == 0 )
		return false;

	m_Get += nBytesRead;
	return true;
}

template <typename T> 
inline void CUtlBuffer::GetType( T &dest )
{
	if (!IsText())
	{
		GetTypeBin( dest );
	}
	else
	{
		GetTypeText( dest );
	}
}

inline char CUtlBuffer::GetChar( )
{
	// LEGACY WARNING: this behaves differently than GetUnsignedChar()
	char c;
	GetTypeBin( c ); // always reads as binary
	return c;
}

inline unsigned char CUtlBuffer::GetUnsignedChar( )
{
	// LEGACY WARNING: this behaves differently than GetChar()
	unsigned char c;
	if (!IsText())
	{
		GetTypeBin( c );
	}
	else
	{
		c = ( unsigned char )GetUnsignedShort();
	}
	return c;
}

inline short CUtlBuffer::GetShort( )
{
	short s;
	GetType( s );
	return s;
}

inline unsigned short CUtlBuffer::GetUnsignedShort( )
{
	unsigned short s;
	GetType( s );
	return s;
}

inline int CUtlBuffer::GetInt( )
{
	int i;
	GetType( i );
	return i;
}

inline int64 CUtlBuffer::GetInt64( )
{
	int64 i;
	GetType( i );
	return i;
}

inline unsigned int CUtlBuffer::GetIntHex( )
{
	uint i;
	if (!IsText())
	{
		GetTypeBin( i );
	}
	else
	{
		GetTypeText( i, 16 );
	}
	return i;
}

inline unsigned int CUtlBuffer::GetUnsignedInt( )
{
	unsigned int i;
	GetType( i );
	return i;
}

inline float CUtlBuffer::GetFloat( )
{
	float f;
	GetType( f );
	return f;
}

inline double CUtlBuffer::GetDouble( )
{
	double d;
	GetType( d );
	return d;
}

inline void *CUtlBuffer::GetPtr( )
{
	void *p;
	// LEGACY WARNING: in text mode, PutPtr writes 32 bit pointers in hex, while GetPtr reads 32 or 64 bit pointers in decimal
#ifndef X64BITS
	p = ( void* )GetUnsignedInt();
#else
	p = ( void* )GetInt64();
#endif
	return p;
}

//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline unsigned char CUtlBuffer::GetFlags() const
{ 
	return m_Flags; 
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsExternallyAllocated() const
{ 
	return m_Memory.IsExternallyAllocated();
}

	
//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline int CUtlBuffer::TellPut( ) const
{
	return m_Put;
}


//-----------------------------------------------------------------------------
// What's the most I've ever written?
//-----------------------------------------------------------------------------
inline int CUtlBuffer::TellMaxPut( ) const
{
	return m_nMaxPut;
}


//-----------------------------------------------------------------------------
// What am I reading?
//-----------------------------------------------------------------------------
inline void* CUtlBuffer::PeekPut( int offset )
{
	return &m_Memory[m_Put + offset - m_nOffset];
}


//-----------------------------------------------------------------------------
// Various put methods
//-----------------------------------------------------------------------------

template <typename T> 
inline void CUtlBuffer::PutObject( T *src )
{
	if ( CheckPut( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			*(T *)PeekPut() = *src;
		}
		else
		{
			m_Byteswap.SwapFieldsToTargetEndian<T>( (T*)PeekPut(), src );
		}
		m_Put += sizeof(T);
		AddNullTermination( m_Put );
	}
}


template <typename T> 
inline void CUtlBuffer::PutObjects( T *src, int count )
{
	for ( int i = 0; i < count; ++i, ++src )
	{
		PutObject<T>( src );
	}
}


template <typename T> 
inline void CUtlBuffer::PutTypeBin( T src )
{
	if ( CheckPut( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			*(T *)PeekPut() = src;
		}
		else
		{
			m_Byteswap.SwapBufferToTargetEndian<T>( (T*)PeekPut(), &src );
		}
		m_Put += sizeof(T);
		AddNullTermination( m_Put );
	}
}

#if defined( _GAMECONSOLE )
template <>
inline void CUtlBuffer::PutTypeBin< float >( float src )
{
	if ( CheckPut( sizeof( src ) ) )
	{
		if ( m_Byteswap.IsSwappingBytes() )
		{
			m_Byteswap.SwapBufferToTargetEndian<float>( &src, &src );
		}

		//
		// Write the data
		//
		unsigned pData = (unsigned)PeekPut();
		if ( pData & 0x03 )
		{
			// handle unaligned write
			byte* dst = (byte*)pData;
			byte* srcPtr = (byte*)&src;
			dst[0] = srcPtr[0];
			dst[1] = srcPtr[1];
			dst[2] = srcPtr[2];
			dst[3] = srcPtr[3];
		}
		else
		{
			*(float *)pData = src;
		}

		m_Put += sizeof(float);
		AddNullTermination( m_Put );
	}
}

template <>
inline void CUtlBuffer::PutTypeBin< double >( double src )
{
	if ( CheckPut( sizeof( src ) ) )
	{
		if ( m_Byteswap.IsSwappingBytes() )
		{
			m_Byteswap.SwapBufferToTargetEndian<double>( &src, &src );
		}

		//
		// Write the data
		//
		unsigned pData = (unsigned)PeekPut();
		if ( pData & 0x07 )
		{
			// handle unaligned write
			byte* dst = (byte*)pData;
			byte* srcPtr = (byte*)&src;
			dst[0] = srcPtr[0];
			dst[1] = srcPtr[1];
			dst[2] = srcPtr[2];
			dst[3] = srcPtr[3];
			dst[4] = srcPtr[4];
			dst[5] = srcPtr[5];
			dst[6] = srcPtr[6];
			dst[7] = srcPtr[7];
		}
		else
		{
			*(double *)pData = src;
		}

		m_Put += sizeof(double);
		AddNullTermination( m_Put );
	}
}
#endif

template <typename T> 
inline void CUtlBuffer::PutType( T src )
{
	if (!IsText())
	{
		PutTypeBin( src );
	}
	else
	{
		Printf( GetFmtStr< T >(), src );
	}
}

//-----------------------------------------------------------------------------
// Methods to help with pretty-printing
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::WasLastCharacterCR()
{
	if ( !IsText() || (TellPut() == 0) )
		return false;
	return ( *( const char * )PeekPut( -1 ) == '\n' );
}

inline void CUtlBuffer::PutTabs()
{
	int nTabCount = ( m_Flags & AUTO_TABS_DISABLED ) ? 0 : m_nTab;
	for (int i = nTabCount; --i >= 0; )
	{
		PutTypeBin<char>( '\t' );
	}
}


//-----------------------------------------------------------------------------
// Push/pop pretty-printing tabs
//-----------------------------------------------------------------------------
inline void CUtlBuffer::PushTab( )
{
	++m_nTab;
}

inline void CUtlBuffer::PopTab()
{
	if ( --m_nTab < 0 )
	{
		m_nTab = 0;
	}
}


//-----------------------------------------------------------------------------
// Temporarily disables pretty print
//-----------------------------------------------------------------------------
inline void CUtlBuffer::EnableTabs( bool bEnable )
{
	if ( bEnable )
	{
		m_Flags &= ~AUTO_TABS_DISABLED;
	}
	else
	{
		m_Flags |= AUTO_TABS_DISABLED; 
	}
}

inline void CUtlBuffer::PutChar( char c )
{
	if ( WasLastCharacterCR() )
	{
		PutTabs();
	}

	PutTypeBin( c );
}

inline void CUtlBuffer::PutUnsignedChar( unsigned char c )
{
	if (!IsText())
	{
		PutTypeBin( c );
	}
	else
	{
		PutUnsignedShort( c );
	}
}

inline void  CUtlBuffer::PutShort( short s )
{
	PutType( s );
}

inline void CUtlBuffer::PutUnsignedShort( unsigned short s )
{
	PutType( s );
}

inline void CUtlBuffer::PutInt( int i )
{
	PutType( i );
}

inline void CUtlBuffer::PutInt64( int64 i )
{
	PutType( i );
}

inline void CUtlBuffer::PutUnsignedInt( unsigned int u )
{
	PutType( u );
}

inline void CUtlBuffer::PutFloat( float f )
{
	PutType( f );
}

inline void CUtlBuffer::PutDouble( double d )
{
	PutType( d );
}

inline void CUtlBuffer::PutPtr( void *p )
{
	// LEGACY WARNING: in text mode, PutPtr writes 32 bit pointers in hex, while GetPtr reads 32 or 64 bit pointers in decimal
	if (!IsText())
	{
		PutTypeBin( p );
	}
	else
	{
		Printf( "0x%p", p );
	}
}

//-----------------------------------------------------------------------------
// Am I a text buffer?
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsText() const 
{ 
	return (m_Flags & TEXT_BUFFER) != 0; 
}


//-----------------------------------------------------------------------------
// Can I grow if I'm externally allocated?
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsGrowable() const 
{ 
	return (m_Flags & EXTERNAL_GROWABLE) != 0; 
}


//-----------------------------------------------------------------------------
// Am I valid? (overflow or underflow error), Once invalid it stays invalid
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsValid() const 
{ 
	return m_Error == 0; 
}


//-----------------------------------------------------------------------------
// Do I contain carriage return/linefeeds? 
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::ContainsCRLF() const 
{ 
	return IsText() && ((m_Flags & CONTAINS_CRLF) != 0); 
} 


//-----------------------------------------------------------------------------
// Am I read-only
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsReadOnly() const
{
	return (m_Flags & READ_ONLY) != 0; 
}


//-----------------------------------------------------------------------------
// Buffer base and size
//-----------------------------------------------------------------------------
inline const void* CUtlBuffer::Base() const	
{ 
	return m_Memory.Base(); 
}

inline void* CUtlBuffer::Base()
{
	return m_Memory.Base(); 
}

inline int CUtlBuffer::Size() const			
{ 
	return m_Memory.NumAllocated(); 
}


//-----------------------------------------------------------------------------
// Clears out the buffer; frees memory
//-----------------------------------------------------------------------------
inline void CUtlBuffer::Clear()
{
	m_Get = 0;
	m_Put = 0;
	m_Error = 0;
	m_nOffset = 0;
	m_nMaxPut = -1;
	AddNullTermination( m_Put );
}

inline void CUtlBuffer::Purge()
{
	m_Get = 0;
	m_Put = 0;
	m_nOffset = 0;
	m_nMaxPut = 0;
	m_Error = 0;
	m_Memory.Purge();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline void *CUtlBuffer::AccessForDirectRead( int nBytes )
{
	Assert( m_Get == 0 && m_Put == 0 && m_nMaxPut == 0 );
	EnsureCapacity( nBytes );
	m_nMaxPut = nBytes;
	return Base();
}

inline void *CUtlBuffer::Detach()
{
	void *p = m_Memory.Detach();
	Clear();
	return p;
}

//-----------------------------------------------------------------------------

inline void CUtlBuffer::Spew( )
{
	SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	char pTmpLine[1024];
	while( IsValid() && GetBytesRemaining() )
	{
		V_memset( pTmpLine, 0, sizeof(pTmpLine) );
		Get( pTmpLine, MIN( ( size_t )GetBytesRemaining(), sizeof(pTmpLine)-1 ) );
		Msg( _T( "%s" ), pTmpLine );
	}
}

#if !defined(_GAMECONSOLE)
inline void CUtlBuffer::SwapCopy(  CUtlBuffer &other  )
{
	m_Get = other.m_Get;
	m_Put = other.m_Put;
	m_Error = other.m_Error;
	m_Flags = other.m_Flags;
	m_Reserved = other.m_Reserved;
	m_nTab = other.m_nTab;
	m_nMaxPut = other.m_nMaxPut;
	m_nOffset = other.m_nOffset;
	m_GetOverflowFunc = other.m_GetOverflowFunc;
	m_PutOverflowFunc = other.m_PutOverflowFunc;
	m_Byteswap = other.m_Byteswap;

	m_Memory.Swap( other.m_Memory );
}
#endif

#endif // UTLBUFFER_H

