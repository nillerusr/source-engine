#ifndef _INCLUDED_NB_WEAPON_DETAIL_H
#define _INCLUDED_NB_WEAPON_DETAIL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class StatsBar;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CASW_WeaponInfo;

class CNB_Weapon_Detail : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Weapon_Detail, vgui::EditablePanel );
public:
	CNB_Weapon_Detail( vgui::Panel *parent, const char *name );
	virtual ~CNB_Weapon_Detail();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();

	void SetWeaponDetails( int nEquipIndex, int nInventorySlot, int nProfileIndex, int nDetailIndex );

	void UpdateLabels( CASW_WeaponInfo *pWeaponData );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Label	*m_pTitleLabel;
	vgui::Label	*m_pValueLabel;
	StatsBar	*m_pStatsBar;
	// == MANAGED_MEMBER_POINTERS_END ==

	bool m_bHidden;
	int m_nEquipIndex;
	int m_nInventorySlot;	
	int m_nDetailIndex;
	int m_nProfileIndex;
};

#endif // _INCLUDED_NB_WEAPON_DETAIL_H



