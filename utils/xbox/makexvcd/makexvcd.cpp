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
#include "choreoscene.h"
#include "choreoevent.h"
#include "iscenetokenprocessor.h"
#include "tier1/utlbuffer.h"
#include "tier1/checksum_crc.h"
#include "pacifier.h"
#include "SceneCache.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ModelSoundsCache.h"
#include "Datacache/imdlcache.h"
#include "datacache/idatacache.h"
#include "studio.h"
#include "appframework/appframework.h"
#include "tier0/icommandline.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "vphysics_interface.h"
#include "icvar.h"
#include "vstdlib/cvar.h"
#include "eventlist.h"

// #define TESTING 1


bool uselogfile = false;
bool modelsoundscache = false;
bool scenecache = false;
bool virtualmodel = false;
bool buildxcds = false;

struct AnalysisData
{
	CUtlSymbolTable				symbols;
};

static AnalysisData g_Analysis;

static char g_szCurrentGameDir[ 512 ];

IFileSystem *filesystem = NULL;
IMDLCache *g_pMDLCache = NULL;
ISoundEmitterSystemBase *soundemitterbase = NULL;

static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;

static bool spewed = false;

static CUtlCachedFileData< CSceneCache >	g_SceneCache( "scene.cache", SCENECACHE_VERSION, 0, UTL_CACHED_FILE_USE_FILESIZE );
static CUtlCachedFileData< CModelSoundsCache >	g_ModelSoundsCache( "modelsounds.cache", MODELSOUNDSCACHE_VERSION, 0, UTL_CACHED_FILE_USE_FILESIZE );

//-----------------------------------------------------------------------------
// FIXME: This trashy glue code is really not acceptable. Figure out a way of making it unnecessary.
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( pModelName );
	*cache = (void*)handle;
	return g_pMDLCache->GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	return g_pMDLCache->GetVirtualModel( (MDLHandle_t)virtualModel );
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return g_pMDLCache->GetAnimBlock( (MDLHandle_t)virtualModel, i );
}

int studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( (MDLHandle_t)virtualModel, pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_pMDLCache->GetStudioHdr( (MDLHandle_t)cache );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	const char	*CurrentToken( void );
	bool		GetToken( bool crossline );
	bool		TokenAvailable( void );
	void		Error( const char *fmt, ... );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CSceneTokenProcessor::CurrentToken( void )
{
	return token;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crossline - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::GetToken( bool crossline )
{
	return ::GetToken( crossline ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::TokenAvailable( void )
{
	return ::TokenAvailable() ? true : false;
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

static CSceneTokenProcessor g_TokenProcessor;

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
	vprint( 0, "usage:  makexvcd [options] -game gamedir\n\
		\t-v = verbose output\n\
		\t-m = rebuild modelsounds.cache\n\
		\t-x = rebuild .xcd files\n\
		\t-s = rebuild scene.cache\n\
		\t-z = rebuild virtualmodel.cache (xbox only)\n\
		\t-l = log to file log.txt\n\
		\ne.g.:  makexvcd -s -m -game episodic\n" );

	// Exit app
	exit( 1 );
}

void BuildFileList_R( CUtlVector< CUtlSymbol >& files, char const *dir, char const *extension )
{
	WIN32_FIND_DATA wfd;

	char directory[ 256 ];
	char filename[ 256 ];
	HANDLE ff;

	Q_snprintf( directory, sizeof( directory ), "%s\\*.*", dir );

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
			Q_snprintf( filename, sizeof( filename ), "%s\\%s", dir, wfd.cFileName );
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

void BuildFileList( char const *basedir, CUtlVector< CUtlSymbol >& files, char const *rootdir, char const *extension )
{
	files.RemoveAll();
	char root[ 512 ];
	Q_snprintf( root, sizeof( root ), "%s\\%s", basedir, rootdir );
	BuildFileList_R( files, root, extension );
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
	vprint( 0, "Valve Software - CompileVCD.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Binary .vcd compiler ---\n" );
}

//-----------------------------------------------------------------------------
// Purpose: For each .wav file in the list, see if any vcd file in the list references it
//  First build an index of .wav to .vcd mappings, then search wav list and print results
// Input  : vcdfiles - 
//			wavfiles - 
//-----------------------------------------------------------------------------

struct VCDList
{
	VCDList()
	{
	}

	VCDList( const VCDList& src )
	{
		int c = src.vcds.Count();
		for ( int i = 0 ; i < c; i++ )
		{
			vcds.AddToTail( src.vcds[ i ] );
		}
	}

	VCDList& operator =( const VCDList& src )
	{
		if ( this == &src )
			return *this;

		int c = src.vcds.Count();
		for ( int i = 0 ; i < c; i++ )
		{
			vcds.AddToTail( src.vcds[ i ] );
		}

		return *this;
	}

	CUtlVector< CUtlSymbol >	vcds;
};

void AppendDisposition( CUtlVector< CUtlSymbol >& disposition, char const *fmt, ... )
{
	char string[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	CUtlSymbol sym;
	sym = string;
	disposition.AddToTail( sym );
}

CChoreoScene *BlockingLoadScene( char const *vcdname )
{
	// Load the .vcd
	char scenefile[ MAX_PATH ];
	Q_snprintf( scenefile, sizeof( scenefile ), "%s\\%s", g_szCurrentGameDir, vcdname );

	LoadScriptFile( scenefile );
	
	CChoreoScene *scene = ChoreoLoadScene( scenefile, NULL, &g_TokenProcessor, Con_Printf );
	return scene;
}

void ProcessVCD( char const *vcdname, CUtlVector< CUtlSymbol >& disposition )
{
	if ( verbose )
	{
		vprint( 0, "Processing '%s'\n", vcdname );
	}

	bool rebuild = false;

	FileHandle_t fh = filesystem->Open( vcdname, "rb", "GAME" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
	{
		Error( "Couldn't open '%s' for reading.", vcdname );
		return;
	}

	size_t bufSize = filesystem->Size( fh );
	char *buffer = new char[ bufSize + 1 ];
	filesystem->Read( buffer, bufSize, fh );
	filesystem->Close( fh );
	buffer[ bufSize ] = 0;

	CRC32_t crc;
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, buffer, bufSize );
	CRC32_Final( &crc );

	delete[] buffer;

	// Now load the file as a binary if it exists...
	char binfile[ 512 ];
	Q_strncpy( binfile, vcdname, sizeof( binfile ) );
	Q_SetExtension( binfile, ".xcd", sizeof( binfile ) );

	if ( buildxcds )
	{
		fh = filesystem->Open( binfile, "rb", "GAME" );
		if ( fh == FILESYSTEM_INVALID_HANDLE )
		{
			AppendDisposition( disposition, "Built '%s'\n", binfile );
			rebuild = true;
		}
		else
		{
			// Read the first bit of data and check
			char crcdata[ 12 ];
			filesystem->Read( crcdata, sizeof( crcdata ), fh );
			filesystem->Close( fh );

			CUtlBuffer buf;
			buf.Put( crcdata, sizeof( crcdata ) );

			CRC32_t fileCRC = 0;
			if ( !CChoreoScene::GetCRCFromBuffer( buf, (unsigned int &)fileCRC ) )
			{
				AppendDisposition( disposition, "Rebuilt '%s' due to version change\n", binfile );
				rebuild = true;
			}
			else
			{
				if ( fileCRC != crc )
				{
					AppendDisposition( disposition, "Rebuilt '%s' due to crc change\n", binfile );
					rebuild = true;
				}
			}
		}
	}

	// Validate the scene cache
	g_SceneCache.RebuildItem( vcdname + strlen( g_szCurrentGameDir ) + 1 );

	if ( !rebuild )
		return;

	// Remove current binary
	if ( filesystem->FileExists( binfile, "GAME" ) )
	{
		_unlink( binfile );
	}

	// Load the .vcd
	LoadScriptFile( (char *)vcdname );
	
	CChoreoScene *scene = ChoreoLoadScene( vcdname, NULL, &g_TokenProcessor, Con_Printf );
	if ( scene )
	{
		scene->SaveBinary( binfile, NULL, crc );
		
		delete scene;
	}
}

void CompileVCDs( CUtlVector< CUtlSymbol >& vcds )
{
	CUtlVector< CUtlSymbol > disposition;

	StartPacifier( "CompileVCDs" );
	int i;
	int c = vcds.Count();
	for ( i = 0 ; i < c; ++i )
	{
		UpdatePacifier( (float)i / (float)c );
		ProcessVCD( g_Analysis.symbols.String( vcds[ i ] ), disposition );
	}
	EndPacifier();

	if ( verbose )
	{
		c = disposition.Count();
		for ( i = 0; i < c; ++i )
		{
			Warning( "%s", disposition[ i ].String() );
		}
	}
}

static CUtlMap< CStudioHdr *, MDLHandle_t > g_ModelMap( 0, 0, DefLessFunc( CStudioHdr * ) ); 

CStudioHdr *ModelSoundsCache_LoadModel( char const *filename )
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( filename );

	CStudioHdr *studioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( handle ), g_pMDLCache ); 

	g_ModelMap.Insert( studioHdr, handle );

	if ( studioHdr->IsValid() )
		return studioHdr;
	return NULL;
}

void ModelSoundsCache_FinishModel( CStudioHdr *hdr )
{
	int idx = g_ModelMap.Find( hdr );
	if ( idx != g_ModelMap.InvalidIndex() )
	{
        g_pMDLCache->Release( g_ModelMap[ idx ] );
		g_ModelMap.RemoveAt( idx );
	}
	delete hdr;
}

void ModelSoundsCache_PrecacheScriptSound( const char *soundname )
{
}

void ProcessMDL( char const *mdlname, CUtlVector< CUtlSymbol >& disposition )
{
	if ( verbose )
	{
		vprint( 0, "Processing '%s'\n", mdlname );
	}

	if ( Q_stristr( mdlname, "ghostanim" ) )
	{
		int n =3 ;
	}
	// Validate the model sounds cache
	g_ModelSoundsCache.RebuildItem( mdlname + strlen( g_szCurrentGameDir ) + 1 );
}

void CompileMDLs( CUtlVector< CUtlSymbol >& mdls )
{
	CUtlVector< CUtlSymbol > disposition;

	StartPacifier( "CompileMDLs" );
	int i;
	int c = mdls.Count();
	for ( i = 0 ; i < c; ++i )
	{
		UpdatePacifier( (float)i / (float)c );
		ProcessMDL( g_Analysis.symbols.String( mdls[ i ] ), disposition );
	}
	EndPacifier();

	c = disposition.Count();
	for ( i = 0; i < c; ++i )
	{
		Warning( "%s", disposition[ i ].String() );
	}
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMakeCachesApp : public CSteamAppSystemGroup
{
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

bool CMakeCachesApp::Create()
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "makexvcd", 2 );

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, VENGINE_CVAR_INTERFACE_VERSION );

	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },

		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	g_pFileSystem = filesystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	g_pMDLCache = (IMDLCache*)FindSystem( MDLCACHE_INTERFACE_VERSION );
	soundemitterbase = (ISoundEmitterSystemBase*)FindSystem(SOUNDEMITTERSYSTEM_INTERFACE_VERSION);
	g_pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );

	if ( !soundemitterbase || !g_pMDLCache || !filesystem || !g_pMaterialSystem )
	{
		Error("Unable to load required library interface!\n");
	}

	g_pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );

	return true;
}

void CMakeCachesApp::Destroy()
{
	g_pFileSystem = filesystem = NULL;
	soundemitterbase = NULL;
	g_pMDLCache = NULL;
}

//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CMakeCachesApp::SetupSearchPaths()
{
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = true;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = filesystem->IsSteam();
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = filesystem;
	fsInfo.m_bToolsMode = true;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = fsInfo.m_pFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	char platform[MAX_PATH];
	Q_strncpy( platform, steamInfo.m_GameInfoPath, MAX_PATH );
	Q_StripTrailingSlash( platform );
	Q_strncat( platform, "/../platform", MAX_PATH, MAX_PATH );

	fsInfo.m_pFileSystem->AddSearchPath( platform, "PLATFORM" );

	// Set gamedir.
	Q_MakeAbsolutePath( gamedir, sizeof( gamedir ), steamInfo.m_GameInfoPath );
	Q_AppendSlash( gamedir, sizeof( gamedir ) );

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CMakeCachesApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
	filesystem->SetWarningFunc( Warning );

	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	return true; 
}

void CMakeCachesApp::PostShutdown()
{
}

//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CMakeCachesApp::Main()
{
	for ( int i=1 ; i<CommandLine()->ParmCount() ; i++)
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
			case 'm':
				modelsoundscache = true;
				break;
			case 's':
				scenecache = true;
				break;
			case 'x':
				buildxcds = true;
				break;
			case 'z':
				virtualmodel = true;
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( CommandLine()->ParmCount() < 2 || ( i != CommandLine()->ParmCount() ) )
	{
		PrintHeader();
		printusage();
	}

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Compiling binary .vcd files to .xvcd ...\n" );

	char vcddir[ 256 ];
	char modelsdir[ 256 ];
	Q_snprintf( vcddir, sizeof( vcddir ), "scenes", CommandLine()->GetParm( i - 1 ) );
	Q_snprintf( modelsdir, sizeof( modelsdir ), "models", CommandLine()->GetParm( i - 1 ) );
	if ( !strstr( vcddir, "scenes" ) )
	{
		vprint( 0, ".vcd dir %s looks invalid (format:  u:/game/hl2/scenes)\n", vcddir );
		return 0;
	}
	if ( !strstr( modelsdir, "models" ) )
	{
		vprint( 0, ".mdl dir %s looks invalid (format:  u:/game/hl2/models)\n", modelsdir );
		return 0;
	}

	char binaries[MAX_PATH];
	Q_strncpy( binaries, gamedir, MAX_PATH );
	Q_StripTrailingSlash( binaries );
	Q_strncat( binaries, "/../bin", MAX_PATH, MAX_PATH );

	filesystem->AddSearchPath( binaries, "EXECUTABLE_PATH");

	soundemitterbase->ModInit();

	// Delete the scene cache file
	if ( scenecache ) filesystem->RemoveFile( "scene.cache", "MOD" );
	if ( modelsoundscache ) filesystem->RemoveFile( "modelsounds.cache", "MOD" );
	if ( virtualmodel ) filesystem->RemoveFile( "virtualmodel.cache", "MOD" );

	CUtlSymbolTable pathStrings;
	CUtlVector< CUtlSymbol >	searchList;
	char searchPaths[ 512 ];
	filesystem->GetSearchPath( "GAME", true, searchPaths, sizeof( searchPaths ) );

	// We want to walk them in reverse order so newer files are "overrides" for older ones, so we add them to a list in reverse order
	for ( char *path = strtok( searchPaths, ";" ); path; path = strtok( NULL, ";" ) )
	{
		char dir[ 512 ];
		Q_strncpy( dir, path, sizeof( dir ) );
		Q_FixSlashes( dir );
		Q_strlower( dir );
		Q_StripTrailingSlash( dir );

		CUtlSymbol sym = pathStrings.AddString( dir );
		// Push them on head so we can walk them in reverse order
		searchList.AddToHead( sym );
	}

	if ( scenecache )
	{
		g_SceneCache.Init();
	}
	if ( modelsoundscache )
	{
		g_ModelSoundsCache.Init();

		g_pMDLCache->InitPreloadData( true );
	}

	EventList_RegisterSharedEvents();

	for ( int sp = 0; sp < searchList.Count(); ++sp )
	{
		char const *basedir = pathStrings.String( searchList[ sp ] );
		Q_strncpy( g_szCurrentGameDir, basedir, sizeof( g_szCurrentGameDir ) );

		vprint( 0, "Processing gamedir %s\n", basedir );

		if ( scenecache )
		{
			vprint( 1, "Building list of .vcd files\n" );
			CUtlVector< CUtlSymbol > vcdfiles;
			BuildFileList( basedir, vcdfiles, vcddir, ".vcd" );
			vprint( 1, "found %i .vcd files\n", vcdfiles.Count() );
		
			CompileVCDs( vcdfiles );
		}

		if ( modelsoundscache )
		{
			vprint( 1, "Building list of .mdl files\n" );
			CUtlVector< CUtlSymbol > mdlfiles;
			BuildFileList( basedir, mdlfiles, modelsdir, ".mdl" );
			vprint( 1, "found %i .mdl files\n", mdlfiles.Count() );
		
			CompileMDLs( mdlfiles );
		}
	}

	if ( scenecache )
	{
		if ( g_SceneCache.IsDirty() )
		{
			g_SceneCache.Save();
		}
		g_SceneCache.Shutdown();
	}

	if ( modelsoundscache )
	{
		if ( g_ModelSoundsCache.IsDirty() )
		{
			g_ModelSoundsCache.Save();
		}
		g_ModelSoundsCache.Shutdown();

		g_pMDLCache->ShutdownPreloadData();
	}

	soundemitterbase->ModShutdown();

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

	CMakeCachesApp sceneManagerApp;
	CSteamApplication steamApplication( &sceneManagerApp );
	int nRetVal = steamApplication.Run();

	return nRetVal;	
}