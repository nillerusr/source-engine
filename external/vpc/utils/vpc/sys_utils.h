//======= Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
//	File Utilities.
//
//===========================================================================//

#ifdef _WIN32
#pragma once
#endif

#define _CRT_SECURE_NO_DEPRECATE 1

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#ifdef POSIX
#include <unistd.h>
#endif

#if defined( LINUX ) || defined( _LINUX )
	#include <sys/io.h>
#endif

#include "tier0/platform.h"
#include "../vpccrccheck/crccheck_shared.h"

template< class T, class NullType, int nMax >
class CSimplePointerStack
{
public:
	inline CSimplePointerStack()
	{
		m_nCount = 0;
	}

	inline void Purge()
	{
		for ( int i=0; i < m_nCount; i++ )
			m_Values[i] = (NullType)NULL;
		m_nCount = 0;
	}

	inline int Count()
	{
		return m_nCount;
	}

	inline T& Top()
	{
		Assert( m_nCount > 0 );
		return m_Values[m_nCount-1];
	}

	inline void Pop( T &val )
	{
		Assert( m_nCount > 0 );
		--m_nCount;
		val = m_Values[m_nCount];
		m_Values[m_nCount] = (NullType)NULL;
	}

	inline void Pop()
	{
		Assert( m_nCount > 0 );
		--m_nCount;
		m_Values[m_nCount] = (NullType)NULL;
	}

	inline void Push( T &val )
	{
		Assert( m_nCount+1 < nMax );
		m_Values[m_nCount] = val;
		++m_nCount;
	}

public:
	T m_Values[nMax];
	int m_nCount;
};

class CXMLWriter
{
public:
	CXMLWriter();

	bool		Open( const char *pFilename, bool bIs2010Format = false );
	void		Close();

	void		PushNode( const char *pName );
	void		PopNode( bool bEmitLabel );

	void		WriteLineNode( const char *pName, const char *pExtra, const char *pString );
	void		PushNode( const char *pName, const char *pString );

	void		Write( const char *p );
	CUtlString	FixupXMLString( const char *pInput );

private:
	void Indent();

	bool				m_b2010Format;
	FILE				*m_fp;

	CUtlString			m_FilenameString;

	CSimplePointerStack< char *, char *, 128 >	m_Nodes;
};

long	Sys_FileLength( const char* filename, bool bText = false );
int		Sys_LoadFile( const char *filename, void **bufferptr, bool bText = false );
bool	Sys_LoadFileIntoBuffer( const char *pchFileIn, CUtlBuffer &buf, bool bText );
void	Sys_StripPath( const char *path, char *outpath );
bool	Sys_Exists( const char *filename );
bool	Sys_Touch( const char *filename );
bool	Sys_FileInfo( const char *pFilename, int64 &nFileSize, int64 &nModifyTime );

bool	Sys_StringToBool( const char *pString );
bool	Sys_ReplaceString( const char *pStream, const char *pSearch, const char *pReplace, char *pOutBuff, int outBuffSize );
bool	Sys_StringPatternMatch( char const *pSrcPattern, char const *pString );

bool	Sys_EvaluateEnvironmentExpression( const char *pExpression, const char *pDefault, char *pOutBuff, int nOutBuffSize );

bool Sys_ExpandFilePattern( const char *pPattern, CUtlVector< CUtlString > &vecResults );
bool Sys_GetExecutablePath( char *pBuf, int cbBuf );

bool	Sys_CopyToMirror( const char *pFilename );
inline bool IsCFileExtension( const char *pExtension )
{
	if ( !pExtension )
		return false;

	return !V_stricmp( pExtension, "cpp" ) || 
		!V_stricmp( pExtension, "cxx" ) || 
		!V_stricmp( pExtension, "cc" ) || 
		!V_stricmp( pExtension, "c" );
}

bool	Sys_GetActualFilenameCase( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize );
bool	Sys_IsFilenameCaseConsistent( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize );
inline bool IsHFileExtension( const char *pExtension )
{
	if ( !pExtension )
		return false;

	return !V_stricmp( pExtension, "hpp" ) || 
		!V_stricmp( pExtension, "hxx" ) || 
		!V_stricmp( pExtension, "hh" ) || 
		!V_stricmp( pExtension, "h" );
}


