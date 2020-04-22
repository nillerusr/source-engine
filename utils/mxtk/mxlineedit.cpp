//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxLineEdit.cpp
// implementation: Win32 API
// last modified:  Mar 18 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxLineEdit.h"
#include <windows.h>
#include "mxtk/mxEvent.h"
#include "mxtk/mxWindow.h"


class mxLineEdit_i
{
public:
	int dummy;
};

#include "tier0/dbg.h"


typedef LRESULT (CALLBACK * WndProc_t)(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

static WndProc_t s_OldWndProc = 0;

static LRESULT CALLBACK EditWndProc (HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	int iret = 0;

	// This lovely bit of hackery ensures we get return key events
	if ( uMessage == WM_CHAR)
	{
		iret = s_OldWndProc( hwnd, uMessage, wParam, lParam );

		// Post the message directly to all windows in the hierarchy until
		// someone responds
		mxLineEdit *lineEdit = (mxLineEdit *) GetWindowLong (hwnd, GWL_USERDATA);
		mxEvent event;
		event.event = mxEvent::KeyDown;
		event.action = lineEdit->getId();
		event.key = (int) wParam;

		mxWindow* window = lineEdit->getParent();
		while (window)
		{
			if (window->handleEvent (&event))
				break;

			window = window->getParent ();
		}
	}
	else
	{
		if ( uMessage == WM_LBUTTONDOWN )
		{
			SetFocus( hwnd );
		}
		iret = s_OldWndProc( hwnd, uMessage, wParam, lParam );
	}
	return iret;
}

mxLineEdit::mxLineEdit (mxWindow *parent, int x, int y, int w, int h, const char *label, int id, int style)
: mxWidget (parent, x, y, w, h, label)
{
	if (!parent)
		return;

	DWORD dwStyle = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP; //  | ES_WANTRETURN | ES_MULTILINE;
	HWND hwndParent = (HWND) ((mxWidget *) parent)->getHandle ();

	if (style == ReadOnly)
		dwStyle |= ES_READONLY;
	else if (style == Password)
		dwStyle |= ES_PASSWORD;

	if (!s_OldWndProc)
	{
		WNDCLASSEX editClass;
		GetClassInfoEx( (HINSTANCE) GetModuleHandle (NULL), "EDIT", &editClass );
		s_OldWndProc = editClass.lpfnWndProc;

		editClass.cbSize = sizeof(WNDCLASSEX);
		editClass.cbClsExtra = 0;
		editClass.lpfnWndProc = EditWndProc;
		editClass.lpszClassName = "mx_edit";
		RegisterClassEx( &editClass ); 
	}

	void *handle = (void *) CreateWindowEx (WS_EX_CLIENTEDGE, "mx_edit", label, dwStyle, //WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
				x, y, w, h, hwndParent,
				(HMENU) id, (HINSTANCE) GetModuleHandle (NULL), NULL);
	
	SendMessage ((HWND) handle, WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_VAR_FONT), MAKELPARAM (TRUE, 0));
	SendMessage ((HWND) getHandle (), EM_LIMITTEXT, (WPARAM) 256, 0L);
	SetWindowLong ((HWND) handle, GWL_USERDATA, (LONG) this);

	setHandle (handle);
	setType (MX_LINEEDIT);
	setParent (parent);
	setId (id);
}



mxLineEdit::~mxLineEdit ()
{
}

void mxLineEdit::clear()
{
	SendMessage( (HWND)getHandle(), WM_SETTEXT, (WPARAM)0, (LPARAM)"" );
}

void mxLineEdit::getText( char *buf, size_t bufsize )
{
	buf[ 0 ] = 0;
	SendMessage( (HWND) getHandle (), WM_GETTEXT, (WPARAM)bufsize, (LPARAM)buf );
}

void
mxLineEdit::setMaxLength (int max)
{
	SendMessage ((HWND) getHandle (), EM_LIMITTEXT, (WPARAM) max, 0L);
}



int
mxLineEdit::getMaxLength () const
{
	return (int) SendMessage ((HWND) getHandle (), EM_GETLIMITTEXT, 0, 0L);
}
