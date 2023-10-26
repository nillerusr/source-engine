#include "asw_key_values_database.h"
#include "asw_system.h"
#include "KeyValues.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CASW_KeyValuesDatabase::m_bLoadedLocalization = false;

CASW_KeyValuesDatabase::CASW_KeyValuesDatabase()
{
}

void CASW_KeyValuesDatabase::LoadFiles( const char *pFolderName )
{
	if ( !m_bLoadedLocalization )
	{
		g_pVGuiLocalize->AddFile( "tilegen/missions_%language%.txt" );
		m_bLoadedLocalization = true;
	}
	
	m_Files.RemoveAll();

	Q_strncpy( m_RootFolder, pFolderName, sizeof( m_RootFolder ) );
	int nStrLen = Q_strlen( m_RootFolder ); 
	nStrLen;
	Assert( nStrLen > 0 && ( m_RootFolder[nStrLen - 1] == '/' || m_RootFolder[nStrLen - 1] == '\\' ) );
	LoadFilesInFolder( m_RootFolder );
}

void CASW_KeyValuesDatabase::LoadFilesInFolder( const char *pPath )
{
	FileFindHandle_t tagsfind = FILESYSTEM_INVALID_FIND_HANDLE;

	// Search the directory structure.
	char mapwild[MAX_PATH];
	Q_snprintf( mapwild, sizeof( mapwild ), "%s*", pPath );
	char const *filename;
	filename = Sys_FindFirst( tagsfind, mapwild, NULL, 0 );

	while (filename)
	{
		if ( g_pFullFileSystem->FindIsDirectory( tagsfind ) )
		{
			if ( Q_strcmp( filename, "." ) && Q_strcmp( filename, ".." ) )
			{
				char subfolder[MAX_PATH];
				Q_snprintf( subfolder, sizeof( subfolder ), "%s%s/", pPath, filename );
				LoadFilesInFolder( subfolder );
			}
		}
		else
		{
			const char *pExt = Q_GetFileExtension( filename );
			if ( pExt && !Q_stricmp( pExt, "txt" ) )
			{
				// load the mission spec
				char fullFileName[256];
				Q_snprintf(fullFileName, sizeof(fullFileName), "%s%s", pPath, filename);
				KeyValues *pKeyValues = new KeyValues( filename );
				if (pKeyValues->LoadFromFile(g_pFullFileSystem, fullFileName, "GAME"))
				{
					AddFile( pKeyValues, fullFileName );
				}
				else
				{
					Msg( "Error: failed to load file: %s\n", fullFileName );
				}
			}
		}
		filename = Sys_FindNext(tagsfind, NULL, 0);
	}

	Sys_FindClose( tagsfind );
}

KeyValues* CASW_KeyValuesDatabase::GetFileByName( const char *pFilename )
{
	for ( int i = 0; i < m_Files.Count(); i++ )
	{
		if ( Q_stricmp( pFilename, m_Files[i].m_Filename ) == 0 )
			return m_Files[i].m_pKeyValues;
	}
	return NULL;
}

KeyValues *CASW_KeyValuesDatabase::ReloadFile( const char *pFilename )
{
	for ( int i = 0; i < m_Files.Count(); i++ )
	{
		if ( !Q_stricmp( pFilename, m_Files[i].m_Filename ) )
		{
			KeyValues *pKeyValues = new KeyValues( pFilename );
			if ( pKeyValues->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" ) )
			{
				m_Files[i].m_pKeyValues->deleteThis();
				m_Files[i].m_pKeyValues = pKeyValues;
				return pKeyValues;
			}
			else
			{
				Warning( "Error: failed to reload file: %s\n", pFilename );
				break;
			}
		}	
	}
	return NULL;
}

void CASW_KeyValuesDatabase::AddFile( KeyValues *pKeyValues, const char *pFilename )
{
	m_Files.AddToTail();
	FileEntry_t *pFileEntry = &m_Files[m_Files.Count() - 1];
	Q_strncpy( pFileEntry->m_Filename, pFilename, MAX_PATH );
	Q_FixSlashes( pFileEntry->m_Filename );
	pFileEntry->m_pKeyValues = pKeyValues;

	pKeyValues->SetString( "Filename", pFileEntry->m_Filename + Q_strlen( m_RootFolder ) );
}