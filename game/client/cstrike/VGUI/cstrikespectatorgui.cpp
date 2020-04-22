//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstrikespectatorgui.h"
#include "hud.h"
#include "cs_shareddefs.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <filesystem.h>
#include "cs_gamerules.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_plantedc4.h"
#include "c_cs_hostage.h"
#include "vtf/vtf.h"
#include "clientmode.h"
#include <vgui_controls/AnimationController.h>
#include "voice_status.h"
#include "hud_radar.h"

using namespace vgui;
DECLARE_HUDELEMENT( CCSMapOverview )

extern ConVar overview_health;
extern ConVar overview_names;
extern ConVar overview_tracks;
extern ConVar overview_locked;
extern ConVar overview_alpha;
extern ConVar cl_radaralpha;
ConVar cl_radar_locked( "cl_radar_locked", "0", FCVAR_ARCHIVE, "Lock the angle of the radar display?" );

void PreferredOverviewModeChanged( IConVar *pConVar, const char *oldString, float flOldValue )
{
	ConVarRef var( pConVar );
	char cmd[32];
	V_snprintf( cmd, sizeof( cmd ), "overview_mode %d\n", var.GetInt() );
	engine->ClientCmd( cmd );
}
ConVar overview_preferred_mode( "overview_preferred_mode", "1", FCVAR_ARCHIVE, "Preferred overview mode", PreferredOverviewModeChanged );

ConVar overview_preferred_view_size( "overview_preferred_view_size", "600", FCVAR_ARCHIVE, "Preferred overview view size" );

#define HOSTAGE_RESCUE_DURATION (2.5f)
#define BOMB_FADE_DURATION (2.5f)
#define DEATH_ICON_FADE (7.5f)
#define DEATH_ICON_DURATION (10.0f)
#define LAST_SEEN_ICON_DURATION (4.0f)
#define DIFFERENCE_THRESHOLD (200.0f)

// To make your own green radar file from the map overview file, turn this on, and include vtf.lib
#define no_GENERATE_RADAR_FILE

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSSpectatorGUI::CCSSpectatorGUI(IViewPort *pViewPort) : CSpectatorGUI(pViewPort)
{
	m_pCTLabel =	NULL;
	m_pCTScore =	NULL;
	m_pTerLabel =	NULL;
	m_pTerScore =	NULL;
	m_pTimer =		NULL;
	m_pTimerLabel =	NULL;
	m_pDivider =	NULL;
	m_pExtraInfo =	NULL;

	m_modifiedWidths = false;

	m_scoreWidth = 0;
	m_extraInfoWidth = 0;


}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Grab some control pointers
	m_pCTLabel =	dynamic_cast<Label *>(FindChildByName("CTScoreLabel"));
	m_pCTScore =	dynamic_cast<Label *>(FindChildByName("CTScoreValue"));
	m_pTerLabel =	dynamic_cast<Label *>(FindChildByName("TerScoreLabel"));
	m_pTerScore =	dynamic_cast<Label *>(FindChildByName("TerScoreValue"));

	m_pTimer =		dynamic_cast<Label *>(FindChildByName("timerclock"));
	m_pTimerLabel =	dynamic_cast<Label *>(FindChildByName("timerlabel"));

	m_pDivider =	dynamic_cast<Panel *>(FindChildByName("DividerBar"));

	m_pExtraInfo =	dynamic_cast<Label *>(FindChildByName("extrainfo"));
}

//-----------------------------------------------------------------------------
// Purpose: Resets the list of players
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::UpdateSpectatorPlayerList()
{
	C_Team *cts = GetGlobalTeam( TEAM_CT );
	if ( cts )
	{
		wchar_t frags[ 10 ];
		_snwprintf( frags, ARRAYSIZE( frags ), L"%i",  cts->Get_Score()  );

		SetLabelText( "CTScoreValue", frags );
	}

	C_Team *ts = GetGlobalTeam( TEAM_TERRORIST );
	if ( ts )
	{
		wchar_t frags[ 10 ];
		_snwprintf( frags, ARRAYSIZE( frags ), L"%i", ts->Get_Score()  );
		
		SetLabelText( "TERScoreValue", frags );
	}
}

bool CCSSpectatorGUI::NeedsUpdate( void )
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
	if ( !player )
		return false;

	if ( m_nLastAccount != player->GetAccount() )
		return true;

	if ( m_nLastTime != (int)CSGameRules()->GetRoundRemainingTime() )
		return true;

	if ( m_nLastSpecMode != player->GetObserverMode() )
		return true;

	if ( m_nLastSpecTarget != player->GetObserverTarget() )
		return true;

	return BaseClass::NeedsUpdate();
}

//=============================================================================
// HPE_BEGIN:
// [smessick]
//=============================================================================
void CCSSpectatorGUI::ShowPanel( bool bShow )
{
	BaseClass::ShowPanel( bShow );

	if ( bShow )
	{
		// Resend the overview command.
		char cmd[32];
		V_snprintf( cmd, sizeof( cmd ), "overview_mode %d\n", overview_preferred_mode.GetInt() );
		engine->ClientCmd( cmd );
	}
}
//=============================================================================
// HPE_END
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: Updates the timer label if one exists
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::UpdateTimer()
{
	// these could be NULL if players modified the UI
	if ( !ControlsPresent() )
		return;

	Color timerColor = m_pTimer->GetFgColor();
	if( g_PlantedC4s.Count() > 0 )
	{
		m_pTimer->SetText( "\\" ); // bomb icon  
		m_pTimerLabel->SetVisible( false );

		if( g_PlantedC4s[0]->m_flNextGlow > gpGlobals->curtime + 0.1f )
			timerColor[3] = 80;
		else
			timerColor[3] = 255;

		m_pTimer->SetFgColor( timerColor );
		return;
	}

	timerColor[3] = 255;
	m_pTimer->SetFgColor( timerColor );
	m_pTimer->SetText( "e" ); // clock icon
	
	m_nLastTime = (int)( CSGameRules()->GetRoundRemainingTime() );

	if ( m_nLastTime < 0 )
		 m_nLastTime  = 0;

	wchar_t szText[ 63 ];
	_snwprintf ( szText, ARRAYSIZE( szText ), L"%d:%02d", (m_nLastTime / 60), (m_nLastTime % 60) );
	szText[62] = 0;

	SetLabelText("timerlabel", szText );
	m_pTimerLabel->SetVisible( true );
}

void CCSSpectatorGUI::UpdateAccount()
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();

	if ( !player )
		return;

	m_nLastAccount = player->GetAccount();

	if ( (player->GetTeamNumber() == TEAM_TERRORIST) || (player->GetTeamNumber() == TEAM_CT) )
	{
		wchar_t szText[ 63 ];
		_snwprintf ( szText, ARRAYSIZE( szText ), L"$%i", m_nLastAccount );
		szText[62] = 0;

		SetLabelText( "extrainfo", szText );
	}
}


/*bool CCSSpectatorGUI::CanSpectateTeam( int iTeam )
{
	bool bRetVal = true;
	int iTeamOnly = 0;// TODO = gCSViewPortInterface->GetForceCamera();

	// if we're not a spectator or HLTV and iTeamOnly is set
	if ( C_BasePlayer::GetLocalPlayer()->GetTeamNumber() // && !gEngfuncs.IsSpectateOnly() 
	&& iTeamOnly )
	{
		// then we want to force the same team
		if ( C_BasePlayer::GetLocalPlayer()->GetTeamNumber() != iTeam )
		{
			bRetVal = false;
		}
	}

	return bRetVal;
}*/

void CCSSpectatorGUI::Update()
{
	BaseClass::Update();
	
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	if( pLocalPlayer )
	{
		m_nLastSpecMode = pLocalPlayer->GetObserverMode();
		m_nLastSpecTarget = pLocalPlayer->GetObserverTarget();
	}

	UpdateTimer();

	UpdateAccount();

	UpdateSpectatorPlayerList();

	if ( pLocalPlayer )
	{
		ResizeControls();
	}

}


//-----------------------------------------------------------------------------
// Purpose: Save off widths for sizing calculations
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::StoreWidths( void )
{
	if ( !ControlsPresent() )
		return;

	if ( !m_modifiedWidths )
	{
		m_scoreWidth = m_pCTScore->GetWide();
		int terScoreWidth = m_pTerScore->GetWide();

		m_extraInfoWidth = m_pExtraInfo->GetWide();

		if ( m_scoreWidth != terScoreWidth )
		{
			m_pTerScore = NULL; // We're working with a modified res file.  Don't muck things up playing with positioning.
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resize controls so scores & map names are not cut off
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::ResizeControls( void )
{
	if ( !ControlsPresent() )
		return;

	int x1, y1, w1, t1;
	int x2, y2, w2, t2;

	StoreWidths();

	// ensure scores are wide enough
	int wCT, hCT, wTer, hTer;
	m_pCTScore->GetBounds( x1, y1, w1, t1 );
	m_pCTScore->GetContentSize( wCT, hCT );
	m_pTerScore->GetBounds( x2, y2, w2, t2 );
	m_pTerScore->GetContentSize( wTer, hTer );
	
	int desiredScoreWidth = m_scoreWidth;
	desiredScoreWidth = MAX( desiredScoreWidth, wCT );
	desiredScoreWidth = MAX( desiredScoreWidth, wTer );

	int diff = desiredScoreWidth - w1;
	if ( diff != 0 )
	{
		m_pCTScore->GetBounds( x1, y1, w1, t1 );
		m_pCTScore->SetBounds( x1 - diff, y1, w1 + diff, t1 );

		m_pTerScore->GetBounds( x1, y1, w1, t1 );
		m_pTerScore->SetBounds( x1 - diff, y1, w1 + diff, t1 );

		m_pCTLabel->GetPos( x1, y1 );
		m_pCTLabel->SetPos( x1 - diff, y1 );

		m_pTerLabel->GetPos( x1, y1 );
		m_pTerLabel->SetPos( x1 - diff, y1 );

		m_modifiedWidths = true;
	}

	// ensure extra info is wide enough
	int wExtra, hExtra;
	m_pExtraInfo->GetBounds( x1, y1, w1, t1 );
	m_pExtraInfo->GetContentSize( wExtra, hExtra );

	int desiredExtraWidth = m_extraInfoWidth;
	desiredExtraWidth = MAX( desiredExtraWidth, wExtra );

	diff = desiredExtraWidth - w1;
	if ( diff != 0 )
	{
		m_pExtraInfo->GetBounds( x1, y1, w1, t1 );
		m_pExtraInfo->SetBounds( x1 - diff, y1, w1 + diff, t1 );

		m_pTimer->GetPos( x1, y1 );
		m_pTimer->SetPos( x1 - diff, y1 );

		m_pTimerLabel->GetPos( x1, y1 );
		m_pTimerLabel->SetPos( x1 - diff, y1 );

		m_pDivider->GetPos( x1, y1 );
		m_pDivider->SetPos( x1 - diff, y1 );

		m_pCTScore->GetPos( x1, y1 );
		m_pCTScore->SetPos( x1 - diff, y1 );

		m_pCTLabel->GetPos( x1, y1 );
		m_pCTLabel->SetPos( x1 - diff, y1 );

		m_pTerScore->GetPos( x1, y1 );
		m_pTerScore->SetPos( x1 - diff, y1 );

		m_pTerLabel->GetPos( x1, y1 );
		m_pTerLabel->SetPos( x1 - diff, y1 );

		m_modifiedWidths = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Verify controls are present to resize
//-----------------------------------------------------------------------------
bool CCSSpectatorGUI::ControlsPresent( void ) const
{
	return ( m_pCTLabel != NULL &&
		m_pCTScore != NULL &&
		m_pTerLabel != NULL &&
		m_pTerScore != NULL &&
		m_pTimer != NULL &&
		m_pTimerLabel != NULL &&
		m_pDivider != NULL &&
		m_pExtraInfo != NULL );
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int AdjustValue( int curValue, int targetValue, int amount )
{
	if ( curValue > targetValue )
	{
		curValue -= amount;

		if ( curValue < targetValue )
			curValue = targetValue;
	}
	else if ( curValue < targetValue )
	{
		curValue += amount;

		if ( curValue > targetValue )
			curValue = targetValue;
	}

	return curValue;
}

void CCSMapOverview::InitTeamColorsAndIcons()
{
	BaseClass::InitTeamColorsAndIcons();

	Q_memset( m_TeamIconsSelf, 0, sizeof(m_TeamIconsSelf) );
	Q_memset( m_TeamIconsDead, 0, sizeof(m_TeamIconsDead) );
	Q_memset( m_TeamIconsOffscreen, 0, sizeof(m_TeamIconsOffscreen) );
	Q_memset( m_TeamIconsDeadOffscreen, 0, sizeof(m_TeamIconsDeadOffscreen) );

	m_bombIconPlanted = -1;
	m_bombIconDropped = -1;
	m_bombIconCarried = -1;
	m_bombRingPlanted = -1;
	m_bombRingDropped = -1;
	m_bombRingCarried = -1;
	m_bombRingCarriedOffscreen = -1;
	m_radioFlash = -1;
	m_radioFlashOffscreen = -1;
	m_radarTint = -1;
	m_hostageFollowing = -1;
	m_hostageFollowingOffscreen = -1;
	m_playerFacing = -1;
	m_cameraIconFirst = -1;
	m_cameraIconThird = -1;
	m_cameraIconFree = -1;
	m_hostageRescueIcon = -1;
	m_bombSiteIconA = -1;
	m_bombSiteIconB = -1;


	//setup team red
	m_TeamColors[MAP_ICON_T] = COLOR_RED;
	m_TeamIcons[MAP_ICON_T] = AddIconTexture( "sprites/player_red_small" );
	m_TeamIconsSelf[MAP_ICON_T] = AddIconTexture( "sprites/player_red_self" );
	m_TeamIconsDead[MAP_ICON_T] = AddIconTexture( "sprites/player_red_dead" );
	m_TeamIconsOffscreen[MAP_ICON_T] = AddIconTexture( "sprites/player_red_offscreen" );
	m_TeamIconsDeadOffscreen[MAP_ICON_T] = AddIconTexture( "sprites/player_red_dead_offscreen" );

	// setup team blue
	m_TeamColors[MAP_ICON_CT] = COLOR_BLUE;
	m_TeamIcons[MAP_ICON_CT] = AddIconTexture( "sprites/player_blue_small" );
	m_TeamIconsSelf[MAP_ICON_CT] = AddIconTexture( "sprites/player_blue_self" );
	m_TeamIconsDead[MAP_ICON_CT] = AddIconTexture( "sprites/player_blue_dead" );
	m_TeamIconsOffscreen[MAP_ICON_CT] = AddIconTexture( "sprites/player_blue_offscreen" );
	m_TeamIconsDeadOffscreen[MAP_ICON_CT] = AddIconTexture( "sprites/player_blue_dead_offscreen" );

	// setup team other
	m_TeamColors[MAP_ICON_HOSTAGE] = COLOR_GREY;
	m_TeamIcons[MAP_ICON_HOSTAGE] = AddIconTexture( "sprites/player_hostage_small" );
	m_TeamIconsSelf[MAP_ICON_HOSTAGE] = -1;
	m_TeamIconsDead[MAP_ICON_HOSTAGE] = AddIconTexture( "sprites/player_hostage_dead" );
	m_TeamIconsOffscreen[MAP_ICON_HOSTAGE] = AddIconTexture( "sprites/player_hostage_offscreen" );
	m_TeamIconsDeadOffscreen[MAP_ICON_HOSTAGE] = AddIconTexture( "sprites/player_hostage_dead_offscreen" );

	m_bombIconPlanted = AddIconTexture( "sprites/bomb_planted" );
	m_bombIconDropped = AddIconTexture( "sprites/bomb_dropped" );
	m_bombIconCarried = AddIconTexture( "sprites/bomb_carried" );

	m_bombRingPlanted = AddIconTexture( "sprites/bomb_planted_ring" );
	m_bombRingDropped = AddIconTexture( "sprites/bomb_dropped_ring" );
	m_bombRingCarried = AddIconTexture( "sprites/bomb_carried_ring" );
	m_bombRingCarriedOffscreen = AddIconTexture( "sprites/bomb_carried_ring_offscreen" );

	m_hostageFollowing = AddIconTexture( "sprites/hostage_following" );
	m_hostageFollowingOffscreen = AddIconTexture( "sprites/hostage_following_offscreen" );
	m_playerFacing = AddIconTexture( "sprites/player_tick" );
	m_cameraIconFirst = AddIconTexture( "sprites/spectator_eye" );
	m_cameraIconThird = AddIconTexture( "sprites/spectator_3rdcam" );
	m_cameraIconFree = AddIconTexture( "sprites/spectator_freecam" );
	m_hostageRescueIcon = AddIconTexture( "sprites/objective_rescue" );;
	m_bombSiteIconA = AddIconTexture( "sprites/objective_site_a" );;
	m_bombSiteIconB = AddIconTexture( "sprites/objective_site_b" );;

	m_radioFlash = AddIconTexture("sprites/player_radio_ring");
	m_radioFlashOffscreen = AddIconTexture("sprites/player_radio_ring_offscreen");

	m_radarTint = AddIconTexture("sprites/radar_trans");

}

//-----------------------------------------------------------------------------
void CCSMapOverview::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings( scheme );

	m_hIconFont = scheme->GetFont( "DefaultSmall", true );
}

//-----------------------------------------------------------------------------
void CCSMapOverview::Update( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return;

	int team = pPlayer->GetTeamNumber();

	// if dead with fadetoblack on, we can't show anything
	if ( mp_fadetoblack.GetBool() && team > TEAM_SPECTATOR && !pPlayer->IsAlive() )
	{
		SetMode( MAP_MODE_OFF );
		return;
	}

	bool inRadarMode = (GetMode() == MAP_MODE_RADAR);
	int specmode = pPlayer->GetObserverMode();
	// if alive, we can only be in radar mode
	if( !inRadarMode  &&  pPlayer->IsAlive())
	{
		SetMode( MAP_MODE_RADAR );
		inRadarMode = true;
	}

	if( inRadarMode )
	{
		if( specmode > OBS_MODE_DEATHCAM )
		{
			// If fully dead, we don't want to be radar any more
			SetMode( m_playerPreferredMode );
			m_flChangeSpeed = 0;
		}
		else
		{
			SetFollowEntity(pPlayer->entindex());
			UpdatePlayers();
		}
	}

	BaseClass::Update();

	if ( GetSpectatorMode() == OBS_MODE_CHASE )
	{
		// Follow the local player in chase cam, so the map rotates using the local player's angles
		SetFollowEntity( pPlayer->entindex() );
	}
}

//-----------------------------------------------------------------------------
CCSMapOverview::CSMapPlayer_t* CCSMapOverview::GetCSInfoForPlayerIndex( int index )
{
	if ( index < 0 || index >= MAX_PLAYERS )
		return NULL;

	return &m_PlayersCSInfo[ index ];
}

//-----------------------------------------------------------------------------
CCSMapOverview::CSMapPlayer_t* CCSMapOverview::GetCSInfoForPlayer(MapPlayer_t *player)
{
	if( player == NULL )
		return NULL;

	for( int i = 0; i < MAX_PLAYERS; i++ )
	{
		if( &m_Players[i] == player )
			return &m_PlayersCSInfo[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
CCSMapOverview::CSMapPlayer_t* CCSMapOverview::GetCSInfoForHostage(MapPlayer_t *hostage)
{
	if( hostage == NULL )
		return NULL;

	for( int i = 0; i < MAX_HOSTAGES; i++ )
	{
		if( &m_Hostages[i] == hostage )
			return &m_HostagesCSInfo[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
#define TIME_SPOTS_STAY_SEEN (0.5f)
#define TIME_UNTIL_ENEMY_SEEN (0.5f)
// rules that define if you can see a player on the overview or not
bool CCSMapOverview::CanPlayerBeSeen( MapPlayer_t *player )
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	if (!localPlayer || !player )
		return false;

	CSMapPlayer_t *csPlayer = GetCSInfoForPlayer(player);
		
	if ( !csPlayer )
		return false;

	if( GetMode() == MAP_MODE_RADAR )
	{
		// This level will be for all the RadarMode thinking.  Base class will be the old way for the other modes.
		float now = gpGlobals->curtime;

		if( player->position == Vector(0,0,0) )
			return false; // Invalid guy.

		// draw special icons if within time
		if ( csPlayer->overrideExpirationTime != -1  &&  csPlayer->overrideExpirationTime > gpGlobals->curtime )
			return true;

		// otherwise, not dead people
		if( player->health <= 0 )
			return false;

		if( localPlayer->GetTeamNumber() == player->team )
			return true;// always yes for teammates.

		// and a living enemy needs to have been seen recently, and have been for a while
		if( csPlayer->timeLastSeen != -1  
			&& ( now - csPlayer->timeLastSeen < TIME_SPOTS_STAY_SEEN ) 
			&& ( now - csPlayer->timeFirstSeen > TIME_UNTIL_ENEMY_SEEN )
			)
			return true;

		return false;
	}
	else if( player->health <= 0 )
	{
		// Have to be under the overriden icon time to draw when dead.
		if ( csPlayer->overrideExpirationTime == -1  ||  csPlayer->overrideExpirationTime <= gpGlobals->curtime )
			return false;
	}
	
	return BaseClass::CanPlayerBeSeen(player);
}

bool CCSMapOverview::CanHostageBeSeen( MapPlayer_t *hostage )
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !localPlayer || !hostage )
		return false;


	CSMapPlayer_t *csHostage = GetCSInfoForHostage(hostage);

	if ( !csHostage )
		return false;

	if( GetMode() == MAP_MODE_RADAR )
	{
		// This level will be for all the RadarMode thinking.  Base class will be the old way for the other modes.
		float now = gpGlobals->curtime;

		if( hostage->position == Vector(0,0,0) )
			return false; // Invalid guy.

		// draw special icons if within time
		if ( csHostage->overrideExpirationTime != -1  &&  csHostage->overrideExpirationTime > gpGlobals->curtime )
			return true;

		// otherwise, not dead people
		if( hostage->health <= 0 )
			return false;

		if( localPlayer->GetTeamNumber() == hostage->team )
			return true;// always yes for teammates.

		// and a living enemy needs to have been seen recently
		if( csHostage->timeLastSeen != -1  &&  now - csHostage->timeLastSeen < TIME_SPOTS_STAY_SEEN )
			return true;

		return false;
	}
	else if( hostage->health <= 0 )
	{
		// Have to be under the overriden icon time to draw when dead.
		if ( csHostage->overrideExpirationTime == -1  ||  csHostage->overrideExpirationTime <= gpGlobals->curtime )
			return false;
	}

	return BaseClass::CanPlayerBeSeen(hostage);
}

CCSMapOverview::CCSMapOverview( const char *pElementName ) : BaseClass( pElementName )
{
	m_nRadarMapTextureID = -1;

	g_pMapOverview = this;  // for cvars access etc

	// restore non-radar modes
	switch ( overview_preferred_mode.GetInt() )
	{
	case MAP_MODE_INSET:
		m_playerPreferredMode = MAP_MODE_INSET;
		break;

	case MAP_MODE_FULL:
		m_playerPreferredMode = MAP_MODE_FULL;
		break;

	default:
		m_playerPreferredMode = MAP_MODE_OFF;
		break;
	}
}

void CCSMapOverview::Init( void )
{
	BaseClass::Init();

	// register for events as client listener
	ListenForGameEvent( "hostage_killed" );
	ListenForGameEvent( "hostage_rescued" );
	ListenForGameEvent( "bomb_defused" );
	ListenForGameEvent( "bomb_exploded" );
}

CCSMapOverview::~CCSMapOverview()
{
	g_pMapOverview = NULL;

	//TODO release Textures ? clear lists
}

void CCSMapOverview::UpdatePlayers()
{
	if( !m_goalIconsLoaded )
		UpdateGoalIcons();

	UpdateHostages();// Update before players so players can spot them

	UpdateBomb();// Before players so player can properly spot where it is in this update

	UpdateFlashes();

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
		return;

	float now = gpGlobals->curtime;

	CBasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if( localPlayer == NULL )
		return;

	MapPlayer_t *localMapPlayer = GetPlayerByUserID(localPlayer->GetUserID());
	if( localMapPlayer == NULL )
		return;

	for ( int i = 1; i<= gpGlobals->maxClients; i++)
	{
		MapPlayer_t *player = &m_Players[i-1];
		CSMapPlayer_t *playerCS = GetCSInfoForPlayerIndex(i-1);

		if ( !playerCS )
			continue;

		// update from global player resources
		if ( pCSPR->IsConnected(i) )
		{
			player->health = pCSPR->GetHealth( i );

			if ( !pCSPR->IsAlive( i ) )
			{
				// Safety actually happens after a TKPunish.
				player->health = 0;
				playerCS->isDead = true;
			}

			if ( player->team != pCSPR->GetTeam( i ) )
			{
				player->team = pCSPR->GetTeam( i );

				if( player == localMapPlayer )
					player->icon = m_TeamIconsSelf[ GetIconNumberFromTeamNumber(player->team) ];
				else
					player->icon = m_TeamIcons[ GetIconNumberFromTeamNumber(player->team) ];

				player->color = m_TeamColors[ GetIconNumberFromTeamNumber(player->team) ];
			}
		}

		Vector position = player->position;
		QAngle angles = player->angle;
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			// update position of active players in our PVS
			position = pPlayer->EyePosition();
			angles = pPlayer->EyeAngles();

			SetPlayerPositions( i-1, position, angles );
		}
	}

	if ( GetMode() == MAP_MODE_RADAR )
	{
		// Check for teammates spotting the bomb
		if ( m_bomb.state != CSMapBomb_t::BOMB_INVALID && pCSPR->IsBombSpotted() )
		{
			SetBombSeen( true );
		}

		// Check for teammates spotting enemy players
		for ( int i = 1; i<= gpGlobals->maxClients; ++i )
		{
			if ( !pCSPR->IsConnected(i) )
				continue;

			if ( !pCSPR->IsAlive(i) )
				continue;

			if ( pCSPR->GetTeam(i) == localMapPlayer->team )
				continue;

			if ( pCSPR->IsPlayerSpotted(i) )
			{
				SetPlayerSeen( i-1 );

				MapPlayer_t *baseEnemy = &m_Players[i-1];
				if( baseEnemy->health > 0 )
				{
					// They were just seen, so if they are alive get rid of their "last known" icon
					CSMapPlayer_t *csEnemy = GetCSInfoForPlayerIndex(i-1);

					if ( csEnemy )
					{
						csEnemy->overrideIcon = -1;
						csEnemy->overrideIconOffscreen = -1;
						csEnemy->overridePosition = Vector(0, 0, 0);
						csEnemy->overrideAngle = QAngle(0, 0, 0);
						csEnemy->overrideFadeTime = -1;
						csEnemy->overrideExpirationTime = -1;
					}
				}
			}
		}

		for( int i = 1; i<= gpGlobals->maxClients; i++ )
		{
			MapPlayer_t *player = &m_Players[i-1];
			CSMapPlayer_t *playerCS = GetCSInfoForPlayerIndex(i-1);
			C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( !pPlayer || !playerCS )
				continue;

			float timeSinceLastSeen = now - playerCS->timeLastSeen;
			if( timeSinceLastSeen < 0.25f )
				continue;
			if( player->health <= 0 )
				continue;// We don't need to spot dead guys, since they always show
			if( player->team == localMapPlayer->team )
				continue;// We don't need to spot our own guys

			// Now that everyone has had a say on people they can see for us, go through and handle baddies that can no longer be seen.
			if( playerCS->timeLastSeen != now  &&  player->health > 0 )
			{
				// We are not seen now, but if we were seen recently (and for long enough),
				// put up a "last known" icon and clear timelastseen
				// if they are alive.  Death icon is more important, which is why the health check above.
				if( timeSinceLastSeen < 0.5f  && ( playerCS->timeLastSeen != -1 ) )
				{
					if( now - playerCS->timeFirstSeen > TIME_UNTIL_ENEMY_SEEN )
					{
						playerCS->overrideIcon = m_TeamIcons[ GetIconNumberFromTeamNumber(player->team) ];;
						playerCS->overrideIconOffscreen = m_TeamIconsOffscreen[ GetIconNumberFromTeamNumber(player->team) ];
						playerCS->overridePosition = player->position;
						playerCS->overrideFadeTime = -1;
						playerCS->overrideExpirationTime = now + LAST_SEEN_ICON_DURATION;
						playerCS->overrideAngle = player->angle;
					}
					playerCS->timeLastSeen = -1;
					playerCS->timeFirstSeen = -1;
				}
			}
		}
	}
}

void CCSMapOverview::UpdateHostages()
{
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
		return;

	for( int i=0; i < MAX_HOSTAGES; i++ )
	{
		if( pCSPR->IsHostageAlive( i ) )
		{
			MapPlayer_t *hostage = GetHostageByEntityID( pCSPR->GetHostageEntityID(i) );
			if( hostage == NULL )
				hostage = &m_Hostages[i];// Don't have entry yet, so need one.  This'll only happen once, at start of map

			CSMapPlayer_t *hostageCS = GetCSInfoForHostage(hostage);

			if ( !hostageCS )
				return;

			if( !hostageCS->isDead )
			{
				hostage->index = pCSPR->GetHostageEntityID(i);
				hostage->position = pCSPR->GetHostagePosition( i );
				hostage->health = 100; // Hostages don't have health available from pCSPR.
				hostage->angle = QAngle(0, 0, 0); // No facing, like no health
				hostage->team = TEAM_CT; // CT in terms of who sees them
				hostage->icon = m_TeamIcons[ MAP_ICON_HOSTAGE ]; // But hostage for icon.
				hostage->color = m_TeamColors[ MAP_ICON_HOSTAGE ];
				hostageCS->isHostage = true;

//				engine->Con_NPrintf( i + 15, "ID:%d Pos:(%.0f,%.0f,%.0f)", hostage->index, hostage->position.x, hostage->position.y, hostage->position.z );
			}
			else
			{
//				engine->Con_NPrintf( i + 15, "Mostly Dead" );
			}
		}
		else
		{
//			engine->Con_NPrintf( i + 15, "Dead" );
		}
	}
}

void CCSMapOverview::UpdateBomb()
{
	if( m_bomb.state == CSMapBomb_s::BOMB_GONE )
		return;// no more updates until map restart

	float now = gpGlobals->curtime;

	// First, decide if it has been too long since the bomb has been seen to clear visibility timers.
	if( now - m_bomb.timeLastSeen >= TIME_SPOTS_STAY_SEEN  &&  m_bomb.timeFirstSeen != -1 )
	{
		SetBombSeen( false );
	}

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
		return;

	float biggestRadius = 0, smallestRadius = 0;
	if ( g_PlantedC4s.Count() > 0 )
	{
		// bomb is planted
		C_PlantedC4 *pC4 = g_PlantedC4s[0];

		if( pC4->IsBombActive() )
		{
			m_bomb.position = pC4->GetAbsOrigin();
			m_bomb.state = CSMapBomb_t::BOMB_PLANTED;
			m_bomb.ringTravelTime = 3.0f;
			smallestRadius = m_flIconSize;
			biggestRadius = m_flIconSize * 15.0f;
		}
		else
		{
			// Defused or exploded
			m_bomb.state = CSMapBomb_t::BOMB_GONE;
		}
	}
	else if ( pCSPR->HasC4( 0 ) )
	{
		// bomb dropped 
		Vector pos = pCSPR->GetC4Postion();

		if ( pos.x != 0 || pos.y != 0 || pos.z != 0 )
		{
			m_bomb.position = pos;
			m_bomb.state = CSMapBomb_t::BOMB_DROPPED;
			m_bomb.ringTravelTime = 6.0f;
			smallestRadius = m_flIconSize;
			biggestRadius = m_flIconSize * 10.0f;
		}
		else
		{
			m_bomb.state = CSMapBomb_t::BOMB_INVALID;
			//Not a bomb map.  Man, what a weird system instead of IsBombMap.  If nobody has it, and it isn't on the ground, then it isn't a bomb map.
		}
	}
	else
	{
		for( int i = 1; i<= gpGlobals->maxClients; i++ )
		{
			if( pCSPR->HasC4(i) )
			{
				C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if( pPlayer == NULL  ||  pPlayer->IsDormant() )
				{
					// Dormant or no player means we are relying on RadarUpdate messages so we can trust the MapOverview position.
					MapPlayer_t *player = &m_Players[i-1];
					m_bomb.position = player->position;
				}
				else
				{
					// Update players is about to put this Real Data in the player sturct, and we don't want the bomb pos lagged one update behind
					m_bomb.position = pPlayer->GetAbsOrigin();
				}

				m_bomb.state = CSMapBomb_t::BOMB_CARRIED;
				m_bomb.ringTravelTime = 0;
				smallestRadius = m_flIconSize * 1.2f;
				biggestRadius = m_flIconSize * 1.2f;
				break;
			}
		}
	}

	int alpha = GetMasterAlpha();

	if( m_bomb.currentRingRadius == m_bomb.maxRingRadius  ||  m_bomb.ringTravelTime == 0 )
	{
		m_bomb.currentRingRadius = smallestRadius;
		m_bomb.maxRingRadius = biggestRadius;
		m_bomb.currentRingAlpha = alpha;
	}
	else
	{
		m_bomb.currentRingRadius += (m_bomb.maxRingRadius - m_flIconSize) * gpGlobals->frametime / m_bomb.ringTravelTime;
		m_bomb.currentRingRadius = MIN( m_bomb.currentRingRadius, m_bomb.maxRingRadius );
		m_bomb.currentRingAlpha = (alpha - 55) * ((m_bomb.maxRingRadius - m_bomb.currentRingRadius) / (m_bomb.maxRingRadius - m_flIconSize)) + 55;
	}
}

bool CCSMapOverview::ShouldDraw( void )
{
	int alpha = GetMasterAlpha();
	if( alpha == 0 )
		return false;// we have been set to fully transparent

	//=============================================================================
	// HPE_BEGIN:
	// [smessick] Turn off large map display when in freezecam.
	//=============================================================================
	if ( IsInFreezeCam() && GetMode() == MAP_MODE_FULL )
	{
		return false;
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	float now = gpGlobals->curtime;
	if( GetMode() == MAP_MODE_RADAR )
	{
		if ( (GET_HUDELEMENT( CHudRadar ))->ShouldDraw() == false )
		{
			return false; 
		}

		// We have to be alive and not blind to draw in this mode.
		C_CSPlayer *pCSPlayer = C_CSPlayer::GetLocalCSPlayer();
		if( !pCSPlayer || pCSPlayer->GetObserverMode() == OBS_MODE_DEATHCAM ) 
		{
			return false;
		}
		else if (pCSPlayer->m_flFlashBangTime > now)
		{
			return false;
		}
	}

	return BaseClass::ShouldDraw();
}

CCSMapOverview::MapPlayer_t* CCSMapOverview::GetHostageByEntityID( int entityID )
{
	for (int i=0; i<MAX_HOSTAGES; i++)
	{
		if ( m_Hostages[i].index == entityID )
			return &m_Hostages[i];
	}

	return NULL;
}

CCSMapOverview::MapPlayer_t* CCSMapOverview::GetPlayerByEntityID( int entityID )
{
	C_BasePlayer *realPlayer = UTIL_PlayerByIndex(entityID);

	if( realPlayer == NULL )
		return NULL;

	for (int i=0; i<MAX_PLAYERS; i++)
	{
		MapPlayer_t *player = &m_Players[i];

		if ( player->userid == realPlayer->GetUserID() )
			return player;
	}

	return NULL;
}

#define BORDER_WIDTH 4
bool CCSMapOverview::AdjustPointToPanel(Vector2D *pos)
{
	if( pos == NULL )
		return false;

	int mapInset = GetBorderSize();// This gives us the amount inside the panel that the background is drawing.
	if( mapInset != 0 )
		mapInset += BORDER_WIDTH; // And this gives us the border inside the map edge to give us room for offscreen icons.

	int x,y,w,t;

	//MapTpPanel has already offset the x and y.  That's why we use 0 for left and top.
	GetBounds( x,y,w,t );

	bool madeChange = false;
	if( pos->x < mapInset )
	{
		pos->x = mapInset;
		madeChange = true;
	}
	if( pos->x > w - mapInset )
	{
		pos->x = w - mapInset;
		madeChange = true;
	}
	if( pos->y < mapInset )
	{
		pos->y = mapInset;
		madeChange = true;
	}
	if( pos->y > t - mapInset )
	{
		pos->y = t - mapInset;
		madeChange = true;
	}

	return madeChange;
}

void CCSMapOverview::DrawMapTexture()
{
	int alpha = GetMasterAlpha();

	if( GetMode() == MAP_MODE_FULL )
		SetBgColor( Color(0,0,0,0) );// no background in big mode
	else
		SetBgColor( Color(0,0,0,alpha * 0.5) );

	int textureIDToUse = m_nMapTextureID;
	bool foundRadarVersion = false;
	if( m_nRadarMapTextureID != -1 && GetMode() == MAP_MODE_RADAR )
	{
		textureIDToUse = m_nRadarMapTextureID;
		foundRadarVersion = true;
	}

	int mapInset = GetBorderSize();
	int pwidth, pheight; 
	GetSize(pwidth, pheight);

	if ( textureIDToUse > 0 )
	{
		// We are drawing to the whole panel with a little border
		Vector2D panelTL = Vector2D( mapInset, mapInset );
		Vector2D panelTR = Vector2D( pwidth - mapInset, mapInset );
		Vector2D panelBR = Vector2D( pwidth - mapInset, pheight - mapInset );
		Vector2D panelBL = Vector2D( mapInset, pheight - mapInset );

		// So where are those four points on the great big map?
		Vector2D textureTL = PanelToMap( panelTL );// The top left corner of the display is where on the master map?
		textureTL /= OVERVIEW_MAP_SIZE;// Texture Vec2D is 0 to 1
		Vector2D textureTR = PanelToMap( panelTR );
		textureTR /= OVERVIEW_MAP_SIZE;
		Vector2D textureBR = PanelToMap( panelBR );
		textureBR /= OVERVIEW_MAP_SIZE;
		Vector2D textureBL = PanelToMap( panelBL );
		textureBL /= OVERVIEW_MAP_SIZE;

		Vertex_t points[4] =
		{
			// To draw a textured polygon, the first column is where you want to draw (to), and the second is what you want to draw (from).
			// We want to draw to the panel (pulled in for a border), and we want to draw the part of the map texture that should be seen.
			// First column is in panel coords, second column is in 0-1 texture coords
			Vertex_t( panelTL, textureTL ),
			Vertex_t( panelTR, textureTR ),
			Vertex_t( panelBR, textureBR ),
			Vertex_t( panelBL, textureBL )
		};

		surface()->DrawSetColor( 255, 255, 255, alpha );
		surface()->DrawSetTexture( textureIDToUse );
		surface()->DrawTexturedPolygon( 4, points );
	}

	// If we didn't find the greenscale version of the map, then at least do a tint.
	if( !foundRadarVersion && GetMode() == MAP_MODE_RADAR )
	{
		surface()->DrawSetColor( 0,255,0, alpha / 4 );
		surface()->DrawFilledRect( mapInset, mapInset, m_vSize.x - 1 - mapInset, m_vSize.y - 1 - mapInset );
	}
}

void CCSMapOverview::DrawBomb()
{
    if( m_bomb.state == CSMapBomb_t::BOMB_INVALID )
		return;

	CBasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if( localPlayer == NULL )
		return;
	MapPlayer_t *localMapPlayer = GetPlayerByUserID(localPlayer->GetUserID());
	if( localMapPlayer == NULL )
		return;
	float now = gpGlobals->curtime;

	if( localMapPlayer->team == TEAM_CT )
	{
		// CT's only get to see it if...

		if( localMapPlayer->health <= 0 )
		{
			if ( mp_forcecamera.GetInt() != OBS_ALLOW_ALL )
				return;// They're dead and spectating isn't restricted 
		}
		else if( (m_bomb.timeLastSeen == -1)  
			||  ( now - m_bomb.timeLastSeen >= TIME_SPOTS_STAY_SEEN ) 
			||  ( now - m_bomb.timeFirstSeen < TIME_UNTIL_ENEMY_SEEN ) 
			)
		{
			return;// It's in view
		}
	}
	// else if you aren't CT you can always see it

	int bombIcon;
	int bombRing;
	int bombRingOffscreen;
	switch(m_bomb.state) 
	{
		case CSMapBomb_t::BOMB_DROPPED:
		{
			bombIcon = m_bombIconDropped;
			bombRing = m_bombRingDropped;
			bombRingOffscreen = m_bombRingDropped;
			break;
		}
		case CSMapBomb_t::BOMB_CARRIED:
		{
			bombIcon = m_bombIconCarried;
			bombRing = m_bombRingCarried;
			bombRingOffscreen = m_bombRingCarriedOffscreen;
			break;
		}
		case CSMapBomb_t::BOMB_PLANTED:
		{
			bombIcon = m_bombIconPlanted;
			bombRing = m_bombRingPlanted;
			bombRingOffscreen = m_bombRingPlanted;
			break;
		}
		case CSMapBomb_t::BOMB_GONE:
		{
			bombIcon = m_bombIconPlanted;
			bombRing = m_bombRingPlanted;
			bombRingOffscreen = m_bombRingPlanted;
			break;
		}
	default:
		return;
	}

	int alpha = 255;

	if( m_bomb.timeGone != -1  &&  m_bomb.timeFade <= gpGlobals->curtime )
		alpha *= 1 - ( (float)(gpGlobals->curtime - m_bomb.timeFade) / (float)(m_bomb.timeGone - m_bomb.timeFade) );

	if( m_bomb.state != CSMapBomb_t::BOMB_GONE )
		DrawIconCS(bombRing, bombRingOffscreen, m_bomb.position, m_bomb.currentRingRadius, 0, m_bomb.currentRingAlpha);
	DrawIconCS(bombIcon, bombIcon, m_bomb.position, m_flIconSize, 0, alpha);
}

bool CCSMapOverview::DrawIconCS( int textureID, int offscreenTextureID, Vector pos, float scale, float angle, int alpha, bool allowRotation, const char *text, Color *textColor, float status, Color *statusColor )
{
	if( GetMode() == MAP_MODE_RADAR  &&  cl_radaralpha.GetInt() == 0 )
		return false;

	if( alpha <= 0 )
		return false;

	Vector2D pospanel = WorldToMap( pos );
	pospanel = MapToPanel( pospanel );

	int idToUse = textureID;
	float angleToUse = angle;

	Vector2D oldPos = pospanel;
	Vector2D adjustment(0,0);
	if( AdjustPointToPanel( &pospanel ) )
	{
		if( offscreenTextureID == -1 )
			return false; //Doesn't want to draw if off screen.

		// Move it in to on panel, and change the icon.
		idToUse = offscreenTextureID;
		// And point towards the original spot
		adjustment = Vector2D(pospanel.x - oldPos.x, pospanel.y - oldPos.y);
		QAngle adjustmentAngles;
		Vector adjustment3D(adjustment.x, -adjustment.y, 0); // Y gets flipped in WorldToMap
		VectorAngles(adjustment3D, adjustmentAngles) ;
		if( allowRotation )
		{
			// Some icons don't want to rotate even when off radar
			angleToUse = adjustmentAngles[YAW];

			// And the angle needs to be in world space, not panel space.
			if( m_bFollowAngle )
			{
				angleToUse += m_fViewAngle;
			}
			else 
			{
				if ( m_bRotateMap )
					angleToUse += 180.0f;
				else
					angleToUse += 90.0f;
			}
		}

		// Don't draw names for icons that are offscreen (bunches up and looks bad)
		text = NULL;
	}

	int d = GetPixelOffset( scale );

	Vector offset;

	offset.x = -scale;	offset.y = scale;
	VectorYawRotate( offset, angleToUse, offset );
	Vector2D pos1 = WorldToMap( pos + offset );
	Vector2D pos1Panel = MapToPanel(pos1);
	pos1Panel.x += adjustment.x;
	pos1Panel.y += adjustment.y;

	offset.x = scale;	offset.y = scale;
	VectorYawRotate( offset, angleToUse, offset );
	Vector2D pos2 = WorldToMap( pos + offset );
	Vector2D pos2Panel = MapToPanel(pos2);
	pos2Panel.x += adjustment.x;
	pos2Panel.y += adjustment.y;

	offset.x = scale;	offset.y = -scale;
	VectorYawRotate( offset, angleToUse, offset );
	Vector2D pos3 = WorldToMap( pos + offset );
	Vector2D pos3Panel = MapToPanel(pos3);
	pos3Panel.x += adjustment.x;
	pos3Panel.y += adjustment.y;

	offset.x = -scale;	offset.y = -scale;
	VectorYawRotate( offset, angleToUse, offset );
	Vector2D pos4 = WorldToMap( pos + offset );
	Vector2D pos4Panel = MapToPanel(pos4);
	pos4Panel.x += adjustment.x;
	pos4Panel.y += adjustment.y;

	Vertex_t points[4] =
	{
		Vertex_t( pos1Panel, Vector2D(0,0) ),
			Vertex_t( pos2Panel, Vector2D(1,0) ),
			Vertex_t( pos3Panel, Vector2D(1,1) ),
			Vertex_t( pos4Panel, Vector2D(0,1) )
	};

	surface()->DrawSetColor( 255, 255, 255, alpha );
	surface()->DrawSetTexture( idToUse );
	surface()->DrawTexturedPolygon( 4, points );

	pospanel.y += d + 4;

	if ( status >=0.0f  && status <= 1.0f && statusColor )
	{
		// health bar is 50x3 pixels
		surface()->DrawSetColor( 0,0,0,255 );
		surface()->DrawFilledRect( pospanel.x-d, pospanel.y-1, pospanel.x+d, pospanel.y+1 );

		int length = (float)(d*2)*status;
		surface()->DrawSetColor( statusColor->r(), statusColor->g(), statusColor->b(), 255 );
		surface()->DrawFilledRect( pospanel.x-d, pospanel.y-1, pospanel.x-d+length, pospanel.y+1 );

		pospanel.y += 3;
	}

	if ( text && textColor )
	{
		wchar_t iconText[ MAX_PLAYER_NAME_LENGTH*2 ];

		g_pVGuiLocalize->ConvertANSIToUnicode( text, iconText, sizeof( iconText ) );

		int wide, tall;
		surface()->GetTextSize( m_hIconFont, iconText, wide, tall );

		int x = pospanel.x-(wide/2);
		int y = pospanel.y;

		// draw black shadow text
		surface()->DrawSetTextColor( 0, 0, 0, 255 );
		surface()->DrawSetTextPos( x+1, y );
		surface()->DrawPrintText( iconText, wcslen(iconText) );

		// draw name in color 
		surface()->DrawSetTextColor( textColor->r(), textColor->g(), textColor->b(), 255 );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( iconText, wcslen(iconText) );
	}

	return true;
}

void CCSMapOverview::DrawMapPlayers()
{
	DrawGoalIcons();
	DrawHostages();

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	surface()->DrawSetTextFont( m_hIconFont );

	Color colorGreen( 0, 255, 0, 255 );	// health bar color
	CBasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	for (int i=0; i < MAX_PLAYERS; i++)
	{
		int alpha = 255;
		MapPlayer_t *player = &m_Players[i];
		CSMapPlayer_t *playerCS = GetCSInfoForPlayerIndex(i);

		if ( !playerCS )
			continue;

		if ( !CanPlayerBeSeen( player ) )
			continue;

		float status = -1;
		const char *name = NULL;

		if ( m_bShowNames && CanPlayerNameBeSeen( player ) )
			name = player->name;

		if ( m_bShowHealth && CanPlayerHealthBeSeen( player ) )
			status = player->health/100.0f;

		// Now draw them
		if( playerCS->overrideExpirationTime > gpGlobals->curtime )// If dead, an X, if alive, an alpha'd normal icon
		{
			int alphaToUse = alpha;
			if( playerCS->overrideFadeTime != -1 && playerCS->overrideFadeTime <= gpGlobals->curtime )
			{
				// Fade linearly from fade start to disappear
				alphaToUse *= 1 - (float)(gpGlobals->curtime - playerCS->overrideFadeTime) / (float)(playerCS->overrideExpirationTime - playerCS->overrideFadeTime);
			}

			DrawIconCS( playerCS->overrideIcon, playerCS->overrideIconOffscreen, playerCS->overridePosition, m_flIconSize * 1.1f, GetViewAngle(), player->health > 0 ? alphaToUse / 2 : alphaToUse, true, name, &player->color, -1, &colorGreen );
			if( player->health > 0 )
				DrawIconCS( m_playerFacing, -1, playerCS->overridePosition, m_flIconSize * 1.1f, playerCS->overrideAngle[YAW], player->health > 0 ? alphaToUse / 2 : alphaToUse, true, name, &player->color, status, &colorGreen );
		}
		else
		{
			float zDifference = 0;
			if( localPlayer )
			{	
				if( (localPlayer->GetObserverMode() != OBS_MODE_NONE) && localPlayer->GetObserverTarget() )
					zDifference = player->position.z - localPlayer->GetObserverTarget()->GetAbsOrigin().z;
				else
					zDifference = player->position.z - localPlayer->GetAbsOrigin().z;
			}

			float sizeForRing = m_flIconSize * 1.4f;
			float sizeForPlayer = m_flIconSize * 1.1f; // The 1.1 is because the player dots are shrunken a little, so their facing pip can have some space to live
			if ( zDifference > DIFFERENCE_THRESHOLD )
			{
				// A dot above is bigger and a little fuzzy now.
				sizeForRing *= 1.4f;
				sizeForPlayer *= 1.4f;
				alpha *= 0.5f;
			}
			else if ( zDifference < -DIFFERENCE_THRESHOLD )
			{
				// A dot below is smaller.
				sizeForRing *= 0.7f;
				sizeForPlayer *= 0.7f;
			}

			bool showTalkRing = localPlayer && (localPlayer->GetTeamNumber() == player->team || localPlayer->GetTeamNumber() == TEAM_SPECTATOR);

			if( showTalkRing && playerCS->currentFlashAlpha > 0 )// Flash type
			{
				// Make them flash a halo
				DrawIconCS(m_radioFlash, m_radioFlashOffscreen, player->position, sizeForRing, player->angle[YAW], playerCS->currentFlashAlpha);
			}
			else if( showTalkRing && pCSPR->IsAlive( i + 1 ) && GetClientVoiceMgr()->IsPlayerSpeaking( i + 1) ) // Or solid on type
			{
				// Make them show a halo
				DrawIconCS(m_radioFlash, m_radioFlashOffscreen, player->position, sizeForRing, player->angle[YAW], 255);
			}
			
			bool doingLocalPlayer = GetPlayerByUserID(localPlayer->GetUserID()) == player;
			float angleForPlayer = GetViewAngle();

			if( doingLocalPlayer )
			{
				sizeForPlayer *= 4.0f; // The self icon is really big since it has a camera view cone attached.
				angleForPlayer = player->angle[YAW];// And, the self icon now rotates, natch.
			}

			int offscreenIcon = m_TeamIconsOffscreen[GetIconNumberFromTeamNumber(player->team)];
			DrawIconCS( player->icon, offscreenIcon, player->position, sizeForPlayer, angleForPlayer, alpha, true, name, &player->color, status, &colorGreen );
			if( !doingLocalPlayer )
			{
				// Draw the facing for everyone but the local player.
				if( player->health > 0 )
					DrawIconCS( m_playerFacing, -1, player->position, sizeForPlayer, player->angle[YAW], alpha, true, name, &player->color, status, &colorGreen );
			}
		}
	}

	DrawBomb();// After players so it can draw on top
}

void CCSMapOverview::DrawHostages()
{
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
		return;

	surface()->DrawSetTextFont( m_hIconFont );

	Color colorGreen( 0, 255, 0, 255 );	// health bar color
	CBasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	for (int i=0; i < MAX_HOSTAGES; i++)
	{
		int alpha = 255;
		MapPlayer_t *hostage = GetHostageByEntityID( pCSPR->GetHostageEntityID(i) );
		if( hostage == NULL )
			continue;

		CSMapPlayer_t *hostageCS = GetCSInfoForHostage(hostage);

		if ( !hostageCS )
			continue;

		if ( !CanHostageBeSeen( hostage ) )
		{
//			engine->Con_NPrintf( i + 30, "Can't be seen." );
			continue;
		}

		float status = -1;
		const char *name = NULL;

		if( hostageCS->overrideExpirationTime > gpGlobals->curtime )// If dead, an X, if alive, an alpha'd normal icon
		{
//			engine->Con_NPrintf( i + 30, "ID:%d Override Pos:(%.0f,%.0f,%.0f)", hostage->index, hostageCS->overridePosition.x, hostageCS->overridePosition.y, hostageCS->overridePosition.z );
			int alphaToUse = alpha;
			if( hostageCS->overrideFadeTime != -1 && hostageCS->overrideFadeTime <= gpGlobals->curtime )
			{
				// Fade linearly from fade start to disappear
				alphaToUse *= 1 - (float)(gpGlobals->curtime - hostageCS->overrideFadeTime) / (float)(hostageCS->overrideExpirationTime - hostageCS->overrideFadeTime);
			}

			DrawIconCS( hostageCS->overrideIcon, hostageCS->overrideIconOffscreen, hostageCS->overridePosition, m_flIconSize, hostageCS->overrideAngle[YAW], hostage->health > 0 ? alphaToUse / 2 : alphaToUse, true, name, &hostage->color, status, &colorGreen );
		}
		else
		{
			if( localPlayer && localPlayer->GetTeamNumber() == hostage->team && hostageCS->currentFlashAlpha > 0 )
			{
				// Make them flash a halo
				DrawIconCS(m_radioFlash, m_radioFlashOffscreen, hostage->position, m_flIconSize * 1.4f, hostage->angle[YAW], hostageCS->currentFlashAlpha);
			}

//			engine->Con_NPrintf( i + 30, "ID:%d Pos:(%.0f,%.0f,%.0f)", hostage->index, hostage->position.x, hostage->position.y, hostage->position.z );
			int normalIcon, offscreenIcon;
			float zDifference = 0;
			if( localPlayer )
			{	
				if( (localPlayer->GetObserverMode() != OBS_MODE_NONE) && localPlayer->GetObserverTarget() )
					zDifference = hostage->position.z - localPlayer->GetObserverTarget()->GetAbsOrigin().z;
				else
					zDifference = hostage->position.z - localPlayer->GetAbsOrigin().z;
			}

			float sizeForHostage = m_flIconSize;
			if( zDifference > DIFFERENCE_THRESHOLD )
			{
				// A dot above is bigger and a little fuzzy now.
				sizeForHostage = m_flIconSize * 1.5f;
				alpha *= 0.5f;
			}
			else if( zDifference < -DIFFERENCE_THRESHOLD )
			{
				// A dot below is smaller.
				sizeForHostage = m_flIconSize * 0.6f;
			}

			normalIcon = hostage->icon;
			offscreenIcon = m_TeamIconsOffscreen[ MAP_ICON_HOSTAGE ];
			DrawIconCS( normalIcon, offscreenIcon, hostage->position, sizeForHostage, GetViewAngle(), alpha, true, name, &hostage->color, status, &colorGreen );

			if( pCSPR->IsHostageFollowingSomeone( i ) )
			{
				// If they are following a CT, then give them a little extra symbol to show it.
				DrawIconCS( m_hostageFollowing, m_hostageFollowingOffscreen, hostage->position, sizeForHostage, hostage->angle[YAW], alpha );
			}
		}
	}
}
void CCSMapOverview::SetMap(const char * levelname)
{
	BaseClass::SetMap(levelname);

	int wide, tall;
	surface()->DrawGetTextureSize( m_nMapTextureID, wide, tall );
	if( wide == 0 && tall == 0 )
	{
		m_nMapTextureID = -1;
		m_nRadarMapTextureID = -1;
		return;// No map image, so no radar image
	}

	char radarFileName[MAX_PATH];
	Q_snprintf(radarFileName, MAX_PATH, "%s_radar", m_MapKeyValues->GetString("material"));
	m_nRadarMapTextureID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nRadarMapTextureID, radarFileName, true, false);
	int radarWide = -1;
	int radarTall = -1;
	surface()->DrawGetTextureSize(m_nRadarMapTextureID, radarWide, radarTall);
	bool radarTextureFound = false;
	if( radarWide == wide  &&  radarTall == tall )
	{
		// Unbelievable that these is no failure return from SetTextureFile, and not
		// even a ValidTexture check on the ID.  So I can check if it is different from
		// the original.  It'll be a 32x32 default if not present.
		radarTextureFound = true;
	}

	if( !radarTextureFound )
	{
		if( !CreateRadarImage(m_MapKeyValues->GetString("material"), radarFileName) )
			m_nRadarMapTextureID = -1;
	}

	ClearGoalIcons();
}

bool CCSMapOverview::CreateRadarImage(const char *mapName, const char * radarFileName)
{
#ifdef GENERATE_RADAR_FILE
	char fullFileName[MAX_PATH];
	Q_snprintf(fullFileName, MAX_PATH, "materials/%s.vtf", mapName);
	char fullRadarFileName[MAX_PATH];
	Q_snprintf(fullRadarFileName, MAX_PATH, "materials/%s.vtf", radarFileName);

	// Not found, so try to make one
	FileHandle_t fp;
	fp = ::filesystem->Open( fullFileName, "rb" );
	if( !fp )
	{
		return false;
	}
	::filesystem->Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	int srcVTFLength = ::filesystem->Tell( fp );
	::filesystem->Seek( fp, 0, FILESYSTEM_SEEK_HEAD );

	CUtlBuffer buf;
	buf.EnsureCapacity( srcVTFLength );
	int overviewMapBytesRead = ::filesystem->Read( buf.Base(), srcVTFLength, fp );
	::filesystem->Close( fp );

	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );// Need to set these explicitly since ->Read goes straight to memory and skips them.
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, overviewMapBytesRead );

	IVTFTexture *radarTexture = CreateVTFTexture();
	if (radarTexture->Unserialize(buf))
	{
		ImageFormat oldImageFormat = radarTexture->Format();
		radarTexture->ConvertImageFormat(IMAGE_FORMAT_RGBA8888, false);
		unsigned char *imageData = radarTexture->ImageData(0,0,0);
		int size = radarTexture->ComputeTotalSize(); // in bytes!
		unsigned char *pEnd = imageData + size;

		for( ; imageData < pEnd; imageData += 4 )
		{
			imageData[0] = 0; // R
			imageData[2] = 0; // B
		}

		radarTexture->ConvertImageFormat(oldImageFormat, false);

		buf.Clear();
		radarTexture->Serialize(buf);

		fp = ::filesystem->Open(fullRadarFileName, "wb");
		::filesystem->Write(buf.Base(), buf.TellPut(), fp);
		::filesystem->Close(fp);
		DestroyVTFTexture(radarTexture);
		buf.Purge();

		// And need a vmt file to go with it.
		char vmtFilename[MAX_PATH];
		Q_snprintf(vmtFilename, MAX_PATH, "%s", fullRadarFileName);
		char *extension = &vmtFilename[Q_strlen(vmtFilename) - 3];
		*extension++ = 'v';
		*extension++ = 'm';
		*extension++ = 't';
		*extension++ = '\0';
		fp = ::filesystem->Open(vmtFilename, "wt");
		::filesystem->Write("\"UnlitGeneric\"\n", 15, fp);
		::filesystem->Write("{\n", 2, fp);
		::filesystem->Write("\t\"$translucent\" \"1\"\n", 20, fp);
		::filesystem->Write("\t\"$basetexture\" \"", 17, fp);
		::filesystem->Write(radarFileName, Q_strlen(radarFileName), fp);
		::filesystem->Write("\"\n", 2, fp);
		::filesystem->Write("\t\"$vertexalpha\" \"1\"\n", 20, fp);
		::filesystem->Write("\t\"$vertexcolor\" \"1\"\n", 20, fp);
		::filesystem->Write("\t\"$no_fullbright\" \"1\"\n", 22, fp);
		::filesystem->Write("\t\"$ignorez\" \"1\"\n", 16, fp);
		::filesystem->Write("}\n", 2, fp);
		::filesystem->Close(fp);

		m_nRadarMapTextureID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_nRadarMapTextureID, radarFileName, true, true);
		return true;
	}
#endif
	return false;
}

void CCSMapOverview::ResetRound()
{
	BaseClass::ResetRound();

	for (int i=0; i<MAX_PLAYERS; i++)
	{
		CSMapPlayer_t *p = &m_PlayersCSInfo[i];

		p->isDead = false;

		p->overrideFadeTime = -1;
		p->overrideExpirationTime = -1;
		p->overrideIcon = -1;
		p->overrideIconOffscreen = -1;
		p->overridePosition = Vector( 0, 0, 0);
		p->overrideAngle = QAngle(0, 0, 0);

		p->timeLastSeen = -1;
		p->timeFirstSeen = -1;
		p->isHostage = false;

		p->flashUntilTime = -1;
		p->nextFlashPeakTime = -1;
		p->currentFlashAlpha = 0;
	}

	for (int i=0; i<MAX_HOSTAGES; i++)
	{
		MapPlayer_t *basep = &m_Hostages[i];
		CSMapPlayer_t *p = &m_HostagesCSInfo[i];

		basep->health = 100;
		Q_memset( basep->trail, 0, sizeof(basep->trail) );
		basep->position = Vector( 0, 0, 0 );
		basep->index = 0;

		p->isDead = false;

		p->overrideFadeTime = -1;
		p->overrideExpirationTime = -1;
		p->overrideIcon = -1;
		p->overrideIconOffscreen = -1;
		p->overridePosition = Vector( 0, 0, 0);
		p->overrideAngle = QAngle(0, 0, 0);

		p->timeLastSeen = -1;
		p->timeFirstSeen = -1;
		p->isHostage = false;

		p->flashUntilTime = -1;
		p->nextFlashPeakTime = -1;
		p->currentFlashAlpha = 0;
	}

	m_bomb.position = Vector(0,0,0);
	m_bomb.state = CSMapBomb_t::BOMB_INVALID;
	m_bomb.timeLastSeen = -1;
	m_bomb.timeFirstSeen = -1;
	m_bomb.timeFade = -1;
	m_bomb.timeGone = -1;

	m_bomb.currentRingRadius = -1;
	m_bomb.currentRingAlpha = -1;
	m_bomb.maxRingRadius = -1;
	m_bomb.ringTravelTime = -1;

	m_goalIconsLoaded = false;
}

void CCSMapOverview::DrawCamera()
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	if (!localPlayer)
		return;

	if( localPlayer->GetObserverMode() == OBS_MODE_ROAMING )
	{
		// Instead of the programmer-art red dot, we'll draw an icon for when our camera is roaming.
		int alpha = 255;
		DrawIconCS(m_cameraIconFree, m_cameraIconFree, localPlayer->GetAbsOrigin(), m_flIconSize * 3.0f, localPlayer->EyeAngles()[YAW], alpha);
	}
	else if( localPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		if( localPlayer->GetObserverTarget() )
		{
			// Fade it if it is on top of a player dot.  And don't rotate it.
			int alpha = 255 * 0.5f;
			DrawIconCS(m_cameraIconFirst, m_cameraIconFirst, localPlayer->GetObserverTarget()->GetAbsOrigin(), m_flIconSize * 1.5f, GetViewAngle(), alpha);
		}
	}
	else if( localPlayer->GetObserverMode() == OBS_MODE_CHASE )
	{
		if( localPlayer->GetObserverTarget() )
		{
			// Or Draw the third-camera a little bigger. (Needs room to be off the dot being followed)
			int alpha = 255;
			DrawIconCS(m_cameraIconThird, m_cameraIconThird, localPlayer->GetObserverTarget()->GetAbsOrigin(), m_flIconSize * 3.0f, localPlayer->EyeAngles()[YAW], alpha);
		}
	}
}

void CCSMapOverview::FireGameEvent( IGameEvent *event )
{
	const char * type = event->GetName();

	if ( Q_strcmp(type,"hostage_killed") == 0 )
	{
		MapPlayer_t *hostage = GetHostageByEntityID( event->GetInt("hostage") );

//		DevMsg("Hostage id %d just died.\n", event->GetInt("hostage"));

		if ( !hostage )
			return;

		CSMapPlayer_t *hostageCS = GetCSInfoForHostage(hostage);

		if ( !hostageCS )
			return;

		hostage->health = 0;
		hostageCS->isDead = true;
		Q_memset( hostage->trail, 0, sizeof(hostage->trail) ); // clear trails

		hostageCS->overrideIcon = m_TeamIconsDead[MAP_ICON_HOSTAGE];
		hostageCS->overrideIconOffscreen = hostageCS->overrideIcon;
		hostageCS->overridePosition = hostage->position;
		hostageCS->overrideAngle = hostage->angle;
		hostageCS->overrideFadeTime = gpGlobals->curtime + DEATH_ICON_FADE;
		hostageCS->overrideExpirationTime = gpGlobals->curtime + DEATH_ICON_DURATION;
	}
	else if ( Q_strcmp(type,"hostage_rescued") == 0 )
	{
		MapPlayer_t *hostage = GetHostageByEntityID( event->GetInt("hostage") );

//		DevMsg("Hostage id %d just got rescued.\n", event->GetInt("hostage"));

		if ( !hostage )
			return;

		CSMapPlayer_t *hostageCS = GetCSInfoForHostage(hostage);

		if ( !hostageCS )
			return;

		hostage->health = 0;
		hostageCS->isDead = true;
		Q_memset( hostage->trail, 0, sizeof(hostage->trail) ); // clear trails

		hostageCS->overrideIcon = hostage->icon;
		hostageCS->overrideIconOffscreen = -1;
		hostageCS->overridePosition = hostage->position;
		hostageCS->overrideAngle = hostage->angle;
		hostageCS->overrideFadeTime = gpGlobals->curtime;
		hostageCS->overrideExpirationTime = gpGlobals->curtime + HOSTAGE_RESCUE_DURATION;
	}
	else if ( Q_strcmp(type,"bomb_defused") == 0 )
	{
		m_bomb.state = CSMapBomb_t::BOMB_GONE;
		m_bomb.timeFade = gpGlobals->curtime;
		m_bomb.timeGone = gpGlobals->curtime + BOMB_FADE_DURATION;
	}
	else if ( Q_strcmp(type,"bomb_exploded") == 0 )
	{
		m_bomb.state = CSMapBomb_t::BOMB_GONE;
		m_bomb.timeFade = gpGlobals->curtime;
		m_bomb.timeGone = gpGlobals->curtime + BOMB_FADE_DURATION;
	}
	else if ( Q_strcmp(type,"player_death") == 0 )
	{
		MapPlayer_t *player = GetPlayerByUserID( event->GetInt("userid") );

		if ( !player )
			return;

		player->health = 0;
		Q_memset( player->trail, 0, sizeof(player->trail) ); // clear trails

		CSMapPlayer_t *playerCS = GetCSInfoForPlayer(player);

		if ( !playerCS )
			return;

		playerCS->isDead = true;
		playerCS->overrideIcon = m_TeamIconsDead[GetIconNumberFromTeamNumber(player->team)];
		playerCS->overrideIconOffscreen = playerCS->overrideIcon;
		playerCS->overridePosition = player->position;
		playerCS->overrideAngle = player->angle;
		playerCS->overrideFadeTime = gpGlobals->curtime + DEATH_ICON_FADE;
		playerCS->overrideExpirationTime = gpGlobals->curtime + DEATH_ICON_DURATION;
	}
	else if ( Q_strcmp(type,"player_team") == 0 )
	{
		MapPlayer_t *player = GetPlayerByUserID( event->GetInt("userid") );

		if ( !player )
			return;

		CBasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
		if( localPlayer == NULL )
			return;
		MapPlayer_t *localMapPlayer = GetPlayerByUserID(localPlayer->GetUserID());

		player->team = event->GetInt("team");

		if( player == localMapPlayer )
			player->icon = m_TeamIconsSelf[ GetIconNumberFromTeamNumber(player->team) ];
		else
			player->icon = m_TeamIcons[ GetIconNumberFromTeamNumber(player->team) ];

		player->color = m_TeamColors[ GetIconNumberFromTeamNumber(player->team) ];
	}
	else
	{
		BaseClass::FireGameEvent(event);
	}
}

void CCSMapOverview::SetMode(int mode)
{
	if ( mode == MAP_MODE_RADAR )
	{
		m_flChangeSpeed = 0; // change size instantly
		// We want the _output_ of the radar to be consistant, so we need to take the map scale in to account.
		float desiredZoom = (DESIRED_RADAR_RESOLUTION * m_fMapScale) / (OVERVIEW_MAP_SIZE * m_fFullZoom);

		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "zoom", desiredZoom, 0.0, 0, vgui::AnimationController::INTERPOLATOR_LINEAR );

		if( CBasePlayer::GetLocalPlayer() )
			SetFollowEntity( CBasePlayer::GetLocalPlayer()->entindex() );

		SetPaintBackgroundType( 2 );// rounded corners
		ShowPanel( true );
	}
	else if ( mode == MAP_MODE_INSET )
	{
		SetPaintBackgroundType( 2 );// rounded corners

		float desiredZoom = (overview_preferred_view_size.GetFloat() * m_fMapScale) / (OVERVIEW_MAP_SIZE * m_fFullZoom);

		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "zoom", desiredZoom, 0.0f, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR );
	}
	else 
	{
		SetPaintBackgroundType( 0 );// square corners

		float desiredZoom = 1.0f;

		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "zoom", desiredZoom, 0.0f, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR );
	}

	BaseClass::SetMode(mode);
}

void CCSMapOverview::UpdateSizeAndPosition()
{
	int x,y,w,h;

	vgui::surface()->GetScreenSize( w, h );

	switch( m_nMode )
	{
	case MAP_MODE_RADAR:
		{
			// To allow custom hud scripts to work, get our size from the HudRadar element that people already tweak.
			int x, y, w, t;
			(GET_HUDELEMENT( CHudRadar ))->GetBounds(x, y, w, t);
			m_vPosition.x = x;
			m_vPosition.y = y;

			if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
			{
				m_vPosition.y += g_pSpectatorGUI->GetTopBarHeight();
			}

			m_vSize.x = w;
			m_vSize.y = w;// Intentionally not 't'.  We need to enforce square-ness to prevent people from seeing more of the map by fiddling their HudLayout
			break;
		}

	case MAP_MODE_INSET:
		{
			m_vPosition.x = XRES(16);
			m_vPosition.y = YRES(16);

			if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
			{
				m_vPosition.y += g_pSpectatorGUI->GetTopBarHeight();
			}

			m_vSize.x = w/4;
			m_vSize.y = m_vSize.x/1.333;
			break;
		}

	case MAP_MODE_FULL:
	default:
		{
			m_vSize.x = w;
			m_vSize.y = h;

			m_vPosition.x = 0;
			m_vPosition.y = 0;

			if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
			{
				m_vPosition.y += g_pSpectatorGUI->GetTopBarHeight();
				m_vSize.y -= g_pSpectatorGUI->GetTopBarHeight();
				m_vSize.y -= g_pSpectatorGUI->GetBottomBarHeight();
			}
			break;
		}
	}

	GetBounds( x,y,w,h );

	if ( m_flChangeSpeed > 0 )
	{
		// adjust slowly
		int pixels = m_flChangeSpeed * gpGlobals->frametime;
		x = AdjustValue( x, m_vPosition.x, pixels );
		y = AdjustValue( y, m_vPosition.y, pixels );
		w = AdjustValue( w, m_vSize.x, pixels );
		h = AdjustValue( h, m_vSize.y, pixels );
	}
	else
	{
		// set instantly
		x = m_vPosition.x;
		y = m_vPosition.y;
		w = m_vSize.x;
		h = m_vSize.y;
	}

	SetBounds( x,y,w,h );
}

void CCSMapOverview::SetPlayerSeen( int index )
{
	CSMapPlayer_t *pCS = GetCSInfoForPlayerIndex(index);

	float now = gpGlobals->curtime;

	if( pCS )
	{
		if( pCS->timeLastSeen == -1 )
			pCS->timeFirstSeen = now;

		pCS->timeLastSeen = now;
	}
}

void CCSMapOverview::SetBombSeen( bool seen )
{
	if( seen )
	{
		float now = gpGlobals->curtime;

		if( m_bomb.timeLastSeen == -1 )
			m_bomb.timeFirstSeen = now;

		m_bomb.timeLastSeen = now;
	}
	else
	{
		m_bomb.timeFirstSeen = -1;
		m_bomb.timeLastSeen = -1;
	}
}

void CCSMapOverview::FlashEntity( int entityID )
{
	MapPlayer_t *player = GetPlayerByEntityID(entityID);
	if( player == NULL )
		return;

	CSMapPlayer_t *playerCS = GetCSInfoForPlayer(player);

	if ( !playerCS )
		return;

	playerCS->flashUntilTime = gpGlobals->curtime + 2.0f;
	playerCS->currentFlashAlpha = 255;
	playerCS->nextFlashPeakTime = gpGlobals->curtime + 0.5f;
}

void CCSMapOverview::UpdateFlashes()
{
	float now = gpGlobals->curtime;
	for (int i=0; i<MAX_PLAYERS; i++)
	{
		CSMapPlayer_t *playerCS = GetCSInfoForPlayerIndex(i);
		if( playerCS->flashUntilTime <= now )
		{
			// Flashing over.
			playerCS->currentFlashAlpha = 0;
		}
		else
		{
			if( playerCS->nextFlashPeakTime <= now )
			{
				// Time for a peak
				playerCS->currentFlashAlpha = 255;
				playerCS->nextFlashPeakTime = now + 0.5f;
				playerCS->nextFlashPeakTime = MIN( playerCS->nextFlashPeakTime, playerCS->flashUntilTime );
			}
			else
			{
				// Just fade away
				playerCS->currentFlashAlpha -= ((playerCS->currentFlashAlpha * gpGlobals->frametime) / (playerCS->nextFlashPeakTime - now));
				playerCS->currentFlashAlpha = MAX( 0, playerCS->currentFlashAlpha );
			}
		}
	}
}


//-----------------------------------------------------------------------------
void CCSMapOverview::SetPlayerPreferredMode( int mode )
{
	// A player has given an explicit overview_mode command, so we need to honor that when we are done being the radar.
	m_playerPreferredMode = mode;

	// save off non-radar preferred modes
	switch ( mode )
	{
	case MAP_MODE_OFF:
		overview_preferred_mode.SetValue( MAP_MODE_OFF );
		break;

	case MAP_MODE_INSET:
		overview_preferred_mode.SetValue( MAP_MODE_INSET );
		break;

	case MAP_MODE_FULL:
		overview_preferred_mode.SetValue( MAP_MODE_FULL );
		break;
	}
}

//-----------------------------------------------------------------------------
void CCSMapOverview::SetPlayerPreferredViewSize( float viewSize )
{
	overview_preferred_view_size.SetValue( viewSize );
}


//-----------------------------------------------------------------------------
int CCSMapOverview::GetIconNumberFromTeamNumber( int teamNumber )
{
	switch(teamNumber) 
	{
	case TEAM_TERRORIST:
		return MAP_ICON_T;

	case TEAM_CT:
		return MAP_ICON_CT;

	default:
		return MAP_ICON_HOSTAGE;
	}
}

//-----------------------------------------------------------------------------
void CCSMapOverview::ClearGoalIcons()
{
	m_goalIcons.RemoveAll();
}

//-----------------------------------------------------------------------------
void CCSMapOverview::UpdateGoalIcons()
{
	// The goal entities don't exist on the client, so we have to get them from the CS Resource.
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
		return;

	Vector bombA = pCSPR->GetBombsiteAPosition();
	if( bombA != vec3_origin )
	{
		CSMapGoal_t bombGoalA;
		bombGoalA.position = bombA;
		bombGoalA.iconToUse = m_bombSiteIconA;
		m_goalIcons.AddToTail( bombGoalA );
		m_goalIconsLoaded = true;
	}

	Vector bombB = pCSPR->GetBombsiteBPosition();
	if( bombB != vec3_origin )
	{
		CSMapGoal_t bombGoalB;
		bombGoalB.position = bombB;
		bombGoalB.iconToUse = m_bombSiteIconB;
		m_goalIcons.AddToTail( bombGoalB );
		m_goalIconsLoaded = true;
	}

	for( int rescueIndex = 0; rescueIndex < MAX_HOSTAGE_RESCUES; rescueIndex++ )
	{
		Vector hostageI = pCSPR->GetHostageRescuePosition( rescueIndex );
		if( hostageI != vec3_origin )
		{
			CSMapGoal_t hostageGoalI;
			hostageGoalI.position = hostageI;
			hostageGoalI.iconToUse = m_hostageRescueIcon;
			m_goalIcons.AddToTail( hostageGoalI );
			m_goalIconsLoaded = true;
		}
	}
}

//-----------------------------------------------------------------------------
void CCSMapOverview::DrawGoalIcons()
{
	for( int iconIndex = 0; iconIndex < m_goalIcons.Count(); iconIndex++ )
	{
		// Goal icons are drawn without turning, but with edge adjustment.
		CSMapGoal_t *currentIcon = &(m_goalIcons[iconIndex]);
		int alpha = 255;
		DrawIconCS(currentIcon->iconToUse, currentIcon->iconToUse, currentIcon->position, m_flIconSize, GetViewAngle(), alpha, false);
	}
}

//-----------------------------------------------------------------------------
bool CCSMapOverview::IsRadarLocked() 
{
	return cl_radar_locked.GetBool();
}

//-----------------------------------------------------------------------------
int CCSMapOverview::GetMasterAlpha( void )
{
	// The master alpha is the alpha that the map wants to draw at.  The background will be at half that, and the icons
	// will always be full.  (The icons fade themselves for functional reasons like seen-recently.)
	int alpha = 255;
	if( GetMode() == MAP_MODE_RADAR )
		alpha = cl_radaralpha.GetInt();
	else
		alpha = overview_alpha.GetFloat() * 255;
	alpha = clamp( alpha, 0, 255 );

	return alpha;
}

//-----------------------------------------------------------------------------
int CCSMapOverview::GetBorderSize( void )
{
	switch( GetMode() )
	{
		case MAP_MODE_RADAR:
			return 4;
		case MAP_MODE_INSET:
			return 4;
		case MAP_MODE_FULL:
		default:
			return 0;
	}
}

//-----------------------------------------------------------------------------
Vector2D CCSMapOverview::PanelToMap( const Vector2D &panelPos )
{
	// This is the reversing of baseclass's MapToPanel
	int pwidth, pheight; 
	GetSize(pwidth, pheight);
	float viewAngle = GetViewAngle();
	float fScale = (m_fZoom * m_fFullZoom) / OVERVIEW_MAP_SIZE;

	Vector offset;
	offset.x = (panelPos.x - (pwidth * 0.5f)) / pheight;
	offset.y = (panelPos.y - (pheight * 0.5f)) / pheight;

	offset.x /= fScale;
	offset.y /= fScale;

	VectorYawRotate( offset, -viewAngle, offset );

	Vector2D mapPos;
	mapPos.x = offset.x + m_MapCenter.x;
	mapPos.y = offset.y + m_MapCenter.y;

	return mapPos;
}
