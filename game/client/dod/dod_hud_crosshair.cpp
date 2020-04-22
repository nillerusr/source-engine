//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "clientmode.h"
#include "c_dod_player.h"
#include "dod_hud_crosshair.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "mathlib/mathlib.h"

ConVar cl_crosshair_red( "cl_crosshair_red", "200", FCVAR_ARCHIVE );
ConVar cl_crosshair_green( "cl_crosshair_green", "200", FCVAR_ARCHIVE );
ConVar cl_crosshair_blue( "cl_crosshair_blue", "200", FCVAR_ARCHIVE );
ConVar cl_crosshair_alpha( "cl_crosshair_alpha", "200", FCVAR_ARCHIVE );

ConVar cl_crosshair_file( "cl_crosshair_file", "crosshair1", FCVAR_ARCHIVE );

ConVar cl_crosshair_scale( "cl_crosshair_scale", "32.0", FCVAR_ARCHIVE );
ConVar cl_crosshair_approach_speed( "cl_crosshair_approach_speed", "0.015" );

ConVar cl_dynamic_crosshair( "cl_dynamic_crosshair", "1", FCVAR_ARCHIVE );

using namespace vgui;

DECLARE_HUDELEMENT( CHudDODCrosshair );

CHudDODCrosshair::CHudDODCrosshair( const char *pName ) :
	vgui::Panel( NULL, "HudDODCrosshair" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	m_szPreviousCrosshair[0] = '\0';
	m_pFrameVar = NULL;
	m_flAccuracy = 0.1;
}

void CHudDODCrosshair::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetSize( ScreenWidth(), ScreenHeight() );
}

void CHudDODCrosshair::LevelShutdown( void )
{
	// forces m_pFrameVar to recreate next map
	m_szPreviousCrosshair[0] = '\0';

	if ( m_pCrosshair )
	{
		delete m_pCrosshair;
		m_pCrosshair = NULL;
	}

	if ( m_pFrameVar )
	{
		delete m_pFrameVar;
		m_pFrameVar = NULL;
	}
}

void CHudDODCrosshair::Init()
{
	m_iCrosshairTextureID = vgui::surface()->CreateNewTextureID();
}

bool CHudDODCrosshair::ShouldDraw()
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( !pPlayer )
		return false;

	CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();

	if ( !pWeapon )
		return false;

	return pWeapon->ShouldDrawCrosshair();
}

void CHudDODCrosshair::Paint()
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if( !pPlayer )
		return;

	const char *crosshairfile = cl_crosshair_file.GetString();

	if ( !crosshairfile )
		return;

	if ( Q_stricmp( m_szPreviousCrosshair, crosshairfile ) != 0 )
	{
		char buf[256];
		Q_snprintf( buf, sizeof(buf), "vgui/crosshairs/%s", crosshairfile );

		vgui::surface()->DrawSetTextureFile( m_iCrosshairTextureID, buf, true, false );

		if ( m_pCrosshair )
		{
			delete m_pCrosshair;
		}

		m_pCrosshair = vgui::surface()->DrawGetTextureMatInfoFactory( m_iCrosshairTextureID );

		if ( !m_pCrosshair )
			return;

		if ( m_pFrameVar )
		{
			delete m_pFrameVar;
		}

		bool bFound = false;
		m_pFrameVar = m_pCrosshair->FindVarFactory( "$frame", &bFound );
		Assert( bFound );

		m_nNumFrames = m_pCrosshair->GetNumAnimationFrames();

		// save the name to compare with the cvar in the future
		Q_strncpy( m_szPreviousCrosshair, crosshairfile, sizeof(m_szPreviousCrosshair) );
	}

	if ( m_pFrameVar )
	{	
		if ( cl_dynamic_crosshair.GetBool() == false )
		{
			m_pFrameVar->SetIntValue( 0 );
		}
		else
		{
			CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();

			if ( !pWeapon )
				return;

			float accuracy = pWeapon->GetWeaponAccuracy( pPlayer->GetAbsVelocity().Length2D() );

			float flMin = 0.02;

			float flMax = 0.125;

			accuracy = clamp( accuracy, flMin, flMax );

			// approach this accuracy from our current accuracy
			m_flAccuracy = Approach( accuracy, m_flAccuracy, cl_crosshair_approach_speed.GetFloat() );

			float flFrame = RemapVal( m_flAccuracy, flMin, flMax, 0, m_nNumFrames-1 );

			m_pFrameVar->SetIntValue( (int)flFrame );
		}
	}

	Color clr( cl_crosshair_red.GetInt(), cl_crosshair_green.GetInt(), cl_crosshair_blue.GetInt(), 255 );

	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);

	int iX = screenWide / 2;
	int iY = screenTall / 2;

	int iWidth, iHeight;

	iWidth = iHeight = cl_crosshair_scale.GetInt();

	vgui::surface()->DrawSetColor( clr );
	vgui::surface()->DrawSetTexture( m_iCrosshairTextureID );
	vgui::surface()->DrawTexturedRect( iX-iWidth, iY-iHeight, iX+iWidth, iY+iHeight );
	vgui::surface()->DrawSetTexture(0);
}

