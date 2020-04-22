//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include <stdio.h>
#include "TimelineItem.h"
#include "choreowidgetdrawhelper.h"
#include "mathlib/mathlib.h"
#include "expressions.h"
#include "StudioModel.h"
#include "expclass.h"
#include "mathlib/mathlib.h"
#include "ExpressionTool.h"
#include "choreoevent.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "ChoreoView.h"
#include "ControlPanel.h"
#include "faceposer_models.h"
#include "MatSysWin.h"
#include "choreoviewcolors.h"
#include "ifaceposersound.h"
#include "curveeditorhelpers.h"

extern double realtime;

#define DOUBLE_CLICK_TIME 0.2

#define GROW_HANDLE_WIDTH	66
#define GROW_HANDLE_HEIGHT	8
#define GROW_HANDLE_INSETPIXELS 8

#define TIMELINEITEM_DEFAULT_HEIGHT 100

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TimelineItem::TimelineItem( mxWindow *workspace )
{
	m_pHelper = new CCurveEditorHelper< TimelineItem >( this );

	m_pWorkspace = workspace;

	m_nDragging = DRAGTYPE_NONE;
	m_nLastX = 0;
	m_nLastY = 0;
	m_nStartX = 0;
	m_nStartY = 0;

	SetExpressionInfo( NULL, 0 );

	m_nNumSelected = 0;

	m_nEditType = 0;

	SetCollapsed( false );
	SetActive( false );

	m_nUndoSetup = 0;
	m_rcBounds.top = 0;
	m_rcBounds.bottom = 0;
	m_rcBounds.left = 0;
	m_rcBounds.right = 0;
	m_bVisible = false;
	m_flLastClickTime = -1;

	m_nCurrentHeight = TIMELINEITEM_DEFAULT_HEIGHT;
}

TimelineItem::~TimelineItem( void )
{
	delete m_pHelper;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::ResetHeight()
{
	m_nCurrentHeight = TIMELINEITEM_DEFAULT_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
// Output : float
//-----------------------------------------------------------------------------
float TimelineItem::GetTimeForMouse( int mx, bool clip /*= false*/ )
{
	float start, end;
	g_pExpressionTool->GetStartAndEndTime( start, end );

	if ( clip )
	{
		if ( mx < m_rcBounds.left )
		{
			return start;
		}
		else if ( mx >= m_rcBounds.right )
		{
			return end;
		}
	}

	float frac = (float)( mx - m_rcBounds.left ) / (float)( m_rcBounds.right - m_rcBounds.left );
	float t = start + frac * ( end - start );
	return t;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : t - 
// Output : int
//-----------------------------------------------------------------------------
int TimelineItem::GetMouseForTime( float t, bool *clipped /*= NULL*/ )
{
	float start, end;
	g_pExpressionTool->GetStartAndEndTime( start, end );

	float frac = ( t - start ) / ( end - start );

	if ( frac < 0.0 || frac > 1.0 )
	{
		if ( clipped )
		{
			*clipped = true;
		}
	}

	int mx = m_rcBounds.left + ( int ) ( frac * (float)( m_rcBounds.right - m_rcBounds.left ) );

	return mx;
}

int TimelineItem::NumSamples()
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return 0;

	// Aggregate both types of tracks together
	return track->GetNumSamples( 0 ) + track->GetNumSamples( 1 );
	// return track->GetNumSamples( m_nEditType );
}

CExpressionSample *TimelineItem::GetSample( int idx )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return NULL;

	if ( idx >= track->GetNumSamples( 0 ) )
	{
		// Rebase and look at left/right track instead
		idx -= track->GetNumSamples( 0 );
		return track->GetSample( idx, 1 );
	}
	return track->GetSample( idx, 0 );
}

CExpressionSample *TimelineItem::GetSampleUnderMouse( int mx, int my, float tolerance /*= FP_TL_SELECTION_TOLERANCE*/ )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return NULL;

	CChoreoEvent *e = track->GetEvent();
	if ( !e )
		return NULL;

	float closest_dist = 9999999.f;
	CExpressionSample *bestsample = NULL;

	// Add a sample point
	int height = m_rcBounds.bottom - m_rcBounds.top;

	mx += m_rcBounds.left;

	for ( int i = 0; i < track->GetNumSamples( m_nEditType ); i++ )
	{
		CExpressionSample *sample = track->GetSample( i, m_nEditType );

		bool clipped = false;
		int px = GetMouseForTime( sample->time, &clipped );		
		int py = height * ( 1.0f - sample->value ); 

		int dx = px - mx;
		int dy = py - my;

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
void TimelineItem::DeselectAll( void )
{
	g_pExpressionTool->DeselectAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::SelectAll( void )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	for ( int t = 0; t < 2; t++ )
	{
		for ( int i = 0; i <  track->GetNumSamples( t ); i++ )
		{
			CExpressionSample *sample = track->GetSample( i, t );
			sample->selected = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::Delete( void )
{
	g_pExpressionTool->DeleteSelectedSamples();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sample - 
//-----------------------------------------------------------------------------
void TimelineItem::AddSample( CExpressionSample const& sample )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	PreDataChanged( "Add sample point" );

	track->AddSample( sample.time, sample.value, m_nEditType );
	track->Resort( m_nEditType );

	SetActive( true );

	PostDataChanged( "Add sample point" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int TimelineItem::CountSelected( void )
{
	m_nNumSelected = m_pHelper->CountSelected( false );
	return m_nNumSelected;
}

void TimelineItem::SetMousePositionForEvent( mxEvent *event )
{
	POINT pt;
	GetCursorPos( &pt );
	ScreenToClient( (HWND)m_pWorkspace->getHandle(), &pt );

	pt.x -= m_rcBounds.left;
	pt.y -= m_rcBounds.top;

	event->x = pt.x;
	event->y = pt.y;
}

int	TimelineItem::handleEvent( mxEvent *event )
{
	int iret = 0;

	// Give helper a shot at the event
	if ( m_pHelper->HelperHandleEvent( event ) )
	{
		return 1;
	}

	switch ( event->event )
	{
	case mxEvent::KeyDown:
		{
			switch ( event->key )
			{
			default:
				iret = g_pChoreoView->HandleZoomKey( g_pExpressionTool, event->key );
				break;
			case VK_ESCAPE:
				DeselectAll();
				DrawSelf();
				break;
			case VK_DELETE:
				Delete();
				DrawSelf();
				break;
			case 'C':
				Copy();
				DrawSelf();
				break;
			case 'V':
				Paste();
				DrawSelf();
				break;
			case 'J':
				{
					g_pExpressionTool->OnCopyToFlex( g_pExpressionTool->GetScrubberSceneTime(), true );
				}
				break;
			case 'K':
				{
					g_pExpressionTool->OnCopyFromFlex( g_pExpressionTool->GetScrubberSceneTime(), false );
				}
				break;
			case 188: // VK_OEM_COMMA:
				{
					g_pExpressionTool->SetScrubTargetTime( 0.0f );
				}
				break;
			case 190: // VK_OEM_PERIOD:
				{
					CChoreoScene *scene = g_pChoreoView->GetScene();
					if ( scene )
					{
						g_pExpressionTool->SetScrubTargetTime( scene->FindStopTime() );
					}
				}
				break;
			case VK_LEFT: 
				{
					CChoreoScene *scene = g_pChoreoView->GetScene();
					if ( scene && scene->GetSceneFPS() > 0 )
					{
						float curscrub = g_pExpressionTool->GetScrub();
						curscrub -= ( 1.0f / (float)scene->GetSceneFPS() );
						curscrub = max( curscrub, 0.0f );
						g_pExpressionTool->SetScrubTargetTime( curscrub );
					}
				}
				break;
			case VK_RIGHT: 
				{
					CChoreoScene *scene = g_pChoreoView->GetScene();
					if ( scene && scene->GetSceneFPS() > 0 )
					{
						float curscrub = g_pExpressionTool->GetScrub();
						curscrub += ( 1.0f / (float)scene->GetSceneFPS() );
						curscrub = min( curscrub, scene->FindStopTime() );
						g_pExpressionTool->SetScrubTargetTime( curscrub );
					}
				}
				break;
			case 191:
				{
					if ( g_pChoreoView->IsPlayingScene() )
					{
						g_pChoreoView->StopScene();
					}
				}
				break;
			}
			iret = 1;
		}
		break;
	case mxEvent::KeyUp:
		{
			switch ( event->key )
			{
			case VK_SPACE:
				{
					CFlexAnimationTrack *track = GetSafeTrack();
					if ( track && track->IsComboType() )
					{
						SetEditType( m_nEditType == 0 ? 1 : 0 );
						DrawSelf();
					}
				}
				break;
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
			sound->Flush();

			SetFocus( (HWND)g_pExpressionTool->getHandle() );

			int height = m_rcBounds.bottom - m_rcBounds.top;

			bool rightbutton = ( event->buttons & mxEvent::MouseRightButton ) ? true : false;

			if ( m_nDragging == DRAGTYPE_NONE )
			{
				bool ctrlDown = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;

				CExpressionSample *sample = GetSampleUnderMouse( event->x, event->y, ctrlDown ? FP_TL_ADDSAMPLE_TOLERANCE : FP_TL_SELECTION_TOLERANCE );

				if ( IsMouseOverGrowHandle( (short)event->x, (short)event->y ) )
				{
					m_nDragging = DRAGTYPE_GROW;
					m_nLastX = (short)event->x;
					m_nLastY = (short)event->y;

					m_nStartX = m_nLastX;
					m_nStartY = m_nLastY;

					MouseDrag( (short)event->x, (short)event->y, event->modifiers );

					DrawGrowRect();
				}
				else if ( sample )
				{
					if  ( event->modifiers & mxEvent::KeyShift ) 
					{
						sample->selected = !sample->selected;
						DrawSelf();
					}
					else if ( sample->selected )
					{
						m_nDragging = rightbutton ? DRAGTYPE_MOVEPOINTS_TIME : DRAGTYPE_MOVEPOINTS_VALUE;
						m_nLastX = (short)event->x;
						m_nLastY = (short)event->y;

						m_nStartX = m_nLastX;
						m_nStartY = m_nLastY;

						PreDataChanged( "Move sample point(s)" );

						MouseDrag( (short)event->x, (short)event->y, event->modifiers );

						DrawSelf();
					}
					else
					{
						if  ( !( event->modifiers & mxEvent::KeyShift ) ) 
						{
							DeselectAll();
							DrawSelf();
						}

						m_nDragging = DRAGTYPE_SELECTION;
						m_nLastX = (short)event->x;
						m_nLastY = (short)event->y;

						m_nStartX = m_nLastX;
						m_nStartY = m_nLastY;

						MouseDrag( (short)event->x, (short)event->y, event->modifiers );

						DrawFocusRect();
					}
				}
				else if ( event->modifiers & mxEvent::KeyCtrl )
				{
					CChoreoEvent *e = g_pExpressionTool->GetSafeEvent();
					if ( e )
					{
						// Add a sample point
						float t = GetTimeForMouse( (short)event->x + m_rcBounds.left );

						CExpressionSample sample;
						sample.time = FacePoser_SnapTime( t );
						sample.value = 1.0f - (float)( (short)( event->y ) ) / (float)height;
						sample.selected = false;

						AddSample( sample );

						DrawSelf();
					}
				}
				else
				{
					if ( rightbutton )
					{
						POINT pt;
						pt.x = event->x;
						pt.y = event->y;
						ClientToScreen( (HWND)m_pWorkspace->getHandle(), &pt );
						ScreenToClient( (HWND)g_pExpressionTool->getHandle(), &pt );
						event->x = pt.x;
						event->y = pt.y;
						g_pExpressionTool->ShowContextMenu( event, true );
						return iret;
					}

					if  ( !( event->modifiers & mxEvent::KeyShift ) ) 
					{
						DeselectAll();
						DrawSelf();
					}

					m_nDragging = DRAGTYPE_SELECTION;
					m_nLastX = (short)event->x;
					m_nLastY = (short)event->y;

					m_nStartX = m_nLastX;
					m_nStartY = m_nLastY;

					MouseDrag( (short)event->x, (short)event->y, event->modifiers );

					DrawFocusRect();
				}
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseDrag:
	case mxEvent::MouseMove:
		{
			if ( m_nDragging != DRAGTYPE_NONE )
			{
				if ( m_nDragging == DRAGTYPE_SELECTION )
				{
					DrawFocusRect();
				}
				else if ( m_nDragging == DRAGTYPE_GROW )
				{
					DrawGrowRect();
				}

				MouseDrag( (short)event->x, (short)event->y, event->modifiers );
				
				if ( m_nDragging == DRAGTYPE_SELECTION )
				{
					DrawFocusRect();
				}
				else if ( m_nDragging == DRAGTYPE_GROW )
				{
					DrawGrowRect();
				}

				if ( m_nDragging == DRAGTYPE_MOVEPOINTS_TIME ||
					 m_nDragging == DRAGTYPE_MOVEPOINTS_VALUE )
				{
					DrawSelf();
				}
			}
			else
			{
				// See if anything is selected
				CountSelected();
				if ( m_nNumSelected <= 0 && g_pExpressionTool->IsFocusItem( this ) )
				{
					// Nothing selected
					// Draw auto highlight
					DrawAutoHighlight( event );
				}
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			bool overgrow = IsMouseOverGrowHandle( (short)event->x, (short)event->y );
				
			if ( m_nDragging != DRAGTYPE_NONE )
			{
				if ( m_nDragging == DRAGTYPE_SELECTION )
				{
					DrawFocusRect();
				}
				else if ( m_nDragging == DRAGTYPE_GROW )
				{
					DrawGrowRect();
				}

				MouseDrag( (short)event->x, (short)event->y, event->modifiers, true );

				if ( m_nDragging == DRAGTYPE_GROW )
				{
					// Finish grow by resizing control
					int desiredheight = m_nCurrentHeight + event->y - m_nStartY;
					if ( desiredheight >= 10 )
					{
						m_nCurrentHeight = desiredheight;
						g_pExpressionTool->LayoutItems( true );
					}
				}
				else if ( m_nDragging != DRAGTYPE_MOVEPOINTS_VALUE &&
					 m_nDragging != DRAGTYPE_MOVEPOINTS_TIME )
				{
					SelectPoints();
				}
				else
				{
					PostDataChanged( "Move sample point(s)" );
				}

				m_nDragging = DRAGTYPE_NONE;

				DrawSelf();
			}
			
			bool rightbutton = ( event->buttons & mxEvent::MouseRightButton ) ? true : false;
			bool shift = ( event->modifiers & mxEvent::KeyShift ) ? true : false;
			bool ctrl = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;

			if ( !rightbutton && !shift && !ctrl )
			{
				if ( realtime - m_flLastClickTime < DOUBLE_CLICK_TIME )
				{
					if ( overgrow || IsCollapsed() )
					{
						OnDoubleClicked();
					}
				}

				m_flLastClickTime = realtime;
			}

			iret = 1;
		}
		break;
	}

	return iret;
}

void TimelineItem::MouseDrag( int x, int y, int modifiers, bool snap /*=false*/ )
{
	if ( m_nDragging == DRAGTYPE_NONE )
		return;

	int width = m_rcBounds.right - m_rcBounds.left;
	int height = m_rcBounds.bottom - m_rcBounds.top;

	if ( m_nDragging == DRAGTYPE_MOVEPOINTS_TIME ||
		 m_nDragging == DRAGTYPE_MOVEPOINTS_VALUE )
	{
		int dx = x - m_nLastX;
		int dy = y - m_nLastY;

		if ( !( modifiers & mxEvent::KeyCtrl ) )
		{
			// Zero out motion on other axis
			if ( m_nDragging == DRAGTYPE_MOVEPOINTS_VALUE )
			{
				dx = 0;
				x = m_nLastX;
			}
			else
			{
				dy = 0;
				y = m_nLastY;
			}
		}

		float dfdx = (float)dx / g_pExpressionTool->GetPixelsPerSecond();
		float dfdy = (float)dy / (float)height;

		g_pExpressionTool->MoveSelectedSamples( dfdx, dfdy, snap );

		// Update the scrubber
		if ( (float)width > 0 )
		{
			float t = GetTimeForMouse( x +  m_rcBounds.left );
			g_pExpressionTool->ForceScrubPosition( t );

			g_pMatSysWindow->Frame();
		}
	}

	m_nLastX = x;
	m_nLastY = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::DrawFocusRect( void )
{
	RECT rcFocus;
	
	rcFocus.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
	rcFocus.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

	rcFocus.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
	rcFocus.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

	POINT offset;
	offset.x = m_rcBounds.left;
	offset.y = m_rcBounds.top;
	ClientToScreen( (HWND)m_pWorkspace->getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	HDC dc = GetDC( NULL );

	::DrawFocusRect( dc, &rcFocus );

	ReleaseDC( NULL, dc );
}

void TimelineItem::DrawSelf( void )
{
	CChoreoWidgetDrawHelper drawHelper( m_pWorkspace, m_rcBounds );
	Draw( drawHelper );
}

void TimelineItem::Draw( CChoreoWidgetDrawHelper& drawHelper )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	CChoreoEvent *e = track->GetEvent();
	if ( !e )
		return;

	Assert( e->HasEndTime() );

	bool active = track && ( IsValid() || IsActive() );

	float starttime;
	float endtime;

	g_pExpressionTool->GetStartAndEndTime( starttime, endtime );

	CountSelected();
	int scount = GetNumSelected();

	COLORREF bgColor = RGB( 230, 230, 200 );
	if ( IsCollapsed() && active )
	{
		bgColor = RGB( 200, 230, 200 );
	}

	RECT rcClient = m_rcBounds;

	drawHelper.DrawFilledRect( bgColor, rcClient );

	COLORREF gray = RGB( 200, 200, 200 );

	DrawEventEnd( drawHelper );

	DrawRelativeTags( drawHelper );
	if ( !IsCollapsed() && track )
	{
		if ( m_nEditType == 1 )
		{
			float zero = track->GetZeroValue( m_nEditType, true );

			drawHelper.DrawColoredLine( RGB( 180, 200, 220 ), PS_SOLID, 1, 
				rcClient.left, ( rcClient.top * zero + rcClient.bottom * (1 - zero)) ,
				rcClient.right, ( rcClient.top * zero + rcClient.bottom  * (1 - zero)) );
		}

		drawHelper.DrawOutlinedRect( RGB( 100, 150, 200 ), PS_SOLID, 1, rcClient );

		// Draw grow handle into background...
		if ( CanHaveGrowHandle() )
		{
			RECT handleRect;
			GetGrowHandleRect( handleRect );
			DrawGrowHandle( drawHelper, handleRect );
		}

		// Draw left/right underneath amount so go backbard
		for ( int type = ( track->IsComboType() ? 1 : 0 ); type >= 0; type-- )
		{
			COLORREF lineColor = ( type == m_nEditType ) ? RGB( 0, 0, 255 ) : gray;
			COLORREF shadowColor = ( type == m_nEditType ) ? RGB( 150, 150, 250 ) : gray;
			COLORREF dotColor = ( type == m_nEditType ) ? RGB( 0, 0, 255 ) : gray;
			COLORREF dotColorSelected = ( type == m_nEditType ) ? RGB( 240, 80, 20 ) : gray;

			int height = rcClient.bottom - rcClient.top;
			int bottom = rcClient.bottom;

			// Fixme, could look at 1st derivative and do more sampling at high rate of change?
			// or near actual sample points!
			float linelength = g_pExpressionTool->IsFocusItem( this ) ? 2.0f : 8.0f;

			float timestepperpixel = linelength / g_pExpressionTool->GetPixelsPerSecond();

			float stoptime = min( endtime, e->GetDuration() );
			
			float prev_t = starttime;
			float prev_value = track->GetFracIntensity( prev_t, type );

			CUtlVector< POINT > segments;

			/*
			if (type == m_nEditType)
			{
				// draw hermite version of time step
				float i0, i1, i2;
				float time10hz = starttime;

				i0 = track->GetFracIntensity( time10hz, type );
				i1 = i0;
				time10hz = starttime + 0.1;
				i2 = track->GetFracIntensity( time10hz, type );;
				
				for ( float t = starttime; t <= stoptime; t += timestepperpixel )
				{
					while (t >= time10hz)
					{
						time10hz += 0.1;
						i0 = i1;
						i1 = i2;
						i2 = track->GetFracIntensity( time10hz, type );;
					}

					float value = Hermite_Spline( i0, i1, i2, (t - time10hz + 0.1) / 0.1 );

					int prevx, x;

					bool clipped1, clipped2;
					x = GetMouseForTime( t, &clipped1 );
					prevx = GetMouseForTime( prev_t, &clipped2 );

					//if ( !clipped1 && !clipped2 )
					{
						// Draw segment
						//drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
						//	prevx, bottom - prev_value * height,
						//	x, bottom - value * height );
						
						POINT pt;

						if ( segments.Count() == 0 )
						{
							pt.x = prevx;	
							pt.y = bottom - prev_value * height;

							segments.AddToTail( pt );
						}

						pt.x = x;
						pt.y = bottom - value * height;

						segments.AddToTail( pt );
					}

					prev_t = t;
					prev_value = value;
				}

				if ( segments.Count() >= 2  )
				{
					drawHelper.DrawColoredPolyLine( shadowColor, PS_SOLID, 1, segments );
				}

				segments.RemoveAll();
			}
			*/
			for ( float t = starttime; t <= stoptime; t += timestepperpixel )
			{
				float value = track->GetFracIntensity( t, type );

				int prevx, x;

				bool clipped1, clipped2;
				x = GetMouseForTime( t, &clipped1 );
				prevx = GetMouseForTime( prev_t, &clipped2 );

				//if ( !clipped1 && !clipped2 )
				{
					// Draw segment
					//drawHelper.DrawColoredLine( lineColor, PS_SOLID, 1,
					//	prevx, bottom - prev_value * height,
					//	x, bottom - value * height );
					
					POINT pt;

					if ( segments.Count() == 0 )
					{
						pt.x = prevx;	
						pt.y = bottom - prev_value * height;

						segments.AddToTail( pt );
					}

					pt.x = x;
					pt.y = bottom - value * height;

					segments.AddToTail( pt );
				}

				prev_t = t;
				prev_value = value;
			}

			if ( segments.Count() >= 2  )
			{
				drawHelper.DrawColoredPolyLine( lineColor, PS_SOLID, 1, segments );
			}

			for ( int sample = 0; sample < track->GetNumSamples( type ); sample++ )
			{
				bool dummy;
				CExpressionSample *start = track->GetBoundedSample( sample, dummy, type );

				/*
				int pixel = (int)( ( start->time / event_time ) * width + 0.5f);
				int x = m_rcBounds.left + pixel;
				float roundedfrac = (float)pixel / (float)width;
				*/
				float value = start->value; // track->GetFracIntensity( start->time, type );
				bool clipped = false;
				int x = GetMouseForTime( start->time, &clipped );
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
				rc.top = clamp( y + 5, rcClient.top + 2, rcClient.bottom - 2 - fontSize );
				rc.bottom = rc.top + fontSize + 1;
				rc.left = x - 75;
				rc.right = x + 175;
				drawHelper.DrawColoredText( "Arial", fontSize, 500, shadowColor, rc, sz );
			}
		}
	}

	if ( track && track->IsComboType() && !IsCollapsed() )
	{
		RECT title = rcClient;
		title.left += 10;
		title.top += 14;
		title.bottom = title.top + 9;

		char sz[ 128 ];

		if ( m_nEditType == 1 )
		{
			sprintf( sz, "left" );

			drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 0, 0, 255 ), title, sz );

			sprintf( sz, "right" );

			title.top = rcClient.bottom - 22;
			title.bottom = rcClient.bottom;

			drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 0, 0, 255 ), title, sz );
		}

		int mid = ( rcClient.top + rcClient.bottom ) / 2;

		title.top = mid - 10;
		title.bottom = mid;

		sprintf( sz, "editmode:  <%s>", m_nEditType == 0 ? "amount" : "left/right" );

		drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 0, 0, 255 ), title, sz );
	}

	if ( track )
	{
		RECT title = rcClient;
		title.left += 2;
		title.top += 2;
		title.bottom = title.top + 9;

		char const *name = track->GetFlexControllerName();
		char sz[ 128 ];

		if ( scount > 0 )
		{
			sprintf( sz, "{%i} - ", scount );

			int len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );
			drawHelper.DrawColoredText( "Arial", 9, 500, 
				RGB( 120, 120, 0 ), title, sz );

			title.left += len + 2;
		}

		sprintf( sz, "%s -", name );

		int len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );

		drawHelper.DrawColoredText( "Arial", 9, 500, 
			active ? RGB( 0, 150, 100 ) : RGB( 100, 100, 100 ), 
			title, sz );

		sprintf( sz, "%s", IsActive() ? "enabled" : "disabled" );

		title.left += len + 2;

		len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );
		drawHelper.DrawColoredText( "Arial", 9, 500, 
			active ? RGB( 0, 150, 100 ) : RGB( 100, 100, 100 ), 
			title, sz );

		if ( active )
		{
			title.left += len + 2;

			sprintf( sz, " <%i>", track->GetNumSamples( 0 ) );

			len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );
			drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 220, 0, 00 ), title, sz );
		}
	}
}

void TimelineItem::DrawAutoHighlight( mxEvent *event )
{
	if ( IsCollapsed() )
		return;
	
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	CExpressionSample *hover = GetSampleUnderMouse( event->x, event->y, 0.0f );
	CChoreoWidgetDrawHelper drawHelper( m_pWorkspace, m_rcBounds, true );

	RECT rcClient = m_rcBounds;

	// Draw left/right underneath amount so go backbard
	int type = m_nEditType;

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
	for ( int sample = 0; sample < track->GetNumSamples( type ); sample++ )
	{
		bool dummy;
		CExpressionSample *start = track->GetBoundedSample( sample, dummy, type );

		float value = start->value;
		bool clipped = false;
		int x = GetMouseForTime( start->time, &clipped );
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

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//-----------------------------------------------------------------------------
void TimelineItem::DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	CChoreoEvent *event = track->GetEvent();
	if ( !event )
		return;

	float duration = event->GetDuration();

	if ( duration <= 0.0f )
		return;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return;

	RECT rcClient = m_rcBounds;;
	//drawHelper.GetClientRect( rcClient );

	// Iterate relative tags
	for ( int i = 0; i < scene->GetNumActors(); i++ )
	{
		CChoreoActor *a = scene->GetActor( i );
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannel *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0 ; k < c->GetNumEvents(); k++ )
			{
				CChoreoEvent *e = c->GetEvent( k );
				if ( !e )
					continue;

				// add each tag to combo box
				for ( int t = 0; t < e->GetNumRelativeTags(); t++ )
				{
					CEventRelativeTag *tag = e->GetRelativeTag( t );
					if ( !tag )
						continue;

					//SendMessage( control, CB_ADDSTRING, 0, (LPARAM)va( "\"%s\" \"%s\"", tag->GetName(), e->GetParameters() ) ); 
					bool clipped = false;
					int tagx = GetMouseForTime( tag->GetStartTime() - event->GetStartTime(), &clipped );
					if ( clipped )
						continue;

					drawHelper.DrawColoredLine( RGB( 180, 180, 220 ), PS_SOLID, 1, tagx, rcClient.top, tagx, rcClient.bottom );
				}
			}
		}
	}

	for ( int t = 0; t < event->GetNumTimingTags(); t++ )
	{
		CFlexTimingTag *tag = event->GetTimingTag( t );
		if ( !tag )
			continue;

		bool clipped = false;
		int tagx = GetMouseForTime( tag->GetStartTime() - event->GetStartTime(), &clipped );
		if ( clipped )
			continue;
		
		// Draw relative tag marker
		drawHelper.DrawColoredLine( RGB( 220, 180, 180 ), PS_SOLID, 1, tagx, rcClient.top, tagx, rcClient.bottom );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *exp - 
//			flexnum - 
//-----------------------------------------------------------------------------
void TimelineItem::SetExpressionInfo( CFlexAnimationTrack *track, int flexnum )
{
	m_szTrackName[ 0 ] = 0;
	if ( track )
	{
		V_strcpy_safe( m_szTrackName, track->GetFlexControllerName() );
		SetActive( track->IsTrackActive() );
	}

	m_nFlexNum = flexnum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::Copy( void )
{
	if ( !g_pExpressionTool )
		return;

	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	g_pExpressionTool->Copy( track );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::Paste( void )
{
	if ( !g_pExpressionTool )
		return;

	if ( !g_pExpressionTool->HasCopyData() )
		return;

	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	g_pExpressionTool->Paste( track );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::Clear( bool preserveundo )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	if ( preserveundo )
	{
		PreDataChanged( "Clear" );
	}

	track->Clear();

	if ( preserveundo )
	{
		PostDataChanged( "Clear" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void TimelineItem::SetCollapsed( bool state )
{
	m_bCollapsed = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TimelineItem::IsCollapsed( void ) const
{
	return m_bCollapsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int TimelineItem::GetHeight( void )
{
	return ( IsCollapsed() ? 12 : m_nCurrentHeight );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void TimelineItem::SetActive( bool state )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	track->SetTrackActive( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TimelineItem::IsActive( void )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return false;

	return track->IsTrackActive();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TimelineItem::IsValid( void )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return false;

	if ( track->GetNumSamples( 0 ) > 0 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CFlexAnimationTrack
//-----------------------------------------------------------------------------
CFlexAnimationTrack *TimelineItem::GetSafeTrack( void )
{	
	if ( !g_pExpressionTool )
		return NULL;

	CChoreoEvent *ev = g_pExpressionTool->GetSafeEvent();
	if ( !ev )
		return NULL;

	// Find track by name
	for ( int i = 0; i < ev->GetNumFlexAnimationTracks() ; i++ )
	{
		CFlexAnimationTrack *track = ev->GetFlexAnimationTrack( i );
		if ( track && !stricmp( track->GetFlexControllerName(), m_szTrackName ) )
		{
			return track;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void TimelineItem::SetEditType( int type )
{
	Assert( type == 0 || type == 1 );

	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track || !track->IsComboType() )
	{
		type = 0;
	}
	
	m_nEditType = type;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int TimelineItem::GetEditType( void )
{
	return m_nEditType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::SelectPoints( void )
{
	RECT rcSelection;
	
	rcSelection.left = m_nStartX < m_nLastX ? m_nStartX : m_nLastX;
	rcSelection.right = m_nStartX < m_nLastX ? m_nLastX : m_nStartX;

	rcSelection.top = m_nStartY < m_nLastY ? m_nStartY : m_nLastY;
	rcSelection.bottom = m_nStartY < m_nLastY ? m_nLastY : m_nStartY;

	int selW = rcSelection.right - rcSelection.left;
	int selH = rcSelection.bottom - rcSelection.top;

	float tolerance = FP_TL_SELECTION_RECTANGLE_TOLERANCE;
	// If they are just clicking and releasing in one spot, capture any items w/in a larger tolerance
	if ( selW <= 2 && selH <= 2 )
	{
		tolerance = FP_TL_SELECTION_TOLERANCE;

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

	int width = m_rcBounds.right - m_rcBounds.left;
	int height = m_rcBounds.bottom - m_rcBounds.top;

	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track || !width || !height )
		return;

	CChoreoEvent *e = track->GetEvent();
	Assert( e );
	if ( !e )
		return;

	float duration = e->GetDuration();

	float fleft = (float)GetTimeForMouse( rcSelection.left + m_rcBounds.left );
	float fright = (float)GetTimeForMouse( rcSelection.right + m_rcBounds.left );

	//fleft *= duration;
	//fright *= duration;

	float ftop = (float)rcSelection.top / (float)height;
	float fbottom = (float)rcSelection.bottom / (float)height;

	fleft = clamp( fleft, 0.0f, duration );
	fright = clamp( fright, 0.0f, duration );
	ftop = clamp( ftop, 0.0f, 1.0f );
	fbottom = clamp( fbottom, 0.0f, 1.0f );

	float timestepperpixel = 1.0f / g_pExpressionTool->GetPixelsPerSecond();
	float yfracstepperpixel = 1.0f / (float)height;

	float epsx = tolerance*timestepperpixel;
	float epsy = tolerance*yfracstepperpixel;

	for ( int i = 0; i < track->GetNumSamples( m_nEditType ); i++ )
	{
		CExpressionSample *sample = track->GetSample( i, m_nEditType );
		
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
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *undodescription - 
//-----------------------------------------------------------------------------
void TimelineItem::PreDataChanged( char const *undodescription )
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
void TimelineItem::PostDataChanged( char const *redodescription )
{
	--m_nUndoSetup;
	if ( m_nUndoSetup == 0 )
	{
		g_pChoreoView->PushRedo( redodescription );
		g_pExpressionTool->InvalidateLayout();
	}
}

void TimelineItem::SetBounds( const RECT& rect )
{
	m_rcBounds = rect;
}

void TimelineItem::GetBounds( RECT& rect )
{
	rect = m_rcBounds;
}

void TimelineItem::SetVisible( bool vis )
{
	m_bVisible = vis;
}

bool TimelineItem::GetVisible( void ) const
{
	return m_bVisible;
}

int TimelineItem::GetNumSelected( void ) 
{
	return m_nNumSelected;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::SnapAll()
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	for ( int t = 0; t < 2; t++ )
	{
		for ( int i = 0; i <  track->GetNumSamples( t ); i++ )
		{
			CExpressionSample *sample = track->GetSample( i, t );
			sample->time = FacePoser_SnapTime( sample->time );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::SnapSelected()
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	for ( int t = 0; t < 2; t++ )
	{
		for ( int i = 0; i <  track->GetNumSamples( t ); i++ )
		{
			CExpressionSample *sample = track->GetSample( i, t );
			if ( !sample->selected )
				continue;

			sample->time = FacePoser_SnapTime( sample->time );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//-----------------------------------------------------------------------------
void TimelineItem::DeletePoints( float start, float end )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	for ( int t = 0; t < 2; t++ )
	{
		int num = track->GetNumSamples( t );
		for ( int i = num - 1; i >= 0 ; i-- )
		{
			CExpressionSample *sample = track->GetSample( i, t );
			if ( sample->time < start || sample->time > end )
				continue;

			track->RemoveSample( i, t );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TimelineItem::OnDoubleClicked()
{
	// Disabled for now by request of BillF
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	SetCollapsed( !IsCollapsed() );
	g_pExpressionTool->LayoutItems( true );
}

void TimelineItem::DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper )
{
	CFlexAnimationTrack *track = GetSafeTrack();
	if ( !track )
		return;

	CChoreoEvent *e = track->GetEvent();
	if ( !e )
		return;

	float duration = e->GetDuration();
	if ( !duration )
		return;

	int leftx = GetMouseForTime( duration );
	if ( leftx > m_rcBounds.right )
		return;

	drawHelper.DrawColoredLine(
		COLOR_CHOREO_ENDTIME, PS_SOLID, 1,
		leftx, m_rcBounds.top, leftx, m_rcBounds.bottom );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : helper - 
//			handleRect - 
//-----------------------------------------------------------------------------
void TimelineItem::DrawGrowHandle( CChoreoWidgetDrawHelper& helper, RECT& handleRect )
{
	Assert(CanHaveGrowHandle());

	RECT useRect = handleRect;
	helper.OffsetSubRect( useRect );

	POINT region[4];
	int cPoints = 4;

	region[ 0 ].x = useRect.left + GROW_HANDLE_INSETPIXELS;
	region[ 0 ].y = useRect.top;

	region[ 1 ].x = useRect.right - GROW_HANDLE_INSETPIXELS;
	region[ 1 ].y = useRect.top;

	region[ 2 ].x = useRect.right;
	region[ 2 ].y = useRect.bottom;

	region[ 3 ].x = useRect.left;
	region[ 3 ].y = useRect.bottom;

	HDC dc = helper.GrabDC();

	HRGN rgn = CreatePolygonRgn( region, cPoints, ALTERNATE );

	int oldPF = SetPolyFillMode( dc, ALTERNATE );
	
	HBRUSH brBg = CreateSolidBrush( RGB( 150, 150, 150 ) );
	HBRUSH brBorder = CreateSolidBrush( RGB( 200, 200, 200 ) );

	FillRgn( dc, rgn, brBg );
	FrameRgn( dc, rgn, brBorder, 1, 1 );

	SetPolyFillMode( dc, oldPF );

	DeleteObject( rgn );

	DeleteObject( brBg );
	DeleteObject( brBorder );

	// draw a line in the middle
	int midy = ( handleRect.bottom + handleRect.top ) * 0.5f;
	int lineinset = GROW_HANDLE_INSETPIXELS *1.5;

	helper.DrawColoredLine( RGB( 63, 63, 63 ), PS_SOLID, 1, 
		handleRect.left + lineinset, midy,
		handleRect.right - lineinset, midy );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void TimelineItem::GetGrowHandleRect( RECT& rc )
{
	rc = m_rcBounds;
	rc.bottom -= 1;
	rc.top = rc.bottom - GROW_HANDLE_HEIGHT;
	rc.left = ( rc.right + rc.left ) / 2 - GROW_HANDLE_WIDTH / 2;
	rc.right = rc.left +  GROW_HANDLE_WIDTH;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TimelineItem::CanHaveGrowHandle()
{
	if ( IsCollapsed() )
		return false;

	if ( !g_pExpressionTool->IsFocusItem( this ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TimelineItem::IsMouseOverGrowHandle( int x, int y)
{
	if ( !CanHaveGrowHandle() )
		return false;

	RECT rcGrowHandle;
	GetGrowHandleRect( rcGrowHandle );

	POINT pt;
	pt.x = x + m_rcBounds.left;
	pt.y = y + m_rcBounds.top;

	return PtInRect( &rcGrowHandle, pt ) ? true : false;
}

void TimelineItem::DrawGrowRect()
{
	RECT rcFocus = m_rcBounds;
	rcFocus.bottom = m_rcBounds.top + m_nLastY;
	OffsetRect( &rcFocus, -m_rcBounds.left, -m_rcBounds.top );

	POINT offset;
	offset.x = m_rcBounds.left;
	offset.y = m_rcBounds.top;
	ClientToScreen( (HWND)m_pWorkspace->getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	HDC dc = GetDC( NULL );

	::DrawFocusRect( dc, &rcFocus );

	ReleaseDC( NULL, dc );
}

void TimelineItem::GetWorkList( bool reflect, CUtlVector< TimelineItem * >& list )
{
	if ( !reflect )
	{
		list.AddToTail( this );
	}
	else
	{
		g_pExpressionTool->GetTimelineItems( list );
	}
}
