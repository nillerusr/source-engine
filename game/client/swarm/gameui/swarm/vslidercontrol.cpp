#include "VSliderControl.h"
#include "VHybridButton.h"
#include "vgui/ISurface.h"
#include "vgui_controls/ProgressBar.h"
#include "tier1/KeyValues.h"
#include "IGameUIFuncs.h"
#include "gameui_util.h"
#include "vgui/IInput.h"
#include "VFlyoutMenu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

DECLARE_BUILD_FACTORY( SliderControl );

//=============================================================================
// Slider Control
//=============================================================================
SliderControl::SliderControl( vgui::Panel* parent, const char* panelName ):
BaseClass( parent, panelName )
{
	m_bDragging = false;

	m_button = NULL;
	m_lblSliderText = NULL;
	m_prgValue = NULL;
	m_defaultMark = NULL;

	m_focusColor = Color( 255, 255, 255, 255 );
	m_unfocusColor = Color( 100, 100, 100, 255 );
	m_backgroundFocusColor = Color( 200, 200, 200, 255 );

	m_min = 0.0f;
	m_max = 1.0f;
	m_curValue = 0.5f;
	m_stepSize = 1.0f;
	m_conVarRef = NULL;
	m_conVarDefaultRef = NULL;

	m_bDirty = false;

	LoadControlSettings( "Resource/UI/BaseModUI/SliderControl.res" );
}

SliderControl::~SliderControl()
{
	SetConCommand( NULL );
	SetConCommandDefault( NULL );
}

void SliderControl::SetEnabled(bool state)
{
	if( m_button )
	{
		m_button->SetEnabled( state );
	}
	BaseClass::SetEnabled( state );

	if ( m_lblSliderText )
	{
		m_lblSliderText->SetEnabled( state );
	}

	if ( m_prgValue )
	{
		m_prgValue->SetVisible( state );
	}

	if ( m_defaultMark )
	{
		m_defaultMark->SetVisible( state );
	}
}

float SliderControl::GetCurrentValue()
{
	return m_curValue;
}

void SliderControl::SetCurrentValue( float value, bool bReset )
{
	float fNewValue = MAX( MIN( value, GetMax() ), GetMin() );

	// If we're just resetting the value don't play sound effects or ignore the same value being set
	if ( !bReset )
	{
		if ( fNewValue == m_curValue )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_CLICK );
		m_bDirty = true;
	}

	m_curValue = fNewValue;

	UpdateProgressBar();
}

float SliderControl::Increment( float stepSize )
{
	SetCurrentValue( GetCurrentValue() + stepSize );
	return GetCurrentValue();
}

float SliderControl::Decrement( float stepSize )
{
	SetCurrentValue( GetCurrentValue() - stepSize );
	return GetCurrentValue();
}

const char* SliderControl::GetConCommand()
{
	if( m_conVarRef && m_conVarRef->IsValid() )
	{
		return m_conVarRef->GetName();
	}

	return NULL;
};

float SliderControl::GetConCommandDefault()
{
	if( m_conVarDefaultRef && m_conVarDefaultRef->IsValid() )
	{
		return m_conVarDefaultRef->GetFloat();
	}

	if( m_conVarRef && m_conVarRef->IsValid() )
	{
		return atof( m_conVarRef->GetDefault() );
	}

	// Couldn't get a default value
	return GetMax();
};


float SliderControl::GetStepSize()
{
	return m_stepSize;
}

float SliderControl::GetMin()
{
	return m_min;
}

float SliderControl::GetMax()
{
	return m_max;
}

void SliderControl::SetConCommand( const char* conCommand )
{
	if( conCommand )
	{
		if( m_conVarRef )
		{
			delete m_conVarRef;
		}

		m_conVarRef = new CGameUIConVarRef( conCommand );

		Reset();
	}
	else
	{
		if( m_conVarRef )
		{
			delete m_conVarRef;
		}

		m_conVarRef = NULL;
	}
}

void SliderControl::SetConCommandDefault( const char* conCommand )
{
	if( conCommand )
	{
		if( m_conVarDefaultRef )
		{
			delete m_conVarDefaultRef;
		}

		m_conVarDefaultRef = new CGameUIConVarRef( conCommand );
	}
	else
	{
		if( m_conVarDefaultRef )
		{
			delete m_conVarDefaultRef;
		}

		m_conVarDefaultRef = NULL;
	}
}

void SliderControl::SetStepSize( float stepSize )
{
	m_stepSize = stepSize;
}

void SliderControl::SetMin( float min )
{
	m_min = min;
	SetCurrentValue( GetCurrentValue(), true ); //make sure that the current value doesn't go out of bounds
}

void SliderControl::SetMax( float max )
{
	m_max = max;
	SetCurrentValue( GetCurrentValue(), true ); //make sure that the current value doesn't go out of bounds
}

void SliderControl::SetInverse( bool inverse )
{
	m_inverse = inverse;
}

bool SliderControl::GetInversed()
{
	return m_inverse;
}

void SliderControl::Reset()
{
	CGameUIConVarRef* conVar = GetConVarRef();
	if( conVar && conVar->IsValid() )
	{
		SetCurrentValue( conVar->GetFloat(), true );
	}

	ResetSliderPosAndDefaultMarkers();
}

void SliderControl::ResetSliderPosAndDefaultMarkers()
{
	if( m_prgValue )
	{
		int centery = GetTall() / 2;

		int xpos, ypos;
		ypos = centery - m_prgValue->GetTall() / 2;
		xpos = GetWide() - m_prgValue->GetWide();
		m_prgValue->SetPos( xpos, ypos );

		if ( m_defaultMark )
		{
			float fInterp = ( GetConCommandDefault() - m_min ) / ( m_max - m_min );

			if ( m_inverse )
			{
				fInterp = 1.0f - fInterp;
			}

			m_defaultMark->SetPos( xpos + ( m_prgValue->GetWide() - m_defaultMark->GetWide() ) * fInterp, ypos - 3 );
			m_defaultMark->SetBgColor( m_unfocusColor );
			m_defaultMark->SetVisible( IsEnabled() );
		}

		m_prgValue->SetVisible( IsEnabled() );
	}
	else
	{
		if ( m_defaultMark )
		{
			m_defaultMark->SetVisible( false );
		}
	}
}

void SliderControl::ApplySettings( KeyValues* inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_button = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnDropButton" ) );
	m_lblSliderText = dynamic_cast< vgui::Label* >( FindChildByName( "LblSliderText" ) );
	m_prgValue = dynamic_cast< vgui::ProgressBar* >( FindChildByName( "PrgValue" ) );
	m_defaultMark = dynamic_cast< vgui::Panel* >( FindChildByName( "PnlDefaultMark" ) );

#ifdef _X360
	if( m_button )
	{
		if( !HasFocus() && !m_button->HasFocus() )
		{
			m_button->NavigateFrom( );
		}
	}
#endif //_X360

	SetStepSize( inResourceData->GetFloat( "stepSize", 1.0f ) );
	SetMin( inResourceData->GetFloat( "minValue", 0.0f ) );
	SetMax( inResourceData->GetFloat( "maxValue", 100.0f ) );
	SetConCommand( inResourceData->GetString( "conCommand", NULL ) );
	SetConCommandDefault( inResourceData->GetString( "conCommandDefault", NULL ) );

	SetInverse( inResourceData->GetInt( "inverseFill", 0 ) == 1 );

	int labelTall = 0;
	int labelWide = 0;
	if( m_lblSliderText )
	{
		vgui::IScheme *scheme = vgui::scheme()->GetIScheme( GetScheme() );
		if( scheme )
		{
			const char* fontName = inResourceData->GetString( "font", NULL );
			if( fontName )
			{
				vgui::HFont font = scheme->GetFont( fontName , true );
				if( font )
				{
					m_lblSliderText->SetFont( font );
				}
			}
		}
		m_lblSliderText->SetText( inResourceData->GetString( "labelText" , "" ) );
		labelTall = m_lblSliderText->GetTall();
		labelWide = m_lblSliderText->GetWide();

		m_lblSliderText->SetEnabled( IsEnabled() );

		m_lblSliderText->SetTall( GetTall() );
	}

	if( m_button )
	{
		int x, y, wide, tall;
		int newTall = m_button->GetTall();
		GetBounds( x, y, wide, tall );
		// move the y up so the control stays up when we make it taller
		SetBounds( x, y - (newTall-tall)/2, wide, newTall );
	}

	ResetSliderPosAndDefaultMarkers();
}

void SliderControl::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_focusColor = pScheme->GetColor( "HybridButton.BorderColor", m_focusColor );
}

//=============================================================================
void SliderControl::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );
		panel->SetMouseInputEnabled( false );
	}

	SetDragEnabled( true );
	SetShowDragHelper( false );

	if( m_prgValue )
	{
		m_prgValue->InvalidateLayout( true );
	}
	if ( m_defaultMark )
	{
		m_defaultMark->InvalidateLayout( true );
	}
	ResetSliderPosAndDefaultMarkers();
}

void SliderControl::OnKeyCodePressed( vgui::KeyCode code )
{
	int userId = GetJoystickForCode( code );
	vgui::KeyCode basecode = GetBaseButtonCode( code );

	int active_userId = CBaseModPanel::GetSingleton().GetLastActiveUserId()	;
	
	if( userId != active_userId || userId < 0 )
	{	
		return;
	}

	switch( basecode )
	{
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XBUTTON_RIGHT_SHOULDER:
	case KEY_RIGHT:
		{
			if ( IsEnabled() )
			{
				if( !GetInversed() )
				{
					Increment( GetStepSize() );
				}
				else
				{
					Decrement( GetStepSize() );
				}
			}

			break;
		}
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XBUTTON_LEFT_SHOULDER:
	case KEY_LEFT:
		{
			if ( IsEnabled() )
			{
				if( !GetInversed() )
				{
					Decrement( GetStepSize() );
				}
				else
				{
					Increment( GetStepSize() );
				}
			}

			break;
		}
		break;
	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
void SliderControl::OnMousePressed( vgui::MouseCode code )
{
	FlyoutMenu::CloseActiveMenu(); //close any open flyouts

	switch ( code )
	{
	case MOUSE_LEFT:
		HandleMouseInput( false );
		break;
	}
}

void SliderControl::OnStartDragging()
{
	BaseClass::OnStartDragging();

	HandleMouseInput( true );
}

void SliderControl::OnContinueDragging()
{
	BaseClass::OnContinueDragging();

	HandleMouseInput( true );
}

void SliderControl::OnFinishDragging( bool mousereleased, vgui::MouseCode code, bool aborted )
{
	BaseClass::OnFinishDragging( mousereleased, code, aborted );

	m_bDragging = false;

	int iClickPosX;
	int iClickPosY;

	input()->GetCursorPos( iClickPosX, iClickPosY );

	int iSliderPosX, iSliderPosY;
	m_prgValue->GetPos( iSliderPosX, iSliderPosY );
	LocalToScreen( iSliderPosX, iSliderPosY );

	int iSliderWide = m_prgValue->GetWide();
	int iSliderTall = m_prgValue->GetTall();

	// See if we clicked within the bounds of the slider bar
	if ( !( iClickPosX >= iSliderPosX && iClickPosX <= iSliderPosX + iSliderWide && 
		    iClickPosY >= iSliderPosY && iClickPosY <= iSliderPosY + iSliderTall ) )
	{
		OnCursorExited();
	}
}

void SliderControl::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	if ( IsPC() )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
		if( GetParent() )
			GetParent()->NavigateToChild( this );
		else
			NavigateTo();
	}
}

void SliderControl::OnCursorExited()
{
	// This is a hack for now, we shouldn't close if the cursor goes to the flyout of this item...
	// Maybe have VFloutMenu check the m_navFrom and it's one of these, keep the SetClosedState...
	BaseClass::OnCursorExited();

	if ( !m_bDragging )
	{
		NavigateFrom();
	}
}

void SliderControl::NavigateToChild( Panel *pNavigateTo )
{
	if( GetParent() )
		GetParent()->NavigateToChild( this ); //pass it up the chain
	else
		BaseClass::NavigateToChild( pNavigateTo );
}

void SliderControl::HandleMouseInput( bool bDrag )
{
	int iClickPosX;
	int iClickPosY;

	input()->GetCursorPos( iClickPosX, iClickPosY );

	int iSliderPosX, iSliderPosY;
	m_prgValue->GetPos( iSliderPosX, iSliderPosY );
	LocalToScreen( iSliderPosX, iSliderPosY );

	int iSliderWide = m_prgValue->GetWide();
	int iSliderTall = m_prgValue->GetTall();

	// See if we clicked within the bounds of the slider bar or are already dragging it
	if ( m_bDragging || 
		 ( iClickPosX >= iSliderPosX && iClickPosX <= iSliderPosX + iSliderWide && 
		   iClickPosY >= iSliderPosY && iClickPosY <= iSliderPosY + iSliderTall ) )
	{
		float fMin = GetMin();
		float fMax = GetMax();

		if ( iSliderWide > 0 )
		{
			float fNormalizedPosition = clamp( static_cast<float>( iClickPosX - iSliderPosX ) / static_cast<float>( iSliderWide ), 0.0f, 1.0f );

			if ( GetInversed() )
			{
				fNormalizedPosition = 1.0f - fNormalizedPosition;
			}

			SetCurrentValue( fNormalizedPosition * ( fMax - fMin ) + fMin, true );

			if ( bDrag )
			{
				m_bDragging = true;
			}
		}
	}
}

void SliderControl::NavigateTo()
{
	BaseClass::NavigateTo();
	if( m_button )
	{
		//m_button->RequestFocus();
		m_button->NavigateTo();
	}

	if ( m_defaultMark )
	{
		m_defaultMark->SetBgColor( m_focusColor );
	}

	if ( m_prgValue )
	{
		m_prgValue->SetFgColor( m_backgroundFocusColor );
	}
}

void SliderControl::NavigateFrom()
{
	BaseClass::NavigateFrom();

	if ( m_defaultMark )
	{
		m_defaultMark->SetBgColor( m_unfocusColor );
	}

	if ( m_prgValue )
	{
		m_prgValue->SetFgColor( m_unfocusColor );
	}
}

CGameUIConVarRef* SliderControl::GetConVarRef()
{
	return m_conVarRef;
}

float SliderControl::UpdateProgressBar()
{
	float percentage = ( GetCurrentValue() - GetMin() ) / ( GetMax() - GetMin() );
	if( GetInversed() )
	{
		percentage = 1.0f - percentage;
	}

	if( m_prgValue )
	{
		m_prgValue->SetProgress( percentage );
	}

	UpdateConVar();
	return percentage;
}

void SliderControl::UpdateConVar()
{
	CGameUIConVarRef* conVar = GetConVarRef();
	if( conVar && conVar->IsValid() )
	{
		conVar->SetValue( GetCurrentValue() );
	}
}
