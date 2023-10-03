#ifndef _INCLUDED_NB_SELECT_MISSION_PANEL_H
#define _INCLUDED_NB_SELECT_MISSION_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class CNB_Header_Footer;
vgui::Label;
class CNB_Horiz_List;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Button;
struct ASW_Mission_Chooser_Mission;

class CNB_Select_Mission_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Mission_Panel, vgui::EditablePanel );
public:
	CNB_Select_Mission_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Mission_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void SelectMissionsFromCampaign( const char *szCampaignName );	// show only missions from this campaign
	void InitList();
	void MissionSelected( ASW_Mission_Chooser_Mission *pMission );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	CNB_Header_Footer	*m_pHeaderFooter;
	vgui::Label	*m_pTitle;
	CNB_Horiz_List	*m_pHorizList;	
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pBackButton;

	char m_szCampaignFilter[ 256 ];
	int m_nLastCount;
};

#endif // _INCLUDED_NB_SELECT_MISSION_PANEL_H
