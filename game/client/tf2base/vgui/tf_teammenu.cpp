//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include <game/client/iviewport.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>
#include <filesystem.h>

#include "vguicenterprint.h"
#include "tf_controls.h"
#include "basemodelpanel.h"
#include "tf_teammenu.h"
#include <convar.h>
#include "IGameUIFuncs.h" // for key bindings
#include "hud.h" // for gEngfuncs
#include "c_tf_player.h"
#include "tf_gamerules.h"
#include "c_team.h"
#include "tf_hud_notification_panel.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFTeamButton::CTFTeamButton( vgui::Panel *parent, const char *panelName ) : CTFButton( parent, panelName, "" )
{
	m_szModelPanel[0] = '\0';
	m_iTeam = TEAM_UNASSIGNED;
	m_flHoverTimeToWait = -1;
	m_flHoverTime = -1;
	m_bMouseEntered = false;
	m_bTeamDisabled = false;

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	Q_strncpy( m_szModelPanel, inResourceData->GetString( "associated_model", "" ), sizeof( m_szModelPanel ) );
	m_iTeam = inResourceData->GetInt( "team", TEAM_UNASSIGNED );
	m_flHoverTimeToWait = inResourceData->GetFloat( "hover", -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetDefaultColor( GetFgColor(), Color( 0, 0, 0, 0 ) );
	SetArmedColor( GetButtonFgColor(), Color( 0, 0, 0, 0 ) );
	SetDepressedColor( GetButtonFgColor(), Color( 0, 0, 0, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::SendAnimation( const char *pszAnimation )
{
	Panel *pParent = GetParent();
	if ( pParent )
	{
		CModelPanel *pModel = dynamic_cast< CModelPanel* >( pParent->FindChildByName( m_szModelPanel ) );
		if ( pModel )
		{
			KeyValues *kvParms = new KeyValues( "SetAnimation" );
			if ( kvParms )
			{
				kvParms->SetString( "animation", pszAnimation );
				PostMessage( pModel, kvParms );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::SetDefaultAnimation( const char *pszName )
{
	Panel *pParent = GetParent();
	if ( pParent )
	{
		CModelPanel *pModel = dynamic_cast< CModelPanel* >( pParent->FindChildByName( m_szModelPanel ) );
		if ( pModel )
		{
			pModel->SetDefaultAnimation( pszName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFTeamButton::IsTeamFull()
{
	bool bRetVal = false;

	if ( ( m_iTeam > TEAM_UNASSIGNED ) && GetParent() )
	{
		CTFTeamMenu *pTeamMenu = dynamic_cast< CTFTeamMenu* >( GetParent() );
		if ( pTeamMenu )
		{
			bRetVal = ( m_iTeam == TF_TEAM_BLUE ) ? pTeamMenu->IsBlueTeamDisabled() : pTeamMenu->IsRedTeamDisabled();
		}
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();

	SetMouseEnteredState( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::OnCursorExited()
{
	BaseClass::OnCursorExited();

	SetMouseEnteredState( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::SetMouseEnteredState( bool state )
{
	if ( state )
	{
		m_bMouseEntered = true;

		if ( m_flHoverTimeToWait > 0 )
		{
			m_flHoverTime = gpGlobals->curtime + m_flHoverTimeToWait;
		}
		else
		{
			m_flHoverTime = -1;
		}

		if ( m_bTeamDisabled )
		{
			SendAnimation( "enter_disabled" );
		}
		else
		{
			SendAnimation( "enter_enabled" );
		}
	}
	else
	{
		m_bMouseEntered = false;
		m_flHoverTime = -1;

		if ( m_bTeamDisabled )
		{
			SendAnimation( "exit_disabled" );
		}
		else
		{
			SendAnimation( "exit_enabled" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamButton::OnTick()
{
	// check to see if our state has changed
	bool bDisabled = IsTeamFull();

	if ( bDisabled != m_bTeamDisabled )
	{
		m_bTeamDisabled = bDisabled;

		if ( m_bMouseEntered )
		{
			// something has changed, so reset our state
			SetMouseEnteredState( true );
		}
		else
		{
			// the mouse isn't currently over the button, but we should update the status
			if ( m_bTeamDisabled )
			{
				SendAnimation( "idle_disabled" );
			}
			else
			{
				SendAnimation( "idle_enabled" );
			}
		}
	}

	if ( ( m_flHoverTime > 0 ) && ( m_flHoverTime < gpGlobals->curtime ) )
	{
		m_flHoverTime = -1;

		if ( m_bTeamDisabled )
		{
			SendAnimation( "hover_disabled" );
		}
		else
		{
			SendAnimation( "hover_enabled" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFTeamMenu::CTFTeamMenu( IViewPort *pViewPort ) : CTeamMenu( pViewPort )
{
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetVisible( false );
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );

	m_iTeamMenuKey = BUTTON_CODE_INVALID;

	m_pBlueTeamButton = new CTFTeamButton( this, "teambutton0" );
	m_pRedTeamButton = new CTFTeamButton( this, "teambutton1" );
	m_pAutoTeamButton = new CTFTeamButton( this, "teambutton2" );
	m_pSpecTeamButton = new CTFTeamButton( this, "teambutton3" );
	m_pSpecLabel = new CTFLabel( this, "TeamMenuSpectate", "" );

#ifdef _X360
	m_pFooter = new CTFFooter( this, "Footer" );
#else
	m_pCancelButton = new CTFButton( this, "CancelButton", "#TF_Cancel" );
#endif

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_bRedDisabled = false;
	m_bBlueDisabled = false;

	LoadControlSettings( "Resource/UI/Teammenu.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFTeamMenu::~CTFTeamMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
void CTFTeamMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/Teammenu.res" );

	Update();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamMenu::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	if ( !C_TFPlayer::GetLocalTFPlayer() )
		return;
	if ( !gameuifuncs || !gViewPortInterface || !engine )
		return;

	if ( bShow )
	{
		if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN && 
			 C_TFPlayer::GetLocalTFPlayer() && 
			 C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TFGameRules()->GetWinningTeam()
			 && C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TEAM_SPECTATOR 
	  		 && C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TEAM_UNASSIGNED )
		{
			SetVisible( false );
			SetMouseInputEnabled( false );

			CHudNotificationPanel *pNotifyPanel = GET_HUDELEMENT( CHudNotificationPanel );
			if ( pNotifyPanel )
			{
				pNotifyPanel->SetupNotifyCustom( "#TF_CantChangeTeamNow", "ico_notify_flag_moving", C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() );
			}

			return;
		}

		gViewPortInterface->ShowPanel( PANEL_CLASS_RED, false );
		gViewPortInterface->ShowPanel( PANEL_CLASS_BLUE, false );

		engine->CheckPoint( "TeamMenu" );

		Activate();
		SetMouseInputEnabled( true );

		// get key bindings if shown
		m_iTeamMenuKey = gameuifuncs->GetButtonCodeForBind( "changeteam" );
		m_iScoreBoardKey = gameuifuncs->GetButtonCodeForBind( "showscores" );

		switch ( C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			if ( IsConsole() )
			{
				m_pBlueTeamButton->OnCursorEntered();
				m_pBlueTeamButton->SetDefaultAnimation( "enter_enabled" );
			}
			GetFocusNavGroup().SetCurrentFocus( m_pBlueTeamButton->GetVPanel(), m_pBlueTeamButton->GetVPanel() );
			break;

		case TF_TEAM_RED:
			if ( IsConsole() )
			{
				m_pRedTeamButton->OnCursorEntered();
				m_pRedTeamButton->SetDefaultAnimation( "enter_enabled" );
			}
			GetFocusNavGroup().SetCurrentFocus( m_pRedTeamButton->GetVPanel(), m_pRedTeamButton->GetVPanel() );
			break;

		default:
			if ( IsConsole() )
			{
				m_pAutoTeamButton->OnCursorEntered();
				m_pAutoTeamButton->SetDefaultAnimation( "enter_enabled" );
			}
			GetFocusNavGroup().SetCurrentFocus( m_pAutoTeamButton->GetVPanel(), m_pAutoTeamButton->GetVPanel() );
			break;
		}
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );

		if ( IsConsole() )
		{
			// Close the door behind us
			CTFTeamButton *pButton = dynamic_cast< CTFTeamButton *> ( GetFocusNavGroup().GetCurrentFocus() );
			if ( pButton )
			{
				pButton->OnCursorExited();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: called to update the menu with new information
//-----------------------------------------------------------------------------
void CTFTeamMenu::Update( void )
{
	BaseClass::Update();

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer && ( pLocalPlayer->GetTeamNumber() != TEAM_UNASSIGNED ) )
	{
#ifdef _X360
		if ( m_pFooter )
		{
			m_pFooter->ShowButtonLabel( "cancel", true );
		}
#else
		if ( m_pCancelButton )
		{
			m_pCancelButton->SetVisible( true );
		}
#endif
	}
	else
	{
#ifdef _X360
		if ( m_pFooter )
		{
			m_pFooter->ShowButtonLabel( "cancel", false );
		}
#else
		if ( m_pCancelButton && m_pCancelButton->IsVisible() )
		{
			m_pCancelButton->SetVisible( false );
		}
#endif
	}
}

#ifdef _X360
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamMenu::Join_Team( const CCommand &args )
{
	if ( args.ArgC() > 1 )
	{
		char cmd[256];
		Q_snprintf( cmd, sizeof( cmd ), "jointeam_nomenus %s", args.Arg( 1 ) );
		OnCommand( cmd );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: chooses and loads the text page to display that describes mapName map
//-----------------------------------------------------------------------------
void CTFTeamMenu::LoadMapPage( const char *mapName )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamMenu::OnKeyCodePressed( KeyCode code )
{
	if ( ( m_iTeamMenuKey != BUTTON_CODE_INVALID && m_iTeamMenuKey == code ) ||
		   code == KEY_XBUTTON_BACK || 
		   code == KEY_XBUTTON_B )
	{
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		if ( pLocalPlayer && ( pLocalPlayer->GetTeamNumber() != TEAM_UNASSIGNED ) )
		{
			ShowPanel( false );
		}
	}
	else if( code == KEY_SPACE )
	{
		engine->ClientCmd( "jointeam auto" );

		ShowPanel( false );
		OnClose();
	}
	else if( code == KEY_XBUTTON_A || code == KEY_XBUTTON_RTRIGGER )
	{
		// select the active focus
		if ( GetFocusNavGroup().GetCurrentFocus() )
		{
			ipanel()->SendMessage( GetFocusNavGroup().GetCurrentFocus()->GetVPanel(), new KeyValues( "PressButton" ), GetVPanel() );
		}
	}
	else if( code == KEY_XBUTTON_RIGHT || code == KEY_XSTICK1_RIGHT )
	{
		CTFTeamButton *pButton;
			
		pButton = dynamic_cast< CTFTeamButton *> ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorExited();
			GetFocusNavGroup().RequestFocusNext( pButton->GetVPanel() );
		}
		else
		{
			GetFocusNavGroup().RequestFocusNext( NULL );
		}

		pButton = dynamic_cast< CTFTeamButton * > ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorEntered();
		}
	}
	else if( code == KEY_XBUTTON_LEFT || code == KEY_XSTICK1_LEFT )
	{
		CTFTeamButton *pButton;

		pButton = dynamic_cast< CTFTeamButton *> ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorExited();
			GetFocusNavGroup().RequestFocusPrev( pButton->GetVPanel() );
		}
		else
		{
			GetFocusNavGroup().RequestFocusPrev( NULL );
		}

		pButton = dynamic_cast< CTFTeamButton * > ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorEntered();
		}
	}
	else if ( m_iScoreBoardKey != BUTTON_CODE_INVALID && m_iScoreBoardKey == code )
	{
		gViewPortInterface->ShowPanel( PANEL_SCOREBOARD, true );
		gViewPortInterface->PostMessageToPanel( PANEL_SCOREBOARD, new KeyValues( "PollHideCode", "code", code ) );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the user picks a team
//-----------------------------------------------------------------------------
void CTFTeamMenu::OnCommand( const char *command )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( Q_stricmp( command, "vguicancel" ) )
	{
		// we're selecting a team, so make sure it's not the team we're already on before sending to the server
		if ( pLocalPlayer && ( Q_strstr( command, "jointeam " ) ) )
		{
			const char *pTeam = command + Q_strlen( "jointeam " );
			int iTeam = TEAM_INVALID;

			if ( Q_stricmp( pTeam, "spectate" ) == 0 )
			{
				iTeam = TEAM_SPECTATOR;
			}
			else if ( Q_stricmp( pTeam, "red" ) == 0 )
			{
				iTeam = TF_TEAM_RED;
			}
			else if ( Q_stricmp( pTeam, "blue" ) == 0 )
			{
				iTeam = TF_TEAM_BLUE;
			}

			if ( iTeam == TF_TEAM_RED && m_bRedDisabled )
			{
				return;
			}

			if ( iTeam == TF_TEAM_BLUE && m_bBlueDisabled )
			{
				return;
			}

			// are we selecting the team we're already on?
			if ( pLocalPlayer->GetTeamNumber() != iTeam )
			{
				engine->ClientCmd( command );
			}
		}
		else if ( pLocalPlayer && ( Q_strstr( command, "jointeam_nomenus " ) ) )
		{
			engine->ClientCmd( command );
		}
	}

	BaseClass::OnCommand( command );
	ShowPanel( false );
	OnClose();
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CTFTeamMenu::OnTick()
{
	// update the number of players on each team

	// enable or disable buttons based on team limit

	C_Team *pRed = GetGlobalTeam( TF_TEAM_RED );
	C_Team *pBlue = GetGlobalTeam( TF_TEAM_BLUE );

	if ( !pRed || !pBlue )
		return;

	// set our team counts
	SetDialogVariable( "bluecount", pBlue->Get_Number_Players() );
	SetDialogVariable( "redcount", pRed->Get_Number_Players() );

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return;

	CTFGameRules *pRules = TFGameRules();

	if ( !pRules )
		return;

	// check if teams are unbalanced
	m_bRedDisabled = m_bBlueDisabled = false;

	int iHeavyTeam, iLightTeam;

	bool bUnbalanced = pRules->AreTeamsUnbalanced( iHeavyTeam, iLightTeam );
	
	int iCurrentTeam = pLocalPlayer->GetTeamNumber();

	if ( ( bUnbalanced && iHeavyTeam == TF_TEAM_RED ) || ( pRules->WouldChangeUnbalanceTeams( TF_TEAM_RED, iCurrentTeam ) ) )
	{
		m_bRedDisabled = true;
	}

	if ( ( bUnbalanced && iHeavyTeam == TF_TEAM_BLUE ) || ( pRules->WouldChangeUnbalanceTeams( TF_TEAM_BLUE, iCurrentTeam ) ) )
	{
		m_bBlueDisabled = true;
	}

	if ( m_pSpecTeamButton && m_pSpecLabel )
	{
		ConVarRef mp_allowspectators( "mp_allowspectators" );
		if ( mp_allowspectators.IsValid() )
		{
			if ( mp_allowspectators.GetBool() )
			{
				if ( !m_pSpecTeamButton->IsVisible() )
				{
					m_pSpecTeamButton->SetVisible( true );
					m_pSpecLabel->SetVisible( true );
				}
			}
			else
			{
				if ( m_pSpecTeamButton->IsVisible() )
				{
					m_pSpecTeamButton->SetVisible( false );
					m_pSpecLabel->SetVisible( false );
				}
			}
		}
	}
}