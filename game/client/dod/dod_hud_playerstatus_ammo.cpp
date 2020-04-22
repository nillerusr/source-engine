//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "iclientmode.h"
#include "c_dod_player.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>

#include "dod_hud_playerstatus_ammo.h"
#include "ihudlcd.h"

float GetScale( int nIconWidth, int nIconHeight, int nWidth, int nHeight )
{
	float flScale = 1.0;

	if ( nIconWidth == nWidth && nIconHeight <= nHeight ) // no scaling necessary
	{
		return flScale;
	}
	else if ( nIconHeight == nHeight && nIconWidth <= nWidth ) // no scaling necessary
	{
		return flScale;
	}
	else if ( nIconWidth < nWidth && nIconHeight < nHeight ) // scale the image up
	{
		float scaleW = 0.0, scaleH = 0.0;

		if ( nIconWidth < nWidth )
		{
			scaleW = (float)nWidth / (float)nIconWidth;
		}

		if ( nIconHeight < nHeight )
		{
			scaleH = (float)nHeight / (float)nIconHeight;
		}

		if ( scaleW != 0.0 && scaleH != 0.0 )
		{
			if ( scaleW < scaleH )
			{
				flScale = scaleW;
			}
			else
			{
				flScale = scaleH;
			}
		}
		else if ( scaleW != 0.0 )
		{
			flScale = scaleW;
		}
		else
		{
			flScale = scaleH;
		}
	}
	else // scale the image down
	{
		float scaleW = 0.0, scaleH = 0.0;

		if ( nIconWidth > nWidth )
		{
			scaleW = (float)nWidth / (float)nIconWidth;
		}

		if ( nIconHeight > nHeight )
		{
			scaleH = (float)nHeight / (float)nIconHeight;
		}

		if ( scaleW != 0.0 && scaleH != 0.0 )
		{
			if ( scaleW < scaleH )
			{
				flScale = scaleW;
			}
			else
			{
				flScale = scaleH;
			}
		}
		else if ( scaleW != 0.0 )
		{
			flScale = scaleW;
		}
		else
		{
			flScale = scaleH;
		}
	}

	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDoDHudAmmo::CDoDHudAmmo( vgui::Panel *parent, const char *name ) : vgui::Panel( parent, name )
{
	m_iAdditiveWhiteID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iAdditiveWhiteID, "vgui/white_additive", true, false );

	m_clrIcon = Color( 255, 255, 255, 255 );

	hudlcd->SetGlobalStat( "(ammo_primary)", "0" );
	hudlcd->SetGlobalStat( "(ammo_secondary)", "0" );
	hudlcd->SetGlobalStat( "(weapon_print_name)", "" );
	hudlcd->SetGlobalStat( "(weapon_name)", "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::Init( void )
{
	m_iAmmo = -1;
	m_iAmmo2 = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrTextColor = pScheme->GetColor( "HudAmmoCount", GetFgColor() );
	m_clrTextXColor = pScheme->GetColor( "HudPanelBorder", GetFgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: called every frame to get ammo info from the weapon
//-----------------------------------------------------------------------------
void CDoDHudAmmo::OnThink()
{
	C_BaseCombatWeapon *wpn = GetActiveWeapon();
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	hudlcd->SetGlobalStat( "(weapon_print_name)", wpn ? wpn->GetPrintName() : " " );
	hudlcd->SetGlobalStat( "(weapon_name)", wpn ? wpn->GetName() : " " );

    if ( !wpn || !player || !wpn->UsesPrimaryAmmo() )
	{
		hudlcd->SetGlobalStat( "(ammo_primary)", "n/a" );
		hudlcd->SetGlobalStat( "(ammo_secondary)", "n/a" );

		SetPaintEnabled( false );
		SetPaintBackgroundEnabled( false );
		return;
	}
	else
	{
		SetPaintEnabled( true );
		SetPaintBackgroundEnabled( true );
	}

	// get the ammo in our clip
	int ammo1 = wpn->Clip1();
	int ammo2;
	if ( ammo1 < 0 )
	{
		// we don't use clip ammo, just use the total ammo count
		ammo1 = player->GetAmmoCount( wpn->GetPrimaryAmmoType() );
		ammo2 = 0;
	}
	else
	{
		// we use clip ammo, so the second ammo is the total ammo
		ammo2 = player->GetAmmoCount( wpn->GetPrimaryAmmoType() );
	}

	hudlcd->SetGlobalStat( "(ammo_primary)", VarArgs( "%d", ammo1 ) );
	hudlcd->SetGlobalStat( "(ammo_secondary)", VarArgs( "%d", ammo2 ) );

	if ( wpn == m_hCurrentActiveWeapon )
	{
		// same weapon, just update counts
		SetAmmo( ammo1, true );
		SetAmmo2( ammo2, true );
	}
	else
	{
		// diferent weapon, change without triggering
		SetAmmo( ammo1, false );
		SetAmmo2( ammo2, false );

		// update whether or not we show the total ammo display
		m_bUsesClips = wpn->UsesClipsForAmmo1();
		m_hCurrentActiveWeapon = wpn;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates ammo display
//-----------------------------------------------------------------------------
void CDoDHudAmmo::SetAmmo( int ammo, bool playAnimation )
{
	if ( ammo != m_iAmmo )
	{
		m_iAmmo = ammo;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates 2nd ammo display
//-----------------------------------------------------------------------------
void CDoDHudAmmo::SetAmmo2( int ammo2, bool playAnimation )
{
	if ( ammo2 != m_iAmmo2 )
	{
		m_iAmmo2 = ammo2;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::DrawAmmoCount( int count )
{
	char buf[16];
	Q_snprintf( buf, sizeof(buf), "x" );
	DrawText( buf, clip_count_text_xpos, clip_count_text_ypos, m_clrTextXColor );

	Q_snprintf( buf, sizeof(buf), "   %d", count );
	DrawText( buf, clip_count_text_xpos, clip_count_text_ypos, m_clrTextColor );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::PaintGrenadeAmmo( CWeaponDODBase *pWpn )
{
	const CHudTexture *pAmmoIcon = pWpn->GetSpriteAmmo();

	Assert( pAmmoIcon );

	int xpos = small_icon_xpos, ypos = small_icon_ypos;
	int w = small_icon_width, t = small_icon_height;

	int nIconWidth = 0, nIconHeight = 0;
	float scale = 1.0f;

	if ( pAmmoIcon )
	{
		nIconWidth = pAmmoIcon->Width();
		nIconHeight = pAmmoIcon->Height();

		scale = GetScale( nIconWidth, nIconHeight, w, t );

		nIconWidth *= scale;
		nIconHeight *= scale;

		if ( nIconWidth < small_icon_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the xpos
		{
			xpos = small_icon_xpos + small_icon_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < small_icon_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = small_icon_ypos + small_icon_height / 2.0 - nIconHeight / 2.0;
		}

		if ( m_iAmmo > 0 )
		{
			pAmmoIcon->DrawSelf( xpos, ypos, nIconWidth, nIconHeight, m_clrIcon );
			DrawAmmoCount( m_iAmmo );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::PaintRifleGrenadeAmmo( CWeaponDODBase *pWpn )
{
	const CHudTexture *pAmmoIcon = pWpn->GetSpriteAmmo();

	Assert( pAmmoIcon );

	int xpos = small_icon_xpos, ypos = small_icon_ypos;
	int w = small_icon_width, t = small_icon_height;

	int nIconWidth = 0, nIconHeight = 0;
	float scale = 1.0f;

	if ( pAmmoIcon )
	{
		nIconWidth = pAmmoIcon->Width();
		nIconHeight = pAmmoIcon->Height();

		scale = GetScale( nIconWidth, nIconHeight, w, t );

		nIconWidth *= scale;
		nIconHeight *= scale;

		if ( nIconWidth < small_icon_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the xpos
		{
			xpos = small_icon_xpos + small_icon_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < small_icon_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = small_icon_ypos + small_icon_height / 2.0 - nIconHeight / 2.0;
		}

		int ammo = m_iAmmo + m_iAmmo2;

		if ( ammo > 0 )
		{
			pAmmoIcon->DrawSelf( xpos, ypos, nIconWidth, nIconHeight, m_clrIcon );
			DrawAmmoCount( ammo );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::PaintBazookaAmmo( CWeaponDODBase *pWpn )
{	
	int panelX, panelY, panelW, panelT;
	GetBounds( panelX, panelY, panelW, panelT );

	const CHudTexture *pTubeIcon = pWpn->GetSpriteAmmo2();
	const CHudTexture *pRocketIcon = pWpn->GetSpriteAmmo();
	const CHudTexture *pExtraIcon = pWpn->GetSpriteAutoaim();

	Assert( pTubeIcon );
	Assert( pRocketIcon );
	Assert( pExtraIcon );

	int xpos = 0, ypos = 0;
	int nIconWidth = 0, nIconHeight = 0;
	float scale = 1.0f;

	if ( pTubeIcon && pRocketIcon )
	{
		nIconWidth = pTubeIcon->Width();
		nIconHeight = pTubeIcon->Height();

		xpos = large_icon_xpos;
		ypos = large_icon_ypos;

		// mad hax
		int width = large_icon_width + XRES(10);

		scale = GetScale( nIconWidth, nIconHeight, width, large_icon_height );

		nIconWidth *= scale;
		nIconHeight *= scale;

		if ( nIconWidth < large_icon_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the xpos
		{
			xpos = small_icon_xpos + small_icon_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < large_icon_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = small_icon_ypos + small_icon_height / 2.0 - nIconHeight / 2.0;
		}

		pTubeIcon->DrawSelf( xpos, ypos, nIconWidth, nIconHeight, m_clrIcon );

		// If our clip is full, draw the rocket
		if( pRocketIcon )
		{
			if( m_iAmmo > 0 )
			{
				pRocketIcon->DrawSelf( xpos, ypos, nIconWidth, nIconHeight, m_clrIcon );
			}
		}
	}

	// Draw the extra rockets
	if( m_iAmmo2 > 0 && pExtraIcon )
	{
		// Align the extra clip on the same baseline as the large clip
		xpos = extra_clip_xpos;
		ypos = extra_clip_ypos;

		nIconWidth = pExtraIcon->Width();
		nIconHeight = pExtraIcon->Height();

		if ( nIconWidth > extra_clip_width || nIconHeight > extra_clip_height )
		{
			scale = GetScale( nIconWidth, nIconHeight, extra_clip_width, extra_clip_height );
			nIconWidth *= scale;
			nIconHeight *= scale;
		}

		if ( nIconWidth < extra_clip_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			xpos = extra_clip_xpos + extra_clip_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < extra_clip_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = extra_clip_ypos + extra_clip_height / 2.0 - nIconHeight / 2.0;
		}

		pExtraIcon->DrawSelf( xpos, ypos, pExtraIcon->Width() * scale, pExtraIcon->Height() * scale, m_clrIcon );
		DrawAmmoCount( m_iAmmo2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::PaintMGAmmo( CWeaponDODBase *pWpn )
{
	int panelX, panelY, panelW, panelT;
	GetBounds( panelX, panelY, panelW, panelT );

	const CHudTexture *pFullClip = pWpn->GetSpriteAmmo();
	const CHudTexture *pExtraClip = pWpn->GetSpriteAmmo2();

	Assert( pFullClip );
	Assert( pExtraClip );

	int xpos = 0, ypos = 0;
	int nIconWidth = 0, nIconHeight = 0;
	float scale = 1.0f;

	if ( pFullClip )
	{
		nIconWidth = pFullClip->Width();
		nIconHeight = pFullClip->Height();

		xpos = large_icon_xpos;
		ypos = large_icon_ypos;

		scale = GetScale( nIconWidth, nIconHeight, large_icon_width, large_icon_height );

		nIconWidth *= scale;
		nIconHeight *= scale;

		if ( nIconWidth < large_icon_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the xpos
		{
			xpos = small_icon_xpos + small_icon_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < large_icon_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = small_icon_ypos + small_icon_height / 2.0 - nIconHeight / 2.0;
		}

		pFullClip->DrawSelf( xpos, ypos, nIconWidth, nIconHeight, m_clrIcon );

		char buf[16];
		Q_snprintf( buf, sizeof(buf), "%d", m_iAmmo );
		DrawText( buf, xpos + nIconWidth - ( (float)nIconWidth / 3.0 ), ypos + nIconHeight - ( (float)nIconHeight / 3.0 ), m_clrTextColor );
	}

	//how many full or partially full clips do we have?
	int clips = m_iAmmo2 / pWpn->GetMaxClip1();

	//account for the partial clip, if it exists
	if( clips * pWpn->GetMaxClip1() < m_iAmmo2 )
	{
		clips++;
	}

	if( clips > 0 && pExtraClip )
	{
		//align the extra clip on the same baseline as the large clip
		xpos = extra_clip_xpos;
		ypos = extra_clip_ypos;

		nIconWidth = pExtraClip->Width();
		nIconHeight = pExtraClip->Height();

		if ( nIconWidth > extra_clip_width || nIconHeight > extra_clip_height )
		{
			scale = GetScale( nIconWidth, nIconHeight, extra_clip_width, extra_clip_height );
			nIconWidth *= scale;
			nIconHeight *= scale;
		}

		if ( nIconWidth < extra_clip_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			xpos = extra_clip_xpos + extra_clip_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < extra_clip_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = extra_clip_ypos + extra_clip_height / 2.0 - nIconHeight / 2.0;
		}

		pExtraClip->DrawSelf( xpos, ypos, pExtraClip->Width() * scale, pExtraClip->Height() * scale, m_clrIcon );
		DrawAmmoCount( clips );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::PaintGunAmmo( CWeaponDODBase *pWpn )
{
	int panelX, panelY, panelW, panelT;
	GetBounds( panelX, panelY, panelW, panelT );

	//regular gun
	const CHudTexture *pEmptyClip = pWpn->GetSpriteAmmo();
	const CHudTexture *pFullClip = pWpn->GetSpriteAmmo2();
	const CHudTexture *pExtraClip = pWpn->GetSpriteAutoaim();

	Assert( pEmptyClip );
	Assert( pFullClip );
	Assert( pExtraClip );

	int xpos = 0, ypos = 0;
	int nIconWidth = 0, nIconHeight = 0;
	float scale = 1.0f;
	
	if ( pFullClip && pEmptyClip )
	{
		nIconWidth = pFullClip->Width();
		nIconHeight = pFullClip->Height();

		xpos = large_icon_xpos;
		ypos = large_icon_ypos;

		scale = GetScale( nIconWidth, nIconHeight, large_icon_width, large_icon_height );

		nIconWidth *= scale;
		nIconHeight *= scale;

		if ( nIconWidth < large_icon_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the xpos
		{
			xpos = small_icon_xpos + small_icon_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < large_icon_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = small_icon_ypos + small_icon_height / 2.0 - nIconHeight / 2.0;
		}

		pFullClip->DrawSelf( xpos, ypos, nIconWidth, nIconHeight, m_clrIcon );

		// base percent is how much of the bullet clip to always draw.
		// total cropped height of the bullet sprite will be 
		// base percent + bullet height * bullets
		float flBasePercent			= (float)pWpn->GetDODWpnData().m_iHudClipBaseHeight / (float)pWpn->GetDODWpnData().m_iHudClipHeight;
		float flBulletHeightPercent = (float)pWpn->GetDODWpnData().m_iHudClipBulletHeight / (float)pWpn->GetDODWpnData().m_iHudClipHeight;

		float flHeight = (float)pEmptyClip->Height();

		//Now we draw the bullets inside based on how full our clip is
		float flDrawHeight = flHeight * ( 1.0 - ( flBasePercent + flBulletHeightPercent * m_iAmmo ) );

		int nOffset = (int)flDrawHeight;
		int yPosOffset = nOffset * scale;

		pEmptyClip->DrawSelfCropped( xpos, ypos + yPosOffset, 0, nOffset, pEmptyClip->Width(), pEmptyClip->Height() - nOffset, nIconWidth, nIconHeight - yPosOffset, m_clrIcon );
	}

	// how many full or partially full clips do we have?
	int clips = m_iAmmo2 / pWpn->GetMaxClip1();

	// account for the partial clip, if it exists
	if( clips * pWpn->GetMaxClip1() < m_iAmmo2 )
	{
		clips++;
	}

	if( clips > 0 && pExtraClip )
	{
		//align the extra clip on the same baseline as the large clip
		xpos = extra_clip_xpos;
		ypos = extra_clip_ypos;

		nIconWidth = pExtraClip->Width();
		nIconHeight = pExtraClip->Height();

		if ( nIconWidth > extra_clip_width || nIconHeight > extra_clip_height )
		{
			scale = GetScale( nIconWidth, nIconHeight, extra_clip_width, extra_clip_height );
			nIconWidth *= scale;
			nIconHeight *= scale;
		}

		if ( nIconWidth < extra_clip_width - XRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			xpos = extra_clip_xpos + extra_clip_width / 2.0 - nIconWidth / 2.0;
		}

		if ( nIconHeight < extra_clip_height - YRES(2) ) // 2 is our buffer for when we need to re-calculate the ypos
		{
			ypos = extra_clip_ypos + extra_clip_height / 2.0 - nIconHeight / 2.0;
		}
	
		pExtraClip->DrawSelf( xpos, ypos, pExtraClip->Width() * scale, pExtraClip->Height() * scale, m_clrIcon );
		DrawAmmoCount( clips );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::Paint( void )
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if( !pPlayer )
		return;

	CWeaponDODBase *pWpn = pPlayer->GetActiveDODWeapon();

	if( !pWpn )
		return;
	
	switch( pWpn->GetDODWpnData().m_WeaponType )
	{
	case WPN_TYPE_GRENADE:
		PaintGrenadeAmmo( pWpn );
		break;

	case WPN_TYPE_RIFLEGRENADE:
		PaintRifleGrenadeAmmo( pWpn );
		break;

	case WPN_TYPE_BAZOOKA:
		PaintBazookaAmmo( pWpn );
		break;

	case WPN_TYPE_MG:
		PaintMGAmmo( pWpn );
		break;

	case WPN_TYPE_CAMERA:
		break;

	default:
		PaintGunAmmo( pWpn );					
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::DrawText( char *text, int x, int y, Color clrText )
{
	vgui::surface()->DrawSetTextColor( clrText );
	vgui::surface()->DrawSetTextFont( m_hNumberFont );
	vgui::surface()->DrawSetTextPos( x, y );

	for (char *pch = text; *pch != 0; pch++)
	{
		vgui::surface()->DrawUnicodeChar(*pch);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudAmmo::DrawNumbers( int num, int x, int y )
{
	if ( !m_pMGNumbers[0] )
	{
		int i;
		for ( i = 0 ; i < 10 ; i++ )
		{
			char buf[8];
			Q_snprintf( buf, sizeof(buf), "mg_%d", i );
			m_pMGNumbers[i] = gHUD.GetIcon( buf );
		}
	}

	Assert( num < 1000 );

	int xpos = x;
	int ypos = y;
	int num_working = num;

	int iconWidth = m_pMGNumbers[0]->Width();

	int hundreds = num_working / 100;
	num_working -= hundreds * 100;

	m_pMGNumbers[hundreds]->DrawSelf( xpos, ypos, m_clrIcon );
	xpos += iconWidth;

	int tens = num_working / 10;
	num_working -= tens * 10;

	m_pMGNumbers[tens]->DrawSelf( xpos, ypos, m_clrIcon );
	xpos += iconWidth;

	m_pMGNumbers[num_working]->DrawSelf( xpos, ypos, m_clrIcon );
	xpos += iconWidth;
}

