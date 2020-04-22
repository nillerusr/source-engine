//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Declaration of CSceneViewerPanel
//
//=============================================================================

#ifndef SCENEVIEWERPANEL_H_2DF240CE_62EF_4391_B733_37C393E04E9E
#define SCENEVIEWERPANEL_H_2DF240CE_62EF_4391_B733_37C393E04E9E

// Valve includes
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmeanimationlist.h"
#include "dme_controls/ElementPropertiesTree.h"
#include "dme_controls/dmecombinationsystemeditorpanel.h"
#include "dme_controls/AssetBuilder.h"
#include "dme_controls/ElementPropertiesTree.h"
#include "vgui_controls/FileOpenStateMachine.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/consoledialog.h"
#include "datamodel/idatamodel.h"

// Local includes
#include "ClipViewPanel.h"


//-----------------------------------------------------------------------------
// main editor panel
//-----------------------------------------------------------------------------
class CSceneViewerPanel : public vgui::Panel, public IDmNotify, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CSceneViewerPanel, vgui::Panel );

public:

	CSceneViewerPanel();
	~CSceneViewerPanel();

	// Inherited from vgui::Panel
	virtual void PerformLayout();
	virtual void OnCommand( char const *cmd );
	virtual vgui::VPANEL GetCurrentKeyFocus();
	virtual vgui::VPANEL GetCurrentMouseFocus();
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	void	OnEdit();

	void	OnUndo();
	void	OnRedo();

	void	OnDescribeUndoStack();

	CDmeDag *GetScene() { return m_pClipViewPanel->GetScene(); }

	MESSAGE_FUNC( OnNew, "New" );
	MESSAGE_FUNC( OnOpen, "Open" );
	MESSAGE_FUNC( OnSave, "Save" );
	MESSAGE_FUNC( OnSaveAs, "SaveAs" );
	MESSAGE_FUNC( OnSaveCurrentAs, "SaveCurrentAs" );
	MESSAGE_FUNC( OnExit, "Exit" );

	MESSAGE_FUNC_CHARPTR( OnLoadFile, "LoadFile", fullpath );
	MESSAGE_FUNC( OnPinAndZoomIt, "PinAndZoomIt" );

	MESSAGE_FUNC( OnShow3DView, "Show3DView" );
	MESSAGE_FUNC( OnHide3DView, "Hide3DView" );
	MESSAGE_FUNC( OnShowComboEditor, "ShowComboEditor" );
	MESSAGE_FUNC( OnHideComboEditor, "HideComboEditor" );
	MESSAGE_FUNC( OnShowAssetBuilder, "ShowAssetBuilder" );
	MESSAGE_FUNC( OnHideAssetBuilder, "HideAssetBuilder" );
	MESSAGE_FUNC( OnShowConsole, "ShowConsole" );
	MESSAGE_FUNC( OnHideConsole, "HideConsole" );
	MESSAGE_FUNC( OnShowNerdEditor, "ShowNerdEditor" );
	MESSAGE_FUNC( OnHideNerdEditor, "HideNerdEditor" );
	MESSAGE_FUNC( OnCombinationOperatorChanged, "CombinationOperatorChanged" );
	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );

	virtual void OnSizeChanged( int newWidth, int newHeight );

protected:
	// Inherited from IDmNotify
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Inherited from IFileOpenStateMachineClient
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );

	// Removes all data references from all vgui things
	void Clear();

	// Loads a Dmx file from disk
	bool Load( const char *pFilename, CUtlStringMap< Vector > *pOldComboVals = NULL );

	bool Reload();

	// Saves the current state of all delta meshes as a single OBJ
	bool SaveCurrentAs( const char *pFilename );

	// Sets all of the various editors to reference the right stuff
	void SetScene();

	// Deals with keybindings
	void	LoadKeyBindings();
	void	ShowKeyBindingsEditor( vgui::Panel *panel, vgui::KeyBindingContextHandle_t handle );
	void	ShowKeyBindingsHelp( vgui::Panel *panel, vgui::KeyBindingContextHandle_t handle, vgui::KeyCode boundKey, int modifiers );
	vgui::KeyBindingContextHandle_t GetKeyBindingsHandle();

	// Keybindings
	vgui::KeyBindingContextHandle_t					m_KeyBindingsHandle;

private:
	// Sets up a new mesh dag
	CDmeModel *CreateNewMeshDag( CDmeMesh **ppMesh, DmFileId_t fileid );

	// Sets up a new mesh animation
	CDmeAnimationList *CreateNewJointAnimation( CDmeModel *pModel );

	// Sets up a new vertex animation
	CDmeAnimationList *CreateNewVertexAnimation( CDmeMesh *pMesh, CDmeCombinationOperator *pComboOp );

	void SendFrameToDagRenderPanel( vgui::Panel *pPanel );

	vgui::MenuBar *m_pMenuBar;
	
	vgui::DHANDLE< vgui::FileOpenDialog > m_hFileOpenDialog;
	vgui::DHANDLE< CElementPropertiesTree > m_hProperties;

	// Root scene object
	vgui::Panel *m_pClientArea;
	CClipViewPanel *m_pClipViewPanel;
	vgui::DHANDLE< CDmeCombinationSystemEditorFrame > m_pCombinationEditor;
	vgui::DHANDLE< CAssetBuilderFrame > m_pAssetBuilder;
	vgui::DHANDLE< CElementPropertiesTree > m_pNerdEditor;

	CDmElement *m_pRoot;
	vgui::FileOpenStateMachine *m_pFileOpenStateMachine;
	vgui::CConsoleDialog *m_pConsole;
	bool m_bConsolePositioned;

	CUtlString m_fileDirectory;
	CUtlString m_filename;
};

#endif // defined SCENEVIEWERPANEL_H_2DF240CE_62EF_4391_B733_37C393E04E9E
