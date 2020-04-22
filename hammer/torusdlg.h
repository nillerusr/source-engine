//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_TORUSDLG_H__C146AA5D_38FE_11D1_AFC9_0060979D2F4E__INCLUDED_)
#define AFX_TORUSDLG_H__C146AA5D_38FE_11D1_AFC9_0060979D2F4E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// TorusDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTorusDlg dialog

#define ARC_MAX_POINTS 4096

class CTorusDlg : public CDialog
{
// Construction
public:
	CTorusDlg(Vector& bmins, Vector& bmaxs, CWnd* pParent = NULL);   // standard constructor
	~CTorusDlg();   // standard constructor

	void DrawTorusTopView( CDC *pDC );
	void DrawTorusCrossSection( CDC *pDC );
	float GetTorusCrossSectionRadius() const;

	BOOL bInitialized;
	Vector bmins, bmaxs;

// Dialog Data
	//{{AFX_DATA(CTorusDlg)
	enum { IDD = IDD_TORUS };
	CSpinButtonCtrl	m_cStartAngleSpin;
	CSpinButtonCtrl	m_cWallWidthSpin;
	CEdit	m_cWallWidth;
	CSpinButtonCtrl	m_cSidesSpin;
	CEdit	m_cSides;
	CSpinButtonCtrl	m_cArcSpin;
	CEdit	m_cArc;
	CStatic	m_cPreview;
	CStatic	m_cTopViewPreview;
	int		m_iSides;
	int		m_iWallWidth;
	float	m_fArc;
	float	m_fAngle;
	float	m_fRotationArc;
	float	m_fRotationAngle;
	int		m_iRotationSides;
	int		m_iAddHeight;
	float	m_fCrossSectionRadius;
	//}}AFX_DATA

	void SetMaxWallWidth(int iMaxWallWidth) { m_iMaxWallWidth = iMaxWallWidth; }
	void SaveValues();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTorusDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	float MaxTorusCrossSectionRadius() const;

	int m_iMaxWallWidth;

	// Generated message map functions
	//{{AFX_MSG(CTorusDlg)
	afx_msg void OnChangeArc();
	afx_msg void OnChangeTorusArc();
	afx_msg void OnCircle();
	afx_msg void OnComputeRadius();
	afx_msg void OnUpdateSides();
	afx_msg void OnUpdateWallwidth();
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnTorusPreview();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TORUSDLG_H__C146AA5D_38FE_11D1_AFC9_0060979D2F4E__INCLUDED_)
