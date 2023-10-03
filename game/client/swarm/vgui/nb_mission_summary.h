#ifndef _INCLUDED_NB_MISSION_SUMMARY_H
#define _INCLUDED_NB_MISSION_SUMMARY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Panel;
class vgui::Label;
class CBitmapButton;
// == MANAGED_CLASS_DECLARATIONS_END ==

class CNB_Mission_Summary : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Mission_Summary, vgui::EditablePanel );
public:
	CNB_Mission_Summary( vgui::Panel *parent, const char *name );
	virtual ~CNB_Mission_Summary();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Panel	*m_pBackground;
	vgui::Panel	*m_pBackgroundInner;
	vgui::Panel	*m_pTitleBG;
	vgui::Panel	*m_pTitleBGBottom;
	vgui::Label	*m_pTitle;
	vgui::Label	*m_pDifficultyTitle;
	vgui::Label	*m_pDifficultyLabel;
	vgui::Label	*m_pMissionTitle;
	vgui::Label	*m_pMissionLabel;
	vgui::Label	*m_pObjectivesTitle;
	vgui::Label	*m_pObjectivesLabel;
	// == MANAGED_MEMBER_POINTERS_END ==
	CBitmapButton	*m_pDetailsButton;
};

#endif // _INCLUDED_NB_MISSION_SUMMARY_H












