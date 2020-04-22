//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MODINFO_H
#define MODINFO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: contains all the data entered about a mod in gameinfo.txt
//-----------------------------------------------------------------------------
class CModInfo
{
public:
	CModInfo();
	~CModInfo();
	void FreeModInfo();

	// loads mod info from gameinfo.txt
	void LoadCurrentGameInfo();

	// loads gameinfo from null-terminated string
	void LoadGameInfoFromBuffer( const char *buffer );

	// data accessors
   	const wchar_t *GetGameTitle();
   	const wchar_t *GetGameTitle2();
	const char *GetGameName();

   	bool IsMultiplayerOnly();
   	bool IsSinglePlayerOnly();

	bool HasPortals();

	bool NoDifficulty();
  	bool NoModels();
  	bool NoHiModel();
  	bool NoCrosshair();
	bool AdvCrosshair();
	int AdvCrosshairLevel();
   	const char *GetFallbackDir();
	bool UseGameLogo();
	bool UseBots();
	bool HasHDContent();
	bool SupportsVR();

	KeyValues *GetHiddenMaps();

private:
	wchar_t m_wcsGameTitle[128];
	wchar_t m_wcsGameTitle2[128];
	KeyValues *m_pModData;
};


// singleton accessor
extern CModInfo &ModInfo();

#endif // MODINFO_H
