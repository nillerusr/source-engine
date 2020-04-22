//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef VGUIWND_H
#define VGUIWND_H
#ifdef _WIN32
#pragma once
#endif

#include "mxtk/mx.h"
#include "color.h"

namespace vgui
{
	class EditablePanel;
	typedef unsigned long HCursor;
}

class CVGuiWnd 
{

public:
	CVGuiWnd(void);
	~CVGuiWnd(void);

public:

	void				SetMainPanel( vgui::EditablePanel * pPanel );
	vgui::EditablePanel	*GetMainPanel();	// returns VGUI main panel
	vgui::EditablePanel *CreateDefaultPanel();

	void				SetParentWindow(mxWindow *pParent);
	mxWindow			*GetParentWnd();	// return mxWindow handle

	void				SetCursor(vgui::HCursor cursor);
	void				SetCursor(const char *filename);

	void				SetRepaintInterval( int msecs );
	int					GetVGuiContext();

protected:
	void DrawVGuiPanel();  // overridden to draw this view
	int HandeEventVGui( mxEvent *event );

	vgui::EditablePanel	*m_pMainPanel;
	mxWindow			*m_pParentWnd;
	int					m_hVGuiContext;
	bool				m_bIsDrawing;
	Color				m_ClearColor;
	bool				m_bClearZBuffer;
};

class CVGuiPanelWnd: public mxWindow, public CVGuiWnd
{
	typedef mxWindow BaseClass;

public:

	CVGuiPanelWnd( mxWindow *parent, int x, int y, int w, int h );

	virtual int handleEvent( mxEvent *event ); 
	virtual void redraw();
};


#endif // VGUIWND_H
