//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// syncfrommirrorDlg.h : header file
//

#include "redirect.h"

#if !defined(AFX_SYNCFROMMIRRORDLG_H__CCDE7319_F912_4B51_A526_4B718AE8E22A__INCLUDED_)
#define AFX_SYNCFROMMIRRORDLG_H__CCDE7319_F912_4B51_A526_4B718AE8E22A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorDlg dialog

class CSyncfrommirrorDlg : public CDialog
{
// Construction
public:
	CSyncfrommirrorDlg(CWnd* pParent = NULL);	// standard constructor

	void RunSync( const char *pszType );

// Dialog Data
	//{{AFX_DATA(CSyncfrommirrorDlg)
	enum { IDD = IDD_SYNCFROMMIRROR_DIALOG };
	CEdit	m_Output;
	CString	m_WorkingFolder;
	BOOL	m_bHL2;
	BOOL	m_bTF2;
	BOOL	m_bHL1Port;
	BOOL	m_bDOD;
	BOOL	m_bCSPort;
	BOOL	m_bMakeCommandPrompts;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSyncfrommirrorDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	CRedirect m_Redirector;

	// Layout info for resizing.
	int m_nEditRightMargin;		// Distance from the right side of the edit control to the right side of the client area
	int m_nEditBottomMargin;	// Distance from the bottom of the edit control to the bottom of the client area
	int m_nButtonOffset;		// Distance from left side of buttons to the right side of the client area

	void RunCommandLine( const char *pCmdLine, const char *pWorkingDir );

	// Generated message map functions
	//{{AFX_MSG(CSyncfrommirrorDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnOK();
	afx_msg void OnStop();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SYNCFROMMIRRORDLG_H__CCDE7319_F912_4B51_A526_4B718AE8E22A__INCLUDED_)
