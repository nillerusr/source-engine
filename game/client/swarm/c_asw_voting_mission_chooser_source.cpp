#include "cbase.h"
#include "c_asw_voting_mission_chooser_source.h"
#include "c_asw_voting_missions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_ASW_Voting_Mission_Chooser_Source::C_ASW_Voting_Mission_Chooser_Source()
{
	m_hVotingMission = NULL;
	m_nMissionOffset = -1;
	m_iNumMissionSlots = -1;
	m_bRequireOverview = false;
	m_nCampaignOffset = -1;
	m_iNumCampaignSlots = -1;
	m_nSaveOffset = -1;
	m_iNumSavedSlots = -1;
	for (int i=0;i<ASW_SAVES_PER_PAGE;i++)
	{
		m_missions[i].m_szMissionName[0] = '\0';		
		m_savedcampaigns[i].m_iMissionsComplete = 0;
		m_savedcampaigns[i].m_szSaveName[0] = '\0';
		m_savedcampaigns[i].m_szCampaignName[0] = '\0';
		m_savedcampaigns[i].m_szDateTime[0] = '\0';
		m_savedcampaigns[i].m_szPlayerNames[0] = '\0';
		m_savedcampaigns[i].m_szPlayerIDs[0] = '\0';
		m_savedcampaigns[i].m_bMultiplayer = true;
	}
	for (int i=0;i<ASW_CAMPAIGNS_PER_PAGE;i++)
	{
		m_campaigns[i].m_szMissionName[0] = '\0';
	}
}

void C_ASW_Voting_Mission_Chooser_Source::SetVotingMission(C_ASW_Voting_Missions* voting)
{
	if (m_hVotingMission.Get() != voting)
	{
		m_hVotingMission = voting;
		// reset which page(s) we were on if we get a new client entity
		ResetCurrentPage();
	}
}

void C_ASW_Voting_Mission_Chooser_Source::ResetCurrentPage()
{
	m_nMissionOffset = -1;
	m_iNumMissionSlots = -1;
	m_bRequireOverview = false;
	m_nCampaignOffset = -1;
	m_iNumCampaignSlots = -1;
	m_nSaveOffset = -1;
	m_iNumSavedSlots = -1;
	m_nCampaignIndex = -1;
}

void C_ASW_Voting_Mission_Chooser_Source::RefreshSavedCampaigns()
{
	// need to tell the server to refresh his list? or don't support this with voting
}

void C_ASW_Voting_Mission_Chooser_Source::FindMissionsInCampaign( int iCampaignIndex, int nMissionOffset, int iNumSlots )
{
	if (nMissionOffset != m_nMissionOffset || iNumSlots != m_iNumMissionSlots || iCampaignIndex != m_nCampaignIndex )
	{
		m_nMissionOffset = nMissionOffset;
		m_iNumMissionSlots = iNumSlots;
		m_bRequireOverview = true;
		m_nCampaignIndex = iCampaignIndex;
		// tell the server our new page/slots/overview settings, so he can update the networked strings appropriately
		char buffer[64];
		Q_snprintf(buffer,sizeof(buffer), "cl_vcampmaplist %d %d", iCampaignIndex, nMissionOffset);
		engine->ClientCmd(buffer);
	}
}

void C_ASW_Voting_Mission_Chooser_Source::FindMissions(int nMissionOffset, int iNumSlots, bool bRequireOverview)
{
	if (nMissionOffset != m_nMissionOffset || iNumSlots != m_iNumMissionSlots || bRequireOverview != m_bRequireOverview || m_nCampaignIndex != -1 )
	{
		m_nMissionOffset = nMissionOffset;
		m_iNumMissionSlots = iNumSlots;
		m_bRequireOverview = bRequireOverview;
		m_nCampaignIndex = -1;
		// tell the server our new page/slots/overview settings, so he can update the networked strings appropriately
		char buffer[64];
		Q_snprintf(buffer,sizeof(buffer), "cl_vmaplist %d", nMissionOffset);
		engine->ClientCmd(buffer);
	}
}

void C_ASW_Voting_Mission_Chooser_Source::FindCampaigns(int nCampaignOffset, int iNumSlots)
{
	if (nCampaignOffset != m_nCampaignOffset || iNumSlots != m_iNumCampaignSlots)
	{
		m_nCampaignOffset = nCampaignOffset;
		m_iNumCampaignSlots = iNumSlots;
		// tell the server our new page/slots settings, so he can update the networked strings appropriately
		char buffer[64];
		Q_snprintf(buffer,sizeof(buffer), "cl_vcamplist %d", nCampaignOffset);
		engine->ClientCmd(buffer);
	}
}

void C_ASW_Voting_Mission_Chooser_Source::FindSavedCampaigns(int nSaveOffset, int iNumSlots, bool bMultiplayer, const char *szFilterID)
{
	// note: can ignore bmultiplayer and filterID, server will assume these parameters when querying its local mission source for us
	if (nSaveOffset != m_nSaveOffset || iNumSlots != m_iNumSavedSlots)
	{
		m_nSaveOffset = nSaveOffset;
		m_iNumSavedSlots = iNumSlots;
		// tell the server our new page/slots settings, so he can update the networked strings appropriately
		char buffer[64];
		Q_snprintf(buffer,sizeof(buffer), "cl_vsaveslist %d", nSaveOffset);
		engine->ClientCmd(buffer);
	}
}

void C_ASW_Voting_Mission_Chooser_Source::Think()
{
	if (!m_hVotingMission.Get())
		return;

	// update our arrays with the ones from the voting mission entity
	for (int i=0;i<ASW_SAVES_PER_PAGE;i++)
	{
		Q_snprintf(m_missions[i].m_szMissionName, sizeof(m_missions[i].m_szMissionName), "%s", m_hVotingMission->m_iszMissionNames[i]);		
		Q_snprintf(m_savedcampaigns[i].m_szSaveName, sizeof(m_savedcampaigns[i].m_szSaveName), "%s", m_hVotingMission->m_iszSaveNames[i]);
		Q_snprintf(m_savedcampaigns[i].m_szCampaignName, sizeof(m_savedcampaigns[i].m_szCampaignName), "%s", m_hVotingMission->m_iszSaveCampaignNames[i]);
		Q_snprintf(m_savedcampaigns[i].m_szDateTime, sizeof(m_savedcampaigns[i].m_szDateTime), "%s", m_hVotingMission->m_iszSaveDateTimes[i]);
		Q_snprintf(m_savedcampaigns[i].m_szPlayerNames, sizeof(m_savedcampaigns[i].m_szPlayerNames), "%s", m_hVotingMission->m_iszSavePlayerNames[i]);
		Q_snprintf(m_savedcampaigns[i].m_szSaveName, sizeof(m_savedcampaigns[i].m_szSaveName), "%s", m_hVotingMission->m_iszSaveNames[i]);
		m_savedcampaigns[i].m_iMissionsComplete = m_hVotingMission->m_iSaveMissionsComplete[i];
	}
	for (int i=0;i<ASW_CAMPAIGNS_PER_PAGE;i++)
	{
		Q_snprintf(m_campaigns[i].m_szMissionName, sizeof(m_campaigns[i].m_szMissionName), "%s", m_hVotingMission->m_iszCampaignNames[i]);
	}
}

int C_ASW_Voting_Mission_Chooser_Source::GetNumMissions(bool bRequireOverview)
{
	if (m_hVotingMission.Get())
	{
		if (bRequireOverview)
			return m_hVotingMission->m_iNumOverviewMissions;

		return m_hVotingMission->m_iNumMissions;
	}
	return 0;
}

int C_ASW_Voting_Mission_Chooser_Source::GetNumMissionsInCampaign( int nCampaignIndex )
{
	Assert( m_nCampaignIndex == nCampaignIndex );
	return GetNumMissions( true );
}

int C_ASW_Voting_Mission_Chooser_Source::GetNumCampaigns()
{
	if (m_hVotingMission.Get())
		return m_hVotingMission->m_iNumCampaigns;
	return 0;
}

int C_ASW_Voting_Mission_Chooser_Source::GetNumSavedCampaigns(bool bMultiplayer, const char *szFilterID)
{
	if (m_hVotingMission.Get())
		return m_hVotingMission->m_iNumSavedCampaigns;
	return 0;
}

ASW_Mission_Chooser_Mission* C_ASW_Voting_Mission_Chooser_Source::GetMission( int nIndex, bool bRequireOverview )
{
	if ( nIndex < m_nMissionOffset || nIndex >= m_nMissionOffset + ASW_MISSIONS_PER_PAGE )
		return NULL;

	return &m_missions[ nIndex - m_nMissionOffset ];
}

ASW_Mission_Chooser_Mission* C_ASW_Voting_Mission_Chooser_Source::GetCampaign( int nIndex )
{
	if ( nIndex < m_nCampaignOffset || nIndex >= m_nCampaignOffset + ASW_CAMPAIGNS_PER_PAGE )
		return NULL;

	return &m_campaigns[ nIndex - m_nCampaignOffset ];
}

ASW_Mission_Chooser_Saved_Campaign* C_ASW_Voting_Mission_Chooser_Source::GetSavedCampaign( int nIndex, bool bMultiplayer, const char *szFilterID )
{
	if ( nIndex < m_nSaveOffset || nIndex >= m_nSaveOffset + ASW_SAVES_PER_PAGE )
		return NULL;

	return &m_savedcampaigns[ nIndex - m_nSaveOffset ];
}

C_ASW_Voting_Mission_Chooser_Source* g_pVotingMissionSource = NULL;

C_ASW_Voting_Mission_Chooser_Source* GetVotingMissionSource()
{
	if (!g_pVotingMissionSource)
	{
		g_pVotingMissionSource = new C_ASW_Voting_Mission_Chooser_Source();
	}
	return g_pVotingMissionSource;
}