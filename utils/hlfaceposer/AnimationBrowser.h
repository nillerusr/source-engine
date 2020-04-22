//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( ANIMATIONBROWSER_H )
#define ANIMATIONBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#define IDC_AB_TRAYSCROLL					1001
#define IDC_AB_THUMBNAIL_INCREASE			1002
#define IDC_AB_THUMBNAIL_DECREASE			1003
#define IDC_AB_CONTEXT_CREATEBITMAP			1004
#define IDC_AB_CONTEXT_CREATEALLBITMAPS		1005
#define IDC_AB_FILTERTAB					1006

#define IDC_AB_CREATE_CUSTOM				1007

#define IDC_AB_ADDTOGROUPSTART				1100
#define IDC_AB_ADDTOGROUPEND				1199

#define IDC_AB_REMOVEFROMGROUPSTART			1200
#define IDC_AB_REMOVEFROMGROUPEND			1299

#define IDC_AB_DELETEGROUPSTART				1300
#define IDC_AB_DELETEGROUPEND				1399

#define IDC_AB_RENAMEGROUPSTART				1400
#define IDC_AB_RENAMEGROUPEND				1499

#define COLOR_TRAYBACKGROUND		RGB( 240, 240, 220 )

#include "faceposertoolwindow.h"
#include "StudioModel.h"

class CAnimBrowserTab;

class CCustomAnim
{
public:
	CCustomAnim( const FileNameHandle_t &h ) 
		:
		m_bDirty( false ),
		m_ShortName( UTL_INVAL_SYMBOL )
	{
		m_Handle = h;
	}

	void		LoadFromFile();
	void		SaveToFile();

	bool		HasAnimation( char const *search );

	bool						m_bDirty;
	CUtlSymbol					m_ShortName;
	FileNameHandle_t			m_Handle;
	CUtlVector< CUtlSymbol >	m_Animations;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class AnimationBrowser : public mxWindow, public IFacePoserToolWindow
{
public:
	enum
	{
		FILTER_NONE = 0,
		FILTER_GESTURES,
		FILTER_POSTURES,
		FILTER_STRING,
		FILTER_FIRST_CUSTOM
	};

						AnimationBrowser( mxWindow *parent, int id = 0 );
	virtual				~AnimationBrowser ( void );

	virtual void		Shutdown();

	virtual void		redraw ();
	virtual bool		PaintBackground( void );

	virtual int			handleEvent (mxEvent *event);

	virtual void		Think( float dt );
	
	void				ThumbnailIncrease( void );
	void				ThumbnailDecrease( void );
	void				RestoreThumbnailSize( void );

	void				Select( int sequence );
	void				Deselect( void );

	void				SetCellSize( int cellsize );

	void				ReloadBitmaps( void );
	virtual void		OnModelChanged();

	void				OnAddCustomAnimationFilter();

private: // Methods

	void				OnFilter();
	bool				SequencePassesFilter( StudioModel *model, int sequence, mstudioseqdesc_t &seqdesc );

	int					GetSequenceCount();
	mstudioseqdesc_t	*GetSeqDesc( int index );
	int					TranslateSequenceNumber( int index );

	int					GetCellUnderPosition( int x, int y );

	bool				ComputeRect( int cell, int& rcx, int& rcy, int& rcw, int& rch );
	int					ComputePixelsNeeded( void );

	void				RepositionSlider();
	void				SetClickedCell( int cell );
	void				ShowRightClickMenu( int mx, int my );

	void				DrawThumbNail( int sequence, CChoreoWidgetDrawHelper& helper, 
							int rcx, int rcy, int rcw, int rch );

	void				DrawSequenceFocusRect( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, COLORREF clr );
	void				DrawSequenceDescription( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, int sequence, mstudioseqdesc_t &seqdesc );

	void				DrawFocusRect( void );

	// Custom group tab stuff
	void				FindCustomFiles( char const *subdir, CUtlVector< FileNameHandle_t >& files );
	void				AddCustomFile( const FileNameHandle_t& handle );
	void				RenameCustomFile( int index );
	void				DeleteCustomFile( int index );
	void				PurgeCustom();
	void				BuildCustomFromFiles( CUtlVector< FileNameHandle_t >& files );
	void				UpdateCustomTabs();
	int					FindCustomFile( char const *shortName );
	void				AddAnimationToCustomFile( int index, char const *animationName );
	void				RemoveAnimationFromCustomFile( int index, char const *animationName );
	void				RemoveAllAnimationsFromCustomFile( int index );

private: // Data

	mxScrollbar			*slScrollbar;
	CAnimBrowserTab		*m_pFilterTab;
	mxLineEdit			*m_pSearchEntry;

	int					m_nTopOffset;

	int					m_nLastNumAnimations;

	int					m_nGranularity;

	int					m_nCurCell;
	int					m_nClickedCell;

	// Formatting
	int					m_nButtonSquare;

	int					m_nGap;
	int					m_nDescriptionHeight;
	int					m_nSnapshotWidth;
	int					m_nSnapshotHeight;

	bool				m_bDragging;
	RECT				m_rcFocus;
	RECT				m_rcOrig;
	int					m_nDragCell;
	int					m_nXStart;
	int					m_nYStart;

	mxButton			*m_pThumbnailIncreaseButton;
	mxButton			*m_pThumbnailDecreaseButton;

	CUtlVector< int >	m_Filtered;
	int					m_nCurFilter;
	char				m_szSearchString[ 256 ];

	float				m_flDragTime;

	CUtlVector< CCustomAnim * >		m_CustomAnimationTabs;
};

extern AnimationBrowser *g_pAnimationBrowserTool;

#endif // ANIMATIONBROWSER_H