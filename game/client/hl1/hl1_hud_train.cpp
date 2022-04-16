//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "hl1_hud_numbers.h"

#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>

using namespace vgui;
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudTrain: public CHudElement, public CHL1HudNumbers
{
	DECLARE_CLASS_SIMPLE( CHudTrain, CHL1HudNumbers );
public:
	CHudTrain( const char *pElementName );
	void	Init( void );
	void	VidInit( void );
	bool	ShouldDraw( void );
	void	MsgFunc_Train(bf_read &msg);

private:
	void	Paint( void );
	void	ApplySchemeSettings( vgui::IScheme *scheme );

private:
	int			m_iPos;
	CHudTexture	*icon_train;
};

//
//-----------------------------------------------------
//

DECLARE_HUDELEMENT( CHudTrain );
DECLARE_HUD_MESSAGE( CHudTrain, Train )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTrain::CHudTrain( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudTrain")
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scheme - 
//-----------------------------------------------------------------------------
void CHudTrain::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::Init(void)
{
	HOOK_HUD_MESSAGE( CHudTrain, Train );

	m_iPos = 0;
}

void CHudTrain::VidInit(void)
{
	BaseClass::VidInit();
	
	m_iPos = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudTrain::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && m_iPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::Paint()
{
	if ( !icon_train )
	{
		icon_train = gHUD.GetIcon( "train" );
	}

	if ( !icon_train )
	{
		return;
	}

	int		r, g, b, a;
	int		x, y;
	Color	clrTrain;

	(gHUD.m_clrYellowish).GetColor( r, g, b, a );
	clrTrain.SetColor( r, g, b, a );

	int nHudElemWidth, nHudElemHeight;
	GetSize( nHudElemWidth, nHudElemHeight );

	// This should show up to the right and part way up the armor number
	y = nHudElemHeight - icon_train->Height() - GetNumberFontHeight();
	x = nHudElemWidth / 3 + icon_train->Width() / 4;

	IMaterial *material = materials->FindMaterial( icon_train->szTextureFile, TEXTURE_GROUP_VGUI );
	if ( material )
	{
		bool found;
		IMaterialVar* pFrameVar = material->FindVar( "$frame", &found, false );
		if ( found )
		{
			pFrameVar->SetFloatValue( m_iPos - 1 );
		}
	}

	icon_train->DrawSelf( x, y, clrTrain );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTrain::MsgFunc_Train( bf_read &msg )
{
	m_iPos = msg.ReadByte();
}
