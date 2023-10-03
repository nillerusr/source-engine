#include "cbase.h"
#include "nb_select_campaign_entry.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_bitmapbutton.h"
#include "KeyValues.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "filesystem.h"
#include "nb_select_campaign_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Campaign_Entry::CNB_Select_Campaign_Entry( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pImage = new CBitmapButton( this, "Image", "" );
	m_pName = new vgui::Label( this, "Name", "" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_pImage->AddActionSignalTarget( this );
	m_pImage->SetCommand( "CampaignClicked" );

	m_nCampaignIndex = -1;
	m_szCampaignName[0] = 0;
}

CNB_Select_Campaign_Entry::~CNB_Select_Campaign_Entry()
{

}

void CNB_Select_Campaign_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_campaign_entry.res" );
	m_szCampaignName[0] = 0;
}

void CNB_Select_Campaign_Entry::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Select_Campaign_Entry::OnThink()
{
	BaseClass::OnThink();

	IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;

	// TODO: If voting, then use:
	//IASW_Mission_Chooser_Source *pSource = GetVotingMissionSource();

	if ( !pSource )
	{
		//Warning( "Unable to select a mission as couldn't find an IASW_Mission_Chooser_Source\n" );
		return;
	}

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;

	ASW_Mission_Chooser_Mission *pMission = pSource->GetCampaign( m_nCampaignIndex );
	if ( pMission )
	{
		if ( Q_strcmp( pMission->m_szMissionName, m_szCampaignName ) )
		{			
			Q_snprintf(m_szCampaignName, sizeof(m_szCampaignName), "%s", pMission->m_szMissionName);

			if (m_szCampaignName[0] == '\0')
			{
				m_pName->SetText( "" );
				m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
				return;
			}

			KeyValues *pKeys = pSource->GetCampaignDetails( m_szCampaignName );

			if ( !pKeys )
			{
				m_pName->SetText( m_szCampaignName );
				if (m_pImage)
				{
					m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
					m_pImage->SetVisible( true );
				}
			}
			else
			{
				const char *szTitle = pKeys->GetString("CampaignName");
				if (szTitle[0] == '\0')
					m_pName->SetText( m_szCampaignName );
				else
					m_pName->SetText( szTitle );
				if (m_pImage)
				{
					const char *szImage = pKeys->GetString("ChooseCampaignTexture");
					if (szImage[0] == '\0')
					{
						//Msg("setting image for %s to unknown pic\n", m_m_szMissionName);
						m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
					}
					else
					{
						//Msg("setting image for %s to %s\n", m_m_szMissionName, szImage);
						char imagename[MAX_PATH];
						Q_snprintf( imagename, sizeof(imagename), "vgui/%s", szImage );
						m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, imagename, white );
						m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, imagename, white );
						m_pImage->SetImage( CBitmapButton::BUTTON_PRESSED, imagename, white );
					}
					m_pImage->SetVisible(true);
				}
			}
		}
	}
	else
	{
		m_pName->SetText( "" );
		m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
	}
}

void CNB_Select_Campaign_Entry::OnCommand( const char *command )
{
	if ( !Q_stricmp( "CampaignClicked", command ) )
	{
		IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
		if ( pSource )
		{
			ASW_Mission_Chooser_Mission *pCampaign = pSource->GetCampaign( m_nCampaignIndex );
			if ( pCampaign )
			{
				CNB_Select_Campaign_Panel *pPanel = dynamic_cast<CNB_Select_Campaign_Panel*>( GetParent()->GetParent()->GetParent() );
				if ( pPanel )
				{
					pPanel->CampaignSelected( pCampaign );
				}
			}
		}
	}
}