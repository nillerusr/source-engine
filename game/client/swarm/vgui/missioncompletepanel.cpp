#include "cbase.h"
#include "vgui_controls/ImagePanel.h"
#include "MissionCompletePanel.h"
#include "MissionCompleteStatsLine.h"
#include "vgui_controls/AnimationController.h"
#include "RestartMissionButton.h"
#include "ReturnCampaignMapButton.h"
#include "c_asw_player.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "vgui/isurface.h"
#include "BriefingTooltip.h"
#include "BriefingPropertySheet.h"
#include "MedalCollectionPanel.h"
#include "MissionStatsPanel.h"
#include "c_playerresource.h"
#include "controller_focus.h"
#include "vgui\DebriefTextPage.h"
#include "vgui\MissionCompleteFrame.h"
#include "experience_report.h"
#include "stats_report.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include "ForceReadyPanel.h"
#include "nb_weapon_unlocked.h"
#include "mission_complete_message.h"
#include "clientmode_asw.h"
#include "nb_vote_panel.h"
#include "nb_suggest_difficulty.h"
#include "debrieftextpage.h"
#include "nb_island.h"
#include "asw_hud_minimap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

#ifdef _DEBUG
extern ConVar asw_unlock_all_weapons;
#endif
ConVar asw_success_sound_delay( "asw_success_sound_delay", "0.0", FCVAR_CHEAT, "Delay before playing mission success music" );
ConVar asw_fail_sound_delay( "asw_fail_sound_delay", "0.0", FCVAR_CHEAT, "Delay before playing mission fail music" );
ConVar asw_show_stats_in_singleplayer( "asw_show_stats_in_singleplayer", "0", FCVAR_NONE, "Show stats screen in singleplayer" );

MissionCompletePanel::MissionCompletePanel(Panel *parent, const char *name, bool bSuccess) : vgui::EditablePanel(parent, name)
{	
	m_pResultImage = NULL;
	m_bViewedStatsPage = false;

	m_pFailAdvice = new vgui::Label( this, "FailAdvice", "" );
	m_pFailAdvice->SetMouseInputEnabled( false );
	m_pIconForwardArrow = new vgui::ImagePanel( this, "IconForwardArrow" );

	m_PropertySheet = NULL;
	m_bSetAlpha = false;

	m_bCreditsSeen = false;

	m_bShowQueuedUnlocks = false;
	
	if ( !bSuccess )
	{
		m_pFailedHeaderFooter = new CNB_Header_Footer( this, "FailedHeaderFooter" );
		m_pFailedHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_TRANSPARENT_RED );
	}
	else
	{
		m_pFailedHeaderFooter = NULL;
	}
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	if ( ASWGameRules() && !ASWGameRules()->GetMissionSuccess() )
	{
		m_pHeaderFooter->SetTitle("#asw_mission_failed");
		m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_TRANSPARENT_BLUE );
		m_pFailedHeaderFooter->SetAlpha( 0 );
	}
	else
	{
		m_pHeaderFooter->SetTitle( "#asw_summary" );
		m_pHeaderFooter->SetAlpha( 0 );
	}

	m_pMissionName = new vgui::Label( this, "MissionName", "" );
	CASWHudMinimap *pMap = GET_HUDELEMENT( CASWHudMinimap );
	if ( pMap )
	{
		m_pMissionName->SetText(pMap->m_szMissionTitle);
	}
	m_pMissionName->SetAlpha( 0 );
	
	//m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_NONE );
	m_pMainElements = new vgui::Panel( this, "MainElements" );

	m_bSuccess = bSuccess;
	
	vgui::Panel *pParent = m_pMainElements;
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");

	m_PropertySheet = new BriefingPropertySheet( m_pMainElements, "MissionCompleteSheet");
	m_PropertySheet->SetPos(0,0);
	m_PropertySheet->SetSize(ScreenWidth(), ScreenHeight());
	m_PropertySheet->SetVisible(true);
	m_PropertySheet->SetShowTabs( false );

	pParent = m_PropertySheet;

	// stats
	if ( ASWGameRules()->IsTutorialMap() )
	{
		m_pExperienceReport = NULL;
	}
	else
	{
		m_pExperienceReport = new CExperienceReport( pParent, "ExperienceReport" );
		m_pExperienceReport->Init();	// causes XP to be awarded
	}
	m_pStatsPanel = new StatsReport( pParent, "StatsReport" );
	
	// if we're showing the debrief text tab, we need a property sheet to be the parent of all this stuff
	m_PropertySheet->SetTabWidth(ScreenWidth()/5.0f);

	if ( !ASWGameRules()->IsTutorialMap() )
	{
		Msg( "Adding experience report\n" );
		m_PropertySheet->AddPageCustomButton( m_pExperienceReport, "#asw_experience_report_tab", scheme );
		if ( GetControllerFocus() )
		{
			GetControllerFocus()->AddToFocusList( m_PropertySheet->GetTab( 0 ), false );
		}
	}

	m_PropertySheet->AddPageCustomButton( m_pStatsPanel, "#asw_stats_tab", scheme );
	if ( GetControllerFocus() )
	{
		GetControllerFocus()->AddToFocusList( m_PropertySheet->GetTab( !ASWGameRules()->IsTutorialMap() ? 1 : 0 ), false );
	}

	if ( !ASWGameRules()->IsTutorialMap() )
	{
		m_PropertySheet->SetActivePage( m_pExperienceReport );
	}
	else
	{
		m_PropertySheet->SetActivePage( m_pStatsPanel );
	}
						
	SetKeyBoardInputEnabled(false);	
	
	m_iStage = MCP_STAGE_INITIAL_DELAY;
	if (bSuccess)
		m_fNextStageTime = gpGlobals->curtime;
	else
		m_fNextStageTime = gpGlobals->curtime + 0.6f;

	m_flForceVisibleButtonsTime = m_fNextStageTime + 6.0f;

	if (!g_hBriefingTooltip.Get())
	{
		g_hBriefingTooltip = new BriefingTooltip(m_pMainElements, "BriefingTooltip");
		g_hBriefingTooltip->SetTooltip(this, "INIT", "INIT", 0, 0);	// have to do this, as the tooltip "jumps" for one frame when it's first drawn (this makes the jump happen while the briefing is fading in)
	}	

	m_pRestartButton = new CNB_Button( this, "RestartButton", "#asw_button_restart", this, "Restart" );
	m_pReadyButton = new CNB_Button( this, "ReadyButton", "#asw_button_ready", this, "Ready" );
	m_pReadyCheckImage = new vgui::ImagePanel( this, "ReadyCheckImage" );
	m_pContinueButton = new CNB_Button( this, "ContinueButton", "#asw_button_continue", this, "Continue" );

	m_pTab[ 0 ] = new CNB_Button( this, "XPTab", "#asw_summary", this, "XPTab" );
	m_pTab[ 1 ] = new CNB_Button( this, "StatsTab", "#asw_stats_tab", this, "StatsTab" );
	m_pTab[ 2 ] = NULL;

	m_pNextButton = new CNB_Button( this, "NextButton", "#asw_button_next", this, "StatsTab" );

	m_pVotePanel = new CNB_Vote_Panel( this, "VotePanel" );
	m_pVotePanel->m_VotePanelType = VPT_DEBRIEF;
	m_pVotePanel->SetZPos( 101 );

	SetAlpha(0);
}

MissionCompletePanel::~MissionCompletePanel()
{
	if (g_hBriefingTooltip.Get())
	{
		g_hBriefingTooltip->MarkForDeletion();
		g_hBriefingTooltip->SetVisible(false);
		g_hBriefingTooltip = NULL;
	}
}

void MissionCompletePanel::ShowImageAndPlaySound()
{
	if ( m_pResultImage )
	{
		m_pResultImage->MarkForDeletion();
	}
	m_pResultImage = new CMission_Complete_Message( this, "MissionCompleteMessage" );
	m_pResultImage->StartMessage( m_bSuccess );

	m_pResultImage->SetMouseInputEnabled( false );


	// set up fail advice
	if ( m_bSuccess )
	{
		m_pFailAdvice->SetText( "" );
	}
	else
	{
		m_pFailAdvice->SetText( ASWGameRules()->GetFailAdviceText() );
		m_pHeaderFooter->SetAlpha( 0 );
	}

	// play sound
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		if ( m_bSuccess )
		{
			pPlayer->EmitSound( "Game.MissionWon" );
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, pPlayer->entindex(), "asw_song.statsSuccess", NULL, gpGlobals->curtime + asw_success_sound_delay.GetFloat() );
		}
		else
		{
			pPlayer->EmitSound( "Game.MissionLost" );
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, pPlayer->entindex(), "asw_song.statsFail", NULL, gpGlobals->curtime + asw_fail_sound_delay.GetFloat() );
		}
	}	

	m_pFailAdvice->SetAlpha(0);
	m_pIconForwardArrow->SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255, 0, 1.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	vgui::GetAnimationController()->RunAnimationCommand(m_pFailAdvice, "alpha", 255, 1.5f, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	if ( m_pFailedHeaderFooter )
	{
		vgui::GetAnimationController()->RunAnimationCommand(m_pIconForwardArrow, "alpha", 255, 1.5f, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pFailedHeaderFooter, "alpha", 255, 0.0f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	}
	MissionCompleteFrame *pFrame = dynamic_cast<MissionCompleteFrame*>(GetParent());
	if ( pFrame )
	{
		pFrame->m_bFadeInBackground = true;
	}
	m_pMainElements->SetAlpha(0);	// hide everything else for now
	m_pHeaderFooter->m_pTitle->SetAlpha( 0 );
	m_pMissionName->SetAlpha( 0 );

	InvalidateLayout();

	m_iStage = MCP_STAGE_FAILSUCCESS;
	if ( m_bSuccess )
	{
		if ( ASWGameRules() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() <= 1 )
		{
			m_fNextStageTime = gpGlobals->curtime + 6.0f;
		}
		else
		{
			m_fNextStageTime = gpGlobals->curtime + 4.0f;
		}
	}
	else
	{
		m_fNextStageTime = gpGlobals->curtime + 6.0f;
	}
}

void MissionCompletePanel::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();
	SetPos(0,0);	
	SetSize(width + 1, height + 1);

	m_pMainElements->SetBounds(0, 0, GetWide(), GetTall());

	if ( m_pFailAdvice )
	{
		int nTextPos = ScreenHeight() * ( 3.0f / 4.0f );
		m_pFailAdvice->SetBounds( 0, nTextPos, ScreenWidth(), 40 );
		m_pFailAdvice->SetContentAlignment( vgui::Label::a_center );
		m_pFailAdvice->SetFont( m_LargeFont );
		m_pFailAdvice->SetZPos( 5 );
	}
}

void MissionCompletePanel::OnThink()
{
	if (m_fNextStageTime!=0 && gpGlobals->curtime > m_fNextStageTime)
	{
		// go to next stage
		switch (m_iStage)
		{
		case MCP_STAGE_INITIAL_DELAY:
			{
				ShowImageAndPlaySound();
				m_iStage = MCP_STAGE_FAILSUCCESS;
				break;
			}
		case MCP_STAGE_FAILSUCCESS:
			{ 
				// fade out the fail or success image
				m_iStage = MCP_STAGE_FAILSUCCESS_FADE;
				float fDuration = 1.0f;
				vgui::GetAnimationController()->RunAnimationCommand( m_pResultImage, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
				vgui::GetAnimationController()->RunAnimationCommand( m_pFailAdvice, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
				vgui::GetAnimationController()->RunAnimationCommand( m_pIconForwardArrow, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
				if ( m_pFailedHeaderFooter )
				{
					vgui::GetAnimationController()->RunAnimationCommand( m_pFailedHeaderFooter, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
				}
				
				m_fNextStageTime = gpGlobals->curtime + 2.0f;

				if ( gpGlobals->maxClients > 1 || asw_show_stats_in_singleplayer.GetBool() )
				{
					// fade in all the main elements
					vgui::GetAnimationController()->RunAnimationCommand(m_pMissionName, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
					vgui::GetAnimationController()->RunAnimationCommand(m_pHeaderFooter, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
					vgui::GetAnimationController()->RunAnimationCommand(m_pMainElements, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
					vgui::GetAnimationController()->RunAnimationCommand(m_pHeaderFooter->m_pTitle, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);

					// create the restart button and fade it in
					vgui::GetAnimationController()->RunAnimationCommand(m_pRestartButton, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
					//C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
					//bool bLeader = (pPlayer && ASWGameResource() && (pPlayer == ASWGameResource()->GetLeader()));
					//if (ASWGameRules() && ASWGameRules()->IsCampaignGame() && bLeader)
						//vgui::GetAnimationController()->RunAnimationCommand(m_pCampaignMapButton, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);				

					//m_pStatsPanel->ShowStats(m_bSuccess);				
					if (ASWGameRules() && ASWGameRules()->GetMissionSuccess())	// show debrief?
						vgui::GetAnimationController()->RunAnimationCommand(m_PropertySheet, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
				}
				
				break;
			}
		case MCP_STAGE_FAILSUCCESS_FADE:
			{
				if ( gpGlobals->maxClients > 1 || asw_show_stats_in_singleplayer.GetBool() )
				{
					m_iStage = MCP_STAGE_STATS;
				}
				else
				{
					// restart mission
					engine->ClientCmd( "cl_restart_mission" );
				}
				m_fNextStageTime = 0;
				break;
			}
		};
	}

	UpdateVisibleButtons();
	UpdateQueuedUnlocks();

	int w, t;
	m_pFailAdvice->GetContentSize( w, t );
	int x, y;
	m_pFailAdvice->GetPos( x, y );
	m_pIconForwardArrow->SetPos( ScreenWidth() * 0.5f - w * 0.5f - YRES( 17 ), y + YRES( 2 ) );
}

void MissionCompletePanel::UpdateVisibleButtons()
{
	if ( gpGlobals->maxClients <= 1 && !asw_show_stats_in_singleplayer.GetBool() )
	{
		m_pNextButton->SetVisible( false );
		m_pRestartButton->SetVisible( false );
		m_pReadyButton->SetVisible( false );
		m_pReadyCheckImage->SetVisible( false );
		m_pContinueButton->SetVisible( false );
		m_pTab[ 0 ]->SetVisible( false );
		m_pTab[ 1 ]->SetVisible( false );
		return;
	}

	C_ASW_Player* player = C_ASW_Player::GetLocalASWPlayer();
	if (!player || !ASWGameRules())
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	bool bLeader = (player == pGameResource->GetLeader());

	if ( !m_bViewedStatsPage )
	{
		if ( ( m_pExperienceReport->m_bDoneAnimating || gpGlobals->curtime > m_flForceVisibleButtonsTime )
			&& m_iStage >= MCP_STAGE_STATS )
		{
			m_pNextButton->SetVisible( true );
		}
		else
		{
			m_pNextButton->SetVisible( false );
		}
		
		m_pRestartButton->SetVisible( false );
		m_pReadyButton->SetVisible( false );
		m_pReadyCheckImage->SetVisible( false );
		m_pContinueButton->SetVisible( false );
		m_pTab[ 0 ]->SetVisible( false );
		m_pTab[ 1 ]->SetVisible( false );
	}
	else
	{
		if ( bLeader )
		{
			m_pReadyButton->SetVisible( false );
			m_pReadyCheckImage->SetVisible( false );
			if ( ASWGameRules()->GetMissionSuccess() )
			{
				if ( ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() <= 1 )
				{
					if ( !m_bCreditsSeen )
					{
						m_pContinueButton->SetText( "#asw_button_credits" );
					}
					else
					{
						m_pContinueButton->SetText( "#asw_button_new_campaign" );
					}
				}
				else
				{
					m_pContinueButton->SetText( "#asw_button_continue" );
				}

				m_pContinueButton->SetVisible( true );
				m_pRestartButton->SetVisible( false );
			}
			else
			{
				m_pContinueButton->SetVisible( false );
				m_pRestartButton->SetVisible( true );
			}
		}
		else
		{
			if ( ASWGameRules()->GetMissionSuccess() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() <= 1 )
			{
				if ( !m_bCreditsSeen )
				{
					m_pContinueButton->SetText( "#asw_button_credits" );
				}
				else
				{
					m_pContinueButton->SetText( "#asw_button_new_campaign" );
				}

				m_pContinueButton->SetVisible( true );
				m_pReadyButton->SetVisible( false );
				m_pReadyCheckImage->SetVisible( false );
			}
			else
			{
				m_pContinueButton->SetVisible( false );
				m_pReadyButton->SetVisible( true );
				m_pReadyCheckImage->SetVisible( true );
			}

			m_pRestartButton->SetVisible( false );

			if ( ASWGameResource()->IsPlayerReady( player ) )
			{
				m_pReadyCheckImage->SetImage( "swarm/HUD/TickBoxTicked" );
			}
			else
			{
				m_pReadyCheckImage->SetImage( "swarm/HUD/TickBoxEmpty" );
			}
		}
		m_pTab[ 0 ]->SetVisible( true );
		m_pTab[ 1 ]->SetVisible( true );
	}
}

void MissionCompletePanel::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	LoadControlSettings( "resource/UI/MissionCompletePanel.res" );

	if ( !m_bSetAlpha )
	{
		SetAlpha( 0 );
		m_bSetAlpha = true;
	}

	m_LargeFont = scheme->GetFont( "DefaultLarge", IsProportional() );

	if ( m_pFailAdvice )
	{
		m_pFailAdvice->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	UpdateVisibleButtons();
}

void MissionCompletePanel::OnCommand(const char* command)
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer || !ASWGameRules() )
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	bool bLeader = ( pPlayer == pGameResource->GetLeader() );

	if ( !Q_stricmp( command, "XPTab" ) )
	{
		m_PropertySheet->SetActivePage( m_PropertySheet->GetPage( 0 ) );
		if ( !ASWGameRules()->GetMissionSuccess() )
		{
			m_pHeaderFooter->SetTitle("#asw_mission_failed");
		}
		else
		{
			m_pHeaderFooter->SetTitle( "#asw_summary" );
		}
	}
	else if ( !Q_stricmp( command, "StatsTab" ) )
	{
		m_PropertySheet->SetActivePage( m_PropertySheet->GetPage( 1 ) );
		m_pHeaderFooter->SetTitle( "#asw_stats_tab" );
		m_bViewedStatsPage = true;
	}
	else if ( !Q_stricmp( command, "Restart" ) )
	{
		if ( !bLeader )
			return;

		bool bAllReady = pGameResource->AreAllOtherPlayersReady( pPlayer->entindex() );
		if ( bAllReady )
		{
			pPlayer->RequestMissionRestart();
		}
		else
		{
			// ForceReadyPanel* pForceReady = 
			engine->ClientCmd("cl_wants_restart"); // notify other players that we're waiting on them
			new ForceReadyPanel(GetParent(), "ForceReady", "#asw_force_restartm", ASW_FR_RESTART);
		}
	}
	else if ( !Q_stricmp( command, "Ready" ) )
	{
		// just make us ready
		pPlayer->StartReady();
	}
	else if ( !Q_stricmp( command, "Continue" ) )
	{
		if ( ASWGameRules()->GetMissionSuccess() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() <= 1 )
		{
			if ( !m_bCreditsSeen )
			{
				C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
				if ( pPlayer )
				{
					m_pStatsPanel->m_pDebrief->m_pPara[0]->SetVisible( false );
					m_pStatsPanel->m_pDebrief->m_pPara[1]->SetVisible( false );
					m_pStatsPanel->m_pDebrief->m_pPara[2]->SetVisible( false );
					pPlayer->LaunchCredits( m_pStatsPanel->m_pDebrief->m_pBackground->m_pBackgroundInner );
					m_bCreditsSeen = true;
					UpdateVisibleButtons();
				}
			}
			else
			{
				// Vote on a new mission
				engine->ClientCmd("asw_vote_chooser 0 notrans");
			}
		}
		else if ( bLeader )
		{
			bool bAllReady = pGameResource->AreAllOtherPlayersReady( pPlayer->entindex() );
			if ( bAllReady )
			{
				if ( ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetMissionSuccess() )   // completed a campaign map
				{
					pPlayer->CampaignSaveAndShow();
				}
				else
				{
					// TODO: Proceed to the next mission, pop up a mission selection dialog, etc?
					// just do a restart for now
					pPlayer->RequestMissionRestart();
				}
			}
			else
			{
				if ( ASWGameRules()->GetMissionSuccess() && ASWGameRules()->IsCampaignGame() )
				{
					// ForceReadyPanel* pForceReady = 
					engine->ClientCmd("cl_wants_continue");	// notify other players that we're waiting on them
					new ForceReadyPanel(GetParent(), "ForceReady", "#asw_force_continuem", ASW_FR_CONTINUE);
				}
				else
				{
					// TODO: Proceed to the next mission, pop up a mission selection dialog, etc?
					// just do a restart for now
					// ForceReadyPanel* pForceReady = 
					engine->ClientCmd("cl_wants_restart"); // notify other players that we're waiting on them
					new ForceReadyPanel(GetParent(), "ForceReady", "#asw_force_restartm", ASW_FR_RESTART);
				}
			}
		}

		return;
	}

	BaseClass::OnCommand(command);
}

void MissionCompletePanel::OnWeaponUnlocked( const char *pszWeaponClass )
{
	m_aUnlockedWeapons.AddToTail( pszWeaponClass );
}

void MissionCompletePanel::ShowQueuedUnlockPanels()
{
	m_bShowQueuedUnlocks = true;
}

void MissionCompletePanel::UpdateQueuedUnlocks()
{
	if ( !m_bShowQueuedUnlocks )
		return;

	if ( m_aUnlockedWeapons.Count() > 0 && m_hSubScreen.Get() == NULL )
	{
		CNB_Weapon_Unlocked *pPanel = new CNB_Weapon_Unlocked( this, "Weapon_Unlocked_Panel" );
		pPanel->SetWeaponClass( m_aUnlockedWeapons[0] );
		pPanel->MoveToFront();

		m_hSubScreen = pPanel;

		m_aUnlockedWeapons.Remove( 0 );
	}
}

void MissionCompletePanel::OnSuggestDifficulty( bool bIncrease )
{
	if ( m_hSubScreen.Get() )
	{
		m_hSubScreen->MarkForDeletion();
	}

	CNB_Suggest_Difficulty *pPanel = new CNB_Suggest_Difficulty( this, "SuggestDifficultyPanel" );
	if ( bIncrease )
	{
		pPanel->SetSuggestIncrease();
	}
	else
	{
		pPanel->SetSuggestDecrease();
	}

	pPanel->MoveToFront();

	m_hSubScreen = pPanel;
}

void ShowCompleteMessage()
{
	if ( GetClientModeASW()->m_hMissionCompleteFrame.Get() )
	{
		GetClientModeASW()->m_hMissionCompleteFrame->MarkForDeletion();
		GetClientModeASW()->m_hMissionCompleteFrame = NULL;
	}
	GetClientModeASW()->m_bLaunchedDebrief = false;
}

static ConCommand showcompletemessage("showcompletemessage", ShowCompleteMessage, "Shows the mission complete message again", 0);
