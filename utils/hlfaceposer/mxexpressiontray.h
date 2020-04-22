//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( MXEXPRESSIONTRAY_H )
#define MXEXPRESSIONTRAY_H
#ifdef _WIN32
#pragma once
#endif

#define IDC_TRAYSCROLL				1001
#define IDC_CONTEXT_NEWEXP			1002
#define IDC_CONTEXT_EDITEXP			1003
#define IDC_CONTEXT_SAVEEXP			1004
#define IDC_CONTEXT_DELETEXP		1005
#define IDC_CONTEXT_REVERT			1012
#define IDC_AB						1014
#define IDC_THUMBNAIL_INCREASE		1015
#define IDC_THUMBNAIL_DECREASE		1016
#define IDC_CONTEXT_CREATEBITMAP	1017

#define COLOR_TRAYBACKGROUND		RGB( 240, 240, 220 )

class ControlPanel;
class FlexPanel;
class mxScrollbar;
class mxCheckBox;
class CChoreoView;
class CExpression;
class CExpClass;
class mxButton;
class CChoreoWidgetDrawHelper;

#include "faceposertoolwindow.h"
#include "mxbitmaptools.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class mxExpressionTray : public mxWindow, public IFacePoserToolWindow
{
public:
						mxExpressionTray( mxWindow *parent, int id = 0 );
	virtual				~mxExpressionTray ( void );

	virtual void		redraw ();
	virtual bool		PaintBackground( void );

	virtual int			handleEvent (mxEvent *event);

	void				ThumbnailIncrease( void );
	void				ThumbnailDecrease( void );
	void				RestoreThumbnailSize( void );

	void				AB( void );

	void				Select( int exp, bool deselect = true );
	void				Deselect( void );
	int					CountSelected( void );

	void				SetCellSize( int cellsize );

	void				ReloadBitmaps( void );
	virtual void		OnModelChanged();

private: // Data structures

	typedef void (mxExpressionTray::*ETMEMBERFUNC)( int cell );

	class mxETButton
	{
	public:
		mxETButton					*next;
		char						m_szName[ 32 ];
		bool						m_bActive;
		RECT						m_rc;
		char						m_szToolTip[ 128 ];
		mxbitmapdata_t				*m_pImage;

		ETMEMBERFUNC				m_fnCallback;
	};

private: // Methods
	void				ChangeWeightOfExpressionInGroup( CExpClass *active, CExpression *exp, CExpression *group );
	int					GetCellUnderPosition( int x, int y );

	bool				ComputeRect( int cell, int& rcx, int& rcy, int& rcw, int& rch );
	int					ComputePixelsNeeded( void );

	void				RepositionSlider();
	void				SetClickedCell( int cell );
	void				ShowRightClickMenu( int mx, int my );

	void				DrawThumbNail( CExpClass *active, CExpression *current, CChoreoWidgetDrawHelper& helper, 
							int rcx, int rcy, int rcw, int rch, int c, int selected, bool updateselection );

	void				DrawDirtyFlag( CChoreoWidgetDrawHelper& helper, CExpression *current, int rcx, int rcy, int rcw, int rch );
	void				DrawExpressionFocusRect( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, COLORREF clr );
	void				DrawExpressionDescription( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, const char *expressionname, const char *description );

	void				CreateButtons( void );
	void				DeleteAllButtons( void );
	void				AddButton( const char *name, const char *tooltip, const char *bitmap, 
							ETMEMBERFUNC pfnCallback, bool active, int x, int y, int w, int h );
	mxETButton			*GetItemUnderCursor( int x, int y );
	void				DrawButton( CChoreoWidgetDrawHelper& helper, int cell, mxETButton *btn );
	void				ActivateButton( const char *name, bool active );
	mxETButton			*FindButton( const char *name );
	
	void				ET_Undo( int cell );
	void				ET_Redo( int cell );

	void				DrawFocusRect( void );

private: // Data

	mxETButton			*m_pButtons;

	mxScrollbar			*slScrollbar;

	int					m_nTopOffset;

	int					m_nLastNumExpressions;

	int					m_nGranularity;

	// For A/B
	int					m_nPrevCell;
	int					m_nCurCell;

	// For context menu
	int					m_nClickedCell;

	// Formatting
	int					m_nButtonSquare;

	int					m_nGap;
	int					m_nDescriptionHeight;
	int					m_nSnapshotWidth;
	int					m_nSnapshotHeight;

	// For detecting that the slider thumbs need to be recomputed
	int					m_nPreviousExpressionCount;

	bool				m_bDragging;
	RECT				m_rcFocus;
	RECT				m_rcOrig;
	int					m_nDragCell;
	int					m_nXStart;
	int					m_nYStart;

	mxButton			*m_pABButton;
	mxButton			*m_pThumbnailIncreaseButton;
	mxButton			*m_pThumbnailDecreaseButton;
};

extern mxExpressionTray *g_pExpressionTrayTool;

#endif // MXEXPRESSIONTRAY_H