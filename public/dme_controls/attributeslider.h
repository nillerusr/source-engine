//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ATTRIBUTESLIDER_H
#define ATTRIBUTESLIDER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "dme_controls/AnimSetAttributeValue.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "datamodel/dmehandle.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimSetAttributeSliderPanel;
class CDmElement;
class CAttributeSliderTextEntry;
class CSubRectImage;


//-----------------------------------------------------------------------------
// CAttributeSlider
//-----------------------------------------------------------------------------

// THIS CODE IS KIND OF A MESS WRT THE VARIOUS STATES WE CAN BE IN:
// we can be driven by the preset pane or by dragging on any individual control
// we can also be driven by ctrl hovering over the preset pane or an individual control
// if we move from control to control in the preset or here, we need to be able to decay into/out of the various individual sliders
class CAttributeSlider : public EditablePanel 
{
	DECLARE_CLASS_SIMPLE( CAttributeSlider, EditablePanel );

	// Overridden methods of EditablePanel
public:
	virtual void Paint();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnCursorMoved(int x, int y);
	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);
	virtual void OnMouseReleased(MouseCode code);
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void OnKeyCodeTyped( KeyCode code );

	// Other public methods
public:
	CAttributeSlider( CBaseAnimSetAttributeSliderPanel *parent, const char *panelName, CDmElement *control );
	virtual ~CAttributeSlider();

	// Returns the control we're modifying
	CDmElement *GetControl();

	// Activates/deactivates a slider control
	// NOTE: Slider control 'value' defaults to active, 'balance' and 'multilevel' defaults to inactive
	void ActivateControl( AnimationControlType_t type, bool bActive );
	bool IsControlActive( AnimationControlType_t type );

	// Gets/sets the slider value. 
	// NOTE: This may not match the value pushed into the control because of fading
	void SetValue( AnimationControlType_t type, float flValue );
	float GetValue( AnimationControlType_t type ) const;
	void SetValue( const AttributeValue_t& value );
	const AttributeValue_t& GetValue() const;

	// Is this slider manipulating a transform control? 
	// [NOTE: This is a utility method; the control contains these states]
	bool IsTransform() const;

	// Returns the default value for a control
	// [NOTE: These is a utility method; the control contains these states]
	float GetControlDefaultValue( AnimationControlType_t type ) const;

	// Are we dragging? If so, what control is being dragged?
	bool IsDragging() const;
	AnimationControlType_t GetDragControl() const;

	// Are we in text entry mode? If so, what control is having text entered?
	bool IsInTextEntry() const;
	AnimationControlType_t GetTextEntryControl() const;

	// Estimates the value of the control given a local coordinate
	float EstimateValueAtPos( int nLocalX, int nLocalY ) const;

	void		SetPreview( const AttributeValue_t &value, const AttributeValue_t &full, bool instantaneous, bool startfromcurrent );
	float		GetPreview( AnimationControlType_t type ) const;
	const AttributeValue_t &GetPreview() const;

	void		EnablePreview( bool state, bool simple, bool faderdrag );
	bool		IsPreviewEnabled() const;
	bool		IsSimplePreview() const; 

	void		UpdateTime( float dt );
	void		UpdateFaderAmount( float amount );

	bool		IsRampingTowardPreview() const;
	void		RampDown();

	bool		IsFaderBeingDragged();

	void		SetIsLogPreviewControl( bool state );

	void		SetSelected( bool state );
	bool		IsSelected() const;

private:
	// Various slider modes
	enum SliderMode_t
	{
		SLIDER_MODE_FIRST_DRAG_MODE = 0x0,
		SLIDER_MODE_FIRST_TEXT_MODE = 0x4,
		SLIDER_MODE_LAST_DRAG_MODE = SLIDER_MODE_FIRST_DRAG_MODE + ANIM_CONTROL_COUNT - 1,
		SLIDER_MODE_LAST_TEXT_MODE = SLIDER_MODE_FIRST_TEXT_MODE + ANIM_CONTROL_COUNT - 1,

		SLIDER_MODE_NONE = -1,

		SLIDER_MODE_DRAG_VALUE = SLIDER_MODE_FIRST_DRAG_MODE + ANIM_CONTROL_VALUE,
		SLIDER_MODE_DRAG_BALANCE = SLIDER_MODE_FIRST_DRAG_MODE + ANIM_CONTROL_BALANCE,
		SLIDER_MODE_DRAG_MULTILEVEL = SLIDER_MODE_FIRST_DRAG_MODE + ANIM_CONTROL_MULTILEVEL,

		SLIDER_MODE_TEXT_VALUE = SLIDER_MODE_FIRST_TEXT_MODE + ANIM_CONTROL_VALUE,
		SLIDER_MODE_TEXT_BALANCE = SLIDER_MODE_FIRST_TEXT_MODE + ANIM_CONTROL_BALANCE,
		SLIDER_MODE_TEXT_MULTILEVEL = SLIDER_MODE_FIRST_TEXT_MODE + ANIM_CONTROL_MULTILEVEL,
	};

	struct Preview_t
	{
		AttributeValue_t m_Current;
		AttributeValue_t m_Full;
	};

private:
	// Returns the location of a particular control
	void GetControlRect( Rect_t *pRect, AnimationControlType_t type ) const;

	// Given a mouse position in (x,y) in local coordinates, which animation control is it over?
	AnimationControlType_t DetermineControl( int x, int y );

	// Draws a tick on a circular control
	void DrawCircularTick( const Color& clr, float flValue, int nCenterX, int nCenterY, float flRadius );

	// Draws a preview of a circular control
	void DrawCircularPreview( AnimationControlType_t type, bool bMainTick, float flRadius );

	// Paints the a circular control
	void PaintCircularControl( float flValue, const Rect_t& rect );

	// Called by the text entry code to enter the value into the logs
	void StampValueIntoLogs( AnimationControlType_t type, float flValue );

	// Methods related to rendering
	void DrawMidpoint( int x, int ty, int ttall );
	void DrawPreviewTick( bool mainTick );
	void DrawTick( const Color& clr, float frac, int width, int inset );
	void DrawNameLabel();
	void DrawValueLabel( float flValue );
	float GetPreviewAlphaScale() const;

	// Methods related to text entry mode
	void EnterTextEntryMode( AnimationControlType_t type, bool bRelatchValues );
	void AcceptTextEntryValue();
	void DiscardTextEntryValue();

private:
	CBaseAnimSetAttributeSliderPanel *m_pParent;
	TextImage *m_pName;
	TextImage *m_pValues[ 3 ];
	CSubRectImage *m_pCircleImage;	// The background for the balance + multilevel controls

	// This is the control we're modifying
	CDmeHandle< CDmElement > m_hControl;

	// White material used for drawing non-textured things
	CMaterialReference m_pWhite;

	// The current mode of the slider
	SliderMode_t m_SliderMode;

	// The slider value; it may not match the control attribute value due to blending
	AttributeValue_t m_Control;

	// Is the slider control active?
	bool m_bIsControlActive[ANIM_CONTROL_COUNT];

	// Info used when in text entry mode
	AttributeValue_t m_InitialTextEntryValue;
	CAttributeSliderTextEntry *m_pTextField; // if this is a stereo control, then this will be the left text field
	CAttributeSliderTextEntry *m_pRightTextField;

	Preview_t		m_Next;
	Preview_t		m_Previous;
	Preview_t		m_Preview;
	float			m_flPreviewGoalTime;
	float			m_flFaderAmount;

	// Fields used to help with drag
	int				m_nDragStartPosition[2];	// Where was the mouse clicked?
	int				m_nAccum[2];				// What's the total mouse movement during the drag?
	float			m_flDragStartValue;			// What was the value of the slider before the drag started?
	float			m_flDragStartBalance;		// What was the balance of the slider before the drag started?

	bool			m_bCursorInsidePanel : 1;	// Used to 
	bool			m_bRampUp : 1;
	bool			m_bPreviewEnabled : 1;
	bool			m_bSimplePreviewOnly : 1;
	bool			m_bFaderBeingDragged : 1;
	bool			m_bIsLogPreviewControl : 1;
	bool			m_bTransform : 1;
	bool			m_bSelected : 1;

	friend class CAttributeSliderTextEntry;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Returns the control
//-----------------------------------------------------------------------------
inline CDmElement *CAttributeSlider::GetControl()
{
	return m_hControl;
}


//-----------------------------------------------------------------------------
// Returns information about the control
//-----------------------------------------------------------------------------
inline bool CAttributeSlider::IsTransform() const
{
	// NOTE: We may well not wish to cache this off in the constructor.
	// It's done purely for efficiency reasons.
	// Uncomment the line to make it read from the control.
	// return m_hControl->GetValue< bool >( "transform" )
	return m_bTransform;
}


//-----------------------------------------------------------------------------
// Are we dragging?
//-----------------------------------------------------------------------------
inline bool CAttributeSlider::IsDragging() const
{
	COMPILE_TIME_ASSERT( ANIM_CONTROL_COUNT < 4 );
	return ( m_SliderMode >= SLIDER_MODE_FIRST_DRAG_MODE && m_SliderMode <= SLIDER_MODE_LAST_DRAG_MODE ); 
}

inline AnimationControlType_t CAttributeSlider::GetDragControl() const
{
	if ( IsDragging() )
		return (AnimationControlType_t)( m_SliderMode - SLIDER_MODE_FIRST_DRAG_MODE );
	return ANIM_CONTROL_INVALID;
}


//-----------------------------------------------------------------------------
// Are we in text entry mode?
//-----------------------------------------------------------------------------
inline bool CAttributeSlider::IsInTextEntry() const
{
	COMPILE_TIME_ASSERT( ANIM_CONTROL_COUNT < 4 );
	return ( m_SliderMode >= SLIDER_MODE_FIRST_TEXT_MODE && m_SliderMode <= SLIDER_MODE_LAST_TEXT_MODE ); 
}

inline AnimationControlType_t CAttributeSlider::GetTextEntryControl() const
{
	if ( IsInTextEntry() )
		return (AnimationControlType_t)( m_SliderMode - SLIDER_MODE_FIRST_TEXT_MODE );
	return ANIM_CONTROL_INVALID;
}

#endif // ATTRIBUTESLIDER_H
