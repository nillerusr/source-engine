//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: To display the player's health with the use of one graphic over another.  A cross in this case
//			Currently this is only used on the freeze cam panel
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cs_hud_freezepanel.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "cs_gamerules.h"

DECLARE_BUILD_FACTORY( CCSHudPlayerHealth );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSHudPlayerHealth::CCSHudPlayerHealth( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	m_pHealthImage = new CCSHealthPanel( this, "PlayerStatusHealthImage" );	
	m_pHealthImageBG = new ImagePanel( this, "PlayerStatusHealthImageBG" );

	m_flNextThink = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSHudPlayerHealth::Reset()
{
	//m_flNextThink = gpGlobals->curtime + 0.05f;
	m_nHealth = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSHudPlayerHealth::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( GetResFilename() );

	m_flNextThink = 0.0f;

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSHudPlayerHealth::SetHealth( int iNewHealth, int iMaxHealth, int	iMaxBuffedHealth )
{
	int nPrevHealth = m_nHealth;

	// set our health
	m_nHealth = iNewHealth;
	m_nMaxHealth = iMaxHealth;
	m_pHealthImage->SetHealth( (float)(m_nHealth) / (float)(m_nMaxHealth) );

	if ( m_pHealthImage )
	{
		m_pHealthImage->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	if ( m_nHealth <= 0 )
	{
		if ( m_pHealthImageBG->IsVisible() )
		{
			m_pHealthImageBG->SetVisible( false );
		}
	}
	else
	{
		if ( !m_pHealthImageBG->IsVisible() )
		{
			m_pHealthImageBG->SetVisible( true );
		}
	}

	// set our health display value
	if ( nPrevHealth != m_nHealth )
	{
		if ( m_nHealth > 0 )
		{
			SetDialogVariable( "Health", m_nHealth );
		}
		else
		{
			SetDialogVariable( "Health", "" );
		}	
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSHealthPanel::CCSHealthPanel( Panel *parent, const char *name ) : vgui::Panel( parent, name )
{
	m_flHealth = 1.0f;

	m_iMaterialIndex = surface()->DrawGetTextureId( "hud/health_color" );
	if ( m_iMaterialIndex == -1 ) // we didn't find it, so create a new one
	{
		m_iMaterialIndex = surface()->CreateNewTextureID();	
	}

	surface()->DrawSetTextureFile( m_iMaterialIndex, "hud/health_color", true, false );

	m_iDeadMaterialIndex = surface()->DrawGetTextureId( "hud/health_dead" );
	if ( m_iDeadMaterialIndex == -1 ) // we didn't find it, so create a new one
	{
		m_iDeadMaterialIndex = surface()->CreateNewTextureID();	
	}
	surface()->DrawSetTextureFile( m_iDeadMaterialIndex, "hud/health_dead", true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSHealthPanel::Paint()
{
	BaseClass::Paint();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	Vertex_t vert[4];	
	float uv1 = 0.0f;
	float uv2 = 1.0f;
	int xpos = 0, ypos = 0;

	if ( m_flHealth <= 0 )
	{
		// Draw the dead material
		surface()->DrawSetTexture( m_iDeadMaterialIndex );

		vert[0].Init( Vector2D( xpos, ypos ), Vector2D( uv1, uv1 ) );
		vert[1].Init( Vector2D( xpos + w, ypos ), Vector2D( uv2, uv1 ) );
		vert[2].Init( Vector2D( xpos + w, ypos + h ), Vector2D( uv2, uv2 ) );				
		vert[3].Init( Vector2D( xpos, ypos + h ), Vector2D( uv1, uv2 ) );

		surface()->DrawSetColor( Color(255,255,255,255) );
	}
	else
	{
		float flDamageY = h * ( 1.0f - m_flHealth );

		// blend in the red "damage" part
		surface()->DrawSetTexture( m_iMaterialIndex );

		Vector2D uv11( uv1, uv2 - m_flHealth );
		Vector2D uv21( uv2, uv2 - m_flHealth );
		Vector2D uv22( uv2, uv2 );
		Vector2D uv12( uv1, uv2 );

		vert[0].Init( Vector2D( xpos, flDamageY ), uv11 );
		vert[1].Init( Vector2D( xpos + w, flDamageY ), uv21 );
		vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
		vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

		surface()->DrawSetColor( GetFgColor() );
	}

	surface()->DrawTexturedPolygon( 4, vert );
}