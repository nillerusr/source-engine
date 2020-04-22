//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef REPLACETEXDLG_H
#define REPLACETEXDLG_H
#ifdef _WIN32
#pragma once
#endif


#include "resource.h"
#include "IEditorTexture.h"
#include "wndTex.h"


class CReplaceTexDlg : public CDialog
{
// Construction
public:
	CReplaceTexDlg(int, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CReplaceTexDlg)
	enum { IDD = IDD_REPLACETEX };
	CEdit	m_cFind;
	CEdit	m_cReplace;
	wndTex	m_cReplacePic;
	wndTex	m_cFindPic;
	int		m_iSearchAll;
	CString	m_strFind;
	CString	m_strReplace;
	int		m_iAction;
	BOOL	m_bMarkOnly;
	BOOL	m_bHidden;
	BOOL	m_bRescaleTextureCoordinates;
	//}}AFX_DATA

	int m_nSelected;

	void DoReplaceTextures();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CReplaceTexDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CReplaceTexDlg)
	afx_msg void OnBrowsereplace();
	afx_msg void OnBrowsefind();
	afx_msg void OnUpdateFind();
	afx_msg void OnUpdateReplace();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void BrowseTex(int iEdit);
};

#endif // REPLACETEXDLG_H
