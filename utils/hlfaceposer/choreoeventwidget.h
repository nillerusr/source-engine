//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOEVENTWIDGET_H
#define CHOREOEVENTWIDGET_H
#ifdef _WIN32
#pragma once
#endif

#include "mxbitmaptools.h"
#include "choreowidget.h"

class CChoreoEvent;
class CChoreoChannelWidget;
class CAudioSource;

#define FP_NUM_BITMAPS 20

//-----------------------------------------------------------------------------
// Purpose: Draw event UI element and handle mouse interactions
//-----------------------------------------------------------------------------
class CChoreoEventWidget : public CChoreoWidget
{
public:
	typedef CChoreoWidget		BaseClass;

	// Construction/destruction
								CChoreoEventWidget( CChoreoWidget *parent );
	virtual						~CChoreoEventWidget( void );

	// Create children
	virtual void				Create( void );
	// Redo layout
	virtual void				Layout( RECT& rc );
	// Screen refresh
	virtual void				redraw(CChoreoWidgetDrawHelper& drawHelper);
	virtual void				redrawStatus( CChoreoWidgetDrawHelper& drawHelper, RECT& rcClient );

	virtual int					GetDurationRightEdge( void );

	// Access underlying object
	CChoreoEvent				*GetEvent( void );
	void						SetEvent( CChoreoEvent *event );

	// If the user changes the association of .mdls to actors, then the gender could change and we could need to access a different .wav file
	// Call this to reconcile things
	void						RecomputeWave();

	// System wide icons for various event types ( indexed by CChoreEvent::m_fType )
	static void					LoadImages( void );
	static void					DestroyImages( void );
	static mxbitmapdata_t		*GetImage( int type );
	static mxbitmapdata_t		*GetPauseImage( void );
	static mxbitmapdata_t		*GetLockImage( void );

private:

	COLORREF					GrayOutColor( COLORREF clr );

	void						DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper, RECT &rcWAV, float length, CChoreoEvent *event );
	void						DrawAbsoluteTags( CChoreoWidgetDrawHelper& drawHelper, RECT &rcWAV, float length, CChoreoEvent *event );

	const char					*GetLabelText( void );

	void						DrawSpeakEvent(  CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventLine );
	void						DrawGestureEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventLine );
	void						DrawGenericEvent( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventLine );
	// Parent widget
	CChoreoWidget		*m_pParent;

	// Underlying event
	CChoreoEvent		*m_pEvent;

	int					m_nDurationRightEdge;

	// For speak events
	CAudioSource		*m_pWaveFile;

	// Bitmaps for drawing event widgets
	static mxbitmapdata_t m_Bitmaps[ FP_NUM_BITMAPS ];
	static mxbitmapdata_t m_ResumeConditionBitmap;
	static mxbitmapdata_t m_LockBodyFacingBitmap;
};

#endif // CHOREOEVENTWIDGET_H
