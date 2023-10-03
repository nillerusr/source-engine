#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Button.h>
#include "KeyValues.h"
#include "FileSystem.h"
#include <vgui_controls/Panel.h>
#include <vgui/isurface.h>
#include "asw_mission_chooser_entry.h"
#include "asw_mission_chooser_list.h"
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Mission_Chooser_Entry::CASW_Mission_Chooser_Entry( vgui::Panel *pParent, const char *pElementName, int iChooserType, int iHostType ) :
	vgui::Panel(pParent, pElementName)
{
	m_ChooserType = iChooserType;
	m_HostType = iHostType;

	m_pImagePanel = new vgui::ImagePanel(this, "EntryImage");
	m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
	m_pImagePanel->SetShouldScaleImage(true);
	m_pImagePanel->SetVisible(false);
	m_pNameLabel = new vgui::Label(this, "EntryName", " ");
	m_pDescriptionLabel = new vgui::Label(this, "EntryDesc", " ");
	m_pDescriptionLabel->SetWrap(true);
	m_pDescriptionLabel->InvalidateLayout();
	m_pImagePanel->SetMouseInputEnabled(false);
	m_pDescriptionLabel->SetMouseInputEnabled(false);
	m_pImagePanel->SetMouseInputEnabled(false);
	SetMouseInputEnabled(true);
	m_MapKeyValues = NULL;
	m_iLabelHeight = 12;
	m_bMouseOver = false;
	Q_snprintf(m_szMapName, sizeof(m_szMapName), "INVALID");
	m_pDeleteButton = NULL;
	m_bMouseReleased = false;
	m_bVoteDisabled = false;

	if (iChooserType == ASW_CHOOSER_SAVED_CAMPAIGN && iHostType != ASW_HOST_TYPE_CALLVOTE)
	{
		m_pDeleteButton = new vgui::Button(this, "DeleteButton", "#asw_delete_save", this, "DeleteSave");
	}
}

CASW_Mission_Chooser_Entry::~CASW_Mission_Chooser_Entry()
{
}

void CASW_Mission_Chooser_Entry::OnThink()
{
	bool bCursorOver = IsCursorOver();
	if (m_pDeleteButton && m_pDeleteButton->IsCursorOver())
		bCursorOver = false;
	if (m_bMouseOver != bCursorOver)
	{
		m_bMouseOver = bCursorOver;
		if (m_bMouseOver)
		{
			if ( m_szMapName[0] != '\0' && !m_bVoteDisabled )
			{
				SetBgColor(Color(75,84,106,255));
			}
		}
		else if ( !m_bVoteDisabled )
		{
			SetBgColor(Color(0,0,0,0));
		}
	}
	if ( !vgui::input()->IsMouseDown( MOUSE_LEFT ) )
	{
		m_bMouseReleased = true;
	}
}

void CASW_Mission_Chooser_Entry::SetDetails(const char *szMapName, int nChooserType)
{
	if ( nChooserType != -1 )
	{
		m_ChooserType = nChooserType;
	}
	if ( m_ChooserType == ASW_CHOOSER_SINGLE_MISSION )
	{
		if ( Q_stricmp( szMapName, m_szMapName ) )
		{			
			Q_snprintf(m_szMapName, sizeof(m_szMapName), "%s", szMapName);

			if (szMapName[0] == '\0')
			{
				m_pNameLabel->SetText("");
				m_pDescriptionLabel->SetText("");	
				if (m_pImagePanel)
					m_pImagePanel->SetVisible(false);
				return;
			}

			// try and get the keyvalues for this mission's overview txt, can set decsription and image from that
			if ( m_MapKeyValues )
				m_MapKeyValues->deleteThis();

			// strip off the extension
			char stripped[MAX_PATH];
			V_StripExtension( szMapName, stripped, MAX_PATH );
			
			m_MapKeyValues = new KeyValues( stripped );

			char tempfile[MAX_PATH];
			Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", stripped );
			
			bool bNoOverview = false;
			if ( !m_MapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
			{
				// try to load it directly from the maps folder
				Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.txt", stripped );
				if ( !m_MapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
				{
					bNoOverview = true;
				}
			}
			SetVoteDisabled( bNoOverview );
			if (bNoOverview)
			{
				m_pNameLabel->SetText(stripped);
				m_pDescriptionLabel->SetText( "#asw_mission_not_installed" );
				if (m_pImagePanel)
				{
					//Msg("setting image for %s to unknown pic\n", m_szMapName);
					m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
					m_pImagePanel->SetVisible(true);
				}
			}
			else
			{
				const char *szTitle = m_MapKeyValues->GetString("missiontitle");
				if (szTitle[0] == '\0')
					m_pNameLabel->SetText(stripped);
				else
					m_pNameLabel->SetText(szTitle);
				m_pDescriptionLabel->SetText(m_MapKeyValues->GetString("description"));
				if (m_pImagePanel)
				{
					const char *szImage = m_MapKeyValues->GetString("image");
					if (szImage[0] == '\0')
					{
						//Msg("setting image for %s to unknown pic\n", m_szMapName);
						m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
					}
					else
					{
						//Msg("setting image for %s to %s\n", m_szMapName, szImage);
						m_pImagePanel->SetImage(szImage);
					}
					m_pImagePanel->SetVisible(true);
				}
				m_pDescriptionLabel->InvalidateLayout();
				m_pDescriptionLabel->GetTextImage()->RecalculateNewLinePositions();
			}
			InvalidateLayout( true, true );
		}
	}
	else if ( m_ChooserType == ASW_CHOOSER_CAMPAIGN )
	{
		if ( Q_stricmp( szMapName, m_szMapName ) )
		{
			Q_snprintf(m_szMapName, sizeof(m_szMapName), "%s", szMapName);

			if (szMapName[0] == '\0')
			{
				m_pNameLabel->SetText("");
				m_pDescriptionLabel->SetText("");	
				if (m_pImagePanel)
					m_pImagePanel->SetVisible(false);
				return;
			}

			// try and get the keyvalues for this mission's overview txt, can set description and image from that
			if ( m_MapKeyValues )
				m_MapKeyValues->deleteThis();

			// strip off the extension
			char stripped[MAX_PATH];
			V_StripExtension( szMapName, stripped, MAX_PATH );
			
			m_MapKeyValues = new KeyValues( stripped );

			char tempfile[MAX_PATH];
			Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", stripped );
			
			if ( !m_MapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
			{
				SetVoteDisabled( true );
				m_pNameLabel->SetText(stripped);
				m_pDescriptionLabel->SetText("#asw_campaign_not_installed");				
				if (m_pImagePanel)
				{
					m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
					m_pImagePanel->SetVisible(true);
				}
			}
			else
			{
				SetVoteDisabled( false );
				const char *szTitle = m_MapKeyValues->GetString("CampaignName");
				if (szTitle[0] == '\0')
					m_pNameLabel->SetText(stripped);
				else
					m_pNameLabel->SetText(szTitle);
				m_pDescriptionLabel->SetText(m_MapKeyValues->GetString("CampaignDescription"));
				if (m_pImagePanel)
				{
					const char * szTexture = m_MapKeyValues->GetString("ChooseCampaignTexture");					
					if (szTexture[0] == '\0')
						m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
					else
						m_pImagePanel->SetImage( szTexture );
					m_pImagePanel->SetVisible(true);
				}
				m_pDescriptionLabel->InvalidateLayout();
				m_pDescriptionLabel->GetTextImage()->RecalculateNewLinePositions();
			}
		}
	}
}

void CASW_Mission_Chooser_Entry::SetSavedCampaignDetails(ASW_Mission_Chooser_Saved_Campaign *pSaved)
{
	if (m_ChooserType != ASW_CHOOSER_SAVED_CAMPAIGN)
		return;	
	if (stricmp(pSaved->m_szSaveName, m_szMapName))
	{
		Q_snprintf(m_szMapName, sizeof(m_szMapName), "%s", pSaved->m_szSaveName);

		if (pSaved->m_szSaveName[0] == '\0')
		{
			m_pNameLabel->SetText("");
			m_pDescriptionLabel->SetText("");
			m_pImagePanel->SetVisible(false);
			if (m_pDeleteButton)
			{
				m_pDeleteButton->SetVisible(false);
				m_pDeleteButton->SetEnabled(false);
			}
		}
		else
		{						
			// strip the extension of the save, for our title
			char stripped[MAX_PATH];
			V_StripExtension( pSaved->m_szSaveName, stripped, MAX_PATH );
			m_pNameLabel->SetText(stripped);
			char buffer[512];

			// load in the campaign details
			char tempfile[MAX_PATH];
			Q_snprintf( tempfile, sizeof( tempfile ), "resource/campaigns/%s.txt", pSaved->m_szCampaignName );

			KeyValues *pKeyValues = new KeyValues(pSaved->m_szCampaignName);
			if ( pKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
			{
				Q_snprintf(buffer, sizeof(buffer), "Campaign: %s       Date: %s\nMissions Completed: %d       Players: %s",
					pKeyValues->GetString("CampaignName"), pSaved->m_szDateTime,
					pSaved->m_iMissionsComplete, pSaved->m_szPlayerNames);			
				if (m_pImagePanel)
				{
					const char * szTexture = pKeyValues->GetString("ChooseCampaignTexture");
					if (szTexture[0] == '\0')
						m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic");
					else
						m_pImagePanel->SetImage(&szTexture[5]);
					m_pImagePanel->SetVisible(true);
				}
			}
			else	// if we failed to load in the campaign txt (should never happen!), then show cut down details
			{						
				Q_snprintf(buffer, sizeof(buffer), "Campaign: %s       Date: %s\nMissions Completed: %d       Players: %s",
					pSaved->m_szCampaignName, pSaved->m_szDateTime, pSaved->m_iMissionsComplete, pSaved->m_szPlayerNames);				
				m_pImagePanel->SetVisible(false);
			}
			pKeyValues->deleteThis();
			pKeyValues = NULL;
			m_pDescriptionLabel->SetText(buffer);
			m_pDescriptionLabel->InvalidateLayout();
			m_pDescriptionLabel->GetTextImage()->RecalculateNewLinePositions();
			if (m_pDeleteButton)
			{
				m_pDeleteButton->SetVisible(true);
				m_pDeleteButton->SetEnabled(true);
			}
		}
	}
}

void CASW_Mission_Chooser_Entry::PerformLayout()
{
	BaseClass::PerformLayout();
	int image_tall = GetTall() * 0.9f;
	int image_wide = image_tall * 1.3333f;
	m_pImagePanel->SetBounds(GetTall() * 0.05f, GetTall() * 0.05f, image_wide, image_tall);
	m_pNameLabel->SetBounds(image_wide * 1.2f,GetTall() * 0.05f,GetWide() - image_wide * 1.4f, m_iLabelHeight);
	m_pDescriptionLabel->SetBounds(image_wide * 1.2f,m_iLabelHeight + GetTall() * 0.05f,GetWide() - image_wide * 1.4f, GetTall() - m_iLabelHeight);
	m_pDescriptionLabel->InvalidateLayout();
	m_pDescriptionLabel->GetTextImage()->SetSize(m_pDescriptionLabel->GetWide(), m_pDescriptionLabel->GetTall());
	if (m_pDeleteButton)
	{
		m_pDeleteButton->SizeToContents();
		m_pDeleteButton->SetPos(GetWide() - m_pDeleteButton->GetWide() * 1.5f, GetTall() * 0.5f - m_pDeleteButton->GetTall() * 0.5f);
	}
}

void CASW_Mission_Chooser_Entry::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_pNameLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pNameLabel->InvalidateLayout();

	m_pDescriptionLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pDescriptionLabel->InvalidateLayout();
	vgui::HFont defaultfont = pScheme->GetFont( "MissionChooserFont", IsProportional() );
	m_pNameLabel->SetFont(defaultfont);
	m_pDescriptionLabel->SetFont(defaultfont);
	m_iLabelHeight = vgui::surface()->GetFontTall(defaultfont);
	m_pDescriptionLabel->SetFgColor(Color(160,160,160,255));

	m_pNameLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pDescriptionLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
	
	if ( m_bVoteDisabled )
	{
		m_pNameLabel->SetFgColor(Color(128,128,128,255));
		m_pDescriptionLabel->SetFgColor(Color(128,128,128,255));
		SetBgColor(Color(64,64,64,64));
	}
	else
	{
		m_pNameLabel->SetFgColor(Color(255,255,255,255));
		m_pDescriptionLabel->SetFgColor(Color(160,160,160,255));
		SetBgColor(Color(0,0,0,0));
	}

	m_pDescriptionLabel->InvalidateLayout();
	m_pDescriptionLabel->GetTextImage()->RecalculateNewLinePositions();
}

void CASW_Mission_Chooser_Entry::OnMouseReleased(vgui::MouseCode code)
{
	// hack to stop us catching the release event after clicking the main menu
	if ( !m_bMouseReleased || m_bVoteDisabled )
		return;

	if (m_pDeleteButton && m_pDeleteButton->IsCursorOver())
	{		
		return;
	}

	if ( code != MOUSE_LEFT )
		return;

	if (m_szMapName[0] != '\0')
	{
		CASW_Mission_Chooser_List *pList = dynamic_cast<CASW_Mission_Chooser_List*>(GetParent());
		if (pList)
		{
			pList->OnEntryClicked(this);
		}
	}
}

void CASW_Mission_Chooser_Entry::OnCommand(const char* command)
{
	if (m_szMapName[0] == '\0')
		return;
	if (m_HostType == ASW_HOST_TYPE_CALLVOTE)
		return;

	if (!stricmp(command, "DeleteSave"))
	{		
		CASW_Mission_Chooser_List *pList = dynamic_cast<CASW_Mission_Chooser_List*>(GetParent());
		if (pList)
		{
			char buffer[MAX_PATH];
			Q_snprintf(buffer, sizeof(buffer), "save/%s", m_szMapName);
			Msg("Deleting %s", buffer);
			g_pFullFileSystem->RemoveFile( buffer, "GAME" );
			pList->OnSaveDeleted();
		}
	}
	else if (!stricmp(command, "LoadSave"))
	{
		CASW_Mission_Chooser_List *pList = dynamic_cast<CASW_Mission_Chooser_List*>(GetParent());
		if (pList)
		{
			pList->OnEntryClicked(this);
		}
	}
}

void CASW_Mission_Chooser_Entry::SetVoteDisabled( bool bDisabled )
{
	if ( m_bVoteDisabled != bDisabled )
	{
		m_bVoteDisabled = bDisabled;
		InvalidateLayout( true, true );
	}
}