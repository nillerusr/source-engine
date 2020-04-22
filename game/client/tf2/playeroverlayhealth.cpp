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
#include "playeroverlayhealth.h"
#include "playeroverlay.h"
#include "CommanderOverlay.h"
#include "hud_commander_statuspanel.h"
//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CHudPlayerOverlayHealth::CHudPlayerOverlayHealth( CHudPlayerOverlay *baseOverlay )
: BaseClass( NULL, "CHudPlayerOverlayHealth" )
{
	m_pBaseOverlay = baseOverlay;

	SetHealth( 0 );

	SetPaintBackgroundEnabled( false );
	// Send mouse inputs (but not cursorenter/exit for now) up to parent
	SetReflectMouse( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CHudPlayerOverlayHealth::~CHudPlayerOverlayHealth( void )
{
}

//-----------------------------------------------------------------------------
// Parse values from the file
//-----------------------------------------------------------------------------

bool CHudPlayerOverlayHealth::Init( KeyValues* pInitData )
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
// Purpose: 
// Input  : health - 
//-----------------------------------------------------------------------------

void CHudPlayerOverlayHealth::SetHealth( float health )
{
	m_Health = health;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPlayerOverlayHealth::Paint( void )
{
	int w, h;

	GetSize( w, h );
	
	m_pBaseOverlay->SetColorLevel( this, m_fgColor, m_bgColor );

	// Use a color related to health value....
	vgui::surface()->DrawSetColor( 0, 255, 0, 255 * m_pBaseOverlay->GetAlphaFrac() );

	int drawwidth;

	float frac = m_Health;
	frac = MIN( 1.0, m_Health );
	frac = MAX( 0.0, m_Health );

	drawwidth = frac * w;

	vgui::surface()->DrawFilledRect( 0, 0, drawwidth, h/2 );

	// This is the hurt part
	if (w != drawwidth)
	{
		vgui::surface()->DrawSetColor( 255, 64, 64, 255 * m_pBaseOverlay->GetAlphaFrac() );
		vgui::surface()->DrawFilledRect( drawwidth, 0, w, h/2 );
	}
}

void CHudPlayerOverlayHealth::OnCursorEntered()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusPrint( TYPE_HINT, "%s", m_pBaseOverlay->GetMouseOverText() );
	}
}

void CHudPlayerOverlayHealth::OnCursorExited()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusClear();
	}
}