#ifndef _INCLUDED_NB_SELECT_CAMPAIGN_PANEL_H
#define _INCLUDED_NB_SELECT_CAMPAIGN_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class CNB_Header_Footer;
class CNB_Horiz_List;
class vgui::Button;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Button;
struct ASW_Mission_Chooser_Mission;

class CNB_Select_Campaign_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Campaign_Panel, vgui::EditablePanel );
public:
	CNB_Select_Campaign_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Campaign_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void InitList();
	void CampaignSelected( ASW_Mission_Chooser_Mission *pMission );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	CNB_Header_Footer	*m_pHeaderFooter;
	CNB_Horiz_List	*m_pHorizList;
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Button	*m_pBackButton;
};

#endif // _INCLUDED_NB_SELECT_CAMPAIGN_PANEL_H
