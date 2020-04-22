//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handle addition and editing of game configurations
//
//=====================================================================================//

#include <windows.h>
#include "interface.h"
#include "tier0/icommandline.h"
#include "filesystem_tools.h"
#include "sdklauncher_main.h"
#include "ConfigManager.h"
#include "KeyValues.h"

#include <io.h>
#include <stdio.h>

#include "configs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern CGameConfigManager	g_ConfigManager;

//-----------------------------------------------------------------------------
// Purpose: Copy a character string into a utlvector of characters
//-----------------------------------------------------------------------------
void UtlStrcpy( CUtlVector<char> &dest, const char *pSrc )
{
	dest.EnsureCount( strlen( pSrc ) + 1 );
	Q_strncpy( dest.Base(), pSrc, dest.Count() );
}

//-----------------------------------------------------------------------------
// Purpose: Return the path for the gamecfg.INI file
//-----------------------------------------------------------------------------
const char *GetIniFilePath( void )
{
	static char iniFilePath[MAX_PATH] = {0};
	if ( iniFilePath[0] == 0 )
	{
		Q_strncpy( iniFilePath, GetSDKLauncherBinDirectory(), sizeof( iniFilePath ) );
		Q_strncat( iniFilePath, "\\gamecfg.ini", sizeof( iniFilePath ), COPY_ALL_CHARACTERS );
	}
	return iniFilePath;
}

//-----------------------------------------------------------------------------
// Purpose: Add a new configuration with proper paths
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

// NOTE: This code is fairly fragile, it'd be a lot better to have a solid solution for adding in a new config

bool AddConfig( const char *pModName, const char *pModDirectory, ModType_t modType )
{
	// Manager must be loaded
	if ( g_ConfigManager.IsLoaded() == false )
		return false;

	// Set to defaults
	defaultConfigInfo_t newInfo;
	memset( &newInfo, 0, sizeof( newInfo ) );

	// Mod name
	Q_strncpy( newInfo.gameName, pModName, sizeof( newInfo.gameName ) );
	
	// Basic FGD
	if ( modType == ModType_HL2 )
	{
		Q_strncpy( newInfo.FGD, "halflife2.fgd", sizeof( newInfo.FGD ) );
	}
	else if ( modType == ModType_HL2_Multiplayer )
	{
		Q_strncpy( newInfo.FGD, "hl2mp.fgd", sizeof( newInfo.FGD ) );
	}
	else
	{
		Q_strncpy( newInfo.FGD, "base.fgd", sizeof( newInfo.FGD ) );
	}

	// Get the base directory
	Q_FileBase( pModDirectory, newInfo.gameDir, sizeof( newInfo.gameDir ) );

	KeyValues *gameBlock = g_ConfigManager.GetGameBlock();

	// Default executable
	Q_strncpy( newInfo.exeName, "hl2.exe", sizeof( newInfo.exeName ) );

	char szPath[MAX_PATH];
	Q_strncpy( szPath, pModDirectory, sizeof( szPath ) );
	Q_StripLastDir( szPath, sizeof( szPath ) );
	Q_StripTrailingSlash( szPath );

	char fullDir[MAX_PATH];
	g_ConfigManager.GetRootGameDirectory( fullDir, sizeof( fullDir ), g_ConfigManager.GetRootDirectory() );
	
	// Add the config into our file
	g_ConfigManager.AddDefaultConfig( newInfo, gameBlock, szPath, fullDir );

	// Set this as the currently active configuration
	SetVConfigRegistrySetting( GAMEDIR_TOKEN, pModDirectory );

	// Save and reload our configs
	g_ConfigManager.SaveConfigs();
	g_ConfigManager.LoadConfigs();

	return true;
}

