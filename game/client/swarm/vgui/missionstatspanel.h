#ifndef _INCLUDED_MISSION_STATS_PANEL_H
#define _INCLUDED_MISSION_STATS_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/ImagePanel.h>
#include "asw_shareddefs.h"

class MissionCompleteStatsLine;
class MissionCompletePlayerStatsLine;
class C_ASW_Debrief_Stats;
class StatsBar;
namespace vgui
{
	class IScheme;
	class Button;
	class PanelListPanel;
};

#define MAX_STAT_LINES 8

// this panel is shown when a mission ends, showing various stats about the mission

class MissionStatsPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MissionStatsPanel, vgui::Panel );
public:
	MissionStatsPanel(vgui::Panel *parent, const char *name);

	virtual void OnThink();
	virtual void PerformLayout();
	void ApplySchemeSettings( vgui::IScheme *scheme );	
	void UpdateStatsLines();
	void ShowStats(bool bSuccess);
	void InitFrom(C_ASW_Debrief_Stats* pStats);

	vgui::PanelListPanel *m_pStatLineList;
	vgui::Panel* m_pStatLineContainer;
	vgui::Label* m_pTitle;
	vgui::Label* m_pMissionLabel;
	vgui::Label* m_pDifficultyLabel;
	vgui::Label* m_pUnlockedLabel;
	vgui::Label* m_pBestKillsLabel;
	vgui::Label* m_pBestTimeLabel;
	//RestartMissionButton* m_pRestartButton;
	//vgui::Button* m_pSaveCampaignButton;
	vgui::HFont m_LargeFont;
	vgui::HFont m_DefaultFont;

	StatsBar* m_pTimeTakenBar;
	StatsBar* m_pTotalKillsBar;
	vgui::ImagePanel* m_pTotalKillsIcon;
	vgui::ImagePanel* m_pTimeTakenIcon;

	MissionCompleteStatsLine* m_pStatLines[MAX_STAT_LINES];
	MissionCompletePlayerStatsLine* m_pPlayerLine[ASW_MAX_READY_PLAYERS];
	CHandle<C_ASW_Debrief_Stats> m_hStats;

	bool m_bSetAlpha;

	static void SetMissionLabels(vgui::Label *pMissionLabel, vgui::Label *pDifficultyLabel);
};


#endif // _INCLUDED_MISSION_STATS_PANEL_H
