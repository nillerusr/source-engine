//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Target reticle hud element
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_basetfplayer.h"
#include "tf_shareddefs.h"
#include "iclientmode.h"
#include "clientmode_tfnormal.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "hud_targetreticle.h"
#include "model_types.h"
#include "view_scene.h"
#include "view.h"
#include <vgui/Cursor.h>
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTargetReticle::CTargetReticle( void )
: BaseClass( NULL, "CTargetReticle" ),
  m_CursorNone(vgui::dc_none)
{
	SetCursor( m_CursorNone );

	SetPaintBackgroundEnabled( false );
	SetAutoDelete( false );
	m_hTargetEntity = NULL;
	m_pTargetLabel = NULL;
	m_iReticleId = 0;
	m_iReticleLeftId = 0;
	m_iReticleRightId = 0;
	m_iRenderTextureId = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTargetReticle::~CTargetReticle()
{
	if ( m_pTargetLabel != NULL )
	{
		delete m_pTargetLabel;
		m_pTargetLabel = NULL;
	}

	SetParent( (vgui::Panel *)NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTargetReticle::Init( C_BaseEntity *pEntity, const char *sName )
{
	vgui::Panel *pParent = GetClientModeNormal()->GetViewport();
	SetParent( pParent );
	SetCursor( pParent->GetCursor() );

	if ( !m_pTargetLabel )
	{
		m_pTargetLabel = new vgui::Label( pParent, "TargetLabel", "Unnamed" );
		m_pTargetLabel->SetPos( 0, 0 );
		m_pTargetLabel->SetFgColor( Color( 255, 170, 0, 255 ) );
		m_pTargetLabel->SetPaintBackgroundEnabled( false );
		m_pTargetLabel->SetAutoDelete( false );
		m_pTargetLabel->SetCursor( m_CursorNone );
	}

	SetSize( XRES(32),YRES(32) );
	m_hTargetEntity = pEntity;
	m_pTargetLabel->SetText( sName );

	int contentW, contentH;
	m_pTargetLabel->GetContentSize( contentW, contentH );
	m_pTargetLabel->SetWide( contentW );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseEntity *CTargetReticle::GetTarget( void )
{
	return m_hTargetEntity;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTargetReticle::Update( void )
{
	if ( !m_hTargetEntity )
	{
		C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
		pPlayer->Remove_Target( this );
		return;
	}

	// Load our textures..
	if ( !m_iReticleId )
	{
		m_iReticleId = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iReticleId, "Hud/target_reticle" , true, false);
	}
	
	if ( !m_iReticleLeftId )
	{
		m_iReticleLeftId = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iReticleLeftId, "Hud/target_reticle_left", true, false );
	}
	
	if ( !m_iReticleRightId )
	{
		m_iReticleRightId = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iReticleRightId, "Hud/target_reticle_right" , true, false);
	}

	int iX, iY;
	GetTargetInScreenSpace( m_hTargetEntity, iX, iY );
	
	int halfWidth = GetWide() / 2;
	halfWidth = MAX( halfWidth, m_pTargetLabel->GetWide() / 2 );

	m_iRenderTextureId = m_iReticleId;
	if( iX < halfWidth || iX > ScreenWidth()-halfWidth )
	{
		// It's off the screen. See what side it's on.
		Vector vCenter = m_hTargetEntity->WorldSpaceCenter( );
		
		if( CurrentViewRight().Dot( vCenter - CurrentViewOrigin() ) > 0 )
		{
			m_iRenderTextureId = m_iReticleRightId;
			iX = ScreenWidth() - halfWidth;
		}
		else
		{
			m_iRenderTextureId = m_iReticleLeftId;
			iX = halfWidth;
		}

		// Put Y in the center of the screen.
		iY = ScreenHeight() / 2;
	}

	// Move the icon there
	SetPos( iX - (GetWide() / 2), iY - (GetTall() / 2) );

	// Center the text under it
	m_pTargetLabel->SetPos( iX - (m_pTargetLabel->GetWide() / 2), iY + (GetTall() / 2) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTargetReticle::Paint()
{
	if ( !m_hTargetEntity || !m_iRenderTextureId )
		return;

	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer == NULL || pPlayer->GetHealth() < 1 )
		return;

	// Show hide label based on EMP state
	bool suppress_reticle = pPlayer->HasPowerup(POWERUP_EMP);

	m_pTargetLabel->SetVisible( suppress_reticle ? false : true );

	// Don't draw the reticle either
	if ( suppress_reticle )
		return;

	vgui::surface()->DrawSetTexture( m_iRenderTextureId );
	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawTexturedRect( 0, 0, GetWide(), GetTall() );
}

