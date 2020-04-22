//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "mdlcheck_util.h"
#include "tier0/dbg.h"
#include "utldict.h"
#include "tier1/utlstring.h"

bool uselogfile = false;
bool verbose = false;
bool checkani = false;

struct QCFile
{
	char outputmodel[ MAX_PATH ];
};

struct ModelFile
{
	char qcfile[ MAX_PATH ];
	int version;
	bool needsrecompile;
	int toobig;
};

struct AnalysisData
{
	CUtlDict< QCFile, int >		files;  // .qc to modelname lookup
	CUtlDict< ModelFile, int >	models;  // .mdl to .qc/version lookup

	CUtlSymbolTable				symbols;
};

static AnalysisData g_Analysis;


SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
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
	vprint( 0, "usage:  mdlcheck <model source directory> <.mdl file directory>\n\
		\t-v = verbose output\n\
		\t-l = log to file log.txt\n\
		\t-a = check for large animation data\n\
		\ne.g.:  mdlcheck -l u:/hl2/hl2/hl2models u:/hl2/hl2/models\n" );

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
				if ( strstr( wfd.cFileName, ".360." ) )
				{
				}
				else if ( !stricmp( &wfd.cFileName[ len - extlen ], extension ) )
				{
					char filename[ MAX_PATH ];
					Q_snprintf( filename, sizeof( filename ), "%s\\%s", dir, wfd.cFileName );
					_strlwr( filename );

					Q_FixSlashes( filename );

					CUtlSymbol sym = g_Analysis.symbols.AddString( filename );
					files.AddToTail( sym );
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

//-----------------------------------------------------------------------------
// This is here because scriplib.cpp is included in this project but cmdlib.cpp
// is not, but scriplib.cpp uses some stuff from cmdlib.cpp, same with
// LoadFile and ExpandPath above.  The only thing that currently uses this
// is $include in scriptlib, if this function returns 0, $include will
// behave the way it did before this change
//-----------------------------------------------------------------------------
int CmdLib_ExpandWithBasePaths( CUtlVector< CUtlString > &expandedPathList, const char *pszPath )
{
	return 0;
}


bool GetModelNameFromSourceFile( char const *filename, char *modelname, int maxlen )
{
	modelname[0]=0;

	int filelength;
	char *buffer = (char *)COM_LoadFile( filename, &filelength );
	if ( !buffer )
	{
		vprint( 0, "Couldn't load %s\n", filename );
		return false;
	}

	bool valid = false;

	// Parse tokens
	char *current = buffer;
	while ( current )
	{
		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			break;

		if ( stricmp( com_token, "$modelname" ) )
			continue;

		current = CC_ParseToken( current );

		strcpy( modelname, com_token );
		_strlwr( modelname );

		Q_FixSlashes( modelname );

		Q_DefaultExtension( modelname, ".mdl", maxlen );

		valid = true;
		break;
	}

	COM_FreeFile( (unsigned char *)buffer );

	if ( !valid )
	{
		vprint_queued( 0, ".qc file %s missing $modelname directive!!!\n", filename );
	}
	return valid;
}

bool AddModelNameFromSource( CUtlDict< ModelFile, int >& models, char const *filename, char const *modelname, int offset )
{
	int idx = models.Find( modelname );
	if ( idx != models.InvalidIndex() )
	{
		char shortname[ MAX_PATH ];
		char shortname2[ MAX_PATH ];
		strcpy( shortname, &filename[ offset ] );
		strcpy( shortname2, &models[ idx ].qcfile[ offset ] );

		vprint_queued( 0, "multiple .qc's build %s\n  %s\n  %s\n",
			modelname,
			shortname, 
			shortname2 );
		return false;
	}

	ModelFile mf;
	strcpy( mf.qcfile, filename );
	_strlwr( mf.qcfile );
	mf.version = 0;

	models.Insert( modelname, mf );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sourcetreebase - 
//			*subdir - 
//			*baseentityclass - 
//-----------------------------------------------------------------------------
void ProcessSourceDirectory( char const *basedir )
{
	// vprint( 0, "building .qc list\n" );

	CUtlVector< CUtlSymbol > files;

	BuildFileList( files, basedir, ".qc" );

	// vprint( 0, "found %i .qc files\n\n", files.Count() );

	int offset = strlen( basedir ) + 1;

	// Add files to QC Files dictionary
	int c = files.Count();
	for ( int i = 0; i < c; i++ )
	{
		QCFile qcf;
		memset( &qcf, 0, sizeof( qcf ) );
		CUtlSymbol& sym = files[ i ];
		g_Analysis.files.Insert( g_Analysis.symbols.String( sym ), qcf );
	}

	vprint_queued( 0, "%s", "\n\n" );

	// Now iterate .qc files, looking into each to find the output model name
	c = g_Analysis.files.Count();
	int valid = 0;
	for ( int i = 0; i < c; i++ )
	{
		char modelname[ 256 ];
		char const *filename = g_Analysis.files.GetElementName( i );
		if ( verbose )
		{
			vprint( 0, "checking %i: %s\n", i, filename );
		}
		if ( GetModelNameFromSourceFile( filename, modelname, sizeof( modelname ) ) )
		{
			if ( AddModelNameFromSource( g_Analysis.models, filename, modelname, offset ) )
			{
				valid++;
			}
		}
	}

	int ecount = c - valid;
	if (ecount != 0)
	{
		// vprint( 0, "\n summary:  found %i/%i (%.2f percent) .qc errors\n\n", ecount, c, 100.0 * ecount / max( c, 1 ) );
		vprint( 0, "\n summary:  found %i .qc errors\n\n", ecount );
	}
}

#include "studio.h"

#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I')
														// little-endian "IDST"
#define IDSTUDIOANIMGROUPHEADER	(('G'<<24)+('A'<<16)+('D'<<8)+'I')
														// little-endian "IDAG"


byte buffer[1024*1024*4];
bool ValidateModelFile( char const *modelname, int offset )
{
	studiohdr_t *pHdr;
	FILE *fp;

	pHdr = (studiohdr_t *)buffer;

	fp = fopen( modelname, "rb" );
	if ( !fp )
	{
		vprint_queued( 0, "Unable to open .mdl file %s\n", modelname );
		return false;
	}

	// See if there's a .qc which builds this model
	char shortname[ MAX_PATH ];
	strcpy( shortname, &modelname[ offset ] );

	Q_FixSlashes( shortname );

	fread( buffer, sizeof( buffer ), 1, fp );
	fclose( fp );

	if ( pHdr->id != IDSTUDIOHEADER )
	{
		vprint_queued( 0, "Bogus studiomdl header for %s, expecting 'IDST' four cc code\n", shortname );
		return false;
	}

	bool valid = true;
	bool needsrecompile = false;
	int toobig = 0;

	// previous version is compatible
	if ( pHdr->version < 44 || pHdr->version > STUDIO_VERSION )
	{
		vprint_queued( 0, "Outdated model %s (ver %i != %i)\n", shortname, pHdr->version, STUDIO_VERSION );
		valid = false;
	}

	if (!Studio_ConvertStudioHdrToNewVersion( pHdr ))
	{
		// vprint( 0, "%s needs to be recompiled\n", pHdr->pszName() );
		needsrecompile = true;
	}

	if (checkani)
	{
		// HACK: since the sequence data is written after the animation data, this is rough way to determine how much anim data there really is
		int totalanimsize = pHdr->localseqindex - pHdr->localanimindex - pHdr->numlocalanim * sizeof( mstudioanimdesc_t );
		if (pHdr->pLocalAnimdesc( 0 )->animblock == 0 && totalanimsize > 1024 * 64)
		{
			toobig = totalanimsize;
		}
	}

	int idx = g_Analysis.models.Find( shortname );
	if ( idx ==  g_Analysis.models.InvalidIndex() )
	{
		vprint_queued( 0, "Couldn't find a .qc which builds %s\n", shortname );
		valid = false;
	}
	else
	{
		g_Analysis.models[idx].version = pHdr->version;
		g_Analysis.models[idx].needsrecompile = needsrecompile;
		g_Analysis.models[idx].toobig = toobig;
	}


	return valid;
}

void ProcessModelsDirectory( char const *basedir )
{
	// vprint( 0, "building .mdl list\n" );

	CUtlVector< CUtlSymbol > models;

	BuildFileList( models, basedir, ".mdl" );

	// vprint( 0, "found %i .mdl files\n\n", models.Count() );

	int offset = strlen( basedir ) + 1;

	// Now iterate model files and check version tag and whether a .qc exists which builds the .mdl

	vprint_queued( 0, "%s", "\n\n" );

	// Add files to QC Files dictionary
	int c = models.Count();
	int valid = 0;
	for ( int i = 0; i < c; i++ )
	{
		QCFile qcf;
		memset( &qcf, 0, sizeof( qcf ) );
		CUtlSymbol& sym = models[ i ];

		char const *modelname = g_Analysis.symbols.String( sym );

		if ( verbose )
		{
			vprint( 0, "checking %i .mdl %s\n", i, modelname );
		}

		if ( ValidateModelFile( modelname, offset ) )
		{
			valid++;
		}
	}

	int ecount = c - valid;
	if (ecount != 0)
	{
		// vprint( 0, "\n summary:  found %i/%i (%.2f percent) .mdl errors\n", ecount, c, 100.0 * ecount / max( c, 1 ) );
		vprint( 0, "\n summary:  found %i .mdl errors\n", ecount );
	}
}



void CheckForUnbuiltModels( )
{
	vprint_queued( 0, "%s", "\n\n" );

	int c = g_Analysis.models.Count();
	int valid = 0;
	for ( int i = 0; i < c; i++ )
	{
		if (g_Analysis.models[i].version == 0)
		{
			vprint_queued( 0, "Can't find %s,\n\tbuilt by %s\n", g_Analysis.models.GetElementName( i ), g_Analysis.models[i].qcfile );
		}
		else if (g_Analysis.models[i].needsrecompile)
		{
			vprint_queued( 0, "%s out of date,\n\tbuilt by %s\n", g_Analysis.models.GetElementName( i ), g_Analysis.models[i].qcfile );
		}
		else if (g_Analysis.models[i].toobig)
		{
			vprint_queued( 0, "%s needs $animblocksize command (%d of animdata),\n\tbuilt by %s\n", g_Analysis.models.GetElementName( i ), g_Analysis.models[i].toobig, g_Analysis.models[i].qcfile );
		}
		else
		{
			valid++;
		}
	}

	int ecount = c - valid;
	if (ecount != 0)
	{
		// vprint( 0, "\n summary:  found %i/%i (%.2f percent) missing .mdl's\n", ecount, c, 100.0 * ecount / max( c, 1 ) );
		vprint( 0, "\n summary:  found %i missing .mdl's\n", ecount );
	}
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	SpewOutputFunc( SpewFunc );

	int i = 1;
	for (i ; i<argc ; i++)
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
			case 'a':
				checkani = true;
				break;
			default:
				printusage();
				break;
			}
		}
	}

	vprint( 0, "Valve Software - mdlcheck.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Source Model Consistency Checker ---\n" );

	if ( argc < 3 || ( i != argc ) )
	{
		printusage();
	}

	CheckLogFile();

	char modelsources[ 256 ];
	strcpy( modelsources, argv[ i - 2 ] );
	char modelsdir[ 256 ];
	strcpy( modelsdir, argv[ i - 1 ] );

	if ( !strstr( modelsdir, "models" ) )
	{
		vprint( 0, "Models dir %s looks invalid (format:  u:/tf2/hl2/models)\n", modelsdir );
		return 0;
	}

	Q_StripTrailingSlash( modelsources );
	Q_StripTrailingSlash( modelsdir );

	ProcessSourceDirectory( modelsources );
	ProcessModelsDirectory( modelsdir );
	CheckForUnbuiltModels( );

	dump_print_queue( );

	return 0;
}
