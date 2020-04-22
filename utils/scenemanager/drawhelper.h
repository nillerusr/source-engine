//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <mxtk/mx.h>
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: Helper class that automagically sets up and destroys a memory device-
//  context for flicker-free refershes
//-----------------------------------------------------------------------------
class CDrawHelper
{
public:
	// Construction/destruction
				CDrawHelper( mxWindow *widget);
				CDrawHelper( mxWindow *widget, COLORREF bgColor );
				CDrawHelper( mxWindow *widget, int x, int y, int w, int h, COLORREF bgColor );
				CDrawHelper( mxWindow *widget, RECT& bounds );
				CDrawHelper( mxWindow *widget, RECT& bounds, COLORREF bgColor );
	virtual		~CDrawHelper( void );

	// Allow caller to draw onto the memory dc, too
	HDC			GrabDC( void );

	// Compute text size
	static int	CalcTextWidth( const char *font, int pointsize, int weight, PRINTF_FORMAT_STRING const char *fmt, ... );
	static int	CalcTextWidth( HFONT font, PRINTF_FORMAT_STRING const char *fmt, ... );

	int			CalcTextWidthW( const char *font, int pointsize, int weight, PRINTF_FORMAT_STRING const wchar_t *fmt, ... );
	int			CalcTextWidthW( HFONT font, PRINTF_FORMAT_STRING const wchar_t *fmt, ... );
	void		DrawColoredTextW( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const wchar_t *fmt, ... );
	void		DrawColoredTextW( HFONT font, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const wchar_t *fmt, ... );
	void		DrawColoredTextCharsetW( const char *font, int pointsize, int weight, DWORD charset, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const wchar_t *fmt, ... );

	void		CalcTextRect( const char *font, int pointsize, int weight, int maxwidth, RECT& rcText, PRINTF_FORMAT_STRING const char *fmt, ... );

	// Draw text
	void		DrawColoredText( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const char *fmt, ... );
	void		DrawColoredText( HFONT font, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const char *fmt, ... );
	void		DrawColoredTextCharset( const char *font, int pointsize, int weight, DWORD charset, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const char *fmt, ... );
	void		DrawColoredTextMultiline( const char *font, int pointsize, int weight, COLORREF clr, RECT& rcText, PRINTF_FORMAT_STRING const char *fmt, ... );
	// Draw a line
	void		DrawColoredLine( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2 );
	void		DrawColoredPolyLine( COLORREF clr, int style, int width, CUtlVector< POINT >& points );

	// Draw a blending ramp
	POINTL		DrawColoredRamp( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2, float rate, float sustain );
	// Draw a filled rect
	void		DrawFilledRect( COLORREF clr, int x1, int y1, int x2, int y2 );
	// Draw an outlined rect
	void		DrawOutlinedRect( COLORREF clr, int style, int width, int x1, int y1, int x2, int y2 );
	void		DrawOutlinedRect( COLORREF clr, int style, int width, RECT& rc );

	void		DrawFilledRect( HBRUSH br, RECT& rc );
	void		DrawFilledRect( COLORREF clr, RECT& rc );

	void		DrawGradientFilledRect( RECT& rc, COLORREF clr1, COLORREF clr2, bool vertical );

	void		DrawLine( int x1, int y1, int x2, int y2, COLORREF clr, int thickness );

	// Draw a triangle
	void		DrawTriangleMarker( RECT& rc, COLORREF fill, bool inverted = false );

	void		DrawCircle( COLORREF clr, int x, int y, int radius, bool filled = true );

	// Get width/height of draw area
	int			GetWidth( void );
	int			GetHeight( void );

	// Get client rect for drawing
	void		GetClientRect( RECT& rc );

	void		StartClipping( RECT& clipRect );
	void		StopClipping( void );

	// Remap rect if we're using a clipped viewport
	void		OffsetSubRect( RECT& rc );

private:
	// Internal initializer
	void		Init( mxWindow *widget, int x, int y, int w, int h, COLORREF bgColor);

	void		ClipToRects( void );

	// The window we are drawing on
	HWND		m_hWnd;
	// The final DC
	HDC			m_dcReal;
	// The working DC
	HDC			m_dcMemory;
	// Client area and offsets
	RECT		m_rcClient;
	int			m_x, m_y;
	int			m_w, m_h;
	// Bitmap for drawing in the memory DC
	HBITMAP		m_bmMemory;
	HBITMAP		m_bmOld;
	// Remember the original default color
	COLORREF	m_clrOld;

	CUtlVector < RECT > m_ClipRects;
	HRGN		m_ClipRegion;
};
