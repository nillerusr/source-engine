//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSMATCHSTATSPAGE_H
#define CSMATCHSTATSPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "base_stats_page.h"

class CMatchStatsPage : public CBaseStatsPage
{
    DECLARE_CLASS_SIMPLE ( CMatchStatsPage, CBaseStatsPage );

public:
    CMatchStatsPage( vgui::Panel *parent, const char *name );
	
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void RepopulateStats();
	int AddSimpleStat( int desiredStat, const StatsCollection_t& personalMatchStats, RoundStatsDirectAverage_t* tStats, RoundStatsDirectAverage_t* ctStats, RoundStatsDirectAverage_t* serverStats );
	int AddAccuracyStat(const StatsCollection_t& personalMatchStats, RoundStatsDirectAverage_t* tStats, RoundStatsDirectAverage_t* ctStats, RoundStatsDirectAverage_t* serverStats );
	int AddKillToDeathStat(const StatsCollection_t& personalMatchStats, RoundStatsDirectAverage_t* tStats, RoundStatsDirectAverage_t* ctStats, RoundStatsDirectAverage_t* serverStats );
	virtual void OnSizeChanged(int wide, int tall);

	CBaseStatGroupPanel* m_allStatsGroupPanel;
	CBaseStatGroupPanel* m_detailedWeaponStatsGroupPanel;
	CBaseStatGroupPanel* m_specialSkillsStatsGroupPanel;
	CBaseStatGroupPanel* m_mapAndMiscellanyStatsGroupPanel;
	CBaseStatGroupPanel* m_missionAndObjectiveStatsGroupPanel;

	vgui::Label*	m_ResetNotice;
};

#endif // CSMATCHSTATSPAGE_H
