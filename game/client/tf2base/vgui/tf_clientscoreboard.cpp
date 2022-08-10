//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include <tier1/fmtstr.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/ImageList.h>
#include <game/client/iviewport.h>
#include <KeyValues.h>
#include <filesystem.h>
#include "IGameUIFuncs.h" // for key bindings

#include "tf_controls.h"
#include "tf_shareddefs.h"
#include "tf_clientscoreboard.h"
#include "tf_gamestats_shared.h"
#include "tf_hud_statpanel.h"
#include "c_playerresource.h"
#include "c_tf_playerresource.h"
#include "c_tf_team.h"
#include "c_tf_player.h"
#include "vgui_avatarimage.h"

#if defined ( _X360 )
#include "engine/imatchmaking.h"
#endif

using namespace vgui;

#define SCOREBOARD_MAX_LIST_ENTRIES 12

extern bool IsInCommentaryMode( void );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFClientScoreBoardDialog::CTFClientScoreBoardDialog( IViewPort *pViewPort ) : CClientScoreBoardDialog( pViewPort )
{
	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetScheme( "ClientScheme" );

	m_pPlayerListBlue = new SectionedListPanel( this, "BluePlayerList" );
	m_pPlayerListRed = new SectionedListPanel( this, "RedPlayerList" );
	m_pLabelPlayerName = new CTFLabel( this, "PlayerNameLabel", "" );
	m_pImagePanelHorizLine = new ImagePanel( this, "HorizontalLine" );
	m_pClassImage = new CTFClassImage( this, "ClassImage" );
	m_iImageDead = 0;
	m_iImageDominated = 0;
	m_iImageNemesis = 0;
	
	ListenForGameEvent( "server_spawn" );

	SetDialogVariable( "server", "" );

	SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFClientScoreBoardDialog::~CTFClientScoreBoardDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::PerformLayout()
{
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings("Resource/UI/scoreboard.res");

	if ( m_pImageList )
	{
		m_iImageDead = m_pImageList->AddImage( scheme()->GetImage( "../hud/leaderboard_dead", true ) );
		m_iImageDominated = m_pImageList->AddImage( scheme()->GetImage( "../hud/leaderboard_dominated", true ) );
		m_iImageNemesis = m_pImageList->AddImage( scheme()->GetImage( "../hud/leaderboard_nemesis", true ) );
		
		// resize the images to our resolution
		for (int i = 1; i < m_pImageList->GetImageCount(); i++ )
		{
			int wide = 13, tall = 13;
			m_pImageList->GetImage(i)->SetSize(scheme()->GetProportionalScaledValueEx( GetScheme(), wide ), scheme()->GetProportionalScaledValueEx( GetScheme(),tall ) );
		}
	}

	SetPlayerListImages( m_pPlayerListBlue );
	SetPlayerListImages( m_pPlayerListRed );

	SetBgColor( Color( 0, 0, 0, 0) );
	SetBorder( NULL );
	SetVisible( false );

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::ShowPanel( bool bShow )
{
	// Catch the case where we call ShowPanel before ApplySchemeSettings, eg when
	// going from windowed <-> fullscreen
	if ( m_pImageList == NULL )
	{
		InvalidateLayout( true, true );
	}

	// Don't show in commentary mode
	if ( IsInCommentaryMode() )
	{
		bShow = false;
	}
	
	if ( IsVisible() == bShow )
	{
		return;
	}

	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "global" );

	if ( bShow )
	{		
		SetVisible( true );
		MoveToFront();

		gHUD.LockRenderGroup( iRenderGroup );

		// Clear the selected item, this forces the default to the local player
		SectionedListPanel *pList = GetSelectedPlayerList();
		if ( pList )
		{
			pList->ClearSelection();
		}
	}
	else
	{
		SetVisible( false );

		gHUD.UnlockRenderGroup( iRenderGroup );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets the scoreboard panel
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::Reset()
{
	InitPlayerList( m_pPlayerListBlue );
	InitPlayerList( m_pPlayerListRed );
}

//-----------------------------------------------------------------------------
// Purpose: Inits the player list in a list panel
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::InitPlayerList( SectionedListPanel *pPlayerList )
{
	pPlayerList->SetVerticalScrollbar( false );
	pPlayerList->RemoveAll();
	pPlayerList->RemoveAllSections();
	pPlayerList->AddSection( 0, "Players", TFPlayerSortFunc );
	pPlayerList->SetSectionAlwaysVisible( 0, true );
	pPlayerList->SetSectionFgColor( 0, Color( 255, 255, 255, 255 ) );
	pPlayerList->SetBgColor( Color( 0, 0, 0, 0 ) );
	pPlayerList->SetBorder( NULL );

	// Avatars are always displayed at 32x32 regardless of resolution
	if ( ShowAvatars() )
	{
		pPlayerList->AddColumnToSection( 0, "avatar", "", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, m_iAvatarWidth );
	}
	pPlayerList->AddColumnToSection( 0, "name", "#TF_Scoreboard_Name", 0, m_iNameWidth );
	pPlayerList->AddColumnToSection( 0, "status", "", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, m_iStatusWidth );
	pPlayerList->AddColumnToSection( 0, "nemesis", "", SectionedListPanel::COLUMN_IMAGE, m_iNemesisWidth );
	pPlayerList->AddColumnToSection( 0, "class", "", 0, m_iClassWidth );
	pPlayerList->AddColumnToSection( 0, "score", "#TF_Scoreboard_Score", SectionedListPanel::COLUMN_RIGHT, m_iScoreWidth );
	pPlayerList->AddColumnToSection( 0, "ping", "#TF_Scoreboard_Ping", SectionedListPanel::COLUMN_RIGHT, m_iPingWidth );
}

//-----------------------------------------------------------------------------
// Purpose: Builds the image list to use in the player list
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::SetPlayerListImages( vgui::SectionedListPanel *pPlayerList )
{
	pPlayerList->SetImageList( m_pImageList, false );
	pPlayerList->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the dialog
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::Update()
{
	UpdateTeamInfo();
	UpdatePlayerList();
	UpdateSpectatorList();
	UpdatePlayerDetails();
	MoveToCenterOfScreen();

	// update every second
	m_fNextUpdateTime = gpGlobals->curtime + 1.0f; 
}

//-----------------------------------------------------------------------------
// Purpose: Updates information about teams
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::UpdateTeamInfo()
{
	// update the team sections in the scoreboard
	for ( int teamIndex = TF_TEAM_RED; teamIndex <= TF_TEAM_BLUE; teamIndex++ )
	{
		wchar_t *teamName = NULL;
		C_Team *team = GetGlobalTeam( teamIndex );
		if ( team )
		{
			// choose dialog variables to set depending on team
				const char *pDialogVarTeamScore = NULL;
			const char *pDialogVarTeamPlayerCount = NULL;
			switch ( teamIndex ) {
				case TF_TEAM_RED:
					pDialogVarTeamScore = "redteamscore";
					pDialogVarTeamPlayerCount = "redteamplayercount";
					break;
				case TF_TEAM_BLUE:
					pDialogVarTeamScore = "blueteamscore";
					pDialogVarTeamPlayerCount = "blueteamplayercount";
					break;
				default:
					Assert( false );
					break;
			}

			// update # of players on each team
			wchar_t name[64];
			wchar_t string1[1024];
			wchar_t wNumPlayers[6];
			_snwprintf( wNumPlayers, ARRAYSIZE( wNumPlayers ), L"%i", team->Get_Number_Players() );
			if ( !teamName && team )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( team->Get_Name(), name, sizeof( name ) );
				teamName = name;
			}
			if ( team->Get_Number_Players() == 1 )
			{
				g_pVGuiLocalize->ConstructString( string1, sizeof(string1), g_pVGuiLocalize->Find( "#TF_ScoreBoard_Player" ), 1, wNumPlayers );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( string1, sizeof(string1), g_pVGuiLocalize->Find( "#TF_ScoreBoard_Players" ), 1, wNumPlayers );
			}

			// set # of players for team in dialog
			SetDialogVariable( pDialogVarTeamPlayerCount, string1 );

			// set team score in dialog
			SetDialogVariable( pDialogVarTeamScore, team->Get_Score() );			
		}
	}
}

bool AreEnemyTeams( int iTeam1, int iTeam2 )
{
	if ( iTeam1 == TF_TEAM_RED && iTeam2 == TF_TEAM_BLUE )
		return true;

	if ( iTeam1 == TF_TEAM_BLUE && iTeam2 == TF_TEAM_RED )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the player list
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::UpdatePlayerList()
{
	int iSelectedPlayerIndex = GetLocalPlayerIndex();

	// Save off which player we had selected
	SectionedListPanel *pList = GetSelectedPlayerList();

	if ( pList )
	{
		int itemID = pList->GetSelectedItem();

		if ( itemID >= 0 )
		{
			KeyValues *pInfo = pList->GetItemData( itemID );
			if ( pInfo )
			{
				iSelectedPlayerIndex = pInfo->GetInt( "playerIndex" );
			}
		}
	}	

	m_pPlayerListRed->RemoveAll();
	m_pPlayerListBlue->RemoveAll();

	C_TF_PlayerResource *tf_PR = dynamic_cast<C_TF_PlayerResource *>( g_PR );
	if ( !tf_PR )
		return;
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return;

	int localteam = pLocalPlayer->GetTeamNumber();

	bool bMadeSelection = false;

	for( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; playerIndex++ )
	{
		if( g_PR->IsConnected( playerIndex ) )
		{
			SectionedListPanel *pPlayerList = NULL;
			switch ( g_PR->GetTeam( playerIndex ) )
			{
			case TF_TEAM_BLUE:
				pPlayerList = m_pPlayerListBlue;
				break;
			case TF_TEAM_RED:
				pPlayerList = m_pPlayerListRed;
				break;
			}
			if ( null == pPlayerList )
				continue;			

			const char *szName = tf_PR->GetPlayerName( playerIndex );
			int score = tf_PR->GetTotalScore( playerIndex );

			KeyValues *pKeyValues = new KeyValues( "data" );

			pKeyValues->SetInt( "playerIndex", playerIndex );
			pKeyValues->SetString( "name", szName );
			pKeyValues->SetInt( "score", score );

			// can only see class information if we're on the same team
			if ( !AreEnemyTeams( g_PR->GetTeam( playerIndex ), localteam ) && !( localteam == TEAM_UNASSIGNED ) )
			{
				// class name
				if( g_PR->IsConnected( playerIndex ) )
				{
						int iClass = tf_PR->GetPlayerClass( playerIndex );
					if ( GetLocalPlayerIndex() == playerIndex && !tf_PR->IsAlive( playerIndex ) ) {
						// If this is local player and he is dead, show desired class (which he will spawn as) rather than current class.
						C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
						int iDesiredClass = pPlayer->m_Shared.GetDesiredPlayerClassIndex();
						// use desired class unless it's random -- if random, his future class is not decided until moment of spawn
						if ( TF_CLASS_RANDOM != iDesiredClass )
						{
							iClass = iDesiredClass;
						}
					} 
					else 
					{
						// for non-local players, show the current class
						iClass = tf_PR->GetPlayerClass( playerIndex );
					}

					if( iClass >= TF_FIRST_NORMAL_CLASS && iClass <= TF_LAST_NORMAL_CLASS )
					{
						pKeyValues->SetString( "class", g_aPlayerClassNames[iClass] );
					}
					else
					{
						pKeyValues->SetString( "class", "" );
					}
				}				
			}
			else
			{
				C_TFPlayer *pPlayerOther = ToTFPlayer( UTIL_PlayerByIndex( playerIndex ) );

#ifdef _X360
				bool bUseTruncatedNames = true;
#else
				bool bUseTruncatedNames = false;
#endif

				if ( pPlayerOther && pPlayerOther->m_Shared.IsPlayerDominated( pLocalPlayer->entindex() ) )
				{
					// if local player is dominated by this player, show a nemesis icon
					pKeyValues->SetInt( "nemesis", m_iImageNemesis );
					pKeyValues->SetString( "class", bUseTruncatedNames ? "#TF_Nemesis_lodef" : "#TF_Nemesis" );
				}
				else if ( pLocalPlayer->m_Shared.IsPlayerDominated( playerIndex) )
				{
					// if this player is dominated by the local player, show the domination icon
					pKeyValues->SetInt( "nemesis", m_iImageDominated );
					pKeyValues->SetString( "class", bUseTruncatedNames ? "#TF_Dominated_lodef" : "#TF_Dominated" );
				}
			}

			// display whether player is alive or dead (all players see this for all other players on both teams)
			pKeyValues->SetInt( "status", tf_PR->IsAlive( playerIndex ) ?  0 : m_iImageDead );

			if ( g_PR->GetPing( playerIndex ) < 1 )
			{
				if ( g_PR->IsFakePlayer( playerIndex ) )
				{
					pKeyValues->SetString( "ping", "#TF_Scoreboard_Bot" );
				}
				else
				{
					pKeyValues->SetString( "ping", "" );
				}
			}
			else
			{
				pKeyValues->SetInt( "ping", g_PR->GetPing( playerIndex ) );
			}


			UpdatePlayerAvatar( playerIndex, pKeyValues );
			
			int itemID = pPlayerList->AddItem( 0, pKeyValues );
			Color clr = g_PR->GetTeamColor( g_PR->GetTeam( playerIndex ) );
			pPlayerList->SetItemFgColor( itemID, clr );

			if ( iSelectedPlayerIndex == playerIndex )
			{
				bMadeSelection = true;
				pPlayerList->SetSelectedItem( itemID );
			}

			pKeyValues->deleteThis();
		}
	}

	// If we're on spectator, find a default selection
	if ( !bMadeSelection )
	{
		if ( m_pPlayerListBlue->GetItemCount() > 0 )
		{
			m_pPlayerListBlue->SetSelectedItem( 0 );
		}
		else if ( m_pPlayerListRed->GetItemCount() > 0 )
		{
			m_pPlayerListRed->SetSelectedItem( 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the spectator list
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::UpdateSpectatorList()
{
	char szSpectatorList[512] = "" ;
	int nSpectators = 0;
	for( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; playerIndex++ )
	{
		if ( ShouldShowAsSpectator( playerIndex ) )
		{
			if ( nSpectators > 0 )
			{
				Q_strncat( szSpectatorList, ", ", ARRAYSIZE( szSpectatorList ) );
			}

			Q_strncat( szSpectatorList, g_PR->GetPlayerName( playerIndex ), ARRAYSIZE( szSpectatorList ) );
			nSpectators++;
		}
	}

	wchar_t wzSpectators[512] = L"";
	if ( nSpectators > 0 )
	{
		const char *pchFormat = ( 1 == nSpectators ? "#ScoreBoard_Spectator" : "#ScoreBoard_Spectators" );

		wchar_t wzSpectatorCount[16];
		wchar_t wzSpectatorList[1024];
		_snwprintf( wzSpectatorCount, ARRAYSIZE( wzSpectatorCount ), L"%i", nSpectators );
		g_pVGuiLocalize->ConvertANSIToUnicode( szSpectatorList, wzSpectatorList, sizeof( wzSpectatorList ) );
		g_pVGuiLocalize->ConstructString( wzSpectators, sizeof(wzSpectators), g_pVGuiLocalize->Find( pchFormat), 2, wzSpectatorCount, wzSpectatorList );
	}
	SetDialogVariable( "spectators", wzSpectators );
}

//-----------------------------------------------------------------------------
// Purpose: Updates details about a player
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::UpdatePlayerDetails()
{
	ClearPlayerDetails();

	C_TF_PlayerResource *tf_PR = dynamic_cast<C_TF_PlayerResource *>( g_PR );
	if ( !tf_PR )
		return;
	
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return;
	int playerIndex = pLocalPlayer->entindex();

	// Make sure the selected player is still connected. 
	if ( !tf_PR->IsConnected( playerIndex ) ) 
		return;

	if ( engine->IsHLTV() )
	{
		SetDialogVariable( "playername", tf_PR->GetPlayerName( playerIndex ) );
		return;
	}

	RoundStats_t &roundStats = GetStatPanel()->GetRoundStatsCurrentGame();

	SetDialogVariable( "kills", tf_PR->GetPlayerScore( playerIndex ) );
	SetDialogVariable( "deaths", tf_PR->GetDeaths( playerIndex ) );
	SetDialogVariable( "assists", roundStats.m_iStat[TFSTAT_KILLASSISTS] );
	SetDialogVariable( "destruction", roundStats.m_iStat[TFSTAT_BUILDINGSDESTROYED] );
	SetDialogVariable( "captures", roundStats.m_iStat[TFSTAT_CAPTURES] );
	SetDialogVariable( "defenses", roundStats.m_iStat[TFSTAT_DEFENSES] );
	SetDialogVariable( "dominations", roundStats.m_iStat[TFSTAT_DOMINATIONS] );
	SetDialogVariable( "revenge", roundStats.m_iStat[TFSTAT_REVENGE] );
	SetDialogVariable( "healing", roundStats.m_iStat[TFSTAT_HEALING] );
	SetDialogVariable( "invulns", roundStats.m_iStat[TFSTAT_INVULNS] );
	SetDialogVariable( "teleports", roundStats.m_iStat[TFSTAT_TELEPORTS] );
	SetDialogVariable( "headshots", roundStats.m_iStat[TFSTAT_HEADSHOTS] );
	SetDialogVariable( "backstabs", roundStats.m_iStat[TFSTAT_BACKSTABS] );
	SetDialogVariable( "playername", tf_PR->GetPlayerName( playerIndex ) );
	SetDialogVariable( "playerscore", GetPointsString( tf_PR->GetTotalScore( playerIndex ) ) );
	Color clr = g_PR->GetTeamColor( g_PR->GetTeam( playerIndex ) );
	m_pLabelPlayerName->SetFgColor( clr );
	m_pImagePanelHorizLine->SetFillColor( clr );

	int iClass = pLocalPlayer->m_Shared.GetDesiredPlayerClassIndex();
	int iTeam = pLocalPlayer->GetTeamNumber();
	if ( ( iTeam >= FIRST_GAME_TEAM ) && ( iClass >= TF_FIRST_NORMAL_CLASS ) && ( iClass <= TF_LAST_NORMAL_CLASS ) )
	{
		m_pClassImage->SetClass( iTeam, iClass, 0 );
		m_pClassImage->SetVisible( true );
	} 
	else
	{
		m_pClassImage->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clears score details
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::ClearPlayerDetails()
{
	m_pClassImage->SetVisible( false );

	// HLTV has no game stats
	bool bVisible = !engine->IsHLTV();

	SetDialogVariable( "kills", "" ); 
	SetControlVisible( "KillsLabel", bVisible );

	SetDialogVariable( "deaths", "" );
	SetControlVisible( "DeathsLabel", bVisible );

	SetDialogVariable( "captures", "" );
	SetControlVisible( "CapturesLabel", bVisible );

	SetDialogVariable( "defenses", "" );
	SetControlVisible( "DefensesLabel", bVisible );

	SetDialogVariable( "dominations", "" );
	SetControlVisible( "DominationLabel", bVisible );

	SetDialogVariable( "revenge", "" );
	SetControlVisible( "RevengeLabel", bVisible );

	SetDialogVariable( "assists", "" );
	SetControlVisible( "AssistsLabel", bVisible );

	SetDialogVariable( "destruction", "" );
	SetControlVisible( "DestructionLabel", bVisible );

	SetDialogVariable( "healing", "" );
	SetControlVisible( "HealingLabel", bVisible );

	SetDialogVariable( "invulns", "" );
	SetControlVisible( "InvulnLabel", bVisible );

	SetDialogVariable( "teleports", "" );
	SetControlVisible( "TeleportsLabel", bVisible );

	SetDialogVariable( "headshots", "" );
	SetControlVisible( "HeadshotsLabel", bVisible );

	SetDialogVariable( "backstabs", "" );
	SetControlVisible( "BackstabsLabel", bVisible );

	SetDialogVariable( "playername", "" );

	SetDialogVariable( "playerscore", "" );

	
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting players
//-----------------------------------------------------------------------------
bool CTFClientScoreBoardDialog::TFPlayerSortFunc( vgui::SectionedListPanel *list, int itemID1, int itemID2 )
{
	KeyValues *it1 = list->GetItemData(itemID1);
	KeyValues *it2 = list->GetItemData(itemID2);
	Assert(it1 && it2);

	// first compare score
	int v1 = it1->GetInt("score");
	int v2 = it2->GetInt("score");
	if (v1 > v2)
		return true;
	else if (v1 < v2)
		return false;

	// if score is the same, use player index to get deterministic sort
	int iPlayerIndex1 = it1->GetInt( "playerIndex" );
	int iPlayerIndex2 = it2->GetInt( "playerIndex" );
	return ( iPlayerIndex1 > iPlayerIndex2 );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a localized string of form "1 point", "2 points", etc for specified # of points
//-----------------------------------------------------------------------------
const wchar_t *GetPointsString( int iPoints )
{
	wchar_t wzScoreVal[128];
	static wchar_t wzScore[128];
	_snwprintf( wzScoreVal, ARRAYSIZE( wzScoreVal ), L"%i", iPoints );
	if ( 1 == iPoints ) 
	{
		g_pVGuiLocalize->ConstructString( wzScore, sizeof(wzScore), g_pVGuiLocalize->Find( "#TF_ScoreBoard_Point" ), 1, wzScoreVal );
	}
	else
	{
		g_pVGuiLocalize->ConstructString( wzScore, sizeof(wzScore), g_pVGuiLocalize->Find( "#TF_ScoreBoard_Points" ), 1, wzScoreVal );
	}
	return wzScore;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the specified player index is a spectator
//-----------------------------------------------------------------------------
bool CTFClientScoreBoardDialog::ShouldShowAsSpectator( int iPlayerIndex )
{
	C_TF_PlayerResource *tf_PR = dynamic_cast<C_TF_PlayerResource *>( g_PR );
	if ( !tf_PR )
		return false;

	// see if player is connected
	if ( tf_PR->IsConnected( iPlayerIndex ) ) 
	{
		// either spectating or unassigned team should show in spectator list
		int iTeam = tf_PR->GetTeam( iPlayerIndex );
		if ( TEAM_SPECTATOR == iTeam || TEAM_UNASSIGNED == iTeam )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Event handler
//-----------------------------------------------------------------------------
void CTFClientScoreBoardDialog::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	if ( 0 == Q_strcmp( type, "server_spawn" ) )
	{		
		// set server name in scoreboard
		const char *hostname = event->GetString( "hostname" );
		wchar_t wzHostName[256];
		wchar_t wzServerLabel[256];
		g_pVGuiLocalize->ConvertANSIToUnicode( hostname, wzHostName, sizeof( wzHostName ) );
		g_pVGuiLocalize->ConstructString( wzServerLabel, sizeof(wzServerLabel), g_pVGuiLocalize->Find( "#Scoreboard_Server" ), 1, wzHostName );
		SetDialogVariable( "server", wzServerLabel );
	}

	if( IsVisible() )
	{
		Update();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
SectionedListPanel *CTFClientScoreBoardDialog::GetSelectedPlayerList( void )
{
	SectionedListPanel *pList = NULL;

	// navigation
	if ( m_pPlayerListBlue->GetSelectedItem() >= 0 )
	{
		pList = m_pPlayerListBlue;
	}
	else if ( m_pPlayerListRed->GetSelectedItem() >= 0 )
	{
		pList = m_pPlayerListRed;
	}

	return pList;
}

#if defined ( _X360 )

//-----------------------------------------------------------------------------
// Purpose: Event handler
//-----------------------------------------------------------------------------
int	CTFClientScoreBoardDialog::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( !IsVisible() )
		return 1;

	if ( !down )
	{
		return 1;
	}

	SectionedListPanel *pList = GetSelectedPlayerList();

	switch( keynum )
	{
	case KEY_XBUTTON_UP:
		{
			if ( pList )
			{
				pList->MoveSelectionUp();
			}			
		}
		return 0;

	case KEY_XBUTTON_DOWN:
		{
			if ( pList )
			{
				pList->MoveSelectionDown();
			}		
		}
		return 0;

	case KEY_XBUTTON_RIGHT:
		{
			if ( m_pPlayerListRed->GetItemCount() == 0 )
				return 0;

			// move to the red list

			// get the row we're in now
			int iSelectedBlueItem = m_pPlayerListBlue->GetSelectedItem();

			m_pPlayerListBlue->ClearSelection();

			if ( iSelectedBlueItem >= 0 )
			{
				int row = m_pPlayerListBlue->GetRowFromItemID( iSelectedBlueItem );

				if ( row >= 0 )
				{
					int iNewItem = m_pPlayerListRed->GetItemIDFromRow( row );

					if ( iNewItem >= 0 )
					{
						m_pPlayerListRed->SetSelectedItem( iNewItem );
					}
					else
					{
						// we have fewer items. Select the last one
						int iLastRow = m_pPlayerListRed->GetItemCount()-1;

						iNewItem = m_pPlayerListRed->GetItemIDFromRow( iLastRow );

						if ( iNewItem >= 0 )
						{
							m_pPlayerListRed->SetSelectedItem( iNewItem );
						}
					}
				}
			}
		}
		return 0;

	case KEY_XBUTTON_LEFT:
		{
			if ( m_pPlayerListBlue->GetItemCount() == 0 )
				return 0;

			// move to the blue list

			// get the row we're in now
			int iSelectedRedItem = m_pPlayerListRed->GetSelectedItem();

			if ( iSelectedRedItem < 0 )
				iSelectedRedItem = 0;

			m_pPlayerListRed->ClearSelection();

			if ( iSelectedRedItem >= 0 )
			{
				int row = m_pPlayerListRed->GetRowFromItemID( iSelectedRedItem );

				if ( row >= 0 )
				{
					int iNewItem = m_pPlayerListBlue->GetItemIDFromRow( row );

					if ( iNewItem >= 0 )
					{
						m_pPlayerListBlue->SetSelectedItem( iNewItem );
					}
					else
					{
						// we have fewer items. Select the last one
						int iLastRow = m_pPlayerListBlue->GetItemCount()-1;

						iNewItem = m_pPlayerListBlue->GetItemIDFromRow( iLastRow );

						if ( iNewItem >= 0 )
						{
							m_pPlayerListBlue->SetSelectedItem( iNewItem );
						}
					}
				}
			}
		}
		return 0;

	case KEY_XBUTTON_B:
		{
			ShowPanel( false );
		}
		return 0;

	case KEY_XBUTTON_A:	// Show GamerCard for the selected player
		{
			if ( pList )
			{
				int iSelectedItem = pList->GetSelectedItem();

				if ( iSelectedItem >= 0 )
				{
					KeyValues *pInfo = pList->GetItemData( iSelectedItem );

					DevMsg( 1, "XShowGamerCardUI for player '%s'\n", pInfo->GetString( "name" ) );

					uint64 xuid = matchmaking->PlayerIdToXuid( pInfo->GetInt( "playerIndex" ) );
					XShowGamerCardUI( XBX_GetPrimaryUserId(), xuid );
				}
			}
		}
		return 0;

	case KEY_XBUTTON_X:	// Show player review for the selected player
		{
			if ( pList )
			{
				int iSelectedItem = pList->GetSelectedItem();

				if ( iSelectedItem >= 0 )
				{
					KeyValues *pInfo = pList->GetItemData( iSelectedItem );

					DevMsg( 1, "XShowPlayerReviewUI for player '%s'\n", pInfo->GetString( "name" ) );

					uint64 xuid = matchmaking->PlayerIdToXuid( pInfo->GetInt( "playerIndex" ) );
					XShowPlayerReviewUI( XBX_GetPrimaryUserId(), xuid );
				}
			}
		}
		return 0;
	
	default:
		break;
	}

	return 1;
}

#endif //_X360
