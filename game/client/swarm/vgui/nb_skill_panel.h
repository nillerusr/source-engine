#ifndef _INCLUDED_NB_SKILL_PANEL_H
#define _INCLUDED_NB_SKILL_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

#include "asw_shareddefs.h"

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class vgui::ImagePanel;
class StatsBar;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CBitmapButton;
class BriefingTooltip;

class CNB_Skill_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Skill_Panel, vgui::EditablePanel );
public:
	CNB_Skill_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Skill_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	
	void SetSkillDetails( int nProfileIndex, int nSkillSlot, int nSkillPoints );

	const char* GetSkillName();

	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Label	*m_pSkillLabel;
	vgui::Label	*m_pSkillNumberLabel;	
	StatsBar	*m_pSkillBar;
	// == MANAGED_MEMBER_POINTERS_END ==
	CBitmapButton	*m_pSkillButton;

	bool CanSpendPoint();

	int m_nSkillSlot;
	int m_nSkillPoints;
	bool m_bSpendPointsMode;

	char m_szLastSkillImage[ 128 ];
	int m_nProfileIndex;
};

// larger version when spending skill points
class CNB_Skill_Panel_Spending : public CNB_Skill_Panel
{
	DECLARE_CLASS_SIMPLE( CNB_Skill_Panel_Spending, CNB_Skill_Panel );
public:
	CNB_Skill_Panel_Spending( vgui::Panel *parent, const char *name );
	virtual ~CNB_Skill_Panel_Spending();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
};

#endif // _INCLUDED_NB_SKILL_PANEL_H




