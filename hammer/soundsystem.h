//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sound management functions. Exposes a list of available sounds.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SOUNDSSYSTEM_H
#define SOUNDSSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

//-----------------------------------------------------------------------------
// Contains lists of all sounds	available for use
//-----------------------------------------------------------------------------
enum SoundType_t
{
	SOUND_TYPE_RAW = 0,
	SOUND_TYPE_GAMESOUND,
	SOUND_TYPE_SCENE,		// vcd file

	SOUND_TYPE_COUNT,
};

class CSoundSystem
{
public:
	CSoundSystem(void);
	virtual ~CSoundSystem(void);

	bool Initialize( );
	void ShutDown(void);

	// Build the list of sounds
	bool BuildSoundList( SoundType_t type );

	// Sound list iteration
	int SoundCount( SoundType_t type );
	const char *SoundName( SoundType_t type, int nIndex );
	const char *SoundFile( SoundType_t type, int nIndex );
	const char *SoundSourceFile( SoundType_t type, int nIndex );

	// Search through all the sounds for the specified one.
	bool FindSoundByName( const char *pFilename, SoundType_t *type, int *nIndex );

	// Plays a sound
	bool Play( SoundType_t type, int nIndex );
	bool PlayScene( const char *pFileName );	// Play the first sound in the specified scene.
	
	// Stops any playing sound.
	void StopSound();

	// Opens the source file associated with a sound
	void OpenSource( SoundType_t type, int nIndex );

private:
	struct SoundInfo_t
	{
		char *m_pSoundName;
		char *m_pSoundFile;
		const char *m_pSourceFile;
	};

	struct StringCache_t
	{
		enum
		{
			STRING_CACHE_SIZE = 128 * 1024,
		};

		char m_pBuf[STRING_CACHE_SIZE];
		int m_nTailIndex;	// Next address to fill
		StringCache_t *m_pNext;
	};

	struct SoundList_t
	{
		CUtlVector< SoundInfo_t >	m_Sounds;
		StringCache_t	*m_pStrings;
	};

private:
	typedef bool (CSoundSystem::*pDirCallbackFn)( const char *pDirectoryName );


	// Allocate, deallocate a string cache
	StringCache_t *CreateStringCache( StringCache_t* pPrevious );
	void DestroyStringCache( StringCache_t *pCache );

	// Adds a string to the string cache
	char *AddStringToCache( SoundType_t type, const char *pString );

	// Adds a sound to a sound list
	void AddSoundToList( SoundType_t type, const char *pSoundName, const char *pActualFile, const char *pSourceFile );

	// Cleans up the sound list
	void CleanupSoundList( SoundType_t type );

	// Goes into all subdirectories and calls the callback for each one..
	bool RecurseIntoDirectories( char const* pDirectoryName, pDirCallbackFn fn );

	// Add all sounds that lie within a single directory recursively
	bool ProcessDirectory_RawFileList( char const* pDirectoryName );
	bool ProcessDirectory_SceneFileList( char const* pDirectoryName );

	// Add all sounds that lie within a single directory
	void BuildFileListInDirectory( char const* pDirectoryName, const char *pExt, SoundType_t soundType );

	// Gamesounds may have macros embedded in them
	void AddGameSoundToList( const char *pGameSound, char const *pFileName, const char *pSourceFile );

	// Load all game sounds from a particular file 
	void AddGameSoundsFromFile( const char *pFileName );

	// Populate the list of game sounds
	bool BuildGameSoundList();

private:
	SoundList_t m_SoundList[SOUND_TYPE_COUNT];	
};


//-----------------------------------------------------------------------------
// Sound list iteration
//-----------------------------------------------------------------------------
inline int CSoundSystem::SoundCount( SoundType_t type )
{
	return m_SoundList[type].m_Sounds.Count();
}

inline const char *CSoundSystem::SoundName( SoundType_t type, int nIndex )
{
	return m_SoundList[type].m_Sounds[nIndex].m_pSoundName;
}

inline const char *CSoundSystem::SoundFile( SoundType_t type, int nIndex )
{
	return m_SoundList[type].m_Sounds[nIndex].m_pSoundFile;
}

inline const char *CSoundSystem::SoundSourceFile( SoundType_t type, int nIndex )
{
	return m_SoundList[type].m_Sounds[nIndex].m_pSourceFile;
}

extern CSoundSystem g_Sounds;


#endif // SOUNDSYSTEM_H
