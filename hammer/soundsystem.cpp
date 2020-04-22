//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sound management functions. Exposes a list of available sounds.
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "soundsystem.h"
#include "mmsystem.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "hammer.h"
#include "HammerScene.h"
#include "ScenePreviewDlg.h"
#include "soundchars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// FIXME: Put gamesounds parsing into shared code somewhere
#define MANIFEST_FILE			"scripts/game_sounds_manifest.txt"
#define SOUNDGENDER_MACRO		"$gender"
#define SOUNDGENDER_MACRO_LENGTH 7		// Length of above including $


// Sounds we're playing are loaded into here for Windows to access while playing them.
CUtlVector<char> g_SoundPlayData;


//-----------------------------------------------------------------------------
// Singleton sound system
//-----------------------------------------------------------------------------
CSoundSystem g_Sounds;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CSoundSystem::CSoundSystem()
{
}

CSoundSystem::~CSoundSystem()
{
	ShutDown();
}


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CSoundSystem::Initialize( )
{
	for ( int i = 0; i < SOUND_TYPE_COUNT; ++i )
	{
		m_SoundList[i].m_Sounds.EnsureCapacity( 1024 );
		m_SoundList[i].m_pStrings = NULL;

		if (!BuildSoundList( (SoundType_t)i ) )
			return false;
	}

	return true;
}

void CSoundSystem::ShutDown(void)
{
	for ( int i = 0; i < SOUND_TYPE_COUNT; ++i )
	{
		CleanupSoundList( (SoundType_t)i );
	}
}


//-----------------------------------------------------------------------------
// Build the list of sounds
//-----------------------------------------------------------------------------
bool CSoundSystem::BuildSoundList( SoundType_t type )
{
	CleanupSoundList( type );

	switch( type )
	{
	case SOUND_TYPE_RAW:
		return RecurseIntoDirectories( "sound", &CSoundSystem::ProcessDirectory_RawFileList );

	case SOUND_TYPE_GAMESOUND:
		return BuildGameSoundList();

	case SOUND_TYPE_SCENE:
		return RecurseIntoDirectories( "scenes", &CSoundSystem::ProcessDirectory_SceneFileList );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Cleans up the sound list
//-----------------------------------------------------------------------------
void CSoundSystem::CleanupSoundList( SoundType_t type )
{
	m_SoundList[type].m_Sounds.RemoveAll();
	DestroyStringCache( m_SoundList[type].m_pStrings );
	m_SoundList[type].m_pStrings = NULL;
}


//-----------------------------------------------------------------------------
// Allocate, deallocate a string cache
//-----------------------------------------------------------------------------
CSoundSystem::StringCache_t *CSoundSystem::CreateStringCache( CSoundSystem::StringCache_t* pPrevious )
{
	StringCache_t *pCache = new StringCache_t;
	pCache->m_nTailIndex = 0;
	pCache->m_pNext = pPrevious;
	return pCache; 
}

void CSoundSystem::DestroyStringCache( CSoundSystem::StringCache_t *pCache )
{
	if ( pCache )
	{
		DestroyStringCache( pCache->m_pNext );
		delete pCache;
	}
}


//-----------------------------------------------------------------------------
// Adds a string to the string cache
//-----------------------------------------------------------------------------
char *CSoundSystem::AddStringToCache( SoundType_t type, const char *pString )
{
	int copyLen = V_strlen( pString ) + 1;

	StringCache_t *pCache = m_SoundList[type].m_pStrings;
	if ( (!pCache) || ( copyLen + pCache->m_nTailIndex > StringCache_t::STRING_CACHE_SIZE ) )
	{
		m_SoundList[type].m_pStrings = CreateStringCache( pCache );
		pCache = m_SoundList[type].m_pStrings;
	}

	char fixedString[MAX_PATH];
	V_strncpy( fixedString, pString, sizeof( fixedString ) );
	V_FixSlashes( fixedString );
	copyLen = V_strlen( fixedString ) + 1;

	char *pDest = &pCache->m_pBuf[ pCache->m_nTailIndex ];
	memcpy( pDest, fixedString, copyLen );
	pCache->m_nTailIndex += copyLen;

	return pDest;
}

	
//-----------------------------------------------------------------------------
// Adds a sound to a sound list
//-----------------------------------------------------------------------------
void CSoundSystem::AddSoundToList( SoundType_t type, const char *pSoundName, const char *pActualFile, const char *pSourceFile )
{
	// FIXME: Optimize the allocation pattern?
	int i = m_SoundList[type].m_Sounds.AddToTail();
	SoundInfo_t &info = m_SoundList[type].m_Sounds[i];

	info.m_pSoundName = AddStringToCache( type, pSoundName ); 
	
	if ( type == SOUND_TYPE_RAW )
	{
		info.m_pSoundFile = info.m_pSoundName;
		info.m_pSourceFile = info.m_pSoundName;
	}
	else
	{
		info.m_pSoundFile = AddStringToCache( type, pActualFile ); 
		info.m_pSourceFile = pSourceFile;
	}
}


//-----------------------------------------------------------------------------
// Add all sounds that lie within a single directory
//-----------------------------------------------------------------------------
void CSoundSystem::BuildFileListInDirectory( char const* pDirectoryName, const char *pExt, SoundType_t soundType )
{
	Assert( Q_strlen( pExt ) <= 3 );

	int nDirectoryNameLen = V_strlen( pDirectoryName );
	char *pWildCard = ( char * )stackalloc( nDirectoryNameLen + 7 );
	Q_snprintf( pWildCard, nDirectoryNameLen + 7, "%s/*.%s", pDirectoryName, pExt );

	FileFindHandle_t findHandle;
	const char *pFileName = g_pFullFileSystem->FindFirst( pWildCard, &findHandle );
	for ( ; pFileName; pFileName = g_pFullFileSystem->FindNext( findHandle ) )
	{
		if( g_pFullFileSystem->FindIsDirectory( findHandle ) )
			continue;

		// Strip off the 'sound/' part of the sound name.
		int nAllocSize = nDirectoryNameLen + Q_strlen(pFileName) + 2;
		char *pFileNameWithPath = (char *)stackalloc( nAllocSize );
		
		const char *pStartPos = max( strchr( pDirectoryName, '/' ), strchr( pDirectoryName, '\\' ) );
		if ( pStartPos )
			Q_snprintf(	pFileNameWithPath, nAllocSize, "%s%c%s", pStartPos+1, CORRECT_PATH_SEPARATOR, pFileName ); 
		else
			V_strncpy( pFileNameWithPath, pFileName, nAllocSize );
		
		Q_strnlwr( pFileNameWithPath, nAllocSize );
		AddSoundToList( soundType, pFileNameWithPath, pFileNameWithPath, NULL );
	}
	g_pFullFileSystem->FindClose( findHandle );
}


//-----------------------------------------------------------------------------
// Populate the list of .WAV files
//-----------------------------------------------------------------------------
bool CSoundSystem::RecurseIntoDirectories( char const* pDirectoryName, pDirCallbackFn fn )
{
	// Have the callback process the directory.
	if ( !(this->*fn)( pDirectoryName ) )
		return false;

	int nDirectoryNameLen = Q_strlen( pDirectoryName );

	char *pWildCard = ( char * )stackalloc( nDirectoryNameLen + 5 );
	strcpy(pWildCard, pDirectoryName);
	strcat(pWildCard, "/*.*");
	int nPathStrLen = nDirectoryNameLen + 1;

	FileFindHandle_t findHandle;
	const char *pFileName = g_pFullFileSystem->FindFirst( pWildCard, &findHandle );
	for ( ; pFileName; pFileName = g_pFullFileSystem->FindNext( findHandle ) )
	{
		if ((pFileName[0] != '.') || (pFileName[1] != '.' && pFileName[1] != 0))
		{
			if( !g_pFullFileSystem->FindIsDirectory( findHandle ) )
				continue;

			int fileNameStrLen = Q_strlen( pFileName );
			char *pFileNameWithPath = ( char * )stackalloc( nPathStrLen + fileNameStrLen + 1 );
			memcpy( pFileNameWithPath, pWildCard, nPathStrLen );
			pFileNameWithPath[nPathStrLen] = '\0';
			Q_strncat( pFileNameWithPath, pFileName, nPathStrLen + fileNameStrLen + 1 );

			if (!RecurseIntoDirectories( pFileNameWithPath, fn ))
				return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Populate the list of .WAV files
//-----------------------------------------------------------------------------
bool CSoundSystem::ProcessDirectory_RawFileList( char const* pDirectoryName )
{
	if ( !g_pFileSystem )
		return false;

	Assert( Q_strnicmp( pDirectoryName, "sound", 5 ) == 0 );

	// Get all sound files out of this directory
	BuildFileListInDirectory( pDirectoryName, "wav", SOUND_TYPE_RAW );
	BuildFileListInDirectory( pDirectoryName, "mp3", SOUND_TYPE_RAW );

	return true;
}


//-----------------------------------------------------------------------------
// Populate the list of .VCD files
//-----------------------------------------------------------------------------
bool CSoundSystem::ProcessDirectory_SceneFileList( char const* pDirectoryName )
{
	if ( !g_pFileSystem )
		return false;

	// Get all sound files out of this directory
	BuildFileListInDirectory( pDirectoryName, "vcd", SOUND_TYPE_SCENE );

	return true;
}


//-----------------------------------------------------------------------------
// Splits a name into 2
//-----------------------------------------------------------------------------
static void SplitName( char const *input, int splitchar, int splitlen, char *before, int beforelen, char *after, int afterlen )
{
	char const *in = input;
	char *out = before;

	int c = 0;
	int l = 0;
	int maxl = beforelen;
	while ( *in )
	{
		if ( c == splitchar )
		{
			while ( --splitlen >= 0 )
			{
				in++;
			}

			*out = 0;
			out = after;
			maxl = afterlen;
			c++;
			continue;
		}

		if ( l >= maxl )
		{
			in++;
			c++;
			continue;
		}

		*out++ = *in++;
		l++;
		c++;
	}

	*out = 0;
}


//-----------------------------------------------------------------------------
// Gamesounds may have macros embedded in them
//-----------------------------------------------------------------------------
void CSoundSystem::AddGameSoundToList( const char *pGameSound, char const *pFileName, const char *pSourceFile )
{
	char const *p = Q_stristr( pFileName, SOUNDGENDER_MACRO );
	if ( !p )
	{
		AddSoundToList( SOUND_TYPE_GAMESOUND, pGameSound, pFileName, pSourceFile );
		return;
	}

	int offset = p - pFileName;
	Assert( offset >= 0 );
	int duration = SOUNDGENDER_MACRO_LENGTH;

	// Create a "male" version of the sound	only for browsing
	char before[ 256 ], after[ 256 ];
	Q_memset( before, 0, sizeof( before ) );
	Q_memset( after, 0, sizeof( after ) );

	SplitName( pFileName, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	char temp[ 256 ];
	Q_snprintf( temp, sizeof( temp ), "%s%s%s", before, "male", after );
	AddSoundToList( SOUND_TYPE_GAMESOUND, pGameSound, temp, pSourceFile );
}

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgoff.h>

//-----------------------------------------------------------------------------
// Load all game sounds from a particular file 
//-----------------------------------------------------------------------------
void CSoundSystem::AddGameSoundsFromFile( const char *pFileName )
{
	KeyValues *kv = new KeyValues( pFileName );
	if ( !kv->LoadFromFile( g_pFileSystem, pFileName, "GAME" ) )
	{
		kv->deleteThis();
		return;
	}

	const char *pSourceFile = AddStringToCache( SOUND_TYPE_GAMESOUND, pFileName );

	// parse out all of the top level sections and save their names
	for ( KeyValues *pKeys = kv; pKeys; pKeys = pKeys->GetNextKey() )
	{
		if ( !pKeys->GetFirstSubKey() )
			continue;

		const char *pRawFile = pKeys->GetString( "wave", NULL );
		if ( pRawFile )
		{
			AddGameSoundToList( pKeys->GetName(), pRawFile, pSourceFile );
		}
		else
		{
			KeyValues *pRndWave = pKeys->FindKey( "rndwave" );
			if ( pRndWave )
			{
				KeyValues *pFirstFile = pRndWave->GetFirstSubKey();
				if ( pFirstFile )
				{
					AddGameSoundToList( pKeys->GetName(), pFirstFile->GetString(), pSourceFile );
				}
			}
		}
	}

	if ( kv )
	{
		kv->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Populate the list of game sounds
//-----------------------------------------------------------------------------
bool CSoundSystem::BuildGameSoundList()
{
	KeyValues *manifest = new KeyValues( MANIFEST_FILE );
	if ( !manifest->LoadFromFile( g_pFileSystem, MANIFEST_FILE, "GAME" ) )
	{
		manifest->deleteThis();
		return false;
	}

	for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
	{
		if ( !Q_stricmp( sub->GetName(), "precache_file" ) ||
			!Q_stricmp( sub->GetName(), "declare_file" ) ||
			!Q_stricmp( sub->GetName(), "preload_file" ) )
		{
			// Add and always precache
			AddGameSoundsFromFile( sub->GetString() );
		}
	}
	manifest->deleteThis();
	return true;
}


//-----------------------------------------------------------------------------
// Plays a sound
//-----------------------------------------------------------------------------

bool CSoundSystem::FindSoundByName( const char *pFilename, SoundType_t *type, int *nIndex )
{
	char searchStr[MAX_PATH];
	V_strncpy( searchStr, pFilename, sizeof( searchStr ) );
	V_FixSlashes( searchStr );
	
	for ( int i = SOUND_TYPE_COUNT; --i >= 0; )
	{
		for ( int j = SoundCount( (SoundType_t)i ); --j >= 0; )
		{
			if ( Q_stristr( searchStr, SoundName( (SoundType_t)i, j ) ) )
			{
				*type = (SoundType_t)i;
				*nIndex = j;
				return true;
			}
		}
	}
	
	return false;
}


bool CSoundSystem::PlayScene( const char *pFileName )
{
	char fullFilename[MAX_PATH];
	V_snprintf( fullFilename, sizeof( fullFilename ), "scenes%c%s", CORRECT_PATH_SEPARATOR, pFileName );
	CChoreoScene *pScene = HammerLoadScene( fullFilename );
	if ( !pScene )
		return false;
		
	CScenePreviewDlg dlg( pScene, pFileName );
	dlg.DoModal();
	return true;
}


//-----------------------------------------------------------------------------
// Plays a sound
//-----------------------------------------------------------------------------

bool CSoundSystem::Play( SoundType_t type, int nIndex )
{
 	const char *pFileName = SoundFile( type, nIndex );
	if ( !pFileName )
		return false;

	// If it's a scene, get the first sound in the scene.
	if ( type == SOUND_TYPE_SCENE )
	{
		return PlayScene( pFileName );
	}

	// Voiceover files have this.
	pFileName = PSkipSoundChars( pFileName );

	char pRelativePath[MAX_PATH];
	Q_snprintf( pRelativePath, MAX_PATH, "sound/%s", pFileName );

	// Stop any previously-playing sound.
	StopSound();

	// We used to use GetLocalPath, but that doesn't work under Steam.
	FileHandle_t fp = g_pFileSystem->Open( pRelativePath, "rb" );
	if ( fp )
	{
		g_SoundPlayData.SetSize( g_pFileSystem->Size( fp ) );
		if ( g_pFileSystem->Read( g_SoundPlayData.Base(), g_SoundPlayData.Count(), fp ) == g_SoundPlayData.Count() )
		{
			return (PlaySound( g_SoundPlayData.Base(), NULL, SND_ASYNC | SND_MEMORY ) != FALSE);
		}
		g_pFileSystem->Close( fp );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Stops any playing sound.
//-----------------------------------------------------------------------------
void CSoundSystem::StopSound()
{
	PlaySound( NULL, NULL, SND_ASYNC | SND_MEMORY );
}


//-----------------------------------------------------------------------------
// Opens the source file associated with a sound
//-----------------------------------------------------------------------------
void CSoundSystem::OpenSource( SoundType_t type, int nIndex )
{
	if ( type == SOUND_TYPE_RAW )
		return;

	const char *pFileName = SoundSourceFile( type, nIndex );
	if ( pFileName )
	{
		char pRelativePath[MAX_PATH];
		Q_snprintf( pRelativePath, MAX_PATH, "%s", pFileName );

		char pFullPath[MAX_PATH];
		if ( g_pFullFileSystem->GetLocalPath( pRelativePath, pFullPath, MAX_PATH ) )
		{
			ShellExecute( NULL, "open", pFullPath, NULL, NULL, SW_SHOWNORMAL );
		}
	}
}

