#ifndef TILEGEN_LEVELTHEME_H
#define TILEGEN_LEVELTHEME_H
#ifdef _WIN32
#pragma once
#endif

#include "Utlvector.h"
#include "tier1/UtlSortVector.h"

class CRoomTemplate;

// alphabetical sorting of room templates by name
class CRoomTemplateLessFunc
{
public:
	bool Less( CRoomTemplate* const &pRoomTemplateLHS, CRoomTemplate* const &pRoomTemplateRHS, void *pCtx );
};

// class holding data about a particular level theme

class CLevelTheme
{
public:
	CLevelTheme(const char *szName, const char *szDescription, bool bRequiresVMFTweak);
	~CLevelTheme();

	char m_szName[64];				// name of this theme
	char m_szDescription[512];		// short description of the theme
	bool m_bRequiresVMFTweak;		// should the vmf_tweak step run on this theme?
	bool m_bSkipErrorCheck;
	Vector m_vecAmbientLight;		// color of ambient light

	// saves this theme to disk
	bool SaveTheme(const char *pszThemeName);		// pass just the theme name like "GreyCorridor" (no extension/path)

	void LoadRoomTemplates();	// makes this theme load in all its room templates
	CRoomTemplate* FindRoom( const char *szRoomTemplate );

	CUtlSortVector<CRoomTemplate*, CRoomTemplateLessFunc> m_RoomTemplates;		// all the room templates that belong to this theme
	
public:
	// loads in every level theme in tilegen/themes/
	static void LoadLevelThemes();
	static CLevelTheme* FindTheme( const char *szThemeName );
	static CUtlVector<CLevelTheme*> s_LevelThemes;	
	static bool s_bLoadedThemes;

	static void SetCurrentTheme(CLevelTheme* pTheme);
	static CLevelTheme* s_pCurrentTheme;
	static CLevelTheme* s_pPreviousTheme;

	static bool SplitThemeAndRoom( const char *szFullName, char *szThemeOut, int nThemeOutSize, char *szRoomOut, int nRoomOutSize );

private:
	void LoadRoomTemplatesInFolder( const char *szPath );
};

#endif TILEGEN_LEVELTHEME_H