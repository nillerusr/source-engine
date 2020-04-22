//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#if !defined(AFX_OPTBUILD_H__33E3A4A0_933D_11D1_8C08_444553540000__INCLUDED_)
#define AFX_OPTBUILD_H__33E3A4A0_933D_11D1_8C08_444553540000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000


/////////////////////////////////////////////////////////////////////////////
// COPTBuild dialog

class COPTBuild : public CPropertyPage
{
// Construction
public:
	COPTBuild();   // standard constructor

// Dialog Data
	//{{AFX_DATA(COPTBuild)
	enum { IDD = IDD_OPTIONS_BUILD };
	CEdit	m_cBSPDir;
	CEdit	m_cVIS;
	CEdit	m_cLIGHT;
	CEdit	m_cGame;
	CEdit	m_cBSP;
	CComboBox	m_cConfigs;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COPTBuild)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COPTBuild)
	afx_msg void OnBrowseBsp();
	afx_msg void OnBrowseGame();
	afx_msg void OnBrowseLight();
	afx_msg void OnBrowseVis();
	afx_msg void OnSelchangeConfigs();
	virtual BOOL OnInitDialog();
	afx_msg void OnParmsBsp();
	afx_msg void OnParmsGame();
	afx_msg void OnParmsLight();
	afx_msg void OnParmsVis();
	afx_msg void OnBrowseBspdir();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CGameConfig *m_pConfig;
	CEdit *m_pAddParmWnd;

	void InsertParm(UINT nID, CEdit *pEdit);
	BOOL HandleInsertParm(UINT nID);
	void DoBrowse(CWnd *pWnd);
	void UpdateConfigList();
	void SaveInfo(CGameConfig *pConfig);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTBUILD_H__33E3A4A0_933D_11D1_8C08_444553540000__INCLUDED_)
