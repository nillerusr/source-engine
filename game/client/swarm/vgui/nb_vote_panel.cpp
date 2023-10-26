#include "cbase.h"
#include "nb_vote_panel.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextImage.h>
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "nb_button.h"
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include "WrappedLabel.h"
#include "asw_gamerules.h"
#include "vgui_controls/AnimationController.h"
#include "c_asw_player.h"
#include "asw_input.h"
#include "clientmode_asw.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CNB_Vote_Panel::CNB_Vote_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pBackground = new vgui::Panel( this, "Background" );
	m_pBackgroundInner = new vgui::Panel( this, "BackgroundInner" );
	m_pTitleBG = new vgui::Panel( this, "TitleBG" );
	m_pTitleBGBottom = new vgui::Panel( this, "TitleBGBottom" );
	m_pTitle = new vgui::Label( this, "Title", "" );

	m_pPressToVoteYesLabel = new vgui::Label(this, "PressToVoteYesLabel", "");
	m_pPressToVoteNoLabel = new vgui::Label(this, "PressToVoteNoLabel", "");
	m_pYesVotesLabel = new vgui::Label(this, "YesCount", "");
	m_pNoVotesLabel = new vgui::Label(this, "NoCount", "");	
	m_pMapNameLabel = new vgui::WrappedLabel(this, "MapName", " ");
	m_pCounterLabel = new vgui::Label(this, "Counter", "");

	m_pProgressBar = new CNB_Progress_Bar( this, "ProgressBar" );

	m_szMapName[0] = '\0';
	m_iNoCount = -1;
	m_iYesCount = -1;
	m_iSecondsLeft = -1;
	m_flCheckBindings = 0;
	m_szVoteYesKey[0] = 0;
	m_szVoteNoKey[0] = 0;
	m_VotePanelType = VPT_BRIEFING;

	vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
}

CNB_Vote_Panel::~CNB_Vote_Panel()
{

}

void CNB_Vote_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/ui/nb_vote_panel.res" );

	SetVisible( false );
}

void CNB_Vote_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, t;
	GetBounds( x, y, w, t );

	m_pBackground->SetBounds( 0, 0, w, t );

	int border = YRES( 2 );
	m_pBackgroundInner->SetBounds( border, border, w - border * 2, t - border * 2 );

	int title_border = YRES( 3 );
	m_pTitleBG->SetBounds( title_border, title_border, w - title_border * 2, YRES( 18 ) );
	m_pTitleBGBottom->SetBounds( title_border, YRES( 15 ), w - title_border * 2, YRES( 6 ) );
	m_pTitle->SetBounds( title_border, title_border, w - title_border * 2, YRES( 18 ) );
}

void CNB_Vote_Panel::OnTick()
{
	UpdateVote();
}

void CNB_Vote_Panel::UpdateVotePanelStatus()
{
	if ( ASWGameRules() )
	{
		float flCorrection = 0.0f;
		if ( !IsVisible() )
		{
			flCorrection = 1.0f;		// prevents us appearing momentarily when the restart timer is up and we transition to the mission complete panel version
		}
		if ( ASWGameRules()->GetRestartingMissionTime() > gpGlobals->curtime + flCorrection )
		{
			if ( GetClientModeASW() && GetClientModeASW()->m_bTechFailure )
			{
				if ( ASWGameRules()->GetGameState() == ASW_GS_INGAME )
				{
					m_VotePanelStatus = VPS_TECH_FAIL;
				}
				else
				{
					m_VotePanelStatus = VPS_NONE;
				}
			}
			else
			{
				m_VotePanelStatus = VPS_RESTART;
			}
			return;
		}
		if ( ASWGameRules()->GetCurrentVoteType() != ASW_VOTE_NONE )
		{
			m_VotePanelStatus = VPS_VOTE;
			return;
		}
	}

	m_VotePanelStatus = VPS_NONE;
}

void CNB_Vote_Panel::UpdateVisibility()
{
	if ( m_VotePanelStatus == VPS_NONE )
	{
		SetVisible( false );
		return;
	}

	if ( !IsVisible() )
	{
		const float flTransitionTime = 0.3f;		
		int target_x = ( ScreenWidth() * 0.5f ) + YRES( 100 );
		int y = YRES( 46 );
		if ( m_VotePanelType == VPT_INGAME )
		{
			target_x = ScreenWidth() - YRES( 180 );
		}
		else if ( m_VotePanelType == VPT_DEBRIEF )
		{
			y = YRES( 320 );
		}
		else if ( m_VotePanelType == VPT_CAMPAIGN_MAP )
		{
			//y = YRES( 320 );
		}
		SetPos( target_x + YRES( 100 ), y );
		vgui::GetAnimationController()->RunAnimationCommand( this, "xpos", target_x, 0, flTransitionTime, vgui::AnimationController::INTERPOLATOR_DEACCEL );

		SetAlpha( 1 );
		vgui::GetAnimationController()->RunAnimationCommand( this, "Alpha", 255, 0, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	}
	SetVisible( true );
}

void CNB_Vote_Panel::UpdateVoteLabels()
{
	m_pProgressBar->SetPos( YRES( 10 ), YRES( 40 ) );

	// update yes/no press buttons
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	m_pMapNameLabel->SetVisible( true );
	m_pYesVotesLabel->SetVisible( true );
	m_pNoVotesLabel->SetVisible( true );

	m_pTitle->SetText( "#asw_vote_mission_title" );

	if ( pPlayer->m_iMapVoted.Get() == 0 )
	{
		if ( gpGlobals->curtime > m_flCheckBindings )
		{
			Q_snprintf(m_szVoteYesKey, sizeof(m_szVoteYesKey), "%s", ASW_FindKeyBoundTo( "vote_yes" ) );
			Q_strupr(m_szVoteYesKey);
			Q_snprintf(m_szVoteNoKey, sizeof(m_szVoteNoKey), "%s", ASW_FindKeyBoundTo( "vote_no" ) );
			Q_strupr(m_szVoteNoKey);

			m_flCheckBindings += 1.0f;
		}		
		// copy the found key into wchar_t format (localize it if it's a token rather than a normal keyname)
		wchar_t keybuffer[ 12 ];
		wchar_t buffer[64];
		g_pVGuiLocalize->ConvertANSIToUnicode( m_szVoteYesKey, keybuffer, sizeof( keybuffer ) );		
		g_pVGuiLocalize->ConstructString( buffer, sizeof(buffer), g_pVGuiLocalize->Find("#press_to_vote_yes"), 1, keybuffer );
		m_pPressToVoteYesLabel->SetText( buffer );

		g_pVGuiLocalize->ConvertANSIToUnicode( m_szVoteNoKey, keybuffer, sizeof( keybuffer ) );		
		g_pVGuiLocalize->ConstructString( buffer, sizeof(buffer), g_pVGuiLocalize->Find("#press_to_vote_no"), 1, keybuffer );
		m_pPressToVoteNoLabel->SetText( buffer );
	}
	else if ( pPlayer->m_iMapVoted.Get() == 1 )
	{
		m_pPressToVoteYesLabel->SetText( "#asw_you_voted_no" );
		m_pPressToVoteNoLabel->SetText( "" );
	}
	else
	{
		m_pPressToVoteYesLabel->SetText( "#asw_you_voted_yes" );
		m_pPressToVoteNoLabel->SetText( "" );
	}

	// update timer
	/*
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
	*/
	m_pCounterLabel->SetText( "" );

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
			m_pTitle->SetText( "#asw_vote_mission_title" );
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
					_snwprintf( wbuffer, sizeof( wbuffer ), L"%s", wmapname );
				}
				else
				{
					_snwprintf( wbuffer, sizeof( wbuffer ), L"%s", wmapname );
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
			m_pTitle->SetText( "#asw_vote_saved_title" );
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

void CNB_Vote_Panel::UpdateVote()
{
	UpdateVotePanelStatus();
	UpdateVisibility();

	if ( m_VotePanelStatus == VPS_NONE )
		return;

	UpdateProgressBar();

	if ( m_VotePanelStatus == VPS_VOTE )
	{
		UpdateVoteLabels();
	}
	else if ( m_VotePanelStatus == VPS_RESTART )
	{
		UpdateRestartLabels();
	}
	else if ( m_VotePanelStatus == VPS_TECH_FAIL )
	{
		UpdateTechFailLabels();
	}
}

void CNB_Vote_Panel::UpdateRestartLabels()
{
	m_pProgressBar->SetPos( YRES( 10 ), YRES( 50 ) );
	m_pCounterLabel->SetPos( YRES( 10 ), YRES( 35 ) );

	m_pTitle->SetText( "#asw_ingame_restart_title" );
	int iSecondsLeft = ASWGameRules()->GetRestartingMissionTime() - gpGlobals->curtime;
	if (iSecondsLeft != m_iSecondsLeft)
	{
		m_iSecondsLeft = iSecondsLeft;
		char buffer[8];
		Q_snprintf(buffer, sizeof(buffer), "%d", iSecondsLeft);

		wchar_t wnumber[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

		wchar_t wbuffer[96];		
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_ingame_restart_in"), 1,
			wnumber);
		m_pCounterLabel->SetText(wbuffer);
	}
	m_pMapNameLabel->SetVisible( false );
	m_pYesVotesLabel->SetVisible( false );
	m_pNoVotesLabel->SetVisible( false );
	m_pPressToVoteYesLabel->SetVisible( false );
	m_pPressToVoteNoLabel->SetVisible( false );
}

void CNB_Vote_Panel::UpdateTechFailLabels()
{
	m_pProgressBar->SetPos( YRES( 10 ), YRES( 60 ) );
	m_pCounterLabel->SetPos( YRES( 10 ), YRES( 45 ) );
			
	m_pTitle->SetText( "#asw_debrief_failed" );
	int iSecondsLeft = ASWGameRules()->GetRestartingMissionTime() - gpGlobals->curtime;
	if (iSecondsLeft != m_iSecondsLeft)
	{
		m_iSecondsLeft = iSecondsLeft;
		char buffer[8];
		Q_snprintf(buffer, sizeof(buffer), "%d", iSecondsLeft);

		wchar_t wnumber[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

		wchar_t wbuffer[96];		
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_ingame_restart_in"), 1,
			wnumber);
		m_pCounterLabel->SetText(wbuffer);
	}
	m_pMapNameLabel->SetText( "#asw_no_live_tech" );
	m_pMapNameLabel->SetVisible( true );
	m_pYesVotesLabel->SetVisible( false );
	m_pNoVotesLabel->SetVisible( false );
	m_pPressToVoteYesLabel->SetVisible( false );
	m_pPressToVoteNoLabel->SetVisible( false );
}

void CNB_Vote_Panel::OnCommand( char const *cmd )
{
	if ( !Q_stricmp( cmd, "VoteYes" ) )
	{
		//GetParent()->SetVisible(false);
		//GetParent()->MarkForDeletion();
		engine->ClientCmd("vote_yes");
	}
	else if ( !Q_stricmp( cmd, "VoteNo" ) )
	{
		//GetParent()->SetVisible(false);
		//GetParent()->MarkForDeletion();
		engine->ClientCmd("vote_no");
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}

extern ConVar asw_vote_duration;

void CNB_Vote_Panel::UpdateProgressBar()
{
	float flProgress = 1.0f;
	if ( m_VotePanelStatus == VPS_VOTE )
	{
		flProgress = (float) ASWGameRules()->GetCurrentVoteTimeLeft() / asw_vote_duration.GetFloat();
	}
	else if ( m_VotePanelStatus == VPS_RESTART || m_VotePanelStatus == VPS_TECH_FAIL )
	{
		flProgress = (float) ( ASWGameRules()->GetRestartingMissionTime() - gpGlobals->curtime ) / 5.9f;
	}

	m_pProgressBar->SetProgress( flProgress );
}

CNB_Progress_Bar::CNB_Progress_Bar( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_flProgress = 0.0f;
}

void CNB_Progress_Bar::SetProgress( float flProgress )
{
	m_flProgress = clamp<float>( flProgress, 0.0f, 1.0f );
}

void CNB_Progress_Bar::Paint()
{
	BaseClass::Paint();

	if ( m_nProgressBarBG == -1 || m_nProgressBar == -1 )
		return;

	int bar_left, bar_top, bar_width, bar_height;
	GetBounds( bar_left, bar_top, bar_width, bar_height );
	bar_left = bar_top = 0;

	// paint progress bar
	vgui::surface()->DrawSetColor(Color(255,255,255,255));
	vgui::surface()->DrawSetTexture(m_nProgressBarBG);	
	vgui::Vertex_t ppointsbg[4] = 
	{ 
		vgui::Vertex_t( Vector2D(bar_left,				bar_top),					Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top),					Vector2D(1,0) ), 
		vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top + bar_height),		Vector2D(1,1) ),
		vgui::Vertex_t( Vector2D(bar_left,				bar_top + bar_height),		Vector2D(0,1) )
	};
	vgui::surface()->DrawTexturedPolygon( 4, ppointsbg );

	bar_width *= m_flProgress;
	vgui::surface()->DrawSetTexture(m_nProgressBar);
	vgui::Vertex_t ppoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(bar_left,				bar_top),					Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top),					Vector2D(m_flProgress,0) ), 
		vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top + bar_height),		Vector2D(m_flProgress,1) ),
		vgui::Vertex_t( Vector2D(bar_left,				bar_top + bar_height),		Vector2D(0,1) )
	};
	vgui::surface()->DrawTexturedPolygon( 4, ppoints );
}