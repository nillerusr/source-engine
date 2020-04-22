//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/PanelListPanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "dme_controls/attributeslider.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IInput.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_HEIGHT 32
#define ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_YPOS 0

//-----------------------------------------------------------------------------
// Enums
//-----------------------------------------------------------------------------
#define SLIDER_PIXEL_SPACING 3
#define RECOMPUTE_PREVIEW_INTERVAL ( 0.5f )
#define CIRCULAR_CONTROL_RADIUS 6.0f
#define FRAC_PER_PIXEL 0.0025f

static ConVar ifm_attributesliderbalance_sensitivity( "ifm_attributesliderbalance_sensitivity", "1.0", 0 );

extern float ifm_fader_timescale;

enum
{
	// Just some arbitrary number that we're not likely to see in another .lib or the main app's code
	UNDO_CHAIN_MOUSEWHEEL_ATTRIBUTE_SLIDER = 9876,
};


bool CBaseAnimSetAttributeSliderPanel::ChannelToSliderLookup_t::Less( const ChannelToSliderLookup_t& lhs, const ChannelToSliderLookup_t& rhs )
{
	return lhs.ch.Get() < rhs.ch.Get();
}


//-----------------------------------------------------------------------------
// Blends flex values in left-right space instead of balance/value space
//-----------------------------------------------------------------------------
static void BlendFlexValues( AttributeValue_t *pResult, const AttributeValue_t &src, const AttributeValue_t &dest, float flBlend, float flBalanceFilter = 0.5f )
{
	// Apply the left-right balance to the target
	float flLeftFilter, flRightFilter;
	ValueBalanceToLeftRight( &flLeftFilter, &flRightFilter, flBlend, flBalanceFilter );

	// Do the math in 'left-right' space because we filter in that space
	float flSrcLeft, flSrcRight;
	ValueBalanceToLeftRight( &flSrcLeft, &flSrcRight, src.m_pValue[ANIM_CONTROL_VALUE], src.m_pValue[ANIM_CONTROL_BALANCE] );

	float flDestLeft, flDestRight;
	ValueBalanceToLeftRight( &flDestLeft, &flDestRight, dest.m_pValue[ANIM_CONTROL_VALUE], dest.m_pValue[ANIM_CONTROL_BALANCE] );

	float flTargetLeft = flSrcLeft + flLeftFilter * ( flDestLeft - flSrcLeft );
	float flTargetRight = flSrcRight + flRightFilter * ( flDestRight - flSrcRight );

	LeftRightToValueBalance( &pResult->m_pValue[ANIM_CONTROL_VALUE], &pResult->m_pValue[ANIM_CONTROL_BALANCE], flTargetLeft, flTargetRight, 
		( flBlend <= 0.5f ) ? src.m_pValue[ANIM_CONTROL_BALANCE] : dest.m_pValue[ANIM_CONTROL_BALANCE] );

	pResult->m_pValue[ANIM_CONTROL_MULTILEVEL] = src.m_pValue[ANIM_CONTROL_MULTILEVEL] + ( dest.m_pValue[ANIM_CONTROL_MULTILEVEL] - src.m_pValue[ANIM_CONTROL_MULTILEVEL] ) * flBlend;
}


//-----------------------------------------------------------------------------
//
// CPresetSideFilterSlider class begins
//
//-----------------------------------------------------------------------------
class CPresetSideFilterSlider : public Slider
{
	DECLARE_CLASS_SIMPLE( CPresetSideFilterSlider, Slider );

public:
	CPresetSideFilterSlider( CBaseAnimSetAttributeSliderPanel *pParent, const char *panelName );
	virtual ~CPresetSideFilterSlider();

	float		GetPos();
	void		SetPos( float frac );

protected:
	virtual void Paint();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( IScheme *scheme );
	virtual void GetTrackRect( int &x, int &y, int &w, int &h );
	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);

private:
	CBaseAnimSetAttributeSliderPanel	*m_pParent;

	Color			m_ZeroColor;
	Color			m_TextColor;
	Color			m_TextColorFocus;
	TextImage		*m_pName;
	float			m_flCurrent;
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CPresetSideFilterSlider::CPresetSideFilterSlider( CBaseAnimSetAttributeSliderPanel *parent, const char *panelName ) :
	BaseClass( (Panel *)parent, panelName ), m_pParent( parent )
{
	SetRange( 0, 1000 );
	SetDragOnRepositionNob( true );
	SetPos( 0.5f );
	SetPaintBackgroundEnabled( true );

	m_pName = new TextImage( "Preset Side Filter" );

	SetBgColor( Color( 128, 128, 128, 128 ) );

	m_ZeroColor = Color( 33, 33, 33, 255 );
	m_TextColor = Color( 200, 200, 200, 255 );
	m_TextColorFocus = Color( 208, 143, 40, 255 );
}

CPresetSideFilterSlider::~CPresetSideFilterSlider()
{
	delete m_pName;
}

void CPresetSideFilterSlider::OnMousePressed(MouseCode code)
{
	if ( code == MOUSE_RIGHT )
	{
		SetPos( 0.5f );
		return;
	}

	BaseClass::OnMousePressed( code );
}

void CPresetSideFilterSlider::OnMouseDoublePressed(MouseCode code)
{
	if ( code == MOUSE_LEFT )
	{
		SetPos( 0.5f );
		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}

float CPresetSideFilterSlider::GetPos()
{
	return GetValue() * 0.001f;
}

void CPresetSideFilterSlider::SetPos( float frac )
{
	SetValue( (int)( frac * 1000.0f + 0.5f ), false );
}

void CPresetSideFilterSlider::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pName->SetFont( scheme->GetFont( "DefaultBold" ) );
	m_pName->SetColor( m_TextColor );
	m_pName->ResizeImageToContent();

	SetFgColor( Color( 194, 120, 0, 255 ) );
	SetThumbWidth( 3 );
}

void CPresetSideFilterSlider::GetTrackRect( int &x, int &y, int &w, int &h )
{
	GetSize( w, h );
	x = 0;
	y = 2;
	h -= 4;
}

void CPresetSideFilterSlider::Paint()
{
	// horizontal nob
	int x, y;
	int wide,tall;
	GetTrackRect( x, y, wide, tall );

	Color col = GetFgColor();
	surface()->DrawSetColor( col );
	surface()->DrawFilledRect( _nobPos[0], 1, _nobPos[1], GetTall() - 1 );
	surface()->DrawSetColor( m_ZeroColor );
	surface()->DrawFilledRect( _nobPos[0] - 1, y + 1,  _nobPos[0], y + tall - 1 );
}

void CPresetSideFilterSlider::PaintBackground()
{
	int w, h;
	GetSize( w, h );
			   
	int tx, ty, tw, th;
	GetTrackRect( tx, ty, tw, th );
	surface()->DrawSetColor( m_ZeroColor );
	surface()->DrawFilledRect( tx, ty, tx + tw, ty + th );

	int cw, ch;
	m_pName->SetColor( _dragging ? m_TextColorFocus : m_TextColor );
	m_pName->GetContentSize( cw, ch );
	m_pName->SetPos( ( w - cw ) * 0.5f, ( h - ch ) * 0.5f );
	m_pName->Paint();
}


//-----------------------------------------------------------------------------
//
// CBaseAnimSetAttributeSliderPanel begins
//
//-----------------------------------------------------------------------------
CBaseAnimSetAttributeSliderPanel::CBaseAnimSetAttributeSliderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor ) :
	BaseClass( parent, className ),
	m_flEstimatedValue( 0.0f ),
	m_ChannelToSliderLookup( 0, 0, ChannelToSliderLookup_t::Less ),
	m_flRecomputePreviewTime( -1.0f ),
	m_bRequestedNewPreview( false ),
	m_flPrevTime( 0.0f ),
	m_nFaderChangeFlags( 0 )
{
	m_hEditor = editor;

	m_pLeftRightBoth[ 0 ] = new Button( this, "AttributeSliderLeftOnly", "", this, "OnLeftOnly" );
	m_pLeftRightBoth[ 1 ] = new Button( this, "AttributeSliderRightOnly", "", this, "OnRightOnly" );
	m_pPresetSideFilter = new CPresetSideFilterSlider( this, "PresetSideFilter" );
	m_Sliders = new PanelListPanel( this, "AttributeSliders" );
	m_Sliders->SetFirstColumnWidth( 0 );
	m_Sliders->SetAutoResize
		( 
		Panel::PIN_TOPLEFT, 
		Panel::AUTORESIZE_DOWNANDRIGHT,
		0, ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_HEIGHT,
		0, 0
		);
	m_Sliders->SetVerticalBufferPixels( 0 );

	m_PreviousPreviewFader = "";
	m_Previous.isbeingdragged = false;
	m_Previous.holdingctrl = false;
	m_Previous.amount = 0.0f;
}

void CBaseAnimSetAttributeSliderPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "OnLeftOnly" ) )
	{
		m_pPresetSideFilter->SetPos( 0.0f );
		return;
	}

	if ( !Q_stricmp( pCommand, "OnRightOnly" ) )
	{
		m_pPresetSideFilter->SetPos( 1.0f );
		return;
	}

	BaseClass::OnCommand( pCommand );
}

void CBaseAnimSetAttributeSliderPanel::StampValueIntoLogs( CDmElement *control, AnimationControlType_t type, float flValue )
{
}

void CBaseAnimSetAttributeSliderPanel::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_Sliders->SetBgColor( Color( 42, 42, 42, 255 ) );
}

void CBaseAnimSetAttributeSliderPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int availH = ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_HEIGHT;
				 	 
	int btnSize = 9;
	m_pLeftRightBoth[ 0 ]->SetBounds( 15, ( availH - btnSize ) / 2, btnSize, btnSize );
	m_pLeftRightBoth[ 1 ]->SetBounds( w - 15, ( availH - btnSize ) / 2, btnSize, btnSize );
	m_pPresetSideFilter->SetBounds( 23 + btnSize, 4, w - 38 - 2 * btnSize, availH - 8 );
}



//-----------------------------------------------------------------------------
// Purpose: Determines:
//  a) are we holding the ctrl key still, if so
//     figures out the crossfade amount of each preset slider with non-zero influence
//  b) not holding control, then just see if we are previewing whichever preset the mouse is over
// Input  :  - 
//-----------------------------------------------------------------------------
void CBaseAnimSetAttributeSliderPanel::OnThink()
{
	BaseClass::OnThink();

	CBaseAnimSetPresetFaderPanel *presets = m_hEditor->GetPresetFader();
	if ( !presets )
		return;

	FaderPreview_t fader;
	presets->GetPreviewFader( fader );

	bool nameChanged = ( fader.name && ( m_PreviousPreviewFader.IsEmpty() || Q_stricmp( m_PreviousPreviewFader.Get(), fader.name ) ) ) ? true : false;
	bool beingDraggedChanged = fader.isbeingdragged != m_Previous.isbeingdragged ? true : false;
	bool ctrlKeyChanged = fader.holdingctrl != m_Previous.holdingctrl ? true : false;
	bool bFaderChanged = ( nameChanged || beingDraggedChanged || ctrlKeyChanged );
	bool bPresetChanged = fader.preset != m_Previous.preset;
	bool faderAmountChanged = fader.amount != m_Previous.amount ? true : false;

	m_nFaderChangeFlags = 0;
	if ( nameChanged ) 
	{
		m_nFaderChangeFlags |= FADER_NAME_CHANGED;
	}
	if ( beingDraggedChanged )
	{
		m_nFaderChangeFlags |= FADER_DRAG_CHANGED;
	}
	if ( ctrlKeyChanged )
	{
		m_nFaderChangeFlags |= FADER_CTRLKEY_CHANGED;
	}
	if ( faderAmountChanged )
	{
		m_nFaderChangeFlags |= FADER_AMOUNT_CHANGED;
	}
	if ( bPresetChanged )
	{
		m_nFaderChangeFlags |= FADER_PRESET_CHANGED;
	}

	m_PreviousPreviewFader = fader.name;
	m_Previous = fader;

	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		slider->EnablePreview( false, false, false );
		if ( !slider->IsVisible() )
			continue;

		if ( !slider->GetControl() )
			continue;

		const char *name = slider->GetName();
		Assert( name );

		if ( m_CtrlKeyPreviewSlider.Get() == slider )
		{
			// The preset stuff shouldn't be active when we're holding ctrl over the raw attribute sliders!!!
			Assert( !fader.isbeingdragged );
			m_CtrlKeyPreviewSliderElement = NULL;
			m_CtrlKeyPreviewSlider = NULL;

			AttributeValue_t dest;
			if ( !slider->IsTransform() )
			{
				dest.m_pValue[ ANIM_CONTROL_VALUE ] = m_flEstimatedValue;
				dest.m_pValue[ ANIM_CONTROL_BALANCE ] = slider->GetValue( ANIM_CONTROL_BALANCE );
				dest.m_pValue[ ANIM_CONTROL_MULTILEVEL ] = slider->GetValue( ANIM_CONTROL_MULTILEVEL );
			}
			else
			{
				dest.m_pValue[ ANIM_CONTROL_VALUE ] = 0.0f;
				dest.m_pValue[ ANIM_CONTROL_BALANCE ] = 0.5f;
				dest.m_pValue[ ANIM_CONTROL_MULTILEVEL ] = 0.5f;
			}

			// If we aren't over any of the preset sliders, then we need to be able to ramp down to the current value, too
			slider->EnablePreview( true, false, false );
			slider->SetPreview( dest, dest, true, true );
			continue;
		}

		if ( !fader.values )
			continue;

		AttributeValue_t preview;
		preview.m_pValue[ ANIM_CONTROL_VALUE ] = slider->IsTransform() ? 0.0f : slider->GetControlDefaultValue( ANIM_CONTROL_VALUE );
		preview.m_pValue[ ANIM_CONTROL_BALANCE ] = 0.5f;
		preview.m_pValue[ ANIM_CONTROL_MULTILEVEL ] = 0.5f;

		AttributeValue_t current = slider->GetValue();

		int idx = fader.values->Find( name );
		if ( idx != fader.values->InvalidIndex() )
		{
			preview = (*fader.values)[ idx ];
		}
		BlendFlexValues( &preview, current, preview, 1.0f, slider->IsControlActive( ANIM_CONTROL_BALANCE ) ? m_pPresetSideFilter->GetPos() : 0.5f );

		bool simple = fader.isbeingdragged || ( !fader.holdingctrl && !slider->IsRampingTowardPreview() && !ctrlKeyChanged );

		slider->EnablePreview( true, simple, fader.isbeingdragged );
		if ( bFaderChanged || fader.isbeingdragged )
		{
			// If being dragged, slam to current value right away
			if ( simple )
			{
				slider->SetPreview( preview, preview, true, true );
			}
			else
			{
				// For the "ramp down" case (just released ctrl key), we need the original values instead of the preview valuees
				if ( ctrlKeyChanged && !fader.holdingctrl && slider->IsRampingTowardPreview() )
				{
					Assert( !fader.isbeingdragged );
					slider->RampDown();
				}
				else
				{
					// Apply the left-right balance to the target
					AttributeValue_t dest;
					BlendFlexValues( &dest, current, preview, fader.amount );
					slider->SetPreview( dest, preview, !fader.holdingctrl, !nameChanged );
				}
			}
		}

		if ( faderAmountChanged || fader.isbeingdragged || fader.holdingctrl )
		{
			slider->UpdateFaderAmount( fader.amount );
		}
	}

	UpdatePreviewSliderTimes();

	ApplySliderValues( false );

	PerformRecomputePreview();
}

void CBaseAnimSetAttributeSliderPanel::ChangeAnimationSet( CDmeAnimationSet *newAnimSet )
{
	int i;
	// Force recomputation
	m_nActiveControlSetMode = -1;

	bool rebuild = true;

	if ( m_AnimSet.Get() &&	newAnimSet == m_AnimSet )
	{
		// See if every slider is the same as before
		const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();
		int controlCount = controls.Count();
		if ( m_SliderList.Count() == controlCount )
		{
			rebuild = false;
			for ( i = 0 ; i < controlCount; ++i )
			{
				CDmElement *control = controls[ i ];
				if ( !control )
					continue;

				if ( Q_stricmp( control->GetName(), m_SliderList[ i ]->GetName() ) )
				{
					rebuild = true;
					break;
				}

				bool bStereo =  control->GetValue< bool >( "combo" );
				bool bIsMulti =  control->GetValue< bool >( "multi" );
				if ( m_SliderList[ i ]->IsControlActive( ANIM_CONTROL_BALANCE ) != bStereo || 
					 m_SliderList[ i ]->IsControlActive( ANIM_CONTROL_MULTILEVEL ) != bIsMulti )
				{
					rebuild = true;
					break;
				}
			}
		}
	}

	m_AnimSet = newAnimSet;

	if ( !m_AnimSet.Get() )
		return;

	if ( !rebuild )
		return;

	int c = m_SliderList.Count();
	for ( i = 0 ; i < c; ++i )
	{
		delete m_SliderList[ i ];
	}
	m_SliderList.RemoveAll();
	m_Sliders->RemoveAll();
	m_ChannelToSliderLookup.Purge();

	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	// Now create sliders for all known controls, nothing visible by default
	int controlCount = controls.Count();
	for ( i = 0 ; i < controlCount; ++i )
	{
		CDmElement *control = controls[ i ];
		if ( !control )
			continue;

		CAttributeSlider *slider = new CAttributeSlider( this, control->GetName(), control );

		slider->SetVisible( false );
		slider->SetValue( ANIM_CONTROL_VALUE, control->GetValue< float >( "value" ) );
		slider->SetSize( 100, 20 );

		bool bStereo = control->GetValue< bool >( "combo" );
		slider->ActivateControl( ANIM_CONTROL_BALANCE, bStereo );
		if ( bStereo )
		{
			slider->SetValue( ANIM_CONTROL_BALANCE, control->GetValue< float >( "balance" ) );
		}

		bool bMulti = control->GetValue< bool >( "multi" );
		slider->ActivateControl( ANIM_CONTROL_MULTILEVEL, bMulti );
		if ( bMulti )
		{
			slider->SetValue( ANIM_CONTROL_MULTILEVEL, control->GetValue< float >( "multilevel" ) );
		}
		m_SliderList.AddToTail( slider );

		ChannelToSliderLookup_t lookup;
		lookup.slider	= control;

		CDmeChannel *ctrlChannels[ LOG_PREVIEW_MAX_CHANNEL_COUNT ];
		GetChannelsForControl( control, ctrlChannels );
		for ( int j = 0 ; j < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++j )
		{
			lookup.ch = ctrlChannels[ j ];
			lookup.type	= (AnimationControlType_t)j;
			if ( lookup.ch )
			{
				Assert( m_ChannelToSliderLookup.Find( lookup) == m_ChannelToSliderLookup.InvalidIndex() );
				m_ChannelToSliderLookup.Insert( lookup );
			}
		}
	}

	RecomputePreview();
}

void CBaseAnimSetAttributeSliderPanel::SetVisibleControlsForSelectionGroup( CUtlSymbolTable& visible )
{
	int i, c;

	bool changed = false;

	// Walk through all sliders and show only those in the symbol table
	c = m_SliderList.Count();
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		const char *sliderName = slider->GetName();

		bool showSlider = visible.Find( sliderName ) != UTL_INVAL_SYMBOL ? true : false;
		if ( slider->IsVisible() != showSlider )
		{
			changed = true;
			break;
		}
	}

	if ( !changed )
		return;

	m_Sliders->RemoveAll();
	c = m_SliderList.Count();
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		const char *sliderName = slider->GetName();

		if ( visible.Find( sliderName ) != UTL_INVAL_SYMBOL )
		{
			slider->SetVisible( true );
			m_Sliders->AddItem( NULL, slider );
		}
		else
		{
			slider->SetVisible( false );
		}
	}

	// Force mode to recompute
	m_nActiveControlSetMode = -1;
	RecomputePreview();
}


void CBaseAnimSetAttributeSliderPanel::ApplyPreset( float flScale, AttributeDict_t& values )
{
	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( !slider || !slider->IsVisible() )
			continue;

		if ( slider->IsTransform() )
			continue;

		AttributeValue_t current, target;
		current = slider->GetValue( );

		target.m_pValue[ ANIM_CONTROL_VALUE ] = slider->GetControlDefaultValue( ANIM_CONTROL_VALUE );
		target.m_pValue[ ANIM_CONTROL_BALANCE ] = 0.5f;
		target.m_pValue[ ANIM_CONTROL_MULTILEVEL ] = 0.5f;

		const char *name = slider->GetName();
		int idx = name ? values.Find( name ) : values.InvalidIndex();
		if ( idx != values.InvalidIndex() )
		{
			target = values[ idx ];
		}

		// Apply the left-right balance to the target
		AttributeValue_t blend;
		BlendFlexValues( &blend, current, target, flScale, slider->IsControlActive( ANIM_CONTROL_BALANCE ) ? m_pPresetSideFilter->GetPos() : 0.5f );
		slider->SetValue( blend );
	}
}

static const char *s_pAnimControlAttribute[ANIM_CONTROL_COUNT] = 
{
	"value", "balance", "multilevel"
};

bool CBaseAnimSetAttributeSliderPanel::ApplySliderValues( bool bForce )
{
	if ( !m_AnimSet.Get() )
		return false;

	if ( !bForce )
	{
		bForce = m_Previous.isbeingdragged;
	}

	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	bool valuesChanged = false;

	CDisableUndoScopeGuard guard;

	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *pSlider = m_SliderList[ i ];
		if ( !pSlider->IsVisible() )
			continue;

		CDmElement *pControl = controls[ i ];
		if ( !pControl )
			continue;

		// Skip these types of sliders...
		if ( pControl->GetValue< bool >( "transform" ) )
			continue;

		CDmeChannel *ctrlChannels[ LOG_PREVIEW_MAX_CHANNEL_COUNT ];
		GetChannelsForControl( pControl, ctrlChannels );

		bool bUsePreviewValue = pSlider->IsPreviewEnabled() && ( !pSlider->IsSimplePreview() || bForce );

		for ( int j = 0; j < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++j )
		{
			AnimationControlType_t type = (AnimationControlType_t)j;
			CDmeChannel *pChannel = ctrlChannels[ j ];

			// Figure out what to do based on the channel's mode
			ChannelMode_t mode = pChannel ? pChannel->GetMode() : CM_PASS;
			bool bPushSlidersIntoScene = ( mode == CM_PASS || mode == CM_RECORD );
			bool bPullSlidersFromScene = ( mode == CM_PLAY );

			if ( bPullSlidersFromScene )
			{
				Assert( pChannel );

				// If it's actively being manipulated, the UI will be up to date
				if ( pSlider->GetDragControl() == type )
					continue;

				// If we're dragging value, we're now going to be changing balance as well
				if ( pSlider->GetDragControl() == ANIM_CONTROL_VALUE && type == ANIM_CONTROL_BALANCE )
					continue;

				// Drive value setting based on the output data
				// NOTE: GetCurrentPlaybackValue might not overwrite flValue.
				float flValue = pSlider->GetControlDefaultValue( type );
				pChannel->GetCurrentPlaybackValue< float >( flValue );
				pSlider->SetValue( type, flValue );
				pControl->SetValue< float >( s_pAnimControlAttribute[type], flValue );
			}
			else if ( bPushSlidersIntoScene )
			{
				float flValue = bUsePreviewValue ? pSlider->GetPreview( type ) : pSlider->GetValue( type );
				if ( pControl->GetValue< float >( s_pAnimControlAttribute[type] ) != flValue || bForce )
				{
					valuesChanged = true;
					pControl->SetValue< float >( s_pAnimControlAttribute[type], flValue );
				}
			}
		}
	}

	guard.Release();

	return valuesChanged;
}

void CBaseAnimSetAttributeSliderPanel::UpdatePreviewSliderTimes()
{
	if ( !m_AnimSet.Get() )
		return;

	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	float curtime = system()->GetFrameTime();
	float dt = clamp( curtime - m_flPrevTime, 0.0f, 0.1f );
	m_flPrevTime = curtime;

	dt *= ifm_fader_timescale;

	bool previewing = false;
	bool changingvalues = false;

	bool ctrlDown = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	int mx, my;
	input()->GetCursorPos( mx, my );
	if ( ctrlDown )
	{
		bool bInside = m_Sliders->IsWithin( mx, my );
		if ( !bInside )
		{
			ctrlDown = false;
		}

		VPANEL topMost = input()->GetMouseOver();
		if ( topMost && !ipanel()->HasParent( topMost, GetVPanel() ) )
		{
			ctrlDown = false;
		}
	}

	CAttributeSlider *dragSlider = NULL;
	CDmElement *dragSliderElement = NULL;

	m_CtrlKeyPreviewSliderElement = NULL;
	m_CtrlKeyPreviewSlider = NULL;
	m_flEstimatedValue = 0.0f;

	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		slider->UpdateTime( dt );

		if ( !slider->IsVisible() )
			continue;

		bool ctrlDownOnThisSlider = false;
		if ( ctrlDown )
		{
			int x, y;
			x = mx;
			y = my;
			slider->ScreenToLocal( x, y );

			int sw, st;
			slider->GetSize( sw, st );
			if ( x >= 0 && x < sw && y >= 0 && y < st )
			{
				// Should only hit one slider!!!
				Assert( !ctrlDownOnThisSlider );
				ctrlDownOnThisSlider = true;
				if ( !slider->IsTransform() )
				{
					m_flEstimatedValue = slider->EstimateValueAtPos( x, y );
				}
			}
		}

		if ( slider->IsPreviewEnabled() )
		{
			if ( !slider->IsSimplePreview() )
			{
				previewing = true;
			}
		}
		// If a fader is being dragged, or we're directly manipulating the slider, then we are going to put
		//  this slider into record mode
		if ( slider->IsFaderBeingDragged() )
		{
			changingvalues = true;
			Assert( !dragSlider );
			dragSlider = NULL;
			dragSliderElement = NULL;
		}
		else if ( slider->IsDragging() )
		{
			Assert( !dragSlider );
			dragSlider = slider;
			dragSliderElement = controls[ i ];
			changingvalues = true;
		}

		if ( ctrlDownOnThisSlider )
		{
			m_CtrlKeyPreviewSliderElement = controls[ i ];
			m_CtrlKeyPreviewSlider = slider;
		}
	}

	switch ( m_hEditor->GetRecordingState() )
	{
	default:
		Assert( 0 );
		break;
	case AS_OFF:
		{
			ActivateControlSetInMode( CM_OFF, CM_OFF, CM_OFF, NULL );
		}
		break;
	case AS_RECORD:
		{
			// Put one or all things into record mode (if dragging a single slider, other channels should be in playback mode)
			if ( changingvalues || previewing )
			{
				ActivateControlSetInMode( changingvalues ? CM_RECORD : CM_PASS, CM_PLAY, CM_PLAY, dragSlider );
			}
			else
			{
				ActivateControlSetInMode( CM_PLAY, CM_PLAY, CM_PLAY, dragSlider );
			}
		}
		break;
	case AS_PLAYBACK:
		{
			// Put things into playback, except if dragging a slider, to preview
			if ( changingvalues || previewing )
			{
				ActivateControlSetInMode( CM_PASS, CM_PLAY, CM_PLAY, NULL );
			}
			else
			{
				ActivateControlSetInMode( CM_PLAY, CM_PLAY, CM_PLAY, NULL );
			}
		}
		break;
	case AS_PREVIEW:
		{
			// Put things into passthru
			ActivateControlSetInMode( CM_PASS, CM_PASS, CM_PLAY, NULL );
		}
		break;
	}

	if ( dragSliderElement )
	{
		SetLogPreviewControl( dragSliderElement );
	}
	else if ( m_CtrlKeyPreviewSliderElement.Get() )
	{
		SetLogPreviewControl( m_CtrlKeyPreviewSliderElement );
	}
}

bool CBaseAnimSetAttributeSliderPanel::GetAttributeSliderValue( AttributeValue_t *pValue, const char *name )
{
	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( Q_stricmp( slider->GetName(), name ) )
			continue;

		*pValue = slider->GetValue( );
		return true;
	}
	return false;
}

void CBaseAnimSetAttributeSliderPanel::GetChannelsForControl( CDmElement *control, CDmeChannel *channels[LOG_PREVIEW_MAX_CHANNEL_COUNT] )
{
	if ( control->GetValue< bool >( "transform" ) )
	{
		CDmeChannel *ch1 = control->GetValueElement< CDmeChannel >( "position" );
		CDmeChannel *ch2 = control->GetValueElement< CDmeChannel >( "orientation" );

		channels[ LOG_PREVIEW_POSITION ] = ch1;
		channels[ LOG_PREVIEW_ORIENTATION ] = ch2;
		for ( int i = 2; i < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++i )
		{
			channels[i] = NULL;
		}
		return;
	}

	if ( control->GetValue< bool >( "combo" ) )
	{
		channels[ LOG_PREVIEW_VALUE ] = control->GetValueElement< CDmeChannel >( "valuechannel" );
		channels[ LOG_PREVIEW_BALANCE ] = control->GetValueElement< CDmeChannel >( "balancechannel" );
	}
	else
	{
		channels[ LOG_PREVIEW_VALUE ] = control->GetValueElement< CDmeChannel >( "channel" );
		channels[ LOG_PREVIEW_BALANCE ] = 0;
	}

	if ( control->GetValue< bool >( "multi" ) )
	{
		channels[ LOG_PREVIEW_MULTILEVEL ] = control->GetValueElement< CDmeChannel >( "multilevelchannel" );
	}
	else
	{
		channels[ LOG_PREVIEW_MULTILEVEL ] = NULL;
	}
	for ( int i = 3; i < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++i )
	{
		channels[i] = NULL;
	}
}

void CBaseAnimSetAttributeSliderPanel::GetActiveTimeSelectionParams( DmeLog_TimeSelection_t& params )
{
	Assert( 0 );
}

void CBaseAnimSetAttributeSliderPanel::ActivateControlSetInMode( int mode, int otherChannelsMode, int hiddenChannelsMode, CAttributeSlider *whichSlider /*= NULL*/ )
{
	int i, c;

	if ( !m_AnimSet.Get() )
		return;

	bool updateChannelsModeChanged = m_nActiveControlSetMode != mode ? true : false;
	if ( !updateChannelsModeChanged )
		return;

	int previousMode = m_nActiveControlSetMode;
	m_nActiveControlSetMode = mode;

	if ( previousMode == CM_RECORD )
	{
		DmeLog_TimeSelection_t	params;
		GetActiveTimeSelectionParams( params );

		// Finalize any previously recording layers!!!
		g_pChannelRecordingMgr->FinishLayerRecording( params.m_flThreshold, true );
	}

	ChannelMode_t channelMode = ( ChannelMode_t )mode;

	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	// Build the list of channels to alter
	CUtlVector< CDmeChannel * >	updateChannels;
	CUtlVector< CDmeChannel * >	otherChannels;
	CUtlVector< CDmeChannel * >	hiddenChannels;

	CDisableUndoScopeGuard guard;

	c = m_SliderList.Count();
	int j;
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		CDmElement *ctrl = controls[ i ];
		if ( !ctrl )
			continue;

		CDmeChannel *ctrlChannels[ LOG_PREVIEW_MAX_CHANNEL_COUNT ];
		GetChannelsForControl( ctrl, ctrlChannels );

		for ( j = 0 ; j < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++j )
		{								    
			if ( !ctrlChannels[ j ] )
				continue;

			CUtlVector< CDmeChannel * > *useArray = NULL;;

			// now characterize it
			bool hidden = !slider->IsVisible();
			if ( hidden )
			{
				useArray = &hiddenChannels;
			}
			else
			{
				if ( !whichSlider )
				{
					useArray = &updateChannels;
				}
				else if ( slider == whichSlider )
				{
					// this causes value and balance to get lumped together, since we're now dragging left/right values
					if ( slider->GetDragControl() == ANIM_CONTROL_MULTILEVEL )
					{
						useArray = ( j == LOG_PREVIEW_MULTILEVEL ) ? &updateChannels : &otherChannels;
					}
					else
					{
						useArray = ( j != LOG_PREVIEW_MULTILEVEL ) ? &updateChannels : &otherChannels;
					}
				}
				else
				{
					useArray = &otherChannels;
				}
			}

			useArray->AddToTail( ctrlChannels[ j ] );
		}
	}

	guard.Release();

	c = updateChannels.Count();
	if ( c > 0 )
	{
		if ( channelMode == CM_RECORD )
		{
			DmeLog_TimeSelection_t	params;
			GetActiveTimeSelectionParams( params );
			params.SetRecordingMode( ( NULL == whichSlider ) ? RECORD_PRESET : RECORD_ATTRIBUTESLIDER );

			g_pChannelRecordingMgr->StartLayerRecording( "Dragging Sliders", &params );

			for ( i = 0; i < c; ++i )
			{
				// THIS PUTS THEM INTO CM_RECORD
				g_pChannelRecordingMgr->AddChannelToRecordingLayer( updateChannels[ i ], GetCurrentMovie(), GetCurrentShot() );
			}

			SetTimeSelectionParametersForRecordingChannels( ( NULL == whichSlider ) ? m_Previous.amount : 1.0f );
		}
		else
		{
			// Don't create undo records for this
			CDisableUndoScopeGuard guardSet;
			for ( i = 0; i < c; ++i )
			{
				updateChannels[ i ]->SetMode( channelMode );
			}
		}
	}

	c = otherChannels.Count();
	if ( c > 0 )
	{
		// Don't create undo records for this
		CDisableUndoScopeGuard guardRecord;

		for ( i = 0; i < c; ++i )
		{
			// These should never go into record!!!
			Assert( (ChannelMode_t)otherChannelsMode != CM_RECORD );
			otherChannels[ i ]->SetMode( (ChannelMode_t)otherChannelsMode );
		}
	}

	c = hiddenChannels.Count();
	if ( c > 0 )
	{
		// Don't create undo records for this
		CDisableUndoScopeGuard guardRecord;

		// For now these should only be able to go into Playback
		//Assert( (ChannelMode_t)hiddenChannelsMode == CM_PLAY );

		for ( i = 0; i < c; ++i )
		{
			hiddenChannels[ i ]->SetMode( (ChannelMode_t)hiddenChannelsMode );
		}
	}
}

static const char *s_pDefaultAttributeName[ANIM_CONTROL_COUNT] = 
{
	"defaultValue", "defaultBalance", "defaultMultilevel"
};

void CBaseAnimSetAttributeSliderPanel::SetupForPreset( FaderPreview_t &fader, int nChangeFlags )
{
	// Nothing special here
}

float CBaseAnimSetAttributeSliderPanel::GetBalanceSliderValue()
{
	return m_pPresetSideFilter->GetPos();
}

void CBaseAnimSetAttributeSliderPanel::SetTimeSelectionParametersForRecordingChannels( float flIntensity )
{
	CBaseAnimSetPresetFaderPanel *pPresets = m_hEditor->GetPresetFader();
	if ( !pPresets )
	{
		Assert( 0 );
		return;
	}

	FaderPreview_t fader;
	pPresets->GetPreviewFader( fader );

	SetupForPreset( fader, m_nFaderChangeFlags );

	int c = g_pChannelRecordingMgr->GetLayerRecordingChannelCount();
	for ( int i = 0; i < c; ++i )
	{
		CDmeChannel *ch = g_pChannelRecordingMgr->GetLayerRecordingChannel( i );
		if ( !ch )
			continue;

		CDmAttribute *pPreset = NULL;

		ChannelToSliderLookup_t search;
		search.ch = ch;
		unsigned int idx = m_ChannelToSliderLookup.Find( search );
		if ( idx != m_ChannelToSliderLookup.InvalidIndex() && fader.values )
		{
			ChannelToSliderLookup_t &item = m_ChannelToSliderLookup[ idx ];

			CDmElement *pControl = item.slider;
			Assert( pControl );

			int faderIndex = fader.values->Find( pControl->GetName() );
			if ( faderIndex != fader.values->InvalidIndex() )
			{
				pPreset = (*fader.values)[ faderIndex ].m_pAttribute[ item.type ];
			}
			if ( !pPreset )
			{
				pPreset = pControl->GetAttribute( s_pDefaultAttributeName[item.type] );
			}
		}

		g_pChannelRecordingMgr->SetPresetValue( ch, pPreset );
	}
}

static bool CanPreviewType( DmAttributeType_t attType )
{
	switch ( attType )
	{
	default:
		return false;
	case AT_FLOAT:
	case AT_VECTOR3:
	case AT_QUATERNION:
		break;
	}
	return true;
}

void CBaseAnimSetAttributeSliderPanel::MaybeAddPreviewLog( CDmeFilmClip *shot, CUtlVector< LogPreview_t >& list, CDmElement *control, bool bDragging, bool isActiveLog, bool bSelected )
{
	CDmeChannel *ctrlChannels[ LOG_PREVIEW_MAX_CHANNEL_COUNT ];
	GetChannelsForControl( control, ctrlChannels );

	LogPreview_t preview;

	preview.m_bDragging = bDragging;
	preview.m_bActiveLog = isActiveLog;
	preview.m_bSelected = bSelected;

	for ( int channel = 0; channel < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++channel )
	{
		CDmeChannel *ch = ctrlChannels[ channel ];
		if ( !ch )
			continue;

		CDmeLog *log = ch->GetLog();
		if ( !log )
			continue;

		DmAttributeType_t attType = log->GetDataType();
		if ( !CanPreviewType( attType ) )
			continue;

		Assert( control );

		preview.m_hControl = control;
		preview.m_hShot	= shot;
		preview.m_hChannels[ channel ] = ch;
		preview.m_hOwner = ch->FindOwnerClipForChannel( shot );
	}

	list.AddToTail( preview );
}

void CBaseAnimSetAttributeSliderPanel::RecomputePreview()
{
	float curtime = system()->GetFrameTime();
	m_flRecomputePreviewTime = curtime + RECOMPUTE_PREVIEW_INTERVAL;
	m_bRequestedNewPreview = true;
}

CDmeFilmClip *CBaseAnimSetAttributeSliderPanel::GetCurrentShot()
{
	return NULL;
}

CDmeFilmClip *CBaseAnimSetAttributeSliderPanel::GetCurrentMovie()
{
	return NULL;
}

void CBaseAnimSetAttributeSliderPanel::PerformRecomputePreview()
{
	m_CurrentPreview.RemoveAll();
	if ( !m_bRequestedNewPreview )
		return;

#if 0
	// Tracker 54528:  While this saves recomputing things all of the time, the delay is annoying so 
	//  we'll turn it off for now
	float curtime = system()->GetFrameTime();
	if ( curtime < m_flRecomputePreviewTime )
		return;
#endif

	m_bRequestedNewPreview = false;
	m_flRecomputePreviewTime = -1.0f;
	// list of bones/root transforms which are in the control set
	m_ActiveTransforms.Purge(); 
	if ( !m_AnimSet.Get() )
		return;

	CDmeFilmClip *shot = GetCurrentShot();
	CDmElement *previewControl = GetLogPreviewControl();
	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	int c = m_SliderList.Count();
	int i;
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( !slider->IsVisible() )
			continue;
		CDmElement *control = controls[ i ];

		MaybeAddPreviewLog( shot, m_ActiveTransforms, control, slider->IsDragging(), false, slider->IsSelected() );
		MaybeAddPreviewLog( shot, m_CurrentPreview, control, slider->IsDragging(), ( control == previewControl ), slider->IsSelected() );
	}
}

CUtlVector< LogPreview_t >* CBaseAnimSetAttributeSliderPanel::GetActiveTransforms()
{
	return &m_ActiveTransforms;
}

CDmElement *CBaseAnimSetAttributeSliderPanel::GetElementFromSlider( CAttributeSlider *pSlider )
{
	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	int c = m_SliderList.Count();
	int i;
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( slider != pSlider )
			continue;

		CDmElement *control = controls[ i ];
		return control;
	}
	return NULL;
}

CDmElement *CBaseAnimSetAttributeSliderPanel::GetLogPreviewControl()
{
	return m_PreviewControl.Get();
}

void CBaseAnimSetAttributeSliderPanel::SetLogPreviewControlFromSlider( CAttributeSlider *pSlider )
{
	CDmElement *control = GetElementFromSlider( pSlider );
	// Note can be NULL
	SetLogPreviewControl( control );
}

void CBaseAnimSetAttributeSliderPanel::SetLogPreviewControl( CDmElement *control )
{
	if ( !m_hEditor.Get() )
		return;

	bool changed = m_PreviewControl != control ? true : false;
	m_PreviewControl = control;
	if ( changed )
	{
		const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

		int itemNumber = 0;
		int nSliders = m_SliderList.Count();
		for ( int i = 0; i < nSliders; ++i )
		{
			CAttributeSlider *slider = m_SliderList[ i ];
			slider->SetIsLogPreviewControl( false );
			if ( !slider->IsVisible() )
				continue;

			CDmElement *c = controls[ i ];
			if ( c == control )
			{
				slider->SetIsLogPreviewControl( true );
				slider->RequestFocus();
				m_Sliders->ScrollToItem( itemNumber );
			}

			++itemNumber;
		}

		RecomputePreview();
	}
}


void CBaseAnimSetAttributeSliderPanel::GetVisibleControls( CUtlVector< VisItem_t>& list )
{
	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	int i, c;
	c = m_SliderList.Count();
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( !slider->IsVisible() )
			continue;

		CDmElement *ctrl = controls[ i ];
		if ( !ctrl )
			continue;

		VisItem_t item;
		item.element = ctrl;
		item.selected = slider->IsSelected();
		item.index = i;

		list.AddToTail( item );
	}
}

int CBaseAnimSetAttributeSliderPanel::BuildVisibleControlList( CUtlVector< LogPreview_t >& list )
{
	if ( !m_AnimSet.Get() )
		return 0;

	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	int i, c;
	c = m_SliderList.Count();
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( !slider->IsVisible() )
			continue;

		CDmElement *ctrl = controls[ i ];
		if ( !ctrl )
			continue;

		MaybeAddPreviewLog( NULL, list, ctrl, slider->IsDragging(), false, slider->IsSelected() );
	}

	return list.Count();
}

int CBaseAnimSetAttributeSliderPanel::BuildFullControlList( CUtlVector< LogPreview_t >& list )
{
	if ( !m_AnimSet.Get() )
		return 0;

	const CDmaElementArray< CDmElement > &controls = m_AnimSet->GetControls();

	int i, c;
	c = m_SliderList.Count();
	for ( i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		CDmElement *ctrl = controls[ i ];
		if ( !ctrl )
			continue;

		MaybeAddPreviewLog( NULL, list, ctrl, slider->IsDragging(), false, slider->IsSelected() );
	}

	return list.Count();
}




void SpewLayer( const char *desc, CDmeTypedLogLayer< float > *l )
{
	Msg( "%s\n", desc );

	int c = l->GetKeyCount();
	for ( int i = 0; i < c; ++i )
	{
		DmeTime_t kt = l->GetKeyTime( i );
		float v = l->GetKeyValue( i );

		Msg( "%d %.3f = %f\n", i, kt.GetSeconds(), v );

		if ( i > 20 )
			break;

	}
}

void CBaseAnimSetAttributeSliderPanel::MoveToSlider( CAttributeSlider *pCurrentSlider, int nDirection )
{
	Assert( pCurrentSlider );
	if ( !pCurrentSlider )
		return;

	// Find current slider index and then move to next / previous one
	CUtlVector< CAttributeSlider * > visible;

	int c = m_SliderList.Count();
	if ( c <= 1 )
		return;

	int i;
	for ( i = 0; i < c; ++i )
	{
		if ( !m_SliderList[ i ]->IsVisible() )
			continue;
		visible.AddToTail( m_SliderList[ i ] );
	}

	c = visible.Count();

	if ( c <= 1 )
		return;

	for ( i = 0; i < c; ++i )
	{
		if ( visible[ i ] != pCurrentSlider )
			continue;

		Msg( "Found slider at %d (old %s)\n", i, pCurrentSlider->GetName() );

		i += nDirection;
		if ( i < 0 )
		{
			i = c - 1;
		}
		else if ( i >= c )
		{
			i = 0;
		}

		Msg( "Change to slider %d %s\n", i, visible[ i ]->GetName() );

		// m_SliderList[ i ]->RequestFocus();
		SetLogPreviewControl( visible[ i ]->GetControl() );
		break;
	}
}

void CBaseAnimSetAttributeSliderPanel::OnKBDeselectAll()
{
	ClearSelectedControls();
}

void CBaseAnimSetAttributeSliderPanel::ClearSelectedControls()
{
	int c = m_SliderList.Count();
	int i;
	for ( i = 0; i < c; ++i )
	{
		m_SliderList[ i ]->SetSelected( false );
	}
	RecomputePreview();
}

void CBaseAnimSetAttributeSliderPanel::SetControlSelected( CAttributeSlider *slider, bool state )
{
	slider->SetSelected( state);
	RecomputePreview();
}

void CBaseAnimSetAttributeSliderPanel::SetControlSelected( CDmElement *control, bool state )
{
	CAttributeSlider *slider = FindSliderForControl( control );
	if ( !slider )
		return;
	SetControlSelected( slider, state );
}

CAttributeSlider *CBaseAnimSetAttributeSliderPanel::FindSliderForControl( CDmElement *control )
{
	int c = m_SliderList.Count();
	int i;
	for ( i = 0; i < c; ++i )
	{
		if ( m_SliderList[ i ]->GetControl() == control )
			return m_SliderList[ i ];
	}

	return NULL;
}

bool CBaseAnimSetAttributeSliderPanel::GetSliderValues( AttributeValue_t *pValue, int nIndex )
{
	Assert( pValue );
	Assert( nIndex >= 0 && nIndex < m_SliderList.Count() );

	CAttributeSlider *pSlider = m_SliderList[ nIndex ];
	bool bForce = m_Previous.isbeingdragged;
	bool bGetPreview = ( pSlider->IsPreviewEnabled() && ( !pSlider->IsSimplePreview() || bForce ) );
	*pValue = bGetPreview ? pSlider->GetPreview() : pSlider->GetValue();
	return pSlider->IsVisible();
}