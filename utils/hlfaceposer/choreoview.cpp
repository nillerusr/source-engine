//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include <stdio.h>
#include <mxtk/mxWindow.h>
#include "mdlviewer.h"
#include "hlfaceposer.h"
#include "StudioModel.h"
#include "expressions.h"
#include "expclass.h"
#include "ChoreoView.h"
#include "choreoevent.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "choreoscene.h"
#include "choreowidget.h"
#include "choreoactorwidget.h"
#include "choreochannelwidget.h"
#include "choreoglobaleventwidget.h"
#include "choreowidgetdrawhelper.h"
#include "choreoeventwidget.h"
#include "viewerSettings.h"
#include "filesystem.h"
#include "choreoviewcolors.h"
#include "ActorProperties.h"
#include "ChannelProperties.h"
#include "EventProperties.h"
#include "GlobalEventProperties.h"
#include "ifaceposersound.h"
#include "snd_wave_source.h"
#include "ifaceposerworkspace.h"
#include "PhonemeEditor.h"
#include "iscenetokenprocessor.h"
#include "InputProperties.h"
#include "filesystem.h"
#include "ExpressionTool.h"
#include "ControlPanel.h"
#include "faceposer_models.h"
#include "choiceproperties.h"
#include "MatSysWin.h"
#include "tier1/strtools.h"
#include "GestureTool.h"
#include "npcevent.h"
#include "RampTool.h"
#include "SceneRampTool.h"
#include "KeyValues.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "cclookup.h"
#include "iclosecaptionmanager.h"
#include "AddSoundEntry.h"
#include "isoundcombiner.h"
#include <vgui/ILocalize.h>
#include "scriplib.h"
#include "WaveBrowser.h"
#include "filesystem_init.h"
#include "flexpanel.h"
#include "tier3/choreoutils.h"
#include "tier2/p4helpers.h"


using namespace vgui;

extern vgui::ILocalize *g_pLocalize;

// 10x magnification
#define MAX_TIME_ZOOM 1000
#define TIME_ZOOM_STEP 4

#define PHONEME_FILTER 0.08f
#define PHONEME_DELAY  0.0f

#define SCRUBBER_HEIGHT	15
#define TIMELINE_NUMBERS_HEIGHT 11

#define COPYPASTE_FILENAME	"scenes/copydatavcd.txt"

extern double realtime;
extern bool NameLessFunc( const char *const& name1, const char *const& name2 );

// Try to keep shifted times at same absolute time
static void RescaleExpressionTimes( CChoreoEvent *event, float newstart, float newend )
{
	if ( !event || event->GetType() != CChoreoEvent::FLEXANIMATION )
		return;

	// Did it actually change
	if ( newstart == event->GetStartTime() &&
		 newend == event->GetEndTime() )
	{
		 return;
	}

	float newduration = newend - newstart;

	float dt = 0.0f;
	//If the end is moving, leave tags stay where they are (dt == 0.0f)
	if ( newstart != event->GetStartTime() )
	{
		// Otherwise, if the new start is later, then tags need to be shifted backwards
		dt -= ( newstart - event->GetStartTime() );
	}

	int count = event->GetNumFlexAnimationTracks();
	int i;

	for ( i = 0; i < count; i++ )
	{
		CFlexAnimationTrack *track = event->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		for ( int type = 0; type < 2; type++ )
		{
			int sampleCount = track->GetNumSamples( type );
			for ( int sample = sampleCount - 1; sample >= 0 ; sample-- )
			{
				CExpressionSample *s = track->GetSample( sample, type );
				if ( !s )
					continue;

				s->time += dt;

				if ( s->time > newduration || s->time < 0.0f )
				{
					track->RemoveSample( sample, type );
				}
			}
		}
	}
}

static void RescaleRamp( CChoreoEvent *event, float newduration )
{
	float oldduration = event->GetDuration();

	if ( fabs( oldduration - newduration ) < 0.000001f )
		return;

	if ( newduration <= 0.0f )
		return;

	float midpointtime = oldduration * 0.5f;
	float newmidpointtime = newduration * 0.5f;

	int count = event->GetRampCount();
	int i;

	for ( i = 0; i < count; i++ )
	{
		CExpressionSample *sample = event->GetRamp( i );
		if ( !sample )
			continue;

		float t = sample->time;
		if ( t < midpointtime )
			continue;

		float timefromend = oldduration - t;

		// There's room to just shift it
		if ( timefromend <= newmidpointtime )
		{
			t = newduration - timefromend;
		}
		else
		{
			// No room, rescale them instead
			float frac = ( t - midpointtime ) / midpointtime;
			t = newmidpointtime + frac * newmidpointtime;
		}

		sample->time = t;
	}
}

bool DoesAnyActorHaveAssociatedModelLoaded( CChoreoScene *scene )
{
	if ( !scene )
		return false;

	int c = scene->GetNumActors();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CChoreoActor *a = scene->GetActor( i );
		if ( !a )
			continue;

		char const *modelname = a->GetFacePoserModelName();
		if ( !modelname )
			continue;

		if ( !modelname[ 0 ] )
			continue;

		char mdlname[ 256 ];
		Q_strncpy( mdlname, modelname, sizeof( mdlname ) );
		Q_FixSlashes( mdlname );

		int idx = models->FindModelByFilename( mdlname );
		if ( idx >= 0 )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *a - 
// Output : StudioModel
//-----------------------------------------------------------------------------
StudioModel *FindAssociatedModel( CChoreoScene *scene, CChoreoActor *a )
{
	if ( !a || !scene )
		return NULL;

	Assert( models->GetActiveStudioModel() );

	StudioModel *model = NULL;
	if ( a->GetFacePoserModelName()[ 0 ] )
	{
		int idx = models->FindModelByFilename( a->GetFacePoserModelName() );
		if ( idx >= 0 )
		{
			model = models->GetStudioModel( idx );
			return model;
		}
	}

	// Is there any loaded model with the actorname in it?
	int c = models->Count();
	for ( int i = 0; i < c; i++ )
	{
		char const *modelname = models->GetModelName( i );
		if ( !Q_stricmp( modelname, a->GetName() ) )
		{
			return models->GetStudioModel( i );
		}
	}

	// Does any actor have an associated model which is loaded
	if ( DoesAnyActorHaveAssociatedModelLoaded( scene ) )
	{
		// Then return NULL here so we don't override with the default an actor who has a valid model going
		return NULL;
	}

	// Couldn't find it and nobody else has a loaded associated model, so just use the default model
	if ( !model )
	{
		model = models->GetActiveStudioModel();
	}
	return model;
}


CChoreoView		*g_pChoreoView = 0;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			x - 
//			y - 
//			w - 
//			h - 
//			id - 
//-----------------------------------------------------------------------------
CChoreoView::CChoreoView( mxWindow *parent, int x, int y, int w, int h, int id )
: IFacePoserToolWindow( "CChoreoView", "Choreography" ), mxWindow( parent, x, y, w, h )
{
	m_bRampOnly = false;

	m_bForceProcess = false;

	m_bSuppressLayout = true;

	SetAutoProcess( true );

	m_flLastMouseClickTime = -1.0f;
	m_bProcessSequences = true;

	m_flPlaybackRate	= 1.0f;

	m_pScene = NULL;

	m_flScrub			= 0.0f;
	m_flScrubTarget		= 0.0f;

	m_bCanDraw = false;

	m_bRedoPending = false;
	m_nUndoLevel = 0;

	CChoreoEventWidget::LoadImages();

	CChoreoWidget::m_pView = this;

	setId( id );

	m_flLastSpeedScale = 0.0f;
	m_bResetSpeedScale = false;

	m_nTopOffset = 0;
	m_flLeftOffset = 0.0f;
	m_nLastHPixelsNeeded = -1;
	m_nLastVPixelsNeeded = -1;


	m_nStartRow = 45;
	m_nLabelWidth = 140;
	m_nRowHeight = 35;

	m_bSimulating = false;
	m_bPaused = false;

	m_bForward = true;

	m_flStartTime = 0.0f;
	m_flEndTime = 0.0f;
	m_flFrameTime = 0.0f;

	m_bAutomated		= false;
	m_nAutomatedAction	= SCENE_ACTION_UNKNOWN;
	m_flAutomationDelay = 0.0f;
	m_flAutomationTime = 0.0f;

	m_pVertScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_CHOREOVSCROLL, mxScrollbar::Vertical );
	m_pHorzScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_CHOREOHSCROLL, mxScrollbar::Horizontal );

	m_bLayoutIsValid = false;
	m_flPixelsPerSecond = 150.0f;

	m_btnPlay = new mxBitmapButton( this, 2, 4, 16, 16, IDC_PLAYSCENE, "gfx/hlfaceposer/play.bmp" );
	m_btnPause = new mxBitmapButton( this, 18, 4, 16, 16, IDC_PAUSESCENE, "gfx/hlfaceposer/pause.bmp"  );
	m_btnStop = new mxBitmapButton( this, 34, 4, 16, 16, IDC_STOPSCENE, "gfx/hlfaceposer/stop.bmp"  );

	m_pPlaybackRate				= new mxSlider( this, 0, 0, 16, 16, IDC_CHOREO_PLAYBACKRATE );
	m_pPlaybackRate->setRange( 0.0, 2.0, 40 );
	m_pPlaybackRate->setValue( m_flPlaybackRate );

	ShowButtons( false );

	m_nFontSize = 12;

	for ( int i = 0; i < MAX_ACTORS; i++ )
	{
		m_ActorExpanded[ i ].expanded = true;
	}

	SetChoreoFile( "" );

	//SetFocus( (HWND)getHandle() );
	if ( workspacefiles->GetNumStoredFiles( IWorkspaceFiles::CHOREODATA ) >= 1 )
	{
		LoadSceneFromFile( workspacefiles->GetStoredFile( IWorkspaceFiles::CHOREODATA, 0 ) );
	}

	ClearABPoints();

	m_pClickedActor = NULL;
	m_pClickedChannel = NULL;
	m_pClickedEvent = NULL;
	m_pClickedGlobalEvent = NULL;
	m_nClickedX = 0;
	m_nClickedY = 0;
	m_nSelectedEvents = 0;
	m_nClickedTag = -1;
	m_nClickedChannelCloseCaptionButton = CChoreoChannelWidget::CLOSECAPTION_NONE;

	// Mouse dragging
	m_bDragging			= false;
	m_xStart			= 0;
	m_yStart			= 0;
	m_nDragType			= DRAGTYPE_NONE;
	m_hPrevCursor		= 0;

	m_nMinX				= 0;
	m_nMaxX				= 0;
	m_bUseBounds		= false;

	m_nScrollbarHeight	= 12;
	m_nInfoHeight		= 30;

	ClearStatusArea();

	SetDirty( false );

	m_bCanDraw = true;
	m_bSuppressLayout = false;
	m_flScrubberTimeOffset = 0.0f;

	m_bShowCloseCaptionData = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : closing - 
//-----------------------------------------------------------------------------
bool CChoreoView::Close( void )
{
	if ( m_pScene && m_bDirty )
	{
		int retval = mxMessageBox( NULL, va( "Save changes to scene '%s'?", GetChoreoFile() ), g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval == 2 )
		{
			return false;
		}
		if ( retval == 0 )
		{
			Save();
		}
	}

	if ( m_pScene )
	{
		UnloadScene();
	}
	return true;
}

bool CChoreoView::CanClose()
{
	if ( m_pScene )
	{
		workspacefiles->StartStoringFiles( IWorkspaceFiles::CHOREODATA );
		workspacefiles->StoreFile( IWorkspaceFiles::CHOREODATA, GetChoreoFile() );
		workspacefiles->FinishStoringFiles( IWorkspaceFiles::CHOREODATA );
	}

	if ( m_pScene && m_bDirty && !Close() )
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called just before window is destroyed
//-----------------------------------------------------------------------------
void CChoreoView::OnDelete()
{
	if ( m_pScene )
	{
		UnloadScene();
	}

	CChoreoWidget::m_pView = NULL;

	CChoreoEventWidget::DestroyImages();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoView::~CChoreoView()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::ReportSceneClearToTools( void )
{
	if ( m_pScene )
	{
		m_pScene->ResetSimulation();
	}

	g_pPhonemeEditor->ClearEvent();
	g_pExpressionTool->LayoutItems( true );
	g_pExpressionTool->redraw();
	g_pGestureTool->redraw();
	g_pRampTool->redraw();
	g_pSceneRampTool->redraw();
}

//-----------------------------------------------------------------------------
// Purpose: Find a time that's less than input on the granularity:
// e.g., 3.01 granularity 0.05 will be 3.00, 3.05 will be 3.05
// Input  : input - 
//			granularity - 
// Output : float
//-----------------------------------------------------------------------------
float SnapTime( float input, float granularity )
{
	float base = (float)(int)input;
	float multiplier = (float)(int)( 1.0f / granularity );
	float fracpart = input - (int)input;

	fracpart *= multiplier;

	fracpart = (float)(int)fracpart;
	fracpart *= granularity;

	return base + fracpart;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//			left - 
//			right - 
//-----------------------------------------------------------------------------
void CChoreoView::DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right )
{
	RECT rcFill = m_rcTimeLine;
	rcFill.bottom -= TIMELINE_NUMBERS_HEIGHT;
	drawHelper.DrawFilledRect( COLOR_CHOREO_DARKBACKGROUND, rcFill );

	RECT rcLabel;
	float granularity = 0.5f / ((float)GetTimeZoom( GetToolName() ) / 100.0f);

	drawHelper.DrawColoredLine( COLOR_CHOREO_TIMELINE, PS_SOLID, 1, rc.left, GetStartRow() - 1, rc.right, GetStartRow() - 1 );

	float f = SnapTime( left, granularity );
	while ( f < right )
	{
		float frac = ( f - left ) / ( right - left );
		if ( frac >= 0.0f && frac <= 1.0f )
		{
			rcLabel.left = GetLabelWidth() + (int)( frac * ( rc.right - GetLabelWidth() ) );
			rcLabel.bottom = GetStartRow() - 1;
			rcLabel.top = rcLabel.bottom - 10;

			if ( f != left )
			{
				drawHelper.DrawColoredLine( RGB( 220, 220, 240 ), PS_DOT,  1, 
					rcLabel.left, GetStartRow(), rcLabel.left, h2() );
			}

			char sz[ 32 ];
			sprintf( sz, "%.2f", f );

			int textWidth = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

			rcLabel.right = rcLabel.left + textWidth;

			OffsetRect( &rcLabel, -textWidth / 2, 0 );

			drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, COLOR_CHOREO_TEXT, rcLabel, sz );

		}
		f += granularity;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::PaintBackground( void )
{
	redraw();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//-----------------------------------------------------------------------------
void CChoreoView::DrawSceneABTicks( CChoreoWidgetDrawHelper& drawHelper )
{
	RECT rcThumb;

	float scenestart = m_rgABPoints[ 0 ].active ? m_rgABPoints[ 0 ].time : 0.0f;
	float sceneend = m_rgABPoints[ 1 ].active ? m_rgABPoints[ 1 ].time : 0.0f;

	if ( scenestart )
	{
		int markerstart = GetPixelForTimeValue( scenestart );

		rcThumb.left = markerstart - 4;
		rcThumb.right = markerstart + 4;
		rcThumb.top = 2 + GetCaptionHeight() + SCRUBBER_HEIGHT;
		rcThumb.bottom = rcThumb.top + 8;

		drawHelper.DrawTriangleMarker( rcThumb, COLOR_CHOREO_TICKAB );
	}

	if ( sceneend )
	{
		int markerend = GetPixelForTimeValue( sceneend );

		rcThumb.left = markerend - 4;
		rcThumb.right = markerend + 4;
		rcThumb.top = 2 + GetCaptionHeight() + SCRUBBER_HEIGHT;
		rcThumb.bottom = rcThumb.top + 8;

		drawHelper.DrawTriangleMarker( rcThumb, COLOR_CHOREO_TICKAB );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rc - 
//-----------------------------------------------------------------------------
void CChoreoView::DrawRelativeTagLines( CChoreoWidgetDrawHelper& drawHelper, RECT& rc )
{
	if ( !m_pScene )
		return;

	RECT rcClip;
	GetClientRect( (HWND)getHandle(), &rcClip );
	rcClip.top = GetStartRow();
	rcClip.bottom -= ( m_nInfoHeight + m_nScrollbarHeight );
	rcClip.right -= m_nScrollbarHeight;

	drawHelper.StartClipping( rcClip );

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				CChoreoEvent *event = e->GetEvent();
				if ( !event )
					continue;

				if ( !event->IsUsingRelativeTag() )
					continue;

				// Using it, find the tag and figure out the time for it
				CEventRelativeTag *tag = m_pScene->FindTagByName( 
					event->GetRelativeWavName(),
					event->GetRelativeTagName() );

				if ( !tag )
					continue;

				// Found it, draw a vertical line
				//
				float tagtime = tag->GetStartTime();
				
				// Convert to pixel value
				bool clipped = false;
				int pixel = GetPixelForTimeValue( tagtime, &clipped );
				if ( clipped )
					continue;

				drawHelper.DrawColoredLine( RGB( 180, 180, 220 ), PS_SOLID, 1, 
					pixel, rcClip.top, pixel, rcClip.bottom );
			}
		}
	}

	drawHelper.StopClipping();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the background markings (actor names, etc)
// Input  : drawHelper - 
//			rc - 
//-----------------------------------------------------------------------------
void CChoreoView::DrawBackground( CChoreoWidgetDrawHelper& drawHelper, RECT& rc )
{
	RECT rcClip;
	GetClientRect( (HWND)getHandle(), &rcClip );
	rcClip.top = GetStartRow();
	rcClip.bottom -= ( m_nInfoHeight + m_nScrollbarHeight );
	rcClip.right -= m_nScrollbarHeight;

	int i;

	for ( i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( event )
		{
			event->redraw( drawHelper );
		}
	}

	drawHelper.StartClipping( rcClip );

	for ( i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actorW = m_SceneActors[ i ];
		if ( !actorW )
			continue;

		actorW->redraw( drawHelper );
	}

	drawHelper.StopClipping();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::redraw() 
{
	if ( !ToolCanDraw() )
		return;

	if ( m_bSuppressLayout )
		return;

	LayoutScene();

	CChoreoWidgetDrawHelper drawHelper( this, COLOR_CHOREO_BACKGROUND );
	HandleToolRedraw( drawHelper );

	if ( !m_bCanDraw )
		return;

	RECT rc;
	rc.left = 0;
	rc.top = GetCaptionHeight();
	rc.right = drawHelper.GetWidth();
	rc.bottom = drawHelper.GetHeight();

	RECT rcInfo;
	rcInfo.left = rc.left;
	rcInfo.right = rc.right - m_nScrollbarHeight;
	rcInfo.bottom = rc.bottom - m_nScrollbarHeight;
	rcInfo.top = rcInfo.bottom - m_nInfoHeight;

	drawHelper.StartClipping( rcInfo );

	RedrawStatusArea( drawHelper, rcInfo );

	drawHelper.StopClipping();

	RECT rcClip = rc;
	rcClip.bottom -= ( m_nInfoHeight + m_nScrollbarHeight );

	drawHelper.StartClipping( rcClip );

	if ( !m_pScene )
	{
		char sz[ 256 ];
		sprintf( sz, "No choreography scene file (.vcd) loaded" );

		int pointsize = 18;
		int textlen = drawHelper.CalcTextWidth( "Arial", pointsize, FW_NORMAL, sz );

		RECT rcText;
		rcText.top = ( rc.bottom - rc.top ) / 2 - pointsize / 2;
		rcText.bottom = rcText.top + pointsize + 10;
		rcText.left = rc.right / 2 - textlen / 2;
		rcText.right = rcText.left + textlen;

		drawHelper.DrawColoredText( "Arial", pointsize, FW_NORMAL, COLOR_CHOREO_LIGHTTEXT, rcText, sz );

		drawHelper.StopClipping();
		return;
	}

	DrawTimeLine( drawHelper, rc, m_flStartTime, m_flEndTime );

	bool clipped = false;
	int finishx = GetPixelForTimeValue( m_pScene->FindStopTime(), &clipped );
	if ( !clipped )
	{
		drawHelper.DrawColoredLine( COLOR_CHOREO_ENDTIME, PS_DOT, 1, finishx, rc.top + GetStartRow(), finishx, rc.bottom );
	}

	DrawRelativeTagLines( drawHelper, rc );
	DrawBackground( drawHelper, rc );

	DrawSceneABTicks( drawHelper );

	drawHelper.StopClipping();

	if ( m_UndoStack.Size() > 0 )
	{
		int length = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, 
			"undo %i/%i", m_nUndoLevel, m_UndoStack.Size() );
		RECT rcText = rc;
		rcText.top = rc.top + 48;
		rcText.bottom = rcText.top + 10;
		rcText.left = GetLabelWidth() - length - 20;
		rcText.right = rcText.left + length;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 100, 180, 100 ), rcText,
			"undo %i/%i", m_nUndoLevel, m_UndoStack.Size() );
	}

	DrawScrubHandle( drawHelper );

	char sz[ 48 ];
	sprintf( sz, "Speed: %.2fx", m_flPlaybackRate );

	int fontsize = 9;

	int length = drawHelper.CalcTextWidth( "Arial", fontsize, FW_NORMAL, sz);
	
	RECT rcText = rc;
	rcText.top = rc.top + 25;
	rcText.bottom = rcText.top + 10;
	rcText.left = GetLabelWidth() + 20;
	rcText.right = rcText.left + length;

	drawHelper.DrawColoredText( "Arial", fontsize, FW_NORMAL, 
		RGB( 50, 50, 50 ), rcText, sz );

	sprintf( sz, "Zoom: %.2fx", (float)GetTimeZoom( GetToolName() ) / 100.0f );

	length = drawHelper.CalcTextWidth( "Arial", fontsize, FW_NORMAL, sz);

	rcText = rc;
	rcText.left = 5;
	rcText.top = rc.top + 48;
	rcText.bottom = rcText.top + 10;
	rcText.right = rcText.left + length;

	drawHelper.DrawColoredText( "Arial", fontsize, FW_NORMAL, 
		RGB( 50, 50, 50 ), rcText, sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : current - 
//			number - 
// Output : int
//-----------------------------------------------------------------------------
void CChoreoView::GetUndoLevels( int& current, int& number )
{
	current = m_nUndoLevel;
	number	= m_UndoStack.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			*clipped - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetPixelForTimeValue( float time, bool *clipped /*=NULL*/ )
{
	if ( clipped )
	{
		*clipped = false;
	}

	float frac = ( time - m_flStartTime ) / ( m_flEndTime - m_flStartTime );
	if ( frac < 0.0 || frac > 1.0 )
	{
		if ( clipped )
		{
			*clipped = true;
		}
	}

	int pixel = GetLabelWidth() + (int)( frac * ( w2() - GetLabelWidth() ) );
	return pixel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			clip - 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoView::GetTimeValueForMouse( int mx, bool clip /*=false*/)
{
	RECT rc = m_rcTimeLine;
	rc.left = GetLabelWidth();

	if ( clip )
	{
		if ( mx < rc.left )
		{
			return m_flStartTime;
		}
		if ( mx > rc.right )
		{
			return m_flEndTime;
		}
	}

	float frac = (float)( mx - rc.left )  / (float)( rc.right - rc.left );

	return m_flStartTime + frac * ( m_flEndTime - m_flStartTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//-----------------------------------------------------------------------------
void CChoreoView::SetStartTime( float time )
{
	m_flStartTime = time;
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoView::GetStartTime( void )
{
	return m_flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoView::GetEndTime( void )
{
	return m_flEndTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoView::GetPixelsPerSecond( void )
{
	return m_flPixelsPerSecond * (float)GetTimeZoom( GetToolName() ) / 100.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			origmx - 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoView::GetTimeDeltaForMouseDelta( int mx, int origmx )
{
	float t1, t2;

	t2 = GetTimeValueForMouse( mx );
	t1 = GetTimeValueForMouse( origmx );

	return t2 - t1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//-----------------------------------------------------------------------------
void CChoreoView::PlaceABPoint( int mx )
{
	m_rgABPoints[ ( m_nCurrentABPoint) & 0x01 ].time = GetTimeValueForMouse( mx );
	m_rgABPoints[ ( m_nCurrentABPoint) & 0x01 ].active = true;
	m_nCurrentABPoint++;

	if ( m_rgABPoints[ 0 ].active && m_rgABPoints	[ 1 ].active && 
		 m_rgABPoints[ 0 ].time > m_rgABPoints[ 1 ].time )
	{
		float temp = m_rgABPoints[ 0 ].time;
		m_rgABPoints[ 0 ].time = m_rgABPoints[ 1 ].time;
		m_rgABPoints[ 1 ].time = temp;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::ClearABPoints( void )
{
	memset( m_rgABPoints, 0, sizeof( m_rgABPoints ) );
	m_nCurrentABPoint = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::IsMouseOverTimeline( int mx, int my )
{
	POINT pt;
	pt.x = mx;
	pt.y = my;

	RECT rcCheck = m_rcTimeLine;
	rcCheck.bottom -= TIMELINE_NUMBERS_HEIGHT;

	if ( PtInRect( &rcCheck, pt ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::ShowContextMenu( int mx, int my )
{
	CChoreoActorWidget		*a = NULL;
	CChoreoChannelWidget	*c = NULL;
	CChoreoEventWidget		*e = NULL;
	CChoreoGlobalEventWidget *ge = NULL;
	int						ct = -1;
	CEventAbsoluteTag		*at = NULL;
	int						clickedCloseCaptionButton = CChoreoChannelWidget::CLOSECAPTION_NONE;

	GetObjectsUnderMouse( mx, my, &a, &c, &e, &ge, &ct, &at, &clickedCloseCaptionButton );
	
	m_pClickedActor			= a;
	m_pClickedChannel		= c;
	m_pClickedEvent			= e;
	m_pClickedGlobalEvent	= ge;
	m_nClickedX				= mx;
	m_nClickedY				= my;
	m_nClickedTag			= ct;
	m_pClickedAbsoluteTag	= at;
	m_nClickedChannelCloseCaptionButton = clickedCloseCaptionButton;

	// Construct main
	mxPopupMenu *pop = new mxPopupMenu();

	if ( a && c )
	{
		if (!e)
		{
			pop->add( "Expression...", IDC_ADDEVENT_EXPRESSION );
			pop->add( "WAV File...", IDC_ADDEVENT_SPEAK );
			pop->add( "Gesture...", IDC_ADDEVENT_GESTURE );
			pop->add( "NULL Gesture...", IDC_ADDEVENT_NULLGESTURE );
			pop->add( "Look at actor...", IDC_ADDEVENT_LOOKAT );
			pop->add( "Move to actor...", IDC_ADDEVENT_MOVETO );
			pop->add( "Face actor...", IDC_ADDEVENT_FACE );
			pop->add( "Fire Trigger...", IDC_ADDEVENT_FIRETRIGGER );
			pop->add( "Generic(AI)...", IDC_ADDEVENT_GENERIC );
			pop->add( "Sequence...", IDC_ADDEVENT_SEQUENCE );
			pop->add( "Flex animation...", IDC_ADDEVENT_FLEXANIMATION );
			pop->add( "Sub-scene...", IDC_ADDEVENT_SUBSCENE );
			pop->add( "Interrupt...", IDC_ADDEVENT_INTERRUPT );
			pop->add( "Permit Responses...", IDC_ADDEVENT_PERMITRESPONSES );

			pop->addSeparator();
		}
		else
		{
			pop->add( va( "Edit Event '%s'...", e->GetEvent()->GetName() ), IDC_EDITEVENT );
			switch ( e->GetEvent()->GetType() )
			{
			default:
				break;
			case CChoreoEvent::FLEXANIMATION:
				{
					pop->add( va( "Edit Event '%s' in expression tool", e->GetEvent()->GetName() ), IDC_EXPRESSIONTOOL );
				}
				break;
			case CChoreoEvent::GESTURE:
				{
					pop->add( va( "Edit Event '%s' in gesture tool", e->GetEvent()->GetName() ), IDC_GESTURETOOL );
				}
				break;
			}

			if ( e->GetEvent()->HasEndTime() )
			{
				pop->add( "Timing Tag...", IDC_ADDTIMINGTAG );
			}

			pop->addSeparator();
		}
	}

	// Construct "New..."
	mxPopupMenu *newMenu = new mxPopupMenu();
	{
		newMenu->add( "Actor...", IDC_ADDACTOR );
		if ( a )
		{
			newMenu->add( "Channel...", IDC_ADDCHANNEL );
		}
		newMenu->add( "Section Pause...", IDC_ADDEVENT_PAUSE );
		newMenu->add( "Loop...", IDC_ADDEVENT_LOOP );
		newMenu->add( "Fire Completion...", IDC_ADDEVENT_STOPPOINT );
	}
	pop->addMenu( "New", newMenu );

	// Now construct "Edit..."
	if ( a || c || e || ge )
	{
		mxPopupMenu	*editMenu = new mxPopupMenu();
		{
			if ( a )
			{
				editMenu->add( va( "Actor '%s'...", a->GetActor()->GetName() ), IDC_EDITACTOR );
			}
			if ( c )
			{
				editMenu->add( va( "Channel '%s'...", c->GetChannel()->GetName() ), IDC_EDITCHANNEL );
			}
			if ( ge )
			{
				switch ( ge->GetEvent()->GetType() )
				{
				default:
					break;
				case CChoreoEvent::SECTION:
					{
						editMenu->add( va( "Section Pause '%s'...", ge->GetEvent()->GetName() ), IDC_EDITGLOBALEVENT );
					}
					break;
				case CChoreoEvent::LOOP:
					{
						editMenu->add( va( "Loop Point '%s'...", ge->GetEvent()->GetName() ), IDC_EDITGLOBALEVENT );
					}
					break;
				case CChoreoEvent::STOPPOINT:
					{
						editMenu->add( va( "Fire Completion '%s'...", ge->GetEvent()->GetName() ), IDC_EDITGLOBALEVENT );
					}
					break;
				}
			}
		}

		pop->addMenu( "Edit", editMenu );

	}

	// Move up/down
	if ( a || c )
	{
		mxPopupMenu	*moveUpMenu = new mxPopupMenu();
		mxPopupMenu	*moveDownMenu = new mxPopupMenu();

		if ( a )
		{
			moveUpMenu->add( va( "Move '%s' up", a->GetActor()->GetName() ), IDC_MOVEACTORUP );
			moveDownMenu->add( va( "Move '%s' down", a->GetActor()->GetName() ), IDC_MOVEACTORDOWN );
		}
		if ( c )
		{
			moveUpMenu->add( va( "Move '%s' up", c->GetChannel()->GetName() ), IDC_MOVECHANNELUP );
			moveDownMenu->add( va( "Move '%s' down", c->GetChannel()->GetName() ), IDC_MOVECHANNELDOWN );
		}

		pop->addMenu( "Move Up", moveUpMenu );
		pop->addMenu( "Move Down", moveDownMenu );
	}

	// Delete
	if ( a || c || e || ge || (ct != -1) )
	{
		mxPopupMenu *deleteMenu = new mxPopupMenu();
		if ( a )
		{
			deleteMenu->add( va( "Actor '%s'", a->GetActor()->GetName() ), IDC_DELETEACTOR );
		}
		if ( c )
		{
			deleteMenu->add( va( "Channel '%s'", c->GetChannel()->GetName() ), IDC_DELETECHANNEL );
		}
		if ( e )
		{
			deleteMenu->add( va( "Event '%s'", e->GetEvent()->GetName() ), IDC_DELETEEVENT );
		}
		if ( ge )
		{
			switch ( ge->GetEvent()->GetType() )
			{
			default:
				break;
			case CChoreoEvent::SECTION:
				{
					deleteMenu->add( va( "Section Pause '%s'...", ge->GetEvent()->GetName() ), IDC_DELETEGLOBALEVENT );
				}
				break;
			case CChoreoEvent::LOOP:
				{
					deleteMenu->add( va( "Loop Point '%s'...", ge->GetEvent()->GetName() ), IDC_DELETEGLOBALEVENT );
				}
				break;
			case CChoreoEvent::STOPPOINT:
				{
					deleteMenu->add( va( "Fire Completion '%s'...", ge->GetEvent()->GetName() ), IDC_DELETEGLOBALEVENT );
				}
				break;
			}
		}
		if ( e && ct != -1 )
		{
			CEventRelativeTag *tag = e->GetEvent()->GetRelativeTag( ct );
			if ( tag )
			{
				deleteMenu->add( va( "Relative Tag '%s'...", tag->GetName() ), IDC_DELETERELATIVETAG );
			}
		}
		pop->addMenu( "Delete", deleteMenu );
	}

	// Select
	{
		mxPopupMenu *selectMenu = new mxPopupMenu();
		selectMenu->add( "Select All", IDC_SELECTALL );
		selectMenu->add( "Deselect All", IDC_DESELECTALL );
		selectMenu->addSeparator();

		selectMenu->add( "All events before", IDC_SELECTEVENTS_ALL_BEFORE );
		selectMenu->add( "All events after", IDC_SELECTEVENTS_ALL_AFTER );
		selectMenu->add( "Active events before", IDC_SELECTEVENTS_ACTIVE_BEFORE );
		selectMenu->add( "Active events after", IDC_SELECTEVENTS_ACTIVE_AFTER );
		selectMenu->add( "Channel events before", IDC_SELECTEVENTS_CHANNEL_BEFORE );
		selectMenu->add( "Channel events after", IDC_SELECTEVENTS_CHANNEL_AFTER );

		if ( a || c )
		{
			selectMenu->addSeparator();
			if ( a )
			{
				selectMenu->add( va( "All events in actor '%s'", a->GetActor()->GetName() ), IDC_CV_ALLEVENTS_ACTOR );
			}
			if ( c )
			{
				selectMenu->add( va( "All events in channel '%s'", c->GetChannel()->GetName() ), IDC_CV_ALLEVENTS_CHANNEL );
			}
		}
		pop->addMenu( "Select/Deselect", selectMenu );
	}

	// Quick delete for events
	if ( e )
	{
		pop->addSeparator();

		switch ( e->GetEvent()->GetType() )
		{
		default:
			break;
		case CChoreoEvent::FLEXANIMATION:
			{
				pop->add( va( "Edit event '%s' in expression tool", e->GetEvent()->GetName() ), IDC_EXPRESSIONTOOL );
			}
			break;
		case CChoreoEvent::GESTURE:
			{
				pop->add( va( "Edit event '%s' in gesture tool", e->GetEvent()->GetName() ), IDC_GESTURETOOL );
			}
			break;
		}

		pop->add( va( "Move event '%s' to back", e->GetEvent()->GetName() ), IDC_MOVETOBACK );
		if ( CountSelectedEvents() > 1 )
		{
			pop->add( va( "Delete events" ), IDC_DELETEEVENT );
			pop->addSeparator();
			pop->add( "Enable events", IDC_CV_ENABLEEVENTS );
			pop->add( "Disable events", IDC_CV_DISABLEEVENTS );
		}
		else
		{
			pop->add( va( "Delete event '%s'", e->GetEvent()->GetName() ), IDC_DELETEEVENT );
			pop->addSeparator();
			if ( e->GetEvent()->GetActive() )
			{
				pop->add( va( "Disable event '%s'", e->GetEvent()->GetName() ), IDC_CV_DISABLEEVENTS );
			}
			else
			{
				pop->add( va( "Enable event '%s'", e->GetEvent()->GetName() ), IDC_CV_ENABLEEVENTS );
			}
		}
	}

	if ( m_rgABPoints[ 0 ].active && m_rgABPoints[ 1 ].active  )
	{
		pop->addSeparator();
		mxPopupMenu *timeMenu = new mxPopupMenu();
		timeMenu->add( "Insert empty space between marks (shifts events right)", IDC_INSERT_TIME );
		timeMenu->add( "Delete events between marks (shifts remaining events left)", IDC_DELETE_TIME );
		pop->addMenu( "Time Marks", timeMenu );
	}


	// Copy/paste
	if ( CanPaste() || e )
	{
		pop->addSeparator();

		if ( CountSelectedEvents() > 1 )
		{
			pop->add( va( "Copy events to clipboard" ), IDC_COPYEVENTS );
		}
		else if ( e )
		{
			pop->add( va( "Copy event '%s' to clipboard", e->GetEvent()->GetName() ), IDC_COPYEVENTS );
		}

		if ( CanPaste() )
		{
			pop->add( va( "Paste events" ), IDC_PASTEEVENTS );
		}
	}

	// Export / import
	pop->addSeparator();

	if ( e )
	{
		mxPopupMenu *exportMenu = new mxPopupMenu();
		if ( CountSelectedEvents() > 1 )
		{
			exportMenu->add( va( "Export events to .vce..." ), IDC_EXPORTEVENTS );
		}
		else if ( e )
		{
			exportMenu->add( va( "Export event '%s' to .vce...", e->GetEvent()->GetName() ), IDC_EXPORTEVENTS );
		}
		exportMenu->add( va( "Export as .vcd..." ), IDC_EXPORT_VCD );
		pop->addMenu( "Export", exportMenu );
	}

	mxPopupMenu *importMenu = new mxPopupMenu();
	importMenu->add( va( "Import events from .vce..." ), IDC_IMPORTEVENTS );
	importMenu->add( va( "Merge from .vcd..." ), IDC_IMPORT_VCD );
	pop->addMenu( "Import", importMenu );

	bool bShowAlignLeft = ( CountSelectedEvents() + CountSelectedGlobalEvents() ) > 1 ? true : false;

	if ( e && ( ( CountSelectedEvents() > 1 ) || bShowAlignLeft ) )
	{
		pop->addSeparator();

		mxPopupMenu *alignMenu = new mxPopupMenu();
		alignMenu->add( "Align Left", IDC_CV_ALIGN_LEFT );
		if ( CountSelectedEvents() > 1 )
		{
			alignMenu->add( "Align Right", IDC_CV_ALIGN_RIGHT );
			alignMenu->add( "Size to Smallest", IDC_CV_SAMESIZE_SMALLEST );
			alignMenu->add( "Size to Largest", IDC_CV_SAMESIZE_LARGEST );
		}
		pop->addMenu( "Align", alignMenu );
	}

	// Misc.
	pop->addSeparator();
	pop->add( va( "Change scale..." ), IDC_CV_CHANGESCALE );
	pop->add( va( "Check sequences" ), IDC_CV_CHECKSEQLENGTHS );
	pop->add( va( "Process sequences" ), IDC_CV_PROCESSSEQUENCES );
	pop->add( va( m_bRampOnly ? "Ramp normal" : "Ramp only" ), IDC_CV_TOGGLERAMPONLY );
	pop->setChecked( IDC_CV_PROCESSSEQUENCES, m_bProcessSequences );

	bool onmaster= ( m_pClickedChannel && 
			m_pClickedChannel->GetCaptionClickedEvent() &&
			m_pClickedChannel->GetCaptionClickedEvent()->GetCloseCaptionType() == CChoreoEvent::CC_MASTER ) ? true : false;
	bool ondisabled = ( m_pClickedChannel && 
			m_pClickedChannel->GetCaptionClickedEvent() &&
			m_pClickedChannel->GetCaptionClickedEvent()->GetCloseCaptionType() == CChoreoEvent::CC_DISABLED ) ? true : false;

	// The close captioning menu
	if ( m_bShowCloseCaptionData && ( AreSelectedEventsCombinable() || AreSelectedEventsInSpeakGroup() || onmaster || ondisabled ) )
	{
		pop->addSeparator();
		if ( AreSelectedEventsCombinable() )
		{
			pop->add( "Combine Speak Events", IDC_CV_COMBINESPEAKEVENTS );
		}
		if ( AreSelectedEventsInSpeakGroup() )
		{
			pop->add( "Uncombine Speak Events", IDC_CV_REMOVESPEAKEVENTFROMGROUP );
		}
		if ( onmaster )
		{
			// Can only change tokens for "combined" files
			if ( m_pClickedChannel->GetCaptionClickedEvent()->GetNumSlaves() >= 1 )
			{
				pop->add( "Change Token", IDC_CV_CHANGECLOSECAPTIONTOKEN );
			}
			pop->add( "Disable captions", IDC_CV_TOGGLECLOSECAPTIONS );
		}
		if ( ondisabled )
		{
			pop->add( "Enable captions", IDC_CV_TOGGLECLOSECAPTIONS );
		}
	}

	// Undo/redo
	if ( CanUndo() || CanRedo() )
	{

		pop->addSeparator();

		if ( CanUndo() )
		{
			pop->add( va( "Undo %s", GetUndoDescription() ), IDC_CVUNDO );
		}
		if ( CanRedo() )
		{
			pop->add( va( "Redo %s", GetRedoDescription() ), IDC_CVREDO );
		}
	}

	if ( m_pScene )
	{
		// Associate map file
		pop->addSeparator();
		pop->add( va( "Associate .bsp (%s)", m_pScene->GetMapname() ), IDC_ASSOCIATEBSP );
		if ( a )
		{
			if ( a->GetActor() && a->GetActor()->GetFacePoserModelName()[0] )
			{
				pop->add( va( "Change .mdl for %s", a->GetActor()->GetName() ), IDC_ASSOCIATEMODEL );
			}
			else
			{
				pop->add( va( "Associate .mdl with %s", a->GetActor()->GetName() ), IDC_ASSOCIATEMODEL );
			}
		}
	}

	pop->popup( this, mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::AssociateModel( void )
{
	if ( !m_pScene )
		return;

	CChoreoActorWidget *actor = m_pClickedActor;
	if ( !actor )
		return;

	CChoreoActor *a = actor->GetActor();
	if ( !a )
		return;

	CChoiceParams params;
	strcpy( params.m_szDialogTitle, "Associate Model" );

	params.m_bPositionDialog = false;
	params.m_nLeft = 0;
	params.m_nTop = 0;
	strcpy( params.m_szPrompt, "Choose model:" );

	params.m_Choices.RemoveAll();

	params.m_nSelected = -1;
	int oldsel = -1;

	int c = models->Count();
	ChoiceText text;
	for ( int i = 0; i < c; i++ )
	{
		char const *modelname = models->GetModelName( i );

		strcpy( text.choice, modelname );

		if ( !stricmp( a->GetName(), modelname ) )
		{
			params.m_nSelected = i;
			oldsel = -1;
		}

		params.m_Choices.AddToTail( text );
	}

	// Add an extra entry which is "No association"
	strcpy( text.choice, "No Associated Model" );
	params.m_Choices.AddToTail( text );

	if ( !ChoiceProperties( &params ) )
		return;
	
	if ( params.m_nSelected == oldsel )
		return;

	// Chose something new...
	if ( params.m_nSelected >= 0 &&
		 params.m_nSelected < params.m_Choices.Count() )
	{
		AssociateModelToActor( a, params.m_nSelected );
	}
	else
	{
		// Chose "No association"
		AssociateModelToActor( a, -1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			modelindex - 
//-----------------------------------------------------------------------------
void CChoreoView::AssociateModelToActor( CChoreoActor *actor, int modelindex )
{
	Assert( actor );

	SetDirty( true );

	PushUndo( "Associate model" );

	// Chose something new...
	if ( modelindex >= 0 &&
		 modelindex < models->Count() )
	{
		actor->SetFacePoserModelName( models->GetModelFileName( modelindex ) );
	}
	else
	{
		// Chose "No Associated Model"
		actor->SetFacePoserModelName( "" );
	}

	RecomputeWaves();

	PushRedo( "Associate model" );
}

void CChoreoView::AssociateBSP( void )
{
	if ( !m_pScene )
		return;

	// Strip game directory and slash
	char mapname[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( mapname, sizeof( mapname ), "maps", "*.bsp" ) )
	{
		return;
	}
	
	m_pScene->SetMapname( mapname );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		RECT rc = m_FocusRects[ i ].m_rcFocus;

		::DrawFocusRect( dc, &rc );
	}

	ReleaseDC( NULL, dc );
}

int CChoreoView::GetSelectedEventWidgets( CUtlVector< CChoreoEventWidget * >& events )
{
	events.RemoveAll();

	int c = 0;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				if ( event->IsSelected() )
				{
					events.AddToTail( event );
					c++;
				}
			}
		}
	}

	return c;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : events - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetSelectedEvents( CUtlVector< CChoreoEvent * >& events )
{
	events.RemoveAll();

	int c = 0;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				if ( event->IsSelected() )
				{
					events.AddToTail( event->GetEvent() );
					c++;
				}
			}
		}
	}

	return c;
}

int CChoreoView::CountSelectedGlobalEvents( void )
{
	int c = 0;
	for ( int i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( !event || !event->IsSelected() )
			continue;

		++c;
	}
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::CountSelectedEvents( void )
{
	int c = 0;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				if ( event->IsSelected() )
					c++;

			}
		}
	}

	return c;
}

bool CChoreoView::IsMouseOverEvent( CChoreoEventWidget *ew, int mx, int my )
{
	int tolerance = DRAG_EVENT_EDGE_TOLERANCE;

	RECT bounds = ew->getBounds();
	mx -= bounds.left;
	my -= bounds.top;

	if ( mx <= -tolerance )
	{
		return false;
	}

	CChoreoEvent *event = ew->GetEvent();
	if ( event )
	{
		if ( event->HasEndTime() )
		{
			int rightside = ew->GetDurationRightEdge() ? ew->GetDurationRightEdge() : ew->w();

			if ( mx > rightside + tolerance )
			{
				return false;
			}
		}
	}

	return true;
}


bool CChoreoView::IsMouseOverEventEdge( CChoreoEventWidget *ew, bool bLeftEdge, int mx, int my )
{
	int tolerance = DRAG_EVENT_EDGE_TOLERANCE;

	RECT bounds = ew->getBounds();
	mx -= bounds.left;
	my -= bounds.top;

	CChoreoEvent *event = ew->GetEvent();
	if ( event && event->HasEndTime() )
	{
		if ( mx > -tolerance && mx <= tolerance )
		{
			return bLeftEdge;
		}

		int rightside = ew->GetDurationRightEdge() ? ew->GetDurationRightEdge() : ew->w();

		if ( mx >= rightside - tolerance )
		{
			if ( mx > rightside + tolerance )
			{
				return false;
			}
			else
			{
				return !bLeftEdge;
			}
		}
	}

	return false;
}

int CChoreoView::GetEarliestEventIndex( CUtlVector< CChoreoEventWidget * >& events )
{
	int best = -1;
	float minTime = FLT_MAX;

	int c = events.Count();
	for ( int i = 0; i < c; ++i )
	{
		CChoreoEvent *e = events[ i ]->GetEvent();
		float t = e->GetStartTime();
		if ( t < minTime )
		{
			minTime = t;
			best = i;
		}
	}

	return best;
}

int	 CChoreoView::GetLatestEventIndex( CUtlVector< CChoreoEventWidget * >& events )
{
	int best = -1;
	float maxTime = FLT_MIN;

	int c = events.Count();
	for ( int i = 0; i < c; ++i )
	{
		CChoreoEvent *e = events[ i ]->GetEvent();
		float t = e->GetEndTime();
		if ( t > maxTime )
		{
			maxTime = t;
			best = i;
		}
	}

	return best;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::ComputeEventDragType( int mx, int my )
{
	int tolerance = DRAG_EVENT_EDGE_TOLERANCE;

	// Iterate the events and see who's closest
	CChoreoEventWidget *ew = GetEventUnderCursorPos( mx, my );
	if ( !ew )
	{
		return DRAGTYPE_NONE;
	}

	// Deal with small windows by lowering tolerance
	if ( ew->w() < 4 * tolerance )
	{
		tolerance = 2;
	}

	int tagnum = GetTagUnderCursorPos( ew, mx, my );
	if ( tagnum != -1 && CountSelectedEvents() <= 1 )
	{
		return DRAGTYPE_EVENTTAG_MOVE;
	}

	CEventAbsoluteTag *tag = GetAbsoluteTagUnderCursorPos( ew, mx, my );
	if ( tag != NULL && CountSelectedEvents() <= 1 )
	{
		return DRAGTYPE_EVENTABSTAG_MOVE;
	}

	if ( CountSelectedEvents() > 1 )
	{
		CUtlVector< CChoreoEventWidget * > events;
		GetSelectedEventWidgets( events );

		int iStart, iEnd;
		iStart = GetEarliestEventIndex( events );
		iEnd = GetLatestEventIndex( events );

		if ( events.IsValidIndex( iStart ) )
		{
			// See if mouse is over left edge of starting event
			if ( IsMouseOverEventEdge( events[ iStart ], true, mx, my ) )
			{
				return DRAGTYPE_RESCALELEFT;
			}
		}
		if ( events.IsValidIndex( iEnd ) )
		{
			if ( IsMouseOverEventEdge( events[ iEnd ], false, mx, my ) )
			{
				return DRAGTYPE_RESCALERIGHT;
			}
		}

		return DRAGTYPE_EVENT_MOVE;
	}

	CChoreoEvent *event = ew->GetEvent();
	if ( event )
	{
		if ( event->IsFixedLength() || !event->HasEndTime() )
		{
			return DRAGTYPE_EVENT_MOVE;
		}
	}

	if ( IsMouseOverEventEdge( ew, true, mx, my ) )
	{
		if ( GetAsyncKeyState( VK_SHIFT ) )
			return DRAGTYPE_EVENT_STARTTIME_RESCALE;
		return DRAGTYPE_EVENT_STARTTIME;
	}

	if ( IsMouseOverEventEdge( ew, false, mx, my ) )
	{
		if ( GetAsyncKeyState( VK_SHIFT ) )
			return DRAGTYPE_EVENT_ENDTIME_RESCALE;
		return DRAGTYPE_EVENT_ENDTIME;
	}

	if ( IsMouseOverEvent( ew, mx, my ) )
	{
		return DRAGTYPE_EVENT_MOVE;
	}

	return DRAGTYPE_NONE;
}

void CChoreoView::StartDraggingSceneEndTime( int mx, int my )
{
	m_nDragType = DRAGTYPE_SCENE_ENDTIME;

	m_FocusRects.Purge();

	RECT rcFocus;
	rcFocus.left = mx;
	rcFocus.top = 0;
	rcFocus.bottom = h2();
	rcFocus.right = rcFocus.left + 2;

	POINT offset;
	offset.x = 0;
	offset.y = 0;
	ClientToScreen( (HWND)getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	CFocusRect fr;
	fr.m_rcFocus = rcFocus;
	fr.m_rcOrig = rcFocus;

	// Relative tag events don't move
	m_FocusRects.AddToTail( fr );

	m_xStart = mx;
	m_yStart = my;
	m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );

	DrawFocusRect();

	m_bDragging = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::StartDraggingEvent( int mx, int my )
{
	m_nDragType = ComputeEventDragType( mx, my );
	if ( m_nDragType == DRAGTYPE_NONE )
	{
		if( m_pClickedGlobalEvent )
		{
			m_nDragType = DRAGTYPE_EVENT_MOVE;
		}
		else
		{
			return;
		}
	}

	m_FocusRects.Purge();

	// Go through all selected events
	RECT rcFocus;
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				if ( !event->IsSelected() )
					continue;

				if ( event == m_pClickedEvent && 
					( m_nClickedTag != -1 || m_pClickedAbsoluteTag ) )
				{
					int leftEdge = 0;
					int tagWidth = 1;
					if ( !m_pClickedAbsoluteTag )
					{
						CEventRelativeTag *tag = event->GetEvent()->GetRelativeTag( m_nClickedTag );
						if ( tag )
						{
							// Determine left edcge
							RECT bounds;
							bounds = event->getBounds();
							if ( bounds.right - bounds.left > 0 )
							{
								leftEdge = (int)( tag->GetPercentage() * (float)( bounds.right - bounds.left ) + 0.5f );
							}
						}
					}
					else
					{
						// Determine left edcge
						RECT bounds;
						bounds = event->getBounds();
						if ( bounds.right - bounds.left > 0 )
						{
							leftEdge = (int)( m_pClickedAbsoluteTag->GetPercentage() * (float)( bounds.right - bounds.left ) + 0.5f );
						}
					}

					rcFocus.left = event->x() + leftEdge - tagWidth;
					rcFocus.top = event->y() - tagWidth;
					rcFocus.right = rcFocus.left + 2 * tagWidth;
					rcFocus.bottom = event->y() + event->h();
				}
				else
				{
					rcFocus.left = event->x();
					rcFocus.top = event->y();
					if ( event->GetDurationRightEdge() )
					{
						rcFocus.right = event->x() + event->GetDurationRightEdge();
					}
					else
					{
						rcFocus.right = rcFocus.left + event->w();
					}
					rcFocus.bottom = rcFocus.top + event->h();
				}

				POINT offset;
				offset.x = 0;
				offset.y = 0;
				ClientToScreen( (HWND)getHandle(), &offset );
				OffsetRect( &rcFocus, offset.x, offset.y );

				CFocusRect fr;
				fr.m_rcFocus = rcFocus;
				fr.m_rcOrig = rcFocus;

				// Relative tag events don't move
				m_FocusRects.AddToTail( fr );
			}
		}
	}

	for ( int i = 0; i < m_SceneGlobalEvents.Count(); i++ )
	{
		CChoreoGlobalEventWidget *gew = m_SceneGlobalEvents[ i ];
		if ( !gew )
			continue;

		if ( !gew->IsSelected() )
			continue;
	
		rcFocus.left = gew->x() + gew->w() / 2;
		rcFocus.top = 0;
		rcFocus.right = rcFocus.left + 2;
		rcFocus.bottom = h2();

		POINT offset;
		offset.x = 0;
		offset.y = 0;
		ClientToScreen( (HWND)getHandle(), &offset );
		OffsetRect( &rcFocus, offset.x, offset.y );

		CFocusRect fr;
		fr.m_rcFocus = rcFocus;
		fr.m_rcOrig = rcFocus;

		m_FocusRects.AddToTail( fr );
	}

	m_xStart = mx;
	m_yStart = my;
	m_hPrevCursor = NULL;
	switch ( m_nDragType )
	{
	default:
		break;
	case DRAGTYPE_EVENTTAG_MOVE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	case DRAGTYPE_EVENTABSTAG_MOVE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_IBEAM ) );
		break;
	case DRAGTYPE_EVENT_MOVE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
		break;
	case DRAGTYPE_EVENT_STARTTIME:
	case DRAGTYPE_EVENT_STARTTIME_RESCALE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	case DRAGTYPE_EVENT_ENDTIME:
	case DRAGTYPE_EVENT_ENDTIME_RESCALE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	case DRAGTYPE_RESCALELEFT:
	case DRAGTYPE_RESCALERIGHT:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	}

	DrawFocusRect();

	m_bDragging = true;
}

bool CChoreoView::IsMouseOverSceneEndTime( int mx )
{
	// See if mouse if over scene end time instead
	if ( m_pScene )
	{
		float endtime = m_pScene->FindStopTime();

		bool clip = false;
		int lastpixel = GetPixelForTimeValue( endtime, &clip );
		if ( !clip )
		{
			if ( abs( mx - lastpixel ) < DRAG_EVENT_EDGE_TOLERANCE )
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::MouseStartDrag( mxEvent *event, int mx, int my )
{
	bool isrightbutton = event->buttons & mxEvent::MouseRightButton ? true : false;

	if ( m_bDragging )
	{
		return;
	}

	GetObjectsUnderMouse( mx, my, &m_pClickedActor, &m_pClickedChannel, &m_pClickedEvent, &m_pClickedGlobalEvent, &m_nClickedTag, &m_pClickedAbsoluteTag, &m_nClickedChannelCloseCaptionButton );

	if ( m_pClickedEvent )
	{
		CChoreoEvent *e = m_pClickedEvent->GetEvent();
		Assert( e );

		int dtPreview = ComputeEventDragType( mx, my );
		// Shift clicking on exact edge shouldn't toggle selection state
		bool bIsEdgeRescale = ( dtPreview == DRAGTYPE_EVENT_ENDTIME_RESCALE || dtPreview == DRAGTYPE_EVENT_STARTTIME_RESCALE );

		if ( !( event->modifiers & ( mxEvent::KeyCtrl | mxEvent::KeyShift ) ) )
		{
			if ( !m_pClickedEvent->IsSelected() )
			{
				DeselectAll();
			}
			TraverseWidgets( &CChoreoView::Select, m_pClickedEvent );
		}
		else if ( !bIsEdgeRescale )
		{
			m_pClickedEvent->SetSelected( !m_pClickedEvent->IsSelected() );
		}

		switch ( m_pClickedEvent->GetEvent()->GetType() )
		{
		default:
			break;
		case CChoreoEvent::FLEXANIMATION:
			{
				g_pExpressionTool->SetEvent( e );
				g_pFlexPanel->SetEvent( e );
			}
			break;
		case CChoreoEvent::GESTURE:
			{
				g_pGestureTool->SetEvent( e );
			}
			break;
		case CChoreoEvent::SPEAK:
			{
				g_pWaveBrowser->SetEvent( e );
			}
			break;
		}

		if ( e->HasEndTime() )
		{
			g_pRampTool->SetEvent( e );
		}

		redraw();
		StartDraggingEvent( mx, my );
	}
	else if ( m_pClickedGlobalEvent )
	{
		if ( !( event->modifiers & ( mxEvent::KeyCtrl | mxEvent::KeyShift ) ) )
		{
			if ( !m_pClickedGlobalEvent->IsSelected() )
			{
				DeselectAll();
			}
			TraverseWidgets( &CChoreoView::Select, m_pClickedGlobalEvent );
		}
		else
		{
			m_pClickedGlobalEvent->SetSelected( !m_pClickedGlobalEvent->IsSelected() );
		}

		redraw();
		StartDraggingEvent( mx, my );
	}
	else if ( IsMouseOverScrubArea( event ) )
	{
		if ( IsMouseOverScrubHandle( event ) )
		{
			m_nDragType = DRAGTYPE_SCRUBBER;

			m_bDragging = true;
			
			float t = GetTimeValueForMouse( (short)event->x );
			m_flScrubberTimeOffset = m_flScrub - t;
			float maxoffset = 0.5f * (float)SCRUBBER_HANDLE_WIDTH / GetPixelsPerSecond();
			m_flScrubberTimeOffset = clamp( m_flScrubberTimeOffset, -maxoffset, maxoffset );
			t += m_flScrubberTimeOffset;

			ClampTimeToSelectionInterval( t );

			SetScrubTime( t );
			SetScrubTargetTime( t );

			redraw();

			RECT rcScrub;
			GetScrubHandleRect( rcScrub, true );

			m_FocusRects.Purge();

			// Go through all selected events
			RECT rcFocus;

			rcFocus.top = GetStartRow();
			rcFocus.bottom = h2() - m_nScrollbarHeight - m_nInfoHeight;
			rcFocus.left = ( rcScrub.left + rcScrub.right ) / 2;
			rcFocus.right = rcFocus.left;

			POINT pt;
			pt.x = pt.y = 0;
			ClientToScreen( (HWND)getHandle(), &pt );

			OffsetRect( &rcFocus, pt.x, pt.y );

			CFocusRect fr;
			fr.m_rcFocus = rcFocus;
			fr.m_rcOrig = rcFocus;

			m_FocusRects.AddToTail( fr );

			m_xStart = mx;
			m_yStart = my;

			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );

			DrawFocusRect();
		}
		else
		{
			float t = GetTimeValueForMouse( mx );

			ClampTimeToSelectionInterval( t );

			SetScrubTargetTime( t );

			// Unpause the scene
			m_bPaused = false;
			redraw();
		}
	}
	else if ( IsMouseOverSceneEndTime( mx ) )
	{
		redraw();
		StartDraggingSceneEndTime( mx, my );
	}
	else  if ( m_pClickedChannel && 
		m_nClickedChannelCloseCaptionButton != CChoreoChannelWidget::CLOSECAPTION_NONE &&
		m_nClickedChannelCloseCaptionButton != CChoreoChannelWidget::CLOSECAPTION_CAPTION )
	{
		switch ( m_nClickedChannelCloseCaptionButton )
		{
		default:
		case CChoreoChannelWidget::CLOSECAPTION_EXPANDCOLLAPSE:
			{
				OnToggleCloseCaptionTags();
			}
			break;
		case CChoreoChannelWidget::CLOSECAPTION_PREVLANGUAGE:
			{
				// Change language
				int id = GetCloseCaptionLanguageId();
				--id;
				if ( id < 0 )
				{
					id = CC_NUM_LANGUAGES  - 1;
					Assert( id >= 0 );
				}
				SetCloseCaptionLanguageId( id );
				redraw();
			}
			break;
		case CChoreoChannelWidget::CLOSECAPTION_NEXTLANGUAGE:
			{
				int id = GetCloseCaptionLanguageId();
				++id;
				if ( id >= CC_NUM_LANGUAGES )
				{
					id = 0;
				}
				SetCloseCaptionLanguageId( id );
				redraw();
			}
			break;
		case CChoreoChannelWidget::CLOSECAPTION_SELECTOR:
			{
				SetDirty( true );

				PushUndo( "Change selector" );

				m_pClickedChannel->HandleSelectorClicked();

				PushRedo( "Change selector" );

				redraw();
			}
			break;
		}
	}
	else
	{
		if ( !( event->modifiers & ( mxEvent::KeyCtrl | mxEvent::KeyShift ) ) )
		{
			DeselectAll();

			if ( !isrightbutton )
			{
				if ( realtime - m_flLastMouseClickTime < 0.3f )
				{
					OnDoubleClicked();
					m_flLastMouseClickTime = -1.0f;
				}
				else
				{
					m_flLastMouseClickTime = realtime;
				}
			}

			redraw();
		}
	}

	CalcBounds( m_nDragType );
}

void CChoreoView::OnDoubleClicked()
{
	if ( m_pClickedChannel )
	{
		switch (m_nClickedChannelCloseCaptionButton )
		{
		default:
			break;
		case CChoreoChannelWidget::CLOSECAPTION_NONE:
			{
				SetDirty( true );
				PushUndo( "Enable/disable Channel" );
		
				m_pClickedChannel->GetChannel()->SetActive( !m_pClickedChannel->GetChannel()->GetActive() );
		
				PushRedo( "Enable/disable Channel" );
			}
			break;
		case CChoreoChannelWidget::CLOSECAPTION_CAPTION:
			{
				CChoreoEvent *e = m_pClickedChannel->GetCaptionClickedEvent();
				if ( e && e->GetNumSlaves() >= 1 )
				{
					OnChangeCloseCaptionToken( e );
				}
			}
			break;
		}

		return;
	}

	if ( m_pClickedActor )
	{
		SetDirty( true );
		PushUndo( "Enable/disable Actor" );

		m_pClickedActor->GetActor()->SetActive( !m_pClickedActor->GetActor()->GetActive() );

		PushRedo( "Enable/disable Actor" );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::MouseContinueDrag( mxEvent *event, int mx, int my )
{
	if ( !m_bDragging )
		return;

	DrawFocusRect();

	ApplyBounds( mx, my );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		CFocusRect *f = &m_FocusRects[ i ];
		f->m_rcFocus = f->m_rcOrig;

		switch ( m_nDragType )
		{
		default:
		case DRAGTYPE_SCRUBBER:
			{
				float t = GetTimeValueForMouse( mx );
				t += m_flScrubberTimeOffset;

				ClampTimeToSelectionInterval( t );

				float dt = t - m_flScrub;

				SetScrubTargetTime( t );

				m_bSimulating = true;
				ScrubThink( dt, true, this );

				SetScrubTime( t );

				OffsetRect( &f->m_rcFocus, ( mx - m_xStart ), 0 );
			}
			break;
		case DRAGTYPE_EVENT_MOVE:
		case DRAGTYPE_EVENTTAG_MOVE:
		case DRAGTYPE_EVENTABSTAG_MOVE:
			{
				int dx = mx - m_xStart;
				int dy = my - m_yStart;
				if ( m_pClickedEvent )
				{
					bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;
					

					// Only allow jumping channels if shift is down
					if ( !shiftdown )
					{
						dy = 0;
					}
					if ( abs( dy ) < m_pClickedEvent->GetItemHeight() )
					{
						dy = 0;
					}
					if ( m_nSelectedEvents > 1 )
					{
						dy = 0;
					}
					if ( m_nDragType == DRAGTYPE_EVENTTAG_MOVE || m_nDragType == DRAGTYPE_EVENTABSTAG_MOVE )
					{
						dy = 0;
					}
					
					if ( m_pClickedEvent->GetEvent()->IsUsingRelativeTag() )
					{
						dx = 0;
					}
				}
				else
				{
					dy = 0;
				}
				OffsetRect( &f->m_rcFocus, dx, dy );
			}
			break;
		case DRAGTYPE_EVENT_STARTTIME:
		case DRAGTYPE_EVENT_STARTTIME_RESCALE:
			f->m_rcFocus.left += ( mx - m_xStart );
			break;
		case DRAGTYPE_EVENT_ENDTIME:
		case DRAGTYPE_EVENT_ENDTIME_RESCALE:
			f->m_rcFocus.right += ( mx - m_xStart );
			break;
		case DRAGTYPE_SCENE_ENDTIME:
			OffsetRect( &f->m_rcFocus, ( mx - m_xStart ), 0 );
			break;
		case DRAGTYPE_RESCALELEFT:
		case DRAGTYPE_RESCALERIGHT:
			//f->m_rcFocus.right += ( mx - m_xStart );
			break;
		}
	}

	if ( m_nDragType == DRAGTYPE_RESCALELEFT || 
		 m_nDragType == DRAGTYPE_RESCALERIGHT )
	{
		int c = m_FocusRects.Count();
		int m_nStart = INT_MAX;
		int m_nEnd = INT_MIN;

		for ( int i = 0; i < c; ++i )
		{
			CFocusRect *f = &m_FocusRects[ i ];
			if ( f->m_rcFocus.left < m_nStart )
			{
				m_nStart = f->m_rcFocus.left;
			}
			if ( f->m_rcFocus.right > m_nEnd )
			{
				m_nEnd = f->m_rcFocus.right;
			}
		}

		// Now figure out rescaling logic
		int dxPixels = mx - m_xStart;

		int oldSize = m_nEnd - m_nStart;
		if ( oldSize > 0 )
		{
			float rescale = 1.0f;
			if ( m_nDragType == DRAGTYPE_RESCALERIGHT )
			{
				rescale = (float)( oldSize + dxPixels )/(float)oldSize;
			}
			else
			{
				rescale = (float)( oldSize - dxPixels )/(float)oldSize;
			}

			for ( int i = 0; i < c; ++i )
			{
				CFocusRect *f = &m_FocusRects[ i ];
				int w = f->m_rcFocus.right - f->m_rcFocus.left;
				if ( m_nDragType == DRAGTYPE_RESCALERIGHT )
				{
					f->m_rcFocus.left = m_nStart + ( int )( rescale * (float)( f->m_rcFocus.left - m_nStart ) + 0.5f );
					f->m_rcFocus.right = f->m_rcFocus.left + ( int )( rescale * (float)w + 0.5f );
				}
				else
				{
					f->m_rcFocus.right = m_nEnd - ( int )( rescale * (float)( m_nEnd - f->m_rcFocus.right ) + 0.5f );
					f->m_rcFocus.left = f->m_rcFocus.right - ( int )( rescale * (float)w + 0.5f );
				}
			}
		}
	}
	DrawFocusRect();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::MouseMove( int mx, int my )
{
	if ( m_bDragging )
		return;

	int dragtype = ComputeEventDragType( mx, my );
	if ( dragtype == DRAGTYPE_NONE )
	{
		CChoreoGlobalEventWidget *ge = NULL;
		GetObjectsUnderMouse( mx, my, NULL, NULL, NULL, &ge, NULL, NULL, NULL );
		if ( ge )
		{
			dragtype = DRAGTYPE_EVENT_MOVE;
		}

		if ( dragtype == DRAGTYPE_NONE )
		{
			if ( IsMouseOverSceneEndTime( mx ) )
			{
				dragtype = DRAGTYPE_SCENE_ENDTIME;
			}
		}
	}

	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = NULL;
	}
	switch ( dragtype )
	{
	default:
		break;
	case DRAGTYPE_EVENTTAG_MOVE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	case DRAGTYPE_EVENTABSTAG_MOVE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_IBEAM ) );
		break;
	case DRAGTYPE_EVENT_MOVE:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
		break;
	case DRAGTYPE_EVENT_STARTTIME:
	case DRAGTYPE_EVENT_STARTTIME_RESCALE:
	case DRAGTYPE_EVENT_ENDTIME:
	case DRAGTYPE_EVENT_ENDTIME_RESCALE:
	case DRAGTYPE_SCENE_ENDTIME:
	case DRAGTYPE_RESCALELEFT:
	case DRAGTYPE_RESCALERIGHT:
		m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *e - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::CheckGestureLength( CChoreoEvent *e, bool bCheckOnly )
{
	Assert( e );
	if ( !e )
		return false;

	if ( e->GetType() != CChoreoEvent::GESTURE )
	{
		Con_Printf( "CheckGestureLength:  called on non-GESTURE event %s\n", e->GetName() );
		return false;
	}

	StudioModel *model = FindAssociatedModel( e->GetScene(), e->GetActor()  );
	if ( !model )
		return false;

	CStudioHdr *pStudioHdr = model->GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	return UpdateGestureLength( e, pStudioHdr, model->GetPoseParameters(), bCheckOnly );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *e - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::DefaultGestureLength( CChoreoEvent *e, bool bCheckOnly )
{
	Assert( e );
	if ( !e )
		return false;

	if ( e->GetType() != CChoreoEvent::GESTURE )
	{
		Con_Printf( "DefaultGestureLength:  called on non-GESTURE event %s\n", e->GetName() );
		return false;
	}

	StudioModel *model = FindAssociatedModel( e->GetScene(), e->GetActor()  );
	if ( !model )
		return false;

	if ( !model->GetStudioHdr() )
		return false;

	int iSequence = model->LookupSequence( e->GetParameters() );
	if ( iSequence < 0 )
		return false;

	bool bret = false;

	float seqduration = model->GetDuration( iSequence );
	if ( seqduration != 0.0f )
	{
		bret = true;
		if ( !bCheckOnly )
		{
			e->SetEndTime( e->GetStartTime() + seqduration );
		}
	}

	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *e - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::AutoaddGestureKeys( CChoreoEvent *e, bool bCheckOnly )
{
	if ( !e )
		return false;

	StudioModel *model = FindAssociatedModel( e->GetScene(), e->GetActor() );
	if ( !model )
		return false;

	CStudioHdr *pStudioHdr = model->GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	return AutoAddGestureKeys( e, pStudioHdr, model->GetPoseParameters(), bCheckOnly );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChoreoView::CheckSequenceLength( CChoreoEvent *e, bool bCheckOnly )
{
	Assert( e );
	if ( !e )
		return false;

	if ( e->GetType() != CChoreoEvent::SEQUENCE )
	{
		Con_Printf( "CheckSequenceLength:  called on non-SEQUENCE event %s\n", e->GetName() );
		return false;
	}

	StudioModel *model = FindAssociatedModel( e->GetScene(), e->GetActor() );
	if ( !model )
		return false;

	CStudioHdr *pStudioHdr = model->GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	return UpdateSequenceLength( e, pStudioHdr, model->GetPoseParameters(), bCheckOnly, true );
}

void CChoreoView::FinishDraggingSceneEndTime( mxEvent *event, int mx, int my )
{
	DrawFocusRect();

	m_FocusRects.Purge();

	m_bDragging = false;

	float mouse_dt = GetTimeDeltaForMouseDelta( mx, m_xStart );
	if ( !mouse_dt )
	{
		return;
	}

	SetDirty( true );

	const char *desc = "Change Scene Duration";

	PushUndo( desc );

	float newendtime = GetTimeValueForMouse( mx );
	float oldendtime = m_pScene->FindStopTime();

	float scene_dt = newendtime - oldendtime;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = a->GetChannel( j );
			if ( !channel )
				continue;

			int k;

			CChoreoEvent *finalGesture = NULL;
			for ( k = channel->GetNumEvents() - 1; k >= 0; k-- )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				CChoreoEvent *e = event->GetEvent();
				if ( e->GetType() != CChoreoEvent::GESTURE )
					continue;

				if ( !finalGesture )
				{
					finalGesture = e;
				}
				else
				{
					if ( e->GetStartTime() > finalGesture->GetStartTime() )
					{
						finalGesture = e;
					}
				}
			}


			for ( k = channel->GetNumEvents() - 1; k >= 0; k-- )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				CChoreoEvent *e = event->GetEvent();

				// Event starts after new end time, kill it
				if ( e->GetStartTime() > newendtime )
				{
					channel->GetChannel()->RemoveEvent( e );
					m_pScene->DeleteReferencedObjects( e );
					continue;
				}

				// No change to normal events that end earlier than new time (but do change gestures)
				if ( e->GetEndTime() < newendtime && 
					e != finalGesture )
				{
					continue;
				}

				float dt = scene_dt;
				if ( e->GetType() == CChoreoEvent::GESTURE )
				{
					if ( e->GetEndTime() < newendtime )
					{
						dt = newendtime - e->GetEndTime();
					}
				}

				float newduration = e->GetDuration() + dt;
				RescaleRamp( e, newduration );
				switch ( e->GetType() )
				{
				default:
					break;
				case CChoreoEvent::GESTURE:
					{
						e->RescaleGestureTimes( e->GetStartTime(), e->GetEndTime() + dt, true );
					}
					break;
				case CChoreoEvent::FLEXANIMATION:
					{
						RescaleExpressionTimes( e, e->GetStartTime(), e->GetEndTime() + dt );
					}
					break;
				}
				e->OffsetEndTime( dt );
				e->SnapTimes();
				e->ResortRamp();
			}
		}
	}

	// Remove event and move to new object
	DeleteSceneWidgets();

	m_nDragType = DRAGTYPE_NONE;

	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = 0;
	}

	PushRedo( desc );

	CreateSceneWidgets();

	InvalidateLayout();

	g_pExpressionTool->LayoutItems( true );
	g_pExpressionTool->redraw();
	g_pGestureTool->redraw();
	g_pRampTool->redraw();
	g_pSceneRampTool->redraw();
}

//-----------------------------------------------------------------------------
// Purpose: Called after association changes to reset .wav file images
// Input  :  - 
//-----------------------------------------------------------------------------
void CChoreoView::RecomputeWaves()
{
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				e->RecomputeWave();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::FinishDraggingEvent( mxEvent *event, int mx, int my )
{
	DrawFocusRect();

	m_FocusRects.Purge();

	m_bDragging = false;

	float dt = GetTimeDeltaForMouseDelta( mx, m_xStart );
	if ( !dt )
	{
		if ( m_pScene && m_pClickedEvent && m_pClickedEvent->GetEvent()->GetType() == CChoreoEvent::SPEAK )
		{
			// Show phone wav in wav viewer
			char sndname[ 512 ];
			Q_strncpy( sndname, FacePoser_TranslateSoundName( m_pClickedEvent->GetEvent() ), sizeof( sndname ) );
			if ( sndname[ 0 ] )
			{
				SetCurrentWaveFile( va( "sound/%s", sndname ), m_pClickedEvent->GetEvent() );
			}
			else
			{
				Warning( "Unable to resolve sound name for '%s', check actor associations\n", m_pClickedEvent->GetEvent()->GetName() );
			}
		}
		return;
	}

	SetDirty( true );

	char const *desc = "";

	switch ( m_nDragType )
	{
		default:
		case DRAGTYPE_EVENT_MOVE:
			desc = "Event Move";
			break;
		case DRAGTYPE_EVENT_STARTTIME:
		case DRAGTYPE_EVENT_STARTTIME_RESCALE:
			desc = "Change Start Time";
			break;
		case DRAGTYPE_EVENT_ENDTIME:
		case DRAGTYPE_EVENT_ENDTIME_RESCALE:
			desc = "Change End Time";
			break;
		case DRAGTYPE_EVENTTAG_MOVE:
			desc = "Move Event Tag";
			break;
		case DRAGTYPE_EVENTABSTAG_MOVE:
			desc = "Move Abs Event Tag";
			break;
		case DRAGTYPE_RESCALELEFT:
		case DRAGTYPE_RESCALERIGHT:
			desc = "Rescale Time";
			break;
	}
	PushUndo( desc );

	CUtlVector< CChoreoEvent * > rescaleHelper;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				if ( !event->IsSelected() )
					continue;

				// Figure out true dt
				CChoreoEvent *e = event->GetEvent();
				if ( e )
				{
					switch ( m_nDragType )
					{
					default:
					case DRAGTYPE_EVENT_MOVE:
						e->OffsetTime( dt );
						e->SnapTimes();
						break;
					case DRAGTYPE_EVENT_STARTTIME:
					case DRAGTYPE_EVENT_STARTTIME_RESCALE:
						{
							float newduration = e->GetDuration() - dt;
							RescaleRamp( e, newduration );
							switch ( e->GetType() )
							{
							default:
								break;
							case CChoreoEvent::GESTURE:
								{
									e->RescaleGestureTimes( e->GetStartTime() + dt, e->GetEndTime(), m_nDragType == DRAGTYPE_EVENT_STARTTIME );
								}
								break;
							case CChoreoEvent::FLEXANIMATION:
								{
									RescaleExpressionTimes( e, e->GetStartTime() + dt, e->GetEndTime() );
								}
								break;
							}
							e->OffsetStartTime( dt );
							e->SnapTimes();
							e->ResortRamp();
						}
						break;
					case DRAGTYPE_EVENT_ENDTIME:
					case DRAGTYPE_EVENT_ENDTIME_RESCALE:
						{
							float newduration = e->GetDuration() + dt;
							RescaleRamp( e, newduration );
							switch ( e->GetType() )
							{
							default:
								break;
							case CChoreoEvent::GESTURE:
								{
									e->RescaleGestureTimes( e->GetStartTime(), e->GetEndTime() + dt, m_nDragType == DRAGTYPE_EVENT_ENDTIME );
								}
								break;
							case CChoreoEvent::FLEXANIMATION:
								{
									RescaleExpressionTimes( e, e->GetStartTime(), e->GetEndTime() + dt );
								}
								break;
							}
							e->OffsetEndTime( dt );
							e->SnapTimes();
							e->ResortRamp();
						}
						break;
					case DRAGTYPE_RESCALELEFT:
					case DRAGTYPE_RESCALERIGHT:
						{
							rescaleHelper.AddToTail( e );
						}
						break;
					case DRAGTYPE_EVENTTAG_MOVE:
						{
							// Get current x position
							if ( m_nClickedTag != -1 )
							{
								CEventRelativeTag *tag = e->GetRelativeTag( m_nClickedTag );
								if ( tag )
								{
									float dx = mx - m_xStart;
									// Determine left edcge
									RECT bounds;
									bounds = event->getBounds();
									if ( bounds.right - bounds.left > 0 )
									{
										int left = bounds.left + (int)( tag->GetPercentage() * (float)( bounds.right - bounds.left ) + 0.5f );

										left += dx;

										if ( left < bounds.left )
										{
											left = bounds.left;
										}
										else if ( left >= bounds.right )
										{
											left = bounds.right - 1;
										}

										// Now convert back to a percentage
										float frac = (float)( left - bounds.left ) / (float)( bounds.right - bounds.left );

										tag->SetPercentage( frac );
									}
								}
							}
						}
						break;
					case DRAGTYPE_EVENTABSTAG_MOVE:
						{
							// Get current x position
							if ( m_pClickedAbsoluteTag != NULL )
							{
								CEventAbsoluteTag *tag = m_pClickedAbsoluteTag;
								if ( tag )
								{
									float dx = mx - m_xStart;
									// Determine left edcge
									RECT bounds;
									bounds = event->getBounds();
									if ( bounds.right - bounds.left > 0 )
									{
										int left = bounds.left + (int)( tag->GetPercentage() * (float)( bounds.right - bounds.left ) + 0.5f );

										left += dx;

										if ( left < bounds.left )
										{
											left = bounds.left;
										}
										else if ( left >= bounds.right )
										{
											left = bounds.right - 1;
										}

										// Now convert back to a percentage
										float frac = (float)( left - bounds.left ) / (float)( bounds.right - bounds.left );

										tag->SetPercentage( frac );
									}
								}
							}
						}
						break;
					}
				}

				switch ( e->GetType() )
				{
				default:
					break;
				case CChoreoEvent::SPEAK:
					{
						// Try and load wav to get length
						CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( e ) ) );
						if ( wave )
						{
							e->SetEndTime( e->GetStartTime() + wave->GetRunningLength() );
							delete wave;
						}
					}
					break;
				case CChoreoEvent::SEQUENCE:
					{
						CheckSequenceLength( e, false );
					}
					break;
				case CChoreoEvent::GESTURE:
					{
						CheckGestureLength( e, false );
					}
					break;
				}
			}
		}
	}

	if ( rescaleHelper.Count() > 0 )
	{
		int i;
		// Determine start and end times for existing "selection"
		float flStart = FLT_MAX;
		float flEnd = FLT_MIN;
		for ( i = 0; i < rescaleHelper.Count(); ++i )
		{
			CChoreoEvent *e = rescaleHelper[ i ];
			float st = e->GetStartTime();
			float ed = e->GetEndTime();

			if ( st < flStart )
			{
				flStart = st;
			}
			if ( ed > flEnd )
			{
				flEnd = ed;
			}
		}

		float flSelectionDuration = flEnd - flStart;
		if ( flSelectionDuration > 0.0f )
		{
			float flNewDuration = 0.0f;
			if ( m_nDragType == DRAGTYPE_RESCALELEFT )
			{
				flNewDuration = max( 0.1f, flSelectionDuration - dt );
			}
			else
			{
				flNewDuration = max( 0.1f, flSelectionDuration + dt );
			}
			float flScale = flNewDuration / flSelectionDuration;

			for ( i = 0; i < rescaleHelper.Count(); ++i )
			{
				CChoreoEvent *e = rescaleHelper[ i ];
				float st = e->GetStartTime();
				float et = e->HasEndTime() ? e->GetEndTime() : e->GetStartTime();
				float flTimeFromStart = st - flStart;
				float flTimeFromEnd = flEnd - et;
				float flDuration = e->GetDuration();
			
				float flNewStartTime = 0.0f;
				float flNewDuration = 0.0f;

				if ( m_nDragType == DRAGTYPE_RESCALELEFT )
				{
					float flNewEndTime = flEnd - flTimeFromEnd * flScale;
					if ( !e->HasEndTime() || e->IsFixedLength() )
					{
						e->OffsetTime( flNewEndTime - flDuration - st );
						continue;
					}
					flNewDuration = flDuration * flScale;
					flNewStartTime = flNewEndTime - flNewDuration;
				}
				else
				{
					flNewStartTime = flTimeFromStart * flScale + flStart;
					if ( !e->HasEndTime() || e->IsFixedLength() )
					{
						e->OffsetTime( flNewStartTime - st );
						continue;
					}
					flNewDuration = flDuration * flScale;
				}
				
				RescaleRamp( e, flNewDuration );
				switch ( e->GetType() )
				{
				default:
					break;
				case CChoreoEvent::GESTURE:
					{
						e->RescaleGestureTimes( flNewStartTime, flNewStartTime + flNewDuration, m_nDragType == DRAGTYPE_EVENT_STARTTIME || m_nDragType == DRAGTYPE_EVENT_ENDTIME );
					}
					break;
				case CChoreoEvent::FLEXANIMATION:
					{
						RescaleExpressionTimes( e, flNewStartTime, flNewStartTime + flNewDuration );
					}
					break;
				}

				e->SetStartTime( flNewStartTime );
				Assert( e->HasEndTime() );
				e->SetEndTime( flNewStartTime + flNewDuration );
			}
		}
	}

	for ( int i = 0; i < m_SceneGlobalEvents.Count(); i++ )
	{
		CChoreoGlobalEventWidget *gew = m_SceneGlobalEvents[ i ];
		if ( !gew || !gew->IsSelected() )
			continue;

		CChoreoEvent *e = gew->GetEvent();
		if ( !e )
			continue;
	
		e->OffsetTime( dt );
		e->SnapTimes();
	}

	m_nDragType = DRAGTYPE_NONE;

	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = 0;
	}

	CChoreoEvent *e = m_pClickedEvent ? m_pClickedEvent->GetEvent() : NULL;

	if ( e )
	{		
		// See if event is moving to a new owner
		CChoreoChannelWidget *chOrig, *chNew;

		int dy = my - m_yStart;
		bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;
		if ( !shiftdown )
		{
			dy = 0;
		}

		if ( abs( dy ) < m_pClickedEvent->GetItemHeight() )
		{
			my = m_yStart;
		}

		chNew = GetChannelUnderCursorPos( mx, my );
		
		InvalidateLayout();

		mx = m_xStart;
		my = m_yStart;

		chOrig = m_pClickedChannel;

		if ( chOrig && chNew && chOrig != chNew )
		{
			// Swap underlying objects
			CChoreoChannel *pOrigChannel, *pNewChannel;

			pOrigChannel = chOrig->GetChannel();
			pNewChannel = chNew->GetChannel();

			Assert( pOrigChannel && pNewChannel );

			// Remove event and move to new object
			DeleteSceneWidgets();

			pOrigChannel->RemoveEvent( e );
			pNewChannel->AddEvent( e );

			e->SetChannel( pNewChannel );
			e->SetActor( pNewChannel->GetActor() );

			CreateSceneWidgets();
		}
		else
		{
			if ( e && e->GetType() == CChoreoEvent::SPEAK )
			{
				// Show phone wav in wav viewer
				SetCurrentWaveFile( va( "sound/%s", FacePoser_TranslateSoundName( e ) ), e );
			}
		}
	}

	PushRedo( desc );
	InvalidateLayout();

	if ( e )
	{
		switch ( e->GetType() )
		{
		default:
			break;
		case CChoreoEvent::FLEXANIMATION:
			{
				g_pExpressionTool->SetEvent( e );
				g_pFlexPanel->SetEvent( e );
			}
			break;
		case CChoreoEvent::GESTURE:
			{
				g_pGestureTool->SetEvent( e );
			}
			break;
		}

		if ( e->HasEndTime() )
		{
			g_pRampTool->SetEvent( e );
		}
	}
	g_pExpressionTool->LayoutItems( true );
	g_pExpressionTool->redraw();
	g_pGestureTool->redraw();
	g_pRampTool->redraw();
	g_pSceneRampTool->redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::MouseFinishDrag( mxEvent *event, int mx, int my )
{
	if ( !m_bDragging )
		return;

	ApplyBounds( mx, my );

	switch ( m_nDragType )
	{
	case DRAGTYPE_SCRUBBER:
		{
			DrawFocusRect();
	
			m_FocusRects.Purge();

			float t = GetTimeValueForMouse( mx );
			t += m_flScrubberTimeOffset;
			m_flScrubberTimeOffset = 0.0f;

			ClampTimeToSelectionInterval( t );

			SetScrubTime( t );
			SetScrubTargetTime( t );

			m_bDragging = false;
			m_nDragType = DRAGTYPE_NONE;

			redraw();
		}
		break;
	case DRAGTYPE_EVENT_MOVE:
	case DRAGTYPE_EVENT_STARTTIME:
	case DRAGTYPE_EVENT_STARTTIME_RESCALE:
	case DRAGTYPE_EVENT_ENDTIME:
	case DRAGTYPE_EVENT_ENDTIME_RESCALE:
	case DRAGTYPE_EVENTTAG_MOVE:
	case DRAGTYPE_EVENTABSTAG_MOVE:
	case DRAGTYPE_RESCALELEFT:
	case DRAGTYPE_RESCALERIGHT:
		FinishDraggingEvent( event, mx, my );
		break;
	case DRAGTYPE_SCENE_ENDTIME:
		FinishDraggingSceneEndTime( event, mx, my );
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::handleEvent( mxEvent *event )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::MouseWheeled:
		{
			CChoreoScene *scene = GetScene();
			if ( scene )
			{
				int tz = GetTimeZoom( GetToolName() );
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

				SetTimeZoom( GetToolName(), tz, true );

				CUtlVector< CChoreoEvent * > selected;
				RememberSelectedEvents( selected );

				DeleteSceneWidgets();
				CreateSceneWidgets();

				ReselectEvents( selected );

				InvalidateLayout();
				Con_Printf( "Zoom factor %i %%\n", GetTimeZoom( GetToolName() ) );
			}
			iret = 1;
		}
		break;
	case mxEvent::Size:
		{
			// Force scroll bars to recompute
			ForceScrollBarsToRecompute( false );

			InvalidateLayout();
			PositionControls();
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
			if ( !m_bDragging )
			{
				if ( event->buttons & mxEvent::MouseRightButton )
				{
					if ( IsMouseOverTimeline( (short)event->x, (short)event->y ) )
					{
						PlaceABPoint( (short)event->x );
						redraw();
					}
					else if ( IsMouseOverScrubArea( event ) )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						
						ClampTimeToSelectionInterval( t );

						SetScrubTime( t );
						SetScrubTargetTime( t );

						sound->Flush();

						// Unpause the scene
						m_bPaused = false;

						redraw();
					}
					else
					{
						// Show right click menu
						ShowContextMenu( (short)event->x, (short)event->y );
					}
				}
				else
				{
					if ( IsMouseOverTimeline( (short)event->x, (short)event->y ) )
					{
						ClearABPoints();
						redraw();
					}
					else
					{
						// Handle mouse dragging here
						MouseStartDrag( event, (short)event->x, (short)event->y );
					}
				}
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseDrag:
		{
			MouseContinueDrag( event, (short)event->x, (short)event->y );
			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			MouseFinishDrag( event, (short)event->x, (short)event->y );
			iret = 1;
		}
		break;
	case mxEvent::MouseMove:
		{
			MouseMove( (short)event->x, (short)event->y );
			UpdateStatusArea( (short)event->x, (short)event->y );
			iret = 1;
		}
		break;
	case mxEvent::KeyDown:
		{
			iret = 1;

			switch ( event->key )
			{
			default:
				iret = 0;
				break;
			case 'E':
				if ( GetAsyncKeyState( VK_CONTROL ) )
				{
					OnPlaceNextSpeakEvent();
				}
				break;
			case VK_ESCAPE:
				DeselectAll();
				break;
			case 'C':
				CopyEvents();
				iret = 1;
				break;
			case 'V':
				PasteEvents();
				redraw();
				break;
			case VK_DELETE:
				{
					if ( IsActiveTool() )
					{
						DeleteSelectedEvents();
					}
				}
				break;
			case VK_RETURN:
				{
					CUtlVector< CChoreoEvent * > events;
					GetSelectedEvents( events );
					if ( events.Count() == 1 )
					{
						if ( GetAsyncKeyState( VK_MENU ) )
						{
							EditEvent( events[ 0 ] );
							redraw();
							iret = 1;
						}
					}
				}
				break;
			case 'Z':  // Undo/Redo
				{
					if ( GetAsyncKeyState( VK_CONTROL ) )
					{
						if ( GetAsyncKeyState( VK_SHIFT ) )
						{
							if ( CanRedo() )
							{
								Con_Printf( "Redo %s\n", GetRedoDescription() );
								Redo();
								iret = 1;
							}
						}
						else
						{
							if ( CanUndo() )
							{
								Con_Printf( "Undo %s\n", GetUndoDescription() );
								Undo();
								iret = 1;
							}
						}
					}
				}
				break;

			case VK_SPACE:
				{
					if ( IsPlayingScene() )
					{
						StopScene();
					}
				}
				break;
			case 188: // VK_OEM_COMMA:
				{
					SetScrubTargetTime( 0.0f );
				}
				break;
			case 190: // VK_OEM_PERIOD:
				{
					CChoreoScene *scene = GetScene();
					if ( scene )
					{
						SetScrubTargetTime( scene->FindStopTime() );
					}
				}
				break;
			case VK_LEFT: 
				{
					CChoreoScene *scene = GetScene();
					if ( scene && scene->GetSceneFPS() > 0 )
					{
						float curscrub = m_flScrub;
						curscrub -= ( 1.0f / (float)scene->GetSceneFPS() );
						curscrub = max( curscrub, 0.0f );
						SetScrubTargetTime( curscrub );
					}
				}
				break;
			case VK_RIGHT: 
				{
					CChoreoScene *scene = GetScene();
					if ( scene && scene->GetSceneFPS() > 0 )
					{
						float curscrub = m_flScrub;
						curscrub += ( 1.0f / (float)scene->GetSceneFPS() );
						curscrub = min( curscrub, scene->FindStopTime() );
						SetScrubTargetTime( curscrub );
					}
				}
				break;
			case VK_HOME:
				{
					MoveTimeSliderToPos( 0 );
				}
				break;
			case VK_END:
				{
					float maxtime = m_pScene->FindStopTime() - 1.0f;
					int pixels = (int)( maxtime * GetPixelsPerSecond() );
					MoveTimeSliderToPos( pixels - 1 );
				}
				break;
			case VK_PRIOR:  // PgUp
				{
					int window = w2() - GetLabelWidth();
					m_flLeftOffset = max( m_flLeftOffset - (float)window, 0.0f );
					MoveTimeSliderToPos( (int)m_flLeftOffset );
				}
				break;
			case VK_NEXT:   // PgDown
				{
					int window = w2() - GetLabelWidth();
					int pixels = ComputeHPixelsNeeded();
					m_flLeftOffset = min( m_flLeftOffset + (float)window, (float)pixels );
					MoveTimeSliderToPos( (int)m_flLeftOffset );
				}
				break;
			}
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				{
					iret = 0;
					int lang_index = event->action - IDC_CV_CC_LANGUAGESTART;
					if ( lang_index >= 0 && lang_index < CC_NUM_LANGUAGES )
					{
						iret = 1;
						SetCloseCaptionLanguageId( lang_index );
					}
				}
				break;
			case IDC_CV_TOGGLECLOSECAPTIONS:
				{
					OnToggleCloseCaptionsForEvent();
				}
				break;
			case IDC_CV_CHANGECLOSECAPTIONTOKEN:
				{
					if ( m_pClickedChannel )
					{
						CChoreoEvent *e = m_pClickedChannel->GetCaptionClickedEvent();
						if ( e && e->GetNumSlaves() >= 1 )
						{
							OnChangeCloseCaptionToken( e );
						}
					}
				}
				break;
			case IDC_CV_REMOVESPEAKEVENTFROMGROUP:
				{
					OnRemoveSpeakEventFromGroup();
				}
				break;
			case IDC_CV_COMBINESPEAKEVENTS:
				{
					OnCombineSpeakEvents();
				}
				break;
			case IDC_CV_CC_SHOW:
				{
					OnToggleCloseCaptionTags();
				}
				break;
			case IDC_CV_TOGGLERAMPONLY:
				{
					m_bRampOnly = !m_bRampOnly;
					redraw();
				}
				break;
			case IDC_CV_PROCESSSEQUENCES:
				{
					m_bProcessSequences = !m_bProcessSequences;
				}
				break;
			case IDC_CV_CHECKSEQLENGTHS:
				{
					OnCheckSequenceLengths();
				}
				break;
			case IDC_CV_CHANGESCALE:
				{
					OnChangeScale();
				}
				break;
			case IDC_CHOREO_PLAYBACKRATE:
				{
					m_flPlaybackRate = m_pPlaybackRate->getValue();
					redraw();
				}
				break;
			case IDC_COPYEVENTS:
				CopyEvents();
				break;
			case IDC_PASTEEVENTS:
				PasteEvents();
				redraw();
				break;
			case IDC_IMPORTEVENTS:
				ImportEvents();
				redraw();
				break;
			case IDC_EXPORTEVENTS:
				ExportEvents();
				redraw();
				break;
			case IDC_EXPORT_VCD:
				ExportVCD();
				redraw();
				break;
			case IDC_IMPORT_VCD:
				ImportVCD();
				redraw();
				break;
			case IDC_EXPRESSIONTOOL:
				OnExpressionTool();
				break;
			case IDC_GESTURETOOL:
				OnGestureTool();
				break;
			case IDC_ASSOCIATEBSP:
				AssociateBSP();
				break;
			case IDC_ASSOCIATEMODEL:
				AssociateModel();
				break;
			case IDC_CVUNDO:
				Undo();
				break;
			case IDC_CVREDO:
				Redo();
				break;
			case IDC_SELECTALL:
				SelectAll();
				break;
			case IDC_DESELECTALL:
				DeselectAll();
				break;
			case IDC_PLAYSCENE:	
				Con_Printf( "Commencing playback\n" );
				PlayScene( true );
				break;
			case IDC_PAUSESCENE:
				Con_Printf( "Pausing playback\n" );
				PauseScene();
				break;
			case IDC_STOPSCENE:
				Con_Printf( "Canceling playback\n" );
				StopScene();
				break;
			case IDC_CHOREOVSCROLL:
				{
					int offset = 0;
					bool processed = true;

					switch ( event->modifiers )
					{
					case SB_THUMBTRACK:
						offset = event->height;
						break;
					case SB_PAGEUP:
						offset = m_pVertScrollBar->getValue();
						offset -= 20;
						offset = max( offset, m_pVertScrollBar->getMinValue() );
						break;
					case SB_PAGEDOWN:
						offset = m_pVertScrollBar->getValue();
						offset += 20;
						offset = min( offset, m_pVertScrollBar->getMaxValue() );
						break;
					case SB_LINEDOWN:
						offset = m_pVertScrollBar->getValue();
						offset += 10;
						offset = min( offset, m_pVertScrollBar->getMaxValue() );
						break;
					case SB_LINEUP:
						offset = m_pVertScrollBar->getValue();
						offset -= 10;
						offset = max( offset, m_pVertScrollBar->getMinValue() );
						break;
					default:
						processed = false;
						break;
					}
		
					if ( processed )
					{
						m_pVertScrollBar->setValue( offset );
						InvalidateRect( (HWND)m_pVertScrollBar->getHandle(), NULL, TRUE );
						m_nTopOffset = offset;
						InvalidateLayout();
					}
				}
				break;
			case IDC_CHOREOHSCROLL:
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
			case IDC_ADDACTOR:
				{
					NewActor();
				}
				break;
			case IDC_EDITACTOR:
				{
					CChoreoActorWidget *actor = m_pClickedActor;
					if ( actor )
					{
						EditActor( actor->GetActor() );
					}			
				}
				break;
			case IDC_DELETEACTOR:
				{
					CChoreoActorWidget *actor = m_pClickedActor;
					if ( actor )
					{
						DeleteActor( actor->GetActor() );
					}
				}
				break;
			case IDC_MOVEACTORUP:
				{
					CChoreoActorWidget *actor = m_pClickedActor;
					if ( actor )				
					{
						MoveActorUp( actor->GetActor()  );
					}
				}
				break;
			case IDC_MOVEACTORDOWN:
				{
					CChoreoActorWidget *actor = m_pClickedActor;
					if ( actor )				
					{
						MoveActorDown( actor->GetActor() );
					}
				}
				break;
			case IDC_CHANNELOPEN:
				{
					CActorBitmapButton *btn = static_cast< CActorBitmapButton * >( event->widget );
					if ( btn )
					{
						CChoreoActorWidget *a = btn->GetActor();
						if ( a )
						{
							a->ShowChannels( true );
						}
					}
				}
				break;
			case IDC_CHANNELCLOSE:
				{
					CActorBitmapButton *btn = static_cast< CActorBitmapButton * >( event->widget );
					if ( btn )
					{
						CChoreoActorWidget *a = btn->GetActor();
						if ( a )
						{
							a->ShowChannels( false );
						}
					}
				}
				break;
			case IDC_ADDEVENT_INTERRUPT:
				{
					AddEvent( CChoreoEvent::INTERRUPT );
				}
				break;
			case IDC_ADDEVENT_PERMITRESPONSES:
				{
					AddEvent( CChoreoEvent::PERMIT_RESPONSES );
				}
				break;
			case IDC_ADDEVENT_EXPRESSION:
				{
					AddEvent( CChoreoEvent::EXPRESSION );
				}
				break;
			case IDC_ADDEVENT_FLEXANIMATION:
				{
					AddEvent( CChoreoEvent::FLEXANIMATION );
				}
				break;
			case IDC_ADDEVENT_GESTURE:
				{
					AddEvent( CChoreoEvent::GESTURE );
				}
				break;
			case IDC_ADDEVENT_NULLGESTURE:
				{
					AddEvent( CChoreoEvent::GESTURE, 1 );
				}
				break;
			case IDC_ADDEVENT_LOOKAT:
				{
					AddEvent( CChoreoEvent::LOOKAT );
				}
				break;
			case IDC_ADDEVENT_MOVETO:
				{
					AddEvent( CChoreoEvent::MOVETO );
				}
				break;
			case IDC_ADDEVENT_FACE:
				{
					AddEvent( CChoreoEvent::FACE );
				}
				break;
			case IDC_ADDEVENT_SPEAK:
				{
					AddEvent( CChoreoEvent::SPEAK );
				}
				break;
			case IDC_ADDEVENT_FIRETRIGGER:
				{
					AddEvent( CChoreoEvent::FIRETRIGGER );
				}
				break;
			case IDC_ADDEVENT_GENERIC:
				{
					AddEvent( CChoreoEvent::GENERIC );
				}
				break;
			case IDC_ADDEVENT_SUBSCENE:
				{
					AddEvent( CChoreoEvent::SUBSCENE );
				}
				break;
			case IDC_ADDEVENT_SEQUENCE:
				{
					AddEvent( CChoreoEvent::SEQUENCE );
				}
				break;
			case IDC_EDITEVENT:
				{
					CChoreoEventWidget *event = m_pClickedEvent;
					if ( event )
					{
						EditEvent( event->GetEvent() );
						redraw();
					}
				}
				break;
			case IDC_DELETEEVENT:
				{
					DeleteSelectedEvents();
				}
				break;
			case IDC_CV_ENABLEEVENTS:
				{
					EnableSelectedEvents( true );
				}
				break;
			case IDC_CV_DISABLEEVENTS:
				{
					EnableSelectedEvents( false );
				}
				break;
			case IDC_MOVETOBACK:
				{
					CChoreoEventWidget *event = m_pClickedEvent;
					if ( event )
					{
						MoveEventToBack( event->GetEvent() );
					}
				}
				break;
			case IDC_DELETERELATIVETAG:
				{
					CChoreoEventWidget *event = m_pClickedEvent;
					if ( event && m_nClickedTag >= 0 )
					{
						DeleteEventRelativeTag( event->GetEvent(), m_nClickedTag );
					}
				}
				break;
			case IDC_ADDTIMINGTAG:
				{
					AddEventRelativeTag();
				}
				break;
			case IDC_ADDEVENT_PAUSE:
				{
					AddGlobalEvent( CChoreoEvent::SECTION );
				}
				break;
			case IDC_ADDEVENT_LOOP:
				{
					AddGlobalEvent( CChoreoEvent::LOOP );
				}
				break;
			case IDC_ADDEVENT_STOPPOINT:
				{
					AddGlobalEvent( CChoreoEvent::STOPPOINT );
				}
				break;
			case IDC_EDITGLOBALEVENT:
				{
					CChoreoGlobalEventWidget *event = m_pClickedGlobalEvent;
					if ( event )
					{
						EditGlobalEvent( event->GetEvent() );
						redraw();
					}
				}
				break;
			case IDC_DELETEGLOBALEVENT:
				{
					CChoreoGlobalEventWidget *event = m_pClickedGlobalEvent;
					if ( event )
					{
						DeleteGlobalEvent( event->GetEvent() );
					}
				}
				break;
			case IDC_ADDCHANNEL:
				{
					NewChannel();
				}
				break;
			case IDC_EDITCHANNEL:
				{
					CChoreoChannelWidget *channel = m_pClickedChannel;
					if ( channel )
					{
						EditChannel( channel->GetChannel() );
					}
				}
				break;
			case IDC_DELETECHANNEL:
				{
					CChoreoChannelWidget *channel = m_pClickedChannel;
					if ( channel )
					{
						DeleteChannel( channel->GetChannel() );
					}
				}
				break;
			case IDC_MOVECHANNELUP:
				{
					CChoreoChannelWidget *channel = m_pClickedChannel;
					if ( channel )
					{
						MoveChannelUp( channel->GetChannel() );
					}
				}
				break;
			case IDC_MOVECHANNELDOWN:
				{
					CChoreoChannelWidget *channel = m_pClickedChannel;
					if ( channel )
					{
						MoveChannelDown( channel->GetChannel() );
					}
				}
				break;
			case IDC_CV_ALLEVENTS_CHANNEL:
				{
					CChoreoChannelWidget *channel = m_pClickedChannel;
					if ( channel )
					{
						SelectAllEventsInChannel( channel );
					}
				}
				break;
			case IDC_CV_ALLEVENTS_ACTOR:
				{
					CChoreoActorWidget *actor = m_pClickedActor;
					if ( actor )
					{
						SelectAllEventsInActor( actor );
					}
				}
				break;
			case IDC_SELECTEVENTS_ALL_BEFORE:
				{
					SelectionParams_t params;
					Q_memset( &params, 0, sizeof( params ) );
					params.forward = false;
					params.time = GetTimeValueForMouse( m_nClickedX );
					params.type = SelectionParams_t::SP_ALL;

					SelectEvents( params );
				}
				break;
			case IDC_SELECTEVENTS_ALL_AFTER:
				{
					SelectionParams_t params;
					Q_memset( &params, 0, sizeof( params ) );
					params.forward = true;
					params.time = GetTimeValueForMouse( m_nClickedX );
					params.type = SelectionParams_t::SP_ALL;

					SelectEvents( params );
				}
				break;
			case IDC_SELECTEVENTS_ACTIVE_BEFORE:
				{
					SelectionParams_t params;
					Q_memset( &params, 0, sizeof( params ) );
					params.forward = false;
					params.time = GetTimeValueForMouse( m_nClickedX );
					params.type = SelectionParams_t::SP_ACTIVE;

					SelectEvents( params );
				}
				break;
			case IDC_SELECTEVENTS_ACTIVE_AFTER:
				{
					SelectionParams_t params;
					Q_memset( &params, 0, sizeof( params ) );
					params.forward = true;
					params.time = GetTimeValueForMouse( m_nClickedX );
					params.type = SelectionParams_t::SP_ACTIVE;

					SelectEvents( params );
				}
				break;
			case IDC_SELECTEVENTS_CHANNEL_BEFORE:
				{
					SelectionParams_t params;
					Q_memset( &params, 0, sizeof( params ) );
					params.forward = false;
					params.time = GetTimeValueForMouse( m_nClickedX );
					params.type = SelectionParams_t::SP_CHANNEL;

					SelectEvents( params );
				}
				break;
			case IDC_SELECTEVENTS_CHANNEL_AFTER:
				{
					SelectionParams_t params;
					Q_memset( &params, 0, sizeof( params ) );
					params.forward = true;
					params.time = GetTimeValueForMouse( m_nClickedX );
					params.type = SelectionParams_t::SP_CHANNEL;

					SelectEvents( params );
				}
				break;
			case IDC_INSERT_TIME:
				{
					OnInsertTime();
				}
				break;
			case IDC_DELETE_TIME:
				{
					OnDeleteTime();
				}
				break;
			case IDC_CV_ALIGN_LEFT:
				{
					OnAlign( true );
				}
				break;
			case IDC_CV_ALIGN_RIGHT:
				{
					OnAlign( false );
				}
				break;
			case IDC_CV_SAMESIZE_SMALLEST:
				{
					OnMakeSameSize( true );
				}
				break;
			case IDC_CV_SAMESIZE_LARGEST:
				{
					OnMakeSameSize( false );
				}
				break;
			}

			if ( iret == 1 )
			{
				SetActiveTool( this );
			}
		}
		break;
	}
	return iret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::PlayScene( bool forward )
{
	m_bForward = forward;
	if ( !m_pScene )
		return;

	sound->Flush();

	// Make sure phonemes are loaded
	FacePoser_EnsurePhonemesLoaded();

	// Unpause
	if ( m_bSimulating && m_bPaused )
	{
		m_bPaused = false;
		return;
	}

	m_bSimulating = true;
	m_bPaused = false;

//	float soundlatency =  max( sound->GetAmountofTimeAhead(), 0.0f );
//	soundlatency = min( 0.5f, soundlatency );

	float soundlatency = 0.0f;

	float sceneendtime = m_pScene->FindStopTime();

	m_pScene->SetSoundFileStartupLatency( soundlatency );

	if ( m_rgABPoints[ 0 ].active ||
		 m_rgABPoints[ 1 ].active  )
	{
		if ( m_rgABPoints[ 0 ].active &&
			 m_rgABPoints[ 1 ].active  )
		{
			float st = m_rgABPoints[ 0 ].time;
			float ed = m_rgABPoints[ 1 ].time;

			m_pScene->ResetSimulation( m_bForward, st, ed );

			SetScrubTime( m_bForward ? st : ed );
			SetScrubTargetTime( m_bForward ? ed : st );
		}
		else
		{
			float startonly = m_rgABPoints[ 0 ].active ? m_rgABPoints[ 0 ].time : m_rgABPoints[ 1 ].time;

			m_pScene->ResetSimulation( m_bForward, startonly );

			SetScrubTime( m_bForward ? startonly : sceneendtime );
			SetScrubTargetTime( m_bForward ? sceneendtime : startonly );
		}
	}
	else
	{
		// NO start end/loop
		m_pScene->ResetSimulation( m_bForward );

		SetScrubTime( m_bForward ? 0 : sceneendtime );
		SetScrubTargetTime( m_bForward ? sceneendtime : 0 );
	}

	if ( g_viewerSettings.speedScale == 0.0f )
	{
		m_flLastSpeedScale = g_viewerSettings.speedScale;
		m_bResetSpeedScale = true;

		g_viewerSettings.speedScale = 1.0f;

		Con_Printf( "Resetting speed scale to 1.0\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//-----------------------------------------------------------------------------
void CChoreoView::MoveTimeSliderToPos( int x )
{
	m_flLeftOffset = (float)x;
	m_pHorzScrollBar->setValue( (int)m_flLeftOffset );
	InvalidateRect( (HWND)m_pHorzScrollBar->getHandle(), NULL, TRUE );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::PauseScene( void )
{
	if ( !m_bSimulating )
		return;

	m_bPaused = true;
	sound->StopAll();
}

//-----------------------------------------------------------------------------
// Purpose: Apply expression to actor's face
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessExpression( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::EXPRESSION );

	StudioModel *model = FindAssociatedModel( scene, event->GetActor() );
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

	CExpClass *p = expressions->FindClass( event->GetParameters(), true );
	if ( !p )
	{
		return;
	}

	CExpression *exp = p->FindExpression( event->GetParameters2() );
	if ( !exp )
	{
		return;
	}

	CChoreoActor *a = event->GetActor();
	if ( !a )
		return;

	CChoreoActorWidget *actor = NULL;

	int i;
	for ( i = 0; i < m_SceneActors.Size(); i++ )
	{
		actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		if ( actor->GetActor() == a )
			break;
	}

	if ( !actor || i >= m_SceneActors.Size() )
		return;

	float *settings = exp->GetSettings();
	Assert( settings );
	float *weights = exp->GetWeights();
	Assert( weights );
	float *current = actor->GetSettings();
	Assert( current );

	float flIntensity = event->GetIntensity( scene->GetTime() );

	// blend in target values for correct actor
	for ( LocalFlexController_t i = (LocalFlexController_t)0; i < hdr->numflexcontrollers(); i++ )
	{
		mstudioflexcontroller_t *pFlex = hdr->pFlexcontroller( i );
		int j = pFlex->localToGlobal;
		if ( j < 0 )
			continue;
		float s = clamp( weights[j] * flIntensity, 0.0, 1.0 );
		current[ j ] = current[j] * (1.0f - s) + settings[ j ] * s;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hdr - 
//			*event - 
//-----------------------------------------------------------------------------
void SetupFlexControllerTracks( CStudioHdr *hdr, CChoreoEvent *event )
{
	Assert( hdr );
	Assert( event );

	if ( !hdr )
		return;

	if ( !event )
		return;

	// Already done
	if ( event->GetTrackLookupSet() )
		return;

	/*
	// FIXME:  Brian hooked this stuff up for some took work, but at this point the .mdl files don't look like they've been updated to include the remapping data yet...
	int c = hdr->numflexcontrollerremaps();
	for ( i = 0; i < c; ++i )
	{
		mstudioflexcontrollerremap_t *remap = hdr->pFlexcontrollerRemap( i );
		Msg( "remap %s\n", remap->pszName() );
		Msg( "  type %d\n", remap->remaptype );
		Msg( "  num remaps %d (stereo %s)\n", remap->numremaps, remap->stereo ? "true" : "false" );
		for ( int j = 0 ; j < remap->numremaps; ++j )
		{
			int index = remap->pRemapControlIndex( j );
			Msg( "  %d:  maps to %d (%s) with %s\n", j, index, hdr->pFlexcontroller( index )->pszName(), remap->pRemapControl( j ) );
		}
	}
	*/

	// Unlink stuff in case it doesn't exist
	int	nTrackCount = event->GetNumFlexAnimationTracks();
	for ( int i = 0; i < nTrackCount; ++i )
	{
		CFlexAnimationTrack *pTrack = event->GetFlexAnimationTrack( i );
		pTrack->SetFlexControllerIndex( LocalFlexController_t(-1), -1, 0 );
		pTrack->SetFlexControllerIndex( LocalFlexController_t(-1), -1, 1 );
	}

	for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); ++i )
	{
		int j = hdr->pFlexcontroller( i )->localToGlobal;

		char const *name = hdr->pFlexcontroller( i )->pszName();
		if ( !name )
			continue;

		bool combo = false;
		// Look up or create all necessary tracks
		if ( strncmp( "right_", name, 6 ) == 0 )
		{
			combo = true;
			name = &name[6];
		}

		CFlexAnimationTrack *track = event->FindTrack( name );
		if ( !track )
		{
			track = event->AddTrack( name );
			Assert( track );
		}

		track->SetFlexControllerIndex( i, j, 0 );
		if ( combo )
		{
			track->SetFlexControllerIndex( LocalFlexController_t(i + 1), hdr->pFlexcontroller( LocalFlexController_t(i + 1) )->localToGlobal, 1 );
			track->SetComboType( true );
		}

		float orig_min = track->GetMin( );
		float orig_max = track->GetMax( );

		// set range
		if (hdr->pFlexcontroller( i )->min == 0.0f || hdr->pFlexcontroller( i )->max == 1.0f)
		{
			track->SetInverted( false );
			track->SetMin( hdr->pFlexcontroller( i )->min );
			track->SetMax( hdr->pFlexcontroller( i )->max );
		}
		else
		{
			// invert ranges for wide ranged, makes sense considering flexcontroller names...
			track->SetInverted( true );
			track->SetMin( hdr->pFlexcontroller( i )->max );
			track->SetMax( hdr->pFlexcontroller( i )->min );
		}

		// resample track based on this models dynamic range
		if (track->GetNumSamples( 0 ) > 0)
		{
			float range = track->GetMax( ) - track->GetMin( );

			for (int i = 0; i < track->GetNumSamples( 0 ); i++)
			{
				CExpressionSample	*sample = track->GetSample( i, 0 );
				float rangedValue = orig_min * (1 - sample->value) + orig_max * sample->value;
				sample->value = clamp( (rangedValue - track->GetMin( )) / range, 0.0, 1.0 );
			}
		}

		// skip next flex since we've already assigned it
		if ( combo )
		{
			i++;
		}
	}

	event->SetTrackLookupSet( true );
}

//-----------------------------------------------------------------------------
// Purpose: Apply flexanimation to actor's face
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessFlexAnimation( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::FLEXANIMATION );

	StudioModel *model = FindAssociatedModel( scene, event->GetActor()  );
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

	CChoreoActor *a = event->GetActor();

	CChoreoActorWidget *actor = NULL;

	int i;
	for ( i = 0; i < m_SceneActors.Size(); i++ )
	{
		actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		if ( !stricmp( actor->GetActor()->GetName(), a->GetName() ) )
			break;
	}

	if ( !actor || i >= m_SceneActors.Size() )
		return;

	float *current = actor->GetSettings();
	Assert( current );

	if ( !event->GetTrackLookupSet() )
	{
		SetupFlexControllerTracks( hdr, event );
	}

	float weight = event->GetIntensity( scene->GetTime() );

	CChoreoEventWidget *eventwidget = FindWidgetForEvent( event );
	bool bUpdateSliders = (eventwidget && eventwidget->IsSelected() && model == models->GetActiveStudioModel() );

	// Iterate animation tracks
	for ( i = 0; i < event->GetNumFlexAnimationTracks(); i++ )
	{
		CFlexAnimationTrack *track = event->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		// Disabled
		if ( !track->IsTrackActive() )
		{
			if ( bUpdateSliders )
			{
				for ( int side = 0; side < 1 + track->IsComboType(); side++ )
				{
					int controller = track->GetFlexControllerIndex( side );
					if ( controller != -1 && !g_pFlexPanel->IsEdited( controller ))
					{
						g_pFlexPanel->SetSlider( controller, 0.0 );
						g_pFlexPanel->SetInfluence( controller, 0.0f );
					}
				}
			}
			continue;
		}

		// Map track flex controller to global name
		if ( track->IsComboType() )
		{
			for ( int side = 0; side < 2; side++ )
			{
				int controller = track->GetFlexControllerIndex( side );
				if ( controller != -1 )
				{
					// Get spline intensity for controller
					float flIntensity = track->GetIntensity( scene->GetTime(), side );

					if (bUpdateSliders && !g_pFlexPanel->IsEdited( controller ) )
					{
						g_pFlexPanel->SetSlider( controller, flIntensity );
						g_pFlexPanel->SetInfluence( controller, 1.0f );
					}

					flIntensity = current[ controller ] * (1 - weight) + flIntensity * weight;
					current[ controller ] = flIntensity;
				}
			}
		}
		else
		{
			int controller = track->GetFlexControllerIndex( 0 );
			if ( controller != -1 )
			{
				// Get spline intensity for controller
				float flIntensity = track->GetIntensity( scene->GetTime(), 0 );

				if (bUpdateSliders && !g_pFlexPanel->IsEdited( controller ) )
				{
					g_pFlexPanel->SetSlider( controller, flIntensity );
					g_pFlexPanel->SetInfluence( controller, 1.0f );
				}

				flIntensity = current[ controller ] * (1 - weight) + flIntensity * weight;
				current[ controller ] = flIntensity;
			}
		}
	}
}


#include "mapentities.h"

//-----------------------------------------------------------------------------
// Purpose: Apply lookat target
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessLookat( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::LOOKAT );

	if ( !event->GetActor() )
		return;

	CChoreoActor *a = event->GetActor();

	Assert( a );

	StudioModel *model = FindAssociatedModel( scene, a );
	if ( !model )
	{
		return;
	}

	float flIntensity = event->GetIntensity( scene->GetTime() );

	// clamp in-ramp to 0.3 seconds
	float flDuration = scene->GetTime() - event->GetStartTime();
	float flMaxIntensity = flDuration < 0.3f ? SimpleSpline( flDuration / 0.3f ) : 1.0f;
	flDuration = event->GetEndTime() - scene->GetTime();
	flMaxIntensity = min( flMaxIntensity, flDuration < 0.3f ? SimpleSpline( flDuration / 0.3f ) : 1.0f );
	flIntensity = clamp( flIntensity, 0.0f, flMaxIntensity );

	if (!stricmp( event->GetParameters(), a->GetName() ) || !stricmp( event->GetParameters(), "!self" ))
	{
		model->AddLookTargetSelf( flIntensity );
	}
	else if ( !stricmp( event->GetParameters(), "player" ) || 
		!stricmp( event->GetParameters(), "!player" ) )
	{
		Vector vecTarget = model->m_origin;
		vecTarget.z = 0;

		model->AddLookTarget( vecTarget, flIntensity );
	}
	else
	{
		mapentities->CheckUpdateMap( scene->GetMapname() );

		Vector orgActor;
		Vector orgTarget;
		QAngle anglesActor;
		QAngle anglesDummy;

		if ( event->GetPitch() != 0 ||
			 event->GetYaw() != 0 )
		{
			QAngle angles( -(float)event->GetPitch(),
				(float)event->GetYaw(),
				0 );

			matrix3x4_t matrix;

			AngleMatrix( model->m_angles, matrix );

			Vector vecForward;
			AngleVectors( angles, &vecForward );

			Vector eyeTarget;
			VectorRotate( vecForward, matrix, eyeTarget );
			VectorScale( eyeTarget, 75, eyeTarget );

			model->AddLookTarget( eyeTarget, flIntensity );
		}
		else
		{
			if ( mapentities->LookupOrigin( a->GetName(), orgActor, anglesActor ) )
			{
				if ( mapentities->LookupOrigin( event->GetParameters(), orgTarget, anglesDummy ) )
				{
					Vector delta = orgTarget - orgActor;
					
					matrix3x4_t matrix;
					Vector lookTarget;

					// Rotate around actor's placed forward direction since we look straight down x in faceposer/hlmv
					AngleMatrix( anglesActor, matrix );
					VectorIRotate( delta, matrix, lookTarget );
					
					model->AddLookTarget( lookTarget, flIntensity );
					return;
				}
			}
			// hack up something based on the name.
			{
				const char *cp = event->GetParameters();
				float value = 0.0;
				while (*cp)
				{
					value += *cp++;
				}
				value = cos( value );
				value = acos( value );
				QAngle angles( 0.0, value * 45 / M_PI, 0.0 );

				matrix3x4_t matrix;
				AngleMatrix( model->m_angles, matrix );

				Vector vecForward;
				AngleVectors( angles, &vecForward );

				Vector eyeTarget;
				VectorRotate( vecForward, matrix, eyeTarget );
				VectorScale( eyeTarget, 75, eyeTarget );

				model->AddLookTarget( eyeTarget, flIntensity );
			}

		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Returns a target for Faceing
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::GetTarget( CChoreoScene *scene, CChoreoEvent *event, Vector &vecTarget, QAngle &vecAngle )
{
	if ( !event->GetActor() )
		return false;

	CChoreoActor *a = event->GetActor();

	Assert( a );

	StudioModel *model = FindAssociatedModel( scene, a );
	if ( !model )
	{
		return false;
	}

	if (!stricmp( event->GetParameters(), a->GetName() ))
	{
		vecTarget = vec3_origin;
		return true;
	}
	else if ( !stricmp( event->GetParameters(), "player" ) || 
		!stricmp( event->GetParameters(), "!player" ) )
	{
		vecTarget = model->m_origin;
		vecTarget.z = 0;
		vecAngle = model->m_angles;

		return true;
	}
	else
	{
		mapentities->CheckUpdateMap( scene->GetMapname() );

		Vector orgActor;
		Vector orgTarget;
		QAngle anglesActor;
		QAngle anglesDummy;

		if ( event->GetPitch() != 0 ||
			 event->GetYaw() != 0 )
		{
			QAngle angles( -(float)event->GetPitch(),
				(float)event->GetYaw(),
				0 );

			matrix3x4_t matrix;

			AngleMatrix( model->m_angles, matrix );

			QAngle angles2 = angles;
			angles2.x *= 0.6f;
			angles2.y *= 0.8f;

			Vector vecForward, vecForward2;
			AngleVectors( angles, &vecForward );
			AngleVectors( angles2, &vecForward2 );

			VectorNormalize( vecForward );
			VectorNormalize( vecForward2 );

			Vector eyeTarget, headTarget;

			VectorRotate( vecForward, matrix, eyeTarget );
			VectorRotate( vecForward2, matrix, headTarget );

			VectorScale( eyeTarget, 150, eyeTarget );

			VectorScale( headTarget, 150, vecTarget );
			return true;

		}
		else
		{
			if ( mapentities->LookupOrigin( a->GetName(), orgActor, anglesActor ) )
			{
				if ( mapentities->LookupOrigin( event->GetParameters(), orgTarget, anglesDummy ) )
				{
					Vector delta = orgTarget - orgActor;
					
					matrix3x4_t matrix;
					Vector lookTarget;

					// Rotate around actor's placed forward direction since we look straight down x in faceposer/hlmv
					AngleMatrix( anglesActor, matrix );
					VectorIRotate( delta, matrix, vecTarget );
					
					return true;
				}
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Apply lookat target
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessFace( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::FACE );

	if ( !event->GetActor() )
		return;

	CChoreoActor *a = event->GetActor();

	Assert( a );

	StudioModel *model = FindAssociatedModel( scene, a );
	if ( !model )
	{
		return;
	}

	Vector vecTarget;
	QAngle vecAngle;

	if (!GetTarget( scene, event, vecTarget, vecAngle ))
	{
		return;
	}

	/*
	// FIXME: this is broke
	float goalYaw = -(vecAngle.y > 180 ? 360 - vecAngle.y : vecAngle.y );

	float intensity = event->GetIntensity( scene->GetTime() );

	float diff = goalYaw * intensity;
	float dir = 1.0;

	if (diff < 0)
	{
		diff = -diff;
		dir = -1;
	}

	float spineintensity = 0 * max( 0.0, (intensity - 0.5) / 0.5 );
	float goalSpineYaw = min( diff * (1.0 - spineintensity), 30 );
	//float idealYaw = info->m_flInitialYaw + (diff - m_goalBodyYaw * dir - m_goalSpineYaw * dir) * dir;
	// float idealYaw = UTIL_AngleMod( info->m_flInitialYaw + diff * intensity );

	// FIXME: this is broke
	// model->SetSpineYaw( goalSpineYaw * dir);
	// model->SetBodyYaw( goalBodyYaw * dir );

	// Msg("yaw %.1f : %.1f (%.1f)\n", info->m_flInitialYaw, idealYaw, intensity ); 
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scene - 
//			*event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessLoop( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::LOOP );

	// Don't loop when dragging scrubber!
	if ( IsScrubbing() )
		return;

	float backtime = (float)atof( event->GetParameters() );

	bool process = true;
	int counter = event->GetLoopCount();
	if ( counter != -1 )
	{
		int remaining = event->GetNumLoopsRemaining();
		if ( remaining <= 0 )
		{
			process = false;
		}
		else
		{
			event->SetNumLoopsRemaining( --remaining );
		}
	}

	if ( !process )
		return;

	scene->LoopToTime( backtime );
	SetScrubTime( backtime ); 
}

//-----------------------------------------------------------------------------
// Purpose: Add a gesture layer
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessGesture( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::GESTURE );

	// NULL event is just a placeholder
	if ( !Q_stricmp( event->GetName(), "NULL" ) )
	{
		return;
	}

	StudioModel *model = FindAssociatedModel( scene, event->GetActor()  );
	if ( !model )
		return;

	if ( !event->GetActor() )
		return;

	CChoreoActor *a = event->GetActor();

	Assert( a );

	int iSequence = model->LookupSequence( event->GetParameters() );
	if (iSequence < 0)
		return;

	// Get spline intensity for controller
	float eventlocaltime = scene->GetTime() - event->GetStartTime();

	float referencetime = event->GetOriginalPercentageFromPlaybackPercentage( eventlocaltime / event->GetDuration() ) * event->GetDuration();

	float resampledtime = event->GetStartTime() + referencetime;

	float cycle = event->GetCompletion( resampledtime );

	int iLayer = model->GetNewAnimationLayer( a->FindChannelIndex( event->GetChannel() ) );

	model->SetOverlaySequence( iLayer, iSequence, event->GetIntensity( scene->GetTime() ) );
	model->SetOverlayRate( iLayer, cycle, 0.0 );
}



//-----------------------------------------------------------------------------
// Purpose: Apply a sequence
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessSequence( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::SEQUENCE );

	if ( !m_bProcessSequences )
	{
		return;
	}

	StudioModel *model = FindAssociatedModel( scene, event->GetActor()  );
	if ( !model )
		return;

	if ( !event->GetActor() )
		return;

	CChoreoActor *a = event->GetActor();

	Assert( a );

	int iSequence = model->LookupSequence( event->GetParameters() );
	if (iSequence < 0)
		return;

	float flFrameRate;
	float flGroundSpeed;
	model->GetSequenceInfo( iSequence, &flFrameRate, &flGroundSpeed );

	float cycle;
	bool looping = model->GetSequenceLoops( iSequence );
	if (looping)
	{
		float dt = scene->GetTime() - event->m_flPrevTime;
		event->m_flPrevTime = scene->GetTime();
		dt = clamp( dt, 0.0, 0.1 );
		cycle = event->m_flPrevCycle + flFrameRate * dt;
		cycle = cycle - (int)cycle;
		event->m_flPrevCycle = cycle;
	}
	else
	{
		float dt = scene->GetTime() - event->GetStartTime();
		cycle = flFrameRate * dt;
		cycle = cycle - (int)(cycle);
	}

	// FIXME: shouldn't sequences always be lower priority than gestures?
	int iLayer = model->GetNewAnimationLayer( a->FindChannelIndex( event->GetChannel() ) );
	model->SetOverlaySequence( iLayer, iSequence, event->GetIntensity( scene->GetTime() ) );
	model->SetOverlayRate( iLayer, cycle, 0.0 );
}


//-----------------------------------------------------------------------------
// Purpose: Apply a walking animation
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessMoveto( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::MOVETO );

	if ( !m_bProcessSequences )
	{
		return;
	}

	StudioModel *model = FindAssociatedModel( scene, event->GetActor()  );
	if ( !model )
		return;

	if ( !event->GetActor() )
		return;

	int iSequence = GetMovetoSequence( scene, event, model );
	if (iSequence < 0)
		return;

	float flFrameRate;
	float flGroundSpeed;
	model->GetSequenceInfo( iSequence, &flFrameRate, &flGroundSpeed );

	float dt = scene->GetTime() - event->GetStartTime();
	float cycle = flFrameRate * dt;
	cycle = cycle - (int)(cycle);

	float idealAccel = 100;

	// accel to ideal
	float t1 = flGroundSpeed / idealAccel;

	float intensity = 1.0;

	if (dt < t1)
	{
		intensity = dt / t1;
	}
	else if (event->GetDuration() - dt < t1)
	{
		intensity = (event->GetDuration() - dt) / t1;
	}

	// movement should always be higher priority than postures, but not gestures....grrr, any way to tell them apart?
	int iLayer = model->GetNewAnimationLayer( 0 /* a->FindChannelIndex( event->GetChannel() ) */ );
	model->SetOverlaySequence( iLayer, iSequence, intensity );
	model->SetOverlayRate( iLayer, cycle, 0.0 );
}



int CChoreoView::GetMovetoSequence( CChoreoScene *scene, CChoreoEvent *event, StudioModel *model )
{
	// FIXME: needs to pull from event (activity or sequence?)
	if ( !event->GetParameters2() || !event->GetParameters2()[0] )
		return model->LookupSequence( "walk_all" );

	// Custom distance styles are appended to param2 with a space as a separator
	const char *pszAct = Q_strstr( event->GetParameters2(), " " );
	if ( pszAct )
	{
		char szActName[256];
		Q_strncpy( szActName, event->GetParameters2(), sizeof(szActName) );
		szActName[ (pszAct-event->GetParameters2()) ] = '\0';
		pszAct = szActName;
	}
	else
	{
		pszAct = event->GetParameters2();
	}

	if ( !Q_strcmp( pszAct, "Walk" ) )
	{
		pszAct = "ACT_WALK";
	}
	else if ( !Q_strcmp( pszAct, "Run" ) )
	{
		pszAct = "ACT_RUN";
	}
	else if ( !Q_strcmp( pszAct, "CrouchWalk" ) )
	{
		pszAct = "ACT_WALK_CROUCH";
	}

	int iSequence = model->LookupActivity( pszAct );

	if (iSequence == -1)
	{
		return model->LookupSequence( "walk_all" );
	}
	return iSequence;
}


//-----------------------------------------------------------------------------
// Purpose: Process a pause event
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessPause( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::SECTION );

	// Don't pause if scrubbing
	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	if ( scrubbing )
		return;

	PauseScene();

	m_bAutomated		= false;
	m_nAutomatedAction	= SCENE_ACTION_UNKNOWN;
	m_flAutomationDelay = 0.0f;
	m_flAutomationTime = 0.0f;

	// Check for auto resume/cancel
	ParseFromMemory( (char *)event->GetParameters(), strlen( event->GetParameters() ) );
	if ( tokenprocessor->TokenAvailable() )
	{
		tokenprocessor->GetToken( false );
		if ( !stricmp( tokenprocessor->CurrentToken(), "automate" ) )
		{
			if ( tokenprocessor->TokenAvailable() )
			{
				tokenprocessor->GetToken( false );
				if ( !stricmp( tokenprocessor->CurrentToken(), "Cancel" ) )
				{
					m_nAutomatedAction = SCENE_ACTION_CANCEL;
				}
				else if ( !stricmp( tokenprocessor->CurrentToken(), "Resume" ) )
				{
					m_nAutomatedAction = SCENE_ACTION_RESUME;
				}

				if ( tokenprocessor->TokenAvailable() && 
					m_nAutomatedAction != SCENE_ACTION_UNKNOWN )
				{
					tokenprocessor->GetToken( false );
					m_flAutomationDelay = (float)atof( tokenprocessor->CurrentToken() );

					if ( m_flAutomationDelay > 0.0f )
					{
						// Success
						m_bAutomated = true;
						m_flAutomationTime = 0.0f;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Main event processor
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !event || !event->GetActive() )
		return;

	CChoreoActor *actor = event->GetActor();
	if ( actor && !actor->GetActive() )
	{
		return;
	}

	CChoreoChannel *channel = event->GetChannel();
	if ( channel && !channel->GetActive() )
	{
		return;
	}

	switch( event->GetType() )
	{
	case CChoreoEvent::EXPRESSION:
		ProcessExpression( scene, event );
		break;
	case CChoreoEvent::FLEXANIMATION:
		ProcessFlexAnimation( scene, event );
		break;
	case CChoreoEvent::LOOKAT:
		ProcessLookat( scene, event );
		break;
	case CChoreoEvent::FACE:
		ProcessFace( scene, event );
		break;
	case CChoreoEvent::GESTURE:
		ProcessGesture( scene, event );
		break;
	case CChoreoEvent::SEQUENCE:
		ProcessSequence( scene, event );
		break;
	case CChoreoEvent::SUBSCENE:
		ProcessSubscene( scene, event );
		break;
	case CChoreoEvent::SPEAK:
		ProcessSpeak( scene, event );
		break;
	case CChoreoEvent::MOVETO:
		ProcessMoveto( scene, event );
		break;
	case CChoreoEvent::STOPPOINT:
		// Nothing
		break;
	case CChoreoEvent::INTERRUPT:
		ProcessInterrupt( scene, event );
		break;
	case CChoreoEvent::PERMIT_RESPONSES:
		ProcessPermitResponses( scene, event );
		break;
	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Main event completion checker
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::CheckEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !event || !event->GetActive() )
		return true;

	CChoreoActor *actor = event->GetActor();
	if ( actor && !actor->GetActive() )
	{
		return true;
	}

	CChoreoChannel *channel = event->GetChannel();
	if ( channel && !channel->GetActive() )
	{
		return true;
	}

	switch( event->GetType() )
	{
	case CChoreoEvent::EXPRESSION:
		break;
	case CChoreoEvent::FLEXANIMATION:
		break;
	case CChoreoEvent::LOOKAT:
		break;
	case CChoreoEvent::GESTURE:
		break;
	case CChoreoEvent::SEQUENCE:
		break;
	case CChoreoEvent::SUBSCENE:
		break;
	case CChoreoEvent::SPEAK:
		break;
	case CChoreoEvent::MOVETO:
		break;
	case CChoreoEvent::INTERRUPT:
		break;
	case CChoreoEvent::PERMIT_RESPONSES:
		break;
	default:
		break;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::PauseThink( void )
{
	// FIXME:  Game code would check for conditions being met

	if ( !m_bAutomated )
		return;

	m_flAutomationTime += fabs( m_flFrameTime );

	RECT rcPauseRect;
	rcPauseRect.left = 0;
	rcPauseRect.right = w2();
	rcPauseRect.top = GetCaptionHeight() + SCRUBBER_HEIGHT;
	rcPauseRect.bottom = rcPauseRect.top + 10;

	CChoreoWidgetDrawHelper drawHelper( this, 
		rcPauseRect,
		COLOR_CHOREO_BACKGROUND );

	DrawSceneABTicks( drawHelper );

	if ( m_flAutomationDelay > 0.0f &&
		m_flAutomationTime < m_flAutomationDelay )
	{
		char sz[ 256 ];
		sprintf( sz, "Pause %.2f/%.2f", m_flAutomationTime, m_flAutomationDelay );
	
		int textlen = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

		RECT rcText;
		GetScrubHandleRect( rcText, true );

		rcText.left = ( rcText.left + rcText.right ) / 2;
		rcText.left -= ( textlen * 0.5f );
		rcText.right = rcText.left + textlen + 1;

		rcText.top = rcPauseRect.top;
		rcText.bottom = rcPauseRect.bottom;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, COLOR_CHOREO_PLAYBACKTICKTEXT, rcText, sz );

		return;
	}

	// Time to act
	m_bAutomated = false;

	switch ( m_nAutomatedAction )
	{
	case SCENE_ACTION_RESUME:
		m_bPaused = false;
		sound->StopAll();
		break;
	case SCENE_ACTION_CANCEL:
		FinishSimulation();
		break;
	default:
		break;
	}

	m_nAutomatedAction = SCENE_ACTION_UNKNOWN;
	m_flAutomationTime = 0.0f;
	m_flAutomationDelay = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Conclude simulation
//-----------------------------------------------------------------------------
void CChoreoView::FinishSimulation( void )
{
	if ( !m_bSimulating )
		return;

//	m_pScene->ResetSimulation();

	m_bSimulating = false;
	m_bPaused = false;

	sound->StopAll();

	if ( m_bResetSpeedScale )
	{
		m_bResetSpeedScale = false;
		g_viewerSettings.speedScale = m_flLastSpeedScale;
		m_flLastSpeedScale = 0.0f;

		Con_Printf( "Resetting speed scale to %f\n", m_flLastSpeedScale );
	}

	models->ClearOverlaysSequences();

	// redraw();
}

void CChoreoView::SceneThink( float time )
{
	if ( !m_pScene )
		return;

	if ( m_bSimulating )
	{
		if ( m_bPaused )
		{
			PauseThink();
		}
		else
		{
			m_pScene->SetSoundFileStartupLatency( 0.0f );

			models->CheckResetFlexes();

			ResetTargetSettings();

			models->ClearOverlaysSequences();
			
			// Tell scene to go
			m_pScene->Think( time );

			// Move flexes toward their targets
			UpdateCurrentSettings();
		}
	}
	else
	{
		FinishSimulation();
	}

	if ( !ShouldProcessSpeak() )
	{
		bool autoprocess = ShouldAutoProcess();
		bool anyscrub = IsAnyToolScrubbing() ;
		bool anyprocessing = IsAnyToolProcessing();

		//Con_Printf( "autoprocess %i anyscrub %i anyprocessing %i\n",
		//	autoprocess ? 1 : 0,
		//	anyscrub ? 1 : 0,
		//	anyprocessing ? 1 : 0 );

		if ( !anyscrub &&
			 !anyprocessing &&
			 autoprocess &&
			 !m_bForceProcess )
		{
			sound->StopAll();

			// why clear lookat?
			//models->ClearModelTargets( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::LayoutScene( void )
{
	if ( !m_pScene )
		return;

	if ( m_bLayoutIsValid )
		return;

	m_pScene->ReconcileTags();

	RECT rc;
	GetClientRect( (HWND)getHandle(), &rc );

	RECT rcClient = rc;
	rcClient.top += GetStartRow();
	OffsetRect( &rcClient, 0, -m_nTopOffset );

	m_flStartTime = m_flLeftOffset / GetPixelsPerSecond();

	m_flEndTime = m_flStartTime + (float)( rcClient.right - GetLabelWidth() ) / GetPixelsPerSecond();

	m_rcTimeLine = rcClient;
	m_rcTimeLine.top = GetCaptionHeight() + SCRUBBER_HEIGHT;
	m_rcTimeLine.bottom = m_rcTimeLine.top + 44;

	int currentRow = rcClient.top + 2;
	int itemHeight;

	// Draw actors
	int i;
	for ( i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		Assert( a );
		if ( !a )
		{
			continue;
		}

		// Figure out rectangle
		itemHeight = a->GetItemHeight();

		RECT rcActor = rcClient;
		rcActor.top = currentRow;
		rcActor.bottom = currentRow + itemHeight;

		a->Layout( rcActor );

		currentRow += itemHeight;
	}

	// Draw section tabs
	for ( i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		CChoreoGlobalEventWidget *e = m_SceneGlobalEvents[ i ];
		if ( !e )
			continue;

		RECT rcEvent;
		rcEvent = m_rcTimeLine;

		float frac = ( e->GetEvent()->GetStartTime() - m_flStartTime ) / ( m_flEndTime - m_flStartTime );
			
		rcEvent.left = GetLabelWidth() + rcEvent.left + (int)( frac * ( m_rcTimeLine.right - m_rcTimeLine.left - GetLabelWidth() ) );
		rcEvent.left -= 4;
		rcEvent.right = rcEvent.left + 8;
		rcEvent.bottom += 0;
		rcEvent.top = rcEvent.bottom - 8;

		if ( rcEvent.left + 10 < GetLabelWidth() )
		{
			e->setVisible( false );
		}
		else
		{
			e->setVisible( true );
		}

	//	OffsetRect( &rcEvent, GetLabelWidth(), 0 );

		e->Layout( rcEvent );
	}

	m_bLayoutIsValid = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::DeleteSceneWidgets( void )
{
	bool oldcandraw = m_bCanDraw;

	m_bCanDraw = false;

	int i;
	CChoreoWidget *w;

	ClearStatusArea();

	for( i = 0 ; i < m_SceneActors.Size(); i++ )
	{
		w = m_SceneActors[ i ];
		m_ActorExpanded[ i ].expanded = ((CChoreoActorWidget *)w)->GetShowChannels();
		delete w;
	}

	m_SceneActors.RemoveAll();

	for( i = 0 ; i < m_SceneGlobalEvents.Size(); i++ )
	{
		w = m_SceneGlobalEvents[ i ];
		delete w;
	}

	m_SceneGlobalEvents.RemoveAll();

	m_bCanDraw = oldcandraw;

	// Make sure nobody is still pointing at us
	m_pClickedActor = NULL;
	m_pClickedChannel = NULL;
	m_pClickedEvent = NULL;
	m_pClickedGlobalEvent = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::InvalidateLayout( void )
{
	if ( m_bSuppressLayout )
		return;

	if ( ComputeHPixelsNeeded() != m_nLastHPixelsNeeded )
	{
		RepositionHSlider();
	}

	if ( ComputeVPixelsNeeded() != m_nLastVPixelsNeeded )
	{
		RepositionVSlider();
	}

	// Recheck gesture start/end times
	if ( m_pScene )
	{
		m_pScene->ReconcileGestureTimes();
		m_pScene->ReconcileCloseCaption();
	}

	m_bLayoutIsValid = false;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::CreateSceneWidgets( void )
{
	DeleteSceneWidgets();

	m_bSuppressLayout = true;

	int i;
	for ( i = 0; i < m_pScene->GetNumActors(); i++ )
	{
		CChoreoActor *a = m_pScene->GetActor( i );
		Assert( a );
		if ( !a )
			continue;

		CChoreoActorWidget *actorWidget = new CChoreoActorWidget( NULL );
		Assert( actorWidget );

		actorWidget->SetActor( a );
		actorWidget->Create();

		m_SceneActors.AddToTail( actorWidget );

		actorWidget->ShowChannels( m_ActorExpanded[ i ].expanded );
	}

	// Find global events
	for ( i = 0; i < m_pScene->GetNumEvents(); i++ )
	{
		CChoreoEvent *e = m_pScene->GetEvent( i );
		if ( !e || e->GetActor() )
			continue;

		CChoreoGlobalEventWidget *eventWidget = new CChoreoGlobalEventWidget( NULL );
		Assert( eventWidget );

		eventWidget->SetEvent( e );
		eventWidget->Create();

		m_SceneGlobalEvents.AddToTail( eventWidget );
	}

	m_bSuppressLayout = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetLabelWidth( void )
{
	return m_nLabelWidth;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetStartRow( void )
{
	return m_nStartRow + GetCaptionHeight() + SCRUBBER_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetRowHeight( void )
{
	return m_nRowHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetFontSize( void )
{
	return m_nFontSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::ComputeVPixelsNeeded( void )
{
	int pixels = 0;
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		pixels += actor->GetItemHeight() + 2;
	}

	pixels += GetStartRow() + 15;

//	pixels += m_nInfoHeight;

	//pixels += 30;
	return pixels;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::ComputeHPixelsNeeded( void )
{
	if ( !m_pScene )
	{
		return 0;
	}

	int pixels = 0;
	float maxtime = m_pScene->FindStopTime();
	if ( maxtime < 5.0 )
	{
		maxtime = 5.0f;
	}
	pixels = (int)( ( maxtime + 5.0 ) * GetPixelsPerSecond() );

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::RepositionVSlider( void )
{
	int pixelsneeded = ComputeVPixelsNeeded();

	if ( pixelsneeded <= ( h2() - GetStartRow() ))
	{
		m_pVertScrollBar->setVisible( false );
		m_nTopOffset = 0;
	}
	else
	{
		m_pVertScrollBar->setVisible( true );
	}

	m_pVertScrollBar->setBounds( w2() - m_nScrollbarHeight, GetStartRow(), m_nScrollbarHeight, h2() - m_nScrollbarHeight - GetStartRow() );

	//int visiblepixels = h2() - m_nScrollbarHeight - GetStartRow();
	//m_nTopOffset = min( pixelsneeded - visiblepixels, m_nTopOffset );
	m_nTopOffset = max( 0, m_nTopOffset );
	m_nTopOffset = min( pixelsneeded, m_nTopOffset );

	m_pVertScrollBar->setRange( 0, pixelsneeded );
	m_pVertScrollBar->setValue( m_nTopOffset );
	m_pVertScrollBar->setPagesize( h2() - GetStartRow() );

	m_nLastVPixelsNeeded = pixelsneeded;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::RepositionHSlider( void )
{
	int pixelsneeded = ComputeHPixelsNeeded();

	int w = w2();
	int lw = GetLabelWidth();

	if ( pixelsneeded <= ( w - lw ) )
	{
		m_pHorzScrollBar->setVisible( false );
	}
	else
	{
		m_pHorzScrollBar->setVisible( true );
	}
	m_pHorzScrollBar->setBounds( 0, h2() - m_nScrollbarHeight, w - m_nScrollbarHeight, m_nScrollbarHeight );

	m_flLeftOffset = max( 0.f, m_flLeftOffset );
	m_flLeftOffset = min( (float)pixelsneeded, m_flLeftOffset );

	m_pHorzScrollBar->setRange( 0, pixelsneeded );
	m_pHorzScrollBar->setValue( (int)m_flLeftOffset );
	m_pHorzScrollBar->setPagesize(w - lw );

	m_nLastHPixelsNeeded = pixelsneeded;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dirty - 
//-----------------------------------------------------------------------------
void CChoreoView::SetDirty( bool dirty, bool clearundo /*=true*/ )
{
	bool changed = dirty != m_bDirty;

	m_bDirty = dirty;

	if ( !dirty && clearundo )
	{
		WipeUndo();
		redraw();
	}

	if ( changed )
	{
		SetPrefix( m_bDirty ? "* " : "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::New( void )
{
	if ( m_pScene )
	{
		Close( );
		if ( m_pScene )
		{
			return;
		}
	}

	char scenefile[ 512 ];
	if ( FacePoser_ShowSaveFileNameDialog( scenefile, sizeof( scenefile ), "scenes", "*.vcd" ) )
	{
		Q_DefaultExtension( scenefile, ".vcd", sizeof( scenefile ) );
		
		m_pScene = new CChoreoScene( this );
		g_MDLViewer->InitGridSettings();
		SetChoreoFile( scenefile );
		m_pScene->SetPrintFunc( Con_Printf );

		ShowButtons( true );

		SetDirty( false );
	}

	if ( !m_pScene )
		return;

	// Get first actor name
	CActorParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Create Actor" );
	strcpy( params.m_szName, "" );

	if ( !ActorProperties( &params ) )
		return;

	if ( strlen( params.m_szName ) <= 0 )
		return;

	SetDirty( true );

	PushUndo( "Create Actor" );

	Con_Printf( "Creating scene %s with actor '%s'\n", GetChoreoFile(), params.m_szName );

	CChoreoActor *actor = m_pScene->AllocActor();
	if ( actor )
	{
		actor->SetName( params.m_szName );
	}

	PushRedo( "Create Actor" );

	CreateSceneWidgets();
	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::Save( void )
{
	if ( !m_pScene )
		return;

	if ( !MakeFileWriteablePrompt( GetChoreoFile(), "VCD File" ) )
	{
		Con_Printf( "Not saving changes to %s\n", GetChoreoFile() );
		return;
	}

	Con_Printf( "Saving changes to %s\n", GetChoreoFile() );

	CP4AutoEditAddFile checkout( GetChoreoFile() );
	if ( !m_pScene->SaveToFile( GetChoreoFile() ) )
	{
  		mxMessageBox( this, va( "Unable to write \"%s\"", GetChoreoFile() ),
  			"SaveToFile", MX_MB_OK | MX_MB_ERROR );
	}

	g_MDLViewer->OnVCDSaved();

	// Refresh the suffix
	SetChoreoFile( GetChoreoFile() );

	SetDirty( false, false );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::SaveAs( void )
{
	if ( !m_pScene )
		return;

	char scenefile[ 512 ];
	if ( !FacePoser_ShowSaveFileNameDialog( scenefile, sizeof( scenefile ), "scenes", "*.vcd" ) )
		return;

	Q_DefaultExtension( scenefile, ".vcd", sizeof( scenefile ) );
	
	Con_Printf( "Saving %s\n", scenefile );

	MakeFileWriteable( scenefile );

	// Change filename
	SetChoreoFile( scenefile );

	// Write it out baby
	CP4AutoEditAddFile checkout( scenefile );
	if (!m_pScene->SaveToFile( GetChoreoFile() ))
	{
  		mxMessageBox( this, va( "Unable to write \"%s\"", GetChoreoFile() ),
  			"SaveToFile", MX_MB_OK | MX_MB_ERROR );
	}

	g_MDLViewer->OnVCDSaved();

	SetDirty( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::Load( void )
{
	char scenefile[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( scenefile, sizeof( scenefile ), "scenes", "*.vcd" ) )
	{
		return;
	}

	Q_DefaultExtension( scenefile, ".vcd", sizeof( scenefile ) );

	LoadSceneFromFile( scenefile );

	m_nextFileList.RemoveAll();
}

void CChoreoView::LoadNext( void )
{
	if (GetChoreoFile() == NULL)
		return;

	char fixedupFile[ 512 ];
	V_FixupPathName( fixedupFile, sizeof( fixedupFile ), GetChoreoFile() );

	char relativeFile[ 512 ];
	filesystem->FullPathToRelativePath( fixedupFile, relativeFile, sizeof( relativeFile ) );

	char relativePath[ 512 ];
	Q_ExtractFilePath( relativeFile, relativePath, sizeof( relativePath ) );

	if (m_nextFileList.Count() == 0)
	{
		// iterate files in the local directory
		char path[ 512 ];
		strcpy( path, relativePath );
		strcat( path, "/*.vcd" );

		FileFindHandle_t hFindFile;
		char const *fn = filesystem->FindFirstEx( path, "MOD", &hFindFile );
		if ( fn )
		{
			while ( fn )
			{
				// Don't do anything with directories
				if ( !filesystem->FindIsDirectory( hFindFile ) )
				{
					CUtlString s = fn;
					m_nextFileList.AddToTail( s );
				}

				fn = filesystem->FindNext( hFindFile );
			}

			filesystem->FindClose( hFindFile );
		}
	}

	// look for a match, then pick the next in the list
	const char *fileBase;
	fileBase = V_UnqualifiedFileName( fixedupFile );

	for (int i = 0; i < m_nextFileList.Count(); i++)
	{
		if (!stricmp( fileBase, m_nextFileList[i] ))
		{
			char fileName[512];
			strcpy( fileName, relativePath );
			if (i < m_nextFileList.Count() - 1)
			{
				strcat( fileName, m_nextFileList[i+1] );
			}
			else
			{
				strcat( fileName, m_nextFileList[0] );
			}

			LoadSceneFromFile( fileName );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CChoreoView::LoadSceneFromFile( const char *filename )
{
	if ( filename[ 0 ] == '/' ||
		 filename[ 0 ] == '\\' )
	{
		++filename;
	}

	char fn[ 512 ];
	Q_strncpy( fn, filename, sizeof( fn ) );
	if ( m_pScene )
	{
		Close();
		if ( m_pScene )
		{
			return;
		}
	}

	m_pScene = LoadScene( fn );
	g_MDLViewer->InitGridSettings();
	if ( !m_pScene )
		return;

	g_MDLViewer->OnFileLoaded( fn );

	ShowButtons( true );

	CChoreoWidget::m_pScene = m_pScene;
	SetChoreoFile( fn );

	bool cleaned = FixupSequenceDurations( m_pScene, false );

	SetDirty( cleaned );

	DeleteSceneWidgets();
	CreateSceneWidgets();

	// Force scroll bars to recompute
	ForceScrollBarsToRecompute( false );

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : closing - 
//-----------------------------------------------------------------------------
void CChoreoView::UnloadScene( void )
{
	InvalidateLayout();
	ReportSceneClearToTools();

	ClearStatusArea();

	delete m_pScene;
	m_pScene = NULL;
	SetDirty( false );
	SetChoreoFile( "" );
	g_MDLViewer->InitGridSettings();
	CChoreoWidget::m_pScene = NULL;

	DeleteSceneWidgets();

	m_pVertScrollBar->setVisible( false );
	m_pHorzScrollBar->setVisible( false );

	ShowButtons( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoView::DeleteChannel( CChoreoChannel *channel )
{
	if ( !channel || !m_pScene )
		return;

	SetDirty( true );

	PushUndo( "Delete Channel" );

	DeleteSceneWidgets();

	// Delete channel and it's children
	// Find the appropriate actor
	for ( int i = 0; i < m_pScene->GetNumActors(); i++ )
	{
		CChoreoActor *a = m_pScene->GetActor( i );
		if ( !a )
			continue;

		if ( a->FindChannelIndex( channel ) == -1 )
			continue;

		Con_Printf( "Deleting %s\n", channel->GetName() );
		a->RemoveChannel( channel );
			
		m_pScene->DeleteReferencedObjects( channel );
		break;
	}

	ReportSceneClearToTools();

	CreateSceneWidgets();

	PushRedo( "Delete Channel" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::NewChannel( void )
{
	if ( !m_pScene )
		return;

	if ( !m_pScene->GetNumActors() )
	{
		Con_Printf( "You must create an actor before you can add a channel\n" );
		return;
	}

	CChannelParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Create Channel" );
	strcpy( params.m_szName, "" );
	params.m_bShowActors = true;
	strcpy( params.m_szSelectedActor, "" );
	params.m_pScene = m_pScene;

	if ( !ChannelProperties( &params ) )
	{
		return;
	}

	if ( strlen( params.m_szName ) <= 0 )
	{
		return;
	}

	CChoreoActor *actor = m_pScene->FindActor( params.m_szSelectedActor );
	if ( !actor )
	{
		Con_Printf( "Can't add channel %s, actor %s doesn't exist\n", params.m_szName, params.m_szSelectedActor );
		return;
	}

	SetDirty( true );

	PushUndo( "Add Channel" );

	DeleteSceneWidgets();

	CChoreoChannel *channel = m_pScene->AllocChannel();
	if ( !channel )
	{
		Con_Printf( "Unable to allocate channel %s!\n", params.m_szName );
	}
	else
	{
		channel->SetName( params.m_szName );
		channel->SetActor( actor );
		actor->AddChannel( channel );
	}

	CreateSceneWidgets();

	PushRedo( "Add Channel" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoView::MoveChannelUp( CChoreoChannel *channel )
{
	SetDirty( true );

	PushUndo( "Move Channel Up" );

	DeleteSceneWidgets();

	// Find the appropriate actor
	for ( int i = 0; i < m_pScene->GetNumActors(); i++ )
	{
		CChoreoActor *a = m_pScene->GetActor( i );
		if ( !a )
			continue;

		int index = a->FindChannelIndex( channel );
		if ( index == -1 )
			continue;

		if ( index != 0 )
		{
			Con_Printf( "Moving %s up\n", channel->GetName() );
			a->SwapChannels( index, index - 1 );
		}
		break;
	}

	CreateSceneWidgets();

	PushRedo( "Move Channel Up" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoView::MoveChannelDown( CChoreoChannel *channel )
{
	SetDirty( true );

	PushUndo( "Move Channel Down" );

	DeleteSceneWidgets();

	// Find the appropriate actor
	for ( int i = 0; i < m_pScene->GetNumActors(); i++ )
	{
		CChoreoActor *a = m_pScene->GetActor( i );
		if ( !a )
			continue;

		int index = a->FindChannelIndex( channel );
		if ( index == -1 )
			continue;

		if ( index < a->GetNumChannels() - 1 )
		{
			Con_Printf( "Moving %s down\n", channel->GetName() );
			a->SwapChannels( index, index + 1 );
		}
		break;
	}

	CreateSceneWidgets();

	PushRedo( "Move Channel Down" );

	// Redraw
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoView::EditChannel( CChoreoChannel *channel )
{
	if ( !channel )
		return;

	CChannelParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Edit Channel" );
	V_strcpy_safe( params.m_szName, channel->GetName() );

	if ( !ChannelProperties( &params ) )
		return;

	if ( strlen( params.m_szName ) <= 0 )
		return;

	SetDirty( true );

	PushUndo( "Edit Channel" );

	channel->SetName( params.m_szName );

	PushRedo( "Edit Channel" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CChoreoView::DeleteActor( CChoreoActor *actor )
{
	if ( !actor || !m_pScene )
		return;

	SetDirty( true );

	PushUndo( "Delete Actor" );

	DeleteSceneWidgets();

	// Delete channel and it's children
	// Find the appropriate actor
	Con_Printf( "Deleting %s\n", actor->GetName() );
	m_pScene->RemoveActor( actor );

	m_pScene->DeleteReferencedObjects( actor );

	ReportSceneClearToTools();

	CreateSceneWidgets();

	PushRedo( "Delete Actor" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::NewActor( void )
{
	if ( !m_pScene )
	{
		Con_ErrorPrintf( "You must load or create a scene file first\n" );
		return;
	}

	CActorParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Create Actor" );
	strcpy( params.m_szName, "" );

	if ( !ActorProperties( &params ) )
		return;

	if ( strlen( params.m_szName ) <= 0 )
		return;

	SetDirty( true );

	PushUndo( "Add Actor" );

	DeleteSceneWidgets();

	Con_Printf( "Adding new actor '%s'\n", params.m_szName );

	CChoreoActor *actor = m_pScene->AllocActor();
	if ( actor )
	{
		actor->SetName( params.m_szName );
	}

	CreateSceneWidgets();

	PushRedo( "Add Actor" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CChoreoView::MoveActorUp( CChoreoActor *actor )
{
	DeleteSceneWidgets();

	int index = m_pScene->FindActorIndex( actor );
	// found it and it's not first
	if ( index != -1 && index != 0 )
	{
		Con_Printf( "Moving %s up\n", actor->GetName() );

		SetDirty( true );

		PushUndo( "Move Actor Up" );

		m_pScene->SwapActors( index, index - 1 );

		PushRedo( "Move Actor Up" );
	}

	CreateSceneWidgets();
	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CChoreoView::MoveActorDown( CChoreoActor *actor )
{
	DeleteSceneWidgets();

	int index = m_pScene->FindActorIndex( actor );
	// found it and it's not first
	if ( index != -1 && ( index < m_pScene->GetNumActors() - 1 ) )
	{
		Con_Printf( "Moving %s down\n", actor->GetName() );
		
		SetDirty( true );
	
		PushUndo( "Move Actor Down" );

		m_pScene->SwapActors( index, index + 1 );

		PushRedo( "Move Actor Down" );
	}

	CreateSceneWidgets();
	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CChoreoView::EditActor( CChoreoActor *actor )
{
	if ( !actor )
		return;

	CActorParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Edit Actor" );
	V_strcpy_safe( params.m_szName, actor->GetName() );

	if ( !ActorProperties( &params ) )
		return;

	if ( strlen( params.m_szName ) <= 0 )
		return;

	SetDirty( true );

	PushUndo( "Edit Actor" );

	actor->SetName( params.m_szName );

	PushRedo( "Edit Actor" );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void CChoreoView::AddEvent( int type, int subtype /*= 0*/, char const *defaultparameters /*= NULL*/ )
{
	int mx, my;
	mx = m_nClickedX;
	my = m_nClickedY;
	CChoreoChannelWidget *channel = m_pClickedChannel;
	if ( !channel || !channel->GetChannel() )
	{
		CChoreoActorWidget *actor = m_pClickedActor;
		if ( actor )
		{
			if ( actor->GetNumChannels() <= 0 )
				return;

			channel = actor->GetChannel( 0 );
			if ( !channel || !channel->GetChannel() )
				return;
		}
		else
		{
			return;
		}
	}

	// Convert click position local to this window
	POINT pt;
	pt.x = mx;
	pt.y = my;

	CEventParams params;
	memset( &params, 0, sizeof( params ) );

	if ( defaultparameters )
	{
		Q_strncpy( params.m_szParameters, defaultparameters, sizeof( params.m_szParameters ) );
	}

	strcpy( params.m_szDialogTitle, "Create Event" );

	params.m_nType = type;
	params.m_pScene = m_pScene;

	params.m_bFixedLength = false;
	params.m_bResumeCondition = false;
	params.m_flStartTime = GetTimeValueForMouse( pt.x );
	params.m_bCloseCaptionNoAttenuate = false;
	params.m_bForceShortMovement = false;
	params.m_bSyncToFollowingGesture = false;
	params.m_bDisabled = false;
	params.m_bPlayOverScript = false;

	switch ( type )
	{
	case CChoreoEvent::EXPRESSION:
	case CChoreoEvent::FLEXANIMATION:
	case CChoreoEvent::GESTURE:
	case CChoreoEvent::SEQUENCE:
	case CChoreoEvent::LOOKAT:
	case CChoreoEvent::MOVETO:
	case CChoreoEvent::FACE:
	case CChoreoEvent::SUBSCENE:
	case CChoreoEvent::INTERRUPT:
	case CChoreoEvent::GENERIC:
	case CChoreoEvent::PERMIT_RESPONSES:
		params.m_bHasEndTime = true;
		params.m_flEndTime = params.m_flStartTime + 0.5f;
		if ( type == CChoreoEvent::GESTURE && subtype == 1 )
		{
			strcpy( params.m_szDialogTitle, "Create <NULL> Gesture" );
			strcpy( params.m_szName, "NULL" );
		}
		break;
	case CChoreoEvent::SPEAK:
		params.m_bFixedLength = true;
		params.m_bHasEndTime = false;
		params.m_flEndTime = -1.0f;
		break;
	default:
		params.m_bHasEndTime = false;
		params.m_flEndTime = -1.0f;
		break;
	}

	params.m_bUsesTag = false;

	while (1)
	{
		SetScrubTargetTime( m_flScrub );
		FinishSimulation();
		sound->Flush();

		m_bForceProcess = true;
		if (!EventProperties( &params ))
		{
			m_bForceProcess = false;
			return;
		}
		m_bForceProcess = false;

		if ( Q_strlen( params.m_szName ) <= 0 )
		{
			mxMessageBox( this, va( "Event must have a valid name" ),
				"Edit Event", MX_MB_OK | MX_MB_ERROR );
			continue;
		}

		if ( Q_strlen( params.m_szParameters ) <= 0 )
		{
			bool shouldBreak = false;

			switch ( params.m_nType )
			{
			case CChoreoEvent::FLEXANIMATION:
			case CChoreoEvent::INTERRUPT:
			case CChoreoEvent::PERMIT_RESPONSES:
				shouldBreak = true;
				break;
			case CChoreoEvent::GESTURE:
				if ( subtype == 1 )
				{
					shouldBreak = true;
				}
				break;
			default:
				// Have to have a non-null parameters block
				break;
			}
			
			if ( !shouldBreak )
			{
				mxMessageBox( this, va( "No parameters specified for %s\n", params.m_szName ),
					"Edit Event", MX_MB_OK | MX_MB_ERROR );
				continue;
			}
		}
		
		break;
	}

	SetDirty( true );

	PushUndo( "Add Event" );

	CChoreoEvent *event = m_pScene->AllocEvent();
	if ( event )
	{
		event->SetType( (CChoreoEvent::EVENTTYPE)type );
		event->SetName( params.m_szName );
		event->SetParameters( params.m_szParameters );
		event->SetParameters2( params.m_szParameters2 );
		event->SetParameters3( params.m_szParameters3 );
		event->SetStartTime( params.m_flStartTime );

		event->SetResumeCondition( params.m_bResumeCondition );
		event->SetLockBodyFacing( params.m_bLockBodyFacing );
		event->SetDistanceToTarget( params.m_flDistanceToTarget );
		event->SetForceShortMovement( params.m_bForceShortMovement );
		event->SetSyncToFollowingGesture( params.m_bSyncToFollowingGesture );
		event->SetActive( !params.m_bDisabled );
		event->SetPlayOverScript( params.m_bPlayOverScript );

		if ( params.m_bUsesTag )
		{
			event->SetUsingRelativeTag( true, params.m_szTagName, params.m_szTagWav );
		}
		else
		{
			event->SetUsingRelativeTag( false );
		}
		CChoreoChannel *pchannel = channel->GetChannel();

		event->SetChannel( pchannel );
		event->SetActor( pchannel->GetActor() );

		if ( params.m_bHasEndTime &&
			params.m_flEndTime != -1.0 &&
			params.m_flEndTime > params.m_flStartTime )
		{
			event->SetEndTime( params.m_flEndTime );
		}
		else
		{
			event->SetEndTime( -1.0f );
		}

		switch ( event->GetType() )
		{
		default:
			break;
		case CChoreoEvent::SUBSCENE:
			{
				// Just grab end time
				CChoreoScene *scene = LoadScene( event->GetParameters() );
				if ( scene )
				{
					event->SetEndTime( params.m_flStartTime + scene->FindStopTime() );
				}
				delete scene;
			}
			break;
		case CChoreoEvent::SEQUENCE:
			{
				CheckSequenceLength( event, false );
				// AutoaddSequenceKeys( event);
			}
			break;
		case CChoreoEvent::GESTURE:
			{
				DefaultGestureLength( event, false );
				AutoaddGestureKeys( event, false );
			}
			break;
		case CChoreoEvent::LOOKAT:
		case CChoreoEvent::FACE:
			{
				if ( params.usepitchyaw )
				{
					event->SetPitch( params.pitch );
					event->SetYaw( params.yaw );
				}
				else
				{
					event->SetPitch( 0 );
					event->SetYaw( 0 );
				}
			}
			break;
		case CChoreoEvent::SPEAK:
			{
				// Try and load wav to get length
				CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( event ) ) );
				if ( wave )
				{
					event->SetEndTime( params.m_flStartTime + wave->GetRunningLength() );
					delete wave;
				}

				event->SetSuppressingCaptionAttenuation( params.m_bCloseCaptionNoAttenuate );
			}
			break;
		}
		event->SnapTimes();

		DeleteSceneWidgets();

		// Add to appropriate channel
		pchannel->AddEvent( event );

		CreateSceneWidgets();

		// Redraw
		InvalidateLayout();
	}

	PushRedo( "Add Event" );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a scene "pause" event
//-----------------------------------------------------------------------------
void CChoreoView::AddGlobalEvent( CChoreoEvent::EVENTTYPE type )
{
	int mx, my;
	mx = m_nClickedX;
	my = m_nClickedY;

	// Convert click position local to this window
	POINT pt;
	pt.x = mx;
	pt.y = my;

	CGlobalEventParams params;
	memset( &params, 0, sizeof( params ) );

	params.m_nType = type;

	switch ( type )
	{
	default:
		Assert( 0 );
		strcpy( params.m_szDialogTitle, "???" );
		break;
	case CChoreoEvent::SECTION:
		{
			strcpy( params.m_szDialogTitle, "Add Pause Point" );
		}
		break;
	case CChoreoEvent::LOOP:
		{
			strcpy( params.m_szDialogTitle, "Add Loop Point" );
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			strcpy( params.m_szDialogTitle, "Add Fire Completion" );
		}
		break;
	}
	strcpy( params.m_szName, "" );
	strcpy( params.m_szAction, "" );

	params.m_flStartTime = GetTimeValueForMouse( pt.x );

	if ( !GlobalEventProperties( &params ) )
		return;

	if ( strlen( params.m_szName ) <= 0 )
	{
		Con_Printf( "Pause section event must have a valid name\n" );
		return;
	}

	if ( strlen( params.m_szAction ) <= 0 )
	{
		Con_Printf( "No action specified for section pause\n" );
		return;
	}

	char undotext[ 256 ];
	undotext[0]=0;
	switch( type )
	{
	default:
		Assert( 0 );
		break;
	case CChoreoEvent::SECTION:
		{
			Q_strcpy( undotext, "Add Section Pause" );
		}
		break;
	case CChoreoEvent::LOOP:
		{
			Q_strcpy( undotext, "Add Loop Point" );
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			Q_strcpy( undotext, "Add Fire Completion" );
		}
		break;
	}

	SetDirty( true );

	PushUndo( undotext );

	CChoreoEvent *event = m_pScene->AllocEvent();
	if ( event )
	{
		event->SetType( type );
		event->SetName( params.m_szName );
		event->SetParameters( params.m_szAction );
		event->SetStartTime( params.m_flStartTime );
		event->SetEndTime( -1.0f );

		switch ( type )
		{
		default:
			break;
		case CChoreoEvent::LOOP:
			{
				event->SetLoopCount( params.m_nLoopCount );
				event->SetParameters( va( "%f", params.m_flLoopTime ) );
			}
			break;
		}

		event->SnapTimes();

		DeleteSceneWidgets();

		CreateSceneWidgets();

		// Redraw
		InvalidateLayout();
	}

	PushRedo( undotext );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::EditGlobalEvent( CChoreoEvent *event )
{
	if ( !event )
		return;

	CGlobalEventParams params;
	memset( &params, 0, sizeof( params ) );

	params.m_nType = event->GetType();

	switch ( event->GetType() )
	{
	default:
		Assert( 0 );
		strcpy( params.m_szDialogTitle, "???" );
		break;
	case CChoreoEvent::SECTION:
		{
			strcpy( params.m_szDialogTitle, "Edit Pause Point" );
			V_strcpy_safe( params.m_szAction, event->GetParameters() );
		}
		break;
	case CChoreoEvent::LOOP:
		{
			strcpy( params.m_szDialogTitle, "Edit Loop Point" );
			strcpy( params.m_szAction, "" );
			params.m_flLoopTime = (float)atof( event->GetParameters() );
			params.m_nLoopCount = event->GetLoopCount();
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			strcpy( params.m_szDialogTitle, "Edit Fire Completion" );
			strcpy( params.m_szAction, "" );
		}
		break;
	}

	strcpy( params.m_szName, event->GetName() );

	params.m_flStartTime = event->GetStartTime();

	if ( !GlobalEventProperties( &params ) )
		return;

	if ( strlen( params.m_szName ) <= 0 )
	{
		Con_Printf( "Event %s must have a valid name\n", event->GetName() );
		return;
	}

	if ( strlen( params.m_szAction ) <= 0 )
	{
		Con_Printf( "No action specified for %s\n", event->GetName() );
		return;
	}

	SetDirty( true );

	char undotext[ 256 ];
	undotext[0]=0;
	switch( event->GetType() )
	{
	default:
		Assert( 0 );
		break;
	case CChoreoEvent::SECTION:
		{
			Q_strcpy( undotext, "Edit Section Pause" );
		}
		break;
	case CChoreoEvent::LOOP:
		{
			Q_strcpy( undotext, "Edit Loop Point" );
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			Q_strcpy( undotext, "Edit Fire Completion" );
		}
		break;
	}

	PushUndo( undotext );

	event->SetName( params.m_szName );
	event->SetStartTime( params.m_flStartTime );
	event->SetEndTime( -1.0f );

	switch ( event->GetType() )
	{
	default:
		{
			event->SetParameters( params.m_szAction );
		}
		break;
	case CChoreoEvent::LOOP:
		{
			event->SetLoopCount( params.m_nLoopCount );
			event->SetParameters( va( "%f", params.m_flLoopTime ) );
		}
		break;
	}

	event->SnapTimes();

	PushRedo( undotext );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::DeleteGlobalEvent( CChoreoEvent *event )
{
	if ( !event || !m_pScene )
		return;

	SetDirty( true );


	char undotext[ 256 ];
	undotext[0]=0;
	switch( event->GetType() )
	{
	default:
		Assert( 0 );
		break;
	case CChoreoEvent::SECTION:
		{
			Q_strcpy( undotext, "Delete Section Pause" );
		}
		break;
	case CChoreoEvent::LOOP:
		{
			Q_strcpy( undotext, "Delete Loop Point" );
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			Q_strcpy( undotext, "Delete Fire Completion" );
		}
		break;
	}

	PushUndo( undotext );

	DeleteSceneWidgets();

	Con_Printf( "Deleting %s\n", event->GetName() );

	m_pScene->DeleteReferencedObjects( event );

	CreateSceneWidgets();

	PushRedo( undotext );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::EditEvent( CChoreoEvent *event )
{
	if ( !event )
		return;

	CEventParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Edit Event" );

	// Copy in current even properties
	params.m_nType = event->GetType();
	params.m_bDisabled = !event->GetActive();

	switch ( params.m_nType )
	{
		case CChoreoEvent::EXPRESSION:
		case CChoreoEvent::SEQUENCE:
		case CChoreoEvent::MOVETO:
		case CChoreoEvent::SPEAK:
		case CChoreoEvent::GESTURE:
		case CChoreoEvent::INTERRUPT:
		case CChoreoEvent::PERMIT_RESPONSES:
		case CChoreoEvent::GENERIC:
			V_strcpy_safe( params.m_szParameters3, event->GetParameters3() );
			V_strcpy_safe( params.m_szParameters2, event->GetParameters2() );
			V_strcpy_safe( params.m_szParameters, event->GetParameters() );
			V_strcpy_safe( params.m_szName, event->GetName() );
			break;
		case CChoreoEvent::FACE:
		case CChoreoEvent::LOOKAT:
		case CChoreoEvent::FIRETRIGGER:
		case CChoreoEvent::FLEXANIMATION:
		case CChoreoEvent::SUBSCENE:
			V_strcpy_safe( params.m_szParameters, event->GetParameters() );
			V_strcpy_safe( params.m_szName, event->GetName() );

			if ( params.m_nType == CChoreoEvent::LOOKAT || params.m_nType == CChoreoEvent::FACE )
			{
				if ( event->GetPitch() != 0 ||
					 event->GetYaw() != 0 )
				{
					params.usepitchyaw = true;
					params.pitch = event->GetPitch();
					params.yaw = event->GetYaw();
				}
			}
			break;
		default:
			Con_Printf( "Don't know how to edit event type %s\n",
				CChoreoEvent::NameForType( (CChoreoEvent::EVENTTYPE)params.m_nType ) );
			return;
	}

	params.m_pScene = m_pScene;
	params.m_pEvent = event;
	params.m_flStartTime = event->GetStartTime();
	params.m_flEndTime = event->GetEndTime();
	params.m_bHasEndTime = event->HasEndTime();

	params.m_bFixedLength = event->IsFixedLength();
	params.m_bResumeCondition = event->IsResumeCondition();
	params.m_bLockBodyFacing = event->IsLockBodyFacing();
	params.m_flDistanceToTarget = event->GetDistanceToTarget();
	params.m_bForceShortMovement = event->GetForceShortMovement();
	params.m_bSyncToFollowingGesture = event->GetSyncToFollowingGesture();
	params.m_bPlayOverScript = event->GetPlayOverScript();
	params.m_bUsesTag = event->IsUsingRelativeTag();
	params.m_bCloseCaptionNoAttenuate = event->IsSuppressingCaptionAttenuation();

	if ( params.m_bUsesTag )
	{
		V_strcpy_safe( params.m_szTagName, event->GetRelativeTagName() );
		V_strcpy_safe( params.m_szTagWav, event->GetRelativeWavName() );
	}

	while (1)
	{
		SetScrubTargetTime( m_flScrub );
		FinishSimulation();
		sound->Flush();

		m_bForceProcess = true;
		if (!EventProperties( &params ))
		{
			m_bForceProcess = false;
			return;
		}
		m_bForceProcess = false;

		if ( Q_strlen( params.m_szName ) <= 0 )
		{
			mxMessageBox( this, va( "Event %s must have a valid name", event->GetName() ),
				"Edit Event", MX_MB_OK | MX_MB_ERROR );
			continue;
		}

		if ( Q_strlen( params.m_szParameters ) <= 0 )
		{
			bool shouldBreak = false;

			switch ( params.m_nType )
			{
			case CChoreoEvent::FLEXANIMATION:
			case CChoreoEvent::INTERRUPT:
			case CChoreoEvent::PERMIT_RESPONSES:
				shouldBreak = true;
				break;
			case CChoreoEvent::GESTURE:
				if ( !Q_stricmp( params.m_szName, "NULL" ) )
				{ 
					shouldBreak = true;
				}
				break;
			default:
				// Have to have a non-null parameters block
				break;
			}
			
			if ( !shouldBreak )
			{
				mxMessageBox( this, va( "No parameters specified for %s\n", params.m_szName ),
					"Edit Event", MX_MB_OK | MX_MB_ERROR );
				continue;
			}
		}
		
		break;
	}


	SetDirty( true );

	PushUndo( "Edit Event" );

	event->SetName( params.m_szName );
	event->SetParameters( params.m_szParameters );
	event->SetParameters2( params.m_szParameters2 );
	event->SetParameters3( params.m_szParameters3 );
	event->SetStartTime( params.m_flStartTime );
	event->SetResumeCondition( params.m_bResumeCondition );
	event->SetLockBodyFacing( params.m_bLockBodyFacing );
	event->SetDistanceToTarget( params.m_flDistanceToTarget );
	event->SetForceShortMovement( params.m_bForceShortMovement );
	event->SetSyncToFollowingGesture( params.m_bSyncToFollowingGesture );
	event->SetActive( !params.m_bDisabled );
	event->SetPlayOverScript( params.m_bPlayOverScript );
	if ( params.m_bUsesTag )
	{
		event->SetUsingRelativeTag( true, params.m_szTagName, params.m_szTagWav );
	}
	else
	{
		event->SetUsingRelativeTag( false );
	}

	if ( params.m_bHasEndTime &&
		params.m_flEndTime != -1.0 &&
		params.m_flEndTime > params.m_flStartTime )
	{
		float dt = params.m_flEndTime - event->GetEndTime();
		float newduration = event->GetDuration() + dt;
		RescaleRamp( event, newduration );
		switch ( event->GetType() )
		{
		default:
			break;
		case CChoreoEvent::GESTURE:
			{
				event->RescaleGestureTimes( event->GetStartTime(), event->GetEndTime() + dt, true );
			}
			break;
		case CChoreoEvent::FLEXANIMATION:
			{
				RescaleExpressionTimes( event, event->GetStartTime(), event->GetEndTime() + dt );
			}
			break;
		}
		event->SetEndTime( params.m_flEndTime );
		event->SnapTimes();
		event->ResortRamp();
	}
	else
	{
		event->SetEndTime( -1.0f );
	}

	switch ( event->GetType() )
	{
	default:
		break;
	case CChoreoEvent::SPEAK:
		{
			// Try and load wav to get length
			CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( event ) ) );
			if ( wave )
			{
				event->SetEndTime( params.m_flStartTime + wave->GetRunningLength() );
				delete wave;
			}

			event->SetSuppressingCaptionAttenuation( params.m_bCloseCaptionNoAttenuate );
		}
		break;
	case CChoreoEvent::SUBSCENE:
		{
			// Just grab end time
			CChoreoScene *scene = LoadScene( event->GetParameters() );
			if ( scene )
			{
				event->SetEndTime( params.m_flStartTime + scene->FindStopTime() );
			}
			delete scene;
		}
		break;
	case CChoreoEvent::SEQUENCE:
		{
			CheckSequenceLength( event, false );
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			CheckGestureLength( event, false );
			AutoaddGestureKeys( event, false );
			g_pGestureTool->redraw();
		}
		break;
	case CChoreoEvent::LOOKAT:
	case CChoreoEvent::FACE:
		{
			if ( params.usepitchyaw )
			{
				event->SetPitch( params.pitch );
				event->SetYaw( params.yaw );
			}
			else
			{
				event->SetPitch( 0 );
				event->SetYaw( 0 );
			}
		}
		break;
	}

	event->SnapTimes();

	PushRedo( "Edit Event" );

	// Redraw
	InvalidateLayout();
}

void CChoreoView::EnableSelectedEvents( bool state )
{
	if ( !m_pScene )
		return;

	SetDirty( true );

	// If we right clicked on an unseleced event, then select it for the user.
	if ( CountSelectedEvents() == 0 )
	{
		CChoreoEventWidget *event = m_pClickedEvent;
		if ( event )
		{
			event->SetSelected( true );
		}
	}

	char const *desc = state ? "Enable Events" : "Disable Events";

	PushUndo( desc );

	// Find the appropriate event by iterating across all actors and channels
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = a->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = channel->GetNumEvents() - 1; k >= 0; k-- )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event->IsSelected() )
					continue;

				event->GetEvent()->SetActive( state );
			}
		}
	}

	for ( int i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( !event || !event->IsSelected() )
			continue;

		event->GetEvent()->SetActive( state );
	}

	PushRedo( desc );

	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::DeleteSelectedEvents( void )
{
	if ( !m_pScene )
		return;

	SetDirty( true );

	PushUndo( "Delete Events" );

	int deleteCount = 0;

	float oldstoptime = m_pScene->FindStopTime();

	// Find the appropriate event by iterating across all actors and channels
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = a->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = channel->GetNumEvents() - 1; k >= 0; k-- )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event->IsSelected() )
					continue;

				channel->GetChannel()->RemoveEvent( event->GetEvent() );
				m_pScene->DeleteReferencedObjects( event->GetEvent() );

				deleteCount++;
			}
		}
	}

	for ( int i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( !event || !event->IsSelected() )
			continue;

		m_pScene->DeleteReferencedObjects( event->GetEvent() );

		deleteCount++;
	}

	DeleteSceneWidgets();

	ReportSceneClearToTools();

	CreateSceneWidgets();

	PushRedo( "Delete Events" );

	Con_Printf( "Deleted <%i> events\n", deleteCount );

	if ( m_pScene->FindStopTime() != oldstoptime )
	{
		// Force scroll bars to recompute
		ForceScrollBarsToRecompute( false );

	}
	// Redraw
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : resetthumb - 
//-----------------------------------------------------------------------------
void CChoreoView::ForceScrollBarsToRecompute( bool resetthumb )
{
	if ( resetthumb )
	{
		m_flLeftOffset = 0.0f;
	}
	m_nLastHPixelsNeeded = -1;
	m_nLastVPixelsNeeded = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
// Output : CChoreoScene
//-----------------------------------------------------------------------------
CChoreoScene *CChoreoView::LoadScene( char const *filename )
{
	// If relative path, then make a full path
	char pFullPathBuf[ MAX_PATH ];
	if ( !Q_IsAbsolutePath( filename ) )
	{
		filesystem->RelativePathToFullPath( filename, "GAME", pFullPathBuf, sizeof( pFullPathBuf ) );
		filename = pFullPathBuf;
	}

	if ( !filesystem->FileExists( filename ) )
		return NULL;

	LoadScriptFile( const_cast<char*>( filename ) );

	CChoreoScene *scene = ChoreoLoadScene( filename, this, tokenprocessor, Con_Printf );
	return scene;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChoreoView::FixupSequenceDurations( CChoreoScene *scene, bool checkonly )
{
	bool bret = false;
	if ( !scene )
		return bret;

	int c = scene->GetNumEvents();
	for ( int i = 0; i < c; i++ )
	{
		CChoreoEvent *event = scene->GetEvent( i );
		if ( !event )
			continue;

		switch ( event->GetType() )
		{
		default:
			break;
		case CChoreoEvent::SPEAK:
			{
				// Try and load wav to get length
				CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( event ) ) );
				if ( wave )
				{
					float endtime = event->GetStartTime() + wave->GetRunningLength();
					if ( event->GetEndTime() != endtime )
					{
						event->SetEndTime( event->GetStartTime() + wave->GetRunningLength() );
						bret = true;
					}
					delete wave;
				}
			}
			break;
		case CChoreoEvent::SEQUENCE:
			{
				if ( CheckSequenceLength( event, checkonly ) )
				{
					bret = true;
				}
			}
			break;
		case CChoreoEvent::GESTURE:
			{
				if ( CheckGestureLength( event, checkonly ) )
				{
					bret = true;
				}
				if ( AutoaddGestureKeys( event, checkonly ) )
				{
					bret = true;
				}
			}
			break;
		}
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: IChoreoEventCallback
// Input  : currenttime - 
//			*event - 
//-----------------------------------------------------------------------------
void CChoreoView::StartEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !event || !event->GetActive() )
		return;

	CChoreoActor *actor = event->GetActor();
	if ( actor && !actor->GetActive() )
	{
		return;
	}

	CChoreoChannel *channel = event->GetChannel();
	if ( channel && !channel->GetActive() )
	{
		return;
	}

	// Con_Printf( "%8.4f:  start %s\n", currenttime, event->GetDescription() );

	switch ( event->GetType() )
	{
	case CChoreoEvent::SEQUENCE:
		{
			ProcessSequence( scene, event );
		}
		break;
	case CChoreoEvent::SUBSCENE:
		{
			if ( !scene->IsSubScene() )
			{
				CChoreoScene *subscene = event->GetSubScene();
				if ( !subscene )
				{
					subscene = LoadScene( event->GetParameters() );
					subscene->SetSubScene( true );
					event->SetSubScene( subscene );
				}

				if ( subscene )
				{
					subscene->ResetSimulation( m_bForward );
				}
			}
		}
		break;
	case CChoreoEvent::SECTION:
		{
			ProcessPause( scene, event );
		}
		break;
	case CChoreoEvent::SPEAK:
		{
			if ( ShouldProcessSpeak() )
			{
				// See if we should trigger CC
				char soundname[ 512 ];
				Q_strncpy( soundname, event->GetParameters(), sizeof( soundname ) );

				float actualEndTime = event->GetEndTime();

				if ( event->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
				{
					char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
					if ( event->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
					{
						float duration = max( event->GetDuration(), event->GetLastSlaveEndTime() - event->GetStartTime() );

						closecaptionmanager->Process( tok, duration, GetCloseCaptionLanguageId() );

						// Use the token as the sound name lookup, too.
						if ( event->IsUsingCombinedFile() &&
							 ( event->GetNumSlaves() > 0 ) ) 
						{
							Q_strncpy( soundname, tok, sizeof( soundname ) );
							actualEndTime = max( actualEndTime, event->GetLastSlaveEndTime() );
						}
					}
				}

				StudioModel *model = FindAssociatedModel( scene, event->GetActor() );

				CAudioMixer *mixer = event->GetMixer();
				if ( !mixer || !sound->IsSoundPlaying( mixer ) )
				{
					CSoundParameters params;

					float volume = VOL_NORM;
					gender_t gender = GENDER_NONE;
					if (model)
					{
						gender = soundemitter->GetActorGender( model->GetFileName() );
					}
					
					if ( !Q_stristr( soundname, ".wav" ) &&
						soundemitter->GetParametersForSound( soundname, params, gender ) )
					{
						volume = params.volume;
					}

					sound->PlaySound( 
						model, 
						volume,
						va( "sound/%s", FacePoser_TranslateSoundName( soundname, model ) ),
						&mixer );
					event->SetMixer( mixer );
				}

				if ( mixer )
				{
					mixer->SetDirection( m_flFrameTime >= 0.0f );
					float starttime, endtime;
					starttime = event->GetStartTime();
					endtime = actualEndTime;

					float soundtime = endtime - starttime;
					if ( soundtime > 0.0f )
					{
						float f = ( currenttime - starttime ) / soundtime;
						f = clamp( f, 0.0f, 1.0f );

						// Compute sample
						float numsamples = (float)mixer->GetSource()->SampleCount();

						int cursample = f * numsamples;
						cursample = clamp( cursample, 0, numsamples - 1 );
						mixer->SetSamplePosition( cursample );
						mixer->SetActive( true );
					}
				}
			}
		}
		break;
	case CChoreoEvent::EXPRESSION:
		{
		}
		break;
	case CChoreoEvent::LOOP:
		{
			ProcessLoop( scene, event );
		}
		break;
	case CChoreoEvent::STOPPOINT:
		{
			// Nothing, this is a symbolic event for keeping the vcd alive for ramping out after the last true event
		}
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : currenttime - 
//			*event - 
//-----------------------------------------------------------------------------
void CChoreoView::EndEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !event || !event->GetActive() )
		return;

	CChoreoActor *actor = event->GetActor();
	if ( actor && !actor->GetActive() )
	{
		return;
	}

	CChoreoChannel *channel = event->GetChannel();
	if ( channel && !channel->GetActive() )
	{
		return;
	}

	switch ( event->GetType() )
	{
	case CChoreoEvent::SUBSCENE:
		{
			CChoreoScene *subscene = event->GetSubScene();
			if ( subscene )
			{
				subscene->ResetSimulation();
			}
		}
		break;
	case CChoreoEvent::SPEAK:
		{
			CAudioMixer *mixer = event->GetMixer();
			if ( mixer && sound->IsSoundPlaying( mixer ) )
			{
				sound->StopSound( mixer );
			}
			event->SetMixer( NULL );
		}
		break;
	default:
		break;
	}

//	Con_Printf( "%8.4f:  finish %s\n", currenttime, event->GetDescription() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//			mx - 
//			my - 
//-----------------------------------------------------------------------------
int CChoreoView::GetTagUnderCursorPos( CChoreoEventWidget *event, int mx, int my )
{
	if ( !event )
	{
		return -1;
	}

	for ( int i = 0; i < event->GetEvent()->GetNumRelativeTags(); i++ )
	{
		CEventRelativeTag *tag = event->GetEvent()->GetRelativeTag( i );
		if ( !tag )
			continue;

		// Determine left edcge
		RECT bounds;
		bounds = event->getBounds();
		int left = bounds.left + (int)( tag->GetPercentage() * (float)( bounds.right - bounds.left ) + 0.5f );

		int tolerance = 3;

		if ( abs( mx - left ) < tolerance )
		{
			if ( abs( my - bounds.top ) < tolerance )
			{
				return i;
			}
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//			mx - 
//			my - 
//-----------------------------------------------------------------------------
CEventAbsoluteTag *CChoreoView::GetAbsoluteTagUnderCursorPos( CChoreoEventWidget *event, int mx, int my )
{
	if ( !event )
	{
		return NULL;
	}

	for ( int i = 0; i < event->GetEvent()->GetNumAbsoluteTags( CChoreoEvent::PLAYBACK ); i++ )
	{
		CEventAbsoluteTag *tag = event->GetEvent()->GetAbsoluteTag( CChoreoEvent::PLAYBACK, i );
		if ( !tag )
			continue;

		// Determine left edcge
		RECT bounds;
		bounds = event->getBounds();
		int left = bounds.left + (int)( tag->GetPercentage() * (float)( bounds.right - bounds.left ) + 0.5f );

		int tolerance = 3;

		if ( abs( mx - left ) < tolerance )
		{
			if ( abs( my - bounds.top ) < tolerance )
			{
				return tag;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//			**actor - 
//			**channel - 
//			**event - 
//-----------------------------------------------------------------------------
void CChoreoView::GetObjectsUnderMouse( int mx, int my, CChoreoActorWidget **actor,
	CChoreoChannelWidget **channel, CChoreoEventWidget **event, CChoreoGlobalEventWidget **globalevent,
	int *clickedTag,
	CEventAbsoluteTag **absolutetag, int *clickedCCArea )
{
	if ( actor )
	{
		*actor = GetActorUnderCursorPos( mx, my );
	}
	if ( channel )
	{
		*channel = GetChannelUnderCursorPos( mx, my );
		if ( *channel && clickedCCArea )
		{
			*clickedCCArea = (*channel)->GetChannelItemUnderMouse( mx, my );
		}
	}
	if ( event )
	{
		*event = GetEventUnderCursorPos( mx, my );
	}
	if ( globalevent )
	{
		*globalevent = GetGlobalEventUnderCursorPos( mx, my );
	}
	if ( clickedTag )
	{
		if ( event && *event )
		{
			*clickedTag = GetTagUnderCursorPos( *event, mx, my );
		}
		else
		{
			*clickedTag = -1;
		}
	}
	if ( absolutetag )
	{
		if ( event && *event )
		{
			*absolutetag = GetAbsoluteTagUnderCursorPos( *event, mx, my );
		}
		else
		{
			*absolutetag = NULL;
		}
	}


	m_nSelectedEvents = CountSelectedEvents();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : CChoreoGlobalEventWidget
//-----------------------------------------------------------------------------
CChoreoGlobalEventWidget *CChoreoView::GetGlobalEventUnderCursorPos( int mx, int my )
{
	POINT check;
	check.x = mx;
	check.y = my;

	CChoreoGlobalEventWidget *event;
	for ( int i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		event = m_SceneGlobalEvents[ i ];
		if ( !event )
			continue;

		RECT bounds;
		event->getBounds( bounds );

		if ( PtInRect( &bounds, check ) )
		{
			return event;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Caller must first translate mouse into screen coordinates
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
CChoreoActorWidget *CChoreoView::GetActorUnderCursorPos( int mx, int my )
{
	POINT check;
	check.x = mx;
	check.y = my;

	CChoreoActorWidget *actor;
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		RECT bounds;
		actor->getBounds( bounds );

		if ( PtInRect( &bounds, check ) )
		{
			return actor;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Caller must first translate mouse into screen coordinates
// Input  : mx - 
//			my - 
// Output : CChoreoChannelWidget
//-----------------------------------------------------------------------------
CChoreoChannelWidget *CChoreoView::GetChannelUnderCursorPos( int mx, int my )
{
	CChoreoActorWidget *actor = GetActorUnderCursorPos( mx, my );
	if ( !actor )
	{
		return NULL;
	}

	POINT check;
	check.x = mx;
	check.y = my;

	CChoreoChannelWidget *channel;
	for ( int i = 0; i < actor->GetNumChannels(); i++ )
	{
		channel = actor->GetChannel( i );
		if ( !channel )
			continue;

		RECT bounds;
		channel->getBounds( bounds );

		if ( PtInRect( &bounds, check ) )
		{
			return channel;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Caller must first translate mouse into screen coordinates
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
CChoreoEventWidget *CChoreoView::GetEventUnderCursorPos( int mx, int my )
{
	CChoreoChannelWidget *channel = GetChannelUnderCursorPos( mx, my );
	if ( !channel )
	{
		return NULL;
	}

	POINT check;
	check.x = mx;
	check.y = my;

	if ( mx < GetLabelWidth() )
		return NULL;

	if ( my < GetStartRow() )
		return NULL;

	if ( my >= h2() - ( m_nInfoHeight + m_nScrollbarHeight ) )
		return NULL;

	CChoreoEventWidget *event;
	for ( int i = 0; i < channel->GetNumEvents(); i++ )
	{
		event = channel->GetEvent( i );
		if ( !event )
			continue;

		RECT bounds;
		event->getBounds( bounds );

		// Events get an expanded border
		InflateRect( &bounds, 8, 4 );

		if ( PtInRect( &bounds, check ) )
		{
			return event;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Select wave file for phoneme editing
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CChoreoView::SetCurrentWaveFile( const char *filename, CChoreoEvent *event )
{
	g_pPhonemeEditor->SetCurrentWaveFile( filename, false, event );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfn - 
//			*param1 - 
//-----------------------------------------------------------------------------
void CChoreoView::TraverseWidgets( CVMEMBERFUNC pfn, CChoreoWidget *param1 )
{
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		(this->*pfn)( actor, param1 );

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			(this->*pfn)( channel, param1 );

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				(this->*pfn)( event, param1 );
			}
		}
	}

	for ( int i = 0; i < m_SceneGlobalEvents.Size(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( !event )
			continue;

		(this->*pfn)( event, param1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			*param1 - 
//-----------------------------------------------------------------------------
void CChoreoView::Deselect( CChoreoWidget *widget, CChoreoWidget *param1 )
{
	if ( widget->IsSelected() )
	{
		widget->SetSelected( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			*param1 - 
//-----------------------------------------------------------------------------
void CChoreoView::Select( CChoreoWidget *widget, CChoreoWidget *param1 )
{
	if ( widget != param1 )
		return;

	if ( widget->IsSelected() )
		return;

	widget->SetSelected( true );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *widget - 
//			*param1 - 
//-----------------------------------------------------------------------------
void CChoreoView::SelectAllEvents( CChoreoWidget *widget, CChoreoWidget *param1 )
{
	CChoreoEventWidget *ew = dynamic_cast< CChoreoEventWidget * >( widget );
	CChoreoGlobalEventWidget *gew = dynamic_cast< CChoreoGlobalEventWidget * >( widget );

	if ( ew || gew )
	{
		if ( widget->IsSelected() )
			return;

		widget->SetSelected( true );
	}
}

bool CChoreoView::CreateAnimationEvent( int mx, int my, char const *animationname )
{
	if ( !animationname || !animationname[0] )
		return false;

	// Convert screen to client
	POINT pt;
	pt.x = mx;
	pt.y = my;

	ScreenToClient( (HWND)getHandle(), &pt );

	if ( pt.x < 0 || pt.y < 0 )
		return false;

	if ( pt.x > w2() || pt.y > h2() )
		return false;

	pt.x -= GetLabelWidth();
	m_nClickedX = pt.x;

	GetObjectsUnderMouse( pt.x, pt.y, &m_pClickedActor, &m_pClickedChannel, &m_pClickedEvent, &m_pClickedGlobalEvent, &m_nClickedTag, &m_pClickedAbsoluteTag, &m_nClickedChannelCloseCaptionButton );

	// Find channel actor and time ( uses screen space coordinates )
	//
	CChoreoChannelWidget *channel = GetChannelUnderCursorPos( pt.x, pt.y );
	if ( !channel )
	{
		CChoreoActorWidget *actor = GetActorUnderCursorPos( pt.x, pt.y );
		if ( !actor )
			return false;

		// Grab first channel
		if ( !actor->GetNumChannels() )
			return false;

		channel = actor->GetChannel( 0 );
	}

	if ( !channel )
		return false;

	CChoreoChannel *pchannel = channel->GetChannel();
	if ( !pchannel )
	{
		Assert( 0 );
		return false;
	}

	// At this point we need to ask the user what type of even to create (gesture or sequence) and just show the approprite dialog
	CChoiceParams params;
	strcpy( params.m_szDialogTitle, "Create Animation Event" );

	params.m_bPositionDialog = false;
	params.m_nLeft = 0;
	params.m_nTop = 0;
	strcpy( params.m_szPrompt, "Type of event:" );

	params.m_Choices.RemoveAll();

	params.m_nSelected = 0;
	ChoiceText text;
	strcpy( text.choice, "gesture" );
	params.m_Choices.AddToTail( text );
	strcpy( text.choice, "sequence" );
	params.m_Choices.AddToTail( text );

	if ( !ChoiceProperties( &params ) )
		return false;
	
	if ( params.m_nSelected < 0 )
		return false;

	switch ( params.m_nSelected )
	{
	default:
	case 0:
		AddEvent( CChoreoEvent::GESTURE, 0, animationname );
		break;
	case 1:
		AddEvent( CChoreoEvent::SEQUENCE, 0, animationname );
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//			*cl - 
//			*exp - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::CreateExpressionEvent( int mx, int my, CExpClass *cl, CExpression *exp )
{
	if ( !m_pScene )
		return false;

	if ( !exp )
		return false;

	// Convert screen to client
	POINT pt;
	pt.x = mx;
	pt.y = my;

	ScreenToClient( (HWND)getHandle(), &pt );

	if ( pt.x < 0 || pt.y < 0 )
		return false;

	if ( pt.x > w2() || pt.y > h2() )
		return false;

	// Find channel actor and time ( uses screen space coordinates )
	//
	CChoreoChannelWidget *channel = GetChannelUnderCursorPos( pt.x, pt.y );
	if ( !channel )
	{
		CChoreoActorWidget *actor = GetActorUnderCursorPos( pt.x, pt.y );
		if ( !actor )
			return false;

		// Grab first channel
		if ( !actor->GetNumChannels() )
			return false;

		channel = actor->GetChannel( 0 );
	}

	if ( !channel )
		return false;

	CChoreoChannel *pchannel = channel->GetChannel();
	if ( !pchannel )
	{
		Assert( 0 );
		return false;
	}

	CChoreoEvent *event = m_pScene->AllocEvent();
	if ( !event )
	{
		Assert( 0 );
		return false;
	}

	float starttime = GetTimeValueForMouse( pt.x, false );

	SetDirty( true );

	PushUndo( "Create Expression" );

	event->SetType( CChoreoEvent::EXPRESSION );
	event->SetName( exp->name );
	event->SetParameters( cl->GetName() );
	event->SetParameters2( exp->name );
	event->SetStartTime( starttime );
	event->SetChannel( pchannel );
	event->SetActor( pchannel->GetActor() );
	event->SetEndTime( starttime + 1.0f );

	event->SnapTimes();

	DeleteSceneWidgets();

	// Add to appropriate channel
	pchannel->AddEvent( event );

	CreateSceneWidgets();

	PushRedo( "Create Expression" );

	// Redraw
	InvalidateLayout();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::IsPlayingScene( void )
{
	return m_bSimulating;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::ResetTargetSettings( void )
{
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *w = m_SceneActors[ i ];
		if ( w )
		{
			w->ResetSettings();
		}
	}

	models->ClearModelTargets( true );
}

//-----------------------------------------------------------------------------
// Purpose: copies the actors "settings" into the models FlexControllers
// Input  : dt - 
//-----------------------------------------------------------------------------
void CChoreoView::UpdateCurrentSettings( void )
{
	StudioModel *defaultModel = models->GetActiveStudioModel();

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *w = m_SceneActors[ i ];
		if ( !w )
			continue;

		if ( !w->GetActor()->GetActive() )
			continue;

		StudioModel *model = FindAssociatedModel( m_pScene, w->GetActor() );
		if ( !model )
			continue;

		CStudioHdr *hdr = model->GetStudioHdr();
		if ( !hdr )
			continue;

		float *current = w->GetSettings();

		for ( LocalFlexController_t j = LocalFlexController_t(0); j < hdr->numflexcontrollers(); j++ )
		{
			int k = hdr->pFlexcontroller( j )->localToGlobal;

			if (k != -1)
			{
				if ( defaultModel == model && g_pFlexPanel->IsEdited( k ) )
				{
					model->SetFlexController( j, g_pFlexPanel->GetSlider( k ) );
				}
				else
				{
					model->SetFlexController( j, current[ k ] );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//			tagnum - 
//-----------------------------------------------------------------------------
void CChoreoView::DeleteEventRelativeTag( CChoreoEvent *event, int tagnum )
{
	if ( !event )
		return;

	CEventRelativeTag *tag = event->GetRelativeTag( tagnum );
	if ( !tag )
		return;

	SetDirty( true );

	PushUndo( "Delete Event Tag" );

	event->RemoveRelativeTag( tag->GetName() );

	m_pScene->ReconcileTags();

	PushRedo( "Delete Event Tag" );

	g_pPhonemeEditor->redraw();
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::AddEventRelativeTag( void )
{
	CChoreoEventWidget *ew = m_pClickedEvent;
	if ( !ew )
		return;

	CChoreoEvent *event = ew->GetEvent();
	if ( !event->GetEndTime() )
	{
		Con_ErrorPrintf( "Event Tag:  Can only tag events with an end time\n" );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Event Tag Name" );
	strcpy( params.m_szPrompt, "Name:" );

	strcpy( params.m_szInputText, "" );

	if ( !InputProperties( &params ) )
		return;

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		Con_ErrorPrintf( "Event Tag Name:  No name entered!\n" );
		return;
	}
	
	RECT bounds = ew->getBounds();

	// Convert click to frac
	float frac = 0.0f;
	if ( bounds.right - bounds.left > 0 )
	{
		frac = (float)( m_nClickedX - bounds.left ) / (float)( bounds.right - bounds.left );
		frac = min( 1.0f, frac );
		frac = max( 0.0f, frac );
	}

	SetDirty( true );

	PushUndo( "Add Event Tag" );

	event->AddRelativeTag( params.m_szInputText, frac );

	PushRedo( "Add Event Tag" );

	InvalidateLayout();
	g_pPhonemeEditor->redraw();
	g_pExpressionTool->redraw();
	g_pGestureTool->redraw();
	g_pRampTool->redraw();
	g_pSceneRampTool->redraw();
}

CChoreoChannelWidget *CChoreoView::FindChannelForEvent( CChoreoEvent *event )
{
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				if ( e->GetEvent() != event )
					continue;

				return c;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : CChoreoEventWidget
//-----------------------------------------------------------------------------
CChoreoEventWidget *CChoreoView::FindWidgetForEvent( CChoreoEvent *event )
{
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				if ( e->GetEvent() != event )
					continue;

				return e;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::SelectAll( void )
{
	TraverseWidgets( &CChoreoView::SelectAllEvents, NULL );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::DeselectAll( void )
{
	TraverseWidgets( &CChoreoView::Deselect, NULL );
	redraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoView::UpdateStatusArea( int mx, int my )
{
	FLYOVER fo;

	GetObjectsUnderMouse( mx, my, &fo.a, &fo.c, 
		&fo.e, &fo.ge, &fo.tag, &fo.at, &fo.ccbutton );

	if ( fo.a )
	{
		m_Flyover.a = fo.a;
	}
	if ( fo.e )
	{
		m_Flyover.e = fo.e;
	}
	if ( fo.c )
	{
		m_Flyover.c = fo.c;
	}
	if ( fo.ge )
	{
		m_Flyover.ge = fo.ge;
	}
	if ( fo.tag != -1 )
	{
		m_Flyover.tag = fo.tag;
	}
	if ( fo.ccbutton != -1 )
	{
		m_Flyover.ccbutton = fo.ccbutton;
		// m_Flyover.e = NULL;
	}

	RECT rcClip;
	GetClientRect( (HWND)getHandle(), &rcClip );
	rcClip.bottom -= m_nScrollbarHeight;
	rcClip.top = rcClip.bottom - m_nInfoHeight;
	rcClip.right -= m_nScrollbarHeight;

	CChoreoWidgetDrawHelper drawHelper( this, rcClip, COLOR_CHOREO_BACKGROUND );

	drawHelper.StartClipping( rcClip );

	RedrawStatusArea( drawHelper, rcClip );

	drawHelper.StopClipping();
	ValidateRect( (HWND)getHandle(), &rcClip );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::ClearStatusArea( void )
{
	memset( &m_Flyover, 0, sizeof( m_Flyover ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcStatus - 
//-----------------------------------------------------------------------------
void CChoreoView::RedrawStatusArea( CChoreoWidgetDrawHelper& drawHelper, RECT& rcStatus )
{
	drawHelper.DrawFilledRect( COLOR_CHOREO_BACKGROUND, rcStatus );

	drawHelper.DrawColoredLine( COLOR_INFO_BORDER, PS_SOLID, 1, rcStatus.left, rcStatus.top,
		rcStatus.right, rcStatus.top );

	RECT rcInfo = rcStatus;

	rcInfo.top += 2;

	if ( m_Flyover.e )
	{
		m_Flyover.e->redrawStatus( drawHelper, rcInfo );
	}
	if ( m_Flyover.c &&
		m_Flyover.ccbutton != -1 )
	{
		m_Flyover.c->redrawStatus( drawHelper, rcInfo, m_Flyover.ccbutton );
	}

	if ( m_pScene )
	{
		char sz[ 512 ];

		int fontsize = 9;
		int fontweight = FW_NORMAL;

		RECT rcText;
		rcText = rcInfo;
		rcText.bottom = rcText.top + fontsize + 2;

		char const *mapname = m_pScene->GetMapname();
		if ( mapname )
		{
			sprintf( sz, "Associated .bsp:  %s", mapname[ 0 ] ? mapname : "none" );

			int len = drawHelper.CalcTextWidth( "Arial", fontsize, fontweight, sz );
			rcText.left = rcText.right - len - 10;

			drawHelper.DrawColoredText( "Arial", fontsize, fontweight, COLOR_INFO_TEXT, rcText, sz );

			OffsetRect( &rcText, 0, fontsize + 2 );
		}

		sprintf( sz, "Scene:  %s", GetChoreoFile() );

		int len = drawHelper.CalcTextWidth( "Arial", fontsize, fontweight, sz );
		rcText.left = rcText.right - len - 10;

		drawHelper.DrawColoredText( "Arial", fontsize, fontweight, COLOR_INFO_TEXT, rcText, sz );
	}

//	drawHelper.DrawColoredText( "Arial", 12, 500, RGB( 0, 0, 0 ), rcInfo, m_Flyover.e ? m_Flyover.e->GetEvent()->GetName() : "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoView::MoveEventToBack( CChoreoEvent *event )
{
	// Now find channel widget
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				if ( event == e->GetEvent() )
				{
					// Move it to back of channel's list
					c->MoveEventToTail( e );
					InvalidateLayout();
					return;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoView::GetEndRow( void )
{
	RECT rcClient;
	GetClientRect( (HWND)getHandle(), &rcClient );

	return rcClient.bottom - ( m_nInfoHeight + m_nScrollbarHeight );
}

// Undo/Redo
void CChoreoView::Undo( void )
{
	if ( m_UndoStack.Size() > 0 && m_nUndoLevel > 0 )
	{
		m_nUndoLevel--;
		CVUndo *u = m_UndoStack[ m_nUndoLevel ];
		Assert( u->undo );

		DeleteSceneWidgets();

		*m_pScene = *(u->undo);
		g_MDLViewer->InitGridSettings();

		CreateSceneWidgets();

		ReportSceneClearToTools();
		ClearStatusArea();
		m_pClickedActor = NULL;
		m_pClickedChannel = NULL;
		m_pClickedEvent = NULL;
		m_pClickedGlobalEvent = NULL; 
	}

	InvalidateLayout();
}

void CChoreoView::Redo( void )
{
	if ( m_UndoStack.Size() > 0 && m_nUndoLevel <= m_UndoStack.Size() - 1 )
	{
		CVUndo *u = m_UndoStack[ m_nUndoLevel ];
		Assert( u->redo );

		DeleteSceneWidgets();

		*m_pScene = *(u->redo);
		g_MDLViewer->InitGridSettings();

		CreateSceneWidgets();

		ReportSceneClearToTools();
		ClearStatusArea();
		m_pClickedActor = NULL;
		m_pClickedChannel = NULL;
		m_pClickedEvent = NULL;
		m_pClickedGlobalEvent = NULL; 

		m_nUndoLevel++;
	}

	InvalidateLayout();
}

static char *CopyString( const char *in )
{
	int len = strlen( in );
	char *n = new char[ len + 1 ];
	strcpy( n, in );
	return n;
}

void CChoreoView::PushUndo( const char *description )
{
	Assert( !m_bRedoPending );
	m_bRedoPending = true;
	WipeRedo();

	// Copy current data
	CChoreoScene *u = new CChoreoScene( this );
	*u = *m_pScene;
	CVUndo *undo = new CVUndo;
	undo->undo = u;
	undo->redo = NULL;
	undo->udescription = CopyString( description );
	undo->rdescription = NULL;
	m_UndoStack.AddToTail( undo );
	m_nUndoLevel++;
}

void CChoreoView::PushRedo( const char *description )
{
	Assert( m_bRedoPending );
	m_bRedoPending = false;

	// Copy current data
	CChoreoScene *r = new CChoreoScene( this );
	*r = *m_pScene;
	CVUndo *undo = m_UndoStack[ m_nUndoLevel - 1 ];
	undo->redo = r;
	undo->rdescription = CopyString( description );

	// Always redo here to reflect that someone has made a change
	redraw();
}

void CChoreoView::WipeUndo( void )
{
	while ( m_UndoStack.Size() > 0 )
	{
		CVUndo *u = m_UndoStack[ 0 ];
		delete u->undo;
		delete u->redo;
		delete[] u->udescription;
		delete[] u->rdescription;
		delete u;
		m_UndoStack.Remove( 0 );
	}
	m_nUndoLevel = 0;
}

void CChoreoView::WipeRedo( void )
{
	// Wipe everything above level
	while ( m_UndoStack.Size() > m_nUndoLevel )
	{
		CVUndo *u = m_UndoStack[ m_nUndoLevel ];
		delete u->undo;
		delete u->redo;
		delete[] u->udescription;
		delete[] u->rdescription;
		delete u;
		m_UndoStack.Remove( m_nUndoLevel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CChoreoView::GetUndoDescription( void )
{
	if ( CanUndo() )
	{
		CVUndo *u = m_UndoStack[ m_nUndoLevel - 1 ];
		return u->udescription;
	}
	return "???undo";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CChoreoView::GetRedoDescription( void )
{
	if ( CanRedo() )
	{
		CVUndo *u = m_UndoStack[ m_nUndoLevel ];
		return u->rdescription;
	}
	return "???redo";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::CanUndo()
{
	return m_nUndoLevel != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::CanRedo()
{
	return m_nUndoLevel != m_UndoStack.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::OnGestureTool( void )
{
	if ( m_pClickedEvent->GetEvent()->GetType() != CChoreoEvent::GESTURE )
		return;

	g_pGestureTool->SetEvent( m_pClickedEvent->GetEvent() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoView::OnExpressionTool( void )
{
	if ( m_pClickedEvent->GetEvent()->GetType() != CChoreoEvent::FLEXANIMATION )
		return;

	g_pExpressionTool->SetEvent( m_pClickedEvent->GetEvent() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CChoreoScene
//-----------------------------------------------------------------------------
CChoreoScene *CChoreoView::GetScene( void )
{
	return m_pScene;
}

bool CChoreoView::CanPaste( void )
{
	char const *copyfile = COPYPASTE_FILENAME;

	if ( !filesystem->FileExists( copyfile ) )
	{
		return false;
	}

	return true;
}

void CChoreoView::CopyEvents( void )
{
	if ( !m_pScene )
		return;

	char const *copyfile = COPYPASTE_FILENAME;
	MakeFileWriteable( copyfile );
	ExportVCDFile( copyfile );
}

void CChoreoView::PasteEvents( void )
{
	if ( !m_pScene )
		return;

	if ( !CanPaste() )
		return;

	char const *copyfile = COPYPASTE_FILENAME;

	ImportVCDFile( copyfile );
}

void CChoreoView::ImportEvents( void )
{
	if ( !m_pScene )
		return;

	if ( !m_pClickedActor || !m_pClickedChannel )
		return;

	char eventfile[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( eventfile, sizeof( eventfile ), "scenes", "*.vce" ) )
		return;
	
	char fullpathbuf[ 512 ];
	char *fullpath = eventfile;
	if ( !Q_IsAbsolutePath( eventfile ) )
	{
		filesystem->RelativePathToFullPath( eventfile, "GAME", fullpathbuf, sizeof( fullpathbuf ) );
		fullpath = fullpathbuf;
	}

	if ( !filesystem->FileExists( fullpath ) )
		return;

	LoadScriptFile( fullpath );

	DeselectAll();

	SetDirty( true );

	PushUndo( "Import Events" );

	m_pScene->ImportEvents( tokenprocessor, m_pClickedActor->GetActor(), m_pClickedChannel->GetChannel() );

	PushRedo( "Import Events" );

	CreateSceneWidgets();
	// Redraw
	InvalidateLayout();

	Con_Printf( "Imported events from %s\n", fullpath );
}

void CChoreoView::ExportEvents( void )
{
	char eventfilename[ 512 ];
	if ( !FacePoser_ShowSaveFileNameDialog( eventfilename, sizeof( eventfilename ), "scenes", "*.vce" ) )
		return;

	Q_DefaultExtension( eventfilename, ".vce", sizeof( eventfilename ) );

	Con_Printf( "Exporting events to %s\n", eventfilename );

	// Write to file
	CUtlVector< CChoreoEvent * > events;

	// Find selected eventss
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				if ( !e->IsSelected() )
					continue;

				CChoreoEvent *event = e->GetEvent();
				if ( !event )
					continue;

				events.AddToTail( event );
			}
		}
	}

	if ( events.Size() > 0 )
	{
		m_pScene->ExportEvents( eventfilename, events );
	}
	else
	{
		Con_Printf( "No events selected\n" );
	}
}

void CChoreoView::ExportVCDFile( char const *filename )
{
	Con_Printf( "Exporting to %s\n", filename );

	// Unmark everything
	m_pScene->MarkForSaveAll( false );

	// Mark everything related to selected events
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				if ( !e->IsSelected() )
					continue;

				CChoreoEvent *event = e->GetEvent();
				if ( !event )
					continue;

				event->SetMarkedForSave( true );
				if ( event->GetChannel() )
				{
					event->GetChannel()->SetMarkedForSave( true );
				}
				if ( event->GetActor() )
				{
					event->GetActor()->SetMarkedForSave( true );
				}
			}
		}
	}

	m_pScene->ExportMarkedToFile( filename );
}

void CChoreoView::ImportVCDFile( char const *filename )
{
	CChoreoScene *merge = LoadScene( filename );
	if ( !merge )
	{
		Con_Printf( "Couldn't load from .vcd %s\n", filename );
		return;
	}

	DeselectAll();

	CUtlRBTree< CChoreoEvent *, int > oldEvents( 0, 0, DefLessFunc( CChoreoEvent * ) );

	int i;
	for ( i = 0; i < m_SceneActors.Count(); ++i )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				oldEvents.Insert( event->GetEvent() );
			}
		}
	}

	SetDirty( true );

	PushUndo( "Merge/Import VCD" );

	m_pScene->Merge( merge );

	PushRedo( "Merge/Import VCD" );

	DeleteSceneWidgets();
	CreateSceneWidgets();

	// Force scroll bars to recompute
	ForceScrollBarsToRecompute( false );

	// Now walk through the "new" events and select everything that wasn't already there (all of the stuff that was "added" during the merge)
	for ( i = 0; i < m_SceneActors.Count(); ++i )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				if ( oldEvents.Find( event->GetEvent() ) == oldEvents.InvalidIndex() )
				{
					event->SetSelected( true );
				}
			}
		}
	}

	// Redraw
	InvalidateLayout();

	Con_Printf( "Imported vcd '%s'\n", filename );

	delete merge;

	redraw();
}

void CChoreoView::ExportVCD()
{
	char scenefile[ 512 ];
	if ( !FacePoser_ShowSaveFileNameDialog( scenefile, sizeof( scenefile ), "scenes", "*.vcd" ) )
	{
		return;
	}

	Q_DefaultExtension( scenefile, ".vcd", sizeof( scenefile ) );

	ExportVCDFile( scenefile );
}

void CChoreoView::ImportVCD()
{
	if ( !m_pScene )
		return;

	if ( !m_pClickedActor || !m_pClickedChannel )
		return;

	char scenefile[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( scenefile, sizeof( scenefile ), "scenes", "*.vcd" ) )
	{
		return;
	}

	ImportVCDFile( scenefile );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::IsProcessing( void )
{
	if ( !m_pScene )
		return false;

	if ( m_flScrub != m_flScrubTarget )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoView::ShouldProcessSpeak( void )
{
	if ( !g_pControlPanel->AllToolsDriveSpeech() )
	{
		if ( !IsActiveTool() )
			return false;
	}

	if ( IFacePoserToolWindow::IsAnyToolScrubbing() )
		return true;

	if ( IFacePoserToolWindow::IsAnyToolProcessing() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scene - 
//			*event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessSpeak( CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !ShouldProcessSpeak() )
		return;

	Assert( event->GetType() == CChoreoEvent::SPEAK );
	Assert( scene );

	float t = scene->GetTime();

	StudioModel *model = FindAssociatedModel( scene, event->GetActor() );

	// See if we should trigger CC
	char soundname[ 512 ];
	Q_strncpy( soundname, event->GetParameters(), sizeof( soundname ) );

	float actualEndTime = event->GetEndTime();

	if ( event->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
	{
		char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		if ( event->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
		{
			// Use the token as the sound name lookup, too.
			if ( event->IsUsingCombinedFile() &&
				 ( event->GetNumSlaves() > 0 ) ) 
			{
				Q_strncpy( soundname, tok, sizeof( soundname ) );
				actualEndTime = max( actualEndTime, event->GetLastSlaveEndTime() );
			}
		}
	}

	CAudioMixer *mixer = event->GetMixer();
	if ( !mixer || !sound->IsSoundPlaying( mixer ) )
	{
		CSoundParameters params;
		float volume = VOL_NORM;
		
		gender_t gender = GENDER_NONE;
		if (model)
		{
			gender = soundemitter->GetActorGender( model->GetFileName() );
		}

		if ( !Q_stristr( soundname, ".wav" ) &&
			soundemitter->GetParametersForSound( soundname, params, gender ) )
		{
			volume = params.volume;
		}

		sound->PlaySound( 
			model, 
			volume,
			va( "sound/%s", FacePoser_TranslateSoundName( soundname, model ) ), 
			&mixer );
		event->SetMixer( mixer );
	}

	mixer = event->GetMixer();
	if ( !mixer )
		return;

	mixer->SetDirection( m_flFrameTime >= 0.0f );
	float starttime, endtime;
	starttime = event->GetStartTime();
	endtime = actualEndTime;

	float soundtime = endtime - starttime;
	if ( soundtime <= 0.0f )
		return;

	float f = ( t - starttime ) / soundtime;
	f = clamp( f, 0.0f, 1.0f );

	// Compute sample
	float numsamples = (float)mixer->GetSource()->SampleCount();

	int cursample = f * numsamples;
	cursample = clamp( cursample, 0, numsamples - 1 );

	int realsample = mixer->GetSamplePosition();

	int dsample = cursample - realsample;

	int samplelimit = mixer->GetSource()->SampleRate() * 0.02f; // don't shift until samples are off by this much
	if (IsScrubbing())
	{
		samplelimit = mixer->GetSource()->SampleRate() * 0.01f; // make it shorter tolerance when scrubbing
	}

	if ( abs( dsample ) > samplelimit )
	{
		mixer->SetSamplePosition( cursample, IsScrubbing() );
	}
	mixer->SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CChoreoView::ProcessSubscene( CChoreoScene *scene, CChoreoEvent *event )
{
	Assert( event->GetType() == CChoreoEvent::SUBSCENE );

	CChoreoScene *subscene = event->GetSubScene();
	if ( !subscene )
		return;

	if ( subscene->SimulationFinished() )
		return;
	
	// Have subscenes think for appropriate time
	subscene->Think( m_flScrub );
}

void CChoreoView::PositionControls()
{
	int topx = GetCaptionHeight() + SCRUBBER_HEIGHT;

	int bx = 2;
	int bw = 16;

	m_btnPlay->setBounds( bx, topx + 4, 16, 16 );

	bx += bw + 2;

	m_btnPause->setBounds( bx, topx + 4, 16, 16 );
	bx += bw + 2;
	m_btnStop->setBounds( bx, topx + 4, 16, 16 );
	bx += bw + 2;
	m_pPlaybackRate->setBounds( bx, topx + 4, 100, 16 );
}

void CChoreoView::SetChoreoFile( char const *filename )
{
	strcpy( m_szChoreoFile, filename );
	if ( m_szChoreoFile[ 0 ] )
	{
		char sz[ 256 ];
		if ( IsFileWriteable( m_szChoreoFile ) )
		{
			Q_snprintf( sz, sizeof( sz ), " - %s", m_szChoreoFile );
		}
		else
		{
			Q_snprintf( sz, sizeof( sz ), " - %s [Read-Only]", m_szChoreoFile );
		}
		SetSuffix( sz );
	}
	else
	{
		SetSuffix( "" );
	}
}

char const *CChoreoView::GetChoreoFile( void ) const
{
	return m_szChoreoFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcHandle - 
//-----------------------------------------------------------------------------
void CChoreoView::GetScrubHandleRect( RECT& rcHandle, bool clipped )
{
	float pixel = 0.0f;

	if ( m_pScene )
	{
		float currenttime = m_flScrub;
		float starttime = m_flStartTime;
		float endtime = m_flEndTime;

		float screenfrac = ( currenttime - starttime ) / ( endtime - starttime );

		pixel = GetLabelWidth() + screenfrac * ( w2() - GetLabelWidth() );

		if ( clipped )
		{
			pixel = clamp( pixel, SCRUBBER_HANDLE_WIDTH/2, w2() - SCRUBBER_HANDLE_WIDTH/2 );
		}
	}

	rcHandle.left = pixel-SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.right = pixel + SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.top = 2 + GetCaptionHeight();
	rcHandle.bottom = rcHandle.top + SCRUBBER_HANDLE_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcArea - 
//-----------------------------------------------------------------------------
void CChoreoView::GetScrubAreaRect( RECT& rcArea )
{
	rcArea.left = 0;
	rcArea.right = w2();
	rcArea.top = 2 + GetCaptionHeight();
	rcArea.bottom = rcArea.top + SCRUBBER_HEIGHT - 4;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcHandle - 
//-----------------------------------------------------------------------------
void CChoreoView::DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );

	HBRUSH br = CreateSolidBrush( RGB( 0, 150, 100 ) );

	drawHelper.DrawFilledRect( br, rcHandle );

	// 
	char sz[ 32 ];
	sprintf( sz, "%.3f", m_flScrub );

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
bool CChoreoView::IsMouseOverScrubHandle( mxEvent *event )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );
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

bool CChoreoView::IsMouseOverScrubArea( mxEvent *event )
{
	RECT rcArea;
	GetScrubAreaRect( rcArea );

	InflateRect( &rcArea, 2, 2 );

	POINT pt;
	pt.x = (short)event->x;
	pt.y = (short)event->y;
	if ( PtInRect( &rcArea, pt ) )
	{
		return true;
	}

	return false;
}

bool CChoreoView::IsScrubbing( void ) const
{
	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	return scrubbing;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CChoreoView::Think( float dt )
{
	bool scrubbing = IFacePoserToolWindow::IsAnyToolScrubbing();

	ScrubThink( dt, scrubbing, this );
}

static int lastthinkframe = -1;
void CChoreoView::ScrubThink( float dt, bool scrubbing, IFacePoserToolWindow *invoker )
{
	// Make sure we don't get called more than once per frame
	int thisframe = g_MDLViewer->GetCurrentFrame();
	if ( thisframe == lastthinkframe )
		return;

	lastthinkframe = thisframe;

	if ( !m_pScene )
		return;

	if ( m_flScrubTarget == m_flScrub && !scrubbing )
	{
		// Act like it's paused
		if ( IFacePoserToolWindow::ShouldAutoProcess() )
		{
			m_bSimulating = true;
			SceneThink( m_flScrub );
		}

		if ( m_bSimulating && !m_bPaused )
		{
			//FinishSimulation();
		}
		return;
	}

	// Make sure we're solving head turns during playback
	models->SetSolveHeadTurn( 1 );

	if ( m_bPaused )
	{
		SceneThink( m_flScrub );
		return;
	}

	// Make sure phonemes are loaded
	FacePoser_EnsurePhonemesLoaded();

	if ( !m_bSimulating )
	{
		m_bSimulating = true;
	}

	float d = m_flScrubTarget - m_flScrub;
	int sign = d > 0.0f ? 1 : -1;

	float maxmove = dt * m_flPlaybackRate;

	float prevScrub = m_flScrub;

	if ( sign > 0 )
	{
		if ( d < maxmove )
		{
			m_flScrub = m_flScrubTarget;
		}
		else
		{
			m_flScrub += maxmove;
		}
	}
	else
	{
		if ( -d < maxmove )
		{
			m_flScrub = m_flScrubTarget;
		}
		else
		{
			m_flScrub -= maxmove;
		}
	}

	m_flFrameTime = ( m_flScrub - prevScrub );

	SceneThink( m_flScrub );

	DrawScrubHandle();

	if ( scrubbing )
	{
		g_pMatSysWindow->Frame();
	}

	if ( invoker != g_pExpressionTool )
	{
		g_pExpressionTool->ForceScrubPositionFromSceneTime( m_flScrub );
	}
	if ( invoker != g_pGestureTool )
	{
		g_pGestureTool->ForceScrubPositionFromSceneTime( m_flScrub );
	}
	if ( invoker != g_pRampTool )
	{
		g_pRampTool->ForceScrubPositionFromSceneTime( m_flScrub );
	}
	if ( invoker != g_pSceneRampTool )
	{
		g_pSceneRampTool->ForceScrubPositionFromSceneTime( m_flScrub );
	}
}

void CChoreoView::DrawScrubHandle( void )
{
	if ( !m_bCanDraw )
		return;

	// Handle new time and
	RECT rcArea;
	GetScrubAreaRect( rcArea );

	CChoreoWidgetDrawHelper drawHelper( this, rcArea, COLOR_CHOREO_BACKGROUND );
	DrawScrubHandle( drawHelper );
}

void CChoreoView::SetScrubTime( float t )
{
	m_flScrub = t;

	m_bPaused = false;
}

void CChoreoView::SetScrubTargetTime( float t )
{
	m_flScrubTarget = t;

	m_bPaused = false;
}

void CChoreoView::ClampTimeToSelectionInterval( float& timeval )
{
	// FIXME hook this up later
	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : show - 
//-----------------------------------------------------------------------------
void CChoreoView::ShowButtons( bool show )
{
	m_btnPlay->setVisible( show );
	m_btnPause->setVisible( show );
	m_btnStop->setVisible( show );
	m_pPlaybackRate->setVisible( show );
}
	
void CChoreoView::RememberSelectedEvents( CUtlVector< CChoreoEvent * >& list )
{
	GetSelectedEvents( list );
}

void CChoreoView::ReselectEvents( CUtlVector< CChoreoEvent * >& list )
{
	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				CChoreoEvent *check = event->GetEvent();
				if ( list.Find( check ) != list.InvalidIndex() )
				{
					event->SetSelected( true );
				}
			}
		}
	}

}

void CChoreoView::OnChangeScale( void )
{
	CChoreoScene *scene = m_pScene;
	if ( !scene )
	{
		return;
	}

	// Zoom time in  / out
	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Change Zoom" );
	strcpy( params.m_szPrompt, "New scale (e.g., 2.5x):" );

	Q_snprintf( params.m_szInputText, sizeof( params.m_szInputText ), "%.2f", (float)GetTimeZoom( GetToolName() ) / 100.0f );

	if ( !InputProperties( &params ) )
		return;

	SetTimeZoom( GetToolName(), clamp( (int)( 100.0f * atof( params.m_szInputText ) ), 1, MAX_TIME_ZOOM ), false );

	// Force scroll bars to recompute
	ForceScrollBarsToRecompute( false );

	CUtlVector< CChoreoEvent * > selected;
	RememberSelectedEvents( selected );

	DeleteSceneWidgets();
	CreateSceneWidgets();

	ReselectEvents( selected );

	InvalidateLayout();
	Con_Printf( "Zoom factor %i %%\n", GetTimeZoom( GetToolName() ) );
}

void CChoreoView::OnCheckSequenceLengths( void )
{
	if ( !m_pScene )
		return;

	Con_Printf( "Checking sequence durations...\n" );

	bool changed = FixupSequenceDurations( m_pScene, true );

	if ( !changed )
	{
		Con_Printf( "   no changes...\n" );
		return;
	}

	SetDirty( true );

	PushUndo( "Check sequence lengths" );

	FixupSequenceDurations( m_pScene, false );

	PushRedo( "Check sequence lengths" );

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scene - 
//-----------------------------------------------------------------------------
void CChoreoView::InvalidateTrackLookup_R( CChoreoScene *scene )
{
	// No need to undo since this data doesn't matter
	int c = scene->GetNumEvents();
	for ( int i = 0; i < c; i++ )
	{
		CChoreoEvent *event = scene->GetEvent( i );
		if ( !event )
			continue;

		switch ( event->GetType() )
		{
		default:
			break;
		case CChoreoEvent::FLEXANIMATION:
			{
				event->SetTrackLookupSet( false );
			}
			break;
		case CChoreoEvent::SUBSCENE:
			{
				CChoreoScene *sub = event->GetSubScene();
				// NOTE:  Don't bother loading it now if it's not on hand
				if ( sub )
				{
					InvalidateTrackLookup_R( sub );
				}
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Model changed so we'll have to re-index flex anim tracks
//-----------------------------------------------------------------------------
void CChoreoView::InvalidateTrackLookup( void )
{
	if ( !m_pScene )
		return;

	InvalidateTrackLookup_R( m_pScene );
}



bool CChoreoView::IsRampOnly( void ) const
{
	return m_bRampOnly;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scene - 
//			*event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessInterrupt( CChoreoScene *scene, CChoreoEvent *event )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scene - 
//			*event - 
//-----------------------------------------------------------------------------
void CChoreoView::ProcessPermitResponses( CChoreoScene *scene, CChoreoEvent *event )
{
}

void CChoreoView::ApplyBounds( int& mx, int& my )
{
	if ( !m_bUseBounds )
		return;

	mx = clamp( mx, m_nMinX, m_nMaxX );
}

//-----------------------------------------------------------------------------
// Purpose: Returns -1 if no event found
// Input  : *channel - 
//			*e - 
//			forward - 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoView::FindNextEventTime( CChoreoEvent::EVENTTYPE type, CChoreoChannel *channel, CChoreoEvent *e, bool forward )
{
	bool foundone = false;
	float bestTime = -1.0f;
	float bestGap = 999999.0f;

	int c = channel->GetNumEvents();
	for ( int i = 0; i < c; i++ )
	{
		CChoreoEvent *test = channel->GetEvent( i );
		if ( test->GetType() != type )
			continue;

		if ( forward )
		{
			float dt = test->GetStartTime() - e->GetEndTime();
			if ( dt <= 0.0f )
				continue;

			if ( dt < bestGap )
			{
				foundone = true;
				bestGap = dt;
				bestTime = test->GetStartTime();
			}
		}
		else
		{
			float dt = e->GetStartTime() - test->GetEndTime();
			if ( dt <= 0.0f )
				continue;

			if ( dt < bestGap )
			{
				foundone = true;
				bestGap = dt;
				bestTime = test->GetEndTime();
			}
		}
	}

	return bestTime;
}

void CChoreoView::CalcBounds( int movetype )
{
	m_bUseBounds = false;
	m_nMinX = 0;
	m_nMaxX = 0;

	if ( !m_pClickedEvent )
		return;

	switch ( movetype )
	{
	default:
		break;
	case DRAGTYPE_EVENT_MOVE:
	case DRAGTYPE_EVENT_STARTTIME:
	case DRAGTYPE_EVENT_ENDTIME:
	case DRAGTYPE_EVENT_STARTTIME_RESCALE:
	case DRAGTYPE_EVENT_ENDTIME_RESCALE:
		{
			m_nMinX = GetPixelForTimeValue( 0 );
			m_nMaxX = GetPixelForTimeValue( m_pScene->FindStopTime() );

			CChoreoEvent *e = m_pClickedEvent->GetEvent();

			m_bUseBounds = false; // e && e->GetType() == CChoreoEvent::GESTURE;
			// FIXME: use this for finding adjacent gesture edges (kenb)
			if ( m_bUseBounds )
			{
				CChoreoChannel *channel = e->GetChannel();
				Assert( channel );

				float forwardTime = FindNextEventTime( e->GetType(), channel, e, true );
				float reverseTime = FindNextEventTime( e->GetType(), channel, e, false );

				// Compute pixel for time
				int nextPixel = forwardTime != -1 ? GetPixelForTimeValue( forwardTime ) : m_nMaxX;
				int prevPixel = reverseTime != -1 ? GetPixelForTimeValue( reverseTime ) : m_nMinX;

				int startPixel = GetPixelForTimeValue( e->GetStartTime() );
				int endPixel = GetPixelForTimeValue( e->GetEndTime() );

				switch ( movetype )
				{
				case DRAGTYPE_EVENT_MOVE:
					{
						m_nMinX = prevPixel + ( m_xStart - startPixel ) + 1;
						m_nMaxX = nextPixel - ( endPixel - m_xStart ) - 1;
					}
					break;
				case DRAGTYPE_EVENT_STARTTIME:
				case DRAGTYPE_EVENT_STARTTIME_RESCALE:
					{
						m_nMinX = prevPixel + ( m_xStart - startPixel ) + 1;
					}
					break;
				case DRAGTYPE_EVENT_ENDTIME:
				case DRAGTYPE_EVENT_ENDTIME_RESCALE:
					{
						m_nMaxX = nextPixel - ( endPixel - m_xStart ) - 1;
					}
					break;
				}
			}
		}
		break;
	}
}

bool CChoreoView::ShouldSelectEvent( SelectionParams_t &params, CChoreoEvent *event )
{
	if ( params.forward )
	{
		if ( event->GetStartTime() >= params.time )
			return true;
	}
	else
	{
		float endtime = event->HasEndTime() ? event->GetEndTime() : event->GetStartTime();

		if ( endtime <= params.time )
			return true;
	}
	return false;
}

void CChoreoView::SelectEvents( SelectionParams_t& params )
{
	if ( !m_pScene )
		return;

	if ( !m_pClickedActor )
		return;

	if ( params.type == SelectionParams_t::SP_CHANNEL && !m_pClickedChannel )
		return;

	//CChoreoActor *actor = m_pClickedActor->GetActor();
	CChoreoChannel *channel = m_pClickedChannel ? m_pClickedChannel->GetChannel() : NULL;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *a = m_SceneActors[ i ];
		if ( !a )
			continue;

		//if ( a->GetActor() != actor )
		//	continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *c = a->GetChannel( j );
			if ( !c )
				continue;

			if ( params.type == SelectionParams_t::SP_CHANNEL &&
				c->GetChannel() != channel )
				continue;

			if ( params.type == SelectionParams_t::SP_ACTIVE &&
				!c->GetChannel()->GetActive() )
				continue;

			for ( int k = 0; k < c->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *e = c->GetEvent( k );
				if ( !e )
					continue;

				CChoreoEvent *event = e->GetEvent();
				if ( !event )
					continue;

				if ( !ShouldSelectEvent( params, event ) )
					continue;

				e->SetSelected( true );
			}
		}
	}

	// Now handle global events, too
	for ( int i = 0; i < m_SceneGlobalEvents.Count(); i++ )
	{
		CChoreoGlobalEventWidget *e = m_SceneGlobalEvents[ i ];
		if ( !e )
			continue;

		CChoreoEvent *event = e->GetEvent();
		if ( !event )
			continue;

		if ( !ShouldSelectEvent( params, event ) )
			continue;

		e->SetSelected( true );
	}

	redraw();
}

void CChoreoView::SetTimeZoom( char const *tool, int tz, bool preserveFocus )
{
	if ( !m_pScene )
		return;
	
	// No change
	int oldZoom = GetTimeZoom( tool );
	if ( tz == oldZoom )
		return;

	SetDirty( true );

	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( (HWND)getHandle(), &pt );

	// Now figure out time under cursor at old zoom scale
	float t = GetTimeValueForMouse( pt.x, true );

	m_pScene->SetTimeZoom( tool, tz );

	if ( preserveFocus )
	{
		RECT rc;
		GetClientRect( (HWND)getHandle(), &rc );
		RECT rcClient = rc;
		rcClient.top += GetStartRow();
		OffsetRect( &rcClient, 0, -m_nTopOffset );
		m_flStartTime = m_flLeftOffset / GetPixelsPerSecond();
		m_flEndTime = m_flStartTime + (float)( rcClient.right - GetLabelWidth() ) / GetPixelsPerSecond();

		// Now figure out tie under pt.x
		float newT = GetTimeValueForMouse( pt.x, true );
		if ( newT != t )
		{
			// We need to scroll over a bit
			float pps = GetPixelsPerSecond();
			float movePixels = pps * ( newT - t );

			m_flLeftOffset -= movePixels;
			if ( m_flLeftOffset < 0.0f )
			{
				//float fixup = - m_flLeftOffset;
				m_flLeftOffset = 0;
			}

			// float maxtime = m_pScene->FindStopTime();
			float flApparentEndTime = max( m_pScene->FindStopTime(), 5.0f ) + 5.0f;
			if ( m_flEndTime > flApparentEndTime )
			{
				movePixels = pps * ( m_flEndTime - flApparentEndTime );
				m_flLeftOffset = max( 0.0f, m_flLeftOffset - movePixels );
			}
		}
	}

	// Deal with the slider
	RepositionHSlider();
	redraw();
}

int CChoreoView::GetTimeZoom( char const *tool )
{
	if ( !m_pScene )
		return 100;

	return m_pScene->GetTimeZoom( tool );
}

void CChoreoView::CheckInsertTime( CChoreoEvent *e, float dt, float starttime, float endtime )
{
	// Not influenced
	float eventend = e->HasEndTime() ? e->GetEndTime() : e->GetStartTime();
	
	if ( eventend < starttime )
		return;

	if ( e->GetStartTime() > starttime )
	{
		e->OffsetTime( dt );
		e->SnapTimes();
	}
	else if ( !e->IsFixedLength() && e->HasEndTime() ) // the event starts before start, but ends after start time, need to insert space into the event, act like user dragged end time
	{
		float newduration = e->GetDuration() + dt;
		RescaleRamp( e, newduration );
		switch ( e->GetType() )
		{
		default:
			break;
		case CChoreoEvent::GESTURE:
			{
				e->RescaleGestureTimes( e->GetStartTime(), e->GetEndTime() + dt, true );
			}
			break;
		case CChoreoEvent::FLEXANIMATION:
			{
				RescaleExpressionTimes( e, e->GetStartTime(), e->GetEndTime() + dt );
			}
			break;
		}
		e->OffsetEndTime( dt );
		e->SnapTimes();
		e->ResortRamp();
	}

	switch ( e->GetType() )
	{
	default:
		break;
	case CChoreoEvent::SPEAK:
		{
			// Try and load wav to get length
			CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( e ) ) );
			if ( wave )
			{
				e->SetEndTime( e->GetStartTime() + wave->GetRunningLength() );
				delete wave;
			}
		}
		break;
	case CChoreoEvent::SEQUENCE:
		{
			CheckSequenceLength( e, false );
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			CheckGestureLength( e, false );
		}
		break;
	}
}

void CChoreoView::OnInsertTime()
{
	if ( !m_rgABPoints[ 0 ].active &&
		 !m_rgABPoints[ 1 ].active  )
	{
		return;
	}

	Con_Printf( "OnInsertTime()\n" );

	float starttime = m_rgABPoints[ 0 ].time;
	float endtime = m_rgABPoints[ 1 ].time;

	// Sort samples correctly
	if ( starttime > endtime )
	{
		float temp = starttime;
		starttime = endtime;
		endtime = temp;
	}

	float dt = endtime - starttime;
	if ( dt == 0.0f )
	{
		// Nothing to do...
		return;
	}

	SetDirty( true );

	PushUndo( "Insert Time" );

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				CChoreoEvent *e = event->GetEvent();
				if ( !e )
					continue;

				CheckInsertTime( e, dt, starttime, endtime );
			}
		}
	}

	// Now handle global events, too
	for ( int i = 0; i < m_SceneGlobalEvents.Count(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( !event )
			continue;

		CChoreoEvent *e = event->GetEvent();
		if ( !e )
			continue;

		CheckInsertTime( e, dt, starttime, endtime );
	}

	PushRedo( "Insert Time" );
	InvalidateLayout();

	g_pExpressionTool->LayoutItems( true );
	g_pExpressionTool->redraw();
	g_pGestureTool->redraw();
	g_pRampTool->redraw();
	g_pSceneRampTool->redraw();
}

void CChoreoView::CheckDeleteTime( CChoreoEvent *e, float dt, float starttime, float endtime, bool& deleteEvent )
{
	deleteEvent = false;

	// Not influenced
	float eventend = e->HasEndTime() ? e->GetEndTime() : e->GetStartTime();

	if ( eventend < starttime )
	{
		return;
	}

	// On right side of start mark, just shift left
	if ( e->GetStartTime() > starttime )
	{
		// If it has no duration and it's in the bounds then kill it.
		if ( !e->HasEndTime() && e->GetStartTime() < endtime )
		{
			deleteEvent = true;
			return;
		}
		else
		{
			float shift = e->GetStartTime() - starttime;
			float maxoffset = min( dt, shift );
	
			e->OffsetTime( -maxoffset );
			e->SnapTimes();
		}
	}
	else if ( !e->IsFixedLength() && e->HasEndTime() ) // the event starts before start, but ends after start time, need to insert space into the event, act like user dragged end time
	{
		float shiftend = e->GetEndTime() - starttime;
		float maxoffset = min( dt, shiftend );

		float newduration = e->GetDuration() - maxoffset;
		if ( newduration <= 0.0f )
		{
			deleteEvent = true;
			return;
		}
		else
		{
			RescaleRamp( e, newduration );
			switch ( e->GetType() )
			{
			default:
				break;
			case CChoreoEvent::GESTURE:
				{
					e->RescaleGestureTimes( e->GetStartTime(), e->GetEndTime() - maxoffset, true );
				}
				break;
			case CChoreoEvent::FLEXANIMATION:
				{
					RescaleExpressionTimes( e, e->GetStartTime(), e->GetEndTime() - maxoffset );
				}
				break;
			}
			e->OffsetEndTime( -maxoffset );
			e->SnapTimes();
			e->ResortRamp();
		}
	}

	switch ( e->GetType() )
	{
	default:
		break;
	case CChoreoEvent::SPEAK:
		{
			// Try and load wav to get length
			CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( e ) ) );
			if ( wave )
			{
				e->SetEndTime( e->GetStartTime() + wave->GetRunningLength() );
				delete wave;
			}
		}
		break;
	case CChoreoEvent::SEQUENCE:
		{
			CheckSequenceLength( e, false );
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			CheckGestureLength( e, false );
		}
		break;
	}
}

void CChoreoView::OnDeleteTime()
{
	if ( !m_rgABPoints[ 0 ].active &&
		 !m_rgABPoints[ 1 ].active  )
	{
		return;
	}

	Con_Printf( "OnDeleteTime()\n" );

	float starttime = m_rgABPoints[ 0 ].time;
	float endtime = m_rgABPoints[ 1 ].time;

	// Sort samples correctly
	if ( starttime > endtime )
	{
		float temp = starttime;
		starttime = endtime;
		endtime = temp;
	}

	float dt = endtime - starttime;
	if ( dt == 0.0f )
	{
		// Nothing to do...
		return;
	}

	SetDirty( true );

	PushUndo( "Delete Time" );

	CUtlVector< CChoreoEventWidget * > deletions;
	CUtlVector< CChoreoGlobalEventWidget * > global_deletions;

	for ( int i = 0; i < m_SceneActors.Size(); i++ )
	{
		CChoreoActorWidget *actor = m_SceneActors[ i ];
		if ( !actor )
			continue;

		for ( int j = 0; j < actor->GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = actor->GetChannel( j );
			if ( !channel )
				continue;

			for ( int k = 0; k < channel->GetNumEvents(); k++ )
			{
				CChoreoEventWidget *event = channel->GetEvent( k );
				if ( !event )
					continue;

				CChoreoEvent *e = event->GetEvent();
				if ( !e )
					continue;

				bool deleteEvent = false;

				CheckDeleteTime( e, dt, starttime, endtime, deleteEvent );
			
				if ( deleteEvent )
				{
					deletions.AddToTail( event );
				}
			}
		}
	}

	// Now handle global events, too
	for ( int i = 0; i < m_SceneGlobalEvents.Count(); i++ )
	{
		CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
		if ( !event )
			continue;

		CChoreoEvent *e = event->GetEvent();
		if ( !e )
			continue;

		bool deleteEvent = false;
		CheckDeleteTime( e, dt, starttime, endtime, deleteEvent );

		if ( deleteEvent )
		{
			global_deletions.AddToTail( event );
		}
	}

	for ( int i = 0; i < deletions.Count(); i++ )
	{
		CChoreoEventWidget *w = deletions[ i ];

		CChoreoEvent *e = w->GetEvent();
		CChoreoChannel *channel = e->GetChannel();
		if ( channel )
		{
			channel->RemoveEvent( e );
		}
		m_pScene->DeleteReferencedObjects( e );
	}

	for ( int i = 0; i < global_deletions.Count(); i++ )
	{
		CChoreoGlobalEventWidget *w = global_deletions[ i ];
		CChoreoEvent *e = w->GetEvent();
		m_pScene->DeleteReferencedObjects( e );
	}

	// Force scroll bars to recompute
	ForceScrollBarsToRecompute( false );

	if ( deletions.Count() > 0 || global_deletions.Count() > 0 )
	{
		DeleteSceneWidgets();
		CreateSceneWidgets();
	}

	PushRedo( "Delete Time" );

	InvalidateLayout();

	g_pExpressionTool->LayoutItems( true );
	g_pExpressionTool->redraw();
	g_pGestureTool->redraw();
	g_pRampTool->redraw();
	g_pSceneRampTool->redraw();
}

void CChoreoView::OnModelChanged()
{
	InvalidateTrackLookup();
	// OnCheckSequenceLengths();
}

void CChoreoView::SetShowCloseCaptionData( bool show )
{
	m_bShowCloseCaptionData = show;
}

bool CChoreoView::GetShowCloseCaptionData( void ) const
{
	return m_bShowCloseCaptionData;
}

void CChoreoView::OnToggleCloseCaptionTags()
{
	m_bShowCloseCaptionData = !m_bShowCloseCaptionData;
	InvalidateLayout();
}



static bool EventStartTimeLessFunc( CChoreoEvent * const &p1, CChoreoEvent * const  &p2 )
{
	CChoreoEvent *w1;
	CChoreoEvent *w2;

	w1 = const_cast< CChoreoEvent * >( p1 );
	w2 = const_cast< CChoreoEvent * >( p2 );

	return w1->GetStartTime() < w2->GetStartTime();
}

bool CChoreoView::GenerateCombinedFile( char const *outfilename, char const *cctoken, gender_t gender, CUtlRBTree< CChoreoEvent * >& sorted )
{
	CUtlVector< CombinerEntry > work;

	char actualfile[ 512 ];
	soundemitter->GenderExpandString( gender, outfilename, actualfile, sizeof( actualfile ) );
	if ( Q_strlen( actualfile ) <= 0 )
	{
		return false;
	}

	int i = sorted.FirstInorder();
	if ( i != sorted.InvalidIndex() )
	{
		CChoreoEvent *e = sorted[ i ];

		float startoffset = e->GetStartTime();

		do
		{
			e = sorted[ i ];

			float curoffset = e->GetStartTime();

			CombinerEntry ce;
			Q_snprintf( ce.wavefile, sizeof( ce.wavefile ), "sound/%s", FacePoser_TranslateSoundNameGender( e->GetParameters(), gender ) );
			ce.startoffset = curoffset - startoffset;

			work.AddToTail( ce );

			i = sorted.NextInorder( i );
		}
		while ( i != sorted.InvalidIndex() );
	}

	bool ok = soundcombiner->CombineSoundFiles( filesystem, actualfile, work );
	if ( !ok )
	{
		Con_ErrorPrintf( "Failed to create combined sound '%s':'%s'\n", cctoken, actualfile );
		return false;
	}
	Con_Printf( "Created combined sound '%s':'%s'\n", cctoken, actualfile );
	return true;
}

bool CChoreoView::ValidateCombinedFileCheckSum( char const *outfilename, char const *cctoken, gender_t gender, CUtlRBTree< CChoreoEvent * >& sorted )
{
	CUtlVector< CombinerEntry > work;

	char actualfile[ 512 ];
	soundemitter->GenderExpandString( gender, outfilename, actualfile, sizeof( actualfile ) );
	if ( Q_strlen( actualfile ) <= 0 )
	{
		return false;
	}

	int i = sorted.FirstInorder();
	if ( i != sorted.InvalidIndex() )
	{
		CChoreoEvent *e = sorted[ i ];

		float startoffset = e->GetStartTime();

		do
		{
			e = sorted[ i ];

			float curoffset = e->GetStartTime();

			CombinerEntry ce;
			Q_snprintf( ce.wavefile, sizeof( ce.wavefile ), "sound/%s", FacePoser_TranslateSoundNameGender( e->GetParameters(), gender ) );
			ce.startoffset = curoffset - startoffset;

			work.AddToTail( ce );

			i = sorted.NextInorder( i );
		}
		while ( i != sorted.InvalidIndex() );
	}

	return soundcombiner->IsCombinedFileChecksumValid( filesystem, actualfile, work );
}

void SuggestCaption( char *dest, int destlen, CUtlVector< CChoreoEvent * >& events )
{
	// Walk through events and concatenate current captions, or raw wav data if have any
	dest[ 0 ] = 0;

	int c = events.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CChoreoEvent *e = events[ i ];

		bool found = false;
		char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		if ( e->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
		{
			wchar_t *localized = g_pLocalize->Find( tok );
			if ( localized )
			{
				found = true;

				char ansi[ 1024 ];
				g_pLocalize->ConvertUnicodeToANSI( localized, ansi, sizeof( ansi ) );
				Q_strncat( dest, ansi, destlen, COPY_ALL_CHARACTERS );
			}
		}
		
		if ( !found )
		{
			// See if the wav file has data...
			CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( e ) ) );
			if ( wave )
			{
				CSentence *sentence = wave->GetSentence();
				if ( sentence )
				{
					Q_strncat( dest, sentence->GetText(), destlen, COPY_ALL_CHARACTERS );
					found = true;
				}
			}
		}

		if ( found && Q_strlen( dest ) > 0 && i != c - 1 )
		{
			Q_strncat( dest, " ", destlen, COPY_ALL_CHARACTERS );
		}
	}
}

void CChoreoView::OnCombineSpeakEvents()
{
	if ( !m_pScene )
		return;

	CChoreoChannel *firstChannel = NULL;

	CUtlVector< CChoreoEvent * >	selected;
	GetSelectedEvents( selected );

	int c = selected.Count();
	// Find the appropriate event by iterating across all actors and channels
	for ( int i = c - 1; i >= 0; --i )
	{
		CChoreoEvent *e = selected[ i ];

		if ( e->GetType() != CChoreoEvent::SPEAK )
		{
			Con_ErrorPrintf( "Can't combine events, all events must be SPEAK events.\n" );
			return;
		}

		if ( !firstChannel )
		{
			firstChannel = e->GetChannel();
		}
		else if ( e->GetChannel() != firstChannel )
		{
			Con_ErrorPrintf( "Can't combine events, all events must reside in the same channel.\n" );
			return;
		}
	}

	if ( selected.Count() < 2 )
	{
		Con_ErrorPrintf( "Can't combine events, must have at least two events selected.\n" );
		return;
	}

	// Let the user pick a CC phrase
	CCloseCaptionLookupParams params;
	Q_strncpy( params.m_szDialogTitle, "Choose Close Caption Token", sizeof( params.m_szDialogTitle ) );

	params.m_bPositionDialog = false;
	params.m_nLeft = 0;
	params.m_nTop = 0;

	char playbacktoken[ CChoreoEvent::MAX_CCTOKEN_STRING ];
	if ( !selected[0]->GetPlaybackCloseCaptionToken( playbacktoken, sizeof( playbacktoken ) ) )
	{
		return;
	}

	if ( !Q_stristr( playbacktoken, "_cc" ) )
	{
		Q_strncpy( params.m_szCCToken, va( "%s_cc", playbacktoken ), sizeof( params.m_szCCToken ) );
	}
	else
	{
		Q_strncpy( params.m_szCCToken, va( "%s", playbacktoken ), sizeof( params.m_szCCToken ) );
	}

	// User hit okay and value actually changed?
	if ( !CloseCaptionLookup( &params ) &&
		params.m_szCCToken[0] != 0 )
	{
		return;
	}

	// See if the token exists?
	StringIndex_t stringIndex = g_pLocalize->FindIndex( params.m_szCCToken );
	if ( INVALID_LOCALIZE_STRING_INDEX == stringIndex )
	{
		// Add token to closecaption_english file.
		// Guess at string and ask user to confirm.
		CInputParams ip;
		memset( &ip, 0, sizeof( ip ) );

		Q_strncpy( ip.m_szDialogTitle, "Add Close Caption", sizeof( ip.m_szDialogTitle ) );
		Q_snprintf( ip.m_szPrompt, sizeof( ip.m_szPrompt ), "Token (%s):", params.m_szCCToken );

		char suggested[ 2048 ];

		SuggestCaption( suggested, sizeof( suggested ), selected );

		Q_snprintf( ip.m_szInputText, sizeof( ip.m_szInputText ), "%s", suggested );

		if ( !InputProperties( &ip ) )
		{
			Con_Printf( "Combining of sound events cancelled\n" );
			return;
		}

		if ( Q_strlen( ip.m_szInputText ) == 0 )
		{
			Q_snprintf( ip.m_szInputText, sizeof( ip.m_szInputText ), "!!!%s", params.m_szCCToken );
		}

		char const *captionFile = "resource/closecaption_english.txt";

		if ( !filesystem->IsFileWritable( captionFile, "GAME" ) )
		{
			Warning( "Forcing %s to be writable!!!\n", captionFile );
			MakeFileWriteable( captionFile );
		}

		wchar_t unicode[ 2048 ];
		g_pLocalize->ConvertANSIToUnicode( ip.m_szInputText, unicode, sizeof( unicode ) );

		g_pLocalize->AddString( params.m_szCCToken, unicode, captionFile );
		g_pLocalize->SaveToFile( captionFile );
	}

	SetDirty( true );

	PushUndo( "Combine Sound Events" );

	c = selected.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		selected[ i ]->SetCloseCaptionToken( params.m_szCCToken );
	}

	PushRedo( "Combine Sound Events" );

	// Redraw
	InvalidateLayout();

	Con_Printf( "Changed %i events to use close caption token '%s'\n", c, params.m_szCCToken );

	// Sort the sounds by start time

	CUtlRBTree< CChoreoEvent * >  sorted( 0, 0, EventStartTimeLessFunc );

	// Sort items
	c = selected.Count();
	bool genderwildcard = false;
	for ( int i = 0; i < c; i++ )
	{
		CChoreoEvent *e = selected[ i ];
		sorted.Insert( e );

		// Get the sound entry name and use it to look up the gender info
		// Look up the sound level from the soundemitter system
		if ( !genderwildcard )
		{
			genderwildcard = soundemitter->IsUsingGenderToken( e->GetParameters() );
		}
	}


	char outfilename[ 512 ];
	Q_memset( outfilename, 0, sizeof( outfilename ) );

	CChoreoEvent *e = sorted[ sorted.FirstInorder() ];

	// Update whether we use the $gender token
	e->SetCombinedUsingGenderToken( genderwildcard );

	if ( !e->ComputeCombinedBaseFileName( outfilename, sizeof( outfilename ), genderwildcard ) )
	{
		Con_ErrorPrintf( "Unable to regenerate wav file name for combined sound\n" );
		return;
	}

	int soundindex = soundemitter->GetSoundIndex( e->GetParameters() );
	char const *scriptfile = soundemitter->GetSourceFileForSound( soundindex );
	if ( !scriptfile || !scriptfile[0] )
	{
		Con_ErrorPrintf( "Unable to find existing script to use for new combined sound entry.\n" );
		return;
	}

	// Create a new sound entry for this sound
	CAddSoundParams asp;
	Q_memset( &asp, 0, sizeof( asp ) );
	Q_strncpy( asp.m_szDialogTitle, "Add Combined Sound Entry", sizeof( asp.m_szDialogTitle ) );
	Q_strncpy( asp.m_szWaveFile, outfilename + Q_strlen( "sound/"), sizeof( asp.m_szWaveFile ) );
	Q_strncpy( asp.m_szScriptName, scriptfile, sizeof( asp.m_szScriptName ) );
	Q_strncpy( asp.m_szSoundName, params.m_szCCToken, sizeof( asp.m_szSoundName ) );

	asp.m_bAllowExistingSound = true;
	asp.m_bReadOnlySoundName = true;

	if ( !AddSound( &asp, (HWND)g_MDLViewer->getHandle() ) )
	{
		return;
	}

	if ( genderwildcard )
	{
		GenerateCombinedFile( outfilename, params.m_szCCToken, GENDER_MALE, sorted );
		GenerateCombinedFile( outfilename, params.m_szCCToken, GENDER_FEMALE, sorted );
	}
	else
	{
		GenerateCombinedFile( outfilename, params.m_szCCToken, GENDER_NONE, sorted );
	}
}

bool CChoreoView::ValidateCombinedSoundCheckSum( CChoreoEvent *e )
{
	if ( !e || e->GetType() != CChoreoEvent::SPEAK )
		return false;

	bool genderwildcard = e->IsCombinedUsingGenderToken();
	char outfilename[ 512 ];
	Q_memset( outfilename, 0, sizeof( outfilename ) );
	if ( !e->ComputeCombinedBaseFileName( outfilename, sizeof( outfilename ), genderwildcard ) )
	{
		Con_ErrorPrintf( "Unable to regenerate wav file name for combined sound (%s)\n", e->GetCloseCaptionToken() );
		return false;
	}

	bool checksumvalid = false;

	CUtlRBTree< CChoreoEvent * > eventList( 0, 0, EventStartTimeLessFunc );

	if ( !e->GetChannel()->GetSortedCombinedEventList( e->GetCloseCaptionToken(), eventList ) )
	{
		Con_ErrorPrintf( "Unable to generated combined event list (%s)\n", e->GetCloseCaptionToken() );
		return false;
	}


	if ( genderwildcard )
	{
		checksumvalid = ValidateCombinedFileCheckSum( outfilename, e->GetCloseCaptionToken(), GENDER_MALE, eventList );
		checksumvalid &= ValidateCombinedFileCheckSum( outfilename, e->GetCloseCaptionToken(), GENDER_FEMALE, eventList );
	}
	else
	{
		checksumvalid = ValidateCombinedFileCheckSum( outfilename, e->GetCloseCaptionToken(), GENDER_NONE, eventList );
	}

	return checksumvalid;
}

void CChoreoView::OnRemoveSpeakEventFromGroup()
{
	if ( !m_pScene )
		return;

	int i, c;

	CUtlVector< CChoreoEvent * >	selected;
	CUtlVector< CChoreoEvent * >	processlist;
	if ( GetSelectedEvents( selected ) > 0 )
	{

		int c = selected.Count();
		// Find the appropriate event by iterating across all actors and channels
		for ( i = c - 1; i >= 0; --i )
		{
			CChoreoEvent *e = selected[ i ];
			if ( e->GetType() != CChoreoEvent::SPEAK )
			{
				selected.Remove( i );
				continue;
			}

			if ( e->GetCloseCaptionType() == CChoreoEvent::CC_DISABLED )
			{
				selected.Remove( i );
				continue;
			}

			m_pClickedChannel->GetMasterAndSlaves( e, processlist );
		}
	}
	else
	{
		m_pClickedChannel->GetMasterAndSlaves( m_pClickedChannel->GetCaptionClickedEvent(), processlist );
	}

	if ( selected.Count() < 1 )
	{
		Con_ErrorPrintf( "No eligible SPEAK event selected.\n" );
		return;
	}

	SetDirty( true );

	PushUndo( "Remove speak event(s)" );

	c = processlist.Count();
	for ( i = 0 ; i < c; ++i )
	{
		processlist[ i ]->SetCloseCaptionToken( "" );
		processlist[ i ]->SetCloseCaptionType( CChoreoEvent::CC_MASTER );
		processlist[ i ]->SetUsingCombinedFile( false );
		processlist[ i ]->SetRequiredCombinedChecksum( 0 );
		processlist[ i ]->SetNumSlaves( 0 );
		processlist[ i ]->SetLastSlaveEndTime( 0.0f );
	}

	PushRedo( "Remove speak event(s)" );

	// Redraw
	InvalidateLayout();
	Con_Printf( "Reverted %i events to use default close caption token\n", c );
}

bool CChoreoView::AreSelectedEventsCombinable()
{
	CUtlVector< CChoreoEvent * > events;
	if ( GetSelectedEvents( events ) <= 0 )
		return false;

	CChoreoChannel *firstChannel = NULL;

	CUtlVector< CChoreoEvent * >	selected;
	GetSelectedEvents( selected );

	int c = selected.Count();
	// Find the appropriate event by iterating across all actors and channels
	for ( int i = c - 1; i >= 0; --i )
	{
		CChoreoEvent *e = selected[ i ];

		if ( e->GetType() != CChoreoEvent::SPEAK )
		{
			return false;
		}

		if ( !firstChannel )
		{
			firstChannel = e->GetChannel();
		}
		else if ( e->GetChannel() != firstChannel )
		{
			return false;
		}
	}
	return selected.Count() >= 2 ? true : false;
}

bool CChoreoView::AreSelectedEventsInSpeakGroup()
{
	CUtlVector< CChoreoEvent * > selected;
	if ( GetSelectedEvents( selected ) <= 0 )
	{
		if ( m_pClickedChannel )
		{
			CChoreoEvent *e = m_pClickedChannel->GetCaptionClickedEvent();
			if ( e && e->GetCloseCaptionType() == CChoreoEvent::CC_MASTER &&
				 e->GetNumSlaves() >= 1 )
			{
				return true;
			}
		}
		return false;
	}

	int c = selected.Count();
	// Find the appropriate event by iterating across all actors and channels
	for ( int i = c - 1; i >= 0; --i )
	{
		CChoreoEvent *e = selected[ i ];
		if ( e->GetType() != CChoreoEvent::SPEAK )
		{
			selected.Remove( i );
			continue;
		}

		if ( e->GetCloseCaptionType() == CChoreoEvent::CC_DISABLED )
		{
			selected.Remove( i );
			continue;
		}
	}

	return selected.Count() >= 1 ? true : false;

}

void CChoreoView::OnChangeCloseCaptionToken( CChoreoEvent *e )
{
	CCloseCaptionLookupParams params;
	Q_strncpy( params.m_szDialogTitle, "Close Caption Token Lookup", sizeof( params.m_szDialogTitle ) );

	params.m_bPositionDialog = false;
	params.m_nLeft = 0;
	params.m_nTop = 0;
	// strcpy( params.m_szPrompt, "Choose model:" );

	Q_strncpy( params.m_szCCToken, e->GetCloseCaptionToken(), sizeof( params.m_szCCToken ) );

	// User hit okay and value actually changed?
	if ( CloseCaptionLookup( &params ) && 
		Q_stricmp( e->GetCloseCaptionToken(), params.m_szCCToken ) )
	{
		char oldToken[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		Q_strncpy( oldToken, e->GetCloseCaptionToken(), sizeof( oldToken ) );

		CUtlVector< CChoreoEvent * > events;
		m_pClickedChannel->GetMasterAndSlaves( e, events );

		if ( events.Count() < 2 )
		{
			Con_ErrorPrintf( "Can't combine events, must have at least two events selected.\n" );
		}

		SetDirty( true );

		PushUndo( "Change closecaption token" );

		// Make the change...
		int c = events.Count();
		for ( int i = 0 ; i < c; ++i )
		{
			events[i]->SetCloseCaptionToken( params.m_szCCToken );
		}

		PushRedo( "Change closecaption token" );

		InvalidateLayout();

		Con_Printf( "Close Caption token for '%s' changed to '%s'\n", e->GetName(), params.m_szCCToken );
	}
}

void CChoreoView::OnToggleCloseCaptionsForEvent()
{
	if ( !m_pClickedChannel )
	{
		return;
	}

	CChoreoEvent *e = m_pClickedChannel->GetCaptionClickedEvent();

	if ( !e )
	{
		return;
	}

	CChoreoEvent::CLOSECAPTION newType = CChoreoEvent::CC_MASTER;
	// Can't mess with slave
	switch ( e->GetCloseCaptionType() )
	{
	default:
	case CChoreoEvent::CC_SLAVE:
		return;
	case CChoreoEvent::CC_MASTER:
		newType = CChoreoEvent::CC_DISABLED;
		break;
	case CChoreoEvent::CC_DISABLED:
		newType = CChoreoEvent::CC_MASTER;
		break;
	}

	SetDirty( true );

	PushUndo( "Enable/disable captions" );

	// Make the change...
	e->SetCloseCaptionType( newType );

	PushRedo( "Enable/disable captions" );

	InvalidateLayout();

	Con_Printf( "Close Caption type for '%s' changed to '%s'\n", e->GetName(), CChoreoEvent::NameForCCType( newType ) );

}

void CChoreoView::StopScene()
{
	SetScrubTargetTime( m_flScrub );
	FinishSimulation();
	sound->Flush();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
template <class T>
void DeleteAllAndPurge( T &tree )
{
	T::IndexType_t i;

	for ( i = tree.FirstInorder(); i != T::InvalidIndex(); i = tree.NextInorder( i ) )
	{
		delete tree[i];
	}

	tree.Purge();
}

void CChoreoView::OnPlaceNextSpeakEvent()
{
	CUtlVector< CChoreoEvent * > list;
	GetSelectedEvents( list );
	if ( list.Count() != 1 )
	{
		Warning( "Can't place sound event, nothing selected\n" );
		return;
	}

	CChoreoEvent *ev = list[ 0 ];
	if ( ev->GetType() != CChoreoEvent::SPEAK )
	{
		Warning( "Can't place sound event, no previous sound event selected\n" );
		return;
	}

	CChoreoChannelWidget *widget = FindChannelForEvent( ev );
	if ( !widget )
	{
		Warning( "Can't place sound event, can't find channel widget for event\n" );
		return;
	}

	CChoreoChannel *channel = widget->GetChannel();
	if ( !channel )
	{
		Warning( "Can't place sound event, can't find channel for new event\n" );
		return;
	}

	CUtlRBTree< char const *, int >		m_SortedNames( 0, 0, NameLessFunc );

	int c = soundemitter->GetSoundCount();
	for ( int i = 0; i < c; i++ )
	{
		char const *name = soundemitter->GetSoundName( i );
		if ( name && name[ 0 ] )
		{
			m_SortedNames.Insert( strdup( name ) );
		}
	}

	int idx = m_SortedNames.Find( ev->GetParameters() );
	if ( idx == m_SortedNames.InvalidIndex() )
	{
		Warning( "Can't place sound event, can't find '%s' in sound list\n", ev->GetParameters() );
		DeleteAllAndPurge( m_SortedNames );
		return;
	}

	int nextIdx = m_SortedNames.NextInorder( idx );
	if ( nextIdx == m_SortedNames.InvalidIndex() )
	{
		Warning( "Can't place sound event, can't next sound after '%s' in sound list\n", ev->GetParameters() );
		DeleteAllAndPurge( m_SortedNames );
		return;
	}

	DeselectAll();

	SetDirty( true );

	PushUndo( "Place Next Speak Event" );

	CChoreoEvent *event = m_pScene->AllocEvent();
	Assert( event );
	if ( event )
	{
		// Copy everything for source event
		*event = *ev;

		event->SetParameters( m_SortedNames[ nextIdx ] );
		// Start it at the end time...
		event->SetStartTime( event->GetEndTime() );
		event->SetResumeCondition( false );
		event->ClearAllRelativeTags();
		event->ClearAllTimingTags();
		event->ClearAllAbsoluteTags( CChoreoEvent::PLAYBACK );
		event->ClearAllAbsoluteTags( CChoreoEvent::ORIGINAL );

		event->SetChannel( channel );
		event->SetActor( channel->GetActor() );

		// Try and load wav to get length
		CAudioSource *wave = sound->LoadSound( va( "sound/%s", FacePoser_TranslateSoundName( event ) ) );
		if ( wave )
		{
			event->SetEndTime( event->GetStartTime() + wave->GetRunningLength() );
			delete wave;
		}

		DeleteSceneWidgets();

		// Add to appropriate channel
		channel->AddEvent( event );

		CreateSceneWidgets();

		CChoreoEventWidget *eventWidget = FindWidgetForEvent( event );
		if ( eventWidget )
		{
			eventWidget->SetSelected( true );
		}
	
		// Redraw
		InvalidateLayout();
	}

	PushRedo( "Place Next Speak Event" );

	DeleteAllAndPurge( m_SortedNames );
}

enum
{
	FM_LEFT = 0,
	FM_RIGHT,
	FM_SMALLESTWIDE,
	FM_LARGESTWIDE
};

static int FindMetric( int type, CUtlVector< CChoreoEvent * > &list, float& value )
{
	float bestVal = 999999.0f;
	int bestIndex = -1;
	bool greater = true;

	switch ( type )
	{
	default:
	case FM_LEFT:
	case FM_SMALLESTWIDE:
		greater = false;
		break;
	case FM_RIGHT:
	case FM_LARGESTWIDE:
		bestVal = -bestVal;
		greater = true;
		break;
	}
	int c = list.Count();
	for ( int i = 0; i < c; ++i )
	{
		CChoreoEvent *e = list[ i ];
		if ( type != FM_LEFT &&
			!e->HasEndTime() )
			continue;

		float val;
		switch ( type )
		{
		default:
		case FM_LEFT:
			val = e->GetStartTime();
			break;
		case FM_RIGHT:
			val = e->GetEndTime();
			break;
		case FM_SMALLESTWIDE:
		case FM_LARGESTWIDE:
			val = e->GetDuration();
			break;
		}

		if ( greater )
		{
			if ( val <= bestVal )
				continue;
		}
		else
		{
			if ( val >= bestVal )
				continue;
		}

		bestVal		= val;
		bestIndex	= i;
	}

	value = bestVal;
	return bestIndex;
}

void CChoreoView::OnAlign( bool left )
{
	CUtlVector< CChoreoEvent * > list;
	GetSelectedEvents( list );

	if ( left )
	{
		for ( int i = 0; i < m_SceneGlobalEvents.Size(); i++ )
		{
			CChoreoGlobalEventWidget *event = m_SceneGlobalEvents[ i ];
			if ( !event || !event->IsSelected() )
				continue;

			list.AddToTail( event->GetEvent() );
		}
	}

	int numSel = list.Count();
	if ( numSel < 2 )
	{
		Warning( "Can't align, must have at least two events selected\n" );
		return;
	}

	float value;
	int idx = FindMetric( left ? FM_LEFT : FM_RIGHT, list, value );
	if ( idx == -1 )
	{
		return;
	}

	SetDirty( true );

	char undotext[ 128 ];
	Q_snprintf( undotext, sizeof( undotext ), "Align %s", left ? "Left" : "Right" );
	PushUndo( undotext );

	for ( int i = 0; i < numSel; ++i )
	{
		if ( i == idx )
			continue;

		CChoreoEvent *e = list[ i ];

		float newStartTime = left ? value : ( value - e->GetDuration() );
		float offset = newStartTime - e->GetStartTime();
		e->OffsetTime( offset );
	}

	PushRedo( undotext );

	InvalidateLayout();
}

void CChoreoView::OnMakeSameSize( bool smallest )
{
	CUtlVector< CChoreoEvent * > list;
	int numSel = GetSelectedEvents( list );
	if ( numSel < 2 )
	{
		Warning( "Can't align, must have at least two events selected\n" );
		return;
	}

	float value;
	int idx = FindMetric( smallest ? FM_SMALLESTWIDE : FM_LARGESTWIDE, list, value );
	if ( idx == -1 )
	{
		return;
	}

	SetDirty( true );

	char undotext[ 128 ];
	Q_snprintf( undotext, sizeof( undotext ), "Size to %s", smallest ? "Smallest" : "Largest" );
	PushUndo( undotext );

	for ( int i = 0; i < numSel; ++i )
	{
		if ( i == idx )
			continue;

		list[ i ]->SetEndTime( list[ i ]->GetStartTime() + value );
	}

	PushRedo( undotext );

	InvalidateLayout();
}

void CChoreoView::SelectAllEventsInActor( CChoreoActorWidget *actor )
{
	TraverseWidgets( &CChoreoView::SelectInActor, actor );
	redraw();
}

void CChoreoView::SelectAllEventsInChannel( CChoreoChannelWidget *channel )
{
	TraverseWidgets( &CChoreoView::SelectInChannel, channel );
	redraw();
}

void CChoreoView::SelectInActor( CChoreoWidget *widget, CChoreoWidget *param1 )
{
	CChoreoEventWidget *ev = dynamic_cast< CChoreoEventWidget * >( widget );
	if ( !ev )
		return;

	if ( ev->IsSelected() )
		return;

	CChoreoChannel *ch = ev->GetEvent()->GetChannel();
	if ( !ch )
		return;
	CChoreoActor *actor = ch->GetActor();
	if ( !actor )
		return;

	CChoreoActorWidget *actorw = dynamic_cast< CChoreoActorWidget * >( param1 );
	if ( !actorw )
		return;

	if ( actorw->GetActor() != actor )
		return;

	widget->SetSelected( true );
}

void CChoreoView::SelectInChannel( CChoreoWidget *widget, CChoreoWidget *param1 )
{
	CChoreoEventWidget *ev = dynamic_cast< CChoreoEventWidget * >( widget );
	if ( !ev )
		return;

	if ( ev->IsSelected() )
		return;

	CChoreoChannel *ch = ev->GetEvent()->GetChannel();
	if ( !ch )
		return;

	CChoreoChannelWidget *chw = dynamic_cast< CChoreoChannelWidget * >( param1 );
	if ( !chw )
		return;

	if ( chw->GetChannel() != ch )
		return;

	widget->SetSelected( true );
}

