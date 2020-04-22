//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSLifetimeSTATSPAGE_H
#define CSLifetimeSTATSPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "base_stats_page.h"

class CLifetimeStatsPage : public CBaseStatsPage
{
    DECLARE_CLASS_SIMPLE ( CLifetimeStatsPage, CBaseStatsPage );

public:
    CLifetimeStatsPage( vgui::Panel *parent, const char *name );
	
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void RepopulateStats();
	int AddSimpleStat( int desiredStat, const StatsCollection_t& personalLifetimeStats);
	int AddAccuracyStat(const StatsCollection_t& personalLifetimeStats);
	int AddFavoriteWeaponStat(const StatsCollection_t& personalLifetimeStats);
	int AddKillToDeathStat(const StatsCollection_t& personalLifetimeStats);	

	CBaseStatGroupPanel* m_allStatsGroupPanel;
	CBaseStatGroupPanel* m_detailedWeaponStatsGroupPanel;
	CBaseStatGroupPanel* m_specialSkillsStatsGroupPanel;
	CBaseStatGroupPanel* m_mapAndMiscellanyStatsGroupPanel;
	CBaseStatGroupPanel* m_mapVictoryStatsGroupPanel;
	CBaseStatGroupPanel* m_missionAndObjectiveStatsGroupPanel;
	

};

#endif // CSLifetimeSTATSPAGE_H
