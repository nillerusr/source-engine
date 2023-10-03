//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VControllerOptionsSticks.h"
#include "VFooterPanel.h"
#include "VSliderControl.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ILocalize.h"
#include "VHybridButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
ControllerOptionsSticks::ControllerOptionsSticks(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_bActivateOnFirstThink = true;

	UpdateFooter();
}

//=============================================================================
ControllerOptionsSticks::~ControllerOptionsSticks()
{
}

void ControllerOptionsSticks::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

void ControllerOptionsSticks::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );
}

enum
{
	CONTROLLER_STICKS_NORMAL = 0,
	CONTROLLER_STICKS_SOUTHPAW,

	NUM_CONTROLLER_STICKS_SETTINGS
};

const char *pszStickSettingsButtonName[NUM_CONTROLLER_STICKS_SETTINGS] =
{
	"BtnDefault",
	"BtnSouthpaw"
};

const char *pszStickButtonNamesNormal[ NUM_CONTROLLER_STICKS_SETTINGS ] =
{
	"#L4D360UI_Controller_Sticks_Default",
	"#L4D360UI_Controller_Sticks_Southpaw"
};

const char *pszStickButtonNamesLegacy[ NUM_CONTROLLER_STICKS_SETTINGS ] =
{
	"#L4D360UI_Controller_Sticks_Legacy",
	"#L4D360UI_Controller_Sticks_Legacy_Southpaw"
};

const char ** arrSticksLegacy[2] =
{
	pszStickButtonNamesNormal,
	pszStickButtonNamesLegacy
};

//=============================================================================
void ControllerOptionsSticks::Activate()
{
	BaseClass::Activate();

	m_iActiveUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	int iActiveController = XBX_GetUserId( m_iActiveUserSlot );

	SetGameUIActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

#if defined ( _X360 )
	CGameUIConVarRef joy_movement_stick("joy_movement_stick");
	if ( joy_movement_stick.IsValid() )
	{
		int iSetting = ( joy_movement_stick.GetInt() > 0 ) ? 1 : 0;

		for ( int k = 0; k < NUM_CONTROLLER_STICKS_SETTINGS; ++ k )
		{
			if ( Panel *btn = FindChildByName( pszStickSettingsButtonName[k] ) )
				btn->NavigateFrom();
		}

		if ( Panel *btnActive = FindChildByName( pszStickSettingsButtonName[iSetting] ) )
		{
			btnActive->NavigateTo();
		}
	}
#endif

	vgui::Label *pLabel = dynamic_cast< vgui::Label * >( FindChildByName( "LblGameTitle" ) );
	if ( pLabel )
	{
		wchar_t *pwcTemplate = g_pVGuiLocalize->Find("#L4D360UI_Controller_Sticks_Title");

		if ( pwcTemplate )
		{		
			const char *pszPlayerName = BaseModUI::CUIGameData::Get()->GetLocalPlayerName( iActiveController );

			wchar_t wWelcomeMsg[128];
			wchar_t wGamerTag[32];
			g_pVGuiLocalize->ConvertANSIToUnicode( pszPlayerName, wGamerTag, sizeof( wGamerTag ) );
			g_pVGuiLocalize->ConstructString( wWelcomeMsg, sizeof( wWelcomeMsg ), pwcTemplate, 1, wGamerTag );

			pLabel->MakeReadyForUse();
			pLabel->SetText( wWelcomeMsg );
		}
	}

	UpdateFooter();
	UpdateHelpText();
	UpdateButtonNames();
}

//=============================================================================
void ControllerOptionsSticks::OnKeyCodePressed(KeyCode code)
{
	if ( m_iActiveUserSlot != CBaseModPanel::GetSingleton().GetLastActiveUserId() )
		return;

	vgui::KeyCode basecode = GetBaseButtonCode( code );

	switch( basecode )
	{
	case KEY_XBUTTON_A:
		// Nav back when the select one of the options
		BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		break;

	case KEY_XBUTTON_Y:
		// Toggle Legacy control settings.
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
			if ( s_joy_legacy.IsValid() )
			{
				s_joy_legacy.SetValue( !s_joy_legacy.GetBool() );
			}

			UpdateFooter();
			UpdateHelpText();
			UpdateButtonNames();
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
void ControllerOptionsSticks::OnThink()
{
	BaseClass::OnThink();

	// for some reason we have to delay the Activate in order to set our default button
	if ( m_bActivateOnFirstThink )
	{
		Activate();

		m_bActivateOnFirstThink = false;
	}
}

//=============================================================================
void ControllerOptionsSticks::OnHybridButtonNavigatedTo( VPANEL defaultButton )
{
	Panel *panel = ipanel()->GetPanel( defaultButton, GetModuleName() );

	if ( panel )
	{
		OnCommand( panel->GetName() );
	}
}

//=============================================================================
void ControllerOptionsSticks::OnCommand(const char *command)
{
	if( !Q_strcmp( command, pszStickSettingsButtonName[CONTROLLER_STICKS_NORMAL] ) )
	{
		CGameUIConVarRef joy_movement_stick("joy_movement_stick");
		if ( joy_movement_stick.IsValid() )
		{
			joy_movement_stick.SetValue( CONTROLLER_STICKS_NORMAL );
		}

		SetControlVisible( "Normal_Move", true );
		SetControlVisible( "Normal_Look", true );
		SetControlVisible( "Southpaw_Move", false );
		SetControlVisible( "Southpaw_Look", false );

		UpdateHelpText();
	}
	else if( !Q_strcmp( command, pszStickSettingsButtonName[CONTROLLER_STICKS_SOUTHPAW] ) )
	{
		CGameUIConVarRef joy_movement_stick("joy_movement_stick");
		if ( joy_movement_stick.IsValid() )
		{
			joy_movement_stick.SetValue( CONTROLLER_STICKS_SOUTHPAW );
		}

		SetControlVisible( "Normal_Move", false );
		SetControlVisible( "Normal_Look", false );
		SetControlVisible( "Southpaw_Move", true );
		SetControlVisible( "Southpaw_Look", true );

		UpdateHelpText();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//=============================================================================
Panel* ControllerOptionsSticks::NavigateBack()
{
	// TCR violation: engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetUserId( m_iActiveUserSlot ) ) );
	// parent dialog will write config
	return BaseClass::NavigateBack();
}

//=============================================================================
void ControllerOptionsSticks::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( !footer )
		return;

	footer->SetButtons( FB_ABUTTON|FB_YBUTTON, FF_CONTROLLER_STICKS, true );
	footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );

	// Legacy Control support hack here 
	static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
	if ( s_joy_legacy.IsValid() )
	{
		if( s_joy_legacy.GetBool() )
		{
			footer->SetButtonText( FB_YBUTTON, "#L4D360UI_LegacyOn" );
		}
		else
		{
			footer->SetButtonText( FB_YBUTTON, "#L4D360UI_LegacyOff" );
		}
	}
}

//=============================================================================
// Set descriptive tooltip text in the case of Legacy mode.
//=============================================================================
void ControllerOptionsSticks::UpdateHelpText()
{
	bool bUsingLegacy = false;
	bool bUsingSouthpaw = false;

	// Legacy Control support hack here 
	static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
	if ( s_joy_legacy.IsValid() && s_joy_legacy.GetBool() )
	{
		bUsingLegacy = true;
	}

	CGameUIConVarRef joy_movement_stick("joy_movement_stick");
	if ( joy_movement_stick.IsValid() )
	{
		bUsingSouthpaw = joy_movement_stick.GetInt() == CONTROLLER_STICKS_SOUTHPAW;
	}

	if( bUsingSouthpaw )
	{
		if( bUsingLegacy ) 
			SetHelpText( "#L4D360UI_Controller_Tooltip_Sticks_Legacy_Southpaw" );
		else
			SetHelpText( "#L4D360UI_Controller_Tooltip_Sticks_Southpaw" );
	}
	else
	{
		if( bUsingLegacy ) 
			SetHelpText( "#L4D360UI_Controller_Tooltip_Sticks_Legacy" );
		else
			SetHelpText( "#L4D360UI_Controller_Tooltip_Sticks_Default" );
	}
}

void ControllerOptionsSticks::UpdateButtonNames()
{
	CGameUIConVarRef joy_legacy("joy_legacy");
	if ( joy_legacy.IsValid() )
	{
		int iLegacy = ( joy_legacy.GetInt() > 0 ) ? 1 : 0;

		const char **arrNames = arrSticksLegacy[iLegacy];
		for ( int k = 0; k < NUM_CONTROLLER_STICKS_SETTINGS; ++ k )
		{
			if ( BaseModHybridButton *btn = dynamic_cast< BaseModHybridButton * >( FindChildByName( pszStickSettingsButtonName[k] ) ) )
				btn->SetText( arrNames[k] );
		}
	}
}

