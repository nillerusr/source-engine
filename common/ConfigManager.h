//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "KeyValues.h"
#include "utlvector.h"
#include "filesystem_init.h"


// See filesystem_init for the vconfig registry values.


#define	TOKEN_GAMES				"Games"
#define	TOKEN_GAME_DIRECTORY	"GameDir"
#define	TOKEN_TOOLS				"Tools"

struct defaultConfigInfo_t
{
	char	gameName[MAX_PATH];
	char	gameDir[MAX_PATH];
	char	FGD[MAX_PATH];
	char	defaultPointEntity[MAX_PATH];
	char	exeName[MAX_PATH];
	int		steamAppID;
};

enum eSDKEpochs
{
	HL2 = 1,
	EP1 = 2,
	EP2 = 3,
	SP2009 = 4,
	MP2009 = 5,
};

extern defaultConfigInfo_t *gDefaultConfigs[];

class CGameConfigManager
{
public:

	enum loadStatus_t
	{
		LOADSTATUS_NONE = 0,	// Configs were loaded with no error
		LOADSTATUS_CONVERTED,	// GameConfig.txt did not exist and was created by converting GameCfg.INI
		LOADSTATUS_CREATED,		// GameCfg.INI was not found, the system created the default configuration based on found GameInfo.txt resources
		LOADSTATUS_ERROR,		// File was not loaded and was unable to perform the above fail-safe procedures
	};

			CGameConfigManager( void );
			CGameConfigManager( const char *fileName );

			~CGameConfigManager( void );

	bool	LoadConfigs( const char *baseDir = NULL );
	bool	SaveConfigs( const char *baseDir = NULL );
	bool	ResetConfigs( const char *baseDir = NULL );
	
	int		GetNumConfigs( void );

	KeyValues	*GetGameBlock( void );
	KeyValues	*GetGameSubBlock( const char *keyName );
	bool		GetDefaultGameBlock( KeyValues *pIn );

	bool	IsLoaded( void ) const { return m_pData != NULL; }

	bool	WasConvertedOnLoad( void ) const { return m_LoadStatus == LOADSTATUS_CONVERTED; }
	bool	WasCreatedOnLoad( void ) const { return m_LoadStatus == LOADSTATUS_CREATED; }
	
	bool	AddDefaultConfig( const defaultConfigInfo_t &info, KeyValues *out, const char *rootDirectory, const char *gameExeDir );

	void	SetBaseDirectory( const char *pDirectory );

	void	GetRootGameDirectory( char *out, size_t outLen, const char *rootDir );

	const char *GetRootDirectory( void );
	void	SetSDKEpoch( eSDKEpochs epoch ) { m_eSDKEpoch = epoch; };

private:

	void	GetRootContentDirectory( char *out, size_t outLen, const char *rootDir );

	const char *GetBaseDirectory( void );
	const char *GetIniFilePath( void );

	bool	LoadConfigsInternal( const char *baseDir, bool bRecursiveCall );
	void	UpdateConfigsInternal( void );
	void	VersionConfig( void );
	bool	IsConfigCurrent( void );

	bool	ConvertGameConfigsINI( void );
	bool	CreateAllDefaultConfigs( void );
	bool	IsAppSubscribed( int nAppID );

	loadStatus_t	m_LoadStatus;	// Holds various state about what occured while loading
	KeyValues		*m_pData;		// Data as read from configuration file
	char			m_szBaseDirectory[MAX_PATH];	// Default directory
	eSDKEpochs		m_eSDKEpoch;    // Holds the "working version" of the SDK for times when we need to create an older set of game configurations.
									// This is required now that the SDK is deploying the tools for both the latest and previous versions of the engine.
};

#endif // CONFIGMANAGER_H
