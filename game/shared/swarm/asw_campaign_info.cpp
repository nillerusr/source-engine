#include "cbase.h"
#include "asw_campaign_info.h"
#include <KeyValues.h>
#include <filesystem.h>
#include "gamestringpool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Campaign_Info::CASW_Campaign_Info()
{
	m_iNumMissions = 0;
	m_CampaignKeyValues = NULL;
	m_iGalaxyX = 0;
	m_iGalaxyY = 0;
	for (int i=0;i<4;i++)
	{
		m_iSearchLightX[i] = 0;
		m_iSearchLightY[i] = 0;
		m_iSearchLightAngle[i] = 0;
	}
	for (int i=0;i<ASW_MAX_CAMPAIGN_MISSIONS;i++)
	{
		m_pMission[i] = NULL;
	}
	ClearCampaign();
}

CASW_Campaign_Info::~CASW_Campaign_Info()
{
	ClearCampaign();
}

void CASW_Campaign_Info::ClearCampaign()
{
	// clears all campaign info allocated so far

	//if ( m_CampaignKeyValues )
		//m_CampaignKeyValues->deleteThis();

	m_CampaignKeyValues = NULL;

	for (int i=0;i<ASW_MAX_CAMPAIGN_MISSIONS;i++)
	{
		if (m_pMission[i])
		{
			m_pMission[i]->m_Links.Purge();
			delete m_pMission[i];			
		}
		m_pMission[i] = NULL;
	}
	m_iNumMissions = 0;
	m_CampaignName = NULL_STRING;
	m_szCampaignFilename[0] = 0;
	m_CampaignTextureName = NULL_STRING;
	m_IntroMap = NULL_STRING;
	m_OutroMap = NULL_STRING;
}

bool CASW_Campaign_Info::LoadCampaign(const char *szCampaignName)
{
	ClearCampaign();

	m_CampaignKeyValues = new KeyValues( szCampaignName );

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", szCampaignName );
	
	if ( !m_CampaignKeyValues->LoadFromFile( filesystem, tempfile, "GAME" ) )
	{
		DevMsg( 1, "CASW_Campaign_Info::LoadCampaign: couldn't load file %s.\n", tempfile );
		return false;
	}

	Q_snprintf( m_szCampaignFilename, sizeof( m_szCampaignFilename ), "%s", szCampaignName );
	m_CampaignName = MAKE_STRING(m_CampaignKeyValues->GetString("CampaignName"));
	m_IntroMap = MAKE_STRING(m_CampaignKeyValues->GetString("IntroMap"));
	m_OutroMap = MAKE_STRING(m_CampaignKeyValues->GetString("OutroMap"));
	m_CampaignTextureName = MAKE_STRING(m_CampaignKeyValues->GetString("CampaignTextureName"));
	m_CampaignTextureLayer1 = MAKE_STRING(m_CampaignKeyValues->GetString("CampaignTextureLayer1"));
	m_CampaignTextureLayer2 = MAKE_STRING(m_CampaignKeyValues->GetString("CampaignTextureLayer2"));
	m_CampaignTextureLayer3 = MAKE_STRING(m_CampaignKeyValues->GetString("CampaignTextureLayer3"));
	m_CustomCreditsFile = MAKE_STRING(m_CampaignKeyValues->GetString("CustomCreditsFile", "scripts/asw_credits"));
	m_iGalaxyX = m_CampaignKeyValues->GetInt("GalaxyX");
	m_iGalaxyY = m_CampaignKeyValues->GetInt("GalaxyY");
	m_iSearchLightX[0] = m_CampaignKeyValues->GetInt("Searchlight1X");
	m_iSearchLightY[0] = m_CampaignKeyValues->GetInt("Searchlight1Y");
	m_iSearchLightAngle[0] = m_CampaignKeyValues->GetInt("Searchlight1Angle");
	m_iSearchLightX[1] = m_CampaignKeyValues->GetInt("Searchlight2X");
	m_iSearchLightY[1] = m_CampaignKeyValues->GetInt("Searchlight2Y");
	m_iSearchLightAngle[1] = m_CampaignKeyValues->GetInt("Searchlight2Angle");
	m_iSearchLightX[2] = m_CampaignKeyValues->GetInt("Searchlight3X");
	m_iSearchLightY[2] = m_CampaignKeyValues->GetInt("Searchlight3Y");
	m_iSearchLightAngle[2] = m_CampaignKeyValues->GetInt("Searchlight3Angle");
	m_iSearchLightX[3] = m_CampaignKeyValues->GetInt("Searchlight4X");
	m_iSearchLightY[3] = m_CampaignKeyValues->GetInt("Searchlight4Y");
	m_iSearchLightAngle[3] = m_CampaignKeyValues->GetInt("Searchlight4Angle");

	// now go through each mission section, adding it
	KeyValues *pkvMission = m_CampaignKeyValues->GetFirstSubKey();
	while ( pkvMission )
	{		
		if (Q_stricmp(pkvMission->GetName(), "MISSION")==0)
		{
			//Msg("adding mission with subkey %s (name is %s)\n", pkvMission->GetName(), pkvMission->GetString("MapName"));
			m_pMission[m_iNumMissions] = new CASW_Campaign_Mission_t;
			
			m_pMission[m_iNumMissions]->m_iMissionIndex = m_iNumMissions;
			m_pMission[m_iNumMissions]->m_MissionName = MAKE_STRING(pkvMission->GetString("MissionName", "Unknown Mission"));
			m_pMission[m_iNumMissions]->m_MapName = MAKE_STRING(pkvMission->GetString("MapName"));
			m_pMission[m_iNumMissions]->m_iLocationX = pkvMission->GetInt("LocationX");
			m_pMission[m_iNumMissions]->m_iLocationY = pkvMission->GetInt("LocationY");
			m_pMission[m_iNumMissions]->m_iDifficultyMod = pkvMission->GetInt("DifficultyModifier");
			m_pMission[m_iNumMissions]->m_LinksString = MAKE_STRING(pkvMission->GetString("Links"));
			m_pMission[m_iNumMissions]->m_LocationDescription = MAKE_STRING(pkvMission->GetString("LocationDescription"));
			m_pMission[m_iNumMissions]->m_ThreatString = MAKE_STRING(pkvMission->GetString("ThreatString"));
			m_pMission[m_iNumMissions]->m_Briefing = MAKE_STRING(pkvMission->GetString("ShortBriefing"));
			m_pMission[m_iNumMissions]->m_bAlwaysVisible = pkvMission->GetBool( "AlwaysVisible" );
			m_pMission[m_iNumMissions]->m_bNeedsMoreThanOneMarine = pkvMission->GetBool( "NeedsMoreThanOneMarine", false );

			m_iNumMissions++;
		}

		pkvMission = pkvMission->GetNextKey();
	}

	// go through all missions and convert their links string into mission ids
	for (int i=0;i<m_iNumMissions;i++)
	{
		if (!m_pMission[i] || !m_pMission[i]->m_LinksString)
			continue;

		CUtlVector<char*, CUtlMemory<char*> > missions;
		Q_SplitString(STRING(m_pMission[i]->m_LinksString), " ", missions);
		int missions_count = missions.Count();
		for (int k=0; k<missions_count; k++)
		{
			CASW_Campaign_Mission_t *pMission = GetMissionByMapName(missions[k]);
			if (pMission)
			{				
				AddMissionLink(i, pMission->m_iMissionIndex);
			}
			else
			{
				Msg("Error linking campaign, couldn't find mission (from mission %d): %s\n", i, missions[k]);
				return false;
			}
		}
		missions.PurgeAndDeleteElements();
	}
	return true;
}

CASW_Campaign_Info::CASW_Campaign_Mission_t* CASW_Campaign_Info::GetMissionByMissionName(const char *szMissionName)
{
	for (int i=0;i<m_iNumMissions;i++)
	{
		if (!m_pMission[i])
			continue;

		if (Q_stricmp(szMissionName, STRING(m_pMission[i]->m_MissionName))==0)
		{
			return m_pMission[i];
		}
	}
	return NULL;
}

CASW_Campaign_Info::CASW_Campaign_Mission_t* CASW_Campaign_Info::GetMissionByMapName(const char *szMissionName)
{
	for (int i=0;i<m_iNumMissions;i++)
	{
		if (!m_pMission[i])
		{
			continue;
		}
		if (Q_stricmp(szMissionName, STRING(m_pMission[i]->m_MapName))==0)
		{
			return m_pMission[i];
		}
	}
	return NULL;
}

// adds a one way link from start mission to end mission
void CASW_Campaign_Info::AddMissionLink(int iStartMission, int iEndMission)
{
	if (!m_pMission[iStartMission])
		return;

	if (!m_pMission[iEndMission])
		return;

	CASW_Campaign_Mission_t *pMission = m_pMission[iStartMission];
	//Msg("Creating a link between mission %d and %d\n", iStartMission, iEndMission);
	pMission->m_Links.AddToTail(iEndMission);
}

void CASW_Campaign_Info::DebugInfo()
{
	Msg("CampaignName: %s\nTextureName: %s\n", STRING(m_CampaignName), STRING(m_CampaignTextureName));
	Msg("IntroMap: %s\nOutroMap: %s\n", STRING(m_IntroMap), STRING(m_OutroMap));		
	Msg("Missions (%d):\n", m_iNumMissions);
	for (int i=0;i<m_iNumMissions;i++)
	{
		Msg("Mission %d/%d is %s\n", i, m_pMission[i]->m_iMissionIndex, STRING(m_pMission[i]->m_MissionName));
		//Msg("  Map:%s  LX:%d LY:%d LINKS:%s", i, STRING(m_pMission[i]->m_MapName), m_pMission[i]->m_iLocationX,
			//m_pMission[i]->m_iLocationY, STRING(m_pMission[i]->m_LinksString));
	}	
}

bool CASW_Campaign_Info::AreMissionsLinked(int i, int j)
{
	CASW_Campaign_Mission_t *pFirst = GetMission(i);
	CASW_Campaign_Mission_t *pSecond = GetMission(j);

	if (!pFirst || !pSecond)
		return false;

	for (int k=0;k<pFirst->m_Links.Count();k++)
	{
		if (pFirst->m_Links[k] == j)
			return true;
	}
	return false;
}

CASW_Campaign_Info::CASW_Campaign_Mission_t* CASW_Campaign_Info::GetMission(int iMissionIndex)
{
	if (iMissionIndex < 0 || iMissionIndex >=ASW_MAX_CAMPAIGN_MISSIONS)
	{
		return NULL;
	}
	return m_pMission[iMissionIndex];
}

void CASW_Campaign_Info::GetGalaxyPos(int &x, int &y)
{
	x = m_iGalaxyX;
	y = m_iGalaxyY;
}

bool CASW_Campaign_Info::IsJacobCampaign()
{
	return !Q_stricmp( m_szCampaignFilename, "jacob" );
}