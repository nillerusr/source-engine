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


// This is a bad way to implement HL1 style sprite fonts, but it will work for now

CHL1HudNumbers::CHL1HudNumbers( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
}


void CHL1HudNumbers::VidInit( void )
{
	for ( int i = 0; i < 10; i++ )
	{
		char szNumString[ 10 ];

		sprintf( szNumString, "number_%d", i );
		icon_digits[ i ] = gHUD.GetIcon( szNumString );
	}
}


int CHL1HudNumbers::GetNumberFontHeight( void )
{
	if ( icon_digits[ 0 ] )
	{
		return icon_digits[ 0 ]->Height();
	}
	else
	{
		return 0;
	}
}


int CHL1HudNumbers::GetNumberFontWidth( void )
{
	if ( icon_digits[ 0 ] )
	{
		return icon_digits[ 0 ]->Width();
	}
	else
	{
		return 0;
	}
}


int CHL1HudNumbers::DrawHudNumber( int x, int y, int iNumber, Color &clrDraw )
{
	int iWidth = GetNumberFontWidth();
	int k;
	
	if ( iNumber > 0 )
	{
		// SPR_Draw 100's
		if ( iNumber >= 100 )
		{
			k = iNumber / 100;
			icon_digits[ k ]->DrawSelf( x, y, clrDraw );
			x += iWidth;
		}
		else
		{
			x += iWidth;
		}

		// SPR_Draw 10's
		if ( iNumber >= 10 )
		{
			k = ( iNumber % 100 ) / 10;
			icon_digits[ k ]->DrawSelf( x, y, clrDraw );
			x += iWidth;
		}
		else
		{
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		icon_digits[ k ]->DrawSelf( x, y, clrDraw );
		x += iWidth;
	} 
	else
	{
		// SPR_Draw 100's
		x += iWidth;

		// SPR_Draw 10's
		x += iWidth;

		// SPR_Draw ones
		k = 0;
		icon_digits[ k ]->DrawSelf( x, y, clrDraw );
		x += iWidth;
	}

	return x;
}
