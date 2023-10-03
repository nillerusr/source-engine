#ifndef _INCLUDED_NB_SELECT_MARINE_ENTRY_H
#define _INCLUDED_NB_SELECT_MARINE_ENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::ImagePanel;
class vgui::Label;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CBitmapButton;
class CNB_Button;

class CNB_Select_Marine_Entry : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Select_Marine_Entry, vgui::EditablePanel );
public:
	CNB_Select_Marine_Entry( vgui::Panel *parent, const char *name );
	virtual ~CNB_Select_Marine_Entry();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void SetProfileIndex( int nProfileIndex ) { m_nProfileIndex = nProfileIndex; }
	int GetProfileIndex() { return m_nProfileIndex; }
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::ImagePanel	*m_pClassImage;
	vgui::Label	*m_pClassLabel;
	vgui::Label	*m_pNameLabel;
	vgui::Label	*m_pPlayerNameLabel;
	// == MANAGED_MEMBER_POINTERS_END ==
	CBitmapButton	*m_pPortraitImage;

	int m_nProfileIndex;
	char m_szLastImageName[ 255 ];

	bool m_bSpendingSkillPointsMode;
	int m_nPreferredLobbySlot;	// the lobby slot we're selecting a marine for, in offline mode
};

#endif // _INCLUDED_NB_SELECT_MARINE_ENTRY_H





