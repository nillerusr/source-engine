//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMSETPRESETFADERPANEL_H
#define BASEANIMSETPRESETFADERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AnimSetAttributeValue.h"
#include "datamodel/dmehandle.h"
#include "vgui_controls/EditablePanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPresetSlider;
class CBaseAnimationSetEditor;
class CDmeAnimationSet;
class CSliderListPanel;
class CAddPresetDialog;
class CDmePreset;
class CDmePresetGroupEditorFrame;

namespace vgui
{
	class InputDialog;
}

struct FaderPreview_t
{
	FaderPreview_t() :
		name( 0 ),
		amount( 0 ),
		isbeingdragged( false ),
		holdingctrl( false ),
		values( 0 )
	{
	}
	const char		*name;
	float			amount;
	bool			isbeingdragged;
	bool			holdingctrl;
	AttributeDict_t *values;
	CDmeHandle< CDmePreset > preset;
};


//-----------------------------------------------------------------------------
// Base class for the preset fader panel
//-----------------------------------------------------------------------------
class CBaseAnimSetPresetFaderPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAnimSetPresetFaderPanel, vgui::EditablePanel );
public:
	CBaseAnimSetPresetFaderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor );

	void	GetPreviewFader( FaderPreview_t& fader );

	void	ChangeAnimationSet( CDmeAnimationSet *newAnimSet );
	void	UpdateControlValues();

	void	ApplyPreset( float flScale, AttributeDict_t& dict );

	// Takes slider current values and creates a new preset
	void	AddNewPreset( const char *pGroupName, const char *pName );
	void	SetPresetFromSliders( CDmePreset *pPreset );
	virtual void	OnOverwritePreset( CDmePreset *pPreset );
	void	OnDeletePreset( CDmePreset *pPreset );

	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	virtual void ProceduralPreset_UpdateCrossfade( CDmePreset *pPreset, bool bFadeIn );

protected:
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );
	MESSAGE_FUNC( OnAddPreset, "AddPreset" );
	MESSAGE_FUNC_PARAMS( OnAddNewPreset, "AddNewPreset", params );
	MESSAGE_FUNC( OnPresetsChanged, "PresetsChanged" );
	MESSAGE_FUNC( OnSetCrossfadeSpeed, "SetPresetCrossfadeSpeed" );
	MESSAGE_FUNC( OnManagePresets, "ManagePresets" );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", params );
	MESSAGE_FUNC_PARAMS( OnPresetNameSelected, "PresetNameSelected", params );

protected:
	void OnAddCompleted( const char *pText, KeyValues *pContextKeyValues );
	void PopulateList( bool bChanged );
	void AddNewPreset( CDmePreset *pPreset );

	vgui::DHANDLE< CBaseAnimationSetEditor > m_hEditor;
	vgui::EditablePanel						*m_pWorkspace;
	vgui::TextEntry							*m_pFilter;
	CSliderListPanel						*m_pSliders;
	CDmeHandle< CDmeAnimationSet >			m_AnimSet;
	float									m_flLastFrameTime;
	CUtlString								m_Filter;
	vgui::DHANDLE< vgui::InputDialog >		m_hInputDialog;
	vgui::DHANDLE< CAddPresetDialog >		m_hAddPresetDialog;
	vgui::DHANDLE< CDmePresetGroupEditorFrame > m_hPresetEditor;

	CUtlVector< CDmeHandle< CDmePreset > >	m_CurrentPresetList;
};

#endif // BASEANIMSETPRESETFADERPANEL_H
