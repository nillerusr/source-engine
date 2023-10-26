#ifndef _INCLUDED_KV_EDITOR_H
#define _INCLUDED_KV_EDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
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

//-----------------------------------------------------------------------------
// Purpose: Main panel for visualizing and editing keyvalues
//-----------------------------------------------------------------------------
class CKV_Editor : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CKV_Editor, vgui::EditablePanel );

public:
	CKV_Editor( Panel *parent, const char *name );
	virtual ~CKV_Editor();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	virtual void SetFileSpec( const char *szFilename, const char *szPathID );
	virtual void SetFileSpec( KeyValues *pKeys );
	KeyValues* GetFileSpec() { return m_pFileSpec; }
	
	virtual void SetKeys( KeyValues *pKeys );
	KeyValues* GetKeys() { return m_pKeys; }
	void SetShowSiblingKeys( bool bShowSiblings ) { m_bShowSiblings = bShowSiblings; }
	
	void SetFileFilter( const char *szFilter, const char *szFilterName );
	void SetFileDirectory( const char *szDirName );
	const char* GetFileFilter() { return m_szFileFilter; }
	const char* GetFileFilterName() { return m_szFileFilterName; }
	const char* GetFileDirectory() { return m_szFileDirectory; }

	virtual void AddToKey( KeyValues *pFileSpecNode, KeyValues *pKey, const char *szNewKeyName );
	virtual void OnKeyDeleted();
	virtual void OnKeyAdded();

	void SetRequiresFileSpec( bool bRequiresFileSpec ) { m_bRequireFileSpec = bRequiresFileSpec; }

protected:
	
	// the keyvalues we're currently editing
	KeyValues *m_pKeys;
	
	// kv view
	CScrollingWindow* m_pScrollingWindow;

	char m_szFileDirectory[MAX_PATH];
	char m_szFileFilter[32];
	char m_szFileFilterName[32];

	void DeleteAllPanels();
	void UpdatePanels( KeyValues *pKV, CKV_Editor_Base_Panel *pParentPanel, bool bIncludeSiblings );
	CKV_Editor_Base_Panel* FindPanel( KeyValues *pKey );
	CKV_Editor_Base_Panel* CreatePanel( const char *szClassName, CKV_Editor_Base_Panel *pParent, KeyValues *pFileSpecNode, KeyValues *pKey );

	//const char* FindPanelClassForKey( KeyValues *pKey );							// finds the panel class a particular key is meant to have
	KeyValues* FindParentForKey( KeyValues *pSearchChild );							// searches m_pKeys for the parent
	KeyValues* FindParentForKey( KeyValues *pRoot, KeyValues *pSearchChild );		// searches pRoot for the parent
	KeyValues* FindFileSpecNodeForKey( KeyValues *pKey );

	CKV_Fit_Children_Panel *m_pContainer;
	CUtlVector< CKV_Editor_Base_Panel* > m_Panels;

	// keyvalues describing the file format of the KeyValues we're editing
	KeyValues* FindFileSpecNodeForKey( KeyValues *pKey, const char *szPathID );
	KeyValues *m_pFileSpec;
	bool m_bRequireFileSpec;
	bool m_bShowSiblings;
	bool m_bAllowDeletion;
};


#endif // _INCLUDED_KV_EDITOR_H