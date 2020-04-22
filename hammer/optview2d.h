//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// OPTView2D.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COPTView2D dialog

class COPTView2D : public CPropertyPage
{
	DECLARE_DYNCREATE(COPTView2D)

// Construction
public:
	COPTView2D();
	~COPTView2D();

// Dialog Data
	//{{AFX_DATA(COPTView2D)
	enum { IDD = IDD_OPTIONS_2D };
	CSliderCtrl	m_cGridIntensity;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COPTView2D)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COPTView2D)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
