//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ModelCheckInDlg.h : header file
//

#if !defined(AFX_MODELCHECKINDLG_H__75541530_E85C_4FAB_8C76_6EE68353E2E2__INCLUDED_)
#define AFX_MODELCHECKINDLG_H__75541530_E85C_4FAB_8C76_6EE68353E2E2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CModelCheckInDlg dialog

class CModelCheckInDlg : public CDialog
{
// Construction
public:
	CModelCheckInDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CModelCheckInDlg)
	enum { IDD = IDD_MODELCHECKIN_DIALOG };
	CString	m_HL2GameDirectory;
	CString	m_TF2GameDirectory;
	CString	m_UserName;
	int		m_HL2Radio;
	int		m_TF2Radio;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CModelCheckInDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CModelCheckInDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	//}}AFX_MSG

	afx_msg  void OnFileCheckIn();
	afx_msg  void OnFileCheckOut();
	afx_msg  void OnFileExit();

	DECLARE_MESSAGE_MAP()

private:
	enum ProjectType_t
	{
		PROJECT_ERROR = -1,
		PROJECT_HL2 = 0,
		PROJECT_TF2
	};

	bool CheckInfo();
	bool SetCurrentSSProject( ProjectType_t project, char const* pRelativeDir );
	ProjectType_t ComputeRelativeFileName( char const* pInFile, char* pRelativeFile );

	void StoreStateIntoRegistry( );
	void RestoreStateFromRegistry( );

	ProjectType_t GetFileNames( char const* pTitle, char*& pRelativePath,
									char*& pFileName, char*& pDestPath );

	void ResetDirectoryEntry( CString &fullPath, char *pRegEntry );
	void PerformCheckoutCommand( ProjectType_t project, 
		char const* pRelativeDir, char const* pDestPath, char const* pFileName );
	void PerformCheckinCommand( ProjectType_t project, 
		char const* pRelativeDir, char const* pDestPath, char const* pFileName );
};

//-----------------------------------------------------------------------------
// registry info
//-----------------------------------------------------------------------------

#define  MDL_CHECKOUT_REG_CLASS "HKEY_CURRENT_USER\\SOFTWARE\\Valve\\MDLCheckOut"
#define  MDL_CHECKOUT_REG_LAST_PATH "Path"
#define  MDL_CHECKOUT_REG_USER		"User"
#define  MDL_CHECKOUT_REG_HL2_PATH	"HL2"
#define  MDL_CHECKOUT_REG_TF2_PATH	"TF2"


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MODELCHECKINDLG_H__75541530_E85C_4FAB_8C76_6EE68353E2E2__INCLUDED_)
