//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: vcd_sound_check.cpp : Defines the entry point for the console application.
//
//=============================================================================//
#include <stdio.h>
#include <windows.h>
#include "tier0/dbg.h"
#include "tier1/utldict.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "tier1/KeyValues.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "vstdlib/random.h"
#include "tier1/UtlBuffer.h"
#include "pacifier.h"
#include "appframework/appframework.h"
#include "tier0/icommandline.h"
#include "keyvaluescompiler.h"
#include "tier2/keyvaluesmacros.h"

#include "kvc_paintkit.h"

#include "tier0/fasttimer.h"

// #define TESTING 1


bool uselogfile = false;
bool timing = false;
qboolean verbose = false;

struct AnalysisData
{
	CUtlSymbolTable				symbols;
};

static AnalysisData g_Analysis;

IBaseFileSystem *filesystem = NULL;
//IFileSystem *g_pFullFileSystem = NULL;

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

void logprint( char const *logfile, const char *fmt, ... )
{
	char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );

	FILE *fp = NULL;
	static bool first = true;
	if ( first )
	{
		first = false;
		fp = fopen( logfile, "wb" );
	}
	else
	{
		fp = fopen( logfile, "ab" );
	}
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


void Con_Printf( const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	vprint( 0, output );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  kvc <wildcard path> [<wildcard path>] <outputfile>\n\
\n\
    -v = verbose output\n\
    -l = log to file log.txt\n\
    -t = perform load timing tests\n\
    -p = perform paint kit macro expansion\n\
         in this mode if no output file is specified,\n\
         the input file is copied to <input>_bak and <input> is overwritten\n\
         with -p, each file specified is processed separately without wildcards\n\
    -f = With -p, output is to <input>_fix and <input> is unchanged\n\
\n\
e.g.:  kvc -l u:/xbox/game/hl2x/materials/*.vmt u:/xbox/game/hl2x/kvc/vmt.kv\n\
\n\
       kvc -v -p americanpastoral_rocketlauncher.paintkit\n\
\n" );

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

#if defined( TESTING )
	if ( files.Count() > 100 )
		return;
#endif

	if ( ( ff = FindFirstFile( directory, &wfd ) ) == INVALID_HANDLE_VALUE )
		return;

	int extlen = strlen( extension );

	do
	{
#if defined( TESTING )
		if ( files.Count() > 100 )
			return;
#endif
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
						vprint( 0, "...found %i .%s files\n", files.Count(), extension );
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


void BuildFileListWildcard_R( CUtlVector< CUtlSymbol >& files, char const *dir, char const *wildcard )
{
	// Match files in current directory againsxt the wildcard
	char filesearch[ 256 ];
	WIN32_FIND_DATA filedata;
	HANDLE h;

	Q_snprintf( filesearch, sizeof( filesearch ), "%s\\%s", dir, wildcard );

	if ( ( h = FindFirstFile( filesearch, &filedata ) ) != INVALID_HANDLE_VALUE )
	{
		do
		{
	#if defined( TESTING )
			if ( files.Count() > 100 )
				return;
	#endif
			if ( filedata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				continue;
			}

			char filename[ MAX_PATH ];
			Q_snprintf( filename, sizeof( filename ), "%s\\%s", dir, filedata.cFileName );
			_strlwr( filename );

			Q_FixSlashes( filename );

			CUtlSymbol sym = g_Analysis.symbols.AddString( filename );
			files.AddToTail( sym );

			if ( !( files.Count() % 3000 ) )
			{
				vprint( 0, "...found %i files\n", files.Count() );
			}

		} while ( FindNextFile( h, &filedata ) );
	}

	// Now iterate the subdirectories and try them, too
	WIN32_FIND_DATA wfd;

	char directory[ 256 ];
	char filename[ 256 ];
	HANDLE ff;

	sprintf( directory, "%s\\*.*", dir );

#if defined( TESTING )
	if ( files.Count() > 100 )
		return;
#endif

	if ( ( ff = FindFirstFile( directory, &wfd ) ) == INVALID_HANDLE_VALUE )
		return;

	do
	{
#if defined( TESTING )
		if ( files.Count() > 100 )
			return;
#endif
		if ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( wfd.cFileName[ 0 ] == '.' )
				continue;

			// Recurse down directory
			sprintf( filename, "%s\\%s", dir, wfd.cFileName );
			BuildFileListWildcard_R( files, filename, wildcard );
		}
	} while ( FindNextFile( ff, &wfd ) );
}

void BuildFileListFromWildcard( CUtlVector< CUtlSymbol >& files, char const *search )
{
	char basepath[ 512 ];
	char wildcard[ 512 ];

	Q_ExtractFilePath( search, basepath, sizeof( basepath ) );
	Q_StripTrailingSlash( basepath );
	Q_strncpy( wildcard, &search[ Q_strlen( basepath ) + 1 ], sizeof( wildcard ) );

	BuildFileListWildcard_R( files, basepath, wildcard );
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
	vprint( 0, "Valve Software - kvc.exe (%s)\n", __DATE__ );
	vprint( 0, "--- KeyValues File compiler ---\n" );
}

void CompileKeyValuesFiles( CUtlVector< CUtlSymbol >& scriptFiles, CCompiledKeyValuesWriter& writer )
{
	CUtlVector< CUtlSymbol > disposition;

	StartPacifier( "CompileKeyValuesFiles" );
	int i;
	int c = scriptFiles.Count();
	for ( i = 0 ; i < c; ++i )
	{
		UpdatePacifier( (float)i / (float)c );
		char filename[ 512 ];
		Q_strncpy( filename, g_Analysis.symbols.String( scriptFiles[ i ] ), sizeof( filename ) );
		writer.AppendKeyValuesFile( &filename[ Q_strlen( gamedir ) ] );
	}
	EndPacifier();
}

void DescribeKV( int depth, KeyValues *parent, KeyValues *kv )
{
	switch ( kv->GetDataType() )
	{
	default:
	case KeyValues::TYPE_NONE:
		vprint( depth, "%s\n", kv->GetName() );
		break;
	case KeyValues::TYPE_STRING:
	case KeyValues::TYPE_INT:
	case KeyValues::TYPE_FLOAT:
	case KeyValues::TYPE_PTR:
	case KeyValues::TYPE_WSTRING:
	case KeyValues::TYPE_COLOR:
		{
			vprint( depth, "%s = %s\n", kv->GetName(), kv->GetString() );
		}
		break;
	}

	// Describe children
	for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
	{
		DescribeKV( depth + 1, kv, sub );
	}

	// Then add peers
	if ( !parent && kv->GetNextKey() )
	{
		DescribeKV( depth, NULL, kv->GetNextKey() );
	}
}

void ExamineKVFile( char const *filename )
{
	int i;
	CCompiledKeyValuesReader reader;

	CFastTimer timer;
	CFastTimer timer2;
	CFastTimer timer3;

	CUtlDict< KeyValues *, unsigned short >	results;

	timer.Start();

	reader.LoadFile( filename );

	timer.End();
	timer2.Start();

	// Now get the actual files
	for ( i = reader.First(); i != reader.InvalidIndex(); i = reader.Next( i ) )
	{
		char fn[ 256 ];
		reader.GetFileName( i, fn, sizeof( fn ) );
		// Instance keyvalues
		KeyValues *kv = reader.Instance( fn );
		results.Insert( fn, kv );
	}

	timer2.End();

	timer3.Start();

	KeyValues *foo = new KeyValues( "bar" );

	// Now get the actual files
	for ( i = reader.First(); i != reader.InvalidIndex(); i = reader.Next( i ) )
	{
		foo->Clear();
		char fn[ 256 ];
		reader.GetFileName( i, fn, sizeof( fn ) );
		// Instance keyvalues
		reader.InstanceInPlace( *foo, fn );
	}

	foo->deleteThis();

	timer3.End();
	/*
	for ( i = results.First(); i != results.InvalidIndex(); i = results.Next( i ) )
	{
		vprint( 1, "%s\n", results.GetElementName( i ) );
		KeyValues *kv = results[ i ];
		DescribeKV( 1, NULL, kv );
	}
	*/
	vprint( 0, "loading of %d elements took %.3f msec, create %.3f in-place %.3f\n", 
		results.Count(), 
		(float)timer.GetDuration().GetMillisecondsF(), 
		(float)timer2.GetDuration().GetMillisecondsF(),
		(float)timer3.GetDuration().GetMillisecondsF());

	CUtlVector< KeyValues * >	test;
	// Now load them the hard way
	CFastTimer kvt;
	kvt.Start();
	for ( i = results.First(); i != results.InvalidIndex(); i = results.Next( i ) )
	{
		KeyValues *kv = new KeyValues( results.GetElementName( i ) );
		kv->LoadFromFile( filesystem, results.GetElementName( i ) );
		test.AddToTail( kv );
	}

	kvt.End();

	// Clean up
	int c = test.Count();
	for ( i = 0; i < c; ++i )
	{
		test[ i ]->deleteThis();
	}

	for ( i = results.First(); i != results.InvalidIndex(); i = results.Next( i ) )
	{
		results[ i ]->deleteThis();
	}

	vprint( 0, "parsing of %d elements took %.3f msec\n", results.Count(), (float)kvt.GetDuration().GetMillisecondsF() );
}

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CCompileKeyValuesApp : public CSteamAppSystemGroup
{
	typedef CSteamAppSystemGroup BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	// Sets up the search paths
	bool SetupSearchPaths();
};

bool CCompileKeyValuesApp::Create()
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "kvc", 2 );

	AppSystemInfo_t appSystems[] = 
	{
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	g_pFullFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	g_pFileSystem = filesystem = g_pFullFileSystem;

	if ( !filesystem )
	{
		Error("Unable to load required library interface!\n");
	}

	return true;
}

void CCompileKeyValuesApp::Destroy()
{
	g_pFullFileSystem = NULL;
	g_pFileSystem = filesystem = NULL;
}

//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CCompileKeyValuesApp::SetupSearchPaths()
{
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	// Set gamedir.
	Q_MakeAbsolutePath( gamedir, sizeof( gamedir ), GetGameInfoPath() );
	Q_AppendSlash( gamedir, sizeof( gamedir ) );

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CCompileKeyValuesApp::PreInit( )
{
//	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
	g_pFullFileSystem->SetWarningFunc( Warning );

	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	return true; 
}

void CCompileKeyValuesApp::PostShutdown()
{
}

//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CCompileKeyValuesApp::Main()
{
	bool bOptPaintKit = false;

	CUtlVector< CUtlSymbol >	worklist;

	int i;
	for ( i=1 ; i<CommandLine()->ParmCount() ; i++ )
	{
		if ( CommandLine()->GetParm( i )[ 0 ] == '-' )
		{
			switch( CommandLine()->GetParm( i )[ 1 ] )
			{
			case 'l':
				uselogfile = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'g': // -game
				++i;
				break;
			case 't':
				timing = true;
				break;
			case 'p':
				bOptPaintKit = true;
				break;
			case 'f':	// -f is valid when -p is specified
				break;
			default:
				printusage();
				break;
			}
		}
		else if ( i != 0 )
		{
			CUtlSymbol sym;
			sym = CommandLine()->GetParm( i );
			worklist.AddToTail( sym );
		}
	}

	if ( bOptPaintKit )
	{
		if ( CommandLine()->ParmCount() < 2 || ( i != CommandLine()->ParmCount() ) || ( worklist.Count() < 1 ) )
		{
			PrintHeader();
			printusage();	// This exits app
		}

		ProcessPaintKitKeyValuesFiles( worklist );

		return 0;
	}

	if ( CommandLine()->ParmCount() < 2 || ( i != CommandLine()->ParmCount() ) || worklist.Count() < 2 )
	{
		PrintHeader();
		printusage();
	}

	CheckLogFile();

	PrintHeader();

	char binaries[MAX_PATH];
	Q_strncpy( binaries, gamedir, MAX_PATH );
	Q_StripTrailingSlash( binaries );
	Q_strncat( binaries, "/../bin", MAX_PATH, MAX_PATH );

	char outfile[ 512 ];
	Q_strncpy( outfile, worklist[ worklist.Count() - 1 ].String() , sizeof( outfile ) );

	g_pFullFileSystem->AddSearchPath( binaries, "EXECUTABLE_PATH");

	vprint( 0, "    Compiling keyvalues files...\n" );

	CUtlVector< CUtlSymbol > diskfiles;

	for ( i = 0; i < worklist.Count() - 1; ++i )
	{
        char workdir[ 256 ];
		Q_snprintf( workdir, sizeof( workdir ), "%s", worklist[ i ].String() );

		Q_StripTrailingSlash( workdir );

		vprint( 0, "Processing '%s'\n", workdir );

		int oldc = diskfiles.Count();
		BuildFileListFromWildcard( diskfiles, workdir );
		int added = diskfiles.Count() - oldc;
        vprint( 0, "found %i files\n\n", added  );
	}

	{
		CCompiledKeyValuesWriter writer;
		CompileKeyValuesFiles( diskfiles, writer );
		vprint( 0, "Writing compiled file '%s'\n", outfile );
		writer.WriteFile( outfile );
	}

	if ( timing )
	{
		ExamineKVFile( outfile );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	CommandLine()->CreateCmdLine( argc, argv );

	CCompileKeyValuesApp kvCompiler;
	CSteamApplication steamApplication( &kvCompiler );
	int nRetVal = steamApplication.Run();

	return nRetVal;	
}