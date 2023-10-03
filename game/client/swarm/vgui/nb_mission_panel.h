#ifndef _INCLUDED_NB_MISSION_PANEL_H
#define _INCLUDED_NB_MISSION_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Frame.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class vgui::ImagePanel;
class vgui::Button;
class CNB_Header_Footer;
// == MANAGED_CLASS_DECLARATIONS_END ==
class ObjectiveListBox;
class ObjectiveDetailsPanel;
class ObjectiveMap;
class ObjectiveIcons;
class CNB_Button;
class CNB_Island;
namespace BaseModUI
{
	class DropDownMenu;
}

class CNB_Mission_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Mission_Panel, vgui::EditablePanel );
public:
	CNB_Mission_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Mission_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Label	*m_pRetriesLabel;	
	// == MANAGED_MEMBER_POINTERS_END ==

	CNB_Header_Footer *m_pHeaderFooter;
	CNB_Button	*m_pBackButton;
	BaseModUI::DropDownMenu* m_drpDifficulty;
	BaseModUI::DropDownMenu* m_drpFixedSkillPoints;
	BaseModUI::DropDownMenu* m_drpFriendlyFire;
	BaseModUI::DropDownMenu* m_drpOnslaught;

	ObjectiveListBox* m_pObjectiveList;
	ObjectiveDetailsPanel* m_pObjectiveDetails;
	ObjectiveMap* m_pObjectiveMap;

	CNB_Island* m_pObjectiveListBoxIsland;
	CNB_Island* m_pObjectiveMapIsland;
	CNB_Island* m_pObjectiveDetailsIsland;
	CNB_Island* m_pDifficultyIsland;
	vgui::Label *m_pDifficultyDescription;
	bool m_bSelectedFirstObjective;

	int m_iLastSkillLevel;
	int m_iLastFixedSkillPoints;
	int m_iLastHardcoreFF;
	int m_iLastOnslaught;
};

class InGameMissionPanelFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( InGameMissionPanelFrame, vgui::Frame );

	InGameMissionPanelFrame(Panel *parent, const char *panelName, bool showTaskbarIcon = true);
	virtual ~InGameMissionPanelFrame();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();

private:
	int m_iAnimated;
};

#endif // _INCLUDED_NB_MISSION_PANEL_H





