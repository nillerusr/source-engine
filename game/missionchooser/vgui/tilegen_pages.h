#ifndef _INCLUDED_TILEGEN_PAGES_H
#define _INCLUDED_TILEGEN_PAGES_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>

class CScrollingWindow;
class CMapLayoutPanel;
class CKV_Editor;
class CTilegenMissionPreprocessor;
class CLayoutSystemKVEditor;
class CASW_KeyValuesDatabase;

namespace vgui
{
	class PanelListPanel;	
	class TreeView;
	class Label;
	class Button;
};

// ==============================================================================================

class CTileGenLayoutPage : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTileGenLayoutPage, vgui::EditablePanel );
public:
	CTileGenLayoutPage( Panel *parent, const char *name );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	CScrollingWindow* m_pScrollingWindow;
	CMapLayoutPanel* m_pMapLayoutPanel;
};

// ==============================================================================================

class CTilegenKVEditorPage : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTilegenKVEditorPage, vgui::EditablePanel );

public:
	CTilegenKVEditorPage( Panel *pParent, const char *pName );
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void OnCommand( const char *command );
	virtual void UpdateList() = 0;

	MESSAGE_FUNC_INT( OnTreeViewItemSelected, "TreeViewItemSelected", itemIndex ) = 0;
	virtual void OnFileSelected( const char *fullpath ) = 0;
	MESSAGE_FUNC_PARAMS( OnCheckOutFromP4, "CheckOutFromP4", pKV );
	DECLARE_PANELMAP();

protected:
	int RecursiveCreateFolderNodes( int iParentIndex, char *szFilename );		
	void UpdateListHelper( CASW_KeyValuesDatabase *pMissionDatabase, const char *pFolderName );
	virtual void SaveNew() = 0;
	virtual KeyValues *GetKeyValues() = 0;

	vgui::TreeView *m_pTree;
	vgui::Label* m_pFilenameLabel;
	vgui::Button* m_pNewButton;
	vgui::Button* m_pSaveButton;
	char m_szFilename[MAX_PATH];
};

// ==============================================================================================

class CTilegenLayoutSystemPage : public CTilegenKVEditorPage
{
	DECLARE_CLASS_SIMPLE( CTilegenLayoutSystemPage, CTilegenKVEditorPage );

public:
	CTilegenLayoutSystemPage( Panel *pParent, const char *pName );
	virtual ~CTilegenLayoutSystemPage();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	virtual void OnCommand( const char *pCommand );

	virtual void OnTreeViewItemSelected( int nItemIndex );
	virtual void OnFileSelected( const char *pFullPath );

	MESSAGE_FUNC_PARAMS( OnAddToP4, "AddToP4", pKV );

	virtual void UpdateList();

	CLayoutSystemKVEditor *GetEditor() { return m_pEditor; }
	CTilegenMissionPreprocessor *GetPreprocessor() { return m_pPreprocessor; }

protected:
	virtual void SaveNew();
	virtual KeyValues *GetKeyValues();
	
	void AddRuleFile( const char *pRuleFilename );
	
	CTilegenMissionPreprocessor *m_pPreprocessor;
	CLayoutSystemKVEditor *m_pEditor;	
	vgui::Button* m_pGenerateButton;
	vgui::Button* m_pReloadMissionButton;
	vgui::Button* m_pReloadRulesButton;
	CASW_KeyValuesDatabase *m_pNewMissionDatabase;
	CASW_KeyValuesDatabase *m_pRulesDatabase;
};

// ==============================================================================================

#endif // _INCLUDED_TILEGEN_PAGES_H