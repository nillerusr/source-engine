//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_FOLDERSELECTPROPS_H__D6AFABE3_5BD5_11D3_A5CF_005004039597__INCLUDED_)
#define AFX_FOLDERSELECTPROPS_H__D6AFABE3_5BD5_11D3_A5CF_005004039597__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FolderSelectProps.h : header file
//

#include "resource.h"
#include "PersistentString.h"
/////////////////////////////////////////////////////////////////////////////
// CFolderSelectProps dialog

class CFolderSelectProps : public CPropertyPage
{
// Construction
public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();

	CFolderSelectProps(CWnd* pParent = NULL);   // standard constructor

	CPersistentString m_StrSupportHTTPPath;
	CPersistentString m_StrPlayerHTTPPath;
	CPersistentString m_StrPlayerDir;
	CPersistentString m_StrTFStatsDir;
	CPersistentString m_StrSupportDir;
	CPersistentString m_StrRuleDir;
	CPersistentString m_StrOutDir;
	CPersistentString m_BoolSupportDefault;
	CPersistentString m_BoolTFStatsDefault;
	CPersistentString m_BoolOutputDefault;
	CPersistentString m_BoolRuleDefault;
	CPersistentString m_BoolPlayerDefault;
	void UpdateFolders(bool safe=true);
	bool alreadyAcknowledged;
	bool lockOutDir;
	bool lockTFSDir;
	bool windowInitted;

// Dialog Data
	//{{AFX_DATA(CFolderSelectProps)
	enum { IDD = IDD_DIRS };
	CButton	m_DefTFStats;
	CButton	m_DefSupportHTTP;
	CButton	m_DefSupport;
	CButton	m_DefRule;
	CButton	m_DefPlayerHTTP;
	CButton	m_DefPlayer;
	CButton	m_DefOutput;
	CEdit	m_SupportHTTPPath;
	CEdit	m_PlayerHTTPPath;
	CEdit	m_PlayerDir;
	CEdit	m_TFStatsDir;
	CEdit	m_SupportDir;
	CEdit	m_RuleDir;
	CEdit	m_OutDir;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFolderSelectProps)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFolderSelectProps)
	afx_msg void OnDefaultCheckBoxClick();
	afx_msg void OnChangeTfstatsdir();
	afx_msg void OnChangeOutputdir();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FOLDERSELECTPROPS_H__D6AFABE3_5BD5_11D3_A5CF_005004039597__INCLUDED_)
