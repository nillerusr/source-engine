#include "VDropDownMenu.h"
#include "VHybridButton.h"
#include "VFlyoutMenu.h"

#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

DECLARE_BUILD_FACTORY( DropDownMenu );

DropDownMenu::DropDownMenu( vgui::Panel* parent, const char* panelName ):
BaseClass( parent, panelName )
{
	SetProportional( true );

	m_pButton = NULL;
	m_hCurrentFlyout = NULL;
	m_pnlBackground = NULL;
	m_curSelText[0] = '\0';
	m_openCallback = 0;
	m_SelectedTextEnabled = true;

	SetConsoleStylePanel( true );

	//	LoadControlSettings( "Resource/UI/BaseModUI/DropDownMenu.res" );
}

DropDownMenu::~DropDownMenu()
{
}

void DropDownMenu::NavigateTo()
{
	BaseClass::NavigateTo();
	if ( m_pButton )
	{
		m_pButton->NavigateTo();
	}
}

void DropDownMenu::NavigateFrom()
{
	BaseClass::NavigateFrom();
	if ( m_pButton )
	{
		m_pButton->NavigateFrom();
	}
}

void DropDownMenu::NavigateToChild( Panel *pNavigateTo )
{
	if( (pNavigateTo == m_pButton) && GetParent() ) //received this call from our button, pass it up since we're supposed to be one with the button
	{
		GetParent()->NavigateToChild( this );
	}
	else
	{
		BaseClass::NavigateToChild( pNavigateTo ); //not sure if we'll ever hit this.
	}
}

void DropDownMenu::SetCurrentSelection( const char *selection )
{
	bool bGotTextFromCommand = false;

	char szDescription[MAX_PATH] = "";

	FlyoutMenu *pFlyout = GetCurrentFlyout();
	if ( pFlyout )
	{
		Button *pCurrentButton = m_hCurrentFlyout->FindChildButtonByCommand( selection );

		if ( pCurrentButton )
		{
			bGotTextFromCommand = true;
			V_strncpy( m_curSelText, selection, sizeof( m_curSelText ) );
			pCurrentButton->GetText( szDescription, sizeof( szDescription ) );
		}
	}

	if ( !bGotTextFromCommand )
	{
		V_strncpy( m_curSelText, selection, sizeof( m_curSelText ) );
		V_strncpy( szDescription, selection, sizeof( szDescription ) );
	}

	if ( m_pButton && m_SelectedTextEnabled )
	{
		m_pButton->SetDropdownSelection( szDescription );
	}

	//when we set a new selection make sure we notify the parent
	GetParent()->OnCommand( selection );
}

void DropDownMenu::ChangeSelection( SelectionChange_t eNext )
{
	if ( !IsEnabled() || !m_pButton || !m_pButton->IsEnabled() )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
		return;
	}

	FlyoutMenu *pFlyout = GetCurrentFlyout();
	if ( pFlyout )
	{
		Button *pCurrentButton = m_hCurrentFlyout->FindChildButtonByCommand( m_curSelText );

		Button *pButton = ( eNext == SELECT_PREV ) ? ( m_hCurrentFlyout->FindPrevChildButtonByCommand( m_curSelText ) ) : ( m_hCurrentFlyout->FindNextChildButtonByCommand( m_curSelText ) );

		if ( pButton )
		{
			pButton->OnThink();
		}

		// Find the next enabled option
		while ( pButton && pButton->GetCommand() && !( pButton == pCurrentButton || pButton->IsEnabled() ) )
		{
			const char *pCommand = pButton->GetCommand()->GetString( "command", NULL );
			pButton = ( eNext == SELECT_PREV ) ? ( m_hCurrentFlyout->FindPrevChildButtonByCommand( pCommand ) ) : ( m_hCurrentFlyout->FindNextChildButtonByCommand( pCommand ) );

			if ( pButton )
			{
				pButton->OnThink();
			}
		}

		if ( pButton == pCurrentButton )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
		}
		else
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_CLICK );

			if ( pButton && pButton->GetCommand() )
			{
				SetCurrentSelection( pButton->GetCommand()->GetString( "command", NULL ) );
			}
		}
	}
}

const char* DropDownMenu::GetCurrentSelection()
{
	return m_curSelText[0] == '\0' ? NULL : m_curSelText;
}

void DropDownMenu::CloseDropDown()
{
	FlyoutMenu* flyout = GetCurrentFlyout();
	if ( flyout && flyout->IsVisible() )
	{
		flyout->CloseMenu( m_pButton );
	}
}

void DropDownMenu::OnCommand( const char* command )
{
	//if the command is the name of a flyout then launch the flyout menu
	bool relayCommand = true;
	m_hCurrentFlyout = dynamic_cast< FlyoutMenu* >( FindSiblingByName( command ) );
	if ( m_hCurrentFlyout.Get() )
	{		
		if ( !m_hCurrentFlyout->IsVisible() )
		{
			// open flyout if not visible
			vgui::Panel* initialSelection = m_hCurrentFlyout->FindChildButtonByCommand( GetCurrentSelection() );
			//RequestFocus();
			m_hCurrentFlyout->OpenMenu( m_pButton, initialSelection );

			if( m_openCallback )
				m_openCallback( this, m_hCurrentFlyout );
		}
		else
		{
			// if flyout for this menu is already open, close it
			m_hCurrentFlyout->CloseMenu( m_pButton );
		}

		relayCommand = false;
	}

	//if we do not handle the command by throwing out a flyout then assume it was a selection
	if( relayCommand )
	{
		SetCurrentSelection( command );
	}
}

BaseModUI::FlyoutMenu* DropDownMenu::GetCurrentFlyout()
{
	if ( m_hCurrentFlyout.Get() == NULL )
	{
		if ( m_pButton && m_pButton->GetCommand() )
		{
			m_hCurrentFlyout = dynamic_cast< FlyoutMenu* >( FindSiblingByName( m_pButton->GetCommand()->GetString( "command", NULL ) ) );
		}
	}

	return m_hCurrentFlyout;
}

void DropDownMenu::SetEnabled( bool state )
{
	if ( m_pButton )
	{
		m_pButton->SetEnabled( state );
		m_pButton->EnableDropdownSelection( state );
	}

	BaseClass::SetEnabled( state );
}

void DropDownMenu::SetSelectedTextEnabled( bool state )
{
	m_SelectedTextEnabled = state;
	if ( !m_SelectedTextEnabled )
	{
		m_curSelText[0] = '\0';
		if ( m_pButton )
		{
			m_pButton->SetDropdownSelection( m_curSelText );
		}
	}
}

void DropDownMenu::SetFlyoutItemEnabled( const char* selection, bool state )
{
	if ( m_hCurrentFlyout.Get() )
	{
		vgui::Panel* btnFlyoutItem = m_hCurrentFlyout->FindChildByName( selection );
		if ( btnFlyoutItem )
		{
			btnFlyoutItem->SetEnabled( state );
		}
	}
}

void DropDownMenu::SetFlyout( const char* flyoutName )
{
	if ( m_pButton )
	{
		m_pButton->SetCommand( flyoutName );
	}

	m_hCurrentFlyout = dynamic_cast< FlyoutMenu* >( FindSiblingByName( flyoutName ) );
}

void DropDownMenu::SetOpenCallback( Callback_t callBack )
{
	m_openCallback = callBack;
}

void DropDownMenu::ApplySettings( KeyValues* inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnDropButton" ) );
	if ( m_pButton )
	{
		if ( !HasFocus() && !m_pButton->HasFocus() )
		{
			m_pButton->NavigateFrom( );
		}
	}

	const char* command = inResourceData->GetString( "command", NULL );
	if ( command )
	{
		SetFlyout( command );
	}

	if ( m_hCurrentFlyout.Get() )
	{
		m_pnlBackground = m_hCurrentFlyout->FindChildByName( "PnlBackground" );
	}

	if ( m_pButton )
	{
		int x, y, wide, tall;
		int newTall = m_pButton->GetTall();
		GetBounds( x, y, wide, tall );
		// move the y up so the control stays up when we make it taller
		SetBounds( x, y - (newTall-tall)/2, wide, newTall );
	}
}

void DropDownMenu::OnKeyCodePressed( vgui::KeyCode code )
{
	int userId = GetJoystickForCode( code );
	vgui::KeyCode basecode = GetBaseButtonCode( code );

	int active_userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( userId != active_userId || userId < 0 )
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
			if ( m_SelectedTextEnabled )
			{
				ChangeSelection( SELECT_NEXT );
				return;
			}

			break;
		}

	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XBUTTON_LEFT_SHOULDER:
	case KEY_LEFT:
		{
			if ( m_SelectedTextEnabled )
			{
				ChangeSelection( SELECT_PREV );
				return;
			}

			break;
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

void DropDownMenu::OnMouseWheeled(int delta)
{
	if ( delta > 0 )
	{
		if ( m_SelectedTextEnabled )
		{
			ChangeSelection( SELECT_NEXT );
		}
	}
	else if ( delta < 0 )
	{
		if ( m_SelectedTextEnabled )
		{
			ChangeSelection( SELECT_PREV );
		}
	}
}