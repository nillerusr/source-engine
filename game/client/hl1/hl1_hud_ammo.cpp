//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "hl1_hud_numbers.h"

#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "ihudlcd.h"

#define MIN_ALPHA	100	


//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CHudAmmo : public CHudElement, public CHL1HudNumbers
{
	DECLARE_CLASS_SIMPLE( CHudAmmo, CHL1HudNumbers );

public:
	CHudAmmo( const char *pElementName );
	void	Init( void );
	void	VidInit( void );
	void	OnThink( void );

private:
	void	Paint( void );
	void	ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHandle<C_BaseCombatWeapon> m_hLastPickedUpWeapon;
	float	m_flFade;
};

DECLARE_HUDELEMENT( CHudAmmo );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudAmmo::CHudAmmo( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudAmmo")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT | HIDEHUD_WEAPONSELECTION );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAmmo::Init( void )
{
	m_hLastPickedUpWeapon	= NULL;
	m_flFade				= 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAmmo::VidInit( void )
{
	Reset();

	BaseClass::VidInit();
}

void CHudAmmo::OnThink( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// find and display our current selection
	C_BaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	if ( !pActiveWeapon )
	{
		m_hLastPickedUpWeapon = NULL;
		return;
	}

	if ( m_hLastPickedUpWeapon != pActiveWeapon )
	{
		m_flFade = 200.0f;
		m_hLastPickedUpWeapon = pActiveWeapon;
	}
}

void CHudAmmo::Paint( void )
{
	int r, g, b, a, nUnused;
	int x, y;
	Color clrAmmo;

	if (!ShouldDraw())
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	// find and display our current selection

	if ( !pPlayer || !pActiveWeapon )
	{
		hudlcd->SetGlobalStat( "(ammo_primary)", "n/a" );
		hudlcd->SetGlobalStat( "(ammo_secondary)", "n/a" );
		return;
	}

	hudlcd->SetGlobalStat( "(weapon_print_name)", pActiveWeapon ? pActiveWeapon->GetPrintName() : " " );
	hudlcd->SetGlobalStat( "(weapon_name)", pActiveWeapon ? pActiveWeapon->GetName() : " " );

	if ( ( pActiveWeapon->GetPrimaryAmmoType() == -1 ) && ( pActiveWeapon->GetSecondaryAmmoType() == -1 ) )
		return;

	int nFontWidth	= GetNumberFontWidth();
	int nFontHeight	= GetNumberFontHeight();

	a = (int)MAX( MIN_ALPHA, m_flFade );

	if ( m_flFade > 0 )
		m_flFade -= ( gpGlobals->frametime * 20 );

	(gHUD.m_clrYellowish).GetColor( r, g, b, nUnused );
	clrAmmo.SetColor( r, g, b, a );

	int nHudElemWidth, nHudElemHeight;
	GetSize( nHudElemWidth, nHudElemHeight );

	// Does this weapon have a clip?
	y = nHudElemHeight - ( nFontHeight * 1.5 );

	// Does weapon have any ammo at all?
	if ( pActiveWeapon->GetPrimaryAmmoType() != -1 )
	{
		CHudTexture	*icon_ammo = gWR.GetAmmoIconFromWeapon( pActiveWeapon->GetPrimaryAmmoType() );

		if ( !icon_ammo )
		{
			return;
		}

		int nIconWidth	= icon_ammo->Width();
		
		if ( pActiveWeapon->UsesClipsForAmmo1() )
		{
			// room for the number and the '|' and the current ammo
			
			x = nHudElemWidth - (8 * nFontWidth) - nIconWidth;
			x = DrawHudNumber( x, y, pActiveWeapon->Clip1(), clrAmmo );

			int nBarWidth = nFontWidth / 10;

			x += nFontWidth / 2;

			(gHUD.m_clrYellowish).GetColor( r, g, b, nUnused );
			clrAmmo.SetColor( r, g, b, a );

			// draw the | bar
			clrAmmo.SetColor( r, g, b, a  );
			vgui::surface()->DrawSetColor( clrAmmo );
			vgui::surface()->DrawFilledRect( x, y, x + nBarWidth, y + nFontHeight );

			x += nBarWidth + nFontWidth / 2;

			x = DrawHudNumber( x, y, pPlayer->GetAmmoCount( pActiveWeapon->GetPrimaryAmmoType() ), clrAmmo );		
		}
		else
		{
			// SPR_Draw a bullets only line
			x = nHudElemWidth - 4 * nFontWidth - nIconWidth;
			x = DrawHudNumber( x, y, pPlayer->GetAmmoCount( pActiveWeapon->GetPrimaryAmmoType() ), clrAmmo );
		}

		// Draw the ammo Icon
		icon_ammo->DrawSelf( x, y, clrAmmo );

		hudlcd->SetGlobalStat( "(ammo_primary)", VarArgs( "%d", pPlayer->GetAmmoCount( pActiveWeapon->GetPrimaryAmmoType() ) ) );
	}
	else
	{
		hudlcd->SetGlobalStat( "(ammo_primary)", "n/a" );
	}

	// Does weapon have seconday ammo?
	if ( pActiveWeapon->GetSecondaryAmmoType() != -1 )
	{
		CHudTexture	*icon_ammo = gWR.GetAmmoIconFromWeapon( pActiveWeapon->GetSecondaryAmmoType() );

		if ( !icon_ammo )
		{
			return;
		}

		int nIconWidth	= icon_ammo->Width();

		// Do we have secondary ammo?
		if ( pPlayer->GetAmmoCount( pActiveWeapon->GetSecondaryAmmoType() ) > 0 )
		{
			y -= ( nFontHeight * 1.25 );
			x = nHudElemWidth - 4 * nFontWidth - nIconWidth;
			x = DrawHudNumber( x, y, pPlayer->GetAmmoCount( pActiveWeapon->GetSecondaryAmmoType() ), clrAmmo );

			// Draw the ammo Icon
			icon_ammo->DrawSelf( x, y, clrAmmo );
		}

		hudlcd->SetGlobalStat( "(ammo_secondary)", VarArgs( "%d", pPlayer->GetAmmoCount( pActiveWeapon->GetSecondaryAmmoType() ) ) );
	}
	else
	{
		hudlcd->SetGlobalStat( "(ammo_secondary)", "n/a" );
	}

}

void CHudAmmo::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);
}
