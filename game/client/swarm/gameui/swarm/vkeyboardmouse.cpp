//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VKeyboardMouse.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "materialsystem/materialsystem_config.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


int GetScreenAspectMode( int width, int height );


//=============================================================================
KeyboardMouse::KeyboardMouse(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose(true);

	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_btnEditBindings = NULL;
	m_drpMouseYInvert = NULL;
	m_drpMouseFilter = NULL;
	m_sldMouseSensitivity = NULL;
	m_drpDeveloperConsole = NULL;
	m_drpGamepadEnable = NULL;
	m_sldGamepadHSensitivity = NULL;
	m_sldGamepadVSensitivity = NULL;
	m_drpGamepadYInvert = NULL;
	m_drpGamepadSwapSticks = NULL;

	m_btnCancel = NULL;
}

//=============================================================================
KeyboardMouse::~KeyboardMouse()
{
	GameUI().AllowEngineHideGameUI();
	UpdateFooter( false );
}

//=============================================================================
void KeyboardMouse::Activate()
{
	BaseClass::Activate();

	if ( m_drpMouseYInvert )
	{
		CGameUIConVarRef m_pitch("m_pitch");

		if ( m_pitch.GetFloat() > 0.0f )
		{
			m_drpMouseYInvert->SetCurrentSelection( "MouseYInvertDisabled" );
		}
		else
		{
			m_drpMouseYInvert->SetCurrentSelection( "MouseYInvertEnabled" );
		}

		FlyoutMenu *pFlyout = m_drpMouseYInvert->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpMouseFilter )
	{
		CGameUIConVarRef m_filter("m_filter");

		if ( !m_filter.GetBool() )
		{
			m_drpMouseFilter->SetCurrentSelection( "MouseFilterDisabled" );
		}
		else
		{
			m_drpMouseFilter->SetCurrentSelection( "MouseFilterEnabled" );
		}

		FlyoutMenu *pFlyout = m_drpMouseFilter->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_sldMouseSensitivity )
	{
		m_sldMouseSensitivity->Reset();
	}

	if ( m_drpDeveloperConsole )
	{
		CGameUIConVarRef con_enable("con_enable");

		if ( !con_enable.GetBool() )
		{
			m_drpDeveloperConsole->SetCurrentSelection( "DeveloperConsoleDisabled" );
		}
		else
		{
			m_drpDeveloperConsole->SetCurrentSelection( "DeveloperConsoleEnabled" );
		}

		FlyoutMenu *pFlyout = m_drpDeveloperConsole->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	CGameUIConVarRef joystick("joystick");

	if ( m_drpGamepadEnable )
	{
		if ( !joystick.GetBool() )
		{
			m_drpGamepadEnable->SetCurrentSelection( "GamepadDisabled" );
		}
		else
		{
			m_drpGamepadEnable->SetCurrentSelection( "GamepadEnabled" );
		}

		FlyoutMenu *pFlyout = m_drpGamepadEnable->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_sldGamepadHSensitivity )
	{
		m_sldGamepadHSensitivity->Reset();

		m_sldGamepadHSensitivity->SetEnabled( joystick.GetBool() );
	}

	if ( m_sldGamepadVSensitivity )
	{
		m_sldGamepadVSensitivity->Reset();

		m_sldGamepadVSensitivity->SetEnabled( joystick.GetBool() );
	}

	if ( m_drpGamepadYInvert )
	{
		CGameUIConVarRef joy_inverty("joy_inverty");

		if ( !joy_inverty.GetBool() )
		{
			m_drpGamepadYInvert->SetCurrentSelection( "GamepadYInvertDisabled" );
		}
		else
		{
			m_drpGamepadYInvert->SetCurrentSelection( "GamepadYInvertEnabled" );
		}

		m_drpGamepadYInvert->SetEnabled( joystick.GetBool() );

		FlyoutMenu *pFlyout = m_drpGamepadYInvert->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpGamepadSwapSticks )
	{
		CGameUIConVarRef joy_movement_stick("joy_movement_stick");

		if ( !joy_movement_stick.GetBool() )
		{
			m_drpGamepadSwapSticks->SetCurrentSelection( "GamepadSwapSticksDisabled" );
		}
		else
		{
			m_drpGamepadSwapSticks->SetCurrentSelection( "GamepadSwapSticksEnabled" );
		}

		m_drpGamepadSwapSticks->SetEnabled( joystick.GetBool() );

		FlyoutMenu *pFlyout = m_drpGamepadSwapSticks->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	UpdateFooter( true );

	if ( m_btnEditBindings )
	{
		if ( m_ActiveControl )
			m_ActiveControl->NavigateFrom( );
		m_btnEditBindings->NavigateTo();
		m_ActiveControl = m_btnEditBindings;
	}
}

void KeyboardMouse::UpdateFooter( bool bEnableCloud )
{
	if ( !BaseModUI::CBaseModPanel::GetSingletonPtr() )
		return;

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );

		footer->SetShowCloud( bEnableCloud );
	}
}

void KeyboardMouse::OnThink()
{
	BaseClass::OnThink();

	bool needsActivate = false;

	if( !m_btnEditBindings )
	{
		m_btnEditBindings = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnEditBindings" ) );
		needsActivate = true;
	}

	if( !m_drpMouseYInvert )
	{
		m_drpMouseYInvert = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMouseYInvert" ) );
		needsActivate = true;
	}

	if( !m_drpMouseFilter )
	{
		m_drpMouseFilter = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMouseFilter" ) );
		needsActivate = true;
	}

	if( !m_sldMouseSensitivity )
	{
		m_sldMouseSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldMouseSensitivity" ) );
		needsActivate = true;
	}

	if( !m_drpDeveloperConsole )
	{
		m_drpDeveloperConsole = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpDeveloperConsole" ) );
		needsActivate = true;
	}

	if( !m_drpGamepadEnable )
	{
		m_drpGamepadEnable = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGamepadEnable" ) );
		needsActivate = true;
	}

	if( !m_sldGamepadHSensitivity )
	{
		m_sldGamepadHSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldGamepadHSensitivity" ) );
		needsActivate = true;
	}

	if( !m_sldGamepadVSensitivity )
	{
		m_sldGamepadVSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldGamepadVSensitivity" ) );
		needsActivate = true;
	}

	if( !m_drpGamepadYInvert )
	{
		m_drpGamepadYInvert = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGamepadYInvert" ) );
		needsActivate = true;
	}

	if( !m_drpGamepadSwapSticks )
	{
		m_drpGamepadSwapSticks = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGamepadSwapSticks" ) );
		needsActivate = true;
	}

	if( !m_btnCancel )
	{
		m_btnCancel = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnCancel" ) );
		needsActivate = true;
	}

	if( needsActivate )
	{
		Activate();
	}
}

void KeyboardMouse::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// Ready to write that data... go ahead and nav back
		BaseClass::OnKeyCodePressed(code);
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

//=============================================================================
void KeyboardMouse::OnCommand(const char *command)
{
	if( Q_stricmp( "#L4D360UI_Controller_Edit_Keys_Buttons", command ) == 0 )
	{
		FlyoutMenu::CloseActiveMenu();
		CBaseModPanel::GetSingleton().OpenKeyBindingsDialog( this );
	}
	else if( Q_stricmp( "MouseYInvertEnabled", command ) == 0 )
	{
		CGameUIConVarRef m_pitch("m_pitch");
		if ( m_pitch.GetFloat() > 0.0f )
		{
			m_pitch.SetValue( -1.0f * m_pitch.GetFloat() );
		}
	}
	else if( Q_stricmp( "MouseYInvertDisabled", command ) == 0 )
	{
		CGameUIConVarRef m_pitch("m_pitch");
		if ( m_pitch.GetFloat() < 0.0f )
		{
			m_pitch.SetValue( -1.0f * m_pitch.GetFloat() );
		}
	}
	else if( Q_stricmp( "MouseFilterEnabled", command ) == 0 )
	{
		CGameUIConVarRef m_filter("m_filter");
		m_filter.SetValue( true );
	}
	else if( Q_stricmp( "MouseFilterDisabled", command ) == 0 )
	{
		CGameUIConVarRef m_filter("m_filter");
		m_filter.SetValue( false );
	}
	else if( Q_stricmp( "DeveloperConsoleEnabled", command ) == 0 )
	{
		CGameUIConVarRef con_enable("con_enable");
		con_enable.SetValue( true );
	}
	else if( Q_stricmp( "DeveloperConsoleDisabled", command ) == 0 )
	{
		CGameUIConVarRef con_enable("con_enable");
		con_enable.SetValue( false );
	}
	else if( Q_stricmp( "GamepadEnabled", command ) == 0 )
	{
		CGameUIConVarRef joystick("joystick");

		if( joystick.GetBool() == false )
		{
			// The joystick is being enabled, and this is a state change
			// rather than a redundant execution of this code. Enable
			// the gamepad controls by execing a config file.
			if ( IsPC() )
			{
				engine->ClientCmd_Unrestricted( "exec 360controller_pc.cfg" );
			}
			else if ( IsX360() )
			{
				engine->ClientCmd_Unrestricted( "exec 360controller_xbox.cfg" );
			}
		}

		joystick.SetValue( true );

		SetControlEnabled( "SldGamepadHSensitivity", true );
		SetControlEnabled( "SldGamepadVSensitivity", true );
		SetControlEnabled( "DrpGamepadYInvert", true );
		SetControlEnabled( "DrpGamepadSwapSticks", true );
	}
	else if( Q_stricmp( "GamepadDisabled", command ) == 0 )
	{
		CGameUIConVarRef joystick("joystick");

		if( joystick.GetBool() == true )
		{
			// The gamepad is being disabled, and this is a state change
			// rather than a redundant execution of this code. Disable
			// the gamepad by execing a config file.
			char const *szConfigFile = "exec undo360controller.cfg";
			engine->ClientCmd_Unrestricted(szConfigFile);
		}

		joystick.SetValue( false );

		SetControlEnabled( "SldGamepadHSensitivity", false );
		SetControlEnabled( "SldGamepadVSensitivity", false );

		if ( m_drpGamepadYInvert )
		{
			m_drpGamepadYInvert->CloseDropDown();
			m_drpGamepadYInvert->SetEnabled( false );
		}

		if ( m_drpGamepadSwapSticks )
		{
			m_drpGamepadSwapSticks->CloseDropDown();
			m_drpGamepadSwapSticks->SetEnabled( false );
		}
	}
	else if( Q_stricmp( "GamepadYInvertEnabled", command ) == 0 )
	{
		CGameUIConVarRef joy_inverty("joy_inverty");
		joy_inverty.SetValue( true );
	}
	else if( Q_stricmp( "GamepadYInvertDisabled", command ) == 0 )
	{
		CGameUIConVarRef joy_inverty("joy_inverty");
		joy_inverty.SetValue( false );
	}
	else if( Q_stricmp( "GamepadSwapSticksEnabled", command ) == 0 )
	{
		CGameUIConVarRef joy_movement_stick("joy_movement_stick");
		joy_movement_stick.SetValue( true );
	}
	else if( Q_stricmp( "GamepadSwapSticksDisabled", command ) == 0 )
	{
		CGameUIConVarRef joy_movement_stick("joy_movement_stick");
		joy_movement_stick.SetValue( false );
	}
	else if( Q_stricmp( "Back", command ) == 0 )
	{
		OnKeyCodePressed( KEY_XBUTTON_B );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void KeyboardMouse::OnNotifyChildFocus( vgui::Panel* child )
{
}

void KeyboardMouse::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter( true );
}

void KeyboardMouse::OnFlyoutMenuCancelled()
{
}

//=============================================================================
Panel* KeyboardMouse::NavigateBack()
{
	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );

	return BaseClass::NavigateBack();
}

void KeyboardMouse::PaintBackground()
{
	BaseClass::DrawDialogBackground( "#L4D360UI_KeyboardMouse", NULL, "#L4D360UI_Controller_Desc", NULL, NULL, true );
}

void KeyboardMouse::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();
}
