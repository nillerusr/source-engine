//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#include "cbase.h"
#include "match_stats_page.h"
#include <vgui_controls/SectionedListPanel.h>
#include "c_team.h"
#include "cs_client_gamestats.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CMatchStatsPage::CMatchStatsPage(vgui::Panel *parent, const char *name) : BaseClass(parent, "CSMatchStatsDialog")
{	
	m_allStatsGroupPanel =					AddGroup( L"all",      "Stats_Button_All",       L"All" );
	m_detailedWeaponStatsGroupPanel =		AddGroup( L"weapon",   "Stats_Button_Weapon",    L"Weapon Stats" );
	m_specialSkillsStatsGroupPanel =		AddGroup( L"skills",   "Stats_Button_Skills",    L"Special Skills" );
	m_mapAndMiscellanyStatsGroupPanel =		AddGroup( L"map",      "Stats_Button_Misc",      L"Miscellaneous" );
	m_missionAndObjectiveStatsGroupPanel =	AddGroup( L"mission",  "Stats_Button_Mission",   L"Mission && Objectives" );

	m_allStatsGroupPanel->SetGroupActive(true);

	m_ResetNotice = new Label( this, "ResetNotice", "");
}
//-----------------------------------------------------------------------------
// Purpose: Loads settings from statsdialog.res in hl2/resource/ui/
//-----------------------------------------------------------------------------
void CMatchStatsPage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );
    LoadControlSettings("resource/ui/CSMatchStatsDialog.res");  
	m_statsList->AddColumnToSection( 0, "name", "", SectionedListPanel::COLUMN_CENTER, 180);
	m_statsList->AddColumnToSection( 0, "playerValue", "", SectionedListPanel::COLUMN_CENTER, 100);
	m_statsList->AddColumnToSection( 0, "ctValue", "", SectionedListPanel::COLUMN_CENTER, 100);
	m_statsList->AddColumnToSection( 0, "tValue", " ", SectionedListPanel::COLUMN_CENTER, 100);	
	m_statsList->AddColumnToSection( 0, "serverValue", "", SectionedListPanel::COLUMN_CENTER, 100);	
}

void CMatchStatsPage::RepopulateStats()
{
	m_statsList->RemoveAll();

	RoundStatsDirectAverage_t*	tStats = g_CSClientGameStats.GetDirectTStatsAverages();
	RoundStatsDirectAverage_t*	ctStats = g_CSClientGameStats.GetDirectCTStatsAverages();
	RoundStatsDirectAverage_t*	serverStats = g_CSClientGameStats.GetDirectPlayerStatsAverages();
	const StatsCollection_t&	personalMatchStats = g_CSClientGameStats.GetMatchStats();
	
	if (m_missionAndObjectiveStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_ROUNDS_WON, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_KILLS, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_DEATHS, personalMatchStats, tStats, ctStats, serverStats);
		AddKillToDeathStat(personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_DAMAGE, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_NUM_BOMBS_PLANTED, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_NUM_BOMBS_DEFUSED, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_NUM_HOSTAGES_RESCUED, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_PISTOLROUNDS_WON, personalMatchStats, tStats, ctStats, serverStats);
	}

	if (m_specialSkillsStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_MVPS, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_SHOTS_FIRED, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_SHOTS_HIT, personalMatchStats, tStats, ctStats, serverStats);		
		AddAccuracyStat(personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_DOMINATIONS, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_DOMINATION_OVERKILLS, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_REVENGES, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_KILLS_HEADSHOT, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_KILLS_ENEMY_BLINDED, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_KILLS_ENEMY_WEAPON, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_KILLS_KNIFE_FIGHT, personalMatchStats, tStats, ctStats, serverStats);
	}

	if (m_mapAndMiscellanyStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		AddSimpleStat(CSSTAT_MONEY_EARNED, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_DECAL_SPRAYS, personalMatchStats, tStats, ctStats, serverStats);		
		AddSimpleStat(CSSTAT_NIGHTVISION_DAMAGE, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_NUM_BROKEN_WINDOWS, personalMatchStats, tStats, ctStats, serverStats);
		AddSimpleStat(CSSTAT_WEAPONS_DONATED, personalMatchStats, tStats, ctStats, serverStats);
	}	

	if (m_detailedWeaponStatsGroupPanel->IsGroupActive() || m_allStatsGroupPanel->IsGroupActive())
	{
		CSStatType_t hitsStat = CSSTAT_HITS_DEAGLE;
		CSStatType_t shotsStat = CSSTAT_SHOTS_DEAGLE;
		for (CSStatType_t killStat = CSSTAT_KILLS_DEAGLE ; killStat <= CSSTAT_KILLS_HEGRENADE ; killStat = (CSStatType_t)(killStat + 1))
		{
			if (shotsStat <= CSSTAT_SHOTS_M249)
			{
				AddSimpleStat(shotsStat, personalMatchStats, tStats, ctStats, serverStats);
		}

			if (hitsStat <= CSSTAT_HITS_M249)
			{
				AddSimpleStat(hitsStat, personalMatchStats, tStats, ctStats, serverStats);
	}
			AddSimpleStat(killStat, personalMatchStats, tStats, ctStats, serverStats);

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

int CMatchStatsPage::AddSimpleStat( int desiredStat, const StatsCollection_t& personalMatchStats, RoundStatsDirectAverage_t* tStats, RoundStatsDirectAverage_t* ctStats, RoundStatsDirectAverage_t* serverStats )
{
	PlayerStatData_t stat = g_CSClientGameStats.GetStatById(desiredStat);

	KeyValues *pKeyValues = new KeyValues( "data" );
	pKeyValues->SetWString( "name", stat.pStatDisplayName );
	pKeyValues->SetFloat( "playerValue", 0 );

	char buf[64];
	Q_snprintf( buf, sizeof( buf ), "%d", personalMatchStats[stat.iStatId] );
	pKeyValues->SetString( "playerValue", (personalMatchStats[stat.iStatId])?buf:"" );

	if (desiredStat == CSSTAT_ROUNDS_WON)
	{
		C_Team *ts = GetGlobalTeam(TEAM_TERRORIST);
		if (ts)
		{
			Q_snprintf( buf, sizeof( buf ), "%d", ts->Get_Score() );
			pKeyValues->SetString( "tValue", buf );
		}

		C_Team *cts = GetGlobalTeam(TEAM_CT);
		if (cts){
			Q_snprintf( buf, sizeof( buf ), "%d", cts->Get_Score());
			pKeyValues->SetString( "ctValue", buf );
		}
	}
	else
	{
		Q_snprintf( buf, sizeof( buf ), "%.1f", tStats->m_fStat[stat.iStatId] );
		pKeyValues->SetString( "tValue", (tStats->m_fStat[stat.iStatId]||personalMatchStats[stat.iStatId])?buf:"" );

		Q_snprintf( buf, sizeof( buf ), "%.1f", ctStats->m_fStat[stat.iStatId] );
		pKeyValues->SetString( "ctValue", (ctStats->m_fStat[stat.iStatId]||personalMatchStats[stat.iStatId])?buf:"" );
	}

	Q_snprintf( buf, sizeof( buf ), "%.1f", serverStats->m_fStat[stat.iStatId] );
	pKeyValues->SetString( "serverValue", (serverStats->m_fStat[stat.iStatId]||personalMatchStats[stat.iStatId])?buf:"" );

	int newItem = m_statsList->AddItem(0, pKeyValues);		
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}


int CMatchStatsPage::AddKillToDeathStat(const StatsCollection_t& personalMatchStats, RoundStatsDirectAverage_t* tStats, RoundStatsDirectAverage_t* ctStats, RoundStatsDirectAverage_t* serverStats )
{
	PlayerStatData_t statKills = g_CSClientGameStats.GetStatById(CSSTAT_KILLS);
	PlayerStatData_t statDeaths = g_CSClientGameStats.GetStatById(CSSTAT_DEATHS);

	KeyValues *pKeyValues = new KeyValues( "data" );

	pKeyValues->SetWString( "name", LocalizeTagOrUseDefault( "Stats_KillToDeathRatio", L"Kill to Death Ratio" ) );
	pKeyValues->SetFloat( "playerValue", 0 );

	float playerKills = personalMatchStats[statKills.iStatId];
	float tKills = tStats->m_fStat[statKills.iStatId];
	float ctKills = ctStats->m_fStat[statKills.iStatId];
	float serverKills = serverStats->m_fStat[statKills.iStatId];

	float playerDeaths = personalMatchStats[statDeaths.iStatId];
	float tDeaths = tStats->m_fStat[statDeaths.iStatId];
	float ctDeaths = ctStats->m_fStat[statDeaths.iStatId];
	float serverDeaths = serverStats->m_fStat[statDeaths.iStatId];
	
	char buf[64];

	if (playerDeaths > 0)
	{
		float playerKillToDeathRatio = playerKills / playerDeaths;
		Q_snprintf( buf, sizeof( buf ), "%.2f", playerKillToDeathRatio );
		pKeyValues->SetString( "playerValue", (playerKillToDeathRatio)?buf:"" );
	}
	else
	{
		pKeyValues->SetString( "playerValue", "" );
	}


	if (tDeaths > 0)
	{
		float tKillToDeathRatio = tKills / tDeaths;
		Q_snprintf( buf, sizeof( buf ), "%.2f", tKillToDeathRatio );
		pKeyValues->SetString( "tValue", ((playerKills&&playerDeaths)||tKillToDeathRatio)?buf:"" );	

	}
	else
	{
		pKeyValues->SetString( "tValue", "" );
	}

	if (ctDeaths > 0)
	{
		float ctKillToDeathRatio = ctKills / ctDeaths;
		Q_snprintf( buf, sizeof( buf ), "%.2f", ctKillToDeathRatio);
		pKeyValues->SetString( "ctValue", ((playerKills&&playerDeaths)||ctKillToDeathRatio)?buf:"" );		
	}
	else
	{
		pKeyValues->SetString( "ctValue", "" );
	}

	if (serverDeaths > 0)
	{
		float serverKillToDeathRatio = serverKills / serverDeaths;
		Q_snprintf( buf, sizeof( buf ), "%.2f", serverKillToDeathRatio );
		pKeyValues->SetString( "serverValue", ((playerKills&&playerDeaths)||serverKillToDeathRatio)?buf:"" );		
	}
	else
	{
		pKeyValues->SetString( "serverValue", "" );
	}

	

	int newItem = m_statsList->AddItem(0, pKeyValues);		
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}



int CMatchStatsPage::AddAccuracyStat(const StatsCollection_t& personalMatchStats, RoundStatsDirectAverage_t* tStats, RoundStatsDirectAverage_t* ctStats, RoundStatsDirectAverage_t* serverStats )
{
	PlayerStatData_t statHits = g_CSClientGameStats.GetStatById(CSSTAT_SHOTS_HIT);
	PlayerStatData_t statShots = g_CSClientGameStats.GetStatById(CSSTAT_SHOTS_FIRED);

	KeyValues *pKeyValues = new KeyValues( "data" );
	pKeyValues->SetWString( "name", LocalizeTagOrUseDefault( "Stats_Accuracy", L"Accuracy" ) );
	pKeyValues->SetFloat( "playerValue", 0 );

	float playerHits = personalMatchStats[statHits.iStatId];
	float tHits = tStats->m_fStat[statHits.iStatId];
	float ctHits = ctStats->m_fStat[statHits.iStatId];
	float serverHits = serverStats->m_fStat[statHits.iStatId];

	float playerShots = personalMatchStats[statShots.iStatId];
	float tShots = tStats->m_fStat[statShots.iStatId];
	float ctShots = ctStats->m_fStat[statShots.iStatId];
	float serverShots = serverStats->m_fStat[statShots.iStatId];

	float playerAccuracy = 0.0;
	float tAccuracy = 0.0;
	float ctAccuracy = 0.0;
	float serverAccuracy = 0.0;

	if (playerShots > 0)
	{
		playerAccuracy = 100.0f * playerHits / playerShots;
	}

	if (tShots > 0)
	{
		tAccuracy = 100.0f * tHits / tShots;
	}

	if (ctShots > 0)
	{
		ctAccuracy = 100.0f * ctHits / ctShots;
	}

	if (serverShots > 0)
	{
		serverAccuracy = 100.0f * serverHits / serverShots;
	}

	char buf[64];
	Q_snprintf( buf, sizeof( buf ), "%.1f%%", playerAccuracy );
	pKeyValues->SetString( "playerValue", (playerAccuracy)?buf:"" );

	Q_snprintf( buf, sizeof( buf ), "%.1f%%", tAccuracy);
	pKeyValues->SetString( "tValue", (playerAccuracy||tAccuracy)?buf:"" );

	Q_snprintf( buf, sizeof( buf ), "%.1f%%", ctAccuracy );
	pKeyValues->SetString( "ctValue", (playerAccuracy||ctAccuracy)?buf:"" );

	Q_snprintf( buf, sizeof( buf ), "%.1f%%", serverAccuracy );
	pKeyValues->SetString( "serverValue", (playerAccuracy||serverAccuracy)?buf:"" );

	int newItem = m_statsList->AddItem(0, pKeyValues);		
	pKeyValues->deleteThis();

	m_statsList->SetItemFont(newItem , m_listItemFont);
	m_statsList->SetItemFgColor(newItem, Color(197,197,197,255));

	return newItem;
}


void CMatchStatsPage::OnSizeChanged(int newWide, int newTall)
{
	BaseClass::OnSizeChanged(newWide, newTall);

	if (m_statsList)
	{
		if (m_ResetNotice)
		{
			int labelX, labelY, listX, listY, listWide, listTall;
			m_statsList->GetBounds(listX, listY, listWide, listTall);

			m_ResetNotice->GetPos(labelX, labelY);
			m_ResetNotice->SetPos(labelX, listY + listTall + 4);
		}
	}
}
