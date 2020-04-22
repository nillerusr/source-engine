//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMSETATTRIBUTESLIDERPANEL_H
#define BASEANIMSETATTRIBUTESLIDERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmehandle.h"
#include "vgui_controls/EditablePanel.h"
#include "dme_controls/AnimSetAttributeValue.h"
#include "dme_controls/logpreview.h"
#include "movieobjects/dmechannel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimationSetEditor;
class CDmeAnimationSet;
class CAttributeSlider;
class CDmElement;
class CDmeChannel;
class CDmeFilmClip;
class CDmeTimeSelection;
struct LogPreview_t;
enum RecordingMode_t;
class DmeLog_TimeSelection_t;
class CPresetSideFilterSlider;
struct FaderPreview_t;

enum
{
	FADER_NAME_CHANGED		= ( 1<<0 ),
	FADER_DRAG_CHANGED		= ( 1<<1 ),
	FADER_CTRLKEY_CHANGED	= ( 1<<2 ),
	FADER_AMOUNT_CHANGED	= ( 1<<3 ),
	FADER_PRESET_CHANGED	= ( 1<< 4 ),
};

//-----------------------------------------------------------------------------
// CBaseAnimSetAttributeSliderPanel
//-----------------------------------------------------------------------------
class CBaseAnimSetAttributeSliderPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAnimSetAttributeSliderPanel, vgui::EditablePanel );
public:
	CBaseAnimSetAttributeSliderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor );

public:

	void								ChangeAnimationSet( CDmeAnimationSet *newAnimSet );
	void								SetVisibleControlsForSelectionGroup( CUtlSymbolTable& visible );
	void								ApplyPreset( float flScale, AttributeDict_t& values );
	bool								GetAttributeSliderValue( AttributeValue_t *pValue, const char *name );

	void								SetLogPreviewControlFromSlider( CAttributeSlider *pSlider );
	CDmElement							*GetElementFromSlider( CAttributeSlider *pSlider );
	CDmElement							*GetLogPreviewControl();
	CBaseAnimationSetEditor*			GetEditor();

	void								RecomputePreview();
	virtual void						PerformRecomputePreview();

	virtual CDmeFilmClip				*GetCurrentShot();
	virtual CDmeFilmClip				*GetCurrentMovie();

	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	CUtlVector< LogPreview_t >			*GetActiveTransforms();

	void								GetChannelsForControl( CDmElement *control, CDmeChannel *channels[LOG_PREVIEW_MAX_CHANNEL_COUNT] );
	void								SetTimeSelectionParametersForRecordingChannels( float flIntensity );
	void								MoveToSlider( CAttributeSlider *pCurrentSlider, int nDirection );
	void								SetLogPreviewControl( CDmElement *ctrl );

	void								ClearSelectedControls();
	void								SetControlSelected( CAttributeSlider *slider, bool state );
	void								SetControlSelected( CDmElement *control, bool state );

	virtual int							BuildVisibleControlList( CUtlVector< LogPreview_t >& list );
	virtual int							BuildFullControlList( CUtlVector< LogPreview_t >& list );


	virtual void						StampValueIntoLogs( CDmElement *control, AnimationControlType_t type, float flValue );

	struct VisItem_t
	{
		VisItem_t() : 
			element( NULL ), selected( false ), index( 0 )
		{
		}
		CDmElement *element;
		bool		selected;
		int			index;
	};

	void									GetVisibleControls( CUtlVector< VisItem_t >& list );

	// Returns true if slider is visible
	bool									GetSliderValues( AttributeValue_t *pValue, int nIndex );

	virtual void SetupForPreset( FaderPreview_t &fader, int nChangeFlags );

	float GetBalanceSliderValue();

protected:

	virtual void OnThink();
	virtual void OnCommand( const char *pCommand );
	virtual bool		ApplySliderValues( bool force );

	virtual void PerformLayout();

	KEYBINDING_FUNC( deselectall, KEY_ESCAPE, 0, OnKBDeselectAll, "#deselectall_help", 0 );

protected:
	struct ChannelToSliderLookup_t
	{
		ChannelToSliderLookup_t() : type( ANIM_CONTROL_VALUE ) {}

		CDmeHandle< CDmeChannel	> ch;
		CDmeHandle< CDmElement	> slider;
		AnimationControlType_t type;

		static bool Less( const ChannelToSliderLookup_t& lhs, const ChannelToSliderLookup_t& rhs );
	};

	void									UpdatePreviewSliderTimes();
	void									ActivateControlSetInMode( int mode, int otherChannelsMode, int hiddenChannelsMode, CAttributeSlider *whichSlider = NULL );
	void									MaybeAddPreviewLog( CDmeFilmClip *shot, CUtlVector< LogPreview_t >& list, CDmElement *control, bool bDragging, bool isActiveLog, bool bSelected );


	CAttributeSlider						*FindSliderForControl( CDmElement *control );

	virtual void							GetActiveTimeSelectionParams( DmeLog_TimeSelection_t& params );

	vgui::DHANDLE< CBaseAnimationSetEditor >	m_hEditor;
	// Visible slider list
	vgui::DHANDLE< vgui::PanelListPanel >	m_Sliders;
	// All sliders
	CUtlVector< CAttributeSlider * >		m_SliderList;
	vgui::Button							*m_pLeftRightBoth[ 2 ];
	CPresetSideFilterSlider				*m_pPresetSideFilter;

	CDmeHandle< CDmeAnimationSet >		m_AnimSet;
	CDmeHandle< CDmElement	>			m_PreviewControl;

	CDmeHandle< CDmElement	>			m_CtrlKeyPreviewSliderElement;
	vgui::DHANDLE< CAttributeSlider >	m_CtrlKeyPreviewSlider;
	float								m_flEstimatedValue;

	CUtlString							m_PreviousPreviewFader;
	FaderPreview_t						m_Previous;

	int									m_nFaderChangeFlags;

	bool								m_bRequestedNewPreview : 1;
	int									m_nActiveControlSetMode;

	CUtlRBTree< ChannelToSliderLookup_t, unsigned short >	m_ChannelToSliderLookup;

	// list of bones/root transforms which are in the control set
	CUtlVector< LogPreview_t >			m_ActiveTransforms;
	float								m_flRecomputePreviewTime;

	CUtlVector< LogPreview_t >			m_CurrentPreview;

	float								m_flPrevTime;

};

inline CBaseAnimationSetEditor*	CBaseAnimSetAttributeSliderPanel::GetEditor()
{
	return m_hEditor;
}


#endif // BASEANIMSETATTRIBUTESLIDERPANEL_H
