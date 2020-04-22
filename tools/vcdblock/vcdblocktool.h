//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VcdBlock tool; main UI smarts class
//
//=============================================================================

#ifndef VCDBLOCKTOOL_H
#define VCDBLOCKTOOL_H

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
#include "dmevmfentity.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "toolutils/savewindowpositions.h"
#include "toolutils/toolwindowfactory.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CVcdBlockDoc;
class CInfoTargetPropertiesPanel;
class CInfoTargetBrowserPanel;

namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Allows the doc to call back into the VcdBlock editor tool
//-----------------------------------------------------------------------------
abstract_class IVcdBlockDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;
};


//-----------------------------------------------------------------------------
// Global methods of the VCD Blocking tool
//-----------------------------------------------------------------------------
abstract_class IVcdBlockTool
{
public:
	// Gets at the rool panel (for modal dialogs)
	virtual vgui::Panel *GetRootPanel() = 0;

	// Gets the registry name (for saving settings)
	virtual const char *GetRegistryName() = 0;

	// Shows a particular entity in the entity properties dialog
	virtual void ShowEntityInEntityProperties( CDmeVMFEntity *pEntity ) = 0;
};

//-----------------------------------------------------------------------------
// Implementation of the VcdBlock tool
//-----------------------------------------------------------------------------
class CVcdBlockTool : public CBaseToolSystem, public IFileMenuCallbacks, public IVcdBlockDocCallback, public IVcdBlockTool
{
	DECLARE_CLASS_SIMPLE( CVcdBlockTool, CBaseToolSystem );

public:
	CVcdBlockTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "VCD Blocking Tool"; }
	virtual bool	Init( );
	virtual void	Shutdown();
	virtual bool	CanQuit();
	virtual void	OnToolActivate();
	virtual void	OnToolDeactivate();
	virtual void	ServerLevelInitPostEntity();
	virtual void	DrawEntitiesInEngine( bool bDrawInEngine );
	virtual void	ClientLevelInitPostEntity();
	virtual void	ClientLevelShutdownPreEntity();
	virtual bool	TrapKey( ButtonCode_t key, bool down );
 	virtual void	ClientPreRender();

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen );

	// Inherited from IVcdBlockDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual vgui::Panel *GetRootPanel() { return this; }
	virtual void ShowEntityInEntityProperties( CDmeVMFEntity *pEntity );

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "VcdBlockTool"; }
	virtual const char *GetBindingsContextFile() { return "cfg/VcdBlock.kb"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );

	MESSAGE_FUNC( Save, "OnSave" );
	void SaveAndTest();
	void RestartMap();

	// Enter mode where we preview dropping nodes
	void EnterTargetDropMode();
	void LeaveTargetDropMode();

	bool IsMiniViewportCursor( int x, int y, Vector &org, Vector &forward );

	// Save/Load game state
	void SetRememberPlayerPosition( bool state = true ) { m_bRememberPlayerPosition = state; };
	bool GetRememberPlayerPosition( void ) { return m_bRememberPlayerPosition; };
	void QuickLoad();
	void QuickSave();
	
	bool IsInNodeDrag( void ) { return m_bInNodeDragMode; };

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

	// Methods related to the VcdBlock menu
	MESSAGE_FUNC( OnAddNewNodes, "AddNewNodes" );
	MESSAGE_FUNC( OnCopyEditsToVMF, "CopyEditsToVMF" );
	MESSAGE_FUNC( OnRememberPosition, "RememberPosition" );

	// Methods related to the view menu
	MESSAGE_FUNC( OnToggleProperties, "OnToggleProperties" );
	MESSAGE_FUNC( OnToggleEntityReport, "OnToggleEntityReport" );
	MESSAGE_FUNC( OnDefaultLayout, "OnDefaultLayout" );

	// Keybindings
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );
	KEYBINDING_FUNC_NODECLARE( VcdBlockAddNewNodes, KEY_A, vgui::MODIFIER_CONTROL, OnAddNewNodes, "#VcdBlockAddNewNodesHelp", 0 );

	void		OpenFileFromHistory( int slot );
	void		OpenSpecificFile( const char *pFileName );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

	void AttachAllEngineEntities();

	// returns the document
	CVcdBlockDoc *GetDocument();

	// Gets at tool windows
	CInfoTargetPropertiesPanel *GetProperties();
	CInfoTargetBrowserPanel *GetInfoTargetBrowser();

	CDmeHandle< CDmeVMFEntity > GetCurrentEntity( void ) { return m_hCurrentEntity; }

private:
	// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	virtual const char *GetLogoTextureName();

	// Creates, destroys tools
	void CreateTools( CVcdBlockDoc *doc );
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
	
private:
	// Document
	CVcdBlockDoc *m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// Element properties for editing material
	vgui::DHANDLE< CInfoTargetPropertiesPanel >	m_hProperties;

	// The entity report
	vgui::DHANDLE< CInfoTargetBrowserPanel > m_hInfoTargetBrowser;

	// The currently viewed entity
	CDmeHandle< CDmeVMFEntity > m_hCurrentEntity;

	// Separate undo context for the act busy tool
	bool m_bInNodeDropMode;
	bool m_bInNodeDragMode;
	int m_iDragX;
	int m_iDragY;
	CDmeHandle< CDmeVMFEntity > m_hPreviewTarget;
	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;

	// remembered player position
	bool m_bRememberPlayerPosition;
	bool m_bHasPlayerPosition;
	Vector m_vecPlayerOrigin;
	QAngle m_vecPlayerAngles;
};

extern CVcdBlockTool *g_pVcdBlockTool;

#endif // VCDBLOCKTOOL_H
