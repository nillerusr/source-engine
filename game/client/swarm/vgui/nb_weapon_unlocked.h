#ifndef _INCLUDED_NB_WEAPON_UNLOCKED_H
#define _INCLUDED_NB_WEAPON_UNLOCKED_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Button;
class CASW_Model_Panel;
class CNB_Gradient_Bar;

class CNB_Weapon_Unlocked : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Weapon_Unlocked, vgui::EditablePanel );
public:
	CNB_Weapon_Unlocked( vgui::Panel *parent, const char *name );
	virtual ~CNB_Weapon_Unlocked();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	virtual void PaintBackground();

	void SetWeaponClass( const char *pszWeaponUnlockClass );
	void UpdateWeaponDetails();
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Label	*m_pTitle;
	vgui::Label *m_pWeaponLabel;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Gradient_Bar *m_pBanner;
	CNB_Button	*m_pBackButton;
	CASW_Model_Panel *m_pItemModelPanel;

	const char *m_pszWeaponUnlockClass;
};

#endif // _INCLUDED_NB_WEAPON_UNLOCKED_H
