//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: vcd_sound_check.cpp : Defines the entry point for the console application.
//
//=============================================================================//
#include "cbase.h"
#include <stdio.h>
#include <windows.h>
#include "tier0/dbg.h"
#include "utldict.h"
#include "tier1/UtlLinkedList.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "KeyValues.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "vstdlib/random.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "choreoscene.h"
#include "choreoevent.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "iscenetokenprocessor.h"

bool uselogfile = false;

struct AnalysisData
{
	CUtlSymbolTable				symbols;
};

static AnalysisData g_Analysis;

IFileSystem *filesystem = NULL;
static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;

ISoundEmitterSystemBase *soundemitter = NULL;

static bool spewed = false;
static bool spewmoveto = false;
static bool vcdonly= false;

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
	vprint( 0, "usage:  vcd_sound_check <.wav root directory> <scenes root directory>\n\
		\t-v = verbose output\n\
		\t-m = spew moveto info\n\
		\t-o = spew vcd overlap info only\n\
		\t-l = log to file log.txt\n\
		\ne.g.:  vcd_sound_check -l u:/hl2/hl2/sound/vo u:/hl2/hl2/scenes\n" );

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
	vprint( 0, "Valve Software - vcd_sound_check.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Voice Wav File .vcd Checker ---\n" );
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

static int ecounter = 0;

void SpewMoveto( bool first, char const *vcdname, CChoreoEvent *e )
{
	if ( !spewmoveto )
		return;

	// 

	if ( first )
	{
		ecounter = 0;
	}

	logprint( "moveto.txt", "\"%s\",%i,\"%s\",%.3f,\"%s\",%s\n", 
		vcdname, 
		++ecounter, 
		e->GetName(), 
		e->GetStartTime(), 
		e->GetParameters(),
		e->IsResumeCondition() ? "YES" : "no" );
}

static bool ChoreEventStartTimeLessFunc( CChoreoEvent * const &p1, CChoreoEvent * const &p2 )
{
	CChoreoEvent *e1;
	CChoreoEvent *e2;

	e1 = const_cast< CChoreoEvent * >( p1 );
	e2 = const_cast< CChoreoEvent * >( p2 );

	return e1->GetStartTime() < e2->GetStartTime();
}

static bool IsFlexTrackBeingUsed( CChoreoEvent *event, char const *trackName )
{
	int tc = event->GetNumFlexAnimationTracks();
	for ( int track = 0; track < tc; ++track )
	{
		CFlexAnimationTrack *t = event->GetFlexAnimationTrack( track );
		if ( !t->IsTrackActive() )
		{
			continue;
		}

		int sampleCountNormal = t->GetNumSamples( 0 );
		int sampleCountBalance = 0;
		if ( t->IsComboType() )
		{
			sampleCountBalance = t->GetNumSamples( 1 );
		}

		if ( !sampleCountNormal && !sampleCountBalance )
			continue;

		// Otherwise, see if the test track has this as an active track
		if ( !Q_stricmp( t->GetFlexControllerName(), trackName ) )
		{
			return true;
		}
	}
	return false;
}

static bool EventCollidesWithRows( CUtlLinkedList< CChoreoEvent*, int >& list, CChoreoEvent *event, char *trackName, size_t trackNameLength )
{
	float st = event->GetStartTime();
	float ed = event->GetEndTime();

	for ( int i = list.Head(); i != list.InvalidIndex(); i = list.Next( i ) )
	{
		CChoreoEvent *test = list[ i ];

		float teststart = test->GetStartTime();
		float testend = test->GetEndTime();

		// See if spans overlap
		if ( teststart >= ed )
			continue;
		if ( testend <= st )
			continue;

		// Now see if they deal with the same flex controller
		int tc = event->GetNumFlexAnimationTracks();
		for ( int track = 0; track < tc; ++track )
		{
			CFlexAnimationTrack *t = event->GetFlexAnimationTrack( track );
			if ( !t->IsTrackActive() )
			{
				continue;
			}

			int sampleCountNormal = t->GetNumSamples( 0 );
			int sampleCountBalance = 0;
			if ( t->IsComboType() )
			{
				sampleCountBalance = t->GetNumSamples( 1 );
			}

			if ( !sampleCountNormal && !sampleCountBalance )
				continue;

			// Otherwise, see if the test track has this as an active track
			if ( IsFlexTrackBeingUsed( test, t->GetFlexControllerName() ) )
			{
				Q_strncpy( trackName, t->GetFlexControllerName(), trackNameLength );
				return true;
			}
		}
		return false;
	}

	return false;
}

void CheckForOverlappingFlexTracks( CChoreoScene *scene )
{
	for ( int a = 0; a < scene->GetNumActors(); ++a )
	{
		CChoreoActor *actor = scene->GetActor( a );

		CUtlRBTree< CChoreoEvent * >  actorFlexEvents( 0, 0, ChoreEventStartTimeLessFunc );

		for ( int c = 0; c < actor->GetNumChannels(); ++c )
		{
			CChoreoChannel *channel = actor->GetChannel( c );
			
			for ( int e = 0 ; e < channel->GetNumEvents(); ++e )
			{
				CChoreoEvent *event = channel->GetEvent( e );

				if ( event->GetType() != CChoreoEvent::FLEXANIMATION )
					continue;

				actorFlexEvents.Insert( event );
			}
		}

		CUtlVector< CUtlLinkedList< CChoreoEvent*, int > >	rows;

		bool done = false;
		int i;
		// Now check for overlaps
		for ( i = actorFlexEvents.FirstInorder(); i != actorFlexEvents.InvalidIndex() && !done; i = actorFlexEvents.NextInorder( i ) )
		{
			CChoreoEvent *e = actorFlexEvents[ i ];
			if ( !rows.Count() )
			{
				rows.AddToTail();

				CUtlLinkedList< CChoreoEvent*, int >& list = rows[ 0 ];
				list.AddToHead( e );
				continue;
			}

			// Does it come totally after what's in rows[0]?
			int rowCount = rows.Count();
			bool addrow = true;

			for ( int j = 0; j < rowCount; j++ )
			{
				CUtlLinkedList< CChoreoEvent*, int >& list = rows[ j ];

				char offender[ 256 ];
				if ( !EventCollidesWithRows( list, e, offender, sizeof( offender ) ) )
				{
					// Update row event list
					list.AddToHead( e );
					addrow = false;
					break;
				}
				else
				{
					Msg( "[%s] has overlapping events for actor [%s] [%s] [flex: %s]\n",
						scene->GetFilename(), actor->GetName(), e->GetName(), offender );
					done = true;
				}
			}

			if ( addrow )
			{
				// Add a new row
				int idx = rows.AddToTail();
				CUtlLinkedList< CChoreoEvent *, int >& list = rows[ idx ];
				list.AddToHead( e );
			}
		}

		// Assert( rows.Count() <= 1 );
	}
}

void ProcessVCD( CUtlDict< VCDList, int >& database, CUtlSymbol& vcdname )
{
	// vprint( 0, "Processing '%s'\n", g_Analysis.symbols.String( vcdname ) );

	// Load the .vcd
	char fullname[ 512 ];
	Q_snprintf( fullname, sizeof( fullname ), "%s", g_Analysis.symbols.String( vcdname ) );

	LoadScriptFile( fullname );
	
	CChoreoScene *scene = ChoreoLoadScene( fullname, NULL, &g_TokenProcessor, Con_Printf );
	if ( scene )
	{
		bool first = true;
		// Now iterate the events looking for speak events
		int c = scene->GetNumEvents();
		for ( int i = 0; i < c; i++ )
		{
			CChoreoEvent *e = scene->GetEvent( i );

			if ( e->GetType() == CChoreoEvent::MOVETO )
			{
				SpewMoveto( first, fullname, e );
				first = false;
			}

			if ( e->GetType() != CChoreoEvent::SPEAK )
				continue;

			// Look up sound in sound emitter system
			char const *wavename = soundemitter->GetWavFileForSound( e->GetParameters(), NULL );
			if ( !wavename || !wavename[ 0 ] )
			{
				continue;
			}

			char fullwavename[ 512 ];
			Q_snprintf( fullwavename, sizeof( fullwavename ), "%ssound\\%s",
				gamedir, wavename );
			Q_FixSlashes( fullwavename );

			// Now add to proper slot
			VCDList *entry = NULL;

			// Add vcd to database
			int slot = database.Find( fullwavename );
			if ( slot == database.InvalidIndex() )
			{
				VCDList nullEntry;
				slot = database.Insert( fullwavename, nullEntry );
			}

			entry = &database[ slot ];
			if ( entry->vcds.Find( vcdname ) == entry->vcds.InvalidIndex() )
			{
				entry->vcds.AddToTail( vcdname );
			}
		}

		if ( vcdonly )
		{
			CheckForOverlappingFlexTracks( scene );
		}
	}
	
	delete scene;
}

void CorrelateWavsAndVCDs( CUtlVector< CUtlSymbol >& vcdfiles, CUtlVector< CUtlSymbol >& wavfiles )
{
	CUtlDict< VCDList, int >	database;

	int i;
	int c = vcdfiles.Count();
	for ( i = 0; i < c; i++ )
	{
		CUtlSymbol& vcdname = vcdfiles[ i ];

		// Load the .vcd and update the database
		ProcessVCD( database, vcdname );
	}

	if ( vcdonly )
		return;

	vprint( 0, "Found %i wav files in %i vcds\n",
		database.Count(), vcdfiles.Count() );

	// Now look for any wavfiles that weren't in the database
	int ecount = 0;

	c = wavfiles.Count();
	for ( i = 0; i < c; i++ )
	{
		CUtlSymbol& wavename = wavfiles[ i ];

		int idx = database.Find( g_Analysis.symbols.String( wavename ) );
		if ( idx != database.InvalidIndex() )
		{
			VCDList *listentry = &database[ idx ];
			int vcdcount = listentry->vcds.Count();
			if ( vcdcount >= 2 && verbose )
			{
				vprint( 0, " wave '%s' used by multiple .vcds:\n", g_Analysis.symbols.String( wavename ) );
				int j;
				for ( j = 0; j < vcdcount; j++ )
				{
					vprint( 1, "%i -- '%s'\n", j+1, g_Analysis.symbols.String( listentry->vcds[ j ] ) );
				}
			}
			continue;
		}

		vprint( 0, "%i -- '%s' not referenced by .vcd\n",
			++ecount, g_Analysis.symbols.String( wavename ) );
	}

	vprint( 0, "\nSummary:  found %i/%i (%.2f percent) .wav errors\n", ecount, c, 100.0 * ecount / max( c, 1 ) );
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
	SpewActivate( "vcd_sound_check", 2 );

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
			case 'm':
				spewmoveto = true;
				break;
			case 'o':
				vcdonly = true;
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 3 || ( i != argc ) )
	{
		PrintHeader();
		printusage();
	}

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Looking for .wav files not referenced in .vcd files...\n" );

	char sounddir[ 256 ];
	char vcddir[ 256 ];
	strcpy( sounddir, argv[ i - 2 ] );
	strcpy( vcddir, argv[ i - 1 ] );
	if ( !strstr( sounddir, "sound" ) )
	{
		vprint( 0, "Sound dir %s looks invalid (format:  u:/tf2/hl2/sound/vo)\n", sounddir );
		return 0;
	}
	if ( !strstr( vcddir, "scenes" ) )
	{
		vprint( 0, ".vcd dir %s looks invalid (format:  u:/tf2/hl2/scenes)\n", vcddir );
		return 0;
	}

	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

	// If they didn't specify -game on the command line, use VPROJECT.
	CmdLib_InitFileSystem( workingdir );

	CSysModule *pSoundEmitterModule = g_pFullFileSystem->LoadModule( "soundemittersystem.dll" );
	if ( !pSoundEmitterModule )
	{
		vprint( 0, "Sys_LoadModule( soundemittersystem.dll ) failed!\n" );
		return 0;		
	}

	CreateInterfaceFn hSoundEmitterFactory = Sys_GetFactory( pSoundEmitterModule );
	if ( !hSoundEmitterFactory )
	{
		vprint( 0, "Sys_GetFactory on soundemittersystem.dll failed!\n" );
		return 0;
	}

	soundemitter = ( ISoundEmitterSystemBase * )hSoundEmitterFactory( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL );
	if ( !soundemitter )
	{
		vprint( 0, "Couldn't get interface %s from soundemittersystem.dll!\n", SOUNDEMITTERSYSTEM_INTERFACE_VERSION );
		return 0;
	}

	filesystem = (IFileSystem *)(CmdLib_GetFileSystemFactory()( FILESYSTEM_INTERFACE_VERSION, NULL ));
	if ( !filesystem )
	{
		AssertMsg( 0, "Failed to create/get IFileSystem" );
		return 1;
	}

	Q_FixSlashes( gamedir );
	Q_strlower( gamedir );

	vprint( 0, "game dir %s\nsounds dir %s\nvcd dir %s\n\n",
		gamedir,
		sounddir,
		vcddir );

	Q_StripTrailingSlash( sounddir );
	Q_StripTrailingSlash( vcddir );


	filesystem->RemoveFile( "moveto.txt", "GAME" );
	//
	//ProcessMaterialsDirectory( vmtdir );

	vprint( 0, "Initializing sound emitter system\n" );
	soundemitter->Connect( FileSystem_GetFactory() );
	soundemitter->Init();

	vprint( 0, "Loaded %i sounds\n", soundemitter->GetSoundCount() );

	vprint( 0, "Building list of .vcd files\n" );
	CUtlVector< CUtlSymbol > vcdfiles;
	BuildFileList( vcdfiles, vcddir, ".vcd" );
	vprint( 0, "found %i .vcd files\n\n", vcdfiles.Count() );

	vprint( 0, "Building list of known .wav files\n" );
	CUtlVector< CUtlSymbol > wavfiles;
	BuildFileList( wavfiles, sounddir, ".wav" );
	vprint( 0, "found %i .wav files\n\n", wavfiles.Count() );

	CorrelateWavsAndVCDs( vcdfiles, wavfiles );

	soundemitter->Shutdown();
	soundemitter = 0;
	g_pFullFileSystem->UnloadModule( pSoundEmitterModule );

	FileSystem_Term();

	return 0;
}