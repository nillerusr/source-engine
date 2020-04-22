//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "../game/shared/choreoscene.h"
#include "../game/shared/choreoactor.h"
#include "../game/shared/choreochannel.h"
#include "../game/shared/choreoevent.h"
#include "../game/shared/iscenetokenprocessor.h"
#include "characterset.h"


//-----------------------------------------------------------------------------
// Purpose: Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	CSceneTokenProcessor();

	const char	*CurrentToken( void );
	bool		GetToken( bool crossline );
	bool		TokenAvailable( void );
	void		Error( const char *fmt, ... );
	void		SetBuffer( char *buffer );
private:

	const char *ParseNextToken (const char *data);

	const char	*m_pBuffer;
	char		m_szToken[ 1024 ];

	characterset_t	m_BreakSetIncludingColons;
};

CSceneTokenProcessor::CSceneTokenProcessor()
{
	CharacterSetBuild( &m_BreakSetIncludingColons, "{}()':" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CSceneTokenProcessor::CurrentToken( void )
{
	return m_szToken;
}

const char *CSceneTokenProcessor::ParseNextToken (const char *data)
{
	unsigned char    c;
	int             len;
	characterset_t	*breaks;
	
	breaks = &m_BreakSetIncludingColons;
	
	len = 0;
	m_szToken[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				m_szToken[len] = 0;
				return data;
			}
			m_szToken[len] = c;
			len++;
		}
	}

// parse single characters
	if ( IN_CHARACTERSET( *breaks, c ) )
	{
		m_szToken[len] = c;
		len++;
		m_szToken[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		m_szToken[len] = c;
		data++;
		len++;
		c = *data;
		if ( IN_CHARACTERSET( *breaks, c ) )
			break;
	} while (c>32);
	
	m_szToken[len] = 0;
	return data;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crossline - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::GetToken( bool crossline )
{
	// NOTE: crossline is ignored here, may need to implement if needed
	m_pBuffer = ParseNextToken( m_pBuffer );
	if ( m_szToken[0] )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::TokenAvailable( void )
{
	const char *search_p = m_pBuffer;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if ( !*search_p )
			return false;

	}

	if (*search_p == ';' || *search_p == '#' ||		 // semicolon and # is comment field
		(*search_p == '/' && *((search_p)+1) == '/')) // also make // a comment field
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::Error( const char *fmt, ... )
{
	char string[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( string, sizeof(string), fmt, argptr );
	va_end( argptr );

	Warning( "%s", string );
	Assert(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::SetBuffer( char *buffer )
{
	m_pBuffer = buffer;
}

CSceneTokenProcessor g_TokenProcessor;

ISceneTokenProcessor *GetTokenProcessor()
{
	return &g_TokenProcessor;
}

void SetTokenProcessorBuffer( const char *buf )
{
	g_TokenProcessor.SetBuffer( (char *)buf );
}

