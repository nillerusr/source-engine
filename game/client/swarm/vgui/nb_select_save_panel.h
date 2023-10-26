#ifndef _INCLUDED_NB_SELECT_SAVE_PANEL_H
#define _INCLUDED_NB_SELECT_SAVE_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
// == MANAGED_CLASS_DECLARATIONS_END ==

class CNB_Select_Save_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Save_Panel, vgui::EditablePanel );
public:
	CNB_Select_Save_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Save_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	// == MANAGED_MEMBER_POINTERS_END ==
};

#endif // _INCLUDED_NB_SELECT_SAVE_PANEL_H
