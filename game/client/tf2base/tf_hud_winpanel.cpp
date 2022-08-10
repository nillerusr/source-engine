
//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_hud_winpanel.h"
#include "tf_hud_statpanel.h"
#include "tf_spectatorgui.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "engine/IEngineSound.h"
#include "c_tf_playerresource.h"
#include "c_team.h"
#include "tf_clientscoreboard.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "vgui_avatarimage.h"
#include "fmtstr.h"
#include "teamplayroundbased_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT_DEPTH( CTFWinPanel, 1 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFWinPanel::CTFWinPanel( const char *pElementName ) : EditablePanel( NULL, "WinPanel" ), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	m_bShouldBeVisible = false;
	SetAlpha( 0 );
	SetScheme( "ClientScheme" );

	m_pTeamScorePanel = new EditablePanel( this, "TeamScoresPanel" );
	m_flTimeUpdateTeamScore = 0;
	m_iBlueTeamScore = 0;
	m_iRedTeamScore = 0;

	RegisterForRenderGroup( "mid" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWinPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWinPanel::Reset()
{
	m_bShouldBeVisible = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWinPanel::Init()
{
	// listen for events
	ListenForGameEvent( "teamplay_win_panel" );
	ListenForGameEvent( "teamplay_round_start" );
	ListenForGameEvent( "teamplay_game_over" );
	ListenForGameEvent( "tf_game_over" );

	m_bShouldBeVisible = false;

	CHudElement::Init();
}

void CTFWinPanel::SetVisible( bool state )
{
	if ( state == IsVisible() )
		return;

	if ( state )
	{
		HideLowerPriorityHudElementsInGroup( "mid" );
	}
	else
	{
		UnhideLowerPriorityHudElementsInGroup( "mid" );
	}

	BaseClass::SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWinPanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( Q_strcmp( "teamplay_round_start", pEventName ) == 0 )
	{
		m_bShouldBeVisible = false;
	}
	else if ( Q_strcmp( "teamplay_game_over", pEventName ) == 0 )
	{
		m_bShouldBeVisible = false;
	}
	else if ( Q_strcmp( "tf_game_over", pEventName ) == 0 )
	{
		m_bShouldBeVisible = false;
	}
	else if ( Q_strcmp( "teamplay_win_panel", pEventName ) == 0 )
	{
		if ( !g_PR )
			return;

		int iWinningTeam = event->GetInt( "winning_team" );
		int iWinReason = event->GetInt( "winreason" );
		int iFlagCapLimit = event->GetInt( "flagcaplimit" );
		bool bRoundComplete = (bool) event->GetInt( "round_complete" );
		int iRoundsRemaining = event->GetInt( "rounds_remaining" );

		LoadControlSettings( "resource/UI/WinPanel.res" );		
		InvalidateLayout( false, true );

		SetDialogVariable( "WinningTeamLabel", "" );
		SetDialogVariable( "AdvancingTeamLabel", "" );
		SetDialogVariable( "WinReasonLabel", "" );
		SetDialogVariable( "DetailsLabel", "" );

		vgui::ImagePanel *pImagePanelBG = dynamic_cast<vgui::ImagePanel *>( FindChildByName("WinPanelBG") );
		Assert( pImagePanelBG );
		if ( !pImagePanelBG )
			return;

		// set the appropriate background image and label text
		const char *pTeamLabel = NULL;
		const char *pTopPlayersLabel = NULL;
		const wchar_t *pLocalizedTeamName = NULL;
		// this is an area defense, but not a round win, if this was a successful defend until time limit but not a complete round
		bool bIsAreaDefense = ( ( WINREASON_DEFEND_UNTIL_TIME_LIMIT == iWinReason ) && !bRoundComplete );
		switch ( iWinningTeam )
		{
		case TF_TEAM_BLUE:
			pImagePanelBG->SetImage( "../hud/winpanel_blue_bg_main.vmt" );
			pTeamLabel = ( bRoundComplete ? "#Winpanel_BlueWins" : ( bIsAreaDefense ? "#Winpanel_BlueDefends" : "#Winpanel_BlueAdvances" ) );
			pTopPlayersLabel = "#Winpanel_BlueMVPs";
			pLocalizedTeamName =  g_pVGuiLocalize->Find( "TF_BlueTeam_Name" );
			break;
		case TF_TEAM_RED:
			pImagePanelBG->SetImage( "../hud/winpanel_red_bg_main.vmt" );
			pTeamLabel = ( bRoundComplete ? "#Winpanel_RedWins" : ( bIsAreaDefense ? "#Winpanel_RedDefends" : "#Winpanel_RedAdvances" ) );
			pTopPlayersLabel = "#Winpanel_RedMVPs";
			pLocalizedTeamName =  g_pVGuiLocalize->Find( "TF_RedTeam_Name" );
			break;
		case TEAM_UNASSIGNED:	// stalemate
			pImagePanelBG->SetImage( "../hud/winpanel_black_bg_main.vmt" );
			pTeamLabel = "#Winpanel_Stalemate";
			pTopPlayersLabel = "#Winpanel_TopPlayers";
			break;
		default:
			Assert( false );
			break;
		}

		SetDialogVariable( bRoundComplete ? "WinningTeamLabel" : "AdvancingTeamLabel", g_pVGuiLocalize->Find( pTeamLabel ) );
		SetDialogVariable( "TopPlayersLabel", g_pVGuiLocalize->Find( pTopPlayersLabel ) );

		wchar_t wzWinReason[256]=L"";
		switch ( iWinReason )
		{
		case WINREASON_ALL_POINTS_CAPTURED:
			g_pVGuiLocalize->ConstructString( wzWinReason, sizeof( wzWinReason ), g_pVGuiLocalize->Find( "#Winreason_AllPointsCaptured" ), 1, pLocalizedTeamName );
			break;
		case WINREASON_FLAG_CAPTURE_LIMIT:
			{
				wchar_t wzFlagCaptureLimit[16];
				_snwprintf( wzFlagCaptureLimit, ARRAYSIZE( wzFlagCaptureLimit), L"%i", iFlagCapLimit );
				g_pVGuiLocalize->ConstructString( wzWinReason, sizeof( wzWinReason ), g_pVGuiLocalize->Find( "#Winreason_FlagCaptureLimit" ), 2, 
					pLocalizedTeamName, wzFlagCaptureLimit );
			}			
			break;
		case WINREASON_OPPONENTS_DEAD:
			g_pVGuiLocalize->ConstructString( wzWinReason, sizeof( wzWinReason ), g_pVGuiLocalize->Find( "#Winreason_OpponentsDead" ), 1, pLocalizedTeamName );
			break;
		case WINREASON_DEFEND_UNTIL_TIME_LIMIT:
			g_pVGuiLocalize->ConstructString( wzWinReason, sizeof( wzWinReason ), g_pVGuiLocalize->Find( "#Winreason_DefendedUntilTimeLimit" ), 1, pLocalizedTeamName );
			break;
		case WINREASON_STALEMATE:
			g_pVGuiLocalize->ConstructString( wzWinReason, sizeof( wzWinReason ), g_pVGuiLocalize->Find( "#Winreason_Stalemate" ), 0 );
			break;	
		default:
			Assert( false );
			break;
		}
		SetDialogVariable( "WinReasonLabel", wzWinReason );

		if ( !bRoundComplete && ( WINREASON_STALEMATE != iWinReason ) )
		{			
			// if this was a mini-round, show # of capture points remaining
			wchar_t wzNumCapturesRemaining[16];
			wchar_t wzCapturesRemainingMsg[256]=L"";
			_snwprintf( wzNumCapturesRemaining, ARRAYSIZE( wzNumCapturesRemaining ), L"%i", iRoundsRemaining );
			g_pVGuiLocalize->ConstructString( wzCapturesRemainingMsg, sizeof( wzCapturesRemainingMsg ), 
				g_pVGuiLocalize->Find( 1 == iRoundsRemaining ? "#Winpanel_CapturePointRemaining" : "Winpanel_CapturePointsRemaining" ),
				1, wzNumCapturesRemaining );
			SetDialogVariable( "DetailsLabel", wzCapturesRemainingMsg );
		}
		else if ( ( WINREASON_ALL_POINTS_CAPTURED == iWinReason ) || ( WINREASON_FLAG_CAPTURE_LIMIT == iWinReason ) )
		{
			// if this was a full round that ended with point capture or flag capture, show the winning cappers
			const char *pCappers = event->GetString( "cappers" );
			int iCappers = Q_strlen( pCappers );
			if ( iCappers > 0 )
			{	
				char szPlayerNames[256]="";
				wchar_t wzPlayerNames[256]=L"";
				wchar_t wzCapMsg[512]=L"";
				for ( int i = 0; i < iCappers; i++ )
				{
					Q_strncat( szPlayerNames, g_PR->GetPlayerName( (int) pCappers[i] ), ARRAYSIZE( szPlayerNames ) );
					if ( i < iCappers - 1 )
					{
						Q_strncat( szPlayerNames, ", ", ARRAYSIZE( szPlayerNames ) );
					}
				}
				g_pVGuiLocalize->ConvertANSIToUnicode( szPlayerNames, wzPlayerNames, sizeof( wzPlayerNames ) );
				g_pVGuiLocalize->ConstructString( wzCapMsg, sizeof( wzCapMsg ), g_pVGuiLocalize->Find( "#Winpanel_WinningCapture" ), 1, wzPlayerNames );
				SetDialogVariable( "DetailsLabel", wzCapMsg );
			}
		}

		// get the current & previous team scores
		int iBlueTeamPrevScore = event->GetInt( "blue_score_prev", 0 );
		int iRedTeamPrevScore = event->GetInt( "red_score_prev", 0 );
		m_iBlueTeamScore = event->GetInt( "blue_score", 0 );
		m_iRedTeamScore = event->GetInt( "red_score", 0 );
		
		if ( m_pTeamScorePanel )
		{			
			if ( bRoundComplete )
			{
				// set the previous team scores in scoreboard
				m_pTeamScorePanel->SetDialogVariable( "blueteamscore", iBlueTeamPrevScore );
				m_pTeamScorePanel->SetDialogVariable( "redteamscore", iRedTeamPrevScore );

				if ( ( m_iBlueTeamScore != iBlueTeamPrevScore ) || ( m_iRedTeamScore != iRedTeamPrevScore ) )
				{
					// if the new scores are different, set ourselves to update the scoreboard to the new values after a short delay, so players
					// see the scores tick up
					m_flTimeUpdateTeamScore = gpGlobals->curtime + 2.0f;
				}
			}
			// only show team scores if round is complete
			m_pTeamScorePanel->SetVisible( bRoundComplete );
		}

		C_TF_PlayerResource *tf_PR = dynamic_cast<C_TF_PlayerResource *>( g_PR );
		if ( !tf_PR )
			return;

		// look for the top 3 players sent in the event
		for ( int i = 1; i <= 3; i++ )
		{
			bool bShow = false;
			char szPlayerIndexVal[64]="", szPlayerScoreVal[64]="";
			// get player index and round points from the event
			Q_snprintf( szPlayerIndexVal, ARRAYSIZE( szPlayerIndexVal ), "player_%d", i );
			Q_snprintf( szPlayerScoreVal, ARRAYSIZE( szPlayerScoreVal ), "player_%d_points", i );
			int iPlayerIndex = event->GetInt( szPlayerIndexVal, 0 );
			int iRoundScore = event->GetInt( szPlayerScoreVal, 0 );
			// round score of 0 means no player to show for that position (not enough players, or didn't score any points that round)
			if ( iRoundScore > 0 )
				bShow = true;

#if !defined( _X360 )
			CAvatarImagePanel *pPlayerAvatar = dynamic_cast<CAvatarImagePanel *>( FindChildByName( CFmtStr( "Player%dAvatar", i ) ) );

			if ( pPlayerAvatar )
			{
				if ( bShow )
				{
					pPlayerAvatar->SetShouldDrawFriendIcon( false );
					pPlayerAvatar->SetPlayer( iPlayerIndex );
				}

				pPlayerAvatar->SetVisible( bShow );
			}
#endif
			vgui::Label *pPlayerName = dynamic_cast<Label *>( FindChildByName( CFmtStr( "Player%dName", i ) ) );
			vgui::Label *pPlayerClass = dynamic_cast<Label *>( FindChildByName( CFmtStr( "Player%dClass", i ) ) );
			vgui::Label *pPlayerScore = dynamic_cast<Label *>( FindChildByName( CFmtStr( "Player%dScore", i ) ) );
			
			if ( !pPlayerName || !pPlayerClass || !pPlayerScore )
				return;

			if ( bShow )
			{
				// set the player labels to team color
				Color clr = g_PR->GetTeamColor( g_PR->GetTeam( iPlayerIndex ) );				
				pPlayerName->SetFgColor( clr );
				pPlayerClass->SetFgColor( clr );
				pPlayerScore->SetFgColor( clr );

				// set label contents
				pPlayerName->SetText( g_PR->GetPlayerName( iPlayerIndex ) );
				pPlayerClass->SetText( g_aPlayerClassNames[tf_PR->GetPlayerClass( iPlayerIndex )] );
				pPlayerScore->SetText( CFmtStr( "%d", iRoundScore ) );
			}

			// show or hide labels for this player position
			pPlayerName->SetVisible( bShow );
			pPlayerClass->SetVisible( bShow );
			pPlayerScore->SetVisible( bShow );
		}

		m_bShouldBeVisible = true;

		MoveToFront();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies scheme settings
//-----------------------------------------------------------------------------
void CTFWinPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: returns whether panel should be drawn
//-----------------------------------------------------------------------------
bool CTFWinPanel::ShouldDraw()
{
	if ( !m_bShouldBeVisible )
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: panel think method
//-----------------------------------------------------------------------------
void CTFWinPanel::OnThink()
{
	// if we've scheduled ourselves to update the team scores, handle it now
	if ( m_flTimeUpdateTeamScore > 0 && ( gpGlobals->curtime > 	m_flTimeUpdateTeamScore ) && m_pTeamScorePanel )
	{
		// play a sound
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Hud.EndRoundScored" );

		// update the team scores
		m_pTeamScorePanel->SetDialogVariable( "blueteamscore", m_iBlueTeamScore );
		m_pTeamScorePanel->SetDialogVariable( "redteamscore", m_iRedTeamScore );
		m_flTimeUpdateTeamScore = 0;
	}
}