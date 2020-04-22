//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#include "cbase.h"
#include "lifetime_stats_page.h"
#include <vgui_controls/SectionedListPanel.h>
#include "cs_client_gamestats.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CLifetimeStatsPage::CLifetimeStatsPage(vgui::Panel *parent, const char *name) : BaseClass(parent, "CSLifetimeStatsDialog")
{
	m_allStatsGroupPanel =					AddGroup( L"all",      "Stats_Button_All",       L"All" );
	m_detailedWeaponStatsGroupPanel =		AddGroup( L"weapon",   "Stats_Button_Weapon",    L"Weapon Stats" );
	m_specialSkillsStatsGroupPanel =		AddGroup( L"skills",   "Stats_Button_Skills",    L"Special Skills" );
	m_mapAndMiscellanyStatsGroupPanel =		AddGroup( L"map",      "Stats_Button_Misc",      L"Miscellaneous" );
	m_mapVictoryStatsGroupPanel =			AddGroup( L"map",      "Stats_Button_Victories", L"Map Victories" );
	m_missionAndObjectiveStatsGroupPanel =	AddGroup( L"mission",  "Stats_Button_Mission",   L"Mission && Objectives" );

	m_allStatsGroupPanel->SetGroupActive(true);
}
//-----------------------------------------------------------------------------
// Purpose: Loads settings from statsdialog.res in hl2/resource/ui/
//-----------------------------------------------------------------------------
void CLifetimeStatsPage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	LoadControlSettings("resource/ui/CSLifetimeStatsDialog.res");

	m_statsList->AddColumnToSection( 0, "name", "", SectionedListPanel::COLUMN_CENTER, 280);
	m_statsList->AddColumnToSection( 0, "playerValue", "", SectionedListPanel::COLUMN_CENTER, 250);
}

void CLifetimeStatsPage::RepopulateStats()
{
	m_statsList->RemoveAll();

	const StatsCollection_t& personalLifetimeStats = g_CSClientGameStats.GetLifetimeStats();

	if (m_missionAndObjectiveStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_ROUNDS_WON, personalLifetimeStats);
		AddSimpleStat(CSSTAT_KILLS, personalLifetimeStats);
		AddSimpleStat(CSSTAT_DEATHS, personalLifetimeStats);
		AddKillToDeathStat(personalLifetimeStats);
		AddSimpleStat(CSSTAT_DAMAGE, personalLifetimeStats);
		AddSimpleStat(CSSTAT_NUM_BOMBS_PLANTED, personalLifetimeStats);
		AddSimpleStat(CSSTAT_NUM_BOMBS_DEFUSED, personalLifetimeStats);
		AddSimpleStat(CSSTAT_NUM_HOSTAGES_RESCUED, personalLifetimeStats);
		AddSimpleStat(CSSTAT_PISTOLROUNDS_WON, personalLifetimeStats);
	}

	if (m_specialSkillsStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_MVPS, personalLifetimeStats);
		AddFavoriteWeaponStat(personalLifetimeStats);
		AddSimpleStat(CSSTAT_SHOTS_FIRED, personalLifetimeStats);
		AddSimpleStat(CSSTAT_SHOTS_HIT, personalLifetimeStats);
		AddAccuracyStat(personalLifetimeStats);
		AddSimpleStat(CSSTAT_DOMINATIONS, personalLifetimeStats);
		AddSimpleStat(CSSTAT_DOMINATION_OVERKILLS, personalLifetimeStats);
		AddSimpleStat(CSSTAT_REVENGES, personalLifetimeStats);
		AddSimpleStat(CSSTAT_KILLS_HEADSHOT, personalLifetimeStats);
		AddSimpleStat(CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, personalLifetimeStats);
		AddSimpleStat(CSSTAT_KILLS_ENEMY_BLINDED, personalLifetimeStats);
		AddSimpleStat(CSSTAT_KILLS_ENEMY_WEAPON, personalLifetimeStats);
		AddSimpleStat(CSSTAT_KILLS_KNIFE_FIGHT, personalLifetimeStats);
	}

	if (m_mapAndMiscellanyStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_MONEY_EARNED, personalLifetimeStats);
		AddSimpleStat(CSSTAT_DECAL_SPRAYS, personalLifetimeStats);
		AddSimpleStat(CSSTAT_NIGHTVISION_DAMAGE, personalLifetimeStats);
		AddSimpleStat(CSSTAT_NUM_BROKEN_WINDOWS, personalLifetimeStats);
		AddSimpleStat(CSSTAT_WEAPONS_DONATED, personalLifetimeStats);
	}

	if (m_mapVictoryStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_MAP_WINS_CS_ASSAULT, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_CS_COMPOUND, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_CS_HAVANA, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_CS_ITALY, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_CS_MILITIA, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_CS_OFFICE, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_AZTEC, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_CBBLE, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_CHATEAU, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_DUST2, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_DUST, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_INFERNO, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_NUKE, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_PIRANESI, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_PORT, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_PRODIGY, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_TIDES, personalLifetimeStats);
		AddSimpleStat(CSSTAT_MAP_WINS_DE_TRAIN, personalLifetimeStats);
	}


	if (m_detailedWeaponStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		CSStatType_t hitsStat = CSSTAT_HITS_DEAGLE;
		CSStatType_t shotsStat = CSSTAT_SHOTS_DEAGLE;
		for (CSStatType_t killStat = CSSTAT_KILLS_DEAGLE ; killStat <= CSSTAT_KILLS_HEGRENADE ; killStat = (CSStatType_t)(killStat + 1))
		{
			if (shotsStat <= CSSTAT_SHOTS_M249)
			{
				AddSimpleStat(shotsStat, personalLifetimeStats);
			}
			if (hitsStat <= CSSTAT_HITS_M249)
			{
				AddSimpleStat(hitsStat, personalLifetimeStats);
			}
			AddSimpleStat(killStat, personalLifetimeStats);

			hitsStat = (CSStatType_t)(hitsStat + 1);
			shotsStat = (CSStatType_t)(shotsStat + 1);
		}
	}

	int itemCount = m_statsList->GetItemCount();
	for (int i = 0; i < itemCount ; ++i)
	{
		m_statsList->SetItemBgColor(i, Color(0,0,0,0));
	}
}

int CLifetimeStatsPage::AddSimpleStat( int desiredStat, const StatsCollection_t& personalLifetimeStats)
{
	PlayerStatData_t stat = g_CSClientGameStats.GetStatById(desiredStat);

	KeyValues *pKeyValues = new KeyValues( "data" );
	pKeyValues->SetWString( "name", stat.pStatDisplayName );
	pKeyValues->SetFloat( "playerValue", 0 );

	char buf[64];
	Q_snprintf( buf, sizeof( buf ), "%d", personalLifetimeStats[stat.iStatId] );
	pKeyValues->SetString( "playerValue", (personalLifetimeStats[stat.iStatId])?buf:"" );

	int newItem = m_statsList->AddItem(0, pKeyValues);
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}

int CLifetimeStatsPage::AddFavoriteWeaponStat(const StatsCollection_t& personalLifetimeStats)
{
	PlayerStatData_t statPlayerBestWeapon;
	statPlayerBestWeapon.iStatId = CSSTAT_UNDEFINED;

	int previousBestKills = 0;

	for (CSStatType_t statId = CSSTAT_KILLS_DEAGLE ; statId <= CSSTAT_KILLS_M249; statId = (CSStatType_t)(statId + 1))
	{
		PlayerStatData_t stat = g_CSClientGameStats.GetStatById( statId );

		//Compare this to previous Weapons
		int playerKillsWithWeapon = personalLifetimeStats[statId];
		{
			if (playerKillsWithWeapon > previousBestKills)
			{
				statPlayerBestWeapon = stat;
				previousBestKills = playerKillsWithWeapon;
			}
		}
	}

	KeyValues *pKeyValues = new KeyValues( "data" );
	pKeyValues->SetWString( "name", LocalizeTagOrUseDefault( "Stats_FavoriteWeapon", L"Favorite Weapon" ) );

	pKeyValues->SetWString( "playerValue", TranslateWeaponKillIDToAlias(statPlayerBestWeapon.iStatId) );

	int newItem = m_statsList->AddItem(0, pKeyValues);
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}


int CLifetimeStatsPage::AddKillToDeathStat(const StatsCollection_t& personalLifetimeStats)
{
	PlayerStatData_t statKills = g_CSClientGameStats.GetStatById(CSSTAT_KILLS);
	PlayerStatData_t statDeaths = g_CSClientGameStats.GetStatById(CSSTAT_DEATHS);

	KeyValues *pKeyValues = new KeyValues( "data" );
	pKeyValues->SetWString( "name", LocalizeTagOrUseDefault( "Stats_KillToDeathRatio", L"Kill to Death Ratio" ) );
	pKeyValues->SetFloat( "playerValue", 0 );

	float playerKills = personalLifetimeStats[statKills.iStatId];
	float playerDeaths = personalLifetimeStats[statDeaths.iStatId];
	float playerKillToDeathRatio = 1.0f;

	if (playerDeaths > 0)
	{
		playerKillToDeathRatio = playerKills / playerDeaths;
	}

	char buf[64];
	Q_snprintf( buf, sizeof( buf ), "%.2f", playerKillToDeathRatio );
	pKeyValues->SetString( "playerValue", (playerKillToDeathRatio)?buf:""  );

	int newItem = m_statsList->AddItem(0, pKeyValues);
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}

int CLifetimeStatsPage::AddAccuracyStat(const StatsCollection_t& personalLifetimeStats)
{
	PlayerStatData_t statHits = g_CSClientGameStats.GetStatById(CSSTAT_SHOTS_HIT);
	PlayerStatData_t statShots = g_CSClientGameStats.GetStatById(CSSTAT_SHOTS_FIRED);

	KeyValues *pKeyValues = new KeyValues( "data" );
	pKeyValues->SetWString( "name", LocalizeTagOrUseDefault( "Stats_Accuracy", L"Accuracy" ) );
	pKeyValues->SetFloat( "playerValue", 0 );

	float playerHits = personalLifetimeStats[statHits.iStatId];
	float playerShots = personalLifetimeStats[statShots.iStatId];
	float playerAccuracy = 0.0;

	if (playerShots > 0)
	{
		playerAccuracy = 100.0f * playerHits / playerShots;
	}

	char buf[64];
	Q_snprintf( buf, sizeof( buf ), "%.1f%%", playerAccuracy );
	pKeyValues->SetString( "playerValue", (playerAccuracy)?buf:""  );

	int newItem = m_statsList->AddItem(0, pKeyValues);
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}

