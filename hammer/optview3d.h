//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// OPTView3D.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COPTView3D dialog

class COPTView3D : public CPropertyPage
{
	DECLARE_DYNCREATE(COPTView3D)

// Construction
public:
	COPTView3D();
	~COPTView3D();

// Dialog Data
	//{{AFX_DATA(COPTView3D)
	enum { IDD = IDD_OPTIONS_3D };
	CStatic	m_cBackText;
	CSliderCtrl	m_cBackPlane;
	CStatic	m_ModelDistanceText;
	CSliderCtrl	m_ModelDistance;
	CStatic	m_DetailDistanceText;
	CSliderCtrl	m_DetailDistance;
	CStatic	m_ForwardSpeedText;
	CSliderCtrl	m_ForwardSpeedMax;
	CStatic	m_TimeToMaxSpeedText;
	CSliderCtrl	m_TimeToMaxSpeed;
	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COPTView3D)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	BOOL m_bOldFilterTextures;

	// Generated message map functions
	//{{AFX_MSG(COPTView3D)
	virtual BOOL OnInitDialog();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};
