//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <vgui/ISurface.h>
#include "clientmode_csnormal.h"
#include "cs_gamerules.h"
#include "hud_numericdisplay.h"
#include "voice_status.h"
#include "c_plantedc4.h"
#include "weapon_c4.h"
#include "c_cs_hostage.h"
#include "c_cs_playerresource.h"
#include <coordsize.h>
#include "hud_macros.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"
#include "mapoverview.h"
#include "cstrikespectatorgui.h"
#include "hud_radar.h"

#define RADAR_DOT_NORMAL		0
#define RADAR_DOT_BOMB			(1<<0)
#define RADAR_DOT_HOSTAGE		(1<<1)
#define RADAR_DOT_BOMBCARRIER	(1<<2)
#define RADAR_DOT_VIP			(1<<3)
#define RADAR_DOT_LARGE_FLASH	(1<<4)
#define RADAR_DOT_BOMB_PLANTED	(1<<5)
#define RADAR_IGNORE_Z			(1<<6)	//always draw this item as if it was at the same Z as the player

extern CUtlVector< CC4* > g_C4s;

ConVar cl_radartype( "cl_radartype", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar cl_radaralpha( "cl_radaralpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, NULL, true, 0, true, 255 );
ConVar cl_locationalpha( "cl_locationalpha", "150", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, NULL, true, 0, true, 255 );

DECLARE_HUDELEMENT( CHudRadar );
DECLARE_HUD_MESSAGE( CHudRadar, UpdateRadar );

static CHudRadar *s_Radar = NULL;
CUtlVector<CPlayerRadarFlash> g_RadarFlashes;


CHudRadar::CHudRadar( const char *pName ) :	vgui::Panel( NULL, "HudRadar" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );

	m_pBackground = NULL;
	m_pBackgroundTrans = NULL;

	m_flNextBombFlashTime = 0.0;
	m_bBombFlash = true;

	m_flNextHostageFlashTime = 0.0;
	m_bHostageFlash = true;
	m_bHideRadar = false;

	s_Radar = this;

	SetHiddenBits( HIDEHUD_PLAYERDEAD );

}


CHudRadar::~CHudRadar()
{
	s_Radar = NULL;
}


void CHudRadar::Init()
{
	HOOK_HUD_MESSAGE( CHudRadar, UpdateRadar );
}

void CHudRadar::LevelInit()
{
	m_flNextBombFlashTime = 0.0;
	m_bBombFlash = true;

	m_flNextHostageFlashTime = 0.0;
	m_bHostageFlash = true;

	g_RadarFlashes.RemoveAll();

	// Map Overview handles radar duties now.
	if( g_pMapOverview )
		g_pMapOverview->SetMode(CCSMapOverview::MAP_MODE_RADAR);
}

void CHudRadar::Reset()
{
	CHudElement::Reset();

	if( g_pMapOverview )
		g_pMapOverview->SetMode(CCSMapOverview::MAP_MODE_RADAR);
}

void CHudRadar::MsgFunc_UpdateRadar(bf_read &msg )
{
	int iPlayerEntity = msg.ReadByte();

	//Draw objects on the radar
	//=============================
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return;

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();

	if ( !pCSPR )
		return;

	//Players
	for(int i=1;i<=MAX_PLAYERS;i++)
	{
		if( i == pLocalPlayer->entindex() )
			continue;

		C_CSPlayer *pPlayer =	ToCSPlayer( UTIL_PlayerByIndex(i) );

		if ( pPlayer )
		{
			pPlayer->m_bDetected = false;
		}
	}


	while ( iPlayerEntity > 0 )
	{
		int x = msg.ReadSBitLong( COORD_INTEGER_BITS-1 ) * 4;
		int y = msg.ReadSBitLong( COORD_INTEGER_BITS-1 ) * 4;
		int z = msg.ReadSBitLong( COORD_INTEGER_BITS-1 ) * 4;
		int a = msg.ReadSBitLong( 9 );

		C_CSPlayer *pPlayer =	ToCSPlayer( UTIL_PlayerByIndex(iPlayerEntity) );

		Vector origin( x, y, z );
		QAngle angles( 0, a, 0 );

		if ( g_pMapOverview )
		{
			g_pMapOverview->SetPlayerPositions( iPlayerEntity-1, origin, angles );
		}

		iPlayerEntity = msg.ReadByte(); // read index for next player

		if ( !pPlayer )
			continue;

		bool bOppositeTeams = (pLocalPlayer->GetTeamNumber() != TEAM_UNASSIGNED && pCSPR->GetTeam( pPlayer->entindex() ) != pLocalPlayer->GetTeamNumber());		

		//=============================================================================
		// HPE_BEGIN:
		// [tj] This used to do slightly different logic that caused other players
		//		to twitch while you were observing.
		//=============================================================================
		// Don't update players if they are in PVS.
		if (!pPlayer->IsDormant())
		{
			continue;
		}

		//Don't update players if you are sill alive and they are an enemy.
		if (bOppositeTeams && !pLocalPlayer->IsObserver())
		{
			continue;
		}
		//=============================================================================
		// HPE_END
		//=============================================================================
		// update origin and angle for players out of my PVS
		origin = pPlayer->GetAbsOrigin();
		angles = pPlayer->GetAbsAngles();

		origin.x = x;
		origin.y = y;
		angles.y = a;

		pPlayer->SetAbsOrigin( origin );
		pPlayer->SetAbsAngles( angles );
		pPlayer->m_bDetected = true;
	}
}


bool CHudRadar::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	
	//=============================================================================
	// HPE_BEGIN:
	// [tj] Added base class call
	//=============================================================================
	return pPlayer && pPlayer->IsAlive() && !m_bHideRadar && CHudElement::ShouldDraw();
	//=============================================================================
	// HPE_END
	//=============================================================================
}

void CHudRadar::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if( g_pMapOverview  &&  g_pMapOverview->GetMode() == CCSMapOverview::MAP_MODE_RADAR )
	{
		// We are the hud element still, but he is in charge of the new style now.
		g_pMapOverview->SetVisible( state );		
	}
}

void CHudRadar::Paint()
{
	// We are the hud element still, but Overview is in charge of the new style now.
	return;
}

void CHudRadar::WorldToRadar( const Vector location, const Vector origin, const QAngle angles, float &x, float &y, float &z_delta )
{
	float x_diff = location.x - origin.x;
	float y_diff = location.y - origin.y;
 
	int iRadarRadius = GetWide();		//width of the panel, it resizes now!
	float fRange = 16 * iRadarRadius;	// radar's range

	float flOffset = atan(y_diff/x_diff);
	flOffset *= 180;
	flOffset /= M_PI;

	if ((x_diff < 0) && (y_diff >= 0))
		flOffset = 180 + flOffset;
	else if ((x_diff < 0) && (y_diff < 0))
		flOffset = 180 + flOffset;
	else if ((x_diff >= 0) && (y_diff < 0))
		flOffset = 360 + flOffset;

	y_diff = -1*(sqrt((x_diff)*(x_diff) + (y_diff)*(y_diff)));
	x_diff = 0;

	flOffset = angles.y - flOffset;

	flOffset *= M_PI;
	flOffset /= 180;		// now theta is in radians

	float xnew_diff = x_diff * cos(flOffset) - y_diff * sin(flOffset);
	float ynew_diff = x_diff * sin(flOffset) + y_diff * cos(flOffset);

	// The dot is out of the radar's range.. Scale it back so that it appears on the border
	if ( (-1 * y_diff) > fRange )
	{
		float flScale;

		flScale = ( -1 * y_diff) / fRange;

		xnew_diff /= flScale;
		ynew_diff /= flScale;
	}
	xnew_diff /= 32;
	ynew_diff /= 32;

	//output
	x = (iRadarRadius/2) + (int)xnew_diff;
	y = (iRadarRadius/2) + (int)ynew_diff;
	z_delta = location.z - origin.z;
}

void CHudRadar::DrawPlayerOnRadar( int iPlayer, C_CSPlayer *pLocalPlayer )
{
	float x, y, z_delta;
	int iBaseDotSize = ScreenWidth() / 256;	
	int r, g, b, a = 235;

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();

	if ( !pCSPR )
		return;

	C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );

	if ( !pPlayer )
		return;

	bool bOppositeTeams = (pLocalPlayer->GetTeamNumber() != TEAM_UNASSIGNED && pCSPR->GetTeam( iPlayer ) != pLocalPlayer->GetTeamNumber());

	if ( bOppositeTeams && pPlayer->m_bDetected == false )
		return;

	
	WorldToRadar( pPlayer->GetAbsOrigin(), pLocalPlayer->GetAbsOrigin(), pLocalPlayer->LocalEyeAngles(), x, y, z_delta );

	if( pCSPR->HasC4( iPlayer ) || pCSPR->IsVIP( iPlayer ) || bOppositeTeams )
	{
		r = 250; g = 0; b = 0;
	}
	else if ( 0 /*m_bTrackArray[i-1] == true */ )		// Tracked players (friends we want to keep track of on the radar)
	{
		iBaseDotSize *= 2;
		r = 185; g = 20; b = 20;
	}
	else
	{
		r = 75; g = 75; b = 250;
	}

	// Handle the radio flashes
	bool bRadarFlash = false;
	if ( g_RadarFlashes.Count() > iPlayer )
		bRadarFlash = g_RadarFlashes[iPlayer].m_bRadarFlash && g_RadarFlashes[iPlayer].m_iNumRadarFlashes > 0;

	if ( bRadarFlash || GetClientVoiceMgr()->IsPlayerSpeaking( iPlayer ) )
	{
		r = 230; g = 110; b = 25; a = 245; 

		DrawRadarDot( x, y, z_delta, iBaseDotSize, RADAR_DOT_LARGE_FLASH, r, g, b, a );
	}
	else
	{
		DrawRadarDot( x, y, z_delta, iBaseDotSize, RADAR_DOT_NORMAL, r, g, b, a );
	}
}


void CHudRadar::DrawEntityOnRadar( CBaseEntity *pEnt, C_CSPlayer *pLocalPlayer, int flags, int r, int g, int b, int a )
{
	float x, y, z_delta;
	int iBaseDotSize = 4;

	WorldToRadar( pEnt->GetAbsOrigin(), pLocalPlayer->GetAbsOrigin(), pLocalPlayer->LocalEyeAngles(), x, y, z_delta );

	if( flags & RADAR_IGNORE_Z )
		z_delta = 0;

	DrawRadarDot( x, y, z_delta, iBaseDotSize, flags, r, g, b, a );
}

void CHudRadar::FillRect( int x, int y, int w, int h )
{
	int panel_x, panel_y, panel_w, panel_h;
	GetBounds( panel_x, panel_y, panel_w, panel_h );
	vgui::surface()->DrawFilledRect( x, y, x+w, y+h );
}

void CHudRadar::DrawRadarDot( int x, int y, float z_diff, int iBaseDotSize, int flags, int r, int g, int b, int a )
{
	vgui::surface()->DrawSetColor( r, g, b, a );

	if ( flags & RADAR_DOT_LARGE_FLASH )
	{
		FillRect( x-1, y-1, iBaseDotSize+1, iBaseDotSize+1 );
	}
	else if ( z_diff < -128 ) // below the player
	{
		z_diff *= -1;

		if ( z_diff > 3096 )
		{
			z_diff = 3096;
		}

		int iBar = (int)( z_diff / 400 ) + 2;

		// Draw an upside-down T shape to symbolize the dot is below the player.

		iBaseDotSize /= 2;

		//horiz
		FillRect( x-(2*iBaseDotSize), y, 5*iBaseDotSize, iBaseDotSize );

		//vert
		FillRect( x, y - iBar*iBaseDotSize, iBaseDotSize, iBar*iBaseDotSize );
	}
	else if ( z_diff > 128 ) // above the player
	{
		if ( z_diff > 3096 )
		{
			z_diff = 3096;
		}

		int iBar = (int)( z_diff / 400 ) + 2;

		iBaseDotSize /= 2;
		
		// Draw a T shape to symbolize the dot is above the player.

		//horiz
		FillRect( x-(2*iBaseDotSize), y, 5*iBaseDotSize, iBaseDotSize );

		//vert
		FillRect( x, y, iBaseDotSize, iBar*iBaseDotSize );
	}
	else 
	{
		if ( flags & RADAR_DOT_HOSTAGE )
		{
			FillRect( x-1, y-1, iBaseDotSize+1, iBaseDotSize+1 );
		}
		else if ( flags & RADAR_DOT_BOMB )
		{
			if ( flags & RADAR_DOT_BOMB_PLANTED )
			{
				iBaseDotSize = 2;
				// draw an X for the planted bomb
				FillRect( x, y, iBaseDotSize, iBaseDotSize );
				FillRect( x-2, y-2, iBaseDotSize, iBaseDotSize );
				FillRect( x-2, y+2, iBaseDotSize, iBaseDotSize );	
				FillRect( x+2, y-2, iBaseDotSize, iBaseDotSize );
				FillRect( x+2, y+2, iBaseDotSize, iBaseDotSize );
			}
			else
			{
				FillRect( x-1, y-1, iBaseDotSize+1, iBaseDotSize+1 );
			}
		}
		else
		{
			FillRect( x, y, iBaseDotSize, iBaseDotSize );
		}
	}
}


void Radar_FlashPlayer( int iPlayer )
{
	if ( g_RadarFlashes.Count() <= iPlayer )
	{
		g_RadarFlashes.AddMultipleToTail( iPlayer - g_RadarFlashes.Count() + 1 );
	}

	CPlayerRadarFlash *pFlash = &g_RadarFlashes[iPlayer];
	pFlash->m_flNextRadarFlashTime = gpGlobals->curtime;
	pFlash->m_iNumRadarFlashes = 16;
	pFlash->m_bRadarFlash = false;

	g_pMapOverview->FlashEntity(iPlayer);
}

CON_COMMAND( drawradar, "Draws HUD radar" )
{
	(GET_HUDELEMENT( CHudRadar ))->DrawRadar();
}

CON_COMMAND( hideradar, "Hides HUD radar" )
{
	(GET_HUDELEMENT( CHudRadar ))->HideRadar();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Location text under radar

DECLARE_HUDELEMENT( CHudLocation );

CHudLocation::CHudLocation( const char *pName ) :	vgui::Label( NULL, "HudLocation", "" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );
}

void CHudLocation::Init()
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

void CHudLocation::LevelInit()
{
}

bool CHudLocation::ShouldDraw()
{
	CCSMapOverview *pCSMapOverview = (CCSMapOverview *)GET_HUDELEMENT( CCSMapOverview );

	if( g_pMapOverview && g_pMapOverview->GetMode() == CMapOverview::MAP_MODE_RADAR && pCSMapOverview && pCSMapOverview->ShouldDraw() == true )
		return true;
	else if( g_pMapOverview && g_pMapOverview->GetMode() == CMapOverview::MAP_MODE_INSET )	
		return true;

	return false;
}

void CHudLocation::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_fgColor = Color( 64, 255, 64, 255 );
	SetFont( pScheme->GetFont( "ChatFont" ) );
	SetBorder( NULL );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetFgColor( m_fgColor );
}

void CHudLocation::OnTick()
{
	m_fgColor[3] = cl_locationalpha.GetInt();
	SetFgColor( m_fgColor );

	const char *pszLocation = "";
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		pszLocation = pPlayer->GetLastKnownPlaceName();
	}
	SetText( g_pVGuiLocalize->Find( pszLocation ) );

	// We have two different locations based on the Overview mode.
	// So we just position ourselves below, and center our text in their width.
	if( g_pMapOverview )
	{
		int x = 0, y = 0;
		int width = 0, height = 0;
		g_pMapOverview->GetAsPanel()->GetPos( x, y );
		g_pMapOverview->GetAsPanel()->GetSize( width, height );
		y += g_pMapOverview->GetAsPanel()->GetTall();
		SetPos( x, y );
		SetWide( width );
	}
}
