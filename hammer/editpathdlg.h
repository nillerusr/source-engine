//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_EDITPATHDLG_H__08B03499_FD3C_11D0_AFA7_0060979D2F4E__INCLUDED_)
#define AFX_EDITPATHDLG_H__08B03499_FD3C_11D0_AFA7_0060979D2F4E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// EditPathDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CEditPathDlg dialog

class CEditPathDlg : public CDialog
{
// Construction
public:
	CEditPathDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditPathDlg)
	enum { IDD = IDD_EDITPATH };
	CComboBox	m_cClass;
	CString	m_strClass;
	int		m_iDirection;
	CString	m_strName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditPathDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEditPathDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITPATHDLG_H__08B03499_FD3C_11D0_AFA7_0060979D2F4E__INCLUDED_)
