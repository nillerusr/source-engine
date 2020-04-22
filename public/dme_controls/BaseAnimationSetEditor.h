//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMATIONSETEDITOR_H
#define BASEANIMATIONSETEDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ImageList.h"
#include "datamodel/dmehandle.h"
#include "vgui/KeyCode.h"
#include "dme_controls//AnimSetAttributeValue.h"
#include "dme_controls/RecordingState.h"
#include "tier1/utlvector.h"
#include "movieobjects/dmelog.h"
#include "vgui_controls/fileopenstatemachine.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct LogPreview_t;
class CDmeAnimationSet;
class CDmeAnimationList;
class CDmeChannelsClip;
class CDmeChannel;
class CBaseAnimSetControlGroupPanel;
class CBaseAnimSetPresetFaderPanel;
class CBaseAnimSetAttributeSliderPanel;
class CDmeGameModel;


//-----------------------------------------------------------------------------
// Base class for the panel for editing animation sets
//-----------------------------------------------------------------------------
class CBaseAnimationSetEditor : public vgui::EditablePanel, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CBaseAnimationSetEditor, vgui::EditablePanel );

public:
	enum EAnimSetLayout_t
	{
		LAYOUT_SPLIT = 0,
		LAYOUT_VERTICAL,
		LAYOUT_HORIZONTAL,
	};

	CBaseAnimationSetEditor( vgui::Panel *parent, char const *panelName, bool bShowGroups );
	virtual ~CBaseAnimationSetEditor();

	virtual void						CreateToolsSubPanels();
	int									BuildVisibleControlList( CUtlVector< LogPreview_t >& list );
	int									BuildFullControlList( CUtlVector< LogPreview_t >& list );
	void								RecomputePreview();
	virtual void						ChangeLayout( EAnimSetLayout_t newLayout );

	CBaseAnimSetControlGroupPanel		*GetControlGroup();
	CBaseAnimSetPresetFaderPanel		*GetPresetFader();
	CBaseAnimSetAttributeSliderPanel	*GetAttributeSlider();

	void								ChangeAnimationSet( CDmeAnimationSet *newAnimSet );
	virtual void						SetRecordingState( RecordingState_t state, bool updateSettings );
	RecordingState_t					GetRecordingState() const;
	CDmeAnimationSet					*GetAnimationSet();

	// Inherited from IFileOpenStateMachineClient
public:
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnDataChanged();

	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", params );
	MESSAGE_FUNC_INT( OnChangeLayout, "OnChangeLayout", value );
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );
	MESSAGE_FUNC_PARAMS( OnButtonToggled, "ButtonToggled", params );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", params );
	MESSAGE_FUNC_PARAMS( OnImportConfirmed, "ImportConfirmed", params );
	MESSAGE_FUNC_PARAMS( OnImportAnimation, "ImportAnimation", params );
	MESSAGE_FUNC( OnExportFacialAnimation, "ExportFacialAnimation" );
	MESSAGE_FUNC_PARAMS( OnImportAnimationSelected, "DmeSelected", params );
	MESSAGE_FUNC_PARAMS( OnImportAnimationCancelled, "DmeSelectionCancelled", params );

	// Selects an animation to import
	void SelectImportAnimation( CDmeAnimationList *pAnimationList, bool bVisibleOnly );

	// Imports a specific channels clip into the animation set
	void ImportAnimation( CDmeChannelsClip *pChannelsClip, bool bVisibleOnly );

	// Finds a channel in the animation set to overwrite with import data
	CDmeChannel* FindImportChannel( CDmeChannel *pChannel, CDmeChannelsClip *pChannelsClip );

	// Transforms an imported channel, if necessary
	void TransformImportedChannel( CDmeChannel *pChannel );

	// Transforms an imported position log
	void TransformImportedPositionLog( const matrix3x4_t& matrix, CDmeVector3Log *pPositionLog );

	// Transforms an imported orientation log
	void TransformImportedOrientationLog( const matrix3x4_t& matrix, CDmeQuaternionLog *pOrientationLog );

	// Expands channels clip time to encompass log
	void FixupChannelsClipTime( CDmeChannel *pChannel, CDmeLog *pLog );
	void FixupChannelsClipTime( CDmeChannelsClip *pChannelsClip, CDmeLog *pLog );

	// Adds a log layer to the list of logs for export
	void AddLogLayerForExport( CDmElement *pRoot, const char *pControlName, CDmeChannel *pChannel, DmeTime_t tExportStart, DmeTime_t tExportEnd );

	// Exports animations
	void ExportAnimations( CDmElement *pAnimations, DmeTime_t tExportStart, DmeTime_t tExportEnd );

	// Inherited classes need to implement this for export to work.
	virtual CDmeFilmClip	*GetAnimationSetClip() { return NULL; }
	virtual CDmeFilmClip	*GetRootClip() { return NULL; }

protected:
	EAnimSetLayout_t					m_Layout;
	vgui::DHANDLE< vgui::Splitter >		m_Splitter;

	vgui::DHANDLE< CBaseAnimSetControlGroupPanel >		m_hControlGroup;
	vgui::DHANDLE< CBaseAnimSetPresetFaderPanel >		m_hPresetFader;
	vgui::DHANDLE< CBaseAnimSetAttributeSliderPanel >	m_hAttributeSlider;

	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;

	vgui::DHANDLE< vgui::FileOpenStateMachine > m_hFileOpenStateMachine;

	vgui::ToggleButton					*m_pState[ NUM_AS_RECORDING_STATES ];

	vgui::ToggleButton					*m_pSelectionModeType;

	vgui::ImageList						m_Images;

	CDmeHandle< CDmeAnimationSet >		m_AnimSet;

	vgui::ComboBox						*m_pComboBox;

	RecordingState_t					m_RecordingState;
};


#endif // BASEANIMATIONSETEDITOR_H
