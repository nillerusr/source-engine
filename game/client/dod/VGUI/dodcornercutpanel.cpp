//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>
#include "dodcornercutpanel.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDCutEditablePanel::CDoDCutEditablePanel( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_nCornerToCut = DOD_CORNERCUT_PANEL_BOTTOMRIGHT;
	m_nCornerCutSize = 1;

	memset( m_szBackgroundTexture, 0, sizeof( m_szBackgroundTexture ) );
	memset( m_szBackgroudColor, 0, sizeof( m_szBackgroudColor ) );
	memset( m_szBorderColor, 0, sizeof( m_szBorderColor ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::SetBorder( vgui::IBorder *border )
{
	BaseClass::SetBorder( border );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// check to see if we have a new name assigned
	Q_strncpy( m_szBackgroundTexture, inResourceData->GetString( "BackgroundTexture", "vgui/white" ), sizeof( m_szBackgroundTexture ) );
	Q_strncpy( m_szBackgroudColor, inResourceData->GetString( "BackgroundColor", "HudPanelForeground" ), sizeof( m_szBackgroudColor ) );
	Q_strncpy( m_szBorderColor, inResourceData->GetString( "BackgroundBorder", "HudPanelBorder" ), sizeof( m_szBorderColor ) );

	m_iBackgroundTexture = vgui::surface()->DrawGetTextureId( m_szBackgroundTexture );
	if ( m_iBackgroundTexture == -1 )
	{
		m_iBackgroundTexture = vgui::surface()->CreateNewTextureID();
	}

	vgui::surface()->DrawSetTextureFile( m_iBackgroundTexture, m_szBackgroundTexture, true, true );

	m_nCornerCutSize = inResourceData->GetInt( "CornerCutSize", 1 );

	// scale the cut size to our screen co-ords
	if ( IsProportional() )
	{
		m_nCornerCutSize = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), m_nCornerCutSize );
	}
	
	const char *pszCorner = inResourceData->GetString( "CornerToCut", "" );
	if ( pszCorner )
	{
		if ( !Q_strcmp( pszCorner, "bottom_right" ) )
		{
			m_nCornerToCut = DOD_CORNERCUT_PANEL_BOTTOMRIGHT;
		}
		else if ( !Q_strcmp( pszCorner, "bottom_left" ) )
		{
			m_nCornerToCut = DOD_CORNERCUT_PANEL_BOTTOMLEFT;
		}
		else if ( !Q_strcmp( pszCorner, "top_right" ) )
		{
			m_nCornerToCut = DOD_CORNERCUT_PANEL_TOPRIGHT;

		}
		else if ( !Q_strcmp( pszCorner, "top_left" ) )
		{
			m_nCornerToCut = DOD_CORNERCUT_PANEL_TOPLEFT;
		}
	}
    
	InvalidateLayout( false, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::GetSettings( KeyValues *outResourceData )
{
	BaseClass::GetSettings( outResourceData );

	outResourceData->SetString( "BackgroundTexture", m_szBackgroundTexture);
	outResourceData->SetString( "BackgroundColor", m_szBackgroudColor);
	outResourceData->SetString( "BackgroundBorder", m_szBorderColor);
	outResourceData->SetFloat( "CornerCutSize", m_nCornerCutSize );

	const char *pszCorner = NULL;
	switch( m_nCornerToCut )
	{
	case DOD_CORNERCUT_PANEL_TOPLEFT:
		pszCorner = "top_left";
		break;
	case DOD_CORNERCUT_PANEL_TOPRIGHT:
		pszCorner = "top_right";
		break;
	case DOD_CORNERCUT_PANEL_BOTTOMLEFT:
		pszCorner = "bottom_left";
		break;
	case DOD_CORNERCUT_PANEL_BOTTOMRIGHT:
	default:
		pszCorner = "bottom_right";
		break;
	}
	outResourceData->SetString( "CornerToCut", pszCorner );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBorder( NULL );

	m_clrBackground = pScheme->GetColor( m_szBackgroudColor, GetFgColor() );
	m_clrBorder = pScheme->GetColor( m_szBorderColor, GetBgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::PaintBackground()
{
	vgui::Vertex_t lineverts[5];
	vgui::Vertex_t verts[5];

	int nwide, ntall;
	GetSize( nwide, ntall );

	int wide = nwide - 1; // -1 because we can't draw all the way out to the width of our panel (it gets clipped), we can only draw to width - 1
	int tall = ntall - 1; // (same as above)

	switch ( m_nCornerToCut )
	{
	case DOD_CORNERCUT_PANEL_TOPLEFT:
		verts[0].Init( Vector2D( m_nCornerCutSize, 0 ) );
		verts[1].Init( Vector2D( wide, 0 ) );
		verts[2].Init( Vector2D( wide, tall ) );
		verts[3].Init( Vector2D( 0, tall ) );
		verts[4].Init( Vector2D( 0, m_nCornerCutSize ) );

		lineverts[0].Init( Vector2D( m_nCornerCutSize-1, 0 ) );
		lineverts[1].Init( Vector2D( wide, 0 ) );
		lineverts[2].Init( Vector2D( wide, tall ) );
		lineverts[3].Init( Vector2D( 0, tall ) );
		lineverts[4].Init( Vector2D( 0, m_nCornerCutSize-1 ) );

		break;

	case DOD_CORNERCUT_PANEL_TOPRIGHT:
		verts[0].Init( Vector2D( 0, 0 ) );
		verts[1].Init( Vector2D( wide - m_nCornerCutSize, 0 ) );
		verts[2].Init( Vector2D( wide, m_nCornerCutSize ) );
		verts[3].Init( Vector2D( wide, tall ) );
		verts[4].Init( Vector2D( 0, tall ) );

		lineverts[0].Init( Vector2D( 0, 0 ) );
		lineverts[1].Init( Vector2D( wide - m_nCornerCutSize, 0 ) );
		lineverts[2].Init( Vector2D( wide, m_nCornerCutSize ) );
		lineverts[3].Init( Vector2D( wide, tall ) );
		lineverts[4].Init( Vector2D( 0, tall ) );

		break;

	case DOD_CORNERCUT_PANEL_BOTTOMLEFT:
		verts[0].Init( Vector2D( 0, 0 ) );
		verts[1].Init( Vector2D( wide, 0 ) );
		verts[2].Init( Vector2D( wide, tall ) );
		verts[3].Init( Vector2D( m_nCornerCutSize, tall ) );
		verts[4].Init( Vector2D( 0, tall - m_nCornerCutSize ) );

		lineverts[0].Init( Vector2D( 0, 0 ) );
		lineverts[1].Init( Vector2D( wide, 0 ) );
		lineverts[2].Init( Vector2D( wide, tall ) );
		lineverts[3].Init( Vector2D( m_nCornerCutSize, tall ) );
		lineverts[4].Init( Vector2D( 0, tall - m_nCornerCutSize ) );

		break;

	case DOD_CORNERCUT_PANEL_BOTTOMRIGHT:
	default:
		verts[0].Init( Vector2D( 0, 0 ) );
		verts[1].Init( Vector2D( wide, 0 ) );
		verts[2].Init( Vector2D( wide, tall - m_nCornerCutSize + 1 ) );
		verts[3].Init( Vector2D( wide - m_nCornerCutSize + 1, tall ) );
		verts[4].Init( Vector2D( 0, tall ) );

		lineverts[0].Init( Vector2D( 0, 0 ) );
		lineverts[1].Init( Vector2D( wide, 0 ) );
		lineverts[2].Init( Vector2D( wide, tall - m_nCornerCutSize ) );
		lineverts[3].Init( Vector2D( wide - m_nCornerCutSize, tall ) );
		lineverts[4].Init( Vector2D( 0, tall ) );

		break;
	}

	vgui::surface()->DrawSetTexture( m_iBackgroundTexture );
	vgui::surface()->DrawSetColor( m_clrBackground );
	vgui::surface()->DrawTexturedPolygon( 5, verts );

	vgui::surface()->DrawSetColor( m_clrBorder );
	vgui::surface()->DrawTexturedPolyLine( lineverts, 5 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::SetBackGroundColor( const char *pszNewColor )
{
	if ( !pszNewColor )
	{
		return;
	}

	Q_strncpy( m_szBackgroudColor, pszNewColor, sizeof( m_szBackgroudColor ) );
	InvalidateLayout( false, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCutEditablePanel::SetBorderColor( const char *pszNewColor )
{
	if ( !pszNewColor )
	{
		return;
	}

	Q_strncpy( m_szBorderColor, pszNewColor, sizeof( m_szBorderColor ) );
	InvalidateLayout( false, true );
}