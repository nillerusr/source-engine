//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef EDITPREFABDLG_H
#define EDITPREFABDLG_H
#ifdef _WIN32
#pragma once
#endif

#include "resource.h"

class CEditPrefabDlg : public CDialog
{
// Construction
public:
	CEditPrefabDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditPrefabDlg)
	enum { IDD = IDD_EDITPREFAB };
	CComboBox	m_CreateIn;
	CEdit	m_Name;
	CEdit	m_Descript;
	CString	m_strDescript;
	CString	m_strName;
	//}}AFX_DATA

	void SetRanges(int iMaxDescript, int iMaxName);
	void EnableLibrary(BOOL = TRUE);
	DWORD m_dwLibraryID;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditPrefabDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int iMaxDescriptChars, iMaxNameChars;
	BOOL m_bEnableLibrary;

	// Generated message map functions
	//{{AFX_MSG(CEditPrefabDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeCreatein();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // EDITPREFABDLG_H
