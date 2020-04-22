//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// StrDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CStrDlg dialog

class CStrDlg : public CDialog
{
// Construction
public:
	CStrDlg(DWORD, LPCTSTR, LPCTSTR, LPCTSTR);   // standard constructor

	enum
	{
		Spin = 0x01
	};

	int iRangeLow, iRangeHigh, iIncrement;
	DWORD dwFlags;
	CString	m_string;
	CString m_strPrompt;
	CString m_strTitle;
	CSpinButtonCtrl	m_cSpin;
	CStatic	m_cPrompt;
	CEdit	m_cEdit;

	void SetRange(int iLow, int iHigh, int iIncrement = 1);

// Dialog Data
	//{{AFX_DATA(CStrDlg)
	enum { IDD = IDD_STRINPUT };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStrDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CStrDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
