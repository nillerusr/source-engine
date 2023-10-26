#ifndef _INCLUDED_NB_SELECT_MARINE_PANEL_H
#define _INCLUDED_NB_SELECT_MARINE_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class vgui::Panel;
class vgui::Button;
class CNB_Skill_Panel;
class vgui::ImagePanel;
class CNB_Header_Footer;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Button;

class CNB_Select_Marine_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Marine_Panel, vgui::EditablePanel );
public:
	CNB_Select_Marine_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Marine_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void InitMarineList();

	void SetHighlight( int nEntryIndex );
	vgui::Panel* GetHighlightedEntry();
	CUtlVector<vgui::PHandle> m_Entries;
	int m_nHighlightedEntry;
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	CNB_Header_Footer *m_pHeaderFooter;
	vgui::Label	*m_pTitle;
	vgui::Label	*m_pMarineNameLabel;
	vgui::Label	*m_pBioTitle;
	vgui::Label	*m_pBioLabel;	
	CNB_Skill_Panel	*m_pSkillPanel0;
	CNB_Skill_Panel	*m_pSkillPanel1;
	CNB_Skill_Panel	*m_pSkillPanel2;
	CNB_Skill_Panel	*m_pSkillPanel3;
	CNB_Skill_Panel	*m_pSkillPanel4;
	vgui::ImagePanel	*m_pPortraitsBackground;
	vgui::ImagePanel	*m_pPortraitsLowerBackground;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pAcceptButton;
	CNB_Button	*m_pBackButton;
	vgui::Label *m_pSkillPointsLabel;

	int m_nInitialProfileIndex;
	int m_nPreferredLobbySlot;
};

#endif // _INCLUDED_NB_SELECT_MARINE_PANEL_H
















