#ifndef _INCLUDED_ASW_MEDALS_H
#define _INCLUDED_ASW_MEDALS_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_medals_shared.h"

class CASW_Marine_Resource;
class CASW_Campaign_Save;
class CASW_Player;

class CASW_Medals
{
public:
	// called during debrief to examine stats collected on the marines and award them medals
	void AwardMedals();
	void AddMedalsToCampaignSave(CASW_Campaign_Save *pSave);

	void OnStartMission();
	// checking if a marine was awarded a particular medal in this mission
	bool HasMedal(int iMedal, CASW_Marine_Resource *pMR, bool bOnlyThisMission=false);

protected:
	void AwardMedalsTo(CASW_Marine_Resource *pMR);
	bool AwardSingleMedalTo(int iMedal, CASW_Marine_Resource *pMR);	

	void AwardPlayerMedalsTo(CASW_Player *pPlayer);
	bool AwardSinglePlayerMedalTo(int iMedal, CASW_Player *pPlayer);
	bool HasPlayerMedal(int iMedal, CASW_Player *pPlayer);

	void DebugMedals(CASW_Marine_Resource *pMR);
	void DebugMedals(CASW_Player *pPlayer);

	int m_iNumEggs;
	bool m_bAwardEggMedal;
	float m_fStartMissionTime;

	int m_bAwardGrubMedal;
	bool m_bAllSurvived;
};

#endif // _INCLUDED_ASW_MEDALS_H