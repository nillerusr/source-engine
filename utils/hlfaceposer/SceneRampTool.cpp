//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdio.h>
#include "hlfaceposer.h"
#include "SceneRampTool.h"
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
#include "curveeditorhelpers.h"
#include "EdgeProperties.h"

SceneRampTool *g_pSceneRampTool = 0;

#define TRAY_HEIGHT 20
#define TRAY_ITEM_INSET 10

#define TAG_TOP ( TRAY_HEIGHT + 12 )
#define TAG_BOTTOM ( TAG_TOP + 20 )

#define MAX_TIME_ZOOM 1000
// 10% per step
#define TIME_ZOOM_STEP 2

SceneRampTool::SceneRampTool( mxWindow *parent )
: IFacePoserToolWindow( "SceneRampTool", "Scene Ramp" ), mxWindow( parent, 0, 0, 0, 0 )
{
	m_pHelper = new CCurveEditorHelper< SceneRampTool >( this );

	m_bSuppressLayout = false;

	SetAutoProcess( true );

	m_flScrub			= 0.0f;
	m_flScrubTarget		= 0.0f;
	m_nDragType			= DRAGTYPE_NONE;

	m_nClickedX			= 0;
	m_nClickedY			= 0;

	m_hPrevCursor		= 0;
	
	m_nStartX			= 0;
	m_nStartY			= 0;

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
	m_pHorzScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_SRT_RAMPHSCROLL, mxScrollbar::Horizontal );
	m_pHorzScrollBar->setVisible( false );

	m_flScrubberTimeOffset = 0.0f;

	m_nUndoSetup = 0;
}

SceneRampTool::~SceneRampTool( void )
{
	delete m_pHelper;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoScene *SceneRampTool::GetSafeScene( void )
{
	if ( !g_pChoreoView )
		return NULL;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return NULL;

	return scene;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcHandle - 
//-----------------------------------------------------------------------------
void SceneRampTool::GetScrubHandleRect( RECT& rcHandle, float scrub, bool clipped )
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcHandle - 
//-----------------------------------------------------------------------------
void SceneRampTool::DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle, float scrub, bool reference )
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

	CChoreoScene *scene = GetSafeScene();
	if ( scene )
	{
		float st, ed;
		st = 0.0f;
		ed = scene->FindStopTime();

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
bool SceneRampTool::IsMouseOverScrubHandle( mxEvent *event )
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
bool SceneRampTool::IsProcessing( void )
{
	if ( !GetSafeScene() )
		return false;

	if ( m_flScrub != m_flScrubTarget )
		return true;

	return false;
}

bool SceneRampTool::IsScrubbing( void ) const
{
	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	return scrubbing;
}

void SceneRampTool::SetScrubTime( float t )
{
	m_flScrub = t;
	CChoreoScene *scene = GetSafeScene();
	if ( scene && scene->FindStopTime() )
	{
		float realtime = m_flScrub;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->DrawScrubHandle();
	}
}

void SceneRampTool::SetScrubTargetTime( float t )
{
	m_flScrubTarget = t;
	CChoreoScene *scene = GetSafeScene();
	if ( scene && scene->FindStopTime() )
	{
		float realtime = m_flScrubTarget;

		g_pChoreoView->SetScrubTargetTime( realtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void SceneRampTool::Think( float dt )
{
	if ( !GetSafeScene() )
		return;

	bool scrubbing = IsScrubbing();
	ScrubThink( dt, scrubbing );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void SceneRampTool::ScrubThink( float dt, bool scrubbing )
{
	if ( !GetSafeScene() )
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

void SceneRampTool::DrawScrubHandles()
{
	RECT rcTray;

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );

	rcTray = rcHandle;
	rcTray.left = 0;
	rcTray.right = w2();

	CChoreoWidgetDrawHelper drawHelper( this, rcTray );
	DrawScrubHandle( drawHelper, rcHandle, m_flScrub, false );
}

void SceneRampTool::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper drawHelper( this );
	HandleToolRedraw( drawHelper );

	RECT rc;
	drawHelper.GetClientRect( rc );

	CChoreoScene *scene = GetSafeScene();
	if ( scene )
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
		RECT rcTextLine = rcText;

		drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 200, 0, 0 ), rcTextLine,
			"Scene:  %s",
			g_pChoreoView->GetChoreoFile() );

		RECT rcTimeLine;
		drawHelper.GetClientRect( rcTimeLine );
		rcTimeLine.left = 0;
		rcTimeLine.right = w2();
		rcTimeLine.top += ( GetCaptionHeight() + 50 );

		float lefttime = GetTimeValueForMouse( 0 );
		float righttime = GetTimeValueForMouse( w2() );

		DrawTimeLine( drawHelper, rcTimeLine, lefttime, righttime );

		OffsetRect( &rcText, 0, 28 );

		rcText.left = 5;

		RECT timeRect = rcText;

		timeRect.right = timeRect.left + 100;

		char sz[ 32 ];

		Q_snprintf( sz, sizeof( sz ), "%.2f", lefttime );

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );

		timeRect = rcText;

		Q_snprintf( sz, sizeof( sz ), "%.2f", righttime );

		int textW = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

		timeRect.right = w2() - 10;
		timeRect.left = timeRect.right - textW;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );
	}

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, m_flScrub, true );
	DrawScrubHandle( drawHelper, rcHandle, m_flScrub, false );

	RECT rcSamples;
	GetSampleTrayRect( rcSamples );
	DrawSamples( drawHelper, rcSamples );

	DrawSceneEnd( drawHelper );

	RECT rcTags = rc;
	rcTags.top = TAG_TOP + GetCaptionHeight();
	rcTags.bottom = TAG_BOTTOM + GetCaptionHeight();

	DrawTimingTags( drawHelper, rcTags );

	RECT rcPos;
	GetMouseOverPosRect( rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SceneRampTool::ShowContextMenu( mxEvent *event, bool include_track_menus )
{
	// Construct main menu
	mxPopupMenu *pop = new mxPopupMenu();

	int current, total;
	g_pChoreoView->GetUndoLevels( current, total );
	if ( total > 0 )
	{
		if ( current > 0 )
		{
			pop->add( va( "Undo %s", g_pChoreoView->GetUndoDescription() ), IDC_UNDO_SRT );
		}
		
		if ( current <= total - 1 )
		{
			pop->add( va( "Redo %s", g_pChoreoView->GetRedoDescription() ), IDC_REDO_SRT );
		}
		pop->addSeparator();
	}

	CChoreoScene *scene = GetSafeScene();
	if ( scene )
	{
		if ( CountSelected() > 0 )
		{
			pop->add( va( "Delete" ), IDC_SRT_DELETE );
			pop->add( "Deselect all", IDC_SRT_DESELECT );
		}
		pop->add( "Select all", IDC_SRT_SELECTALL );
	}

	pop->add( va( "Change scale..." ), IDC_SRT_CHANGESCALE );
	pop->addSeparator();
	pop->add( "Edge Properties...", IDC_SRT_EDGEPROPERTIES );

	pop->popup( this, (short)event->x, (short)event->y );
}

void SceneRampTool::GetWorkspaceLeftRight( int& left, int& right )
{
	left = 0;
	right = w2();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SceneRampTool::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		RECT rc = m_FocusRects[ i ].m_rcFocus;

		::DrawFocusRect( dc, &rc );
	}

	ReleaseDC( NULL, dc );
}

void SceneRampTool::SetClickedPos( int x, int y )
{
	m_nClickedX = x;
	m_nClickedY = y;
}

float SceneRampTool::GetTimeForClickedPos( void )
{
	CChoreoScene *scene = GetSafeScene();
	if ( !scene )
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
void SceneRampTool::StartDragging( int dragtype, int startx, int starty, HCURSOR cursor )
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
	default:
		{
			rcStart.top = starty;
			rcStart.bottom = starty;
		}
		break;
	}


	if ( addrect )
	{
		AddFocusRect( rcStart );
	}
	
	DrawFocusRect();
}

void SceneRampTool::OnMouseMove( mxEvent *event )
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
				{
					OffsetRect( &f->m_rcFocus, ( mx - m_nStartX ),	( my - m_nStartY ) );
				}
				break;
			case DRAGTYPE_SCRUBBER:
				{
					ApplyBounds( mx, my );
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( mx );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
					}

					OffsetRect( &f->m_rcFocus, ( mx - m_nStartX ),	0 );
				}
				break;
			case DRAGTYPE_MOVEPOINTS_TIME:
			case DRAGTYPE_MOVEPOINTS_VALUE:
				{
					int dx = mx - m_nLastX;
					int dy = my - m_nLastY;

					if ( !( event->modifiers & mxEvent::KeyCtrl ) )
					{
						// Zero out motion on other axis
						if ( m_nDragType == DRAGTYPE_MOVEPOINTS_VALUE )
						{
							dx = 0;
							mx = m_nLastX;
						}
						else
						{
							dy = 0;
							my = m_nLastY;
						}
					}
					else
					{
						SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
					}

					RECT rcSamples;
					GetSampleTrayRect( rcSamples );

					int height = rcSamples.bottom - rcSamples.top;
					Assert( height > 0 );

					float dfdx = (float)dx / GetPixelsPerSecond();
					float dfdy = (float)dy / (float)height;

					MoveSelectedSamples( dfdx, dfdy );

					// Update the scrubber
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( mx );
						ForceScrubPosition( t );
						g_pMatSysWindow->Frame();
					}

					OffsetRect( &f->m_rcFocus, dx, dy );
				}
				break;
			case DRAGTYPE_SELECTION:
				{
					RECT rcFocus;
					
					rcFocus.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
					rcFocus.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

					rcFocus.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
					rcFocus.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

					POINT offset;
					offset.x = 0;
					offset.y = 0;
					ClientToScreen( (HWND)getHandle(), &offset );
					OffsetRect( &rcFocus, offset.x, offset.y );

					f->m_rcFocus = rcFocus;
				}
				break;
			}
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
		/*
		else if ( IsMouseOverTag( mx, my ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		*/
		// See if anything is selected
		if ( CountSelected() <= 0 )
		{
			// Nothing selected
			// Draw auto highlight
			DrawAutoHighlight( event );
		}
	}

	m_nLastX = (short)event->x;
	m_nLastY = (short)event->y;
}

int	SceneRampTool::handleEvent( mxEvent *event )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	// Give helper a shot at the event
	if ( m_pHelper->HelperHandleEvent( event ) )
	{
		return 1;
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
			redraw();
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
			bool ctrldown = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;
			bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;

			bool rightbutton = ( event->buttons & mxEvent::MouseRightButton ) ? true : false;

			iret = 1;

			int mx = (short)event->x;
			int my = (short)event->y;

			SetClickedPos( mx, my );

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			POINT pt;
			pt.x = mx;
			pt.y = my;

			RECT rcSamples;
			GetSampleTrayRect( rcSamples );

			bool insamplearea = PtInRect( &rcSamples, pt ) ? true : false;

			if ( m_nDragType == DRAGTYPE_NONE )
			{
				bool ctrlDown = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;

				CExpressionSample *sample = GetSampleUnderMouse( event->x, event->y, ctrlDown ? FP_SRT_ADDSAMPLE_TOLERANCE : FP_SRT_SELECTION_TOLERANCE );

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
				else if ( insamplearea )
				{
					if ( sample )
					{
						if  ( shiftdown ) 
						{
							sample->selected = !sample->selected;
							redraw();
						}
						else if ( sample->selected )
						{
							PreDataChanged( "move scene ramp points" );

							StartDragging( 
								rightbutton ? DRAGTYPE_MOVEPOINTS_TIME : DRAGTYPE_MOVEPOINTS_VALUE, 
								m_nClickedX, m_nClickedY, 
								LoadCursor( NULL, rightbutton ? IDC_SIZEWE : IDC_SIZENS ) );
						}
						else
						{
							if  ( !shiftdown ) 
							{
								DeselectAll();
							}

							StartDragging( DRAGTYPE_SELECTION, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_ARROW ) );
						}
					}
					else if ( ctrldown )
					{
						CChoreoScene *s = GetSafeScene();
						if ( s )
						{
							// Add a sample point
							float t = GetTimeValueForMouse( mx );
							
							t = FacePoser_SnapTime( t );
							float value = 1.0f - (float)( (short)event->y - rcSamples.top ) / (float)( rcSamples.bottom - rcSamples.top );
							value = clamp( value, 0.0f, 1.0f );
							
							PreDataChanged( "Add scene ramp point" );

							s->AddSceneRamp( t, value, false );

							s->ResortSceneRamp();

							PostDataChanged( "Add scene ramp point" );
						}
					}
					else
					{
						if ( event->buttons & mxEvent::MouseRightButton )
						{
							ShowContextMenu( event, false );
							iret = 1;
							return iret;
						}
						else
						{
							if  ( !shiftdown ) 
							{
								DeselectAll();
							}

							StartDragging( DRAGTYPE_SELECTION, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_ARROW ) );
						}
					}
				}
				else
				{
					if ( event->buttons & mxEvent::MouseRightButton )
					{
						ShowContextMenu( event, false );
						iret = 1;
						return iret;
					}
					else
					{
						if ( w2() > 0 )
						{
							float t = GetTimeValueForMouse( (short)event->x );

							SetScrubTargetTime( t );
						}
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
			OnMouseMove( event );

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
			case DRAGTYPE_MOVEPOINTS_VALUE:
			case DRAGTYPE_MOVEPOINTS_TIME:
				{
					PostDataChanged( "move ramp points" );
				}
				break;
			case DRAGTYPE_SELECTION:
				{
					SelectPoints();
				}
				break;
			}

			m_nDragType = DRAGTYPE_NONE;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			redraw();

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
			case IDC_UNDO_SRT:
				{
					OnUndo();
				}
				break;
			case IDC_REDO_SRT:
				{
					OnRedo();
				}
				break;
			case IDC_SRT_DELETE:
				{
					Delete();
				}
				break;
			case IDC_SRT_DESELECT:
				{
					DeselectAll();
				}
				break;
			case IDC_SRT_SELECTALL:
				{
					SelectAll();
				}
				break;
			case IDC_SRT_RAMPHSCROLL:
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
			case IDC_SRT_CHANGESCALE:
				{
					OnChangeScale();
				}
				break;
			case IDC_SRT_EDGEPROPERTIES:
				{
					OnEdgeProperties();
				}
				break;
			}
		}
		break;
	case mxEvent::KeyDown:
		{
			iret = 1;
			switch ( event->key )
			{
			default:
				iret = g_pChoreoView->HandleZoomKey( this, event->key );
				break;
			case VK_ESCAPE:
				DeselectAll();
				break;
			case VK_DELETE:
				Delete();
				break;
			}
		}
	}
	return iret;
}

void SceneRampTool::ApplyBounds( int& mx, int& my )
{
	if ( !m_bUseBounds )
		return;

	mx = clamp( mx, m_nMinX, m_nMaxX );
}

void SceneRampTool::CalcBounds( int movetype )
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
	}
}

bool SceneRampTool::PaintBackground()
{
	redraw();
	return false;
}

void SceneRampTool::OnUndo( void )
{
	g_pChoreoView->Undo();
}

void SceneRampTool::OnRedo( void )
{
	g_pChoreoView->Redo();
}

void SceneRampTool::ForceScrubPositionFromSceneTime( float scenetime )
{
	CChoreoScene *s = GetSafeScene();
	if ( !s || !s->FindStopTime() )
		return;

	float t = scenetime;
	m_flScrub = t;
	m_flScrubTarget = t;
	DrawScrubHandles();
}

void SceneRampTool::ForceScrubPosition( float t )
{
	m_flScrub = t;
	m_flScrubTarget = t;
	
	CChoreoScene *s = GetSafeScene();
	if ( s && s->FindStopTime() )
	{
		float realtime = t;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->SetScrubTargetTime( realtime );

		g_pChoreoView->DrawScrubHandle();
	}

	DrawScrubHandles();
}

void SceneRampTool::SetMouseOverPos( int x, int y )
{
	m_nMousePos[ 0 ] = x;
	m_nMousePos[ 1 ] = y;
}

void SceneRampTool::GetMouseOverPos( int &x, int& y )
{
	x = m_nMousePos[ 0 ];
	y = m_nMousePos[ 1 ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcPos - 
//-----------------------------------------------------------------------------
void SceneRampTool::GetMouseOverPosRect( RECT& rcPos )
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
void SceneRampTool::DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos )
{
	// Compute time for pixel x
	float t = GetTimeValueForMouse( m_nMousePos[ 0 ] );
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

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
void SceneRampTool::DrawMouseOverPos()
{
	RECT rcPos;
	GetMouseOverPosRect( rcPos );

	CChoreoWidgetDrawHelper drawHelper( this, rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

void SceneRampTool::AddFocusRect( RECT& rc )
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
// Input  : drawHelper - 
//			rc - 
//			left - 
//			right - 
//-----------------------------------------------------------------------------
void SceneRampTool::DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right )
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

void SceneRampTool::DrawTimingTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc )
{
	CChoreoScene *scene = GetSafeScene();
	if ( !scene )
		return;

	float starttime = GetTimeValueForMouse( 0 );
	float endtime = GetTimeValueForMouse( w2() );

	if ( endtime - starttime <= 0.0f )
		return;

	RECT rcText = rc;
	rcText.bottom = rcText.top + 10;

	drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rcText, "Timing Tags:" );

	// Loop through all events in scene

	int c = scene->GetNumEvents();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CChoreoEvent *e = scene->GetEvent( i );
		if ( !e )
			continue;

		// See if time overlaps
		if ( !e->HasEndTime() )
			continue;

		if ( e->GetEndTime() < starttime )
			continue;

		if ( e->GetStartTime() > endtime )
			continue;

		if ( e->GetNumRelativeTags() > 0 )
		{
			DrawRelativeTagsForEvent( drawHelper, rc, e, starttime, endtime );
		}
		if ( e->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ) > 0 )
		{
			DrawAbsoluteTagsForEvent( drawHelper, rc, e, starttime, endtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			&rc - 
//-----------------------------------------------------------------------------
void SceneRampTool::DrawAbsoluteTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT &rc, CChoreoEvent *event, float starttime, float endtime )
{
	if ( !event )
		return;

	for ( int i = 0; i < event->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ); i++ )
	{
		CEventAbsoluteTag *tag = event->GetAbsoluteTag( CChoreoEvent::PLAYBACK, i );
		if ( !tag )
			continue;

		float tagtime = ( event->GetStartTime() + tag->GetPercentage() * event->GetDuration() );
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		bool clipped = false;
		int left = GetPixelForTimeValue( tagtime, &clipped );
		if ( clipped )
			continue;

		if ( event->GetType() == CChoreoEvent::GESTURE )
		{
			continue;
		}

		COLORREF clr = RGB( 0, 100, 250 );

		RECT rcMark;
		rcMark = rc;
		rcMark.top = rc.bottom - 8;
		rcMark.bottom = rc.bottom;
		rcMark.left = left - 4;
		rcMark.right = left + 4;

		drawHelper.DrawTriangleMarker( rcMark, clr );

		RECT rcText;
		rcText = rcMark;
		rcText.top -= 12;
		
		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
		rcText.left = left - len / 2;
		rcText.right = rcText.left + len + 2;

		rcText.bottom = rcText.top + 10;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, clr, rcText, tag->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//-----------------------------------------------------------------------------
void SceneRampTool::DrawRelativeTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, CChoreoEvent *event, float starttime, float endtime )
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
		float tagtime = ( event->GetStartTime() + tag->GetPercentage() * event->GetDuration() );
		if ( tagtime < starttime || tagtime > endtime )
			continue;

		bool clipped = false;
		int left = GetPixelForTimeValue( tagtime, &clipped );
		if ( clipped )
			continue;

		//float frac = ( tagtime - starttime ) / ( endtime - starttime );

		//int left = rc.left + (int)( frac * ( float )( rc.right - rc.left ) + 0.5f );

		COLORREF clr = RGB( 100, 100, 100 );

		RECT rcMark;
		rcMark = rc;
		rcMark.top = rc.bottom - 8;
		rcMark.bottom = rc.bottom;
		rcMark.left = left - 4;
		rcMark.right = left + 4;

		drawHelper.DrawTriangleMarker( rcMark, clr );

		RECT rcText;
		rcText = rc;
		rcText.bottom = rc.bottom - 10;
		rcText.top = rcText.bottom - 10;
	
		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
		rcText.left = left - len / 2;
		rcText.right = rcText.left + len + 2;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, clr, rcText, tag->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int SceneRampTool::ComputeHPixelsNeeded( void )
{
	CChoreoScene *scene = GetSafeScene();
	if ( !scene )
		return 0;

	int pixels = 0;
	float maxtime = scene->FindStopTime();
	pixels = (int)( ( maxtime ) * GetPixelsPerSecond() + 10 );

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SceneRampTool::RepositionHSlider( void )
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
float SceneRampTool::GetPixelsPerSecond( void )
{
	return m_flPixelsPerSecond * (float)g_pChoreoView->GetTimeZoom( GetToolName() )/100.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//-----------------------------------------------------------------------------
void SceneRampTool::MoveTimeSliderToPos( int x )
{
	m_flLeftOffset = (float)x;
	m_pHorzScrollBar->setValue( (int)m_flLeftOffset );
	InvalidateRect( (HWND)m_pHorzScrollBar->getHandle(), NULL, TRUE );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SceneRampTool::InvalidateLayout( void )
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
void SceneRampTool::GetStartAndEndTime( float& st, float& ed )
{
	st = m_flLeftOffset / GetPixelsPerSecond();
	ed = st + (float)w2() / GetPixelsPerSecond();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : float
//-----------------------------------------------------------------------------
float SceneRampTool::GetEventEndTime()
{
	CChoreoScene *scene = GetSafeScene();
	if ( !scene )
		return 1.0f;

	return scene->FindStopTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			*clipped - 
// Output : int
//-----------------------------------------------------------------------------
int SceneRampTool::GetPixelForTimeValue( float time, bool *clipped /*=NULL*/ )
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
float SceneRampTool::GetTimeValueForMouse( int mx, bool clip /*=false*/)
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

void SceneRampTool::OnChangeScale( void )
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

void SceneRampTool::DrawSceneEnd( CChoreoWidgetDrawHelper& drawHelper )
{
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	float duration = s->FindStopTime();
	if ( !duration )
		return;

	int leftx = GetPixelForTimeValue( duration );
	if ( leftx >= w2() )
		return;

	RECT rcSample;
	GetSampleTrayRect( rcSample );

	drawHelper.DrawColoredLine(
		COLOR_CHOREO_ENDTIME, PS_SOLID, 1,
		leftx, rcSample.top, leftx, rcSample.bottom );
}

void SceneRampTool::GetSampleTrayRect( RECT& rc )
{
	rc.left = 0;
	rc.right = w2();
	rc.top = GetCaptionHeight() + 65;

	rc.bottom = h2() - m_nScrollbarHeight-2;
}

void SceneRampTool::DrawSamplesSimple( CChoreoWidgetDrawHelper& drawHelper, CChoreoScene *scene, bool clearbackground, COLORREF sampleColor, RECT &rcSamples )
{
	if ( clearbackground )
	{
		drawHelper.DrawFilledRect( RGB( 230, 230, 215 ), rcSamples );
	}

	if ( !scene )
		return;

	float starttime = 0.0f;
	float endtime = scene->FindStopTime();

	COLORREF lineColor = sampleColor;

	int width = rcSamples.right  - rcSamples.left;
	if ( width <= 0.0f )
		return;

	int height = rcSamples.bottom - rcSamples.top;
	int bottom = rcSamples.bottom;

	float timestepperpixel = endtime / (float)width;

	float prev_value = scene->GetSceneRampIntensity( starttime );
	int prev_x = rcSamples.left;
	float prev_t = 0.0f;

	for ( float x = rcSamples.left; x < rcSamples.right; x+=3 )
	{
		float t = (float)( x - rcSamples.left ) * timestepperpixel;

		float value =  scene->GetSceneRampIntensity( starttime + t );

		// Draw segment
		drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
			prev_x, bottom - prev_value * height,
			x, bottom - value * height );

		prev_x = x;
		prev_t = t;
		prev_value = value;
	}
}

void SceneRampTool::DrawSamples( CChoreoWidgetDrawHelper& drawHelper, RECT &rcSamples )
{
	drawHelper.DrawFilledRect( RGB( 230, 230, 215 ), rcSamples );

	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	int rampCount = s->GetSceneRampCount();
	if ( !rampCount )
		return;

	float starttime;
	float endtime;

	GetStartAndEndTime( starttime, endtime );

	COLORREF lineColor = RGB( 0, 0, 255 );
	COLORREF dotColor = RGB( 0, 0, 255 );
	COLORREF dotColorSelected = RGB( 240, 80, 20 );
	COLORREF shadowColor = RGB( 150, 150, 250 );

	int height = rcSamples.bottom - rcSamples.top;
	int bottom = rcSamples.bottom;
	int top = rcSamples.top;

	float timestepperpixel = 1.0f / GetPixelsPerSecond();

	float stoptime = min( endtime, s->FindStopTime() );
	
	float prev_t = starttime;
	float prev_value = s->GetSceneRampIntensity( prev_t );

	for ( float t = starttime-timestepperpixel; t <= stoptime; t += timestepperpixel )
	{
		float value =  s->GetSceneRampIntensity( t );

		int prevx, x;

		bool clipped1, clipped2;
		x = GetPixelForTimeValue( t, &clipped1 );
		prevx = GetPixelForTimeValue( prev_t, &clipped2 );

		if ( !clipped1 && !clipped2 )
		{
			// Draw segment
			drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
				prevx, clamp( bottom - prev_value * height, top, bottom ),
				x, clamp( bottom - value * height, top, bottom ) );
		}

		prev_t = t;
		prev_value = value;

	}

	for ( int sample = 0; sample < rampCount; sample++ )
	{
		CExpressionSample *start = s->GetSceneRamp( sample );

		/*
		int pixel = (int)( ( start->time / event_time ) * width + 0.5f);
		int x = m_rcBounds.left + pixel;
		float roundedfrac = (float)pixel / (float)width;
		*/
		float value = start->value;
		bool clipped = false;
		int x = GetPixelForTimeValue( start->time, &clipped );
		if ( clipped )
			continue;
		int y = bottom - value * height;

		int dotsize = 6;
		int dotSizeSelected = 6;

		COLORREF clr = dotColor;
		COLORREF clrSelected = dotColorSelected;

		drawHelper.DrawCircle( 
			start->selected ? clrSelected : clr, 
			x, y, 
			start->selected ? dotSizeSelected : dotsize,
			true );

		if ( !start->selected )
			continue;

		if ( start->GetCurveType() == CURVE_DEFAULT )
			continue;

		// Draw curve type indicator...
		char sz[ 128 ];
		Q_snprintf( sz, sizeof( sz ), "%s", Interpolator_NameForCurveType( start->GetCurveType(), true ) );
		RECT rc;
		int fontSize = 9;
		rc.top = clamp( y + 5, rcSamples.top + 2, rcSamples.bottom - 2 - fontSize );
		rc.bottom = rc.top + fontSize + 1;
		rc.left = x - 75;
		rc.right = x + 175;
		drawHelper.DrawColoredText( "Arial", fontSize, 500, shadowColor, rc, sz );
	}
}

void SceneRampTool::DrawAutoHighlight( mxEvent *event )
{
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	CExpressionSample *hover = GetSampleUnderMouse( event->x, event->y, 0.0f );
	RECT rcSamples;
	GetSampleTrayRect( rcSamples );

	CChoreoWidgetDrawHelper drawHelper( this, rcSamples, true );

	RECT rcClient = rcSamples;

	COLORREF dotColor = RGB( 0, 0, 255 );
	COLORREF dotColorSelected = RGB( 240, 80, 20 );
	COLORREF clrHighlighted = RGB( 0, 200, 0 );

	int height = rcClient.bottom - rcClient.top;
	int bottom = rcClient.bottom;

	int dotsize = 6;
	int dotSizeSelected = 6;
	int dotSizeHighlighted = 6;

	COLORREF clr = dotColor;
	COLORREF clrSelected = dotColorSelected;
	COLORREF bgColor = RGB( 230, 230, 200 );

	// Fixme, could look at 1st derivative and do more sampling at high rate of change?
	// or near actual sample points!
	int sampleCount = s->GetSceneRampCount();
	for ( int sample = 0; sample < sampleCount; sample++ )
	{
		CExpressionSample *start = s->GetSceneRamp( sample );

		float value = start->value;
		bool clipped = false;
		int x = GetPixelForTimeValue( start->time, &clipped );
		if ( clipped )
			continue;
		int y = bottom - value * height;

		if ( hover == start )
		{
			drawHelper.DrawCircle( 
				bgColor, 
				x, y, 
				dotSizeHighlighted,
				true );

			drawHelper.DrawCircle( 
				clrHighlighted, 
				x, y, 
				dotSizeHighlighted,
				false );


		}
		else
		{
			drawHelper.DrawCircle( 
				start->selected ? clrSelected : clr, 
				x, y, 
				start->selected ? dotSizeSelected : dotsize,
				true );
		}
	}
}


int SceneRampTool::NumSamples()
{
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return 0;

	return s->GetSceneRampCount();
}

CExpressionSample *SceneRampTool::GetSample( int idx )
{
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return NULL;

	return s->GetSceneRamp( idx );
}

CExpressionSample *SceneRampTool::GetSampleUnderMouse( int mx, int my, float tolerance /*= FP_SRT_SELECTION_TOLERANCE*/ )
{
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return NULL;

	RECT rcSamples;
	GetSampleTrayRect( rcSamples );

	POINT pt;
	pt.x = mx;
	pt.y = my;

	if ( !PtInRect( &rcSamples, pt ) )
		return NULL;

	pt.y -= rcSamples.top;

	float closest_dist = 9999999.f;
	CExpressionSample *bestsample = NULL;

	int height = rcSamples.bottom - rcSamples.top;

	for ( int i = 0; i < s->GetSceneRampCount(); i++ )
	{
		CExpressionSample *sample = s->GetSceneRamp( i );
		Assert( sample );

		bool clipped = false;
		int px = GetPixelForTimeValue( sample->time, &clipped );		
		int py = height * ( 1.0f - sample->value ); 

		int dx = px - pt.x;
		int dy = py - pt.y;

		float dist = sqrt( (float)(dx * dx + dy * dy) );

		if ( dist < closest_dist )
		{
			bestsample = sample;
			closest_dist = dist;
		}

	}

	// Not close to any of them!!!
	if ( ( tolerance != 0.0f ) && 
		( closest_dist > tolerance ) )
		return NULL;

	return bestsample;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SceneRampTool::SelectPoints( void )
{
	RECT rcSelection;
	
	rcSelection.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
	rcSelection.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

	rcSelection.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
	rcSelection.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

	int selW = rcSelection.right - rcSelection.left;
	int selH = rcSelection.bottom - rcSelection.top;

	float tolerance = FP_SRT_SELECTION_RECTANGLE_TOLERANCE;
	// If they are just clicking and releasing in one spot, capture any items w/in a larger tolerance
	if ( selW <= 2 && selH <= 2 )
	{
		tolerance = FP_SRT_SELECTION_TOLERANCE;

		CExpressionSample *sample = GetSampleUnderMouse( rcSelection.left + selW * 0.5f, rcSelection.top + selH * 0.5f );
		if ( sample )
		{
			sample->selected = true;
			return;
		}
	}
	else
	{
		InflateRect( &rcSelection, 3, 3 );
	}

	RECT rcSamples;
	GetSampleTrayRect( rcSamples );

	int height = rcSamples.bottom - rcSamples.top;

	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	float duration = s->FindStopTime();
	if ( !duration )
		return;

	float fleft = (float)GetTimeValueForMouse( rcSelection.left );
	float fright = (float)GetTimeValueForMouse( rcSelection.right );

	//fleft *= duration;
	//fright *= duration;

	float ftop = (float)( rcSelection.top - rcSamples.top ) / (float)height;
	float fbottom = (float)( rcSelection.bottom - rcSamples.top ) / (float)height;

	fleft = clamp( fleft, 0.0f, duration );
	fright = clamp( fright, 0.0f, duration );
	ftop = clamp( ftop, 0.0f, 1.0f );
	fbottom = clamp( fbottom, 0.0f, 1.0f );

	float timestepperpixel = 1.0f / GetPixelsPerSecond();
	float yfracstepperpixel = 1.0f / (float)height;


	float epsx = tolerance*timestepperpixel;
	float epsy = tolerance*yfracstepperpixel;

	for ( int i = 0; i < s->GetSceneRampCount(); i++ )
	{
		CExpressionSample *sample = s->GetSceneRamp( i );
		
		if ( sample->time + epsx < fleft )
			continue;

		if ( sample->time - epsx > fright )
			continue;

		if ( (1.0f - sample->value ) + epsy < ftop )
			continue;

		if ( (1.0f - sample->value ) - epsy > fbottom )
			continue;

		sample->selected = true;
	}

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int SceneRampTool::CountSelected( void )
{
	return m_pHelper->CountSelected( false );
}

void SceneRampTool::MoveSelectedSamples( float dfdx, float dfdy )
{
	int selecteditems = CountSelected();
	if ( !selecteditems )
		return;

	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	int c = s->GetSceneRampCount();

	float duration = s->FindStopTime();
	//dfdx *= duration;

	for ( int i = 0; i < c; i++ )
	{
		CExpressionSample *sample = s->GetSceneRamp( i );
		if ( !sample || !sample->selected )
			continue;

		sample->time += dfdx;
		sample->time = clamp( sample->time, 0.0f, duration );

		sample->value -= dfdy;
		sample->value = clamp( sample->value, 0.0f, 1.0f );
	}
			
	s->ResortSceneRamp();
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SceneRampTool::DeselectAll( void )
{
	int i;

	int selecteditems = CountSelected();
	if ( !selecteditems )
		return;

	CChoreoScene *s = GetSafeScene();
	Assert( s );
	if ( !s )
		return;

	for ( i = s->GetSceneRampCount() - 1; i >= 0 ; i-- )
	{
		CExpressionSample *sample = s->GetSceneRamp( i );
		sample->selected = false;
	}
	
	redraw();
}

void SceneRampTool::SelectAll( void )
{
	int i;

	CChoreoScene *s = GetSafeScene();
	Assert( s );
	if ( !s )
		return;

	for ( i = s->GetSceneRampCount() - 1; i >= 0 ; i-- )
	{
		CExpressionSample *sample = s->GetSceneRamp( i );
		sample->selected = true;
	}
	
	redraw();
}

void SceneRampTool::Delete( void )
{
	int i;

	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	int selecteditems = CountSelected();
	if ( !selecteditems )
		return;

	PreDataChanged( "Delete scene ramp points" );

	for ( i = s->GetSceneRampCount() - 1; i >= 0 ; i-- )
	{
		CExpressionSample *sample = s->GetSceneRamp( i );
		if ( !sample->selected )
			continue;

		s->DeleteSceneRamp( i );
	}

	PostDataChanged( "Delete scene ramp points" );
}

void SceneRampTool::OnModelChanged()
{
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *undodescription - 
//-----------------------------------------------------------------------------
void SceneRampTool::PreDataChanged( char const *undodescription )
{
	if ( m_nUndoSetup  == 0 )
	{
		g_pChoreoView->SetDirty( true );
		g_pChoreoView->PushUndo( undodescription );
	}
	++m_nUndoSetup;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *redodescription - 
//-----------------------------------------------------------------------------
void SceneRampTool::PostDataChanged( char const *redodescription )
{
	--m_nUndoSetup;
	if ( m_nUndoSetup == 0 )
	{
		g_pChoreoView->PushRedo( redodescription );
		redraw();
	}
}

void SceneRampTool::SetMousePositionForEvent( mxEvent *event )
{
	POINT pt;
	GetCursorPos( &pt );
	ScreenToClient( (HWND)getHandle(), &pt );

	event->x = pt.x;
	event->y = pt.y;
}

void SceneRampTool::OnEdgeProperties()
{
	CChoreoScene *s = GetSafeScene();
	if ( !s )
		return;

	CEdgePropertiesParams params;
	Q_memset( &params, 0, sizeof( params ) );
	Q_strcpy( params.m_szDialogTitle, "Edge Properties" );

	params.SetFromCurve( s->GetSceneRamp() );

	if ( !EdgeProperties( &params ) )
	{
		return;
	}

	char *undotext = "Change Scene Ramp Edge Properties";
	
	PreDataChanged( undotext );

	// Apply changes.
	params.ApplyToCurve( s->GetSceneRamp() );

	PostDataChanged( undotext );
}

void SceneRampTool::GetWorkList( bool reflect, CUtlVector< SceneRampTool * >& list )
{
	NOTE_UNUSED( reflect );
	list.AddToTail( this );
}
