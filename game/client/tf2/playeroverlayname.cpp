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
#include <KeyValues.h>
#include "playeroverlay.h"
#include "playeroverlayname.h"
#include <KeyValues.h>
#include "commanderoverlay.h"
#include "hud_commander_statuspanel.h"
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CHudPlayerOverlayName::CHudPlayerOverlayName( CHudPlayerOverlay *baseOverlay, const char *name ) : 
vgui::Label( (vgui::Panel *)NULL, "OverlayName", name )
{
	m_pBaseOverlay = baseOverlay;

	Q_strncpy( m_szName, name, sizeof( m_szName ) );

	SetPaintBackgroundEnabled( false );

	// Send mouse inputs (but not cursorenter/exit for now) up to parent
	SetReflectMouse( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudPlayerOverlayName::~CHudPlayerOverlayName( void )
{
}

bool CHudPlayerOverlayName::Init( KeyValues* pInitData )
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

	SetContentAlignment( vgui::Label::a_west );

	return true;
}

void CHudPlayerOverlayName::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetFont( pScheme->GetFont( "primary" ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CHudPlayerOverlayName::SetName( const char *name )
{
	Q_strncpy( m_szName, name, sizeof( m_szName ) );
	SetText( name );
}

void CHudPlayerOverlayName::Paint()
{
	m_pBaseOverlay->SetColorLevel( this, m_fgColor, m_bgColor );

	BaseClass::Paint();
}

void CHudPlayerOverlayName::SetReflectMouse( bool reflect )
{
	m_bReflectMouse = true;
}

void CHudPlayerOverlayName::OnCursorMoved(int x,int y)
{
	if ( !m_bReflectMouse )
		return;

	if ( !GetParent() )
		return;

	LocalToScreen( x, y );

	vgui::ivgui()->PostMessage( 
		GetParent()->GetVPanel(), 
		new KeyValues( "CursorMoved", "xpos", x, "ypos", y ), 
		GetVPanel()  );
}

void CHudPlayerOverlayName::OnMousePressed(vgui::MouseCode code)
{
	if ( !m_bReflectMouse )
		return;

	if ( !GetParent() )
		return;

	vgui::ivgui()->PostMessage( 
		GetParent()->GetVPanel(), 
		new KeyValues( "MousePressed", "code", code ), 
		GetVPanel()  );
}

void CHudPlayerOverlayName::OnMouseDoublePressed(vgui::MouseCode code)
{
	if ( !m_bReflectMouse )
		return;

	if ( !GetParent() )
		return;

	vgui::ivgui()->PostMessage( 
		GetParent()->GetVPanel(), 
		new KeyValues( "MouseDoublePressed", "code", code ), 
		GetVPanel()  );
}

void CHudPlayerOverlayName::OnMouseReleased(vgui::MouseCode code)
{
	if ( !m_bReflectMouse )
		return;

	if ( !GetParent() )
		return;

	vgui::ivgui()->PostMessage( 
		GetParent()->GetVPanel(), 
		new KeyValues( "MouseReleased", "code", code ), 
		GetVPanel()  );
}

void CHudPlayerOverlayName::OnMouseWheeled(int delta)
{
	if ( !m_bReflectMouse )
		return;

	if ( !GetParent() )
		return;

	vgui::ivgui()->PostMessage( 
		GetParent()->GetVPanel(), 
		new KeyValues( "MouseWheeled", "delta", delta ), 
		GetVPanel()  );
}

void CHudPlayerOverlayName::OnCursorEntered()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusPrint( TYPE_HINT, "%s", m_pBaseOverlay->GetMouseOverText() );
	}
}

void CHudPlayerOverlayName::OnCursorExited()
{
	if ( m_pBaseOverlay->GetMouseOverText() )
	{
		StatusClear();
	}
}