//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: P.E.T. (Particle Editing Tool); main UI smarts class
//
//=============================================================================

#ifndef PETTOOL_H
#define PETTOOL_H

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
#include "datamodel/dmehandle.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "toolutils/savewindowpositions.h"
#include "toolutils/toolwindowfactory.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "particles/particles.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CPetDoc;
class CParticleSystemPropertiesContainer;
class CParticleSystemDefinitionBrowser;
class CParticleSystemPreviewPanel;
class CDmeParticleSystemDefinition;
enum ParticleFunctionType_t;

namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Allows the doc to call back into the CommEdit editor tool
//-----------------------------------------------------------------------------
abstract_class IPetDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;
};


//-----------------------------------------------------------------------------
// Global methods of the commedit tool
//-----------------------------------------------------------------------------
abstract_class IPetTool
{
public:
	// Gets at the rool panel (for modal dialogs)
	virtual vgui::Panel *GetRootPanel() = 0;

	// Gets the registry name (for saving settings)
	virtual const char *GetRegistryName() = 0;
};

//-----------------------------------------------------------------------------
// Implementation of the CommEdit tool
//-----------------------------------------------------------------------------
class CPetTool : public CBaseToolSystem, public IFileMenuCallbacks, public IPetDocCallback, public IPetTool
{
	DECLARE_CLASS_SIMPLE( CPetTool, CBaseToolSystem );

public:
	CPetTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "Particle Editor"; }
	virtual bool	Init( );
	virtual void	Shutdown();
	virtual bool	CanQuit();
	virtual void	OnToolActivate();
	virtual void	OnToolDeactivate();
	virtual void	Think( bool finalTick );

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen );

	// Inherited from IPetDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual vgui::Panel *GetRootPanel() { return this; }

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "PetTool"; }
	virtual const char *GetBindingsContextFile() { return "cfg/Pet.kb"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );

	MESSAGE_FUNC( Save, "OnSave" );
	void SaveAndTest();

public:
	MESSAGE_FUNC( OnRestartLevel, "RestartLevel" );
	MESSAGE_FUNC( OnNew, "OnNew" );
	MESSAGE_FUNC( OnOpen, "OnOpen" );
	MESSAGE_FUNC( OnSaveAs, "OnSaveAs" );
	MESSAGE_FUNC( OnClose, "OnClose" );
	MESSAGE_FUNC( OnCloseNoSave, "OnCloseNoSave" );
	MESSAGE_FUNC( OnMarkNotDirty, "OnMarkNotDirty" );
	MESSAGE_FUNC( OnExit, "OnExit" );
	MESSAGE_FUNC( OnCopy, "OnCopy" );
	MESSAGE_FUNC( OnPaste, "OnPaste" );

	// Commands related to the edit menu
	void		OnDescribeUndo();

	// Methods related to the view menu
	MESSAGE_FUNC( OnToggleProperties, "OnToggleProperties" );
	MESSAGE_FUNC( OnToggleParticleSystemBrowser, "OnToggleParticleSystemBrowser" );
	MESSAGE_FUNC( OnToggleParticlePreview, "OnToggleParticlePreview" );
	MESSAGE_FUNC( OnDefaultLayout, "OnDefaultLayout" );

	// Keybindings
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );
	KEYBINDING_FUNC_NODECLARE( edit_copy, KEY_C, vgui::MODIFIER_CONTROL, OnCopy, "#edit_copy_help", 0 );
	KEYBINDING_FUNC_NODECLARE( edit_paste, KEY_V, vgui::MODIFIER_CONTROL, OnPaste, "#edit_paste_help", 0 );

	void		PerformNew();
	void		OpenFileFromHistory( int slot );
	void		OpenSpecificFile( const char *pFileName );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

	// returns the document
	CPetDoc *GetDocument();

	// Gets at tool windows
	CParticleSystemPropertiesContainer *GetProperties();
	CParticleSystemDefinitionBrowser *GetParticleSystemDefinitionBrowser();
	CParticleSystemPreviewPanel *GetParticlePreview();

	void SetCurrentParticleSystem( CDmeParticleSystemDefinition *pParticleSystem, bool bForceBrowserSelection = true );
	CDmeParticleSystemDefinition* GetCurrentParticleSystem( void );

private:
	// Creates a new document
	void NewDocument( );

	// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	virtual const char *GetLogoTextureName();

	// Creates, destroys tools
	void CreateTools( CPetDoc *doc );
	void DestroyTools();

	// Initializes the tools
	void InitTools();

	// Shows, toggles tool windows
	void ToggleToolWindow( Panel *tool, char const *toolName );
	void ShowToolWindow( Panel *tool, char const *toolName, bool visible );

	// Kills all tool windows
	void DestroyToolContainers();

private:
	// Document
	CPetDoc *m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// Element properties for editing material
	vgui::DHANDLE< CParticleSystemPropertiesContainer > m_hProperties;

	// The entity report
	vgui::DHANDLE< CParticleSystemDefinitionBrowser > m_hParticleSystemDefinitionBrowser;

	// Particle preview window
	vgui::DHANDLE< CParticleSystemPreviewPanel > m_hParticlePreview;

	// The currently viewed entity
	CDmeHandle< CDmeParticleSystemDefinition > m_hCurrentParticleSystem;

	// Separate undo context for the act busy tool
	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;
};

extern CPetTool *g_pPetTool;

#endif // PETTOOL_H
