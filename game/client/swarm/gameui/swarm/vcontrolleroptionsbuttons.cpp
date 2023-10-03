//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VControllerOptionsButtons.h"
#include "VFooterPanel.h"
#include "VSliderControl.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

namespace BaseModUI
{

//////////////////////////////////////////////////////////
struct ControllerBindingMap
{
	vgui::KeyCode m_keyCode;
	const char *m_pszLabelName;
};

#define NUM_BINDABLE_KEYS	10

//this array represents the "configurable" buttons on the controller
static const ControllerBindingMap sControllerBindings[NUM_BINDABLE_KEYS] =	
{
	{ KEY_XBUTTON_A,				"LblAButton" },
	{ KEY_XBUTTON_B,				"LblBButton" },
	{ KEY_XBUTTON_X,				"LblXButton" },
	{ KEY_XBUTTON_Y,				"LblYButton" },
	{ KEY_XBUTTON_LEFT_SHOULDER,	"LblLShoulder" },
	{ KEY_XBUTTON_RIGHT_SHOULDER,	"LblRShoulder" },
	{ KEY_XBUTTON_STICK1,			"LblLStick" },
	{ KEY_XBUTTON_STICK2,			"LblRStick" },
	{ KEY_XBUTTON_LTRIGGER,			"LblLTrigger" },
	{ KEY_XBUTTON_RTRIGGER,			"LblRTrigger" }
};

// Map the important bindings to the string that should be displayed on the diagram
struct BindingDisplayMap
{
	const char *pszBinding;
	const char *pszDisplay;
};

static const BindingDisplayMap sBindingToDisplay[] = 
{
	{ "+duck",					"#L4D360UI_Controller_Crouch" },
	{ "+jump",					"#L4D360UI_Controller_Jump" },
	{ "+use",					"#L4D360UI_Controller_Use" },
	{ "+attack",				"#L4D360UI_Controller_Attack" },
	{ "+attack2",				"#L4D360UI_Controller_Melee" },
	{ "+reload",				"#L4D360UI_Controller_Reload" },
	{ "lastinv",				"#L4D360UI_Controller_LastInv" },
	{ "+zoom",					"#L4D360UI_Controller_Zoom" },
	{ "+lookspin",				"#L4D360UI_Controller_Lookspin" },
	{ "vocalize smartlook",		"#L4D360UI_Controller_Vocalize" }
};


//=============================================================================
ControllerOptionsButtons::ControllerOptionsButtons(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_bActivateOnFirstThink = true;
	m_nRecalculateLabelsTicks = -1;

	UpdateFooter();
}

//=============================================================================
ControllerOptionsButtons::~ControllerOptionsButtons()
{
}

void ControllerOptionsButtons::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

void ControllerOptionsButtons::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );
}

enum
{
	CONTROLLER_BUTTONS_SPEC1 = 0,
	CONTROLLER_BUTTONS_SPEC2,
	CONTROLLER_BUTTONS_SPEC3,
	CONTROLLER_BUTTONS_SPEC4,

	NUM_CONTROLLER_BUTTONS_SETTINGS
};

const char *pszButtonSettingsButtonName[NUM_CONTROLLER_BUTTONS_SETTINGS] =
{
	"BtnSpec1",
	"BtnSpec2",
	"BtnSpec3",
	"BtnSpec4"
};

const char *pszButtonSettingConfigs[NUM_CONTROLLER_BUTTONS_SETTINGS] = 
{
	"joy_preset_1.cfg",
	"joy_preset_2.cfg",
	"joy_preset_3.cfg",
	"joy_preset_4.cfg"
};

void ControllerOptionsButtons::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON, FF_AB_ONLY, IsPC() ? true : false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
	}	
}

//=============================================================================
void ControllerOptionsButtons::Activate()
{
	BaseClass::Activate();

	m_iActiveUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	int iActiveController = XBX_GetUserId( m_iActiveUserSlot );

	SetGameUIActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

	// Figure out which button should be default

#if defined ( _X360 )
	CGameUIConVarRef joy_cfg_preset("joy_cfg_preset");
	if ( joy_cfg_preset.IsValid() )
	{
		int iPreset = clamp( joy_cfg_preset.GetInt(), 0, NUM_CONTROLLER_BUTTONS_SETTINGS-1 );

		vgui::Panel * firstPanel = FindChildByName( pszButtonSettingsButtonName[iPreset] );
		if ( firstPanel )
		{
			firstPanel->NavigateTo();
		}
	}		
#endif

	vgui::Label *pLabel = dynamic_cast< vgui::Label * >( FindChildByName( "LblGameTitle" ) );
	if ( pLabel )
	{
		wchar_t *pwcTemplate = g_pVGuiLocalize->Find("#L4D360UI_Controller_Buttons_Title");

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

	RecalculateBindingLabels();
	UpdateFooter();
}

//=============================================================================
void ControllerOptionsButtons::OnKeyCodePressed(KeyCode code)
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

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}


//=============================================================================
void ControllerOptionsButtons::RecalculateBindingLabels( void )
{
	// Populate the bindings labels with the currently bound keys

	EditablePanel *pContainer = dynamic_cast<EditablePanel *>( FindChildByName( "LabelContainer" ) );
	if ( !pContainer )
		return;

	// for every button on the controller
	for ( int i=0;i<sizeof(sControllerBindings) / sizeof( ControllerBindingMap );i++ )
	{
		// what is it bound to?
		vgui::KeyCode code = sControllerBindings[i].m_keyCode;

		//int nJoystick = m_iActiveUserSlot;
		code = ButtonCodeToJoystickButtonCode( code, m_iActiveUserSlot );

		const char *pBinding = engine->Key_BindingForKey( code );
		if ( !pBinding )
		{
			// key is not bound to anything
			pContainer->SetControlString( sControllerBindings[i].m_pszLabelName, L"<Not Bound>" );
			continue;
		}

		// find the localized string for this binding and set the label text
		for( int j=0;j<sizeof( sBindingToDisplay ) / sizeof( BindingDisplayMap );j++ )
		{
			const BindingDisplayMap *entry =  &( sBindingToDisplay[ j ] );

			if ( Q_strstr( pBinding, entry->pszBinding ) )
			{
				Label *pLabel = dynamic_cast< Label * >( pContainer->FindChildByName( sControllerBindings[i].m_pszLabelName ) );
				if ( pLabel )
				{
					pLabel->SetText( entry->pszDisplay );
				}
			}
		}
	}
}

//=============================================================================
void ControllerOptionsButtons::OnThink()
{
	BaseClass::OnThink();

	// for some reason we have to delay the Activate in order to set our default button
	if ( m_bActivateOnFirstThink )
	{
		Activate();

		m_bActivateOnFirstThink = false;
	}

	if ( m_nRecalculateLabelsTicks >= 0 )
	{
		if ( m_nRecalculateLabelsTicks > 0 )
		{
			m_nRecalculateLabelsTicks--;
		}
		else
		{
			RecalculateBindingLabels();
			m_nRecalculateLabelsTicks = -1;
		}
	}
}

//=============================================================================
void ControllerOptionsButtons::OnHybridButtonNavigatedTo( VPANEL defaultButton )
{
	Panel *panel = ipanel()->GetPanel( defaultButton, GetModuleName() );

	if ( panel )
	{
		OnCommand( panel->GetName() );
	}
}

//=============================================================================
void ControllerOptionsButtons::OnCommand(const char *command)
{
	for ( int i=0;i<NUM_CONTROLLER_BUTTONS_SETTINGS;i++ )
	{
		if( !Q_strcmp( command, pszButtonSettingsButtonName[i] ) )
		{
			CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" );
			if ( joy_cfg_preset.IsValid() )
			{
				joy_cfg_preset.SetValue( i );
			}

			int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
			engine->SetActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

			const char *pszCfg = pszButtonSettingConfigs[i];
			engine->ExecuteClientCmd( VarArgs( "exec %s", pszCfg ) );

			engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

//			RecalculateBindingLabels();
			m_nRecalculateLabelsTicks = 1; // used to delay polling the values until we've flushed the command buffer 
			return;
		}
	}

	BaseClass::OnCommand( command );
}

//=============================================================================
Panel* ControllerOptionsButtons::NavigateBack()
{
	// TCR violation: engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetUserId( m_iActiveUserSlot ) ) );
	// parent dialog will write config
	return BaseClass::NavigateBack();
}

};	// BaseModUI