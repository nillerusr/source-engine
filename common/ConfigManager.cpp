//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <windows.h>
#include "interface.h"
#include "tier0/icommandline.h"
#include "filesystem_tools.h"
#include "KeyValues.h"
#include "tier1/utlbuffer.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <stdio.h>
#include "ConfigManager.h"
#include "SourceAppInfo.h"
#include "steam/steam_api.h"

extern CSteamAPIContext *steamapicontext;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define	GAME_CONFIG_FILENAME	"GameConfig.txt"
#define TOKEN_SDK_VERSION		"SDKVersion"

// Version history:
//	0 - Initial release
//	1 - Versioning added, DoD configuration added
//	2 - Ep1 added
//	3 - Ep2, TF2, and Portal added
//  4 - TF2 moved to its own engine

#define SDK_LAUNCHER_VERSION 5

// Half-Life 2
defaultConfigInfo_t HL2Info =
{
	"Half-Life 2",
	"hl2",
	"halflife2.fgd",
	"info_player_start",
	"hl2.exe",
	GetAppSteamAppId( k_App_HL2 )
};

// Counter-Strike: Source
defaultConfigInfo_t CStrikeInfo =
{
	"Counter-Strike: Source",
	"cstrike",
	"cstrike.fgd",
	"info_player_terrorist",
	"hl2.exe",
	GetAppSteamAppId( k_App_CSS )
};

//Half-Life 2: Deathmatch
defaultConfigInfo_t HL2DMInfo =
{
	"Half-Life 2: Deathmatch",
	"hl2mp",
	"hl2mp.fgd",
	"info_player_deathmatch",
	"hl2.exe",
	GetAppSteamAppId( k_App_HL2MP )
};

// Day of Defeat: Source
defaultConfigInfo_t DODInfo = 
{
	"Day of Defeat: Source",
	"dod",
	"dod.fgd",
	"info_player_allies",
	"hl2.exe",
	GetAppSteamAppId( k_App_DODS )
};

// Half-Life 2 Episode 1
defaultConfigInfo_t Episode1Info =
{
	"Half-Life 2: Episode One",
	"episodic",
	"halflife2.fgd",
	"info_player_start",
	"hl2.exe",
	GetAppSteamAppId( k_App_HL2_EP1 ) 
};

// Half-Life 2 Episode 2
defaultConfigInfo_t Episode2Info =
{
	"Half-Life 2: Episode Two",
	"ep2",
	"halflife2.fgd",
	"info_player_start",
	"hl2.exe",
	GetAppSteamAppId( k_App_HL2_EP2 ) 
};

// Team Fortress 2
defaultConfigInfo_t TF2Info =
{
	"Team Fortress 2",
	"tf",
	"tf.fgd",
	"info_player_teamspawn",
	"hl2.exe",
	GetAppSteamAppId( k_App_TF2 )
};

// Portal
defaultConfigInfo_t PortalInfo =
{
	"Portal",
	"portal",
	"portal.fgd",
	"info_player_start",
	"hl2.exe",
	GetAppSteamAppId( k_App_PORTAL )
};

// Portal
defaultConfigInfo_t SourceTestInfo =
{
	"SourceTest",
	"sourcetest",
	"halflife2.fgd",
	"info_player_start",
	"hl2.exe",
	243730
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameConfigManager::CGameConfigManager( void ) : m_pData( NULL ), m_LoadStatus( LOADSTATUS_NONE )
{
	// Start with default directory
	GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), m_szBaseDirectory, sizeof( m_szBaseDirectory ) );
	Q_StripLastDir( m_szBaseDirectory, sizeof( m_szBaseDirectory ) );	// Get rid of the filename.
	Q_StripTrailingSlash( m_szBaseDirectory );
	m_eSDKEpoch = (eSDKEpochs) SDK_LAUNCHER_VERSION;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CGameConfigManager::~CGameConfigManager( void )
{
	// Release the keyvalues
	if ( m_pData != NULL )
	{
		m_pData->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Config loading interface
// Input  : *baseDir - base directory for our file
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfigManager::LoadConfigs( const char *baseDir )
{
	return LoadConfigsInternal( baseDir, false );
}


//-----------------------------------------------------------------------------
// Purpose: Loads a file into the given utlbuffer.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ReadUtlBufferFromFile( CUtlBuffer &buffer, const char *szPath )
{
	struct _stat fileInfo;
	if ( _stat( szPath, &fileInfo ) == -1 )
	{
		return false;
	}

	buffer.EnsureCapacity( fileInfo.st_size );

	int nFile = _open( szPath, _O_BINARY | _O_RDONLY );
	if ( nFile == -1 )
	{
		return false;
	} 

	if ( _read( nFile, buffer.Base(), fileInfo.st_size ) != fileInfo.st_size )
	{
		_close( nFile );
		return false;
	}

	_close( nFile );
	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, fileInfo.st_size );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Loads a file into the given utlbuffer.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool SaveUtlBufferToFile( CUtlBuffer &buffer, const char *szPath )
{
	int nFile = _open( szPath, _O_TEXT | _O_CREAT | _O_TRUNC | _O_RDWR, _S_IWRITE );
	if ( nFile == -1 )
	{
		return false;
	} 

	int nSize = buffer.TellMaxPut();

	if ( _write( nFile, buffer.Base(), nSize ) < nSize )
	{
		_close( nFile );
		return false;
	}

	_close( nFile );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Load a game configuration file (with fail-safes)
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfigManager::LoadConfigsInternal( const char *baseDir, bool bRecursiveCall )
{
	// Init the config if it doesn't exist
	if ( !IsLoaded() )
	{
		m_pData = new KeyValues( GAME_CONFIG_FILENAME );

		if ( !IsLoaded() )
		{
			m_LoadStatus = LOADSTATUS_ERROR;
			return false;
		}
	}

	// Clear it out
	m_pData->Clear();

	// Build our default directory
	if ( baseDir != NULL && baseDir[0] != NULL )
	{
		SetBaseDirectory( baseDir );
	}

	// Make a full path name
	char szPath[MAX_PATH];
	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", GetBaseDirectory(), GAME_CONFIG_FILENAME );

	bool bLoaded = false;

	CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( ReadUtlBufferFromFile( buffer, szPath ) )
	{
		bLoaded = m_pData->LoadFromBuffer( szPath, buffer, NULL, NULL );
	}

	if ( !bLoaded )
	{
		// Attempt to re-create the configs
		if ( CreateAllDefaultConfigs() )
		{
			// Only allow this once
			if ( !bRecursiveCall )
				return LoadConfigsInternal( baseDir, true );

			// Version the config.
			VersionConfig();
		}

		m_LoadStatus = LOADSTATUS_ERROR;
		return false;
	}
	else
	{
		// Check to see if the gameconfig.txt is up to date.
		UpdateConfigsInternal();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Add to the current config.
//-----------------------------------------------------------------------------
void CGameConfigManager::UpdateConfigsInternal( void )
{
	// Check to a valid gameconfig.txt file buffer.
	if ( !IsLoaded() )
		return;

	// Check for version first.  If the version is up to date, it is assumed to be accurate
	if ( IsConfigCurrent() )
		return;

	KeyValues *pGameBlock = GetGameBlock();
	if ( !pGameBlock )
	{
		// If we don't have a game block, reset the config file.
		ResetConfigs();
		return;
	}

	KeyValues *pDefaultBlock = new KeyValues( "DefaultConfigs" );
	if ( pDefaultBlock != NULL )
	{
		// Compile our default configurations
		GetDefaultGameBlock( pDefaultBlock );

		// Compare our default block to our current configs
		KeyValues *pNextSubKey = pDefaultBlock->GetFirstTrueSubKey();
		while ( pNextSubKey != NULL )
		{
			// If we already have the name, we don't care about it
			if ( pGameBlock->FindKey( pNextSubKey->GetName() ) )
			{
				// Advance by one key
				pNextSubKey = pNextSubKey->GetNextTrueSubKey();
				continue;
			}

			// Copy the data through to our game block
			KeyValues *pKeyCopy = pNextSubKey->MakeCopy();
			pGameBlock->AddSubKey( pKeyCopy );

			// Advance by one key
			pNextSubKey = pNextSubKey->GetNextTrueSubKey();
		}
		
		// All done
		pDefaultBlock->deleteThis();
	}

	// Save the new config.
	SaveConfigs();

	// Add the new version as we have been updated.
	VersionConfig();
}

//-----------------------------------------------------------------------------
// Purpose: Update the gameconfig.txt version number.
//-----------------------------------------------------------------------------
void CGameConfigManager::VersionConfig( void )
{
	// Check to a valid gameconfig.txt file buffer.
	if ( !IsLoaded() )
		return;

	// Look for the a version key value pair and update it.
	KeyValues *pKeyVersion =  m_pData->FindKey( TOKEN_SDK_VERSION );

	// Update the already existing version key value pair.
	if ( pKeyVersion )
	{
		if ( pKeyVersion->GetInt() == m_eSDKEpoch )
			return;

		m_pData->SetInt( TOKEN_SDK_VERSION, m_eSDKEpoch );
	}
	// Create a new version key value pair.
	else
	{
		m_pData->SetInt( TOKEN_SDK_VERSION, m_eSDKEpoch );
	}

	// Save the configuration.
	SaveConfigs();
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the version of the gameconfig.txt is up to date.
//-----------------------------------------------------------------------------
bool CGameConfigManager::IsConfigCurrent( void )
{
	// Check to a valid gameconfig.txt file buffer.
	if ( !IsLoaded() )
		return false;

	KeyValues *pKeyValue = m_pData->FindKey( TOKEN_SDK_VERSION );
	if ( !pKeyValue )
		return false;

	int nVersion = pKeyValue->GetInt();
	if ( nVersion == m_eSDKEpoch )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the base path for a default config's install (handling steam's paths)
//-----------------------------------------------------------------------------
void CGameConfigManager::GetRootGameDirectory( char *out, size_t outLen, const char *rootDir )
{
	Q_strncpy( out, rootDir, outLen );
}

//-----------------------------------------------------------------------------
// Purpose: Get the base path for a default config's content sources (handling steam's paths)
//-----------------------------------------------------------------------------
void CGameConfigManager::GetRootContentDirectory( char *out, size_t outLen, const char *rootDir )
{
	// Steam install is different
	if ( g_pFullFileSystem )
	{
		Q_snprintf( out, outLen, "%s\\sourcesdk_content", rootDir );
	}
	else
	{
		Q_snprintf( out, outLen, "%s\\content", rootDir );
	}
}

// Default game configuration template
const char szDefaultConfigText[] =
"\"%gamename%\"\
{\
	\"GameDir\"	\"%gamedir%\"\
	\"Hammer\"\
	{\
		\"TextureFormat\"		\"5\"\
		\"MapFormat\"		\"4\"\
		\"DefaultTextureScale\"	\"0.250000\"\
		\"DefaultLightmapScale\"	\"16\"\
		\"DefaultSolidEntity\"	\"func_detail\"\
		\"DefaultPointEntity\"	\"%defaultpointentity%\"\
		\"GameExeDir\"		\"%gameexe%\"\
		\"MapDir\"		\"%gamemaps%\"\
		\"CordonTexture\"		\"tools\\toolsskybox\"\
		\"MaterialExcludeCount\"	\"0\"\
		\"GameExe\"	\"%gameEXE%\"\
		\"BSP\"		\"%bspdir%\"\
		\"Vis\"		\"%visdir%\"\
		\"Light\"	\"%lightdir%\"\
}}";

// NOTE: This function could use some re-write, it can't handle non-retail paths well

//-----------------------------------------------------------------------------
// Purpose: Add a templated default configuration with proper paths
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfigManager::AddDefaultConfig( const defaultConfigInfo_t &info, KeyValues *out, const char *rootDirectory, const char *gameExeDir )
{	
	// NOTE: Freed by head keyvalue
	KeyValues *newConfig = new KeyValues( info.gameName );
	if ( newConfig->LoadFromBuffer( "defaultcfg.txt", szDefaultConfigText ) == false )
		return false;

	newConfig->SetName( info.gameName );
	
	// Game's root directory (with special steam name handling)
	char rootGameDir[MAX_PATH];
	GetRootGameDirectory( rootGameDir, sizeof( rootGameDir ), rootDirectory );

	// Game's content directory
	char contentRootDir[MAX_PATH];
	GetRootContentDirectory( contentRootDir, sizeof( contentRootDir ), rootDirectory );

	char szPath[MAX_PATH];

	// Game directory
	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", rootGameDir, info.gameDir );

	if ( !g_pFullFileSystem->IsDirectory( szPath ) )
		return false;

	newConfig->SetString( "GameDir", szPath );

	// Create the Hammer portion of this block
	KeyValues *hammerBlock = newConfig->FindKey( "Hammer" );

	if ( hammerBlock == NULL )
		return false;

	hammerBlock->SetString( "GameExeDir", gameExeDir );

	// Fill in the proper default point entity
	hammerBlock->SetString( "DefaultPointEntity", info.defaultPointEntity );

	// Fill in the default VMF directory
	char contentMapDir[MAX_PATH];
	Q_snprintf( contentMapDir, sizeof( contentMapDir ), "%s\\%s\\mapsrc", contentRootDir, info.gameDir );
	hammerBlock->SetString( "MapDir", contentMapDir );

	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s\\maps", rootGameDir, info.gameDir );
	hammerBlock->SetString( "BSPDir", szPath );

	// Fill in the game executable
	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", gameExeDir, info.exeName );
	hammerBlock->SetString( "GameEXE", szPath );

	//Fill in game FGDs
	if ( info.FGD[0] != '\0' )
	{
		Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", GetBaseDirectory(), info.FGD );
		hammerBlock->SetString( "GameData0", szPath );
	}

	// Fill in the tools path
	Q_snprintf( szPath, sizeof( szPath ), "%s\\vbsp.exe", GetBaseDirectory() );
	hammerBlock->SetString( "BSP", szPath );

	Q_snprintf( szPath, sizeof( szPath ), "%s\\vvis.exe", GetBaseDirectory() );
	hammerBlock->SetString( "Vis", szPath );

	Q_snprintf( szPath, sizeof( szPath ), "%s\\vrad.exe", GetBaseDirectory() );
	hammerBlock->SetString( "Light", szPath );

	// Get our insertion point
	KeyValues *insertSpot = out->GetFirstTrueSubKey();
	
	// Set this as the sub key if there's nothing already there
	if ( insertSpot == NULL )
	{
		out->AddSubKey( newConfig );
	}
	else
	{
		// Find the last subkey
		while ( insertSpot->GetNextTrueSubKey() )
		{
			insertSpot = insertSpot->GetNextTrueSubKey();
		}
		
		// Become a peer to it
		insertSpot->SetNextKey( newConfig );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether the requested appID is installed on this computer
// Input  : nAppID - ID to verify
// Output : Returns true if installed, false if not.
//-----------------------------------------------------------------------------
bool CGameConfigManager::IsAppSubscribed( int nAppID )
{
	bool bIsSubscribed = false;

	if ( steamapicontext && steamapicontext->SteamApps() )
	{
		// See if specified app is installed
		bIsSubscribed = steamapicontext->SteamApps()->BIsSubscribedApp( nAppID );
	}
	else
	{
		// If we aren't running FileSystem Steam then we must be doing internal development. Give everything.
		bIsSubscribed = true;
	}

	return bIsSubscribed;
}

//-----------------------------------------------------------------------------
// Purpose: Create default configurations for all Valve retail applications
//-----------------------------------------------------------------------------
bool CGameConfigManager::CreateAllDefaultConfigs( void )
{
	bool bRetVal = true;

	// Start our new block
	KeyValues *configBlock = new KeyValues( "Configs" );
	KeyValues *gameBlock = configBlock->CreateNewKey();
	gameBlock->SetName( "Games" );

	GetDefaultGameBlock( gameBlock );

	bRetVal = !gameBlock->IsEmpty(); 

	// Make a full path name
	char szPath[MAX_PATH];
	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", GetBaseDirectory(), GAME_CONFIG_FILENAME );

	CUtlBuffer buffer;
	configBlock->RecursiveSaveToFile( buffer, 0 );
	SaveUtlBufferToFile( buffer, szPath );

	configBlock->deleteThis();

	m_LoadStatus = LOADSTATUS_CREATED;

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: Load game information from an INI file
//-----------------------------------------------------------------------------
bool CGameConfigManager::ConvertGameConfigsINI( void )
{
	const char *iniFilePath = GetIniFilePath();

	// Load our INI file
	int nNumConfigs = GetPrivateProfileInt( "Configs", "NumConfigs", 0, iniFilePath );
	if ( nNumConfigs <= 0 )
		return false;

	// Build a new keyvalue file
	KeyValues *headBlock = new KeyValues( "Configs" );

	// Create the block for games
	KeyValues *gamesBlock = headBlock->CreateNewKey( );
	gamesBlock->SetName( "Games" );

	int		i;
	int		nStrlen;
	char	szSectionName[MAX_PATH];
	char	textBuffer[MAX_PATH];

	// Parse all the configs
	for ( int nConfig = 0; nConfig < nNumConfigs; nConfig++ )
	{
		// Each came configuration is stored in a different section, named "GameConfig0..GameConfigN".
		// If the "Name" key exists in this section, try to load the configuration from this section.
		sprintf(szSectionName, "GameConfig%d", nConfig);

		int nCount = GetPrivateProfileString(szSectionName, "Name", "", textBuffer, sizeof(textBuffer), iniFilePath);
		if (nCount > 0)
		{
			// Make a new section
			KeyValues *subGame = gamesBlock->CreateNewKey();
			subGame->SetName( textBuffer );

			GetPrivateProfileString( szSectionName, "ModDir", "", textBuffer, sizeof(textBuffer), iniFilePath);
			
			// Add the mod dir
			subGame->SetString( "GameDir", textBuffer );
			
			// Start a block for Hammer settings
			KeyValues *hammerBlock = subGame->CreateNewKey();
			hammerBlock->SetName( "Hammer" );
			
			i = 0;

			// Get all FGDs	
			do
			{
				char szGameData[MAX_PATH];

				sprintf( szGameData, "GameData%d", i );
				nStrlen = GetPrivateProfileString( szSectionName, szGameData, "", textBuffer, sizeof(textBuffer), iniFilePath );
				
				if ( nStrlen > 0 )
				{
					hammerBlock->SetString( szGameData, textBuffer );
					i++;
				}
			} while ( nStrlen > 0 );

			hammerBlock->SetInt( "TextureFormat", GetPrivateProfileInt( szSectionName, "TextureFormat", 5 /*FIXME: tfVMT*/, iniFilePath ) );
			hammerBlock->SetInt( "MapFormat", GetPrivateProfileInt( szSectionName, "MapFormat", 4 /*FIXME: mfHalfLife2*/, iniFilePath ) );
			
			// Default texture scale
			GetPrivateProfileString( szSectionName, "DefaultTextureScale", "1", textBuffer, sizeof(textBuffer), iniFilePath );
			float defaultTextureScale = (float) atof( textBuffer );
			if ( defaultTextureScale == 0 )
			{
				defaultTextureScale = 1.0f;
			}

			hammerBlock->SetFloat( "DefaultTextureScale", defaultTextureScale );
			
			hammerBlock->SetInt( "DefaultLightmapScale", GetPrivateProfileInt( szSectionName, "DefaultLightmapScale", 16 /*FIXME: DEFAULT_LIGHTMAP_SCALE*/, iniFilePath ) );

			GetPrivateProfileString( szSectionName, "GameExe", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "GameExe", textBuffer );

			GetPrivateProfileString( szSectionName, "DefaultSolidEntity", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "DefaultSolidEntity", textBuffer );
			
			GetPrivateProfileString( szSectionName, "DefaultPointEntity", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "DefaultPointEntity", textBuffer );
			
			GetPrivateProfileString( szSectionName, "BSP", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "BSP", textBuffer );
			
			GetPrivateProfileString( szSectionName, "Vis", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "Vis", textBuffer );
			
			GetPrivateProfileString( szSectionName, "Light", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "Light", textBuffer );

			GetPrivateProfileString( szSectionName, "GameExeDir", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "GameExeDir", textBuffer );

			GetPrivateProfileString( szSectionName, "MapDir", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "MapDir", textBuffer );
			
			GetPrivateProfileString( szSectionName, "BSPDir", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "BSPDir", textBuffer );
			
			GetPrivateProfileString( szSectionName, "CordonTexture", "", textBuffer, sizeof(textBuffer), iniFilePath );
			hammerBlock->SetString( "CordonTexture", textBuffer );
			
			GetPrivateProfileString( szSectionName, "MaterialExcludeCount", "0", textBuffer, sizeof(textBuffer), iniFilePath );
			int materialExcludeCount = atoi( textBuffer );
			hammerBlock->SetInt( "MaterialExcludeCount", materialExcludeCount );
			
			char excludeDir[MAX_PATH];

			// Write out all excluded directories
			for( i = 0; i < materialExcludeCount; i++ )
			{
				sprintf( &excludeDir[0], "-MaterialExcludeDir%d", i );
				GetPrivateProfileString( szSectionName, excludeDir, "", textBuffer, sizeof( textBuffer ), iniFilePath ); 
				hammerBlock->SetString( excludeDir, textBuffer );
			}
		}
	}
	// Make a full path name
	char szPath[MAX_PATH];
	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", GetBaseDirectory(), GAME_CONFIG_FILENAME );

	CUtlBuffer buffer;
	headBlock->RecursiveSaveToFile( buffer, 0 );
	SaveUtlBufferToFile( buffer, szPath );

	// Rename the old INI file
	char newFilePath[MAX_PATH];
	Q_snprintf( newFilePath, sizeof( newFilePath ), "%s.OLD", iniFilePath );

	rename( iniFilePath, newFilePath );

	// Notify that we were converted
	m_LoadStatus = LOADSTATUS_CONVERTED;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Write out a game configuration file
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameConfigManager::SaveConfigs( const char *baseDir )
{
	if ( !IsLoaded() )
		return false;

	// Build our default directory
	if ( baseDir != NULL && baseDir[0] != NULL )
	{
		SetBaseDirectory( baseDir );
	}

	// Make a full path name
	char szPath[MAX_PATH];
	Q_strncpy( szPath, GetBaseDirectory(), sizeof(szPath) );
	Q_AppendSlash( szPath, sizeof(szPath) );
	Q_strncat( szPath, GAME_CONFIG_FILENAME, sizeof( szPath ), COPY_ALL_CHARACTERS );
	
	CUtlBuffer buffer;
	m_pData->RecursiveSaveToFile( buffer, 0 );

	return SaveUtlBufferToFile( buffer, szPath );
}

//-----------------------------------------------------------------------------
// Purpose: Find the directory our .exe is based out of
//-----------------------------------------------------------------------------
const char *CGameConfigManager::GetBaseDirectory( void )
{
	return m_szBaseDirectory;
}

//-----------------------------------------------------------------------------
// Purpose: Find the root directory
//-----------------------------------------------------------------------------
const char *CGameConfigManager::GetRootDirectory( void )
{
	static char path[MAX_PATH] = {0};
	if ( path[0] == 0 )
	{
		Q_strncpy( path, GetBaseDirectory(), sizeof( path ) );
		Q_StripLastDir( path, sizeof( path ) );	// Get rid of the 'bin' directory
		Q_StripTrailingSlash( path );
	}
	return path;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the game configuation block
//-----------------------------------------------------------------------------
KeyValues *CGameConfigManager::GetGameBlock( void )
{
	if ( !IsLoaded() )
		return NULL;

	return ( m_pData->FindKey( TOKEN_GAMES, true ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a piece of the game configuation block of the given name
// Input  : *keyName - name of the block to return
//-----------------------------------------------------------------------------
KeyValues *CGameConfigManager::GetGameSubBlock( const char *keyName )
{
	if ( !IsLoaded() )
		return NULL;

	KeyValues *pGameBlock = GetGameBlock();
	if ( pGameBlock == NULL )
		return NULL;

	// Return the data
	KeyValues *pSubBlock = pGameBlock->FindKey( keyName );

	return pSubBlock;
}

//-----------------------------------------------------------------------------
// Purpose: Get the gamecfg.ini file for conversion
//-----------------------------------------------------------------------------
const char *CGameConfigManager::GetIniFilePath( void )
{
	static char iniFilePath[MAX_PATH] = {0};
	if ( iniFilePath[0] == 0 )
	{
		Q_strncpy( iniFilePath, GetBaseDirectory(), sizeof( iniFilePath ) );
		Q_strncat( iniFilePath, "\\gamecfg.ini", sizeof( iniFilePath ), COPY_ALL_CHARACTERS );
	}

	return iniFilePath;
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the current config and recreates it with default values
//-----------------------------------------------------------------------------
bool CGameConfigManager::ResetConfigs( const char *baseDir /*= NULL*/ )
{
	// Build our default directory
	if ( baseDir != NULL && baseDir[0] != NULL )
	{
		SetBaseDirectory( baseDir );
	}

	// Make a full path name
	char szPath[MAX_PATH];
	Q_snprintf( szPath, sizeof( szPath ), "%s\\%s", GetBaseDirectory(), GAME_CONFIG_FILENAME );

	// Delete the file
	if ( unlink( szPath ) )
		return false;

	// Load the file again (causes defaults to be created)
	if ( LoadConfigsInternal( baseDir, false ) == false )
		return false;

	// Save it out
	return SaveConfigs( baseDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameConfigManager::SetBaseDirectory( const char *pDirectory )
{
	// Clear it
	if ( pDirectory == NULL || pDirectory[0] == '\0' )
	{
		m_szBaseDirectory[0] = '\0';
		return;
	}

	// Copy it
	Q_strncpy( m_szBaseDirectory, pDirectory, sizeof( m_szBaseDirectory ) );
	Q_StripTrailingSlash( m_szBaseDirectory );
}

//-----------------------------------------------------------------------------
// Purpose: Create a block of keyvalues containing our default configurations
// Output : A block of keyvalues
//-----------------------------------------------------------------------------
bool CGameConfigManager::GetDefaultGameBlock( KeyValues *pIn )
{
	CUtlVector<defaultConfigInfo_t> defaultConfigs;

	// Add HL2 games to list
	defaultConfigs.AddToTail( HL2DMInfo );
	defaultConfigs.AddToTail( HL2Info );
	defaultConfigs.AddToTail( Episode1Info );
	defaultConfigs.AddToTail( Episode2Info );
	defaultConfigs.AddToTail( PortalInfo );
	defaultConfigs.AddToTail( SourceTestInfo );

	// Add TF2 games to list
	defaultConfigs.AddToTail( TF2Info );
	defaultConfigs.AddToTail( DODInfo );
	defaultConfigs.AddToTail( CStrikeInfo );

	if ( pIn == NULL )
		return false;

	char szPath[MAX_PATH];

	// Add all default configs
	int nNumConfigs = defaultConfigs.Count();
	for ( int i = 0; i < nNumConfigs; i++ )
	{
		// If it's installed, add it
		if ( IsAppSubscribed( defaultConfigs[i].steamAppID ) )
		{
			GetRootGameDirectory( szPath, sizeof( szPath ), GetRootDirectory() );
			AddDefaultConfig( defaultConfigs[i], pIn, GetRootDirectory(), szPath );
		}
	}

	return true;
}
