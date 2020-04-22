//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "ihudlcd.h"
#include "vgui/ILocalize.h"

#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>

//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CHudAmmo : public CHudNumericDisplay, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudAmmo, CHudNumericDisplay );

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
	CHandle< C_BaseCombatWeapon > m_hCurrentActiveWeapon;
	int		m_iAmmo;
	int		m_iAmmo2;

	bool	m_bUsesClips;

	int		m_iAdditiveWhiteID;

	CPanelAnimationVarAliasType( float, digit_xpos, "digit_xpos", "50", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit_ypos, "digit_ypos", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit2_xpos, "digit2_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, digit2_ypos, "digit2_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_xpos, "bar_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_ypos, "bar_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_width, "bar_width", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, bar_height, "bar_height", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_xpos, "icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_ypos, "icon_ypos", "0", "proportional_float" );

	CPanelAnimationVar( Color, m_TextColor, "TextColor", "FgColor" );
	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudNumbers" );
};

DECLARE_HUDELEMENT( CHudAmmo );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudAmmo::CHudAmmo( const char *pElementName ) : BaseClass(NULL, "HudAmmo"), CHudElement( pElementName )
{
	m_iAdditiveWhiteID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iAdditiveWhiteID, "vgui/white_additive" , true, false);

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
		if (ammo == 0)
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("PrimaryAmmoEmpty");
		}
		else if (ammo < m_iAmmo)
		{
			// ammo has decreased
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("PrimaryAmmoDecrement");
		}
		else
		{
			// ammunition has increased
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("PrimaryAmmoIncrement");
		}

		m_iAmmo = ammo;
	}

	SetDisplayValue(ammo);
}

//-----------------------------------------------------------------------------
// Purpose: Updates 2nd ammo display
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo2(int ammo2, bool playAnimation)
{
	if (ammo2 != m_iAmmo2)
	{
		if (ammo2 == 0)
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SecondaryAmmoEmpty");
		}
		else if (ammo2 < m_iAmmo2)
		{
			// ammo has decreased
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SecondaryAmmoDecrement");
		}
		else
		{
			// ammunition has increased
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SecondaryAmmoIncrement");
		}

		m_iAmmo2 = ammo2;
	}
}

void CHudAmmo::Paint( void )
{
	float alpha = 1.0f;
	Color fgColor = GetFgColor();
	fgColor[3] *= alpha;
	SetFgColor( fgColor );

	int x, y;

	if( m_bUsesClips )
	{
		x = digit_xpos;
		y = digit_ypos;
	}
	else
	{
		x = digit2_xpos;
		y = digit2_ypos;
	}

	// Assume constant width font
	int charWidth = vgui::surface()->GetCharacterWidth( m_hNumberFont, '0' );

	int digits = clamp( log10((double)m_iAmmo)+1, 1, 3 );
	
	x += ( 3 - digits ) * charWidth;

	// draw primary ammo
	vgui::surface()->DrawSetTextColor(GetFgColor());
	PaintNumbers( m_hNumberFont, x, y, m_iAmmo );

	//draw reserve ammo
	if( m_bUsesClips )
	{
		//draw the divider
		Color c = GetFgColor();
		vgui::surface()->DrawSetColor(c);
		vgui::surface()->DrawSetTexture( m_iAdditiveWhiteID );
		vgui::surface()->DrawTexturedRect( bar_xpos, bar_ypos, bar_xpos + bar_width, bar_ypos + bar_height );

		digits = clamp( log10((double)m_iAmmo2)+1, 1, 3 );
		x = digit2_xpos + ( 3 - digits ) * charWidth;

		// draw secondary ammo
		vgui::surface()->DrawSetTextColor(GetFgColor());
		PaintNumbers( m_hNumberFont, x, digit2_ypos, m_iAmmo2 );
	}

	//draw the icon
	C_BaseCombatWeapon *wpn = GetActiveWeapon();
	if( wpn )
	{
		int ammoType = wpn->GetPrimaryAmmoType();

		CHudTexture *icon = gWR.GetAmmoIconFromWeapon( ammoType );

		if( icon )
		{
			float icon_tall = GetTall() - YRES(2);
			float scale = icon_tall / (float)icon->Height();
			float icon_wide = ( scale ) * (float)icon->Width();

			icon->DrawSelf( icon_xpos, icon_ypos, icon_wide, icon_tall, GetFgColor() );
		}
	}
}
