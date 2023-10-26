#ifndef _INCLUDED_KV_EDITOR_FRAME_H
#define _INCLUDED_KV_EDITOR_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/PHandle.h>
#include <FileSystem.h>
#include "vgui/mousecode.h"
#include "vgui/IScheme.h"
#include "ConfigManager.h"

class CScrollingWindow;
class CKV_Editor_Base_Panel;
class CKV_Fit_Children_Panel;
class CKV_Editor;
namespace vgui
{
	class PanelListPanel;
	class ImagePanel;
	class MenuBar;
	class CheckButton;
	class Button;
	class Menu;
};

using namespace vgui;


enum KV_FileSelectType
{
	KV_FST_LAYOUT_OPEN,
	KV_FST_LAYOUT_SAVE_AS
};

//-----------------------------------------------------------------------------
// Purpose: Main dialog for visualizing and editing keyvalues
//-----------------------------------------------------------------------------
class CKV_Editor_Frame : public Frame
{
	DECLARE_CLASS_SIMPLE( CKV_Editor_Frame, Frame );

public:
	CKV_Editor_Frame( Panel *parent, const char *name );
	virtual ~CKV_Editor_Frame();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	// main menu bar
	MenuBar *m_pMenuBar;	
	vgui::Menu *m_pFileMenu;
	vgui::Menu *m_pToolsMenu;

    // panel for editing keyvalues
	CKV_Editor* m_pEditor;

	DECLARE_PANELMAP();	

	void DoFileSaveAs();
	void DoFileOpen();
	void OnFileSelected(const char *fullpath);
	KV_FileSelectType m_FileSelectType;

protected:
	virtual void OnClose();
	virtual void OnCommand( const char *command );

	char m_szLastFileName[MAX_PATH];
	bool m_bFirstPerformLayout;
};


#endif // _INCLUDED_KV_EDITOR_FRAME_H