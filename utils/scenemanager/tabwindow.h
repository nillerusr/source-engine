//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TABWINDOW_H
#define TABWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "utlvector.h"

class CDrawHelper;

//-----------------------------------------------------------------------------
// Purpose: A custom tab control for handling expression class strings
//-----------------------------------------------------------------------------
class CTabWindow : public mxWindow
{
public:
	enum
	{
		COLOR_BG = 0,
		COLOR_FG,
		COLOR_FG_SELECTED,
		COLOR_HILITE,
		COLOR_HILITE_SELECTED,
		COLOR_TEXT,
		COLOR_TEXT_SELECTED,

		NUM_COLORS
	};

						CTabWindow( mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0 );
	virtual				~CTabWindow ( void );

	virtual void		redraw( void );
	virtual int			handleEvent (mxEvent *event);
	
	// MANIPULATORS
	virtual void		add (const char *item);
	virtual void		select (int index);
	virtual void		remove (int index);
	virtual void		removeAll ();
	virtual void		setPrefix( int item, char const *prefix );

	// ACCESSORS
	virtual int			getItemCount () const;
	virtual int			getSelectedIndex () const;

	virtual char const	*getLabel( int item );
	virtual char const	*getPrefix( int item );

	virtual void		ShowRightClickMenu( int mx, int my ) = 0;

	void				SetColor( int index, COLORREF clr );

	void				SetInverted( bool invert );
	void				SetRightJustify( bool rightjustify );

	int					GetBestHeight( int width );
	void				SetRowHeight( int rowheight );

protected:
	void				GetTabRect( const RECT& rcClient, RECT& tabRect, int tabNum );
	virtual void		DrawTab( CDrawHelper& drawHelper, RECT& rcClient, int tabnum, bool selected = false );

	int					RecomputeLayout( int windowWidth, bool dolayout = true );

	class CETItem
	{
	public:
		enum
		{
			MAX_ET_STRING_LENGTH = 64
		};

		char			m_szString[ MAX_ET_STRING_LENGTH ];
		char			m_szPrefix[ MAX_ET_STRING_LENGTH ];
		RECT			rect;
	};

	int						GetItemUnderMouse( int mx, int my );

	CUtlVector <CETItem>	m_Items;
	int						m_nRowsRequired;

	int						m_nSelected;

	int						m_nTabWidth;
	int						m_nPixelDelta;
	bool					m_bInverted;
	bool					m_bRightJustify;

	COLORREF				m_Colors[ NUM_COLORS ];

	int						m_nRowHeight;
};
#endif // TABWINDOW_H
