//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: vcd_sound_check.cpp : Defines the entry point for the console application.
//
//=============================================================================//
#include "cbase.h"
#include <stdio.h>
#include <windows.h>
#include <direct.h>
#include "tier0/dbg.h"
#include "utldict.h"
#include "KeyValues.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "vstdlib/random.h"

bool uselogfile = true;

struct AnalysisData
{
	CUtlSymbolTable				symbols;
};

static AnalysisData g_Analysis;

static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;

static bool spewed = false;
static bool preview = false;
static bool parallel = false;

static char g_szDesignation[ 256 ];
static char outdir[ 256 ];
static char rootdir[ 256 ];
static int rootoffset = 0;

int START_PAGE = 0;

#define LINE_WIDTH	80
#define LINES_PER_PAGE	45

char *va( const char *fmt, ... )
{
	static char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );
	return string;
}

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
	vprint( 0, "usage:  paginate startpage \"designation\" <root directory> [<outdir>]\n\
		\t-p = preview only\n\
		\ne.g.:  paginate -p 700000 \"Confidential\" u:/production u:/production_numbered" );

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

	int extlen = extension ? strlen( extension ) : 0 ;

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
				if ( !extension || !stricmp( &wfd.cFileName[ len - extlen ], extension ) )
				{
					char filename[ MAX_PATH ];
					Q_snprintf( filename, sizeof( filename ), "%s\\%s", dir, wfd.cFileName );
					_strlwr( filename );

					Q_FixSlashes( filename );

					CUtlSymbol sym = g_Analysis.symbols.AddString( filename );
					files.AddToTail( sym );

					if ( !( files.Count() % 3000 ) )
					{
						if( extension )
						{
							vprint( 0, "...found %i .%s files\n", files.Count(), extension );
						}
						else
						{
							vprint( 0, "...found %i files\n", files.Count() );
						}
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

struct PageStats_t
{
	int linecount;
	int pagecount;
	int totalbytes;
};

void Paginate_CreatePath(const char *path)
{
	char temppath[512];
	Q_strncpy( temppath, path, sizeof(temppath) );
	
	for (char *ofs = temppath+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{       // create the directory
			char old = *ofs;
			*ofs = 0;
			_mkdir (temppath);
			*ofs = old;
		}
	}
}

void PaginateFile( int startpage, PageStats_t *stats, char const *filename )
{
	FILE *fp = fopen( filename, "rb" );
	if ( !fp )
		return;

	char outfn[ 256 ];

	Q_StripExtension( filename, outfn, sizeof( outfn ) );

	if ( parallel )
	{
		Q_snprintf( outfn, sizeof( outfn ), "%s/%s", outdir, &filename[ rootoffset ] );
		Q_FixSlashes( outfn );

		if ( !preview )
		{
			Paginate_CreatePath( outfn );
		}
	}
	else
	{
		Q_strncat( outfn, "_numbered.out", sizeof( outfn ), COPY_ALL_CHARACTERS );
	}

	FILE *outfile = NULL;
	
	if ( !preview )
	{
		outfile = fopen( outfn, "wb" );
		if ( !outfile )
		{
			vprint( 0, "Unable to open %s for writing\n", outfn );
			return;
		}
	}
	

	fseek( fp, 0, SEEK_END );
	int size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	char *buf = new char[ size + 1 ];
	if ( !buf )
		exit( -2 );

	fread( buf, size, 1, fp );
	fclose( fp );

	buf[ size ] = 0;

	stats->totalbytes += size;

	char *in = buf;
	
	int numlines = stats->linecount;
	int chars_on_line = 0;

	while ( 1 )
	{
		if ( !*in )
			break;

		if ( *in == '\n' )
		{
			++stats->linecount;
			chars_on_line = 0;
		}

		if ( *in == '\t' )
		{
			if ( !preview )
			{
				fprintf( outfile, "    " );
			}
			chars_on_line += 4;
			++in;
		}

		if ( ( chars_on_line ) >= LINE_WIDTH )
		{
			chars_on_line = 0;
			++stats->linecount;
		}

		if ( stats->linecount - numlines >= LINES_PER_PAGE )
		{
			++stats->pagecount;
			numlines = stats->linecount;

			if ( !preview )
			{
				fprintf( outfile, "\r\n%s  VLV%i\r\nPAGEBREAK\r\n",
					g_szDesignation,
					startpage + START_PAGE + stats->pagecount );
			}
		}

		if ( !preview )
		{
			fwrite( (void *)in, 1, 1, outfile );
		}

		++chars_on_line;
		++in;
	}

	// Final page number
	{
		++stats->pagecount;
		numlines = stats->linecount;

		if ( !preview )
		{
			fprintf( outfile, "\r\n%s  VLV%i\r\nPAGEBREAK\r\n",
				g_szDesignation,
				startpage + START_PAGE + stats->pagecount );
		}
	}

	if ( !preview )
	{
		fclose( outfile );
	}

	delete[] buf;
}

void PaginateFilesInDirectory( char const *rootdir )
{
	CUtlVector< CUtlSymbol > files;
	BuildFileList( files, rootdir, NULL );

	vprint( 0, "Found %i files\n", files.Count() );

	PageStats_t stats;
	memset( &stats, 0, sizeof( stats ) );

	// Process files...
	int c = files.Count();

	vprint( 1, "%-160s\t%8s\t%10s\t%12s\t%35s\n",
		"filename",
		"pages",
		"lines",
		"size",
		"Bates#" );

	for ( int i = 0 ; i < c; ++i )
	{
		PageStats_t pagestats;
		memset( &pagestats, 0, sizeof( pagestats ) );
		char const *fn = g_Analysis.symbols.String( files[ i ] );

		PaginateFile( stats.pagecount, &pagestats, fn );

		char p1[ 32 ];
		char p2[ 32 ];

		Q_snprintf( p1, sizeof( p1 ), "VLV%i", START_PAGE + stats.pagecount + 1 );
		Q_snprintf( p2, sizeof( p2 ), "VLV%i", START_PAGE + stats.pagecount + pagestats.pagecount );

		vprint( 1, "%-160s\t%8i\t%10i\t%12s\t%35s\n",
			fn,
			pagestats.pagecount,
			pagestats.linecount,
			Q_pretifymem( pagestats.totalbytes, 2 ),
			va( "%s -> %s", p1, p2 ) );

		stats.linecount += pagestats.linecount;
		stats.pagecount += pagestats.pagecount;
		stats.totalbytes += pagestats.totalbytes;

	}

	vprint( 0, "Processed %s\n", Q_pretifymem( stats.totalbytes, 2 ));
	vprint( 0, "Line count %i\n", stats.linecount );
	vprint( 0, "Page count %i\n", stats.pagecount );

	vprint( 0, "Final Bates # %d\n", START_PAGE + stats.pagecount );
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
	vprint( 0, "Valve Software - paginate.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Insert Bates #'s for the lawyers ---\n" );
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
	SpewActivate( "paginate", 2 );

	Q_strncpy( g_szDesignation, "Confidential - Attorney's Eyes Only", sizeof( g_szDesignation ) );

	int i=1;
	for ( i ; i<argc ; i++)
	{
		if ( argv[ i ][ 0 ] == '-' )
		{
			switch( argv[ i ][ 1 ] )
			{
			case 'p':
				preview = true;
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 4 || ( i != argc ) )
	{
		PrintHeader();
		printusage();
	}

	outdir[ 0 ] = 0;

	if ( argc == 6 || ( argc == 5 && !preview ) )
	{
		--i;
		Q_strncpy( outdir, argv[ argc - 1 ], sizeof( outdir ) );
		parallel = true;
		--argc;
	}

	START_PAGE = atoi( argv[ argc - 3 ] );
	if ( START_PAGE == 0 )
	{
		PrintHeader();
		printusage();
	}

	Q_strncpy( g_szDesignation, argv[ argc - 2 ], sizeof( g_szDesignation ) );

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Paginating and Bates numbering documents...\n" );

	strcpy( rootdir, argv[ argc - 1 ] );

	Q_FixSlashes( rootdir );
	Q_strlower( rootdir );
	Q_StripTrailingSlash( rootdir );

	rootoffset = Q_strlen( rootdir ) + 1;

	vprint( 0, "dir %s\n\n", rootdir );

	PaginateFilesInDirectory( rootdir );

	return 0;
}