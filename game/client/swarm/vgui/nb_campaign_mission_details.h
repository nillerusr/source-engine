#ifndef _INCLUDED_NB_CAMPAIGN_MISSION_DETAILS_H
#define _INCLUDED_NB_CAMPAIGN_MISSION_DETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "asw_campaign_info.h"

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Panel;
class vgui::Label;
// == MANAGED_CLASS_DECLARATIONS_END ==

class CNB_Campaign_Mission_Details : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Campaign_Mission_Details, vgui::EditablePanel );
public:
	CNB_Campaign_Mission_Details( vgui::Panel *parent, const char *name );
	virtual ~CNB_Campaign_Mission_Details();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void SetCurrentMission( CASW_Campaign_Info::CASW_Campaign_Mission_t *pMission ) { m_pCurrentMission = pMission; }
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::Panel	*m_pBackground;
	vgui::Panel	*m_pBackgroundInner;
	vgui::Panel	*m_pTitleBG;
	vgui::Panel	*m_pTitleBGBottom;
	vgui::Label	*m_pTitle;
	vgui::Label	*m_pMissionName;
	vgui::Label	*m_pMissionDescription;
	// == MANAGED_MEMBER_POINTERS_END ==

	CASW_Campaign_Info::CASW_Campaign_Mission_t* m_pCurrentMission;
};

#endif // _INCLUDED_NB_CAMPAIGN_MISSION_DETAILS_H
