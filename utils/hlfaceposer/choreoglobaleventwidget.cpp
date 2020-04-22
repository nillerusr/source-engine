//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include "hlfaceposer.h"
#include "choreoviewcolors.h"
#include "choreoglobaleventwidget.h"
#include "choreowidgetdrawhelper.h"
#include "ChoreoView.h"
#include "choreoevent.h"
#include "choreoeventwidget.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CChoreoGlobalEventWidget::CChoreoGlobalEventWidget( CChoreoWidget *parent )
: CChoreoWidget( parent )
{
	m_pEvent = NULL;

	m_bDragging			= false;
	m_xStart			= 0;
	m_hPrevCursor		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoGlobalEventWidget::~CChoreoGlobalEventWidget( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoGlobalEventWidget::Create( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoGlobalEventWidget::Layout( RECT& rc )
{
	setBounds( rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top );
}

//-----------------------------------------------------------------------------
// Redraw to screen
//-----------------------------------------------------------------------------
void CChoreoGlobalEventWidget::redraw( CChoreoWidgetDrawHelper& drawHelper )
{
	if ( !getVisible() )
		return;

	CChoreoEvent *event = GetEvent();
	if ( !event )
		return;

	RECT rcTab;
	rcTab = getBounds();

	bool isLoop = false;
	COLORREF pointColor = COLOR_CHOREO_SEGMENTDIVIDER;
	COLORREF clr = COLOR_CHOREO_SEGMENTDIVIDER_BG;
	switch ( event->GetType() )
	{
	default:
		break;
	case CChoreoEvent::LOOP:
		{
			clr				= COLOR_CHOREO_LOOPPOINT_BG;
			pointColor		= COLOR_CHOREO_LOOPPOINT;
			isLoop			= true;
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			clr				= COLOR_CHOREO_STOPPOINT_BG;
			pointColor		= COLOR_CHOREO_STOPPOINT;
		}
		break;
	}

	if ( IsSelected() )
	{
		InflateRect( &rcTab, 2, 2 );

		drawHelper.DrawTriangleMarker( rcTab, pointColor );

		InflateRect( &rcTab, -2, -2 );

		drawHelper.DrawTriangleMarker( rcTab, RGB( 240, 240, 220 ) );

	}
	else
	{
		drawHelper.DrawTriangleMarker( rcTab, pointColor );
	}

	RECT rcClient;
	drawHelper.GetClientRect( rcClient );

	RECT rcLine = rcTab;
	rcLine.top = rcTab.bottom + 2;
	rcLine.bottom = rcClient.bottom;
	rcLine.left = ( rcLine.left + rcLine.right ) / 2;
	rcLine.right = rcLine.left;

	if ( IsSelected() )
	{
		drawHelper.DrawColoredLine( clr, PS_DOT, 2, rcLine.left, rcLine.top, rcLine.right, rcLine.bottom );
	}
	else
	{
		drawHelper.DrawColoredLine( clr, PS_DOT, 1, rcLine.left, rcLine.top, rcLine.right, rcLine.bottom );
	}

	if ( event->GetType() == CChoreoEvent::STOPPOINT )
	{
		OffsetRect( &rcTab, -4, 15 );

		mxbitmapdata_t *image = CChoreoEventWidget::GetImage( event->GetType() );
		if ( image )
		{
			drawHelper.OffsetSubRect( rcTab );
			DrawBitmapToDC( drawHelper.GrabDC(), rcTab.left, rcTab.top, 16, 16, *image );	
		}
	}

	if ( !isLoop )
		return;

	COLORREF labelText = COLOR_INFO_TEXT;
	DrawLabel( drawHelper, labelText, rcLine.left, rcLine.top + 2, false );

	// Figure out loop spot
	float looptime = (float)atof( event->GetParameters() );

	// Find pixel for that
	bool clipped = false;
	int x = m_pView->GetPixelForTimeValue( looptime, &clipped );
	if ( clipped )
		return;

	rcLine.left = x;
	rcLine.right = x;

	clr = COLOR_CHOREO_LOOPPOINT_START_BG;
	drawHelper.DrawColoredLine( clr, PS_SOLID, 1, rcLine.left, rcLine.top, rcLine.right, rcLine.top + 28);

	DrawLabel( drawHelper, labelText, rcLine.left, rcLine.top + 2, true );
}

void CChoreoGlobalEventWidget::DrawLabel( CChoreoWidgetDrawHelper& drawHelper, COLORREF clr, int x, int y, bool right )
{
	CChoreoEvent *event = GetEvent();
	if ( !event )
		return;

	int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, va( "%s", event->GetName() ) );

	RECT rcText;
	rcText.top = y;
	rcText.bottom = y + 10;
	rcText.left = x - len / 2;
	rcText.right = rcText.left + len;

	if ( !right )
	{
		drawHelper.DrawColoredTextCharset( "Marlett", 9, FW_NORMAL, SYMBOL_CHARSET, clr, rcText, "3" );
		OffsetRect( &rcText, 8, 0 );
		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, clr, rcText, va( "%s", event->GetName() ) );
	}
	else
	{
		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, clr, rcText, va( "%s", event->GetName() ) );
		OffsetRect( &rcText, len, 0 );
		drawHelper.DrawColoredTextCharset( "Marlett", 9, FW_NORMAL, SYMBOL_CHARSET, clr, rcText, "4" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoGlobalEventWidget::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	::DrawFocusRect( dc, &m_rcFocus );

	ReleaseDC( NULL, dc );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CChoreoEvent
//-----------------------------------------------------------------------------
CChoreoEvent *CChoreoGlobalEventWidget::GetEvent( void )
{
	return m_pEvent;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoGlobalEventWidget::SetEvent( CChoreoEvent *event )
{
	m_pEvent = event;
}