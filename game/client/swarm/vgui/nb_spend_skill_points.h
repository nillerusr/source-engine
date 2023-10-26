#ifndef _INCLUDED_NB_SPEND_SKILL_POINTS_H
#define _INCLUDED_NB_SPEND_SKILL_POINTS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "asw_shareddefs.h"

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class vgui::Panel;
class vgui::Button;
class CNB_Skill_Panel_Spending;
class vgui::ImagePanel;
class CNB_Header_Footer;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Select_Marine_Entry;
class CNB_Button;
class CNB_Gradient_Bar;
class CBitmapButton;
class SkillAnimPanel;

#define NUM_SKILL_PANELS 5

class CNB_Spend_Skill_Points : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Spend_Skill_Points, vgui::EditablePanel );
public:
	CNB_Spend_Skill_Points( vgui::Panel *parent, const char *name );
	virtual ~CNB_Spend_Skill_Points();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void Init();
	void DoSpendPointAnimation( int nProfileIndex, int nSkillSlot );
	
	CNB_Header_Footer	*m_pHeaderFooter;
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Label	*m_pMarineNameLabel;
	vgui::Label	*m_pBioTitle;
	vgui::Label	*m_pBioLabel;
	vgui::Label	*m_pSkillTitle;
	vgui::Label	*m_pSkillDescription;
	vgui::Button	*m_pAcceptButton;
	vgui::ImagePanel *m_pSelectedMarine;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pBackButton;
	CNB_Gradient_Bar *m_pSkillBackground;
	CNB_Gradient_Bar *m_pMarineBackground;
	vgui::Label *m_pSpareSkillPointsTitle;
	vgui::Label *m_pSpareSkillPointsLabel;
	CBitmapButton *m_pUndoButton;
	SkillAnimPanel* m_pSkillAnimPanel;

	CNB_Skill_Panel_Spending	*m_pSkillPanel[ NUM_SKILL_PANELS ];
	int m_nProfileIndex;
};

#endif // _INCLUDED_NB_SPEND_SKILL_POINTS_H
