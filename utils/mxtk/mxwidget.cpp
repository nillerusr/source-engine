//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxWidget.cpp
// implementation: Win32 API
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxWidget.h"
#include <windows.h>
#include <commctrl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void mxTab_resizeChild (HWND hwnd);
void mx_addWidget (mxWidget *widget);
void mx_removeWidget (mxWidget *widget);



class mxWidget_i
{
public:
	mxWindow *d_parent_p;
	HWND d_hwnd;
	void *d_userData;
	int d_type;
};



mxWidget::mxWidget (mxWindow *parent, int x, int y, int w, int h, const char *label)
{
	d_this = new mxWidget_i;

	setHandle (0);
	setType (-1);
	setParent (parent);
	setBounds (x, y, w, h);
	setVisible (true);
	setEnabled (true);
	setId (0);
	setUserData (0);
	setLabel (label);

	mx_addWidget (this);
}



mxWidget::~mxWidget ()
{
	mx_removeWidget (this);

	if (d_this->d_type == MX_MENU ||
		d_this->d_type == MX_MENUBAR ||
		d_this->d_type == MX_POPUPMENU)
		DestroyMenu ((HMENU) d_this->d_hwnd);
	else
		DestroyWindow (d_this->d_hwnd);

	delete d_this;
}

bool mxWidget::CanClose()
{
	// Assume yes
	return true;
}

void mxWidget::OnDelete()
{
	// Nothing
}

void
mxWidget::setHandle (void *handle)
{
	d_this->d_hwnd = (HWND) handle;
}



void
mxWidget::setType (int type)
{
	d_this->d_type = type;
}



void
mxWidget::setParent (mxWindow *parentWindow)
{
	d_this->d_parent_p = parentWindow;
}



void
mxWidget::setBounds (int x, int y, int w, int h)
{
	char str[128];
	GetClassName (d_this->d_hwnd, str, 128);

	if (!strcmp (str, "COMBOBOX"))
		MoveWindow (d_this->d_hwnd, x, y, w, h + 100, TRUE);
	else
		MoveWindow (d_this->d_hwnd, x, y, w, h, TRUE);

	if (!strcmp (str, WC_TABCONTROL))
		mxTab_resizeChild (d_this->d_hwnd);
}



void
mxWidget::setLabel (const char *format, ... )
{
	if (format == NULL)
	{
		if (d_this->d_hwnd)
		{
			SetWindowText (d_this->d_hwnd, NULL);
		}
		return;
	}

	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	if (d_this->d_hwnd)
	{
		SetWindowText (d_this->d_hwnd, string);
	}
}


void
mxWidget::setVisible (bool b)
{
	if (b)
		SetWindowPos (d_this->d_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	else
		ShowWindow (d_this->d_hwnd, SW_HIDE);
}



void
mxWidget::setEnabled (bool b)
{
	EnableWindow (d_this->d_hwnd, b);
}



void
mxWidget::setId (int id)
{
	SetWindowLong (d_this->d_hwnd, GWL_ID, (LONG) id);
}



void
mxWidget::setUserData (void *userData)
{
	d_this->d_userData = userData;
}



void*
mxWidget:: getHandle () const
{
	return (void *) d_this->d_hwnd;
}



int
mxWidget::getType () const
{
	return d_this->d_type;
}



mxWindow*
mxWidget::getParent () const
{
	return d_this->d_parent_p;
}



int
mxWidget::x () const
{
	RECT rc;
	GetWindowRect (d_this->d_hwnd, &rc);
	return  (int) rc.left;
}



int
mxWidget::y () const
{
	RECT rc;
	GetWindowRect (d_this->d_hwnd, &rc);
	return (int) rc.top;
}



int
mxWidget::w () const
{
	RECT rc;
	GetWindowRect (d_this->d_hwnd, &rc);
	return (int) (rc.right - rc.left);
}



int
mxWidget::h () const
{
	RECT rc;
	GetWindowRect (d_this->d_hwnd, &rc);
	return (int) (rc.bottom - rc.top);
}



int
mxWidget::w2 () const
{
	RECT rc;
	GetClientRect (d_this->d_hwnd, &rc);
	return (int) (rc.right - rc.left);
}



int
mxWidget::h2 () const
{
	RECT rc;
	GetClientRect (d_this->d_hwnd, &rc);
	return (int) (rc.bottom - rc.top);
}



const char*
mxWidget::getLabel () const
{
	static char label[256];
	GetWindowText (d_this->d_hwnd, label, 256);
	return label;
}



bool
mxWidget::isVisible () const
{
	return ( IsWindowVisible (d_this->d_hwnd) ? true : false );
}



bool
mxWidget::isEnabled () const
{
	return ( IsWindowEnabled (d_this->d_hwnd) ? true : false );
}



int
mxWidget::getId () const
{
	return (int) GetWindowLong (d_this->d_hwnd, GWL_ID);
}



void*
mxWidget::getUserData () const
{
	return d_this->d_userData;
}
