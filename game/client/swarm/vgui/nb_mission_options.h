#ifndef _INCLUDED_NB_MISSION_OPTIONS_H
#define _INCLUDED_NB_MISSION_OPTIONS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class vgui::Panel;
class vgui::Button;
class CNB_Header_Footer;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Button;

#define ASW_NUM_GAME_STYLES 4

class CNB_Mission_Options : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Mission_Options, vgui::EditablePanel );
public:
	CNB_Mission_Options( vgui::Panel *parent, const char *name );
	virtual ~CNB_Mission_Options();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( char const* command );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	CNB_Header_Footer *m_pHeaderFooter;
	vgui::Button* m_pDifficultyUpButton;
	vgui::Button* m_pDifficultyDownButton;
	vgui::Label* m_pDifficultyTitle;
	vgui::Label* m_pSkillLevelLabel;
	vgui::Label* m_pSkillDescriptionLabel;
	vgui::Label* m_pStyleTitle;
	vgui::Label* m_pStyleLabel;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pBackButton;
	vgui::Button* m_pGameStyle[ASW_NUM_GAME_STYLES];

	int m_iLastSkillLevel;
};

#endif // _INCLUDED_NB_MISSION_OPTIONS_H


