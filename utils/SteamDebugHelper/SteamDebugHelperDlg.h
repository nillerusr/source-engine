//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// SteamDebugHelperDlg.h : header file
//

#if !defined(AFX_STEAMDEBUGHELPERDLG_H__E56B9648_8997_47D7_BEE1_CA0B572B380B__INCLUDED_)
#define AFX_STEAMDEBUGHELPERDLG_H__E56B9648_8997_47D7_BEE1_CA0B572B380B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KeyValues.h"

/////////////////////////////////////////////////////////////////////////////
// CSteamDebugHelperDlg dialog

class CSteamDebugHelperDlg : public CDialog
{
// Construction
public:
	CSteamDebugHelperDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CSteamDebugHelperDlg)
	enum { IDD = IDD_STEAMDEBUGHELPER_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSteamDebugHelperDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

	void SetConfigFilename( const char *pName );

	CString m_ConfigFilename;
	KeyValues* LoadConfigFile();

	
	// Extracted from the kv file.
	const char *m_pSourceExeDir;
	KeyValues *m_pSteamAppCfg;

	const char *m_pSteamAppDir;
	char m_SteamBaseDir[512];


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CSteamDebugHelperDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSetupForDebugging();
	afx_msg void OnUnsetupForDebugging();
	afx_msg void OnStartSteam();
	afx_msg void OnEditConfigFile();
	afx_msg void OnEditChooseConfigFile();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STEAMDEBUGHELPERDLG_H__E56B9648_8997_47D7_BEE1_CA0B572B380B__INCLUDED_)
