//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#if !defined(AFX_ARCHDLG_H__C146AA5D_38FE_11D1_AFC9_0060979D2F4E__INCLUDED_)
#define AFX_ARCHDLG_H__C146AA5D_38FE_11D1_AFC9_0060979D2F4E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define ARC_MAX_POINTS 4096

class CArchDlg : public CDialog
{
// Construction
public:
	CArchDlg(Vector& bmins, Vector& bmaxs, CWnd* pParent = NULL);   // standard constructor
	~CArchDlg();   // standard constructor

	void DrawArch(CDC *pDC);

	Vector bmins, bmaxs;

// Dialog Data
	//{{AFX_DATA(CArchDlg)
	enum { IDD = IDD_ARCH };
	CSpinButtonCtrl	m_cStartAngleSpin;
	CSpinButtonCtrl	m_cWallWidthSpin;
	CEdit	m_cWallWidth;
	CSpinButtonCtrl	m_cSidesSpin;
	CEdit	m_cSides;
	CSpinButtonCtrl	m_cArcSpin;
	CEdit	m_cArc;
	CStatic	m_cPreview;
	int		m_iSides;
	int		m_iWallWidth;
	int		m_iAddHeight;
	float	m_fArc;
	float	m_fAngle;
	//}}AFX_DATA

	void SetMaxWallWidth(int iMaxWallWidth) { m_iMaxWallWidth = iMaxWallWidth; }
	void SaveValues();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CArchDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	int m_iMaxWallWidth;

	// Generated message map functions
	//{{AFX_MSG(CArchDlg)
	afx_msg void OnChangeArc();
	afx_msg void OnCircle();
	afx_msg void OnUpdateSides();
	afx_msg void OnUpdateWallwidth();
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnArchPreview();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ARCHDLG_H__C146AA5D_38FE_11D1_AFC9_0060979D2F4E__INCLUDED_)
