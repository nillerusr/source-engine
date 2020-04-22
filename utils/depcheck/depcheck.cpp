//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <windows.h>
#include "depcheck_util.h"
#include "icodeprocessor.h"
#include "tier1/strtools.h"

char    *va(char *format, ...)
{
	va_list         argptr;
	static char             string[1024];
	
	va_start (argptr, format);
	Q_vsnprintf( string, 1024, format, argptr );
	va_end (argptr);

	return string;  
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  depcheck -q -l <root source directory> <game:  hl2 | tf2>\n\
		\t-q = quiet\n\
		\t-l = log to file log.txt\n" );

	// Exit app
	exit( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sourcetreebase - 
//			*subdir - 
//			*baseentityclass - 
//-----------------------------------------------------------------------------
void ProcessDirectory( const char *game, const char *sourcetreebase, const char *subdir, const char *dsp, const char *config )
{
	char rootdirectory[ 256 ];
	sprintf( rootdirectory, "%s\\%s", sourcetreebase, subdir );

	// check for existence
	if ( COM_DirectoryExists( rootdirectory ) )
	{
		processor->Process( game, rootdirectory, dsp, config );
	}
	else
	{
		vprint( 0, "Couldn't find directory %s, check path %s\n", rootdirectory, sourcetreebase );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckLogFile( void )
{
	if ( processor->GetLogFile() )
	{
		_unlink( "log.txt" );
		vprint( 0, "    Outputting to log.txt\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	vprint( 0, "Valve Software - depcheck.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Game Code Simple Build Dependency Analyzer ---\n" );

	int i=1;
	for ( i ; i<argc ; i++)
	{
		if ( argv[ i ][ 0 ] == '-' )
		{
			switch( argv[ i ][ 1 ] )
			{
			case 'q':
				processor->SetQuiet( true );
				break;
			case 'l':
				processor->SetLogFile( true );
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 3 || ( i != argc ) )
	{
		printusage();
	}

	CheckLogFile();

	vprint( 0, "    Looking for unneeded header includes...\n" );

	char sourcetreebase[ 256 ];
	strcpy( sourcetreebase, argv[i-2] );

	Q_StripTrailingSlash( sourcetreebase );

//	ProcessDirectory( argv[ i-1 ], sourcetreebase, "engine", "engdll.dsp", "quiver - Win32 GL Debug" );
//	ProcessDirectory( argv[ i-1 ], sourcetreebase, "dlls", "hl.dsp", va( "hl - Win32 %s Debug", argv[ i - 1 ] ) );
	ProcessDirectory( argv[ i-1 ], sourcetreebase, "cl_dll", "client.dsp", va( "client - Win32 Debug %s", argv[ i - 1 ] ) );

	return 0;
}
