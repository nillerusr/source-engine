#include "cbase.h"
#ifdef CLIENT_DLL
#include "c_asw_campaign_save.h"
#define CASW_Campaign_Save C_ASW_Campaign_Save
#else
#include "asw_campaign_save.h"
#endif
#include "asw_gamerules.h"
#include "asw_campaign_info.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CASW_Campaign_Save::IsMissionLinkedToACompleteMission(int i, CASW_Campaign_Info* pCampaignInfo)
{
	if (!pCampaignInfo && !ASWGameRules())
		return false;

	CASW_Campaign_Info::CASW_Campaign_Mission_t *pMission = pCampaignInfo->GetMission(i);
	if (!pMission)
		return false;

	// last mission, don't reveal it unless all other missions are complete
	if (i == pCampaignInfo->GetNumMissions()-1)
	{
		if (ASWGameRules()->CampaignMissionsLeft() > 1)
			return false;
	}

	for (int k=0;k<pMission->m_Links.Count();k++)
	{
		int iLinked = pMission->m_Links[k];
		if (iLinked >= 0 && iLinked < ASW_MAX_MISSIONS_PER_CAMPAIGN && (m_MissionComplete[iLinked] == 1 || iLinked == 0))
			return true;
	}
	return false;
}

bool CASW_Campaign_Save::IsMarineAlive(int iProfileIndex)
{
	if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return false;

	return !m_bMarineDead[iProfileIndex];
}

bool CASW_Campaign_Save::IsMarineWounded(int iProfileIndex)
{
	if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES)
		return false;

	return m_bMarineWounded[iProfileIndex];
}
	