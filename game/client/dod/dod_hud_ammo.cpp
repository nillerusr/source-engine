//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_bitmapnumericdisplay.h"
#include "iclientmode.h"
#include "c_dod_player.h"
#include "ihudlcd.h"

#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>

//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CHudAmmo : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudAmmo, vgui::Panel );

public:
	CHudAmmo( const char *pElementName );
	void Init( void );
	void VidInit( void );

	void SetAmmo(int ammo, bool playAnimation);
	void SetAmmo2(int ammo2, bool playAnimation);
		
protected:
	virtual void OnThink();
	virtual void Paint( void );
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	
private:
	void DrawText( char *text, int x, int y, Color clrText );
	void DrawNumbers( int num, int x, int y );

	void PaintGrenadeAmmo( CWeaponDODBase *pWpn );
	void PaintBazookaAmmo( CWeaponDODBase *pWpn );
	void PaintMGAmmo( CWeaponDODBase *pWpn );
	void PaintGunAmmo( CWeaponDODBase *pWpn );
	void PaintRifleGrenadeAmmo( CWeaponDODBase *pWpn );

	CHandle< C_BaseCombatWeapon > m_hCurrentActiveWeapon;
	int		m_iAmmo;
	int		m_iAmmo2;

	bool	m_bUsesClips;

	int		m_iAdditiveWhiteID;

	CPanelAnimationVarAliasType( float, digit2_xpos, "digit2_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit2_ypos, "digit2_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_xpos, "bar_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_ypos, "bar_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_width, "bar_width", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_height, "bar_height", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_xpos, "icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_ypos, "icon_ypos", "0", "proportional_float" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudSelectionNumbers" );

	Color m_clrIcon;

	CHudTexture *m_pMGNumbers[10];
};

//DECLARE_HUDELEMENT( CHudAmmo );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudAmmo::CHudAmmo( const char *pElementName ) : vgui::Panel( NULL, "HudAmmo" ), CHudElement( pElementName )
{
	SetParent( g_pClientMode->GetViewport() );

	m_iAdditiveWhiteID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iAdditiveWhiteID, "vgui/white_additive" , true, false);

	SetActive( true );

	m_clrIcon = Color(255,255,255,255);

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_WEAPONSELECTION );

	hudlcd->SetGlobalStat( "(ammo_primary)", "0" );
	hudlcd->SetGlobalStat( "(ammo_secondary)", "0" );
	hudlcd->SetGlobalStat( "(weapon_print_name)", "" );
	hudlcd->SetGlobalStat( "(weapon_name)", "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAmmo::Init( void )
{
	m_iAmmo		= -1;
	m_iAmmo2	= -1;
}

void CHudAmmo::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAmmo::VidInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: called every frame to get ammo info from the weapon
//-----------------------------------------------------------------------------
void CHudAmmo::OnThink()
{
	C_BaseCombatWeapon *wpn = GetActiveWeapon();

	hudlcd->SetGlobalStat( "(weapon_print_name)", wpn ? wpn->GetPrintName() : " " );
	hudlcd->SetGlobalStat( "(weapon_name)", wpn ? wpn->GetName() : " " );

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if (!wpn || !player || !wpn->UsesPrimaryAmmo())
	{
		hudlcd->SetGlobalStat( "(ammo_primary)", "n/a" );
		hudlcd->SetGlobalStat( "(ammo_secondary)", "n/a" );

		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
		return;
	}
	else
	{
		SetPaintEnabled(true);
		SetPaintBackgroundEnabled(true);
	}

	// get the ammo in our clip
	int ammo1 = wpn->Clip1();
	int ammo2;
	if (ammo1 < 0)
	{
		// we don't use clip ammo, just use the total ammo count
		ammo1 = player->GetAmmoCount(wpn->GetPrimaryAmmoType());
		ammo2 = 0;
	}
	else
	{
		// we use clip ammo, so the second ammo is the total ammo
		ammo2 = player->GetAmmoCount(wpn->GetPrimaryAmmoType());
	}

	hudlcd->SetGlobalStat( "(ammo_primary)", VarArgs( "%d", ammo1 ) );
	hudlcd->SetGlobalStat( "(ammo_secondary)", VarArgs( "%d", ammo2 ) );

	if (wpn == m_hCurrentActiveWeapon)
	{
		// same weapon, just update counts
		SetAmmo(ammo1, true);
		SetAmmo2(ammo2, true);
	}
	else
	{
		// diferent weapon, change without triggering
		SetAmmo(ammo1, false);
		SetAmmo2(ammo2, false);

		// update whether or not we show the total ammo display
		if (wpn->UsesClipsForAmmo1())
		{
			m_bUsesClips = true;

		}
		else
		{
			m_bUsesClips = false;
		}

		m_hCurrentActiveWeapon = wpn;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates ammo display
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo(int ammo, bool playAnimation)
{
	if (ammo != m_iAmmo)
	{
		m_iAmmo = ammo;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates 2nd ammo display
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo2(int ammo2, bool playAnimation)
{
	if (ammo2 != m_iAmmo2)
	{
		m_iAmmo2 = ammo2;
	}
}

void CHudAmmo::PaintGrenadeAmmo( CWeaponDODBase *pWpn )
{
	const CHudTexture *pAmmoIcon = pWpn->GetSpriteAmmo();

	Assert( pAmmoIcon );

	int x,y,w,h;
	GetBounds( x, y, w, h );

	if (m_iAmmo > 0 && pAmmoIcon )
	{
		int xpos = w - 2 * pAmmoIcon->Width();
		int ypos = h - pAmmoIcon->Height();

		pAmmoIcon->DrawSelf( xpos, ypos, pAmmoIcon->Width(), pAmmoIcon->Height(), m_clrIcon );

		char buf[16];
		Q_snprintf( buf, sizeof(buf), "x %d", m_iAmmo );

		DrawText( buf, xpos + pAmmoIcon->Width(), ypos + pAmmoIcon->Height() / 2, m_clrIcon );
	}
}

void CHudAmmo::PaintRifleGrenadeAmmo( CWeaponDODBase *pWpn )
{
	const CHudTexture *pAmmoIcon = pWpn->GetSpriteAmmo();

	Assert( pAmmoIcon );

	int x,y,w,h;
	GetBounds( x, y, w, h );

	int ammo = m_iAmmo + m_iAmmo2;

	if (ammo > 0 && pAmmoIcon )
	{
		int xpos = w - 2 * pAmmoIcon->Width();
		int ypos = h - pAmmoIcon->Height();

		pAmmoIcon->DrawSelf( xpos, ypos, pAmmoIcon->Width(), pAmmoIcon->Height(), m_clrIcon );

		char buf[16];
		Q_snprintf( buf, sizeof(buf), "x %d", ammo );

		DrawText( buf, xpos + pAmmoIcon->Width(), ypos + pAmmoIcon->Height() / 2, m_clrIcon );
	}
}

void CHudAmmo::PaintBazookaAmmo( CWeaponDODBase *pWpn )
{	
	const CHudTexture *pTubeIcon = pWpn->GetSpriteAmmo2();
	const CHudTexture *pRocketIcon = pWpn->GetSpriteAmmo();
	const CHudTexture *pExtraIcon = pWpn->GetSpriteAutoaim();

	Assert( pTubeIcon );
	Assert( pRocketIcon );
	Assert( pExtraIcon );

	int xpos = 0;
	int ypos = 0;

	int x, y, w, h;
	GetBounds(x,y,w,h);

	//Draw the rocket tube
	if( pTubeIcon )
	{
		xpos = w - 2 * pTubeIcon->Width();
		ypos = h - pTubeIcon->Height();
		pTubeIcon->DrawSelf( xpos, ypos, m_clrIcon );
	}

	//If our clip is full, draw the rocket
	if( pRocketIcon )
	{
		if( m_iAmmo > 0 )
			pRocketIcon->DrawSelf( xpos, ypos, m_clrIcon );

		xpos += pRocketIcon->Width() + 10;
		ypos += pRocketIcon->Height();
	}

	char buf[16];
	Q_snprintf( buf, sizeof(buf), "%d %d", m_iAmmo, m_iAmmo2 );
	DrawText( buf, xpos, ypos, m_clrIcon );

	//Draw the extra rockets
	if( m_iAmmo2 > 0 && pExtraIcon )
	{
		ypos -= pExtraIcon->Height();

		pExtraIcon->DrawSelf( xpos, ypos, m_clrIcon );

		xpos += pExtraIcon->Width();
		
		char buf[16];
		Q_snprintf( buf, sizeof(buf), "x %d", m_iAmmo2 );
		DrawText( buf, xpos, ypos + ( pExtraIcon->Height() * 0.75 ), m_clrIcon );
	}
}

void CHudAmmo::PaintMGAmmo( CWeaponDODBase *pWpn )
{
	const CHudTexture *pFullClip = pWpn->GetSpriteAmmo();
	const CHudTexture *pExtraClip = pWpn->GetSpriteAmmo2();

	int xpos = 0;
	int ypos = 0;
	
	int x, y, w, h;
	GetBounds(x,y,w,h);

	if( pFullClip )
	{
		xpos = w - pFullClip->Width() * 3;
		ypos = h - pFullClip->Height();
		pFullClip->DrawSelf( xpos, ypos, m_clrIcon );

		//Haxoration! The box that contains the numbers must be in the same position
		// in both the webley and mg34/mg42/30cal sprites.
		DrawNumbers( m_iAmmo, xpos + 36, ypos + pFullClip->Height() - 16 );
		
		xpos += pFullClip->Width();
		ypos += pFullClip->Height();
	}

	//how many full or partially full clips do we have?
	int clips = m_iAmmo2 / pWpn->GetMaxClip1();

	//account for the partial clip, if it exists
	if( clips * pWpn->GetMaxClip1() < m_iAmmo2 )
		clips++;

	if( pExtraClip && clips > 0 )
	{
		ypos -= pExtraClip->Height();

		pExtraClip->DrawSelf( xpos, ypos, m_clrIcon );

		char buf[16];
		Q_snprintf( buf, sizeof(buf), "x %d", clips );
		DrawText( buf, xpos + pExtraClip->Width(), ypos + pExtraClip->Height() / 2, m_clrIcon );
	}
}

void CHudAmmo::PaintGunAmmo( CWeaponDODBase *pWpn )
{
	//regular gun
	const CHudTexture *pEmptyClip = pWpn->GetSpriteAmmo();
	const CHudTexture *pFullClip = pWpn->GetSpriteAmmo2();
	const CHudTexture *pExtraClip = pWpn->GetSpriteAutoaim();

	Assert( pEmptyClip );
	Assert( pFullClip );
	Assert( pExtraClip );

	int x, y, w, h;
	GetBounds( x, y, w, h );

	int xpos = 0;
	int ypos = 0;

	if( pFullClip )
	{
		xpos = w - 3 * pFullClip->Width();
		ypos = h - pFullClip->Height() * 1.2;

		//Always draw the empty clip
		pFullClip->DrawSelf( xpos, ypos, Color(255,255,255,255) );
	}

	if( pEmptyClip )
	{
		// base percent is how much of the bullet clip to always draw.
		// total cropped height of the bullet sprite will be 
		// base percent + bullet height * bullets
		float flBasePercent			= (float)pWpn->GetDODWpnData().m_iHudClipBaseHeight / (float)pWpn->GetDODWpnData().m_iHudClipHeight;
		float flBulletHeightPercent = (float)pWpn->GetDODWpnData().m_iHudClipBulletHeight / (float)pWpn->GetDODWpnData().m_iHudClipHeight;

		float flHeight = (float)pEmptyClip->Height();

		//Now we draw the bullets inside based on how full our clip is
		float flDrawHeight = flHeight * ( 1.0 - ( flBasePercent + flBulletHeightPercent * m_iAmmo ) );

		int nOffset = (int)flDrawHeight;

		pEmptyClip->DrawSelfCropped( xpos, ypos + nOffset, 0, nOffset, pEmptyClip->Width(), pEmptyClip->Height() - nOffset, Color(255,255,255,255) );
		
		ypos += pEmptyClip->Height();

		xpos += pEmptyClip->Width() + 10;					
	}

	//how many full or partially full clips do we have?
	int clips = m_iAmmo2 / pWpn->GetMaxClip1();

	//account for the partial clip, if it exists
	if( clips * pWpn->GetMaxClip1() < m_iAmmo2 )
		clips++;

	if( pExtraClip && clips > 0 )
	{
		//align the extra clip on the same baseline as the large clip
		ypos -= pExtraClip->Height();

		pExtraClip->DrawSelf( xpos, ypos, Color(255,255,255,255) );

		char buf[16];
		Q_snprintf( buf, sizeof(buf), "x %d", clips );
		DrawText( buf, xpos + pExtraClip->Width(), ypos + pExtraClip->Height() / 2, m_clrIcon );
	}
}

void CHudAmmo::Paint( void )
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
		PaintGrenadeAmmo(pWpn);
		break;

	case WPN_TYPE_RIFLEGRENADE:
		PaintRifleGrenadeAmmo(pWpn);
		break;

	case WPN_TYPE_BAZOOKA:
		PaintBazookaAmmo(pWpn);
		break;

	case WPN_TYPE_MG:
		PaintMGAmmo(pWpn);
		break;

	default:
		PaintGunAmmo(pWpn);					
		break;
	}
}

void CHudAmmo::DrawText( char *text, int x, int y, Color clrText )
{
	vgui::surface()->DrawSetTextColor( clrText );
	vgui::surface()->DrawSetTextFont( m_hNumberFont );
	vgui::surface()->DrawSetTextPos( x, y );

	for (char *pch = text; *pch != 0; pch++)
	{
		vgui::surface()->DrawUnicodeChar(*pch);
	}
}

void CHudAmmo::DrawNumbers( int num, int x, int y )
{
	if( !m_pMGNumbers[0] )
	{
		int i;
		for( i=0;i<10;i++ )
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

