#ifndef _INCLUDED_NB_VOTE_PANEL_H
#define _INCLUDED_NB_VOTE_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

class vgui::Label;
class CNB_Button;

class CNB_Progress_Bar : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Progress_Bar, vgui::EditablePanel );
public:
	CNB_Progress_Bar( vgui::Panel *parent, const char *name );
	virtual void Paint();

	void SetProgress( float flProgress );
	float GetProgress() { return m_flProgress; }

private:
	float m_flProgress;

	CPanelAnimationVarAliasType( int, m_nProgressBar, "HorizProgressBarTexture", "vgui/loadingbar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nProgressBarBG, "HorizProgressBarTextureBG", "vgui/loadingbar_bg", "textureid" );
};

// box showing current map/campaign vote

enum EVotePanelType
{
	VPT_BRIEFING = 0,
	VPT_INGAME,
	VPT_DEBRIEF,
	VPT_CAMPAIGN_MAP
};

enum EVotePanelStatus
{
	VPS_NONE = 0,
	VPS_VOTE,
	VPS_TECH_FAIL,
	VPS_RESTART	
};

class CNB_Vote_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Vote_Panel, vgui::EditablePanel );
public:
	CNB_Vote_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Vote_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnCommand( const char *command );
	virtual void OnTick();
	void UpdateVote();
	void UpdateVotePanelStatus();
	void UpdateVisibility();
	void UpdateVoteLabels();
	void UpdateRestartLabels();
	void UpdateProgressBar();
	void UpdateTechFailLabels();

	vgui::Panel	*m_pBackground;
	vgui::Panel	*m_pBackgroundInner;
	vgui::Panel	*m_pTitleBG;
	vgui::Panel	*m_pTitleBGBottom;
	vgui::Label	*m_pTitle;
	
	vgui::Label* m_pYesVotesLabel;
	vgui::Label* m_pNoVotesLabel;
	vgui::Label* m_pMapNameLabel;
	vgui::Label* m_pCounterLabel;

	vgui::Label* m_pPressToVoteYesLabel;
	vgui::Label* m_pPressToVoteNoLabel;

	float m_flCheckBindings;
	char m_szVoteYesKey[12];
	char m_szVoteNoKey[12];

	char m_szMapName[64];
	int m_iNoCount;
	int m_iYesCount;
	int m_iSecondsLeft;
	bool m_bVoteMapInstalled;

	EVotePanelType m_VotePanelType;
	EVotePanelStatus m_VotePanelStatus;

	CNB_Progress_Bar	*m_pProgressBar;
};

#endif // _INCLUDED_NB_VOTE_PANEL_H
