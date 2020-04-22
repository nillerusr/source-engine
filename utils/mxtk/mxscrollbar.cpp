//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxScrollbar.cpp
// implementation: Win32 API
// last modified:  Mar 18 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxScrollbar.h"
#include <windows.h>
#include <commctrl.h>



class mxScrollbar_i
{
public:
	int dummy;
};



mxScrollbar::mxScrollbar (mxWindow *parent, int x, int y, int w, int h, int id, int style)
: mxWidget (parent, x, y, w, h)
{
	if (!parent)
		return;

	DWORD dwStyle = WS_CHILD | WS_VISIBLE;
	HWND hwndParent = (HWND) ((mxWidget *) parent)->getHandle ();

	if (style == Horizontal)
		dwStyle = WS_CHILD | WS_VISIBLE | SBS_HORZ | SBS_RIGHTALIGN;
	else if (style == Vertical)
		dwStyle = WS_CHILD | WS_VISIBLE | SBS_VERT | SBS_RIGHTALIGN; // WS_VSCROLL;

	void *handle = (void *) CreateWindowEx (0, "SCROLLBAR", "", dwStyle,
				x, y, w, h, hwndParent,
				(HMENU) id, (HINSTANCE) GetModuleHandle (NULL), NULL);
	
	SendMessage ((HWND) handle, WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_VAR_FONT), MAKELPARAM (TRUE, 0));
	SetWindowLong ((HWND) handle, GWL_USERDATA, (LONG) this);

	setHandle (handle);
	setType (MX_SCROLLBAR);
	setParent (parent);
	setId (id);
}



mxScrollbar::~mxScrollbar ()
{
}



void
mxScrollbar::setValue (int ivalue)
{
	SetScrollPos( (HWND) getHandle (), SB_CTL, ivalue, FALSE );
}
	


void
mxScrollbar::setRange (int min, int max )
{
	SCROLLINFO si = { sizeof( SCROLLINFO ), SIF_RANGE, min, max, 0, 0, 0 };

	SetScrollInfo( (HWND) getHandle (), SB_CTL, &si, TRUE );
}



void
mxScrollbar::setPagesize (int size)
{
	SCROLLINFO si = { sizeof( SCROLLINFO ), SIF_PAGE, 0, 0, (UINT)size, 0, 0 };

	SetScrollInfo( (HWND) getHandle (), SB_CTL, &si, TRUE );
}



int
mxScrollbar::getValue () const
{
	// SCROLLINFO si = { sizeof( SCROLLINFO ), SIF_POS | SIF_TRACKPOS, 0, 0, 0, 0, 0 };
	// GetScrollInfo( (HWND) getHandle (), SB_CTL, &si );
	// return si.nPos;
	return GetScrollPos( (HWND) getHandle (), SB_CTL );
}



int
mxScrollbar::getMinValue () const
{
	SCROLLINFO si = { sizeof( SCROLLINFO ), SIF_RANGE, 0, 0, 0, 0, 0 };

	GetScrollInfo( (HWND) getHandle (), SB_CTL, &si );

	return si.nMin;
}



int
mxScrollbar::getMaxValue () const
{
	SCROLLINFO si = { sizeof( SCROLLINFO ), SIF_RANGE, 0, 0, 0, 0, 0 };

	GetScrollInfo( (HWND) getHandle (), SB_CTL, &si );

	return si.nMax;
}



int
mxScrollbar::getPagesize () const
{
	SCROLLINFO si = { sizeof( SCROLLINFO ), SIF_PAGE, 0, 0, 0, 0, 0 };

	GetScrollInfo( (HWND) getHandle (), SB_CTL, &si );

	return si.nPage;
}
