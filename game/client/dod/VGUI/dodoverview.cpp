//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include "dod_shareddefs.h"
#include "dodoverview.h"
#include "c_playerresource.h"	
#include "c_dod_objective_resource.h"
#include "usermessages.h"
#include "coordsize.h"
#include "clientmode.h"
#include <vgui_controls/AnimationController.h>
#include "voice_status.h"
#include "spectatorgui.h"
#include "dod_hud_freezepanel.h"

using namespace vgui;

void __MsgFunc_UpdateRadar(bf_read &msg) 
{
	if ( !g_pMapOverview )
		return;

	int iPlayerEntity = msg.ReadByte();

	while ( iPlayerEntity > 0 )
	{
		int x = msg.ReadSBitLong( COORD_INTEGER_BITS-1 ) * 4;
		int y = msg.ReadSBitLong( COORD_INTEGER_BITS-1 ) * 4;
		int a = msg.ReadSBitLong( 9 );

		Vector origin( x, y, 0 );
		QAngle angles( 0, a, 0 );

		g_pMapOverview->SetPlayerPositions( iPlayerEntity-1, origin, angles );

		iPlayerEntity = msg.ReadByte(); // read index for next player
	}
}

extern ConVar _overview_mode;
ConVar _cl_minimapzoom( "_cl_minimapzoom", "1", FCVAR_ARCHIVE );
ConVar _overview_mode( "_overview_mode", "1", FCVAR_ARCHIVE, "Overview mode - 0=off, 1=inset, 2=full\n", true, 0, true, 2 );


CDODMapOverview *GetDODOverview( void )
{
	return dynamic_cast<CDODMapOverview *>(g_pMapOverview);
}

// overview_togglezoom rotates through 3 levels of zoom for the small map
//-----------------------------------------------------------------------
void ToggleZoom( void )
{
	if ( !GetDODOverview() )
		return;

	GetDODOverview()->ToggleZoom();
}
static ConCommand overview_togglezoom( "overview_togglezoom", ToggleZoom );

// overview_largemap toggles showing the large map
//------------------------------------------------
void ShowLargeMap( void )
{
	if ( !GetDODOverview() )
		return;

	GetDODOverview()->ShowLargeMap();
}
static ConCommand overview_showlargemap( "+overview_largemap", ShowLargeMap );

void HideLargeMap( void )
{
	if ( !GetDODOverview() )
		return;

	GetDODOverview()->HideLargeMap();
}
static ConCommand overview_hidelargemap( "-overview_largemap", HideLargeMap );

//--------------------------------
// map border ?
// icon minimum zoom
// flag swipes
// grenades
// chatting icon
// voice com icon
//---------------------------------

DECLARE_HUDELEMENT( CDODMapOverview );

ConVar dod_overview_voice_icon_size( "dod_overview_voice_icon_size", "64", FCVAR_ARCHIVE );

CDODMapOverview::CDODMapOverview( const char *pElementName ) : BaseClass( pElementName )
{
	InitTeamColorsAndIcons();
	m_flIconSize = 96.0f;
	m_iLastMode = MAP_MODE_OFF;
	usermessages->HookMessage( "UpdateRadar", __MsgFunc_UpdateRadar );
}

void CDODMapOverview::Update()
{
	UpdateCapturePoints();

	BaseClass::Update();
}

void CDODMapOverview::VidInit( void )
{
	m_pC4Icon = gHUD.GetIcon( "icon_c4" );
	m_pExplodedIcon = gHUD.GetIcon( "icon_c4_exploded" );
	m_pC4PlantedBG = gHUD.GetIcon( "icon_c4_planted_bg" );
	m_pIconDefended = gHUD.GetIcon( "icon_defended" );

	BaseClass::VidInit();
}

void CDODMapOverview::UpdateCapturePoints()
{
	if ( !g_pObjectiveResource )
		return;

	Color colorGreen(0,255,0,255);

	if ( !g_pObjectiveResource )
		return;

	for( int i=0;i<g_pObjectiveResource->GetNumControlPoints();i++ )
	{
		// check if CP is visible at all
		if( !g_pObjectiveResource->IsCPVisible(i) )
		{
			if ( m_CapturePoints[i] != 0 )
			{
				// remove capture point from map
				RemoveObject( m_CapturePoints[i] );
				m_CapturePoints[i] = 0;
			}

			continue;
		}

		// ok, show CP
		int iOwningTeam = g_pObjectiveResource->GetOwningTeam(i);
		int iCappingTeam = g_pObjectiveResource->GetCappingTeam(i);

		int iOwningIcon = g_pObjectiveResource->GetIconForTeam( i, iOwningTeam );
		if ( iOwningIcon <= 0 )
			continue;	// baah

		const char *textureName = GetMaterialNameFromIndex( iOwningIcon );

		int objID = m_CapturePoints[i];

		if ( objID == 0 )
		{
			// add object if not already there
			objID = m_CapturePoints[i] = AddObject( textureName, 0, -1 );

			// objective positions never change (so far)
			SetObjectPosition( objID, g_pObjectiveResource->GetCPPosition(i), vec3_angle );

			AddObjectFlags( objID, MAP_OBJECT_ALIGN_TO_MAP );
		}

		SetObjectIcon( objID, textureName, 128.0 );


		int iBombs = g_pObjectiveResource->GetBombsRemaining( i );
		if ( iBombs > 0 )
		{
			char text[8];
			Q_snprintf( text, sizeof(text), "%d", iBombs );
			SetObjectText( objID, text, colorGreen );
		}
		//Draw the number of cappers below the icon
		else if ( iCappingTeam != TEAM_UNASSIGNED )
		{
			int numPlayers = g_pObjectiveResource->GetNumPlayersInArea( i, iCappingTeam );
			int requiredPlayers = g_pObjectiveResource->GetRequiredCappers( i, iCappingTeam );

			if( requiredPlayers > 1 )
			{
				char text[8];
				Q_snprintf( text, sizeof(text), "%d/%d", numPlayers, requiredPlayers );
				SetObjectText( objID, text, colorGreen );
			}
			else
			{
				SetObjectText( objID, NULL, colorGreen );
			}
		}
		else
		{
			SetObjectText( objID, NULL, colorGreen );
		}

		float flBombTime = g_pObjectiveResource->GetBombTimeForPoint( i );
		
		//Draw cap percentage
		if( iCappingTeam != TEAM_UNASSIGNED )
		{
			SetObjectStatus( objID, g_pObjectiveResource->GetCPCapPercentage(i), colorGreen );
		}
		else if ( flBombTime > 0 )
		{
			float flPercentRemaining = ( flBombTime / DOD_BOMB_TIMER_LENGTH );

			SetObjectStatus( objID, flPercentRemaining, colorGreen );
		}
		else
		{
			SetObjectStatus( objID, -1, colorGreen ); // turn it off
		}

	}
}

void CDODMapOverview::InitTeamColorsAndIcons()
{
	BaseClass::InitTeamColorsAndIcons();

	m_TeamColors[TEAM_ALLIES] = COLOR_DOD_GREEN;
	m_TeamIcons[TEAM_ALLIES] = AddIconTexture( "sprites/minimap_icons/aplayer" );
	m_CameraIcons[TEAM_ALLIES] = AddIconTexture( "sprites/minimap_icons/allies_camera" );
	

	m_TeamColors[TEAM_AXIS] = COLOR_DOD_RED;
	m_TeamIcons[TEAM_AXIS] = AddIconTexture( "sprites/minimap_icons/gplayer" );
	m_CameraIcons[TEAM_AXIS] = AddIconTexture( "sprites/minimap_icons/axis_camera" );

	Q_memset( m_flPlayerChatTime, 0, sizeof(m_flPlayerChatTime ) );
	m_iVoiceIcon = AddIconTexture( "voice/icntlk_pl" );
	m_iChatIcon = AddIconTexture( "sprites/minimap_icons/voiceIcon" );

	Q_memset( m_CapturePoints, 0, sizeof(m_CapturePoints) );
}

void CDODMapOverview::DrawCamera()
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !localPlayer )
		return;

	int iTexture = m_CameraIcons[localPlayer->GetTeamNumber()];

	if ( localPlayer->IsObserver() || iTexture <= 0 )
	{
		BaseClass::DrawCamera();
	}
	else
	{
		MapObject_t obj;
		memset( &obj, 0, sizeof(MapObject_t) );

		obj.icon = iTexture;
		obj.position = localPlayer->GetAbsOrigin();
		obj.size = m_flIconSize * 1.5;
		obj.angle = localPlayer->EyeAngles();
		obj.status = -1;

		DrawIcon( &obj );

		DrawVoiceIconForPlayer( localPlayer->entindex() - 1 );
	}
}

void CDODMapOverview::FireGameEvent( IGameEvent *event )
{
	const char * type = event->GetName();

	if ( Q_strcmp(type, "player_death") == 0 )
	{
		MapPlayer_t *player = GetPlayerByUserID( event->GetInt("userid") );

		if ( player && CanPlayerBeSeen( player ) )
		{
			// create skull icon for 3 seconds
			int handle = AddObject( "sprites/minimap_icons/death", 0, 3 );
			SetObjectText( handle, player->name, player->color );
			SetObjectPosition( handle, player->position, player->angle );
		}
	}
	else if ( Q_strcmp(type, "game_newmap") == 0 )
	{
		SetMode( _overview_mode.GetInt() );
	}

	BaseClass::FireGameEvent( event	);
}

// rules that define if you can see a player on the overview or not
bool CDODMapOverview::CanPlayerBeSeen(MapPlayer_t *player)
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !localPlayer || !player )
		return false;

	// don't draw ourselves
	if ( localPlayer->entindex() == (player->index+1) )
		return false;

	// if local player is on spectator team, he can see everyone
	if ( localPlayer->GetTeamNumber() <= TEAM_SPECTATOR )
		return true;

	// we never track unassigned or real spectators
	if ( player->team <= TEAM_SPECTATOR )
		return false;

	// ingame and as dead player we can only see our own teammates
	return (localPlayer->GetTeamNumber() == player->team );
}

void CDODMapOverview::ShowLargeMap( void )
{
	// remember old mode
	m_iLastMode = GetMode();

	// if we hit the toggle while full, set to disappear when we release
	if ( m_iLastMode == MAP_MODE_FULL )
		m_iLastMode = MAP_MODE_OFF;

	SetMode( MAP_MODE_FULL );
}

void CDODMapOverview::HideLargeMap( void )
{
	SetMode( m_iLastMode );
}

void CDODMapOverview::ToggleZoom( void )
{
	if ( GetMode() != MAP_MODE_INSET )
		return;

	int iZoomLevel = ( _cl_minimapzoom.GetInt() + 1 ) % DOD_MAP_ZOOM_LEVELS;

	_cl_minimapzoom.SetValue( iZoomLevel );

	switch( _cl_minimapzoom.GetInt() )
	{
	case 0:
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapZoomLevel1" );
		break;
	case 1:
	default:
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapZoomLevel2" );
		break;
	}
}

void CDODMapOverview::SetMode(int mode)
{
	m_flChangeSpeed = 0; // change size instantly

	if ( mode == MAP_MODE_OFF )
	{
		ShowPanel( false );

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapOff" );
	}
	else if ( mode == MAP_MODE_INSET )
	{
		switch( _cl_minimapzoom.GetInt() )
		{
		case 0:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapZoomLevel1" );
			break;
		case 1:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapZoomLevel2" );
			break;
		case 2:
		default:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapZoomLevel3" );
			break;
		}

		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

		if ( pPlayer )
			SetFollowEntity( pPlayer->entindex() );

		ShowPanel( true );

		if ( m_nMode == MAP_MODE_FULL )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MapScaleToSmall" );
		else
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SnapToSmall" );
	}
	else if ( mode == MAP_MODE_FULL )
	{
		SetFollowEntity( 0 );

		ShowPanel( true );

		if ( m_nMode == MAP_MODE_INSET )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ZoomToLarge" );
		else
            g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SnapToLarge" );
	}

	// finally set mode
	m_nMode = mode;

	// save in a cvar for archive
	_overview_mode.SetValue( m_nMode );

	UpdateSizeAndPosition();
}

void CDODMapOverview::UpdateSizeAndPosition()
{
	// move back up if the spectator menu is not visible
	if ( !g_pSpectatorGUI || ( !g_pSpectatorGUI->IsVisible() && GetMode() == MAP_MODE_INSET ) )
	{
		int x,y,w,h;

		GetBounds( x,y,w,h );

		y = YRES(5);	// hax, align to top of the screen

		SetBounds( x,y,w,h );
	}

	BaseClass::UpdateSizeAndPosition();
}

void CDODMapOverview::AddGrenade( C_DODBaseGrenade *pGrenade )
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !localPlayer )
		return;

	int localTeam = localPlayer->GetTeamNumber();

	// Spectators can see all grenades
	// players can only see them if they are on the same team
	if ( localTeam == TEAM_SPECTATOR || ( localTeam == pGrenade->GetTeamNumber() ) )
	{
		AddObject( pGrenade->GetOverviewSpriteName(), pGrenade->entindex(), -1 );
	}
}

void CDODMapOverview::RemoveGrenade( C_DODBaseGrenade *pGrenade )
{
	RemoveObjectByIndex( pGrenade->entindex() );
}

ConVar cl_voicetest( "cl_voicetest", "0", FCVAR_CHEAT );
ConVar cl_overview_chat_time( "cl_overview_chat_time", "2.0", FCVAR_ARCHIVE );

void CDODMapOverview::PlayerChat( int index )
{
	m_flPlayerChatTime[index-1] = gpGlobals->curtime + cl_overview_chat_time.GetFloat();
}

void CDODMapOverview::DrawMapPlayers()
{
	BaseClass::DrawMapPlayers();

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();

	Assert( localPlayer );

	int iLocalPlayer = localPlayer->entindex() - 1;

	for (int i=0; i<MAX_PLAYERS; i++)
	{
		if ( i == iLocalPlayer )
			continue;

		MapPlayer_t *player = &m_Players[i];

		if ( !CanPlayerBeSeen( player ) )
			continue;

		if ( player->health <= 0 )	// don't draw dead players / spectators
			continue;

		DrawVoiceIconForPlayer( i );
	}
}

void CDODMapOverview::DrawVoiceIconForPlayer( int playerIndex )
{
	Assert( playerIndex >= 0 && playerIndex < MAX_PLAYERS );

	MapPlayer_t *player = &m_Players[playerIndex];

	// if they just sent a chat msg, or are using voice, or did a hand signal or voice command
	// draw a chat icon

	if ( cl_voicetest.GetInt() || GetClientVoiceMgr()->IsPlayerSpeaking( player->index+1 ) )
	{
		MapObject_t obj;
		memset( &obj, 0, sizeof(MapObject_t) );

		obj.icon = m_iVoiceIcon;
		obj.position = player->position;
		obj.size = dod_overview_voice_icon_size.GetFloat();
		obj.status = -1;

		DrawIcon( &obj );
	}
	else if ( m_flPlayerChatTime[player->index] > gpGlobals->curtime )
	{
		MapObject_t obj;
		memset( &obj, 0, sizeof(MapObject_t) );

		obj.icon = m_iChatIcon;
		obj.position = player->position;
		obj.size = dod_overview_voice_icon_size.GetFloat();
		obj.status = -1;

		DrawIcon( &obj );
	}
}

bool CDODMapOverview::DrawIcon( MapObject_t *obj )
{
	for ( int i=0;i<MAX_CONTROL_POINTS;i++ )
	{
		if ( obj->objectID == m_CapturePoints[i] && obj->objectID != 0 )
		{
			return DrawCapturePoint( i, obj );
		}
	}

	return  BaseClass::DrawIcon( obj );
}

void CDODMapOverview::DrawQuad( Vector pos, int scale, float angle, int textureID, int alpha )
{
	Vector offset;
	offset.z = 0;

	offset.x = -scale;	offset.y = scale;
	VectorYawRotate( offset, angle, offset );
	Vector2D pos1 = WorldToMap( pos + offset );

	offset.x = scale;	offset.y = scale;
	VectorYawRotate( offset, angle, offset );
	Vector2D pos2 = WorldToMap( pos + offset );

	offset.x = scale;	offset.y = -scale;
	VectorYawRotate( offset, angle, offset );
	Vector2D pos3 = WorldToMap( pos + offset );

	offset.x = -scale;	offset.y = -scale;
	VectorYawRotate( offset, angle, offset );
	Vector2D pos4 = WorldToMap( pos + offset );

	Vertex_t points[4] =
	{
		Vertex_t( MapToPanel ( pos1 ), Vector2D(0,0) ),
		Vertex_t( MapToPanel ( pos2 ), Vector2D(1,0) ),
		Vertex_t( MapToPanel ( pos3 ), Vector2D(1,1) ),
		Vertex_t( MapToPanel ( pos4 ), Vector2D(0,1) )
	};

	surface()->DrawSetColor( 255, 255, 255, alpha );
	surface()->DrawSetTexture( textureID );
	surface()->DrawTexturedPolygon( 4, points );
}

bool CDODMapOverview::DrawCapturePoint( int iCP, MapObject_t *obj )
{
	int textureID = obj->icon;
	Vector pos = obj->position;
	float scale = obj->size;
	float angle = 0;

	Vector2D pospanel = WorldToMap( pos );
	pospanel = MapToPanel( pospanel );

	if ( !IsInPanel( pospanel ) )
		return false; // player is not within overview panel

	int iBombsRequired = g_pObjectiveResource->GetBombsRequired( iCP );

	if ( iBombsRequired )
	{
		if ( g_pObjectiveResource->IsBombSetAtPoint( iCP ) )
		{
			// draw swipe over blank icon

			// 'white' icon
			int iBlankIcon = g_pObjectiveResource->GetCPTimerCapIcon( iCP );
			const char *textureName = GetMaterialNameFromIndex( iBlankIcon );
			DrawQuad( pos, scale, 0, AddIconTexture( textureName ), 255 );

			// the circular swipe
			float flBombTime = g_pObjectiveResource->GetBombTimeForPoint( iCP );
			float flPercentRemaining = ( flBombTime / DOD_BOMB_TIMER_LENGTH );

			DrawBombTimerSwipeIcon( pos, scale, textureID, flPercentRemaining );
		}
		else
		{
			DrawQuad( pos, scale, 0, textureID, 255 );
		}
	}
	else
	{
		// draw capture swipe
		DrawQuad( pos, scale, 0, textureID, 255 );

		int iCappingTeam = g_pObjectiveResource->GetCappingTeam( iCP );

		if ( iCappingTeam != TEAM_UNASSIGNED )
		{
			int iCapperIcon = g_pObjectiveResource->GetCPCappingIcon( iCP );
			const char *textureName = GetMaterialNameFromIndex( iCapperIcon );

			float flCapPercent = g_pObjectiveResource->GetCPCapPercentage(iCP);
			bool bSwipeLeft = ( iCappingTeam == TEAM_AXIS ) ? true : false;

			DrawHorizontalSwipe( pos, scale, AddIconTexture( textureName ), flCapPercent, bSwipeLeft );
		}

		// fixup for noone is capping, but someone is in the area
		int iNumAllies = g_pObjectiveResource->GetNumPlayersInArea( iCP, TEAM_ALLIES );
		int iNumAxis = g_pObjectiveResource->GetNumPlayersInArea( iCP, TEAM_AXIS );

		int iOwningTeam = g_pObjectiveResource->GetOwningTeam( iCP );
		if ( iCappingTeam == TEAM_UNASSIGNED )
		{
			if ( iNumAllies > 0 && iNumAxis == 0 && iOwningTeam != TEAM_ALLIES )
			{
				iCappingTeam = TEAM_ALLIES;
			}
			else if ( iNumAxis > 0 && iNumAllies == 0 && iOwningTeam != TEAM_AXIS )
			{
				iCappingTeam = TEAM_AXIS;
			}
		}

		if ( iCappingTeam != TEAM_UNASSIGNED )
		{
			// Draw the number of cappers below the icon
			int numPlayers = g_pObjectiveResource->GetNumPlayersInArea( iCP, iCappingTeam );
			int requiredPlayers = g_pObjectiveResource->GetRequiredCappers( iCP, iCappingTeam );

			if ( requiredPlayers > 1 )
			{
				numPlayers = MIN( numPlayers, requiredPlayers );

				wchar_t wText[6];
				_snwprintf( wText, sizeof(wText)/sizeof(wchar_t), L"%d/%d", numPlayers, requiredPlayers );

				int wide, tall;
				surface()->GetTextSize( m_hIconFont, wText, wide, tall );

				int x = pospanel.x-(wide/2);
				int y = pospanel.y;

				// match the offset that MapOverview uses
				y += GetPixelOffset( scale ) + 4;

				// draw black shadow text
				surface()->DrawSetTextColor( 0, 0, 0, 255 );
				surface()->DrawSetTextPos( x+1, y );
				surface()->DrawPrintText( wText, wcslen(wText) );

				// draw name in color 
				surface()->DrawSetTextColor( g_PR->GetTeamColor( iCappingTeam ) );
				surface()->DrawSetTextPos( x, y );
				surface()->DrawPrintText( wText, wcslen(wText) );
			}
		}
	}

	// draw bombs underneath if necessary
	if ( iBombsRequired > 0 )
	{
		int iBombsRemaining = g_pObjectiveResource->GetBombsRemaining( iCP );
		bool bBombPlanted = g_pObjectiveResource->IsBombSetAtPoint( iCP );

		// draw bomb state underneath
		float flBombIconScale = scale * 0.5;


		switch( iBombsRequired )
		{
		case 1:
			{
				Vector bombPos = pos;
				bombPos.y -= scale * 1.5;

				switch( iBombsRemaining )
				{
					case 0:
						DrawQuad( bombPos, flBombIconScale, angle, m_pExplodedIcon->textureId, 255 );
						break;
					case 1:
						if ( bBombPlanted )
						{
							// draw the background behind 1
							int alpha = (float)( abs( sin(2*gpGlobals->curtime) ) * 205.0 + 50.0 );
							DrawQuad( bombPos, flBombIconScale*2, angle, m_pC4PlantedBG->textureId, alpha );
						}
						DrawQuad( bombPos, flBombIconScale, angle, m_pC4Icon->textureId, 255 );
						break;
				}
			}
			break;
		case 2:
			{
				Vector bombPos1, bombPos2;
				bombPos1 = bombPos2 = pos;
				bombPos1.y = bombPos2.y = pos.y - scale * 1.5;

				bombPos1.x = pos.x - scale * 0.5;
				bombPos2.x = pos.x + scale * 0.5;

				switch( iBombsRemaining )
				{
				case 0:
					DrawQuad( bombPos1, flBombIconScale, angle, m_pExplodedIcon->textureId, 255 );
					DrawQuad( bombPos2, flBombIconScale, angle, m_pExplodedIcon->textureId, 255 );
					break;
				case 1:
					if ( bBombPlanted )
					{
						// draw the background behind 1
						int alpha = (float)( abs( sin(2*gpGlobals->curtime) ) * 205.0 + 50.0 );
						DrawQuad( bombPos1, flBombIconScale*2, angle, m_pC4PlantedBG->textureId, alpha );
					}
					DrawQuad( bombPos1, flBombIconScale, angle, m_pC4Icon->textureId, 255 );
					DrawQuad( bombPos2, flBombIconScale, angle, m_pExplodedIcon->textureId, 255 );
					break;
				case 2:
					if ( bBombPlanted )
					{
						// draw the background behind 2
						int alpha = (float)( abs( sin(2*gpGlobals->curtime) ) * 205.0 + 50.0 );
						DrawQuad( bombPos2, flBombIconScale*2, angle, m_pC4PlantedBG->textureId, alpha );
					}
					DrawQuad( bombPos1, flBombIconScale, angle, m_pC4Icon->textureId, 255 );
					DrawQuad( bombPos2, flBombIconScale, angle, m_pC4Icon->textureId, 255 );
					break;
				}
			}
			break;
		}

		// draw shield over top
		if ( g_pObjectiveResource->IsBombBeingDefused( iCP ) )
		{
			DrawQuad( pos, scale * 0.75, angle, m_pIconDefended->textureId, 255 );
		}
	}

	return true;
}

void CDODMapOverview::DrawBombTimerSwipeIcon( Vector pos, int scale, int textureID, float flPercentRemaining )
{
	const float flCompleteCircle = ( 2.0f * M_PI );
	const float fl90degrees = flCompleteCircle * 0.25f;
	const float fl45degrees = fl90degrees * 0.5f;

	float flEndAngle = flCompleteCircle * flPercentRemaining; // clockwise

	typedef struct 
	{
		Vector2D vecTrailing;
		Vector2D vecLeading;
	} icon_quadrant_t;

	/*
	Quadrants are numbered 0 - 7 counter-clockwise
	_________________
	|	  0	| 7		|
	|		|		|
	|  1	|    6  |
	-----------------
	|  2	|	 5  |
	|		|		|
	|	  3	| 4		|
	-----------------
	*/

	// Encode the leading and trailing edge of each quadrant 
	// in the range 0.0 -> 1.0

	icon_quadrant_t quadrants[8];
	quadrants[0].vecTrailing.Init( 0.5, 0.0 );
	quadrants[0].vecLeading.Init( 0.0, 0.0 );

	quadrants[1].vecTrailing.Init( 0.0, 0.0 );
	quadrants[1].vecLeading.Init( 0.0, 0.5 );

	quadrants[2].vecTrailing.Init( 0.0, 0.5 );
	quadrants[2].vecLeading.Init( 0.0, 1.0 );

	quadrants[3].vecTrailing.Init( 0.0, 1.0 );
	quadrants[3].vecLeading.Init( 0.5, 1.0 );

	quadrants[4].vecTrailing.Init( 0.5, 1.0 );
	quadrants[4].vecLeading.Init( 1.0, 1.0 );

	quadrants[5].vecTrailing.Init( 1.0, 1.0 );
	quadrants[5].vecLeading.Init( 1.0, 0.5 );

	quadrants[6].vecTrailing.Init( 1.0, 0.5 );
	quadrants[6].vecLeading.Init( 1.0, 0.0 );

	quadrants[7].vecTrailing.Init( 1.0, 0.0 );
	quadrants[7].vecLeading.Init( 0.5, 0.0 );

	surface()->DrawSetColor( 255, 255, 255, 255 );
	surface()->DrawSetTexture( textureID );

	Vector2D uvMid( 0.5, 0.5 );
	Vector2D newPos( pos.x - scale, pos.y + scale );

	int j;
	for ( j=0;j<=7;j++ )
	{
		float flMinAngle = j * fl45degrees;	

		float flAngle = clamp( flEndAngle - flMinAngle, 0, fl45degrees );

		if ( flAngle <= 0 )
		{
			// past our quadrant, draw nothing
			continue;
		}
		else
		{
			// draw our segment
			vgui::Vertex_t vert[3];	

			// vert 0 is mid ( 0.5, 0.5 )

			Vector2D pos0 = WorldToMap( pos );
			vert[0].Init( MapToPanel( pos0 ), uvMid );

			int xdir = 0, ydir = 0;

			switch( j )
			{
			case 0:  
			case 7:
				//right
				xdir = 1;
				ydir = 0;
				break;

			case 1: 
			case 2:
				//up
				xdir = 0;
				ydir = -1;
				break;

			case 3:
			case 4:
				//left
				xdir = -1;
				ydir = 0;
				break;

			case 5:
			case 6:
				//down
				xdir = 0;
				ydir = 1;
				break;
			}

			Vector vec1;
			Vector2D uv1;

			// vert 1 is the variable vert based on leading edge
			vec1.x = newPos.x + quadrants[j].vecTrailing.x * scale*2 - xdir * tan(flAngle) * scale;
			vec1.y = newPos.y - quadrants[j].vecTrailing.y * scale*2 + ydir * tan(flAngle) * scale;

			uv1.x = quadrants[j].vecTrailing.x - xdir * abs( quadrants[j].vecLeading.x - quadrants[j].vecTrailing.x ) * tan(flAngle);
			uv1.y = quadrants[j].vecTrailing.y - ydir * abs( quadrants[j].vecLeading.y - quadrants[j].vecTrailing.y ) * tan(flAngle);

			Vector2D pos1 = WorldToMap( vec1 );
			vert[1].Init( MapToPanel( pos1 ), uv1 );

			// vert 2 is our trailing edge
			Vector vec2;
			
			vec2.x = newPos.x + quadrants[j].vecTrailing.x * scale*2;
            vec2.y = newPos.y - quadrants[j].vecTrailing.y * scale*2;

			Vector2D pos2 = WorldToMap( vec2 );
			vert[2].Init( MapToPanel( pos2 ), quadrants[j].vecTrailing );

			surface()->DrawTexturedPolygon( 3, vert );
		}	
	}
}

void CDODMapOverview::DrawHorizontalSwipe( Vector pos, int scale, int textureID, float flCapPercentage, bool bSwipeLeft )
{
	float flIconSize = scale * 2;
	float width = ( flIconSize * flCapPercentage );

	float uv1 = 0.0f;
	float uv2 = 1.0f;

	Vector2D uv11( uv1, uv2 );
	Vector2D uv21( flCapPercentage, uv2 );
	Vector2D uv22( flCapPercentage, uv1 );
	Vector2D uv12( uv1, uv1 );

	// reversing the direction of the swipe effect
	if ( bSwipeLeft )
	{
		uv11.x = uv2 - flCapPercentage;
		uv21.x = uv2;
		uv22.x = uv2;
		uv12.x = uv2 - flCapPercentage;
	}

	float flXPos = pos.x - scale;
	float flYPos = pos.y - scale;

	Vector upperLeft( flXPos,			flYPos, 0 );
	Vector upperRight( flXPos + width,	flYPos, 0 );
	Vector lowerRight( flXPos + width,	flYPos + flIconSize, 0 );
	Vector lowerLeft ( flXPos,			flYPos + flIconSize, 0 );

	/// reversing the direction of the swipe effect
	if ( bSwipeLeft )
	{
		upperLeft.x  = flXPos + flIconSize - width;
		upperRight.x = flXPos + flIconSize;
		lowerRight.x = flXPos + flIconSize;
		lowerLeft.x  = flXPos + flIconSize - width;
	}

	vgui::Vertex_t vert[4];	

	Vector2D pos0 = WorldToMap( upperLeft );
	vert[0].Init( MapToPanel( pos0 ), uv11 );

	Vector2D pos3 = WorldToMap( lowerLeft );
	vert[1].Init( MapToPanel( pos3 ), uv12 );

	Vector2D pos2 = WorldToMap( lowerRight );
	vert[2].Init( MapToPanel( pos2 ), uv22 );

	Vector2D pos1 = WorldToMap( upperRight );
	vert[3].Init( MapToPanel( pos1 ), uv21 );

	surface()->DrawSetColor( 255, 255, 255, 255 );
	surface()->DrawSetTexture( textureID );
	surface()->DrawTexturedPolygon( 4, vert );
}


bool CDODMapOverview::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}