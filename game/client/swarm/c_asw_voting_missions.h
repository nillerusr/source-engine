#ifndef _INCLUDED_C_ASW_VOTING_MISSIONS_H
#define _INCLUDED_C_ASW_VOTING_MISSIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "missionchooser/iasw_mission_chooser_source.h"

// provides lists of missions, saves and campaigns from a client entity
class C_ASW_Voting_Missions : public C_BaseEntity
{
	DECLARE_CLASS( C_ASW_Voting_Missions, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
	C_ASW_Voting_Missions();
	
	void Update();

	int m_iListType; // 0 = none, 1 = missions, 2 = campaigns, 3 = saved games

	int m_iNumMissions;
	int m_iNumOverviewMissions;
	int m_iNumCampaigns;
	int m_iNumSavedCampaigns;
	int m_nCampaignIndex;

	char	m_iszMissionNames[ASW_SAVES_PER_PAGE][64];
	char	m_iszCampaignNames[ASW_CAMPAIGNS_PER_PAGE][64];
	char	m_iszSaveNames[ASW_SAVES_PER_PAGE][64];
	char	m_iszSaveCampaignNames[ASW_SAVES_PER_PAGE][64];
	char	m_iszSaveDateTimes[ASW_SAVES_PER_PAGE][64];
	char	m_iszSavePlayerNames[ASW_SAVES_PER_PAGE][256];
	int		m_iSaveMissionsComplete[ASW_SAVES_PER_PAGE];

	bool m_bLaunchedChooser;
};

#endif // _INCLUDED_C_ASW_VOTING_MISSIONS_H
