#ifndef _INCLUDED_NB_SELECT_WEAPON_ENTRY_H
#define _INCLUDED_NB_SELECT_WEAPON_ENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::ImagePanel;
class vgui::Label;
class vgui::Panel;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CBitmapButton;

class CNB_Select_Weapon_Entry : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Weapon_Entry, vgui::EditablePanel );
public:
	CNB_Select_Weapon_Entry( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Weapon_Entry();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	virtual bool IsCursorOver();
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::ImagePanel	*m_pClassImage;
	vgui::Label	*m_pClassLabel;
	vgui::Label	*m_pNewLabel;
	vgui::ImagePanel	*m_pLockedImage;
	vgui::Panel	*m_pLockedBG;
	// == MANAGED_MEMBER_POINTERS_END ==

	CBitmapButton		*m_pWeaponImage;

	int m_nInventorySlot;		// either 0 for primary/secondary or 2 for extra
	int m_nEquipIndex;
	int m_nProfileIndex;
	bool m_bCanEquip;

	char m_szLastImage[ 256 ];
};

#endif // _INCLUDED_NB_SELECT_WEAPON_ENTRY_H






