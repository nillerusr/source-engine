//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//

#include "pch_tier0.h"
#include "tier0/icommandline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tier0/dbg.h"

#include "tier0/memdbgon.h"

#ifdef POSIX
#include <limits.h>
#define _MAX_PATH PATH_MAX
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static const int MAX_PARAMETER_LEN = 128;

//-----------------------------------------------------------------------------
// Purpose: Implements ICommandLine
//-----------------------------------------------------------------------------
class CCommandLine : public ICommandLine
{
public:
	// Construction
						CCommandLine( void );
	virtual 			~CCommandLine( void );

	// Implements ICommandLine
	virtual void		CreateCmdLine( const char *commandline  );
	virtual void		CreateCmdLine( int argc, char **argv );
	virtual const char	*GetCmdLine( void ) const;
	virtual	const char	*CheckParm( const char *psz, const char **ppszValue = 0 ) const;
	// A bool return of whether param exists, useful for just checking if param that is just a flag is set
	virtual bool		HasParm( const char *psz ) const;

	virtual void		RemoveParm( const char *parm );
	virtual void		AppendParm( const char *pszParm, const char *pszValues );

	virtual int			ParmCount() const;
	virtual int			FindParm( const char *psz ) const;
	virtual const char* GetParm( int nIndex ) const;

	virtual const char	*ParmValue( const char *psz, const char *pDefaultVal = NULL ) const OVERRIDE;
	virtual int			ParmValue( const char *psz, int nDefaultVal ) const OVERRIDE;
	virtual float		ParmValue( const char *psz, float flDefaultVal ) const OVERRIDE;
	virtual const char *ParmValueByIndex( int nIndex, const char *pDefaultVal = 0 ) const OVERRIDE;

	virtual void        SetParm( int nIndex, char const *pParm );

private:
	enum
	{
		MAX_PARAMETER_LEN = 128,
		MAX_PARAMETERS = 256,
	};

	// When the commandline contains @name, it reads the parameters from that file
	void LoadParametersFromFile( const char *&pSrc, char *&pDst, int maxDestLen, bool bInQuotes );

	// Parse command line...
	void ParseCommandLine();

	// Frees the command line arguments
	void CleanUpParms();

	// Adds an argument..
	void AddArgument( const char *pFirst, const char *pLast );

	// Copy of actual command line
	char *m_pszCmdLine;

	// Pointers to each argument...
	int m_nParmCount;
	char *m_ppParms[MAX_PARAMETERS];
};


//-----------------------------------------------------------------------------
// Instance singleton and expose interface to rest of code
//-----------------------------------------------------------------------------
static CCommandLine g_CmdLine;
ICommandLine *CommandLine()
{
	return &g_CmdLine;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommandLine::CCommandLine( void )
{
	m_pszCmdLine = NULL;
	m_nParmCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommandLine::~CCommandLine( void )
{
	CleanUpParms();
	delete[] m_pszCmdLine;
}


//-----------------------------------------------------------------------------
// Read commandline from file instead...
//-----------------------------------------------------------------------------
void CCommandLine::LoadParametersFromFile( const char *&pSrc, char *&pDst, int maxDestLen, bool bInQuotes )
{
	// Suck out the file name
	char szFileName[ _MAX_PATH ];
	char *pOut;
	char *pDestStart = pDst;

	if ( maxDestLen < 3 )
		return;

	// Skip the @ sign
	pSrc++;

	pOut = szFileName;

	char terminatingChar = ' ';
	if ( bInQuotes )
		terminatingChar = '\"';

	while ( *pSrc && *pSrc != terminatingChar )
	{
		*pOut++ = *pSrc++;
		if ( (pOut - szFileName) >= (_MAX_PATH-1) )
			break;
	}

	*pOut = '\0';

	// Skip the space after the file name
	if ( *pSrc )
		pSrc++;

	// Now read in parameters from file
	FILE *fp = fopen( szFileName, "r" );
	if ( fp )
	{
		char c;
		c = (char)fgetc( fp );
		while ( c != EOF )
		{
			// Turn return characters into spaces
			if ( c == '\n' )
				c = ' ';

			*pDst++ = c;
			
			// Don't go past the end, and allow for our terminating space character AND a terminating null character.
			if ( (pDst - pDestStart) >= (maxDestLen-2) )
				break;

			// Get the next character, if there are more
			c = (char)fgetc( fp );
		}
	
		// Add a terminating space character
		*pDst++ = ' ';

		fclose( fp );
	}
	else
	{
		printf( "Parameter file '%s' not found, skipping...", szFileName );
	}
}


//-----------------------------------------------------------------------------
// Creates a command line from the arguments passed in
//-----------------------------------------------------------------------------
void CCommandLine::CreateCmdLine( int argc, char **argv )
{
	char cmdline[ 2048 ];
	cmdline[ 0 ] = 0;

	char *dest = cmdline;
	size_t size = sizeof( cmdline );
	const char *space = "";

	for ( int i = 0; i < argc; ++i )
	{
		// We need room for: space, arg, 2 quotes, and a nil.
		Assert( strlen( space ) + strlen( argv[ i ] ) + 2 + 1 <= size );

		if ( size )
		{
			_snprintf( dest, size, "%s\"%s\"", space, argv[ i ] );
			dest[ size - 1 ] = 0;
		}

		size_t len = strlen( dest );
		size -= len;
		dest += len;
		space = " ";
	}

	CreateCmdLine( cmdline );
}


//-----------------------------------------------------------------------------
// Purpose: Create a command line from the passed in string
//  Note that if you pass in a @filename, then the routine will read settings
//  from a file instead of the command line
//-----------------------------------------------------------------------------
void CCommandLine::CreateCmdLine( const char *commandline )
{
	if ( m_pszCmdLine )
	{
		delete[] m_pszCmdLine;
	}

	char szFull[ 4096 ];
	szFull[0] = '\0';

	char *pDst = szFull;
	const char *pSrc = commandline;

	bool bInQuotes = false;
	const char *pInQuotesStart = 0;
	while ( *pSrc )
	{
		// Is this an unslashed quote?
		if ( *pSrc == '"' )
		{
			if ( pSrc == commandline || ( pSrc[-1] != '/' && pSrc[-1] != '\\' ) )
			{
				bInQuotes = !bInQuotes;
				pInQuotesStart = pSrc + 1;
			}
		}

		if ( *pSrc == '@' )
		{
			if ( pSrc == commandline || (!bInQuotes && isspace( pSrc[-1] )) || (bInQuotes && pSrc == pInQuotesStart) )
			{
				LoadParametersFromFile( pSrc, pDst, sizeof( szFull ) - (pDst - szFull), bInQuotes );
				continue;
			}
		}	
		
		// Don't go past the end.
		if ( (pDst - szFull) >= (sizeof( szFull ) - 1) )
			break;

		*pDst++ = *pSrc++;
	}

	*pDst = '\0';

	int len = strlen( szFull ) + 1;
	m_pszCmdLine = new char[len];
	memcpy( m_pszCmdLine, szFull, len );

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Finds a string in another string with a case insensitive test
//-----------------------------------------------------------------------------
static char * _stristr( char * pStr, const char * pSearch )
{
	AssertValidStringPtr(pStr);
	AssertValidStringPtr(pSearch);

	if (!pStr || !pSearch) 
		return 0;

	char* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower((unsigned char)*pLetter) == tolower((unsigned char)*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (tolower((unsigned char)*pMatch) != tolower((unsigned char)*pTest))
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


//-----------------------------------------------------------------------------
// Purpose: Remove specified string ( and any args attached to it ) from command line
// Input  : *pszParm - 
//-----------------------------------------------------------------------------
void CCommandLine::RemoveParm( const char *pszParm )
{
	if ( !m_pszCmdLine )
		return;

	// Search for first occurrence of pszParm
	char *p, *found;
	char *pnextparam;
	int n;
	int curlen;

	p = m_pszCmdLine;
	while ( *p )
	{
		curlen = strlen( p );

		found = _stristr( p, pszParm );
		if ( !found )
			break;
			
		pnextparam = found + 1;
		bool bHadQuote = false;
		if ( found > m_pszCmdLine && found[-1] == '\"' )
			bHadQuote = true;
		
		while ( pnextparam && *pnextparam && (*pnextparam != ' ') && (*pnextparam != '\"') )
			pnextparam++;

		if ( pnextparam && ( static_cast<size_t>( pnextparam - found ) > strlen( pszParm ) ) )
		{
			p = pnextparam;
			continue;
		}

		while ( pnextparam && *pnextparam && (*pnextparam != '-') && (*pnextparam != '+') )
			pnextparam++;

		if ( bHadQuote )
		{
			found--;
		}

		if ( pnextparam && *pnextparam )
		{
			// We are either at the end of the string, or at the next param.  Just chop out the current param.
			n = curlen - ( pnextparam - p ); // # of characters after this param.
			memmove( found, pnextparam, n );

			found[n] = '\0';
		}
		else
		{
			// Clear out rest of string.
			n = pnextparam - found;
			memset( found, 0, n );
		}
	}

	// Strip and trailing ' ' characters left over.
	while ( 1 )
	{
		int len = strlen( m_pszCmdLine );
		if ( len == 0 || m_pszCmdLine[ len - 1 ] != ' ' )
			break;
		
		m_pszCmdLine[len - 1] = '\0';
	}

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Purpose: Append parameter and argument values to command line
// Input  : *pszParm - 
//			*pszValues - 
//-----------------------------------------------------------------------------
void CCommandLine::AppendParm( const char *pszParm, const char *pszValues )
{
	int nNewLength = 0;
	char *pCmdString;

	nNewLength = strlen( pszParm );            // Parameter.
	if ( pszValues )
		nNewLength += strlen( pszValues ) + 1;  // Values + leading space character.
	nNewLength++; // Terminal 0;

	if ( !m_pszCmdLine )
	{
		m_pszCmdLine = new char[ nNewLength ];
		strcpy( m_pszCmdLine, pszParm );
		if ( pszValues )
		{
			strcat( m_pszCmdLine, " " );
			strcat( m_pszCmdLine, pszValues );
		}

		ParseCommandLine();
		return;
	}

	// Remove any remnants from the current Cmd Line.
	RemoveParm( pszParm );

	nNewLength += strlen( m_pszCmdLine ) + 1 + 1;

	pCmdString = new char[ nNewLength ];
	memset( pCmdString, 0, nNewLength );

	strcpy ( pCmdString, m_pszCmdLine ); // Copy old command line.
	strcat ( pCmdString, " " ); // Put in a space
	strcat ( pCmdString, pszParm );
	if ( pszValues )
	{
		strcat( pCmdString, " " );
		strcat( pCmdString, pszValues );
	}

	// Kill off the old one
	delete[] m_pszCmdLine;

	// Point at the new command line.
	m_pszCmdLine = pCmdString;

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Purpose: Return current command line
// Output : const char
//-----------------------------------------------------------------------------
const char *CCommandLine::GetCmdLine( void ) const
{
	return m_pszCmdLine;
}


//-----------------------------------------------------------------------------
// Purpose: Search for the parameter in the current commandline
// Input  : *psz - 
//			**ppszValue - 
// Output : char
//-----------------------------------------------------------------------------
const char *CCommandLine::CheckParm( const char *psz, const char **ppszValue ) const
{
	if ( ppszValue )
		*ppszValue = NULL;
	
	int i = FindParm( psz );
	if ( i == 0 )
		return NULL;
	
	if ( ppszValue )
	{
		if ( (i+1) >= m_nParmCount )
		{
			*ppszValue = NULL;
		}
		else
		{
			*ppszValue = m_ppParms[i+1];
		}
	}
	
	return m_ppParms[i];
}


//-----------------------------------------------------------------------------
// Adds an argument..
//-----------------------------------------------------------------------------
void CCommandLine::AddArgument( const char *pFirst, const char *pLast )
{
	if ( pLast <= pFirst )
		return;

	if ( m_nParmCount >= MAX_PARAMETERS )
		Error( "CCommandLine::AddArgument: exceeded %d parameters", MAX_PARAMETERS );

	size_t nLen = pLast - pFirst + 1;
	m_ppParms[m_nParmCount] = new char[nLen];
	memcpy( m_ppParms[m_nParmCount], pFirst, nLen - 1 );
	m_ppParms[m_nParmCount][nLen - 1] = 0;

	++m_nParmCount;
}


//-----------------------------------------------------------------------------
// Parse command line...
//-----------------------------------------------------------------------------
void CCommandLine::ParseCommandLine()
{
	CleanUpParms();
	if (!m_pszCmdLine)
		return;

	const char *pChar = m_pszCmdLine;
	while ( *pChar && isspace(*pChar) )
	{
		++pChar;
	}

	bool bInQuotes = false;
	const char *pFirstLetter = NULL;
	for ( ; *pChar; ++pChar )
	{
		if ( bInQuotes )
		{
			if ( *pChar != '\"' )
				continue;

			AddArgument( pFirstLetter, pChar );
			pFirstLetter = NULL;
			bInQuotes = false;
			continue;
		}

		// Haven't started a word yet...
		if ( !pFirstLetter )
		{
			if ( *pChar == '\"' )
			{
				bInQuotes = true;
				pFirstLetter = pChar + 1;
				continue;
			}

			if ( isspace( *pChar ) )
				continue;

			pFirstLetter = pChar;
			continue;
		}

		// Here, we're in the middle of a word. Look for the end of it.
		if ( isspace( *pChar ) )
		{
			AddArgument( pFirstLetter, pChar );
			pFirstLetter = NULL;
		}
	}

	if ( pFirstLetter )
	{
		AddArgument( pFirstLetter, pChar );
	}
}


//-----------------------------------------------------------------------------
// Individual command line arguments
//-----------------------------------------------------------------------------
void CCommandLine::CleanUpParms()
{
	for ( int i = 0; i < m_nParmCount; ++i )
	{
		delete [] m_ppParms[i];
		m_ppParms[i] = NULL;
	}
	m_nParmCount = 0;
}


//-----------------------------------------------------------------------------
// Returns individual command line arguments
//-----------------------------------------------------------------------------
int CCommandLine::ParmCount() const
{
	return m_nParmCount;
}

int CCommandLine::FindParm( const char *psz ) const
{
	// Start at 1 so as to not search the exe name
	for ( int i = 1; i < m_nParmCount; ++i )
	{
		if ( !_stricmp( psz, m_ppParms[i] ) )
			return i;
	}
	return 0;
}

bool CCommandLine::HasParm( const char *psz ) const
{
	return ( FindParm( psz ) != 0 );
}

const char* CCommandLine::GetParm( int nIndex ) const
{
	Assert( (nIndex >= 0) && (nIndex < m_nParmCount) );
	if ( (nIndex < 0) || (nIndex >= m_nParmCount) )
		return "";
	return m_ppParms[nIndex];
}
void CCommandLine::SetParm( int nIndex, char const *pParm )
{
	if ( pParm )
	{
		Assert( (nIndex >= 0) && (nIndex < m_nParmCount) );
		if ( (nIndex >= 0) && (nIndex < m_nParmCount) )
		{
			if ( m_ppParms[nIndex] )
				delete[] m_ppParms[nIndex];
			m_ppParms[nIndex] = strdup( pParm );
		}

	}

}


//-----------------------------------------------------------------------------
// Returns the argument after the one specified, or the default if not found
//-----------------------------------------------------------------------------
const char *CCommandLine::ParmValue( const char *psz, const char *pDefaultVal ) const
{
	int nIndex = FindParm( psz );
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return pDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return pDefaultVal;

	return m_ppParms[nIndex + 1];
}

int	CCommandLine::ParmValue( const char *psz, int nDefaultVal ) const
{
	int nIndex = FindParm( psz );
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return nDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return nDefaultVal;

	return atoi( m_ppParms[nIndex + 1] );
}

float CCommandLine::ParmValue( const char *psz, float flDefaultVal ) const
{
	int nIndex = FindParm( psz );
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return flDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return flDefaultVal;

	return atof( m_ppParms[nIndex + 1] );
}
const char *CCommandLine::ParmValueByIndex( int nIndex, const char *pDefaultVal ) const
{
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return pDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return pDefaultVal;

	return m_ppParms[nIndex + 1];
}

