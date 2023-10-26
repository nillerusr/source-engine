#ifndef _INCLUDED_NB_COMMANDER_LIST_H
#define _INCLUDED_NB_COMMANDER_LIST_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Panel;
class vgui::Label;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CNB_Commander_List_Entry;

#define COMMANDER_LIST_MAX_COMMANDERS 8

class CNB_Commander_List : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Commander_List, vgui::EditablePanel );
public:
	CNB_Commander_List( vgui::Panel *parent, const char *name );
	virtual ~CNB_Commander_List();
	
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
	// == MANAGED_MEMBER_POINTERS_END ==
	CNB_Commander_List_Entry *m_Entries[ COMMANDER_LIST_MAX_COMMANDERS ];
};

#endif // _INCLUDED_NB_COMMANDER_LIST_H
