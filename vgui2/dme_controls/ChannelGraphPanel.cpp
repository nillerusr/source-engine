//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <math.h>

#include <dme_controls/ChannelGraphPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include "vgui/IInput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( CChannelGraphPanel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CChannelGraphPanel::CChannelGraphPanel( Panel *parent, const char *name )
	: BaseClass( parent, name ), m_font( 0 ),
	m_graphMinTime( 0 ), m_graphMaxTime( 0 ),
	m_graphMinValue( 0.0f ), m_graphMaxValue( 0.0f ),
	m_nMouseStartX( -1 ), m_nMouseStartY( -1 ),
	m_nMouseLastX( -1 ), m_nMouseLastY( -1 ),
	m_nTextBorder( 2 ), m_nGraphOriginX( 40 ), m_nGraphOriginY( 10 )
{
}


void CChannelGraphPanel::SetChannel( CDmeChannel *pChannel )
{
	m_hChannel = pChannel;
	CDmeLog *pLog = m_hChannel->GetLog();
	m_graphMinTime = pLog->GetBeginTime();
	m_graphMaxTime = pLog->GetEndTime();

	m_graphMinValue = FLT_MAX;
	m_graphMaxValue = -FLT_MAX;

	int nComponents = NumComponents( pLog->GetDataType() );
	int nKeys = pLog->GetKeyCount();
	for ( int k = 0; k < nKeys; ++k )
	{
		DmeTime_t t = pLog->GetKeyTime( k );
		for ( int i = 0; i < nComponents; ++i )
		{
			float f = pLog->GetComponent( t, i );
			m_graphMinValue = min( m_graphMinValue, f );
			m_graphMaxValue = max( m_graphMaxValue, f );
		}
	}
}

//-----------------------------------------------------------------------------
// input methods
//-----------------------------------------------------------------------------
void CChannelGraphPanel::OnSizeChanged( int newWide, int newTall ) // called after the size of a panel has been changed
{
	int wide = newWide - m_nGraphOriginX;
	int tall = newTall - m_nGraphOriginY;
	m_flTimeToPixel = wide / ( m_graphMaxTime - m_graphMinTime ).GetSeconds();
	m_flValueToPixel = tall / ( m_graphMaxValue - m_graphMinValue );
}

void CChannelGraphPanel::OnMousePressed( MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( code != MOUSE_LEFT )
		return;

	vgui::input()->GetCursorPos( m_nMouseStartX, m_nMouseStartY );
	ScreenToLocal( m_nMouseStartX, m_nMouseStartY );
	m_nMouseLastX = m_nMouseStartX;
	m_nMouseLastY = m_nMouseStartY;

	input()->SetMouseCapture( GetVPanel() );
}

void CChannelGraphPanel::OnMouseReleased( MouseCode code )
{
	BaseClass::OnMouseReleased( code );
	if ( code != MOUSE_LEFT )
		return;

	m_nMouseStartX = m_nMouseStartY = -1;
	m_nMouseLastX = m_nMouseLastY = -1;

	input()->SetMouseCapture( NULL );
}

void CChannelGraphPanel::OnCursorMoved( int mx, int my )
{
	BaseClass::OnCursorMoved( mx, my );
	if ( !vgui::input()->IsMouseDown( MOUSE_LEFT ) )
		return;

	bool bInValueLegend = m_nMouseStartX < m_nGraphOriginX;
	bool bInTimeLegend = m_nMouseStartY > GetTall() - m_nGraphOriginY - 1;
	if ( bInTimeLegend && bInValueLegend )
	{
		bInTimeLegend = bInValueLegend = false;
	}

	int dx = mx - m_nMouseLastX;
	int dy = my - m_nMouseLastY;

	if ( bInTimeLegend )
	{
		if ( abs( dy ) > abs( dx ) )
		{
			m_graphMinTime -= DmeTime_t( dy / m_flTimeToPixel );
			m_graphMaxTime += DmeTime_t( dy / m_flTimeToPixel );
			m_flTimeToPixel = ( GetWide() - m_nGraphOriginX ) / ( m_graphMaxTime - m_graphMinTime ).GetSeconds();

			int x = mx = m_nMouseLastX;
			int y = my = m_nMouseLastY;
			LocalToScreen( x, y );
			vgui::input()->SetCursorPos( x, y );
		}
		else
		{
			m_graphMinTime -= DmeTime_t( dx / m_flTimeToPixel );
			m_graphMaxTime -= DmeTime_t( dx / m_flTimeToPixel );
		}
	}
	else if ( bInValueLegend )
	{
		if ( abs( dx ) > abs( dy ) )
		{
			m_graphMinValue += dx / m_flValueToPixel;
			m_graphMaxValue -= dx / m_flValueToPixel;
			m_flValueToPixel = ( GetTall() - m_nGraphOriginY ) / ( m_graphMaxValue - m_graphMinValue );

			int x = mx = m_nMouseLastX;
			int y = my = m_nMouseLastY;
			LocalToScreen( x, y );
			vgui::input()->SetCursorPos( x, y );
		}
		else
		{
			m_graphMinValue += dy / m_flValueToPixel;
			m_graphMaxValue += dy / m_flValueToPixel;
		}
	}

	m_nMouseLastX = mx;
	m_nMouseLastY = my;
}

void CChannelGraphPanel::OnMouseWheeled( int delta )
{
	// TODO - zoom in around current time?
}

//-----------------------------------------------------------------------------
// Purpose: lays out the graph
//-----------------------------------------------------------------------------
void CChannelGraphPanel::PerformLayout()
{
	BaseClass::PerformLayout();
}


float GetDisplayIncrement( int windowpixels, int fontpixels, float valuerange, int *pDecimalPlaces = NULL )
{
	float ratio = valuerange * fontpixels / ( windowpixels );
	int nPower = ( int )ceil( log10( ratio ) );
	if ( pDecimalPlaces )
	{
		*pDecimalPlaces = max( 0, -nPower );
	}
	return pow( 10.0f, nPower );
}

int CChannelGraphPanel::TimeToPixel( DmeTime_t time )
{
	return m_nGraphOriginX + ( int )floor( m_flTimeToPixel * ( time - m_graphMinTime ).GetSeconds() + 0.5f );
}

int CChannelGraphPanel::ValueToPixel( float flValue )
{
	return m_nGraphOriginY + ( int )floor( m_flValueToPixel * ( flValue - m_graphMinValue ) + 0.5f );
}

//-----------------------------------------------------------------------------
// Purpose: draws the graph
//-----------------------------------------------------------------------------
void CChannelGraphPanel::Paint()
{
	// estimate the size of the graph marker text
	int wide = GetWide() - m_nGraphOriginX;
	int tall = GetTall() - m_nGraphOriginY;

	int textwidth = 40, textheight = 10;
	surface()->GetTextSize( m_font, L"999.9", textwidth, textheight );

	// draw current time marker
	DmeTime_t curtime = m_hChannel->GetCurrentTime();
	if ( curtime >= m_graphMinTime && curtime <= m_graphMaxTime )
	{
		Color cyan( 0, 255, 255, 255 );
		surface()->DrawSetColor( cyan );
		int x = TimeToPixel( curtime );
		surface()->DrawLine( x, 0, x, GetTall() - m_nGraphOriginY - 1 );
	}

	// draw left/bottom graph border
	Color black( 0, 0, 0, 255 );
	surface()->DrawSetColor( black );
	surface()->DrawLine( m_nGraphOriginX, GetTall() - m_nGraphOriginY - 1, GetWide(), GetTall() - m_nGraphOriginY - 1 );
	surface()->DrawLine( m_nGraphOriginX, GetTall() - m_nGraphOriginY - 1, m_nGraphOriginX, 0 );

	surface()->DrawSetTextColor( black );
	surface()->DrawSetTextFont( m_font );

	// draw graph tickmarks and values along the left border
	int nDecimalPlaces = 0;
	float flValueIncrement = GetDisplayIncrement( tall, ( int )( 1.5f * textheight ), m_graphMaxValue - m_graphMinValue, &nDecimalPlaces );
	int nMinValueIndex = ( int )ceil ( m_graphMinValue / flValueIncrement );
	int nMaxValueIndex = ( int )floor( m_graphMaxValue / flValueIncrement );
	float flValue = nMinValueIndex * flValueIncrement;
	for ( int i = nMinValueIndex; i <= nMaxValueIndex; ++i, flValue += flValueIncrement )
	{
		wchar_t pFormat[ 32 ];
		V_swprintf_safe( pFormat, L"%%.%df", nDecimalPlaces );

		wchar_t wstring[ 32 ];
		V_swprintf_safe( wstring, pFormat, flValue );

		int tw = 0, th = 0;
		surface()->GetTextSize( m_font, wstring, tw, th );

		int y = GetTall() - ValueToPixel( flValue ) - 1;
		surface()->DrawSetTextPos( m_nGraphOriginX - m_nTextBorder - tw, y - textheight / 2 );
		surface()->DrawPrintText( wstring, wcslen( wstring ) );

		surface()->DrawLine( m_nGraphOriginX - m_nTextBorder, y, m_nGraphOriginX, y );
	}

	// draw graph tickmarks and times along the bottom border
	float flTimeIncrement = GetDisplayIncrement( wide, textwidth, ( m_graphMaxTime - m_graphMinTime ).GetSeconds(), &nDecimalPlaces );
	int nMinTimeIndex = ( int )ceil ( m_graphMinTime.GetSeconds() / flTimeIncrement );
	int nMaxTimeIndex = ( int )floor( m_graphMaxTime.GetSeconds() / flTimeIncrement );
	float flTime = nMinTimeIndex * flTimeIncrement;
	for ( int i = nMinTimeIndex; i <= nMaxTimeIndex; ++i, flTime += flTimeIncrement )
	{
		wchar_t pFormat[ 32 ];
		V_swprintf_safe( pFormat, L"%%.%df", nDecimalPlaces );

		wchar_t wstring[ 32 ];
		V_swprintf_safe( wstring, pFormat, flTime );

		int tw = 0, th = 0;
		surface()->GetTextSize( m_font, wstring, tw, th );

		int x = TimeToPixel( DmeTime_t( flTime ) );
		surface()->DrawSetTextPos( x - tw / 2, GetTall() - m_nGraphOriginY + m_nTextBorder - 1 );
		surface()->DrawPrintText( wstring, wcslen( wstring ) );

		surface()->DrawLine( x, GetTall() - m_nGraphOriginY + m_nTextBorder - 1, x, GetTall() - m_nGraphOriginY - 1 );
	}

	static Color s_componentColors[] =
	{
		Color( 255, 0, 0, 255 ),
		Color( 0, 255, 0, 255 ),
		Color( 0, 0, 255, 255 ),
		Color( 0, 0, 0, 255 ),
	};

	CDmeLog *pLog = m_hChannel->GetLog();
	int nComponents = NumComponents( pLog->GetDataType() );
	int nKeys = pLog->GetKeyCount();

	// draw plotted graph
	for ( int i = 0; i < nComponents; ++i )
	{
		Color &color = s_componentColors[ i % ARRAYSIZE( s_componentColors ) ];
		surface()->DrawSetColor( color );

		int lastx = -1;
		int lasty = -1;
		for ( int k = 0; k < nKeys; ++k )
		{
			DmeTime_t t = pLog->GetKeyTime( k );
			float f = pLog->GetComponent( t, i );
			int x = TimeToPixel( t );
			int y = GetTall() - ValueToPixel( f ) - 1;
			if ( k )
			{
				surface()->DrawLine( lastx, lasty, x, y );
			}
			lastx = x;
			lasty = y;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up colors
//-----------------------------------------------------------------------------
void CChannelGraphPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_font = pScheme->GetFont( "DefaultVerySmall" );

	surface()->GetTextSize( m_font, L"999.9", m_nGraphOriginX, m_nGraphOriginY );
	m_nGraphOriginX += 2 * m_nTextBorder;
	m_nGraphOriginY += 2 * m_nTextBorder;

	SetFgColor(GetSchemeColor("CChannelGraphPanel.FgColor", pScheme));
	SetBgColor(GetSchemeColor("CChannelGraphPanel.BgColor", pScheme));
	SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
}


//-----------------------------------------------------------------------------
//
// CChannelGraphFrame methods 
//
//-----------------------------------------------------------------------------
CChannelGraphFrame::CChannelGraphFrame( Panel *parent, const char *pTitle )
: BaseClass( parent, "CChannelGraphFrame" )
{
	SetTitle( pTitle, true );

	SetSizeable( true );
	SetCloseButtonVisible( true );
	SetMinimumSize( 200, 100 );

	SetVisible( true );

	SetSize( 400, 200 );
	SetPos( 100, 100 );

	m_pChannelGraph = new CChannelGraphPanel( this, "ChannelGraph" );

	SetScheme( vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" ) );
}

void CChannelGraphFrame::SetChannel( CDmeChannel *pChannel )
{
	m_pChannelGraph->SetChannel( pChannel );
}

void CChannelGraphFrame::OnCommand( const char *cmd )
{
	BaseClass::OnCommand( cmd );
	m_pChannelGraph->OnCommand( cmd );
}

void CChannelGraphFrame::PerformLayout()
{
	BaseClass::PerformLayout();

	int border = 5;
	int iWidth, iHeight;
	GetSize( iWidth, iHeight );
	m_pChannelGraph->SetPos( border, GetCaptionHeight() + border );
	m_pChannelGraph->SetSize( iWidth - 2 * border, iHeight - GetCaptionHeight() - 2 * border );
}
