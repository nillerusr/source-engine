//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//


#include "MapReslistGenerator.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "cmd.h"
#include "common.h"
#include "quakedef.h"
#include "vengineserver_impl.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <tier0/dbg.h>
#include "host.h"
#include "host_state.h"
#include "utlbuffer.h"
#include "characterset.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PAUSE_FRAMES_BETWEEN_MAPS	300
#define PAUSE_TIME_BETWEEN_MAPS		2.0f

extern engineparms_t host_parms;

#define ENGINE_RESLIST_FILE "engine.lst"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MapReslistGenerator_Usage()
{ 
	Msg( "-makereslists usage:\n" );
	Msg( "  [ -makereslists <optionalscriptfile> ] -- script file to control more complex makereslists operations (multiple passes, etc.)\n" );
	Msg( "  [ -usereslistfile filename ] -- get map list from specified file, default is to build for maps/*.bsp\n" );
	Msg( "  [ -startmap mapname ] -- restart generation at specified map (after crash, implies resume)\n" );
	Msg( "  [ -condebug ] -- prepend console.log entries with mapname or engine if not in a map\n" );
	Msg( "  [ +map mapname ] -- generate reslists for specified map and exit after that map\n" );
	Msg( "  [ -rebuildaudio ] -- force rebuild of _other_rebuild.cache (metacache) file at exit\n" );
	Msg( "  [ -forever ] -- when you get to the end of the maplist, start over from the top\n" );
	Msg( "  [ -reslistdir ] -- default is 'reslists', use this to override\n" );
	Msg( "  [ -startstage nnn ] -- when running from script file, this starts at specified stage\n" );
	Msg( "  [ -collate ] -- skip everything, just merge the reslist from temp folders to the final folder again\n" );
}

void MapReslistGenerator_Init()
{
	// check for reslist generation
	if ( CommandLine()->FindParm("-makereslists") )
	{
		bool usemaplistfile = false;
		if ( CommandLine()->FindParm("-usereslistfile") )
		{
			usemaplistfile = true;
		}
		MapReslistGenerator().EnableReslistGeneration( usemaplistfile );
	}
	else if ( CommandLine()->FindParm( "-rebuildaudio" ) )
	{
		MapReslistGenerator().SetAutoQuit( true );
	}

	if ( CommandLine()->FindParm( "-trackdeletions" ) )
	{
		MapReslistGenerator().EnableDeletionsTracking();
	}
}

void MapReslistGenerator_Shutdown()
{
	MapReslistGenerator().Shutdown();
}

void MapReslistGenerator_BuildMapList()
{
	MapReslistGenerator().BuildMapList();
}

CMapReslistGenerator g_MapReslistGenerator;
CMapReslistGenerator &MapReslistGenerator()
{
	return g_MapReslistGenerator;
}

static bool ReslistLogLessFunc( CUtlString const &pLHS, CUtlString const &pRHS )
{
	return CaselessStringLessThan( pLHS.Get(), pRHS.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMapReslistGenerator::CMapReslistGenerator() : 
	m_AlreadyWrittenFileNames( 0, 0, true ),
	m_DeletionListWarnings( 0, 0, DefLessFunc( CUtlSymbol ) ),
	m_EngineLog( 0, 0, ReslistLogLessFunc ),
	m_MapLog( 0, 0, ReslistLogLessFunc ),
	m_bAutoQuit( false )
{
	MEM_ALLOC_CREDIT_CLASS();

	m_bUsingMapList = false;
	m_bTrackingDeletions = false;
	m_bLoggingEnabled = false;
	m_iCurrentMap = 0;
	m_flNextMapRunTime = 0.0f;
	m_iFrameCountdownToRunningNextMap = 0;
	m_szPrefix[0] = '\0';
	m_szLevelName[0] = '\0';
	m_iPauseTimeBetweenMaps = PAUSE_TIME_BETWEEN_MAPS;
	m_iPauseFramesBetweenMaps = PAUSE_FRAMES_BETWEEN_MAPS;
	m_bRestartOnTransition = false;
	m_bLogToEngineList = true;
	m_sResListDir = "reslists";
}

void CMapReslistGenerator::SetAutoQuit( bool bState )
{
	m_bAutoQuit = bState;
}

void CMapReslistGenerator::BuildMapList()
{
	if ( !IsEnabled() )
		return;

	MapReslistGenerator_Usage();

	// Get the maplist file, if any
	const char *pMapFile = NULL;
	CommandLine()->CheckParm( "-usereslistfile", &pMapFile );

	// +map argument precludes using a maplist file
	bool bUseMap = CommandLine()->FindParm("+map") != 0;
	bool bUseMapListFile = bUseMap ? false : CommandLine()->FindParm("-usereslistfile") != 0;

	// Build the map list
	if ( !BuildGeneralMapList( &m_Maps, bUseMapListFile, pMapFile, "reslists", &m_iCurrentMap ) )
	{
		m_bLoggingEnabled = false;
	}
}

bool BuildGeneralMapList( CUtlVector<maplist_map_t> *aMaps, bool bUseMapListFile, const char *pMapFile, char *pSystemMsg, int *iCurrentMap )
{
	if ( !bUseMapListFile )
	{
		// If the user passed in a +map parameter, just use that single map
		char const *pMapName = NULL;
		if ( CommandLine()->CheckParm( "+map", &pMapName ) && pMapName )
		{
			// ensure validity
			char szMapFile[64] = { 0 };
			V_snprintf( szMapFile, sizeof( szMapFile ), "maps/%s.bsp", pMapName );
			if (g_pVEngineServer->IsMapValid( szMapFile ))
			{
				// add to list
				maplist_map_t newMap;
				Q_strncpy(newMap.name, pMapName, sizeof(newMap.name));
				aMaps->AddToTail( newMap );
			}

			CommandLine()->RemoveParm( "+map" );
		}
		else
		{
			// build the list of all the levels to scan
			// Search the directory structure.
			const char *mapwild = "maps/*.bsp";
			char const *findfn = Sys_FindFirst( mapwild, NULL, 0 );
			while ( findfn )
			{
				// make sure that it's in the mod filesystem
				if ( !g_pFileSystem->FileExists( va("maps/%s", findfn), "MOD" ) )
				{
					findfn = Sys_FindNext( NULL, 0 );
					continue;
				}

				// strip extension
				char sz[ MAX_PATH ];
				Q_strncpy( sz, findfn, sizeof( sz ) );
				char *ext = strchr( sz, '.' );
				if (ext)
				{
					ext[0] = 0;
				}

				// move to next item
				findfn = Sys_FindNext( NULL, 0  );

				// ensure validity
				char szMapFile[64] = { 0 };
				V_snprintf( szMapFile, sizeof( szMapFile ), "maps/%s.bsp", sz );
				if (!g_pVEngineServer->IsMapValid( szMapFile ))
					continue;

				// add to list
				maplist_map_t newMap;
				Q_strncpy(newMap.name, sz, sizeof(newMap.name));
				aMaps->AddToTail( newMap );
			}

			Sys_FindClose();
		}
	}
	else
	{
		// Read from file
		if ( pMapFile )
		{
			// Load them in
			FileHandle_t resfilehandle;
			resfilehandle = g_pFileSystem->Open( pMapFile, "rb" );
			if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
			{
				// Read in and parse mapcycle.txt
				int length = g_pFileSystem->Size(resfilehandle);
				if ( length > 0 )
				{
					char *pStart = (char *)new char[ length + 1 ];
					if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) ) )
					{
						pStart[ length ] = 0;
						const char *pFileList = pStart;

						while ( 1 )
						{
							char szMap[ MAX_OSPATH ];

							pFileList = COM_Parse( pFileList );
							if ( strlen( com_token ) <= 0 )
								break;

							Q_strncpy(szMap, com_token, sizeof(szMap));

							// ensure validity
							char szMapFile[64] = { 0 };
							V_snprintf( szMapFile, sizeof( szMapFile ), "maps/%s.bsp", szMap );
							if (!g_pVEngineServer->IsMapValid( szMapFile ))
								continue;

							// Any more tokens on this line?
							while ( COM_TokenWaiting( pFileList ) )
							{
								pFileList = COM_Parse( pFileList );
							}

							maplist_map_t newMap;
							Q_strncpy(newMap.name, szMap, sizeof(newMap.name));
							aMaps->AddToTail( newMap );
						}
					}
					delete[] pStart;
				}

				g_pFileSystem->Close(resfilehandle);
			}
			else
			{
				Error( "Unable to load %s maplist file: %s\n", pSystemMsg, pMapFile );
				return false;
			}

		}
		else
		{
			Error( "Unable to find %s maplist filename\n", pSystemMsg );
			return false;
		}
	}

	int c = aMaps->Count();
	if ( c == 0 )
	{
		Msg( "%s: No maps found\n", pSystemMsg );
		return false;
	}

	Msg( "%s: Creating for:\n", pSystemMsg );

	// Determine the current map (-startmap allows starts mid-maplist)
	*iCurrentMap = 0;
	char const *startmap = NULL;
	if ( CommandLine()->CheckParm( "-startmap", &startmap ) && startmap )
	{
		for ( int i = 0 ; i < c; ++i )
		{
			if ( !Q_stricmp( aMaps->Element(i).name, startmap ) )
			{
				*iCurrentMap = i;
			}
		}
	}

	for ( int i = 0 ; i < c; ++i )
	{
		if ( i < *iCurrentMap )
		{
			Msg( "-  %s\n", aMaps->Element(i).name );
		}
		else
		{
			Msg( "+  %s\n", aMaps->Element(i).name );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Reconstructs engine log dictionary from existing engine reslist.
// This is used to restore state after a restart, otherwise the engine log
// would aggregate duplicate files.
//-----------------------------------------------------------------------------
void CMapReslistGenerator::BuildEngineLogFromReslist()
{
	m_EngineLog.RemoveAll();

	CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFileSystem->ReadFile( CFmtStr( "%s\\%s", m_sResListDir.String(), ENGINE_RESLIST_FILE ), "DEFAULT_WRITE_PATH", buffer ) )
	{
		// does not exist
		return;
	}

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// parse reslist
	char szToken[MAX_PATH];
	for ( ;; )
	{
		int nTokenSize = buffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}

		int idx = m_EngineLog.Find( szToken );
		if ( idx == m_EngineLog.InvalidIndex() )
		{
			m_EngineLog.Insert( szToken );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends specified line to the engine reslist.
//-----------------------------------------------------------------------------
void CMapReslistGenerator::LogToEngineReslist( char const *pLine )
{
	// prevent unecessary duplication due to file appending
	int idx = m_EngineLog.Find( pLine );
	if ( idx != m_EngineLog.InvalidIndex() )
	{
		// already logged
		return;
	}

	m_EngineLog.Insert( pLine );

	// Open for append, write data, close.
	FileHandle_t fh = g_pFileSystem->Open( CFmtStr( "%s\\%s", m_sResListDir.String(), ENGINE_RESLIST_FILE ), "at", "DEFAULT_WRITE_PATH" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Write( "\"", 1, fh );
		g_pFileSystem->Write( pLine, Q_strlen( pLine  ), fh );
		g_pFileSystem->Write( "\"\n", 2, fh );
		g_pFileSystem->Close( fh );
	}
}

//-----------------------------------------------------------------------------
// Purpose: initializes the object to enable reslist generation
//-----------------------------------------------------------------------------
void CMapReslistGenerator::EnableReslistGeneration( bool usemaplistfile )
{
	//hackhack !!!! This is a work-around until CS precaches things on level start, not player spawn
	if ( !Q_stricmp( "cstrike", GetCurrentMod() ))
	{
		m_iPauseTimeBetweenMaps = PAUSE_TIME_BETWEEN_MAPS * 3;
		m_iPauseFramesBetweenMaps = PAUSE_FRAMES_BETWEEN_MAPS * 3;
	}

	m_bUsingMapList = usemaplistfile;

	m_bLoggingEnabled = true;

	char const *pszDir = NULL;
	if ( CommandLine()->CheckParm( "-reslistdir", &pszDir ) && pszDir )
	{
		char szDir[ MAX_PATH ];
		Q_strncpy( szDir, pszDir, sizeof( szDir ) );
		Q_StripTrailingSlash( szDir );
		Q_strlower( szDir );
		Q_FixSlashes( szDir );

		if ( Q_strlen( szDir ) > 0 )
		{
			m_sResListDir = szDir;
		}
	}

	// create file to dump out to
	g_pFileSystem->CreateDirHierarchy( m_sResListDir.String() , "DEFAULT_WRITE_PATH" );

	// Leave the existing one if resuming from a specific map, otherwise, blow it away
	if ( !CommandLine()->FindParm( "-startmap" ) )
	{
		g_pFileSystem->RemoveFile( CFmtStr( "%s\\%s", m_sResListDir.String(), ENGINE_RESLIST_FILE ), "DEFAULT_WRITE_PATH" );
		m_EngineLog.RemoveAll();
	}
	else
	{
		BuildEngineLogFromReslist();
	}

	// add logging function
	g_pFileSystem->AddLoggingFunc(&FileSystemLoggingFunc);
}

//-----------------------------------------------------------------------------
// Purpose: starts the first map
//-----------------------------------------------------------------------------
void CMapReslistGenerator::StartReslistGeneration()
{
	m_iCurrentMap = 0;
	m_iFrameCountdownToRunningNextMap = m_iPauseFramesBetweenMaps;
	m_flNextMapRunTime = Sys_FloatTime() + m_iPauseTimeBetweenMaps;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mapname - 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::SetPrefix( char const *mapname )
{
	Q_snprintf( m_szPrefix, sizeof( m_szPrefix ), "%s:  ", mapname );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CMapReslistGenerator::LogPrefix()
{
	// If not recording stuff to file, then use the "default" prefix.
	if ( m_bLogToEngineList )
	{
		return "engine:  ";
	}

	return m_szPrefix;
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnLevelLoadStart(const char *levelName)
{
	if ( !IsEnabled() )
		return;

	// reset the duplication list
	m_AlreadyWrittenFileNames.RemoveAll();

	// prepare for map logging
	m_bLogToEngineList = false;
	m_MapLog.RemoveAll();
	V_strncpy( m_szLevelName, levelName, sizeof( m_szLevelName ) );

	// add in the bsp file to the list, and its node graph
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "maps\\%s.bsp", levelName );
	OnResourcePrecached( path );

	bool useNodeGraph = true;
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		useNodeGraph = modinfo->GetInt( "nodegraph", 1 ) != 0;
	}
	modinfo->deleteThis();

	if ( useNodeGraph )
	{
		Q_snprintf(path, sizeof(path), "maps\\graphs\\%s.ain", levelName);
		OnResourcePrecached(path);
	}
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnLevelLoadEnd()
{
}

void CMapReslistGenerator::OnPlayerSpawn()
{
	if ( !IsEnabled() )
		return;

	// initiate the next level
	m_iFrameCountdownToRunningNextMap = m_iPauseFramesBetweenMaps;
	m_flNextMapRunTime = Sys_FloatTime() + m_iPauseTimeBetweenMaps;
}

bool CMapReslistGenerator::ShouldRebuildCaches()
{
	if ( !IsEnabled() )
	{
		return CommandLine()->FindParm( "-rebuildaudio" ) != 0;
	}

	if ( !CommandLine()->FindParm( "-norebuildaudio" ) )
		return true;
	return false;
}

char const *CMapReslistGenerator::GetResListDirectory() const
{
	return m_sResListDir.String();
}

void CMapReslistGenerator::DoQuit()
{
	Cbuf_AddText( "quit\n" );
	// remove the logging
	g_pFileSystem->RemoveLoggingFunc(&FileSystemLoggingFunc);
	m_bLogToEngineList = true;
}

//-----------------------------------------------------------------------------
// Purpose: call every frame if we're enabled, just so that the next map can be triggered at the right time
//-----------------------------------------------------------------------------
void CMapReslistGenerator::RunFrame()
{
	if ( !IsEnabled() )
	{
		if ( m_bAutoQuit )
		{
			m_bAutoQuit = false;
			DoQuit();
		}
		return;
	}

	if ( --m_iFrameCountdownToRunningNextMap > 0 )
		return;

	if ( m_flNextMapRunTime && m_flNextMapRunTime < Sys_FloatTime() )
	{
		// about to transition or terminate, emit the current map log
		WriteMapLog();

		if ( m_Maps.IsValidIndex( m_iCurrentMap ) )
		{
			m_flNextMapRunTime = 0.0f;
			m_iFrameCountdownToRunningNextMap = 0;

			if ( !m_bRestartOnTransition )
			{
				Cbuf_AddText( va( "map %s\n", m_Maps[m_iCurrentMap].name ) );

				SetPrefix( m_Maps[m_iCurrentMap].name );

				++m_iCurrentMap;
				if ( m_Maps.IsValidIndex( m_iCurrentMap ) )
				{
					// cause a full engine restart on the transition to the next map
					// ensure that one-time init code logs correctly to each map reslist
					m_bRestartOnTransition = true;
				}
			}
			else
			{
				// restart at specified map
				CommandLine()->RemoveParm( "-startmap" );
				CommandLine()->AppendParm( "-startmap", m_Maps[m_iCurrentMap].name );	
				HostState_Restart();
			}
		}
		else
		{
			// no more levels, just quit
			if ( !CommandLine()->FindParm( "-forever" ) )
			{
				DoQuit();
			}
			else
			{
				StartReslistGeneration();
				m_bRestartOnTransition = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: logs and handles mdl files being precaches
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnModelPrecached(const char *relativePathFileName)
{
	if ( !IsEnabled() )
		return;

	if (strstr(relativePathFileName, ".vmt"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		char file[_MAX_PATH];

		if (!Q_strnicmp(relativePathFileName, "materials", strlen("materials")))
		{
			Q_strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			Q_snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}
		OnResourcePrecached(file);

		// get the matching vtf file
		char *ext = strstr(file, ".vmt");
		if (ext)
		{
			Q_strncpy(ext, ".vtf", 5);
			OnResourcePrecached(file);
		}
	}
	else
	{
		OnResourcePrecached(relativePathFileName);
	}
}

//-----------------------------------------------------------------------------
// Purpose: logs sound file access
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnSoundPrecached(const char *relativePathFileName)
{
	// skip any special characters
	if (!V_isalnum(relativePathFileName[0]))
	{
		++relativePathFileName;
	}

	// prepend the sound/ directory if necessary
	char file[_MAX_PATH];
	if (!Q_strnicmp(relativePathFileName, "sound", strlen("sound")))
	{
		Q_strncpy(file, relativePathFileName, sizeof(file));
	}
	else
	{
		// prepend the sound directory
		Q_snprintf(file, sizeof(file), "sound\\%s", relativePathFileName);
	}

	OnResourcePrecached(file);
}

//-----------------------------------------------------------------------------
// Purpose: logs the precache as a file access
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnResourcePrecached(const char *relativePathFileName)
{
	if ( !IsEnabled() )
		return;

	// ignore empty string
	if (relativePathFileName[0] == 0)
		return;

	// ignore files that start with '*' since they signify special models
	if (relativePathFileName[0] == '*')
		return;

	char fullPath[_MAX_PATH];
	if (g_pFileSystem->GetLocalPath(relativePathFileName, fullPath, sizeof(fullPath)))
	{
		OnResourcePrecachedFullPath(fullPath);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Logs out file access to a file
//-----------------------------------------------------------------------------
void CMapReslistGenerator::OnResourcePrecachedFullPath(const char *fullPathFileName)
{
	char fixed[ MAX_PATH ];
	Q_strncpy( fixed, fullPathFileName, sizeof( fixed ) );
	Q_strlower( fixed );
	Q_FixSlashes( fixed );

	// make sure the filename hasn't already been written
	UtlSymId_t filename = m_AlreadyWrittenFileNames.Find( fixed );
	if ( filename != UTL_INVAL_SYMBOL )
		return;

	// record in list, so we don't write it again
	m_AlreadyWrittenFileNames.AddString( fixed );

	// add extras for mdl's
	if (strstr(fixed, ".mdl"))
	{
		// it's a model, get it's other files as well
		char file[_MAX_PATH];
		Q_strncpy(file, fixed, sizeof(file) - 10);
		char *ext = strstr(file, ".mdl");

		Q_strncpy(ext, ".vvd", 10);
		OnResourcePrecachedFullPath(file);
		Q_strncpy(ext, ".ani", 10);
		OnResourcePrecachedFullPath(file);
		Q_strncpy(ext, ".dx80.vtx", 10);
		OnResourcePrecachedFullPath(file);
		Q_strncpy(ext, ".dx90.vtx", 10);
		OnResourcePrecachedFullPath(file);
		Q_strncpy(ext, ".sw.vtx", 10);
		OnResourcePrecachedFullPath(file);
		Q_strncpy(ext, ".phy", 10);
		OnResourcePrecachedFullPath(file);
		Q_strncpy(ext, ".jpg", 10);
		OnResourcePrecachedFullPath(file);
	}

	// strip it down relative to the root directory of the game (for steam)
	char const *relativeFileName = Q_stristr( fixed, GetBaseDirectory() );
	if ( relativeFileName )
	{
		// Skip the basedir and slash
		relativeFileName += ( Q_strlen( GetBaseDirectory() ) + 1 );
	}

	if ( !relativeFileName )
	{
		return;
	}

	if ( m_bLogToEngineList )
	{
		LogToEngineReslist( relativeFileName );
	}
	else
	{
		// find or add to sorted tree
		int idx = m_MapLog.Find( relativeFileName );
		if ( idx == m_MapLog.InvalidIndex() )
		{
			m_MapLog.Insert( relativeFileName );
		}
	}
}

void CMapReslistGenerator::WriteMapLog()
{
	if ( !m_szLevelName[0] )
	{
		// log has not been established yet
		return;
	}

	// write the sorted map log, allows for easier diffs between revisions
	char path[_MAX_PATH];
	Q_snprintf( path, sizeof( path ), "%s\\%s.lst", m_sResListDir.String(), m_szLevelName );
	FileHandle_t fh = g_pFileSystem->Open( path, "wt", "DEFAULT_WRITE_PATH" );
	for ( int i = m_MapLog.FirstInorder(); i != m_MapLog.InvalidIndex(); i = m_MapLog.NextInorder( i ) )
	{
		const char *pLine = m_MapLog[i].String();
		g_pFileSystem->Write( "\"", 1, fh );
		g_pFileSystem->Write( pLine, Q_strlen( pLine ), fh );
		g_pFileSystem->Write( "\"\n", 2, fh );
	}
	g_pFileSystem->Close( fh );
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CMapReslistGenerator::FileSystemLoggingFunc(const char *fullPathFileName, const char *options)
{
	g_MapReslistGenerator.OnResourcePrecachedFullPath(fullPathFileName);
}

#define DELETIONS_BATCH_FILE		"deletions.bat"
#define DELETIONS_WARNINGS_FILE		"undelete.lst"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::EnableDeletionsTracking()
{
	unsigned int deletions = 0;
	unsigned int warnings = 0;

	// Load deletions file and build dictionary
	m_bTrackingDeletions =  true;;

	// Open up deletions.bat and parse out all filenames
	// Load them in
	FileHandle_t deletionsfile;
	deletionsfile = g_pFileSystem->Open( DELETIONS_BATCH_FILE, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != deletionsfile )
	{
		// Read in and parse mapcycle.txt
		int length = g_pFileSystem->Size(deletionsfile);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFileSystem->Read(pStart, length, deletionsfile) ) )
			{
				pStart[ length ] = 0;
				const char *pFileList = pStart;

				while ( 1 )
				{
					char filename[ MAX_OSPATH ];

					pFileList = COM_Parse( pFileList );
					if ( strlen( com_token ) <= 0 )
						break;

					if ( !Q_stricmp( com_token, "del" ) )
						continue;

					Q_snprintf(filename, sizeof( filename ), "%s/%s", com_gamedir, com_token );

					// Any more tokens on this line?
					while ( COM_TokenWaiting( pFileList ) )
					{
						pFileList = COM_Parse( pFileList );
					}

					Q_FixSlashes( filename );
					Q_strlower( filename );
	
					m_DeletionList.AddString( filename );

					++deletions;
				}
			}
			delete[] pStart;
		}

		g_pFileSystem->Close(deletionsfile);
	}
	else
	{
		Warning( "Unable to load deletions.bat file %s\n", DELETIONS_BATCH_FILE );
		m_bTrackingDeletions = false;
		return;
	}

	FileHandle_t warningsfile = g_pFileSystem->Open( DELETIONS_WARNINGS_FILE, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != warningsfile )
	{
		// Read in and parse mapcycle.txt
		int length = g_pFileSystem->Size(warningsfile);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFileSystem->Read(pStart, length, warningsfile) ) )
			{
				pStart[ length ] = 0;
				const char *pFileList = pStart;

				while ( 1 )
				{
					pFileList = COM_Parse( pFileList );
					if ( strlen( com_token ) <= 0 )
						break;

					Q_FixSlashes( com_token );
					Q_strlower( com_token );
	
					CUtlSymbol sym = m_DeletionListWarningsSymbols.AddString( com_token );
					int idx = m_DeletionListWarnings.Find( sym );
					if ( idx == m_DeletionListWarnings.InvalidIndex() )
					{
						m_DeletionListWarnings.Insert( sym );
						++warnings;
					}
				}
			}
			delete[] pStart;
		}

		g_pFileSystem->Close(warningsfile);
	}
	
	// Hook up logging function
	g_pFileSystem->AddLoggingFunc( &TrackDeletionsLoggingFunc );

	Msg( "Tracking deletions (%u files in deletion list in '%s', %u previous warnings loaded from '%s'\n",
		deletions, DELETIONS_BATCH_FILE, warnings, DELETIONS_WARNINGS_FILE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fullPathFileName - 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::TrackDeletions( const char *fullPathFileName )
{
	Assert( m_bTrackingDeletions );

	char test[ _MAX_PATH ];
	Q_strncpy( test, fullPathFileName, sizeof( test ) );
	Q_FixSlashes( test );
	Q_strlower( test );

	CUtlSymbol sym = m_DeletionList.Find( test );
	if ( UTL_INVAL_SYMBOL != sym )
	{
		CUtlSymbol warningSymbol = m_DeletionListWarningsSymbols.AddString( test );

		uint idx = m_DeletionListWarnings.Find( warningSymbol );
		if ( idx == m_DeletionListWarnings.InvalidIndex() )
		{
			Msg( "--> Referenced file marked for deletion \"%s\"\n", test );
			m_DeletionListWarnings.Insert( warningSymbol );
		}
	}

	// add extras for mdl's
	if (strstr(test, ".mdl"))
	{
		// it's a model, get it's other files as well
		char file[_MAX_PATH];
		Q_strncpy(file, test, sizeof(file) - 10);
		char *ext = strstr(file, ".mdl");

		Q_strncpy(ext, ".vvd", 10);
		TrackDeletions(file);
		Q_strncpy(ext, ".ani", 10);
		TrackDeletions(file);
		Q_strncpy(ext, ".dx80.vtx", 10);
		TrackDeletions(file);
		Q_strncpy(ext, ".dx90.vtx", 10);
		TrackDeletions(file);
		Q_strncpy(ext, ".sw.vtx", 10);
		TrackDeletions(file);
		Q_strncpy(ext, ".phy", 10);
		TrackDeletions(file);
		Q_strncpy(ext, ".jpg", 10);
		TrackDeletions(file);
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CMapReslistGenerator::TrackDeletionsLoggingFunc(const char *fullPathFileName, const char *options)
{
	g_MapReslistGenerator.TrackDeletions(fullPathFileName);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapReslistGenerator::Shutdown()
{
	if ( m_bTrackingDeletions )
	{
		SpewTrackedDeletionsLog();

		g_pFileSystem->RemoveLoggingFunc( &TrackDeletionsLoggingFunc );
		m_DeletionList.RemoveAll();
		m_DeletionListWarnings.RemoveAll();
		m_DeletionListWarningsSymbols.RemoveAll();
		m_bTrackingDeletions = NULL;
	}
}

void CMapReslistGenerator::SpewTrackedDeletionsLog()
{
	if ( !m_bTrackingDeletions )
		return;

	FileHandle_t hUndeleteFile = g_pFileSystem->Open( DELETIONS_WARNINGS_FILE, "wt", "DEFAULT_WRITE_PATH" );
	if ( FILESYSTEM_INVALID_HANDLE == hUndeleteFile )
	{
		return;
	}

	for ( int i = m_DeletionListWarnings.FirstInorder(); i != m_DeletionListWarnings.InvalidIndex() ; i = m_DeletionListWarnings.NextInorder( i ) )
	{
		char const *filename = m_DeletionListWarningsSymbols.String( m_DeletionListWarnings[ i ] );

		g_pFileSystem->Write("\"", 1, hUndeleteFile);
		g_pFileSystem->Write(filename, Q_strlen(filename), hUndeleteFile);
		g_pFileSystem->Write("\"\n", 2, hUndeleteFile);
	}

	g_pFileSystem->Close( hUndeleteFile );
}

	
