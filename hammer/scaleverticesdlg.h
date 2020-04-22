//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_SCALEVERTICESDLG_H__1E50C989_FEEB_11D0_AFA8_0060979D2F4E__INCLUDED_)
#define AFX_SCALEVERTICESDLG_H__1E50C989_FEEB_11D0_AFA8_0060979D2F4E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ScaleVerticesDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CScaleVerticesDlg dialog

class CScaleVerticesDlg : public CDialog
{
// Construction
public:
	CScaleVerticesDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CScaleVerticesDlg)
	enum { IDD = IDD_SCALEVERTICES };
	CSpinButtonCtrl	m_cScaleSpin;
	CEdit	m_cScale;
	//}}AFX_DATA

	float m_fScale;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScaleVerticesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CScaleVerticesDlg)
	afx_msg void OnChangeScale();
	afx_msg void OnDeltaposScalespin(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCALEVERTICESDLG_H__1E50C989_FEEB_11D0_AFA8_0060979D2F4E__INCLUDED_)
