#ifndef _INCLUDED_NB_SELECT_WEAPON_PANEL_H
#define _INCLUDED_NB_SELECT_WEAPON_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class CNB_Horiz_List;
class vgui::Panel;
class vgui::Button;
class CNB_Weapon_Detail;
class CASW_Model_Panel;
class CNB_Header_Footer;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Button;

class CNB_Select_Weapon_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Weapon_Panel, vgui::EditablePanel );
public:
	CNB_Select_Weapon_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Weapon_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void InitWeaponList();
	void SelectWeapon( int nProfileIndex, int nInventorySlot ) { m_nProfileIndex = nProfileIndex; m_nInventorySlot = nInventorySlot; }
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	CNB_Header_Footer *m_pHeaderFooter;
	vgui::Label	*m_pTitle;
	CNB_Horiz_List	*m_pHorizList;
	vgui::Label	*m_pWeaponLabel;
	vgui::Label	*m_pDescriptionTitle;
	vgui::Label	*m_pDescriptionLabel;
	CNB_Weapon_Detail	*m_pWeaponDetail0;
	CNB_Weapon_Detail	*m_pWeaponDetail1;
	CNB_Weapon_Detail	*m_pWeaponDetail2;
	CNB_Weapon_Detail	*m_pWeaponDetail3;
	CNB_Weapon_Detail	*m_pWeaponDetail4;
	CNB_Weapon_Detail	*m_pWeaponDetail5;
	CNB_Weapon_Detail	*m_pWeaponDetail6;
	CASW_Model_Panel	*m_pItemModelPanel;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pAcceptButton;
	CNB_Button	*m_pBackButton;

	int m_nProfileIndex;
	int m_nInventorySlot;
	int m_nLastWeaponHash;
};

#endif // _INCLUDED_NB_SELECT_WEAPON_PANEL_H
















