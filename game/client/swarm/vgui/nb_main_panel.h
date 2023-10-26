#ifndef _INCLUDED_NB_MAIN_PANEL_H
#define _INCLUDED_NB_MAIN_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "vgui_bitmapbutton.h"

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class vgui::Panel;
class vgui::Button;
class vgui::ImagePanel;
class CNB_Lobby_Row;
class CNB_Lobby_Row_Small;
class CNB_Lobby_Tooltip;
class CNB_Mission_Summary;
class CNB_Header_Footer;
class CNB_Button;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Vote_Panel;
class CBitmapButton;

class CNB_Main_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Main_Panel, vgui::EditablePanel );
public:
	CNB_Main_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Main_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void ChangeMarine( int nLobbySlot );
	void ChangeWeapon( int nLobbySlot, int nInventorySlot );
	void SpendSkillPointsOnMarine( int nProfileIndex );
	void ShowMissionDetails();
	void ShowMissionOptions();
	void ShowPromotionPanel();

	vgui::DHANDLE<vgui::Panel> m_hSubScreen;
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	CNB_Header_Footer *m_pHeaderFooter;
	vgui::Label	*m_pLeaderLabel;
	
	vgui::ImagePanel	*m_pReadyCheckImage;
	CNB_Lobby_Row	*m_pLobbyRow0;
	CNB_Lobby_Row_Small	*m_pLobbyRow1;
	CNB_Lobby_Row_Small	*m_pLobbyRow2;
	CNB_Lobby_Row_Small	*m_pLobbyRow3;
	CNB_Lobby_Tooltip	*m_pLobbyTooltip;
	CNB_Mission_Summary	*m_pMissionSummary;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pReadyButton;
	CNB_Button	*m_pMissionDetailsButton;
	CNB_Button	*m_pFriendsButton;
	CNB_Button	*m_pPromotionButton;
	CNB_Vote_Panel *m_pVotePanel;
	CBitmapButton *m_pChatButton;
	CBitmapButton *m_pVoteButton;

	bool m_bLocalLeader;
	uint64 m_FlyoutSteamID;

	void ProcessSkillSpendQueue();
	void OnFinishedSpendingSkillPoints();

	static void QueueSpendSkillPoints( int nProfileIndex );
	static void RemoveFromSpendQueue( int nProfileIndex );
	static CUtlVector<int> s_QueuedSpendSkillPoints;	// queue of marine profile indices to spend points on
};

#endif // _INCLUDED_NB_MAIN_PANEL_H














