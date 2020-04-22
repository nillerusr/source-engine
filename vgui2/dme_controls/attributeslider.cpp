//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/attributeslider.h"
#include "materialsystem/imesh.h"
#include "movieobjects/dmeanimationset.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/subrectimage.h"
#include "vgui_controls/CheckButton.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Enums
//-----------------------------------------------------------------------------
#define SLIDER_PIXEL_SPACING 3
#define CIRCULAR_CONTROL_RADIUS 6.0f
#define UNDO_CHAIN_MOUSEWHEEL_ATTRIBUTE_SLIDER 9876
#define FRAC_PER_PIXEL 0.0025f
#define ANIM_SET_ATTRIBUTE_SLIDER_BALANCE_INSET 30
#define ANIM_SET_ATTRIBUTE_SLIDER_LEFT_BORDER 5
#define ANIM_SET_ATTRIBUTE_SLIDER_GRAPH_BUTTON_WIDTH 16
#define ANIM_SET_ATTRIBUTE_SLIDER_MULTILEVEL_INSET 30


static ConVar ifm_attributeslider_sensitivity( "ifm_attributeslider_sensitivity", "3.0", 0 );


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static Color s_TextColor( 200, 200, 200, 192 );
static Color s_TextColorFocus( 208, 143, 40, 192 );

// NOTE: Index with [preview][selected]
static Color s_BarColor[2][2] = 
{
	{ Color( 45, 45, 45, 255 ), Color( 150, 80, 0, 255 ) },
	{ Color( 30, 255, 255, 80 ), Color( 30, 180, 255, 255 ) }
};

static Color s_ZeroColor[2][2] = 
{
	{ Color( 33, 33, 33, 255 ), Color( 0, 255, 255, 60 ) },
	{ Color( 100, 80, 0, 255 ), Color( 0, 180, 255, 255 ) }
};

static Color s_DraggingBarColor( 142, 142, 142, 255 );
static Color s_PreviewTickColor( 255, 164, 8, 255 );
static Color s_OldValueTickColor( 100, 100, 100, 63 );

static Color s_MidpointColor( 115, 115, 115, 255 );

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
// The panel used to do text entry when double-clicking in the slider
//-----------------------------------------------------------------------------
class CAttributeSliderTextEntry : public TextEntry
{
	DECLARE_CLASS_SIMPLE( CAttributeSliderTextEntry, TextEntry );

public:
	CAttributeSliderTextEntry( CAttributeSlider *slider, const char *panelName ) :
		BaseClass( (Panel *)slider, panelName ), m_pSlider( slider )
	{
		Assert( m_pSlider );
	}

	MESSAGE_FUNC_PARAMS( OnKillFocus, "KillFocus", kv );
	virtual void OnMouseWheeled( int delta );

private:
	CAttributeSlider *m_pSlider;
};



//-----------------------------------------------------------------------------
//
// CAttributeSlider begins here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CAttributeSlider::CAttributeSlider( CBaseAnimSetAttributeSliderPanel *parent, const char *panelName, CDmElement *pControl ) :
	BaseClass( (Panel *)parent, panelName ), 
	m_pParent( parent ),
	m_pWhite( NULL ),
	m_bPreviewEnabled( false ),
	m_bSimplePreviewOnly( true ),
	m_bCursorInsidePanel( false ),
	m_flPreviewGoalTime( -1.0f ),
	m_bRampUp( false ),
	m_bFaderBeingDragged( false ),
	m_flFaderAmount( 1.0f ),
	m_bIsLogPreviewControl( false ),
	m_bSelected( false ),
	m_pRightTextField( 0 )
{
	m_SliderMode = SLIDER_MODE_NONE;
	m_hControl = pControl;

	// Cache off control information since this state should never change
	// NOTE: If it ever does, just change the implementations of 
	// IsTransform + GetMidpoint to always read these values from the attributes
	m_bTransform = pControl->GetValue< bool >( "transform" );

	m_nDragStartPosition[ 0 ] = m_nDragStartPosition[ 1 ] = 0;
	m_nAccum[ 0 ] =  m_nAccum[ 1 ] = 0;
	m_flDragStartValue = 1.0f;
	m_flDragStartBalance = 0.5f;

	SetPaintBackgroundEnabled( true );

	m_pName = new TextImage( panelName );
	m_pValues[ 0 ] = new TextImage( "" );
	m_pValues[ 1 ] = new TextImage( "" );
	m_pValues[ 2 ] = new TextImage( "" );

	m_pCircleImage = new CSubRectImage( "tools/ifm/icon_balance", false, 7, 8, 19, 15 );

	// Allocate a white material
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	m_pWhite.Init( "AttributeSlider_White", NULL, pVMTKeyValues );

	SetBgColor( Color( 42, 42, 42, 255 ) );

	m_bIsControlActive[ANIM_CONTROL_VALUE] = true;
	m_bIsControlActive[ANIM_CONTROL_BALANCE] = false;
	m_bIsControlActive[ANIM_CONTROL_MULTILEVEL] = false;

	m_pTextField = new CAttributeSliderTextEntry( this, panelName );
	m_pTextField->SetVisible( false );
	m_pTextField->SetEnabled( false );
	m_pTextField->SelectAllOnFocusAlways( true );

	SetPaintBorderEnabled( false );
}

CAttributeSlider::~CAttributeSlider()
{
	m_pWhite.Shutdown();
	delete m_pCircleImage;
	delete m_pName;
	delete m_pValues[ 0 ];
	delete m_pValues[ 1 ];
	delete m_pValues[ 2 ];
}


//-----------------------------------------------------------------------------
// Scheme
//-----------------------------------------------------------------------------
void CAttributeSlider::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pName->SetFont( scheme->GetFont( "Default" ) );
	m_pName->SetColor( s_TextColor );
	m_pName->ResizeImageToContent();

	m_pValues[ 0 ]->SetColor( s_TextColor );
	m_pValues[ 0 ]->SetFont( scheme->GetFont( "Default" ) );
	m_pValues[ 1 ]->SetColor( s_TextColorFocus );
	m_pValues[ 1 ]->SetFont( scheme->GetFont( "Default" ) );
	m_pValues[ 2 ]->SetColor( s_TextColor );
	m_pValues[ 2 ]->SetFont( scheme->GetFont( "Default" ) );

	m_pCircleImage->SetColor( Color( 255, 255, 255, 255 ) );

	SetBgColor( Color( 42, 42, 42, 255 ) );
	SetFgColor( Color( 194, 120, 0, 255 ) );
}


//-----------------------------------------------------------------------------
// Gets/sets the slider value. 
// NOTE: This may not match the value pushed into the control because of fading
//-----------------------------------------------------------------------------
static const char *s_pChangeMessage[ANIM_CONTROL_COUNT] = 
{
	"SliderMoved",
	"BalanceChanged",
	"MultiLevelChanged",
};

static const char *s_pChangeKeyValue[ANIM_CONTROL_COUNT] = 
{
	"position",
	"balance",
	"level",
};

void CAttributeSlider::ActivateControl( AnimationControlType_t type, bool bActive )
{
	if ( m_bIsControlActive[type] != bActive )
	{
		m_bIsControlActive[type] = bActive;
		if ( bActive )
		{
			PostActionSignal( new KeyValues( s_pChangeMessage[type], s_pChangeKeyValue[type], m_Control.m_pValue[type] ) );
		}
	}
}

bool CAttributeSlider::IsControlActive( AnimationControlType_t type )
{
	return m_bIsControlActive[type];
}

void CAttributeSlider::SetValue( AnimationControlType_t type, float flValue )
{
	if ( m_Control.m_pValue[type] != flValue )
	{
		m_Control.m_pValue[type] = flValue;
		if ( m_bIsControlActive[type] )
		{
			PostActionSignal( new KeyValues( s_pChangeMessage[type], s_pChangeKeyValue[type], flValue ) );
		}
	}
}

void CAttributeSlider::SetValue( const AttributeValue_t& value )
{
	for ( int i = 0; i < ANIM_CONTROL_COUNT; ++i )
	{
		SetValue( (AnimationControlType_t)i, value.m_pValue[i] );
	}
}

float CAttributeSlider::GetValue( AnimationControlType_t type ) const
{
	return m_Control.m_pValue[type];
}

const AttributeValue_t& CAttributeSlider::GetValue() const
{
	return m_Control;
}


//-----------------------------------------------------------------------------
// Returns the default value for the control
//-----------------------------------------------------------------------------
float CAttributeSlider::GetControlDefaultValue( AnimationControlType_t type ) const
{
	if ( IsTransform() )
		return 0.0f;

	Assert( m_hControl.Get() );
	if ( !m_hControl.Get() )
		return 0.0f;

	switch ( type )
	{
	case ANIM_CONTROL_VALUE:
		return m_hControl->GetValue<float>( "defaultValue" );
	case ANIM_CONTROL_BALANCE:
		return m_hControl->GetValue<float>( "defaultBalance" );
	case ANIM_CONTROL_MULTILEVEL:
		return m_hControl->GetValue<float>( "defaultMultilevel" );
	}
	return 0.0f;
}


//-----------------------------------------------------------------------------
// Given a mouse position in (x,y) in local coordinates, which animation control is it over?
//-----------------------------------------------------------------------------
AnimationControlType_t CAttributeSlider::DetermineControl( int x, int y )
{
	if ( IsControlActive( ANIM_CONTROL_MULTILEVEL ) )
	{
		Rect_t rect;
		GetControlRect( &rect, ANIM_CONTROL_MULTILEVEL );
		if ( x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height )
			return ANIM_CONTROL_MULTILEVEL;
	}

	return ANIM_CONTROL_VALUE;
}


void CAttributeSlider::SetSelected( bool state )
{
	m_bSelected = state;
}

bool CAttributeSlider::IsSelected() const
{
	return m_bSelected;
}

void CAttributeSlider::SetIsLogPreviewControl( bool state )
{
	m_bIsLogPreviewControl = state;
}

void CAttributeSlider::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	m_bCursorInsidePanel = true;
}

void CAttributeSlider::OnCursorExited()
{
	BaseClass::OnCursorExited();
	m_bCursorInsidePanel = false;
}


//-----------------------------------------------------------------------------
// Mouse event handlers
//-----------------------------------------------------------------------------
void CAttributeSlider::OnMousePressed( MouseCode code )
{
	if ( !IsEnabled() || IsInTextEntry() || IsDragging() )
		return;

	// Deal with transform sliders
	if ( m_bTransform )
	{
		bool bCtrlDown = ( input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL ) );
		m_pParent->SetLogPreviewControl( m_hControl );
		if ( !bCtrlDown )
		{
			m_pParent->ClearSelectedControls();
		}
		m_pParent->SetControlSelected( this, !IsSelected() );
		return;
	}

	// Determine which control we clicked on
	int x,y;
	input()->GetCursorPosition( x, y );
	ScreenToLocal( x, y );
	AnimationControlType_t type = DetermineControl( x, y );

	// Right click sets the value to match the default value
	if ( code == MOUSE_RIGHT )
	{
		SetValue( type, GetControlDefaultValue( type ) );

		CUndoScopeGuard guard( "Set Slider Value To Default" );

		StampValueIntoLogs( type, GetControlDefaultValue( type ) );
		return;
	}

	if ( code != MOUSE_LEFT )
		return;

	// Cache off the value at the click point
	// in case we end up receiving a double-click
	m_InitialTextEntryValue = m_Control;

	// Enter drag mode
	m_SliderMode = (SliderMode_t)( SLIDER_MODE_FIRST_DRAG_MODE + type );
	m_nDragStartPosition[ 0 ] = x;
	m_nDragStartPosition[ 1 ] = y;
	m_nAccum[ 0 ] = m_nAccum[ 1 ] = 0;
	m_flDragStartValue = GetValue( type );
	m_flDragStartBalance = GetValue( ANIM_CONTROL_BALANCE );
	input()->SetMouseCapture( GetVPanel() );
	SetCursor( dc_blank );
	m_pParent->RecomputePreview();
}


void CAttributeSlider::OnCursorMoved( int x, int y )
{
	if ( !IsEnabled() || !IsDragging() || m_bTransform )
		return;

	// NOTE: This works because we always slam the mouse to be back at the start position
	// at the end of this function

	// Accumulate the total mouse movement
	int dx = x - m_nDragStartPosition[ 0 ];
	m_nAccum[ 0 ] += dx;
	float flFactor = FRAC_PER_PIXEL * ifm_attributeslider_sensitivity.GetFloat();

	bool bInRecordMode = m_pParent->GetEditor()->GetRecordingState() == AS_RECORD;
	float flMinVal = bInRecordMode ? -1.0f : 0.0f;
	float flMaxVal = bInRecordMode ?  2.0f : 1.0f;

	// Clamp accum so we never generate values < -1 or > 2
	int nMinVal = floor( ( -m_flDragStartValue + flMinVal ) / flFactor );
	int nMaxVal = ceil( ( -m_flDragStartValue + flMaxVal ) / flFactor );
	m_nAccum[ 0 ] = clamp( m_nAccum[ 0 ], nMinVal, nMaxVal );

	float flDelta = flFactor * m_nAccum[ 0 ];
	if ( GetDragControl() == ANIM_CONTROL_VALUE && IsControlActive( ANIM_CONTROL_BALANCE ) )
	{
		// do the hacky conversion from the ui's left/right to the underlying value/balance
		float flLeftValue, flRightValue;
		ValueBalanceToLeftRight( &flLeftValue, &flRightValue, m_flDragStartValue, m_flDragStartBalance );

		float flLeftDelta, flRightDelta;
		ValueBalanceToLeftRight( &flLeftDelta, &flRightDelta, flDelta, m_pParent->GetBalanceSliderValue() );

		flLeftValue  = clamp( flLeftValue  + flLeftDelta,  flMinVal, flMaxVal );
		flRightValue = clamp( flRightValue + flRightDelta, flMinVal, flMaxVal );

		float flValue, flBalance;
		LeftRightToValueBalance( &flValue, &flBalance, flLeftValue, flRightValue );

		SetValue( GetDragControl(), flValue );
		SetValue( ANIM_CONTROL_BALANCE, flBalance ); // TODO - add balance for multi control as well
	}
	else
	{
		float flValue = clamp( m_flDragStartValue + flDelta, flMinVal, flMaxVal );
		SetValue( GetDragControl(), flValue );
	}

	// Slam the cursor back to the drag start point
	if ( x != m_nDragStartPosition[ 0 ] || y != m_nDragStartPosition[ 1 ] )
	{
		x = m_nDragStartPosition[ 0 ];
		y = m_nDragStartPosition[ 1 ];
		LocalToScreen( x, y );
		input()->SetCursorPos( x, y );
	}
}

void CAttributeSlider::OnMouseReleased( MouseCode code )
{
	if ( !IsEnabled() || !IsDragging() || m_bTransform )
		return;

	m_SliderMode = SLIDER_MODE_NONE;
	input()->SetMouseCapture( NULL );
	SetCursor( dc_arrow );
	m_pParent->RecomputePreview();
}



//-----------------------------------------------------------------------------
//
// Methods related to text entry mode
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Called by the text entry code to enter the value into the logs
//-----------------------------------------------------------------------------
void CAttributeSlider::StampValueIntoLogs( AnimationControlType_t type, float flValue )
{
	Assert( !m_bTransform );
	m_pParent->StampValueIntoLogs( m_hControl, type, flValue );
}


//-----------------------------------------------------------------------------
// Key typed key handler
//-----------------------------------------------------------------------------
void CAttributeSlider::OnKeyCodeTyped( KeyCode code )
{
	if ( !IsInTextEntry() )
	{
		BaseClass::OnKeyCodeTyped( code );
		return;
	}

	switch ( code )
	{
	default:
		BaseClass::OnKeyCodeTyped( code );
		break;

	case KEY_ESCAPE:
		DiscardTextEntryValue();
		break;

	case KEY_ENTER:
		AcceptTextEntryValue();
		break;
	}
}


//-----------------------------------------------------------------------------
// Methods to entry text entry mode
//-----------------------------------------------------------------------------
void CAttributeSlider::EnterTextEntryMode( AnimationControlType_t type, bool bRelatchValues )
{
	if ( m_bTransform )
		return;

	m_SliderMode = (SliderMode_t)( SLIDER_MODE_FIRST_TEXT_MODE + type );

	// For double-clicking, ignore the value set by the first single mouse click
	if ( !bRelatchValues )
	{
		SetValue( m_InitialTextEntryValue );
	}

	m_pTextField->SetVisible( true );
	m_pTextField->SetEnabled( true );

	if ( type == ANIM_CONTROL_VALUE && IsControlActive( ANIM_CONTROL_BALANCE ) )
	{
		if ( !m_pRightTextField )
		{
			m_pRightTextField = new CAttributeSliderTextEntry( this, GetName() );
			m_pRightTextField->SetVisible( false );
			m_pRightTextField->SetEnabled( false );
			m_pRightTextField->SelectAllOnFocusAlways( true );
			InvalidateLayout();
		}
		m_pRightTextField->SetVisible( true );
		m_pRightTextField->SetEnabled( true );

		float flValue   = m_InitialTextEntryValue.m_pValue[ ANIM_CONTROL_VALUE ];
		float flBalance = m_InitialTextEntryValue.m_pValue[ ANIM_CONTROL_BALANCE ];
		float flLeftValue, flRightValue;
		ValueBalanceToLeftRight( &flLeftValue, &flRightValue, flValue, flBalance );

		char val[ 64 ];
		V_snprintf( val, sizeof( val ), "%f", flLeftValue );
		m_pTextField->SetText( val );
		V_snprintf( val, sizeof( val ), "%f", flRightValue );
		m_pRightTextField->SetText( val );

		m_pRightTextField->GotoTextEnd();
		m_pRightTextField->RequestFocus();
	}
	else
	{
		char val[ 64 ];
		Q_snprintf( val, sizeof( val ), "%f", m_InitialTextEntryValue.m_pValue[ type ] );
		m_pTextField->SetText( val );
	}

	m_pTextField->GotoTextEnd();
	m_pTextField->RequestFocus();
}


//-----------------------------------------------------------------------------
// Methods to accept or discard the value in the text entry field
//-----------------------------------------------------------------------------
void CAttributeSlider::AcceptTextEntryValue()
{
	if ( !IsInTextEntry() )
		return;

	Assert( !m_bTransform );

	// Get the value in the text entry field
	char buf[ 64 ];
	m_pTextField->GetText( buf, sizeof( buf ) );
	float flValue = Q_atof( buf );

	// Hide the text entry
	m_pTextField->SetVisible( false );
	m_pTextField->SetEnabled( false );

	if ( m_pRightTextField && GetTextEntryControl() == ANIM_CONTROL_VALUE && IsControlActive( ANIM_CONTROL_BALANCE ) )
	{
		float flLeftValue = flValue;

		// Get the value in the text entry field
		buf[0] = 0;
		m_pRightTextField->GetText( buf, sizeof( buf ) );
		float flRightValue = Q_atof( buf );

		// Hide the text entry
		m_pRightTextField->SetVisible( false );
		m_pRightTextField->SetEnabled( false );

		float flBalance;
		LeftRightToValueBalance( &flValue, &flBalance, flLeftValue, flRightValue );

		SetValue( ANIM_CONTROL_BALANCE, flBalance );
		StampValueIntoLogs( ANIM_CONTROL_BALANCE, flBalance );
	}

	// Apply the change
	AnimationControlType_t type = GetTextEntryControl();
	SetValue( type, flValue );
	StampValueIntoLogs( type, flValue );

	m_SliderMode = SLIDER_MODE_NONE;
	RequestFocus();
}

void CAttributeSlider::DiscardTextEntryValue()
{
	if ( !IsInTextEntry() )
		return;

	Assert( !m_bTransform );

	// Hide the text entry
	m_pTextField->SetVisible( false );
	m_pTextField->SetEnabled( false );

	if ( m_pRightTextField && GetTextEntryControl() == ANIM_CONTROL_VALUE && IsControlActive( ANIM_CONTROL_BALANCE ) )
	{
		m_pRightTextField->SetVisible( false );
		m_pRightTextField->SetEnabled( false );
	}

	m_SliderMode = SLIDER_MODE_NONE;
	RequestFocus();
}


//-----------------------------------------------------------------------------
// Methods of the text entry widget
//-----------------------------------------------------------------------------
void CAttributeSliderTextEntry::OnKillFocus( KeyValues *pParams )
{
	Assert( m_pSlider );

	SelectNone();

	VPANEL hPanel = (VPANEL)pParams->GetPtr( "newPanel" );
	if ( hPanel != INVALID_PANEL && vgui::ipanel()->GetParent( hPanel ) == m_pSlider->GetVPanel() )
		return;

	m_pSlider->AcceptTextEntryValue();
}

void CAttributeSliderTextEntry::OnMouseWheeled( int delta )
{
	if ( m_pSlider->m_bTransform )
		return;

	float deltaFactor;
	if ( input()->IsKeyDown(KEY_LSHIFT) )
	{
		deltaFactor = ((float)delta) * 10.0f;
	}
	else if ( input()->IsKeyDown(KEY_LCONTROL) )
	{
		deltaFactor = ((float)delta) / 100.0;
	}
	else
	{
		deltaFactor = ((float)delta) / 10.0;
	}

	char sz[ 64 ];
	GetText( sz, sizeof( sz ) );

	float val = Q_atof( sz ) + deltaFactor;
	if ( input()->IsKeyDown(KEY_LALT) )
	{
		val = clamp( val, 0.0f, 1.0f );
	}

	Q_snprintf( sz, sizeof( sz ), "%f", val );

	SetText( sz );
	m_pSlider->SetValue( ANIM_CONTROL_VALUE, val );

	CUndoScopeGuard guard( UNDO_CHAIN_MOUSEWHEEL_ATTRIBUTE_SLIDER, "Set Slider Value" );

	m_pSlider->StampValueIntoLogs( m_pSlider->GetTextEntryControl(), val );
}

void CAttributeSlider::OnMouseDoublePressed( MouseCode code )
{
	if ( !IsEnabled() || IsDragging() )
		return;

	if ( code != MOUSE_LEFT )
		return;

	int x,y;
	input()->GetCursorPosition( x, y );
	ScreenToLocal( x, y );
	AnimationControlType_t type = DetermineControl( x, y );
	EnterTextEntryMode( type, false );
}


//-----------------------------------------------------------------------------
//
// Methods related to preview
//
//-----------------------------------------------------------------------------
void CAttributeSlider::EnablePreview( bool state, bool simple, bool faderdrag )
{
	m_bPreviewEnabled = state;
	m_bSimplePreviewOnly = simple;
	m_bFaderBeingDragged = faderdrag;
}

bool CAttributeSlider::IsPreviewEnabled() const
{
	return m_bPreviewEnabled;
}

bool CAttributeSlider::IsSimplePreview() const
{
	return m_bSimplePreviewOnly;
}

#define ATTRIBUTE_SLIDER_RAMP_TIME 0.5f

bool CAttributeSlider::IsRampingTowardPreview() const
{
	if ( m_flPreviewGoalTime == -1.0f )
		return false;

	return true;
}

void CAttributeSlider::RampDown()
{
	if ( m_flPreviewGoalTime == -1.0f )
		return;

	m_Previous.m_Current = GetValue();
	m_Previous.m_Full = GetValue( );
	m_bRampUp = false;
}

void CAttributeSlider::UpdateFaderAmount( float flAmount )
{
	m_flFaderAmount = flAmount;
	AttributeValue_t current = GetValue();
	if ( m_flPreviewGoalTime == -1.0f )
	{
		BlendFlexValues( &m_Preview.m_Current, current, m_Preview.m_Full, flAmount );
		return;
	}

	BlendFlexValues( &m_Next.m_Current, current, m_Next.m_Full, flAmount );
}

void CAttributeSlider::UpdateTime( float dt )
{
	if ( m_flPreviewGoalTime == -1.0f )
		return;

	// Move toward goal
	if ( m_bRampUp )
	{
		if ( m_flPreviewGoalTime < ATTRIBUTE_SLIDER_RAMP_TIME )
		{
			m_flPreviewGoalTime += dt;
		}
	}
	else
	{
		m_flPreviewGoalTime -= dt;
	}

	if ( m_flPreviewGoalTime >= ATTRIBUTE_SLIDER_RAMP_TIME )
	{
		m_Preview = m_Next;
		m_flPreviewGoalTime = ATTRIBUTE_SLIDER_RAMP_TIME;

	}
	else if ( m_flPreviewGoalTime <= 0.0f )
	{
		m_flPreviewGoalTime = -1.0f;
		m_Preview = m_Previous;
	}
	else
	{
		float frac = m_flPreviewGoalTime / ATTRIBUTE_SLIDER_RAMP_TIME;
		BlendFlexValues( &m_Preview.m_Current, m_Previous.m_Current, m_Next.m_Current, frac );
		BlendFlexValues( &m_Preview.m_Full, m_Previous.m_Full, m_Next.m_Full, frac );
	}
}

void CAttributeSlider::SetPreview( const AttributeValue_t &value, const AttributeValue_t &full, bool instantaneous, bool startfromcurrent )
{
	m_bRampUp = true;

	if ( instantaneous )
	{
		m_Next.m_Current = value;
		m_Next.m_Full = full;

		m_Preview = m_Previous = m_Next;
		m_flPreviewGoalTime = -1.0f;
	}
	else
	{
		// Current becomes previous, next becomes goal and preview starts moving toward that goal
		if ( startfromcurrent )
		{
			m_Previous.m_Current = GetValue( );
			m_Previous.m_Full = GetValue( );
		}
		else
		{
			m_Previous = m_Preview;
		}

		m_Next.m_Current = value;
		m_Next.m_Full = full;
		m_flPreviewGoalTime = 0.0f;
	}
}

const AttributeValue_t &CAttributeSlider::GetPreview() const
{
	return m_Preview.m_Current;
}

float CAttributeSlider::GetPreview( AnimationControlType_t type ) const
{
	return m_Preview.m_Current.m_pValue[type];
}

// Estimates the value of the control given a local coordinate
float CAttributeSlider::EstimateValueAtPos( int nLocalX, int nLocalY ) const
{
	Rect_t rect;
	GetControlRect( &rect, ANIM_CONTROL_VALUE );

	float flFactor = rect.width > 1 ? (float)( nLocalX - rect.x ) / (float)( rect.width - 1 ) : 0.5f;
	flFactor = clamp( flFactor, 0.0f, 1.0f );
	return flFactor;
}


//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CAttributeSlider::PerformLayout()
{
	BaseClass::PerformLayout();

	Rect_t rect;
	GetControlRect( &rect, ANIM_CONTROL_VALUE );

	// Place the text entry along the main attribute track rectangle
	if ( m_pRightTextField && GetTextEntryControl() == ANIM_CONTROL_VALUE && IsControlActive( ANIM_CONTROL_BALANCE ) )
	{
		m_pTextField     ->SetBounds( rect.x,                  rect.y, rect.width / 2, rect.height );
		m_pRightTextField->SetBounds( rect.x + rect.width / 2, rect.y, rect.width / 2, rect.height );
	}
	else
	{
		m_pTextField->SetBounds( rect.x, rect.y, rect.width, rect.height );
	}
}

void CAttributeSlider::GetControlRect( Rect_t *pRect, AnimationControlType_t type ) const
{
	int sw, sh;
	const_cast<CAttributeSlider*>( this )->GetSize( sw, sh );

	int cw, ch;
	m_pCircleImage->GetSize( cw, ch );

	switch ( type )
	{
	case ANIM_CONTROL_VALUE:
		pRect->x = 2 * SLIDER_PIXEL_SPACING + cw;
		pRect->y = SLIDER_PIXEL_SPACING;
		pRect->width = sw - pRect->x * 2;
		pRect->height = max( 0, sh - SLIDER_PIXEL_SPACING * 2 );
		break;
/*
	case ANIM_CONTROL_BALANCE:
		pRect->x = SLIDER_PIXEL_SPACING;
		pRect->y = max( 0, sh - ch ) / 2;
		pRect->width = cw;
		pRect->height = min( ch, sh );
		break;
*/
	case ANIM_CONTROL_MULTILEVEL:
		pRect->x = sw - SLIDER_PIXEL_SPACING - cw;
		pRect->y = max( 0, sh - ch ) / 2;
		pRect->width = cw;
		pRect->height = min( ch, sh );
		break;
	}
}

bool CAttributeSlider::IsFaderBeingDragged()
{
	return IsPreviewEnabled() && m_bFaderBeingDragged;
}


//-----------------------------------------------------------------------------
//
// Methods related to painting start here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Used to control how fader-driven ticks look
//-----------------------------------------------------------------------------
float CAttributeSlider::GetPreviewAlphaScale() const
{
	return max( m_flFaderAmount, 0.1f );
}


//-----------------------------------------------------------------------------
// Draws a tick on the main control
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawTick( const Color& clr, float frac, int width, int inset )
{
	// Get the control position
	Rect_t rect;
	GetControlRect( &rect, ANIM_CONTROL_VALUE );

	// Inset by 1 pixel
	rect.x++; rect.y++; rect.width -= 2; rect.height -= 2;

	surface()->DrawSetColor( clr );

	int previewx = (int)( frac * (float)rect.width + 0.5f ) + rect.x;
	int previewtall = rect.height - 2 * inset;
	int ypos = rect.y + ( rect.height - previewtall ) / 2;

	int xpos = previewx - width / 2;
	xpos = clamp( xpos, rect.x, rect.x + rect.width - width );
	surface()->DrawFilledRect( xpos, ypos, xpos + width, ypos + previewtall );
}


//-----------------------------------------------------------------------------
// Draws a preview tick on the main control
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawPreviewTick( bool bMainTick )
{
	Color col = s_PreviewTickColor;
	col[ 3 ] *= bMainTick ? GetPreviewAlphaScale() : 0.5f;
	DrawTick( col, m_Next.m_Full.m_pValue[ ANIM_CONTROL_VALUE ], 2, 2 );
}


//-----------------------------------------------------------------------------
// Draws a tick on a circular control
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawCircularTick( const Color& clr, float flValue, int nCenterX, int nCenterY, float flRadius )
{
	float flFraction = 1.0f;
	float flAngle = 0.0f;
	if ( flValue < 0.5f )
	{
		flFraction = ( flValue / 0.5f );
		flAngle = 180.0f + flFraction * 180.0f;
	}
	else
	{
		flFraction = ( flValue - 0.5f ) * 2.0f;
		flAngle = flFraction * 180.0f;
	}

	float flRadians = DEG2RAD( flAngle );
	float ca = cos( flRadians );
	float sa = sin( flRadians );

	int nEndX = nCenterX + flRadius * sa;
	int nEndY = nCenterY - flRadius * ca;

	surface()->DrawSetColor( clr );
	surface()->DrawLine( nCenterX, nCenterY, nEndX, nEndY );
}


//-----------------------------------------------------------------------------
// Draws a preview of a circular control
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawCircularPreview( AnimationControlType_t type, bool bMainTick, float flRadius )
{
	Rect_t rect;
	GetControlRect( &rect, type );

	// Fill left from top
	float flPreview = m_Next.m_Full.m_pValue[type];
	float flCurrent = GetValue( type );

	Color clr = s_PreviewTickColor;
	clr[ 3 ] *= bMainTick ? GetPreviewAlphaScale() : 0.5f;

	int nCenterX = rect.x + rect.width / 2;
	int nCenterY = rect.y + rect.height / 2;
	DrawCircularTick( clr, flPreview, nCenterX, nCenterY, flRadius );

	if ( m_bSimplePreviewOnly && !m_bFaderBeingDragged )
		return;

	clr = s_OldValueTickColor;
	if ( !bMainTick )
	{
		clr[ 3 ] *= 0.5f;
	}	

	DrawCircularTick( clr, flCurrent, nCenterX, nCenterY, flRadius );
}


//-----------------------------------------------------------------------------
// Paints ticks
//-----------------------------------------------------------------------------
void CAttributeSlider::Paint()
{
	DrawTick( s_OldValueTickColor, GetValue( ANIM_CONTROL_VALUE ), 1, 0 );
	if ( m_bPreviewEnabled )
	{
		DrawPreviewTick( true );
		if ( IsControlActive( ANIM_CONTROL_BALANCE ) )
		{
			DrawCircularPreview( ANIM_CONTROL_BALANCE, true, CIRCULAR_CONTROL_RADIUS );
		}
		if ( IsControlActive( ANIM_CONTROL_MULTILEVEL ) )
		{
			DrawCircularPreview( ANIM_CONTROL_MULTILEVEL, true, CIRCULAR_CONTROL_RADIUS );
		}
	}
}


//-----------------------------------------------------------------------------
// Draws the min, current, and max values for the slider
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawValueLabel( float flValue )
{
	float flMinVal = 0.0f;
	float flMaxVal = 1.0f;
	flValue = clamp( flValue, flMinVal, flMaxVal );

	Rect_t rect;
	GetControlRect( &rect, ANIM_CONTROL_VALUE );

	int cw, ch;
	char sz[ 32 ];
	Q_snprintf( sz, sizeof( sz ), "%.1f", flMinVal );
	m_pValues[ 0 ]->SetText( sz );
	m_pValues[ 0 ]->ResizeImageToContent();
	m_pValues[ 0 ]->GetContentSize( cw, ch );
	m_pValues[ 0 ]->SetPos( rect.x + 5, rect.y + ( rect.height - ch ) * 0.5f );
	m_pValues[ 0 ]->Paint();

	Q_snprintf( sz, sizeof( sz ), "%.1f", flMaxVal );
	m_pValues[ 2 ]->SetText( sz );
	m_pValues[ 2 ]->ResizeImageToContent();
	m_pValues[ 2 ]->GetContentSize( cw, ch );
	m_pValues[ 2 ]->SetPos( rect.x + rect.width - cw - 5, rect.y + ( rect.height - ch ) * 0.5f );
	m_pValues[ 2 ]->Paint();

	Q_snprintf( sz, sizeof( sz ), "%.3f", flValue );
	m_pValues[ 1 ]->SetText( sz );
	m_pValues[ 1 ]->ResizeImageToContent();
	m_pValues[ 1 ]->GetContentSize( cw, ch );
	m_pValues[ 1 ]->SetPos( rect.x + ( rect.width - cw ) * 0.5f, rect.y + ( rect.height - ch ) * 0.5f );
	m_pValues[ 1 ]->Paint();
}


//-----------------------------------------------------------------------------
// Draws the text for the slider. It's either the slider name, or its value if dragging is happening
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawNameLabel()
{
	if ( IsDragging() )
	{
		float flValue = GetValue( GetDragControl() );
		DrawValueLabel( flValue );
		return;
	}

	if ( IsInTextEntry() )
		return;

	int w, h;
	GetSize( w, h );

	int cw, ch;
	Color clr = m_bCursorInsidePanel ? s_TextColorFocus : s_TextColor;
	m_pName->SetColor( clr );
	m_pName->GetContentSize( cw, ch );

	Rect_t rect;
	GetControlRect( &rect, ANIM_CONTROL_VALUE );

	m_pName->SetPos( rect.x + ( rect.width - cw ) * 0.5f, rect.y + ( rect.height - ch ) * 0.5f );
	m_pName->Paint();
}


//-----------------------------------------------------------------------------
// Draws the midpoint value for the slider
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawMidpoint( int x, int ty, int ttall )
{
	surface()->DrawSetColor( s_MidpointColor );
	surface()->DrawFilledRect( x, ty, x + 1, ty + ttall );
}


//-----------------------------------------------------------------------------
// Paints circular controls used for balance + multilevel controls
//-----------------------------------------------------------------------------
void CAttributeSlider::PaintCircularControl( float flValue, const Rect_t& rect )
{
	flValue = clamp( flValue, 0.0f, 1.0f );

	m_pCircleImage->SetPos( rect.x, rect.y );
	m_pCircleImage->Paint();

	int ofs[ 2 ] = { 0 };
	LocalToScreen( ofs[ 0 ], ofs[ 1 ] );

	int nCenterX = ofs[ 0 ] + rect.x + rect.width / 2;
	int nCenterY = ofs[ 1 ] + rect.y + rect.height / 2;

	float maxTrianges = 36.0f;

	float frac = 0.0f;
	float step = 180.0f / (float)( maxTrianges );
	float ang = 0.0f;
	float clamp = 360.0f;
	int numTriangles = 0;
	float radius = CIRCULAR_CONTROL_RADIUS;

	float zpos = vgui::surface()->GetZPos();

	Vector centerVert( nCenterX, nCenterY, zpos );

	Vector top;
	top = centerVert;
	top.y -= radius;

	// Fill left from top
	if ( flValue < 0.5f )
	{
		frac = 1.0f - ( flValue / 0.5f );
		numTriangles = (int)( frac * ( maxTrianges ) + 0.5f );
		clamp = 180.0f;
		step = -step;
		ang = 360.0f;
	}
	else
	{
		frac = ( flValue - 0.5f ) / 0.5f;
		numTriangles = (int)( frac * ( maxTrianges ) + 0.5f );
		clamp = 180.0f;
	}

	if ( numTriangles == 0 )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_pWhite );

	Color clr( 102, 102, 102, 255 );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, numTriangles );

	Vector next;
	next.Init();

	for ( int j = 0; j < numTriangles; j++ )
	{
		ang += step;
		//ang = min( ang, clamp );

		float flRadians = DEG2RAD( ang );

		float ca = cos( flRadians );
		float sa = sin( flRadians );

		meshBuilder.Position3fv( centerVert.Base() );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		next.Init();
		next.x = radius * sa;
		next.y = -radius * ca;
		next += centerVert;

		if ( step > 0 )
		{
			meshBuilder.Position3fv( top.Base() );
			meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
			meshBuilder.TexCoord2f( 0, 0, 1 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( next.Base() );
			meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
			meshBuilder.TexCoord2f( 0, 1, 0 );
			meshBuilder.AdvanceVertex();
		}
		else
		{
			meshBuilder.Position3fv( next.Base() );
			meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
			meshBuilder.TexCoord2f( 0, 0, 1 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( top.Base() );
			meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
			meshBuilder.TexCoord2f( 0, 1, 0 );
			meshBuilder.AdvanceVertex();
		}

		top = next;
	}

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Paints the slider
//-----------------------------------------------------------------------------
void CAttributeSlider::PaintBackground()
{
	Rect_t rect;
	GetControlRect( &rect, ANIM_CONTROL_VALUE );

	// Paint the border
	surface()->DrawSetColor( Color( 24, 24, 24, 255 ) );
	// top and left
	surface()->DrawOutlinedRect( rect.x, rect.y, rect.x + rect.width, rect.y + 1 );
	surface()->DrawOutlinedRect( rect.x, rect.y, rect.x + 1, rect.y + rect.height );
	// right
	surface()->DrawSetColor( Color( 33, 33, 33, 255 ) );
	surface()->DrawOutlinedRect( rect.x + rect.width - 1, rect.y, rect.x + rect.width, rect.y + rect.height );
	// bottom
	surface()->DrawSetColor( Color( 56, 56, 56, 255 ) );
	surface()->DrawOutlinedRect( rect.x, rect.y + rect.height - 1, rect.x + rect.width, rect.y + rect.height );

	// Inset the rect by 1 pixel
	++rect.x; ++rect.y; rect.width -= 2; rect.height -= 2;

	int y0 = rect.y;
	int y1 = rect.y + rect.height / 2;
	int y2 = rect.y + rect.height;

	// Draw the main bar background
	surface()->DrawSetColor( s_ZeroColor[ m_bIsLogPreviewControl ][ IsSelected() ] );
	surface()->DrawFilledRect( rect.x, y0, rect.x + rect.width, y2 );

	AnimationControlType_t viewType = ANIM_CONTROL_VALUE;
	if ( IsDragging() )
	{
		viewType = GetDragControl();
	}
	else if ( IsInTextEntry() )
	{
		viewType = GetTextEntryControl();
	}

	bool bUsePreview = m_bPreviewEnabled && ( !m_bSimplePreviewOnly || m_bFaderBeingDragged );

	float flMidPoint = GetControlDefaultValue( viewType );
	int nMidPoint   = (int)( (float)rect.width * clamp( flMidPoint,   0.0f, 1.0f ) + 0.5f );

	float flValue = bUsePreview ? m_Preview.m_Current.m_pValue[viewType] : GetValue( viewType );
	if ( viewType == ANIM_CONTROL_VALUE && IsControlActive( ANIM_CONTROL_BALANCE ) )
	{
		float flBalance = bUsePreview ? m_Preview.m_Current.m_pValue[ ANIM_CONTROL_BALANCE ] : GetValue( ANIM_CONTROL_BALANCE );
		float flLeftValue, flRightValue;
		ValueBalanceToLeftRight( &flLeftValue, &flRightValue, flValue, flBalance );

		int nLeftValue  = (int)( (float)rect.width * clamp( flLeftValue,  0.0f, 1.0f ) + 0.5f );
		int nRightValue = (int)( (float)rect.width * clamp( flRightValue, 0.0f, 1.0f ) + 0.5f );

		// Draw the current value as a bar from the midpoint
		surface()->DrawSetColor( IsDragging() ? s_DraggingBarColor : s_BarColor[ m_bIsLogPreviewControl ][ IsSelected() ] );
		surface()->DrawFilledRect( rect.x + min( nLeftValue,  nMidPoint ), y0, rect.x + max( nLeftValue,  nMidPoint ), y1 );
		surface()->DrawFilledRect( rect.x + min( nRightValue, nMidPoint ), y1, rect.x + max( nRightValue, nMidPoint ), y2 );
	}
	else
	{
		Assert( viewType != ANIM_CONTROL_BALANCE );

		int nValue = (int)( (float)rect.width * clamp( flValue, 0.0f, 1.0f ) + 0.5f );

		// Draw the current value as a bar from the midpoint
		surface()->DrawSetColor( IsDragging() ? s_DraggingBarColor : s_BarColor[ m_bIsLogPreviewControl ][ IsSelected() ] );
		surface()->DrawFilledRect( rect.x + min( nValue, nMidPoint ), y0, rect.x + max( nValue, nMidPoint ), y2 );
	}

	// Draw the midpoint over the top of the current value
	DrawMidpoint( rect.x + nMidPoint, rect.y, rect.height );

	// Draw the name or value over the top of that
	DrawNameLabel();

	// Paints the circular controls
	if ( IsControlActive( ANIM_CONTROL_MULTILEVEL ) )
	{
		float flMultiValue = bUsePreview ? m_Preview.m_Current.m_pValue[ANIM_CONTROL_MULTILEVEL] : GetValue( ANIM_CONTROL_MULTILEVEL );
		GetControlRect( &rect, ANIM_CONTROL_MULTILEVEL );
		PaintCircularControl( flMultiValue, rect );

		// Draws the midpoint for the circular controls
		int nCenterX = rect.x + rect.width / 2;
		int nCenterY = rect.y + rect.height / 2;
		DrawCircularTick( s_MidpointColor, GetControlDefaultValue( ANIM_CONTROL_MULTILEVEL ), nCenterX, nCenterY, CIRCULAR_CONTROL_RADIUS );
	}
}