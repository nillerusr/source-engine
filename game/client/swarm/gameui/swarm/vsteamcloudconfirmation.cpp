//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VSteamCloudConfirmation.h"

#include "EngineInterface.h"

#include "ConfigManager.h"

#include "vgui_controls/Label.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
SteamCloudConfirmation::SteamCloudConfirmation( Panel *parent, const char *panelName )
 : BaseClass( parent, panelName, true, false, false )
 , m_pSteamCloudCheckBox( 0 )
{
	SetProportional( true );

	m_pSteamCloudCheckBox = new CvarToggleCheckButton<CGameUIConVarRef>( 
		this, 
		"CheckButtonCloud", 
		"#L4D360UI_Cloud_KeepInSync_Tip", 
		"cl_cloud_settings",
		true );

	SetTitle( "", false );
	SetDeleteSelfOnClose( true );
	SetLowerGarnishEnabled( false );
	SetMoveable( false );
}

//=============================================================================
SteamCloudConfirmation::~SteamCloudConfirmation()
{
}

//=============================================================================
void SteamCloudConfirmation::OnThink()
{
	vgui::Label	*pLblOptionsAccess = dynamic_cast< vgui::Label * > ( FindChildByName( "LblOptionsAccess" ) );
	if ( m_pSteamCloudCheckBox && pLblOptionsAccess )
	{
		pLblOptionsAccess->SetVisible( !m_pSteamCloudCheckBox->IsSelected() );
	}
}

//=============================================================================
void SteamCloudConfirmation::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "OK" ) == 0 )
	{
		static CGameUIConVarRef cl_cloud_settings( "cl_cloud_settings" );

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

		if ( m_pSteamCloudCheckBox )
		{
			if ( m_pSteamCloudCheckBox->IsSelected() )
			{
				cl_cloud_settings.SetValue( STEAMREMOTESTORAGE_CLOUD_ALL );
				// Re-read the configuration
				engine->ReadConfiguration( -1, false );
			}
			else
			{
				cl_cloud_settings.SetValue( 0 );
			}
		}

		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

