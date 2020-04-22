//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxGlWindow.cpp
// implementation: Win32 API
// last modified:  Apr 21 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxGlWindow.h"
#include <windows.h>
//#include <ostream.h"



static int g_formatMode = mxGlWindow::FormatDouble;
static int g_formatColorBits = 24;
static int g_formatDepthBits = 16;



class mxGlWindow_i
{
public:
	HDC hdc;
	HGLRC hglrc;
};



mxGlWindow::mxGlWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: mxWindow (parent, x, y, w, h, label, style)
{
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof (PIXELFORMATDESCRIPTOR),		// size of this pfd
		 1,		// version number
		 PFD_DRAW_TO_WINDOW |	// support window
		 PFD_SUPPORT_OPENGL |	// support OpenGL
		 PFD_DOUBLEBUFFER,	// double buffered
		 PFD_TYPE_RGBA,	// RGBA type
		 24,		// 24-bit color depth
		 0, 0, 0, 0, 0, 0,	// color bits ignored
		 0,		// no alpha buffer
		 0,		// shift bit ignored
		 0,		// no accumulation buffer
		 0, 0, 0, 0,	// accum bits ignored
		 16,		// 32-bit z-buffer      
		 0,		// no stencil buffer
		 0,		// no auxiliary buffer
		 PFD_MAIN_PLANE,	// main layer
		 0,		// reserved
		 0, 0, 0	// layer masks ignored
	};

	d_this = new mxGlWindow_i;

	pfd.cColorBits = g_formatColorBits;
	pfd.cDepthBits = g_formatDepthBits;

	bool error = false;

	if ((d_this->hdc = GetDC ((HWND) getHandle ())) == NULL)
	{
		error = true;
		goto done;
	}

	int pfm;
	if ((pfm = ChoosePixelFormat (d_this->hdc, &pfd)) == 0)
	{
		error = true;
		goto done;
	}

	if (SetPixelFormat (d_this->hdc, pfm, &pfd) == FALSE)
	{
		error = true;
		goto done;
	}

	DescribePixelFormat (d_this->hdc, pfm, sizeof (pfd), &pfd);

	if ((d_this->hglrc = wglCreateContext (d_this->hdc)) == 0)
	{
		error = true;
		goto done;
	}

	if (!wglMakeCurrent (d_this->hdc, d_this->hglrc))
	{
		error = true;
		goto done;
	}

	setType (MX_GLWINDOW);
	setDrawFunc (0);

done:
	if (error)
		delete this;
}



mxGlWindow::~mxGlWindow ()
{
	if (d_this->hglrc)
	{
		wglMakeCurrent (NULL, NULL);
		//wglDeleteContext (d_this->hglrc);
	}

	if (d_this->hdc)
		ReleaseDC ((HWND) getHandle (), d_this->hdc);

	delete d_this;
}



int
mxGlWindow::handleEvent (mxEvent *event)
{
	return 0;
}



void
mxGlWindow::redraw ()
{
	makeCurrent ();
	if (d_drawFunc)
		d_drawFunc ();
	else
		draw ();
	swapBuffers ();
}



void
mxGlWindow::draw ()
{
}



int
mxGlWindow::makeCurrent ()
{
	if (wglMakeCurrent (d_this->hdc, d_this->hglrc))
		return 1;

	return 0;
}



int
mxGlWindow::swapBuffers ()
{
	if (SwapBuffers (d_this->hdc))
		return 1;

	return 0;
}



void
mxGlWindow::setDrawFunc (void (*func) (void))
{
	d_drawFunc = func;
}



void
mxGlWindow::setFormat (int mode, int colorBits, int depthBits)
{
	g_formatMode = mode;
	g_formatColorBits = colorBits;
	g_formatDepthBits = depthBits;
}
