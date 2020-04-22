//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMatSysWindow.cpp
// implementation: Win32 API
// last modified:  Apr 21 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxMatSysWindow.h"
#include <windows.h>

class mxMatSysWindow_i
{
public:
	HDC hdc;
	HGLRC hglrc;
};



mxMatSysWindow::mxMatSysWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: mxWindow (parent, x, y, w, h, label, style)
{
	d_this = new mxMatSysWindow_i;

	bool error = false;

	if ((d_this->hdc = GetDC ((HWND) getHandle ())) == NULL)
	{
		error = true;
		goto done;
	}

	setDrawFunc (0);

done:
	if (error)
		delete this;
}



mxMatSysWindow::~mxMatSysWindow ()
{
	if (d_this->hdc)
		ReleaseDC ((HWND) getHandle (), d_this->hdc);

	delete d_this;
}



int
mxMatSysWindow::handleEvent (mxEvent *event)
{
	return 0;
}



void
mxMatSysWindow::redraw ()
{
	// makeCurrent ();
	if (d_drawFunc)
		d_drawFunc ();
	else
		draw ();
	// swapBuffers ();
}



void
mxMatSysWindow::draw ()
{
}



int
mxMatSysWindow::makeCurrent ()
{
	return 1;
}



int
mxMatSysWindow::swapBuffers ()
{
	return 0;
}



void
mxMatSysWindow::setDrawFunc (void (*func) (void))
{
	d_drawFunc = func;
}


