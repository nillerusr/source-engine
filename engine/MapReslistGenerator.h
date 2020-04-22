//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPRESLISTGENERATOR_H
#define MAPRESLISTGENERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "filesystem.h"
#include "utlvector.h"
#include "utlsymbol.h"
#include "utlstring.h"

// Map list creation
// Map entry
struct maplist_map_t
{
	char name[64];
};

// General purpose maplist generator.
//	aMaps: Utlvector you'd like filled with the maps.
//	bUseMapListFile: true if you want to use a maplist file, vs parsing the maps directory or using +map
//	pMapFile: If you're using a maplist file, this should be the filename of it
//	pSystemMsg: Used to preface and debug messages
//	iCurrentMap: The map in the list to begin at. Handles the -startmap parameter for you.
bool BuildGeneralMapList( CUtlVector<maplist_map_t> *aMaps, bool bUseMapListFile, const char *pMapFile, char *pSystemMsg, int *iCurrentMap );

// initialization
void MapReslistGenerator_Init();
void MapReslistGenerator_Shutdown();
void MapReslistGenerator_BuildMapList();

//-----------------------------------------------------------------------------
// Purpose: Handles collating lists of resources on level load
//			Used to generate reslists for steam
//-----------------------------------------------------------------------------
class CMapReslistGenerator
{
public:
	CMapReslistGenerator();

	// initializes the object to enable reslist generation
	void		EnableReslistGeneration( bool usemaplistfile );

	// starts the reslist generation (starts cycling maps)
	void		StartReslistGeneration();

	void		BuildMapList();

	void		Shutdown();

	// call every frame if we're enabled, just so that the next map can be triggered at the right time
	void		RunFrame();

	// returns true if reslist generation is enabled
	bool		IsEnabled()			{ return m_bLoggingEnabled; }
	bool		IsLoggingToMap()	{ return m_bLoggingEnabled && !m_bLogToEngineList; }

	// call to mark level load/end
	void		OnLevelLoadStart(const char *levelName);
	void		OnLevelLoadEnd();
	void		OnPlayerSpawn();

	// call to mark resources as being precached
	void		OnResourcePrecached(const char *relativePathFileName);
	void		OnModelPrecached(const char *relativePathFileName);
	void		OnSoundPrecached(const char *relativePathFileName);

	char const *LogPrefix();

	void		EnableDeletionsTracking();
	void		TrackDeletions( const char *fullPathFileName );

	bool		ShouldRebuildCaches();
	char const *GetResListDirectory() const;

	void		SetAutoQuit( bool bState );

private:
	static void TrackDeletionsLoggingFunc(const char *fullPathFileName, const char *options);
	static void FileSystemLoggingFunc(const char *fullPathFileName, const char *options);
	void		OnResourcePrecachedFullPath(const char *fullPathFileName);
	void		BuildEngineLogFromReslist();
	void		LogToEngineReslist( char const *pLine );
	void		WriteMapLog();
	void		SetPrefix( char const *mapname );
	void		SpewTrackedDeletionsLog();
	void		DoQuit();

	bool		m_bTrackingDeletions;
	bool		m_bLoggingEnabled;
	bool		m_bUsingMapList;
	bool		m_bRestartOnTransition;
	// true for engine, false for map
	bool		m_bLogToEngineList;
	bool		m_bAutoQuit;

	// list of all maps to scan
	CUtlVector<maplist_map_t> m_Maps;
	int m_iCurrentMap;
	float m_flNextMapRunTime;
	int m_iFrameCountdownToRunningNextMap;
	CUtlSymbolTable m_AlreadyWrittenFileNames;
	int m_iPauseTimeBetweenMaps;
	int m_iPauseFramesBetweenMaps;

	char		m_szPrefix[64];
	char		m_szLevelName[64];

	CUtlSymbolTable m_DeletionList;
	CUtlRBTree< CUtlSymbol > m_DeletionListWarnings;
	CUtlSymbolTable m_DeletionListWarningsSymbols;

	CUtlRBTree< CUtlString, int > m_MapLog;
	CUtlRBTree< CUtlString, int > m_EngineLog;
	CUtlString	m_sResListDir;
};

// singleton accessor
CMapReslistGenerator &MapReslistGenerator();


#endif // MAPRESLISTGENERATOR_H
