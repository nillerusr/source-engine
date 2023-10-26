//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VControllerOptions.h"
#include "VFooterPanel.h"
#include "VSliderControl.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "VHybridButton.h"
#include "VGenericConfirmation.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


#define DUCK_MODE_HOLD		0
#define DUCK_MODE_TOGGLE	1

const char *pszDuckModes[2] =
{
	"#L4D360UI_Controller_Hold",
	"#L4D360UI_Controller_Toggle"
};

#define CONTROLLER_LOOK_TYPE_NORMAL		0
#define CONTROLLER_LOOK_TYPE_INVERTED	1

const char *pszLookTypes[2] =
{
	"#L4D360UI_Controller_Normal",
	"#L4D360UI_Controller_Inverted"
};

const char *pszButtonSettingsDisplayName[4] =
{
	"#L4D360UI_Controller_Buttons_Config1",
	"#L4D360UI_Controller_Buttons_Config2",
	"#L4D360UI_Controller_Buttons_Config3",
	"#L4D360UI_Controller_Buttons_Config4"
};

const char *pszStickSettingsDisplayName[4] = 
{
	"#L4D360UI_Controller_Sticks_Default",
	"#L4D360UI_Controller_Sticks_Southpaw",
	"#L4D360UI_Controller_Sticks_Legacy",
	"#L4D360UI_Controller_Sticks_Legacy_Southpaw"
};

static ControllerOptions *s_pControllerOptions = NULL;

//=============================================================================
ControllerOptions::ControllerOptions(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose(true);

	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_pVerticalSensitivity = NULL;
	m_pHorizontalSensitivity = NULL;
	m_pLookType = NULL;
	m_pDuckMode = NULL;
	m_pEditButtons = NULL;
	m_pEditSticks = NULL;

	m_Title[0] = 0;

	m_bDirty = false;
	m_bNeedsActivate = false;

	m_nResetControlValuesTicks = -1;

	UpdateFooter();
}

//=============================================================================
ControllerOptions::~ControllerOptions()
{
}

void ControllerOptions::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, IsPC() ? true : false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );
	}
}	


//=============================================================================
void ControllerOptions::Activate()
{
	BaseClass::Activate();

	m_iActiveUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	int iActiveController = XBX_GetUserId( m_iActiveUserSlot );

	SetGameUIActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

#if defined ( _X360 )
	vgui::Panel * firstPanel = FindChildByName( "BtnEditButtons" );
	if ( firstPanel )
	{
		if ( m_ActiveControl )
			m_ActiveControl->NavigateFrom( );
		firstPanel->NavigateTo();
	}
#endif

	wchar_t *pwcTemplate = g_pVGuiLocalize->Find( "#L4D360UI_Controller_Title" );
	if ( pwcTemplate )
	{
		wchar_t wGamerTag[32];
		const char *pszPlayerName = BaseModUI::CUIGameData::Get()->GetLocalPlayerName( iActiveController );
		g_pVGuiLocalize->ConvertANSIToUnicode( pszPlayerName, wGamerTag, sizeof( wGamerTag ) );
		g_pVGuiLocalize->ConstructString( m_Title, sizeof( m_Title ), pwcTemplate, 1, wGamerTag );
	}

	ResetControlValues();

	if ( m_bNeedsActivate )
	{
		m_bDirty = false;
		m_bNeedsActivate = false;
	}

	UpdateFooter();
}

//=============================================================================
void ControllerOptions::ResetControlValues( void )
{
	// labels for button config and stick config
	CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" );
	int iButtonSetting = clamp( joy_cfg_preset.GetInt(), 0, 3 );
	if ( m_pEditButtons )
	{
		m_pEditButtons->SetDropdownSelection( pszButtonSettingsDisplayName[iButtonSetting] );
	}

	CGameUIConVarRef joy_movement_stick("joy_movement_stick");
	int iStickSetting = ( joy_movement_stick.GetInt() > 0 ) ? 1 : 0;
	if ( m_pEditSticks )
	{
		static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
		if ( s_joy_legacy.IsValid() && s_joy_legacy.GetBool() )
		{
			iStickSetting += 2; // Go to the legacy version of the default/southpaw string.
		}

		m_pEditSticks->SetDropdownSelection( pszStickSettingsDisplayName[iStickSetting] );
	}

	if( m_pVerticalSensitivity )
	{
		m_pVerticalSensitivity->Reset();
	}

	if( m_pHorizontalSensitivity )
	{
		m_pHorizontalSensitivity->Reset();
	}

	if( m_pLookType )
	{
		m_pLookType->SetFlyout( "FlmLookType" );

		CGameUIConVarRef joy_inverty("joy_inverty");

		int iInvert = ( joy_inverty.GetInt() > 0 ) ? CONTROLLER_LOOK_TYPE_INVERTED : CONTROLLER_LOOK_TYPE_NORMAL;
		m_pLookType->SetCurrentSelection( pszLookTypes[iInvert] );

		FlyoutMenu *pFlyout = m_pLookType->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if( m_pDuckMode )
	{
		m_pDuckMode->SetFlyout( "FlmDuckMode" );

		CGameUIConVarRef option_duck_method("option_duck_method");

		int iDuckMode = ( option_duck_method.GetInt() > 0 ) ? DUCK_MODE_TOGGLE : DUCK_MODE_HOLD;
		m_pDuckMode->SetCurrentSelection( pszDuckModes[iDuckMode] );

		FlyoutMenu *pFlyout = m_pDuckMode->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}
}

//=============================================================================
void ControllerOptions::OnThink()
{
	BaseClass::OnThink();
	m_bNeedsActivate = false;

	if( !m_pVerticalSensitivity )
	{
		m_pVerticalSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldVertSens" ) );
		m_bNeedsActivate = true;
	}

	if( !m_pHorizontalSensitivity )
	{
		m_pHorizontalSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldHorizSens" ) );
		m_bNeedsActivate = true;
	}

	if( !m_pLookType )
	{
		m_pLookType = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpLookType" ) );
		m_bNeedsActivate = true;
	}

	if( !m_pDuckMode )
	{
		m_pDuckMode = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpDuckMode" ) );
		m_bNeedsActivate = true;
	}

	if ( !m_pEditButtons  )
	{
		m_pEditButtons = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnEditButtons" ) );
	}

	if ( !m_pEditSticks  )
	{
		m_pEditSticks = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnEditSticks" ) );
	}

	if( m_bNeedsActivate )
	{
		Activate();
	}

	if ( m_nResetControlValuesTicks >= 0 )
	{
		if ( m_nResetControlValuesTicks > 0 )
		{
			m_nResetControlValuesTicks--;
		}
		else
		{
			ResetControlValues();
			m_nResetControlValuesTicks = -1;
		}
	}
}

//=============================================================================
void ControllerOptionsResetDefaults_Confirm( void )
{
	s_pControllerOptions->ResetToDefaults();
}

//=============================================================================
void ControllerOptions::ResetToDefaults( void )
{
	int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
	engine->SetActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

	engine->ExecuteClientCmd( "exec config.360.cfg" );

	engine->ExecuteClientCmd( "exec joy_preset_1.cfg" );

#ifdef _X360
	int iCtrlr = XBX_GetUserId( m_iActiveUserSlot );
	UserProfileData const &upd = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()
		->GetLocalPlayer( iCtrlr )->GetPlayerProfileData();

	int nMovementControl = ( upd.action_movementcontrol == XPROFILE_ACTION_MOVEMENT_CONTROL_L_THUMBSTICK ) ? 0 : 1;
	engine->ExecuteClientCmd( VarArgs( "joy_movement_stick %d", nMovementControl ) );

	int nYinvert = upd.yaxis;
	engine->ExecuteClientCmd( VarArgs( "joy_inverty %d", nYinvert ) );
#endif

	engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

//	ResetControlValues();
	m_nResetControlValuesTicks = 1; // used to delay polling the values until we've flushed the command buffer 

	m_bDirty = true;
}

//=============================================================================
void ControllerOptions::OnKeyCodePressed(KeyCode code)
{
	if ( m_iActiveUserSlot != CBaseModPanel::GetSingleton().GetLastActiveUserId() )
		return;

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XSTICK1_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XBUTTON_LEFT_SHOULDER:
	case KEY_XSTICK1_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XBUTTON_RIGHT_SHOULDER:
	{

		// If they want to modify buttons or sticks with a direction, take them to the edit menu
		vgui::Panel *panel = FindChildByName( "BtnEditButtons" );
		if ( panel->HasFocus() )
		{
			CBaseModPanel::GetSingleton().OpenWindow(WT_CONTROLLER_BUTTONS, this, true);
			m_bDirty = true;
		}
		else
		{
			panel = FindChildByName( "BtnEditSticks" );
			if ( panel->HasFocus() )
			{
				CBaseModPanel::GetSingleton().OpenWindow(WT_CONTROLLER_STICKS, this, true);
				m_bDirty = true;
			}
			else
			{
				BaseClass::OnKeyCodePressed(code);
			}
		}
		break;
	}

	case KEY_XBUTTON_B:
		if ( m_bDirty || 
			 ( m_pVerticalSensitivity && m_pVerticalSensitivity->IsDirty() ) || 
			 ( m_pHorizontalSensitivity && m_pHorizontalSensitivity->IsDirty() ) )
		{
			if ( !CBaseModPanel::GetSingleton().IsReadyToWriteConfig() )
			{
				// We've written data in the past 3 seconds and will fail cert if we do it again!
				// We'll have to stay in the menu a bit longer
				return;
			}
		}
		break;
	}

	BaseClass::OnKeyCodePressed(code);
}


//=============================================================================
void ControllerOptions::OnCommand(const char *command)
{
	if ( !Q_strcmp( command, "EditButtons" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_CONTROLLER_BUTTONS, this, true);
		m_bDirty = true;
	}
	else if ( !Q_strcmp( command, "EditSticks" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow(WT_CONTROLLER_STICKS, this, true);
		m_bDirty = true;
	}
	else if( !Q_strcmp( command, pszDuckModes[DUCK_MODE_HOLD] ) )
	{
		CGameUIConVarRef option_duck_method("option_duck_method");
		if ( option_duck_method.IsValid() )
		{
			option_duck_method.SetValue( DUCK_MODE_HOLD );
			m_bDirty = true;
		}
	}
	else if( !Q_strcmp( command, pszDuckModes[DUCK_MODE_TOGGLE] ) )
	{
		CGameUIConVarRef option_duck_method("option_duck_method");
		if ( option_duck_method.IsValid() )
		{
			option_duck_method.SetValue( DUCK_MODE_TOGGLE );
			m_bDirty = true;
		}
	}	
	else if( !Q_strcmp( command, pszLookTypes[CONTROLLER_LOOK_TYPE_NORMAL] ) )
	{
		CGameUIConVarRef joy_inverty("joy_inverty");
		if ( joy_inverty.IsValid() )
		{
			joy_inverty.SetValue( CONTROLLER_LOOK_TYPE_NORMAL );
			m_bDirty = true;
		}
	}
	else if( !Q_strcmp( command, pszLookTypes[CONTROLLER_LOOK_TYPE_INVERTED] ) )
	{
		CGameUIConVarRef joy_inverty("joy_inverty");
		if ( joy_inverty.IsValid() )
		{
			joy_inverty.SetValue( CONTROLLER_LOOK_TYPE_INVERTED );
			m_bDirty = true;
		}
	}
	else if ( !Q_strcmp( command, "Defaults" ) )
	{
		GenericConfirmation* confirmation = 
			static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().
			OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

		if ( confirmation )
		{
			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#L4D360UI_Controller_Default";
			data.pMessageText =	"#L4D360UI_Controller_Default_Details";
			data.bOkButtonEnabled = true;
			data.bCancelButtonEnabled = true;

			s_pControllerOptions = this;

			data.pfnOkCallback = ControllerOptionsResetDefaults_Confirm;
			data.pfnCancelCallback = NULL;

			confirmation->SetUsageData(data);
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void ControllerOptions::OnNotifyChildFocus( vgui::Panel* child )
{
}

void ControllerOptions::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter();
}

void ControllerOptions::OnFlyoutMenuCancelled()
{
}

//=============================================================================
Panel* ControllerOptions::NavigateBack()
{
	if ( m_bDirty || 
		 ( m_pVerticalSensitivity && m_pVerticalSensitivity->IsDirty() ) || 
		 ( m_pHorizontalSensitivity && m_pHorizontalSensitivity->IsDirty() ) )
	{
		// Only write the data if we haven't done it in the past 3 seconds...
		// We should never get to the nav back with dirty data and not ready, but this check will prevent any crasy edge cases
		if ( CBaseModPanel::GetSingleton().IsReadyToWriteConfig() )
		{
			engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetUserId( m_iActiveUserSlot ) ) );
		}
	}

	SetGameUIActiveSplitScreenPlayerSlot( 0 );

	return BaseClass::NavigateBack();
}

void ControllerOptions::PaintBackground()
{
	BaseClass::DrawDialogBackground( NULL, m_Title, "#L4D360UI_Controller_Desc", NULL );
}

void ControllerOptions::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();
}
