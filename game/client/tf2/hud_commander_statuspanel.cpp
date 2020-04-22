//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Flyover/Tooltip hint area for commander
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <stdarg.h>
#include "hud_commander_statuspanel.h"
#include "techtree.h"
#include <vgui/IVGui.h>
#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CCommanderStatusPanel *g_pCommanderStatusPanel = NULL;

// 
#define ALPHA_ADJUST_TIME 0.1f
#define MAX_FILLED_INFO_ALPHA 127.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderStatusPanel::CCommanderStatusPanel( void ) : 
	BaseClass( NULL, "CCommanderStatusPanel" )
{
	m_hFont = m_hFontText = 0;
	m_nLeftEdge = 0;
	m_nBottomEdge = 0;

//	m_pBorder = new vgui::LineBorder( 2, vgui::Color( 127, 127, 127, 255 ) );
//	setBorder( m_pBorder );
	
	SetBgColor( Color( 0, 0, 0, 100 ) );

	// we need updating
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	InternalClear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommanderStatusPanel::~CCommanderStatusPanel( void )
{
//	delete m_pBorder;
}

//-----------------------------------------------------------------------------
// Scheme settings
//-----------------------------------------------------------------------------
void CCommanderStatusPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "Trebuchet20" );
	// FIXME:  Outline, weight 1000
	m_hFontText = pScheme->GetFont( "Trebuchet18" );
	// FIXME:  Outline, weight 700
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderStatusPanel::OnTick()
{
	// Simulate alpha
	float dt = gpGlobals->frametime;
	float maxmove = 1.0f / ALPHA_ADJUST_TIME;
	float moverequested = dt * maxmove;

	float remaining = m_flGoalAlpha - m_flCurrentAlpha;
	if ( fabs( remaining ) < 0.01f )
	{
		if ( m_flCurrentAlpha < 0.01f )
		{
			InternalClear();
			return;
		}
	}

	if ( moverequested > fabs( remaining ) )
	{
		moverequested = fabs( remaining );
	}

	if ( remaining > 0.0f )
	{
		m_flCurrentAlpha += moverequested;
	}
	else
	{
		m_flCurrentAlpha -= moverequested;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderStatusPanel::Paint()
{
	if ( m_flCurrentAlpha <= 0.0f )
		return;

	int wide, tall;
	GetSize( wide, tall );

	int x = 2;
	int y = 2;
	
	wide -= 4;
	tall -= 4;

	int alpha = 255.0f * m_flCurrentAlpha;

	if ( m_Type == TYPE_INFO && m_nTitlePos != -1 )
	{
		m_szText[ m_nTitlePos ] = 0;

		g_pMatSystemSurface->DrawColoredTextRect( m_hFont, x, y, wide, tall, 220, 220, 255, alpha, "%s", m_szText );

		m_szText[ m_nTitlePos ] = '\n';

		y += vgui::surface()->GetFontTall( m_hFont );
		tall -= vgui::surface()->GetFontTall( m_hFont );

		// Start after title
		g_pMatSystemSurface->DrawColoredTextRect( m_hFontText, x, y, wide, tall, 255, 255, 255, alpha, "%s", &m_szText[ m_nTitlePos + 1 ] );

		if ( m_bShowTechnology && m_pTechnology )
		{
			int x = 0;
			int y = tall;
			int size;
			int r, g, b;
			r = g = 192;
			b = 255;

			float fResourceCost = m_pTechnology->GetResourceCost();
			float fResourceLevel = m_pTechnology->GetResourceLevel();

			int techLevel = m_pTechnology->GetLevel();
			x = 5;
			g_pMatSystemSurface->DrawColoredText( m_hFontText, x, y, r, g, b, alpha, "Level <%i>", techLevel );

			x = wide - 5;

			if ( m_pTechnology->GetActive() )
			{
				size = g_pMatSystemSurface->DrawTextLen( m_hFontText, "Already owned" );

				x -= size;

				g_pMatSystemSurface->DrawColoredText( m_hFontText, x, y, r, g, b, alpha, "Already owned" );
			}
			else
			{
				if ( !fResourceCost )
				{
					size = g_pMatSystemSurface->DrawTextLen( m_hFontText, "Cost:  Free" );

					x -= size; 

					g_pMatSystemSurface->DrawColoredText( m_hFontText, x, y, r, g, b, alpha, "Cost:  Free" );
				}
				else
				{
					int sizecost = g_pMatSystemSurface->DrawTextLen( m_hFontText, "WWW: 000 (000)" ) + 5;
					
					size = g_pMatSystemSurface->DrawTextLen( m_hFontText, "Cost: " );

					x = wide - size - 4 * sizecost;

					r = 255;
					g = 255;
					b = 255;
					g_pMatSystemSurface->DrawColoredText( m_hFontText, x, y, r, g, b, alpha, "Cost: " );

					x += size;

					char szCostString[64];

					// Draw cost string backward to right justify it
					if ( fResourceCost )
					{
						if ( fResourceLevel )
							Q_snprintf( szCostString, sizeof( szCostString ), "A: %i (%i)", (int)fResourceLevel, (int)fResourceCost );
						else
							Q_snprintf( szCostString, sizeof( szCostString ), "A: %i", (int)fResourceCost );
						g_pMatSystemSurface->DrawColoredText( m_hFontText, x, y, sResourceColor.r, sResourceColor.g, sResourceColor.b, alpha, "%s", szCostString );
						sizecost = g_pMatSystemSurface->DrawTextLen( m_hFontText, "%s", szCostString ) + 2;
					}
					x += sizecost;
				}
			}
		}
	}
	else
	{
		g_pMatSystemSurface->DrawColoredTextRect( m_hFont, 
			x, 
			y + tall - vgui::surface()->GetFontTall( m_hFont ) - 4, 
			wide, 
			vgui::surface()->GetFontTall( m_hFont ) + 2, 
			220, 220, 255, alpha, 
			"%s",
			m_szText );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderStatusPanel::PaintBackground()
{
	if ( m_flCurrentAlpha <= 0.0f )
		return;

	if ( m_Type == TYPE_INFO )
	{
		int alpha = MAX_FILLED_INFO_ALPHA * m_flCurrentAlpha;

		SetBgColor( Color( 0, 0, 0, alpha ) );

		alpha += ( 255 - alpha ) / 2;

//		m_pBorder->SetColor( vgui::Color( 120, 120, 180, alpha ) ); 
	}
	else
	{
		SetBgColor( Color( 0, 0, 0, 0 ) );
//		m_pBorder->SetColor( vgui::Color( 0, 0, 0, 0 ) );
	}
	BaseClass::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommanderStatusPanel::Clear( void )
{
	m_flGoalAlpha = 0.0f; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CCommanderStatusPanel::SetText( STATUSTYPE type, const char *fmt, ... )
{
	va_list argptr;
	va_start( argptr, fmt );
	 _vsnprintf(m_szText, MAX_STATUS_TEXT, fmt, argptr);
	va_end(argptr);
	m_szText[ MAX_STATUS_TEXT - 1 ] = 0;

	m_Type = type;
	SetVisible( true );

	m_nTitlePos = -1;

	if ( m_Type == TYPE_INFO )
	{
		// Search for first \n
		char *p = m_szText;
		while ( *p && *p != '\n' )
		{
			p++;
		}

		if ( *p == '\n' )
		{
			m_nTitlePos = p - m_szText;
		}
	}

	m_flCurrentAlpha = MAX( m_flCurrentAlpha, 0.01f );
	m_flGoalAlpha = 1.0f;
	m_bShowTechnology = false;
	m_pTechnology = NULL;

	RecomputeBounds();
}

void CCommanderStatusPanel::SetTechnology( CBaseTechnology *technology )
{
	m_bShowTechnology = true;
	m_pTechnology = technology;
}

void CCommanderStatusPanel::InternalClear( void )
{
	m_Type = TYPE_UNKNOWN;
	m_szText[ 0 ] = 0;
	m_nTitlePos = -1;

	m_flCurrentAlpha = 0.0f;
	m_flGoalAlpha = 0.0f;

	m_bShowTechnology = false;
	m_pTechnology = NULL;

	SetVisible( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
}

void CCommanderStatusPanel::RecomputeBounds( void )
{
	int maxlines = 5;
	int lineheight = vgui::surface()->GetFontTall( m_hFont );

	int height;
	height = lineheight + ( maxlines - 1 ) * vgui::surface()->GetFontTall( m_hFontText );
	height += 4; // 2 pixels top and bottom

	SetBounds( m_nLeftEdge, m_nBottomEdge - height, ScreenWidth() * 0.6, height );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( true );
}

void CCommanderStatusPanel::SetLeftBottom( int l, int b )
{
	m_nLeftEdge		= l;
	m_nBottomEdge	= b;

	RecomputeBounds();
}

//////////////////////////////////////////
//
// Status Panel Creation/Destruction and public api
//
//////////////////////////////////////////
void StatusCreate( vgui::Panel *parent, int treetoprow )
{
	Assert( !g_pCommanderStatusPanel );
	g_pCommanderStatusPanel = new CCommanderStatusPanel();
	g_pCommanderStatusPanel->SetAutoDelete( false );
	g_pCommanderStatusPanel->SetParent( parent );
	g_pCommanderStatusPanel->SetLeftBottom( 0, treetoprow - 10 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : treeetoprow - 
//-----------------------------------------------------------------------------
void StatusSetTopRow( int treetoprow )
{
	Assert( g_pCommanderStatusPanel );
	g_pCommanderStatusPanel->SetLeftBottom( 0, treetoprow - 10 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StatusDestroy( void )
{
	delete g_pCommanderStatusPanel;
	g_pCommanderStatusPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void StatusPrint( STATUSTYPE type, const char *fmt, ... )
{
	char text[ CCommanderStatusPanel::MAX_STATUS_TEXT ];

	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf(text, CCommanderStatusPanel::MAX_STATUS_TEXT, fmt, argptr);
	va_end(argptr);
	text[ CCommanderStatusPanel::MAX_STATUS_TEXT - 1 ] = 0;

	if ( g_pCommanderStatusPanel )
	{
		g_pCommanderStatusPanel->SetText( type, text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StatusClear( void )
{
	if ( g_pCommanderStatusPanel )
	{
		g_pCommanderStatusPanel->Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *technology - 
//-----------------------------------------------------------------------------
void StatusTechnology( CBaseTechnology *technology )
{
	if ( g_pCommanderStatusPanel )
	{
		g_pCommanderStatusPanel->SetTechnology(technology );
	}
}