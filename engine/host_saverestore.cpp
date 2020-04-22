//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Save game read and write. Any *.hl? files may be stored in memory, so use
//			g_pSaveRestoreFileSystem when accessing them. The .sav file is always stored
//			on disk, so use g_pFileSystem when accessing it.
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
// Save / Restore System

#include <ctype.h>
#ifdef _WIN32
#include "winerror.h"
#endif
#include "client.h"
#include "server.h"
#include "vengineserver_impl.h"
#include "host_cmd.h"
#include "sys.h"
#include "cdll_int.h"
#include "tmessage.h"
#include "screen.h"
#include "decal.h"
#include "zone.h"
#include "sv_main.h"
#include "host.h"
#include "r_local.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "host_state.h"
#include "datamap.h"
#include "string_t.h"
#include "PlayerState.h"
#include "saverestoretypes.h"
#include "demo.h"
#include "icliententity.h"
#include "r_efx.h"
#include "icliententitylist.h"
#include "cdll_int.h"
#include "utldict.h"
#include "decal_private.h"
#include "engine/IEngineTrace.h"
#include "enginetrace.h"
#include "baseautocompletefilelist.h"
#include "sound.h"
#include "vgui_baseui_interface.h"
#include "gl_matsysiface.h"
#include "cl_main.h"
#include "pr_edict.h"
#include "tier0/vprof.h"
#include <vgui/ILocalize.h>
#include "vgui_controls/Controls.h"
#include "tier0/icommandline.h"
#include "testscriptmgr.h"
#include "vengineserver_impl.h"
#include "saverestore_filesystem.h"
#include "tier1/callqueue.h"
#include "vstdlib/jobthread.h"
#include "enginebugreporter.h"
#include "tier1/memstack.h"
#include "vstdlib/jobthread.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#else
#include "xbox/xbox_launch.h"
#endif

#include "ixboxsystem.h"
extern IXboxSystem *g_pXboxSystem;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IBaseClientDLL *g_ClientDLL;

extern ConVar	deathmatch;
extern ConVar	skill;
extern ConVar	save_in_memory;
extern CGlobalVars g_ServerGlobalVariables;

extern CNetworkStringTableContainer *networkStringTableContainerServer;

// Keep the last 1 autosave / quick saves
ConVar save_history_count("save_history_count", "1", 0, "Keep this many old copies in history of autosaves and quicksaves." );
ConVar sv_autosave( "sv_autosave", "1", 0, "Set to 1 to autosave game on level transition. Does not affect autosave triggers." );
ConVar save_async( "save_async", "1" );
ConVar save_disable( "save_disable", "0" );
ConVar save_noxsave( "save_noxsave", "0" );

ConVar save_screenshot( "save_screenshot", "1", 0, "0 = none, 1 = non-autosave, 2 = always" );

ConVar save_spew( "save_spew", "0" );

#define SaveMsg if ( !save_spew.GetBool() ) ; else Msg

// HACK HACK:  Some hacking to keep the .sav file backward compatible on the client!!!
#define SECTION_MAGIC_NUMBER	0x54541234
#define SECTION_VERSION_NUMBER	2

CCallQueue g_AsyncSaveCallQueue;
static bool g_ConsoleInput = false;

static char g_szMapLoadOverride[32];

#define MOD_DIR ( IsX360() ? "DEFAULT_WRITE_PATH" : "MOD" )

//-----------------------------------------------------------------------------

IThreadPool *g_pSaveThread;

static bool g_bSaveInProgress = false;

//-----------------------------------------------------------------------------
static bool HaveExactMap( const char *pszMapName )
{
	char szCanonName[64] = { 0 };
	V_strncpy( szCanonName, pszMapName, sizeof( szCanonName ) );
	IVEngineServer::eFindMapResult eResult = g_pVEngineServer->FindMap( szCanonName, sizeof( szCanonName ) );

	switch ( eResult )
	{
	case IVEngineServer::eFindMap_Found:
		return true;
	case IVEngineServer::eFindMap_NonCanonical:
	case IVEngineServer::eFindMap_NotFound:
	case IVEngineServer::eFindMap_FuzzyMatch:
	case IVEngineServer::eFindMap_PossiblyAvailable:
		return false;
	}

	AssertMsg( false, "Unhandled engine->FindMap return value\n" );
	return false;
}

void FinishAsyncSave()
{
	LOCAL_THREAD_LOCK();
	SaveMsg( "FinishAsyncSave() (%d/%d)\n", ThreadInMainThread(), ThreadGetCurrentId() );
	if ( g_AsyncSaveCallQueue.Count() )
	{
		g_AsyncSaveCallQueue.CallQueued();
		g_pFileSystem->AsyncFinishAllWrites();
	}
	g_bSaveInProgress = false;
}

void DispatchAsyncSave()
{
	Assert( !g_bSaveInProgress );
	g_bSaveInProgress = true;

	if ( save_async.GetBool() )
	{
		g_pSaveThread->QueueCall( &FinishAsyncSave );
	}
	else
	{
		FinishAsyncSave();
	}
}

//-----------------------------------------------------------------------------

inline void GetServerSaveCommentEx( char *comment, int maxlength, float flMinutes, float flSeconds )
{
	if ( g_iServerGameDLLVersion >= 5 )
	{
		serverGameDLL->GetSaveComment( comment, maxlength, flMinutes, flSeconds );
	}
	else
	{
		Assert( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Alloc/free memory for save games
// Input  : num - 
//			size - 
//-----------------------------------------------------------------------------
class CSaveMemory : public CMemoryStack
{
public:
	CSaveMemory()
	{
		MEM_ALLOC_CREDIT();
		Init( 32*1024*1024, 64, 2*1024*1024 + 192*1024 );
	}

	int m_nSaveAllocs;
};

CSaveMemory &GetSaveMemory()
{
	static CSaveMemory g_SaveMemory;
	return g_SaveMemory;
}

void *SaveAllocMemory( size_t num, size_t size, bool bClear )
{
	MEM_ALLOC_CREDIT();
	++GetSaveMemory().m_nSaveAllocs;
	size_t nBytes = num * size;
	return GetSaveMemory().Alloc( nBytes, bClear );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSaveMem - 
//-----------------------------------------------------------------------------
void SaveFreeMemory( void *pSaveMem )
{
	--GetSaveMemory().m_nSaveAllocs;
	if ( !GetSaveMemory().m_nSaveAllocs )
	{
		GetSaveMemory().FreeAll( false );
	}
}

//-----------------------------------------------------------------------------
// Reset save memory stack, as some failed save/load paths will leak
//-----------------------------------------------------------------------------
void SaveResetMemory()
{
	GetSaveMemory().m_nSaveAllocs = 0;
	GetSaveMemory().FreeAll( false );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct GAME_HEADER
{
	DECLARE_SIMPLE_DATADESC();

	char	mapName[32];
	char	comment[80];
	int		mapCount;		// the number of map state files in the save file.  This is usually number of maps * 3 (.hl1, .hl2, .hl3 files)
	char	originMapName[32];
	char	landmark[256];
};

struct SAVE_HEADER 
{
	DECLARE_SIMPLE_DATADESC();

	int		saveId;
	int		version;
	int		skillLevel;
	int		connectionCount;
	int		lightStyleCount;
	int		mapVersion;
	float	time__USE_VCR_MODE; // This is renamed to include the __USE_VCR_MODE prefix due to a #define on win32 from the VCR mode changes
								// The actual save games have the string "time__USE_VCR_MODE" in them
	char	mapName[32];
	char	skyName[32];
};

struct SAVELIGHTSTYLE 
{
	DECLARE_SIMPLE_DATADESC();

	int		index;
	char	style[64];
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSaveRestore : public ISaveRestore
{
public:
	CSaveRestore()
	{
		m_bClearSaveDir = false;
		m_szSaveGameScreenshotFile[0] = 0;
		SetMostRecentElapsedMinutes( 0 );
		SetMostRecentElapsedSeconds( 0 );
		m_szMostRecentSaveLoadGame[0] = 0;
		m_szSaveGameName[ 0 ] = 0;
		m_bIsXSave = IsX360();
	}

	void					Init( void );
	void					Shutdown( void );
	void					OnFrameRendered();
	virtual bool			SaveFileExists( const char *pName );
	bool					LoadGame( const char *pName );
	char					*GetSaveDir(void);
	void					ClearSaveDir( void );
	void					DoClearSaveDir( bool bIsXSave );
	void					RequestClearSaveDir( void );
	int						LoadGameState( char const *level, bool createPlayers );
	void					LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
	const char				*FindRecentSave( char *pNameBuf, int nameBufLen );
	void					ForgetRecentSave( void );
	int						SaveGameSlot( const char *pSaveName, const char *pSaveComment, bool onlyThisLevel, bool bSetMostRecent, const char *pszDestMap = NULL, const char *pszLandmark = NULL );
	bool					SaveGameState( bool bTransition, CSaveRestoreData ** = NULL, bool bOpenContainer = true, bool bIsAutosaveOrDangerous = false );
	void					RestoreClientState( char const *fileName, bool adjacent );
	void					RestoreAdjacenClientState( char const *map );
	int						IsValidSave( void );
	void					Finish( CSaveRestoreData *save );
	void					ClearRestoredIndexTranslationTables();
	void					OnFinishedClientRestore();
	void					AutoSaveDangerousIsSafe();
	virtual void			UpdateSaveGameScreenshots();
	virtual char const		*GetMostRecentlyLoadedFileName();
	virtual char const		*GetSaveFileName();

	virtual void			SetIsXSave( bool bIsXSave ) { m_bIsXSave = bIsXSave; }
	virtual bool			IsXSave() { return ( m_bIsXSave && !save_noxsave.GetBool() ); }

	virtual void			FinishAsyncSave() { ::FinishAsyncSave(); }

	void					AddDeferredCommand( char const *pchCommand );
	virtual bool			StorageDeviceValid( void );

	virtual bool			IsSaveInProgress();

private:
	bool					SaveClientState( const char *name );

	void					EntityPatchWrite( CSaveRestoreData *pSaveData, const char *level, bool bAsync = false );
	void					EntityPatchRead( CSaveRestoreData *pSaveData, const char *level );
	void					DirectoryCount( const char *pPath, int *pResult );
	void					DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave );
	bool					DirectoryExtract( FileHandle_t pFile, int mapCount );
	void					DirectoryClear( const char *pPath );

	void					AgeSaveList( const char *pName, int count, bool bIsXSave );
	void					AgeSaveFile( const char *pName, const char *ext, int count, bool bIsXSave );
	int						SaveReadHeader( FileHandle_t pFile, GAME_HEADER *pHeader, int readGlobalState, bool *pbOldSave );
	CSaveRestoreData		*LoadSaveData( const char *level );
	void					ParseSaveTables( CSaveRestoreData *pSaveData, SAVE_HEADER *pHeader, int updateGlobals );
	int						FileSize( FileHandle_t pFile );

	bool					CalcSaveGameName( const char *pName, char *output, int outputStringLength );

	CSaveRestoreData *		SaveGameStateInit( void );
	void 					SaveGameStateGlobals( CSaveRestoreData *pSaveData );
	int						SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP(nameSize) char *name, int nameSize, OUT_Z_CAP(commentSize) char *comment, int commentSize ) OVERRIDE;
	void					BuildRestoredIndexTranslationTable( char const *mapname, CSaveRestoreData *pSaveData, bool verbose );
	char const				*GetSaveGameMapName( char const *level );

	void					SetMostRecentSaveGame( const char *pSaveName );
	int						GetMostRecentElapsedMinutes( void );
	int						GetMostRecentElapsedSeconds( void );
	int						GetMostRecentElapsedTimeSet( void );
	void					SetMostRecentElapsedMinutes( const int min );
	void					SetMostRecentElapsedSeconds( const int sec );

	struct SaveRestoreTranslate
	{
		string_t classname;
		int savedindex;
		int restoredindex;
	};

	struct RestoreLookupTable
	{
		RestoreLookupTable() :
			m_vecLandMarkOffset( 0, 0, 0 )
		{
		}

		void Clear()
		{
			lookup.RemoveAll();
			m_vecLandMarkOffset.Init();
		}

		RestoreLookupTable( const RestoreLookupTable& src )
		{
			int c = src.lookup.Count();
			for ( int i = 0 ; i < c; i++ )
			{
				lookup.AddToTail( src.lookup[ i ] );
			}

			m_vecLandMarkOffset = src.m_vecLandMarkOffset;
		}

		RestoreLookupTable& operator=( const RestoreLookupTable& src )
		{
			if ( this == &src )
				return *this;

			int c = src.lookup.Count();
			for ( int i = 0 ; i < c; i++ )
			{
				lookup.AddToTail( src.lookup[ i ] );
			}

			m_vecLandMarkOffset = src.m_vecLandMarkOffset;

			return *this;
		}

		CUtlVector< SaveRestoreTranslate >	lookup;
		Vector								m_vecLandMarkOffset;
	};

	RestoreLookupTable		*FindOrAddRestoreLookupTable( char const *mapname );
	int						LookupRestoreSpotSaveIndex( RestoreLookupTable *table, int save );
	void					ReapplyDecal( bool adjacent, RestoreLookupTable *table, decallist_t *entry );

	CUtlDict< RestoreLookupTable, int >	m_RestoreLookup;

	bool	m_bClearSaveDir;
	char	m_szSaveGameScreenshotFile[MAX_OSPATH];
	float	m_flClientSaveRestoreTime;

	char	m_szMostRecentSaveLoadGame[MAX_OSPATH];
	char	m_szSaveGameName[MAX_OSPATH];

	int		m_MostRecentElapsedMinutes;
	int		m_MostRecentElapsedSeconds;
	int		m_MostRecentElapsedTimeSet;

	bool	m_bWaitingForSafeDangerousSave;
	bool	m_bIsXSave;

	int		m_nDeferredCommandFrames;
	CUtlVector< CUtlSymbol > m_sDeferredCommands;
};

CSaveRestore g_SaveRestore;
ISaveRestore *saverestore = (ISaveRestore *)&g_SaveRestore;

BEGIN_SIMPLE_DATADESC( GAME_HEADER )

	DEFINE_FIELD( mapCount, FIELD_INTEGER ),
	DEFINE_ARRAY( mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( comment, FIELD_CHARACTER, 80 ),
	DEFINE_ARRAY( originMapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( landmark, FIELD_CHARACTER, 256 ),

END_DATADESC()


// The proper way to extend the file format (add a new data chunk) is to add a field here, and use it to determine
// whether your new data chunk is in the file or not.  If the file was not saved with your new field, the chunk 
// won't be there either.
// Structure members can be added/deleted without any problems, new structures must be reflected in an existing struct
// and not read unless actually in the file.  New structure members will be zeroed out when reading 'old' files.

BEGIN_SIMPLE_DATADESC( SAVE_HEADER )

//	DEFINE_FIELD( saveId, FIELD_INTEGER ),
//	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_FIELD( skillLevel, FIELD_INTEGER ),
	DEFINE_FIELD( connectionCount, FIELD_INTEGER ),
	DEFINE_FIELD( lightStyleCount, FIELD_INTEGER ),
	DEFINE_FIELD( mapVersion, FIELD_INTEGER ),
	DEFINE_FIELD( time__USE_VCR_MODE, FIELD_TIME ),
	DEFINE_ARRAY( mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( skyName, FIELD_CHARACTER, 32 ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( levellist_t )
	DEFINE_ARRAY( mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( landmarkName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( pentLandmark, FIELD_EDICT ),
	DEFINE_FIELD( vecLandmarkOrigin, FIELD_VECTOR ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( SAVELIGHTSTYLE )
	DEFINE_FIELD( index, FIELD_INTEGER ),
	DEFINE_ARRAY( style, FIELD_CHARACTER, 64 ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CSaveRestore::GetSaveGameMapName( char const *level )
{
	Assert( level );

	static char mapname[ 256 ];
	Q_FileBase( level, mapname, sizeof( mapname ) );
	return mapname;
}

//-----------------------------------------------------------------------------
// Purpose: returns the most recent save
//-----------------------------------------------------------------------------
const char *CSaveRestore::FindRecentSave( char *pNameBuf, int nameBufLen )
{
	Q_strncpy( pNameBuf, m_szMostRecentSaveLoadGame, nameBufLen );

	if ( !m_szMostRecentSaveLoadGame[0] )
		return NULL;

	return pNameBuf;
}

//-----------------------------------------------------------------------------
// Purpose: Forgets the most recent save game
//			this is so the current level will just restart if the player dies
//-----------------------------------------------------------------------------
void CSaveRestore::ForgetRecentSave()
{
	m_szMostRecentSaveLoadGame[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the save game directory for the current player profile
//-----------------------------------------------------------------------------
char *CSaveRestore::GetSaveDir(void)
{
	static char szDirectory[MAX_OSPATH];
	Q_memset(szDirectory, 0, MAX_OSPATH);
	Q_strncpy(szDirectory, "save/", sizeof( szDirectory ) );
	return szDirectory;
}

//-----------------------------------------------------------------------------
// Purpose: keeps the last few save files of the specified file around, renamed
//-----------------------------------------------------------------------------
void CSaveRestore::AgeSaveList( const char *pName, int count, bool bIsXSave )
{
	// age all the previous save files (including screenshots)
	while ( count > 0 )
	{
		AgeSaveFile( pName, IsX360() ? "360.sav" : "sav", count, bIsXSave );
		if ( !IsX360() )
		{
			AgeSaveFile( pName, "tga", count, bIsXSave );
		}
		count--;
	}
}

//-----------------------------------------------------------------------------
// Purpose: ages a single sav file
//-----------------------------------------------------------------------------
void CSaveRestore::AgeSaveFile( const char *pName, const char *ext, int count, bool bIsXSave )
{
	char newName[MAX_OSPATH], oldName[MAX_OSPATH];

	if ( !IsXSave() )
	{
		if ( count == 1 )
		{
			Q_snprintf( oldName, sizeof( oldName ), "//%s/%s%s.%s", MOD_DIR, GetSaveDir(), pName, ext );// quick.sav. DON'T FixSlashes on this, it needs to be //MOD
		}
		else
		{
			Q_snprintf( oldName, sizeof( oldName ), "//%s/%s%s%02d.%s", MOD_DIR, GetSaveDir(), pName, count-1, ext );	// quick04.sav, etc. DON'T FixSlashes on this, it needs to be //MOD
		}

		Q_snprintf( newName, sizeof( newName ), "//%s/%s%s%02d.%s", MOD_DIR, GetSaveDir(), pName, count, ext ); // DON'T FixSlashes on this, it needs to be //MOD
	}
	else
	{
		if ( count == 1 )
		{
			Q_snprintf( oldName, sizeof( oldName ), "%s:\\%s.%s", GetCurrentMod(), pName, ext );
		}
		else
		{
			Q_snprintf( oldName, sizeof( oldName ), "%s:\\%s%02d.%s", GetCurrentMod(), pName, count-1, ext );
		}

		Q_snprintf( newName, sizeof( newName ), "%s:\\%s%02d.%s", GetCurrentMod(), pName, count, ext );
	}

	// Scroll the name list down (rename quick04.sav to quick05.sav)
	if ( g_pFileSystem->FileExists( oldName ) )
	{
		if ( count == save_history_count.GetInt() )
		{
			// there could be an old version, remove it
			if ( g_pFileSystem->FileExists( newName ) )
			{
				g_pFileSystem->RemoveFile( newName );
			}
		}

		g_pFileSystem->RenameFile( oldName, newName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSaveRestore::IsValidSave( void )
{
	if (cmd_source != src_command)
		return 0;

	// Don't parse autosave/transition save/restores during playback!
	if ( demoplayer->IsPlayingBack() )
	{
		return 0;
	}

	if ( !sv.IsActive() )
	{
		ConMsg ("Not playing a local game.\n");
		return 0;
	}

	if ( !cl.IsActive() )
	{
		ConMsg ("Can't save if not active.\n");
		return 0;
	}

	if ( sv.IsMultiplayer() )
	{
		ConMsg ("Can't save multiplayer games.\n");
		return 0;
	}

	if ( sv.GetClientCount() > 0 && sv.GetClient(0)->IsActive() )
	{
		Assert( serverGameClients );
		CGameClient *pGameClient = sv.Client( 0 );
		CPlayerState *pl = serverGameClients->GetPlayerState( pGameClient->edict );
		if ( !pl )
		{
			ConMsg ("Can't savegame without a player!\n");
			return 0;
		}
			
		// we can't save if we're dead... unless we're reporting a bug.
		if ( pl->deadflag != false && !bugreporter->IsVisible() )
		{
			ConMsg ("Can't savegame with a dead player\n");
			return 0;
		}
	}
	
	// Passed all checks, it's ok to save
	return 1;
}

static ConVar save_asyncdelay( "save_asyncdelay", "0", 0, "For testing, adds this many milliseconds of delay to the save operation." );

//-----------------------------------------------------------------------------
// Purpose: save a game with the given name/comment
//			note: Added S_ExtraUpdate calls to fix audio pops in autosaves
//-----------------------------------------------------------------------------
int CSaveRestore::SaveGameSlot( const char *pSaveName, const char *pSaveComment, bool onlyThisLevel, bool bSetMostRecent, const char *pszDestMap, const char *pszLandmark )
{
	if ( save_disable.GetBool()  )
	{
		return 0;
	}

	if ( save_asyncdelay.GetInt() > 0 )
	{
		Sys_Sleep( clamp( save_asyncdelay.GetInt(), 0, 3000 ) );
	}

	SaveMsg( "Start save... (%d/%d)\n", ThreadInMainThread(), ThreadGetCurrentId() );
	VPROF_BUDGET( "SaveGameSlot", "Save" );
	char			hlPath[256], name[256], *pTokenData;
	int				tag, i, tokenSize;
	CSaveRestoreData	*pSaveData;
	GAME_HEADER		gameHeader;

#if defined( _MEMTEST )
	Cbuf_AddText( "mem_dump\n" );
#endif

	g_pSaveRestoreFileSystem->AsyncFinishAllWrites();

	S_ExtraUpdate();
	FinishAsyncSave();
	SaveResetMemory();
	S_ExtraUpdate();

	g_AsyncSaveCallQueue.DisableQueue( !save_async.GetBool() );

	// Figure out the name for this save game
	CalcSaveGameName( pSaveName, name, sizeof( name ) );
	ConDMsg( "Saving game to %s...\n", name );

	Q_strncpy( m_szSaveGameName, name, sizeof( m_szSaveGameName )) ;

	if ( m_bClearSaveDir )
	{
		m_bClearSaveDir = false;
		g_AsyncSaveCallQueue.QueueCall( this, &CSaveRestore::DoClearSaveDir, IsXSave() );
	}

	if ( !IsXSave() )
	{
		if ( onlyThisLevel )
		{
			Q_snprintf( hlPath, sizeof( hlPath ), "%s%s*.HL?", GetSaveDir(), sv.GetMapName() );
		}
		else
		{
			Q_snprintf( hlPath, sizeof( hlPath ), "%s*.HL?", GetSaveDir() );
		}
	}
	else
	{
		if ( onlyThisLevel )
		{
			Q_snprintf( hlPath, sizeof( hlPath ), "%s:\\%s*.HL?", GetCurrentMod(), sv.GetMapName() );
		}
		else
		{
			Q_snprintf( hlPath, sizeof( hlPath ), "%s:\\*.HL?", GetCurrentMod() );
		}
	}

	// Output to disk
	bool bClearFile = true;
	bool bIsQuick = ( stricmp(pSaveName, "quick") == 0 );
	bool bIsAutosave = ( !bIsQuick && stricmp(pSaveName,"autosave") == 0 );
	bool bIsAutosaveDangerous = ( !bIsAutosave && stricmp(pSaveName,"autosavedangerous") == 0 );
	if ( bIsQuick || bIsAutosave || bIsAutosaveDangerous )
	{
		bClearFile = false;
		SaveMsg( "Queue AgeSaveList\n"); 
		if ( StorageDeviceValid() )
		{
			g_AsyncSaveCallQueue.QueueCall( this, &CSaveRestore::AgeSaveList, CUtlEnvelope<const char *>(pSaveName), save_history_count.GetInt(), IsXSave() );
		}
	}

	S_ExtraUpdate();
	if (!SaveGameState( (pszDestMap != NULL ), NULL, false, ( bIsAutosave || bIsAutosaveDangerous )  ) )
	{
		m_szSaveGameName[ 0 ] = 0;
		return 0;	
	}
	S_ExtraUpdate();

	//---------------------------------
			
	pSaveData = serverGameDLL->SaveInit( 0 );

	if ( !pSaveData )
	{
		m_szSaveGameName[ 0 ] = 0;
		return 0;	
	}

	Q_FixSlashes( hlPath );
	Q_strncpy( gameHeader.comment, pSaveComment, sizeof( gameHeader.comment ) );

	if ( pszDestMap && pszLandmark && *pszDestMap && *pszLandmark )
	{
		Q_strncpy( gameHeader.mapName, pszDestMap, sizeof( gameHeader.mapName ) );
		Q_strncpy( gameHeader.originMapName, sv.GetMapName(), sizeof( gameHeader.originMapName ) );
		Q_strncpy( gameHeader.landmark, pszLandmark, sizeof( gameHeader.landmark ) );
	}
	else
	{
		Q_strncpy( gameHeader.mapName, sv.GetMapName(), sizeof( gameHeader.mapName ) );
		gameHeader.originMapName[0] = 0;
		gameHeader.landmark[0] = 0;
	}

	gameHeader.mapCount = 0; // No longer used. The map packer will place the map count at the head of the compound files (toml 7/18/2007)
	serverGameDLL->SaveWriteFields( pSaveData, "GameHeader", &gameHeader, NULL, GAME_HEADER::m_DataMap.dataDesc, GAME_HEADER::m_DataMap.dataNumFields );
	serverGameDLL->SaveGlobalState( pSaveData );

	// Write entity string token table
	pTokenData = pSaveData->AccessCurPos();
	for( i = 0; i < pSaveData->SizeSymbolTable(); i++ )
	{
		const char *pszToken = (pSaveData->StringFromSymbol( i )) ? pSaveData->StringFromSymbol( i ) : "";
		if ( !pSaveData->Write( pszToken, strlen(pszToken) + 1 ) )
		{
			ConMsg( "Token Table Save/Restore overflow!" );
			break;
		}
	}	

	tokenSize = pSaveData->AccessCurPos() - pTokenData;
	pSaveData->Rewind( tokenSize );


	// open the file to validate it exists, and to clear it
	if ( bClearFile && !IsX360() )
	{		
		FileHandle_t pSaveFile = g_pSaveRestoreFileSystem->Open( name, "wb" );
		if (!pSaveFile && g_pFileSystem->FileExists( name, "GAME" ) )
		{
			Msg("Save failed: invalid file name '%s'\n", pSaveName);
			m_szSaveGameName[ 0 ] = 0;
			return 0;
		}
		if ( pSaveFile )
			g_pSaveRestoreFileSystem->Close( pSaveFile );
		S_ExtraUpdate();
	}

	// If this isn't a dangerous auto save use it next
	if ( bSetMostRecent )
	{
		SetMostRecentSaveGame( pSaveName );
	}
	m_bWaitingForSafeDangerousSave = bIsAutosaveDangerous;

	int iHeaderBufferSize = 64 + tokenSize + pSaveData->GetCurPos();
	void *pMem = malloc(iHeaderBufferSize);
	CUtlBuffer saveHeader( pMem, iHeaderBufferSize );

	// Write the header -- THIS SHOULD NEVER CHANGE STRUCTURE, USE SAVE_HEADER FOR NEW HEADER INFORMATION
	// THIS IS ONLY HERE TO IDENTIFY THE FILE AND GET IT'S SIZE.
	tag = MAKEID('J','S','A','V');
	saveHeader.Put( &tag, sizeof(int) );
	tag = SAVEGAME_VERSION;
	saveHeader.Put( &tag, sizeof(int) );
	tag = pSaveData->GetCurPos();
	saveHeader.Put( &tag, sizeof(int) ); // Does not include token table

	// Write out the tokens first so we can load them before we load the entities
	tag = pSaveData->SizeSymbolTable();
	saveHeader.Put( &tag, sizeof(int) );
	saveHeader.Put( &tokenSize, sizeof(int) );
	saveHeader.Put( pTokenData, tokenSize );

	saveHeader.Put( pSaveData->GetBuffer(), pSaveData->GetCurPos() );
	
	// Create the save game container before the directory copy 
	g_AsyncSaveCallQueue.QueueCall( g_pSaveRestoreFileSystem, &ISaveRestoreFileSystem::AsyncWrite, CUtlEnvelope<const char *>(name), saveHeader.Base(), saveHeader.TellPut(), true, false, (FSAsyncControl_t *) NULL );
	g_AsyncSaveCallQueue.QueueCall( this, &CSaveRestore::DirectoryCopy, CUtlEnvelope<const char *>(hlPath), CUtlEnvelope<const char *>(name), m_bIsXSave );

	// Finish all writes and close the save game container
	// @TODO: this async finish all writes has to go away, very expensive and will make game hitchy. switch to a wait on the last async op
	g_AsyncSaveCallQueue.QueueCall( g_pFileSystem, &IFileSystem::AsyncFinishAllWrites );
	
	if ( IsXSave() && StorageDeviceValid() )
	{
		// Finish all pending I/O to the storage devices
		g_AsyncSaveCallQueue.QueueCall( g_pXboxSystem, &IXboxSystem::FinishContainerWrites );
	}

	S_ExtraUpdate();
	Finish( pSaveData );
	S_ExtraUpdate();

	// queue up to save a matching screenshot
	if ( !IsX360() && save_screenshot.GetBool() ) // X360TBD: Faster savegame screenshots
	{
		if ( !( bIsAutosave || bIsAutosaveDangerous ) || save_screenshot.GetInt() == 2 )
		{
			Q_snprintf( m_szSaveGameScreenshotFile, sizeof( m_szSaveGameScreenshotFile ), "%s%s%s.tga", GetSaveDir(), pSaveName, GetPlatformExt() );
		}
	}

	S_ExtraUpdate();

	DispatchAsyncSave();

	m_szSaveGameName[ 0 ] = 0;
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Saves a screenshot for save game if necessary
//-----------------------------------------------------------------------------
void CSaveRestore::UpdateSaveGameScreenshots()
{
	if ( IsPC() && g_LostVideoMemory )
		return;

#ifndef SWDS
	if ( m_szSaveGameScreenshotFile[0] )
	{
		host_framecount++;
		g_ClientGlobalVariables.framecount = host_framecount;
		g_ClientDLL->WriteSaveGameScreenshot( m_szSaveGameScreenshotFile );
		m_szSaveGameScreenshotFile[0] = 0;
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSaveRestore::SaveReadHeader( FileHandle_t pFile, GAME_HEADER *pHeader, int readGlobalState, bool *pbOldSave )
{
	int					i, tag, size, tokenCount, tokenSize;
	char				*pszTokenList;
	CSaveRestoreData	*pSaveData = NULL;

	if( g_pSaveRestoreFileSystem->Read( &tag, sizeof(int), pFile ) != sizeof(int) )
		return 0;

	if ( tag != MAKEID('J','S','A','V') )
	{
		Warning( "Can't load saved game, incorrect FILEID\n" );
		return 0;
	}
		
	if ( g_pSaveRestoreFileSystem->Read( &tag, sizeof(int), pFile ) != sizeof(int) )
		return 0;

	if ( tag != SAVEGAME_VERSION )				// Enforce version for now
	{
		Warning( "Can't load saved game, incorrect version (got %i expecting %i)\n", tag, SAVEGAME_VERSION );
		return 0;
	}

	if ( g_pSaveRestoreFileSystem->Read( &size, sizeof(int), pFile ) != sizeof(int) )
		return 0;

	if ( g_pSaveRestoreFileSystem->Read( &tokenCount, sizeof(int), pFile ) != sizeof(int) )
		return 0;

	if ( g_pSaveRestoreFileSystem->Read( &tokenSize, sizeof(int), pFile ) != sizeof(int) )
		return 0;

	// At this point we must clean this data up if we fail!
	void *pSaveMemory = SaveAllocMemory( sizeof(CSaveRestoreData) + tokenSize + size, sizeof(char) );
	if ( !pSaveMemory )
	{
		return 0;
	}

	pSaveData = MakeSaveRestoreData( pSaveMemory );

	pSaveData->levelInfo.connectionCount = 0;

	pszTokenList = (char *)(pSaveData + 1);

	if ( tokenSize > 0 )
	{
		if ( g_pSaveRestoreFileSystem->Read( pszTokenList, tokenSize, pFile ) != tokenSize )
		{
			Finish( pSaveData );
			return 0;
		}

		pSaveMemory = SaveAllocMemory( tokenCount, sizeof(char *), true );
		if ( !pSaveMemory )
		{
			Finish( pSaveData );
			return 0;
		}

		pSaveData->InitSymbolTable( (char**)pSaveMemory, tokenCount );

		// Make sure the token strings pointed to by the pToken hashtable.
		for( i=0; i<tokenCount; i++ )
		{
			if ( *pszTokenList )
			{
				Verify( pSaveData->DefineSymbol( pszTokenList, i ) );
			}
			while( *pszTokenList++ );				// Find next token (after next null)
		}
	}
	else
	{
		pSaveData->InitSymbolTable( NULL, 0 );
	}


	pSaveData->levelInfo.fUseLandmark = false;
	pSaveData->levelInfo.time = 0;

	// pszTokenList now points after token data
	pSaveData->Init( pszTokenList, size ); 
	if ( g_pSaveRestoreFileSystem->Read( pSaveData->GetBuffer(), size, pFile ) != size )
	{
		Finish( pSaveData );
		return 0;
	}
	
	serverGameDLL->SaveReadFields( pSaveData, "GameHeader", pHeader, NULL, GAME_HEADER::m_DataMap.dataDesc, GAME_HEADER::m_DataMap.dataNumFields );
	if ( g_szMapLoadOverride[0] )
	{
		V_strncpy( pHeader->mapName, g_szMapLoadOverride, sizeof( pHeader->mapName ) );
		g_szMapLoadOverride[0] = 0;
	}

	if ( pHeader->mapCount != 0 && pbOldSave)
		*pbOldSave = true;

	if ( readGlobalState && pHeader->mapCount == 0 ) // Alfred: Only load save games from the OB era engine where mapCount is forced to zero
	{
		serverGameDLL->RestoreGlobalState( pSaveData );
	}

	Finish( pSaveData );

	if ( pHeader->mapCount == 0 )
	{
		if ( g_pSaveRestoreFileSystem->Read( &pHeader->mapCount, sizeof(pHeader->mapCount), pFile ) != sizeof(pHeader->mapCount) )
			return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//			*output - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSaveRestore::CalcSaveGameName( const char *pName, char *output, int outputStringLength )
{
	if (!pName || !pName[0])
		return false;

	if ( IsXSave() )
	{
		Q_snprintf( output, outputStringLength, "%s:/%s", GetCurrentMod(), pName );
	}
	else
	{
		Q_snprintf( output, outputStringLength, "%s%s", GetSaveDir(), pName );
	}
	Q_DefaultExtension( output, IsX360() ? ".360.sav" : ".sav", outputStringLength );
	Q_FixSlashes( output );

	return true;
}


//-----------------------------------------------------------------------------
// Does this save file exist?
//-----------------------------------------------------------------------------
bool CSaveRestore::SaveFileExists( const char *pName )
{
	FinishAsyncSave();
	char name[256];
	if ( !CalcSaveGameName( pName, name, sizeof( name ) ) )
		return false;

	bool bExists = false;

	if ( IsXSave() )
	{
		if ( StorageDeviceValid() )
		{
			bExists = g_pFileSystem->FileExists( name );
		}
		else
		{
			bExists = g_pSaveRestoreFileSystem->FileExists( name );
		}
	}
	else
	{
		bExists = g_pFileSystem->FileExists( name );
	}

	return bExists;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
// Output : int
//-----------------------------------------------------------------------------
bool CL_HL2Demo_MapCheck( const char *name ); // in host_cmd.cpp
bool CL_PortalDemo_MapCheck( const char *name ); // in host_cmd.cpp
bool CSaveRestore::LoadGame( const char *pName )
{
	FileHandle_t	pFile;
	GAME_HEADER		gameHeader;
	char			name[ MAX_PATH ];
	bool			validload = false;

	FinishAsyncSave();
	SaveResetMemory();

	if ( !CalcSaveGameName( pName, name, sizeof( name ) ) )
	{
		DevWarning("Loaded bad game %s\n", pName);
		Assert(0);
		return false;
	}

	// store off the most recent save
	SetMostRecentSaveGame( pName );

	ConMsg( "Loading game from %s...\n", name );

	m_bClearSaveDir = false;
	DoClearSaveDir( IsXSave() );

	bool bLoadedToMemory = false;
	if ( IsX360() )
	{
		bool bValidStorageDevice = StorageDeviceValid();
		if ( bValidStorageDevice )
		{
			// Load the file into memory, whole hog
			bLoadedToMemory = g_pSaveRestoreFileSystem->LoadFileFromDisk( name );
			if ( bLoadedToMemory == false )
				return false;
		}
	}
	
	int iElapsedMinutes = 0;
	int iElapsedSeconds = 0;
	bool bOldSave = false;

	pFile = g_pSaveRestoreFileSystem->Open( name, "rb", MOD_DIR );
	if ( pFile )
	{
		char szDummyName[ MAX_PATH ];
		char szComment[ MAX_PATH ];
		char szElapsedTime[ MAX_PATH ];

		if ( SaveReadNameAndComment( pFile, szDummyName, sizeof(szDummyName), szComment, sizeof(szComment) ) )
		{
			// Elapsed time is the last 6 characters in comment. (mmm:ss)
			int i;
			i = strlen( szComment );
			Q_strncpy( szElapsedTime, "??", sizeof( szElapsedTime ) );
			if (i >= 6)
			{
				Q_strncpy( szElapsedTime, (char *)&szComment[i - 6], 7 );
				szElapsedTime[6] = '\0';

				// parse out
				iElapsedMinutes = atoi( szElapsedTime );
				iElapsedSeconds = atoi( szElapsedTime + 4);
			}
		}
		else
		{
			g_pSaveRestoreFileSystem->Close( pFile );
			if ( bLoadedToMemory )
			{
				g_pSaveRestoreFileSystem->RemoveFile( name );
			}
			return NULL;
		}

		// Reset the file pointer to the start of the file
		g_pSaveRestoreFileSystem->Seek( pFile, 0, FILESYSTEM_SEEK_HEAD );

		if ( SaveReadHeader( pFile, &gameHeader, 1, &bOldSave ) )
		{
			validload = DirectoryExtract( pFile, gameHeader.mapCount );
		}

		if ( !HaveExactMap( gameHeader.mapName ) )
		{
			Msg( "Map '%s' missing or invalid\n", gameHeader.mapName );
			validload = false;
		}

		g_pSaveRestoreFileSystem->Close( pFile );
		
		if ( bLoadedToMemory )
		{
			g_pSaveRestoreFileSystem->RemoveFile( name );
		}
	}
	else
	{
		ConMsg( "File not found or failed to open.\n" );
		return false;
	}

	if ( !validload )
	{
		Msg("Save file %s is not valid\n", name );
		return false;
	}

	// stop demo loop in case this fails
	cl.demonum = -1;		

	deathmatch.SetValue( 0 );
	coop.SetValue( 0 );

	if ( !CL_HL2Demo_MapCheck( gameHeader.mapName ) )
	{
		Warning( "Save file %s is not valid\n", name );
		return false;	
	}
	
	if ( !CL_PortalDemo_MapCheck( gameHeader.mapName ) )
	{
		Warning( "Save file %s is not valid\n", name );
		return false;	
	}

	bool bIsTransitionSave = ( gameHeader.originMapName[0] != 0 );

	bool retval = Host_NewGame( gameHeader.mapName, true, false, ( bIsTransitionSave ) ? gameHeader.originMapName : NULL, ( bIsTransitionSave ) ? gameHeader.landmark : NULL, bOldSave );

	SetMostRecentElapsedMinutes( iElapsedMinutes );
	SetMostRecentElapsedSeconds( iElapsedSeconds );

	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Remebers the most recent save game
//-----------------------------------------------------------------------------
void CSaveRestore::SetMostRecentSaveGame( const char *pSaveName )
{
	// Only remember xsaves in the x360 case
	if ( IsX360() && IsXSave() == false )
		return;

	if ( pSaveName )
	{
		Q_strncpy( m_szMostRecentSaveLoadGame, pSaveName, sizeof(m_szMostRecentSaveLoadGame) );
	}
	else
	{
		m_szMostRecentSaveLoadGame[0] = 0;
	}
	if ( !m_szMostRecentSaveLoadGame[0] )
	{
		DevWarning("Cleared most recent save!\n");
		Assert(0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the last recored elapsed minutes
//-----------------------------------------------------------------------------
int CSaveRestore::GetMostRecentElapsedMinutes( void )
{
	return m_MostRecentElapsedMinutes;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the last recored elapsed seconds
//-----------------------------------------------------------------------------
int CSaveRestore::GetMostRecentElapsedSeconds( void )
{
	return m_MostRecentElapsedSeconds;
}

int CSaveRestore::GetMostRecentElapsedTimeSet( void )
{
	return m_MostRecentElapsedTimeSet;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the last recored elapsed minutes
//-----------------------------------------------------------------------------
void CSaveRestore::SetMostRecentElapsedMinutes( const int min )
{
	m_MostRecentElapsedMinutes = min;
	m_MostRecentElapsedTimeSet = g_ServerGlobalVariables.curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the last recored elapsed seconds
//-----------------------------------------------------------------------------
void CSaveRestore::SetMostRecentElapsedSeconds( const int sec )
{
	m_MostRecentElapsedSeconds = sec;
	m_MostRecentElapsedTimeSet = g_ServerGlobalVariables.curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CSaveRestoreData
//-----------------------------------------------------------------------------

struct SaveFileHeaderTag_t
{
	int id;
	int version;
	
	bool operator==(const SaveFileHeaderTag_t &rhs) const { return ( memcmp( this, &rhs, sizeof(SaveFileHeaderTag_t) ) == 0 ); }
	bool operator!=(const SaveFileHeaderTag_t &rhs) const { return ( memcmp( this, &rhs, sizeof(SaveFileHeaderTag_t) ) != 0 ); }
};

#define MAKEID(d,c,b,a)	( ((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)) )

const struct SaveFileHeaderTag_t CURRENT_SAVEFILE_HEADER_TAG = { MAKEID('V','A','L','V'), SAVEGAME_VERSION };

struct SaveFileSectionsInfo_t
{
	int nBytesSymbols;
	int nSymbols;
	int nBytesDataHeaders;
	int nBytesData;
	
	int SumBytes() const
	{
		return ( nBytesSymbols + nBytesDataHeaders + nBytesData );
	}
};

struct SaveFileSections_t
{
	char *pSymbols;
	char *pDataHeaders;
	char *pData;
};

void CSaveRestore::SaveGameStateGlobals( CSaveRestoreData *pSaveData )
{
	SAVE_HEADER header;

	INetworkStringTable * table = sv.GetLightStyleTable();

	Assert( table );
	
	// Write global data
	header.version 			= build_number( );
	header.skillLevel 		= skill.GetInt();	// This is created from an int even though it's a float
	header.connectionCount 	= pSaveData->levelInfo.connectionCount;
	header.time__USE_VCR_MODE	= sv.GetTime();
	ConVarRef skyname( "sv_skyname" );
	if ( skyname.IsValid() )
	{
		Q_strncpy( header.skyName, skyname.GetString(), sizeof( header.skyName ) );
	}
	else
	{
		Q_strncpy( header.skyName, "unknown", sizeof( header.skyName ) );
	}

	Q_strncpy( header.mapName, sv.GetMapName(), sizeof( header.mapName ) );
	header.lightStyleCount 	= 0;
	header.mapVersion = g_ServerGlobalVariables.mapversion;

	int i;
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		const char * ligthStyle = (const char*) table->GetStringUserData( i, NULL );
		if ( ligthStyle && ligthStyle[0] )
			header.lightStyleCount++;
	}

	pSaveData->levelInfo.time = 0; // prohibits rebase of header.time (why not just save time as a field_float and ditch this hack?)
	serverGameDLL->SaveWriteFields( pSaveData, "Save Header", &header, NULL, SAVE_HEADER::m_DataMap.dataDesc, SAVE_HEADER::m_DataMap.dataNumFields );
	pSaveData->levelInfo.time = header.time__USE_VCR_MODE;

	// Write adjacency list
	for ( i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
		serverGameDLL->SaveWriteFields( pSaveData, "ADJACENCY", pSaveData->levelInfo.levelList + i, NULL, levellist_t::m_DataMap.dataDesc, levellist_t::m_DataMap.dataNumFields );

	// Write the lightstyles
	SAVELIGHTSTYLE	light;
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		const char * ligthStyle = (const char*) table->GetStringUserData( i, NULL );

		if ( ligthStyle && ligthStyle[0] )
		{
			light.index = i;
			Q_strncpy( light.style, ligthStyle, sizeof( light.style ) );
			serverGameDLL->SaveWriteFields( pSaveData, "LIGHTSTYLE", &light, NULL, SAVELIGHTSTYLE::m_DataMap.dataDesc, SAVELIGHTSTYLE::m_DataMap.dataNumFields );
		}
	}
}

CSaveRestoreData *CSaveRestore::SaveGameStateInit( void )
{
	CSaveRestoreData *pSaveData = serverGameDLL->SaveInit( 0 );
	
	return pSaveData;
}

bool CSaveRestore::SaveGameState( bool bTransition, CSaveRestoreData **ppReturnSaveData, bool bOpenContainer, bool bIsAutosaveOrDangerous )
{
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	SaveMsg( "SaveGameState...\n" );
	int i;
	SaveFileSectionsInfo_t sectionsInfo;
	SaveFileSections_t sections;

	if ( ppReturnSaveData )
	{
		*ppReturnSaveData = NULL;
	}

	if ( bTransition )
	{
		if ( m_bClearSaveDir )
		{
			m_bClearSaveDir = false;
			DoClearSaveDir( IsXSave() );
		}
	}

	S_ExtraUpdate();
	CSaveRestoreData *pSaveData = SaveGameStateInit();
	if ( !pSaveData )
	{
		return false;
	}

	pSaveData->bAsync = bIsAutosaveOrDangerous;

	//---------------------------------
	// Save the data
	sections.pData = pSaveData->AccessCurPos();
	
	//---------------------------------
	// Pre-save

	serverGameDLL->PreSave( pSaveData );
	// Build the adjacent map list (after entity table build by game in presave)
	if ( bTransition )
	{
		serverGameDLL->BuildAdjacentMapList();
	}
	else
	{
		pSaveData->levelInfo.connectionCount = 0;
	}
	S_ExtraUpdate();

	//---------------------------------

	SaveGameStateGlobals( pSaveData );

	S_ExtraUpdate();
	serverGameDLL->Save( pSaveData );
	S_ExtraUpdate();
	
	sectionsInfo.nBytesData = pSaveData->AccessCurPos() - sections.pData;

	
	//---------------------------------
	// Save necessary tables/dictionaries/directories
	sections.pDataHeaders = pSaveData->AccessCurPos();
	
	serverGameDLL->WriteSaveHeaders( pSaveData );
	
	sectionsInfo.nBytesDataHeaders = pSaveData->AccessCurPos() - sections.pDataHeaders;

	//---------------------------------
	// Write the save file symbol table
	sections.pSymbols = pSaveData->AccessCurPos();

	for( i = 0; i < pSaveData->SizeSymbolTable(); i++ )
	{
		const char *pszToken = ( pSaveData->StringFromSymbol( i ) ) ? pSaveData->StringFromSymbol( i ) : "";
		if ( !pSaveData->Write( pszToken, strlen(pszToken) + 1 ) )
		{
			break;
		}
	}	

	sectionsInfo.nBytesSymbols = pSaveData->AccessCurPos() - sections.pSymbols;
	sectionsInfo.nSymbols = pSaveData->SizeSymbolTable();

	//---------------------------------
	// Output to disk
	char name[256];
	int nBytesStateFile = sizeof(CURRENT_SAVEFILE_HEADER_TAG) + 
		sizeof(sectionsInfo) + 
		sectionsInfo.nBytesSymbols + 
		sectionsInfo.nBytesDataHeaders + 
		sectionsInfo.nBytesData;

	void *pBuffer = new byte[nBytesStateFile];
	CUtlBuffer buffer( pBuffer, nBytesStateFile );

	// Write the header -- THIS SHOULD NEVER CHANGE STRUCTURE, USE SAVE_HEADER FOR NEW HEADER INFORMATION
	// THIS IS ONLY HERE TO IDENTIFY THE FILE AND GET IT'S SIZE.

	buffer.Put( &CURRENT_SAVEFILE_HEADER_TAG, sizeof(CURRENT_SAVEFILE_HEADER_TAG) );

	// Write out the tokens and table FIRST so they are loaded in the right order, then write out the rest of the data in the file.
	buffer.Put( &sectionsInfo, sizeof(sectionsInfo) );
	buffer.Put( sections.pSymbols, sectionsInfo.nBytesSymbols );
	buffer.Put( sections.pDataHeaders, sectionsInfo.nBytesDataHeaders );
	buffer.Put( sections.pData, sectionsInfo.nBytesData );

	if ( !IsXSave() )
	{
		Q_snprintf( name, 256, "//%s/%s%s.HL1", MOD_DIR, GetSaveDir(), GetSaveGameMapName( sv.GetMapName() ) ); // DON'T FixSlashes on this, it needs to be //MOD
		SaveMsg( "Queue COM_CreatePath\n" );
		g_AsyncSaveCallQueue.QueueCall( &COM_CreatePath, CUtlEnvelope<const char *>(name) );
	}
	else
	{
		Q_snprintf( name, 256, "%s:/%s.HL1", GetCurrentMod(), GetSaveGameMapName( sv.GetMapName() ) ); // DON'T FixSlashes on this, it needs to be //MOD
	}

	S_ExtraUpdate();

	SaveMsg( "Queue AsyncWrite (%s)\n", name );
	g_AsyncSaveCallQueue.QueueCall( g_pSaveRestoreFileSystem, &ISaveRestoreFileSystem::AsyncWrite, CUtlEnvelope<const char *>(name), pBuffer, nBytesStateFile, true, false, (FSAsyncControl_t *)NULL );
	pBuffer = NULL;
	
	//---------------------------------
	
	EntityPatchWrite( pSaveData, GetSaveGameMapName( sv.GetMapName() ), true );
	if ( !ppReturnSaveData )
	{
		Finish( pSaveData );
	}
	else
	{
		*ppReturnSaveData = pSaveData;
	}

	if ( !IsXSave() )
	{
		Q_snprintf(name, sizeof( name ), "//%s/%s%s.HL2", MOD_DIR, GetSaveDir(), GetSaveGameMapName( sv.GetMapName() ) );// DON'T FixSlashes on this, it needs to be //MOD
	}
	else
	{
		Q_snprintf(name, sizeof( name ), "%s:/%s.HL2", GetCurrentMod(), GetSaveGameMapName( sv.GetMapName() ) );// DON'T FixSlashes on this, it needs to be //MOD
	}
	// Let the client see the server entity to id lookup tables, etc.
	S_ExtraUpdate();
	bool bSuccess = SaveClientState( name );
	S_ExtraUpdate();

	//---------------------------------

	if ( bTransition )
	{
		FinishAsyncSave();
	}
	S_ExtraUpdate();

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *save - 
//-----------------------------------------------------------------------------
void CSaveRestore::Finish( CSaveRestoreData *save )
{
	char **pTokens = save->DetachSymbolTable();
	if ( pTokens )
		SaveFreeMemory( pTokens );

	entitytable_t *pEntityTable = save->DetachEntityTable();
	if ( pEntityTable )
		SaveFreeMemory( pEntityTable );

	save->PurgeEntityHash();
	SaveFreeMemory( save );


	g_ServerGlobalVariables.pSaveData = NULL;
}

BEGIN_SIMPLE_DATADESC( musicsave_t )

	DEFINE_ARRAY( songname, FIELD_CHARACTER, 128 ),
	DEFINE_FIELD( sampleposition, FIELD_INTEGER ),
	DEFINE_FIELD( master_volume, FIELD_SHORT ),

END_DATADESC()

BEGIN_SIMPLE_DATADESC( decallist_t )

	DEFINE_FIELD( position, FIELD_POSITION_VECTOR ),
	DEFINE_ARRAY( name, FIELD_CHARACTER, 128 ),
	DEFINE_FIELD( entityIndex, FIELD_SHORT ),
	//	DEFINE_FIELD( depth, FIELD_CHARACTER ),
	DEFINE_FIELD( flags, FIELD_CHARACTER ),
	DEFINE_FIELD( impactPlaneNormal, FIELD_VECTOR ),

END_DATADESC()

struct baseclientsectionsold_t
{
	int entitysize;
	int headersize;
	int decalsize;
	int symbolsize;

	int decalcount;
	int symbolcount;

	int SumBytes()
	{
		return entitysize + headersize + decalsize + symbolsize;
	}
};

struct clientsectionsold_t : public baseclientsectionsold_t
{
	char	*symboldata;
	char	*entitydata;
	char	*headerdata;
	char	*decaldata;
};

// FIXME:  Remove the above and replace with this once we update the save format!!
struct baseclientsections_t
{
	int entitysize;
	int headersize;
	int decalsize;
	int musicsize;
	int symbolsize;

	int decalcount;
	int	musiccount;
	int symbolcount;

	int SumBytes()
	{
		return entitysize + headersize + decalsize + symbolsize + musicsize;
	}
};

struct clientsections_t : public baseclientsections_t
{
	char	*symboldata;
	char	*entitydata;
	char	*headerdata;
	char	*decaldata;
	char	*musicdata;
};

int CSaveRestore::LookupRestoreSpotSaveIndex( RestoreLookupTable *table, int save )
{
	int c = table->lookup.Count();
	for ( int i = 0; i < c; i++ )
	{
		SaveRestoreTranslate *slot = &table->lookup[ i ];
		if ( slot->savedindex == save )
			return slot->restoredindex;
	}
	
	return -1;
}

void CSaveRestore::ReapplyDecal( bool adjacent, RestoreLookupTable *table, decallist_t *entry )
{
	int flags = entry->flags;
	if ( adjacent )
	{
		flags |= FDECAL_DONTSAVE;
	}

	// unlock sting tables to allow changes, helps to find unwanted changes (bebug build only)
	bool oldlock = networkStringTableContainerServer->Lock( false );

	if ( adjacent )
	{
		// These entities might not exist over transitions, so we'll use the saved plane and do a traceline instead
		Vector testspot = entry->position;
		VectorMA( testspot, 5.0f, entry->impactPlaneNormal, testspot );

		Vector testend = entry->position;
		VectorMA( testend, -5.0f, entry->impactPlaneNormal, testend );

		CTraceFilterHitAll traceFilter;
		trace_t tr;
		Ray_t ray;
		ray.Init( testspot, testend );
		g_pEngineTraceServer->TraceRay( ray, MASK_OPAQUE, &traceFilter, &tr );

		if ( tr.fraction != 1.0f && !tr.allsolid )
		{
			// Check impact plane normal
			float dot = entry->impactPlaneNormal.Dot( tr.plane.normal );
			if ( dot >= 0.99 )
			{
				// Hack, have to use server traceline stuff to get at an actuall index here
				edict_t *hit = tr.GetEdict();
				if ( hit != NULL )
				{
					// Looks like a good match for original splat plane, reapply the decal
					int entityToHit = NUM_FOR_EDICT( hit );
					if ( entityToHit >= 0 )
					{
						IClientEntity *clientEntity = entitylist->GetClientEntity( entityToHit );
						if ( !clientEntity )
							return;
						
						bool found = false;
						int decalIndex = Draw_DecalIndexFromName( entry->name, &found );
						if ( !found )
						{
							// This is a serious HACK because we're grabbing the index that the server hasn't networked down to us and forcing
							//  the decal name directly.  However, the message should eventually arrive and set the decal back to the same
							//  name on the server's index...we can live with that I suppose.
							decalIndex = sv.PrecacheDecal( entry->name, RES_FATALIFMISSING );
							Draw_DecalSetName( decalIndex, entry->name );
						}

						g_pEfx->DecalShoot( 
							decalIndex, 
							entityToHit, 
							clientEntity->GetModel(), 
							clientEntity->GetAbsOrigin(), 
							clientEntity->GetAbsAngles(),
							entry->position, 0, flags );
					}
				}
			}
		}

	}
	else
	{
		int entityToHit = entry->entityIndex != 0 ? LookupRestoreSpotSaveIndex( table, entry->entityIndex ) : entry->entityIndex;
		if ( entityToHit >= 0 )
		{
			// NOTE: I re-initialized the origin and angles as the decals pos/angle are saved in local space (ie. relative to
			//       the entity they are attached to.
			Vector vecOrigin( 0.0f, 0.0f, 0.0f );
			QAngle vecAngle( 0.0f, 0.0f, 0.0f );

			const model_t *pModel = NULL;
			IClientEntity *clientEntity = entitylist->GetClientEntity( entityToHit );
			if ( clientEntity )
			{
				pModel = clientEntity->GetModel();
			}
			else
			{
				// This breaks client/server.  However, non-world entities are not in your PVS potentially.
				edict_t *pEdict = EDICT_NUM( entityToHit );
				if ( pEdict )
				{
					IServerEntity *pServerEntity = pEdict->GetIServerEntity();
					if ( pServerEntity )
					{
						pModel = sv.GetModel( pServerEntity->GetModelIndex() );						
					}
				}
			}

			if ( pModel )
			{
				bool found = false;
				int decalIndex = Draw_DecalIndexFromName( entry->name, &found );
				if ( !found )
				{
					// This is a serious HACK because we're grabbing the index that the server hasn't networked down to us and forcing
					//  the decal name directly.  However, the message should eventually arrive and set the decal back to the same
					//  name on the server's index...we can live with that I suppose.
					decalIndex = sv.PrecacheDecal( entry->name, RES_FATALIFMISSING );
					Draw_DecalSetName( decalIndex, entry->name );
				}
				
				g_pEfx->DecalShoot( decalIndex, entityToHit, pModel, vecOrigin, vecAngle, entry->position, 0, flags );
			}
		}
	}

	// unlock sting tables to allow changes, helps to find unwanted changes (bebug build only)
	networkStringTableContainerServer->Lock( oldlock );
}

void CSaveRestore::RestoreClientState( char const *fileName, bool adjacent )
{
	FileHandle_t pFile;

	pFile = g_pSaveRestoreFileSystem->Open( fileName, "rb" );
	if ( !pFile )
	{
		DevMsg( "Failed to open client state file %s\n", fileName );
		return;
	}

	SaveFileHeaderTag_t tag;
	g_pSaveRestoreFileSystem->Read( &tag, sizeof(tag), pFile );
	if ( tag != CURRENT_SAVEFILE_HEADER_TAG )
	{
		g_pSaveRestoreFileSystem->Close( pFile );
		return;
	}

	// Check for magic number
	int savePos = g_pSaveRestoreFileSystem->Tell( pFile );

	int sectionheaderversion = 1;
	int magicnumber = 0;
	baseclientsections_t sections;

	g_pSaveRestoreFileSystem->Read( &magicnumber, sizeof( magicnumber ), pFile );

	if ( magicnumber == SECTION_MAGIC_NUMBER )
	{
		g_pSaveRestoreFileSystem->Read( &sectionheaderversion, sizeof( sectionheaderversion ), pFile );

		if ( sectionheaderversion != SECTION_VERSION_NUMBER )
		{
			g_pSaveRestoreFileSystem->Close( pFile );
			return;
		}
		g_pSaveRestoreFileSystem->Read( &sections, sizeof(baseclientsections_t), pFile );
	}
	else
	{
		// Rewind
		g_pSaveRestoreFileSystem->Seek( pFile, savePos, FILESYSTEM_SEEK_HEAD );
	
		baseclientsectionsold_t oldsections;

		g_pSaveRestoreFileSystem->Read( &oldsections, sizeof(baseclientsectionsold_t), pFile );

		Q_memset( &sections, 0, sizeof( sections ) );
		sections.entitysize = oldsections.entitysize;
		sections.headersize = oldsections.headersize;
		sections.decalsize = oldsections.decalsize;
		sections.symbolsize = oldsections.symbolsize;

		sections.decalcount = oldsections.decalcount;
		sections.symbolcount = oldsections.symbolcount;
	}


	void *pSaveMemory = SaveAllocMemory( sizeof(CSaveRestoreData) + sections.SumBytes(), sizeof(char) );
	if ( !pSaveMemory )
	{
		return;
	}

	CSaveRestoreData *pSaveData = MakeSaveRestoreData( pSaveMemory );
	// Needed?
	Q_strncpy( pSaveData->levelInfo.szCurrentMapName, fileName, sizeof( pSaveData->levelInfo.szCurrentMapName ) );

	g_pSaveRestoreFileSystem->Read( (char *)(pSaveData + 1), sections.SumBytes(), pFile );
	g_pSaveRestoreFileSystem->Close( pFile );

	char *pszTokenList = (char *)(pSaveData + 1);

	if ( sections.symbolsize > 0 )
	{
		pSaveMemory = SaveAllocMemory( sections.symbolcount, sizeof(char *), true );
		if ( !pSaveMemory )
		{
			SaveFreeMemory( pSaveData );
			return;
		}

		pSaveData->InitSymbolTable( (char**)pSaveMemory, sections.symbolcount );

		// Make sure the token strings pointed to by the pToken hashtable.
		for( int i=0; i<sections.symbolcount; i++ )
		{
			if ( *pszTokenList )
			{
				Verify( pSaveData->DefineSymbol( pszTokenList, i ) );
			}
			while( *pszTokenList++ );				// Find next token (after next null)
		}
	}
	else
	{
		pSaveData->InitSymbolTable( NULL, 0 );
	}

	Assert( pszTokenList - (char *)(pSaveData + 1) == sections.symbolsize );

	//---------------------------------
	// Set up the restore basis
	int size = sections.SumBytes() - sections.symbolsize;

	pSaveData->Init( (char *)(pszTokenList), size );	// The point pszTokenList was incremented to the end of the tokens

	g_ClientDLL->ReadRestoreHeaders( pSaveData );
	
	pSaveData->Rebase();

	//HACKHACK
	pSaveData->levelInfo.time = m_flClientSaveRestoreTime;

	char name[256];
	Q_FileBase( fileName, name, sizeof( name ) );
	Q_strlower( name );

	RestoreLookupTable *table = FindOrAddRestoreLookupTable( name );

	pSaveData->levelInfo.fUseLandmark = adjacent;
	if ( adjacent )
	{
		pSaveData->levelInfo.vecLandmarkOffset = table->m_vecLandMarkOffset;
	}

	bool bFixTable = false;

	// Fixup restore indices based on what server re-created for us
	int c = pSaveData->NumEntities();
	for ( int i = 0 ; i < c; i++ )
	{
		entitytable_t *entry = pSaveData->GetEntityInfo( i );
		
		entry->restoreentityindex = LookupRestoreSpotSaveIndex( table, entry->saveentityindex );

		//Adrian: This means we are a client entity with no index to restore and we need our model precached.
		if ( entry->restoreentityindex == -1 && entry->classname != NULL_STRING && entry->modelname != NULL_STRING )
		{
			sv.PrecacheModel( STRING( entry->modelname ), RES_FATALIFMISSING | RES_PRELOAD );
			bFixTable = true;
		}
	}


	//Adrian: Fix up model string tables to make sure they match on both sides.
	if ( bFixTable == true )
	{
		int iCount = cl.m_pModelPrecacheTable->GetNumStrings();

		while ( iCount < sv.GetModelPrecacheTable()->GetNumStrings() )
		{
			string_t szString = MAKE_STRING( sv.GetModelPrecacheTable()->GetString( iCount ) );
			cl.m_pModelPrecacheTable->AddString( true, STRING( szString ) );
			iCount++;
		}
	}

	g_ClientDLL->Restore( pSaveData, false );

	if ( r_decals.GetInt() )
	{
		for ( int i = 0; i < sections.decalcount; i++ )
		{
			decallist_t entry;
			g_ClientDLL->SaveReadFields( pSaveData, "DECALLIST", &entry, NULL, decallist_t::m_DataMap.dataDesc, decallist_t::m_DataMap.dataNumFields );

			ReapplyDecal( adjacent, table, &entry );
		}
	}

	for ( int i = 0; i < sections.musiccount; i++ )
	{
		musicsave_t song;

		g_ClientDLL->SaveReadFields( pSaveData, "MUSICLIST", &song, NULL, musicsave_t::m_DataMap.dataDesc, musicsave_t::m_DataMap.dataNumFields );

		// Tell sound system to restart the music
		S_RestartSong( &song );
	}

	Finish( pSaveData );
}

void CSaveRestore::RestoreAdjacenClientState( char const *map )
{
	char name[256];
	if ( !IsXSave() )
	{
		Q_snprintf( name, sizeof( name ), "//%s/%s%s.HL2", MOD_DIR, GetSaveDir(), GetSaveGameMapName( map ) );// DON'T FixSlashes on this, it needs to be //MOD
	}
	else
	{
		Q_snprintf( name, sizeof( name ), "%s:/%s.HL2", GetCurrentMod(), GetSaveGameMapName( map ) );// DON'T FixSlashes on this, it needs to be //MOD
	}
	COM_CreatePath( name );

	RestoreClientState( name, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
bool CSaveRestore::SaveClientState( const char *name )
{
#ifndef SWDS
	decallist_t		*decalList;
	int				i;

	clientsections_t	sections;

	CSaveRestoreData *pSaveData = g_ClientDLL->SaveInit( 0 );
	if ( !pSaveData )
	{
		return false;
	}
	
	sections.entitydata = pSaveData->AccessCurPos();

	// Now write out the client .dll entities to the save file, too
	g_ClientDLL->PreSave( pSaveData );
	g_ClientDLL->Save( pSaveData );

	sections.entitysize = pSaveData->AccessCurPos() - sections.entitydata;

	sections.headerdata = pSaveData->AccessCurPos();

	g_ClientDLL->WriteSaveHeaders( pSaveData );

	sections.headersize = pSaveData->AccessCurPos() - sections.headerdata;

	sections.decaldata = pSaveData->AccessCurPos();

	decalList = (decallist_t*)malloc( sizeof(decallist_t) * Draw_DecalMax() );
	sections.decalcount = DecalListCreate( decalList );

	for ( i = 0; i < sections.decalcount; i++ )
	{
		decallist_t *entry = &decalList[ i ];

		g_ClientDLL->SaveWriteFields( pSaveData, "DECALLIST", entry, NULL, decallist_t::m_DataMap.dataDesc, decallist_t::m_DataMap.dataNumFields );
	}

	sections.decalsize = pSaveData->AccessCurPos() - sections.decaldata;

	sections.musicdata = pSaveData->AccessCurPos();

	CUtlVector< musicsave_t >	music;

	// Ask sound system for current music tracks
	S_GetCurrentlyPlayingMusic( music );

	sections.musiccount = music.Count();

	for ( i = 0; i < sections.musiccount; ++i )
	{
		musicsave_t *song = &music[ i ];

		g_ClientDLL->SaveWriteFields( pSaveData, "MUSICLIST", song, NULL, musicsave_t::m_DataMap.dataDesc, musicsave_t::m_DataMap.dataNumFields );
	}

	sections.musicsize = pSaveData->AccessCurPos() - sections.musicdata;

	// Write string token table
	sections.symboldata = pSaveData->AccessCurPos();

	for( i = 0; i < pSaveData->SizeSymbolTable(); i++ )
	{
		const char *pszToken = (pSaveData->StringFromSymbol( i )) ? pSaveData->StringFromSymbol( i ) : "";
		if ( !pSaveData->Write( pszToken, strlen(pszToken) + 1 ) )
		{
			ConMsg( "Token Table Save/Restore overflow!" );
			break;
		}
	}	

	sections.symbolcount = pSaveData->SizeSymbolTable();
	sections.symbolsize = pSaveData->AccessCurPos() - sections.symboldata;

	int magicnumber = SECTION_MAGIC_NUMBER;
	int sectionheaderversion = SECTION_VERSION_NUMBER;

	unsigned nBytes = sizeof(CURRENT_SAVEFILE_HEADER_TAG) +
						sizeof( magicnumber ) +
						sizeof( sectionheaderversion ) + 
						sizeof( baseclientsections_t ) +
						sections.symbolsize + 
						sections.headersize + 
						sections.entitysize + 
						sections.decalsize + 
						sections.musicsize;



	void *pBuffer = new byte[nBytes];
	CUtlBuffer buffer( pBuffer, nBytes );
	buffer.Put( &CURRENT_SAVEFILE_HEADER_TAG, sizeof(CURRENT_SAVEFILE_HEADER_TAG) );
	buffer.Put( &magicnumber, sizeof( magicnumber ) );
	buffer.Put( &sectionheaderversion, sizeof( sectionheaderversion ) );
	buffer.Put( (baseclientsections_t * )&sections, sizeof( baseclientsections_t ) );
	buffer.Put( sections.symboldata, sections.symbolsize );
	buffer.Put( sections.headerdata, sections.headersize );
	buffer.Put( sections.entitydata, sections.entitysize );
	buffer.Put( sections.decaldata, sections.decalsize );
	buffer.Put( sections.musicdata, sections.musicsize );

	SaveMsg( "Queue AsyncWrite (%s)\n", name );
	g_AsyncSaveCallQueue.QueueCall( g_pSaveRestoreFileSystem, &ISaveRestoreFileSystem::AsyncWrite, CUtlEnvelope<const char *>(name), pBuffer, nBytes, true, false, (FSAsyncControl_t *)NULL );

	Finish( pSaveData );

	free( decalList );
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Parses and confirms save information. Pulled from PC UI
//-----------------------------------------------------------------------------
int CSaveRestore::SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP(nameSize) char *name, int nameSize, OUT_Z_CAP(commentSize) char *comment, int commentSize )
{
	int i, tag, size, tokenSize, tokenCount;
	char *pSaveData = NULL;
	char *pFieldName = NULL;
	char **pTokenList = NULL;

	name[0] = '\0';
	comment[0] = '\0';

	// Make sure we can at least read in the first five fields
	unsigned int tagsize = sizeof(int) * 5;
	if ( g_pSaveRestoreFileSystem->Size( f ) < tagsize )
		return 0;

	int nRead = g_pSaveRestoreFileSystem->Read( &tag, sizeof(int), f );
	if ( ( nRead != sizeof(int) ) || tag != MAKEID('J','S','A','V') )
		return 0;

	if ( g_pSaveRestoreFileSystem->Read( &tag, sizeof(int), f ) != sizeof(int) )
		return 0;

	if ( g_pSaveRestoreFileSystem->Read( &size, sizeof(int), f ) != sizeof(int) )
		return 0;

	if ( g_pSaveRestoreFileSystem->Read( &tokenCount, sizeof(int), f ) != sizeof(int) )	// These two ints are the token list
		return 0;

	if ( g_pSaveRestoreFileSystem->Read( &tokenSize, sizeof(int), f ) != sizeof(int) )
		return 0;

	size += tokenSize;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > 1024 * 1024 * 32  )
	{
		return 0;
	}

	if ( tokenSize < 0 || tokenSize > 1024*1024*10  )
	{
		return 0;
	}


	pSaveData = (char *)new char[size];
	if ( g_pSaveRestoreFileSystem->Read(pSaveData, size, f) != size )
	{
		delete[] pSaveData;
		return 0;
	}

	int nNumberOfFields;

	char *pData;
	int nFieldSize;
	
	pData = pSaveData;

	// Allocate a table for the strings, and parse the table
	if ( tokenSize > 0 )
	{
		pTokenList = new char *[tokenCount];

		// Make sure the token strings pointed to by the pToken hashtable.
		for( i=0; i<tokenCount; i++ )
		{
			pTokenList[i] = *pData ? pData : NULL;	// Point to each string in the pToken table
			while( *pData++ );				// Find next token (after next null)
		}
	}
	else
		pTokenList = NULL;

	// short, short (size, index of field name)
	nFieldSize = *(short *)pData;
	pData += sizeof(short);
	pFieldName = pTokenList[ *(short *)pData ];

	if ( !pFieldName || Q_stricmp( pFieldName, "GameHeader" ) )
	{
		delete[] pSaveData;
		delete[] pTokenList;
		return 0;
	};

	// int (fieldcount)
	pData += sizeof(short);
	nNumberOfFields = *(int*)pData;
	pData += nFieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for ( i = 0; i < nNumberOfFields; ++i )
	{
		// Data order is:
		// Size
		// szName
		// Actual Data

		nFieldSize = *(short *)pData;
		pData += sizeof(short);

		pFieldName = pTokenList[ *(short *)pData ];
		pData += sizeof(short);

		if ( !Q_stricmp( pFieldName, "comment" ) )
		{
			int copySize = MAX( commentSize, nFieldSize );
			Q_strncpy( comment, pData, copySize );
		}
		else if ( !Q_stricmp( pFieldName, "mapName" ) )
		{
			int copySize = MAX( commentSize, nFieldSize );
			Q_strncpy( name, pData, copySize );
		};

		// Move to Start of next field.
		pData += nFieldSize;
	}

	// Delete the string table we allocated
	delete[] pTokenList;
	delete[] pSaveData;
	
	if ( strlen( name ) > 0 && strlen( comment ) > 0 )
		return 1;
	
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *level - 
// Output : CSaveRestoreData
//-----------------------------------------------------------------------------
CSaveRestoreData *CSaveRestore::LoadSaveData( const char *level )
{
	char			name[MAX_OSPATH];
	FileHandle_t	pFile;

	if ( !IsXSave() )
	{
		Q_snprintf( name, sizeof( name ), "//%s/%s%s.HL1", MOD_DIR, GetSaveDir(), level);// DON'T FixSlashes on this, it needs to be //MOD
	}
	else
	{
		Q_snprintf( name, sizeof( name ), "%s:/%s.HL1", GetCurrentMod(), level);// DON'T FixSlashes on this, it needs to be //MOD
	}
	ConMsg ("Loading game from %s...\n", name);

	pFile = g_pSaveRestoreFileSystem->Open( name, "rb" );
	if (!pFile)
	{
		ConMsg ("ERROR: couldn't open.\n");
		return NULL;
	}

	//---------------------------------
	// Read the header
	SaveFileHeaderTag_t tag;
	if ( g_pSaveRestoreFileSystem->Read( &tag, sizeof(tag), pFile ) != sizeof(tag) )
		return NULL;

	// Is this a valid save?
	if ( tag != CURRENT_SAVEFILE_HEADER_TAG )
		return NULL;

	//---------------------------------
	// Read the sections info and the data
	//
	SaveFileSectionsInfo_t sectionsInfo;
	
	if ( g_pSaveRestoreFileSystem->Read( &sectionsInfo, sizeof(sectionsInfo), pFile ) != sizeof(sectionsInfo) )
		return NULL;

	void *pSaveMemory = SaveAllocMemory( sizeof(CSaveRestoreData) + sectionsInfo.SumBytes(), sizeof(char) );
	if ( !pSaveMemory )
	{
		return 0;
	}

	CSaveRestoreData *pSaveData = MakeSaveRestoreData( pSaveMemory );
	Q_strncpy( pSaveData->levelInfo.szCurrentMapName, level, sizeof( pSaveData->levelInfo.szCurrentMapName ) );
	
	if ( g_pSaveRestoreFileSystem->Read( (char *)(pSaveData + 1), sectionsInfo.SumBytes(), pFile ) != sectionsInfo.SumBytes() )
	{
		// Free the memory and give up
		Finish( pSaveData );
		return NULL;
	}

	g_pSaveRestoreFileSystem->Close( pFile );
	
	//---------------------------------
	// Parse the symbol table
	char *pszTokenList = (char *)(pSaveData + 1);// Skip past the CSaveRestoreData structure

	if ( sectionsInfo.nBytesSymbols > 0 )
	{
		pSaveMemory = SaveAllocMemory( sectionsInfo.nSymbols, sizeof(char *), true );
		if ( !pSaveMemory )
		{
			SaveFreeMemory( pSaveData );
			return 0;
		}

		pSaveData->InitSymbolTable( (char**)pSaveMemory, sectionsInfo.nSymbols );

		// Make sure the token strings pointed to by the pToken hashtable.
		for( int i = 0; i<sectionsInfo.nSymbols; i++ )
		{
			if ( *pszTokenList )
			{
				Verify( pSaveData->DefineSymbol( pszTokenList, i ) );
			}
			while( *pszTokenList++ );				// Find next token (after next null)
		}
	}
	else
	{
		pSaveData->InitSymbolTable( NULL, 0 );
	}

	Assert( pszTokenList - (char *)(pSaveData + 1) == sectionsInfo.nBytesSymbols );

	//---------------------------------
	// Set up the restore basis
	int size = sectionsInfo.SumBytes() - sectionsInfo.nBytesSymbols;

	pSaveData->levelInfo.connectionCount = 0;
	pSaveData->Init( (char *)(pszTokenList), size );	// The point pszTokenList was incremented to the end of the tokens
	pSaveData->levelInfo.fUseLandmark = true;
	pSaveData->levelInfo.time = 0;
	VectorCopy( vec3_origin, pSaveData->levelInfo.vecLandmarkOffset );
	g_ServerGlobalVariables.pSaveData = (CSaveRestoreData*)pSaveData;

	return pSaveData;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSaveData - 
//			*pHeader - 
//			updateGlobals - 
//-----------------------------------------------------------------------------
void CSaveRestore::ParseSaveTables( CSaveRestoreData *pSaveData, SAVE_HEADER *pHeader, int updateGlobals )
{
	int				i;
	SAVELIGHTSTYLE	light;
	INetworkStringTable * table = sv.GetLightStyleTable();
	
	// Re-base the savedata since we re-ordered the entity/table / restore fields
	pSaveData->Rebase();
	// Process SAVE_HEADER
	serverGameDLL->SaveReadFields( pSaveData, "Save Header", pHeader, NULL, SAVE_HEADER::m_DataMap.dataDesc, SAVE_HEADER::m_DataMap.dataNumFields );
//	header.version = ENGINE_VERSION;

	pSaveData->levelInfo.mapVersion = pHeader->mapVersion;
	pSaveData->levelInfo.connectionCount = pHeader->connectionCount;
	pSaveData->levelInfo.time = pHeader->time__USE_VCR_MODE;
	pSaveData->levelInfo.fUseLandmark = true;
	VectorCopy( vec3_origin, pSaveData->levelInfo.vecLandmarkOffset );

	// Read adjacency list
	for ( i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
		serverGameDLL->SaveReadFields( pSaveData, "ADJACENCY", pSaveData->levelInfo.levelList + i, NULL, levellist_t::m_DataMap.dataDesc, levellist_t::m_DataMap.dataNumFields );
	
	if ( updateGlobals )
  	{
  		for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
  			table->SetStringUserData( i, 1, "" );
  	}


	for ( i = 0; i < pHeader->lightStyleCount; i++ )
	{
		serverGameDLL->SaveReadFields( pSaveData, "LIGHTSTYLE", &light, NULL, SAVELIGHTSTYLE::m_DataMap.dataDesc, SAVELIGHTSTYLE::m_DataMap.dataNumFields );
		if ( updateGlobals )
		{
			table->SetStringUserData( light.index, Q_strlen(light.style)+1, light.style );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Write out the list of entities that are no longer in the save file for this level
//  (they've been moved to another level)
// Input  : *pSaveData - 
//			*level - 
//-----------------------------------------------------------------------------
void CSaveRestore::EntityPatchWrite( CSaveRestoreData *pSaveData, const char *level, bool bAsync )
{
	char			name[MAX_OSPATH];
	int				i, size;

	if ( !IsXSave() )
	{
		Q_snprintf( name, sizeof( name ), "//%s/%s%s.HL3", MOD_DIR, GetSaveDir(), level);// DON'T FixSlashes on this, it needs to be //MOD
	}
	else
	{
		Q_snprintf( name, sizeof( name ), "%s:/%s.HL3", GetCurrentMod(), level);// DON'T FixSlashes on this, it needs to be //MOD
	}

	size = 0;
	for ( i = 0; i < pSaveData->NumEntities(); i++ )
	{
		if ( pSaveData->GetEntityInfo(i)->flags & FENTTABLE_REMOVED )
			size++;
	}

	int nBytesEntityPatch = sizeof(int) + size * sizeof(int);
	void *pBuffer = new byte[nBytesEntityPatch];
	CUtlBuffer buffer( pBuffer, nBytesEntityPatch );

	// Patch count
	buffer.Put( &size, sizeof(int) );
	for ( i = 0; i < pSaveData->NumEntities(); i++ )
	{
		if ( pSaveData->GetEntityInfo(i)->flags & FENTTABLE_REMOVED )
			buffer.Put( &i, sizeof(int) );
	}


	if ( !bAsync )
	{
		g_pSaveRestoreFileSystem->AsyncWrite( name, pBuffer, nBytesEntityPatch, true, false );
		g_pSaveRestoreFileSystem->AsyncFinishAllWrites();
	}
	else
	{
		SaveMsg( "Queue AsyncWrite (%s)\n", name );
		g_AsyncSaveCallQueue.QueueCall( g_pSaveRestoreFileSystem, &ISaveRestoreFileSystem::AsyncWrite, CUtlEnvelope<const char *>(name), pBuffer, nBytesEntityPatch, true, false, (FSAsyncControl_t *)NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read the list of entities that are no longer in the save file for this level (they've been moved to another level)
//   and correct the table
// Input  : *pSaveData - 
//			*level - 
//-----------------------------------------------------------------------------
void CSaveRestore::EntityPatchRead( CSaveRestoreData *pSaveData, const char *level )
{
	char			name[MAX_OSPATH];
	FileHandle_t	pFile;
	int				i, size, entityId;

	if ( !IsXSave() )
	{
		Q_snprintf(name, sizeof( name ), "//%s/%s%s.HL3", MOD_DIR, GetSaveDir(), GetSaveGameMapName( level ) );// DON'T FixSlashes on this, it needs to be //MOD
	}
	else
	{
		Q_snprintf(name, sizeof( name ), "%s:/%s.HL3", GetCurrentMod(), GetSaveGameMapName( level ) );// DON'T FixSlashes on this, it needs to be //MOD
	}

	pFile = g_pSaveRestoreFileSystem->Open( name, "rb" );
	if ( pFile )
	{
		// Patch count
		g_pSaveRestoreFileSystem->Read( &size, sizeof(int), pFile );
		for ( i = 0; i < size; i++ )
		{
			g_pSaveRestoreFileSystem->Read( &entityId, sizeof(int), pFile );
			pSaveData->GetEntityInfo(entityId)->flags = FENTTABLE_REMOVED;
		}
		g_pSaveRestoreFileSystem->Close( pFile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *level - 
//			createPlayers - 
// Output : int
//-----------------------------------------------------------------------------
int CSaveRestore::LoadGameState( char const *level, bool createPlayers )
{
	VPROF("CSaveRestore::LoadGameState");

	SAVE_HEADER		header;
	CSaveRestoreData *pSaveData;
	pSaveData = LoadSaveData( GetSaveGameMapName( level ) );
	if ( !pSaveData )		// Couldn't load the file
		return 0;

	serverGameDLL->ReadRestoreHeaders( pSaveData );

	ParseSaveTables( pSaveData, &header, 1 );
	EntityPatchRead( pSaveData, level );
	
	if ( !IsX360() )
	{
		skill.SetValue( header.skillLevel );
	}

	Q_strncpy( sv.m_szMapname, header.mapName, sizeof( sv.m_szMapname ) );
	ConVarRef skyname( "sv_skyname" );
	if ( skyname.IsValid() )
	{
		skyname.SetValue( header.skyName );
	}
	
	// Create entity list
	serverGameDLL->Restore( pSaveData, createPlayers );

	BuildRestoredIndexTranslationTable( level, pSaveData, false );

	m_flClientSaveRestoreTime = pSaveData->levelInfo.time;

	Finish( pSaveData );

	sv.m_nTickCount = (int)( header.time__USE_VCR_MODE / host_state.interval_per_tick );
	// SUCCESS!
	return 1;
}

CSaveRestore::RestoreLookupTable *CSaveRestore::FindOrAddRestoreLookupTable( char const *mapname )
{
	int idx = m_RestoreLookup.Find( mapname );
	if ( idx == m_RestoreLookup.InvalidIndex() )
	{
		idx = m_RestoreLookup.Insert( mapname );
	}
	return &m_RestoreLookup[ idx ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSaveData - 
// Output : int
//-----------------------------------------------------------------------------
void CSaveRestore::BuildRestoredIndexTranslationTable( char const *mapname, CSaveRestoreData *pSaveData, bool verbose )
{
	char name[ 256 ];
	Q_FileBase( mapname, name, sizeof( name ) );
	Q_strlower( name );

	// Build Translation Lookup
	RestoreLookupTable *table = FindOrAddRestoreLookupTable( name );
	table->Clear();

	int c = pSaveData->NumEntities();
	for ( int i = 0; i < c; i++ )
	{
		entitytable_t *entry = pSaveData->GetEntityInfo( i );
		SaveRestoreTranslate slot;

		slot.classname		= entry->classname;
		slot.savedindex		= entry->saveentityindex;
		slot.restoredindex	= entry->restoreentityindex;

		table->lookup.AddToTail( slot );
	}

	table->m_vecLandMarkOffset = pSaveData->levelInfo.vecLandmarkOffset;
}

void CSaveRestore::ClearRestoredIndexTranslationTables()
{
	m_RestoreLookup.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Find all occurances of the map in the adjacency table
// Input  : *pSaveData - 
//			*pMapName - 
//			index - 
// Output : int
//-----------------------------------------------------------------------------
int EntryInTable( CSaveRestoreData *pSaveData, const char *pMapName, int index )
{
	int i;

	index++;
	for ( i = index; i < pSaveData->levelInfo.connectionCount; i++ )
	{
		if ( !stricmp( pSaveData->levelInfo.levelList[i].mapName, pMapName ) )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSaveData - 
//			output - 
//			*pLandmarkName - 
//-----------------------------------------------------------------------------
void LandmarkOrigin( CSaveRestoreData *pSaveData, Vector& output, const char *pLandmarkName )
{
	int i;

	for ( i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
	{
		if ( !stricmp( pSaveData->levelInfo.levelList[i].landmarkName, pLandmarkName ) )
		{
			VectorCopy( pSaveData->levelInfo.levelList[i].vecLandmarkOrigin, output );
			return;
		}
	}

	VectorCopy( vec3_origin, output );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOldLevel - 
//			*pLandmarkName - 
//-----------------------------------------------------------------------------
void CSaveRestore::LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName )
{
	FinishAsyncSave();

	CSaveRestoreData currentLevelData, *pSaveData;
	int				i, test, flags, index, movedCount = 0;
	SAVE_HEADER		header;
	Vector			landmarkOrigin;

	memset( &currentLevelData, 0, sizeof(CSaveRestoreData) );
	g_ServerGlobalVariables.pSaveData = &currentLevelData;
	// Build the adjacent map list
	serverGameDLL->BuildAdjacentMapList();
	bool foundprevious = false;

	for ( i = 0; i < currentLevelData.levelInfo.connectionCount; i++ )
	{
		// make sure the previous level is in the connection list so we can
		// bring over the player.
		if ( !strcmpi( currentLevelData.levelInfo.levelList[i].mapName, pOldLevel ) )
		{
			foundprevious = true;
		}

		for ( test = 0; test < i; test++ )
		{
			// Only do maps once
			if ( !stricmp( currentLevelData.levelInfo.levelList[i].mapName, currentLevelData.levelInfo.levelList[test].mapName ) )
				break;
		}
		// Map was already in the list
		if ( test < i )
			continue;

//		ConMsg("Merging entities from %s ( at %s )\n", currentLevelData.levelInfo.levelList[i].mapName, currentLevelData.levelInfo.levelList[i].landmarkName );
		pSaveData = LoadSaveData( GetSaveGameMapName( currentLevelData.levelInfo.levelList[i].mapName ) );

		if ( pSaveData )
		{
			serverGameDLL->ReadRestoreHeaders( pSaveData );

			ParseSaveTables( pSaveData, &header, 0 );
			EntityPatchRead( pSaveData, currentLevelData.levelInfo.levelList[i].mapName );
			pSaveData->levelInfo.time = sv.GetTime();// - header.time;
			pSaveData->levelInfo.fUseLandmark = true;
			flags = 0;
			LandmarkOrigin( &currentLevelData, landmarkOrigin, pLandmarkName );
			LandmarkOrigin( pSaveData, pSaveData->levelInfo.vecLandmarkOffset, pLandmarkName );
			VectorSubtract( landmarkOrigin, pSaveData->levelInfo.vecLandmarkOffset, pSaveData->levelInfo.vecLandmarkOffset );
			if ( !stricmp( currentLevelData.levelInfo.levelList[i].mapName, pOldLevel ) )
				flags |= FENTTABLE_PLAYER;

			index = -1;
			while ( 1 )
			{
				index = EntryInTable( pSaveData, sv.GetMapName(), index );
				if ( index < 0 )
					break;
				flags |= 1<<index;
			}
			
			if ( flags )
				movedCount = serverGameDLL->CreateEntityTransitionList( pSaveData, flags );

			// If ents were moved, rewrite entity table to save file
			if ( movedCount )
				EntityPatchWrite( pSaveData, GetSaveGameMapName( currentLevelData.levelInfo.levelList[i].mapName ) );

			BuildRestoredIndexTranslationTable( currentLevelData.levelInfo.levelList[i].mapName, pSaveData, true );

			Finish( pSaveData );
		}
	}
	g_ServerGlobalVariables.pSaveData = NULL;
	if ( !foundprevious )
	{
		// Host_Error( "Level transition ERROR\nCan't find connection to %s from %s\n", pOldLevel, sv.GetMapName() );
		Warning( "Level transition ERROR\nCan't find connection to %s from %s\n", pOldLevel, sv.GetMapName() );
		Cbuf_AddText( "disconnect\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : int
//-----------------------------------------------------------------------------
int CSaveRestore::FileSize( FileHandle_t pFile )
{
	if ( !pFile )
		return 0;

	return g_pSaveRestoreFileSystem->Size(pFile);
}

//-----------------------------------------------------------------------------
// Purpose: Copies the contents of the save directory into a single file
//-----------------------------------------------------------------------------
void CSaveRestore::DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave )
{
	SaveMsg( "Directory copy (%s)\n", pPath );

	g_pSaveRestoreFileSystem->AsyncFinishAllWrites();
	int nMaps = g_pSaveRestoreFileSystem->DirectoryCount( pPath );
	FileHandle_t hFile = g_pSaveRestoreFileSystem->Open( pDestFileName, "ab+" );
	if ( hFile )
	{
		g_pSaveRestoreFileSystem->Write( &nMaps, sizeof(nMaps), hFile );
		g_pSaveRestoreFileSystem->Close( hFile );
		g_pSaveRestoreFileSystem->DirectoryCopy( pPath, pDestFileName, bIsXSave );
	}
	else
	{
		Warning( "Invalid save, failed to open file\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Extracts all the files contained within pFile
//-----------------------------------------------------------------------------
bool CSaveRestore::DirectoryExtract( FileHandle_t pFile, int fileCount )
{
	return g_pSaveRestoreFileSystem->DirectoryExtract( pFile, fileCount, IsXSave() );
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of save files in the specified filter
//-----------------------------------------------------------------------------
void CSaveRestore::DirectoryCount( const char *pPath, int *pResult )
{
	LOCAL_THREAD_LOCK();
	if ( *pResult == -1 )
		*pResult = g_pSaveRestoreFileSystem->DirectoryCount( pPath );
	// else already set by worker thread
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPath - 
//-----------------------------------------------------------------------------
void CSaveRestore::DirectoryClear( const char *pPath )
{
	g_pSaveRestoreFileSystem->DirectoryClear( pPath, IsXSave() );
}


//-----------------------------------------------------------------------------
// Purpose: deletes all the partial save files from the save game directory
//-----------------------------------------------------------------------------
void CSaveRestore::ClearSaveDir( void )
{
	m_bClearSaveDir = true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSaveRestore::DoClearSaveDir( bool bIsXSave )
{
	// before we clear the save dir, we need to make sure that 
	// any async-written save games have finished writing, 
	// since we still may need these temp files to write the save game

	char szName[MAX_OSPATH];

	if ( !bIsXSave )
	{
		Q_snprintf(szName, sizeof( szName ), "%s", GetSaveDir() );
		Q_FixSlashes( szName );
		// Create save directory if it doesn't exist
		Sys_mkdir( szName );
	}
	else
	{
		Q_snprintf( szName, sizeof( szName ), "%s:\\", GetCurrentMod() );
	}

	Q_strncat( szName, "*.HL?", sizeof( szName ), COPY_ALL_CHARACTERS );
	DirectoryClear( szName );
}

void CSaveRestore::RequestClearSaveDir( void )
{
	m_bClearSaveDir = true;
}

void CSaveRestore::OnFinishedClientRestore()
{
	g_ClientDLL->DispatchOnRestore();

	ClearRestoredIndexTranslationTables();

	if ( m_bClearSaveDir )
	{
		m_bClearSaveDir = false;
		FinishAsyncSave();
		DoClearSaveDir( IsXSave() );
	}
}

void CSaveRestore::AutoSaveDangerousIsSafe()
{
	if ( save_async.GetBool() && ThreadInMainThread() && g_pSaveThread )
	{
		g_pSaveThread->QueueCall(  this, &CSaveRestore::FinishAsyncSave );

		g_pSaveThread->QueueCall(  this, &CSaveRestore::AutoSaveDangerousIsSafe );

		return;
	}

	if ( !m_bWaitingForSafeDangerousSave )
		return;

	m_bWaitingForSafeDangerousSave = false;

	ConDMsg( "Committing autosavedangerous...\n" );

	char szOldName[MAX_PATH];
	char szNewName[MAX_PATH];

	// Back up the old autosaves
	if ( StorageDeviceValid() )
	{
		AgeSaveList( "autosave", save_history_count.GetInt(), IsXSave() );
	}

	// Rename the screenshot
	if ( !IsX360() )
	{
		Q_snprintf( szOldName, sizeof( szOldName ), "//%s/%sautosavedangerous%s.tga", MOD_DIR, GetSaveDir(), GetPlatformExt() );
		Q_snprintf( szNewName, sizeof( szNewName ), "//%s/%sautosave%s.tga", MOD_DIR, GetSaveDir(), GetPlatformExt() );

		// there could be an old version, remove it
		if ( g_pFileSystem->FileExists( szNewName ) )
		{
			g_pFileSystem->RemoveFile( szNewName );
		}

		if ( g_pFileSystem->FileExists( szOldName ) )
		{
			if ( !g_pFileSystem->RenameFile( szOldName, szNewName ) )
			{
				SetMostRecentSaveGame( "autosavedangerous" );
				return;
			}
		}
	}

	// Rename the dangerous auto save as a normal auto save
	if ( !IsXSave() )
	{
		Q_snprintf( szOldName, sizeof( szOldName ), "//%s/%sautosavedangerous%s.sav", MOD_DIR, GetSaveDir(), GetPlatformExt() );
		Q_snprintf( szNewName, sizeof( szNewName ), "//%s/%sautosave%s.sav", MOD_DIR, GetSaveDir(), GetPlatformExt() );
	}
	else
	{
		Q_snprintf( szOldName, sizeof( szOldName ), "%s:\\autosavedangerous%s.sav", GetCurrentMod(), GetPlatformExt() );
		Q_snprintf( szNewName, sizeof( szNewName ), "%s:\\autosave%s.sav", GetCurrentMod(), GetPlatformExt() );
	}

	// there could be an old version, remove it
	if ( g_pFileSystem->FileExists( szNewName ) )
	{
		g_pFileSystem->RemoveFile( szNewName );
	}

	if ( !g_pFileSystem->RenameFile( szOldName, szNewName ) )
	{
		SetMostRecentSaveGame( "autosavedangerous" );
		return;
	}

	// Use this as the most recent now that it's safe
	SetMostRecentSaveGame( "autosave" );

	// Finish off all writes
	if ( IsXSave() )
	{
		g_pXboxSystem->FinishContainerWrites();
	}
}

static void SaveGame( const CCommand &args )
{
	bool bFinishAsync = false;
	bool bSetMostRecent = true;
	bool bRenameMap = false;
	if ( args.ArgC() > 2 )
	{
		for ( int i = 2; i < args.ArgC(); i++ )
		{
			if ( !Q_stricmp( args[i], "wait" ) )
			{
				bFinishAsync = true;
			}
			else if ( !Q_stricmp(args[i], "notmostrecent"))
			{
				bSetMostRecent = false;
			}
			else if ( !Q_stricmp( args[i], "copymap" ) )
			{
				bRenameMap = true;
			}
		}
	}

	char szMapName[MAX_PATH];
	if ( bRenameMap )
	{
		// HACK: The bug is going to make a copy of this map, so replace the global state to
		// fool the system
		Q_strncpy( szMapName, sv.m_szMapname, sizeof(szMapName) );
		Q_strncpy( sv.m_szMapname, args[1], sizeof(sv.m_szMapname) );
	}

	int iAdditionalSeconds = g_ServerGlobalVariables.curtime - saverestore->GetMostRecentElapsedTimeSet();
	int iAdditionalMinutes = iAdditionalSeconds / 60;
	iAdditionalSeconds -= iAdditionalMinutes * 60;

	char comment[80];
	GetServerSaveCommentEx( 
		comment, 
		sizeof( comment ), 
		saverestore->GetMostRecentElapsedMinutes() + iAdditionalMinutes,
		saverestore->GetMostRecentElapsedSeconds() + iAdditionalSeconds );

	saverestore->SaveGameSlot( args[1], comment, false, bSetMostRecent );

	if ( bFinishAsync )
	{
		FinishAsyncSave();
	}

	if ( bRenameMap )
	{
		// HACK: Put the original name back
		Q_strncpy( sv.m_szMapname, szMapName, sizeof(sv.m_szMapname) );
	}

#if !defined (SWDS)
	CL_HudMessage( IsX360() ? "GAMESAVED_360" : "GAMESAVED" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_Savegame_f
//-----------------------------------------------------------------------------
CON_COMMAND_F( save, "Saves current game.", FCVAR_DONTRECORD )
{
	// Can we save at this point?
	if ( !saverestore->IsValidSave() )
		return;

	if ( args.ArgC() < 2 )
	{
		ConDMsg("save <savename> [wait]: save a game\n");
		return;
	}

	if ( strstr(args[1], ".." ) )
	{
		ConDMsg ("Relative pathnames are not allowed.\n");
		return;
	}

	if ( strstr(sv.m_szMapname, "background" ) )
	{
		ConDMsg ("\"background\" is a reserved map name and cannot be saved or loaded.\n");
		return;
	}

	g_SaveRestore.SetIsXSave( false );
	SaveGame( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_Savegame_f
//-----------------------------------------------------------------------------
CON_COMMAND_F( xsave, "Saves current game to a 360 storage device.", FCVAR_DONTRECORD )
{
	// Can we save at this point?
	if ( !saverestore->IsValidSave() )
		return;

	if ( args.ArgC() < 2 )
	{
		ConDMsg("save <savename> [wait]: save a game\n");
		return;
	}

	if ( strstr(args[1], ".." ) )
	{
		ConDMsg ("Relative pathnames are not allowed.\n");
		return;
	}

	if ( strstr(sv.m_szMapname, "background" ) )
	{
		ConDMsg ("\"background\" is a reserved map name and cannot be saved or loaded.\n");
		return;
	}

	g_SaveRestore.SetIsXSave( IsX360() );
	SaveGame( args );
}

//-----------------------------------------------------------------------------
// Purpose: saves the game, but only includes the state for the current level
//			useful for bug reporting.
// Output : 
//-----------------------------------------------------------------------------
CON_COMMAND_F( minisave, "Saves game (for current level only!)", FCVAR_DONTRECORD )
{
	// Can we save at this point?
	if ( !saverestore->IsValidSave() )
		return;

	if (args.ArgC() != 2 || strstr(args[1], ".."))
		return;

	int iAdditionalSeconds = g_ServerGlobalVariables.curtime - saverestore->GetMostRecentElapsedTimeSet();
	int iAdditionalMinutes = iAdditionalSeconds / 60;
	iAdditionalSeconds -= iAdditionalMinutes * 60;

	char comment[80];
	GetServerSaveCommentEx( 
		comment, 
		sizeof( comment ),
		saverestore->GetMostRecentElapsedMinutes() + iAdditionalMinutes,
		saverestore->GetMostRecentElapsedSeconds() + iAdditionalSeconds );
	saverestore->SaveGameSlot( args[1], comment, true, true );
}

static void AutoSave_Silent( bool bDangerous )
{
	// Can we save at this point?
	if ( !saverestore->IsValidSave() )
		return;

	int iAdditionalSeconds = g_ServerGlobalVariables.curtime - saverestore->GetMostRecentElapsedTimeSet();
	int iAdditionalMinutes = iAdditionalSeconds / 60;
	iAdditionalSeconds -= iAdditionalMinutes * 60;

	char comment[80];
	GetServerSaveCommentEx( 
		comment, 
		sizeof( comment ),
		saverestore->GetMostRecentElapsedMinutes() + iAdditionalMinutes,
		saverestore->GetMostRecentElapsedSeconds() + iAdditionalSeconds );

	g_SaveRestore.SetIsXSave( IsX360() );
	if ( !bDangerous )
	{
		saverestore->SaveGameSlot( "autosave", comment, false, true );
	}
	else
	{
		saverestore->SaveGameSlot( "autosavedangerous", comment, false, false );
	}
}

static ConVar save_console( "save_console", "0", 0, "Autosave on the PC behaves like it does on the consoles." );
static ConVar save_huddelayframes( "save_huddelayframes", "1", 0, "Number of frames to defer for drawing the Saving message." );

CON_COMMAND( _autosave, "Autosave" )
{
	AutoSave_Silent( false );
	bool bConsole = save_console.GetBool();
#if defined ( _X360 )
	bConsole = true;
#endif
	if ( bConsole )
	{
#if !defined (SWDS)
		CL_HudMessage( IsX360() ? "GAMESAVED_360" : "GAMESAVED" );
#endif
	}
}

CON_COMMAND( _autosavedangerous, "AutoSaveDangerous" )
{
	// Don't even bother if we've got an invalid save
	if ( saverestore->StorageDeviceValid() == false )
		return;

	AutoSave_Silent( true );
	bool bConsole = save_console.GetBool();
#if defined ( _X360 )
	bConsole = true;
#endif
	if ( bConsole )
	{
#if !defined (SWDS)
		CL_HudMessage( IsX360() ? "GAMESAVED_360" : "GAMESAVED" );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_AutoSave_f
//-----------------------------------------------------------------------------
CON_COMMAND( autosave, "Autosave" )
{
	// Can we save at this point?
	if ( !saverestore->IsValidSave() || !sv_autosave.GetBool() )
		return;

	bool bConsole = save_console.GetBool();
	char const *pchSaving = IsX360() ? "GAMESAVING_360" : "GAMESAVING";
#if defined ( _X360 )
	bConsole = true;
#endif

	if ( bConsole )
	{
#if !defined (SWDS)
		CL_HudMessage( pchSaving );
#endif
		g_SaveRestore.AddDeferredCommand( "_autosave" );
	}
	else
	{
		AutoSave_Silent( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_AutoSaveDangerous_f
//-----------------------------------------------------------------------------
CON_COMMAND( autosavedangerous, "AutoSaveDangerous" )
{
	// Can we save at this point?
	if ( !saverestore->IsValidSave() || !sv_autosave.GetBool() )
		return;

	// Don't even bother if we've got an invalid save
	if ( saverestore->StorageDeviceValid() == false )
		return;

	//Don't print out "SAVED" unless we're running on an Xbox (in which case it prints "CHECKPOINT").
	bool bConsole = save_console.GetBool();
	char const *pchSaving = IsX360() ? "GAMESAVING_360" : "GAMESAVING";
#if defined ( _X360 )
	bConsole = true;
#endif

	if ( bConsole )
	{
#if !defined (SWDS)
		CL_HudMessage( pchSaving );
#endif
		g_SaveRestore.AddDeferredCommand( "_autosavedangerous" );
	}
	else
	{
		AutoSave_Silent( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_AutoSaveSafe_f
//-----------------------------------------------------------------------------
CON_COMMAND( autosavedangerousissafe, "" )
{
	saverestore->AutoSaveDangerousIsSafe();	
}

//-----------------------------------------------------------------------------
// Purpose: Load a save game in response to a console command (load or xload)
//-----------------------------------------------------------------------------
static void LoadSaveGame( const char *savename )
{
	// Make sure the freaking save file exists....
	if ( !saverestore->SaveFileExists( savename ) )
	{
		Warning( "Can't load '%s', file missing!\n", savename );
		return;
	}

	GetTestScriptMgr()->SetWaitCheckPoint( "load_game" );

	// if we're not currently in a game, show progress
	if ( !sv.IsActive() || sv.IsLevelMainMenuBackground() )
	{
		EngineVGui()->EnabledProgressBarForNextLoad();
	}

	// Put up loading plaque
	SCR_BeginLoadingPlaque();

	Host_Disconnect( false );	// stop old game

	HostState_LoadGame( savename, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_Loadgame_f
//-----------------------------------------------------------------------------
void Host_Loadgame_f( const CCommand &args )
{
	if ( cmd_source != src_command )
		return;

	if ( sv.IsMultiplayer() )
	{
		ConMsg ("Can't load in multiplayer games.\n");
		return;
	}

	if (args.ArgC() < 2)
	{
		ConMsg ("load <savename> : load a game\n");
		return;
	}

	g_szMapLoadOverride[0] = 0;

	if ( args.ArgC() > 2)
	{
		V_strncpy( g_szMapLoadOverride, args[2], sizeof( g_szMapLoadOverride ) );
	}

	g_SaveRestore.SetIsXSave( false );
	LoadSaveGame( args[1] );
}

// Always loads saves from DEFAULT_WRITE_PATH, regardless of platform
CON_COMMAND_AUTOCOMPLETEFILE( load, Host_Loadgame_f, "Load a saved game.", "save", sav );

// Loads saves from the 360 storage device
CON_COMMAND( xload, "Load a saved game from a 360 storage device." )
{
	if ( sv.IsMultiplayer() )
	{
		ConMsg ("Can't load in multiplayer games.\n");
		return;
	}
	if (args.ArgC() != 2)
	{
		ConMsg ("xload <savename>\n");
		return;
	}

	g_SaveRestore.SetIsXSave( IsX360() );
	LoadSaveGame( args[1] );
}

CON_COMMAND( save_finish_async, "" )
{
	FinishAsyncSave();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSaveRestore::Init( void )
{
	int minplayers = 1;
	// serverGameClients should have been initialized by the CModAppSystemGroup Create method (so it's before the Host_Init stuff which calls this)
	Assert( serverGameClients );
	if ( serverGameClients )
	{
		int dummy = 1;
		int dummy2 = 1;
		serverGameClients->GetPlayerLimits( minplayers, dummy, dummy2 );
	}

	if ( !serverGameClients || 
		( minplayers == 1 ) )
	{
		GetSaveMemory();

		Assert( !g_pSaveThread );

		ThreadPoolStartParams_t threadPoolStartParams;
		threadPoolStartParams.nThreads = 1;
		if ( !IsX360() )
		{
			threadPoolStartParams.fDistribute = TRS_FALSE;
		}
		else
		{
			threadPoolStartParams.iAffinityTable[0] = XBOX_PROCESSOR_1;
			threadPoolStartParams.bUseAffinityTable = true;
		}

		g_pSaveThread = CreateThreadPool();
		g_pSaveThread->Start( threadPoolStartParams, "SaveJob" );
	}

	m_nDeferredCommandFrames = 0;
	m_szSaveGameScreenshotFile[0] = 0;
	if ( !IsX360() && !CommandLine()->FindParm( "-noclearsave" ) )
	{
		ClearSaveDir();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSaveRestore::Shutdown( void )
{
	FinishAsyncSave();
	if ( g_pSaveThread )
	{
		g_pSaveThread->Stop();
		g_pSaveThread->Release();
		g_pSaveThread = NULL;
	}
	m_szSaveGameScreenshotFile[0] = 0;
}

char const *CSaveRestore::GetMostRecentlyLoadedFileName()
{
	return m_szMostRecentSaveLoadGame;
}

char const *CSaveRestore::GetSaveFileName()
{
	return m_szSaveGameName;
}

void CSaveRestore::AddDeferredCommand( char const *pchCommand )
{
	m_nDeferredCommandFrames = clamp( save_huddelayframes.GetInt(), 0, 10 );
	CUtlSymbol sym;
	sym = pchCommand;
	m_sDeferredCommands.AddToTail( sym );
}

void CSaveRestore::OnFrameRendered()
{
	if ( m_nDeferredCommandFrames > 0 )
	{
		--m_nDeferredCommandFrames;
		if ( m_nDeferredCommandFrames == 0 )
		{
			// Dispatch deferred command
			for ( int i = 0; i < m_sDeferredCommands.Count(); ++i )
			{
				Cbuf_AddText( m_sDeferredCommands[ i ].String() );
			}
			m_sDeferredCommands.Purge();
		}
	}
}

bool CSaveRestore::StorageDeviceValid( void )
{
	// PC is always valid
	if ( !IsX360() )
		return true;

	// Non-XSaves are always valid
	if ( !IsXSave() )
		return true;

#ifdef _X360
	// Otherwise, we must have a real storage device
	int nStorageDeviceID = XBX_GetStorageDeviceId();
	return ( nStorageDeviceID != XBX_INVALID_STORAGE_ID && nStorageDeviceID != XBX_STORAGE_DECLINED );
#endif

	return true;
}

bool CSaveRestore::IsSaveInProgress()
{
	return g_bSaveInProgress;
}

