#ifndef _INCLUDED_PLAYER_LIST_PANEL_H
#define _INCLUDED_PLAYER_LIST_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

namespace vgui
{
	class ImagePanel;
	class CheckButton;
	class Label;
};
class ImageButton;
class CNB_Header_Footer;
class CNB_Button;

class PlayerListLine;

#define MAX_PLAYER_LINES 8

// shows all the players in the current game, along with some info and buttons to vote kick or promote them,
//  and buttons for map voting
class PlayerListPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( PlayerListPanel, vgui::EditablePanel );
public:
	PlayerListPanel(vgui::Panel *parent, const char *name);
	virtual ~PlayerListPanel();
	
	virtual void OnThink();
	void UpdateKickLeaderTicks();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	void KickClicked(PlayerListLine* pLine);
	void LeaderClicked(PlayerListLine* pLine);

	CNB_Header_Footer *m_pHeaderFooter;
	vgui::Label *m_pPlayerHeader;
	vgui::Label *m_pMarinesHeader;
	vgui::Label *m_pPingHeader;
	
	// todo: button to start a map vote
	// todo: button to vote yes/no to a map vote
	
	int m_iKickVoteIndex;
	int m_iLeaderVoteIndex;

	CUtlVector<PlayerListLine*> m_PlayerLine;

	// voting buttons
	void UpdateVoteButtons();
	void SetShowStartVoteElements(bool bVisible);
	void SetShowCurrentVoteElements(bool bVisible);
	void OnCommand( char const *cmd );
	//void OnMouseReleased(vgui::MouseCode code);
	
	vgui::Panel* m_pVoteBackground;
	vgui::Label* m_pStartVoteTitle;
	CNB_Button* m_pStartCampaignVoteButton;
	ImageButton* m_pStartSavedCampaignVoteButton;

	CNB_Button* m_pRestartMissionButton;
	vgui::Panel* m_pLeaderButtonsBackground;

	vgui::Label* m_pCurrentVoteTitle;
	CNB_Button* m_pVoteYesButton;
	CNB_Button* m_pVoteNoButton;
	vgui::Label* m_pYesVotesLabel;
	vgui::Label* m_pNoVotesLabel;
	vgui::Label* m_pMapNameLabel;
	vgui::Label* m_pCounterLabel;

	char m_szMapName[64];
	int m_iNoCount;
	int m_iYesCount;
	int m_iSecondsLeft;
	bool m_bVoteMapInstalled;

	vgui::Label* m_pServerLabel;
	vgui::Label* m_pMissionLabel;
	vgui::Label* m_pDifficultyLabel;
	float m_fUpdateDifficultyTime;
	CPanelAnimationVar( vgui::HFont, m_hDefaultFont, "DefaultFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hDefaultLargeFont, "DefaultLargeFont", "DefaultLarge" );

	void UpdateServerName();
	char m_szServerName[128];
};

#endif // _INCLUDED_PLAYER_LIST_PANEL_H