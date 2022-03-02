//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
 //====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "clientmode_csnormal.h"
#include "cs_gamerules.h"
#include "hud_numericdisplay.h"


class CHudArmor : public CHudElement, public CHudNumericDisplay
{
public:
	DECLARE_CLASS_SIMPLE( CHudArmor, CHudNumericDisplay );

	CHudArmor( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void Init();
	virtual void ApplySchemeSettings( IScheme *scheme );

private:
	CHudTexture *m_pArmorIcon;
	CHudTexture *m_pArmor_HelmetIcon;

	CPanelAnimationVarAliasType( float, icon_xpos, "icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_ypos, "icon_ypos", "2", "proportional_float" );

	float icon_wide;
	float icon_tall;
};


DECLARE_HUDELEMENT( CHudArmor );


CHudArmor::CHudArmor( const char *pName ) : CHudNumericDisplay( NULL, "HudArmor" ), CHudElement( pName )
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}


void CHudArmor::Init()
{
	SetIndent(true);
}

void CHudArmor::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	if( !m_pArmorIcon )
	{
		m_pArmorIcon = gHUD.GetIcon( "shield_bright" );
	}

	if( !m_pArmor_HelmetIcon )
	{
		m_pArmor_HelmetIcon = gHUD.GetIcon( "shield_kevlar_bright" );
	}	

	if( m_pArmorIcon )
	{
		icon_tall = GetTall() - YRES(2);
		float scale = icon_tall / (float)m_pArmorIcon->Height();
		icon_wide = ( scale ) * (float)m_pArmorIcon->Width();
	}
}


bool CHudArmor::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		return !pPlayer->IsObserver();
	}
	else
	{
		return false;
	}
}


void CHudArmor::Paint()
{
	// Update the time.
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		if( pPlayer->HasHelmet() && (int)pPlayer->ArmorValue() > 0 )
		{
			if( m_pArmor_HelmetIcon )
			{
				m_pArmor_HelmetIcon->DrawSelf( icon_xpos, icon_ypos, icon_wide, icon_tall, GetFgColor() );
			}
		}
		else
		{
			if( m_pArmorIcon )
			{
				m_pArmorIcon->DrawSelf( icon_xpos, icon_ypos, icon_wide, icon_tall, GetFgColor() );
			}
		}

		SetDisplayValue( (int)pPlayer->ArmorValue() );
		SetShouldDisplayValue( true );
		BaseClass::Paint();
	}
}

