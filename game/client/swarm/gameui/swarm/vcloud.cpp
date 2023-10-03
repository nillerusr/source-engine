//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VCloud.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "materialsystem/materialsystem_config.h"
#include "ConfigManager.h"
#include "cdll_util.h"
#include "nb_header_footer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
Cloud::Cloud(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose(true);

	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetFooterEnabled( true );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 120, 160 );

	m_drpCloud = NULL;

	m_btnCancel = NULL;
}

//=============================================================================
Cloud::~Cloud()
{
	GameUI().AllowEngineHideGameUI();
}

//=============================================================================
void Cloud::Activate()
{
	BaseClass::Activate();

	if ( m_drpCloud )
	{
		CGameUIConVarRef cl_cloud_settings( "cl_cloud_settings" );

		int iCloudSettings = cl_cloud_settings.GetInt();

		if ( iCloudSettings == -1 || iCloudSettings != 0 )
		{
			m_bCloudEnabled = true;
			m_drpCloud->SetCurrentSelection( "SteamCloudEnabled" );
		}
		else
		{
			m_bCloudEnabled = false;
			m_drpCloud->SetCurrentSelection( "SteamCloudDisabled" );
		}

		FlyoutMenu *pFlyout = m_drpCloud->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	UpdateFooter();

	if ( m_drpCloud )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}

		m_drpCloud->NavigateTo();
		m_ActiveControl = m_drpCloud;
	}
}

void Cloud::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );
	}
}

void Cloud::OnThink()
{
	BaseClass::OnThink();

	bool needsActivate = false;

	if( !m_drpCloud )
	{
		m_drpCloud = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpCloud" ) );
		needsActivate = true;
	}

// 	if( !m_btnCancel )
// 	{
// 		m_btnCancel = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnCancel" ) );
// 		needsActivate = true;
// 	}

	if( needsActivate )
	{
		Activate();
	}
}

void Cloud::OnKeyCodePressed(KeyCode code)
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
void Cloud::OnCommand(const char *command)
{
	if( Q_stricmp( "SteamCloudEnabled", command ) == 0 )
	{
		m_bCloudEnabled = true;
	}
	else if( Q_stricmp( "SteamCloudDisabled", command ) == 0 )
	{
		m_bCloudEnabled = false;
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

void Cloud::OnNotifyChildFocus( vgui::Panel* child )
{
}

void Cloud::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter();
}

void Cloud::OnFlyoutMenuCancelled()
{
}

//=============================================================================
Panel* Cloud::NavigateBack()
{
	CGameUIConVarRef cl_cloud_settings( "cl_cloud_settings" );
	cl_cloud_settings.SetValue( ( ( m_bCloudEnabled ) ? ( STEAMREMOTESTORAGE_CLOUD_CONFIG | STEAMREMOTESTORAGE_CLOUD_SPRAY ) : ( 0 ) ) );

	return BaseClass::NavigateBack();
}

void Cloud::PaintBackground()
{
	//BaseClass::DrawDialogBackground( "#L4D360UI_Cloud_Title", NULL, "#L4D360UI_Cloud_Subtitle", NULL, NULL, true );
}

void Cloud::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();
}
