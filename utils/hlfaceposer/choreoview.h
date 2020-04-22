//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOVIEW_H
#define CHOREOVIEW_H
#ifdef _WIN32
#pragma once
#endif


#include <mxtk/mx.h>
#include <mxtk/mxWindow.h>
#include "mxBitmapButton.h"
#include "utlvector.h"
#include "ChoreoWidget.h"
#include "ichoreoeventcallback.h"
#include "faceposertoolwindow.h"
#include "ChoreoEvent.h"
#include "mathlib/mathlib.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#define IDC_CV_CC_LANGUAGESTART			5300

#define IDC_STOPSCENE		5000
#define IDC_PLAYSCENE		5001
#define IDC_PAUSESCENE		5002
#define IDC_CHOREOVSCROLL	5003
#define IDC_CHOREOHSCROLL	5004

#define IDC_ADDACTOR		5005
#define IDC_DELETEACTOR		5006
#define IDC_MOVEACTORUP		5007
#define IDC_MOVEACTORDOWN	5008
#define IDC_EDITACTOR		5009

#define IDC_EDITEVENT		5010
#define IDC_DELETEEVENT		5011

#define IDC_ADDEVENT_EXPRESSION		5012
#define IDC_ADDEVENT_GESTURE		5013
#define IDC_ADDEVENT_LOOKAT			5014
#define IDC_ADDEVENT_MOVETO			5015
#define IDC_ADDEVENT_SPEAK			5016
#define IDC_ADDEVENT_FACE			5017
#define IDC_ADDEVENT_FIRETRIGGER	5018
#define IDC_ADDEVENT_SEQUENCE		5019
#define IDC_ADDEVENT_GENERIC		5020

#define	IDC_CV_CHANGESCALE			5021

#define IDC_EDITGLOBALEVENT			5022
#define IDC_DELETEGLOBALEVENT		5023
#define IDC_ADDEVENT_PAUSE			5024

#define IDC_ADDCHANNEL		5025
#define IDC_EDITCHANNEL		5026
#define IDC_DELETECHANNEL	5027
#define IDC_MOVECHANNELUP	5028
#define IDC_MOVECHANNELDOWN	5029

#define IDC_CHANNELOPEN		5030
#define IDC_CHANNELCLOSE	5031
#define IDC_CBACTORACTIVE	5032
#define IDC_DELETERELATIVETAG 5033
#define IDC_ADDTIMINGTAG	5034

#define IDC_SELECTALL		5035
#define IDC_DESELECTALL		5036
#define IDC_MOVETOBACK		5037

#define IDC_UNDO			5038
#define IDC_REDO			5039

#define IDC_EXPRESSIONTOOL	5040
#define IDC_ASSOCIATEBSP	5041

#define IDC_ADDEVENT_FLEXANIMATION 5042 

#define IDC_COPYEVENTS		5043
#define IDC_PASTEEVENTS		5044

#define IDC_IMPORTEVENTS	5045
#define IDC_EXPORTEVENTS	5046

#define IDC_ADDEVENT_SUBSCENE		5047
#define IDC_PLAYSCENE_BACKWARD		5048

#define IDC_ASSOCIATEMODEL			5049
#define IDC_CHOREO_PLAYBACKRATE			5050

#define IDC_CV_CHECKSEQLENGTHS		5051
#define IDC_CV_PROCESSSEQUENCES		5052

#define IDC_GESTURETOOL				5053

#define IDC_ADDEVENT_LOOP			5054
#define IDC_CV_TOGGLERAMPONLY		5055

#define IDC_ADDEVENT_INTERRUPT		5056
#define IDC_ADDEVENT_STOPPOINT		5067

#define	SCENE_ACTION_UNKNOWN		0
#define SCENE_ACTION_CANCEL			1
#define SCENE_ACTION_RESUME			2

#define SCENE_ANIMLAYER_SEQUENCE	0
#define SCENE_ANIMLAYER_GESTURE		1

#define IDC_ADDEVENT_NULLGESTURE	5068

#define IDC_SELECTEVENTS_ALL_BEFORE			5069
#define IDC_SELECTEVENTS_ALL_AFTER			5070
#define IDC_SELECTEVENTS_ACTIVE_BEFORE		5071
#define IDC_SELECTEVENTS_ACTIVE_AFTER		5072
#define IDC_SELECTEVENTS_CHANNEL_BEFORE		5073
#define IDC_SELECTEVENTS_CHANNEL_AFTER		5074

#define IDC_INSERT_TIME	5075
#define IDC_DELETE_TIME 5076

#define IDC_EXPORT_VCD		5077
#define IDC_IMPORT_VCD		5078

#define IDC_ADDEVENT_PERMITRESPONSES	5079

#define IDC_CV_CC_SHOW						5080
#define IDC_CV_COMBINESPEAKEVENTS			5081
#define IDC_CV_REMOVESPEAKEVENTFROMGROUP	5082
#define IDC_CV_CHANGECLOSECAPTIONTOKEN		5083
#define	IDC_CV_TOGGLECLOSECAPTIONS			5084

#define IDC_CV_ALIGN_LEFT						5085
#define IDC_CV_ALIGN_RIGHT						5086
#define IDC_CV_SAMESIZE_SMALLEST				5087
#define IDC_CV_SAMESIZE_LARGEST					5088

#define IDC_CV_ALLEVENTS_CHANNEL				5089
#define IDC_CV_ALLEVENTS_ACTOR					5090

#define IDC_CV_ENABLEEVENTS						5091
#define IDC_CV_DISABLEEVENTS					5092

/////////////////////////////////////////////////////////////////////////////
// CChoreoView window
class CChoreoScene;
class CChoreoEvent;
class CChoreoActor;
class CChoreoChannel;

class CChoreoActorWidget;
class CChoreoChannelWidget;
class CChoreoEventWidget;
class CChoreoGlobalEventWidget;
class CChoreoWidgetDrawHelper;

class PhonemeEditor;
class CExpression;
class CExpClass;
class StudioModel;

//-----------------------------------------------------------------------------
// Purpose: The main view of the choreography data for a scene
//-----------------------------------------------------------------------------
class CChoreoView : public mxWindow, public IFacePoserToolWindow, public IChoreoEventCallback
{
// Construction
public:
						CChoreoView( mxWindow *parent, int x, int y, int w, int h, int id );
	virtual				~CChoreoView();

	virtual void		OnModelChanged();

	virtual void		OnDelete();
	virtual bool		CanClose();

	void				InvalidateTrackLookup( void );

	// Implements IChoreoEventCallback
	virtual void		StartEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual void		EndEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual void		ProcessEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual	bool		CheckEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	void				SetChoreoFile( char const *filename );
	char const			*GetChoreoFile( void ) const;

	// Scene load/unload
	void				LoadSceneFromFile( const char *filename );
	CChoreoScene		*LoadScene( char const *filename );
	void				UnloadScene( void );

	// UI
	void				New( void );
	void				Save( void );
	void				SaveAs( void );
	void				Load( void );
	void				LoadNext( void );
	bool				Close( void );

	// Drag/drop from expression thumbnail

	bool				CreateExpressionEvent( int mx, int my, CExpClass *cl, CExpression *exp );
	bool				CreateAnimationEvent( int mx, int my, char const *animationname );

	void				SelectAll( void );
	void				DeselectAll( void );

	struct SelectionParams_t
	{
		enum
		{
			SP_CHANNEL = 0,
			SP_ACTIVE,
			SP_ALL
		};

		float			time;
		bool			forward;
		int				type;
	};

	void				SelectEvents( SelectionParams_t& params );

	// Channel/actor right click menu
	void				NewChannel( void );
	void				DeleteChannel( CChoreoChannel *channel );
	void				MoveChannelUp( CChoreoChannel *channel );
	void				MoveChannelDown( CChoreoChannel *channel );
	void				EditChannel( CChoreoChannel *channel );
	void				AddEvent( int type, int subtype = 0, char const *defaultparameters = NULL );

	// Actor right click menu
	void				NewActor( void );
	void				DeleteActor( CChoreoActor *actor );
	void				MoveActorUp( CChoreoActor *actor );
	void				MoveActorDown( CChoreoActor *actor );
	void				EditActor( CChoreoActor *actor );

	// Event menu
	void				EditEvent( CChoreoEvent *event );
	void				DeleteSelectedEvents( void );
	void				EnableSelectedEvents( bool state );
	void				DeleteEventRelativeTag( CChoreoEvent *event, int tagnum );
	void				AddEventRelativeTag( void );
	CChoreoEventWidget	*FindWidgetForEvent( CChoreoEvent *event );
	CChoreoChannelWidget *FindChannelForEvent( CChoreoEvent *event );

	void				MoveEventToBack( CChoreoEvent *event );
	void				OnExpressionTool( void );
	void				OnGestureTool( void );

	// Global event ( pause ) menu
	void				EditGlobalEvent( CChoreoEvent *event );
	void				DeleteGlobalEvent( CChoreoEvent *event );
	void				AddGlobalEvent( CChoreoEvent::EVENTTYPE type );

	void				AssociateBSP( void );
	void				AssociateModel( void );
	void				AssociateModelToActor( CChoreoActor *actor, int modelindex );

	// UI Layout
	void				CreateSceneWidgets( void );
	void				DeleteSceneWidgets( void );
	void				LayoutScene( void );
	void				InvalidateLayout( void );
	void				ForceScrollBarsToRecompute( bool resetthumb );

	// Layout data that children should obey
	int					GetLabelWidth( void );
	int					GetStartRow( void );
	int					GetRowHeight( void );
	int					GetFontSize( void );
	int					GetEndRow( void );

	// Simulation
	void				PlayScene( bool forward );
	void				PauseScene( void );

	bool				IsPlayingScene( void );
	void				ResetTargetSettings( void );
	void				UpdateCurrentSettings( void );

	virtual void		Think( float dt );
	virtual bool		IsScrubbing( void ) const;
	virtual bool		IsProcessing( void );


	bool				ShouldProcessSpeak( void );

	void				SceneThink( float time );
	void				PauseThink( void );
	void				FinishSimulation( void );
	void				StopScene();

	float				GetStartTime( void );
	float				GetEndTime( void );
	void				SetStartTime( float time );

	void 				ProcessExpression( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessFlexAnimation( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessLookat( CChoreoScene *scene, CChoreoEvent *event );
	bool				GetTarget( CChoreoScene *scene, CChoreoEvent *event, Vector &vecTarget, QAngle &vecAngle );
	void				ProcessFace( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessGesture( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessSequence( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessMoveto( CChoreoScene *scene, CChoreoEvent *event );
	int					GetMovetoSequence( CChoreoScene *scene, CChoreoEvent *event, StudioModel *model );
	void				ProcessSubscene( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessPause( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessSpeak( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessLoop( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessInterrupt( CChoreoScene *scene, CChoreoEvent *event );
	void				ProcessPermitResponses( CChoreoScene *scene, CChoreoEvent *event );

	float				GetPixelsPerSecond( void );

	// mxWindow overrides
	virtual void		redraw();
	virtual bool		PaintBackground( void );
	virtual int			handleEvent( mxEvent *event );

	// Draw helpers
	void				DrawSceneABTicks( CChoreoWidgetDrawHelper& drawHelper );
	void				DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right );
	void				DrawBackground( CChoreoWidgetDrawHelper& drawHelper, RECT& rc );
	void				DrawRelativeTagLines( CChoreoWidgetDrawHelper& drawHelper, RECT& rc );

	// Remap click position to/from time value
	float				GetTimeValueForMouse( int mx, bool clip = false );
	int					GetPixelForTimeValue( float time, bool *clipped = NULL );
	float				GetTimeDeltaForMouseDelta( int mx, int origmx );

	// Readjust slider
	void				MoveTimeSliderToPos( int x );

	// Dirty flag for file save prompting

	bool				GetDirty( void );
	void				SetDirty( bool dirty, bool clearundo = true );

	void				ShowContextMenu( int mx, int my );

	// Caller must first translate mouse into screen coordinates
	bool				IsMouseOverTimeline( int mx, int my );

	CChoreoActorWidget	*GetActorUnderCursorPos( int mx, int my );
	CChoreoChannelWidget *GetChannelUnderCursorPos( int mx, int my );
	CChoreoEventWidget	*GetEventUnderCursorPos( int mx, int my );
	CChoreoGlobalEventWidget *GetGlobalEventUnderCursorPos( int mx, int my );
	int					GetTagUnderCursorPos( CChoreoEventWidget *event, int mx, int my );
	CEventAbsoluteTag	*GetAbsoluteTagUnderCursorPos( CChoreoEventWidget *event, int mx, int my );


	void				GetObjectsUnderMouse( int mx, int my, 
							CChoreoActorWidget **actor,
							CChoreoChannelWidget **channel, 
							CChoreoEventWidget **event,
							CChoreoGlobalEventWidget **globalevent,
							int* clickedTag,
							CEventAbsoluteTag **absolutetag,
							int *clickedCCButton );

	void				OnDoubleClicked( void );

	void				ApplyBounds( int& mx, int& my );
	void				CalcBounds( int movetype );

	void				MouseStartDrag( mxEvent *event, int mx, int my );
	void				MouseContinueDrag( mxEvent *event, int mx, int my );
	void				MouseFinishDrag( mxEvent *event, int mx, int my );
	void				MouseMove( int mx, int my );

	//void				StartDraggingGlobalEvent( int mx, int my );
	void				StartDraggingEvent( int mx, int my );
	//void				FinishDraggingGlobalEvent( int mx, int my );	
	void				FinishDraggingEvent( mxEvent *event, int mx, int my );

	// Draw focus rect while mouse dragging is going on
	void				DrawFocusRect( void );
	int					ComputeEventDragType( int mx, int my );

	void				SetCurrentWaveFile( const char *filename, CChoreoEvent *event );
	
	void				RecomputeWaves();

	typedef void (CChoreoView::*CVMEMBERFUNC)( CChoreoWidget *widget, CChoreoWidget *param1 );

	void				TraverseWidgets( CVMEMBERFUNC pfn, CChoreoWidget *param1 );

	int					CountSelectedEvents( void );
	int					CountSelectedGlobalEvents( void );
	int					GetSelectedEvents( CUtlVector< CChoreoEvent * >& events );
	int					GetSelectedEventWidgets( CUtlVector< CChoreoEventWidget * >& events );
	int					GetEarliestEventIndex( CUtlVector< CChoreoEventWidget * >& events );
	int					GetLatestEventIndex( CUtlVector< CChoreoEventWidget * >& events );

	// Traversal functions
	void				Deselect( CChoreoWidget *widget, CChoreoWidget *param1 );
	void				Select( CChoreoWidget *widget, CChoreoWidget *param1 );
	void				SelectInActor( CChoreoWidget *widget, CChoreoWidget *param1 );
	void				SelectInChannel( CChoreoWidget *widget, CChoreoWidget *param1 );

	void				SelectAllEvents( CChoreoWidget *widget, CChoreoWidget *param1 );

	void				SelectAllEventsInActor( CChoreoActorWidget *actor );
	void				SelectAllEventsInChannel( CChoreoChannelWidget *channel );

	// Adjust scroll bars
	void				RepositionVSlider( void );
	void				RepositionHSlider( void );

	void				UpdateStatusArea( int mx, int my );
	void				ClearStatusArea( void );
	void				RedrawStatusArea( CChoreoWidgetDrawHelper& drawHelper, RECT& rcStatus );

	void				GetUndoLevels( int& current, int& number );

	// Undo/Redo
	void				Undo( void );
	void				Redo( void );

	bool				CanUndo();
	bool				CanRedo();	

	// Do push before changes
	void				PushUndo( const char *description );
	// Do this push after changes, must match pushundo 1for1
	void				PushRedo( const char *description );

	void				WipeUndo( void );
	void				WipeRedo( void );

	const char			*GetUndoDescription( void );
	const char			*GetRedoDescription( void );

	CChoreoScene		*GetScene( void );

	void				ReportSceneClearToTools( void );

	void				CopyEvents( void );
	void				PasteEvents( void );
	void				ImportEvents( void );
	void				ExportEvents( void );

	void				ExportVCD();
	void				ImportVCD();

	void				ExportVCDFile( char const *filename );
	void				ImportVCDFile( char const *filename );

	bool				CanPaste( void );

	bool				IsMouseOverScrubHandle( mxEvent *event );
	bool				IsMouseOverScrubArea( mxEvent *event );

	void				GetScrubHandleRect( RECT& rcHandle, bool clipped = false );
	void				GetScrubAreaRect( RECT& rcArea );
	void				DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper );
	void				DrawScrubHandle( void );
	void				ScrubThink( float dt, bool scrubbing, IFacePoserToolWindow *invoker );

	void				ClampTimeToSelectionInterval( float& timeval );

	void				SetScrubTime( float t );
	void				SetScrubTargetTime( float t );

	void				OnCheckSequenceLengths( void );

	bool				IsRampOnly( void ) const;

	void				SetTimeZoom( const char *tool, int tz, bool preserveFocus );
    int					GetTimeZoom( const char *tool );
	template< class T >
	void				SetPreservedTimeZoom( T *other, int tz );
	template< class T >
	int					HandleZoomKey( T *other, int keyCode );

	void				OnInsertTime();
	void				OnDeleteTime();

	bool				GetShowCloseCaptionData( void ) const;
	bool				ValidateCombinedSoundCheckSum( CChoreoEvent *e );

	void				OnPlaceNextSpeakEvent();

private:

	void				CheckInsertTime( CChoreoEvent *e, float dt, float starttime, float endtime );
	void				CheckDeleteTime( CChoreoEvent *d, float dt, float starttime, float endtime, bool& deleteEvent );

	bool				FixupSequenceDurations( CChoreoScene *scene, bool checkonly );
	bool				CheckSequenceLength( CChoreoEvent *e, bool checkonly );
	bool				CheckGestureLength( CChoreoEvent *e, bool checkonly );
	bool				DefaultGestureLength( CChoreoEvent *e, bool checkonly );
	bool				AutoaddGestureKeys( CChoreoEvent *e, bool checkonly );

	void				InvalidateTrackLookup_R( CChoreoScene *scene );

	void				ShowButtons( bool show );

	// Compute full size of data in pixels, for setting up scroll bars
	int					ComputeVPixelsNeeded( void );
	int					ComputeHPixelsNeeded( void );
	void				PositionControls();

	void				OnChangeScale();

	float				FindNextEventTime( CChoreoEvent::EVENTTYPE type, CChoreoChannel *channel, CChoreoEvent *e, bool forward );

	bool				ShouldSelectEvent( SelectionParams_t &params, CChoreoEvent *event );

	bool				IsMouseOverSceneEndTime( int mx );

	void				StartDraggingSceneEndTime( int mx, int my );
	void				FinishDraggingSceneEndTime( mxEvent *event, int mx, int my );

	void				SetShowCloseCaptionData( bool show );

	void				OnToggleCloseCaptionTags();
	void				OnCombineSpeakEvents();
	void				OnRemoveSpeakEventFromGroup();
	void				OnChangeCloseCaptionToken( CChoreoEvent *e );

	bool				AreSelectedEventsCombinable();
	bool				AreSelectedEventsInSpeakGroup();

	void				OnToggleCloseCaptionsForEvent();

	bool				GenerateCombinedFile( char const *outfilename, const char *cctoken, gender_t gender, CUtlRBTree< CChoreoEvent * >& sorted );
	bool				ValidateCombinedFileCheckSum( char const *outfilename, char const *cctoken, gender_t gender, CUtlRBTree< CChoreoEvent * >& sorted );

	void				RememberSelectedEvents( CUtlVector< CChoreoEvent * >& list );
	void				ReselectEvents( CUtlVector< CChoreoEvent * >& list );
	
	void				OnAlign( bool left );
	void				OnMakeSameSize( bool smallest );

	bool				IsMouseOverEventEdge( CChoreoEventWidget *ew, bool bLeftEdge, int mx, int my );
	bool				IsMouseOverEvent( CChoreoEventWidget *ew, int mx, int my );
	typedef struct
	{
		bool	active;
		float	time;
	} SCENEAB;

	void				PlaceABPoint( int mx );
	void				ClearABPoints( void );

	SCENEAB				m_rgABPoints[ 2 ];
	int					m_nCurrentABPoint;

	bool				m_bForward;

	// The underlying scene we are editing
	CChoreoScene		*m_pScene;

	enum
	{
		MAX_ACTORS = 32
	};

	typedef struct
	{
		bool	expanded;
	} ACTORSTATE;

	ACTORSTATE									m_ActorExpanded[ MAX_ACTORS ];
	// The scene's ui actors
	CUtlVector < CChoreoActorWidget * >			m_SceneActors;
	// The scenes segment markers
	CUtlVector < CChoreoGlobalEventWidget * >	m_SceneGlobalEvents;

	// How many pixels per second we are showing in the UI
	float				m_flPixelsPerSecond;

	// Do we need to move controls?
	bool				m_bLayoutIsValid;

	// Starting row of first actor
	int					m_nStartRow;
	// How wide the actor/channel name area is
	int					m_nLabelWidth;
	// Height between channel/actor names
	int					m_nRowHeight;
	// Font size for drawing event labels
	int					m_nFontSize;

	// Height/width of scroll bars
	int					m_nScrollbarHeight;
	// Height off info area for flyover info / help
	int					m_nInfoHeight;

	// Simulation info
	float				m_flStartTime;
	float				m_flEndTime;

	float				m_flFrameTime;
	bool				m_bSimulating;
	bool				m_bPaused;
	float				m_flLastSpeedScale;
	bool				m_bResetSpeedScale;
	bool				m_bAutomated;
	int					m_nAutomatedAction;
	float				m_flAutomationDelay;
	float				m_flAutomationTime;

	// Some rectangles for the UI
	RECT				m_rcTitles;
	RECT				m_rcTimeLine;

	// Play/pause buttons for simulation
	mxBitmapButton		*m_btnPlay;
	mxBitmapButton		*m_btnPause;
	mxBitmapButton		*m_btnStop;

	mxSlider			*m_pPlaybackRate;
	float				m_flPlaybackRate;

	// The scroll bars
	mxScrollbar			*m_pVertScrollBar;
	mxScrollbar			*m_pHorzScrollBar;
	int					m_nLastHPixelsNeeded;
	int					m_nLastVPixelsNeeded;

	// Current sb values
	int					m_nTopOffset;
	float				m_flLeftOffset;

	// Need save?
	bool				m_bDirty;
	// Currently loaded scene file
	char				m_szChoreoFile[ 256 ];

	CChoreoActorWidget	*m_pClickedActor;
	CChoreoChannelWidget	*m_pClickedChannel;
	CChoreoEventWidget	*m_pClickedEvent;
	CChoreoGlobalEventWidget *m_pClickedGlobalEvent;
	// Relative to the clicked event
	int					m_nClickedTag;
	CEventAbsoluteTag	*m_pClickedAbsoluteTag;
	int					m_nSelectedEvents;
	int					m_nClickedX, m_nClickedY;
	int					m_nClickedChannelCloseCaptionButton;

	// For mouse dragging
		// How close to right or left edge the mouse has to be before the wider/shorten
	//  cursor shows up
	enum
	{
		DRAG_EVENT_EDGE_TOLERANCE = 5,
	};

	// When dragging, what time of action is being performed
	enum
	{
		DRAGTYPE_NONE = 0,
		DRAGTYPE_EVENT_MOVE,
		DRAGTYPE_EVENT_STARTTIME,
		DRAGTYPE_EVENT_STARTTIME_RESCALE,
		DRAGTYPE_EVENT_ENDTIME,
		DRAGTYPE_EVENT_ENDTIME_RESCALE,
		DRAGTYPE_EVENTTAG_MOVE,
		DRAGTYPE_EVENTABSTAG_MOVE,
		DRAGTYPE_SCRUBBER,
		DRAGTYPE_SCENE_ENDTIME,
		DRAGTYPE_RESCALELEFT,
		DRAGTYPE_RESCALERIGHT,
	};

	float				m_flScrub;
	float				m_flScrubTarget;

	bool				m_bDragging;
	int					m_xStart;
	int					m_yStart;

	bool				m_bUseBounds;
	int					m_nMinX;
	int					m_nMaxX;

	struct CFocusRect
	{
		RECT	m_rcOrig;
		RECT	m_rcFocus;
	};
	CUtlVector < CFocusRect >	m_FocusRects;

	int					m_nDragType;
	HCURSOR				m_hPrevCursor;

	struct FLYOVER
	{
		CChoreoActorWidget			*a;
		CChoreoChannelWidget		*c;
		CChoreoEventWidget			*e;
		CChoreoGlobalEventWidget	*ge;
		int							tag;
		CEventAbsoluteTag			*at;
		int							ccbutton;
	};

	FLYOVER				m_Flyover;

	bool				m_bCanDraw;

	struct CVUndo
	{
		CChoreoScene *undo;
		CChoreoScene *redo;
		char		 *udescription;
		char		 *rdescription;
	};

	CUtlVector< CVUndo * >	m_UndoStack;
	int					m_nUndoLevel;
	bool				m_bRedoPending;

	bool				m_bProcessSequences;

	float				m_flLastMouseClickTime;

	bool				m_bSuppressLayout;

	bool				m_bRampOnly;
	float				m_flScrubberTimeOffset;

	bool						m_bShowCloseCaptionData;

	bool				m_bForceProcess;

	// cached version of the local directory when a scene is loaded
	CUtlVector< CUtlString > m_nextFileList;
};

extern CChoreoView		*g_pChoreoView;

template< class T >
void CChoreoView::SetPreservedTimeZoom( T *other, int tz )
{
	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( (HWND)other->getHandle(), &pt );

	// Now figure out time under cursor at old zoom scale
	float t = other->GetTimeValueForMouse( pt.x, true );

	// Call CChoreoView's version
	SetTimeZoom( other->GetToolName(), tz, false );

	// Now figure out tie under pt.x
	float newT = other->GetTimeValueForMouse( pt.x, true );
	if ( newT != t )
	{
		// We need to scroll over a bit
		float pps = other->GetPixelsPerSecond();
		float movePixels = pps * ( newT - t );

		float newOffset = other->m_flLeftOffset - movePixels;
		if ( newOffset < 0.0f )
		{
			newOffset = 0;
		}

		float ed = other->GetEventEndTime();
		float flLastPixel = ed * pps;
		if ( newOffset + other->w2() > flLastPixel )
		{
			newOffset = flLastPixel - other->w2();
		}

		other->m_flLeftOffset = newOffset;
		other->m_pHorzScrollBar->setValue( (int)( other->m_flLeftOffset ) );
	}

	other->RepositionHSlider();
}

template< class T >
int CChoreoView::HandleZoomKey( T *other, int keyCode )
{
	int iret = 1;
	switch ( keyCode )
	{
	default:
		{
			iret = 0;
		}
		break;

	case VK_HOME:
		{
			other->MoveTimeSliderToPos( 0 );
		}
		break;
	case VK_END:
		{
			float maxtime = other->GetEventEndTime();
			int pixels = max( 0, (int)( maxtime * other->GetPixelsPerSecond() ) - other->w2() );
			other->MoveTimeSliderToPos( pixels );
		}
		break;
	case VK_PRIOR:  // PgUp
		{
			int window = other->w2();
			other->m_flLeftOffset = max( other->m_flLeftOffset - (float)window, 0.0f );
			other->MoveTimeSliderToPos( (int)other->m_flLeftOffset );
		}
		break;
	case VK_NEXT:   // PgDown
		{
			int window = other->w2();
			float maxtime = other->GetEventEndTime();
			int pixels = max( 0, (int)( maxtime * other->GetPixelsPerSecond() ) - other->w2() );
			other->m_flLeftOffset = min( other->m_flLeftOffset + (float)window, (float)pixels );
			other->MoveTimeSliderToPos( (int)other->m_flLeftOffset );
		}
		break;
	}

	return iret;
}


float SnapTime( float input, float granularity );

class StudioModel;
StudioModel *FindAssociatedModel( CChoreoScene *scene, CChoreoActor *a );

#endif // CHOREOVIEW_H
