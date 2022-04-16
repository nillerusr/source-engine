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
#include "hl1_c_player.h"

#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>


#define MIN_ALPHA	 100	


class CHudFlashlight : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFlashlight, vgui::Panel );

public:
	CHudFlashlight( const char *pElementName );

private:
	void	Paint( void );
	void	ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHudTexture		*icon_flash_empty;
	CHudTexture		*icon_flash_full;
	CHudTexture		*icon_flash_beam;
	Color			m_clrReddish;
};

DECLARE_HUDELEMENT( CHudFlashlight );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudFlashlight::CHudFlashlight( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudFlashlight")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

void CHudFlashlight::Paint( void )
{
	int		r, g, b, a, nUnused;
	int		x, y;
	bool	bIsOn;
	Color	clrFlash;

	C_HL1_Player *pPlayer = ToHL1Player( C_HL1_Player::GetLocalPlayer() );
	if ( !pPlayer )
		return;

	if ( !icon_flash_empty )
	{
		icon_flash_empty = gHUD.GetIcon( "flash_empty" );
	}

	if ( !icon_flash_full )
	{
		icon_flash_full = gHUD.GetIcon( "flash_full" );
	}

	if ( !icon_flash_beam )
	{
		icon_flash_beam = gHUD.GetIcon( "flash_beam" );
	}

	if ( !icon_flash_empty || !icon_flash_full || !icon_flash_beam )
	{
		return;
	}

	bIsOn = pPlayer->IsEffectActive( EF_DIMLIGHT );

	if ( bIsOn )
		a = 225;
	else
		a = MIN_ALPHA;

	if ( pPlayer->m_nFlashBattery < 20 )
	{
		m_clrReddish.GetColor( r, g, b, nUnused );
	}
	else
	{
		(gHUD.m_clrYellowish).GetColor( r, g, b, nUnused );
	}

	clrFlash.SetColor( r, g, b, a );

	y = icon_flash_empty->Height() / 2;
	x = GetWide() - ( icon_flash_empty->Width() * 1.5 );

	// Draw the flashlight casing
	icon_flash_empty->DrawSelf( x, y, clrFlash );

	if ( bIsOn )
	{  // draw the flashlight beam
		x = GetWide() - icon_flash_empty->Width() / 2;

		icon_flash_beam->DrawSelf( x, y, clrFlash );
	}

	// draw the flashlight energy level
	x = GetWide() - ( icon_flash_empty->Width() * 1.5 );

	int nOffset = icon_flash_empty->Width() * ( 1.0 - ( (float)pPlayer->m_nFlashBattery / 100.0 ) );
	if ( nOffset < icon_flash_empty->Width() )
	{
		icon_flash_full->DrawSelfCropped( x + nOffset, y, nOffset, 0, icon_flash_full->Width() - nOffset, icon_flash_full->Height(), clrFlash );
	}
}

void CHudFlashlight::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	m_clrReddish = pScheme->GetColor( "Reddish", Color( 255, 16, 16, 255 ) );
}
