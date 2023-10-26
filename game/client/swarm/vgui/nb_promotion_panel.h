#ifndef _INCLUDED_NB_PROMOTION_PANEL_H
#define _INCLUDED_NB_PROMOTION_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

class vgui::Label;
class CNB_Button;
class vgui::ImagePanel;

class CNB_Promotion_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Promotion_Panel, vgui::EditablePanel );
public:
	CNB_Promotion_Panel( vgui::Panel *parent, const char *name );
	virtual ~CNB_Promotion_Panel();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	virtual void PaintBackground();
	
	vgui::Label	*m_pTitle;
	vgui::Panel *m_pBanner;
	vgui::Label *m_pDescriptionLabel;
	vgui::Label *m_pMedalLabel;
	vgui::Label *m_pWarningLabel;
	vgui::ImagePanel *m_pMedalIcon;
	CNB_Button	*m_pBackButton;
	CNB_Button	*m_pAcceptButton;
};

#endif // _INCLUDED_NB_PROMOTION_PANEL_H
