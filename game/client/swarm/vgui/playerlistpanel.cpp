#include "cbase.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/CheckButton.h>
#include "c_asw_marine_resource.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_campaign_save.h"
#include "c_asw_game_resource.h"
#include "PlayerListPanel.h"
#include "PlayerListLine.h"
#include "asw_gamerules.h"
#include "c_playerresource.h"
#include "ImageButton.h"
#include <vgui/ILocalize.h>
#include "WrappedLabel.h"
#include <vgui_controls/TextImage.h>
#include <vgui/ISurface.h>
#include "MissionStatsPanel.h"
#include "iinput.h"
#include "input.h"
#include "ForceReadyPanel.h"
#include "iclientmode.h"
#include "hud_element_helper.h"
#include "asw_hud_minimap.h"
#include "nb_header_footer.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "nb_button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

int g_asw_iPlayerListOpen = 0;

PlayerListPanel::PlayerListPanel(vgui::Panel *parent, const char *name) :
	vgui::EditablePanel(parent, name)
{
	//input->MouseEvent(0, false);	// unclick all our mouse buttons when this panel pops up (so firing doesn't get stuck on)

	m_bVoteMapInstalled = true;
	m_PlayerLine.Purge();

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetFooterEnabled( true );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 75, 320 );
	if ( ASWGameRules() && ASWGameRules()->GetGameState() != ASW_GS_INGAME )
	{
		m_pHeaderFooter->SetMovieEnabled( true );
		m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_DARK );
	}
	else
	{
		m_pHeaderFooter->SetMovieEnabled( false );
		m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_NONE );
	}

	m_pPlayerHeader = new vgui::Label(this, "PlayerHeader", "#asw_player_list_player");
	m_pMarinesHeader = new vgui::Label(this, "MarinesHeader", "#asw_player_list_marines");
	m_pPingHeader = new vgui::Label(this, "PingHeader", "#asw_player_list_ping");

	m_pMissionLabel = new vgui::Label(this, "MissionLabel", "");
	m_pServerLabel = new vgui::Label(this, "ServerLabel", "");
	m_pDifficultyLabel = new vgui::Label(this, "DifficultyLabel", "");	

	m_szMapName[0] = '\0';
	m_iNoCount = -1;
	m_iYesCount = -1;
	m_iSecondsLeft = -1;
	m_fUpdateDifficultyTime = 0;

	// voting buttons
	m_pVoteBackground = new vgui::Panel(this, "VoteBG");
	m_pLeaderButtonsBackground = new vgui::Panel(this, "LeaderBG");
	m_pStartVoteTitle = new vgui::Label(this, "StartVoteTitle", "#asw_start_vote");
	m_pCurrentVoteTitle = new vgui::Label(this, "CurrentVoteTitle", "#asw_current_vote");	
	m_pYesVotesLabel = new vgui::Label(this, "YesCount", "");
	m_pNoVotesLabel = new vgui::Label(this, "NoCount", "");	
	m_pMapNameLabel = new vgui::WrappedLabel(this, "MapName", " ");
	m_pCounterLabel = new vgui::Label(this, "Counter", "");

	m_pRestartMissionButton = new CNB_Button( this, "RestartButton", "" ); //ImageButton(this, "RestartButton", );
	m_pStartCampaignVoteButton = new CNB_Button(this, "CampaignVoteButton", "#asw_campaign_vote");
	m_pStartSavedCampaignVoteButton = new ImageButton(this, "SavedVoteButton", "#asw_saved_vote");
	m_pStartSavedCampaignVoteButton->SetVisible( false );
	m_pVoteYesButton = new CNB_Button(this, "Yes", "#asw_vote_yes");
	m_pVoteNoButton = new CNB_Button(this, "No", "#asw_vote_no");	

	m_pStartSavedCampaignVoteButton->SetButtonTexture("swarm/Briefing/ShadedButton");
	m_pStartSavedCampaignVoteButton->SetButtonOverTexture("swarm/Briefing/ShadedButton_over");
	
	m_pRestartMissionButton->AddActionSignalTarget( this );
	//m_pStartCampaignVoteButton->AddActionSignalTarget( this );
	m_pStartSavedCampaignVoteButton->AddActionSignalTarget( this );
	m_pVoteYesButton->AddActionSignalTarget( this );
	m_pVoteNoButton->AddActionSignalTarget( this );

	m_pStartSavedCampaignVoteButton->SetContentAlignment(vgui::Label::a_center);

	KeyValues *newcampaignmsg = new KeyValues("Command");
	newcampaignmsg->SetString("command", "NewCampaignVote");
	m_pStartCampaignVoteButton->SetCommand(newcampaignmsg);

	KeyValues *newsavedmsg = new KeyValues("Command");
	newsavedmsg->SetString("command", "NewSavedVote");
	m_pStartSavedCampaignVoteButton->SetCommand(newsavedmsg);

	KeyValues *voteyesmsg = new KeyValues("Command");
	voteyesmsg->SetString("command", "VoteYes");
	m_pVoteYesButton->SetCommand(voteyesmsg);

	KeyValues *votenomsg = new KeyValues("Command");
	votenomsg->SetString("command", "VoteNo");
	m_pVoteNoButton->SetCommand(votenomsg);

	KeyValues *resmsg = new KeyValues("Command");
	resmsg->SetString("command", "RestartMis");
	m_pRestartMissionButton->SetCommand(resmsg);	

	UpdateKickLeaderTicks();

	g_asw_iPlayerListOpen++;

	m_szServerName[0] = '\0';
}

PlayerListPanel::~PlayerListPanel()
{
	g_asw_iPlayerListOpen--;
}

void PlayerListPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	LoadControlSettings( "resource/ui/PlayerListPanel.res" );

	vgui::HFont DefaultFont = pScheme->GetFont( "Default", IsProportional() );
	m_pPlayerHeader->SetFont(DefaultFont);
	m_pPlayerHeader->SetFgColor(Color(255,255,255,255));
	m_pMarinesHeader->SetFont(DefaultFont);
	m_pMarinesHeader->SetFgColor(Color(255,255,255,255));
	m_pPingHeader->SetFont(DefaultFont);
	m_pPingHeader->SetFgColor(Color(255,255,255,255));
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);

	// more translucent when viewed ingame
	if (GetParent() && GetParent()->GetParent() && GetParent()->GetParent() == GetClientMode()->GetViewport())
		SetBgColor(Color(0,0,0,128));
	else
		SetBgColor(Color(0,0,0,220));


	m_pLeaderButtonsBackground->SetPaintBackgroundEnabled(false);	// temp removal of this
	m_pLeaderButtonsBackground->SetPaintBackgroundType(0);
	m_pLeaderButtonsBackground->SetBgColor(Color(0,0,0,128));

	m_pStartSavedCampaignVoteButton->m_pLabel->SetFont(DefaultFont);

	Color col_white(255,255,255,255);
	Color col_grey(128,128,128,255);
	m_pStartSavedCampaignVoteButton->SetColors(col_white, col_grey, col_white, col_grey, col_white);
}

void PlayerListPanel::PerformLayout()
{	
	float fScale = (ScreenHeight() / 768.0f);
	int border = 12.0f * fScale;
	int middle_button_width = 160.0f * fScale;
	int player_list_top = YRES( 85 ); //border * 2 + mission_tall + default_tall * 2;
	int line_top = 35 * fScale + player_list_top;
	int line_height = 48 * fScale;
	int header_height = 32 * fScale;
	int padding = 5 * fScale;	

	if (!GetParent())
		return;

	SetBounds(0, 0, GetParent()->GetWide(),GetParent()->GetTall());
	
	int left_edge = GetWide() * 0.5f - YRES( 250 );

	m_pPlayerHeader->SetBounds( left_edge + PLAYER_LIST_PLAYER_X * fScale, player_list_top, PLAYER_LIST_PLAYER_W * fScale, header_height);
	m_pMarinesHeader->SetBounds( left_edge + PLAYER_LIST_MARINES_X * fScale, player_list_top, PLAYER_LIST_MARINES_W * fScale, header_height);
	m_pPingHeader->SetBounds( left_edge + PLAYER_LIST_PING_X * fScale, player_list_top, PLAYER_LIST_PING_W * fScale, header_height);
	for (int i=0;i<m_PlayerLine.Count();i++)
	{
		m_PlayerLine[i]->SetBounds( left_edge + border, line_top + i * (line_height + padding), YRES( 500 ) - 24.0f * fScale, line_height);
	}
	
	int button_height = 32.0f * fScale;
	int button_width = 140.0f * fScale;	
	int wide_button_width = 285.0f * fScale;
	int button_padding = 8.0f * fScale;
	int button_y = YRES( 420 ) - (button_height + button_padding + border);
	int label_height = 32.0f * fScale;
	int button_x = button_padding + border;

	m_pStartSavedCampaignVoteButton->SetBounds( left_edge + button_x + button_width + middle_button_width + button_padding * 2, button_y, wide_button_width, button_height);
	int vote_y = YRES( 420 ) - (button_height * 2 + button_padding * 2 + border);

	bool bVoteActive = (ASWGameRules() && (ASWGameRules()->GetCurrentVoteType() != ASW_VOTE_NONE));
	int bg_top = button_y - (label_height + button_padding);
	if (bVoteActive)
		bg_top = vote_y - button_padding;

	m_pLeaderButtonsBackground->SetBounds( left_edge + border, bg_top - (button_height + border * 3), YRES( 500 ) - (border * 2),
			button_height + border * 2);
}

void PlayerListPanel::OnThink()
{
	// make sure we have enough line panels per player and that each line knows the index of the player its displaying
	int iNumPlayersInGame = 0;
	bool bNeedsLayout = false;
	for ( int j = 1; j <= gpGlobals->maxClients; j++ )
	{	
		if ( g_PR->IsConnected( j ) )
		{
			iNumPlayersInGame++;
			while (m_PlayerLine.Count() <= iNumPlayersInGame)
			{
				// temp comment
				m_PlayerLine.AddToTail(new PlayerListLine(this, "PlayerLine"));
			}
			if ( m_PlayerLine.Count() > ( iNumPlayersInGame - 1 ) && m_PlayerLine[ iNumPlayersInGame - 1 ] )
			{
				m_PlayerLine[iNumPlayersInGame-1]->SetVisible(true);
				if (m_PlayerLine[iNumPlayersInGame-1]->SetPlayerIndex(j))
				{
					bNeedsLayout = true;
				}
			}
		}
	}

	// hide any extra ones we might have
	for (int i=iNumPlayersInGame;i<m_PlayerLine.Count();i++)
	{
		m_PlayerLine[i]->SetVisible(false);
	}

	UpdateVoteButtons();
	UpdateKickLeaderTicks();
	if (gpGlobals->curtime > m_fUpdateDifficultyTime)
	{
		MissionStatsPanel::SetMissionLabels(m_pMissionLabel, m_pDifficultyLabel);
		m_fUpdateDifficultyTime = gpGlobals->curtime + 1.0f;
	}

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	bool bShowRestart = pPlayer && ASWGameResource() && (ASWGameResource()->GetLeaderEntIndex() == pPlayer->entindex());
	if ( ASWGameRules() && ASWGameRules()->IsIntroMap() )
		bShowRestart = false;
	//Msg("bLeader = %d leaderentindex=%d player entindex=%d\n", bLeader, ASWGameResource()->GetLeaderEntIndex(), pPlayer->entindex());
	m_pRestartMissionButton->SetVisible(bShowRestart);
	m_pLeaderButtonsBackground->SetVisible(bShowRestart);	

	UpdateServerName();

	if (bNeedsLayout)
		InvalidateLayout(true);
}


void PlayerListPanel::KickClicked(PlayerListLine* pLine)
{
	// unselect all the other check boxes
	for (int i=0;i<m_PlayerLine.Count();i++)
	{
		if (m_PlayerLine[i] == pLine)			
		{
			if (m_iKickVoteIndex != m_PlayerLine[i]->m_iPlayerIndex)
			{
				char buffer[64];
				Q_snprintf(buffer, sizeof(buffer), "cl_kickvote %d", i+1);
				engine->ClientCmd(buffer);
			}
			else	// we were already wanting to kick this player, so toggle it off
			{
				char buffer[64];
				Q_snprintf(buffer, sizeof(buffer), "cl_kickvote -1");
				engine->ClientCmd(buffer);
			}
		}
	}
}

void PlayerListPanel::LeaderClicked(PlayerListLine* pLine)
{
	// unselect all the other check boxes
	for (int i=0;i<m_PlayerLine.Count();i++)
	{
		if (m_PlayerLine[i] == pLine)			
		{
			if (m_iLeaderVoteIndex != m_PlayerLine[i]->m_iPlayerIndex)
			{
				char buffer[64];
				Q_snprintf(buffer, sizeof(buffer), "cl_leadervote %d", i+1);
				engine->ClientCmd(buffer);
			}
			else	// we were already wanting to kick this play, so toggle it off
			{
				char buffer[64];
				Q_snprintf(buffer, sizeof(buffer), "cl_leadervote -1");
				engine->ClientCmd(buffer);
			}
		}
	}
}

void PlayerListPanel::UpdateVoteButtons()
{
	if (!ASWGameRules())
	{
		SetShowStartVoteElements(false);
		SetShowCurrentVoteElements(false);
		return;
	}

	if (ASWGameRules()->GetCurrentVoteType() == ASW_VOTE_NONE)
	{
		SetShowStartVoteElements(true);
		SetShowCurrentVoteElements(false);
		return;
	}
	
	SetShowStartVoteElements(false);
	SetShowCurrentVoteElements(true);

	// update timer
	int iSecondsLeft = ASWGameRules()->GetCurrentVoteTimeLeft();
	if (iSecondsLeft != m_iSecondsLeft)
	{
		m_iSecondsLeft = iSecondsLeft;
		char buffer[8];
		Q_snprintf(buffer, sizeof(buffer), "%d", iSecondsLeft);

		wchar_t wnumber[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

		wchar_t wbuffer[96];		
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_time_left"), 1,
				wnumber);
		m_pCounterLabel->SetText(wbuffer);
	}	

	// update count and other labels
	if (m_iYesCount != ASWGameRules()->GetCurrentVoteYes())
	{
		m_iYesCount = ASWGameRules()->GetCurrentVoteYes();
		char buffer[8];
		Q_snprintf(buffer, sizeof(buffer), "%d", m_iYesCount);

		wchar_t wnumber[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

		wchar_t wbuffer[96];		
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_yes_votes"), 1,
				wnumber);
		m_pYesVotesLabel->SetText(wbuffer);
	}
	if (m_iNoCount != ASWGameRules()->GetCurrentVoteNo())
	{
		m_iNoCount = ASWGameRules()->GetCurrentVoteNo();
		char buffer[8];
		Q_snprintf(buffer, sizeof(buffer), "%d", m_iNoCount);

		wchar_t wnumber[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

		wchar_t wbuffer[96];		
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_no_votes"), 1,
				wnumber);
		m_pNoVotesLabel->SetText(wbuffer);
	}	
	if (Q_strcmp(m_szMapName, ASWGameRules()->GetCurrentVoteDescription()))
	{
		Q_snprintf(m_szMapName, sizeof(m_szMapName), "%s", ASWGameRules()->GetCurrentVoteDescription());
		
		wchar_t wmapname[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(m_szMapName, wmapname, sizeof( wmapname ));

		wchar_t wbuffer[96];						
		if (ASWGameRules()->GetCurrentVoteType() == ASW_VOTE_CHANGE_MISSION)
		{
			m_bVoteMapInstalled = true;
			if ( missionchooser && missionchooser->LocalMissionSource() )
			{
				if ( !missionchooser->LocalMissionSource()->GetMissionDetails( ASWGameRules()->GetCurrentVoteMapName() ) )
					m_bVoteMapInstalled = false;
			}

			if ( m_bVoteMapInstalled )
			{
				const char *szContainingCampaign = ASWGameRules()->GetCurrentVoteCampaignName();
				if ( !szContainingCampaign || !szContainingCampaign[0] )
				{
					g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
						g_pVGuiLocalize->Find("#asw_current_mission_vote"), 1,
							wmapname);
				}
				else
				{
					// TODO: Show campaign name too?
					g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
						g_pVGuiLocalize->Find("#asw_current_mission_vote"), 1,
						wmapname);
				}
			}
			else
			{
				g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#asw_current_mission_vote_not_installed"), 1,
					wmapname);
			}
		}
		else if (ASWGameRules()->GetCurrentVoteType() == ASW_VOTE_SAVED_CAMPAIGN)
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
				g_pVGuiLocalize->Find("#asw_current_saved_vote"), 1,
					wmapname);
		}

		int w, t;
		m_pMapNameLabel->GetSize(w, t);
		if (m_pMapNameLabel->GetTextImage())
			m_pMapNameLabel->GetTextImage()->SetSize(w, t);
		m_pMapNameLabel->SetText(wbuffer);
		m_pMapNameLabel->InvalidateLayout(true);
	}		
}

void PlayerListPanel::SetShowStartVoteElements(bool bVisible)
{
	m_pStartVoteTitle->SetVisible(bVisible);
	m_pStartCampaignVoteButton->SetVisible(bVisible);
	m_pStartSavedCampaignVoteButton->SetVisible(false);	// disable loading saves for now
}

void PlayerListPanel::SetShowCurrentVoteElements(bool bVisible)
{
	m_pCurrentVoteTitle->SetVisible(bVisible);
	m_pVoteYesButton->SetVisible(bVisible);
	m_pVoteNoButton->SetVisible(bVisible);
	m_pYesVotesLabel->SetVisible(bVisible);
	m_pNoVotesLabel->SetVisible(bVisible);
	m_pMapNameLabel->SetVisible(bVisible);
	m_pCounterLabel->SetVisible(bVisible);
}

void PlayerListPanel::OnCommand( char const *cmd )
{
	if ( !Q_stricmp( cmd, "NewMissionVote" ) )
	{
		GetParent()->SetVisible(false);
		GetParent()->MarkForDeletion();
		Msg("PlayerListPanel::OnCommand sending asw_vote_chooser cc\n");
		if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
			engine->ClientCmd("asw_vote_chooser 2");
		else
			engine->ClientCmd("asw_vote_chooser 2 notrans");
	}
	else if ( !Q_stricmp( cmd, "NewCampaignVote" ) )
	{
		GetParent()->SetVisible(false);
		GetParent()->MarkForDeletion();
		if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
			engine->ClientCmd("asw_vote_chooser 0");
		else
			engine->ClientCmd("asw_vote_chooser 0 notrans");
	}
	else if ( !Q_stricmp( cmd, "NewSavedVote" ) )
	{
		GetParent()->SetVisible(false);
		GetParent()->MarkForDeletion();
		if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
			engine->ClientCmd("asw_vote_chooser 1");	
		else
			engine->ClientCmd("asw_vote_chooser 1 notrans");
	}
	else if ( !Q_stricmp( cmd, "VoteYes" ) )
	{
		GetParent()->SetVisible(false);
		GetParent()->MarkForDeletion();
		engine->ClientCmd("vote_yes");
	}
	else if ( !Q_stricmp( cmd, "VoteNo" ) )
	{
		GetParent()->SetVisible(false);
		GetParent()->MarkForDeletion();
		engine->ClientCmd("vote_no");
	}
	else if ( !Q_stricmp( cmd, "Back" ) )
	{
		GetParent()->SetVisible(false);
		GetParent()->MarkForDeletion();
	}
	else if ( !Q_stricmp( cmd, "RestartMis" ) )
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (!pPlayer)
			return;
		bool bLeader = pPlayer && ASWGameResource() && ASWGameResource()->GetLeaderEntIndex() == pPlayer->entindex();
		if (!bLeader)
			return;

		bool bCanStart = ASWGameRules() && 
					( (ASWGameRules()->GetGameState() <= ASW_GS_INGAME) 
				   || (ASWGameResource() && ASWGameResource()->AreAllOtherPlayersReady(pPlayer->entindex()) ) );

		if (bCanStart)
		{
			GetParent()->SetVisible(false);
			GetParent()->MarkForDeletion();	
			engine->ClientCmd("asw_restart_mission");
		}
		else
		{
			if (GetParent() && GetParent()->GetParent())
			{
				vgui::Panel *p = new ForceReadyPanel(GetParent()->GetParent(), "ForceReady", "#asw_force_restartm", ASW_FR_RESTART);
				p->SetZPos(201);	// make sure it's in front of the player list
			}
		}
	}	
	else
	{
		BaseClass::OnCommand( cmd );
	}
}

void PlayerListPanel::UpdateKickLeaderTicks()
{
	m_iKickVoteIndex = -1;
	m_iLeaderVoteIndex = -1;
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		m_iKickVoteIndex = pPlayer->m_iKickVoteIndex;
		m_iLeaderVoteIndex = pPlayer->m_iLeaderVoteIndex;
	}
}

void PlayerListPanel::UpdateServerName()
{
	const char* szServerName="";
	if (gpGlobals->maxClients > 1)
	{
		CASWHudMinimap *pMinimap = GET_HUDELEMENT( CASWHudMinimap );
		if (pMinimap)
			szServerName = pMinimap->m_szServerName;
	}
	if (Q_strcmp(szServerName, m_szServerName))
	{
		Q_snprintf(m_szServerName, sizeof(m_szServerName), "%s", szServerName);
		if (m_pServerLabel)
		{
			// post message to avoid translation
			PostMessage( m_pServerLabel, new KeyValues( "SetText", "text", m_szServerName ) );
		}
	}
}