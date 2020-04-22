//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Panel.h>

using namespace vgui;

#include "hudelement.h"
#include "hl1_hud_numbers.h"

#include "convar.h"

#define FADE_TIME	100
#define MIN_ALPHA	100	

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CHudBattery : public CHudElement, public CHL1HudNumbers
{
	DECLARE_CLASS_SIMPLE( CHudBattery, CHL1HudNumbers );

public:
	CHudBattery( const char *pElementName );

	void			Init( void );
	void			Reset( void );
	void			VidInit( void );
	void			MsgFunc_Battery(bf_read &msg);

private:
	void	Paint( void );
	void	ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHudTexture		*icon_suit_empty;
	CHudTexture		*icon_suit_full;
	int				m_iBattery;
	float			m_flFade;
};	

DECLARE_HUDELEMENT( CHudBattery );
DECLARE_HUD_MESSAGE( CHudBattery, Battery );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudBattery::CHudBattery( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudSuit")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::Init()
{
	HOOK_HUD_MESSAGE( CHudBattery, Battery );
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::Reset()
{
	m_iBattery	= 0;
	m_flFade	= 0;
}

void CHudBattery::VidInit()
{
	Reset();

	BaseClass::VidInit();
}

void CHudBattery::Paint()
{
	Color	clrHealth;
	int		a;
	int		x;
	int		y;

	BaseClass::Paint();

	if ( !icon_suit_empty )
	{
		icon_suit_empty = gHUD.GetIcon( "suit_empty" );
	}

	if ( !icon_suit_full )
	{
		icon_suit_full = gHUD.GetIcon( "suit_full" );
	}

	if ( !icon_suit_empty || !icon_suit_full )
	{
		return;
	}

	// Has health changed? Flash the health #
	if ( m_flFade )
	{
		if (m_flFade > FADE_TIME)
			m_flFade = FADE_TIME;

		m_flFade -= ( gpGlobals->frametime * 20 );
		if ( m_flFade <= 0 )
		{
			a = 128;
			m_flFade = 0;
		}
		else
		{
			// Fade the health number back to dim
			a = MIN_ALPHA + ( m_flFade / FADE_TIME ) * 128;
		}
	}
	else
	{
		a = MIN_ALPHA;
	}

	int r, g, b, nUnused;
	(gHUD.m_clrYellowish).GetColor( r, g, b, nUnused );
	clrHealth.SetColor( r, g, b, a );

	int nFontHeight	= GetNumberFontHeight();

	int nHudElemWidth, nHudElemHeight;
	GetSize( nHudElemWidth, nHudElemHeight );

	int iOffset = icon_suit_empty->Height() / 6;

	x = nHudElemWidth / 5;
	y = nHudElemHeight - ( nFontHeight * 1.5 );

	icon_suit_empty->DrawSelf( x, y - iOffset, clrHealth );

	if ( m_iBattery > 0 )
	{
		int nSuitOffset = icon_suit_full->Height() * ((float)(100-(MIN(100,m_iBattery))) * 0.01);	// battery can go from 0 to 100 so * 0.01 goes from 0 to 1
		icon_suit_full->DrawSelfCropped( x, y - iOffset + nSuitOffset, 0, nSuitOffset, icon_suit_full->Width(), icon_suit_full->Height() - nSuitOffset, clrHealth );
	}

	x += icon_suit_empty->Width();
	DrawHudNumber( x, y, m_iBattery, clrHealth );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::MsgFunc_Battery( bf_read &msg )
{
	int x = msg.ReadShort();

	if ( x != m_iBattery )
	{
		m_flFade	= FADE_TIME;
		m_iBattery	= x;
	}
}

void CHudBattery::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);
}
