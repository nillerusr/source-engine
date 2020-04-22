//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "tier0/dbg.h"
#include <stdio.h>
#include "choreoview.h"
#include "choreowidgetdrawhelper.h"
#include "choreoviewcolors.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget )
{
	Init( widget, 0, 0, 0, 0, COLOR_CHOREO_BACKGROUND, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, COLORREF bgColor )
{
	Init( widget, 0, 0, 0, 0, bgColor, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			bounds - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds )
{
	Init( widget, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, COLOR_CHOREO_BACKGROUND, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			bounds - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds, bool noPageFlip )
{
	Init( widget, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, COLOR_CHOREO_BACKGROUND, noPageFlip );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, int x, int y, int w, int h, COLORREF bgColor )
{
	Init( widget, x, y, w, h, bgColor, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			bounds - 
//			bgColor - 
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::CChoreoWidgetDrawHelper( mxWindow *widget, RECT& bounds, COLORREF bgColor )
{
	Init( widget, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, bgColor, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::Init( mxWindow *widget, int x, int y, int w, int h, COLORREF bgColor, bool noPageFlip )
{
	m_bNoPageFlip = noPageFlip;

	m_x = x;
	m_y = y;

	m_w = w ? w : widget->w2();
	m_h = h ? h : widget->h2();

	m_hWnd = (HWND)widget->getHandle();
	Assert( m_hWnd );
	m_dcReal = GetDC( m_hWnd );
	m_rcClient.left		= m_x;
	m_rcClient.top		= m_y;
	m_rcClient.right	=  m_x + m_w;
	m_rcClient.bottom	= m_y + m_h;

	if ( !noPageFlip )
	{
		m_dcMemory = CreateCompatibleDC( m_dcReal );
		m_bmMemory = CreateCompatibleBitmap( m_dcReal, m_w, m_h );
		m_bmOld = (HBITMAP)SelectObject( m_dcMemory, m_bmMemory );
	}
	else
	{
		m_dcMemory = m_dcReal;
		m_x = m_y = 0;
	}

	m_clrOld = SetBkColor( m_dcMemory, bgColor );

	RECT rcFill = m_rcClient;
	OffsetRect( &rcFill, -m_rcClient.left, -m_rcClient.top );

	if ( !noPageFlip )
	{
		HBRUSH br = CreateSolidBrush( bgColor );
		FillRect( m_dcMemory, &rcFill, br );
		DeleteObject( br );
	}

	m_ClipRegion = (HRGN)0;
}

//-----------------------------------------------------------------------------
// Purpose: Finish up
//-----------------------------------------------------------------------------
CChoreoWidgetDrawHelper::~CChoreoWidgetDrawHelper( void )
{
	SelectClipRgn( m_dcMemory, NULL );

	while ( m_ClipRects.Size() > 0 )
	{
		StopClipping();
	}

	if ( !m_bNoPageFlip )
	{
		BitBlt( m_dcReal, m_x, m_y, m_w, m_h, m_dcMemory, 0, 0, SRCCOPY );

		SetBkColor( m_dcMemory, m_clrOld );

		SelectObject( m_dcMemory, m_bmOld );
		DeleteObject( m_bmMemory );

		DeleteObject( m_dcMemory );
	}

	ReleaseDC( m_hWnd, m_dcReal );

	ValidateRect( m_hWnd, &m_rcClient );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::GetWidth( void )
{
	return m_w;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::GetHeight( void )
{
	return m_h;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::GetClientRect( RECT& rc )
{
	rc.left = rc.top = 0;
	rc.right = m_w;
	rc.bottom = m_h;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : HDC
//-----------------------------------------------------------------------------
HDC CChoreoWidgetDrawHelper::GrabDC( void )
{
	return m_dcMemory;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			maxwidth - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::CalcTextRect( const char *font, int pointsize, int weight, int maxwidth, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 font );

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );

	DrawText( m_dcMemory, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_WORDBREAK | DT_CALCRECT );

	SelectObject( m_dcMemory, oldFont );
	DeleteObject( fnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			*fmt - 
//			... - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::CalcTextWidth( const char *font, int pointsize, int weight, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 font );

	HDC screen = GetDC( NULL );

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = pointsize + 5;
	rcText.right = rcText.left + 2048;

	DrawText( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	SelectObject( screen, oldFont );
	DeleteObject( fnt );

	ReleaseDC( NULL, screen );

	return rcText.right;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			*fmt - 
//			... - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::CalcTextWidthW( const char *font, int pointsize, int weight, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vwprintf( fmt, args );
	vswprintf( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 font );

	HDC screen = GetDC( NULL );

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = pointsize + 5;
	rcText.right = rcText.left + 2048;

	DrawTextW( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	SelectObject( screen, oldFont );
	DeleteObject( fnt );

	ReleaseDC( NULL, screen );

	return rcText.right;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fnt - 
//			*fmt - 
//			... - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoWidgetDrawHelper::CalcTextWidth( HFONT fnt, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	HDC screen = GetDC( NULL );

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = 1000;
	rcText.right = rcText.left + 2048;

	DrawText( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	SelectObject( screen, oldFont );

	ReleaseDC( NULL, screen );

	return rcText.right;
}

int CChoreoWidgetDrawHelper::CalcTextWidthW( HFONT fnt, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vwprintf( fmt, args );
	vswprintf( output, fmt, args );

	HDC screen = GetDC( NULL );

	HFONT oldFont = (HFONT)SelectObject( screen, fnt );

	RECT rcText;
	rcText.left = rcText.top = 0;
	rcText.bottom = 1000;
	rcText.right = rcText.left + 2048;

	DrawTextW( screen, output, -1, &rcText, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT );

	SelectObject( screen, oldFont );

	ReleaseDC( NULL, screen );

	return rcText.right;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredText( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vsprintf( output, fmt, args );
	va_end( args  );
	
	DrawColoredTextCharset( font, pointsize, weight, ANSI_CHARSET, clr, rcText, output );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextW( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vswprintf( output, fmt, args );
	va_end( args  );
	
	DrawColoredTextCharsetW( font, pointsize, weight, ANSI_CHARSET, clr, rcText, output );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredText( HFONT font, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vsprintf( output, fmt, args );
	va_end( args  );
	
	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, font );
	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	int oldMode = SetBkMode( m_dcMemory, TRANSPARENT );

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawText( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );

	SetBkMode( m_dcMemory, oldMode );

	SetTextColor( m_dcMemory, oldColor );

	SelectObject( m_dcMemory, oldFont );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextW( HFONT font, COLORREF clr, RECT& rcText, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vswprintf( output, fmt, args );
	va_end( args  );
	
	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, font );
	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	int oldMode = SetBkMode( m_dcMemory, TRANSPARENT );

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawTextW( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );

	SetBkMode( m_dcMemory, oldMode );

	SetTextColor( m_dcMemory, oldColor );

	SelectObject( m_dcMemory, oldFont );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextCharset( const char *font, int pointsize, int weight, DWORD charset, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vsprintf( output, fmt, args );
	va_end( args  );
	

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 charset,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 font );

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	int oldMode = SetBkMode( m_dcMemory, TRANSPARENT );

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawText( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );

	SetBkMode( m_dcMemory, oldMode );

	SetTextColor( m_dcMemory, oldColor );

	SelectObject( m_dcMemory, oldFont );
	DeleteObject( fnt );
}

void CChoreoWidgetDrawHelper::DrawColoredTextCharsetW( const char *font, int pointsize, int weight, DWORD charset, COLORREF clr, RECT& rcText, const wchar_t *fmt, ... )
{
	va_list args;
	static wchar_t output[1024];

	va_start( args, fmt );
	vswprintf( output, fmt, args );
	va_end( args  );
	

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 charset,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 font );

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	int oldMode = SetBkMode( m_dcMemory, TRANSPARENT );

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawTextW( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS );

	SetBkMode( m_dcMemory, oldMode );

	SetTextColor( m_dcMemory, oldColor );

	SelectObject( m_dcMemory, oldFont );
	DeleteObject( fnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *font - 
//			pointsize - 
//			weight - 
//			clr - 
//			rcText - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredTextMultiline( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	HFONT fnt = CreateFont(
		 -pointsize, 
		 0,
		 0,
		 0,
		 weight,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 font );

	HFONT oldFont = (HFONT)SelectObject( m_dcMemory, fnt );
	COLORREF oldColor = SetTextColor( m_dcMemory, clr );
	int oldMode = SetBkMode( m_dcMemory, TRANSPARENT );

	RECT rcTextOffset = rcText;
	OffsetSubRect( rcTextOffset );

	DrawText( m_dcMemory, output, -1, &rcTextOffset, DT_LEFT | DT_NOPREFIX | DT_VCENTER | DT_WORDBREAK | DT_WORD_ELLIPSIS );

	SetBkMode( m_dcMemory, oldMode );

	SetTextColor( m_dcMemory, oldColor );

	SelectObject( m_dcMemory, oldFont );
	DeleteObject( fnt );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			style - 
//			width - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredLine( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2 )
{
	HPEN pen = CreatePen( style, width, clr );
	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	MoveToEx( m_dcMemory, x1-m_x, y1-m_y, NULL );
	LineTo( m_dcMemory, x2-m_x, y2-m_y );
	SelectObject( m_dcMemory, oldPen );
	DeleteObject( pen );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clr - 
//			style - 
//			width - 
//			count - 
//			*pts - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawColoredPolyLine( COLORREF clr, int style, int width, CUtlVector< POINT >& points )
{
	int c = points.Count();
	if ( c < 2 )
		return;

	HPEN pen = CreatePen( style, width, clr );
	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );

	POINT *temp = (POINT *)_alloca( c * sizeof( POINT ) );
	Assert( temp );
	int i;
	for ( i = 0; i < c; i++ )
	{
		POINT *pt = &points[ i ];

		temp[ i ].x = pt->x - m_x;
		temp[ i ].y = pt->y - m_y;
	}
	
	Polyline( m_dcMemory, temp, c );

	SelectObject( m_dcMemory, oldPen );
	DeleteObject( pen );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			style - 
//			width - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
POINTL CChoreoWidgetDrawHelper::DrawColoredRamp( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2, float rate, float sustain )
{
	HPEN pen = CreatePen( style, width, clr );
	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	MoveToEx( m_dcMemory, x1-m_x, y1-m_y, NULL );
	int dx = x2 - x1;
	int dy = y2 - y1;

	POINTL p;
	p.x = 0L;
	p.y = 0L;
	for (float i = 0.1f; i <= 1.09f; i += 0.1f)
	{
		float j = 3.0f * i * i - 2.0f * i * i * i;
		p.x = x1+(int)(dx*i*(1.0f-rate))-m_x;
		p.y = y1+(int)(dy*sustain*j)-m_y;
		LineTo( m_dcMemory, p.x, p.y );
	}
	SelectObject( m_dcMemory, oldPen );
	DeleteObject( pen );

	return p;
};

//-----------------------------------------------------------------------------
// Purpose: Draw a filled rect
// Input  : clr - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawFilledRect( COLORREF clr, RECT& rc )
{
	RECT rcCopy = rc;

	HBRUSH br = CreateSolidBrush( clr );
	OffsetSubRect( rcCopy );
	FillRect( m_dcMemory, &rcCopy, br );
	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a filled rect
// Input  : clr - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawFilledRect( COLORREF clr, int x1, int y1, int x2, int y2 )
{
	HBRUSH br = CreateSolidBrush( clr );
	RECT rc;
	rc.left = x1;
	rc.right = x2;
	rc.top = y1;
	rc.bottom = y2;
	OffsetSubRect( rc );
	FillRect( m_dcMemory, &rc, br );
	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clr - 
//			style - 
//			width - 
//			rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawOutlinedRect( COLORREF clr, int style, int width, RECT& rc )
{
	DrawOutlinedRect( clr, style, width, rc.left, rc.top, rc.right, rc.bottom );
}

//-----------------------------------------------------------------------------
// Purpose: Draw an outlined rect
// Input  : clr - 
//			style - 
//			width - 
//			x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawOutlinedRect( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2 )
{
	HPEN oldpen, pen;
	HBRUSH oldbrush, brush;

	pen = CreatePen( PS_SOLID, width, clr );
	oldpen = (HPEN)SelectObject( m_dcMemory, pen );

	brush = (HBRUSH)GetStockObject( NULL_BRUSH );
	oldbrush = (HBRUSH)SelectObject( m_dcMemory, brush );

	RECT rc;
	rc.left = x1;
	rc.right = x2;
	rc.top = y1;
	rc.bottom = y2;
	OffsetSubRect( rc);

	Rectangle( m_dcMemory, rc.left, rc.top, rc.right, rc.bottom );

	SelectObject( m_dcMemory, oldbrush );
	DeleteObject( brush );
	SelectObject( m_dcMemory, oldpen );
	DeleteObject( pen );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x1 - 
//			y1 - 
//			x2 - 
//			y2 - 
//			clr - 
//			thickness - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawLine( int x1, int y1, int x2, int y2, COLORREF clr, int thickness )
{
	HPEN oldpen, pen;
	HBRUSH oldbrush, brush;

	pen = CreatePen( PS_SOLID, thickness, clr );
	oldpen = (HPEN)SelectObject( m_dcMemory, pen );

	brush = (HBRUSH)GetStockObject( NULL_BRUSH );
	oldbrush = (HBRUSH)SelectObject( m_dcMemory, brush );

	// Offset
	x1 -= m_x;
	x2 -= m_x;
	y1 -= m_y;
	y2 -= m_y;

	MoveToEx( m_dcMemory, x1, y1, NULL );
	LineTo( m_dcMemory, x2, y2 );

	SelectObject( m_dcMemory, oldbrush );
	DeleteObject( brush );
	SelectObject( m_dcMemory, oldpen );
	DeleteObject( pen );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//			fillr - 
//			fillg - 
//			fillb - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawTriangleMarker( RECT& rc, COLORREF fill, bool inverted /*= false*/ )
{
	POINT region[3];
	int cPoints = 3;

	if ( !inverted )
	{
		region[ 0 ].x = rc.left - m_x;
		region[ 0 ].y = rc.top - m_y;

		region[ 1 ].x = rc.right - m_x;
		region[ 1 ].y = rc.top - m_y;

		region[ 2 ].x = ( ( rc.left + rc.right ) / 2 ) - m_x;
		region[ 2 ].y = rc.bottom - m_y;
	}
	else
	{
		region[ 0 ].x = rc.left - m_x;
		region[ 0 ].y = rc.bottom - m_y;

		region[ 1 ].x = rc.right - m_x;
		region[ 1 ].y = rc.bottom - m_y;

		region[ 2 ].x = ( ( rc.left + rc.right ) / 2 ) - m_x;
		region[ 2 ].y = rc.top - m_y;
	}

	HRGN rgn = CreatePolygonRgn( region, cPoints, ALTERNATE );

	int oldPF = SetPolyFillMode( m_dcMemory, ALTERNATE );
	
	HBRUSH brFace = CreateSolidBrush( fill );

	FillRgn( m_dcMemory, rgn, brFace );

	DeleteObject( brFace );
	
	SetPolyFillMode( m_dcMemory, oldPF );

	DeleteObject( rgn );
}

void CChoreoWidgetDrawHelper::StartClipping( RECT& clipRect )
{
	RECT fixed = clipRect;
	OffsetSubRect( fixed );

	m_ClipRects.AddToTail( fixed );

	ClipToRects();
}

void CChoreoWidgetDrawHelper::StopClipping( void )
{
	Assert( m_ClipRects.Size() > 0 );
	if ( m_ClipRects.Size() <= 0 )
		return;

	m_ClipRects.Remove( m_ClipRects.Size() - 1 );

	ClipToRects();
}

void CChoreoWidgetDrawHelper::ClipToRects( void )
{
	SelectClipRgn( m_dcMemory, NULL );
	if ( m_ClipRegion )
	{
		DeleteObject( m_ClipRegion );
		m_ClipRegion = HRGN( 0 );
	}

	if ( m_ClipRects.Size() > 0 )
	{
		RECT rc = m_ClipRects[ 0 ];
		m_ClipRegion = CreateRectRgn( rc.left, rc.top, rc.right, rc.bottom );
		for ( int i = 1; i < m_ClipRects.Size(); i++ )
		{
			RECT add = m_ClipRects[ i ];

			HRGN addIn = CreateRectRgn( add.left, add.top, add.right, add.bottom );
			HRGN result = CreateRectRgn( 0, 0, 100, 100 );

			CombineRgn( result, m_ClipRegion, addIn, RGN_AND );
			
			DeleteObject( m_ClipRegion );
			DeleteObject( addIn );

			m_ClipRegion = result;
		}
	}

	SelectClipRgn( m_dcMemory, m_ClipRegion );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::OffsetSubRect( RECT& rc )
{
	OffsetRect( &rc, -m_x, -m_y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : br - 
//			rc - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawFilledRect( HBRUSH br, RECT& rc )
{
	RECT rcFill = rc;
	OffsetSubRect( rcFill );
	FillRect( m_dcMemory, &rcFill, br );
}

void CChoreoWidgetDrawHelper::DrawCircle( COLORREF clr, int x, int y, int radius, bool filled /*= true*/ )
{
	RECT rc;
	int ihalfradius = radius >> 1;

	rc.left = x - ihalfradius;
	rc.right = rc.left + 2 * ihalfradius;
	rc.top = y - ihalfradius;
	rc.bottom = y + 2 * ihalfradius - 1;

	OffsetSubRect( rc );

	HPEN pen = CreatePen( PS_SOLID, 1, clr );
	HBRUSH br = CreateSolidBrush( clr );

	HPEN oldPen = (HPEN)SelectObject( m_dcMemory, pen );
	HBRUSH oldBr = (HBRUSH)SelectObject( m_dcMemory, br );

	if ( filled )
	{
		Ellipse( m_dcMemory, rc.left, rc.top, rc.right, rc.bottom );
	}
	else
	{
		Arc( m_dcMemory, rc.left, rc.top, rc.right, rc.bottom,
			rc.left, rc.top, rc.left, rc.top );
	}
	
	SelectObject( m_dcMemory, oldPen );
	SelectObject( m_dcMemory, oldBr );

	DeleteObject( pen );
	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//			clr1 - 
//			clr2 - 
//			vertical - 
//-----------------------------------------------------------------------------
void CChoreoWidgetDrawHelper::DrawGradientFilledRect( RECT& rc, COLORREF clr1, COLORREF clr2, bool vertical )
{
	RECT rcDraw = rc;
	OffsetRect( &rcDraw, -m_x, -m_y );

	TRIVERTEX        vert[2] ;
	GRADIENT_RECT    gradient_rect;
	vert[0].x      = rcDraw.left;
	vert[0].y      = rcDraw.top;
	vert[0].Red    = GetRValue( clr1 ) << 8;
	vert[0].Green  = GetGValue( clr1 ) << 8;
	vert[0].Blue   = GetBValue( clr1 ) << 8;
	vert[0].Alpha  = 0x0000;
	
	vert[1].x      = rcDraw.right;
	vert[1].y      = rcDraw.bottom; 
	vert[1].Red    = GetRValue( clr2 ) << 8;
	vert[1].Green  = GetGValue( clr2 ) << 8;
	vert[1].Blue   = GetBValue( clr2 ) << 8;
	vert[1].Alpha  = 0x0000;

	gradient_rect.UpperLeft  = 0;
	gradient_rect.LowerRight = 1;

	GradientFill(
		m_dcMemory,
		vert, 2,
		&gradient_rect, 1,
		vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H );
}
