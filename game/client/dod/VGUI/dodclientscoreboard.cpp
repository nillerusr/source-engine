//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "dodclientscoreboard.h"
#include "c_team.h"
#include "c_dod_team.h"
#include "c_dod_playerresource.h"
#include "c_dod_player.h"
#include "dod_gamerules.h"
#include "backgroundpanel.h"

#include <KeyValues.h>

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/ImageList.h>

#include "voice_status.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODClientScoreBoardDialog::CDODClientScoreBoardDialog( IViewPort *pViewPort ):CClientScoreBoardDialog( pViewPort )
{
	m_pPlayerListAllies = new SectionedListPanel( this, "PlayerListAllies" );
	m_pPlayerListAxis = new SectionedListPanel( this, "PlayerListAxis" );

	m_iImageDead = 0;
	m_iImageDominated = 0;
	m_iImageNemesis = 0;

	m_pAllies_PlayerCount = new Label( this, "Allies_PlayerCount", "" );
	m_pAllies_Score = new Label( this, "Allies_Score", "" );
	m_pAllies_Kills = new Label( this, "Allies_Kills", "" );
	m_pAllies_Deaths = new Label( this, "Allies_Deaths", "" );
	m_pAllies_Ping = new Label( this, "Allies_Ping", "" );

	m_pAxis_PlayerCount = new Label( this, "Axis_PlayerCount", "" );
	m_pAxis_Score = new Label( this, "Axis_Score", "" );
	m_pAxis_Kills = new Label( this, "Axis_Kills", "" );
	m_pAxis_Deaths = new Label( this, "Axis_Deaths", "" );
	m_pAxis_Ping = new Label( this, "Axis_Ping", "" );

	ListenForGameEvent( "server_spawn" );
	SetDialogVariable( "server", "" );
	SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDODClientScoreBoardDialog::~CDODClientScoreBoardDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Paint background for rounded corners
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::PaintBackground()
{
	int wide, tall;
	GetSize( wide, tall );

	DrawRoundedBackground( m_bgColor, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Paint border for rounded corners
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::PaintBorder()
{
	int wide, tall;
	GetSize( wide, tall );

	DrawRoundedBorder( m_borderColor, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/scoreboard.res" );

	m_bgColor = GetSchemeColor( "SectionedListPanel.BgColor", GetBgColor(), pScheme );
	m_borderColor = pScheme->GetColor( "Yellow", Color( 251, 206, 60, 255 ) );

	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( pScheme->GetBorder( "BaseBorder" ) );

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

	if ( m_pPlayerListAllies )
	{
		m_pPlayerListAllies->SetImageList( m_pImageList, false );
		m_pPlayerListAllies->SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pPlayerListAllies->SetBorder( NULL );
		m_pPlayerListAllies->SetVisible( true );
	}

	if ( m_pPlayerListAxis )
	{
		m_pPlayerListAxis->SetImageList( m_pImageList, false );
		m_pPlayerListAxis->SetBgColor( Color( 0, 0, 0, 0 ) );
		m_pPlayerListAxis->SetBorder( NULL );
		m_pPlayerListAxis->SetVisible( true );
	}

	// turn off the default player list since we have our own
	if ( m_pPlayerList )
	{
		m_pPlayerList->SetVisible( false );
	}

	if ( m_pAllies_PlayerCount && m_pAllies_Score && m_pAllies_Kills && m_pAllies_Deaths && m_pAllies_Ping )
	{
		m_pAllies_PlayerCount->SetFgColor( COLOR_DOD_GREEN );
		m_pAllies_Score->SetFgColor( COLOR_DOD_GREEN );
		m_pAllies_Kills->SetFgColor( COLOR_DOD_GREEN );
		m_pAllies_Deaths->SetFgColor( COLOR_DOD_GREEN );
		m_pAllies_Ping->SetFgColor( COLOR_DOD_GREEN );
	}

	if ( m_pAxis_PlayerCount && m_pAxis_Score && m_pAxis_Kills && m_pAxis_Deaths && m_pAxis_Ping )
	{
		m_pAxis_PlayerCount->SetFgColor( COLOR_DOD_RED );
		m_pAxis_Score->SetFgColor( COLOR_DOD_RED );
		m_pAxis_Kills->SetFgColor( COLOR_DOD_RED );
		m_pAxis_Deaths->SetFgColor( COLOR_DOD_RED );
		m_pAxis_Ping->SetFgColor( COLOR_DOD_RED );
	}

	SetVisible( false );
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Resets the scoreboard panel
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::Reset()
{
	InitPlayerList( m_pPlayerListAllies, TEAM_ALLIES );
	InitPlayerList( m_pPlayerListAxis, TEAM_AXIS );
}

//-----------------------------------------------------------------------------
// Purpose: Sorts players
//-----------------------------------------------------------------------------
bool CDODClientScoreBoardDialog::DODPlayerSortFunc( vgui::SectionedListPanel *list, int itemID1, int itemID2 )
{
	KeyValues *it1 = list->GetItemData( itemID1 );
	KeyValues *it2 = list->GetItemData( itemID2 );
	Assert( it1 && it2 );

	// first compare score
	int v1 = it1->GetInt( "score" );
	int v2 = it2->GetInt( "score" );
	if ( v1 > v2 )
		return true;
	else if ( v1 < v2 )
		return false;

	// then compare frags
	v1 = it1->GetInt( "frags" );
	v2 = it2->GetInt( "frags" );
	if ( v1 > v2 )
		return true;
	else if ( v1 < v2 )
		return false;

	// next compare deaths
	v1 = it1->GetInt( "deaths" );
	v2 = it2->GetInt( "deaths" );
	if ( v1 > v2 )
		return false;
	else if ( v1 < v2 )
		return true;

	// if score and deaths are the same, use player index to get deterministic sort
	int iPlayerIndex1 = it1->GetInt( "playerIndex" );
	int iPlayerIndex2 = it2->GetInt( "playerIndex" );
	return ( iPlayerIndex1 > iPlayerIndex2 );
}

//-----------------------------------------------------------------------------
// Purpose: Inits the player list in a list panel
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::InitPlayerList( SectionedListPanel *pPlayerList, int teamNumber )
{
	pPlayerList->SetVerticalScrollbar( false );
	pPlayerList->RemoveAll();
	pPlayerList->RemoveAllSections();
	pPlayerList->AddSection( 0, "Players", DODPlayerSortFunc );
	pPlayerList->SetSectionAlwaysVisible( 0, true );
	pPlayerList->SetSectionFgColor( 0, Color( 255, 255, 255, 255 ) );
	pPlayerList->SetBgColor( Color( 0, 0, 0, 0 ) );
	pPlayerList->SetBorder( NULL );

	// set the section to have the team color
	if ( teamNumber && GameResources() )
	{
		pPlayerList->SetSectionFgColor( 0, GameResources()->GetTeamColor( teamNumber ) );
	}

	// Avatars are always displayed at 32x32 regardless of resolution
	pPlayerList->AddColumnToSection( 0, "avatar",	"", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, m_iProportionalAvatarWidth );
	pPlayerList->AddColumnToSection( 0, "name",		"", 0, m_iNameWidth );
	pPlayerList->AddColumnToSection( 0, "status",	"", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, m_iStatusWidth );
	pPlayerList->AddColumnToSection( 0, "class",	"", 0, m_iClassWidth );
	pPlayerList->AddColumnToSection( 0, "score",	"", SectionedListPanel::COLUMN_RIGHT, m_iScoreWidth );
	pPlayerList->AddColumnToSection( 0, "frags",	"", SectionedListPanel::COLUMN_RIGHT, m_iFragsWidth );
	pPlayerList->AddColumnToSection( 0, "deaths",	"", SectionedListPanel::COLUMN_RIGHT, m_iDeathWidth );
	pPlayerList->AddColumnToSection( 0, "ping",		"", SectionedListPanel::COLUMN_RIGHT, m_iPingWidth );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the dialog
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::Update()
{
	UpdateTeamInfo();
	UpdatePlayerList();
	UpdateSpectatorList();
	MoveToCenterOfScreen();

	// update every second
	m_fNextUpdateTime = gpGlobals->curtime + 1.0f; 
}

//-----------------------------------------------------------------------------
// Purpose: Updates information about teams
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::UpdateTeamInfo()
{
	// update the team sections in the scoreboard
	for ( int teamIndex = TEAM_ALLIES; teamIndex <= TEAM_AXIS; teamIndex++ )
	{
		wchar_t *teamName = NULL;;
		C_DODTeam *team = dynamic_cast<C_DODTeam *>( GetGlobalTeam(teamIndex) );
		if ( team )
		{
			// choose dialog variables to set depending on team
			const char *pDialogVarTeamScore = NULL;
			const char *pDialogVarTeamPlayerCount = NULL;
			const char *pDialogVarTeamPing = NULL;
			const char *pDialogVarTeamDeaths = NULL;
			const char *pDialogVarTeamFrags = NULL;
			switch ( teamIndex ) 
			{
			case TEAM_ALLIES:
					teamName = g_pVGuiLocalize->Find( "#Teamname_Allies" );
					pDialogVarTeamScore = "allies_teamscore";
					pDialogVarTeamPlayerCount = "allies_teamplayercount";
					pDialogVarTeamPing = "allies_teamping";
					pDialogVarTeamDeaths = "allies_teamdeaths";
					pDialogVarTeamFrags = "allies_teamfrags";
					break;
			case TEAM_AXIS:
					teamName = g_pVGuiLocalize->Find( "#Teamname_Axis" );
					pDialogVarTeamScore = "axis_teamscore";
					pDialogVarTeamPlayerCount = "axis_teamplayercount";
					pDialogVarTeamPing = "axis_teamping";
					pDialogVarTeamDeaths = "axis_teamdeaths";
					pDialogVarTeamFrags = "axis_teamfrags";
					break;
			default:
					Assert( false );
					break;
			}							

			// update team name
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
				g_pVGuiLocalize->ConstructString( string1, sizeof(string1), g_pVGuiLocalize->Find( "#scoreboard_Player" ), 2, teamName, wNumPlayers );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( string1, sizeof(string1), g_pVGuiLocalize->Find( "#scoreboard_Players" ), 2, teamName, wNumPlayers );
			}

			// set # of players for team in dialog
			SetDialogVariable( pDialogVarTeamPlayerCount, string1 );

			// Rounds won ( + tick )
			wchar_t wTeamScore[128];
			wchar_t wRoundsWon[8];
			wchar_t wTickScore[8];
			_snwprintf( wRoundsWon, ARRAYSIZE( wRoundsWon ), L"%i", team->GetRoundsWon() );
			_snwprintf( wTickScore, ARRAYSIZE( wTickScore ), L"%i", team->Get_Score() );
			g_pVGuiLocalize->ConstructString( wTeamScore, sizeof(wTeamScore), g_pVGuiLocalize->Find( "#scoreboard_teamscore" ), 2, wRoundsWon, wTickScore );

			// set team score in dialog
			SetDialogVariable( pDialogVarTeamScore, wTeamScore );		

			int kills = 0;
			int deaths = 0;
			int pingsum = 0;
			int numcounted = 0;
			int ping;
			
			for( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; playerIndex++ )
			{
				if( g_PR->IsConnected( playerIndex ) && g_PR->GetTeam( playerIndex ) == teamIndex )
				{
					ping = g_PR->GetPing( playerIndex );
					kills += g_PR->GetPlayerScore( playerIndex );
					deaths += g_PR->GetDeaths( playerIndex );		

					if ( ping >= 1 )
					{
						pingsum += ping;
						numcounted++;	
					}
				}
			}
			
			if ( numcounted > 0 )
			{
				int ping = (int)( (float)pingsum / (float)numcounted );
				SetDialogVariable( pDialogVarTeamPing, ping );		
			}
			else
			{
				SetDialogVariable( pDialogVarTeamPing, "" );	
			}

			SetDialogVariable( pDialogVarTeamFrags, kills );	
			SetDialogVariable( pDialogVarTeamDeaths, deaths );	
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the player list
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::UpdatePlayerList()
{
	m_pPlayerListAllies->RemoveAll();
	m_pPlayerListAxis->RemoveAll();

	C_DOD_PlayerResource *dod_PR = dynamic_cast<C_DOD_PlayerResource *>( g_PR );
	if ( !dod_PR )
		return;

	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pLocalPlayer )
		return;

	int iLocalPlayerIndex = GetLocalPlayerIndex();

	for( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; playerIndex++ )
	{
		if( g_PR->IsConnected( playerIndex ) )
		{
			SectionedListPanel *pPlayerList = NULL;
			switch ( g_PR->GetTeam( playerIndex ) )
			{
			case TEAM_ALLIES:
				pPlayerList = m_pPlayerListAllies;
				break;
			case TEAM_AXIS:
				pPlayerList = m_pPlayerListAxis;
				break;
			}

			if ( pPlayerList == NULL )
			{
				continue;			
			}

			KeyValues *pKeyValues = new KeyValues( "data" );
			GetPlayerScoreInfo( playerIndex, pKeyValues );

			if ( pLocalPlayer->m_Shared.IsPlayerDominatingMe( playerIndex ) )
			{
				// if local player is dominated by this player, show a nemesis icon
				pKeyValues->SetString( "class", "#Scoreboard_Nemesis" );
				pKeyValues->SetInt( "status", m_iImageNemesis );
			}
			else if ( pLocalPlayer->m_Shared.IsPlayerDominated( playerIndex) )
			{
				// if this player is dominated by the local player, show the domination icon
				pKeyValues->SetString( "class", "#Scoreboard_Dominated" );
				pKeyValues->SetInt( "status", m_iImageDominated );
			}

			int itemID = pPlayerList->AddItem( 0, pKeyValues );
			Color clr = g_PR->GetTeamColor( g_PR->GetTeam( playerIndex ) );
			pPlayerList->SetItemFgColor( itemID, clr );

			if ( playerIndex == iLocalPlayerIndex )
			{
				pPlayerList->SetSelectedItem( itemID );
			}

			pKeyValues->deleteThis();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the spectator list
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::UpdateSpectatorList()
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
// Purpose: Returns whether the specified player index is a spectator
//-----------------------------------------------------------------------------
bool CDODClientScoreBoardDialog::ShouldShowAsSpectator( int iPlayerIndex )
{
	C_DOD_PlayerResource *dod_PR = dynamic_cast<C_DOD_PlayerResource *>( g_PR );
	if ( !dod_PR )
		return false;

	// see if player is connected
	if ( dod_PR->IsConnected( iPlayerIndex ) ) 
	{
		// either spectator or unassigned team should show in spectator list
		int iTeam = dod_PR->GetTeam( iPlayerIndex );
		if ( TEAM_SPECTATOR == iTeam || TEAM_UNASSIGNED == iTeam )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Event handler
//-----------------------------------------------------------------------------
void CDODClientScoreBoardDialog::FireGameEvent( IGameEvent *event )
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
// Purpose: Adds a new row to the scoreboard, from the playerinfo structure
//-----------------------------------------------------------------------------
bool CDODClientScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, KeyValues *kv )
{
	C_DOD_PlayerResource *dod_PR = dynamic_cast<C_DOD_PlayerResource *>( g_PR );
	if ( !dod_PR )
		return true;

	// Clean up the player name
	const char *oldName = g_PR->GetPlayerName( playerIndex );
	int bufsize = strlen( oldName ) * 2 + 1;
	char *newName = (char *)_alloca( bufsize );
	UTIL_MakeSafeName( oldName, newName, bufsize );
	kv->SetString( "name", newName );

	kv->SetInt( "playerIndex", playerIndex );
	kv->SetInt( "score", dod_PR->GetScore( playerIndex ) );
	kv->SetInt( "frags", g_PR->GetPlayerScore( playerIndex ) );
	kv->SetInt( "deaths", g_PR->GetDeaths( playerIndex ) );
	kv->SetString( "class", "" );

	UpdatePlayerAvatar( playerIndex, kv );
	
	if ( g_PR->GetPing( playerIndex ) < 1 )
	{
		if ( g_PR->IsFakePlayer( playerIndex ) )
		{
			kv->SetString( "ping", "BOT" );
		}
		else
		{
			kv->SetString( "ping", "" );
		}
	}
	else
	{
		kv->SetInt( "ping", g_PR->GetPing( playerIndex ) );
	}

	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pLocalPlayer )
		return true;

	int team = g_PR->GetTeam( playerIndex );
	int localteam = pLocalPlayer->GetTeamNumber();

	// If we are on a team that shows class/status, and the local player is allowed to see this information
	if( ( localteam == TEAM_SPECTATOR || localteam == TEAM_UNASSIGNED || team == localteam ) )
	{
		// class name
		if( g_PR->IsConnected( playerIndex ) )
		{
			C_DODTeam *pTeam = dynamic_cast<C_DODTeam *>( GetGlobalTeam( team ) );

			Assert( pTeam );

			int cls = dod_PR->GetPlayerClass( playerIndex );

			char szClassName[64];
			szClassName[0] = '\0';

			if( cls != PLAYERCLASS_UNDEFINED )
			{
				const CDODPlayerClassInfo &info = pTeam->GetPlayerClassInfo( cls );

				g_pVGuiLocalize->ConvertUnicodeToANSI( g_pVGuiLocalize->Find( info.m_szPrintName ), szClassName, sizeof(szClassName) );
			}

			kv->SetString( "class", szClassName );
		}
		else
		{
			Assert(0);
		}

		// status
		// display whether player is alive or dead (all players see this for all other players on both teams)
		kv->SetInt( "status", g_PR->IsAlive( playerIndex ) ?  0 : m_iImageDead );
	}

	if ( g_PR->IsHLTV( playerIndex ) )
	{
		// show #spectators in class field, it's transmitted as player's score
		char numspecs[32];
		Q_snprintf( numspecs, sizeof( numspecs ), "%i Spectators", m_HLTVSpectators );
		kv->SetString( "class", numspecs );
	}

	return true;
}

void CDODClientScoreBoardDialog::ShowPanel( bool bShow )
{
	BaseClass::ShowPanel( bShow );

	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "global" );

	if ( bShow )
	{
		gHUD.LockRenderGroup( iRenderGroup );
	}
	else
	{
		gHUD.UnlockRenderGroup( iRenderGroup );
	}
}