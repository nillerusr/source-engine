#include "LevelTheme.h"
#include "KeyValues.h"
#include "RoomTemplate.h"
#include "filesystem.h"
#include "asw_system.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CUtlVector<CLevelTheme*> CLevelTheme::s_LevelThemes;
CLevelTheme* CLevelTheme::s_pCurrentTheme = NULL;
CLevelTheme* CLevelTheme::s_pPreviousTheme = NULL;
bool CLevelTheme::s_bLoadedThemes = false;

static FileFindHandle_t	g_hthemefind = FILESYSTEM_INVALID_FIND_HANDLE;

ConVar asw_tilegen_theme( "asw_tilegen_theme", "Rydberg", FCVAR_ARCHIVE, "Default theme selected in TileGen" );

// Loads in all level themes found in tilegen/themes/
void CLevelTheme::LoadLevelThemes()
{
	if ( s_bLoadedThemes )
		return;

	s_pCurrentTheme = NULL;
	s_LevelThemes.PurgeAndDeleteElements();
	s_bLoadedThemes = true;

	// Search the directory structure.
	char mapwild[MAX_PATH];
	Q_strncpy(mapwild,"tilegen/themes/*.theme", sizeof( mapwild ) );
	char const *filename;
	filename = Sys_FindFirst( g_hthemefind, mapwild, NULL, 0 );

	while (filename)
	{
		// load the level theme
		char szFullFileName[256];
		Q_snprintf(szFullFileName, sizeof(szFullFileName), "tilegen/themes/%s", filename);
		KeyValues *pThemeKeyValues = new KeyValues( filename );
		if (pThemeKeyValues->LoadFromFile(g_pFullFileSystem, szFullFileName, "GAME"))
		{
			CLevelTheme* pTheme = new CLevelTheme(pThemeKeyValues->GetString("ThemeName"),
												pThemeKeyValues->GetString("ThemeDescription"),
												pThemeKeyValues->GetInt("VMFTweak", 0) > 0 );
			pTheme->m_bSkipErrorCheck = ( pThemeKeyValues->GetInt( "SkipErrorCheck", 0 ) > 0 );
			sscanf( pThemeKeyValues->GetString( "AmbientLight" ) , "%f %f %f", &pTheme->m_vecAmbientLight.x, &pTheme->m_vecAmbientLight.y, &pTheme->m_vecAmbientLight.z );
			// temp to avoid the 0,0,0 losing ambient light bug
			if ( pTheme->m_vecAmbientLight == vec3_origin )
			{
				pTheme->m_vecAmbientLight.x = 1;
				pTheme->m_vecAmbientLight.y = 1;
				pTheme->m_vecAmbientLight.z = 1;
			}
			pTheme->LoadRoomTemplates();
			s_LevelThemes.AddToTail(pTheme);
			if (!Q_stricmp(pTheme->m_szName, asw_tilegen_theme.GetString()))	// default to the Rydberg theme
				SetCurrentTheme(pTheme);
		}
		else
		{
			Msg("Error: failed to load theme %s\n", szFullFileName);
		}
		pThemeKeyValues->deleteThis();
		filename = Sys_FindNext(g_hthemefind, NULL, 0);
	}

	if ( s_pCurrentTheme == NULL && s_LevelThemes.Count() > 0 )
	{
		SetCurrentTheme( s_LevelThemes[0] );
	}
		
	Sys_FindClose(g_hthemefind);
}

void CLevelTheme::SetCurrentTheme(CLevelTheme* pTheme)
{
	if (s_pCurrentTheme)
		s_pPreviousTheme = s_pCurrentTheme;
	s_pCurrentTheme = pTheme;
	asw_tilegen_theme.SetValue( pTheme->m_szName );
}

CLevelTheme::CLevelTheme(const char *szName, const char *szDescription, bool bRequiresVMFTweak)
{
	m_szName[0] = '\0';
	m_szDescription[0] = '\0';
	m_bRequiresVMFTweak = bRequiresVMFTweak;
	m_vecAmbientLight = vec3_origin;

	if (szName) Q_strcpy(m_szName, szName);
	if (szDescription) Q_strcpy(m_szDescription, szDescription);
}

CLevelTheme::~CLevelTheme()
{
	m_RoomTemplates.PurgeAndDeleteElements();
}

bool CLevelTheme::SaveTheme(const char *pszThemeName)
{
	char szFullFileName[MAX_PATH];
	Q_snprintf(szFullFileName, sizeof(szFullFileName), "tilegen/themes/%s.theme", pszThemeName);
	KeyValues *pThemeKeyValues = new KeyValues( pszThemeName );
	pThemeKeyValues->SetString("ThemeName", m_szName);
	pThemeKeyValues->SetString("ThemeDescription", m_szDescription);
	pThemeKeyValues->SetBool( "VMFTweak", m_bRequiresVMFTweak );
	pThemeKeyValues->SetBool( "SkipErrorCheck", m_bSkipErrorCheck );
	char buffer[128];
	Q_snprintf( buffer, sizeof( buffer ), "%f %f %f", m_vecAmbientLight.x, m_vecAmbientLight.y, m_vecAmbientLight.z );
	pThemeKeyValues->SetString( "AmbientLight", buffer );
	if (!pThemeKeyValues->SaveToFile(g_pFullFileSystem, szFullFileName, "GAME"))
	{
		Msg("Error: Failed to save theme %s\n", szFullFileName);
		return false;
	}
	// TODO: check the room templates folder for this theme exists
	return true;
}

#define ROOMTEMPLATES_FOLDER "tilegen/roomtemplates/"

// makes this theme load in all its room templates
void CLevelTheme::LoadRoomTemplates()
{
	m_RoomTemplates.PurgeAndDeleteElements();
	char szPath[MAX_PATH];
	Q_snprintf( szPath, sizeof( szPath ), ROOMTEMPLATES_FOLDER "%s", m_szName );
	LoadRoomTemplatesInFolder( szPath );
}

void CLevelTheme::LoadRoomTemplatesInFolder( const char *szPath )
{
	char mapwild[MAX_PATH];
	Q_snprintf( mapwild, sizeof( mapwild ), "%s/*", szPath );
	char const *filename;
	FileFindHandle_t	hRoomfind = FILESYSTEM_INVALID_FIND_HANDLE;
	filename = Sys_FindFirst( hRoomfind, mapwild, NULL, 0 );
	while ( filename )
	{
		if ( g_pFullFileSystem->FindIsDirectory( hRoomfind ) )
		{
			if ( Q_strcmp( filename, "." ) && Q_strcmp( filename, ".." ) )
			{
				char subfolder[MAX_PATH];
				Q_snprintf( subfolder, sizeof( subfolder ), "%s/%s", szPath, filename );
				LoadRoomTemplatesInFolder( subfolder );
			}
		}
		else
		{	
			const char *pExt = Q_GetFileExtension( filename );
			if ( pExt && !Q_stricmp( pExt, "roomtemplate" ) )
			{
				// load the room template
				char szFullFileName[256];
				Q_snprintf(szFullFileName, sizeof(szFullFileName), "%s/%s", szPath, filename);
				KeyValues *pRoomTemplateKeyValues = new KeyValues( filename );
				if (pRoomTemplateKeyValues->LoadFromFile(g_pFullFileSystem, szFullFileName, "GAME"))
				{
					CRoomTemplate* pRoomTemplate = new CRoomTemplate(this);
					pRoomTemplate->LoadFromKeyValues( szFullFileName + Q_strlen( ROOMTEMPLATES_FOLDER ) + 1 + Q_strlen( m_szName ), pRoomTemplateKeyValues );		// sets the room templates properties based on these keyvalues
					m_RoomTemplates.Insert(pRoomTemplate);
				}
				else
				{
					Msg("Error: failed to load room template %s\n", szFullFileName);
				}
				pRoomTemplateKeyValues->deleteThis();
			}
		}
		filename = Sys_FindNext(hRoomfind, NULL, 0);
	}
		
	Sys_FindClose(hRoomfind);
}


CLevelTheme* CLevelTheme::FindTheme( const char *szThemeName )
{
	if ( !s_bLoadedThemes )
	{
		CLevelTheme::LoadLevelThemes();
	}
	// find pointer to the room template
	// first find the theme with matching name
	int iThemes = CLevelTheme::s_LevelThemes.Count();
	for (int i=0;i<iThemes;i++)
	{
		CLevelTheme* pTheme = CLevelTheme::s_LevelThemes[i];
		if (!pTheme)
			continue;
		if (!Q_stricmp(pTheme->m_szName, szThemeName))
		{
			return pTheme;
		}
	}
	return NULL;
}



bool CRoomTemplateLessFunc::Less( CRoomTemplate* const &pRoomTemplateLHS, CRoomTemplate* const &pRoomTemplateRHS, void *pCtx )
{
	return CaselessStringLessThan( pRoomTemplateLHS->GetFullName(), pRoomTemplateRHS->GetFullName() );
}

CRoomTemplate* CLevelTheme::FindRoom( const char *szRoomTemplate )
{
	// strip off .vmf if it's there
	static char buffer[ 256 ];
	Q_snprintf( buffer, sizeof( buffer ), "%s", szRoomTemplate );
	int len = Q_strlen( buffer );
	if ( len >= 4 && !Q_stricmp( buffer - 4, ".vmf" ) )
	{
		buffer[ len - 4 ] = 0;
	}
	Q_FixSlashes( buffer );

	for ( int i = 0; i < m_RoomTemplates.Count(); i++ )
	{
		CRoomTemplate *pTemplate = m_RoomTemplates[i];
		if ( !pTemplate )
			continue;

		if ( !Q_stricmp( pTemplate->GetFullName(), buffer ) )
			return pTemplate;
	}
	return NULL;
}

bool CLevelTheme::SplitThemeAndRoom( const char *pszFullName, char *szThemeOut, int nThemeOutSize, char *szRoomOut, int nRoomOutSize )
{
	const char *pszFirstForwardSlash = Q_strnchr( pszFullName, '/', Q_strlen( pszFullName ) );
	const char *pszFirstBackSlash = Q_strnchr( pszFullName, '\\', Q_strlen( pszFullName ) );

	if ( pszFirstBackSlash && pszFirstForwardSlash )
	{
		pszFirstForwardSlash = pszFirstForwardSlash < pszFirstBackSlash ? pszFirstForwardSlash : pszFirstBackSlash;
	}
	else if ( !pszFirstForwardSlash && pszFirstBackSlash )
	{
		pszFirstForwardSlash = pszFirstBackSlash;
	}
	else if ( !pszFirstForwardSlash && !pszFirstBackSlash )
	{
		return false;
	}

	Q_strncpy( szThemeOut, pszFullName, nThemeOutSize );
	int iSlashPos = pszFirstForwardSlash - pszFullName;
	if ( iSlashPos < nThemeOutSize )
	{
		szThemeOut[ iSlashPos ] = 0;
	}

	Q_strncpy( szRoomOut, pszFirstForwardSlash + 1, nRoomOutSize );
	return true;
}