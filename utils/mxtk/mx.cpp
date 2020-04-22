//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mx.cpp
// implementation: Win32 API
// last modified:  Apr 18 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mx.h"
#include "mxtk/mxWindow.h"
#include "mxtk/mxEvent.h"
#include "mxtk/mxLinkedList.h"
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tier1/utlvector.h"


#define WM_MOUSEWHEEL                   0x020A

//#include <ostream.h"



void mxTab_resizeChild (HWND hwnd);



mxWindow *g_mainWindow = 0;
static mxLinkedList *g_widgetList = 0;
static mxWindow *g_idleWindow = 0;

static MSG msg;
static HWND g_hwndToolTipControl = 0;
static bool isClosing = false;
static HACCEL g_hAcceleratorTable = NULL;

void mx::createAccleratorTable( int numentries, Accel_t *entries )
{
	CUtlVector< ACCEL > accelentries;

	for ( int i = 0; i < numentries; ++i )
	{
		const Accel_t& entry = entries[ i ];

		ACCEL add;
		add.key = entry.key;
		add.cmd = entry.command;
		add.fVirt = 0;
		if ( entry.flags & ACCEL_ALT )
		{
			add.fVirt |= FALT;
		}
		if ( entry.flags & ACCEL_CONTROL )
		{
			add.fVirt |= FCONTROL;
		}
		if ( entry.flags & ACCEL_SHIFT )
		{
			add.fVirt |= FSHIFT;
		}
		if ( entry.flags & ACCEL_VIRTKEY )
		{
			add.fVirt |= FVIRTKEY;
		}

		accelentries.AddToTail( add );
	}

	g_hAcceleratorTable = ::CreateAcceleratorTable( accelentries.Base(), accelentries.Count() );
}



void
mx_addWidget (mxWidget *widget)
{
	if (g_widgetList)
		g_widgetList->add ((void *) widget);
}



void
mx_removeWidget (mxWidget *widget)
{
	if (g_widgetList)
		g_widgetList->remove ((void *) widget);
}



HWND
mx_CreateToolTipControl ()
{
	if (!g_hwndToolTipControl)
	{
		if (g_mainWindow)
		{
			g_hwndToolTipControl = CreateWindowEx (0, TOOLTIPS_CLASS, "", WS_POPUP | WS_EX_TOPMOST,
				0, 0, 0, 0, (HWND) g_mainWindow->getHandle (),
				(HMENU) NULL, (HINSTANCE) GetModuleHandle (NULL), NULL);
		}
	}

	return g_hwndToolTipControl;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *window - 
//			*event - 
// Output : static void
//-----------------------------------------------------------------------------
static void RecursiveHandleEvent( mxWindow *window, mxEvent *event )
{
	while ( window )
	{
		if ( window->handleEvent ( event ) )
			break;

		window = window->getParent();
	}
}

char const *translatecode( int code )
{
	switch ( code )
	{
	case NM_CLICK:
		return "NM_CLICK";
	case NM_CUSTOMDRAW:
		return "NM_CUSTOMDRAW";
	case NM_DBLCLK:
		return "NM_DBLCLK";
	case NM_KILLFOCUS:
		return "NM_KILLFOCUS";
	case NM_RCLICK:
		return "NM_RCLICK";
	case NM_RETURN:
		return "NM_RETURN";
	case NM_SETCURSOR:
		return "NM_SETCURSOR";
	case NM_SETFOCUS:
		return "NM_SETFOCUS"; 
	case TVN_BEGINDRAG:
		return "TVN_BEGINDRAG";
	case TVN_BEGINLABELEDIT:
		return "TVN_BEGINLABELEDIT";
	case TVN_BEGINRDRAG:
		return "TVN_BEGINRDRAG";
	case TVN_DELETEITEM:
		return "TVN_DELETEITEM";
	case TVN_ENDLABELEDIT:
		return "TVN_ENDLABELEDIT";
	case TVN_GETDISPINFO:
		return "TVN_GETDISPINFO";
	case TVN_GETINFOTIP:
		return "TVN_GETINFOTIP";
	case TVN_ITEMEXPANDED:
		return "TVN_ITEMEXPANDED";
	case TVN_ITEMEXPANDING:
		return "TVN_ITEMEXPANDING";
	case TVN_KEYDOWN :
		return "TVN_KEYDOWN";
	case TVN_SELCHANGED :
		return "TVN_SELCHANGED";
	case TVN_SELCHANGING :
		return "TVN_SELCHANGING";
	case TVN_SETDISPINFO :
		return "TVN_SETDISPINFO";
	case TVN_SINGLEEXPAND:
		return "TVN_SINGLEEXPAND";
	}

	return "Unknown!!!";
}
static LRESULT CALLBACK WndProc (HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	static bool bDragging = FALSE;

	switch (uMessage)
	{
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if ( window )
		{
			mxEvent event;
			event.event = mxEvent::Focus;
			event.widget = NULL;
			event.action = (uMessage == WM_SETFOCUS);
			RecursiveHandleEvent( window, &event );
			return 0;
		}
	}
	break;

	case WM_ACTIVATE:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if ( window )
		{
			mxEvent event;
			event.event = mxEvent::Activate;
			event.widget = NULL;
			event.action = (LOWORD( wParam ) != WA_INACTIVE);
			RecursiveHandleEvent( window, &event );
			return 0;
		}
	}
	break;

	case WM_COMMAND:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (LOWORD (wParam) > 0 && window)
		{
			WORD wNotifyCode = (WORD) HIWORD (wParam);
			HWND hwndCtrl = (HWND) lParam;
			mxEvent event;

			CHAR className[128];
			GetClassName (hwndCtrl, className, 128);
			if (!strcmpi (className, "edit"))
			{
				if (wNotifyCode != EN_CHANGE)
					break;
			}
			else if (!strcmpi (className, "combobox"))
			{
				if (wNotifyCode != CBN_SELCHANGE)
					break;
			}
			else if (!strcmpi (className, "listbox"))
			{
				if (wNotifyCode != LBN_SELCHANGE)
					break;
			}

			event.event = mxEvent::Action;
			event.widget = (mxWidget *) GetWindowLong ((HWND) lParam, GWL_USERDATA);
			event.action = (int) LOWORD (wParam);
			RecursiveHandleEvent( window, &event );
		}
	}
	break;

	case WM_NOTIFY:
	{
		if (isClosing)
			break;

		NMHDR *nmhdr = (NMHDR *) lParam;
		mxEvent event;

#if 0
		//if ( nmhdr->idFrom > 0 ) 
		{
			mxWidget *temp = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
			if ( temp && temp->getType() == MX_TREEVIEW )
			{
				NMTREEVIEW *nmt = ( NMTREEVIEW * )nmhdr;

				HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);

				char sz[ 256 ];
				sprintf( sz, "tree view receiving notify %i : %s action %i old %p new %p selection %p\n", nmhdr->code, translatecode( nmhdr->code ),
					nmt->action, nmt->itemOld, nmt->itemNew, hItem );
				
				OutputDebugString( sz );
			}
		}
#endif

		if (nmhdr->code == TVN_SELCHANGED)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;

				RECT rc;
				HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);
				TreeView_GetItemRect (nmhdr->hwndFrom, hItem, &rc, TRUE);
				event.x = (int) rc.left;
				event.y = (int) rc.bottom;
				RecursiveHandleEvent( window, &event );

			}
		}
		else if (nmhdr->code == LVN_ITEMCHANGED)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;

				RecursiveHandleEvent( window, &event );
			}
		}
		else if (nmhdr->code == NM_RCLICK)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;
				event.flags = mxEvent::RightClicked;

				if ( event.widget )
				{
					if ( event.widget->getType () == MX_TREEVIEW )
					{
						RECT rc;
						HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);
						TreeView_GetItemRect (nmhdr->hwndFrom, hItem, &rc, TRUE);
						event.x = (int) rc.left;
						event.y = (int) rc.bottom;
					}
				}
				RecursiveHandleEvent( window, &event );
			}
		}
		else if (nmhdr->code == NM_DBLCLK)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;
				event.flags = mxEvent::DoubleClicked;

				if (event.widget )
				{
					if ( event.widget->getType () == MX_TREEVIEW )
					{
						RECT rc;
						HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);
						TreeView_GetItemRect (nmhdr->hwndFrom, hItem, &rc, TRUE);
						event.x = (int) rc.left;
						event.y = (int) rc.bottom;
					}
				}

				RecursiveHandleEvent( window, &event );
				return TRUE;
			}
		}
		else if (nmhdr->code == TCN_SELCHANGING)
		{
			TC_ITEM ti;

			int index = TabCtrl_GetCurSel (nmhdr->hwndFrom);
			if (index >= 0)
			{
				ti.mask = TCIF_PARAM;
				TabCtrl_GetItem (nmhdr->hwndFrom, index, &ti);
				mxWindow *window = (mxWindow *) ti.lParam;
				if (window)
					window->setVisible (false);
			}
		}
		else if (nmhdr->code == TCN_SELCHANGE)
		{
			mxTab_resizeChild (nmhdr->hwndFrom);
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;
				RecursiveHandleEvent( window, &event );
			}
		}
	}
	break;

	case WM_SIZE:
	{
		mxEvent event;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			event.event = mxEvent::Size;
			event.width = (int) LOWORD (lParam);
			event.height = (int) HIWORD (lParam);
			window->handleEvent (&event);
		}
	}
	break;
	case WM_WINDOWPOSCHANGED:
	{
		mxEvent event;

		
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			event.event = mxEvent::PosChanged;

			WINDOWPOS *wp = ( WINDOWPOS * )lParam;

			event.x			= wp->x;
			event.y			= wp->y;
			event.width		= wp->cx;
			event.height	= wp->cy;

			window->handleEvent (&event);
		}
	}
	break;

	case WM_ERASEBKGND:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			if (window->getType () == MX_GLWINDOW)
				return 0;
			if (window->getType () == MX_MATSYSWINDOW)
				return 0;

			if ( !isClosing && !window->PaintBackground() )
			{
				return 0;
			}
		}
	}
	break;

	case WM_HSCROLL:
	case WM_VSCROLL:
	{
		mxWidget *widget = (mxWidget *) GetWindowLong ((HWND) lParam, GWL_USERDATA);
		if (!widget)
		{
			break;
		}

		if (widget->getType() != MX_SCROLLBAR && widget->getType() != MX_SLIDER)
		{
			break;
		}


		switch (LOWORD (wParam))
		{
		case TB_LINEUP: // 	SB_LINEUP SB_LINELEFT
			break;
		case TB_LINEDOWN: // SB_LINEDOWN SB_LINERIGHT
			break;
		case TB_PAGEUP: // SB_PAGEUP SB_PAGELEFT
			break;
		case TB_PAGEDOWN: // SB_PAGEDOWN SB_PAGERIGHT
			break;
		case TB_THUMBPOSITION:  // SB_THUMBPOSITION
			break;
		case TB_THUMBTRACK: // SB_THUMBTRACK
			break;
		case TB_TOP: // SB_TOP SB_LEFT
			break;
		case TB_BOTTOM: // SB_BOTTOM SB_RIGHT
			break;
		case TB_ENDTRACK: // SB_ENDSCROLL
			break;
		default:
			break;
		}

		switch (LOWORD (wParam))
		{
		case TB_LINEUP: // 	SB_LINEUP SB_LINELEFT
		case TB_LINEDOWN: // SB_LINEDOWN SB_LINERIGHT
		case TB_PAGEUP: // SB_PAGEUP SB_PAGELEFT
		case TB_PAGEDOWN: // SB_PAGEDOWN SB_PAGERIGHT
		case TB_THUMBPOSITION:  // SB_THUMBPOSITION
		case TB_THUMBTRACK: // SB_THUMBTRACK
		case TB_TOP: // SB_TOP SB_LEFT
		case TB_BOTTOM: // SB_BOTTOM SB_RIGHT
		case TB_ENDTRACK: // SB_ENDSCROLL
		{
			mxEvent event;

			event.event = mxEvent::Action;
			event.widget = widget;
			event.action = widget->getId ();
			event.modifiers = LOWORD (wParam);
			event.height = HIWORD( wParam );
			mxWindow *window = widget->getParent ();

			if ( event.action > 0 )
			{
				RecursiveHandleEvent( window, &event );
			}
		}
		break;
		}
	}
	break;

	case WM_PAINT:
	{
		if ( !isClosing )
		{
			mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
			if (window)
			{
				window->redraw ();
			}
		}
	}
	break;

	case WM_PARENTNOTIFY:
		{
			mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
			if (window)
			{
				if ( wParam == WM_LBUTTONDOWN ||
					 wParam == WM_MBUTTONDOWN ||
					 wParam == WM_RBUTTONDOWN /*||
					 wParam & WM_XBUTTONDOWN*/ )
				{
					mxEvent event;
					event.event = mxEvent::ParentNotify;
					event.x = (short)LOWORD (lParam);
					event.y = (short)HIWORD (lParam);
					event.buttons = 0;
					event.modifiers = 0;

					if ( wParam == WM_LBUTTONDOWN )
						event.buttons |= mxEvent::MouseLeftButton;

					if ( wParam == WM_RBUTTONDOWN )
						event.buttons |= mxEvent::MouseRightButton;

					if ( wParam == WM_MBUTTONDOWN )
						event.buttons |= mxEvent::MouseMiddleButton;

					window->handleEvent (&event);
					RecursiveHandleEvent( window, &event );
					return 0;
				}
			}
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		bDragging = TRUE;
		SetCapture (hwnd);
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);

		if (window)
		{
			mxEvent event;
			event.event = mxEvent::MouseDown;
			event.x = (short)LOWORD (lParam);
			event.y = (short)HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (uMessage == WM_MBUTTONDOWN)
				event.buttons |= mxEvent::MouseMiddleButton;
			else if (uMessage == WM_RBUTTONDOWN)
				event.buttons |= mxEvent::MouseRightButton;
			else
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
		}
	}
	break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::MouseUp;
			event.x = (short) LOWORD (lParam);
			event.y = (short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (uMessage == WM_MBUTTONUP)
				event.buttons |= mxEvent::MouseMiddleButton;
			else if (uMessage == WM_RBUTTONUP)
				event.buttons |= mxEvent::MouseRightButton;
			else
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
		}
		bDragging = FALSE;
		ReleaseCapture ();
	}
	break;

	case WM_MOUSEMOVE:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;

			if (bDragging)
				event.event = mxEvent::MouseDrag;
			else
				event.event = mxEvent::MouseMove;

			event.x = (short) LOWORD (lParam);
			event.y = (short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
		}
	}
	break;
	case WM_NCLBUTTONDOWN:
	case WM_NCMBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);

		if (window)
		{
			mxEvent event;
			event.event = mxEvent::NCMouseDown;
			event.x = (short) LOWORD (lParam);
			event.y = (short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (uMessage == WM_NCMBUTTONDOWN)
				event.buttons |= mxEvent::MouseMiddleButton;
			else if (uMessage == WM_NCRBUTTONDOWN)
				event.buttons |= mxEvent::MouseRightButton;
			else
				event.buttons |= mxEvent::MouseLeftButton;

			window->handleEvent (&event);
		}
	}
	break;

	case WM_NCLBUTTONUP:
	case WM_NCMBUTTONUP:
	case WM_NCRBUTTONUP:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::NCMouseUp;
			event.x = (short) LOWORD (lParam);
			event.y = (short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (uMessage == WM_NCMBUTTONUP)
				event.buttons |= mxEvent::MouseMiddleButton;
			else if (uMessage == WM_NCRBUTTONUP)
				event.buttons |= mxEvent::MouseRightButton;
			else
				event.buttons |= mxEvent::MouseLeftButton;

			window->handleEvent (&event);
		}
	}
	break;

	case WM_NCMOUSEMOVE:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;

			event.event = mxEvent::NCMouseMove;

			event.x = (short) LOWORD (lParam);
			event.y = (short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			window->handleEvent (&event);
		}
	}
	break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::KeyDown;
			event.key = (int) wParam;
			if ( window->handleEvent (&event) )
				return 0;
		}
	}
	break;

	case WM_CHAR:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::Char;
			event.key = (int) wParam;
			if ( window->handleEvent (&event) )
				return 0;
		}
	}
	break;

	case WM_SYSCHAR:
		return 0;
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::KeyUp;
			event.key = (int) wParam;
			if ( window->handleEvent (&event) )
				return 0;
		}
	}
	break;

	case WM_MOUSEWHEEL:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			memset( &event, 0, sizeof( event ) );
			event.event = mxEvent::MouseWheeled;
			event.x = (short) LOWORD (lParam);
			event.y = (short) HIWORD (lParam);

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			event.height = (short)HIWORD( wParam );;
			RecursiveHandleEvent( window, &event );
		}
	}
	break;
	case WM_TIMER:
	{
		if (isClosing)
			break;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::Timer;
			window->handleEvent (&event);
		}
	}
	break;

	case WM_CLOSE:
		if (g_mainWindow)
		{
			if ((void *) hwnd == g_mainWindow->getHandle ())
			{
				mx::quit ();
			}
			else
			{
				ShowWindow (hwnd, SW_HIDE);

				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				if (window)
				{
					mxEvent event;
					event.event = mxEvent::Close;
					window->handleEvent( &event );
				}
			}
		}
		//else // shouldn't happen
			//DestroyWindow (hwnd);
		return 0;
/*
	case WM_DESTROY:
		if (g_mainWindow)
		{
			if ((void *) hwnd == g_mainWindow->getHandle ())
				mx::quit ();
		}
		break;
*/
	}

	return DefWindowProc (hwnd, uMessage, wParam, lParam);
}



int
mx::init(int argc, char **argv)
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = (HINSTANCE) GetModuleHandle (NULL);
    wc.hIcon = LoadIcon (wc.hInstance, "MX_ICON");
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) COLOR_WINDOW;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "mx_class";

	if (!wc.hIcon)
		wc.hIcon = LoadIcon (NULL, IDI_WINLOGO);

	if (!RegisterClass (&wc))
		return 0;

	InitCommonControls ();

	g_widgetList = new mxLinkedList ();

	isClosing = false;

	return 1;
}



int
mx::run()
{
	int messagecount = 0;

	while (1)
	{
		bool doframe = false;
		if ( PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE) || !g_idleWindow )
		{
			if (!GetMessage (&msg, NULL, 0, 0))
			{
				doframe = false;
				break;
			}

			if ( !g_hAcceleratorTable ||
				!TranslateAccelerator( (HWND)g_mainWindow->getHandle (), g_hAcceleratorTable, &msg )) 
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
			messagecount++;

			if ( messagecount > 10 )
			{
				messagecount = 0;
				doframe = true;
			}
		}
		else if (g_idleWindow)
		{
			doframe = true;
			messagecount = 0;
		}

		if ( doframe && g_idleWindow )
		{
			mxEvent event;
			event.event = mxEvent::Idle;
			g_idleWindow->handleEvent (&event);
		}
	}

	return msg.wParam;
}



int
mx::check ()
{
	if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (GetMessage (&msg, NULL, 0, 0))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		return 1;
	}

	return 0;
}



void
mx::quit ()
{
	isClosing = true;

	mxWindow *mainwnd = getMainWindow();
	if ( mainwnd )
	{
		if ( !mainwnd->Closing() )
		{
			isClosing = false;
			return;
		}
	}

	if (g_widgetList)
	{
		// remove from back to front
		mxListNode *node = g_widgetList->getLast ();

		// Pass 1, see if anyone objects to closing
		while (node)
		{
			mxWidget *widget = (mxWidget *) g_widgetList->getData (node);
			node = g_widgetList->getPrev (node);

			bool canclose = true;
			if ( widget )
			{
				if ( !widget->CanClose() )
				{
					canclose = false;
				}
			}

			if ( !canclose )
			{
				isClosing = false;
				return;
			}
		}

		node = g_widgetList->getLast ();

		// Pass 2, call OnDelete to allow final cleanup
		while (node)
		{
			mxWidget *widget = (mxWidget *) g_widgetList->getData (node);
			node = g_widgetList->getPrev (node);

			if ( widget )
			{
				widget->OnDelete();
			}
		}

		node = g_widgetList->getLast ();

		// Pass 3, delete stuff
		while (node)
		{
			mxWidget *widget = (mxWidget *) g_widgetList->getData (node);
			node = g_widgetList->getPrev (node);

			// remove it!
			if ( widget )
			{
				delete widget;
			}
		}

		delete g_widgetList;
	}

	if (g_hwndToolTipControl)
		DestroyWindow (g_hwndToolTipControl);

	if ( g_hAcceleratorTable )
	{
		DestroyAcceleratorTable( g_hAcceleratorTable );
		g_hAcceleratorTable = 0;
	}

	PostQuitMessage (0);
	UnregisterClass ("mx_class", (HINSTANCE) GetModuleHandle (NULL));
}



int
mx::setDisplayMode (int w, int h, int bpp)
{
	DEVMODE dm;

	dm.dmSize = sizeof (DEVMODE);
	dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	dm.dmBitsPerPel = bpp;
	dm.dmPelsWidth = w;
	dm.dmPelsHeight = h;

	if (w == 0 || h == 0 || bpp == 0)
		ChangeDisplaySettings (0, 0);
	else
		ChangeDisplaySettings (&dm, CDS_FULLSCREEN);

	return 0;
}



void
mx::setIdleWindow (mxWindow *window)
{
	g_idleWindow = window;
}



int
mx::getDisplayWidth ()
{
	return (int) GetSystemMetrics (SM_CXSCREEN);
}



int
mx::getDisplayHeight ()
{
	return (int) GetSystemMetrics (SM_CYSCREEN);
}



mxWindow*
mx::getMainWindow ()
{
	return g_mainWindow;
}



const char *
mx::getApplicationPath ()
{
	static char path[256];
	GetModuleFileName (0, path, 256);
	char *ptr = strrchr (path, '\\');
	if (ptr)
		*ptr = '\0';

	return path;
}



int
mx::getTickCount ()
{
	return (int) GetTickCount ();
}
