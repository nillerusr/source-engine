//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tabwindow.h"
#include "DrawHelper.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *parent - 
//			x - 
//			y - 
//			w - 
//			h - 
//			id - 
//			style - 
//-----------------------------------------------------------------------------
CTabWindow::CTabWindow( mxWindow *parent, int x, int y, int w, int h, int id /*= 0*/, int style /*=0*/ )
: mxWindow( parent, x, y, w, h, "", style )
{
	setId( id );

	m_nSelected = -1;

	m_nRowHeight = 20;
	m_nRowsRequired = 1;

	m_nTabWidth = 80;
	m_nPixelDelta = 3;
	m_bInverted = false;
	m_bRightJustify = false;
	SetColor( COLOR_BG, GetSysColor( COLOR_BTNFACE ) );
	SetColor( COLOR_FG, GetSysColor( COLOR_INACTIVECAPTION ) );
	SetColor( COLOR_FG_SELECTED, GetSysColor( COLOR_ACTIVECAPTION ) );
	SetColor( COLOR_HILITE, GetSysColor( COLOR_3DSHADOW ) );
	SetColor( COLOR_HILITE_SELECTED, GetSysColor( COLOR_3DHILIGHT ) );
	SetColor( COLOR_TEXT, GetSysColor( COLOR_BTNTEXT ) );
	SetColor( COLOR_TEXT_SELECTED, GetSysColor( COLOR_INACTIVECAPTIONTEXT ) );

	SceneManager_AddWindowStyle( this, WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CTabWindow::~CTabWindow
//-----------------------------------------------------------------------------
CTabWindow::~CTabWindow ( void )
{
	removeAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			clr - 
//-----------------------------------------------------------------------------
void CTabWindow::SetColor( int index, COLORREF clr )
{
	if ( index < 0 || index >= NUM_COLORS )
		return;

	m_Colors[ index ] = clr;
}

void CTabWindow::SetInverted( bool invert )
{
	m_bInverted = invert;
	RecomputeLayout( w2() );
}

void CTabWindow::SetRightJustify( bool rightjustify )
{
	m_bRightJustify = true;
	RecomputeLayout( w2() );
}

//-----------------------------------------------------------------------------
// Purpose: Tabs are sized to string content
// Input  : rcClient - 
//			tabRect - 
//			tabNum - 
//-----------------------------------------------------------------------------
void CTabWindow::GetTabRect( const RECT& rcClient, RECT& tabRect, int tabNum )
{
	tabRect = m_Items[ tabNum ].rect;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcClient - 
//			tabnum - 
//			selected - 
//-----------------------------------------------------------------------------
void CTabWindow::DrawTab( CDrawHelper& drawHelper, RECT& rcClient, int tabnum, bool selected )
{
	RECT rcTab;

	if ( tabnum < 0 || tabnum >= m_Items.Size() )
		return;

#if defined( _DEBUG )
	CETItem *p = &m_Items[ tabnum ];
	Assert( p );
#endif

	GetTabRect( rcClient, rcTab, tabnum );

	COLORREF fgcolor = m_Colors[ selected ? COLOR_FG_SELECTED : COLOR_FG ];
	COLORREF hilightcolor = m_Colors[ selected ? COLOR_HILITE_SELECTED : COLOR_HILITE ];
	COLORREF text = m_Colors[ selected ? COLOR_TEXT_SELECTED : COLOR_TEXT ];

	// Create a trapezoid/paralleogram
	POINT region[4];
	int cPoints = 4;

	OffsetRect( &rcTab, 0, m_bInverted ? 1 : -1 );

	if ( m_bInverted )
	{
		region[ 0 ].x = rcTab.left - m_nPixelDelta;
		region[ 0 ].y = rcTab.top;

		region[ 1 ].x = rcTab.right + m_nPixelDelta;
		region[ 1 ].y = rcTab.top;

		region[ 2 ].x = rcTab.right - m_nPixelDelta;
		region[ 2 ].y = rcTab.bottom;

		region[ 3 ].x = rcTab.left + m_nPixelDelta;
		region[ 3 ].y = rcTab.bottom;
	}
	else
	{
		region[ 0 ].x = rcTab.left + m_nPixelDelta;
		region[ 0 ].y = rcTab.top;

		region[ 1 ].x = rcTab.right - m_nPixelDelta;
		region[ 1 ].y = rcTab.top;

		region[ 2 ].x = rcTab.right + m_nPixelDelta;
		region[ 2 ].y = rcTab.bottom;

		region[ 3 ].x = rcTab.left - m_nPixelDelta;
		region[ 3 ].y = rcTab.bottom;
	}

	HDC dc = drawHelper.GrabDC();

	HRGN rgn = CreatePolygonRgn( region, cPoints, ALTERNATE );

	int oldPF = SetPolyFillMode( dc, ALTERNATE );
	
	HBRUSH brBg = CreateSolidBrush( fgcolor );
	HBRUSH brBorder = CreateSolidBrush( hilightcolor );
	//HBRUSH brInset = CreateSolidBrush( fgcolor );

	FillRgn( dc, rgn, brBg );
	FrameRgn( dc, rgn, brBorder, 1, 1 );

	SetPolyFillMode( dc, oldPF );

	DeleteObject( rgn );

	DeleteObject( brBg );
	DeleteObject( brBorder );
	//DeleteObject( brInset );

	// Position label
	InflateRect( &rcTab, -5, 0 );
	OffsetRect( &rcTab, 2, 0 );

	// Draw label
	drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, text, rcTab, "%s%s", getPrefix( tabnum ), getLabel( tabnum ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTabWindow::redraw( void )
{
	CDrawHelper drawHelper( this, m_Colors[ COLOR_BG ] );

	int liney = m_bInverted ? 1 : h2() - 2;

	drawHelper.DrawColoredLine( m_Colors[ COLOR_HILITE ], PS_SOLID, 1, 0, liney, w(), liney );
	RECT rc;
	drawHelper.GetClientRect( rc );

	// Draw non-selected first
	for ( int i = 0 ; i < m_Items.Size(); i++ )
	{
		if ( i == m_nSelected )
			continue;

		DrawTab( drawHelper, rc, i );
	}

	// Draw selected last, so that it appears to pop to top of z order
	if ( m_nSelected >= 0 && m_nSelected < m_Items.Size() )
	{
		DrawTab( drawHelper, rc, m_nSelected, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : int
//-----------------------------------------------------------------------------
int CTabWindow::GetItemUnderMouse( int mx, int my )
{
	RECT rcClient;
	GetClientRect( (HWND)getHandle(), &rcClient );

	for ( int i = 0; i < m_Items.Size() ; i++ )
	{
		RECT rcTab;
		GetTabRect( rcClient, rcTab, i );

		if ( mx < rcTab.left || 
			 mx > rcTab.right ||
			 my < rcTab.top ||
			 my > rcTab.bottom )
		{
			 continue;
		}
		return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int CTabWindow::handleEvent
//-----------------------------------------------------------------------------
int CTabWindow::handleEvent (mxEvent *event)
{
	int iret = 0;

	switch ( event->event )
	{
	case mxEvent::MouseDown:
		{
			int item = GetItemUnderMouse( (short)event->x, (short)event->y );
			if ( item != -1 )
			{
				m_nSelected = item;
				redraw();

				// Send CBN_SELCHANGE WM_COMMAND message to parent
				HWND parent = (HWND)( getParent() ? getParent()->getHandle() : NULL );
				if ( parent )
				{
					LPARAM lp;
					WPARAM wp;

					wp = MAKEWPARAM( getId(), CBN_SELCHANGE );
					lp = (long)getHandle();

					PostMessage( parent, WM_COMMAND, wp, lp );
				}
				iret = 1;
			}

			if ( event->buttons & mxEvent::MouseRightButton )
			{
				ShowRightClickMenu( (short)event->x, (short)event->y );
				iret = 1;
			}
		}
		break;
	case mxEvent::Size:
		{
			RecomputeLayout( w2() );
		}
		break;
	}
	return iret;
}

//-----------------------------------------------------------------------------
// Purpose: Add string to table
// Input  : *item - 
//-----------------------------------------------------------------------------
void CTabWindow::add( const char *item )
{
	m_Items.AddToTail();
	CETItem *p = &m_Items[ m_Items.Size() - 1 ];
	Assert( p );

	Q_memset( &p->rect, 0, sizeof( p->rect) );

	strcpy( p->m_szString, item );
	p->m_szPrefix[ 0 ] = 0;
	m_nSelected = min( m_nSelected, m_Items.Size() - 1 );
	m_nSelected = max( m_nSelected, 0 );

	RecomputeLayout( w2() );

	redraw();
}

void CTabWindow::setPrefix( int item, char const *prefix )
{
	if ( item < 0 || item >= m_Items.Size() )
		return;

	strncpy( m_Items[ item ].m_szPrefix, prefix, sizeof( m_Items[ item ].m_szPrefix ) );
//	RecomputeLayout( w2() );
}

//-----------------------------------------------------------------------------
// Purpose: Change selected item
// Input  : index - 
//-----------------------------------------------------------------------------
void CTabWindow::select( int index )
{
	if ( index < 0 || index >= m_Items.Size() )
		return;

	m_nSelected = index;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: Remove a string
// Input  : index - 
//-----------------------------------------------------------------------------
void CTabWindow::remove( int index )
{
	if ( index < 0 || index >= m_Items.Size() )
		return;
	
	m_Items.Remove( index );

	m_nSelected = min( m_nSelected, m_Items.Size() - 1 );
	m_nSelected = max( m_nSelected, 0 );

	RecomputeLayout( w2() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: Clear out everything
//-----------------------------------------------------------------------------
void CTabWindow::removeAll()
{
	m_nSelected = -1;
	m_Items.RemoveAll();

	RecomputeLayout( w2() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int 
//-----------------------------------------------------------------------------
int CTabWindow::getItemCount () const
{
	return m_Items.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int	
//-----------------------------------------------------------------------------
int	CTabWindow::getSelectedIndex () const
{
	// Convert based on override index
	return m_nSelected;
}

char const *CTabWindow::getLabel( int item )
{
	if ( item < 0 || item >= m_Items.Count() )
		return "";

	return m_Items[ item ].m_szString;
}

char const	*CTabWindow::getPrefix( int item )
{
	if ( item < 0 || item >= m_Items.Count() )
		return "";

	return m_Items[ item ].m_szPrefix;
}

void CTabWindow::SetRowHeight( int rowheight )
{
	m_nRowHeight = rowheight;
	RecomputeLayout( w2() );
	redraw();
}

int CTabWindow::GetBestHeight( int width )
{
	return RecomputeLayout( width, false ) * m_nRowHeight;
}


int CTabWindow::RecomputeLayout( int windowWidth, bool dolayout /*=true*/ )
{
	// Draw non-selected first
	int curedge = m_nPixelDelta + 1;
	int curtop = 0;

	if ( m_bRightJustify )
	{
		curedge = windowWidth - ( m_nPixelDelta + 1 ) - 5;
	}

	int startedge = curedge;

	int currentrow = 0;

	for ( int i = 0 ; i < m_Items.Size(); i++ )
	{
		CETItem *p = &m_Items[ i ];

		RECT rc;

		int textwidth = CDrawHelper::CalcTextWidth( "Arial", 9, FW_NORMAL, "%s%s", p->m_szPrefix, p->m_szString ) + 15;

		if ( !m_bRightJustify )
		{
			// Starting column
			if ( curedge + textwidth > windowWidth )
			{
				curedge = startedge;
				curtop += m_nRowHeight;
				currentrow++;
			}

			rc.left = curedge;
			rc.right = curedge + textwidth;
			rc.top = curtop + 2;
			rc.bottom = curtop + m_nRowHeight;

			curedge += textwidth;

			p->rect = rc;
		}
		else
		{
			// Starting column
			if ( curedge - textwidth < 0 )
			{
				curedge = startedge;
				curtop += m_nRowHeight;
				currentrow++;
			}

			rc.left = curedge - textwidth;
			rc.right = curedge;
			rc.top = curtop;
			rc.bottom = curtop + m_nRowHeight - 2;

			curedge -= textwidth;
		}

		if ( dolayout )
		{
			p->rect = rc;
		}
	}

	if ( dolayout )
	{
		m_nRowsRequired = currentrow + 1;
	}

	return currentrow + 1;
}
