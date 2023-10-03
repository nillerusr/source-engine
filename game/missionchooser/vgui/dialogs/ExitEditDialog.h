#ifndef EXITEDITDIALOG_H
#define EXITEDITDIALOG_H
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

class CRoomTemplateExit;
class CRoomTemplate;

// this dialog allows you to edit an exit's properties

class CExitEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CExitEditDialog, vgui::Frame );

public:

	CExitEditDialog( vgui::Panel *parent, const char *name, CRoomTemplate *pRoomTemplate, CRoomTemplateExit *pExit );
	virtual ~CExitEditDialog();

	virtual void OnCommand( const char *command );

	vgui::TextEntry* m_pExitTagEdit;
	vgui::TextEntry* m_pThemeDescriptionEdit;
	vgui::CheckButton* m_pChokeGrowCheck;

	CRoomTemplateExit* m_pExit;
	CRoomTemplate* m_pRoomTemplate;
};

#endif // EXITEDITDIALOG_H