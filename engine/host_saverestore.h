//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( HOST_SAVERESTORE_H )
#define HOST_SAVERESTORE_H
#ifdef _WIN32
#pragma once
#endif

class CSaveRestoreData;

abstract_class ISaveRestore
{
public:
	virtual void					Init( void ) = 0;
	virtual void					Shutdown( void ) = 0;
	virtual void					OnFrameRendered() = 0;
	virtual bool					SaveFileExists( const char *pName ) = 0;
	virtual bool					LoadGame( const char *pName ) = 0;
	virtual char					*GetSaveDir(void) = 0;
	virtual void					ClearSaveDir( void ) = 0;
	virtual void					RequestClearSaveDir( void ) = 0;
	virtual int						LoadGameState( char const *level, bool createPlayers ) = 0;
	virtual void					LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName ) = 0;
	virtual const char				*FindRecentSave( char *pNameBuf, int nameBufLen ) = 0;
	virtual void					ForgetRecentSave() = 0;
	virtual int						SaveGameSlot( const char *pSaveName, const char *pSaveComment, bool onlyThisLevel = false, bool bSetMostRecent = true, const char *pszDestMap = NULL, const char *pszLandmark = NULL ) = 0;
	virtual bool					SaveGameState( bool bTransition, CSaveRestoreData ** = NULL, bool bOpenContainer = true, bool bIsAutosaveOrDangerous = false ) = 0;
	virtual int						IsValidSave( void ) = 0;
	virtual void					Finish( CSaveRestoreData *save ) = 0;

	virtual void					RestoreClientState( char const *fileName, bool adjacent ) = 0;
	virtual void					RestoreAdjacenClientState( char const *map ) = 0;
	virtual int						SaveReadNameAndComment( FileHandle_t f, OUT_Z_CAP(nameSize) char *name, int nameSize, OUT_Z_CAP(commentSize) char *comment, int commentSize ) = 0;
	virtual int						GetMostRecentElapsedMinutes( void ) = 0;
	virtual int						GetMostRecentElapsedSeconds( void ) = 0;
	virtual int						GetMostRecentElapsedTimeSet( void ) = 0;
	virtual void					SetMostRecentElapsedMinutes( const int min ) = 0;
	virtual void					SetMostRecentElapsedSeconds( const int sec ) = 0;

	virtual void					UpdateSaveGameScreenshots() = 0;

	virtual void					OnFinishedClientRestore() = 0;

	virtual void					AutoSaveDangerousIsSafe() = 0;

	virtual char const				*GetMostRecentlyLoadedFileName() = 0;
	virtual char const				*GetSaveFileName() = 0;

	virtual bool					IsXSave( void ) = 0;
	virtual void					SetIsXSave( bool bState ) = 0;

	virtual void					FinishAsyncSave() = 0;
	virtual bool					StorageDeviceValid() = 0;
	virtual void					SetMostRecentSaveGame( const char *lpszFilename ) = 0;

	virtual bool					IsSaveInProgress() = 0;
};

void *SaveAllocMemory( size_t num, size_t size, bool bClear = false );
void SaveFreeMemory( void *pSaveMem );

extern ISaveRestore *saverestore;

#endif // HOST_SAVERESTORE_H
