//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ProcessWnd.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CProcessWnd window

class CProcessWnd : public CWnd
{
// Construction
public:
	CProcessWnd();

// Attributes
public:
	// pipes:
	char *pEditBuf;
	UINT uBufLen;

	CEdit Edit;
	CFont Font;

// Operations
public:
	int Execute(LPCTSTR pszCmd, LPCTSTR pszCmdLine);
	int Execute(PRINTF_FORMAT_STRING LPCTSTR pszCmd, ...);

	void Clear();
	void Append(CString str);
	void GetReady();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProcessWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CProcessWnd();

	// Generated message map functions
protected:

	BOOL PreTranslateMessage(MSG* pMsg);

	CString m_EditText;
	CButton m_btnCopyAll;

	//{{AFX_MSG(CProcessWnd)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void CProcessWnd::OnCopyAll();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
