//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include <assert.h>
#include <io.h>
#include <stdio.h>
#include <malloc.h>
#include <windows.h>
#include "depcheck_util.h"
#include "icodeprocessor.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : depth - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void vprint( int depth, const char *fmt, ... )
{
	char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );

	FILE *fp = NULL;

	if ( processor->GetLogFile() )
	{
		fp = fopen( "log.txt", "ab" );
	}

	while ( depth-- > 0 )
	{
		printf( "  " );
		OutputDebugString( "  " );
		if ( fp )
		{
			fprintf( fp, "  " );
		}
	}

	::printf( "%s", string );
	OutputDebugString( string );

	if ( fp )
	{
		char *p = string;
		while ( *p )
		{
			if ( *p == '\n' )
			{
				fputc( '\r', fp );
			}
			fputc( *p, fp );
			p++;
		}
		fclose( fp );
	}
}


bool com_ignorecolons = false;  // YWB:  Ignore colons as token separators in COM_Parse
bool com_ignoreinlinecomment = false;
static bool s_com_token_unget = false;
char	com_token[1024];
int linesprocessed = 0;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CC_UngetToken( void )
{
	s_com_token_unget = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ch - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CC_IsBreakChar( char ch )
{
	bool brk = false;
	switch ( ch )
	{
	case '{':
	case '}':
	case ')':
	case '(':
	case '[':
	case ']':
	case '\'':
	case '/':
	case ',':
	case ';':
		brk = true;
		break;
		
	case ':':
		if ( !com_ignorecolons )
			brk = true;
		break;
	default:
		break;
	}
	return brk;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *data - 
// Output : char
//-----------------------------------------------------------------------------
char *CC_ParseToken(char *data)
{
	int             c;
	int             len;
	
	if ( s_com_token_unget )
	{
		s_com_token_unget = false;
		return data;
	}

	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
	
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			return NULL;                    // end of file;
		}
		if ( c== '\n' )
		{
			linesprocessed++;
		}
		data++;
	}
	
// skip // comments
	if ( !com_ignoreinlinecomment )
	{
		if (c=='/' && data[1] == '/')
		{
			while (*data && *data != '\n')
			{
				data++;
			}
			goto skipwhite;
		}
	}
	
	if ( c == '/' && data[1] == '*' )
	{
		while (data[0] && data[1] && !( data[0] == '*' && data[1] == '/' ) )
		{
			if ( *data == '\n' )
			{
				linesprocessed++;
			}
			data++;
		}

		if ( data[0] == '*' && data[1] == '/' )
		{
			data+=2;
		}
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c || c =='\n' || len >= (sizeof( com_token ) - 1 ))
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if ( CC_IsBreakChar( c ) )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if ( CC_IsBreakChar( c ) )
			break;
	} while (c>32);
	com_token[len] = 0;
	return data;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			*len - 
// Output : unsigned char
//-----------------------------------------------------------------------------

static int allocated = 0;

unsigned char *COM_LoadFile( const char *name, int *len)
{
	FILE *fp;
	fp = fopen( name, "rb" );
	if ( !fp )
	{
		*len = 0;
		return NULL;
	}

	fseek( fp, 0, SEEK_END );
	*len = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	
	unsigned char *buffer = new unsigned char[ *len + 1 ];
	fread( buffer, *len, 1, fp );
	fclose( fp );
	buffer[ *len ] = 0;

	size_t nsize = _msize( buffer );
	assert( nsize <= 5000000 );
	allocated += nsize;


	return buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void COM_FreeFile( unsigned char *buffer )
{
	size_t nsize = _msize( buffer );
	assert( nsize <= 5000000 );

	allocated -= nsize;

	delete[] buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *dir - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COM_DirectoryExists( const char *dir )
{
	if ( !_access( dir, 0 ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *input - 
// Output : char
//-----------------------------------------------------------------------------
char *CC_ParseUntilEndOfLine( char *input )
{
	while (*input && *input != '\n')
		input++;

	return input;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *input - 
//			*ch - 
//			*breakchar - 
// Output : char
//-----------------------------------------------------------------------------
char *CC_RawParseChar( char *input, const char *ch, char *breakchar )
{
	bool done = false;
	int listlen = strlen( ch );

	do
	{
		input = CC_ParseToken( input );
		if ( strlen( com_token ) <= 0 )
			break;

		if ( strlen( com_token ) == 1 )
		{
			for ( int i = 0; i < listlen; i++ )
			{
				if ( com_token[ 0 ] == ch[ i ] )
				{
					*breakchar = ch [ i ];
					done = true;
					break;
				}
			}
		}
	} while ( !done );

	return input;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *input - 
//			*pairing - 
// Output : char
//-----------------------------------------------------------------------------
char *CC_DiscardUntilMatchingCharIncludingNesting( char *input, const char *pairing )
{
	int nestcount = 1;

	do
	{
		input = CC_ParseToken( input );
		if ( strlen( com_token ) <= 0 )
			break;

		if ( strlen( com_token ) == 1 )
		{
			if ( com_token[ 0 ] == pairing[ 0 ] )
			{
				nestcount++;
			}
			else if ( com_token[ 0 ] == pairing[ 1 ] )
			{
				nestcount--;
			}
		}
	} while ( nestcount != 0 );

	return input;
}