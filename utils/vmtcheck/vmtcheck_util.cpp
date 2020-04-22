//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include "vmtcheck_util.h"
#include "tier0/dbg.h"

extern bool uselogfile;

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

	if ( uselogfile )
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


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			*len - 
// Output : unsigned char
//-----------------------------------------------------------------------------
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

	return buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void COM_FreeFile( unsigned char *buffer )
{
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
