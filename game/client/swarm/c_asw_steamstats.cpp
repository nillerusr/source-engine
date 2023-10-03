#include "cbase.h"
#include "c_asw_steamstats.h"
#include "c_asw_debrief_stats.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "fmtstr.h"
#include "asw_equipment_list.h"
#include "string_t.h"
#include "asw_util_shared.h"
#include "asw_marine_profile.h"
#include <vgui/ILocalize.h>
#include "asw_shareddefs.h"
#include "c_asw_marine_resource.h"
#include "c_asw_campaign_save.h"

CASW_Steamstats g_ASW_Steamstats;

namespace 
{
#define FETCH_STEAM_STATS( apiname, membername ) \
	{ const char * szApiName = apiname; \
	if( !steamapicontext->SteamUserStats()->GetUserStat( playerSteamID, szApiName, &membername ) ) \
	{ \
		bOK = false; \
		Msg( "STEAMSTATS: Failed to retrieve stat %s.\n", szApiName ); \
} }

#define SEND_STEAM_STATS( apiname, membername ) \
	{ const char * szApiName = apiname; \
	steamapicontext->SteamUserStats()->SetStat( szApiName, membername ); }

	// Define some strings used by the stats API
	const char* szGamesTotal = ".games.total";
	const char* szGamesSuccess = ".games.success";
	const char* szGamesSuccessPercent = ".games.success.percent";
	const char* szKillsTotal = ".kills.total";
	const char* szDamageTotal = ".damage.total";
	const char* szFFTotal = ".ff.total";
	const char* szAccuracy = ".accuracy.avg";
	const char* szShotsTotal = ".shotsfired.total";
	const char* szShotsHit = ".shotshit.total";
	const char* szHealingTotal = ".healing.total";
	const char* szTimeTotal = ".time.total";
	const char* szKillsAvg = ".kills.avg";
	const char* szDamageAvg = ".damage.avg";
	const char* szFFAvg = ".ff.avg";
	const char* szTimeAvg = ".time.avg";
	const char* szBestDifficulty = ".difficulty.best";
	const char* szBestTime = ".time.best";
	const char* szBestSpeedrunDifficulty = ".time.best.difficulty";

	// difficulty names used when fetching steam stats
	const char* g_szDifficulties[] =
	{
		"Easy",
		"Normal",
		"Hard",
		"Insane",
		"imba"
	};

	const char *g_OfficialMaps[] =
	{
		"asi-jac1-landingbay_01",
		"asi-jac1-landingbay_02",
		"asi-jac2-deima",
		"asi-jac3-rydberg",
		"asi-jac4-residential",
		"asi-jac6-sewerjunction",
		"asi-jac7-timorstation"
	};
}

bool IsOfficialCampaign()
{
	if( !ASWGameRules()->IsCampaignGame() )
		return false;

	CASW_Campaign_Save *pCampaign = ASWGameRules()->GetCampaignSave();

	const char *szMapName = engine->GetLevelNameShort();
	const char *szCampaignName = pCampaign->GetCampaignName();
	if( FStrEq( szCampaignName, "jacob" ) )
	{
		for( int i=0; i < ARRAYSIZE( g_OfficialMaps ); ++i )
		{
			if( FStrEq( szMapName, g_OfficialMaps[i] ) )
				return true;
		}
	}

	return false;
}

bool IsDamagingWeapon( const char* szWeaponName, bool bIsExtraEquip )
{
	if( bIsExtraEquip )
	{
		// Check for the few damaging weapons
		if( !Q_strcmp( szWeaponName, "asw_weapon_laser_mines" ) || 
			!Q_strcmp( szWeaponName, "asw_weapon_hornet_barrage" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_mines" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_grenades" ) || 
			!Q_strcmp( szWeaponName, "asw_weapon_smart_bomb" ) )
			return true;
		else
			return false;
	}
	else
	{
		// Check for the few non-damaging weapons
		if( !Q_strcmp( szWeaponName, "asw_weapon_heal_grenade" ) || 
			!Q_strcmp( szWeaponName, "asw_weapon_ammo_satchel" ) || 
			!Q_strcmp( szWeaponName, "asw_weapon_heal_gun" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_fire_extinguisher" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_t75" ) )
			return false;
		else
			return true;
	}
}

Class_T GetDamagingWeaponClassFromName( const char *szClassName )
{
	if( FStrEq( szClassName, "asw_weapon_rifle") )
		return (Class_T)CLASS_ASW_RIFLE;
	else if( FStrEq( szClassName, "asw_weapon_prifle") )
		return (Class_T)CLASS_ASW_PRIFLE;
	else if( FStrEq( szClassName, "asw_weapon_autogun") )
		return (Class_T)CLASS_ASW_AUTOGUN;
	else if( FStrEq( szClassName, "asw_weapon_vindicator") )
		return (Class_T)CLASS_ASW_ASSAULT_SHOTGUN;
	else if( FStrEq( szClassName, "asw_weapon_pistol") )
		return (Class_T)CLASS_ASW_PISTOL;
	else if( FStrEq( szClassName, "asw_weapon_sentry") )
		return (Class_T)CLASS_ASW_SENTRY_GUN;
	else if( FStrEq( szClassName, "asw_weapon_shotgun") )
		return (Class_T)CLASS_ASW_SHOTGUN;
	else if( FStrEq( szClassName, "asw_weapon_tesla_gun") )
		return (Class_T)CLASS_ASW_TESLA_GUN;
	else if( FStrEq( szClassName, "asw_weapon_railgun") )
		return (Class_T)CLASS_ASW_RAILGUN;
	else if( FStrEq( szClassName, "asw_weapon_pdw") )
		return (Class_T)CLASS_ASW_PDW;
	else if( FStrEq( szClassName, "asw_weapon_flamer") )
		return (Class_T)CLASS_ASW_FLAMER;
	else if( FStrEq( szClassName, "asw_weapon_sentry_freeze") )
		return (Class_T)CLASS_ASW_SENTRY_FREEZE;
	else if( FStrEq( szClassName, "asw_weapon_minigun") )
		return (Class_T)CLASS_ASW_MINIGUN;
	else if( FStrEq( szClassName, "asw_weapon_sniper_rifle") )
		return (Class_T)CLASS_ASW_SNIPER_RIFLE;
	else if( FStrEq( szClassName, "asw_weapon_sentry_flamer") )
		return (Class_T)CLASS_ASW_SENTRY_FLAMER;
	else if( FStrEq( szClassName, "asw_weapon_chainsaw") )
		return (Class_T)CLASS_ASW_CHAINSAW;
	else if( FStrEq( szClassName, "asw_weapon_sentry_cannon") )
		return (Class_T)CLASS_ASW_SENTRY_CANNON;
	else if( FStrEq( szClassName, "asw_weapon_grenade_launcher") )
		return (Class_T)CLASS_ASW_GRENADE_LAUNCHER;
	else if( FStrEq( szClassName, "asw_weapon_mining_laser") )
		return (Class_T)CLASS_ASW_MINING_LASER;

	else if( FStrEq( szClassName, "asw_weapon_laser_mines") )
		return (Class_T)CLASS_ASW_LASER_MINES;
	else if( FStrEq( szClassName, "asw_weapon_hornet_barrage") )
		return (Class_T)CLASS_ASW_HORNET_BARRAGE;
	else if( FStrEq( szClassName, "asw_weapon_mines") )
		return (Class_T)CLASS_ASW_MINES;
	else if( FStrEq( szClassName, "asw_weapon_grenades") )
		return (Class_T)CLASS_ASW_GRENADES;
	else if( FStrEq( szClassName, "asw_weapon_smart_bomb") )
		return (Class_T)CLASS_ASW_SMART_BOMB;

	else if( FStrEq( szClassName, "asw_rifle_grenade") )
		return (Class_T)( 255 - CLASS_ASW_RIFLE );
	else if( FStrEq( szClassName, "asw_vindicator_grenade") )
		return (Class_T)( 255 - CLASS_ASW_ASSAULT_SHOTGUN );

	else
		return (Class_T)CLASS_ASW_UNKNOWN;
}
bool CASW_Steamstats::FetchStats( CSteamID playerSteamID, CASW_Player *pPlayer )
{
	bool bOK = true;

	m_PrimaryEquipCounts.Purge();
	m_SecondaryEquipCounts.Purge();
	m_ExtraEquipCounts.Purge();
	m_MarineSelectionCounts.Purge();
	m_DifficultyCounts.Purge();
	m_WeaponStats.Purge();

	// Returns true so we don't re-fetch stats
	if( !IsOfficialCampaign() )
		return true;

	// Fetch the player's overall stats
	FETCH_STEAM_STATS( "iTotalKills", m_iTotalKills );
	FETCH_STEAM_STATS( "fAccuracy", m_fAccuracy );
	FETCH_STEAM_STATS( "iFriendlyFire", m_iFriendlyFire );
	FETCH_STEAM_STATS( "iDamage", m_iDamage );
	FETCH_STEAM_STATS( "iShotsFired", m_iShotsFired );
	FETCH_STEAM_STATS( "iShotsHit", m_iShotsHit );
	FETCH_STEAM_STATS( "iAliensBurned", m_iAliensBurned );
	FETCH_STEAM_STATS( "iHealing", m_iHealing );
	FETCH_STEAM_STATS( "iFastHacks", m_iFastHacks );
	FETCH_STEAM_STATS( "iGamesTotal", m_iGamesTotal );
	FETCH_STEAM_STATS( "iGamesSuccess", m_iGamesSuccess );
	FETCH_STEAM_STATS( "fGamesSuccessPercent", m_fGamesSuccessPercent );
	FETCH_STEAM_STATS( "ammo_deployed", m_iAmmoDeployed );
	FETCH_STEAM_STATS( "sentryguns_deployed", m_iSentryGunsDeployed );
	FETCH_STEAM_STATS( "sentry_flamers_deployed", m_iSentryFlamerDeployed );
	FETCH_STEAM_STATS( "sentry_freeze_deployed", m_iSentryFreezeDeployed );
	FETCH_STEAM_STATS( "sentry_cannon_deployed", m_iSentryCannonDeployed );
	FETCH_STEAM_STATS( "medkits_used", m_iMedkitsUsed );
	FETCH_STEAM_STATS( "flares_used", m_iFlaresUsed );
	FETCH_STEAM_STATS( "adrenaline_used", m_iAdrenalineUsed );
	FETCH_STEAM_STATS( "tesla_traps_deployed", m_iTeslaTrapsDeployed );
	FETCH_STEAM_STATS( "freeze_grenades_thrown", m_iFreezeGrenadesThrown );
	FETCH_STEAM_STATS( "electric_armor_used", m_iElectricArmorUsed );
	FETCH_STEAM_STATS( "healgun_heals", m_iHealGunHeals );
	FETCH_STEAM_STATS( "healbeacon_heals", m_iHealBeaconHeals );
	FETCH_STEAM_STATS( "healgun_heals_self", m_iHealGunHeals_Self );
	FETCH_STEAM_STATS( "healbeacon_heals_self", m_iHealBeaconHeals_Self );
	FETCH_STEAM_STATS( "damage_amps_used", m_iDamageAmpsUsed );
	FETCH_STEAM_STATS( "healbeacons_deployed", m_iHealBeaconsDeployed );
	FETCH_STEAM_STATS( "playtime.total", m_iTotalPlayTime );

	// Fetch starting equip information
	int i=0;
	while( ASWEquipmentList()->GetRegular( i ) )
	{
		// Get weapon information
		if( IsDamagingWeapon( ASWEquipmentList()->GetRegular( i )->m_EquipClass, false ) )
		{
			int weaponIndex = m_WeaponStats.AddToTail();
			const char *szClassname = ASWEquipmentList()->GetRegular( i )->m_EquipClass;
			m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
			m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
			m_WeaponStats[weaponIndex].m_bIsExtra = false;
			m_WeaponStats[weaponIndex].m_szClassName = const_cast<char*>( szClassname );
		}
		
		// For primary equips
		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "equips.%s.primary", ASWEquipmentList()->GetRegular( i )->m_EquipClass ), iTempCount );
		m_PrimaryEquipCounts.AddToTail( iTempCount );

		// For secondary equips
		iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "equips.%s.secondary", ASWEquipmentList()->GetRegular( i++ )->m_EquipClass ), iTempCount );
		m_SecondaryEquipCounts.AddToTail( iTempCount );

	}

	i=0;
	while( ASWEquipmentList()->GetExtra( i ) )
	{
		// Get weapon information
		if( IsDamagingWeapon( ASWEquipmentList()->GetExtra( i )->m_EquipClass, true ) )
		{
			int weaponIndex = m_WeaponStats.AddToTail();
			const char *szClassname = ASWEquipmentList()->GetExtra( i )->m_EquipClass;
			m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
			m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
			m_WeaponStats[weaponIndex].m_bIsExtra = true;
			m_WeaponStats[weaponIndex].m_szClassName = const_cast<char*>( szClassname );
		}

		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "equips.%s.total", ASWEquipmentList()->GetExtra( i++ )->m_EquipClass ), iTempCount );
		m_ExtraEquipCounts.AddToTail( iTempCount );
	}

	// Get weapon stats for rifle grenade and vindicator grenade
	int weaponIndex = m_WeaponStats.AddToTail();
	char *szClassname = "asw_rifle_grenade";
	m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
	m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
	m_WeaponStats[weaponIndex].m_bIsExtra = false;
	m_WeaponStats[weaponIndex].m_szClassName = szClassname;

	weaponIndex = m_WeaponStats.AddToTail();
	szClassname = "asw_vindicator_grenade";
	m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
	m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
	m_WeaponStats[weaponIndex].m_bIsExtra = false;
	m_WeaponStats[weaponIndex].m_szClassName = szClassname;


	// Fetch marine counts
	i=0;
	while( MarineProfileList()->GetProfile( i ) )
	{
		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "marines.%i.total", i++ ), iTempCount );
		m_MarineSelectionCounts.AddToTail( iTempCount );
	}

	// Get difficulty counts
	for( int i=0; i < 5; ++i )
	{
		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "%s.games.total", g_szDifficulties[ i ] ), iTempCount );
		m_DifficultyCounts.AddToTail( iTempCount );

		// Fetch all stats for that difficulty
		bOK &= m_DifficultyStats[i].FetchDifficultyStats( steamapicontext, playerSteamID, i + 1 );
	}

	// Get stats for this mission/difficulty/marine
	bOK &= m_MissionStats.FetchMissionStats( steamapicontext, playerSteamID );
	
	return bOK;
}

void CASW_Steamstats::PrepStatsForSend( CASW_Player *pPlayer )
{
	if( !steamapicontext )
		return;

	// Update stats from the briefing screen
	if( !GetDebriefStats() 
		|| !ASWGameResource() 
		|| !IsOfficialCampaign() 
#ifndef DEBUG 
		|| ASWGameRules()->m_bCheated 
#endif
		)
		return;

	if( m_MarineSelectionCounts.Count() == 0 || 
		m_DifficultyCounts.Count() == 0 || 
		m_PrimaryEquipCounts.Count() == 0 || 
		m_SecondaryEquipCounts.Count() == 0 || 
		m_ExtraEquipCounts.Count() == 0 )
		return;

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
		return;
	
	int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
	if ( iMarineIndex == -1 )
		return;

	int iMarineProfileIndex = pMR->m_MarineProfileIndex;

	m_iTotalKills += GetDebriefStats()->GetKills( iMarineIndex );
	m_iFriendlyFire += GetDebriefStats()->GetFriendlyFire( iMarineIndex );
	m_iDamage += GetDebriefStats()->GetDamageTaken( iMarineIndex );
	m_iShotsFired += GetDebriefStats()->GetShotsFired( iMarineIndex );
	m_iShotsHit += GetDebriefStats()->GetShotsHit( iMarineIndex );
	m_fAccuracy = ( m_iShotsFired > 0 ) ? ( m_iShotsHit / (float)m_iShotsFired * 100.0f ) : 0;
	m_iAliensBurned += GetDebriefStats()->GetAliensBurned( iMarineIndex );
	m_iHealing += GetDebriefStats()->GetHealthHealed( iMarineIndex );
	m_iFastHacks += GetDebriefStats()->GetFastHacks( iMarineIndex );
	m_iGamesTotal++;
	if( m_DifficultyCounts.IsValidIndex( ASWGameRules()->GetSkillLevel() - 1 ) )
		m_DifficultyCounts[ ASWGameRules()->GetSkillLevel() - 1 ] += 1;
	m_iGamesSuccess += ASWGameRules()->GetMissionSuccess() ? 1 : 0;
	m_fGamesSuccessPercent = m_iGamesSuccess / m_iGamesTotal * 100.0f;
	if( m_MarineSelectionCounts.IsValidIndex( iMarineProfileIndex ) )
		m_MarineSelectionCounts[iMarineProfileIndex] += 1;
	m_iAmmoDeployed += GetDebriefStats()->GetAmmoDeployed( iMarineIndex );
	m_iSentryGunsDeployed += GetDebriefStats()->GetSentrygunsDeployed( iMarineIndex );
	m_iSentryFlamerDeployed += GetDebriefStats()->GetSentryFlamersDeployed( iMarineIndex );
	m_iSentryFreezeDeployed += GetDebriefStats()->GetSentryFreezeDeployed( iMarineIndex );
	m_iSentryCannonDeployed += GetDebriefStats()->GetSentryCannonDeployed( iMarineIndex );
	m_iMedkitsUsed += GetDebriefStats()->GetMedkitsUsed( iMarineIndex );
	m_iFlaresUsed += GetDebriefStats()->GetFlaresUsed( iMarineIndex );
	m_iAdrenalineUsed += GetDebriefStats()->GetAdrenalineUsed( iMarineIndex );
	m_iTeslaTrapsDeployed += GetDebriefStats()->GetTeslaTrapsDeployed( iMarineIndex );
	m_iFreezeGrenadesThrown += GetDebriefStats()->GetFreezeGrenadesThrown( iMarineIndex );
	m_iElectricArmorUsed += GetDebriefStats()->GetElectricArmorUsed( iMarineIndex );
	m_iHealGunHeals += GetDebriefStats()->GetHealgunHeals( iMarineIndex );
	m_iHealBeaconHeals += GetDebriefStats()->GetHealbeaconHeals( iMarineIndex );
	m_iHealGunHeals_Self += GetDebriefStats()->GetHealgunHeals_Self( iMarineIndex );
	m_iHealBeaconHeals_Self += GetDebriefStats()->GetHealbeaconHeals_self( iMarineIndex );
	m_iDamageAmpsUsed += GetDebriefStats()->GetDamageAmpsUsed( iMarineIndex );
	m_iHealBeaconsDeployed += GetDebriefStats()->GetHealbeaconsDeployed( iMarineIndex );
	m_iTotalPlayTime += (int)GetDebriefStats()->m_fTimeTaken;

	// Get starting equips
	int iPrimaryIndex = GetDebriefStats()->GetStartingPrimaryEquip( iMarineIndex );
	int iSecondaryIndex = GetDebriefStats()->GetStartingSecondaryEquip( iMarineIndex );
	int iExtraIndex = GetDebriefStats()->GetStartingExtraEquip( iMarineIndex );
	CASW_EquipItem *pPrimary = ASWEquipmentList()->GetRegular( iPrimaryIndex );
	CASW_EquipItem *pSecondary = ASWEquipmentList()->GetRegular( iSecondaryIndex );
	CASW_EquipItem *pExtra = ASWEquipmentList()->GetExtra( iExtraIndex );

	Assert( pPrimary && pSecondary && pExtra );

	m_PrimaryEquipCounts[iPrimaryIndex]++;
	m_SecondaryEquipCounts[iSecondaryIndex]++;
	m_ExtraEquipCounts[iExtraIndex]++;

	m_DifficultyStats[ASWGameRules()->GetSkillLevel() - 1].PrepStatsForSend( pPlayer );
	m_MissionStats.PrepStatsForSend( pPlayer );

	// Send player's overall stats
	SEND_STEAM_STATS( "iTotalKills", m_iTotalKills );
	SEND_STEAM_STATS( "fAccuracy", m_fAccuracy );
	SEND_STEAM_STATS( "iFriendlyFire", m_iFriendlyFire );
	SEND_STEAM_STATS( "iDamage", m_iDamage );
	SEND_STEAM_STATS( "iShotsFired", m_iShotsFired );
	SEND_STEAM_STATS( "iShotsHit", m_iShotsHit );
	SEND_STEAM_STATS( "iAliensBurned", m_iAliensBurned );
	SEND_STEAM_STATS( "iHealing", m_iHealing );
	SEND_STEAM_STATS( "iFastHacks", m_iFastHacks );
	SEND_STEAM_STATS( "iGamesTotal", m_iGamesTotal );
	SEND_STEAM_STATS( "iGamesSuccess", m_iGamesSuccess );
	SEND_STEAM_STATS( "fGamesSuccessPercent", m_fGamesSuccessPercent );

	SEND_STEAM_STATS( "ammo_deployed", m_iAmmoDeployed );
	SEND_STEAM_STATS( "sentryguns_deployed", m_iSentryGunsDeployed );
	SEND_STEAM_STATS( "sentry_flamers_deployed", m_iSentryFlamerDeployed );
	SEND_STEAM_STATS( "sentry_freeze_deployed", m_iSentryFreezeDeployed );
	SEND_STEAM_STATS( "sentry_cannon_deployed", m_iSentryCannonDeployed );
	SEND_STEAM_STATS( "medkits_used", m_iMedkitsUsed );
	SEND_STEAM_STATS( "flares_used", m_iFlaresUsed );
	SEND_STEAM_STATS( "adrenaline_used", m_iAdrenalineUsed );
	SEND_STEAM_STATS( "tesla_traps_deployed", m_iTeslaTrapsDeployed );
	SEND_STEAM_STATS( "freeze_grenades_thrown", m_iFreezeGrenadesThrown );
	SEND_STEAM_STATS( "electric_armor_used", m_iElectricArmorUsed );
	SEND_STEAM_STATS( "healgun_heals", m_iHealGunHeals );
	SEND_STEAM_STATS( "healbeacon_heals", m_iHealBeaconHeals );
	SEND_STEAM_STATS( "healgun_heals_self", m_iHealGunHeals_Self );
	SEND_STEAM_STATS( "healbeacon_heals_self", m_iHealBeaconHeals_Self );
	SEND_STEAM_STATS( "damage_amps_used", m_iDamageAmpsUsed );
	SEND_STEAM_STATS( "healbeacons_deployed", m_iHealBeaconsDeployed );
	SEND_STEAM_STATS( "playtime.total", m_iTotalPlayTime );

	SEND_STEAM_STATS( CFmtStr( "equips.%s.primary", pPrimary->m_EquipClass), m_PrimaryEquipCounts[iPrimaryIndex] );
	SEND_STEAM_STATS( CFmtStr( "equips.%s.secondary", pSecondary->m_EquipClass), m_SecondaryEquipCounts[iSecondaryIndex] );
	SEND_STEAM_STATS( CFmtStr( "equips.%s.total", pExtra->m_EquipClass), m_ExtraEquipCounts[iExtraIndex] );
	SEND_STEAM_STATS( CFmtStr( "marines.%i.total", iMarineProfileIndex ), m_MarineSelectionCounts[iMarineProfileIndex] );
	int iLevel = pPlayer->GetLevel();
	SEND_STEAM_STATS( "level", iLevel );
	int iPromotion = pPlayer->GetPromotion();
	float flXPRequired = ( iLevel == NELEMS( g_iLevelExperience ) ) ? 0 : g_iLevelExperience[ iLevel ];
	flXPRequired *= g_flPromotionXPScale[ iPromotion ];
	SEND_STEAM_STATS( "level.xprequired", (int) flXPRequired );

	// Send favorite equip info
	SEND_STEAM_STATS( "equips.primary.fav", GetFavoriteEquip(0) );
	SEND_STEAM_STATS( "equips.secondary.fav", GetFavoriteEquip(1) );
	SEND_STEAM_STATS( "equips.extra.fav", GetFavoriteEquip(2) );
	SEND_STEAM_STATS( "equips.primary.fav.pct", GetFavoriteEquipPercent(0) );
	SEND_STEAM_STATS( "equips.secondary.fav.pct", GetFavoriteEquipPercent(1) );
	SEND_STEAM_STATS( "equips.extra.fav.pct", GetFavoriteEquipPercent(2) );

	// Send favorite marine info
	SEND_STEAM_STATS( "marines.fav", GetFavoriteMarine() );
	SEND_STEAM_STATS( "marines.class.fav", GetFavoriteMarineClass() );
	SEND_STEAM_STATS( "marines.fav.pct", GetFavoriteMarinePercent() );
	SEND_STEAM_STATS( "marines.class.fav.pct", GetFavoriteMarineClassPercent() );

	// Send favorite difficulty info
	SEND_STEAM_STATS( "difficulty.fav", GetFavoriteDifficulty() );
	SEND_STEAM_STATS( "difficulty.fav.pct", GetFavoriteDifficultyPercent() );

	// Send weapon stats
	for( int i=0; i<m_WeaponStats.Count(); ++i )
	{
		m_WeaponStats[i].PrepStatsForSend( pPlayer );
	}

}

int CASW_Steamstats::GetFavoriteEquip( int iSlot )
{
	Assert( iSlot < ASW_MAX_EQUIP_SLOTS );

	StatList_Int_t *pList = NULL;
	if( iSlot == 0 )
		pList = &m_PrimaryEquipCounts;
	else if( iSlot == 1 )
		pList = &m_SecondaryEquipCounts;
	else
		pList = &m_ExtraEquipCounts;

	int iFav = 0;
	for( int i=0; i<pList->Count(); ++i )
	{
		if( pList->operator []( iFav ) < pList->operator [](i) )
			iFav = i;
	}

	return iFav;
}

int CASW_Steamstats::GetFavoriteMarine( void )
{
	int iFav = 0;
	for( int i=0; i<m_MarineSelectionCounts.Count(); ++i )
	{
		if( m_MarineSelectionCounts[ iFav ] < m_MarineSelectionCounts[i] )
			iFav = i;
	}

	return iFav;
}

int CASW_Steamstats::GetFavoriteMarineClass( void )
{
	// Find the marine's class
	CASW_Marine_Profile *pProfile = MarineProfileList()->GetProfile( GetFavoriteMarine() );
	Assert( pProfile );
	return pProfile ? pProfile->GetMarineClass() : 0;
}

int CASW_Steamstats::GetFavoriteDifficulty( void )
{
	int iFav = 0;
	for( int i=0; i<m_DifficultyCounts.Count(); ++i )
	{
		if( m_DifficultyCounts[ iFav ] < m_DifficultyCounts[i] )
			iFav = i;
	}

	return iFav + 1;
}

float CASW_Steamstats::GetFavoriteEquipPercent( int iSlot )
{
	Assert( iSlot < ASW_MAX_EQUIP_SLOTS );

	StatList_Int_t *pList = NULL;
	if( iSlot == 0 )
		pList = &m_PrimaryEquipCounts;
	else if( iSlot == 1 )
		pList = &m_SecondaryEquipCounts;
	else
		pList = &m_ExtraEquipCounts;

	int iFav = 0;
	float fTotal = 0;
	for( int i=0; i<pList->Count(); ++i )
	{
		fTotal += pList->operator [](i);
		if( iFav < pList->operator [](i) )
			iFav = pList->operator [](i);
	}
	return ( fTotal > 0.0f ) ? ( iFav / fTotal * 100.0f ) : 0.0f;
}

float CASW_Steamstats::GetFavoriteMarinePercent( void )
{
	int iFav = 0;
	float fTotal = 0;
	for( int i=0; i<m_MarineSelectionCounts.Count(); ++i )
	{
		fTotal += m_MarineSelectionCounts[i];
		if( m_MarineSelectionCounts[iFav] < m_MarineSelectionCounts[i] )
			iFav = i;
	}

	return ( fTotal > 0.0f ) ? ( m_MarineSelectionCounts[iFav] / fTotal * 100.0f ) : 0.0f;
}

float CASW_Steamstats::GetFavoriteMarineClassPercent( void )
{
	int iFav = 0;
	float fTotal = 0;
	int iClassCounts[NUM_MARINE_CLASSES] = {0};
	for( int i=0; i<m_MarineSelectionCounts.Count(); ++i )
	{
		// Find the marine's class
		CASW_Marine_Profile *pProfile = MarineProfileList()->GetProfile( i );
		Assert( pProfile );
		if( !pProfile )
			continue;

		int iProfileClass = pProfile->GetMarineClass();
		iClassCounts[iProfileClass] += m_MarineSelectionCounts[i];
		fTotal += m_MarineSelectionCounts[i];
		if( iClassCounts[iFav] < iClassCounts[iProfileClass] )
			iFav = iProfileClass;
	}

	return ( fTotal > 0.0f ) ? ( iClassCounts[iFav] / fTotal * 100.0f ) : 0.0f;
}

float CASW_Steamstats::GetFavoriteDifficultyPercent( void )
{
	int iFav = 0;
	float fTotal = 0;
	for( int i=0; i<m_DifficultyCounts.Count(); ++i )
	{
		fTotal += m_DifficultyCounts[i];
		if( iFav < m_DifficultyCounts[i] )
			iFav = m_DifficultyCounts[i];
	}

	return ( fTotal > 0.0f ) ? ( iFav / fTotal * 100.0f ) : 0.0f;
}

bool DifficultyStats_t::FetchDifficultyStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID, int iDifficulty )
{
	if( !ASWGameRules() )
		return false;

	bool bOK = true;
	char* szDifficulty = NULL;

	switch( iDifficulty )
	{
		case 1: szDifficulty = "easy"; 
			break;
		case 2: szDifficulty = "normal";
			break;
		case 3: szDifficulty = "hard";
			break;
		case 4: szDifficulty = "insane";
			break;
		case 5: szDifficulty = "imba";
			break;
	}
	if( szDifficulty )
	{
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesTotal ), m_iGamesTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccess ), m_iGamesSuccess );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccessPercent ), m_fGamesSuccessPercent );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szKillsTotal ), m_iKillsTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szDamageTotal ), m_iDamageTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szFFTotal ), m_iFFTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szAccuracy ), m_fAccuracyAvg );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsHit ), m_iShotsHitTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsTotal ), m_iShotsFiredTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szHealingTotal ), m_iHealingTotal );
	}

	return bOK;
}

void DifficultyStats_t::PrepStatsForSend( CASW_Player *pPlayer )
{
	if( !steamapicontext )
		return;

	// Update stats from the briefing screen
	if( !GetDebriefStats() || !ASWGameResource() )
		return;

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( pMR )
	{
		int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
		if ( iMarineIndex != -1 )
		{
			m_iKillsTotal += GetDebriefStats()->GetKills( iMarineIndex );
			m_iFFTotal += GetDebriefStats()->GetFriendlyFire( iMarineIndex );
			m_iDamageTotal += GetDebriefStats()->GetDamageTaken( iMarineIndex );
			m_iShotsFiredTotal += GetDebriefStats()->GetShotsFired( iMarineIndex );
			m_iHealingTotal += GetDebriefStats()->GetHealthHealed( iMarineIndex );
			m_iShotsHitTotal += GetDebriefStats()->GetShotsHit( iMarineIndex );
			m_fAccuracyAvg = ( m_iShotsFiredTotal > 0 ) ? ( m_iShotsHitTotal / (float)m_iShotsFiredTotal * 100.0f ) : 0;
			m_iGamesTotal++;
			m_iGamesSuccess += ASWGameRules()->GetMissionSuccess() ? 1 : 0;
			m_fGamesSuccessPercent = m_iGamesSuccess / (float)m_iGamesTotal * 100.0f;
		}
	}
	char* szDifficulty = NULL;
	int iDifficulty = ASWGameRules()->GetSkillLevel();

	switch( iDifficulty )
	{
	case 1: szDifficulty = "easy"; 
		break;
	case 2: szDifficulty = "normal";
		break;
	case 3: szDifficulty = "hard";
		break;
	case 4: szDifficulty = "insane";
		break;
	case 5: szDifficulty = "imba";
		break;
	}
	if( szDifficulty )
	{
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesTotal ), m_iGamesTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccess ), m_iGamesSuccess );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccessPercent ), m_fGamesSuccessPercent );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szKillsTotal ), m_iKillsTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szDamageTotal ), m_iDamageTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szFFTotal ), m_iFFTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szAccuracy ), m_fAccuracyAvg );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsHit ), m_iShotsHitTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsTotal ), m_iShotsFiredTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szHealingTotal ), m_iHealingTotal );
	}
}

bool MissionStats_t::FetchMissionStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID )
{
	bool bOK = true;
	const char* szLevelName = NULL;
	if( !engine )
		return false;

	szLevelName = engine->GetLevelNameShort();
	if( !szLevelName )
		return false;

	// Fetch stats. Skip the averages, they're write only
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesTotal ), m_iGamesTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccess ), m_iGamesSuccess );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccessPercent ), m_fGamesSuccessPercent );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szKillsTotal ), m_iKillsTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szDamageTotal ), m_iDamageTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szFFTotal ), m_iFFTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeTotal ), m_iTimeTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szBestDifficulty ), m_iHighestDifficulty );
	for( int i=0; i<5; ++i )
	{
		FETCH_STEAM_STATS( CFmtStr( "%s%s.%s", szLevelName, szBestTime, g_szDifficulties[i] ), m_iBestSpeedrunTimes[i] );
	}

	return bOK;
}

void MissionStats_t::PrepStatsForSend( CASW_Player *pPlayer )
{
	// Update stats from the briefing screen
	if( !GetDebriefStats() || !ASWGameResource() )
		return;

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	int iDifficulty = ASWGameRules()->GetSkillLevel();
	if ( pMR )
	{
		int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
		if ( iMarineIndex != -1 )
		{
			m_iKillsTotal += GetDebriefStats()->GetKills( iMarineIndex );
			m_iFFTotal += GetDebriefStats()->GetFriendlyFire( iMarineIndex );
			m_iDamageTotal += GetDebriefStats()->GetDamageTaken( iMarineIndex );
			m_iGamesTotal++;
			m_iGamesSuccess += ASWGameRules()->GetMissionSuccess() ? 1 : 0;
			m_fGamesSuccessPercent = m_iGamesSuccess / (float)m_iGamesTotal * 100.0f;
			m_iTimeTotal += GetDebriefStats()->m_fTimeTaken;
			if( ASWGameRules()->GetMissionSuccess() )
			{
				if( iDifficulty > m_iHighestDifficulty )
					m_iHighestDifficulty = iDifficulty;

				if( (unsigned int)m_iBestSpeedrunTimes[ iDifficulty - 1 ] > GetDebriefStats()->m_fTimeTaken )
				{
					m_iBestSpeedrunTimes[ iDifficulty - 1 ] = GetDebriefStats()->m_fTimeTaken;
				}
			}
			
			
			// Safely compute averages
			m_fKillsAvg = m_iKillsTotal / (float)m_iGamesTotal;
			m_fFFAvg = m_iFFTotal / (float)m_iGamesTotal;
			m_fDamageAvg = m_iDamageTotal / (float)m_iGamesTotal;
			m_iTimeAvg = m_iTimeTotal / (float)m_iGamesTotal;
		}
	}

	const char* szLevelName = NULL;
	if( !engine )
		return;

	szLevelName = engine->GetLevelNameShort();
	if( !szLevelName )
		return;

	// Send stats
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesTotal ), m_iGamesTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccess ), m_iGamesSuccess );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccessPercent ), m_fGamesSuccessPercent );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szKillsTotal ), m_iKillsTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szDamageTotal ), m_iDamageTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szFFTotal ), m_iFFTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeTotal ), m_iTimeTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szKillsAvg ), m_fKillsAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szDamageAvg ), m_fDamageAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szFFAvg ), m_fFFAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeAvg ), m_iTimeAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szBestDifficulty ), m_iHighestDifficulty );
	SEND_STEAM_STATS( CFmtStr( "%s%s.%s", szLevelName, szBestTime, g_szDifficulties[ iDifficulty - 1 ] ), m_iBestSpeedrunTimes[ iDifficulty - 1 ] );
}

bool WeaponStats_t::FetchWeaponStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID, const char *szClassName )
{
	bool bOK = true;

	// Fetch stats. Skip the averages, they're write only
	FETCH_STEAM_STATS( CFmtStr( "equips.%s.damage", szClassName ), m_iDamage );
	FETCH_STEAM_STATS( CFmtStr( "equips.%s.ff_damage", szClassName ), m_iFFDamage );
	FETCH_STEAM_STATS( CFmtStr( "equips.%s.shots_fired", szClassName ), m_iShotsFired );
	FETCH_STEAM_STATS( CFmtStr( "equips.%s.shots_hit", szClassName ), m_iShotsHit );
	FETCH_STEAM_STATS( CFmtStr( "equips.%s.kills", szClassName ), m_iKills );

	return bOK;
}

void WeaponStats_t::PrepStatsForSend( CASW_Player *pPlayer )
{
	if( !GetDebriefStats() || !ASWGameResource() || !ASWEquipmentList() )
		return;

	// Check to see if the weapon is in the debrief stats
	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
		return;
	
	int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
	if ( iMarineIndex == -1 )
		return;

	
	if( !m_szClassName )
		return;
	int iDamage, iFFDamage, iShotsFired, iShotsHit, iKills = 0;

	if( GetDebriefStats()->GetWeaponStats( iMarineIndex, m_iWeaponIndex, iDamage, iFFDamage, iShotsFired, iShotsHit, iKills ) )
	{
		SEND_STEAM_STATS( CFmtStr( "equips.%s.damage", m_szClassName ), m_iDamage + iDamage );
		SEND_STEAM_STATS( CFmtStr( "equips.%s.ff_damage", m_szClassName ), m_iFFDamage + iFFDamage );
		SEND_STEAM_STATS( CFmtStr( "equips.%s.shots_fired", m_szClassName ), m_iShotsFired + iShotsFired );
		SEND_STEAM_STATS( CFmtStr( "equips.%s.shots_hit", m_szClassName ), m_iShotsHit + iShotsHit );
		SEND_STEAM_STATS( CFmtStr( "equips.%s.kills", m_szClassName ), m_iKills + iKills );
	}
}