#include "qlimits.h"
#include "tier1/strtools.h"
#include "tier1/utlvector.h"
#include "asw_mission_chooser_source_local.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "convar.h"
#include "asw_system.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static FileFindHandle_t	g_hmapfind = FILESYSTEM_INVALID_FIND_HANDLE;
static FileFindHandle_t	g_hcampaignfind = FILESYSTEM_INVALID_FIND_HANDLE;
static FileFindHandle_t	g_hsavedfind = FILESYSTEM_INVALID_FIND_HANDLE;

#define ASW_SKILL_POINTS_PER_MISSION 2		// keep in sync with asw_shareddefs.h (we need a h shared between missionchooser and game dlls...)

ConVar asw_max_saves("asw_max_saves", "200", FCVAR_ARCHIVE, "Maximum number of multiplayer saves that will be stored on this server.");

namespace
{
	//-----------------------------------------------------------------------------
	// Purpose: Slightly modified strtok. Does not modify the input string. Does
	//			not skip over more than one separator at a time. This allows parsing
	//			strings where tokens between separators may or may not be present:
	//
	//			Door01,,,0 would be parsed as "Door01"  ""  ""  "0"
	//			Door01,Open,,0 would be parsed as "Door01"  "Open"  ""  "0"
	//
	// Input  : token - Returns with a token, or zero length if the token was missing.
	//			str - String to parse.
	//			sep - Character to use as separator. UNDONE: allow multiple separator chars
	// Output : Returns a pointer to the next token to be parsed.
	//-----------------------------------------------------------------------------
	const char *nexttoken(char *token, const char *str, char sep)
	{
		if ((str == NULL) || (*str == '\0'))
		{
			*token = '\0';
			return(NULL);
		}

		//
		// Copy everything up to the first separator into the return buffer.
		// Do not include separators in the return buffer.
		//
		while ((*str != sep) && (*str != '\0'))
		{
			*token++ = *str++;
		}
		*token = '\0';

		//
		// Advance the pointer unless we hit the end of the input string.
		//
		if (*str == '\0')
		{
			return(str);
		}

		return(++str);
	}
};

CASW_Mission_Chooser_Source_Local::CASW_Mission_Chooser_Source_Local()
{
	for (int i=0;i<ASW_SAVES_PER_PAGE;i++)	 // this array needs to be large enough for either ASW_SAVES_PER_PAGE (for when showing save games), or ASW_MISSIONS_PER_PAGE (for when showing missions) or ASW_CAMPAIGNS_PER_SCREEN (for when showing campaigns)
	{
		Q_snprintf(m_missions[i].m_szMissionName, sizeof(m_missions[i].m_szMissionName), "");
	}
	m_bBuiltMapList = false;
	m_bBuiltCampaignList = false;
	m_bBuiltSavedCampaignList = false;

	m_bBuildingMapList = false;
	m_bBuildingCampaignList = false;
	m_bBuildingSavedCampaignList = false;

	m_pszMapFind = NULL;
	m_pszCampaignFind = NULL;
	m_pszSavedFind = NULL;
}

CASW_Mission_Chooser_Source_Local::~CASW_Mission_Chooser_Source_Local()
{
	m_MissionDetails.PurgeAndDeleteElements();
	m_CampaignDetails.PurgeAndDeleteElements();
}

void CASW_Mission_Chooser_Source_Local::OnSaveDeleted(const char *szSaveName)
{
	char fixedname[ 256 ];
	Q_strncpy( fixedname, szSaveName, sizeof( fixedname ) );
	Q_SetExtension( fixedname, ".campaignsave", sizeof( fixedname ) );
	// check if this save is in the list of summaries, if so, remove it
	for (int i=0;i<m_SavedCampaignList.Count();i++)
	{
		if (!Q_stricmp(m_SavedCampaignList[i].m_szSaveName, fixedname))
		{
			m_SavedCampaignList.Remove(i);
			return;
		}
	}
}

void CASW_Mission_Chooser_Source_Local::OnSaveUpdated(const char *szSaveName)
{
	// if we haven't started scanning for saves yet, don't worry about it
	if (!m_bBuiltSavedCampaignList && !m_bBuildingSavedCampaignList)
		return;

	// make sure it has the campaignsave extension
	char stripped[256];
	V_StripExtension(szSaveName, stripped, sizeof(stripped));
	char szWithExtension[256];
	Q_snprintf(szWithExtension, sizeof(szWithExtension), "%s.campaignsave", stripped);
	// check it's not already in the saved list
	for (int i=0;i<m_SavedCampaignList.Count();i++)
	{
		if (!Q_strcmp(m_SavedCampaignList[i].m_szSaveName, szWithExtension))
		{
			m_SavedCampaignList.Remove(i);
			break;
		}
	}
	Msg("Updating save game summary %s\n", szSaveName);
	AddToSavedCampaignList(szWithExtension);
}

void CASW_Mission_Chooser_Source_Local::RefreshSavedCampaigns()
{
	m_bBuildingSavedCampaignList = false;
	m_bBuiltSavedCampaignList = false;
	BuildSavedCampaignList();
}

void CASW_Mission_Chooser_Source_Local::ClearMapList()
{
	m_Items.Purge();
}

void CASW_Mission_Chooser_Source_Local::AddToMapList(const char *szMapName)
{
	MapListName item;
	Q_snprintf(item.szMapName, sizeof(item.szMapName), "%s", szMapName);		
	m_Items.Insert( item );

	// check if it has an overview txt
	char stripped[MAX_PATH];
	V_StripExtension( szMapName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", stripped );
	bool bHasOverviewOnHost = (g_pFullFileSystem->FileExists(tempfile));
	//bool bNoOverview = false;
	if ( !bHasOverviewOnHost )
	{
		// try to load it directly from the maps folder
		Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.txt", stripped );
		bHasOverviewOnHost = (g_pFullFileSystem->FileExists(tempfile));
	}

	// add it to our overview list if it has an overview
	if (bHasOverviewOnHost)
	{		
		m_OverviewItems.Insert( item );		
	}
}

void CASW_Mission_Chooser_Source_Local::BuildMapList()
{
	if (m_bBuildingMapList || m_bBuiltMapList)	// already building
		return;

	ClearMapList();

	m_bBuildingMapList = true;

	// Search the directory structure.
	char mapwild[MAX_QPATH];
	Q_strncpy(mapwild,"maps/*.bsp", sizeof( mapwild ) );
	m_pszMapFind = Sys_FindFirst( g_hmapfind, mapwild, NULL, 0 );
	// think will continue the search
}

void CASW_Mission_Chooser_Source_Local::Think()
{
	// if we're building the map list, continue our search of files
	if (m_bBuildingMapList)
	{
		if (m_pszMapFind)
		{
			//Msg("Adding map: %s\n", m_pszMapFind);
			AddToMapList(m_pszMapFind);
			m_pszMapFind = Sys_FindNext(g_hmapfind, NULL, 0);
		}
		else
		{
			//Msg("Ending search for maps\n");
			Sys_FindClose(g_hmapfind);
			m_bBuildingMapList = false;
			m_bBuiltMapList= true;
		}
	}
	// if we're building the campaign list, continue our search of files
	if (m_bBuildingCampaignList)
	{
		if (m_pszCampaignFind)
		{
			//Msg("Adding campaign: %s\n", m_pszCampaignFind);
			AddToCampaignList(m_pszCampaignFind);
			m_pszCampaignFind = Sys_FindNext(g_hcampaignfind, NULL, 0);
		}
		else
		{
			//Msg("Ending search for campaigns\n");
			Sys_FindClose(g_hcampaignfind);
			m_bBuildingCampaignList = false;
			m_bBuiltCampaignList= true;
		}
	}
	// if we're building the saved campaign list, continue our search of files
	if (m_bBuildingSavedCampaignList)
	{
		if (m_pszSavedFind)
		{
			//Msg("Adding saved campaign: %s\n", m_pszSavedFind);
			AddToSavedCampaignList(m_pszSavedFind);
			m_pszSavedFind = Sys_FindNext(g_hsavedfind, NULL, 0);
		}
		else
		{
			//Msg("Ending search for saved campaigns\n");
			Sys_FindClose(g_hsavedfind);
			m_bBuildingSavedCampaignList = false;
			m_bBuiltSavedCampaignList= true;
		}
	}
}

// called when server is in a relatively idle state (like during the briefing)
//  we can use this time to scan for missions if we need to
void CASW_Mission_Chooser_Source_Local::IdleThink()
{
	// if we're not currently building any lists, try to start building one
	if (!m_bBuildingMapList && !m_bBuildingCampaignList && !m_bBuildingSavedCampaignList)
	{
		if (!m_bBuiltMapList)
		{
			BuildMapList();
		}
		else
		{
			if (!m_bBuiltCampaignList)
			{
				BuildCampaignList();
			}
			else if (!m_bBuiltSavedCampaignList)
			{
				BuildSavedCampaignList();
			}
		}
	}
	Think();
}

void CASW_Mission_Chooser_Source_Local::FindMissionsInCampaign( int iCampaignIndex, int nMissionOffset, int iNumSlots )
{
	if (!m_bBuiltMapList)
		BuildMapList();

	ASW_Mission_Chooser_Mission* pCampaign = GetCampaign( iCampaignIndex );
	if ( !pCampaign )
		return;

	KeyValues *pCampaignDetails = GetCampaignDetails( pCampaign->m_szMissionName );
	if ( !pCampaignDetails )
		return;

	CUtlVector<KeyValues*> aMissionKeys;
	bool bSkippedFirst = false;
	for ( KeyValues *pMission = pCampaignDetails->GetFirstSubKey(); pMission; pMission = pMission->GetNextKey() )
	{
		if ( !Q_stricmp( pMission->GetName(), "MISSION" ) )
		{
			if ( !bSkippedFirst )
			{
				bSkippedFirst = true;
			}
			else
			{
				aMissionKeys.AddToTail( pMission );
			}
		}
	}
	
	int max_items = aMissionKeys.Count();
	for ( int stored=0;stored<iNumSlots;stored++ )
	{
		int realoffset = nMissionOffset;
		if ( realoffset >= max_items || realoffset < 0 )
		{
			// no more missions...
			Q_snprintf( m_missions[stored].m_szMissionName, sizeof( m_missions[stored].m_szMissionName ), "" );
		}
		else
		{
			Q_snprintf( m_missions[stored].m_szMissionName, sizeof( m_missions[stored].m_szMissionName ), "%s", aMissionKeys[realoffset]->GetString( "MapName" ) );
		}
		nMissionOffset++;
	}
}

int CASW_Mission_Chooser_Source_Local::GetNumMissionsInCampaign( int iCampaignIndex )
{
	if (!m_bBuiltMapList)
		BuildMapList();

	ASW_Mission_Chooser_Mission* pCampaign = GetCampaign( iCampaignIndex );
	if ( !pCampaign )
		return 0;

	KeyValues *pCampaignDetails = GetCampaignDetails( pCampaign->m_szMissionName );
	if ( !pCampaignDetails )
		return 0;

	CUtlVector<KeyValues*> aMissionKeys;
	bool bSkippedFirst = false;
	for ( KeyValues *pMission = pCampaignDetails->GetFirstSubKey(); pMission; pMission = pMission->GetNextKey() )
	{
		if ( !Q_stricmp( pMission->GetName(), "MISSION" ) )
		{
			if ( !bSkippedFirst )
			{
				bSkippedFirst = true;
			}
			else
			{
				aMissionKeys.AddToTail( pMission );
			}
		}
	}

	return aMissionKeys.Count();
}

void CASW_Mission_Chooser_Source_Local::FindMissions(int nMissionOffset, int iNumSlots, bool bRequireOverview)
{
	if (!m_bBuiltMapList)
		BuildMapList();
	
	int max_items = bRequireOverview ? m_OverviewItems.Count() : m_Items.Count();
	for (int stored=0;stored<iNumSlots;stored++)
	{
		int realoffset = nMissionOffset;
		if (realoffset >= max_items || realoffset < 0)
		{
			// no more missions...
			Q_snprintf(m_missions[stored].m_szMissionName, sizeof(m_missions[stored].m_szMissionName), "");
		}
		else
		{
			if (bRequireOverview)
			{
				
				Q_snprintf(m_missions[stored].m_szMissionName, sizeof(m_missions[stored].m_szMissionName), "%s", m_OverviewItems[realoffset].szMapName);
			}
			else			
			{
				Q_snprintf(m_missions[stored].m_szMissionName, sizeof(m_missions[stored].m_szMissionName), "%s", m_Items[realoffset].szMapName);
			}
		}
		nMissionOffset++;
	}
}


// pass an array of mission names back
ASW_Mission_Chooser_Mission* CASW_Mission_Chooser_Source_Local::GetMissions()
{
	return m_missions;
}

ASW_Mission_Chooser_Mission* CASW_Mission_Chooser_Source_Local::GetMission( int nIndex, bool bRequireOverview )
{
	if (!m_bBuiltMapList)
		BuildMapList();

	int max_items = bRequireOverview ? m_OverviewItems.Count() : m_Items.Count();
	static ASW_Mission_Chooser_Mission mission;
	if (nIndex >= max_items || nIndex < 0)
	{
		// no more missions...
		Q_snprintf(mission.m_szMissionName, sizeof(mission.m_szMissionName), "");
	}
	else
	{
		if (bRequireOverview)
		{

			Q_snprintf(mission.m_szMissionName, sizeof(mission.m_szMissionName), "%s", m_OverviewItems[nIndex].szMapName);
		}
		else			
		{
			Q_snprintf(mission.m_szMissionName, sizeof(mission.m_szMissionName), "%s", m_Items[nIndex].szMapName);
		}
	}
	return &mission;
}

int	 CASW_Mission_Chooser_Source_Local::GetNumMissions(bool bRequireOverview)
{
	if (!m_bBuiltMapList)
		BuildMapList();

	if (bRequireOverview)
		return m_OverviewItems.Count();
	return m_Items.Count();
}

void CASW_Mission_Chooser_Source_Local::ClearCampaignList()
{
	m_CampaignList.Purge();
}

void CASW_Mission_Chooser_Source_Local::AddToCampaignList(const char *szCampaignName)
{
	MapListName item;
	Q_snprintf(item.szMapName, sizeof(item.szMapName), "%s", szCampaignName);		
	m_CampaignList.Insert( item );
}

void CASW_Mission_Chooser_Source_Local::BuildCampaignList()
{
	if (m_bBuildingCampaignList || m_bBuiltCampaignList)	// already building
		return;

	ClearCampaignList();

	m_bBuildingCampaignList= true;

	// Search the directory structure.
	char mapwild[MAX_QPATH];
	Q_strncpy(mapwild,"resource/campaigns/*.txt", sizeof( mapwild ) );
	m_pszCampaignFind = Sys_FindFirst( g_hcampaignfind, mapwild, NULL, 0 );
	// think will continue the search
}

void CASW_Mission_Chooser_Source_Local::FindCampaigns(int nCampaignOffset, int iNumSlots)
{
	if (!m_bBuiltCampaignList)
		BuildCampaignList();
	
	int max_items = m_CampaignList.Count();	
	for (int stored=0;stored<iNumSlots;stored++)
	{
		int realoffset = nCampaignOffset;
		if (stored < ASW_CAMPAIGNS_PER_PAGE)
		{
			if (realoffset >= max_items || realoffset < 0)
			{
				Q_snprintf(m_campaigns[stored].m_szMissionName, sizeof(m_campaigns[stored].m_szMissionName), "");
			}
			else
			{	
				Q_snprintf(m_campaigns[stored].m_szMissionName, sizeof(m_campaigns[stored].m_szMissionName), "%s", m_CampaignList[realoffset].szMapName);

			}		
			nCampaignOffset++;
		}
	}
}

// Passes an array of campaign names back
ASW_Mission_Chooser_Mission* CASW_Mission_Chooser_Source_Local::GetCampaigns()
{
	if (!m_bBuiltCampaignList)
		BuildCampaignList();
	return m_campaigns;
}

ASW_Mission_Chooser_Mission* CASW_Mission_Chooser_Source_Local::GetCampaign( int nIndex )
{
	if (!m_bBuiltCampaignList)
		BuildCampaignList();

	static ASW_Mission_Chooser_Mission campaign;
	int max_items = m_CampaignList.Count();	
	if (nIndex >= max_items || nIndex < 0)
	{
		Q_snprintf(campaign.m_szMissionName, sizeof(campaign.m_szMissionName), "");
	}
	else
	{	
		Q_snprintf(campaign.m_szMissionName, sizeof(campaign.m_szMissionName), "%s", m_CampaignList[nIndex].szMapName);

	}	
	return &campaign;
}

int	 CASW_Mission_Chooser_Source_Local::GetNumCampaigns()
{
	if (!m_bBuiltCampaignList)
		BuildCampaignList();

	return m_CampaignList.Count();
}

// todo: accept a steam ID and only find saves from that person?
void CASW_Mission_Chooser_Source_Local::FindSavedCampaigns( int nSaveOffset, int iNumSlots, bool bMultiplayer, const char *szFilterID)
{
	if (!m_bBuiltSavedCampaignList)
		BuildSavedCampaignList();
	int skip_entries = nSaveOffset;	// how many filter matching entries we're skipping
	int max_items = m_SavedCampaignList.Count();
	int stored = 0;
	int offset = 0;

	// count the offset up until we've skipped the desired number of filter matching entries
	while (skip_entries > 0 && offset < max_items)
	{
		if (bMultiplayer == m_SavedCampaignList[offset].m_bMultiplayer && SavePassesFilter(&m_SavedCampaignList[offset], szFilterID))
		{
			skip_entries--;
		}
		offset++;
	}

	while (stored < iNumSlots && offset < max_items)
	{
		//Msg("testing save %d multi = %d we want multi = %d\n", offset, m_SavedCampaignList[offset].m_bMultiplayer, bMultiplayer);
		if (bMultiplayer == m_SavedCampaignList[offset].m_bMultiplayer && SavePassesFilter(&m_SavedCampaignList[offset], szFilterID))
		{
			Q_snprintf(m_savedcampaigns[stored].m_szSaveName, sizeof(m_savedcampaigns[stored].m_szSaveName), "%s", m_SavedCampaignList[offset].m_szSaveName);
			Q_snprintf(m_savedcampaigns[stored].m_szCampaignName, sizeof(m_savedcampaigns[stored].m_szCampaignName), "%s", m_SavedCampaignList[offset].m_szCampaignName);
			Q_snprintf(m_savedcampaigns[stored].m_szDateTime, sizeof(m_savedcampaigns[stored].m_szDateTime), "%s", m_SavedCampaignList[offset].m_szDateTime);
			Q_snprintf(m_savedcampaigns[stored].m_szPlayerNames, sizeof(m_savedcampaigns[stored].m_szPlayerNames), "%s", m_SavedCampaignList[offset].m_szPlayerNames);
			m_savedcampaigns[stored].m_iMissionsComplete = m_SavedCampaignList[offset].m_iMissionsComplete;
			stored++;
		}
		offset++;
	}

	// blank out any slots that didn't get filled
	for (int i=stored;i<iNumSlots;i++)
	{
		Q_snprintf(m_savedcampaigns[i].m_szSaveName, sizeof(m_savedcampaigns[i].m_szSaveName), "");
		Q_snprintf(m_savedcampaigns[i].m_szCampaignName, sizeof(m_savedcampaigns[i].m_szCampaignName), "");
		Q_snprintf(m_savedcampaigns[i].m_szDateTime, sizeof(m_savedcampaigns[i].m_szDateTime), "");
		Q_snprintf(m_savedcampaigns[i].m_szPlayerNames, sizeof(m_savedcampaigns[i].m_szPlayerNames), "");
		m_savedcampaigns[i].m_iMissionsComplete = 0;		
	}
}

ASW_Mission_Chooser_Saved_Campaign* CASW_Mission_Chooser_Source_Local::GetSavedCampaigns()
{
	if (!m_bBuiltSavedCampaignList)
		BuildSavedCampaignList();

	return m_savedcampaigns;
}

ASW_Mission_Chooser_Saved_Campaign* CASW_Mission_Chooser_Source_Local::GetSavedCampaign( int nIndex, bool bMultiplayer, const char *szFilterID )
{
	if (!m_bBuiltSavedCampaignList)
		BuildSavedCampaignList();

	int skip_entries = nIndex;	// how many filter matching entries we're skipping
	int max_items = m_SavedCampaignList.Count();
	int offset = 0;

	// count the offset up until we've skipped the desired number of filter matching entries
	while (skip_entries > 0 && offset < max_items)
	{
		if (bMultiplayer == m_SavedCampaignList[offset].m_bMultiplayer && SavePassesFilter(&m_SavedCampaignList[offset], szFilterID))
		{
			skip_entries--;
		}
		offset++;
	}

	static ASW_Mission_Chooser_Saved_Campaign save;
	Q_snprintf(save.m_szSaveName, sizeof(save.m_szSaveName), "%s", m_SavedCampaignList[offset].m_szSaveName);
	Q_snprintf(save.m_szCampaignName, sizeof(save.m_szCampaignName), "%s", m_SavedCampaignList[offset].m_szCampaignName);
	Q_snprintf(save.m_szDateTime, sizeof(save.m_szDateTime), "%s", m_SavedCampaignList[offset].m_szDateTime);
	Q_snprintf(save.m_szPlayerNames, sizeof(save.m_szPlayerNames), "%s", m_SavedCampaignList[offset].m_szPlayerNames);
	save.m_iMissionsComplete = m_SavedCampaignList[offset].m_iMissionsComplete;

	return &save;
}

bool CASW_Mission_Chooser_Source_Local::SavePassesFilter(ASW_Mission_Chooser_Saved_Campaign* pSaved, const char *szFilterID)
{
	if (!pSaved)
		return false;

	if (!szFilterID || Q_strlen(szFilterID) < 1)
		return true;

	// check our filterID is present in the playerids with this save
	const char	*p = pSaved->m_szPlayerIDs;
	char		token[128];
	
	p = nexttoken( token, p, ' ' );

	while ( Q_strlen( token ) > 0 )  
	{
		// found a match
		if (!Q_stricmp(szFilterID, token))
			return true;
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';
	}
	

	return false;
}

int	 CASW_Mission_Chooser_Source_Local::GetNumSavedCampaigns(bool bMultiplayer, const char *szFilterID)
{
	int iNumSaves = 0;
	for (int i=0;i<m_SavedCampaignList.Count();i++)
	{
		if (m_SavedCampaignList[i].m_bMultiplayer == bMultiplayer && SavePassesFilter(&m_SavedCampaignList[i], szFilterID))
		{
			iNumSaves++;
		}
	}

	return iNumSaves;
}

void CASW_Mission_Chooser_Source_Local::ClearSavedCampaignList()
{
	m_SavedCampaignList.Purge();
}

void CASW_Mission_Chooser_Source_Local::AddToSavedCampaignList(const char *szSaveName)
{
	// find out what campaign this save is for
	char szFullFileName[256];
	Q_snprintf(szFullFileName, sizeof(szFullFileName), "save/%s", szSaveName);
	KeyValues *pSaveKeyValues = new KeyValues( szSaveName );
	if (pSaveKeyValues->LoadFromFile(g_pFullFileSystem, szFullFileName))
	{
		const char *pCampaignName = pSaveKeyValues->GetString("CampaignName");
		// check the campaign exists
		char tempfile[MAX_PATH];
		Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", pCampaignName );
		if (g_pFullFileSystem->FileExists(tempfile))
		{
			ASW_Mission_Chooser_Saved_Campaign item;
			Q_snprintf(item.m_szSaveName, sizeof(item.m_szSaveName), "%s", szSaveName);
			Q_snprintf(item.m_szCampaignName, sizeof(item.m_szCampaignName), "%s", pCampaignName);
			Q_snprintf(item.m_szDateTime, sizeof(item.m_szDateTime), "%s", pSaveKeyValues->GetString("DateTime"));
			//Msg("save %s multiplayer %d\n", szSaveName, pSaveKeyValues->GetInt("Multiplayer"));
			item.m_bMultiplayer = (pSaveKeyValues->GetInt("Multiplayer") > 0);
			//Msg("item multiplayer = %d\n", item.m_bMultiplayer);

			// check subsections for player names and player IDs, concat them into two strings
			char namebuffer[256];
			char idbuffer[512];
			namebuffer[0] = '\0';
			idbuffer[0] = '\0';
			int namepos = 0;
			int idpos = 0;
			KeyValues *pkvSubSection = pSaveKeyValues->GetFirstSubKey();
			while ( pkvSubSection )
			{
				if ((Q_stricmp(pkvSubSection->GetName(), "PLAYER")==0) && namepos < 253)
				{
					const char *pName = pkvSubSection->GetString("PlayerName");
					if (pName && pName[0] != '\0')
					{
						int namelength = Q_strlen(pName);
						for (int charcopy=0; charcopy<namelength && namepos<253; charcopy++)
						{
							namebuffer[namepos] = pName[charcopy];
							namepos++;
						}
						namebuffer[namepos] = ' '; namepos++;
						namebuffer[namepos] = '\0';
					}
				}
				if ((Q_stricmp(pkvSubSection->GetName(), "DATA")==0) && idpos < 253)
				{
					const char *pID = pkvSubSection->GetString("DataBlock");
					if (pID && pID[0] != '\0')
					{
						int idlength = Q_strlen(pID);
						for (int charcopy=0; charcopy<idlength && idpos<253; charcopy++)
						{
							idbuffer[idpos] = pID[charcopy];
							idpos++;
						}
						idbuffer[idpos] = ' '; idpos++;
						idbuffer[idpos] = '\0';
					}
				}
				pkvSubSection = pkvSubSection->GetNextKey();
			}
			Q_snprintf(item.m_szPlayerNames, sizeof(item.m_szPlayerNames), "%s", namebuffer);
			Q_snprintf(item.m_szPlayerIDs, sizeof(item.m_szPlayerIDs), "%s", idbuffer);
			item.m_iMissionsComplete = pSaveKeyValues->GetInt("NumMissionsComplete");
			m_SavedCampaignList.Insert( item );
		}
	}
	pSaveKeyValues->deleteThis();

	// check if there's now too many save games
	int iNumMultiplayer = GetNumSavedCampaigns(true, NULL);
	if (iNumMultiplayer > asw_max_saves.GetInt())
	{
		// find the oldest one
		ASW_Mission_Chooser_Saved_Campaign* pChosen = false;
		int iChosen = -1;
		for (int i=m_SavedCampaignList.Count()-1; i>=0; i--)
		{
			if (m_SavedCampaignList[i].m_bMultiplayer)
			{
				pChosen = &m_SavedCampaignList[i];
				iChosen = i;
				break;
			}			
		}
		// delete if found
		if (iChosen != -1 && pChosen)
		{
			char buffer[MAX_PATH];
			Q_snprintf(buffer, sizeof(buffer), "save/%s", pChosen->m_szSaveName);
			Msg("Deleting save %s as we have too many\n", buffer);
			g_pFullFileSystem->RemoveFile( buffer, "GAME" );
			m_SavedCampaignList.Remove(iChosen);
		}
	}
}

void CASW_Mission_Chooser_Source_Local::BuildSavedCampaignList()
{
	// don't start searching for saves until we've loaded in all the campaigns
	if (!m_bBuiltCampaignList)
	{
		BuildCampaignList();
		return;
	}
	if (m_bBuildingSavedCampaignList || m_bBuiltSavedCampaignList)
		return;

	ClearSavedCampaignList();

	m_bBuildingSavedCampaignList = true;

	if (m_CampaignList.Count() <= 0)
	{
		//Msg("Error: Cannot build saved campaign list until the campaign list is built\n");
		return;
	}

	// Search the directory structure for save games
	char mapwild[MAX_QPATH];
	Q_strncpy(mapwild,"save/*.campaignsave", sizeof( mapwild ) );
	m_pszSavedFind = Sys_FindFirst( g_hsavedfind, mapwild, NULL, 0 );
	// think will continue the search
}

bool CASW_Mission_Chooser_Source_Local::MissionExists(const char *szMapName, bool bRequireOverview)
{
	if ( !szMapName )
		return false;

	// check it has an overview txt
	char stripped[MAX_PATH];
	V_StripExtension( szMapName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	if (bRequireOverview)
	{
		Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", stripped );
		if (!g_pFullFileSystem->FileExists(tempfile))
			return false;
	}

	// check the map exists	
	Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.bsp", stripped );
	return (g_pFullFileSystem->FileExists(tempfile));
}

bool CASW_Mission_Chooser_Source_Local::CampaignExists(const char *szCampaignName)
{
	if ( !szCampaignName )
		return false;

	// check the campaign txt exists
	char stripped[MAX_PATH];
	V_StripExtension( szCampaignName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", stripped );
	return (g_pFullFileSystem->FileExists(tempfile));
}

bool CASW_Mission_Chooser_Source_Local::SavedCampaignExists(const char *szSaveName)
{
	// check the save file exists
	char stripped[MAX_PATH];
	V_StripExtension( szSaveName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "save/%s.campaignsave", stripped );
	return (g_pFullFileSystem->FileExists(tempfile));
}

int CASW_Mission_Chooser_Source_Local::GetNumMissionsCompleted(const char *szSaveName)
{
	Msg("GetNumMissionsCompleted %s\n", szSaveName);
	// check the save file exists
	char stripped[MAX_PATH];
	V_StripExtension( szSaveName, stripped, MAX_PATH );
	Msg("  stripped = %s\n", stripped);
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "save/%s.campaignsave", stripped );
	Msg("  tempfile = %s\n", tempfile);
	Q_strlower( tempfile );
	Msg("  tempfile lowered = %s\n", tempfile);
	if (!g_pFullFileSystem->FileExists(tempfile))
	{
		Msg("  this save doesn't exist! returning -1 missions\n");		
		return -1;
	}

	KeyValues *pSaveKeyValues = new KeyValues( szSaveName );
	if (pSaveKeyValues->LoadFromFile(g_pFullFileSystem, tempfile))
	{
		int iMissions = pSaveKeyValues->GetInt("NumMissionsComplete");
		pSaveKeyValues->deleteThis();
		Msg(" loaded keyvalues from file and it thinks num missions is %d\n", iMissions);
		return iMissions;
	}
	
	Msg("  Couldn't load save keyvalues from file, returning -1\n");
	if (pSaveKeyValues->LoadFromFile(g_pFullFileSystem, tempfile, "MOD"))
		Msg("  but it loaded if we use the MOD path\n");
	else if (pSaveKeyValues->LoadFromFile(g_pFullFileSystem, tempfile, "GAME"))
		Msg("  but it loaded if we use the GAME path\n");
	pSaveKeyValues->deleteThis();
	return -1;
}

const char* CASW_Mission_Chooser_Source_Local::GetPrettyMissionName(const char *szMapName)
{
	static char szPrettyName[64];
	szPrettyName[0] = '\0';

	char stripped[MAX_PATH];
	V_StripExtension( szMapName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", stripped );

	KeyValues *pOverviewKeyValues = new KeyValues( szMapName );
	if (pOverviewKeyValues->LoadFromFile(g_pFullFileSystem, tempfile))
	{
		Q_snprintf(szPrettyName, sizeof(szPrettyName), "%s", pOverviewKeyValues->GetString("missiontitle"));
	}
	pOverviewKeyValues->deleteThis();

	return szPrettyName;
}

const char* CASW_Mission_Chooser_Source_Local::GetPrettyCampaignName(const char *szCampaignName)
{
	static char szPrettyName[64];
	szPrettyName[0] = '\0';

	char stripped[MAX_PATH];
	V_StripExtension( szCampaignName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", stripped );

	KeyValues *pCampaignKeyValues = new KeyValues( szCampaignName );
	if (pCampaignKeyValues->LoadFromFile(g_pFullFileSystem, tempfile))
	{
		Q_snprintf(szPrettyName, sizeof(szPrettyName), "%s", pCampaignKeyValues->GetString("CampaignName"));
	}
	pCampaignKeyValues->deleteThis();

	return szPrettyName;
}

const char* CASW_Mission_Chooser_Source_Local::GetPrettySavedCampaignName(const char *szSaveName)
{
	static char szPrettyName[256];
	szPrettyName[0] = '\0';

	char stripped[MAX_PATH];
	V_StripExtension( szSaveName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "save/%s.campaignsave", stripped );

	KeyValues *pSaveKeyValues = new KeyValues( szSaveName );
	if (pSaveKeyValues->LoadFromFile(g_pFullFileSystem, tempfile))
	{
		const char *szCampaignName = pSaveKeyValues->GetString("CampaignName");
		const char *szPrettyCampaignName = szCampaignName;
		if (szCampaignName && Q_strlen(szCampaignName) > 0)
		{
			szPrettyCampaignName = GetPrettyCampaignName(szCampaignName);
		}
		const char *szDate = pSaveKeyValues->GetString("DateTime");
		char namebuffer[256];
		namebuffer[0] = '\0';
		int namepos = 0;
		KeyValues *pkvSubSection = pSaveKeyValues->GetFirstSubKey();
		while ( pkvSubSection && namepos < 253)
		{
			if (Q_stricmp(pkvSubSection->GetName(), "PLAYER")==0)
			{
				const char *pName = pkvSubSection->GetString("PlayerName");
				if (pName && pName[0] != '\0')
				{
					if (namepos != 0)
					{
						namebuffer[namepos] = ' '; namepos++;
					}
					int namelength = Q_strlen(pName);
					for (int charcopy=0; charcopy<namelength && namepos<253; charcopy++)
					{
						namebuffer[namepos] = pName[charcopy];
						namepos++;
					}
					namebuffer[namepos] = '\0';
				}
			}
			pkvSubSection = pkvSubSection->GetNextKey();
		}
		Q_snprintf(szPrettyName, sizeof(szPrettyName),
			"%s (%s) (%s)",
			szPrettyCampaignName, szDate, namebuffer);
	}
	pSaveKeyValues->deleteThis();

	return szPrettyName;
}

// a new save has been created, add it to our summary list
void CASW_Mission_Chooser_Source_Local::NotifyNewSave(const char *szSaveName)
{
	// if we haven't started scanning for saves yet, don't worry about it
	if (!m_bBuiltSavedCampaignList && !m_bBuildingSavedCampaignList)
		return;

	// make sure it has the campaignsave extension
	char stripped[256];
	V_StripExtension(szSaveName, stripped, sizeof(stripped));
	char szWithExtension[256];
	Q_snprintf(szWithExtension, sizeof(szWithExtension), "%s.campaignsave", stripped);
	// check it's not already in the saved list
	for (int i=0;i<m_SavedCampaignList.Count();i++)
	{
		if (!Q_strcmp(m_SavedCampaignList[i].m_szSaveName, szWithExtension))
			return;
	}
	Msg("New save created, adding it to the list of saved campaigns: %s\n", szSaveName);
	AddToSavedCampaignList(szWithExtension);
}

// a new save has been created, add it to our summary list
void CASW_Mission_Chooser_Source_Local::NotifySaveDeleted(const char *szSaveName)
{
	// if we haven't started scanning for saves yet, don't worry about it
	if (!m_bBuiltSavedCampaignList && !m_bBuildingSavedCampaignList)
		return;

	// make sure it has the campaignsave extension
	char stripped[256];
	V_StripExtension(szSaveName, stripped, sizeof(stripped));
	char szWithExtension[256];
	Q_snprintf(szWithExtension, sizeof(szWithExtension), "%s.campaignsave", stripped);
	// find it in the list
	for (int i=0;i<m_SavedCampaignList.Count();i++)
	{
		if (!Q_strcmp(m_SavedCampaignList[i].m_szSaveName, szWithExtension))
		{
			m_SavedCampaignList.Remove(i);
			return;
		}
	}	
}

// !!NOTE!! - these numbers are from asw_campaign_save.h.  Duplicated constants for the lose.
#define ASW_MAX_PLAYERS_PER_SAVE 10
#define ASW_MAX_MISSIONS_PER_CAMPAIGN 32
#define ASW_CURRENT_SAVE_VERSION 1
#define ASW_NUM_MARINE_PROFILES 9	// from asw_shareddefs.h

bool CASW_Mission_Chooser_Source_Local::ASW_Campaign_CreateNewSaveGame(char *szFileName, int iFileNameMaxLen, const char *szCampaignName, bool bMultiplayerGame, const char *szStartingMission)	// szFileName arg is the desired filename or NULL for an autogenerated one.  Function sets szFileName with the filename used.
{
	if (!szFileName)
		return false;

	char stripped[MAX_PATH];
	V_StripExtension( szCampaignName, stripped, MAX_PATH );

	char szStartingMissionStripped[64];
	if ( szStartingMission )
	{
		V_StripExtension( szStartingMission, szStartingMissionStripped, sizeof( szStartingMissionStripped ) );
	}
	else
	{
		szStartingMissionStripped[0] = 0;
	}

	// check the campaign file exists
	char campbuffer[MAX_PATH];
	Q_snprintf(campbuffer, sizeof(campbuffer), "resource/campaigns/%s.txt", stripped);
	if (!g_pFullFileSystem->FileExists(campbuffer))
	{
		Msg("No such campaign: %s\n", campbuffer);
		return false;
	}

	// Get the current time and date as a string
	char szDateTime[256];
	int year, month, dayOfWeek, day, hour, minute, second;
	ASW_System_GetCurrentTimeAndDate(&year, &month, &dayOfWeek, &day, &hour, &minute, &second);
	Q_snprintf(szDateTime, sizeof(szDateTime), "%02d/%02d/%02d %02d:%02d", month, day, year, hour, minute);

	if (szFileName[0] == '\0')
	{
		// autogenerate a filename based on the current time and date
		Q_snprintf(szFileName, iFileNameMaxLen, "%s_save_%02d_%02d_%02d_%02d_%02d_%02d", stripped, year, month, day, hour, minute, second);
	}

	// make sure the path and extension are correct
	Q_SetExtension( szFileName, ".campaignsave", iFileNameMaxLen );
	char tempbuffer[256];
	Q_snprintf(tempbuffer, sizeof(tempbuffer), "%s", szFileName);
	const char *pszNoPathName = Q_UnqualifiedFileName(tempbuffer);
	Msg("Unqualified = %s\n", pszNoPathName);
	char szFullFileName[256];
	Q_snprintf(szFullFileName, sizeof(szFullFileName), "save/%s", pszNoPathName);
	Msg("Creating new save with filename: %s\n", szFullFileName);

	KeyValues *pSaveKeyValues = new KeyValues( pszNoPathName );

	int nMissionsComplete = 0;
	if ( szStartingMission && szStartingMission[0] )
	{
		KeyValues *pCampaignDetails = GetCampaignDetails( stripped );
		if ( pCampaignDetails )
		{
			int nMissionSections = 0;
			for ( KeyValues *pMission = pCampaignDetails->GetFirstSubKey(); pMission; pMission = pMission->GetNextKey() )
			{
				if ( !Q_stricmp( pMission->GetName(), "MISSION" ) )
				{
					if ( !Q_stricmp( pMission->GetString( "MapName", "" ), szStartingMissionStripped ) )
					{
						nMissionsComplete = nMissionSections - 1;		// skip first dummy mission
						break;
					}
					nMissionSections++;
				}
			}
		}
	}
	
	pSaveKeyValues->SetInt("Version", ASW_CURRENT_SAVE_VERSION);
	pSaveKeyValues->SetString("CampaignName", stripped);
	pSaveKeyValues->SetInt("CurrentPosition", nMissionsComplete + 1);		// position squad on the first uncompleted mission
	pSaveKeyValues->SetInt("NumMissionsComplete", nMissionsComplete);
	pSaveKeyValues->SetInt("InitialNumMissionsComplete", nMissionsComplete);
	pSaveKeyValues->SetInt("Multiplayer", bMultiplayerGame ? 1 : 0);
	pSaveKeyValues->SetString("DateTime", szDateTime);
	pSaveKeyValues->SetInt("NumPlayers", 0);	
	
	// write out each mission's status
	KeyValues *pSubSection;
	for (int i=0; i<ASW_MAX_MISSIONS_PER_CAMPAIGN; i++)
	{
		pSubSection = new KeyValues("MISSION");
		pSubSection->SetInt("MissionID", i);
		bool bComplete = ( i != 0 ) && ( i <= nMissionsComplete );
		pSubSection->SetInt("MissionComplete", bComplete ? 1 : 0 );
		pSaveKeyValues->AddSubKey(pSubSection);
	}

	const int nInitialSkillPoints = 0;

	// write out each marine's stats
	for (int i=0; i<ASW_NUM_MARINE_PROFILES; i++)
	{
		pSubSection = new KeyValues("MARINE");
		pSubSection->SetInt("MarineID", i);
		pSubSection->SetInt("SkillSlot0", 0);
		pSubSection->SetInt("SkillSlot1", 0);
		pSubSection->SetInt("SkillSlot2", 0);
		pSubSection->SetInt("SkillSlot3", 0);
		pSubSection->SetInt("SkillSlot4", 0);
		
		pSubSection->SetInt("SkillSlotSpare", nInitialSkillPoints + nMissionsComplete * ASW_SKILL_POINTS_PER_MISSION );

		pSubSection->SetInt("UndoSkillSlot0", 0);
		pSubSection->SetInt("UndoSkillSlot1", 0);
		pSubSection->SetInt("UndoSkillSlot2", 0);
		pSubSection->SetInt("UndoSkillSlot3", 0);
		pSubSection->SetInt("UndoSkillSlot4", 0);
		
		pSubSection->SetInt("UndoSkillSlotSpare", nInitialSkillPoints + nMissionsComplete * ASW_SKILL_POINTS_PER_MISSION );
		pSubSection->SetString("MissionsCompleted", "");
		pSubSection->SetString("Medals", "");
		pSubSection->SetInt("Wounded", 0);
		pSubSection->SetInt("Dead", 0);
		pSubSection->SetInt("ParasitesKilled", 0);
		pSaveKeyValues->AddSubKey(pSubSection);
	}
	// players section is empty at first 

	// Create the save sub-directory
	if (!g_pFullFileSystem->IsDirectory( "save", "MOD" ))
	{
		g_pFullFileSystem->CreateDirHierarchy( "save", "MOD" );
	}

	// save it
	if (pSaveKeyValues->SaveToFile(g_pFullFileSystem, szFullFileName))
	{
		// make sure our save summary list adds this to it, if needed
		Msg("New save created: %s\n", szFullFileName);
		NotifyNewSave(pszNoPathName);
		return true;
	}
	Msg("Save to file failed. Filename=%s\n", szFullFileName);
	return false;
}

// normal alphabetical sorting for map/campaigns
bool CASW_Mission_Chooser_Source_Local::MapNameLess::Less( MapListName const& src1, MapListName const& src2, void *pCtx )
{
	return !!Q_strcmp(src1.szMapName,src2.szMapName);
}

// sort by the datetime string
bool CASW_Mission_Chooser_Source_Local::SavedCampaignLess::Less( ASW_Mission_Chooser_Saved_Campaign const& src1, ASW_Mission_Chooser_Saved_Campaign const& src2, void *pCtx )
{
	int month, day, year, hour, minute;
	int month2, day2, year2, hour2, minute2;

	if ( sscanf( src1.m_szDateTime, "%d/%d/%d %d:%d", &month, &day, &year, &hour, &minute ) != 5 )
		return false;
	if ( sscanf( src2.m_szDateTime, "%d/%d/%d %d:%d", &month2, &day2, &year2, &hour2, &minute2 ) != 5 )
		return false;

	//Msg("src1 month:%d day:%d year:%d hour:%d minute:%d\n", month, day, year, hour, minute);
	//Msg("src2 month:%d day:%d year:%d hour:%d minute:%d\n", month2, day2, year2, hour2, minute2);

	if (year > year2)
	{
		//Msg("src1 is newer because of year\n");
		return true;
	}
	else if (year < year2)
	{
		//Msg("src2 is newer because of year\n");
		return false;
	}

	if (month > month2)
	{
		//Msg("src1 is newer because of month\n");
		return true;
	}
	else if (month < month2)
	{
		//Msg("src2 is newer because of month\n");
		return false;
	}

	if (day > day2)
	{
		//Msg("src1 is newer because of day\n");
		return true;
	}
	else if (day < day2)
	{
		//Msg("src2 is newer because of day\n");
		return false;
	}

	if (hour > hour2)
	{
		//Msg("src1 is newer because of hour\n");
		return true;
	}
	else if (hour < hour2)
	{
		//Msg("src2 is newer because of hour\n");
		return false;
	}

	if (minute > minute2)
	{
		//Msg("src1 is newer because of minute\n");
		return true;
	}
	else if (minute < minute2)
	{
		//Msg("src2 is newer because of minute\n");
		return false;
	}
	
	//if ((year > year2) || (month > month2) || (day > day2) || (hour > hour2) || (minute > minute2))
		//return true;
	//Msg("neither is newer\n");
	
	return false;
}

#define ASW_DEFAULT_INTRO_MAP "intro_jacob"
const char* CASW_Mission_Chooser_Source_Local::GetCampaignSaveIntroMap(const char *szSaveName)
{
	// check the save file exists
	char stripped[MAX_PATH];
	V_StripExtension( szSaveName, stripped, MAX_PATH );
	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "save/%s.campaignsave", stripped );
	if (!g_pFullFileSystem->FileExists(tempfile))
		return ASW_DEFAULT_INTRO_MAP;

	KeyValues *pSaveKeyValues = new KeyValues( szSaveName );
	const char* pszCampaign = NULL;
	if (pSaveKeyValues->LoadFromFile(g_pFullFileSystem, tempfile))
	{		
		pszCampaign = pSaveKeyValues->GetString("CampaignName");
	}
	if (!pszCampaign)
	{
		pSaveKeyValues->deleteThis();
		return ASW_DEFAULT_INTRO_MAP;
	}

	char ctempfile[MAX_PATH];
	Q_snprintf( ctempfile, sizeof( ctempfile ), "resource/campaigns/%s.txt", pszCampaign );
	if (!g_pFullFileSystem->FileExists(ctempfile))	/// check it exists
	{
		pSaveKeyValues->deleteThis();
		return ASW_DEFAULT_INTRO_MAP;
	}

	// now read in the campaign txt and find the intro map name
	KeyValues *pCampaignKeyValues = new KeyValues( pszCampaign );
	if (pCampaignKeyValues->LoadFromFile(g_pFullFileSystem, ctempfile))
	{
		static char s_introname[128];
		Q_strncpy( s_introname, pCampaignKeyValues->GetString("IntroMap"), 128 );
		// check we actually got a valid intro map name string
		if ( Q_strlen(s_introname) > 5 && !Q_strnicmp( s_introname, "intro", 5 ) )
		{
			pSaveKeyValues->deleteThis();
			pCampaignKeyValues->deleteThis();
			return s_introname;
		}
	}

	pSaveKeyValues->deleteThis();
	pCampaignKeyValues->deleteThis();
	return ASW_DEFAULT_INTRO_MAP;
}

KeyValues *CASW_Mission_Chooser_Source_Local::GetMissionDetails( const char *szMissionName )
{
	// see if we have this cached already
	for ( int i = 0; i < m_MissionDetails.Count(); i++ )
	{
		if ( !Q_stricmp( m_MissionDetails[ i ]->szMissionName, szMissionName ) )
		{
			return m_MissionDetails[ i ]->m_pMissionKeys;
		}
	}

	// strip off the extension
	char stripped[MAX_PATH];
	V_StripExtension( szMissionName, stripped, MAX_PATH );

	KeyValues *pMissionKeys = new KeyValues( stripped );

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", stripped );

	bool bNoOverview = false;
	if ( !pMissionKeys->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
	{
		// try to load it directly from the maps folder
		Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.txt", stripped );
		if ( !pMissionKeys->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
		{
			bNoOverview = true;
		}
	}

	MissionDetails_t *pDetails = new MissionDetails_t;
	pDetails->m_pMissionKeys = pMissionKeys;
	Q_snprintf( pDetails->szMissionName, sizeof( pDetails->szMissionName ), "%s", szMissionName );
	m_MissionDetails.AddToTail( pDetails );

	return pMissionKeys;
}

KeyValues *CASW_Mission_Chooser_Source_Local::GetCampaignDetails( const char *szCampaignName )
{
	// see if we have this cached already
	for ( int i = 0; i < m_CampaignDetails.Count(); i++ )
	{
		if ( !Q_stricmp( m_CampaignDetails[ i ]->szCampaignName, szCampaignName ) )
		{
			return m_CampaignDetails[ i ]->m_pCampaignKeys;
		}
	}

	// strip off the extension
	char stripped[MAX_PATH];
	V_StripExtension( szCampaignName, stripped, MAX_PATH );

	KeyValues *pCampaignKeys = new KeyValues( stripped );

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", stripped );

	bool bNoOverview = false;
	if ( !pCampaignKeys->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
	{
		// try to load it directly from the maps folder
		Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.txt", stripped );
		if ( !pCampaignKeys->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
		{
			bNoOverview = true;
		}
	}

	CampaignDetails_t *pDetails = new CampaignDetails_t;
	pDetails->m_pCampaignKeys = pCampaignKeys;
	Q_snprintf( pDetails->szCampaignName, sizeof( pDetails->szCampaignName ), "%s", szCampaignName );
	m_CampaignDetails.AddToTail( pDetails );

	return pCampaignKeys;
}