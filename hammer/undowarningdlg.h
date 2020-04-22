//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// UndoWarningDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CUndoWarningDlg dialog

class CUndoWarningDlg : public CDialog
{
// Construction
public:
	CUndoWarningDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CUndoWarningDlg)
	enum { IDD = IDD_UNDOWARNING };
	BOOL	m_bNoShow;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUndoWarningDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CUndoWarningDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
