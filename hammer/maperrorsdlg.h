//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// MapErrorsDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMapErrorsDlg dialog

class CMapErrorsDlg : public CDialog
{
// Construction
public:
	CMapErrorsDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMapErrorsDlg)
	enum { IDD = IDD_MAPERRORS };
	CListBox	m_cErrors;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMapErrorsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMapErrorsDlg)
	afx_msg void OnClear();
	afx_msg void OnDblclkErrors();
	afx_msg void OnView();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
