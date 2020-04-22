//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOGLOBALEVENTWIDGET_H
#define CHOREOGLOBALEVENTWIDGET_H
#ifdef _WIN32
#pragma once
#endif

#include "choreowidget.h"

class CChoreoEvent;

//-----------------------------------------------------------------------------
// Purpose: For section start/end
// FIXME: Finish this
//-----------------------------------------------------------------------------
class CChoreoGlobalEventWidget : public CChoreoWidget
{
public:
	typedef CChoreoWidget		BaseClass;

	// Construction/destruction
					CChoreoGlobalEventWidget( CChoreoWidget *parent );
	virtual			~CChoreoGlobalEventWidget( void );

	// Create children
	virtual void	Create( void );
	// Redo layout
	virtual void	Layout( RECT& rc );

	// Screen refresh
	virtual void	redraw(CChoreoWidgetDrawHelper& drawHelper);

	// Access underlying scene object
	CChoreoEvent	*GetEvent( void );
	void			SetEvent( CChoreoEvent *event );

	// Draw focus rect while mouse dragging is going on
	void			DrawFocusRect( void );
private:

	void			DrawLabel( CChoreoWidgetDrawHelper& drawHelper, COLORREF clr, int x, int y, bool right );

	// The underlying scene object
	CChoreoEvent	*m_pEvent;

	// For updating focus rect
	bool				m_bDragging;
	int					m_xStart;
	RECT				m_rcFocus;
	RECT				m_rcOrig;
	HCURSOR				m_hPrevCursor;
};

#endif // CHOREOGLOBALEVENTWIDGET_H
