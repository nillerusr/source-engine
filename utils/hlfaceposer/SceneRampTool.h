//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SCENERAMPTOOL_H
#define SCENERAMPTOOL_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "studio.h"
#include "utlvector.h"
#include "faceposertoolwindow.h"

class CChoreoEvent;
class CChoreoScene;
class CChoreoWidgetDrawHelper;
class CChoreoView;
struct CExpressionSample;

#define IDC_REDO_SRT					1000
#define IDC_UNDO_SRT					1001

#define IDC_SRT_DELETE				1002
#define IDC_SRT_DESELECT				1003
#define IDC_SRT_SELECTALL			1004

#define IDC_SRT_CHANGESCALE			1005
#define IDC_SRT_RAMPHSCROLL			1006

#define IDC_SRT_EDGEPROPERTIES		1007

#define FP_SRT_SELECTION_TOLERANCE					30.0f
#define FP_SRT_SELECTION_RECTANGLE_TOLERANCE		5.0f
#define FP_SRT_ADDSAMPLE_TOLERANCE					5.0f

template< class T > class CCurveEditorHelper;

class SceneRampTool : public mxWindow, public IFacePoserToolWindow
{
public:
	// Construction
						SceneRampTool( mxWindow *parent );
						~SceneRampTool( void );

	virtual void		Think( float dt );
	void				ScrubThink( float dt, bool scrubbing );
	virtual bool		IsScrubbing( void ) const;
	virtual bool		IsProcessing( void );

	virtual int			handleEvent( mxEvent *event );
	virtual void		redraw( void );
	virtual bool		PaintBackground();

	void				GetScrubHandleRect( RECT& rcHandle, float scrub, bool clipped = false );

	void				DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle, float scrub, bool reference );
	void				DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right );
	void				DrawSceneEnd( CChoreoWidgetDrawHelper& drawHelper );

	void				SetMouseOverPos( int x, int y );
	void				GetMouseOverPos( int &x, int& y );
	void				GetMouseOverPosRect( RECT& rcPos );
	void				DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos );
	void				DrawMouseOverPos();

	void				DrawScrubHandles();

	CChoreoScene		*GetSafeScene( void );

	bool				IsMouseOverScrubHandle( mxEvent *event );
	void				ForceScrubPosition( float newtime );
	void				ForceScrubPositionFromSceneTime( float scenetime );

	void				SetScrubTime( float t );
	void				SetScrubTargetTime( float t );

	void				DrawSamplesSimple( CChoreoWidgetDrawHelper& drawHelper, CChoreoScene *scene, bool clearbackground, COLORREF sampleColor, RECT &rcSamples );

	virtual void		OnModelChanged();

	void				SetMousePositionForEvent( mxEvent *event );

	int					NumSamples();
	CExpressionSample	*GetSample( int idx );
	void				PreDataChanged( char const *undodescription );
	void				PostDataChanged( char const *redodescription );
	CExpressionSample	*GetSampleUnderMouse( int mx, int my, float tolerance = FP_SRT_SELECTION_TOLERANCE );
	void				GetWorkList( bool reflect, CUtlVector< SceneRampTool * >& list );

private:

	void				GetSampleTrayRect( RECT& rc );
	void				DrawSamples( CChoreoWidgetDrawHelper& drawHelper, RECT &rcSamples );

	void				SelectPoints( void );
	void				DeselectAll();
	void				SelectAll();
	void				Delete( void );

	int					CountSelected( void );
	void				MoveSelectedSamples( float dfdx, float dfdy );

	void				StartDragging( int dragtype, int startx, int starty, HCURSOR cursor );
	void				AddFocusRect( RECT& rc );
	void				OnMouseMove( mxEvent *event );
	void				DrawFocusRect( void );
	void				ShowContextMenu( mxEvent *event, bool include_track_menus );
	void				GetWorkspaceLeftRight( int& left, int& right );
	void				SetClickedPos( int x, int y );
	float				GetTimeForClickedPos( void );
	
	void				DrawAutoHighlight( mxEvent *event );

	void				ApplyBounds( int& mx, int& my );
	void				CalcBounds( int movetype );
	void				OnUndo( void );
	void				OnRedo( void );

	void				OnRevert( void );

	void				OnEdgeProperties();

	void				DrawTimingTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc );
	void				DrawRelativeTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, CChoreoEvent *event, float starttime, float endtime );
	void				DrawAbsoluteTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT &rc, CChoreoEvent *event, float starttime, float endtime );


	// Readjust slider
	void				MoveTimeSliderToPos( int x );
	void				OnChangeScale();
	int					ComputeHPixelsNeeded( void );
	float				GetPixelsPerSecond( void );
	void				InvalidateLayout( void );
	void				RepositionHSlider( void );
	void				GetStartAndEndTime( float& st, float& ed );
	float				GetEventEndTime();
	float				GetTimeValueForMouse( int mx, bool clip = false );
	int					GetPixelForTimeValue( float time, bool *clipped = NULL );

	float				m_flScrub;
	float				m_flScrubTarget;

	enum
	{
		DRAGTYPE_NONE = 0,
		DRAGTYPE_SCRUBBER,
		DRAGTYPE_MOVEPOINTS_VALUE,
		DRAGTYPE_MOVEPOINTS_TIME,
		DRAGTYPE_SELECTION,
	};

	int					m_nMousePos[ 2 ];

	bool				m_bUseBounds;
	int					m_nMinX;
	int					m_nMaxX;

	HCURSOR				m_hPrevCursor;
	int					m_nDragType;

	int					m_nStartX;
	int					m_nStartY;
	int					m_nLastX;
	int					m_nLastY;

	int					m_nClickedX;
	int					m_nClickedY;

	struct CFocusRect
	{
		RECT	m_rcOrig;
		RECT	m_rcFocus;
	};
	CUtlVector < CFocusRect >	m_FocusRects;

	bool				m_bSuppressLayout;
	// Height/width of scroll bars
	int					m_nScrollbarHeight;
	float				m_flLeftOffset;
	mxScrollbar			*m_pHorzScrollBar;
	int					m_nLastHPixelsNeeded;
	// How many pixels per second we are showing in the UI
	float				m_flPixelsPerSecond;
	// Do we need to move controls?
	bool				m_bLayoutIsValid;
	float				m_flLastDuration;
	float				m_flScrubberTimeOffset;
	int					m_nUndoSetup;

	CCurveEditorHelper< SceneRampTool >	*m_pHelper;

	friend class		CChoreoView;
};

extern SceneRampTool	*g_pSceneRampTool;

#endif // SCENERAMPTOOL_H
