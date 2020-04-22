//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <windows.h>
#include "mxExpressionSlider.h"
#include "expressiontool.h"
#include "mathlib/mathlib.h"
#include "hlfaceposer.h"
#include "choreowidgetdrawhelper.h"

mxExpressionSlider::mxExpressionSlider (mxWindow *parent, int x, int y, int w, int h, int id )
 : mxWindow( parent, x, y, w, h )
{
	setId( id );
	setType( MX_SLIDER );

	FacePoser_AddWindowStyle( this, WS_CLIPCHILDREN | WS_CLIPSIBLINGS );

	m_flMin[ 0 ] = 0.0;
	m_flMax[ 0 ] = 1.0f;
	m_nTicks[ 0 ] = 20;
	m_flCurrent[ 0 ] = 0.0f;

	m_flMin[ 1 ] = 0.0;
	m_flMax[ 1 ] = 1.0f;
	m_nTicks[ 1 ] = 20;
	m_flCurrent[ 1 ] = 0.5f;

	m_flSetting[ 0 ] = 0.0;
	m_flSetting[ 1 ] = 0.0;

	m_bIsEdited[ 0 ] = false;
	m_bIsEdited[ 1 ] = false;

	m_bDraggingThumb = false;
	m_nCurrentBar = 0;

	m_bPaired = false;

	m_nTitleWidth = 120;
	m_bDrawTitle = true;

	m_pInfluence = new mxCheckBox( this, 2, 4, 12, 12, "", IDC_INFLUENCE );
}

mxExpressionSlider::~mxExpressionSlider( void )
{
}

void mxExpressionSlider::SetTitleWidth( int width )
{
	m_nTitleWidth = width;
	redraw();
}

void mxExpressionSlider::SetDrawTitle( bool drawTitle )
{
	m_bDrawTitle = drawTitle;
	redraw();
}


void mxExpressionSlider::SetMode( bool paired )
{
	if ( m_bPaired != paired )
	{
		m_bPaired = paired;
		redraw();
	}
}

void mxExpressionSlider::BoundValue( void )
{
	for ( int i = 0; i < NUMBARS; i++ )
	{
		if ( m_flCurrent[ i ] > m_flMax[ i ] )
		{
			m_flCurrent[ i ] = m_flMax[ i ];
		}
		if ( m_flCurrent[ i ] < m_flMin[ i ] )
		{
			m_flCurrent[ i ] = m_flMin[ i ];
		}
	}
}

	
void mxExpressionSlider::setValue( int barnum, float value )
{
	if (m_flSetting[ barnum ] == value && m_bIsEdited[ barnum ] == false)
		return;

	m_flSetting[ barnum ] = value;
	m_bIsEdited[ barnum ] = false;

	if (m_bPaired)
	{
		if (m_flSetting[ 0 ] < m_flSetting[ 1 ])
		{
			m_flCurrent[ 0 ] = m_flSetting[ 1 ];
			m_flCurrent[ 1 ] = 1.0 - (m_flSetting[ 0 ] / m_flSetting[ 1 ]) * 0.5;
		}
		else if (m_flSetting[ 0 ] > m_flSetting[ 1 ])
		{
			m_flCurrent[ 0 ] = m_flSetting[ 0 ];
			m_flCurrent[ 1 ] = (m_flSetting[ 1 ] / m_flSetting[ 0 ]) * 0.5;
		}
		else
		{
			m_flCurrent[ 0 ] = m_flSetting[ 0 ];
			m_flCurrent[ 1 ] = 0.5;
		}
	}
	else
	{
		m_flCurrent[ barnum ] = value;
	}

	BoundValue();
	// FIXME: delay this until all sliders are set
	if (!m_bPaired || barnum == 1)
		redraw();
}

void mxExpressionSlider::setRange( int barnum, float min, float max, int ticks /*= 100*/ )
{
	m_flMin[ barnum ] = min;
	m_flMax[ barnum ] = max;
	
	Assert( m_flMax[ barnum ] > m_flMin[ barnum ] );

	m_nTicks[ barnum ] = ticks;
	
	BoundValue();

	redraw();
}

void mxExpressionSlider::setInfluence( float value )
{
	bool bWasChecked = m_pInfluence->isChecked( );
	bool bNowChecked = value > 0.0f ? true : false;
	if (bNowChecked != bWasChecked)
	{
		m_pInfluence->setChecked( bNowChecked );
		redraw();
	}
}

float mxExpressionSlider::getRawValue( int barnum ) const
{
	return m_flCurrent[ barnum ];
}

float mxExpressionSlider::getValue( int barnum ) const
{
	float scale = 1.0;
	if (m_bPaired)
	{
		// if it's paired, this is assuming that m_flCurrent[0] holds the max value, 
		// and m_flCurrent[1] is a weighting from 0 to 1, with 0.5 being even
		if (barnum == 0 && m_flCurrent[ 1 ] > 0.5)
		{
			scale = (1.0 - m_flCurrent[ 1 ]) / 0.5;
		}
		else if (barnum == 1 && m_flCurrent[ 1 ] < 0.5)
		{
			scale = (m_flCurrent[ 1 ] / 0.5);
		}
	}

	return m_flCurrent[ 0 ] * scale;
}

float mxExpressionSlider::getMinValue( int barnum ) const
{
	return m_flMin[ barnum ];
}

float mxExpressionSlider::getMaxValue( int barnum ) const
{
	return m_flMax[ barnum ];
}

float mxExpressionSlider::getInfluence(  ) const
{
	return m_pInfluence->isChecked() ? 1.0f : 0.0f;
}

void mxExpressionSlider::setEdited( int barnum, bool isEdited )
{
	if (m_bIsEdited[ barnum ] == isEdited)
		return;
	
	m_bIsEdited[ barnum ] = isEdited;
	redraw();
}

bool mxExpressionSlider::isEdited( int barnum ) const
{
	return (m_bIsEdited[ barnum ]);
}

void mxExpressionSlider::GetSliderRect( RECT& rc )
{
	HWND wnd = (HWND)getHandle();
	if ( !wnd )
		return;

	GetClientRect( wnd, &rc );

	if ( m_bDrawTitle )
	{
		rc.left += m_nTitleWidth;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &rc - 
//-----------------------------------------------------------------------------
void mxExpressionSlider::GetBarRect( RECT &rcBar )
{
	RECT rc;
	GetSliderRect( rc );

	rcBar = rc;

	InflateRect( &rcBar, -10, 0 );
	rcBar.top += 5;
	rcBar.bottom = rcBar.top + 2;

	int midy = ( rcBar.top + rcBar.bottom ) / 2;
	rcBar.top = midy - 1;
	rcBar.bottom = midy + 1;
}


void mxExpressionSlider::GetThumbRect( int barnum, RECT &rcThumb )
{
	RECT rc;
	GetSliderRect( rc );

	RECT rcBar = rc;
	GetBarRect( rcBar );

	float frac = 0.0f;

	if ( m_flMax[ barnum ] - m_flMin[ barnum ] > 0 )
	{
		frac = (m_flCurrent[ barnum ] - m_flMin[ barnum ]) / ( m_flMax[ barnum ] - m_flMin[ barnum ] );
	}

	int tickmark = (int)( frac * m_nTicks[ barnum ] + 0.5 );
	tickmark = min( m_nTicks[ barnum ], tickmark );
	tickmark = max( 0, tickmark );

	int thumbwidth = 20;
	int thumbheight = 14;
	int xoffset = -thumbwidth / 2;
	int yoffset = -thumbheight / 2 + 2;

	int leftedge = rcBar.left + (int)( (float)( rcBar.right - rcBar.left ) * (float)(tickmark) / (float)m_nTicks[ barnum ] );

	rcThumb.left = leftedge + xoffset;
	rcThumb.right = rcThumb.left + thumbwidth;
	rcThumb.top = rcBar.top + yoffset;
	rcThumb.bottom = rcThumb.top + thumbheight;
}

void mxExpressionSlider::DrawBar( HDC& dc )
{
	RECT rcBar;

	GetBarRect( rcBar );

	HPEN oldPen;

	HPEN shadow;
	HBRUSH face;
	HPEN hilight;

	shadow = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_3DSHADOW ) );
	hilight = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_3DHIGHLIGHT ) );
	face = CreateSolidBrush( GetSysColor( COLOR_3DFACE ) );

	oldPen = (HPEN)SelectObject( dc, hilight );

	MoveToEx( dc, rcBar.left, rcBar.bottom, NULL );
	LineTo( dc, rcBar.left, rcBar.top );
	LineTo( dc, rcBar.right, rcBar.top );

	SelectObject( dc, shadow );

	LineTo( dc, rcBar.right, rcBar.bottom );
	LineTo( dc, rcBar.left, rcBar.bottom );

	rcBar.left += 1;
	//rcBar.right -= 1;
	rcBar.top += 1;
	rcBar.bottom -= 1;

	FillRect( dc, &rcBar, face );

	SelectObject( dc, oldPen );

	DeleteObject( face );
	DeleteObject( shadow );
	DeleteObject( hilight );
}

void mxExpressionSlider::DrawThumb( int barnum, HDC& dc )
{
	RECT rcThumb;

	GetThumbRect( barnum, rcThumb );

	// Draw it


	HPEN oldPen;

	HPEN shadow;
	HBRUSH face;
	HPEN hilight;

	shadow = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_3DDKSHADOW ) );
	hilight = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_3DHIGHLIGHT ) );
	
	switch ( barnum )
	{
	default:
	case MAGNITUDE_BAR:
		{
			float frac;
			if (m_flCurrent[ barnum ] < 0)
			{
				frac = m_flCurrent[ barnum ] / m_flMin[ barnum ];
			}
			else
			{
				frac = m_flCurrent[ barnum ] / m_flMax[ barnum ];
			}
			frac = min( 1.0f, frac );
			frac = max( 0.0f, frac );

			COLORREF clr = GetSysColor( COLOR_3DFACE );
			int r, g, b;
			r = GetRValue( clr );
			g = GetRValue( clr );
			b = GetRValue( clr );

			// boost colors
			r = (int)( (1-frac) * b );
			b = min( 255, (int)(r + ( 255 - r ) * frac ) );
			g = (int)( (1-frac) * g );

			face = CreateSolidBrush( RGB( r, g, b ) );
		}
		break;
	case BALANCE_BAR:
		{
			float frac = m_flCurrent[ barnum ];
			frac = min( 1.0f, frac );
			frac = max( 0.0f, frac );

			COLORREF clr = GetSysColor( COLOR_3DFACE );
			int r, g, b;
			r = GetRValue( clr );
			g = GetRValue( clr );
			b = GetRValue( clr );

			// boost colors toward red if we are not at 0.5
			float boost = 2.0 * ( fabs( frac - 0.5f ) );

			r = r + ( 255 - r ) * boost;
			g = ( 1 - boost ) * g;
			b = ( 1 - boost ) * b;

			face = CreateSolidBrush( RGB( r, g, b ) );
		}
		break;
	}

	//rcThumb.left += 1;
	//rcThumb.right -= 1;
	//rcThumb.top += 1;
	//rcThumb.bottom -= 1;

	//FillRect( dc, &rcThumb, face );
	POINT region[3];
	int cPoints = 3;

	InflateRect( &rcThumb, -2, 0 );

//	int height = rcThumb.bottom - rcThumb.top;
//	int offset = height / 2 + 1;
	int offset = 2;

	switch ( barnum )
	{
	case MAGNITUDE_BAR:
	default:
		{
			region[ 0 ].x = rcThumb.left;
			region[ 0 ].y = rcThumb.top;
			
			region[ 1 ].x = rcThumb.right;
			region[ 1 ].y = rcThumb.top;
			
			region[ 2 ].x = ( rcThumb.left + rcThumb.right ) / 2;
			region[ 2 ].y = rcThumb.bottom - offset;
		}
		break;
	case BALANCE_BAR:
		{
			region[ 0 ].x = ( rcThumb.left + rcThumb.right ) / 2;
			region[ 0 ].y = rcThumb.top + offset;

			region[ 1 ].x = rcThumb.left;
			region[ 1 ].y = rcThumb.bottom;

			region[ 2 ].x = rcThumb.right;
			region[ 2 ].y = rcThumb.bottom;

		}
		break;
	}

	HRGN rgn = CreatePolygonRgn( region, cPoints, ALTERNATE );

	int oldPF = SetPolyFillMode( dc, ALTERNATE );
	FillRgn( dc, rgn, face );
	SetPolyFillMode( dc, oldPF );

	DeleteObject( rgn );

	oldPen = (HPEN)SelectObject( dc, hilight );

	MoveToEx( dc, region[0].x, region[0].y, NULL );
	LineTo( dc, region[1].x, region[1].y );
	SelectObject( dc, shadow );
	LineTo( dc, region[2].x, region[2].y );
	SelectObject( dc, hilight );
	LineTo( dc, region[0].x, region[0].y );

	SelectObject( dc, oldPen );

	DeleteObject( face );
	DeleteObject( shadow );
	DeleteObject( hilight );
}

void mxExpressionSlider::DrawTitle( HDC &dc )
{
	if ( !m_bDrawTitle )
		return;

	HWND wnd = (HWND)getHandle();
	if ( !wnd )
		return;

	RECT rc;
	GetClientRect( wnd, &rc );
	rc.right = m_nTitleWidth;
	rc.left += 16;

	InflateRect( &rc, -5, -2 );

	char sz[ 128 ];
	sprintf( sz, "%s", getLabel() );

	HFONT fnt, oldfont;

	fnt = CreateFont(
		-12					             // H
		, 0   					         // W
		, 0								 // Escapement
		, 0							     // Orient
		, FW_NORMAL 					 // Wt.  (BOLD)
		, 0							 // Ital.
		, 0             				 // Underl.
		, 0                 			 // SO
		, ANSI_CHARSET		    		 // Charset
		, OUT_TT_PRECIS					 // Out prec.
		, CLIP_DEFAULT_PRECIS			 // Clip prec.
		, PROOF_QUALITY   			     // Qual.
		, VARIABLE_PITCH | FF_DONTCARE   // Pitch and Fam.
		, "Arial" );

	COLORREF oldColor;

	if (!isEdited( 0 ))
	{
		oldColor = SetTextColor( dc, GetSysColor( COLOR_BTNTEXT ) );
	}
	else
	{
		oldColor = SetTextColor( dc, RGB( 255, 0, 0 ) );
	}
	int oldMode = SetBkMode( dc, TRANSPARENT );
	oldfont = (HFONT)SelectObject( dc, fnt );

	DrawText( dc, sz, -1, &rc, DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_LEFT | DT_WORD_ELLIPSIS );
	
	SelectObject( dc, oldfont );
	DeleteObject( fnt );
	SetBkMode( dc, oldMode );
	SetTextColor( dc, oldColor );
}

void mxExpressionSlider::redraw()
{
	HWND wnd = (HWND)getHandle();
	if ( !wnd )
		return;

	HDC finalDC = GetDC( wnd );
	if ( !finalDC )
		return;

	RECT rc;
	GetClientRect( wnd, &rc );

	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	HDC dc = CreateCompatibleDC( finalDC );
	HBITMAP oldbm, bm;

	bm = CreateCompatibleBitmap( finalDC, w, h );

	oldbm = (HBITMAP)SelectObject( dc, bm );

	HBRUSH br = CreateSolidBrush( GetSysColor( COLOR_3DFACE ) );

	FillRect( dc, &rc, br );

	DeleteObject( br );

	DrawTitle( dc );

	DrawBar( dc );

	// Draw slider
	for ( int i = ( m_bPaired ? 1 : 0 ); i >= 0 ; i-- )
	{
		DrawThumb( i, dc );
	}

	BitBlt( finalDC, 0, 0, w, h, dc, 0, 0, SRCCOPY );

	SelectObject( dc, oldbm );

	DeleteObject( bm );

	DeleteDC( dc );

	ReleaseDC( wnd, finalDC );
	ValidateRect( wnd, &rc );
}

void mxExpressionSlider::MoveThumb( int barnum, int xpos, bool finish )
{
	RECT rcBar;
	
	GetBarRect( rcBar );

	if ( xpos < rcBar.left )
	{
		m_flCurrent[ barnum ] = m_flMin[ barnum ];
	}
	else if ( xpos > rcBar.right )
	{
		m_flCurrent[ barnum ] = m_flMax[ barnum ];
	}
	else
	{
		float frac = (float)( xpos - rcBar.left ) / (float)( rcBar.right - rcBar.left );
		// snap slider to nearest "tick" so that it get drawn in the correct place
		int curtick = (int)( frac * m_nTicks[ 0 ] + 0.5);
		m_flCurrent[ barnum ] = m_flMin[ barnum ] + ((float)curtick / (float)m_nTicks[ 0 ]) * (m_flMax[ barnum ] - m_flMin[ barnum ]);
	}

	// update equivalent setting
	m_flSetting[ 0 ] = getValue( 0 );
	m_flSetting[ 1 ] = getValue( 1 );

	m_bIsEdited[ 0 ] = true;
	m_bIsEdited[ 1 ] = true;

	// Send message to parent
	HWND parent = (HWND)( getParent() ? getParent()->getHandle() : NULL );
	if ( parent )
	{
		LPARAM lp;
		WPARAM wp;

		wp = MAKEWPARAM( finish ? SB_ENDSCROLL : SB_THUMBPOSITION, barnum );
		lp = (long)getHandle();

		SendMessage( parent, WM_HSCROLL, wp, lp );
	}

	BoundValue();
	redraw();
}

bool mxExpressionSlider::PaintBackground( void )
{
	return false;
}

int mxExpressionSlider::handleEvent( mxEvent *event )
{
	int iret = 0;
	switch ( event->event )
	{
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_INFLUENCE:
				{
					SetFocus( (HWND)getHandle() );

					setEdited( 0, false );
					setEdited( 1, false );

					// Send message to parent
					HWND parent = (HWND)( getParent() ? getParent()->getHandle() : NULL );
					if ( parent )
					{
						LPARAM lp;
						WPARAM wp;

						wp = MAKEWPARAM( SB_ENDSCROLL, m_nCurrentBar );
						lp = (long)getHandle();

						SendMessage( parent, WM_HSCROLL, wp, lp );
					}
					break;
				}
			}
		}
		break;
	case mxEvent::MouseDown:
		{
			SetFocus( (HWND)getHandle() );

			if ( !m_bDraggingThumb )
			{
				RECT rcThumb;
				POINT pt;

				pt.x = (short)event->x;
				pt.y = (short)event->y;

				m_nCurrentBar = ( event->buttons & mxEvent::MouseRightButton ) ? BALANCE_BAR : MAGNITUDE_BAR;
				GetThumbRect( m_nCurrentBar, rcThumb );

				if ( PtInRect( &rcThumb, pt ) )
				{
					m_bDraggingThumb = true;
				}

				// Snap position if they didn't click on the thumb itself
#if 0
				else
				{
					m_bDraggingThumb = true;
					MoveThumb( m_nCurrentBar, (short)event->x, false );
				}
#endif
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseDrag:
	case mxEvent::MouseMove:
		{
			if ( m_bDraggingThumb )
			{
				m_pInfluence->setChecked( true );
				MoveThumb( m_nCurrentBar, (short)event->x, false );
				iret = 1;
			}
		}
		break;
	case mxEvent::MouseUp:
		{
			if ( m_bDraggingThumb )
			{
				m_pInfluence->setChecked( true );
				m_bDraggingThumb = false;
				MoveThumb( m_nCurrentBar, (short)event->x, true );
				m_nCurrentBar = 0;
			}
			iret = 1;
		}
		break;
	case mxEvent::KeyDown:
		{
			if ( event->key == VK_RETURN ||
				 event->key == 'S' )
			{
				// See if there is an event in the flex animation window
				g_pExpressionTool->OnSetSingleKeyFromFlex( getLabel() );
			}
		}
		break;
	}

	return iret;
}
