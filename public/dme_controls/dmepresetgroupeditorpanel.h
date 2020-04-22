//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PRESETGROUPEDITORPANEL_H
#define PRESETGROUPEDITORPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "vgui_controls/Frame.h"
#include "datamodel/dmehandle.h"
#include "vgui_controls/fileopenstatemachine.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeAnimationSet;
class CDmePresetListPanel;
class CDmePresetGroupListPanel;
class CDmePresetGroup;
class CDmePreset;
namespace vgui
{
	class PropertySheet;
	class PropertyPage;
	class Button;
}


//-----------------------------------------------------------------------------
// Dag editor panel
//-----------------------------------------------------------------------------
class CDmePresetGroupEditorPanel : public vgui::EditablePanel, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CDmePresetGroupEditorPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmePresetGroupEditorPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmePresetGroupEditorPanel();

	// Sets the current scene + animation list
	void SetAnimationSet( CDmeAnimationSet *pAnimationSet );
	CDmeAnimationSet *GetAnimationSet();

	void RefreshAnimationSet();
	void NotifyDataChanged();

	// Returns selected presets/groups
	CDmePresetGroup* GetSelectedPresetGroup( );
	CDmePreset* GetSelectedPreset( );

	// Drag/drop reordering of preset groups
	void MovePresetGroupInFrontOf( CDmePresetGroup *pDragGroup, CDmePresetGroup *pDropGroup );

	// Drag/drop reordering of presets
	void MovePresetInFrontOf( CDmePreset *pDragPreset, CDmePreset *pDropPreset );

	// Drag/drop preset moving
	void MovePresetIntoGroup( CDmePreset *pPreset, CDmePresetGroup *pGroup );

	// Toggle group visibility
	void ToggleGroupVisibility( CDmePresetGroup *pPresetGroup );

	MESSAGE_FUNC( OnMovePresetUp, "MovePresetUp" );
	MESSAGE_FUNC( OnMovePresetDown, "MovePresetDown" );
	MESSAGE_FUNC( OnMoveGroupUp, "MoveGroupUp" );
	MESSAGE_FUNC( OnMoveGroupDown, "MoveGroupDown" );
	MESSAGE_FUNC( OnRemoveGroup, "RemoveGroup" );
	MESSAGE_FUNC( OnRemovePreset, "RemovePreset" );

	// Inherited from IFileOpenStateMachineClient
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );

private:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );
	MESSAGE_FUNC( OnAddGroup, "AddGroup" );
	MESSAGE_FUNC( OnAddPhonemeGroup, "AddPhonemeGroup" );
	MESSAGE_FUNC( OnRenameGroup, "RenameGroup" );
	MESSAGE_FUNC( OnEditPresetRemapping, "EditPresetRemapping" );
	MESSAGE_FUNC( OnRemoveDefaultControls, "RemoveDefaultControls" );
	MESSAGE_FUNC( OnRemapPresets, "RemapPresets" );
	MESSAGE_FUNC( OnAddPreset, "AddPreset" );
	MESSAGE_FUNC( OnRenamePreset, "RenamePreset" );
	MESSAGE_FUNC( OnToggleGroupVisibility, "ToggleGroupVisibility" );
	MESSAGE_FUNC( OnToggleGroupSharing, "ToggleGroupSharing" );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "ItemDeselected", kv );	
	MESSAGE_FUNC( OnImportPresets, "ImportPresets" );
	MESSAGE_FUNC( OnExportPresets, "ExportPresets" );
	MESSAGE_FUNC( OnImportPresetGroups, "ImportPresetGroups" );
	MESSAGE_FUNC( OnExportPresetGroups, "ExportPresetGroups" );
	MESSAGE_FUNC( OnExportPresetGroupToVFE, "ExportPresetGroupsToVFE" );
	MESSAGE_FUNC( OnExportPresetGroupToTXT, "ExportPresetGroupsToTXT" );
	MESSAGE_FUNC_PARAMS( OnPresetPicked, "PresetPicked", params );
	MESSAGE_FUNC_PARAMS( OnPresetPickCancelled, "PresetPickCancelled", params );
	MESSAGE_FUNC_PARAMS( OnFileStateMachineFinished, "FileStateMachineFinished", params );

	// Cleans up the context menu
	void CleanupContextMenu();

	// If it finds a duplicate group/preset name, reports an error message and returns it found one
	bool HasDuplicatePresetName( const char *pPresetName, CDmePreset *pIgnorePreset = NULL );
	bool HasDuplicateGroupName( const char *pControlName, CDmePresetGroup *pIgnoreGroup = NULL );

	// Refreshes the list of presets
	void RefreshPresetNames( );

	// Called by OnInputCompleted after we get a new group or preset name
	void PerformAddGroup( const char *pNewGroupName );
	void PerformAddPhonemeGroup( const char *pNewGroupName );
	void PerformRenameGroup( const char *pNewGroupName );
	void PerformAddPreset( const char *pNewPresetName );
	void PerformRenamePreset( const char *pNewPresetName );

	// Called to open a context-sensitive menu for a particular preset
	void OnOpenPresetContextMenu( );

	// Gets/sets a selected preset
	void SetSelectedPreset( CDmePreset* pPreset );

	// Selects a particular preset group
	void SetSelectedPresetGroup( CDmePresetGroup* pPresetGroup );

	// Imports presets
	void ImportPresets( const CUtlVector< CDmePreset * >& presets );

	CDmeHandle< CDmeAnimationSet > m_hAnimationSet;
	vgui::Splitter *m_pSplitter;
	CDmePresetGroupListPanel *m_pPresetGroupList;
	CDmePresetListPanel *m_pPresetList;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
	vgui::DHANDLE< vgui::FileOpenStateMachine > m_hFileOpenStateMachine;
};


//-----------------------------------------------------------------------------
// Frame for combination system
//-----------------------------------------------------------------------------
class CDmePresetGroupEditorFrame : public vgui::Frame, public IDmNotify
{
	DECLARE_CLASS_SIMPLE( CDmePresetGroupEditorFrame, vgui::Frame );

public:
	CDmePresetGroupEditorFrame( vgui::Panel *pParent, const char *pTitle );
	~CDmePresetGroupEditorFrame();

	// Sets the current scene + animation list
	void SetAnimationSet( CDmeAnimationSet *pAnimationSet );

	// Inherited from IDmNotify
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

private:
	MESSAGE_FUNC( OnPresetsChanged, "PresetsChanged" );
	MESSAGE_FUNC_PARAMS( OnAddNewPreset, "AddNewPreset", params );
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

	CDmePresetGroupEditorPanel *m_pEditor;
	vgui::Button *m_pOkButton;
};


#endif // PRESETGROUPEDITORPANEL_H