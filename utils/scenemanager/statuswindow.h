//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef STATUSWINDOW_H
#define STATUSWINDOW_H
#ifdef _WIN32
#pragma once
#endif

class mxScrollbar;

#define IDC_STATUS_SCROLL 1000

class CStatusWindow : public mxWindow
{
public:
	CStatusWindow (mxWindow *parent, int x, int y, int w, int h, const char *label = 0 );
	~CStatusWindow();

	void StatusPrint( int r, int g, int b, bool overwrite, const char *text );

	virtual void redraw();
	virtual bool PaintBackground( void );

	virtual int handleEvent( mxEvent *event );

//	virtual void Think( float dt );

private:

	void PositionSliders( int sboffset );

	enum
	{
		MAX_TEXT_LINES = 1024,
		TEXT_LINE_MASK = MAX_TEXT_LINES - 1,
	};

	struct TextLine
	{
		char		m_szText[ 512 ];
		int			r, g, b;
		float		curtime;
	};

	TextLine		m_rgTextLines[ MAX_TEXT_LINES ];
	int				m_nCurrentLine;

	mxScrollbar		*m_pScrollbar;
};

extern CStatusWindow *g_pStatusWindow;

#endif // STATUSWINDOW_H
