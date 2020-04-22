//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "playeroverlay.h"
#include "playeroverlayselected.h"
#include <KeyValues.h>
#include "commanderoverlay.h"
#include "hud_commander_statuspanel.h"
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudPlayerOverlaySelected::CHudPlayerOverlaySelected( CHudPlayerOverlay *baseOverlay )
 : BaseClass( NULL, "CHudPlayerOverlaySelected" )
{
	m_pBaseOverlay = baseOverlay;

	int i;
	for ( i = 0; i < 4; i++ )
	{
		m_pImages[ i ] = NULL;
	}

	SetPaintBackgroundEnabled( false );

	// Send mouse inputs (but not cursorenter/exit for now) up to parent
	SetReflectMouse( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudPlayerOverlaySelected::~CHudPlayerOverlaySelected( void )
{
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------

bool CHudPlayerOverlaySelected::Init( KeyValues* pInitData )
{
	if (!pInitData)
		return false;

	if (!ParseRGBA(pInitData, "fgcolor", m_fgColor ))
		return false;

	if (!ParseRGBA(pInitData, "bgcolor", m_bgColor ))
		return false;

	int x, y, w, h;
	if (!ParseRect(pInitData, "position", x, y, w, h ))
		return false;
	SetPos( x, y );
	SetSize( w, h );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: NOTE:  This is not hooked up yet.
// Input  : pImage -- the four corner images
//-----------------------------------------------------------------------------
void CHudPlayerOverlaySelected::SetImages( BitmapImage *pImage[4] )
{
	int i;
	for ( i = 0 ; i < 4; i++ )
	{
		m_pImages[ i ] = pImage[ i ];
	}
}

//-----------------------------------------------------------------------------
// Purpose: FIXME:  Use materials for corners, not just procedural drawing?
//-----------------------------------------------------------------------------
void CHudPlayerOverlaySelected::Paint( void )
{
	int w, h;

	m_pBaseOverlay->SetColorLevel( this, m_fgColor, m_bgColor );

	GetSize( w, h );
	int r, g, b, a;
	Color clr = GetFgColor();
	clr.GetColor( r, g, b, a );
	vgui::surface()->DrawSetColor( r, g, b, a );

	// Draw corners
	int inSetSize = 6;
	int thickness = 2;

	vgui::surface()->DrawFilledRect( 0, 0, inSetSize, thickness );
	vgui::surface()->DrawFilledRect( 0, 0, thickness, inSetSize );

	vgui::surface()->DrawFilledRect( w-inSetSize, 0, w, thickness );
	vgui::surface()->DrawFilledRect( w-thickness, 0, w, inSetSize );

	vgui::surface()->DrawFilledRect( w-inSetSize, h-thickness, w, h );
	vgui::surface()->DrawFilledRect( w-thickness, h-inSetSize, w, h );

	vgui::surface()->DrawFilledRect( 0, h-thickness, inSetSize, h );
	vgui::surface()->DrawFilledRect( 0, h-inSetSize, thickness, h  );
}

void CHudPlayerOverlaySelected::OnCursorEntered()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusPrint( TYPE_HINT, "%s", m_pBaseOverlay->GetMouseOverText() );
	}
}

void CHudPlayerOverlaySelected::OnCursorExited()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusClear();
	}
}