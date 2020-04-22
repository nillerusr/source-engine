//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CONFIGS_H
#define CONFIGS_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"


enum ModType_t
{
	ModType_HL2,
	ModType_HL2_Multiplayer,
	ModType_FromScratch,
	ModType_SourceCodeOnly
};


class CGameConfig
{
public:
	CUtlVector<char> m_Name;
	CUtlVector<char> m_ModDir;
};


const char* GetIniFilePath();

void UtlStrcpy( CUtlVector<char> &dest, const char *pSrc );
void LoadGameConfigs( CUtlVector<CGameConfig*> &configs );
bool AddConfigToGameIni( const char *pModName, const char *pModDirectory, const char *pSourceIniFilename="new_mod_config.ini" );
void AddDefaultHalfLife2Config( bool bForce );
void AddDefaultHL2MPConfig( bool bForce );
void AddDefaultHammerIniFile();
bool AddConfig( const char *pModName, const char *pModDirectory, ModType_t modType );


#endif // CONFIGS_H
