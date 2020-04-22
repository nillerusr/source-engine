//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "sessionlobbydialog.h"
#include "engine/imatchmaking.h"
#include "GameUI_Interface.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ILocalize.h"
#include "BasePanel.h"
#include "matchmakingbasepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CSessionLobbyDialog *g_pLobbyDialog;

//--------------------------------
// CSessionLobbyDialog
//--------------------------------
CSessionLobbyDialog::CSessionLobbyDialog( vgui::Panel *pParent ) : BaseClass( pParent, "SessionLobbyDialog" )
{
	m_Menus[0].SetParent( this );
	m_Menus[0].SetName( "BluePlayers" );
	m_Menus[1].SetParent( this );
	m_Menus[1].SetName( "RedPlayers" );

	m_iLocalTeam = -1;
	m_iActiveMenu = -1;
	m_nHostId = 0;
	m_bHostLobby = false;
	m_bCenterOnScreen = true;

	m_pLobbyStateBg = new vgui::Panel( this, "LobbyStateBg" );
	m_pLobbyStateLabel = new CPropertyLabel( this, "LobbyStateLabel", "" );
	m_pLobbyStateIcon = new CPropertyLabel( this, "LobbyStateIcon", "" );
	m_pHostLabel = new CPropertyLabel( this, "HostLabel", "" );
	m_pHostOptionsPanel = new vgui::EditablePanel( this, "HostOptions" );

	m_pScenarioInfo = new CScenarioInfoPanel( this, "GameScenario" );
	m_pTeamInfos[BLUE_TEAM_LOBBY] = new CScenarioInfoPanel( this, "BlueTeamDescription" );
	m_pTeamInfos[RED_TEAM_LOBBY] = new CScenarioInfoPanel( this, "RedTeamDescription" );

	m_pDialogKeys = NULL;

	g_pLobbyDialog = this;

	m_bStartingGame = false;
	m_nLastPlayersNeeded = 0;

	m_nMinInfoHeight[BLUE_TEAM_LOBBY] = 0;
	m_nMinInfoHeight[RED_TEAM_LOBBY] = 0;
}

CSessionLobbyDialog::~CSessionLobbyDialog()
{
	delete m_pLobbyStateBg;
	delete m_pLobbyStateLabel;
	delete m_pLobbyStateIcon;
	delete m_pHostLabel;
	delete m_pHostOptionsPanel;

	delete m_pScenarioInfo;
	for ( int i = 0; i < TOTAL_LOBBY_TEAMS; ++i )
	{
		delete m_pTeamInfos[i];
	}
}

//---------------------------------------------------------------------
// Purpose: Dialog keys contain session contexts and properties
//---------------------------------------------------------------------
void CSessionLobbyDialog::SetDialogKeys( KeyValues *pKeys )
{
	m_pDialogKeys = pKeys;
	InvalidateLayout();
}

//---------------------------------------------------------------------
// Purpose: Helper to set label text from keyvalues
//---------------------------------------------------------------------
void CSessionLobbyDialog::SetTextFromKeyvalues( CPropertyLabel *pLabel )
{
	KeyValues *pKey = m_pDialogKeys->FindKey( pLabel->m_szPropertyString );
	if ( pKey )
	{
		const char *pString = pKey->GetString( "displaystring", NULL );
		if ( pString )
		{
			pLabel->SetText( pString );
		}
	}
}

//---------------------------------------------------------------------
// Purpose: Center the dialog on the screen
//---------------------------------------------------------------------
void CSessionLobbyDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( !m_pDialogKeys )
		return;

	// Set the label strings according to the keyvalues passed in
	SetTextFromKeyvalues( m_pScenarioInfo->m_pTitle );
	SetTextFromKeyvalues( m_pScenarioInfo->m_pDescOne );
	SetTextFromKeyvalues( m_pScenarioInfo->m_pDescTwo );
	SetTextFromKeyvalues( m_pScenarioInfo->m_pDescThree );
	SetTextFromKeyvalues( m_pScenarioInfo->m_pValueTwo );
	SetTextFromKeyvalues( m_pScenarioInfo->m_pValueThree );

	const char *pDiskName = "unknown";
	KeyValues *pName = m_pDialogKeys->FindKey( "MapDiskNames" );
	if ( pName )
	{
		KeyValues *pScenario = m_pDialogKeys->FindKey( "CONTEXT_SCENARIO" );
		if ( pScenario )
		{
			pDiskName = pName->GetString( pScenario->GetString( "displaystring" ), "unknown" );
		}
	}

	// find the scenario type
	KeyValues *pType = m_pDialogKeys->FindKey( "ScenarioTypes" );
	if ( pType )
	{
		const char *pString = pType->GetString( pDiskName, NULL );
		if ( pString )
		{
			m_pScenarioInfo->m_pSubtitle->SetText( pString );
		}
	}

	// Set the team goals
	KeyValues *pGoals = m_pDialogKeys->FindKey( "TeamGoals" );
	if ( pGoals )
	{
		KeyValues *pTeam = pGoals->FindKey( "Blue" );
		if ( pTeam )
		{
			m_pTeamInfos[BLUE_TEAM_LOBBY]->m_pDescOne->SetText( pTeam->GetString( pDiskName, "" ) );
		}
		pTeam = pGoals->FindKey( "Red" );
		if ( pTeam )
		{
			m_pTeamInfos[RED_TEAM_LOBBY]->m_pDescOne->SetText( pTeam->GetString( pDiskName, "" ) );
		}
	}

	for ( int i = 0; i < TOTAL_LOBBY_TEAMS; ++i )
	{
		UpdatePlayerCountDisplay( i );
	}

	if ( m_bCenterOnScreen )
	{
		MoveToCenterOfScreen();
	}

	// Don't allow player reviews in system link games
	CMatchmakingBasePanel *pBase = dynamic_cast< CMatchmakingBasePanel* >( m_pParent );
	if ( pBase )
	{
		pBase->SetFooterButtonVisible( "#GameUI_PlayerReview", pBase->GetGameType() != GAMETYPE_SYSTEMLINK_MATCH );

		// hide the settings changing if we're in a ranked game
		if ( m_pHostOptionsPanel )
		{
			bool bVisible = pBase->GetGameType() != GAMETYPE_RANKED_MATCH;
			vgui::Label *pSettingsLabel = (vgui::Label *)m_pHostOptionsPanel->FindChildByName("ChangeSettingsButton",true);
			if ( pSettingsLabel )
			{
				pSettingsLabel->SetVisible( bVisible );
			}
			pSettingsLabel = (vgui::Label *)m_pHostOptionsPanel->FindChildByName("ChangeSettingsText",true);
			if ( pSettingsLabel )
			{
				pSettingsLabel->SetVisible( bVisible );
			}
		}
	}
}

//---------------------------------------------------------------------
// Purpose: Parse session properties and contexts from the resource file
//---------------------------------------------------------------------
void CSessionLobbyDialog::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );

	m_nImageBorderWidth = pResourceData->GetInt( "imageborderwidth", 15 );
	m_nTeamspacing = pResourceData->GetInt( "teamspacing", 0 );
	m_bHostLobby = pResourceData->GetInt( "hostlobby", 0 ) != 0;
	m_bCenterOnScreen = pResourceData->GetInt( "center", 1 ) == 1;

	Q_strncpy( m_szCommand, pResourceData->GetString( "commandstring", "NULL" ), sizeof( m_szCommand ) );
}

//---------------------------------------------------------------------
// Purpose: Set up colors and other such stuff
//---------------------------------------------------------------------
void CSessionLobbyDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	Color cLabelColor = pScheme->GetColor( "MatchmakingDialogTitleColor", Color( 0, 0, 0, 255 ) );

	m_pLobbyStateLabel->SetFgColor( cLabelColor );
	m_pHostLabel->SetFgColor( cLabelColor );

	m_pLobbyStateBg->SetBgColor( pScheme->GetColor( "TanDarker", Color( 0, 0, 0, 255 ) ) );
	m_pLobbyStateBg->SetPaintBackgroundType( 2 );

	m_pHostOptionsPanel->SetBgColor( pScheme->GetColor( "TanDarker", Color( 0, 0, 0, 255 ) ) );
	m_pHostOptionsPanel->SetPaintBackgroundType( 2 );

	m_pScenarioInfo->SetBgColor( pScheme->GetColor( "TanDarker", Color( 0, 0, 0, 255 ) ) );
	m_pTeamInfos[BLUE_TEAM_LOBBY]->SetBgColor( pScheme->GetColor( "HudBlueTeam", Color( 0, 0, 0, 255 ) ) );
	m_pTeamInfos[RED_TEAM_LOBBY]->SetBgColor( pScheme->GetColor( "HudRedTeam", Color( 0, 0, 0, 255 ) ) );

	// Cache of these heights so we never go below them when resizing
	m_nMinInfoHeight[BLUE_TEAM_LOBBY] = m_pTeamInfos[BLUE_TEAM_LOBBY]->GetTall();
	m_nMinInfoHeight[RED_TEAM_LOBBY] = m_pTeamInfos[RED_TEAM_LOBBY]->GetTall();

	//Lets set all the labels this panel owns to be the right fgcolor. hooray vgui!
	int iChildren = m_pHostOptionsPanel->GetChildCount();

	for ( int i=0;i<iChildren;i++ )
	{
		vgui::Label *pLabel = dynamic_cast< vgui::Label * >( m_pHostOptionsPanel->GetChild(i) );
		if ( pLabel )
		{
			SETUP_PANEL( pLabel );
			pLabel->SetFgColor( cLabelColor );
		}
	}

	vgui::Label *pPlayerReviewLabel = (vgui::Label *)FindChildByName("PlayerReviewLabel" );
	if ( pPlayerReviewLabel )
	{
		SETUP_PANEL( pPlayerReviewLabel );
		pPlayerReviewLabel->SetFgColor( cLabelColor );
	}

	SetLobbyReadyState( m_nLastPlayersNeeded );
}

void CSessionLobbyDialog::PositionTeamInfos()
{
	// Line up the team info panels and menus
	int x, y;
	int menux, menuy;
	m_pTeamInfos[0]->GetPos( x, y );
	m_Menus[0].GetPos( menux, menuy );

	for ( int i = 1; i < TOTAL_LOBBY_TEAMS; ++i )
	{
		y += m_pTeamInfos[i - 1]->GetTall() + m_nTeamspacing;
		m_pTeamInfos[i]->SetPos( x, y );
		m_Menus[i].SetPos( menux, y );
	}
}

void CSessionLobbyDialog::ActivateNextMenu()
{
	int startIndex = m_iActiveMenu;
	m_Menus[m_iActiveMenu].SetFocus( -1 );
	do
	{
		m_iActiveMenu = (m_iActiveMenu + 1) % TOTAL_LOBBY_TEAMS;
	} while ( m_Menus[m_iActiveMenu].GetItemCount() == 0 && m_iActiveMenu != startIndex );

	m_Menus[m_iActiveMenu].SetFocus( 0 );
}

void CSessionLobbyDialog::ActivatePreviousMenu()
{
	int startIndex = m_iActiveMenu;
	m_Menus[m_iActiveMenu].SetFocus( -1 );
	do
	{
		m_iActiveMenu = m_iActiveMenu ? m_iActiveMenu - 1 : TOTAL_LOBBY_TEAMS - 1;
	} while ( m_Menus[m_iActiveMenu].GetItemCount() == 0 && m_iActiveMenu != startIndex );

	m_Menus[m_iActiveMenu].SetFocus( m_Menus[m_iActiveMenu].GetItemCount() - 1 );
}

void CSessionLobbyDialog::UpdatePlayerCountDisplay( int iTeam )
{
	int ct =  m_Menus[iTeam].GetItemCount();
	wchar_t wszString[32];
	wchar_t *wzPlayersFmt = g_pVGuiLocalize->Find( ct != 1 ? "#TF_ScoreBoard_Players" : "#TF_ScoreBoard_Player" );
	wchar_t wzPlayerCt[8];
	V_snwprintf( wzPlayerCt, ARRAYSIZE( wzPlayerCt ), L"%d", ct );
	g_pVGuiLocalize->ConstructString( wszString, sizeof( wszString ), wzPlayersFmt, 1, wzPlayerCt );

	m_pTeamInfos[iTeam]->m_pSubtitle->SetText( wszString );

	if ( m_nMinInfoHeight[iTeam] == 0 )
	{
		m_nMinInfoHeight[iTeam] = m_pTeamInfos[iTeam]->GetTall();
	}

	int height = max( m_nMinInfoHeight[iTeam], m_Menus[iTeam].GetTall() );
	m_pTeamInfos[iTeam]->SetTall( height );

	PositionTeamInfos();
}

void CSessionLobbyDialog::UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int iTeam, byte cVoiceState, int nPlayersNeeded, bool bHost )
{
	if ( m_iLocalTeam == -1 )
	{
		m_iLocalTeam = iTeam;
	}

	bool bReady = ( nPlayersNeeded == 0 );

	// Look for the player
	int iFoundTeam = -1;
	int iFoundItem = -1;
	CPlayerItem *pFound = NULL;
	for ( int iMenu = 0; iMenu < TOTAL_LOBBY_TEAMS && !pFound; ++iMenu )
	{
		CDialogMenu &menu = m_Menus[iMenu];

		if ( menu.GetItemCount() == 0 )
			continue;

		for ( int idx = 0; idx < menu.GetItemCount(); ++idx )
		{
			CPlayerItem *pPlayerItem = dynamic_cast< CPlayerItem* >( menu.GetItem( idx ) );
			if ( pPlayerItem && pPlayerItem->m_nId == nPlayerId )
			{
				pFound = pPlayerItem;
				iFoundTeam = iMenu;
				iFoundItem = idx;
				break;
			}
		}
	}

	// Update menu and item focus if the player changed teams
	if ( iFoundTeam != iTeam )
	{
		if ( pFound )
		{
			// Remove the player from the current team
			m_Menus[iFoundTeam].RemovePlayerItem( iFoundItem );
			UpdatePlayerCountDisplay( iFoundTeam );
		}

		if ( 0 <= iTeam && iTeam < TOTAL_LOBBY_TEAMS )
		{
			// Add the player to the new team
			m_Menus[iTeam].AddPlayerItem( pName, nPlayerId, cVoiceState, bReady );
			UpdatePlayerCountDisplay( iTeam );
		}

		// if the player joined an empty lobby, set the active team
		if ( m_iActiveMenu == -1 )
		{
			m_iActiveMenu = iTeam;
			m_Menus[m_iActiveMenu].SetFocus( 0 );
		}

		// update the highlight position
		if ( iFoundTeam == m_iActiveMenu )
		{
			CDialogMenu &activeMenu = m_Menus[m_iActiveMenu];
			int iActive = activeMenu.GetActiveItemIndex();

			if ( iActive == iFoundItem )
			{
				// The changed player was also the highlighted player
				if ( iTeam >= 0 )
				{
					// Move the highlight to the player's new position
					activeMenu.SetFocus( -1 );
					m_iActiveMenu = iTeam;
					m_Menus[m_iActiveMenu].SetFocus( m_Menus[m_iActiveMenu].GetItemCount() - 1 );
				}
				else
				{
					// player left the game, move the highlight to the next filled slot
					if ( iActive >= activeMenu.GetItemCount() )
					{
						ActivateNextMenu();
					}
				}
			}
			else if ( iActive > iFoundItem )
			{
				// Need to drop the highlighted index one slot
				m_Menus[m_iActiveMenu].SetFocus( iActive - 1 );
			}
		}
	}
	else
	{
		if ( pFound )
		{
			if ( pFound->m_bVoice != cVoiceState )
			{
				pFound->m_bVoice = cVoiceState;
				pFound->InvalidateLayout();
			}
		}
	}

	if ( bHost )
	{
		wchar_t wszString[MAX_PATH];
		wchar_t wszHostname[MAX_PATH];
		wchar_t *wzHostFmt = g_pVGuiLocalize->Find( "#TF_Lobby_Host" );
		g_pVGuiLocalize->ConvertANSIToUnicode( pName, wszHostname, sizeof( wszHostname ) );

		V_snwprintf( wszString, ARRAYSIZE(wszString), L"%s\n%s", wzHostFmt, wszHostname );

		m_pHostLabel->SetText( wszString );
		m_nHostId = nPlayerId;
	}

	int iPlayersNeeded = ( bReady ) ? 0 : nPlayersNeeded;
	SetLobbyReadyState( iPlayersNeeded );
	m_nLastPlayersNeeded = iPlayersNeeded;

	InvalidateLayout( true, false );
}

void CSessionLobbyDialog::SetLobbyReadyState( int nPlayersNeeded )
{
	// check if the host is allowed to start the game
	if ( nPlayersNeeded <= 0 )
	{
		if ( m_bHostLobby )
		{
			m_pLobbyStateLabel->SetText( "#TF_PressStart" );
			m_pLobbyStateIcon->SetText( "#GameUI_Icons_START" );
		}
		else
		{
			m_pLobbyStateLabel->SetText( "#TF_WaitingForHost" );
			m_pLobbyStateIcon->SetText( "#TF_Icon_Alert" );

			m_bStartingGame = false;	// client guesses that they can change teams
		}
	}
	else
	{
		wchar_t wszWaiting[64];
		wchar_t *wzWaitingFmt;
		if ( nPlayersNeeded == 1 )
		{
			wzWaitingFmt = g_pVGuiLocalize->Find( "#TF_WaitingForPlayerFmt" );
		}
		else
		{
			wzWaitingFmt = g_pVGuiLocalize->Find( "#TF_WaitingForPlayersFmt" );
		}
		wchar_t wzPlayers[8];
		V_snwprintf( wzPlayers, ARRAYSIZE( wzPlayers ), L"%d", nPlayersNeeded );
		g_pVGuiLocalize->ConstructString( wszWaiting, sizeof( wszWaiting ), wzWaitingFmt, 1, wzPlayers );
		m_pLobbyStateLabel->SetText( wszWaiting );
		m_pLobbyStateIcon->SetText( "#TF_Icon_Alert" );

		// If we were starting and dropped below min players, cancel
		SetStartGame( false );
	}
}

void CSessionLobbyDialog::UpdateCountdown( int seconds )
{
	if ( seconds == -1 )
	{
		// countdown was canceled
		SetLobbyReadyState( 0 );
		return;
	}

	// Set the text in the countdown label
	wchar_t wszCountdown[MAX_PATH];
	wchar_t wszSeconds[MAX_PATH];
	wchar_t *wzCountdownFmt;
	if ( seconds != 1 )
	{
		wzCountdownFmt = g_pVGuiLocalize->Find( "#TF_StartingInSecs" );
	}
	else
	{
		wzCountdownFmt = g_pVGuiLocalize->Find( "#TF_StartingInSec" );
	}
	V_snwprintf( wszSeconds, ARRAYSIZE( wszSeconds ), L"%d", seconds );
	g_pVGuiLocalize->ConstructString( wszCountdown, sizeof( wszCountdown ), wzCountdownFmt, 1, wszSeconds );

	m_pLobbyStateLabel->SetText( wszCountdown );

	if ( !m_bHostLobby )
	{
		m_bStartingGame = true;	// client guesses that they can't change teams
	}
}

//-----------------------------------------------------------------
// Purpose: Send key presses to the dialog's menu
//-----------------------------------------------------------------
void CSessionLobbyDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	CDialogMenu *pMenu = &m_Menus[m_iActiveMenu];
	if ( !pMenu )
		return;

	int idx = pMenu->GetActiveItemIndex();
	int itemCt = pMenu->GetItemCount();

	CPlayerItem *pItem = dynamic_cast< CPlayerItem* >( pMenu->GetItem( idx ) );
	if ( !pItem )
		return;

	SetDeleteSelfOnClose( true );

	switch( code )
	{
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case STEAMCONTROLLER_DPAD_DOWN:
		if ( idx >= itemCt - 1 )
		{
			ActivateNextMenu();
		}
		else
		{
			pMenu->HandleKeyCode( code );
		}
		break;

	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case STEAMCONTROLLER_DPAD_UP:
		if ( idx <= 0 )
		{
			ActivatePreviousMenu();
		}
		else
		{
			pMenu->HandleKeyCode( code );
		}
		break;

	case KEY_XBUTTON_A:
	case STEAMCONTROLLER_A:
#ifdef _X360
		XShowGamerCardUI( XBX_GetPrimaryUserId(), pItem->m_nId );
#endif
		break;

	case KEY_XBUTTON_RIGHT_SHOULDER:
#ifdef _X360
		{
			// Don't allow player reviews in system link games
			CMatchmakingBasePanel *pBase = dynamic_cast< CMatchmakingBasePanel* >( m_pParent );
			if ( pBase && pBase->GetGameType() == GAMETYPE_SYSTEMLINK_MATCH )
				break;
		}

		XShowPlayerReviewUI( XBX_GetPrimaryUserId(), pItem->m_nId );
#endif
		break;

	case KEY_XBUTTON_LEFT_SHOULDER:
		{
			// Don't kick ourselves
			if ( m_bHostLobby )
			{
				if ( pItem && ((CPlayerItem*)pItem)->m_nId != m_nHostId )
				{
					GameUI().ShowMessageDialog( MD_KICK_CONFIRMATION, this );
				}
				else
				{
					vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
				}
			}
		}
		break;

	case KEY_XBUTTON_B:
	case STEAMCONTROLLER_B:
		GameUI().ShowMessageDialog( MD_EXIT_SESSION_CONFIRMATION, this );

		if ( m_bHostLobby )
		{
			SetStartGame( false );
		}
		break;

	case KEY_XBUTTON_X:
	case STEAMCONTROLLER_X:
		if ( m_bStartingGame )
		{
			// We think we're loading the game, so play deny sound
			vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
		}
		else
		{
			matchmaking->ChangeTeam( NULL );
		}
		break;

	case KEY_XBUTTON_Y:
	case STEAMCONTROLLER_Y:
		if ( m_bHostLobby )
		{
			// Don't allow settings changes in ranked games
			CMatchmakingBasePanel *pBase = dynamic_cast< CMatchmakingBasePanel* >( m_pParent );
			if ( pBase && pBase->GetGameType() == GAMETYPE_RANKED_MATCH )
				break;

			SetDeleteSelfOnClose( false );
			OnCommand( "SessionOptions_Modify" );
			SetStartGame( false );
		}
		break;

	case KEY_XBUTTON_START:
		SetStartGame( !m_bStartingGame );
		break;

	default:
		pMenu->HandleKeyCode( code );
		break;
	}
}

//---------------------------------------------------------------------
// Purpose: start and stop the countdown
//---------------------------------------------------------------------
void CSessionLobbyDialog::SetStartGame( bool bStartGame )
{
	if ( !m_bHostLobby )
		return;

	bool bCanStart = true;
	if ( m_bStartingGame != bStartGame )
	{
		if ( bStartGame )
		{
			m_bStartingGame = matchmaking->StartGame();
			bCanStart = m_bStartingGame;
		}
		else
		{
			if ( matchmaking->CancelStartGame() )
			{
				m_bStartingGame = false;
			}
		}

		// If we can start the game but haven't started yet, show the "Start Game" label
		bool bShowStartGame = bCanStart && !m_bStartingGame;

		// show/hide the "start game" and countdown hint label based on state
		vgui::Label *pStartGame = (vgui::Label *)m_pHostOptionsPanel->FindChildByName("StartGameText" );
		if ( pStartGame )
		{
			pStartGame->SetVisible( m_bStartingGame == false );
		}

		vgui::Label *pCancelCountdown = (vgui::Label *)m_pHostOptionsPanel->FindChildByName("CancelGameText" );
		if ( pCancelCountdown )
		{
			pCancelCountdown->SetVisible( m_bStartingGame == true );
		}

		if ( bShowStartGame )
		{
			m_pLobbyStateLabel->SetText( "#TF_PressStart" );
			m_pLobbyStateIcon->SetText( "#GameUI_Icons_START" );
		}
	}
}

//---------------------------------------------------------------------
// Purpose: Handle menu commands
//---------------------------------------------------------------------
void CSessionLobbyDialog::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "ReturnToMainMenu" ) )
	{
		matchmaking->KickPlayerFromSession( 0 );
	}
	else if ( !Q_stricmp( pCommand, "KickPlayer" ) )
	{
		CDialogMenu *pMenu = &m_Menus[m_iActiveMenu];
		CPlayerItem *pItem = (CPlayerItem*)pMenu->GetItem( pMenu->GetActiveItemIndex() );
		if ( pItem  )
		{
			matchmaking->KickPlayerFromSession( pItem->m_nId );
		}
	}

	GetParent()->OnCommand( pCommand );
}

static int pnum = 1;

CON_COMMAND( mm_add_player, "Add a player" )
{
	if ( args.ArgC() >= 5 )
	{
		int team = atoi( args[1] );
		const char *pName = args[2];
		uint32 id = atoi( args[3] );
		byte cVoiceState = atoi( args[4] ) != 0;
		int nPlayersNeeded = atoi( args[5] );
		g_pLobbyDialog->UpdatePlayerInfo( id, pName, team, cVoiceState, nPlayersNeeded, false );
	}
	else if ( args.ArgC() >= 1 )
	{
		char name[ 32 ];
		int team = pnum & 0x1;
		int id = pnum++;
		if ( args.ArgC() >= 2 )
		{
			team = atoi( args[ 1 ] );
			if ( args.ArgC() >= 3 )
			{
				id = atoi( args[2] );
			}
		}
		Q_snprintf( name, sizeof( name ), "Player%d", pnum );
		g_pLobbyDialog->UpdatePlayerInfo( id, name, team, true, 0, false );
	}
}