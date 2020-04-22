//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_CSTM1DLG_H__F9EAE5A1_5043_41B1_80CA_495CB6723480__INCLUDED_)
#define AFX_CSTM1DLG_H__F9EAE5A1_5043_41B1_80CA_495CB6723480__INCLUDED_

// cstm1dlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCustom1Dlg dialog

class CCustom1Dlg : public CAppWizStepDlg
{
// Construction
public:
	CCustom1Dlg();
	virtual BOOL OnDismiss();

// Dialog Data
	//{{AFX_DATA(CCustom1Dlg)
	enum { IDD = IDD_CUSTOM1 };
	CString	m_RootPath;
	CString	m_TargetPath;
	int		m_ProjectType;
	BOOL	m_ToolProject;
	CString	m_ImplibPath;
	BOOL	m_PublicProject;
	BOOL	m_ConsoleApp;
	BOOL	m_PublishImportLib;
	CString	m_SrcPath;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCustom1Dlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CCustom1Dlg)
	afx_msg void OnSelchangeSelectProjectType();
	afx_msg void OnChangeEditRootPath();
	afx_msg void OnCheckPublic();
	afx_msg void OnCheckTool();
	afx_msg void OnCheckPublishImport();
	afx_msg void OnChangeEditSrcPath();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void RecomputeTargetPath();
	bool ComputeRelativePath( );
	void EnableCheckboxes();
};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CSTM1DLG_H__F9EAE5A1_5043_41B1_80CA_495CB6723480__INCLUDED_)
