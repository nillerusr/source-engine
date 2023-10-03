//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "iconpanel.h"
#include "KeyValues.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DECLARE_BUILD_FACTORY( CIconPanel );

CIconPanel::CIconPanel( vgui::Panel *parent, const char *name ) : vgui::Panel( parent, name )
{
	m_szIcon[0] = '\0';
	m_icon = NULL;
	m_bScaleImage = false;
}

void CIconPanel::ApplySettings( KeyValues *inResourceData )
{
	Q_strncpy( m_szIcon, inResourceData->GetString( "icon", "" ), sizeof( m_szIcon ) );

	m_icon = HudIcons().GetIcon( m_szIcon );

	m_bScaleImage = inResourceData->GetBool( "scaleImage", false );

	BaseClass::ApplySettings( inResourceData );
}

void CIconPanel::SetIcon( const char *szIcon )
{
	Q_strncpy( m_szIcon, szIcon, sizeof(m_szIcon) );

	m_icon = HudIcons().GetIcon( m_szIcon );
}

void CIconPanel::Paint()
{
	BaseClass::Paint();

	if ( m_icon )
	{
		int x, y, w, h;
		GetBounds( x, y, w, h );

		if ( m_bScaleImage )
		{
			m_icon->DrawSelf( 0, 0, w, h, m_IconColor );
		}
		else
		{
			m_icon->DrawSelf( 0, 0, m_IconColor );
		}
	}	
}

void CIconPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
    
	if ( m_szIcon[0] != '\0' )
	{
		m_icon = HudIcons().GetIcon( m_szIcon );
	}

	SetFgColor( pScheme->GetColor( "FgColor", Color( 255, 255, 255, 255 ) ) );
}
