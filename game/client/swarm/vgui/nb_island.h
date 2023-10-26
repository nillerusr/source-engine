#ifndef _INCLUDED_NB_ISLAND_H
#define _INCLUDED_NB_ISLAND_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

class vgui::Label;

// a box with rounded corners and a title

class CNB_Island : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Island, vgui::EditablePanel );
public:
	CNB_Island( vgui::Panel *parent, const char *name );
	virtual ~CNB_Island();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();

	vgui::Panel	*m_pBackground;
	vgui::Panel	*m_pBackgroundInner;
	vgui::Panel	*m_pTitleBG;
	vgui::Panel	*m_pTitleBGBottom;
	vgui::Label	*m_pTitle;
	vgui::Panel	*m_pTitleBGLine;
};

#endif // _INCLUDED_NB_ISLAND_H
