//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdio.h>
#include "hlfaceposer.h"
#include "GestureTool.h"
#include "mdlviewer.h"
#include "choreowidgetdrawhelper.h"
#include "TimelineItem.h"
#include "expressions.h"
#include "expclass.h"
#include "choreoevent.h"
#include "StudioModel.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "ChoreoView.h"
#include "InputProperties.h"
#include "ControlPanel.h"
#include "FlexPanel.h"
#include "mxExpressionTray.h"
#include "ExpressionProperties.h"
#include "tier1/strtools.h"
#include "faceposer_models.h"
#include "UtlBuffer.h"
#include "filesystem.h"
#include "iscenetokenprocessor.h"
#include "choreoviewcolors.h"
#include "MatSysWin.h"

GestureTool *g_pGestureTool = 0;

#define TRAY_HEIGHT 20
#define TRAY_ITEM_INSET 10

#define TAG_TOP ( TRAY_HEIGHT + 32 )
#define TAG_BOTTOM ( TAG_TOP + 20 )

#define MAX_TIME_ZOOM 1000
// 10% per step
#define TIME_ZOOM_STEP 2

float SnapTime( float input, float granularity );

GestureTool::GestureTool( mxWindow *parent )
: IFacePoserToolWindow( "GestureTool", "Gesture" ), mxWindow( parent, 0, 0, 0, 0 )
{
	m_bSuppressLayout = false;

	SetAutoProcess( true );

	m_nFocusEventGlobalID = -1;

	m_flScrub			= 0.0f;
	m_flScrubTarget		= 0.0f;
	m_nDragType			= DRAGTYPE_NONE;

	m_nClickedX			= 0;
	m_nClickedY			= 0;

	m_hPrevCursor		= 0;
	
	m_nStartX			= 0;
	m_nStartY			= 0;

	m_pLastEvent		= NULL;

	m_nMousePos[ 0 ] = m_nMousePos[ 1 ] = 0;

	m_nMinX				= 0;
	m_nMaxX				= 0;
	m_bUseBounds		= false;

	m_bLayoutIsValid = false;
	m_flPixelsPerSecond = 500.0f;

	m_flLastDuration = 0.0f;
	m_nScrollbarHeight	= 12;
	m_flLeftOffset = 0.0f;
	m_nLastHPixelsNeeded = -1;
	m_pHorzScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_GESTUREHSCROLL, mxScrollbar::Horizontal );
	m_pHorzScrollBar->setVisible( false );

	m_bInSetEvent = false;
	m_flScrubberTimeOffset = 0.0f;
}

GestureTool::~GestureTool( void )
{
}

void GestureTool::SetEvent( CChoreoEvent *event )
{
	if ( m_bInSetEvent )
		return;

	m_bInSetEvent = true;

	if ( event == m_pLastEvent )
	{
		if ( event )
		{
			if ( event->GetDuration() != m_flLastDuration )
			{
				m_flLastDuration = event->GetDuration();
				m_nLastHPixelsNeeded = -1;
				m_flLeftOffset = 0.0f;
				InvalidateLayout();
			}

			m_nFocusEventGlobalID = event->GetGlobalID();
		}

		m_bInSetEvent = false;
		return;
	}

	m_pLastEvent = event;

	m_nFocusEventGlobalID = -1;
	if ( event )
	{
		m_nFocusEventGlobalID = event->GetGlobalID();
	}
	
	if ( event )
	{
		m_flLastDuration = event->GetDuration();
	}
	else
	{
		m_flLastDuration = 0.0f;
	}
	m_flLeftOffset = 0.0f;
	m_nLastHPixelsNeeded = -1;
	InvalidateLayout();

	m_bInSetEvent = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoEvent *GestureTool::GetSafeEvent( void )
{
	if ( m_nFocusEventGlobalID == -1 )
		return NULL;

	if ( !g_pChoreoView )
		return NULL;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return NULL;

	// Find event by name
	for ( int i = 0; i < scene->GetNumEvents() ; i++ )
	{
		CChoreoEvent *e = scene->GetEvent( i );
		if ( !e || e->GetType() != CChoreoEvent::GESTURE )
			continue;

		if ( e->GetGlobalID() == m_nFocusEventGlobalID )
		{
			return e;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcHandle - 
//-----------------------------------------------------------------------------
void GestureTool::GetScrubHandleRect( RECT& rcHandle, float scrub, bool clipped )
{
	float pixel = 0.0f;
	if ( w2() > 0 )
	{
		pixel = GetPixelForTimeValue( scrub );

		if  ( clipped )
		{
			pixel = clamp( pixel, SCRUBBER_HANDLE_WIDTH / 2, w2() - SCRUBBER_HANDLE_WIDTH / 2 );
		}
	}

	rcHandle.left = pixel- SCRUBBER_HANDLE_WIDTH / 2;
	rcHandle.right = pixel + SCRUBBER_HANDLE_WIDTH / 2;
	rcHandle.top = 2 + GetCaptionHeight();
	rcHandle.bottom = rcHandle.top + SCRUBBER_HANDLE_HEIGHT;
}

void GestureTool::GetScrubHandleReferenceRect( RECT& rcHandle, float scrub, bool clipped /*= false*/ )
{
	float pixel = 0.0f;
	if ( w2() > 0 )
	{
		pixel = GetPixelForTimeValue( scrub );

		if  ( clipped )
		{
			pixel = clamp( pixel, SCRUBBER_HANDLE_WIDTH/2, w2() - SCRUBBER_HANDLE_WIDTH/2 );
		}
	}

	rcHandle.left = pixel-SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.right = pixel + SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.top = 2 + GetCaptionHeight() + 195;
	rcHandle.bottom = rcHandle.top + SCRUBBER_HANDLE_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcHandle - 
//-----------------------------------------------------------------------------
void GestureTool::DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle, float scrub, bool reference )
{
	HBRUSH br = CreateSolidBrush( reference ? RGB( 150, 0, 0 ) : RGB( 0, 150, 100 ) );

	COLORREF areaBorder = RGB( 230, 230, 220 );

	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.top, w2(), rcHandle.top );
	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.bottom, w2(), rcHandle.bottom );

	drawHelper.DrawFilledRect( br, rcHandle );

	// 
	char sz[ 32 ];
	sprintf( sz, "%.3f", scrub );

	CChoreoEvent *ev = GetSafeEvent();
	if ( ev )
	{
		float st, ed;
		st = ev->GetStartTime();
		ed = ev->GetEndTime();

		float dt = ed - st;
		if ( dt > 0.0f )
		{
			sprintf( sz, "%.3f", st + scrub );
		}
	}

	int len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );

	RECT rcText = rcHandle;

	int textw = rcText.right - rcText.left;

	rcText.left += ( textw - len ) / 2;

	drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 255, 255, 255 ), rcText, sz );

	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool GestureTool::IsMouseOverScrubHandle( mxEvent *event )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );
	InflateRect( &rcHandle, 2, 2 );

	POINT pt;
	pt.x = (short)event->x;
	pt.y = (short)event->y;
	if ( PtInRect( &rcHandle, pt ) )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool GestureTool::IsProcessing( void )
{
	if ( !GetSafeEvent() )
		return false;

	if ( m_flScrub != m_flScrubTarget )
		return true;

	return false;
}

bool GestureTool::IsScrubbing( void ) const
{
	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	return scrubbing;
}

void GestureTool::SetScrubTime( float t )
{
	m_flScrub = t;
	CChoreoEvent *e = GetSafeEvent();
	if ( e && e->GetDuration() )
	{
		float realtime = e->GetStartTime() + m_flScrub;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->DrawScrubHandle();
	}
}

void GestureTool::SetScrubTargetTime( float t )
{
	m_flScrubTarget = t;
	CChoreoEvent *e = GetSafeEvent();
	if ( e && e->GetDuration() )
	{
		float realtime = e->GetStartTime() + m_flScrubTarget;

		g_pChoreoView->SetScrubTargetTime( realtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void GestureTool::Think( float dt )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	bool scrubbing = IsScrubbing();
	ScrubThink( dt, scrubbing );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void GestureTool::ScrubThink( float dt, bool scrubbing )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	if ( m_flScrubTarget == m_flScrub && !scrubbing )
		return;

	float d = m_flScrubTarget - m_flScrub;
	int sign = d > 0.0f ? 1 : -1;

	float maxmove = dt;

	if ( sign > 0 )
	{
		if ( d < maxmove )
		{
			SetScrubTime( m_flScrubTarget );
		}
		else
		{
			SetScrubTime( m_flScrub + maxmove );
		}
	}
	else
	{
		if ( -d < maxmove )
		{
			SetScrubTime( m_flScrubTarget );
		}
		else
		{
			SetScrubTime( m_flScrub - maxmove );
		}
	}
	
	if ( scrubbing )
	{
		g_pMatSysWindow->Frame();
	}
}

void GestureTool::DrawScrubHandles()
{
	RECT rcTray;

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );

	rcTray = rcHandle;
	rcTray.left = 0;
	rcTray.right = w2();

	CChoreoWidgetDrawHelper drawHelper( this, rcTray );
	DrawScrubHandle( drawHelper, rcHandle, m_flScrub, false );
	
	CChoreoEvent *ev = GetSafeEvent();
	if ( ev && ev->GetDuration() > 0.0f )
	{
		float scrub = ev->GetOriginalPercentageFromPlaybackPercentage( m_flScrub / ev->GetDuration() ) * ev->GetDuration();
		GetScrubHandleReferenceRect( rcHandle, scrub, true );

		rcTray = rcHandle;
		rcTray.left = 0;
		rcTray.right = w2();

		CChoreoWidgetDrawHelper drawHelper( this, rcTray );
		DrawScrubHandle( drawHelper, rcHandle, scrub, true );
	}
}

void GestureTool::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper drawHelper( this );
	HandleToolRedraw( drawHelper );

	RECT rc;
	drawHelper.GetClientRect( rc );

	CChoreoEvent *ev = GetSafeEvent();
	if ( ev )
	{
		RECT rcText;
		drawHelper.GetClientRect( rcText );
		rcText.top += GetCaptionHeight()+1;
		rcText.bottom = rcText.top + 13;
		rcText.left += 5;
		rcText.right -= 5;

		OffsetRect( &rcText, 0, 12 );

		int current, total;

		g_pChoreoView->GetUndoLevels( current, total );
		if ( total > 0 )
		{
			RECT rcUndo = rcText;
			OffsetRect( &rcUndo, 0, 2 );

			drawHelper.DrawColoredText( "Small Fonts", 8, FW_NORMAL, RGB( 0, 100, 0 ), rcUndo,
				"Undo:  %i/%i", current, total );
		}

		rcText.left += 60;
		
		// Found it, write out description
		// 
		float seqduration;
		ev->GetGestureSequenceDuration( seqduration );

		RECT rcTextLine = rcText;

		drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 200, 0, 0 ), rcTextLine,
			"Event:  %s",
			ev->GetName() );

		OffsetRect( &rcTextLine, 0, 12 );

		drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 200, 0, 0 ), rcTextLine,
			"Sequence:  '%s' %.3f s.",
			ev->GetParameters(),
			seqduration );

		RECT rcTimeLine;
		drawHelper.GetClientRect( rcTimeLine );
		rcTimeLine.left = 0;
		rcTimeLine.right = w2();
		rcTimeLine.top += ( GetCaptionHeight() + 70 );

		float lefttime = GetTimeValueForMouse( 0 );
		float righttime = GetTimeValueForMouse( w2() );

		DrawTimeLine( drawHelper, rcTimeLine, lefttime, righttime );

		OffsetRect( &rcText, 0, 30 );

		rcText.left = 5;

		RECT timeRect = rcText;

		timeRect.right = timeRect.left + 100;

		char sz[ 32 ];

		Q_snprintf( sz, sizeof( sz ), "%.2f", lefttime + ev->GetStartTime() );

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );

		timeRect = rcText;

		Q_snprintf( sz, sizeof( sz ), "%.2f", righttime + ev->GetStartTime() );

		int textW = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

		timeRect.right = w2() - 10;
		timeRect.left = timeRect.right - textW;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );
	}

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );
	DrawScrubHandle( drawHelper, rcHandle, m_flScrub, false );

	DrawEventEnd( drawHelper );

	if ( ev && ev->GetDuration() > 0.0f )
	{
		float scrub = ev->GetOriginalPercentageFromPlaybackPercentage( m_flScrub / ev->GetDuration() ) * ev->GetDuration();
		GetScrubHandleReferenceRect( rcHandle, scrub, true );
		DrawScrubHandle( drawHelper, rcHandle, scrub, true );
	}

	RECT rcTags = rc;
	rcTags.top = TAG_TOP + GetCaptionHeight();
	rcTags.bottom = TAG_BOTTOM + GetCaptionHeight();

	DrawRelativeTags( drawHelper, rcTags );

	DrawAbsoluteTags( drawHelper );

	RECT rcPos;
	GetMouseOverPosRect( rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GestureTool::ShowContextMenu( mxEvent *event, bool include_track_menus )
{
	// Construct main menu
	mxPopupMenu *pop = new mxPopupMenu();

	int current, total;
	g_pChoreoView->GetUndoLevels( current, total );
	if ( total > 0 )
	{
		if ( current > 0 )
		{
			pop->add( va( "Undo %s", g_pChoreoView->GetUndoDescription() ), IDC_UNDO_GT );
		}
		
		if ( current <= total - 1 )
		{
			pop->add( va( "Redo %s", g_pChoreoView->GetRedoDescription() ), IDC_REDO_GT );
		}
		pop->addSeparator();
	}

	CEventAbsoluteTag *tag = IsMouseOverTag( (short)event->x, (short)event->y );
	if ( tag )
	{
		pop->add( va( "Delete '%s'...", tag->GetName() ), IDC_GT_DELETE_TAG );
	}
	else
	{
		pop->add( "Insert Tag...", IDC_GT_INSERT_TAG );
	}
	pop->add( "Revert Tag Timings", IDC_GT_REVERT );
	pop->add( va( "Change scale..." ), IDC_GT_CHANGESCALE );

	pop->popup( this, (short)event->x, (short)event->y );
}

void GestureTool::GetWorkspaceLeftRight( int& left, int& right )
{
	left = 0;
	right = w2();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GestureTool::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		RECT rc = m_FocusRects[ i ].m_rcFocus;

		::DrawFocusRect( dc, &rc );
	}

	ReleaseDC( NULL, dc );
}

void GestureTool::SetClickedPos( int x, int y )
{
	m_nClickedX = x;
	m_nClickedY = y;
}

float GestureTool::GetTimeForClickedPos( void )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return 0.0f;

	float t = GetTimeValueForMouse( m_nClickedX );
	return t;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dragtype - 
//			startx - 
//			cursor - 
//-----------------------------------------------------------------------------
void GestureTool::StartDragging( int dragtype, int startx, int starty, HCURSOR cursor )
{
	m_nDragType = dragtype;
	m_nStartX	= startx;
	m_nLastX	= startx;
	m_nStartY	= starty;
	m_nLastY	= starty;
	
	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = NULL;
	}
	m_hPrevCursor = SetCursor( cursor );

	m_FocusRects.Purge();

	RECT rcStart;
	rcStart.left = startx;
	rcStart.right = startx;

	bool addrect = true;
	switch ( dragtype )
	{
	default:
	case DRAGTYPE_SCRUBBER:
		{
			RECT rcScrub;
			GetScrubHandleRect( rcScrub, m_flScrub, true );

			rcStart = rcScrub;
			rcStart.left = ( rcScrub.left + rcScrub.right ) / 2;
			rcStart.right = rcStart.left;
			rcStart.top = rcScrub.bottom;

			rcStart.bottom = h2();
		}
		break;
	case DRAGTYPE_ABSOLUTE_TIMING_TAG:
		{
			rcStart.top = 0;
			rcStart.bottom = h2();
		}
		break;
	}


	if ( addrect )
	{
		AddFocusRect( rcStart );
	}
	
	DrawFocusRect();
}

void GestureTool::OnMouseMove( mxEvent *event )
{
	int mx = (short)event->x;
	int my = (short)event->y;

	event->x = (short)mx;

	if ( m_nDragType != DRAGTYPE_NONE )
	{
		DrawFocusRect();

		for ( int i = 0; i < m_FocusRects.Size(); i++ )
		{
			CFocusRect *f = &m_FocusRects[ i ];
			f->m_rcFocus = f->m_rcOrig;

			switch ( m_nDragType )
			{
			default:
			case DRAGTYPE_SCRUBBER:
				{
					ApplyBounds( mx, my );
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( mx );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
					}
				}
				break;
			case DRAGTYPE_ABSOLUTE_TIMING_TAG:
				{
					ApplyBounds( mx, my );
				}
				break;
			}

			OffsetRect( &f->m_rcFocus, ( mx - m_nStartX ),	0 );
		}

		DrawFocusRect();
	}
	else
	{
		if ( m_hPrevCursor )
		{
			SetCursor( m_hPrevCursor );
			m_hPrevCursor = NULL;
		}

		if ( IsMouseOverScrubHandle( event ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		else if ( IsMouseOverTag( mx, my ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
	}

	m_nLastX = (short)event->x;
	m_nLastY = (short)event->y;
}

int	GestureTool::handleEvent( mxEvent *event )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::Size:
		{
			int w, h;
			w = event->width;
			h = event->height;

			m_nLastHPixelsNeeded = 0;
			InvalidateLayout();
			iret = 1;
		}
		break;
	case mxEvent::MouseWheeled:
		{
			CChoreoScene *scene = g_pChoreoView->GetScene();
			if ( scene )
			{
				int tz = g_pChoreoView->GetTimeZoom( GetToolName() );
				bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;
				int stepMultipiler = shiftdown ? 5 : 1;

				// Zoom time in  / out
				if ( event->height > 0 )
				{
					tz = min( tz + TIME_ZOOM_STEP * stepMultipiler, MAX_TIME_ZOOM );
				}
				else
				{
					tz = max( tz - TIME_ZOOM_STEP * stepMultipiler, TIME_ZOOM_STEP );
				}

				g_pChoreoView->SetPreservedTimeZoom( this, tz );
			}
			RepositionHSlider();
			redraw();
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
			iret = 1;

			int mx = (short)event->x;
			int my = (short)event->y;

			SetClickedPos( mx, my );

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			if ( event->buttons & mxEvent::MouseRightButton )
			{
				ShowContextMenu( event, false );
				return iret;
			}
		
			if ( m_nDragType == DRAGTYPE_NONE )
			{
				if ( IsMouseOverScrubHandle( event ) )
				{
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						m_flScrubberTimeOffset = m_flScrub - t;
						float maxoffset = 0.5f * (float)SCRUBBER_HANDLE_WIDTH / GetPixelsPerSecond();
						m_flScrubberTimeOffset = clamp( m_flScrubberTimeOffset, -maxoffset, maxoffset );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
					}

					StartDragging( DRAGTYPE_SCRUBBER, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
				}
				else if ( IsMouseOverTag( mx, my ) )
				{
					StartDragging( DRAGTYPE_ABSOLUTE_TIMING_TAG, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
				}
				else
				{
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );

						SetScrubTargetTime( t );
					}
				}

				CalcBounds( m_nDragType );
			}
		}
		break;
	case mxEvent::MouseDrag:
	case mxEvent::MouseMove:
		{
			int mx = (short)event->x;
			int my = (short)event->y;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			OnMouseMove( event );

			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			if ( event->buttons & mxEvent::MouseRightButton )
			{
				return 1;
			}

			int mx = (short)event->x;
			int my = (short)event->y;

			if ( m_nDragType != DRAGTYPE_NONE )
			{
				DrawFocusRect();
			}

			if ( m_hPrevCursor )
			{
				SetCursor( m_hPrevCursor );
				m_hPrevCursor = 0;
			}

			switch ( m_nDragType )
			{
			case DRAGTYPE_NONE:
				break;
			case DRAGTYPE_SCRUBBER:
				{
					ApplyBounds( mx, my );

					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
						m_flScrubberTimeOffset = 0.0f;
					}
				}
				break;
			case DRAGTYPE_ABSOLUTE_TIMING_TAG:
				{
					ApplyBounds( mx, my );

					CEventAbsoluteTag *tag = IsMouseOverTag( m_nClickedX, m_nClickedY );
					if ( tag && w2() && GetSafeEvent() )
					{
						float t = GetTimeValueForMouse( mx );
						float lastfrac = t / GetSafeEvent()->GetDuration();
						lastfrac = clamp( lastfrac, 0.0f, 1.0f );

						g_pChoreoView->SetDirty( true );
						g_pChoreoView->PushUndo( "move absolute tag" );
						tag->SetPercentage( lastfrac );
						g_pChoreoView->PushRedo( "move absolute tag" );

						g_pChoreoView->InvalidateLayout();

						redraw();
					}
			
				}
				break;
			}

			m_nDragType = DRAGTYPE_NONE;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			iret = 1;
		}
		break;
	case mxEvent::KeyDown:
		{
			iret = g_pChoreoView->HandleZoomKey( this, event->key );
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
			case IDC_UNDO_GT:
				OnUndo();
				break;
			case IDC_REDO_GT:
				OnRedo();
				break;
			case IDC_GT_DELETE_TAG:
				OnDeleteTag();
				break;
			case IDC_GT_INSERT_TAG:
				OnInsertTag();
				break;
			case IDC_GT_REVERT:
				OnRevert();
				break;
			case IDC_GESTUREHSCROLL:
				{
					int offset = 0;
					bool processed = true;

					switch ( event->modifiers )
					{
					case SB_THUMBTRACK:
						offset = event->height;
						break;
					case SB_PAGEUP:
						offset = m_pHorzScrollBar->getValue();
						offset -= 20;
						offset = max( offset, m_pHorzScrollBar->getMinValue() );
						break;
					case SB_PAGEDOWN:
						offset = m_pHorzScrollBar->getValue();
						offset += 20;
						offset = min( offset, m_pHorzScrollBar->getMaxValue() );
						break;
					case SB_LINEUP:
						offset = m_pHorzScrollBar->getValue();
						offset -= 10;
						offset = max( offset, m_pHorzScrollBar->getMinValue() );
						break;
					case SB_LINEDOWN:
						offset = m_pHorzScrollBar->getValue();
						offset += 10;
						offset = min( offset, m_pHorzScrollBar->getMaxValue() );
						break;
					default:
						processed = false;
						break;
					}

					if ( processed )
					{
						MoveTimeSliderToPos( offset );
					}
				}
				break;
			case IDC_GT_CHANGESCALE:
				{
					OnChangeScale();
				}
				break;
			}
		}
		break;
	}
	return iret;
}

void GestureTool::ApplyBounds( int& mx, int& my )
{
	if ( !m_bUseBounds )
		return;

	mx = clamp( mx, m_nMinX, m_nMaxX );
}

int GestureTool::GetTagTypeForTag( CEventAbsoluteTag const *tag )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return -1;

	for ( int t = 0; t < CChoreoEvent::NUM_ABS_TAG_TYPES; t++ )
	{
		CChoreoEvent::AbsTagType tagtype = (CChoreoEvent::AbsTagType)t;

		for ( int i = 0; i < e->GetNumAbsoluteTags( tagtype ); i++ )
		{
			CEventAbsoluteTag *ptag = e->GetAbsoluteTag( tagtype, i );
			Assert( ptag );
			if ( ptag == tag )
				return t;
		}
	}

	return -1;
}

void GestureTool::CalcBounds( int movetype )
{
	switch ( movetype )
	{
	default:
	case DRAGTYPE_NONE:
		{
			m_bUseBounds = false;
			m_nMinX = 0;
			m_nMaxX = 0;
		}
		break;
	case DRAGTYPE_SCRUBBER:
		{
			m_bUseBounds = true;
			m_nMinX = 0;
			m_nMaxX = w2();
		}
		break;
	case DRAGTYPE_ABSOLUTE_TIMING_TAG:
		{
			m_bUseBounds = true;
			m_nMinX = 0;
			m_nMaxX = w2();

			CChoreoEvent *e = GetSafeEvent();
			CEventAbsoluteTag *tag = IsMouseOverTag( m_nClickedX, m_nClickedY );
			if ( tag && e && e->GetDuration() )
			{
				m_nMinX = GetPixelForTimeValue( 0 );
				m_nMaxX = max( w2(), GetPixelForTimeValue( e->GetDuration() ) );

				int t = GetTagTypeForTag( tag );
				if ( t != -1 )
				{
					CChoreoEvent::AbsTagType tagtype = (CChoreoEvent::AbsTagType)t;

					CEventAbsoluteTag *prevTag = NULL, *nextTag = NULL;
					int c = e->GetNumAbsoluteTags( tagtype );
					int i;
					for ( i = 0; i < c; i++ )
					{
						CEventAbsoluteTag *t = e->GetAbsoluteTag( tagtype, i );
						Assert( t );

						if ( t == tag )
						{
							prevTag = i > 0 ?  e->GetAbsoluteTag( tagtype, i-1 ) : NULL;
							nextTag = i < c - 1 ? e->GetAbsoluteTag( tagtype, i+1 ) : NULL;
							break;
						}
					}

					if ( i < c )
					{
						if ( prevTag )
						{
							m_nMinX = GetPixelForTimeValue( prevTag->GetPercentage() * e->GetDuration() ) + 1;
						}
						if ( nextTag )
						{
							m_nMaxX = GetPixelForTimeValue( nextTag->GetPercentage() * e->GetDuration() ) - 1;
						}
					}
					else
					{
						Assert( 0 );
					}
				}
			}
		}
		break;
	}
}

bool GestureTool::PaintBackground()
{
	redraw();
	return false;
}

void GestureTool::OnUndo( void )
{
	g_pChoreoView->Undo();
}

void GestureTool::OnRedo( void )
{
	g_pChoreoView->Redo();
}

void GestureTool::ForceScrubPositionFromSceneTime( float scenetime )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e || !e->GetDuration() )
		return;

	float t = scenetime - e->GetStartTime();
	m_flScrub = t;
	m_flScrubTarget = t;
	DrawScrubHandles();
}

void GestureTool::ForceScrubPosition( float t )
{
	m_flScrub = t;
	m_flScrubTarget = t;
	
	CChoreoEvent *e = GetSafeEvent();
	if ( e && e->GetDuration() )
	{
		float realtime = e->GetStartTime() + t;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->SetScrubTargetTime( realtime );

		g_pChoreoView->DrawScrubHandle();
	}

	DrawScrubHandles();
}

void GestureTool::SetMouseOverPos( int x, int y )
{
	m_nMousePos[ 0 ] = x;
	m_nMousePos[ 1 ] = y;
}

void GestureTool::GetMouseOverPos( int &x, int& y )
{
	x = m_nMousePos[ 0 ];
	y = m_nMousePos[ 1 ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcPos - 
//-----------------------------------------------------------------------------
void GestureTool::GetMouseOverPosRect( RECT& rcPos )
{
	rcPos.top = GetCaptionHeight() + 12;
	rcPos.left = w2() - 200;
	rcPos.right = w2() - 5;
	rcPos.bottom = rcPos.top + 13;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcPos - 
//-----------------------------------------------------------------------------
void GestureTool::DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos )
{
	// Compute time for pixel x
	float t = GetTimeValueForMouse( m_nMousePos[ 0 ] );
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	t += e->GetStartTime();
	float snapped = FacePoser_SnapTime( t );

	// Found it, write out description
	// 
	char sz[ 128 ];
	if ( t != snapped )
	{
		Q_snprintf( sz, sizeof( sz ), "%s", FacePoser_DescribeSnappedTime( t ) );
	}
	else
	{
		Q_snprintf( sz, sizeof( sz ), "%.3f", t );
	}

	int len = drawHelper.CalcTextWidth( "Arial", 11, 900, sz );

	RECT rcText = rcPos;
	rcText.left = max( rcPos.left, rcPos.right - len );

	drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 255, 50, 70 ), rcText, sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GestureTool::DrawMouseOverPos()
{
	RECT rcPos;
	GetMouseOverPosRect( rcPos );

	CChoreoWidgetDrawHelper drawHelper( this, rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

void GestureTool::AddFocusRect( RECT& rc )
{
	RECT rcFocus = rc;

	POINT offset;
	offset.x = 0;
	offset.y = 0;
	ClientToScreen( (HWND)getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	// Convert to screen space?
	CFocusRect fr;
	fr.m_rcFocus = rcFocus;
	fr.m_rcOrig = rcFocus;

	m_FocusRects.AddToTail( fr );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &rcClient - 
//			tagtype - 
//			rcTray - 
//-----------------------------------------------------------------------------
void GestureTool::GetTagTrayRect( RECT &rcClient, int tagtype, RECT& rcTray )
{
	rcTray = rcClient;

	rcTray.top += ( GetCaptionHeight() + 110 );

	rcTray.bottom	= rcTray.top + 6;

	if ( tagtype == CChoreoEvent::ORIGINAL )
	{
		OffsetRect( &rcTray, 0, 45 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcClient - 
//			*event - 
//			tagtype - 
//			*tag - 
//			rcTag - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool GestureTool::GetAbsTagRect( RECT& rcClient, CChoreoEvent *event, 
	int tagtype, CEventAbsoluteTag *tag, RECT& rcTag )
{
	rcTag = rcClient;

	GetTagTrayRect( rcClient, tagtype, rcTag );

	bool clipped = false;
	float t = tag->GetPercentage() * event->GetDuration();
	int tagx = GetPixelForTimeValue( t, &clipped );

	rcTag.left		= tagx - 3;
	rcTag.right		= tagx + 3;

	if ( clipped )
		return false;

	return true;
}

void GestureTool::DrawAbsoluteTags( CChoreoWidgetDrawHelper& drawHelper )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	RECT rcClient;
	drawHelper.GetClientRect( rcClient );

	bool showDots = true;
	if ( event->GetNumAbsoluteTags( (CChoreoEvent::AbsTagType)0 ) !=
		 event->GetNumAbsoluteTags( (CChoreoEvent::AbsTagType)1 ) )
	{
		showDots = false;
	}

	int t;
	for ( t = 0; t < CChoreoEvent::NUM_ABS_TAG_TYPES; t++ )
	{
		CChoreoEvent::AbsTagType tagtype = ( CChoreoEvent::AbsTagType )t;

		RECT rcTray;
		GetTagTrayRect( rcClient, tagtype, rcTray );

		drawHelper.DrawColoredLine( RGB( 220, 220, 220 ), PS_SOLID, 1, rcTray.left, rcTray.top, rcTray.right, rcTray.top );
		drawHelper.DrawColoredLine( RGB( 220, 220, 220 ), PS_SOLID, 1, rcTray.left, rcTray.bottom, rcTray.right, rcTray.bottom );

		RECT rcText;
		rcText = rcTray;

		InflateRect( &rcText, 0, 4 );
		OffsetRect( &rcText, 0, t == 0 ? -10 : 10 );

		rcText.left = 2;

		drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 150, 150, 150 ), rcText, "%s", 
			t == 0 ? "Playback Time" : "Original Time" );

		for ( int i = 0; i < event->GetNumAbsoluteTags( tagtype ); i++ )
		{
			CEventAbsoluteTag *tag = event->GetAbsoluteTag( tagtype, i );
			if ( !tag )
				continue;

			RECT rcMark;

			bool visible = GetAbsTagRect( rcClient, event, tagtype, tag, rcMark );

			if ( showDots && t == 1 )
			{
				CChoreoEvent::AbsTagType tagtypeOther = (CChoreoEvent::AbsTagType)0;

				RECT rcMark2;
				CEventAbsoluteTag *otherTag = event->GetAbsoluteTag( tagtypeOther, i );
				if ( otherTag )
				{
					GetAbsTagRect( rcClient, event, tagtypeOther, otherTag, rcMark2 );
					{
						int midx1 = ( rcMark.left + rcMark.right ) / 2;
						int midx2 = ( rcMark2.left + rcMark2.right ) / 2;

						int y1 = rcMark.top;
						int y2 = rcMark2.bottom;

						drawHelper.DrawColoredLine(
							RGB( 200, 200, 200 ), PS_SOLID, 1,
							midx1, y1, midx2, y2 );
					}
				}
			}

			if ( !visible )
				continue;

			drawHelper.DrawTriangleMarker( rcMark, RGB( 200, 0, 30 ), tagtype != CChoreoEvent::PLAYBACK );
			
			RECT rcText;
			rcText = rcMark;

			if ( tagtype == CChoreoEvent::PLAYBACK )
			{
				rcText.top -= 15;
			}
			else
			{
				rcText.top += 10;
			}
			
			char text[ 256 ];
			sprintf( text, "%s", tag->GetName() );

			int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, text );
			rcText.left = ( rcMark.left + rcMark.right ) / 2 - len / 2;
			rcText.right = rcText.left + len + 2;
			
			rcText.bottom = rcText.top + 10;
			
			drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 200, 100, 100 ), rcText, text );
			
			if ( tagtype == CChoreoEvent::PLAYBACK )
			{
				rcText.top -= 10;
			}
			else
			{
				rcText.top += 10;
			}
			
			// sprintf( text, "%.3f", tag->GetPercentage() * event->GetDuration() + event->GetStartTime() );
			sprintf( text, "%.3f", tag->GetPercentage() );

			len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, text );
			rcText.left = ( rcMark.left + rcMark.right ) / 2 - len / 2;
			rcText.right = rcText.left + len + 2;
			
			rcText.bottom = rcText.top + 10;
			
			drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 200, 100, 100 ), rcText, text );
		}	
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//			left - 
//			right - 
//-----------------------------------------------------------------------------
void GestureTool::DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right )
{
	RECT rcLabel;
	float granularity = 0.5f;

	drawHelper.DrawColoredLine( RGB( 150, 150, 200 ), PS_SOLID, 1, rc.left, rc.top + 2, rc.right, rc.top + 2 );

	float f = SnapTime( left, granularity );
	while ( f < right )
	{
		float frac = ( f - left ) / ( right - left );
		if ( frac >= 0.0f && frac <= 1.0f )
		{
			rcLabel.left = GetPixelForTimeValue( f );
			rcLabel.top = rc.top + 5;
			rcLabel.bottom = rcLabel.top + 10;

			if ( f != left )
			{
				drawHelper.DrawColoredLine( RGB( 220, 220, 240 ), PS_DOT,  1, 
					rcLabel.left, rc.top, rcLabel.left, h2() );
			}

			char sz[ 32 ];
			sprintf( sz, "%.2f", f );

			int textWidth = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

			rcLabel.right = rcLabel.left + textWidth;

			OffsetRect( &rcLabel, -textWidth / 2, 0 );

			RECT rcOut = rcLabel;
			if ( rcOut.left <= 0 )
			{
				OffsetRect( &rcOut, -rcOut.left + 2, 0 );
			}

			drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 50, 150 ), rcOut, sz );

		}
		f += granularity;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : CFlexTimingTag
//-----------------------------------------------------------------------------
CEventAbsoluteTag *GestureTool::IsMouseOverTag( int mx, int my )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return NULL;

	RECT rcClient;
	GetClientRect( (HWND)getHandle(), &rcClient );

	POINT pt;
	pt.x = mx;
	pt.y = my;

	for ( int t = 0; t < CChoreoEvent::NUM_ABS_TAG_TYPES; t++ )
	{
		CChoreoEvent::AbsTagType tagtype = ( CChoreoEvent::AbsTagType )t;

		for ( int i = 0; i < event->GetNumAbsoluteTags( tagtype ); i++ )
		{
			CEventAbsoluteTag *tag = event->GetAbsoluteTag( tagtype, i );
			if ( !tag )
				continue;

			if ( tag->GetLocked() )
				continue;

			RECT rcTag;

			if ( !GetAbsTagRect( rcClient, event, tagtype, tag, rcTag ) )
				continue;

			if ( !PtInRect( &rcTag, pt ) )
				continue;

			return tag;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : int
//-----------------------------------------------------------------------------
int GestureTool::GetTagTypeForMouse( int mx, int my )
{
	RECT rcClient;
	rcClient.left = 0;
	rcClient.right = w2();
	rcClient.top = 0;
	rcClient.bottom = h2();

	POINT pt;
	pt.x = mx;
	pt.y = my;

	for ( int t = 0; t < CChoreoEvent::NUM_ABS_TAG_TYPES; t++ )
	{
		RECT rcTray;
		GetTagTrayRect( rcClient, t, rcTray );

		if ( PtInRect( &rcTray, pt ) )
		{
			return t;
		}
	}
	return -1;
}

void GestureTool::OnInsertTag( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	if ( event->GetType() != CChoreoEvent::GESTURE )
	{
		Con_ErrorPrintf( "Absolute Tag:  Can only tag GESTURE events\n" );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Absolute Tag Name" );
	strcpy( params.m_szPrompt, "Name:" );

	strcpy( params.m_szInputText, "" );

	if ( !InputProperties( &params ) )
		return;

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		Con_ErrorPrintf( "Timing Tag Name:  No name entered!\n" );
		return;
	}
	
	// Convert click to frac
	float t = GetTimeValueForMouse( m_nClickedX ) / event->GetDuration();
	float tshifted = event->GetOriginalPercentageFromPlaybackPercentage( t );

	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Add Gesture Tag" );

	event->AddAbsoluteTag( CChoreoEvent::ORIGINAL, params.m_szInputText, tshifted );
	event->AddAbsoluteTag( CChoreoEvent::PLAYBACK, params.m_szInputText, t );

	g_pChoreoView->PushRedo( "Add Gesture Tag" );

	// Redraw this window
	redraw();
}

void GestureTool::OnRevert()
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	if ( !event->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ) )
		return;

	if ( event->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ) !=
		 event->GetNumAbsoluteTags( CChoreoEvent::ORIGINAL ) )
	{
		Assert( 0 );
		return;
	}

	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Revert Gesture Tags" );

	int c = event->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK );
	for ( int i = 0; i < c; i++ )
	{
		CEventAbsoluteTag *original = event->GetAbsoluteTag( CChoreoEvent::ORIGINAL, i );
		CEventAbsoluteTag *playback = event->GetAbsoluteTag( CChoreoEvent::PLAYBACK, i );

		playback->SetPercentage( original->GetPercentage() );
	}
		

	g_pChoreoView->PushRedo( "Revert Gesture Tags" );

	// Redraw this window
	redraw();
}

void GestureTool::OnDeleteTag( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	CEventAbsoluteTag *tag = IsMouseOverTag( m_nClickedX, m_nClickedY );
	if ( !tag )
		return;
	
	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Remove Gesture Tag" );

	char sz[ 512 ];
	Q_strncpy( sz, tag->GetName(), sizeof( sz ) );

	for ( int t = 0; t < CChoreoEvent::NUM_ABS_TAG_TYPES; t++ )
	{
		event->RemoveAbsoluteTag( (CChoreoEvent::AbsTagType)t, sz );
	}

	g_pChoreoView->PushRedo( "Remove Gesture Tags" );

	// Redraw this window
	redraw();
}

void GestureTool::DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc )
{
	CChoreoEvent *gesture = GetSafeEvent();
	if ( !gesture )
		return;

	CChoreoScene *scene = gesture->GetScene();
	if ( !scene )
		return;

	float starttime = GetTimeValueForMouse( 0 );
	float endtime = GetTimeValueForMouse( w2() );

	if ( endtime - starttime <= 0.0f )
		return;

	drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rc, "Timing Tags:" );

	// Loop through all events in scene

	int c = scene->GetNumEvents();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CChoreoEvent *e = scene->GetEvent( i );
		if ( !e )
			continue;

		if ( e->GetNumRelativeTags() <= 0 )
			continue;

		// See if time overlaps
		if ( !e->HasEndTime() )
			continue;

		if ( ( e->GetEndTime() - e->GetStartTime() ) < starttime )
			continue;

		if ( ( e->GetStartTime() - e->GetStartTime() ) > endtime )
			continue;

		DrawRelativeTagsForEvent( drawHelper, rc, gesture, e, starttime, endtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//-----------------------------------------------------------------------------
void GestureTool::DrawRelativeTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, CChoreoEvent *gesture, CChoreoEvent *event, float starttime, float endtime )
{
	if ( !event )
		return;

	//drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, PEColor( COLOR_PHONEME_TIMING_TAG ), rc, "Timing Tags:" );

	for ( int i = 0; i < event->GetNumRelativeTags(); i++ )
	{
		CEventRelativeTag *tag = event->GetRelativeTag( i );
		if ( !tag )
			continue;

		// 
		float tagtime = ( event->GetStartTime() + tag->GetPercentage() * event->GetDuration() ) - gesture->GetStartTime();
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		bool clipped = false;
		int left = GetPixelForTimeValue( tagtime, &clipped );
		if ( clipped )
			continue;

		//float frac = ( tagtime - starttime ) / ( endtime - starttime );

		//int left = rc.left + (int)( frac * ( float )( rc.right - rc.left ) + 0.5f );

		RECT rcMark;
		rcMark = rc;
		rcMark.top = rc.bottom - 8;
		rcMark.bottom = rc.bottom;
		rcMark.left = left - 4;
		rcMark.right = left + 4;

		drawHelper.DrawTriangleMarker( rcMark, RGB( 0, 100, 200 ) );

		RECT rcText;
		rcText = rc;
		rcText.bottom = rc.bottom - 10;
		rcText.top = rcText.bottom - 10;
	
		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
		rcText.left = left - len / 2;
		rcText.right = rcText.left + len + 2;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rcText, tag->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int GestureTool::ComputeHPixelsNeeded( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return 0;

	int pixels = 0;
	float maxtime = event->GetDuration();
	pixels = (int)( ( maxtime ) * GetPixelsPerSecond() ) + 10;

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GestureTool::RepositionHSlider( void )
{
	int pixelsneeded = ComputeHPixelsNeeded();

	if ( pixelsneeded <= w2() )
	{
		m_pHorzScrollBar->setVisible( false );
	}
	else
	{
		m_pHorzScrollBar->setVisible( true );
	}
	m_pHorzScrollBar->setBounds( 0, h2() - m_nScrollbarHeight, w2() - m_nScrollbarHeight, m_nScrollbarHeight );

	m_flLeftOffset = max( 0.f, m_flLeftOffset );
	m_flLeftOffset = min( (float)pixelsneeded, m_flLeftOffset );

	m_pHorzScrollBar->setRange( 0, pixelsneeded );
	m_pHorzScrollBar->setValue( (int)m_flLeftOffset );
	m_pHorzScrollBar->setPagesize( w2() );

	m_nLastHPixelsNeeded = pixelsneeded;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float GestureTool::GetPixelsPerSecond( void )
{
	return m_flPixelsPerSecond * (float)g_pChoreoView->GetTimeZoom( GetToolName() )/100.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//-----------------------------------------------------------------------------
void GestureTool::MoveTimeSliderToPos( int x )
{
	m_flLeftOffset = (float)x;
	m_pHorzScrollBar->setValue( (int)m_flLeftOffset );
	InvalidateRect( (HWND)m_pHorzScrollBar->getHandle(), NULL, TRUE );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GestureTool::InvalidateLayout( void )
{
	if ( m_bSuppressLayout )
		return;

	if ( ComputeHPixelsNeeded() != m_nLastHPixelsNeeded )
	{
		RepositionHSlider();
	}

	m_bLayoutIsValid = false;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : st - 
//			ed - 
//-----------------------------------------------------------------------------
void GestureTool::GetStartAndEndTime( float& st, float& ed )
{
	st = m_flLeftOffset / GetPixelsPerSecond();
	ed = st + (float)w2() / GetPixelsPerSecond();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : float
//-----------------------------------------------------------------------------
float GestureTool::GetEventEndTime()
{
	CChoreoEvent *ev = GetSafeEvent();
	if ( !ev )
		return 1.0f;

	return ev->GetDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			*clipped - 
// Output : int
//-----------------------------------------------------------------------------
int GestureTool::GetPixelForTimeValue( float time, bool *clipped /*=NULL*/ )
{
	if ( clipped )
	{
		*clipped = false;
	}

	float st, ed;
	GetStartAndEndTime( st, ed );

	float frac = ( time - st ) / ( ed - st );
	if ( frac < 0.0 || frac > 1.0 )
	{
		if ( clipped )
		{
			*clipped = true;
		}
	}

	int pixel = ( int )( frac * w2() );
	return pixel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			clip - 
// Output : float
//-----------------------------------------------------------------------------
float GestureTool::GetTimeValueForMouse( int mx, bool clip /*=false*/)
{
	float st, ed;
	GetStartAndEndTime( st, ed );

	if ( clip )
	{
		if ( mx < 0 )
		{
			return st;
		}
		if ( mx > w2() )
		{
			return ed;
		}
	}

	float frac = (float)( mx )  / (float)( w2() );
	return st + frac * ( ed - st );
}

void GestureTool::OnChangeScale( void )
{
	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
	{
		return;
	}

	// Zoom time in  / out
	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Change Zoom" );
	strcpy( params.m_szPrompt, "New scale (e.g., 2.5x):" );

	Q_snprintf( params.m_szInputText, sizeof( params.m_szInputText ), "%.2f", (float)g_pChoreoView->GetTimeZoom( GetToolName() ) / 100.0f );

	if ( !InputProperties( &params ) )
		return;

	g_pChoreoView->SetTimeZoom( GetToolName(), clamp( (int)( 100.0f * atof( params.m_szInputText ) ), 1, MAX_TIME_ZOOM ), false );

	m_nLastHPixelsNeeded = -1;
	m_flLeftOffset= 0.0f;
	InvalidateLayout();
	Con_Printf( "Zoom factor %i %%\n", g_pChoreoView->GetTimeZoom( GetToolName() ) );
}

void GestureTool::DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	float duration = e->GetDuration();
	if ( !duration )
		return;

	int leftx = GetPixelForTimeValue( duration );
	if ( leftx >= w2() )
		return;

	RECT rcClient;
	drawHelper.GetClientRect( rcClient );

	drawHelper.DrawColoredLine(
		COLOR_CHOREO_ENDTIME, PS_SOLID, 1,
		leftx, GetCaptionHeight() + 73, leftx, rcClient.bottom );

}

void GestureTool::OnModelChanged()
{
	redraw();
}