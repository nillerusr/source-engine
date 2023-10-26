#ifndef _INCLUDED_NB_CAMPAIGN_MAP_H
#define _INCLUDED_NB_CAMPAIGN_MAP_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
// == MANAGED_CLASS_DECLARATIONS_END ==

class CNB_Campaign_Map : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Campaign_Map, vgui::EditablePanel );
public:
	CNB_Campaign_Map( vgui::Panel *parent, const char *name );
	virtual ~CNB_Campaign_Map();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	// == MANAGED_MEMBER_POINTERS_END ==
};

#endif // _INCLUDED_NB_CAMPAIGN_MAP_H
