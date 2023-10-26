#ifndef _INCLUDED_WEAPON_UNLOCK_PANEL_H
#define _INCLUDED_WEAPON_UNLOCK_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

namespace vgui {
	class Label;
};
class CASW_Model_Panel;

class WeaponUnlockPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( WeaponUnlockPanel, vgui::EditablePanel );
public:
	WeaponUnlockPanel(vgui::Panel *pParent, const char *szPanelName );
	virtual ~WeaponUnlockPanel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();

	void UpdateWeaponDetails();
	void SetDetails( const char *szWeaponClass, int iPlayerLevel );

	vgui::Label *m_pUnlockLabel;
	vgui::Label *m_pWeaponLabel;
	vgui::Label *m_pLevelRequirementLabel;
	vgui::ImagePanel *m_pLockedIcon;
	vgui::Panel *m_pLockedBG;
	CASW_Model_Panel *m_pItemModelPanel;
	char m_szWeaponClass[ 128 ];
	int m_iPlayerLevel;
};

#endif // _INCLUDED_WEAPON_UNLOCK_PANEL_H