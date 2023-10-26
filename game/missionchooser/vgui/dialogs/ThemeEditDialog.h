#ifndef THEMEEDITDIALOG_H
#define THEMEEDITDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui {
	class PanelListPanel;
	class Label;
	class ImagePanel;
	class TextEntry;
	class RichText;
	class CheckButton;
};

class CLevelTheme;

// this dialog allows you to edit a theme's properties

class CThemeEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CThemeEditDialog, vgui::Frame );

public:

	CThemeEditDialog( vgui::Panel *parent, const char *name, CLevelTheme *pTheme, bool bCreatingNew );
	virtual ~CThemeEditDialog();

	virtual void OnCommand( const char *command );

	vgui::TextEntry* m_pThemeNameEdit;
	vgui::TextEntry* m_pThemeDescriptionEdit;
	vgui::TextEntry* m_pThemeAmbientEdit;
	vgui::CheckButton* m_pVMFTweakCheck;

	CLevelTheme* m_pLevelTheme;
	bool m_bCreatingNew;
};

#endif // THEMEEDITDIALOG_H