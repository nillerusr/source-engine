//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ISurface.h>
#include "c_dod_player.h"
#include "clientmode_dod.h"
#include "dod_hud_tnt_pickup.h"
#include <vgui/ILocalize.h>

DECLARE_HUDELEMENT( CDODHudTNTPickupPanel );

ConVar hud_c4pickuppanel( "hud_c4pickuppanel", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Set to 0 to not draw the HUD c4 pickup panel" );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODHudTNTPickupPanel::CDODHudTNTPickupPanel( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTNTPickupPanel" ) 
{
	SetParent( g_pClientMode->GetViewport() );

	m_pBackground = new vgui::Panel( this, "CapturePanelBackground" );
	m_pTNTImage = new CIconPanel( this, "TNTImage" );

	m_pPickupLabel = new vgui::Label( this, "pickupLabel", "..." );

	// load control settings...
	LoadControlSettings( "resource/UI/HudTNTPickupPanel.res" );

	SetVisible( false );
	m_flShowUntilTime = 0;

	m_bInitLayout = true;

	RegisterForRenderGroup( "winpanel" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudTNTPickupPanel::Init()
{
	// listen for client side events
	ListenForGameEvent( "dod_tnt_pickup" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudTNTPickupPanel::VidInit()
{
	// listen for client side events
	m_flShowUntilTime = 0;

	m_bInitLayout = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudTNTPickupPanel::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	LoadControlSettings( "resource/UI/HudTNTPickupPanel.res" );

	m_bInitLayout = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudTNTPickupPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HudTNTPickupPanel.res" );
	m_bInitLayout = true;

	if ( m_pBackground )
	{
		m_pBackground->SetBgColor( GetSchemeColor( "HintMessageBg", pScheme ) );
		m_pBackground->SetPaintBackgroundType( 2 );
	}
}

void CDODHudTNTPickupPanel::FireGameEvent( IGameEvent *event )
{
	const char *pszEventName = event->GetName();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( !Q_strcmp( pszEventName, "dod_tnt_pickup" ) && pPlayer && pPlayer->ShouldShowHints() )
	{
		if ( hud_c4pickuppanel.GetBool() )
		{
			// fire the show animation
			SetVisible( true ); 
			m_flShowUntilTime = gpGlobals->curtime + 3.5;

			m_pTNTImage->SetVisible( true );
		}
	}
}

void CDODHudTNTPickupPanel::OnThink( void )
{
	BaseClass::OnThink();

	// if only vgui had relative layouts for elements..
	if ( m_bInitLayout )
	{
		// localize text if we need
		m_pPickupLabel->SetText( g_pVGuiLocalize->Find( "dod_tnt_pickup_help" ) );

		// size label to contents
		m_pPickupLabel->SizeToContents();

		int labelX, labelY, labelW, labelH;
		m_pPickupLabel->GetBounds( labelX, labelY, labelW, labelH );

		int imageX, imageY, imageH, imageW;
		m_pTNTImage->GetBounds( imageX, imageY, imageH, imageW );

		// total width is:
		// margin + image width + margin + text + margin
		int newWidth = 3 * XRES(10) + imageW + labelW;

		int bgX, bgY, bgW, bgH;
		m_pBackground->GetBounds( bgX, bgY, bgW, bgH );

		int newX = XRES(320) - newWidth/2;

		m_pBackground->SetBounds( newX, bgY, newWidth, bgH );

		m_pTNTImage->SetPos( newX + XRES(10), imageY );

		m_pPickupLabel->SetPos( newX + 2 * XRES(10) + imageW, bgY + ( bgH - labelY) / 2 );

		m_bInitLayout = false;
	}

	if ( IsVisible() && gpGlobals->curtime > m_flShowUntilTime )
	{
		SetVisible( false );
	}
}