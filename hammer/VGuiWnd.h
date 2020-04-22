//========= Copyright Valve Corporation, All rights reserved. ============//
#pragma once
#include "afxwin.h"
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

	void				SetParentWindow(CWnd *pParent);
	CWnd				*GetParentWnd();	// return CWnd handle

	void				SetCursor(vgui::HCursor cursor);
	void				SetCursor(const char *filename);

	void				SetRepaintInterval( int msecs );
	int					GetVGuiContext();
	
protected:
	void DrawVGuiPanel();  // overridden to draw this view
	long WindowProcVGui( UINT message, WPARAM wParam, LPARAM lParam ); //
	
	vgui::EditablePanel	*m_pMainPanel;
	CWnd		*m_pParentWnd;
	int			m_hVGuiContext;
	bool		m_bIsDrawing;
	Color		m_ClearColor;
	bool		m_bClearZBuffer;
};

class CVGuiPanelWnd: public CWnd, public CVGuiWnd
{
protected:
	DECLARE_DYNCREATE(CVGuiPanelWnd)

public:

	// Generated message map functions
	//{{AFX_MSG(CVGuiViewModel)
	//}}AFX_MSG

	virtual LRESULT WindowProc( UINT message, WPARAM wParam, LPARAM lParam );

	DECLARE_MESSAGE_MAP()
};

