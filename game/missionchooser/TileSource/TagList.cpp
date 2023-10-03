#include "TagList.h"
#include "asw_system.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CTagList* g_pTagList = NULL;
CTagList* TagList()
{
	if ( !g_pTagList )
	{
		g_pTagList = new CTagList();
	}
	return g_pTagList;
}

CTagList::CTagList()
{
	LoadTags();
}

CTagList::~CTagList()
{
	for (int i=0;i<m_tags.Count();i++)
	{
		delete[] m_tags[i];
		delete[] m_tagDescriptions[i];
	}
	m_tags.Purge();
	m_tagDescriptions.Purge();
}

// searches the list for the specified tag and returns our copy of it
const char* CTagList::FindTag( const char *TagName )
{
	for ( int i=0;i<m_tags.Count();i++ )
	{
		if ( !Q_stricmp( m_tags[i], TagName ) )
		{
			return m_tags[i];
		}
	}
	return NULL;
}

const char* CTagList::FindTagDescription( const char *TagName )
{
	for ( int i=0;i<m_tags.Count();i++ )
	{
		if ( !Q_stricmp( m_tags[i], TagName ) )
		{
			return m_tagDescriptions[i];
		}
	}
	return NULL;
}

// loads all taglist files
void CTagList::LoadTags()
{
	FileFindHandle_t	tagsfind = FILESYSTEM_INVALID_FIND_HANDLE;

	// Search the directory structure.
	char mapwild[MAX_PATH];
	Q_strncpy(mapwild,"tilegen/tags/*.txt", sizeof( mapwild ) );
	char const *filename;
	filename = Sys_FindFirst( tagsfind, mapwild, NULL, 0 );

	while (filename)
	{
		// load the tags list
		char szFullFileName[256];
		Q_snprintf(szFullFileName, sizeof(szFullFileName), "tilegen/tags/%s", filename);
		KeyValues *pKeyValues = new KeyValues( filename );
		if (pKeyValues->LoadFromFile(g_pFullFileSystem, szFullFileName, "GAME"))
		{
			for ( KeyValues *sub = pKeyValues->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
			{
				const char *pszTag = sub->GetName();
				char *pszTagCopy = new char[ Q_strlen( pszTag ) + 1];
				Q_strcpy( pszTagCopy, pszTag );
				m_tags.AddToTail( pszTagCopy );

				const char *pszTagDescription = sub->GetString();
				char *pszTagDescriptionCopy = new char[ Q_strlen( pszTagDescription ) + 1];
				Q_strcpy( pszTagDescriptionCopy, pszTagDescription );
				m_tagDescriptions.AddToTail( pszTagDescriptionCopy );
			}
		}
		else
		{
			Msg("Error: failed to load tags file %s\n", szFullFileName);
		}
		pKeyValues->deleteThis();
		filename = Sys_FindNext(tagsfind, NULL, 0);
	}

	Sys_FindClose( tagsfind );
}