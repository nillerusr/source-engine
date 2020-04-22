//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ASSETBUILDER_H
#define ASSETBUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/FileOpenStateMachine.h"
#include "vgui_controls/PHandle.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlstack.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class IScheme;
	class ListPanel;
	class Menu;
	class MenuButton;
	class Splitter;
	class FileOpenStateMachine;
	class PropertySheet;
	class PropertyPage;
}

class CDmePanel;
class CCompileStatusBar;
class CDmeMakefile;
class CDmeSource;
struct DmeMakefileType_t;
enum CompilationState_t;


//-----------------------------------------------------------------------------
// Purpose: Asset builder
//-----------------------------------------------------------------------------
class CAssetBuilder : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CAssetBuilder, EditablePanel );

public:
	CAssetBuilder( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CAssetBuilder();

	// Inherited from vgui::Frame
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnTick();

	void SetRootMakefile( CDmeMakefile *pMakeFile );
	void SetCurrentMakefile( CDmeMakefile *pMakeFile );
	void SetDmeElement( CDmeMakefile *pMakeFile );
	CDmeMakefile *GetMakeFile();
	CDmeMakefile *GetRootMakeFile();

	void Refresh();

	// Default behavior is to destroy the makefile when we close
	void DestroyMakefileOnClose( bool bEnable );

	/*
	messages sent:
		"DmeElementChanged"	The makefile has been changed
	*/

private:
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "ItemDeselected", kv );	
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );	
	MESSAGE_FUNC( SetDirty, "DmeElementChanged" );
	MESSAGE_FUNC( OnAddSource, "AddSource" );
	MESSAGE_FUNC( OnNewSourceFile, "NewSourceFile" );
	MESSAGE_FUNC( OnLoadSourceFile, "LoadSourceFile" );
	MESSAGE_FUNC( OnEditSourceFile, "EditSourceFile" );
	MESSAGE_FUNC( OnRemoveSource, "RemoveSource" );
	MESSAGE_FUNC( OnBrowseSourceFile, "BrowseSourceFile" );
	MESSAGE_FUNC( OnZoomInSource, "ZoomInSource" );
	MESSAGE_FUNC( OnZoomOutSource, "ZoomOutSource" );

	void OnCompile();
	void OnAbortCompile();
	void OnPublish();

	// Called to create a new makefile
	void OnNewSourceFileSelected( const char *pFileName, KeyValues *pDialogKeys );

	// Called when a list panel's selection changes
	void OnSourceItemSelectionChanged( );

	// Refresh the source list
	void RefreshSourceList( );

	// Refreshes the output list
	void RefreshOutputList();

	// Selects a particular source
	void SelectSource( CDmeSource *pSource );
	 
	// Called when the source file name changes
	void OnSourceFileNameChanged( const char *pFileName );

	// Called when we're browsing for a source file and one was selected
	void OnSourceFileAdded( const char *pFileName, const char *pTypeName );

	// Shows the source file browser
	void ShowSourceFileBrowser( const char *pTitle, DmeMakefileType_t *pSourceType, KeyValues *pDialogKeys );

	// Make all outputs writeable
	void MakeOutputsWriteable( );

	// Cleans up the context menu
	void CleanupContextMenu();

	// Removes a makefile from memory
	void CleanupMakefile();

	// Builds a unique list of file IDs
	void BuildFileIDList( CDmeMakefile *pMakeFile, CUtlVector<DmFileId_t> &fileIds );

	// Selects a particular row of the source list
	void SelectSourceListRow( int nRow );

	// Returns the curerntly selected row
	int GetSelectedRow( );

	// Finishes compilation
	void FinishCompilation( CompilationState_t state );

	// Returns the selected source (if there's only 1 source selected)
	CDmeSource *GetSelectedSource( );
	KeyValues *GetSelectedSourceKeyvalues( );

	vgui::PropertySheet *m_pInputOutputSheet;
	vgui::PropertyPage *m_pInputPage;
	vgui::PropertyPage *m_pOutputPage;
	vgui::PropertyPage *m_pCompilePage;
	vgui::PropertyPage *m_pOutputPreviewPage;

	vgui::Splitter *m_pPropertiesSplitter;
	vgui::ListPanel *m_pSourcesList;
	vgui::ListPanel *m_pOutputList;
	CDmePanel *m_pDmePanel;
	CDmePanel *m_pOututPreviewPanel;
	vgui::TextEntry *m_pCompileOutput;
	vgui::Button *m_pCompile;
	vgui::Button *m_pPublish;
	vgui::Button *m_pAbortCompile;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
	CCompileStatusBar *m_pCompileStatusBar;

	CDmeHandle< CDmeMakefile > m_hRootMakefile;
	CDmeHandle< CDmeMakefile > m_hMakefile;
	CUtlStack< CDmeHandle< CDmeMakefile > > m_hMakefileStack;
	bool m_bIsCompiling : 1;
	bool m_bDestroyMakefileOnClose : 1;
};


//-----------------------------------------------------------------------------
// Purpose: Asset builder frame
//-----------------------------------------------------------------------------
class CAssetBuilderFrame : public vgui::Frame, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CAssetBuilderFrame, vgui::Frame );

public:
	CAssetBuilderFrame( vgui::Panel *pParent, const char *pTitle );
	virtual ~CAssetBuilderFrame();

	// Inherited from IFileOpenStateMachineClient
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );

protected:
	// Call to change the makefile
	void Reset( CDmeMakefile *pMakefile );

	CAssetBuilder *m_pAssetBuilder;

private:
	MESSAGE_FUNC( OnDmeElementChanged, "DmeElementChanged" );	
	MESSAGE_FUNC( OnFileNew, "FileNew" );	
	MESSAGE_FUNC( OnFileOpen, "FileOpen" );	
	MESSAGE_FUNC( OnFileSave, "FileSave" );	
	MESSAGE_FUNC( OnFileSaveAs, "FileSaveAs" );
	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );	
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );
	MESSAGE_FUNC_PARAMS( OnFileStateMachineFinished, "FileStateMachineFinished", kv );
	MESSAGE_FUNC_PARAMS( OnPerformFileNew, "PerformFileNew", kv );

	// Updates the file name
	MESSAGE_FUNC( UpdateFileName, "UpdateFileName" );

	// Shows a picker for creating a new asset
	void ShowNewAssetPicker( );

	// Marks the file dirty ( or not )
	void SetDirty( bool bDirty );
	bool IsDirty() const;

	vgui::FileOpenStateMachine *m_pFileOpenStateMachine;
	CUtlString m_TitleString;
};


#endif // ASSETBUILDER_H
