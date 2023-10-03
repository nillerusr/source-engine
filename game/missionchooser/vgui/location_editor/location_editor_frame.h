#ifndef _INCLUDED_LOCATION_EDITOR_FRAME_H
#define _INCLUDED_LOCATION_EDITOR_FRAME_H
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
#include "vstdlib/random.h"

class CScrollingWindow;
class CKV_Editor_Base_Panel;
class CKV_Fit_Children_Panel;
class CKV_Editor;
class CLocation_Layout_Panel;
class IASW_Location_Group;
namespace vgui
{
	class PanelListPanel;
	class ImagePanel;
	class MenuBar;
	class CheckButton;
	class Button;
	class Menu;
	class TreeView;
};

//-----------------------------------------------------------------------------
// Purpose: Shows kv editor for manipulating a specific group
//-----------------------------------------------------------------------------
class CGroup_Edit_Page : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGroup_Edit_Page, vgui::Panel );

public:
	CGroup_Edit_Page( Panel *parent, const char *name );
	virtual ~CGroup_Edit_Page();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand( const char *command );

	// panel for editing keyvalues
	CKV_Editor* m_pEditor;	
	vgui::Button *m_pNewGroupButton;
	vgui::Button *m_pDeleteGroupButton;
	int m_iCurrentGroupIndex;
};


//-----------------------------------------------------------------------------
// Purpose: Main dialog for visualizing and editing location grid keyvalues
//-----------------------------------------------------------------------------
class CLocation_Editor_Frame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CLocation_Editor_Frame, vgui::Frame );

public:
	CLocation_Editor_Frame( Panel *parent, const char *name );
	virtual ~CLocation_Editor_Frame();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	int GetCurrentLocationID() { return m_iCurrentLocationID; }

	void SetGroup( IASW_Location_Group *pGroup );

	MESSAGE_FUNC_PTR( KeyValuesChanged, "KeyValuesChanged", panel );
	MESSAGE_FUNC_PTR( NewGroup, "NewGroup", panel );
	MESSAGE_FUNC_PTR( DeleteGroup, "DeleteGroup", panel );

protected:
	virtual void OnClose();
	virtual void OnCommand( const char *command );

	void SetLocationID( int iLocationID );

	bool m_bFirstPerformLayout;

	CGroup_Edit_Page *m_pGroupPage;
	vgui::Button *m_pSaveButton;
	CScrollingWindow* m_pScrollingWindow;
	CLocation_Layout_Panel* m_pLocationLayoutPanel;

	void UpdateTree();
	MESSAGE_FUNC_INT( OnTreeViewItemSelected, "TreeViewItemSelected", itemIndex );
	vgui::TreeView *m_pTree;
	int m_iCurrentLocationID;
	int m_iRootIndex;

	CUniformRandomStream m_Random;
};


#endif // _INCLUDED_LOCATION_EDITOR_FRAME_H