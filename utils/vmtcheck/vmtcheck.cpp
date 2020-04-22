//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "vmtcheck_util.h"
#include "tier0/dbg.h"
#include "utldict.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "KeyValues.h"
#include "cmdlib.h"

bool uselogfile = false;

struct AnalysisData
{
	CUtlSymbolTable				symbols;
};

static AnalysisData g_Analysis;


static bool spewed = false;

SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	spewed = true;

	printf( "%s", pMsg );
	OutputDebugString( pMsg );
	
	if ( type == SPEW_ERROR )
	{
		printf( "\n" );
		OutputDebugString( "\n" );
	}

	return SPEW_CONTINUE;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  vmtcheck <materials/.vmt root directory>\n\
		\t-v = verbose output\n\
		\t-l = log to file log.txt\n\
		\ne.g.:  vmtcheck -l u:/hl2/hl2/materials\n" );

	// Exit app
	exit( 1 );
}

void BuildFileList_R( CUtlVector< CUtlSymbol >& files, char const *dir, char const *extension )
{
	WIN32_FIND_DATA wfd;

	char directory[ 256 ];
	char filename[ 256 ];
	HANDLE ff;

	sprintf( directory, "%s\\*.*", dir );

	if ( ( ff = FindFirstFile( directory, &wfd ) ) == INVALID_HANDLE_VALUE )
		return;

	int extlen = strlen( extension );

	do
	{
		if ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{

			if ( wfd.cFileName[ 0 ] == '.' )
				continue;

			// Recurse down directory
			sprintf( filename, "%s\\%s", dir, wfd.cFileName );
			BuildFileList_R( files, filename, extension );
		}
		else
		{
			int len = strlen( wfd.cFileName );
			if ( len > extlen )
			{
				if ( !stricmp( &wfd.cFileName[ len - extlen ], extension ) )
				{
					char filename[ MAX_PATH ];
					Q_snprintf( filename, sizeof( filename ), "%s\\%s", dir, wfd.cFileName );
					_strlwr( filename );

					Q_FixSlashes( filename );

					CUtlSymbol sym = g_Analysis.symbols.AddString( filename );
					files.AddToTail( sym );

					if ( !( files.Count() % 3000 ) )
					{
						vprint( 0, "...found %i .vmt files\n", files.Count() );
					}
				}
			}
		}
	} while ( FindNextFile( ff, &wfd ) );
}

void BuildFileList( CUtlVector< CUtlSymbol >& files, char const *rootdir, char const *extension )
{
	files.RemoveAll();
	BuildFileList_R( files, rootdir, extension );
}

bool ValidateVMTFile( char const *vmtname, int offset )
{
	bool valid = true;

	KeyValues *kv = new KeyValues( "Test" );
	if ( kv->LoadFromFile( g_pFileSystem, &vmtname[offset] ) )
	{
		// Do any custom checking here...
	}
	else
	{
		valid = false;
	}
	kv->deleteThis();
	
	return valid;
}

void ProcessMaterialsDirectory( char const *basedir )
{
	vprint( 0, "building .vmt list\n" );

	CUtlVector< CUtlSymbol > vmts;

	BuildFileList( vmts, basedir, ".vmt" );

	vprint( 0, "found %i .vmt files\n\n", vmts.Count() );

	int offset = strlen( basedir ) + 1;
	offset -= strlen( "materials/" );
	if ( offset < 0 )
	{
		Error( "Bogus offset\n" );
	}

	// Now iterate vmts and load into memory, etc.

	int c = vmts.Count();
	int valid = 0;
	for ( int i = 0; i < c; i++ )
	{
		CUtlSymbol& sym = vmts[ i ];

		char const *vmtfile = g_Analysis.symbols.String( sym );

		if ( verbose )
		{
			vprint( 0, "checking %i .vmt %s\n", i, vmtfile );
		}

		spewed = false;
		if ( ValidateVMTFile( vmtfile, offset ) )
		{
			if ( !spewed )
			{
				valid++;
			}
		}

		if ( i > 0 && !( i % 1000 ) )
		{
			vprint( 0, "Analyzed %i .vmt files (%.2f %%%%)\n", i, 100.0f * (float)i/(float)c );
		}
	}

	int ecount = c - valid;
	vprint( 0, "\nSummary:  found %i/%i (%.2f percent) .vmt errors\n", ecount, c, 100.0 * ecount / max( c, 1 ) );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckLogFile( void )
{
	if ( uselogfile )
	{
		_unlink( "log.txt" );
		vprint( 0, "    Outputting to log.txt\n" );
	}
}

void PrintHeader()
{
	vprint( 0, "Valve Software - vmtcheck.exe (%s)\n", __DATE__ );
	vprint( 0, "--- VMT File Consistency Checker ---\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "vmtcheck", 2 );

	int i=1;
	for ( i ; i<argc ; i++)
	{
		if ( argv[ i ][ 0 ] == '-' )
		{
			switch( argv[ i ][ 1 ] )
			{
			case 'l':
				uselogfile = true;
				break;
			case 'v':
				verbose = true;
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 2 || ( i != argc ) )
	{
		PrintHeader();
		printusage();
	}

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Looking for messed up .vmt files...\n" );

	char vmtdir[ 256 ];
	strcpy( vmtdir, argv[ i - 1 ] );

	if ( !strstr( vmtdir, "materials" ) )
	{
		vprint( 0, "Materials dir %s looks invalid (format:  u:/tf2/hl2/materials)\n", vmtdir );

		return 0;
	}

	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

	// If they didn't specify -game on the command line, use VPROJECT.
	CmdLib_InitFileSystem( workingdir );

	vprint( 0, "game dir %s\nmaterials dir %s\n\n",
		gamedir,
		vmtdir );

	Q_StripTrailingSlash( vmtdir );
	ProcessMaterialsDirectory( vmtdir );

	FileSystem_Term();

	return 0;
}
