//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud_vehicle_role.h"
#include "iclientmode.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


DECLARE_HUDELEMENT( CVehicleRoleHudElement );

using namespace vgui;

CVehicleRoleHudElement::CVehicleRoleHudElement( const char *pElementName ) : 
	CHudElement( pElementName ), BaseClass( NULL, "VehicleRoleHudElement" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iRole = -1;

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

void CVehicleRoleHudElement::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_hTextFont = scheme->GetFont( "Trebuchet24" );

	SetPaintBackgroundEnabled( false );
}


void CVehicleRoleHudElement::ShowVehicleRole( int iRole )
{
	m_iRole = iRole;
}


void CVehicleRoleHudElement::Paint()
{
	if ( m_iRole == -1 )
		return;

	surface()->DrawSetTextFont( m_hTextFont );
	surface()->DrawSetTextColor( GetFgColor() );

	char str[512];
	if ( m_iRole == VEHICLE_ROLE_DRIVER )
	{
		Q_strncpy( str, "Driver", sizeof( str ) );
	}
	else
	{
		Q_snprintf( str, sizeof( str ), "Passenger %d", m_iRole );
	}

	int pixels = UTIL_ComputeStringWidth( m_hTextFont, str );
	int x = ( GetWide() - pixels ) / 2;

	surface()->DrawSetTextPos( x, 0 );

	char *p = str;
	while ( *p )
	{
		surface()->DrawUnicodeChar( *p );
		p++;
	}
}


