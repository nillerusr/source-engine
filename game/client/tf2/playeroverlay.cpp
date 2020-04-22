//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "playeroverlay.h"
#include "playeroverlayhealth.h"
#include "playeroverlayname.h"
#include "playeroverlayclass.h"
#include "playeroverlayselected.h"
#include "playeroverlaysquad.h"
#include "hud_minimap.h"
#include "c_basetfplayer.h"
#include "mapdata.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "commanderoverlay.h"
#include <KeyValues.h>
#include <vgui/IVGui.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------

DECLARE_OVERLAY_FACTORY( CHudPlayerOverlay, "player" );


// THE ACTUAL OVERLAY

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CHudPlayerOverlay::CHudPlayerOverlay( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, "CHudPlayerOverlay" )
{
	m_pHealth	= 0;
	m_pName		= 0;
	m_pClass	= 0;
	m_pSquad	= 0;
	m_pSelected = 0;

	SetAutoDelete( false );

	m_hPlayer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CHudPlayerOverlay::~CHudPlayerOverlay( void )
{
	Clear();
}

void CHudPlayerOverlay::Clear( void )
{
	delete m_pHealth;
	delete m_pName;
	delete m_pClass;
	delete m_pSelected;
	delete m_pSquad;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudPlayerOverlay::Init( KeyValues* pInitData, C_BaseEntity* pEntity )
{
	Clear();

	m_hPlayer = dynamic_cast< C_BaseTFPlayer * >( pEntity );
	if (!m_hPlayer.Get())
		return false;

	if (!BaseClass::Init( pInitData, pEntity ))
		return false;

	if (!ParseRGBA(pInitData, "fgcolor", m_fgColor ))
		return false;

	if (!ParseRGBA(pInitData, "bgcolor", m_bgColor ))
		return false;

	if (!ParseCoord(pInitData, "offset", m_OffsetX, m_OffsetY ))
		return false;

	int w, h;
	if (!ParseCoord(pInitData, "size", w, h ))
		return false;

	SetSize( w, h );
	SetVisible( false );

	m_iOrgWidth = w;
	m_iOrgHeight = h;
	m_iOrgOffsetX = m_OffsetX;
	m_iOrgOffsetY = m_OffsetY;

	const char *mouseover = pInitData->GetString( "mousehint", "" );
	if ( mouseover && mouseover[ 0 ] )
	{
		Q_strncpy( m_szMouseOverText, mouseover, sizeof( m_szMouseOverText ) );
	}

	m_pHealth = new CHudPlayerOverlayHealth( this );
	KeyValues* pHealthValue = pInitData->FindKey("HealthBar");
	if (!m_pHealth->Init( pHealthValue ))
		return false;

	m_pHealth->SetVisible( false );
	m_pHealth->SetParent( this );

	m_pName = new CHudPlayerOverlayName( this, "" );
	KeyValues* pNameValue = pInitData->FindKey("Name");
	if (!m_pName->Init( pNameValue ))
		return false;

	m_pName->SetVisible( false );
	m_pName->SetParent( this );

	m_pClass = new CHudPlayerOverlayClass( this );
	KeyValues* pClassValue = pInitData->FindKey("Class");
	if (!m_pClass->Init( pClassValue ))
		return false;

	m_pClass->SetVisible( false );
	m_pClass->SetParent( this );

	m_pSquad = new CHudPlayerOverlaySquad( this, "" );
	KeyValues* pSquadValue = pInitData->FindKey("Squad");
	if (!m_pSquad->Init( pSquadValue ))
		return false;

	m_pSquad->SetVisible( false );
	m_pSquad->SetParent( this );

	m_pSelected = new CHudPlayerOverlaySelected( this );
	KeyValues* pSelectedValue = pInitData->FindKey("Selection");
	if (!m_pSelected->Init( pSelectedValue ))
		return false;

	m_pSelected->SetVisible( false );
	m_pSelected->SetParent( this );

	// Um, is there a better way?
	m_PlayerNum = GetEntity() ? GetEntity()->index : 0;
	if (m_PlayerNum == 0)
		return false;

	return true;
}

void CHudPlayerOverlay::Hide( void )
{
	SetVisible( false );
	if ( m_pHealth )
	{
		m_pHealth->SetVisible( false);
	}

	if ( m_pName )
	{
		m_pName->SetVisible( false );
	}

	if ( m_pClass )
	{
		m_pClass->SetVisible( false );
	}

	if ( m_pSquad )
	{
		m_pSquad->SetVisible( false );
	}

	if ( m_pSelected )
	{
		m_pSelected->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerNum - 
//-----------------------------------------------------------------------------

void CHudPlayerOverlay::OnTick( )
{
	BaseClass::OnTick();

	if (!IsLocalPlayerInTactical() || !engine->IsInGame())
	{
		Hide();
		return;
	}

	// Don't draw if I'm not visible in the tactical map
	if ( MapData().IsEntityVisibleToTactical( GetEntity() ) == false )
	{
		Hide();
		return;
	}

	// Don't draw it if I'm on team 0 (haven't decided on a team)
	C_BaseTFPlayer *pPlayer = m_hPlayer.Get();
	if (!pPlayer || (pPlayer->GetTeamNumber() == 0) || (pPlayer->GetClass() == TFCLASS_UNDECIDED))
	{
		Hide();
		return;
	}

	SetVisible( true );

	const char *pName = g_PR->GetPlayerName( m_PlayerNum );
	if ( pName )
	{
		m_pName->SetName( pName );
	}
	else
	{
		Hide();
		return;
	}

	Vector pos, screen;

	C_BaseTFPlayer *tf2player = dynamic_cast<C_BaseTFPlayer *>( GetEntity() );
	int iteam = 0;
	int iclass = 0;
	if ( tf2player )
	{
		iteam	= tf2player->GetTeamNumber();
		iclass	= tf2player->PlayerClass();

		// FIXME: Get max health for player
		m_pHealth->SetHealth( (float)tf2player->GetHealth() / (float)100.0f );
	}

	m_pClass->SetImage( 0 );
	if ( iteam != 0 && iclass != TFCLASS_UNDECIDED )
		m_pClass->SetTeamAndClass( iteam, iclass );

	// Update our position on screen
	int sx, sy;
	GetEntityPosition( sx, sy );

	// Set the position
	SetPos( (int)(sx + m_OffsetX + 0.5f), (int)(sy + m_OffsetY + 0.5f));

	// Add it in
	m_pHealth->SetVisible( true );
	m_pName->SetVisible( true );
	m_pClass->SetVisible( true );

	if ( MapData().m_Players[ m_PlayerNum - 1 ].m_bSelected )
	{
		m_pSelected->SetVisible( true );
	}
	else
	{
		m_pSelected->SetVisible( false );
	}

	if ( MapData().m_Players[ m_PlayerNum - 1 ].m_nSquadNumber != 0 )
	{
		char sz[ 32 ];
		Q_snprintf( sz, sizeof( sz ), "%i", MapData().m_Players[ m_PlayerNum - 1 ].m_nSquadNumber );

		m_pSquad->SetSquad( sz );
		m_pSquad->SetVisible( true );
	}
	else
	{
		m_pSquad->SetVisible( false );
	}

	// Hide details if it's an enemy
	if ( ArePlayersOnSameTeam( engine->GetLocalPlayer(), m_PlayerNum ) == false )
	{
		m_pHealth->SetVisible( false );
		m_pName->SetVisible( false );
		m_pSelected->SetVisible( false );
		m_pSquad->SetVisible( false );

		// Only show class icon
		m_pClass->SetVisible( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPlayerOverlay::Paint()
{
	// Don't draw if I'm not visible in the tactical map
	if ( MapData().IsEntityVisibleToTactical( GetEntity() ) == false )
		return;

	// Don't draw if if I haven't chosen a class...

	SetFgColor( m_fgColor );
	SetBgColor( m_bgColor );

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//			fg - 
//			bg - 
//-----------------------------------------------------------------------------
void CHudPlayerOverlay::SetColorLevel( vgui::Panel *panel, Color& fg, Color& bg )
{
	float frac = GetAlphaFrac();

	if ( frac == 1.0f )
	{
		panel->SetFgColor( fg );
		panel->SetBgColor( bg );
		return;
	}

	Color foreground;
	Color background;

	foreground = fg;
	background = bg;

	int r, g, b, a;
	foreground.GetColor( r, g, b, a );
	foreground.SetColor( r, g, b, (int)( ( float )a * frac ) );

	panel->SetFgColor( foreground );

	background.GetColor( r, g, b, a );
	background.SetColor( r, g, b, (int)( ( float )a * frac ) );

	panel->SetBgColor( background );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
float CHudPlayerOverlay::GetAlphaFrac( void )
{
	//
	// return fmod( gpGlobals->curtime, 1.0f );

	C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
	if ( !local )
		return 1.0;

	C_BaseTFPlayer *pPlayer = m_hPlayer.Get();
	if (!pPlayer || (pPlayer->GetTeamNumber() == local->GetTeamNumber()) )
		return 1.0f;

	return pPlayer->GetOverlayAlpha();
}
