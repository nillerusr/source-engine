#ifndef _INCLUDED_C_ASW_MEDAL_STORE_H
#define _INCLUDED_C_ASW_MEDAL_STORE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "asw_medals_shared.h"

// class responsible for loading/saving the client's medal store

class C_ASW_Marine_Resource;

class C_ASW_Medal_Store
{	
public:
	C_ASW_Medal_Store();

	void LoadMedalStore();
	bool SaveMedalStore();
	void ClearMedalStore();

	bool IsPlayerMedal(int i);
	void DebugInfo();

	bool OnAwardedMedals(const char *pszMedalsAwarded, int iProfileIndex, bool bMultiplayer);
	bool AddMarineMedal(int iProfileIndex, int iMedal, bool bMultiplayer);

	bool OnAwardedPlayerMedals(int iPlayerIndex, const char *pszPlayerMedals, bool bMultiplayer);
	bool AddPlayerMedal(int iMedal, bool bMultiplayer);

	void OnIncreaseCounts(int iAddMission, int iAddCampaign, int iAddKills, bool bOffline);
	void GetCounts(int &iMissions, int &iCampaigns, int &iKills, bool bOffline);

	bool HasMedal(int iMedalIndex, bool bOffline, int iMarine=-1);

	void OnUnlockedEquipment( const char *pszWeaponUnlockClass );
	void OnSelectedEquipment( bool bExtraItem, int nEquipmentListIndex );
	bool IsWeaponNew( bool bExtraItem, int nEquipmentListIndex );
	void ClearNewWeapons();

	void SetExperience( int nXP );
	int GetExperience();

	void SetPromotion( int nPromotion );
	int GetPromotion();

	bool m_bFoundNewClientDat;
	
private:
	typedef CUtlVector<int> MedalList_t;

	int m_iMissionsCompleted;
	int m_iCampaignsCompleted;
	int m_iAliensKilled;

	int m_iOfflineMissionsCompleted;
	int m_iOfflineCampaignsCompleted;
	int m_iOfflineAliensKilled;

	MedalList_t m_MarineMedals[ASW_NUM_MARINE_PROFILES];
	MedalList_t m_PlayerMedals;

	MedalList_t m_OfflineMarineMedals[ASW_NUM_MARINE_PROFILES];
	MedalList_t m_OfflinePlayerMedals;
	bool m_bLoaded;	

	CUtlVector<int> m_NewEquipment;

	int m_iXP;
	int m_iPromotion;
};

C_ASW_Medal_Store* GetMedalStore();

#endif // _INCLUDED_C_ASW_MEDAL_STORE_H