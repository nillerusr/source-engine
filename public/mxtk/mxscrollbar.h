//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxScrollbar.h
// implementation: all
// last modified:  Mar 14 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXSCROLLBAR
#define INCLUDED_MXSCROLLBAR



#ifndef INCLUDED_MXWIDGET
#include "mxtk/mxWidget.h"
#endif



class mxWindow;



class mxScrollbar_i;
class mxScrollbar : public mxWidget
{
	mxScrollbar_i *d_this;

public:
	// ENUMS
	enum { Horizontal, Vertical };

	// CREATORS
	mxScrollbar (mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0);
	virtual ~mxScrollbar ();

	// MANIPULATORS
	void setValue (int ivalue);
	void setRange (int min, int max);
	void setPagesize (int size);

	// ACCESSORS
	int getValue () const;
	int getMinValue () const;
	int getMaxValue () const;
	int getPagesize () const;

private:
	// NOT IMPLEMENTED
	mxScrollbar (const mxScrollbar&);
	mxScrollbar& operator= (const mxScrollbar&);
};



#endif // INCLUDED_MXSCROLLBAR
