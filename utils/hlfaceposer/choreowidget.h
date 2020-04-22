//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOWIDGET_H
#define CHOREOWIDGET_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>

class CChoreoView;
class CChoreoScene;
class CChoreoWidgetDrawHelper;

//-----------------------------------------------------------------------------
// Purpose: CChoreoWidgets are mxWindows that we show in the Choreography view area
//  so that we can manipulate them with the mouse.  The widgets follow the scene
//  hierarchy of actors/channels/events, without having to hang off of the underlying
//  data.  They are just for the UI.
//-----------------------------------------------------------------------------
class CChoreoWidget
{
public:
	// memory handling, uses calloc so members are zero'd out on instantiation
    void					*operator new( size_t stAllocateBlock );
	void					operator delete( void *pMem );

							CChoreoWidget( CChoreoWidget *parent );
	virtual					~CChoreoWidget( void );

	// All widgets implement these pure virtuals

	// Called to force a widget to create its children based on the scene data
	virtual void			Create( void ) = 0;
	// Force widget to redo layout of self and any children
	virtual void			Layout( RECT& rc ) = 0;
	// Redraw the widget
	virtual void			redraw( CChoreoWidgetDrawHelper& drawHelper ) = 0;
	// Don't overdraw background
	virtual bool			PaintBackground( void ) { return false; };
	// Determine height to reserver for widget ( Actors can be expanded or collapsed, e.g. )
	virtual int				GetItemHeight( void );

	virtual void			LocalToScreen( int& mx, int& my );

	virtual bool			IsSelected( void );
	virtual void			SetSelected( bool selected );

	virtual void			setBounds( int x, int y, int w, int h );
	virtual int				x( void );
	virtual int				y( void );
	virtual int				w( void );
	virtual int				h( void );
	virtual CChoreoWidget	*getParent( void );
	virtual void			setVisible( bool visible );
	virtual bool			getVisible( void );

	virtual void			getBounds( RECT& bounds );
	virtual RECT			&getBounds( void );

	// Globally accessible scene and view pointers
	static CChoreoScene		*m_pScene;
	static CChoreoView		*m_pView;

private:
	bool					m_bSelected;
	bool					m_bVisible;

	RECT					m_rcBounds;

protected:
	CChoreoWidget			*m_pParent;
};

#endif // CHOREOWIDGET_H
