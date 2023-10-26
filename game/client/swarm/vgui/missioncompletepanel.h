#ifndef _INCLUDED_MISSION_COMPLETE_PANEL_H
#define _INCLUDED_MISSION_COMPLETE_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "asw_shareddefs.h"

class RestartMissionButton;
class ReturnCampaignMapButton;
class MissionStatsPanel;
class BriefingPropertySheet;
class CExperienceReport;
class StatsReport;
class BriefingStartButton;
class CNB_Header_Footer;
class CNB_Button;
class CMission_Complete_Message;
class CNB_Vote_Panel;
namespace vgui
{
	class IScheme;
	class Button;
	class ImagePanel;
};

#define MAX_STAT_LINES 8
#define ASW_MAX_MISSION_COMPLETE_TABS 2

// this panel is shown when a mission ends (contains child elements for showing success/fail messages, stats, etc.)

class MissionCompletePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( MissionCompletePanel, vgui::EditablePanel );
public:
	MissionCompletePanel(vgui::Panel *parent, const char *name, bool bSuccess);
	virtual ~MissionCompletePanel();

	virtual void OnThink();
	//virtual void OnScreenSizeChanged(int iOldWide, int iOldTall);
	virtual void PerformLayout();
	void ApplySchemeSettings( vgui::IScheme *scheme );
	void ShowImageAndPlaySound();	
	void OnCommand(const char *command);

	vgui::Panel* m_pMainElements;	// all the main elements of debrief are put into this subpanel to keep it behind the win screen

	CNB_Header_Footer *m_pHeaderFooter;	
	CNB_Header_Footer *m_pFailedHeaderFooter;	
	CMission_Complete_Message* m_pResultImage;	// success/fail
	vgui::Label* m_pFailAdvice;	// strat advice
	vgui::Label * m_pMissionName;
	vgui::ImagePanel* m_pIconForwardArrow;	// arrow to the left of the fail hint
	CNB_Button *m_pRestartButton;
	vgui::ImagePanel	*m_pReadyCheckImage;
	CNB_Button *m_pReadyButton;
	CNB_Button *m_pContinueButton;
	CNB_Button *m_pNextButton;
	StatsReport* m_pStatsPanel;
	vgui::HFont m_LargeFont;
	CNB_Vote_Panel *m_pVotePanel;
	bool m_bSuccess;
	int m_iStage;
	float m_fNextStageTime;
	bool m_bViewedStatsPage;

	BriefingPropertySheet *m_PropertySheet;
	CExperienceReport *m_pExperienceReport;

	void UpdateVisibleButtons();

	CNB_Button *m_pTab[ ASW_MAX_MISSION_COMPLETE_TABS ];

	enum
	{
		MCP_STAGE_INITIAL_DELAY,
		MCP_STAGE_FAILSUCCESS,
		MCP_STAGE_FAILSUCCESS_FADE,
		MCP_STAGE_STATS
	};

	bool m_bSetAlpha;

	void OnWeaponUnlocked( const char *pszWeaponClass );
	void ShowQueuedUnlockPanels();
	void OnSuggestDifficulty( bool bIncrease );
	vgui::DHANDLE<vgui::Panel> m_hSubScreen;

	bool m_bCreditsSeen;
	float m_flForceVisibleButtonsTime;

	bool m_bShowQueuedUnlocks;
	void UpdateQueuedUnlocks();
	CUtlVector<const char*> m_aUnlockedWeapons;
};


#endif // _INCLUDED_MISSION_COMPLETE_PANEL_H
