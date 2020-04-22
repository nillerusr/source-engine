//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GESTURETOOL_H
#define GESTURETOOL_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "studio.h"
#include "utlvector.h"
#include "faceposertoolwindow.h"

class CChoreoEvent;
class CChoreoWidgetDrawHelper;
class CChoreoView;
class CEventAbsoluteTag;

#define IDC_REDO_GT					1000
#define IDC_UNDO_GT					1001
#define IDC_GT_DELETE_TAG			1002
#define IDC_GT_INSERT_TAG			1003
#define IDC_GT_REVERT				1004

#define IDC_GT_CHANGESCALE			1005
#define IDC_GESTUREHSCROLL			1006

class GestureTool : public mxWindow, public IFacePoserToolWindow
{
public:
	// Construction
						GestureTool( mxWindow *parent );
						~GestureTool( void );

	virtual void		Think( float dt );
	void				ScrubThink( float dt, bool scrubbing );
	virtual bool		IsScrubbing( void ) const;
	virtual bool		IsProcessing( void );

	virtual int			handleEvent( mxEvent *event );
	virtual void		redraw( void );
	virtual bool		PaintBackground();

	void				SetEvent( CChoreoEvent *event );

	void				GetScrubHandleRect( RECT& rcHandle, float scrub, bool clipped = false );
	void				GetScrubHandleReferenceRect( RECT& rcHandle, float scrub, bool clipped = false );

	void				DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle, float scrub, bool reference );
	void				DrawTimeLine( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, float left, float right );
	void				DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper );

	void				DrawAbsoluteTags( CChoreoWidgetDrawHelper& drawHelper );
	bool				GetAbsTagRect( RECT& rcClient, CChoreoEvent *event, int tagtype, CEventAbsoluteTag *tag, RECT& rcTag );
	void				GetTagTrayRect( RECT &rcClient, int tagtype, RECT& rcTray );

	void				SetMouseOverPos( int x, int y );
	void				GetMouseOverPos( int &x, int& y );
	void				GetMouseOverPosRect( RECT& rcPos );
	void				DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos );
	void				DrawMouseOverPos();

	void				DrawScrubHandles();

	CChoreoEvent		*GetSafeEvent( void );

	bool				IsMouseOverScrubHandle( mxEvent *event );
	void				ForceScrubPosition( float newtime );
	void				ForceScrubPositionFromSceneTime( float scenetime );

	void				SetScrubTime( float t );
	void				SetScrubTargetTime( float t );
	virtual void		OnModelChanged();

private:

	void				StartDragging( int dragtype, int startx, int starty, HCURSOR cursor );
	void				AddFocusRect( RECT& rc );
	void				OnMouseMove( mxEvent *event );
	void				DrawFocusRect( void );
	void				ShowContextMenu( mxEvent *event, bool include_track_menus );
	void				GetWorkspaceLeftRight( int& left, int& right );
	void				SetClickedPos( int x, int y );
	float				GetTimeForClickedPos( void );
	
	void				ApplyBounds( int& mx, int& my );
	void				CalcBounds( int movetype );
	void				OnUndo( void );
	void				OnRedo( void );

	CEventAbsoluteTag	*IsMouseOverTag( int mx, int my );
	int					GetTagTypeForMouse( int mx, int my );

	int					GetTagTypeForTag( CEventAbsoluteTag const *tag );
	void				OnInsertTag( void );
	void				OnDeleteTag( void );
	void				OnRevert( void );

	void				DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper, RECT& rc );
	void				DrawRelativeTagsForEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rc, CChoreoEvent *gesture, CChoreoEvent *event, float starttime, float endtime );

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
		DRAGTYPE_ABSOLUTE_TIMING_TAG,
	};

	int					m_nFocusEventGlobalID;

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
	CChoreoEvent				*m_pLastEvent;

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
	bool				m_bInSetEvent;
	float				m_flScrubberTimeOffset;

	friend class		CChoreoView;
};

extern GestureTool	*g_pGestureTool;
#endif // GESTURETOOL_H
