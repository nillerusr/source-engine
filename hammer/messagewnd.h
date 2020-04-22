//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MESSAGEWND_H
#define MESSAGEWND_H
#pragma once

#include <afxtempl.h>
#include "GlobalFunctions.h"

const int MAX_MESSAGE_WND_LINES = 5000;

enum
{
	MESSAGE_WND_MESSAGE_LENGTH = 150
};


class CMessageWnd : private CMDIChildWnd
{
public:

	static CMessageWnd *CreateMessageWndObject();
	void CreateMessageWindow( CMDIFrameWnd *pwndParent, CRect &rect );

	void ShowMessageWindow();
	void ToggleMessageWindow();
	bool IsVisible();

	void Activate();

	void Resize( CRect &rect );

	DECLARE_DYNCREATE(CMessageWnd)

protected:
	CMessageWnd();           // protected constructor used by dynamic creation

	struct MWMSGSTRUCT
	{
		MWMSGTYPE type;
		TCHAR szMsg[MESSAGE_WND_MESSAGE_LENGTH];
		int MsgLen;	// length of message w/o 0x0
	} ;

// Attributes
public:
	void AddMsg(MWMSGTYPE type, TCHAR* msg);

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMessageWnd)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CMessageWnd();

	void CalculateScrollSize();
	CArray<MWMSGSTRUCT, MWMSGSTRUCT&> MsgArray;

	CFont Font;
	int iCharWidth;	// calculated in first paint
	int iNumMsgs;

	// Generated message map functions
	//{{AFX_MSG(CMessageWnd)
	afx_msg void OnPaint();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif // MESSAGEWND_H
