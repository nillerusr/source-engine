//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include <afxtempl.h>
#include "hammer.h"
#include "MessageWnd.h"
#include "mainfrm.h"
#include "GlobalFunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_DYNCREATE(CMessageWnd, CMDIChildWnd)

const int iMsgPtSize = 10;


BEGIN_MESSAGE_MAP(CMessageWnd, CMDIChildWnd)
	//{{AFX_MSG_MAP(CMessageWnd)
	ON_WM_PAINT()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_SIZE()
	ON_WM_KEYDOWN()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Static factory function to create the message window object. The window
// itself is created by CMessageWnd::CreateWindow.
//-----------------------------------------------------------------------------
CMessageWnd *CMessageWnd::CreateMessageWndObject()
{
	CMessageWnd *pMsgWnd = (CMessageWnd *)RUNTIME_CLASS(CMessageWnd)->CreateObject();
	return pMsgWnd;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMessageWnd::CMessageWnd()
{
	// set initial elements
	iCharWidth = -1;
	iNumMsgs = 0;

	// load font
	Font.CreatePointFont(iMsgPtSize * 10, "Courier New");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMessageWnd::~CMessageWnd()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::CreateMessageWindow( CMDIFrameWnd *pwndParent, CRect &rect )
{
	Create( NULL, "Messages", WS_OVERLAPPEDWINDOW | WS_CHILD, rect, pwndParent );

	bool bErrors = true;
	MWMSGSTRUCT mws;
	for ( int i = 0; i < iNumMsgs; i++ )
	{
		mws = MsgArray[i];
		if ( ( mws.type == mwError ) || ( mws.type == mwWarning ) )
		{
			bErrors = true;
		}		
	}
	
	if ( bErrors )
	{
		ShowWindow( SW_SHOW );
	}
}


//-----------------------------------------------------------------------------
// Emit a message to our messages array.
// NOTE: During startup the window itself might not exist yet!
//-----------------------------------------------------------------------------
void CMessageWnd::AddMsg(MWMSGTYPE type, TCHAR* msg)
{
	int iAddAt = iNumMsgs;

	// Don't allow growth after MAX_MESSAGE_WND_LINES
	if ( iNumMsgs == MAX_MESSAGE_WND_LINES )
	{
		MWMSGSTRUCT *p = MsgArray.GetData();
		memcpy(p, p+1, sizeof(*p) * ( MAX_MESSAGE_WND_LINES - 1 ));
		iAddAt = MAX_MESSAGE_WND_LINES - 1;
	}
	else
	{
		++iNumMsgs;
	}

	// format message
	MWMSGSTRUCT mws;	
	mws.MsgLen = strlen(msg);
	mws.type = type;
	Assert(mws.MsgLen <= (sizeof(mws.szMsg) / sizeof(TCHAR)));
	_tcscpy(mws.szMsg, msg);

	// Add the message, growing the array as necessary
	MsgArray.SetAtGrow(iAddAt, mws);

	// Don't do stuff that requires the window to exist.
	if ( m_hWnd == NULL )
		return;

	CalculateScrollSize();
	Invalidate();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::ShowMessageWindow()
{
	if ( m_hWnd == NULL )
		return;

	ShowWindow( SW_SHOW );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::ToggleMessageWindow()
{
	if ( m_hWnd == NULL )
		return;

	ShowWindow( IsWindowVisible() ? SW_HIDE : SW_SHOWNA );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::Activate()
{
	if ( m_hWnd == NULL )
		return;

	ShowWindow( SW_SHOW );
	SetWindowPos( &( CWnd::wndTopMost ), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
	BringWindowToTop();
	SetFocus();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMessageWnd::IsVisible()
{
	if ( m_hWnd == NULL )
		return false;

	return ( IsWindowVisible() == TRUE );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::Resize( CRect &rect )
{
	if ( m_hWnd == NULL )
		return;

	MoveWindow( rect );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::CalculateScrollSize()
{
	if ( m_hWnd == NULL )
		return;

	int iHorz;
	int iVert;

	iVert = iNumMsgs * (iMsgPtSize + 2);
	iHorz = 0;
	for(int i = 0; i < iNumMsgs; i++)
	{
		int iTmp = MsgArray[i].MsgLen * iCharWidth;
		if(iTmp > iHorz)
			iHorz = iTmp;
	}
	Invalidate();

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0; 
	si.nPos = 0;

	CRect clientrect;
	GetClientRect(clientrect);

	// horz
	si.nMax = iHorz;
	si.nPage = clientrect.Width();
	SetScrollInfo(SB_HORZ, &si);

	// vert
	si.nMax = iVert;
	si.nPage = clientrect.Height();
	SetScrollInfo(SB_VERT, &si);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::OnPaint() 
{
	CPaintDC	dc(this);		// device context for painting
	int			nScrollMin;
	int			nScrollMax;

	// select font
	dc.SelectObject(&Font);
	dc.SetBkMode(TRANSPARENT);

	// first paint?
	if(iCharWidth == -1)
	{
		dc.GetCharWidth('A', 'A', &iCharWidth);
		CalculateScrollSize();
	}

	GetScrollRange( SB_VERT, &nScrollMin, &nScrollMax );

	// paint messages
	MWMSGSTRUCT mws;
	CRect r(0, 0, 1, iMsgPtSize+2);

	dc.SetWindowOrg(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));

	for(int i = 0; i < iNumMsgs; i++)
	{
		mws = MsgArray[i];

		r.right = mws.MsgLen * iCharWidth;
		
		if ( r.bottom < nScrollMin )
			continue;
		if ( r.top > nScrollMax )
			break;

		// color of msg
		switch(mws.type)
		{
		case mwError:
			dc.SetTextColor(RGB(255, 60, 60));
			break;
		case mwStatus:
			dc.SetTextColor(RGB(0, 0, 0));
			break;
		}

		// draw text
		dc.TextOut(r.left, r.top, mws.szMsg, mws.MsgLen);

		// move rect down
		r.OffsetRect(0, iMsgPtSize + 2);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int iPos = int(nPos);
	SCROLLINFO si;

	GetScrollInfo(SB_HORZ, &si);
	int iCurPos = GetScrollPos(SB_HORZ);
	int iLimit = GetScrollLimit(SB_HORZ);

	switch(nSBCode)
	{
	case SB_LINELEFT:
		iPos = -int(si.nPage / 4);
		break;
	case SB_LINERIGHT:
		iPos = int(si.nPage / 4);
		break;
	case SB_PAGELEFT:
		iPos = -int(si.nPage / 2);
		break;
	case SB_PAGERIGHT:
		iPos = int(si.nPage / 2);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		iPos -= iCurPos;
		break;
	}

	if(iCurPos + iPos < 0)
		iPos = -iCurPos;
	if(iCurPos + iPos > iLimit)
		iPos = iLimit - iCurPos;
	if(iPos)
	{
		SetScrollPos(SB_HORZ, iCurPos + iPos);
		ScrollWindow(-iPos, 0);
		UpdateWindow();
	}
	CMDIChildWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int iPos = int(nPos);
	SCROLLINFO si;

	GetScrollInfo(SB_VERT, &si);
	int iCurPos = GetScrollPos(SB_VERT);
	int iLimit = GetScrollLimit(SB_VERT);

	switch(nSBCode)
	{
	case SB_LINEUP:
		iPos = -int(si.nPage / 4);
		break;
	case SB_LINEDOWN:
		iPos = int(si.nPage / 4);
		break;
	case SB_PAGEUP:
		iPos = -int(si.nPage / 2);
		break;
	case SB_PAGEDOWN:
		iPos = int(si.nPage / 2);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		iPos -= iCurPos;
		break;
	}

	if(iCurPos + iPos < 0)
		iPos = -iCurPos;
	if(iCurPos + iPos > iLimit)
		iPos = iLimit - iCurPos;
	if(iPos)
	{
		SetScrollPos(SB_VERT, iCurPos + iPos);
		ScrollWindow(0, -iPos);
		UpdateWindow();
	}

	CMDIChildWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::OnSize(UINT nType, int cx, int cy) 
{
	CMDIChildWnd::OnSize(nType, cx, cy);
	CalculateScrollSize();	
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	// up/down
	switch(nChar)
	{
	case VK_UP:
		OnVScroll(SB_LINEUP, 0, NULL);
		break;
	case VK_DOWN:
		OnVScroll(SB_LINEDOWN, 0, NULL);
		break;
	case VK_PRIOR:
		OnVScroll(SB_PAGEUP, 0, NULL);
		break;
	case VK_NEXT:
		OnVScroll(SB_PAGEDOWN, 0, NULL);
		break;
	case VK_HOME:
		OnVScroll(SB_THUMBPOSITION, 0, NULL);
		break;
	case VK_END:
		OnVScroll(SB_THUMBPOSITION, GetScrollLimit(SB_VERT), NULL);
		break;
	}

	CMDIChildWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMessageWnd::OnClose() 
{
	// just hide the window
	ShowWindow(SW_HIDE);
}
