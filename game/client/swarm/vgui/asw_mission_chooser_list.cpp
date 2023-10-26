#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include "asw_mission_chooser_list.h"
#include "asw_mission_chooser_entry.h"
#include "asw_difficulty_chooser.h"
#include "FileSystem.h"
#include "ServerOptionsPanel.h"
#include <vgui_controls/Panel.h>
#include <vgui/isurface.h>
#include <vgui/IInput.h>
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Mission_Chooser_List::CASW_Mission_Chooser_List( vgui::Panel *pParent, const char *pElementName,
													 int iChooserType, int iHostType,
													 IASW_Mission_Chooser_Source *pMissionSource) :
	vgui::PropertyPage(pParent, pElementName)
{
	m_pMissionSource = pMissionSource;
	m_ChooserType = iChooserType;
	m_HostType = iHostType;
	m_pShowAllCheck = NULL;
	m_pServerOptions = NULL;
	m_iMaxPages = 0;
	m_nCampaignIndex = -1;

	m_pCancelButton = new vgui::Button(this, "CancelButton", "#asw_chooser_close", this, "Cancel");

	if (iChooserType == 0)
		m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_select_campaign");
	else if (iChooserType == 1)
		m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_select_saved_campaign");
	else
		m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_select_single_mission");
	m_pTitleLabel->SetVisible(false);

	m_iNumSlots = ASW_MISSIONS_PER_PAGE;
	if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
		m_iNumSlots = 3;
	
	m_pPageLabel = new vgui::Label(this, "PageLabel", " ");
	m_pPageLabel->SetContentAlignment(vgui::Label::a_west);

	// create each entry
	for (int i=0;i<m_iNumSlots;i++)
	{
		m_pEntry[i] = new CASW_Mission_Chooser_Entry(this, "ChooserEntry", m_ChooserType, m_HostType);
		m_pEntry[i]->SetVisible(false);
	}
	for ( int i = m_iNumSlots; i < ASW_SAVES_PER_PAGE; i++ )
	{
		m_pEntry[i] = NULL;
	}

	m_pNextPageButton = new vgui::Button(this, "NextPage", "#asw_button_next", this, "Next");
	m_pPrevPageButton = new vgui::Button(this, "PrevPage", "#asw_button_prev", this, "Prev");
	m_iPage = 0;
	if (m_ChooserType == ASW_CHOOSER_SINGLE_MISSION)
	{
		if (iHostType != ASW_HOST_TYPE_CALLVOTE)
		{
			m_pShowAllCheck = new vgui::CheckButton(this, "ShowAllCheck", "#asw_show_all_files");
			m_pShowAllCheck->SetCheckButtonCheckable(true);
			m_pShowAllCheck->SetSelected(false);
			m_pShowAllCheck->AddActionSignalTarget(this);
		}		
		m_pMissionSource->FindMissions(m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots, !m_pShowAllCheck || !m_pShowAllCheck->IsSelected());
	}
	else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
		m_pMissionSource->FindCampaigns(m_iPage * ASW_CAMPAIGNS_PER_PAGE, m_iNumSlots);
	else if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
		m_pMissionSource->FindSavedCampaigns(m_iPage, m_iNumSlots, m_HostType != ASW_HOST_TYPE_SINGLEPLAYER, NULL);

	UpdateNumPages();
}

CASW_Mission_Chooser_List::~CASW_Mission_Chooser_List()
{
	
}

void CASW_Mission_Chooser_List::ChangeToShowingMissionsWithinCampaign( int nCampaignIndex )
{
	m_nCampaignIndex = nCampaignIndex;
	m_iNumSlots = ASW_MISSIONS_PER_PAGE;
	m_ChooserType = ASW_CHOOSER_SINGLE_MISSION;
	m_iPage = 0;
	for (int i=0;i<m_iNumSlots;i++)
	{
		if ( !m_pEntry[i] )
		{
			m_pEntry[i] = new CASW_Mission_Chooser_Entry(this, "ChooserEntry", m_ChooserType, m_HostType);
			m_pEntry[i]->SetVisible(false);
		}
	}
	m_pTitleLabel->SetText( "#asw_select_single_mission" );	// TODO: better string
	m_pMissionSource->FindMissionsInCampaign( m_nCampaignIndex, m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots );
	InvalidateLayout( true );
}

void CASW_Mission_Chooser_List::OnButtonChecked()
{
	if ( m_ChooserType == ASW_CHOOSER_SINGLE_MISSION && m_nCampaignIndex == -1 )
	{
		m_iPage = 0;
		m_pMissionSource->FindMissions(m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots, !m_pShowAllCheck || !m_pShowAllCheck->IsSelected());
		UpdateNumPages();
	}
}

void CASW_Mission_Chooser_List::UpdateNumPages()
{
	if (m_ChooserType == ASW_CHOOSER_SINGLE_MISSION)
	{
		int iNumEntries = m_pMissionSource->GetNumMissions(!m_pShowAllCheck || !m_pShowAllCheck->IsSelected()) - 1;
		m_iMaxPages = (iNumEntries / m_iNumSlots) + 1;
		//Msg("Num missions = %d slots = %d Num pages = %d\n", m_pMissionSource->GetNumMissions(!m_pShowAllCheck || !m_pShowAllCheck->IsSelected()), m_iNumSlots, m_iMaxPages);
	}
	else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
	{
		int iNumEntries = m_pMissionSource->GetNumCampaigns() - 1;
		m_iMaxPages = (iNumEntries / m_iNumSlots) + 1;
		//Msg("Num campaigns = %d slots = %d Num pages = %d\n", m_pMissionSource->GetNumCampaigns(), m_iNumSlots, m_iMaxPages);
	}
	else if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
	{
		int iNumEntries = m_pMissionSource->GetNumSavedCampaigns(m_HostType != ASW_HOST_TYPE_SINGLEPLAYER, NULL) - 1;
		m_iMaxPages = (iNumEntries / m_iNumSlots) + 1;
		//Msg("Num saved campaigns = %d slots = %d Num pages = %d\n", m_pMissionSource->GetNumSavedCampaigns(), m_iNumSlots, m_iMaxPages);
	}
	if (m_iMaxPages < 1)
		m_iMaxPages = 1;
	if (m_pPageLabel)
	{
		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "Page %d/%d", m_iPage+1, m_iMaxPages);
		m_pPageLabel->SetText(buffer);
		m_pPageLabel->SizeToContents();
		InvalidateLayout();
	}
}

void CASW_Mission_Chooser_List::OnCommand(const char* command)
{
	if (!stricmp(command, "Next"))
	{
		m_iPage++;
		if ( m_ChooserType == ASW_CHOOSER_SINGLE_MISSION )
		{
			if ( m_nCampaignIndex == -1 )
			{
				m_pMissionSource->FindMissions(m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots, !m_pShowAllCheck || !m_pShowAllCheck->IsSelected());
			}
			else
			{
				m_pMissionSource->FindMissionsInCampaign( m_nCampaignIndex, m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots );
			}
		}
		else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
			m_pMissionSource->FindCampaigns(m_iPage * ASW_CAMPAIGNS_PER_PAGE, m_iNumSlots);
		else if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
			m_pMissionSource->FindSavedCampaigns(m_iPage * ASW_SAVES_PER_PAGE, m_iNumSlots, m_HostType != ASW_HOST_TYPE_SINGLEPLAYER, NULL);
		UpdateNumPages();
	}
	else if (!stricmp(command, "Prev"))
	{
		if (m_iPage > 0)
		{
			m_iPage--;
			if ( m_ChooserType == ASW_CHOOSER_SINGLE_MISSION )
			{
				if ( m_nCampaignIndex == -1 )
				{
					m_pMissionSource->FindMissions(m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots, !m_pShowAllCheck || !m_pShowAllCheck->IsSelected());
				}
				else
				{
					m_pMissionSource->FindMissionsInCampaign( m_nCampaignIndex, m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots );
				}
			}
			else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
				m_pMissionSource->FindCampaigns(m_iPage * ASW_CAMPAIGNS_PER_PAGE, m_iNumSlots);
			else if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
				m_pMissionSource->FindSavedCampaigns(m_iPage * ASW_SAVES_PER_PAGE, m_iNumSlots, m_HostType != ASW_HOST_TYPE_SINGLEPLAYER, NULL);
			UpdateNumPages();
		}
	}
	else if (!stricmp(command, "Cancel"))
	{
		CloseSelf();
	}
}

void CASW_Mission_Chooser_List::PerformLayout()
{
	BaseClass::PerformLayout();

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float fScale = float(sh) / 768.0f;
	m_pCancelButton->SetTextInset(6.0f * fScale, 0);
	m_pPrevPageButton->SetTextInset(6.0f * fScale, 0);
	m_pNextPageButton->SetTextInset(6.0f * fScale, 0);

	m_pTitleLabel->SetPos(0, 0);
	m_pTitleLabel->SetSize(GetWide(), 32 * fScale);

	int iFooterSize = 32 * fScale;	

	int iEntryHeight = (GetTall() - iFooterSize) / m_iNumSlots;

	for (int i=0;i<m_iNumSlots;i++)
	{
		m_pEntry[i]->SetBounds(0, iEntryHeight*i, GetWide(), iEntryHeight);
		m_pEntry[i]->InvalidateLayout();
	}

	m_pCancelButton->GetTextImage()->ResizeImageToContent();
	m_pCancelButton->SizeToContents();
	int cancel_wide = m_pCancelButton->GetWide();
	m_pCancelButton->SetSize(cancel_wide, iFooterSize - 4 * fScale);
	m_pCancelButton->SetPos(GetWide() - cancel_wide,
							GetTall() - iFooterSize);

	m_pPageLabel->GetTextImage()->ResizeImageToContent();
	m_pPageLabel->SizeToContents();
	int middle = GetWide() * 0.5f;
	int label_wide = m_pPageLabel->GetWide();	
	int button_wide = 80.0f * fScale;
	int label_x = middle + 2 * fScale;
	m_pPageLabel->SetSize(label_wide, iFooterSize - 4 * fScale);
	m_pPageLabel->SetPos(label_x, GetTall() - iFooterSize + 2 * fScale);
	int next_x = middle - (button_wide + 4 * fScale);
	int prev_x = next_x - (button_wide + 4 * fScale) - 2 * fScale;
	m_pPrevPageButton->SetBounds(prev_x, GetTall() - iFooterSize, button_wide, iFooterSize - 4 * fScale);
	m_pNextPageButton->SetBounds(next_x, GetTall() - iFooterSize, button_wide, iFooterSize - 4 * fScale);

	if (m_pShowAllCheck)
	{
		m_pShowAllCheck->SetBounds(0, GetTall() - iFooterSize, GetWide() * 0.25f, iFooterSize - 4 * fScale);
	}
}

void CASW_Mission_Chooser_List::OnThink()
{
	// give our chooser source a chance to think (so it can continue scanning folders, etc)
	if (m_pMissionSource)
	{
		m_pMissionSource->Think();

		// if we're not hosting a multiplayer game, we can devote more time to scanning
		//  so think enough to fill one page each time
		if (engine->GetMaxClients() <= 1)
		{
			for (int i=0;i<7;i++)
			{
				m_pMissionSource->Think();
			}
		}
	}
	// grab the current list from the source
	UpdateDetails();
}

void CASW_Mission_Chooser_List::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	vgui::HFont defaultfont = pScheme->GetFont( "MissionChooserFont", IsProportional() );
	m_pCancelButton->SetFont(defaultfont);
	m_pNextPageButton->SetFont(defaultfont);
	m_pPrevPageButton->SetFont(defaultfont);
	m_pPageLabel->SetFont(defaultfont);
	m_pPageLabel->SetBgColor( Color( 0, 0, 0, 0 ) );

	m_pPrevPageButton->SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );		
	m_pPrevPageButton->SetArmedColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );
	m_pPrevPageButton->SetDepressedColor( Color( 255, 255, 255, 255 ), Color( 65, 74, 96, 255 ) );

	m_pNextPageButton->SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );		
	m_pNextPageButton->SetArmedColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );
	m_pNextPageButton->SetDepressedColor( Color( 255, 255, 255, 255 ), Color( 65, 74, 96, 255 ) );

	m_pCancelButton->SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );		
	m_pCancelButton->SetArmedColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 128 ) );
	m_pCancelButton->SetDepressedColor( Color( 255, 255, 255, 255 ), Color( 65, 74, 96, 255 ) );

	// Color( 65, 74, 96, 255 )
// 	m_pPrevPageButton->SetButtonBorderEnabled( true );
// 	m_pPrevPageButton->SetDefaultBorder( pScheme->GetBorder("ButtonBorder") );
// 	m_pPrevPageButton->SetDepressedBorder( pScheme->GetBorder("ButtonBorder") );
// 	m_pPrevPageButton->SetKeyFocusBorder( pScheme->GetBorder("ButtonBorder") );
	SetPaintBackgroundEnabled( false );
}

void CASW_Mission_Chooser_List::UpdateDetails()
{
	if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
	{
		// figure out num pages

		m_pMissionSource->FindSavedCampaigns(m_iPage, m_iNumSlots, m_HostType != ASW_HOST_TYPE_SINGLEPLAYER, NULL);
		UpdateNumPages();
		ASW_Mission_Chooser_Saved_Campaign *savedcampaigns = m_pMissionSource->GetSavedCampaigns();
		for (int i=0;i<m_iNumSlots;i++)
		{			
			m_pEntry[i]->SetSavedCampaignDetails(&savedcampaigns[i]);
			m_pEntry[i]->SetVisible(true);
		}
	}
	else
	{
		ASW_Mission_Chooser_Mission* missions = NULL;
		if (m_ChooserType == ASW_CHOOSER_SINGLE_MISSION)
		{
			if ( m_nCampaignIndex == -1 )
			{
				m_pMissionSource->FindMissions(m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots, !m_pShowAllCheck || !m_pShowAllCheck->IsSelected());	// update the list with current missions found
			}
			else
			{
				m_pMissionSource->FindMissionsInCampaign( m_nCampaignIndex, m_iPage * ASW_MISSIONS_PER_PAGE, m_iNumSlots );
			}
			UpdateNumPages();
			missions = m_pMissionSource->GetMissions();
		}
		else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
		{
			m_pMissionSource->FindCampaigns(m_iPage * ASW_CAMPAIGNS_PER_PAGE, m_iNumSlots);
			UpdateNumPages();
			missions = m_pMissionSource->GetCampaigns();
		}
		
		for (int i=0;i<m_iNumSlots;i++)
		{
			m_pEntry[i]->SetDetails(missions[i].m_szMissionName, m_ChooserType);
			m_pEntry[i]->SetVisible(true);
		}
	}
	// hide or show page details as appropriate
	if (m_iMaxPages <= 1)
	{
		if (m_pPageLabel->IsVisible())
			m_pPageLabel->SetVisible(false);
		if (m_pNextPageButton->IsVisible())
			m_pNextPageButton->SetVisible(false);
		if (m_pPrevPageButton->IsVisible())
			m_pPrevPageButton->SetVisible(false);
		if (m_pNextPageButton->IsEnabled())
			m_pNextPageButton->SetEnabled(false);
		if (m_pPrevPageButton->IsEnabled())
			m_pPrevPageButton->SetEnabled(false);
	}
	else
	{
		if (!m_pPageLabel->IsVisible())
			m_pPageLabel->SetVisible(true);
		if (m_iPage > 0)
		{
			if (!m_pPrevPageButton->IsVisible())
				m_pPrevPageButton->SetVisible(true);
			if (!m_pPrevPageButton->IsEnabled())
				m_pPrevPageButton->SetEnabled(true);
		}
		else
		{	
			if (m_pPrevPageButton->IsVisible())
				m_pPrevPageButton->SetVisible(false);
			if (m_pPrevPageButton->IsEnabled())
				m_pPrevPageButton->SetEnabled(false);
		}
		if (m_iPage < m_iMaxPages - 1)
		{
			if (!m_pNextPageButton->IsVisible())
				m_pNextPageButton->SetVisible(true);
			if (!m_pNextPageButton->IsEnabled())
				m_pNextPageButton->SetEnabled(true);
		}
		else
		{	
			if (m_pNextPageButton->IsVisible())
				m_pNextPageButton->SetVisible(false);
			if (m_pNextPageButton->IsEnabled())
				m_pNextPageButton->SetEnabled(false);
		}
	}
}

void CASW_Mission_Chooser_List::OnEntryClicked(CASW_Mission_Chooser_Entry *pClicked)
{
	// find which entry we clicked
	int iChosen = -1;
	for (int i=0;i<ASW_SAVES_PER_PAGE;i++)
	{
		if (m_pEntry[i] == pClicked)
		{
			iChosen = i;
			break;
		}
	}
	if (iChosen == -1)
		return;

	if (m_HostType == ASW_HOST_TYPE_CALLVOTE)
	{
		// issue the appropriate client command to request a vote
		char buffer[128];
		if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
		{
			ASW_Mission_Chooser_Saved_Campaign *savedcampaigns = m_pMissionSource->GetSavedCampaigns();
			char stripped[64];
			V_StripExtension(savedcampaigns[iChosen].m_szSaveName, stripped, sizeof(stripped));
			Q_snprintf(buffer, sizeof(buffer), "asw_vote_saved_campaign %s", stripped);
			engine->ClientCmd(buffer);
			CloseSelf();
		}
		else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
		{
			ChangeToShowingMissionsWithinCampaign( iChosen + m_iPage * ASW_CAMPAIGNS_PER_PAGE );
		}
		else if (m_ChooserType == ASW_CHOOSER_SINGLE_MISSION)
		{
			if ( m_nCampaignIndex == -1 )
			{
				ASW_Mission_Chooser_Mission* missions = m_pMissionSource->GetMissions();
				char stripped[64];
				V_StripExtension(missions[iChosen].m_szMissionName, stripped, sizeof(stripped));
				Q_snprintf(buffer, sizeof(buffer), "asw_vote_mission %s", stripped);
			}
			else
			{
				ASW_Mission_Chooser_Mission* missions = m_pMissionSource->GetMissions();
				char stripped[64];
				V_StripExtension(missions[iChosen].m_szMissionName, stripped, sizeof(stripped));
				Q_snprintf(buffer, sizeof(buffer), "asw_vote_campaign %d %s", m_nCampaignIndex, stripped);
			}
			engine->ClientCmd(buffer);
			CloseSelf();
		}
	}
	else
	{
		// find the launch name (either the map name, campaign name or save name)
		char szLaunchName[256];
		szLaunchName[0] = '\0';
		if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
		{
			ASW_Mission_Chooser_Saved_Campaign *savedcampaigns = m_pMissionSource->GetSavedCampaigns();
			Q_snprintf(szLaunchName, sizeof(szLaunchName), "%s", savedcampaigns[iChosen].m_szSaveName);
		}
		else
		{
			ASW_Mission_Chooser_Mission* missions = NULL;
			if (m_ChooserType == ASW_CHOOSER_SINGLE_MISSION)
				missions = m_pMissionSource->GetMissions();
			else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
				missions = m_pMissionSource->GetCampaigns();
						
			//Q_snprintf(szLaunchName, sizeof(szLaunchName), "%s", missions[iChosen].m_szMissionName);
			V_StripExtension( missions[iChosen].m_szMissionName, szLaunchName, sizeof(szLaunchName) );
		}
		if (szLaunchName[0] == '\0')
			return;

		int iPlayers = 1;
		if (m_HostType != ASW_HOST_TYPE_SINGLEPLAYER)
		{
			iPlayers = 6;
			if (m_pServerOptions)
				iPlayers = m_pServerOptions->GetMaxPlayers();
		}
		
		char szMapCommand[1024];

		// create the command to execute
		// sv_password \"%s\"\n
		// hostname \"%s\"\n
		char szHostnameCommand[64];
		char szPasswordCommand[64];
		char szLANCommand[64];
		if (m_HostType == ASW_HOST_TYPE_CREATESERVER && m_pServerOptions)
		{
			Q_snprintf(szHostnameCommand, sizeof(szHostnameCommand), "hostname \"%s\"\n", m_pServerOptions->GetHostName());
			Q_snprintf(szPasswordCommand, sizeof(szPasswordCommand), "sv_password \"%s\"\n", m_pServerOptions->GetPassword());
			Q_snprintf(szLANCommand, sizeof(szLANCommand), "sv_lan %d\nsetmaster enable\n", m_pServerOptions->GetLAN() ? 1 : 0);
		}
		else
		{
			Q_snprintf(szHostnameCommand, sizeof(szHostnameCommand), "hostname \"AS:I Server\"\n");
			Q_snprintf(szPasswordCommand, sizeof(szPasswordCommand), "");
			Q_snprintf(szLANCommand, sizeof(szLANCommand), "");
		}

		if (m_ChooserType == ASW_CHOOSER_SINGLE_MISSION)
		{
			Q_snprintf(szMapCommand, sizeof( szMapCommand ), "%s%s%smaxplayers %d\nprogress_enable\nmap %s\n",				
				szHostnameCommand,
				szPasswordCommand,
				szLANCommand,
				iPlayers,
				szLaunchName
			);
			ShowDifficultyChooser(szMapCommand);	// note: this adds disconnect, wait, skill to the start
		}
		else if (m_ChooserType == ASW_CHOOSER_CAMPAIGN)
		{
			const char *pNewSaveName = GenerateNewSaveGameName();
			if (!pNewSaveName)
			{
				Msg("Error! No more room for autogenerated save names, delete some save games!\n");
				return;
			}
			Q_snprintf(szMapCommand, sizeof( szMapCommand ), "%s%s%smaxplayers %d\nprogress_enable\nasw_startcampaign %s %s %s\n",				
				szHostnameCommand,
				szPasswordCommand,
				szLANCommand,
				iPlayers,
				szLaunchName,
				pNewSaveName,
				iPlayers == 1 ? "SP" : "MP"
			);		
			ShowDifficultyChooser(szMapCommand);	// note: this adds disconnect, wait, skill to the start
		}
		else if (m_ChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
		{			
			Q_snprintf(szMapCommand, sizeof( szMapCommand ), "disconnect\nwait\nwait\n%s%s%smaxplayers %d\nprogress_enable\nasw_loadcampaign %s %s\n",
				szHostnameCommand,
				szPasswordCommand,
				szLANCommand,
				iPlayers,
				szLaunchName,
				iPlayers == 1 ? "SP" : "MP"
			);
			CloseSelf();
			engine->ClientCmd(szMapCommand);
		}
	}
}

void CASW_Mission_Chooser_List::ShowDifficultyChooser(const char *szCommand)
{
	CASW_Difficulty_Chooser *pFrame = new CASW_Difficulty_Chooser( GetParent(), "DifficultyChooser", szCommand );
	vgui::VPANEL gameuiPanel = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	pFrame->SetParent( gameuiPanel );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(0, "resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	pFrame->SetScheme(scheme);
	
	
	pFrame->SetTitle("#asw_select_difficulty", true );

	pFrame->SetTitleBarVisible(true);
	pFrame->SetMoveable(true);
	pFrame->SetSizeable(false);
	pFrame->SetMenuButtonVisible(false);
	pFrame->SetMaximizeButtonVisible(false);
	pFrame->SetMinimizeToSysTrayButtonVisible(false);
	pFrame->SetMinimizeButtonVisible(false);
	pFrame->SetCloseButtonVisible(true);	
	
	pFrame->RequestFocus();
	pFrame->SetVisible(true);
	pFrame->SetEnabled(true);

	pFrame->InvalidateLayout(true, true);

	CloseSelf();
	vgui::input()->SetAppModalSurface(pFrame->GetVPanel());
}

void CASW_Mission_Chooser_List::CloseSelf()
{
	// yarr, this assumes we're inside a propertysheet, inside a frame..
	//  if the list is placed in a different config, it'll close the wrong thing
	if (GetParent() && GetParent()->GetParent())
	{
		GetParent()->GetParent()->SetVisible(false);
		GetParent()->GetParent()->MarkForDeletion();
	}
}

const char * CASW_Mission_Chooser_List::GenerateNewSaveGameName()
{
	static char szNewSaveName[256];	
	// count up save names until we find one that doesn't exist
	for (int i=1;i<10000;i++)
	{
		Q_snprintf(szNewSaveName, sizeof(szNewSaveName), "save/save%d.campaignsave", i);
		if (!g_pFullFileSystem->FileExists(szNewSaveName))
		{
			Q_snprintf(szNewSaveName, sizeof(szNewSaveName), "save%d.campaignsave", i);
			return szNewSaveName;
		}
	}

	return NULL;
}

void CASW_Mission_Chooser_List::OnSaveDeleted()
{
	if (m_ChooserType != ASW_CHOOSER_SAVED_CAMPAIGN)
		return;

	if (m_ChooserType == ASW_HOST_TYPE_CALLVOTE)
		return;

	if (m_pMissionSource)
	{
		m_pMissionSource->RefreshSavedCampaigns();
		m_pMissionSource->FindSavedCampaigns(m_iPage, m_iNumSlots, false, NULL);	// ignoring multiplayer/filter here, since only singleplayer saves can only be deleted from ingame
		UpdateNumPages();
	}
}