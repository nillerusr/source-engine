//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// tagbuild.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <windows.h>
#include <sys/stat.h>
#include <time.h>

#include "interface.h"
#include "imysqlwrapper.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlstring.h"
#include "tier1/utldict.h"
#include "KeyValues.h"
#include "filesystem_helpers.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "base_gamestats_parse.h"
#include "cbase.h"
#include "gamestats.h"
#include "tier0/icommandline.h"

// roll-our-own symbol table class.  Note we don't use CUtlSymbolTable because that and related classes have short int deeply baked in as index type, so can
// only hold 64K entries.  We sometimes need to process more than 64K files at a time.
struct AnalysisData
{
	AnalysisData()
	{
		symbols.SetLessFunc( CaselessStringLessThanIgnoreSlashes );
	}

	~AnalysisData()
	{
		int i = symbols.FirstInorder();
		while ( i != symbols.InvalidIndex() )
		{
			const char *symbol = symbols[i];
			if ( symbol )
			{
				delete symbol;
			}
			i = symbols.NextInorder( i );
		}
	}

	CUtlRBTree<const char*,int>	symbols;
};

static AnalysisData g_Analysis;

static bool describeonly = false;

typedef int (*DataParseFunc)( ParseContext_t * );
typedef void (*PostImportFunc) ( IMySQL *sql );
typedef bool (*ParseCurrentUserIDFunc)( char const *pchDataFile, char *pchUserID, size_t bufsize, time_t &modifiedtime );

extern int CS_ParseCustomGameStatsData( ParseContext_t *ctx );
extern int Ep2_ParseCustomGameStatsData( ParseContext_t *ctx );
extern int TF_ParseCustomGameStatsData( ParseContext_t *ctx );
extern void TF_PostImport( IMySQL *sql );

int Default_ParseCustomGameStatsData( ParseContext_t *ctx );

extern bool Ep2_ParseCurrentUserID( char const *pchDataFile, char *pchUserID, size_t bufsize, time_t &modifiedtime );

struct DataParser_t
{
	char const		*pchGameName;
	DataParseFunc	pfnParseFunc;
	PostImportFunc	pfnPostImport;
	ParseCurrentUserIDFunc pfnParseUserID;
};

static DataParser_t g_ParseFuncs[] =
{
	{ "cstrike", CS_ParseCustomGameStatsData, NULL },
	{ "tf", TF_ParseCustomGameStatsData, TF_PostImport },
//	{ "dods", Default_ParseCustomGameStatsData, NULL },
//	{ "portal", Default_ParseCustomGameStatsData, NULL },
	{ "ep1", Default_ParseCustomGameStatsData, NULL }, // Just a STUB
	{ "ep2", Ep2_ParseCustomGameStatsData, NULL, Ep2_ParseCurrentUserID }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	printf( "processgamestats:\n" );
	printf( "processgamestats game dbhost user password dbname rootdir\n" );
	printf( "processgamestats game datafile [describe only]\n\n" );
	printf( "valid gamenames:\n" );

	for ( int i = 0 ; i < ARRAYSIZE( g_ParseFuncs ); ++i )
	{
		printf( "  %s\n", g_ParseFuncs[ i ].pchGameName );
	}

	// Exit app
	exit( 1 );
}

void BuildFileList_R( CUtlVector< int >& files, char const *dir, char const *extension )
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

					char *symbol = strdup( filename );
					int sym = g_Analysis.symbols.Insert( symbol );

					files.AddToTail( sym );
				}
			}
		}
	} while ( FindNextFile( ff, &wfd ) );
}

void BuildFileList( CUtlVector< int >& files, char const *rootdir, char const *extension )
{
	files.RemoveAll();
	BuildFileList_R( files, rootdir, extension );
}

void DescribeData( BasicGameStats_t &stats, const char *szStatsFileUserID, int iStatsFileVersion )
{
	double averageSession = 0.0f;
	if ( stats.m_Summary.m_nCount > 0 )
	{
		averageSession = (double)stats.m_Summary.m_nSeconds / (double)stats.m_Summary.m_nCount;
	}

	Msg( "---------------------------------------------------------------------------\n" );
	Msg( "%16.16s  :  %s\n", "User", szStatsFileUserID );
	Msg( "  %16.16s:  %8d\n", "Blob version", iStatsFileVersion );
	Msg( "  %16.16s:  %8d sessions\n", "Played", stats.m_Summary.m_nCount );
	Msg( "  %16.16s:  %8d seconds\n", "Total Time", stats.m_Summary.m_nSeconds );
	Msg( "  %16.16s:  %8.2f seconds\n", "Avg Session", averageSession );

	Msg( "  %16.16s:  %8d\n", "Commentary", stats.m_Summary.m_nCommentary );
	Msg( "  %16.16s:  %8d\n", "HDR", stats.m_Summary.m_nHDR );
	Msg( "  %16.16s:  %8d\n", "Captions", stats.m_Summary.m_nCaptions );
	Msg( "  %16.16s:  %8d\n", "Easy", stats.m_Summary.m_nSkill[ 0 ] );
	Msg( "  %16.16s:  %8d\n", "Medium", stats.m_Summary.m_nSkill[ 1 ] );
	Msg( "  %16.16s:  %8d\n", "Hard", stats.m_Summary.m_nSkill[ 2 ] );
	Msg( "  %16.16s:  %8d seconds\n", "Completion time ", stats.m_nSecondsToCompleteGame );
	Msg( "  %16.16s:  %8d\n", "Number of deaths", stats.m_Summary.m_nDeaths );

	Msg( " -- Maps played --\n" );

	for ( int i = stats.m_MapTotals.First(); i != stats.m_MapTotals.InvalidIndex(); i = stats.m_MapTotals.Next( i ) )
	{
		char const *mapname = stats.m_MapTotals.GetElementName( i );
		BasicGameStatsRecord_t &rec = stats.m_MapTotals[ i ];

		Msg( " %16.16s:  %5d seconds in %3d sessions (%4d deaths)\n", mapname, rec.m_nSeconds, rec.m_nCount, rec.m_nDeaths );
	}
}

#include <string>
//-------------------------------------------------
void v_escape_string (std::string& s)
{
	if ( !s.size() ) 
		return;
	for ( unsigned int i = 0;i<s.size();i++ )
	{
		switch (s[i]) 
		{
		case '\0':				/* Must be escaped for "mysql" */
			s[i] = '\\';
			s.insert(i+1,"0",1); i++;//lint !e534
			break;
		case '\n':				/* Must be escaped for logs */
			s[i] = '\\';
			s.insert(i+1,"n",1); i++;//lint !e534
			break;
		case '\r':
			s[i] = '\\';
			s.insert(i+1,"r",1); i++;//lint !e534
			break;
		case '\\':
			s[i] = '\\';
			s.insert(i+1,"\\",1); i++;//lint !e534
			break;
		case '\"':
			s[i] = '\\';
			s.insert(i+1,"\"",1); i++;//lint !e534
			break;
		case '\'':				/* Better safe than sorry */
			s[i] = '\\';
			s.insert(i+1,"\'",1); i++;//lint !e534
			break;
		case '\032':			/* This gives problems on Win32 */
			s[i] = '\\';
			s.insert(i+1,"Z",1); i++;//lint !e534
			break;
		default: 
			break;
		}
	}
}

void InsertData( CUtlDict< int, unsigned short >& mapOrder, IMySQL *sql, BasicGameStats_t &gs, const char *szStatsFileUserID, int iStatsFileVersion, const char *gamename, char const *tag = NULL )
{
	if ( !sql )
		return;

	char q[ 512 ];

	std::string userid;
	userid = szStatsFileUserID;
	v_escape_string( userid );

	int farthestPlayed = -1;
	std::string highestmap;

	int namelen = 20;
	if ( !Q_stricmp( gamename, "ep1" ) )
	{
		namelen = 16;
	}

	char finalname[ 64 ];

	std::string finaltag;
	finaltag = tag ? tag : "";
	v_escape_string( finaltag );

	// Deal with the maps
	for ( int i = gs.m_MapTotals.First(); i != gs.m_MapTotals.InvalidIndex(); i = gs.m_MapTotals.Next( i ) )
	{
		char const *pszMapName = gs.m_MapTotals.GetElementName( i );
		std::string mapname;
		mapname = pszMapName;
		v_escape_string( mapname );

		Q_strncpy( finalname, mapname.c_str(), namelen );

		int slot = mapOrder.Find( pszMapName );
		if ( slot != mapOrder.InvalidIndex() )
		{
			int order = mapOrder[ slot ];
			if ( order > farthestPlayed )
			{
				farthestPlayed = order;
			}
		}
		else
		{
			if ( Q_stricmp( pszMapName, "devtest" ) )
				continue;
		}

		

		BasicGameStatsRecord_t& rec = gs.m_MapTotals[ i ];

		if ( tag )
		{
			Q_snprintf( q, sizeof( q ), "REPLACE into %s_maps (UserID,LastUpdate,Version,MapName,Tag,Count,Seconds,HDR,Captions,Commentary,Easy,Medium,Hard,nonsteam,cybercafe,Deaths) values (\"%s\",Now(),%d,\"%s\",\"%s\",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d);",
				gamename,
				userid.c_str(),
				iStatsFileVersion,
				finalname,
				finaltag.c_str(),
				rec.m_nCount,
				rec.m_nSeconds,
				rec.m_nHDR,
				rec.m_nCaptions,
				rec.m_nCommentary,
				rec.m_nSkill[ 0 ],
				rec.m_nSkill[ 1 ],
				rec.m_nSkill[ 2 ],
				rec.m_bSteam ? 0 : 1,
				rec.m_bCyberCafe ? 1 : 0,
				rec.m_nDeaths );
		}
		else
		{
			Q_snprintf( q, sizeof( q ), "REPLACE into %s_maps (UserID,LastUpdate,Version,MapName,Count,Seconds,HDR,Captions,Commentary,Easy,Medium,Hard,nonsteam,cybercafe,Deaths) values (\"%s\",Now(),%d,\"%s\",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d);",
				gamename,
				userid.c_str(),
				iStatsFileVersion,
				finalname,
				rec.m_nCount,
				rec.m_nSeconds,
				rec.m_nHDR,
				rec.m_nCaptions,
				rec.m_nCommentary,
				rec.m_nSkill[ 0 ],
				rec.m_nSkill[ 1 ],
				rec.m_nSkill[ 2 ],
				rec.m_bSteam ? 0 : 1,
				rec.m_bCyberCafe ? 1 : 0,
				rec.m_nDeaths );
		}

		int retcode = sql->Execute( q );
		if ( retcode != 0 )
		{
			printf( "Query %s failed\n", q );
			return;
		}
	}

	if ( farthestPlayed != -1 )
	{
		highestmap = mapOrder.GetElementName( farthestPlayed );
	}
	v_escape_string( highestmap );
	Q_strncpy( finalname, highestmap.c_str(), namelen );

	if ( tag )
	{
		Q_snprintf( q, sizeof( q ), "REPLACE into %s (UserID,LastUpdate,Version,Tag,Count,Seconds,HDR,Captions,Commentary,Easy,Medium,Hard,SecondsToCompleteGame,HighestMap,nonsteam,cybercafe,hl2_chapter,dxlevel,Deaths) values (\"%s\",Now(),%d,\"%s\",%d,%d,%d,%d,%d,%d,%d,%d,%d,\"%s\",%d,%d,%d,%d,%d);",

			gamename,
			userid.c_str(),
			iStatsFileVersion,
			finaltag.c_str(),
			gs.m_Summary.m_nCount,
			gs.m_Summary.m_nSeconds,
			gs.m_Summary.m_nHDR,
			gs.m_Summary.m_nCaptions,
			gs.m_Summary.m_nCommentary,
			gs.m_Summary.m_nSkill[ 0 ],
			gs.m_Summary.m_nSkill[ 1 ],
			gs.m_Summary.m_nSkill[ 2 ],
			gs.m_nSecondsToCompleteGame,
			finalname,
			gs.m_bSteam ? 0 : 1,
			gs.m_bCyberCafe ? 1 : 0,
			gs.m_nHL2ChaptureUnlocked,
			gs.m_nDXLevel,
			gs.m_Summary.m_nDeaths );
	}
	else
	{
		Q_snprintf( q, sizeof( q ), "REPLACE into %s (UserID,LastUpdate,Version,Count,Seconds,HDR,Captions,Commentary,Easy,Medium,Hard,SecondsToCompleteGame,HighestMap,nonsteam,cybercafe,hl2_chapter,dxlevel,Deaths) values (\"%s\",Now(),%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\"%s\",%d,%d,%d,%d,%d);",

			gamename,
			userid.c_str(),
			iStatsFileVersion,
			gs.m_Summary.m_nCount,
			gs.m_Summary.m_nSeconds,
			gs.m_Summary.m_nHDR,
			gs.m_Summary.m_nCaptions,
			gs.m_Summary.m_nCommentary,
			gs.m_Summary.m_nSkill[ 0 ],
			gs.m_Summary.m_nSkill[ 1 ],
			gs.m_Summary.m_nSkill[ 2 ],
			gs.m_nSecondsToCompleteGame,
			finalname,
			gs.m_bSteam ? 0 : 1,
			gs.m_bCyberCafe ? 1 : 0,
			gs.m_nHL2ChaptureUnlocked,
			gs.m_nDXLevel,
			gs.m_Summary.m_nDeaths );
	}

	int retcode = sql->Execute( q );
	if ( retcode != 0 )
	{
		printf( "Query %s failed\n", q );
		return;
	}
}
CUtlDict< int, unsigned short > g_mapOrder;

void BuildMapList( void )
{
	void *buffer = NULL;
	char *pFileList;
	FILE * pFile;
	pFile = fopen ("maplist.txt", "r");
	int i = 0;

	if ( pFile )
	{
		long lSize;
		// obtain file size.
		fseek (pFile , 0 , SEEK_END);
		lSize = ftell (pFile);
		rewind (pFile);

		// allocate memory to contain the whole file.
		buffer = (char*) malloc (lSize);
		if ( buffer != NULL )
		{
			// copy the file into the buffer.
			fread (buffer,1,lSize,pFile);
			pFileList = (char*)buffer;
			char szToken[1024];

			while ( 1 )
			{
				pFileList = ParseFile( pFileList, szToken, false );

				if ( pFileList == NULL )
					break;

				g_mapOrder.Insert( szToken, i );
				i++;
			}
		}

		fclose( pFile );
		free( buffer );
	}
	else
	{
		Msg( "Couldn't load maplist.txt for mod!!!\n" );
	}
}

int Default_ParseCustomGameStatsData( ParseContext_t *ctx )
{
	FILE *fp = fopen( ctx->file, "rb" );
	if ( fp )
	{
		CUtlBuffer statsBuffer;

		struct _stat sb;
		_stat( ctx->file, &sb );

		statsBuffer.Clear();
		statsBuffer.EnsureCapacity( sb.st_size );
		fread( statsBuffer.Base(), sb.st_size, 1, fp );
		statsBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, sb.st_size );
		fclose( fp );

		char shortname[ 128 ];
		Q_FileBase( ctx->file, shortname, sizeof( shortname ) );

		char szCurrentStatsFileUserID[17];
		int iCurrentStatsFileVersion;

		iCurrentStatsFileVersion = statsBuffer.GetShort();
		statsBuffer.Get( szCurrentStatsFileUserID, 16 );
		szCurrentStatsFileUserID[ sizeof( szCurrentStatsFileUserID ) - 1 ] = 0;

		bool valid = true;

		unsigned int iCheckIfStandardDataSaved = statsBuffer.GetUnsignedInt();
		if( iCheckIfStandardDataSaved != GAMESTATS_STANDARD_NOT_SAVED )
		{
			//standard data was saved, rewind so the stats can read in time to completion
			statsBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, -((int)sizeof( unsigned int )) );

			BasicGameStats_t stats;
			valid = stats.ParseFromBuffer( statsBuffer, iCurrentStatsFileVersion );

			if ( describeonly )
			{
				DescribeData( stats, szCurrentStatsFileUserID, iCurrentStatsFileVersion );					
			}
			else
			{
				if ( valid )
				{
					InsertData( g_mapOrder, ctx->mysql, stats, szCurrentStatsFileUserID, iCurrentStatsFileVersion, ctx->gamename );
				}
				else
				{
					++ctx->skipcount;
				}
			}
		}

		//check for custom data
		bool bHasCustomData = (valid && (statsBuffer.TellPut() != statsBuffer.TellGet()));

		if( bHasCustomData )
		{
			if( describeonly )
			{				
				//separate out the custom data and store it off for processing by other applications,
				//since they only wanted to 'describe' the data, just use a local temp and overwrite it each time

				const char *szCustomDataOutputFileName = "customdata_temp.dat";

				Msg( "\n\nFound custom data, dumping to %s\n", szCustomDataOutputFileName );

				FILE *pCustomDataOutput = fopen( szCustomDataOutputFileName, "wb+" );
				if( pCustomDataOutput )
				{
					int iGetPosition = statsBuffer.TellGet();
					fwrite( (((unsigned char *)statsBuffer.Base()) + iGetPosition), statsBuffer.TellPut() - iGetPosition, 1, pCustomDataOutput );
					fclose( pCustomDataOutput );
				}
			}
			else
			{				
				//separate out the custom data and store it off for processing by other applications,
				//assume we will have multiple input stats files from the same user, so store custom data under their userid name and overwrite old data to avoid bloat
				if( ctx->bCustomDirectoryNotMade )
				{
					CreateDirectory( "customdatadumps", NULL );
					ctx->bCustomDirectoryNotMade = false;
				}

				char szCustomDataOutputFileName[256];						
				Q_snprintf( szCustomDataOutputFileName, sizeof( szCustomDataOutputFileName ), "customdatadumps/%s.dat", szCurrentStatsFileUserID );

				FILE *pCustomDataOutput = fopen( szCustomDataOutputFileName, "wb+" );
				if( pCustomDataOutput )
				{
					int iGetPosition = statsBuffer.TellGet();
					fwrite( (((unsigned char *)statsBuffer.Base()) + iGetPosition), statsBuffer.TellPut() - iGetPosition, 1, pCustomDataOutput );
					fclose( pCustomDataOutput );
				}				
			}
		}
	}
	return CUSTOMDATA_SUCCESS;
}

int main(int argc, char* argv[])
{
	CommandLine()->CreateCmdLine( argc, argv );

	ParseContext_t ctx;

	if ( argc < 7 && argc != 3 )
	{
		printusage();
	}

	describeonly = argc == 3;

	int gameArg = 1;
	int hostArg = 2;
	int usernameArg = 3;
	int pwArg = 4;
	int dbArg = 5;
	int dirArg = 6;
	if ( describeonly )
	{
		dirArg = 2;
	}

	InitDefaultFileSystem();

	BuildMapList();
	const char *gamename = argv[ gameArg ];
	DataParseFunc parseFunc = NULL; 
	PostImportFunc postImportFunc = NULL;
	ParseCurrentUserIDFunc parseUserIDFunc = NULL;
	for ( int i = 0 ; i < ARRAYSIZE( g_ParseFuncs ); ++i )
	{
		if ( !Q_stricmp( g_ParseFuncs[ i ].pchGameName, gamename ) )
		{
			parseFunc = g_ParseFuncs[ i ].pfnParseFunc;
			postImportFunc = g_ParseFuncs[ i ].pfnPostImport;
			parseUserIDFunc = g_ParseFuncs[ i ].pfnParseUserID;
			break;
		}
	}

	if ( !parseFunc )
	{
		printf( "Invalid game name '%s'\n", gamename );
		printusage();
	}

	bool batchMode = true;

	CUtlVector< int > files;
	if ( describeonly || Q_stristr( argv[ dirArg ], ".dat" ) )
	{
		char filename[ MAX_PATH ];
		Q_snprintf( filename, sizeof( filename ), "%s", argv[ dirArg ] );
		_strlwr( filename );
		Q_FixSlashes( filename );
		char *symbol = strdup( filename );
		int sym = g_Analysis.symbols.Insert( symbol );
		files.AddToTail( sym );

		batchMode = false;
	}
	else
	{
		Msg( "Building file list\n" );
		BuildFileList( files, argv[ dirArg ], "dat" );
	}

	if ( !files.Count() )
	{
		printf( "No files to operate upon\n" );
		exit( -1 );
	}

	int c = files.Count();

	// Cull list of files by looking for most recent version of user's stats and only keeping around those files
	if ( parseUserIDFunc )
	{
		struct CUserIDFileMapping
		{
			CUserIDFileMapping() : 
				filename( UTL_INVAL_SYMBOL ), filemodifiedtime( 0 ), modcount( 1 )
			{
				userid[ 0 ] = 0;
			}

			char		userid[ 17 ];
			CUtlSymbol	filename;
			time_t		filemodifiedtime;
			int			modcount;

			static bool Less( const CUserIDFileMapping &lhs, const CUserIDFileMapping &rhs )
			{
				return Q_stricmp( lhs.userid, rhs.userid ) < 0;
			}
		};


		CUtlRBTree< CUserIDFileMapping, int > userIDToFileMap( 0, 0, CUserIDFileMapping::Less );

		int nDiscards = 0;
		int nSkips =0;
		int nMaxMod = 1;
		for ( int i = 0; i < c; ++i )
		{
			char const *fn = g_Analysis.symbols.Element( files[ i ] );

			CUserIDFileMapping search;
			search.filename = files[ i ];
			if ( (*parseUserIDFunc)( fn, search.userid, sizeof( search.userid ), search.filemodifiedtime ) )
			{
				// Find map index
				int idx = userIDToFileMap.Find( search );
				if ( idx == userIDToFileMap.InvalidIndex() )
				{
					userIDToFileMap.Insert( search );
				}
				else
				{
					CUserIDFileMapping &update = userIDToFileMap[ idx ];
					if ( search.filemodifiedtime > update.filemodifiedtime )
					{
						update.filename = files[ i ];
						update.filemodifiedtime = search.filemodifiedtime;
						update.modcount++;
						if ( update.modcount > nMaxMod )
						{
							nMaxMod = update.modcount;
						}
					}
					++nDiscards;
				}
			}
			else
			{
				++nSkips;
			}

			if ( i > 0 && !( i % 100 ) )
			{
				printf( "Parsing user ID's:  [%-6.6d/%-6.6d] %.2f %% complete\n", i, c, 100.0f * (float)i/(float)c );
			}
		}

		Msg( "discarded %d of %d, remainder %d [%d skipped] max mod %d\n", nDiscards, c, userIDToFileMap.Count(), nSkips, nMaxMod );

		// Now re-write files and count with pared down listing
		files.Purge();
		for( int i = userIDToFileMap.FirstInorder(); i != userIDToFileMap.InvalidIndex(); i = userIDToFileMap.NextInorder( i ) )
		{
			files.AddToTail( userIDToFileMap[ i ].filename );
		}
		c = files.Count();
	}
	
	bool bTrySql = !describeonly;

	bool bSqlOkay = false;

	CSysModule *sql = NULL;
	CreateInterfaceFn factory = NULL;
	IMySQL *mysql = NULL;

	if ( bTrySql )
	{
		// Now connect to steamweb and update the engineaccess table
		sql = Sys_LoadModule( "mysql_wrapper" );
		if ( sql )
		{
			factory = Sys_GetFactory( sql );
			if ( factory )
			{
				mysql = ( IMySQL * )factory( MYSQL_WRAPPER_VERSION_NAME, NULL );
				if ( mysql )
				{
 					if ( mysql->InitMySQL( argv[ dbArg ], argv[ hostArg ], argv[ usernameArg ], argv[ pwArg ] ) )
					{
						bSqlOkay = true;
						if ( batchMode )
						{
							Msg( "Successfully connected to database %s on host %s, user %s\n", 
								argv[ dbArg ], argv[ hostArg ], argv[ usernameArg ] );
						}
					}
					else
					{
						Msg( "mysql->InitMySQL( %s, %s, %s, [password]) failed\n",
							argv[ dbArg ], argv[ hostArg ], argv[ usernameArg ] );
					}
				}
				else
				{
					Msg( "Unable to get MYSQL_WRAPPER_VERSION_NAME(%s) from mysql_wrapper\n", MYSQL_WRAPPER_VERSION_NAME );
				}
			}
			else
			{
				Msg( "Sys_GetFactory on mysql_wrapper failed\n" );
			}
		}
		else
		{
			Msg( "Sys_LoadModule( mysql_wrapper ) failed\n" );
		}
	}

	ctx.gamename = gamename;
	ctx.describeonly = describeonly;
	ctx.mysql = mysql;
	ctx.skipcount = 0;
	ctx.bCustomDirectoryNotMade = true;

	if ( bSqlOkay || describeonly )
	{

		for ( int i = 0; i < c; ++i )
		{
			char const *fn = g_Analysis.symbols.Element( files[ i ] );
			
			ctx.file = fn;

			int iCustomData = (*parseFunc)( &ctx );
			if ( iCustomData == CUSTOMDATA_SUCCESS )
			{
				if ( i > 0 && !( i % 100 ) )
				{
					printf( "Processing:  [%-6.6d/%-6.6d] %.2f %% complete\n", i, c, 100.0f * (float)i/(float)c );
				}
			}
		}

		if ( ctx.skipcount > 0 )
		{
			printf( "Skipped %d samples which appear to be malformed or contain bogus data\n", ctx.skipcount );
		}

		// if this game has a post-import function to call after all the files have been imported, call it now
		if ( bSqlOkay && postImportFunc )
		{
			postImportFunc( mysql );
		}
	}

	if ( bSqlOkay )
	{
		if ( mysql )
		{
			mysql->Release();
			mysql = NULL;
		}

		if ( sql )
		{
			Sys_UnloadModule( sql );
			sql = NULL;
		}
	}

	return 0;
}


static void OverWriteCharsWeHate( char *pStr )
{
	while( *pStr )
	{
		switch( *pStr )
		{
			case '\n':
			case '\r':
			case '\\':
			case '\"':
			case '\'':
			case '\032':
			case ';':
				*pStr = ' ';
		}
		pStr++;
	}
}

void InsertKeyDataIntoTable( IMySQL *pSQL, time_t fileTime, char const *pTableName, char const *pPerfData, char const *pKeyWhiteList[], int nNumFields )
{
	char szDate[128]="Now()";
	if ( fileTime > 0 )
	{
		tm * pTm = localtime( &fileTime );
		Q_snprintf( szDate, ARRAYSIZE( szDate ), "'%04d-%02d-%02d %02d:%02d:%02d'",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec );
	}

	// we don't need to worry about semicolons embedded in string fields because we supressed them
	// on the client.  if some malicious person inserts them, the mandled field names will fail the
	// whitelist check, causing the record to be ignored.
	CUtlVector<char *> tokens;
	// split into tokens at non-quoated spaces or ;'s
	for(;;)
	{
		char const *pStr = pPerfData;
		if ( pStr[0] == 0 )
			break;
		while( pStr[0] && ( pStr[0] != ' ' ) && ( pStr[0] != ';' ) )
		{
			if ( pStr[0]=='"')
			{
				// skip to end quote
				char const *pEq = strchr( pStr + 1, '\"' );
				if ( ! pEq )
				{
					printf(" close quote with no open quote\n" );
					return;
				}
				pStr = pEq;
			}
			pStr++;
		}
		// got a field
		int nlen = pStr - pPerfData;
		if ( nlen > 2 )
		{
			char *pToken = new char[ nlen + 1 ];
			memcpy( pToken, pPerfData, nlen );
			pToken[nlen] = 0;
			tokens.AddToTail( pToken );
		}
		if ( pStr[0] )
			pStr++;
		pPerfData = pStr;
	}

	bool bBadData = false;
	char fieldNameBuffer[1024];
	char fieldValueBuffer[2048];
	strcpy( fieldNameBuffer, "(CreationTimeStamp, " );
	Q_snprintf( fieldValueBuffer, ARRAYSIZE( fieldValueBuffer), "( %s,", szDate );
	for( int i = 0; i < tokens.Count(); i++ )
	{
		char *pKVData = tokens[i];
		char *pEqualsSign = strchr( pKVData, '=' );
		if (! pEqualsSign )
		{
			bBadData = true;
			break;
		}
		*pEqualsSign = 0;								// *semicolon->null
		// check that the field is in the white list
		bool bFoundIt = false;
		for( int nCheck = 0; nCheck < nNumFields; nCheck++ )
			if ( strcmp( pKVData, pKeyWhiteList[nCheck] ) == 0 )
			{
				bFoundIt = true;
				break;
			}
		V_strncat( fieldNameBuffer, pKVData, sizeof( fieldNameBuffer ) );
		if ( i != tokens.Count() -1 )
			V_strncat( fieldNameBuffer, ",", sizeof( fieldNameBuffer ) );
		else
			V_strncat( fieldNameBuffer, ")", sizeof( fieldNameBuffer ) );
		char *pValue = pEqualsSign + 1;
		OverWriteCharsWeHate( pValue );
		if ( ( strlen( pValue ) < 1 ) || (! bFoundIt ) )
		{
			bBadData = true;
			break;
		}
		// kill lead + trail space
		if ( pValue[0] == ' ' )
			pValue++;
		if ( pValue[strlen(pValue) - 1 ] == ' ' )
			pValue[strlen( pValue ) - 1 ] =0;
		V_strncat( fieldValueBuffer, "'", sizeof( fieldValueBuffer ) );
		V_strncat( fieldValueBuffer, pValue, sizeof( fieldValueBuffer ) );
		if ( i != tokens.Count() -1 )
			V_strncat( fieldValueBuffer, "',", sizeof( fieldValueBuffer ) );
		else
			V_strncat( fieldValueBuffer, "')", sizeof( fieldValueBuffer ) );
	}
	if (! bBadData )
	{
		char sqlCommandBuffer[1024 + sizeof( fieldNameBuffer ) + sizeof( fieldValueBuffer ) ];
		sprintf( sqlCommandBuffer, "insert into %s %s values %s;", pTableName, fieldNameBuffer, fieldValueBuffer );
//			printf("cmd %s\n", sqlCommandBuffer);
		int retcode = pSQL->Execute( sqlCommandBuffer );
		if ( retcode != 0 )
		{
			printf( "command %s failed\n", sqlCommandBuffer );
		}
	}

	tokens.PurgeAndDeleteElements();
}

char const *s_PerfKeyList[] = {
	"AvgFps",
	"MinFps",
	"MaxFps",
	"CPUID",
	"CPUGhz",
	"NumCores",
	"GPUDrv",
	"GPUVendor",
	"GPUDeviceID",
	"GPUDriverVersion",
	"DxLvl",
	"Width",
	"Height",
	"MapName",
	"TotalLevelTime",
	"NumLevels"
};

void ProcessPerfData( IMySQL *pSQL, time_t fileTime, char const *pTableName, char const *pPerfData )
{
	InsertKeyDataIntoTable( pSQL, fileTime, pTableName, pPerfData, s_PerfKeyList, ARRAYSIZE( s_PerfKeyList) );
}

