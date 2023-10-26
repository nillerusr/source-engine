#ifndef THEMESDIALOG_H
#define THEMESDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui {
	class PanelListPanel;
	class Label;
};
class CLevelTheme;
class CMissionChooserTGAImagePanel;
class CThemeDetails;

// this dialog allows you to select the active theme and shows controls for editing/deleting and creating new themes

class CThemesDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CThemesDialog, vgui::Frame );

public:

	CThemesDialog( vgui::Panel *parent, const char *name, bool bGlobalDialog );
	virtual ~CThemesDialog();

	virtual void PopulateThemeList();
	virtual bool ShouldHighlight( CThemeDetails *pDetails );

	virtual void ThemeClicked( CThemeDetails* pDetails );
	virtual void OnCommand( const char *command );
	MESSAGE_FUNC_PARAMS( OnThemeChanged, "ThemeChanged", params );	
	MESSAGE_FUNC_PTR( OnThemeDetailsClicked, "ThemeDetailsClicked", panel );

	vgui::PanelListPanel* m_pThemePanelList;
	vgui::Label* m_pCurrentThemeLabel;
};

// panel in the list showing details about a particular theme
class CThemeDetails : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CThemeDetails, vgui::Panel );

public:
	CThemeDetails( vgui::Panel *parent, const char *name, CThemesDialog *pThemesDialog );
	virtual ~CThemeDetails();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnThink();

	void SetTheme(CLevelTheme* pTheme);

	CMissionChooserTGAImagePanel *m_pTGAImagePanel;
	vgui::Label *m_pNameLabel;
	vgui::Label *m_pDescriptionLabel;
	CLevelTheme *m_pTheme;
	CThemesDialog *m_pThemesDialog;

	int m_iDesiredHeight;
	int m_iDesiredWidth;
	bool m_bCurrentTheme;
};

#endif