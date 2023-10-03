#ifndef DEFINED_C_ASW_STEAMSTATS_H_
#define DEFINED_C_ASW_STEAMSTATS_H_

#include "steam/steam_api.h"
#include "platform.h"
#include "c_asw_player.h"
#include "utlvector.h"

struct DifficultyStats_t
{
	bool FetchDifficultyStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID, int iDifficulty );
	void PrepStatsForSend( CASW_Player *pPlayer ); 

	int32 m_iGamesTotal;
	int32 m_iGamesSuccess;
	float32 m_fGamesSuccessPercent;
	int32 m_iKillsTotal;
	int32 m_iDamageTotal;
	int32 m_iFFTotal;
	float32 m_fAccuracyAvg;
	int32 m_iShotsFiredTotal;
	int32 m_iShotsHitTotal;
	int32 m_iHealingTotal;
};

struct MissionStats_t
{
	bool FetchMissionStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID );
	void PrepStatsForSend( CASW_Player *pPlayer ); 

	int32 m_iGamesTotal;
	int32 m_iGamesSuccess;
	float32 m_fGamesSuccessPercent;
	int32 m_iKillsTotal;
	int32 m_iDamageTotal;
	int32 m_iFFTotal;
	float32 m_fKillsAvg;
	float32 m_fDamageAvg;
	float32 m_fFFAvg;
	int32 m_iTimeTotal;
	int32 m_iTimeAvg;
	int32 m_iHighestDifficulty;
	int32 m_iBestSpeedrunTimes[5];
};

struct WeaponStats_t
{
	bool FetchWeaponStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID, const char *szClassName );
	void PrepStatsForSend( CASW_Player *pPlayer ); 

	int32 m_iWeaponIndex;
	int32 m_iDamage;
	int32 m_iFFDamage;
	int32 m_iShotsFired;
	int32 m_iShotsHit;
	int32 m_iKills;
	bool  m_bIsExtra;
	char *m_szClassName;
};


class CASW_Steamstats
{
public:
	// Fetch the client's steamstats
	bool FetchStats( CSteamID playerSteamID, CASW_Player *pPlayer );

	// Send the client's stats off to steam
	void PrepStatsForSend( CASW_Player *pPlayer ); 

private:
	int32 m_iTotalKills;
	float32 m_fAccuracy;
	int32 m_iFriendlyFire;
	int32 m_iDamage;
	int32 m_iShotsFired;
	int32 m_iShotsHit;
	int32 m_iAliensBurned;
	int32 m_iHealing;
	int32 m_iFastHacks;
	int32 m_iGamesTotal;
	int32 m_iGamesSuccess;
	float32 m_fGamesSuccessPercent;
	int32	m_iAmmoDeployed;
	int32	m_iSentryGunsDeployed;
	int32	m_iSentryFlamerDeployed;
	int32	m_iSentryFreezeDeployed;
	int32	m_iSentryCannonDeployed;
	int32	m_iMedkitsUsed;
	int32	m_iFlaresUsed;
	int32	m_iAdrenalineUsed;
	int32	m_iTeslaTrapsDeployed;
	int32	m_iFreezeGrenadesThrown;
	int32	m_iElectricArmorUsed;
	int32	m_iHealGunHeals;
	int32	m_iHealBeaconHeals;
	int32	m_iHealGunHeals_Self;
	int32	m_iHealBeaconHeals_Self;
	int32	m_iDamageAmpsUsed;
	int32	m_iHealBeaconsDeployed;
	int32	m_iTotalPlayTime;
	
	typedef CUtlVector<int32> StatList_Int_t;
	typedef CUtlVector<WeaponStats_t> WeaponStatList_t;

	StatList_Int_t m_PrimaryEquipCounts;
	StatList_Int_t m_SecondaryEquipCounts;
	StatList_Int_t m_ExtraEquipCounts;
	StatList_Int_t m_MarineSelectionCounts;
	StatList_Int_t m_DifficultyCounts;

	DifficultyStats_t m_DifficultyStats[5];
	MissionStats_t m_MissionStats;
	WeaponStatList_t m_WeaponStats;
	
	int GetFavoriteEquip( int iSlot );
	int GetFavoriteMarine( void );
	int GetFavoriteMarineClass( void );
	int GetFavoriteDifficulty( void );

	float GetFavoriteEquipPercent( int iSlot );
	float GetFavoriteMarinePercent( void );
	float GetFavoriteMarineClassPercent( void );
	float GetFavoriteDifficultyPercent( void );
};

extern CASW_Steamstats g_ASW_Steamstats;

#endif // #ifndef DEFINED_C_ASW_STEAMSTATS_H_