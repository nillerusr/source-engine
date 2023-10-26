#ifndef _INCLUDED_NB_LOBBY_TOOLTIP_H
#define _INCLUDED_NB_LOBBY_TOOLTIP_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Panel;
class vgui::Label;
class CNB_Skill_Panel;
class CNB_Weapon_Detail;
class CASW_Model_Panel;
// == MANAGED_CLASS_DECLARATIONS_END ==
class vgui::ImagePanel;
class vgui::Label;

class CNB_Lobby_Tooltip : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Lobby_Tooltip, vgui::EditablePanel );
public:
	CNB_Lobby_Tooltip( vgui::Panel *parent, const char *name );
	virtual ~CNB_Lobby_Tooltip();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnTick();

	void ShowMarineTooltip( int nLobbySlot );
	void ShowWeaponTooltip( int nLobbySlot, int nInventorySlot );
	void ShowMarinePromotionTooltip( int nLobbySlot );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Panel	*m_pBackground;
	vgui::Panel	*m_pBackgroundInner;
	vgui::Panel	*m_pTitleBG;
	vgui::Panel	*m_pTitleBGBottom;
	vgui::Label	*m_pTitle;
	CNB_Skill_Panel	*m_pSkillPanel0;
	CNB_Skill_Panel	*m_pSkillPanel1;
	CNB_Skill_Panel	*m_pSkillPanel2;
	CNB_Skill_Panel	*m_pSkillPanel3;
	CNB_Skill_Panel	*m_pSkillPanel4;
	CNB_Weapon_Detail	*m_pWeaponDetail0;
	CNB_Weapon_Detail	*m_pWeaponDetail1;
	CNB_Weapon_Detail	*m_pWeaponDetail2;
	CNB_Weapon_Detail	*m_pWeaponDetail3;
	CNB_Weapon_Detail	*m_pWeaponDetail4;
	CNB_Weapon_Detail	*m_pWeaponDetail5;
	vgui::Panel	*m_pTitleBGLine;
	CASW_Model_Panel	*m_pItemModelPanel;
	// == MANAGED_MEMBER_POINTERS_END ==
	vgui::ImagePanel *m_pPromotionIcon;
	vgui::Label *m_pPromotionLabel;

	bool m_bPromotionTooltip;
	bool m_bMarineTooltip;
	int m_nLobbySlot;
	int m_nInventorySlot;
	bool m_bValidTooltip;
	
	int m_nLastWeaponHash;
	int m_nLastInventorySlot;
};

#endif // _INCLUDED_NB_LOBBY_TOOLTIP_H

















