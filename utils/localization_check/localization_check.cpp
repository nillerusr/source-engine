//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: localization_check.cpp : Defines the entry point for the console application.
//
//
// What the tool does:
//
// Load g_pSoundEmitterSystem, vgui::localize, soundcombiner system, vcd system
//
// Catalog all files in closecaption/ folder
// Iterate all known vcds
//
// 1)  For each sound emitted by a vcd not marked as CC_DISABLED, verify that there's an entry in the localization table
// 2)  For each combined sound in a vcd, make sure there's a valid entry in the localization table
// 3)  For each combined sound, verify that the english version combined .wav file has the proper checksum
// 4)  Note any files in the closecaption folder which are orphaned after parsing the above
// 5)  If hl2_french directories etc. exist, then compare combined .wav files with localized versions and 
//   see if localized checksum tag differs from US one, or warn if tag missing, but complain if .wav duration is different.
//
//	UNDONE:re-create combined .wav files in english to the extent that the checksums mismatch?
//
//===========================================================================//
#include "cbase.h"
#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <mmreg.h>
#include <direct.h>
#include "tier0/dbg.h"
#include "utldict.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "appframework/tier3app.h"
#include "vstdlib/random.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "choreoscene.h"
#include "choreoevent.h"
#include "choreochannel.h"
#include "choreoactor.h"
#include "iscenetokenprocessor.h"
#include "ifaceposersound.h"
#include "snd_audio_source.h"
#include "snd_wave_source.h"
#include "AudioWaveOutput.h"
#include "isoundcombiner.h"
#include "tier0/icommandline.h"
#include <vgui/ILocalize.h>
#include "vgui/ivgui.h"
#include "soundchars.h"
#include "sentence.h"
#include "tier2/riff.h"
#include "utlbuffer.h"
#include "FileSystem_Helpers.h"
#include "pacifier.h"
#include "phonemeextractor/PhonemeExtractor.h"
#include "UnicodeFileHelpers.h"

using namespace vgui;

bool uselogfile = false;
bool regenerate = false;
bool regenerate_quiet = false;
bool regenerate_all = false; // user hit a to y/n/all prompt
bool generate_usage = false;
bool nuke = false;
bool checkscriptsounds = false;
bool build_cc = false;
bool build_script = false;
bool wavcheck = false;
bool extractphonemes = false;
bool checkforloops = false;
bool importcaptions = false;
bool checkfordups = false;
bool makecopybatch = false;
bool syncducking = false;

char sounddir[ 512 ];
char importfile[ 512 ];  // for -i processing
char fromdir[ 512 ];
char todir[ 512 ];

struct AnalysisData
{
	CUtlSymbolTable				symbols;
};

IFileSystem *filesystem = NULL;

static AnalysisData g_Analysis;

static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;

static bool spewed = false;
static bool forceextract = false;

#define SOUND_DURATION_TOLERANCE	0.1f		// 100 msec of slop


void vprint( int depth, const char *fmt, ... );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  localization_check <opts> languagename\n\
		\t-v = verbose output\n\
		\t-l = log to file log.txt\n\
		\t-u = generate usage data for .vcds based on -makereslists maplist.txt files\n\
		\t-b = generate nuke.bat which will nuke all of the unreferenced .vcds from your tree\n\
		\t-s = generate list of unused sounds.txt entries\n\
		\t-c = build cc fixed/fixed2.txt files\n\
		\t-r = regenerate missing/mismatched combined wav files\n\
		\t\t-q = quiet mode during regenerate\n\
		languagename = check combined language files for existence and duration, 'english' for no extra checks\n\
		\t-x = build script of dialog from .vcds\n\
		\t-w sounddir = spew csv of wave files in directory, including sound and cc info\n\
		\t-e sounddir = do textless phoneme processing on files in dir and subdirs\n\
		\t-f	with above, forces extraction even if wav already has phonemes (danger!)\n\
		\t-i = import unicode wavename/caption into a new closecaption_test.txt file\n\
		\t-d = check for duplicated unicode strings\n\
		\t-p = pull raw english txt out of closecaption document, for spellchecking\n\
		\t-m = given a directory of .wav files finds the full directory path they should live in based on english\n\
		\t-a english_sound_dir localized_sound_dir = sets voice duck for sounds in localized_sound_dir to match the values in the english dir\n\
		\t-loop = warn on any sound files in specified directory having loop markers in the .wav\n\
		\ne.g.:  localization_check -l -w npc/metropolice/vo -r french\n" );

	// Exit app
	exit( 1 );
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

static char *va( char const *fmt, ... )
{
	static char string[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( string, sizeof(string), fmt, argptr );
	va_end( argptr );
	return string;
}

void cleanquotes( char *text )
{
	char *out = text;
	while ( *out )
	{
		if ( *out == '\"' )
		{
			*out++ = '\'';
		}
		else
		{
			++out;
		}
	}
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

void nuke_print( int depth, const char *fmt, ... )
{
	char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );
	
	static bool first = false;


	FILE *fp = NULL;
	
	char const *nukefile = "nuke.bat";

	if ( first )
	{
		first = false;
		fp = fopen( nukefile, "wb" );
	}
	else
	{
		fp = fopen( nukefile, "ab" );
	}

	while ( depth-- > 0 )
	{
		fprintf( fp, "  " );
	}

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

void Con_Printf( const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	Q_vsnprintf( output, sizeof( output ), fmt, args );
	va_end( args );

	vprint( 0, output );
}

void BuildFileList_R( CUtlVector< CUtlSymbol >& files, char const *dir, char const *extension )
{
	WIN32_FIND_DATA wfd;

	char directory[ 256 ];
	char filename[ MAX_PATH ];
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
	vprint( 0, "Valve Software - localization_check.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Voice Wav File .vcd Checker ---\n" );
}

char const *FacePoser_TranslateSoundNameGender( char const *soundname, gender_t gender )
{
	if ( Q_stristr( soundname, ".wav" ) )
		return PSkipSoundChars( soundname );

	return PSkipSoundChars( g_pSoundEmitterSystem->GetWavFileForSound( soundname, gender ) );
}

//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)g_pFullFileSystem->Open( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( !file )
			return 0;

		return g_pFullFileSystem->Read( pOutput, size, (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		if ( !file )
			return;

		g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		if ( !file )
			return 0;

		return g_pFullFileSystem->Tell( (FileHandle_t)file );
	}

	unsigned int size( int file )
	{
		if ( !file )
			return 0;

		return g_pFullFileSystem->Size( (FileHandle_t)file );
	}

	void close( int file )
	{
		if ( !file )
			return;

		g_pFullFileSystem->Close( (FileHandle_t)file );
	}
};


class StdIOWriteBinary : public IFileWriteBinary
{
public:
	int create( const char *pFileName )
	{
		g_pFullFileSystem->SetFileWritable( pFileName, true, "GAME" );
		return (int)g_pFullFileSystem->Open( pFileName, "wb" );
	}

	int write( void *pData, int size, int file )
	{
		return g_pFullFileSystem->Write( pData, size, (FileHandle_t)file );
	}

	void close( int file )
	{
		g_pFullFileSystem->Close( (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		return g_pFullFileSystem->Tell( (FileHandle_t)file );
	}
};

static StdIOWriteBinary io_out;
static StdIOReadBinary io_in;

#define RIFF_WAVE			MAKEID('W','A','V','E')
#define WAVE_FMT			MAKEID('f','m','t',' ')
#define WAVE_DATA			MAKEID('d','a','t','a')
#define WAVE_FACT			MAKEID('f','a','c','t')
#define WAVE_CUE			MAKEID('c','u','e',' ')

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &walk - 
//-----------------------------------------------------------------------------
static void ParseSentence( CSentence& sentence, IterateRIFF &walk )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( walk.ChunkSize() );
	walk.ChunkRead( buf.Base() );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );

	sentence.InitFromDataChunk( buf.Base(), buf.TellPut() );
}

bool LoadSentenceFromWavFileUsingIO( char const *wavfile, CSentence& sentence, IFileReadBinary& io, void *formatbuffer = NULL, int* formatsize = NULL, int *datasize = NULL )
{
	int insize = 0;

	if ( formatsize )
	{
		insize = *formatsize;
		*formatsize = 0;
	}

	if ( datasize )
	{
		*datasize = 0;
	}

	sentence.Reset();

	InFileRIFF riff( wavfile, io );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return false;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	bool found = false;
	while ( walk.ChunkAvailable( ) )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_FMT:
			{
				if ( formatbuffer && formatsize )
				{
					if ( walk.ChunkSize() <= insize )
					{
						*formatsize = walk.ChunkSize();
						walk.ChunkRead( formatbuffer );
					}	
					else
					{
						Error( "oops, format tag too big!!!" );
					}
				}
			}
			break;
		case WAVE_VALVEDATA:
			{
				found = true;
				ParseSentence( sentence, walk );
			}
			break;
		case WAVE_DATA:
			{
				if ( datasize )
				{
					*datasize = walk.ChunkSize();
				}
			}
			break;
		}
		walk.ChunkNext();
	}

	return true;
}

bool LoadSentenceFromWavFile( char const *wavfile, CSentence& sentence, void *formatbuffer = NULL, int* formatsize = NULL, int *dataSize = NULL )
{
	return LoadSentenceFromWavFileUsingIO( wavfile, sentence, io_in, formatbuffer, formatsize, dataSize );
}

bool ValidateCombinedFileCheckSum( char const *outfilename, char const *cctoken, gender_t gender, CUtlRBTree< CChoreoEvent * >& sorted, CUtlRBTree< CUtlSymbol, int >& referencedcaptionwaves )
{
	CUtlVector< CombinerEntry > work;

	char actualfile[ 512 ];
	g_pSoundEmitterSystem->GenderExpandString( gender, outfilename, actualfile, sizeof( actualfile ) );
	if ( Q_strlen( actualfile ) <= 0 )
	{
		return false;
	}

	int i = sorted.FirstInorder();
	if ( i != sorted.InvalidIndex() )
	{
		CChoreoEvent *e = sorted[ i ];

		float startoffset = e->GetStartTime();

		do
		{
			e = sorted[ i ];

			float curoffset = e->GetStartTime();

			CombinerEntry ce;
			Q_snprintf( ce.wavefile, sizeof( ce.wavefile ), "sound/%s", FacePoser_TranslateSoundNameGender( e->GetParameters(), gender ) );
			ce.startoffset = curoffset - startoffset;

			work.AddToTail( ce );

			i = sorted.NextInorder( i );
		}
		while ( i != sorted.InvalidIndex() );

		int c = work.Count();

		char worklist[ 2048 ];

		worklist[ 0 ]  = 0;

		for ( i = 0; i < c; ++i )
		{
			CombinerEntry &item = work[ i ];
			Q_strncat( worklist, item.wavefile, sizeof( worklist ), COPY_ALL_CHARACTERS );
			if ( i != c - 1 )
			{
				Q_strncat( worklist, ", ", sizeof( worklist ), COPY_ALL_CHARACTERS );
			}
		}

		logprint( "cc_combined.txt", "combined .wav '%s':  %s\n", actualfile, worklist );
	}

	bool valid = soundcombiner->IsCombinedFileChecksumValid( g_pFullFileSystem, actualfile, work );
	if ( !valid )
	{
		vprint( 0, "combined file (%s) checksum mismatch for '%s'\n  event '%s' of scene '%s'\n", actualfile, cctoken,
			sorted[0]->GetName(), sorted[0]->GetScene()->GetFilename() );

		if ( regenerate )
		{
			bool dothisfile = false;
			if ( !regenerate_all )
			{
				vprint( 0, "Regenerate '%s'? (Yes/No/All)", actualfile );
				char ch = getch();
				if ( ch == 'y' || ch == 'Y' )
				{
					dothisfile = true;
				}
				if ( ch == 'a' || ch == 'A' )
				{	
					regenerate_all = true;
					dothisfile = true;
				}
				vprint( 0, "\n" );
			}
			else
			{
				dothisfile = true;
			}

			if ( dothisfile )
			{
				bool success = soundcombiner->CombineSoundFiles( g_pFullFileSystem, actualfile, work );
				vprint( 1, "%s:  %s\n", actualfile, success ? "succeeded" : "FAILED" );
			}
		}
	}
	else
	{
		if ( regenerate )
		{
			vprint( 0, "combined file (%s) checksum still matches for %s, skipping rebuild...\n", actualfile, cctoken );
		}
	}

	// Mark the file as referenced
	//
	char fn[ 512 ];
	Q_snprintf( fn, sizeof( fn ), "%s%s", gamedir, actualfile );

	_strlwr( fn );
	Q_FixSlashes( fn );
	CUtlSymbol sym = g_Analysis.symbols.AddString( fn );
	
	if ( referencedcaptionwaves.Find( sym ) == referencedcaptionwaves.InvalidIndex() )
	{
		referencedcaptionwaves.Insert( sym );
	}

	return valid;
}

static bool EventStartTimeLessFunc( CChoreoEvent * const &p1, CChoreoEvent * const  &p2 )
{
	CChoreoEvent *w1;
	CChoreoEvent *w2;

	w1 = const_cast< CChoreoEvent * >( p1 );
	w2 = const_cast< CChoreoEvent * >( p2 );

	return w1->GetStartTime() < w2->GetStartTime();
}

static bool SymbolLessFunc( const CUtlSymbol & p1, const CUtlSymbol &p2 )
{
	if ( Q_stricmp( g_Analysis.symbols.String( p1 ), g_Analysis.symbols.String( p2 ) ) < 0 )
		return true;
	return false;
}

bool ValidateCombinedSoundCheckSum( CChoreoEvent *e, CUtlRBTree< CUtlSymbol, int >& referencedcaptionwaves )
{
	if ( !e || e->GetType() != CChoreoEvent::SPEAK )
		return false;

	bool genderwildcard = e->IsCombinedUsingGenderToken();
	char outfilename[ 512 ];
	Q_memset( outfilename, 0, sizeof( outfilename ) );
	if ( !e->ComputeCombinedBaseFileName( outfilename, sizeof( outfilename ), genderwildcard ) )
	{
		vprint( 0, "Unable to regenerate wav file name for combined sound (%s)\n", e->GetCloseCaptionToken() );
		return false;
	}

	bool checksumvalid = false;

	CUtlRBTree< CChoreoEvent * > eventList( 0, 0, EventStartTimeLessFunc );

	if ( !e->GetChannel()->GetSortedCombinedEventList( e->GetCloseCaptionToken(), eventList ) )
	{
		vprint( 0, "Unable to generated combined event list (%s)\n", e->GetCloseCaptionToken() );
		return false;
	}


	if ( genderwildcard )
	{
		checksumvalid = ValidateCombinedFileCheckSum( outfilename, e->GetCloseCaptionToken(), GENDER_MALE, eventList, referencedcaptionwaves );
		checksumvalid &= ValidateCombinedFileCheckSum( outfilename, e->GetCloseCaptionToken(), GENDER_FEMALE, eventList, referencedcaptionwaves );
	}
	else
	{
		checksumvalid = ValidateCombinedFileCheckSum( outfilename, e->GetCloseCaptionToken(), GENDER_NONE, eventList, referencedcaptionwaves );
	}

	return checksumvalid;
}

struct PerMapVCDS
{
	PerMapVCDS()
	{
	}

	PerMapVCDS( const PerMapVCDS& src )
	{
		int i = src.vcds.FirstInorder();
		while ( i != src.vcds.InvalidIndex() )
		{
			vcds.Insert( src.vcds[ i ] );
			i = src.vcds.NextInorder( i );
		}
	}

	class CTree : public CUtlRBTree< CUtlSymbol >
	{
	public:
		CTree()
			: CUtlRBTree< CUtlSymbol >( 0, 0, DefLessFunc( CUtlSymbol ) )
		{
		}

		CTree &operator=( const CTree &from )
		{
			CopyFrom( from );
			return *this;
		}
	};

	CTree	vcds;
};

CUtlDict< PerMapVCDS, int >	g_PerMapVCDS;
CUtlDict< CUtlSymbol, int >	g_FirstMapForVCD;

void ParseVCDFilesFromResList( CUtlVector< CUtlSymbol >& vcdsinreslist, char const *resfile )
{
	char gd[ 256 ];
	Q_strncpy( gd, gamedir, sizeof( gd ) );
	Q_StripTrailingSlash( gd );
	_strlwr( gd );
	Q_FixSlashes( gd );

	int gdlen = strlen( gd );

	char resbase[ 512 ];
	Q_FileBase( resfile, resbase, sizeof( resbase ) );

	int addedStrings = 0;
	int resourcesConsidered = 0;

	FileHandle_t resfilehandle;
	resfilehandle = g_pFullFileSystem->Open( resfile, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
	{
		// Read in the entire file
		int length = g_pFullFileSystem->Size(resfilehandle);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFullFileSystem->Read(pStart, length, resfilehandle) )
			   )
			{
				pStart[ length ] = 0;

				char *pFileList = pStart;

				char tokenFile[512];

				while ( 1 )
				{
					pFileList = ParseFile( pFileList, tokenFile, NULL );
					if ( !pFileList )
						break;

					if ( strlen( tokenFile ) > 0 )
					{
						char szFileName[ 256 ];
						Q_strncpy( szFileName, tokenFile, sizeof( szFileName ) );
						_strlwr( szFileName );
						Q_FixSlashes( szFileName );
						while ( szFileName[ strlen( szFileName ) - 1 ] == '\n' ||
							szFileName[ strlen( szFileName ) - 1 ] == '\r' )
						{
							szFileName[ strlen( szFileName ) - 1 ] = 0;
						}

						char *pFile = szFileName;
						if ( !Q_strnicmp( szFileName, gd, gdlen ) )
						{
							pFile = szFileName + gdlen + 1;
						}
						else
						{
							// Ack
							//vprint( 1, "File %s not under game directory but in reslist, skipping!!!\n", szFileName );
							pFileList = ParseFile( pFileList, tokenFile, NULL );
							continue;
						}

						++resourcesConsidered;

						// Is it a .vcd?
						if ( !Q_stristr( pFile, ".vcd" ) )
							continue;

						char symname[ 512 ];
						Q_snprintf( symname, sizeof( symname ), "%s%s", gamedir, pFile );
						_strlwr( symname );
						Q_FixSlashes( symname );

						CUtlSymbol sym = g_Analysis.symbols.AddString( symname );

						int idx = vcdsinreslist.Find( sym );
						if ( idx == vcdsinreslist.InvalidIndex() )
						{
							++addedStrings;

							// This is the first time this vcd was encountered, remember which map we are in
							PerMapVCDS e;
							e.vcds.Insert( sym );
							g_PerMapVCDS.Insert( resbase, e );

							CUtlSymbol mapsym = g_Analysis.symbols.AddString( resbase );
							g_FirstMapForVCD.Insert( symname, mapsym );

							vcdsinreslist.AddToTail( sym );
						}
					}
				}

			}
			delete[] pStart;
		}

		g_pFullFileSystem->Close(resfilehandle);
	}

//	int filesFound = addedStrings;
//	vprint( 1, "\rFound %i new resources (%7i total) in %64s", filesFound, resourcesConsidered, resfile );
}

#define MAPLIST_FILE	"maplist.txt"

void AddFileToList( CUtlVector< CUtlSymbol >& list, char const *filename )
{
	char fn[ 512 ];
	Q_strncpy( fn, filename, sizeof( fn ) );
	_strlwr( fn );
	Q_FixSlashes( fn );
	CUtlSymbol sym = g_Analysis.symbols.AddString( fn );
	list.AddToTail( sym );
}

void BuildVCDAndMapNameListsFromReslists( CUtlVector< CUtlSymbol >& vcdsinreslist )
{
	// Load all .rst files in the reslists folder
	CUtlVector< CUtlSymbol > reslists;

	// If maplist.txt exists, use it, otherwise
	bool loaded = false;

	if ( g_pFullFileSystem->FileExists( MAPLIST_FILE ) )
	{
		// Parse the true list from the maplist.txt file
		// and add engine.lst and all.lst at the very end

		// Load them in
		FileHandle_t resfilehandle;
		resfilehandle = g_pFullFileSystem->Open( MAPLIST_FILE, "rb" );
		if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
		{
			// Read in and parse mapcycle.txt
			int length = g_pFullFileSystem->Size(resfilehandle);
			if ( length > 0 )
			{
				char *pStart = (char *)new char[ length + 1 ];
				if ( pStart && ( length == g_pFullFileSystem->Read(pStart, length, resfilehandle) )
				   )
				{
					pStart[ length ] = 0;
					const char *pFileList = pStart;

					while ( 1 )
					{
						char szMap[ 512 ];

						pFileList = ParseFile( pFileList, com_token, NULL );

						if ( strlen( com_token ) <= 0 )
							break;

						Q_strncpy(szMap, com_token, sizeof(szMap));

						// Any more tokens on this line?
						//while ( TokenWaiting( pFileList ) )
						//{
						//	pFileList = ParseFile( pFileList, com_token, NULL );
						//}

						char fn[ 512 ];
						Q_snprintf( fn, sizeof( fn ), "%sreslists/%s.lst", gamedir, szMap );

						AddFileToList( reslists, fn );
					}
				}
				delete[] pStart;

				AddFileToList( reslists, va( "%sreslists/engine.lst", gamedir ) );
				AddFileToList( reslists, va( "%sreslists/all.lst", gamedir ) );

				loaded = true;
			}

			g_pFullFileSystem->Close(resfilehandle);
		}
	}
	

	if ( !loaded )
	{
		char reslistdir[ 512 ];
		Q_snprintf( reslistdir, sizeof( reslistdir ), "%sreslists", gamedir );

		BuildFileList_R( reslists, reslistdir, ".lst" );
	}

	int c = reslists.Count();

	StartPacifier( "ParseVCDFilesFromResList:  " );

	for ( int i = 0; i < c; ++i )
	{
		UpdatePacifier( (float)( i + 1 ) / (float)c );
		ParseVCDFilesFromResList( vcdsinreslist, g_Analysis.symbols.String( reslists[ i ] ) );
	}

	EndPacifier( true );
}

void CheckUnusedVcds( CUtlVector< CUtlSymbol >& vcdsinreslist, CUtlVector< CUtlSymbol >& vcdfiles )
{
	vprint( 1, "Checking for orphaned vcd files\n" );

	// For each reslist, load in the filenames, looking for .vcds
	vprint( 1, "Found %i .vcd files referenced (%i total)\n", vcdsinreslist.Count(), vcdfiles.Count() );

	// For each vcd in the min list, see if it's in the sublist
	int i;
	int c = vcdfiles.Count();

	int invalid_index = vcdsinreslist.InvalidIndex();

	int unrefcount = 0;

	for ( i = 0; i < c; ++i )
	{
		CUtlSymbol& sym = vcdfiles[ i ];

		if ( vcdsinreslist.Find( sym ) == invalid_index )
		{
			++unrefcount;
			vprint( 1, "  unref .vcd:  %s\n", g_Analysis.symbols.String( sym ) );

			if ( nuke )
			{
				nuke_print( 0, "del %s /f\n", g_Analysis.symbols.String( sym ) );
			}
		}
	}

	// For each reslist, load in the filenames, looking for .vcds
	vprint( 1, "Found %i unreferenced vcds (%i total)\n", unrefcount, vcdfiles.Count() );

}

void ParseUsedSoundsFromSndFile( CUtlRBTree< int, int >& usedsounds, char const *sndfile )
{
	char gd[ 256 ];
	Q_strncpy( gd, gamedir, sizeof( gd ) );
	Q_StripTrailingSlash( gd );
	_strlwr( gd );
	Q_FixSlashes( gd );

	int addedStrings = 0;
	int resourcesConsidered = 0;

	FileHandle_t resfilehandle;
	resfilehandle = g_pFullFileSystem->Open( sndfile, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
	{
		// Read in the entire file
		int length = g_pFullFileSystem->Size(resfilehandle);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFullFileSystem->Read(pStart, length, resfilehandle) )
			   )
			{
				pStart[ length ] = 0;

				char *pFileList = pStart;

				char tokenFile[512];

				while ( 1 )
				{
					pFileList = ParseFile( pFileList, tokenFile, NULL );
					if ( !pFileList )
						break;

					if ( strlen( tokenFile ) > 0 )
					{
						char soundname[ 256 ];
						Q_strncpy( soundname, tokenFile, sizeof( soundname ) );
						_strlwr( soundname );

						++resourcesConsidered;

						int index = g_pSoundEmitterSystem->GetSoundIndex( soundname );
						if ( !g_pSoundEmitterSystem->IsValidIndex( index ) )
						{
							vprint( 1, "---> Sound %s doesn't exist in g_pSoundEmitterSystemsystem!!!\n", soundname );
							continue;
						}

						int idx = usedsounds.Find( index );
						if ( idx == usedsounds.InvalidIndex() )
						{
							++addedStrings;
							usedsounds.Insert( index );
						}
					}
				}

			}
			delete[] pStart;
		}

		g_pFullFileSystem->Close(resfilehandle);
	}

	vprint( 1, "Found %i new resources (%i total) in %s\n", addedStrings, resourcesConsidered, sndfile );
}

bool SplitCommand( wchar_t const **ppIn, wchar_t *cmd, wchar_t *args );

void SpewDuplicatedText( char const *lang, const char *entry, const wchar_t *str )
{
	const wchar_t *curpos = str;

	wchar_t cleaned[ 4096 ];

	wchar_t *out = cleaned;
	
	for ( ; curpos && *curpos != L'\0'; ++curpos )
	{
		wchar_t cmd[ 256 ];
		wchar_t args[ 256 ];

		if ( SplitCommand( &curpos, cmd, args ) )
		{
			continue;
		}

		// Only copy non command, non-whitespace characters
		if ( iswspace( *curpos ) )
		{
			continue;
		}

		*out++ = *curpos;
	}

	*out = L'\0';

	int len = wcslen( cleaned );
	if ( len < 5 )
		return;

	// Now see how many characters from the first 50% of the text are also in the second 50%
	int halflen = len / 2;

	int foundcount = 0;
	for ( int i = 0; i < halflen; ++i )
	{
		wchar_t ch[3];
		ch[0] = cleaned[ i ];
		ch[1] = cleaned[ i +  1 ];
		ch[2] = L'\0';

		if ( wcsstr( &cleaned[ halflen ], ch ) )
		{
			++foundcount;
		}
	}

	if ( foundcount > 0.7 * halflen )
	{
		logprint( "cc_duplicatedtext.txt", "%s:  Suspect token %s\n", lang, entry );
	}
}

void CheckDuplcatedText( void )
{
	g_pFullFileSystem->RemoveFile( "cc_duplicatedtext.txt", "GAME" );

	for ( int lang = 0; lang < CC_NUM_LANGUAGES; ++lang )
	{
		char language[ 256 ];
		Q_strncpy( language, CSentence::NameForLanguage( lang ), sizeof( language ) );

		vprint( 0, "adding langauge file for '%s'\n", language );

		g_pVGuiLocalize->AddFile( "resource/closecaption_english.txt" );

		char fn[ 256 ];
		Q_snprintf( fn, sizeof( fn ), "resource/closecaption_%s.txt", language );

		g_pVGuiLocalize->AddFile( fn );
	
		// Now check for closecaption_xxx.txt entries which are orphaned because there isn't an existing sound script entry in use for them
		StringIndex_t str = g_pVGuiLocalize->GetFirstStringIndex();
		while ( str != INVALID_LOCALIZE_STRING_INDEX )
		{
			char const *keyname = g_pVGuiLocalize->GetNameByIndex( str );
			if ( keyname )
			{
				const wchar_t *value = g_pVGuiLocalize->GetValueByIndex( str );

				SpewDuplicatedText( language, keyname, value );
			}

			str = g_pVGuiLocalize->GetNextStringIndex( str );
		}
	}
}

static bool IsAllSpaces( const wchar_t *stream )
{
	const wchar_t *p = stream;
	while ( *p != L'\0' )
	{
		if ( !iswspace( *p ) )
			return false;

		p++;
	}

	return true;
}

void SpewEnglishText( const wchar_t *str )
{
	const wchar_t *curpos = str;

	wchar_t cleaned[ 4096 ];

	wchar_t *out = cleaned;
	
	for ( ; curpos && *curpos != L'\0'; ++curpos )
	{
		wchar_t cmd[ 256 ];
		wchar_t args[ 256 ];

		if ( SplitCommand( &curpos, cmd, args ) )
		{
			continue;
		}

		*out++ = *curpos;
	}

	*out = L'\0';

	if ( IsAllSpaces( cleaned ) )
		return;

	char ansi[ 4096 ];
	g_pVGuiLocalize->ConvertUnicodeToANSI( cleaned, ansi, sizeof( ansi ) );

	logprint( "cc_english.txt", "\"%s\"\n", ansi );
}

void ExtractEnglish()
{
	g_pFullFileSystem->RemoveFile( "cc_english.txt", "GAME" );
	// Now check for closecaption_xxx.txt entries which are orphaned because there isn't an existing sound script entry in use for them
	StringIndex_t str = g_pVGuiLocalize->GetFirstStringIndex();
	while ( str != INVALID_LOCALIZE_STRING_INDEX )
	{
		char const *keyname = g_pVGuiLocalize->GetNameByIndex( str );
		if ( keyname )
		{
			const wchar_t *value = g_pVGuiLocalize->GetValueByIndex( str );

			SpewEnglishText( value );
		}

		str = g_pVGuiLocalize->GetNextStringIndex( str );
	}
}

void CheckUnusedSounds()
{
	vprint( 1, "Checking for unused sounds.txt entries\n" );

	CUtlRBTree< int, int > usedsounds( 0, 0, DefLessFunc(int) );

	// Load all .snd files in the reslists folder
	CUtlVector< CUtlSymbol > sndlists;

	char reslistdir[ 512 ];
	Q_snprintf( reslistdir, sizeof( reslistdir ), "%sreslists", gamedir );

	BuildFileList_R( sndlists, reslistdir, ".snd" );

	int c = sndlists.Count();

	for ( int i = 0; i < c; ++i )
	{
		ParseUsedSoundsFromSndFile( usedsounds, g_Analysis.symbols.String( sndlists[ i ] ) );
	}

	// For each reslist, load in the filenames, looking for .vcds
	vprint( 1, "Found %i unique sounds referenced\n", usedsounds.Count() );

	// For each vcd in the min list, see if it's in the sublist
	c = g_pSoundEmitterSystem->GetSoundCount();

	int unrefcount = 0;

	int invalidindex = usedsounds.InvalidIndex();

	CUtlRBTree< int, int > usedscripts( 0, 0, DefLessFunc(int) );

	for ( int i = 0; i < c; ++i )
	{
		int slot = usedsounds.Find( i );
		if ( invalidindex == slot )
		{
			++unrefcount;
			char const *soundname = g_pSoundEmitterSystem->GetSoundName( i );

			vprint( 1, "  unref:  %s : %s\n", soundname, g_pSoundEmitterSystem->GetSourceFileForSound( i ) );
		}
		else
		{
			int scriptindex = g_pSoundEmitterSystem->FindSoundScript( g_pSoundEmitterSystem->GetSourceFileForSound( i ) ); 
			if ( scriptindex != -1 )
			{
				slot = usedscripts.Find( scriptindex );
				if ( usedscripts.InvalidIndex() == slot )
				{
					usedscripts.Insert( scriptindex );
				}
			}
		}
	}

	// For each reslist, load in the filenames, looking for .vcds
	vprint( 1, "Found %i unreferenced sounds (%i total)\n", unrefcount, c );

	c = g_pSoundEmitterSystem->GetNumSoundScripts();
	for ( int i = 0; i < c; ++i )
	{
		char const *scriptname = g_pSoundEmitterSystem->GetSoundScriptName( i );

		int slot = usedscripts.Find( i );
		if ( usedscripts.InvalidIndex() == slot )
		{
			vprint( 1, "  No sounds fron script %s are being used, should delete from manifest!!!\n", scriptname );
		}
	}

	if ( !build_cc )
		return;

	g_pFullFileSystem->RemoveFile( "fixed.txt", "GAME" );
	g_pFullFileSystem->RemoveFile( "fixed2.txt", "GAME" );
	g_pFullFileSystem->RemoveFile( "todo.csv", "GAME" );
	g_pFullFileSystem->RemoveFile( "cc_add.txt", "GAME" );
	g_pFullFileSystem->RemoveFile( "cc_delete.txt", "GAME" );
	g_pFullFileSystem->RemoveFile( "cc_foundphonemes.txt", "GAME" );
	g_pFullFileSystem->RemoveFile( "cc_combined.txt", "GAME" );

	logprint( "todo.csv", "\"CC_TOKEN\",\"TEXT\",\"WAVE FILE\"\n" );
			
	// Now check for closecaption_xxx.txt entries which are orphaned because there isn't an existing sound script entry in use for them
	StringIndex_t str = g_pVGuiLocalize->GetFirstStringIndex();
	while ( str != INVALID_LOCALIZE_STRING_INDEX )
	{
		char const *keyname = g_pVGuiLocalize->GetNameByIndex( str );

		if ( keyname )
		{
			wchar_t *value = g_pVGuiLocalize->GetValueByIndex( str );

			char ansi[ 512 ];
			g_pVGuiLocalize->ConvertUnicodeToANSI( value, ansi, sizeof( ansi ) );

			// See if key exists in g_pSoundEmitterSystem system
			int soundindex = g_pSoundEmitterSystem->GetSoundIndex( keyname );
			if( soundindex == -1 )
			{
				vprint( 1, "  cc token %s not in current g_pSoundEmitterSystem scripts\n", keyname );

				// Just write it back out as is...
				logprint( "fixed2.txt", "\t\"%s\"\t\t\"%s\"\n", keyname, ansi );
			}
			else
			{
				// See if it's referenced
				int slot = usedsounds.Find( soundindex );
				if ( usedsounds.InvalidIndex() == slot )
				{
					vprint( 1, "  cc token %s exists, but the sound is not used by the game\n", keyname );

					logprint( "cc_delete.txt", "\"%s\"\n", keyname );
				}
				else
				{
					// Now try to find a better bit of text
					CSoundParametersInternal *internal = g_pSoundEmitterSystem->InternalGetParametersForSound( soundindex );
					if ( internal && internal->NumSoundNames() > 0 )
					{
						CUtlSymbol &symwave = internal->GetSoundNames()[ 0 ].symbol;
						char const *wavname = g_pSoundEmitterSystem->GetWaveName( symwave );
						if ( wavname && ( Q_stristr( wavname, "vo/" ) || Q_stristr( wavname, "vo\\" ) ) )
						{
							// See if 1) it's marked as !!! and try to figure out the text from .wav files...
							if ( !Q_strnicmp( ansi, "!!!", 3 ) )
							{
								CSentence sentence;
								if ( LoadSentenceFromWavFile( va( "sound/%s", PSkipSoundChars( wavname ) ), sentence ) )
								{
									if ( Q_strlen( sentence.GetText() ) > 0 ) 
									{
										Q_snprintf( ansi, sizeof( ansi ), "%s", sentence.GetText() );
										cleanquotes( ansi );

										logprint( "cc_foundphonemes.txt", "\t\"%s\"\t\t\"%s\"\n", keyname, ansi );
									}
								}
							}

							logprint( "fixed.txt", "\t\"%s\"\t\t\"%s\"\n", keyname, ansi );

							for ( int w = 0; w < internal->NumSoundNames() ; ++w )
							{
								wavname = g_pSoundEmitterSystem->GetWaveName( internal->GetSoundNames()[ w ].symbol );
								logprint( "todo.csv", "\"%s\",\"%s\",\"%s\"\n",
									keyname, ansi, va( "sound/%s", PSkipSoundChars( wavname ) ) );
							}
						}
					}
					else
					{
						logprint( "fixed.txt", "\t\"%s\"\t\t\"%s\"\n", keyname, ansi );
					}
				}
			}
		}
		str = g_pVGuiLocalize->GetNextStringIndex( str );
	}

	// Now walk through all of the sounds that were used, but not in the localization file and and those, too
	c = g_pSoundEmitterSystem->GetSoundCount();
	for ( int i = 0; i < c; ++i )
	{
		int slot = usedsounds.Find( i );
		if ( usedsounds.InvalidIndex() == slot )
			continue;

		char const *soundname = g_pSoundEmitterSystem->GetSoundName( i );

		// See if it exists in the localization file
		wchar_t *text = g_pVGuiLocalize->Find( soundname );
		if ( text )
		{
			continue;
		}
		else
		{
			char ansi[ 512 ];
			Q_snprintf( ansi, sizeof( ansi ), "!!!%s", soundname );

			// Now try to find a better bit of text
			CSoundParametersInternal *internal = g_pSoundEmitterSystem->InternalGetParametersForSound( i );
			if ( internal && internal->NumSoundNames() > 0 )
			{
				CUtlSymbol &symwave = internal->GetSoundNames()[ 0 ].symbol;
				char const *wavname = g_pSoundEmitterSystem->GetWaveName( symwave );
				if ( wavname && ( Q_stristr( wavname, "vo/" ) || Q_stristr( wavname, "vo\\" ) ) )
				{
					CSentence sentence;
					if ( LoadSentenceFromWavFile( va( "sound/%s", PSkipSoundChars( wavname ) ), sentence ) )
					{
						if ( Q_strlen( sentence.GetText() ) > 0 )
						{
							Q_snprintf( ansi, sizeof( ansi ), "%s", sentence.GetText() );
							cleanquotes( ansi );
						}
					}

					// Add an entry for stuff in vo/
					logprint( "fixed.txt", "\t\"%s\"\t\t\"%s\"\n", soundname, ansi );

					for ( int w = 0; w < internal->NumSoundNames() ; ++w )
					{
						wavname = g_pSoundEmitterSystem->GetWaveName( internal->GetSoundNames()[ w ].symbol );
						logprint( "todo.csv", "\"%s\",\"%s\",\"%s\"\n",
							soundname, ansi, va( "sound/%s", PSkipSoundChars( wavname ) ) );
					}

					logprint( "cc_add.txt", "\"%s\"\n", soundname );
				}
			}
		}
	}
}

// Removes commas from text
void RemoveCommas( char *in )
{
	char *out = in;
	while ( out && *out )
	{
		if ( *in == ',' )
		{
			*out++ = ';';
			in++;
		}
		else
		{
			*out++ = *in++;
		}
	}
	*out = 0;
}

void SpewScript( char const *vcdname, CUtlRBTree< CChoreoEvent *, int >& list )
{
	if ( !build_script )
		return;

	if ( list.Count() == 0 )
		return;

	logprint( "script.txt", "VCD( %s )\n\n", vcdname );

	for ( int i = list.FirstInorder(); i != list.InvalidIndex(); i = list.NextInorder( i ) )
	{
		CChoreoEvent *e = list[ i ];

		if ( e->GetCloseCaptionType() != CChoreoEvent::CC_MASTER )
		{
			continue;
		}

		char actorname[ 512 ];

		if ( e->GetActor() )
		{
			Q_strncpy( actorname, e->GetActor()->GetName(), sizeof( actorname ) );
			_strupr( actorname );
		}
		else
		{
			Q_strncpy( actorname, "(NULL ACTOR)", sizeof( actorname ) );

		}

		logprint( "script.txt", "\t\t\t%s\n", actorname);


		// Now try to find a better bit of text

		char wavname[ 512 ];
		wavname[ 0 ] = 0;

		char sentence_text[ 1024 ];
		sentence_text[ 0  ] = 0;

		int soundindex = g_pSoundEmitterSystem->GetSoundIndex( e->GetParameters() );
		if ( soundindex != -1 )
		{
			CSoundParametersInternal *internal = g_pSoundEmitterSystem->InternalGetParametersForSound( soundindex );
			if ( internal && internal->NumSoundNames() > 0 )
			{
				CUtlSymbol &symwave = internal->GetSoundNames()[ 0 ].symbol;
				char const *pname = g_pSoundEmitterSystem->GetWaveName( symwave );
				if ( pname && ( Q_stristr( pname, "vo/" ) || Q_stristr( pname, "vo\\" ) || Q_stristr( pname, "combined" ) ) )
				{
					Q_strncpy( wavname, pname, sizeof( wavname ) );

					// Convert to regular text
					logprint( "script.txt", "\t\t\t\twav(%s)\n", wavname );

					CSentence sentence;
					if ( LoadSentenceFromWavFile( va( "sound/%s", PSkipSoundChars( wavname ) ), sentence ) )
					{
						if ( Q_strlen( sentence.GetText() ) > 0 ) 
						{
							Q_snprintf( sentence_text, sizeof( sentence_text ), "%s", sentence.GetText() );
							cleanquotes( sentence_text );
						}
					}
				}
			}
		}

		char tok[ 256 ];
		Q_strncpy( tok, e->GetParameters(), sizeof( tok ) );

		char ansi[ 2048 ];
		ansi[ 0 ] = 0;

		wchar_t *str = g_pVGuiLocalize->Find( tok );
		if ( !str )
		{
			logprint( "script.txt", "\t\tMissing token '%s' event '%s'\n\n", tok, e->GetName() );
			Q_snprintf( ansi, sizeof( ansi ), "missing '%s' for '%s'", tok, e->GetName() );
		}
		else
		{
			if ( !wcsncmp( str, L"!!!", wcslen( L"!!!" ) ) )
			{
				logprint( "script.txt", "\t\t'%s':  event '%s'\n\n", tok, e->GetName() );
				Q_snprintf( ansi, sizeof( ansi ), "!!! '%s' for '%s'", tok, e->GetName() );
			}
			else
			{
				g_pVGuiLocalize->ConvertUnicodeToANSI( str, ansi, sizeof( ansi ) );

				// Convert to regular text
				logprint( "script.txt", "\t\t\t\tcc_token(%s)\n\n\t\t\"%s\"\n\n", tok, ansi );
			}
		}

		// Now spit out the CSV version...
		RemoveCommas( actorname );
		RemoveCommas( tok );
		RemoveCommas( ansi );

		char mapname[ 512 ];

		mapname[ 0 ] = 0;

		int idx = g_FirstMapForVCD.Find( vcdname );
		if ( idx != g_FirstMapForVCD.InvalidIndex() )
		{
			Q_strncpy( mapname, g_Analysis.symbols.String( g_FirstMapForVCD[ idx ] ), sizeof( mapname ) );
		}

		static unsigned int sortindex = 0;

		logprint( "script.csv", "%u,%s,%s,%s,%6.3f,%s,%s,\"%s\",\"%s\"\n",
			sortindex++, mapname, vcdname, actorname, e->GetStartTime(), tok, wavname, ansi, sentence_text );
	}
}

void CheckLocalizationEntries( CUtlVector< CUtlSymbol >& vcdfiles, CUtlRBTree< CUtlSymbol, int >& referencedcaptionwaves )
{
	int disabledcount = 0;
	int validcount = 0;
	int missingcount = 0;
	int wavfile = 0;

	int gamedirskip = Q_strlen( gamedir );

	if ( build_script )
	{
		g_pFullFileSystem->RemoveFile( "script.txt", "GAME" );
		g_pFullFileSystem->RemoveFile( "script.csv", "GAME" );
	}

	int c = vcdfiles.Count();
	for ( int i = 0; i < c; ++i )
	{
		CUtlSymbol& vcdname = vcdfiles[ i ];

		CUtlRBTree< CChoreoEvent *, int >	sortedSpeakEvents( 0, 0, EventStartTimeLessFunc );


		// Load the .vcd
		char fullname[ 512 ];
		Q_snprintf( fullname, sizeof( fullname ), "%s", g_Analysis.symbols.String( vcdname ) );

		LoadScriptFile( fullname );
	
		CChoreoScene *scene = ChoreoLoadScene( fullname, NULL, &g_TokenProcessor, Con_Printf );
		if ( !scene )
		{
			vprint( 0, "Warning:  Unable to load %s\n", fullname );
			continue;
		}

		// Now iterate the events looking for speak events
		int numevents = scene->GetNumEvents();
		for ( int j = 0; j < numevents; j++ )
		{
			CChoreoEvent *e = scene->GetEvent( j );
			if ( e->GetType() != CChoreoEvent::SPEAK )
				continue;

			if ( e->GetCloseCaptionType() == CChoreoEvent::CC_DISABLED )
			{
				++disabledcount;
				continue;
			}

			if ( build_script )
			{
				if ( sortedSpeakEvents.Find( e ) == sortedSpeakEvents.InvalidIndex() )
				{
					sortedSpeakEvents.Insert( e );
				}
			}

			char tok[ 256 ];

			for ( int pass = 0; pass <= 1; ++pass )
			{
				bool iscombined = false;

				if ( pass == 0 )
				{
					Q_strncpy( tok, e->GetParameters(), sizeof( tok ) );
				}
				else
				{
					if ( e->GetCloseCaptionType() != CChoreoEvent::CC_MASTER )
						continue;

					if ( e->GetNumSlaves() <= 0 )
						continue;

					if ( !e->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
					{
						++missingcount;
						continue;;
					}

					iscombined = true;
				}

				// Look it up
				wchar_t *str = g_pVGuiLocalize->Find( tok );
				if ( !str )
				{
					char fn[ 256 ];
					//Q_FileBase( g_Analysis.symbols.String( vcdname ), fn, sizeof( fn ) );
					Q_strncpy( fn, &g_Analysis.symbols.String( vcdname )[ gamedirskip ], sizeof( fn ) );

					
					if ( Q_stristr( tok, ".wav" ) )
					{
						if ( verbose )
						{	
							if ( !regenerate_quiet )
							{
								vprint( 0, "(OBSOLETE???)missing cc token '%s' (!.wav file):  vcd (%s), event (%s)\n",
									tok, fn, e->GetName() );
							}
						}

						++wavfile;
					}
					else
					{
						if ( !regenerate_quiet )
						{
							vprint( 0, "missing %s cc token '%s':  vcd (%s), event (%s)\n",
								pass == 0 ? "normal" : "combined",
								tok, 
								fn, 
								e->GetName() );
						}

						// Add the "!!!entry" to a temp file
						if ( verbose )
						{
							char suggested[ 4096 ];
							Q_snprintf( suggested, sizeof( suggested ), "!!!%s", tok );

							int soundindex = g_pSoundEmitterSystem->GetSoundIndex( tok );
							if ( soundindex != -1 )
							{
								// Now try to find a better bit of text
								CSoundParametersInternal *internal = g_pSoundEmitterSystem->InternalGetParametersForSound( soundindex );
								if ( internal && internal->NumSoundNames() > 0 )
								{
									CUtlSymbol &symwave = internal->GetSoundNames()[ 0 ].symbol;
									char const *wavname = g_pSoundEmitterSystem->GetWaveName( symwave );
									if ( wavname && ( Q_stristr( wavname, "vo/" ) || Q_stristr( wavname, "vo\\" ) ) )
									{
										CSentence sentence;
										if ( LoadSentenceFromWavFile( va( "sound/%s", PSkipSoundChars( wavname ) ), sentence ) )
										{
											if ( Q_strlen( sentence.GetText() ) > 0 ) 
											{
												Q_snprintf( suggested, sizeof( suggested ), "%s", sentence.GetText() );
												cleanquotes( suggested );
											}
										}
									}
								}
							}

							logprint( "missing.txt", "\t\"%s\"\t\t\"%s\"\n", tok, suggested );
						}

						++missingcount;
					}
					
					
				}
				else
				{
					if ( verbose )
					{
						if ( !wcsncmp( str, L"!!!", wcslen( L"!!!" ) ) )
						{
							if ( !regenerate_quiet )
							{
								vprint( 0, "Autogenerated closecaption token '%s' not edited\n", tok );
							}
						}
					}
					++validcount;
				}

				// Verify checksum
				if ( iscombined )
				{
					ValidateCombinedSoundCheckSum( e, referencedcaptionwaves );
				}
			}
		}

		SpewScript( fullname, sortedSpeakEvents );
		sortedSpeakEvents.RemoveAll();

		delete scene;
	}

	int total = validcount + missingcount + wavfile + disabledcount;
	if ( total != 0 )
	{
		vprint( 0, "\n%.2f %%%% invalid (%i valid, %i missing, %i wavfile(OBSOLETE), %i disabled - total %i)\n",
			100.0f * (float)missingcount / (float)total,
			validcount,
			missingcount,
			wavfile,
			disabledcount,
			total );
	}
}

void CheckForOrphanedCombinedWavs( CUtlVector< CUtlSymbol >& diskwaves, CUtlRBTree< CUtlSymbol, int >& captionsused )
{
	if ( g_pFullFileSystem->FileExists( "orphaned.bat", "GAME" ) )
	{
		g_pFullFileSystem->RemoveFile( "orphaned.bat", "GAME" );
	}

	int orphans = 0;
	int c = diskwaves.Count();
	for ( int i = 0; i < c; ++i )
	{
		CUtlSymbol &sym = diskwaves[ i ];

		if ( captionsused.Find( sym ) != captionsused.InvalidIndex() )
			continue;

		char fn[ 256 ];
		Q_strncpy( fn, g_Analysis.symbols.String( sym ), sizeof( fn ) );

		vprint( 1, "Orphaned wav file '%s'\n", fn );

		logprint( "orphaned.bat", "del \"%s\" /f\n", fn );

		++orphans;
	}

	if ( orphans != 0 )
	{
		vprint( 0, "\n%.2f %%%% (%i/%i), orphaned combined .wav files in sound/combined/... folder\n",
			100.0f * (float)orphans / (float)c,
			orphans, c );

		vprint( 0, "created orphaned.bat file\n" );
	}
	else
	{
		vprint( 0, "\nNo orphaned files found among %d possible disk waves\n", c );
	}
}

float GetWaveDuration( char const *wavname )
{
	if ( !g_pFullFileSystem->FileExists( wavname ) )
	{
		return 0.0f;
	}

	CAudioSource *wave = sound->LoadSound( wavname );
	if ( !wave )
	{
		//vprint( 0, "unable to load %s\n", wavname );
		return 0.0f;
	}
	
	CAudioMixer *pMixer = wave->CreateMixer();
	if ( !pMixer )
	{
		vprint( 0, "unable to create mixer for %s\n", wavname );
		delete wave;
		return 0.0f;
	}
	
	float duration = wave->GetRunningLength();
	return duration;
}

void GetWaveSentence( char const *wavname, CSentence& sentence )
{
	sentence.Reset();
	if ( !g_pFullFileSystem->FileExists( wavname ) )
	{
		return;
	}

	CAudioSource *wave = sound->LoadSound( wavname );
	if ( !wave )
	{
		//vprint( 0, "unable to load %s\n", wavname );
		return;
	}

	sentence = *wave->GetSentence();
}

void ValidateForeignLanguageWaves( char const *language, CUtlVector< CUtlSymbol >& combinedwavfiles )
{
	// Need to compute the gamedir to the specified language
	char langdir[ 512 ];
	char strippedgamedir[ 512 ];
	Q_strncpy( langdir, gamedir, sizeof( langdir ) );
	
	Q_StripTrailingSlash( langdir );

	Q_strncpy( strippedgamedir, langdir, sizeof( strippedgamedir ) );

	Q_strcat( langdir, "_", sizeof(langdir) );
	Q_strcat( langdir, language, sizeof(langdir) );

	int skipchars = Q_strlen( strippedgamedir );

	// Need to add this to the file system
	int missing = 0;
	int outdated = 0;

	int c = combinedwavfiles.Count();
	for ( int i = 0; i < c; ++i )
	{
		CUtlSymbol& sym = combinedwavfiles[ i ];
		
		char wavname[ 512 ];
		Q_strncpy( wavname, g_Analysis.symbols.String( sym ), sizeof( wavname ) );

		// Now get language specific wav name
		char localizedwavename[ 512 ];
		Q_snprintf( localizedwavename, sizeof( localizedwavename ), "%s%s",
			langdir,
			&wavname[ skipchars ] );

		float duration_english = GetWaveDuration( wavname );
		if ( !duration_english )
		{
			continue;
		}
		// Now see if the localized file exists

		float duration_localized = GetWaveDuration( localizedwavename );
		if ( !duration_localized )
		{
			++missing;
			vprint( 0, "Missing localized file %s\n", localizedwavename );
			continue;
		}

		CSentence sentence_english;
		GetWaveSentence( wavname, sentence_english );
		CSentence sentence_localized;
		GetWaveSentence( localizedwavename, sentence_localized );

		if ( sentence_english.GetText() &&
			 sentence_english.GetText()[0] )
		{
			if ( !sentence_localized.GetText() || !sentence_localized.GetText()[0] )
			{
				vprint( 0, "--> Localized combined file for '%s' doesn't have sentence data '%s'\n",
					language, localizedwavename );
			}
			else if ( !Q_stricmp( sentence_english.GetText(), sentence_localized.GetText()) )
			{
				vprint( 0, "--> Localized combined file for '%s' still using english phoneme and text data '%s'\n",
					language, localizedwavename );
			}
		}

		if ( fabs( duration_localized - duration_english ) > SOUND_DURATION_TOLERANCE )
		{
			++outdated;
			vprint( 0, "--> Mismatched localized file %s (english %.2f s./%s %.2f s.)\n", localizedwavename,
				duration_english, language, duration_localized );
			continue;
		}
	}

	if ( c != 0 )
	{
		vprint( 0, "%.2f %%%% missing(%i)+outdated(%i)/total(%i), combined .wav files in %s closecaption/ folder\n",
			100.0f * (float)(missing + outdated ) / (float)c,
			missing, outdated, c, language );
	}
}

void BuildReverseSoundLookup( CUtlDict< CUtlSymbol, int >& wavtosound )
{
	// Build a dictionary of wav names to sound names
	int c = g_pSoundEmitterSystem->GetSoundCount();
	for ( int i = 0; i < c; ++i )
	{
		char const *soundname = g_pSoundEmitterSystem->GetSoundName( i );

		CUtlSymbol soundSymbol = g_Analysis.symbols.AddString( soundname );

		CSoundParametersInternal* params = g_pSoundEmitterSystem->InternalGetParametersForSound( i );
		if ( soundname && params )
		{
			int soundcount = params->NumSoundNames();
			for ( int j = 0; j < soundcount; ++j )
			{
				SoundFile& sf = params->GetSoundNames()[ j ];

				char const *pwavname = g_pSoundEmitterSystem->GetWaveName( sf.symbol );
				char fixed[ 512 ];
				Q_strncpy( fixed, PSkipSoundChars( pwavname ), sizeof( fixed ) );
				_strlwr( fixed );
				Q_FixSlashes( fixed );

				int curidx = wavtosound.Find( fixed );

				if ( curidx == wavtosound.InvalidIndex() )
				{
					wavtosound.Insert( fixed, soundSymbol );

					//vprint( 0, "entry %s == %s\n", fixed, soundname );
				}
			}
		}
	}

	vprint( 0, "Reverse lookup has %i entries from %i available sounds\n", wavtosound.Count(), c );
}

#define UNK_SOUND_ENTRY "<nosoundentry>"
char const *FindSoundEntry( CUtlDict< CUtlSymbol, int >& wavtosound, char const *wavname )
{
	char fixed[ 512 ];
	Q_strncpy( fixed, PSkipSoundChars( wavname ), sizeof( fixed ) );
	_strlwr( fixed );
	Q_FixSlashes( fixed );


	int idx = wavtosound.Find( fixed );
	if ( idx != wavtosound.InvalidIndex() )
	{
		CUtlSymbol snd = wavtosound[ idx ];
		return g_Analysis.symbols.String( snd );
	}
	return UNK_SOUND_ENTRY;
}

void CheckWaveFile( CUtlDict< CUtlSymbol, int >& wavtosound, char const *wavname )
{
//	vprint( 0, "%s\n", wavname );

	char const *soundname = FindSoundEntry( wavtosound, wavname );

	char ansi[ 512 ];

	ansi[ 0 ] = 0;

	CSentence sentence;
	if ( LoadSentenceFromWavFile( va( "sound/%s", PSkipSoundChars( wavname ) ), sentence ) )
	{
		if ( Q_strlen( sentence.GetText() ) > 0 ) 
		{
			Q_snprintf( ansi, sizeof( ansi ), "%s", sentence.GetText() );
			cleanquotes( ansi );
		}
	}

	// Now look up cc token
	wchar_t *text = g_pVGuiLocalize->Find( soundname );

	char caption[ 1024 ];
	Q_strncpy( caption, "!!!", sizeof( caption ) );
	if ( text )
	{
		g_pVGuiLocalize->ConvertUnicodeToANSI( text, caption, sizeof( caption ) );
	}
	else
	{
		if ( !Q_stricmp( soundname, UNK_SOUND_ENTRY ) )
		{
			Q_snprintf( caption, sizeof( caption ), "!!!%s", soundname );
		}
	}

	logprint( "wavcheck.csv", 
		"\"%s\",\"%s\",\"%s\",\"%s\"\n",
		wavname,
		soundname,
		caption,
		ansi );
}

void WavCheck( CUtlVector< CUtlSymbol >& wavfiles )
{
	g_pFullFileSystem->RemoveFile( "wavcheck.csv", "GAME" );

	vprint( 0, "Building reverse lookup\n" );


	logprint( "wavcheck.csv", 
		"\"%s\",\"%s\",\"%s\",\"%s\"\n",
		"WaveName",
		"Sound Script Name",
		"Close Caption",
		"Phoneme Data String" );

	CUtlDict< CUtlSymbol, int >	wavtosound;
	BuildReverseSoundLookup( wavtosound );

	vprint( 0, "Performing wavcheck\n" );
	int c = wavfiles.Count();
	int offset = Q_strlen( gamedir ) + Q_strlen( "sound/" );

	for ( int i = 0; i < c; ++i )
	{
		char const *wavname = g_Analysis.symbols.String( wavfiles[ i ] );
		CheckWaveFile( wavtosound, wavname + offset );

		if ( !(i % 100 ) )
		{
			vprint( 0, "Finished %i/%i\n", i, c );
		}
	}
}

void BuildWavFileToFullPathLookup( CUtlVector< CUtlSymbol >& wavfile, CUtlDict< int, int >& wavtofullpath )
{
	int c = wavfile.Count();
	for ( int i = 0; i < c; ++i )
	{
		CUtlSymbol &sym = wavfile[ i ];

		char shortname[ 512 ];
		Q_FileBase( g_Analysis.symbols.String( sym ), shortname, sizeof( shortname ) );

		Q_SetExtension( shortname, ".wav", sizeof( shortname ) );
		Q_FixSlashes( shortname );
		Q_strlower( shortname );

		int idx = wavtofullpath.Find( shortname );
		if ( idx == wavtofullpath.InvalidIndex() )
		{
			wavtofullpath.Insert( shortname, i );
		}
	}
}

static void COM_CreatePath (const char *path)
{
	char temppath[512];
	Q_strncpy( temppath, path, sizeof(temppath) );
	
	for (char *ofs = temppath+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{       // create the directory
			char old = *ofs;
			*ofs = 0;
			mkdir (temppath);
			*ofs = old;
		}
	}
}

void MakeBatchFile( CUtlVector< CUtlSymbol >& wavfiles, char const *pchFromdir, char const *pchTodir )
{
	g_pFullFileSystem->RemoveFile( "copywaves.bat", "GAME" );

	vprint( 0, "Building reverse lookup\n" );

	CUtlDict< int, int >	wavtofullpath;
	BuildWavFileToFullPathLookup( wavfiles, wavtofullpath );

	CUtlVector< CUtlSymbol > files;
	BuildFileList( files, pchFromdir, ".wav" );

	int gamedirskip = Q_strlen( gamedir ) + Q_strlen( "sound//" );

	int c = files.Count();
	for ( int i = 0; i < c; ++i )
	{
		char const *sname = g_Analysis.symbols.String( files[ i ] );
		if ( !sname )
			continue;

		char shortname[ 512 ];
		Q_strncpy( shortname, sname, sizeof( shortname ) );

		char fn[ 512 ];
		Q_FileBase( shortname, fn, sizeof( fn ) );
		Q_SetExtension( fn, ".wav", sizeof( fn ) );
		Q_strlower( fn );
		Q_FixSlashes( fn );

		int slot = wavtofullpath.Find( fn );
		if ( slot == wavtofullpath.InvalidIndex() )
		{
			vprint( 0, "Couldn't find slot for '%s'\n", sname );
			continue;
		}

		char fullname[ 512 ];
		Q_snprintf( fullname, sizeof( fullname ), "%s/%s", pchTodir, &g_Analysis.symbols.String( wavfiles[ wavtofullpath[ slot ] ] )[ gamedirskip ] );
		Q_strlower( fullname );
		Q_FixSlashes( fullname );

		//logprint( "copywaves.bat", "xcopy \"%s\" \"%s\"\n", 
			//shortname,
			//fullname );

		
		COM_CreatePath( fullname );

		CopyFile( shortname, fullname, TRUE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : store - 
//-----------------------------------------------------------------------------
void StoreValveDataChunk( CSentence& sentence, IterateOutputRIFF& store )
{
	// Buffer and dump data
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	sentence.SaveToBuffer( buf );

	// Copy into store
	store.ChunkWriteData( buf.Base(), buf.TellPut() );
}

void SaveWave( char const *filename, CSentence& s )
{
	char infile[ 512 ];
	Q_strncpy( infile, filename, sizeof( infile ) );

	Q_SetExtension( infile, ".tmp", sizeof( infile ) );

	// Rename infile
	MoveFile( filename, infile );
	SetFileAttributes( filename, FILE_ATTRIBUTE_NORMAL );

	{
		InFileRIFF riff( infile, io_in );
		Assert( riff.RIFFName() == RIFF_WAVE );

		// set up the iterator for the whole file (root RIFF is a chunk)
		IterateRIFF walk( riff, riff.RIFFSize() );

		OutFileRIFF riffout( filename, io_out );

		IterateOutputRIFF store( riffout );

		bool wordtrackwritten = false;

		// Walk input chunks and copy to output
		while ( walk.ChunkAvailable() )
		{
			unsigned int originalPos = store.ChunkGetPosition();

			store.ChunkStart( walk.ChunkName() );

			bool skipchunk = false;

			switch ( walk.ChunkName() )
			{
			case WAVE_VALVEDATA:
				// Overwrite data
				StoreValveDataChunk( s, store );
				wordtrackwritten = true;
				break;
			default:
				store.CopyChunkData( walk );
				break;
			}

			store.ChunkFinish();
			if ( skipchunk )
			{
				store.ChunkSetPosition( originalPos );
			}

			walk.ChunkNext();
		}

		if ( !wordtrackwritten )
		{
			store.ChunkStart( WAVE_VALVEDATA );
			StoreValveDataChunk( s, store );
			store.ChunkFinish();
		}
	}

	SetFileAttributes( infile, FILE_ATTRIBUTE_NORMAL );
	DeleteFile( infile );
}

void ExtractPhonemesForWave( IPhonemeExtractor *extractor, char const *wavname )
{
	char formatbuffer[ 1024 ];
	int formatsize = sizeof( formatbuffer );
	int dataSize = 0;

	CSentence sentence;
	if ( !LoadSentenceFromWavFile( wavname, sentence, formatbuffer, &formatsize, &dataSize ) )
	{
		vprint( 0, "  skip '%s' missing\n", wavname );
		return;
	}

	if ( !forceextract && 
		sentence.m_Words.Count() > 0 )
	{
		vprint( 0, "  skip '%s', already has phonemes\n", wavname );
		return;
	}

	if ( forceextract )
	{
		sentence.Reset();
	}

	if ( formatsize == 0 )
	{
		vprint( 0, "  skip '%s', not WAVE_FMT parsed\n", wavname );
		return;
	}

	const WAVEFORMATEX *pHeader = (const WAVEFORMATEX *)formatbuffer;

	int format = pHeader->wFormatTag;

	int bits = pHeader->wBitsPerSample;
	int rate = pHeader->nSamplesPerSec;
	int channels = pHeader->nChannels;

	int sampleSize = (bits * channels) / 8;
	
	// this can never be zero -- other functions divide by this. 
	// This should never happen, but avoid crashing
	if ( sampleSize <= 0 )
		sampleSize = 1;

	int sampleCount = 0;
	float truesamplesize = sampleSize;

	if ( format == WAVE_FORMAT_ADPCM )
	{
		sampleSize = 1;

		ADPCMWAVEFORMAT *pFormat = (ADPCMWAVEFORMAT *)formatbuffer;
		int blockSize = ((pFormat->wSamplesPerBlock - 2) * pFormat->wfx.nChannels ) / 2;
		blockSize += 7 * pFormat->wfx.nChannels;

		int blockCount = sampleCount / blockSize;
		int blockRem = sampleCount % blockSize;
		
		// total samples in complete blocks
		sampleCount = blockCount * pFormat->wSamplesPerBlock;

		// add remaining in a short block
		if ( blockRem )
		{
			sampleCount += pFormat->wSamplesPerBlock - (((blockSize - blockRem) * 2) / channels);
		}

		truesamplesize = 0.5f;
	}
	else
	{
		sampleCount = dataSize / sampleSize;
	}

	// Do extraction
	// Current set of tags
	CSentence			outsentence;

	char filename[ 512 ];
	Q_snprintf( filename, sizeof( filename ), "%s", wavname );

	int result = extractor->Extract( 
		filename,
		dataSize, // (int)( m_pWaveFile->GetRunningLength() * m_pWaveFile->SampleRate() * m_pWaveFile->TrueSampleSize() ),
		Msg,
		sentence,
		outsentence );

	if ( result != SR_RESULT_SUCCESS )
	{
		vprint( 0, " failed to analyze '%s', skipping\n", wavname );
		return;
	}


	float bytespersecond = rate * truesamplesize;

	// Now convert byte offsets to times
	int i;
	for ( i = 0; i < outsentence.m_Words.Size(); i++ )
	{
		CWordTag *tag = outsentence.m_Words[ i ];
		Assert( tag );
		if ( !tag )
			continue;

		tag->m_flStartTime = ( float )(tag->m_uiStartByte ) / bytespersecond;
		tag->m_flEndTime = ( float )(tag->m_uiEndByte ) / bytespersecond;

		for ( int j = 0; j < tag->m_Phonemes.Size(); j++ )
		{
			CPhonemeTag *ptag = tag->m_Phonemes[ j ];
			Assert( ptag );
			if ( !ptag )
				continue;

			ptag->SetStartTime( ( float )(ptag->m_uiStartByte ) / bytespersecond );
			ptag->SetEndTime( ( float )(ptag->m_uiEndByte ) / bytespersecond );
		}
	}

	sentence = outsentence;

	outsentence.Reset();

	// Resave it
	SaveWave( filename, sentence );
}

struct Extractor
{
	PE_APITYPE			apitype;
	CSysModule			*module;
	IPhonemeExtractor	*extractor;
};

CUtlVector< Extractor >	g_Extractors;

void UnloadPhonemeConverters()
{
	int c = g_Extractors.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		Extractor *e = &g_Extractors[ i ];
		g_pFullFileSystem->UnloadModule( e->module );
	}

	g_Extractors.RemoveAll();
}

int LoadPhonemeExtractors()
{
	// Enumerate modules under bin folder of exe
	FileFindHandle_t findHandle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx( "phonemeextractors/*.dll", "EXECUTABLE_PATH", &findHandle );
	int useextractor = -1;
	while ( pFilename )
	{	
		char fullpath[ 512 ];
		Q_snprintf( fullpath, sizeof( fullpath ), "phonemeextractors/%s", pFilename  );

		pFilename = g_pFullFileSystem->FindNext( findHandle );

		Con_Printf( "Loading extractor from %s\n", fullpath );

		Extractor e;
		e.module = Sys_LoadModule( fullpath );
		if ( !e.module )
		{
			Warning( "Unable to Sys_LoadModule %s\n", fullpath );
			continue;
		}

		CreateInterfaceFn factory = Sys_GetFactory( e.module );
		if ( !factory )
		{
			Warning( "Unable to get factory from %s\n", fullpath );
			continue;
		}

		e.extractor = ( IPhonemeExtractor * )factory( VPHONEME_EXTRACTOR_INTERFACE, NULL );
		if ( !e.extractor )
		{
			Warning( "Unable to get IPhonemeExtractor interface version %s from %s\n", VPHONEME_EXTRACTOR_INTERFACE, fullpath );
			continue;
		}

		e.apitype = e.extractor->GetAPIType();
		if ( e.apitype == SPEECH_API_LIPSINC )
		{
			useextractor = g_Extractors.Count();
		}

		g_Extractors.AddToTail( e );	
	}

	g_pFullFileSystem->FindClose( findHandle );

	return useextractor;
}

void ExtractPhonemes( CUtlVector< CUtlSymbol >& wavfiles )
{
	int index = LoadPhonemeExtractors();
	if ( index == -1 )
		return;

	if ( index == 0 )
	{
		vprint( 0, "Couldn't find suitable extractor\n" );
		return;
	}

	IPhonemeExtractor	*extractor = g_Extractors[ index ].extractor;
	Assert( extractor );

	vprint( 0, "Using %s\n", extractor->GetName() );

	int c = wavfiles.Count();

	vprint( 0, "Performing '%i' extractions (might take a while...)\n", c );
	for ( int i = 0; i < c; ++i )
	{
		char const *wavname = g_Analysis.symbols.String( wavfiles[ i ] );
		ExtractPhonemesForWave( extractor, wavname );

		if ( !(i % 50 ) )
		{
			vprint( 0, "Finished %i/%i\n", i, c );
		}
	}

	UnloadPhonemeConverters();
}

void CheckWavForLoops( char const *wavname )
{
	InFileRIFF riff( wavname, io_in );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	while ( walk.ChunkAvailable( ) )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_CUE:
			vprint( 0, "'%s' has a CUE chunk\n", wavname );
			return;
		default:
			break;
		}
		walk.ChunkNext();
	}
}

void CheckForLoops( CUtlVector< CUtlSymbol >& wavfiles )
{
	int c = wavfiles.Count();

	vprint( 0, "Performing '%i' extractions (might take a while...)\n", c );
	for ( int i = 0; i < c; ++i )
	{
		char const *wavname = g_Analysis.symbols.String( wavfiles[ i ] );
		CheckWavForLoops( wavname );
		if ( !(i % 50 ) )
		{
			vprint( 0, "Finished %i/%i\n", i, c );
		}
	}
}

#define MAX_LOCALIZED_CHARS 2048
//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize)
{
	int result = ::WideCharToMultiByte(CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL);
	ansi[ansiBufferSize - 1] = 0;
	return result;
}

struct OrderedCaption_t
{
	OrderedCaption_t() :
		sym( UTL_INVAL_SYMBOL ),
		commands( NULL ),
		english( NULL ),
		blankenglish( false )
	{
	}

	OrderedCaption_t( const OrderedCaption_t& src )
	{
		sym = src.sym;

		if ( src.commands )
		{
			int len = wcslen( src.commands ) + 1;
			commands = new wchar_t[ len ];
			wcscpy( commands, src.commands );
		}
		else
		{
			commands = NULL;
		}

		if ( src.english )
		{
			int len = wcslen( src.english ) + 1;
			english = new wchar_t[ len ];
			wcscpy( english, src.english );
		}
		else
		{
			english = NULL;
		}
		blankenglish = src.blankenglish;
	}

	~OrderedCaption_t()
	{
		delete[] commands;
		delete[] english;
	}

	CUtlSymbol	sym;
	wchar_t		*commands;  // any <cmd:arg> stuff at the beginning of the US captions
	wchar_t		*english;
	bool		blankenglish;
};

bool SplitCommand( wchar_t const **ppIn, wchar_t *cmd, wchar_t *args )
{
	const wchar_t *in = *ppIn;
	const wchar_t *oldin = in;

	if ( in[0] != L'<' )
	{
		*ppIn += ( oldin - in );
		return false;
	}

	args[ 0 ] = 0;
	cmd[ 0 ]= 0;
	wchar_t *out = cmd;
	in++;
	while ( *in != L'\0' && *in != L':' && *in != L'>' && !isspace( *in ) )
	{
		*out++ = *in++;
	}
	*out = L'\0';

	if ( *in != L':' )
	{
		*ppIn += ( in - oldin );
		return true;
	}

	in++;
	out = args;
	while ( *in != L'\0' && *in != L'>' )
	{
		*out++ = *in++;
	}
	*out = L'\0';

	//if ( *in == L'>' )
	//	in++;

	*ppIn += ( in - oldin );
	return true;
}

wchar_t *GetStartupCommands( const wchar_t *str )
{
	const wchar_t *curpos = str;
	
	for ( ; curpos && *curpos != L'\0'; ++curpos )
	{
		wchar_t cmd[ 256 ];
		wchar_t args[ 256 ];

		if ( SplitCommand( &curpos, cmd, args ) )
		{
			continue;
		}

		// Got to first non-command character
		break;
	}

	if ( curpos - str >= 1 )
	{
		int len = curpos - str;
		wchar_t *cmds = new wchar_t[ len + 1 ];
		wcsncpy( cmds, str, len );
		cmds[ len ] = L'\0';
		return cmds;
	}
	
	return NULL;
}

wchar_t *CopyUnicode( const wchar_t *in )
{
	int len = wcslen( in ) + 1;
	wchar_t *out = new wchar_t[ len ];
	wcsncpy( out, in, len );
	out[ len - 1 ] = L'\0';
	return out;
}

void BuildOrderedCaptionList( CUtlVector< OrderedCaption_t >& list )
{
	// parse out the file
	FileHandle_t file = g_pFullFileSystem->Open( "resource/closecaption_english.txt", "rb");
	if ( file == FILESYSTEM_INVALID_HANDLE )
	{
		// assert(!("CLocalizedStringTable::AddFile() failed to load file"));
		return;
	}

	// read into a memory block
	int fileSize = g_pFullFileSystem->Size(file) ;
	wchar_t *memBlock = (wchar_t *)malloc(fileSize + sizeof(wchar_t));
	wchar_t *data = memBlock;
	g_pFullFileSystem->Read(memBlock, fileSize, file);

	// null-terminate the stream
	memBlock[fileSize / sizeof(wchar_t)] = 0x0000;

	// check the first character, make sure this a little-endian unicode file
	if (data[0] != 0xFEFF)
	{
		g_pFullFileSystem->Close(file);
		free(memBlock);
		return;
	}
	data++;

	// parse out a token at a time
	enum states_e
	{
		STATE_BASE,		// looking for base settings
		STATE_TOKENS,	// reading in unicode tokens
	};

	bool bQuoted;
	bool bEnglishFile = true;

	states_e state = STATE_BASE;
	while (1)
	{
		// read the key and the value
		wchar_t keytoken[128];
		data = ReadUnicodeToken(data, keytoken, 128, bQuoted);
		if (!keytoken[0])
			break;	// we've hit the null terminator

		// convert the token to a string
		char key[128];
		ConvertUnicodeToANSI(keytoken, key, sizeof(key));

		// if we have a C++ style comment, read to end of line and continue
		if (!strnicmp(key, "//", 2))
		{
			data = ReadToEndOfLine(data);
			continue;
		}

		wchar_t valuetoken[ MAX_LOCALIZED_CHARS ];
		data = ReadUnicodeToken(data, valuetoken, MAX_LOCALIZED_CHARS, bQuoted);
		if (!valuetoken[0] && !bQuoted)
			break;	// we've hit the null terminator
		
		if (state == STATE_BASE)
		{
			if (!stricmp(key, "Language"))
			{
				// copy out our language setting
				/*
				char value[MAX_LOCALIZED_CHARS];
				ConvertUnicodeToANSI(valuetoken, value, sizeof(value));
				strncpy(m_szLanguage, value, sizeof(m_szLanguage) - 1);
				*/
			}
			else if (!stricmp(key, "Tokens"))
			{
				state = STATE_TOKENS;
			}
			else if (!stricmp(key, "}"))
			{
				// we've hit the end
				break;
			}
		}
		else if (state == STATE_TOKENS)
		{
			if (!stricmp(key, "}"))
			{
				// end of tokens
				state = STATE_BASE;
			}
			else
			{
				// skip our [english] beginnings (in non-english files)
				if ( (bEnglishFile) || (!bEnglishFile && strnicmp(key, "[english]", 9)))
				{
					// add the string to the table
					//AddString(key, valuetoken, NULL);
					CUtlSymbol sym = g_Analysis.symbols.AddString( key );

					OrderedCaption_t cap;
					cap.sym = sym;
					cap.commands = GetStartupCommands( valuetoken );
					cap.english = CopyUnicode( valuetoken );
					cap.blankenglish = IsAllSpaces( valuetoken );

					list.AddToTail( cap );
				}
			}
		}
	}

	g_pFullFileSystem->Close(file);
	free(memBlock);

	vprint( 0, "Loaded %i captionnames from closecaption_english.txt\n", list.Count() );
}

struct LookupData_t
{
	LookupData_t() :
		unicode( 0 ),
		caption( 0 )
		{
		}

	wchar_t		*unicode;
	char		*caption;
};

void LoadImportData( char const *filename, CUtlDict< LookupData_t, int >& lookup )
{
// parse out the file
	FileHandle_t file = g_pFullFileSystem->Open( filename, "rb");
	if ( file == FILESYSTEM_INVALID_HANDLE )
	{
		// assert(!("CLocalizedStringTable::AddFile() failed to load file"));
		return;
	}

	// read into a memory block
	int fileSize = g_pFullFileSystem->Size(file) ;
	wchar_t *memBlock = (wchar_t *)malloc(fileSize + sizeof(wchar_t));
	wchar_t *data = memBlock;
	g_pFullFileSystem->Read(memBlock, fileSize, file);

	// null-terminate the stream
	memBlock[fileSize / sizeof(wchar_t)] = 0x0000;

	// check the first character, make sure this a little-endian unicode file
	if (data[0] != 0xFEFF)
	{
		g_pFullFileSystem->Close(file);
		free(memBlock);
		return;
	}
	data++;

	bool bQuoted;

	while (1)
	{
		// read the key and the value
		wchar_t keytoken[128];
		data = ReadUnicodeTokenNoSpecial(data, keytoken, 128, bQuoted);
		if (!keytoken[0])
			break;	// we've hit the null terminator

		// convert the token to a string
		char key[128];
		ConvertUnicodeToANSI(keytoken, key, sizeof(key));

		// vprint( 0, "keyname %s\n", key );

		// if we have a C++ style comment, read to end of line and continue
		if (!strnicmp(key, "//", 2))
		{
			data = ReadToEndOfLine(data);
			continue;
		}

		wchar_t valuetoken[ MAX_LOCALIZED_CHARS ];
		data = ReadUnicodeToken(data, valuetoken, MAX_LOCALIZED_CHARS, bQuoted);
		if (!valuetoken[0] && !bQuoted)
			break;	// we've hit the null terminator
		
		wchar_t *vcopy = new wchar_t[ wcslen( valuetoken ) + 1 ];
		wcscpy( vcopy, valuetoken );

		LookupData_t ld;
		ld.unicode = vcopy;
		ld.caption = NULL;

		lookup.Insert( key, ld );
	}

	g_pFullFileSystem->Close(file);
	free(memBlock);

	vprint( 0, "Loaded %i wav/captions from %s\n", lookup.Count(), filename );
}

#define CAPTION_OUT_FILE "resource/closecaption_test.txt"

//-----------------------------------------------------------------------------
// Purpose: importfile is a unicode file contains pairs of .wav names and caption strings
//  we need to read in the closecaption_english.txt file
//   and then build a reverse lookup of .wav to caption name and build a new
//   closecaption_test.txt file based on the unicode caption strings
// Input  : *importfile - 
//-----------------------------------------------------------------------------
void ImportCaptions( char const *pchImportfile )
{
	CUtlVector< OrderedCaption_t > captionlist;
	BuildOrderedCaptionList( captionlist );

	CUtlDict< LookupData_t, int >	newCaptions;
	LoadImportData( pchImportfile, newCaptions );

	// Now build a .wav to caption name lookup
	CUtlDict< CUtlSymbol, int >	wavtosound;
	BuildReverseSoundLookup( wavtosound );

	CUtlDict< wchar_t *, int >	captionToUnicode;

	// Now walk the import data, and try to figure out the caption name for each one
	int c = newCaptions.Count();
	for ( int i = 0; i < c ; ++i )
	{
		char const *wavname = newCaptions.GetElementName( i );
		LookupData_t& data = newCaptions[ i ];

		char fn[ 512 ];
		Q_strncpy( fn, wavname, sizeof( fn ) );
		Q_strlower( fn );
		Q_FixSlashes( fn );

		// See if we can find the wavname in the reverse lookup
		int idx = wavtosound.Find( fn );
		if ( idx != wavtosound.InvalidIndex() )
		{
			data.caption = strdup( g_Analysis.symbols.String( wavtosound[ idx ] ) );

			captionToUnicode.Insert( data.caption, data.unicode );
		}
		else
		{
			vprint( 0, "unable to find caption matching '%s'\n", wavname );
		}
	}

	CUtlBuffer buf( 0, 0 );

	c = captionlist.Count();
	for ( int i = 0; i < c; ++i )
	{
		char const *captionname = g_Analysis.symbols.String( captionlist[ i ].sym );

		// Find it in the captionToUnicodeFolder
		int idx = captionToUnicode.Find( captionname );
		if ( idx != captionToUnicode.InvalidIndex() )
		{
			// Skip blank english entries
			if ( captionlist[ i ].blankenglish )
			{
				vprint( 0, "skipping %s, english caption is blank\n", captionname );
				continue;
			}

			wchar_t *u = captionToUnicode[ idx ];

			wchar_t *prefix = captionlist[ i ].commands;

			wchar_t composed[ MAX_LOCALIZED_CHARS ];

			int maxlen = ( sizeof( composed ) / sizeof( wchar_t ) ) - 1;

			if ( prefix )
			{
				_snwprintf( composed, maxlen, L"%s%s", prefix, u );
			}
			else
			{
				wcsncpy( composed, u, maxlen );
			}

			composed[ maxlen ] = L'\0';

			// Write to buffer
			WriteAsciiStringAsUnicode( buf, captionname, true );
			WriteAsciiStringAsUnicode( buf, "\t", false );
			WriteUnicodeString( buf, composed, true );
			WriteAsciiStringAsUnicode( buf, "\r\n", false );

			// Now write the "[ENGLISH]" entry
			char engcap[ 512 ];
			Q_snprintf( engcap, sizeof( engcap ), "[english]%s", captionname );

			WriteAsciiStringAsUnicode( buf, engcap, true );
			WriteAsciiStringAsUnicode( buf, "\t", false );
			WriteUnicodeString( buf, captionlist[ i ].english ? captionlist[ i ].english : L"???", true );
			WriteAsciiStringAsUnicode( buf, "\r\n", false );
		}
		else
		{
			vprint( 0, "no lookup for cc token '%s'\n", captionname );
		}
	}

	// Now try and spit out a file like the cc english file, but with the new data
	FileHandle_t fh = g_pFullFileSystem->Open( CAPTION_OUT_FILE , "wb" );
	if ( FILESYSTEM_INVALID_HANDLE != fh )
	{
		g_pFullFileSystem->Write( buf.Base(), buf.TellPut(), fh );
		g_pFullFileSystem->Close( fh );
	}
	else
	{
		vprint( 0, "Unable to open %s for writing\n", CAPTION_OUT_FILE );
	}

	// Cleanup memory
	c = newCaptions.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		LookupData_t& data = newCaptions[ i ];
		delete[] data.unicode;
		free( data.caption );
	}

	newCaptions.Purge();
}

bool IsWavFileDucked( char const *name )
{
	CSentence sentence;
	if ( LoadSentenceFromWavFile( name , sentence ) )
	{
		return sentence.GetVoiceDuck();
	}

	vprint( 0, "IsWavFileDucked:  Missing .wav %s!!!\n", name );
	return false;
}

void SetWavFileDucking( char const *name, bool ducking )
{
	CSentence sentence;
	if ( LoadSentenceFromWavFile( name , sentence ) )
	{
		Assert( sentence.GetVoiceDuck() != ducking );

		sentence.SetVoiceDuck( ducking );

		// Save it back out
		SaveWave( name, sentence );

		vprint( 1, "duck(%s):  %s\n", ducking ? "true" : "false", name );
		return;
	}

	vprint( 0, "SetWavFileDucking:  Missing .wav %s!!!\n", name );
}

void SyncDucking( CUtlVector< CUtlSymbol >& english, CUtlVector< CUtlSymbol >& localized )
{
	int i, c;

	CUtlRBTree< CUtlSymbol > englishducked( 0, 0, DefLessFunc( CUtlSymbol ) );
	CUtlRBTree< CUtlSymbol > englishunducked( 0, 0, DefLessFunc( CUtlSymbol ) );

	int fromoffset = Q_strlen( fromdir ) + 1;
	int tooffset = Q_strlen( todir ) + 1;

	c = english.Count();
	for ( i = 0; i < c; ++i )
	{
		CUtlSymbol& sym = english[ i ];
		char fn[ 512 ];
		Q_strncpy( fn, g_Analysis.symbols.String( sym ), sizeof( fn ) );

		if ( !( i % 1000 ) )
		{
			vprint( 1, "analyzed %i / %i (%.1f %%)\n", i, c, 100.0f * (float)i/(float)c );
		}

		bool ducked = IsWavFileDucked( fn );

		CUtlSymbol croppedSym = g_Analysis.symbols.AddString( &fn[ fromoffset ] );

		if ( ducked )
		{
			englishducked.Insert( croppedSym );
		}
		else
		{
			englishunducked.Insert( croppedSym );
		}
	}


	int updated = 0;

	// Now walk the localized tree and sync it to the english version
	c = localized.Count();
	for ( i = 0; i < c; ++i )
	{
		CUtlSymbol& sym = localized[ i ];
		char fn[ 512 ];
		Q_strncpy( fn, g_Analysis.symbols.String( sym ), sizeof( fn ) );

		bool ducked = IsWavFileDucked( fn );

		CUtlSymbol croppedSym = g_Analysis.symbols.AddString( &fn[ tooffset ] );

		bool inenglishducked = englishducked.Find( croppedSym ) != englishducked.InvalidIndex() ? true : false;
		bool inenglishunducked = englishunducked.Find( croppedSym ) != englishunducked.InvalidIndex() ? true : false;

		if ( !( i % 100 ) )
		{
			vprint( 1, "sync'd %i / %i (%.1f %%)\n", i, c, 100.0f * (float)i/(float)c );
		}

		if ( ducked && inenglishducked )
			continue;
		if ( !ducked && inenglishunducked )
			continue;

		if ( !inenglishducked && !inenglishunducked )
		{
			vprint( 0, "Warning:  %s is in localized tree, missing from english tree!!\n", fn );
			continue;
		}

		Assert( inenglishducked ^ inenglishunducked );

		SetWavFileDucking( fn, inenglishducked );
		++updated;
	}

	vprint( 0, "finished, updated %i / %i (%.1f %%) localized .wavs\n", updated, c, 100.0f * (float)updated/(float)c );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CLocalizationCheckApp : public CTier3SteamApp
{
	typedef CTier3SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual void Destroy() {}
};

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CLocalizationCheckApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CLocalizationCheckApp::Create()
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "localization_check", 2 );

	AppSystemInfo_t appSystems[] = 
	{
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CLocalizationCheckApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	g_pFileSystem = filesystem = g_pFullFileSystem;
	if ( !g_pFullFileSystem || !g_pSoundEmitterSystem || !g_pVGuiLocalize )
	{
		Error( "Unable to load required library interface!\n" );
		return false;
	}

	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

	// If they didn't specify -game on the command line, use VPROJECT.
	if ( !SetupSearchPaths( workingdir, false, true ) )
	{
		Warning( "Unable to set up the file system!\n" );
		return false;
	}

	// work out of the root directory (same as the reslists)
	g_pFullFileSystem->AddSearchPath(".", "root");

	return true; 
}


void CLocalizationCheckApp::PostShutdown( )
{
	g_pFileSystem = filesystem = NULL;
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int CLocalizationCheckApp::Main()
{
	char language[ 256 ];
	memset( language, 0, sizeof( language ) );

	bool extractenglish = false;
	bool forceducking = false;

	int iArg = 1;
	int argc = CommandLine()->ParmCount();
	for (; iArg<argc ; iArg++)
	{
		char const *pArg = CommandLine()->GetParm( iArg );
		if ( pArg[ 0 ] == '-' )
		{
			switch( pArg[ 1 ] )
			{
			case 'p':
				extractenglish = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'r':
				regenerate = true;
				break;
			case 'q':
				regenerate_quiet = true;
				break;
			case 'u':
				generate_usage = true;
				break;
			case 'b':
				nuke = true;
				break;
			case 's':
				checkscriptsounds = true;
				break;
			case 'c':
				build_cc = true;
				break;
			case 'x':
				build_script = true;
				break;
			case 'w':
				wavcheck = true;
				Q_strncpy( sounddir, CommandLine()->GetParm( iArg + 1 ), sizeof( sounddir ) );
				iArg++;
				break;
			case 'e':
				extractphonemes = true;
				Q_strncpy( sounddir, CommandLine()->GetParm( iArg + 1 ), sizeof( sounddir ) );
				iArg++;
				break;
			case 'l':
				if ( !Q_stricmp( &pArg[1], "loop" ) )
				{
					checkforloops = true;
					Q_strncpy( sounddir, CommandLine()->GetParm( iArg + 1 ), sizeof( sounddir ) );
					iArg++;
				}
				else
				{
					uselogfile = true;
				}
				break;
			case 'f':
				if ( !Q_stricmp( pArg, "-forceduck" ))
				{
					forceducking = true;
					break;
				}
				forceextract = true;
				break;
			case 'd':
				checkfordups = true;
				break;
			case 'i':
				{
					importcaptions = true;
					Q_strncpy( importfile, CommandLine()->GetParm( iArg + 1 ), sizeof( importfile ) );
					iArg++;
				}
				break;
			case 'm':
				{
					makecopybatch = true;
					Q_strncpy( fromdir, CommandLine()->GetParm( iArg + 1 ), sizeof( fromdir ) );
					Q_strncpy( todir, CommandLine()->GetParm( iArg + 2 ), sizeof( todir ) );
					iArg += 2;
				}
				break;
			case 'a':
				{
					syncducking = true;
					Q_strncpy( fromdir, CommandLine()->GetParm( iArg + 1 ), sizeof( fromdir ) );
					Q_strncpy( todir, CommandLine()->GetParm( iArg + 2 ), sizeof( todir ) );
					iArg += 2;
				}
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 2 || (iArg != argc ) )
	{
		PrintHeader();
		printusage();
	}

	Q_strncpy( language, CommandLine()->GetParm( argc - 1 ), sizeof( language ) );

	// If it's english, turn off checks.
	if ( !Q_stricmp( language, "english" ) )
	{
		language[ 0 ] = 0;
	}

	if ( !forceducking && !checkforloops && !syncducking && !extractphonemes && !importcaptions && language[0] != 0 )
	{
		// See if it's a valid language
		int idx = CSentence::LanguageForName( language );
		if ( idx == -1 )
		{
			vprint( 0, "\nSkipping language check, '%s' is not a valid language\n", language );

			vprint( 0, "Valid Language Names:\n" );
			for ( int j = 0; j < CC_NUM_LANGUAGES; ++j )
			{
				vprint( 2, "%s\n", CSentence::NameForLanguage( j ) );
			}

			printusage();
		}
	}

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Looking for localization inconsistencies...\n" );

	if ( !checkforloops&& !syncducking && !extractphonemes && !importcaptions && language[0] != 0 )
	{
		vprint( 0, "\nLanguage:  %s\n", language );
	}

	vprint( 0, "Initializing stub sound system\n" );

	sound->Init();

	vprint( 0, "Initializing localization database system\n" );

	// Always start with english
	g_pVGuiLocalize->AddFile( "resource/closecaption_english.txt" );

	// Todo add language specific file

	Q_FixSlashes( gamedir );
	Q_strlower( gamedir );

	char vcddir[ 512 ];
	Q_snprintf( vcddir, sizeof( vcddir ), "%sscenes", gamedir );
	char ccdir[ 512 ];
	Q_snprintf( ccdir, sizeof( ccdir ), "%ssound/combined", gamedir );

	vprint( 0, "game dir %s\nvcd dir %s\n\n",
		gamedir,
		vcddir );

	Q_StripTrailingSlash( sounddir );
	Q_StripTrailingSlash( vcddir );

	//
	//ProcessMaterialsDirectory( vmtdir );

	vprint( 0, "Initializing sound emitter system\n" );
	g_pSoundEmitterSystem->ModInit();

	vprint( 0, "Loaded %i sounds\n", g_pSoundEmitterSystem->GetSoundCount() );

	if ( forceducking )
	{
		CUtlVector< CUtlSymbol > wavefiles;

		char workingdir[ 256 ];
		workingdir[0] = 0;
		Q_getwd( workingdir, sizeof( workingdir ) );

		BuildFileList( wavefiles, workingdir, ".wav" );
		vprint( 0, "forcing ducking on %i .wav files in %s\n\n", wavefiles.Count(), workingdir );
		for ( int i = 0; i < wavefiles.Count(); i++ )
		{
			CUtlSymbol& sym = wavefiles[ i ];
			char fn[ 512 ];
			Q_strncpy( fn, g_Analysis.symbols.String( sym ), sizeof( fn ) );
			SetWavFileDucking( fn, true );
		}
	}
	else
	{
		vprint( 0, "Building list of .vcd files\n" );
		CUtlVector< CUtlSymbol > vcdfiles;

		BuildFileList( vcdfiles, vcddir, ".vcd" );
		vprint( 0, "found %i .vcd files\n\n", vcdfiles.Count() );

		if ( extractenglish && !language[0] )
		{
			vprint( 0, "extractenglish:  pulling raw english txt from file\n" );
			ExtractEnglish();
		}
		else if ( wavcheck )
		{
			vprint( 0, "wavcheck:  building list of known .wav files\n" );
			CUtlVector< CUtlSymbol > wavfiles;
			BuildFileList( wavfiles, va( "%ssound/%s", gamedir, sounddir ), ".wav" );

			vprint( 0, "found %i .wav files\n\n", wavfiles.Count() );

			WavCheck( wavfiles );
		}
		else if ( makecopybatch )
		{
			vprint( 0, "makecopybatch:  building list of known .wav files\n" );
			CUtlVector< CUtlSymbol > wavfiles;
			BuildFileList( wavfiles, va( "%ssound/%s", gamedir, sounddir ), ".wav" );

			vprint( 0, "found %i .wav files\n\n", wavfiles.Count() );

			MakeBatchFile( wavfiles, fromdir, todir );
		}
		else if ( extractphonemes )
		{
			vprint( 0, "extractphonemes:  building list of known .wav files\n" );
			CUtlVector< CUtlSymbol > wavfiles;
			BuildFileList( wavfiles, sounddir, ".wav" );

			vprint( 0, "found %i .wav files in %s (and subdirs)\n\n", wavfiles.Count(), sounddir );

			ExtractPhonemes( wavfiles );
		}
		else if ( checkforloops )
		{
			vprint( 0, "checkforloops:  building list of known .wav files\n" );
			CUtlVector< CUtlSymbol > wavfiles;
			BuildFileList( wavfiles, sounddir, ".wav" );

			vprint( 0, "found %i .wav files in %s (and subdirs)\n\n", wavfiles.Count(), sounddir );

			CheckForLoops( wavfiles );
		}
		else if ( importcaptions )
		{
			vprint( 0, "importcaptions:  importing captions from '%s'\n", importfile );
			ImportCaptions( importfile );
		}
		else if ( checkfordups )
		{
			vprint( 0, "checkfordups:  checking for duplicate captions\n" );
			CheckDuplcatedText();
		}
		else if ( syncducking )
		{
			vprint( 0, "syncducking:  building list of known .wav files in\n  %s\n  %s\n", fromdir, todir );
			CUtlVector< CUtlSymbol > englishfiles;
			BuildFileList( englishfiles, fromdir, ".wav" );
			vprint( 0, "found %i .wav files in %s (and subdirs)\n\n", englishfiles.Count(), fromdir );

			CUtlVector< CUtlSymbol > localized_files;
			BuildFileList( localized_files, todir, ".wav" );
			vprint( 0, "found %i .wav files in %s (and subdirs)\n\n", localized_files.Count(), todir );
		
			SyncDucking( englishfiles, localized_files );
		}
		else
		{

			CUtlVector< CUtlSymbol > vcdsinreslist;

			BuildVCDAndMapNameListsFromReslists( vcdsinreslist );

			// Check for missing localization data for all speak events not marked CC_DISABLED
			CUtlRBTree< CUtlSymbol, int >	referencedcaptionwaves( 0, 0, SymbolLessFunc );

			vprint( 0, "\nValidating close caption tokens and combined .wav file checksums\n\n" );


			CheckLocalizationEntries( vcdfiles, referencedcaptionwaves );

			vprint( 0, "\nChecking for orphaned combined .wav files\n\n" );

			CUtlVector< CUtlSymbol > combinedwavfiles;
			BuildFileList( combinedwavfiles, ccdir, ".wav" );

			CheckForOrphanedCombinedWavs( combinedwavfiles, referencedcaptionwaves );

			if ( language[0] != 0 )
			{
				vprint( 0, "\nChecking for missing or out of date localized combined .wav files\n\n" );
				ValidateForeignLanguageWaves( language, combinedwavfiles );
			}

			if ( generate_usage )
			{
				// Figure out which .vcds are unused
				CheckUnusedVcds( vcdsinreslist, vcdfiles );
			}

			if ( checkscriptsounds )
			{
				CheckUnusedSounds();
			}
		}
	}

	vprint( 0, "\nCleaning up...\n" );

	g_pSoundEmitterSystem->ModShutdown();

	// Unload localization system
	g_pVGuiLocalize->RemoveAll();

	sound->Shutdown();

	FileSystem_Term();

	g_Analysis.symbols.RemoveAll();

	return 0;
}
