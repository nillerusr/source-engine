//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "ModInfo.h"
#include "KeyValues.h"
#include "vgui_controls/Controls.h"
#include "FileSystem.h"
#include "EngineInterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CModInfo &ModInfo()
{
	static CModInfo s_ModInfo;
	return s_ModInfo;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CModInfo::CModInfo()
{
	m_pModData = new KeyValues("ModData");
	m_wcsGameTitle[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CModInfo::~CModInfo()
{
	FreeModInfo();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModInfo::FreeModInfo()
{
	if (m_pModData)
	{
		m_pModData->deleteThis();
		m_pModData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::IsMultiplayerOnly()
{
	return (stricmp(m_pModData->GetString("type", ""), "multiplayer_only") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::IsSinglePlayerOnly()
{
#ifndef _XBOX
	return (stricmp(m_pModData->GetString("type", ""), "singleplayer_only") == 0);
#else
	// xboxissue - no support for disparate mounted content
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModInfo::GetFallbackDir()
{
	return m_pModData->GetString("fallback_dir", "");
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const wchar_t *CModInfo::GetGameTitle()
{
	if (!m_wcsGameTitle[0])
	{
		// for some reason, the standard ILocalize::ConvertANSIToUnicode() strips off
		// the '²' character in 'HALF-LIFE²' - so just do a straight upconvert to unicode
		const char *title = m_pModData->GetString("title", "");
		int i = 0;
		for (; title[i] != 0; ++i)
		{
			m_wcsGameTitle[i] = (wchar_t)title[i];
		}
		m_wcsGameTitle[i] = 0;
	}

	return m_wcsGameTitle;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const wchar_t *CModInfo::GetGameTitle2()
{
	if (!m_wcsGameTitle2[0])
	{
		// for some reason, the standard ILocalize::ConvertANSIToUnicode() strips off
		// the '²' character in 'HALF-LIFE²' - so just do a straight upconvert to unicode
		const char *title2 = m_pModData->GetString("title2", "");
		int i = 0;
		for (; title2[i] != 0; ++i)
		{
			m_wcsGameTitle2[i] = (wchar_t)title2[i];
		}
		m_wcsGameTitle2[i] = 0;
	}

	return m_wcsGameTitle2;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModInfo::GetGameName()
{
	return m_pModData->GetString("game", "");
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
KeyValues *CModInfo::GetHiddenMaps()
{
	return m_pModData->FindKey( "hidden_maps" );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::HasPortals()
{
	return (stricmp(m_pModData->GetString("hasportals", "0"), "1") == 0);
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoDifficulty()
{
	return (stricmp(m_pModData->GetString("nodifficulty", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoModels()
{
	return (stricmp(m_pModData->GetString("nomodels", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoHiModel()
{
	return (stricmp(m_pModData->GetString("nohimodel", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoCrosshair()
{
	return (stricmp(m_pModData->GetString("nocrosshair", "1"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::AdvCrosshair()
{
	return (stricmp(m_pModData->GetString("advcrosshair", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModInfo::LoadCurrentGameInfo()
{
	// Load up gameinfo for the current mod
	char const *filename = "gameinfo.txt";
	m_pModData->LoadFromFile( g_pFullFileSystem, filename );
}

//-----------------------------------------------------------------------------
// Purpose: loads file from null-terminated buffer
//-----------------------------------------------------------------------------
void CModInfo::LoadGameInfoFromBuffer( const char *buffer )
{
	// Load up gameinfo.txt for the current mod
	m_pModData->LoadFromBuffer( "", buffer );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::UseGameLogo()
{
	return ( Q_stricmp( m_pModData->GetString( "gamelogo", "0" ), "1" ) == 0 );
}
