#ifndef _INCLUDED_MISSION_COMPLETE_STATS_LINE_H
#define _INCLUDED_MISSION_COMPLETE_STATS_LINE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/ImagePanel.h>

namespace vgui
{
	class IScheme;
};
class MedalArea;
class C_ASW_Debrief_Stats;
class StatsBar;

#define ASW_NUM_STATS_BARS 7

// a single line on the stats screen during debrief.  Shows various stats about how the marine played in that mission.

class MissionCompleteStatsLine : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MissionCompleteStatsLine, vgui::Panel );
public:
	MissionCompleteStatsLine(vgui::Panel *parent, const char *name);
	virtual ~MissionCompleteStatsLine();

	virtual void OnThink();
	virtual void PerformLayout();
	void ApplySchemeSettings( vgui::IScheme *scheme );
	void UpdateLabels();
	void SetMarineIndex(int i);
	void InitFrom(C_ASW_Debrief_Stats* pStats);
	void ShowStats();

	vgui::Label *m_pNameLabel;
	vgui::Label *m_pWoundedLabel;
	StatsBar *m_pStats[ASW_NUM_STATS_BARS];
	vgui::ImagePanel *m_pBarIcons[ASW_NUM_STATS_BARS];
	MedalArea *m_pMedalArea;
	Color m_pBGColor;
	char m_szCurrentName[64];
	int iCurrentShots;
	int iCurrentDamage;
	int iCurrentKills;
	int m_iMarineIndex;
	bool m_bInitStats;

	bool m_bSetAlpha;
};

// a line that shows a particular player and any medals he has won
class MissionCompletePlayerStatsLine : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MissionCompletePlayerStatsLine, vgui::Panel );
public:
	MissionCompletePlayerStatsLine(vgui::Panel *parent, const char *name);	

	virtual void OnThink();	
	virtual void PerformLayout();
	void ApplySchemeSettings( vgui::IScheme *scheme );
	void UpdateLabels();
	void SetPlayerIndex(int i);

	vgui::Label *m_pNameLabel;	
	MedalArea *m_pMedalArea;
	Color m_pBGColor;
	wchar_t m_wszCurrentName[ 128 ];
	int m_iPlayerIndex;
};


#endif // _INCLUDED_MISSION_COMPLETE_STATS_LINE_H
