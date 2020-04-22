//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_SERVICEINSTALLDLG_H__761BDEEF_D549_4F10_817C_1C1FAF9FCA47__INCLUDED_)
#define AFX_SERVICEINSTALLDLG_H__761BDEEF_D549_4F10_817C_1C1FAF9FCA47__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// JobWatchDlg.h : header file
//


#include "resource.h"
#include "tier1/utlvector.h"



/////////////////////////////////////////////////////////////////////////////
// CJobWatchDlg dialog

class CServiceInstallDlg : public CDialog
{
// Construction
public:
	CServiceInstallDlg( CWnd* pParent = NULL);   // standard constructor
	virtual ~CServiceInstallDlg();

// Dialog Data
	//{{AFX_DATA(CJobWatchDlg)
	enum { IDD = IDD_SERVICE_INSTALL_DIALOG};
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CJobWatchDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	SC_HANDLE m_hSCManager;
	
	void VerifyInstallFiles();
	
	bool DoStartExisting();
	bool NukeDirectory( const char *pDir, char errorFile[MAX_PATH] );
	bool DoUninstall( bool bShowMessage );

	// Generated message map functions
	//{{AFX_MSG(CJobWatchDlg)
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnInstall();
	virtual void OnUninstall();
	virtual void OnStartExisting();
	virtual void OnStopExisting();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERVICEINSTALLDLG_H__761BDEEF_D549_4F10_817C_1C1FAF9FCA47__INCLUDED_)
