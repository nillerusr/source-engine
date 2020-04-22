//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxWindow.cpp
// implementation: Win32 API
// last modified:  Apr 12 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxWindow.h"
#include <windows.h>



extern mxWindow *g_mainWindow;



class mxWindow_i
{
public:
	UINT d_uTimer;
};



mxWindow::mxWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: mxWidget (parent, x, y, w, h, label)
{
	d_this = new mxWindow_i;
	d_this->d_uTimer = 0;

	DWORD dwStyle = 0;
	if (style == Normal)
		dwStyle = WS_OVERLAPPEDWINDOW;
	else if (style == Popup)
		dwStyle = WS_POPUP;
	else if (style == Dialog || style == ModalDialog)
		dwStyle = WS_CAPTION | WS_SYSMENU;

	void *parentHandle = 0;
	if (parent)
	{
		parentHandle = parent->getHandle ();
		dwStyle = WS_CHILD | WS_VISIBLE;
	}

	void *handle = (void *) CreateWindowEx (0, "mx_class", label, dwStyle,
					x, y, w, h, (HWND) parentHandle,
					(HMENU) NULL, (HINSTANCE) GetModuleHandle (NULL), NULL);

	SetWindowLong ((HWND) handle, GWL_USERDATA, reinterpret_cast< LONG >( this ) );

	setHandle (handle);
	setType (MX_WINDOW);
	setParent (parent);
	//setLabel (label);
	//setBounds (x, y, w, h);

	if (!parent && !g_mainWindow)
		g_mainWindow = this;
}



mxWindow::~mxWindow ()
{
	SetWindowLong ((HWND) (HWND) getHandle(), GWL_USERDATA, (LONG) 0 );
	delete d_this;
}



int
mxWindow::handleEvent (mxEvent * /*event*/ )
{
	return 0;
}



void
mxWindow::redraw ()
{
}

void
mxWindow::setTimer (int milliSeconds)
{
	if (d_this->d_uTimer)
	{
		KillTimer ((HWND) getHandle (), d_this->d_uTimer);
		d_this->d_uTimer = 0;
	}

	if (milliSeconds > 0)
	{
		d_this->d_uTimer = 21001;
		d_this->d_uTimer = (UINT)SetTimer ((HWND) getHandle (), d_this->d_uTimer, milliSeconds, NULL);
	}
}



void
mxWindow::setMenuBar (mxMenuBar *menuBar)
{
	SetMenu ((HWND) getHandle (), (HMENU) ((mxWidget *) menuBar)->getHandle ());
}
