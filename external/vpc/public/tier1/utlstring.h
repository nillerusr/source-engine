//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef UTLSTRING_H
#define UTLSTRING_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/utlmemory.h"
#include "tier1/strtools.h"
#include "limits.h"


//-----------------------------------------------------------------------------
// Base class, containing simple memory management
//-----------------------------------------------------------------------------
class CUtlBinaryBlock
{
public:
	CUtlBinaryBlock( int growSize = 0, int initSize = 0 );
	~CUtlBinaryBlock()
	{
#ifdef _DEBUG
		m_nActualLength = 0x7BADF00D;
#else
		m_nActualLength = 0;
#endif
	}

	// NOTE: nInitialLength indicates how much of the buffer starts full
	CUtlBinaryBlock( void* pMemory, int nSizeInBytes, int nInitialLength );
	CUtlBinaryBlock( const void* pMemory, int nSizeInBytes );
	CUtlBinaryBlock( const CUtlBinaryBlock& src );

	void		Get( void *pValue, int nMaxLen ) const;
	void		Set( const void *pValue, int nLen );
	const void	*Get( ) const;
	void		*Get( );

	unsigned char& operator[]( int i );
	const unsigned char& operator[]( int i ) const;

	int			Length() const;
	void		SetLength( int nLength );	// Undefined memory will result
	bool		IsEmpty() const;
	void		Clear();
	void		Purge();

	bool		IsReadOnly() const;

	CUtlBinaryBlock &operator=( const CUtlBinaryBlock &src );

	// Test for equality
	bool operator==( const CUtlBinaryBlock &src ) const;

private:
	CUtlMemory<unsigned char> m_Memory;
	int m_nActualLength;
};


//-----------------------------------------------------------------------------
// class inlines
//-----------------------------------------------------------------------------
inline const void *CUtlBinaryBlock::Get( ) const
{
	return m_Memory.Base();
}

inline void *CUtlBinaryBlock::Get( )
{
	return m_Memory.Base();
}

inline int CUtlBinaryBlock::Length() const
{
	return m_nActualLength;
}

inline unsigned char& CUtlBinaryBlock::operator[]( int i )
{
	return m_Memory[i];
}

inline const unsigned char& CUtlBinaryBlock::operator[]( int i ) const
{
	return m_Memory[i];
}

inline bool CUtlBinaryBlock::IsReadOnly() const
{
	return m_Memory.IsReadOnly();
}

inline bool CUtlBinaryBlock::IsEmpty() const
{
	return Length() == 0;
}

inline void CUtlBinaryBlock::Clear()
{
	SetLength( 0 );
}

inline void CUtlBinaryBlock::Purge()
{
	SetLength( 0 );
	m_Memory.Purge();
}


//-----------------------------------------------------------------------------
// Simple string class. 
// NOTE: This is *not* optimal! Use in tools, but not runtime code
//-----------------------------------------------------------------------------
class CUtlString
{
public:
	typedef enum
	{
		PATTERN_NONE		= 0x00000000,
		PATTERN_DIRECTORY	= 0x00000001
	} TUtlStringPattern;

public:
	CUtlString();
	CUtlString( const char *pString );
	CUtlString( const CUtlString& string );

	// Attaches the string to external memory. Useful for avoiding a copy
	CUtlString( void* pMemory, int nSizeInBytes, int nInitialLength );
	CUtlString( const void* pMemory, int nSizeInBytes );

	const char	*Get( ) const;
	void		Set( const char *pValue );

	void Clear() { Set( NULL ); }

	// Converts to c-strings
	operator const char*() const;

	// for compatibility switching items from UtlSymbol
	const char  *String() const { return Get(); }

	// Returns strlen
	int			Length() const;
	bool		IsEmpty() const;

	// Sets the length (used to serialize into the buffer )
	// Note: If nLen != 0, then this adds an extra byte for a null-terminator.	
	void		SetLength( int nLen );
	char		*Get();
	void		Purge();

	// Case Change
	void		ToLower( );

	void		Append( const char *pchAddition );

	// Strips the trailing slash
	void		StripTrailingSlash();

	CUtlString &operator=( const CUtlString &src );
	CUtlString &operator=( const char *src );

	// Test for equality
	bool operator==( const CUtlString &src ) const;
	bool operator==( const char *src ) const;
	bool operator!=( const CUtlString &src ) const { return !operator==( src ); }
	bool operator!=( const char *src ) const { return !operator==( src ); }

	// If these are not defined, CUtlString as rhs will auto-convert
	// to const char* and do logical operations on the raw pointers. Ugh.
	inline friend bool operator==( const char *lhs, const CUtlString &rhs ) { return rhs.operator==( lhs ); }
	inline friend bool operator!=( const char *lhs, const CUtlString &rhs ) { return rhs.operator!=( lhs ); }

	CUtlString &operator+=( const CUtlString &rhs );
	CUtlString &operator+=( const char *rhs );
	CUtlString &operator+=( char c );
	CUtlString &operator+=( int rhs );
	CUtlString &operator+=( double rhs );
	
	CUtlString operator+( const char *pOther ) const;

	bool MatchesPattern( const CUtlString &Pattern, int nFlags = 0 ) const;		// case SENSITIVE, use * for wildcard in pattern string

	int Format( const char *pFormat, ... );
	void SetDirect( const char *pValue, int nChars );

	// Defining AltArgumentType_t hints that associative container classes should
	// also implement Find/Insert/Remove functions that take const char* params.
	typedef const char *AltArgumentType_t;

	// Take a piece out of the string.
	// If you only specify nStart, it'll go from nStart to the end.
	// You can use negative numbers and it'll wrap around to the start.
	CUtlString Slice( int32 nStart=0, int32 nEnd=INT_MAX ) const;

	// Grab a substring starting from the left or the right side.
	CUtlString Left( int32 nChars ) const;
	CUtlString Right( int32 nChars ) const;

	// Replace all instances of one character with another.
	CUtlString Replace( char cFrom, char cTo ) const;

	// Calls right through to V_MakeAbsolutePath.
	CUtlString AbsPath( const char *pStartingDir=NULL ) const;	

	// Gets the filename (everything except the path.. c:\a\b\c\somefile.txt -> somefile.txt).
	CUtlString UnqualifiedFilename() const;
	
	// Strips off one directory. Uses V_StripLastDir but strips the last slash also!
	CUtlString DirName() const;
	
	// Works like V_ComposeFileName.
	static CUtlString PathJoin( const char *pStr1, const char *pStr2 );

	// These can be used for utlvector sorts.
	static int __cdecl SortCaseInsensitive( const CUtlString *pString1, const CUtlString *pString2 );
	static int __cdecl SortCaseSensitive( const CUtlString *pString1, const CUtlString *pString2 );

private:
	CUtlBinaryBlock m_Storage;
};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline bool CUtlString::IsEmpty() const
{
	return Length() == 0;
}

inline int __cdecl CUtlString::SortCaseInsensitive( const CUtlString *pString1, const CUtlString *pString2 )
{
	return V_stricmp( pString1->String(), pString2->String() );
}

inline int __cdecl CUtlString::SortCaseSensitive( const CUtlString *pString1, const CUtlString *pString2 )
{
	return V_strcmp( pString1->String(), pString2->String() );
}

//-----------------------------------------------------------------------------
// Purpose: Implementation of low-level string functionality for character types.
//-----------------------------------------------------------------------------

template < typename T >
class StringFuncs
{
public:
	static T		*Duplicate( const T *pValue );
	static void		 Copy( T *out_pOut, const T *pIn, int iLength );
	static int		 Compare( const T *pLhs, const T *pRhs );
	static int		 Length( const T *pValue );
	static const T  *FindChar( const T *pStr, const T cSearch );
	static const T	*EmptyString();
};

template < >
class StringFuncs<char>
{
public:
	static char		  *Duplicate( const char *pValue ) { return strdup( pValue ); }
	static void		   Copy( char *out_pOut, const char *pIn, int iLength ) { strncpy( out_pOut, pIn, iLength ); }
	static int		   Compare( const char *pLhs, const char *pRhs ) { return strcmp( pLhs, pRhs ); }
	static int		   Length( const char *pValue ) { return strlen( pValue ); }
	static const char *FindChar( const char *pStr, const char cSearch ) { return strchr( pStr, cSearch ); }
	static const char *EmptyString() { return ""; }
};

template < >
class StringFuncs<wchar_t>
{
public:
	static wchar_t		 *Duplicate( const wchar_t *pValue ) { return wcsdup( pValue ); }
	static void			  Copy( wchar_t *out_pOut, const wchar_t  *pIn, int iLength ) { wcsncpy( out_pOut, pIn, iLength ); }
	static int			  Compare( const wchar_t *pLhs, const wchar_t *pRhs ) { return wcscmp( pLhs, pRhs ); }
	static int			  Length( const wchar_t *pValue ) { return wcslen( pValue ); }
	static const wchar_t *FindChar( const wchar_t *pStr, const wchar_t cSearch ) { return wcschr( pStr, cSearch ); }
	static const wchar_t *EmptyString() { return L""; }
};

//-----------------------------------------------------------------------------
// Dirt-basic auto-release string class. Not intended for manipulation,
// can be stored in a container or forwarded as a functor parameter.
// Note the benefit over CUtlString: sizeof(CUtlConstString) == sizeof(char*).
// Also note: null char* pointers are treated identically to empty strings.
//-----------------------------------------------------------------------------

template < typename T = char >
class CUtlConstStringBase
{
public:
	CUtlConstStringBase() : m_pString( NULL ) {}
	CUtlConstStringBase( const T *pString ) : m_pString( NULL ) { Set( pString ); }
	CUtlConstStringBase( const CUtlConstStringBase& src ) : m_pString( NULL ) { Set( src.m_pString ); }
	~CUtlConstStringBase() { Set( NULL ); }

	void Set( const T *pValue );
	void Clear() { Set( NULL ); }

	const T *Get() const { return m_pString ? m_pString : StringFuncs<T>::EmptyString(); }
	operator const T*() const { return m_pString ? m_pString : StringFuncs<T>::EmptyString(); }

	bool IsEmpty() const { return m_pString == NULL; } // Note: empty strings are never stored by Set

	int Compare( const T *rhs ) const;

	// Logical ops
	bool operator<( const T *rhs ) const { return Compare( rhs ) < 0; }
	bool operator==( const T *rhs ) const { return Compare( rhs ) == 0; }
	bool operator!=( const T *rhs ) const { return Compare( rhs ) != 0; }
	bool operator<( const CUtlConstStringBase &rhs ) const { return Compare( rhs.m_pString ) < 0; }
	bool operator==( const CUtlConstStringBase &rhs ) const { return Compare( rhs.m_pString ) == 0; }
	bool operator!=( const CUtlConstStringBase &rhs ) const { return Compare( rhs.m_pString ) != 0; }

	// If these are not defined, CUtlConstString as rhs will auto-convert
	// to const char* and do logical operations on the raw pointers. Ugh.
	inline friend bool operator<( const T *lhs, const CUtlConstStringBase &rhs ) { return rhs.Compare( lhs ) > 0; }
	inline friend bool operator==( const T *lhs, const CUtlConstStringBase &rhs ) { return rhs.Compare( lhs ) == 0; }
	inline friend bool operator!=( const T *lhs, const CUtlConstStringBase &rhs ) { return rhs.Compare( lhs ) != 0; }

	CUtlConstStringBase &operator=( const T *src ) { Set( src ); return *this; }
	CUtlConstStringBase &operator=( const CUtlConstStringBase &src ) { Set( src.m_pString ); return *this; }

	// Defining AltArgumentType_t is a hint to containers that they should
	// implement Find/Insert/Remove functions that take const char* params.
	typedef const T *AltArgumentType_t;

protected:
	const T *m_pString;
};

template < typename T >
void CUtlConstStringBase<T>::Set( const T *pValue )
{
	if ( pValue != m_pString )
	{
		free( ( void* ) m_pString );
		m_pString = pValue && pValue[0] ? StringFuncs<T>::Duplicate( pValue ) : NULL;
	}
}

template < typename T >
int CUtlConstStringBase<T>::Compare( const T *rhs ) const
{
	if ( m_pString )
	{
		if ( rhs )
			return StringFuncs<T>::Compare( m_pString, rhs );
		else
			return 1;
	}
	else
	{
		if ( rhs )
			return -1;
		else
			return 0;
	}
}

typedef	CUtlConstStringBase<char>		CUtlConstString;
typedef	CUtlConstStringBase<wchar_t>	CUtlConstWideString;


#endif // UTLSTRING_H
