//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_LOGSELECTPROPS_H__D6AFABDD_5BD5_11D3_A5CF_005004039597__INCLUDED_)
#define AFX_LOGSELECTPROPS_H__D6AFABDD_5BD5_11D3_A5CF_005004039597__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LogSelectProps.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// LogSelectProps dialog
#include "persistentstring.h"
#include "ui.h"
#include <list>
using std::list;
class CLogSelectProps : public CPropertyPage
{
// Construction
public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	list<CUIApp::CTFStatsExec>* getList();
	CLogSelectProps(CWnd* pParent = NULL);   // standard constructor

	CPersistentString m_persistLastDirectory;

	void UpdateAppList();
	bool alreadyAcknowledged;

// Dialog Data
	//{{AFX_DATA(CLogSelectProps)
	enum { IDD = IDD_LOGSELS };
	CButton	m_SelectButton;
	CButton	m_RemoveButton;
	CListBox	m_Logs2Do;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLogSelectProps)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLogSelectProps)
	afx_msg void OnSelect();
	afx_msg void OnRemovelog();
	afx_msg void OnSelchangeLogs2do();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LOGSELECTPROPS_H__D6AFABDD_5BD5_11D3_A5CF_005004039597__INCLUDED_)
