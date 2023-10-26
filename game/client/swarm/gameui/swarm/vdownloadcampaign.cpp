//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VDownloadCampaign.h"

#include "EngineInterface.h"

#include "ConfigManager.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"

#include "VHybridButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
DownloadCampaign::DownloadCampaign( Panel *parent, const char *panelName )
 : BaseClass( parent, panelName, true, false, false )
{
	SetProportional( true );

	SetDeleteSelfOnClose( true );
	SetLowerGarnishEnabled( false );
	SetMoveable( false );

	m_fromLobby = false;
	m_bDownload = true;
}

//=============================================================================
DownloadCampaign::~DownloadCampaign()
{
}

//=============================================================================
void DownloadCampaign::UpdateText()
{
	wchar_t s1[MAX_PATH];
	wchar_t s2[MAX_PATH];
	wchar_t text[MAX_PATH];

	g_pVGuiLocalize->ConvertANSIToUnicode( m_campaignName.Get(), s1, sizeof( s1 ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( m_author.Get(), s2, sizeof( s2 ) );
	const wchar_t * downloadCampaignText = g_pVGuiLocalize->Find( "#L4D360UI_DownloadCampaign_Campaign" );
	if ( downloadCampaignText )
	{
		g_pVGuiLocalize->ConstructString( text, sizeof( text ), downloadCampaignText, 2, s1, s2 );

		vgui::Label	*LblDownloadCampaign = dynamic_cast< vgui::Label * > ( FindChildByName( "LblDownloadCampaign" ) );
		if ( LblDownloadCampaign )
		{
			LblDownloadCampaign->SetText( text );
		}
	}

	g_pVGuiLocalize->ConvertANSIToUnicode( m_webSite.Get(), s1, sizeof( s1 ) );
	BaseModHybridButton *BtnURL = dynamic_cast< BaseModHybridButton * > ( FindChildByName( "BtnURL" ) );
	if ( BtnURL )
	{
		BtnURL->SetText( s1 );
	}

	if ( m_fromLobby )
	{
		vgui::Label	*LblDownloadText = dynamic_cast< vgui::Label * > ( FindChildByName( "LblDownloadText" ) );
		if ( LblDownloadText )
		{
			LblDownloadText->SetText( "#L4D360UI_DownloadCampaign_Text_FromLobby" );
		}
	}
}

//=============================================================================
void DownloadCampaign::SetDataSettings( KeyValues *pSettings )
{
	m_campaignName = pSettings->GetString( "game/missioninfo/displaytitle", "#L4D360UI_CampaignName_Unknown" ); 
	m_author =       pSettings->GetString( "game/missioninfo/author", "" );
	m_webSite =      pSettings->GetString( "game/missioninfo/website", "" );

	char const *szFrom = pSettings->GetString( "game/missioninfo/from", "" );
	m_fromLobby = !Q_stricmp( szFrom, "lobby" );

	char const *szAction = pSettings->GetString( "game/missioninfo/action", "" );
	m_bDownload = !!Q_stricmp( szAction, "website" );

	UpdateText();
}

//=============================================================================
void DownloadCampaign::ApplySchemeSettings( vgui::IScheme * pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Load the proper control settings file
	char const *szSettingsFile = "DownloadCampaign";
	if ( !m_bDownload )
		szSettingsFile = "VisitCampaignWebsite";

	char szPath[MAX_PATH];
	V_snprintf( szPath, sizeof( szPath ), "Resource/UI/BaseModUI/%s.res", szSettingsFile );

	LoadControlSettings( szPath );

	UpdateText();
}

//=============================================================================
void DownloadCampaign::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

//=============================================================================
void DownloadCampaign::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "Continue" ) == 0 )
	{
		CUtlString dest;

		if ( V_strnicmp( "http:", m_webSite.Get(), 4 ) != 0 || V_strnicmp( "https:", m_webSite.Get(), 4 ) != 0 )
		{
			dest = "http://";
			dest += m_webSite;
		}
		else
		{
			dest = m_webSite;
		}

		system()->ShellExecute("open", dest.Get() );
		NavigateBack();
	} 
	else if ( Q_stricmp( command, "Back" ) == 0 )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
		NavigateBack();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


