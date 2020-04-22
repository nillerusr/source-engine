//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <mxtk/mx.h>
#include "mxStatusWindow.h"
#include "hlfaceposer.h"
#include "choreowidgetdrawhelper.h"
#include "MDLViewer.h"
#include "faceposertoolwindow.h"

extern double realtime;

mxStatusWindow *g_pStatusWindow = NULL;

#define STATUS_SCROLLBAR_SIZE		12
#define STATUS_FONT_SIZE			9

mxStatusWindow::mxStatusWindow(mxWindow *parent, int x, int y, int w, int h, const char *label /*= 0*/ ) 
: mxWindow( parent, x, y, w, h, label ), IFacePoserToolWindow( "Status Window", "Output" ), m_pScrollbar(NULL)
{
	for ( int i = 0; i < MAX_TEXT_LINES; i++ )
	{
		m_rgTextLines[ i ].m_szText[ 0 ] = 0;
		m_rgTextLines[ i ].rgb = CONSOLE_COLOR;
		m_rgTextLines[ i ].curtime = 0;
	}
	m_nCurrentLine = 0;

	m_pScrollbar = new mxScrollbar( this, 0, 0, STATUS_SCROLLBAR_SIZE, 100, IDC_STATUS_SCROLL, mxScrollbar::Vertical );
	m_pScrollbar->setRange( 0, 1000 );
	m_pScrollbar->setPagesize( 100 );
}

mxStatusWindow::~mxStatusWindow()
{
	g_pStatusWindow = NULL;
}

void mxStatusWindow::redraw()
{
//	if ( !ToolCanDraw() )
//		return;

	if ( !m_pScrollbar )
		return;

	CChoreoWidgetDrawHelper helper( this, RGB( 0, 0, 0 ) );
	HandleToolRedraw( helper );

	RECT rc;
	helper.GetClientRect( rc );

	RECT rcText = rc;

	int lineheight = ( STATUS_FONT_SIZE + 2 );

	InflateRect( &rcText, -4, 0 );
	rcText.bottom = h2() - 4;
	rcText.top = rcText.bottom - lineheight;

	//int minval = m_pScrollbar->getMinValue();
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

		helper.DrawColoredText( "Arial", STATUS_FONT_SIZE, FW_NORMAL, m_rgTextLines[ line ].rgb, rcTime, ptext );

		OffsetRect( &rcText, 0, -lineheight );
	}

	DrawActiveTool();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool mxStatusWindow::PaintBackground( void )
{
	redraw();
	return false;
}

void mxStatusWindow::StatusPrint( COLORREF clr, bool overwrite, const char *text )
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

	m_rgTextLines[ i ].rgb = clr;
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
void mxStatusWindow::PositionSliders( int sboffset )
{
	int lineheight = ( STATUS_FONT_SIZE + 2 );

	int linesused = min( (int)MAX_TEXT_LINES, m_nCurrentLine );
	linesused = max( linesused, 1 );

	int trueh = h2() - GetCaptionHeight();

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
int mxStatusWindow::handleEvent( mxEvent *event )
{
	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	default:
		break;
	case mxEvent::Size:
		{
			m_pScrollbar->setBounds( w2() - STATUS_SCROLLBAR_SIZE, GetCaptionHeight(), STATUS_SCROLLBAR_SIZE, h2()-GetCaptionHeight() );
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
						DrawActiveTool();
					}
				}
				break;
			}
		}
		break;
	}

	return iret;
}
#include "StudioModel.h"

#include "faceposer_models.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxStatusWindow::DrawActiveTool()
{
	RECT rcTool;
	rcTool.left = 0;
	rcTool.top = GetCaptionHeight() + 2;
	rcTool.bottom = h2();
	rcTool.right = w2() - 16;

	rcTool.bottom = rcTool.top + 10;
	rcTool.left = rcTool.right - 500;

	char sz[ 256 ];

	IFacePoserToolWindow *activeTool = IFacePoserToolWindow::GetActiveTool();

	static float lastrealtime = 0.0f;

	float dt = (float)realtime - lastrealtime;
	dt = clamp( dt, 0.0f, 1.0f );

	float fps = 0.0f;
	if ( dt > 0.0001f )
	{
		fps = 1.0f / dt;
	}

	sprintf( sz, "%s (%i) at %.3f (%.2f fps) (soundcount %i)", 
		activeTool ? activeTool->GetToolName() : "None", 
		g_MDLViewer->GetCurrentFrame(), 
		(float)realtime, 
		fps,
		models->CountActiveSources() );

	lastrealtime = realtime;

	int len = CChoreoWidgetDrawHelper::CalcTextWidth( "Courier New", 10, FW_NORMAL, sz );

	CChoreoWidgetDrawHelper helper( this, rcTool, RGB( 32, 0, 0 ) );

	rcTool.left = rcTool.right - len - 15;

	helper.DrawColoredText( "Courier New", 10, FW_NORMAL, RGB( 255, 255, 200 ), rcTool, sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void mxStatusWindow::Think( float dt )
{
	DrawActiveTool();
}