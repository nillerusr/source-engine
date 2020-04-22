//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#ifndef BASETOOLSYSTEM_H
#define BASETOOLSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "toolframework/itoolsystem.h"
#include "vgui/IScheme.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/PHandle.h"
#include "toolutils/recentfilelist.h"
#include "vgui/keycode.h"
#include "vgui_controls/fileopenstatemachine.h"


// #defines
#define TOGGLE_WINDOWED_KEY_CODE    KEY_F11
#define TOGGLE_WINDOWED_KEY_NAME    "F11"

#define TOGGLE_INPUT_KEY_CODE       KEY_F10
#define TOGGLE_INPUT_KEY_NAME       "F10"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class KeyValues;
class CToolUI;
class CToolMenuButton;
class CMiniViewport;
class IGlobalFlexController;

namespace vgui
{
	class Panel;
	class Menu;
	class CKeyBoardEditorDialog;
	class CKeyBindingHelpDialog;
	enum KeyBindingContextHandle_t;
	class IScheme;
}


//-----------------------------------------------------------------------------
// Save document types
//-----------------------------------------------------------------------------
enum SaveDocumentCloseType_t
{
	SAVEDOC_QUIT_AFTER_SAVE = 0,
	SAVEDOC_CLOSE_AFTER_SAVE,
	SAVEDOC_LEAVEOPEN_AFTER_SAVE,
	SAVEDOC_POSTCOMMAND_AFTER_SAVE,				// Closes, then posts a command
	SAVEDOC_LEAVEOPEN_POSTCOMMAND_AFTER_SAVE,	// Leaves open, then posts a command
};


//-----------------------------------------------------------------------------
// The toolsystem panel is the main panel in which the tool "ui" lives.
// The tool "ui" encapsulates the main menu and a client area in which the
// tools are drawn.
// Usually, the workspace is the size of the entire screen
// and the ui can be smaller than the toolsystem panel. The reason these are decoupled
// is so that you can get the 'action' menu no matter where you click on the screen
//-----------------------------------------------------------------------------
class CBaseToolSystem : public vgui::EditablePanel, public IToolSystem, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CBaseToolSystem, vgui::EditablePanel );

public:
	// Methods inherited from IToolSystem
	virtual bool	Init( );
    virtual void	Shutdown();
	virtual bool	ServerInit( CreateInterfaceFn serverFactory );
	virtual bool	ClientInit( CreateInterfaceFn clientFactory );
	virtual void	ServerShutdown();
	virtual void	ClientShutdown();
	virtual bool	CanQuit(); 
    virtual void	PostMessage( HTOOLHANDLE hEntity, KeyValues *message );
	virtual void	Think( bool finalTick );
	virtual void	ServerLevelInitPreEntity();
	virtual void	ServerLevelInitPostEntity();
	virtual void	ServerLevelShutdownPreEntity();
	virtual void	ServerLevelShutdownPostEntity();
	virtual void	ServerFrameUpdatePreEntityThink();
	virtual void	ServerFrameUpdatePostEntityThink();
	virtual void	ServerPreClientUpdate();
	virtual void	ServerPreSetupVisibility();
	virtual const char* GetEntityData( const char *pActualEntityData );
	virtual void	ClientLevelInitPreEntity();
	virtual void	ClientLevelInitPostEntity();
	virtual void	ClientLevelShutdownPreEntity();
	virtual void	ClientLevelShutdownPostEntity();
	virtual void	ClientPreRender();
	virtual void	ClientPostRender();
	virtual void	OnToolActivate();
	virtual void	OnToolDeactivate();
	virtual bool	TrapKey( ButtonCode_t key, bool down );
	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual bool	SetupEngineView( Vector &origin, QAngle &angles, float &fov );
	virtual bool	SetupAudioState( AudioState_t &audioState );
	virtual bool	ShouldGameRenderView();
	virtual bool	IsThirdPersonCamera();
	virtual bool	IsToolRecording();
	virtual IMaterialProxy *LookupProxy( const char *proxyName );
	virtual bool	GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info );
	virtual void	HostRunFrameBegin();
	virtual void	HostRunFrameEnd();
	virtual void	RenderFrameBegin();
	virtual void	RenderFrameEnd();
	virtual void	VGui_PreRender( int paintMode );
	virtual void	VGui_PostRender( int paintMode );
	virtual void	VGui_PreSimulate();
	virtual void	VGui_PostSimulate();

	// Inherited from vgui::Panel
	virtual void	OnMousePressed( vgui::MouseCode code );
	virtual void	OnThink();
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme);

	// Inherited from IFileOpenStateMachineClient
	virtual void	SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues ) { Assert(0); }
	virtual bool	OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues ) { Assert(0); return false; }
	virtual bool	OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues ) { Assert(0); return false; }

	MESSAGE_FUNC_INT( OnUnhandledMouseClick, "UnhandledMouseClick", code );

public:
	// Other methods
	// NOTE: This name here is 'general' strictly so 'general' shows up in the keybinding dialog
	CBaseToolSystem( char const *toolName = "#ToolGeneral" );

	// Gets the action target to sent to panels so that the tool system's OnCommand is called
	vgui::Panel *GetActionTarget();

	// Gets at the action menu
	vgui::Menu *GetActionMenu();

	// Returns the client area
	vgui::Panel* GetClientArea();

	// Adds a menu button to the main menu bar
	void		AddMenuButton( CToolMenuButton *pMenuButton );

	// Returns the current map name
	char const	*MapName() const;

	// Derived classes implement this to create an action menu
	// that appears if you right-click in the tool workspace
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent ) { return NULL; }

	// Derived classes implement this to create a custom menubar
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );
	// Derived classes implement this to create status bar, can return NULL for no status bar in tool...
	virtual vgui::Panel *CreateStatusBar( vgui::Panel *pParent );

	virtual CMiniViewport	*CreateMiniViewport( vgui::Panel *parent );

	virtual void UpdateMenu( vgui::Menu *menu );

	virtual void ShowMiniViewport( bool state );
	void SetMiniViewportBounds( int x, int y, int width, int height );
	void SetMiniViewportText( const char *pText );

	void GetMiniViewportEngineBounds( int &x, int &y, int &width, int &height );
	vgui::Panel	*GetMiniViewport( void );

	virtual void		ComputeMenuBarTitle( char *buf, size_t buflen );

	// Usage mode
	void SetMode( bool bGameInputEnabled, bool bFullscreen );
	bool IsFullscreen() const;
 	bool IsGameInputEnabled() const;
	void EnableFullscreenToolMode( bool bEnable );

	// Is this the active tool?
	bool IsActiveTool( ) const;

	// Returns the tool that had focus most recently
	Panel *GetMostRecentlyFocusedTool();

	void	PostMessageToAllTools( KeyValues *message );

protected:
	virtual void	PaintBackground();

	// Derived classes must implement this to specify where in the 
	// registry to store registry settings
	virtual const char *GetRegistryName() = 0;

	// Derived classes must return the key bindings context
	virtual const char *GetBindingsContextFile() = 0;

	// Derived classes implement this to do stuff when the tool is shown or hidden
	virtual void OnModeChanged() {}

	// Derived classes can implement this to get a new scheme to be applied to this tool
	virtual vgui::HScheme	GetToolScheme() { return 0; }

	// Derived classes can implement this to get notified when files are saved/loaded
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues ) {}

	// Used to open a specified file, and deal with all the lovely dialogs
	void OpenFile( const char *pOpenFileType, const char *pSaveFileName = NULL, const char *pSaveFileType = NULL, int nFlags = 0, KeyValues *pKeyValues = NULL );
	void OpenFile( const char *pOpenFileName, const char *pOpenFileType, const char *pSaveFileName = NULL, const char *pSaveFileType = NULL, int nFlags = 0, KeyValues *pKeyValues = NULL );
	
	// Used to save a specified file, and deal with all the lovely dialogs
	// Pass in NULL to get a dialog to choose a filename to save
	// Posts the keyvalues
	void SaveFile( const char *pFileName, const char *pFileType, int nFlags, KeyValues *pKeyValues = NULL );
	
	KEYBINDING_FUNC_NODECLARE( editkeybindings, KEY_E, vgui::MODIFIER_SHIFT | vgui::MODIFIER_CONTROL | vgui::MODIFIER_ALT, OnEditKeyBindings, "#editkeybindings_help", 0 );
	KEYBINDING_FUNC( keybindinghelp, KEY_H, 0, OnKeyBindingHelp, "#keybindinghelp_help", 0 );

	virtual char const *GetBackgroundTextureName();
	virtual char const *GetLogoTextureName() = 0;

	virtual bool		HasDocument();

	virtual void ToggleForceToolCamera();

	// Shows, hides the tool ui (menu, client area, status bar)
	void	SetToolUIVisible( bool bVisible );

	// Deals with keybindings
	void	LoadKeyBindings();
	void	ShowKeyBindingsEditor( vgui::Panel *panel, vgui::KeyBindingContextHandle_t handle );
	void	ShowKeyBindingsHelp( vgui::Panel *panel, vgui::KeyBindingContextHandle_t handle, vgui::KeyCode boundKey, int modifiers );
	vgui::KeyBindingContextHandle_t GetKeyBindingsHandle();

	// Registers tool window
	void	RegisterToolWindow( vgui::PHandle hPanel );
	void	UnregisterAllToolWindows();
	void	PostMessageToActiveTool( char const *msg, float delay = 0.0f );
	void	PostMessageToActiveTool( KeyValues *pKeyValues, float flDelay = 0.0f );

protected:
	// Recent file list
	CRecentFileList	m_RecentFiles;

private:
	// Shows/hides the tool
	bool				ShowUI( bool bVisible );

	// Updates UI visibility
	void				UpdateUIVisibility();

	// Create, destroy action menu
	void				InitActionMenu();
	void				ShutdownActionMenu();

	// Positions the action menu when it's time to pop it up
	void				PositionActionMenu();

	// Messages related to saving a file
	MESSAGE_FUNC_PARAMS( OnFileStateMachineFinished, "FileStateMachineFinished", kv );

	// Handlers for standard menus
	MESSAGE_FUNC( OnClearRecent, "OnClearRecent" );
	MESSAGE_FUNC( OnEditKeyBindings, "OnEditKeyBindings" );

	// The root toolsystem panel which should cover the entire screen
	// here to allow us to do action menus anywhere

	// The tool UI
	CToolUI *m_pToolUI;

	// The action menu
	vgui::DHANDLE<vgui::Menu> m_hActionMenu;

	bool						m_bGameInputEnabled;
	bool						m_bFullscreenMode;
	bool						m_bIsActive;
	bool						m_bFullscreenToolModeEnabled;

	vgui::DHANDLE< CMiniViewport >	m_hMiniViewport;
	vgui::FileOpenStateMachine	*m_pFileOpenStateMachine;
	IMaterial					*m_pBackground;
	IMaterial					*m_pLogo;

	// Keybindings
	vgui::KeyBindingContextHandle_t					m_KeyBindingsHandle;
	vgui::DHANDLE< vgui::CKeyBoardEditorDialog >	m_hKeyBindingsEditor;
	vgui::DHANDLE< vgui::CKeyBindingHelpDialog >	m_hKeyBindingsHelp;
	CUtlVector< vgui::PHandle >						m_Tools;
	vgui::PHandle									m_MostRecentlyFocused;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Is this the active tool?
//-----------------------------------------------------------------------------
inline bool CBaseToolSystem::IsActiveTool( ) const
{
	return m_bIsActive;
}


//-----------------------------------------------------------------------------
// Mode query
//-----------------------------------------------------------------------------
inline bool CBaseToolSystem::IsFullscreen( ) const
{
	return m_bFullscreenMode;
}

inline bool CBaseToolSystem::IsGameInputEnabled() const
{
	// NOTE: IsActive check here is a little bogus.
	// It's necessary to get the IFM to play nice with other tools, though.
	// Is there a better way of doing it?
	return m_bGameInputEnabled || !m_bIsActive;
}


#endif // BASETOOLSYSTEM_H
