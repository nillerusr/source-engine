#include "cbase.h"
#include "nb_select_mission_entry.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_bitmapbutton.h"
#include "KeyValues.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "filesystem.h"
#include "nb_select_mission_panel.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Mission_Entry::CNB_Select_Mission_Entry( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pImage = new CBitmapButton( this, "Image", "" );
	m_pName = new vgui::Label( this, "Name", "" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_pImage->AddActionSignalTarget( this );
	m_pImage->SetCommand( "MissionClicked" );

	m_nMissionIndex = -1;
	m_szMissionName[0] = 0;
	m_flZoom = 0.0f;
}

CNB_Select_Mission_Entry::~CNB_Select_Mission_Entry()
{

}

void CNB_Select_Mission_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_mission_entry.res" );
	m_szMissionName[0] = 0;
}

void CNB_Select_Mission_Entry::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Select_Mission_Entry::OnThink()
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

	ASW_Mission_Chooser_Mission *pMission = pSource->GetMission( m_nMissionIndex, true );
	if ( pMission )
	{
		if ( Q_strcmp( pMission->m_szMissionName, m_szMissionName ) )
		{			
			Q_snprintf(m_szMissionName, sizeof(m_szMissionName), "%s", pMission->m_szMissionName);

			if (m_szMissionName[0] == '\0')
			{
				m_pName->SetText( "" );
				m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
				return;
			}

			// strip off the extension
			char stripped[MAX_PATH];
			V_StripExtension( m_szMissionName, stripped, MAX_PATH );

			KeyValues *pMissionKeys = new KeyValues( stripped );

			char tempfile[MAX_PATH];
			Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", stripped );

			bool bNoOverview = false;
			if ( !pMissionKeys->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
			{
				// try to load it directly from the maps folder
				Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.txt", stripped );
				if ( !pMissionKeys->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
				{
					bNoOverview = true;
				}
			}
			if (bNoOverview)
			{
				m_pName->SetText(stripped);
				if (m_pImage)
				{
					//Msg("setting image for %s to unknown pic\n", m_m_szMissionName);
					m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
					m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/swarm/MissionPics/UnknownMissionPic", white );
					m_pImage->SetImage( CBitmapButton::BUTTON_PRESSED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
					m_pImage->SetVisible( true );
				}
			}
			else
			{
				const char *szTitle = pMissionKeys->GetString("missiontitle");
				if (szTitle[0] == '\0')
					m_pName->SetText( stripped );
				else
					m_pName->SetText( szTitle );
				if (m_pImage)
				{
					const char *szImage = pMissionKeys->GetString("image");
					if (szImage[0] == '\0')
					{
						//Msg("setting image for %s to unknown pic\n", m_m_szMissionName);
						m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
						m_pImage->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/swarm/MissionPics/UnknownMissionPic", white );
						m_pImage->SetImage( CBitmapButton::BUTTON_PRESSED, "vgui/swarm/MissionPics/UnknownMissionPic", white );
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

	float flTargetZoom = m_pImage->IsCursorOver() ? 1.0f : 0;
	if ( flTargetZoom == 1.0f && m_flZoom == 0.0f )
	{
		vgui::surface()->PlaySound( "UI/menu_focus.wav" );
	}
	m_flZoom = Approach( flTargetZoom, m_flZoom, gpGlobals->frametime * 10.0f );

	float flZoom = 1.0f; // m_flZoom;
	float flZoomDistX = YRES( 10 );
	float flZoomDistY = YRES( 7 );
	int x = flZoomDistX - flZoomDistX * flZoom;
	int y = YRES( 34 ) + flZoomDistY - flZoomDistY * flZoom;
	int w = YRES( 133 ) - ( flZoomDistX * 2 ) + ( flZoomDistX * 2 * flZoom );
	int h = YRES( 100 ) - ( flZoomDistY * 2 ) + ( flZoomDistY * 2 * flZoom );
	// 		"xpos"		"0"
	// 		"ypos"		"34"
	// 		"wide"		"133"
	// 		"tall"		"100"

	if ( flTargetZoom == 1.0f )
	{
		m_pName->SetFgColor( Color( 255, 255, 255, 255 ) );
	}
	else
	{
		m_pName->SetFgColor( Color( 83, 148, 192, 255 ) );
	}

	m_pImage->SetBounds( x, y, w, h );
}

void CNB_Select_Mission_Entry::OnCommand( const char *command )
{
	if ( !Q_stricmp( "MissionClicked", command ) )
	{
		IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
		if ( pSource )
		{
			ASW_Mission_Chooser_Mission *pMission = pSource->GetMission( m_nMissionIndex, true );
			if ( pMission )
			{
				CNB_Select_Mission_Panel *pPanel = dynamic_cast<CNB_Select_Mission_Panel*>( GetParent()->GetParent()->GetParent() );
				if ( pPanel )
				{
					pPanel->MissionSelected( pMission );
				}
			}
		}
	}
}