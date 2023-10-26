#ifndef _INCLUDED_NB_SUGGEST_DIFFICULTY_H
#define _INCLUDED_NB_SUGGEST_DIFFICULTY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

class vgui::Label;
class CNB_Button;
class CASW_Model_Panel;
class CNB_Gradient_Bar;

class CNB_Suggest_Difficulty : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Suggest_Difficulty, vgui::EditablePanel );
public:
	CNB_Suggest_Difficulty( vgui::Panel *parent, const char *name );
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnCommand( const char *command );
	virtual void PaintBackground();

	void SetSuggestIncrease( void );
	void SetSuggestDecrease( void );
	void UpdateDetails( void );
	
	vgui::Label	*m_pTitle;
	vgui::Label *m_pPerformanceLabel;
	CNB_Gradient_Bar *m_pBanner;
	CNB_Button	*m_pNoButton;
	CNB_Button	*m_pYesButton;

	bool m_bIncrease;
};

#endif // _INCLUDED_NB_SUGGEST_DIFFICULTY_H
