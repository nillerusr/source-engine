//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EDITPATHNODEDLG_H
#define EDITPATHNODEDLG_H
#ifdef _WIN32
#pragma once
#endif

#include "resource.h"


/////////////////////////////////////////////////////////////////////////////
// CEditPathNodeDlg dialog

class CEditPathNodeDlg : public CDialog
{
// Construction
public:
	CEditPathNodeDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditPathNodeDlg)
	enum { IDD = IDD_EDITPATHNODE };
	BOOL	m_bRetrigger;
	int		m_iSpeed;
	int		m_iWait;
	int		m_iYawSpeed;
	CString	m_strName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditPathNodeDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEditPathNodeDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // EDITPATHNODEDLG_H
