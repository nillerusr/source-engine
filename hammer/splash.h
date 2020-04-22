//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SPLASH_H
#define SPLASH_H
#ifdef _WIN32
#pragma once
#endif


class CSplashWnd : public CWnd
{
// Construction
protected:
	CSplashWnd();

// Attributes:
public:
	CBitmap m_bitmap;

// Operations
public:
	static void EnableSplashScreen(bool bEnable = TRUE);
	static void ShowSplashScreen(CWnd *pParentWnd = NULL);
	static BOOL PreTranslateAppMessage(MSG* pMsg);
	static void HideSplashScreen();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSplashWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	~CSplashWnd();
	virtual void PostNcDestroy();

	BOOL Create(CWnd* pParentWnd = NULL);

// Generated message map functions
protected:

	void DoHide();

	//{{AFX_MSG(CSplashWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	bool m_bHideRequested;		// Set when the app signals the splash screen to hide.
	bool m_bMinTimerExpired;	// Set when OnTimer is called -- ensures that we stay up long enough to be seen.
};


#endif // SPLASH_H
