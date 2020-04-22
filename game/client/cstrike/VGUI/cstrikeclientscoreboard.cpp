//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "cstrikeclientscoreboard.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "backgroundpanel.h"
#include "clientmode.h"

#include <KeyValues.h>

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/ScalableImagePanel.h>
#include "VGuiMatSurface/IMatSystemSurface.h"

#include "voice_status.h"
#include "vgui_avatarimage.h"

#include "achievementmgr.h"
#include "engine/imatchmaking.h"

using namespace vgui;

const int kInvalidImageID = -1;
const int kMaxMVPCount = 9999;
const float kScaleMVP = 0.75f;					// scale of the MVP star relative to the player name height
const float kUpdateInterval = 1.0f;				// how often the scoreboard refreshes
const float kTeamScoreMargin = 0.15f;			// margin as a ratio of avatar height
const float kTeamScoreLineLeadingRatio = 0.25f;	// padding as a ratio of avatar height

// CT player data colors
ConVar cl_scoreboard_ct_color_red( "cl_scoreboard_ct_color_red", "150", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard CT player data red channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_ct_color_green( "cl_scoreboard_ct_color_green", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard CT player data green channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_ct_color_blue( "cl_scoreboard_ct_color_blue", "255", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard CT player data blue channel", true, 0.0f, true, 255.0f );

// T player data colors
ConVar cl_scoreboard_t_color_red( "cl_scoreboard_t_color_red", "240", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard T player data red channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_t_color_green( "cl_scoreboard_t_color_green", "90", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard T player data green channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_t_color_blue( "cl_scoreboard_t_color_blue", "90", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard T player data blue channel", true, 0.0f, true, 255.0f );

// Dead player data colors
ConVar cl_scoreboard_dead_color_red( "cl_scoreboard_dead_color_red", "125", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard dead player data red channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_dead_color_green( "cl_scoreboard_dead_color_green", "125", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard dead player data green channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_dead_color_blue( "cl_scoreboard_dead_color_blue", "125", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard dead player data blue channel", true, 0.0f, true, 255.0f );

// Clan colors
ConVar cl_scoreboard_clan_ct_color_red( "cl_scoreboard_clan_ct_color_red", "150", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard CT player clan tag red channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_clan_ct_color_green( "cl_scoreboard_clan_ct_color_green", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard CT player clan tag green channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_clan_ct_color_blue( "cl_scoreboard_clan_ct_color_blue", "255", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard CT player clan tag blue channel", true, 0.0f, true, 255.0f );

ConVar cl_scoreboard_clan_t_color_red( "cl_scoreboard_clan_t_color_red", "240", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard T player clan tag red channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_clan_t_color_green( "cl_scoreboard_clan_t_color_green", "90", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard T player clan tag green channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_clan_t_color_blue( "cl_scoreboard_clan_t_color_blue", "90", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard T player clan tag blue channel", true, 0.0f, true, 255.0f );

ConVar cl_scoreboard_dead_clan_color_red( "cl_scoreboard_dead_clan_color_red", "125", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard dead player clan tag red channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_dead_clan_color_green( "cl_scoreboard_dead_clan_color_green", "125", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard dead player clan tag green channel", true, 0.0f, true, 255.0f );
ConVar cl_scoreboard_dead_clan_color_blue( "cl_scoreboard_dead_clan_color_blue", "125", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scoreboard dead player clan tag blue channel", true, 0.0f, true, 255.0f );


// [tj] These ConVars are defined at various places in the global scope. Just declaring them here so we can use them
extern ConVar mp_winlimit;
extern ConVar mp_maxrounds;
extern ConVar mp_timelimit;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSClientScoreBoardDialog::CCSClientScoreBoardDialog( IViewPort *pViewPort ) : CClientScoreBoardDialog( pViewPort ),
	m_DeadPlayerDataColor( 125, 125, 125, 125 ),
	m_PlayerDataBgColor( 0, 0, 0, 0 ),
	m_DeadPlayerClanColor( 125, 125, 125, 125 )
{
	m_teamDisplayT.playerDataColor = Color( 240, 90, 90, 255 );
	m_teamDisplayT.playerClanColor = Color( 240, 90, 90, 255 );

	m_teamDisplayCT.playerDataColor = Color( 150, 200, 255, 255 );
	m_teamDisplayCT.playerClanColor = Color( 150, 200, 255, 255 );

    m_iImageDead = kInvalidImageID;
    m_iImageDominated = kInvalidImageID;
    m_iImageNemesis = kInvalidImageID;
    m_iImageBomb = kInvalidImageID;
    m_iImageVIP = kInvalidImageID;
    m_iImageFriend = kInvalidImageID;
    m_iImageNemesisDead = kInvalidImageID;
    m_iImageDominationDead = kInvalidImageID;

	m_pWinConditionLabel = new Label( this, "WinConditionLabel", "" );
	m_pClockLabel = new Label( this, "Icon_Clock", "" );

    m_pLabelMapName = new Label( this, "MapName", "" );
	m_pServerLabel = new Label( this, "ServerNameLabel", "" );

    ListenForGameEvent( "server_spawn" );
    ListenForGameEvent( "game_newmap" );
    ListenForGameEvent( "match_end_conditions" );
    ListenForGameEvent( "cs_win_panel_match" );

    SetDialogVariable( "server", "" );
    SetVisible( false );
    SetProportional(true);
    SetPaintBorderEnabled(false);
    SetScheme( "ClientScheme" );

	// [pfreese] Make the scoreboard a popup so it renders over the chat interface (which is also a popup). Hacky.
	MakePopup();
	
    m_listItemFont = NULL;

    m_LocalPlayerItemID = -1;
    m_iImageMVP = -1;

    m_gameOver = false;

    if ( g_pClientMode &&
		 g_pClientMode->GetMapName() )
    {
		V_wcsncpy( m_pMapName, g_pClientMode->GetMapName(), sizeof( m_pMapName ) );
        SetDialogVariable( "mapname", m_pMapName );
        m_pLabelMapName->SetVisible( true );
    }

    SetKeyBoardInputEnabled( true );

    for ( int i = 0; i < cMaxScoreLines; ++i )
    {
		PlayerDisplay* pPlayerDisplay;

		pPlayerDisplay = &m_teamDisplayCT.playerDisplay[i];
		pPlayerDisplay->pClanLabel = NULL;
		pPlayerDisplay->pNameLabel = NULL;
		pPlayerDisplay->pScoreLabel = NULL;
		pPlayerDisplay->pDeathsLabel = NULL;
		pPlayerDisplay->pPingLabel = NULL;
		pPlayerDisplay->pMVPCountLabel = NULL;
		pPlayerDisplay->pAvatar = NULL;
		pPlayerDisplay->pStatusImage = NULL;
		pPlayerDisplay->pMVPImage = NULL;
		pPlayerDisplay->pSelect = NULL;

		pPlayerDisplay = &m_teamDisplayT.playerDisplay[i];
		pPlayerDisplay->pClanLabel = NULL;
		pPlayerDisplay->pNameLabel = NULL;
		pPlayerDisplay->pScoreLabel = NULL;
		pPlayerDisplay->pDeathsLabel = NULL;
		pPlayerDisplay->pPingLabel = NULL;
		pPlayerDisplay->pMVPCountLabel = NULL;
		pPlayerDisplay->pAvatar = NULL;
		pPlayerDisplay->pStatusImage = NULL;
		pPlayerDisplay->pMVPImage = NULL;
		pPlayerDisplay->pSelect = NULL;
	}

	m_pServerName[0] = L'\0';
    m_pStatsEnabled[0] = L'\0';
    m_pStatsDisabled[0] = L'\0';

	m_MVPXOffset = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSClientScoreBoardDialog::~CCSClientScoreBoardDialog()
{
    for (int i = 0; i < cMaxScoreLines; ++i)
    {
		PlayerDisplay* pPlayerDisplay;

		pPlayerDisplay = &m_teamDisplayCT.playerDisplay[i];
		delete pPlayerDisplay->pClanLabel;
        delete pPlayerDisplay->pNameLabel;
        delete pPlayerDisplay->pScoreLabel;
        delete pPlayerDisplay->pDeathsLabel;
        delete pPlayerDisplay->pPingLabel;
		delete pPlayerDisplay->pMVPCountLabel;
		delete pPlayerDisplay->pAvatar;
        delete pPlayerDisplay->pStatusImage;
		delete pPlayerDisplay->pMVPImage;
		delete pPlayerDisplay->pSelect;

		pPlayerDisplay = &m_teamDisplayT.playerDisplay[i];
		delete pPlayerDisplay->pClanLabel;
		delete pPlayerDisplay->pNameLabel;
		delete pPlayerDisplay->pScoreLabel;
		delete pPlayerDisplay->pDeathsLabel;
		delete pPlayerDisplay->pPingLabel;
		delete pPlayerDisplay->pMVPCountLabel;
		delete pPlayerDisplay->pAvatar;
		delete pPlayerDisplay->pStatusImage;
		delete pPlayerDisplay->pMVPImage;
		delete pPlayerDisplay->pSelect;
    }
}

const wchar_t *LocalizeFindSafe( const char *pTokenName )
{
	const wchar_t *pStr = g_pVGuiLocalize->Find( pTokenName );
	return pStr ? pStr : L"\0";
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

	//
	// [smessick] Note: ApplySchemeSettings is called multiple times for the scoreboard.
	// Therefore, we must make sure to delete previously allocated items.
	//

    LoadControlSettings( "Resource/UI/scoreboard.res" );

    //Just used for a background alpha value.  50% opacity
    m_listItemFont = pScheme->GetFont( "ScoreboardBody_1", IsProportional() );
    m_listItemFontSmaller = pScheme->GetFont( "ScoreboardBody_2", IsProportional() );
    m_listItemFontSmallest = pScheme->GetFont( "ScoreboardBody_3", IsProportional() );
	m_MVPFont = pScheme->GetFont( "ScoreboardMVP", IsProportional() );

    SetBgColor( Color( 0, 0, 0, 0 ) );
    SetBorder( pScheme->GetBorder( "BaseBorder" ) );

    // turn off the default player list since we have our own
    if ( m_pPlayerList )
    {
        m_pPlayerList->SetVisible( false );
    }

	// Set the player colors from the convars.
	UpdatePlayerColors();

	m_MVPXOffset = scheme()->GetProportionalScaledValueEx( GetScheme(), 2 );

	SetupTeamDisplay( m_teamDisplayCT, "CT" );
	SetupTeamDisplay( m_teamDisplayT, "T" );

	// Set the server name (in the case of a resolution change).
	if ( m_pServerName[0] == L'\0' &&
		 g_pClientMode->GetServerName() != NULL )
	{
		V_wcsncpy( m_pServerName, g_pClientMode->GetServerName(), sizeof( m_pServerName ) );
	}

	// Cache the stats enabled string.
	if ( m_pStatsEnabled[0] == L'\0' )
	{
		V_wcsncpy( m_pStatsEnabled, LocalizeFindSafe( "#Cstrike_Scoreboard_StatsEnabled" ), sizeof( m_pStatsEnabled ) );
	}

	// Cache the stats disabled string.
	if ( m_pStatsDisabled[0] == L'\0' )
	{
		V_wcsncpy( m_pStatsDisabled, LocalizeFindSafe( "#Cstrike_Scoreboard_StatsDisabled" ), sizeof( m_pStatsDisabled ) );
	}
 
    SetVisible( false );
}


void CCSClientScoreBoardDialog::SetupTeamDisplay( TeamDisplayInfo& teamDisplay, const char* szTeamPrefix )
{
	const int selectMargin = scheme()->GetProportionalScaledValueEx( GetScheme(), 1 );
	const int mvpLabelWidth = scheme()->GetProportionalScaledValueEx( GetScheme(), 20 );
	const int mvpLabelYOffset = scheme()->GetProportionalScaledValueEx( GetScheme(), 2 );

	char tmpName[32];

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerArea", szTeamPrefix );
	Panel *pPlayerArea = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerAvatar0", szTeamPrefix );
	Panel *pPlayerAvatar0 = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerClan0", szTeamPrefix );
	Panel *pPlayerClan0 = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerName0", szTeamPrefix );
	Panel *pPlayerName0 = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerStatus0", szTeamPrefix );
	Panel *pPlayerStatus0 = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerScore0", szTeamPrefix );
	Panel *pPlayerScore0 = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerDeaths0", szTeamPrefix );
	Panel *pPlayerDeaths0 = FindChildByName( tmpName );

	V_snprintf( tmpName, sizeof(tmpName), "%sPlayerLatency0", szTeamPrefix );
	Panel *pPlayerLatency0 = FindChildByName( tmpName );

	// Get the bounds of the player area.
	int playerX = 0;
	int playerY = 0;
	int playerWide = 0;
	int playerTall = 0;
	if ( pPlayerArea )
		pPlayerArea->GetBounds( playerX, playerY, playerWide, playerTall );

	// determine the line height needed
	int x, y, wide, tall;
	teamDisplay.scoreAreaLineHeight = 0;
	teamDisplay.scoreAreaMinX = INT_MAX;
	teamDisplay.scoreAreaMaxX = INT_MIN;

	if ( pPlayerAvatar0 != NULL )
	{
		pPlayerAvatar0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}
	if ( pPlayerClan0 != NULL )
	{
		pPlayerClan0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}
	if ( pPlayerName0 != NULL )
	{
		pPlayerName0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}
	if ( pPlayerScore0 != NULL )
	{
		pPlayerScore0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}
	if ( pPlayerDeaths0 != NULL )
	{
		pPlayerDeaths0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}
	if ( pPlayerLatency0 != NULL )
	{
		pPlayerLatency0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}
	if ( pPlayerStatus0 != NULL )
	{
		pPlayerStatus0->GetBounds( x, y, wide, tall );
		teamDisplay.scoreAreaLineHeight = MAX( tall, teamDisplay.scoreAreaLineHeight );
		teamDisplay.scoreAreaMinX = MIN( teamDisplay.scoreAreaMinX, x );
		teamDisplay.scoreAreaMaxX = MAX( teamDisplay.scoreAreaMaxX, x + wide );
	}

	int marginY = RoundFloatToInt(teamDisplay.scoreAreaLineHeight * kTeamScoreMargin);

	teamDisplay.scoreAreaInnerHeight = playerTall - 2 * marginY;
	teamDisplay.scoreAreaLinePreferredLeading = RoundFloatToInt(teamDisplay.scoreAreaLineHeight * kTeamScoreLineLeadingRatio);
	teamDisplay.scoreAreaStartY = playerY + marginY;
	teamDisplay.maxPlayersVisible = MIN(cMaxScoreLines, teamDisplay.scoreAreaInnerHeight / teamDisplay.scoreAreaLineHeight);

	// Calculate the starting point for player data.
	int startY = teamDisplay.scoreAreaStartY;
	int spacingY = teamDisplay.scoreAreaLineHeight + teamDisplay.scoreAreaLinePreferredLeading;

	for ( int i = 0; i < cMaxScoreLines; ++i, startY += spacingY )
	{
		PlayerDisplay& playerDisplay = teamDisplay.playerDisplay[i];

		int wide = 0;
		int tall = 0;
		int x = 0;
		int y = 0;

		// avatar
		if ( pPlayerAvatar0 != NULL )
		{
			pPlayerAvatar0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playeravatar%d", szTeamPrefix, i );
			delete playerDisplay.pAvatar;
			playerDisplay.pAvatar = (CAvatarImagePanel*)SETUP_PANEL( new CAvatarImagePanel( this, tmpName ) );
			playerDisplay.pAvatar->SetBounds( x, startY, wide, tall );
			playerDisplay.pAvatar->SetDefaultAvatar( scheme()->GetImage( &teamDisplay == &m_teamDisplayCT ? CSTRIKE_DEFAULT_CT_AVATAR : CSTRIKE_DEFAULT_T_AVATAR, true ) );
			playerDisplay.pAvatar->SetShouldDrawFriendIcon( false );
			playerDisplay.pAvatar->SetShouldScaleImage( true );
			playerDisplay.pAvatar->SetVisible( false );
		}

		// clan
		if ( pPlayerClan0 != NULL )
		{
			pPlayerClan0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playerclan%d", szTeamPrefix, i );
			delete playerDisplay.pClanLabel;
			playerDisplay.pClanLabel = (Label*)SETUP_PANEL( new Label( this, tmpName, tmpName ) );
			playerDisplay.pClanLabel->SetFont( m_listItemFont );
			playerDisplay.pClanLabel->SetBounds( x, startY, wide, tall );
			playerDisplay.pClanLabel->SetBgColor( m_PlayerDataBgColor );
			playerDisplay.pClanLabel->SetFgColor( m_teamDisplayCT.playerClanColor );
			playerDisplay.pClanLabel->SetContentAlignment( Label::a_east );
			playerDisplay.pClanLabel->SetVisible( false );
		}

		// name
		if ( pPlayerName0 != NULL )
		{
			pPlayerName0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playername%d", szTeamPrefix, i );
			delete playerDisplay.pNameLabel;
			playerDisplay.pNameLabel = (Label*)SETUP_PANEL( new Label( this, tmpName, tmpName ) );
			playerDisplay.pNameLabel->SetFont( m_listItemFont );
			playerDisplay.pNameLabel->SetBounds( x, startY, wide, tall);
			playerDisplay.pNameLabel->SetBgColor( m_PlayerDataBgColor );
			playerDisplay.pNameLabel->SetFgColor( m_teamDisplayCT.playerDataColor );
			playerDisplay.pNameLabel->SetContentAlignment( Label::a_west );
			playerDisplay.pNameLabel->SetVisible( false );

			// mvp
			int wideMVP = RoundFloatToInt(teamDisplay.scoreAreaLineHeight * kScaleMVP);
			int tallMVP = RoundFloatToInt(teamDisplay.scoreAreaLineHeight * kScaleMVP);
			int yMVP = ( teamDisplay.scoreAreaLineHeight - tallMVP ) / 2;

			V_snprintf( tmpName, 32, "_%s_mvp%d", szTeamPrefix, i );
			delete playerDisplay.pMVPImage;
			playerDisplay.pMVPImage = (ImagePanel*)SETUP_PANEL( new ImagePanel( this, tmpName ) );
			playerDisplay.pMVPImage->SetBounds( x, startY + yMVP, wideMVP, tallMVP );
			playerDisplay.pMVPImage->SetImage( "../hud/scoreboard_mvp" );
			playerDisplay.pMVPImage->SetShouldScaleImage( true );
			playerDisplay.pMVPImage->SetVisible( false );

			// mvp count
			V_snprintf( tmpName, 32, "_%s_mvpcount%d", szTeamPrefix, i );
			delete playerDisplay.pMVPCountLabel;
			playerDisplay.pMVPCountLabel = (Label*)SETUP_PANEL( new Label( this, tmpName, "" ) );
			playerDisplay.pMVPCountLabel->SetFont( m_MVPFont );
			playerDisplay.pMVPCountLabel->SetBounds( 0, -mvpLabelYOffset, mvpLabelWidth, tallMVP );
			playerDisplay.pMVPCountLabel->SetVisible( false );

			// Pin to the mvp image.
			V_snprintf( tmpName, 32, "_%s_mvp%d", szTeamPrefix, i );
			playerDisplay.pMVPCountLabel->PinToSibling( tmpName, PIN_CENTER_LEFT, PIN_CENTER_RIGHT );
		}

		// score
		if ( pPlayerScore0 != NULL )
		{
			pPlayerScore0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playerscore%d", szTeamPrefix, i );
			delete playerDisplay.pScoreLabel;
			playerDisplay.pScoreLabel = (Label*)SETUP_PANEL( new Label( this, tmpName, "" ) );
			playerDisplay.pScoreLabel->SetFont( m_listItemFont );
			playerDisplay.pScoreLabel->SetBounds( x, startY, wide, tall );
			playerDisplay.pScoreLabel->SetBgColor( m_PlayerDataBgColor );
			playerDisplay.pScoreLabel->SetFgColor( m_teamDisplayCT.playerDataColor );
			playerDisplay.pScoreLabel->SetContentAlignment( Label::a_center );
			playerDisplay.pScoreLabel->SetVisible( false );
		}

		// deaths
		if ( pPlayerDeaths0 != NULL )
		{
			pPlayerDeaths0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playerdeaths%d", szTeamPrefix, i );
			delete playerDisplay.pDeathsLabel;
			playerDisplay.pDeathsLabel = (Label*)SETUP_PANEL( new Label( this, tmpName, "" ) );
			playerDisplay.pDeathsLabel->SetFont( m_listItemFont );
			playerDisplay.pDeathsLabel->SetBounds( x, startY, wide, tall );
			playerDisplay.pDeathsLabel->SetBgColor( m_PlayerDataBgColor );
			playerDisplay.pDeathsLabel->SetFgColor( m_teamDisplayCT.playerDataColor );
			playerDisplay.pDeathsLabel->SetContentAlignment( Label::a_center );
			playerDisplay.pDeathsLabel->SetVisible( false );
		}

		// latency
		if ( pPlayerLatency0 != NULL )
		{
			pPlayerLatency0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playerping%d", szTeamPrefix, i );
			delete playerDisplay.pPingLabel;
			playerDisplay.pPingLabel = (Label*)SETUP_PANEL( new Label( this, tmpName, "" ) );
			playerDisplay.pPingLabel->SetFont( m_listItemFont );
			playerDisplay.pPingLabel->SetBounds( x, startY, wide, tall );
			playerDisplay.pPingLabel->SetBgColor( m_PlayerDataBgColor );
			playerDisplay.pPingLabel->SetFgColor( m_teamDisplayCT.playerDataColor );
			playerDisplay.pPingLabel->SetContentAlignment( Label::a_center );
			playerDisplay.pPingLabel->SetVisible( false );
		}

		// status
		if ( pPlayerStatus0 != NULL )
		{
			pPlayerStatus0->GetBounds( x, y, wide, tall );
			V_snprintf( tmpName, 32, "_%s_playerstatus%d", szTeamPrefix, i );
			delete playerDisplay.pStatusImage;
			playerDisplay.pStatusImage = (ImagePanel*)SETUP_PANEL( new ImagePanel( this, tmpName ) );
			playerDisplay.pStatusImage->SetBounds( x, startY, wide, tall );
			playerDisplay.pStatusImage->SetImage( "../hud/scoreboard_dead" );
			playerDisplay.pStatusImage->SetShouldScaleImage( true );
			playerDisplay.pStatusImage->SetVisible( false );
		}

		// select
		{
			int x1 = teamDisplay.scoreAreaMinX - selectMargin;
			int y1 = startY - selectMargin;
			int x2 = teamDisplay.scoreAreaMaxX + selectMargin;
			int y2 = startY + teamDisplay.scoreAreaLineHeight + selectMargin;

			V_snprintf( tmpName, 32, "_%s_playerselect%d", szTeamPrefix, i );
			delete playerDisplay.pSelect;
			playerDisplay.pSelect = (ImagePanel*)SETUP_PANEL( new ImagePanel( this, tmpName ) );
			playerDisplay.pSelect->SetBounds( x1, y1, x2 - x1, y2 - y1 );
			playerDisplay.pSelect->SetImage( "../vgui/scoreboard/scoreboard-select" );
			playerDisplay.pSelect->SetShouldScaleImage( true );
			playerDisplay.pSelect->SetVisible( false );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used for sorting players
//-----------------------------------------------------------------------------
int CCSClientScoreBoardDialog::PlayerSortFunction( PlayerScoreInfo* const* pLeft, PlayerScoreInfo* const* pRight )
{
	// a return value < 0 puts pLeft earlier in the list, > 0 puts pRight earlier
    const PlayerScoreInfo* pPlayer1 = *pLeft;
    const PlayerScoreInfo* pPlayer2 = *pRight;
    Assert( pPlayer1 && pPlayer2 );

    // bail out early if either player is an empty slot, i.e. has a player index of -1
    if( pPlayer1->playerIndex == -1 )
		return 1;
    if( pPlayer2->playerIndex == -1 )
		return -1;

    // first compare scores
    if ( pPlayer1->frags > pPlayer2->frags )
		return -1;
    if ( pPlayer1->frags < pPlayer2->frags )
		return 1;

    // second compare deaths
    if ( pPlayer1->deaths > pPlayer2->deaths )
		return 1;
    if ( pPlayer1->deaths < pPlayer2->deaths )
		return -1;

    // if score and deaths are the same, use player index to get deterministic sort
    if ( pPlayer1->playerIndex < pPlayer2->playerIndex )
		return -1;
	else
		return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Updates the dialog
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::Update()
{
    if ( m_pServerLabel )
    {
        m_pServerLabel->SetText( m_pServerName );
    }

	// Update the stats status.
    CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr*>( engine->GetAchievementMgr() );
    if ( pAchievementMgr != NULL &&
		 pAchievementMgr->CheckAchievementsEnabled() )
	{
		SetDialogVariable( "statsstatus", m_pStatsEnabled );
	}
	else
	{
		SetDialogVariable( "statsstatus", m_pStatsDisabled );
	}

    UpdateTeamInfo();
    UpdatePlayerList();
    UpdateSpectatorList();
	UpdateHLTVList();
    UpdateMatchEndText();
	UpdateMvpElements();

    // update every second
    m_fNextUpdateTime = gpGlobals->curtime + kUpdateInterval;

    // Catch the case where we call ShowPanel before ApplySchemeSettings, eg when going from windowed <-> fullscreen
    if ( m_pImageList == NULL )
    {
        InvalidateLayout( true, true );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Updates information about teams
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdateTeamInfo()
{
	if ( g_PR == NULL )
	{
		return;
	}

    // update the team sections in the scoreboard
    for ( int teamIndex = TEAM_TERRORIST; teamIndex <= TEAM_CT; teamIndex++ )
    {
        wchar_t *teamName = NULL;
        C_Team *team = GetGlobalTeam( teamIndex );
        if ( team )
        {
            // choose dialog variables to set depending on team
            const char *pDialogVarTeamName = NULL;
            const char *pDialogVarAliveCount = NULL;
			const char *pDialogVarTeamScore = NULL;
            switch ( teamIndex )
            {
            case TEAM_TERRORIST:
                teamName = g_pVGuiLocalize->Find( "#Cstrike_Team_T" );
                pDialogVarTeamName = "t_teamname";
				pDialogVarAliveCount = "t_alivecount";
				pDialogVarTeamScore = "t_totalteamscore";
                break;
            case TEAM_CT:
                teamName = g_pVGuiLocalize->Find( "#Cstrike_Team_CT" );
                pDialogVarTeamName = "ct_teamname";
				pDialogVarAliveCount = "ct_alivecount";
				pDialogVarTeamScore = "ct_totalteamscore";
                break;
            default:
                Assert( false );
                break;
            }

			// Set the team name if it hasn't been set.
            wchar_t name[64];
            if ( !teamName && team && team->Get_Name() != NULL )
            {
                g_pVGuiLocalize->ConvertANSIToUnicode( team->Get_Name(), name, sizeof( name ) );
                teamName = name;
            }

			// Count the players on the team.
            int numPlayers = 0;
			int numAlive = 0;
            for ( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; playerIndex++ )
            {
                if ( g_PR->IsConnected( playerIndex ) && g_PR->GetTeam( playerIndex ) == teamIndex )
                {
                    numPlayers++;
					if ( g_PR->IsAlive( playerIndex ) )
					{
						++numAlive;
					}
                }				
            }

            SetDialogVariable( pDialogVarTeamName, teamName );

			// Team score
            wchar_t wNumScore[16];
            V_snwprintf( wNumScore, ARRAYSIZE( wNumScore ), L"%i", team->Get_Score() );
            SetDialogVariable( pDialogVarTeamScore, wNumScore );

			// Number of alive players
			wchar_t numAliveString[32];
            V_snwprintf( numAliveString, ARRAYSIZE( numAliveString ), L"%i / %i", numAlive, numPlayers);
            SetDialogVariable( pDialogVarAliveCount, numAliveString );
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Helper to shrink font size when a string gets too long for its label
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::AdjustFontToFit( const char *pString, vgui::Label *pLabel )
{
	if ( !pString || !pLabel )
		return;

	int len = Q_strlen( pString );
	if ( !len )
		return;

	int arraySize = len + 1;
	wchar_t *pWideString = new wchar_t[arraySize];
	V_UTF8ToUnicode( pString, pWideString, (arraySize * sizeof(wchar_t)) );

	int stringWidth, stringHeight;
	g_pMatSystemSurface->GetTextSize( m_listItemFont, pWideString, stringWidth, stringHeight );

	int labelWidth = pLabel->GetWide();
	if ( stringWidth <= labelWidth )
	{
		pLabel->SetFont( m_listItemFont );
	}
	else
	{
		g_pMatSystemSurface->GetTextSize( m_listItemFontSmaller, pWideString, stringWidth, stringHeight );
		if ( stringWidth <= labelWidth )
		{
			pLabel->SetFont( m_listItemFontSmaller );
		}
		else
		{
			pLabel->SetFont(m_listItemFontSmallest);
		}
	}
	delete [] pWideString;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the player list
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdatePlayerList()
{
    m_teamDisplayT.playerScores.PurgeAndDeleteElements();
    m_teamDisplayCT.playerScores.PurgeAndDeleteElements();

    C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
    if ( !pLocalPlayer )
        return;

	// Set the player colors from the convars.
	UpdatePlayerColors();

    for( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; playerIndex++ )
    {
        if( g_PR->IsConnected( playerIndex ) )
        {
            PlayerScoreInfo* playerScoreInfo = new PlayerScoreInfo;
            if ( !GetPlayerScoreInfo( playerIndex, *playerScoreInfo ) )
			{
                delete playerScoreInfo;
				continue;
			}

            if ( g_PR->GetTeam( playerIndex ) == TEAM_TERRORIST )
            {
                m_teamDisplayT.playerScores.AddToTail(playerScoreInfo);
            }
            else if ( g_PR->GetTeam( playerIndex ) == TEAM_CT )
            {
                m_teamDisplayCT.playerScores.AddToTail(playerScoreInfo);
            }
            else
            {
				// [mhansen] make sure we don't leak here
                delete playerScoreInfo;
            }
        }
    }

    // Sort the lists of players
    m_teamDisplayT.playerScores.Sort(PlayerSortFunction);
    m_teamDisplayCT.playerScores.Sort(PlayerSortFunction);

    // Force the local player to be visible when he is below the visible portion of the sorted list
	if ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST )
		ForceLocalPlayerVisible(m_teamDisplayT);
	else if ( pLocalPlayer->GetTeamNumber() == TEAM_CT )
		ForceLocalPlayerVisible(m_teamDisplayCT);

	UpdateTeamPlayerDisplay(m_teamDisplayT);
	UpdateTeamPlayerDisplay(m_teamDisplayCT);
}

void CCSClientScoreBoardDialog::UpdateTeamPlayerDisplay( TeamDisplayInfo& teamDisplay )
{
	const int selectMargin = scheme()->GetProportionalScaledValueEx( GetScheme(), 1 );

	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	if ( !cs_PR )
		return;
	int iLocalPlayerIndex = GetLocalPlayerIndex();

	int maxTeamSize = MAX(m_teamDisplayT.playerScores.Count(), m_teamDisplayCT.playerScores.Count());

	// adjust spacing
	int leadingAvailable = teamDisplay.scoreAreaInnerHeight - maxTeamSize * teamDisplay.scoreAreaLineHeight;
	int leading = 0;
	if ( maxTeamSize > 1 )	// only makes sense if we have more than one player
	{
		leading = clamp(leadingAvailable / (maxTeamSize - 1), 0, teamDisplay.scoreAreaLinePreferredLeading);
	}
	int spacingY = teamDisplay.scoreAreaLineHeight + leading;

	// temp values for updating just the y position of elements
	int xPos, yPos;

	int i = 0;
	int startY = teamDisplay.scoreAreaStartY;
	for ( i = 0; i < MIN( cMaxScoreLines, teamDisplay.playerScores.Count() ); ++i, startY += spacingY )
	{
		if ( startY + teamDisplay.scoreAreaLineHeight > teamDisplay.scoreAreaStartY + teamDisplay.scoreAreaInnerHeight )
			break;

		PlayerDisplay& playerDisplay = teamDisplay.playerDisplay[i];

		const PlayerScoreInfo* pPlayerScore = teamDisplay.playerScores[i];
		if ( pPlayerScore )
		{
			int playerIndex = pPlayerScore->playerIndex;

			const char* pUTF8Name = pPlayerScore->szName;

// 			int bufsize;
// 			if ( g_PR->IsFakePlayer( playerIndex ) )
// 				bufsize = strlen( oldName ) * 2 + 14 + 1;
// 			else
// 				bufsize = strlen( oldName ) * 2 + 1;
// 
// 			char *newName = (char *)_alloca( bufsize );
// 			UTIL_MakeSafeName( oldName, newName, bufsize );

			if ( pUTF8Name != NULL && V_strlen( pUTF8Name ) > 0 )
			{
				bool isAlive = cs_PR->IsAlive( playerIndex );
				Color fgColor = ( isAlive ? teamDisplay.playerDataColor : m_DeadPlayerDataColor );
				Color fgClanColor = ( isAlive ? teamDisplay.playerClanColor : m_DeadPlayerClanColor );

				if ( playerDisplay.pNameLabel != NULL )
				{
					AdjustFontToFit( pUTF8Name, playerDisplay.pNameLabel );

					playerDisplay.pNameLabel->SetVisible( true );
					playerDisplay.pNameLabel->SetText( pUTF8Name );
					playerDisplay.pNameLabel->SetBgColor( m_PlayerDataBgColor );
					playerDisplay.pNameLabel->SetFgColor( fgColor );
					playerDisplay.pNameLabel->GetPos(xPos, yPos);
					playerDisplay.pNameLabel->SetPos(xPos, startY);

					int tallMVP = RoundFloatToInt(teamDisplay.scoreAreaLineHeight * kScaleMVP);
					int yMVP = ( teamDisplay.scoreAreaLineHeight - tallMVP ) / 2;
					playerDisplay.pMVPImage->SetPos( xPos, startY + yMVP);
				}

				if ( playerDisplay.pClanLabel != NULL )
				{
					const char* pUTF8Clan = pPlayerScore->szClanTag;
					AdjustFontToFit( pUTF8Clan, playerDisplay.pClanLabel );

					playerDisplay.pClanLabel->SetVisible( true );
					playerDisplay.pClanLabel->SetText( pUTF8Clan );
					playerDisplay.pClanLabel->SetBgColor( m_PlayerDataBgColor );
					playerDisplay.pClanLabel->SetFgColor( fgClanColor );
					playerDisplay.pClanLabel->GetPos(xPos, yPos);
					playerDisplay.pClanLabel->SetPos(xPos, startY);
				}

				char tmpbuf[16];

				if ( playerDisplay.pScoreLabel != NULL )
				{
					Q_snprintf( tmpbuf, sizeof( tmpbuf ), "%d", pPlayerScore->frags);
					playerDisplay.pScoreLabel->SetVisible( true );
					playerDisplay.pScoreLabel->SetText( tmpbuf );
					playerDisplay.pScoreLabel->SetBgColor( m_PlayerDataBgColor );
					playerDisplay.pScoreLabel->SetFgColor( fgColor );
					playerDisplay.pScoreLabel->GetPos(xPos, yPos);
					playerDisplay.pScoreLabel->SetPos(xPos, startY);
				}

				if ( playerDisplay.pDeathsLabel != NULL )
				{
					Q_snprintf( tmpbuf, sizeof( tmpbuf ), "%d", pPlayerScore->deaths);
					playerDisplay.pDeathsLabel->SetVisible( true );
					playerDisplay.pDeathsLabel->SetText( tmpbuf );
					playerDisplay.pDeathsLabel->SetBgColor( m_PlayerDataBgColor );
					playerDisplay.pDeathsLabel->SetFgColor( fgColor );
					playerDisplay.pDeathsLabel->GetPos(xPos, yPos);
					playerDisplay.pDeathsLabel->SetPos(xPos, startY);

				}

				if ( playerDisplay.pPingLabel != NULL )
				{
					if ( pPlayerScore->ping >= 0 )
						Q_snprintf( tmpbuf, sizeof( tmpbuf ), "%d", pPlayerScore->ping);
					else
						Q_strcpy( tmpbuf, "BOT");

					playerDisplay.pPingLabel->SetVisible( true );
					playerDisplay.pPingLabel->SetText( tmpbuf );
					playerDisplay.pPingLabel->SetBgColor( m_PlayerDataBgColor );
					playerDisplay.pPingLabel->SetFgColor( fgColor );
					playerDisplay.pPingLabel->GetPos(xPos, yPos);
					playerDisplay.pPingLabel->SetPos(xPos, startY);

				}
			}

			if ( playerDisplay.pStatusImage != NULL )
			{
				if ( pPlayerScore->szStatus == NULL )
				{
					playerDisplay.pStatusImage->SetVisible( false );
				}
				else
				{
					playerDisplay.pStatusImage->SetVisible( true );
					playerDisplay.pStatusImage->SetImage( pPlayerScore->szStatus );
					playerDisplay.pStatusImage->GetPos(xPos, yPos);
					playerDisplay.pStatusImage->SetPos(xPos, startY);
					playerDisplay.pStatusImage->SetDrawColor(pPlayerScore->bStatusPlayerColor ? teamDisplay.playerDataColor : COLOR_WHITE);
				}
			}

			if ( playerDisplay.pAvatar != NULL )
			{
				playerDisplay.pAvatar->SetVisible( true );
				playerDisplay.pAvatar->SetPlayer( playerIndex, k_EAvatarSize32x32 );				
				playerDisplay.pAvatar->GetPos(xPos, yPos);
				playerDisplay.pAvatar->SetPos(xPos, startY);
			}

			if ( playerDisplay.pSelect != NULL )
			{
				playerDisplay.pSelect->SetVisible( pPlayerScore->playerIndex == iLocalPlayerIndex );
				playerDisplay.pSelect->SetPos(teamDisplay.scoreAreaMinX - selectMargin, startY - selectMargin);
			}
		}
	}

	// set any remaining entries to non-visible
	for ( ; i < cMaxScoreLines; ++i )
	{
		PlayerDisplay& playerDisplay = teamDisplay.playerDisplay[i];

		if ( playerDisplay.pClanLabel != NULL )
			playerDisplay.pClanLabel->SetVisible(false);

		if ( playerDisplay.pNameLabel != NULL )
			playerDisplay.pNameLabel->SetVisible(false);

		if ( playerDisplay.pScoreLabel != NULL )
			playerDisplay.pScoreLabel->SetVisible(false);

		if ( playerDisplay.pDeathsLabel != NULL )
			playerDisplay.pDeathsLabel->SetVisible(false);

		if ( playerDisplay.pPingLabel != NULL )
			playerDisplay.pPingLabel->SetVisible(false);

		if ( playerDisplay.pStatusImage != NULL )
			playerDisplay.pStatusImage->SetVisible(false);

		if ( playerDisplay.pAvatar != NULL )
			playerDisplay.pAvatar->SetVisible( false );

		if ( playerDisplay.pMVPImage != NULL )
			playerDisplay.pMVPImage->SetVisible( false );

		if ( playerDisplay.pMVPCountLabel != NULL )
			playerDisplay.pMVPCountLabel->SetVisible( false );

		if ( playerDisplay.pSelect != NULL )
			playerDisplay.pSelect->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the spectator list
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdateSpectatorList()
{
	if ( g_PR == NULL )
	{
		return;
	}

	const int listTextLen = 100;
	wchar_t listText[listTextLen];
	listText[0] = L'\0';

	const wchar_t *delimText = L", ";
	const int delimTextLen = V_wcslen( delimText );

	// Count the number of spectators and build up a list.
	int nSpectators = 0;
	for ( int playerIndex = 1 ; playerIndex <= MAX_PLAYERS; ++playerIndex )
	{
		if ( ShouldShowAsSpectator( playerIndex ) )
		{
			const char *playerName = g_PR->GetPlayerName( playerIndex );
			if ( playerName != NULL )
			{
				// Convert the name to wide char.
				wchar_t playerBuf[MAX_PLAYER_NAME_LENGTH];
				playerBuf[0] = L'\0';
				V_UTF8ToUnicode( playerName, playerBuf, sizeof( playerBuf ) );

				//
				// Check to see if there is space for the player name and delimiter.
				//

				bool addDelim = ( nSpectators > 0 );

				int playerNameLen = V_wcslen( playerBuf );
				if ( addDelim )
				{
					playerNameLen += delimTextLen;
				}

				int currentLen = V_wcslen( listText );
				int remainingLen = listTextLen - currentLen - playerNameLen - 1;
				if ( remainingLen >= 0 )
				{
					// Append the delimiter.
					if ( addDelim )
					{
						V_wcscat_safe( listText, delimText );
					}

					// Append the player name.
					V_wcscat_safe( listText, playerBuf );
				}
			}

			++nSpectators;
		}
	}

	wchar_t labelText[512];
	labelText[0] = L'\0';

	if ( nSpectators == 0 )
	{
		// No spectators.
		wchar_t *noSpectators = g_pVGuiLocalize->Find( "#Cstrike_Scoreboard_NoSpectators" );
		if ( noSpectators != NULL )
		{
			V_wcsncpy( labelText, noSpectators, sizeof( labelText ) );
		}
	}
	else
	{
		// Build the text for the number of spectators.
		const int countTextLen = 16;
		wchar_t countText[countTextLen];
		countText[0] = L'\0';
		V_snwprintf( countText, countTextLen, L"%i", nSpectators );
		countText[countTextLen - 1] = L'\0';

		// Build the combined count and spectator list text.
		wchar_t *formatLabel = g_pVGuiLocalize->Find( ( nSpectators == 1 ) ? "#Cstrike_Scoreboard_Spectator" : "#Cstrike_Scoreboard_Spectators" );
		if ( formatLabel != NULL )
		{
			g_pVGuiLocalize->ConstructString( labelText, sizeof( labelText ), formatLabel, 2, countText, listText );
		}
	}

	SetDialogVariable( "spectators", labelText );
}

//-----------------------------------------------------------------------------
// Purpose: Display the number of HLTV viewers
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdateHLTVList( void )
{
	// Build the text for the number of viewers.
	const int countTextLen = 16;
	wchar_t countText[countTextLen];
	countText[0] = L'\0';
	V_snwprintf( countText, countTextLen, L"%i", m_HLTVSpectators );
	countText[countTextLen - 1] = L'\0';

	// Build the combined text.
	wchar_t labelText[512];
	labelText[0] = L'\0';
	wchar_t *formatLabel = g_pVGuiLocalize->Find( "#Cstrike_Scoreboard_HLTV" );
	if ( formatLabel != NULL )
	{
		g_pVGuiLocalize->ConstructString( labelText, sizeof( labelText ), formatLabel, 1, countText );
	}

	SetDialogVariable( "sourcetv", labelText );
}

/**
*	Special processing for MVP UI elements
*/
void CCSClientScoreBoardDialog::UpdateMvpElements()
{
	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	if ( cs_PR == NULL )
	{
		return;
	}

	for ( int teamIndex = TEAM_TERRORIST; teamIndex <= TEAM_CT; ++teamIndex )
	{
		TeamDisplayInfo& teamDisplay = ( teamIndex == TEAM_TERRORIST ) ? m_teamDisplayT : m_teamDisplayCT;

		for ( int i = 0; i < cMaxScoreLines && i < teamDisplay.playerScores.Count(); ++i )
		{
			PlayerDisplay& playerDisplay = teamDisplay.playerDisplay[i];

			// Get the player index.
			int playerIndex = -1;
			if ( teamDisplay.playerScores[i] != NULL )
			{
				playerIndex = teamDisplay.playerScores[i]->playerIndex;
			}

			if ( playerIndex == -1 )
				continue;

			// early out fail conditions
			if ( playerDisplay.pNameLabel == NULL || playerDisplay.pMVPImage == NULL || playerDisplay.pMVPCountLabel == NULL )
				continue;

			// Get the number of MVPs and cap it.
			int numMVPs = MIN( kMaxMVPCount, cs_PR->GetNumMVPs( playerIndex ) );

			// No MVPs.
			if ( numMVPs == 0 )
			{
				playerDisplay.pMVPImage->SetVisible( false );
				playerDisplay.pMVPCountLabel->SetVisible( false );
				continue;
			}

			// Get the dimensions of the player label.
			int xName = 0;
			int yName = 0;
			int wideName = 0;
			int tallName = 0;
			playerDisplay.pNameLabel->GetBounds( xName, yName, wideName, tallName );

			wchar_t playerName[64];
			playerDisplay.pNameLabel->GetText( playerName, sizeof(playerName) );

			// Find the actual width of the rendered player name.
			int actualWideName = 0;
			int actualTallName = 0;
			g_pMatSystemSurface->GetTextSize( playerDisplay.pNameLabel->GetFont(), playerName, actualWideName, actualTallName );
			if ( actualWideName > wideName )
			{
				actualWideName = wideName;
			}

			// Get the dimensions of the mvp image.
			int xMVPImage = 0;
			int yMVPImage = 0;
			int wideMVPImage = 0;
			int tallMVPImage = 0;
			playerDisplay.pMVPImage->GetBounds( xMVPImage, yMVPImage, wideMVPImage, tallMVPImage );

			// The MVP label is hidden for only one star.
			bool showMVPNumber = ( numMVPs > 1 );

			// Set the MVP label text and get the actual size.
			int actualWideMVPLabel = 0;
			if ( showMVPNumber )
			{
				const int mvpTextSize = 8;
				wchar_t mvpText[mvpTextSize];
				V_snwprintf( mvpText, ARRAYSIZE( mvpText ), L"%i", numMVPs );
				playerDisplay.pMVPCountLabel->SetText( mvpText );
				playerDisplay.pMVPCountLabel->SetVisible( true );

				int actualTallMVPLabel = 0;
				g_pMatSystemSurface->GetTextSize( playerDisplay.pMVPCountLabel->GetFont(), mvpText, actualWideMVPLabel, actualTallMVPLabel );
			}

			// Calculate the total width of the mvp stuff.
			int wideMVPTotal = wideMVPImage;
			if ( showMVPNumber )
			{
				wideMVPTotal += actualWideMVPLabel;
			}

			// Calculate the optimal place for the mvp.
			int x = xName + actualWideName + m_MVPXOffset;

			// Get the position of the status image.
			if ( playerDisplay.pStatusImage )
			{
				int xStatus = 0;
				int yStatus = 0;
				playerDisplay.pStatusImage->GetPos( xStatus, yStatus );

				if ( x + wideMVPTotal > xStatus )
				{
					// Don't run over the status image. Back up.
					x = xStatus - wideMVPTotal;
				}
			}

			playerDisplay.pMVPImage->SetPos( x, yMVPImage );
			playerDisplay.pMVPImage->SetVisible( true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether the specified player index is a spectator
//-----------------------------------------------------------------------------
bool CCSClientScoreBoardDialog::ShouldShowAsSpectator( int iPlayerIndex )
{
    C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
    if ( !cs_PR )
        return false;

    // see if player is connected
    if ( cs_PR->IsConnected( iPlayerIndex ) ) 
    {
        // either spectator or unassigned team should show in spectator list
        int iTeam = cs_PR->GetTeam( iPlayerIndex );
        if ( TEAM_SPECTATOR == iTeam || TEAM_UNASSIGNED == iTeam )
            return true;
    }
    return false;
}


void CCSClientScoreBoardDialog::FireGameEvent( IGameEvent *event )
{
	if ( event == NULL )
		return;

    const char *pEventName = event->GetName();
	if ( pEventName == NULL )
		return;

    if ( Q_strcmp( pEventName, "server_spawn" ) == 0 )
    {
        // set server name in scoreboard
        const char *hostname = event->GetString( "hostname" );
		if ( hostname != NULL )
		{
			wchar_t wzHostName[256];
			g_pVGuiLocalize->ConvertANSIToUnicode( hostname, wzHostName, sizeof( wzHostName ) );
			g_pVGuiLocalize->ConstructString( m_pServerName, sizeof(m_pServerName), g_pVGuiLocalize->Find( "#Cstrike_SB_Server" ), 1, wzHostName );

			if ( m_pServerLabel )
			{
				m_pServerLabel->SetText( m_pServerName );
			}

			if ( m_gameOver )
			{
				ResetFromGameOverState();
			}

			// Save the server name for use after this panel is reconstructed
			if ( g_pClientMode )
			{
				g_pClientMode->SetServerName( m_pServerName );
			}
		}
    }
    else if ( Q_strcmp( pEventName, "game_newmap" ) == 0 )
    {
        const char *mapName = event->GetString( "mapname" );
		if ( mapName != NULL )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( mapName, m_pMapName, sizeof( m_pMapName ) );
			SetDialogVariable( "mapname", m_pMapName );

			if ( m_gameOver )
			{
				ResetFromGameOverState();
			}

			// Save the map name for use after this panel is reconstructed
			if ( g_pClientMode )
			{
				g_pClientMode->SetMapName( m_pMapName );
			}
		}
    }
    else if ( Q_strcmp( pEventName, "match_end_conditions" ) == 0 )
    {
        UpdateMatchEndText();
    }
    else if ( Q_strcmp( pEventName, "cs_win_panel_match" ) == 0 )
    {
        m_gameOver = true;
        UpdateMatchEndText();
    }

	BaseClass::FireGameEvent( event );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a new row to the scoreboard, from the playerinfo structure
//-----------------------------------------------------------------------------
bool CCSClientScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, PlayerScoreInfo& playerScoreInfo )
{
	if ( g_PR == NULL )
		return false;

    // Clean up the player name
    const char *oldName = g_PR->GetPlayerName( playerIndex );
	if ( oldName == NULL )
		return false;

	playerScoreInfo.szName = g_PR->GetPlayerName( playerIndex );

    playerScoreInfo.playerIndex = playerIndex;
    playerScoreInfo.frags = g_PR->GetPlayerScore( playerIndex );
    playerScoreInfo.deaths = g_PR->GetDeaths( playerIndex );

    if ( g_PR->GetPing( playerIndex ) < 1 )
    {
        if ( g_PR->IsFakePlayer( playerIndex ) )
        {
			playerScoreInfo.ping = -1;
        }
        else
        {
			playerScoreInfo.ping = 0;
        }
    }
    else
    {
		playerScoreInfo.ping = g_PR->GetPing( playerIndex );
    }

    // get CS specific infos
    C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

    C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

    if ( !cs_PR || !pLocalPlayer )
	{
        return false;
	}

	// Get the clan tag
	playerScoreInfo.szClanTag = cs_PR->GetClanTag( playerIndex );

    bool bShowExtraInfo = 
        ( pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED ) || // we're not spawned yet
        ( pLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR ) || // we are a spectator
        ( pLocalPlayer->IsPlayerDead() && mp_forcecamera.GetInt() == OBS_ALLOW_ALL ) ||	 // we are dead and allowed to spectate opponents
        ( pLocalPlayer->GetTeamNumber() == g_PR->GetTeam( playerIndex ) ); // we're on the same team

	playerScoreInfo.szStatus = NULL;
	playerScoreInfo.bStatusPlayerColor = false;

	// set the status icon; lowest priority icons are tested first, and highest last

	if ( cs_PR->IsVIP( playerIndex ) && bShowExtraInfo )
	{
		playerScoreInfo.szStatus = "../hud/scoreboard_clock";
		playerScoreInfo.bStatusPlayerColor = true;
	}

	if ( !g_PR->IsAlive( playerIndex ) && g_PR->GetTeam( playerIndex ) > TEAM_SPECTATOR )
	{
		playerScoreInfo.szStatus = "../hud/scoreboard_dead";
	}

    if ( g_PR->IsHLTV( playerIndex ) )
    {
//         // show #spectators in class field, it's transmitted as player's score
//         char numspecs[32];
//         Q_snprintf( numspecs, sizeof( numspecs ), "%i Spectators", m_HLTVSpectators );
//         kv->SetString( "class", numspecs );
    }

    // Set the dominated icon
	if ( pLocalPlayer->IsPlayerDominatingMe( playerIndex ) )
	{
		if ( g_PR->IsAlive( playerIndex ) )
		{
			playerScoreInfo.szStatus = "../hud/scoreboard_nemesis";
		}
		else
		{
			playerScoreInfo.szStatus = "../hud/scoreboard_nemesis-dead";
		}
	}

	if ( pLocalPlayer->IsPlayerDominated(playerIndex) )
	{
		if ( g_PR->IsAlive( playerIndex ) )
		{
			playerScoreInfo.szStatus = "../hud/scoreboard_dominated";
		}
		else
		{
			playerScoreInfo.szStatus = "../hud/scoreboard_domination-dead";
		}
	}

	if ( cs_PR->HasC4( playerIndex ) && bShowExtraInfo )
	{
 		playerScoreInfo.szStatus = "../hud/scoreboard_bomb";
		playerScoreInfo.bStatusPlayerColor = true;
	}

	if ( cs_PR->HasDefuser( playerIndex ) && bShowExtraInfo )
	{
		playerScoreInfo.szStatus = "../hud/scoreboard_defuser";
		playerScoreInfo.bStatusPlayerColor = true;
	}

    return true;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the time/round remaining display and server and map name
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdateMatchEndText()
{
	// Hide the win condition.
	if ( m_pWinConditionLabel )
	{
		m_pWinConditionLabel->SetVisible( false );
	}

	// Hide the clock.
	if ( m_pClockLabel )
	{
		m_pClockLabel->SetVisible( false );
	}

	if ( !m_gameOver )
	{
		SetDialogVariable( "mapname", m_pMapName );

		wchar_t wzMatchEndCausesLabel[128], wzMatchEndCause[32];

		// Time limit
		if ( mp_timelimit.GetInt() != 0 )
		{
			int timeTillEndOfMatch = CSGameRules()->GetMapRemainingTime();
			bool showTime = ( timeTillEndOfMatch != -1 );
			if ( showTime )
			{
				if ( m_pWinConditionLabel )
				{
					V_snwprintf( wzMatchEndCause, ARRAYSIZE( wzMatchEndCause ), L"%.2i:%.2i", timeTillEndOfMatch / 60, timeTillEndOfMatch % 60 );
					g_pVGuiLocalize->ConstructString( wzMatchEndCausesLabel, sizeof( wzMatchEndCausesLabel ),
						g_pVGuiLocalize->Find( "#Cstrike_Time_LeftVariable" ), 1, wzMatchEndCause );
					m_pWinConditionLabel->SetText( wzMatchEndCausesLabel );
					m_pWinConditionLabel->SetVisible( true );
				}

				if ( m_pClockLabel )
				{
					m_pClockLabel->SetVisible( true );
				}
			}
		}
		// Round limit
		else if ( mp_maxrounds.GetInt() != 0 )
		{
			if ( m_pWinConditionLabel )
			{
				// Get the number of rounds played.
				int roundsPlayed = 0;
				for ( int teamIndex = TEAM_TERRORIST; teamIndex <= TEAM_CT; teamIndex++ )
				{
					C_Team *team = GetGlobalTeam( teamIndex );
					if ( team )
					{
						roundsPlayed += team->Get_Score();
					}
				}

				V_snwprintf( wzMatchEndCause, ARRAYSIZE( wzMatchEndCause ), L"%d", mp_maxrounds.GetInt() - roundsPlayed );
				g_pVGuiLocalize->ConstructString( wzMatchEndCausesLabel, sizeof( wzMatchEndCausesLabel ),
					g_pVGuiLocalize->Find( "#Cstrike_Rounds_LeftVariable" ), 1, wzMatchEndCause );
				m_pWinConditionLabel->SetText( wzMatchEndCausesLabel );
				m_pWinConditionLabel->SetVisible( true );
			}
		}
		// Win limit
		else if ( mp_winlimit.GetInt() != 0 )
		{
			if ( m_pWinConditionLabel )
			{
				V_snwprintf( wzMatchEndCause, ARRAYSIZE( wzMatchEndCause ), L"%d", mp_winlimit.GetInt() );
				g_pVGuiLocalize->ConstructString( wzMatchEndCausesLabel, sizeof( wzMatchEndCausesLabel ),
					g_pVGuiLocalize->Find( "#Cstrike_Wins_NeededVariable" ), 1, wzMatchEndCause );
				m_pWinConditionLabel->SetText( wzMatchEndCausesLabel );
				m_pWinConditionLabel->SetVisible( true );
			}
		}
	}
}

// Resets all changes made by the scoreboard's state at the match end
void CCSClientScoreBoardDialog::ResetFromGameOverState()
{
    m_gameOver = false;

    if ( m_pLabelMapName )
    {
        m_pLabelMapName->SetVisible( true );
    }

    UpdateMatchEndText();
}

// [tj] We hook into the show command so we can lock or unlock all the elements that need to be hidden
//
// [pfreese] This used to enable/disable keyboard input, but since the scoreboard is now a popup, we have
// to leave the keyboard disabled
void CCSClientScoreBoardDialog::ShowPanel( bool state )
{
    BaseClass::ShowPanel(state);

    int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "hide_for_scoreboard" );

    if ( state )
    {	
        gHUD.LockRenderGroup( iRenderGroup );
    }
    else
    {		
        gHUD.UnlockRenderGroup( iRenderGroup );
    }
}


//-----------------------------------------------------------------------------
// Purpose: Grabs the player data colors from the convars.
//-----------------------------------------------------------------------------
void CCSClientScoreBoardDialog::UpdatePlayerColors( void )
{
	m_teamDisplayT.playerDataColor.SetColor( cl_scoreboard_t_color_red.GetInt(), cl_scoreboard_t_color_green.GetInt(), cl_scoreboard_t_color_blue.GetInt(), 255 );
	m_teamDisplayCT.playerDataColor.SetColor( cl_scoreboard_ct_color_red.GetInt(), cl_scoreboard_ct_color_green.GetInt(), cl_scoreboard_ct_color_blue.GetInt(), 255 );
	m_DeadPlayerDataColor.SetColor( cl_scoreboard_dead_color_red.GetInt(), cl_scoreboard_dead_color_green.GetInt(), cl_scoreboard_dead_color_blue.GetInt(), 255 );
	m_teamDisplayT.playerClanColor.SetColor( cl_scoreboard_clan_t_color_red.GetInt(), cl_scoreboard_clan_t_color_green.GetInt(), cl_scoreboard_clan_t_color_blue.GetInt(), 255 );
	m_teamDisplayCT.playerClanColor.SetColor( cl_scoreboard_clan_ct_color_red.GetInt(), cl_scoreboard_clan_ct_color_green.GetInt(), cl_scoreboard_clan_ct_color_blue.GetInt(), 255 );
	m_DeadPlayerClanColor.SetColor( cl_scoreboard_dead_clan_color_red.GetInt(), cl_scoreboard_dead_clan_color_green.GetInt(), cl_scoreboard_dead_clan_color_blue.GetInt(), 255 );
}

// [tj] Disabling joystick input if you are dead.
void CCSClientScoreBoardDialog::OnThink()
{
    BaseClass::OnThink();

#ifdef _XBOX
    C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
    if ( pLocalPlayer )
    {
        bool mouseEnabled = IsMouseInputEnabled();
        if (pLocalPlayer->IsAlive() == mouseEnabled)
        {
            SetMouseInputEnabled( !mouseEnabled );
        }
    }
#endif
}

bool CCSClientScoreBoardDialog::ForceLocalPlayerVisible( TeamDisplayInfo& teamDisplay )
{
	int iLocalPlayerIndex = GetLocalPlayerIndex();

	// Look for the local player in the non-visible portion of the member list
	for (int i = teamDisplay.maxPlayersVisible; i < teamDisplay.playerScores.Count(); ++i)
	{
		PlayerScoreInfo*  pPlayerScore = teamDisplay.playerScores[i];
		Assert(pPlayerScore != NULL);

		// Determine if this is the local player's entry
		if ( pPlayerScore->playerIndex == iLocalPlayerIndex )
		{
			// Remove the local player entry from the current position
			teamDisplay.playerScores.Remove(i);

			// Re-insert the local player entry at the end of the visible list
			teamDisplay.playerScores.InsertBefore( teamDisplay.maxPlayersVisible - 1, pPlayerScore );

			return true;
		}
	}
	return false;
}
