//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CommEdit tool; main UI smarts class
//
//=============================================================================

#ifndef COMMEDITTOOL_H
#define COMMEDITTOOL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "toolutils/basetoolsystem.h"
#include "toolutils/recentfilelist.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/toolswitchmenubutton.h"
#include "toolutils/tooleditmenubutton.h"
#include "toolutils/toolfilemenubutton.h"
#include "toolutils/toolmenubutton.h"
#include "datamodel/dmelement.h"
#include "dmecommentarynodeentity.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "toolutils/savewindowpositions.h"
#include "toolutils/toolwindowfactory.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CConsolePage;
class CCommEditDoc;
class CCommentaryPropertiesPanel;
class CCommentaryNodeBrowserPanel;

namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Allows the doc to call back into the CommEdit editor tool
//-----------------------------------------------------------------------------
abstract_class ICommEditDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;
};


//-----------------------------------------------------------------------------
// Global methods of the commedit tool
//-----------------------------------------------------------------------------
abstract_class ICommEditTool
{
public:
	// Gets at the rool panel (for modal dialogs)
	virtual vgui::Panel *GetRootPanel() = 0;

	// Gets the registry name (for saving settings)
	virtual const char *GetRegistryName() = 0;

	// Shows a particular entity in the entity properties dialog
	virtual void ShowEntityInEntityProperties( CDmeCommentaryNodeEntity *pEntity ) = 0;
};

//-----------------------------------------------------------------------------
// Implementation of the CommEdit tool
//-----------------------------------------------------------------------------
class CCommEditTool : public CBaseToolSystem, public IFileMenuCallbacks, public ICommEditDocCallback, public ICommEditTool
{
	DECLARE_CLASS_SIMPLE( CCommEditTool, CBaseToolSystem );

public:
	CCommEditTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "Commentary Editor"; }
	virtual bool	Init( );
	virtual void	Shutdown();
	virtual bool	CanQuit();
	virtual void	OnToolActivate();
	virtual void	OnToolDeactivate();
	virtual const char* GetEntityData( const char *pActualEntityData );
	virtual void	DrawCommentaryNodeEntitiesInEngine( bool bDrawInEngine );
	virtual void	ClientLevelInitPostEntity();
	virtual void	ClientLevelShutdownPreEntity();
	virtual bool	TrapKey( ButtonCode_t key, bool down );
 	virtual void	ClientPreRender();

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen );

	// Inherited from ICommEditDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual vgui::Panel *GetRootPanel() { return this; }
	virtual void ShowEntityInEntityProperties( CDmeCommentaryNodeEntity *pEntity );

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "CommEditTool"; }
	virtual const char *GetBindingsContextFile() { return "cfg/CommEdit.kb"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );

	MESSAGE_FUNC( Save, "OnSave" );
	void SaveAndTest();
	void CenterView( CDmeCommentaryNodeEntity* pEntity );

	// Enter mode where we preview dropping nodes
	void EnterNodeDropMode();
	void LeaveNodeDropMode();

public:
	MESSAGE_FUNC( OnRestartLevel, "RestartLevel" );
	MESSAGE_FUNC( OnNew, "OnNew" );
	MESSAGE_FUNC( OnOpen, "OnOpen" );
	MESSAGE_FUNC( OnSaveAs, "OnSaveAs" );
	MESSAGE_FUNC( OnClose, "OnClose" );
	MESSAGE_FUNC( OnCloseNoSave, "OnCloseNoSave" );
	MESSAGE_FUNC( OnMarkNotDirty, "OnMarkNotDirty" );
	MESSAGE_FUNC( OnExit, "OnExit" );

	// Commands related to the edit menu
	void		OnDescribeUndo();

	// Methods related to the CommEdit menu
	MESSAGE_FUNC( OnAddNewNodes, "AddNewNodes" );

	// Methods related to the view menu
	MESSAGE_FUNC( OnToggleProperties, "OnToggleProperties" );
	MESSAGE_FUNC( OnToggleEntityReport, "OnToggleEntityReport" );
	MESSAGE_FUNC( OnToggleConsole, "ToggleConsole" );

	MESSAGE_FUNC( OnDefaultLayout, "OnDefaultLayout" );

	// Keybindings
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );
	KEYBINDING_FUNC_NODECLARE( CommEditAddNewNodes, KEY_A, vgui::MODIFIER_CONTROL, OnAddNewNodes, "#CommEditAddNewNodesHelp", 0 );

	void		PerformNew();
	void		OpenFileFromHistory( int slot );
	void		OpenSpecificFile( const char *pFileName );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

	void AttachAllEngineEntities();

	// returns the document
	CCommEditDoc *GetDocument();

	// Gets at tool windows
	CCommentaryPropertiesPanel *GetProperties();
	CCommentaryNodeBrowserPanel *GetCommentaryNodeBrowser();
	CConsolePage *GetConsole();

	CDmeHandle< CDmeCommentaryNodeEntity > GetCurrentEntity( void ) { return m_hCurrentEntity; }

private:
	// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	virtual const char *GetLogoTextureName();

	// Creates, destroys tools
	void CreateTools( CCommEditDoc *doc );
	void DestroyTools();

	// Initializes the tools
	void InitTools();

	// Shows, toggles tool windows
	void ToggleToolWindow( Panel *tool, char const *toolName );
	void ShowToolWindow( Panel *tool, char const *toolName, bool visible );

	// Kills all tool windows
	void DestroyToolContainers();

	// Gets the position of the preview object
	void GetPlacementInfo( Vector &vecOrigin, QAngle &angles );
	
	// Brings the console to front
	void BringConsoleToFront();

private:
	// Document
	CCommEditDoc *m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// Element properties for editing material
	vgui::DHANDLE< CCommentaryPropertiesPanel >	m_hProperties;

	// The entity report
	vgui::DHANDLE< CCommentaryNodeBrowserPanel > m_hCommentaryNodeBrowser;

	// The console
	vgui::DHANDLE< CConsolePage >				m_hConsole;

	// The currently viewed entity
	CDmeHandle< CDmeCommentaryNodeEntity > m_hCurrentEntity;

	// Separate undo context for the act busy tool
	bool m_bInNodeDropMode;
	bool m_bDroppingCommentaryNodes;
	CDmeHandle< CDmeCommentaryNodeEntity > m_hPreviewNode;
	CDmeHandle< CDmeCommentaryNodeEntity > m_hPreviewTarget;
	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;
};

extern CCommEditTool *g_pCommEditTool;

#endif // COMMEDITTOOL_H
