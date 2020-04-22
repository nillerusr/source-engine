//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "statuswindow.h"
#include "drawhelper.h"

CStatusWindow *g_pStatusWindow = NULL;

#define STATUS_SCROLLBAR_SIZE		12
#define STATUS_FONT_SIZE			10

CStatusWindow::CStatusWindow(mxWindow *parent, int x, int y, int w, int h, const char *label /*= 0*/ ) 
: mxWindow( parent, x, y, w, h, label ), m_pScrollbar(NULL)
{
	for ( int i = 0; i < MAX_TEXT_LINES; i++ )
	{
		m_rgTextLines[ i ].m_szText[ 0 ] = 0;
		m_rgTextLines[ i ].r = CONSOLE_R;
		m_rgTextLines[ i ].g = CONSOLE_G;
		m_rgTextLines[ i ].b = CONSOLE_B;
		m_rgTextLines[ i ].curtime = 0;
	}
	m_nCurrentLine = 0;

	m_pScrollbar = new mxScrollbar( this, 0, 0, STATUS_SCROLLBAR_SIZE, 100, IDC_STATUS_SCROLL, mxScrollbar::Vertical );
	m_pScrollbar->setRange( 0, 1000 );
	m_pScrollbar->setPagesize( 100 );
	g_pStatusWindow = this;
}

CStatusWindow::~CStatusWindow()
{
	g_pStatusWindow = NULL;
}

void CStatusWindow::redraw()
{
//	if ( !ToolCanDraw() )
//		return;

	if ( !m_pScrollbar )
		return;

	CDrawHelper helper( this, RGB( 0, 0, 0 ) );

	RECT rc;
	helper.GetClientRect( rc );

	RECT rcText = rc;

	int lineheight = ( STATUS_FONT_SIZE + 2 );

	InflateRect( &rcText, -4, 0 );
	rcText.bottom = h2() - 4;
	rcText.top = rcText.bottom - lineheight;

//	int minval = m_pScrollbar->getMinValue();
	int maxval = m_pScrollbar->getMaxValue();
	int pagesize = m_pScrollbar->getPagesize();
	int curval = m_pScrollbar->getValue();

	int offset = ( maxval - pagesize ) - curval;
	offset = ( offset + lineheight - 1 ) / lineheight;

	offset = max( 0, offset );
	//offset = 0;
	//offset += 10;
	//offset = max( 0, offset );

	for ( int i = 0; i < MAX_TEXT_LINES - offset; i++ )
	{
		int rawline = m_nCurrentLine - i - 1;
		if ( rawline <= 0 )
			continue;

		if ( rcText.bottom < 0 )
			break;

		int line = ( rawline - offset ) & TEXT_LINE_MASK;

		COLORREF clr = RGB( m_rgTextLines[ line ].r, m_rgTextLines[ line ].g, m_rgTextLines[ line ].b );

		char *ptext = m_rgTextLines[ line ].m_szText;
		
		RECT rcTime = rcText;
		rcTime.right = rcTime.left + 50;

		char sz[ 32 ];
		sprintf( sz, "%.3f",  m_rgTextLines[ line ].curtime );

		int len = helper.CalcTextWidth( "Arial", STATUS_FONT_SIZE, FW_NORMAL, sz );

		rcTime.left = rcTime.right - len - 5;

		helper.DrawColoredText( "Arial", STATUS_FONT_SIZE, FW_NORMAL, RGB( 255, 255, 150 ), rcTime, sz );

		rcTime = rcText;
		rcTime.left += 50;

		helper.DrawColoredText( "Arial", STATUS_FONT_SIZE, FW_NORMAL, clr, rcTime, ptext );

		OffsetRect( &rcText, 0, -lineheight );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CStatusWindow::PaintBackground( void )
{
	redraw();
	return false;
}

void CStatusWindow::StatusPrint( int r, int g, int b, bool overwrite, const char *text )
{
	float curtime = (float)Plat_FloatTime();

	char sz[32];
	sprintf( sz, "%.3f  ", curtime );

	OutputDebugString( sz );
	OutputDebugString( text );

	char fixedtext[ 512 ];
	char *in, *out;
	in = (char *)text;
	out = fixedtext;

	int c = 0;
	while ( *in && c < 511 )
	{
		if ( *in == '\n' || *in == '\r' )
		{
			in++;
		}
		else
		{
			*out++ = *in++;
			c++;
		}
	}
	*out = 0;

	if ( overwrite )
	{
		m_nCurrentLine--;
	}

	int i =  m_nCurrentLine & TEXT_LINE_MASK;

	strncpy( m_rgTextLines[ i ].m_szText, fixedtext, 511 );
	m_rgTextLines[ i ].m_szText[ 511 ] = 0;

	m_rgTextLines[ i ].r = r;
	m_rgTextLines[ i ].g = g;
	m_rgTextLines[ i ].b = b;
	m_rgTextLines[ i ].curtime = curtime;

	m_nCurrentLine++;

	if ( m_nCurrentLine <= MAX_TEXT_LINES )
	{
		PositionSliders( 0 );
	}
	m_pScrollbar->setValue( m_pScrollbar->getMaxValue() );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sboffset - 
//-----------------------------------------------------------------------------
void CStatusWindow::PositionSliders( int sboffset )
{
	int lineheight = ( STATUS_FONT_SIZE + 2 );

	int linesused = min( (int)MAX_TEXT_LINES, m_nCurrentLine );
	linesused = max( linesused, 1 );

	int trueh = h2();

	int vpixelsneeded = max( linesused * lineheight, trueh );
	m_pScrollbar->setVisible( linesused * lineheight > trueh );
	

	m_pScrollbar->setPagesize( trueh );
	m_pScrollbar->setRange( 0, vpixelsneeded );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int CStatusWindow::handleEvent( mxEvent *event )
{
	int iret = 0;

	switch ( event->event )
	{
	default:
		break;
	case mxEvent::Size:
		{
			m_pScrollbar->setBounds( w2() - STATUS_SCROLLBAR_SIZE, 0, STATUS_SCROLLBAR_SIZE, h2() );
			PositionSliders( 0 );
			m_pScrollbar->setValue( m_pScrollbar->getMaxValue() );
			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_STATUS_SCROLL:
				{
					if ( event->event == mxEvent::Action &&
						event->modifiers == SB_THUMBTRACK)
					{
						int offset = event->height;
						m_pScrollbar->setValue( offset ); 
						PositionSliders( offset );
					}
				}
				break;
			}
		}
		break;
	}

	return iret;
}
