//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EXPRESSIONTOOL_H
#define EXPRESSIONTOOL_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "studio.h"
#include "utlvector.h"
#include "tier1/utldict.h"
#include "faceposertoolwindow.h"

class CChoreoEvent;
class TimelineItem;
class CFlexAnimationTrack;
class CExpClass;
class CChoreoWidgetDrawHelper;
class CExpressionToolWorkspace;
class CChoreoView;
class CFlexTimingTag;
class CExpression;
class mxSlider;

#define IDC_EXPRESSIONTOOLVSCROLL	1000
#define IDC_ADDTRACKS				1001
#define IDC_COLLAPSEALL				1002
#define IDC_EXPANDALL				1003
#define IDC_EXPANDVALID				1004
#define IDC_INSERT_TIMING_TAG		1005
#define IDC_DELETE_TIMING_TAG		1006
#define IDC_LOCK_TIMING_TAG			1007
#define IDC_UNLOCK_TIMING_TAG		1008

#define IDC_COPY_TO_FLEX			1009
#define IDC_COPY_FROM_FLEX			1010

#define	IDC_NEW_EXPRESSION_FROM_FLEXANIMATION 1011

#define IDC_EXPORT_FA				1012
#define IDC_IMPORT_FA				1013

#define IDC_REDO_FA					1014
#define IDC_UNDO_FA					1015

#define IDC_TL_COPY					1016
#define IDC_TL_PASTE				1017
#define IDC_TL_DELETE				1018
#define IDC_TL_DESELECT				1019
#define IDC_TL_SELECTALL			1020

#define IDC_TL_COLLAPSE				1021
#define IDC_TL_EXPAND				1022
#define IDC_TL_ENABLE				1023
#define IDC_TL_DISABLE				1024

#define IDC_TL_EDITNORMAL			1025
#define IDC_TL_EDITLEFTRIGHT		1026

#define IDC_COLLAPSE_ALL_EXCEPT		1027
#define IDC_DISABLE_ALL_EXCEPT		1028
#define IDC_ENABLE_ALL_VALID		1029

#define IDC_TL_SNAPSELECTED			1030
#define IDC_TL_SNAPPOINTS			1031
#define IDC_TL_DELETECOLUMN			1032
#define IDC_TL_SNAPALL				1033

#define IDC_FLEX_CHANGESCALE		1034
#define	IDC_FLEXHSCROLL				1035

#define IDC_ET_SORT_BY_USED			1036
#define IDC_ET_SORT_BY_NAME			1037

#define IDC_ET_SELECTION_DELETE		1038
#define IDC_ET_SELECTION_EXCISE		1039

#define IDC_ET_RESET_ITEM_SIZE		1040
#define IDC_ET_RESET_ALL_ITEM_SIZES	1041

#define IDC_FLEX_SCALESAMPLES		1042

#define IDC_TL_KB_TENSION			1050
#define IDC_TL_KB_BIAS				1051
#define IDC_TL_KB_CONTINUITY		1052

#define IDC_ET_EDGEPROPERTIES		1053
#define IDC_ET_SELECTION_COPY		1054
#define IDC_ET_SELECTION_PASTE		1055

#include "ExpressionSample.h"

class ExpressionTool : public mxWindow, public IFacePoserToolWindow
{
public:
	// Construction
						ExpressionTool( mxWindow *parent );
						~ExpressionTool( void );

	virtual void		Think( float dt );
	void				ScrubThink( float dt, bool scrubbing );
	virtual bool		IsScrubbing( void ) const;
	virtual bool		IsProcessing( void );


	virtual int			handleEvent( mxEvent *event );
	virtual void		redraw( void );
	virtual bool		PaintBackground();

	bool				SetFlexAnimationTrackFromExpression( int mx, int my, CExpClass *cl, CExpression *exp );

	void				SetEvent( CChoreoEvent *event );

	bool				HasCopyData( void );

	void				Copy( CFlexAnimationTrack *source );
	void				Paste( CFlexAnimationTrack *destination );

	void				GetScrubHandleRect( RECT& rcHandle, bool clipped = false );
	void				DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle );
	void				DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper );

	CChoreoEvent		*GetSafeEvent( void );
	
	void				ExpandAll( void );
	void				ExpandValid( void );

	void				LayoutItems( bool force = false );

	void				OnCopyToFlex( bool isEdited );
	void				OnCopyFromFlex( bool isEdited );

	void				OnCopyToFlex( float scenetime, bool isEdited );
	void				OnCopyFromFlex( float scenetime, bool isEdited );
	void				OnSetSingleKeyFromFlex( char const *sliderName );

	void				OnNewExpression( void );
	void				ShowContextMenu( mxEvent *event, bool include_track_menus );

	void				ForceScrubPosition( float newtime );
	void				ForceScrubPositionFromSceneTime( float scenetime );

	void				SetScrubTime( float t );
	void				SetScrubTargetTime( float t );

	void				DrawScrubHandles();

	void				SetClickedPos( int x, int y );
	float				GetTimeForClickedPos( void );

	void				SetMouseOverPos( int x, int y );
	void				GetMouseOverPos( int &x, int& y );
	void				GetMouseOverPosRect( RECT& rcPos );
	void				DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos );
	void				DrawMouseOverPos();


	void				MoveSelectedSamples( float dfdx, float dfdy, bool snap );
	void				DeleteSelectedSamples( void );
	int					CountSelectedSamples( void );
	void				DeselectAll( void );

	void				RepositionHSlider( void );

	bool				IsFocusItem( TimelineItem *item );
	virtual void		OnModelChanged();

	float				GetScrub() const { return m_flScrub; }
	float				GetScrubberSceneTime();

	void				GetTimelineItems( CUtlVector< TimelineItem * >& list );
	void				InvalidateLayout( void );

private:
	void				DoTrackLookup( CChoreoEvent *event );

	void				AddFlexTimingTag( int mx );
	void				DeleteFlexTimingTag( int mx, int my );

	void				OnSortByUsed( void );
	void				OnSortByName( void );

	void				OnDeleteSelection( bool excise_time );
	void				OnResetItemSize();
	void				OnResetAllItemSizes();
	void				ResampleControlPoints( CFlexTimingTag *tag, float newposition );

	void				OnScaleSamples();

	void				LockTimingTag( void );
	void				UnlockTimingTag( void );

	bool				GetTimingTagRect( RECT& rcClient, CChoreoEvent *event, CFlexTimingTag *tag, RECT& rcTag );

//	float				MouseToFrac( int mx );
//float				MouseToTime( int mx );
//	int					TimeToMouse( float t );

	void				GetWorkspaceLeftRight( int& left, int& right );

	bool				IsMouseOverScrubHandle( mxEvent *event );
	CFlexTimingTag		*IsMouseOverTag( int mx, int my );

	void				DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper );

	void				DrawFocusRect( void );

	void				ApplyBounds( int& mx, int& my );
	void				CalcBounds( int movetype );

	void				OnExportFlexAnimation( void );
	void				OnImportFlexAnimation( void );

	void				OnUndo( void );
	void				OnRedo( void );

	void				StartDragging( int dragtype, int startx, int starty, HCURSOR cursor );
	void				GetWorkspaceRect( RECT &rc );
	void				AddFocusRect( RECT& rc );
	void				OnMouseMove( mxEvent *event );

	// Mouse control over selected samples
	void				SelectPoints( float starttime, float endtime );
	void				FinishSelect( int startx, int mx );
	void				FinishMoveSelection( int startx, int mx );
	void				FinishMoveSelectionStart( int startx, int mx );
	void				FinishMoveSelectionEnd( int startx, int mx );

	// In general over the point area tray
	bool				IsMouseOverPoints( int mx, int my );
	// Specifically over selected points
	bool				IsMouseOverSelection( int mx, int my );
	bool				IsMouseOverSelectionStartEdge( mxEvent *event );
	bool				IsMouseOverSelectionEndEdge( mxEvent *event );

	// Readjust slider
	void				MoveTimeSliderToPos( int x );
	void				OnChangeScale();
	int					ComputeHPixelsNeeded( void );
	float				GetTimeValueForMouse( int mx, bool clip = false );

	void				OnEdgeProperties();

public:
	int					GetPixelForTimeValue( float time, bool *clipped = NULL );
	float				GetPixelsPerSecond( void );
	void				GetStartAndEndTime( float& st, float& ed );
	float				GetEventEndTime();

private:

	class CColumnCopier
	{
	public:
		class CTrackData
		{
		public:
			CTrackData() {};
			CTrackData( const CTrackData& other )
			{
				m_Samples[ 0 ].CopyArray( other.m_Samples[ 0 ].Base(), other.m_Samples[ 0 ].Count() );
				m_Samples[ 1 ].CopyArray( other.m_Samples[ 1 ].Base(), other.m_Samples[ 1 ].Count() );
			}
			CUtlVector< CExpressionSample > m_Samples[ 2 ];
		};

		bool					m_bActive;
		float					m_flCopyTimes[ 2 ];
		CUtlDict< CTrackData, int >	m_Data;

		CColumnCopier() : m_bActive( false )
		{
			m_flCopyTimes[ 0 ] = m_flCopyTimes[ 1 ] = 0.0f;
		}

		void Reset()
		{
			m_bActive = false;
			m_flCopyTimes[ 0 ] = m_flCopyTimes[ 1 ] = 0.0f;
			m_Data.Purge();
		}
	};

	bool				HasCopiedColumn();
	void				OnCopyColumn();
	void				OnPasteColumn();
	void				ClearColumnCopy();

	CColumnCopier		m_ColumnCopy;

	int					m_nFocusEventGlobalID;

	float				m_flScrub;
	float				m_flScrubTarget;

	enum
	{
		DRAGTYPE_NONE = 0,
		DRAGTYPE_SCRUBBER,
		DRAGTYPE_FLEXTIMINGTAG,

		DRAGTYPE_SELECTSAMPLES,
		DRAGTYPE_MOVESELECTION,
		DRAGTYPE_MOVESELECTIONSTART,
		DRAGTYPE_MOVESELECTIONEND,
	};

	HCURSOR				m_hPrevCursor;
	int					m_nDragType;

	int					m_nStartX;
	int					m_nStartY;
	int					m_nLastX;
	int					m_nLastY;

	int					m_nClickedX;
	int					m_nClickedY;

	bool				m_bUseBounds;
	int					m_nMinX;
	int					m_nMaxX;
	
	struct CFocusRect
	{
		RECT	m_rcOrig;
		RECT	m_rcFocus;
	};
	CUtlVector < CFocusRect >	m_FocusRects;

	CUtlVector< CExpressionSample > m_CopyData[2];

	CExpressionToolWorkspace *m_pWorkspace;

	CChoreoEvent				*m_pLastEvent;

	int					m_nMousePos[ 2 ];

	float				m_flSelection[ 2 ];
	bool				m_bSelectionActive;

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

extern ExpressionTool	*g_pExpressionTool;

#endif // EXPRESSIONTOOL_H
