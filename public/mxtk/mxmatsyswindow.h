//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMatSysWindow.h
// implementation: all
// last modified:  Apr 21 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXMATSYSWINDOW
#define INCLUDED_MXMATSYSWINDOW



#ifndef INCLUDED_MXWINDOW
#include "mxtk/mxWindow.h"
#endif



class mxMatSysWindow_i;
class mxMatSysWindow : public mxWindow
{
	mxMatSysWindow_i *d_this;
	void (*d_drawFunc) (void);

public:
	// ENUMS
	enum { FormatDouble, FormatSingle };

	// CREATORS
	mxMatSysWindow (mxWindow *parent, int x, int y, int w, int h, const char *label = 0, int style = 0);
	virtual ~mxMatSysWindow ();

	// IMaterialSystem *Init( const char *szGamedir, const char *szMaterialSystem, const char *szShader, const char *szProxy );

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
	virtual void redraw ();
	virtual void draw ();

	int makeCurrent ();
	int swapBuffers ();

	// MANIPULATORS
	void setDrawFunc (void (*func) (void));

private:
	// NOT IMPLEMENTED
	mxMatSysWindow (const mxMatSysWindow&);
	mxMatSysWindow& operator= (const mxMatSysWindow&);
};



#endif // INCLUDED_MXGLWINDOW
