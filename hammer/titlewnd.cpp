//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <commctrl.h>
#include "TitleWnd.h"
#include "MainFrm.h"
#include "Resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


CFont CTitleWnd::m_FontNormal;
CFont CTitleWnd::m_FontActive;


BEGIN_MESSAGE_MAP(CTitleWnd, CWnd)
	ON_WM_PAINT()
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Factory. Registers the title window class if necessary and creates
//			a title window.
// Input  : pwndParent - Pointer to parent window.
//			uID - Window ID to use for the title window.
// Output : Returns a pointer to the newly created title window.
//-----------------------------------------------------------------------------
CTitleWnd *CTitleWnd::CreateTitleWnd(CWnd *pwndParent, UINT uID)
{
	//
	// Register the window class if we have not done so already.
	//
	static CString strTitleWndClass;
	if (strTitleWndClass.IsEmpty())
	{
		strTitleWndClass = AfxRegisterWndClass(CS_BYTEALIGNCLIENT, AfxGetApp()->LoadStandardCursor(IDC_ARROW), HBRUSH(GetStockObject(BLACK_BRUSH)));
	}

	//
	// Create the title window.
	//
	CTitleWnd *pWnd = new CTitleWnd();
	if (pWnd != NULL)
	{
		pWnd->Create(strTitleWndClass, "Title Window", WS_CHILD | WS_VISIBLE, CRect(0, 0, 5, 5), pwndParent, uID);
	}

	return(pWnd);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Creates fonts the first time it is called.
//-----------------------------------------------------------------------------
CTitleWnd::CTitleWnd(void)
{
	if (!m_FontNormal.m_hObject)
	{
		//
		// Create two fonts, a normal one and a bold one for when we are active.
		//
		m_FontNormal.CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
		m_FontActive.CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
	}

	m_bMenuOpen = false;
	m_bMouseOver = false;
	m_szTitle[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: Sets the text to display in the window. The window size is recalculated
//			to ensure that the text fits.
// Input  : pszTitle - Text to display in the window.
//-----------------------------------------------------------------------------
void CTitleWnd::SetTitle(LPCTSTR pszTitle)
{
	Assert(pszTitle != NULL);
	if (pszTitle != NULL)
	{
		strcpy(m_szTitle, pszTitle);
		if (::IsWindow(m_hWnd))
		{
			CDC *pDC = GetDC();
			if (pDC != NULL)
			{
				pDC->SelectObject(&m_FontActive);
				CSize TextSize = pDC->GetTextExtent(m_szTitle, strlen(m_szTitle));
				SetWindowPos(NULL, 0, 0, TextSize.cx, TextSize.cy, SWP_NOMOVE | SWP_NOZORDER);
				Invalidate();
				UpdateWindow();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Turns off the active font when the mouse leaves our client area.
// Input  : Per WM_MOUSELEAVE.
//-----------------------------------------------------------------------------
LRESULT CTitleWnd::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
	m_bMouseOver = false;
	Invalidate();
	UpdateWindow();
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Enables mouse tracking so we can render with a special font when
//			the mouse floats over the window.
// Input  : Per MFC OnMouseMove.
//-----------------------------------------------------------------------------
void CTitleWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bMouseOver)
	{
		TRACKMOUSEEVENT Track;
		Track.cbSize = sizeof(Track);
		Track.dwFlags = TME_HOVER | TME_LEAVE;
		Track.hwndTrack = m_hWnd;
		Track.dwHoverTime = 0.1;

		_TrackMouseEvent(&Track);

		m_bMouseOver = true;

		Invalidate();
		UpdateWindow();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders the title window. A special font is used if the mouse is
//			over the title window or if the window's menu is open.
//-----------------------------------------------------------------------------
void CTitleWnd::OnPaint(void)
{
	if (m_szTitle[0] != '\0')
	{
		if (GetUpdateRect(NULL, TRUE))
		{
			CPaintDC dc(this);
			CFont *pFontOld;

			if ((m_bMouseOver) || (m_bMenuOpen))
			{
				pFontOld = dc.SelectObject(&m_FontActive);
				dc.SetTextColor(RGB(255, 255, 255));
			}
			else
			{
				pFontOld = dc.SelectObject(&m_FontNormal);
				dc.SetTextColor(RGB(200, 200, 200));
			}

			dc.SetBkMode(TRANSPARENT);
			dc.TextOut(0, 0, m_szTitle, strlen(m_szTitle));
			dc.SelectObject(pFontOld);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Opens the context menu when right-clicked upon.
// Input  : Per MFC OnRightButtonDown.
//-----------------------------------------------------------------------------
void CTitleWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	OnMouseButton();
}


//-----------------------------------------------------------------------------
// Purpose: Opens the context menu when right-clicked upon.
// Input  : Per MFC OnRightButtonDown.
//-----------------------------------------------------------------------------
void CTitleWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
	OnMouseButton();
}


//-----------------------------------------------------------------------------
// Purpose: Opens the context menu when right-clicked upon.
// Input  : Per MFC OnRightButtonDown.
//-----------------------------------------------------------------------------
void CTitleWnd::OnMouseButton(void)
{
	static BOOL bFirstTime = TRUE;
	static CMenu Menu;

	if (bFirstTime)
	{
		Menu.LoadMenu(IDR_POPUPS);
		bFirstTime = FALSE;
	}

	CMenu *pPopupMenu = Menu.GetSubMenu(5);
	Assert(pPopupMenu);

	CRect rect;
	GetClientRect(&rect);

	CPoint MenuLocation(0, rect.bottom);
	ClientToScreen(&MenuLocation);

	m_bMenuOpen = true;
	pPopupMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, MenuLocation.x, MenuLocation.y, (CWnd *)GetMainWnd(), NULL);
	m_bMenuOpen = false;
}

