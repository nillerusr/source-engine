//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxWindow.h
// implementation: all
// last modified:  Mar 14 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXWINDOW
#define INCLUDED_MXWINDOW



#ifndef INCLUDED_MXWIDGET
#include "mxtk/mxWidget.h"
#endif

#ifndef INCLUDED_MXEVENT
#include "mxtk/mxEvent.h"
#endif



class mxMenuBar;



class mxWindow_i;
class mxWindow : public mxWidget
{
	mxWindow_i *d_this;

public:
	// ENUMS
	enum { Normal, Popup, Dialog, ModalDialog };

	// CREATORS
	mxWindow (mxWindow *parent, int x, int y, int w, int h, const char *label = 0, int style = 0);
	virtual ~mxWindow ();

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
/*
	virtual int handleActionEvent (int action);
	virtual void handleSizeEvent (int w, int h);
	virtual void handleMouseEvent (int event, int x, int y, int button);
	virtual void handleKeyEvent (int event, int key);
	virtual void handleTimerEvent ();
	virtual void handleIdleEvent ();
*/
	virtual void redraw ();

	virtual bool PaintBackground( void ) { return true; };

	// Called at exit
	virtual bool Closing( void ) { return true; };

	void setTimer (int milliSeconds);
	void setMenuBar (mxMenuBar *menuBar);

private:
	// NOT IMPLEMENTED
	mxWindow (const mxWindow&);
	mxWindow& operator= (const mxWindow&);
};



#endif // INCLUDED_MXWINDOW
