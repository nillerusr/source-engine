#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui_controls/ListPanel.h>
#include "SwarmopediaTopics.h"
#include "SwarmopediaPanel.h"
#include <KeyValues.h>
#include "FileSystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SwarmopediaTopics* g_pTopics = NULL;

SwarmopediaTopics* GetSwarmopediaTopics()
{
	if (!g_pTopics)
	{
		g_pTopics = new SwarmopediaTopics();
	}
	return g_pTopics;
}

SwarmopediaTopics::SwarmopediaTopics()
{
	m_pKeys = NULL;
	LoadTopics();
}

SwarmopediaTopics::~SwarmopediaTopics()
{
	if (m_pKeys)
		m_pKeys->deleteThis();
}


void SwarmopediaTopics::LoadTopics()
{
	if (m_pKeys)
	{
		Msg("Error: Attempted to load SwarmopediaTopics twice\n");
		return;
	}
	m_pKeys = new KeyValues( "TOPICS" );
	if ( !m_pKeys->LoadFromFile( g_pFullFileSystem, "resource/swarmopedia/topics.txt" ) )
	{
		Msg("Error: Couldn't fine any Swarmopedia topics!, keyvalues is empty!\n");
		// if we get here, it means the player has no mapscores.dat file and therefore no scores
		// (but that's fine)
		return;
	}
}

// fills in the target list panel with buttons for each of the entries in the desired list
bool SwarmopediaTopics::SetupList(SwarmopediaPanel *pPanel, const char *szDesiredList)
{
	// look through our KeyValues for the desired list
	KeyValues *pkvList = GetSubkeyForList(szDesiredList);
	if (!pkvList)
	{
		Msg("Swarmopedia error: Couldn't find list %s\n", szDesiredList);
		return false;
	}

	// now go through every LISTENTRY in the subkey we found, adding it to the list panel
	int iPlayerUnlockLevel = 999;	// todo: have this check a convar that measures how far through the jacob campaign the player has got
	while ( pkvList )
	{
		if (Q_stricmp(pkvList->GetName(), "LISTENTRY")==0)
		{
			const char *szEntryName = pkvList->GetString("Name");
			const char *szArticleTarget = pkvList->GetString("ArticleTarget");
			const char *szListTarget = pkvList->GetString("ListTarget");
			int iEntryUnlockLevel = pkvList->GetInt("UnlockLevel");
			int iSectionHeader = pkvList->GetInt("SectionHeader");

			if (iEntryUnlockLevel == 0 || iEntryUnlockLevel < iPlayerUnlockLevel)
			{
				pPanel->AddListEntry(szEntryName, szArticleTarget, szListTarget, iSectionHeader);
			}
		}
		pkvList = pkvList->GetNextKey();
	}
	return true;
}

KeyValues* SwarmopediaTopics::GetSubkeyForList(const char *szListName)
{
	// now go through each mission section, adding it
	KeyValues *pkvList = m_pKeys->GetFirstSubKey();
	while ( pkvList )
	{
		if (Q_stricmp(pkvList->GetName(), "LIST")==0)
		{
			const char *szName = pkvList->GetString("ListName");
			if (!Q_stricmp(szName, szListName))
				return pkvList->GetFirstSubKey();
		}
		pkvList = pkvList->GetNextKey();
	}
	return NULL;
}