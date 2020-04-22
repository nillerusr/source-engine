//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef VPICKER_MAIN_H
#define VPICKER_MAIN_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <KeyValues.h>

#include "tier2/vconfig.h"
#include "ConfigManager.h"

#define	INVALID_CONFIG_ID	-1
#define	NEW_CONFIG_ID		-2

void UtlStrcpy( CUtlVector<char> &dest, const char *pSrc );

// ==============================================
// Container class for config information
// ==============================================
class CGameConfig
{
public:
	CGameConfig( void ) {}
	CGameConfig( const char *name, const char *dir )
	{
		UtlStrcpy( m_Name, name );
		UtlStrcpy( m_ModDir, dir );
	}

	CUtlVector<char> m_Name;
	CUtlVector<char> m_ModDir;
};

extern CGameConfigManager			g_ConfigManager;
extern CUtlVector<CGameConfig *>	g_Configs;

bool UpdateConfigs( void );
void ReloadConfigs( bool bNoWarning = false );
bool RemoveConfig( int configID );
bool AddConfig( int configID );
bool SaveConfigs( void );
void SetMayaScriptSettings( );
void SetXSIScriptSettings( );
void SetPathSettings( );

const char *GetBaseDirectory( void );
void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, PRINTF_FORMAT_STRING const char *pMsg, ... );


#endif // VPICKER_MAIN_H
